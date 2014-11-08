/*
 * Copyright (c) 2013-2014, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/memory_alloc.h>
#include <linux/debugfs.h>
#include <linux/time.h>
#include <linux/atomic.h>
#include <sound/apr_audio-v2.h>
#include <sound/lsm_params.h>
#include <sound/q6core.h>
#include <sound/q6lsm.h>
#include <asm/ioctls.h>
#include <mach/memory.h>
#include <mach/debug_mm.h>
#include "audio_acdb.h"

#define APR_TIMEOUT	(5 * HZ)
#define LSM_CAL_SIZE	4096
#define LSM_ALIGN_BOUNDARY 512

enum {
	CMD_STATE_CLEARED = 0,
	CMD_STATE_WAIT_RESP = 1,
};

enum {
	LSM_INVALID_SESSION_ID = 0,
	LSM_MIN_SESSION_ID = 1,
	LSM_MAX_SESSION_ID = 8,
	LSM_CONTROL_SESSION = 0x0F,
};

#define CHECK_SESSION(x) (x < LSM_MIN_SESSION_ID || x > LSM_MAX_SESSION_ID)
struct lsm_common {
	void *apr;
	atomic_t apr_users;
	struct lsm_client	common_client[LSM_MAX_SESSION_ID + 1];
	struct mutex apr_lock;
};

static struct lsm_common lsm_common;

/*
 * mmap_handle_p can point either client->sound_model.mem_map_handle or
 * lsm_common.mmap_handle_for_cal.
 * mmap_lock must be held while accessing this.
 */
static spinlock_t mmap_lock;
static uint32_t *mmap_handle_p;

static spinlock_t lsm_session_lock;
static struct lsm_client *lsm_session[LSM_MAX_SESSION_ID + 1];

static int q6lsm_mmapcallback(struct apr_client_data *data, void *priv);
static int q6lsm_send_cal(struct lsm_client *client);

static int q6lsm_callback(struct apr_client_data *data, void *priv)
{
	struct lsm_client *client = (struct lsm_client *)priv;
	uint32_t token;
	uint32_t *payload;

	if (!client || !data) {
		WARN_ON(1);
		return -EINVAL;
	}

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: SSR event received 0x%x, event 0x%x, proc 0x%x\n",
			 __func__, data->opcode, data->reset_event,
			 data->reset_proc);
		return 0;
	}

	payload = data->payload;
	pr_debug("%s: Session %d opcode 0x%x token 0x%x payload size %d\n"
			 "payload [0] = %x\n", __func__, client->session,
		data->opcode, data->token, data->payload_size, payload[0]);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		token = data->token;
		switch (payload[0]) {
		case LSM_SESSION_CMD_START:
		case LSM_SESSION_CMD_STOP:
		case LSM_SESSION_CMD_SET_PARAMS:
		case LSM_SESSION_CMD_OPEN_TX:
		case LSM_SESSION_CMD_CLOSE_TX:
		case LSM_SESSION_CMD_REGISTER_SOUND_MODEL:
		case LSM_SESSION_CMD_DEREGISTER_SOUND_MODEL:
		case LSM_SESSION_CMD_SHARED_MEM_UNMAP_REGIONS:
			if (token != client->session &&
			    payload[0] !=
				LSM_SESSION_CMD_DEREGISTER_SOUND_MODEL) {
				pr_err("%s: Invalid session %d receivced expected %d\n",
					__func__, token, client->session);
				return -EINVAL;
			}
			if (atomic_cmpxchg(&client->cmd_state,
					   CMD_STATE_WAIT_RESP,
					   CMD_STATE_CLEARED) ==
					       CMD_STATE_WAIT_RESP)
				wake_up(&client->cmd_wait);
			break;
		default:
			pr_debug("%s: Unknown command 0x%x\n",
				__func__, payload[0]);
			break;
		}
		return 0;
	}

	if (client->cb)
		client->cb(data->opcode, data->token, data->payload,
			   client->priv);

	return 0;
}

