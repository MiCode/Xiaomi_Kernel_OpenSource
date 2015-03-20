/*
 * Copyright (c) 2013-2015, Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/time.h>
#include <linux/atomic.h>
#include <sound/apr_audio-v2.h>
#include <sound/lsm_params.h>
#include <sound/q6core.h>
#include <sound/q6lsm.h>
#include <asm/ioctls.h>
#include <linux/memory.h>
#include <linux/msm_audio_ion.h>
#include <sound/q6afe-v2.h>
#include <sound/audio_cal_utils.h>

#define APR_TIMEOUT	(5 * HZ)
#define LSM_ALIGN_BOUNDARY 512
#define LSM_SAMPLE_RATE 16000
#define QLSM_PARAM_ID_MINOR_VERSION 1
static int lsm_afe_port;

enum {
	LSM_CUSTOM_TOP_IDX,
	LSM_TOP_IDX,
	LSM_CAL_IDX,
	LSM_MAX_CAL_IDX
};

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

	int set_custom_topology;
	struct cal_type_data	*cal_data[LSM_MAX_CAL_IDX];

	struct mutex apr_lock;
};

struct lsm_module_param_ids {
	uint32_t module_id;
	uint32_t param_id;
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
static int q6lsm_send_cal(struct lsm_client *client, u32 set_params_opcode);
static int q6lsm_memory_map_regions(struct lsm_client *client,
				    dma_addr_t dma_addr_p, uint32_t dma_buf_sz,
				    uint32_t *mmap_p);
static int q6lsm_memory_unmap_regions(struct lsm_client *client,
				      uint32_t handle);

static void q6lsm_set_param_hdr_info(
		struct lsm_set_params_hdr *param_hdr,
		u32 payload_size, u32 addr_lsw, u32 addr_msw,
		u32 mmap_handle)
{
	param_hdr->data_payload_size = payload_size;
	param_hdr->data_payload_addr_lsw = addr_lsw;
	param_hdr->data_payload_addr_msw = addr_msw;
	param_hdr->mem_map_handle = mmap_handle;
}

static void q6lsm_set_param_common(
		struct lsm_param_payload_common *common,
		struct lsm_module_param_ids *ids,
		u32 param_size, u32 set_param_version)
{
	common->module_id = ids->module_id;
	common->param_id = ids->param_id;

	switch (set_param_version) {
	case LSM_SESSION_CMD_SET_PARAMS_V2:
		common->p_size.param_size = param_size;
		break;
	case LSM_SESSION_CMD_SET_PARAMS:
	default:
		common->p_size.sr.param_size =
			(u16) param_size;
		common->p_size.sr.reserved = 0;
		break;
	}
}

static int q6lsm_callback(struct apr_client_data *data, void *priv)
{
	struct lsm_client *client = (struct lsm_client *)priv;
	uint32_t token;
	uint32_t *payload;

	if (!client || !data) {
		pr_err("%s: client %p data %p\n",
			__func__, client, data);
		WARN_ON(1);
		return -EINVAL;
	}

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: SSR event received 0x%x, event 0x%x, proc 0x%x\n",
			 __func__, data->opcode, data->reset_event,
			 data->reset_proc);

		cal_utils_clear_cal_block_q6maps(LSM_MAX_CAL_IDX,
			lsm_common.cal_data);
		mutex_lock(&lsm_common.cal_data[LSM_CUSTOM_TOP_IDX]->lock);
		lsm_common.set_custom_topology = 1;
		mutex_unlock(&lsm_common.cal_data[LSM_CUSTOM_TOP_IDX]->lock);
		return 0;
	}

	payload = data->payload;
	pr_debug("%s: Session %d opcode 0x%x token 0x%x payload size %d\n"
			 "payload [0] = 0x%x\n", __func__, client->session,
		data->opcode, data->token, data->payload_size, payload[0]);
	if (data->opcode == LSM_DATA_EVENT_READ_DONE) {
		struct lsm_cmd_read_done read_done;
		token = data->token;
		if (data->payload_size > sizeof(read_done)) {
			pr_err("%s: read done error payload size %d expected size %zd\n",
				__func__, data->payload_size,
				sizeof(read_done));
			return -EINVAL;
		}
		pr_debug("%s: opcode %x status %x lsw %x msw %x mem_map handle %x\n",
			__func__, data->opcode, payload[0], payload[1],
			payload[2], payload[3]);
		read_done.status = payload[0];
		read_done.buf_addr_lsw = payload[1];
		read_done.buf_addr_msw = payload[2];
		read_done.mem_map_handle = payload[3];
		read_done.total_size = payload[4];
		read_done.offset = payload[5];
		if (client->cb)
			client->cb(data->opcode, data->token,
					(void *)&read_done,
					client->priv);
		return 0;
	} else if (data->opcode == APR_BASIC_RSP_RESULT) {
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
		case LSM_SESSION_CMD_EOB:
		case LSM_SESSION_CMD_READ:
		case LSM_SESSION_CMD_OPEN_TX_V2:
		case LSM_CMD_ADD_TOPOLOGIES:
		case LSM_SESSION_CMD_SET_PARAMS_V2:
			if (token != client->session &&
			    payload[0] !=
				LSM_SESSION_CMD_DEREGISTER_SOUND_MODEL) {
				pr_err("%s: Invalid session %d receivced expected %d\n",
					__func__, token, client->session);
				return -EINVAL;
			}
			client->cmd_err_code = payload[1];
			if (client->cmd_err_code)
				pr_err("%s: cmd 0x%x failed status %d\n",
				__func__, payload[0], client->cmd_err_code);
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
			pr_debug("%s: Unable to register APR LSM common port\n",
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
			pr_debug("%s: APR De-Register common port\n", __func__);
		}
	}
	return 0;
}

struct lsm_client *q6lsm_client_alloc(lsm_app_cb cb, void *priv)
{
	struct lsm_client *client;
	int n;

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
	if (CHECK_SESSION(client->session)) {
		pr_err("%s: Client session %d\n",
			__func__, client->session);
		kfree(client);
		return NULL;
	}
	pr_debug("%s: Client Session %d\n", __func__, client->session);
	client->apr = apr_register("ADSP", "LSM", q6lsm_callback,
				   ((client->session) << 8 | client->session),
				   client);

	if (client->apr == NULL) {
		pr_err("%s: Registration with APR failed\n", __func__);
		goto fail;
	}

	pr_debug("%s: Registering the common port with APR\n", __func__);
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
		pr_err("%s: Invalid Session %d\n", __func__, client->session);
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
	struct apr_hdr *msg_hdr = (struct apr_hdr *) data;

	pr_debug("%s: enter wait %d\n", __func__, wait);
	if (wait)
		mutex_lock(&lsm_common.apr_lock);
	if (mmap_p) {
		WARN_ON(!wait);
		spin_lock_irqsave(&mmap_lock, flags);
		mmap_handle_p = mmap_p;
	}
	atomic_set(&client->cmd_state, CMD_STATE_WAIT_RESP);
	client->cmd_err_code = 0;
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
		if (likely(ret)) {
			/* q6 returned error */
			if (client->cmd_err_code)
				ret = -EINVAL;
			else
				ret = 0;
		} else {
			pr_err("%s: wait timedout, apr_opcode = 0x%x, size = %d\n",
				__func__, msg_hdr->opcode, msg_hdr->pkt_size);
			/* ret = 0 means wait timed out */
			ret = -ETIMEDOUT;
		}
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


