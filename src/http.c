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
#include <unistd.h>
#include <errno.h>

/*
 * Perform one HTTP transaction.  Returns:
 *   0  final 2xx response written to stdout
 *   1  final non-2xx response written to stdout (error already reported)
 *   2  redirect; *location_out owns the Location value
 *  -1  transport/protocol error
 */
int
do_request(const url_parts *u, char **location_out)
{
	int sock = -1;
	SSL *ssl = NULL;
	char *host_header = NULL;
	char *header = NULL;
	size_t header_cap = 0;
	size_t header_len = 0;
	char buf[BUF_SIZE];
	int code = 0;
	int chunked = 0;
	int have_content_length = 0;
	size_t content_length = 0;
	char *location = NULL;
	int header_end = -1;
	int interim = 0;
	ssize_t n;
	int rc = -1;

	*location_out = NULL;

	sock = connect_host(u->host, u->port, u->is_ssl, &ssl);
	if (sock < 0)
		goto done;

	host_header = build_host_header(u);
	if (!host_header) {
		fprintf(stderr, "error: out of memory\n");
		goto done;
	}
	{
		int snd_rc = send_request(sock, ssl, host_header, u->path);

		if (snd_rc != 0) {
			if (snd_rc == -2)
				fprintf(stderr, "error: timeout after %d seconds\n", config.timeout_sec);
			else
				fprintf(stderr, "error: failed to send request\n");
			goto done;
		}
	}

	header_cap = BUF_SIZE;
	header = malloc(header_cap);
	if (!header) {
		fprintf(stderr, "error: out of memory\n");
		goto done;
	}

	for (;;) {
		header_end = find_header_end(header, header_len);
		if (header_end >= 0) {
			int parse_rc;
			char saved;

			if ((size_t)header_end > MAX_HEADER_SIZE) {
				fprintf(stderr, "error: response headers too large\n");
				goto done;
			}
			if (memchr(header, '\0', (size_t)header_end) != NULL) {
				fprintf(stderr, "error: invalid response header\n");
				goto done;
			}

			saved = header[header_end];
			header[header_end] = '\0';
			parse_rc = parse_status(header, &code, NULL);
			header[header_end] = saved;

			if (parse_rc < 0) {
				fprintf(stderr, "error: failed to parse HTTP status\n");
				goto done;
			}

			/* Informational responses precede the final response. */
			if (code >= 100 && code < 200) {
				if (code == 101) {
					fprintf(stderr, "error: unexpected protocol switch\n");
					goto done;
				}
				if (++interim > MAX_INTERIM_RESPONSES) {
					fprintf(stderr, "error: too many informational responses\n");
					goto done;
				}
				memmove(header, header + header_end,
					header_len - (size_t)header_end);
				header_len -= (size_t)header_end;
				header[header_len] = '\0';
				continue;
			}
			break;
		}

		if (header_len > MAX_HEADER_SIZE) {
			fprintf(stderr, "error: response headers too large\n");
			goto done;
		}

		errno = 0;
		n = read_some(sock, ssl, buf, sizeof(buf));
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				fprintf(stderr, "error: timeout after %d seconds\n", config.timeout_sec);
			else
				fprintf(stderr, "error: failed to read response\n");
			goto done;
		}
		if (n == 0)
			break;

		if (header_len + (size_t)n + 1 > header_cap) {
			size_t new_cap = header_cap;
			char *tmp;

			while (header_len + (size_t)n + 1 > new_cap)
				new_cap *= 2;
			tmp = realloc(header, new_cap);
			if (!tmp) {
				fprintf(stderr, "error: out of memory\n");
				goto done;
			}
			header = tmp;
			header_cap = new_cap;
		}
		memcpy(header + header_len, buf, (size_t)n);
		header_len += (size_t)n;
		header[header_len] = '\0';
	}

	if (header_len == 0) {
		fprintf(stderr, "error: empty response\n");
		goto done;
	}
	if (header_end < 0) {
		fprintf(stderr, "error: invalid response\n");
		goto done;
	}

	{
		char saved = header[header_end];
		int parse_rc;
		int te = 0;
		int length_rc = 0;

		header[header_end] = '\0';
		parse_rc = parse_status(header, &code, &location);
		if (parse_rc == 0) {
			te = header_transfer_encoding(header);
			if (te == 0)
				length_rc = header_content_length(header,
								  &content_length);
		}
		header[header_end] = saved;

		if (parse_rc == -2) {
			fprintf(stderr, "error: out of memory\n");
			goto done;
		}
		if (parse_rc < 0) {
			fprintf(stderr, "error: failed to parse HTTP status\n");
			goto done;
		}
		if (te < 0) {
			fprintf(stderr, "error: unsupported Transfer-Encoding\n");
			goto done;
		}
		if (length_rc < 0) {
			fprintf(stderr, "error: invalid Content-Length header\n");
			goto done;
		}

		if (te == 1) {
			chunked = 1;
			have_content_length = 0;
		} else if (te == 2) {
			chunked = 0;
			have_content_length = 0;
		} else {
			chunked = 0;
			have_content_length = length_rc;
		}
		if (code == 204 || code == 205 || code == 304) {
			chunked = 0;
			have_content_length = 1;
			content_length = 0;
		}
	}

	if (is_redirect_code(code)) {
		if (!location || location[0] == '\0') {
			fprintf(stderr, "error: redirect response without a valid Location header\n");
			goto done;
		}
		*location_out = location;
		location = NULL;
		rc = 2;
		goto done;
	}

	{
		stream_reader reader;
		int body_rc;

		stream_init(&reader, sock, ssl, header + header_end,
			    header_len - (size_t)header_end);

		if (chunked)
			body_rc = write_chunked_body(&reader);
		else
			body_rc = write_plain_body(&reader, content_length,
						   have_content_length);

		if (body_rc == BODY_WRITE_ERR) {
			fprintf(stderr, "error: failed to write to stdout\n");
			goto done;
		}
		if (body_rc != BODY_OK) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				fprintf(stderr, "error: timeout after %d seconds\n", config.timeout_sec);
			else
				fprintf(stderr, "error: failed to read response body\n");
			goto done;
		}
	}

	if (fflush(stdout) != 0) {
		fprintf(stderr, "error: failed to write to stdout\n");
		goto done;
	}

	if (code >= 200 && code < 300) {
		rc = 0;
	} else {
		fprintf(stderr, "error: HTTP %d\n", code);
		rc = 1;
	}

