/* Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2008-2010, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/debug_mm.h>

#include "msm7kv2-pcm.h"

/* Audrec Queue command sent macro's */
#define audrec_send_bitstreamqueue(audio, cmd, len) \
	msm_adsp_write(audio->audrec, ((audio->queue_id & 0xFFFF0000) >> 16),\
		cmd, len)

#define audrec_send_audrecqueue(audio, cmd, len) \
	msm_adsp_write(audio->audrec, (audio->queue_id & 0x0000FFFF),\
		cmd, len)

static int alsa_dsp_read_buffer(struct msm_audio *audio,
			uint32_t read_cnt);
static void alsa_get_dsp_frames(struct msm_audio *prtd);
static int alsa_in_param_config(struct msm_audio *audio);

static int alsa_in_mem_config(struct msm_audio *audio);
static int alsa_in_enc_config(struct msm_audio *audio, int enable);

int intcnt;
struct audio_frame {
	uint16_t count_low;
	uint16_t count_high;
	uint16_t bytes;
	uint16_t unknown;
	unsigned char samples[];
} __attribute__ ((packed));

void alsa_dsp_event(void *data, unsigned id, uint16_t *msg)
{
	struct msm_audio *prtd = data;
	struct buffer *frame;
	unsigned long flag = 0;

	MM_DBG("\n");
	switch (id) {
	case AUDPP_MSG_HOST_PCM_INTF_MSG: {
		unsigned id = msg[3];
		unsigned idx = msg[4] - 1;

		MM_DBG("HOST_PCM id %d idx %d\n", id, idx);
		if (id != AUDPP_MSG_HOSTPCM_ID_ARM_RX) {
			MM_ERR("bogus id\n");
			break;
		}
		if (idx > 1) {
			MM_ERR("bogus buffer idx\n");
			break;
		}

		/* Update with actual sent buffer size */
		if (prtd->out[idx].used != BUF_INVALID_LEN)
			prtd->pcm_irq_pos += prtd->out[idx].used;

		if (prtd->pcm_irq_pos > prtd->pcm_size)
			prtd->pcm_irq_pos = prtd->pcm_count;

		if (prtd->ops->playback)
			prtd->ops->playback(prtd);

		if (prtd->mmap_flag)
			break;

		spin_lock_irqsave(&the_locks.write_dsp_lock, flag);
		if (prtd->running) {
			prtd->out[idx].used = 0;
			frame = prtd->out + prtd->out_tail;
			if (frame->used) {
				alsa_dsp_send_buffer(
					prtd, prtd->out_tail, frame->used);
				/* Reset eos_ack flag to avoid stale
				 * PCMDMAMISS been considered
				 */
				prtd->eos_ack = 0;
				prtd->out_tail ^= 1;
			} else {
				prtd->out_needed++;
			}
			wake_up(&the_locks.write_wait);
		}
		spin_unlock_irqrestore(&the_locks.write_dsp_lock, flag);
		break;
	}
	case AUDPP_MSG_PCMDMAMISSED:
		MM_INFO("PCMDMAMISSED %d\n", msg[0]);
		prtd->eos_ack++;
		MM_DBG("PCMDMAMISSED Count per Buffer %d\n", prtd->eos_ack);
		wake_up(&the_locks.eos_wait);
		break;
	case AUDPP_MSG_CFG_MSG:
		if (msg[0] == AUDPP_MSG_ENA_ENA) {
			MM_DBG("CFG_MSG ENABLE\n");
			prtd->out_needed = 0;
			prtd->running = 1;
			audpp_dsp_set_vol_pan(prtd->session_id, &prtd->vol_pan,
					POPP);
			audpp_route_stream(prtd->session_id,
				msm_snddev_route_dec(prtd->session_id));
			audio_dsp_out_enable(prtd, 1);
		} else if (msg[0] == AUDPP_MSG_ENA_DIS) {
			MM_DBG("CFG_MSG DISABLE\n");
			prtd->running = 0;
		} else {
			MM_DBG("CFG_MSG %d?\n", msg[0]);
		}
		break;
	default:
		MM_DBG("UNKNOWN (%d)\n", id);
	}
}

