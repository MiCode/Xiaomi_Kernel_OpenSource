/* arch/arm/mach-msm/qdsp5/audmgr.c
 *
 * interface to "audmgr" service on the baseband cpu
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009, 2012, 2013 The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/slab.h>

#include <asm/atomic.h>
#include <mach/msm_rpcrouter.h>

#include "audmgr.h"
#include <mach/debug_mm.h>

#define STATE_CLOSED    0
#define STATE_DISABLED  1
#define STATE_ENABLING  2
#define STATE_ENABLED   3
#define STATE_DISABLING 4
#define STATE_ERROR	5
#define MAX_DEVICE_INFO_CALLBACK 1
#define SESSION_VOICE 0
#define SESSION_PLAYBACK 1
#define SESSION_RECORDING 2

/* store information used across complete audmgr sessions */
struct audmgr_global {
	struct mutex *lock;
	struct msm_rpc_endpoint *ept;
	struct task_struct *task;
	uint32_t rpc_version;
	uint32_t rx_device;
	uint32_t tx_device;
	int cad;
	struct device_info_callback *device_cb[MAX_DEVICE_INFO_CALLBACK];

};
static DEFINE_MUTEX(audmgr_lock);

static struct audmgr_global the_audmgr_state = {
	.lock = &audmgr_lock,
};

static void audmgr_rpc_connect(struct audmgr_global *amg)
{
	amg->cad = 0;
	amg->ept = msm_rpc_connect_compatible(AUDMGR_PROG,
			AUDMGR_VERS_COMP_VER3,
			MSM_RPC_UNINTERRUPTIBLE);
	if (IS_ERR(amg->ept)) {
		MM_DBG("connect failed with current VERS"\
				"= %x, trying again with  Cad API\n",
				AUDMGR_VERS_COMP_VER3);
		amg->ept = msm_rpc_connect_compatible(AUDMGR_PROG,
				AUDMGR_VERS_COMP_VER4,
				MSM_RPC_UNINTERRUPTIBLE);
		if (IS_ERR(amg->ept)) {
			amg->ept = msm_rpc_connect_compatible(AUDMGR_PROG,
					AUDMGR_VERS_COMP_VER2,
					MSM_RPC_UNINTERRUPTIBLE);
			if (IS_ERR(amg->ept)) {
				MM_ERR("connect failed with current VERS" \
					"= %x, trying again with another API\n",
					AUDMGR_VERS_COMP_VER2);
				amg->ept = msm_rpc_connect_compatible(
						AUDMGR_PROG,
						AUDMGR_VERS_COMP,
						MSM_RPC_UNINTERRUPTIBLE);
				if (IS_ERR(amg->ept)) {
					MM_ERR("connect failed with current" \
						"VERS=%x, trying again with" \
						"another API\n",
						AUDMGR_VERS_COMP);
					amg->ept = msm_rpc_connect(AUDMGR_PROG,
						AUDMGR_VERS,
						MSM_RPC_UNINTERRUPTIBLE);
					amg->rpc_version = AUDMGR_VERS;
				} else
					amg->rpc_version = AUDMGR_VERS_COMP;
			} else
				amg->rpc_version = AUDMGR_VERS_COMP_VER2;
		} else {
			amg->rpc_version = AUDMGR_VERS_COMP_VER4;
			amg->cad = 1;
		}
	} else
		amg->rpc_version = AUDMGR_VERS_COMP_VER3;

	if (IS_ERR(amg->ept)) {
		amg->ept = NULL;
		MM_ERR("failed to connect to audmgr svc\n");
	}

	return;
}

static void rpc_ack(struct msm_rpc_endpoint *ept, uint32_t xid)
{
	uint32_t rep[6];

	rep[0] = cpu_to_be32(xid);
	rep[1] = cpu_to_be32(1);
	rep[2] = cpu_to_be32(RPCMSG_REPLYSTAT_ACCEPTED);
	rep[3] = cpu_to_be32(RPC_ACCEPTSTAT_SUCCESS);
	rep[4] = 0;
	rep[5] = 0;

	msm_rpc_write(ept, rep, sizeof(rep));
}

