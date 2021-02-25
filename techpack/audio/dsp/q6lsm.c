// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2020, Linux Foundation. All rights reserved.
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
#include <audio/sound/lsm_params.h>
#include <asm/ioctls.h>
#include <linux/memory.h>
#include <dsp/msm_audio_ion.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/q6core.h>
#include <dsp/q6lsm.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6common.h>
#include <dsp/audio_cal_utils.h>
#include "adsp_err.h"

#define APR_TIMEOUT	(HZ)
#define LSM_SAMPLE_RATE 16000
#define QLSM_PARAM_ID_MINOR_VERSION 1
#define QLSM_PARAM_ID_MINOR_VERSION_2 2

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

static struct lsm_common lsm_common;
static DEFINE_MUTEX(session_lock);

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
static int q6lsm_send_cal(struct lsm_client *client,
		u32 set_params_opcode, struct lsm_params_info_v2 *p_info);
static int q6lsm_memory_map_regions(struct lsm_client *client,
				    dma_addr_t dma_addr_p, uint32_t dma_buf_sz,
				    uint32_t *mmap_p);
static int q6lsm_memory_unmap_regions(struct lsm_client *client,
				      uint32_t handle);

struct lsm_client_afe_data {
	uint64_t fe_id;
	uint16_t unprocessed_data;
};

static struct lsm_client_afe_data lsm_client_afe_data[LSM_MAX_SESSION_ID + 1];

static int q6lsm_get_session_id_from_lsm_client(struct lsm_client *client)
{
	int n;

	for (n = LSM_MIN_SESSION_ID; n <= LSM_MAX_SESSION_ID; n++) {
		if (lsm_session[n] == client)
			return n;
	}
	pr_err("%s: cannot find matching lsm client.\n", __func__);
	return LSM_INVALID_SESSION_ID;
}

static bool q6lsm_is_valid_lsm_client(struct lsm_client *client)
{
	return q6lsm_get_session_id_from_lsm_client(client) ? 1 : 0;
}

static int q6lsm_callback(struct apr_client_data *data, void *priv)
{
	struct lsm_client *client = (struct lsm_client *)priv;
	uint32_t token;
	uint32_t *payload;
	unsigned long flags;

	if (!client || !data) {
		pr_err("%s: client %pK data %pK\n",
			__func__, client, data);
		WARN_ON(1);
		return -EINVAL;
	}

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: SSR event received 0x%x, event 0x%x, proc 0x%x\n",
			 __func__, data->opcode, data->reset_event,
			 data->reset_proc);

		mutex_lock(&session_lock);
		if (!client || !q6lsm_is_valid_lsm_client(client)) {
			pr_err("%s: client already freed/invalid, return\n",
				__func__);
			mutex_unlock(&session_lock);
			return 0;
		}
		apr_reset(client->apr);
		client->apr = NULL;
		atomic_set(&client->cmd_state, CMD_STATE_CLEARED);
		wake_up(&client->cmd_wait);
		cal_utils_clear_cal_block_q6maps(LSM_MAX_CAL_IDX,
			lsm_common.cal_data);
		mutex_lock(&lsm_common.cal_data[LSM_CUSTOM_TOP_IDX]->lock);
		lsm_common.set_custom_topology = 1;
		mutex_unlock(&lsm_common.cal_data[LSM_CUSTOM_TOP_IDX]->lock);
		mutex_unlock(&session_lock);
		return 0;
	}

	spin_lock_irqsave(&lsm_session_lock, flags);
	if (!client || !q6lsm_is_valid_lsm_client(client)) {
		pr_err("%s: client already freed/invalid, return\n",
			__func__);
		spin_unlock_irqrestore(&lsm_session_lock, flags);
		return -EINVAL;
	}
	payload = data->payload;
	pr_debug("%s: Session %d opcode 0x%x token 0x%x payload size %d\n"
			 "payload [0] = 0x%x\n", __func__, client->session,
		data->opcode, data->token, data->payload_size, payload[0]);
	if (data->opcode == LSM_DATA_EVENT_READ_DONE) {
		struct lsm_cmd_read_done read_done;

		token = data->token;
		if (data->payload_size > sizeof(read_done) ||
				data->payload_size < 6 * sizeof(payload[0])) {
			pr_err("%s: read done error payload size %d expected size %zd\n",
				__func__, data->payload_size,
				sizeof(read_done));
			spin_unlock_irqrestore(&lsm_session_lock, flags);
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
					sizeof(read_done),
					client->priv);
		spin_unlock_irqrestore(&lsm_session_lock, flags);
		return 0;
	} else if (data->opcode == LSM_SESSION_CMDRSP_GET_PARAMS_V3 ||
		data->opcode == LSM_SESSION_CMDRSP_GET_PARAMS_V2) {

		uint32_t payload_min_size_expected = 0;
		uint32_t param_size = 0, ret = 0;
		/*
		 * sizeof(uint32_t) is added to accomodate the status field
		 * in adsp response payload
		 */

		if (data->opcode == LSM_SESSION_CMDRSP_GET_PARAMS_V3)
			payload_min_size_expected  =  sizeof(uint32_t) +
						sizeof(struct param_hdr_v3);
		else
			payload_min_size_expected  =  sizeof(uint32_t) +
						sizeof(struct param_hdr_v2);

		if (data->payload_size < payload_min_size_expected) {
			pr_err("%s: invalid payload size %d expected size %d\n",
				__func__, data->payload_size,
				payload_min_size_expected);
			ret = -EINVAL;
			goto done;
		}

		if (data->opcode == LSM_SESSION_CMDRSP_GET_PARAMS_V3)
			param_size = payload[4];
		else
			param_size = payload[3];

		if (data->payload_size != payload_min_size_expected + param_size) {
			pr_err("%s: cmdrsp_get_params error payload size %d expected size %d\n",
				__func__, data->payload_size,
				payload_min_size_expected + param_size);
			ret = -EINVAL;
			goto done;
		}

		if (client->param_size != param_size) {
			pr_err("%s: response payload size %d mismatched with user requested %d\n",
			    __func__, param_size, client->param_size);
			ret = -EINVAL;
			goto done;
		}

		memcpy((u8 *)client->get_param_payload,
			(u8 *)payload + payload_min_size_expected, param_size);
done:
		spin_unlock_irqrestore(&lsm_session_lock, flags);
		atomic_set(&client->cmd_state, CMD_STATE_CLEARED);
		wake_up(&client->cmd_wait);
		return ret;
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
		case LSM_SESSION_CMD_OPEN_TX_V3:
		case LSM_CMD_ADD_TOPOLOGIES:
		case LSM_SESSION_CMD_SET_PARAMS_V2:
		case LSM_SESSION_CMD_SET_PARAMS_V3:
		case LSM_SESSION_CMD_GET_PARAMS_V2:
		case LSM_SESSION_CMD_GET_PARAMS_V3:
			if (token != client->session &&
			    payload[0] !=
				LSM_SESSION_CMD_DEREGISTER_SOUND_MODEL) {
				pr_err("%s: Invalid session %d receivced expected %d\n",
					__func__, token, client->session);
				spin_unlock_irqrestore(&lsm_session_lock, flags);
				return -EINVAL;
			}
			if (data->payload_size < 2 * sizeof(payload[0])) {
				pr_err("%s: payload has invalid size[%d]\n",
					__func__, data->payload_size);
				spin_unlock_irqrestore(&lsm_session_lock, flags);
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
		spin_unlock_irqrestore(&lsm_session_lock, flags);
		return 0;
	}

	if (client->cb)
		client->cb(data->opcode, data->token, data->payload,
				data->payload_size, client->priv);

	spin_unlock_irqrestore(&lsm_session_lock, flags);
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
	lsm_session[client->session] = NULL;
	lsm_client_afe_data[client->session].fe_id = 0;
	lsm_client_afe_data[client->session].unprocessed_data = 0;
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
	if (lsm_common.apr) {
		if (atomic_read(&lsm_common.apr_users) <= 0) {
			WARN("%s: APR common port already closed\n", __func__);
		} else {
			if (atomic_dec_return(&lsm_common.apr_users) == 0) {
				apr_deregister(lsm_common.apr);
				pr_debug("%s: APR De-Register common port\n",
					__func__);
			}
		}
	}
	return 0;
}

/**
 * q6lsm_client_alloc -
 *       Allocate session for LSM client
 *
 * @cb: callback fn
 * @priv: private data
 *
 * Returns LSM client handle on success or NULL on failure
 */
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

	init_waitqueue_head(&client->cmd_wait);
	mutex_init(&client->cmd_lock);
	atomic_set(&client->cmd_state, CMD_STATE_CLEARED);

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

	pr_debug("%s: New client allocated\n", __func__);
	return client;
fail:
	q6lsm_client_free(client);
	return NULL;
}
EXPORT_SYMBOL(q6lsm_client_alloc);

/**
 * q6lsm_client_free -
 *       Performs LSM client free
 *
 * @client: LSM client handle
 *
 */
void q6lsm_client_free(struct lsm_client *client)
{
	if (!client)
		return;
	if (CHECK_SESSION(client->session)) {
		pr_err("%s: Invalid Session %d\n", __func__, client->session);
		return;
	}
	apr_deregister(client->apr);
	q6lsm_mmap_apr_dereg();
	client->mmap_apr = NULL;
	mutex_lock(&session_lock);
	q6lsm_session_free(client);
	mutex_destroy(&client->cmd_lock);
	kfree(client);
	client = NULL;
	mutex_unlock(&session_lock);
}
EXPORT_SYMBOL(q6lsm_client_free);

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

	if (!handle) {
		pr_err("%s: handle is NULL\n", __func__);
		return -EINVAL;
	}

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
			if (client->cmd_err_code) {
				pr_err("%s: DSP returned error[%s]\n",
					__func__, adsp_err_get_err_str(
					client->cmd_err_code));
				ret = adsp_err_get_lnx_err_code(
						client->cmd_err_code);
			} else {
				ret = 0;
			}
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

	if (mmap_p && *mmap_p == 0)
		ret = -ENOMEM;
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

