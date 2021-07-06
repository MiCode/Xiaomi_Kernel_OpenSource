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
 */

#include <linux/soc/mediatek/mtk-cmdq.h>
#include "mdp_cmdq_device.h"

#define DECLAR_EVENT(event_enum, dts_name) \
	{event_enum, #event_enum, #dts_name},

static struct cmdq_event_table cmdq_events[] = {
	/* MDP start frame */
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA0_SOF, mdp_rdma0_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA1_SOF, mdp_rdma1_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA2_SOF, mdp_rdma2_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA3_SOF, mdp_rdma3_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ0_SOF, mdp_rsz0_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ1_SOF, mdp_rsz1_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ2_SOF, mdp_rsz2_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ3_SOF, mdp_rsz3_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP_SOF, mdp_tdshp_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP1_SOF, mdp_tdshp1_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP2_SOF, mdp_tdshp2_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP3_SOF, mdp_tdshp3_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WDMA_SOF, mdp_wdma_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT_SOF, mdp_wrot_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT0_SOF, mdp_wrot0_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT1_SOF, mdp_wrot1_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT2_SOF, mdp_wrot2_sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT3_SOF, mdp_wrot3_sof)

	/* Display start frame */
	DECLAR_EVENT(CMDQ_EVENT_DISP_RDMA0_SOF, disp_rdma0_sof)

	/* MDP frame done */
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA0_EOF, mdp_rdma0_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA1_EOF, mdp_rdma1_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA2_EOF, mdp_rdma2_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA3_EOF, mdp_rdma3_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ0_EOF, mdp_rsz0_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ1_EOF, mdp_rsz1_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ2_EOF, mdp_rsz2_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ3_EOF, mdp_rsz3_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP_EOF, mdp_tdshp_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP1_EOF, mdp_tdshp1_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP2_EOF, mdp_tdshp2_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP3_EOF, mdp_tdshp3_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WDMA_EOF, mdp_wdma_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT_WRITE_EOF, mdp_wrot_write_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT0_WRITE_EOF,
		mdp_wrot0_write_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT1_WRITE_EOF,
		mdp_wrot1_write_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT2_WRITE_EOF,
		mdp_wrot2_write_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT3_WRITE_EOF,
		mdp_wrot3_write_frame_done)

	/* Display frame done */
	DECLAR_EVENT(CMDQ_EVENT_DISP_OVL0_EOF, disp_ovl0_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DISP_RDMA0_EOF, disp_rdma0_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DISP_WDMA0_EOF, disp_wdma0_frame_done)

	/* ISP frame done */
	DECLAR_EVENT(CMDQ_EVENT_ISP_PASS2_2_EOF, isp_frame_done_p2_2)
	DECLAR_EVENT(CMDQ_EVENT_ISP_PASS2_1_EOF, isp_frame_done_p2_1)
	DECLAR_EVENT(CMDQ_EVENT_ISP_PASS2_0_EOF, isp_frame_done_p2_0)

	/* ISP (IMGSYS) frame done */
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD0_EOF, dip_cq_thread0_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD1_EOF, dip_cq_thread1_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD2_EOF, dip_cq_thread2_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD3_EOF, dip_cq_thread3_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD4_EOF, dip_cq_thread4_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD5_EOF, dip_cq_thread5_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD6_EOF, dip_cq_thread6_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD7_EOF, dip_cq_thread7_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD8_EOF, dip_cq_thread8_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD9_EOF, dip_cq_thread9_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD10_EOF,
		dip_cq_thread10_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD11_EOF,
		dip_cq_thread11_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD12_EOF,
		dip_cq_thread12_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD13_EOF,
		dip_cq_thread13_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD14_EOF,
		dip_cq_thread14_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD15_EOF,
		dip_cq_thread15_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD16_EOF,
		dip_cq_thread16_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD17_EOF,
		dip_cq_thread17_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD18_EOF,
		dip_cq_thread18_frame_done)

	/* JPEG frame done */
	DECLAR_EVENT(CMDQ_EVENT_JPEG_ENC_EOF, jpgenc_done)
	DECLAR_EVENT(CMDQ_EVENT_JPEG_DEC_EOF, jpgdec_done)

	/* WPE frame done */
	DECLAR_EVENT(CMDQ_EVENT_WPE_A_EOF, wpe_a_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_WPE_B_FRAME_DONE, wpe_b_frame_done)

	/* Direct-link start frame */
	DECLAR_EVENT(CMDQ_EVENT_IMG_DL_RELAY_SOF, img_dl_relay_sof)
	DECLAR_EVENT(CMDQ_EVENT_IMG_DL_RELAY1_SOF, img_dl_relay1_sof)
	DECLAR_EVENT(CMDQ_EVENT_IMG_DL_RELAY2_SOF, img_dl_relay2_sof)
	DECLAR_EVENT(CMDQ_EVENT_IMG_DL_RELAY3_SOF, img_dl_relay3_sof)

	/* Dual DIP frame done */
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_0,
		dip_cq_thread0_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_1,
		dip_cq_thread1_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_2,
		dip_cq_thread2_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_3,
		dip_cq_thread3_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_4,
		dip_cq_thread4_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_5,
		dip_cq_thread5_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_6,
		dip_cq_thread6_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_7,
		dip_cq_thread7_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_8,
		dip_cq_thread8_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_9,
		dip_cq_thread9_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_10,
		dip_cq_thread10_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_11,
		dip_cq_thread11_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_12,
		dip_cq_thread12_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_13,
		dip_cq_thread13_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_14,
		dip_cq_thread14_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_15,
		dip_cq_thread15_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_16,
		dip_cq_thread16_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_17,
		dip_cq_thread17_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_18,
		dip_cq_thread18_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_19,
		dip_cq_thread19_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_20,
		dip_cq_thread20_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_21,
		dip_cq_thread21_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_22,
		dip_cq_thread22_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_23,
		dip_cq_thread23_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_0,
		dip2_cq_thread0_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_1,
		dip2_cq_thread1_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_2,
		dip2_cq_thread2_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_3,
		dip2_cq_thread3_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_4,
		dip2_cq_thread4_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_5,
		dip2_cq_thread5_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_6,
		dip2_cq_thread6_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_7,
		dip2_cq_thread7_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_8,
		dip2_cq_thread8_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_9,
		dip2_cq_thread9_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_10,
		dip2_cq_thread10_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_11,
		dip2_cq_thread11_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_12,
		dip2_cq_thread12_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_13,
		dip2_cq_thread13_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_14,
		dip2_cq_thread14_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_15,
		dip2_cq_thread15_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_16,
		dip2_cq_thread16_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_17,
		dip2_cq_thread17_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_18,
		dip2_cq_thread18_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_19,
		dip2_cq_thread19_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_20,
		dip2_cq_thread20_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_21,
		dip2_cq_thread21_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_22,
		dip2_cq_thread22_frame_done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_23,
		dip2_cq_thread23_frame_done)

	/* Keep this at the end of HW events */
	DECLAR_EVENT(CMDQ_MAX_HW_EVENT_COUNT, hw_event_conunt)

	/* SW Sync Tokens (Pre-defined) */
	/* Pass-2 notifies VENC frame is ready to be encoded */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_VENC_INPUT_READY, sw_token)
	/* VENC notifies Pass-2 encode done so next frame may start */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_VENC_EOF, sw_token)

	/* SW Sync Tokens (User-defined) */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_USER_0, sw_token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_USER_1, sw_token)

	/* isp */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_MSS, sw_token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_MSF, sw_token)

	/* GPR access tokens (for HW register backup)
	 * There are 15 32-bit GPR, 3 GPR form a set
	 * (64-bit for address, 32-bit for value)
	 */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_GPR_SET_0, sw_token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_GPR_SET_1, sw_token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_GPR_SET_2, sw_token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_GPR_SET_3, sw_token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_GPR_SET_4, sw_token)

	/* Resource lock event to control resource in GCE thread */
	DECLAR_EVENT(CMDQ_SYNC_RESOURCE_WROT0, sw_token)
	DECLAR_EVENT(CMDQ_SYNC_RESOURCE_WROT1, sw_token)

	/* event id is 10 bit */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_MAX, max_token)
};

struct cmdq_event_table *cmdq_event_get_table(void)
{
	return cmdq_events;
}

u32 cmdq_event_get_table_size(void)
{
	return ARRAY_SIZE(cmdq_events);
}
