// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: owen.chen <owen.chen@mediatek.com>
 */

/*
 * @file    mtk-clk-buf-hw.c
 * @brief   Driver for clock buffer control of each platform
 *
 */

#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "mtk_clkbuf_ctl.h"
#include "mtk_clkbuf_common.h"
#include "mtk_clkbuf_hw.h"

#define INVALID_VAL		(0xffff)
#define LOG_NUM			1024
#define MAX_CFG			10

#define CLKBUF_DEBUG

#define REG_CLR_SET(_sta, _set, _clr, _bit) { \
		.sta_ofs = _sta,	\
		.set_ofs = _set,	\
		.clr_ofs = _clr,	\
		.bit = _bit,		\
	}

#define REG(_sta, _bit) { \
		.sta_ofs = _sta,	\
		.set_ofs = 0,	\
		.clr_ofs = 0,	\
		.bit = _bit,		\
	}

enum pmic_id {
	PMIC_6357 = 0,
	PMIC_6359,
	PMIC_6359P,
	PMIC_NUM,
};

enum reg_id {
	XO1_MODE = 0,
	XO2_MODE,
	XO3_MODE,
	XO4_MODE,
	XO5_MODE,
	XO6_MODE,
	XO7_MODE,
	XO1_EN_M,
	XO2_EN_M,
	XO3_EN_M,
	XO4_EN_M,
	XO5_EN_M,
	XO6_EN_M,
	XO7_EN_M,
	XO1_ISET_M,
	XO2_ISET_M,
	XO3_ISET_M,
	XO4_ISET_M,
	XO5_ISET_M,
	XO6_ISET_M,
	XO7_ISET_M,
	XO1_BBLPM_EN_MSK,
	XO2_BBLPM_EN_MSK,
	XO3_BBLPM_EN_MSK,
	XO4_BBLPM_EN_MSK,
	XO5_BBLPM_EN_MSK,
	XO6_BBLPM_EN_MSK,
	XO7_BBLPM_EN_MSK,
	XO_BB_LPM_EN_M,
	XO_BB_LPM_EN_SEL,
	SRCLKEN_IN3_EN,
	LDO_VRFCK_HW14_OP_EN,
	LDO_VRFCK_EN,
	LDO_VBBCK_HW14_OP_EN,
	LDO_VBBCK_EN,
	VRFCK_HV_EN,
	DRV_CURR1,
	DRV_CURR2,
	DRV_CURR3,
	DRV_CURR4,
	DRV_CURR5,
	DRV_CURR6,
	DRV_CURR7,
	AUXOUT_SEL,
	AUXOUT_XO1,
	AUXOUT_XO2,
	AUXOUT_XO3,
	AUXOUT_XO4,
	AUXOUT_XO5,
	AUXOUT_XO6,
	AUXOUT_XO7,
	AUXOUT_DRV_CURR1,
	AUXOUT_DRV_CURR2,
	AUXOUT_DRV_CURR3,
	AUXOUT_DRV_CURR4,
	AUXOUT_DRV_CURR5,
	AUXOUT_DRV_CURR6,
	AUXOUT_DRV_CURR7,
	AUXOUT_BBLPM_EN,
	AUXOUT_BBLPM_O,
	AUXOUT_IS_AUTO_CALI,
	DCXO_START,
	DCXO_END,
	REG_NUM,
};

static struct regmap *pmic_regmap;
static int xo_num;
static unsigned int *xo_exist;
static const char **xo_name;
static unsigned int xo_setclr[20];
static int xo_setclr_num;
static unsigned int cfg_val[REG_NUM][MAX_CFG];
static int cfg_num[REG_NUM];

static const char *PMIC_NAME[PMIC_NUM] = {
	[PMIC_6357] = "mediatek,mt6357",
	[PMIC_6359] = "mediatek,mt6359",
	[PMIC_6359P] = "mediatek,mt6359p",
};

static const char * const pmic_cfg_prop[REG_NUM] = {
	[AUXOUT_XO1] = "pmic-auxout-xo",
	[AUXOUT_DRV_CURR1] = "pmic-auxout-drvcurr",
	[AUXOUT_BBLPM_EN] = "pmic-auxout-bblpm-en",
	[AUXOUT_BBLPM_O] = "pmic-auxout-bblpm-o",
	[AUXOUT_IS_AUTO_CALI] = "pmic-is-auto-calc",
};