static int q6lsm_pack_params(u8 *dest, struct param_hdr_v3 *param_info,
			     u8 *param_data, size_t *final_length,
			     u32 set_param_opcode)
{
	bool iid_supported = q6common_is_instance_id_supported();
	union param_hdrs *param_hdr = NULL;
	u32 param_size = param_info->param_size;
	size_t hdr_size;
	size_t provided_size = *final_length;

	hdr_size = iid_supported ? sizeof(struct param_hdr_v3) :
				   sizeof(struct param_hdr_v2);
	if (provided_size < hdr_size) {
		pr_err("%s: Provided size %zu is not large enough, need %zu\n",
		       __func__, provided_size, hdr_size);
		return -EINVAL;
	}

	if (iid_supported) {
		memcpy(dest, param_info, hdr_size);
	} else {
		/* MID, PID and structure size are the same in V1 and V2 */
		param_hdr = (union param_hdrs *) dest;
		param_hdr->v2.module_id = param_info->module_id;
		param_hdr->v2.param_id = param_info->param_id;

		switch (set_param_opcode) {
		case LSM_SESSION_CMD_SET_PARAMS_V2:
			param_hdr->v2.param_size = param_size;
			break;
		case LSM_SESSION_CMD_SET_PARAMS:
		default:
			if (param_size > U16_MAX) {
				pr_err("%s: Invalid param size %d\n", __func__,
				       param_size);
				return -EINVAL;
			}

			param_hdr->v1.param_size = param_size;
			param_hdr->v1.reserved = 0;
			break;
		}
	}

	*final_length = hdr_size;

	if (param_data != NULL) {
		if (provided_size < hdr_size + param_size) {
			pr_err("%s: Provided size %zu is not large enough, need %zu\n",
			       __func__, provided_size, hdr_size + param_size);
			return -EINVAL;
		}
		memcpy(dest + hdr_size, param_data, param_size);
		*final_length += param_size;
	}
	return 0;
}

static int q6lsm_set_params_v2(struct lsm_client *client,
			       struct mem_mapping_hdr *mem_hdr,
			       uint8_t *param_data, uint32_t param_size,
			       uint32_t set_param_opcode)
{
	struct lsm_session_cmd_set_params_v2 *lsm_set_param = NULL;
	uint32_t pkt_size = 0;
	int ret;

	pkt_size = sizeof(struct lsm_session_cmd_set_params_v2);
	/* Only include param size in packet size when inband */
	if (param_data != NULL)
		pkt_size += param_size;

	lsm_set_param = kzalloc(pkt_size, GFP_KERNEL);
	if (!lsm_set_param)
		return -ENOMEM;

	q6lsm_add_hdr(client, &lsm_set_param->apr_hdr, pkt_size, true);
	lsm_set_param->apr_hdr.opcode = set_param_opcode;
	lsm_set_param->payload_size = param_size;

	if (mem_hdr != NULL) {
		lsm_set_param->mem_hdr = *mem_hdr;
	} else if (param_data != NULL) {
		memcpy(lsm_set_param->param_data, param_data, param_size);
	} else {
		pr_err("%s: Received NULL pointers for both memory header and data\n",
		       __func__);
		ret = -EINVAL;
		goto done;
	}

	ret = q6lsm_apr_send_pkt(client, client->apr, lsm_set_param, true,
				 NULL);
done:
	kfree(lsm_set_param);
	return ret;
}

static int q6lsm_set_params_v3(struct lsm_client *client,
			       struct mem_mapping_hdr *mem_hdr,
			       uint8_t *param_data, uint32_t param_size)
{
	struct lsm_session_cmd_set_params_v3 *lsm_set_param = NULL;
	uint16_t pkt_size = 0;
	int ret = 0;

	pkt_size = sizeof(struct lsm_session_cmd_set_params_v3);
	/* Only include param size in packet size when inband */
	if (param_data != NULL)
		pkt_size += param_size;

	lsm_set_param = kzalloc(pkt_size, GFP_KERNEL);
	if (!lsm_set_param)
		return -ENOMEM;

	q6lsm_add_hdr(client, &lsm_set_param->apr_hdr, pkt_size, true);
	lsm_set_param->apr_hdr.opcode = LSM_SESSION_CMD_SET_PARAMS_V3;
	lsm_set_param->payload_size = param_size;

	if (mem_hdr != NULL) {
		lsm_set_param->mem_hdr = *mem_hdr;
	} else if (param_data != NULL) {
		memcpy(lsm_set_param->param_data, param_data, param_size);
	} else {
		pr_err("%s: Received NULL pointers for both memory header and data\n",
		       __func__);
		ret = -EINVAL;
		goto done;
	}

	ret = q6lsm_apr_send_pkt(client, client->apr, lsm_set_param, true,
				 NULL);
done:
	kfree(lsm_set_param);
	return ret;
}

static int q6lsm_get_params_v2(struct lsm_client *client,
				struct mem_mapping_hdr *mem_hdr,
				struct param_hdr_v2 *param_hdr)
{
	struct lsm_session_cmd_get_params_v2 lsm_get_param;
	uint16_t pkt_size = sizeof(lsm_get_param);

	memset(&lsm_get_param, 0, pkt_size);
	q6lsm_add_hdr(client, &lsm_get_param.apr_hdr, pkt_size, true);
	lsm_get_param.apr_hdr.opcode = LSM_SESSION_CMD_GET_PARAMS_V2;

	if (mem_hdr != NULL)
		lsm_get_param.mem_hdr = *mem_hdr;

	memcpy(&lsm_get_param.param_info, param_hdr,
		sizeof(struct param_hdr_v2));

	return q6lsm_apr_send_pkt(client, client->apr, &lsm_get_param, true,
				 NULL);
}

static int q6lsm_get_params_v3(struct lsm_client *client,
				struct mem_mapping_hdr *mem_hdr,
				struct param_hdr_v3 *param_hdr)
{
	struct lsm_session_cmd_get_params_v3 lsm_get_param;
	uint16_t pkt_size = sizeof(lsm_get_param);

	memset(&lsm_get_param, 0, pkt_size);
	q6lsm_add_hdr(client, &lsm_get_param.apr_hdr, pkt_size, true);
	lsm_get_param.apr_hdr.opcode = LSM_SESSION_CMD_GET_PARAMS_V3;

	if (mem_hdr != NULL)
		lsm_get_param.mem_hdr = *mem_hdr;

	memcpy(&lsm_get_param.param_info, param_hdr,
		sizeof(struct param_hdr_v3));

	return q6lsm_apr_send_pkt(client, client->apr, &lsm_get_param, true,
				 NULL);
}

static int q6lsm_set_params(struct lsm_client *client,
			    struct mem_mapping_hdr *mem_hdr,
			    uint8_t *param_data, uint32_t param_size,
			    uint32_t set_param_opcode)

{
	if (q6common_is_instance_id_supported())
		return q6lsm_set_params_v3(client, mem_hdr, param_data,
					   param_size);
	else
		return q6lsm_set_params_v2(client, mem_hdr, param_data,
					   param_size, set_param_opcode);
}

static int q6lsm_pack_and_set_params(struct lsm_client *client,
				     struct param_hdr_v3 *param_info,
				     uint8_t *param_data,
				     uint32_t set_param_opcode)

{
	u8 *packed_data = NULL;
	size_t total_size = 0;
	int ret = 0;

	total_size = sizeof(union param_hdrs) + param_info->param_size;
	packed_data = kzalloc(total_size, GFP_KERNEL);
	if (!packed_data)
		return -ENOMEM;

	ret = q6lsm_pack_params(packed_data, param_info, param_data,
				&total_size, set_param_opcode);
	if (ret)
		goto done;

	ret = q6lsm_set_params(client, NULL, packed_data, total_size,
			       set_param_opcode);

done:
	kfree(packed_data);
	return ret;
}

static int q6lsm_get_params(struct lsm_client *client,
				struct mem_mapping_hdr *mem_hdr,
				struct param_hdr_v3 *param_info)

{
	struct param_hdr_v2 param_info_v2;
	int ret = 0;
	bool iid_supported = q6common_is_instance_id_supported();
	memset(&param_info_v2, 0, sizeof(struct param_hdr_v2));

