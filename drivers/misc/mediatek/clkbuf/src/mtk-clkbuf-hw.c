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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "mtk_clkbuf_ctl.h"
#include "mtk_clkbuf_common.h"
#include "mtk_clkbuf_hw.h"
#if defined(CONFIG_MTK_UFS_SUPPORT)
#include "ufs-mtk.h"
#endif

#define CLKBUF_STATUS_INFO_SIZE	2048
#define XO_NAME_LEN		10
#define XO_MODE_LEN		20

#define CONN_EN_BIT		(cfg[PWRAP_CONN_EN].bit[0])
#define NFC_EN_BIT		(cfg[PWRAP_NFC_EN].bit[0])
#define DCXO_CONN_ENABLE	(0x1 << CONN_EN_BIT)
#define DCXO_NFC_ENABLE		(0x1 << NFC_EN_BIT)

/* TODO: set this flag to false after driver is ready */
static bool is_clkbuf_bringup;
static bool get_bringup_state_done;
static bool is_clkbuf_initiated;
static bool is_flightmode_on;
static bool clkbuf_debug;
/* read dts property to enable below flags */
static bool has_impedance;
static bool has_desense;
static bool has_drv_curr;
static bool bblpm_support;

static unsigned int xo_mode_init[XO_NUMBER];
static unsigned int pwrap_dcxo_en_init;
/* store all register information including offset & bit shift */
static struct reg_info *cfg;
static char **xo_name;
static unsigned int xo_num;
static int xo_match_idx[XO_NUMBER] = {-1};

/*
 * This is initial value of dts property, it would be replaced after dws file
 * generate cust.dtsi.
 * Including CLK_BUF_STATUS, CLK_BUF_OUTPUT_IMPEDANCE,
 * CLK_BUF_CONTROLS_DESENSE.
 */
static unsigned int CLK_BUF_STATUS[XO_NUMBER] = {
		CLOCK_BUFFER_HW_CONTROL,
		CLOCK_BUFFER_SW_CONTROL,
		CLOCK_BUFFER_SW_CONTROL,
		CLOCK_BUFFER_HW_CONTROL,
		CLOCK_BUFFER_DISABLE,
		CLOCK_BUFFER_DISABLE,
		CLOCK_BUFFER_SW_CONTROL};

static unsigned int CLK_BUF_OUTPUT_IMPEDANCE[XO_NUMBER] = {
	CLK_BUF_OUTPUT_IMPEDANCE_6,
	CLK_BUF_OUTPUT_IMPEDANCE_4,
	CLK_BUF_OUTPUT_IMPEDANCE_6,
	CLK_BUF_OUTPUT_IMPEDANCE_4,
	CLK_BUF_OUTPUT_IMPEDANCE_0,
	CLK_BUF_OUTPUT_IMPEDANCE_0,
	CLK_BUF_OUTPUT_IMPEDANCE_6};

static unsigned int CLK_BUF_CONTROLS_DESENSE[XO_NUMBER] = {
	CLK_BUF_CONTROLS_FOR_DESENSE_0,
	CLK_BUF_CONTROLS_FOR_DESENSE_4,
	CLK_BUF_CONTROLS_FOR_DESENSE_0,
	CLK_BUF_CONTROLS_FOR_DESENSE_4,
	CLK_BUF_CONTROLS_FOR_DESENSE_0,
	CLK_BUF_CONTROLS_FOR_DESENSE_0,
	CLK_BUF_CONTROLS_FOR_DESENSE_0};

static unsigned int CLK_BUF_DRIVING_CURRENT[XO_NUMBER] = {
	CLK_BUF_DRIVING_CURR_1,
	CLK_BUF_DRIVING_CURR_1,
	CLK_BUF_DRIVING_CURR_1,
	CLK_BUF_DRIVING_CURR_1,
	CLK_BUF_DRIVING_CURR_1,
	CLK_BUF_DRIVING_CURR_1,
	CLK_BUF_DRIVING_CURR_1};
/*
 * This is strings defined for debug usage, it's better understand of meaning
 * of data we present.
 */
static char XO_NAME[XO_NUMBER][7] = {
		"XO_SOC",
		"XO_WCN",
		"XO_NFC",
		"XO_CEL",
		"XO_AUD",
		"XO_PD",
		"XO_EXT"};

static char XO_M_NAME[XO_NUMBER][MODE_M_NUM][XO_MODE_LEN] = {
	{"SOC_EN_M", "SOC_EN_BB_G", "SOC_CLK_SEL_G", "SOC_EN_BB_CLK_SEL_G"},
	{"WCN_EN_M", "WCN_EN_BB_G", "WCN_SRCLKEN_CONN", "WCN_BUF24_EN"},
	{"NFC_EN_M", "NFC_EN_BB_G", "NFC_CLK_SEL_G", "NFC_BUF234_EN"},
	{"CEL_EN_M", "CEL_EN_BB_G", "CEL_CLK_SEL_G", "CEL_BUF24_EN"},
	{},
	{},
	{"EXT_EN_M", "EXT_EN_BB_G", "EXT_CLK_SEL_G", "EXT_BUF247_EN"},
};

/*
 * @struct dts_predef
 * @brief
 * @	const char prop[20] : property string for dts node to fetch data
 *	u32 len : the length of array we need to put in the offset&bit data.
 *	u32 idx : the index of property data we serach for.
 *	u32 mask : the mask defined by hw register mapping.
 *	u32 interval : if interval != 0, means we use this value to calculate
 *		      the reset of offset. (offset = 0x990, interval = 0x2, we
 *		      get 0x992, 0x994, 0x996...depending our length defined)
 */
static struct dts_predef clkbuf_dts[DTS_NUM] = {
	[PWRAP_DCXO_EN] = {"pwrap-dcxo-en", 1, 0, 0xffff, 0},
	[PWRAP_CONN_EN] = {"pwrap-dcxo-en", 1, 2, 0x1, 0},
	[PWRAP_NFC_EN] = {"pwrap-dcxo-en", 1, 4, 0x1, 0},
	[PWRAP_CONN_CFG] = {"pwrap-dcxo-cfg", 4, 0, 0xffff, 0x4},
	[PWRAP_NFC_CFG] = {"pwrap-dcxo-cfg", 4, 1, 0xffff, 0x4},

