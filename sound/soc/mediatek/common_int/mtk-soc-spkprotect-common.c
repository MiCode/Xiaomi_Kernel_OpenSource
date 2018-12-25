/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */


/****************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_spkprotect_common.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio playback
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *---------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */

#include "mtk-auddrv-scp-spkprotect-common.h"
#include <linux/compat.h>
#include <scp_helper.h>
#include <scp_ipi.h>

#ifdef CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT
#include "audio_ipi_client_spkprotect.h"
#include <audio_dma_buf_control.h>
#include <audio_ipi_client_spkprotect.h>
#include <audio_task_manager.h>
#endif

static struct aud_spk_message mAud_Spk_Message;
static struct audio_resv_dram_t resv_dram_spkprotect;
static struct spk_dump_ops *mspk_dump_op;

void init_spkscp_reserved_dram(void)
{

	/*speaker protection*/
	resv_dram_spkprotect.phy_addr =
		(char *)scp_get_reserve_mem_phys(SPK_PROTECT_MEM_ID);
	resv_dram_spkprotect.vir_addr =
		(char *)scp_get_reserve_mem_virt(SPK_PROTECT_MEM_ID);
	resv_dram_spkprotect.size =
		(uint32_t)scp_get_reserve_mem_size(SPK_PROTECT_MEM_ID);

	pr_info("resv_dram: pa %p, va %p, sz 0x%x\n",
		resv_dram_spkprotect.phy_addr, resv_dram_spkprotect.vir_addr,
		resv_dram_spkprotect.size);

	if (is_scp_ready(SCP_B_ID)) {
		AUDIO_ASSERT(resv_dram_spkprotect.phy_addr != NULL);
		AUDIO_ASSERT(resv_dram_spkprotect.vir_addr != NULL);
		AUDIO_ASSERT(resv_dram_spkprotect.size > 0);
	}
}

audio_resv_dram_t *get_reserved_dram_spkprotect(void)
{
	return &resv_dram_spkprotect;
}

char *get_resv_dram_spkprotect_vir_addr(char *resv_dram_phy_addr)
{
	uint32_t offset = 0;

	offset = resv_dram_phy_addr - resv_dram_spkprotect.phy_addr;
	return resv_dram_spkprotect.vir_addr + offset;
}

void spkproc_service_set_spk_dump_message(struct spk_dump_ops *ops)
{
	pr_debug("%s\n ", __func__);
	if (ops != NULL) {
		mspk_dump_op = ops;
		mspk_dump_op->spk_dump_callback(NULL);
	}
}

void spkproc_service_ipicmd_received(struct ipi_msg_t *ipi_msg)
{
	switch (ipi_msg->msg_id) {
	case SPK_PROTECT_IRQDL:
		mAud_Spk_Message.msg_id = ipi_msg->msg_id;
		mAud_Spk_Message.param1 = ipi_msg->param1;
		mAud_Spk_Message.param2 = ipi_msg->param2;
		mAud_Spk_Message.payload = ipi_msg->payload;
		AudDrv_DSP_IRQ_handler((void *)&mAud_Spk_Message);
		break;
	case SPK_PROTECT_PCMDUMP_OK:
		if (mspk_dump_op->spk_dump_callback != NULL)
			mspk_dump_op->spk_dump_callback(ipi_msg);
		break;
	default:
		break;
	}
}

void spkproc_service_ipicmd_send(uint8_t data_type, uint8_t ack_type,
				 uint16_t msg_id, uint32_t param1,
				 uint32_t param2, char *payload)
{
	struct ipi_msg_t ipi_msg;
	int send_result = 0;
	int retry_count;
	const int k_max_try_count = 200; /* maximum wait 20ms */

	memset((void *)&ipi_msg, 0, sizeof(struct ipi_msg_t));
	for (retry_count = 0; retry_count < k_max_try_count; retry_count++) {
		if (ack_type == AUDIO_IPI_MSG_DIRECT_SEND)
			send_result = audio_send_ipi_msg(
				&ipi_msg, TASK_SCENE_SPEAKER_PROTECTION,
				AUDIO_IPI_LAYER_KERNEL_TO_SCP_ATOMIC, data_type,
				ack_type, msg_id, param1, param2,
				(char *)payload);
		else
			send_result = audio_send_ipi_msg(
				&ipi_msg, TASK_SCENE_SPEAKER_PROTECTION,
				AUDIO_IPI_LAYER_KERNEL_TO_SCP, data_type,
				ack_type, msg_id, param1, param2,
				(char *)payload);
		if (send_result == 0)
			break;
		udelay(100);
	}

	if (send_result < 0) {
		pr_err("%s(), scp_ipi send fail\n", __func__);
		return;
	}
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SpkPotect");
