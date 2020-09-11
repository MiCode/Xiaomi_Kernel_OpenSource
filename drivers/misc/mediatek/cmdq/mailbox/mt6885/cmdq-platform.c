// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <dt-bindings/gce/mt6885-gce.h>

#include "../cmdq-util.h"

#define GCE_D_PA	0x10228000
#define GCE_M_PA	0x10318000

const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	if (gce_pa == GCE_D_PA) {
		switch (thread) {
		case 0 ... 9:
			return "DISP";
		case 16:
			return "VDEC";
		case 17 ... 18:
			return "VENC";
		case 23:
			return "CMDQ-TEST";
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
	case CMDQ_SYNC_TOKEN_MSS:
		return "MSS";
	case CMDQ_SYNC_TOKEN_MSF:
		return "MSF";

	case CMDQ_EVENT_GPR_TIMER ... CMDQ_EVENT_GPR_TIMER+32:
		return cmdq_thread_module_dispatch(gce_pa, thread);
	}

	if (gce_pa == GCE_D_PA) // GCE-D
		switch (event) {
		case CMDQ_EVENT_DISP_OVL0_SOF ... CMDQ_EVENT_DP_INTF_SOF:
		case CMDQ_EVENT_DISP_RDMA4_SOF
			... CMDQ_EVENT_DISP_UFBC_WDMA1_SOF:
		case CMDQ_EVENT_DSI1_FRAME_DONE
			... CMDQ_EVENT_DISP_OVL0_2L_RST_DONE:
		case CMDQ_EVENT_DISP_POSTMASK1_RST_DONE
			... CMDQ_EVENT_DISP_POSTMASK0_RST_DONE:
			return "DISP";

		case CMDQ_EVENT_MDP_AAL4_SOF ... CMDQ_EVENT_MDP_TDSHP5_SOF:
		case CMDQ_EVENT_MDP_TDSHP5_FRAME_DONE
			... CMDQ_EVENT_MDP_AAL4_FRAME_DONE:
			return "MDP";

		case CMDQ_EVENT_DP_VDE_END ... CMDQ_EVENT_DP_TARGET_LINE:
		case CMDQ_EVENT_VDEC_LAT_SOF_0 ... CMDQ_EVENT_VDEC_7:
		case CMDQ_EVENT_VDEC_CORE0_SOF_0
			... CMDQ_EVENT_VDEC_CORE0_7:
			return "VDEC";

		case CMDQ_EVENT_VENC_CMDQ_FRAME_DONE_C1
			... CMDQ_EVENT_VENC_CMDQ_PAUSE_DONE_C1:
		case CMDQ_EVENT_VENC_CMDQ_MB_DONE_C1
			... CMDQ_EVENT_VENC_CMDQ_128BYTE_CNT_DONE_C1:
		case CMDQ_EVENT_VENC_C0_CMDQ_WP_2ND_STAGE_DONE_C1
			... CMDQ_EVENT_VENC_CMDQ_PAUSE_DONE:
		case CMDQ_EVENT_VENC_CMDQ_MB_DONE
			... CMDQ_EVENT_VENC_CMDQ_128BYTE_CNT_DONE:
		case CMDQ_EVENT_VENC_C0_CMDQ_WP_2ND_STAGE_DONE
			... CMDQ_EVENT_VENC_C0_CMDQ_WP_3RD_STAGE_DONE:
			return "VENC";

		case CMDQ_EVENT_JPGENC_CMDQ_DONE_C1:
		case CMDQ_EVENT_JPGENC_CMDQ_DONE:
			return "JPGENC";

		case CMDQ_EVENT_JPGDEC_CMDQ_DONE_C1
			... CMDQ_EVENT_JPGDEC_C1_INSUFF_CMDQ_DONE_C1:
		case CMDQ_EVENT_JPGDEC_CMDQ_DONE
			... CMDQ_EVENT_JPGDEC_C1_INSUFF_CMDQ_DONE:
			return "JPGDEC";

		default:
			return "CMDQ";
		}

	if (gce_pa == GCE_M_PA) // GCE-M
		switch (event) {
		case CMDQ_EVENT_IMG2_EVENT_TX_FRAME_DONE_0
			... CMDQ_EVENT_IMG1_EVENT_TX_FRAME_DONE_23:
		case CMDQ_EVENT_IMG_DL_RELAY0_SOF
			... CMDQ_EVENT_IMG_DL_RELAY3_SOF:
			return "IMG";

		case CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0
			... CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_4:
			return "IPE";

		case CMDQ_EVENT_ISP_FRAME_DONE_A
			... CMDQ_EVENT_ISP_FRAME_DONE_C:
			return "ISP";

		case CMDQ_EVENT_CAMSV0_PASS1_DONE
			... CMDQ_EVENT_SENINF_CAM12_FIFO_FULL:
			return "CAM";

		case CMDQ_EVENT_MDP_RDMA0_SOF ... CMDQ_EVENT_MDP_TCC3_SOF:
		case CMDQ_EVENT_MDP_WROT3_FRAME_DONE
			... CMDQ_EVENT_MDP_RDMA0_SW_RST_DONE:
			return "MDP";

		default:
			return "CMDQ";
		}

	return "CMDQ";
}
EXPORT_SYMBOL(cmdq_event_module_dispatch);

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
		0x1f016f00,	/* VIDO_BASE_ADDR */
		0x14116100,	/* MMSYS_CG_CON0 */
		0x1602f10c,	/* VDEC_AXI_ASIF_CFG0 */
		0x17000104,	/* Reserved (venc_top) */
		0x17800104,	/* Reserved (venc_core1) */
		0x1a000370,	/* CAMSYS_APB3_SPARE */
		0x15020000,	/* IMGSYS1 */
		0x15820000,	/* IMGSYS2 */
		0x1B000000,	/* IPESYS */
		0x112300A0,	/* perisys apb msdc0 SW_DBG_SEL */
		0x1121004C,	/* perisys apb audio0 AFE_I2S_CON3_OFFSET */
		0x110020BC,	/* perisys apb uart0 UART */
	};

	*regs_out = regs;
	return ARRAY_SIZE(regs);
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
