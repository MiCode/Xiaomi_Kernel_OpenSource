/*
 *  sst_drv_interface.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corp
 *  Authors:	Vinod Koul <vinod.koul@intel.com>
 *		Harsha Priya <priya.harsha@intel.com>
 *		Dharageswari R <dharageswari.r@intel.com)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
 * This file defines the interface  between the platform driver and the SST
 * driver.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/firmware.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/math64.h>
#include <linux/intel_mid_pm.h>
#include <sound/compress_offload.h>
#include <sound/pcm.h>
#include <sound/intel_sst_ioctl.h>
#include "../sst_platform.h"
#include "../platform_ipc_v2.h"
#include "sst.h"

#define NUM_CODEC 2
#define MIN_FRAGMENT 2
#define MAX_FRAGMENT 4
#define MIN_FRAGMENT_SIZE (50 * 1024)
#define MAX_FRAGMENT_SIZE (1024 * 1024)
#define SST_GET_BYTES_PER_SAMPLE(pcm_wd_sz)  (((pcm_wd_sz + 15) >> 4) << 1)

void sst_restore_fw_context(void)
{
	struct snd_sst_ctxt_params fw_context;
	struct ipc_post *msg = NULL;
	int retval = 0;
	struct sst_block *block;

	/* Skip the context restore, when fw_clear_context is set */
	/* fw_clear_context set through debugfs support */
	if (atomic_read(&sst_drv_ctx->fw_clear_context)) {
		pr_debug("Skipping restore_fw_context\n");
		atomic_set(&sst_drv_ctx->fw_clear_context, 0);
		return;
	}

	pr_debug("restore_fw_context\n");
	/*nothing to restore*/
	if (!sst_drv_ctx->fw_cntx_size)
		return;
	pr_debug("restoring context......\n");
	/*send msg to fw*/
	retval = sst_create_block_and_ipc_msg(&msg, true, sst_drv_ctx, &block,
			IPC_IA_SET_FW_CTXT, 0);
	if (retval) {
		pr_err("Can't allocate block/msg. No restore fw_context\n");
		return;
	}

	sst_fill_header(&msg->header, IPC_IA_SET_FW_CTXT, 1, 0);

	msg->header.part.data = sizeof(fw_context) + sizeof(u32);
	fw_context.address = virt_to_phys((void *)sst_drv_ctx->fw_cntx);
	fw_context.size = sst_drv_ctx->fw_cntx_size;
	memcpy(msg->mailbox_data, &msg->header, sizeof(u32));
	memcpy(msg->mailbox_data + sizeof(u32),
				&fw_context, sizeof(fw_context));
	sst_add_to_dispatch_list_and_post(sst_drv_ctx, msg);
	retval = sst_wait_timeout(sst_drv_ctx, block);
	sst_free_block(sst_drv_ctx, block);
	if (retval)
		pr_err("sst_restore_fw_context..timeout!\n");
	return;
}

/*
 * sst_download_fw - download the audio firmware to DSP
 *
 * This function is called when the FW needs to be downloaded to SST DSP engine
 */
int sst_download_fw(void)
{
	int retval = 0;

	retval = sst_load_fw();
	if (retval)
		return retval;
	pr_debug("fw loaded successful!!!\n");

	if (sst_drv_ctx->ops->restore_dsp_context)
		sst_drv_ctx->ops->restore_dsp_context();
	sst_drv_ctx->sst_state = SST_FW_RUNNING;
	return retval;
}

int free_stream_context(unsigned int str_id)
{
	struct stream_info *stream;
	int ret = 0;

	stream = get_stream_info(str_id);
	if (stream) {
		/* str_id is valid, so stream is alloacted */
		ret = sst_free_stream(str_id);
		if (ret)
			sst_clean_stream(&sst_drv_ctx->streams[str_id]);
		return ret;
	}
	return ret;
}

/*
 * sst_send_algo_param - send LPE Mixer param to SST
 *
 * this function sends the algo parameter to sst dsp engine
 */
static int sst_send_algo_param(struct snd_ppp_params *algo_params)
{
	u32 header_size = 0;
	struct ipc_post *msg = NULL;
	u32 ipc_msg_size = sizeof(u32) + sizeof(*algo_params)
			 - sizeof(algo_params->params) + algo_params->size;
	u32 offset = 0;

	if (ipc_msg_size > sst_drv_ctx->mailbox_size)
		return -ENOMEM;
	if (sst_create_ipc_msg(&msg, true))
		return -ENOMEM;
	sst_fill_header(&msg->header,
			IPC_IA_ALG_PARAMS, 1, algo_params->str_id);
	msg->header.part.data = sizeof(u32) + sizeof(*algo_params)
			 - sizeof(algo_params->params) + algo_params->size;
	memcpy(msg->mailbox_data, &msg->header, sizeof(u32));
	offset = sizeof(u32);
	header_size = sizeof(*algo_params) - sizeof(algo_params->params);
	memcpy(msg->mailbox_data + sizeof(u32), algo_params,
		sizeof(*algo_params) - sizeof(algo_params->params));
	offset += header_size;
	memcpy(msg->mailbox_data + offset , algo_params->params,
			algo_params->size);
	sst_add_to_dispatch_list_and_post(sst_drv_ctx, msg);
	return 0;
}

