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
#include <errno.h>

/*
 * Print untrusted text without letting control characters reach the terminal.
 * Printable non-ASCII bytes are passed through untouched.
 */
void
fprint_sanitized(FILE *out, const char *s)
{
	const unsigned char *p = (const unsigned char *)s;

	for (; *p; p++) {
		if (*p < 0x20 || *p == 0x7f || (*p >= 0x80 && *p <= 0x9f))
			fprintf(out, "\\x%02x", (unsigned int)*p);
		else
			fputc(*p, out);
	}
}

char *
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

char *
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

int
valid_header_value(const char *s)
{
	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;

		if ((c < 0x20 && c != '\t') || c == 0x7f)
			return 0;
	}
	return 1;
}

int
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