static int q6lsm_send_custom_topologies(struct lsm_client *client)
{
	int rc;
	struct cal_block_data *cal_block = NULL;
	struct lsm_custom_topologies cstm_top;

	if (lsm_common.cal_data[LSM_CUSTOM_TOP_IDX] == NULL) {
		pr_err("%s: LSM_CUSTOM_TOP_IDX invalid\n", __func__);
		rc = -EINVAL;
		goto done;
	}

	lsm_common.set_custom_topology = 0;

	mutex_lock(&lsm_common.cal_data[LSM_CUSTOM_TOP_IDX]->lock);
	cal_block = cal_utils_get_only_cal_block(
			lsm_common.cal_data[LSM_CUSTOM_TOP_IDX]);
	if (!cal_block) {
		pr_err("%s: Cal block for LSM_CUSTOM_TOP_IDX not found\n",
			__func__);
		rc = -EINVAL;
		goto unlock;
	}

	if (cal_block->cal_data.size <= 0) {
		pr_err("%s: Invalid size for LSM_CUSTOM_TOP %zd\n",
			__func__, cal_block->cal_data.size);
		rc = -EINVAL;
		goto unlock;
	}

	memset(&cstm_top, 0, sizeof(cstm_top));
	/* Map the memory for out-of-band data */
	rc = q6lsm_memory_map_regions(client, cal_block->cal_data.paddr,
				      cal_block->map_data.map_size,
				      &cal_block->map_data.q6map_handle);
	if (rc < 0) {
		pr_err("%s: Failed to map custom topologied, err = %d\n",
			__func__, rc);
		goto unlock;
	}

	q6lsm_add_hdr(client, &cstm_top.hdr,
		      sizeof(cstm_top), true);
	cstm_top.hdr.opcode = LSM_CMD_ADD_TOPOLOGIES;

	/*
	 * For ADD_TOPOLOGIES, the dest_port should be 0
	 * Note that source port cannot be zero as it is used
	 * to route the response to a specific client registered
	 * on APR
	 */
	cstm_top.hdr.dest_port = 0;

	cstm_top.data_payload_addr_lsw =
			lower_32_bits(cal_block->cal_data.paddr);
	cstm_top.data_payload_addr_msw =
			upper_32_bits(cal_block->cal_data.paddr);
	cstm_top.mem_map_handle = cal_block->map_data.q6map_handle;
	cstm_top.buffer_size = cal_block->cal_data.size;

	rc = q6lsm_apr_send_pkt(client, client->apr,
				&cstm_top, true, NULL);
	if (rc)
		pr_err("%s: Failed to add custom top, err = %d\n",
			__func__, rc);
	/* go ahead and unmap even if custom top failed */
	rc = q6lsm_memory_unmap_regions(client,
					cal_block->map_data.q6map_handle);
	if (rc) {
		pr_err("%s: Failed to unmap, err = %d\n",
			__func__, rc);
		/* Even if mem unmap failed, treat the cmd as success */
		rc = 0;
	}

unlock:
	mutex_unlock(&lsm_common.cal_data[LSM_CUSTOM_TOP_IDX]->lock);
done:
	return rc;
}

static int q6lsm_do_open_v2(struct lsm_client *client,
		uint16_t app_id)
{
	int rc;
	struct cal_block_data *cal_block = NULL;
	struct audio_cal_info_lsm_top *lsm_top;
	struct lsm_stream_cmd_open_tx_v2 open_v2;

	if (lsm_common.cal_data[LSM_TOP_IDX] == NULL) {
		pr_err("%s: LSM_TOP_IDX invalid\n", __func__);
		rc = -EINVAL;
		goto done;
	}

	mutex_lock(&lsm_common.cal_data[LSM_TOP_IDX]->lock);
	cal_block = cal_utils_get_only_cal_block(
			lsm_common.cal_data[LSM_TOP_IDX]);
	if (!cal_block) {
		pr_err("%s: Cal block for LSM_TOP_IDX not found\n",
			__func__);
		rc = -EINVAL;
		goto unlock;
	}

	lsm_top = (struct audio_cal_info_lsm_top *)
			cal_block->cal_info;
	if (!lsm_top) {
		pr_err("%s: cal_info for LSM_TOP_IDX not found\n",
			__func__);
		rc = -EINVAL;
		goto unlock;
	}

	pr_debug("%s: topology_id = 0x%x, acdb_id = 0x%x, app_type = 0x%x\n",
		 __func__, lsm_top->topology, lsm_top->acdb_id,
		 lsm_top->app_type);

	if (lsm_top->topology == 0) {
		pr_err("%s: toplogy id not sent for app_type 0x%x\n",
			__func__, lsm_top->app_type);
		rc = -EINVAL;
		goto unlock;
	}

	client->app_id = lsm_top->app_type;
	memset(&open_v2, 0, sizeof(open_v2));
	q6lsm_add_hdr(client, &open_v2.hdr,
		      sizeof(open_v2), true);
	open_v2.topology_id = lsm_top->topology;
	open_v2.hdr.opcode = LSM_SESSION_CMD_OPEN_TX_V2;

	rc = q6lsm_apr_send_pkt(client, client->apr,
				&open_v2, true, NULL);
	if (rc)
		pr_err("%s: open_v2 failed, err = %d\n",
			__func__, rc);
	else
		client->use_topology = true;
unlock:
	mutex_unlock(&lsm_common.cal_data[LSM_TOP_IDX]->lock);
done:
	return rc;

}

void q6lsm_sm_set_param_data(struct lsm_client *client,
		struct lsm_params_info *p_info,
		size_t *offset)
{
	struct lsm_param_payload_common *param;

	param = (struct lsm_param_payload_common *)
			client->sound_model.data;
	param->module_id = p_info->module_id;
	param->param_id = p_info->param_id;
	param->p_size.param_size = client->sound_model.size;
	*offset = sizeof(*param);
}

