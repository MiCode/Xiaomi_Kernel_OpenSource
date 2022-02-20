// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <dt-bindings/gce/mt6789-gce.h>

#include "cmdq-util.h"

const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	switch (thread) {
	case 0 ... 6:
	case 8 ... 9:
		return "DISP";
	case 7:
		return "VDEC";
	case 10:
	case 16 ... 19:
		return "MDP";
	case 11:
	case 20 ... 23:
		return "ISP";
	case 12:
		return "VENC";
	case 13 ... 15:
	default:
		return "CMDQ";
	}
}

const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event,
	s32 thread)
{
	switch (event) {
	case CMDQ_EVENT_VDEC_GCE_EVENT_0
		... CMDQ_EVENT_VDEC_VDEC_MINI_MDP_EVENT_15:
		return "VDEC";
	case CMDQ_EVENT_CAM_FRAME_DONE_0 ... CMDQ_EVENT_CAM_ENG_EVENT_28:
		return "CAM";
	case CMDQ_EVENT_VENC_VENCSYS_CMDQ_DONE_CAT_0
		... CMDQ_EVENT_VENC_VENC_CMDQ_BSDMA_FULL:
		return "VENC";
	case CMDQ_EVENT_IPE_GCE_EVENT_0 ... CMDQ_EVENT_IPE_GCE_EVENT_4:
		return "IPE";
	case CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_0
		... CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_23:
		return "IMG2";
	case CMDQ_EVENT_MDP_MDP_RDMA0_SOF
		... CMDQ_EVENT_MDP_MDP_RDMA0_SW_RST_DONE_ENG_EVENT:
		return "MDP";
	case CMDQ_EVENT_DISP_DISP_OVL0_SOF
		... CMDQ_EVENT_DISP_BUF_UNDERRUN_ENG_EVENT_7:
		return "DISP";
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
module_init(cmdq_platform_init);

MODULE_LICENSE("GPL v2");
