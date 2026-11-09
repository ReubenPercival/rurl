/*
 * rurl - Minimal raw URL fetcher (no libcurl)
 * Copyright (C) 2026  SocialistSoftwareFoundation
 *
 * Licensed under the EUPL, Version 1.2 or - as soon they will be approved by
 * the European Commission - subsequent versions of the EUPL (the "Licence");
 * You may not use this work except in compliance with the Licence.
 * You may obtain a copy of the Licence at:
 *
 * https://joinup.ec.europa.eu/software/page/eupl
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Licence is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing permissions and
 * limitations under the Licence.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_REDIRS 20
#define RURL_VERSION "0.1.5"
#define RURL_USERAGENT "rurl/" RURL_VERSION
#define BUF_SIZE 8192
#define MAX_HEADER_SIZE (64 * 1024)
#define TIMEOUT_SEC 30

static const char *useragent = RURL_USERAGENT;
static int insecure = 0;
static int max_redirs = MAX_REDIRS;
static int timeout_sec = TIMEOUT_SEC;

typedef struct {
	int is_ssl;
	char *host;
	char *port;
	char *path;
} url_parts;

static void
print_usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [-A USERAGENT] [-k] [-n MAXREDIRS] [-t TIMEOUT] URL\n", prog);
	fprintf(stderr, "  -A USERAGENT  Set custom User-Agent string (default: %s)\n", RURL_USERAGENT);
	fprintf(stderr, "  -k            Allow insecure SSL connections\n");
	fprintf(stderr, "  -n MAXREDIRS  Maximum redirects (default: %d)\n", MAX_REDIRS);
	fprintf(stderr, "  -t TIMEOUT    Timeout in seconds (default: %d)\n", TIMEOUT_SEC);
	fprintf(stderr, "  -h            Show this help message\n");
	fprintf(stderr, "  -V            Show version\n");
}

static char *
concat2(const char *a, const char *b)
{
	size_t la = strlen(a);
	size_t lb = strlen(b);
	char *r = malloc(la + lb + 1);

	if (!r)
		return NULL;
	memcpy(r, a, la);
	memcpy(r + la, b, lb + 1);
	return r;
}

static char *
concat3(const char *a, const char *b, const char *c)
{
	size_t la = strlen(a);
	size_t lb = strlen(b);
	size_t lc = strlen(c);
	char *r = malloc(la + lb + lc + 1);

	if (!r)
		return NULL;
	memcpy(r, a, la);
	memcpy(r + la, b, lb);
	memcpy(r + la + lb, c, lc + 1);
	return r;
}

static void
free_url_parts(url_parts *u)
{
	free(u->host);
	free(u->port);
	free(u->path);
	u->host = NULL;
	u->port = NULL;
	u->path = NULL;
}

static int
valid_port(const char *s)
{
	char *end;
	long v;

	errno = 0;
	v = strtol(s, &end, 10);
	return !(errno != 0 || end == s || *end != '\0' || v < 1 || v > 65535);
}

static int
parse_url(const char *url, url_parts *u)
{
	const char *p = url;
	const char *host_start;
	const char *host_end;
	const char *at;
	const char *c;
	size_t host_len;
	char *colon;

	memset(u, 0, sizeof(*u));

	if (strncmp(p, "http://", 7) == 0) {
		u->is_ssl = 0;
		p += 7;
	} else if (strncmp(p, "https://", 8) == 0) {
		u->is_ssl = 1;
		p += 8;
	} else {
		return -1;
	}

	/* Reject CR/LF up front: they would allow request-header injection. */
	if (strpbrk(p, "\r\n") != NULL)
		return -1;

	host_start = p;
	host_end = p + strcspn(p, "/?#");
	host_len = (size_t)(host_end - host_start);

	/* Strip optional userinfo (user:password@host). */
	at = memchr(host_start, '@', host_len);
	if (at) {
		host_start = at + 1;
		host_len = (size_t)(host_end - host_start);
	}
	if (host_len == 0)
		return -1;

	u->host = malloc(host_len + 1);
	if (!u->host)
		return -1;
	memcpy(u->host, host_start, host_len);
	u->host[host_len] = '\0';

	if (u->host[0] == '[') {
		char *rb = strchr(u->host, ']');

		if (!rb || (rb[1] != '\0' && rb[1] != ':'))
			goto fail;
		colon = (rb[1] == ':') ? rb + 1 : NULL;
	} else {
		colon = strchr(u->host, ':');
		if (colon && strchr(colon + 1, ':'))
			goto fail;	/* unbracketed IPv6 literal */
	}

	if (colon) {
		*colon = '\0';
		if (colon[1] == '\0')
			goto fail;
		u->port = strdup(colon + 1);
		if (!u->port || !valid_port(u->port))
			goto fail;
	} else {
		u->port = strdup(u->is_ssl ? "443" : "80");
		if (!u->port)
			goto fail;
	}

	if (u->host[0] == '\0' || (u->host[0] == '[' && u->host[1] == ']'))
		goto fail;

	if (*host_end == '/') {
		u->path = strdup(host_end);
	} else if (*host_end == '?') {
		u->path = concat2("/", host_end);
	} else {
		u->path = strdup("/");
	}
	if (!u->path)
		goto fail;

	{
		char *frag = strchr(u->path, '#');

		if (frag)
			*frag = '\0';
	}

	/* Spaces and control characters are not valid in a request target. */
	for (c = u->path; *c; c++) {
		if ((unsigned char)*c <= 0x20 || (unsigned char)*c == 0x7f)
			goto fail;
	}

	return 0;