static int sst_send_lpe_mixer_algo_params(void)
{
	struct snd_ppp_params algo_param;
	struct snd_ppp_mixer_params mixer_param;
	unsigned int input_mixer, stream_device_id;
	int retval = 0;

	retval = intel_sst_check_device();
	if (retval) {
		pr_err("sst_check_device failed %d\n", retval);
		return retval;
	}

	mutex_lock(&sst_drv_ctx->mixer_ctrl_lock);
	input_mixer = (sst_drv_ctx->device_input_mixer)
				& SST_INPUT_STREAM_MIXED;
	pr_debug("Input Mixer settings %d", input_mixer);
	stream_device_id = sst_drv_ctx->device_input_mixer - input_mixer;
	algo_param.algo_id = SST_ALGO_MIXER;
	algo_param.str_id = stream_device_id;
	algo_param.enable = 1;
	algo_param.operation = SST_SET_ALGO;
	algo_param.size = sizeof(mixer_param);
	mixer_param.type = SST_ALGO_PARAM_MIXER_STREAM_CFG;
	mixer_param.input_stream_bitmap = input_mixer;
	mixer_param.size = sizeof(input_mixer);
	algo_param.params = &mixer_param;
	mutex_unlock(&sst_drv_ctx->mixer_ctrl_lock);
	pr_debug("setting pp param\n");
	pr_debug("Algo ID %d Str id %d Enable %d Size %d\n",
			algo_param.algo_id, algo_param.str_id,
			algo_param.enable, algo_param.size);
	sst_send_algo_param(&algo_param);
	sst_pm_runtime_put(sst_drv_ctx);
	return retval;
}

/*
 * sst_get_stream_allocated - this function gets a stream allocated with
 * the given params
 *
 * @str_param : stream params
 * @lib_dnld : pointer to pointer of lib downlaod struct
 *
 * This creates new stream id for a stream, in case lib is to be downloaded to
 * DSP, it downloads that
 */
int sst_get_stream_allocated(struct snd_sst_params *str_param,
		struct snd_sst_lib_download **lib_dnld)
{
	int retval, str_id;
	struct sst_block *block;
	struct snd_sst_alloc_response *response;
	struct stream_info *str_info;

	pr_debug("In %s\n", __func__);
	block = sst_create_block(sst_drv_ctx, 0, 0);
	if (block == NULL)
		return -ENOMEM;

	retval = sst_drv_ctx->ops->alloc_stream((char *) str_param, block);
	str_id = retval;
	if (retval < 0) {
		pr_err("sst_alloc_stream failed %d\n", retval);
		goto free_block;
	}
	pr_debug("Stream allocated %d\n", retval);
	str_info = get_stream_info(str_id);
	if (str_info == NULL) {
		pr_err("get stream info returned null\n");
		str_id = -EINVAL;
		goto free_block;
	}

	/* Block the call for reply */
	retval = sst_wait_timeout(sst_drv_ctx, block);
	if (block->data) {
		response = (struct snd_sst_alloc_response *)block->data;
		retval = response->str_type.result;
		if (!retval)
			goto free_block;

		pr_err("sst: FW alloc failed retval %d\n", retval);
		if (retval == SST_ERR_STREAM_IN_USE) {
			pr_err("sst:FW not in clean state, send free for:%d\n",
					str_id);
			sst_free_stream(str_id);
			*lib_dnld = NULL;
		}
		if (retval == SST_LIB_ERR_LIB_DNLD_REQUIRED) {
			*lib_dnld = kzalloc(sizeof(**lib_dnld), GFP_KERNEL);
			if (*lib_dnld == NULL) {
				str_id = -ENOMEM;
				goto free_block;
			}
			memcpy(*lib_dnld, &response->lib_dnld, sizeof(**lib_dnld));
			sst_clean_stream(str_info);
		} else {
			*lib_dnld = NULL;
		}
		str_id = -retval;
	} else if (retval != 0) {
		pr_err("sst: FW alloc failed retval %d\n", retval);
		/* alloc failed, so reset the state to uninit */
		str_info->status = STREAM_UN_INIT;
		str_id = retval;
	}
free_block:
	sst_free_block(sst_drv_ctx, block);
	return str_id; /*will ret either error (in above if) or correct str id*/
}

/*
 * sst_get_sfreq - this function returns the frequency of the stream
 *
 * @str_param : stream params
 */
int sst_get_sfreq(struct snd_sst_params *str_param)
{
	switch (str_param->codec) {
	case SST_CODEC_TYPE_PCM:
		return str_param->sparams.uc.pcm_params.sfreq;
	case SST_CODEC_TYPE_AAC:
		return str_param->sparams.uc.aac_params.externalsr;
	case SST_CODEC_TYPE_MP3:
		return 0;
	default:
		return -EINVAL;
	}
}

