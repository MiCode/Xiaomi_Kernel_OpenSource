// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/of_address.h>

#include <dt-bindings/gce/mt6855-gce.h>

#include "cmdq-util.h"

#define GCE_D_PA	0x1e980000
#define GCE_M_PA	0x1e990000

const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	if (gce_pa == GCE_D_PA) {
		switch (thread) {
		case 0 ... 9:
			return "DISP";
		case 28 ... 29:
			return "MML";
		default:
			return "CMDQ";
		}
	} else if (gce_pa == GCE_M_PA) {
		switch (thread) {
		case 0 ... 4:
		case 11:
			return "ISP";
		case 6 ... 7:
		case 29:
			return "VFMT";
		case 12:
		case 22 ... 23:
			return "VENC";
		case 10:
		case 16 ... 19:
		case 26 ... 27:
			return "MDP";
		default:
			return "CMDQ";
		}
	}

	return "CMDQ";
}

const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event,
	s32 thread)
{
	switch (event) {
	case CMDQ_SYNC_TOKEN_CONFIG_DIRTY:
	case CMDQ_SYNC_TOKEN_STREAM_EOF:
	case CMDQ_SYNC_TOKEN_ESD_EOF:
	case CMDQ_SYNC_TOKEN_STREAM_BLOCK:
	case CMDQ_SYNC_TOKEN_CABC_EOF:
		return "DISP";
	case CMDQ_SYNC_TOKEN_IMGSYS_POOL_1
		... CMDQ_SYNC_TOKEN_IMGSYS_POOL_100:
	case CMDQ_SYNC_TOKEN_MSS
		... CMDQ_SYNC_TOKEN_MSF:
		return "IMGSYS";

	case CMDQ_EVENT_GPR_TIMER ... CMDQ_EVENT_GPR_TIMER + 32:
		return cmdq_thread_module_dispatch(gce_pa, thread);
	}

	if (gce_pa == GCE_D_PA) // GCE-D
		switch (event) {
		case CMDQ_EVENT_DISPSYS_OVL0_SOF
			... CMDQ_EVENT_DISPSYS_BUF_UNDERRUN_ENG_EVENT_7:
			return "DISP";
		default:
			return "CMDQ";
		}

	if (gce_pa == GCE_M_PA) // GCE-M
		switch (event) {
		case CMDQ_EVENT_VENC_EVENV_0 ... CMDQ_EVENT_VENC_EVENV_12:
			return "VENC";
		case CMDQ_EVENT_VDEC_EVENT_0 ... CMDQ_EVENT_VDEC_EVENT_15:
			return "VDEC";
		case CMDQ_EVENT_GCE_SMI_ALL_EVENT_0
			... CMDQ_EVENT_GCE_SMI_ALL_EVENT_2:
			return "SMI";
		case CMDQ_EVENT_IMG2_DIP_FRAME_DONE_P2_0
			... CMDQ_EVENT_IMG1_MSS_DONE_LINK_MISC:
			return "IMG";
		case CMDQ_EVENT_CAM_EVENT_0
			... CMDQ_EVENT_CAM_SENINF_CAM14_FIFO_FULL:
			return "CAM";
		case CMDQ_EVENT_MDPSYS_STREAM_DONE_ENG_EVENT_0
			... CMDQ_EVENT_MDPSYS_BUF_UNDERRUN_ENG_EVENT_3:
			return "MDP";
		case CMDQ_EVENT_VDEC_FMT_MDP0_RDMA_SW_RST_DONE_ENG_EVENT
			... CMDQ_EVENT_VDEC_FMT_MDP1_WDMA_TILE_DONE:
			return "VFMT";
		case CMDQ_EVENT_IPE_SOF
			... CMDQ_EVENT_IPE_ENG_EVENT:
			return "IPE";
		default:
			return "CMDQ";
		}

	return "CMDQ";
}

u32 cmdq_util_hw_id(u32 pa)
{
	switch (pa) {
	case GCE_D_PA:
		return 0;
	case GCE_M_PA:
		return 1;
	default:
		cmdq_err("unknown addr:%x", pa);
	}

	return 0;
}

u32 cmdq_test_get_subsys_list(u32 **regs_out)
{
	static u32 regs[] = {
		0x1f003000,	/* mdp_rdma0 */
		0x14000100,	/* mmsys_config */
		0x14001000,	/* dispsys */
		0x15020000,	/* imgsys */
		0x1000106c,	/* infra */
	};

	*regs_out = regs;
	return ARRAY_SIZE(regs);
}

void cmdq_test_set_ostd(void)
{
	void __iomem	*va_base;

	u32 val = 0x01014000;
	u32 pa_base;
	u32 preval, newval;

	cmdq_msg("%s in", __func__);
	/* 1. set mminfra_smi_u_disp_comm_SMI_COMMON outstanding to 1 : 0x1E801000 = 0x01014000 */
	pa_base = 0x1E801000;
	va_base = ioremap(pa_base + 0x10C, 0x1000);
	preval = readl(va_base);
	writel(val, va_base);
	newval = readl(va_base);
	cmdq_msg("%s addr%#x: %#x -> %#x  ", __func__, pa_base, preval, newval);

	/* 2. set MMINFRA_SMI_U_MDP_SUB_COMM0 outstanding to 1 : 0x1e809000 = 0x01014000 */
	pa_base = 0x1e809000;
	va_base = ioremap(pa_base + 0x104, 0x1000);
	preval = readl(va_base);
	writel(val, va_base);
	newval = readl(va_base);
	cmdq_msg("%s addr%#x: %#x -> %#x  ", __func__, pa_base, preval, newval);
}

const char *cmdq_util_hw_name(void *chan)
{
	u32 hw_id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(chan));

	if (hw_id == 0)
		return "GCE-D";

	if (hw_id == 1)
		return "GCE-M";

	return "CMDQ";
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
	.test_set_ostd = cmdq_test_set_ostd,
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