fail:
	free_url_parts(u);
	return -1;
}

/*
 * Remove "." and ".." segments from the path portion of a URL, leaving the
 * query string intact.
 */
static void
normalize_url_path(char *url)
{
	char *scheme = strstr(url, "://");
	char *path;
	char *query;
	char *result;
	size_t len;
	size_t i;
	size_t r = 0;

	if (!scheme)
		return;
	path = strchr(scheme + 3, '/');
	if (!path)
		return;

	query = strchr(path, '?');
	len = query ? (size_t)(query - path) : strlen(path);
	result = malloc(len + (query ? strlen(query) : 0) + 2);
	if (!result)
		return;

	i = 0;
	while (i < len) {
		if (i + 2 <= len && path[i] == '/' && path[i + 1] == '.' &&
		    (i + 2 == len || path[i + 2] == '/')) {
			i += 2;
			continue;
		}
		if (i + 3 <= len && path[i] == '/' && path[i + 1] == '.' &&
		    path[i + 2] == '.' &&
		    (i + 3 == len || path[i + 3] == '/')) {
			i += 3;
			while (r > 0 && result[r - 1] != '/')
				r--;
			if (r > 0)
				r--;
			continue;
		}
		result[r++] = path[i++];
	}

	if (r == 0)
		result[r++] = '/';
	if (query)
		memcpy(result + r, query, strlen(query) + 1);
	else
		result[r] = '\0';

	memcpy(path, result, strlen(result) + 1);
	free(result);
}

/*
 * Resolve a Location header against the URL that produced it.  Handles
 * absolute URLs, scheme-relative ("//host/..."), absolute paths, query-only,
 * fragment-only, and path-relative references.
 */
static char *
resolve_location(const url_parts *base, const char *loc)
{
	const char *scheme = base->is_ssl ? "https" : "http";
	int default_port = base->is_ssl ? 443 : 80;
	const char *path = base->path;
	const char *query = strchr(path, '?');
	size_t path_len = query ? (size_t)(query - path) : strlen(path);
	char *origin;
	char *result;

	if (strncmp(loc, "http://", 7) == 0 ||
	    strncmp(loc, "https://", 8) == 0)
		return strdup(loc);

	origin = concat3(scheme, "://", base->host);
	if (!origin)
		return NULL;
	if (atoi(base->port) != default_port) {
		char *tmp = concat3(origin, ":", base->port);

		free(origin);
		if (!tmp)
			return NULL;
		origin = tmp;
	}

	if (loc[0] == '\0' || loc[0] == '#') {
		result = concat2(origin, path);
	} else if (loc[0] == '/' && loc[1] == '/') {
		char *prefix = concat2(scheme, ":");

		if (!prefix) {
			free(origin);
			return NULL;
		}
		result = concat2(prefix, loc);
		free(prefix);
	} else if (loc[0] == '/') {
		result = concat2(origin, loc);
	} else if (loc[0] == '?') {
		char *dir = malloc(path_len + 1);

		if (!dir) {
			free(origin);
			return NULL;
		}
		memcpy(dir, path, path_len);
		dir[path_len] = '\0';
		result = concat3(origin, dir, loc);
		free(dir);
	} else {
		size_t dir_len = 0;
		size_t i;
		char *dir;

		for (i = path_len; i > 0; i--) {
			if (path[i - 1] == '/') {
				dir_len = i;
				break;
			}
		}
		dir = malloc(dir_len + 1);
		if (!dir) {
			free(origin);
			return NULL;
		}
		memcpy(dir, path, dir_len);
		dir[dir_len] = '\0';
		result = concat3(origin, dir, loc);
		free(dir);
	}

	free(origin);
	if (result)
		normalize_url_path(result);
	return result;
}

