/* arch/arm/mach-msm/qdsp6/audiov2/q6audio.c
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
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

#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>

#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/android_pmem.h>
#include <linux/gpio.h>
#include <mach/msm_qdsp6_audiov2.h>

#include "../dal.h"
#include "dal_audio.h"
#include "dal_audio_format.h"
#include "dal_acdb.h"
#include "dal_adie.h"
#include "q6audio_devices.h"

struct q6_hw_info {
	int min_gain;
	int max_gain;
};

/* TODO: provide mechanism to configure from board file */

static struct q6_hw_info q6_audio_hw[Q6_HW_COUNT] = {
	[Q6_HW_HANDSET] = {
		.min_gain = -2000,
		.max_gain = 0,
	},
	[Q6_HW_HEADSET] = {
		.min_gain = -2000,
		.max_gain = 0,
	},
	[Q6_HW_SPEAKER] = {
		.min_gain = -1500,
		.max_gain = 0,
	},
	[Q6_HW_TTY] = {
		.min_gain = -2000,
		.max_gain = 0,
	},
	[Q6_HW_BT_SCO] = {
		.min_gain = -2000,
		.max_gain = 0,
	},
	[Q6_HW_BT_A2DP] = {
		.min_gain = -2000,
		.max_gain = 0,
	},
};

static struct wake_lock idlelock;
static int idlecount;
static DEFINE_MUTEX(idlecount_lock);

void audio_prevent_sleep(void)
{
	mutex_lock(&idlecount_lock);
	if (++idlecount == 1)
		wake_lock(&idlelock);
	mutex_unlock(&idlecount_lock);
}

void audio_allow_sleep(void)
{
	mutex_lock(&idlecount_lock);
	if (--idlecount == 0)
		wake_unlock(&idlelock);
	mutex_unlock(&idlecount_lock);
}

static struct clk *icodec_rx_clk;
static struct clk *icodec_tx_clk;
static struct clk *ecodec_clk;
static struct clk *sdac_clk;

static struct q6audio_analog_ops default_analog_ops;
static struct q6audio_analog_ops *analog_ops = &default_analog_ops;
uint32_t tx_clk_freq = 8000;
static int tx_mute_status;

void q6audio_register_analog_ops(struct q6audio_analog_ops *ops)
{
	analog_ops = ops;
}

static struct q6_device_info *q6_lookup_device(uint32_t device_id)
{
	struct q6_device_info *di = q6_audio_devices;
	for (;;) {
		if (di->id == device_id)
			return di;
		if (di->id == 0) {
			pr_err("q6_lookup_device: bogus id 0x%08x\n",
			       device_id);
			return di;
		}
		di++;
	}
}

static uint32_t q6_device_to_codec(uint32_t device_id)
{
	struct q6_device_info *di = q6_lookup_device(device_id);
	return di->codec;
}

static uint32_t q6_device_to_dir(uint32_t device_id)
{
	struct q6_device_info *di = q6_lookup_device(device_id);
	return di->dir;
}

static uint32_t q6_device_to_cad_id(uint32_t device_id)
{
	struct q6_device_info *di = q6_lookup_device(device_id);
	return di->cad_id;
}

static uint32_t q6_device_to_path(uint32_t device_id)
{
	struct q6_device_info *di = q6_lookup_device(device_id);
	return di->path;
}

static uint32_t q6_device_to_rate(uint32_t device_id)
{
	struct q6_device_info *di = q6_lookup_device(device_id);
	return di->rate;
}

int q6_device_volume(uint32_t device_id, int level)
{
	struct q6_device_info *di = q6_lookup_device(device_id);
	struct q6_hw_info *hw;

	hw = &q6_audio_hw[di->hw];

	return hw->min_gain + ((hw->max_gain - hw->min_gain) * level) / 100;
}

static inline int adie_open(struct dal_client *client)
{
	return dal_call_f0(client, DAL_OP_OPEN, 0);
}

static inline int adie_close(struct dal_client *client)
{
	return dal_call_f0(client, DAL_OP_CLOSE, 0);
}

