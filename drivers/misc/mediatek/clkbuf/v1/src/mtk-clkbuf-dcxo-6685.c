// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */

#include <linux/mfd/syscon.h>

#include "mtk-clkbuf-dcxo.h"
#include "mtk-clkbuf-dcxo-6685.h"
#include "mtk_clkbuf_common.h"

#define MT6685_SET_REG_BY_NAME(reg, name)			\
	SET_REG_BY_NAME(reg, MT6685_ ## name)

#define XO_NUM				13

static const char * const valid_cmd_type[] = {
	"ON",
	"OFF",
	"EN_BB",
	"SIG",
	"INIT",
	NULL,
};

struct mt6685_ldo_regs {
	struct reg_t _vrfck1_en;
	struct reg_t _vrfck1_op_en14;
	struct reg_t _vrfck2_en;
	struct reg_t _vrfck2_op_en14;
	struct reg_t _vbbck_en;
	struct reg_t _vbbck_op_en14;
};

struct mt6685_debug_regs {
	struct reg_t _xo_pmic_top_dig_sw;
	struct reg_t _dcxo_manual_cw1;
	struct reg_t _dcxo_bblpm_cw0;
	struct reg_t _dcxo_bblpm_cw1;
	struct reg_t _dcxo_buf_cw0;
	struct reg_t _dcxo_dig26m_div2;
	struct reg_t _xo_clksel_man;
	struct reg_t _xo_clksel_en_m;
};

static struct mt6685_ldo_regs ldo_reg = {
	MT6685_SET_REG_BY_NAME(vrfck1_en, RG_LDO_VRFCK1_EN)
	MT6685_SET_REG_BY_NAME(vrfck1_op_en14, RG_LDO_VRFCK1_HW14_OP_EN)
	MT6685_SET_REG_BY_NAME(vrfck2_en, RG_LDO_VRFCK2_EN)
	MT6685_SET_REG_BY_NAME(vrfck2_op_en14, RG_LDO_VRFCK2_HW14_OP_EN)
	MT6685_SET_REG_BY_NAME(vbbck_en, RG_LDO_VBBCK_EN)
	MT6685_SET_REG_BY_NAME(vbbck_op_en14, RG_LDO_VBBCK_HW14_OP_EN)
};

static struct mt6685_debug_regs debug_reg = {
	MT6685_SET_REG_BY_NAME(xo_pmic_top_dig_sw, XO_PMIC_TOP_DIG_SW)
	SET_REG(dcxo_manual_cw1, MT6685_DCXO_DIG_MANCTRL_CW1, 0xFF, 0)
	SET_REG(dcxo_bblpm_cw0, MT6685_DCXO_BBLPM_CW0, 0xFF, 0)
	SET_REG(dcxo_bblpm_cw1, MT6685_DCXO_BBLPM_CW1, 0xFF, 0)
	SET_REG(dcxo_buf_cw0, MT6685_DCXO_EXTBUF1_CW0, 0xFF, 0)
	MT6685_SET_REG_BY_NAME(dcxo_dig26m_div2, RG_XO_DIG26M_DIV2)
	MT6685_SET_REG_BY_NAME(xo_clksel_man, XO_CLKSEL_MAN)
	MT6685_SET_REG_BY_NAME(xo_clksel_en_m, XO_CLKSEL_EN_M)
};

static int mt6685_dcxo_dump_reg_log(char *buf);
static int mt6685_dcxo_dump_misc_log(char *buf);
static int mt6685_dcxo_misc_store(const char *obj, const char *arg);

static struct xo_buf_t xo_bufs[XO_NUM] = {
	[0] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_BBCK1_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_BBCK1_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_H_ADDR, 0x1, 7)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_BBCK1_RSEL)
		MT6685_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF_BBCK1_HD)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_BBCK1_VOTE_L)
		MT6685_SET_REG_BY_NAME(hwbblpm_msk, XO_BBCK1_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[1] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_BBCK2_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_BBCK2_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_H_ADDR, 0x1, 6)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_BBCK2_RSEL)
		MT6685_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF_BBCK2_HD)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_BBCK2_VOTE_L)
		MT6685_SET_REG_BY_NAME(hwbblpm_msk, XO_BBCK2_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[2] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_BBCK3_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_BBCK3_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_H_ADDR, 0x1, 5)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_BBCK3_RSEL)
		MT6685_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF_BBCK3_HD)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_BBCK3_VOTE_L)
		MT6685_SET_REG_BY_NAME(hwbblpm_msk, XO_BBCK3_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[3] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_BBCK4_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_BBCK4_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_H_ADDR, 0x1, 4)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_BBCK4_RSEL)
		MT6685_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF_BBCK4_HD)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_BBCK4_VOTE_L)
		MT6685_SET_REG_BY_NAME(hwbblpm_msk, XO_BBCK4_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[4] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_BBCK5_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_BBCK5_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_H_ADDR, 0x1, 3)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_BBCK5_RSEL)
		MT6685_SET_REG_BY_NAME(de_sense, RG_XO_EXTBUF_BBCK5_HD)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_BBCK5_VOTE_L)
		MT6685_SET_REG_BY_NAME(hwbblpm_msk, XO_BBCK5_BBLPM_EN_MASK)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[5] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_RFCK1A_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_RFCK1A_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_H_ADDR, 0x1, 2)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_RFCK1A_RSEL)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_RFCK1A_VOTE_L)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[6] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_RFCK1B_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_RFCK1B_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_H_ADDR, 0x1, 1)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_RFCK1B_RSEL)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_RFCK1B_VOTE_L)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[7] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_RFCK1C_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_RFCK1C_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_H_ADDR, 0x1, 0)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_RFCK1C_RSEL)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_RFCK1C_VOTE_L)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[8] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_RFCK2A_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_RFCK2A_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_L_ADDR, 0x1, 7)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_RFCK2A_RSEL)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_RFCK2A_VOTE_L)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[9] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_RFCK2B_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_RFCK2B_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_L_ADDR, 0x1, 6)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_RFCK2B_RSEL)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_RFCK2B_VOTE_L)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[10] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_RFCK2C_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_RFCK2C_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_L_ADDR, 0x1, 5)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_RFCK2C_RSEL)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_RFCK2C_VOTE_L)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[11] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_CONCK1_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_CONCK1_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_L_ADDR, 0x1, 4)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_CONCK1_RSEL)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_CONCK1_VOTE_L)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
	[12] = {
		MT6685_SET_REG_BY_NAME(xo_mode, XO_CONCK2_MODE)
		MT6685_SET_REG_BY_NAME(xo_en, XO_CONCK2_EN_M)
		SET_REG(xo_en_auxout, MT6685_XO_STATIC_AUXOUT_L_ADDR, 0x1, 3)
		MT6685_SET_REG_BY_NAME(impedance, RG_XO_EXTBUF_CONCK2_RSEL)
		MT6685_SET_REG_BY_NAME(rc_voter, XO_CONCK2_VOTE_L)
		.xo_en_auxout_sel = 15,
		.in_use = 1,
	},
};