	[SPM_MD_PWR_STA] = {"spm-pwr-status", 1, 0, 0x1, 0},
	[SPM_CONN_PWR_STA] = {"spm-pwr-status", 1, 2, 0x1, 0},
};

static const char *base_n[REGMAP_NUM] = {"pwrap", "sleep"};
static const struct pmic_clkbuf_op *pmic_op;
static bool clkbuf_pmic_op_done;

#ifndef CLKBUF_BRINGUP
static enum CLK_BUF_TYPE  pmic_clk_buf_swctrl[XO_NUMBER] = {
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_DISABLE,
	CLK_BUF_SW_DISABLE,
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_DISABLE,
	CLK_BUF_SW_DISABLE,
	CLK_BUF_SW_ENABLE
};
#else /* For Bring-up */
static enum CLK_BUF_TYPE  pmic_clk_buf_swctrl[XO_NUMBER] = {
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_DISABLE,
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_ENABLE
};
#endif

void set_clkbuf_ops(const struct pmic_clkbuf_op *ops)
{
	pmic_op = ops;
	clkbuf_pmic_op_done = true;
}
EXPORT_SYMBOL(set_clkbuf_ops);

static inline void clkbuf_read(u32 dts, u32 id, u32 *val)
{
	u32 regval = 0;

	regmap_read(cfg[dts].regmap, cfg[dts].ofs[id], &regval);

	*val =  (regval >> cfg[dts].bit[id]) & clkbuf_dts[dts].mask;
}

static inline void clkbuf_write(u32 dts, u32 id, u32 val)
{
	val <<= cfg[dts].bit[id];
	pr_info("offset: 0x%x\n", cfg[dts].ofs[id]);
	regmap_write(cfg[dts].regmap, cfg[dts].ofs[id], val);
}

static inline void clkbuf_set(u32 dts, u32 id, u32 val)
{
	val <<= cfg[dts].bit[id];
	regmap_write(cfg[dts].regmap, cfg[dts].ofs[id] + 0x2, val);
}

static inline void clkbuf_clr(u32 dts, u32 id, u32 val)
{
	val <<= cfg[dts].bit[id];
	regmap_write(cfg[dts].regmap, cfg[dts].ofs[id] + 0x4, val);
}

static inline void clkbuf_update(u32 dts, u32 id, u32 val)
{
	u32 mask = 0;

	val <<= cfg[dts].bit[id];
	mask = clkbuf_dts[dts].mask << cfg[dts].bit[id];

	regmap_update_bits(cfg[dts].regmap,
			cfg[dts].ofs[id],
			mask,
			val);

	regmap_read(cfg[dts].regmap, cfg[dts].ofs[id], &val);
}

bool clk_buf_get_init_sta(void)
{
	return is_clkbuf_initiated;
}

void clk_buf_set_init_sta(bool done)
{
	is_clkbuf_initiated = done;
}

static inline void _set_impedance_support(bool enable)
{
	has_impedance = enable;
}

static inline bool _get_impedance_support(void)
{
	return has_impedance;
}

static inline void _set_desense_support(bool enable)
{
	has_desense = enable;
}

static inline bool _get_desense_support(void)
{
	return has_desense;
}

static inline void _set_drv_curr_support(bool enable)
{
	has_drv_curr = enable;
}

static inline bool _get_drv_curr_support(void)
{
	return has_drv_curr;
}

static inline void _set_bblpm_support(bool enable)
{
	bblpm_support = enable;
}

static inline bool _get_bblpm_support(void)
{
	return bblpm_support;
}

static bool _chk_xo_cond(s32 id)
{
	if (!clk_buf_get_init_sta()
			|| id < 0 || id >= XO_NUMBER
			|| id == -1)
		return false;

	return true;
}

static void _clk_buf_xo_match_idx_init(void)
{
	int i;

	for (i = 0; i < XO_NUMBER; i++)
		xo_match_idx[i] = -1;
}

static int _clk_buf_xo_match_idx_set(const char *name, u32 idx)
{
	u32 match = 0;
	int i;

	for (i = 0; i < XO_NUMBER; i++) {
		if (!strcmp(name, XO_NAME[i])) {
			match = i;
			xo_match_idx[match] = idx;
		}
	}

	if (match < XO_NUMBER)
		return 0;

	return -EINVAL;
}

static int _clk_buf_get_xo_num(void)
{
	int ret  = 0;

	if (pmic_op && pmic_op->pmic_clk_buf_get_xo_num)
		xo_num = pmic_op->pmic_clk_buf_get_xo_num();
	else
		return CLK_BUF_NOT_READY;

	if (!xo_num) {
		pr_err("xo_num cannot be zero\n");
		return CLK_BUF_NOT_SUPPORT;
	}

	return ret;
}

static int _clk_buf_xo_get_name(s32 id, char *name)
{
	int ret  = 0;

	if (pmic_op && pmic_op->pmic_clk_buf_get_xo_name)
		ret = pmic_op->pmic_clk_buf_get_xo_name(id,
				name);
	else
		return CLK_BUF_NOT_READY;

	return ret;
}

static unsigned int _clk_buf_mode_get(s32 id)
{
	unsigned int val = 0;
	int ret = 0;

	if (!_chk_xo_cond(id))
		return CLK_BUF_NOT_SUPPORT;

	if (pmic_op && pmic_op->pmic_clk_buf_get_xo_mode)
		ret = pmic_op->pmic_clk_buf_get_xo_mode(
				id, &val);
	else
		return CLK_BUF_NOT_READY;

	return val;
}

