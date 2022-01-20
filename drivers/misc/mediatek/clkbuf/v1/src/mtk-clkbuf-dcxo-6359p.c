// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */

#include <linux/mfd/syscon.h>

#include "mtk-clkbuf-dcxo.h"
#include "mtk-clkbuf-dcxo-6359p.h"
#include "mtk_clkbuf_common.h"

#define MT6359P_SET_REG_BY_NAME(reg, name)			\
	SET_REG_BY_NAME(reg, MT6359P_ ## name)

#define XO_NUM				7

static const char * const valid_cmd_type[] = {
	"ON",
	"OFF",
	"EN_BB",
	"SIG",
	"CO_BUF",
	"INIT",
	NULL,
};

struct mt6359p_ldo_regs {
	struct reg_t _vrfck_en;
	struct reg_t _vrfck_op_en14;
	struct reg_t _vbbck_en;
	struct reg_t _vbbck_op_en14;
	struct reg_t _vrfck_hv_en;
	struct reg_t _vrfck_ana_sel;
	struct reg_t _vrfck_ndis_en;
	struct reg_t _vrfck_1_ndis_en;
};

struct mt6359p_debug_regs {
	struct reg_t _dcxo_cw00;
	struct reg_t _dcxo_cw08;
	struct reg_t _dcxo_cw09;
	struct reg_t _dcxo_cw12;
	struct reg_t _dcxo_cw13;
	struct reg_t _dcxo_cw19;
};

static struct mt6359p_ldo_regs ldo_reg = {
	MT6359P_SET_REG_BY_NAME(vrfck_en, RG_LDO_VRFCK_EN)
	MT6359P_SET_REG_BY_NAME(vrfck_op_en14, RG_LDO_VRFCK_HW14_OP_EN)
	MT6359P_SET_REG_BY_NAME(vbbck_en, RG_LDO_VBBCK_EN)
	MT6359P_SET_REG_BY_NAME(vbbck_op_en14, RG_LDO_VBBCK_HW14_OP_EN)
	MT6359P_SET_REG_BY_NAME(vrfck_hv_en, RG_VRFCK_HV_EN)
	MT6359P_SET_REG_BY_NAME(vrfck_ana_sel, RG_LDO_VRFCK_ANA_SEL)
	MT6359P_SET_REG_BY_NAME(vrfck_ndis_en, RG_VRFCK_NDIS_EN)
	MT6359P_SET_REG_BY_NAME(vrfck_1_ndis_en, RG_VRFCK_1_NDIS_EN)
};

static struct mt6359p_debug_regs debug_reg = {
	SET_REG(dcxo_cw00, MT6359P_DCXO_CW00, 0xFFFF, 0)
	SET_REG(dcxo_cw08, MT6359P_DCXO_CW08, 0xFFFF, 0)
	SET_REG(dcxo_cw09, MT6359P_DCXO_CW09, 0xFFFF, 0)
	SET_REG(dcxo_cw12, MT6359P_DCXO_CW12, 0xFFFF, 0)
	SET_REG(dcxo_cw13, MT6359P_DCXO_CW13, 0xFFFF, 0)
	SET_REG(dcxo_cw19, MT6359P_DCXO_CW19, 0xFFFF, 0)
};

static int mt6359p_dcxo_dump_reg_log(char *buf);
static int mt6359p_dcxo_dump_misc_log(char *buf);
static int mt6359p_dcxo_misc_store(const char *obj, const char *arg);

static struct xo_buf_t xo_bufs[XO_NUM] = {
	[0] = {
		MT6359P_SET_REG_BY_NAME(xo_mode, XO_EXTBUF1_MODE)
		MT6359P_SET_REG_BY_NAME(xo_en, XO_EXTBUF1_EN_M)
		SET_REG(xo_en_auxout, MT6359P_XO_STATIC_AUXOUT_ADDR, 0x1, 13)
		MT6359P_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF1_RSEL)
		MT6359P_SET_REG_BY_NAME(rc_voter, XO_SOC_VOTE)
		MT6359P_SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF1_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
	[1] = {
		MT6359P_SET_REG_BY_NAME(xo_mode, XO_EXTBUF2_MODE)
		MT6359P_SET_REG_BY_NAME(xo_en, XO_EXTBUF2_EN_M)
		SET_REG(xo_en_auxout, MT6359P_XO_STATIC_AUXOUT_ADDR, 0x1, 11)
		MT6359P_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF2_RSEL)
		MT6359P_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF2_SRSEL)
		MT6359P_SET_REG_BY_NAME(rc_voter, XO_WCN_VOTE)
		MT6359P_SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF2_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
	[2] = {
		MT6359P_SET_REG_BY_NAME(xo_mode, XO_EXTBUF3_MODE)
		MT6359P_SET_REG_BY_NAME(xo_en, XO_EXTBUF3_EN_M)
		SET_REG(xo_en_auxout, MT6359P_XO_STATIC_AUXOUT_ADDR, 0x1, 9)
		MT6359P_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF3_RSEL)
		MT6359P_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF3_HD)
		MT6359P_SET_REG_BY_NAME(rc_voter, XO_NFC_VOTE)
		MT6359P_SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF3_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
	[3] = {
		MT6359P_SET_REG_BY_NAME(xo_mode, XO_EXTBUF4_MODE)
		MT6359P_SET_REG_BY_NAME(xo_en, XO_EXTBUF4_EN_M)
		SET_REG(xo_en_auxout, MT6359P_XO_STATIC_AUXOUT_ADDR, 0x1, 7)
		MT6359P_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF4_RSEL)
		MT6359P_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF4_SRSEL)
		MT6359P_SET_REG_BY_NAME(rc_voter, XO_CEL_VOTE)
		MT6359P_SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF4_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
	[6] = {
		MT6359P_SET_REG_BY_NAME(xo_mode, XO_EXTBUF7_MODE)
		MT6359P_SET_REG_BY_NAME(xo_en, XO_EXTBUF7_EN_M)
		SET_REG(xo_en_auxout, MT6359P_XO_STATIC_AUXOUT_ADDR, 0x1, 3)
		MT6359P_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF7_RSEL)
		MT6359P_SET_REG_BY_NAME(rc_voter, XO_EXT_VOTE)
		MT6359P_SET_REG_BY_NAME(hwbblpm_msk, XO_EXTBUF7_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 6,
		.in_use = 1,
	},
};