static void process_audmgr_callback(struct audmgr_global *amg,
				   void *args, int len)
{
	struct audmgr *am;
	int i = 0;
	struct rpc_audmgr_cb_device_info *temp;

	/* Allow only if complete arguments recevied*/
	if (len < MIN_RPC_DATA_LENGTH)
		return;

	/* Allow only if valid argument */
	if (be32_to_cpu(((struct rpc_audmgr_cb_common *)args)->set_to_one) != 1)
		return;

	switch (be32_to_cpu(((struct rpc_audmgr_cb_common *)args)->status)) {
	case RPC_AUDMGR_STATUS_READY:
		am = (struct audmgr *) be32_to_cpu(
			((struct rpc_audmgr_cb_ready *)args)->client_data);
		if (!am)
			return;
		am->handle = be32_to_cpu(
				((struct rpc_audmgr_cb_ready *)args)->u.handle);
		MM_INFO("rpc READY handle=0x%08x\n", am->handle);
		break;
	case RPC_AUDMGR_STATUS_CODEC_CONFIG: {
		MM_INFO("rpc CODEC_CONFIG\n");
		am = (struct audmgr *) be32_to_cpu(
			((struct rpc_audmgr_cb_ready *)args)->client_data);
		if (!am)
			return;
		if (am->state != STATE_ENABLED)
			am->state = STATE_ENABLED;
		if (!amg->cad) {
			wake_up(&am->wait);
			break;
		}

		if (am->evt.session_info == SESSION_PLAYBACK &&
			am->evt.dev_type.rx_device != amg->rx_device) {
			am->evt.dev_type.rx_device = amg->rx_device;
			am->evt.dev_type.tx_device = 0;
			am->evt.acdb_id = am->evt.dev_type.rx_device;
		}
		if (am->evt.session_info == SESSION_RECORDING &&
			am->evt.dev_type.tx_device != amg->tx_device) {
			am->evt.dev_type.rx_device = 0;
			am->evt.dev_type.tx_device = amg->tx_device;
			am->evt.acdb_id = am->evt.dev_type.tx_device;
		}

		while ((amg->device_cb[i] != NULL) &&
				(i < MAX_DEVICE_INFO_CALLBACK) &&
				(amg->cad)) {
			amg->device_cb[i]->func(&(am->evt),
					amg->device_cb[i]->private);
			i++;
		}
		wake_up(&am->wait);
		break;
	}
	case RPC_AUDMGR_STATUS_PENDING:
		MM_ERR("PENDING?\n");
		break;
	case RPC_AUDMGR_STATUS_SUSPEND:
		MM_ERR("SUSPEND?\n");
		break;
	case RPC_AUDMGR_STATUS_FAILURE:
		MM_ERR("FAILURE\n");
		break;
	case RPC_AUDMGR_STATUS_VOLUME_CHANGE:
		MM_ERR("VOLUME_CHANGE?\n");
		break;
	case RPC_AUDMGR_STATUS_DISABLED:
		MM_ERR("DISABLED\n");
		am = (struct audmgr *) be32_to_cpu(
			((struct rpc_audmgr_cb_ready *)args)->client_data);
		if (!am)
			return;
		am->state = STATE_DISABLED;
		wake_up(&am->wait);
		break;
	case RPC_AUDMGR_STATUS_ERROR:
		MM_ERR("ERROR?\n");
		am = (struct audmgr *) be32_to_cpu(
			((struct rpc_audmgr_cb_ready *)args)->client_data);
		if (!am)
			return;
		am->state = STATE_ERROR;
		wake_up(&am->wait);
		break;
	case RPC_AUDMGR_STATUS_DEVICE_INFO:
		MM_INFO("rpc DEVICE_INFO\n");
		if (!amg->cad)
			break;
		temp = (struct rpc_audmgr_cb_device_info *)args;
		am = (struct audmgr *) be32_to_cpu(temp->client_data);
		if (!am)
			return;
		if (am->evt.session_info == SESSION_PLAYBACK) {
			am->evt.dev_type.rx_device =
					be32_to_cpu(temp->d.rx_device);
			am->evt.dev_type.tx_device = 0;
			am->evt.acdb_id = am->evt.dev_type.rx_device;
			amg->rx_device = am->evt.dev_type.rx_device;
		} else if (am->evt.session_info == SESSION_RECORDING) {
			am->evt.dev_type.rx_device = 0;
			am->evt.dev_type.tx_device =
					be32_to_cpu(temp->d.tx_device);
			am->evt.acdb_id = am->evt.dev_type.tx_device;
			amg->tx_device = am->evt.dev_type.tx_device;
		}
		am->evt.dev_type.ear_mute =
					be32_to_cpu(temp->d.ear_mute);
		am->evt.dev_type.mic_mute =
					be32_to_cpu(temp->d.mic_mute);
		am->evt.dev_type.volume =
					be32_to_cpu(temp->d.volume);
		break;
	case RPC_AUDMGR_STATUS_DEVICE_CONFIG:
		MM_ERR("rpc DEVICE_CONFIG\n");
		break;
	default:
		break;
	}
}

static void process_rpc_request(uint32_t proc, uint32_t xid,
				void *data, int len, void *private)
{
	struct audmgr_global *amg = private;

	if (proc == AUDMGR_CB_FUNC_PTR)
		process_audmgr_callback(amg, data, len);
	else
		MM_ERR("unknown rpc proc %d\n", proc);
	rpc_ack(amg->ept, xid);
}