	if (iid_supported)
		ret = q6lsm_get_params_v3(client, mem_hdr, param_info);
	else {
		param_info_v2.module_id = param_info->module_id;
		param_info_v2.param_id = param_info->param_id;
		param_info_v2.param_size = param_info->param_size;
		ret = q6lsm_get_params_v2(client, mem_hdr, &param_info_v2);
	}
	return ret;
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
			msm_audio_populate_upper_32_bits(
					cal_block->cal_data.paddr);
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

static int q6lsm_get_topology_for_app_type(struct lsm_client *client,
				int app_type, uint32_t *topology)
{
	int rc = -EINVAL;
	struct cal_block_data *cal_block = NULL;
	struct audio_cal_info_lsm_top *lsm_top;
	struct list_head *ptr;

	if (lsm_common.cal_data[LSM_TOP_IDX] == NULL) {
		pr_err("%s: LSM_TOP_IDX invalid\n", __func__);
		return rc;
	}

	mutex_lock(&lsm_common.cal_data[LSM_TOP_IDX]->lock);
	list_for_each(ptr, &lsm_common.cal_data[LSM_TOP_IDX]->cal_blocks) {
		cal_block = list_entry(ptr, struct cal_block_data, list);
		if (!cal_block) {
			pr_err("%s: Cal block for LSM_TOP_IDX not found\n",
				__func__);
			break;
		}

		lsm_top = (struct audio_cal_info_lsm_top *) cal_block->cal_info;
		if (!lsm_top) {
			pr_err("%s: cal_info for LSM_TOP_IDX not found\n",
				__func__);
			break;
		}

		pr_debug("%s: checking topology 0x%x, app_type 0x%x\n",
			 __func__, lsm_top->topology, lsm_top->app_type);

		if (app_type == 0  || lsm_top->app_type == app_type) {
			*topology = lsm_top->topology;
			rc = 0;
			break;
		}
	}
	mutex_unlock(&lsm_common.cal_data[LSM_TOP_IDX]->lock);

	pr_debug("%s: found topology_id = 0x%x, app_type = 0x%x\n",
		 __func__, *topology, app_type);

	return rc;
}

static int q6lsm_do_open_v3(struct lsm_client *client)
{
	int rc, app_type;
	struct lsm_stream_cmd_open_tx_v3 *open_v3;
	size_t cmd_size = 0;
	int stage_idx = LSM_STAGE_INDEX_FIRST;
	uint32_t topology_id = 0, *uint32_ptr = NULL;

	cmd_size = sizeof(struct lsm_stream_cmd_open_tx_v3);
	cmd_size += client->num_stages * sizeof(struct lsm_stream_stage_info);
	open_v3 = kzalloc(cmd_size, GFP_KERNEL);
	if (!open_v3)
		return -ENOMEM;

	q6lsm_add_hdr(client, &open_v3->hdr, cmd_size, true);
	open_v3->hdr.opcode = LSM_SESSION_CMD_OPEN_TX_V3;
	open_v3->num_stages = client->num_stages;
	uint32_ptr = &open_v3->num_stages;
	uint32_ptr++;

	for (; stage_idx < client->num_stages; stage_idx++) {
		app_type = client->stage_cfg[stage_idx].app_type;
		rc = q6lsm_get_topology_for_app_type(client, app_type, &topology_id);
		if (rc) {
			pr_err("%s: failed to get topology for stage %d\n",
				 __func__, stage_idx);
			return -EINVAL;
		}
		*uint32_ptr++ = topology_id;
		*uint32_ptr++ = client->stage_cfg[stage_idx].lpi_enable;
	}

	rc = q6lsm_apr_send_pkt(client, client->apr, open_v3, true, NULL);
	if (rc)
		pr_err("%s: open_v3 failed, err = %d\n", __func__, rc);
	else
		client->use_topology = true;

	kfree(open_v3);
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

	pr_debug("%s: topology_id = 0x%x, app_type = 0x%x\n",
		 __func__, lsm_top->topology, lsm_top->app_type);

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

/**
 * q6lsm_sm_set_param_data -
 *       Update sound model param data
 *
 * @client: LSM client handle
 * @p_info: param info
 * @offset: pointer to retrieve size
 * @sm: pointer to sound model
 */
void q6lsm_sm_set_param_data(struct lsm_client *client,
			     struct lsm_params_info_v2 *p_info,
			     size_t *offset, struct lsm_sound_model *sm)
{
	struct param_hdr_v3 param_hdr;
	int ret;

	memset(&param_hdr, 0, sizeof(param_hdr));

	param_hdr.module_id = p_info->module_id;
	param_hdr.instance_id = p_info->instance_id;
	param_hdr.param_id = p_info->param_id;
	param_hdr.param_size = p_info->param_size;

	sm->model_id = p_info->model_id;

	ret = q6lsm_pack_params(sm->data, &param_hdr,
				NULL, offset, LSM_SESSION_CMD_SET_PARAMS_V2);
	if (ret)
		pr_err("%s: Failed to pack params, error %d\n", __func__, ret);
}
EXPORT_SYMBOL(q6lsm_sm_set_param_data);

/**
 * q6lsm_support_multi_stage_detection -
 *       check for multi-stage support in adsp lsm framework service
 *
 * Returns true if multi-stage support available, else false
 */
bool q6lsm_adsp_supports_multi_stage_detection(void)
{
	return q6core_get_avcs_api_version_per_service(
			APRV2_IDS_SERVICE_ID_ADSP_LSM_V) >= LSM_API_VERSION_V3;
}
EXPORT_SYMBOL(q6lsm_adsp_supports_multi_stage_detection);

/**
 * q6lsm_open -
 *       command to open LSM session
 *
 * @client: LSM client handle
 * @app_id: App ID for LSM
 *
 * Returns 0 on success or error on failure
 */
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
	if ((client->stage_cfg[LSM_STAGE_INDEX_FIRST].app_type != 0) &&
		q6lsm_adsp_supports_multi_stage_detection())
		rc = q6lsm_do_open_v3(client);
	else
		rc = q6lsm_do_open_v2(client, app_id);
	if (!rc)
		/* open_v2/v3 was successful */
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
EXPORT_SYMBOL(q6lsm_open);

static int q6lsm_send_confidence_levels(struct lsm_client *client,
					struct param_hdr_v3 *param_info,
					uint32_t set_param_opcode, uint32_t model_id)
{
	struct lsm_param_confidence_levels *conf_levels = NULL;
	uint32_t num_conf_levels = client->num_confidence_levels;
	struct lsm_param_multi_snd_model_conf_levels *multi_sm_conf_levels = NULL;
	uint32_t num_keywords = client->num_keywords;
	uint8_t i = 0;
	uint8_t padd_size = 0;
	uint32_t param_size = 0;
	int rc = 0;

	if (model_id != 0) {
		/* No padding is need since the structure is always 4 byte aligend.
		 * The number "2" below represents the first two u32 variables in
		 * struct lsm_param_multi_snd_model_conf_levels.
		 */
		param_size = (2 + num_keywords) * sizeof(uint32_t);
		param_info->param_size = param_size;
		pr_debug("%s: Set Conf Levels PARAM SIZE: %d\n", __func__, param_size);

		multi_sm_conf_levels = kzalloc(param_size, GFP_KERNEL);
		if (!multi_sm_conf_levels)
			return -ENOMEM;

		multi_sm_conf_levels->model_id = model_id;
		multi_sm_conf_levels->num_keywords = num_keywords;
		pr_debug("%s: snd_model id: %d, num_keywords: %d\n",
			 __func__, model_id, num_keywords);

		memcpy(multi_sm_conf_levels->confidence_levels,
		       client->multi_snd_model_confidence_levels,
		       sizeof(uint32_t) * num_keywords);
		for (i = 0; i < num_keywords; i++)
			pr_debug("%s: Confidence_level[%d] = %d\n", __func__, i,
				 multi_sm_conf_levels->confidence_levels[i]);

		rc = q6lsm_pack_and_set_params(client, param_info,
					       (uint8_t *) multi_sm_conf_levels,
					       set_param_opcode);
		if (rc)
			pr_err("%s: Send multi_snd_model_conf_levels cmd failed, err %d\n",
			       __func__, rc);
		kfree(multi_sm_conf_levels);
	} else {
		/* Data must be 4 byte aligned so add any necessary padding. */
		padd_size = (4 - (num_conf_levels % 4)) - 1;
		param_size = (sizeof(uint8_t) + num_conf_levels + padd_size) *
			      sizeof(uint8_t);
		param_info->param_size = param_size;
		pr_debug("%s: Set Conf Levels PARAM SIZE = %d\n", __func__, param_size);

		conf_levels = kzalloc(param_size, GFP_KERNEL);
		if (!conf_levels)
			return -ENOMEM;

		conf_levels->num_confidence_levels = num_conf_levels;
		pr_debug("%s: Num conf_level = %d\n", __func__, num_conf_levels);

		memcpy(conf_levels->confidence_levels, client->confidence_levels,
		       num_conf_levels);
		for (i = 0; i < num_conf_levels; i++)
			pr_debug("%s: Confidence_level[%d] = %d\n", __func__, i,
				 conf_levels->confidence_levels[i]);

		rc = q6lsm_pack_and_set_params(client, param_info,
					       (uint8_t *) conf_levels,
					       set_param_opcode);
		if (rc)
			pr_err("%s: Send confidence_levels cmd failed, err = %d\n",
			       __func__, rc);
		kfree(conf_levels);
	}
	return rc;
}

static int q6lsm_send_param_opmode(struct lsm_client *client,
				   struct param_hdr_v3 *param_info,
				   u32 set_param_opcode)
{
	struct lsm_param_op_mode op_mode;
	int rc = 0;

	memset(&op_mode, 0, sizeof(op_mode));
	param_info->param_size = sizeof(op_mode);

	op_mode.minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	op_mode.mode = client->mode;
	pr_debug("%s: mode = 0x%x", __func__, op_mode.mode);

	rc = q6lsm_pack_and_set_params(client, param_info, (uint8_t *) &op_mode,
				       set_param_opcode);
	if (rc)
		pr_err("%s: Failed set_params, rc %d\n", __func__, rc);

	pr_debug("%s: leave %d\n", __func__, rc);
	return rc;
}

/**
 * set_lsm_port -
 *       Update LSM AFE port
 *
 */
void set_lsm_port(int lsm_port)
{
	lsm_afe_port = lsm_port;
}
EXPORT_SYMBOL(set_lsm_port);

int get_lsm_port(void)
{
	return lsm_afe_port;
}

/**
 * q6lsm_set_afe_data_format -
 * command to set afe data format
 *
 * @fe_id: FrontEnd DAI link ID
 * @afe_data_format: afe data format
 *
 * Returns 0 on success or -EINVAL on failure
 */
int q6lsm_set_afe_data_format(uint64_t fe_id, uint16_t afe_data_format)
{
	int n = 0;

	if (0 != afe_data_format && 1 != afe_data_format)
		goto done;

	pr_debug("%s: afe data is %s\n", __func__,
		 afe_data_format ? "unprocessed" : "processed");

	for (n = LSM_MIN_SESSION_ID; n <= LSM_MAX_SESSION_ID; n++) {
		if (0 == lsm_client_afe_data[n].fe_id) {
			lsm_client_afe_data[n].fe_id = fe_id;
			lsm_client_afe_data[n].unprocessed_data =
							afe_data_format;
			pr_debug("%s: session ID is %d, fe_id is %d\n",
				 __func__, n, fe_id);
			return 0;
		}
	}

	pr_err("%s: all lsm sessions are taken\n", __func__);
done:
	return -EINVAL;
}
EXPORT_SYMBOL(q6lsm_set_afe_data_format);

/**
 * q6lsm_get_afe_data_format -
 * command to get afe data format
 *
 * @fe_id: FrontEnd DAI link ID
 * @afe_data_format: afe data format
 *
 */
void q6lsm_get_afe_data_format(uint64_t fe_id, uint16_t *afe_data_format)
{
	int n = 0;

	if (NULL == afe_data_format) {
		pr_err("%s: Pointer afe_data_format is NULL\n", __func__);
		return;
	}

	for (n = LSM_MIN_SESSION_ID; n <= LSM_MAX_SESSION_ID; n++) {
		if (fe_id == lsm_client_afe_data[n].fe_id) {
			*afe_data_format =
				lsm_client_afe_data[n].unprocessed_data;
			pr_debug("%s: session: %d, fe_id: %d, afe data: %s\n",
				__func__, n, fe_id,
				*afe_data_format ? "unprocessed" : "processed");
			return;
		}
	}
}
EXPORT_SYMBOL(q6lsm_get_afe_data_format);

/**
 * q6lsm_set_port_connected -
 *       command to set LSM port connected
 *
 * @client: LSM client handle
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_set_port_connected(struct lsm_client *client)
{
	struct lsm_param_connect_to_port connect_port;
	struct param_hdr_v3 connectport_hdr;
	u32 set_param_opcode = 0;
	int rc = 0;

	memset(&connect_port, 0, sizeof(connect_port));
	memset(&connectport_hdr, 0, sizeof(connectport_hdr));

	if (client->use_topology) {
		set_param_opcode = LSM_SESSION_CMD_SET_PARAMS_V2;
		connectport_hdr.module_id = LSM_MODULE_ID_FRAMEWORK;
	} else {
		set_param_opcode = LSM_SESSION_CMD_SET_PARAMS;
		connectport_hdr.module_id = LSM_MODULE_ID_VOICE_WAKEUP;
	}
	connectport_hdr.instance_id = INSTANCE_ID_0;
	connectport_hdr.param_id = LSM_PARAM_ID_CONNECT_TO_PORT;
	connectport_hdr.param_size = sizeof(connect_port);

	client->connect_to_port = get_lsm_port();
	if (ADM_LSM_PORT_ID != client->connect_to_port)
		q6lsm_get_afe_data_format(client->fe_id,
					  &client->unprocessed_data);
	connect_port.minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	connect_port.port_id = client->connect_to_port;
	connect_port.unprocessed_data = client->unprocessed_data;

	rc = q6lsm_pack_and_set_params(client, &connectport_hdr,
				       (uint8_t *) &connect_port,
				       set_param_opcode);
	if (rc)
		pr_err("%s: Failed set_params, rc %d\n", __func__, rc);

	return rc;
}
EXPORT_SYMBOL(q6lsm_set_port_connected);

static int q6lsm_send_param_polling_enable(struct lsm_client *client,
					   bool poll_en,
					   struct param_hdr_v3 *param_info,
					   u32 set_param_opcode)
{
	struct lsm_param_poll_enable polling_enable;
	int rc = 0;

	memset(&polling_enable, 0, sizeof(polling_enable));
	param_info->param_size = sizeof(polling_enable);

	polling_enable.minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	polling_enable.polling_enable = (poll_en) ? 1 : 0;

	rc = q6lsm_pack_and_set_params(client, param_info,
				       (uint8_t *) &polling_enable,
				       set_param_opcode);
	if (rc)
		pr_err("%s: Failed set_params, rc %d\n", __func__, rc);
	return rc;
}

/**
 * q6lsm_set_fwk_mode_cfg -
 *       command to set LSM fwk mode cfg
 *
 * @client: LSM client handle
 * @event_mode: mode for fwk cfg
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_set_fwk_mode_cfg(struct lsm_client *client,
			   uint32_t event_mode)
{
	struct lsm_param_fwk_mode_cfg fwk_mode_cfg;
	struct param_hdr_v3 fwk_mode_cfg_hdr;
	int rc = 0;

	memset(&fwk_mode_cfg, 0, sizeof(fwk_mode_cfg));
	memset(&fwk_mode_cfg_hdr, 0, sizeof(fwk_mode_cfg_hdr));

	if (!client->use_topology) {
		pr_debug("%s: Ignore sending event mode\n", __func__);
		return rc;
	}

	fwk_mode_cfg_hdr.module_id = LSM_MODULE_ID_FRAMEWORK;
	fwk_mode_cfg_hdr.instance_id = INSTANCE_ID_0;
	fwk_mode_cfg_hdr.param_id = LSM_PARAM_ID_FWK_MODE_CONFIG;
	fwk_mode_cfg_hdr.param_size = sizeof(fwk_mode_cfg);

	fwk_mode_cfg.minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	fwk_mode_cfg.mode = event_mode;
	pr_debug("%s: mode = %d\n", __func__, fwk_mode_cfg.mode);

	rc = q6lsm_pack_and_set_params(client, &fwk_mode_cfg_hdr,
				       (uint8_t *) &fwk_mode_cfg,
				       LSM_SESSION_CMD_SET_PARAMS_V2);
	if (rc)
		pr_err("%s: Failed set_params, rc %d\n", __func__, rc);
	return rc;
}
EXPORT_SYMBOL(q6lsm_set_fwk_mode_cfg);

static int q6lsm_arrange_mch_map(uint8_t *ch_map, int ch_cnt)
{
	int ch_idx;
	u8 mch_map[LSM_V3P0_MAX_NUM_CHANNELS] = {
			PCM_CHANNEL_FL, PCM_CHANNEL_FR, PCM_CHANNEL_FC,
			PCM_CHANNEL_LS, PCM_CHANNEL_RS, PCM_CHANNEL_LFE,
			PCM_CHANNEL_LB, PCM_CHANNEL_RB, PCM_CHANNEL_CS};


	if (ch_cnt > LSM_V3P0_MAX_NUM_CHANNELS) {
		pr_err("%s: invalid num_chan %d\n", __func__, ch_cnt);
		return -EINVAL;
	}

	if (ch_cnt == 1) {
		ch_map[0] = PCM_CHANNEL_FC;
	} else if (ch_cnt == 4) {
		ch_map[0] = PCM_CHANNEL_FL;
		ch_map[1] = PCM_CHANNEL_FR;
		ch_map[2] = PCM_CHANNEL_LS;
		ch_map[3] = PCM_CHANNEL_RS;
	} else {
		for (ch_idx = 0; ch_idx < ch_cnt; ch_idx++)
			ch_map[ch_idx] = mch_map[ch_idx];
	}

	return 0;
}

/**
 * q6lsm_set_media_fmt_params -
 *       command to set LSM media fmt params
 *
 * @client: LSM client handle
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_set_media_fmt_params(struct lsm_client *client)
{
	struct lsm_param_media_fmt media_fmt;
	struct lsm_hw_params in_param = client->in_hw_params;
	struct param_hdr_v3 media_fmt_hdr;
	int rc = 0;

	memset(&media_fmt, 0, sizeof(media_fmt));
	memset(&media_fmt_hdr, 0, sizeof(media_fmt_hdr));

	if (!client->use_topology) {
		pr_debug("%s: Ignore sending media format\n", __func__);
		goto err_ret;
	}

	media_fmt_hdr.module_id = LSM_MODULE_ID_FRAMEWORK;
	media_fmt_hdr.instance_id = INSTANCE_ID_0;
	media_fmt_hdr.param_id = LSM_PARAM_ID_MEDIA_FMT;
	media_fmt_hdr.param_size = sizeof(media_fmt);

	media_fmt.minor_version = QLSM_PARAM_ID_MINOR_VERSION_2;
	media_fmt.sample_rate = in_param.sample_rate;
	media_fmt.num_channels = in_param.num_chs;
	media_fmt.bit_width = in_param.sample_size;
	rc = q6lsm_arrange_mch_map(media_fmt.channel_mapping,
				   media_fmt.num_channels);
	if (rc)
		goto err_ret;

	pr_debug("%s: sample rate= %d, channels %d bit width %d\n", __func__,
		 media_fmt.sample_rate, media_fmt.num_channels,
		 media_fmt.bit_width);

	rc = q6lsm_pack_and_set_params(client, &media_fmt_hdr,
				       (uint8_t *) &media_fmt,
				       LSM_SESSION_CMD_SET_PARAMS_V2);
	if (rc)
		pr_err("%s: Failed set_params, rc %d\n", __func__, rc);
err_ret:
	return rc;
}
EXPORT_SYMBOL(q6lsm_set_media_fmt_params);

/*
 * q6lsm_set_media_fmt_v2_params -
 *       command to set LSM media fmt (version2) params
 *
 * @client: LSM client handle
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_set_media_fmt_v2_params(struct lsm_client *client)
{
	u8 *param_buf;
	struct lsm_param_media_fmt_v2 *media_fmt_v2;
	struct lsm_hw_params *in_param = &client->in_hw_params;
	struct param_hdr_v3 media_fmt_v2_hdr;
	int param_len = 0, rc = 0;

	memset(&media_fmt_v2_hdr, 0, sizeof(media_fmt_v2_hdr));

	param_len = sizeof(*media_fmt_v2) +
		    (sizeof(uint8_t) * in_param->num_chs);

	/* Add padding to make sure total length is 4-byte aligned */
	if (param_len % 4)
		param_len += (4 - (param_len % 4));

	param_buf = kzalloc(param_len, GFP_KERNEL);
	if (!param_buf)
		return -ENOMEM;
	media_fmt_v2 = (struct lsm_param_media_fmt_v2 *) param_buf;
	media_fmt_v2->minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	media_fmt_v2->sample_rate = in_param->sample_rate;
	media_fmt_v2->num_channels = in_param->num_chs;
	media_fmt_v2->bit_width = in_param->sample_size;
	rc = q6lsm_arrange_mch_map(media_fmt_v2->channel_mapping,
				   in_param->num_chs);
	if (rc)
		goto err_mch_map;

	media_fmt_v2_hdr.module_id = LSM_MODULE_ID_FRAMEWORK;
	media_fmt_v2_hdr.instance_id = INSTANCE_ID_0;
	media_fmt_v2_hdr.param_id = LSM_PARAM_ID_MEDIA_FMT_V2;
	media_fmt_v2_hdr.param_size = param_len;

	pr_debug("%s: sample rate= %d, channels %d bit width %d\n", __func__,
		 media_fmt_v2->sample_rate, media_fmt_v2->num_channels,
		 media_fmt_v2->bit_width);

	rc = q6lsm_pack_and_set_params(client, &media_fmt_v2_hdr,
				       param_buf,
				       LSM_SESSION_CMD_SET_PARAMS_V2);
	if (rc)
		pr_err("%s: Failed set_params, rc %d\n", __func__, rc);

err_mch_map:
	kfree(param_buf);
	return rc;
}
EXPORT_SYMBOL(q6lsm_set_media_fmt_v2_params);

/**
 * q6lsm_set_data -
 *       Command to set LSM data
 *
 * @client: LSM client handle
 * @mode: LSM detection mode value
 * @detectfailure: flag for detect failure
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_set_data(struct lsm_client *client,
			   enum lsm_detection_mode mode,
			   bool detectfailure)
{
	struct param_hdr_v3 param_hdr;
	int rc = 0;
	struct lsm_params_info_v2 p_info = {0};

	memset(&param_hdr, 0, sizeof(param_hdr));

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

	param_hdr.module_id = LSM_MODULE_ID_VOICE_WAKEUP;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = LSM_PARAM_ID_OPERATION_MODE;
	rc = q6lsm_send_param_opmode(client, &param_hdr,
				     LSM_SESSION_CMD_SET_PARAMS);
	if (rc) {
		pr_err("%s: Failed to set lsm config params %d\n",
			__func__, rc);
		goto err_ret;
	}

	param_hdr.param_id = LSM_PARAM_ID_MIN_CONFIDENCE_LEVELS;
	rc = q6lsm_send_confidence_levels(client, &param_hdr,
					  LSM_SESSION_CMD_SET_PARAMS, 0);
	if (rc) {
		pr_err("%s: Failed to send conf_levels, err = %d\n",
			__func__, rc);
		goto err_ret;
	}

	p_info.stage_idx = LSM_STAGE_INDEX_FIRST;
	rc = q6lsm_send_cal(client, LSM_SESSION_CMD_SET_PARAMS, &p_info);
	if (rc) {
		pr_err("%s: Failed to send calibration data %d\n",
			__func__, rc);
		goto err_ret;
	}

err_ret:
	return rc;
}
EXPORT_SYMBOL(q6lsm_set_data);

/**
 * q6lsm_register_sound_model -
 *       Register LSM snd model
 *
 * @client: LSM client handle
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_register_sound_model(struct lsm_client *client,
			       enum lsm_detection_mode mode,
			       bool detectfailure)
{
	int rc;
	struct lsm_cmd_reg_snd_model cmd;
	struct lsm_sound_model *sm;

	memset(&cmd, 0, sizeof(cmd));
	rc = q6lsm_set_data(client, mode, detectfailure);
	if (rc) {
		pr_err("%s: Failed to set lsm data, err = %d\n",
			__func__, rc);
		return rc;
	}

	sm = &client->stage_cfg[LSM_STAGE_INDEX_FIRST].sound_model;

	q6lsm_add_hdr(client, &cmd.hdr, sizeof(cmd), true);
	cmd.hdr.opcode = LSM_SESSION_CMD_REGISTER_SOUND_MODEL;
	cmd.model_addr_lsw = lower_32_bits(sm->phys);
	cmd.model_addr_msw = msm_audio_populate_upper_32_bits(sm->phys);
	cmd.model_size = sm->size;
	/* read updated mem_map_handle by q6lsm_mmapcallback */
	rmb();
	cmd.mem_map_handle = sm->mem_map_handle;