static int _clk_buf_mode_set(s32 id, unsigned int mode)
{
	unsigned int val = 0;
	int ret = 0;

	if (!_chk_xo_cond(id))
		return CLK_BUF_NOT_SUPPORT;

	if (!pmic_op || !pmic_op->pmic_clk_buf_set_xo_mode)
		return CLK_BUF_NOT_READY;

	val = _clk_buf_mode_get(id);
	if (val == CLK_BUF_NOT_READY)
		return CLK_BUF_NOT_READY;

	pr_info("[%s]: xo: %d, mode: %d\n", __func__, id, mode);
	ret = pmic_op->pmic_clk_buf_set_xo_mode(id, mode);

	return ret;
}

static u32 _clk_buf_en_get(s32 id)
{
	u32 onoff = 0;
	int ret  = 0;

	if (!_chk_xo_cond(id))
		return CLK_BUF_NOT_SUPPORT;

	if (pmic_op && pmic_op->pmic_clk_buf_get_xo_sw_en) {
		ret = pmic_op->pmic_clk_buf_get_xo_sw_en(
				id, &onoff);
		if (ret) {
			pr_err("get xo sw enable cfg fail(%d)\n", ret);
			return ret;
		}
	} else
		return CLK_BUF_NOT_READY;

	return onoff;
}

static int _clk_buf_en_set(s32 id, bool onoff)
{
	int ret = 0;

	if (!_chk_xo_cond(id))
		return CLK_BUF_NOT_SUPPORT;

	if (!pmic_op || !pmic_op->pmic_clk_buf_set_xo_sw_en)
		return CLK_BUF_NOT_READY;
	if (onoff) {
		ret = pmic_op->pmic_clk_buf_set_xo_sw_en(id, true);
		if (ret) {
			pr_err("set xo sw enable cfg fail(%d)\n", ret);
			return ret;
		}
	} else {
		ret = pmic_op->pmic_clk_buf_set_xo_sw_en(id, false);
		if (ret) {
			pr_err("set xo sw enable cfg fail(%d)\n", ret);
			return ret;
		}
	}

	return ret;
}

static enum dev_sta _get_nfc_dev_state(void)
{
#ifdef CONFIG_MTK_CLKBUF_NFC
	pr_info("%s: NFC support\n", __func__);
	return DEV_ON;
#else
	return DEV_NOT_SUPPORT;
#endif
}

static enum dev_sta _get_ufs_dev_state(void)
{
	pr_info("%s: UFS support: %d\n", __func__, UFS_CLKBUF_SUPPORT);
#if UFS_CLKBUF_SUPPORT
	return DEV_ON;
#endif
	return DEV_NOT_SUPPORT;
}

#ifdef CONFIG_MTK_CLKBUF_BBLPM
static int _clk_buf_set_bblpm_hw_en(bool on)
{
	if (!_get_bblpm_support()) {
		pr_info("[%s]: bblpm not support, continue without\n",
			__func__);
		return 0;
	}

	if (pmic_op && pmic_op->pmic_clk_buf_bblpm_hw_en)
		pmic_op->pmic_clk_buf_bblpm_hw_en(on);
	else
		return CLK_BUF_NOT_SUPPORT;

	return 0;
}

static void _clk_buf_set_bblpm_hw_msk(s32 id, bool onoff)
{
	int ret = 0;

	if (!_chk_xo_cond(id)) {
		pr_info("[%s]: %s isn't enabled\n", __func__,
				xo_name[id]);
		return;
	}

	if (!_get_bblpm_support()) {
		pr_info("[%s]: bblpm not support, continue without\n",
			__func__);
		return;
	}

	mutex_lock(&clk_buf_ctrl_lock);
	if (pmic_op && pmic_op->pmic_clk_buf_set_bblpm_hw_msk) {
		ret = pmic_op->pmic_clk_buf_set_bblpm_hw_msk(id, onoff);
		if (ret)
			pr_err("set bblpm msk fail(%d)\n", ret);
	}

	mutex_unlock(&clk_buf_ctrl_lock);
}

void clk_buf_get_enter_bblpm_cond(u32 *bblpm_cond)
{
	u32 val = 0;

	if (!is_clkbuf_initiated || !_get_bblpm_support()) {
		(*bblpm_cond) |= BBLPM_SKIP;
		return;
	}

	clkbuf_read(SPM_MD_PWR_STA, 0, &val);
	if (val)
		(*bblpm_cond) |= BBLPM_CEL;

	clkbuf_read(SPM_CONN_PWR_STA, 0, &val);
	if (val || pmic_clk_buf_swctrl[xo_match_idx[XO_WCN]])
		(*bblpm_cond) |= BBLPM_WCN;

	val = _clk_buf_en_get(xo_match_idx[CLK_BUF_NFC]);
	if ((val >= 0) && (val || pmic_clk_buf_swctrl[xo_match_idx[XO_NFC]]))
		(*bblpm_cond) |= BBLPM_NFC;

	pr_info("%s: bblpm condition: 0x%x\n", __func__, *bblpm_cond);
}

static int _clk_buf_get_bblpm_en(u32 *stat)
{
	int ret = 0;

	if (pmic_op && pmic_op->pmic_clk_buf_get_bblpm_en) {
		ret = pmic_op->pmic_clk_buf_get_bblpm_en(stat);
		if (ret)
			pr_err("get bblpm enable stat fail(%d)\n", ret);
	} else
		return CLK_BUF_NOT_SUPPORT;

	return ret;
}

static int _clk_buf_get_bblpm_en_stat(void)
{
	u32 stat = 0;
	int ret = 0;

	ret = _clk_buf_get_bblpm_en(&stat);
	if (ret < 0)
		return ret;
	return stat;
}

int clk_buf_ctrl_bblpm_sw(bool enable)
{
	if (!_get_bblpm_support()) {
		pr_info("[%s]: bblpm not support, continue without\n",
			__func__);
		return 0;
	}
	_clk_buf_set_bblpm_hw_en(false);

	/* get set/clr register offset */

	if (pmic_op && pmic_op->pmic_clk_buf_set_bblpm_sw_en)
		pmic_op->pmic_clk_buf_set_bblpm_sw_en(enable);
	else
		return CLK_BUF_NOT_SUPPORT;

	if (_clk_buf_get_bblpm_en_stat() != enable) {
		pr_info("manual set bblpm fail\n");
		return -1;
	}

	return 0;
}

