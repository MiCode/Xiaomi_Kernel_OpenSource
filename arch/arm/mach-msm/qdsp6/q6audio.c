/*
 * Copyright (C) 2009 Google, Inc.
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
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

#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/android_pmem.h>
#include <linux/firmware.h>
#include <linux/miscdevice.h>
#include <linux/pm_qos.h>

#include "dal.h"
#include "dal_audio.h"
#include "dal_audio_format.h"
#include "dal_acdb.h"
#include "dal_adie.h"
#include <mach/msm_qdsp6_audio.h>
#include <mach/cpuidle.h>

#include <linux/msm_audio_aac.h>

#include <linux/gpio.h>

#include "q6audio_devices.h"
#include <mach/debug_mm.h>


struct q6_hw_info {
	int min_gain;
	int max_gain;
};

/* TODO: provide mechanism to configure from board file */

static struct q6_hw_info q6_audio_hw[Q6_HW_COUNT] = {
	[Q6_HW_HANDSET] = {
		.min_gain = -400,
		.max_gain = 1100,
	},
	[Q6_HW_HEADSET] = {
		.min_gain = -1100,
		.max_gain = 400,
	},
	[Q6_HW_SPEAKER] = {
		.min_gain = -1000,
		.max_gain = 500,
	},
	[Q6_HW_TTY] = {
		.min_gain = 0,
		.max_gain = 0,
	},
	[Q6_HW_BT_SCO] = {
		.min_gain = -1100,
		.max_gain = 400,
	},
	[Q6_HW_BT_A2DP] = {
		.min_gain = -1100,
		.max_gain = 400,
	},
};

static struct wake_lock wakelock;
static struct pm_qos_request pm_qos_req;
static int idlecount;
static DEFINE_MUTEX(idlecount_lock);

void audio_prevent_sleep(void)
{
	mutex_lock(&idlecount_lock);
	if (++idlecount == 1) {
		wake_lock(&wakelock);
		pm_qos_update_request(&pm_qos_req,
				      msm_cpuidle_get_deep_idle_latency());
	}
	mutex_unlock(&idlecount_lock);
}

void audio_allow_sleep(void)
{
	mutex_lock(&idlecount_lock);
	if (--idlecount == 0) {
		pm_qos_update_request(&pm_qos_req, PM_QOS_DEFAULT_VALUE);
		wake_unlock(&wakelock);
	}
	mutex_unlock(&idlecount_lock);
}

static struct clk *icodec_rx_clk;
static struct clk *icodec_tx_clk;
static struct clk *ecodec_clk;
static struct clk *sdac_clk;

static struct q6audio_analog_ops default_analog_ops;
static struct q6audio_analog_ops *analog_ops = &default_analog_ops;
static uint32_t tx_clk_freq = 8000;
static int tx_mute_status = 0;
static int rx_vol_level = 100;
static uint32_t tx_acdb = 0;
static uint32_t rx_acdb = 0;

void q6audio_register_analog_ops(struct q6audio_analog_ops *ops)
{
	analog_ops = ops;
}

static struct q6_device_info *q6_lookup_device(uint32_t device_id,
						uint32_t acdb_id)
{
	struct q6_device_info *di = q6_audio_devices;

	pr_debug("[%s:%s] device_id = 0x%x, acdb_id = %d\n", __MM_FILE__,
		__func__, device_id, acdb_id);
	if (acdb_id) {
		for (;;) {
			if (di->cad_id == acdb_id && di->id == device_id)
				return di;
			if (di->id == 0) {
				pr_err("[%s:%s] bogus id 0x%08x\n",
					__MM_FILE__, __func__, device_id);
				return di;
			}
			di++;
		}
	} else {
		for (;;) {
			if (di->id == device_id)
				return di;
			if (di->id == 0) {
				pr_err("[%s:%s] bogus id 0x%08x\n",
					__MM_FILE__, __func__, device_id);
				return di;
			}
			di++;
		}
	}
}

static uint32_t q6_device_to_codec(uint32_t device_id)
{
	struct q6_device_info *di = q6_lookup_device(device_id, 0);
	return di->codec;
}

static uint32_t q6_device_to_dir(uint32_t device_id)
{
	struct q6_device_info *di = q6_lookup_device(device_id, 0);
	return di->dir;
}

static uint32_t q6_device_to_cad_id(uint32_t device_id)
{
	struct q6_device_info *di = q6_lookup_device(device_id, 0);
	return di->cad_id;
}

static uint32_t q6_device_to_path(uint32_t device_id, uint32_t acdb_id)
{
	struct q6_device_info *di = q6_lookup_device(device_id, acdb_id);
	return di->path;
}

static uint32_t q6_device_to_rate(uint32_t device_id)
{
	struct q6_device_info *di = q6_lookup_device(device_id, 0);
	return di->rate;
}

int q6_device_volume(uint32_t device_id, int level)
{
	struct q6_device_info *di = q6_lookup_device(device_id, 0);
	struct q6_hw_info *hw;

	hw = &q6_audio_hw[di->hw];

	return hw->min_gain + ((hw->max_gain - hw->min_gain) * level) / 100;
}

static inline int adie_open(struct dal_client *client) 
{
	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	return dal_call_f0(client, DAL_OP_OPEN, 0);
}

static inline int adie_close(struct dal_client *client) 
{
	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	return dal_call_f0(client, DAL_OP_CLOSE, 0);
}

static inline int adie_set_path(struct dal_client *client,
				uint32_t id, uint32_t path_type)
{
	pr_debug("[%s:%s] id = 0x%x, path_type = %d\n", __MM_FILE__,
		__func__, id, path_type);
	return dal_call_f1(client, ADIE_OP_SET_PATH, id, path_type);
}

static inline int adie_set_path_freq_plan(struct dal_client *client,
					  uint32_t path_type, uint32_t plan) 
{
	pr_debug("[%s:%s] path_type = %d, plan = %d\n",	__MM_FILE__,
		__func__, path_type, plan);
	return dal_call_f1(client, ADIE_OP_SET_PATH_FREQUENCY_PLAN,
			   path_type, plan);
}

static inline int adie_proceed_to_stage(struct dal_client *client,
					uint32_t path_type, uint32_t stage)
{
	pr_debug("[%s:%s] path_type = %d, stage = 0x%x\n", __MM_FILE__,
		__func__, path_type, stage);
	return dal_call_f1(client, ADIE_OP_PROCEED_TO_STAGE,
			   path_type, stage);
}

static inline int adie_mute_path(struct dal_client *client,
				 uint32_t path_type, uint32_t mute_state)
{
	pr_debug("[%s:%s] path_type = %d, mute = %d\n",	__MM_FILE__, __func__,
		 path_type, mute_state);
	return dal_call_f1(client, ADIE_OP_MUTE_PATH, path_type, mute_state);
}

static int adie_refcount;

static struct dal_client *adie;
static struct dal_client *adsp;
static struct dal_client *acdb;

static int adie_enable(void)
{
	adie_refcount++;
	if (adie_refcount == 1)
		adie_open(adie);
	return 0;
}

static int adie_disable(void)
{
	adie_refcount--;
	if (adie_refcount == 0)
		adie_close(adie);
	return 0;
}

/* 4k PMEM used for exchanging acdb device config tables
 * and stream format descriptions with the DSP.
 */
static char *audio_data;
static int32_t audio_phys;

#define SESSION_MIN 0
#define SESSION_MAX 64

static DEFINE_MUTEX(session_lock);
static DEFINE_MUTEX(audio_lock);

static struct audio_client *session[SESSION_MAX];

static int session_alloc(struct audio_client *ac)
{
	int n;

	mutex_lock(&session_lock);
	for (n = SESSION_MIN; n < SESSION_MAX; n++) {
		if (!session[n]) {
			session[n] = ac;
			mutex_unlock(&session_lock);
			pr_debug("[%s:%s] session = %d\n", __MM_FILE__,
				__func__, n);
			return n;
		}
	}
	mutex_unlock(&session_lock);
	return -ENOMEM;
}

static void session_free(int n, struct audio_client *ac)
{
	mutex_lock(&session_lock);
	if (session[n] == ac) {
		session[n] = 0;
		pr_debug("[%s:%s] session = %d\n", __MM_FILE__, __func__, n);
	}
	mutex_unlock(&session_lock);
}

static void audio_client_free(struct audio_client *ac)
{
	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	session_free(ac->session, ac);

	if (ac->buf[0].data) {
		iounmap(ac->buf[0].data);
		pmem_kfree(ac->buf[0].phys);
	}
	if (ac->buf[1].data) {
		iounmap(ac->buf[1].data);
		pmem_kfree(ac->buf[1].phys);
	}
	kfree(ac);
}

