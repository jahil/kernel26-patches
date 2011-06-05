/*
 * skaidrus, an example transparent proxy application
 * Copyright (C) 2004, 2005 Lennert Buytenhek
 *
 * This program is free software; you can redistribute and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __SKAIDRUS_PRIVATE_H
#define __SKAIDRUS_PRIVATE_H

struct connection
{
	struct list_head		list;
	struct skaidrus_instance	*si;
	int				state;
	struct iv_fd			user_fd;
	struct iv_fd			server_fd;
	int				us_buf_length;
	int				us_saw_fin;
	int				su_buf_length;
	int				su_saw_fin;
	unsigned char			us_buf[4096];
	unsigned char			su_buf[4096];
};


/* After accepting the user connection and initiating the tproxy connect
 * to the backend server (for which we're faking the user's original source
 * address), we're now waiting for that tproxy connect to either succeed
 * or fail.
 */
#define CONNECTION_STATE_WAITING_CONN	(0)

/* The tproxy connect succeeded, we're now bidirectionally proxying the
 * user connection and the connection to the backend server.
 */
#define CONNECTION_STATE_FORWARDING	(1)


#endif