static void audpreproc_dsp_event(void *data, unsigned id,  void *msg)
{
	struct msm_audio *prtd = data;

	switch (id) {
	case AUDPREPROC_ERROR_MSG: {
		struct audpreproc_err_msg *err_msg = msg;

		MM_ERR("ERROR_MSG: stream id %d err idx %d\n",
			err_msg->stream_id, err_msg->aud_preproc_err_idx);
		/* Error case */
		break;
	}
	case AUDPREPROC_CMD_CFG_DONE_MSG: {
		MM_DBG("CMD_CFG_DONE_MSG\n");
		break;
	}
	case AUDPREPROC_CMD_ENC_CFG_DONE_MSG: {
		struct audpreproc_cmd_enc_cfg_done_msg *enc_cfg_msg = msg;

		MM_DBG("CMD_ENC_CFG_DONE_MSG: stream id %d enc type \
			0x%8x\n", enc_cfg_msg->stream_id,
			enc_cfg_msg->rec_enc_type);
		/* Encoder enable success */
		if (enc_cfg_msg->rec_enc_type & ENCODE_ENABLE)
			alsa_in_param_config(prtd);
		else { /* Encoder disable success */
			prtd->running = 0;
			alsa_in_record_config(prtd, 0);
		}
		break;
	}
	case AUDPREPROC_CMD_ENC_PARAM_CFG_DONE_MSG: {
		MM_DBG("CMD_ENC_PARAM_CFG_DONE_MSG\n");
		alsa_in_mem_config(prtd);
		break;
	}
	case AUDPREPROC_AFE_CMD_AUDIO_RECORD_CFG_DONE_MSG: {
		MM_DBG("AFE_CMD_AUDIO_RECORD_CFG_DONE_MSG\n");
		wake_up(&the_locks.enable_wait);
		break;
	}
	default:
		MM_DBG("Unknown Event id %d\n", id);
	}
}

static void audrec_dsp_event(void *data, unsigned id, size_t len,
		      void (*getevent) (void *ptr, size_t len))
{
	struct msm_audio *prtd = data;
	unsigned long flag = 0;

	switch (id) {
	case AUDREC_CMD_MEM_CFG_DONE_MSG: {
		MM_DBG("AUDREC_CMD_MEM_CFG_DONE_MSG\n");
		prtd->running = 1;
		alsa_in_record_config(prtd, 1);
		break;
	}
	case AUDREC_FATAL_ERR_MSG: {
		struct audrec_fatal_err_msg fatal_err_msg;

		getevent(&fatal_err_msg, AUDREC_FATAL_ERR_MSG_LEN);
		MM_ERR("FATAL_ERR_MSG: err id %d\n",
			fatal_err_msg.audrec_err_id);
		/* Error stop the encoder */
		prtd->stopped = 1;
		wake_up(&the_locks.read_wait);
		break;
	}
	case AUDREC_UP_PACKET_READY_MSG: {
		struct audrec_up_pkt_ready_msg pkt_ready_msg;
		MM_DBG("AUDREC_UP_PACKET_READY_MSG\n");

		getevent(&pkt_ready_msg, AUDREC_UP_PACKET_READY_MSG_LEN);
		MM_DBG("UP_PACKET_READY_MSG: write cnt lsw  %d \
			write cnt msw %d read cnt lsw %d  read cnt msw %d \n",\
			pkt_ready_msg.audrec_packet_write_cnt_lsw, \
			pkt_ready_msg.audrec_packet_write_cnt_msw, \
			pkt_ready_msg.audrec_up_prev_read_cnt_lsw, \
			pkt_ready_msg.audrec_up_prev_read_cnt_msw);

		alsa_get_dsp_frames(prtd);
		++intcnt;
		if (prtd->channel_mode == 1) {
			spin_lock_irqsave(&the_locks.read_dsp_lock, flag);
			if (prtd->pcm_irq_pos >= prtd->pcm_size)
				prtd->pcm_irq_pos = 0;
			spin_unlock_irqrestore(&the_locks.read_dsp_lock, flag);

			if (prtd->ops->capture)
				prtd->ops->capture(prtd);
		} else if ((prtd->channel_mode == 0) && (intcnt % 2 == 0)) {
			spin_lock_irqsave(&the_locks.read_dsp_lock, flag);
			if (prtd->pcm_irq_pos >= prtd->pcm_size)
				prtd->pcm_irq_pos = 0;
			spin_unlock_irqrestore(&the_locks.read_dsp_lock, flag);
			if (prtd->ops->capture)
				prtd->ops->capture(prtd);
		}
		break;
	}
	default:
		MM_DBG("Unknown Event id %d\n", id);
	}
}

