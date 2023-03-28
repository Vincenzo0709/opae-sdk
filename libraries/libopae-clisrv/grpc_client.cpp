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

  if (debug_) std::cout << "fpgaEnumerate reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) {
    num_matches = reply.num_matches();
    const size_t toks = std::min(num_matches, reply.max_tokens());

    tokens.reserve(toks);
    tokens.resize(toks);
    std::transform(reply.tokens().cbegin(), reply.tokens().cbegin() + toks,
                   tokens.begin(), to_opae_token_header);
  }

  return res;
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

  if (debug_) std::cout << "fpgaDestroyToken reply " << reply << std::endl;

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

  if (debug_) std::cout << "fpgaCloneToken reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) dest_token_hdr = to_opae_token_header(reply.dest_token());

  return res;
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

  if (debug_) std::cout << "fpgaGetProperties reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK)
    properties = to_opae_fpga_properties(this, reply.properties());

  return res;
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

  if (debug_) std::cout << "fpgaUpdateProperties reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK)
    properties = to_opae_fpga_properties(this, reply.properties());

  return res;
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

  if (debug_) std::cout << "fpgaOpen reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) header = to_opae_handle_header(reply.handle());

  return res;
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

  if (debug_) std::cout << "fpgaClose reply " << reply << std::endl;

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

  if (debug_) std::cout << "fpgaReset reply " << reply << std::endl;

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

  if (debug_)
    std::cout << "fpgaGetPropertiesFromHandle reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK)
    properties = to_opae_fpga_properties(this, reply.properties());

  return res;
}

