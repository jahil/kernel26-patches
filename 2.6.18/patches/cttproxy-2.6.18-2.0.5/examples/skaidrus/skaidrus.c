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
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include "skaidrus.h"
#include "skaidrus_private.h"


/* util functions ***********************************************************/
static int determine_source_address(struct sockaddr_in *destination,
				    struct sockaddr_in *source)
{
	struct sockaddr_in addr;
	socklen_t addrlen;
	char textaddr[32];
	int sock;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		syslog(LOG_CRIT, "determine_source_address: error "
				 "%d while creating socket", errno);
		abort();
	}

	addr = *destination;
	addr.sin_port = htons(80);
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		syslog(LOG_ALERT, "determine_source_address: error %d "
				  "while looking up route for %d", errno,
				  inet_aton(textaddr, &(addr.sin_addr)));
		close(sock);
		return -1;
	}

	addrlen = sizeof(*source);
	if (getsockname(sock, (struct sockaddr *)source, &addrlen) < 0) {
		syslog(LOG_CRIT, "determine_source_address: error %d "
				 "while retrieving source address", errno);
		abort();
	}
	source->sin_port = 0;

	close(sock);

	return 0;
}


/* If we're going to tproxy an outgoing connection anyway, we do not have
 * to bind to the very same source address that we would use in case we
 * wouldn't be tproxy'ing.
 *
 * In earlier versions of the tproxy code, at least in 1.9.6, there used
 * to be an issue where tproxy (ip_conntrack) and the local TCP stack
 * would disagree about which local port numbers are in use, which would
 * sometimes lead to outgoing connections being tproxy'd with the wrong
 * source address, being the source address that was previously used
 * for this local port.  (So if we would tproxy-connect from local port
 * number 12345 with source address 1.2.3.4, and a few minutes later from
 * local port number 12345 with source address 2.3.4.5, 1.2.3.4 would
 * be used instead of 2.3.4.5 as source address even if the connection
 * with source address 1.2.3.4 would already have been terminated.)
 *
 * One possible way to work around that is to assign a set of 'fake' IP
 * addresses to localhost, say 10.10.10.{1-255}, and to round-robin local
 * port allocation over these IPs, binding ~50000 (or whatever the size
 * of your ip_local_port_range is) connections to each IP before moving
 * on to the next one.
 */
static int bind_to_source_address(int fd, struct sockaddr_in *addr)
{
	struct sockaddr_in source;

	if (determine_source_address(addr, &source) < 0)
		return -1;

	if (bind(fd, (struct sockaddr *)&source, sizeof(source)) < 0) {
		perror("bind");
		return -1;
	}

	return 0;
}

static int is_local_address(struct sockaddr_in *_addr)
{
	struct sockaddr_in addr;
	int is_local;
	int sock;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		syslog(LOG_CRIT, "is_local_address: error %d in socket(2)", errno);
		abort();
	}

	addr = *_addr;
	addr.sin_port = htons(0);

	is_local = 0;
	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
		is_local = 1;

	close(sock);

	return is_local;
}


/* state FORWARDING *********************************************************/
static void got_user_data(void *_req);
static void got_user_write_space(void *_req);
static void got_server_data(void *_req);
static void got_server_write_space(void *_req);

static void connection_kill(struct connection *req)
{
	list_del(&(req->list));

	iv_unregister_fd(&(req->user_fd));
	close(req->user_fd.fd);
	iv_unregister_fd(&(req->server_fd));
	close(req->server_fd.fd);
	free(req);
}


