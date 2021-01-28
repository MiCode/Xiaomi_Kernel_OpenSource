// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include "disp_helper.h"
#include "ddp_rsz.h"
#include "ddp_postmask.h"
#include "ddp_manager.h"

static char *ddp_signal_0(int bit)
{
	switch (bit) {
	case 0:
		return "aal0__to__gamma0";
	case 1:
		return "ccorr0__to__aal0";
	case 2:
		return "color0__to__color_out_sel_in0";
	case 3:
		return "disp_color_out_sel__to__ccorr0";
	case 4:
		return "dither0__to__dither0_mout";
	case 5:
		return "dither0_mout0__to__dsi0_sel_in0";
	case 6:
		return "dither0_mout1__to__dpi_sel_in2";
	case 7:
		return "dither0_mout2__to__wdma0_pre_sel_in1";
	case 8:
		return "gamma0__to__postmask";
	case 9:
		return "ovl0_2l_mout_out0__to__path0_sel_in1";
	case 10:
		return "ovl0_2l_mout_out1__to__ovl_to_wrot_sel_in1";
	case 11:
		return "ovl0_2l_mout_out2__to__ovl_to_wdma_sel_in1";
	case 12:
		return "ovl0_2l_mout_out3__to__ovl_to_rsz_sel_in1";
	case 13:
		return "ovl0_2l_mout_out4__to__rsz_sel_in5";
	case 14:
		return "ovl0_2l_out0__to__ovl0_2l_mout";
	case 15:
		return "ovl0_2l_out1__to__ovl0_2l_wcg_mout";
	case 16:
		return "ovl0_2l_out2__to__rsz_sel_in1";
	case 17:
		return "ovl0_2l_sel__to__ovl0_2l_in1";
	case 18:
		return "ovl0_2l_wcg_mout_out0__to__ovl0_wcg_sel_in0";
	case 19:
		return "ovl0_2l_wcg_mout_out1__to__ovl1_2l_wcg_sel_in1";
	case 20:
		return "ovl0_2l_wcg_sel__to__ovl0_2l_in0";
	case 21:
		return "ovl0_mout_out0__to__path0_sel_in0";
	case 22:
		return "ovl0_mout_out1__to__ovl_to_wrot_sel_in0";
	case 23:
		return "ovl0_mout_out2__to__ovl_to_wdma_sel_in0";
	case 24:
		return "ovl0_mout_out3__to__ovl_to_rsz_sel_in0";
	case 25:
		return "ovl0_mout_out4__to__rsz_sel_in4";
	case 26:
		return "ovl0_out0__to__ovl0_mout";
	case 27:
		return "ovl0_out1__to__ovl0_wcg_mout";
	case 28:
		return "ovl0_out2__to__rsz_sel_in0";
	case 29:
		return "ovl0_sel__to__ovl0_in1";
	case 30:
		return "ovl0_wcg_mout_out0__to__ovl0_2l_wcg_sel_in0";
	case 31:
		return "ovl0_wcg_mout_out1__to__ovl1_2l_wcg_sel_in0";
	default:
		break;
	}
	return NULL;
}

static char *ddp_signal_1(int bit)
{
	switch (bit) {
	case 0:
		return "ovl0_wcg_sel__to__ovl0_in0";
	case 1:
		return "ovl1_2l_mout_out0__to__path0_sel_in2";
	case 2:
		return "ovl1_2l_mout_out1__to__ovl_to_wrot_sel_in2";
	case 3:
		return "ovl1_2l_mout_out2__to__ovl_to_wdma_sel_in2";
	case 4:
		return "ovl1_2l_mout_out3__to__ovl_to_rsz_sel_in2";
	case 5:
		return "ovl1_2l_mout_out4__to__rdma1";
	case 6:
		return "ovl1_2l_mout_out5__to__rsz_sel_in6";
	case 7:
		return "ovl1_2l_out0__to__ovl1_2l_mout";
	case 8:
		return "ovl1_2l_out1__to__ovl1_2l_wcg_mout";
	case 9:
		return "ovl1_2l_out2__to__rsz_sel_in2";
	case 10:
		return "ovl1_2l_wcg_mout_out0__to__ovl0_2l_wcg_sel_in0";
	case 11:
		return "ovl1_2l_wcg_mout_out1__to__ovl0_wcg_sel_in1";
	case 12:
		return "ovl1_2l_wcg_sel__to__ovl1_2l_in0";
	case 13:
		return "path0_sel__to__rdma0";
	case 14:
		return "postmask0__to__dither0";
	case 15:
		return "rdma0__to__rdma0_rsz_in_sout";
	case 16:
		return "rdma0_rsz_in_sout_out0__to__rdma0_rsz_out_sel_in0";
	case 17:
		return "rdma0_rsz_in_sout_out1__to__rsz_sel_in3";
	case 18:
		return "rdma0_esz_out_sel__to__rdma0_sout";
	case 19:
		return "rdma0_sout_out0__to__dsi0_sel_in1";
	case 20:
		return "rdma0_sout_out1__to__color0";
	case 21:
		return "rdma0_sout_out2__to__color_out_sel_in1";
	case 22:
		return "rdma0_sout_out3__to__dpi0_sel_in0";
	case 23:
		return "rdma1__to__rdma1_sout";
	case 24:
		return "rdma1_sout_out0__to__dpi0_sel_in1";
	case 25:
		return "rdma1_sout_out1__to__dsi0_sel_in2";
	case 26:
		return "rsz0__to__rsz_mout";
	case 27:
		return "rsz_mout_out0__to__ovl0_in2";
	case 28:
		return "rsz_mout_out1__to__ovl0_2l_in2";
	case 29:
		return "rsz_mout_out2__to__ovl1_2l_in2";
	case 30:
		return "ovl_to_wdma_sel__to__wdma0_pre_sel_in0";
	case 31:
		return "rsz_mout_out4__to__ovl_to_wdma_sel_in3";
	default:
		break;
	}
	return NULL;
}

