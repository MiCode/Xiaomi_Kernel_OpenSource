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

#include "disp_helper.h"


static char *ddp_signal_0(int bit)
{
	switch (bit) {
	case 31:
		return "OVL0_MOUT1-WDMA0_SEL1";
	case 30:
		return "OVL0_MOUT0-COLOR0_SEL0";
	case 17:
		return "DSI0_SEL-DSI0";
	case 16:
		return "DITHER_MOUT2-WDMA0_SEL1";
	case 15:
		return "DITHER_MOUT1-UFOE_SEL1";
	case 14:
		return "DITHER_MOUT0-RMDA0";
	case 13:
		return "WDMA0_SEL-WDMA0";
	case 12:
		return "RDMA0_SOUT2-DBI0_SEL1";
	case 11:
		return "RDMA0_SOUT2-DSI0_SEL1";
	case 10:
		return "RDMA0_SOUT1-COLOR0_SEL0";
	case 9:
		return "RDMA0_SOUT0-UFOE0_SEL0";
	case 8:
		return "RDMA0-RDMA0_SOUT";
	case 7:
		return "OVL0-OVL_MOUT";
	case 6:
		return "GAMMA0-DITHER0";
	case 5:
		return "DITHER0-DITHER0_MOUT";
	case 4:
		return "COLOR0-CCORR0";
	case 3:
		return "CCORR0_AAL0";
	case 2:
		return "AAL0-GAMMA0";
	case 1:
		return "DBI0_SEL-DBI0";
	case 0:
		return "COLOR0_SEL-COLOR0";
	default:
		return NULL;
	}
}

static char *ddp_signal_1(int bit)
{
	switch (bit) {
	case 12:
		return "UFOE_MOUT0-DSI0_SEL0";
	case 13:
		return "UFOE_MOUT1-DBI0_SEL0";
	case 14:
		return "UFOE_MOUT2-WDMA0_SEL2";
	case 15:
		return "UFOE_SEL-UFOE_MOUT";
	default:
		return NULL;
	}
}

static char *ddp_greq_name(int bit)
{
	switch (bit) {
	case 0:
		return "OVL0";
	case 1:
		return "RDMA0";
	case 2:
		return "WDMA0";
	case 3:
		return "MDP_RDMA0";
	case 4:
		return "MDP_WDMA";
	case 5:
		return "MDP_WROT0";
	case 6:
		return "FAKE";
	default:
		return NULL;
	}
}

static char *ddp_get_mutex_module0_name(unsigned int bit)
{
	switch (bit) {
	case 0:  return "mdp-rdma0";
	case 1:  return "mdp-rsz0";
	case 2:  return "mdp-rsz1";
	case 3:  return "mdp-wdma0";
	case 4:  return "mdp_wrot0";
	case 5:  return "mdp_tdshp";
	case 6:  return "disp-ovl0";
	case 7:  return "disp-rdma0";
	case 8:  return "disp-wdma0";
	case 9:  return "disp-color0";
	case 10: return "disp-ccr0";
	case 11: return "disp-aal0;";
	case 12: return "disp-gamma0";
	case 13: return "disp-dither";
	case 14: return "disp-dsi";
	case 15: return "disp-dbi";
	case 16: return "disp-pwm";

	default:
		return "mutex-unknown";
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
			DDPDUMP("ddp_get_fmt_name, unknown fmt=%d, module=%d\n", fmt, module);
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
			DDPDUMP("ddp_get_fmt_name, unknown fmt=%d, module=%d\n", fmt, module);
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
			DDPDUMP("ddp_get_fmt_name, unknown fmt=%d, module=%d\n", fmt, module);
			return "unknown";
		}
	} else {
		DDPDUMP("ddp_get_fmt_name, unknown module=%d\n", module);
	}

	return "unknown";
}

static char *ddp_clock_0(int bit)
{
	switch (bit) {
	case 0:
		return "SMI_COMMON";
	case 1:
		return "SMI_LARB0";
	case 2:
		return "GALS_COMM0";
	case 3:
		return "GALS_COMM1";
	case 4:
		return "ISP_DL";
	case 5:
		return "MDP_RDMA0";
	case 6:
		return "MDP_RSZ0";
	case 7:
		return "MDP_RSZ1";
	case 8:
		return "MDP_TDSHP";
	case 9:
		return "MDP_WROT0";
	case 10:
		return "MDP_WDMA0";
	case 11:
		return "FAKE_ENG";
	case 12:
		return "DISP_OVL0";
	case 13:
		return "DISP_RDMA0";
	case 14:
		return "DISP_WDMA0";
	case 15:
		return "DISP_COLOR0";
	case 16:
		return "DISP_CCORR0";
	case 17:
		return "DISP_AAL0";
	case 18:
		return "DISP_GAMMA0";
	case 19:
		return "DISP_DITHER0";
	case 20:
		return "DSI0_MM_clock";
	case 21:
		return "DSI0_interface_clock";
	case 22:
		return "DBI0_MM_clock";
	case 23:
		return "DBI0_interface_clock";
	case 24:
		return "F26M_HRT_clock";

	default:
		return NULL;
	}
}

/*************************Module Analysis start***********************/

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
			      ddp_get_mutex_sof_name(REG_FLD_VAL_GET(SOF_FLD_MUTEX0_SOF, val)),
			      ddp_get_mutex_sof_name(REG_FLD_VAL_GET(SOF_FLD_MUTEX0_EOF, val)),
				REG_FLD_VAL_GET(SOF_FLD_MUTEX0_SOF_WAIT, val));

		p += len;
		for (j = 0; j < 32; j++) {
			unsigned int regval = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD0(i));

			if ((regval & (1 << j))) {
				len = sprintf(p, "%s,", ddp_get_mutex_module0_name(j));
				p += len;
			}
		}
		DDPDUMP("%s)\n", mutex_module);
	}
}



