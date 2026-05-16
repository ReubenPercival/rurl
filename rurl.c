/*
 * rurl - Minimal raw URL fetcher
 * Copyright (C) 2026  SocialistSofwareFoundation
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
#include <curl/curl.h>

static size_t
write_cb(void *data, size_t size, size_t nmemb, void *userp)
{
	size_t bytes = size * nmemb;
	(void)userp;
	fwrite(data, 1, bytes, stdout);
	return bytes;
}

int
main(int argc, char *argv[])
{
	CURL *curl;
	CURLcode res;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s URL\n", argv[0]);
		return 1;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();

	if (!curl) {
		fprintf(stderr, "curl_easy_init failed\n");
		curl_global_cleanup();
		return 1;
	}

	curl_easy_setopt(curl, CURLOPT_URL, argv[1]);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

	res = curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	curl_global_cleanup();

	if (res != CURLE_OK) {
		fprintf(stderr, "error: %s\n", curl_easy_strerror(res));
		return 1;
	}

	return 0;
}