struct dcxo_hw mt6685_dcxo = {
	.hw = {
		.is_pmic = true,
	},
	.xo_num = XO_NUM,
	.bblpm_auxout_sel = 79,
	.xo_bufs = xo_bufs,
	MT6685_SET_REG_BY_NAME(static_aux_sel, XO_STATIC_AUXOUT_SEL)
	SET_REG(bblpm_auxout, MT6685_XO_STATIC_AUXOUT_H_ADDR, 0x1, 7)
	MT6685_SET_REG_BY_NAME(swbblpm_en, XO_BB_LPM_EN_M)
	MT6685_SET_REG_BY_NAME(hwbblpm_sel, XO_BB_LPM_EN_SEL)
	MT6685_SET_REG_BY_NAME(dcxo_pmrc_en, PMRC_EN0)
	MT6685_SET_REG_BY_NAME(xo_cdac_fpm, RG_XO_CDAC_FPM)
	MT6685_SET_REG_BY_NAME(xo_aac_fpm_swen, RG_XO_AAC_FPM_SWEN)
	MT6685_SET_REG_BY_NAME(xo_heater_sel, RG_XO_HEATER_SEL)
	.ops = {
		.dcxo_dump_reg_log = mt6685_dcxo_dump_reg_log,
		.dcxo_dump_misc_log = mt6685_dcxo_dump_misc_log,
		.dcxo_misc_store = mt6685_dcxo_misc_store,
	},
	.valid_dcxo_cmd = valid_cmd_type,
};