/*  ------ clock:
  * Before power on mmsys:
  * CLK_CFG_0_CLR (address is 0x10000048) = 0x80000000 (bit 31).
  * Before using DISP_PWM0 or DISP_PWM1:
  * CLK_CFG_1_CLR(address is 0x10000058)=0x80 (bit 7).
  * Before using DPI pixel clock:
  * CLK_CFG_6_CLR(address is 0x100000A8)=0x80 (bit 7).
  *
  * Only need to enable the corresponding bits of MMSYS_CG_CON0 and MMSYS_CG_CON1 for the modules:
  * smi_common, larb0, mdp_crop, fake_eng, mutex_32k, pwm0, pwm1, dsi0, dsi1, dpi.
  * Other bits could keep 1. Suggest to keep smi_common and larb0 always clock on.
  *
  * --------valid & ready
  * example:
  * ovl0 -> ovl0_mout_ready=1 means engines after ovl_mout are ready for receiving data
  *	ovl0_mout_ready=0 means ovl0_mout can not receive data, maybe ovl0_mout or after engines config error
  * ovl0 -> ovl0_mout_valid=1 means engines before ovl0_mout is OK,
  *	ovl0_mout_valid=0 means ovl can not transfer data to ovl0_mout, means ovl0 or before engines are not ready.
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
				strncat(clock_on, name, (sizeof(clock_on) -
							strlen(clock_on) - 1));
		}
	}

	DDPDUMP("clock on modules:%s\n", clock_on);

	DDPDUMP("valid0=0x%x, valid1=0x%x, ready0=0x%x, ready1=0x%x, greq=0%x\n",
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
		DDPDUMP("smi greq not grant module: (greq: 1 means SMI dose not grant, maybe SMI hang)");

	clock_on[0] = '\0';
	for (i = 0; i < 32; i++) {
		if (greq & (1 << i)) {
			name = ddp_greq_name(i);
			if (!name)
				continue;
			strncat(clock_on, name, (sizeof(clock_on) -
						strlen(clock_on) - 1));
		}
	}
	DDPDUMP("%s\n", clock_on);
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
	DDPDUMP("gamma: en=%d, w=%d, h=%d, in_p_cnt=%d, in_l_cnt=%d, out_p_cnt=%d, out_l_cnt=%d\n",
		DISP_REG_GET(DISP_REG_GAMMA_EN + i * offset),
		(DISP_REG_GET(DISP_REG_GAMMA_SIZE + i * offset) >> 16) & 0x1fff,
		DISP_REG_GET(DISP_REG_GAMMA_SIZE + i * offset) & 0x1fff,
		DISP_REG_GET(DISP_REG_GAMMA_INPUT_COUNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_REG_GAMMA_INPUT_COUNT + i * offset) >> 16) & 0x1fff,
		DISP_REG_GET(DISP_REG_GAMMA_OUTPUT_COUNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_REG_GAMMA_OUTPUT_COUNT + i * offset) >> 16) & 0x1fff);


	DDPDUMP("== DISP GAMMA%d IMPORTANT REGS ==\n", i);
	DDPDUMP("(0x000)GA_EN=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_EN + i * offset));
	DDPDUMP("(0x004)GA_RESET=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_RESET + i * offset));
	DDPDUMP("(0x008)GA_INTEN=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_INTEN + i * offset));
	DDPDUMP("(0x00c)GA_INTSTA=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_INTSTA + i * offset));
	DDPDUMP("(0x010)GA_STATUS=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_STATUS + i * offset));
	DDPDUMP("(0x020)GA_CFG=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_CFG + i * offset));
	DDPDUMP("(0x024)GA_IN_COUNT=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_INPUT_COUNT + i * offset));
	DDPDUMP("(0x028)GA_OUT_COUNT=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_OUTPUT_COUNT + i * offset));
	DDPDUMP("(0x02c)GA_CHKSUM=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_CHKSUM + i * offset));
	DDPDUMP("(0x030)GA_SIZE=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_SIZE + i * offset));
	DDPDUMP("(0x0c0)GA_DUMMY_REG=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_DUMMY_REG + i * offset));
	DDPDUMP("(0x800)GA_LUT=0x%x\n", DISP_REG_GET(DISP_REG_GAMMA_LUT + i * offset));

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

	DDPDUMP("== DISP COLOR%d REGS ==\n", index);
	DDPDUMP("(0x400)COLOR_CFG_MAIN=0x%x\n", DISP_REG_GET(DISP_COLOR_CFG_MAIN));
	DDPDUMP("(0x404)COLOR_PXL_CNT_MAIN=0x%x\n", DISP_REG_GET(DISP_COLOR_PXL_CNT_MAIN));
	DDPDUMP("(0x408)COLOR_LINE_CNT_MAIN=0x%x\n", DISP_REG_GET(DISP_COLOR_LINE_CNT_MAIN));
	DDPDUMP("(0xc00)COLOR_START=0x%x\n", DISP_REG_GET(DISP_COLOR_START));
	DDPDUMP("(0xc28)DISP_COLOR_CK_ON=0x%x\n", DISP_REG_GET(DISP_COLOR_CK_ON));
	DDPDUMP("(0xc50)COLOR_INTER_IP_W=0x%x\n", DISP_REG_GET(DISP_COLOR_INTERNAL_IP_WIDTH));
	DDPDUMP("(0xc54)COLOR_INTER_IP_H=0x%x\n", DISP_REG_GET(DISP_COLOR_INTERNAL_IP_HEIGHT));
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
	DDPDUMP("aal: bypass=%d, relay=%d, en=%d, w=%d, h=%d, in(%d,%d),out(%d,%d)\n",
		DISP_REG_GET(DISP_AAL_EN + i * offset) == 0x0,
		DISP_REG_GET(DISP_AAL_CFG + i * offset) & 0x01,
		DISP_REG_GET(DISP_AAL_EN + i * offset),
		(DISP_REG_GET(DISP_AAL_SIZE + i * offset) >> 16) & 0x1fff,
		DISP_REG_GET(DISP_AAL_SIZE + i * offset) & 0x1fff,
		DISP_REG_GET(DISP_AAL_IN_CNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_AAL_IN_CNT + i * offset) >> 16) & 0x1fff,
		DISP_REG_GET(DISP_AAL_OUT_CNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_AAL_OUT_CNT + i * offset) >> 16) & 0x1fff);

	DDPDUMP("== DISP AAL%d REGS ==\n", i);
	DDPDUMP("(0x000)AAL_EN=0x%x\n", DISP_REG_GET(DISP_AAL_EN + i * offset));
	DDPDUMP("(0x008)AAL_INTEN=0x%x\n", DISP_REG_GET(DISP_AAL_INTEN + i * offset));
	DDPDUMP("(0x00c)AAL_INTSTA=0x%x\n", DISP_REG_GET(DISP_AAL_INTSTA + i * offset));
	DDPDUMP("(0x020)AAL_CFG=0x%x\n", DISP_REG_GET(DISP_AAL_CFG + i * offset));
	DDPDUMP("(0x024)AAL_IN_CNT=0x%x\n", DISP_REG_GET(DISP_AAL_IN_CNT + i * offset));
	DDPDUMP("(0x028)AAL_OUT_CNT=0x%x\n", DISP_REG_GET(DISP_AAL_OUT_CNT + i * offset));
	DDPDUMP("(0x030)AAL_SIZE=0x%x\n", DISP_REG_GET(DISP_AAL_SIZE + i * offset));
	DDPDUMP("(0x20c)AAL_CABC_00=0x%x\n", DISP_REG_GET(DISP_AAL_CABC_00 + i * offset));
	DDPDUMP("(0x214)AAL_CABC_02=0x%x\n", DISP_REG_GET(DISP_AAL_CABC_02 + i * offset));
	DDPDUMP("(0x20c)AAL_STATUS_00=0x%x\n", DISP_REG_GET(DISP_AAL_STATUS_00 + i * offset));
	DDPDUMP("(0x210)AAL_STATUS_01=0x%x\n", DISP_REG_GET(DISP_AAL_STATUS_00 + 0x4 + i * offset));
	DDPDUMP("(0x2a0)AAL_STATUS_31=0x%x\n", DISP_REG_GET(DISP_AAL_STATUS_32 - 0x4 + i * offset));
	DDPDUMP("(0x2a4)AAL_STATUS_32=0x%x\n", DISP_REG_GET(DISP_AAL_STATUS_32 + i * offset));
	DDPDUMP("(0x3b0)AAL_DRE_MAPPING_00=0x%x\n", DISP_REG_GET(DISP_AAL_DRE_MAPPING_00 + i * offset));
}

static void pwm_dump_analysis(enum DISP_MODULE_ENUM module)
{
	int index = 0;
	unsigned long reg_base = 0;

	index = 0;
	reg_base = DISPSYS_PWM0_BASE;

	DDPDUMP("== DISP PWM%d ANALYSIS ==\n", index);

	DDPDUMP("(0x000)PWM_EN=0x%x\n", DISP_REG_GET(reg_base + DISP_PWM_EN_OFF));
	DDPDUMP("(0x008)PWM_CON_0=0x%x\n", DISP_REG_GET(reg_base + DISP_PWM_CON_0_OFF));
	DDPDUMP("(0x010)PWM_CON_1=0x%x\n", DISP_REG_GET(reg_base + DISP_PWM_CON_1_OFF));
	DDPDUMP("(0x028)PWM_DEBUG=0x%x\n", DISP_REG_GET(reg_base + 0x28));

}

static void ccorr_dump_analyze(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned int offset = 0x1000;

	if (module == DISP_MODULE_CCORR0)
		i = 0;
	else
		i = 1;

	DDPDUMP("ccorr: en=%d, config=%d, w=%d, h=%d, in_p_cnt=%d, in_l_cnt=%d, out_p_cnt=%d, out_l_cnt=%d\n",
		DISP_REG_GET(DISP_REG_CCORR_EN + i * offset),
		DISP_REG_GET(DISP_REG_CCORR_CFG + i * offset),
		(DISP_REG_GET(DISP_REG_CCORR_SIZE + i * offset) >> 16) & 0x1fff,
		DISP_REG_GET(DISP_REG_CCORR_SIZE + i * offset) & 0x1fff,
		DISP_REG_GET(DISP_REG_CCORR_IN_CNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_REG_CCORR_IN_CNT + i * offset) >> 16) & 0x1fff,
		DISP_REG_GET(DISP_REG_CCORR_IN_CNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_REG_CCORR_IN_CNT + i * offset) >> 16) & 0x1fff);

	DDPDUMP("== DISP CCORR REGS ==\n");
	DDPDUMP("(00)EN=0x%x\n", DISP_REG_GET(DISP_REG_CCORR_EN + i * offset));
	DDPDUMP("(20)CFG=0x%x\n", DISP_REG_GET(DISP_REG_CCORR_CFG + i * offset));
	DDPDUMP("(24)IN_CNT=0x%x\n", DISP_REG_GET(DISP_REG_CCORR_IN_CNT + i * offset));
	DDPDUMP("(28)OUT_CNT=0x%x\n", DISP_REG_GET(DISP_REG_CCORR_OUT_CNT + i * offset));
	DDPDUMP("(30)SIZE=0x%x\n", DISP_REG_GET(DISP_REG_CCORR_SIZE + i * offset));

}

static void dither_dump_analyze(enum DISP_MODULE_ENUM module)
{
	int i;
	unsigned int offset = 0x1000;

	if (module == DISP_MODULE_DITHER0)
		i = 0;
	else
		i = 1;


	DDPDUMP
		("dither: en=%d, config=%d, w=%d, h=%d, in_p_cnt=%d, in_l_cnt=%d, out_p_cnt=%d, out_l_cnt=%d\n",
		DISP_REG_GET(DISPSYS_DITHER0_BASE + 0x000 + i * offset),
		DISP_REG_GET(DISPSYS_DITHER0_BASE + 0x020 + i * offset),
		(DISP_REG_GET(DISP_REG_DITHER_SIZE + i * offset) >> 16) & 0x1fff,
		DISP_REG_GET(DISP_REG_DITHER_SIZE + i * offset) & 0x1fff,
		DISP_REG_GET(DISP_REG_DITHER_IN_CNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_REG_DITHER_IN_CNT + i * offset) >> 16) & 0x1fff,
		DISP_REG_GET(DISP_REG_DITHER_OUT_CNT + i * offset) & 0x1fff,
		(DISP_REG_GET(DISP_REG_DITHER_OUT_CNT + i * offset) >> 16) & 0x1fff);

	DDPDUMP("== DISP DITHER REGS ==\n");
	DDPDUMP("(00)EN=0x%x\n", DISP_REG_GET(DISP_REG_DITHER_EN + i * offset));
	DDPDUMP("(20)CFG=0x%x\n", DISP_REG_GET(DISP_REG_DITHER_CFG + i * offset));
	DDPDUMP("(24)IN_CNT=0x%x\n", DISP_REG_GET(DISP_REG_DITHER_IN_CNT + i * offset));
	DDPDUMP("(28)OUT_CNT=0x%x\n", DISP_REG_GET(DISP_REG_DITHER_OUT_CNT + i * offset));
	DDPDUMP("(30)SIZE=0x%x\n", DISP_REG_GET(DISP_REG_DITHER_SIZE + i * offset));
}

int split_dump_analysis(enum DISP_MODULE_ENUM module)
{
#if 0
	unsigned int pixel = DISP_REG_GET_FIELD(DEBUG_FLD_IN_PIXEL_CNT, DISP_REG_SPLIT_DEBUG);
	unsigned int state = DISP_REG_GET_FIELD(DEBUG_FLD_SPLIT_FSM, DISP_REG_SPLIT_DEBUG);

	DDPMSG("== DISP SPLIT0 ANALYSIS ==\n");
	DDPMSG("cur_pixel %u, state %s\n", pixel, split_state(state));
#endif
	return 0;
}


/*************************Module Analysis end***********************/

