#ifndef _BUFFER_SHARED_H
#define _BUFFER_SHARED_H

#include "opae/access.h"
#include "common_int.h"

fpga_result buffer_allocate_shared(void **addr, uint64_t len, int flags);

fpga_result fpgaPrepareBuffer_shared(fpga_handle handle, uint64_t len,
					   void **buf_addr, uint64_t *wsid,
					   int flags);

#endif