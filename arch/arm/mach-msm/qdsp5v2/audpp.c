/* arch/arm/mach-msm/qdsp5/audpp.c
 *
 * common code to deal with the AUDPP dsp task (audio postproc)
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>


#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <mach/board.h>
#include <mach/msm_adsp.h>
#include <mach/qdsp5v2/audio_acdbi.h>
#include <mach/qdsp5v2/qdsp5audppcmdi.h>
#include <mach/qdsp5v2/qdsp5audppmsg.h>
#include <mach/qdsp5v2/audpp.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>

#include "../qdsp5/evlog.h"
#include <mach/debug_mm.h>

enum {
	EV_NULL,
	EV_ENABLE,
	EV_DISABLE,
	EV_EVENT,
	EV_DATA,
};

static const char *dsp_log_strings[] = {
	"NULL",
	"ENABLE",
	"DISABLE",
	"EVENT",
	"DATA",
};

DECLARE_LOG(dsp_log, 64, dsp_log_strings);

static int __init _dsp_log_init(void)
{
	return ev_log_init(&dsp_log);
}

module_init(_dsp_log_init);
#define LOG(id, arg) ev_log_write(&dsp_log, id, arg)

static DEFINE_MUTEX(audpp_lock);
static DEFINE_MUTEX(audpp_dec_lock);
static struct wake_lock audpp_wake_lock;

#define CH_COUNT 5
#define AUDPP_CLNT_MAX_COUNT 6

#define AUDPP_CMD_CFG_OBJ_UPDATE 0x8000
#define AUDPP_CMD_EQ_FLAG_DIS	0x0000
#define AUDPP_CMD_EQ_FLAG_ENA	-1
#define AUDPP_CMD_IIR_FLAG_DIS	  0x0000
#define AUDPP_CMD_IIR_FLAG_ENA	  -1
#define AUDPP_CMD_STF_FLAG_ENA -1
#define AUDPP_CMD_STF_FLAG_DIS 0x0000

#define MAX_EVENT_CALLBACK_CLIENTS 	1

#define AUDPP_CONCURRENCY_DEFAULT 0	/* Set default to LPA mode */
#define AUDPP_MAX_DECODER_CNT 5
#define AUDPP_CODEC_MASK 0x000000FF
#define AUDPP_MODE_MASK 0x00000F00
#define AUDPP_OP_MASK 0xF0000000

struct audpp_decoder_info {
	unsigned int codec;
	pid_t pid;
};

struct audpp_state {
	struct msm_adsp_module *mod;
	audpp_event_func func[AUDPP_CLNT_MAX_COUNT];
	void *private[AUDPP_CLNT_MAX_COUNT];
	struct mutex *lock;
	unsigned open_count;
	unsigned enabled;

	/* Related to decoder allocation */
	struct mutex *lock_dec;
	struct msm_adspdec_database *dec_database;
	struct audpp_decoder_info dec_info_table[AUDPP_MAX_DECODER_CNT];
	unsigned dec_inuse;
	unsigned long concurrency;

	struct audpp_event_callback *cb_tbl[MAX_EVENT_CALLBACK_CLIENTS];

	/* Related to decoder instances */
	uint8_t op_mode; /* Specifies Turbo/Non Turbo mode */
	uint8_t decoder_count; /* No. of decoders active running */
	uint8_t codec_max_instances; /* Max codecs allowed currently */
	uint8_t codec_cnt[MSM_MAX_DEC_CNT]; /* Nr of each codec
						 type enabled */

	wait_queue_head_t event_wait;
};

struct audpp_state the_audpp_state = {
	.lock = &audpp_lock,
	.lock_dec = &audpp_dec_lock,
};

static inline void prevent_suspend(void)
{
	wake_lock(&audpp_wake_lock);
}
static inline void allow_suspend(void)
{
	wake_unlock(&audpp_wake_lock);
}

int audpp_send_queue1(void *cmd, unsigned len)
{
	return msm_adsp_write(the_audpp_state.mod,
			      QDSP_uPAudPPCmd1Queue, cmd, len);
}
EXPORT_SYMBOL(audpp_send_queue1);

int audpp_send_queue2(void *cmd, unsigned len)
{
	return msm_adsp_write(the_audpp_state.mod,
			      QDSP_uPAudPPCmd2Queue, cmd, len);
}
EXPORT_SYMBOL(audpp_send_queue2);

int audpp_send_queue3(void *cmd, unsigned len)
{
	return msm_adsp_write(the_audpp_state.mod,
			      QDSP_uPAudPPCmd3Queue, cmd, len);
}
EXPORT_SYMBOL(audpp_send_queue3);

static int audpp_dsp_config(int enable)
{
	struct audpp_cmd_cfg cmd;

	cmd.cmd_id = AUDPP_CMD_CFG;
	cmd.cfg = enable ? AUDPP_CMD_CFG_ENABLE : AUDPP_CMD_CFG_SLEEP;

	return audpp_send_queue1(&cmd, sizeof(cmd));
}