/*
 * sst_get_sfreq - this function returns the frequency of the stream
 *
 * @str_param : stream params
 */
int sst_get_num_channel(struct snd_sst_params *str_param)
{
	switch (str_param->codec) {
	case SST_CODEC_TYPE_PCM:
		return str_param->sparams.uc.pcm_params.num_chan;
	case SST_CODEC_TYPE_MP3:
		return str_param->sparams.uc.mp3_params.num_chan;
	case SST_CODEC_TYPE_AAC:
		return str_param->sparams.uc.aac_params.num_chan;
	default:
		return -EINVAL;
	}
}

/*
 * sst_get_stream - this function prepares for stream allocation
 *
 * @str_param : stream param
 */
int sst_get_stream(struct snd_sst_params *str_param)
{
	int retval;
	struct stream_info *str_info;
	struct snd_sst_lib_download *lib_dnld;

	pr_debug("In %s\n", __func__);
	/* stream is not allocated, we are allocating */
	retval = sst_get_stream_allocated(str_param, &lib_dnld);

	if (retval == -(SST_LIB_ERR_LIB_DNLD_REQUIRED)) {
		/* codec download is required */

		pr_debug("Codec is required.... trying that\n");
		if (lib_dnld == NULL) {
			pr_err("lib download null!!! abort\n");
			return -EIO;
		}

		retval = sst_load_library(lib_dnld, str_param->ops);
		kfree(lib_dnld);

		if (!retval) {
			pr_debug("codec was downloaded successfully\n");

			retval = sst_get_stream_allocated(str_param, &lib_dnld);
			if (retval <= 0) {
				retval = -EIO;
				goto err;
			}

			pr_debug("Alloc done stream id %d\n", retval);
		} else {
			pr_debug("codec download failed\n");
			retval = -EIO;
			goto err;
		}
	} else if  (retval <= 0) {
		retval = -EIO;
		goto err;
	}
	/* store sampling freq */
	str_info = &sst_drv_ctx->streams[retval];
	str_info->sfreq = sst_get_sfreq(str_param);

err:
	return retval;
}

/**
* intel_sst_check_device - checks SST device
*
* This utility function checks the state of SST device and downlaods FW if
* not done, or resumes the device if suspended
*/
int intel_sst_check_device(void)
{
	int retval = 0;

	pr_debug("In %s\n", __func__);

	mutex_lock(&sst_drv_ctx->sst_lock);
	if (sst_drv_ctx->sst_suspend_state) {
		pr_err("%s:SST is in suspended state; Returning\n", __func__);
		mutex_unlock(&sst_drv_ctx->sst_lock);
		return -ENODEV;
	}
	mutex_unlock(&sst_drv_ctx->sst_lock);

	pm_runtime_get_sync(sst_drv_ctx->dev);
	atomic_inc(&sst_drv_ctx->pm_usage_count);

	pr_debug("%s: count is %d now\n", __func__,
				atomic_read(&sst_drv_ctx->pm_usage_count));

	mutex_lock(&sst_drv_ctx->sst_lock);

	if (sst_drv_ctx->sst_state == SST_RECOVERY) {
		pr_info("LPE is in recovery state\n");
		mutex_unlock(&sst_drv_ctx->sst_lock);
		sst_pm_runtime_put(sst_drv_ctx);
		return -EAGAIN;
	}

	if (sst_drv_ctx->sst_state == SST_RESET) {

		/* FW is not downloaded */
		pr_debug("DSP Downloading FW now...\n");
		retval = sst_download_fw();
		if (retval) {
			pr_err("FW download fail %x\n", retval);
			sst_drv_ctx->sst_state = SST_RESET;
			mutex_unlock(&sst_drv_ctx->sst_lock);
			sst_pm_runtime_put(sst_drv_ctx);
			return retval;
		}
	}
	mutex_unlock(&sst_drv_ctx->sst_lock);
	return retval;
}

void sst_process_mad_ops(struct work_struct *work)
{

	struct mad_ops_wq *mad_ops =
			container_of(work, struct mad_ops_wq, wq);
	int retval = 0;

	switch (mad_ops->control_op) {
	case SST_SND_PAUSE:
		retval = sst_pause_stream(mad_ops->stream_id);
		break;
	case SST_SND_RESUME:
		retval = sst_resume_stream(mad_ops->stream_id);
		break;
	default:
		pr_err(" wrong control_ops reported\n");
	}
	if (retval)
		pr_err("%s(): op: %d, retval: %d\n",
				__func__, mad_ops->control_op, retval);
	kfree(mad_ops);
	return;
}

static int sst_power_control(bool state)
{
	pr_debug("%s for %d", __func__, state);

	/* should we do ref count here, or rely on pcm handle?? */
	if (state == true)
		return intel_sst_check_device();
	else
		return sst_pm_runtime_put(sst_drv_ctx);
}
/*
 * sst_open_pcm_stream - Open PCM interface
 *
 * @str_param: parameters of pcm stream
 *
 * This function is called by MID sound card driver to open
 * a new pcm interface
 */
