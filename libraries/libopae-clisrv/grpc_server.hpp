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
#pragma once

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <map>

#include <opae/fpga.h>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "convert.hpp"

#include "opae.grpc.pb.h"
#include "opae.pb.h"

using grpc::ServerContext;
using grpc::Status;
using opaegrpc::OPAEService;
using opaegrpc::EnumerateRequest;
using opaegrpc::EnumerateReply;
using opaegrpc::DestroyTokenRequest;
using opaegrpc::DestroyTokenReply;
using opaegrpc::CloneTokenRequest;
using opaegrpc::CloneTokenReply;

class OPAEServiceImpl final : public OPAEService::Service
{
  public:
    typedef std::map<fpga_remote_id, fpga_token> token_map_t;

    Status fpgaEnumerate(ServerContext *context, const EnumerateRequest *request, EnumerateReply *reply) override;
    Status fpgaDestroyToken(ServerContext *context, const DestroyTokenRequest *request, DestroyTokenReply *reply) override;
    Status fpgaCloneToken(ServerContext *context, const CloneTokenRequest *request, CloneTokenReply *reply) override;


    fpga_token find_token(const fpga_remote_id &rid) const
    {
      token_map_t::const_iterator it = token_map_.find(rid);
      return (it == token_map_.end()) ? nullptr : it->second;
    }

    bool add_token(const fpga_remote_id &rid, fpga_token token)
    {
      std::pair<token_map_t::iterator, bool> res =
        token_map_.insert(std::make_pair(rid, token));
      return res.second;
    }

    bool remove_token(const fpga_remote_id &rid)
    {
      token_map_t::iterator it = token_map_.find(rid);

      if (it == token_map_.end())
        return false;

      token_map_.erase(it);

      return true;
    }

  private:
    token_map_t token_map_;
};