static char *
build_host_header(const url_parts *u)
{
	int default_port = u->is_ssl ? 443 : 80;

	if (atoi(u->port) == default_port)
		return strdup(u->host);
	return concat3(u->host, ":", u->port);
}

static int
connect_with_timeout(int sock, const struct sockaddr *addr, socklen_t addrlen)
{
	int flags;
	int rc;
	fd_set wfds;
	struct timeval tv;

	flags = fcntl(sock, F_GETFL, 0);
	if (flags < 0)
		return -1;
	if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
		return -1;

	rc = connect(sock, addr, addrlen);
	if (rc < 0 && errno == EINPROGRESS) {
		for (;;) {
			FD_ZERO(&wfds);
			FD_SET(sock, &wfds);
			tv.tv_sec = timeout_sec;
			tv.tv_usec = 0;
			rc = select(sock + 1, NULL, &wfds, NULL, &tv);
			if (rc < 0 && errno == EINTR)
				continue;
			break;
		}

		if (rc == 0) {
			errno = ETIMEDOUT;
			rc = -1;
		} else if (rc > 0) {
			int err = 0;
			socklen_t errlen = sizeof(err);

			if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0) {
				rc = -1;
			} else if (err != 0) {
				errno = err;
				rc = -1;
			} else {
				rc = 0;
			}
		}
	}

	if (fcntl(sock, F_SETFL, flags) < 0)
		return -1;
	return rc;
}

