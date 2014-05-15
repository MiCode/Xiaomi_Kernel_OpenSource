/*
 *  ipc_lib_v2.c - Intel MID Platform Driver IPC wrappers for mrfld
 *
 *  Copyright (C) 2013 Intel Corp
 *  Author: Lakshmi N Vinnakota <lakshmi.n.vinnakota@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *
 */
#include <sound/soc.h>
#include <asm/platform_sst_audio.h>
#include "../platform_ipc_v2.h"
#include "../sst_platform.h"
#include "../sst_platform_pvt.h"


static inline void sst_fill_dsp_hdr(struct ipc_dsp_hdr *hdr, u8 index, u8 pipe,
	u16 module, u16 cmd, u16 len)
{
	hdr->mod_index_id = index;
	hdr->pipe_id = pipe;
	hdr->mod_id = module;
	hdr->cmd_id = cmd;
	hdr->length = len;

}

static inline void sst_fill_byte_control_hdr(struct snd_sst_bytes_v2 *hdr,
	u8 type, u8 msg, u8 block, u8 task, u8 pipe, u16 len)
{
	hdr->type = type;
	hdr->ipc_msg = msg;
	hdr->block = block;
	hdr->task_id = task;
	hdr->pipe_id = pipe;
	hdr->rsvd = 0;
	hdr->len = len;
}

#define SST_GAIN_V2_TIME_CONST 50

void sst_create_compr_vol_ipc(char *bytes, unsigned int type,
	struct sst_algo_int_control_v2 *kdata)
{
	struct snd_sst_gain_v2 gain1;
	struct snd_sst_bytes_v2 byte_hdr;
	struct ipc_dsp_hdr dsp_hdr;
	char *tmp;
	u16 len;
	u8 ipc_msg;

	/* Fill gain params */
	gain1.gain_cell_num = 1;  /* num of gain cells to modify*/
	gain1.cell_nbr_idx = kdata->instance_id; /* instance index */
	gain1.cell_path_idx = kdata->pipe_id; /* pipe id */
	gain1.module_id = kdata->module_id; /*module id */
	gain1.left_cell_gain = kdata->value; /* left gain value in dB*/
	gain1.right_cell_gain = kdata->value; /* same value as left in dB*/
	/* set to default recommended value*/
	gain1.gain_time_const = SST_GAIN_V2_TIME_CONST;

	/* fill dsp header */
	/* Get params format for vol ctrl lib, size 6 bytes :
	 * u16 left_gain, u16 right_gain, u16 ramp
	 */
	memset(&dsp_hdr, 0, sizeof(dsp_hdr));
	if (type == SND_SST_BYTES_GET) {
		len = 6;
		ipc_msg = IPC_GET_PARAMS;
	} else {
		len = sizeof(gain1);
		ipc_msg = IPC_SET_PARAMS;
	}

	sst_fill_dsp_hdr(&dsp_hdr, 0, kdata->pipe_id, kdata->module_id,
				IPC_IA_SET_GAIN_MRFLD, len);

	/* fill byte control header */
	memset(&byte_hdr, 0, sizeof(byte_hdr));
	len = sizeof(dsp_hdr) + dsp_hdr.length;
	sst_fill_byte_control_hdr(&byte_hdr, type, ipc_msg, 1,
			SST_TASK_ID_MEDIA, kdata->pipe_id, len);

	/* fill complete byte stream as ipc payload */
	tmp = bytes;
	memcpy(tmp, &byte_hdr, sizeof(byte_hdr));
	memcpy((tmp + sizeof(byte_hdr)), &dsp_hdr, sizeof(dsp_hdr));
	if (type != SND_SST_BYTES_GET)
		memcpy((tmp + sizeof(byte_hdr) + sizeof(dsp_hdr)), &gain1,
			sizeof(gain1));
#ifdef DEBUG_HEX_DUMP_BYTES
	print_hex_dump_bytes(__func__, DUMP_PREFIX_NONE, bytes, 32);
#endif
}