struct msm_adsp_ops alsa_audrec_adsp_ops = {
	.event = audrec_dsp_event,
};

int alsa_audio_configure(struct msm_audio *prtd)
{
	if (prtd->enabled)
		return 0;

	MM_DBG("\n");
	if (prtd->dir == SNDRV_PCM_STREAM_PLAYBACK) {
		prtd->out_weight = 100;
		if (audpp_enable(-1, alsa_dsp_event, prtd)) {
			MM_ERR("audpp_enable() failed\n");
			return -ENODEV;
		}
	}
	if (prtd->dir == SNDRV_PCM_STREAM_CAPTURE) {
		if (audpreproc_enable(prtd->session_id,
				&audpreproc_dsp_event, prtd)) {
			MM_ERR("audpreproc_enable failed\n");
			return -ENODEV;
		}

		if (msm_adsp_enable(prtd->audrec)) {
			MM_ERR("msm_adsp_enable(audrec) enable failed\n");
			audpreproc_disable(prtd->session_id, prtd);
			return -ENODEV;
		}
		alsa_in_enc_config(prtd, 1);

	}
	prtd->enabled = 1;
	return 0;
}
EXPORT_SYMBOL(alsa_audio_configure);

ssize_t alsa_send_buffer(struct msm_audio *prtd, const char __user *buf,
			  size_t count, loff_t *pos)
{
	unsigned long flag = 0;
	const char __user *start = buf;
	struct buffer *frame;
	size_t xfer;
	int ret = 0;

	MM_DBG("\n");
	mutex_lock(&the_locks.write_lock);
	while (count > 0) {
		frame = prtd->out + prtd->out_head;
		ret = wait_event_interruptible(the_locks.write_wait,
					      (frame->used == 0)
					      || (prtd->stopped));
		if (ret < 0)
			break;
		if (prtd->stopped) {
			ret = -EBUSY;
			break;
		}
		xfer = count > frame->size ? frame->size : count;
		if (copy_from_user(frame->data, buf, xfer)) {
			ret = -EFAULT;
			break;
		}
		frame->used = xfer;
		prtd->out_head ^= 1;
		count -= xfer;
		buf += xfer;

		spin_lock_irqsave(&the_locks.write_dsp_lock, flag);
		frame = prtd->out + prtd->out_tail;
		if (frame->used && prtd->out_needed) {
			alsa_dsp_send_buffer(prtd, prtd->out_tail,
					      frame->used);
			/* Reset eos_ack flag to avoid stale
			 * PCMDMAMISS been considered
			 */
			prtd->eos_ack = 0;
			prtd->out_tail ^= 1;
			prtd->out_needed--;
		}
		spin_unlock_irqrestore(&the_locks.write_dsp_lock, flag);
	}
	mutex_unlock(&the_locks.write_lock);
	if (buf > start)
		return buf - start;
	return ret;
}
EXPORT_SYMBOL(alsa_send_buffer);

