/*
 * rurl - Minimal raw URL fetcher (no libcurl)
 * Copyright (C) 2026  rurl contributors
 *
 * SPDX-License-Identifier: EUPL-1.2
 *
 * This file is part of rurl.  See the LICENSE file in the project root for
 * the full licence text.
 */

#ifndef RURL_H
#define RURL_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

#include <openssl/ssl.h>

#define MAX_REDIRS 20
#define RURL_VERSION "0.1.6"
#define RURL_USERAGENT "rurl/" RURL_VERSION
#define BUF_SIZE 8192
#define MAX_HEADER_SIZE (64 * 1024)
#define MAX_INTERIM_RESPONSES 10
#define TIMEOUT_SEC 30

typedef struct {
	int is_ssl;
	char *host;
	char *port;
	char *path;
} url_parts;

typedef struct {
	const char *useragent;
	int insecure;
	int max_redirs;
	int timeout_sec;
} rurl_config;

extern rurl_config config;

/* util.c */
void fprint_sanitized(FILE *out, const char *s);
char *concat2(const char *a, const char *b);
char *concat3(const char *a, const char *b, const char *c);
int valid_header_value(const char *s);
int parse_int_arg(const char *s, long min, long max, long *out);

/* url.c */
int http_scheme(const char *url, int *is_ssl, size_t *skip);
int has_uri_scheme(const char *s);
void free_url_parts(url_parts *u);
int parse_url(const char *url, url_parts *u);
void normalize_url_path(char *url);
char *resolve_location(const url_parts *base, const char *loc);
char *build_host_header(const url_parts *u);

/* net.c */
int connect_host(const char *host, const char *port, int is_ssl, SSL **ssl_out);
int send_request(int sock, SSL *ssl, const char *host_header, const char *path);
ssize_t read_some(int sock, SSL *ssl, void *buf, size_t len);

/* http.c */
int do_request(const url_parts *u, char **location_out);
int fetch_url(const char *start_url);

#endif /* RURL_H */