static char *ddp_signal_2(int bit)
{
	switch (bit) {
	case 0:
		return "rsz_mout_out5__to__rdma0_rsz_out_sel_in3";
	case 1:
		return "rsz_mout_out6__to__ovl_to_wrot_sel_in3";
	case 2:
		return "rsz_sel__to__rsz0";
	case 3:
		return "to_wrot_sout_out0__to__mdp_wrot0_sel_in1";
	case 4:
		return "to_wrot_sout_out1__to__mdp_wrot1_sel_in1";
	case 5:
		return "wdma0_pre_sel__to__wdma0_sel_in3";
	case 6:
		return "wdma0_sel__to__wdma0";
	case 7:
		return "dpi0_sel__to__dpi0_thp_lmt";
	case 8:
		return "dpi0_thp_lmt__to__dpi0";
	case 9:
		return "dsi0_sel__to__dsi0_thp_lmt";
	case 10:
		return "dsi0_thp_lmt__to__dsi0";
	default:
		break;
	}
	return NULL;
}

static char *ddp_greq_name(int bit)
{
	switch (bit) {
	case 0:
		return "OVL0";
	case 1:
		return "OVL0_2L_LARB0";
	case 2:
		return "RDMA0";
	case 3:
		return "WDMA0";
	case 4:
		return "MDP_RDMA0";
	case 5:
		return "MDP_WROT0";
	case 6:
		return "DISP_FAKE0";
	case 16:
		return "OVL1";
	case 17:
		return "RDMA1";
	case 18:
		return "OVL0_2L_LARB1";
	case 19:
		return "MDP_RDMA1";
	case 20:
		return "MDP_WROT1";
	case 21:
		return "DISP_FAKE1";
	default:
		break;
	}
	return NULL;
}

static char *ddp_get_mutex_module0_name(unsigned int bit)
{
	switch (bit) {
	case 0:
		return "rdma0";
	case 1:
		return "rdma1";
	case 2:
		return "mdp_rdma0";
	case 4:
		return "mdp_rsz0";
	case 5:
		return "mdp_rsz1";
	case 6:
		return "mdp_tdshp";
	case 7:
		return "mdp_wrot0";
	case 8:
		return "mdp_wrot1";
	case 9:
		return "ovl0";
	case 10:
		return "ovl0_2L";
	case 11:
		return "ovl1_2L";
	case 12:
		return "wdma0";
	case 13:
		return "color0";
	case 14:
		return "ccorr0";
	case 15:
		return "aal0";
	case 16:
		return "gamma0";
	case 17:
		return "dither0";
	case 18:
		return "PWM0";
	case 19:
		return "DSI";
	case 20:
		return "DPI";
	case 21:
		return "postmask";
	case 22:
		return "rsz";
	default:
		break;
	}
	return "unknown-mutex";
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
			DDPDUMP("%s, unknown fmt=%d, module=%d\n", __func__,
				fmt, module);
			return "unknown-fmt";
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
			DDPDUMP("%s, unknown fmt=%d, module=%d\n", __func__,
				fmt, module);
			return "unknown-fmt";
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
			DDPDUMP("%s, unknown fmt=%d, module=%d\n", __func__,
				fmt, module);
			return "unknown-fmt";
		}
	} else {
		DDPDUMP("%s, unknown module=%d\n", __func__, module);
	}

	return "unknown";
}

static char *ddp_clock_0(int bit)
{
	switch (bit) {
	case 0:
		return "smi_common(cg), ";
	case 1:
		return "smi_larb0(cg), ";
	case 2:
		return "smi_larb1(cg), ";
	case 3:
		return "gals_common0(cg), ";
	case 4:
		return "gals_common1(cg), ";
	case 20:
		return "ovl0, ";
	case 21:
		return "ovl0_2L, ";
	case 22:
		return "ovl1_2L, ";
	case 23:
		return "rdma0, ";
	case 24:
		return "rdma1, ";
	case 25:
		return "wdma0, ";
	case 26:
		return "color, ";
	case 27:
		return "ccorr, ";
	case 28:
		return "aal, ";
	case 29:
		return "gamma, ";
	case 30:
		return "dither, ";
	case 31:
		return "split, ";
	default:
		break;
	}
	return NULL;
}

static char *ddp_clock_1(int bit)
{
	switch (bit) {
	case 0:
		return "dsi0_mm(cg), ";
	case 1:
		return "dsi0_interface(cg), ";
	case 2:
		return "dpi_mm(cg), ";
	case 3:
		return "dpi_interface, ";
	case 7:
		return "26M, ";
	default:
		break;
	}
	return NULL;
}

void dump_reg_row(unsigned long baddr, unsigned long offset, unsigned int count)
{
	const int buf_len = 7 + count * 11;
	char buf[buf_len];
	int len = 0;
	int i = 0;
	unsigned int val = 0;

	if (count > 4)
		return;

	len = snprintf(buf, buf_len, "0x%03lx:", offset);
	for (i = 0; i < count; i++) {
		val = INREG32(baddr + offset + 0x4 * i);
		if (val)
			len += snprintf(buf + len, buf_len - len,
					"0x%08x", val);
		else
			len += snprintf(buf + len, buf_len - len, "%10x", val);

		if (i < count - 1)
			len += snprintf(buf + len, buf_len - len, " ");
		else if (i == count - 1)
			len += snprintf(buf + len, buf_len - len, "\n");
	}

	DDPDUMP("%s", buf);
}

