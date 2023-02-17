// Copyright(c) 2023, Intel Corporation
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
#endif  // HAVE_CONFIG_H

#include <algorithm>
#include <cstring>
#include <iostream>

#include "convert.hpp"
#include "grpc_server.hpp"

Status OPAEServiceImpl::fpgaEnumerate(ServerContext *context,
                                      const EnumerateRequest *request,
                                      EnumerateReply *reply) {
  std::cout << "fpgaEnumerate request " << *request << std::endl;

  UNUSED_PARAM(context);
  std::vector<fpga_properties> req_filters;
  fpga_properties *pfilters = nullptr;
  uint32_t req_num_filters;
  uint32_t req_max_tokens;

  req_filters.reserve(request->filters().size());
  for (const auto &f : request->filters()) {
    req_filters.push_back(to_opae_fpga_properties(this, f));
  }
  if (req_filters.size() > 0) pfilters = req_filters.data();

  req_num_filters = request->num_filters();
  req_max_tokens = request->max_tokens();

  fpga_token *tokens = nullptr;

  if (req_max_tokens) {
    tokens = new fpga_token[req_max_tokens];
    memset(tokens, 0, req_max_tokens * sizeof(fpga_token));
  }

  uint32_t resp_max_tokens = 0;
  uint32_t resp_num_matches = 0;
  fpga_result result;

  result = ::fpgaEnumerate(pfilters, req_num_filters, tokens, req_max_tokens,
                           &resp_num_matches);
  resp_max_tokens = req_max_tokens;

  if (tokens) {
    uint32_t i;
    const uint32_t num_tokens = std::min(resp_num_matches, req_max_tokens);

    // Walk through the tokens, opening each and grabbing its header.
    for (i = 0; i < num_tokens; ++i) {
      opae_wrapped_token *wt =
          reinterpret_cast<opae_wrapped_token *>(tokens[i]);
      fpga_token_header *hdr =
          reinterpret_cast<fpga_token_header *>(wt->opae_token);

      // If we're not already tracking this token_id, then add it.
      if (!token_map_.find(hdr->token_id))
        token_map_.add(hdr->token_id, tokens[i]);

      // Add the token header to our outbound reply.
      to_grpc_token_header(*hdr, reply->add_tokens());
    }
  }

  reply->set_max_tokens(resp_max_tokens);
  reply->set_num_matches(resp_num_matches);
  reply->set_result(to_grpc_fpga_result[result]);

  if (tokens) delete[] tokens;

  return Status::OK;
}

Status OPAEServiceImpl::fpgaDestroyToken(ServerContext *context,
                                         const DestroyTokenRequest *request,
                                         DestroyTokenReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id req_token_id;
  fpga_token token;
  fpga_result res;

  std::cout << "fpgaDestroyToken request " << *request << std::endl;

  req_token_id = to_opae_fpga_remote_id(request->token_id());
  token = token_map_.find(req_token_id);

  if (!token) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  token_map_.remove(req_token_id);

  res = ::fpgaDestroyToken(&token);
  reply->set_result(to_grpc_fpga_result[res]);

  return Status::OK;
}

