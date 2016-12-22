/*
 * Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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

#include <linux/msm_audio.h>
#include <linux/compat.h>
#include "q6audio_common.h"
#include "audio_utils_aio.h"
#include <sound/msm-audio-effects-q6-v2.h>
#include <sound/msm-dts-eagle.h>

#define MAX_CHANNELS_SUPPORTED		8
#define WAIT_TIMEDOUT_DURATION_SECS	1

struct q6audio_effects {
	wait_queue_head_t		read_wait;
	wait_queue_head_t		write_wait;

	struct audio_client             *ac;
	struct msm_hwacc_effects_config  config;

	struct mutex			lock;

	atomic_t			in_count;
	atomic_t			out_count;

	int				opened;
	int				started;
	int				buf_alloc;
	struct msm_nt_eff_all_config audio_effects;
};

static void audio_effects_init_pp(struct audio_client *ac)
{
	int ret = 0;
	struct asm_softvolume_params softvol = {
		.period = SOFT_VOLUME_PERIOD,
		.step = SOFT_VOLUME_STEP,
		.rampingcurve = SOFT_VOLUME_CURVE_LINEAR,
	};

	if (!ac) {
		pr_err("%s: audio client null to init pp\n", __func__);
		return;
	}
	switch (ac->topology) {
	case ASM_STREAM_POSTPROC_TOPO_ID_HPX_MASTER:

		ret = q6asm_set_softvolume_v2(ac, &softvol,
					      SOFT_VOLUME_INSTANCE_1);
		if (ret < 0)
			pr_err("%s: Send SoftVolume1 Param failed ret=%d\n",
				__func__, ret);
		ret = q6asm_set_softvolume_v2(ac, &softvol,
					      SOFT_VOLUME_INSTANCE_2);
		if (ret < 0)
			pr_err("%s: Send SoftVolume2 Param failed ret=%d\n",
				 __func__, ret);

		msm_dts_eagle_init_master_module(ac);

		break;
	default:
		ret = q6asm_set_softvolume_v2(ac, &softvol,
					      SOFT_VOLUME_INSTANCE_1);
		if (ret < 0)
			pr_err("%s: Send SoftVolume Param failed ret=%d\n",
				__func__, ret);
		break;
	}
}

static void audio_effects_deinit_pp(struct audio_client *ac)
{
	if (!ac) {
		pr_err("%s: audio client null to deinit pp\n", __func__);
		return;
	}
	switch (ac->topology) {
	case ASM_STREAM_POSTPROC_TOPO_ID_HPX_MASTER:
		msm_dts_eagle_deinit_master_module(ac);
		break;
	default:
		break;
	}
}

static void audio_effects_event_handler(uint32_t opcode, uint32_t token,
				 uint32_t *payload,  void *priv)
{
	struct q6audio_effects *effects;

	if (!payload || !priv) {
		pr_err("%s: invalid data to handle events, payload: %pK, priv: %pK\n",
			__func__, payload, priv);
		return;
	}

	effects = (struct q6audio_effects *)priv;
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE_V2: {
		atomic_inc(&effects->out_count);
		wake_up(&effects->write_wait);
		break;
	}
	case ASM_DATA_EVENT_READ_DONE_V2: {
		atomic_inc(&effects->in_count);
		wake_up(&effects->read_wait);
		break;
	}
	case APR_BASIC_RSP_RESULT: {
		pr_debug("%s: APR_BASIC_RSP_RESULT Cmd[0x%x] Status[0x%x]\n",
			 __func__, payload[0], payload[1]);
		switch (payload[0]) {
		case ASM_SESSION_CMD_RUN_V2:
			pr_debug("ASM_SESSION_CMD_RUN_V2\n");
			break;
		default:
			pr_debug("%s: Payload = [0x%x] stat[0x%x]\n",
				 __func__, payload[0], payload[1]);
			break;
		}
		break;
	}
	default:
		pr_debug("%s: Unhandled Event 0x%x token = 0x%x\n",
			 __func__, opcode, token);
		break;
	}
}

static int audio_effects_shared_ioctl(struct file *file, unsigned cmd,
				      unsigned long arg)
{
	struct q6audio_effects *effects = file->private_data;
	int rc = 0;
	switch (cmd) {
	case AUDIO_START: {
		pr_debug("%s: AUDIO_START\n", __func__);

		rc = q6asm_open_read_write_v2(effects->ac,
					FORMAT_LINEAR_PCM,
					FORMAT_MULTI_CHANNEL_LINEAR_PCM,
					effects->config.meta_mode_enabled,
					effects->config.output.bits_per_sample,
					true /*overwrite topology*/,
					ASM_STREAM_POSTPROC_TOPO_ID_HPX_MASTER);
		if (rc < 0) {
			pr_err("%s: Open failed for hw accelerated effects:rc=%d\n",
				__func__, rc);
			rc = -EINVAL;
			goto ioctl_fail;
		}
		effects->opened = 1;

		pr_debug("%s: dec buf size: %d, num_buf: %d, enc buf size: %d, num_buf: %d\n",
			 __func__, effects->config.output.buf_size,
			 effects->config.output.num_buf,
			 effects->config.input.buf_size,
			 effects->config.input.num_buf);
		rc = q6asm_audio_client_buf_alloc_contiguous(IN, effects->ac,
					effects->config.output.buf_size,
					effects->config.output.num_buf);
		if (rc < 0) {
			pr_err("%s: Write buffer Allocation failed rc = %d\n",
				__func__, rc);
			rc = -ENOMEM;
			goto ioctl_fail;
		}
		atomic_set(&effects->in_count, effects->config.input.num_buf);
		rc = q6asm_audio_client_buf_alloc_contiguous(OUT, effects->ac,
					effects->config.input.buf_size,
					effects->config.input.num_buf);
		if (rc < 0) {
			pr_err("%s: Read buffer Allocation failed rc = %d\n",
				__func__, rc);
			rc = -ENOMEM;
			goto readbuf_fail;
		}
		atomic_set(&effects->out_count, effects->config.output.num_buf);
		effects->buf_alloc = 1;

		pr_debug("%s: enc: sample_rate: %d, num_channels: %d\n",
			 __func__, effects->config.input.sample_rate,
			effects->config.input.num_channels);
		rc = q6asm_enc_cfg_blk_pcm(effects->ac,
					   effects->config.input.sample_rate,
					   effects->config.input.num_channels);
		if (rc < 0) {
			pr_err("%s: pcm read block config failed\n", __func__);
			rc = -EINVAL;
			goto cfg_fail;
		}
		pr_debug("%s: dec: sample_rate: %d, num_channels: %d, bit_width: %d\n",
			 __func__, effects->config.output.sample_rate,
			effects->config.output.num_channels,
			effects->config.output.bits_per_sample);
		rc = q6asm_media_format_block_pcm_format_support(
				effects->ac, effects->config.output.sample_rate,
				effects->config.output.num_channels,
				effects->config.output.bits_per_sample);
		if (rc < 0) {
			pr_err("%s: pcm write format block config failed\n",
				__func__);
			rc = -EINVAL;
			goto cfg_fail;
		}

		audio_effects_init_pp(effects->ac);

		rc = q6asm_run(effects->ac, 0x00, 0x00, 0x00);
		if (!rc)
			effects->started = 1;
		else {
			effects->started = 0;
			pr_err("%s: ASM run state failed\n", __func__);
		}
		break;
	}
	case AUDIO_EFFECTS_WRITE: {
		char *bufptr = NULL;
		uint32_t idx = 0;
		uint32_t size = 0;

		mutex_lock(&effects->lock);

		if (!effects->started) {
			rc = -EFAULT;
			mutex_unlock(&effects->lock);
			goto ioctl_fail;
		}

		rc = wait_event_timeout(effects->write_wait,
					atomic_read(&effects->out_count),
					WAIT_TIMEDOUT_DURATION_SECS * HZ);
		if (!rc) {
			pr_err("%s: write wait_event_timeout\n", __func__);
			rc = -EFAULT;
			 mutex_unlock(&effects->lock);
			goto ioctl_fail;
		}
		if (!atomic_read(&effects->out_count)) {
			pr_err("%s: pcm stopped out_count 0\n", __func__);
			rc = -EFAULT;
			mutex_unlock(&effects->lock);
			goto ioctl_fail;
		}

		bufptr = q6asm_is_cpu_buf_avail(IN, effects->ac, &size, &idx);
		if (bufptr) {
			if ((effects->config.buf_cfg.output_len > size) ||
				copy_from_user(bufptr, (void *)arg,
					effects->config.buf_cfg.output_len)) {
				rc = -EFAULT;
				mutex_unlock(&effects->lock);
				goto ioctl_fail;
			}
			rc = q6asm_write(effects->ac,
					 effects->config.buf_cfg.output_len,
					 0, 0, NO_TIMESTAMP);
			if (rc < 0) {
				rc = -EFAULT;
				mutex_unlock(&effects->lock);
				goto ioctl_fail;
			}
			atomic_dec(&effects->out_count);
		} else {
			pr_err("%s: AUDIO_EFFECTS_WRITE: Buffer dropped\n",
				__func__);
		}
		mutex_unlock(&effects->lock);
		break;
	}
	case AUDIO_EFFECTS_READ: {
		char *bufptr = NULL;
		uint32_t idx = 0;
		uint32_t size = 0;

		if (!effects->started) {
			rc = -EFAULT;
			goto ioctl_fail;
		}

		atomic_set(&effects->in_count, 0);

		q6asm_read_v2(effects->ac, effects->config.buf_cfg.input_len);
		/* Read might fail initially, don't error out */
		if (rc < 0)
			pr_err("%s: read failed\n", __func__);

		rc = wait_event_timeout(effects->read_wait,
					atomic_read(&effects->in_count),
					WAIT_TIMEDOUT_DURATION_SECS * HZ);
		if (!rc) {
			pr_err("%s: read wait_event_timeout\n", __func__);
			rc = -EFAULT;
			goto ioctl_fail;
		}
		if (!atomic_read(&effects->in_count)) {
			pr_err("%s: pcm stopped in_count 0\n", __func__);
			rc = -EFAULT;
			goto ioctl_fail;
		}

		bufptr = q6asm_is_cpu_buf_avail(OUT, effects->ac, &size, &idx);
		if (bufptr) {
			if (!((void *)arg)) {
				rc = -EFAULT;
				goto ioctl_fail;
			}
			if ((effects->config.buf_cfg.input_len > size) ||
				copy_to_user((void *)arg, bufptr,
					  effects->config.buf_cfg.input_len)) {
				rc = -EFAULT;
				goto ioctl_fail;
			}
		}
		break;
	}
	default:
		pr_err("%s: Invalid effects config module\n", __func__);
		rc = -EINVAL;
		break;
	}