	pr_debug("%s: addr %pK, size %d, handle 0x%x\n", __func__,
		&sm->phys, cmd.model_size, cmd.mem_map_handle);
	rc = q6lsm_apr_send_pkt(client, client->apr, &cmd, true, NULL);
	if (rc)
		pr_err("%s: Failed cmd op[0x%x]rc[%d]\n", __func__,
		       cmd.hdr.opcode, rc);
	else
		pr_debug("%s: Register sound model succeeded\n", __func__);

	return rc;
}
EXPORT_SYMBOL(q6lsm_register_sound_model);

/**
 * q6lsm_deregister_sound_model -
 *       De-register LSM snd model
 *
 * @client: LSM client handle
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_deregister_sound_model(struct lsm_client *client)
{
	int rc = 0;
	struct lsm_cmd_reg_snd_model cmd;
	struct lsm_sound_model *sm = NULL, *next = NULL;
	/*
	 * With multi-stage support sm buff allocation/free usage param info
	 * to check stage index for which this sound model is being set, and
	 * to check whether sm data is sent using set param command or not.
	 * Hence, set param ids to '0' to indicate allocation is for legacy
	 * reg_sm cmd, where buffer for param header need not be allocated,
	 * also set stage index to LSM_STAGE_INDEX_FIRST.
	 */
	struct lsm_params_info_v2 p_info = {0};
	p_info.stage_idx = LSM_STAGE_INDEX_FIRST;

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

	if (client->num_sound_models > 0) {
		p_info.param_type = LSM_DEREG_MULTI_SND_MODEL;
		list_for_each_entry_safe(sm, next,
				&client->stage_cfg[p_info.stage_idx].sound_models,
				list) {
			pr_debug("%s: current snd_model: %d, num of sound models left %d\n",
				 __func__, sm->model_id, client->num_sound_models);
			q6lsm_snd_model_buf_free(client, &p_info, sm);
			list_del(&sm->list);
			kfree(sm);
			sm = NULL;
			client->num_sound_models--;

			if (0 == client->num_sound_models)
				break;
		}
	} else {
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

		p_info.param_type = LSM_DEREG_SND_MODEL;
		sm = &client->stage_cfg[p_info.stage_idx].sound_model;
		q6lsm_snd_model_buf_free(client, &p_info, sm);
	}

	return rc;
}
EXPORT_SYMBOL(q6lsm_deregister_sound_model);

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

	pr_debug("%s: dma_addr_p 0x%pK, dma_buf_sz %d, mmap_p 0x%pK, session %d\n",
		__func__, &dma_addr_p, dma_buf_sz, mmap_p,
		client->session);
	if (CHECK_SESSION(client->session)) {
		pr_err("%s: session[%d]", __func__, client->session);
		return -EINVAL;
	}
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
	mregions->shm_addr_msw = msm_audio_populate_upper_32_bits(dma_addr_p);
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
			u32 set_params_opcode, struct lsm_params_info_v2 *p_info)
{
	int rc = 0, stage_idx = p_info->stage_idx;
	struct mem_mapping_hdr mem_hdr;
	dma_addr_t lsm_cal_phy_addr;

	memset(&mem_hdr, 0, sizeof(mem_hdr));

	pr_debug("%s: Session id %d\n", __func__, client->session);
	if (CHECK_SESSION(client->session)) {
		pr_err("%s: session[%d]", __func__, client->session);
		return -EINVAL;
	}

	lsm_cal_phy_addr = client->stage_cfg[stage_idx].cal_info.phys;
	if (lsm_cal_phy_addr != 0) {
		lsm_common.common_client[client->session].session = client->session;
		mem_hdr.data_payload_addr_lsw = lower_32_bits(lsm_cal_phy_addr);
		mem_hdr.data_payload_addr_msw =
			msm_audio_populate_upper_32_bits(lsm_cal_phy_addr);
		mem_hdr.mem_map_handle =
			client->stage_cfg[stage_idx].cal_info.mem_map_handle;

		rc = q6lsm_set_params(client, &mem_hdr, NULL,
				client->stage_cfg[stage_idx].cal_info.size, set_params_opcode);
		if (rc)
			pr_err("%s: Failed set_params, rc %d\n", __func__, rc);
	}

	return rc;
}

