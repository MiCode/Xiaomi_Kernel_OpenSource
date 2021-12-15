/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 * Header Files
 *****************************************************************************/
#include <linux/delay.h>
#include "ultra_ipi.h"
#include "audio_messenger_ipi.h"
#include "scp_ipi.h"
#include "audio_task_manager.h"
#include "audio_task.h"
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#include <mt-plat/mtk_tinysys_ipi.h>
#endif
static void ultra_ipi_IPICmd_Received(struct ipi_msg_t *ipi_msg);
static bool ultra_ipi_IPICmd_ReceiveAck(struct ipi_msg_t *ipi_msg);
static void ultra_ipi_Unloaded_Handling(void);
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
static void ultra_ipi_Unloaded_Handling(void)
{
	pr_info("%s()\n", __func__);
}
static void ultra_ipi_IPICmd_Received(struct ipi_msg_t *ipi_msg)
{
	pr_info("%s(),msg_id=%d\n", ipi_msg->msg_id);
	ultra_ipi_rx_handle(ipi_msg->msg_id, (void *)ipi_msg->payload);
}
static bool ultra_ipi_IPICmd_ReceiveAck(struct ipi_msg_t *ipi_msg)
{
	pr_info("%s(),msg_id=%d\n", ipi_msg->msg_id);
	ultra_ipi_tx_ack_handle(ipi_msg->msg_id, 0);
	return true;
}
void ultra_ipi_register(void (*ipi_rx_call)(unsigned int, void *),
			bool (*ipi_tx_ack_call)(unsigned int, unsigned int))
{
	audio_task_register_callback(TASK_SCENE_VOICE_ULTRASOUND,
					ultra_ipi_IPICmd_Received, ultra_ipi_Unloaded_Handling);
	ultra_ipi_rx_handle = ipi_rx_call;
	ultra_ipi_tx_ack_handle = ipi_tx_ack_call;
}

bool ultra_ipi_send(unsigned int msg_id,
		    bool polling_mode,
		    unsigned int payload_len,
		    int *payload,
		    unsigned int need_ack)
{
	bool ret = false;
	struct ipi_msg_t ipi_msg;
	int ipi_result = -1;
	unsigned int retry_time = ULTRA_IPI_SEND_CNT_TIMEOUT;
	unsigned int retry_cnt;
	uint8_t data_type = 0;
	uint8_t ack_type = AUDIO_IPI_MSG_BYPASS_ACK;
	uint32_t param1 = sizeof(unsigned int) * payload_len;
	uint32_t param2 = 0;

	if (!ultra_check_scp_status()) {
		pr_err("SCP is off, bypass send ipi id(%d)\n", msg_id);
		return false;
	}
	// TODO: Should handle scp resovery
	if (ultra_GetScpRecoverStatus() == true) {
		pr_info("scp is recovering, then break\n");
		return false;
	}
	ack_type = need_ack;
	if (payload == NULL)
		data_type = AUDIO_IPI_MSG_ONLY;
	else
		data_type = AUDIO_IPI_PAYLOAD;
	for (retry_cnt = 0; retry_cnt <= retry_time; retry_cnt++) {
		ipi_result = audio_send_ipi_msg(&ipi_msg,
						TASK_SCENE_VOICE_ULTRASOUND,
						AUDIO_IPI_LAYER_TO_DSP,
						data_type,
						ack_type,
						msg_id,
						param1,
						param2,
						payload);
		if (ipi_result == 0)
			break;
		if (ultra_GetScpRecoverStatus() == true) {
			pr_debug("scp is recovering, then break\n");
			break;
		}
		if (!polling_mode)
			msleep(ULTRA_WAITCHECK_INTERVAL_MS);
	}
	if (ipi_result == 0) {
		/* ipi send pass */
		if (ipi_msg.ack_type == AUDIO_IPI_MSG_ACK_BACK)
			ret = ultra_ipi_IPICmd_ReceiveAck(&ipi_msg);
		else
			ret = true;
	}
	pr_info("%s(), ipi_id=%d,ret=%d,need_ack=%d,ack_return=%d\n",
		__func__, msg_id, ipi_result, need_ack, ret);
	return ret;
}