static void mutex_dump_reg(void)
{
	unsigned long module_base = DISPSYS_MUTEX_BASE;

	DDPDUMP("== DISP MUTEX REGS ==\n");
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x0, INREG32(module_base + 0x0),
		0x4, INREG32(module_base + 0x4),
		0x8, INREG32(module_base + 0x8),
		0xC, INREG32(module_base + 0xC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x10, INREG32(module_base + 0x10),
		0x18, INREG32(module_base + 0x18),
		0x1C, INREG32(module_base + 0x1C),
		0x020, INREG32(module_base + 0x020));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x024, INREG32(module_base + 0x024),
		0x028, INREG32(module_base + 0x028),
		0x02C, INREG32(module_base + 0x02C),
		0x030, INREG32(module_base + 0x030));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x040, INREG32(module_base + 0x040),
		0x044, INREG32(module_base + 0x044),
		0x048, INREG32(module_base + 0x048),
		0x04C, INREG32(module_base + 0x04C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x050, INREG32(module_base + 0x050),
		0x060, INREG32(module_base + 0x060),
		0x064, INREG32(module_base + 0x064),
		0x068, INREG32(module_base + 0x068));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x06C, INREG32(module_base + 0x06C),
		0x070, INREG32(module_base + 0x070),
		0x080, INREG32(module_base + 0x080),
		0x084, INREG32(module_base + 0x084));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x088, INREG32(module_base + 0x088),
		0x08C, INREG32(module_base + 0x08C),
		0x090, INREG32(module_base + 0x090),
		0x0A0, INREG32(module_base + 0x0A0));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x0A4, INREG32(module_base + 0x0A4),
		0x0A8, INREG32(module_base + 0x0A8),
		0x0AC, INREG32(module_base + 0x0AC),
		0x0B0, INREG32(module_base + 0x0B0));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x0C0, INREG32(module_base + 0x0C0),
		0x0C4, INREG32(module_base + 0x0C4),
		0x0C8, INREG32(module_base + 0x0C8),
		0x0CC, INREG32(module_base + 0x0CC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x0D0, INREG32(module_base + 0x0D0),
		0x0E0, INREG32(module_base + 0x0E0),
		0x0E4, INREG32(module_base + 0x0E4),
		0x0E8, INREG32(module_base + 0x0E8));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x0EC, INREG32(module_base + 0x0EC),
		0x0F0, INREG32(module_base + 0x0F0),
		0x100, INREG32(module_base + 0x100),
		0x104, INREG32(module_base + 0x104));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x108, INREG32(module_base + 0x108),
		0x10C, INREG32(module_base + 0x10C),
		0x110, INREG32(module_base + 0x110),
		0x120, INREG32(module_base + 0x120));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x124, INREG32(module_base + 0x124),
		0x128, INREG32(module_base + 0x128),
		0x12C, INREG32(module_base + 0x12C),
		0x130, INREG32(module_base + 0x130));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x140, INREG32(module_base + 0x140),
		0x144, INREG32(module_base + 0x144),
		0x148, INREG32(module_base + 0x148),
		0x14C, INREG32(module_base + 0x14C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x150, INREG32(module_base + 0x150),
		0x160, INREG32(module_base + 0x160),
		0x164, INREG32(module_base + 0x164),
		0x168, INREG32(module_base + 0x168));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x16C, INREG32(module_base + 0x16C),
		0x170, INREG32(module_base + 0x170),
		0x180, INREG32(module_base + 0x180),
		0x184, INREG32(module_base + 0x184));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x188, INREG32(module_base + 0x188),
		0x18C, INREG32(module_base + 0x18C),
		0x190, INREG32(module_base + 0x190),
		0x300, INREG32(module_base + 0x300));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x304, INREG32(module_base + 0x304),
		0x30C, INREG32(module_base + 0x30C));
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
		len = sprintf(p, "MUTEX%d:SOF=%s,EOF=%s,WAIT=%d,module=(",
		      i, ddp_get_mutex_sof_name(
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

	DDPDUMP("== DISP MMSYS_CONFIG REGS ==\n");
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x00C, INREG32(module_base + 0x00C),
		0x010, INREG32(module_base + 0x010));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x014, INREG32(module_base + 0x014),
		0x018, INREG32(module_base + 0x018),
		0x020, INREG32(module_base + 0x020),
		0x024, INREG32(module_base + 0x024));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x028, INREG32(module_base + 0x028),
		0x02C, INREG32(module_base + 0x02C),
		0x030, INREG32(module_base + 0x030),
		0x034, INREG32(module_base + 0x034));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x038, INREG32(module_base + 0x038),
		0x048, INREG32(module_base + 0x048),
		0x0F0, INREG32(module_base + 0x0F0),
		0x0F4, INREG32(module_base + 0x0F4));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x0F8, INREG32(module_base + 0x0F8),
		0x100, INREG32(module_base + 0x100),
		0x104, INREG32(module_base + 0x104),
		0x108, INREG32(module_base + 0x108));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x110, INREG32(module_base + 0x110),
		0x114, INREG32(module_base + 0x114),
		0x118, INREG32(module_base + 0x118),
		0x120, INREG32(module_base + 0x120));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x124, INREG32(module_base + 0x124),
		0x128, INREG32(module_base + 0x128),
		0x130, INREG32(module_base + 0x130),
		0x134, INREG32(module_base + 0x134));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x138, INREG32(module_base + 0x138),
		0x140, INREG32(module_base + 0x140),
		0x144, INREG32(module_base + 0x144),
		0x150, INREG32(module_base + 0x150));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x180, INREG32(module_base + 0x180),
		0x184, INREG32(module_base + 0x184),
		0x190, INREG32(module_base + 0x190),
		0x200, INREG32(module_base + 0x200));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x204, INREG32(module_base + 0x204),
		0x208, INREG32(module_base + 0x208),
		0x20C, INREG32(module_base + 0x20C),
		0x210, INREG32(module_base + 0x210));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x214, INREG32(module_base + 0x214),
		0x218, INREG32(module_base + 0x218),
		0x220, INREG32(module_base + 0x220),
		0x224, INREG32(module_base + 0x224));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x228, INREG32(module_base + 0x228),
		0x22C, INREG32(module_base + 0x22C),
		0x230, INREG32(module_base + 0x230),
		0x234, INREG32(module_base + 0x234));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x238, INREG32(module_base + 0x238),
		0x800, INREG32(module_base + 0x800),
		0x804, INREG32(module_base + 0x804),
		0x808, INREG32(module_base + 0x808));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x80C, INREG32(module_base + 0x80C),
		0x810, INREG32(module_base + 0x810),
		0x814, INREG32(module_base + 0x814),
		0x818, INREG32(module_base + 0x818));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x81C, INREG32(module_base + 0x81C),
		0x820, INREG32(module_base + 0x820),
		0x824, INREG32(module_base + 0x824),
		0x828, INREG32(module_base + 0x828));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x82C, INREG32(module_base + 0x82C),
		0x830, INREG32(module_base + 0x830),
		0x834, INREG32(module_base + 0x834),
		0x838, INREG32(module_base + 0x838));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x83C, INREG32(module_base + 0x83C),
		0x840, INREG32(module_base + 0x840),
		0x844, INREG32(module_base + 0x844),
		0x848, INREG32(module_base + 0x848));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x84C, INREG32(module_base + 0x84C),
		0x854, INREG32(module_base + 0x854),
		0x858, INREG32(module_base + 0x858),
		0x85C, INREG32(module_base + 0x85C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x860, INREG32(module_base + 0x860),
		0x864, INREG32(module_base + 0x864),
		0x868, INREG32(module_base + 0x868),
		0x870, INREG32(module_base + 0x870));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x874, INREG32(module_base + 0x874),
		0x878, INREG32(module_base + 0x878),
		0x88C, INREG32(module_base + 0x88C),
		0x890, INREG32(module_base + 0x890));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x894, INREG32(module_base + 0x894),
		0x898, INREG32(module_base + 0x898),
		0x89C, INREG32(module_base + 0x89C),
		0x8A0, INREG32(module_base + 0x8A0));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x8A4, INREG32(module_base + 0x8A4),
		0x8A8, INREG32(module_base + 0x8A8),
		0x8AC, INREG32(module_base + 0x8AC),
		0x8B0, INREG32(module_base + 0x8B0));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x8B4, INREG32(module_base + 0x8B4),
		0x8B8, INREG32(module_base + 0x8B8),
		0x8C0, INREG32(module_base + 0x8C0),
		0x8C4, INREG32(module_base + 0x8C4));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x8CC, INREG32(module_base + 0x8CC),
		0x8D0, INREG32(module_base + 0x8D0),
		0x8D4, INREG32(module_base + 0x8D4),
		0x8D8, INREG32(module_base + 0x8D8));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x8DC, INREG32(module_base + 0x8DC),
		0x8E0, INREG32(module_base + 0x8E0),
		0x8E4, INREG32(module_base + 0x8E4),
		0x8E8, INREG32(module_base + 0x8E8));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x8EC, INREG32(module_base + 0x8EC),
		0x8F0, INREG32(module_base + 0x8F0),
		0x908, INREG32(module_base + 0x908),
		0x90C, INREG32(module_base + 0x90C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x910, INREG32(module_base + 0x910),
		0x914, INREG32(module_base + 0x914),
		0x918, INREG32(module_base + 0x918),
		0x91C, INREG32(module_base + 0x91C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x920, INREG32(module_base + 0x920),
		0x924, INREG32(module_base + 0x924),
		0x928, INREG32(module_base + 0x928),
		0x934, INREG32(module_base + 0x934));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x938, INREG32(module_base + 0x938),
		0x93C, INREG32(module_base + 0x93C),
		0x940, INREG32(module_base + 0x940),
		0x944, INREG32(module_base + 0x944));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xF00, INREG32(module_base + 0xF00),
		0xF04, INREG32(module_base + 0xF04),
		0xF08, INREG32(module_base + 0xF08),
		0xF0C, INREG32(module_base + 0xF0C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xF10, INREG32(module_base + 0xF10),
		0xF20, INREG32(module_base + 0xF20),
		0xF24, INREG32(module_base + 0xF24),
		0xF28, INREG32(module_base + 0xF28));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xF2C, INREG32(module_base + 0xF2C),
		0xF30, INREG32(module_base + 0xF30),
		0xF34, INREG32(module_base + 0xF34),
		0xF38, INREG32(module_base + 0xF38));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xF3C, INREG32(module_base + 0xF3C),
		0xF40, INREG32(module_base + 0xF40),
		0xF44, INREG32(module_base + 0xF44),
		0xF48, INREG32(module_base + 0xF48));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xF50, INREG32(module_base + 0xF50),
		0xF54, INREG32(module_base + 0xF54),
		0xF58, INREG32(module_base + 0xF58),
		0xF5C, INREG32(module_base + 0xF5C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xF60, INREG32(module_base + 0xF60),
		0xF64, INREG32(module_base + 0xF64),
		0xF68, INREG32(module_base + 0xF68));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xF80, INREG32(module_base + 0xF80),
		0xF84, INREG32(module_base + 0xF84),
		0xF88, INREG32(module_base + 0xF88),
		0xF8C, INREG32(module_base + 0xF8C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xF90, INREG32(module_base + 0xF90),
		0xF94, INREG32(module_base + 0xF94),
		0xF98, INREG32(module_base + 0xF98),
		0xFA0, INREG32(module_base + 0xFA0));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xFA4, INREG32(module_base + 0xFA4),
		0xFA8, INREG32(module_base + 0xFA8),
		0xFAC, INREG32(module_base + 0xFAC),
		0xFB0, INREG32(module_base + 0xFB0));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xFC0, INREG32(module_base + 0xFC0),
		0xFC4, INREG32(module_base + 0XFC4),
		0xFC8, INREG32(module_base + 0xFC8),
		0xFCC, INREG32(module_base + 0xFCC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xFD0, INREG32(module_base + 0xFD0),
		0xFD4, INREG32(module_base + 0xFD4),
		0xFD8, INREG32(module_base + 0xFD8),
		0xFDC, INREG32(module_base + 0xFDC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xFE0, INREG32(module_base + 0xFE0),
		0xFE4, INREG32(module_base + 0xFE4));
}

