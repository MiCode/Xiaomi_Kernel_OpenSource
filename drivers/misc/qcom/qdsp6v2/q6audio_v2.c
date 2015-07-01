/* Copyright (c) 2012-2013, 2015, The Linux Foundation. All rights reserved.
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
#include <linux/atomic.h>
#include <asm/ioctls.h>
#include "audio_utils.h"

void q6asm_in_cb(uint32_t opcode, uint32_t token,
		uint32_t *payload, void *priv)
{
	struct q6audio_in *audio = (struct q6audio_in *)priv;
	unsigned long flags;

	pr_debug("%s:session id %d: opcode[0x%x]\n", __func__,
			audio->ac->session, opcode);

	spin_lock_irqsave(&audio->dsp_lock, flags);
	switch (opcode) {
	case ASM_DATA_EVENT_READ_DONE_V2:
		audio_in_get_dsp_frames(audio, token, payload);
		break;
	case ASM_DATA_EVENT_WRITE_DONE_V2:
		atomic_inc(&audio->in_count);
		wake_up(&audio->write_wait);
		break;
	case ASM_DATA_EVENT_RENDERED_EOS:
		audio->eos_rsp = 1;
		wake_up(&audio->read_wait);
		break;
	case ASM_STREAM_CMDRSP_GET_PP_PARAMS_V2:
		break;
	case ASM_SESSION_EVENTX_OVERFLOW:
		pr_err("%s:session id %d: ASM_SESSION_EVENT_TX_OVERFLOW\n",
			__func__, audio->ac->session);
		break;
	case RESET_EVENTS:
		pr_debug("%s:received RESET EVENTS\n", __func__);
		audio->enabled = 0;
		audio->stopped = 1;
		audio->event_abort = 1;
		audio->reset_event = true;
		wake_up(&audio->read_wait);
		wake_up(&audio->write_wait);
		break;
	default:
		pr_debug("%s:session id %d: Ignore opcode[0x%x]\n", __func__,
			audio->ac->session, opcode);
		break;
	}
	spin_unlock_irqrestore(&audio->dsp_lock, flags);
}

void  audio_in_get_dsp_frames(void *priv,
	uint32_t token,	uint32_t *payload)
{
	struct q6audio_in *audio = (struct q6audio_in *)priv;
	uint32_t index;

	index = token;
	pr_debug("%s:session id %d: index=%d nr frames=%d offset[%d]\n",
			__func__, audio->ac->session, token, payload[9],
			payload[5]);
	pr_debug("%s:session id %d: timemsw=%d lsw=%d\n", __func__,
			audio->ac->session, payload[7], payload[6]);
	pr_debug("%s:session id %d: uflags=0x%8x uid=0x%8x\n", __func__,
			audio->ac->session, payload[8], payload[10]);
	pr_debug("%s:session id %d: enc_framesotal_size=0x%8x\n", __func__,
			audio->ac->session, payload[4]);

	audio->out_frame_info[index][0] = payload[9];
	audio->out_frame_info[index][1] = payload[5];

	/* statistics of read */
	atomic_add(payload[4], &audio->in_bytes);
	atomic_add(payload[9], &audio->in_samples);

	if (atomic_read(&audio->out_count) <= audio->str_cfg.buffer_count) {
		atomic_inc(&audio->out_count);
		wake_up(&audio->read_wait);
	}
}