static int
connect_host(const char *host, const char *port, int is_ssl, SSL **ssl_out)
{
	struct addrinfo hints, *result = NULL, *rp;
	struct timeval tv;
	int sock = -1;
	int saved_errno = 0;
	int is_ip = 0;
	SSL *ssl = NULL;
	SSL_CTX *ctx = NULL;
	char *lookup = NULL;
	const char *lookup_host = host;
	unsigned char addr[sizeof(struct in6_addr)];

	if (host[0] == '[') {
		size_t hlen = strlen(host);

		if (hlen > 2 && host[hlen - 1] == ']') {
			size_t n = hlen - 2;

			lookup = malloc(n + 1);
			if (!lookup)
				return -1;
			memcpy(lookup, host + 1, n);
			lookup[n] = '\0';
			lookup_host = lookup;
		}
	}

	if (inet_pton(AF_INET, lookup_host, addr) == 1 ||
	    inet_pton(AF_INET6, lookup_host, addr) == 1)
		is_ip = 1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(lookup_host, port, &hints, &result) != 0) {
		fprintf(stderr, "error: failed to resolve host: %s\n", host);
		free(lookup);
		return -1;
	}

	for (rp = result; rp; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock < 0)
			continue;
		if (connect_with_timeout(sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		saved_errno = errno;
		close(sock);
		sock = -1;
	}

	freeaddrinfo(result);

	if (sock < 0) {
		if (saved_errno == ETIMEDOUT)
			fprintf(stderr, "error: connection timed out after %d seconds\n", timeout_sec);
		else
			fprintf(stderr, "error: failed to connect to %s:%s\n", host, port);
		free(lookup);
		return -1;
	}

	tv.tv_sec = timeout_sec;
	tv.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	if (is_ssl) {
		ctx = SSL_CTX_new(TLS_client_method());
		if (!ctx) {
			close(sock);
			free(lookup);
			fprintf(stderr, "error: SSL_CTX_new failed\n");
			return -1;
		}

		/*
		 * HTTP/1.1 uses "Connection: close"; many servers drop the TCP
		 * connection without sending a TLS close_notify.  Treat that as a
		 * clean end of response instead of failing after a complete body.
		 */
#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
		SSL_CTX_set_options(ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif
		if (!insecure) {
			SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
			if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
				fprintf(stderr, "error: failed to load system CA certificates\n");
				SSL_CTX_free(ctx);
				close(sock);
				free(lookup);
				return -1;
			}
		}

		ssl = SSL_new(ctx);
		SSL_CTX_free(ctx);
		if (!ssl) {
			close(sock);
			free(lookup);
			fprintf(stderr, "error: SSL_new failed\n");
			return -1;
		}

		SSL_set_fd(ssl, sock);
		if (!is_ip && SSL_set_tlsext_host_name(ssl, lookup_host) != 1) {
			fprintf(stderr, "error: failed to set TLS server name\n");
			SSL_free(ssl);
			close(sock);
			free(lookup);
			return -1;
		}

		if (!insecure) {
			X509_VERIFY_PARAM *param = SSL_get0_param(ssl);
			int ok;

			if (is_ip)
				ok = X509_VERIFY_PARAM_set1_ip_asc(param, lookup_host);
			else
				ok = SSL_set1_host(ssl, lookup_host);
			if (!ok) {
				fprintf(stderr, "error: failed to configure certificate verification\n");
				SSL_free(ssl);
				close(sock);
				free(lookup);
				return -1;
			}
		}

		if (SSL_connect(ssl) != 1) {
			unsigned long err = ERR_get_error();
			const char *reason = err ? ERR_reason_error_string(err) : NULL;

			fprintf(stderr, "error: SSL handshake failed%s%s\n",
				reason ? ": " : "", reason ? reason : "");
			SSL_free(ssl);
			close(sock);
			free(lookup);
			return -1;
		}
	}

	free(lookup);
	*ssl_out = ssl;
	return sock;
}

static int
send_request(int sock, SSL *ssl, const char *host_header, const char *path)
{
	char buf[4096];
	int len, total, n;

	len = snprintf(buf, sizeof(buf),
		"GET %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"User-Agent: %s\r\n"
		"Connection: close\r\n"
		"\r\n",
		path, host_header, useragent);

	if (len < 0 || (size_t)len >= sizeof(buf))
		return -1;

	total = 0;
	while (total < len) {
		if (ssl) {
			n = SSL_write(ssl, buf + total, len - total);
			if (n <= 0) {
				int err = SSL_get_error(ssl, n);

				if (err == SSL_ERROR_SYSCALL && errno == EINTR)
					continue;
				return -1;
			}
		} else {
			n = (int)send(sock, buf + total, (size_t)(len - total), 0);
			if (n < 0 && errno == EINTR)
				continue;
			if (n <= 0)
				return -1;
		}
		total += n;
	}

	return 0;
}

static ssize_t
read_some(int sock, SSL *ssl, void *buf, size_t len)
{
	if (ssl) {
		for (;;) {
			int n;
			int err;

			errno = 0;
			n = SSL_read(ssl, buf, (int)len);
			if (n > 0)
				return n;
			err = SSL_get_error(ssl, n);
			if (err == SSL_ERROR_ZERO_RETURN)
				return 0;
			if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
				errno = EAGAIN;
				return -1;
			}
			if (err == SSL_ERROR_SYSCALL) {
				if (errno == EINTR)
					continue;
				/* Peer closed without sending close_notify. */
				if (n == 0 || errno == 0)
					return 0;
			}
			return -1;
		}
	} else {
		ssize_t n;

		do {
			n = recv(sock, buf, len, 0);
		} while (n < 0 && errno == EINTR);
		return n;
	}
}

#define BODY_OK 0
#define BODY_READ_ERR -1
#define BODY_WRITE_ERR -2

typedef struct {
	int sock;
	SSL *ssl;
	const char *pending;
	size_t pending_len;
	size_t pending_pos;
	char buf[BUF_SIZE];
	size_t pos;
	size_t len;
	int eof;
} stream_reader;

