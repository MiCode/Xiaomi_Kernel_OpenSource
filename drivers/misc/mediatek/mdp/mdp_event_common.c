// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include "cmdq_device.h"

#define DECLAR_EVENT(event_enum, dts_name) \
	{event_enum, #event_enum, #dts_name},

static struct cmdq_event_table cmdq_events[] = {
	/* MDP start frame */
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA0_SOF, mdp-rdma0-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA1_SOF, mdp-rdma1-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA2_SOF, mdp-rdma2-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA3_SOF, mdp-rdma3-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ0_SOF, mdp-rsz0-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ1_SOF, mdp-rsz1-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ2_SOF, mdp-rsz2-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ3_SOF, mdp-rsz3-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP_SOF, mdp-tdshp-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP1_SOF, mdp-tdshp1-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP2_SOF, mdp-tdshp2-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP3_SOF, mdp-tdshp3-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WDMA_SOF, mdp-wdma-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT_SOF, mdp-wrot-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT0_SOF, mdp-wrot0-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT1_SOF, mdp-wrot1-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT2_SOF, mdp-wrot2-sof)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT3_SOF, mdp-wrot3-sof)

	/* Display start frame */
	DECLAR_EVENT(CMDQ_EVENT_DISP_RDMA0_SOF, disp-rdma0-sof)

	/* MDP frame done */
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA0_EOF, mdp-rdma0-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA1_EOF, mdp-rdma1-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA2_EOF, mdp-rdma2-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RDMA3_EOF, mdp-rdma3-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ0_EOF, mdp-rsz0-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ1_EOF, mdp-rsz1-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ2_EOF, mdp-rsz2-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_RSZ3_EOF, mdp-rsz3-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP_EOF, mdp-tdshp-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP1_EOF, mdp-tdshp1-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP2_EOF, mdp-tdshp2-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_TDSHP3_EOF, mdp-tdshp3-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WDMA_EOF, mdp-wdma-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT_WRITE_EOF, mdp-wrot-write-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT0_WRITE_EOF,
		mdp-wrot0-write-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT1_WRITE_EOF,
		mdp-wrot1-write-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT2_WRITE_EOF,
		mdp-wrot2-write-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_MDP_WROT3_WRITE_EOF,
		mdp-wrot3-write-frame-done)

	/* Display frame done */
	DECLAR_EVENT(CMDQ_EVENT_DISP_OVL0_EOF, disp-ovl0-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DISP_RDMA0_EOF, disp-rdma0-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DISP_WDMA0_EOF, disp-wdma0-frame-done)

	/* ISP frame done */
	DECLAR_EVENT(CMDQ_EVENT_ISP_PASS2_2_EOF, isp-frame-done-p2-2)
	DECLAR_EVENT(CMDQ_EVENT_ISP_PASS2_1_EOF, isp-frame-done-p2-1)
	DECLAR_EVENT(CMDQ_EVENT_ISP_PASS2_0_EOF, isp-frame-done-p2-0)

	/* ISP (IMGSYS) frame done */
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD0_EOF, dip-cq-thread0-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD1_EOF, dip-cq-thread1-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD2_EOF, dip-cq-thread2-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD3_EOF, dip_cq-thread3-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD4_EOF, dip_cq-thread4-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD5_EOF, dip_cq-thread5-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD6_EOF, dip_cq_thread6-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD7_EOF, dip-cq-thread7-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD8_EOF, dip-cq-thread8-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD9_EOF, dip-cq-thread9-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD10_EOF,
		dip-cq-thread10-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD11_EOF,
		dip-cq-thread11-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD12_EOF,
		dip-cq-thread12-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD13_EOF,
		dip-cq-thread13-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD14_EOF,
		dip-cq-thread14-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD15_EOF,
		dip-cq-thread15-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD16_EOF,
		dip-cq-thread16-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD17_EOF,
		dip-cq-thread17-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_DIP_CQ_THREAD18_EOF,
		dip-cq-thread18-frame-done)

	/* JPEG frame done */
	DECLAR_EVENT(CMDQ_EVENT_JPEG_ENC_EOF, jpgenc-done)
	DECLAR_EVENT(CMDQ_EVENT_JPEG_DEC_EOF, jpgdec-done)

	/* WPE frame done */
	DECLAR_EVENT(CMDQ_EVENT_WPE_A_EOF, wpe-a-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_WPE_B_FRAME_DONE, wpe-b-frame-done)

	/* Direct-link start frame */
	DECLAR_EVENT(CMDQ_EVENT_IMG_DL_RELAY_SOF, img-dl-relay-sof)
	DECLAR_EVENT(CMDQ_EVENT_IMG_DL_RELAY1_SOF, img-dl-relay1-sof)
	DECLAR_EVENT(CMDQ_EVENT_IMG_DL_RELAY2_SOF, img-dl-relay2-sof)
	DECLAR_EVENT(CMDQ_EVENT_IMG_DL_RELAY3_SOF, img-dl-relay3-sof)

	/* Dual DIP frame done */
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_0,
		dip-cq-thread0-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_1,
		dip-cq-thread1-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_2,
		dip-cq-thread2-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_3,
		dip-cq-thread3-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_4,
		dip-cq-thread4-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_5,
		dip-cq-thread5-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_6,
		dip-cq-thread6-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_7,
		dip-cq-thread7-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_8,
		dip-cq-thread8-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_9,
		dip-cq-thread9-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_10,
		dip-cq-thread10-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_11,
		dip-cq-thread11-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_12,
		dip-cq-thread12-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_13,
		dip-cq-thread13-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_14,
		dip-cq-thread14-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_15,
		dip-cq-thread15-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_16,
		dip-cq-thread16-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_17,
		dip-cq-thread17-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_18,
		dip-cq-thread18-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_19,
		dip-cq-thread19-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_20,
		dip-cq-thread20-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_21,
		dip-cq-thread21-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_22,
		dip-cq-thread22-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_23,
		dip-cq-thread23-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_0,
		dip2-cq-thread0-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_1,
		dip2-cq-thread1-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_2,
		dip2-cq-thread2-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_3,
		dip2-cq-thread3-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_4,
		dip2-cq-thread4-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_5,
		dip2-cq-thread5-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_6,
		dip2-cq-thread6-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_7,
		dip2-cq-thread7-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_8,
		dip2-cq-thread8-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_9,
		dip2-cq-thread9-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_10,
		dip2-cq-thread10-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_11,
		dip2-cq-thread11-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_12,
		dip2-cq-thread12-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_13,
		dip2-cq-thread13-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_14,
		dip2-cq-thread14-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_15,
		dip2-cq-thread15-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_16,
		dip2-cq-thread16-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_17,
		dip2-cq-thread17-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_18,
		dip2-cq-thread18-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_19,
		dip2-cq-thread19-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_20,
		dip2-cq-thread20-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_21,
		dip2-cq-thread21-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_22,
		dip2-cq-thread22-frame-done)
	DECLAR_EVENT(CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_23,
		dip2-cq-thread23-frame-done)

	/* Keep this at the end of HW events */
	DECLAR_EVENT(CMDQ_MAX_HW_EVENT_COUNT, hw-event-conunt)

	/* SW Sync Tokens (Pre-defined) */
	/* Pass-2 notifies VENC frame is ready to be encoded */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_VENC_INPUT_READY, sw-token)
	/* VENC notifies Pass-2 encode done so next frame may start */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_VENC_EOF, sw-token)

	/* SW Sync Tokens (User-defined) */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_USER_0, sw-token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_USER_1, sw-token)

	/* isp */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_MSS, sw-token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_MSF, sw-token)

	/* GPR access tokens (for HW register backup)
	 * There are 15 32-bit GPR, 3 GPR form a set
	 * (64-bit for address, 32-bit for value)
	 */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_GPR_SET_0, sw-token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_GPR_SET_1, sw-token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_GPR_SET_2, sw-token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_GPR_SET_3, sw-token)
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_GPR_SET_4, sw-token)

	/* Resource lock event to control resource in GCE thread */
	DECLAR_EVENT(CMDQ_SYNC_RESOURCE_WROT0, sw-token)
	DECLAR_EVENT(CMDQ_SYNC_RESOURCE_WROT1, sw-token)

	/* event id is 10 bit */
	DECLAR_EVENT(CMDQ_SYNC_TOKEN_MAX, max-token)
};

struct cmdq_event_table *cmdq_event_get_table(void)
{
	return cmdq_events;
}

u32 cmdq_event_get_table_size(void)
{
	return ARRAY_SIZE(cmdq_events);
}