static int mt6685_dcxo_dump_reg_log(char *buf)
{
	u32 val = 0;
	int len = 0;
	uint8_t i = 0;

	if (clk_buf_read(&mt6685_dcxo.hw, &debug_reg._dcxo_manual_cw1, &val))
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len,
		"DCXO_MANUAL_CW1=0x%x, ", val);

	if (clk_buf_read(&mt6685_dcxo.hw, &debug_reg._dcxo_bblpm_cw0, &val))
		goto DUMP_REG_LOG_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
		"DCXO_BBLPM_CW0=0x%x, ", val);

	if (clk_buf_read(&mt6685_dcxo.hw, &debug_reg._dcxo_bblpm_cw1, &val))
		goto DUMP_REG_LOG_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
		"DCXO_BBLPM_CW1=0x%x\n", val);

	len += snprintf(buf + len, PAGE_SIZE - len,
		"DCXO_BUF0_CW0 ~ DCXO_BUF%u_CW0=", XO_NUM);
	for (i = 0; i < XO_NUM; i++) {
		if (clk_buf_read_with_ofs(
				&mt6685_dcxo.hw,
				&debug_reg._dcxo_buf_cw0,
				&val,
				i))
			goto DUMP_REG_LOG_FAILED;
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%x/", val);
	}
	len -= 1;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	if (clk_buf_read(&mt6685_dcxo.hw, &debug_reg._dcxo_dig26m_div2, &val))
		return len;

	len += snprintf(buf + len, PAGE_SIZE - len,
		"DCXO%uM\n", val ? 26 : 52);

	return len;
DUMP_REG_LOG_FAILED:
	len -= 2;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

static int mt6685_dcxo_dump_misc_log(char *buf)
{
	u32 val = 0;
	int len = 0;
	int ret = 0;

	ret = clk_buf_read(&mt6685_dcxo.hw, &ldo_reg._vrfck1_en, &val);
	if (ret)
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vrfck1_en=0x%x, ", val);

	if (clk_buf_read(&mt6685_dcxo.hw, &ldo_reg._vrfck1_op_en14, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vrfck1_op_en=0x%x, ", val);

	if (clk_buf_read(&mt6685_dcxo.hw, &ldo_reg._vrfck2_en, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vrfck2_en=0x%x, ", val);

	if (clk_buf_read(&mt6685_dcxo.hw, &ldo_reg._vrfck2_op_en14, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vrfck2_op_en=0x%x, ", val);

	if (clk_buf_read(&mt6685_dcxo.hw, &ldo_reg._vbbck_en, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vbbck_en=0x%x, ", val);

	if (clk_buf_read(&mt6685_dcxo.hw, &ldo_reg._vbbck_op_en14, &val))
		goto DUMP_MISC_FAILED;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"vbbck_op_mode=0x%x\n", val);

	return len;

DUMP_MISC_FAILED:
	len -= 2;
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

static int mt6685_dcxo_misc_store(const char *obj, const char *arg)
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
		mutex_lock(&mt6685_dcxo.lock);

	if (!strcmp(obj, "VRFCK1_EN"))
		ret = clk_buf_write(&mt6685_dcxo.hw, &ldo_reg._vrfck1_en, cmd);
	else if (!strcmp(obj, "VRFCK1_OP_MODE"))
		ret = clk_buf_write(&mt6685_dcxo.hw,
				&ldo_reg._vrfck1_op_en14,
				cmd);
	else if (!strcmp(obj, "VRFCK2_EN"))
		ret = clk_buf_write(&mt6685_dcxo.hw, &ldo_reg._vrfck2_en, cmd);
	else if (!strcmp(obj, "VRFCK2_OP_MODE"))
		ret = clk_buf_write(&mt6685_dcxo.hw,
				&ldo_reg._vrfck2_op_en14,
				cmd);
	else if (!strcmp(obj, "VBBCK_EN"))
		ret = clk_buf_write(&mt6685_dcxo.hw, &ldo_reg._vbbck_en, cmd);
	else if (!strcmp(obj, "VBBCK_OP_MODE"))
		ret = clk_buf_write(&mt6685_dcxo.hw,
				&ldo_reg._vbbck_op_en14,
				cmd);
	else if (!strcmp(obj, "TOP_PMIC_DIG_SW"))
		ret = clk_buf_write(&mt6685_dcxo.hw,
				&debug_reg._xo_pmic_top_dig_sw,
				cmd);
	else if (!strcmp(obj, "CLKSEL_MODE"))
		ret = clk_buf_write(&mt6685_dcxo.hw,
				&debug_reg._xo_clksel_man,
				cmd);
	else if (!strcmp(obj, "CLKSEL_EN"))
		ret = clk_buf_write(&mt6685_dcxo.hw,
				&debug_reg._xo_clksel_en_m,
				cmd);
	else
		ret = -EPERM;

	if (!no_lock)
		mutex_unlock(&mt6685_dcxo.lock);

	return ret;
}