static const u32 PMIC_CLKBUF_MASK[] = {
	[XO1_MODE] = 0x3,
	[XO1_EN_M] = 0x1,
	[XO1_ISET_M] = 0x3,
	[XO1_BBLPM_EN_MSK] = 0x1,
	[XO_BB_LPM_EN_M] = 0x1,
	[XO_BB_LPM_EN_SEL] = 0x1,
	[SRCLKEN_IN3_EN] = 0x1,
	[LDO_VRFCK_HW14_OP_EN] = 0x1,
	[LDO_VRFCK_EN] = 0x1,
	[LDO_VBBCK_HW14_OP_EN] = 0x1,
	[LDO_VBBCK_EN] = 0x1,
	[VRFCK_HV_EN] = 0x1,
	[DRV_CURR1] = 0x3,
	[AUXOUT_SEL] = 0x2f,
	[AUXOUT_XO1] = 0x1,
	[AUXOUT_DRV_CURR1] = 0x3,
	[AUXOUT_BBLPM_EN] = 0x1,
	[AUXOUT_BBLPM_O] = 0x1,
	[AUXOUT_IS_AUTO_CALI] = 0x3,
	[DCXO_START] = 0xffff,
};

struct reg_data {
	unsigned int sta_ofs;
	unsigned int set_ofs;
	unsigned int clr_ofs;
	unsigned int bit;
};

static struct reg_data  reg_mt6357[] = {
	[XO1_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 0),
	[XO1_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 2),
	[XO1_ISET_M] = REG(0x7b0, 0),
	[XO2_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 3),
	[XO2_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 5),
	[XO2_ISET_M] = REG(0x7b0, 2),
	[XO3_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 6),
	[XO3_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 8),
	[XO3_ISET_M] = REG(0x7b0, 4),
	[XO4_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 9),
	[XO4_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 11),
	[XO4_ISET_M] = REG(0x7b0, 6),
	[XO7_MODE] = REG_CLR_SET(0x7A2, 0x7A4, 0x7A6, 11),
	[XO7_EN_M] = REG_CLR_SET(0x7A2, 0x7A4, 0x7A6, 13),
	[XO7_ISET_M] = REG(0x7b0, 12),
	[XO_BB_LPM_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 12),
	[SRCLKEN_IN3_EN] = REG(0x44A, 0),
	[DCXO_START] = REG(0x788, 0),
	[DCXO_END] = REG(0x7BC, 0),
	[AUXOUT_SEL] = REG(0x7B4, 0),
	[AUXOUT_XO1] =  REG(0x7B6, 0),
	[AUXOUT_XO2] =  REG(0x7B6, 6),
	[AUXOUT_XO3] =  REG(0x7B6, 0),
	[AUXOUT_XO4] =  REG(0x7B6, 6),
	[AUXOUT_XO6] =  REG(0x7B6, 6),
	[AUXOUT_XO7] =  REG(0x7B6, 12),
	[AUXOUT_DRV_CURR1] =  REG(0x7B6, 1),
	[AUXOUT_DRV_CURR2] =  REG(0x7B6, 7),
	[AUXOUT_DRV_CURR3] =  REG(0x7B6, 1),
	[AUXOUT_DRV_CURR4] =  REG(0x7B6, 7),
	[AUXOUT_DRV_CURR5] =  REG(0x7B6, 1),
	[AUXOUT_DRV_CURR6] =  REG(0x7B6, 7),
	[AUXOUT_DRV_CURR7] =  REG(0x7B6, 12),
	[AUXOUT_BBLPM_EN] =  REG(0x7B6, 0),
	[AUXOUT_IS_AUTO_CALI] = REG(0x7B6, 2),
};

