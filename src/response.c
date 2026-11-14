/*
 * rurl - Minimal raw URL fetcher (no libcurl)
 * Copyright (C) 2026  rurl contributors
 *
 * SPDX-License-Identifier: EUPL-1.2
 *
 * This file is part of rurl.  See the LICENSE file in the project root for
 * the full licence text.
 */

#include "http_internal.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>

void
stream_init(stream_reader *r, int sock, SSL *ssl, const char *pending,
	    size_t pending_len)
{
	r->sock = sock;
	r->ssl = ssl;
	r->pending = pending;
	r->pending_len = pending_len;
	r->pending_pos = 0;
	r->pos = 0;
	r->len = 0;
	r->eof = 0;
}

static ssize_t
stream_read(stream_reader *r, void *dst, size_t n)
{
	size_t got = 0;

	while (got < n) {
		size_t take;

		if (r->pending_pos < r->pending_len) {
			take = r->pending_len - r->pending_pos;
			if (take > n - got)
				take = n - got;
			memcpy((char *)dst + got, r->pending + r->pending_pos, take);
			r->pending_pos += take;
			got += take;
			continue;
		}

		if (r->pos == r->len) {
			ssize_t rc = read_some(r->sock, r->ssl, r->buf, sizeof(r->buf));

			if (rc < 0)
				return -1;
			if (rc == 0) {
				r->eof = 1;
				break;
			}
			r->pos = 0;
			r->len = (size_t)rc;
		}

		take = r->len - r->pos;
		if (take > n - got)
			take = n - got;
		memcpy((char *)dst + got, r->buf + r->pos, take);
		r->pos += take;
		got += take;
	}

	return (ssize_t)got;
}

static int
stream_read_line(stream_reader *r, char *line, size_t cap)
{
	size_t i = 0;

	for (;;) {
		char c;
		ssize_t n = stream_read(r, &c, 1);

		if (n <= 0)
			return -1;
		if (c == '\0') {
			errno = EPROTO;
			return -1;
		}
		if (c == '\n')
			break;
		if (i + 1 >= cap) {
			errno = EOVERFLOW;
			return -1;
		}
		line[i++] = c;
	}
	if (i > 0 && line[i - 1] == '\r')
		i--;
	line[i] = '\0';
	return 0;
}

/*
 * Inspect Transfer-Encoding.  Returns:
 *   0  header absent; Content-Length may be used
 *   1  final transfer coding is chunked; decode the body
 *   2  transfer coding present but not chunked; read until EOF
 *  -1  chunked is present but not the final coding (unsupported)
 */
int
header_transfer_encoding(const char *header)
{
	const char *line = header;
	int seen_header = 0;
	int seen_token = 0;
	int seen_chunked = 0;
	int last_chunked = 0;

	while (*line) {
		const char *nl = strchr(line, '\n');
		const char *hdr;
		const char *end;
		const char *p;

		if (!nl)
			break;
		hdr = nl + 1;
		if (strncasecmp(hdr, "Transfer-Encoding:", 18) != 0) {
			line = hdr;
			continue;
		}
		seen_header = 1;
		end = strpbrk(hdr, "\r\n");
		if (!end)
			end = hdr + strlen(hdr);
		p = hdr + 18;
		while (p < end) {
			const char *tok_start;
			const char *tok_end;
			const char *comma;
			const char *semi;

			while (p < end && (*p == ' ' || *p == '\t' || *p == ','))
				p++;
			if (p >= end)
				break;
			tok_start = p;
			comma = memchr(p, ',', (size_t)(end - p));
			tok_end = comma ? comma : end;
			semi = memchr(tok_start, ';', (size_t)(tok_end - tok_start));
			if (semi)
				tok_end = semi;
			while (tok_end > tok_start &&
			       (tok_end[-1] == ' ' || tok_end[-1] == '\t'))
				tok_end--;
			if (tok_end > tok_start) {
				seen_token = 1;
				if ((size_t)(tok_end - tok_start) == 7 &&
				    strncasecmp(tok_start, "chunked", 7) == 0) {
					seen_chunked = 1;
					last_chunked = 1;
				} else {
					last_chunked = 0;
				}
			}
			p = comma ? comma + 1 : end;
		}
		line = hdr;
	}

	if (!seen_token)
		return seen_header ? -1 : 0;
	if (last_chunked)
		return 1;
	if (seen_chunked)
		return -1;
	return 2;
}

/*
 * Parse Content-Length.  Returns 1 when present, 0 when absent, -1 when the
 * value is malformed.
 */
int
header_content_length(const char *header, size_t *out)
{
	const char *line = header;
	int found = 0;
	size_t value = 0;

	while (*line) {
		const char *nl = strchr(line, '\n');
		const char *hdr;
		const char *end;
		const char *p;
		char *num_end;
		unsigned long long v;

		if (!nl)
			break;
		hdr = nl + 1;
		end = strpbrk(hdr, "\r\n");
		if (!end)
			end = hdr + strlen(hdr);
		if (strncasecmp(hdr, "Content-Length:", 15) == 0) {
			p = hdr + 15;
			while (p < end && (*p == ' ' || *p == '\t'))
				p++;
			if (p == end || !isdigit((unsigned char)*p))
				return -1;
			errno = 0;
			v = strtoull(p, &num_end, 10);
			if (errno != 0 || num_end == p)
				return -1;
			while (num_end < end && (*num_end == ' ' || *num_end == '\t'))
				num_end++;
			if (num_end != end || v > SIZE_MAX)
				return -1;
			/* Multiple differing Content-Length values are ambiguous. */
			if (found && value != (size_t)v)
				return -1;
			found = 1;
			value = (size_t)v;
		}
		line = hdr;
	}

	if (!found)
		return 0;
	*out = value;
	return 1;
}