static int q6lsm_session_alloc(struct lsm_client *client)
{
	unsigned long flags;
	int n, ret = -ENOMEM;

	spin_lock_irqsave(&lsm_session_lock, flags);
	for (n = LSM_MIN_SESSION_ID; n <= LSM_MAX_SESSION_ID; n++) {
		if (!lsm_session[n]) {
			lsm_session[n] = client;
			ret = n;
			break;
		}
	}
	spin_unlock_irqrestore(&lsm_session_lock, flags);
	pr_debug("%s: Alloc Session %d", __func__, n);
	return ret;
}

static void q6lsm_session_free(struct lsm_client *client)
{
	unsigned long flags;
	pr_debug("%s: Freeing session ID %d\n", __func__, client->session);
	spin_lock_irqsave(&lsm_session_lock, flags);
	lsm_session[client->session] = LSM_INVALID_SESSION_ID;
	spin_unlock_irqrestore(&lsm_session_lock, flags);
	client->session = LSM_INVALID_SESSION_ID;
}

static void *q6lsm_mmap_apr_reg(void)
{
	if (atomic_inc_return(&lsm_common.apr_users) == 1) {
		lsm_common.apr =
		    apr_register("ADSP", "LSM", q6lsm_mmapcallback,
				 0x0FFFFFFFF, &lsm_common);
		if (!lsm_common.apr) {
			pr_debug("%s Unable to register APR LSM common port\n",
				 __func__);
			atomic_dec(&lsm_common.apr_users);
		}
	}

	return lsm_common.apr;
}

static int q6lsm_mmap_apr_dereg(void)
{
	if (atomic_read(&lsm_common.apr_users) <= 0) {
		WARN("%s: APR common port already closed\n", __func__);
	} else {
		if (atomic_dec_return(&lsm_common.apr_users) == 0) {
			apr_deregister(lsm_common.apr);
			pr_debug("%s:APR De-Register common port\n", __func__);
		}
	}
	return 0;
}

struct lsm_client *q6lsm_client_alloc(lsm_app_cb cb, void *priv)
{
	struct lsm_client *client;
	int n;

	pr_debug("%s: enter\n", __func__);
	client = kzalloc(sizeof(struct lsm_client), GFP_KERNEL);
	if (!client)
		return NULL;
	n = q6lsm_session_alloc(client);
	if (n <= 0) {
		kfree(client);
		return NULL;
	}
	client->session = n;
	client->cb = cb;
	client->priv = priv;
	pr_debug("%s:Client session %d\n", __func__, client->session);
	if (CHECK_SESSION(client->session)) {
		kfree(client);
		return NULL;
	}
	pr_debug("%s:Client Session %d\n", __func__, client->session);
	client->apr = apr_register("ADSP", "LSM", q6lsm_callback,
				   ((client->session) << 8 | client->session),
				   client);

	if (client->apr == NULL) {
		pr_err("%s: Registration with APR failed\n", __func__);
		goto fail;
	}

	pr_debug("%s Registering the common port with APR\n", __func__);
	client->mmap_apr = q6lsm_mmap_apr_reg();
	if (!client->mmap_apr) {
		pr_err("%s: APR registration failed\n", __func__);
		goto fail;
	}

	init_waitqueue_head(&client->cmd_wait);
	mutex_init(&client->cmd_lock);
	atomic_set(&client->cmd_state, CMD_STATE_CLEARED);
	pr_debug("%s: New client allocated\n", __func__);
	return client;
fail:
	q6lsm_client_free(client);
	return NULL;
}

void q6lsm_client_free(struct lsm_client *client)
{
	if (!client)
		return;
	if (CHECK_SESSION(client->session)) {
		pr_err("%s: Invalid Session %d", __func__, client->session);
		return;
	}
	apr_deregister(client->apr);
	client->mmap_apr = NULL;
	q6lsm_session_free(client);
	q6lsm_mmap_apr_dereg();
	mutex_destroy(&client->cmd_lock);
	kfree(client);
}

/*
 * q6lsm_apr_send_pkt : If wait == true, hold mutex to prevent from preempting
 *			other thread's wait.
 *			If mmap_handle_p != NULL, disable irq and spin lock to
 *			protect mmap_handle_p
 */
