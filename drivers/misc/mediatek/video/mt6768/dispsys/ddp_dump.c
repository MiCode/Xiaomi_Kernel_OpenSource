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

#define LOG_TAG "dump"

#include "ddp_reg.h"
#include "ddp_log.h"
#include "ddp_dump.h"
#include "ddp_ovl.h"
#include "ddp_wdma.h"
#include "ddp_wdma_ex.h"
#include "ddp_rdma.h"
#include "ddp_rdma_ex.h"
#include "ddp_dsi.h"
#include "ddp_rsz.h"
#include "smi_public.h"
#include "disp_helper.h"

static char *ddp_signal_0(int bit)
{
	switch (bit) {
	case 0:
		return
			"disp_aal0__to__disp_gamma0";
	case 1:
		return
			"disp_ccorr0__to__disp_aal0";
	case 2:
		return
			"disp_color0__to__disp_color0_out_sel_in0";
	case 3:
		return
			"disp_color_out_sel__to__disp_ccorr0";
	case 4:
		return
			"disp_dither0__to__disp_dither0_mout";
	case 5:
		return
			"disp_gamma0__to__disp_dither0";
	case 6:
		return
			"disp_ovl0_2l__to__disp_ovl0_2l_mout";
	case 7:
		return
			"disp_ovl0_2l_mout_out0__to__disp_path0_sel_in1";
	case 8:
		return
			"disp_ovl0_2l_mout_out1__to__disp_wdma0_sel_in2";
	case 9:
		return
			"disp_ovl0_2l_mout_out2__to__disp_rsz0_sel_in1";
	case 10:
		return
			"disp_ovl0__to__disp_ovl0_mout";
	case 11:
		return
			"disp_ovl0_mout_out0__to__disp_path0_sel_in0";
	case 12:
		return
			"disp_ovl0_mout_out1__to__disp_ovl0_2l_in0";
	case 13:
		return
			"disp_ovl0_mout_out2__to__disp_wdma0_sel_in1";
	case 14:
		return
			"disp_ovl0_mout_out3__to__disp_rsz0_sel_in0";
	case 15:
		return
			"disp_path0_sel__to__disp_rdma0";
	case 16:
		return
			"disp_rdma0__to__disp_rdma0_rsz0_in_sout";
	case 17:
		return
			"disp_rdma_rsz_in_sout_out0__to__disp_rdma_rsz_sel_in0";
	case 18:
		return
			"disp_rdma_rsz_in_sout_out1__to__disp_rsz0_sel_in2";
	case 19:
		return
			"disp_rdma0_rsz0_sel__to__disp_rdma0_sout";
	case 20:
		return
			"disp_rdma0_sout_out0__to__dsi0_sel_in0";
	case 21:
		return
			"disp_rdma0_sout_out1__to__disp_color0";
	case 22:
		return
			"disp_rdma0_sout_out2__to__disp_color0_out_sel_in1";
	case 23:
		return
			"disp_rsz0__to__disp_rsz0_mout";
	case 24:
		return
			"disp_rsz0_mout_out0__to__disp_path0_sel_in2";
	case 25:
		return
			"disp_rsz0_mout_out1__to__disp_ovl0_in2";
	case 26:
		return
			"disp_rsz0_mout_out2__to__disp_ovl0_2l_in2";
	case 27:
		return
			"disp_rsz0_mout_out3__to__disp_rdma0_rsz0_sel_in1";
	case 28:
		return
			"disp_rsz0_mout_out4__to__disp_wdma0_sel_in3";
	case 29:
		return
			"disp_rsz0_sel__to__disp_rsz0";
	case 30:
		return
			"disp_wdma0_sel__to__disp_wdma0";
	case 31:
		return
			"dither0_mout_out0__to__dsi0_sel_in1";
	default:
		return NULL;
	}
}

static char *ddp_signal_1(int bit)
{
	switch (bit) {
	case 0:
		return "dither0_mout_out1__to__disp_wdma0_sel_in0";
	case 1:
		return "dsi0_sel__to__dsi0";
	default:
		return NULL;
	}
}

static char *ddp_greq_name(int bit)
{
	switch (bit) {
	case 0:
		return "OVL0 ";
	case 1:
		return "OVL0_2L ";
	case 2:
		return "DISP_RDMA0 ";
	case 3:
		return "DISP_WDMA0 ";
	case 4:
		return "MDP_RDMA0 ";
	case 5:
		return "MDP_WDMA0 ";
	case 6:
		return "MDP_WROT0 ";
	case 7:
		return "DISP_FAKE ";
	default:
		return NULL;
	}
}

static char *ddp_get_mutex_module0_name(unsigned int bit)
{
	switch (bit) {
	case 0:  return "mdp_rdma0";
	case 1:  return "mdp_ccorr0";
	case 2:  return "mdp_rsz0";
	case 3:  return "mdp_rsz1";
	case 4:  return "mdp_wdma0";
	case 5:  return "mdp_wrot0";
	case 6:  return "mdp_tdshp";
	case 7:  return "disp_ovl0";
	case 8:  return "disp_ovl0_2l";
	case 9:  return "disp_rdma0";
	case 10: return "disp_wdma0";
	case 11: return "disp_color0";
	case 12: return "disp_ccorr0";
	case 13: return "disp_aal0";
	case 14: return "disp_gamma0";
	case 15: return "disp_dither0";
	case 16: return "DSI0";
	case 17: return "disp_rsz0";
	case 18: return "img_dl_ready";
	case 19: return "disp_pwm0";
	default: return "mutex-unknown";
	}
}



char *ddp_get_fmt_name(enum DISP_MODULE_ENUM module, unsigned int fmt)
{
	if (module == DISP_MODULE_WDMA0) {
		switch (fmt) {
		case 0:
			return "rgb565";
		case 1:
			return "rgb888";
		case 2:
			return "rgba8888";
		case 3:
			return "argb8888";
		case 4:
			return "uyvy";
		case 5:
			return "yuy2";
		case 7:
			return "y-only";
		case 8:
			return "iyuv";
		case 12:
			return "nv12";
		default:
			DDPDUMP("%s: unknown fmt=%d, module=%d\n",
				__func__, fmt, module);
			return "unknown";
		}
	} else if (module == DISP_MODULE_OVL0) {
		switch (fmt) {
		case 0:
			return "rgb565";
		case 1:
			return "rgb888";
		case 2:
			return "rgba8888";
		case 3:
			return "argb8888";
		case 4:
			return "uyvy";
		case 5:
			return "yuyv";
		default:
			DDPDUMP("%s: unknown fmt=%d, module=%d\n",
				__func__, fmt, module);
			return "unknown";
		}
	} else if (module == DISP_MODULE_RDMA0 || module == DISP_MODULE_RDMA1) {
		switch (fmt) {
		case 0:
			return "rgb565";
		case 1:
			return "rgb888";
		case 2:
			return "rgba8888";
		case 3:
			return "argb8888";
		case 4:
			return "uyvy";
		case 5:
			return "yuyv";
		default:
			DDPDUMP("%s: unknown fmt=%d, module=%d\n",
				__func__, fmt, module);
			return "unknown";
		}
	} else {
		DDPDUMP("%s: unknown module=%d\n", __func__, module);
	}

	return "unknown";
}