static void
stream_init(stream_reader *r, int sock, SSL *ssl, const char *pending, size_t pending_len)
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

static int
header_is_chunked(const char *header)
{
	const char *line = header;

	while (*line) {
		const char *nl = strchr(line, '\n');
		const char *hdr;
		const char *end;
		const char *q;

		if (!nl)
			break;
		hdr = nl + 1;
		end = strpbrk(hdr, "\r\n");
		if (!end)
			end = hdr + strlen(hdr);
		if (strncasecmp(hdr, "Transfer-Encoding:", 18) == 0) {
			for (q = hdr + 18; q + 7 <= end; q++) {
				if (strncasecmp(q, "chunked", 7) == 0)
					return 1;
			}
		}
		line = hdr;
	}
	return 0;
}

/*
 * Parse Content-Length.  Returns 1 when present, 0 when absent, -1 when the
 * value is malformed.
 */
static int
header_content_length(const char *header, size_t *out)
{
	const char *line = header;

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
			if (p == end)
				return -1;
			errno = 0;
			v = strtoull(p, &num_end, 10);
			if (errno != 0 || num_end == p)
				return -1;
			while (num_end < end && (*num_end == ' ' || *num_end == '\t'))
				num_end++;
			if (num_end != end || v > SIZE_MAX)
				return -1;
			*out = (size_t)v;
			return 1;
		}
		line = hdr;
	}
	return 0;
}

static int
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
		if (!isxdigit((unsigned char)*p))
			return BODY_READ_ERR;
		while (isxdigit((unsigned char)*p)) {
			int digit;

			if (*p >= '0' && *p <= '9')
				digit = *p - '0';
			else if (*p >= 'a' && *p <= 'f')
				digit = *p - 'a' + 10;
			else
				digit = *p - 'A' + 10;
			if (size > (SIZE_MAX - (size_t)digit) / 16)
				return BODY_READ_ERR;
			size = size * 16 + (size_t)digit;
			p++;
		}

		if (size == 0) {
			/* Consume trailers through the final blank line. */
			for (;;) {
				if (stream_read_line(r, line, sizeof(line)) < 0)
					return BODY_OK;
				if (line[0] == '\0')
					return BODY_OK;
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
		if (stream_read_line(r, line, sizeof(line)) < 0)
			return BODY_READ_ERR;
	}
}

static int
parse_status(const char *buf, int *code, char **location)
{
	const char *p;
	char *end;
	long c;

	if (strncmp(buf, "HTTP/", 5) != 0)
		return -1;

	p = buf + 5;
	while (*p && *p != ' ')
		p++;
	if (!*p)
		return -1;
	p++;

	c = strtol(p, &end, 10);
	if (end == p || c < 100 || c > 999)
		return -1;
	*code = (int)c;

	if (location) {
		*location = NULL;
		if (*code >= 300 && *code < 400) {
			const char *line = buf;

			while (*line) {
				const char *nl = strchr(line, '\n');
				const char *hdr;

				if (!nl)
					break;
				hdr = nl + 1;
				if (strncasecmp(hdr, "Location:", 9) == 0) {
					const char *val_end;
					size_t len;

					hdr += 9;
					while (*hdr == ' ' || *hdr == '\t')
						hdr++;
					val_end = hdr;
					while (*val_end && *val_end != '\r' && *val_end != '\n')
						val_end++;
					while (val_end > hdr &&
					       (val_end[-1] == ' ' || val_end[-1] == '\t'))
						val_end--;
					len = (size_t)(val_end - hdr);
					*location = malloc(len + 1);
					if (*location) {
						memcpy(*location, hdr, len);
						(*location)[len] = '\0';
					}
					break;
				}
				line = hdr;
			}
		}
	}

	return 0;
}

static int
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

/*
 * Perform one HTTP transaction.  Returns:
 *   0  final 2xx response written to stdout
 *   1  final non-2xx response written to stdout (error already reported)
 *   2  redirect; *location_out owns the Location value
 *  -1  transport/protocol error
 */