static int q6lsm_apr_send_pkt(struct lsm_client *client, void *handle,
			      void *data, bool wait, uint32_t *mmap_p)
{
	int ret;
	unsigned long flags = 0;

	pr_debug("%s: enter wait %d\n", __func__, wait);
	if (wait)
		mutex_lock(&lsm_common.apr_lock);
	if (mmap_p) {
		WARN_ON(!wait);
		spin_lock_irqsave(&mmap_lock, flags);
		mmap_handle_p = mmap_p;
	}
	atomic_set(&client->cmd_state, CMD_STATE_WAIT_RESP);
	ret = apr_send_pkt(handle, data);
	if (mmap_p)
		spin_unlock_irqrestore(&mmap_lock, flags);

	if (ret < 0) {
		pr_err("%s: apr_send_pkt failed %d\n", __func__, ret);
	} else if (wait) {
		ret = wait_event_timeout(client->cmd_wait,
					 (atomic_read(&client->cmd_state) ==
					      CMD_STATE_CLEARED),
					 APR_TIMEOUT);
		if (likely(ret))
			ret = 0;
		else
			pr_err("%s: wait timedout\n", __func__);
	} else {
		ret = 0;
	}
	if (wait)
		mutex_unlock(&lsm_common.apr_lock);

	pr_debug("%s: leave ret %d\n", __func__, ret);
	return ret;
}

static void q6lsm_add_hdr(struct lsm_client *client, struct apr_hdr *hdr,
			uint32_t pkt_size, bool cmd_flg)
{
	pr_debug("%s: pkt_size %d cmd_flg %d session %d\n", __func__,
		pkt_size, cmd_flg, client->session);
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				       APR_HDR_LEN(sizeof(struct apr_hdr)),
				       APR_PKT_VER);
	hdr->src_svc = APR_SVC_LSM;
	hdr->src_domain = APR_DOMAIN_APPS;
	hdr->dest_svc = APR_SVC_LSM;
	hdr->dest_domain = APR_DOMAIN_ADSP;
	hdr->src_port = ((client->session << 8) & 0xFF00) | client->session;
	hdr->dest_port = ((client->session << 8) & 0xFF00) | client->session;
	hdr->pkt_size = pkt_size;
	if (cmd_flg)
		hdr->token = client->session;
}

int q6lsm_open(struct lsm_client *client)
{
	int rc;
	struct lsm_stream_cmd_open_tx open;

	memset(&open, 0, sizeof(open));
	q6lsm_add_hdr(client, &open.hdr, sizeof(open), true);

	open.hdr.opcode = LSM_SESSION_CMD_OPEN_TX;
	open.app_id = 1;
	open.sampling_rate = 16000;

	rc = q6lsm_apr_send_pkt(client, client->apr, &open, true, NULL);
	if (rc)
		pr_err("%s: Open failed opcode 0x%x, rc %d\n",
		       __func__, open.hdr.opcode, rc);
	pr_debug("%s: leave %d\n", __func__, rc);
	return rc;
}

