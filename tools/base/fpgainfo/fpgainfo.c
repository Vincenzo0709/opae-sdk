// Copyright(c) 2018, Intel Corporation
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

#define _GNU_SOURCE
#include <string.h>

#include "fpgainfo.h"
#include "bmcinfo.h"
#include "opae/fpga.h"
#include <inttypes.h>
#include <uuid/uuid.h>
#include "safe_string/safe_string.h"
#include <ctype.h>
#include <limits.h>

/*
 * Print readable error message for fpga_results
 */
void fpgainfo_print_err(const char *s, fpga_result res)
{
	if (s && res)
		fprintf(stderr, "Error %s: %s\n", s, fpgaErrStr(res));
}

void fpgainfo_print_common(const char *hdr, fpga_properties props)
{
	fpga_result res = FPGA_OK;
	char guid_str[38];
	uint64_t object_id = 0;
	uint8_t bus = 0;
	uint16_t segment = 0;
	uint8_t device = 0;
	uint8_t function = 0;
	uint16_t device_id = 0;
	uint8_t socket_id = 0;
	fpga_guid guid = {0};
	fpga_guid port_guid = {0};
	uint32_t num_slots = 0;
	uint64_t bbs_id = 0;
	fpga_version bbs_version = {0};
	fpga_objtype objtype = -1;
	fpga_properties pprops = props;
	fpga_token par = NULL;
	int is_accelerator = 0;
	bool has_parent = true;

	res = fpgaPropertiesGetObjectType(props, &objtype);
	fpgainfo_print_err("reading objtype from properties", res);

	res = fpgaPropertiesGetObjectID(props, &object_id);
	fpgainfo_print_err("reading object_id from properties", res);

	res = fpgaPropertiesGetBus(props, &bus);
	fpgainfo_print_err("reading bus from properties", res);

	res = fpgaPropertiesGetSegment(props, &segment);
	fpgainfo_print_err("reading segment from properties", res);

	res = fpgaPropertiesGetDevice(props, &device);
	fpgainfo_print_err("reading device from properties", res);

	res = fpgaPropertiesGetFunction(props, &function);
	fpgainfo_print_err("reading function from properties", res);

	res = fpgaPropertiesGetDeviceID(props, &device_id);
	fpgainfo_print_err("reading device_id from properties", res);

	if (objtype != FPGA_DEVICE) {
		res = fpgaPropertiesGetGUID(props, &port_guid);
		fpgainfo_print_err("reading guid from properties", res);
		is_accelerator = 1;
	}

	// Go up the tree until we find the device
	while (objtype != FPGA_DEVICE) {
		res = fpgaPropertiesGetParent(pprops, &par);
		if (FPGA_NOT_FOUND == res) {
			has_parent = false;
			break;
		}
		fpgainfo_print_err("reading objtype from properties", res);

		if (pprops != props) {
			res = fpgaDestroyProperties(&pprops);
			fpgainfo_print_err("destroying parent properties", res);
			pprops = props;
		}

		res = fpgaGetProperties(par, &pprops);
		fpgainfo_print_err("reading parent properties", res);

		res = fpgaPropertiesGetObjectType(pprops, &objtype);
		fpgainfo_print_err("reading objtype from properties", res);

		res = fpgaDestroyToken(&par);
		fpgainfo_print_err("destroying parent token", res);
	};

	res = fpgaPropertiesGetDeviceID(pprops, &device_id);
	fpgainfo_print_err("reading device_id from properties", res);

	res = fpgaPropertiesGetGUID(pprops, &guid);
	fpgainfo_print_err("reading guid from properties", res);

	if (objtype == FPGA_DEVICE) {
		res = fpgaPropertiesGetNumSlots(pprops, &num_slots);
		fpgainfo_print_err("reading num_slots from properties", res);

		res = fpgaPropertiesGetBBSID(pprops, &bbs_id);
		fpgainfo_print_err("reading bbs_id from properties", res);

		res = fpgaPropertiesGetSocketID(props, &socket_id);
		fpgainfo_print_err("reading socket_id from properties", res);

		res = fpgaPropertiesGetBBSVersion(pprops, &bbs_version);
		fpgainfo_print_err("reading bbs_version from properties", res);
	}

	// TODO: Implement once model and capabilities accessors are
	// implemented

	// res = fpgaPropertiesGetModel(props, &model);
	// fpgainfo_print_err("reading model from properties", res);

	// res = fpgaPropertiesGetCapabilities(props, &capabilities);
	// fpgainfo_print_err("reading capabilities from properties", res);

	if (pprops != props) {
		res = fpgaDestroyProperties(&pprops);
		fpgainfo_print_err("destroying parent properties after use",
				   res);
		pprops = props;
	}

	if (has_parent) {
		print_bmc_info(get_sysfs_path(pprops, FPGA_DEVICE, NULL));
	}

	printf("%s\n", hdr);
	printf("%-29s : 0x%2" PRIX64 "\n", "Object Id", object_id);
	printf("%-29s : %04X:%02X:%02X:%01X\n", "PCIe s:b:d:f", segment, bus,
	       device, function);
	printf("%-29s : 0x%04X\n", "Device Id", device_id);
	if (objtype == FPGA_DEVICE) {
		printf("%-29s : 0x%02X\n", "Socket Id", socket_id);
		printf("%-29s : %02d\n", "Ports Num", num_slots);
		printf("%-29s : 0x%" PRIX64 "\n", "Bitstream Id", bbs_id);
		printf("%-29s : 0x%lX\n", "Bitstream Version",
		       *(uint64_t *)&bbs_version);
	}

	if (objtype == FPGA_DEVICE) {
		uuid_unparse(guid, guid_str);
		printf("%-29s : %s\n", "Pr Interface Id", guid_str);
	}
	if (is_accelerator) {
		uuid_unparse(port_guid, guid_str);
		printf("%-29s : %s\n", "Accelerator Id", guid_str);
	}
}

// Replace occurrences of character within string
char *replace_chars(char *str, char match, char rep)
{
	char *tmp = strchr(str, match);
	while (tmp) {
		*tmp = rep;
		tmp = strchr(tmp + 1, match);
	}
	return str;
}

// Turn all "pcie" into "PCIe"
char *upcase_pci(char *str)
{
	char *tmp = strcasestr(str, "pci");
	while (tmp) {
		*tmp++ = 'P';
		*tmp++ = 'C';
		*tmp++ = 'I';
		tmp = strcasestr(tmp, "pci");
	}
	return str;
}

// Upper-case the first letter of each word in str
char *upcase_first(char *str)
{
	*str = toupper(*str);
	char *tmp = strchr(str + 1, ' ');
	while (tmp) {
		if (tmp[1] && isalpha(tmp[1])) {
			tmp[1] = toupper(tmp[1]);
		}
		tmp = strchr(tmp + 1, ' ');
	}
	return str;
}

// TODO: Move this to a common file for reuse in other fpgainfo files
int str_in_list(const char *key, const char *list[], size_t size)
{
	size_t i = 0;
	for (; i < size; ++i) {
		if (!strcmp(key, list[i])) {
			return (int)i;
		}
	}
	return INT_MAX;
}
