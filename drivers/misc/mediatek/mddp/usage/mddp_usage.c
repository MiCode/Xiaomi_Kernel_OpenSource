// SPDX-License-Identifier: GPL-2.0
/*
 * mddp_usage.c - Data usage API.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>

#include "mddp_debug.h"
#include "mddp_dev.h"
#include "mddp_filter.h"
#include "mddp_ipc.h"
#include "mddp_usage.h"
#include "mtk_ccci_common.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Private helper macro.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Function prototype.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Private variables.
//------------------------------------------------------------------------------
static uint32_t mddp_u_iq_trans_id_s;

//------------------------------------------------------------------------------
// Private helper macro.
//------------------------------------------------------------------------------
#define MDDP_U_GET_IQ_TRANS_ID()        (mddp_u_iq_trans_id_s++)

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public functions - WiFi Hotspot.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
int32_t mddp_usage_init(void)
{
	return 0;
}

void mddp_usage_uninit(void)
{

}

void mddp_u_get_data_stats(void *buf, uint32_t *buf_len)
{
	static struct mddp_u_data_stats_t       cur_stats = {0};
	struct mddp_u_data_stats_t             *md_stats;
	struct mddp_u_data_stats_t             *usage;
	uint32_t                                sm_len = 0;

	md_stats = get_smem_start_addr(MD_SYS1, SMEM_USER_RAW_NETD, &sm_len);

	if (sm_len >= sizeof(struct mddp_u_data_stats_t)) {
		usage = (struct mddp_u_data_stats_t *)buf;
		*buf_len = sizeof(struct mddp_u_data_stats_t);
		#define DIFF_FROM_SMEM(x) (usage->x = \
		(md_stats->x > cur_stats.x) ? (md_stats->x - cur_stats.x) : 0)
		DIFF_FROM_SMEM(total_tx_pkts);
		DIFF_FROM_SMEM(total_tx_bytes);
		DIFF_FROM_SMEM(tx_tcp_bytes);
		DIFF_FROM_SMEM(tx_tcp_pkts);
		DIFF_FROM_SMEM(tx_udp_bytes);
		DIFF_FROM_SMEM(tx_udp_pkts);
		DIFF_FROM_SMEM(tx_others_bytes);
		DIFF_FROM_SMEM(tx_others_pkts);
		DIFF_FROM_SMEM(total_rx_pkts);
		DIFF_FROM_SMEM(total_rx_bytes);
		DIFF_FROM_SMEM(rx_tcp_bytes);
		DIFF_FROM_SMEM(rx_tcp_pkts);
		DIFF_FROM_SMEM(rx_udp_bytes);
		DIFF_FROM_SMEM(rx_udp_pkts);
		DIFF_FROM_SMEM(rx_others_bytes);
		DIFF_FROM_SMEM(rx_others_pkts);

		memcpy(&cur_stats, md_stats, *buf_len);
	} else {
		MDDP_U_LOG(MDDP_LL_ERR,
				"%s: Failed to copy data stats, sm_len(%d), buf_len(%d)!\n",
				__func__, sm_len, *buf_len);
		*buf_len = 0;
	}
}

int32_t mddp_u_set_data_limit(uint8_t *buf, uint32_t buf_len)
{
	uint32_t                                md_status;
	struct mddp_md_msg_t                   *md_msg;
	struct mddp_dev_req_set_data_limit_t   *in_req;
	struct mddp_u_data_limit_t              limit;
	int8_t                                  id;

	if (buf_len != sizeof(struct mddp_dev_req_set_data_limit_t)) {
		MDDP_U_LOG(MDDP_LL_ERR,
				"%s: Invalid parameter, buf_len(%d)!\n",
				__func__, buf_len);
		WARN_ON(1);
		return -EINVAL;
	}

	md_status = exec_ccci_kern_func_by_md_id(0, ID_GET_MD_STATE, NULL, 0);

	if (md_status != MD_STATE_READY) {
		MDDP_U_LOG(MDDP_LL_NOTICE,
				"%s: Invalid state, md_status(%d)!\n",
				__func__, md_status);
		return -ENODEV;
	}

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) + sizeof(limit),
			GFP_ATOMIC);
	if (unlikely(!md_msg)) {
		WARN_ON(1);
		return -EAGAIN;
	}

	in_req = (struct mddp_dev_req_set_data_limit_t *)buf;
	id = mddp_f_data_usage_wan_dev_name_to_id(in_req->ul_dev_name);
	if (unlikely(id < 0)) {
		MDDP_U_LOG(MDDP_LL_ERR,
				"%s: Invalid dev_name, dev_name(%s)!\n",
				__func__, in_req->ul_dev_name);
		WARN_ON(1);
		return -EINVAL;
	}

	memset(&limit, 0, sizeof(limit));
	limit.cmd = MSG_ID_DPFM_SET_IQUOTA_REQ;
	limit.trans_id = MDDP_U_GET_IQ_TRANS_ID();
	limit.limit_buffer_size = in_req->limit_size;
	limit.id = id;
	MDDP_U_LOG(MDDP_LL_NOTICE,
			"%s: Send cmd(%d)/id(%d)/name(%s) limit(%llx) to MD.\n",
			__func__, limit.cmd, limit.id, in_req->ul_dev_name,
			limit.limit_buffer_size);

	md_msg->msg_id = IPC_MSG_ID_DPFM_DATA_USAGE_CMD;
	md_msg->data_len = sizeof(limit);
	memcpy(md_msg->data, &limit, sizeof(limit));
	mddp_ipc_send_md(NULL, md_msg, MDFPM_USER_ID_DPFM);

	return 0;
}

int32_t mddp_u_msg_hdlr(uint32_t msg_id, void *buf, uint32_t buf_len)
{
	int32_t                     ret = -EINVAL;
	struct mddp_u_iquota_ind_t *ind;

	if (msg_id == IPC_MSG_ID_DPFM_DATA_USAGE_CMD) {
		ind = buf;
		switch (ind->cmd) {
		case MSG_ID_DPFM_ALERT_IQUOTA_IND:
			mddp_dev_response(MDDP_APP_TYPE_ALL,
				MDDP_CMCMD_LIMIT_IND, true, NULL, 0);
			ret = 0;
			break;
		default:
			break;
		}
	}

	return ret;
}