static int q6lsm_set_params(struct lsm_client *client)
{
	int rc;
	struct lsm_cmd_set_params params;
	struct lsm_params_payload *payload = &params.payload;

	pr_debug("%s: enter\n", __func__);
	q6lsm_add_hdr(client, &params.hdr, sizeof(params), true);

	params.hdr.opcode = LSM_SESSION_CMD_SET_PARAMS;
	params.data_payload_addr_lsw = 0;
	params.data_payload_addr_msw = 0;
	params.mem_map_handle = 0;
	params.data_payload_size = sizeof(struct lsm_params_payload);

	payload->op_mode.common.module_id  = LSM_MODULE_ID_VOICE_WAKEUP;
	payload->op_mode.common.param_id = LSM_PARAM_ID_OPERATION_MODE;
	payload->op_mode.common.param_size =
	    sizeof(struct lsm_param_op_mode) - sizeof(payload->op_mode.common);
	payload->op_mode.common.reserved = 0;
	payload->op_mode.minor_version = 1;
	payload->op_mode.mode = client->mode;
	payload->op_mode.reserved = 0;

	payload->connect_to_port.common.module_id = LSM_MODULE_ID_VOICE_WAKEUP;
	payload->connect_to_port.common.param_id = LSM_PARAM_ID_CONNECT_TO_PORT;
	payload->connect_to_port.common.param_size =
	    sizeof(payload->connect_to_port) - sizeof(payload->op_mode.common);
	payload->connect_to_port.common.reserved = 0;
	payload->connect_to_port.minor_version = 1;
	payload->connect_to_port.port_id = client->connect_to_port;
	payload->connect_to_port.reserved = 0;

	payload->kwds.common.module_id = LSM_MODULE_ID_VOICE_WAKEUP;
	payload->kwds.common.param_id = LSM_PARAM_ID_KEYWORD_DETECT_SENSITIVITY;
	payload->kwds.common.param_size =
	    sizeof(payload->kwds) - sizeof(payload->op_mode.common);
	payload->kwds.common.reserved = 0;
	payload->kwds.minor_version = 1;
	payload->kwds.keyword_sensitivity = client->kw_sensitivity;
	payload->kwds.reserved = 0;

	payload->uds.common.module_id = LSM_MODULE_ID_VOICE_WAKEUP;
	payload->uds.common.param_id = LSM_PARAM_ID_USER_DETECT_SENSITIVITY;
	payload->uds.common.param_size =
	    sizeof(payload->uds) - sizeof(payload->op_mode.common);
	payload->uds.common.reserved = 0;
	payload->uds.minor_version = 1;
	payload->uds.user_sensitivity = client->user_sensitivity;
	payload->uds.reserved = 0;

	rc = q6lsm_apr_send_pkt(client, client->apr, &params, true, NULL);
	if (rc)
		pr_err("%s: Failed set_params opcode 0x%x, rc %d\n",
		       __func__, params.hdr.opcode, rc);

	pr_debug("%s: leave %d\n", __func__, rc);
	return rc;
}

int q6lsm_register_sound_model(struct lsm_client *client,
			       enum lsm_detection_mode mode, u16 minkeyword,
			       u16 minuser, bool detectfailure)
{
	int rc;
	struct lsm_cmd_reg_snd_model cmd;

	memset(&cmd, 0, sizeof(cmd));
	if (mode == LSM_MODE_KEYWORD_ONLY_DETECTION) {
		client->mode = 0x01;
	} else if (mode == LSM_MODE_USER_KEYWORD_DETECTION) {
		client->mode = 0x03;
	} else {
		pr_err("%s: Incorrect detection mode %d\n", __func__, mode);
		return -EINVAL;
	}
	client->mode |= detectfailure << 2;
	client->kw_sensitivity = minkeyword;
	client->user_sensitivity = minuser;
	client->connect_to_port = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_5_TX;

	rc = q6lsm_set_params(client);
	if (rc < 0) {
		pr_err("%s: Failed to set lsm config params\n", __func__);
		return rc;
	}
	rc = q6lsm_send_cal(client);
	if (rc < 0) {
		pr_err("%s: Failed to send calibration data\n", __func__);
		return rc;
	}

	q6lsm_add_hdr(client, &cmd.hdr, sizeof(cmd), true);
	cmd.hdr.opcode = LSM_SESSION_CMD_REGISTER_SOUND_MODEL;
	cmd.model_addr_lsw = lower_32_bits(client->sound_model.phys);
	cmd.model_addr_msw = upper_32_bits(client->sound_model.phys);
	cmd.model_size = client->sound_model.size;
	/* read updated mem_map_handle by q6lsm_mmapcallback */
	rmb();
	cmd.mem_map_handle = client->sound_model.mem_map_handle;

	pr_debug("%s: addr %pa, size %d, handle %x\n", __func__,
		&client->sound_model.phys, cmd.model_size, cmd.mem_map_handle);
	rc = q6lsm_apr_send_pkt(client, client->apr, &cmd, true, NULL);
	if (rc)
		pr_err("%s: Failed cmd op[0x%x]rc[%d]\n", __func__,
		       cmd.hdr.opcode, rc);
	else
		pr_debug("%s: Register sound model succeeded\n", __func__);

	return rc;
}