int q6lsm_open(struct lsm_client *client, uint16_t app_id)
{
	int rc = 0;
	struct lsm_stream_cmd_open_tx open;

	/* Add Custom topologies if needed */
	if (lsm_common.set_custom_topology) {
		rc = q6lsm_send_custom_topologies(client);
		if (rc)
			pr_err("%s: Failed to send cust_top, err = %d\n",
				__func__, rc);
	}

	/* Try to open with topology first */
	rc = q6lsm_do_open_v2(client, app_id);
	if (!rc)
		/* open_v2 was successful */
		goto done;

	pr_debug("%s: try without topology\n",
		 __func__);

	memset(&open, 0, sizeof(open));
	q6lsm_add_hdr(client, &open.hdr, sizeof(open), true);
	switch (client->app_id) {
	case LSM_VOICE_WAKEUP_APP_ID_V2:
		open.app_id = client->app_id;
		break;
	default:
		pr_err("%s:  default err 0x%x\n", __func__, client->app_id);
		rc = -EINVAL;
		break;
	}

	open.sampling_rate = LSM_SAMPLE_RATE;
	open.hdr.opcode = LSM_SESSION_CMD_OPEN_TX;
	rc = q6lsm_apr_send_pkt(client, client->apr,
				&open, true, NULL);
	if (rc)
		pr_err("%s: Open failed opcode 0x%x, rc %d\n",
		       __func__, open.hdr.opcode, rc);
	else
		client->use_topology = false;
done:
	pr_debug("%s: leave %d\n", __func__, rc);
	return rc;
}

static int q6lsm_send_confidence_levels(
		struct lsm_client *client,
		struct lsm_module_param_ids *ids,
		u32 set_param_opcode)
{
	u8 *packet;
	size_t pkt_size;
	struct lsm_cmd_set_params_conf *conf_params;
	struct apr_hdr *msg_hdr;
	struct lsm_param_min_confidence_levels *cfl;
	uint8_t i = 0;
	uint8_t padd_size = 0;
	u8 *conf_levels;
	int rc;
	u32 payload_size, param_size;

	padd_size = (4 - (client->num_confidence_levels % 4)) - 1;
	pkt_size = sizeof(*conf_params) + padd_size +
		   client->num_confidence_levels;

	packet = kzalloc(pkt_size, GFP_KERNEL);
	if (!packet) {
		pr_err("%s: no memory for confidence level, size = %zd\n",
			__func__, pkt_size);
		return -ENOMEM;
	}

	conf_params = (struct lsm_cmd_set_params_conf *) packet;
	conf_levels = (u8 *) (packet + sizeof(*conf_params));
	msg_hdr = &conf_params->msg_hdr;
	q6lsm_add_hdr(client, msg_hdr,
		      pkt_size, true);
	msg_hdr->opcode = set_param_opcode;
	payload_size = pkt_size - sizeof(*msg_hdr) -
		       sizeof(conf_params->params_hdr);
	q6lsm_set_param_hdr_info(&conf_params->params_hdr,
				 payload_size, 0, 0, 0);
	cfl = &conf_params->conf_payload;
	param_size = ((sizeof(uint8_t) + padd_size +
		       client->num_confidence_levels)) *
		      sizeof(uint8_t);
	q6lsm_set_param_common(&cfl->common, ids,
			       param_size, set_param_opcode);
	cfl->num_confidence_levels = client->num_confidence_levels;

	pr_debug("%s: CMD PARAM SIZE = %d\n",
		 __func__, param_size);
	pr_debug("%s: Num conf_level = %d\n",
		 __func__, client->num_confidence_levels);

	memcpy(conf_levels, client->confidence_levels,
	       client->num_confidence_levels);
	for (i = 0; i < client->num_confidence_levels; i++)
		pr_debug("%s: Confidence_level[%d] = %d\n",
			 __func__, i, conf_levels[i]);

	rc = q6lsm_apr_send_pkt(client, client->apr,
				packet, true, NULL);
	if (rc)
		pr_err("%s: confidence_levels cmd failed, err = %d\n",
			__func__, rc);
	kfree(packet);
	return rc;
}

static int q6lsm_send_params(struct lsm_client *client,
		struct lsm_module_param_ids *opmode_ids,
		struct lsm_module_param_ids *connectport_ids,
		u32 set_param_opcode)
{
	int rc;
	struct lsm_cmd_set_opmode_connectport opmode_connectport;
	struct apr_hdr  *msg_hdr;
	struct lsm_param_connect_to_port *connect_to_port;
	struct lsm_param_op_mode *op_mode;
	u32 data_payload_size, param_size;

	msg_hdr = &opmode_connectport.msg_hdr;
	q6lsm_add_hdr(client, msg_hdr,
		      sizeof(opmode_connectport), true);
	msg_hdr->opcode = set_param_opcode;
	data_payload_size = sizeof(opmode_connectport) -
			    sizeof(*msg_hdr) -
			    sizeof(opmode_connectport.params_hdr);
	q6lsm_set_param_hdr_info(&opmode_connectport.params_hdr,
				 data_payload_size, 0, 0, 0);
	connect_to_port = &opmode_connectport.connect_to_port;
	op_mode = &opmode_connectport.op_mode;

	param_size = sizeof(struct lsm_param_op_mode) -
		     sizeof(op_mode->common);
	q6lsm_set_param_common(&op_mode->common,
			       opmode_ids, param_size,
			       set_param_opcode);
	op_mode->minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	op_mode->mode = client->mode;
	op_mode->reserved = 0;
	pr_debug("%s: mode = 0x%x", __func__, op_mode->mode);

	param_size = (sizeof(struct lsm_param_connect_to_port) -
		      sizeof(connect_to_port->common));
	q6lsm_set_param_common(&connect_to_port->common,
			       connectport_ids, param_size,
			       set_param_opcode);
	connect_to_port->minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	connect_to_port->port_id = client->connect_to_port;
	connect_to_port->reserved = 0;
	pr_debug("%s: port= %d", __func__, connect_to_port->port_id);

	rc = q6lsm_apr_send_pkt(client, client->apr,
				&opmode_connectport, true, NULL);
	if (rc)
		pr_err("%s: Failed set_params opcode 0x%x, rc %d\n",
		       __func__, msg_hdr->opcode, rc);

	pr_debug("%s: leave %d\n", __func__, rc);
	return rc;
}

void set_lsm_port(int lsm_port)
{
	lsm_afe_port = lsm_port;
}

int get_lsm_port()
{
	return lsm_afe_port;
}