static struct audio_client *audio_client_alloc(unsigned bufsz)
{
	struct audio_client *ac;
	int n;

	pr_debug("[%s:%s] bufsz = %d\n", __MM_FILE__, __func__, bufsz);
	ac = kzalloc(sizeof(*ac), GFP_KERNEL);
	if (!ac)
		return 0;

	n = session_alloc(ac);
	if (n < 0)
		goto fail_session;
	ac->session = n;

	if (bufsz > 0) {
		ac->buf[0].phys = pmem_kalloc(bufsz,
					PMEM_MEMTYPE_EBI1|PMEM_ALIGNMENT_4K);
		ac->buf[0].data = ioremap(ac->buf[0].phys, bufsz);
		if (!ac->buf[0].data)
			goto fail;
		ac->buf[1].phys = pmem_kalloc(bufsz,
					PMEM_MEMTYPE_EBI1|PMEM_ALIGNMENT_4K);
		ac->buf[1].data = ioremap(ac->buf[1].phys, bufsz);
		if (!ac->buf[1].data)
			goto fail;

		ac->buf[0].size = bufsz;
		ac->buf[1].size = bufsz;
	}

	init_waitqueue_head(&ac->wait);
	ac->client = adsp;

	return ac;

fail:
	session_free(n, ac);
fail_session:
	audio_client_free(ac);
	return 0;
}

void audio_client_dump(struct audio_client *ac)
{
	dal_trace_dump(ac->client);
}

static int audio_ioctl(struct audio_client *ac, void *ptr, uint32_t len)
{
	struct adsp_command_hdr *hdr = ptr;
	uint32_t tmp;
	int r;

	hdr->size = len - sizeof(u32);
	hdr->dst = AUDIO_ADDR(ac->session, 0, AUDIO_DOMAIN_DSP);
	hdr->src = AUDIO_ADDR(ac->session, 0, AUDIO_DOMAIN_APP);
	hdr->context = ac->session;
	ac->cb_status = -EBUSY;
	r = dal_call(ac->client, AUDIO_OP_CONTROL, 5, ptr, len, &tmp, sizeof(tmp));
	if (r != 4)
		return -EIO;
	if (!wait_event_timeout(ac->wait, (ac->cb_status != -EBUSY), 5*HZ)) {
		dal_trace_dump(ac->client);
		pr_err("[%s:%s] timeout. dsp dead?\n", __MM_FILE__, __func__);
		q6audio_dsp_not_responding();
	}
	return ac->cb_status;
}