int alsa_audio_disable(struct msm_audio *prtd)
{
	if (prtd->enabled) {
		MM_DBG("\n");
		mutex_lock(&the_locks.lock);
		prtd->enabled = 0;
		audio_dsp_out_enable(prtd, 0);
		wake_up(&the_locks.write_wait);
		audpp_disable(-1, prtd);
		prtd->out_needed = 0;
		mutex_unlock(&the_locks.lock);
	}
	return 0;
}
EXPORT_SYMBOL(alsa_audio_disable);

int alsa_audrec_disable(struct msm_audio *prtd)
{
	if (prtd->enabled) {
		prtd->enabled = 0;
		alsa_in_enc_config(prtd, 0);
		wake_up(&the_locks.read_wait);
		msm_adsp_disable(prtd->audrec);
		prtd->out_needed = 0;
		audpreproc_disable(prtd->session_id, prtd);
	}
	return 0;
}
EXPORT_SYMBOL(alsa_audrec_disable);

static int alsa_in_enc_config(struct msm_audio *prtd, int enable)
{
	struct audpreproc_audrec_cmd_enc_cfg cmd;
	int i;
	unsigned short *ptrmem = (unsigned short *)&cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDPREPROC_AUDREC_CMD_ENC_CFG;
	cmd.stream_id = prtd->session_id;

	if (enable)
		cmd.audrec_enc_type = prtd->type | ENCODE_ENABLE;
	else
		cmd.audrec_enc_type &= ~(ENCODE_ENABLE);
	for (i = 0; i < sizeof(cmd)/2; i++, ++ptrmem)
		MM_DBG("cmd[%d]=0x%04x\n", i, *ptrmem);

	return audpreproc_send_audreccmdqueue(&cmd, sizeof(cmd));
}

static int alsa_in_param_config(struct msm_audio *prtd)
{
	struct audpreproc_audrec_cmd_parm_cfg_wav cmd;
	int i;
	unsigned short *ptrmem = (unsigned short *)&cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPREPROC_AUDREC_CMD_PARAM_CFG;
	cmd.common.stream_id = prtd->session_id;

	cmd.aud_rec_samplerate_idx = prtd->samp_rate;
	cmd.aud_rec_stereo_mode = prtd->channel_mode;
	for (i = 0; i < sizeof(cmd)/2; i++, ++ptrmem)
		MM_DBG("cmd[%d]=0x%04x\n", i, *ptrmem);

	return audpreproc_send_audreccmdqueue(&cmd, sizeof(cmd));
}

int alsa_in_record_config(struct msm_audio *prtd, int enable)
{
	struct audpreproc_afe_cmd_audio_record_cfg cmd;
	int i;
	unsigned short *ptrmem = (unsigned short *)&cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDPREPROC_AFE_CMD_AUDIO_RECORD_CFG;
	cmd.stream_id = prtd->session_id;
	if (enable)
		cmd.destination_activity = AUDIO_RECORDING_TURN_ON;
	else
		cmd.destination_activity = AUDIO_RECORDING_TURN_OFF;
	cmd.source_mix_mask = prtd->source;
	if (prtd->session_id == 2) {
		if ((cmd.source_mix_mask &
			INTERNAL_CODEC_TX_SOURCE_MIX_MASK) ||
			(cmd.source_mix_mask & AUX_CODEC_TX_SOURCE_MIX_MASK) ||
			(cmd.source_mix_mask & VOICE_UL_SOURCE_MIX_MASK) ||
			(cmd.source_mix_mask & VOICE_DL_SOURCE_MIX_MASK)) {
			cmd.pipe_id = SOURCE_PIPE_1;
		}
		if (cmd.source_mix_mask &
			AUDPP_A2DP_PIPE_SOURCE_MIX_MASK)
			cmd.pipe_id |= SOURCE_PIPE_0;
	}
	for (i = 0; i < sizeof(cmd)/2; i++, ++ptrmem)
		MM_DBG("cmd[%d]=0x%04x\n", i, *ptrmem);
	return audpreproc_send_audreccmdqueue(&cmd, sizeof(cmd));
}

