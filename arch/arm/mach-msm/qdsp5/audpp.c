
/* arch/arm/mach-msm/qdsp5/audpp.c
 *
 * common code to deal with the AUDPP dsp task (audio postproc)
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009-2010, 2012 Code Aurora Forum. All rights reserved.
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
#include <linux/dma-mapping.h>

#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <mach/board.h>
#include <mach/msm_adsp.h>

#include "audmgr.h"

#include <mach/qdsp5/qdsp5audppcmdi.h>
#include <mach/qdsp5/qdsp5audppmsg.h>
#include <mach/debug_mm.h>

#include "evlog.h"

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
#define LOG(id,arg) ev_log_write(&dsp_log, id, arg)

static DEFINE_MUTEX(audpp_lock);
static DEFINE_MUTEX(audpp_dec_lock);

#define CH_COUNT 5
#define AUDPP_CLNT_MAX_COUNT 6
#define AUDPP_AVSYNC_INFO_SIZE 7

#define AUDPP_SRS_PARAMS 2
#define AUDPP_SRS_PARAMS_G 0
#define AUDPP_SRS_PARAMS_W 1
#define AUDPP_SRS_PARAMS_C 2
#define AUDPP_SRS_PARAMS_H 3
#define AUDPP_SRS_PARAMS_P 4
#define AUDPP_SRS_PARAMS_L 5

#define AUDPP_CMD_CFG_OBJ_UPDATE 0x8000
#define AUDPP_CMD_EQ_FLAG_DIS	0x0000
#define AUDPP_CMD_EQ_FLAG_ENA	-1
#define AUDPP_CMD_IIR_FLAG_DIS	  0x0000
#define AUDPP_CMD_IIR_FLAG_ENA	  -1

#define AUDPP_CMD_VOLUME_PAN		0
#define AUDPP_CMD_IIR_TUNING_FILTER	1
#define AUDPP_CMD_EQUALIZER		2
#define AUDPP_CMD_ADRC			3
#define AUDPP_CMD_SPECTROGRAM		4
#define AUDPP_CMD_QCONCERT		5
#define AUDPP_CMD_SIDECHAIN_TUNING_FILTER	6
#define AUDPP_CMD_SAMPLING_FREQUENCY	7
#define AUDPP_CMD_QAFX			8
#define AUDPP_CMD_QRUMBLE		9
#define AUDPP_CMD_MBADRC		10

#define MAX_EVENT_CALLBACK_CLIENTS 	1

#define AUDPP_CONCURRENCY_DEFAULT 6	/* All non tunnel mode */
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

	/* which channels are actually enabled */
	unsigned avsync_mask;

	/* flags, 48 bits sample/bytes counter per channel */
	uint16_t avsync[CH_COUNT * AUDPP_CLNT_MAX_COUNT + 1];
	struct audpp_event_callback *cb_tbl[MAX_EVENT_CALLBACK_CLIENTS];

	spinlock_t avsync_lock;

	wait_queue_head_t event_wait;
};

struct audpp_state the_audpp_state = {
	.lock = &audpp_lock,
	.lock_dec = &audpp_dec_lock,
};

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
	audpp_cmd_cfg cmd;

	cmd.cmd_id = AUDPP_CMD_CFG;
	cmd.cfg = enable ? AUDPP_CMD_CFG_ENABLE : AUDPP_CMD_CFG_SLEEP;

	return audpp_send_queue1(&cmd, sizeof(cmd));
}

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

static void audpp_fake_event(struct audpp_state *audpp, int id,
			     unsigned event, unsigned arg)
{
	uint16_t msg[1];
	msg[0] = arg;
	audpp->func[id] (audpp->private[id], event, msg);
}

static void audpp_dsp_event(void *data, unsigned id, size_t len,
			    void (*getevent) (void *ptr, size_t len))
{
	struct audpp_state *audpp = data;
	unsigned long flags;
	uint16_t msg[8];
	int cid = 0;

	if (id == AUDPP_MSG_AVSYNC_MSG) {
		spin_lock_irqsave(&audpp->avsync_lock, flags);
		getevent(audpp->avsync, sizeof(audpp->avsync));

		/* mask off any channels we're not watching to avoid
		 * cases where we might get one last update after
		 * disabling avsync and end up in an odd state when
		 * we next read...
		 */
		audpp->avsync[0] &= audpp->avsync_mask;
		spin_unlock_irqrestore(&audpp->avsync_lock, flags);
		return;
	}

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
			if (!audpp->enabled) {
				audpp->enabled = 1;
				audpp_broadcast(audpp, id, msg);
			} else {
				cid = msg[1];
				audpp_fake_event(audpp, cid,
					id, AUDPP_MSG_ENA_ENA);
			}

		} else if (msg[0] == AUDPP_MSG_ENA_DIS) {
			if (audpp->open_count == 0) {
				MM_INFO("DISABLE\n");
				audpp->enabled = 0;
				wake_up(&audpp->event_wait);
				audpp_broadcast(audpp, id, msg);
			} else {
				cid = msg[1];
				audpp_fake_event(audpp, cid,
					id, AUDPP_MSG_ENA_DIS);
				audpp->func[cid] = NULL;
				audpp->private[cid] = NULL;
			}
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
		MM_DBG("Received ADSP event: module enable/disable(audpptask)");
		break;
	default:
		MM_ERR("unhandled msg id %x\n", id);
	}
}