static char *ddp_clock_0(int bit)
{
	switch (bit) {
	case 7:
		return "ovl0, ";
	case 8:
		return "ovl0_2L, ";
	case 9:
		return "disp_rsz0, ";
	case 10:
		return "rdma0, ";
	case 11:
		return "wdma0, ";
	case 12:
		return "color, ";
	case 13:
		return "ccorr, ";
	case 14:
		return "aal, ";
	case 15:
		return "gamma, ";
	case 16:
		return "dither, ";
	case 17:
		return "dsi0_mm(cg), ";
	case 19:
		return "smi_common(cg), ";
	case 20:
		return "smi_larb0(cg), ";
	case 21:
		return "smi_comm0(cg), ";
	case 22:
		return "smi_comm1(cg), ";
	case 28:
		return "dsi0_interface(cg), ";
	case 29:
		return "26M, ";
	default:
		return NULL;
	}
}

/* no need ddp_clock_1 */
/*
 *
 *	static char *ddp_clock_1(int bit)
 *	{
 *		switch (bit) {
 *		case 0:
 *			return "dsi0_mm(cg), ";
 *		case 1:
 *			return "dsi0_interface(cg), ";
 *		case 7:
 *			return "26M, ";
 *		default:
 *			return NULL;
 *		}
 *	}
 */

static void mutex_dump_reg(void)
{
	unsigned long module_base = DISPSYS_MUTEX_BASE;
	char module_name[] = "MUTEX:";

	DDPDUMP("== START: DISP MUTEX registers ==\n");
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x0, INREG32(module_base + 0x0),
		0x4, INREG32(module_base + 0x4),
		0x8, INREG32(module_base + 0x8),
		0xC, INREG32(module_base + 0xC));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x10, INREG32(module_base + 0x10),
		0x18, INREG32(module_base + 0x18),
		0x1C, INREG32(module_base + 0x1C),
		0x020, INREG32(module_base + 0x020));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x024, INREG32(module_base + 0x024),
		0x028, INREG32(module_base + 0x028),
		0x02C, INREG32(module_base + 0x02C),
		0x030, INREG32(module_base + 0x030));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x040, INREG32(module_base + 0x040),
		0x044, INREG32(module_base + 0x044),
		0x048, INREG32(module_base + 0x048),
		0x04C, INREG32(module_base + 0x04C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x050, INREG32(module_base + 0x050),
		0x060, INREG32(module_base + 0x060),
		0x064, INREG32(module_base + 0x064),
		0x068, INREG32(module_base + 0x068));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x06C, INREG32(module_base + 0x06C),
		0x070, INREG32(module_base + 0x070),
		0x080, INREG32(module_base + 0x080),
		0x084, INREG32(module_base + 0x084));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x088, INREG32(module_base + 0x088),
		0x08C, INREG32(module_base + 0x08C),
		0x090, INREG32(module_base + 0x090),
		0x0A0, INREG32(module_base + 0x0A0));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x0A4, INREG32(module_base + 0x0A4),
		0x0A8, INREG32(module_base + 0x0A8),
		0x0AC, INREG32(module_base + 0x0AC),
		0x0B0, INREG32(module_base + 0x0B0));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x0C0, INREG32(module_base + 0x0C0),
		0x0C4, INREG32(module_base + 0x0C4),
		0x0C8, INREG32(module_base + 0x0C8),
		0x0CC, INREG32(module_base + 0x0CC));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x0D0, INREG32(module_base + 0x0D0),
		0x0E0, INREG32(module_base + 0x0E0),
		0x0E4, INREG32(module_base + 0x0E4),
		0x0E8, INREG32(module_base + 0x0E8));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x0EC, INREG32(module_base + 0x0EC),
		0x0F0, INREG32(module_base + 0x0F0),
		0x100, INREG32(module_base + 0x100),
		0x104, INREG32(module_base + 0x104));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x108, INREG32(module_base + 0x108),
		0x10C, INREG32(module_base + 0x10C),
		0x110, INREG32(module_base + 0x110),
		0x120, INREG32(module_base + 0x120));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x124, INREG32(module_base + 0x124),
		0x128, INREG32(module_base + 0x128),
		0x12C, INREG32(module_base + 0x12C),
		0x130, INREG32(module_base + 0x130));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x140, INREG32(module_base + 0x140),
		0x144, INREG32(module_base + 0x144),
		0x148, INREG32(module_base + 0x148),
		0x14C, INREG32(module_base + 0x14C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x150, INREG32(module_base + 0x150),
		0x160, INREG32(module_base + 0x160),
		0x164, INREG32(module_base + 0x164),
		0x168, INREG32(module_base + 0x168));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x16C, INREG32(module_base + 0x16C),
		0x170, INREG32(module_base + 0x170),
		0x180, INREG32(module_base + 0x180),
		0x184, INREG32(module_base + 0x184));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x188, INREG32(module_base + 0x188),
		0x18C, INREG32(module_base + 0x18C),
		0x190, INREG32(module_base + 0x190),
		0x300, INREG32(module_base + 0x300));
	DDPDUMP("%s 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		module_name,
		0x304, INREG32(module_base + 0x304),
		0x30C, INREG32(module_base + 0x30C));
	DDPDUMP("-- END: DISP MUTEX registers --\n");

}


static void mutex_dump_analysis(void)
{
	int i = 0;
	int j = 0;
	char mutex_module[512] = { '\0' };
	char *p = NULL;
	int len = 0;
	unsigned int val;

	DDPDUMP("== DISP Mutex Analysis ==\n");
	for (i = 0; i < 5; i++) {
		p = mutex_module;
		len = 0;
		if (DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD0(i)) == 0)
			continue;

		val = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_SOF(i));
		len = sprintf(p, "MUTEX%d :SOF=%s,EOF=%s,WAIT=%d,module=(",
			i,
			ddp_get_mutex_sof_name(
				REG_FLD_VAL_GET(SOF_FLD_MUTEX0_SOF, val)),
			ddp_get_mutex_sof_name(
				REG_FLD_VAL_GET(SOF_FLD_MUTEX0_EOF, val)),
			REG_FLD_VAL_GET(SOF_FLD_MUTEX0_SOF_WAIT, val));

		p += len;
		for (j = 0; j < 32; j++) {
			unsigned int regval =
				DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD0(i));

			if ((regval & (1 << j))) {
				len = sprintf(p, "%s,",
					ddp_get_mutex_module0_name(j));
				p += len;
			}
		}
		DDPDUMP("%s)\n", mutex_module);
	}
}