static int alsa_in_mem_config(struct msm_audio *prtd)
{
	struct audrec_cmd_arecmem_cfg cmd;
	uint16_t *data = (void *) prtd->data;
	int n;
	int i;
	unsigned short *ptrmem = (unsigned short *)&cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_MEM_CFG_CMD;
	cmd.audrec_up_pkt_intm_count = 1;
	cmd.audrec_ext_pkt_start_addr_msw = prtd->phys >> 16;
	cmd.audrec_ext_pkt_start_addr_lsw = prtd->phys;
	cmd.audrec_ext_pkt_buf_number = FRAME_NUM;

	/* prepare buffer pointers:
	* Mono: 1024 samples + 4 halfword header
	* Stereo: 2048 samples + 4 halfword header
	*/
	for (n = 0; n < FRAME_NUM; n++) {
		prtd->in[n].data = data + 4;
		data += (4 + (prtd->channel_mode ? 2048 : 1024));
		MM_DBG("0x%8x\n", (int)(prtd->in[n].data - 8));
	}
	for (i = 0; i < sizeof(cmd)/2; i++, ++ptrmem)
		MM_DBG("cmd[%d]=0x%04x\n", i, *ptrmem);

	return audrec_send_audrecqueue(prtd, &cmd, sizeof(cmd));
}

int audio_dsp_out_enable(struct msm_audio *prtd, int yes)
{
	struct audpp_cmd_pcm_intf cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id	= AUDPP_CMD_PCM_INTF;
	cmd.stream	= AUDPP_CMD_POPP_STREAM;
	cmd.stream_id	= prtd->session_id;
	cmd.config	= AUDPP_CMD_PCM_INTF_CONFIG_CMD_V;
	cmd.intf_type	= AUDPP_CMD_PCM_INTF_RX_ENA_ARMTODSP_V;

	if (yes) {
		cmd.write_buf1LSW	= prtd->out[0].addr;
		cmd.write_buf1MSW	= prtd->out[0].addr >> 16;
		cmd.write_buf1_len	= prtd->out[0].size;
		cmd.write_buf2LSW	= prtd->out[1].addr;
		cmd.write_buf2MSW	= prtd->out[1].addr >> 16;
		if (prtd->out[1].used)
			cmd.write_buf2_len	= prtd->out[1].used;
		else
			cmd.write_buf2_len	= prtd->out[1].size;
		cmd.arm_to_rx_flag	= AUDPP_CMD_PCM_INTF_ENA_V;
		cmd.weight_decoder_to_rx = prtd->out_weight;
		cmd.weight_arm_to_rx	= 1;
		cmd.partition_number_arm_to_dsp = 0;
		cmd.sample_rate		= prtd->out_sample_rate;
		cmd.channel_mode	= prtd->out_channel_mode;
	}

	return audpp_send_queue2(&cmd, sizeof(cmd));
}

int alsa_buffer_read(struct msm_audio *prtd, void __user *buf,
		      size_t count, loff_t *pos)
{
	unsigned long flag;
	void *data;
	uint32_t index;
	uint32_t size;
	int ret = 0;

	mutex_lock(&the_locks.read_lock);
	while (count > 0) {
		ret = wait_event_interruptible(the_locks.read_wait,
					      (prtd->in_count > 0)
					      || prtd->stopped ||
						  prtd->abort);

		if (ret < 0)
			break;

		if (prtd->stopped) {
			ret = -EBUSY;
			break;
		}

		if (prtd->abort) {
			MM_DBG(" prtd->abort !\n");
			ret = -EPERM; /* Not permitted due to abort */
			break;
		}

		index = prtd->in_tail;
		data = (uint8_t *) prtd->in[index].data;
		size = prtd->in[index].size;
		if (count >= size) {
			if (copy_to_user(buf, data, size)) {
				ret = -EFAULT;
				break;
			}
			spin_lock_irqsave(&the_locks.read_dsp_lock, flag);
			if (index != prtd->in_tail) {
				/* overrun: data is invalid, we need to retry */
				spin_unlock_irqrestore(&the_locks.read_dsp_lock,
						       flag);
				continue;
			}
			prtd->in[index].size = 0;
			prtd->in_tail = (prtd->in_tail + 1) & (FRAME_NUM - 1);
			prtd->in_count--;
			spin_unlock_irqrestore(&the_locks.read_dsp_lock, flag);
			count -= size;
			buf += size;
		} else {
			break;
		}
	}
	mutex_unlock(&the_locks.read_lock);
	return ret;
}
EXPORT_SYMBOL(alsa_buffer_read);