static int sst_open_pcm_stream(struct snd_sst_params *str_param)
{
	int retval;

	if (!str_param)
		return -EINVAL;

	pr_debug("%s: doing rtpm_get\n", __func__);

	retval = intel_sst_check_device();

	if (retval)
		return retval;
	retval = sst_get_stream(str_param);
	if (retval > 0) {
		sst_drv_ctx->stream_cnt++;
	} else {
		pr_err("sst_get_stream returned err %d\n", retval);
		sst_pm_runtime_put(sst_drv_ctx);
	}

	return retval;
}

static int sst_cdev_open(struct snd_sst_params *str_params,
		struct sst_compress_cb *cb)
{
	int str_id, retval;
	struct stream_info *stream;

	pr_debug("%s: doing rtpm_get\n", __func__);

	retval = intel_sst_check_device();
	if (retval)
		return retval;

	str_id = sst_get_stream(str_params);
	if (str_id > 0) {
		pr_debug("stream allocated in sst_cdev_open %d\n", str_id);
		stream = &sst_drv_ctx->streams[str_id];
		stream->compr_cb = cb->compr_cb;
		stream->compr_cb_param = cb->param;
		stream->drain_notify = cb->drain_notify;
		stream->drain_cb_param = cb->drain_cb_param;
	} else {
		pr_err("stream encountered error during alloc %d\n", str_id);
		str_id = -EINVAL;
		sst_pm_runtime_put(sst_drv_ctx);
	}
	return str_id;
}

static int sst_cdev_close(unsigned int str_id)
{
	int retval;
	struct stream_info *stream;

	pr_debug("%s: Entry\n", __func__);
	stream = get_stream_info(str_id);
	if (!stream) {
		pr_err("stream info is NULL for str %d!!!\n", str_id);
		return -EINVAL;
	}

	if (stream->status == STREAM_RESET) {
		/* silently fail here as we have cleaned the stream */
		pr_debug("stream in reset state...\n");
		stream->status = STREAM_UN_INIT;

		retval = 0;
		goto put;
	}

	retval = sst_free_stream(str_id);
put:
	stream->compr_cb_param = NULL;
	stream->compr_cb = NULL;

	/* The free_stream will return a error if there is no stream to free,
	(i.e. the alloc failure case). And in this case the open does a put in
	the error scenario, so skip in this case.
		In the close we need to handle put in the success scenario and
	the timeout error(EBUSY) scenario. */
	if (!retval || (retval == -EBUSY))
		sst_pm_runtime_put(sst_drv_ctx);
	else
		pr_err("%s: free stream returned err %d\n", __func__, retval);

	pr_debug("%s: End\n", __func__);
	return retval;

}

static int sst_cdev_ack(unsigned int str_id, unsigned long bytes)
{
	struct stream_info *stream;
	struct snd_sst_tstamp fw_tstamp = {0,};
	int offset;
	void __iomem *addr;

	pr_debug("sst:  ackfor %d\n", str_id);
	stream = get_stream_info(str_id);
	if (!stream)
		return -EINVAL;

	/* update bytes sent */
	stream->cumm_bytes += bytes;
	pr_debug("bytes copied %d inc by %ld\n", stream->cumm_bytes, bytes);

	memcpy_fromio(&fw_tstamp,
		((void *)(sst_drv_ctx->mailbox + sst_drv_ctx->tstamp)
		+(str_id * sizeof(fw_tstamp))),
		sizeof(fw_tstamp));

	fw_tstamp.bytes_copied = stream->cumm_bytes;
	pr_debug("bytes sent to fw %llu inc by %ld\n", fw_tstamp.bytes_copied,
							 bytes);

	addr =  ((void *)(sst_drv_ctx->mailbox + sst_drv_ctx->tstamp)) +
			(str_id * sizeof(fw_tstamp));
	offset =  offsetof(struct snd_sst_tstamp, bytes_copied);
	sst_shim_write(addr, offset, fw_tstamp.bytes_copied);
	return 0;

}

static int sst_cdev_set_metadata(unsigned int str_id,
				struct snd_compr_metadata *metadata)
{
	int retval = 0, pvt_id, len;
	struct ipc_post *msg = NULL;
	struct stream_info *str_info;
	struct ipc_dsp_hdr dsp_hdr;

	pr_debug("set metadata for stream %d\n", str_id);

	str_info = get_stream_info(str_id);
	if (!str_info)
		return -EINVAL;

	if (sst_create_ipc_msg(&msg, 1))
		return -ENOMEM;