struct dcxo_hw mt6359p_dcxo = {
	.hw = {
		.is_pmic = true,
	},
	.xo_num = XO_NUM,
	.bblpm_auxout_sel = 27,
	.xo_bufs = xo_bufs,
	MT6359P_SET_REG_BY_NAME(static_aux_sel, XO_STATIC_AUXOUT_SEL)
	SET_REG(bblpm_auxout, MT6359P_XO_STATIC_AUXOUT_ADDR, 0x1, 0)
	MT6359P_SET_REG_BY_NAME(swbblpm_en, XO_BB_LPM_EN_M)
	MT6359P_SET_REG_BY_NAME(hwbblpm_sel, XO_BB_LPM_EN_SEL)
	MT6359P_SET_REG_BY_NAME(srclken_i3, RG_SRCLKEN_IN3_EN)
	MT6359P_SET_REG_BY_NAME(dcxo_pmrc_en, PMRC_EN)
	.ops = {
		.dcxo_dump_reg_log = mt6359p_dcxo_dump_reg_log,
		.dcxo_dump_misc_log = mt6359p_dcxo_dump_misc_log,
		.dcxo_misc_store = mt6359p_dcxo_misc_store,
	},
	.valid_dcxo_cmd = valid_cmd_type,
};

static int mt6359p_dcxo_dump_reg_log(char *buf)
{
	u32 val = 0;
	int len = 0;

	if (clk_buf_read(&mt6359p_dcxo.hw, &debug_reg._dcxo_cw00, &val))
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len, "DCXO_CW00=0x%x, ", val);

	if (clk_buf_read(&mt6359p_dcxo.hw, &debug_reg._dcxo_cw08, &val))
		goto DUMP_REG_LOG_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len, "DCOX_CW08=0x%x, ", val);
	if (clk_buf_read(&mt6359p_dcxo.hw, &debug_reg._dcxo_cw09, &val))
		goto DUMP_REG_LOG_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len, "DCXO_CW09=0x%x, ", val);

	if (clk_buf_read(&mt6359p_dcxo.hw, &debug_reg._dcxo_cw12, &val))
		goto DUMP_REG_LOG_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len, "DCXO_CW12=0x%x, ", val);

	if (clk_buf_read(&mt6359p_dcxo.hw, &debug_reg._dcxo_cw13, &val))
		goto DUMP_REG_LOG_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len, "DCXO_CW13=0x%x, ", val);

	if (clk_buf_read(&mt6359p_dcxo.hw, &debug_reg._dcxo_cw19, &val))
		goto DUMP_REG_LOG_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len, "DCXO_CW19=0x%x\n", val);

	return len;
