// SPDX-License-Identifier: GPL-2.0
/*
 *  vow_scp.c  --  VoW SCP
 *
 *  Copyright (c) 2020 MediaTek Inc.
 *  Author: Michael HSiao <michael.hsiao@mediatek.com>
 */

/*****************************************************************************
 * Header Files
 *****************************************************************************/
#include <linux/delay.h>
#include "vow_scp.h"
#include "vow.h"
#include "vow_assert.h"
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include "scp.h"
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
static int vow_ipi_recv_handler(unsigned int id,
				void *prdata,
				void *data,
				unsigned int len);
static int vow_ipi_ack_handler(unsigned int id,
			       void *prdata,
			       void *data,
			       unsigned int len);
#endif

unsigned int ipi_ack_return;
unsigned int ipi_ack_id;
unsigned int ipi_ack_data;

struct vow_ipi_receive_info vow_ipi_receive;
struct vow_ipi_ack_info vow_ipi_send_ack;
void (*ipi_rx_handle)(unsigned int a, void *b);
bool (*ipi_tx_ack_handle)(unsigned int a, unsigned int b);

/*****************************************************************************
 * Function
 ****************************************************************************/
unsigned int vow_check_scp_status(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	return is_scp_ready(SCP_A_ID);
#else
	return 0;
#endif
}

void vow_ipi_register(void (*ipi_rx_call)(unsigned int, void *),
		      bool (*ipi_tx_ack_call)(unsigned int, unsigned int))
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	mtk_ipi_register(&scp_ipidev, IPI_IN_AUDIO_VOW_1,
			(void *)vow_ipi_recv_handler, NULL,
			 &vow_ipi_receive);
	mtk_ipi_register(&scp_ipidev, IPI_IN_AUDIO_VOW_ACK_1,
			(void *)vow_ipi_ack_handler, NULL,
			 &vow_ipi_send_ack);
#endif
	ipi_rx_handle = ipi_rx_call;
	ipi_tx_ack_handle = ipi_tx_ack_call;
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
static int vow_ipi_recv_handler(unsigned int id,
				void *prdata,
				void *data,
				unsigned int len)
{
	struct vow_ipi_receive_info *ipi_info =
		(struct vow_ipi_receive_info *)data;

	ipi_rx_handle(ipi_info->msg_id, (void *)ipi_info->msg_data);
	return 0;
}

static int vow_ipi_ack_handler(unsigned int id,
			       void *prdata,
			       void *data,
			       unsigned int len)
{
	struct vow_ipi_ack_info *ipi_info =
		(struct vow_ipi_ack_info *)data;

	ipi_tx_ack_handle(ipi_info->msg_id, ipi_info->msg_data);
	ipi_ack_return = ipi_info->msg_need_ack;
	ipi_ack_id = ipi_info->msg_id;
	ipi_ack_data = ipi_info->msg_data;
	return 0;
}
#endif

bool vow_ipi_send(unsigned int msg_id,
		  unsigned int payload_len,
		  unsigned int *payload,
		  unsigned int need_ack)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	bool ret = false;
	int ipi_result = -1;
	unsigned int retry_time = VOW_IPI_SEND_CNT_TIMEOUT;
	unsigned int retry_cnt = 0;
	unsigned int ack_time = VOW_IPI_WAIT_ACK_TIMEOUT;
	unsigned int ack_cnt = 0;
	unsigned int msg_need_ack = 0;
	unsigned int resend_cnt = 0;
	struct vow_ipi_send_info ipi_data;

	if (!vow_check_scp_status()) {
		VOWDRV_DEBUG("SCP is off, bypass send ipi id(%d)\n", msg_id);
		return false;
	}
	if (vow_service_GetScpRecoverStatus() == true) {
		VOWDRV_DEBUG("scp is recovering, then break\n");
		return false;
	}

	/* clear send buffer */
	memset(&ipi_data.payload[0], 0,
		sizeof(unsigned int) * VOW_IPI_SEND_BUFFER_LENGTH);

	resend_cnt = 0;
	msg_need_ack = need_ack;
	ipi_data.msg_id = msg_id;
	ipi_data.msg_need_ack = msg_need_ack;
	ipi_data.param1 = 0;
	ipi_data.param2 = 0;
	ipi_data.msg_length = payload_len;

	if (payload > 0) {
		/* have payload */
		memcpy(&ipi_data.payload[0], payload,
		       sizeof(unsigned int) * payload_len);
	}

RESEND_IPI:
	if (resend_cnt == VOW_IPI_RESEND_TIMES) {
		VOWDRV_DEBUG("%s(), resend over time, drop id:%d\n",
			__func__, msg_id);
		return false;
	}
	/* ipi ack reset */
	ipi_ack_return = 0;
	ipi_ack_id = 0xFF;
	ipi_ack_data = 0;

	for (retry_cnt = 0; retry_cnt <= retry_time; retry_cnt++) {
		ipi_result = mtk_ipi_send(&scp_ipidev,
					  IPI_OUT_AUDIO_VOW_1,
					  0,
					  &ipi_data,
					  PIN_OUT_SIZE_AUDIO_VOW_1,
					  0);
		if (ipi_result == IPI_ACTION_DONE) {
			if (retry_cnt != 0) {
				VOWDRV_DEBUG("%s(), ipi_id(%d) succeed after retry cnt =%d\n",
						 __func__,
						 msg_id,
						 retry_cnt);
			}
			break;
		} else {
			if (retry_cnt == retry_time) { // already retry max times
				VOWDRV_DEBUG("%s() ERROR, ipi_id(%d) Fail %d after retry cnt =%d\n",
						 __func__,
						 msg_id,
						 ipi_result,
						 retry_cnt);
			}
		}
		if (vow_service_GetScpRecoverStatus() == true) {
			VOWDRV_DEBUG("scp is recovering, then break\n");
			break;
		}
		msleep(VOW_WAITCHECK_INTERVAL_MS);
	}

	if (ipi_result == IPI_ACTION_DONE) {
		if (need_ack == VOW_IPI_NEED_ACK) {
			for (ack_cnt = 0; ack_cnt <= ack_time; ack_cnt++) {
				if ((ipi_ack_return == VOW_IPI_ACK_BACK) &&
				    (ipi_ack_id == msg_id)) {
					/* ack back */
					break;
				}
				if (ack_cnt >= ack_time) {
					/* no ack */
					VOWDRV_DEBUG("%s(), no ack\n",
						__func__);
					resend_cnt++;
					goto RESEND_IPI;
				}
				msleep(VOW_WAITCHECK_INTERVAL_MS);
			}
		}
		VOWDRV_DEBUG("%s(), ipi_id(%d) pass\n", __func__, msg_id);
		ret = true;
	}
	return ret;
#else
	(void) msg_id;
	(void) payload_len;
	(void) payload;
	(void) need_ack;
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
	return false;
#endif
}
EXPORT_SYMBOL_GPL(vow_ipi_send);
