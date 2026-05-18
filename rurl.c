/*
 * rurl - Minimal raw URL fetcher (no libcurl)
 * Copyright (C) 2026  SocialistSoftwareFoundation
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_REDIRS 20
#define BUF_SIZE 8192
#define TIMEOUT_SEC 30

static char *useragent = "rurl/0.1.3";
static int insecure = 0;
static int max_redirs = MAX_REDIRS;
static int timeout_sec = TIMEOUT_SEC;
static time_t start_time;

static void
print_usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [-A USERAGENT] [-k] [-n MAXREDIRS] [-t TIMEOUT] URL\n", prog);
	fprintf(stderr, "  -A USERAGENT  Set custom User-Agent string (default: rurl/0.1.3)\n");
	fprintf(stderr, "  -k            Allow insecure SSL connections\n");
	fprintf(stderr, "  -n MAXREDIRS  Maximum redirects (default: %d)\n", MAX_REDIRS);
	fprintf(stderr, "  -t TIMEOUT    Timeout in seconds (default: %d)\n", TIMEOUT_SEC);
	fprintf(stderr, "  -h            Show this help message\n");
	fprintf(stderr, "  -V            Show version\n");
}

static int
parse_url(const char *url, char **host, char **port, char **path, int *is_ssl)
{
	const char *p = url;
	const char *host_start;
	const char *host_end;
	const char *path_start;
	size_t host_len;
	size_t path_len;

	if (strncmp(p, "http://", 7) == 0) {
		*is_ssl = 0;
		p += 7;
	} else if (strncmp(p, "https://", 8) == 0) {
		*is_ssl = 1;
		p += 8;
	} else {
		return -1;
	}

	host_start = p;
	host_end = strchr(p, '/');
	if (!host_end) {
		host_len = strlen(p);
		path_start = "/";
		path_len = 1;
	} else {
		host_len = host_end - p;
		path_start = host_end;
		path_len = strlen(host_end);
	}

	*host = malloc(host_len + 1);
	if (!*host) return -1;
	memcpy(*host, host_start, host_len);
	(*host)[host_len] = '\0';

	char *colon = strchr(*host, ':');
	if (colon) {
		*port = strdup(colon + 1);
		*colon = '\0';
	} else {
		*port = *is_ssl ? strdup("443") : strdup("80");
	}

	*path = malloc(path_len + 1);
	if (!*path) { free(*host); free(*port); return -1; }
	memcpy(*path, path_start, path_len);
	(*path)[path_len] = '\0';

	return 0;
}

static int
connect_host(const char *host, const char *port, int is_ssl, SSL **ssl_out)
{
	struct addrinfo hints, *result, *rp;
	int sock = -1;
	SSL *ssl = NULL;
	SSL_CTX *ctx = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(host, port, &hints, &result) != 0) {
		fprintf(stderr, "error: failed to resolve host: %s\n", host);
		return -1;
	}

	for (rp = result; rp; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock < 0) continue;
		if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) break;
		close(sock);
		sock = -1;
	}

	freeaddrinfo(result);

	if (sock < 0) {
		fprintf(stderr, "error: failed to connect to %s:%s\n", host, port);
		return -1;
	}

	if (is_ssl) {
		SSL_library_init();
		SSL_load_error_strings();
		ctx = SSL_CTX_new(TLS_client_method());
		if (!ctx) {
			close(sock);
			fprintf(stderr, "error: SSL_CTX_new failed\n");
			return -1;
		}

		ssl = SSL_new(ctx);
		SSL_CTX_free(ctx);
		if (!ssl) {
			close(sock);
			fprintf(stderr, "error: SSL_new failed\n");
			return -1;
		}

		SSL_set_fd(ssl, sock);

		if (insecure) {
			SSL_set_verify(ssl, SSL_VERIFY_NONE, NULL);
		}

		if (SSL_connect(ssl) != 1) {
			fprintf(stderr, "error: SSL handshake failed\n");
			SSL_free(ssl);
			close(sock);
			return -1;
		}
	}

	*ssl_out = ssl;
	return sock;
}

static int
send_request(int sock, SSL *ssl, const char *host, const char *path)
{
	char buf[1024];
	int len;

	len = snprintf(buf, sizeof(buf),
		"GET %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"User-Agent: %s\r\n"
		"Connection: close\r\n"
		"\r\n",
		path, host, useragent);

	if (ssl) {
		if (SSL_write(ssl, buf, len) <= 0) return -1;
	} else {
		if (send(sock, buf, len, 0) <= 0) return -1;
	}

	return 0;
}

static int
parse_status(const char *buf, int *code, char **location)
{
	const char *p;
	char *end;
	long c;

	if (strncmp(buf, "HTTP/", 5) != 0) return -1;

	p = buf + 5;
	while (*p && *p != ' ') p++;
	if (!*p) return -1;
	p++;

	c = strtol(p, &end, 10);
	if (end == p) return -1;
	*code = (int)c;

	if (location) {
		*location = NULL;
		if (*code >= 300 && *code < 400) {
			const char *loc = strstr(buf, "Location:");
			if (loc) {
				loc += 9;
				while (*loc == ' ' || *loc == '\t') loc++;
				const char *end = loc;
				while (*end && *end != '\r' && *end != '\n') end++;
				size_t len = end - loc;
				*location = malloc(len + 1);
				if (*location) {
					memcpy(*location, loc, len);
					(*location)[len] = '\0';
				}
			}
		}
	}

	return 0;
}

static int
find_header_end(const char *buf, size_t len)
{
	const char *p = buf;
	size_t i;

	for (i = 0; i + 3 < len; i++) {
		if (p[i] == '\r' && p[i+1] == '\n' && p[i+2] == '\r' && p[i+3] == '\n') {
			return (int)(i + 4);
		}
	}
	return -1;
}

static int
fetch_url(const char *url, int depth)
{
	char *host = NULL, *port = NULL, *path = NULL;
	int is_ssl;
	int sock = -1;
	SSL *ssl = NULL;
	char *read_buf = NULL;
	size_t buf_cap = 0;
	size_t buf_len = 0;
	int code;
	char *location = NULL;
	int ret = -1;
	ssize_t n;

	if (depth > max_redirs) {
		fprintf(stderr, "error: too many redirects\n");
		return -1;
	}

	if (parse_url(url, &host, &port, &path, &is_ssl) < 0) {
		fprintf(stderr, "error: invalid URL\n");
		return -1;
	}

	start_time = time(NULL);

	sock = connect_host(host, port, is_ssl, &ssl);
	if (sock < 0) {
		goto done;
	}

	if (send_request(sock, ssl, host, path) < 0) {
		fprintf(stderr, "error: failed to send request\n");
		goto done;
	}

	buf_cap = BUF_SIZE;
	read_buf = malloc(buf_cap);
	if (!read_buf) goto done;

	while (1) {
		if (buf_len >= buf_cap - 1) {
			buf_cap *= 2;
			char *new_buf = realloc(read_buf, buf_cap);
			if (!new_buf) goto done;
			read_buf = new_buf;
		}

		if (ssl) {
			n = SSL_read(ssl, read_buf + buf_len, (int)(buf_cap - buf_len - 1));
		} else {
			n = recv(sock, read_buf + buf_len, buf_cap - buf_len - 1, 0);
		}

		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (time(NULL) - start_time > timeout_sec) {
					fprintf(stderr, "error: timeout after %d seconds\n", timeout_sec);
					goto done;
				}
				usleep(10000);
				continue;
			}
			break;
		}
		if (n == 0) break;

		buf_len += n;
		read_buf[buf_len] = '\0';
	}

	if (buf_len == 0) {
		fprintf(stderr, "error: empty response\n");
		goto done;
	}

	int header_end = find_header_end(read_buf, buf_len);
	if (header_end < 0) {
		fprintf(stderr, "error: invalid response\n");
		goto done;
	}

	read_buf[header_end] = '\0';

	if (parse_status(read_buf, &code, &location) < 0) {
		fprintf(stderr, "error: failed to parse HTTP status\n");
		goto done;
	}

	size_t body_len = buf_len - header_end;
	if (fwrite(read_buf + header_end, 1, body_len, stdout) != body_len) {
		fprintf(stderr, "error: failed to write to stdout\n");
		goto done;
	}

	if (code >= 200 && code < 300) {
		ret = 0;
	} else if (code >= 300 && code < 400 && location) {
		ret = fetch_url(location, depth + 1);
	} else {
		fprintf(stderr, "error: HTTP %d\n", code);
		ret = 1;
	}

done:
	free(location);
	free(read_buf);
	free(host);
	free(port);
	free(path);
	if (ssl) SSL_free(ssl);
	if (sock >= 0) close(sock);

	return ret;
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
		case 'n':
			max_redirs = atoi(optarg);
			if (max_redirs <= 0) max_redirs = MAX_REDIRS;
			break;
		case 't':
			timeout_sec = atoi(optarg);
			if (timeout_sec <= 0) timeout_sec = TIMEOUT_SEC;
			break;
		case 'h':
			print_usage(argv[0]);
			return 0;
		case 'V':
			fprintf(stderr, "rurl 0.1.3\n");
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

	return fetch_url(url, 0);
}