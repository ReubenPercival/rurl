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
#include <strings.h>
#include <ctype.h>
#include <errno.h>

int
http_scheme(const char *url, int *is_ssl, size_t *skip)
{
	if (strncasecmp(url, "http://", 7) == 0) {
		*is_ssl = 0;
		*skip = 7;
		return 1;
	}
	if (strncasecmp(url, "https://", 8) == 0) {
		*is_ssl = 1;
		*skip = 8;
		return 1;
	}
	return 0;
}

int
has_uri_scheme(const char *s)
{
	const char *p;

	if (!isalpha((unsigned char)s[0]))
		return 0;
	for (p = s + 1; *p; p++) {
		if (*p == ':')
			return 1;
		if (!(isalnum((unsigned char)*p) || *p == '+' ||
		      *p == '-' || *p == '.'))
			return 0;
	}
	return 0;
}

static int
valid_port(const char *s)
{
	const char *p;
	char *end;
	long v;

	if (!isdigit((unsigned char)*s))
		return 0;
	for (p = s; *p; p++) {
		if (!isdigit((unsigned char)*p))
			return 0;
	}
	errno = 0;
	v = strtol(s, &end, 10);
	return !(errno != 0 || end == s || *end != '\0' || v < 1 || v > 65535);
}

void
free_url_parts(url_parts *u)
{
	free(u->host);
	free(u->port);
	free(u->path);
	u->host = NULL;
	u->port = NULL;
	u->path = NULL;
}

int
parse_url(const char *url, url_parts *u)
{
	const char *p = url;
	const char *host_start;
	const char *host_end;
	const char *at;
	const char *c;
	size_t host_len;
	size_t scheme_len;
	char *colon;

	memset(u, 0, sizeof(*u));

	if (!http_scheme(p, &u->is_ssl, &scheme_len))
		return -1;
	p += scheme_len;

	/* Reject CR/LF up front: they would allow request-header injection. */
	if (strpbrk(p, "\r\n") != NULL)
		return -1;

	host_start = p;
	host_end = p + strcspn(p, "/?#");
	host_len = (size_t)(host_end - host_start);

	/* Strip optional userinfo (user:password@host).  Use the last '@'
	 * so that a raw '@' in the password is not mistaken for the host
	 * delimiter. */
	at = NULL;
	for (c = host_start; c < host_end; c++) {
		if (*c == '@')
			at = c;
	}
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

	for (c = u->host; *c; c++) {
		if ((unsigned char)*c <= 0x20 || (unsigned char)*c == 0x7f ||
		    *c == '@')
			goto fail;
	}

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
void
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
			/* A trailing "/." denotes a directory; keep the slash. */
			if (i == len && (r == 0 || result[r - 1] != '/'))
				result[r++] = '/';
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
			/* Likewise for a trailing "/..". */
			if (i == len && (r == 0 || result[r - 1] != '/'))
				result[r++] = '/';
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
char *
resolve_location(const url_parts *base, const char *loc)
{
	const char *scheme = base->is_ssl ? "https" : "http";
	int default_port = base->is_ssl ? 443 : 80;
	const char *path = base->path;
	const char *query = strchr(path, '?');
	size_t path_len = query ? (size_t)(query - path) : strlen(path);
	char *origin;
	char *result;

	/* Absolute URI (including unsupported schemes, which are rejected by
	 * parse_url rather than being mistaken for relative paths). */
	if (has_uri_scheme(loc)) {
		result = strdup(loc);
		if (result)
			normalize_url_path(result);
		return result;
	}

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

char *
build_host_header(const url_parts *u)
{
	int default_port = u->is_ssl ? 443 : 80;

	if (atoi(u->port) == default_port)
		return strdup(u->host);
	return concat3(u->host, ":", u->port);
}