static void mmsys_config_dump_reg(void)
{
	unsigned long module_base = DISPSYS_CONFIG_BASE;
	char module_name[] = "MMSYS_CONFIG:";

	DDPDUMP("== START: DISP MMSYS_CONFIG registers ==\n");
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x00C, INREG32(module_base + 0x00C),
		0x010, INREG32(module_base + 0x010));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x014, INREG32(module_base + 0x014),
		0x018, INREG32(module_base + 0x018),
		0x020, INREG32(module_base + 0x020),
		0x024, INREG32(module_base + 0x024));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x028, INREG32(module_base + 0x028),
		0x02C, INREG32(module_base + 0x02C),
		0x030, INREG32(module_base + 0x030),
		0x034, INREG32(module_base + 0x034));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x038, INREG32(module_base + 0x038),
		0x048, INREG32(module_base + 0x048),
		0x0F0, INREG32(module_base + 0x0F0),
		0x0F4, INREG32(module_base + 0x0F4));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x0F8, INREG32(module_base + 0x0F8),
		0x100, INREG32(module_base + 0x100),
		0x104, INREG32(module_base + 0x104),
		0x108, INREG32(module_base + 0x108));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x110, INREG32(module_base + 0x110),
		0x114, INREG32(module_base + 0x114),
		0x118, INREG32(module_base + 0x118),
		0x120, INREG32(module_base + 0x120));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x124, INREG32(module_base + 0x124),
		0x128, INREG32(module_base + 0x128),
		0x130, INREG32(module_base + 0x130),
		0x134, INREG32(module_base + 0x134));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x138, INREG32(module_base + 0x138),
		0x140, INREG32(module_base + 0x140),
		0x144, INREG32(module_base + 0x144),
		0x150, INREG32(module_base + 0x150));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x180, INREG32(module_base + 0x180),
		0x184, INREG32(module_base + 0x184),
		0x190, INREG32(module_base + 0x190),
		0x200, INREG32(module_base + 0x200));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x204, INREG32(module_base + 0x204),
		0x208, INREG32(module_base + 0x208),
		0x20C, INREG32(module_base + 0x20C),
		0x210, INREG32(module_base + 0x210));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x214, INREG32(module_base + 0x214),
		0x218, INREG32(module_base + 0x218),
		0x220, INREG32(module_base + 0x220),
		0x224, INREG32(module_base + 0x224));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x228, INREG32(module_base + 0x228),
		0x22C, INREG32(module_base + 0x22C),
		0x230, INREG32(module_base + 0x230),
		0x234, INREG32(module_base + 0x234));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x238, INREG32(module_base + 0x238),
		0x800, INREG32(module_base + 0x800),
		0x804, INREG32(module_base + 0x804),
		0x808, INREG32(module_base + 0x808));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x80C, INREG32(module_base + 0x80C),
		0x810, INREG32(module_base + 0x810),
		0x814, INREG32(module_base + 0x814),
		0x818, INREG32(module_base + 0x818));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x81C, INREG32(module_base + 0x81C),
		0x820, INREG32(module_base + 0x820),
		0x824, INREG32(module_base + 0x824),
		0x828, INREG32(module_base + 0x828));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x82C, INREG32(module_base + 0x82C),
		0x830, INREG32(module_base + 0x830),
		0x834, INREG32(module_base + 0x834),
		0x838, INREG32(module_base + 0x838));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x83C, INREG32(module_base + 0x83C),
		0x840, INREG32(module_base + 0x840),
		0x844, INREG32(module_base + 0x844),
		0x848, INREG32(module_base + 0x848));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x84C, INREG32(module_base + 0x84C),
		0x854, INREG32(module_base + 0x854),
		0x858, INREG32(module_base + 0x858),
		0x85C, INREG32(module_base + 0x85C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x860, INREG32(module_base + 0x860),
		0x864, INREG32(module_base + 0x864),
		0x868, INREG32(module_base + 0x868),
		0x870, INREG32(module_base + 0x870));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x874, INREG32(module_base + 0x874),
		0x878, INREG32(module_base + 0x878),
		0x88C, INREG32(module_base + 0x88C),
		0x890, INREG32(module_base + 0x890));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x894, INREG32(module_base + 0x894),
		0x898, INREG32(module_base + 0x898),
		0x89C, INREG32(module_base + 0x89C),
		0x8A0, INREG32(module_base + 0x8A0));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x8A4, INREG32(module_base + 0x8A4),
		0x8A8, INREG32(module_base + 0x8A8),
		0x8AC, INREG32(module_base + 0x8AC),
		0x8B0, INREG32(module_base + 0x8B0));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x8B4, INREG32(module_base + 0x8B4),
		0x8B8, INREG32(module_base + 0x8B8),
		0x8C0, INREG32(module_base + 0x8C0),
		0x8C4, INREG32(module_base + 0x8C4));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x8CC, INREG32(module_base + 0x8CC),
		0x8D0, INREG32(module_base + 0x8D0),
		0x8D4, INREG32(module_base + 0x8D4),
		0x8D8, INREG32(module_base + 0x8D8));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x8DC, INREG32(module_base + 0x8DC),
		0x8E0, INREG32(module_base + 0x8E0),
		0x8E4, INREG32(module_base + 0x8E4),
		0x8E8, INREG32(module_base + 0x8E8));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x8EC, INREG32(module_base + 0x8EC),
		0x8F0, INREG32(module_base + 0x8F0),
		0x908, INREG32(module_base + 0x908),
		0x90C, INREG32(module_base + 0x90C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x910, INREG32(module_base + 0x910),
		0x914, INREG32(module_base + 0x914),
		0x918, INREG32(module_base + 0x918),
		0x91C, INREG32(module_base + 0x91C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x920, INREG32(module_base + 0x920),
		0x924, INREG32(module_base + 0x924),
		0x928, INREG32(module_base + 0x928),
		0x934, INREG32(module_base + 0x934));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0x938, INREG32(module_base + 0x938),
		0x93C, INREG32(module_base + 0x93C),
		0x940, INREG32(module_base + 0x940),
		0x944, INREG32(module_base + 0x944));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xF00, INREG32(module_base + 0xF00),
		0xF04, INREG32(module_base + 0xF04),
		0xF08, INREG32(module_base + 0xF08),
		0xF0C, INREG32(module_base + 0xF0C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xF10, INREG32(module_base + 0xF10),
		0xF14, INREG32(module_base + 0xF14),
		0xF18, INREG32(module_base + 0xF18),
		0xF1C, INREG32(module_base + 0xF1C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xF20, INREG32(module_base + 0xF20),
		0xF24, INREG32(module_base + 0xF24),
		0xF28, INREG32(module_base + 0xF28),
		0xF2C, INREG32(module_base + 0xF2C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xF30, INREG32(module_base + 0xF30),
		0xF34, INREG32(module_base + 0xF34),
		0xF38, INREG32(module_base + 0xF38),
		0xF3C, INREG32(module_base + 0xF3C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xF40, INREG32(module_base + 0xF40),
		0xF44, INREG32(module_base + 0xF44),
		0xF48, INREG32(module_base + 0xF48),
		0xF4C, INREG32(module_base + 0xF4C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xF50, INREG32(module_base + 0xF50),
		0xF54, INREG32(module_base + 0xF54),
		0xF58, INREG32(module_base + 0xF58),
		0xF5C, INREG32(module_base + 0xF5C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xF60, INREG32(module_base + 0xF60),
		0xF64, INREG32(module_base + 0xF64),
		0xF68, INREG32(module_base + 0xF68),
		0xF6C, INREG32(module_base + 0xF6C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xF80, INREG32(module_base + 0xF80),
		0xF84, INREG32(module_base + 0xF84),
		0xF88, INREG32(module_base + 0xF88),
		0xF8C, INREG32(module_base + 0xF8C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xF90, INREG32(module_base + 0xF90),
		0xF94, INREG32(module_base + 0xF94),
		0xF98, INREG32(module_base + 0xF98),
		0xF9C, INREG32(module_base + 0xF9C));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xFA0, INREG32(module_base + 0xFA0),
		0xFA4, INREG32(module_base + 0xFA4),
		0xFA8, INREG32(module_base + 0xFA8),
		0xFAC, INREG32(module_base + 0xFAC));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xFB0, INREG32(module_base + 0xFB0),
		0xFB4, INREG32(module_base + 0xFB4),
		0xFB8, INREG32(module_base + 0xFB8),
		0xFBC, INREG32(module_base + 0xFBC));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xFC0, INREG32(module_base + 0xFC0),
		0XFC4, INREG32(module_base + 0XFC4),
		0xFC8, INREG32(module_base + 0xFC8),
		0xFCC, INREG32(module_base + 0xFCC));
	DDPDUMP("%s 0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		module_name,
		0xFD0, INREG32(module_base + 0xFD0),
		0xFD4, INREG32(module_base + 0xFD4),
		0xFD8, INREG32(module_base + 0xFD8),
		0xFDC, INREG32(module_base + 0xFDC));
	DDPDUMP("%s 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		module_name,
		0xFE0, INREG32(module_base + 0xFE0),
		0xFE4, INREG32(module_base + 0xFE4));
	DDPDUMP("-- END: DISP MMSYS_CONFIG registers --\n");
}

 /*  ------ clock:
  * Before power on mmsys:
  * CLK_CFG_0_CLR (address is 0x10000048) = 0x80000000 (bit 31).
  * Before using DISP_PWM0 or DISP_PWM1:
  * CLK_CFG_1_CLR(address is 0x10000058)=0x80 (bit 7).
  * Before using DPI pixel clock:
  * CLK_CFG_6_CLR(address is 0x100000A8)=0x80 (bit 7).
  *
  * Only need to enable the corresponding bits of MMSYS_CG_CON0 and
  * MMSYS_CG_CON1 for the modules: smi_common, larb0, mdp_crop, fake_eng,
  * mutex_32k, pwm0, pwm1, dsi0, dsi1, dpi.
  * Other bits could keep 1. Suggest to keep smi_common and larb0
  * always clock on.
  *
  * --------valid & ready
  * example:
  * ovl0 -> ovl0_mout_ready=1 means engines after ovl_mout are
  *         ready for receiving data
  *	ovl0_mout_ready=0 means ovl0_mout can not receive data,
  *         maybe ovl0_mout or after engines config error
  * ovl0 -> ovl0_mout_valid=1 means engines before ovl0_mout is OK,
  *	ovl0_mout_valid=0 means ovl can not transfer data to ovl0_mout,
  *         means ovl0 or before engines are not ready.
  */

static void mmsys_config_dump_analysis(void)
{
	unsigned int i = 0;
	unsigned int reg = 0;
	char clock_on[512] = { '\0' };
	char *pos = NULL;
	char *name;
	/* int len = 0; */

	unsigned int valid0 = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_VALID_0);
	unsigned int valid1 = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_VALID_1);
	unsigned int ready0 = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_READY_0);
	unsigned int ready1 = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_READY_1);
	unsigned int greq = DISP_REG_GET(DISP_REG_CONFIG_SMI_LARB0_GREQ);

	DDPDUMP("== DISP MMSYS_CONFIG ANALYSIS ==\n");
