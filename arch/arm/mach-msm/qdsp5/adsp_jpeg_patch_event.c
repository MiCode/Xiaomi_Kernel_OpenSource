/* arch/arm/mach-msm/qdsp5/adsp_jpeg_patch_event.c
 *
 * Verification code for aDSP JPEG events.
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2008-2009, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <mach/qdsp5/qdsp5jpegmsg.h>
#include "adsp.h"

int adsp_jpeg_patch_event(struct msm_adsp_module *module,
			struct adsp_event *event)
{
	if (event->msg_id == JPEG_MSG_ENC_OP_PRODUCED) {
		jpeg_msg_enc_op_produced *op = (jpeg_msg_enc_op_produced *)event->data.msg16;
		return adsp_pmem_paddr_fixup(module, (void **)&op->op_buf_addr);
	}
	if (event->msg_id == JPEG_MSG_DEC_OP_PRODUCED) {
		jpeg_msg_dec_op_produced *op = (jpeg_msg_dec_op_produced *)
			event->data.msg16;
		return adsp_pmem_paddr_fixup(module,
				(void **)&op->luma_op_buf_addr) ||
			adsp_pmem_paddr_fixup(module,
				(void **)&op->chroma_op_buf_addr);
	}

	return 0;
}