static void got_user_data(void *_req)
{
	struct connection *req = (struct connection *)_req;
	unsigned char *ptr;
	int space;
	int ret;

	ptr = req->us_buf + req->us_buf_length;
	space = sizeof(req->us_buf) - req->us_buf_length;
	if (!space) {
		syslog(LOG_CRIT, "got_user_data: no space");
		abort();
	}

	ret = iv_read(&(req->user_fd), ptr, space);
	if (ret < 0) {
		if (errno != EAGAIN)
			connection_kill(req);
		return;
	}

	if (!req->us_buf_length && req->state != CONNECTION_STATE_WAITING_CONN)
		iv_fd_set_handler_out(&(req->server_fd), got_server_write_space);

	if (ret == 0) {
		req->us_saw_fin = 1;
		iv_fd_set_handler_in(&(req->user_fd), NULL);
		return;
	}

	req->us_buf_length += ret;
	if (req->us_buf_length == sizeof(req->us_buf))
		iv_fd_set_handler_in(&(req->user_fd), NULL);
}

static void got_user_write_space(void *_req)
{
	struct connection *req = (struct connection *)_req;

	if (!req->su_buf_length && req->su_saw_fin != 1) {
		syslog(LOG_CRIT, "got_user_write_space: nothing to do");
		abort();
	}

	if (req->su_buf_length) {
		int ret;

		ret = iv_write(&(req->user_fd), req->su_buf, req->su_buf_length);
		if (ret <= 0) {
			if (ret == 0 || errno != EAGAIN)
				connection_kill(req);
			return;
		}

		if (req->su_buf_length == sizeof(req->su_buf) && !req->su_saw_fin)
			iv_fd_set_handler_in(&(req->server_fd), got_server_data);
		req->su_buf_length -= ret;
		memmove(req->su_buf, req->su_buf + ret, req->su_buf_length);
	}

	if (!req->su_buf_length) {
		iv_fd_set_handler_out(&(req->user_fd), NULL);
		switch (req->su_saw_fin) {
		case 0:
			break;
		case 1:
			req->su_saw_fin = 2;
			shutdown(req->user_fd.fd, SHUT_WR);
			if (req->us_saw_fin == 2)
				connection_kill(req);
			break;
		case 2:
			syslog(LOG_CRIT, "got_user_write_space: already relayed fin");
			abort();
		}
	}
}

static void got_user_error(void *_req)
{
	struct connection *req = (struct connection *)_req;
	int retlen;
	int ret;

	retlen = sizeof(ret);
	if (getsockopt(req->user_fd.fd, SOL_SOCKET, SO_ERROR, &ret, &retlen) < 0) {
		syslog(LOG_CRIT, "got_user_error: error %d while getsockopt(SO_ERROR)", errno);
		abort();
	}

	if (ret == 0) {
		syslog(LOG_CRIT, "got_user_error: no error?!");
		abort();
	}

	connection_kill(req);
}


static void got_server_data(void *_req)
{
	struct connection *req = (struct connection *)_req;
	unsigned char *ptr;
	int space;
	int ret;

	ptr = req->su_buf + req->su_buf_length;
	space = sizeof(req->su_buf) - req->su_buf_length;
	if (!space) {
		syslog(LOG_CRIT, "got_server_data: no space");
		abort();
	}

	ret = iv_read(&(req->server_fd), ptr, space);
	if (ret < 0) {
		if (errno != EAGAIN)
			connection_kill(req);
		return;
	}

	if (!req->su_buf_length && req->state == CONNECTION_STATE_FORWARDING)
		iv_fd_set_handler_out(&(req->user_fd), got_user_write_space);

	if (ret == 0) {
		req->su_saw_fin = 1;
		iv_fd_set_handler_in(&(req->server_fd), NULL);
		return;
	}

	req->su_buf_length += ret;
	if (req->su_buf_length == sizeof(req->su_buf))
		iv_fd_set_handler_in(&(req->server_fd), NULL);
}