	if (!sst_drv_ctx->use_32bit_ops) {
		pvt_id = sst_assign_pvt_id(sst_drv_ctx);
		pr_debug("pvt id = %d\n", pvt_id);
		pr_debug("pipe id = %d\n", str_info->pipe_id);
		sst_fill_header_mrfld(&msg->mrfld_header,
			IPC_CMD, str_info->task_id, 1, pvt_id);

		len = sizeof(*metadata) + sizeof(dsp_hdr);
		msg->mrfld_header.p.header_low_payload = len;
		sst_fill_header_dsp(&dsp_hdr, IPC_IA_SET_STREAM_PARAMS_MRFLD,
				str_info->pipe_id, sizeof(*metadata));
		memcpy(msg->mailbox_data, &dsp_hdr, sizeof(dsp_hdr));
		memcpy(msg->mailbox_data + sizeof(dsp_hdr),
				metadata, sizeof(*metadata));
	} else {
		sst_fill_header(&msg->header, IPC_IA_SET_STREAM_PARAMS,
					1, str_id);
		msg->header.part.data = sizeof(u32) + sizeof(*metadata);
		memcpy(msg->mailbox_data, &msg->header, sizeof(u32));
		memcpy(msg->mailbox_data + sizeof(u32),
				metadata, sizeof(*metadata));
	}

	sst_drv_ctx->ops->sync_post_message(msg);
	return retval;
}

static int sst_cdev_control(unsigned int cmd, unsigned int str_id)
{
	pr_debug("recieved cmd %d on stream %d\n", cmd, str_id);

	if (sst_drv_ctx->sst_state != SST_FW_RUNNING)
		return 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return sst_pause_stream(str_id);
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		return sst_resume_stream(str_id);
	case SNDRV_PCM_TRIGGER_START: {
		struct stream_info *str_info;
		str_info = get_stream_info(str_id);
		if (!str_info)
			return -EINVAL;
		str_info->prev = str_info->status;
		str_info->status = STREAM_RUNNING;
		return sst_start_stream(str_id);
	}
	case SNDRV_PCM_TRIGGER_STOP:
		return sst_drop_stream(str_id);
	case SND_COMPR_TRIGGER_DRAIN:
		return sst_drain_stream(str_id, false);
	case SND_COMPR_TRIGGER_NEXT_TRACK:
		return sst_next_track();
	case SND_COMPR_TRIGGER_PARTIAL_DRAIN:
		return sst_drain_stream(str_id, true);
	default:
		return -EINVAL;
	}
}

static int sst_cdev_tstamp(unsigned int str_id, struct snd_compr_tstamp *tstamp)
{
	struct snd_sst_tstamp fw_tstamp = {0,};
	struct stream_info *stream;

	memcpy_fromio(&fw_tstamp,
		((void *)(sst_drv_ctx->mailbox + sst_drv_ctx->tstamp)
		+(str_id * sizeof(fw_tstamp))),
		sizeof(fw_tstamp));

	stream = get_stream_info(str_id);
	if (!stream)
		return -EINVAL;
	pr_debug("rb_counter %llu in bytes\n", fw_tstamp.ring_buffer_counter);

	tstamp->copied_total = fw_tstamp.ring_buffer_counter;
	tstamp->pcm_frames = fw_tstamp.frames_decoded;
	tstamp->pcm_io_frames = div_u64(fw_tstamp.hardware_counter,
			(u64)((stream->num_ch) * SST_GET_BYTES_PER_SAMPLE(24)));
	tstamp->sampling_rate = fw_tstamp.sampling_frequency;
	pr_debug("PCM  = %u\n", tstamp->pcm_io_frames);
	pr_debug("Pointer Query on strid = %d  copied_total %d, decodec %d\n",
		str_id, tstamp->copied_total, tstamp->pcm_frames);
	pr_debug("rendered %d\n", tstamp->pcm_io_frames);
	return 0;
}

static int sst_cdev_caps(struct snd_compr_caps *caps)
{
	caps->num_codecs = NUM_CODEC;
	caps->min_fragment_size = MIN_FRAGMENT_SIZE;  /* 50KB */
	caps->max_fragment_size = MAX_FRAGMENT_SIZE;  /* 1024KB */
	caps->min_fragments = MIN_FRAGMENT;
	caps->max_fragments = MAX_FRAGMENT;
	caps->codecs[0] = SND_AUDIOCODEC_MP3;
	caps->codecs[1] = SND_AUDIOCODEC_AAC;
	return 0;
}