static struct reg_data  reg_mt6359[] = {
	[XO1_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 0),
	[XO1_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 2),
	[XO2_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 3),
	[XO2_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 5),
	[XO3_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 6),
	[XO3_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 8),
	[XO4_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 9),
	[XO4_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 11),
	[XO7_MODE] = REG_CLR_SET(0x79E, 0x7A0, 0x7A2, 12),
	[XO7_EN_M] = REG_CLR_SET(0x79E, 0x7A0, 0x7A2, 14),
	[XO_BB_LPM_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 0),
	[XO_BB_LPM_EN_SEL] = REG(0x7A8, 0),
	[XO1_BBLPM_EN_MSK] = REG(0x7A8, 1),
	[XO2_BBLPM_EN_MSK] = REG(0x7A8, 2),
	[XO3_BBLPM_EN_MSK] = REG(0x7A8, 3),
	[XO4_BBLPM_EN_MSK] = REG(0x7A8, 4),
	[XO7_BBLPM_EN_MSK] = REG(0x7A8, 6),
	[SRCLKEN_IN3_EN] = REG(0x458, 0),
	[LDO_VRFCK_HW14_OP_EN] = REG_CLR_SET(0x1D1E, 0x1D20, 0x1D22, 14),
	[LDO_VRFCK_EN] = REG(0x1D1A, 0),
	[LDO_VBBCK_HW14_OP_EN] = REG_CLR_SET(0x1D2E, 0x1D30, 0xD32, 14),
	[LDO_VBBCK_EN] = REG(0x1D2A, 0),
	[DCXO_START] = REG(0x788, 0),
	[DCXO_END] = REG(0x7B6, 0),
	[AUXOUT_SEL] = REG(0x7B0, 0),
	[AUXOUT_XO1] =  REG(0x7B2, 13),
	[AUXOUT_XO2] =  REG(0x7B2, 11),
	[AUXOUT_XO3] =  REG(0x7B2, 9),
	[AUXOUT_XO4] =  REG(0x7B2, 7),
	[AUXOUT_XO6] =  REG(0x7B2, 5),
	[AUXOUT_XO7] =  REG(0x7B2, 3),
	[AUXOUT_BBLPM_EN] =  REG(0x7B2, 0),
};

static struct reg_data  reg_mt6359p[] = {
	[XO1_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 0),
	[XO1_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 2),
	[XO2_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 3),
	[XO2_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 5),
	[XO3_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 6),
	[XO3_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 8),
	[XO4_MODE] = REG_CLR_SET(0x788, 0x78A, 0x78C, 9),
	[XO4_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 11),
	[XO7_MODE] = REG_CLR_SET(0x79E, 0x7A0, 0x7A2, 12),
	[XO7_EN_M] = REG_CLR_SET(0x79E, 0x7A0, 0x7A2, 14),
	[XO_BB_LPM_EN_M] = REG_CLR_SET(0x788, 0x78A, 0x78C, 0),
	[XO_BB_LPM_EN_SEL] = REG(0x7A8, 0),
	[XO1_BBLPM_EN_MSK] = REG(0x7A8, 1),
	[XO2_BBLPM_EN_MSK] = REG(0x7A8, 2),
	[XO3_BBLPM_EN_MSK] = REG(0x7A8, 3),
	[XO4_BBLPM_EN_MSK] = REG(0x7A8, 4),
	[XO7_BBLPM_EN_MSK] = REG(0x7A8, 6),
	[SRCLKEN_IN3_EN] = REG(0x458, 0),
	[LDO_VRFCK_HW14_OP_EN] = REG_CLR_SET(0x1D22, 0x1D24, 0x1D26, 14),
	[LDO_VRFCK_EN] = REG(0x1D1C, 0),
	[LDO_VBBCK_HW14_OP_EN] = REG_CLR_SET(0x1D34, 0x1D36, 0x1D38, 14),
	[LDO_VBBCK_EN] = REG(0x1D2E, 0),
	[VRFCK_HV_EN] = REG(0x209C, 9),
	[AUXOUT_SEL] = REG(0x7B0, 0),
	[AUXOUT_XO1] =  REG(0x7B2, 13),
	[AUXOUT_XO2] =  REG(0x7B2, 11),
	[AUXOUT_XO3] =  REG(0x7B2, 9),
	[AUXOUT_XO4] =  REG(0x7B2, 7),
	[AUXOUT_XO6] =  REG(0x7B2, 5),
	[AUXOUT_XO7] =  REG(0x7B2, 3),
	[AUXOUT_BBLPM_EN] =  REG(0x7B2, 0),
	[AUXOUT_BBLPM_O] =  REG(0x7B2, 15),
	[DCXO_START] = REG(0x788, 0),
	[DCXO_END] = REG(0x7B6, 0),
};

static const struct reg_data *reg;

static inline bool _is_pmic_clk_buf_debug_enable(void)
{
#ifdef CLKBUF_DEBUG
	return 1;
#else
	return 0;
#endif
}