static int audio_command(struct audio_client *ac, uint32_t cmd)
{
	struct adsp_command_hdr rpc;
	memset(&rpc, 0, sizeof(rpc));
	rpc.opcode = cmd;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_open_control(struct audio_client *ac)
{
	struct adsp_open_command rpc;

	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_DEVICE;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_out_open(struct audio_client *ac, uint32_t bufsz,
			  uint32_t rate, uint32_t channels)
{
	struct adsp_open_command rpc;

	memset(&rpc, 0, sizeof(rpc));

	rpc.format.standard.format = ADSP_AUDIO_FORMAT_PCM;
	rpc.format.standard.channels = channels;
	rpc.format.standard.bits_per_sample = 16;
	rpc.format.standard.sampling_rate = rate;
	rpc.format.standard.is_signed = 1;
	rpc.format.standard.is_interleaved = 1;

	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_WRITE;
	rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;
	rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_PLAYBACK;
	rpc.buf_max_size = bufsz;

	pr_debug("[%s:%s]ac = %p\n", __MM_FILE__, __func__, ac);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_in_open(struct audio_client *ac, uint32_t bufsz,
			 uint32_t flags, uint32_t rate, uint32_t channels)
{
	struct adsp_open_command rpc;

	memset(&rpc, 0, sizeof(rpc));

	rpc.format.standard.format = ADSP_AUDIO_FORMAT_PCM;
	rpc.format.standard.channels = channels;
	rpc.format.standard.bits_per_sample = 16;
	rpc.format.standard.sampling_rate = rate;
	rpc.format.standard.is_signed = 1;
	rpc.format.standard.is_interleaved = 1;

	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_READ;
	rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;
	if (flags == AUDIO_FLAG_READ)
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_RECORD;
	else
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_MIXED_RECORD;

	rpc.buf_max_size = bufsz;

	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_auxpcm_out_open(struct audio_client *ac,
			  uint32_t rate, uint32_t channels)
{
	struct adsp_open_command rpc;

	memset(&rpc, 0, sizeof(rpc));

	rpc.format.standard.format = ADSP_AUDIO_FORMAT_PCM;
	rpc.format.standard.channels = channels;
	rpc.format.standard.bits_per_sample = 16;
	rpc.format.standard.sampling_rate = rate;
	rpc.format.standard.is_signed = 1;
	rpc.format.standard.is_interleaved = 1;

	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_READ;
	rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;
	rpc.mode =  ADSP_AUDIO_OPEN_STREAM_MODE_AUX_PCM;
	rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_RECORD;

	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_auxpcm_in_open(struct audio_client *ac, uint32_t rate,
		uint32_t channels)
{
	struct adsp_open_command rpc;

	memset(&rpc, 0, sizeof(rpc));

	rpc.format.standard.format = ADSP_AUDIO_FORMAT_PCM;
	rpc.format.standard.channels = channels;
	rpc.format.standard.bits_per_sample = 16;
	rpc.format.standard.sampling_rate = rate;
	rpc.format.standard.is_signed = 1;
	rpc.format.standard.is_interleaved = 1;

	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_WRITE;
	rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;
	rpc.mode =  ADSP_AUDIO_OPEN_STREAM_MODE_AUX_PCM;
	rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_PLAYBACK;

	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_mp3_open(struct audio_client *ac, uint32_t bufsz,
			  uint32_t rate, uint32_t channels)
{
	struct adsp_open_command rpc;

	memset(&rpc, 0, sizeof(rpc));

	rpc.format.standard.format = ADSP_AUDIO_FORMAT_MP3;
	rpc.format.standard.channels = channels;
	rpc.format.standard.bits_per_sample = 16;
	rpc.format.standard.sampling_rate = rate;
	rpc.format.standard.is_signed = 1;
	rpc.format.standard.is_interleaved = 0;

	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_WRITE;
	rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;
	rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_PLAYBACK;
	rpc.buf_max_size = bufsz;

	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_dtmf_open(struct audio_client *ac,
			  uint32_t rate, uint32_t channels)
{
	struct adsp_open_command rpc;

	memset(&rpc, 0, sizeof(rpc));

	rpc.format.standard.format = ADSP_AUDIO_FORMAT_DTMF;
	rpc.format.standard.channels = channels;
	rpc.format.standard.bits_per_sample = 16;
	rpc.format.standard.sampling_rate = rate;
	rpc.format.standard.is_signed = 1;
	rpc.format.standard.is_interleaved = 0;

	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_WRITE;
	rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;
	rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_PLAYBACK;

	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_aac_open(struct audio_client *ac, uint32_t bufsz,
			  uint32_t sample_rate, uint32_t channels,
			  uint32_t bit_rate, uint32_t flags,
					uint32_t stream_format)
{
	struct adsp_open_command rpc;
	int audio_object_type;
	int index = sizeof(u32);
	u32 *aac_type = NULL;

	memset(&rpc, 0, sizeof(rpc));

	rpc.format.binary.format = ADSP_AUDIO_FORMAT_MPEG4_AAC;
	/* only 48k sample rate is supported */
	sample_rate = 3;
	/* AAC OBJECT LC */
	audio_object_type = 2;

	aac_type = (u32 *)rpc.format.binary.data;
	switch (stream_format) {
	case AUDIO_AAC_FORMAT_ADTS:
		/* AAC Encoder expect MPEG4_ADTS media type */
		*aac_type = ADSP_AUDIO_AAC_MPEG4_ADTS;
	break;
	case AUDIO_AAC_FORMAT_RAW:
		/* for ADIF recording */
		*aac_type = ADSP_AUDIO_AAC_RAW;
	break;
	}

	rpc.format.binary.data[index++] = (u8)(
			((audio_object_type & 0x1F) << 3) |
			((sample_rate >> 1) & 0x7));
			rpc.format.binary.data[index] = (u8)(
			((sample_rate & 0x1) << 7) |
			((channels & 0x7) << 3));
	rpc.format.binary.num_bytes = index + 1;
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_READ;
	rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;

	if (flags == AUDIO_FLAG_READ)
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_RECORD;
	else
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_MIXED_RECORD;

	rpc.buf_max_size = bufsz;
	rpc.config.aac.bit_rate = bit_rate;
	rpc.config.aac.encoder_mode = ADSP_AUDIO_ENC_AAC_LC_ONLY_MODE;
	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_qcp_open(struct audio_client *ac, uint32_t bufsz,
				uint32_t min_rate, uint32_t max_rate,
				uint32_t flags, uint32_t format)
{
	struct adsp_open_command rpc;

	memset(&rpc, 0, sizeof(rpc));

	rpc.format.standard.format = format;
	rpc.format.standard.channels = 1;
	rpc.format.standard.bits_per_sample = 16;
	rpc.format.standard.sampling_rate = 8000;
	rpc.format.standard.is_signed = 1;
	rpc.format.standard.is_interleaved = 0;

	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_READ;
	rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;

	if (flags == AUDIO_FLAG_READ)
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_RECORD;
	else
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_MIXED_RECORD;
	rpc.buf_max_size = bufsz;
	rpc.config.evrc.min_rate = min_rate;
	rpc.config.evrc.max_rate = max_rate;

	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_amrnb_open(struct audio_client *ac, uint32_t bufsz,
					uint32_t enc_mode, uint32_t flags,
					uint32_t dtx_enable)
{
	struct adsp_open_command rpc;

	memset(&rpc, 0, sizeof(rpc));

	rpc.format.standard.format = ADSP_AUDIO_FORMAT_AMRNB_FS;
	rpc.format.standard.channels = 1;
	rpc.format.standard.bits_per_sample = 16;
	rpc.format.standard.sampling_rate = 8000;
	rpc.format.standard.is_signed = 1;
	rpc.format.standard.is_interleaved = 0;

	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_READ;
	rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;

	if (flags == AUDIO_FLAG_READ)
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_RECORD;
	else
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_MIXED_RECORD;

	rpc.buf_max_size = bufsz;
	rpc.config.amr.mode = enc_mode;
	rpc.config.amr.dtx_mode = dtx_enable;
	rpc.config.amr.enable = 1;

	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}



static int audio_close(struct audio_client *ac)
{
	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	audio_command(ac, ADSP_AUDIO_IOCTL_CMD_STREAM_STOP);
	audio_command(ac, ADSP_AUDIO_IOCTL_CMD_CLOSE);
	return 0;
}

static int audio_set_table(struct audio_client *ac,
			   uint32_t device_id, int size)
{
	struct adsp_set_dev_cfg_table_command rpc;

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_SET_DEVICE_CONFIG_TABLE;
	if (q6_device_to_dir(device_id) == Q6_TX) {
		if (tx_clk_freq > 16000)
			rpc.hdr.data = 48000;
		else if (tx_clk_freq > 8000)
			rpc.hdr.data = 16000;
		else
			rpc.hdr.data = 8000;
	}
	rpc.device_id = device_id;
	rpc.phys_addr = audio_phys;
	rpc.phys_size = size;
	rpc.phys_used = size;

	pr_debug("[%s:%s] ac = %p, device_id = 0x%x, size = %d\n", __MM_FILE__,
		__func__, ac, device_id, size);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

int q6audio_read(struct audio_client *ac, struct audio_buffer *ab)
{
	struct adsp_buffer_command rpc;
	uint32_t res;
	int r;

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.size = sizeof(rpc) - sizeof(u32);
	rpc.hdr.dst = AUDIO_ADDR(ac->session, 0, AUDIO_DOMAIN_DSP);
	rpc.hdr.src = AUDIO_ADDR(ac->session, 0, AUDIO_DOMAIN_APP);
	rpc.hdr.context = ac->session;
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_DATA_TX;
	rpc.buffer.addr = ab->phys;
	rpc.buffer.max_size = ab->size;
	rpc.buffer.actual_size = ab->actual_size;

	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	r = dal_call(ac->client, AUDIO_OP_DATA, 5, &rpc, sizeof(rpc),
		     &res, sizeof(res));
	return 0;
}

int q6audio_write(struct audio_client *ac, struct audio_buffer *ab)
{
	struct adsp_buffer_command rpc;
	uint32_t res;
	int r;

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.size = sizeof(rpc) - sizeof(u32);
	rpc.hdr.dst = AUDIO_ADDR(ac->session, 0, AUDIO_DOMAIN_DSP);
	rpc.hdr.src = AUDIO_ADDR(ac->session, 0, AUDIO_DOMAIN_APP);
	rpc.hdr.context = ac->session;
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_DATA_RX;
	rpc.buffer.addr = ab->phys;
	rpc.buffer.max_size = ab->size;
	rpc.buffer.actual_size = ab->actual_size;

	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	r = dal_call(ac->client, AUDIO_OP_DATA, 5, &rpc, sizeof(rpc),
		     &res, sizeof(res));
	return 0;
}

static int audio_rx_volume(struct audio_client *ac, uint32_t dev_id, int32_t volume)
{
	struct adsp_set_dev_volume_command rpc;

	pr_debug("[%s:%s] volume = %d\n", __MM_FILE__, __func__, volume);
	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_SET_DEVICE_VOL;
	rpc.device_id = dev_id;
	rpc.path = ADSP_PATH_RX;
	rpc.volume = volume;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_rx_mute(struct audio_client *ac, uint32_t dev_id, int mute)
{
	struct adsp_set_dev_mute_command rpc;

	pr_debug("[%s:%s] mute = %d, dev_id = 0x%x\n", __MM_FILE__,
			__func__, mute, dev_id);
	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_SET_DEVICE_MUTE;
	rpc.device_id = dev_id;
	rpc.path = ADSP_PATH_RX;
	rpc.mute = !!mute;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_tx_mute(struct audio_client *ac, uint32_t dev_id, int mute)
{
	struct adsp_set_dev_mute_command rpc;

	pr_debug("[%s:%s] mute = %d\n", __MM_FILE__, __func__, mute);
	if (mute < 0  ||  mute > 3) {
		pr_err("[%s:%s] invalid mute status %d\n", __MM_FILE__,
				__func__, mute);
		return -EINVAL;
	}

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_SET_DEVICE_MUTE;
	if ((mute == STREAM_UNMUTE) || (mute == STREAM_MUTE)) {
		rpc.device_id = ADSP_AUDIO_DEVICE_ID_VOICE;
		rpc.path = ADSP_PATH_TX_CNG_DIS;
	} else {
		rpc.device_id = dev_id;
		rpc.path = ADSP_PATH_TX;
	}
	mute &= 0x01;
	rpc.mute = !!mute;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_stream_volume(struct audio_client *ac, int volume)
{
	struct adsp_set_volume_command rpc;
	int rc;

	pr_debug("[%s:%s] volume = %d\n", __MM_FILE__, __func__, volume);
	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_SET_STREAM_VOL;
	rpc.volume = volume;
	rc = audio_ioctl(ac, &rpc, sizeof(rpc));
	return rc;
}

static int audio_stream_mute(struct audio_client *ac, int mute)
{
	struct adsp_set_mute_command rpc;
	int rc;

	pr_debug("[%s:%s] mute = %d\n", __MM_FILE__, __func__, mute);
	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_SET_STREAM_MUTE;
	rpc.mute = mute;
	rc = audio_ioctl(ac, &rpc, sizeof(rpc));
	return rc;
}

static void callback(void *data, int len, void *cookie)
{
	struct adsp_event_hdr *e = data;
	struct audio_client *ac;
	struct adsp_buffer_event *abe = data;

	if (e->context >= SESSION_MAX) {
		pr_err("[%s:%s] bogus session %d\n", __MM_FILE__, __func__,
				e->context);
		return;
	}
	ac = session[e->context];
	if (!ac) {
		pr_err("[%s:%s] unknown session %d\n", __MM_FILE__, __func__,
				e->context);
		return;
	}

	if (e->event_id == ADSP_AUDIO_IOCTL_CMD_STREAM_EOS) {
		pr_debug("[%s:%s] CB Stream eos, ac = %p\n",
			__MM_FILE__, __func__, ac);
		if (e->status)
			pr_err("[%s:%s] playback status %d\n", __MM_FILE__,
					__func__, e->status);
		if (ac->cb_status == -EBUSY) {
			ac->cb_status = e->status;
			wake_up(&ac->wait);
		}
		return;
	}

	if (e->event_id == ADSP_AUDIO_EVT_STATUS_BUF_DONE) {
		pr_debug("[%s:%s] CB done, ac = %p, status = %d\n",
				__MM_FILE__, __func__, ac, e->status);
		if (e->status)
			pr_err("[%s:%s] buffer status %d\n", __MM_FILE__,
					__func__, e->status);

		ac->buf[ac->dsp_buf].actual_size = abe->buffer.actual_size;
		ac->buf[ac->dsp_buf].used = 0;
		ac->dsp_buf ^= 1;
		wake_up(&ac->wait);
		return;
	}

	pr_debug("[%s:%s] ac = %p, event_id = 0x%x, status = %d\n",
			__MM_FILE__, __func__, ac, e->event_id, e->status);
	if (e->status)
		pr_warning("audio_cb: s=%d e=%08x status=%d\n",
			   e->context, e->event_id, e->status);
	if (ac->cb_status == -EBUSY) {
		ac->cb_status = e->status;
		wake_up(&ac->wait);
	}
}

static void audio_init(struct dal_client *client)
{
	u32 tmp[3];

	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	tmp[0] = 2 * sizeof(u32);
	tmp[1] = 0;
	tmp[2] = 0;
	dal_call(client, AUDIO_OP_INIT, 5, tmp, sizeof(tmp),
		 tmp, sizeof(u32));
}

static struct audio_client *ac_control;

static int q6audio_init(void)
{
	struct audio_client *ac = 0;
	int res;

	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	mutex_lock(&audio_lock);
	if (ac_control) {
		res = 0;
		goto done;
	}

	pr_info("[%s:%s] codecs\n", __MM_FILE__, __func__);
	icodec_rx_clk = clk_get(0, "icodec_rx_clk");
	icodec_tx_clk = clk_get(0, "icodec_tx_clk");
	ecodec_clk = clk_get(0, "ecodec_clk");
	sdac_clk = clk_get(0, "sdac_clk");
	audio_phys = pmem_kalloc(4096, PMEM_MEMTYPE_EBI1|PMEM_ALIGNMENT_4K);
	audio_data = ioremap(audio_phys, 4096);

	pr_info("[%s:%s] attach ADSP\n", __MM_FILE__, __func__);
	adsp = dal_attach(AUDIO_DAL_DEVICE, AUDIO_DAL_PORT, 1,
			  callback, 0);
	if (!adsp) {
		pr_err("[%s:%s] cannot attach to adsp\n", __MM_FILE__,
				__func__);
		res = -ENODEV;
		goto done;
	}
	pr_info("[%s:%s] INIT\n", __MM_FILE__, __func__);
	audio_init(adsp);
	dal_trace(adsp);

	ac = audio_client_alloc(0);
	if (!ac) {
		pr_err("[%s:%s] cannot allocate client\n",
				__MM_FILE__, __func__);
		res = -ENOMEM;
		goto done;
	}

	pr_info("[%s:%s] OPEN control\n", __MM_FILE__, __func__);
	if (audio_open_control(ac)) {
		pr_err("[%s:%s] cannot open control channel\n",
				__MM_FILE__, __func__);
		res = -ENODEV;
		goto done;
	}

	pr_info("[%s:%s] attach ACDB\n", __MM_FILE__, __func__);
	acdb = dal_attach(ACDB_DAL_DEVICE, ACDB_DAL_PORT, 0, 0, 0);
	if (!acdb) {
		pr_err("[%s:%s] cannot attach to acdb channel\n",
				__MM_FILE__, __func__);
		res = -ENODEV;
		goto done;
	}

	pr_info("[%s:%s] attach ADIE\n", __MM_FILE__, __func__);
	adie = dal_attach(ADIE_DAL_DEVICE, ADIE_DAL_PORT, 0, 0, 0);
	if (!adie) {
		pr_err("[%s:%s] cannot attach to adie\n",
				__MM_FILE__, __func__);
		res = -ENODEV;
		goto done;
	}
	if (analog_ops->init)
		analog_ops->init();

	res = 0;
	ac_control = ac;

	pm_qos_add_request(&pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);
	wake_lock_init(&wakelock, WAKE_LOCK_SUSPEND, "audio_pcm_suspend");
done:
	if ((res < 0) && ac)
		audio_client_free(ac);
	mutex_unlock(&audio_lock);

	pr_debug("[%s:%s] res = %d\n", __MM_FILE__, __func__, res);
	return res;
}

struct audio_config_data {
	uint32_t device_id;
	uint32_t sample_rate;
	uint32_t offset;
	uint32_t length;
};

struct audio_config_database {
	uint8_t magic[8];
	uint32_t entry_count;
	uint32_t unused;
	struct audio_config_data entry[0];
};

void *acdb_data;
const struct firmware *acdb_fw;
extern struct miscdevice q6_control_device;

static int acdb_get_config_table(uint32_t device_id, uint32_t sample_rate)
{
	struct acdb_cmd_device_table rpc;
	struct acdb_result res;
	int r;

	pr_debug("[%s:%s] device_id = 0x%x, samplerate = %d\n", __MM_FILE__,
		__func__, device_id, sample_rate);
	if (q6audio_init())
		return 0;

	memset(audio_data, 0, 4096);
	memset(&rpc, 0, sizeof(rpc));

	rpc.size = sizeof(rpc) - (2 * sizeof(uint32_t));
	rpc.command_id = ACDB_GET_DEVICE_TABLE;
	rpc.device_id = device_id;
	rpc.sample_rate_id = sample_rate;
	rpc.total_bytes = 4096;
	rpc.unmapped_buf = audio_phys;
	rpc.res_size = sizeof(res) - (2 * sizeof(uint32_t));

	r = dal_call(acdb, ACDB_OP_IOCTL, 8, &rpc, sizeof(rpc),
		&res, sizeof(res));

	if ((r == sizeof(res)) && (res.dal_status == 0))
		return res.used_bytes;

	return -EIO;
}

static uint32_t audio_rx_path_id = ADIE_PATH_HANDSET_RX;
static uint32_t audio_rx_device_id = ADSP_AUDIO_DEVICE_ID_HANDSET_SPKR;
static uint32_t audio_rx_device_group = -1;
static uint32_t audio_tx_path_id = ADIE_PATH_HANDSET_TX;
static uint32_t audio_tx_device_id = ADSP_AUDIO_DEVICE_ID_HANDSET_MIC;
static uint32_t audio_tx_device_group = -1;

static int qdsp6_devchg_notify(struct audio_client *ac,
			       uint32_t dev_type, uint32_t dev_id)
{
	struct adsp_device_switch_command rpc;

	if (dev_type != ADSP_AUDIO_RX_DEVICE &&
	    dev_type != ADSP_AUDIO_TX_DEVICE)
		return -EINVAL;

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_DEVICE_SWITCH_PREPARE;
	if (dev_type == ADSP_AUDIO_RX_DEVICE) {
		rpc.old_device = audio_rx_device_id;
		rpc.new_device = dev_id;
	} else {
		rpc.old_device = audio_tx_device_id;
		rpc.new_device = dev_id;
	}
	rpc.device_class = 0;
	rpc.device_type = dev_type;
	pr_debug("[%s:%s] dev_id = 0x%x\n", __MM_FILE__, __func__, dev_id);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int qdsp6_standby(struct audio_client *ac)
{
	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	return audio_command(ac, ADSP_AUDIO_IOCTL_CMD_DEVICE_SWITCH_STANDBY);
}

static int qdsp6_start(struct audio_client *ac)
{
	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	return audio_command(ac, ADSP_AUDIO_IOCTL_CMD_DEVICE_SWITCH_COMMIT);
}

static void audio_rx_analog_enable(int en)
{
	pr_debug("[%s:%s] audio_rx_device_id = 0x%x, en = %d\n", __MM_FILE__,
		__func__, audio_rx_device_id, en);
	switch (audio_rx_device_id) {
	case ADSP_AUDIO_DEVICE_ID_HEADSET_SPKR_MONO:
	case ADSP_AUDIO_DEVICE_ID_HEADSET_SPKR_STEREO:
	case ADSP_AUDIO_DEVICE_ID_TTY_HEADSET_SPKR:
		if (analog_ops->headset_enable)
			analog_ops->headset_enable(en);
		break;
	case ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO_W_MONO_HEADSET:
	case ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO_W_STEREO_HEADSET:
	case ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_STEREO_W_MONO_HEADSET:
	case ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_STEREO_W_STEREO_HEADSET:
		if (analog_ops->headset_enable)
			analog_ops->headset_enable(en);
		if (analog_ops->speaker_enable)
			analog_ops->speaker_enable(en);
		break;
	case ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO:
	case ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_STEREO:
		if (analog_ops->speaker_enable)
			analog_ops->speaker_enable(en);
		break;
	case ADSP_AUDIO_DEVICE_ID_BT_SCO_SPKR:
		if (analog_ops->bt_sco_enable)
			analog_ops->bt_sco_enable(en);
		break;
	case ADSP_AUDIO_DEVICE_ID_HANDSET_SPKR:
		if (analog_ops->receiver_enable)
			analog_ops->receiver_enable(en);
		break;
	}
}

static void audio_tx_analog_enable(int en)
{
	pr_debug("[%s:%s] audio_tx_device_id = 0x%x, en = %d\n", __MM_FILE__,
		__func__, audio_tx_device_id, en);
	switch (audio_tx_device_id) {
	case ADSP_AUDIO_DEVICE_ID_HANDSET_MIC:
	case ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MIC:
		if (analog_ops->int_mic_enable)
			analog_ops->int_mic_enable(en);
		break;
	case ADSP_AUDIO_DEVICE_ID_HEADSET_MIC:
	case ADSP_AUDIO_DEVICE_ID_TTY_HEADSET_MIC:
	case ADSP_AUDIO_DEVICE_ID_HANDSET_DUAL_MIC:
	case ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_DUAL_MIC:
		if (analog_ops->ext_mic_enable)
			analog_ops->ext_mic_enable(en);
		break;
	case ADSP_AUDIO_DEVICE_ID_BT_SCO_MIC:
		if (analog_ops->bt_sco_enable)
			analog_ops->bt_sco_enable(en);
		break;
	}
}

static int audio_update_acdb(uint32_t adev, uint32_t acdb_id)
{
	uint32_t sample_rate;
	int sz;

	pr_debug("[%s:%s] adev = 0x%x, acdb_id = 0x%x\n", __MM_FILE__,
		__func__, adev, acdb_id);
	if (q6_device_to_dir(adev) == Q6_RX) {
		rx_acdb = acdb_id;
		sample_rate = q6_device_to_rate(adev);
	} else {

		tx_acdb = acdb_id;
		if (tx_clk_freq > 16000)
			sample_rate = 48000;
		else if (tx_clk_freq > 8000)
			sample_rate = 16000;
		else
			sample_rate = 8000;
	}

	if (acdb_id == 0)
		acdb_id = q6_device_to_cad_id(adev);

	sz = acdb_get_config_table(acdb_id, sample_rate);
	audio_set_table(ac_control, adev, sz);

	return 0;
}

static void adie_rx_path_enable(uint32_t acdb_id)
{
	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	if (audio_rx_path_id) {
		adie_enable();
		adie_set_path(adie, audio_rx_path_id, ADIE_PATH_RX);
		adie_set_path_freq_plan(adie, ADIE_PATH_RX, 48000);

		adie_proceed_to_stage(adie, ADIE_PATH_RX,
				ADIE_STAGE_DIGITAL_READY);
		adie_proceed_to_stage(adie, ADIE_PATH_RX,
				ADIE_STAGE_DIGITAL_ANALOG_READY);
	}
}

static void q6_rx_path_enable(int reconf, uint32_t acdb_id)
{
	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	if (!reconf)
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_RX_DEVICE, audio_rx_device_id);
	audio_update_acdb(audio_rx_device_id, acdb_id);
	qdsp6_standby(ac_control);
	qdsp6_start(ac_control);
}

static void _audio_rx_path_enable(int reconf, uint32_t acdb_id)
{
	pr_debug("[%s:%s] reconf = %d\n", __MM_FILE__, __func__, reconf);
	q6_rx_path_enable(reconf, acdb_id);
	if (audio_rx_path_id)
		adie_rx_path_enable(acdb_id);
	audio_rx_analog_enable(1);
}

static void _audio_tx_path_enable(int reconf, uint32_t acdb_id)
{
	pr_debug("[%s:%s] reconf = %d, tx_clk_freq = %d\n", __MM_FILE__,
			__func__, reconf, tx_clk_freq);
	audio_tx_analog_enable(1);

	if (audio_tx_path_id) {
		adie_enable();
		adie_set_path(adie, audio_tx_path_id, ADIE_PATH_TX);

		if (tx_clk_freq > 16000)
			adie_set_path_freq_plan(adie, ADIE_PATH_TX, 48000);
		else if (tx_clk_freq > 8000)
			adie_set_path_freq_plan(adie, ADIE_PATH_TX, 16000);
		else
			adie_set_path_freq_plan(adie, ADIE_PATH_TX, 8000);

		adie_proceed_to_stage(adie, ADIE_PATH_TX,
				ADIE_STAGE_DIGITAL_READY);
		adie_proceed_to_stage(adie, ADIE_PATH_TX,
				ADIE_STAGE_DIGITAL_ANALOG_READY);
	}


	if (!reconf)
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_TX_DEVICE,
				audio_tx_device_id);
	audio_update_acdb(audio_tx_device_id, acdb_id);
	qdsp6_standby(ac_control);
	qdsp6_start(ac_control);

	audio_tx_mute(ac_control, audio_tx_device_id, tx_mute_status);
}

static void _audio_rx_path_disable(void)
{
	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	audio_rx_analog_enable(0);

	if (audio_rx_path_id) {
		adie_proceed_to_stage(adie, ADIE_PATH_RX,
				ADIE_STAGE_ANALOG_OFF);
		adie_proceed_to_stage(adie, ADIE_PATH_RX,
				ADIE_STAGE_DIGITAL_OFF);
		adie_disable();
	}
}

static void _audio_tx_path_disable(void)
{
	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	audio_tx_analog_enable(0);

	if (audio_tx_path_id) {
		adie_proceed_to_stage(adie, ADIE_PATH_TX,
				ADIE_STAGE_ANALOG_OFF);
		adie_proceed_to_stage(adie, ADIE_PATH_TX,
				ADIE_STAGE_DIGITAL_OFF);
		adie_disable();
	}
}

static int icodec_rx_clk_refcount;
static int icodec_tx_clk_refcount;
static int ecodec_clk_refcount;
static int sdac_clk_refcount;

static void ecodec_clk_enable(void)
{
	ecodec_clk_refcount++;
	if (ecodec_clk_refcount == 1) {
		clk_set_rate(ecodec_clk, 2048000);
		clk_enable(ecodec_clk);
	}
}
static void ecodec_clk_disable(int group_reset, int path)
{
	ecodec_clk_refcount--;
	if (ecodec_clk_refcount == 0) {
		clk_disable(ecodec_clk);
		if (group_reset) {
			if (path == ADSP_PATH_TX)
				audio_tx_device_group = -1;
			else
				audio_rx_device_group = -1;
		}
	}
}
static void _audio_rx_clk_enable(void)
{
	uint32_t device_group = q6_device_to_codec(audio_rx_device_id);

	pr_debug("[%s:%s] rx_clk_refcount = %d\n", __MM_FILE__, __func__,
		icodec_rx_clk_refcount);
	switch(device_group) {
	case Q6_ICODEC_RX:
		icodec_rx_clk_refcount++;
		if (icodec_rx_clk_refcount == 1) {
			clk_set_rate(icodec_rx_clk, 12288000);
			clk_enable(icodec_rx_clk);
		}
		break;
	case Q6_ECODEC_RX:
		ecodec_clk_enable();
		break;
	case Q6_SDAC_RX:
		sdac_clk_refcount++;
		if (sdac_clk_refcount == 1) {
			clk_set_rate(sdac_clk, 12288000);
			clk_enable(sdac_clk);
		}
		break;
	default:
		return;
	}
	audio_rx_device_group = device_group;
}

static void _audio_tx_clk_enable(void)
{
	uint32_t device_group = q6_device_to_codec(audio_tx_device_id);
	uint32_t icodec_tx_clk_rate;

	pr_debug("[%s:%s] tx_clk_refcount = %d\n", __MM_FILE__, __func__,
		icodec_tx_clk_refcount);
	switch (device_group) {
	case Q6_ICODEC_TX:
		icodec_tx_clk_refcount++;
		if (icodec_tx_clk_refcount == 1) {
			if (tx_clk_freq > 16000)
				icodec_tx_clk_rate = 48000;
			else if (tx_clk_freq > 8000)
				icodec_tx_clk_rate = 16000;
			else
				icodec_tx_clk_rate = 8000;

			clk_set_rate(icodec_tx_clk, icodec_tx_clk_rate * 256);
			clk_enable(icodec_tx_clk);
		}
		break;
	case Q6_ECODEC_TX:
		ecodec_clk_enable();
		break;
	case Q6_SDAC_TX:
		/* TODO: In QCT BSP, clk rate was set to 20480000 */
		sdac_clk_refcount++;
		if (sdac_clk_refcount == 1) {
			clk_set_rate(sdac_clk, 12288000);
			clk_enable(sdac_clk);
		}
		break;
	default:
		return;
	}
	audio_tx_device_group = device_group;
}

static void _audio_rx_clk_disable(void)
{
	pr_debug("[%s:%s] rx_clk_refcount = %d\n", __MM_FILE__, __func__,
		icodec_rx_clk_refcount);
	switch (audio_rx_device_group) {
	case Q6_ICODEC_RX:
		icodec_rx_clk_refcount--;
		if (icodec_rx_clk_refcount == 0) {
			clk_disable(icodec_rx_clk);
			audio_rx_device_group = -1;
		}
		break;
	case Q6_ECODEC_RX:
		ecodec_clk_disable(1, ADSP_PATH_RX);
		break;
	case Q6_SDAC_RX:
		sdac_clk_refcount--;
		if (sdac_clk_refcount == 0) {
			clk_disable(sdac_clk);
			audio_rx_device_group = -1;
		}
		break;
	default:
		pr_err("[%s:%s] invalid rx device group %d\n", __MM_FILE__,
				__func__, audio_rx_device_group);
		break;
	}
}

static void _audio_tx_clk_disable(void)
{
	pr_debug("[%s:%s] tx_clk_refcount = %d\n", __MM_FILE__, __func__,
		icodec_tx_clk_refcount);
	switch (audio_tx_device_group) {
	case Q6_ICODEC_TX:
		icodec_tx_clk_refcount--;
		if (icodec_tx_clk_refcount == 0) {
			clk_disable(icodec_tx_clk);
			audio_tx_device_group = -1;
		}
		break;
	case Q6_ECODEC_TX:
		ecodec_clk_disable(1, ADSP_PATH_TX);
		break;
	case Q6_SDAC_TX:
		sdac_clk_refcount--;
		if (sdac_clk_refcount == 0) {
			clk_disable(sdac_clk);
			audio_tx_device_group = -1;
		}
		break;
	default:
		pr_err("[%s:%s] invalid tx device group %d\n",
			__MM_FILE__, __func__, audio_tx_device_group);
		break;
	}
}

static void _audio_rx_clk_reinit(uint32_t rx_device, uint32_t acdb_id)
{
	uint32_t device_group = q6_device_to_codec(rx_device);

	pr_debug("[%s:%s] rx_device = 0x%x\n", __MM_FILE__, __func__,
		rx_device);
	if (device_group != audio_rx_device_group)
		_audio_rx_clk_disable();

	audio_rx_device_id = rx_device;
	audio_rx_path_id = q6_device_to_path(rx_device, acdb_id);

	if (device_group != audio_rx_device_group)
		_audio_rx_clk_enable();

}

static void _audio_tx_clk_reinit(uint32_t tx_device, uint32_t acdb_id)
{
	uint32_t device_group = q6_device_to_codec(tx_device);

	pr_debug("[%s:%s] tx_device = 0x%x\n", __MM_FILE__, __func__,
		tx_device);
	if (device_group != audio_tx_device_group)
		_audio_tx_clk_disable();

	audio_tx_device_id = tx_device;
	audio_tx_path_id = q6_device_to_path(tx_device, acdb_id);

	if (device_group != audio_tx_device_group)
		_audio_tx_clk_enable();
}

static DEFINE_MUTEX(audio_path_lock);
static int audio_rx_path_refcount;
static int audio_tx_path_refcount;

static int audio_rx_path_enable(int en, uint32_t acdb_id)
{
	pr_debug("[%s:%s] en = %d\n", __MM_FILE__, __func__, en);
	mutex_lock(&audio_path_lock);
	if (en) {
		audio_rx_path_refcount++;
		if (audio_rx_path_refcount == 1) {
			_audio_rx_clk_enable();
			_audio_rx_path_enable(0, acdb_id);
		}
	} else {
		audio_rx_path_refcount--;
		if (audio_rx_path_refcount == 0) {
			_audio_rx_path_disable();
			_audio_rx_clk_disable();
		}
	}
	mutex_unlock(&audio_path_lock);
	return 0;
}

static int audio_tx_path_enable(int en, uint32_t acdb_id)
{
	pr_debug("[%s:%s] en = %d\n", __MM_FILE__, __func__, en);
	mutex_lock(&audio_path_lock);
	if (en) {
		audio_tx_path_refcount++;
		if (audio_tx_path_refcount == 1) {
			_audio_tx_clk_enable();
			_audio_tx_path_enable(0, acdb_id);
		}
	} else {
		audio_tx_path_refcount--;
		if (audio_tx_path_refcount == 0) {
			_audio_tx_path_disable();
			_audio_tx_clk_disable();
		}
	}
	mutex_unlock(&audio_path_lock);
	return 0;
}

int q6audio_update_acdb(uint32_t id_src, uint32_t id_dst)
{
	int res;

	pr_debug("[%s:%s] id_src = 0x%x\n, id_dst = 0x%x\n", __MM_FILE__,
		__func__, id_src, id_dst);
	if (q6audio_init())
		return 0;

	mutex_lock(&audio_path_lock);

	if (q6_device_to_dir(id_dst) == Q6_RX)
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_RX_DEVICE, id_dst);
	else
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_TX_DEVICE, id_dst);
	res = audio_update_acdb(id_dst, id_src);
	if (res)
		goto done;

	qdsp6_standby(ac_control);
	qdsp6_start(ac_control);
done:
	mutex_unlock(&audio_path_lock);
	return res;
}

int q6audio_set_tx_mute(int mute)
{
	uint32_t adev;
	int rc;

	if (q6audio_init())
		return 0;

	mutex_lock(&audio_path_lock);

	if (mute == tx_mute_status) {
		mutex_unlock(&audio_path_lock);
		return 0;
	}

	adev = audio_tx_device_id;
	rc = audio_tx_mute(ac_control, adev, mute);

	/* DSP caches the requested MUTE state when it cannot apply the state
	  immediately. In that case, it returns EUNSUPPORTED and applies the
	  cached state later */
	if ((rc == ADSP_AUDIO_STATUS_SUCCESS) ||
			(rc == ADSP_AUDIO_STATUS_EUNSUPPORTED)) {
		pr_debug("[%s:%s] return status = %d\n",
			__MM_FILE__, __func__, rc);
		tx_mute_status = mute;
	}
	mutex_unlock(&audio_path_lock);
	return 0;
}

int q6audio_set_stream_volume(struct audio_client *ac, int vol)
{
	if (vol > 1200 || vol < -4000) {
		pr_err("[%s:%s] unsupported volume level %d\n", __MM_FILE__,
				__func__, vol);
		return -EINVAL;
	}
	mutex_lock(&audio_path_lock);
	audio_stream_mute(ac, 0);
	audio_stream_volume(ac, vol);
	mutex_unlock(&audio_path_lock);
	return 0;
}

int q6audio_set_rx_volume(int level)
{
	uint32_t adev;
	int vol;

	pr_debug("[%s:%s] level = %d\n", __MM_FILE__, __func__, level);
	if (q6audio_init())
		return 0;

	if (level < 0 || level > 100)
		return -EINVAL;

	mutex_lock(&audio_path_lock);
	adev = ADSP_AUDIO_DEVICE_ID_VOICE;

	if (level) {
		vol = q6_device_volume(audio_rx_device_id, level);
		audio_rx_mute(ac_control, adev, 0);
		audio_rx_volume(ac_control, adev, vol);
	} else
		audio_rx_mute(ac_control, adev, 1);

	rx_vol_level = level;
	mutex_unlock(&audio_path_lock);
	return 0;
}

static void do_rx_routing(uint32_t device_id, uint32_t acdb_id)
{
	pr_debug("[%s:%s] device_id = 0x%x, acdb_id = 0x%x\n", __MM_FILE__,
		__func__, device_id, acdb_id);
	if (device_id == audio_rx_device_id &&
		audio_rx_path_id == q6_device_to_path(device_id, acdb_id)) {
		if (acdb_id != rx_acdb) {
			qdsp6_devchg_notify(ac_control, ADSP_AUDIO_RX_DEVICE, device_id);
			audio_update_acdb(device_id, acdb_id);
			qdsp6_standby(ac_control);
			qdsp6_start(ac_control);
		}
		return;
	}

	if (audio_rx_path_refcount > 0) {
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_RX_DEVICE, device_id);
		_audio_rx_path_disable();
		_audio_rx_clk_reinit(device_id, acdb_id);
		_audio_rx_path_enable(1, acdb_id);
	} else {
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_RX_DEVICE,
					 device_id);
		audio_update_acdb(device_id, acdb_id);
		qdsp6_standby(ac_control);
		qdsp6_start(ac_control);
		audio_rx_device_id = device_id;
		audio_rx_path_id = q6_device_to_path(device_id, acdb_id);
	}
}

static void do_tx_routing(uint32_t device_id, uint32_t acdb_id)
{
	pr_debug("[%s:%s] device_id = 0x%x, acdb_id = 0x%x\n", __MM_FILE__,
		__func__, device_id, acdb_id);
	if (device_id == audio_tx_device_id &&
		audio_tx_path_id == q6_device_to_path(device_id, acdb_id)) {
		if (acdb_id != tx_acdb) {
			qdsp6_devchg_notify(ac_control, ADSP_AUDIO_TX_DEVICE,
						 device_id);
			audio_update_acdb(device_id, acdb_id);
			qdsp6_standby(ac_control);
			qdsp6_start(ac_control);
		}
		return;
	}

	if (audio_tx_path_refcount > 0) {
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_TX_DEVICE, device_id);
		_audio_tx_path_disable();
		_audio_tx_clk_reinit(device_id, acdb_id);
		_audio_tx_path_enable(1, acdb_id);
	} else {
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_TX_DEVICE,
					 device_id);
		audio_update_acdb(device_id, acdb_id);
		qdsp6_standby(ac_control);
		qdsp6_start(ac_control);
		audio_tx_device_id = device_id;
		audio_tx_path_id = q6_device_to_path(device_id, acdb_id);
		tx_acdb = acdb_id;
	}
}

int q6audio_do_routing(uint32_t device_id, uint32_t acdb_id)
{
	if (q6audio_init())
		return 0;

	mutex_lock(&audio_path_lock);

	switch(q6_device_to_dir(device_id)) {
	case Q6_RX:
		do_rx_routing(device_id, acdb_id);
		break;
	case Q6_TX:
		do_tx_routing(device_id, acdb_id);
		break;
	}

	mutex_unlock(&audio_path_lock);
	return 0;
}

int q6audio_set_route(const char *name)
{
	uint32_t route;
	if (!strcmp(name, "speaker")) {
		route = ADIE_PATH_SPEAKER_STEREO_RX;
	} else if (!strcmp(name, "headphones")) {
		route = ADIE_PATH_HEADSET_STEREO_RX;
	} else if (!strcmp(name, "handset")) {
		route = ADIE_PATH_HANDSET_RX;
	} else {
		return -EINVAL;
	}

	mutex_lock(&audio_path_lock);
	if (route == audio_rx_path_id)
		goto done;

	audio_rx_path_id = route;

	if (audio_rx_path_refcount > 0) {
		_audio_rx_path_disable();
		_audio_rx_path_enable(1, 0);
	}
	if (audio_tx_path_refcount > 0) {
		_audio_tx_path_disable();
		_audio_tx_path_enable(1, 0);
	}
done:
	mutex_unlock(&audio_path_lock);
	return 0;
}

static int audio_stream_equalizer(struct audio_client *ac, void *eq_config)
{
	int i;
	struct adsp_set_equalizer_command rpc;
	struct adsp_audio_eq_stream_config *eq_cfg;
	eq_cfg = (struct adsp_audio_eq_stream_config *) eq_config;

	memset(&rpc, 0, sizeof(rpc));

	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_SET_SESSION_EQ_CONFIG;
	rpc.enable = eq_cfg->enable;
	rpc.num_bands = eq_cfg->num_bands;
	for (i = 0; i < eq_cfg->num_bands; i++) {
		rpc.eq_bands[i].band_idx = eq_cfg->eq_bands[i].band_idx;
		rpc.eq_bands[i].filter_type = eq_cfg->eq_bands[i].filter_type;
		rpc.eq_bands[i].center_freq_hz =
					eq_cfg->eq_bands[i].center_freq_hz;
		rpc.eq_bands[i].filter_gain = eq_cfg->eq_bands[i].filter_gain;
		rpc.eq_bands[i].q_factor = eq_cfg->eq_bands[i].q_factor;
	}
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

int q6audio_set_stream_eq_pcm(struct audio_client *ac, void *eq_config)
{
	int rc = 0;
	mutex_lock(&audio_path_lock);
	rc = audio_stream_equalizer(ac, eq_config);
	mutex_unlock(&audio_path_lock);
	return rc;
}

struct audio_client *q6audio_open_auxpcm(uint32_t rate,
				      uint32_t channels, uint32_t flags, uint32_t acdb_id)
{
	int rc, retry = 5;
	struct audio_client *ac;

	pr_debug("[%s:%s] rate = %d, channels = %d\n", __MM_FILE__, __func__,
		rate, channels);
	if (q6audio_init())
		return NULL;
	ac = audio_client_alloc(0);
	if (!ac)
		return NULL;

	ac->flags = flags;

	mutex_lock(&audio_path_lock);

	if (ac->flags & AUDIO_FLAG_WRITE) {
		audio_tx_path_refcount++;
		if (audio_tx_path_refcount == 1) {
			tx_clk_freq = rate;
			_audio_tx_clk_enable();
			_audio_tx_path_enable(0, acdb_id);
		}
	} else {
		audio_rx_path_refcount++;
		if (audio_rx_path_refcount == 1) {
			_audio_rx_clk_enable();
			_audio_rx_path_enable(0, acdb_id);
		}
	}

	ecodec_clk_enable();

	for (retry = 5;; retry--) {
		if (ac->flags & AUDIO_FLAG_WRITE)
			rc = audio_auxpcm_out_open(ac, rate, channels);
		else
			rc = audio_auxpcm_in_open(ac, rate, channels);
		if (rc == 0)
			break;
		if (retry == 0)
			q6audio_dsp_not_responding();

		pr_err("[%s:%s] open pcm error %d, retrying\n",
			__MM_FILE__, __func__, rc);
		msleep(1);
	}

	mutex_unlock(&audio_path_lock);

	for (retry = 5;; retry--) {
		rc = audio_command(ac, ADSP_AUDIO_IOCTL_CMD_SESSION_START);
		if (rc == 0)
			break;
		if (retry == 0)
			q6audio_dsp_not_responding();

		pr_err("[%s:%s] stream start error %d, retrying\n",
			__MM_FILE__, __func__, rc);
	}
	audio_prevent_sleep();
	return ac;

}

struct audio_client *q6audio_open_pcm(uint32_t bufsz, uint32_t rate,
		      uint32_t channels, uint32_t flags, uint32_t acdb_id)
{
	int rc, retry = 5;
	struct audio_client *ac;

	pr_debug("[%s:%s] bufsz = %d, rate = %d, channels = %d\n", __MM_FILE__,
		__func__, bufsz, rate, channels);
	if (q6audio_init())
		return 0;

	ac = audio_client_alloc(bufsz);
	if (!ac)
		return 0;

	ac->flags = flags;

	mutex_lock(&audio_path_lock);

	if (ac->flags & AUDIO_FLAG_WRITE) {
		audio_rx_path_refcount++;
		if (audio_rx_path_refcount == 1) {
			_audio_rx_clk_enable();
			q6_rx_path_enable(0, acdb_id);
			adie_rx_path_enable(acdb_id);
		}
	} else {
		/* TODO: consider concurrency with voice call */
		audio_tx_path_refcount++;
		if (audio_tx_path_refcount == 1) {
			tx_clk_freq = rate;
			_audio_tx_clk_enable();
			_audio_tx_path_enable(0, acdb_id);
		}
	}

	for (retry = 5;;retry--) {
		if (ac->flags & AUDIO_FLAG_WRITE)
			rc = audio_out_open(ac, bufsz, rate, channels);
		else
			rc = audio_in_open(ac, bufsz, flags, rate, channels);
		if (rc == 0)
			break;
		if (retry == 0)
			q6audio_dsp_not_responding();

		pr_err("[%s:%s] open pcm error %d, retrying\n",
			__MM_FILE__, __func__, rc);
		msleep(1);
	}

	if (ac->flags & AUDIO_FLAG_WRITE) {
		if (audio_rx_path_refcount == 1)
			audio_rx_analog_enable(1);
	}
	mutex_unlock(&audio_path_lock);

	for (retry = 5;;retry--) {
		rc = audio_command(ac, ADSP_AUDIO_IOCTL_CMD_SESSION_START);
		if (rc == 0)
			break;
		if (retry == 0)
			q6audio_dsp_not_responding();

		pr_err("[%s:%s] stream start error %d, retrying\n",
			__MM_FILE__, __func__, rc);
	}

	if (!(ac->flags & AUDIO_FLAG_WRITE)) {
		ac->buf[0].used = 1;
		ac->buf[1].used = 1;
		q6audio_read(ac, &ac->buf[0]);
		q6audio_read(ac, &ac->buf[1]);
	}

	audio_prevent_sleep();
	return ac;
}

int q6audio_close(struct audio_client *ac)
{
	audio_close(ac);
	if (ac->flags & AUDIO_FLAG_WRITE)
		audio_rx_path_enable(0, 0);
	else
		audio_tx_path_enable(0, 0);
	audio_client_free(ac);
	audio_allow_sleep();
	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	return 0;
}

int q6audio_auxpcm_close(struct audio_client *ac)
{
	audio_close(ac);
	if (ac->flags & AUDIO_FLAG_WRITE) {
		audio_tx_path_enable(0, 0);
		ecodec_clk_disable(0, ADSP_PATH_RX);
	} else {
		audio_rx_path_enable(0, 0);
		ecodec_clk_disable(0, ADSP_PATH_TX);
	}

	audio_client_free(ac);
	audio_allow_sleep();
	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	return 0;
}
struct audio_client *q6voice_open(uint32_t flags)
{
	struct audio_client *ac;

	pr_debug("[%s:%s] flags = %d\n", __MM_FILE__, __func__, flags);
	if (q6audio_init())
		return 0;

	ac = audio_client_alloc(0);
	if (!ac)
		return 0;

	ac->flags = flags;
	if (ac->flags & AUDIO_FLAG_WRITE)
		audio_rx_path_enable(1, rx_acdb);
	else {
		if (!audio_tx_path_refcount)
			tx_clk_freq = 8000;
		audio_tx_path_enable(1, tx_acdb);
	}

	return ac;
}

int q6voice_close(struct audio_client *ac)
{
	if (ac->flags & AUDIO_FLAG_WRITE)
		audio_rx_path_enable(0, 0);
	else
		audio_tx_path_enable(0, 0);

	tx_mute_status = 0;
	audio_client_free(ac);
	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	return 0;
}

struct audio_client *q6audio_open_mp3(uint32_t bufsz, uint32_t rate,
				      uint32_t channels, uint32_t acdb_id)
{
	struct audio_client *ac;

	pr_debug("[%s:%s] bufsz = %d, rate = %d\n, channels = %d",
		__MM_FILE__, __func__, bufsz, rate, channels);

	if (q6audio_init())
		return 0;

	ac = audio_client_alloc(bufsz);
	if (!ac)
		return 0;

	ac->flags = AUDIO_FLAG_WRITE;
	audio_rx_path_enable(1, acdb_id);

	audio_mp3_open(ac, bufsz, rate, channels);
	audio_command(ac, ADSP_AUDIO_IOCTL_CMD_SESSION_START);

	mutex_lock(&audio_path_lock);
	audio_rx_mute(ac_control, audio_rx_device_id, 0);
	audio_rx_volume(ac_control, audio_rx_device_id,
			q6_device_volume(audio_rx_device_id, rx_vol_level));
	mutex_unlock(&audio_path_lock);
	return ac;
}

struct audio_client *q6audio_open_dtmf(uint32_t rate,
				      uint32_t channels, uint32_t acdb_id)
{
	struct audio_client *ac;

	pr_debug("[%s:%s] rate = %d\n, channels = %d", __MM_FILE__, __func__,
		 rate, channels);
	if (q6audio_init())
		return 0;

	ac = audio_client_alloc(0);
	if (!ac)
		return 0;

	ac->flags = AUDIO_FLAG_WRITE;
	audio_rx_path_enable(1, acdb_id);

	audio_dtmf_open(ac, rate, channels);
	audio_command(ac, ADSP_AUDIO_IOCTL_CMD_SESSION_START);

	mutex_lock(&audio_path_lock);
	audio_rx_mute(ac_control, audio_rx_device_id, 0);
	audio_rx_volume(ac_control, audio_rx_device_id,
		q6_device_volume(audio_rx_device_id, rx_vol_level));
	mutex_unlock(&audio_path_lock);

	return ac;
}

int q6audio_play_dtmf(struct audio_client *ac, uint16_t dtmf_hi,
			 uint16_t dtmf_low, uint16_t duration, uint16_t rx_gain)
{
	struct adsp_audio_dtmf_start_command dtmf_cmd;

	pr_debug("[%s:%s] high = %d, low = %d\n", __MM_FILE__, __func__,
		dtmf_hi, dtmf_low);

	dtmf_cmd.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_SESSION_DTMF_START;
	dtmf_cmd.hdr.response_type = ADSP_AUDIO_RESPONSE_COMMAND;
	dtmf_cmd.tone1_hz = dtmf_hi;
	dtmf_cmd.tone2_hz = dtmf_low;
	dtmf_cmd.duration_usec = duration * 1000;
	dtmf_cmd.gain_mb = rx_gain;

	return audio_ioctl(ac, &dtmf_cmd,
		 sizeof(struct adsp_audio_dtmf_start_command));

}

int q6audio_mp3_close(struct audio_client *ac)
{
	pr_debug("[%s:%s]\n", __MM_FILE__, __func__);
	audio_close(ac);
	audio_rx_path_enable(0, 0);
	audio_client_free(ac);
	return 0;
}


struct audio_client *q6audio_open_aac(uint32_t bufsz, uint32_t samplerate,
					uint32_t channels, uint32_t bitrate,
					uint32_t stream_format, uint32_t flags,
					uint32_t acdb_id)
{
	struct audio_client *ac;

	pr_debug("[%s:%s] bufsz = %d, samplerate = %d, channels = %d\n",
		__MM_FILE__, __func__, bufsz, samplerate, channels);

	if (q6audio_init())
		return 0;

	ac = audio_client_alloc(bufsz);
	if (!ac)
		return 0;

	ac->flags = flags;

	if (ac->flags & AUDIO_FLAG_WRITE)
		audio_rx_path_enable(1, acdb_id);
	else{
		if (!audio_tx_path_refcount)
			tx_clk_freq = 48000;
		audio_tx_path_enable(1, acdb_id);
	}

	audio_aac_open(ac, bufsz, samplerate, channels, bitrate, flags,
							stream_format);
	audio_command(ac, ADSP_AUDIO_IOCTL_CMD_SESSION_START);

	if (!(ac->flags & AUDIO_FLAG_WRITE)) {
		ac->buf[0].used = 1;
		ac->buf[1].used = 1;
		q6audio_read(ac, &ac->buf[0]);
		q6audio_read(ac, &ac->buf[1]);
	}
	audio_prevent_sleep();
	return ac;
}


struct audio_client *q6audio_open_qcp(uint32_t bufsz, uint32_t min_rate,
					uint32_t max_rate, uint32_t flags,
					uint32_t format, uint32_t acdb_id)
{
	struct audio_client *ac;

	pr_debug("[%s:%s] bufsz = %d\n", __MM_FILE__, __func__, bufsz);

	if (q6audio_init())
		return 0;

	ac = audio_client_alloc(bufsz);
	if (!ac)
		return 0;

	ac->flags = flags;

	if (ac->flags & AUDIO_FLAG_WRITE)
		audio_rx_path_enable(1, acdb_id);
	else{
		if (!audio_tx_path_refcount)
			tx_clk_freq = 8000;
		audio_tx_path_enable(1, acdb_id);
	}

	audio_qcp_open(ac, bufsz, min_rate, max_rate, flags, format);
	audio_command(ac, ADSP_AUDIO_IOCTL_CMD_SESSION_START);

	if (!(ac->flags & AUDIO_FLAG_WRITE)) {
		ac->buf[0].used = 1;
		ac->buf[1].used = 1;
		q6audio_read(ac, &ac->buf[0]);
		q6audio_read(ac, &ac->buf[1]);
	}
	audio_prevent_sleep();
	return ac;
}

struct audio_client *q6audio_open_amrnb(uint32_t bufsz, uint32_t enc_mode,
					uint32_t dtx_mode_enable,
					uint32_t flags, uint32_t acdb_id)
{
	struct audio_client *ac;

	pr_debug("[%s:%s] bufsz = %d, dtx_mode = %d\n", __MM_FILE__,
			__func__, bufsz, dtx_mode_enable);

	if (q6audio_init())
		return 0;

	ac = audio_client_alloc(bufsz);
	if (!ac)
		return 0;

	ac->flags = flags;
	if (ac->flags & AUDIO_FLAG_WRITE)
		audio_rx_path_enable(1, acdb_id);
	else{
		if (!audio_tx_path_refcount)
			tx_clk_freq = 8000;
		audio_tx_path_enable(1, acdb_id);
	}

	audio_amrnb_open(ac, bufsz, enc_mode, flags, dtx_mode_enable);
	audio_command(ac, ADSP_AUDIO_IOCTL_CMD_SESSION_START);

	if (!(ac->flags & AUDIO_FLAG_WRITE)) {
		ac->buf[0].used = 1;
		ac->buf[1].used = 1;
		q6audio_read(ac, &ac->buf[0]);
		q6audio_read(ac, &ac->buf[1]);
	}
	audio_prevent_sleep();
	return ac;
}

int q6audio_async(struct audio_client *ac)
{
	struct adsp_command_hdr rpc;
	pr_debug("[%s:%s] ac = %p\n", __MM_FILE__, __func__, ac);
	memset(&rpc, 0, sizeof(rpc));
	rpc.opcode = ADSP_AUDIO_IOCTL_CMD_STREAM_EOS;
	rpc.response_type = ADSP_AUDIO_RESPONSE_ASYNC;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}
