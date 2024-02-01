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
#include "nlb_cache_prime_hls.h"
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
, afu_id_("F7DF405C-BD7A-CF72-22F1-44B0B93ACD18")//("5FA7FD4B-867C-484C-9440-28430B016F3D")
// , dsm_size_(MB(2))
, stride_acs_(1)
, num_strides_(0)
, step_(1)
, begin_(1)
, end_(1)
, frequency_(DEFAULT_FREQ)
, cont_(false)
, suppress_header_(false)
, csv_format_(false)
, suppress_stats_(false)
// , dsm_timeout_(FPGA_DSM_TIMEOUT)
, cachelines_(0)
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
    // options_.add_option<bool>("warm-fpga-cache",     'H', option::no_argument,   "Attempt to prime the cache with hits", false);
    // options_.add_option<bool>("cool-fpga-cache",     'M', option::no_argument,   "Attempt to prime the cache with misses", false);
    // options_.add_option<bool>("cool-cpu-cache",      'C', option::no_argument,   "Attempt to prime the cpu cache with misses", false);
    // options_.add_option<std::string>("cache-policy", 'p', option::with_argument, "one of {wrline-M, wrline-I, wrpush-I}", "wrline-M");
    // options_.add_option<std::string>("cache-hint",   'i', option::with_argument, "one of {rdline-I, rdline-S}", "rdline-I");
    // options_.add_option<std::string>("read-vc",      'r', option::with_argument, "one of {auto, vl0, vh0, vh1, random}", "auto");
    // options_.add_option<std::string>("write-vc",     'w', option::with_argument, "one of {auto, vl0, vh0, vh1, random}", "auto");
    // options_.add_option<std::string>("wrfence-vc",   'f', option::with_argument, "one of {auto, vl0, vh0, vh1}", "auto");
    // options_.add_option<bool>("alt-wr-pattern",      'l', option::no_argument,   "use alt wr pattern", false);
    // options_.add_option<uint64_t>("dsm-timeout-usec",     option::with_argument, "Timeout for test completion", dsm_timeout_.count());
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
    options_.add_option<uint32_t>("freq",            'T', option::with_argument, "Clock frequency (used for bw measurements)", frequency_);
    options_.add_option<bool>("suppress-hdr",             option::no_argument,   "Suppress column headers", suppress_header_);
    options_.add_option<bool>("csv",                 'V', option::no_argument,   "Comma separated value format", csv_format_);
    options_.add_option<bool>("suppress-stats",           option::no_argument,   "Show stas at end", suppress_stats_);
}

nlb3::~nlb3()
{
}