int q6lsm_deregister_sound_model(struct lsm_client *client)
{
	int rc;
	struct lsm_cmd_reg_snd_model cmd;

	if (!client || !client->apr) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("%s: session[%d]", __func__, client->session);
	if (CHECK_SESSION(client->session))
		return -EINVAL;

	memset(&cmd, 0, sizeof(cmd));
	q6lsm_add_hdr(client, &cmd.hdr, sizeof(cmd.hdr), false);
	cmd.hdr.opcode = LSM_SESSION_CMD_DEREGISTER_SOUND_MODEL;

	rc = q6lsm_apr_send_pkt(client, client->apr, &cmd.hdr, true, NULL);
	if (rc < 0) {
		pr_err("%s: Failed cmd opcode 0x%x, rc %d\n", __func__,
		       cmd.hdr.opcode, rc);
	} else {
		pr_debug("%s: Deregister sound model succeeded\n", __func__);
	}

	q6lsm_snd_model_buf_free(client);

	return rc;
}

static void q6lsm_add_mmaphdr(struct lsm_client *client, struct apr_hdr *hdr,
			      u32 pkt_size, u32 cmd_flg, u32 token)
{
	pr_debug("%s:pkt size=%d cmd_flg=%d session=%d\n", __func__, pkt_size,
		 cmd_flg, client->session);
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				       APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	hdr->src_port = 0x00;
	hdr->dest_port = client->session;
	if (cmd_flg)
		hdr->token = token;
	hdr->pkt_size = pkt_size;
	return;
}

static int q6lsm_memory_map_regions(struct lsm_client *client,
				    dma_addr_t dma_addr_p, uint32_t dma_buf_sz,
				    uint32_t *mmap_p)
{
	struct avs_cmd_shared_mem_map_regions *mmap_regions = NULL;
	struct avs_shared_map_region_payload *mregions = NULL;
	void *mmap_region_cmd = NULL;
	void *payload = NULL;
	int rc;
	int cmd_size = 0;

	pr_debug("%s: dma_addr_p 0x%pa, dma_buf_sz %d, mmap_p 0x%p, session %d\n",
		__func__, &dma_addr_p, dma_buf_sz, mmap_p,
		client->session);
	if (CHECK_SESSION(client->session))
		return -EINVAL;
	cmd_size = sizeof(struct avs_cmd_shared_mem_map_regions) +
		   sizeof(struct avs_shared_map_region_payload);

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!mmap_region_cmd)
		return -ENOMEM;

	mmap_regions = (struct avs_cmd_shared_mem_map_regions *)mmap_region_cmd;
	q6lsm_add_mmaphdr(client, &mmap_regions->hdr, cmd_size, true,
			  (client->session << 8));

	mmap_regions->hdr.opcode = LSM_SESSION_CMD_SHARED_MEM_MAP_REGIONS;
	mmap_regions->mem_pool_id = ADSP_MEMORY_MAP_SHMEM8_4K_POOL;
	mmap_regions->num_regions = 1;
	mmap_regions->property_flag = 0x00;
	payload = ((u8 *)mmap_region_cmd +
		   sizeof(struct avs_cmd_shared_mem_map_regions));
	mregions = (struct avs_shared_map_region_payload *)payload;

	mregions->shm_addr_lsw = lower_32_bits(dma_addr_p);
	mregions->shm_addr_msw = upper_32_bits(dma_addr_p);
	mregions->mem_size_bytes = dma_buf_sz;

	rc = q6lsm_apr_send_pkt(client, client->mmap_apr, mmap_region_cmd,
				true, mmap_p);
	if (rc)
		pr_err("%s: Failed mmap_regions opcode 0x%x, rc %d\n",
			__func__, mmap_regions->hdr.opcode, rc);

	pr_debug("%s: leave %d\n", __func__, rc);
	kfree(mmap_region_cmd);
	return rc;
}