done:
	free(location);
	free(host_header);
	free(header);
	if (ssl)
		SSL_free(ssl);
	if (sock >= 0)
		close(sock);
	return rc;
}

int
fetch_url(const char *start_url)
{
	char *current = strdup(start_url);
	int redirects = 0;

	if (!current) {
		fprintf(stderr, "error: out of memory\n");
		return -1;
	}
	normalize_url_path(current);

	for (;;) {
		url_parts u;
		char *location = NULL;
		char *next;
		int rc;

		if (parse_url(current, &u) < 0) {
			fprintf(stderr, "error: invalid URL: ");
			fprint_sanitized(stderr, current);
			fputc('\n', stderr);
			free(current);
			return -1;
		}

		rc = do_request(&u, &location);

		if (rc != 2) {
			free_url_parts(&u);
			free(current);
			return rc;
		}

		if (redirects >= config.max_redirs) {
			fprintf(stderr, "error: too many redirects\n");
			free(location);
			free_url_parts(&u);
			free(current);
			return -1;
		}

		next = resolve_location(&u, location);
		free_url_parts(&u);
		free(location);
		if (!next) {
			fprintf(stderr, "error: out of memory\n");
			free(current);
			return -1;
		}
		free(current);
		current = next;
		redirects++;
	}
}