static inline int pmic_clkbuf_read(u32 id, u32 idx, u32 *val)
{
	unsigned int index = 0;
	u32 regval = 0;

	if (id < 0 || id >= REG_NUM ||
		idx < 0 || idx >= xo_num)
		return -1;

	index  = id + idx;

	if (!reg[index].sta_ofs)
		return -1;

	regmap_read(pmic_regmap, reg[index].sta_ofs, &regval);
	*val = (regval >> reg[index].bit & PMIC_CLKBUF_MASK[id]);

	return 0;
}

static inline int pmic_clkbuf_write(u32 id, u32 idx, u32 val)
{
	unsigned int index = 0;

	if (id < 0 || id >= REG_NUM ||
		idx < 0 || idx >= xo_num)
		return -1;

	index  = id + idx;

	if (!reg[index].sta_ofs)
		return -1;

	val <<= reg[index].bit;

	regmap_write(pmic_regmap, reg[index].sta_ofs, val);

	return 0;
}

static inline int pmic_clkbuf_set(u32 id, u32 idx, u32 val)
{
	unsigned int index = 0;

	if (id < 0 || id >= REG_NUM ||
		idx < 0 || idx >= xo_num)
		return -1;

	index  = id + idx;

	if (!reg[index].set_ofs)
		return -1;

	val <<= reg[index].bit;
	regmap_write(pmic_regmap, reg[index].set_ofs, val);

	return 0;
}

static inline int pmic_clkbuf_clr(u32 id, u32 idx, u32 val)
{
	unsigned int index = 0;

	if (id < 0 || id >= REG_NUM ||
		idx < 0 || idx >= xo_num)
		return -1;

	index  = id + idx;

	if (!reg[index].clr_ofs)
		return -1;

	val <<= reg[index].bit;
	regmap_write(pmic_regmap, reg[index].clr_ofs, val);

	return 0;
}

static inline int pmic_clkbuf_update(u32 id, u32 idx, u32 val)
{
	unsigned int index = 0;
	u32 msk = 0;
	u32 out = 0;

	if (id < 0 || id >= REG_NUM ||
		idx < 0 || idx >= xo_num)
		return -1;

	index  = id + idx;

	if (!reg[index].sta_ofs)
		return -1;

	val <<= reg[index].bit;
	msk = PMIC_CLKBUF_MASK[id] << reg[index].bit;

	regmap_update_bits(pmic_regmap, reg[index].sta_ofs, msk, val);
	regmap_read(pmic_regmap, reg[index].sta_ofs, &out);

	if (_is_pmic_clk_buf_debug_enable()) {
		pr_info("[%s]: shift: %u\n", __func__, reg[index].bit);
		pr_info("[%s]: shift val: 0x%x\n", __func__, val);
		pr_info("[%s]: mask: 0x%x, shift mask: 0x%x\n",
				__func__, PMIC_CLKBUF_MASK[id], msk);
		pr_info("%s: update value: 0x%x\n", __func__, out);
	}

	return 0;
}

static int _pmic_clk_buf_bblpm_sw_en(bool on)
{
	u32 val = 0;
	int ret = 0;

	if (on) {
		ret = pmic_clkbuf_set(XO_BB_LPM_EN_M, 0, on);
		if (ret)
			return CLK_BUF_NOT_READY;
	} else {
		ret = pmic_clkbuf_clr(XO_BB_LPM_EN_M, 0, on);
		if (ret)
			return CLK_BUF_NOT_READY;
	}

	ret = pmic_clkbuf_read(XO_BB_LPM_EN_M, 0, &val);
	if (ret)
		return CLK_BUF_NOT_READY;

	pr_debug("%s(%u): bblpm_sw=0x%x\n",
			__func__, (on ? 1 : 0), val);
	return ret;
}

static int _pmic_clk_buf_set_bblpm_hw_msk(u32 id, bool onoff)
{
	int ret = 0;

	ret = pmic_clkbuf_update(XO1_BBLPM_EN_MSK, id, onoff);
	if (ret)
		return CLK_BUF_NOT_READY;

	return ret;
}

static int _pmic_clk_buf_bblpm_hw_en(bool on)
{
	u32 val = 0;
	int ret = 0;

	ret = pmic_clkbuf_update(XO_BB_LPM_EN_SEL, 0, on);
	if (ret)
		return CLK_BUF_NOT_READY;

	ret = pmic_clkbuf_read(XO_BB_LPM_EN_SEL, 0, &val);
	if (ret)
		return CLK_BUF_NOT_READY;

	pr_debug("%s(%u): bblpm_hw=0x%x\n",
			__func__, (on ? 1 : 0), val);
	return ret;
}