/**
 * ------ clock:
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

	unsigned int valid0 = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_VALID_0);
	unsigned int valid1 = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_VALID_1);
	unsigned int valid2 = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_VALID_2);
	unsigned int ready0 = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_READY_0);
	unsigned int ready1 = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_READY_1);
	unsigned int ready2 = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_READY_2);
	unsigned int greq = DISP_REG_GET(DISP_REG_CONFIG_SMI_LARB_GREQ);

	const int len = 200;
	char msg[len];
	int n = 0;

	DDPDUMP("== DISP MMSYS_CONFIG ANALYSIS ==\n");

	reg = DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0);
	for (i = 0; i < 32; i++) {
		if ((reg & (1 << i)) == 0) {
			name = ddp_clock_0(i);
			if (name)
				strncat(clock_on, name, (sizeof(clock_on) -
							 strlen(clock_on) - 1));
		}
	}

	reg = DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1);
	for (i = 0; i < 32; i++) {
		if ((reg & (1 << i)) == 0) {
			name = ddp_clock_1(i);
			if (name)
				strncat(clock_on, name, (sizeof(clock_on) -
							 strlen(clock_on) - 1));
		}
	}
	DDPDUMP("clock on modules:%s\n", clock_on);

	DDPDUMP("valid0=0x%x,valid1=0x%x,valid2=0x%x\n",
		valid0, valid1, valid2);
	DDPDUMP("ready0=0x%x,ready1=0x%x,ready2=0x%x,greq=0%x\n",
		ready0, ready1, ready2, greq);
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

	for (i = 0; i < 11; i++) {
		name = ddp_signal_2(i);
		if (!name)
			continue;

		pos = clock_on;

		if ((valid2 & (1 << i)))
			pos += sprintf(pos, "%s,", "v");
		else
			pos += sprintf(pos, "%s,", "n");

		if ((ready2 & (1 << i)))
			pos += sprintf(pos, "%s", "r");
		else
			pos += sprintf(pos, "%s", "n");

		pos += sprintf(pos, ": %s", name);

		DDPDUMP("%s\n", clock_on);
	}

	/* greq: 1 means SMI dose not grant, maybe SMI hang */
	if (greq) {
		n = snprintf(msg, len, "smi greq not grant module: (greq: ");
		n += snprintf(msg + n, len - n,
			      "1 means SMI dose not grant, maybe SMI hang)");
		DDPDUMP("%s", msg);
	}

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
}