static struct msm_adsp_ops adsp_ops = {
	.event = audpp_dsp_event,
};

int audpp_enable(int id, audpp_event_func func, void *private)
{
	struct audpp_state *audpp = &the_audpp_state;
	uint16_t msg[8];
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
			MM_ERR("cannot open AUDPPTASK\n");
			audpp->open_count = 0;
			audpp->func[id] = NULL;
			audpp->private[id] = NULL;
			goto out;
		}
		LOG(EV_ENABLE, 2);
		msm_adsp_enable(audpp->mod);
		audpp_dsp_config(1);
	} else {
		if (audpp->enabled) {
			msg[0] = AUDPP_MSG_ENA_ENA;
			msg[1] = id;
			res = msm_adsp_generate_event(audpp, audpp->mod,
					 AUDPP_MSG_CFG_MSG, sizeof(msg),
					 sizeof(uint16_t), (void *)msg);
			if (res < 0)
				goto out;
		}
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
	uint16_t msg[8];
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

	msg[0] = AUDPP_MSG_ENA_DIS;
	msg[1] = id;
	rc = msm_adsp_generate_event(audpp, audpp->mod,
				 AUDPP_MSG_CFG_MSG, sizeof(msg),
				 sizeof(uint16_t), (void *)msg);
	if (rc < 0)
		goto out;

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
	}
out:
	mutex_unlock(audpp->lock);
}
EXPORT_SYMBOL(audpp_disable);

#define BAD_ID(id) ((id < 0) || (id >= CH_COUNT))