static int _pmic_clk_buf_get_drv_curr(u32 id, u32 *drvcurr)
{
	int cfg = 0;
	int ret = 0;

	cfg = cfg_val[AUXOUT_DRV_CURR1][id];
	if (cfg == INVALID_VAL)
		return CLK_BUF_NOT_SUPPORT;

	if (_is_pmic_clk_buf_debug_enable())
		pr_info("%s: AUXOUT drv curr idx: %d, value: %d\n",
				__func__, id, cfg);

	ret = pmic_clkbuf_write(AUXOUT_SEL, 0, cfg);
	if (ret)
		return CLK_BUF_NOT_READY;

	ret = pmic_clkbuf_read(AUXOUT_DRV_CURR1, id, drvcurr);
	if (ret)
		return CLK_BUF_NOT_READY;

	return ret;
}

static int _pmic_clk_buf_set_drv_curr(u32 id, u32 drvcurr)
{
	int ret = 0;

	if (!xo_exist[id])
		return CLK_BUF_NOT_SUPPORT;
	ret = pmic_clkbuf_update(DRV_CURR1, id, drvcurr % 4);
	if (ret)
		return CLK_BUF_NOT_READY;

	return ret;
}

static int clk_buf_get_xo_num(void)
{
	return xo_num;
}

static int clk_buf_get_xo_name(u32 id, char *name)
{
	int len = 0;

	if (!xo_exist[id])
		return CLK_BUF_NOT_SUPPORT;

	len = strlen(xo_name[id]) + 4;
	len = snprintf(name, len, "XO_%s", xo_name[id]);
	if (!name)
		return -ENOMEM;

	return 0;
}

static int clk_buf_get_xo_en(u32 id, u32 *stat)
{
	u32 cfg = 0;
	int ret = 0;

	cfg = cfg_val[AUXOUT_XO1][id];
	if (cfg == INVALID_VAL)
		return CLK_BUF_NOT_SUPPORT;

	pmic_clkbuf_write(AUXOUT_SEL, 0, cfg);
	if (ret) {
		pr_notice("fail to write auxout sel\n");
		return CLK_BUF_NOT_READY;
	}
	pmic_clkbuf_read(AUXOUT_XO1, id, stat);
	if (ret) {
		pr_notice("fail to read auxout XO_%s\n", xo_name[id]);
		return CLK_BUF_NOT_READY;
	}

	pr_info("[%s]: EN_STAT = (XO_%s)%u\n",
			__func__, xo_name[id], *stat);

	return ret;
}

static int clk_buf_get_xo_sw_en(u32 id, u32 *stat)
{
	int ret = 0;

	if (!xo_exist[id])
		return CLK_BUF_NOT_SUPPORT;

	ret = pmic_clkbuf_read(XO1_EN_M, id, stat);
	if (ret)
		return CLK_BUF_NOT_READY;

	pr_info("[%s]: SW_EN_STAT = %d\n", __func__, *stat);

	return 0;
}

static int clk_buf_set_xo_sw_en(u32 id, bool on)
{
	int ret = 0;

	if (!xo_exist[id])
		return CLK_BUF_NOT_SUPPORT;

	ret = pmic_clkbuf_update(XO1_EN_M, id, on);
	if (ret)
		return CLK_BUF_NOT_READY;

	pr_info("[%s]: SET SW_EN = %d\n", __func__, on);

	return 0;
}

static int clk_buf_get_xo_mode(u32 id, u32 *mode)
{
	int ret = 0;

	if (!xo_exist[id])
		return CLK_BUF_NOT_SUPPORT;

	ret = pmic_clkbuf_read(XO1_MODE, id, mode);
	if (ret)
		return CLK_BUF_NOT_READY;

	pr_info("[%s]: XO_%s_MODE = %u\n", __func__, xo_name[id], *mode);

	return 0;
}

static int clk_buf_set_xo_mode(u32 id, u32 mode)
{
	int ret = 0;

	if (!xo_exist[id])
		return CLK_BUF_NOT_SUPPORT;

	pr_info("[%s]: xo: %u, mode: %u\n", __func__, id, mode);

	ret = pmic_clkbuf_update(XO1_MODE, id, mode);
	if (ret)
		return CLK_BUF_NOT_READY;

	pr_info("[%s]: SET XO_MODE = %u\n", __func__, mode);

	return 0;
}

