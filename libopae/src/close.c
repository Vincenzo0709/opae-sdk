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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#include <opae/access.h>
#include "common_int.h"
#include "wsid_list_int.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void unmap_mmio_region(struct wsid_map *wm)
{
	if (munmap((void *)wm->offset, wm->len)) {
		FPGA_MSG("munmap failed: %s",
			 strerror(errno));
	}
}

fpga_result __FPGA_API__ fpgaClose(fpga_handle handle)
{
	struct _fpga_handle *_handle = (struct _fpga_handle *)handle;
	fpga_result result = FPGA_OK;
	int err = 0;

	result = handle_check_and_lock(_handle);
	if (result)
		return result;

	if (-1 == _handle->fddev) {
		FPGA_ERR("Invalid handle file descriptor");
		err = pthread_mutex_unlock(&_handle->lock);
		if (err) {
			FPGA_ERR("pthread_mutex_unlock() failed: %S", strerror(err));
		}
		return FPGA_INVALID_PARAM;
	}

	wsid_tracker_cleanup(_handle->wsid_root, NULL);
	wsid_tracker_cleanup(_handle->mmio_root, unmap_mmio_region);
	free_umsg_buffer(handle);
	close(_handle->fddev);
	if (_handle->fdfpgad >= 0)
		close(_handle->fdfpgad);

	// invalidate magic (just in case)
	_handle->magic = FPGA_INVALID_MAGIC;

	err = pthread_mutex_unlock(&_handle->lock);
	if (err) {
		FPGA_ERR("pthread_mutex_unlock() failed: %S", strerror(err));
	}
	err = pthread_mutex_destroy(&_handle->lock);
	if (err) {
		FPGA_ERR("pthread_mutex_unlock() failed: %S", strerror(err));
	}

	free(_handle);

	return FPGA_OK;
}
