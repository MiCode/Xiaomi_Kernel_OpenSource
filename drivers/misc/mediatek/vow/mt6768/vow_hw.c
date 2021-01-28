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
#include "vow_hw.h"
#include "vow.h"
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#include "scp_ipi.h"
#include "audio_task_manager.h"
#include "audio_ipi_queue.h"
#endif


static void vow_IPICmd_Received(struct ipi_msg_t *ipi_msg);
static bool vow_IPICmd_ReceiveAck(struct ipi_msg_t *ipi_msg);

/* ipi */
void (*ipi_rx_handle)(unsigned int a, void *b);
bool (*ipi_tx_ack_handle)(unsigned int a, unsigned int b);

/*****************************************************************************
 * Function
 ****************************************************************************/
unsigned int vow_check_scp_status(void)
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	return is_scp_ready(SCP_A_ID);
#else
	return 0;
#endif
}

void vow_ipi_register(void (*ipi_rx_call)(unsigned int, void *),
		      bool (*ipi_tx_ack_call)(unsigned int, void *))
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	audio_load_task(TASK_SCENE_VOW);
	audio_task_register_callback(TASK_SCENE_VOW,
				     vow_IPICmd_Received,
				     vow_Task_Unloaded_Handling);
#endif
	ipi_rx_handle = ipi_rx_call;
	ipi_tx_ack_handle = ipi_tx_ack_call;
}

static void vow_Task_Unloaded_Handling(void)
{
	VOWDRV_DEBUG("%s()\n", __func__);
}

static void vow_IPICmd_Received(struct ipi_msg_t *ipi_msg)
{
	ipi_rx_handle(ipi_msg->msg_id, (void *)ipi_msg->payload);
}

static bool vow_IPICmd_ReceiveAck(struct ipi_msg_t *ipi_msg)
{
	bool result = false;

	result = ipi_tx_ack_handle(ipi_msg->msg_id, ipi_msg->param2);

	return result;
}

bool vow_ipi_send(unsigned int msg_id,
		  unsigned int payload_len,
		  unsigned int *payload,
		  unsigned int need_ack)
{
	bool ret = false;
	struct ipi_msg_t ipi_msg;
	int ipi_result = -1;
	unsigned int retry_time = VOW_IPI_SEND_CNT_TIMEOUT;
	unsigned int retry_cnt;

	uint8_t data_type;
	uint8_t ack_type;
	uint32_t param1;
	uint32_t param2;
	char *payload;

	if (!vow_check_scp_status()) {
		VOWDRV_DEBUG("SCP is off, bypass send ipi id(%d)\n", msg_id);
		return false;
	}
	if (vow_service_GetScpRecoverStatus() == true) {
		VOWDRV_DEBUG("scp is recovering, then break\n");
		return false;
	}

	data_type = (payload_len == 0)?AUDIO_IPI_MSG_ONLY : AUDIO_IPI_PAYLOAD;
	ack_type =
	    (need_ack == 0)?AUDIO_IPI_MSG_BYPASS_ACK : AUDIO_IPI_MSG_NEED_ACK;
	param1 = sizeof(unsigned int) * payload_len;
	param2 = 0;

	for (retry_cnt = 0; retry_cnt <= retry_time; retry_cnt++) {
		ipi_result = audio_send_ipi_msg(&ipi_msg,
						TASK_SCENE_VOW,
						AUDIO_IPI_LAYER_TO_DSP,
						data_type,
						ack_type,
						(uint16_t)msg_id,
						param1,
						param2,
						(char *)payload);
		if (ipi_result == 0)
			break;
		if (vow_service_GetScpRecoverStatus() == true) {
			VOWDRV_DEBUG("scp is recovering, then break\n");
			break;
		}
		VOW_ASSERT(retry_cnt != retry_time);
		msleep(VOW_WAITCHECK_INTERVAL_MS);
	}
	if (ipi_result == 0) {
		/* ipi send pass */
		if (ipi_msg.ack_type == AUDIO_IPI_MSG_ACK_BACK)
			ret = vow_IPICmd_ReceiveAck(&ipi_msg);
		else
			ret = true;
	}
	return ret;
}