static int
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
	ssize_t n;
	int rc = -1;

	*location_out = NULL;

	sock = connect_host(u->host, u->port, u->is_ssl, &ssl);
	if (sock < 0)
		goto done;

	host_header = build_host_header(u);
	if (!host_header)
		goto done;
	if (send_request(sock, ssl, host_header, u->path) < 0) {
		fprintf(stderr, "error: failed to send request\n");
		goto done;
	}

	header_cap = BUF_SIZE;
	header = malloc(header_cap);
	if (!header)
		goto done;

	for (;;) {
		n = read_some(sock, ssl, buf, sizeof(buf));
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				fprintf(stderr, "error: timeout after %d seconds\n", timeout_sec);
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
			if (!tmp)
				goto done;
			header = tmp;
			header_cap = new_cap;
		}
		memcpy(header + header_len, buf, (size_t)n);
		header_len += (size_t)n;
		header[header_len] = '\0';
		header_end = find_header_end(header, header_len);
		if (header_end >= 0)
			break;
		if (header_len > MAX_HEADER_SIZE) {
			fprintf(stderr, "error: response headers too large\n");
			goto done;
		}
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
		int length_rc;

		header[header_end] = '\0';
		parse_rc = parse_status(header, &code, &location);
		chunked = header_is_chunked(header);
		length_rc = chunked ? 0 :
			header_content_length(header, &content_length);
		header[header_end] = saved;

		if (parse_rc < 0) {
			fprintf(stderr, "error: failed to parse HTTP status\n");
			goto done;
		}
		if (length_rc < 0) {
			fprintf(stderr, "error: invalid Content-Length header\n");
			goto done;
		}
		have_content_length = length_rc;
	}

	if (code >= 300 && code < 400 && location && location[0] != '\0') {
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
				fprintf(stderr, "error: timeout after %d seconds\n", timeout_sec);
			else
				fprintf(stderr, "error: failed to read response body\n");
			goto done;
		}
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

static int
fetch_url(const char *start_url)
{
	char *current = strdup(start_url);
	int redirects = 0;

	if (!current) {
		fprintf(stderr, "error: out of memory\n");
		return -1;
	}

	for (;;) {
		url_parts u;
		char *location = NULL;
		char *next;
		int rc;

		if (parse_url(current, &u) < 0) {
			fprintf(stderr, "error: invalid URL: %s\n", current);
			free(current);
			return -1;
		}

		rc = do_request(&u, &location);

		if (rc != 2) {
			free_url_parts(&u);
			free(current);
			return rc;
		}

		if (redirects >= max_redirs) {
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

static int
parse_int_arg(const char *s, long min, long max, long *out)
{
	char *end;
	long v;

	errno = 0;
	v = strtol(s, &end, 10);
	if (errno != 0 || end == s || *end != '\0' || v < min || v > max)
		return -1;
	*out = v;
	return 0;
}

int
main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "A:kn:t:hV")) != -1) {
		switch (opt) {
		case 'A':
			useragent = optarg;
			break;
		case 'k':
			insecure = 1;
			break;
		case 'n': {
			long v;

			if (parse_int_arg(optarg, 0, INT_MAX, &v) < 0) {
				fprintf(stderr, "error: invalid redirect limit: %s\n", optarg);
				return 1;
			}
			max_redirs = (int)v;
			break;
		}
		case 't': {
			long v;

			if (parse_int_arg(optarg, 1, INT_MAX, &v) < 0) {
				fprintf(stderr, "error: invalid timeout: %s\n", optarg);
				return 1;
			}
			timeout_sec = (int)v;
			break;
		}
		case 'h':
			print_usage(argv[0]);
			return 0;
		case 'V':
			fprintf(stderr, "rurl %s\n", RURL_VERSION);
			return 0;
		default:
			print_usage(argv[0]);
			return 1;
		}
	}

	if (optind >= argc) {
		print_usage(argv[0]);
		return 1;
	}

	char *url = argv[optind];
	if (!url || url[0] == '\0') {
		fprintf(stderr, "error: empty URL\n");
		return 1;
	}

	if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
		fprintf(stderr, "error: URL must start with http:// or https://\n");
		return 1;
	}

	{
		int rc = fetch_url(url);

		return rc < 0 ? 1 : rc;
	}
}
