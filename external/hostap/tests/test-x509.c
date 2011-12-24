/*
 * Testing tool for X.509v3 routines
 * Copyright (c) 2006-2009, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"

#include "common.h"
#include "tls/x509v3.h"

extern int wpa_debug_level;


int main(int argc, char *argv[])
{
	FILE *f;
	u8 buf[3000];
	size_t len;
	struct x509_certificate *cert;

	wpa_debug_level = 0;

	f = fopen(argv[1], "rb");
	if (f == NULL)
		return -1;
	len = fread(buf, 1, sizeof(buf), f);
	fclose(f);

	cert = x509_certificate_parse(buf, len);
	if (cert == NULL)
		printf("Failed to parse X.509 certificate\n");
	x509_certificate_free(cert);

	return 0;
}