int clk_buf_bblpm_init(void)
{
	if (!_get_bblpm_support()) {
		pr_info("[%s]: bblpm not support, continue without\n",
			__func__);
	}
	_clk_buf_set_bblpm_hw_msk(xo_match_idx[CLK_BUF_BB_MD], true);
	_clk_buf_set_bblpm_hw_msk(xo_match_idx[CLK_BUF_CONN], false);
	_clk_buf_set_bblpm_hw_msk(xo_match_idx[CLK_BUF_NFC], false);
	_clk_buf_set_bblpm_hw_msk(xo_match_idx[CLK_BUF_RF], false);
	_clk_buf_set_bblpm_hw_msk(xo_match_idx[CLK_BUF_UFS], false);

	if (CLK_BUF_STATUS[xo_match_idx[XO_CEL]] == CLOCK_BUFFER_DISABLE)
		_clk_buf_set_bblpm_hw_en(true);
	else
		_clk_buf_set_bblpm_hw_en(false);

	return 0;
}
#else
static int _clk_buf_set_bblpm_hw_en(bool on)
{
	pr_info("not support bblpm\n");

	return -1;
}

void clk_buf_get_enter_bblpm_cond(u32 *bblpm_cond)
{
	(*bblpm_cond) |= BBLPM_SKIP;
}

static int  _clk_buf_get_bblpm_en(u32 *stat)
{
	pr_info("not support bblpm\n");
	return -1;
}
static int _clk_buf_get_bblpm_en_stat(void)
{
	pr_info("not support bblpm\n");

	return -1;
}
int clk_buf_ctrl_bblpm_sw(bool enable)
{
	_clk_buf_get_bblpm_en_stat();
	pr_info("not support bblpm\n");

	return -1;
}

int clk_buf_bblpm_init(void)
{
	pr_info("not support bblpm\n");

	return -1;
}
#endif

/* for spm driver use */
bool clk_buf_get_flight_mode(void)
{
	return is_flightmode_on;
}

/* for ccci driver to notify this */
int clk_buf_hw_set_flight_mode(bool on)
{
	int ret = 0;

	is_flightmode_on = on;

	if (is_flightmode_on)
		ret = _clk_buf_set_bblpm_hw_en(true);
	else
		ret = _clk_buf_set_bblpm_hw_en(false);

	return ret;
}

static int _clk_buf_ctrl_internal(u32 id, enum cmd_type cmd)
{
	int match_id = xo_match_idx[id];
	short no_lock = 0;
	int val = 0;
	int ret = 0;

	/* we should not turn off SOC 26M */
	if (!_chk_xo_cond(match_id)) {
		pr_info("%s: id=%d isn't supported\n", __func__, id);
		return -1;
	}

	if (preempt_count() > 0 || irqs_disabled() ||
		system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&clk_buf_ctrl_lock);

	switch (cmd) {
	case CLK_BUF_OFF:
		if (id == CLK_BUF_CONN)
			clkbuf_update(PWRAP_CONN_EN, 0, 0);
		else if (id == CLK_BUF_NFC)
			clkbuf_update(PWRAP_NFC_EN, 0, 0);

		ret = _clk_buf_mode_set(match_id, BUF_MAN_M);
		if (ret)
			goto FAIL;
		ret = _clk_buf_en_set(match_id, false);
		if (ret)
			goto FAIL;
		pmic_clk_buf_swctrl[id] = 0;
		break;
	case CLK_BUF_ON:
		if (id == CLK_BUF_CONN)
			clkbuf_update(PWRAP_CONN_EN, 0, 0);
		else if (id == CLK_BUF_NFC)
			clkbuf_update(PWRAP_NFC_EN, 0, 0);

		ret = _clk_buf_mode_set(match_id, BUF_MAN_M);
		if (ret)
			goto FAIL;
		ret = _clk_buf_en_set(match_id, true);
		if (ret)
			goto FAIL;
		pmic_clk_buf_swctrl[id] = 1;
		break;
	case CLK_BUF_ENBB:
		ret = _clk_buf_mode_set(match_id, EN_BB_M);
		if (ret)
			goto FAIL;
		break;
	case CLK_BUF_SIG:
		ret = _clk_buf_mode_set(match_id, SIG_CTRL_M);
		if (ret)
			goto FAIL;
		break;
	case CLK_BUF_COBUF:
		ret = _clk_buf_mode_set(id, CO_BUF_M);
		if (ret)
			goto FAIL;
		break;
	case CLK_BUF_INIT_SETTING:
		if (id == CLK_BUF_CONN) {
			val = pwrap_dcxo_en_init >> CONN_EN_BIT;
			clkbuf_update(PWRAP_CONN_EN, 0, val);
		} else if (id == CLK_BUF_NFC) {
			val = pwrap_dcxo_en_init >> NFC_EN_BIT;
			clkbuf_update(PWRAP_NFC_EN, 0, val);
		}

		ret = _clk_buf_mode_set(match_id, xo_mode_init[match_id]);
		if (ret)
			goto FAIL;
		break;
	default:
		ret = CLK_BUF_NOT_SUPPORT;
		pr_info("%s: id=%d isn't supported\n", __func__, id);
		break;
	}

	clkbuf_read(PWRAP_DCXO_EN, 0, &val);
	pr_debug("%s: id=%d, cmd=%d, DCXO_EN = 0x%x\n",
		__func__, id, cmd, val);

FAIL:
	if (!no_lock)
		mutex_unlock(&clk_buf_ctrl_lock);

	return ret;
}

static int _clk_buf_get_drv_curr(u32 id, u32 *drv_curr)
{
	if (pmic_op && pmic_op->pmic_clk_buf_get_drv_curr)
		return pmic_op->pmic_clk_buf_get_drv_curr(id, drv_curr);
	else
		return CLK_BUF_NOT_SUPPORT;
}