Status OPAEServiceImpl::fpgaCloneToken(ServerContext *context,
                                       const CloneTokenRequest *request,
                                       CloneTokenReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id req_src_token_id;
  fpga_token src_token;
  fpga_token dest_token = NULL;
  fpga_token_header resp_token_hdr;
  fpga_result res;

  std::cout << "fpgaCloneToken request " << *request << std::endl;

  req_src_token_id = to_opae_fpga_remote_id(request->src_token_id());
  src_token = token_map_.find(req_src_token_id);

  if (!src_token) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  res = ::fpgaCloneToken(src_token, &dest_token);
  if (res == FPGA_OK) {
    opae_wrapped_token *wt = reinterpret_cast<opae_wrapped_token *>(dest_token);
    fpga_token_header *hdr =
        reinterpret_cast<fpga_token_header *>(wt->opae_token);

    resp_token_hdr = *hdr;
    token_map_.add(resp_token_hdr.token_id, dest_token);

    opaegrpc::fpga_token_header *ghdr = new opaegrpc::fpga_token_header();
    to_grpc_token_header(resp_token_hdr, ghdr);

    reply->set_allocated_dest_token(ghdr);
  }

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaGetProperties(ServerContext *context,
                                          const GetPropertiesRequest *request,
                                          GetPropertiesReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id token_id;
  fpga_token token;
  _fpga_properties *_props = nullptr;
  fpga_result res;

  std::cout << "fpgaGetProperties request " << *request << std::endl;

  token_id = to_opae_fpga_remote_id(request->token_id());
  token = token_map_.find(token_id);

  if (!token) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  res =
      ::fpgaGetProperties(token, reinterpret_cast<fpga_properties *>(&_props));
  if (!_props) {
    reply->set_result(to_grpc_fpga_result[FPGA_NO_MEMORY]);
    return Status::OK;
  }

  opaegrpc::fpga_properties *gprops = new opaegrpc::fpga_properties();
  to_grpc_fpga_properties(this, gprops, _props);

  reply->set_allocated_properties(gprops);

  ::fpgaDestroyProperties(reinterpret_cast<fpga_properties *>(&_props));

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaUpdateProperties(
    ServerContext *context, const UpdatePropertiesRequest *request,
    UpdatePropertiesReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id token_id;
  fpga_token token;
  fpga_properties resp_props = nullptr;
  fpga_result res;

  std::cout << "fpgaUpdateProperties request " << *request << std::endl;

  token_id = to_opae_fpga_remote_id(request->token_id());
  token = token_map_.find(token_id);
  if (!token) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  res = ::fpgaGetProperties(nullptr, &resp_props);
  if (res) {
    reply->set_result(to_grpc_fpga_result[res]);
    return Status::OK;
  }

  res = ::fpgaUpdateProperties(token, resp_props);
  if (res) {
    ::fpgaDestroyProperties(&resp_props);
    reply->set_result(to_grpc_fpga_result[res]);
    return Status::OK;
  }

  opaegrpc::fpga_properties *gprops = new opaegrpc::fpga_properties();
  to_grpc_fpga_properties(
      this, gprops, reinterpret_cast<const _fpga_properties *>(resp_props));
  reply->set_allocated_properties(gprops);

  ::fpgaDestroyProperties(&resp_props);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaOpen(ServerContext *context,
                                 const OpenRequest *request, OpenReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id token_id;
  fpga_token token;
  fpga_handle handle = nullptr;
  int flags;
  fpga_result res;

  std::cout << "fpgaOpen request " << *request << std::endl;

  token_id = to_opae_fpga_remote_id(request->token_id());
  token = token_map_.find(token_id);

  if (!token) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  flags = request->flags();

  res = ::fpgaOpen(token, &handle, flags);
  if (res) {
    reply->set_result(to_grpc_fpga_result[res]);
    return Status::OK;
  }

  opae_wrapped_handle *wh = reinterpret_cast<opae_wrapped_handle *>(handle);
  fpga_handle_header *hdr =
      reinterpret_cast<fpga_handle_header *>(wh->opae_handle);

  if (!handle_map_.find(hdr->handle_id))
    handle_map_.add(hdr->handle_id, handle);

  reply->set_allocated_handle(to_grpc_handle_header(*hdr));

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaClose(ServerContext *context,
                                  const CloseRequest *request,
                                  CloseReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  fpga_result res;

  std::cout << "fpgaClose request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  res = ::fpgaClose(handle);
  if (res == FPGA_OK) handle_map_.remove(handle_id);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaReset(ServerContext *context,
                                  const ResetRequest *request,
                                  ResetReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  fpga_result res;

  std::cout << "fpgaReset request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  res = ::fpgaReset(handle);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaGetPropertiesFromHandle(
    ServerContext *context, const GetPropertiesFromHandleRequest *request,
    GetPropertiesFromHandleReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  fpga_properties resp_props = nullptr;
  fpga_result res;

  std::cout << "fpgaGetPropertiesFromHandle request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  res = ::fpgaGetProperties(nullptr, &resp_props);
  if (res) {
    reply->set_result(to_grpc_fpga_result[res]);
    return Status::OK;
  }

  res = ::fpgaGetPropertiesFromHandle(handle, &resp_props);
  if (res) {
    ::fpgaDestroyProperties(&resp_props);
    reply->set_result(to_grpc_fpga_result[res]);
    return Status::OK;
  }

  opaegrpc::fpga_properties *gprops = new opaegrpc::fpga_properties();
  to_grpc_fpga_properties(
      this, gprops, reinterpret_cast<const _fpga_properties *>(resp_props));
  reply->set_allocated_properties(gprops);

  ::fpgaDestroyProperties(&resp_props);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaMapMMIO(ServerContext *context,
                                    const MapMMIORequest *request,
                                    MapMMIOReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  uint32_t mmio_num;
  uint64_t *mmio_ptr = nullptr;
  fpga_remote_id mmio_id;
  fpga_result res;

  std::cout << "fpgaMapMMIO request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  mmio_num = request->mmio_num();
  res = ::fpgaMapMMIO(handle, mmio_num, &mmio_ptr);

  if (res) {
    reply->set_result(to_grpc_fpga_result[res]);
    return Status::OK;
  }

  opae_get_remote_id(&mmio_id);
  mmio_map_.add(mmio_id, mmio_ptr);

  reply->set_allocated_mmio_id(to_grpc_fpga_remote_id(mmio_id));
  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaUnmapMMIO(ServerContext *context,
                                      const UnmapMMIORequest *request,
                                      UnmapMMIOReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_remote_id mmio_id;
  fpga_handle handle;
  uint32_t mmio_num;
  fpga_result res;

  std::cout << "fpgaUnmapMMIO request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  mmio_num = request->mmio_num();

  res = ::fpgaUnmapMMIO(handle, mmio_num);
  if (res) {
    reply->set_result(to_grpc_fpga_result[res]);
    return Status::OK;
  }

  mmio_id = to_opae_fpga_remote_id(request->mmio_id());
  mmio_map_.remove(mmio_id);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaReadMMIO32(ServerContext *context,
                                       const ReadMMIO32Request *request,
                                       ReadMMIO32Reply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  uint32_t mmio_num;
  uint64_t offset;
  uint32_t value = 0;
  fpga_result res;

  std::cout << "fpgaReadMMIO32 request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  mmio_num = request->mmio_num();
  offset = request->offset();

  res = ::fpgaReadMMIO32(handle, mmio_num, offset, &value);

  reply->set_value(value);
  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaWriteMMIO32(ServerContext *context,
                                        const WriteMMIO32Request *request,
                                        WriteMMIO32Reply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  uint32_t mmio_num;
  uint64_t offset;
  uint32_t value;
  fpga_result res;

  std::cout << "fpgaWriteMMIO32 request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  mmio_num = request->mmio_num();
  offset = request->offset();
  value = request->value();

  res = ::fpgaWriteMMIO32(handle, mmio_num, offset, value);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaReadMMIO64(ServerContext *context,
                                       const ReadMMIO64Request *request,
                                       ReadMMIO64Reply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  uint32_t mmio_num;
  uint64_t offset;
  uint64_t value = 0;
  fpga_result res;

  std::cout << "fpgaReadMMIO64 request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  mmio_num = request->mmio_num();
  offset = request->offset();

  res = ::fpgaReadMMIO64(handle, mmio_num, offset, &value);

  reply->set_value(value);
  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaWriteMMIO64(ServerContext *context,
                                        const WriteMMIO64Request *request,
                                        WriteMMIO64Reply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  uint32_t mmio_num;
  uint64_t offset;
  uint64_t value;
  fpga_result res;

  std::cout << "fpgaWriteMMIO64 request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  mmio_num = request->mmio_num();
  offset = request->offset();
  value = request->value();

  res = ::fpgaWriteMMIO64(handle, mmio_num, offset, value);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

constexpr size_t bits_to_bytes(size_t bits) { return bits / 8; }

Status OPAEServiceImpl::fpgaWriteMMIO512(ServerContext *context,
                                         const WriteMMIO512Request *request,
                                         WriteMMIO512Reply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  uint32_t mmio_num;
  uint64_t offset;
  fpga_result res;

  std::cout << "fpgaWriteMMIO512 request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  mmio_num = request->mmio_num();
  offset = request->offset();
  const std::string &values = request->values();

  if (values.length() < bits_to_bytes(512)) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  res = ::fpgaWriteMMIO512(handle, mmio_num, offset, values.c_str());

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaPrepareBuffer(ServerContext *context,
                                          const PrepareBufferRequest *request,
                                          PrepareBufferReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  uint64_t length;
  bool have_buf_addr;
  void **buf_addr = nullptr;
  void *addr = nullptr;
  int flags;
  uint64_t wsid = 0;
  fpga_remote_id buf_id;
  fpga_result res;

  std::cout << "fpgaPrepareBuffer request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  length = request->length();
  have_buf_addr = request->have_buf_addr();

  if (have_buf_addr) {
    buf_addr = &addr;
    addr = (void *)request->pre_allocated_addr();
  }

  flags = request->flags();

  res = ::fpgaPrepareBuffer(handle, length, buf_addr, &wsid, flags);

  if ((res == FPGA_OK) && buf_addr) {
    // Allocate a new remote ID for the buffer.
    opae_get_remote_id(&buf_id);

    OPAEBufferInfo *binfo =
        new OPAEBufferInfo(handle_id, length, *buf_addr, wsid, flags);
    binfo_map_.add(buf_id, binfo);

    reply->set_allocated_buf_id(to_grpc_fpga_remote_id(buf_id));
  }

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaReleaseBuffer(ServerContext *context,
                                          const ReleaseBufferRequest *request,
                                          ReleaseBufferReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  fpga_remote_id buf_id;
  OPAEBufferInfo *binfo;
  fpga_result res;

  std::cout << "fpgaReleaseBuffer request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  buf_id = to_opae_fpga_remote_id(request->buf_id());
  binfo = binfo_map_.find(buf_id);

  if (!binfo) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  if (!opae_remote_ids_match(&handle_id, &binfo->handle_id_)) {
    reply->set_result(to_grpc_fpga_result[FPGA_NOT_FOUND]);
    return Status::OK;
  }

  res = ::fpgaReleaseBuffer(handle, binfo->wsid_);
  if (res == FPGA_OK) binfo_map_.remove(buf_id);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaGetIOAddress(ServerContext *context,
                                         const GetIOAddressRequest *request,
                                         GetIOAddressReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  fpga_remote_id buf_id;
  OPAEBufferInfo *binfo;
  uint64_t ioaddr = 0;
  fpga_result res;

  std::cout << "fpgaGetIOAddress request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  buf_id = to_opae_fpga_remote_id(request->buf_id());
  binfo = binfo_map_.find(buf_id);

  if (!binfo) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  if (!opae_remote_ids_match(&handle_id, &binfo->handle_id_)) {
    reply->set_result(to_grpc_fpga_result[FPGA_NOT_FOUND]);
    return Status::OK;
  }

  res = ::fpgaGetIOAddress(handle, binfo->wsid_, &ioaddr);

  reply->set_ioaddr(ioaddr);
  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaReadError(ServerContext *context,
                                      const ReadErrorRequest *request,
                                      ReadErrorReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id token_id;
  fpga_token token;
  uint32_t error_num;
  uint64_t value = 0;
  fpga_result res;

  std::cout << "fpgaReadError request " << *request << std::endl;

  token_id = to_opae_fpga_remote_id(request->token_id());
  token = token_map_.find(token_id);

  if (!token) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  error_num = request->error_num();

  res = ::fpgaReadError(token, error_num, &value);

  reply->set_value(value);
  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaGetErrorInfo(ServerContext *context,
                                         const GetErrorInfoRequest *request,
                                         GetErrorInfoReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id token_id;
  fpga_token token;
  uint32_t error_num;
  fpga_error_info error_info;
  fpga_result res;

  std::cout << "fpgaGetErrorInfo request " << *request << std::endl;

  token_id = to_opae_fpga_remote_id(request->token_id());
  token = token_map_.find(token_id);

  if (!token) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  error_num = request->error_num();

  res = ::fpgaGetErrorInfo(token, error_num, &error_info);

  reply->set_allocated_error_info(to_grpc_fpga_error_info(error_info));
  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaClearError(ServerContext *context,
                                       const ClearErrorRequest *request,
                                       ClearErrorReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id token_id;
  fpga_token token;
  uint32_t error_num;
  fpga_result res;

  std::cout << "fpgaClearError request " << *request << std::endl;

  token_id = to_opae_fpga_remote_id(request->token_id());
  token = token_map_.find(token_id);

  if (!token) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  error_num = request->error_num();

  res = ::fpgaClearError(token, error_num);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaClearAllErrors(ServerContext *context,
                                           const ClearAllErrorsRequest *request,
                                           ClearAllErrorsReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id token_id;
  fpga_token token;
  fpga_result res;

  std::cout << "fpgaClearAllErrors request " << *request << std::endl;

  token_id = to_opae_fpga_remote_id(request->token_id());
  token = token_map_.find(token_id);

  if (!token) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  res = ::fpgaClearAllErrors(token);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaTokenGetObject(ServerContext *context,
                                           const TokenGetObjectRequest *request,
                                           TokenGetObjectReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id token_id;
  fpga_token token;
  int flags;
  fpga_object obj = nullptr;
  fpga_result res;

  std::cout << "fpgaTokenGetObject request " << *request << std::endl;

  token_id = to_opae_fpga_remote_id(request->token_id());
  token = token_map_.find(token_id);

  if (!token) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  flags = request->flags();
  const std::string &name = request->name();

  res = ::fpgaTokenGetObject(token, name.c_str(), &obj, flags);
  if (res == FPGA_OK) {
    fpga_remote_id object_id;

    // Allocate a new remote ID for the object.
    opae_get_remote_id(&object_id);

    sysobj_map_.add(object_id, obj);

    reply->set_allocated_object_id(to_grpc_fpga_remote_id(object_id));
  }

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaDestroyObject(ServerContext *context,
                                          const DestroyObjectRequest *request,
                                          DestroyObjectReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id object_id;
  fpga_object sysobj;
  fpga_result res;

  std::cout << "fpgaDestroyObject request " << *request << std::endl;

  object_id = to_opae_fpga_remote_id(request->object_id());
  sysobj = sysobj_map_.find(object_id);

  if (!sysobj) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  res = ::fpgaDestroyObject(&sysobj);
  if (res == FPGA_OK) sysobj_map_.remove(object_id);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaObjectGetType(ServerContext *context,
                                          const ObjectGetTypeRequest *request,
                                          ObjectGetTypeReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id object_id;
  fpga_object sysobj;
  fpga_sysobject_type type = FPGA_OBJECT_ATTRIBUTE;
  fpga_result res;

  std::cout << "fpgaObjectGetType request " << *request << std::endl;

  object_id = to_opae_fpga_remote_id(request->object_id());
  sysobj = sysobj_map_.find(object_id);

  if (!sysobj) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  res = ::fpgaObjectGetType(sysobj, &type);
  if (res == FPGA_OK) reply->set_type(to_grpc_fpga_sysobject_type[type]);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaObjectGetName(ServerContext *context,
                                          const ObjectGetNameRequest *request,
                                          ObjectGetNameReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id object_id;
  fpga_object sysobj;
  fpga_result res;

  std::cout << "fpgaObjectGetName request " << *request << std::endl;

  object_id = to_opae_fpga_remote_id(request->object_id());
  sysobj = sysobj_map_.find(object_id);

  if (!sysobj) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  char buf[PATH_MAX] = {
      0,
  };
  res = ::fpgaObjectGetName(sysobj, buf, sizeof(buf));

  if (res == FPGA_OK) reply->set_allocated_name(new std::string(buf));

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaObjectGetSize(ServerContext *context,
                                          const ObjectGetSizeRequest *request,
                                          ObjectGetSizeReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id object_id;
  fpga_object sysobj;
  int flags;
  uint32_t value = 0;
  fpga_result res;

  std::cout << "fpgaObjectGetSize request " << *request << std::endl;

  object_id = to_opae_fpga_remote_id(request->object_id());
  sysobj = sysobj_map_.find(object_id);

  if (!sysobj) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  flags = request->flags();

  res = ::fpgaObjectGetSize(sysobj, &value, flags);

  if (res == FPGA_OK) reply->set_value(value);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaObjectRead(ServerContext *context,
                                       const ObjectReadRequest *request,
                                       ObjectReadReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id object_id;
  fpga_object sysobj;
  uint64_t offset;
  uint64_t length;
  int flags;
  fpga_result res;

  std::cout << "fpgaObjectRead request " << *request << std::endl;

  object_id = to_opae_fpga_remote_id(request->object_id());
  sysobj = sysobj_map_.find(object_id);

  if (!sysobj) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  offset = request->offset();
  length = request->length();
  flags = request->flags();

  char buf[4096] = {
      0,
  };
  res = ::fpgaObjectRead(sysobj, (uint8_t *)buf, offset, length, flags);

  if (res == FPGA_OK) {
    reply->set_allocated_value(new std::string(buf, length));
  }

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaObjectRead64(ServerContext *context,
                                         const ObjectRead64Request *request,
                                         ObjectRead64Reply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id object_id;
  fpga_object sysobj;
  int flags;
  uint64_t value = 0;
  fpga_result res;

  std::cout << "fpgaObjectRead64 request " << *request << std::endl;

  object_id = to_opae_fpga_remote_id(request->object_id());
  sysobj = sysobj_map_.find(object_id);

  if (!sysobj) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  flags = request->flags();

  res = ::fpgaObjectRead64(sysobj, &value, flags);

  if (res == FPGA_OK) reply->set_value(value);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaObjectWrite64(ServerContext *context,
                                          const ObjectWrite64Request *request,
                                          ObjectWrite64Reply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id object_id;
  fpga_object sysobj;
  uint64_t value;
  int flags;
  fpga_result res;

  std::cout << "fpgaObjectWrite64 request " << *request << std::endl;

  object_id = to_opae_fpga_remote_id(request->object_id());
  sysobj = sysobj_map_.find(object_id);

  if (!sysobj) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  value = request->value();
  flags = request->flags();

  res = ::fpgaObjectWrite64(sysobj, value, flags);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaHandleGetObject(
    ServerContext *context, const HandleGetObjectRequest *request,
    HandleGetObjectReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  int flags;
  fpga_object sysobj = nullptr;
  fpga_result res;

  std::cout << "fpgaHandleGetObject request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  const std::string &name = request->name();
  flags = request->flags();

  res = ::fpgaHandleGetObject(handle, name.c_str(), &sysobj, flags);
  if (res == FPGA_OK) {
    fpga_remote_id object_id;

    // Allocate a new remote ID for the object.
    opae_get_remote_id(&object_id);

    sysobj_map_.add(object_id, sysobj);

    reply->set_allocated_object_id(to_grpc_fpga_remote_id(object_id));
  }

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaObjectGetObject(
    ServerContext *context, const ObjectGetObjectRequest *request,
    ObjectGetObjectReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id parent_id;
  fpga_object parent;
  fpga_object child = nullptr;
  int flags;
  fpga_result res;

  std::cout << "fpgaObjectGetObject request " << *request << std::endl;

  parent_id = to_opae_fpga_remote_id(request->object_id());
  parent = sysobj_map_.find(parent_id);

  if (!parent) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  const std::string &name = request->name();
  flags = request->flags();

  res = ::fpgaObjectGetObject(parent, name.c_str(), &child, flags);
  if (res == FPGA_OK) {
    fpga_remote_id child_id;

    // Allocate a new remote ID for the child object.
    opae_get_remote_id(&child_id);

    sysobj_map_.add(child_id, child);

    reply->set_allocated_object_id(to_grpc_fpga_remote_id(child_id));
  }

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaObjectGetObjectAt(
    ServerContext *context, const ObjectGetObjectAtRequest *request,
    ObjectGetObjectAtReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id parent_id;
  fpga_object parent;
  uint64_t index;
  fpga_object child = nullptr;
  fpga_result res;

  std::cout << "fpgaObjectGetObjectAt request " << *request << std::endl;

  parent_id = to_opae_fpga_remote_id(request->object_id());
  parent = sysobj_map_.find(parent_id);

  if (!parent) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  index = request->index();

  res = ::fpgaObjectGetObjectAt(parent, index, &child);

  if (res == FPGA_OK) {
    fpga_remote_id child_id;

    // Allocate a new remote ID for the child object.
    opae_get_remote_id(&child_id);

    sysobj_map_.add(child_id, child);

    reply->set_allocated_object_id(to_grpc_fpga_remote_id(child_id));
  }

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaSetUserClock(ServerContext *context,
                                         const SetUserClockRequest *request,
                                         SetUserClockReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  uint64_t high_clk;
  uint64_t low_clk;
  int flags;
  fpga_result res;

  std::cout << "fpgaSetUserClock request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  high_clk = request->high_clk();
  low_clk = request->low_clk();
  flags = request->flags();

  res = ::fpgaSetUserClock(handle, high_clk, low_clk, flags);

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}

Status OPAEServiceImpl::fpgaGetUserClock(ServerContext *context,
                                         const GetUserClockRequest *request,
                                         GetUserClockReply *reply) {
  UNUSED_PARAM(context);
  fpga_remote_id handle_id;
  fpga_handle handle;
  int flags;
  uint64_t high_clk = 0;
  uint64_t low_clk = 0;
  fpga_result res;

  std::cout << "fpgaGetUserClock request " << *request << std::endl;

  handle_id = to_opae_fpga_remote_id(request->handle_id());
  handle = handle_map_.find(handle_id);

  if (!handle) {
    reply->set_result(to_grpc_fpga_result[FPGA_INVALID_PARAM]);
    return Status::OK;
  }

  flags = request->flags();

  res = ::fpgaGetUserClock(handle, &high_clk, &low_clk, flags);

  if (res == FPGA_OK) {
    reply->set_high_clk(high_clk);
    reply->set_low_clk(low_clk);
  }

  reply->set_result(to_grpc_fpga_result[res]);
  return Status::OK;
}
