/*
 * skaidrus, an example transparent proxy application
 * Copyright (C) 2004, 2005 Lennert Buytenhek
 *
 * This program is free software; you can redistribute and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
                                                                                
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <iv.h>
#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_tproxy.h>
#include <netinet/in.h>
#include <syslog.h>
#include <unistd.h>
#include "skaidrus.h"

static int detect_tproxy_v2(void)
{
	struct in_tproxy itp;
	int sock;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		return 1;
	}

	itp.op = TPROXY_VERSION;
	itp.v.version = 0x02000000;
	if (setsockopt(sock, SOL_IP, IP_TPROXY, &itp, sizeof(itp)) < 0) {
		perror("setsockopt(SOL_IP, IP_TPROXY, TPROXY_VERSION)");
		close(sock);
		return 1;
	}

	close(sock);

	return 0;
}

int main()
{
	struct skaidrus_instance si;

	fprintf(stderr, "skaidrus, an example transparent proxy application\n");
	fprintf(stderr, "\n");

	if (geteuid() != 0) {
		fprintf(stderr, "error: need root privileges for performing "
				"transparent proxy functions\n");
		return 1;
	}

	if (detect_tproxy_v2()) {
		fprintf(stderr, "error: version >= 2.0.0 of linux transparent "
				"proxy patches not detected\n");
		return 1;
	}

	iv_init();
	openlog("skaidrus", LOG_NDELAY | LOG_PERROR, LOG_USER);

	si.local_addr.sin_family = AF_INET;
	si.local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	si.local_addr.sin_port = htons(9841);
	if (!register_skaidrus_instance(&si)) {
		fprintf(stderr, "can't register skaidrus instance\n");
		return 1;
	}

	fprintf(stderr, "up and running\n");
	iv_main();

	return 0;
}
