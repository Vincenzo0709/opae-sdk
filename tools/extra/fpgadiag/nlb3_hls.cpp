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

#include "nlb3_hls.h"
#include "fpga_app/fpga_common.h"
#include "nlb_stats_hls.h"
#include "log.h"
#include <chrono>
#include <thread>

using namespace intel::fpga::nlb;
using namespace intel::utils;
using namespace std::chrono;

namespace intel
{
namespace fpga
{
namespace diag
{

nlb3::nlb3()
: name_("nlb3_hls")
, config_("nlb3_hls.json")
, target_("fpga")
, mode_("read")
, afu_id_("F7DF405C-BD7A-CF72-22F1-44B0B93ACD18")
, stride_acs_(1)
, num_strides_(0)
, step_(1)
, begin_(1)
, end_(1)
, cont_(false)
, suppress_header_(false)
, csv_format_(false)
, suppress_stats_(false)
, cachelines_(0)
, print_(false)
{
    options_.add_option<bool>("help",                'h', option::no_argument,   "Show help", false);
    options_.add_option<std::string>("config",       'c', option::with_argument, "Path to test config file", config_);
    options_.add_option<std::string>("target",       't', option::with_argument, "one of {fpga, ase}", target_);
    options_.add_option<std::string>("mode",         'm', option::with_argument, "mode {read, write, trput}", mode_);
    options_.add_option<uint32_t>("begin",           'b', option::with_argument, "where 1 <= <value> <= 65535", begin_);
    options_.add_option<uint32_t>("end",             'e', option::with_argument, "where 1 <= <value> <= 65535", end_);
    options_.add_option<uint32_t>("multi-cl",        'u', option::with_argument, "one of {1, 2, 4}", 1);
    options_.add_option<uint32_t>("strided-access",  'a', option::with_argument, "where 1 <= <value> <= 64", 1);
    options_.add_option<bool>("cont",                'L', option::no_argument,   "Continuous mode", cont_);
    options_.add_option<uint32_t>("timeout-usec",         option::with_argument, "Timeout for continuous mode (microseconds portion)", 0);
    options_.add_option<uint32_t>("timeout-msec",         option::with_argument, "Timeout for continuous mode (milliseconds portion)", 0);
    options_.add_option<uint32_t>("timeout-sec",          option::with_argument, "Timeout for continuous mode (seconds portion)", 1);
    options_.add_option<uint32_t>("timeout-min",          option::with_argument, "Timeout for continuous mode (minutes portion)", 0);
    options_.add_option<uint32_t>("timeout-hour",         option::with_argument, "Timeout for continuous mode (hours portion)", 0);
    options_.add_option<uint8_t>("socket-id",        'S', option::with_argument, "Socket id encoded in BBS");
    options_.add_option<uint8_t>("bus",              'B', option::with_argument, "Bus number of PCIe device");
    options_.add_option<uint8_t>("device",           'D', option::with_argument, "Device number of PCIe device");
    options_.add_option<uint8_t>("function",         'F', option::with_argument, "Function number of PCIe device");
    options_.add_option<std::string>("guid",         'G', option::with_argument, "accelerator id to enumerate", afu_id_);
    options_.add_option<bool>("suppress-hdr",             option::no_argument,   "Suppress column headers", suppress_header_);
    options_.add_option<bool>("csv",                 'V', option::no_argument,   "Comma separated value format", csv_format_);
    options_.add_option<bool>("suppress-stats",           option::no_argument,   "Show stats at end", suppress_stats_);
    options_.add_option<bool>("print",               'p', option::no_argument,  "Prints extra infos during execution", print_);
}

nlb3::~nlb3()
{
}

bool nlb3::setup()
{
    options_.get_value<std::string>("target", target_);
    options_.get_value<bool>("suppress-stats", suppress_stats_);
    options_.get_value<bool>("print", print_);

    // mode
    options_.get_value<std::string>("mode", mode_);
    if ((mode_ != "read") && (mode_ != "write") && (mode_ != "trput"))
    {
        std::cerr << "Invalid --mode: " << mode_ << std::endl;
        return false;
    }

    if (mode_ == "read")
    {
        cfg_ |= nlb3_cfg::read;
    }
    else if (mode_ == "write")
    {
        cfg_ |= nlb3_cfg::write;
    }
    else
    {
        cfg_ |= nlb3_cfg::trput;
    }

    // continuous?
    options_.get_value<bool>("cont", cont_);
    if (cont_)
    {
        cfg_ |= nlb3_cfg::cont;
    }

    // multi-cl
    uint32_t multi_cl = 1;
    options_.get_value<uint32_t>("multi-cl", multi_cl);
    switch(multi_cl)
    {
        case 1 :
            step_ = 1;
            break;
        case 2 :
            cfg_ |= nlb3_cfg::mcl2;
            step_ = 2;
            break;
        case 4 :
            cfg_ |= nlb3_cfg::mcl4;
            step_ = 4;
            break;
        default :
            log_.error("nlb3_hls") << "multi-cl must be one of {1, 2, 4}" << std::endl;
            return false;
    }

    // stride distance 
    if (options_.get_value<uint32_t>("strided-access", stride_acs_))
    {
        if (stride_acs_ > 64)
        {
            log_.error("nlb3_hls") << "strided access too big" << std::endl;
            return false;
        }
        num_strides_ = (stride_acs_ - 1);
    }

    // begin, end
    options_.get_value<uint32_t>("begin", begin_);
    options_.get_value<uint32_t>("end", end_);
    auto end_opt = options_.find("end");

    if (begin_ > MAX_CL)
    {
        log_.error("nlb3_hls") << "begin: " << begin_ << " is greater than max: " << MAX_CL << std::endl;
        return false;
    }

    if (begin_ % step_ > 0)
    {
        log_.error("nlb3_hls") << "begin: " << begin_ << " is not a multiple of mcl: " << step_ << std::endl;
        return false;
    }

    if (end_opt && !end_opt->is_set())
    {
        end_ = begin_;
    }
    else
    {
        if (end_ < begin_)
        {
            log_.warn("nlb3_hls") << "end: " << end_ << " is less than begin: " << begin_ << std::endl;
            end_ = begin_;
        }
    }

    // timeout
    if (cont_)
    {
        cont_timeout_ = duration<double>(0.0);
        uint32_t timeout_usec = 0;
        if (options_.get_value<uint32_t>("timeout-usec", timeout_usec))
        {
            cont_timeout_ += microseconds(timeout_usec);
        }
        uint32_t timeout_msec = 0;
        if (options_.get_value<uint32_t>("timeout-msec", timeout_msec))
        {
            cont_timeout_ += milliseconds(timeout_msec);
        }
        uint32_t timeout_sec = 0;
        if (options_.get_value<uint32_t>("timeout-sec", timeout_sec))
        {
            cont_timeout_ += seconds(timeout_sec);
        }
        uint32_t timeout_min = 0;
        if (options_.get_value<uint32_t>("timeout-min", timeout_min))
        {
            cont_timeout_ += minutes(timeout_min);
        }
        uint32_t timeout_hour = 0;
        if (options_.get_value<uint32_t>("timeout-hour", timeout_hour))
        {
            cont_timeout_ += hours(timeout_hour);
        }
    }

    options_.get_value<bool>("suppress-hdr", suppress_header_);
    options_.get_value<bool>("csv", csv_format_);

    return true;
}

bool nlb3::run()
{
    dma_buffer::ptr_t ice;
    dma_buffer::ptr_t inout; // shared workspace, if possible
    dma_buffer::ptr_t inp;   // input workspace
    dma_buffer::ptr_t out;   // output workspace

    // Three longs more are needed for num_reads, num_writes and complete signal
    std::size_t buf_size = CL(stride_acs_ * end_ + 1);  // size of input and output buffer (each)

    if (buf_size <= KB(2) || (buf_size > KB(4) && buf_size <= MB(1)) ||
                             (buf_size > MB(2) && buf_size < MB(512))) {  // split
        inout = accelerator_->allocate_buffer(buf_size * 2);
        if (!inout) {
            log_.error("nlb3_hls") << "failed to allocate input/output buffers." << std::endl;
            return false;
        }
        std::vector<dma_buffer::ptr_t> bufs = dma_buffer::split(inout, {buf_size, buf_size});
        inp = bufs[0];
        out = bufs[1];
    } else {
        inp = accelerator_->allocate_buffer(buf_size);
        out = accelerator_->allocate_buffer(buf_size);
        if (!inp || !out) {
            log_.error("nlb3_hls") << "failed to allocate input/output buffers." << std::endl;
            return false;
        }
    }

    if (!inp) {
        log_.error("nlb3_hls") << "failed to allocate input workspace." << std::endl;
        return false;
    }
    if (!out) {
        log_.error("nlb3_hls") << "failed to allocate output workspace." << std::endl;
        return false;
    }

    if (!accelerator_->reset())
    {
        log_.error("nlb3_hls") << "accelerator reset failed." << std::endl;
        return false;
    }

    uint64_t data = 0;
    if (print_) {
        if (!accelerator_->read_mmio64(static_cast<uint32_t>(AFU_DFH_REG), data)) {
            log_.error("nlb3_hls") << "slave register read failed" << std::endl;
            return false;
        }
        printf("AFU DFH REG = %08lx\n", data);

        if (!accelerator_->read_mmio64(static_cast<uint32_t>(AFU_ID_LO), data)) {
            log_.error("nlb3_hls") << "slave register read failed" << std::endl;
            return false;
        }
        printf("AFU ID LO = %08lx\n", data);

        if (!accelerator_->read_mmio64(static_cast<uint32_t>(AFU_ID_HI), data)) {
            log_.error("nlb3_hls") << "slave register read failed" << std::endl;
            return false;
        }
        printf("AFU ID HI = %08lx\n", data);

        if (!accelerator_->read_mmio64(static_cast<uint32_t>(AFU_NEXT), data)) {
            log_.error("nlb3_hls") << "slave register read failed" << std::endl;
            return false;
        }
        printf("AFU NEXT = %08lx\n", data);

        if (!accelerator_->read_mmio64(static_cast<uint32_t>(AFU_RESERVED), data)) {
            log_.error("nlb3_hls") << "slave register read failed" << std::endl;
            return false;
        }
        printf("AFU RESERVED = %08lx\n", data);
    }

    for (uint32_t i = begin_; i <= end_; i+=step_)
    {
        // Enabling interrupts
        accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::interrupt), data);
        PRINT("INTERRUPT SLAVE REG = %08lx\n", data);
        
        accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::interrupt), data | interrupt_hls);
        accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::interrupt), data);
        PRINT("INTERRUPT SLAVE REG = %08lx\n", data);

        // Matching masterRead and masterWrite with buffers
        accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::masterRead), data);
        PRINT("MASTER READ = %08lx\n", data);
        accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::masterWrite), data);
        PRINT("MASTER WRITE = %08lx\n", data);
        
        accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::masterRead), /*CACHELINE_ALIGNED_ADDR*/(inp->iova()));
        accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::masterWrite), /*ssCACHELINE_ALIGNED_ADDR*/(out->iova()));
        accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::masterRead), data);
        PRINT("MASTER READ = %016lx\n", data);
        accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::masterWrite), data);
        PRINT("MASTER WRITE = %016lx\n", data);

        // start bit set
        uint32_t dat = 0;
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), dat);
        PRINT("START SLAVE REG = %08x\n", dat);
        
        accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), dat | start_value_int);
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), dat);
        PRINT("START SLAVE REG = %08x\n", dat);

        // set the test mode
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::cfg), dat);
        PRINT("CFG = %08x\n", dat);
        accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::cfg), cfg_.value());
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::cfg), dat);
        PRINT("CFG = %08x\n", dat);

        // set the stride value
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::strided_acs), dat);
        PRINT("STRIDED ACS = %08x\n", dat);
        accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::strided_acs), num_strides_);
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::strided_acs), dat);
        PRINT("STRIDED ACS = %08x\n", dat);

        // reset buffers
        out->fill(0);
        inp->fill(0);

        // set number of cache lines for test
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::num_lines), dat);
        PRINT("NUM LINES = %08x\n", dat);
        accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::num_lines), i);
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::num_lines), dat);
        PRINT("NUM LINES = %08x\n", dat);
        
        // start the test
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::start), dat);
        PRINT("START = %08x\n", dat);

        // to measure latency
        auto tv_start = std::chrono::system_clock::now();
        
        accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::start), dat | start_value_hls);
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::start), dat);
        PRINT("START = %08x\n", dat);

        if (cont_)
        {
            std::this_thread::sleep_for(cont_timeout_);
            
            // Stop must be give on masterRead on all locations (only the first uint32_t needed)
            for (int i=0; i<DEFAULT_LINES; i++) {
                inp->write<uint32_t>(stop_value_int, i * LINE_INTS);
            }
            
            std::chrono::duration<int, std::ratio<1>> t(5);
            std::this_thread::sleep_for(t);
            if (!out->wait(BUF_SIZE_LONG, dma_buffer::microseconds_t(10),
                    dma_buffer::microseconds_t(1000), complete_value_int, complete_value_int)) {
                    log_.error("nlb3_hls")  << "test timeout at "
                                            << i << " cachelines." << std::endl;
                    return false;
            }
            
            auto tv_end = std::chrono::system_clock::now();
            std::chrono::duration<double, std::milli> elapsed = tv_end-tv_start;
            PRINT("AFU Latency: %0.5f milliseconds\n", elapsed.count());

            // checking and resetting done signal
            uint64_t done_val;
            accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::done), done_val);
            PRINT("DONE = %08lx\n", done_val);
            
            // write on done interrupt register to reset
            accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::done), done_val | interrupt_status_hls);

            // test that complete bit is set
            if ((done_val & done_value_hls) != done_value_hls) {
                log_.error("nlb3_hls") << "test timeout at " << i << " cachelines." << std::endl;
                return false;
            }
            accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::done), done_val);
            PRINT("DONE = %08lx\n", done_val);

            // read return value only for completeness
            uint64_t ret;
            accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::retrn), ret);
            PRINT("RETURN = %08lx\n", ret);

            if (print_) {
                printf("\nInput after ->\t[...,\n\t\t %u, ", inp->read<uint32_t>(BUF_SIZE_INT - (LINE_INTS * 3)));
                for (int i=(BUF_SIZE_INT - (LINE_INTS * 3) + 1); i < BUF_SIZE_INT; i++) {
                    if (i == (BUF_SIZE_INT - 1))
                        printf("%u]\n", inp->read<uint32_t>(i));
                    else if ((i % LINE_INTS) == (LINE_INTS - 1))
                        printf("%u\n\t\t ", inp->read<uint32_t>(i));
                    else
                        printf("%u, ", inp->read<uint32_t>(i));
                }

                printf("\nOutput after -> [...,\n\t\t %lu, ", out->read<uint64_t>(BUF_SIZE_LONG - (LINE_LONGS * 4)));
                for (int i = (BUF_SIZE_LONG - (LINE_LONGS * 4) + 1); i < (BUF_SIZE_LONG + LINE_LONGS); i++) {
                    if (i == (BUF_SIZE_LONG + LINE_LONGS - 1))
                        printf("%lu]\n", out->read<uint64_t>(i));
                    else if ((i % LINE_LONGS) == (LINE_LONGS - 1))
                        printf("%lu\n\t\t ", out->read<uint64_t>(i));
                    else
                        printf("%lu, ", out->read<uint64_t>(i));
                }
            }
        }
        else
        {
            log_.error("nlb3_hls") << "this version doesn't support non continuous mode" << std::endl;
            return false;
        }

        cachelines_ += i;

        // if we don't suppress stats then we show them at the end of each iteration
        if (!suppress_stats_)
        {
            std::cout << intel::fpga::nlb::nlb_stats(out,
                                                     i,
                                                     cont_,
                                                     suppress_header_,
                                                     csv_format_);
        }
    }
    
    return true;
}

void nlb3::show_help(std::ostream &os)
{
    os << "Usage: fpgadiag --mode {read,write,trput} [options]:" << std::endl
       << std::endl;

    for (const auto &it : options_)
    {
        it->show_help(os);
    }
}

} // end of namespace diag
} // end of namespace fpga
} // end of namespace intel
