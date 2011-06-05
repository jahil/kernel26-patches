/*
 * skaidrus, an example transparent proxy application
 * Copyright (C) 2004, 2005 Lennert Buytenhek
 *
 * This program is free software; you can redistribute and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
                                                                                
#ifndef __SKAIDRUS_H
#define __SKAIDRUS_H

#include <iv.h>
#include <netinet/in.h>

struct skaidrus_instance
{
	struct sockaddr_in	local_addr;

	struct iv_fd		listen_fd;
	struct list_head	connections;
};

int register_skaidrus_instance(struct skaidrus_instance *si);
void unregister_skaidrus_instance(struct skaidrus_instance *si);


#endif