static int _clk_buf_set_manual_drv_curr(u32 id, u32 drv_curr_vals)
{
	u32 drv_curr = 0;
	int ret  = 0;

	if (pmic_op && pmic_op->pmic_clk_buf_set_drv_curr)
		pmic_op->pmic_clk_buf_set_drv_curr(id, drv_curr_vals);
	else
		return CLK_BUF_NOT_SUPPORT;

	_clk_buf_get_drv_curr(id, &drv_curr);
	pr_notice("%s: after set curr[%d], read back as 0x%x\n",
			__func__, id, drv_curr);

	return ret;
}

int clk_buf_hw_ctrl(u32 id, bool onoff)
{
	int match_id = xo_match_idx[id];
	short no_lock = 0;
	int ret = 0;

	if (!_chk_xo_cond(match_id)) {
		pr_info("%s: id=%d isn't controlled by SW\n",
			__func__, id);
		return CLK_BUF_NOT_SUPPORT;
	}

	pr_debug("%s: id=%d, onoff=%d\n",
		__func__, id, onoff);

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&clk_buf_ctrl_lock);

	if (_clk_buf_mode_get(match_id) == BUF_MAN_M)
		ret = _clk_buf_ctrl_internal(id, onoff);

	pmic_clk_buf_swctrl[match_id] = onoff;

	if (!no_lock)
		mutex_unlock(&clk_buf_ctrl_lock);

	return ret;
}

static int _clk_buf_dump_dws_log(char *buf)
{
	u32 len = 0;
	u32 i = 0;

	for (i = 0; i < XO_NUMBER; i++) {
		if (!_chk_xo_cond(xo_match_idx[i]))
			continue;
		len += snprintf(buf+len, PAGE_SIZE-len,
				"CLK_BUF%d_STATUS=%d\n",
				i+1, CLK_BUF_STATUS[i]);
	}

	if (_get_impedance_support()) {
		for (i = 0; i < XO_NUMBER; i++) {
			if (!_chk_xo_cond(xo_match_idx[i]))
				continue;
			len += snprintf(buf+len, PAGE_SIZE-len,
				"CLK_BUF%u_OUTPUT_IMPEDANCE=%u\n",
				i + 1, CLK_BUF_OUTPUT_IMPEDANCE[i]);
		}
	}

	if (_get_desense_support()) {
		for (i = 0; i < XO_NUMBER; i++) {
			if (!_chk_xo_cond(xo_match_idx[i]))
				continue;
			len += snprintf(buf+len, PAGE_SIZE-len,
				"CLK_BUF%u_CONTROLS_DESENSE=%u\n",
				i + 1, CLK_BUF_CONTROLS_DESENSE[i]);
		}
	}

	if (_get_drv_curr_support()) {
		for (i = 0; i < XO_NUMBER; i++) {
			if (!_chk_xo_cond(xo_match_idx[i]))
				continue;
			len += snprintf(buf+len, PAGE_SIZE-len,
				"CLK_BUF%u_DRIVING_CURRENT=%u\n",
				i + 1, CLK_BUF_DRIVING_CURRENT[i]);
		}
	}

	pr_info("%s: %s\n", __func__, buf);

	return len;
}

static int _clk_buf_dump_misc_log(char *buf)
{
	u32 len = 0;

	if (pmic_op && pmic_op->pmic_clk_buf_dump_misc_log)
		len += pmic_op->pmic_clk_buf_dump_misc_log(buf);
	else
		return CLK_BUF_NOT_READY;

	return len;
}

void clk_buf_hw_dump_misc_log(void)
{
	char *buf = NULL;
	int ret = 0;

	buf = vmalloc(CLKBUF_STATUS_INFO_SIZE);

	ret = _clk_buf_dump_misc_log(buf);
	if (ret <= 0)
		return;

	pr_notice("%s\n", buf);
}

int clk_buf_hw_get_xo_en(u32 id, u32 *stat)
{
	if (!_chk_xo_cond(id))
		return CLK_BUF_NOT_SUPPORT;

	if (!pmic_op || !pmic_op->pmic_clk_buf_get_xo_en)
		return CLK_BUF_NOT_READY;

	return pmic_op->pmic_clk_buf_get_xo_en(id, stat);
}

