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

#include <sstream>
#include <iomanip>
#include "nlb_stats_hls.h"

namespace intel
{
namespace fpga
{
namespace nlb
{

nlb_stats::nlb_stats(dma_buffer::ptr_t out,
                     uint32_t cachelines,
                     bool continuous,
                     bool suppress_hdr,
                     bool csv)
: out_(out)
, cachelines_(cachelines)
, continuous_(continuous)
, suppress_hdr_(suppress_hdr)
, csv_(csv)
{

}

std::ostream & operator << (std::ostream &os, const nlb_stats &stats)
{
    auto header = !stats.suppress_hdr_;
    auto csv = stats.csv_;

    auto num_reads = stats.out_->read<uint64_t>(BUF_SIZE_LONG + 1);
    auto num_writes = stats.out_->read<uint64_t>(BUF_SIZE_LONG + 2);

    if (csv)
    {
        if (header)
        {
            os << "Cachelines,Read_Count,Write_Count,Cache_Rd_Hit,Cache_Wr_Hit,Cache_Rd_Miss,Cache_Wr_Miss,Eviction,'Clocks(@"
               << "?" << ")',Rd_Bandwidth,Wr_Bandwidth,VH0_Rd_Count,VH0_Wr_Count,VH1_Rd_Count,VH1_Wr_Count,VL0_Rd_Count,VL0_Wr_Count" << std::endl;
        }

        os << stats.cachelines_                                         << ','
           << num_reads                                                 << ','
           << num_writes                                                << ','
           << "0"                                                       << ','
           << "0"                                                       << ','
           << "0"                                                       << ','
           << "0"                                                       << ','
           << "0"                                                       << ','
           << "0"                                                       << ','
           << stats.read_bandwidth()                                    << ','
           << stats.write_bandwidth()                                   << ','
           << "0"                                                       << ','
           << "0"                                                       << ','
           << "0"                                                       << ','
           << "0"                                                       << ','
           << "0"                                                       << ','
           << "0"                                                       << std::endl;
    }
    else
    {
        os << std::endl;

        if (header)
        {
                 //0123456789 0123456789 01234567890 012345678901 012345678901 0123456789012 0123456789012 0123456789 0123456789012
            os << "Cachelines Read_Count Write_Count Cache_Rd_Hit Cache_Wr_Hit Cache_Rd_Miss Cache_Wr_Miss   Eviction 'Clocks(@"
               << "?" << ")'"
                   // 01234567890123 01234567890123
               << "   Rd_Bandwidth   Wr_Bandwidth" << std::endl;
        }

        os << std::setw(10) << stats.cachelines_                                        << ' '
           << std::setw(10) << num_reads                                                << ' '
           << std::setw(11) << num_writes                                               << ' '
           << std::setw(12) << "0"                                                      << ' '
           << std::setw(12) << "0"                                                      << ' '
           << std::setw(13) << "0"                                                      << ' '
           << std::setw(13) << "0"                                                      << ' '
           << std::setw(10) << "0"                                                      << ' '
           << std::setw(11) << "0"                                                      << ' '
           << std::setw(14) << stats.read_bandwidth()                                   << ' '
           << std::setw(14) << stats.write_bandwidth()
           << std::endl     << std::endl;

        if (header)
        {
                // 012345678901 012345678901 012345678901 012345678901 012345678901 012345678901
            os << "VH0_Rd_Count VH0_Wr_Count VH1_Rd_Count VH1_Wr_Count VL0_Rd_Count VL0_Wr_Count " << std::endl;
        }

        os << std::setw(12) << "0" << ' '
           << std::setw(12) << "0" << ' '
           << std::setw(12) << "0" << ' '
           << std::setw(12) << "0" << ' '
           << std::setw(12) << "0" << ' '
           << std::setw(12) << "0" << ' '
           << std::endl     << std::endl;
    }

    return os;
}

std::string nlb_stats::read_bandwidth() const
{
    const double giga = 1000.0 * 1000.0 * 1000.0;

    const double Rds   = out_->read<uint64_t>(BUF_SIZE_LONG + 1);

    double bw = (Rds * CCI_BYTES * BYTE_BITS) / 5;

    bw /= giga;

    std::ostringstream oss;

    oss.precision(3);
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << bw;
    if (!(csv_ && suppress_hdr_))
    {
        oss << " GB/s";
    }

    return oss.str();
}

std::string nlb_stats::write_bandwidth() const
{
    const double giga = 1000.0 * 1000.0 * 1000.0;

    const double Wrs   = out_->read<uint64_t>(BUF_SIZE_LONG + 2);

    double bw = (Wrs * CL(CCI_LINES) * 8) / 5;

    bw /= giga;

    std::ostringstream oss;

    oss.precision(3);
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << bw;
    if (!(csv_ && suppress_hdr_))
    {
        oss << " GB/s";
    }

    return oss.str();
}

} // end of namespace nlb
} // end of namespace fpga
} // end of namespace intel

