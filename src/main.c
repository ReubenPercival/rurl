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
#include <limits.h>
#include <signal.h>
#include <unistd.h>

rurl_config config = {
	RURL_USERAGENT,
	0,
	MAX_REDIRS,
	TIMEOUT_SEC
};

static void
print_usage(FILE *out, const char *prog)
{
	fprintf(out, "Usage: %s [-A USERAGENT] [-k] [-n MAXREDIRS] [-t TIMEOUT] URL\n", prog);
	fprintf(out, "  -A USERAGENT  Set custom User-Agent string (default: %s)\n", RURL_USERAGENT);
	fprintf(out, "  -k            Allow insecure SSL connections\n");
	fprintf(out, "  -n MAXREDIRS  Maximum redirects (default: %d)\n", MAX_REDIRS);
	fprintf(out, "  -t TIMEOUT    Timeout in seconds (default: %d)\n", TIMEOUT_SEC);
	fprintf(out, "  -h            Show this help message\n");
	fprintf(out, "  -V            Show version\n");
}

int
main(int argc, char *argv[])
{
	int opt;
	int scheme_ssl;
	size_t scheme_len;

	/* A closed pipe or a peer that resets the connection must not kill the
	 * process before errors can be reported and resources released. */
	signal(SIGPIPE, SIG_IGN);

	while ((opt = getopt(argc, argv, "A:kn:t:hV")) != -1) {
		switch (opt) {
		case 'A':
			if (!valid_header_value(optarg)) {
				fprintf(stderr, "error: invalid User-Agent value\n");
				return 1;
			}
			config.useragent = optarg;
			break;
		case 'k':
			config.insecure = 1;
			break;
		case 'n': {
			long v;

			if (parse_int_arg(optarg, 0, INT_MAX, &v) < 0) {
				fprintf(stderr, "error: invalid redirect limit: ");
				fprint_sanitized(stderr, optarg);
				fputc('\n', stderr);
				return 1;
			}
			config.max_redirs = (int)v;
			break;
		}
		case 't': {
			long v;

			if (parse_int_arg(optarg, 1, INT_MAX, &v) < 0) {
				fprintf(stderr, "error: invalid timeout: ");
				fprint_sanitized(stderr, optarg);
				fputc('\n', stderr);
				return 1;
			}
			config.timeout_sec = (int)v;
			break;
		}
		case 'h':
			print_usage(stdout, argv[0]);
			return 0;
		case 'V':
			fprintf(stdout, "rurl %s\n", RURL_VERSION);
			return 0;
		default:
			print_usage(stderr, argv[0]);
			return 1;
		}
	}

	if (optind >= argc) {
		print_usage(stderr, argv[0]);
		return 1;
	}
	if (optind + 1 < argc) {
		fprintf(stderr, "error: too many arguments\n");
		return 1;
	}

	{
		char *url = argv[optind];

		if (!url || url[0] == '\0') {
			fprintf(stderr, "error: empty URL\n");
			return 1;
		}

		if (!http_scheme(url, &scheme_ssl, &scheme_len)) {
			fprintf(stderr, "error: URL must start with http:// or https://\n");
			return 1;
		}
	}

	{
		int rc = fetch_url(argv[optind]);

		return rc < 0 ? 1 : rc;
	}
}