static int q6lsm_snd_cal_free(struct lsm_client *client,
			struct lsm_params_info_v2 *p_info)
{
	int rc = 0, stage_idx = p_info->stage_idx;
	struct lsm_cal_data_info *cal = NULL;

	if (!client->stage_cfg[stage_idx].cal_info.data)
		return 0;

	mutex_lock(&client->cmd_lock);
	cal = &client->stage_cfg[stage_idx].cal_info;
	if (cal->mem_map_handle != 0) {
		rc = q6lsm_memory_unmap_regions(client, cal->mem_map_handle);
		if (rc)
			pr_err("%s: CMD Memory_unmap_regions failed %d\n",
					__func__, rc);
		cal->mem_map_handle = 0;
	}
	msm_audio_ion_free(cal->dma_buf);
	cal->dma_buf = NULL;
	cal->data = NULL;
	cal->phys = 0;
	mutex_unlock(&client->cmd_lock);

	return rc;
}

static int q6lsm_snd_cal_alloc(struct lsm_client *client,
			struct lsm_params_info_v2 *p_info)
{
	int rc = 0;
	size_t len = 0, total_mem = 0;
	struct lsm_cal_data_info *cal = NULL;
	struct cal_block_data *cal_block = NULL;
	struct audio_cal_info_lsm *lsm_cal_info = NULL;
	struct list_head *ptr = NULL;
	int app_type, stage_idx = p_info->stage_idx;
	bool cal_block_found = false;