/*************************Module reg dump start*********************/

void disp_aal_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000000, INREG32(module_base + 0x00000000),
		0x00000004, INREG32(module_base + 0x00000004),
		0x00000008, INREG32(module_base + 0x00000008),
		0x0000000c, INREG32(module_base + 0x0000000c));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000010, INREG32(module_base + 0x00000010),
		0x00000020, INREG32(module_base + 0x00000020),
		0x00000024, INREG32(module_base + 0x00000024),
		0x00000028, INREG32(module_base + 0x00000028));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0000002c, INREG32(module_base + 0x0000002c),
		0x00000030, INREG32(module_base + 0x00000030),
		0x000000b0, INREG32(module_base + 0x000000b0),
		0x000000c0, INREG32(module_base + 0x000000c0));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x000000fc, INREG32(module_base + 0x000000fc),
		0x00000204, INREG32(module_base + 0x00000204),
		0x0000021c, INREG32(module_base + 0x0000021c),
		0x00000224, INREG32(module_base + 0x00000224));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000228, INREG32(module_base + 0x00000228),
		0x0000022c, INREG32(module_base + 0x0000022c),
		0x00000230, INREG32(module_base + 0x00000230),
		0x00000234, INREG32(module_base + 0x00000234));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000238, INREG32(module_base + 0x00000238),
		0x0000023c, INREG32(module_base + 0x0000023c),
		0x00000240, INREG32(module_base + 0x00000240),
		0x00000244, INREG32(module_base + 0x00000244));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000248, INREG32(module_base + 0x00000248),
		0x0000024c, INREG32(module_base + 0x0000024c),
		0x00000250, INREG32(module_base + 0x00000250),
		0x00000254, INREG32(module_base + 0x00000254));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000258, INREG32(module_base + 0x00000258),
		0x0000025c, INREG32(module_base + 0x0000025c),
		0x00000260, INREG32(module_base + 0x00000260),
		0x00000264, INREG32(module_base + 0x00000264));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000268, INREG32(module_base + 0x00000268),
		0x0000026c, INREG32(module_base + 0x0000026c),
		0x00000270, INREG32(module_base + 0x00000270),
		0x00000274, INREG32(module_base + 0x00000274));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000278, INREG32(module_base + 0x00000278),
		0x0000027c, INREG32(module_base + 0x0000027c),
		0x00000280, INREG32(module_base + 0x00000280),
		0x00000284, INREG32(module_base + 0x00000284));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000288, INREG32(module_base + 0x00000288),
		0x0000028c, INREG32(module_base + 0x0000028c),
		0x00000290, INREG32(module_base + 0x00000290),
		0x00000294, INREG32(module_base + 0x00000294));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000298, INREG32(module_base + 0x00000298),
		0x0000029c, INREG32(module_base + 0x0000029c),
		0x000002a0, INREG32(module_base + 0x000002a0),
		0x000002a4, INREG32(module_base + 0x000002a4));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000358, INREG32(module_base + 0x00000358),
		0x0000035c, INREG32(module_base + 0x0000035c),
		0x00000360, INREG32(module_base + 0x00000360),
		0x00000364, INREG32(module_base + 0x00000364));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000368, INREG32(module_base + 0x00000368),
		0x0000036c, INREG32(module_base + 0x0000036c),
		0x00000370, INREG32(module_base + 0x00000370),
		0x00000374, INREG32(module_base + 0x00000374));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000378, INREG32(module_base + 0x00000378),
		0x0000037c, INREG32(module_base + 0x0000037c),
		0x00000380, INREG32(module_base + 0x00000380),
		0x000003b0, INREG32(module_base + 0x000003b0));
	DDPDUMP("aal: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000440, INREG32(module_base + 0x00000440),
		0x00000444, INREG32(module_base + 0x00000444),
		0x00000448, INREG32(module_base + 0x00000448),
		0x0000044c, INREG32(module_base + 0x0000044c));
	DDPDUMP("aal: 0x%04x=0x%08x\n",
		0x00000450, INREG32(module_base + 0x00000450));

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void disp_ccorr_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("ccorr: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x008, INREG32(module_base + 0x008),
		0x00c, INREG32(module_base + 0x00c));
	DDPDUMP("ccorr: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x010, INREG32(module_base + 0x010),
		0x020, INREG32(module_base + 0x020),
		0x024, INREG32(module_base + 0x024),
		0x028, INREG32(module_base + 0x028));
	DDPDUMP("ccorr: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x02c, INREG32(module_base + 0x02c),
		0x030, INREG32(module_base + 0x030),
		0x080, INREG32(module_base + 0x080),
		0x084, INREG32(module_base + 0x084));
	DDPDUMP("ccorr: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x088, INREG32(module_base + 0x088),
		0x08c, INREG32(module_base + 0x08c),
		0x090, INREG32(module_base + 0x090),
		0x0a0, INREG32(module_base + 0x0a0));
	DDPDUMP("ccorr: 0x%04x=0x%08x\n",
		0x0c0, INREG32(module_base + 0x0c0));

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void disp_color_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000400, INREG32(module_base + 0x00000400),
		0x00000404, INREG32(module_base + 0x00000404),
		0x00000408, INREG32(module_base + 0x00000408),
		0x0000040c, INREG32(module_base + 0x0000040c));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000410, INREG32(module_base + 0x00000410),
		0x00000418, INREG32(module_base + 0x00000418),
		0x0000041c, INREG32(module_base + 0x0000041c),
		0x00000420, INREG32(module_base + 0x00000420));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000428, INREG32(module_base + 0x00000428),
		0x0000042c, INREG32(module_base + 0x0000042c),
		0x00000430, INREG32(module_base + 0x00000430),
		0x00000434, INREG32(module_base + 0x00000434));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000438, INREG32(module_base + 0x00000438),
		0x00000484, INREG32(module_base + 0x00000484),
		0x00000488, INREG32(module_base + 0x00000488),
		0x0000048c, INREG32(module_base + 0x0000048c));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000490, INREG32(module_base + 0x00000490),
		0x00000494, INREG32(module_base + 0x00000494),
		0x00000498, INREG32(module_base + 0x00000498),
		0x0000049c, INREG32(module_base + 0x0000049c));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x000004a0, INREG32(module_base + 0x000004a0),
		0x000004a4, INREG32(module_base + 0x000004a4),
		0x000004a8, INREG32(module_base + 0x000004a8),
		0x000004ac, INREG32(module_base + 0x000004ac));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x000004b0, INREG32(module_base + 0x000004b0),
		0x000004b4, INREG32(module_base + 0x000004b4),
		0x000004b8, INREG32(module_base + 0x000004b8),
		0x000004bc, INREG32(module_base + 0x000004bc));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000620, INREG32(module_base + 0x00000620),
		0x00000624, INREG32(module_base + 0x00000624),
		0x00000628, INREG32(module_base + 0x00000628),
		0x0000062c, INREG32(module_base + 0x0000062c));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000630, INREG32(module_base + 0x00000630),
		0x00000740, INREG32(module_base + 0x00000740),
		0x0000074c, INREG32(module_base + 0x0000074c),
		0x00000768, INREG32(module_base + 0x00000768));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0000076c, INREG32(module_base + 0x0000076c),
		0x0000079c, INREG32(module_base + 0x0000079c),
		0x000007e0, INREG32(module_base + 0x000007e0),
		0x000007e4, INREG32(module_base + 0x000007e4));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x000007e8, INREG32(module_base + 0x000007e8),
		0x000007ec, INREG32(module_base + 0x000007ec),
		0x000007f0, INREG32(module_base + 0x000007f0),
		0x000007fc, INREG32(module_base + 0x000007fc));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000800, INREG32(module_base + 0x00000800),
		0x00000804, INREG32(module_base + 0x00000804),
		0x00000808, INREG32(module_base + 0x00000808),
		0x0000080c, INREG32(module_base + 0x0000080c));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000810, INREG32(module_base + 0x00000810),
		0x00000814, INREG32(module_base + 0x00000814),
		0x00000818, INREG32(module_base + 0x00000818),
		0x0000081c, INREG32(module_base + 0x0000081c));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000820, INREG32(module_base + 0x00000820),
		0x00000824, INREG32(module_base + 0x00000824),
		0x00000828, INREG32(module_base + 0x00000828),
		0x0000082c, INREG32(module_base + 0x0000082c));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000830, INREG32(module_base + 0x00000830),
		0x00000834, INREG32(module_base + 0x00000834),
		0x00000838, INREG32(module_base + 0x00000838),
		0x0000083c, INREG32(module_base + 0x0000083c));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000840, INREG32(module_base + 0x00000840),
		0x00000844, INREG32(module_base + 0x00000844),
		0x00000848, INREG32(module_base + 0x00000848),
		0x0000084c, INREG32(module_base + 0x0000084c));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000850, INREG32(module_base + 0x00000850),
		0x00000854, INREG32(module_base + 0x00000854),
		0x00000858, INREG32(module_base + 0x00000858),
		0x0000085c, INREG32(module_base + 0x0000085c));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000c00, INREG32(module_base + 0x00000c00),
		0x00000c04, INREG32(module_base + 0x00000c04),
		0x00000c08, INREG32(module_base + 0x00000c08),
		0x00000c0c, INREG32(module_base + 0x00000c0c));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000c10, INREG32(module_base + 0x00000c10),
		0x00000c14, INREG32(module_base + 0x00000c14),
		0x00000c18, INREG32(module_base + 0x00000c18),
		0x00000c28, INREG32(module_base + 0x00000c28));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000c50, INREG32(module_base + 0x00000c50),
		0x00000c54, INREG32(module_base + 0x00000c54),
		0x00000c60, INREG32(module_base + 0x00000c60),
		0x00000ca0, INREG32(module_base + 0x00000ca0));
	DDPDUMP("color: 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000cb0, INREG32(module_base + 0x00000cb0),
		0x00000cf0, INREG32(module_base + 0x00000cf0));

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void disp_dither_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("dither: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000000, INREG32(module_base + 0x00000000),
		0x00000004, INREG32(module_base + 0x00000004),
		0x00000008, INREG32(module_base + 0x00000008),
		0x0000000c, INREG32(module_base + 0x0000000c));
	DDPDUMP("dither: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000010, INREG32(module_base + 0x00000010),
		0x00000020, INREG32(module_base + 0x00000020),
		0x00000024, INREG32(module_base + 0x00000024),
		0x00000028, INREG32(module_base + 0x00000028));
	DDPDUMP("dither: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0000002c, INREG32(module_base + 0x0000002c),
		0x00000030, INREG32(module_base + 0x00000030),
		0x000000c0, INREG32(module_base + 0x000000c0),
		0x00000100, INREG32(module_base + 0x00000100));
	DDPDUMP("dither: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000114, INREG32(module_base + 0x00000114),
		0x00000118, INREG32(module_base + 0x00000118),
		0x0000011c, INREG32(module_base + 0x0000011c),
		0x00000120, INREG32(module_base + 0x00000120));
	DDPDUMP("dither: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000124, INREG32(module_base + 0x00000124),
		0x00000128, INREG32(module_base + 0x00000128),
		0x0000012c, INREG32(module_base + 0x0000012c),
		0x00000130, INREG32(module_base + 0x00000130));
	DDPDUMP("dither: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000134, INREG32(module_base + 0x00000134),
		0x00000138, INREG32(module_base + 0x00000138),
		0x0000013c, INREG32(module_base + 0x0000013c),
		0x00000140, INREG32(module_base + 0x00000140));
	DDPDUMP("dither: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00000144, INREG32(module_base + 0x00000144),
		0x0000014c, INREG32(module_base + 0x0000014c),
		0x00000150, INREG32(module_base + 0x00000150),
		0x00000154, INREG32(module_base + 0x00000154));
	DDPDUMP("dither:\n");

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void disp_dsi_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x008, INREG32(module_base + 0x008),
		0x00c, INREG32(module_base + 0x00c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x010, INREG32(module_base + 0x010),
		0x014, INREG32(module_base + 0x014),
		0x018, INREG32(module_base + 0x018),
		0x01c, INREG32(module_base + 0x01c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x020, INREG32(module_base + 0x020),
		0x024, INREG32(module_base + 0x024),
		0x028, INREG32(module_base + 0x028),
		0x02c, INREG32(module_base + 0x02c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x030, INREG32(module_base + 0x030),
		0x034, INREG32(module_base + 0x034),
		0x038, INREG32(module_base + 0x038),
		0x03c, INREG32(module_base + 0x03c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x050, INREG32(module_base + 0x050),
		0x054, INREG32(module_base + 0x054),
		0x058, INREG32(module_base + 0x058),
		0x05c, INREG32(module_base + 0x05c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x060, INREG32(module_base + 0x060),
		0x064, INREG32(module_base + 0x064),
		0x068, INREG32(module_base + 0x068),
		0x074, INREG32(module_base + 0x074));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x078, INREG32(module_base + 0x078),
		0x07c, INREG32(module_base + 0x07c),
		0x080, INREG32(module_base + 0x080),
		0x084, INREG32(module_base + 0x084));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x088, INREG32(module_base + 0x088),
		0x090, INREG32(module_base + 0x090),
		0x094, INREG32(module_base + 0x094),
		0x098, INREG32(module_base + 0x098));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0a0, INREG32(module_base + 0x0a0),
		0x0a4, INREG32(module_base + 0x0a4),
		0x0a8, INREG32(module_base + 0x0a8),
		0x0f0, INREG32(module_base + 0x0f0));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x100, INREG32(module_base + 0x100),
		0x104, INREG32(module_base + 0x104),
		0x108, INREG32(module_base + 0x108),
		0x10c, INREG32(module_base + 0x10c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x110, INREG32(module_base + 0x110),
		0x114, INREG32(module_base + 0x114),
		0x118, INREG32(module_base + 0x118),
		0x11c, INREG32(module_base + 0x11c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x130, INREG32(module_base + 0x130),
		0x134, INREG32(module_base + 0x134),
		0x138, INREG32(module_base + 0x138),
		0x13c, INREG32(module_base + 0x13c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x140, INREG32(module_base + 0x140),
		0x144, INREG32(module_base + 0x144),
		0x148, INREG32(module_base + 0x148),
		0x14c, INREG32(module_base + 0x14c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x150, INREG32(module_base + 0x150),
		0x154, INREG32(module_base + 0x154),
		0x158, INREG32(module_base + 0x158),
		0x15c, INREG32(module_base + 0x15c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x160, INREG32(module_base + 0x160),
		0x164, INREG32(module_base + 0x164),
		0x168, INREG32(module_base + 0x168),
		0x16c, INREG32(module_base + 0x16c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x170, INREG32(module_base + 0x170),
		0x174, INREG32(module_base + 0x174),
		0x178, INREG32(module_base + 0x178),
		0x17c, INREG32(module_base + 0x17c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x180, INREG32(module_base + 0x180),
		0x184, INREG32(module_base + 0x184),
		0x188, INREG32(module_base + 0x188),
		0x18c, INREG32(module_base + 0x18c));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x190, INREG32(module_base + 0x190),
		0x194, INREG32(module_base + 0x194),
		0x1a0, INREG32(module_base + 0x1a0),
		0x1a4, INREG32(module_base + 0x1a4));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x1a8, INREG32(module_base + 0x1a8),
		0x1ac, INREG32(module_base + 0x1ac),
		0x1b0, INREG32(module_base + 0x1b0),
		0x1b4, INREG32(module_base + 0x1b4));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x1b8, INREG32(module_base + 0x1b8),
		0x1bc, INREG32(module_base + 0x1bc),
		0x1c0, INREG32(module_base + 0x1c0),
		0x1c4, INREG32(module_base + 0x1c4));
	DDPDUMP("dsi: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x1c8, INREG32(module_base + 0x1c8),
		0x1cc, INREG32(module_base + 0x1cc),
		0x1d0, INREG32(module_base + 0x1d0),
		0x1d4, INREG32(module_base + 0x1d4));
	DDPDUMP("dsi: 0x%04x=0x%08x\n",
		0x200, INREG32(module_base + 0x200));

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void disp_gamma_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("gamma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x008, INREG32(module_base + 0x008),
		0x00c, INREG32(module_base + 0x00c));
	DDPDUMP("gamma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x010, INREG32(module_base + 0x010),
		0x020, INREG32(module_base + 0x020),
		0x024, INREG32(module_base + 0x024),
		0x028, INREG32(module_base + 0x028));
	DDPDUMP("gamma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x02c, INREG32(module_base + 0x02c),
		0x030, INREG32(module_base + 0x030),
		0x700, INREG32(module_base + 0x700));

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void disp_mutex_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("mutex: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0, INREG32(module_base + 0x0),
		0x4, INREG32(module_base + 0x4),
		0x8, INREG32(module_base + 0x8),
		0x020, INREG32(module_base + 0x020));
	DDPDUMP("mutex: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x028, INREG32(module_base + 0x028),
		0x02c, INREG32(module_base + 0x02c),
		0x030, INREG32(module_base + 0x030),
		0x040, INREG32(module_base + 0x040));
	DDPDUMP("mutex: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x048, INREG32(module_base + 0x048),
		0x04c, INREG32(module_base + 0x04c),
		0x050, INREG32(module_base + 0x050),
		0x060, INREG32(module_base + 0x060));
	DDPDUMP("mutex: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x068, INREG32(module_base + 0x068),
		0x06c, INREG32(module_base + 0x06c),
		0x070, INREG32(module_base + 0x070),
		0x080, INREG32(module_base + 0x080));
	DDPDUMP("mutex: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x088, INREG32(module_base + 0x088),
		0x08c, INREG32(module_base + 0x08c),
		0x090, INREG32(module_base + 0x090),
		0x0a0, INREG32(module_base + 0x0a0));
	DDPDUMP("mutex: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0a8, INREG32(module_base + 0x0a8),
		0x0ac, INREG32(module_base + 0x0ac),
		0x0b0, INREG32(module_base + 0x0b0),
		0x0c0, INREG32(module_base + 0x0c0));
	DDPDUMP("mutex: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0c8, INREG32(module_base + 0x0c8),
		0x0cc, INREG32(module_base + 0x0cc),
		0x0d0, INREG32(module_base + 0x0d0),
		0x0e0, INREG32(module_base + 0x0e0));
	DDPDUMP("mutex: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0e8, INREG32(module_base + 0x0e8),
		0x0ec, INREG32(module_base + 0x0ec),
		0x0f0, INREG32(module_base + 0x0f0),
		0x100, INREG32(module_base + 0x100));
	DDPDUMP("mutex: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x108, INREG32(module_base + 0x108),
		0x10c, INREG32(module_base + 0x10c),
		0x110, INREG32(module_base + 0x110),
		0x120, INREG32(module_base + 0x120));
	DDPDUMP("mutex: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x128, INREG32(module_base + 0x128),
		0x12c, INREG32(module_base + 0x12c),
		0x130, INREG32(module_base + 0x130),
		0x140, INREG32(module_base + 0x140));
	DDPDUMP("mutex: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x148, INREG32(module_base + 0x148),
		0x14c, INREG32(module_base + 0x14c),
		0x150, INREG32(module_base + 0x150),
		0x30c, INREG32(module_base + 0x30c));
	DDPDUMP("mutex:\n");

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void disp_ovl_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0, INREG32(module_base + 0x0),
		0x4, INREG32(module_base + 0x4),
		0x8, INREG32(module_base + 0x8),
		0xc, INREG32(module_base + 0xc));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x10, INREG32(module_base + 0x10),
		0x14, INREG32(module_base + 0x14),
		0x20, INREG32(module_base + 0x20),
		0x24, INREG32(module_base + 0x24));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x28, INREG32(module_base + 0x28),
		0x2c, INREG32(module_base + 0x2c),
		0x30, INREG32(module_base + 0x30),
		0x34, INREG32(module_base + 0x34));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x38, INREG32(module_base + 0x38),
		0x3c, INREG32(module_base + 0x3c),
		0xf40, INREG32(module_base + 0xf40),
		0x44, INREG32(module_base + 0x44));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x48, INREG32(module_base + 0x48),
		0x4c, INREG32(module_base + 0x4c),
		0x50, INREG32(module_base + 0x50),
		0x54, INREG32(module_base + 0x54));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x58, INREG32(module_base + 0x58),
		0x5c, INREG32(module_base + 0x5c),
		0xf60, INREG32(module_base + 0xf60),
		0x64, INREG32(module_base + 0x64));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x68, INREG32(module_base + 0x68),
		0x6c, INREG32(module_base + 0x6c),
		0x70, INREG32(module_base + 0x70),
		0x74, INREG32(module_base + 0x74));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x78, INREG32(module_base + 0x78),
		0x7c, INREG32(module_base + 0x7c),
		0xf80, INREG32(module_base + 0xf80),
		0x84, INREG32(module_base + 0x84));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x88, INREG32(module_base + 0x88),
		0x8c, INREG32(module_base + 0x8c),
		0x90, INREG32(module_base + 0x90),
		0x94, INREG32(module_base + 0x94));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x98, INREG32(module_base + 0x98),
		0x9c, INREG32(module_base + 0x9c),
		0xfa0, INREG32(module_base + 0xfa0),
		0xa4, INREG32(module_base + 0xa4));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xa8, INREG32(module_base + 0xa8),
		0xac, INREG32(module_base + 0xac),
		0xc0, INREG32(module_base + 0xc0),
		0xc8, INREG32(module_base + 0xc8));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xcc, INREG32(module_base + 0xcc),
		0xd0, INREG32(module_base + 0xd0),
		0xe0, INREG32(module_base + 0xe0),
		0xe8, INREG32(module_base + 0xe8));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xec, INREG32(module_base + 0xec),
		0xf0, INREG32(module_base + 0xf0),
		0x100, INREG32(module_base + 0x100),
		0x108, INREG32(module_base + 0x108));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x10c, INREG32(module_base + 0x10c),
		0x110, INREG32(module_base + 0x110),
		0x120, INREG32(module_base + 0x120),
		0x128, INREG32(module_base + 0x128));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x12c, INREG32(module_base + 0x12c),
		0x130, INREG32(module_base + 0x130),
		0x1d4, INREG32(module_base + 0x1d4),
		0x1dc, INREG32(module_base + 0x1dc));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x1e0, INREG32(module_base + 0x1e0),
		0x1e4, INREG32(module_base + 0x1e4),
		0x1e8, INREG32(module_base + 0x1e8),
		0x1ec, INREG32(module_base + 0x1ec));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x1f0, INREG32(module_base + 0x1f0),
		0x1f4, INREG32(module_base + 0x1f4),
		0x1f8, INREG32(module_base + 0x1f8),
		0x1fc, INREG32(module_base + 0x1fc));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x200, INREG32(module_base + 0x200),
		0x208, INREG32(module_base + 0x208),
		0x20c, INREG32(module_base + 0x20c),
		0x210, INREG32(module_base + 0x210));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x214, INREG32(module_base + 0x214),
		0x218, INREG32(module_base + 0x218),
		0x21c, INREG32(module_base + 0x21c),
		0x220, INREG32(module_base + 0x220));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x224, INREG32(module_base + 0x224),
		0x228, INREG32(module_base + 0x228),
		0x22c, INREG32(module_base + 0x22c),
		0x230, INREG32(module_base + 0x230));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x234, INREG32(module_base + 0x234),
		0x238, INREG32(module_base + 0x238),
		0x240, INREG32(module_base + 0x240),
		0x244, INREG32(module_base + 0x244));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x24c, INREG32(module_base + 0x24c),
		0x250, INREG32(module_base + 0x250),
		0x254, INREG32(module_base + 0x254),
		0x258, INREG32(module_base + 0x258));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x25c, INREG32(module_base + 0x25c),
		0x260, INREG32(module_base + 0x260),
		0x264, INREG32(module_base + 0x264),
		0x268, INREG32(module_base + 0x268));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x26c, INREG32(module_base + 0x26c),
		0x270, INREG32(module_base + 0x270),
		0x280, INREG32(module_base + 0x280),
		0x284, INREG32(module_base + 0x284));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x288, INREG32(module_base + 0x288),
		0x28c, INREG32(module_base + 0x28c),
		0x290, INREG32(module_base + 0x290),
		0x29c, INREG32(module_base + 0x29c));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x2a0, INREG32(module_base + 0x2a0),
		0x2a4, INREG32(module_base + 0x2a4),
		0x2b0, INREG32(module_base + 0x2b0),
		0x2b4, INREG32(module_base + 0x2b4));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x2b8, INREG32(module_base + 0x2b8),
		0x2bc, INREG32(module_base + 0x2bc),
		0x2c0, INREG32(module_base + 0x2c0),
		0x2c4, INREG32(module_base + 0x2c4));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x2c8, INREG32(module_base + 0x2c8),
		0x324, INREG32(module_base + 0x324),
		0x330, INREG32(module_base + 0x330),
		0x334, INREG32(module_base + 0x334));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x338, INREG32(module_base + 0x338),
		0x33c, INREG32(module_base + 0x33c),
		0xfb0, INREG32(module_base + 0xfb0),
		0x344, INREG32(module_base + 0x344));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x348, INREG32(module_base + 0x348),
		0x34c, INREG32(module_base + 0x34c),
		0x350, INREG32(module_base + 0x350),
		0x354, INREG32(module_base + 0x354));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x358, INREG32(module_base + 0x358),
		0x35c, INREG32(module_base + 0x35c),
		0xfb4, INREG32(module_base + 0xfb4),
		0x364, INREG32(module_base + 0x364));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x368, INREG32(module_base + 0x368),
		0x36c, INREG32(module_base + 0x36c),
		0x370, INREG32(module_base + 0x370),
		0x374, INREG32(module_base + 0x374));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x378, INREG32(module_base + 0x378),
		0x37c, INREG32(module_base + 0x37c),
		0xfb8, INREG32(module_base + 0xfb8),
		0x384, INREG32(module_base + 0x384));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x388, INREG32(module_base + 0x388),
		0x38c, INREG32(module_base + 0x38c),
		0x390, INREG32(module_base + 0x390),
		0x394, INREG32(module_base + 0x394));
	DDPDUMP("ovl: 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x398, INREG32(module_base + 0x398),
		0xfc0, INREG32(module_base + 0xfc0));

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void disp_pwm_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("pwm: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0, INREG32(module_base + 0x0),
		0x4, INREG32(module_base + 0x4),
		0x8, INREG32(module_base + 0x8),
		0xc, INREG32(module_base + 0xc));
	DDPDUMP("pwm: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x10, INREG32(module_base + 0x10),
		0x14, INREG32(module_base + 0x14),
		0x18, INREG32(module_base + 0x18),
		0x1c, INREG32(module_base + 0x1c));
	DDPDUMP("pwm: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x80, INREG32(module_base + 0x80),
		0x28, INREG32(module_base + 0x28),
		0x2c, INREG32(module_base + 0x2c),
		0x30, INREG32(module_base + 0x30));
	DDPDUMP("pwm: 0x%04x=0x%08x\n",
		0xc0, INREG32(module_base + 0xc0));

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void disp_rdma_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x010, INREG32(module_base + 0x010),
		0x014, INREG32(module_base + 0x014));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x018, INREG32(module_base + 0x018),
		0x01c, INREG32(module_base + 0x01c),
		0x024, INREG32(module_base + 0x024),
		0x02c, INREG32(module_base + 0x02c));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x030, INREG32(module_base + 0x030),
		0x034, INREG32(module_base + 0x034),
		0x038, INREG32(module_base + 0x038),
		0x03c, INREG32(module_base + 0x03c));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x040, INREG32(module_base + 0x040),
		0x044, INREG32(module_base + 0x044),
		0x054, INREG32(module_base + 0x054),
		0x058, INREG32(module_base + 0x058));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x05c, INREG32(module_base + 0x05c),
		0x060, INREG32(module_base + 0x060),
		0x064, INREG32(module_base + 0x064),
		0x068, INREG32(module_base + 0x068));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x06c, INREG32(module_base + 0x06c),
		0x070, INREG32(module_base + 0x070),
		0x074, INREG32(module_base + 0x074),
		0x078, INREG32(module_base + 0x078));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x07c, INREG32(module_base + 0x07c),
		0x080, INREG32(module_base + 0x080),
		0x084, INREG32(module_base + 0x084),
		0x088, INREG32(module_base + 0x088));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x08c, INREG32(module_base + 0x08c),
		0x090, INREG32(module_base + 0x090),
		0x094, INREG32(module_base + 0x094),
		0xf00, INREG32(module_base + 0xf00));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0a0, INREG32(module_base + 0x0a0),
		0x0a4, INREG32(module_base + 0x0a4),
		0x0a8, INREG32(module_base + 0x0a8),
		0x0ac, INREG32(module_base + 0x0ac));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0b0, INREG32(module_base + 0x0b0),
		0x0b4, INREG32(module_base + 0x0b4),
		0x0bc, INREG32(module_base + 0x0bc),
		0x0c0, INREG32(module_base + 0x0c0));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0d0, INREG32(module_base + 0x0d0),
		0x0d4, INREG32(module_base + 0x0d4),
		0x0d8, INREG32(module_base + 0x0d8),
		0x0dc, INREG32(module_base + 0x0dc));
	DDPDUMP("rdma: 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0e0, INREG32(module_base + 0x0e0),
		0x0e4, INREG32(module_base + 0x0e4));

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void disp_wdma_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0, INREG32(module_base + 0x0),
		0x4, INREG32(module_base + 0x4),
		0x8, INREG32(module_base + 0x8),
		0xc, INREG32(module_base + 0xc));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x10, INREG32(module_base + 0x10),
		0x14, INREG32(module_base + 0x14),
		0x18, INREG32(module_base + 0x18),
		0x1c, INREG32(module_base + 0x1c));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x20, INREG32(module_base + 0x20),
		0x28, INREG32(module_base + 0x28),
		0x2c, INREG32(module_base + 0x2c),
		0x30, INREG32(module_base + 0x30));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x38, INREG32(module_base + 0x38),
		0x78, INREG32(module_base + 0x78),
		0x80, INREG32(module_base + 0x80),
		0x84, INREG32(module_base + 0x84));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x88, INREG32(module_base + 0x88),
		0xa0, INREG32(module_base + 0xa0),
		0xa4, INREG32(module_base + 0xa4),
		0xa8, INREG32(module_base + 0xa8));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xac, INREG32(module_base + 0xac),
		0xb8, INREG32(module_base + 0xb8),
		0x100, INREG32(module_base + 0x100),
		0x104, INREG32(module_base + 0x104));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x108, INREG32(module_base + 0x108),
		0x200, INREG32(module_base + 0x200),
		0x204, INREG32(module_base + 0x204),
		0x208, INREG32(module_base + 0x208));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x20c, INREG32(module_base + 0x20c),
		0x210, INREG32(module_base + 0x210),
		0x214, INREG32(module_base + 0x214),
		0x218, INREG32(module_base + 0x218));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x21c, INREG32(module_base + 0x21c),
		0x220, INREG32(module_base + 0x220),
		0x224, INREG32(module_base + 0x224),
		0x228, INREG32(module_base + 0x228));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x22c, INREG32(module_base + 0x22c),
		0x230, INREG32(module_base + 0x230),
		0x234, INREG32(module_base + 0x234),
		0x250, INREG32(module_base + 0x250));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x254, INREG32(module_base + 0x254),
		0x258, INREG32(module_base + 0x258),
		0x25c, INREG32(module_base + 0x25c),
		0xf00, INREG32(module_base + 0xf00));
	DDPDUMP("wdma: 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xf04, INREG32(module_base + 0xf04),
		0xf08, INREG32(module_base + 0xf08));

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void mipi_tx_config_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x00c, INREG32(module_base + 0x00c),
		0x010, INREG32(module_base + 0x010),
		0x014, INREG32(module_base + 0x014),
		0x018, INREG32(module_base + 0x018));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x028, INREG32(module_base + 0x028),
		0x02c, INREG32(module_base + 0x02c),
		0x030, INREG32(module_base + 0x030),
		0x034, INREG32(module_base + 0x034));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x038, INREG32(module_base + 0x038),
		0x03c, INREG32(module_base + 0x03c),
		0x040, INREG32(module_base + 0x040),
		0x044, INREG32(module_base + 0x044));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x068, INREG32(module_base + 0x068),
		0x070, INREG32(module_base + 0x070),
		0x078, INREG32(module_base + 0x078),
		0x100, INREG32(module_base + 0x100));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x104, INREG32(module_base + 0x104),
		0x108, INREG32(module_base + 0x108),
		0x10c, INREG32(module_base + 0x10c),
		0x110, INREG32(module_base + 0x110));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x114, INREG32(module_base + 0x114),
		0x118, INREG32(module_base + 0x118),
		0x11c, INREG32(module_base + 0x11c),
		0x120, INREG32(module_base + 0x120));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x124, INREG32(module_base + 0x124),
		0x128, INREG32(module_base + 0x128),
		0x130, INREG32(module_base + 0x130),
		0x140, INREG32(module_base + 0x140));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x144, INREG32(module_base + 0x144),
		0x148, INREG32(module_base + 0x148),
		0x14c, INREG32(module_base + 0x14c),
		0x150, INREG32(module_base + 0x150));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x154, INREG32(module_base + 0x154),
		0x158, INREG32(module_base + 0x158),
		0x15c, INREG32(module_base + 0x15c),
		0x160, INREG32(module_base + 0x160));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x164, INREG32(module_base + 0x164),
		0x180, INREG32(module_base + 0x180),
		0x184, INREG32(module_base + 0x184),
		0x188, INREG32(module_base + 0x188));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x18c, INREG32(module_base + 0x18c),
		0x190, INREG32(module_base + 0x190),
		0x194, INREG32(module_base + 0x194),
		0x198, INREG32(module_base + 0x198));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x19c, INREG32(module_base + 0x19c),
		0x1c0, INREG32(module_base + 0x1c0),
		0x1c4, INREG32(module_base + 0x1c4),
		0x200, INREG32(module_base + 0x200));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x204, INREG32(module_base + 0x204),
		0x208, INREG32(module_base + 0x208),
		0x20c, INREG32(module_base + 0x20c),
		0x210, INREG32(module_base + 0x210));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x214, INREG32(module_base + 0x214),
		0x218, INREG32(module_base + 0x218),
		0x21c, INREG32(module_base + 0x21c),
		0x220, INREG32(module_base + 0x220));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x224, INREG32(module_base + 0x224),
		0x228, INREG32(module_base + 0x228),
		0x230, INREG32(module_base + 0x230),
		0x240, INREG32(module_base + 0x240));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x244, INREG32(module_base + 0x244),
		0x248, INREG32(module_base + 0x248),
		0x24c, INREG32(module_base + 0x24c),
		0x250, INREG32(module_base + 0x250));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x254, INREG32(module_base + 0x254),
		0x258, INREG32(module_base + 0x258),
		0x25c, INREG32(module_base + 0x25c),
		0x260, INREG32(module_base + 0x260));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x264, INREG32(module_base + 0x264),
		0x280, INREG32(module_base + 0x280),
		0x284, INREG32(module_base + 0x284),
		0x288, INREG32(module_base + 0x288));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x28c, INREG32(module_base + 0x28c),
		0x290, INREG32(module_base + 0x290),
		0x294, INREG32(module_base + 0x294),
		0x298, INREG32(module_base + 0x298));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x29c, INREG32(module_base + 0x29c),
		0x2c0, INREG32(module_base + 0x2c0),
		0x2c4, INREG32(module_base + 0x2c4),
		0x300, INREG32(module_base + 0x300));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x304, INREG32(module_base + 0x304),
		0x308, INREG32(module_base + 0x308),
		0x30c, INREG32(module_base + 0x30c),
		0x310, INREG32(module_base + 0x310));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x314, INREG32(module_base + 0x314),
		0x318, INREG32(module_base + 0x318),
		0x31c, INREG32(module_base + 0x31c),
		0x320, INREG32(module_base + 0x320));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x324, INREG32(module_base + 0x324),
		0x328, INREG32(module_base + 0x328),
		0x330, INREG32(module_base + 0x330),
		0x340, INREG32(module_base + 0x340));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x344, INREG32(module_base + 0x344),
		0x348, INREG32(module_base + 0x348),
		0x34c, INREG32(module_base + 0x34c),
		0x350, INREG32(module_base + 0x350));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x354, INREG32(module_base + 0x354),
		0x358, INREG32(module_base + 0x358),
		0x35c, INREG32(module_base + 0x35c),
		0x360, INREG32(module_base + 0x360));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x364, INREG32(module_base + 0x364),
		0x380, INREG32(module_base + 0x380),
		0x384, INREG32(module_base + 0x384),
		0x388, INREG32(module_base + 0x388));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x38c, INREG32(module_base + 0x38c),
		0x390, INREG32(module_base + 0x390),
		0x394, INREG32(module_base + 0x394),
		0x398, INREG32(module_base + 0x398));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x39c, INREG32(module_base + 0x39c),
		0x3c0, INREG32(module_base + 0x3c0),
		0x3c4, INREG32(module_base + 0x3c4),
		0x400, INREG32(module_base + 0x400));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x404, INREG32(module_base + 0x404),
		0x408, INREG32(module_base + 0x408),
		0x40c, INREG32(module_base + 0x40c),
		0x410, INREG32(module_base + 0x410));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x414, INREG32(module_base + 0x414),
		0x418, INREG32(module_base + 0x418),
		0x41c, INREG32(module_base + 0x41c),
		0x420, INREG32(module_base + 0x420));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x424, INREG32(module_base + 0x424),
		0x428, INREG32(module_base + 0x428),
		0x430, INREG32(module_base + 0x430),
		0x440, INREG32(module_base + 0x440));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x444, INREG32(module_base + 0x444),
		0x448, INREG32(module_base + 0x448),
		0x44c, INREG32(module_base + 0x44c),
		0x450, INREG32(module_base + 0x450));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x454, INREG32(module_base + 0x454),
		0x458, INREG32(module_base + 0x458),
		0x45c, INREG32(module_base + 0x45c),
		0x460, INREG32(module_base + 0x460));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x464, INREG32(module_base + 0x464),
		0x480, INREG32(module_base + 0x480),
		0x484, INREG32(module_base + 0x484),
		0x488, INREG32(module_base + 0x488));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x48c, INREG32(module_base + 0x48c),
		0x490, INREG32(module_base + 0x490),
		0x494, INREG32(module_base + 0x494),
		0x498, INREG32(module_base + 0x498));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x49c, INREG32(module_base + 0x49c),
		0x4c0, INREG32(module_base + 0x4c0),
		0x4c4, INREG32(module_base + 0x4c4),
		0x500, INREG32(module_base + 0x500));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x504, INREG32(module_base + 0x504),
		0x508, INREG32(module_base + 0x508),
		0x50c, INREG32(module_base + 0x50c),
		0x510, INREG32(module_base + 0x510));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x514, INREG32(module_base + 0x514),
		0x518, INREG32(module_base + 0x518),
		0x51c, INREG32(module_base + 0x51c),
		0x520, INREG32(module_base + 0x520));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x524, INREG32(module_base + 0x524),
		0x528, INREG32(module_base + 0x528),
		0x530, INREG32(module_base + 0x530),
		0x540, INREG32(module_base + 0x540));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x544, INREG32(module_base + 0x544),
		0x548, INREG32(module_base + 0x548),
		0x54c, INREG32(module_base + 0x54c),
		0x550, INREG32(module_base + 0x550));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x554, INREG32(module_base + 0x554),
		0x558, INREG32(module_base + 0x558),
		0x55c, INREG32(module_base + 0x55c),
		0x560, INREG32(module_base + 0x560));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x564, INREG32(module_base + 0x564),
		0x580, INREG32(module_base + 0x580),
		0x584, INREG32(module_base + 0x584),
		0x588, INREG32(module_base + 0x588));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x58c, INREG32(module_base + 0x58c),
		0x590, INREG32(module_base + 0x590),
		0x594, INREG32(module_base + 0x594),
		0x598, INREG32(module_base + 0x598));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x59c, INREG32(module_base + 0x59c),
		0x5c0, INREG32(module_base + 0x5c0),
		0x5c4, INREG32(module_base + 0x5c4),
		0x600, INREG32(module_base + 0x600));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x604, INREG32(module_base + 0x604),
		0x608, INREG32(module_base + 0x608),
		0x60c, INREG32(module_base + 0x60c),
		0x610, INREG32(module_base + 0x610));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x614, INREG32(module_base + 0x614),
		0x618, INREG32(module_base + 0x618),
		0x61c, INREG32(module_base + 0x61c),
		0x620, INREG32(module_base + 0x620));
	DDPDUMP("mipi_tx: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x624, INREG32(module_base + 0x624),
		0x628, INREG32(module_base + 0x628),
		0x62c, INREG32(module_base + 0x62c),
		0x630, INREG32(module_base + 0x630));
	DDPDUMP("mipi_tx:\n");

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