fpga_result OPAEClient::fpgaMapMMIO(const fpga_remote_id &handle_id,
                                    uint32_t mmio_num,
                                    fpga_remote_id &mmio_id) {
  opaegrpc::MapMMIORequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_mmio_num(mmio_num);

  opaegrpc::MapMMIOReply reply;
  ClientContext context;

  Status status = stub_->fpgaMapMMIO(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaMapMMIO() gRPC failed: %s", status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaMapMMIO reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) mmio_id = to_opae_fpga_remote_id(reply.mmio_id());

  return res;
}

fpga_result OPAEClient::fpgaUnmapMMIO(const fpga_remote_id &handle_id,
                                      const fpga_remote_id &mmio_id,
                                      uint32_t mmio_num) {
  opaegrpc::UnmapMMIORequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_allocated_mmio_id(to_grpc_fpga_remote_id(mmio_id));
  request.set_mmio_num(mmio_num);

  opaegrpc::UnmapMMIOReply reply;
  ClientContext context;

  Status status = stub_->fpgaUnmapMMIO(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaUnmapMMIO() gRPC failed: %s", status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaUnmapMMIO reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaReadMMIO32(const fpga_remote_id &handle_id,
                                       uint32_t mmio_num, uint64_t offset,
                                       uint32_t &value) {
  opaegrpc::ReadMMIO32Request request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_mmio_num(mmio_num);
  request.set_offset(offset);

  opaegrpc::ReadMMIO32Reply reply;
  ClientContext context;

  Status status = stub_->fpgaReadMMIO32(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaReadMMIO32() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaReadMMIO32 reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) value = reply.value();

  return res;
}

fpga_result OPAEClient::fpgaWriteMMIO32(const fpga_remote_id &handle_id,
                                        uint32_t mmio_num, uint64_t offset,
                                        uint32_t value) {
  opaegrpc::WriteMMIO32Request request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_mmio_num(mmio_num);
  request.set_offset(offset);
  request.set_value(value);

  opaegrpc::WriteMMIO32Reply reply;
  ClientContext context;

  Status status = stub_->fpgaWriteMMIO32(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaWriteMMIO32() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaWriteMMIO32 reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaReadMMIO64(const fpga_remote_id &handle_id,
                                       uint32_t mmio_num, uint64_t offset,
                                       uint64_t &value) {
  opaegrpc::ReadMMIO64Request request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_mmio_num(mmio_num);
  request.set_offset(offset);

  opaegrpc::ReadMMIO64Reply reply;
  ClientContext context;

  Status status = stub_->fpgaReadMMIO64(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaReadMMIO64() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaReadMMIO64 reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) value = reply.value();

  return res;
}

fpga_result OPAEClient::fpgaWriteMMIO64(const fpga_remote_id &handle_id,
                                        uint32_t mmio_num, uint64_t offset,
                                        uint64_t value) {
  opaegrpc::WriteMMIO64Request request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_mmio_num(mmio_num);
  request.set_offset(offset);
  request.set_value(value);

  opaegrpc::WriteMMIO64Reply reply;
  ClientContext context;

  Status status = stub_->fpgaWriteMMIO64(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaWriteMMIO64() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaWriteMMIO64 reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

constexpr size_t bits_to_bytes(size_t bits) { return bits / 8; }

fpga_result OPAEClient::fpgaWriteMMIO512(const fpga_remote_id &handle_id,
                                         uint32_t mmio_num, uint64_t offset,
                                         const void *value) {
  opaegrpc::WriteMMIO512Request request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_mmio_num(mmio_num);
  request.set_offset(offset);

  const char *pvals = (const char *)value;
  std::string *alloced_str = new std::string(pvals, bits_to_bytes(512));
  request.set_allocated_values(alloced_str);

  opaegrpc::WriteMMIO512Reply reply;
  ClientContext context;

  Status status = stub_->fpgaWriteMMIO512(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaWriteMMIO512() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaWriteMMIO512 reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaPrepareBuffer(const fpga_remote_id &handle_id,
                                          uint64_t length, void **buf_addr,
                                          int flags, fpga_remote_id &buf_id) {
  opaegrpc::PrepareBufferRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_length(length);
  request.set_have_buf_addr(false);
  request.set_pre_allocated_addr(0);

  if (buf_addr) {
    request.set_have_buf_addr(true);
    request.set_pre_allocated_addr((uint64_t)*buf_addr);
  }

  request.set_flags(flags);

  opaegrpc::PrepareBufferReply reply;
  ClientContext context;

  Status status = stub_->fpgaPrepareBuffer(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaPrepareBuffer() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaPrepareBuffer reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) buf_id = to_opae_fpga_remote_id(reply.buf_id());

  return res;
}

fpga_result OPAEClient::fpgaReleaseBuffer(const fpga_remote_id &handle_id,
                                          const fpga_remote_id &buf_id) {
  opaegrpc::ReleaseBufferRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_allocated_buf_id(to_grpc_fpga_remote_id(buf_id));

  opaegrpc::ReleaseBufferReply reply;
  ClientContext context;

  Status status = stub_->fpgaReleaseBuffer(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaReleaseBuffer() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaReleaseBuffer reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaGetIOAddress(const fpga_remote_id &handle_id,
                                         const fpga_remote_id &buf_id,
                                         uint64_t &ioaddr) {
  opaegrpc::GetIOAddressRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_allocated_buf_id(to_grpc_fpga_remote_id(buf_id));

  opaegrpc::GetIOAddressReply reply;
  ClientContext context;

  Status status = stub_->fpgaGetIOAddress(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaGetIOAddress() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaGetIOAddress reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) ioaddr = reply.ioaddr();

  return res;
}

fpga_result OPAEClient::fpgaReadError(const fpga_remote_id &token_id,
                                      uint32_t error_num, uint64_t &value) {
  opaegrpc::ReadErrorRequest request;
  request.set_allocated_token_id(to_grpc_fpga_remote_id(token_id));
  request.set_error_num(error_num);

  opaegrpc::ReadErrorReply reply;
  ClientContext context;

  Status status = stub_->fpgaReadError(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaReadError() gRPC failed: %s", status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaReadError reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) value = reply.value();

  return res;
}

fpga_result OPAEClient::fpgaGetErrorInfo(const fpga_remote_id &token_id,
                                         uint32_t error_num,
                                         fpga_error_info &error_info) {
  opaegrpc::GetErrorInfoRequest request;
  request.set_allocated_token_id(to_grpc_fpga_remote_id(token_id));
  request.set_error_num(error_num);

  opaegrpc::GetErrorInfoReply reply;
  ClientContext context;

  Status status = stub_->fpgaGetErrorInfo(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaGetErrorInfo() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaGetErrorInfo reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) error_info = to_opae_fpga_error_info(reply.error_info());

  return res;
}

fpga_result OPAEClient::fpgaClearError(const fpga_remote_id &token_id,
                                       uint32_t error_num) {
  opaegrpc::ClearErrorRequest request;
  request.set_allocated_token_id(to_grpc_fpga_remote_id(token_id));
  request.set_error_num(error_num);

  opaegrpc::ClearErrorReply reply;
  ClientContext context;

  Status status = stub_->fpgaClearError(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaClearError() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaClearError reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaClearAllErrors(const fpga_remote_id &token_id) {
  opaegrpc::ClearAllErrorsRequest request;
  request.set_allocated_token_id(to_grpc_fpga_remote_id(token_id));

  opaegrpc::ClearAllErrorsReply reply;
  ClientContext context;

  Status status = stub_->fpgaClearAllErrors(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaClearAllErrors() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaClearAllErrors reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaTokenGetObject(const fpga_remote_id &token_id,
                                           const char *name, int flags,
                                           fpga_remote_id &object_id) {
  opaegrpc::TokenGetObjectRequest request;
  request.set_allocated_token_id(to_grpc_fpga_remote_id(token_id));
  request.set_allocated_name(new std::string(name));
  request.set_flags(flags);

  opaegrpc::TokenGetObjectReply reply;
  ClientContext context;

  Status status = stub_->fpgaTokenGetObject(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaTokenGetObject() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaTokenGetObject reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) object_id = to_opae_fpga_remote_id(reply.object_id());

  return res;
}

fpga_result OPAEClient::fpgaDestroyObject(const fpga_remote_id &object_id) {
  opaegrpc::DestroyObjectRequest request;
  request.set_allocated_object_id(to_grpc_fpga_remote_id(object_id));

  opaegrpc::DestroyObjectReply reply;
  ClientContext context;

  Status status = stub_->fpgaDestroyObject(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaDestroyObject() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaDestroyObject reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaObjectGetType(const fpga_remote_id &object_id,
                                          fpga_sysobject_type &type) {
  opaegrpc::ObjectGetTypeRequest request;
  request.set_allocated_object_id(to_grpc_fpga_remote_id(object_id));

  opaegrpc::ObjectGetTypeReply reply;
  ClientContext context;

  Status status = stub_->fpgaObjectGetType(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaObjectGetType() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaObjectGetType reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) type = to_opae_fpga_sysobject_type[reply.type()];

  return res;
}

fpga_result OPAEClient::fpgaObjectGetName(const fpga_remote_id &object_id,
                                          char *name, size_t max_len) {
  opaegrpc::ObjectGetNameRequest request;
  request.set_allocated_object_id(to_grpc_fpga_remote_id(object_id));

  opaegrpc::ObjectGetNameReply reply;
  ClientContext context;

  Status status = stub_->fpgaObjectGetName(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaObjectGetName() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaObjectGetName reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) {
    const std::string &n = reply.name();
    size_t len = std::min(max_len - 1, n.length());
    std::copy(n.c_str(), n.c_str() + len + 1, name);
  }

  return res;
}

fpga_result OPAEClient::fpgaObjectGetSize(const fpga_remote_id &object_id,
                                          int flags, uint32_t &value) {
  opaegrpc::ObjectGetSizeRequest request;
  request.set_allocated_object_id(to_grpc_fpga_remote_id(object_id));
  request.set_flags(flags);

  opaegrpc::ObjectGetSizeReply reply;
  ClientContext context;

  Status status = stub_->fpgaObjectGetSize(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaObjectGetSize() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaObjectGetSize reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) value = reply.value();

  return res;
}

fpga_result OPAEClient::fpgaObjectRead(const fpga_remote_id &object_id,
                                       uint8_t *buffer, size_t offset,
                                       size_t length, int flags) {
  opaegrpc::ObjectReadRequest request;
  request.set_allocated_object_id(to_grpc_fpga_remote_id(object_id));
  request.set_offset(offset);
  request.set_length(length - 1);
  request.set_flags(flags);

  opaegrpc::ObjectReadReply reply;
  ClientContext context;

  Status status = stub_->fpgaObjectRead(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaObjectRead() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaObjectRead reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) {
    const std::string &value(reply.value());
    size_t len = std::min(length - 1, value.length());
    std::copy(value.c_str(), value.c_str() + len + 1, buffer);
  }

  return res;
}

fpga_result OPAEClient::fpgaObjectRead64(const fpga_remote_id &object_id,
                                         int flags, uint64_t &value) {
  opaegrpc::ObjectRead64Request request;
  request.set_allocated_object_id(to_grpc_fpga_remote_id(object_id));
  request.set_flags(flags);

  opaegrpc::ObjectRead64Reply reply;
  ClientContext context;

  Status status = stub_->fpgaObjectRead64(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaObjectRead64() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaObjectRead64 reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) value = reply.value();

  return res;
}

fpga_result OPAEClient::fpgaObjectWrite64(const fpga_remote_id &object_id,
                                          uint64_t value, int flags) {
  opaegrpc::ObjectWrite64Request request;
  request.set_allocated_object_id(to_grpc_fpga_remote_id(object_id));
  request.set_value(value);
  request.set_flags(flags);

  opaegrpc::ObjectWrite64Reply reply;
  ClientContext context;

  Status status = stub_->fpgaObjectWrite64(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaObjectWrite64() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaObjectWrite64 reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaHandleGetObject(const fpga_remote_id &handle_id,
                                            const char *name, int flags,
                                            fpga_remote_id &object_id) {
  opaegrpc::HandleGetObjectRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_allocated_name(new std::string(name));
  request.set_flags(flags);

  opaegrpc::HandleGetObjectReply reply;
  ClientContext context;

  Status status = stub_->fpgaHandleGetObject(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaHandleGetObject() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaHandleGetObject reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) object_id = to_opae_fpga_remote_id(reply.object_id());

  return res;
}

fpga_result OPAEClient::fpgaObjectGetObject(const fpga_remote_id &parent_id,
                                            const char *name, int flags,
                                            fpga_remote_id &child_id) {
  opaegrpc::ObjectGetObjectRequest request;
  request.set_allocated_object_id(to_grpc_fpga_remote_id(parent_id));
  request.set_allocated_name(new std::string(name));
  request.set_flags(flags);

  opaegrpc::ObjectGetObjectReply reply;
  ClientContext context;

  Status status = stub_->fpgaObjectGetObject(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaObjectGetObject() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaObjectGetObject reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) child_id = to_opae_fpga_remote_id(reply.object_id());

  return res;
}

fpga_result OPAEClient::fpgaObjectGetObjectAt(const fpga_remote_id &parent_id,
                                              size_t index,
                                              fpga_remote_id &child_id) {
  opaegrpc::ObjectGetObjectAtRequest request;
  request.set_allocated_object_id(to_grpc_fpga_remote_id(parent_id));
  request.set_index(index);

  opaegrpc::ObjectGetObjectAtReply reply;
  ClientContext context;

  Status status = stub_->fpgaObjectGetObjectAt(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaObjectGetObjectAt() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaObjectGetObjectAt reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) child_id = to_opae_fpga_remote_id(reply.object_id());

  return res;
}

fpga_result OPAEClient::fpgaSetUserClock(const fpga_remote_id &handle_id,
                                         uint64_t high_clk, uint64_t low_clk,
                                         int flags) {
  opaegrpc::SetUserClockRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_high_clk(high_clk);
  request.set_low_clk(low_clk);
  request.set_flags(flags);

  opaegrpc::SetUserClockReply reply;
  ClientContext context;

  Status status = stub_->fpgaSetUserClock(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaSetUserClock() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaSetUserClock reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaGetUserClock(const fpga_remote_id &handle_id,
                                         int flags, uint64_t &high_clk,
                                         uint64_t &low_clk) {
  opaegrpc::GetUserClockRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_flags(flags);

  opaegrpc::GetUserClockReply reply;
  ClientContext context;

  Status status = stub_->fpgaGetUserClock(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaGetUserClock() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaGetUserClock reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) {
    high_clk = reply.high_clk();
    low_clk = reply.low_clk();
  }

  return res;
}

fpga_result OPAEClient::fpgaGetNumMetrics(const fpga_remote_id &handle_id,
                                          uint64_t &num_metrics) {
  opaegrpc::GetNumMetricsRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));

  opaegrpc::GetNumMetricsReply reply;
  ClientContext context;

  Status status = stub_->fpgaGetNumMetrics(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaGetNumMetrics() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaGetNumMetrics reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) num_metrics = reply.num_metrics();

  return res;
}

fpga_result OPAEClient::fpgaGetMetricsInfo(
    const fpga_remote_id &handle_id, uint64_t &num_metrics,
    std::vector<fpga_metric_info> &info) {
  const uint64_t input_num_metrics = num_metrics;
  opaegrpc::GetMetricsInfoRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_num_metrics(input_num_metrics);

  opaegrpc::GetMetricsInfoReply reply;
  ClientContext context;

  Status status = stub_->fpgaGetMetricsInfo(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaGetMetricsInfo() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaGetMetricsInfo reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) {
    uint64_t output_num_metrics = reply.num_metrics();
    output_num_metrics = std::min(input_num_metrics, output_num_metrics);

    num_metrics = output_num_metrics;

    info.reserve(output_num_metrics);
    info.resize(output_num_metrics);

    std::transform(reply.info().cbegin(),
                   reply.info().cbegin() + output_num_metrics, info.begin(),
                   to_opae_fpga_metric_info);
  }

  return res;
}

fpga_result OPAEClient::fpgaGetMetricsByIndex(
    const fpga_remote_id &handle_id, const std::vector<uint64_t> &metric_num,
    uint64_t num_metric_indexes, std::vector<fpga_metric> &metrics) {
  uint64_t input_num_metric_indexes = num_metric_indexes;
  opaegrpc::GetMetricsByIndexRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));

  input_num_metric_indexes =
      std::min(input_num_metric_indexes, metric_num.size());

  for (auto n : metric_num) request.add_metric_num(n);

  request.set_num_metric_indexes(input_num_metric_indexes);

  opaegrpc::GetMetricsByIndexReply reply;
  ClientContext context;

  Status status = stub_->fpgaGetMetricsByIndex(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaGetMetricsByIndex() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaGetMetricsByIndex reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) {
    uint64_t output_num_metric_indexes = reply.num_metric_indexes();
    output_num_metric_indexes =
        std::min(output_num_metric_indexes, (uint64_t)reply.metrics().size());

    metrics.reserve(output_num_metric_indexes);
    metrics.resize(output_num_metric_indexes);
    std::transform(reply.metrics().cbegin(),
                   reply.metrics().cbegin() + output_num_metric_indexes,
                   metrics.begin(), to_opae_fpga_metric);
  }

  return res;
}

fpga_result OPAEClient::fpgaGetMetricsByName(
    const fpga_remote_id &handle_id,
    const std::vector<std::string> &metrics_names, uint64_t num_metric_names,
    std::vector<fpga_metric> &metrics) {
  uint64_t input_num_metric_names = num_metric_names;
  opaegrpc::GetMetricsByNameRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));

  input_num_metric_names =
      std::min(input_num_metric_names, metrics_names.size());

  for (auto n : metrics_names) *request.add_metrics_names() = n;

  request.set_num_metric_names(input_num_metric_names);

  opaegrpc::GetMetricsByNameReply reply;
  ClientContext context;

  Status status = stub_->fpgaGetMetricsByName(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaGetMetricsByName() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaGetMetricsByName reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) {
    uint64_t output_num_metric_names = reply.num_metric_names();
    output_num_metric_names =
        std::min(output_num_metric_names, (uint64_t)reply.metrics().size());

    metrics.reserve(output_num_metric_names);
    metrics.resize(output_num_metric_names);
    std::transform(reply.metrics().cbegin(),
                   reply.metrics().cbegin() + output_num_metric_names,
                   metrics.begin(), to_opae_fpga_metric);
  }

  return res;
}

fpga_result OPAEClient::fpgaGetMetricsThresholdInfo(
    const fpga_remote_id &handle_id, uint32_t &num_thresholds,
    std::vector<metric_threshold> &metric_threshold) {
  uint64_t input_num_thresholds = num_thresholds;
  opaegrpc::GetMetricsThresholdInfoRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_num_thresholds(input_num_thresholds);

  opaegrpc::GetMetricsThresholdInfoReply reply;
  ClientContext context;

  Status status = stub_->fpgaGetMetricsThresholdInfo(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaGetMetricsThresholdInfo() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_)
    std::cout << "fpgaGetMetricsThresholdInfo reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) {
    uint64_t output_num_thresholds = reply.num_thresholds();
    output_num_thresholds = std::min(output_num_thresholds,
                                     (uint64_t)reply.metric_threshold().size());

    num_thresholds = output_num_thresholds;

    metric_threshold.reserve(output_num_thresholds);
    metric_threshold.resize(output_num_thresholds);
    std::transform(reply.metric_threshold().cbegin(),
                   reply.metric_threshold().cbegin() + output_num_thresholds,
                   metric_threshold.begin(), to_opae_metric_threshold);
  }

  return res;
}

fpga_result OPAEClient::fpgaReconfigureSlotByName(
    const fpga_remote_id &handle_id, uint32_t slot, const std::string &path,
    int flags) {
  opaegrpc::ReconfigureSlotByNameRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_slot(slot);
  request.set_allocated_path(new std::string(path));
  request.set_flags(flags);

  opaegrpc::ReconfigureSlotByNameReply reply;
  ClientContext context;

  Status status = stub_->fpgaReconfigureSlotByName(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaReconfigureSlotByName() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_)
    std::cout << "fpgaReconfigureSlotByName reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaBufMemSet(const fpga_remote_id &handle_id,
                                      const fpga_remote_id &buf_id,
                                      size_t offset, int c, size_t n) {
  opaegrpc::BufMemSetRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_allocated_buf_id(to_grpc_fpga_remote_id(buf_id));
  request.set_offset(offset);
  request.set_c(c);
  request.set_n(n);

  opaegrpc::BufMemSetReply reply;
  ClientContext context;

  Status status = stub_->fpgaBufMemSet(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaBufMemSet() gRPC failed: %s", status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaBufMemSet reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaBufMemCpyToRemote(const fpga_remote_id &handle_id,
                                              const fpga_remote_id &dest_buf_id,
                                              size_t dest_offset,
                                              const void *src, size_t n) {
  opaegrpc::BufMemCpyToRemoteRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_allocated_dest_buf_id(to_grpc_fpga_remote_id(dest_buf_id));
  request.set_dest_offset(dest_offset);
  request.set_allocated_src(
      new std::string(reinterpret_cast<const char *>(src), n));
  request.set_n(n);

  opaegrpc::BufMemCpyToRemoteReply reply;
  ClientContext context;

  Status status = stub_->fpgaBufMemCpyToRemote(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaBufMemCpyToRemote() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaBufMemCpyToRemote reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaBufPoll(const fpga_remote_id &handle_id,
                                    const fpga_remote_id &buf_id,
                                    uint64_t offset, int width, uint64_t mask,
                                    uint64_t expected_value,
                                    uint64_t sleep_interval,
                                    uint64_t loops_timeout) {
  opaegrpc::BufPollRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_allocated_buf_id(to_grpc_fpga_remote_id(buf_id));
  request.set_offset(offset);
  request.set_width(width);
  request.set_mask(mask);
  request.set_expected_value(expected_value);
  request.set_sleep_interval(sleep_interval);
  request.set_loops_timeout(loops_timeout);

  opaegrpc::BufPollReply reply;
  ClientContext context;

  Status status = stub_->fpgaBufPoll(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaBufPoll() gRPC failed: %s", status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaBufPoll reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}

fpga_result OPAEClient::fpgaBufMemCmp(const fpga_remote_id &handle_id,
                                      const fpga_remote_id &bufa_id,
                                      size_t bufa_offset,
                                      const fpga_remote_id &bufb_id,
                                      size_t bufb_offset, size_t n,
                                      int &cmp_result) {
  opaegrpc::BufMemCmpRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_allocated_bufa_id(to_grpc_fpga_remote_id(bufa_id));
  request.set_bufa_offset(bufa_offset);
  request.set_allocated_bufb_id(to_grpc_fpga_remote_id(bufb_id));
  request.set_bufb_offset(bufb_offset);
  request.set_n(n);

  opaegrpc::BufMemCmpReply reply;
  ClientContext context;

  Status status = stub_->fpgaBufMemCmp(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaBufMemCmp() gRPC failed: %s", status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaBufMemCmp reply " << reply << std::endl;

  fpga_result res = to_opae_fpga_result[reply.result()];

  if (res == FPGA_OK) cmp_result = reply.cmp_result();

  return res;
}

fpga_result OPAEClient::fpgaBufWritePattern(const fpga_remote_id &handle_id,
                                            const fpga_remote_id &buf_id,
                                            const char *pattern_name) {
  opaegrpc::BufWritePatternRequest request;
  request.set_allocated_handle_id(to_grpc_fpga_remote_id(handle_id));
  request.set_allocated_buf_id(to_grpc_fpga_remote_id(buf_id));
  request.set_allocated_pattern_name(new std::string(pattern_name));

  opaegrpc::BufWritePatternReply reply;
  ClientContext context;

  Status status = stub_->fpgaBufWritePattern(&context, request, &reply);
  if (!status.ok()) {
    OPAE_ERR("fpgaBufWritePattern() gRPC failed: %s",
             status.error_message().c_str());
    OPAE_ERR("details: %s", status.error_details().c_str());
    return FPGA_EXCEPTION;
  }

  if (debug_) std::cout << "fpgaBufWritePattern reply " << reply << std::endl;

  return to_opae_fpga_result[reply.result()];
}