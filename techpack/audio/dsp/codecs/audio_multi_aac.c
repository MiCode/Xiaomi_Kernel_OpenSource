/* aac audio output device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#include <linux/msm_audio_aac.h>
#include <linux/compat.h>
#include <soc/qcom/socinfo.h>
#include "audio_utils_aio.h"

#define AUDIO_AAC_DUAL_MONO_INVALID -1


/* Default number of pre-allocated event packets */
#define PCM_BUFSZ_MIN_AACM	((8*1024) + sizeof(struct dec_meta_out))
static struct miscdevice audio_multiaac_misc;
static struct ws_mgr audio_multiaac_ws_mgr;

#ifdef CONFIG_DEBUG_FS
static const struct file_operations audio_aac_debug_fops = {
	.read = audio_aio_debug_read,
	.open = audio_aio_debug_open,
};
#endif

static long audio_ioctl_shared(struct file *file, unsigned int cmd,
						void *arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
		struct asm_aac_cfg aac_cfg;
		struct msm_audio_aac_config *aac_config;
		uint32_t sbr_ps = 0x00;

		aac_config = (struct msm_audio_aac_config *)audio->codec_cfg;
		if (audio->feedback == TUNNEL_MODE) {
			aac_cfg.sample_rate = aac_config->sample_rate;
			aac_cfg.ch_cfg = aac_config->channel_configuration;
		} else {
			aac_cfg.sample_rate =  audio->pcm_cfg.sample_rate;
			aac_cfg.ch_cfg = audio->pcm_cfg.channel_count;
		}
		pr_debug("%s: AUDIO_START session_id[%d]\n", __func__,
						audio->ac->session);
		if (audio->feedback == NON_TUNNEL_MODE) {
			/* Configure PCM output block */
			rc = q6asm_enc_cfg_blk_pcm(audio->ac,
					audio->pcm_cfg.sample_rate,
					audio->pcm_cfg.channel_count);
			if (rc < 0) {
				pr_err("pcm output block config failed\n");
				break;
			}
		}
		/* turn on both sbr and ps */
		rc = q6asm_enable_sbrps(audio->ac, sbr_ps);
		if (rc < 0)
			pr_err("sbr-ps enable failed\n");
		if (aac_config->sbr_ps_on_flag)
			aac_cfg.aot = AAC_ENC_MODE_EAAC_P;
		else if (aac_config->sbr_on_flag)
			aac_cfg.aot = AAC_ENC_MODE_AAC_P;
		else
			aac_cfg.aot = AAC_ENC_MODE_AAC_LC;

		switch (aac_config->format) {
		case AUDIO_AAC_FORMAT_ADTS:
			aac_cfg.format = 0x00;
			break;
		case AUDIO_AAC_FORMAT_LOAS:
			aac_cfg.format = 0x01;
			break;
		case AUDIO_AAC_FORMAT_ADIF:
			aac_cfg.format = 0x02;
			break;
		default:
		case AUDIO_AAC_FORMAT_RAW:
			aac_cfg.format = 0x03;
		}
		aac_cfg.ep_config = aac_config->ep_config;
		aac_cfg.section_data_resilience =
			aac_config->aac_section_data_resilience_flag;
		aac_cfg.scalefactor_data_resilience =
			aac_config->aac_scalefactor_data_resilience_flag;
		aac_cfg.spectral_data_resilience =
			aac_config->aac_spectral_data_resilience_flag;

		pr_debug("%s:format=%x aot=%d  ch=%d sr=%d\n",
			__func__, aac_cfg.format,
			aac_cfg.aot, aac_cfg.ch_cfg,
			aac_cfg.sample_rate);

		/* Configure Media format block */
		rc = q6asm_media_format_block_multi_aac(audio->ac, &aac_cfg);
		if (rc < 0) {
			pr_err("cmd media format block failed\n");
			break;
		}

		/* Fall back to the default number of channels
		 * if aac_cfg.ch_cfg is not between 1-6
		 */
		if ((aac_cfg.ch_cfg == 0) || (aac_cfg.ch_cfg > 6))
			aac_cfg.ch_cfg = 2;

		rc = q6asm_set_encdec_chan_map(audio->ac, aac_cfg.ch_cfg);
		if (rc < 0) {
			pr_err("%s: cmd set encdec_chan_map failed\n",
				__func__);
			break;
		}
		rc = audio_aio_enable(audio);
		audio->eos_rsp = 0;
		audio->eos_flag = 0;
		if (!rc) {
			audio->enabled = 1;
		} else {
			audio->enabled = 0;
			pr_err("Audio Start procedure failed rc=%d\n", rc);
			break;
		}
		pr_info("%s: AUDIO_START sessionid[%d]enable[%d]\n", __func__,
						audio->ac->session,
						audio->enabled);
		if (audio->stopped == 1)
			audio->stopped = 0;
		break;
	}
	case AUDIO_SET_AAC_CONFIG: {
		struct msm_audio_aac_config *aac_config;
		uint16_t sce_left = 1, sce_right = 2;

		if (arg == NULL) {
			pr_err("%s: NULL config pointer\n", __func__);
			rc = -EINVAL;
			break;
		}
		memcpy(audio->codec_cfg, arg,
				sizeof(struct msm_audio_aac_config));
		aac_config = audio->codec_cfg;
		if (aac_config->dual_mono_mode >
		    AUDIO_AAC_DUAL_MONO_PL_SR) {
			pr_err("%s:AUDIO_SET_AAC_CONFIG: Invalid dual_mono mode =%d\n",
				 __func__, aac_config->dual_mono_mode);
		} else {
			/* convert the data from user into sce_left
			 * and sce_right based on the definitions
			 */
			pr_debug("%s: AUDIO_SET_AAC_CONFIG: modify dual_mono mode =%d\n",
				 __func__, aac_config->dual_mono_mode);
			switch (aac_config->dual_mono_mode) {
			case AUDIO_AAC_DUAL_MONO_PL_PR:
				sce_left = 1;
				sce_right = 1;
				break;
			case AUDIO_AAC_DUAL_MONO_SL_SR:
				sce_left = 2;
				sce_right = 2;
				break;
			case AUDIO_AAC_DUAL_MONO_SL_PR:
				sce_left = 2;
				sce_right = 1;
				break;
			case AUDIO_AAC_DUAL_MONO_PL_SR:
			default:
				sce_left = 1;
				sce_right = 2;
				break;
			}
			rc = q6asm_cfg_dual_mono_aac(audio->ac,
						sce_left, sce_right);
			if (rc < 0)
				pr_err("%s: asm cmd dualmono failed rc=%d\n",
							 __func__, rc);
		}			break;
		break;
	}
	case AUDIO_SET_AAC_MIX_CONFIG:	{
		u32 *mix_coeff = (u32 *)arg;

		if (!arg) {
			pr_err("%s: Invalid param for %s\n",
				__func__, "AUDIO_SET_AAC_MIX_CONFIG");
			rc = -EINVAL;
			break;
		}
		pr_debug("%s, AUDIO_SET_AAC_MIX_CONFIG", __func__);
		pr_debug("%s, value of coeff = %d",
					__func__, *mix_coeff);
		q6asm_cfg_aac_sel_mix_coef(audio->ac, *mix_coeff);
		if (rc < 0)
			pr_err("%s asm aac_sel_mix_coef failed rc=%d\n",
				 __func__, rc);
		break;
	}
	default:
		pr_err("%s: Unknown ioctl cmd = %d", __func__, cmd);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
		rc = audio_ioctl_shared(file, cmd, (void *)arg);
		break;
	}
	case AUDIO_GET_AAC_CONFIG: {
		if (copy_to_user((void *)arg, audio->codec_cfg,
			sizeof(struct msm_audio_aac_config))) {
			pr_err("%s: copy_to_user for AUDIO_GET_AAC_CONFIG failed\n"
				, __func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_SET_AAC_CONFIG: {
		struct msm_audio_aac_config aac_config;

		if (copy_from_user(&aac_config, (void *)arg,
			sizeof(aac_config))) {
			pr_err("%s: copy_from_user for AUDIO_SET_AAC_CONFIG failed\n"
				, __func__);
			rc = -EFAULT;
		}
		rc = audio_ioctl_shared(file, cmd, &aac_config);
		if (rc)
			pr_err("%s:AUDIO_SET_AAC_CONFIG failed. Rc= %d\n",
				__func__, rc);
		break;
	}
	case AUDIO_SET_AAC_MIX_CONFIG:	{
		u32 mix_config;

		pr_debug("%s, AUDIO_SET_AAC_MIX_CONFIG", __func__);
		if (copy_from_user(&mix_config, (void *)arg,
			sizeof(u32))) {
			pr_err("%s: copy_from_user for AUDIO_SET_AAC_MIX_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		rc = audio_ioctl_shared(file, cmd, &mix_config);
		if (rc)
			pr_err("%s:AUDIO_SET_AAC_CONFIG failed. Rc= %d\n",
			__func__, rc);
		break;
	}
	default: {
		pr_debug("Calling utils ioctl\n");
		rc = audio->codec_ioctl(file, cmd, arg);
	}
	}
	return rc;
}

#ifdef CONFIG_COMPAT
struct msm_audio_aac_config32 {
	s16 format;
	u16 audio_object;
	u16 ep_config;  /* 0 ~ 3 useful only obj = ERLC */
	u16 aac_section_data_resilience_flag;
	u16 aac_scalefactor_data_resilience_flag;
	u16 aac_spectral_data_resilience_flag;
	u16 sbr_on_flag;
	u16 sbr_ps_on_flag;
	u16 dual_mono_mode;
	u16 channel_configuration;
	u16 sample_rate;
};

enum {
	AUDIO_SET_AAC_CONFIG_32 = _IOW(AUDIO_IOCTL_MAGIC,
		(AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_aac_config32),
	AUDIO_GET_AAC_CONFIG_32 = _IOR(AUDIO_IOCTL_MAGIC,
		(AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_aac_config32),
};

static long audio_compat_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
		rc = audio_ioctl_shared(file, cmd, (void *)arg);
		break;
	}
	case AUDIO_GET_AAC_CONFIG_32: {
		struct msm_audio_aac_config *aac_config;
		struct msm_audio_aac_config32 aac_config_32;

		memset(&aac_config_32, 0, sizeof(aac_config_32));

		aac_config = (struct msm_audio_aac_config *)audio->codec_cfg;
		aac_config_32.format = aac_config->format;
		aac_config_32.audio_object = aac_config->audio_object;
		aac_config_32.ep_config = aac_config->ep_config;
		aac_config_32.aac_section_data_resilience_flag =
			aac_config->aac_section_data_resilience_flag;
		aac_config_32.aac_scalefactor_data_resilience_flag =
			aac_config->aac_scalefactor_data_resilience_flag;
		aac_config_32.aac_spectral_data_resilience_flag =
			aac_config->aac_spectral_data_resilience_flag;
		aac_config_32.sbr_on_flag = aac_config->sbr_on_flag;
		aac_config_32.sbr_ps_on_flag = aac_config->sbr_ps_on_flag;
		aac_config_32.dual_mono_mode = aac_config->dual_mono_mode;
		aac_config_32.channel_configuration =
			aac_config->channel_configuration;
		aac_config_32.sample_rate = aac_config->sample_rate;

		if (copy_to_user((void *)arg, &aac_config_32,
			sizeof(aac_config_32))) {
			pr_err("%s: copy_to_user for AUDIO_GET_AAC_CONFIG_32 failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_SET_AAC_CONFIG_32: {
		struct msm_audio_aac_config aac_config;
		struct msm_audio_aac_config32 aac_config_32;

		pr_debug("%s: AUDIO_SET_AAC_CONFIG\n", __func__);
		if (copy_from_user(&aac_config_32, (void *)arg,
			sizeof(aac_config_32))) {
			pr_err(
				"%s: copy_from_user for AUDIO_SET_AAC_CONFIG_32 failed",
				__func__);
			rc = -EFAULT;
			break;
		}
		aac_config.format = aac_config_32.format;
		aac_config.audio_object = aac_config_32.audio_object;
		aac_config.ep_config = aac_config_32.ep_config;
		aac_config.aac_section_data_resilience_flag =
			aac_config_32.aac_section_data_resilience_flag;
		aac_config.aac_scalefactor_data_resilience_flag =
			aac_config_32.aac_scalefactor_data_resilience_flag;
		aac_config.aac_spectral_data_resilience_flag =
			aac_config_32.aac_spectral_data_resilience_flag;
		aac_config.sbr_on_flag = aac_config_32.sbr_on_flag;
		aac_config.sbr_ps_on_flag = aac_config_32.sbr_ps_on_flag;
		aac_config.dual_mono_mode = aac_config_32.dual_mono_mode;
		aac_config.channel_configuration =
				aac_config_32.channel_configuration;
		aac_config.sample_rate = aac_config_32.sample_rate;

		cmd = AUDIO_SET_AAC_CONFIG;
		rc = audio_ioctl_shared(file, cmd, &aac_config);
		if (rc)
			pr_err("%s:AUDIO_SET_AAC_CONFIG failed. rc= %d\n",
				__func__, rc);
		break;
	}
	case AUDIO_SET_AAC_MIX_CONFIG: {
		u32 mix_config;

		pr_debug("%s, AUDIO_SET_AAC_MIX_CONFIG\n", __func__);
		if (copy_from_user(&mix_config, (void *)arg,
			sizeof(u32))) {
			pr_err("%s: copy_from_user for AUDIO_SET_AAC_MIX_CONFIG failed\n"
				, __func__);
			rc = -EFAULT;
			break;
		}
		rc = audio_ioctl_shared(file, cmd, &mix_config);
		if (rc)
			pr_err("%s:AUDIO_SET_AAC_CONFIG failed. Rc= %d\n",
				__func__, rc);
		break;
	}
	default: {
		pr_debug("Calling utils ioctl\n");
		rc = audio->codec_compat_ioctl(file, cmd, arg);
	}
	}
	return rc;
}
#else
#define audio_compat_ioctl NULL
#endif

static int audio_open(struct inode *inode, struct file *file)
{
	struct q6audio_aio *audio = NULL;
	int rc = 0;
	struct msm_audio_aac_config *aac_config = NULL;

#ifdef CONFIG_DEBUG_FS
	/* 4 bytes represents decoder number, 1 byte for terminate string */
	char name[sizeof "msm_multi_aac_" + 5];
#endif
	audio = kzalloc(sizeof(struct q6audio_aio), GFP_KERNEL);

	if (audio == NULL)
		return -ENOMEM;

	audio->codec_cfg = kzalloc(sizeof(struct msm_audio_aac_config),
					GFP_KERNEL);
	if (audio->codec_cfg == NULL) {
		kfree(audio);
		return -ENOMEM;
	}

	aac_config = audio->codec_cfg;

	audio->pcm_cfg.buffer_size = PCM_BUFSZ_MIN_AACM;
	audio->miscdevice = &audio_multiaac_misc;
	audio->wakelock_voted = false;
	audio->audio_ws_mgr = &audio_multiaac_ws_mgr;
	aac_config->dual_mono_mode = AUDIO_AAC_DUAL_MONO_INVALID;

	audio->ac = q6asm_audio_client_alloc((app_cb) q6_audio_cb,
					     (void *)audio);

	if (!audio->ac) {
		pr_err("Could not allocate memory for audio client\n");
		kfree(audio->codec_cfg);
		kfree(audio);
		return -ENOMEM;
	}
	rc = audio_aio_open(audio, file);
	if (rc < 0) {
		pr_err("%s: audio_aio_open rc=%d\n",
			__func__, rc);
		goto fail;
	}

	/* open in T/NT mode */
	if ((file->f_mode & FMODE_WRITE) && (file->f_mode & FMODE_READ)) {
		rc = q6asm_open_read_write(audio->ac, FORMAT_LINEAR_PCM,
					   FORMAT_MPEG4_MULTI_AAC);
		if (rc < 0) {
			pr_err("NT mode Open failed rc=%d\n", rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->feedback = NON_TUNNEL_MODE;
		/* open AAC decoder, expected frames is always 1
		 * audio->buf_cfg.frames_per_buf = 0x01;
		 */
		audio->buf_cfg.meta_info_enable = 0x01;
	} else if ((file->f_mode & FMODE_WRITE) &&
			!(file->f_mode & FMODE_READ)) {
		rc = q6asm_open_write(audio->ac, FORMAT_MPEG4_MULTI_AAC);
		if (rc < 0) {
			pr_err("T mode Open failed rc=%d\n", rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->feedback = TUNNEL_MODE;
		audio->buf_cfg.meta_info_enable = 0x00;
	} else {
		pr_err("Not supported mode\n");
		rc = -EACCES;
		goto fail;
	}

#ifdef CONFIG_DEBUG_FS
	snprintf(name, sizeof(name), "msm_multi_aac_%04x", audio->ac->session);
	audio->dentry = debugfs_create_file(name, S_IFREG | 0444,
					    NULL, (void *)audio,
					    &audio_aac_debug_fops);

	if (IS_ERR(audio->dentry))
		pr_debug("debugfs_create_file failed\n");
#endif
	pr_info("%s:AAC 5.1 Decoder OPEN success mode[%d]session[%d]\n",
		__func__, audio->feedback, audio->ac->session);
	return rc;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio->codec_cfg);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_aac_fops = {
	.owner = THIS_MODULE,
	.open = audio_open,
	.release = audio_aio_release,
	.unlocked_ioctl = audio_ioctl,
	.fsync = audio_aio_fsync,
	.compat_ioctl = audio_compat_ioctl
};

static struct miscdevice audio_multiaac_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_multi_aac",
	.fops = &audio_aac_fops,
};

static int __init audio_aac_init(void)
{
	int ret = misc_register(&audio_multiaac_misc);

	if (ret == 0)
		device_init_wakeup(audio_multiaac_misc.this_device, true);
	audio_multiaac_ws_mgr.ref_cnt = 0;
	mutex_init(&audio_multiaac_ws_mgr.ws_lock);

	return ret;
}

device_initcall(audio_aac_init);