int q6lsm_set_data(struct lsm_client *client,
			   enum lsm_detection_mode mode,
			   bool detectfailure)
{
	int rc = 0;
	struct lsm_module_param_ids opmode_ids, connectport_ids;
	struct lsm_module_param_ids conf_levels_ids;

	if (!client->confidence_levels) {
		/*
		 * It is possible that confidence levels are
		 * not provided. This is not a error condition.
		 * Return gracefully without any error
		 */
		pr_debug("%s: no conf levels to set\n",
			__func__);
		return rc;
	}

	if (mode == LSM_MODE_KEYWORD_ONLY_DETECTION) {
		client->mode = 0x01;
	} else if (mode == LSM_MODE_USER_KEYWORD_DETECTION) {
		client->mode = 0x03;
	} else {
		pr_err("%s: Incorrect detection mode %d\n", __func__, mode);
		rc = -EINVAL;
		goto err_ret;
	}
	client->mode |= detectfailure << 2;
	client->connect_to_port = get_lsm_port();

	opmode_ids.module_id = LSM_MODULE_ID_VOICE_WAKEUP;
	opmode_ids.param_id = LSM_PARAM_ID_OPERATION_MODE;

	connectport_ids.module_id = LSM_MODULE_ID_VOICE_WAKEUP;
	connectport_ids.param_id = LSM_PARAM_ID_CONNECT_TO_PORT;

	rc = q6lsm_send_params(client, &opmode_ids, &connectport_ids,
			      LSM_SESSION_CMD_SET_PARAMS);
	if (rc) {
		pr_err("%s: Failed to set lsm config params %d\n",
			__func__, rc);
		goto err_ret;
	}

	conf_levels_ids.module_id = LSM_MODULE_ID_VOICE_WAKEUP;
	conf_levels_ids.param_id = LSM_PARAM_ID_MIN_CONFIDENCE_LEVELS;

	rc = q6lsm_send_confidence_levels(client, &conf_levels_ids,
					 LSM_SESSION_CMD_SET_PARAMS);
	if (rc) {
		pr_err("%s: Failed to send conf_levels, err = %d\n",
			__func__, rc);
		goto err_ret;
	}

	rc = q6lsm_send_cal(client, LSM_SESSION_CMD_SET_PARAMS);
	if (rc) {
		pr_err("%s: Failed to send calibration data %d\n",
			__func__, rc);
		goto err_ret;
	}

err_ret:
	return rc;
}