bool nlb3::setup()
{
    options_.get_value<std::string>("target", target_);
    options_.get_value<bool>("suppress-stats", suppress_stats_);
    // if (target_ == "fpga")
    // {
    //     dsm_timeout_ = FPGA_DSM_TIMEOUT;
    // }
    // else if (target_ == "ase")
    // {
    //     dsm_timeout_ = ASE_DSM_TIMEOUT;
    // }
    // else
    // {
    //     std::cerr << "Invalid --target: " << target_ << std::endl;
    //     return false;
    // }

    // uint64_t dsm_timeout_usec = 0;
    // if (options_.get_value<uint64_t>("dsm-timeout-usec", dsm_timeout_usec))
    // {
    //     dsm_timeout_ = microseconds(dsm_timeout_usec);
    // }

    // mode
    options_.get_value<std::string>("mode", mode_);
    if ((mode_ != "read") && (mode_ != "write") && (mode_ != "trput"))
    {
        std::cerr << "Invalid --mode: " << mode_ << std::endl;
        return false;
    }

    if (mode_ == "read")
    {
        cfg_ |= nlb_mode::read;
    }
    else if (mode_ == "write")
    {
        cfg_ |= nlb_mode::write;
    }
    else
    {
        cfg_ |= nlb_mode::throughput;
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
            log_.error("nlb3") << "multi-cl must be one of {1, 2, 4}" << std::endl;
            return false;
    }

    // stride distance 
    if (options_.get_value<uint32_t>("strided-access", stride_acs_))
    {
        if (stride_acs_ > 64)
        {
            log_.error("nlb3") << "strided access too big" << std::endl;
            return false;
        }
        num_strides_ = multi_cl * (stride_acs_ - 1);
    }

    // cache policy
    // std::string cache_policy;
    // options_.get_value<std::string>("cache-policy", cache_policy);
    // if (cache_policy == "wrline-M")
    // {
    //     cfg_ |= nlb3_cfg::wrline_m;
    // }
    // else if (cache_policy == "wrpush-I")
    // {
    //     cfg_ |= nlb3_cfg::wrpush_i;
    // }
    // else if (cache_policy == "wrline-I")
    // {
    //     cfg_ |= nlb3_cfg::wrline_i;
    // }
    // else
    // {
    //     log_.error("nlb3") << "cache-policy must be one of {wrline-M, wrline-I, wrpush-I}" << std::endl;
    //     return false;
    // }

    // cache hint
    // std::string cache_hint;
    // options_.get_value<std::string>("cache-hint", cache_hint);
    // if (cache_hint == "rdline-I")
    // {
    //     cfg_ |= nlb3_cfg::rdi;
    // }
    // else if (cache_hint == "rdline-S")
    // {
    //     cfg_ |= nlb3_cfg::rds;
    // }
    // else
    // {
    //     log_.error("nlb3") << "cache-hint must be one of {rdline-I, rdline-S}" << std::endl;
    //     return false;
    // }

    // read channel
    // std::string rd_channel;
    // options_.get_value<std::string>("read-vc", rd_channel);
    // if (rd_channel == "auto")
    // {
    //     cfg_ |= nlb3_cfg::va;
    // }
    // else if (rd_channel == "vl0")
    // {
    //     cfg_ |= nlb3_cfg::read_vl0;
    // }
    // else if (rd_channel == "vh0")
    // {
    //     cfg_ |= nlb3_cfg::read_vh0;
    // }
    // else if (rd_channel == "vh1")
    // {
    //     cfg_ |= nlb3_cfg::read_vh1;
    // }
    // else if (rd_channel == "random")
    // {
    //     cfg_ |= nlb3_cfg::read_vr;
    // }
    // else
    // {
    //     log_.error("nlb3") << "read-vc must be one of {auto, vl0, vh0, vh1, random}" << std::endl;
    //     return false;
    // }

    // write fence channel
    // std::string wrfence;
    // auto wrfrence_opt = options_.find("wrfence-vc");
    // options_.get_value<std::string>("wrfence-vc", wrfence);
    // if (wrfence == "auto")
    // {
    //     cfg_ |= nlb3_cfg::wrfence_va;
    // }
    // else if (wrfence == "vl0")
    // {
    //     cfg_ |= nlb3_cfg::wrfence_vl0;
    // }
    // else if (wrfence == "vh0")
    // {
    //     cfg_ |= nlb3_cfg::wrfence_vh0;
    // }
    // else if (wrfence == "vh1")
    // {
    //     cfg_ |= nlb3_cfg::wrfence_vh1;
    // }
    // else
    // {
    //     log_.error("nlb3") << "wrfence-vc must be one of {auto, vl0, vh0, vh1}" << std::endl;
    //     return false;
    // }

    // write channel
    // bool wrfence_set = wrfrence_opt && wrfrence_opt->is_set() && (wrfence != "auto");
    // std::string wr_channel;
    // options_.get_value<std::string>("write-vc", wr_channel);
    // if (wr_channel == "auto")
    // {
    //     cfg_ |= nlb3_cfg::va;
    // }
    // else if (wr_channel == "vl0")
    // {
    //     cfg_ |= nlb3_cfg::write_vl0;
    //     if (!wrfence_set)
    //     {
    //         cfg_ |= nlb3_cfg::wrfence_vl0;
    //     }
    // }
    // else if (wr_channel == "vh0")
    // {
    //     cfg_ |= nlb3_cfg::write_vh0;
    //     if (!wrfence_set)
    //     {
    //         cfg_ |= nlb3_cfg::wrfence_vh0;
    //     }
    // }
    // else if (wr_channel == "vh1")
    // {
    //     cfg_ |= nlb3_cfg::write_vh1;
    //     if (!wrfence_set)
    //     {
    //         cfg_ |= nlb3_cfg::wrfence_vh1;
    //     }
    // }
    // else if (wr_channel == "random")
    // {
    //     cfg_ |= nlb3_cfg::write_vr;
    // }
    // else
    // {
    //     log_.error("nlb3") << "write-vc must be one of {auto, vl0, vh0, vh1, random}" << std::endl;
    //     return false;
    // }

    // alt-wr-pattern
    // auto alt_wr_pattern_opt = options_.find("alt-wr-pattern");
    // if (alt_wr_pattern_opt && alt_wr_pattern_opt->value<bool>())
    // {
    //     cfg_ |= nlb3_cfg::alt_wr_prn;
    // }

    // begin, end
    options_.get_value<uint32_t>("begin", begin_);
    options_.get_value<uint32_t>("end", end_);
    auto end_opt = options_.find("end");

    if (begin_ > MAX_CL)
    {
        log_.error("nlb3") << "begin: " << begin_ << " is greater than max: " << MAX_CL << std::endl;
        return false;
    }

    if (begin_ % step_ > 0)
    {
        log_.error("nlb3") << "begin: " << begin_ << " is not a multiple of mcl: " << step_ << std::endl;
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
            log_.warn("nlb3") << "end: " << end_ << " is less than begin: " << begin_ << std::endl;
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

    options_.get_value<uint32_t>("freq", frequency_);
    options_.get_value<bool>("suppress-hdr", suppress_header_);
    options_.get_value<bool>("csv", csv_format_);

    // MOD * status2 register doesn't exist
    // TODO: Infer pclock from the device id
    // For now, get the pclock frequency from status2 register
    // that frequency (MHz) is encoded in bits [47:32]
    // uint64_t s2 = 0;
    // if (accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::status2), s2)){
    //   uint32_t freq = (s2 >> 32) & 0xffff;
    //   if (freq > 0){
    //     // frequency_ is in Hz
    //     frequency_ = freq * 1E6;
    //   }
    // }
    // * MOD

    // MOD * dsm is allocated at the end of out buffer
    // FIXME: use actual size for dsm size
    // dsm_ = accelerator_->allocate_buffer(dsm_size_);
    // if (!dsm_) {
    //     log_.error("nlb3") << "failed to allocate DSM workspace." << std::endl;
    //     return false;
    // }
    // * MOD
    return true;
}

bool nlb3::run()
{
    dma_buffer::ptr_t ice;
    dma_buffer::ptr_t inout; // shared workspace, if possible
    dma_buffer::ptr_t inp;   // input workspace
    dma_buffer::ptr_t out;   // output workspace

    // MOD * three longs more are needed for num_reads, num_writes and complete signal
    std::size_t buf_size = CL(stride_acs_ * end_ + 1);  // size of input and output buffer (each)
    // * MOD

    // Allocate the smallest possible workspaces for DSM, Input and Output
    // buffers.
    // ice = accelerator_->allocate_buffer(static_cast<size_t>
    //                             (nlb_cache_cool::fpga_cache_cool_size));
    // if (!ice) {
    //     log_.error("nlb3") << "failed to allocate ICE workspace." << std::endl;
    //     return false;
    // }

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

    // const uint32_t read_data = 0xc0cac01a;

    // dsm_->fill(0);
    // MOD * not needed
    // inp->fill(read_data);
    // out->fill(0);
    // * MOD

    if (!accelerator_->reset())
    {
        log_.error("nlb3") << "accelerator reset failed." << std::endl;
        return false;
    }

    // // prime cache
    // bool do_cool_fpga = false;
    // options_.get_value<bool>("cool-fpga-cache", do_cool_fpga);
    // bool do_warm_fpga = false;
    // options_.get_value<bool>("warm-fpga-cache", do_warm_fpga);
    // bool do_read_warm_fpga  = do_warm_fpga &&
    //     (cfg_ & nlb_mode::read) &&
    //     ((cfg_ & nlb_mode::throughput) != static_cast<uint32_t>(nlb_mode::throughput));
    // bool do_write_warm_fpga = do_warm_fpga &&
    //     (cfg_ & nlb_mode::write) &&
    //     ((cfg_ & nlb_mode::throughput) != static_cast<uint32_t>(nlb_mode::throughput));
    // bool do_cool_cpu = false;
    // options_.get_value<bool>("cool-cpu-cache", do_cool_cpu);

    // if (do_cool_fpga)
    // {
    //     nlb_cache_cool cooler(target_, accelerator_, dsm_, ice);
    //     cooler.cool();
    // }

    // if (do_read_warm_fpga || do_write_warm_fpga)
    // {
    //     if (do_cool_fpga)
    //     {
    //         log_.warn("nlb3") << "cannot do cool-fpga-cache and warm-fpga-cache together" << std::endl;
    //     }
    //     else
    //     {
    //         if (do_read_warm_fpga)
    //         {
    //             nlb_read_cache_warm warmer(target_, accelerator_, dsm_, inp, out);
    //             warmer.warm();
    //         }

    //         if (do_write_warm_fpga)
    //         {
    //             nlb_write_cache_warm warmer(target_, accelerator_, dsm_, inp, out);
    //             warmer.warm();
    //         }
    //     }
    // }
    // std::vector<uint32_t> cpu_cool_buff(0);
    // if (do_cool_cpu)
    // {
    //     cpu_cool_buff.resize(max_cpu_cache_size/sizeof(uint32_t));
    //     uint32_t i = 0;
    //     for (auto & it : cpu_cool_buff)
    //     {
    //         it = i++;
    //     }
    // }
    uint64_t data = 0;
	if (!accelerator_->read_mmio64(static_cast<uint32_t>(AFU_DFH_REG), data)) {
        printf("Errore in lettura\n");
    }
	printf("AFU DFH REG = %08lx\n", data);

    if (!accelerator_->read_mmio64(static_cast<uint32_t>(AFU_ID_LO), data)) {
        printf("Errore in lettura\n");
    }
	printf("AFU ID LO = %08lx\n", data);

    if (!accelerator_->read_mmio64(static_cast<uint32_t>(AFU_ID_HI), data)) {
        printf("Errore in lettura\n");
    }
	printf("AFU ID HI = %08lx\n", data);

    if (!accelerator_->read_mmio64(static_cast<uint32_t>(AFU_NEXT), data)) {
        printf("Errore in lettura\n");
    }
	printf("AFU NEXT = %08lx\n", data);

    if (!accelerator_->read_mmio64(static_cast<uint32_t>(AFU_RESERVED), data)) {
        printf("Errore in lettura\n");
    }
	printf("AFU RESERVED = %08lx\n", data);

    // MOD * reset not needed, no dsm
    // // assert afu reset
    // accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), 0);
    // // de-assert afu reset
    // accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), 1);
    // // set dsm base, high then low
    // accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_dsm::basel), dsm_->iova());
    // * MOD

    // MOD * no src or dst register, but need to assign buffer addr to masterRead and masterWrite
    // // set input workspace address
    // accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::src_addr), CACHELINE_ALIGNED_ADDR(inp->iova()));
    // // set output workspace address
    // accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::dst_addr), CACHELINE_ALIGNED_ADDR(out->iova()));
    
    // Enabling interrupts
    accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::interrupt), data);
    printf("INTERRUPT SLAVE REG = %08lx\n", data);
    accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::interrupt), data | interrupt_hls);
    accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::interrupt), data);
    printf("INTERRUPT SLAVE REG = %08lx\n", data);

    // Matching masterRead and masterWrite with buffers
    accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::masterRead), data);
    printf("MASTER READ = %08lx\n", data);
    accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::masterWrite), data);
    printf("MASTER WRITE = %08lx\n", data);
    accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::masterRead), /*CACHELINE_ALIGNED_ADDR*/(inp->iova()));
    accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::masterWrite), /*CACHELINE_ALIGNED_ADDR*/(out->iova()));
    accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::masterRead), data);
    printf("MASTER READ = %08lx\n", data);
    accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::masterWrite), data);
    printf("MASTER WRITE = %08lx\n", data);

    // start bit set
    uint32_t dat;
    accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), dat);
    printf("START SLAVE REG = %08x\n", dat);
    accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), dat | start_value_int);
    accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), dat);
    printf("START SLAVE REG = %08x\n", dat);

    // * MOD

    // set the test mode
    accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::cfg), dat);
    printf("CFG = %08x\n", dat);
    accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::cfg), cfg_.value());
    accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::cfg), dat);
    printf("CFG = %08x\n", dat);

    // set the stride value
    accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::strided_acs), dat);
    printf("STRIDED ACS = %08x\n", dat);
    accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::strided_acs), num_strides_);
    accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::strided_acs), dat);
    printf("STRIDED ACS = %08x\n", dat);


    // dsm_tuple dsm_tpl;
    // run tests
    for (uint32_t i = begin_; i <= end_; i+=step_)
    {
        // dsm_->fill(0);
        out->fill(0);
        // MOD * input values cannot be = 3 (stop value)
        inp->fill(0);
        
        // printf("\nInput before ->\t[...,\n\t\t\t %x, ", inp->read<uint32_t>(BUF_SIZE_INT - (LINE_INTS * 3)));
        // for (int i=(BUF_SIZE_INT - (LINE_INTS * 3) + 1); i < BUF_SIZE_INT; i++) {
        //     if (i == (BUF_SIZE_INT - 1))
        //         printf("%x]\n", inp->read<uint32_t>(i));
        //     else if ((i % LINE_INTS) == (LINE_INTS - 1))
        //         printf("%x\n\t\t\t ", inp->read<uint32_t>(i));
        //     else
        //         printf("%x, ", inp->read<uint32_t>(i));
        // }

        // printf("\nOutput before ->\t[...,\n\t\t %lx, ", out->read<uint64_t>(BUF_SIZE_LONG - (LINE_LONGS * 3)));
        // for (int i = (BUF_SIZE_LONG - (LINE_LONGS * 3) + 1); i < (BUF_SIZE_LONG + LINE_LONGS); i++) {
        //     if (i == (BUF_SIZE_LONG + LINE_LONGS - 1))
        //         printf("%lx]\n", out->read<uint64_t>(i));
        //     else if ((i % LINE_LONGS) == (LINE_LONGS - 1))
        //         printf("%lx\n\t\t\t ", out->read<uint64_t>(i));
        //     else
        //         printf("%lx, ", out->read<uint64_t>(i));
        // }

        // * MOD

        // MOD * Reset will not work asynchronously
        // // assert afu reset
        // accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), 0);
        // // de-assert afu reset
        // accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), 1);
        // * MOD

        // set number of cache lines for test
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::num_lines), dat);
        printf("NUM LINES = %08x\n", dat);
        accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::num_lines), i);
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::num_lines), dat);
        printf("NUM LINES = %08x\n", dat);

        // Read perf counters.
        fpga_cache_counters  start_cache_ctrs ;
        fpga_fabric_counters start_fabric_ctrs;
        if (!suppress_stats_)
        {
            start_cache_ctrs  = accelerator_->cache_counters();
            start_fabric_ctrs = accelerator_->fabric_counters();
        }
        // MOD * start signal must be given on a different offset
        // start the test
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::start), dat);
        printf("START = %08x\n", dat);

        auto tv_start = std::chrono::system_clock::now();
        accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::start), dat | start_value_hls);
        accelerator_->read_mmio32(static_cast<uint32_t>(nlb3_csr::start), dat);
        printf("START = %08x\n", dat);

        // * MOD

        if (cont_)
        {
            std::this_thread::sleep_for(cont_timeout_);
            // MOD * stop must be give on masterRead on all loactions
            // stop the device
            //accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), 7);
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
            printf("AFU Latency: %0.5f milliseconds\n", elapsed.count());

            uint64_t done_val;
            accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::done), done_val);
            printf("DONE = %08lx\n", done_val);
            // write on done interrupt register to reset
            accelerator_->write_mmio64(static_cast<uint32_t>(nlb3_csr::done), done_val | interrupt_status_hls);

            // Test that complete bit is set
            if ((done_val & done_value_hls) != done_value_hls) {
                log_.error("nlb3_hls") << "test timeout at " << i << " cachelines." << std::endl;
                return false;
            }
            accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::done), done_val);
            printf("DONE = %08lx\n", done_val);

            // read return value only for completeness
            uint64_t ret;
            accelerator_->read_mmio64(static_cast<uint32_t>(nlb3_csr::retrn), ret);
            printf("RETURN = %08lx\n", ret);
            // if (!dsm_->wait(static_cast<size_t>(nlb3_dsm::test_complete),
            //             dma_buffer::microseconds_t(10), dsm_timeout_, 0x1, 1))
            // {
            //     log_.error("nlb3") << "test timeout at "
            //                        << i << " cachelines." << std::endl;
            //     return false;
            // }
            // * MOD

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
        else
        {
            // MOD *
            // if (!dsm_->wait(static_cast<size_t>(nlb3_dsm::test_complete),
            //             dma_buffer::microseconds_t(10), dsm_timeout_, 0x1, 1))
            // {
            //     log_.error("nlb3") << "test timeout at "
            //                        << i << " cachelines." << std::endl;
            //     return false;
            // }
            // // stop the device
            // accelerator_->write_mmio32(static_cast<uint32_t>(nlb3_csr::ctl), 7);
            // * MOD
            printf("No continuous mode provided\n");
        }
        cachelines_ += i;
        // if we don't suppress stats then we show them at the end of each iteration
        if (!suppress_stats_)
        {
            // Read Perf Counters
            fpga_cache_counters  end_cache_ctrs  = accelerator_->cache_counters();
            fpga_fabric_counters end_fabric_ctrs = accelerator_->fabric_counters();
            std::cout << intel::fpga::nlb::nlb_stats(out,
                                                     i,
                                                     end_cache_ctrs - start_cache_ctrs,
                                                     end_fabric_ctrs - start_fabric_ctrs,
                                                     frequency_,
                                                     cont_,
                                                     suppress_header_,
                                                     csv_format_);
        }
        else
        {
            // if we suppress stats, add the current dsm stats to the rolling tuple
            // dsm_tpl += dsm_tuple(dsm_);
        }
    }
    // dsm_tpl.put(dsm_);

    // dsm_.reset();
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
