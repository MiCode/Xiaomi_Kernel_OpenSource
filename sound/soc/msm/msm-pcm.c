/* sound/soc/msm/msm-pcm.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2008-2009, 2012 The Linux Foundation. All rights reserved.
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

#include "msm-pcm.h"

#define MAX_DATA_SIZE 496
#define AUDPP_ALSA_DECODER	(-1)

#define DB_TABLE_INDEX		(50)

#define audio_send_queue_recbs(prtd, cmd, len) \
	msm_adsp_write(prtd->audrec, QDSP_uPAudRecBitStreamQueue, cmd, len)
#define audio_send_queue_rec(prtd, cmd, len) \
	msm_adsp_write(prtd->audrec, QDSP_uPAudRecCmdQueue, cmd, len)

int intcnt;

struct audio_frame {
	uint16_t count_low;
	uint16_t count_high;
	uint16_t bytes;
	uint16_t unknown;
	unsigned char samples[];
} __attribute__ ((packed));

/* Table contains dB to raw value mapping */
static const unsigned decoder_db_table[] = {

      31 , /* -50 dB */
      35 ,      39 ,      44 ,      50 ,      56 ,
      63 ,      70 ,      79 ,      89 ,      99 ,
     112 ,     125 ,     141 ,     158 ,     177 ,
     199 ,     223 ,     251 ,     281 ,     316 ,
     354 ,     398 ,     446 ,     501 ,     562 ,
     630 ,     707 ,     794 ,     891 ,     999 ,
    1122 ,    1258 ,    1412 ,    1584 ,    1778 ,
    1995 ,    2238 ,    2511 ,    2818 ,    3162 ,
    3548 ,    3981 ,    4466 ,    5011 ,    5623 ,
    6309 ,    7079 ,    7943 ,    8912 ,   10000 ,
   11220 ,   12589 ,   14125 ,   15848 ,   17782 ,
   19952 ,   22387 ,   25118 ,   28183 ,   31622 ,
   35481 ,   39810 ,   44668 ,   50118 ,   56234 ,
   63095 ,   70794 ,   79432 ,   89125 ,  100000 ,
  112201 ,  125892 ,  141253 ,  158489 ,  177827 ,
  199526 ,  223872 ,  251188 ,  281838 ,  316227 ,
  354813 ,  398107 ,  446683 ,  501187 ,  562341 ,
  630957 ,  707945 ,  794328 ,  891250 , 1000000 ,
 1122018 , 1258925 , 1412537 , 1584893 , 1778279 ,
 1995262 , 2238721 , 2511886 , 2818382 , 3162277 ,
 3548133   /*  51 dB */

};

static unsigned compute_db_raw(int db)
{
	unsigned reg_val = 0;        /* Computed result for correspondent db */
	/* Check if the given db is out of range */
	if (db <= MIN_DB)
		return 0;
	else if (db > MAX_DB)
		db = MAX_DB;       /* If db is too high then set to max    */
	reg_val = decoder_db_table[DB_TABLE_INDEX+db];
	return reg_val;
}

int msm_audio_volume_update(unsigned id,
				int volume, int pan)
{
	unsigned vol_raw;

	vol_raw = compute_db_raw(volume);
	printk(KERN_INFO "volume: %8x vol_raw: %8x \n", volume, vol_raw);
	return audpp_set_volume_and_pan(id, vol_raw, pan);
}
EXPORT_SYMBOL(msm_audio_volume_update);