	app_type = client->stage_cfg[stage_idx].app_type;
	pr_debug("%s: app_type %d, stage_idx %d\n",
			__func__, app_type, stage_idx);

	mutex_lock(&client->cmd_lock);
	mutex_lock(&lsm_common.cal_data[LSM_CAL_IDX]->lock);
	list_for_each(ptr, &lsm_common.cal_data[LSM_CAL_IDX]->cal_blocks) {
		cal_block = list_entry(ptr, struct cal_block_data, list);
		lsm_cal_info = (struct audio_cal_info_lsm *)
						(cal_block) ? cal_block->cal_info : NULL;
		if ((cal_block && cal_block->cal_data.paddr) &&
			(lsm_cal_info != NULL) &&
			(app_type == 0 || app_type == lsm_cal_info->app_type)) {
			cal_block_found = true;
			len = cal_block->cal_data.size;
			break;
		}
	}

	if (!cal_block_found) {
		pr_info("%s: cal not found for stage_idx %d\n", __func__, stage_idx);
		goto exit;
	}

	if (!len) {
		pr_debug("%s: cal size is 0, for stage_idx %d\n", __func__, stage_idx);
		goto exit;
	}

	cal = &client->stage_cfg[stage_idx].cal_info;
	if (cal->data) {
		pr_debug("%s: cal data for stage_idx(%d) is already set \n",
			__func__, stage_idx);
		goto exit;
	}

	cal->size = len;
	total_mem = PAGE_ALIGN(len);
	pr_debug("%s: cal info data size %zd Total mem %zd, stage_idx %d\n",
		 __func__, len, total_mem, stage_idx);

	rc = msm_audio_ion_alloc(&cal->dma_buf, total_mem,
			&cal->phys, &len, &cal->data);
	if (rc) {
		pr_err("%s: Audio ION alloc is failed for stage_idx %d, rc = %d\n",
			__func__, stage_idx, rc);
		cal->dma_buf = NULL;
		cal->data = NULL;
		goto exit;
	}

	memcpy(cal->data, (uint32_t *)cal_block->cal_data.kvaddr, cal->size);
	mutex_unlock(&lsm_common.cal_data[LSM_CAL_IDX]->lock);
	mutex_unlock(&client->cmd_lock);
	rc = q6lsm_memory_map_regions(client, cal->phys, len, &cal->mem_map_handle);
	if (rc) {
		pr_err("%s: CMD Memory_map_regions failed for stage_idx %d, rc = %d\n",
			__func__, stage_idx, rc);
		cal->mem_map_handle = 0;
		goto fail;
	}

	return 0;

exit:
	mutex_unlock(&client->cmd_lock);
	mutex_unlock(&lsm_common.cal_data[LSM_CAL_IDX]->lock);
fail:
	q6lsm_snd_cal_free(client, p_info);
	return rc;
}

/**
 * q6lsm_snd_model_buf_free -
 *       Free memory for LSM snd model
 *
 * @client: LSM client handle
 * @p_info: sound model param info
 * @sm: pointer to sound model
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_snd_model_buf_free(struct lsm_client *client,
			     struct lsm_params_info_v2 *p_info,
			     struct lsm_sound_model *sm)
{
	int rc = 0, stage_idx = p_info->stage_idx;

	pr_debug("%s: Session id %d\n", __func__, client->session);
	if (CHECK_SESSION(client->session)) {
		pr_err("%s: session[%d]", __func__, client->session);
		return -EINVAL;
	}

	if (p_info->param_type == LSM_DEREG_SND_MODEL &&
	    !client->stage_cfg[stage_idx].sound_model.data)
		return 0;

	mutex_lock(&client->cmd_lock);
	if (sm->mem_map_handle != 0) {
		rc = q6lsm_memory_unmap_regions(client, sm->mem_map_handle);
		if (rc)
			pr_err("%s: CMD Memory_unmap_regions failed %d\n",
				__func__, rc);
		sm->mem_map_handle = 0;
	}
	msm_audio_ion_free(sm->dma_buf);
	sm->dma_buf = NULL;
	sm->data = NULL;
	sm->phys = 0;
	mutex_unlock(&client->cmd_lock);

	if ((p_info->param_type == LSM_DEREG_MULTI_SND_MODEL &&
	    client->num_sound_models == 1) ||
	    p_info->param_type == LSM_DEREG_SND_MODEL)
		rc = q6lsm_snd_cal_free(client, p_info);
	return rc;
}
EXPORT_SYMBOL(q6lsm_snd_model_buf_free);

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

		if (sid < LSM_MIN_SESSION_ID || sid > LSM_MAX_SESSION_ID)
			pr_err("%s: Invalid session %d\n", __func__, sid);
		apr_reset(lsm_common.apr);
		lsm_common.apr = NULL;
		atomic_set(&lsm_common.apr_users, 0);
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
		/* fallthrough */
	default:
		pr_debug("%s: command 0x%x return code 0x%x opcode 0x%x\n",
			 __func__, command, retcode, data->opcode);
		break;
	}
	if (client->cb)
		client->cb(data->opcode, data->token,
			   data->payload, data->payload_size,
			   client->priv);
	return 0;
}

/**
 * q6lsm_snd_model_buf_alloc -
 *       Allocate memory for LSM snd model
 *
 * @client: LSM client handle
 * @len: size of sound model
 * @p_info: sound model param info
 *          p_info->param_id != 0 when using set param to register sound model
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_snd_model_buf_alloc(struct lsm_client *client, size_t len,
			      struct lsm_params_info_v2 *p_info,
			      struct lsm_sound_model *sm)
{
	int rc = -EINVAL, stage_idx = p_info->stage_idx;
	int model_id = p_info->model_id;
	size_t total_mem = 0;

	if (!client)
		return rc;

	pr_debug("%s:Snd Model len %zd, stage_idx %d, model_id %d\n",
		 __func__, len, stage_idx, model_id);

	mutex_lock(&client->cmd_lock);
	if (!sm->data) {
		/*
		 * If sound model is sent as set_param, i.e. param_id != 0,
		 * Then memory needs to be allocated for
		 * set_param payload as well.
		 */
		if (p_info->param_id != 0)
			len += sizeof(union param_hdrs);

		sm->size = len;
		total_mem = PAGE_ALIGN(len);
		pr_debug("%s: sm param size %zd Total mem %zd, stage_idx %d\n",
				 __func__, len, total_mem, stage_idx);
		rc = msm_audio_ion_alloc(&sm->dma_buf, total_mem,
					 &sm->phys, &len, &sm->data);
		if (rc) {
			pr_err("%s: Audio ION alloc is failed, rc = %d, stage_idx = %d\n",
				__func__, rc, stage_idx);
			goto fail;
		}
	} else {
		pr_err("%s: sound model busy, stage_idx %d\n", __func__, stage_idx);
		rc = -EBUSY;
		goto fail;
	}

	rc = q6lsm_memory_map_regions(client, sm->phys, len, &sm->mem_map_handle);
	if (rc) {
		pr_err("%s: CMD Memory_map_regions failed %d, stage_idx %d\n",
			__func__, rc, stage_idx);
		sm->mem_map_handle = 0;
		goto fail;
	}
	mutex_unlock(&client->cmd_lock);

	rc = q6lsm_snd_cal_alloc(client, p_info);
	if (rc) {
		pr_err("%s: cal alloc failed %d, stage_idx %d\n",
			__func__, rc, stage_idx);
		goto fail_1;
	}
	return rc;

fail:
	mutex_unlock(&client->cmd_lock);
fail_1:
	q6lsm_snd_model_buf_free(client, p_info, sm);
	return rc;
}
EXPORT_SYMBOL(q6lsm_snd_model_buf_alloc);

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

static int q6lsm_send_param_epd_thres(struct lsm_client *client, void *data,
				      struct param_hdr_v3 *param_info)
{
	struct snd_lsm_ep_det_thres *ep_det_data = NULL;
	struct lsm_param_epd_thres epd_thres;
	int rc = 0;

	memset(&epd_thres, 0, sizeof(epd_thres));
	param_info->param_size = sizeof(epd_thres);

	ep_det_data = (struct snd_lsm_ep_det_thres *) data;
	epd_thres.minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	epd_thres.epd_begin = ep_det_data->epd_begin;
	epd_thres.epd_end = ep_det_data->epd_end;

	rc = q6lsm_pack_and_set_params(client, param_info,
				       (uint8_t *) &epd_thres,
				       LSM_SESSION_CMD_SET_PARAMS_V2);
	if (unlikely(rc))
		pr_err("%s: EPD_THRESHOLD failed, rc %d\n", __func__, rc);
	return rc;
}

static int q6lsm_send_param_gain(struct lsm_client *client, u16 gain,
				 struct param_hdr_v3 *param_info)
{
	struct lsm_param_gain lsm_gain;
	int rc = 0;

	memset(&lsm_gain, 0, sizeof(lsm_gain));
	param_info->param_size = sizeof(lsm_gain);

	lsm_gain.minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	lsm_gain.gain = gain;

	rc = q6lsm_pack_and_set_params(client, param_info,
				       (uint8_t *) &lsm_gain,
				       LSM_SESSION_CMD_SET_PARAMS_V2);
	if (unlikely(rc))
		pr_err("%s: LSM_GAIN CMD send failed, rc %d\n", __func__, rc);
	return rc;
}