static void got_server_write_space(void *_req)
{
	struct connection *req = (struct connection *)_req;

	if (!req->us_buf_length && req->us_saw_fin != 1) {
		syslog(LOG_CRIT, "got_server_write_space: nothing to do");
		abort();
	}

	if (req->us_buf_length) {
		int ret;

		ret = iv_write(&(req->server_fd), req->us_buf, req->us_buf_length);
		if (ret <= 0) {
			if (ret == 0 || errno != EAGAIN)
				connection_kill(req);
			return;
		}

		if (req->us_buf_length == sizeof(req->us_buf) && !req->us_saw_fin)
			iv_fd_set_handler_in(&(req->user_fd), got_user_data);
		req->us_buf_length -= ret;
		memmove(req->us_buf, req->us_buf + ret, req->us_buf_length);
	}

	if (!req->us_buf_length) {
		iv_fd_set_handler_out(&(req->server_fd), NULL);
		switch (req->us_saw_fin) {
		case 0:
			break;
		case 1:
			req->us_saw_fin = 2;
			shutdown(req->server_fd.fd, SHUT_WR);
			if (req->su_saw_fin == 2)
				connection_kill(req);
			break;
		case 2:
			syslog(LOG_CRIT, "got_server_write_space: already relayed fin");
			abort();
		}
	}
}

static void got_server_error(void *_req)
{
	struct connection *req = (struct connection *)_req;
	int retlen;
	int ret;

	retlen = sizeof(ret);
	if (getsockopt(req->server_fd.fd, SOL_SOCKET, SO_ERROR, &ret, &retlen) < 0) {
		syslog(LOG_CRIT, "got_server_error: error %d while getsockopt(SO_ERROR)", errno);
		abort();
	}

	if (ret == 0) {
		syslog(LOG_CRIT, "got_server_error: no error?!");
		abort();
	}

	connection_kill(req);
}


/* state WAITING_CONN *******************************************************/
static void got_server_connect(void *_req)
{
	struct connection *req = (struct connection *)_req;
	int retlen;
	int ret;

	retlen = sizeof(ret);
	if (getsockopt(req->server_fd.fd, SOL_SOCKET, SO_ERROR, &ret, &retlen) < 0) {
		syslog(LOG_CRIT, "got_server_connect: error %d while getsockopt(SO_ERROR)", errno);
		abort();
	}

	if (ret) {
		if (ret != EINPROGRESS)
			connection_kill(req);
		return;
	}

	iv_fd_set_handler_in(&(req->server_fd), got_server_data);
	if (req->us_buf_length)
		iv_fd_set_handler_out(&(req->server_fd), got_server_write_space);
	else
		iv_fd_set_handler_out(&(req->server_fd), NULL);
	iv_fd_set_handler_err(&(req->server_fd), got_server_error);

	req->su_buf_length = 0;
	req->su_saw_fin = 0;

	req->state = CONNECTION_STATE_FORWARDING;
}