static int q6lsm_memory_unmap_regions(struct lsm_client *client,
				      uint32_t handle)
{
	struct avs_cmd_shared_mem_unmap_regions unmap;
	int rc = 0;
	int cmd_size = 0;
	if (CHECK_SESSION(client->session))
		return -EINVAL;
	cmd_size = sizeof(struct avs_cmd_shared_mem_unmap_regions);
	q6lsm_add_mmaphdr(client, &unmap.hdr, cmd_size,
			  true, (client->session << 8));
	unmap.hdr.opcode = LSM_SESSION_CMD_SHARED_MEM_UNMAP_REGIONS;
	unmap.mem_map_handle = handle;

	pr_debug("%s: unmap handle 0x%x\n", __func__, unmap.mem_map_handle);
	rc = q6lsm_apr_send_pkt(client, client->mmap_apr, &unmap, true,
				NULL);
	if (rc)
		pr_err("%s: Failed mmap_regions opcode 0x%x rc %d\n",
		       __func__, unmap.hdr.opcode, rc);

	return rc;
}

static int q6lsm_send_cal(struct lsm_client *client)
{
	int rc;

	struct lsm_cmd_set_params params;
	struct acdb_cal_block lsm_cal;

	pr_debug("%s: Session %d\n", __func__, client->session);
	if (CHECK_SESSION(client->session))
		return -EINVAL;
	memset(&lsm_cal, 0, sizeof(lsm_cal));
	get_lsm_cal(&lsm_cal);
	/* Cache mmap address, only map once or if new addr */
	lsm_common.common_client[client->session].session = client->session;
	q6lsm_add_hdr(client, &params.hdr, sizeof(params), true);
	params.hdr.opcode = LSM_SESSION_CMD_SET_PARAMS;
	params.data_payload_addr_lsw = lower_32_bits(client->lsm_cal_phy_addr);
	params.data_payload_addr_msw = upper_32_bits(client->lsm_cal_phy_addr);
	params.mem_map_handle = client->sound_model.mem_map_handle;
	params.data_payload_size = lsm_cal.cal_size;
	pr_debug("%s: Cal Size = %x", __func__, client->lsm_cal_size);
	rc = q6lsm_apr_send_pkt(client, client->apr, &params, true, NULL);
	if (rc)
		pr_err("%s: Failed set_params opcode 0x%x, rc %d\n",
		       __func__, params.hdr.opcode, rc);
	return rc;
}

int q6lsm_snd_model_buf_free(struct lsm_client *client)
{
	int rc;

	pr_debug("%s: Session id %d\n", __func__, client->session);
	if (CHECK_SESSION(client->session))
		return -EINVAL;

	mutex_lock(&client->cmd_lock);
	rc = q6lsm_memory_unmap_regions(client,
					client->sound_model.mem_map_handle);
	if (rc < 0)
		pr_err("%s CMD Memory_unmap_regions failed\n", __func__);

	if (client->sound_model.data) {
		ion_unmap_kernel(client->sound_model.client,
						 client->sound_model.handle);
		ion_free(client->sound_model.client,
				 client->sound_model.handle);
		ion_client_destroy(client->sound_model.client);
		client->sound_model.client = NULL;
		client->sound_model.handle = NULL;
		client->sound_model.data = NULL;
		client->sound_model.phys = 0;
		client->lsm_cal_phy_addr = 0;
		client->lsm_cal_size = 0;
	}
	mutex_unlock(&client->cmd_lock);
	return rc;
}

static struct lsm_client *q6lsm_get_lsm_client(int session_id)
{
	unsigned long flags;
	struct lsm_client *client = NULL;

	if (session_id == LSM_CONTROL_SESSION) {
		client = &lsm_common.common_client[session_id];
		goto done;
	}

	spin_lock_irqsave(&lsm_session_lock, flags);
	if (session_id < LSM_MIN_SESSION_ID || session_id > LSM_MAX_SESSION_ID)
		pr_err("%s: Invalid session %d\n", __func__, session_id);
	else if (!lsm_session[session_id])
		pr_err("%s: Not an active session %d\n", __func__, session_id);
	else
		client = lsm_session[session_id];
	spin_unlock_irqrestore(&lsm_session_lock, flags);
done:
	return client;
}

