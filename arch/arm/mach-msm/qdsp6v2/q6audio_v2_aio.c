/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
	case RESET_EVENTS:
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
	case RESET_EVENTS:
		pr_debug("%s: Received opcode:0x%x\n", __func__, opcode);
		audio->stopped = 1;
		wake_up(&audio->event_wait);
		break;
	default:
		break;
	}
}

void extract_meta_out_info(struct q6audio_aio *audio,
		struct audio_aio_buffer_node *buf_node, int dir)
{
	struct dec_meta_out *meta_data = buf_node->kvaddr;
	uint32_t temp;

	if (dir) { /* input buffer - Write */
		if (audio->buf_cfg.meta_info_enable)
			memcpy(&buf_node->meta_info.meta_in,
			(char *)buf_node->kvaddr, sizeof(struct dec_meta_in));
		else
			memset(&buf_node->meta_info.meta_in,
			0, sizeof(struct dec_meta_in));
		pr_debug("%s[%p]:i/p: msw_ts 0x%lx lsw_ts 0x%lx nflags 0x%8x\n",
			__func__, audio,
			buf_node->meta_info.meta_in.ntimestamp.highpart,
			buf_node->meta_info.meta_in.ntimestamp.lowpart,
			buf_node->meta_info.meta_in.nflags);
	} else { /* output buffer - Read */
		memcpy((char *)buf_node->kvaddr,
			&buf_node->meta_info.meta_out,
			sizeof(struct dec_meta_out));
		meta_data->meta_out_dsp[0].nflags = 0x00000000;
		temp = meta_data->meta_out_dsp[0].msw_ts;
		meta_data->meta_out_dsp[0].msw_ts =
				meta_data->meta_out_dsp[0].lsw_ts;
		meta_data->meta_out_dsp[0].lsw_ts = temp;

		pr_debug("%s[%p]:o/p: msw_ts 0x%8x lsw_ts 0x%8x nflags 0x%8x, num_frames = %d\n",
		__func__, audio,
		((struct dec_meta_out *)buf_node->kvaddr)->\
			meta_out_dsp[0].msw_ts,
		((struct dec_meta_out *)buf_node->kvaddr)->\
			meta_out_dsp[0].lsw_ts,
		((struct dec_meta_out *)buf_node->kvaddr)->\
			meta_out_dsp[0].nflags,
		((struct dec_meta_out *)buf_node->kvaddr)->num_of_frames);
	}
}

/* Read buffer from DSP / Handle Ack from DSP */
void audio_aio_async_read_ack(struct q6audio_aio *audio, uint32_t token,
			uint32_t *payload)
{
	unsigned long flags;
	union msm_audio_event_payload event_payload;
	struct audio_aio_buffer_node *filled_buf;
	pr_debug("%s\n", __func__);

	/* No active flush in progress */
	if (audio->rflush)
		return;

	/* Statistics of read */
	atomic_add(payload[4], &audio->in_bytes);
	atomic_add(payload[9], &audio->in_samples);

	spin_lock_irqsave(&audio->dsp_lock, flags);
	if (list_empty(&audio->in_queue)) {
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		pr_warning("%s unexpected ack from dsp\n", __func__);
		return;
	}
	filled_buf = list_first_entry(&audio->in_queue,
					struct audio_aio_buffer_node, list);

	pr_debug("%s token: 0x[%d], filled_buf->token: 0x[%lu]",
				 __func__, token, filled_buf->token);
	if (token == (filled_buf->token)) {
		list_del(&filled_buf->list);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		event_payload.aio_buf = filled_buf->buf;
		/* Read done Buffer due to flush/normal condition
		after EOS event, so append EOS buffer */
		if (audio->eos_rsp == 0x1) {
			event_payload.aio_buf.data_len =
			insert_eos_buf(audio, filled_buf);
			/* Reset flag back to indicate eos intimated */
			audio->eos_rsp = 0;
		} else {
			filled_buf->meta_info.meta_out.num_of_frames\
							 = payload[9];
			event_payload.aio_buf.data_len = payload[4]\
				 + payload[5] + sizeof(struct dec_meta_out);
			pr_debug("%s[%p]:nr of frames 0x%8x len=%d\n",
				__func__, audio,
				filled_buf->meta_info.meta_out.num_of_frames,
				event_payload.aio_buf.data_len);
			extract_meta_out_info(audio, filled_buf, 0);
			audio->eos_rsp = 0;
		}
		pr_debug("%s, posting read done to the app here\n", __func__);
		audio_aio_post_event(audio, AUDIO_EVENT_READ_DONE,
					event_payload);
		kfree(filled_buf);
	} else {
		pr_err("%s[%p]:expected=%lx ret=%x\n",
			__func__, audio, filled_buf->token, token);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
	}
}
