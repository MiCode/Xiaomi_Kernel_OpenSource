// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <dt-bindings/gce/mt6781-gce.h>
#include "../cmdq-util.h"

const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	switch (thread) {
	case 0 ... 6:
	case 8 ... 9:
		return "DISP";
	case 7:
		return "VDEC";
	case 10:
	case 19 ... 22:
		return "MDP";
	case 11:
	case 13 ... 14:
	case 16 ... 18:
		return "ISP";
	case 12:
		return "VENC";
	case 15:
		return "CMDQ";
	case 23:
		return "VFMT";
	default:
		return "CMDQ";
	}
}

const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event,
	s32 thread)
{
	switch (event) {
	case CMDQ_EVENT_LINE_COUNT_THRESHOLD_INTERRUPT ... CMDQ_EVENT_GCE_CNT_OP_THRESHOLD:
		return "VDEC";
	case CMDQ_EVENT_VDEC_MINI_MDP_EVENT_0 ... CMDQ_EVENT_VDEC_MINI_MDP_EVENT_15:
		return "VFMT";
	case CMDQ_EVENT_ISP_FRAME_DONE_A ... CMDQ_EVENT_CQ_VR_SNAP_B_INT:
		return "CAM";
	case CMDQ_EVENT_VENC_CMDQ_FRAME_DONE ... CMDQ_EVENT_VENC_CMDQ_VPS_DONE:
		return "VENC";
	case CMDQ_EVENT_FDVT_DONE ... CMDQ_EVENT_DVP_DONE_ASYNC_SHOT:
		return "IPE";
	case CMDQ_EVENT_GCE_IMG2_EVENT0 ... CMDQ_EVENT_GCE_IMG1_EVENT23:
		return "IMG";
	case CMDQ_EVENT_MDP_RDMA0_SOF ... CMDQ_EVENT_MDP_RDMA0_SW_RST_DONE_ENG_EVENT:
		return "MDP";
	case CMDQ_EVENT_DISP_OVL0_SOF ... CMDQ_EVENT_BUF_UNDERRUN_ENG_EVENT_7:
		return "DISP";
	default:
		return cmdq_thread_module_dispatch(gce_pa, thread);
	}
}
EXPORT_SYMBOL(cmdq_event_module_dispatch);

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

bool cmdq_thread_ddr_user_check(const s32 thread)
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
EXPORT_SYMBOL(cmdq_thread_ddr_user_check);
