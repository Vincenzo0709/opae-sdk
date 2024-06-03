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
#include <iostream>
#include "dma_buffer.h"
#include "fpga_app/fpga_common.h"
#include "perf_counters.h"

namespace intel
{
namespace fpga
{
namespace nlb
{

// Dimensions and durations
#define BYTE_BITS       8
#define SHORT_BYTES     2
#define INT_BYTES       4
#define LONG_BYTES      8

#define LINE_BYTES      64
#define CCI_LINES       1
#define CCI_BYTES       (LINE_BYTES * CCI_LINES)
#define LINE_SHORTS     (LINE_BYTES / SHORT_BYTES)
#define LINE_INTS       (LINE_BYTES / INT_BYTES)
#define LINE_LONGS      (LINE_BYTES / LONG_BYTES)
#define STRIDE_ACS      1
#define DEFAULT_LINES   8388608

#define BUF_SIZE_BYTES      (STRIDE_ACS * DEFAULT_LINES * LINE_BYTES)
#define BUF_SIZE_INT        (BUF_SIZE_BYTES / 4)
#define BUF_SIZE_SHORT      (BUF_SIZE_BYTES / 2)
#define BUF_SIZE_LONG       (BUF_SIZE_BYTES / 8)

enum class dsm_version
{
    nlb_classic,
    cmdq_batch
};

class nlb_stats
{
public:
    nlb_stats(dma_buffer::ptr_t out,
              uint32_t cachelines,
              std::chrono::duration<double, std::milli> dur_,
              bool suppress_hdr=false,
              bool csv=false);

friend std::ostream & operator << (std::ostream &os, const nlb_stats &stats);

private:
    dma_buffer::ptr_t out_;
    uint32_t cachelines_;
    bool suppress_hdr_;
    bool csv_;
    std::chrono::duration<double, std::milli> dur_;

    std::string read_bandwidth() const;
    std::string write_bandwidth() const;
};

} // end of namespace nlb
} // end of namespace fpga
} // end of namespace intel