ioctl_fail:
	return rc;
readbuf_fail:
	q6asm_audio_client_buf_free_contiguous(IN,
					effects->ac);
	return rc;
cfg_fail:
	q6asm_audio_client_buf_free_contiguous(IN,
					effects->ac);
	q6asm_audio_client_buf_free_contiguous(OUT,
					effects->ac);
	effects->buf_alloc = 0;
	return rc;
}

static long audio_effects_set_pp_param(struct q6audio_effects *effects,
				long *values)
{
	int rc = 0;
	int effects_module = values[0];
	switch (effects_module) {
	case VIRTUALIZER_MODULE:
		pr_debug("%s: VIRTUALIZER_MODULE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(
			effects_module, effects->ac->topology))
			msm_audio_effects_virtualizer_handler(
				effects->ac,
				&(effects->audio_effects.virtualizer),
				(long *)&values[1]);
		break;
	case REVERB_MODULE:
		pr_debug("%s: REVERB_MODULE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(
			effects_module, effects->ac->topology))
			msm_audio_effects_reverb_handler(effects->ac,
				 &(effects->audio_effects.reverb),
				 (long *)&values[1]);
		break;
	case BASS_BOOST_MODULE:
		pr_debug("%s: BASS_BOOST_MODULE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(
			effects_module, effects->ac->topology))
			msm_audio_effects_bass_boost_handler(
				effects->ac,
				&(effects->audio_effects.bass_boost),
				(long *)&values[1]);
		break;
	case PBE_MODULE:
		pr_debug("%s: PBE_MODULE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(
			effects_module, effects->ac->topology))
			msm_audio_effects_pbe_handler(
				effects->ac,
				&(effects->audio_effects.pbe),
				(long *)&values[1]);
		break;
	case EQ_MODULE:
		pr_debug("%s: EQ_MODULE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(
			effects_module, effects->ac->topology))
			msm_audio_effects_popless_eq_handler(
				effects->ac,
				&(effects->audio_effects.equalizer),
				(long *)&values[1]);
		break;
	case SOFT_VOLUME_MODULE:
		pr_debug("%s: SA PLUS VOLUME_MODULE\n", __func__);
		msm_audio_effects_volume_handler_v2(effects->ac,
				&(effects->audio_effects.saplus_vol),
				(long *)&values[1], SOFT_VOLUME_INSTANCE_1);
		break;
	case SOFT_VOLUME2_MODULE:
		pr_debug("%s: TOPOLOGY SWITCH VOLUME MODULE\n",
			 __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(
			effects_module, effects->ac->topology))
			msm_audio_effects_volume_handler_v2(effects->ac,
			      &(effects->audio_effects.topo_switch_vol),
			      (long *)&values[1], SOFT_VOLUME_INSTANCE_2);
		break;
	case DTS_EAGLE_MODULE_ENABLE:
		pr_debug("%s: DTS_EAGLE_MODULE_ENABLE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(
			effects_module, effects->ac->topology)) {
			/*
			 * HPX->OFF: first disable HPX and then
			 * enable SA+
			 * HPX->ON: first disable SA+ and then
			 * enable HPX
			 */
			bool hpx_state = (bool)values[1];
			if (hpx_state)
				msm_audio_effects_enable_extn(effects->ac,
					&(effects->audio_effects),
					false);
			msm_dts_eagle_enable_asm(effects->ac,
				hpx_state,
				AUDPROC_MODULE_ID_DTS_HPX_PREMIX);
			msm_dts_eagle_enable_asm(effects->ac,
				hpx_state,
				AUDPROC_MODULE_ID_DTS_HPX_POSTMIX);
			if (!hpx_state)
				msm_audio_effects_enable_extn(effects->ac,
					&(effects->audio_effects),
					true);
		}
		break;
	default:
		pr_err("%s: Invalid effects config module\n", __func__);
		rc = -EINVAL;
	}
	return rc;
}

