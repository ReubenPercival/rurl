/*
 * rurl - Minimal raw URL fetcher (no libcurl)
 * Copyright (C) 2026  rurl contributors
 *
 * SPDX-License-Identifier: EUPL-1.2
 *
 * This file is part of rurl.  See the LICENSE file in the project root for
 * the full licence text.
 */

#include "rurl.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

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
			tv.tv_sec = config.timeout_sec;
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

int
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
	const char *server_name;
	char tls_host[256];
	unsigned char addr[sizeof(struct in6_addr)];

	if (host[0] == '[') {
		size_t hlen = strlen(host);

		if (hlen > 2 && host[hlen - 1] == ']') {
			size_t n = hlen - 2;

			lookup = malloc(n + 1);
			if (!lookup) {
				fprintf(stderr, "error: out of memory\n");
				return -1;
			}
			memcpy(lookup, host + 1, n);
			lookup[n] = '\0';
			lookup_host = lookup;
		}
	}

	server_name = lookup_host;

	if (inet_pton(AF_INET, lookup_host, addr) == 1 ||
	    inet_pton(AF_INET6, lookup_host, addr) == 1) {
		is_ip = 1;
	} else {
		size_t n = strlen(lookup_host);

		/* A trailing dot marks an absolute DNS name.  It is not part of
		 * the name presented to TLS or matched against a certificate. */
		if (n > 0 && n < sizeof(tls_host) && lookup_host[n - 1] == '.') {
			memcpy(tls_host, lookup_host, n - 1);
			tls_host[n - 1] = '\0';
			server_name = tls_host;
		}
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(lookup_host, port, &hints, &result) != 0) {
		fprintf(stderr, "error: failed to resolve host: ");
		fprint_sanitized(stderr, host);
		fputc('\n', stderr);
		free(lookup);
		return -1;
	}

	for (rp = result; rp; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock < 0) {
			saved_errno = errno;
			continue;
		}
		if (connect_with_timeout(sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		saved_errno = errno;
		close(sock);
		sock = -1;
	}

	freeaddrinfo(result);

	if (sock < 0) {
		if (saved_errno == ETIMEDOUT)
			fprintf(stderr, "error: connection timed out after %d seconds\n", config.timeout_sec);
		else {
			fprintf(stderr, "error: failed to connect to ");
			fprint_sanitized(stderr, host);
			fputc(':', stderr);
			fprint_sanitized(stderr, port);
			fputc('\n', stderr);
		}
		free(lookup);
		return -1;
	}

	tv.tv_sec = config.timeout_sec;
	tv.tv_usec = 0;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0 ||
	    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
		fprintf(stderr, "error: failed to set socket timeout\n");
		close(sock);
		free(lookup);
		return -1;
	}

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
		if (!config.insecure) {
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

		if (SSL_set_fd(ssl, sock) != 1) {
			fprintf(stderr, "error: SSL_set_fd failed\n");
			SSL_free(ssl);
			close(sock);
			free(lookup);
			return -1;
		}
		if (!is_ip && SSL_set_tlsext_host_name(ssl, server_name) != 1) {
			fprintf(stderr, "error: failed to set TLS server name\n");
			SSL_free(ssl);
			close(sock);
			free(lookup);
			return -1;
		}

		if (!config.insecure) {
			X509_VERIFY_PARAM *param = SSL_get0_param(ssl);
			int ok;

			if (is_ip)
				ok = X509_VERIFY_PARAM_set1_ip_asc(param, lookup_host);
			else
				ok = SSL_set1_host(ssl, server_name);
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
			long verify = SSL_get_verify_result(ssl);

			fprintf(stderr, "error: TLS handshake failed");
			if (reason)
				fprintf(stderr, ": %s", reason);
			if (verify != X509_V_OK)
				fprintf(stderr, " (certificate: %s)",
					X509_verify_cert_error_string(verify));
			fputc('\n', stderr);
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

#define REQUEST_FORMAT \
	"GET %s HTTP/1.1\r\n" \
	"Host: %s\r\n" \
	"User-Agent: %s\r\n" \
	"Accept: */*\r\n" \
	"Accept-Encoding: identity\r\n" \
	"Connection: close\r\n" \
	"\r\n"

int
send_request(int sock, SSL *ssl, const char *host_header, const char *path)
{
	char *buf;
	int len, total, n;
	int rc = 0;

	len = snprintf(NULL, 0, REQUEST_FORMAT, path, host_header, config.useragent);
	if (len < 0)
		return -1;
	buf = malloc((size_t)len + 1);
	if (!buf)
		return -1;
	snprintf(buf, (size_t)len + 1, REQUEST_FORMAT, path, host_header,
		 config.useragent);

	total = 0;
	while (total < len) {
		if (ssl) {
			n = SSL_write(ssl, buf + total, len - total);
			if (n <= 0) {
				int err = SSL_get_error(ssl, n);

				if (err == SSL_ERROR_WANT_READ ||
				    err == SSL_ERROR_WANT_WRITE) {
					rc = -2;
					break;
				}
				if (err == SSL_ERROR_SYSCALL && errno == EINTR)
					continue;
				if (err == SSL_ERROR_SYSCALL &&
				    (errno == EAGAIN || errno == EWOULDBLOCK)) {
					rc = -2;
					break;
				}
				rc = -1;
				break;
			}
		} else {
			n = (int)send(sock, buf + total, (size_t)(len - total), 0);
			if (n < 0 && errno == EINTR)
				continue;
			if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
				rc = -2;
				break;
			}
			if (n <= 0) {
				rc = -1;
				break;
			}
		}
		total += n;
	}

	free(buf);
	return rc;
}

ssize_t
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