void audpp_route_stream(unsigned short dec_id, unsigned short mixer_mask)
{
	struct audpp_cmd_cfg_dev_mixer_params mixer_params_cmd;

	memset(&mixer_params_cmd, 0, sizeof(mixer_params_cmd));

	mixer_params_cmd.cmd_id = AUDPP_CMD_CFG_DEV_MIXER;
	mixer_params_cmd.stream_id = dec_id;
	mixer_params_cmd.mixer_cmd = mixer_mask;
	audpp_send_queue1(&mixer_params_cmd, sizeof(mixer_params_cmd));

}
EXPORT_SYMBOL(audpp_route_stream);

int is_audpp_enable(void)
{
	struct audpp_state *audpp = &the_audpp_state;

	return audpp->enabled;
}
EXPORT_SYMBOL(is_audpp_enable);

int audpp_register_event_callback(struct audpp_event_callback *ecb)
{
	struct audpp_state *audpp = &the_audpp_state;
	int i;

	for (i = 0; i < MAX_EVENT_CALLBACK_CLIENTS; ++i) {
		if (NULL == audpp->cb_tbl[i]) {
			audpp->cb_tbl[i] = ecb;
			return 0;
		}
	}
	return -1;
}
EXPORT_SYMBOL(audpp_register_event_callback);


int audpp_unregister_event_callback(struct audpp_event_callback *ecb)
{
	struct audpp_state *audpp = &the_audpp_state;
	int i;

	for (i = 0; i < MAX_EVENT_CALLBACK_CLIENTS; ++i) {
		if (ecb == audpp->cb_tbl[i]) {
			audpp->cb_tbl[i] = NULL;
			return 0;
		}
	}
	return -1;
}
EXPORT_SYMBOL(audpp_unregister_event_callback);

static void audpp_broadcast(struct audpp_state *audpp, unsigned id,
			    uint16_t *msg)
{
	unsigned n;
	for (n = 0; n < AUDPP_CLNT_MAX_COUNT; n++) {
		if (audpp->func[n])
			audpp->func[n] (audpp->private[n], id, msg);
	}

	for (n = 0; n < MAX_EVENT_CALLBACK_CLIENTS; ++n)
		if (audpp->cb_tbl[n] && audpp->cb_tbl[n]->fn)
			audpp->cb_tbl[n]->fn(audpp->cb_tbl[n]->private, id,
					     msg);
}

static void audpp_notify_clnt(struct audpp_state *audpp, unsigned clnt_id,
			      unsigned id, uint16_t *msg)
{
	if (clnt_id < AUDPP_CLNT_MAX_COUNT && audpp->func[clnt_id])
		audpp->func[clnt_id] (audpp->private[clnt_id], id, msg);
}

static void audpp_handle_pcmdmamiss(struct audpp_state *audpp,
				    uint16_t bit_mask)
{
	uint8_t b_index;

	for (b_index = 0; b_index < AUDPP_CLNT_MAX_COUNT; b_index++) {
		if (bit_mask & (0x1 << b_index))
			if (audpp->func[b_index])
				audpp->func[b_index] (audpp->private[b_index],
						      AUDPP_MSG_PCMDMAMISSED,
						      &bit_mask);
	}
}

static void audpp_dsp_event(void *data, unsigned id, size_t len,
			    void (*getevent) (void *ptr, size_t len))
{
	struct audpp_state *audpp = data;
	uint16_t msg[8];

	getevent(msg, sizeof(msg));

	LOG(EV_EVENT, (id << 16) | msg[0]);
	LOG(EV_DATA, (msg[1] << 16) | msg[2]);

	switch (id) {
	case AUDPP_MSG_STATUS_MSG:{
			unsigned cid = msg[0];
			MM_DBG("status %d %d %d\n", cid, msg[1], msg[2]);

			if ((cid < 5) && audpp->func[cid])
				audpp->func[cid] (audpp->private[cid], id, msg);
			break;
		}
	case AUDPP_MSG_HOST_PCM_INTF_MSG:
		if (audpp->func[5])
			audpp->func[5] (audpp->private[5], id, msg);
		break;
	case AUDPP_MSG_PCMDMAMISSED:
		audpp_handle_pcmdmamiss(audpp, msg[0]);
		break;
	case AUDPP_MSG_CFG_MSG:
		if (msg[0] == AUDPP_MSG_ENA_ENA) {
			MM_INFO("ENABLE\n");
			audpp->enabled = 1;
			audpp_broadcast(audpp, id, msg);
		} else if (msg[0] == AUDPP_MSG_ENA_DIS) {
			MM_INFO("DISABLE\n");
			audpp->enabled = 0;
			wake_up(&audpp->event_wait);
			audpp_broadcast(audpp, id, msg);
		} else {
			MM_ERR("invalid config msg %d\n", msg[0]);
		}
		break;
	case AUDPP_MSG_ROUTING_ACK:
		audpp_notify_clnt(audpp, msg[0], id, msg);
		break;
	case AUDPP_MSG_FLUSH_ACK:
		audpp_notify_clnt(audpp, msg[0], id, msg);
		break;
	case ADSP_MESSAGE_ID:
		MM_DBG("Received ADSP event: module enable/disable \
				(audpptask)");
		break;
	case AUDPP_MSG_AVSYNC_MSG:
		audpp_notify_clnt(audpp, msg[0], id, msg);
		break;
#ifdef CONFIG_DEBUG_FS
	case AUDPP_MSG_FEAT_QUERY_DM_DONE:
		MM_INFO(" RTC ACK --> %x %x %x %x %x %x %x %x\n", msg[0],\
			msg[1], msg[2], msg[3], msg[4], \
			msg[5], msg[6], msg[7]);
		acdb_rtc_set_err(msg[3]);
		break;
#endif
	default:
		MM_INFO("unhandled msg id %x\n", id);
	}
}

