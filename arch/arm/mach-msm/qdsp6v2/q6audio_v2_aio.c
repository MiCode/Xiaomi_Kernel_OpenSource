/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include "audio_utils_aio.h"

void q6_audio_cb(uint32_t opcode, uint32_t token,
		uint32_t *payload, void *priv)
{
	struct q6audio_aio *audio = (struct q6audio_aio *)priv;

	pr_debug("%s:opcode = %x token = 0x%x\n", __func__, opcode, token);
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE_V2:
	case ASM_DATA_EVENT_READ_DONE_V2:
	case ASM_DATA_EVENT_RENDERED_EOS:
	case ASM_DATA_CMD_MEDIA_FMT_UPDATE_V2:
	case ASM_STREAM_CMD_SET_ENCDEC_PARAM:
	case ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY:
	case ASM_DATA_EVENT_ENC_SR_CM_CHANGE_NOTIFY:
		audio_aio_cb(opcode, token, payload, audio);
		break;
	default:
		pr_debug("%s:Unhandled event = 0x%8x\n", __func__, opcode);
		break;
	}
}

void audio_aio_cb(uint32_t opcode, uint32_t token,
		uint32_t *payload,  void *priv/*struct q6audio_aio *audio*/)
{
	struct q6audio_aio *audio = (struct q6audio_aio *)priv;
	union msm_audio_event_payload e_payload;

	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE_V2:
		pr_debug("%s[%p]:ASM_DATA_EVENT_WRITE_DONE token = 0x%x\n",
			__func__, audio, token);
		audio_aio_async_write_ack(audio, token, payload);
		break;
	case ASM_DATA_EVENT_READ_DONE_V2:
		pr_debug("%s[%p]:ASM_DATA_EVENT_READ_DONE token = 0x%x\n",
			__func__, audio, token);
		audio_aio_async_read_ack(audio, token, payload);
		break;
	case ASM_DATA_EVENT_RENDERED_EOS:
		/* EOS Handle */
		pr_debug("%s[%p]:ASM_DATA_CMDRSP_EOS\n", __func__, audio);
		if (audio->feedback) { /* Non-Tunnel mode */
			audio->eos_rsp = 1;
			/* propagate input EOS i/p buffer,
			after receiving DSP acknowledgement */
			if (audio->eos_flag &&
				(audio->eos_write_payload.aio_buf.buf_addr)) {
				audio_aio_post_event(audio,
						AUDIO_EVENT_WRITE_DONE,
						audio->eos_write_payload);
				memset(&audio->eos_write_payload , 0,
					sizeof(union msm_audio_event_payload));
				audio->eos_flag = 0;
			}
		} else { /* Tunnel mode */
			audio->eos_rsp = 1;
			wake_up(&audio->write_wait);
			wake_up(&audio->cmd_wait);
		}
		break;
	case ASM_DATA_CMD_MEDIA_FMT_UPDATE_V2:
	case ASM_STREAM_CMD_SET_ENCDEC_PARAM:
		pr_debug("%s[%p]:payload0[%x] payloa1d[%x]opcode= 0x%x\n",
			__func__, audio, payload[0], payload[1], opcode);
		break;
	case ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY:
	case ASM_DATA_EVENT_ENC_SR_CM_CHANGE_NOTIFY:

		pr_debug("%s[%p]: ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY, payload[0]-sr = %d, payload[1]-chl = %d, payload[2] = %d, payload[3] = %d\n",
					 __func__, audio, payload[0],
					 payload[1], payload[2], payload[3]);

		pr_debug("%s[%p]: ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY, sr(prev) = %d, chl(prev) = %d,",
				__func__, audio, audio->pcm_cfg.sample_rate,
				audio->pcm_cfg.channel_count);

		audio->pcm_cfg.sample_rate = payload[0];
		audio->pcm_cfg.channel_count = payload[1] & 0xFFFF;
		e_payload.stream_info.chan_info = audio->pcm_cfg.channel_count;
		e_payload.stream_info.sample_rate = audio->pcm_cfg.sample_rate;
		audio_aio_post_event(audio, AUDIO_EVENT_STREAM_INFO, e_payload);
		break;
	default:
		break;
	}
}