static int clk_buf_get_bblpm_en(u32 *stat)
{
	char buf[256];
	u32 len = 0;
	u32 val = 0;
	u32 ret = 0;

	if (_is_pmic_clk_buf_debug_enable())
		pr_info("[%s]: auxout bblpm idx: %u %u\n", __func__,
				cfg_val[AUXOUT_BBLPM_EN][0],
				cfg_val[AUXOUT_BBLPM_O][0]);

	ret = pmic_clkbuf_write(AUXOUT_SEL, 0, cfg_val[AUXOUT_BBLPM_EN][0]);
	if (ret)
		return CLK_BUF_NOT_READY;
	ret = pmic_clkbuf_read(AUXOUT_BBLPM_EN, 0, &val);
	if (!ret) {
		*stat = val;
		len += snprintf(buf+len, PAGE_SIZE-len,
				"bblpm auxout en_stat(%d)\n", val);
	}
	pmic_clkbuf_write(AUXOUT_SEL, 0, cfg_val[AUXOUT_BBLPM_O][0]);
	if (ret)
		return CLK_BUF_NOT_READY;
	pmic_clkbuf_read(AUXOUT_BBLPM_O, 0, &val);
	if (!ret) {
		*stat &= val;
		len += snprintf(buf+len, PAGE_SIZE-len,
				"bblpm auxout o_stat(%d)\n", val);
	}

	pr_notice("%s\n", buf);

	return ret;
}

static int clk_buf_dump_xo_reg(char *buf)
{
	u32 start_ofs = reg[DCXO_START].sta_ofs;
	u32 end_ofs = reg[DCXO_END].sta_ofs;
	u32 msk = PMIC_CLKBUF_MASK[DCXO_START];
	u32 setclr = 0;
	u32 ofs = 0;
	u32 len = 0;
	u32 val = 0;
	u32 i = 0, j = 0;

	if (!start_ofs || !end_ofs)
		return 0;

	do {
		ofs = start_ofs + (i * 2) + (setclr * 4);

		regmap_read(pmic_regmap, ofs, &val);
		val = (val >> reg[DCXO_START].bit & msk);
		len += snprintf(buf+len, PAGE_SIZE-len,
			"DCXO_CW%02d[0x%x]=0x%x\n", i, ofs, val);

		for (j = 0; j < xo_setclr_num; j++) {
			if (xo_setclr[j] == i)
				setclr++;
		}
		i++;
	} while (ofs < end_ofs);

	return len;
}

static int clk_buf_dump_misc_log(char *buf)
{
	u32 len = 0;
	u32 val = 0;
	int ret = 0;

	len += clk_buf_dump_xo_reg(buf);

	ret = pmic_clkbuf_read(SRCLKEN_IN3_EN, 0, &val);
	if (!ret)
		len += snprintf(buf+len, PAGE_SIZE-len, "%s(%s)=0x%x\n",
			"srclken_conn",
			"srclken_in3",
			val);

	ret = pmic_clkbuf_read(LDO_VRFCK_HW14_OP_EN, 0, &val);
	if (!ret)
		len += snprintf(buf+len, PAGE_SIZE-len, "vrfck(%s)=0x%x\n",
				"op_mode", val);

	ret = pmic_clkbuf_read(LDO_VRFCK_EN, 0, &val);
	if (!ret)
		len += snprintf(buf+len, PAGE_SIZE-len, "vrfck(%s)=0x%x\n",
				"en", val);

	ret = pmic_clkbuf_read(LDO_VBBCK_HW14_OP_EN, 0, &val);
	if (!ret)
		len += snprintf(buf+len, PAGE_SIZE-len, "vbbck(%s)=0x%x\n",
				"op_mode", val);

	ret = pmic_clkbuf_read(LDO_VBBCK_EN, 0, &val);
	if (!ret)
		len += snprintf(buf+len, PAGE_SIZE-len,
				"vbbck(%s)=0x%x\n", "en", val);

	return len;
}