#if 0 /* TODO: mmsys clk?? */
	DDPDUMP("mmsys clock=0x%x, CG_CON0=0x%x, CG_CON1=0x%x\n",
		DISP_REG_GET(DISP_REG_CLK_CFG_0_MM_CLK),
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0),
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1));
	if ((DISP_REG_GET(DISP_REG_CLK_CFG_0_MM_CLK) >> 31) & 0x1)
		DDPERR("mmsys clock abnormal!!\n");
#endif

	reg = DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0);
	for (i = 0; i < 32; i++) {
		if ((reg & (1 << i)) == 0) {
			name = ddp_clock_0(i);
			if (name)
				strncat(clock_on, name,
					(sizeof(clock_on) -
					strlen(clock_on) - 1));
		}
	}

/* no need ddp_clock_1 */
/*
 *	reg = DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1);
 *	for (i = 0; i < 32; i++) {
 *		if ((reg & (1 << i)) == 0) {
 *			name = ddp_clock_1(i);
 *			if (name)
 *				strncat(clock_on, name,
 *					(sizeof(clock_on) -
 *					strlen(clock_on) - 1));
 *		}
 *	}
 */
	DDPDUMP("clock on modules:%s\n", clock_on);

	DDPDUMP("valid0=0x%x,valid1=0x%x,ready0=0x%x,ready1=0x%x,greq=0%x\n",
		valid0, valid1, ready0, ready1, greq);
	for (i = 0; i < 32; i++) {
		name = ddp_signal_0(i);
		if (!name)
			continue;

		pos = clock_on;

		if ((valid0 & (1 << i)))
			pos += sprintf(pos, "%s,", "v");
		else
			pos += sprintf(pos, "%s,", "n");

		if ((ready0 & (1 << i)))
			pos += sprintf(pos, "%s", "r");
		else
			pos += sprintf(pos, "%s", "n");

		pos += sprintf(pos, ": %s", name);

		DDPDUMP("%s\n", clock_on);
	}

	for (i = 0; i < 32; i++) {
		name = ddp_signal_1(i);
		if (!name)
			continue;

		pos = clock_on;

		if ((valid1 & (1 << i)))
			pos += sprintf(pos, "%s,", "v");
		else
			pos += sprintf(pos, "%s,", "n");

		if ((ready1 & (1 << i)))
			pos += sprintf(pos, "%s", "r");
		else
			pos += sprintf(pos, "%s", "n");

		pos += sprintf(pos, ": %s", name);

		DDPDUMP("%s\n", clock_on);
	}

	/* greq: 1 means SMI dose not grant, maybe SMI hang */
	if (greq)
		DDPDUMP("greq: 1 means SMI dose not grant, maybe SMI hang");

	clock_on[0] = '\0';
	for (i = 0; i < 32; i++) {
		if (greq & (1 << i)) {
			name = ddp_greq_name(i);
			if (!name)
				continue;
			strncat(clock_on, name,
				(sizeof(clock_on) - strlen(clock_on) - 1));
		}
	}
	DDPDUMP("%s\n", clock_on);

	/* dump SMI status, when maybe SMI hang */
	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		if (greq)
			smi_debug_bus_hang_detect(false, "DISP");
	}
}