void audpp_avsync(int id, unsigned rate)
{
	unsigned long flags;
	audpp_cmd_avsync cmd;

	if (BAD_ID(id))
		return;

	spin_lock_irqsave(&the_audpp_state.avsync_lock, flags);
	if (rate)
		the_audpp_state.avsync_mask |= (1 << id);
	else
		the_audpp_state.avsync_mask &= (~(1 << id));
	the_audpp_state.avsync[0] &= the_audpp_state.avsync_mask;
	spin_unlock_irqrestore(&the_audpp_state.avsync_lock, flags);

	cmd.cmd_id = AUDPP_CMD_AVSYNC;
	cmd.object_number = id;
	cmd.interrupt_interval_lsw = rate;
	cmd.interrupt_interval_msw = rate >> 16;
	audpp_send_queue1(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_avsync);

unsigned audpp_avsync_sample_count(int id)
{
	struct audpp_state *audpp = &the_audpp_state;
	uint16_t *avsync = audpp->avsync;
	unsigned val;
	unsigned long flags;
	unsigned mask;

	if (BAD_ID(id))
		return 0;

	mask = 1 << id;
	id = id * AUDPP_AVSYNC_INFO_SIZE + 2;
	spin_lock_irqsave(&audpp->avsync_lock, flags);
	if (avsync[0] & mask)
		val = (avsync[id] << 16) | avsync[id + 1];
	else
		val = 0;
	spin_unlock_irqrestore(&audpp->avsync_lock, flags);

	return val;
}
EXPORT_SYMBOL(audpp_avsync_sample_count);

unsigned audpp_avsync_byte_count(int id)
{
	struct audpp_state *audpp = &the_audpp_state;
	uint16_t *avsync = audpp->avsync;
	unsigned val;
	unsigned long flags;
	unsigned mask;

	if (BAD_ID(id))
		return 0;

	mask = 1 << id;
	id = id * AUDPP_AVSYNC_INFO_SIZE + 5;
	spin_lock_irqsave(&audpp->avsync_lock, flags);
	if (avsync[0] & mask)
		val = (avsync[id] << 16) | avsync[id + 1];
	else
		val = 0;
	spin_unlock_irqrestore(&audpp->avsync_lock, flags);

	return val;
}
EXPORT_SYMBOL(audpp_avsync_byte_count);

int audpp_set_volume_and_pan(unsigned id, unsigned volume, int pan)
{
	/* cmd, obj_cfg[7], cmd_type, volume, pan */
	uint16_t cmd[11];

	if (id > 6)
		return -EINVAL;

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = AUDPP_CMD_CFG_OBJECT_PARAMS;
	cmd[1 + id] = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd[8] = AUDPP_CMD_VOLUME_PAN;
	cmd[9] = volume;
	cmd[10] = pan;

	return audpp_send_queue3(cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_set_volume_and_pan);

/* Implementation of COPP features */
int audpp_dsp_set_mbadrc(unsigned id, unsigned enable,
			 audpp_cmd_cfg_object_params_mbadrc *mbadrc)
{
	audpp_cmd_cfg_object_params_mbadrc cmd;

	if (id != 6)
		return -EINVAL;

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.comman_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_CMD_MBADRC;

	if (enable) {
		memcpy(&cmd.num_bands, &mbadrc->num_bands,
		       sizeof(*mbadrc) -
		       (AUDPP_CMD_CFG_OBJECT_PARAMS_COMMON_LEN + 2));
		cmd.enable = AUDPP_CMD_ADRC_FLAG_ENA;
	} else
		cmd.enable = AUDPP_CMD_ADRC_FLAG_DIS;

	/*order the writes to mbadrc */
	dma_coherent_pre_ops();
	return audpp_send_queue3(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_dsp_set_mbadrc);

int audpp_dsp_set_qconcert_plus(unsigned id, unsigned enable,
				audpp_cmd_cfg_object_params_qconcert *
				qconcert_plus)
{
	audpp_cmd_cfg_object_params_qconcert cmd;
	if (id != 6)
		return -EINVAL;

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.comman_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_CMD_QCONCERT;

	if (enable) {
		memcpy(&cmd.op_mode, &qconcert_plus->op_mode,
		       sizeof(audpp_cmd_cfg_object_params_qconcert) -
		       (AUDPP_CMD_CFG_OBJECT_PARAMS_COMMON_LEN + 2));
		cmd.enable_flag = AUDPP_CMD_ADRC_FLAG_ENA;
	} else
		cmd.enable_flag = AUDPP_CMD_ADRC_FLAG_DIS;

	return audpp_send_queue3(&cmd, sizeof(cmd));
}

int audpp_dsp_set_rx_iir(unsigned id, unsigned enable,
			 audpp_cmd_cfg_object_params_pcm *iir)
{
	audpp_cmd_cfg_object_params_pcm cmd;

	if (id != 6)
		return -EINVAL;

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.comman_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_CMD_IIR_TUNING_FILTER;

	if (enable) {
		cmd.active_flag = AUDPP_CMD_IIR_FLAG_ENA;
		cmd.num_bands = iir->num_bands;
		memcpy(&cmd.params_filter, &iir->params_filter,
		       sizeof(iir->params_filter));
	} else
		cmd.active_flag = AUDPP_CMD_IIR_FLAG_DIS;

	return audpp_send_queue3(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_dsp_set_rx_iir);

int audpp_dsp_set_rx_srs_trumedia_g(
	struct audpp_cmd_cfg_object_params_srstm_g *srstm)
{
	struct audpp_cmd_cfg_object_params_srstm_g cmd;

	MM_DBG("%s\n", __func__);
	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPP_SRS_PARAMS;
	cmd.common.comman_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_SRS_PARAMS_G;

	memcpy(cmd.v, srstm->v, sizeof(srstm->v));

	return audpp_send_queue3(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_dsp_set_rx_srs_trumedia_g);

int audpp_dsp_set_rx_srs_trumedia_w(
	struct audpp_cmd_cfg_object_params_srstm_w *srstm)
{
	struct audpp_cmd_cfg_object_params_srstm_w cmd;

	MM_DBG("%s\n", __func__);
	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPP_SRS_PARAMS;
	cmd.common.comman_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_SRS_PARAMS_W;

	memcpy(cmd.v, srstm->v, sizeof(srstm->v));

	return audpp_send_queue3(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_dsp_set_rx_srs_trumedia_w);

int audpp_dsp_set_rx_srs_trumedia_c(
	struct audpp_cmd_cfg_object_params_srstm_c *srstm)
{
	struct audpp_cmd_cfg_object_params_srstm_c cmd;

	MM_DBG("%s\n", __func__);
	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPP_SRS_PARAMS;
	cmd.common.comman_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_SRS_PARAMS_C;

	memcpy(cmd.v, srstm->v, sizeof(srstm->v));

	return audpp_send_queue3(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_dsp_set_rx_srs_trumedia_c);

int audpp_dsp_set_rx_srs_trumedia_h(
	struct audpp_cmd_cfg_object_params_srstm_h *srstm)
{
	struct audpp_cmd_cfg_object_params_srstm_h cmd;

	MM_DBG("%s\n", __func__);
	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPP_SRS_PARAMS;
	cmd.common.comman_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_SRS_PARAMS_H;

	memcpy(cmd.v, srstm->v, sizeof(srstm->v));

	return audpp_send_queue3(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_dsp_set_rx_srs_trumedia_h);

int audpp_dsp_set_rx_srs_trumedia_p(
	struct audpp_cmd_cfg_object_params_srstm_p *srstm)
{
	struct audpp_cmd_cfg_object_params_srstm_p cmd;

	MM_DBG("%s\n", __func__);
	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPP_SRS_PARAMS;
	cmd.common.comman_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_SRS_PARAMS_P;

	memcpy(cmd.v, srstm->v, sizeof(srstm->v));

	return audpp_send_queue3(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_dsp_set_rx_srs_trumedia_p);

int audpp_dsp_set_rx_srs_trumedia_l(
	struct audpp_cmd_cfg_object_params_srstm_l *srstm)
{
	struct audpp_cmd_cfg_object_params_srstm_l cmd;

	MM_DBG("%s\n", __func__);
	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPP_SRS_PARAMS;
	cmd.common.comman_cfg = AUDPP_CMD_CFG_OBJ_UPDATE;
	cmd.common.command_type = AUDPP_SRS_PARAMS_L;

	memcpy(cmd.v, srstm->v, sizeof(srstm->v));

	return audpp_send_queue3(&cmd, sizeof(cmd));
}
EXPORT_SYMBOL(audpp_dsp_set_rx_srs_trumedia_l);

/* Implementation Of COPP + POPP */
int audpp_dsp_set_eq(unsigned id, unsigned enable,
		     audpp_cmd_cfg_object_params_eqalizer *eq)
{
	audpp_cmd_cfg_object_params_eqalizer cmd;
	unsigned short *id_ptr = (unsigned short *)&cmd;

	if (id > 6 || id == 5)
		return -EINVAL;

	memset(&cmd, 0, sizeof(cmd));
	id_ptr[1 + id] = AUDPP_CMD_CFG_OBJ_UPDATE;
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
			  audpp_cmd_cfg_object_params_volume *vol_pan)
{
	audpp_cmd_cfg_object_params_volume cmd;
	unsigned short *id_ptr = (unsigned short *)&cmd;

	if (id > 6)
		return -EINVAL;

	memset(&cmd, 0, sizeof(cmd));
	id_ptr[1 + id] = AUDPP_CMD_CFG_OBJ_UPDATE;
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
	if (pause == 1)
		pause_cmd[1 + id] = AUDPP_CMD_UPDATE_V | AUDPP_CMD_PAUSE_V;
	else if (pause == 0)
		pause_cmd[1 + id] = AUDPP_CMD_UPDATE_V | AUDPP_CMD_RESUME_V;
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
	flush_cmd[1 + id] = AUDPP_CMD_UPDATE_V | AUDPP_CMD_FLUSH_V;

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
	mutex_lock(audpp->lock_dec);
	/* Represents in bit mask */
	mode = ((dec_attrb & AUDPP_MODE_MASK) << 16);
	codec = (1 << (dec_attrb & AUDPP_CODEC_MASK));
	/* Point to Last entry of the row */
	concurrency_entry = ((audpp->dec_database->dec_concurrency_table +
			      ((audpp->concurrency + 1) *
			       (audpp->dec_database->num_dec))) - 1);

	lidx = audpp->dec_database->num_dec;
	min_codecs_supported = sizeof(unsigned int) * 8;

	MM_DBG("mode = 0x%08x codec = 0x%08x\n", mode, codec);

	for (idx = lidx; idx > 0; idx--, concurrency_entry--) {
		if (!(audpp->dec_inuse & (1 << (idx - 1)))) {
			if ((mode & *concurrency_entry) &&
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
		MM_INFO("decid =0x%08x module_name=%s, queueid=%d \n",
			decid, *module_name, *queueid);
	}
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

	MM_INFO("Number of decoder supported %d\n",
			audpp->dec_database->num_dec);
	MM_INFO("Number of concurrency supported %d\n",
			audpp->dec_database->num_concurrency_support);

	init_waitqueue_head(&audpp->event_wait);

	spin_lock_init(&audpp->avsync_lock);

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

	for (idx = 0; idx < audpp->dec_database->num_dec; idx++) {
		rc = device_create_file(&pdev->dev, &dev_attr_decoder[idx]);
		if (rc)
			goto err;
	}
	rc = device_create_file(&pdev->dev, &dev_attr_concurrency);
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