/**
 * q6lsm_set_one_param -
 *       command for LSM set params
 *
 * @client: LSM client handle
 * p_info: Params info
 * data: payload based on param type
 * param_type: LSM param type
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_set_one_param(struct lsm_client *client,
			struct lsm_params_info_v2 *p_info,
			void *data, uint32_t param_type)
{
	struct param_hdr_v3 param_info;
	int rc = 0;

	memset(&param_info, 0, sizeof(param_info));

	switch (param_type) {
	case LSM_ENDPOINT_DETECT_THRESHOLD: {
		param_info.module_id = p_info->module_id;
		param_info.instance_id = p_info->instance_id;
		param_info.param_id = p_info->param_id;
		rc = q6lsm_send_param_epd_thres(client, data, &param_info);
		if (rc)
			pr_err("%s: LSM_ENDPOINT_DETECT_THRESHOLD failed, rc %d\n",
			       __func__, rc);
		break;
	}

	case LSM_OPERATION_MODE: {
		struct snd_lsm_detect_mode *det_mode = data;

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

		param_info.module_id = p_info->module_id;
		param_info.instance_id = p_info->instance_id;
		param_info.param_id = p_info->param_id;

		rc = q6lsm_send_param_opmode(client, &param_info,
					     LSM_SESSION_CMD_SET_PARAMS_V2);
		if (rc)
			pr_err("%s: OPERATION_MODE failed, rc %d\n",
				__func__, rc);
		break;
	}

	case LSM_GAIN: {
		struct snd_lsm_gain *lsm_gain = (struct snd_lsm_gain *) data;
		param_info.module_id = p_info->module_id;
		param_info.instance_id = p_info->instance_id;
		param_info.param_id = p_info->param_id;
		rc = q6lsm_send_param_gain(client, lsm_gain->gain, &param_info);
		if (rc)
			pr_err("%s: LSM_GAIN command failed, rc %d\n",
				__func__, rc);
		break;
	}

	case LSM_MIN_CONFIDENCE_LEVELS:
	case LSM_MULTI_SND_MODEL_CONFIDENCE_LEVELS:
		param_info.module_id = p_info->module_id;
		param_info.instance_id = p_info->instance_id;
		param_info.param_id = p_info->param_id;
		rc = q6lsm_send_confidence_levels(
			client, &param_info, LSM_SESSION_CMD_SET_PARAMS_V2,
			p_info->model_id);
		if (rc)
			pr_err("%s: %s cmd failed, rc %d\n",
				 __func__,
				 param_type == LSM_MIN_CONFIDENCE_LEVELS ?
				 "LSM_MIN_CONFIDENCE_LEVELS" :
				 "LSM_MULTI_SND_MODEL_CONFIDENCE_LEVELS", rc);
		break;
	case LSM_POLLING_ENABLE: {
		struct snd_lsm_poll_enable *lsm_poll_enable =
				(struct snd_lsm_poll_enable *) data;
		param_info.module_id = p_info->module_id;
		param_info.instance_id = p_info->instance_id;
		param_info.param_id = p_info->param_id;
		rc = q6lsm_send_param_polling_enable(
			client, lsm_poll_enable->poll_en, &param_info,
			LSM_SESSION_CMD_SET_PARAMS_V2);
		if (rc)
			pr_err("%s: POLLING ENABLE cmd failed, rc %d\n",
				 __func__, rc);
		break;
	}

	case LSM_REG_SND_MODEL:
	case LSM_REG_MULTI_SND_MODEL: {
		struct mem_mapping_hdr mem_hdr;
		u32 payload_size;
		struct lsm_sound_model *sm = NULL;

		memset(&mem_hdr, 0, sizeof(mem_hdr));

		if (q6common_is_instance_id_supported())
			payload_size = p_info->param_size +
				       sizeof(struct param_hdr_v3);
		else
			payload_size = p_info->param_size +
				       sizeof(struct param_hdr_v2);

		if (param_type == LSM_REG_MULTI_SND_MODEL) {
			list_for_each_entry(sm,
					    &client->stage_cfg[p_info->stage_idx].sound_models,
					    list) {
				pr_debug("%s: current snd_model: %d, looking for snd_model %d\n",
					__func__, sm->model_id, p_info->model_id);
				if (sm->model_id == p_info->model_id)
					break;
			}
		} else {
			sm = &client->stage_cfg[p_info->stage_idx].sound_model;
		}
		mem_hdr.data_payload_addr_lsw =
			lower_32_bits(sm->phys);
		mem_hdr.data_payload_addr_msw =
			msm_audio_populate_upper_32_bits(
				sm->phys),
		mem_hdr.mem_map_handle = sm->mem_map_handle;

		rc = q6lsm_set_params(client, &mem_hdr, NULL, payload_size,
				      LSM_SESSION_CMD_SET_PARAMS_V2);
		if (rc) {
			pr_err("%s: %s failed, rc %d\n",
				__func__, param_type == LSM_REG_SND_MODEL ?
				"LSM_REG_SND_MODEL" : "LSM_REG_MULTI_SND_MODEL", rc);
			return rc;
		}

		if (client->num_sound_models == 0) {
			rc = q6lsm_send_cal(client, LSM_SESSION_CMD_SET_PARAMS, p_info);
			if (rc)
				pr_err("%s: Failed to send lsm cal, err = %d\n",
					__func__, rc);
		}
		break;
	}

	case LSM_DEREG_SND_MODEL:
	case LSM_DEREG_MULTI_SND_MODEL: {
		param_info.module_id = p_info->module_id;
		param_info.instance_id = p_info->instance_id;
		param_info.param_id = p_info->param_id;
		param_info.param_size = 0;

		if (param_type == LSM_DEREG_MULTI_SND_MODEL) {
			param_info.param_size = p_info->param_size;
			rc = q6lsm_pack_and_set_params(client, &param_info,
							(uint8_t *)&p_info->model_id,
							LSM_SESSION_CMD_SET_PARAMS_V2);
		} else {
			rc = q6lsm_pack_and_set_params(client, &param_info, NULL,
							LSM_SESSION_CMD_SET_PARAMS_V2);
		}
		if (rc)
			pr_err("%s: %s failed, rc %d\n",
				__func__,  param_type == LSM_DEREG_SND_MODEL ?
				"LSM_DEREG_SND_MODEL" : "LSM_DEREG_MULTI_SND_MODEL", rc);
		break;
	}

	case LSM_CUSTOM_PARAMS: {
		u32 param_size = p_info->param_size;

		/* Check minimum size, V2 structure is smaller than V3 */
		if (param_size < sizeof(struct param_hdr_v2)) {
			pr_err("%s: Invalid param_size %d\n", __func__,
			       param_size);
			return -EINVAL;
		}

		rc = q6lsm_set_params(client, NULL, data, param_size,
				      LSM_SESSION_CMD_SET_PARAMS_V2);
		if (rc)
			pr_err("%s: CUSTOM_PARAMS failed, rc %d\n",
				__func__, rc);
		break;
	}

	case LSM_DET_EVENT_TYPE: {
		struct lsm_param_det_event_type det_event_type;
		struct snd_lsm_det_event_type *det_event_data =
					(struct snd_lsm_det_event_type *)data;

		param_info.module_id = p_info->module_id;
		param_info.instance_id = p_info->instance_id;
		param_info.param_id = p_info->param_id;
		param_info.param_size = sizeof(det_event_type);

		memset(&det_event_type, 0, sizeof(det_event_type));

		det_event_type.minor_version = QLSM_PARAM_ID_MINOR_VERSION;
		det_event_type.event_type = det_event_data->event_type;
		det_event_type.mode = det_event_data->mode;

		rc = q6lsm_pack_and_set_params(client, &param_info,
					       (uint8_t *)&det_event_type,
					       LSM_SESSION_CMD_SET_PARAMS_V2);
		if (rc)
			pr_err("%s: DET_EVENT_TYPE cmd failed, rc %d\n",
				 __func__, rc);
		break;
	}

	default:
		pr_err("%s: wrong param_type 0x%x\n",
			__func__, p_info->param_type);
	}

	return rc;
}
EXPORT_SYMBOL(q6lsm_set_one_param);

int q6lsm_get_one_param(struct lsm_client *client,
		struct lsm_params_get_info *p_info,
		uint32_t param_type)
{
	struct param_hdr_v3 param_info;
	int rc = 0;
	bool iid_supported = q6common_is_instance_id_supported();

	memset(&param_info, 0, sizeof(param_info));

	switch (param_type) {
	case LSM_GET_CUSTOM_PARAMS: {
		param_info.module_id = p_info->module_id;
		param_info.instance_id = p_info->instance_id;
		param_info.param_id = p_info->param_id;

		if (iid_supported)
			param_info.param_size = p_info->param_size + sizeof(struct param_hdr_v3);
		else
			param_info.param_size = p_info->param_size + sizeof(struct param_hdr_v2);

		rc = q6lsm_get_params(client, NULL, &param_info);
		if (rc) {
			pr_err("%s: LSM_GET_CUSTOM_PARAMS failed, rc %d\n",
				__func__, rc);
		}
		break;

	}
	default:
		pr_err("%s: wrong param_type 0x%x\n",
			__func__, p_info->param_type);
	}
	return rc;
}
EXPORT_SYMBOL(q6lsm_get_one_param);