static void gamma_dump_reg(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned int offset = 0x1000;

	if (module == DISP_MODULE_GAMMA0)
		i = 0;
	else
		i = 1;


	DDPDUMP("== DISP GAMMA%d REGS ==\n", i);
	DDPDUMP("(0x000)GA_EN=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_EN + i * offset));
	DDPDUMP("(0x004)GA_RESET=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_RESET + i * offset));
	DDPDUMP("(0x008)GA_INTEN=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_INTEN + i * offset));
	DDPDUMP("(0x00c)GA_INTSTA=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_INTSTA + i * offset));
	DDPDUMP("(0x010)GA_STATUS=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_STATUS + i * offset));
	DDPDUMP("(0x020)GA_CFG=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_CFG + i * offset));
	DDPDUMP("(0x024)GA_IN_COUNT=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_INPUT_COUNT + i * offset));
	DDPDUMP("(0x028)GA_OUT_COUNT=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_OUTPUT_COUNT + i * offset));
	DDPDUMP("(0x02c)GA_CHKSUM=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_CHKSUM + i * offset));
	DDPDUMP("(0x030)GA_SIZE=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_SIZE + i * offset));
	DDPDUMP("(0x0c0)GA_DUMMY_REG=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_DUMMY_REG + i * offset));
	DDPDUMP("(0x800)GA_LUT=0x%x\n",
		DISP_REG_GET(DISP_REG_GAMMA_LUT + i * offset));
}

static void gamma_dump_analysis(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned int offset = 0x1000;

	if (module == DISP_MODULE_GAMMA0)
		i = 0;
	else
		i = 1;


	DDPDUMP("== DISP GAMMA%d ANALYSIS ==\n", i);
	DDPDUMP("gamma: en=%d, w=%d, h=%d,\n",
		DISP_REG_GET(DISP_REG_GAMMA_EN + i * offset),
		(DISP_REG_GET(DISP_REG_GAMMA_SIZE + i * offset) >> 16) &
			0x1fff,
		DISP_REG_GET(DISP_REG_GAMMA_SIZE + i * offset) & 0x1fff);
	DDPDUMP("gamma: in_p_cnt=%d, in_l_cnt=%d, out_p_cnt=%d, out_l_cnt=%d\n",
		DISP_REG_GET(DISP_REG_GAMMA_INPUT_COUNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_REG_GAMMA_INPUT_COUNT + i * offset) >> 16) &
			0x1fff,
		DISP_REG_GET(DISP_REG_GAMMA_OUTPUT_COUNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_REG_GAMMA_OUTPUT_COUNT + i * offset) >>
			16) & 0x1fff);

}


static void color_dump_reg(enum DISP_MODULE_ENUM module)
{
	int idx = 0;
	unsigned long module_base = DISPSYS_COLOR0_BASE;

	DDPDUMP("== START: DISP COLOR%d registers ==\n", idx);
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000400, INREG32(module_base + 0x00000400),
		0x00000404, INREG32(module_base + 0x00000404),
		0x00000408, INREG32(module_base + 0x00000408),
		0x0000040C, INREG32(module_base + 0x0000040C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000410, INREG32(module_base + 0x00000410),
		0x00000418, INREG32(module_base + 0x00000418),
		0x0000041C, INREG32(module_base + 0x0000041C),
		0x00000420, INREG32(module_base + 0x00000420));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000428, INREG32(module_base + 0x00000428),
		0x0000042C, INREG32(module_base + 0x0000042C),
		0x00000430, INREG32(module_base + 0x00000430),
		0x00000434, INREG32(module_base + 0x00000434));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000438, INREG32(module_base + 0x00000438),
		0x00000484, INREG32(module_base + 0x00000484),
		0x00000488, INREG32(module_base + 0x00000488),
		0x0000048C, INREG32(module_base + 0x0000048C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000490, INREG32(module_base + 0x00000490),
		0x00000494, INREG32(module_base + 0x00000494),
		0x00000498, INREG32(module_base + 0x00000498),
		0x0000049C, INREG32(module_base + 0x0000049C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x000004A0, INREG32(module_base + 0x000004A0),
		0x000004A4, INREG32(module_base + 0x000004A4),
		0x000004A8, INREG32(module_base + 0x000004A8),
		0x000004AC, INREG32(module_base + 0x000004AC));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x000004B0, INREG32(module_base + 0x000004B0),
		0x000004B4, INREG32(module_base + 0x000004B4),
		0x000004B8, INREG32(module_base + 0x000004B8),
		0x000004BC, INREG32(module_base + 0x000004BC));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000620, INREG32(module_base + 0x00000620),
		0x00000624, INREG32(module_base + 0x00000624),
		0x00000628, INREG32(module_base + 0x00000628),
		0x0000062C, INREG32(module_base + 0x0000062C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000630, INREG32(module_base + 0x00000630),
		0x00000740, INREG32(module_base + 0x00000740),
		0x0000074C, INREG32(module_base + 0x0000074C),
		0x00000768, INREG32(module_base + 0x00000768));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x0000076C, INREG32(module_base + 0x0000076C),
		0x0000079C, INREG32(module_base + 0x0000079C),
		0x000007E0, INREG32(module_base + 0x000007E0),
		0x000007E4, INREG32(module_base + 0x000007E4));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x000007E8, INREG32(module_base + 0x000007E8),
		0x000007EC, INREG32(module_base + 0x000007EC),
		0x000007F0, INREG32(module_base + 0x000007F0),
		0x000007FC, INREG32(module_base + 0x000007FC));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000800, INREG32(module_base + 0x00000800),
		0x00000804, INREG32(module_base + 0x00000804),
		0x00000808, INREG32(module_base + 0x00000808),
		0x0000080C, INREG32(module_base + 0x0000080C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000810, INREG32(module_base + 0x00000810),
		0x00000814, INREG32(module_base + 0x00000814),
		0x00000818, INREG32(module_base + 0x00000818),
		0x0000081C, INREG32(module_base + 0x0000081C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000820, INREG32(module_base + 0x00000820),
		0x00000824, INREG32(module_base + 0x00000824),
		0x00000828, INREG32(module_base + 0x00000828),
		0x0000082C, INREG32(module_base + 0x0000082C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000830, INREG32(module_base + 0x00000830),
		0x00000834, INREG32(module_base + 0x00000834),
		0x00000838, INREG32(module_base + 0x00000838),
		0x0000083C, INREG32(module_base + 0x0000083C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000840, INREG32(module_base + 0x00000840),
		0x00000844, INREG32(module_base + 0x00000844),
		0x00000848, INREG32(module_base + 0x00000848),
		0x0000084C, INREG32(module_base + 0x0000084C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000850, INREG32(module_base + 0x00000850),
		0x00000854, INREG32(module_base + 0x00000854),
		0x00000858, INREG32(module_base + 0x00000858),
		0x0000085C, INREG32(module_base + 0x0000085C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000860, INREG32(module_base + 0x00000860),
		0x00000864, INREG32(module_base + 0x00000864),
		0x00000868, INREG32(module_base + 0x00000868),
		0x0000086C, INREG32(module_base + 0x0000086C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000870, INREG32(module_base + 0x00000870),
		0x00000874, INREG32(module_base + 0x00000874),
		0x00000878, INREG32(module_base + 0x00000878),
		0x0000087C, INREG32(module_base + 0x0000087C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000880, INREG32(module_base + 0x00000880),
		0x00000884, INREG32(module_base + 0x00000884),
		0x00000888, INREG32(module_base + 0x00000888),
		0x0000088C, INREG32(module_base + 0x0000088C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000890, INREG32(module_base + 0x00000890),
		0x00000894, INREG32(module_base + 0x00000894),
		0x00000898, INREG32(module_base + 0x00000898),
		0x0000089C, INREG32(module_base + 0x0000089C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x000008A0, INREG32(module_base + 0x000008A0),
		0x000008A4, INREG32(module_base + 0x000008A4),
		0x000008A8, INREG32(module_base + 0x000008A8),
		0x000008AC, INREG32(module_base + 0x000008AC));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x000008B0, INREG32(module_base + 0x000008B0),
		0x000008B4, INREG32(module_base + 0x000008B4),
		0x000008B8, INREG32(module_base + 0x000008B8),
		0x000008BC, INREG32(module_base + 0x000008BC));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x000008C0, INREG32(module_base + 0x000008C0),
		0x000008C4, INREG32(module_base + 0x000008C4),
		0x000008C8, INREG32(module_base + 0x000008C8),
		0x000008CC, INREG32(module_base + 0x000008CC));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x000008D0, INREG32(module_base + 0x000008D0),
		0x000008D4, INREG32(module_base + 0x000008D4),
		0x000008D8, INREG32(module_base + 0x000008D8),
		0x000008DC, INREG32(module_base + 0x000008DC));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x000008E0, INREG32(module_base + 0x000008E0),
		0x000008E4, INREG32(module_base + 0x000008E4),
		0x000008E8, INREG32(module_base + 0x000008E8),
		0x000008EC, INREG32(module_base + 0x000008EC));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x000008F0, INREG32(module_base + 0x000008F0),
		0x000008F4, INREG32(module_base + 0x000008F4),
		0x000008F8, INREG32(module_base + 0x000008F8),
		0x000008FC, INREG32(module_base + 0x000008FC));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000900, INREG32(module_base + 0x00000900),
		0x00000904, INREG32(module_base + 0x00000904),
		0x00000908, INREG32(module_base + 0x00000908),
		0x0000090C, INREG32(module_base + 0x0000090C));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000910, INREG32(module_base + 0x00000910),
		0x00000914, INREG32(module_base + 0x00000914),
		0x00000C00, INREG32(module_base + 0x00000C00),
		0x00000C04, INREG32(module_base + 0x00000C04));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000C08, INREG32(module_base + 0x00000C08),
		0x00000C0C, INREG32(module_base + 0x00000C0C),
		0x00000C10, INREG32(module_base + 0x00000C10),
		0x00000C14, INREG32(module_base + 0x00000C14));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000C18, INREG32(module_base + 0x00000C18),
		0x00000C28, INREG32(module_base + 0x00000C28),
		0x00000C50, INREG32(module_base + 0x00000C50),
		0x00000C54, INREG32(module_base + 0x00000C54));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000C60, INREG32(module_base + 0x00000C60),
		0x00000CA0, INREG32(module_base + 0x00000CA0),
		0x00000CB0, INREG32(module_base + 0x00000CB0),
		0x00000CF0, INREG32(module_base + 0x00000CF0));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000CF4, INREG32(module_base + 0x00000CF4),
		0x00000CF8, INREG32(module_base + 0x00000CF8),
		0x00000CFC, INREG32(module_base + 0x00000CFC),
		0x00000D00, INREG32(module_base + 0x00000D00));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000D04, INREG32(module_base + 0x00000D04),
		0x00000D08, INREG32(module_base + 0x00000D08),
		0x00000D0C, INREG32(module_base + 0x00000D0C),
		0x00000D10, INREG32(module_base + 0x00000D10));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000D14, INREG32(module_base + 0x00000D14),
		0x00000D18, INREG32(module_base + 0x00000D18),
		0x00000D1C, INREG32(module_base + 0x00000D1C),
		0x00000D20, INREG32(module_base + 0x00000D20));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000D24, INREG32(module_base + 0x00000D24),
		0x00000D28, INREG32(module_base + 0x00000D28),
		0x00000D2C, INREG32(module_base + 0x00000D2C),
		0x00000D30, INREG32(module_base + 0x00000D30));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000D34, INREG32(module_base + 0x00000D34),
		0x00000D38, INREG32(module_base + 0x00000D38),
		0x00000D3C, INREG32(module_base + 0x00000D3C),
		0x00000D40, INREG32(module_base + 0x00000D40));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000D44, INREG32(module_base + 0x00000D44),
		0x00000D48, INREG32(module_base + 0x00000D48),
		0x00000D4C, INREG32(module_base + 0x00000D4C),
		0x00000D50, INREG32(module_base + 0x00000D50));
	DDPDUMP("COLOR%d:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		idx,
		0x00000D54, INREG32(module_base + 0x00000D54),
		0x00000D58, INREG32(module_base + 0x00000D58),
		0x00000D5C, INREG32(module_base + 0x00000D5C));
	DDPDUMP("-- END: DISP COLOR%d registers --\n", idx);

}