static void gamma_dump_reg(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned int offset = 0x1000;

	if (module == DISP_MODULE_GAMMA0)
		i = 0;
	else
		i = 1;

	DDPDUMP("== DISP %s REGS ==\n", ddp_get_module_name(module));
	if (!disp_helper_get_option(DISP_OPT_PQ_REG_DUMP))
		return;

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

	DDPDUMP("== DISP %s ANALYSIS ==\n", ddp_get_module_name(module));
	DDPDUMP("en=%d,wh(%dx%d),pos:in(%d,%d)out(%d,%d)\n",
		DISP_REG_GET(DISP_REG_GAMMA_EN + i * offset),
		(DISP_REG_GET(DISP_REG_GAMMA_SIZE + i * offset) >> 16) & 0x1fff,
		DISP_REG_GET(DISP_REG_GAMMA_SIZE + i * offset) & 0x1fff,
		DISP_REG_GET(DISP_REG_GAMMA_INPUT_COUNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_REG_GAMMA_INPUT_COUNT + i * offset) >> 16) &
			0x1fff,
		DISP_REG_GET(DISP_REG_GAMMA_OUTPUT_COUNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_REG_GAMMA_OUTPUT_COUNT + i * offset) >>
			16) & 0x1fff);
}

static void color_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = DISPSYS_COLOR0_BASE;

	DDPDUMP("== DISP %s REGS ==\n", ddp_get_module_name(module));
	if (!disp_helper_get_option(DISP_OPT_PQ_REG_DUMP))
		return;

	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x400, INREG32(module_base + 0x400),
		0x404, INREG32(module_base + 0x404),
		0x408, INREG32(module_base + 0x408),
		0x40C, INREG32(module_base + 0x40C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x410, INREG32(module_base + 0x410),
		0x418, INREG32(module_base + 0x418),
		0x41C, INREG32(module_base + 0x41C),
		0x420, INREG32(module_base + 0x420));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x428, INREG32(module_base + 0x428),
		0x42C, INREG32(module_base + 0x42C),
		0x430, INREG32(module_base + 0x430),
		0x434, INREG32(module_base + 0x434));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x438, INREG32(module_base + 0x438),
		0x484, INREG32(module_base + 0x484),
		0x488, INREG32(module_base + 0x488),
		0x48C, INREG32(module_base + 0x48C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x490, INREG32(module_base + 0x490),
		0x494, INREG32(module_base + 0x494),
		0x498, INREG32(module_base + 0x498),
		0x49C, INREG32(module_base + 0x49C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x4A0, INREG32(module_base + 0x4A0),
		0x4A4, INREG32(module_base + 0x4A4),
		0x4A8, INREG32(module_base + 0x4A8),
		0x4AC, INREG32(module_base + 0x4AC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x4B0, INREG32(module_base + 0x4B0),
		0x4B4, INREG32(module_base + 0x4B4),
		0x4B8, INREG32(module_base + 0x4B8),
		0x4BC, INREG32(module_base + 0x4BC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x620, INREG32(module_base + 0x620),
		0x624, INREG32(module_base + 0x624),
		0x628, INREG32(module_base + 0x628),
		0x62C, INREG32(module_base + 0x62C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x630, INREG32(module_base + 0x630),
		0x740, INREG32(module_base + 0x740),
		0x74C, INREG32(module_base + 0x74C),
		0x768, INREG32(module_base + 0x768));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x76C, INREG32(module_base + 0x76C),
		0x79C, INREG32(module_base + 0x79C),
		0x7E0, INREG32(module_base + 0x7E0),
		0x7E4, INREG32(module_base + 0x7E4));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x7E8, INREG32(module_base + 0x7E8),
		0x7EC, INREG32(module_base + 0x7EC),
		0x7F0, INREG32(module_base + 0x7F0),
		0x7FC, INREG32(module_base + 0x7FC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x800, INREG32(module_base + 0x800),
		0x804, INREG32(module_base + 0x804),
		0x808, INREG32(module_base + 0x808),
		0x80C, INREG32(module_base + 0x80C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x810, INREG32(module_base + 0x810),
		0x814, INREG32(module_base + 0x814),
		0x818, INREG32(module_base + 0x818),
		0x81C, INREG32(module_base + 0x81C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x820, INREG32(module_base + 0x820),
		0x824, INREG32(module_base + 0x824),
		0x828, INREG32(module_base + 0x828),
		0x82C, INREG32(module_base + 0x82C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x830, INREG32(module_base + 0x830),
		0x834, INREG32(module_base + 0x834),
		0x838, INREG32(module_base + 0x838),
		0x83C, INREG32(module_base + 0x83C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x840, INREG32(module_base + 0x840),
		0x844, INREG32(module_base + 0x844),
		0x848, INREG32(module_base + 0x848),
		0x84C, INREG32(module_base + 0x84C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x850, INREG32(module_base + 0x850),
		0x854, INREG32(module_base + 0x854),
		0x858, INREG32(module_base + 0x858),
		0x85C, INREG32(module_base + 0x85C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x860, INREG32(module_base + 0x860),
		0x864, INREG32(module_base + 0x864),
		0x868, INREG32(module_base + 0x868),
		0x86C, INREG32(module_base + 0x86C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x870, INREG32(module_base + 0x870),
		0x874, INREG32(module_base + 0x874),
		0x878, INREG32(module_base + 0x878),
		0x87C, INREG32(module_base + 0x87C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x880, INREG32(module_base + 0x880),
		0x884, INREG32(module_base + 0x884),
		0x888, INREG32(module_base + 0x888),
		0x88C, INREG32(module_base + 0x88C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x890, INREG32(module_base + 0x890),
		0x894, INREG32(module_base + 0x894),
		0x898, INREG32(module_base + 0x898),
		0x89C, INREG32(module_base + 0x89C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x8A0, INREG32(module_base + 0x8A0),
		0x8A4, INREG32(module_base + 0x8A4),
		0x8A8, INREG32(module_base + 0x8A8),
		0x8AC, INREG32(module_base + 0x8AC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x8B0, INREG32(module_base + 0x8B0),
		0x8B4, INREG32(module_base + 0x8B4),
		0x8B8, INREG32(module_base + 0x8B8),
		0x8BC, INREG32(module_base + 0x8BC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x8C0, INREG32(module_base + 0x8C0),
		0x8C4, INREG32(module_base + 0x8C4),
		0x8C8, INREG32(module_base + 0x8C8),
		0x8CC, INREG32(module_base + 0x8CC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x8D0, INREG32(module_base + 0x8D0),
		0x8D4, INREG32(module_base + 0x8D4),
		0x8D8, INREG32(module_base + 0x8D8),
		0x8DC, INREG32(module_base + 0x8DC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x8E0, INREG32(module_base + 0x8E0),
		0x8E4, INREG32(module_base + 0x8E4),
		0x8E8, INREG32(module_base + 0x8E8),
		0x8EC, INREG32(module_base + 0x8EC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x8F0, INREG32(module_base + 0x8F0),
		0x8F4, INREG32(module_base + 0x8F4),
		0x8F8, INREG32(module_base + 0x8F8),
		0x8FC, INREG32(module_base + 0x8FC));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x900, INREG32(module_base + 0x900),
		0x904, INREG32(module_base + 0x904),
		0x908, INREG32(module_base + 0x908),
		0x90C, INREG32(module_base + 0x90C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x910, INREG32(module_base + 0x910),
		0x914, INREG32(module_base + 0x914),
		0xC00, INREG32(module_base + 0xC00),
		0xC04, INREG32(module_base + 0xC04));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xC08, INREG32(module_base + 0xC08),
		0xC0C, INREG32(module_base + 0xC0C),
		0xC10, INREG32(module_base + 0xC10),
		0xC14, INREG32(module_base + 0xC14));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xC18, INREG32(module_base + 0xC18),
		0xC28, INREG32(module_base + 0xC28),
		0xC50, INREG32(module_base + 0xC50),
		0xC54, INREG32(module_base + 0xC54));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xC60, INREG32(module_base + 0xC60),
		0xCA0, INREG32(module_base + 0xCA0),
		0xCB0, INREG32(module_base + 0xCB0),
		0xCF0, INREG32(module_base + 0xCF0));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xCF4, INREG32(module_base + 0xCF4),
		0xCF8, INREG32(module_base + 0xCF8),
		0xCFC, INREG32(module_base + 0xCFC),
		0xD00, INREG32(module_base + 0xD00));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xD04, INREG32(module_base + 0xD04),
		0xD08, INREG32(module_base + 0xD08),
		0xD0C, INREG32(module_base + 0xD0C),
		0xD10, INREG32(module_base + 0xD10));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xD14, INREG32(module_base + 0xD14),
		0xD18, INREG32(module_base + 0xD18),
		0xD1C, INREG32(module_base + 0xD1C),
		0xD20, INREG32(module_base + 0xD20));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xD24, INREG32(module_base + 0xD24),
		0xD28, INREG32(module_base + 0xD28),
		0xD2C, INREG32(module_base + 0xD2C),
		0xD30, INREG32(module_base + 0xD30));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xD34, INREG32(module_base + 0xD34),
		0xD38, INREG32(module_base + 0xD38),
		0xD3C, INREG32(module_base + 0xD3C),
		0xD40, INREG32(module_base + 0xD40));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xD44, INREG32(module_base + 0xD44),
		0xD48, INREG32(module_base + 0xD48),
		0xD4C, INREG32(module_base + 0xD4C),
		0xD50, INREG32(module_base + 0xD50));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0xD54, INREG32(module_base + 0xD54),
		0xD58, INREG32(module_base + 0xD58),
		0xD5C, INREG32(module_base + 0xD5C));
}

static void color_dump_analysis(enum DISP_MODULE_ENUM module)
{
	DDPDUMP("== DISP %s ANALYSIS ==\n", ddp_get_module_name(module));
	DDPDUMP("bypass=%d,wh(%dx%d),pos:in(%d,%d)\n",
		(DISP_REG_GET(DISP_COLOR_CFG_MAIN) >> 7) & 0x1,
		DISP_REG_GET(DISP_COLOR_INTERNAL_IP_WIDTH),
		DISP_REG_GET(DISP_COLOR_INTERNAL_IP_HEIGHT),
		DISP_REG_GET(DISP_COLOR_PXL_CNT_MAIN) & 0xffff,
		(DISP_REG_GET(DISP_COLOR_LINE_CNT_MAIN) >> 16) & 0x1fff);
}

static void aal_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = DISPSYS_AAL0_BASE;

	DDPDUMP("== DISP %s REGS ==\n", ddp_get_module_name(module));
	if (!disp_helper_get_option(DISP_OPT_PQ_REG_DUMP))
		return;

	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x008, INREG32(module_base + 0x008),
		0x00C, INREG32(module_base + 0x00C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x010, INREG32(module_base + 0x010),
		0x020, INREG32(module_base + 0x020),
		0x024, INREG32(module_base + 0x024),
		0x028, INREG32(module_base + 0x028));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x02C, INREG32(module_base + 0x02C),
		0x030, INREG32(module_base + 0x030),
		0x0B0, INREG32(module_base + 0x0B0),
		0x0C0, INREG32(module_base + 0x0C0));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x0FC, INREG32(module_base + 0x0FC),
		0x204, INREG32(module_base + 0x204),
		0x20C, INREG32(module_base + 0x20C),
		0x214, INREG32(module_base + 0x214));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x21C, INREG32(module_base + 0x21C),
		0x224, INREG32(module_base + 0x224),
		0x228, INREG32(module_base + 0x228),
		0x22C, INREG32(module_base + 0x22C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x230, INREG32(module_base + 0x230),
		0x234, INREG32(module_base + 0x234),
		0x238, INREG32(module_base + 0x238),
		0x23C, INREG32(module_base + 0x23C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x240, INREG32(module_base + 0x240),
		0x244, INREG32(module_base + 0x244),
		0x248, INREG32(module_base + 0x248),
		0x24C, INREG32(module_base + 0x24C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x250, INREG32(module_base + 0x250),
		0x254, INREG32(module_base + 0x254),
		0x258, INREG32(module_base + 0x258),
		0x25C, INREG32(module_base + 0x25C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x260, INREG32(module_base + 0x260),
		0x264, INREG32(module_base + 0x264),
		0x268, INREG32(module_base + 0x268),
		0x26C, INREG32(module_base + 0x26C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x270, INREG32(module_base + 0x270),
		0x274, INREG32(module_base + 0x274),
		0x278, INREG32(module_base + 0x278),
		0x27C, INREG32(module_base + 0x27C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x280, INREG32(module_base + 0x280),
		0x284, INREG32(module_base + 0x284),
		0x288, INREG32(module_base + 0x288),
		0x28C, INREG32(module_base + 0x28C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x290, INREG32(module_base + 0x290),
		0x294, INREG32(module_base + 0x294),
		0x298, INREG32(module_base + 0x298),
		0x29C, INREG32(module_base + 0x29C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x2A0, INREG32(module_base + 0x2A0),
		0x2A4, INREG32(module_base + 0x2A4),
		0x358, INREG32(module_base + 0x358),
		0x35C, INREG32(module_base + 0x35C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x360, INREG32(module_base + 0x360),
		0x364, INREG32(module_base + 0x364),
		0x368, INREG32(module_base + 0x368),
		0x36C, INREG32(module_base + 0x36C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x370, INREG32(module_base + 0x370),
		0x374, INREG32(module_base + 0x374),
		0x378, INREG32(module_base + 0x378),
		0x37C, INREG32(module_base + 0x37C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x380, INREG32(module_base + 0x380),
		0x3B0, INREG32(module_base + 0x3B0),
		0x40C, INREG32(module_base + 0x40C),
		0x410, INREG32(module_base + 0x410));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x414, INREG32(module_base + 0x414),
		0x418, INREG32(module_base + 0x418),
		0x41C, INREG32(module_base + 0x41C),
		0x420, INREG32(module_base + 0x420));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x424, INREG32(module_base + 0x424),
		0x428, INREG32(module_base + 0x428),
		0x42C, INREG32(module_base + 0x42C),
		0x430, INREG32(module_base + 0x430));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x434, INREG32(module_base + 0x434),
		0x440, INREG32(module_base + 0x440),
		0x444, INREG32(module_base + 0x444),
		0x448, INREG32(module_base + 0x448));
}

static void aal_dump_analysis(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned int offset = 0x1000;

	if (module == DISP_MODULE_AAL0)
		i = 0;
	else
		i = 1;

	DDPDUMP("== DISP %s ANALYSIS ==\n", ddp_get_module_name(module));
	DDPDUMP("bypass=%d,relay=%d,en=%d,wh(%dx%d),pos:in(%d,%d)out(%d,%d)\n",
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

	DDPDUMP("== DISP %s REGS ==\n", ddp_get_module_name(module));
	if (!disp_helper_get_option(DISP_OPT_PQ_REG_DUMP))
		return;

	DDPDUMP("0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x\n",
		0x0, INREG32(module_base + 0x0),
		0x4, INREG32(module_base + 0x4),
		0x8, INREG32(module_base + 0x8),
		0xC, INREG32(module_base + 0xC));
	DDPDUMP("0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x\n",
		0x10, INREG32(module_base + 0x10),
		0x14, INREG32(module_base + 0x14),
		0x18, INREG32(module_base + 0x18),
		0x1C, INREG32(module_base + 0x1C));
	DDPDUMP("0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x\n",
		0x80, INREG32(module_base + 0x80),
		0x28, INREG32(module_base + 0x28),
		0x2C, INREG32(module_base + 0x2C),
		0x30, INREG32(module_base + 0x30));
	DDPDUMP("0x%04x=0x%08x\n",
		0xC0, INREG32(module_base + 0xC0));
}

static void pwm_dump_analysis(enum DISP_MODULE_ENUM module)
{
	unsigned int reg_base = 0;

	reg_base = DISPSYS_PWM0_BASE;

	DDPDUMP("== DISP %s ANALYSIS ==\n", ddp_get_module_name(module));
}

static void ccorr_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = DISPSYS_CCORR0_BASE;

	DDPDUMP("== DISP %s REGS ==\n", ddp_get_module_name(module));
	if (!disp_helper_get_option(DISP_OPT_PQ_REG_DUMP))
		return;

	DDPDUMP("0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x\n",
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x008, INREG32(module_base + 0x008),
		0x00C, INREG32(module_base + 0x00C));
	DDPDUMP("0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x\n",
		0x010, INREG32(module_base + 0x010),
		0x020, INREG32(module_base + 0x020),
		0x024, INREG32(module_base + 0x024),
		0x028, INREG32(module_base + 0x028));
	DDPDUMP("0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x\n",
		0x02C, INREG32(module_base + 0x02C),
		0x030, INREG32(module_base + 0x030),
		0x080, INREG32(module_base + 0x080),
		0x084, INREG32(module_base + 0x084));
	DDPDUMP("0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x 0x%04x=0x%08x\n",
		0x088, INREG32(module_base + 0x088),
		0x08C, INREG32(module_base + 0x08C),
		0x090, INREG32(module_base + 0x090),
		0x0A0, INREG32(module_base + 0x0A0));
	DDPDUMP("0x%04x=0x%08x\n",
		0x0C0, INREG32(module_base + 0x0C0));
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

	DDPDUMP("== DISP %s ANALYSIS ==\n", ddp_get_module_name(module));
	DDPDUMP("en=%d,config=0x%08x,wh(%dx%d),pos:in(%d,%d)out(%d,%d)\n",
		ccorr_en, ccorr_cfg, (ccorr_size >> 16) & 0x1fff,
		ccorr_size & 0x1fff, ccorr_in_cnt & 0x1fff,
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

	DDPDUMP("== DISP %s REGS ==\n", ddp_get_module_name(module));
	if (!disp_helper_get_option(DISP_OPT_PQ_REG_DUMP))
		return;

	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x008, INREG32(module_base + 0x008),
		0x00C, INREG32(module_base + 0x00C));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x010, INREG32(module_base + 0x010),
		0x020, INREG32(module_base + 0x020),
		0x024, INREG32(module_base + 0x024),
		0x028, INREG32(module_base + 0x028));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x02C, INREG32(module_base + 0x02C),
		0x030, INREG32(module_base + 0x030),
		0x0C0, INREG32(module_base + 0x0C0),
		0x100, INREG32(module_base + 0x100));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x114, INREG32(module_base + 0x114),
		0x118, INREG32(module_base + 0x118),
		0x11C, INREG32(module_base + 0x11C),
		0x120, INREG32(module_base + 0x120));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x124, INREG32(module_base + 0x124),
		0x128, INREG32(module_base + 0x128),
		0x12C, INREG32(module_base + 0x12C),
		0x130, INREG32(module_base + 0x130));
	DDPDUMP("0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x 0x%03x=0x%08x\n",
		0x134, INREG32(module_base + 0x134),
		0x138, INREG32(module_base + 0x138),
		0x13C, INREG32(module_base + 0x13C),
		0x140, INREG32(module_base + 0x140));
	DDPDUMP("0x%03x=0x%08x\n",
		0x144, INREG32(module_base + 0x144));
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

	DDPDUMP("== DISP %s ANALYSIS ==\n", ddp_get_module_name(module));
	DDPDUMP("en=%d,config=0x%08x,wh(%dx%d),pos:in(%d,%d)out(%d,%d)\n",
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
	if (!dpmgr_is_power_on())
		return 0;

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
	case DISP_MODULE_POSTMASK:
		postmask_dump_reg(module);
		break;
	default:
		DDPDUMP("no dump_reg for module %s(%d)\n",
			ddp_get_module_name(module), module);
		break;
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
	case DISP_MODULE_POSTMASK:
		postmask_dump_analysis(module);
		break;
	default:
		DDPDUMP("no dump_analysis for module %s(%d)\n",
			ddp_get_module_name(module), module);
	}
	return 0;
}
