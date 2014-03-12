/*
 * Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
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
#include <linux/msm_audio_aac.h>
#include <asm/atomic.h>
#include <asm/ioctls.h>
#include "audio_utils.h"


/* Buffer with meta*/
#define PCM_BUF_SIZE		(4096 + sizeof(struct meta_in))

/* Maximum 5 frames in buffer with meta */
#define FRAME_SIZE		(1 + ((1536+sizeof(struct meta_out_dsp)) * 5))

#define AAC_FORMAT_ADTS 65535

/* ------------------- device --------------------- */
static long aac_in_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct q6audio_in  *audio = file->private_data;
	int rc = 0;
	int cnt = 0;

	switch (cmd) {
	case AUDIO_START: {
		struct msm_audio_aac_enc_config *enc_cfg;
		struct msm_audio_aac_config *aac_config;
		uint32_t aac_mode = AAC_ENC_MODE_AAC_LC;

		enc_cfg = audio->enc_cfg;
		aac_config = audio->codec_cfg;
		/* ENCODE CFG (after new set of API's are published )bharath*/
		pr_debug("%s:session id %d: default buf alloc[%d]\n", __func__,
				audio->ac->session, audio->buf_alloc);
		if (audio->enabled == 1) {
			pr_info("%s:AUDIO_START already over\n", __func__);
			rc = 0;
			break;
		}

		if (audio->opened) {
			rc = audio_in_buf_alloc(audio);
			if (rc < 0) {
				pr_err("%s:session id %d: buffer allocation failed\n",
					 __func__, audio->ac->session);
				break;
			}
		} else {
			if (audio->feedback == NON_TUNNEL_MODE) {
				pr_debug("%s: starting in non_tunnel mode",
					__func__);
				rc = q6asm_open_read_write(audio->ac,
					FORMAT_MPEG4_AAC, FORMAT_LINEAR_PCM);
				if (rc < 0) {
					pr_err("%s:open read write failed\n",
						__func__);
					break;
				}
			}
			if (audio->feedback == TUNNEL_MODE) {
				pr_debug("%s: starting in tunnel mode",
					__func__);
				rc = q6asm_open_read(audio->ac,
							FORMAT_MPEG4_AAC);

				if (rc < 0) {
					pr_err("%s:open read failed\n",
							__func__);
					break;
				}
			}
			audio->stopped = 0;
		}

		pr_debug("%s:sbr_ps_flag = %d, sbr_flag = %d\n", __func__,
			aac_config->sbr_ps_on_flag, aac_config->sbr_on_flag);
		if (aac_config->sbr_ps_on_flag)
			aac_mode = AAC_ENC_MODE_EAAC_P;
		else if (aac_config->sbr_on_flag)
			aac_mode = AAC_ENC_MODE_AAC_P;
		else
			aac_mode = AAC_ENC_MODE_AAC_LC;

		rc = q6asm_enc_cfg_blk_aac(audio->ac,
					audio->buf_cfg.frames_per_buf,
					enc_cfg->sample_rate,
					enc_cfg->channels,
					enc_cfg->bit_rate,
					aac_mode,
					enc_cfg->stream_format);
		if (rc < 0) {
			pr_err("%s:session id %d: cmd media format block"
				"failed\n", __func__, audio->ac->session);
			break;
		}
		if (audio->feedback == NON_TUNNEL_MODE) {
			rc = q6asm_media_format_block_pcm(audio->ac,
						audio->pcm_cfg.sample_rate,
						audio->pcm_cfg.channel_count);
			if (rc < 0) {
				pr_err("%s:session id %d: media format block"
				"failed\n", __func__, audio->ac->session);
				break;
			}
		}
		rc = audio_in_enable(audio);
		if (!rc) {
			audio->enabled = 1;
		} else {
			audio->enabled = 0;
			pr_err("%s:session id %d: Audio Start procedure"
			"failed rc=%d\n", __func__, audio->ac->session, rc);
			break;
		}
		while (cnt++ < audio->str_cfg.buffer_count)
			q6asm_read(audio->ac);
		pr_debug("%s:session id %d: AUDIO_START success enable[%d]\n",
				__func__, audio->ac->session, audio->enabled);
		break;
	}
	case AUDIO_STOP: {
		pr_debug("%s:session id %d: Rxed AUDIO_STOP\n", __func__,
				audio->ac->session);
		rc = audio_in_disable(audio);
		if (rc  < 0) {
			pr_err("%s:session id %d: Audio Stop procedure failed"
				"rc=%d\n", __func__, audio->ac->session, rc);
			break;
		}
		break;
	}
	case AUDIO_GET_AAC_ENC_CONFIG: {
		struct msm_audio_aac_enc_config cfg;
		struct msm_audio_aac_enc_config *enc_cfg;
		memset(&cfg, 0, sizeof(cfg));
		enc_cfg = audio->enc_cfg;
		if (enc_cfg->channels == CH_MODE_MONO)
			cfg.channels = 1;
		else
			cfg.channels = 2;
		cfg.sample_rate = enc_cfg->sample_rate;
		cfg.bit_rate = enc_cfg->bit_rate;

		switch (enc_cfg->stream_format) {
		case 0x00:
			cfg.stream_format = AUDIO_AAC_FORMAT_ADTS;
			break;
		case 0x01:
			cfg.stream_format = AUDIO_AAC_FORMAT_LOAS;
			break;
		case 0x02:
			cfg.stream_format = AUDIO_AAC_FORMAT_ADIF;
			break;
		default:
		case 0x03:
			cfg.stream_format = AUDIO_AAC_FORMAT_RAW;
		}

		pr_debug("%s:session id %d: Get-aac-cfg: format=%d sr=%d"
			"bitrate=%d\n", __func__, audio->ac->session,
			cfg.stream_format, cfg.sample_rate, cfg.bit_rate);
		if (copy_to_user((void *)arg, &cfg, sizeof(cfg)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_SET_AAC_ENC_CONFIG: {
		struct msm_audio_aac_enc_config cfg;
		struct msm_audio_aac_enc_config *enc_cfg;
		uint32_t min_bitrate, max_bitrate;
		enc_cfg = audio->enc_cfg;
		if (copy_from_user(&cfg, (void *)arg, sizeof(cfg))) {
			rc = -EFAULT;
			break;
		}
		pr_debug("%s:session id %d: Set-aac-cfg: stream=%d\n", __func__,
					audio->ac->session, cfg.stream_format);

		switch (cfg.stream_format) {
		case AUDIO_AAC_FORMAT_ADTS:
			enc_cfg->stream_format = 0x00;
			break;
		case AUDIO_AAC_FORMAT_LOAS:
			enc_cfg->stream_format = 0x01;
			break;
		case AUDIO_AAC_FORMAT_ADIF:
			enc_cfg->stream_format = 0x02;
			break;
		case AUDIO_AAC_FORMAT_RAW:
			enc_cfg->stream_format = 0x03;
			break;
		default:
			pr_err("%s:session id %d: unsupported AAC format %d\n",
				__func__, audio->ac->session,
				cfg.stream_format);
			rc = -EINVAL;
			break;
		}

		if (cfg.channels == 1) {
			cfg.channels = CH_MODE_MONO;
		} else if (cfg.channels == 2) {
			cfg.channels = CH_MODE_STEREO;
		} else {
			rc = -EINVAL;
			break;
		}
		if ((cfg.sample_rate < 8000) || (cfg.sample_rate > 48000)) {
			pr_err("%s: ERROR in setting samplerate = %d\n",
				__func__, cfg.sample_rate);
			rc = -EINVAL;
			break;
		}
		/* For aac-lc, min_bit_rate = min(24Kbps, 0.5*SR*num_chan);
		max_bi_rate = min(192Kbps, 6*SR*num_chan);
		min_sample_rate = 8000Hz, max_rate=48000 */
		min_bitrate = ((cfg.sample_rate)*(cfg.channels))/2;
		if (min_bitrate < 24000)
			min_bitrate = 24000;
		max_bitrate = 6*(cfg.sample_rate)*(cfg.channels);
		if (max_bitrate > 192000)
			max_bitrate = 192000;
		if ((cfg.bit_rate < min_bitrate) ||
					(cfg.bit_rate > max_bitrate)) {
			pr_err("%s: bitrate permissible: max=%d, min=%d\n",
				__func__, max_bitrate, min_bitrate);
			pr_err("%s: ERROR in setting bitrate = %d\n",
				__func__, cfg.bit_rate);
			rc = -EINVAL;
			break;
		}
		enc_cfg->sample_rate = cfg.sample_rate;
		enc_cfg->channels = cfg.channels;
		enc_cfg->bit_rate = cfg.bit_rate;
		pr_debug("%s:session id %d: Set-aac-cfg:SR= 0x%x ch=0x%x"
			"bitrate=0x%x, format(adts/raw) = %d\n",
			__func__, audio->ac->session, enc_cfg->sample_rate,
			enc_cfg->channels, enc_cfg->bit_rate,
			enc_cfg->stream_format);
		break;
	}
	case AUDIO_GET_AAC_CONFIG: {
		if (copy_to_user((void *)arg, &audio->codec_cfg,
				 sizeof(struct msm_audio_aac_config))) {
			rc = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_SET_AAC_CONFIG: {
		struct msm_audio_aac_config aac_cfg;
		struct msm_audio_aac_config *audio_aac_cfg;
		struct msm_audio_aac_enc_config *enc_cfg;
		enc_cfg = audio->enc_cfg;
		audio_aac_cfg = audio->codec_cfg;

		if (copy_from_user(&aac_cfg, (void *)arg,
				 sizeof(struct msm_audio_aac_config))) {
			rc = -EFAULT;
			break;
		}
		pr_debug("%s:session id %d: AUDIO_SET_AAC_CONFIG: sbr_flag = %d"
				 " sbr_ps_flag = %d\n", __func__,
				 audio->ac->session, aac_cfg.sbr_on_flag,
				 aac_cfg.sbr_ps_on_flag);
		audio_aac_cfg->sbr_on_flag = aac_cfg.sbr_on_flag;
		audio_aac_cfg->sbr_ps_on_flag = aac_cfg.sbr_ps_on_flag;
		if ((audio_aac_cfg->sbr_on_flag == 1) ||
			 (audio_aac_cfg->sbr_ps_on_flag == 1)) {
			if (enc_cfg->sample_rate < 24000) {
				pr_err("%s: ERROR in setting samplerate = %d"
					"\n", __func__, enc_cfg->sample_rate);
				rc = -EINVAL;
				break;
			}
		}
		break;
	}
	default:
		rc = -EINVAL;
	}
	return rc;
}

static int aac_in_open(struct inode *inode, struct file *file)
{
	struct q6audio_in *audio = NULL;
	struct msm_audio_aac_enc_config *enc_cfg;
	struct msm_audio_aac_config *aac_config;
	int rc = 0;

	audio = kzalloc(sizeof(struct q6audio_in), GFP_KERNEL);

	if (audio == NULL) {
		pr_err("%s: Could not allocate memory for aac"
				"driver\n", __func__);
		return -ENOMEM;
	}
	/* Allocate memory for encoder config param */
	audio->enc_cfg = kzalloc(sizeof(struct msm_audio_aac_enc_config),
				GFP_KERNEL);
	if (audio->enc_cfg == NULL) {
		pr_err("%s:session id %d: Could not allocate memory for aac"
				"config param\n", __func__, audio->ac->session);
		kfree(audio);
		return -ENOMEM;
	}
	enc_cfg = audio->enc_cfg;

	audio->codec_cfg = kzalloc(sizeof(struct msm_audio_aac_config),
				GFP_KERNEL);
	if (audio->codec_cfg == NULL) {
		pr_err("%s:session id %d: Could not allocate memory for aac"
				"config\n", __func__, audio->ac->session);
		kfree(audio->enc_cfg);
		kfree(audio);
		return -ENOMEM;
	}
	aac_config = audio->codec_cfg;

	mutex_init(&audio->lock);
	mutex_init(&audio->read_lock);
	mutex_init(&audio->write_lock);
	spin_lock_init(&audio->dsp_lock);
	init_waitqueue_head(&audio->read_wait);
	init_waitqueue_head(&audio->write_wait);

	/* Settings will be re-config at AUDIO_SET_CONFIG,
	* but at least we need to have initial config
	*/
	audio->str_cfg.buffer_size = FRAME_SIZE;
	audio->str_cfg.buffer_count = FRAME_NUM;
	audio->min_frame_size = 1536;
	audio->max_frames_per_buf = 5;
	enc_cfg->sample_rate = 8000;
	enc_cfg->channels = 1;
	enc_cfg->bit_rate = 16000;
	enc_cfg->stream_format = 0x00;/* 0:ADTS, 3:RAW */
	audio->buf_cfg.meta_info_enable = 0x01;
	audio->buf_cfg.frames_per_buf   = 0x01;
	audio->pcm_cfg.buffer_count = PCM_BUF_COUNT;
	audio->pcm_cfg.buffer_size  = PCM_BUF_SIZE;
	aac_config->format = AUDIO_AAC_FORMAT_ADTS;
	aac_config->audio_object = AUDIO_AAC_OBJECT_LC;
	aac_config->sbr_on_flag = 0;
	aac_config->sbr_ps_on_flag = 0;
	aac_config->channel_configuration = 1;

	audio->ac = q6asm_audio_client_alloc((app_cb)q6asm_in_cb,
							(void *)audio);

	if (!audio->ac) {
		pr_err("%s: Could not allocate memory for"
				"audio client\n", __func__);
		kfree(audio->enc_cfg);
		kfree(audio->codec_cfg);
		kfree(audio);
		return -ENOMEM;
	}
	/* open aac encoder in tunnel mode */
	audio->buf_cfg.frames_per_buf = 0x01;

	if ((file->f_mode & FMODE_WRITE) &&
		(file->f_mode & FMODE_READ)) {
		audio->feedback = NON_TUNNEL_MODE;
		rc = q6asm_open_read_write(audio->ac, FORMAT_MPEG4_AAC,
						FORMAT_LINEAR_PCM);

		if (rc < 0) {
			pr_err("%s:session id %d: NT Open failed rc=%d\n",
				__func__, audio->ac->session, rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->buf_cfg.meta_info_enable = 0x01;
		pr_info("%s:session id %d: NT mode encoder success\n", __func__,
				audio->ac->session);
	} else if (!(file->f_mode & FMODE_WRITE) &&
				(file->f_mode & FMODE_READ)) {
		audio->feedback = TUNNEL_MODE;
		rc = q6asm_open_read(audio->ac, FORMAT_MPEG4_AAC);

		if (rc < 0) {
			pr_err("%s:session id %d: Tunnel Open failed rc=%d\n",
				__func__, audio->ac->session, rc);
			rc = -ENODEV;
			goto fail;
		}
		/* register for tx overflow (valid for tunnel mode only) */
		rc = q6asm_reg_tx_overflow(audio->ac, 0x01);
		if (rc < 0) {
			pr_err("%s:session id %d: TX Overflow registration"
				"failed rc=%d\n", __func__,
				audio->ac->session, rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->buf_cfg.meta_info_enable = 0x00;
		pr_info("%s:session id %d: T mode encoder success\n", __func__,
			audio->ac->session);
	} else {
		pr_err("%s:session id %d: Unexpected mode\n", __func__,
				audio->ac->session);
		rc = -EACCES;
		goto fail;
	}
	audio->opened = 1;
	atomic_set(&audio->in_count, PCM_BUF_COUNT);
	atomic_set(&audio->out_count, 0x00);
	audio->enc_ioctl = aac_in_ioctl;
	file->private_data = audio;

	pr_info("%s:session id %d: success\n", __func__, audio->ac->session);
	return 0;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio->enc_cfg);
	kfree(audio->codec_cfg);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_in_fops = {
	.owner		= THIS_MODULE,
	.open		= aac_in_open,
	.release	= audio_in_release,
	.read		= audio_in_read,
	.write		= audio_in_write,
	.unlocked_ioctl	= audio_in_ioctl,
};

struct miscdevice audio_aac_in_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_aac_in",
	.fops	= &audio_in_fops,
};

static int __init aac_in_init(void)
{
	return misc_register(&audio_aac_in_misc);
}
device_initcall(aac_in_init);
