/*
 * rurl - Minimal raw URL fetcher (no libcurl)
 * Copyright (C) 2026  rurl contributors
 *
 * SPDX-License-Identifier: EUPL-1.2
 *
 * This file is part of rurl.  See the LICENSE file in the project root for
 * the full licence text.
 */

#ifndef RURL_HTTP_INTERNAL_H
#define RURL_HTTP_INTERNAL_H
#include "rurl.h"

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

void stream_init(stream_reader *r, int sock, SSL *ssl, const char *pending,
		 size_t pending_len);
int write_plain_body(stream_reader *r, size_t limit, int have_limit);
int write_chunked_body(stream_reader *r);

int header_transfer_encoding(const char *header);
int header_content_length(const char *header, size_t *out);
int parse_status(const char *buf, int *code, char **location);
int find_header_end(const char *buf, size_t len);
int is_redirect_code(int code);

#endif /* RURL_HTTP_INTERNAL_H */