int q6lsm_register_sound_model(struct lsm_client *client,
			       enum lsm_detection_mode mode,
			       bool detectfailure)
{
	int rc;
	struct lsm_cmd_reg_snd_model cmd;

	memset(&cmd, 0, sizeof(cmd));
	rc = q6lsm_set_data(client, mode, detectfailure);
	if (rc) {
		pr_err("%s: Failed to set lsm data, err = %d\n",
			__func__, rc);
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

	pr_debug("%s: addr %pa, size %d, handle 0x%x\n", __func__,
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

	if (!client) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	if (!client->apr) {
		pr_err("APR client handle NULL\n");
		return -EINVAL;
	}

	if (CHECK_SESSION(client->session)) {
		pr_err("%s: session[%d]", __func__, client->session);
		return -EINVAL;
	}

	memset(&cmd, 0, sizeof(cmd));
	q6lsm_add_hdr(client, &cmd.hdr, sizeof(cmd.hdr), false);
	cmd.hdr.opcode = LSM_SESSION_CMD_DEREGISTER_SOUND_MODEL;

	rc = q6lsm_apr_send_pkt(client, client->apr, &cmd.hdr, true, NULL);
	if (rc) {
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
	pr_debug("%s: pkt size=%d cmd_flg=%d session=%d\n", __func__, pkt_size,
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
	if (CHECK_SESSION(client->session)) {
		pr_err("%s: session[%d]", __func__, client->session);
		return -EINVAL;
	}
	cmd_size = sizeof(struct avs_cmd_shared_mem_map_regions) +
		   sizeof(struct avs_shared_map_region_payload);

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!mmap_region_cmd) {
		pr_err("%s: memory allocation failed\n", __func__);
		return -ENOMEM;
	}

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
	if (CHECK_SESSION(client->session)) {
		pr_err("%s: session[%d]", __func__, client->session);
		return -EINVAL;
	}
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

static int q6lsm_send_cal(struct lsm_client *client,
			  u32 set_params_opcode)
{
	int rc = 0;
	struct lsm_cmd_set_params params;
	struct lsm_set_params_hdr *params_hdr = &params.param_hdr;
	struct apr_hdr *msg_hdr = &params.msg_hdr;
	struct cal_block_data *cal_block = NULL;

	pr_debug("%s: Session id %d\n", __func__, client->session);
	if (CHECK_SESSION(client->session)) {
		pr_err("%s: session[%d]", __func__, client->session);
		return -EINVAL;
	}

	if (lsm_common.cal_data[LSM_CAL_IDX] == NULL)
		goto done;

	mutex_lock(&lsm_common.cal_data[LSM_CAL_IDX]->lock);
	cal_block = cal_utils_get_only_cal_block(
		lsm_common.cal_data[LSM_CAL_IDX]);
	if (cal_block == NULL)
		goto unlock;

	if (cal_block->cal_data.size <= 0) {
		pr_debug("%s: No cal to send!\n", __func__);
		rc = -EINVAL;
		goto unlock;
	}
	if (cal_block->cal_data.size != client->lsm_cal_size) {
		pr_err("%s: Cal size %zd doesn't match lsm cal size %d\n",
			__func__, cal_block->cal_data.size,
			client->lsm_cal_size);
		rc = -EINVAL;
		goto unlock;
	}
	/* Cache mmap address, only map once or if new addr */
	lsm_common.common_client[client->session].session = client->session;
	q6lsm_add_hdr(client, msg_hdr, sizeof(params), true);
	msg_hdr->opcode = set_params_opcode;
	q6lsm_set_param_hdr_info(params_hdr,
			cal_block->cal_data.size,
			lower_32_bits(client->lsm_cal_phy_addr),
			upper_32_bits(client->lsm_cal_phy_addr),
			client->sound_model.mem_map_handle);

	pr_debug("%s: Cal Size = %zd", __func__,
		cal_block->cal_data.size);
	rc = q6lsm_apr_send_pkt(client, client->apr, &params, true, NULL);
	if (rc)
		pr_err("%s: Failed set_params opcode 0x%x, rc %d\n",
		       __func__, msg_hdr->opcode, rc);
unlock:
	mutex_unlock(&lsm_common.cal_data[LSM_CAL_IDX]->lock);
done:
	return rc;
}


int q6lsm_snd_model_buf_free(struct lsm_client *client)
{
	int rc;

	pr_debug("%s: Session id %d\n", __func__, client->session);
	if (CHECK_SESSION(client->session)) {
		pr_err("%s: session[%d]", __func__, client->session);
		return -EINVAL;
	}

	mutex_lock(&client->cmd_lock);
	rc = q6lsm_memory_unmap_regions(client,
					client->sound_model.mem_map_handle);
	if (rc)
		pr_err("%s: CMD Memory_unmap_regions failed %d\n",
			__func__, rc);

	if (client->sound_model.data) {
		msm_audio_ion_free(client->sound_model.client,
				 client->sound_model.handle);
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

	spin_lock_irqsave(&lsm_session_lock, flags);
	if (session_id < LSM_MIN_SESSION_ID || session_id > LSM_MAX_SESSION_ID)
		pr_err("%s: Invalid session %d\n", __func__, session_id);
	else if (!lsm_session[session_id])
		pr_err("%s: Not an active session %d\n", __func__, session_id);
	else
		client = lsm_session[session_id];
	spin_unlock_irqrestore(&lsm_session_lock, flags);
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
		cal_utils_clear_cal_block_q6maps(LSM_MAX_CAL_IDX,
			lsm_common.cal_data);
		lsm_common.set_custom_topology = 1;
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
		switch (command) {
		case LSM_SESSION_CMD_SHARED_MEM_UNMAP_REGIONS:
			atomic_set(&client->cmd_state, CMD_STATE_CLEARED);
			wake_up(&client->cmd_wait);
			break;
		case LSM_SESSION_CMD_SHARED_MEM_MAP_REGIONS:
			if (retcode != 0) {
				/* error state, signal to stop waiting */
				if (atomic_read(&client->cmd_state) ==
					CMD_STATE_WAIT_RESP) {
					spin_lock_irqsave(&mmap_lock, flags);
					/* implies barrier */
					spin_unlock_irqrestore(&mmap_lock,
						flags);
					atomic_set(&client->cmd_state,
						CMD_STATE_CLEARED);
					wake_up(&client->cmd_wait);
				}
			}
		break;
		default:
			pr_warn("%s: Unexpected command 0x%x\n", __func__,
				command);
		}
	default:
		pr_debug("%s: command 0x%x return code 0x%x opcode 0x%x\n",
			 __func__, command, retcode, data->opcode);
		break;
	}
	if (client->cb)
		client->cb(data->opcode, data->token,
			   data->payload, client->priv);
	return 0;
}

int q6lsm_snd_model_buf_alloc(struct lsm_client *client, size_t len,
			      bool allocate_module_data)
{
	int rc = -EINVAL;
	struct cal_block_data		*cal_block = NULL;

	size_t pad_zero = 0, total_mem = 0;

	if (!client || len <= LSM_ALIGN_BOUNDARY)
		return rc;

	mutex_lock(&client->cmd_lock);

	mutex_lock(&lsm_common.cal_data[LSM_CAL_IDX]->lock);
	cal_block = cal_utils_get_only_cal_block(
		lsm_common.cal_data[LSM_CAL_IDX]);
	if (cal_block == NULL)
		goto fail;

	pr_debug("%s:Snd Model len = %zd cal size %zd phys addr %pa", __func__,
		len, cal_block->cal_data.size,
		&cal_block->cal_data.paddr);
	if (!cal_block->cal_data.paddr) {
		pr_err("%s: No LSM calibration set for session", __func__);
		rc = -EINVAL;
		goto fail;
	}
	if (!client->sound_model.data) {

		/*
		 * if sound module is sent as set_param
		 * Then memory needs to be allocated for
		 * set_param payload as well.
		 */
		if (allocate_module_data)
			len += sizeof(struct lsm_param_payload_common);

		client->sound_model.size = len;
		pad_zero = (LSM_ALIGN_BOUNDARY -
			    (len % LSM_ALIGN_BOUNDARY));
		if ((len > SIZE_MAX - pad_zero) ||
		    (len + pad_zero >
		     SIZE_MAX - cal_block->cal_data.size)) {
			pr_err("%s: invalid allocation size, len = %zd, pad_zero =%zd, cal_size = %zd\n",
				__func__, len, pad_zero,
				cal_block->cal_data.size);
			rc = -EINVAL;
			goto fail;
		}

		total_mem = PAGE_ALIGN(pad_zero + len +
			cal_block->cal_data.size);
		pr_debug("%s: Pad zeros sound model %zd Total mem %zd\n",
				 __func__, pad_zero, total_mem);
		rc = msm_audio_ion_alloc("lsm_client",
				&client->sound_model.client,
				&client->sound_model.handle,
				total_mem,
				&client->sound_model.phys,
				&len,
				&client->sound_model.data);
		if (rc) {
			pr_err("%s: Audio ION alloc is failed, rc = %d\n",
				__func__, rc);
			goto fail;
		}
	pr_debug("%s: Length = %zd\n", __func__, len);
	client->lsm_cal_phy_addr = (pad_zero +
				    client->sound_model.phys +
				    client->sound_model.size);
	client->lsm_cal_size = cal_block->cal_data.size;
	memcpy((client->sound_model.data + pad_zero +
		client->sound_model.size),
	       (uint32_t *)cal_block->cal_data.kvaddr, client->lsm_cal_size);
	pr_debug("%s: Copy cal start virt_addr %p phy_addr %pa\n"
			 "Offset cal virtual Addr %p\n", __func__,
			 client->sound_model.data, &client->sound_model.phys,
			 (pad_zero + client->sound_model.data +
			 client->sound_model.size));
	} else {
		pr_err("%s: sound model busy\n", __func__);
		rc = -EBUSY;
		goto fail;
	}
	mutex_unlock(&lsm_common.cal_data[LSM_CAL_IDX]->lock);
	mutex_unlock(&client->cmd_lock);

	rc = q6lsm_memory_map_regions(client, client->sound_model.phys,
				      len,
				      &client->sound_model.mem_map_handle);
	if (rc) {
		pr_err("%s: CMD Memory_map_regions failed %d\n", __func__, rc);
		goto exit;
	}

	return 0;
fail:
	mutex_unlock(&lsm_common.cal_data[LSM_CAL_IDX]->lock);
	mutex_unlock(&client->cmd_lock);
exit:
	q6lsm_snd_model_buf_free(client);
	return rc;
}

static int q6lsm_cmd(struct lsm_client *client, int opcode, bool wait)
{
	struct apr_hdr hdr;
	int rc;

	pr_debug("%s: enter opcode %x wait %d\n", __func__, opcode, wait);
	q6lsm_add_hdr(client, &hdr, sizeof(hdr), true);
	switch (opcode) {
	case LSM_SESSION_CMD_START:
	case LSM_SESSION_CMD_STOP:
	case LSM_SESSION_CMD_CLOSE_TX:
	case LSM_SESSION_CMD_EOB:
		hdr.opcode = opcode;
		break;
	default:
		pr_err("%s: Invalid opcode 0x%x\n", __func__, opcode);
		return -EINVAL;
	}
	rc = q6lsm_apr_send_pkt(client, client->apr, &hdr, wait, NULL);
	if (rc)
		pr_err("%s: Failed commmand 0x%x\n", __func__, hdr.opcode);

	pr_debug("%s: leave %d\n", __func__, rc);
	return rc;
}

static int q6lsm_send_param_epd_thres(
		struct lsm_client *client,
		void *data, struct lsm_module_param_ids *ids)
{
	struct snd_lsm_ep_det_thres *ep_det_data;
	struct lsm_cmd_set_epd_threshold epd_cmd;
	struct apr_hdr *msg_hdr = &epd_cmd.msg_hdr;
	struct lsm_set_params_hdr *param_hdr =
			&epd_cmd.param_hdr;
	struct lsm_param_epd_thres *epd_thres =
			&epd_cmd.epd_thres;
	int rc;

	ep_det_data = (struct snd_lsm_ep_det_thres *) data;
	q6lsm_add_hdr(client, msg_hdr,
		      sizeof(epd_cmd), true);
	msg_hdr->opcode = LSM_SESSION_CMD_SET_PARAMS_V2;
	q6lsm_set_param_hdr_info(param_hdr,
		sizeof(*epd_thres), 0, 0, 0);
	q6lsm_set_param_common(&epd_thres->common, ids,
		sizeof(*epd_thres) - sizeof(epd_thres->common),
		LSM_SESSION_CMD_SET_PARAMS_V2);
	epd_thres->minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	epd_thres->epd_begin = ep_det_data->epd_begin;
	epd_thres->epd_end = ep_det_data->epd_end;

	rc = q6lsm_apr_send_pkt(client, client->apr,
				&epd_cmd, true, NULL);
	if (unlikely(rc))
		pr_err("%s: EPD_THRESHOLD failed, rc %d\n",
			__func__, rc);
	return rc;
}

static int q6lsm_send_param_gain(
		struct lsm_client *client,
		u16 gain, struct lsm_module_param_ids *ids)
{
	struct lsm_cmd_set_gain lsm_cmd_gain;
	struct apr_hdr *msg_hdr = &lsm_cmd_gain.msg_hdr;
	struct lsm_param_gain *lsm_gain = &lsm_cmd_gain.lsm_gain;
	int rc;

	q6lsm_add_hdr(client, msg_hdr,
		      sizeof(lsm_cmd_gain), true);
	msg_hdr->opcode = LSM_SESSION_CMD_SET_PARAMS_V2;
	q6lsm_set_param_hdr_info(&lsm_cmd_gain.param_hdr,
			sizeof(*lsm_gain), 0, 0, 0);
	q6lsm_set_param_common(&lsm_gain->common, ids,
		sizeof(*lsm_gain) - sizeof(lsm_gain->common),
		LSM_SESSION_CMD_SET_PARAMS_V2);
	lsm_gain->minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	lsm_gain->gain = gain;
	lsm_gain->reserved = 0;

	rc = q6lsm_apr_send_pkt(client, client->apr,
				&lsm_cmd_gain, true, NULL);
	if (unlikely(rc))
		pr_err("%s: LSM_GAIN CMD send failed, rc %d\n",
			 __func__, rc);
	return rc;
}

int q6lsm_set_one_param(struct lsm_client *client,
	struct lsm_params_info *p_info, void *data,
	enum LSM_PARAM_TYPE param_type)
{
	int rc = 0, pkt_sz;
	struct lsm_module_param_ids ids;
	u8 *packet;

	memset(&ids, sizeof(ids), 0);
	switch (param_type) {
	case LSM_ENDPOINT_DETECT_THRESHOLD: {
		ids.module_id = p_info->module_id;
		ids.param_id = p_info->param_id;
		rc = q6lsm_send_param_epd_thres(client, data,
						&ids);
		break;
	}

	case LSM_OPERATION_MODE: {
		struct snd_lsm_detect_mode *det_mode = data;
		struct lsm_module_param_ids opmode_ids;
		struct lsm_module_param_ids connectport_ids;

		if (det_mode->mode == LSM_MODE_KEYWORD_ONLY_DETECTION) {
			client->mode = 0x01;
		} else if (det_mode->mode == LSM_MODE_USER_KEYWORD_DETECTION) {
			client->mode = 0x03;
		} else {
			pr_err("%s: Incorrect detection mode %d\n",
				__func__, det_mode->mode);
			return -EINVAL;
		}

		client->mode |= det_mode->detect_failure << 2;
		client->connect_to_port = get_lsm_port();

		opmode_ids.module_id = p_info->module_id;
		opmode_ids.param_id = p_info->param_id;

		connectport_ids.module_id = LSM_MODULE_ID_FRAMEWORK;
		connectport_ids.param_id = LSM_PARAM_ID_CONNECT_TO_PORT;

		rc = q6lsm_send_params(client, &opmode_ids, &connectport_ids,
				       LSM_SESSION_CMD_SET_PARAMS_V2);
		if (rc)
			pr_err("%s: OPERATION_MODE failed, rc %d\n",
				__func__, rc);
		break;
	}

	case LSM_GAIN: {
		struct snd_lsm_gain *lsm_gain = (struct snd_lsm_gain *) data;
		ids.module_id = p_info->module_id;
		ids.param_id = p_info->param_id;
		rc = q6lsm_send_param_gain(client, lsm_gain->gain, &ids);
		if (rc)
			pr_err("%s: LSM_GAIN command failed, rc %d\n",
				__func__, rc);
		break;
	}

	case LSM_MIN_CONFIDENCE_LEVELS:
		ids.module_id = p_info->module_id;
		ids.param_id = p_info->param_id;
		rc = q6lsm_send_confidence_levels(client, &ids,
				LSM_SESSION_CMD_SET_PARAMS_V2);
		if (rc)
			pr_err("%s: CONFIDENCE_LEVELS cmd failed, rc %d\n",
				 __func__, rc);
		break;
	case LSM_REG_SND_MODEL: {
		struct lsm_cmd_set_params model_param;
		u32 payload_size;

		memset(&model_param, 0, sizeof(model_param));
		q6lsm_add_hdr(client, &model_param.msg_hdr,
			      sizeof(model_param), true);
		model_param.msg_hdr.opcode = LSM_SESSION_CMD_SET_PARAMS_V2;
		payload_size = p_info->param_size +
			       sizeof(struct lsm_param_payload_common);
		q6lsm_set_param_hdr_info(&model_param.param_hdr,
				payload_size,
				lower_32_bits(client->sound_model.phys),
				upper_32_bits(client->sound_model.phys),
				client->sound_model.mem_map_handle);

		rc = q6lsm_apr_send_pkt(client, client->apr,
					&model_param, true, NULL);
		if (rc) {
			pr_err("%s: REG_SND_MODEL failed, rc %d\n",
				__func__, rc);
			return rc;
		}

		rc = q6lsm_send_cal(client, LSM_SESSION_CMD_SET_PARAMS);
		if (rc)
			pr_err("%s: Failed to send lsm cal, err = %d\n",
				__func__, rc);
		break;
	}

	case LSM_DEREG_SND_MODEL: {
		struct lsm_param_payload_common *common;
		struct lsm_cmd_set_params *param;

		pkt_sz = sizeof(*param) + sizeof(*common);
		packet = kzalloc(pkt_sz, GFP_KERNEL);
		if (!packet) {
			pr_err("%s: No memory for DEREG_SND_MODEL pkt, size = %d\n",
				__func__, pkt_sz);
			return -ENOMEM;
		}

		param = (struct lsm_cmd_set_params *) packet;
		common = (struct lsm_param_payload_common *)
				(packet + sizeof(*param));
		q6lsm_add_hdr(client, &param->msg_hdr, pkt_sz, true);
		param->msg_hdr.opcode = LSM_SESSION_CMD_SET_PARAMS_V2;
		q6lsm_set_param_hdr_info(&param->param_hdr,
					 sizeof(*common),
					 0, 0, 0);
		ids.module_id = p_info->module_id;
		ids.param_id = p_info->param_id;
		q6lsm_set_param_common(common, &ids, 0,
				       LSM_SESSION_CMD_SET_PARAMS_V2);
		rc = q6lsm_apr_send_pkt(client, client->apr,
					packet, true, NULL);
		if (rc)
			pr_err("%s: DEREG_SND_MODEL failed, rc %d\n",
				__func__, rc);
		kfree(packet);
		break;
	}

	case LSM_CUSTOM_PARAMS: {
		struct apr_hdr *hdr;
		u8 *custom_data;

		if (p_info->param_size <
		    sizeof(struct lsm_param_payload_common)) {
			pr_err("%s: Invalid param_size %d\n",
				__func__, p_info->param_size);
			return -EINVAL;
		}

		pkt_sz = p_info->param_size + sizeof(*hdr);
		packet = kzalloc(pkt_sz, GFP_KERNEL);
		if (!packet) {
			pr_err("%s: no memory for CUSTOM_PARAMS, size = %d\n",
				__func__, pkt_sz);
			return -ENOMEM;
		}

		hdr = (struct apr_hdr *) packet;
		custom_data = (u8 *) (packet + sizeof(*hdr));
		q6lsm_add_hdr(client, hdr, pkt_sz, true);
		hdr->opcode = LSM_SESSION_CMD_SET_PARAMS_V2;
		memcpy(custom_data, data, p_info->param_size);

		rc = q6lsm_apr_send_pkt(client, client->apr,
					packet, true, NULL);
		if (rc)
			pr_err("%s: CUSTOM_PARAMS failed, rc %d\n",
				__func__, rc);
		kfree(packet);
		break;
	}
	default:
		pr_err("%s: wrong param_type 0x%x\n",
			__func__, p_info->param_type);
	}

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

int q6lsm_lab_control(struct lsm_client *client, u32 enable)
{
	int rc = 0;
	struct lsm_params_lab_enable lab_enable;
	struct lsm_params_lab_config lab_config;
	struct lsm_module_param_ids lab_ids;
	u32 param_size;

	if (!client) {
		pr_err("%s: invalid param client %p\n", __func__, client);
		return -EINVAL;
	}
	/* enable/disable lab on dsp */
	q6lsm_add_hdr(client, &lab_enable.msg_hdr, sizeof(lab_enable), true);
	lab_enable.msg_hdr.opcode = LSM_SESSION_CMD_SET_PARAMS;
	q6lsm_set_param_hdr_info(&lab_enable.params_hdr,
				 sizeof(struct lsm_lab_enable),
				 0, 0, 0);
	param_size = (sizeof(struct lsm_lab_enable) -
		      sizeof(struct lsm_param_payload_common));
	lab_ids.module_id = LSM_MODULE_ID_LAB;
	lab_ids.param_id = LSM_PARAM_ID_LAB_ENABLE;
	q6lsm_set_param_common(&lab_enable.lab_enable.common,
				&lab_ids, param_size,
				LSM_SESSION_CMD_SET_PARAMS);
	lab_enable.lab_enable.enable = (enable) ? 1 : 0;
	rc = q6lsm_apr_send_pkt(client, client->apr, &lab_enable, true, NULL);
	if (rc) {
		pr_err("%s: Lab enable failed rc %d\n", __func__, rc);
		return rc;
	}
	if (!enable)
		goto exit;
	/* lab session is being enabled set the config values */
	q6lsm_add_hdr(client, &lab_config.msg_hdr, sizeof(lab_config), true);
	lab_config.msg_hdr.opcode = LSM_SESSION_CMD_SET_PARAMS;
	q6lsm_set_param_hdr_info(&lab_config.params_hdr,
				 sizeof(struct lsm_lab_config),
				 0, 0, 0);
	lab_ids.module_id = LSM_MODULE_ID_LAB;
	lab_ids.param_id = LSM_PARAM_ID_LAB_CONFIG;
	param_size = (sizeof(struct lsm_lab_config) -
		      sizeof(struct lsm_param_payload_common));
	q6lsm_set_param_common(&lab_config.lab_config.common,
			       &lab_ids, param_size,
			       LSM_SESSION_CMD_SET_PARAMS);
	lab_config.lab_config.minor_version = 1;
	lab_config.lab_config.wake_up_latency_ms = 250;
	rc = q6lsm_apr_send_pkt(client, client->apr, &lab_config, true, NULL);
	if (rc) {
		pr_err("%s: Lab config failed rc %d disable lab\n",
		 __func__, rc);
		/* Lab config failed disable lab */
		lab_enable.lab_enable.enable = 0;
		if (q6lsm_apr_send_pkt(client, client->apr,
			&lab_enable, true, NULL))
			pr_err("%s: Lab disable failed\n", __func__);
	}
exit:
	return rc;
}

int q6lsm_stop_lab(struct lsm_client *client)
{
	int rc = 0;
	if (!client) {
		pr_err("%s: invalid param client %p\n", __func__, client);
		return -EINVAL;
	}
	rc = q6lsm_cmd(client, LSM_SESSION_CMD_EOB, true);
	if (rc)
		pr_err("%s: Lab stop failed %d\n", __func__, rc);
	return rc;
}

int q6lsm_read(struct lsm_client *client, struct lsm_cmd_read *read)
{
	int rc = 0;
	if (!client || !read) {
		pr_err("%s: Invalid params client %p read %p\n", __func__,
			client, read);
		return -EINVAL;
	}
	pr_debug("%s: read call memmap handle %x address %x%x size %d\n",
		 __func__, read->mem_map_handle, read->buf_addr_msw,
		read->buf_addr_lsw, read->buf_size);
	q6lsm_add_hdr(client, &read->hdr, sizeof(struct lsm_cmd_read), true);
	read->hdr.opcode = LSM_SESSION_CMD_READ;
	rc = q6lsm_apr_send_pkt(client, client->apr, read, false, NULL);
	if (rc)
		pr_err("%s: read buffer call failed rc %d\n", __func__, rc);
	return rc;
}

int q6lsm_lab_buffer_alloc(struct lsm_client *client, bool alloc)
{
	int ret = 0, i = 0;
	size_t allocate_size = 0, len = 0;
	if (!client) {
		pr_err("%s: invalid client\n", __func__);
		return -EINVAL;
	}
	if (alloc) {
		if (client->lab_buffer) {
			pr_err("%s: buffers are allocated period count %d period size %d\n",
				__func__,
				client->hw_params.period_count,
				client->hw_params.buf_sz);
			return -EINVAL;
		}
		allocate_size = client->hw_params.period_count *
				client->hw_params.buf_sz;
		allocate_size = PAGE_ALIGN(allocate_size);
		client->lab_buffer =
			kzalloc(sizeof(struct lsm_lab_buffer) *
			client->hw_params.period_count, GFP_KERNEL);
		if (!client->lab_buffer) {
			pr_err("%s: memory allocation for lab buffer failed count %d\n"
				, __func__,
				client->hw_params.period_count);
			return -ENOMEM;
		}
		ret = msm_audio_ion_alloc("lsm_lab",
			&client->lab_buffer[0].client,
			&client->lab_buffer[0].handle,
			allocate_size, &client->lab_buffer[0].phys,
			&len,
			&client->lab_buffer[0].data);
		if (ret)
			pr_err("%s: ion alloc failed ret %d size %zd\n",
				__func__, ret, allocate_size);
		else {
			ret = q6lsm_memory_map_regions(client,
				client->lab_buffer[0].phys, len,
				&client->lab_buffer[0].mem_map_handle);
			if (ret) {
				pr_err("%s: memory map filed ret %d size %zd\n",
					__func__, ret, len);
				msm_audio_ion_free(
				client->lab_buffer[0].client,
				client->lab_buffer[0].handle);
			}
		}
		if (ret) {
			pr_err("%s: alloc lab buffer failed ret %d\n",
				__func__, ret);
			kfree(client->lab_buffer);
			client->lab_buffer = NULL;
		} else {
			pr_debug("%s: Memory map handle %x phys %pa size %d\n",
				__func__,
				client->lab_buffer[0].mem_map_handle,
				&client->lab_buffer[0].phys,
				client->hw_params.buf_sz);
			for (i = 0; i < client->hw_params.period_count; i++) {
				client->lab_buffer[i].phys =
				client->lab_buffer[0].phys +
				(i * client->hw_params.buf_sz);
				client->lab_buffer[i].size =
				client->hw_params.buf_sz;
				client->lab_buffer[i].data =
				(u8 *)(client->lab_buffer[0].data) +
				(i * client->hw_params.buf_sz);
				client->lab_buffer[i].mem_map_handle =
				client->lab_buffer[0].mem_map_handle;
			}
		}
	} else {
		ret = q6lsm_memory_unmap_regions(client,
			client->lab_buffer[0].mem_map_handle);
		if (!ret)
			msm_audio_ion_free(
			client->lab_buffer[0].client,
			client->lab_buffer[0].handle);
		else
			pr_err("%s: unmap failed not freeing memory\n",
			__func__);
		kfree(client->lab_buffer);
		client->lab_buffer = NULL;
	}
	return ret;
}

static int get_cal_type_index(int32_t cal_type)
{
	int ret = -EINVAL;

	switch (cal_type) {
	case LSM_CUST_TOPOLOGY_CAL_TYPE:
		ret = LSM_CUSTOM_TOP_IDX;
		break;
	case LSM_TOPOLOGY_CAL_TYPE:
		ret = LSM_TOP_IDX;
		break;
	case LSM_CAL_TYPE:
		ret = LSM_CAL_IDX;
		break;
	default:
		pr_err("%s: invalid cal type %d!\n", __func__, cal_type);
	}
	return ret;
}

static int q6lsm_alloc_cal(int32_t cal_type,
				size_t data_size, void *data)
{
	int				ret = 0;
	int				cal_index;
	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	ret = cal_utils_alloc_cal(data_size, data,
		lsm_common.cal_data[cal_index], 0, NULL);
	if (ret < 0) {
		pr_err("%s: cal_utils_alloc_block failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		ret = -EINVAL;
		goto done;
	}
done:
	return ret;
}

static int q6lsm_dealloc_cal(int32_t cal_type,
				size_t data_size, void *data)
{
	int				ret = 0;
	int				cal_index;
	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	ret = cal_utils_dealloc_cal(data_size, data,
		lsm_common.cal_data[cal_index]);
	if (ret < 0) {
		pr_err("%s: cal_utils_dealloc_block failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		ret = -EINVAL;
		goto done;
	}
done:
	return ret;
}

static int q6lsm_set_cal(int32_t cal_type,
			size_t data_size, void *data)
{
	int				ret = 0;
	int				cal_index;
	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	ret = cal_utils_set_cal(data_size, data,
		lsm_common.cal_data[cal_index], 0, NULL);
	if (ret < 0) {
		pr_err("%s: cal_utils_set_cal failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		ret = -EINVAL;
		goto done;
	}

	if (cal_index == LSM_CUSTOM_TOP_IDX) {
		mutex_lock(&lsm_common.cal_data[LSM_CUSTOM_TOP_IDX]->lock);
		lsm_common.set_custom_topology = 1;
		mutex_unlock(&lsm_common.cal_data[LSM_CUSTOM_TOP_IDX]->lock);
	}

done:
	return ret;
}

static void lsm_delete_cal_data(void)
{
	pr_debug("%s:\n", __func__);

	cal_utils_destroy_cal_types(LSM_MAX_CAL_IDX, lsm_common.cal_data);
	return;
}

static int q6lsm_init_cal_data(void)
{
	int ret = 0;
	struct cal_type_info	cal_type_info[] = {
		{{LSM_CUST_TOPOLOGY_CAL_TYPE,
		{q6lsm_alloc_cal, q6lsm_dealloc_cal, NULL,
		q6lsm_set_cal, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{LSM_TOPOLOGY_CAL_TYPE,
		{NULL, NULL, NULL,
		q6lsm_set_cal, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{LSM_CAL_TYPE,
		{q6lsm_alloc_cal, q6lsm_dealloc_cal, NULL,
		q6lsm_set_cal, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} }
	};
	pr_debug("%s:\n", __func__);

	ret = cal_utils_create_cal_types(LSM_MAX_CAL_IDX,
		lsm_common.cal_data, cal_type_info);
	if (ret < 0) {
		pr_err("%s: could not create cal type!\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}

	return ret;
err:
	lsm_delete_cal_data();
	return ret;
}

static int __init q6lsm_init(void)
{
	int i = 0;
	pr_debug("%s:\n", __func__);
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

	if (q6lsm_init_cal_data())
		pr_err("%s: could not init cal data!\n", __func__);

	return 0;
}

static void __exit q6lsm_exit(void)
{
	lsm_delete_cal_data();
}

device_initcall(q6lsm_init);
__exitcall(q6lsm_exit);
