/*
* Support for Medfield PNW Camera Imaging ISP subsystem.
*
* Copyright (c) 2010 Intel Corporation. All Rights Reserved.
*
* Copyright (c) 2010 Silicon Hive www.siliconhive.com.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version
* 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
* 02110-1301, USA.
*
*/

#include "sh_css_firmware.h"

#include "sh_css_defs.h"
#include "sh_css_internal.h"
#include "sh_css_sp_start.h"

#include "memory_access.h"

#include "isp.h"				/* PMEM_WIDTH_LOG2 */

struct sh_css_fw_info	  sh_css_sp_fw;
struct sh_css_blob_descr *sh_css_blob_info; /* Only ISP blob info (no SP) */
unsigned		  sh_css_num_binaries; /* This includes 1 SP binary */

/*
 * Split the loaded firmware into blobs
 */

/* Setup sp binary */
static void
setup_sp(struct sh_css_fw_info *fw, const char *fw_data)
{
	const char  *blob_data = fw_data + fw->blob.offset;

	sh_css_sp_fw = *fw;
	sh_css_sp_fw.blob.text = blob_data + fw->blob.text_source;
	sh_css_sp_fw.blob.data = blob_data + fw->blob.data_source;
}

enum sh_css_err
sh_css_load_firmware(const char *fw_data,
		     unsigned int fw_size)
{
	unsigned i;
	struct sh_css_fw_info *binaries;
	struct sh_css_fw_bi_file_h *file_header;

	file_header = (struct sh_css_fw_bi_file_h *)fw_data;
	binaries = (struct sh_css_fw_info *)(&file_header[1]);

	/* some sanity checks */
	if (!fw_data || fw_size < sizeof(struct sh_css_fw_bi_file_h))
		return sh_css_err_internal_error;

	if (file_header->h_size != sizeof(struct sh_css_fw_bi_file_h))
		return sh_css_err_internal_error;

	sh_css_num_binaries = file_header->binary_nr;
	/* Only allocate memory for ISP blob info */
	sh_css_blob_info = sh_css_malloc((sh_css_num_binaries - 1) *
						sizeof(*sh_css_blob_info));
	if (sh_css_blob_info == NULL)
		return sh_css_err_cannot_allocate_memory;

	for (i = 0; i < sh_css_num_binaries; i++) {
		struct sh_css_fw_info *bi = &binaries[i];
		const char *name;

		name = (const char *)fw_data + bi->blob.prog_name_offset;

		/* sanity check */
		if (bi->blob.size != bi->blob.text_size + bi->blob.icache_size + bi->blob.data_size)
			return sh_css_err_internal_error;

		if (bi->blob.offset + bi->blob.size > fw_size)
			return sh_css_err_internal_error;

		if ((bi->blob.offset % (1UL<<(ISP_PMEM_WIDTH_LOG2-3))) != 0)
			return sh_css_err_internal_error;

		if (bi->type == sh_css_sp_firmware) {
			/* The first binary (i==0) is always the SP firmware */
			setup_sp(bi, fw_data);
		} else {
			/* All subsequent binaries (i>=1) are ISP firmware */
			const unsigned char *blob =
				(const unsigned char *)fw_data +
				bi->blob.offset;
			sh_css_blob_info[i-1].blob = blob;
			sh_css_blob_info[i-1].header = *bi;
			sh_css_blob_info[i-1].name = name;
		}
	}
	return sh_css_success;
}

void sh_css_unload_firmware(void)
{
	if (sh_css_blob_info) {
		sh_css_free(sh_css_blob_info);
		sh_css_blob_info = NULL;
	}
}

hrt_vaddress
sh_css_load_blob(const unsigned char *blob, unsigned size)
{
	hrt_vaddress target_addr = mmgr_malloc(size);
	/* this will allocate memory aligned to a DDR word boundary which
	   is required for the CSS DMA to read the instructions. */
	mmgr_store(target_addr, blob, size);
#if SH_CSS_PREVENT_UNINIT_READS
	{
		unsigned padded_size = CEIL_MUL(size, HIVE_ISP_DDR_WORD_BYTES);
		mmgr_clear(target_addr + size, padded_size - size);
	}
#endif
	return target_addr;
}

enum sh_css_err
sh_css_load_blob_info(const char *fw, struct sh_css_blob_descr *bd)
{
	const char *name;
	const unsigned char *blob;
	struct sh_css_fw_info *bi = (struct sh_css_fw_info *)fw;

	name = fw + sizeof(*bi);
	blob = (const unsigned char *)name + strlen(name)+1;

	/* sanity check */
	if (bi->header_size != sizeof(*bi))
		return sh_css_err_isp_binary_header_mismatch;
	if (bi->blob.size != bi->blob.text_size + bi->blob.icache_size + bi->blob.data_size)
		return sh_css_err_isp_binary_header_mismatch;

	bd->blob = blob;
	bd->header = *bi;
	bd->name = name;
	return sh_css_success;
}