static ssize_t _clk_buf_show_status_info_internal(char *buf)
{
	int stat = 0;
	int drv_curr = 0;
	int bblpm_stat = 0;
	int buf_mode = 0;
	int buf_en = 0;
	u32 val[4] = {0};
	int len = 0;
	int ret = 0;
	int i = 0;

	for (i = 0; i < XO_NUMBER; i++) {
		int id = xo_match_idx[i];

		if (!_chk_xo_cond(id))
			continue;

		ret = clk_buf_hw_get_xo_en(id, &stat);
		if (ret)
			pr_notice("get xo enable stat fail(%d)\n", ret);

		len += snprintf(buf+len, PAGE_SIZE-len,
			"%s   SW(1)/HW(2) CTL: %d, Dis(0)/En(1): %d, RS: %u\n",
			xo_name[id], CLK_BUF_STATUS[id],
			pmic_clk_buf_swctrl[id], stat);
	}

	len += snprintf(buf+len, PAGE_SIZE-len,
			".********** clock buffer debug info **********\n");

	len += _clk_buf_dump_misc_log(buf+len);
	len += _clk_buf_dump_dws_log(buf+len);

	for (i = 0; i < XO_NUMBER; i++) {
		int id = xo_match_idx[i];

		if (!_chk_xo_cond(id))
			continue;

		buf_mode = _clk_buf_mode_get(id);

		buf_en = _clk_buf_en_get(id);

		if (buf_en >= 0)
			len += snprintf(buf+len, PAGE_SIZE-len,
				"buf[%02d]: mode = %s(0x%x), en_m = %s(%d)\n",
				i, XO_M_NAME[id][buf_mode],
				buf_mode, buf_en ? "sw enable" : "sw disable",
				buf_en);
	}

	for (i = 0; i < 4; i++)
		clkbuf_read(PWRAP_CONN_CFG, i, &val[i]);
	len += snprintf(buf+len, PAGE_SIZE-len,
		"DCXO_CONN_ADR0/WDATA0/ADR1/WDATA1=0x%x %x %x %x\n",
		val[0], val[1], val[2], val[3]);
	for (i = 0; i < 4; i++)
		clkbuf_read(PWRAP_NFC_CFG, i, &val[i]);
	len += snprintf(buf+len, PAGE_SIZE-len,
		"DCXO_NFC_ADR0/WDATA0/ADR1/WDATA1=0x%x %x %x %x\n",
		val[0], val[1], val[2], val[3]);

	clkbuf_read(PWRAP_DCXO_EN, 0, &val[0]);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"DCXO_ENABLE=0x%x, flight mode = %d ",
			val[0], clk_buf_get_flight_mode());

	ret = _clk_buf_get_bblpm_en(&bblpm_stat);
	if (!ret)
		len += snprintf(buf+len, PAGE_SIZE-len, "bblpm = %d\n", bblpm_stat);
	else
		len += snprintf(buf+len, PAGE_SIZE-len, "bblpm = %d\n", ret);

	for (i = 0; i < XO_NUMBER; i++) {
		int id = xo_match_idx[i];

		if (!_chk_xo_cond(id))
			continue;

		ret = _clk_buf_get_drv_curr(id, &drv_curr);
		if (!ret)
			len += snprintf(buf + len, PAGE_SIZE - len,
				"PMIC_CLKBUF_DRV_CURR (%d)= %u\n", drv_curr);
	}

	len += snprintf(buf+len, PAGE_SIZE-len,
			".********** clock buffer command help **********\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"PMIC switch on/off: echo pmic en1 en2 en3 en4 en5 ");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"en6 en7 > /sys/kernel/clk_buf/clk_buf_ctrl\n");
	return len;
}

#ifdef CONFIG_PM
static ssize_t clk_buf_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 *clk_buf_en = NULL;
	u32 pwrap_dcxo_en = 0;
	u32 val = 0;
	u32 i = 0;
	char cmd[32] = {0};

	clk_buf_en = kcalloc(xo_num, sizeof(*clk_buf_en), GFP_KERNEL);

	if (sscanf(buf, "%31s %x %x %x %x %x %x %x", cmd,
		&clk_buf_en[XO_SOC],
		&clk_buf_en[XO_WCN],
		&clk_buf_en[XO_NFC],
		&clk_buf_en[XO_CEL],
		&clk_buf_en[XO_AUD],
		&clk_buf_en[XO_PD],
		&clk_buf_en[XO_EXT])
		!= (XO_NUMBER + 1))
		return -EPERM;

	for (i = 0; i < XO_NUMBER; i++) {
		int id = xo_match_idx[i];
		int ret = 0;

		if (!_chk_xo_cond(id))
			continue;

		if (!strcmp(cmd, "pmic")) {
			pmic_clk_buf_swctrl[id] = clk_buf_en[i];

			_clk_buf_ctrl_internal(id, pmic_clk_buf_swctrl[id] % 2);

			pr_info("%s clk_buf_swctrl[%d]=[%u]\n",
				__func__, id, pmic_clk_buf_swctrl[id]);

			return count;
		} else if (!strcmp(cmd, "pwrap")) {
			mutex_lock(&clk_buf_ctrl_lock);

			if (i == XO_WCN) {
				if (clk_buf_en[i])
					pwrap_dcxo_en |= DCXO_CONN_ENABLE;
				else
					pwrap_dcxo_en &= ~DCXO_CONN_ENABLE;
			} else if (i == XO_NFC) {
				if (clk_buf_en[i])
					pwrap_dcxo_en |= DCXO_NFC_ENABLE;
				else
					pwrap_dcxo_en &= ~DCXO_NFC_ENABLE;
			}

			clkbuf_write(PWRAP_DCXO_EN, 0, pwrap_dcxo_en);
			clkbuf_read(PWRAP_DCXO_EN, 0, &val);
			pr_info("%s: DCXO_ENABLE=0x%x, pwrap_dcxo_en=0x%x\n",
					__func__, val, pwrap_dcxo_en);

			mutex_unlock(&clk_buf_ctrl_lock);

			return count;
		} else if (!strcmp(cmd, "drvcurr")) {
			mutex_lock(&clk_buf_ctrl_lock);
			ret = _clk_buf_set_manual_drv_curr(id, clk_buf_en[i]);
			if (ret)
				return -EFAULT;
			mutex_unlock(&clk_buf_ctrl_lock);

			return count;
		} else {
			return -EINVAL;
		}
	}

	return count;
}

static ssize_t clk_buf_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = _clk_buf_show_status_info_internal(buf);

	return len;
}

static int _clk_buf_debug_internal(char *cmd, u32 id)
{
	int ret = 0;

	if (!strcmp(cmd, "OFF"))
		ret = _clk_buf_ctrl_internal(id, CLK_BUF_OFF);
	else if (!strcmp(cmd, "ON"))
		ret = _clk_buf_ctrl_internal(id, CLK_BUF_ON);
	else if (!strcmp(cmd, "EN_BB"))
		ret = _clk_buf_ctrl_internal(id, CLK_BUF_ENBB);
	else if (!strcmp(cmd, "SIG"))
		ret = _clk_buf_ctrl_internal(id, CLK_BUF_SIG);
	else if (!strcmp(cmd, "CO_BUFFER"))
		ret = _clk_buf_ctrl_internal(id, CLK_BUF_COBUF);
	else if (!strcmp(cmd, "INIT"))
		ret = _clk_buf_ctrl_internal(id, CLK_BUF_INIT_SETTING);
	else if (!strcmp(cmd, "TEST_ON"))
		ret = clk_buf_hw_ctrl(id, true);
	else if (!strcmp(cmd, "TEST_OFF"))
		ret = clk_buf_hw_ctrl(id, false);
	else
		ret = -1;

	pr_info("[%s]: ret: %d\n", __func__, ret);
	return ret;
}