static int _pmic_clk_buf_dts_init(struct device_node *node)
{
	int val = 0;
	int ret = 0;
	int i, j;

	ret =  of_property_count_elems_of_size(node,
				"pmic-xo-exist", sizeof(u32));
	if (ret < 0) {
		pr_info("[%s]: no find property %s\n",
				__func__, "pmic-xo-exist");
		xo_num = 0;
	} else
		xo_num = ret;

	xo_exist = kcalloc(xo_num, sizeof(*xo_exist), GFP_KERNEL);
	xo_name = kcalloc(xo_num, sizeof(*xo_name), GFP_KERNEL);

	for (i = 0; i < xo_num; i++) {
		ret = of_property_read_u32_index(node,
				"pmic-xo-exist", i, &xo_exist[i]);
		if (ret) {
			pr_info("[%s]: read number of pmic clkbuf xo failed\n",
					__func__);
			goto no_property;
		}

		if (!xo_exist[i])
			continue;

		ret = of_property_read_string_index(node,
				"pmic-xo-name", i, &xo_name[i]);
		if (ret) {
			pr_info("[%s]: read pmic clkbuf xo name failed\n",
					__func__);
			goto no_property;
		}
	}

	ret =  of_property_count_elems_of_size(node,
				"pmic-xo-set-clr", sizeof(u32));
	if (ret < 0) {
		pr_info("[%s]: no find property %s\n",
				__func__, "pmic-xo-set-clr");
		xo_setclr_num = 0;
	} else
		xo_setclr_num = ret;

	for (i = 0; i < xo_setclr_num; i++) {
		ret = of_property_read_u32_index(node,
					"pmic-xo-set-clr", i, &val);
		if (ret) {
			pr_info("[%s]: read pmic clkbuf xo_set_clr failed\n",
					__func__);
			goto no_property;
		}

		xo_setclr[i] = val;
	}

	for (i = 0; i < REG_NUM; i++) {
		if (!pmic_cfg_prop[i])
			continue;

		ret =  of_property_count_elems_of_size(node,
				pmic_cfg_prop[i], sizeof(u32));
		if (ret < 0) {
			pr_info("[%s]: no find property %s\n",
					__func__, pmic_cfg_prop[i]);
			cfg_num[i] = 0;
		} else
			cfg_num[i] = ret;

		if (!cfg_num[i])
			continue;

		for (j = 0; j < cfg_num[i]; j++) {
			ret = of_property_read_u32_index(node,
					pmic_cfg_prop[i], j, &val);

			if (_is_pmic_clk_buf_debug_enable())
				pr_debug("[%s]: find property %s\n",
						__func__, pmic_cfg_prop[i]);
			if (ret) {
				pr_info("[%s]: read property failed %s[%d]\n",
						__func__, pmic_cfg_prop[i],
						j, cfg_num[i]);
				goto no_property;
			}

			cfg_val[i][j] = val;
		}
	}
	pr_info("[%s]: pmic dts init done\n", __func__);

	return 0;

no_property:
	return -1;
}

