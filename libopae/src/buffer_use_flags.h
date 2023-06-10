#ifndef _BUFFER_USE_FLAGS_H
#define _UFFER_USE_FLAGS

#include "opae/access.h"
#include "common_int.h"

static fpga_result buffer_allocate_use_flags(void **addr, uint64_t len, int flags);

fpga_result __FPGA_API__ fpgaPrepareBuffer_use_flags(fpga_handle handle, uint64_t len,
					   void **buf_addr, uint64_t *wsid,
					   int flags);

#endif