static ssize_t clk_buf_debug_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[32] =  {'\0'}, xo_user[11] = {'\0'};
	u32 i = 0;

	if ((sscanf(buf, "%31s %10s", cmd, xo_user) != 2))
		return -EPERM;

	for (i = 0; i < xo_num; i++)
		if (!strcmp(xo_user, xo_name[i])) {
			if (_clk_buf_debug_internal(cmd, i) < 0)
				goto ERROR_CMD;
			else if (!strcmp(cmd, "INIT"))
				clkbuf_debug = false;
			else
				clkbuf_debug = true;
			return count;
		}

ERROR_CMD:
	pr_info("bad argument!! please follow correct format\n");
	return -EPERM;
}

static ssize_t clk_buf_debug_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "clkbuf_debug=%d\n",
		clkbuf_debug);

	return len;
}

static ssize_t clk_buf_bblpm_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 onoff = 0;
	int ret = 0;

	if ((kstrtouint(buf, 10, &onoff))) {
		pr_info("bblpm input error\n");
		return -EPERM;
	}

	if (onoff == 2)
		_clk_buf_set_bblpm_hw_en(true);
	else if (onoff == 1)
		ret = clk_buf_ctrl_bblpm_sw(true);
	else if (onoff == 0)
		ret = clk_buf_ctrl_bblpm_sw(false);

	if (ret)
		return ret;

	return count;
}

static ssize_t clk_buf_bblpm_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	u32 bblpm_stat = 0;
	u32 bblpm_cond = 0;
	u32 xo_stat = 0;
	int len = 0;
	int ret = 0;
	int i;

	for (i = 0; i < XO_NUMBER; i++) {
		int id = xo_match_idx[i];

		ret = clk_buf_hw_get_xo_en(id, &xo_stat);
		if (ret)
			pr_notice("get xo enable stat fail(%d)\n", ret);

		len += snprintf(buf+len, PAGE_SIZE-len,
			"EN_STAT(%s)=%d\n",
			xo_name[id],
			xo_stat);
	}

	_clk_buf_get_bblpm_en(&bblpm_stat);
	clk_buf_get_enter_bblpm_cond(&bblpm_cond);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"bblpm en_stat(%d)\n",
		bblpm_stat);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"bblpm cond(0x%x)\n",
		bblpm_cond);

	return len;
}

DEFINE_ATTR_RW(clk_buf_ctrl);
DEFINE_ATTR_RW(clk_buf_debug);
DEFINE_ATTR_RW(clk_buf_bblpm);
static struct attribute *clk_buf_attrs[] = {
	/* for clock buffer control */
	__ATTR_OF(clk_buf_ctrl),
	__ATTR_OF(clk_buf_debug),
	__ATTR_OF(clk_buf_bblpm),

	/* must */
	NULL,
};

static struct attribute_group clk_buf_attr_group = {
	.name	= "clk_buf",
	.attrs	= clk_buf_attrs,
};

int clk_buf_fs_init(void)
{
	int r = 0;

	/* create /sys/kernel/clk_buf/xxx */
	r = sysfs_create_group(kernel_kobj, &clk_buf_attr_group);
	if (r)
		pr_notice("FAILED TO CREATE /sys/kernel/clk_buf (%d)\n", r);

	return r;
}
#else /* !CONFIG_PM */
int clk_buf_fs_init(void)
{
	return 0;
}
#endif /* CONFIG_PM */

#if defined(CONFIG_OF)

static void _clk_buf_read_dts_misc_node(struct device_node *node)
{
	int ret = 0;
	const char *str = NULL;

	ret = of_property_read_u32_array(node,
		"mediatek,clkbuf-output-impedance",
		CLK_BUF_OUTPUT_IMPEDANCE, xo_num);
	if (ret) {
		pr_info("[%s]: No impedance property read, continue without\n",
			__func__);
		_set_impedance_support(false);
	} else {
		_set_impedance_support(true);
	}

	ret = of_property_read_u32_array(node,
		"mediatek,clkbuf-controls-for-desense",
		CLK_BUF_CONTROLS_DESENSE, xo_num);
	if (ret) {
		pr_info("[%s]: No control desense property read, continue without\n",
			__func__);
		_set_desense_support(false);
	} else {
		_set_desense_support(true);
	}

	ret = of_property_read_u32_array(node,
		"mediatek,clkbuf-driving-current",
		CLK_BUF_DRIVING_CURRENT, xo_num);
	if (ret) {
		pr_info("[%s]: No driving current property read, continue without\n",
			__func__);
		_set_drv_curr_support(false);
	} else {
		_set_drv_curr_support(true);
	}

	ret = of_property_read_string(node,
		"mediatek,bblpm-support", &str);
	if (ret || (strcmp(str, "enable"))) {
		pr_info("[%s]: No bblpm support read or bblpm is not enable, continue without bblpm support\n",
			__func__);
		_set_bblpm_support(false);
	} else {
		_set_bblpm_support(true);
	}
}