/*
 * q6lsm_mmapcallback : atomic context
 */
static int q6lsm_mmapcallback(struct apr_client_data *data, void *priv)
{
	unsigned long flags;
	uint32_t command;
	uint32_t retcode;
	uint32_t sid;
	const uint32_t *payload = data->payload;
	struct lsm_client *client = NULL;

	if (data->opcode == RESET_EVENTS) {
		sid = (data->token >> 8) & 0x0F;
		pr_debug("%s: SSR event received 0x%x, event 0x%x,\n"
			 "proc 0x%x SID 0x%x\n", __func__, data->opcode,
			 data->reset_event, data->reset_proc, sid);
		lsm_common.common_client[sid].lsm_cal_phy_addr = 0;
		return 0;
	}

	command = payload[0];
	retcode = payload[1];
	sid = (data->token >> 8) & 0x0F;
	pr_debug("%s: opcode 0x%x command 0x%x return code 0x%x SID 0x%x\n",
		 __func__, data->opcode, command, retcode, sid);
	client = q6lsm_get_lsm_client(sid);
	if (!client) {
		pr_debug("%s: Session %d already freed\n", __func__, sid);
		return 0;
	}

	switch (data->opcode) {
	case LSM_SESSION_CMDRSP_SHARED_MEM_MAP_REGIONS:
		if (atomic_read(&client->cmd_state) == CMD_STATE_WAIT_RESP) {
			spin_lock_irqsave(&mmap_lock, flags);
			*mmap_handle_p = command;
			/* spin_unlock_irqrestore implies barrier */
			spin_unlock_irqrestore(&mmap_lock, flags);
			atomic_set(&client->cmd_state, CMD_STATE_CLEARED);
			wake_up(&client->cmd_wait);
		}
		break;
	case APR_BASIC_RSP_RESULT:
		if (command == LSM_SESSION_CMD_SHARED_MEM_UNMAP_REGIONS) {
			atomic_set(&client->cmd_state, CMD_STATE_CLEARED);
			wake_up(&client->cmd_wait);
		} else {
			pr_warn("%s: Unexpected command 0x%x\n", __func__,
				command);
		}
		break;
	default:
		pr_debug("%s: command 0x%x return code 0x%x\n",
			 __func__, command, retcode);
		break;
	}
	if (client->cb)
		client->cb(data->opcode, data->token,
			   data->payload, client->priv);
	return 0;
}