static int sst_cdev_codec_caps(struct snd_compr_codec_caps *codec)
{

	if (codec->codec == SND_AUDIOCODEC_MP3) {
		codec->num_descriptors = 2;
		codec->descriptor[0].max_ch = 2;
		codec->descriptor[1].sample_rates[0] = SNDRV_PCM_RATE_8000;
		codec->descriptor[1].sample_rates[1] = SNDRV_PCM_RATE_11025;
		codec->descriptor[1].sample_rates[2] = SNDRV_PCM_RATE_16000;
		codec->descriptor[1].sample_rates[3] = SNDRV_PCM_RATE_22050;
		codec->descriptor[1].sample_rates[4] = SNDRV_PCM_RATE_32000;
		codec->descriptor[1].sample_rates[5] = SNDRV_PCM_RATE_44100;
		codec->descriptor[1].sample_rates[6] = SNDRV_PCM_RATE_48000;
		codec->descriptor[0].bit_rate[0] = 320; /* 320kbps */
		codec->descriptor[0].bit_rate[1] = 192;
		codec->descriptor[0].num_bitrates = 2;
		codec->descriptor[0].profiles = 0;
		codec->descriptor[0].modes = SND_AUDIOCHANMODE_MP3_STEREO;
		codec->descriptor[0].formats = 0;
	} else if (codec->codec == SND_AUDIOCODEC_AAC) {
		codec->num_descriptors = 2;
		codec->descriptor[1].max_ch = 2;
		codec->descriptor[1].sample_rates[0] = SNDRV_PCM_RATE_8000;
		codec->descriptor[1].sample_rates[1] = SNDRV_PCM_RATE_11025;
		codec->descriptor[1].sample_rates[2] = SNDRV_PCM_RATE_16000;
		codec->descriptor[1].sample_rates[3] = SNDRV_PCM_RATE_22050;
		codec->descriptor[1].sample_rates[4] = SNDRV_PCM_RATE_32000;
		codec->descriptor[1].sample_rates[5] = SNDRV_PCM_RATE_44100;
		codec->descriptor[1].sample_rates[6] = SNDRV_PCM_RATE_48000;
		codec->descriptor[1].bit_rate[0] = 320; /* 320kbps */
		codec->descriptor[1].bit_rate[1] = 192;
		codec->descriptor[1].num_bitrates = 2;
		codec->descriptor[1].profiles = 0;
		codec->descriptor[1].modes = 0;
		codec->descriptor[1].formats =
			(SND_AUDIOSTREAMFORMAT_MP4ADTS |
				SND_AUDIOSTREAMFORMAT_RAW);
	} else {
		return -EINVAL;
	}

	return 0;
}

void sst_cdev_fragment_elapsed(int str_id)
{
	struct stream_info *stream;

	pr_debug("fragment elapsed from firmware for str_id %d\n", str_id);
	stream = &sst_drv_ctx->streams[str_id];
	if (stream->compr_cb)
		stream->compr_cb(stream->compr_cb_param);
}

/*
 * sst_close_pcm_stream - Close PCM interface
 *
 * @str_id: stream id to be closed
 *
 * This function is called by MID sound card driver to close
 * an existing pcm interface
 */
static int sst_close_pcm_stream(unsigned int str_id)
{
	struct stream_info *stream;
	int retval = 0;

	pr_debug("%s: Entry\n", __func__);
	stream = get_stream_info(str_id);
	if (!stream) {
		pr_err("stream info is NULL for str %d!!!\n", str_id);
		return -EINVAL;
	}

	if (stream->status == STREAM_RESET) {
		/* silently fail here as we have cleaned the stream */
		pr_debug("stream in reset state...\n");

		retval = 0;
		goto put;
	}

	retval = free_stream_context(str_id);
put:
	stream->pcm_substream = NULL;
	stream->status = STREAM_UN_INIT;
	stream->period_elapsed = NULL;
	sst_drv_ctx->stream_cnt--;

	/* The free_stream will return a error if there is no stream to free,
	(i.e. the alloc failure case). And in this case the open does a put in
	the error scenario, so skip in this case.
		In the close we need to handle put in the success scenario and
	the timeout error(EBUSY) scenario. */
	if (!retval || (retval == -EBUSY))
		sst_pm_runtime_put(sst_drv_ctx);
	else
		pr_err("%s: free stream returned err %d\n", __func__, retval);

	pr_debug("%s: Exit\n", __func__);
	return 0;
}

int sst_send_sync_msg(int ipc, int str_id)
{
	struct ipc_post *msg = NULL;

	if (sst_create_ipc_msg(&msg, false))
		return -ENOMEM;
	sst_fill_header(&msg->header, ipc, 0, str_id);
	return sst_drv_ctx->ops->sync_post_message(msg);
}

static inline int sst_calc_tstamp(struct pcm_stream_info *info,
		struct snd_pcm_substream *substream,
		struct snd_sst_tstamp *fw_tstamp)
{
	size_t delay_bytes, delay_frames;
	size_t buffer_sz;
	u32 pointer_bytes, pointer_samples;

	pr_debug("mrfld ring_buffer_counter %llu in bytes\n",
			fw_tstamp->ring_buffer_counter);
	pr_debug("mrfld hardware_counter %llu in bytes\n",
			 fw_tstamp->hardware_counter);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		delay_bytes = (size_t) (fw_tstamp->ring_buffer_counter -
					fw_tstamp->hardware_counter);
	else
		delay_bytes = (size_t) (fw_tstamp->hardware_counter -
					fw_tstamp->ring_buffer_counter);
	delay_frames = bytes_to_frames(substream->runtime, delay_bytes);
	buffer_sz = snd_pcm_lib_buffer_bytes(substream);
	div_u64_rem(fw_tstamp->ring_buffer_counter, buffer_sz, &pointer_bytes);
	pointer_samples = bytes_to_samples(substream->runtime, pointer_bytes);

	pr_debug("pcm delay %zu in bytes\n", delay_bytes);

	info->buffer_ptr = pointer_samples / substream->runtime->channels;