void alsa_dsp_event(void *data, unsigned id, uint16_t *msg)
{
	struct msm_audio *prtd = data;
	struct buffer *frame;
	unsigned long flag;

	switch (id) {
	case AUDPP_MSG_STATUS_MSG:
		break;
	case AUDPP_MSG_SPA_BANDS:
		break;
	case AUDPP_MSG_HOST_PCM_INTF_MSG:{
			unsigned id = msg[2];
			unsigned idx = msg[3] - 1;
			if (id != AUDPP_MSG_HOSTPCM_ID_ARM_RX) {
				printk(KERN_ERR "bogus id\n");
				break;
			}
			if (idx > 1) {
				printk(KERN_ERR "bogus buffer idx\n");
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
					alsa_dsp_send_buffer(prtd,
							      prtd->out_tail,
							      frame->used);
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
		pr_info("alsa_dsp_event: PCMDMAMISSED %d\n", msg[0]);
		prtd->eos_ack = 1;
		wake_up(&the_locks.eos_wait);
		break;
	case AUDPP_MSG_CFG_MSG:
		if (msg[0] == AUDPP_MSG_ENA_ENA) {
			prtd->out_needed = 0;
			prtd->running = 1;
			audio_dsp_out_enable(prtd, 1);
		} else if (msg[0] == AUDPP_MSG_ENA_DIS) {
			prtd->running = 0;
		} else {
			printk(KERN_ERR "alsa_dsp_event:CFG_MSG=%d\n", msg[0]);
		}
		break;
	case EVENT_MSG_ID:
		printk(KERN_INFO"alsa_dsp_event: arm9 event\n");
		break;
	default:
		printk(KERN_ERR "alsa_dsp_event: UNKNOWN (%d)\n", id);
	}
}

void alsa_audpre_dsp_event(void *data, unsigned id, size_t len,
		      void (*getevent) (void *ptr, size_t len))
{
	uint16_t msg[MAX_DATA_SIZE/2];

	if (len > MAX_DATA_SIZE) {
		printk(KERN_ERR"audpre: event too large(%d bytes)\n", len);
		return;
	}
	getevent(msg, len);

	switch (id) {
	case AUDPREPROC_MSG_CMD_CFG_DONE_MSG:
		break;
	case AUDPREPROC_MSG_ERROR_MSG_ID:
		printk(KERN_ERR "audpre: err_index %d\n", msg[0]);
		break;
	case EVENT_MSG_ID:
		printk(KERN_INFO"audpre: arm9 event\n");
		break;
	default:
		printk(KERN_ERR "audpre: unknown event %d\n", id);
	}
}

void audrec_dsp_event(void *data, unsigned id, size_t len,
		      void (*getevent) (void *ptr, size_t len))
{
	struct msm_audio *prtd = data;
	unsigned long flag;
	uint16_t msg[MAX_DATA_SIZE/2];

	if (len > MAX_DATA_SIZE) {
		printk(KERN_ERR"audrec: event/msg too large(%d bytes)\n", len);
		return;
	}
	getevent(msg, len);

	switch (id) {
	case AUDREC_MSG_CMD_CFG_DONE_MSG:
		if (msg[0] & AUDREC_MSG_CFG_DONE_TYPE_0_UPDATE) {
			if (msg[0] & AUDREC_MSG_CFG_DONE_TYPE_0_ENA)
				audrec_encoder_config(prtd);
			else
				prtd->running = 0;
		}
		break;
	case AUDREC_MSG_CMD_AREC_PARAM_CFG_DONE_MSG:{
			prtd->running = 1;
			break;
		}
	case AUDREC_MSG_FATAL_ERR_MSG:
		printk(KERN_ERR "audrec: ERROR %x\n", msg[0]);
		break;
	case AUDREC_MSG_PACKET_READY_MSG:
		alsa_get_dsp_frames(prtd);
		++intcnt;
		if (prtd->channel_mode == 1) {
			spin_lock_irqsave(&the_locks.read_dsp_lock, flag);
			prtd->pcm_irq_pos += prtd->pcm_count;
			if (prtd->pcm_irq_pos >= prtd->pcm_size)
				prtd->pcm_irq_pos = 0;
			spin_unlock_irqrestore(&the_locks.read_dsp_lock, flag);

			if (prtd->ops->capture)
				prtd->ops->capture(prtd);
		} else if ((prtd->channel_mode == 0) && (intcnt % 2 == 0)) {
			spin_lock_irqsave(&the_locks.read_dsp_lock, flag);
			prtd->pcm_irq_pos += prtd->pcm_count;
			if (prtd->pcm_irq_pos >= prtd->pcm_size)
				prtd->pcm_irq_pos = 0;
			spin_unlock_irqrestore(&the_locks.read_dsp_lock, flag);
			if (prtd->ops->capture)
				prtd->ops->capture(prtd);
		}
		break;
	case EVENT_MSG_ID:
		printk(KERN_INFO"audrec: arm9 event\n");
		break;
	default:
		printk(KERN_ERR "audrec: unknown event %d\n", id);
	}
}

struct msm_adsp_ops aud_pre_adsp_ops = {
	.event = alsa_audpre_dsp_event,
};

struct msm_adsp_ops aud_rec_adsp_ops = {
	.event = audrec_dsp_event,
};

int alsa_adsp_configure(struct msm_audio *prtd)
{
	int ret, i;

	if (prtd->dir == SNDRV_PCM_STREAM_PLAYBACK) {
		prtd->data = prtd->playback_substream->dma_buffer.area;
		prtd->phys = prtd->playback_substream->dma_buffer.addr;
	}
	if (prtd->dir == SNDRV_PCM_STREAM_CAPTURE) {
		prtd->data = prtd->capture_substream->dma_buffer.area;
		prtd->phys = prtd->capture_substream->dma_buffer.addr;
	}
	if (!prtd->data) {
		ret = -ENOMEM;
		goto err1;
	}

	ret = audmgr_open(&prtd->audmgr);
	if (ret)
		goto err2;
	if (prtd->dir == SNDRV_PCM_STREAM_PLAYBACK) {
		prtd->out_buffer_size = PLAYBACK_DMASZ;
		prtd->out_sample_rate = 44100;
		prtd->out_channel_mode = AUDPP_CMD_PCM_INTF_STEREO_V;
		prtd->out_weight = 100;

		prtd->out[0].data = prtd->data + 0;
		prtd->out[0].addr = prtd->phys + 0;
		prtd->out[0].size = BUFSZ;
		prtd->out[1].data = prtd->data + BUFSZ;
		prtd->out[1].addr = prtd->phys + BUFSZ;
		prtd->out[1].size = BUFSZ;
	}
	if (prtd->dir == SNDRV_PCM_STREAM_CAPTURE) {
		prtd->samp_rate = RPC_AUD_DEF_SAMPLE_RATE_44100;
		prtd->samp_rate_index = AUDREC_CMD_SAMP_RATE_INDX_44100;
		prtd->channel_mode = AUDREC_CMD_STEREO_MODE_STEREO;
		prtd->buffer_size = STEREO_DATA_SIZE;
		prtd->type = AUDREC_CMD_TYPE_0_INDEX_WAV;
		prtd->tx_agc_cfg.cmd_id = AUDPREPROC_CMD_CFG_AGC_PARAMS;
		prtd->ns_cfg.cmd_id = AUDPREPROC_CMD_CFG_NS_PARAMS;
		prtd->iir_cfg.cmd_id =
		    AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS;

		ret = msm_adsp_get("AUDPREPROCTASK",
				   &prtd->audpre, &aud_pre_adsp_ops, prtd);
		if (ret)
			goto err3;
		ret = msm_adsp_get("AUDRECTASK",
				   &prtd->audrec, &aud_rec_adsp_ops, prtd);
		if (ret) {
			msm_adsp_put(prtd->audpre);
			goto err3;
		}
		prtd->dsp_cnt = 0;
		prtd->in_head = 0;
		prtd->in_tail = 0;
		prtd->in_count = 0;
		for (i = 0; i < FRAME_NUM; i++) {
			prtd->in[i].size = 0;
			prtd->in[i].read = 0;
		}
	}

	return 0;

err3:
	audmgr_close(&prtd->audmgr);

err2:
	prtd->data = NULL;
err1:
	return ret;
}
EXPORT_SYMBOL(alsa_adsp_configure);

int alsa_audio_configure(struct msm_audio *prtd)
{
	struct audmgr_config cfg;
	int rc;

	if (prtd->enabled)
		return 0;

	/* refuse to start if we're not ready with first buffer */
	if (!prtd->out[0].used)
		return -EIO;

	cfg.tx_rate = 0;
	cfg.rx_rate = RPC_AUD_DEF_SAMPLE_RATE_48000;
	cfg.def_method = RPC_AUD_DEF_METHOD_HOST_PCM;
	cfg.codec = RPC_AUD_DEF_CODEC_PCM;
	cfg.snd_method = RPC_SND_METHOD_MIDI;
	rc = audmgr_enable(&prtd->audmgr, &cfg);
	if (rc < 0)
		return rc;

	if (audpp_enable(AUDPP_ALSA_DECODER, alsa_dsp_event, prtd)) {
		printk(KERN_ERR "audio: audpp_enable() failed\n");
		audmgr_disable(&prtd->audmgr);
		return -ENODEV;
	}

	prtd->enabled = 1;
	return 0;
}
EXPORT_SYMBOL(alsa_audio_configure);

ssize_t alsa_send_buffer(struct msm_audio *prtd, const char __user *buf,
			  size_t count, loff_t *pos)
{
	unsigned long flag;
	const char __user *start = buf;
	struct buffer *frame;
	size_t xfer;
	int rc = 0;

	mutex_lock(&the_locks.write_lock);
	while (count > 0) {
		frame = prtd->out + prtd->out_head;
		rc = wait_event_interruptible(the_locks.write_wait,
					      (frame->used == 0)
					      || (prtd->stopped));
		if (rc < 0)
			break;
		if (prtd->stopped) {
			rc = -EBUSY;
			break;
		}
		xfer = count > frame->size ? frame->size : count;
		if (copy_from_user(frame->data, buf, xfer)) {
			rc = -EFAULT;
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
			prtd->out_tail ^= 1;
			prtd->out_needed--;
		}
		spin_unlock_irqrestore(&the_locks.write_dsp_lock, flag);
	}
	mutex_unlock(&the_locks.write_lock);
	if (buf > start)
		return buf - start;
	return rc;
}
EXPORT_SYMBOL(alsa_send_buffer);

int alsa_audio_disable(struct msm_audio *prtd)
{
	if (prtd->enabled) {
		mutex_lock(&the_locks.lock);
		prtd->enabled = 0;
		audio_dsp_out_enable(prtd, 0);
		wake_up(&the_locks.write_wait);
		audpp_disable(AUDPP_ALSA_DECODER, prtd);
		audmgr_disable(&prtd->audmgr);
		prtd->out_needed = 0;
		mutex_unlock(&the_locks.lock);
	}
	return 0;
}
EXPORT_SYMBOL(alsa_audio_disable);

int alsa_audrec_disable(struct msm_audio *prtd)
{
	if (prtd->enabled) {
		mutex_lock(&the_locks.lock);
		prtd->enabled = 0;
		alsa_rec_dsp_enable(prtd, 0);
		wake_up(&the_locks.read_wait);
		msm_adsp_disable(prtd->audpre);
		msm_adsp_disable(prtd->audrec);
		audmgr_disable(&prtd->audmgr);
		prtd->out_needed = 0;
		prtd->opened = 0;
		mutex_unlock(&the_locks.lock);
	}
	return 0;
}
EXPORT_SYMBOL(alsa_audrec_disable);

static int audio_dsp_read_buffer(struct msm_audio *prtd, uint32_t read_cnt)
{
	audrec_cmd_packet_ext_ptr cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_PACKET_EXT_PTR;
	/* Both WAV and AAC use AUDREC_CMD_TYPE_0 */
	cmd.type = AUDREC_CMD_TYPE_0;
	cmd.curr_rec_count_msw = read_cnt >> 16;
	cmd.curr_rec_count_lsw = read_cnt;

	return audio_send_queue_recbs(prtd, &cmd, sizeof(cmd));
}

int audrec_encoder_config(struct msm_audio *prtd)
{
	audrec_cmd_arec0param_cfg cmd;
	uint16_t *data = (void *)prtd->data;
	unsigned n;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_AREC0PARAM_CFG;
	cmd.ptr_to_extpkt_buffer_msw = prtd->phys >> 16;
	cmd.ptr_to_extpkt_buffer_lsw = prtd->phys;
	cmd.buf_len = FRAME_NUM;	/* Both WAV and AAC use 8 frames */
	cmd.samp_rate_index = prtd->samp_rate_index;
	/* 0 for mono, 1 for stereo */
	cmd.stereo_mode = prtd->channel_mode;
	cmd.rec_quality = 0x1C00;

	/* prepare buffer pointers:
	 * Mono: 1024 samples + 4 halfword header
	 * Stereo: 2048 samples + 4 halfword header
	 */

	for (n = 0; n < FRAME_NUM; n++) {
		prtd->in[n].data = data + 4;
		data += (4 + (prtd->channel_mode ? 2048 : 1024));
	}

	return audio_send_queue_rec(prtd, &cmd, sizeof(cmd));
}

int audio_dsp_out_enable(struct msm_audio *prtd, int yes)
{
	audpp_cmd_pcm_intf cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDPP_CMD_PCM_INTF_2;
	cmd.object_num = AUDPP_CMD_PCM_INTF_OBJECT_NUM;
	cmd.config = AUDPP_CMD_PCM_INTF_CONFIG_CMD_V;
	cmd.intf_type = AUDPP_CMD_PCM_INTF_RX_ENA_ARMTODSP_V;

	if (yes) {
		cmd.write_buf1LSW = prtd->out[0].addr;
		cmd.write_buf1MSW = prtd->out[0].addr >> 16;
		cmd.write_buf1_len = 0;
		cmd.write_buf2LSW = prtd->out[1].addr;
		cmd.write_buf2MSW = prtd->out[1].addr >> 16;
		cmd.write_buf2_len = prtd->out[1].used;
		cmd.arm_to_rx_flag = AUDPP_CMD_PCM_INTF_ENA_V;
		cmd.weight_decoder_to_rx = prtd->out_weight;
		cmd.weight_arm_to_rx = 1;
		cmd.partition_number_arm_to_dsp = 0;
		cmd.sample_rate = prtd->out_sample_rate;
		cmd.channel_mode = prtd->out_channel_mode;
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
	int rc = 0;

	mutex_lock(&the_locks.read_lock);
	while (count > 0) {
		rc = wait_event_interruptible(the_locks.read_wait,
					      (prtd->in_count > 0)
					      || prtd->stopped);
		if (rc < 0)
			break;

		if (prtd->stopped) {
			rc = -EBUSY;
			break;
		}

		index = prtd->in_tail;
		data = (uint8_t *) prtd->in[index].data;
		size = prtd->in[index].size;
		if (count >= size) {
			if (copy_to_user(buf, data, size)) {
				rc = -EFAULT;
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
	return rc;
}
EXPORT_SYMBOL(alsa_buffer_read);

int alsa_dsp_send_buffer(struct msm_audio *prtd,
					unsigned idx, unsigned len)
{
	audpp_cmd_pcm_intf_send_buffer cmd;
	cmd.cmd_id = AUDPP_CMD_PCM_INTF_2;
	cmd.host_pcm_object = AUDPP_CMD_PCM_INTF_OBJECT_NUM;
	cmd.config = AUDPP_CMD_PCM_INTF_BUFFER_CMD_V;
	cmd.intf_type = AUDPP_CMD_PCM_INTF_RX_ENA_ARMTODSP_V;
	cmd.dsp_to_arm_buf_id = 0;
	cmd.arm_to_dsp_buf_id = idx + 1;
	cmd.arm_to_dsp_buf_len = len;
	return audpp_send_queue2(&cmd, sizeof(cmd));
}

int alsa_rec_dsp_enable(struct msm_audio *prtd, int enable)
{
	audrec_cmd_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_CFG;
	cmd.type_0 = enable ? AUDREC_CMD_TYPE_0_ENA : AUDREC_CMD_TYPE_0_DIS;
	cmd.type_0 |= (AUDREC_CMD_TYPE_0_UPDATE | prtd->type);
	cmd.type_1 = 0;

	return audio_send_queue_rec(prtd, &cmd, sizeof(cmd));
}
EXPORT_SYMBOL(alsa_rec_dsp_enable);

void alsa_get_dsp_frames(struct msm_audio *prtd)
{
	struct audio_frame *frame;
	uint32_t index = 0;
	unsigned long flag;

	if (prtd->type == AUDREC_CMD_TYPE_0_INDEX_WAV) {
		index = prtd->in_head;

		frame =
		    (void *)(((char *)prtd->in[index].data) - sizeof(*frame));

		spin_lock_irqsave(&the_locks.read_dsp_lock, flag);
		prtd->in[index].size = frame->bytes;

		prtd->in_head = (prtd->in_head + 1) & (FRAME_NUM - 1);

		/* If overflow, move the tail index foward. */
		if (prtd->in_head == prtd->in_tail)
			prtd->in_tail = (prtd->in_tail + 1) & (FRAME_NUM - 1);
		else
			prtd->in_count++;

		audio_dsp_read_buffer(prtd, prtd->dsp_cnt++);
		spin_unlock_irqrestore(&the_locks.read_dsp_lock, flag);

		wake_up(&the_locks.read_wait);
	} else {
		/* TODO AAC not supported yet. */
	}
}
EXPORT_SYMBOL(alsa_get_dsp_frames);
