// Copyright(c) 2017, Intel Corporation
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
#include <cstdint>
#include "accelerator.h"

namespace intel
{
namespace fpga
{
namespace nlb
{

//MOD * other utility values
const uint32_t start_value_int = 2;         // start bit in ctl
const uint64_t start_value_hls = 1;         // start signal to slave register (write only)
const uint32_t stop_value_int = 4;          // stop signal to component onto masterRead
const uint64_t done_value_hls = 1;          // done value in slave register (read only)
const uint64_t interrupt_status_hls = 1;    // written to reset done value
const uint64_t complete_value_int = 1;      // complete signal from component onto masterWrite
const uint64_t interrupt_hls = 1;           // enables interrupt in slave register

const uint64_t cacheline_size = 64;
const uint64_t max_cpu_cache_size = 100 * 1024*1024;
const uint64_t max_cachelines = 65535;
const uint32_t max_cmdq_counter = 32768;
const uint32_t mtnlb_max_threads = 2047;  // 11 bits
const uint32_t mtnlb_max_count = 1048575; // 20 bits
const uint32_t mtnlb_max_stride = 4294967295; // 32 bit

const uint32_t hls_base_offset = 0x80; //match tho component offset in Platform Designer

/// @brief test mode constants to use for
/// NLB config register
enum class nlb3_csr : uint32_t
{
    busy        = hls_base_offset + 0x0000,
    start       = hls_base_offset + 0x0008,
    interrupt   = hls_base_offset + 0x0010,
    done        = hls_base_offset + 0x0018,
    retrn       = hls_base_offset + 0x0020,
    masterRead  = hls_base_offset + 0x0028,
    masterWrite = hls_base_offset + 0x0030,
    num_lines   = hls_base_offset + 0x0038,
    ctl         = hls_base_offset + 0x0040,
    cfg         = hls_base_offset + 0x0048,
    strided_acs = hls_base_offset + 0x0050
};

enum class nlb3_cfg : uint32_t
{
    cont     	 = 0x002,
    read     	 = 0x004,
    write    	 = 0x008,
    trput    	 = 0x00c,
    mcl2         = 0x0020,
    mcl4         = 0x0060,
};

enum class nlb3_dsm : uint32_t
{
    test_complete_offs= 0x0040,
    num_reads_offs    = 0x0050,
    num_writes_offs   = 0x0054,
};

} // end of namespace diag
} // end of namespace fpga
} // end of namespace intel