int
write_plain_body(stream_reader *r, size_t limit, int have_limit)
{
	char out[BUF_SIZE];
	size_t total = 0;

	for (;;) {
		size_t want = sizeof(out);
		ssize_t n;

		if (have_limit) {
			if (total >= limit)
				return BODY_OK;
			if (want > limit - total)
				want = limit - total;
		}

		n = stream_read(r, out, want);
		if (n < 0)
			return BODY_READ_ERR;
		if (n == 0) {
			if (have_limit && total < limit)
				return BODY_READ_ERR;	/* truncated body */
			return BODY_OK;
		}
		if (fwrite(out, 1, (size_t)n, stdout) != (size_t)n)
			return BODY_WRITE_ERR;
		total += (size_t)n;
	}
}

static int
hex_value(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

int
write_chunked_body(stream_reader *r)
{
	char line[BUF_SIZE];
	char out[BUF_SIZE];

	for (;;) {
		char *p;
		size_t size = 0;
		size_t remaining;

		if (stream_read_line(r, line, sizeof(line)) < 0)
			return BODY_READ_ERR;
		p = line;
		while (*p == ' ' || *p == '\t')
			p++;
		if (hex_value((unsigned char)*p) < 0)
			return BODY_READ_ERR;
		for (;;) {
			int digit = hex_value((unsigned char)*p);

			if (digit < 0)
				break;
			if (size > (SIZE_MAX - (size_t)digit) / 16)
				return BODY_READ_ERR;
			size = size * 16 + (size_t)digit;
			p++;
		}
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p != '\0' && *p != ';')
			return BODY_READ_ERR;

		if (size == 0) {
			size_t trailer_bytes = 0;

			/* Consume trailers through the final blank line. */
			for (;;) {
				size_t line_len;

				if (stream_read_line(r, line, sizeof(line)) < 0)
					return BODY_OK;
				if (line[0] == '\0')
					return BODY_OK;
				line_len = strlen(line);
				if (trailer_bytes > MAX_HEADER_SIZE - line_len)
					return BODY_READ_ERR;
				trailer_bytes += line_len;
			}
		}

		remaining = size;
		while (remaining > 0) {
			size_t want = remaining < sizeof(out) ? remaining : sizeof(out);
			ssize_t n = stream_read(r, out, want);

			if (n <= 0)
				return BODY_READ_ERR;
			if (fwrite(out, 1, (size_t)n, stdout) != (size_t)n)
				return BODY_WRITE_ERR;
			remaining -= (size_t)n;
		}

		/* Consume the CRLF that terminates the chunk data. */
		if (stream_read_line(r, line, sizeof(line)) < 0 || line[0] != '\0')
			return BODY_READ_ERR;
	}
}

/*
 * Parse a status line.  Returns 0 on success, -1 on a malformed line and -2
 * when a Location header could not be allocated.
 */
int
parse_status(const char *buf, int *code, char **location)
{
	const char *p;
	int c;

	if (strncmp(buf, "HTTP/", 5) != 0)
		return -1;

	p = buf + 5;
	if (!isdigit((unsigned char)p[0]))
		return -1;
	p++;
	if (*p != '.')
		return -1;
	p++;
	if (!isdigit((unsigned char)p[0]))
		return -1;
	p++;
	if (*p != ' ')
		return -1;
	p++;

	if (!isdigit((unsigned char)p[0]) ||
	    !isdigit((unsigned char)p[1]) ||
	    !isdigit((unsigned char)p[2]))
		return -1;
	if (p[3] != '\0' && p[3] != ' ' && p[3] != '\r' && p[3] != '\n')
		return -1;

	c = (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
	if (c < 100)
		return -1;
	*code = c;

	if (location) {
		*location = NULL;
		if (c >= 300 && c < 400) {
			const char *line = buf;

			while (*line) {
				const char *nl = strchr(line, '\n');
				const char *hdr;
				const char *end;
				const char *val;
				size_t len;

				if (!nl)
					break;
				hdr = nl + 1;
				if (strncasecmp(hdr, "Location:", 9) != 0) {
					line = hdr;
					continue;
				}
				end = strpbrk(hdr, "\r\n");
				if (!end)
					end = hdr + strlen(hdr);
				val = hdr + 9;
				while (val < end && (*val == ' ' || *val == '\t'))
					val++;
				while (end > val &&
				       (end[-1] == ' ' || end[-1] == '\t'))
					end--;
				len = (size_t)(end - val);
				*location = malloc(len + 1);
				if (!*location)
					return -2;
				memcpy(*location, val, len);
				(*location)[len] = '\0';
				break;
			}
		}
	}

	return 0;
}

int
find_header_end(const char *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (buf[i] == '\n' && i + 1 < len && buf[i + 1] == '\n')
			return (int)(i + 2);
		if (buf[i] == '\r' && i + 3 < len &&
		    buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n')
			return (int)(i + 4);
	}
	return -1;
}

int
is_redirect_code(int code)
{
	return code == 301 || code == 302 || code == 303 ||
	       code == 307 || code == 308;
}