#define RPC_TYPE_REQUEST 0
#define RPC_TYPE_REPLY 1

#define RPC_VERSION 2

#define RPC_COMMON_HDR_SZ  (sizeof(uint32_t) * 2)
#define RPC_REQUEST_HDR_SZ (sizeof(struct rpc_request_hdr))
#define RPC_REPLY_HDR_SZ   (sizeof(uint32_t) * 3)
#define RPC_REPLY_SZ       (sizeof(uint32_t) * 6)

static int audmgr_rpc_thread(void *data)
{
	struct audmgr_global *amg = data;
	struct rpc_request_hdr *hdr = NULL;
	uint32_t type;
	int len;

	MM_INFO("start\n");

	while (!kthread_should_stop()) {
		if (hdr) {
			kfree(hdr);
			hdr = NULL;
		}
		len = msm_rpc_read(amg->ept, (void **) &hdr, -1, -1);
		if (len < 0) {
			MM_ERR("rpc read failed (%d)\n", len);
			break;
		}
		if (len < RPC_COMMON_HDR_SZ)
			continue;

		type = be32_to_cpu(hdr->type);
		if (type == RPC_TYPE_REPLY) {
			struct rpc_reply_hdr *rep = (void *) hdr;
			uint32_t status;
			if (len < RPC_REPLY_HDR_SZ)
				continue;
			status = be32_to_cpu(rep->reply_stat);
			if (status == RPCMSG_REPLYSTAT_ACCEPTED) {
				status = be32_to_cpu(rep->data.acc_hdr.accept_stat);
				MM_INFO("rpc_reply status %d\n", status);
			} else {
				MM_INFO("rpc_reply denied!\n");
			}
			/* process reply */
			continue;
		}

		if (len < RPC_REQUEST_HDR_SZ)
			continue;

		process_rpc_request(be32_to_cpu(hdr->procedure),
				    be32_to_cpu(hdr->xid),
				    (void *) (hdr + 1),
				    len - sizeof(*hdr),
				    data);
	}
	MM_INFO("exit\n");
	if (hdr) {
		kfree(hdr);
		hdr = NULL;
	}
	amg->task = NULL;
	return 0;
}

static unsigned convert_samp_index(unsigned index)
{
	switch (index) {
	case RPC_AUD_DEF_SAMPLE_RATE_48000:	return 48000;
	case RPC_AUD_DEF_SAMPLE_RATE_44100:	return 44100;
	case RPC_AUD_DEF_SAMPLE_RATE_32000:	return 32000;
	case RPC_AUD_DEF_SAMPLE_RATE_24000:	return 24000;
	case RPC_AUD_DEF_SAMPLE_RATE_22050:	return 22050;
	case RPC_AUD_DEF_SAMPLE_RATE_16000:	return 16000;
	case RPC_AUD_DEF_SAMPLE_RATE_12000:	return 12000;
	case RPC_AUD_DEF_SAMPLE_RATE_11025:	return 11025;
	case RPC_AUD_DEF_SAMPLE_RATE_8000:	return 8000;
	default:				return 11025;
	}
}

static void get_current_session_info(struct audmgr *am,
				struct audmgr_config *cfg)
{
	if (cfg->def_method == RPC_AUD_DEF_METHOD_PLAYBACK ||
	   (cfg->def_method == RPC_AUD_DEF_METHOD_HOST_PCM && cfg->rx_rate)) {
		am->evt.session_info = SESSION_PLAYBACK; /* playback */
		am->evt.sample_rate = convert_samp_index(cfg->rx_rate);
	} else if (cfg->def_method == RPC_AUD_DEF_METHOD_RECORD) {
		am->evt.session_info = SESSION_RECORDING; /* recording */
		am->evt.sample_rate = convert_samp_index(cfg->tx_rate);
	} else
		am->evt.session_info = SESSION_VOICE;
}

struct audmgr_enable_msg {
	struct rpc_request_hdr hdr;
	struct rpc_audmgr_enable_client_args args;
};

struct audmgr_disable_msg {
	struct rpc_request_hdr hdr;
	uint32_t handle;
};