int alsa_dsp_send_buffer(struct msm_audio *prtd,
					unsigned idx, unsigned len)
{
	struct audpp_cmd_pcm_intf_send_buffer cmd;
	int i;
	unsigned short *ptrmem = (unsigned short *)&cmd;

	cmd.cmd_id	= AUDPP_CMD_PCM_INTF;
	cmd.stream	= AUDPP_CMD_POPP_STREAM;
	cmd.stream_id	= prtd->session_id;
	cmd.config	= AUDPP_CMD_PCM_INTF_BUFFER_CMD_V;
	cmd.intf_type	= AUDPP_CMD_PCM_INTF_RX_ENA_ARMTODSP_V;
	cmd.dsp_to_arm_buf_id	= 0;
	cmd.arm_to_dsp_buf_id	= idx + 1;
	cmd.arm_to_dsp_buf_len	= len;
	for (i = 0; i < sizeof(cmd)/2; i++, ++ptrmem)
		MM_DBG("cmd[%d]=0x%04x\n", i, *ptrmem);

	return audpp_send_queue2(&cmd, sizeof(cmd));
}

static int alsa_dsp_read_buffer(struct msm_audio *audio, uint32_t read_cnt)
{
	struct up_audrec_packet_ext_ptr cmd;
	int i;
	unsigned short *ptrmem = (unsigned short *)&cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = UP_AUDREC_PACKET_EXT_PTR;
	cmd.audrec_up_curr_read_count_msw = read_cnt >> 16;
	cmd.audrec_up_curr_read_count_lsw = read_cnt;
	for (i = 0; i < sizeof(cmd)/2; i++, ++ptrmem)
		MM_DBG("cmd[%d]=0x%04x\n", i, *ptrmem);

	return audrec_send_bitstreamqueue(audio, &cmd, sizeof(cmd));
}

static void alsa_get_dsp_frames(struct msm_audio *prtd)
{
	struct audio_frame *frame;
	uint32_t index = 0;
	unsigned long flag;

	if (prtd->type == ENC_TYPE_WAV) {
		index = prtd->in_head;

		frame =
		    (void *)(((char *)prtd->in[index].data) - sizeof(*frame));

		spin_lock_irqsave(&the_locks.read_dsp_lock, flag);
		prtd->in[index].size = frame->bytes;
		MM_DBG("frame = %08x\n", (unsigned int) frame);
		MM_DBG("prtd->in[index].size = %08x\n",
				(unsigned int) prtd->in[index].size);

		prtd->in_head = (prtd->in_head + 1) & (FRAME_NUM - 1);

		/* If overflow, move the tail index foward. */
		if (prtd->in_head == prtd->in_tail)
			prtd->in_tail = (prtd->in_tail + 1) & (FRAME_NUM - 1);
		else
			prtd->in_count++;

		prtd->pcm_irq_pos += frame->bytes;
		alsa_dsp_read_buffer(prtd, prtd->dsp_cnt++);
		spin_unlock_irqrestore(&the_locks.read_dsp_lock, flag);

		wake_up(&the_locks.read_wait);
	}
}