void mmsys_config_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = ddp_get_module_va(module);

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x00c, INREG32(module_base + 0x00c),
		0x010, INREG32(module_base + 0x010));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x014, INREG32(module_base + 0x014),
		0x018, INREG32(module_base + 0x018),
		0x020, INREG32(module_base + 0x020),
		0x024, INREG32(module_base + 0x024));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x028, INREG32(module_base + 0x028),
		0x034, INREG32(module_base + 0x034),
		0x038, INREG32(module_base + 0x038),
		0x0f0, INREG32(module_base + 0x0f0));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0f8, INREG32(module_base + 0x0f8),
		0x100, INREG32(module_base + 0x100),
		0x104, INREG32(module_base + 0x104),
		0x108, INREG32(module_base + 0x108));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x110, INREG32(module_base + 0x110),
		0x114, INREG32(module_base + 0x114),
		0x118, INREG32(module_base + 0x118),
		0x120, INREG32(module_base + 0x120));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x124, INREG32(module_base + 0x124),
		0x128, INREG32(module_base + 0x128),
		0x130, INREG32(module_base + 0x130),
		0x134, INREG32(module_base + 0x134));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x138, INREG32(module_base + 0x138),
		0x140, INREG32(module_base + 0x140),
		0x144, INREG32(module_base + 0x144),
		0x150, INREG32(module_base + 0x150));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x190, INREG32(module_base + 0x190),
		0x200, INREG32(module_base + 0x200),
		0x204, INREG32(module_base + 0x204),
		0x208, INREG32(module_base + 0x208));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x20c, INREG32(module_base + 0x20c),
		0x210, INREG32(module_base + 0x210),
		0x214, INREG32(module_base + 0x214),
		0x218, INREG32(module_base + 0x218));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x800, INREG32(module_base + 0x800),
		0x804, INREG32(module_base + 0x804),
		0x808, INREG32(module_base + 0x808),
		0x80c, INREG32(module_base + 0x80c));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x810, INREG32(module_base + 0x810),
		0x814, INREG32(module_base + 0x814),
		0x818, INREG32(module_base + 0x818),
		0x81c, INREG32(module_base + 0x81c));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x820, INREG32(module_base + 0x820),
		0x824, INREG32(module_base + 0x824),
		0x828, INREG32(module_base + 0x828),
		0x82c, INREG32(module_base + 0x82c));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x830, INREG32(module_base + 0x830),
		0x834, INREG32(module_base + 0x834),
		0x838, INREG32(module_base + 0x838),
		0x83c, INREG32(module_base + 0x83c));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x840, INREG32(module_base + 0x840),
		0x848, INREG32(module_base + 0x848),
		0x84c, INREG32(module_base + 0x84c),
		0x854, INREG32(module_base + 0x854));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x858, INREG32(module_base + 0x858),
		0x85c, INREG32(module_base + 0x85c),
		0x864, INREG32(module_base + 0x864),
		0x868, INREG32(module_base + 0x868));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x870, INREG32(module_base + 0x870),
		0x874, INREG32(module_base + 0x874),
		0x88c, INREG32(module_base + 0x88c),
		0x890, INREG32(module_base + 0x890));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x894, INREG32(module_base + 0x894),
		0x898, INREG32(module_base + 0x898),
		0x89c, INREG32(module_base + 0x89c),
		0x8a0, INREG32(module_base + 0x8a0));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x8a4, INREG32(module_base + 0x8a4),
		0x8a8, INREG32(module_base + 0x8a8),
		0x8ac, INREG32(module_base + 0x8ac),
		0x8b0, INREG32(module_base + 0x8b0));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x8dc, INREG32(module_base + 0x8dc),
		0x8f0, INREG32(module_base + 0x8f0),
		0xf00, INREG32(module_base + 0xf00),
		0xf04, INREG32(module_base + 0xf04));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xf08, INREG32(module_base + 0xf08),
		0xf0c, INREG32(module_base + 0xf0c),
		0xf10, INREG32(module_base + 0xf10),
		0xf14, INREG32(module_base + 0xf14));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xf18, INREG32(module_base + 0xf18),
		0xf1c, INREG32(module_base + 0xf1c),
		0xf20, INREG32(module_base + 0xf20),
		0xf24, INREG32(module_base + 0xf24));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xf28, INREG32(module_base + 0xf28),
		0xf2c, INREG32(module_base + 0xf2c),
		0xf30, INREG32(module_base + 0xf30),
		0xf34, INREG32(module_base + 0xf34));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xf38, INREG32(module_base + 0xf38),
		0xf3c, INREG32(module_base + 0xf3c),
		0xf40, INREG32(module_base + 0xf40),
		0xf44, INREG32(module_base + 0xf44));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xf48, INREG32(module_base + 0xf48),
		0xf4c, INREG32(module_base + 0xf4c),
		0xf50, INREG32(module_base + 0xf50),
		0xf54, INREG32(module_base + 0xf54));
	DDPDUMP("mmsys: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xf58, INREG32(module_base + 0xf58),
		0xf5c, INREG32(module_base + 0xf5c),
		0xf60, INREG32(module_base + 0xf60),
		0xf64, INREG32(module_base + 0xf64));
	DDPDUMP("mmsys:\n");

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}