	info->pcm_delay = delay_frames / substream->runtime->channels;
	pr_debug("buffer ptr %llu pcm_delay rep: %llu\n",
			info->buffer_ptr, info->pcm_delay);
	return 0;
}

static int sst_read_timestamp(struct pcm_stream_info *info)
{
	struct stream_info *stream;
	struct snd_pcm_substream *substream;
	struct snd_sst_tstamp fw_tstamp;
	unsigned int str_id;

	str_id = info->str_id;
	stream = get_stream_info(str_id);
	if (!stream)
		return -EINVAL;

	if (!stream->pcm_substream)
		return -EINVAL;
	substream = stream->pcm_substream;

	memcpy_fromio(&fw_tstamp,
		((void *)(sst_drv_ctx->mailbox + sst_drv_ctx->tstamp)
			+ (str_id * sizeof(fw_tstamp))),
		sizeof(fw_tstamp));
	return sst_calc_tstamp(info, substream, &fw_tstamp);
}

/*
 * sst_device_control - Set Control params
 *
 * @cmd: control cmd to be set
 * @arg: command argument
 *
 * This function is called by MID sound card driver to set
 * SST/Sound card controls for an opened stream.
 * This is registered with MID driver
 */
static int sst_device_control(int cmd, void *arg)
{
	int retval = 0, str_id = 0;


	if (sst_drv_ctx->sst_state != SST_FW_RUNNING)
		return 0;

	switch (cmd) {
	case SST_SND_PAUSE:
	case SST_SND_RESUME: {
		struct mad_ops_wq *work = kzalloc(sizeof(*work), GFP_ATOMIC);
		if (!work)
			return -ENOMEM;
		INIT_WORK(&work->wq, sst_process_mad_ops);
		work->control_op = cmd;
		work->stream_id = *(int *)arg;
		queue_work(sst_drv_ctx->mad_wq, &work->wq);
		break;
	}
	case SST_SND_START: {
		struct stream_info *str_info;
		int ipc;
		str_id = *(int *)arg;
		str_info = get_stream_info(str_id);
		if (!str_info)
			return -EINVAL;
		ipc = IPC_IA_START_STREAM;
		str_info->prev = str_info->status;
		str_info->status = STREAM_RUNNING;
		sst_start_stream(str_id);

		break;
	}
	case SST_SND_DROP: {
		struct stream_info *str_info;
		int ipc;
		str_id = *(int *)arg;
		str_info = get_stream_info(str_id);
		if (!str_info)
			return -EINVAL;
		ipc = IPC_IA_DROP_STREAM;
		str_info->prev = STREAM_UN_INIT;
		str_info->status = STREAM_INIT;
		if (sst_drv_ctx->use_32bit_ops)
			retval = sst_send_sync_msg(ipc, str_id);
		else
			retval = sst_drop_stream(str_id);
		break;
	}
	case SST_SND_STREAM_INIT: {
		struct pcm_stream_info *str_info;
		struct stream_info *stream;

		pr_debug("stream init called\n");
		str_info = (struct pcm_stream_info *)arg;
		str_id = str_info->str_id;
		stream = get_stream_info(str_id);
		if (!stream) {
			retval = -EINVAL;
			break;
		}
		pr_debug("setting the period ptrs\n");
		stream->pcm_substream = str_info->mad_substream;
		stream->period_elapsed = str_info->period_elapsed;
		stream->sfreq = str_info->sfreq;
		stream->prev = stream->status;
		stream->status = STREAM_INIT;
		pr_debug("pcm_substream %p, period_elapsed %p, sfreq %d, status %d\n",
				stream->pcm_substream, stream->period_elapsed, stream->sfreq, stream->status);
		break;
	}

	case SST_SND_BUFFER_POINTER: {
		struct pcm_stream_info *stream_info;

		stream_info = (struct pcm_stream_info *)arg;
		retval = sst_read_timestamp(stream_info);
		pr_debug("pointer %llu, delay %llu\n",
			stream_info->buffer_ptr, stream_info->pcm_delay);
		break;
	}
	default:
		/* Illegal case */
		pr_warn("illegal req\n");
		return -EINVAL;
	}

	return retval;
}

/*
 * sst_copy_runtime_param - copy runtime params from src to dst
 *				 structure.
 *
 *@dst: destination runtime structure
 *@src: source runtime structure
 *
 * This helper function is called to copy the runtime parameter
 * structure.
*/
static int sst_copy_runtime_param(struct snd_sst_runtime_params *dst,
			struct snd_sst_runtime_params *src)
{
	dst->type = src->type;
	dst->str_id = src->str_id;
	dst->size = src->size;
	if (dst->addr) {
		pr_err("mem allocated in prev setting, use the same memory\n");
		return -EINVAL;
	}
	dst->addr = kzalloc(dst->size, GFP_KERNEL);
	if (!dst->addr)
		return -ENOMEM;
	memcpy(dst->addr, src->addr, dst->size);
	return 0;
}
/*
 * sst_set_generic_params - Set generic params
 *
 * @cmd: control cmd to be set
 * @arg: command argument
 *
 * This function is called by MID sound card driver to configure
 * SST runtime params.
 */