/* new connection ***********************************************************/
static void connection_new(struct skaidrus_instance *si, int fd, struct sockaddr_in *src)
{
	struct sockaddr_in addr;
	struct in_tproxy itp;
	struct connection *req;
	socklen_t addrlen;
	int server_fd;
	int ret;

	addrlen = sizeof(addr);
	if (getsockopt(fd, SOL_IP, SO_ORIGINAL_DST, &addr, &addrlen) < 0) {
		syslog(LOG_INFO, "connection_new: no SO_ORIGINAL_DST");
		close(fd);
		return;
	}

	if (is_local_address(&addr)) {
		syslog(LOG_ALERT, "connection_new: originally for me, dropping");
		close(fd);
		return;
	}

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		syslog(LOG_ALERT, "connection_new: socket err %d", errno);
		close(fd);
		return;
	}

	if (bind_to_source_address(server_fd, &addr) < 0) {
		close(server_fd);
		close(fd);
		return;
	}

	itp.op = TPROXY_ASSIGN;
	itp.v.addr.faddr = src->sin_addr;
	itp.v.addr.fport = htons(0);
	if (setsockopt(server_fd, SOL_IP, IP_TPROXY, &itp, sizeof(itp)) < 0) {
		syslog(LOG_ALERT, "connection_new: tproxy_assign err %d", errno);
		close(server_fd);
		close(fd);
		return;
	}

	itp.op = TPROXY_FLAGS;
	itp.v.flags = ITP_CONNECT;
	if (setsockopt(server_fd, SOL_IP, IP_TPROXY, &itp, sizeof(itp)) < 0) {
		syslog(LOG_ALERT, "connection_new: tproxy_flags err %d", errno);
		close(server_fd);
		close(fd);
		return;
	}

	req = malloc(sizeof(*req));
	if (req == NULL) {
		syslog(LOG_CRIT, "connection_new: out of memory");
		close(server_fd);
		close(fd);
		return;
	}

	list_add_tail(&(req->list), &(si->connections));
	req->si = si;
	req->state = CONNECTION_STATE_WAITING_CONN;
	INIT_IV_FD(&(req->user_fd));
	req->user_fd.fd = fd;
	req->user_fd.cookie = (void *)req;
	req->user_fd.handler_in = got_user_data;
	req->user_fd.handler_err = got_user_error;
	iv_register_fd(&(req->user_fd));
	INIT_IV_FD(&(req->server_fd));
	req->server_fd.fd = server_fd;
	req->server_fd.cookie = (void *)req;
	req->server_fd.handler_in = got_server_connect;
	req->server_fd.handler_out = got_server_connect;
	iv_register_fd(&(req->server_fd));
	req->us_buf_length = 0;
	req->us_saw_fin = 0;

	ret = iv_connect(&(req->server_fd), (struct sockaddr *)&addr, sizeof(addr));
	if (ret == 0 || errno != EINPROGRESS)
		got_server_connect((void *)req);
}

static void got_connection(void *_si)
{
	struct skaidrus_instance *si = (struct skaidrus_instance *)_si;
	struct sockaddr_in addr;
	socklen_t addrlen;
	int yes;
	int fd;

	addrlen = sizeof(addr);
	fd = iv_accept(&(si->listen_fd), (struct sockaddr *)&addr, &addrlen);
	if (fd < 0) {
		if (errno != EAGAIN && errno != ECONNABORTED) {
			syslog(LOG_CRIT, "got_connection: error %d", errno);
			abort();
		}
		return;
	}

#if 0
	printf("got connection from %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
#endif

	yes = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes)) < 0) {
		syslog(LOG_CRIT, "got_connection: error %d in "
				 "setsockopt(SO_KEEPALIVE)", errno);
		abort();
	}

	connection_new(si, fd, &addr);
}

int register_skaidrus_instance(struct skaidrus_instance *si)
{
	int fd;
	int yes;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		syslog(LOG_CRIT, "make_listening_connection: socket() error %d",
				errno);
		return 0;
	}

	yes = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		syslog(LOG_CRIT, "make_listening_socket: setsockopt() error "
				"%d", errno);
		return 0;
	}

	if (bind(fd, (struct sockaddr *)&(si->local_addr), sizeof(si->local_addr)) < 0) {
		syslog(LOG_CRIT, "make_listening_socket: bind() error %d",
				errno);
		return 0;
	}

	if (listen(fd, 100) < 0) {
		syslog(LOG_CRIT, "make_listening_socket: listen() error %d",
				errno);
		return 0;
	}

	INIT_IV_FD(&(si->listen_fd));
	si->listen_fd.fd = fd;
	si->listen_fd.cookie = (void *)si;
	si->listen_fd.handler_in = got_connection;
	iv_register_fd(&(si->listen_fd));

	INIT_LIST_HEAD(&(si->connections));

	return 1;
}

void unregister_skaidrus_instance(struct skaidrus_instance *si)
{
	struct list_head *lh;
	struct list_head *lh2;

	iv_unregister_fd(&(si->listen_fd));
	close(si->listen_fd.fd);

	list_for_each_safe (lh, lh2, &(si->connections)) {
		struct connection *req;
		req = list_entry(lh, struct connection, list);
		connection_kill(req);
	}
}
