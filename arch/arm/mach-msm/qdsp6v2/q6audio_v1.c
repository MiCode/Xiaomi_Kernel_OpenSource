/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <asm/atomic.h>
#include <asm/ioctls.h>
#include "audio_utils.h"

void q6asm_in_cb(uint32_t opcode, uint32_t token,
		uint32_t *payload, void *priv)
{
	struct q6audio_in * audio = (struct q6audio_in *)priv;
	unsigned long flags;

	pr_debug("%s:session id %d: opcode[0x%x]\n", __func__,
			audio->ac->session, opcode);

	spin_lock_irqsave(&audio->dsp_lock, flags);
	switch (opcode) {
	case ASM_DATA_EVENT_READ_DONE:
		audio_in_get_dsp_frames(audio, token, payload);
		break;
	case ASM_DATA_EVENT_WRITE_DONE:
		atomic_inc(&audio->in_count);
		wake_up(&audio->write_wait);
		break;
	case ASM_DATA_CMDRSP_EOS:
		audio->eos_rsp = 1;
		wake_up(&audio->read_wait);
		break;
	case ASM_STREAM_CMDRSP_GET_ENCDEC_PARAM:
		break;
	case ASM_STREAM_CMDRSP_GET_PP_PARAMS:
		break;
	case ASM_SESSION_EVENT_TX_OVERFLOW:
		pr_err("%s:session id %d: ASM_SESSION_EVENT_TX_OVERFLOW\n",
			__func__, audio->ac->session);
		break;
	default:
		pr_debug("%s:session id %d: Ignore opcode[0x%x]\n", __func__,
			audio->ac->session, opcode);
		break;
	}
	spin_unlock_irqrestore(&audio->dsp_lock, flags);
}

void  audio_in_get_dsp_frames(/*struct q6audio_in *audio,*/void *aud,
	uint32_t token,	uint32_t *payload)
{
	struct q6audio_in *audio = (struct q6audio_in *)aud;
	uint32_t index;

	index = token;
	pr_debug("%s:session id %d: index=%d nr frames=%d offset[%d]\n",
			__func__, audio->ac->session, token, payload[7],
			payload[3]);
	pr_debug("%s:session id %d: timemsw=%d lsw=%d\n", __func__,
			audio->ac->session, payload[4], payload[5]);
	pr_debug("%s:session id %d: uflags=0x%8x uid=0x%8x\n", __func__,
			audio->ac->session, payload[6], payload[8]);
	pr_debug("%s:session id %d: enc frame size=0x%8x\n", __func__,
			audio->ac->session, payload[2]);

	audio->out_frame_info[index][0] = payload[7];
	audio->out_frame_info[index][1] = payload[3];

	/* statistics of read */
	atomic_add(payload[2], &audio->in_bytes);
	atomic_add(payload[7], &audio->in_samples);

	if (atomic_read(&audio->out_count) <= audio->str_cfg.buffer_count) {
		atomic_inc(&audio->out_count);
		wake_up(&audio->read_wait);
	}
}
