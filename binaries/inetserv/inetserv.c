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

#include "inetserv.h"

int inet_server_init(inet_server_context *c, inet_server_config *cfg)
{
	int server_socket;
	struct sockaddr_in sin;
	int res;

	c->cfg = cfg;

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		OPAE_ERR("socket() failed");
		return 1;
	}

	sin.sin_family = AF_INET;

	res = inet_pton(AF_INET, cfg->ip_addr, &sin.sin_addr);
	if (res != 1) {
		OPAE_ERR("inet_pton() failed with %d", res);
		return 2;
	}

	sin.sin_port = htons(cfg->port);

	res = bind(server_socket, (struct sockaddr *)&sin, sizeof(sin));
	if (res < 0) {
		OPAE_ERR("bind() failed with %d", res);
		close(server_socket);
		return 3;
	}

	res = listen(server_socket, LISTEN_BACKLOG);
	if (res < 0) {
		OPAE_ERR("listen() failed with %d", res);
		close(server_socket);
		return 4;
	}

	c->pollfds[SRV_SOCKET].fd = server_socket;
	c->pollfds[SRV_SOCKET].events = POLLIN | POLLPRI;
	c->num_fds = 1;

	c->poll_timeout = POLL_TIMEOUT;

	return 0;
}

int inet_server_poll_loop(inet_server_context *c)
{
	int res;
	int i;

	while (c->cfg->running) {

		res = poll(c->pollfds, c->num_fds, c->poll_timeout);
		if (res < 0) {
			OPAE_ERR("poll() error: %s", strerror(errno));
			continue;
		}

		if (!res) { // timeout
			if (c->on_poll_timeout)
				c->on_poll_timeout(c);
			continue;
		}

		if ((nfds_t)res > c->num_fds) { // strange
			OPAE_ERR("something went wrong during poll()");
			continue;
		}

		// Handle requests arriving from existing clients.
		for (i = FIRST_CLIENT_SOCKET ; i < (int)c->num_fds ; ++i) {
			if (c->pollfds[i].revents) {
				c->handle_client_message(c,
							 c->remote_context[i],
							 c->pollfds[i].fd);
			}
		}

		// Handle new client connection requests.
		if (c->pollfds[SRV_SOCKET].revents) {
			int client_sock;

			if (c->num_fds > MAX_CLIENT_CONNECTIONS) {
				OPAE_ERR("server exceeded max connections. Rejecting..");
				continue;
			}

			client_sock = accept(c->pollfds[SRV_SOCKET].fd, NULL, NULL);

			if (client_sock < 0) {
				OPAE_ERR("accept() failed");
			} else {
				OPAE_MSG("accepting new connection");

				c->pollfds[c->num_fds].fd = client_sock;
				c->pollfds[c->num_fds].events = POLLIN | POLLPRI;

				if (c->init_remote_context)
					c->init_remote_context(c, c->num_fds);

				++c->num_fds;
			}
		}

	}

	return 0;
}

void inet_server_close_client(inet_server_context *c, int client)
{
	nfds_t i;
	nfds_t j;
	nfds_t removed = 0;

	for (i = j = FIRST_CLIENT_SOCKET ; i < c->num_fds ; ++i) {
		if (client != c->pollfds[i].fd) {
			if (j != i)
				c->pollfds[j] = c->pollfds[i];
			++j;
		} else {
			if (c->release_remote_context)
				c->release_remote_context(c, i);
			++removed;
		}

	}

	close(client);
	c->num_fds -= removed;
}

void inet_server_release(inet_server_context *c)
{
	int i;

	for (i = FIRST_CLIENT_SOCKET ; i < (int)c->num_fds ; ++i) {
		close(c->pollfds[i].fd);
	}

	close(c->pollfds[SRV_SOCKET].fd);
}