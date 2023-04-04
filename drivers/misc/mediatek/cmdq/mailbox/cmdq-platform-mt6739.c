// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <dt-bindings/gce/mt6739-gce.h>

#include "cmdq-util.h"

const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	switch (thread) {
	case 0:
	case 2 ... 4:
	case 6 ... 9:
		return "DISP";
	case 1:
		return "VDEC";
	case 10:
		return "MDP";
	case 5:
		return "VENC";
	case 15:
		return "CMDQ";
	default:
		return "CMDQ";
	}
}

const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event,
	s32 thread)
{
	switch (event) {
	case CMDQ_EVENT_DISP_OVL0_SOF:
	case CMDQ_EVENT_DISP_RDMA0_SOF:
	case CMDQ_EVENT_DISP_WDMA0_SOF:
	case CMDQ_EVENT_DISP_COLOR0_SOF:
	case CMDQ_EVENT_DISP_CCORR0_SOF:
	case CMDQ_EVENT_DISP_AAL0_SOF:
	case CMDQ_EVENT_DISP_GAMMA0_SOF:
	case CMDQ_EVENT_DISP_DITHER0_SOF:
	case CMDQ_EVENT_DISP_DSI0_SOF:
	case CMDQ_EVENT_DISP_DBI0_SOF:
	case CMDQ_EVENT_DISP_PWM0_SOF:
	case CMDQ_EVENT_DISP_RDMA0_EOF:
	case CMDQ_EVENT_DISP_OVL0_EOF:
	case CMDQ_EVENT_DISP_WDMA0_EOF:
	case CMDQ_EVENT_DISP_COLOR0_EOF:
	case CMDQ_EVENT_DISP_CCORR0_EOF:
	case CMDQ_EVENT_DISP_AAL0_EOF:
	case CMDQ_EVENT_DISP_GAMMA0_EOF:
	case CMDQ_EVENT_DISP_DITHER0_EOF:
	case CMDQ_EVENT_DISP_DSI0_EOF:
	case CMDQ_EVENT_DISP_DBI0_EOF:
	case CMDQ_EVENT_DISP_RDMA0_UNDERRUN:
	case CMDQ_EVENT_DISP_RDMA1_UNDERRUN:
	case CMDQ_EVENT_DSI0_TE:
	case CMDQ_EVENT_DSI0_IRQ_EVENT:
	case CMDQ_EVENT_DSI0_DONE_EVENT:
	case CMDQ_EVENT_DISP_WDMA0_RST_DONE:
	case CMDQ_EVENT_DISP_OVL0_RST_DONE:
	case CMDQ_EVENT_MUTEX0_STREAM_EOF:
	case CMDQ_EVENT_MUTEX1_STREAM_EOF:
	case CMDQ_EVENT_MUTEX2_STREAM_EOF:
	case CMDQ_EVENT_MUTEX3_STREAM_EOF:
	case CMDQ_EVENT_MUTEX4_STREAM_EOF:
		return "DISP";
	case CMDQ_EVENT_MDP_RDMA0_SOF:
	case CMDQ_EVENT_MDP_RSZ0_SOF:
	case CMDQ_EVENT_MDP_RSZ1_SOF:
	case CMDQ_EVENT_MDP_WDMA_SOF:
	case CMDQ_EVENT_MDP_WROT0_SOF:
	case CMDQ_EVENT_MDP_TDSHP0_SOF:
	case CMDQ_EVENT_MDP_RDMA0_EOF:
	case CMDQ_EVENT_MDP_RSZ0_EOF:
	case CMDQ_EVENT_MDP_RSZ1_EOF:
	case CMDQ_EVENT_MDP_WROT0_W_EOF:
	case CMDQ_EVENT_MDP_WDMA_EOF:
	case CMDQ_EVENT_MDP_TDSHP0_EOF:
	case CMDQ_EVENT_MDP_WDMA_RST_DONE:
	case CMDQ_EVENT_MDP_WROT0_RST_DONE:
	case CMDQ_EVENT_MDP_RDMA0_RST_DONE:
	case CMDQ_EVENT_MUTEX5_STREAM_EOF:
	case CMDQ_EVENT_MUTEX6_STREAM_EOF:
	case CMDQ_EVENT_MUTEX7_STREAM_EOF:
	case CMDQ_EVENT_MUTEX8_STREAM_EOF:
	case CMDQ_EVENT_MUTEX9_STREAM_EOF:
		return "MDP";
	case CMDQ_EVENT_VENC_EOF:
	case CMDQ_EVENT_VENC_MB_DONE:
	case CMDQ_EVENT_VENC_128BYTE_CNT_DONE:
	case CMDQ_EVENT_JPEG_DEC_EOF:
	case CMDQ_EVENT_JPEG_ENC_EOF:
		return "VENC";
	case CMDQ_EVENT_ISP_PASS2_2_EOF:
	case CMDQ_EVENT_ISP_PASS2_1_EOF:
	case CMDQ_EVENT_ISP_PASS2_0_EOF:
	case CMDQ_EVENT_ISP_PASS1_1_EOF:
	case CMDQ_EVENT_ISP_PASS1_2_EOF:
	case CMDQ_EVENT_ISP_CAMSV_2_PASS1_DONE:
	case CMDQ_EVENT_ISP_CAMSV_1_PASS1_DONE:
	case CMDQ_EVENT_ISP_SENINF_CAM1_2_3_FULL:
	case CMDQ_EVENT_ISP_SENINF_CAM0_FULL:
		return "ISP";
	default:
		return cmdq_thread_module_dispatch(gce_pa, thread);
	}
}

u32 cmdq_util_hw_id(u32 pa)
{
	return 0;
}

u32 cmdq_test_get_subsys_list(u32 **regs_out)
{
	static u32 regs[] = {
		0x14000100,	/* mmsys MMSYS_CG_CON0 */
		0x112300a0,	/* msdc0 SW_DBG_SEL: LSB 16-bit only */
		0x1121004c,	/* To-do: audio AFE_I2S_CON3_OFFSET */
		0x110020bc,	/* uart0:LSB 1-bit only */
	};

	*regs_out = regs;
	return ARRAY_SIZE(regs);
}

const char *cmdq_util_hw_name(void *chan)
{
	return "GCE";
}

bool cmdq_thread_ddr_module(const s32 thread)
{
	switch (thread) {
	case 0 ... 6:
	case 8 ... 9:
	case 15:
		return false;
	default:
		return true;
	}
}

struct cmdq_util_platform_fp platform_fp = {
	.thread_module_dispatch = cmdq_thread_module_dispatch,
	.event_module_dispatch = cmdq_event_module_dispatch,
	.util_hw_id = cmdq_util_hw_id,
	.test_get_subsys_list = cmdq_test_get_subsys_list,
	.util_hw_name = cmdq_util_hw_name,
	.thread_ddr_module = cmdq_thread_ddr_module,
};

static int __init cmdq_platform_init(void)
{
	cmdq_util_set_fp(&platform_fp);
	return 0;
}

static void __exit cmdq_platform_exit(void)
{
	cmdq_util_reset_fp(&platform_fp);
}

module_init(cmdq_platform_init);
module_exit(cmdq_platform_exit);

MODULE_LICENSE("GPL v2");
