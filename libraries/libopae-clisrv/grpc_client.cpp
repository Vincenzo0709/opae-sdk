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

#include "opae.pb.h"
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif  // HAVE_CONFIG_H

#include <algorithm>
#include <iostream>
#include <string>

#include "convert.hpp"
#include "grpc_client.hpp"

fpga_result OPAEClient::fpgaEnumerate(
    const std::vector<fpga_properties> &filters, uint32_t num_filters,
    uint32_t max_tokens, uint32_t &num_matches,
    std::vector<fpga_token_header> &tokens) {
  opaegrpc::EnumerateRequest request;

  for (auto f : filters) {
    _fpga_properties *p = reinterpret_cast<_fpga_properties *>(f);
    opaegrpc::fpga_properties *gprops = request.add_filters();
    to_grpc_fpga_properties(this, gprops, p);
  }
  request.set_num_filters(num_filters);
  request.set_max_tokens(max_tokens);

  opaegrpc::EnumerateReply reply;
  ClientContext context;

  Status status = stub_->fpgaEnumerate(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaEnumerate() gRPC failed: %s", status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  std::cout << "fpgaEnumerate reply " << reply << std::endl;

  num_matches = reply.num_matches();
  const size_t toks = std::min(num_matches, reply.max_tokens());

  tokens.reserve(toks);
  tokens.resize(toks);
  std::transform(reply.tokens().cbegin(), reply.tokens().cbegin() + toks,
                 tokens.begin(), to_opae_token_header);

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaDestroyToken(const fpga_remote_id &token_id) {
  opaegrpc::DestroyTokenRequest request;
  request.set_allocated_token_id(to_grpc_fpga_remote_id(token_id));

  opaegrpc::DestroyTokenReply reply;
  ClientContext context;

  Status status = stub_->fpgaDestroyToken(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaDestroyToken() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  std::cout << "fpgaDestroyToken reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaCloneToken(const fpga_remote_id &src_token_id,
                                       fpga_token_header &dest_token_hdr) {
  opaegrpc::CloneTokenRequest request;
  request.set_allocated_src_token_id(to_grpc_fpga_remote_id(src_token_id));

  opaegrpc::CloneTokenReply reply;
  ClientContext context;

  Status status = stub_->fpgaCloneToken(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaCloneToken() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  std::cout << "fpgaCloneToken reply " << reply << std::endl;

  dest_token_hdr = to_opae_token_header(reply.dest_token());

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaGetProperties(const fpga_remote_id &token_id,
                                          fpga_properties &properties) {
  opaegrpc::GetPropertiesRequest request;
  request.set_allocated_token_id(to_grpc_fpga_remote_id(token_id));

  opaegrpc::GetPropertiesReply reply;
  ClientContext context;

  Status status = stub_->fpgaGetProperties(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaGetProperties() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  std::cout << "fpgaGetProperties reply " << reply << std::endl;

  if (reply.has_properties())
    properties = to_opae_fpga_properties(this, reply.properties());

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaUpdateProperties(const fpga_remote_id &token_id,
                                             fpga_properties &properties) {
  opaegrpc::UpdatePropertiesRequest request;
  request.set_allocated_token_id(to_grpc_fpga_remote_id(token_id));

  opaegrpc::UpdatePropertiesReply reply;
  ClientContext context;

  Status status = stub_->fpgaUpdateProperties(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaUpdateProperties() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  std::cout << "fpgaUpdateProperties reply " << reply << std::endl;

  if (reply.has_properties())
    properties = to_opae_fpga_properties(this, reply.properties());

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaOpen(const fpga_remote_id &token_id, int flags,
                                 fpga_handle_header &header) {
  opaegrpc::OpenRequest request;
  request.set_allocated_token_id(to_grpc_fpga_remote_id(token_id));
  request.set_flags(flags);

  opaegrpc::OpenReply reply;
  ClientContext context;

  Status status = stub_->fpgaOpen(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaOpen() gRPC failed: %s", status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  std::cout << "fpgaOpen reply " << reply << std::endl;

  if (reply.has_handle()) header = to_opae_handle_header(reply.handle());

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaClose(const fpga_remote_id &handle_id) {
  opaegrpc::CloseRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));

  opaegrpc::CloseReply reply;
  ClientContext context;

  Status status = stub_->fpgaClose(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaClose() gRPC failed: %s", status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  std::cout << "fpgaClose reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaReset(const fpga_remote_id &handle_id) {
  opaegrpc::ResetRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));

  opaegrpc::ResetReply reply;
  ClientContext context;

  Status status = stub_->fpgaReset(&context, request, &reply);

  if (!status.ok()) {
    OPAE_ERR("fpgaReset() gRPC failed: %s", status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  std::cout << "fpgaReset reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaGetPropertiesFromHandle(
    const fpga_remote_id &handle_id, fpga_properties &properties) {
  opaegrpc::GetPropertiesFromHandleRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));

  opaegrpc::GetPropertiesFromHandleReply reply;
  ClientContext context;

  Status status = stub_->fpgaGetPropertiesFromHandle(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaGetPropertiesFromHandle() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  std::cout << "fpgaGetPropertiesFromHandle reply " << reply << std::endl;

  if (reply.has_properties())
    properties = to_opae_fpga_properties(this, reply.properties());

  return to_opae_fpga_result[reply.result()];
}
