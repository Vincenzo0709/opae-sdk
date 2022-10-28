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

#include <opae/types.h>

#include "mock/opae_std.h"

#include "request.h"
#include "response.h"
#include "remote.h"

struct _remote_handle *
opae_create_remote_handle(struct _remote_token *token,
			  fpga_handle_header *hdr)
{
	struct _remote_handle *h =
		(struct _remote_handle *)opae_calloc(1, sizeof(*h));
	if (h) {
		h->hdr = *hdr;
		h->token = token;
	}
	return h;
}

void opae_destroy_remote_handle(struct _remote_handle *h)
{
	opae_free(h);
}

fpga_result __REMOTE_API__
remote_fpgaOpen(fpga_token token, fpga_handle *handle, int flags)
{
	opae_fpgaOpen_request req;
	opae_fpgaOpen_response resp;
	struct _remote_token *tok;
	struct _remote_handle *h;
	char *req_json;
	size_t len;
	ssize_t slen;
	char recvbuf[OPAE_RECEIVE_BUF_MAX];

	if (!token) {
		OPAE_ERR("NULL token");
		return FPGA_INVALID_PARAM;
	}

	if (!handle) {
		OPAE_ERR("NULL handle pointer");
		return FPGA_INVALID_PARAM;
	}

	tok = (struct _remote_token *)token;
	req.token = tok->hdr;
	req.flags = flags;

	req_json = opae_encode_fpgaOpen_request_5(
		&req, tok->json_to_string_flags);

	if (!req_json)
		return FPGA_NO_MEMORY;

	len = strlen(req_json);

	slen = tok->ifc->send(tok->ifc->connection,
			      req_json,
			      len + 1);
	if (slen < 0) {
		opae_free(req_json);
		return FPGA_EXCEPTION;
	}

	opae_free(req_json);

	slen = tok->ifc->receive(tok->ifc->connection,
				 recvbuf,
				 sizeof(recvbuf));
	if (slen < 0)
		return FPGA_EXCEPTION;

printf("%s\n", recvbuf);

	if (!opae_decode_fpgaOpen_response_5(recvbuf, &resp))
		return FPGA_EXCEPTION;

	h = opae_create_remote_handle(tok, &resp.handle);
	if (!h) {
		OPAE_ERR("calloc failed");
		return FPGA_NO_MEMORY;
	}

	*handle = h;

	return resp.result;
}