/**
 * q6lsm_start -
 *       command for LSM start
 *
 * @client: LSM client handle
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_start(struct lsm_client *client, bool wait)
{
	return q6lsm_cmd(client, LSM_SESSION_CMD_START, wait);
}
EXPORT_SYMBOL(q6lsm_start);

/**
 * q6lsm_stop -
 *       command for LSM stop
 *
 * @client: LSM client handle
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_stop(struct lsm_client *client, bool wait)
{
	return q6lsm_cmd(client, LSM_SESSION_CMD_STOP, wait);
}
EXPORT_SYMBOL(q6lsm_stop);

/**
 * q6lsm_close -
 *       command for LSM close
 *
 * @client: LSM client handle
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_close(struct lsm_client *client)
{
	return q6lsm_cmd(client, LSM_SESSION_CMD_CLOSE_TX, true);
}
EXPORT_SYMBOL(q6lsm_close);

/**
 * q6lsm_lab_control -
 *       command to set LSM LAB control params
 *
 * @client: LSM client handle
 * @enable: bool flag  to enable or disable LAB on DSP
 * @p_info: param info to be used for sending lab control param
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_lab_control(struct lsm_client *client, u32 enable,
		struct lsm_params_info_v2 *p_info)
{
	struct lsm_param_lab_enable lab_enable;
	struct param_hdr_v3 lab_enable_hdr;
	struct lsm_param_lab_config lab_config;
	struct param_hdr_v3 lab_config_hdr;
	int rc = 0;

	memset(&lab_enable, 0, sizeof(lab_enable));
	memset(&lab_enable_hdr, 0, sizeof(lab_enable_hdr));
	memset(&lab_config, 0, sizeof(lab_config));
	memset(&lab_config_hdr, 0, sizeof(lab_config_hdr));

	if (!client) {
		pr_err("%s: invalid param client %pK\n", __func__, client);
		return -EINVAL;
	}

	/* enable/disable lab on dsp */
	lab_enable_hdr.module_id = p_info->module_id;
	lab_enable_hdr.instance_id = p_info->instance_id;
	lab_enable_hdr.param_id = LSM_PARAM_ID_LAB_ENABLE;
	lab_enable_hdr.param_size = sizeof(lab_enable);
	lab_enable.enable = (enable) ? 1 : 0;
	rc = q6lsm_pack_and_set_params(client, &lab_enable_hdr,
				       (uint8_t *) &lab_enable,
				       LSM_SESSION_CMD_SET_PARAMS);
	if (rc) {
		pr_err("%s: Lab enable failed rc %d\n", __func__, rc);
		return rc;
	}
	if (!enable)
		goto exit;

	/* lab session is being enabled set the config values */
	lab_config_hdr.module_id = p_info->module_id;
	lab_config_hdr.instance_id = p_info->instance_id;
	lab_config_hdr.param_id = LSM_PARAM_ID_LAB_CONFIG;
	lab_config_hdr.param_size = sizeof(lab_config);
	lab_config.minor_version = 1;
	lab_config.wake_up_latency_ms = 40;
	rc = q6lsm_pack_and_set_params(client, &lab_config_hdr,
				       (uint8_t *) &lab_config,
				       LSM_SESSION_CMD_SET_PARAMS);
	if (rc) {
		pr_err("%s: Lab config failed rc %d disable lab\n",
		 __func__, rc);
		/* Lab config failed disable lab */
		lab_enable.enable = 0;
		if (q6lsm_pack_and_set_params(client, &lab_enable_hdr,
					      (uint8_t *) &lab_enable,
					      LSM_SESSION_CMD_SET_PARAMS))
			pr_err("%s: Lab disable failed\n", __func__);
	}
exit:
	return rc;
}
EXPORT_SYMBOL(q6lsm_lab_control);

/*
 * q6lsm_lab_out_ch_cfg -
 *	Command to set the channel configuration
 *	for look-ahead buffer.
 *
 * @client: LSM client handle
 * @ch_map: Channel map indicating the order
 *	    of channels to be configured.
 * @p_info: param info to be used for sending lab config param
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_lab_out_ch_cfg(struct lsm_client *client,
			 u8 *ch_map, struct lsm_params_info_v2 *p_info)
{
	u8 *param_buf;
	struct lsm_param_lab_out_ch_cfg *lab_out_cfg;
	struct param_hdr_v3 lab_out_cfg_hdr;
	struct lsm_hw_params *out_params = &client->out_hw_params;
	int i, rc = 0, param_len = 0;

	param_len = sizeof(*lab_out_cfg) +
		    sizeof(u8) * out_params->num_chs;

	if (param_len % 4)
		param_len += (4 - (param_len % 4));

	param_buf = kzalloc(param_len, GFP_KERNEL);
	if (!param_buf)
		return -ENOMEM;

	lab_out_cfg = (struct lsm_param_lab_out_ch_cfg *) param_buf;
	lab_out_cfg->minor_version = QLSM_PARAM_ID_MINOR_VERSION;
	lab_out_cfg->num_channels = out_params->num_chs;

	for (i = 0; i < out_params->num_chs; i++)
		lab_out_cfg->channel_indices[i] = ch_map[i];

	memset(&lab_out_cfg_hdr, 0, sizeof(lab_out_cfg_hdr));
	lab_out_cfg_hdr.module_id = p_info->module_id;
	lab_out_cfg_hdr.instance_id = p_info->instance_id;
	lab_out_cfg_hdr.param_id = LSM_PARAM_ID_LAB_OUTPUT_CHANNEL_CONFIG;
	lab_out_cfg_hdr.param_size = param_len;

	rc = q6lsm_pack_and_set_params(client, &lab_out_cfg_hdr,
				       param_buf,
				       LSM_SESSION_CMD_SET_PARAMS_V2);
	if (rc)
		pr_err("%s: Lab out channel config failed %d\n",
			__func__, rc);

	kfree(param_buf);

	return rc;
}
EXPORT_SYMBOL(q6lsm_lab_out_ch_cfg);

/**
 * q6lsm_stop_lab -
 *       command to stop LSM LAB
 *
 * @client: LSM client handle
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_stop_lab(struct lsm_client *client)
{
	int rc = 0;

	if (!client) {
		pr_err("%s: invalid param client %pK\n", __func__, client);
		return -EINVAL;
	}
	rc = q6lsm_cmd(client, LSM_SESSION_CMD_EOB, true);
	if (rc)
		pr_err("%s: Lab stop failed %d\n", __func__, rc);
	return rc;
}
EXPORT_SYMBOL(q6lsm_stop_lab);

/**
 * q6lsm_read -
 *       command for LSM read
 *
 * @client: LSM client handle
 * @lsm_cmd_read: LSM read command
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_read(struct lsm_client *client, struct lsm_cmd_read *read)
{
	int rc = 0;

	if (!client || !read) {
		pr_err("%s: Invalid params client %pK read %pK\n", __func__,
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
EXPORT_SYMBOL(q6lsm_read);

/**
 * q6lsm_lab_buffer_alloc -
 *       Lab buffer allocation or de-alloc
 *
 * @client: LSM client handle
 * @alloc: Allocate or free ion memory
 *
 * Returns 0 on success or error on failure
 */
int q6lsm_lab_buffer_alloc(struct lsm_client *client, bool alloc)
{
	int ret = 0, i = 0;
	size_t allocate_size = 0, len = 0;
	struct lsm_hw_params *out_params = &client->out_hw_params;

	if (!client) {
		pr_err("%s: invalid client\n", __func__);
		return -EINVAL;
	}
	if (alloc) {
		if (client->lab_buffer) {
			pr_err("%s: buffers are allocated period count %d period size %d\n",
				__func__,
				out_params->period_count,
				out_params->buf_sz);
			return -EINVAL;
		}
		allocate_size = out_params->period_count *
				out_params->buf_sz;
		allocate_size = PAGE_ALIGN(allocate_size);
		client->lab_buffer =
			kzalloc(sizeof(struct lsm_lab_buffer) *
			out_params->period_count, GFP_KERNEL);
		if (!client->lab_buffer) {
			pr_err("%s: memory allocation for lab buffer failed count %d\n"
				, __func__,
				out_params->period_count);
			return -ENOMEM;
		}
		ret = msm_audio_ion_alloc(&client->lab_buffer[0].dma_buf,
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
				client->lab_buffer[0].dma_buf);
			}
		}
		if (ret) {
			pr_err("%s: alloc lab buffer failed ret %d\n",
				__func__, ret);
			kfree(client->lab_buffer);
			client->lab_buffer = NULL;
		} else {
			pr_debug("%s: Memory map handle %x phys %pK size %d\n",
				__func__,
				client->lab_buffer[0].mem_map_handle,
				&client->lab_buffer[0].phys,
				out_params->buf_sz);

			for (i = 0; i < out_params->period_count; i++) {
				client->lab_buffer[i].phys =
				client->lab_buffer[0].phys +
				(i * out_params->buf_sz);
				client->lab_buffer[i].size =
				out_params->buf_sz;
				client->lab_buffer[i].data =
				(u8 *)(client->lab_buffer[0].data) +
				(i * out_params->buf_sz);
				client->lab_buffer[i].mem_map_handle =
				client->lab_buffer[0].mem_map_handle;
			}
		}
	} else {
		ret = q6lsm_memory_unmap_regions(client,
			client->lab_buffer[0].mem_map_handle);
		if (!ret)
			msm_audio_ion_free(client->lab_buffer[0].dma_buf);
		else
			pr_err("%s: unmap failed not freeing memory\n",
			__func__);
		kfree(client->lab_buffer);
		client->lab_buffer = NULL;
	}
	return ret;
}
EXPORT_SYMBOL(q6lsm_lab_buffer_alloc);

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
	int ret = 0;
	int cal_index;

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
	int ret = 0;
	int cal_index;

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
	int ret = 0;
	int cal_index;

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

int __init q6lsm_init(void)
{
	int i = 0;

	pr_debug("%s:\n", __func__);

	memset(&lsm_common, 0, sizeof(lsm_common));
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

void q6lsm_exit(void)
{
	lsm_delete_cal_data();
}