static int sst_set_generic_params(enum sst_controls cmd, void *arg)
{
	int ret_val = 0;
	pr_debug("Enter:%s, cmd:%d\n", __func__, cmd);

	if (NULL == arg)
		return -EINVAL;

	switch (cmd) {
	case SST_SET_RUNTIME_PARAMS: {
		struct snd_sst_runtime_params *src;
		struct snd_sst_runtime_params *dst;

		src = (struct snd_sst_runtime_params *)arg;
		dst = &(sst_drv_ctx->runtime_param.param);
		ret_val = sst_copy_runtime_param(dst, src);
		break;
		}
	case SST_SET_ALGO_PARAMS: {
		unsigned int device_input_mixer;
		if (NULL == arg)
			return -EINVAL;
		device_input_mixer = *((unsigned int *)arg);
		pr_debug("LPE mixer algo param set %x\n", device_input_mixer);
		mutex_lock(&sst_drv_ctx->mixer_ctrl_lock);
		sst_drv_ctx->device_input_mixer = device_input_mixer;
		mutex_unlock(&sst_drv_ctx->mixer_ctrl_lock);
		ret_val = sst_send_lpe_mixer_algo_params();
		break;
	}
	case SST_SET_BYTE_STREAM: {
		ret_val = intel_sst_check_device();
		if (ret_val)
			return ret_val;

		ret_val = sst_send_byte_stream_mrfld(arg);
		sst_pm_runtime_put(sst_drv_ctx);
		break;
	}
	case SST_GET_PROBE_BYTE_STREAM: {
		struct snd_sst_probe_bytes *prb_bytes = (struct snd_sst_probe_bytes *)arg;

		if (sst_drv_ctx->probe_bytes) {
			prb_bytes->len = sst_drv_ctx->probe_bytes->len;
			memcpy(prb_bytes->bytes, &sst_drv_ctx->probe_bytes->bytes, prb_bytes->len);
		}
		break;
	}
	case SST_SET_PROBE_BYTE_STREAM: {
		struct snd_sst_probe_bytes *prb_bytes = (struct snd_sst_probe_bytes *)arg;

		if (sst_drv_ctx->probe_bytes) {
			sst_drv_ctx->probe_bytes->len = prb_bytes->len;
			memcpy(&sst_drv_ctx->probe_bytes->bytes, prb_bytes->bytes, prb_bytes->len);
		}

		ret_val = intel_sst_check_device();
		if (ret_val)
			return ret_val;

		ret_val = sst_send_probe_bytes(sst_drv_ctx);
		break;
	}
	case SST_SET_VTSV_INFO: {
		ret_val = intel_sst_check_device();
		if (ret_val)
			return ret_val;

		ret_val = sst_send_vtsv_data_to_fw(sst_drv_ctx);
		if (ret_val)
			pr_err("vtsv data send failed\n");
		sst_pm_runtime_put(sst_drv_ctx);
		break;
	}
	case SST_SET_MONITOR_LPE: {
		if (sst_drv_ctx->pdata->start_recovery_timer)
			ret_val = sst_set_timer(&sst_drv_ctx->monitor_lpe, *(bool *)arg);
		break;
	}
	default:
		pr_err("Invalid cmd request:%d\n", cmd);
		ret_val = -EINVAL;
	}
	return ret_val;
}

static struct sst_ops pcm_ops = {
	.open = sst_open_pcm_stream,
	.device_control = sst_device_control,
	.set_generic_params = sst_set_generic_params,
	.close = sst_close_pcm_stream,
	.power = sst_power_control,
};

static struct compress_sst_ops compr_ops = {
	.open = sst_cdev_open,
	.close = sst_cdev_close,
	.control = sst_cdev_control,
	.tstamp = sst_cdev_tstamp,
	.ack = sst_cdev_ack,
	.get_caps = sst_cdev_caps,
	.get_codec_caps = sst_cdev_codec_caps,
	.set_metadata = sst_cdev_set_metadata,
};


static struct sst_device sst_dsp_device = {
	.name = "Intel(R) SST LPE",
	.dev = NULL,
	.ops = &pcm_ops,
	.compr_ops = &compr_ops,
	.cb_ops	   = NULL,
};

int sst_platform_cb(struct sst_platform_cb_params *cb_params)
{
	if (sst_dsp_device.cb_ops && sst_dsp_device.cb_ops->async_cb) {
		return sst_dsp_device.cb_ops->async_cb(cb_params);
	} else {
		pr_info("Async ops not defined\n");
		return 0;
	}
}

/*
 * register_sst - function to register DSP
 *
 * This functions registers DSP with the platform driver
 */
int register_sst(struct device *dev)
{
	int ret_val;
	sst_dsp_device.dev = dev;
	ret_val = sst_register_dsp(&sst_dsp_device);
	if (ret_val)
		pr_err("Unable to register DSP with platform driver\n");

	return ret_val;
}

int unregister_sst(struct device *dev)
{
	return sst_unregister_dsp(&sst_dsp_device);
}