static int _clk_buf_dts_init_internal(struct device_node *node, u32 idx)
{
	u32 interval = clkbuf_dts[idx].interval;
	u32 setclr = 0;
	u32 base = 0;
	int ret = 0;
	int i = 0;

	cfg[idx].ofs = kcalloc(clkbuf_dts[idx].len, sizeof(u32), GFP_KERNEL);
	if (!cfg[idx].ofs)
		goto no_mem;

	cfg[idx].bit = kcalloc(clkbuf_dts[idx].len, sizeof(u32), GFP_KERNEL);
	if (!cfg[idx].bit) {
		kfree(cfg[idx].ofs);
		goto no_mem;
	}

	for (i = 0; i < clkbuf_dts[idx].len && interval == 0; i++) {
		ret = of_property_read_u32_index(node,
				clkbuf_dts[idx].prop,
				clkbuf_dts[idx].idx + i * 2,
				&cfg[idx].ofs[i]);
		if (ret) {
			pr_info("[%s]: find %s property failed\n",
				__func__, clkbuf_dts[idx].prop);
			goto no_property;
		}

		ret = of_property_read_u32_index(node,
				clkbuf_dts[idx].prop,
				clkbuf_dts[idx].idx + i * 2 + 1,
				&cfg[idx].bit[i]);
		if (ret) {
			pr_info("[%s]: find %s property failed\n",
				__func__, clkbuf_dts[idx].prop);
			goto no_property;
		}
	}

	if (interval != 0) {
		ret = of_property_read_u32_index(node,
				clkbuf_dts[idx].prop,
				clkbuf_dts[idx].idx,
				&base);
		if (ret)
			goto no_property;

		for (i = 0; i < clkbuf_dts[idx].len; i++) {
			int val = 0;

			val = base + (i * interval) + (setclr * interval * 2);
			cfg[idx].ofs[i] = val;
			cfg[idx].bit[i] = 0;
		}
	}

	return 0;

no_property:
	pr_info("%s can't find property %d\n",
			__func__, ret);
	return -1;
no_mem:
	pr_info("%s can't allocate memory %d\n",
			__func__, ret);
	return -ENOMEM;
}

int clk_buf_dts_init(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	u32 start[] = {PWRAP_START, SPM_START};
	u32 end[] = {PWRAP_END, SPM_END};
	int i = 0, j = 0;
	int ret = 0;

	ret = of_property_read_u32_array(node,
		"mediatek,clkbuf-config",
		CLK_BUF_STATUS, xo_num);
	if (ret)
		goto no_property;

	_clk_buf_read_dts_misc_node(node);

	cfg = kzalloc(sizeof(struct reg_info) * DTS_NUM, GFP_KERNEL);
	if (!cfg)
		goto no_mem;

	for (i = 0; i < REGMAP_NUM; i++) {
		struct regmap *regmap;

		regmap = syscon_regmap_lookup_by_phandle(node,
					base_n[i]);
		if (IS_ERR(regmap)) {
			dev_err(&pdev->dev,
				"Cannot find %s controller: %ld\n",
				base_n[i],
				PTR_ERR(regmap));
			goto no_compatible;
		}

		for (j = start[i]; j < end[i]; j++) {
			cfg[j].regmap = regmap;

			ret = _clk_buf_dts_init_internal(node, j);
			if (ret)
				goto pmic_dts_fail;
		}

	}

	return 0;

no_compatible:
	pr_info("%s can't find compatible node %ld\n",
			__func__, PTR_ERR(node));
	return PTR_ERR(node);
no_property:
	pr_info("%s can't find property %d\n",
			__func__, ret);
	return ret;
pmic_dts_fail:
	kfree(cfg);
no_mem:
	pr_info("%s can't allocate memory %d\n",
			__func__, ret);
	return -ENOMEM;
}
#else /* !CONFIG_OF */
static int _clk_buf_dts_init_internal(struct device_node *node, int idx)
{
	return 0;
}

int clk_buf_dts_init(struct platform_device *pdev)
{
	return 0;
}
#endif

bool clk_buf_get_bringup_sta(void)
{
	if (is_clkbuf_bringup)
		pr_info("%s: skipped for bring up\n", __func__);

	return is_clkbuf_bringup;
}

static void _clk_buf_set_bringup_sta(bool enable)
{
	is_clkbuf_bringup = enable;
}

void clk_buf_get_bringup_node(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;

	if (!get_bringup_state_done) {
		const char *str;
		int ret = 0;

		ret = of_property_read_string(node,
			"mediatek,bring-up", &str);
		if (ret || (!strcmp(str, "enable"))) {
			pr_info("[%s]: bring up enable\n",
				__func__);
			_clk_buf_set_bringup_sta(true);
		} else {
			_clk_buf_set_bringup_sta(false);
		}

		get_bringup_state_done = true;
	}
}

int clk_buf_xo_init(void)
{
	int val = 0;
	int ret = 0;
	int i = 0;

	ret = _clk_buf_get_xo_num();
	if (ret)
		return ret;

	/* save setting after init done */
	xo_name = kcalloc(xo_num, sizeof(*xo_name), GFP_KERNEL);

	/* init exist xo name and index */
	_clk_buf_xo_match_idx_init();
	for (i = 0; i < xo_num; i++) {
		xo_name[i] = kzalloc(sizeof(char) * XO_NAME_LEN, GFP_KERNEL);
		ret = _clk_buf_xo_get_name(i, xo_name[i]);
		if (ret)
			continue;

		if (_clk_buf_xo_match_idx_set(xo_name[i], i))
			return CLK_BUF_FAIL;
	}

	/* set disable flag to unused external hw */
	if (_get_ufs_dev_state() != DEV_ON)
		CLK_BUF_STATUS[xo_match_idx[XO_EXT]] =
				CLOCK_BUFFER_DISABLE;

	if (_get_nfc_dev_state() != DEV_ON)
		CLK_BUF_STATUS[xo_match_idx[XO_NFC]] =
				CLOCK_BUFFER_DISABLE;

	/* save xo init setting & disable unused xo buffer */
	for (i = 0; i < XO_NUMBER; i++) {
		int idx = xo_match_idx[i];

		if (!_chk_xo_cond(idx))
			continue;

		if (CLK_BUF_STATUS[idx] != CLOCK_BUFFER_DISABLE) {
			xo_mode_init[idx] = _clk_buf_mode_get(idx);
		} else {
			_clk_buf_ctrl_internal(i, CLK_BUF_OFF);
			pmic_clk_buf_swctrl[idx] = CLK_BUF_SW_DISABLE;
		}
	}

	clkbuf_read(PWRAP_DCXO_EN, 0, &val);
	pwrap_dcxo_en_init = val;

	return ret;
}

int clkbuf_hw_is_ready(void)
{
	if (!clkbuf_pmic_op_done)
		return CLK_BUF_NOT_READY;

	return CLK_BUF_OK;
}