static void color_dump_analysis(enum DISP_MODULE_ENUM module)
{
	int index = 0;

	DDPDUMP("== DISP COLOR%d ANALYSIS ==\n", index);
	DDPDUMP("color%d: bypass=%d, w=%d, h=%d, pixel_cnt=%d, line_cnt=%d,\n",
		index,
		(DISP_REG_GET(DISP_COLOR_CFG_MAIN) >> 7) & 0x1,
		DISP_REG_GET(DISP_COLOR_INTERNAL_IP_WIDTH),
		DISP_REG_GET(DISP_COLOR_INTERNAL_IP_HEIGHT),
		DISP_REG_GET(DISP_COLOR_PXL_CNT_MAIN) & 0xffff,
		(DISP_REG_GET(DISP_COLOR_LINE_CNT_MAIN) >> 16) & 0x1fff);

}

static void aal_dump_reg(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned long module_base = DISPSYS_AAL0_BASE;

	if (module == DISP_MODULE_AAL0)
		i = 0;
	else
		i = 1;

	DDPDUMP("== START: DISP AAL%d registers ==\n", i);
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000000, INREG32(module_base + 0x00000000),
		0x00000004, INREG32(module_base + 0x00000004),
		0x00000008, INREG32(module_base + 0x00000008),
		0x0000000C, INREG32(module_base + 0x0000000C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000010, INREG32(module_base + 0x00000010),
		0x00000020, INREG32(module_base + 0x00000020),
		0x00000024, INREG32(module_base + 0x00000024),
		0x00000028, INREG32(module_base + 0x00000028));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x0000002C, INREG32(module_base + 0x0000002C),
		0x00000030, INREG32(module_base + 0x00000030),
		0x000000B0, INREG32(module_base + 0x000000B0),
		0x000000C0, INREG32(module_base + 0x000000C0));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x000000FC, INREG32(module_base + 0x000000FC),
		0x00000204, INREG32(module_base + 0x00000204),
		0x0000020C, INREG32(module_base + 0x0000020C),
		0x00000214, INREG32(module_base + 0x00000214));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x0000021C, INREG32(module_base + 0x0000021C),
		0x00000224, INREG32(module_base + 0x00000224),
		0x00000228, INREG32(module_base + 0x00000228),
		0x0000022C, INREG32(module_base + 0x0000022C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000230, INREG32(module_base + 0x00000230),
		0x00000234, INREG32(module_base + 0x00000234),
		0x00000238, INREG32(module_base + 0x00000238),
		0x0000023C, INREG32(module_base + 0x0000023C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000240, INREG32(module_base + 0x00000240),
		0x00000244, INREG32(module_base + 0x00000244),
		0x00000248, INREG32(module_base + 0x00000248),
		0x0000024C, INREG32(module_base + 0x0000024C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000250, INREG32(module_base + 0x00000250),
		0x00000254, INREG32(module_base + 0x00000254),
		0x00000258, INREG32(module_base + 0x00000258),
		0x0000025C, INREG32(module_base + 0x0000025C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000260, INREG32(module_base + 0x00000260),
		0x00000264, INREG32(module_base + 0x00000264),
		0x00000268, INREG32(module_base + 0x00000268),
		0x0000026C, INREG32(module_base + 0x0000026C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000270, INREG32(module_base + 0x00000270),
		0x00000274, INREG32(module_base + 0x00000274),
		0x00000278, INREG32(module_base + 0x00000278),
		0x0000027C, INREG32(module_base + 0x0000027C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000280, INREG32(module_base + 0x00000280),
		0x00000284, INREG32(module_base + 0x00000284),
		0x00000288, INREG32(module_base + 0x00000288),
		0x0000028C, INREG32(module_base + 0x0000028C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000290, INREG32(module_base + 0x00000290),
		0x00000294, INREG32(module_base + 0x00000294),
		0x00000298, INREG32(module_base + 0x00000298),
		0x0000029C, INREG32(module_base + 0x0000029C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x000002A0, INREG32(module_base + 0x000002A0),
		0x000002A4, INREG32(module_base + 0x000002A4),
		0x00000358, INREG32(module_base + 0x00000358),
		0x0000035C, INREG32(module_base + 0x0000035C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000360, INREG32(module_base + 0x00000360),
		0x00000364, INREG32(module_base + 0x00000364),
		0x00000368, INREG32(module_base + 0x00000368),
		0x0000036C, INREG32(module_base + 0x0000036C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000370, INREG32(module_base + 0x00000370),
		0x00000374, INREG32(module_base + 0x00000374),
		0x00000378, INREG32(module_base + 0x00000378),
		0x0000037C, INREG32(module_base + 0x0000037C));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000380, INREG32(module_base + 0x00000380),
		0x000003B0, INREG32(module_base + 0x000003B0),
		0x0000040C, INREG32(module_base + 0x0000040C),
		0x00000410, INREG32(module_base + 0x00000410));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000414, INREG32(module_base + 0x00000414),
		0x00000418, INREG32(module_base + 0x00000418),
		0x0000041C, INREG32(module_base + 0x0000041C),
		0x00000420, INREG32(module_base + 0x00000420));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000424, INREG32(module_base + 0x00000424),
		0x00000428, INREG32(module_base + 0x00000428),
		0x0000042C, INREG32(module_base + 0x0000042C),
		0x00000430, INREG32(module_base + 0x00000430));
	DDPDUMP("AAL:0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x,0x%04x=0x%08x\n",
		0x00000434, INREG32(module_base + 0x00000434),
		0x00000440, INREG32(module_base + 0x00000440),
		0x00000444, INREG32(module_base + 0x00000444),
		0x00000448, INREG32(module_base + 0x00000448));
	DDPDUMP("-- END: DISP AAL%d registers --\n", i);
}

static void aal_dump_analysis(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned int offset = 0x1000;

	if (module == DISP_MODULE_AAL0)
		i = 0;
	else
		i = 1;


	DDPDUMP("== DISP AAL ANALYSIS ==\n");
	DDPDUMP("aal:bypass=%d,relay=%d,en=%d,w=%d,h=%d,in(%d,%d),out(%d,%d)\n",
		DISP_REG_GET(DISP_AAL_EN + i * offset) == 0x0,
		DISP_REG_GET(DISP_AAL_CFG + i * offset) & 0x01,
		DISP_REG_GET(DISP_AAL_EN + i * offset),
		(DISP_REG_GET(DISP_AAL_SIZE + i * offset) >> 16) & 0x1fff,
		DISP_REG_GET(DISP_AAL_SIZE + i * offset) & 0x1fff,
		DISP_REG_GET(DISP_AAL_IN_CNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_AAL_IN_CNT + i * offset) >> 16) & 0x1fff,
		DISP_REG_GET(DISP_AAL_OUT_CNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_AAL_OUT_CNT + i * offset) >> 16) & 0x1fff);
}

static void pwm_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = DISPSYS_PWM0_BASE;

	DDPDUMP("== START: DISP PWM0 registers ==\n");
	DDPDUMP("PWM0:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x0, INREG32(module_base + 0x0),
		0x4, INREG32(module_base + 0x4),
		0x8, INREG32(module_base + 0x8),
		0xC, INREG32(module_base + 0xC));
	DDPDUMP("PWM0:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x10, INREG32(module_base + 0x10),
		0x14, INREG32(module_base + 0x14),
		0x18, INREG32(module_base + 0x18),
		0x1C, INREG32(module_base + 0x1C));
	DDPDUMP("PWM0:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x80, INREG32(module_base + 0x80),
		0x28, INREG32(module_base + 0x28),
		0x2C, INREG32(module_base + 0x2C),
		0x30, INREG32(module_base + 0x30));
	DDPDUMP("PWM0:0x%x=0x%x\n",
		0xC0, INREG32(module_base + 0xC0));
	DDPDUMP("-- END: DISP PWM0 registers --\n");
}

static void pwm_dump_analysis(enum DISP_MODULE_ENUM module)
{
	int index = 0;
	unsigned int reg_base = 0;

	index = 0;
	reg_base = DISPSYS_PWM0_BASE;

	DDPDUMP("== DISP PWM%d ANALYSIS ==\n", index);
#if 0 /* TODO: clk reg?? */
	DDPDUMP("pwm clock=%d\n",
		(DISP_REG_GET(DISP_REG_CLK_CFG_1_CLR) >> 7) & 0x1);
#endif

}



static void ccorr_dump_reg(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned long module_base = DISPSYS_CCORR0_BASE;

	if (module == DISP_MODULE_CCORR0)
		i = 0;
	else
		i = 1;

	DDPDUMP("== START: DISP CCORR%i registers ==\n", i);
	DDPDUMP("CCORR:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x008, INREG32(module_base + 0x008),
		0x00C, INREG32(module_base + 0x00C));
	DDPDUMP("CCORR:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x010, INREG32(module_base + 0x010),
		0x020, INREG32(module_base + 0x020),
		0x024, INREG32(module_base + 0x024),
		0x028, INREG32(module_base + 0x028));
	DDPDUMP("CCORR:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x02C, INREG32(module_base + 0x02C),
		0x030, INREG32(module_base + 0x030),
		0x080, INREG32(module_base + 0x080),
		0x084, INREG32(module_base + 0x084));
	DDPDUMP("CCORR:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x088, INREG32(module_base + 0x088),
		0x08C, INREG32(module_base + 0x08C),
		0x090, INREG32(module_base + 0x090),
		0x0A0, INREG32(module_base + 0x0A0));
	DDPDUMP("CCORR:0x%x=0x%x\n",
		0x0C0, INREG32(module_base + 0x0C0));
	DDPDUMP("-- END: DISP CCORR%d registers --\n", i);
}

static void ccorr_dump_analyze(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned int offset = 0x1000;
	int ccorr_en, ccorr_cfg, ccorr_size, ccorr_in_cnt, ccorr_out_cnt;

	if (module == DISP_MODULE_CCORR0)
		i = 0;
	else
		i = 1;

	ccorr_en = DISP_REG_GET(DISP_REG_CCORR_EN + i * offset);
	ccorr_cfg = DISP_REG_GET(DISP_REG_CCORR_CFG + i * offset);
	ccorr_size = DISP_REG_GET(DISP_REG_CCORR_SIZE + i * offset);
	ccorr_in_cnt = DISP_REG_GET(DISP_REG_CCORR_IN_CNT + i * offset);
	ccorr_out_cnt = DISP_REG_GET(DISP_REG_CCORR_OUT_CNT + i * offset);

	DDPDUMP("ccorr: en=%d, config=%d, w=%d, h=%d\n",
		ccorr_en, ccorr_cfg, (ccorr_size >> 16) & 0x1fff,
		ccorr_size & 0x1fff);
	DDPDUMP("ccorr: in_p_cnt=%d, in_l_cnt=%d, out_p_cnt=%d, out_l_cnt=%d\n",
		ccorr_in_cnt & 0x1fff,
		(ccorr_in_cnt >> 16) & 0x1fff,	ccorr_out_cnt & 0x1fff,
		(ccorr_out_cnt >> 16) & 0x1fff);
}

static void dither_dump_reg(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned long module_base = DISPSYS_DITHER0_BASE;

	if (module == DISP_MODULE_DITHER0)
		i = 0;
	else
		i = 1;

	DDPDUMP("== START: DISP DITHER%d registers ==\n", i);
	DDPDUMP("DITHER:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x00000000, INREG32(module_base + 0x00000000),
		0x00000004, INREG32(module_base + 0x00000004),
		0x00000008, INREG32(module_base + 0x00000008),
		0x0000000C, INREG32(module_base + 0x0000000C));
	DDPDUMP("DITHER:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x00000010, INREG32(module_base + 0x00000010),
		0x00000020, INREG32(module_base + 0x00000020),
		0x00000024, INREG32(module_base + 0x00000024),
		0x00000028, INREG32(module_base + 0x00000028));
	DDPDUMP("DITHER:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x0000002C, INREG32(module_base + 0x0000002C),
		0x00000030, INREG32(module_base + 0x00000030),
		0x000000C0, INREG32(module_base + 0x000000C0),
		0x00000100, INREG32(module_base + 0x00000100));
	DDPDUMP("DITHER:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x00000114, INREG32(module_base + 0x00000114),
		0x00000118, INREG32(module_base + 0x00000118),
		0x0000011C, INREG32(module_base + 0x0000011C),
		0x00000120, INREG32(module_base + 0x00000120));
	DDPDUMP("DITHER:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x00000124, INREG32(module_base + 0x00000124),
		0x00000128, INREG32(module_base + 0x00000128),
		0x0000012C, INREG32(module_base + 0x0000012C),
		0x00000130, INREG32(module_base + 0x00000130));
	DDPDUMP("DITHER:0x%x=0x%x,0x%x=0x%x,0x%x=0x%x,0x%x=0x%x\n",
		0x00000134, INREG32(module_base + 0x00000134),
		0x00000138, INREG32(module_base + 0x00000138),
		0x0000013C, INREG32(module_base + 0x0000013C),
		0x00000140, INREG32(module_base + 0x00000140));
	DDPDUMP("DITHER:0x%x=0x%x\n",
		0x00000144, INREG32(module_base + 0x00000144));
	DDPDUMP("-- END: DISP DITHER%d registers --\n", i);

}


static void dither_dump_analyze(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned int offset = 0x1000;
	int dither_size, dither_in_cnt, dither_out_cnt;

	if (module == DISP_MODULE_DITHER0)
		i = 0;
	else
		i = 1;

	dither_size = DISP_REG_GET(DISP_REG_DITHER_SIZE + i * offset);
	dither_in_cnt = DISP_REG_GET(DISP_REG_DITHER_IN_CNT + i * offset);
	dither_out_cnt = DISP_REG_GET(DISP_REG_DITHER_OUT_CNT + i * offset);

	DDPDUMP(
		"dither: en=%d, config=%d, w=%d, h=%d, in_p_cnt=%d, in_l_cnt=%d, out_p_cnt=%d, out_l_cnt=%d\n",
		 DISP_REG_GET(DISPSYS_DITHER0_BASE + 0x000 + i * offset),
		 DISP_REG_GET(DISPSYS_DITHER0_BASE + 0x020 + i * offset),
		 (dither_size >> 16) & 0x1fff, dither_size & 0x1fff,
		 dither_in_cnt & 0x1fff, (dither_in_cnt >> 16) & 0x1fff,
		 dither_out_cnt & 0x1fff, (dither_out_cnt >> 16) & 0x1fff);
}

static void dsi_dump_reg(enum DISP_MODULE_ENUM module)
{
	DSI_DumpRegisters(module, 1);
}

int ddp_dump_reg(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_WDMA0:
		wdma_dump_reg(module);
		break;
	case DISP_MODULE_RDMA0:
	case DISP_MODULE_RDMA1:
		rdma_dump_reg(module);
		break;
	case DISP_MODULE_OVL0:
	case DISP_MODULE_OVL0_2L:
	case DISP_MODULE_OVL1_2L:
		ovl_dump_reg(module);
		break;
	case DISP_MODULE_GAMMA0:
		gamma_dump_reg(module);
		break;
	case DISP_MODULE_CONFIG:
		mmsys_config_dump_reg();
		break;
	case DISP_MODULE_MUTEX:
		mutex_dump_reg();
		break;
	case DISP_MODULE_COLOR0:
		color_dump_reg(module);
		break;
	case DISP_MODULE_AAL0:
		aal_dump_reg(module);
		break;
	case DISP_MODULE_PWM0:
		pwm_dump_reg(module);
		break;
	case DISP_MODULE_DSI0:
	case DISP_MODULE_DSI1:
	case DISP_MODULE_DSIDUAL:
		dsi_dump_reg(module);
		break;
	case DISP_MODULE_CCORR0:
		ccorr_dump_reg(module);
		break;
	case DISP_MODULE_DITHER0:
		dither_dump_reg(module);
		break;
	case DISP_MODULE_RSZ0:
		rsz_dump_reg(module);
		break;
	default:
		DDPDUMP("no dump_reg for module %s(%d)\n",
			ddp_get_module_name(module), module);
	}
	return 0;
}

int ddp_dump_analysis(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_WDMA0:
		wdma_dump_analysis(module);
		break;
	case DISP_MODULE_RDMA0:
	case DISP_MODULE_RDMA1:
		rdma_dump_analysis(module);
		break;
	case DISP_MODULE_OVL0:
	case DISP_MODULE_OVL0_2L:
	case DISP_MODULE_OVL1_2L:
		ovl_dump_analysis(module);
		break;
	case DISP_MODULE_GAMMA0:
		gamma_dump_analysis(module);
		break;
	case DISP_MODULE_CONFIG:
		mmsys_config_dump_analysis();
		break;
	case DISP_MODULE_MUTEX:
		mutex_dump_analysis();
		break;
	case DISP_MODULE_COLOR0:
		color_dump_analysis(module);
		break;
	case DISP_MODULE_AAL0:
		aal_dump_analysis(module);
		break;
	case DISP_MODULE_PWM0:
		pwm_dump_analysis(module);
		break;
	case DISP_MODULE_DSI0:
	case DISP_MODULE_DSI1:
	case DISP_MODULE_DSIDUAL:
		dsi_analysis(module);
		break;
	case DISP_MODULE_CCORR0:
		ccorr_dump_analyze(module);
		break;
	case DISP_MODULE_DITHER0:
		dither_dump_analyze(module);
		break;
	case DISP_MODULE_RSZ0:
		rsz_dump_analysis(module);
		break;
	default:
		DDPDUMP("no dump_analysis for module %s(%d)\n",
			ddp_get_module_name(module), module);
	}
	return 0;
}