void disp_split_dump_regs(enum DISP_MODULE_ENUM module)
{
	DDPDUMP("No support the mdoule: %s\n", __func__);
}

/*************************Module reg dump end *********************/


int ddp_dump_reg(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_WDMA0:
		disp_wdma_dump_reg(module);
		break;
	case DISP_MODULE_RDMA0:
	case DISP_MODULE_RDMA1:
		disp_rdma_dump_reg(module);
		break;
	case DISP_MODULE_OVL0:
	case DISP_MODULE_OVL0_2L:
	case DISP_MODULE_OVL1_2L:
		disp_ovl_dump_reg(module);
		break;

	case DISP_MODULE_CONFIG:
		mmsys_config_dump_reg(module);
		break;
	case DISP_MODULE_MUTEX:
		disp_mutex_dump_reg(module);
		break;
	case DISP_MODULE_SPLIT0:
		disp_split_dump_regs(module);
		break;
	case DISP_MODULE_DSI0:
		disp_dsi_dump_reg(module);
#ifndef CONFIG_FPGA_EARLY_PORTING
		mipi_tx_config_dump_reg(DISP_MODULE_MIPI0);
#endif
		break;
	case DISP_MODULE_DSI1:
		disp_dsi_dump_reg(module);
#ifndef CONFIG_FPGA_EARLY_PORTING
		mipi_tx_config_dump_reg(DISP_MODULE_MIPI1);
#endif
		break;
	case DISP_MODULE_DSIDUAL:
		disp_dsi_dump_reg(DISP_MODULE_DSI0);
		disp_dsi_dump_reg(DISP_MODULE_DSI1);
#ifndef CONFIG_FPGA_EARLY_PORTING
		mipi_tx_config_dump_reg(DISP_MODULE_MIPI0);
		mipi_tx_config_dump_reg(DISP_MODULE_MIPI1);
#endif
		break;

	case DISP_MODULE_PWM0:
		if (disp_helper_get_option(DISP_OPT_REG_PARSER_RAW_DUMP))
			disp_pwm_dump_reg(module);
		break;
/******PQ start****/
	case DISP_MODULE_GAMMA0:
		if (disp_helper_get_option(DISP_OPT_REG_PARSER_RAW_DUMP))
			disp_gamma_dump_reg(module);
		break;
	case DISP_MODULE_COLOR0:
		if (disp_helper_get_option(DISP_OPT_REG_PARSER_RAW_DUMP))
			disp_color_dump_reg(module);
		break;
	case DISP_MODULE_AAL0:
		if (disp_helper_get_option(DISP_OPT_REG_PARSER_RAW_DUMP))
			disp_aal_dump_reg(module);
		break;
	case DISP_MODULE_CCORR0:
		if (disp_helper_get_option(DISP_OPT_REG_PARSER_RAW_DUMP))
			disp_ccorr_dump_reg(module);
		break;
	case DISP_MODULE_DITHER0:
		if (disp_helper_get_option(DISP_OPT_REG_PARSER_RAW_DUMP))
			disp_dither_dump_reg(module);
		break;
/******PQ end****/
	default:
		DDPDUMP("no dump_reg for module %s(%d)\n", ddp_get_module_name(module), module);
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
	case DISP_MODULE_SPLIT0:
		split_dump_analysis(DISP_MODULE_SPLIT0);
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
	default:
		DDPDUMP("no dump_analysis for module %s(%d)\n", ddp_get_module_name(module),
			module);
	}
	return 0;
}