static struct msm_adsp_ops adsp_ops = {
	.event = audpp_dsp_event,
};

static void audpp_fake_event(struct audpp_state *audpp, int id,
			     unsigned event, unsigned arg)
{
	uint16_t msg[1];
	msg[0] = arg;
	audpp->func[id] (audpp->private[id], event, msg);
}

int audpp_enable(int id, audpp_event_func func, void *private)
{
	struct audpp_state *audpp = &the_audpp_state;
	int res = 0;

	if (id < -1 || id > 4)
		return -EINVAL;

	if (id == -1)
		id = 5;

	mutex_lock(audpp->lock);
	if (audpp->func[id]) {
		res = -EBUSY;
		goto out;
	}

	audpp->func[id] = func;
	audpp->private[id] = private;

	LOG(EV_ENABLE, 1);
	if (audpp->open_count++ == 0) {
		MM_DBG("enable\n");
		res = msm_adsp_get("AUDPPTASK", &audpp->mod, &adsp_ops, audpp);
		if (res < 0) {
			MM_ERR("audpp: cannot open AUDPPTASK\n");
			audpp->open_count = 0;
			audpp->func[id] = NULL;
			audpp->private[id] = NULL;
			goto out;
		}
		LOG(EV_ENABLE, 2);
		prevent_suspend();
		msm_adsp_enable(audpp->mod);
		audpp_dsp_config(1);
	} else {
		unsigned long flags;
		local_irq_save(flags);
		if (audpp->enabled)
			audpp_fake_event(audpp, id,
					 AUDPP_MSG_CFG_MSG, AUDPP_MSG_ENA_ENA);
		local_irq_restore(flags);
	}

	res = 0;
out:
	mutex_unlock(audpp->lock);
	return res;
}
EXPORT_SYMBOL(audpp_enable);

void audpp_disable(int id, void *private)
{
	struct audpp_state *audpp = &the_audpp_state;
	unsigned long flags;
	int rc;

	if (id < -1 || id > 4)
		return;

	if (id == -1)
		id = 5;

	mutex_lock(audpp->lock);
	LOG(EV_DISABLE, 1);
	if (!audpp->func[id])
		goto out;
	if (audpp->private[id] != private)
		goto out;

	local_irq_save(flags);
	audpp_fake_event(audpp, id, AUDPP_MSG_CFG_MSG, AUDPP_MSG_ENA_DIS);
	audpp->func[id] = NULL;
	audpp->private[id] = NULL;
	local_irq_restore(flags);

	if (--audpp->open_count == 0) {
		MM_DBG("disable\n");
		LOG(EV_DISABLE, 2);
		audpp_dsp_config(0);
		rc = wait_event_interruptible(audpp->event_wait,
				(audpp->enabled == 0));
		if (audpp->enabled == 0)
			MM_INFO("Received CFG_MSG_DISABLE from ADSP\n");
		else
			MM_ERR("Didn't receive CFG_MSG DISABLE \
					message from ADSP\n");
		msm_adsp_disable(audpp->mod);
		msm_adsp_put(audpp->mod);
		audpp->mod = NULL;
		allow_suspend();
	}
out:
	mutex_unlock(audpp->lock);
}
EXPORT_SYMBOL(audpp_disable);

#define BAD_ID(id) ((id < 0) || (id >= CH_COUNT))