static long audio_effects_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct q6audio_effects *effects = file->private_data;
	int rc = 0;
	long argvalues[MAX_PP_PARAMS_SZ] = {0};

	switch (cmd) {
	case AUDIO_SET_EFFECTS_CONFIG: {
		pr_debug("%s: AUDIO_SET_EFFECTS_CONFIG\n", __func__);
		memset(&effects->config, 0, sizeof(effects->config));
		if (copy_from_user(&effects->config, (void *)arg,
				   sizeof(effects->config))) {
			pr_err("%s: copy from user for AUDIO_SET_EFFECTS_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
		}
		pr_debug("%s: write buf_size: %d, num_buf: %d, sample_rate: %d, channel: %d\n",
			 __func__, effects->config.output.buf_size,
			 effects->config.output.num_buf,
			 effects->config.output.sample_rate,
			 effects->config.output.num_channels);
		pr_debug("%s: read buf_size: %d, num_buf: %d, sample_rate: %d, channel: %d\n",
			 __func__, effects->config.input.buf_size,
			 effects->config.input.num_buf,
			 effects->config.input.sample_rate,
			 effects->config.input.num_channels);
		break;
	}
	case AUDIO_EFFECTS_SET_BUF_LEN: {
		mutex_lock(&effects->lock);
		if (copy_from_user(&effects->config.buf_cfg, (void *)arg,
				   sizeof(effects->config.buf_cfg))) {
			pr_err("%s: copy from user for AUDIO_EFFECTS_SET_BUF_LEN failed\n",
				__func__);
			rc = -EFAULT;
		}
		pr_debug("%s: write buf len: %d, read buf len: %d\n",
			 __func__, effects->config.buf_cfg.output_len,
			 effects->config.buf_cfg.input_len);
		mutex_unlock(&effects->lock);
		break;
	}
	case AUDIO_EFFECTS_GET_BUF_AVAIL: {
		struct msm_hwacc_buf_avail buf_avail;

		buf_avail.input_num_avail = atomic_read(&effects->in_count);
		buf_avail.output_num_avail = atomic_read(&effects->out_count);
		pr_debug("%s: write buf avail: %d, read buf avail: %d\n",
			 __func__, buf_avail.output_num_avail,
			 buf_avail.input_num_avail);
		if (copy_to_user((void *)arg, &buf_avail,
				   sizeof(buf_avail))) {
			pr_err("%s: copy to user for AUDIO_EFFECTS_GET_NUM_BUF_AVAIL failed\n",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_EFFECTS_SET_PP_PARAMS: {
		if (copy_from_user(argvalues, (void *)arg,
				   MAX_PP_PARAMS_SZ*sizeof(long))) {
			pr_err("%s: copy from user for pp params failed\n",
				__func__);
			return -EFAULT;
		}
		rc = audio_effects_set_pp_param(effects, argvalues);
		break;
	}
	default:
		pr_debug("%s: Calling shared ioctl\n", __func__);
		rc = audio_effects_shared_ioctl(file, cmd, arg);
		break;
	}
	if (rc)
		pr_err("%s: cmd 0x%x failed\n", __func__, cmd);
	return rc;
}

#ifdef CONFIG_COMPAT
struct msm_hwacc_data_config32 {
	__u32 buf_size;
	__u32 num_buf;
	__u32 num_channels;
	__u8 channel_map[MAX_CHANNELS_SUPPORTED];
	__u32 sample_rate;
	__u32 bits_per_sample;
};

struct msm_hwacc_buf_cfg32 {
	__u32 input_len;
	__u32 output_len;
};

struct msm_hwacc_buf_avail32 {
	__u32 input_num_avail;
	__u32 output_num_avail;
};

struct msm_hwacc_effects_config32 {
	struct msm_hwacc_data_config32 input;
	struct msm_hwacc_data_config32 output;
	struct msm_hwacc_buf_cfg32 buf_cfg;
	__u32 meta_mode_enabled;
	__u32 overwrite_topology;
	__s32 topology;
};

enum {
	AUDIO_SET_EFFECTS_CONFIG32 = _IOW(AUDIO_IOCTL_MAGIC, 99,
					  struct msm_hwacc_effects_config32),
	AUDIO_EFFECTS_SET_BUF_LEN32 = _IOW(AUDIO_IOCTL_MAGIC, 100,
					   struct msm_hwacc_buf_cfg32),
	AUDIO_EFFECTS_GET_BUF_AVAIL32 = _IOW(AUDIO_IOCTL_MAGIC, 101,
					     struct msm_hwacc_buf_avail32),
	AUDIO_EFFECTS_WRITE32 = _IOW(AUDIO_IOCTL_MAGIC, 102, compat_uptr_t),
	AUDIO_EFFECTS_READ32 = _IOWR(AUDIO_IOCTL_MAGIC, 103, compat_uptr_t),
	AUDIO_EFFECTS_SET_PP_PARAMS32 = _IOW(AUDIO_IOCTL_MAGIC, 104,
					   compat_uptr_t),
	AUDIO_START32 = _IOW(AUDIO_IOCTL_MAGIC, 0, unsigned),
};

static long audio_effects_compat_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	struct q6audio_effects *effects = file->private_data;
	int rc = 0, i;

	switch (cmd) {
	case AUDIO_SET_EFFECTS_CONFIG32: {
		struct msm_hwacc_effects_config32 config32;
		struct msm_hwacc_effects_config *config = &effects->config;
		memset(&effects->config, 0, sizeof(effects->config));
		if (copy_from_user(&config32, (void *)arg,
				   sizeof(config32))) {
			pr_err("%s: copy to user for AUDIO_SET_EFFECTS_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		config->input.buf_size = config32.input.buf_size;
		config->input.num_buf = config32.input.num_buf;
		config->input.num_channels = config32.input.num_channels;
		config->input.sample_rate = config32.input.sample_rate;
		config->input.bits_per_sample = config32.input.bits_per_sample;
		config->input.buf_size = config32.input.buf_size;
		for (i = 0; i < MAX_CHANNELS_SUPPORTED; i++)
			config->input.channel_map[i] =
						config32.input.channel_map[i];
		config->output.buf_size = config32.output.buf_size;
		config->output.num_buf = config32.output.num_buf;
		config->output.num_channels = config32.output.num_channels;
		config->output.sample_rate = config32.output.sample_rate;
		config->output.bits_per_sample =
					 config32.output.bits_per_sample;
		config->output.buf_size = config32.output.buf_size;
		for (i = 0; i < MAX_CHANNELS_SUPPORTED; i++)
			config->output.channel_map[i] =
						config32.output.channel_map[i];
		config->buf_cfg.input_len = config32.buf_cfg.input_len;
		config->buf_cfg.output_len = config32.buf_cfg.output_len;
		config->meta_mode_enabled = config32.meta_mode_enabled;
		config->overwrite_topology = config32.overwrite_topology;
		config->topology = config32.topology;
		pr_debug("%s: write buf_size: %d, num_buf: %d, sample_rate: %d, channels: %d\n",
			 __func__, effects->config.output.buf_size,
			 effects->config.output.num_buf,
			 effects->config.output.sample_rate,
			 effects->config.output.num_channels);
		pr_debug("%s: read buf_size: %d, num_buf: %d, sample_rate: %d, channels: %d\n",
			 __func__, effects->config.input.buf_size,
			 effects->config.input.num_buf,
			 effects->config.input.sample_rate,
			 effects->config.input.num_channels);
		break;
	}
	case AUDIO_EFFECTS_SET_BUF_LEN32: {
		struct msm_hwacc_buf_cfg32 buf_cfg32;
		struct msm_hwacc_effects_config *config = &effects->config;
		if (copy_from_user(&buf_cfg32, (void *)arg,
				   sizeof(buf_cfg32))) {
			pr_err("%s: copy from user for AUDIO_EFFECTS_SET_BUF_LEN failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		config->buf_cfg.input_len = buf_cfg32.input_len;
		config->buf_cfg.output_len = buf_cfg32.output_len;
		pr_debug("%s: write buf len: %d, read buf len: %d\n",
			 __func__, effects->config.buf_cfg.output_len,
			 effects->config.buf_cfg.input_len);
		break;
	}
	case AUDIO_EFFECTS_GET_BUF_AVAIL32: {
		struct msm_hwacc_buf_avail32 buf_avail;

		memset(&buf_avail, 0, sizeof(buf_avail));

		buf_avail.input_num_avail = atomic_read(&effects->in_count);
		buf_avail.output_num_avail = atomic_read(&effects->out_count);
		pr_debug("%s: write buf avail: %d, read buf avail: %d\n",
			 __func__, buf_avail.output_num_avail,
			 buf_avail.input_num_avail);
		if (copy_to_user((void *)arg, &buf_avail,
				   sizeof(buf_avail))) {
			pr_err("%s: copy to user for AUDIO_EFFECTS_GET_NUM_BUF_AVAIL failed\n",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_EFFECTS_SET_PP_PARAMS32: {
		long argvalues[MAX_PP_PARAMS_SZ] = {0};
		int argvalues32[MAX_PP_PARAMS_SZ] = {0};

		if (copy_from_user(argvalues32, (void *)arg,
				   MAX_PP_PARAMS_SZ*sizeof(int))) {
			pr_err("%s: copy from user failed for pp params\n",
				__func__);
			return -EFAULT;
		}
		for (i = 0; i < MAX_PP_PARAMS_SZ; i++)
			argvalues[i] = argvalues32[i];

		rc = audio_effects_set_pp_param(effects, argvalues);
		break;
	}
	case AUDIO_START32: {
		rc = audio_effects_shared_ioctl(file, AUDIO_START, arg);
		break;
	}
	case AUDIO_EFFECTS_WRITE32: {
		rc = audio_effects_shared_ioctl(file, AUDIO_EFFECTS_WRITE, arg);
		break;
	}
	case AUDIO_EFFECTS_READ32: {
		rc = audio_effects_shared_ioctl(file, AUDIO_EFFECTS_READ, arg);
		break;
	}
	default:
		pr_debug("%s: unhandled ioctl\n", __func__);
		rc = -EINVAL;
		break;
	}
	return rc;
}
#endif

static int audio_effects_release(struct inode *inode, struct file *file)
{
	struct q6audio_effects *effects = file->private_data;
	int rc = 0;
	if (!effects) {
		pr_err("%s: effect is NULL\n", __func__);
		return -EINVAL;
	}
	if (effects->opened) {
		rc = wait_event_timeout(effects->write_wait,
					atomic_read(&effects->out_count),
					WAIT_TIMEDOUT_DURATION_SECS * HZ);
		if (!rc)
			pr_err("%s: write wait_event_timeout failed\n",
				__func__);
		rc = wait_event_timeout(effects->read_wait,
					atomic_read(&effects->in_count),
					WAIT_TIMEDOUT_DURATION_SECS * HZ);
		if (!rc)
			pr_err("%s: read wait_event_timeout failed\n",
				__func__);
		rc = q6asm_cmd(effects->ac, CMD_CLOSE);
		if (rc < 0)
			pr_err("%s[%pK]:Failed to close the session rc=%d\n",
				__func__, effects, rc);
		effects->opened = 0;
		effects->started = 0;

		audio_effects_deinit_pp(effects->ac);
	}

	if (effects->buf_alloc) {
		q6asm_audio_client_buf_free_contiguous(IN, effects->ac);
		q6asm_audio_client_buf_free_contiguous(OUT, effects->ac);
	}
	q6asm_audio_client_free(effects->ac);

	mutex_destroy(&effects->lock);
	kfree(effects);

	pr_debug("%s: close session success\n", __func__);
	return rc;
}

static int audio_effects_open(struct inode *inode, struct file *file)
{
	struct q6audio_effects *effects;
	int rc = 0;

	effects = kzalloc(sizeof(struct q6audio_effects), GFP_KERNEL);
	if (!effects) {
		pr_err("%s: Could not allocate memory for hw acc effects driver\n",
			__func__);
		return -ENOMEM;
	}

	effects->ac = q6asm_audio_client_alloc(
					(app_cb)audio_effects_event_handler,
					(void *)effects);
	if (!effects->ac) {
		pr_err("%s: Could not allocate memory for audio client\n",
			__func__);
		kfree(effects);
		return -ENOMEM;
	}

	init_waitqueue_head(&effects->read_wait);
	init_waitqueue_head(&effects->write_wait);
	mutex_init(&effects->lock);

	effects->opened = 0;
	effects->started = 0;
	effects->buf_alloc = 0;
	file->private_data = effects;
	pr_debug("%s: open session success\n", __func__);
	return rc;
}

static const struct file_operations audio_effects_fops = {
	.owner = THIS_MODULE,
	.open = audio_effects_open,
	.release = audio_effects_release,
	.unlocked_ioctl = audio_effects_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = audio_effects_compat_ioctl,
#endif
};

struct miscdevice audio_effects_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_hweffects",
	.fops = &audio_effects_fops,
};

static int __init audio_effects_init(void)
{
	return misc_register(&audio_effects_misc);
}

device_initcall(audio_effects_init);
MODULE_DESCRIPTION("Audio hardware accelerated effects driver");
MODULE_LICENSE("GPL v2");