int audmgr_open(struct audmgr *am)
{
	struct audmgr_global *amg = &the_audmgr_state;
	int rc;

	if (am->state != STATE_CLOSED)
		return 0;

	mutex_lock(amg->lock);

	/* connect to audmgr end point and polling thread only once */
	if (amg->ept == NULL) {
		audmgr_rpc_connect(amg);
		if (IS_ERR(amg->ept)) {
			rc = PTR_ERR(amg->ept);
			amg->ept = NULL;
			MM_ERR("failed to connect to audmgr svc\n");
			goto done;
		}

		amg->task = kthread_run(audmgr_rpc_thread, amg, "audmgr_rpc");
		if (IS_ERR(amg->task)) {
			rc = PTR_ERR(amg->task);
			amg->task = NULL;
			msm_rpc_close(amg->ept);
			amg->ept = NULL;
			goto done;
		}
	}

	/* Initialize session parameters */
	init_waitqueue_head(&am->wait);
	am->state = STATE_DISABLED;
	rc = 0;
done:
	mutex_unlock(amg->lock);
	return rc;
}
EXPORT_SYMBOL(audmgr_open);

int audmgr_close(struct audmgr *am)
{
	return -EBUSY;
}
EXPORT_SYMBOL(audmgr_close);

int audmgr_enable(struct audmgr *am, struct audmgr_config *cfg)
{
	struct audmgr_global *amg = &the_audmgr_state;
	struct audmgr_enable_msg msg;
	int rc;

	if (am->state == STATE_ENABLED)
		return 0;

	if (am->state == STATE_DISABLING)
		MM_ERR("state is DISABLING in enable?\n");
	am->state = STATE_ENABLING;

	MM_INFO("session 0x%08x\n", (int) am);
	msg.args.set_to_one = cpu_to_be32(1);
	msg.args.tx_sample_rate = cpu_to_be32(cfg->tx_rate);
	msg.args.rx_sample_rate = cpu_to_be32(cfg->rx_rate);
	msg.args.def_method = cpu_to_be32(cfg->def_method);
	msg.args.codec_type = cpu_to_be32(cfg->codec);
	msg.args.snd_method = cpu_to_be32(cfg->snd_method);
	msg.args.cb_func = cpu_to_be32(0x11111111);
	msg.args.client_data = cpu_to_be32((int)am);

	get_current_session_info(am, cfg);
	msm_rpc_setup_req(&msg.hdr, AUDMGR_PROG, amg->rpc_version,
			  AUDMGR_ENABLE_CLIENT);

	rc = msm_rpc_write(amg->ept, &msg, sizeof(msg));
	if (rc < 0)
		return rc;

	rc = wait_event_timeout(am->wait, am->state != STATE_ENABLING, 15 * HZ);
	if (rc == 0) {
		MM_ERR("ARM9 did not reply to RPC am->state = %d\n", am->state);
	}
	if (am->state == STATE_ENABLED)
		return 0;

	am->evt.session_info = -1;
	MM_ERR("unexpected state %d while enabling?!\n", am->state);
	return -ENODEV;
}
EXPORT_SYMBOL(audmgr_enable);

int audmgr_disable(struct audmgr *am)
{
	struct audmgr_global *amg = &the_audmgr_state;
	struct audmgr_disable_msg msg;
	int rc;

	if (am->state == STATE_DISABLED)
		return 0;

	MM_INFO("session 0x%08x\n", (int) am);
	am->evt.session_info = -1;
	msg.handle = cpu_to_be32(am->handle);
	msm_rpc_setup_req(&msg.hdr, AUDMGR_PROG, amg->rpc_version,
			  AUDMGR_DISABLE_CLIENT);

	am->state = STATE_DISABLING;

	rc = msm_rpc_write(amg->ept, &msg, sizeof(msg));
	if (rc < 0)
		return rc;

	rc = wait_event_timeout(am->wait, am->state != STATE_DISABLING, 15 * HZ);
	if (rc == 0) {
		MM_ERR("ARM9 did not reply to RPC am->state = %d\n", am->state);
	}

	if (am->state == STATE_DISABLED)
		return 0;

	MM_ERR("unexpected state %d while disabling?!\n", am->state);
	return -ENODEV;
}
EXPORT_SYMBOL(audmgr_disable);

int audmgr_register_device_info_callback(struct device_info_callback *dcb)
{
	struct audmgr_global *amg = &the_audmgr_state;
	int i;

	for (i = 0; i < MAX_DEVICE_INFO_CALLBACK; i++) {
		if (NULL == amg->device_cb[i]) {
			amg->device_cb[i] = dcb;
			return 0;
		}
	}
	return -EINVAL;
}
EXPORT_SYMBOL(audmgr_register_device_info_callback);

int audmgr_deregister_device_info_callback(struct device_info_callback *dcb)
{
	struct audmgr_global *amg = &the_audmgr_state;
	int i;

	for (i = 0; i < MAX_DEVICE_INFO_CALLBACK; i++) {
		if (dcb == amg->device_cb[i]) {
			amg->device_cb[i] = NULL;
			return 0;
		}
	}
	return -EINVAL;
}
EXPORT_SYMBOL(audmgr_deregister_device_info_callback);
