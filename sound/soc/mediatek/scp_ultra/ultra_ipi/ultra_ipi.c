/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


/*****************************************************************************
 * Header Files
 *****************************************************************************/
#include <linux/delay.h>
#include "ultra_ipi.h"
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#include <mt-plat/mtk_tinysys_ipi.h>
#include "scp_ipi_pin.h"
#include "scp_mbox_layout.h"  /* for IPI mbox size */
#endif

static int ultra_ipi_recv_handler(unsigned int id,
				 void *prdata,
				 void *data,
				 unsigned int len);
static int ultra_ipi_ack_handler(unsigned int id,
				void *prdata,
				void *data,
				unsigned int len);

static unsigned int ipi_ack_return;
static unsigned int ipi_ack_id;
static unsigned int ipi_ack_data;

static bool scp_recovering;

struct ultra_ipi_receive_info ultra_ipi_receive;
struct ultra_ipi_ack_info ultra_ipi_send_ack;
void (*ultra_ipi_rx_handle)(unsigned int a, void *b);
bool (*ultra_ipi_tx_ack_handle)(unsigned int a, unsigned int b);

/*****************************************************************************
 * Function
 ****************************************************************************/
unsigned int ultra_check_scp_status(void)
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	return is_scp_ready(SCP_A_ID);
#else
	return 0;
#endif
}

bool ultra_GetScpRecoverStatus(void)
{
	return scp_recovering;
}

void ultra_SetScpRecoverStatus(bool recovering)
{
	scp_recovering = recovering;
}



void ultra_ipi_register(void (*ipi_rx_call)(unsigned int, void *),
			bool (*ipi_tx_ack_call)(unsigned int, unsigned int))
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	mtk_ipi_register(&scp_ipidev, IPI_IN_AUDIO_ULTRA_SND_0,
			(mbox_pin_cb_t)ultra_ipi_recv_handler, NULL,
			 &ultra_ipi_receive);
	mtk_ipi_register(&scp_ipidev, IPI_IN_AUDIO_ULTRA_SND_ACK_0,
			(mbox_pin_cb_t)ultra_ipi_ack_handler, NULL,
			&ultra_ipi_send_ack);
#endif
	ultra_ipi_rx_handle = ipi_rx_call;
	ultra_ipi_tx_ack_handle = ipi_tx_ack_call;
}

static int ultra_ipi_recv_handler(unsigned int id,
				 void *prdata,
				 void *data,
				 unsigned int len)
{
	struct ultra_ipi_receive_info *ipi_info =
		(struct ultra_ipi_receive_info *)data;

	ultra_ipi_rx_handle(ipi_info->msg_id, (void *)ipi_info->msg_data);
	return 0;
}

static int ultra_ipi_ack_handler(unsigned int id,
				  void *prdata,
				  void *data,
				  unsigned int len)
{
	struct ultra_ipi_ack_info *ipi_info =
		(struct ultra_ipi_ack_info *)data;

	ultra_ipi_tx_ack_handle(ipi_info->msg_id, ipi_info->msg_data);
	ipi_ack_return = ipi_info->msg_need_ack;
	ipi_ack_id = ipi_info->msg_id;
	ipi_ack_data = ipi_info->msg_data;
	pr_info("%s(), ipi_ack_return(%d), ipi_ack_id(%d)\n",
		__func__, ipi_ack_return, ipi_ack_id);
	return 0;
}

bool ultra_ipi_send(unsigned int msg_id,
		    bool polling_mode,
		    unsigned int payload_len,
		    int *payload,
		    unsigned int need_ack)
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	bool ret = false;
	int ipi_result = -1;
	unsigned int retry_time = ULTRA_IPI_SEND_CNT_TIMEOUT;
	unsigned int retry_cnt = 0;
	unsigned int ack_time = ULTRA_IPI_WAIT_ACK_TIMEOUT;
	unsigned int ack_cnt = 0;
	unsigned int msg_need_ack = 0;
	unsigned int resend_cnt = 0;
	struct ultra_ipi_send_info ipi_data;

	if (!ultra_check_scp_status()) {
		pr_err("SCP is off, bypass send ipi id(%d)\n", msg_id);
		return false;
	}
	// TODO: Should handle scp resovery

	if (ultra_GetScpRecoverStatus() == true) {
		pr_info("scp is recovering, then break\n");
		return false;
	}

	/* clear send buffer */
	memset(&ipi_data.payload[0], 0,
		sizeof(int) * ULTRA_IPI_SEND_BUFFER_LENGTH);

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
	if (resend_cnt == ULTRA_IPI_RESEND_TIMES) {
		pr_err("%s(), resend over time, drop id:%d\n",
			__func__, msg_id);
		return false;
	}
	/* ipi ack reset */
	ipi_ack_return = 0;
	ipi_ack_id = 0xFF;
	ipi_ack_data = 0;

	for (retry_cnt = 0; retry_cnt <= retry_time; retry_cnt++) {
		ipi_result = mtk_ipi_send(&scp_ipidev,
			IPI_OUT_AUDIO_ULTRA_SND_0,
			polling_mode ? IPI_SEND_POLLING : IPI_SEND_WAIT,
			&ipi_data,
			PIN_OUT_SIZE_AUDIO_ULTRA_SND_0,
			0);
		if (ipi_result == IPI_ACTION_DONE)
			break;

		/* send error, print it */
		pr_info("%s(), ipi_id(%d) fail=%d\n",
			     __func__,
			     msg_id,
			     ipi_result);

		if (ultra_GetScpRecoverStatus() == true) {
			pr_info("scp is recovering, then break\n");
			break;
		}
		if (!polling_mode)
			msleep(ULTRA_WAITCHECK_INTERVAL_MS);
	}

	if (ipi_result == IPI_ACTION_DONE) {
		if (need_ack == ULTRA_IPI_NEED_ACK) {
			for (ack_cnt = 0; ack_cnt <= ack_time; ack_cnt++) {
				if ((ipi_ack_return == ULTRA_IPI_ACK_BACK) &&
					(ipi_ack_id == msg_id)) {
					/* ack back */
					break;
				}
				if (ack_cnt >= ack_time) {
					/* no ack */
					pr_info("%s(), no ack, ipi_id=%d\n",
						__func__, msg_id);
					resend_cnt++;
					goto RESEND_IPI;
				}
				if (!polling_mode)
					msleep(ULTRA_WAITCHECK_INTERVAL_MS);
			}
		}
		ret = true;
	}
	pr_info("%s(), ipi_id=%d,ret=%d,need_ack=%d,ack_return=%d,ack_id=%d\n",
		__func__, msg_id, ret, need_ack, ipi_ack_return, ipi_ack_id);
	return ret;
#else
	(void) msg_id;
	(void) payload_len;
	(void) payload;
	(void) need_ack;
	pr_info("ultra:SCP no support\n\r");
	return false;
#endif
}