int q6lsm_snd_model_buf_alloc(struct lsm_client *client, size_t len)
{
	int rc = -EINVAL;
	struct acdb_cal_block lsm_cal;
	size_t pad_zero = 0, total_mem = 0;

	if (!client || len <= LSM_ALIGN_BOUNDARY)
		return rc;
	memset(&lsm_cal, 0, sizeof(lsm_cal));
	mutex_lock(&client->cmd_lock);
	get_lsm_cal(&lsm_cal);
	pr_debug("%s:Snd Model len = %d cal size %d", __func__,
			 len, lsm_cal.cal_size);
	if (!lsm_cal.cal_paddr) {
		pr_err("%s: No LSM calibration set for session", __func__);
		mutex_unlock(&client->cmd_lock);
		return -EINVAL;
	}
	if (!client->sound_model.data) {
		client->sound_model.size = len;
		pad_zero = (LSM_ALIGN_BOUNDARY -
			   (len % LSM_ALIGN_BOUNDARY));
		total_mem = pad_zero + len + lsm_cal.cal_size;
		pr_debug("%s: Pad zeros sound model %d Total mem %d\n",
				 __func__, pad_zero, total_mem);
		client->sound_model.client =
		    msm_ion_client_create(UINT_MAX, "lsm_client");
		if (IS_ERR_OR_NULL(client->sound_model.client)) {
			pr_err("%s: ION create client for AUDIO failed\n",
			       __func__);
			goto fail;
		}
		client->sound_model.handle =
		ion_alloc(client->sound_model.client,
			  total_mem, SZ_4K, (0x1 << ION_AUDIO_HEAP_ID), 0);
		if (IS_ERR_OR_NULL(client->sound_model.handle)) {
			pr_err("%s: ION memory allocation for AUDIO failed\n",
			       __func__);
			goto fail;
		}

		rc = ion_phys(client->sound_model.client,
			      client->sound_model.handle,
			      (ion_phys_addr_t *)&client->sound_model.phys,
			      (size_t *)&len);
		if (rc) {
			pr_err("%s: ION get physical mem failed, rc%d\n",
			       __func__, rc);
			goto fail;
		}

		client->sound_model.data =
		    ion_map_kernel(client->sound_model.client,
				   client->sound_model.handle);
		if (IS_ERR_OR_NULL(client->sound_model.data)) {
			pr_err("%s: ION memory mapping failed\n", __func__);
			goto fail;
		}
		memset(client->sound_model.data, 0, len);
		client->lsm_cal_phy_addr = (pad_zero +
					    client->sound_model.phys +
					    client->sound_model.size);
		client->lsm_cal_size = lsm_cal.cal_size;
		memcpy((client->sound_model.data + pad_zero +
			client->sound_model.size),
			(uint32_t *)lsm_cal.cal_kvaddr, lsm_cal.cal_size);
	} else {
		rc = -EBUSY;
		goto fail;
	}
	mutex_unlock(&client->cmd_lock);

	rc = q6lsm_memory_map_regions(client, client->sound_model.phys,
				      len,
				      &client->sound_model.mem_map_handle);
	if (rc < 0) {
		pr_err("%s:CMD Memory_map_regions failed\n", __func__);
		goto exit;
	}

	return 0;
fail:
	mutex_unlock(&client->cmd_lock);
exit:
	q6lsm_snd_model_buf_free(client);
	return rc;
}

static int q6lsm_cmd(struct lsm_client *client, int opcode, bool wait)
{
	struct apr_hdr hdr;
	int rc;

	pr_debug("%s: enter opcode %d wait %d\n", __func__, opcode, wait);
	q6lsm_add_hdr(client, &hdr, sizeof(hdr), true);
	switch (opcode) {
	case LSM_SESSION_CMD_START:
	case LSM_SESSION_CMD_STOP:
	case LSM_SESSION_CMD_CLOSE_TX:
		hdr.opcode = opcode;
		break;
	default:
		pr_err("%s: Invalid opcode %d\n", __func__, opcode);
		return -EINVAL;
	}
	rc = q6lsm_apr_send_pkt(client, client->apr, &hdr, wait, NULL);
	if (rc)
		pr_err("%s: Failed commmand 0x%x\n", __func__, hdr.opcode);

	pr_debug("%s: leave %d\n", __func__, rc);
	return rc;
}

int q6lsm_start(struct lsm_client *client, bool wait)
{
	return q6lsm_cmd(client, LSM_SESSION_CMD_START, wait);
}

int q6lsm_stop(struct lsm_client *client, bool wait)
{
	return q6lsm_cmd(client, LSM_SESSION_CMD_STOP, wait);
}

int q6lsm_close(struct lsm_client *client)
{
	return q6lsm_cmd(client, LSM_SESSION_CMD_CLOSE_TX, true);
}

static int __init q6lsm_init(void)
{
	int i = 0;
	pr_debug("%s\n", __func__);
	spin_lock_init(&lsm_session_lock);
	spin_lock_init(&mmap_lock);
	mutex_init(&lsm_common.apr_lock);
	for (; i <= LSM_MAX_SESSION_ID; i++) {
		lsm_common.common_client[i].session = LSM_CONTROL_SESSION;
		init_waitqueue_head(&lsm_common.common_client[i].cmd_wait);
		mutex_init(&lsm_common.common_client[i].cmd_lock);
		atomic_set(&lsm_common.common_client[i].cmd_state,
			   CMD_STATE_CLEARED);
	}
	return 0;
}

device_initcall(q6lsm_init);