static inline int adie_set_path(struct dal_client *client,
				uint32_t *adie_params, uint32_t size)
{
	uint32_t tmp;
	return dal_call(client, ADIE_OP_SET_PATH, 5, adie_params, size,
		(void *)&tmp, sizeof(uint32_t));

}

static inline int adie_proceed_to_stage(struct dal_client *client,
					uint32_t path_type, uint32_t stage)
{
	return dal_call_f1(client, ADIE_OP_PROCEED_TO_STAGE,
			   path_type, stage);
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

/* 4k DMA scratch page used for exchanging acdb device config tables
 * and stream format descriptions with the DSP.
 */
char *audio_data;
int32_t audio_phys;

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
			return n;
		}
	}
	mutex_unlock(&session_lock);
	return -ENOMEM;
}

static void session_free(int n, struct audio_client *ac)
{
	mutex_lock(&session_lock);
	if (session[n] == ac)
		session[n] = 0;
	mutex_unlock(&session_lock);
}

static void audio_client_free(struct audio_client *ac)
{
	session_free(ac->session, ac);

	if (ac->buf[0].data)
		pmem_kfree(ac->buf[0].phys);
	if (ac->buf[1].data)
		pmem_kfree(ac->buf[1].phys);
	kfree(ac);
}

static struct audio_client *audio_client_alloc(unsigned bufsz)
{
	struct audio_client *ac;
	int n;

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
	pr_err("pmem_kalloc failed\n");
	session_free(n, ac);
fail_session:
	audio_client_free(ac);
	return 0;
}

static int audio_ioctl(struct audio_client *ac, void *ptr, uint32_t len)
{
	struct adsp_command_hdr *hdr = ptr;
	uint32_t tmp;
	int r;

	hdr->size = len - sizeof(u32);
	hdr->dest = AUDIO_ADDR(DOMAIN_DSP, ac->session, 0);
	hdr->src = AUDIO_ADDR(DOMAIN_APP, ac->session, 0);
	hdr->context = ac->session;
	ac->cb_status = -EBUSY;
	r = dal_call(ac->client, AUDIO_OP_CONTROL, 5, ptr, len,
						&tmp, sizeof(tmp));
	if (r != 4)
		return -EIO;
	wait_event(ac->wait, (ac->cb_status != -EBUSY));
	return tmp;
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

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_DEVICE;
	rpc.hdr.dest = AUDIO_ADDR(DOMAIN_DSP, ac->session, 0);
	rpc.hdr.src = AUDIO_ADDR(DOMAIN_APP, ac->session, 0);
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}