static struct pmic_clkbuf_op pmic_op[PMIC_NUM] = {
	[PMIC_6357] = {
		.pmic_name = "mt6357",
		.pmic_clk_buf_get_drv_curr = _pmic_clk_buf_get_drv_curr,
		.pmic_clk_buf_set_drv_curr = _pmic_clk_buf_set_drv_curr,
		.pmic_clk_buf_get_xo_num = clk_buf_get_xo_num,
		.pmic_clk_buf_get_xo_name = clk_buf_get_xo_name,
		.pmic_clk_buf_get_xo_en = clk_buf_get_xo_en,
		.pmic_clk_buf_get_xo_sw_en = clk_buf_get_xo_sw_en,
		.pmic_clk_buf_set_xo_sw_en = clk_buf_set_xo_sw_en,
		.pmic_clk_buf_get_xo_mode = clk_buf_get_xo_mode,
		.pmic_clk_buf_set_xo_mode = clk_buf_set_xo_mode,
		.pmic_clk_buf_get_bblpm_en = clk_buf_get_bblpm_en,
		.pmic_clk_buf_set_bblpm_sw_en = _pmic_clk_buf_bblpm_sw_en,
	},
	[PMIC_6359] = {
		.pmic_name = "mt6359",
		.pmic_clk_buf_set_bblpm_hw_msk =
					_pmic_clk_buf_set_bblpm_hw_msk,
		.pmic_clk_buf_bblpm_hw_en = _pmic_clk_buf_bblpm_hw_en,
		.pmic_clk_buf_get_xo_num = clk_buf_get_xo_num,
		.pmic_clk_buf_get_xo_name = clk_buf_get_xo_name,
		.pmic_clk_buf_get_xo_en = clk_buf_get_xo_en,
		.pmic_clk_buf_get_xo_sw_en = clk_buf_get_xo_sw_en,
		.pmic_clk_buf_set_xo_sw_en = clk_buf_set_xo_sw_en,
		.pmic_clk_buf_get_xo_mode = clk_buf_get_xo_mode,
		.pmic_clk_buf_set_xo_mode = clk_buf_set_xo_mode,
		.pmic_clk_buf_get_bblpm_en = clk_buf_get_bblpm_en,
		.pmic_clk_buf_set_bblpm_sw_en = _pmic_clk_buf_bblpm_sw_en,
		.pmic_clk_buf_dump_misc_log = clk_buf_dump_misc_log,
	},
	[PMIC_6359P] = {
		.pmic_name = "mt6359p",
		.pmic_clk_buf_set_bblpm_hw_msk =
					_pmic_clk_buf_set_bblpm_hw_msk,
		.pmic_clk_buf_bblpm_hw_en = _pmic_clk_buf_bblpm_hw_en,
		.pmic_clk_buf_get_xo_num = clk_buf_get_xo_num,
		.pmic_clk_buf_get_xo_name = clk_buf_get_xo_name,
		.pmic_clk_buf_get_xo_en = clk_buf_get_xo_en,
		.pmic_clk_buf_get_xo_sw_en = clk_buf_get_xo_sw_en,
		.pmic_clk_buf_set_xo_sw_en = clk_buf_set_xo_sw_en,
		.pmic_clk_buf_get_xo_mode = clk_buf_get_xo_mode,
		.pmic_clk_buf_set_xo_mode = clk_buf_set_xo_mode,
		.pmic_clk_buf_get_bblpm_en = clk_buf_get_bblpm_en,
		.pmic_clk_buf_set_bblpm_sw_en = _pmic_clk_buf_bblpm_sw_en,
		.pmic_clk_buf_dump_misc_log = clk_buf_dump_misc_log,
	},
};

int get_pmic_clkbuf(struct device_node *node)
{
	u32 idx = 0;
	struct device_node *parent_node = node->parent;

	for (idx = 0; idx < PMIC_NUM; idx++) {
		if (of_device_is_compatible(parent_node, PMIC_NAME[idx])) {
			set_clkbuf_ops(&pmic_op[idx]);
			return 0;
		}
	}
	return -1;
}

static int mtk_clkbuf_pmic_probe(struct platform_device *pdev)
{
	struct mt6397_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;

	if (!chip->regmap)
		goto no_regmap;
	pmic_regmap = chip->regmap;
	reg = of_device_get_match_data(&pdev->dev);
	ret = get_pmic_clkbuf(node);
	if (ret) {
		pr_info("[%s]: pmic op not found\n", __func__);
		goto no_pmic_op;
	}

	_pmic_clk_buf_dts_init(node);

	return ret;

no_regmap:
no_pmic_op:
	return -1;
}

static const struct platform_device_id mtk_clkbuf_pmic_ids[] = {
	{"mtk-clock-buffer", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mtk_clkbuf_pmic_ids);

static const struct of_device_id mtk_clkbuf_pmic_of_match[] = {
	{
		.compatible = "mediatek,mt6357-clock-buffer",
		.data = &reg_mt6357[0],
	}, {
		.compatible = "mediatek,mt6359-clock-buffer",
		.data = &reg_mt6359[0],
	}, {
		.compatible = "mediatek,mt6359p-clock-buffer",
		.data = &reg_mt6359p[0],
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_clkbuf_pmic_of_match);

static struct platform_driver mtk_clkbuf_pmic_driver = {
	.driver = {
		.name = "mtk-clock-buffer-pmic",
		.of_match_table = of_match_ptr(mtk_clkbuf_pmic_of_match),
	},
	.probe = mtk_clkbuf_pmic_probe,
	.id_table = mtk_clkbuf_pmic_ids,
};

module_platform_driver(mtk_clkbuf_pmic_driver);
MODULE_AUTHOR("Owen Chen <owen.chen@mediatek.com>");
MODULE_DESCRIPTION("SOC Driver for MediaTek PMIC Clock Buffer");
MODULE_LICENSE("GPL v2");
