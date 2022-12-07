// Copyright(c) 2022, Intel Corporation
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>

#include <opae/log.h>

#include "mock/opae_std.h"

#include "inet-ifc.h"

int opae_inet_client_open(void *con)
{
	opae_inet_client_connection *c =
		(opae_inet_client_connection *)con;
	struct addrinfo *aptr;

	if (c->client_socket >= 0)
		return 0; // already open

	for (aptr = c->addrs ; aptr ; aptr = aptr->ai_next) {

		c->client_socket = socket(aptr->ai_family,
					  aptr->ai_socktype,
					  aptr->ai_protocol);
		if (c->client_socket < 0)
			continue;

		if (connect(c->client_socket,
			    aptr->ai_addr,
			    aptr->ai_addrlen) != -1)
			break;

		close(c->client_socket);
		c->client_socket = -1;
	}

	if (!aptr) {
		OPAE_ERR("failed to connect to %s:%d",
			 c->hostname, c->port);
		return 1;
	}

	return 0;
}

int opae_inet_client_close(void *con)
{
	opae_inet_client_connection *c =
		(opae_inet_client_connection *)con;

	if (c->client_socket >= 0) {
		close(c->client_socket);
		c->client_socket = -1;
	}

	return 0;
}

int opae_inet_client_release(void *con)
{
	opae_inet_client_connection *c =
		(opae_inet_client_connection *)con;

	if (c->addrs)
		freeaddrinfo(c->addrs);

	opae_free(con);
	return 0;
}

ssize_t opae_inet_client_send(void *con, const void *buf, size_t len)
{
	opae_inet_client_connection *c =
		(opae_inet_client_connection *)con;

	if (c->client_socket < 0) {
		OPAE_ERR("invalid client_socket");
		return -1;
	}

	return chunked_send(c->client_socket, buf, len, c->send_flags);
}

ssize_t opae_inet_client_receive(void *con, void *buf, size_t len)
{
	opae_inet_client_connection *c =
		(opae_inet_client_connection *)con;
	ssize_t total_bytes;

	if (c->client_socket < 0) {
		OPAE_ERR("invalid client_socket");
		return -1;
	}

	total_bytes = chunked_recv(c->client_socket,
				   buf,
				   len,
				   c->receive_flags);

	if (total_bytes == -2) {
		// Orderly shutdown by peer.
		OPAE_ERR("%s: peer closed connection",
			 c->hostname);
		opae_inet_client_close(con);
	}

	return total_bytes;
}

int opae_inet_ifc_init(opae_remote_client_ifc *i,
		       const char *ip_addr_or_host,
		       int port,
		       int send_flags,
		       int receive_flags)
{
	opae_inet_client_connection *con =
		(opae_inet_client_connection *)calloc(1, sizeof(*con));
	struct addrinfo hints;
	char port_buf[32];
	size_t len;
	int res;

	if (!con) {
		OPAE_ERR("calloc failed");
		return 1;
	}

	memset(con->hostname, 0, sizeof(con->hostname));
	len = strnlen(ip_addr_or_host, HOST_NAME_MAX);
	memcpy(con->hostname, ip_addr_or_host, len + 1);

	con->port = (in_port_t)port;

	if (snprintf(port_buf, sizeof(port_buf),
		     "%" PRIu16, (in_port_t)port) >=
		(int)sizeof(port_buf)) {
		OPAE_ERR("snprintf() buffer overflow");
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	con->addrs = NULL;
	res = getaddrinfo(ip_addr_or_host,
			  port_buf,
			  &hints,
			  &con->addrs);
	if (res) {
		OPAE_ERR("getaddrinfo(\"%s\", \"%s\"): %s",
			 ip_addr_or_host, port_buf, gai_strerror(res));
		return 2;
	}

	con->client_socket = -1;
	con->send_flags = send_flags;
	con->receive_flags = receive_flags;

	i->open = opae_inet_client_open;
	i->close = opae_inet_client_close;
	i->release = opae_inet_client_release;
	i->send = opae_inet_client_send;
	i->receive = opae_inet_client_receive;
	i->connection = con;

	return 0;
}