int audpp_restore_avsync(int id, uint16_t *avsync)
{
	struct audpp_cmd_avsync cmd;

	if (BAD_ID(id))
		return -1;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDPP_CMD_AVSYNC;
	cmd.stream_id = id;
	cmd.interrupt_interval = 0; /* Setting it to Zero as there won't be
					periodic update */
	cmd.sample_counter_dlsw = avsync[3];
	cmd.sample_counter_dmsw = avsync[2];
	cmd.sample_counter_msw = avsync[1];
	cmd.byte_counter_dlsw = avsync[6];
	cmd.byte_counter_dmsw = avsync[5];
	cmd.byte_counter_msw = avsync[4];

	return audpp_send_queue1(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_restore_avsync);

int audpp_query_avsync(int id)
{
	struct audpp_cmd_query_avsync cmd;

	if (BAD_ID(id))
		return -EINVAL;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDPP_CMD_QUERY_AVSYNC;
	cmd.stream_id = id;
	return audpp_send_queue1(&cmd, sizeof(cmd));

}
EXPORT_SYMBOL(audpp_query_avsync);

int audpp_set_volume_and_pan(unsigned id, unsigned volume, int pan,
			enum obj_type objtype)
{
	/* cmd, obj_cfg[7], cmd_type, volume, pan */
	uint16_t cmd[7];

	if (objtype) {
		if (id > 5) {
			MM_ERR("Wrong POPP decoder id: %d\n", id);
			return -EINVAL;
		}
	} else {
		if (id > 3) {
			MM_ERR("Wrong COPP decoder id: %d\n", id);
			return -EINVAL;
		}
	}

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = AUDPP_CMD_CFG_OBJECT_PARAMS;
	if (objtype)
		cmd[1] = AUDPP_CMD_POPP_STREAM;
	else
		cmd[1] = AUDPP_CMD_COPP_STREAM;
	cmd[2] = id;
	cmd[3] = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd[4] = AUDPP_CMD_VOLUME_PAN;
	cmd[5] = volume;
	cmd[6] = pan;

	return audpp_send_queue3(cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_set_volume_and_pan);

/* Implementation of COPP features */
int audpp_dsp_set_mbadrc(unsigned id, unsigned enable,
	struct audpp_cmd_cfg_object_params_mbadrc *mbadrc,
	enum obj_type objtype)
{
	if (objtype) {
		if (id > 5) {
			MM_ERR("Wrong POPP decoder id: %d\n", id);
			return -EINVAL;
		}
	} else {
		if (id > 3) {
			MM_ERR("Wrong COPP decoder id: %d\n", id);
			return -EINVAL;
		}
	}

	mbadrc->common.cmd_id = AUDPP_CMD_CFG_OBJECT_PARAMS;
	if (objtype)
		mbadrc->common.stream = AUDPP_CMD_POPP_STREAM;
	else
		mbadrc->common.stream = AUDPP_CMD_COPP_STREAM;

	mbadrc->common.stream_id = id;
	mbadrc->common.obj_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	mbadrc->common.command_type = AUDPP_CMD_MBADRC;

	if (enable)
		mbadrc->enable = AUDPP_CMD_ADRC_FLAG_ENA;
	else
		mbadrc->enable = AUDPP_CMD_ADRC_FLAG_DIS;

	return audpp_send_queue3(mbadrc,
			sizeof(struct audpp_cmd_cfg_object_params_mbadrc));
}
EXPORT_SYMBOL(audpp_dsp_set_mbadrc);

int audpp_dsp_set_qconcert_plus(unsigned id, unsigned enable,
	struct audpp_cmd_cfg_object_params_qconcert *qconcert_plus,
	enum obj_type objtype)
{
	if (objtype) {
		if (id > 5) {
			MM_ERR("Wrong POPP decoder id: %d\n", id);
			return -EINVAL;
		}
	} else {
		if (id > 3) {
			MM_ERR("Wrong COPP decoder id: %d\n", id);
			return -EINVAL;
		}
	}

	qconcert_plus->common.cmd_id = AUDPP_CMD_CFG_OBJECT_PARAMS;
	if (objtype)
		qconcert_plus->common.stream = AUDPP_CMD_POPP_STREAM;
	else
		qconcert_plus->common.stream = AUDPP_CMD_COPP_STREAM;

	qconcert_plus->common.stream_id = id;
	qconcert_plus->common.obj_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	qconcert_plus->common.command_type = AUDPP_CMD_QCONCERT;

	if (enable)
		qconcert_plus->enable_flag = AUDPP_CMD_ADRC_FLAG_ENA;
	else
		qconcert_plus->enable_flag = AUDPP_CMD_ADRC_FLAG_DIS;

	return audpp_send_queue3(qconcert_plus,
		sizeof(struct audpp_cmd_cfg_object_params_qconcert));
}
EXPORT_SYMBOL(audpp_dsp_set_qconcert_plus);

int audpp_dsp_set_rx_iir(unsigned id, unsigned enable,
	struct audpp_cmd_cfg_object_params_pcm *iir,
	enum obj_type objtype)
{

	if (objtype) {
		if (id > 5) {
			MM_ERR("Wrong POPP decoder id: %d\n", id);
			return -EINVAL;
		}
	} else {
		if (id > 3) {
			MM_ERR("Wrong COPP decoder id: %d\n", id);
			return -EINVAL;
		}
	}

	iir->common.cmd_id = AUDPP_CMD_CFG_OBJECT_PARAMS;
	if (objtype)
		iir->common.stream = AUDPP_CMD_POPP_STREAM;
	else
		iir->common.stream = AUDPP_CMD_COPP_STREAM;

	iir->common.stream_id = id;
	iir->common.obj_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	iir->common.command_type = AUDPP_CMD_IIR_TUNING_FILTER;

	if (enable)
		iir->active_flag = AUDPP_CMD_IIR_FLAG_ENA;
	else
		iir->active_flag = AUDPP_CMD_IIR_FLAG_DIS;

	return audpp_send_queue3(iir,
		sizeof(struct audpp_cmd_cfg_object_params_pcm));
}
EXPORT_SYMBOL(audpp_dsp_set_rx_iir);

int audpp_dsp_set_gain_rx(unsigned id,
			struct audpp_cmd_cfg_cal_gain *calib_gain_rx,
			enum obj_type objtype)
{
	if (objtype) {
		return -EINVAL;
	} else {
		if (id > 3) {
			MM_ERR("Wrong COPP decoder id: %d\n", id);
			return -EINVAL;
		}
	}
	calib_gain_rx->common.cmd_id = AUDPP_CMD_CFG_OBJECT_PARAMS;
	calib_gain_rx->common.stream = AUDPP_CMD_COPP_STREAM;

	calib_gain_rx->common.stream_id = id;
	calib_gain_rx->common.obj_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	calib_gain_rx->common.command_type = AUDPP_CMD_CALIB_GAIN_RX;

	return audpp_send_queue3(calib_gain_rx,
			sizeof(struct audpp_cmd_cfg_cal_gain));
}
EXPORT_SYMBOL(audpp_dsp_set_gain_rx);

int audpp_dsp_set_pbe(unsigned id, unsigned enable,
			struct audpp_cmd_cfg_pbe *pbe_block,
			enum obj_type objtype)
{
	if (objtype) {
		if (id > 5) {
			MM_ERR("Wrong POPP decoder id: %d\n", id);
			return -EINVAL;
		}
	} else {
		if (id > 3) {
			MM_ERR("Wrong COPP decoder id: %d\n", id);
			return -EINVAL;
		}
	}

	pbe_block->common.cmd_id = AUDPP_CMD_CFG_OBJECT_PARAMS;
	if (objtype)
		pbe_block->common.stream = AUDPP_CMD_POPP_STREAM;
	else
		pbe_block->common.stream = AUDPP_CMD_COPP_STREAM;

	pbe_block->common.stream_id = id;
	pbe_block->common.obj_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	pbe_block->common.command_type = AUDPP_CMD_PBE;

	if (enable)
		pbe_block->pbe_enable = AUDPP_CMD_PBE_FLAG_ENA;
	else
		pbe_block->pbe_enable = AUDPP_CMD_PBE_FLAG_DIS;

	return audpp_send_queue3(pbe_block,
			sizeof(struct audpp_cmd_cfg_pbe));
}
EXPORT_SYMBOL(audpp_dsp_set_pbe);

int audpp_dsp_set_spa(unsigned id,
     struct audpp_cmd_cfg_object_params_spectram *spa,
			enum obj_type objtype){
	struct audpp_cmd_cfg_object_params_spectram cmd;

	if (objtype) {
		if (id > 5) {
			MM_ERR("Wrong POPP decoder id: %d\n", id);
			return -EINVAL;
		}
	} else {
		if (id > 3) {
			MM_ERR("Wrong COPP decoder id: %d\n", id);
			return -EINVAL;
		}
	}

	memset(&cmd, 0, sizeof(cmd));
	if (objtype)
		cmd.common.stream = AUDPP_CMD_POPP_STREAM;
	else
		cmd.common.stream = AUDPP_CMD_COPP_STREAM;

	cmd.common.stream_id = id;
	cmd.common.obj_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_CMD_SPECTROGRAM;
       cmd.sample_interval = spa->sample_interval;
	cmd.num_coeff = spa->num_coeff;
	return audpp_send_queue3(&cmd, sizeof(cmd));

}
EXPORT_SYMBOL(audpp_dsp_set_spa);

int audpp_dsp_set_stf(unsigned id, unsigned enable,
     struct audpp_cmd_cfg_object_params_sidechain *stf,
			enum obj_type objtype){
	if (objtype) {
		if (id > 5) {
			MM_ERR("Wrong POPP decoder id: %d\n", id);
			return -EINVAL;
		}
	} else {
		if (id > 3) {
			MM_ERR("Wrong COPP decoder id: %d\n", id);
			return -EINVAL;
		}
	}

	stf->common.cmd_id = AUDPP_CMD_CFG_OBJECT_PARAMS;
	if (objtype)
		stf->common.stream = AUDPP_CMD_POPP_STREAM;
	else
		stf->common.stream = AUDPP_CMD_COPP_STREAM;

	stf->common.stream_id = id;
	stf->common.obj_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	stf->common.command_type = AUDPP_CMD_SIDECHAIN_TUNING_FILTER;

	if (enable)
		stf->active_flag = AUDPP_CMD_STF_FLAG_ENA;
	else
		stf->active_flag = AUDPP_CMD_STF_FLAG_DIS;
	return audpp_send_queue3(stf,
		sizeof(struct audpp_cmd_cfg_object_params_sidechain));
}
EXPORT_SYMBOL(audpp_dsp_set_stf);

/* Implementation Of COPP + POPP */
int audpp_dsp_set_eq(unsigned id, unsigned enable,
		     struct audpp_cmd_cfg_object_params_eqalizer *eq,
				enum obj_type objtype)
{
	struct audpp_cmd_cfg_object_params_eqalizer cmd;

	if (objtype) {
		if (id > 5) {
			MM_ERR("Wrong POPP decoder id: %d\n", id);
			return -EINVAL;
		}
	} else {
		if (id > 3) {
			MM_ERR("Wrong COPP decoder id: %d\n", id);
			return -EINVAL;
		}
	}

	memset(&cmd, 0, sizeof(cmd));
	if (objtype)
		cmd.common.stream = AUDPP_CMD_POPP_STREAM;
	else
		cmd.common.stream = AUDPP_CMD_COPP_STREAM;

	cmd.common.stream_id = id;
	cmd.common.obj_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_CMD_EQUALIZER;
	if (enable) {
		cmd.eq_flag = AUDPP_CMD_EQ_FLAG_ENA;
		cmd.num_bands = eq->num_bands;
		memcpy(&cmd.eq_coeff, &eq->eq_coeff, sizeof(eq->eq_coeff));
	} else
		cmd.eq_flag = AUDPP_CMD_EQ_FLAG_DIS;

	return audpp_send_queue3(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_dsp_set_eq);

int audpp_dsp_set_vol_pan(unsigned id,
			  struct audpp_cmd_cfg_object_params_volume *vol_pan,
					enum obj_type objtype)
{
	struct audpp_cmd_cfg_object_params_volume cmd;

	if (objtype) {
		if (id > 5) {
			MM_ERR("Wrong POPP decoder id: %d\n", id);
			return -EINVAL;
		}
	} else {
		if (id > AUDPP_MAX_COPP_DEVICES) {
			MM_ERR("Wrong COPP decoder id: %d\n", id);
			return -EINVAL;
		}
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPP_CMD_CFG_OBJECT_PARAMS;
	if (objtype)
		cmd.common.stream = AUDPP_CMD_POPP_STREAM;
	else
		cmd.common.stream = AUDPP_CMD_COPP_STREAM;

	cmd.common.stream_id = id;
	cmd.common.obj_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_CMD_VOLUME_PAN;

	cmd.volume = vol_pan->volume;
	cmd.pan = vol_pan->pan;

	return audpp_send_queue3(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_dsp_set_vol_pan);

int audpp_pause(unsigned id, int pause)
{
	/* pause 1 = pause 0 = resume */
	u16 pause_cmd[AUDPP_CMD_DEC_CTRL_LEN / sizeof(unsigned short)];

	if (id >= CH_COUNT)
		return -EINVAL;

	memset(pause_cmd, 0, sizeof(pause_cmd));

	pause_cmd[0] = AUDPP_CMD_DEC_CTRL;
	pause_cmd[1] = id;
	if (pause == 1)
		pause_cmd[2] = AUDPP_CMD_UPDATE_V | AUDPP_CMD_PAUSE_V;
	else if (pause == 0)
		pause_cmd[2] = AUDPP_CMD_UPDATE_V | AUDPP_CMD_RESUME_V;
	else
		return -EINVAL;

	return audpp_send_queue1(pause_cmd, sizeof(pause_cmd));
}
EXPORT_SYMBOL(audpp_pause);

int audpp_flush(unsigned id)
{
	u16 flush_cmd[AUDPP_CMD_DEC_CTRL_LEN / sizeof(unsigned short)];

	if (id >= CH_COUNT)
		return -EINVAL;

	memset(flush_cmd, 0, sizeof(flush_cmd));

	flush_cmd[0] = AUDPP_CMD_DEC_CTRL;
	flush_cmd[1] = id;
	flush_cmd[2] = AUDPP_CMD_UPDATE_V | AUDPP_CMD_FLUSH_V;

	return audpp_send_queue1(flush_cmd, sizeof(flush_cmd));
}
EXPORT_SYMBOL(audpp_flush);

/* dec_attrb = 7:0, 0 - No Decoder, else supported decoder *
 * like mp3, aac, wma etc ... *
 *           =  15:8, bit[8] = 1 - Tunnel, bit[9] = 1 - NonTunnel *
 *           =  31:16, reserved */
int audpp_adec_alloc(unsigned dec_attrb, const char **module_name,
		     unsigned *queueid)
{
	struct audpp_state *audpp = &the_audpp_state;
	int decid = -1, idx, lidx, mode, codec;
	int codecs_supported, min_codecs_supported;
	unsigned int *concurrency_entry;
	u8 max_instance, codec_type;

	struct dec_instance_table *dec_instance_list;
	dec_instance_list = (struct dec_instance_table *)
				(audpp->dec_database->dec_instance_list);

	mutex_lock(audpp->lock_dec);
	/* Represents in bit mask */
	mode = ((dec_attrb & AUDPP_MODE_MASK) << 16);
	codec = (1 << (dec_attrb & AUDPP_CODEC_MASK));
	codec_type = (dec_attrb & AUDPP_CODEC_MASK);

	/* Find  whether same/different codec instances are running */
	audpp->decoder_count++;
	audpp->codec_cnt[codec_type]++;
	max_instance = 0;

	/*if different instance of codec*/
	if (audpp->codec_cnt[codec_type] < audpp->decoder_count) {
		max_instance = audpp->codec_max_instances;
		/* Get the maximum no. of instances that can be supported */
		for (idx = 0; idx < MSM_MAX_DEC_CNT; idx++) {
			if (audpp->codec_cnt[idx]) {
				if ((dec_instance_list +
					audpp->op_mode * MSM_MAX_DEC_CNT +
						idx)->
						max_instances_diff_dec <
						max_instance) {
						max_instance =
						(dec_instance_list +
							 audpp->op_mode *
								MSM_MAX_DEC_CNT
								+ idx)->
							max_instances_diff_dec;
				}
			}
		}
		/* if different codec type, should not cross maximum other
		   supported */
		if (audpp->decoder_count > (max_instance + 1)) {
			MM_ERR("Can not support, already reached max\n");
			audpp->decoder_count--;
			audpp->codec_cnt[codec_type]--;
			goto done;
		}
		audpp->codec_max_instances = max_instance;
		MM_DBG("different codec running\n");
	} else {
		max_instance = (dec_instance_list + audpp->op_mode *
						MSM_MAX_DEC_CNT +
						 codec_type)->
							max_instances_same_dec;
		/* if same codec type, should not cross maximum supported */
		if (audpp->decoder_count > max_instance) {
			MM_ERR("Can not support, already reached max\n");
			audpp->decoder_count--;
			audpp->codec_cnt[codec_type]--;
			goto done;
		}
		audpp->codec_max_instances = max_instance;
		MM_DBG("same codec running\n");
	}

	/* Point to Last entry of the row */
	concurrency_entry = ((audpp->dec_database->dec_concurrency_table +
			      ((audpp->concurrency + 1) *
			       (audpp->dec_database->num_dec))) - 1);

	lidx = audpp->dec_database->num_dec;
	min_codecs_supported = sizeof(unsigned int) * 8;

	MM_DBG("mode = 0x%08x codec = 0x%08x\n", mode, codec);

	for (idx = lidx; idx > 0; idx--, concurrency_entry--) {
		if (!(audpp->dec_inuse & (1 << (idx - 1)))) {
			if (((mode & *concurrency_entry) == mode) &&
			    (codec & *concurrency_entry)) {
				/* Check supports minimum number codecs */
				codecs_supported =
				    audpp->dec_database->dec_info_list[idx -
								       1].
				    nr_codec_support;
				if (codecs_supported < min_codecs_supported) {
					lidx = idx - 1;
					min_codecs_supported = codecs_supported;
				}
			}
		}
	}

	if (lidx < audpp->dec_database->num_dec) {
		audpp->dec_inuse |= (1 << lidx);
		*module_name =
		    audpp->dec_database->dec_info_list[lidx].module_name;
		*queueid =
		    audpp->dec_database->dec_info_list[lidx].module_queueid;
		decid = audpp->dec_database->dec_info_list[lidx].module_decid;
		audpp->dec_info_table[lidx].codec =
		    (dec_attrb & AUDPP_CODEC_MASK);
		audpp->dec_info_table[lidx].pid = current->pid;
		/* point to row to get supported operation */
		concurrency_entry =
		    ((audpp->dec_database->dec_concurrency_table +
		      ((audpp->concurrency) * (audpp->dec_database->num_dec))) +
		     lidx);
		decid |= ((*concurrency_entry & AUDPP_OP_MASK) >> 12);
		MM_INFO("decid =0x%08x module_name=%s, queueid=%d \n", decid,
				*module_name, *queueid);
	}
done:
	mutex_unlock(audpp->lock_dec);
	return decid;

}
EXPORT_SYMBOL(audpp_adec_alloc);

void audpp_adec_free(int decid)
{
	struct audpp_state *audpp = &the_audpp_state;
	int idx;
	mutex_lock(audpp->lock_dec);
	for (idx = audpp->dec_database->num_dec; idx > 0; idx--) {
		if (audpp->dec_database->dec_info_list[idx - 1].module_decid ==
		    decid) {
			audpp->decoder_count--;
			audpp->\
			codec_cnt[audpp->dec_info_table[idx - 1].codec]--;
			audpp->dec_inuse &= ~(1 << (idx - 1));
			audpp->dec_info_table[idx - 1].codec = -1;
			audpp->dec_info_table[idx - 1].pid = 0;
			MM_INFO("free decid =%d \n", decid);
			break;
		}
	}
	mutex_unlock(audpp->lock_dec);
	return;

}
EXPORT_SYMBOL(audpp_adec_free);

static ssize_t concurrency_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct audpp_state *audpp = &the_audpp_state;
	int rc;
	mutex_lock(audpp->lock_dec);
	rc = sprintf(buf, "%ld\n", audpp->concurrency);
	mutex_unlock(audpp->lock_dec);
	return rc;
}

static ssize_t concurrency_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct audpp_state *audpp = &the_audpp_state;
	unsigned long concurrency;
	int rc = -1;
	mutex_lock(audpp->lock_dec);
	if (audpp->dec_inuse) {
		MM_ERR("Can not change profile, while playback in progress\n");
		goto done;
	}
	rc = strict_strtoul(buf, 10, &concurrency);
	if (!rc &&
		(concurrency < audpp->dec_database->num_concurrency_support)) {
		audpp->concurrency = concurrency;
		MM_DBG("Concurrency case %ld\n", audpp->concurrency);
		rc = count;
	} else {
		MM_ERR("Not a valid Concurrency case\n");
		rc = -EINVAL;
	}
done:
	mutex_unlock(audpp->lock_dec);
	return rc;
}

static ssize_t decoder_info_show(struct device *dev,
				 struct device_attribute *attr, char *buf);
static struct device_attribute dev_attr_decoder[AUDPP_MAX_DECODER_CNT] = {
	__ATTR(decoder0, S_IRUGO, decoder_info_show, NULL),
	__ATTR(decoder1, S_IRUGO, decoder_info_show, NULL),
	__ATTR(decoder2, S_IRUGO, decoder_info_show, NULL),
	__ATTR(decoder3, S_IRUGO, decoder_info_show, NULL),
	__ATTR(decoder4, S_IRUGO, decoder_info_show, NULL),
};

static ssize_t decoder_info_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int cpy_sz = 0;
	struct audpp_state *audpp = &the_audpp_state;
	const ptrdiff_t off = attr - dev_attr_decoder;	/* decoder number */
	mutex_lock(audpp->lock_dec);
	cpy_sz += scnprintf(buf + cpy_sz, PAGE_SIZE - cpy_sz, "%d:",
			    audpp->dec_info_table[off].codec);
	cpy_sz += scnprintf(buf + cpy_sz, PAGE_SIZE - cpy_sz, "%d\n",
			    audpp->dec_info_table[off].pid);
	mutex_unlock(audpp->lock_dec);
	return cpy_sz;
}

static DEVICE_ATTR(concurrency, S_IWUSR | S_IRUGO, concurrency_show,
	    concurrency_store);
static int audpp_probe(struct platform_device *pdev)
{
	int rc, idx;
	struct audpp_state *audpp = &the_audpp_state;
	audpp->concurrency = AUDPP_CONCURRENCY_DEFAULT;
	audpp->dec_database =
	    (struct msm_adspdec_database *)pdev->dev.platform_data;

	MM_INFO("Number of decoder supported  %d\n",
		audpp->dec_database->num_dec);
	MM_INFO("Number of concurrency supported  %d\n",
		audpp->dec_database->num_concurrency_support);
	init_waitqueue_head(&audpp->event_wait);
	for (idx = 0; idx < audpp->dec_database->num_dec; idx++) {
		audpp->dec_info_table[idx].codec = -1;
		audpp->dec_info_table[idx].pid = 0;
		MM_INFO("module_name:%s\n",
			audpp->dec_database->dec_info_list[idx].module_name);
		MM_INFO("queueid:%d\n",
			audpp->dec_database->dec_info_list[idx].module_queueid);
		MM_INFO("decid:%d\n",
			audpp->dec_database->dec_info_list[idx].module_decid);
		MM_INFO("nr_codec_support:%d\n",
			audpp->dec_database->dec_info_list[idx].
			nr_codec_support);
	}

	wake_lock_init(&audpp_wake_lock, WAKE_LOCK_SUSPEND, "audpp");
	for (idx = 0; idx < audpp->dec_database->num_dec; idx++) {
		rc = device_create_file(&pdev->dev, &dev_attr_decoder[idx]);
		if (rc)
			goto err;
	}
	rc = device_create_file(&pdev->dev, &dev_attr_concurrency);
	audpp->op_mode = 0; /* Consider as non turbo mode */
	if (rc)
		goto err;
	else
		goto done;
err:
	while (idx--)
		device_remove_file(&pdev->dev, &dev_attr_decoder[idx]);
done:
	return rc;
}

static struct platform_driver audpp_plat_driver = {
	.probe = audpp_probe,
	.driver = {
		   .name = "msm_adspdec",
		   .owner = THIS_MODULE,
		   },
};

static int __init audpp_init(void)
{
	return platform_driver_register(&audpp_plat_driver);
}

device_initcall(audpp_init);