DUMP_REG_LOG_FAILED:
	len -= 2;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

static int mt6359p_dcxo_dump_misc_log(char *buf)
{
	u32 val = 0;
	int len = 0;
	int ret = 0;

	ret = clk_buf_read(&mt6359p_dcxo.hw, &mt6359p_dcxo._srclken_i3, &val);
	if (ret)
		return len;

	len += snprintf(buf + len, PAGE_SIZE - len, "srclken_conn=0x%x\n", val);

	ret = clk_buf_read(&mt6359p_dcxo.hw, &ldo_reg._vrfck_en, &val);
	if (ret)
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vrfck_en=0x%x, ", val);

	if (clk_buf_read(&mt6359p_dcxo.hw, &ldo_reg._vrfck_op_en14, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vrfck_op_mode= 0x%x, ", val);

	if (clk_buf_read(&mt6359p_dcxo.hw, &ldo_reg._vbbck_en, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vbbck_en=0x%x, ", val);

	if (clk_buf_read(&mt6359p_dcxo.hw, &ldo_reg._vbbck_op_en14, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vbbck_op_mode=0x%x, ", val);

	if (clk_buf_read(&mt6359p_dcxo.hw, &ldo_reg._vrfck_hv_en, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vrfck_hv_en=0x%x, ", val);

	if (clk_buf_read(&mt6359p_dcxo.hw, &ldo_reg._vrfck_ana_sel, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vrfck_ana_sel=0x%x, ", val);

	if (clk_buf_read(&mt6359p_dcxo.hw, &ldo_reg._vrfck_ndis_en, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vrfck_ndis_en=0x%x, ", val);

	if (clk_buf_read(&mt6359p_dcxo.hw, &ldo_reg._vrfck_1_ndis_en, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vrfck_1_ndis_en=0x%x\n", val);

	return len;

DUMP_MISC_FAILED:
	len -= 2;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

static int mt6359p_dcxo_misc_store(const char *obj, const char *arg)
{
	short no_lock = 0;
	u32 cmd = 0;
	int ret = 0;

	if (!strcmp(arg, "ON"))
		cmd = 1;
	else if (!strcmp(arg, "OFF"))
		cmd = 0;
	else
		return -EPERM;

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&mt6359p_dcxo.lock);

	if (!strcmp(obj, "VRFCK_EN"))
		ret = clk_buf_write(&mt6359p_dcxo.hw, &ldo_reg._vrfck_en, cmd);
	else if (!strcmp(obj, "VRFCK_OP_MODE"))
		ret = clk_buf_write(&mt6359p_dcxo.hw,
				&ldo_reg._vrfck_op_en14,
				cmd);
	else if (!strcmp(obj, "VBBCK_EN"))
		ret = clk_buf_write(&mt6359p_dcxo.hw, &ldo_reg._vbbck_en, cmd);
	else if (!strcmp(obj, "VBBCK_OP_MODE"))
		ret = clk_buf_write(&mt6359p_dcxo.hw,
				&ldo_reg._vbbck_op_en14,
				cmd);
	else if (!strcmp(obj, "VRFCK_HV_EN"))
		ret = clk_buf_write(&mt6359p_dcxo.hw,
				&ldo_reg._vrfck_hv_en,
				cmd);
	else if (!strcmp(obj, "VRFCK_ANA_SEL"))
		ret = clk_buf_write(&mt6359p_dcxo.hw,
				&ldo_reg._vrfck_ana_sel,
				cmd);
	else if (!strcmp(obj, "VRFCK_NDIS_EN"))
		ret = clk_buf_write(&mt6359p_dcxo.hw,
				&ldo_reg._vrfck_ndis_en,
				cmd);
	else if (!strcmp(obj, "VRFCK_1_NDIS_EN"))
		ret = clk_buf_write(&mt6359p_dcxo.hw,
				&ldo_reg._vrfck_1_ndis_en,
				cmd);
	else
		ret = -EPERM;

	if (!no_lock)
		mutex_unlock(&mt6359p_dcxo.lock);

	return ret;
}