static int audio_close(struct audio_client *ac)
{
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
	rpc.hdr.dest = AUDIO_ADDR(DOMAIN_DSP, ac->session, 0);
	rpc.hdr.src = AUDIO_ADDR(DOMAIN_APP, ac->session, 0);
	rpc.device_id = device_id;
	rpc.phys_addr = audio_phys;
	rpc.phys_size = size;
	rpc.phys_used = size;

	if (q6_device_to_dir(device_id) == Q6_TX)
		rpc.hdr.data = tx_clk_freq;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

int q6audio_read(struct audio_client *ac, struct audio_buffer *ab)
{
	struct adsp_buffer_command rpc;
	uint32_t res;
	int r;

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.size = sizeof(rpc) - sizeof(u32);
	rpc.hdr.dest = AUDIO_ADDR(DOMAIN_DSP, ac->session, 0);
	rpc.hdr.src = AUDIO_ADDR(DOMAIN_APP, ac->session, 0);
	rpc.hdr.context = ac->session;
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_DATA_TX;
	rpc.buffer.addr = ab->phys;
	rpc.buffer.max_size = ab->size;
	rpc.buffer.actual_size = ab->actual_size;

	r = dal_call(ac->client, AUDIO_OP_DATA, 5, &rpc, sizeof(rpc),
		     &res, sizeof(res));

	if ((r == sizeof(res)))
		return 0;

	return -EIO;

}

int q6audio_write(struct audio_client *ac, struct audio_buffer *ab)
{
	struct adsp_buffer_command rpc;
	uint32_t res;
	int r;

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.size = sizeof(rpc) - sizeof(u32);
	rpc.hdr.src = AUDIO_ADDR(DOMAIN_APP, ac->session, 0);
	rpc.hdr.dest = AUDIO_ADDR(DOMAIN_DSP, ac->session, 0);
	rpc.hdr.context = ac->session;
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_DATA_RX;
	rpc.buffer.addr = ab->phys;
	rpc.buffer.max_size = ab->size;
	rpc.buffer.actual_size = ab->actual_size;

	r = dal_call(ac->client, AUDIO_OP_DATA, 5, &rpc, sizeof(rpc),
		     &res, sizeof(res));
	return 0;
}

static int audio_rx_volume(struct audio_client *ac, uint32_t dev_id,
				 int32_t volume)
{
	struct adsp_set_dev_volume_command rpc;

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_SET_DEVICE_VOL;
	rpc.hdr.dest = AUDIO_ADDR(DOMAIN_DSP, ac->session, 0);
	rpc.hdr.src = AUDIO_ADDR(DOMAIN_APP, ac->session, 0);
	rpc.device_id = dev_id;
	rpc.path = ADSP_PATH_RX;
	rpc.volume = volume;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_rx_mute(struct audio_client *ac, uint32_t dev_id, int mute)
{
	struct adsp_set_dev_mute_command rpc;

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_SET_DEVICE_MUTE;
	rpc.hdr.dest = AUDIO_ADDR(DOMAIN_DSP, ac->session, 0);
	rpc.hdr.src = AUDIO_ADDR(DOMAIN_APP, ac->session, 0);
	rpc.device_id = dev_id;
	rpc.path = ADSP_PATH_RX;
	rpc.mute = !!mute;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_tx_volume(struct audio_client *ac, uint32_t dev_id,
				 int32_t volume)
{
	struct adsp_set_dev_volume_command rpc;

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_SET_DEVICE_VOL;
	rpc.hdr.dest = AUDIO_ADDR(DOMAIN_DSP, ac->session, 0);
	rpc.hdr.src = AUDIO_ADDR(DOMAIN_APP, ac->session, 0);
	rpc.device_id = dev_id;
	rpc.path = ADSP_PATH_TX;
	rpc.volume = volume;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int audio_tx_mute(struct audio_client *ac, uint32_t dev_id, int mute)
{
	struct adsp_set_dev_mute_command rpc;

	memset(&rpc, 0, sizeof(rpc));
	rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_SET_DEVICE_MUTE;
	rpc.hdr.dest = AUDIO_ADDR(DOMAIN_DSP, ac->session, 0);
	rpc.hdr.src = AUDIO_ADDR(DOMAIN_APP, ac->session, 0);
	rpc.device_id = dev_id;
	rpc.path = ADSP_PATH_TX;
	rpc.mute = !!mute;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static void callback(void *data, int len, void *cookie)
{
	struct adsp_event_hdr *e = data;
	struct audio_client *ac;
	struct adsp_buffer_event *abe = data;

	if (e->context >= SESSION_MAX) {
		pr_err("audio callback: bogus session %d\n",
		       e->context);
		return;
	}
	ac = session[e->context];
	if (!ac) {
		pr_err("audio callback: unknown session %d\n",
		       e->context);
		return;
	}

	if (e->event_id == ADSP_AUDIO_IOCTL_CMD_STREAM_EOS) {
		pr_info("playback done\n");
		if (e->status)
			pr_err("playback status %d\n", e->status);
		if (ac->cb_status == -EBUSY) {
			ac->cb_status = e->status;
			wake_up(&ac->wait);
		}
		return;
	}

	if (e->event_id == ADSP_AUDIO_EVT_STATUS_BUF_DONE) {
		if (e->status)
			pr_err("buffer status %d\n", e->status);

		ac->buf[ac->dsp_buf].actual_size = abe->buffer.actual_size;
		ac->buf[ac->dsp_buf].used = 0;
		ac->dsp_buf ^= 1;
		wake_up(&ac->wait);
		return;
	}

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
	int res = -ENODEV;

	mutex_lock(&audio_lock);
	if (ac_control) {
		res = 0;
		goto done;
	}

	icodec_rx_clk = clk_get(0, "icodec_rx_clk");
	icodec_tx_clk = clk_get(0, "icodec_tx_clk");
	ecodec_clk = clk_get(0, "ecodec_clk");
	sdac_clk = clk_get(0, "sdac_clk");

	tx_mute_status = 0;
	audio_phys = pmem_kalloc(4096, PMEM_MEMTYPE_EBI1|PMEM_ALIGNMENT_4K);
	audio_data = ioremap(audio_phys, 4096);
	if (!audio_data) {
		pr_err("pmem kalloc failed\n");
		res = -ENOMEM;
		goto done;
	}

	adsp = dal_attach(AUDIO_DAL_DEVICE, AUDIO_DAL_PORT, 1,
			  callback, 0);
	if (!adsp) {
		pr_err("audio_init: cannot attach to adsp\n");
		res = -ENODEV;
		goto done;
	}
	if (check_version(adsp, AUDIO_DAL_VERSION) != 0) {
		pr_err("Incompatible adsp version\n");
		res = -ENODEV;
		goto done;
	}

	audio_init(adsp);

	ac = audio_client_alloc(0);
	if (!ac) {
		pr_err("audio_init: cannot allocate client\n");
		res = -ENOMEM;
		goto done;
	}

	if (audio_open_control(ac)) {
		pr_err("audio_init: cannot open control channel\n");
		res = -ENODEV;
		goto done;
	}

	acdb = dal_attach(ACDB_DAL_DEVICE, ACDB_DAL_PORT, 0, 0, 0);
	if (!acdb) {
		pr_err("audio_init: cannot attach to acdb channel\n");
		res = -ENODEV;
		goto done;
	}
	if (check_version(acdb, ACDB_DAL_VERSION) != 0) {
		pr_err("Incompatablie acdb version\n");
		res = -ENODEV;
		goto done;
	}


	adie = dal_attach(ADIE_DAL_DEVICE, ADIE_DAL_PORT, 0, 0, 0);
	if (!adie) {
		pr_err("audio_init: cannot attach to adie\n");
		res = -ENODEV;
		goto done;
	}
	if (check_version(adie, ADIE_DAL_VERSION) != 0) {
		pr_err("Incompatablie adie version\n");
		res = -ENODEV;
		goto done;
	}
	if (analog_ops->init)
		analog_ops->init();

	res = 0;
	ac_control = ac;

	wake_lock_init(&idlelock, WAKE_LOCK_IDLE, "audio_pcm_idle");
done:
	if ((res < 0) && ac)
		audio_client_free(ac);
	mutex_unlock(&audio_lock);

	return res;
}

static int acdb_get_config_table(uint32_t device_id, uint32_t sample_rate)
{
	struct acdb_cmd_device_table rpc;
	struct acdb_result res;
	int r;

	if (q6audio_init())
		return 0;

	memset(audio_data, 0, 4096);
	memset(&rpc, 0, sizeof(rpc));

	rpc.size = sizeof(rpc) - (2 * sizeof(uint32_t));
	rpc.command_id = ACDB_GET_DEVICE_TABLE;
	rpc.device_id = q6_device_to_cad_id(device_id);
	rpc.network_id = 0x00010023;
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
	rpc.hdr.dest = AUDIO_ADDR(DOMAIN_DSP, ac->session, 0);
	rpc.hdr.src = AUDIO_ADDR(DOMAIN_APP, ac->session, 0);

	if (dev_type == ADSP_AUDIO_RX_DEVICE) {
		rpc.old_device = audio_rx_device_id;
		rpc.new_device = dev_id;
	} else {
		rpc.old_device = audio_tx_device_id;
		rpc.new_device = dev_id;
	}
	rpc.device_class = 0;
	rpc.device_type = dev_type;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}

static int qdsp6_standby(struct audio_client *ac)
{
	return audio_command(ac, ADSP_AUDIO_IOCTL_CMD_DEVICE_SWITCH_STANDBY);
}

static int qdsp6_start(struct audio_client *ac)
{
	return audio_command(ac, ADSP_AUDIO_IOCTL_CMD_DEVICE_SWITCH_COMMIT);
}

static void audio_rx_analog_enable(int en)
{
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
	}
}

static void audio_tx_analog_enable(int en)
{
	switch (audio_tx_device_id) {
	case ADSP_AUDIO_DEVICE_ID_HANDSET_MIC:
	case ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MIC:
		if (analog_ops->int_mic_enable)
			analog_ops->int_mic_enable(en);
		break;
	case ADSP_AUDIO_DEVICE_ID_HEADSET_MIC:
	case ADSP_AUDIO_DEVICE_ID_TTY_HEADSET_MIC:
		if (analog_ops->ext_mic_enable)
			analog_ops->ext_mic_enable(en);
		break;
	case ADSP_AUDIO_DEVICE_ID_BT_SCO_MIC:
		if (analog_ops->bt_sco_enable)
			analog_ops->bt_sco_enable(en);
		break;
	}
}

static void _audio_rx_path_enable(void)
{
	uint32_t adev, sample_rate;
	int sz;
	uint32_t adie_params[5];

	adev = audio_rx_device_id;
	sample_rate = q6_device_to_rate(adev);

	sz = acdb_get_config_table(adev, sample_rate);
	audio_set_table(ac_control, adev, sz);

	adie_params[0] = 4*sizeof(uint32_t);
	adie_params[1] = audio_rx_path_id;
	adie_params[2] = ADIE_PATH_RX;
	adie_params[3] = 48000;
	adie_params[4] = 256;
	/*check for errors here*/
	if (!adie_set_path(adie, adie_params, sizeof(adie_params)))
		pr_err("adie set rx path failed\n");

	adie_proceed_to_stage(adie, ADIE_PATH_RX,
				ADIE_STAGE_DIGITAL_READY);
	adie_proceed_to_stage(adie, ADIE_PATH_RX,
				ADIE_STAGE_DIGITAL_ANALOG_READY);

	audio_rx_analog_enable(1);

	audio_rx_mute(ac_control, adev, 0);

	audio_rx_volume(ac_control, adev, q6_device_volume(adev, 100));
}

static void _audio_tx_path_enable(void)
{
	uint32_t adev;
	int sz;
	uint32_t adie_params[5];

	adev = audio_tx_device_id;

	pr_info("audiolib: load %08x cfg table\n", adev);

	if (tx_clk_freq > 16000) {
		adie_params[3] = 48000;
		sz = acdb_get_config_table(adev, 48000);

	} else if (tx_clk_freq > 8000) {
		adie_params[3] = 16000;
		sz = acdb_get_config_table(adev, 16000);
	} else {

		adie_params[3] = 8000;
		sz = acdb_get_config_table(adev, 8000);
	}

	pr_info("cfg table is %d bytes\n", sz);
	audio_set_table(ac_control, adev, sz);

	pr_info("audiolib: set adie tx path\n");

	adie_params[0] = 4*sizeof(uint32_t);
	adie_params[1] = audio_tx_path_id;
	adie_params[2] = ADIE_PATH_TX;
	adie_params[4] = 256;

	if (!adie_set_path(adie, adie_params, sizeof(adie_params)))
		pr_err("adie set tx path failed\n");

	adie_proceed_to_stage(adie, ADIE_PATH_TX,
					 ADIE_STAGE_DIGITAL_READY);
	adie_proceed_to_stage(adie, ADIE_PATH_TX,
					 ADIE_STAGE_DIGITAL_ANALOG_READY);

	audio_tx_analog_enable(1);
	audio_tx_mute(ac_control, adev, tx_mute_status);

	if (!tx_mute_status)
		audio_tx_volume(ac_control, adev, q6_device_volume(adev, 100));
}

static void _audio_rx_path_disable(void)
{
	audio_rx_analog_enable(0);

	adie_proceed_to_stage(adie, ADIE_PATH_RX, ADIE_STAGE_ANALOG_OFF);
	adie_proceed_to_stage(adie, ADIE_PATH_RX, ADIE_STAGE_DIGITAL_OFF);
}

static void _audio_tx_path_disable(void)
{
	audio_tx_analog_enable(0);

	adie_proceed_to_stage(adie, ADIE_PATH_TX, ADIE_STAGE_ANALOG_OFF);
	adie_proceed_to_stage(adie, ADIE_PATH_TX, ADIE_STAGE_DIGITAL_OFF);
}

static int icodec_rx_clk_refcount;
static int icodec_tx_clk_refcount;
static int ecodec_clk_refcount;
static int sdac_clk_refcount;

static void _audio_rx_clk_enable(void)
{
	uint32_t device_group = q6_device_to_codec(audio_rx_device_id);

	switch (device_group) {
	case Q6_ICODEC_RX:
		icodec_rx_clk_refcount++;
		if (icodec_rx_clk_refcount == 1) {
			clk_set_rate(icodec_rx_clk, 12288000);
			clk_enable(icodec_rx_clk);
		}
		break;
	case Q6_ECODEC_RX:
		ecodec_clk_refcount++;
		if (ecodec_clk_refcount == 1) {
			clk_set_rate(ecodec_clk, 2048000);
			clk_enable(ecodec_clk);
		}
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

	switch (device_group) {
	case Q6_ICODEC_TX:
		icodec_tx_clk_refcount++;
		if (icodec_tx_clk_refcount == 1) {
			clk_set_rate(icodec_tx_clk, tx_clk_freq * 256);
			clk_enable(icodec_tx_clk);
		}
		break;
	case Q6_ECODEC_TX:
		ecodec_clk_refcount++;
		if (ecodec_clk_refcount == 1) {
			clk_set_rate(ecodec_clk, 2048000);
			clk_enable(ecodec_clk);
		}
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
	switch (audio_rx_device_group) {
	case Q6_ICODEC_RX:
		icodec_rx_clk_refcount--;
		if (icodec_rx_clk_refcount == 0) {
			clk_disable(icodec_rx_clk);
			audio_rx_device_group = -1;
		}
		break;
	case Q6_ECODEC_RX:
		ecodec_clk_refcount--;
		if (ecodec_clk_refcount == 0) {
			clk_disable(ecodec_clk);
			audio_rx_device_group = -1;
		}
		break;
	case Q6_SDAC_RX:
		sdac_clk_refcount--;
		if (sdac_clk_refcount == 0) {
			clk_disable(sdac_clk);
			audio_rx_device_group = -1;
		}
		break;
	default:
		pr_err("audiolib: invalid rx device group %d\n",
			audio_rx_device_group);
		break;
	}
}

static void _audio_tx_clk_disable(void)
{
	switch (audio_tx_device_group) {
	case Q6_ICODEC_TX:
		icodec_tx_clk_refcount--;
		if (icodec_tx_clk_refcount == 0) {
			clk_disable(icodec_tx_clk);
			audio_tx_device_group = -1;
		}
		break;
	case Q6_ECODEC_TX:
		ecodec_clk_refcount--;
		if (ecodec_clk_refcount == 0) {
			clk_disable(ecodec_clk);
			audio_tx_device_group = -1;
		}
		break;
	case Q6_SDAC_TX:
		sdac_clk_refcount--;
		if (sdac_clk_refcount == 0) {
			clk_disable(sdac_clk);
			audio_tx_device_group = -1;
		}
		break;
	default:
		pr_err("audiolib: invalid tx device group %d\n",
			audio_tx_device_group);
		break;
	}
}

static void _audio_rx_clk_reinit(uint32_t rx_device)
{
	uint32_t device_group = q6_device_to_codec(rx_device);

	if (device_group != audio_rx_device_group)
		_audio_rx_clk_disable();

	audio_rx_device_id = rx_device;
	audio_rx_path_id = q6_device_to_path(rx_device);

	if (device_group != audio_rx_device_group)
		_audio_rx_clk_enable();

}

static void _audio_tx_clk_reinit(uint32_t tx_device)
{
	uint32_t device_group = q6_device_to_codec(tx_device);

	if (device_group != audio_tx_device_group)
		_audio_tx_clk_disable();

	audio_tx_device_id = tx_device;
	audio_tx_path_id = q6_device_to_path(tx_device);

	if (device_group != audio_tx_device_group)
		_audio_tx_clk_enable();
}

static DEFINE_MUTEX(audio_path_lock);
static int audio_rx_path_refcount;
static int audio_tx_path_refcount;

static int audio_rx_path_enable(int en)
{
	mutex_lock(&audio_path_lock);
	if (en) {
		audio_rx_path_refcount++;
		if (audio_rx_path_refcount == 1) {
			adie_enable();
			_audio_rx_clk_enable();
			_audio_rx_path_enable();
		}
	} else {
		audio_rx_path_refcount--;
		if (audio_rx_path_refcount == 0) {
			_audio_rx_path_disable();
			_audio_rx_clk_disable();
			adie_disable();
		}
	}
	mutex_unlock(&audio_path_lock);
	return 0;
}

static int audio_tx_path_enable(int en)
{
	mutex_lock(&audio_path_lock);
	if (en) {
		audio_tx_path_refcount++;
		if (audio_tx_path_refcount == 1) {
			adie_enable();
			_audio_tx_clk_enable();
			_audio_tx_path_enable();
		}
	} else {
		audio_tx_path_refcount--;
		if (audio_tx_path_refcount == 0) {
			_audio_tx_path_disable();
			_audio_tx_clk_disable();
			adie_disable();
		}
	}
	mutex_unlock(&audio_path_lock);
	return 0;
}

int q6audio_update_acdb(uint32_t id_src, uint32_t id_dst)
{
	mutex_lock(&audio_path_lock);
	mutex_unlock(&audio_path_lock);
	return 0;
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
	if (!rc)
		tx_mute_status = mute;
	mutex_unlock(&audio_path_lock);
	return 0;
}

int q6audio_set_rx_volume(int level)
{
	uint32_t adev;
	int vol;

	if (q6audio_init())
		return 0;

	if (level < 0 || level > 100)
		return -EINVAL;

	mutex_lock(&audio_path_lock);
	adev = audio_rx_device_id;
	vol = q6_device_volume(adev, level);
	audio_rx_mute(ac_control, adev, 0);
	audio_rx_volume(ac_control, adev, vol);
	mutex_unlock(&audio_path_lock);
	return 0;
}

static void do_rx_routing(uint32_t device_id)
{
	int sz;
	uint32_t sample_rate;

	if (device_id == audio_rx_device_id)
		return;

	if (audio_rx_path_refcount > 0) {
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_RX_DEVICE,
					 device_id);
		_audio_rx_path_disable();
		_audio_rx_clk_reinit(device_id);
		_audio_rx_path_enable();
	} else {
		sample_rate = q6_device_to_rate(device_id);
		sz = acdb_get_config_table(device_id, sample_rate);
		if (sz < 0)
			pr_err("could not get ACDB config table\n");

		audio_set_table(ac_control, device_id, sz);
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_RX_DEVICE,
					 device_id);
		qdsp6_standby(ac_control);
		qdsp6_start(ac_control);
		audio_rx_device_id = device_id;
		audio_rx_path_id = q6_device_to_path(device_id);
	}
}

static void do_tx_routing(uint32_t device_id)
{
	int sz;
	uint32_t sample_rate;

	if (device_id == audio_tx_device_id)
		return;

	if (audio_tx_path_refcount > 0) {
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_TX_DEVICE,
					 device_id);
		_audio_tx_path_disable();
		_audio_tx_clk_reinit(device_id);
		_audio_tx_path_enable();
	} else {
		sample_rate = q6_device_to_rate(device_id);
		sz = acdb_get_config_table(device_id, sample_rate);
		audio_set_table(ac_control, device_id, sz);
		qdsp6_devchg_notify(ac_control, ADSP_AUDIO_TX_DEVICE,
					 device_id);
		qdsp6_standby(ac_control);
		qdsp6_start(ac_control);
		audio_tx_device_id = device_id;
		audio_tx_path_id = q6_device_to_path(device_id);
	}
}

int q6audio_do_routing(uint32_t device_id)
{
	if (q6audio_init())
		return 0;

	mutex_lock(&audio_path_lock);

	switch (q6_device_to_dir(device_id)) {
	case Q6_RX:
		do_rx_routing(device_id);
		break;
	case Q6_TX:
		do_tx_routing(device_id);
		break;
	}

	mutex_unlock(&audio_path_lock);
	return 0;
}

int q6audio_set_route(const char *name)
{
	uint32_t route;
	if (!strcmp(name, "speaker"))
		route = ADIE_PATH_SPEAKER_STEREO_RX;
	else if (!strcmp(name, "headphones"))
		route = ADIE_PATH_HEADSET_STEREO_RX;
	else if (!strcmp(name, "handset"))
		route = ADIE_PATH_HANDSET_RX;
	else
		return -EINVAL;

	mutex_lock(&audio_path_lock);
	if (route == audio_rx_path_id)
		goto done;

	audio_rx_path_id = route;

	if (audio_rx_path_refcount > 0) {
		_audio_rx_path_disable();
		_audio_rx_path_enable();
	}
	if (audio_tx_path_refcount > 0) {
		_audio_tx_path_disable();
		_audio_tx_path_enable();
	}
done:
	mutex_unlock(&audio_path_lock);
	return 0;
}

struct audio_client *q6audio_open(uint32_t flags, uint32_t bufsz)
{
	struct audio_client *ac;

	if (q6audio_init())
		return 0;

	ac = audio_client_alloc(bufsz);
	if (!ac)
		return 0;

	ac->flags = flags;
	if (ac->flags & AUDIO_FLAG_WRITE)
		audio_rx_path_enable(1);
	else
		audio_tx_path_enable(1);

	return ac;
}

int q6audio_start(struct audio_client *ac, void *rpc,
						uint32_t len)
{

	audio_ioctl(ac, rpc, len);

	audio_command(ac, ADSP_AUDIO_IOCTL_CMD_SESSION_START);

	if (!(ac->flags & AUDIO_FLAG_WRITE)) {
		ac->buf[0].used = 1;
		ac->buf[1].used = 1;
		q6audio_read(ac, &ac->buf[0]);
		q6audio_read(ac, &ac->buf[1]);
	}

	audio_prevent_sleep();
	return 0;
}

int q6audio_close(struct audio_client *ac)
{
	audio_close(ac);

	if (ac->flags & AUDIO_FLAG_WRITE)
		audio_rx_path_enable(0);
	else
		audio_tx_path_enable(0);

	audio_client_free(ac);
	audio_allow_sleep();
	return 0;
}

struct audio_client *q6voice_open(void)
{
	struct audio_client *ac;

	if (q6audio_init())
		return 0;

	ac = audio_client_alloc(0);
	if (!ac)
		return 0;

	return ac;
}

int q6voice_setup(void)
{
	audio_rx_path_enable(1);
	tx_clk_freq = 8000;
	audio_tx_path_enable(1);

	return 0;
}

int q6voice_teardown(void)
{
	audio_rx_path_enable(0);
	audio_tx_path_enable(0);
	return 0;
}


int q6voice_close(struct audio_client *ac)
{
	audio_client_free(ac);
	return 0;
}

int q6audio_async(struct audio_client *ac)
{
	struct adsp_command_hdr rpc;
	memset(&rpc, 0, sizeof(rpc));
	rpc.opcode = ADSP_AUDIO_IOCTL_CMD_STREAM_EOS;
	rpc.response_type = ADSP_AUDIO_RESPONSE_ASYNC;
	return audio_ioctl(ac, &rpc, sizeof(rpc));
}
