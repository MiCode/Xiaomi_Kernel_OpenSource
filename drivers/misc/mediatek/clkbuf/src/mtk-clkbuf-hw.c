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
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/mt6397/core.h>

#include <mtk-clkbuf-bridge.h>
#include <mtk_clkbuf_ctl.h>
#include "mtk_clkbuf_pmic.h"
#include <mtk_clkbuf_common.h>
#if defined(CONFIG_MTK_UFS_SUPPORT)
#include "ufs-mtk.h"
#endif

#define CLKBUF_STATUS_INFO_SIZE	2048

#define CONN_EN_BIT		(cfg[PWRAP_CONN_EN].bit[0])
#define NFC_EN_BIT		(cfg[PWRAP_NFC_EN].bit[0])
#define DCXO_CONN_ENABLE	(0x1 << CONN_EN_BIT)
#define DCXO_NFC_ENABLE		(0x1 << NFC_EN_BIT)

/* TODO: set this flag to false after driver is ready */
static bool is_clkbuf_bringup;
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

static char XO_M_NAME[XO_NUMBER][MODE_M_NUM][20] = {
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
	[XO_SW_EN] = {"pmic-xo-en", 7, 0, 0x1, 0},
	[DCXO_CW] = {"pmic-dcxo-cw", 20, 0, 0xffff, 0x2},
	[XO_HW_SEL] = {"pmic-xo-mode", 7, 0, 0x3, 0},

	[BBL_SW_EN] = {"pmic-bblpm-sw", 1, 0, 0x1, 0},

	[MISC_SRCLKENI_EN] = {"pmic-srclkeni3", 1, 0, 0x1, 0},

	[PWRAP_DCXO_EN] = {"pwrap-dcxo-en", 1, 0, 0xffff, 0},
	[PWRAP_CONN_EN] = {"pwrap-dcxo-en", 1, 2, 0x1, 0},
	[PWRAP_NFC_EN] = {"pwrap-dcxo-en", 1, 4, 0x1, 0},
	[PWRAP_CONN_CFG] = {"pwrap-dcxo-cfg", 4, 0, 0xffff, 0x4},
	[PWRAP_NFC_CFG] = {"pwrap-dcxo-cfg", 4, 1, 0xffff, 0x4},

	[SPM_MD_PWR_STA] = {"spm-pwr-status", 1, 0, 0x1, 0},
	[SPM_CONN_PWR_STA] = {"spm-pwr-status", 1, 2, 0x1, 0},
};

static const char *base_n[REGMAP_NUM] = {"pmic", "pwrap", "sleep"};
struct pmic_clkbuf_op *pmic_op;

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

static bool check_pmic_clkbuf_op(void)
{
	if (!pmic_op) {
		pr_info("[%s]: pmic operation not registered!\n", __func__);
		return false;
	}
	return true;
}

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
	u32 mask;

	val <<= cfg[dts].bit[id];
	mask = clkbuf_dts[dts].mask << cfg[dts].bit[id];

	regmap_update_bits(cfg[dts].regmap,
			cfg[dts].ofs[id],
			mask,
			val);

	regmap_read(cfg[dts].regmap, cfg[dts].ofs[id], &val);
}

static bool _clk_buf_get_init_sta(void)
{
	return is_clkbuf_initiated;
}

static void _clk_buf_set_init_sta(bool done)
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

static unsigned int _clk_buf_mode_get(enum clk_buf_id id)
{
	unsigned int val = 0;

	if (CLK_BUF_STATUS[id] != CLOCK_BUFFER_DISABLE)
		clkbuf_read(XO_HW_SEL, id, &val);

	return val;
}

static void _clk_buf_mode_set(enum clk_buf_id id, unsigned int mode)
{
	unsigned int val = 0;

	val = _clk_buf_mode_get(id);

	if (mode > val)
		clkbuf_set(XO_HW_SEL, id, mode - val);
	else if (mode < val)
		clkbuf_clr(XO_HW_SEL, id, val - mode);
	else
		pr_debug("already set mode as requested\n");
}

static u32 _clk_buf_en_get(enum clk_buf_id id)
{
	u32 onoff = 0;

	if (CLK_BUF_STATUS[id] != CLOCK_BUFFER_DISABLE)
		clkbuf_read(XO_SW_EN, id, &onoff);

	return onoff;
}

static void _clk_buf_en_set(enum clk_buf_id id, bool onoff)
{
	if (onoff)
		clkbuf_set(XO_SW_EN, id, 0x1);
	else
		clkbuf_clr(XO_SW_EN, id, 0x1);
}

static enum dev_sta _get_nfc_dev_state(void)
{
	pr_info("%s: NFC support: %d\n", __func__, NFC_CLKBUF_SUPPORT);
#if defined(CONFIG_MTK_CLKBUF_NFC)
	return DEV_ON;
#else
	return DEV_NOT_SUPPORT;
#endif
}

static enum dev_sta _get_ufs_dev_state(void)
{
#if defined(CONFIG_SCSI_UFS_MEDIATEK)
	return DEV_ON;
#endif
	return DEV_NOT_SUPPORT;
}

#if BBLPM_SUPPORT
static int _clk_buf_set_bblpm_hw_en(bool on)
{
	if (!_get_bblpm_support()) {
		pr_info("[%s]: bblpm not support, continue without\n",
			__func__);
		return 0;
	}

	if (check_pmic_clkbuf_op())
		pmic_op->pmic_clk_buf_bblpm_hw_en(on);

	return 0;
}

static void _clk_buf_set_bblpm_hw_msk(enum clk_buf_id id, bool onoff)
{
	if (!_get_bblpm_support()) {
		pr_info("[%s]: bblpm not support, continue without\n",
			__func__);
		return;
	}

	if (id < 0 || id >= CLK_BUF_INVALID) {
		pr_info("[%s]: %s isn't support hw bblpm\n",
			__func__, XO_NAME[id]);
		return;
	}

	if (CLK_BUF_STATUS[id] == CLOCK_BUFFER_DISABLE) {
		pr_info("[%s]: %s isn't enabled\n", __func__, XO_NAME[id]);
		return;
	}

	mutex_lock(&clk_buf_ctrl_lock);
	if (check_pmic_clkbuf_op())
		pmic_op->pmic_clk_buf_set_bblpm_hw_msk(id, onoff);
	mutex_unlock(&clk_buf_ctrl_lock);
	return;
}

static void _clk_buf_get_enter_bblpm_cond(u32 *bblpm_cond)
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
	if (val || pmic_clk_buf_swctrl[XO_WCN])
		(*bblpm_cond) |= BBLPM_WCN;

	val = _clk_buf_en_get(CLK_BUF_NFC);
	if (val || pmic_clk_buf_swctrl[XO_NFC])
		(*bblpm_cond) |= BBLPM_NFC;

	pr_info("%s: bblpm condition: 0x%x\n", __func__, *bblpm_cond);
}

static void _clk_buf_get_bblpm_en(u32 *stat)
{
	if (check_pmic_clkbuf_op())
		pmic_op->pmic_clk_buf_get_bblpm_en(stat);

	clkbuf_read(BBL_SW_EN, 0, &(stat[1]));

	pr_info("%s: bblpm auxout en_stat(%d)\n", __func__, stat[0]);
	pr_info("%s: bblpm sw en(%d)\n", __func__, stat[1]);
}

static int _clk_buf_get_bblpm_en_stat(void)
{
	u32 stat[2];

	_clk_buf_get_bblpm_en(stat);

	return stat[0];
}

static int _clk_buf_ctrl_bblpm_sw(bool enable)
{
	if (!_get_bblpm_support()) {
		pr_info("[%s]: bblpm not support, continue without\n",
			__func__);
		return 0;
	}
	_clk_buf_set_bblpm_hw_en(false);

	/* get set/clr register offset */

	if (enable)
		clkbuf_set(BBL_SW_EN, 0, 0x1);
	else
		clkbuf_clr(BBL_SW_EN, 0, 0x1);

	if (_clk_buf_get_bblpm_en_stat() != enable) {
		pr_info("manual set bblpm fail\n");
		return -1;
	}

	return 0;
}

static int _clk_buf_bblpm_init(void)
{
	if (!_get_bblpm_support()) {
		pr_info("[%s]: bblpm not support, continue without\n",
			__func__);
	}
	_clk_buf_set_bblpm_hw_msk(CLK_BUF_BB_MD, true);
	_clk_buf_set_bblpm_hw_msk(CLK_BUF_CONN, false);
	_clk_buf_set_bblpm_hw_msk(CLK_BUF_NFC, false);
	_clk_buf_set_bblpm_hw_msk(CLK_BUF_RF, false);
	_clk_buf_set_bblpm_hw_msk(CLK_BUF_UFS, false);

	if (CLK_BUF_STATUS[XO_CEL] == CLOCK_BUFFER_DISABLE)
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

static void _clk_buf_get_enter_bblpm_cond(u32 *bblpm_cond)
{
	(*bblpm_cond) |= BBLPM_SKIP;
}

static void _clk_buf_get_bblpm_en(u32 *stat)
{
	pr_info("not support bblpm\n");
}
static int _clk_buf_get_bblpm_en_stat(void)
{
	pr_info("not support bblpm\n");

	return -1;
}
static int _clk_buf_ctrl_bblpm_sw(bool enable)
{
	pr_info("not support bblpm\n");

	return -1;
}

static int _clk_buf_bblpm_init(void)
{
	pr_info("not support bblpm\n");

	return -1;
}
#endif

/* for spm driver use */
bool _clk_buf_get_flight_mode(void)
{
	return is_flightmode_on;
}

/* for ccci driver to notify this */
void _clk_buf_set_flight_mode(bool on)
{
	is_flightmode_on = on;

	if (is_flightmode_on)
		_clk_buf_set_bblpm_hw_en(true);
	else
		_clk_buf_set_bblpm_hw_en(false);
}

static int _clk_buf_ctrl_internal(enum clk_buf_id id,
		enum cmd_type cmd)
{
	short ret = 0, no_lock = 0;
	int val;

	if (!_clk_buf_get_init_sta())
		return -1;

	/* we should not turn off SOC 26M */
	if (id < 0 || id >= CLK_BUF_INVALID ||
			CLK_BUF_STATUS[id] == CLOCK_BUFFER_DISABLE) {
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

		_clk_buf_mode_set(id, BUF_MAN_M);
		_clk_buf_en_set(id, false);
		pmic_clk_buf_swctrl[id] = 0;
		break;
	case CLK_BUF_ON:
		if (id == CLK_BUF_CONN)
			clkbuf_update(PWRAP_CONN_EN, 0, 0);
		else if (id == CLK_BUF_NFC)
			clkbuf_update(PWRAP_NFC_EN, 0, 0);

		_clk_buf_mode_set(id, BUF_MAN_M);
		_clk_buf_en_set(id, true);
		pmic_clk_buf_swctrl[id] = 1;
		break;
	case CLK_BUF_ENBB:
		_clk_buf_mode_set(id, EN_BB_M);
		break;
	case CLK_BUF_SIG:
		_clk_buf_mode_set(id, SIG_CTRL_M);
		break;
	case CLK_BUF_COBUF:
		_clk_buf_mode_set(id, CO_BUF_M);
		break;
	case CLK_BUF_INIT_SETTING:
		if (id == CLK_BUF_CONN) {
			val = pwrap_dcxo_en_init >> CONN_EN_BIT;
			clkbuf_update(PWRAP_CONN_EN, 0, val);
		} else if (id == CLK_BUF_NFC) {
			val = pwrap_dcxo_en_init >> NFC_EN_BIT;
			clkbuf_update(PWRAP_NFC_EN, 0, val);
		}

		_clk_buf_mode_set(id, xo_mode_init[id]);

		break;
	default:
		ret = -1;
		pr_info("%s: id=%d isn't supported\n", __func__, id);
		break;
	}

	clkbuf_read(PWRAP_DCXO_EN, 0, &val);
	pr_debug("%s: id=%d, cmd=%d, DCXO_EN = 0x%x\n",
		__func__, id, cmd, val);

	if (!no_lock)
		mutex_unlock(&clk_buf_ctrl_lock);

	return ret;
}

static void _clk_buf_get_drv_curr(u32 *drv_curr)
{
	if (check_pmic_clkbuf_op())
		pmic_op->pmic_clk_buf_get_drv_curr(drv_curr);
}

static void _clk_buf_set_manual_drv_curr(u32 *drv_curr_vals)
{
	u32 drv_curr[XO_NUMBER] = {0};

	pmic_op->pmic_clk_buf_set_drv_curr(drv_curr_vals);

	_clk_buf_get_drv_curr(drv_curr);
}

static void _pmic_clk_buf_ctrl(enum CLK_BUF_TYPE *status)
{
	u32 i;

	if (!_clk_buf_get_init_sta())
		return;

	for (i = 0; i < XO_NUMBER; i++)
		_clk_buf_ctrl_internal(i, status[i] % 2);

	pr_info("%s clk_buf_swctrl=[%u %u %u %u 0 0 %u]\n",
		__func__, status[XO_SOC], status[XO_WCN],
		status[XO_NFC], status[XO_CEL], status[XO_EXT]);
}

static bool _clk_buf_ctrl(enum clk_buf_id id, bool onoff)
{
	short ret = true, no_lock = 0;

	if (!_clk_buf_get_init_sta())
		return false;

	pr_debug("%s: id=%d, onoff=%d\n",
		__func__, id, onoff);

	if (preempt_count() > 0 || irqs_disabled()
		|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&clk_buf_ctrl_lock);

	if (id < 0 || id >= CLK_BUF_INVALID) {
		ret = false;
		pr_info("%s: id=%d isn't supported\n", __func__, id);
		goto wrong_input;
	}

	if (CLK_BUF_STATUS[id] != CLOCK_BUFFER_SW_CONTROL) {
		ret = false;
		pr_info("%s: id=%d isn't controlled by SW\n",
			__func__, id);
		goto wrong_input;
	} else {
		if (_clk_buf_mode_get(id) == BUF_MAN_M)
			_clk_buf_ctrl_internal(id, onoff);

		pmic_clk_buf_swctrl[id] = onoff;
	}

wrong_input:

	if (!no_lock)
		mutex_unlock(&clk_buf_ctrl_lock);

	return ret;
}

static int _clk_buf_dump_dws_log(char *buf)
{
	u32 len = strlen(buf);
	u32 i = 0;

	for (i = 0; i < XO_NUMBER; i++)
		len += snprintf(buf+len, PAGE_SIZE-len,
				"CLK_BUF%d_STATUS=%d\n",
				i+1, CLK_BUF_STATUS[i]);

	if (_get_impedance_support()) {
		for (i = 0; i < XO_NUMBER; i++)
			len += snprintf(buf+len, PAGE_SIZE-len,
				"CLK_BUF%u_OUTPUT_IMPEDANCE=%u\n",
				i + 1, CLK_BUF_OUTPUT_IMPEDANCE[i]);
	}

	if (_get_desense_support()) {
		for (i = 0; i < XO_NUMBER; i++)
			len += snprintf(buf+len, PAGE_SIZE-len,
				"CLK_BUF%u_CONTROLS_DESENSE=%u\n",
				i + 1, CLK_BUF_CONTROLS_DESENSE[i]);
	}

	if (_get_drv_curr_support()) {
		for (i = 0; i < XO_NUMBER; i++)
			len += snprintf(buf+len, PAGE_SIZE-len,
				"CLK_BUF%u_DRIVING_CURRENT=%u\n",
				i + 1, CLK_BUF_DRIVING_CURRENT[i]);
	}

	pr_info("%s: %s\n", __func__, buf);

	return len;
}

static int _clk_buf_dump_misc_log(char *buf)
{
	u32 len = strlen(buf);
	u32 val = 0;
	u32 i = 0;

	for (i = 0; i < clkbuf_dts[DCXO_CW].len; i++) {
		clkbuf_read(DCXO_CW, i, &val);

		len += snprintf(buf+len, PAGE_SIZE-len,
			"DCXO_CW%02d=0x%x\n", i, val);
	}

	for (i = MISC_START; i < MISC_END; i++) {
		clkbuf_read(i, 0, &val);
		len += snprintf(buf+len, PAGE_SIZE-len, "%s(%s)=0x%x\n",
				clkbuf_dts[i].prop,
				((i - 1) % 2) ? "en" : "op_mode",
				val);
	}

	if (check_pmic_clkbuf_op())
		len = pmic_op->pmic_clk_buf_dump_misc_log(buf);

	return len;
}

static void _clk_buf_get_xo_en(u32 *stat)
{
	if (check_pmic_clkbuf_op())
		pmic_op->pmic_clk_buf_get_xo_en(stat);
}

static ssize_t _clk_buf_show_status_info_internal(char *buf)
{
	u32 stat[XO_NUMBER];
	u32 drv_curr[XO_NUMBER];
	u32 bblpm_stat[2];
	u32 buf_mode;
	u32 buf_en;
	u32 val[4];
	int len = 0;
	int i;

	_clk_buf_get_xo_en(stat);
	_clk_buf_get_drv_curr(drv_curr);

	for (i = 0; i < XO_NUMBER; i++)
		len += snprintf(buf+len, PAGE_SIZE-len,
			"%s   SW(1)/HW(2) CTL: %d, Dis(0)/En(1): %d, RS: %u\n",
			XO_NAME[i], CLK_BUF_STATUS[i], pmic_clk_buf_swctrl[i],
			stat[i]);

	len += snprintf(buf+len, PAGE_SIZE-len,
			".********** clock buffer debug info **********\n");

	len += _clk_buf_dump_misc_log(buf);
	len += _clk_buf_dump_dws_log(buf);

	for (i = 0; i < XO_NUMBER; i++) {
		if (CLK_BUF_STATUS[i] == CLOCK_BUFFER_DISABLE)
			continue;

		buf_mode = _clk_buf_mode_get(i);

		buf_en = _clk_buf_en_get(i);

		len += snprintf(buf+len, PAGE_SIZE-len,
			"buf%02dmode = %s(0x%x), en_m = %s(%d)\n", i,
			XO_M_NAME[i][buf_mode], buf_mode,
			buf_en ? "sw enable" : "sw disable", buf_en);
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
	_clk_buf_get_bblpm_en(bblpm_stat);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"DCXO_ENABLE=0x%x, flight mode = %d bblpm = %d\n",
			val[0], _clk_buf_get_flight_mode(), bblpm_stat[0]);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"PMIC_CLKBUF_DRV_CURR (1/2/3/4/6/7)= %u %u %u %u %u %u\n",
			drv_curr[XO_SOC], drv_curr[XO_WCN],
			drv_curr[XO_NFC], drv_curr[XO_CEL],
			drv_curr[XO_PD], drv_curr[XO_EXT]);

	len += snprintf(buf+len, PAGE_SIZE-len,
			".********** clock buffer command help **********\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"PMIC switch on/off: echo pmic en1 en2 en3 en4 en5 ");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"en6 en7 > /sys/kernel/clk_buf/clk_buf_ctrl\n");
	return len;
}

static int _clk_buf_get_xo_en_sta(enum xo_id id)
{
	u32 stat[XO_NUMBER];

	_clk_buf_get_xo_en(stat);

	return stat[id];
}

static void _clk_buf_show_status_info(void)
{
	int len;
	char *buf, *str;

	buf = vmalloc(CLKBUF_STATUS_INFO_SIZE);
	if (buf) {
		len = _clk_buf_show_status_info_internal(buf);
		while ((str = strsep(&buf, ".")) != NULL)
			pr_info("%s\n", str);

		vfree(buf);
	} else
		pr_info("%s: allocate memory fail\n", __func__);
}

#ifdef CONFIG_PM
static ssize_t clk_buf_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 clk_buf_en[XO_NUMBER];
	u32 pwrap_dcxo_en = 0;
	u32 val;
	u32 i;
	char cmd[32];

	if (sscanf(buf, "%31s %x %x %x %x %x %x %x", cmd, &clk_buf_en[XO_SOC],
		&clk_buf_en[XO_WCN], &clk_buf_en[XO_NFC], &clk_buf_en[XO_CEL],
		&clk_buf_en[XO_AUD], &clk_buf_en[XO_PD], &clk_buf_en[XO_EXT])
		!= (XO_NUMBER + 1))
		return -EPERM;

	if (!strcmp(cmd, "pmic")) {
		for (i = 0; i < XO_NUMBER; i++)
			pmic_clk_buf_swctrl[i] = clk_buf_en[i];

		_pmic_clk_buf_ctrl(pmic_clk_buf_swctrl);

		return count;
	} else if (!strcmp(cmd, "pwrap")) {
		mutex_lock(&clk_buf_ctrl_lock);

		for (i = 0; i < XO_NUMBER; i++) {
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
		}

		clkbuf_write(PWRAP_DCXO_EN, 0, pwrap_dcxo_en);
		clkbuf_read(PWRAP_DCXO_EN, 0, &val);
		pr_info("%s: DCXO_ENABLE=0x%x, pwrap_dcxo_en=0x%x\n",
				__func__, val, pwrap_dcxo_en);

		mutex_unlock(&clk_buf_ctrl_lock);

		return count;
	} else if (!strcmp(cmd, "drvcurr")) {
		mutex_lock(&clk_buf_ctrl_lock);
		_clk_buf_set_manual_drv_curr(clk_buf_en);
		mutex_unlock(&clk_buf_ctrl_lock);
		return count;
	} else {
		return -EINVAL;
	}
}

static ssize_t clk_buf_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len;

	len = _clk_buf_show_status_info_internal(buf);

	return len;
}

static int _clk_buf_debug_internal(char *cmd, enum clk_buf_id id)
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
		ret = _clk_buf_ctrl(id, true);
	else if (!strcmp(cmd, "TEST_OFF"))
		ret = _clk_buf_ctrl(id, false);
	else
		ret = -1;

	return ret;
}

static ssize_t clk_buf_debug_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[32] =  {'\0'}, xo_user[11] = {'\0'};
	u32 i;

	if ((sscanf(buf, "%31s %10s", cmd, xo_user) != 2))
		return -EPERM;

	for (i = 0; i < XO_NUMBER; i++)
		if (!strcmp(xo_user, XO_NAME[i])) {
			if (_clk_buf_debug_internal(cmd, i) < 0)
				goto ERROR_CMD;
			else if (!strcmp(cmd, "INIT"))
				clkbuf_debug = false;
			else
				clkbuf_debug = true;
		}
	if (strcmp(xo_user, "0"))
		goto ERROR_CMD;

	return count;
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
	u32 onoff;
	int ret = 0;

	if ((kstrtouint(buf, 10, &onoff))) {
		pr_info("bblpm input error\n");
		return -EPERM;
	}

	if (onoff == 2)
		_clk_buf_set_bblpm_hw_en(true);
	else if (onoff == 1)
		ret = _clk_buf_ctrl_bblpm_sw(true);
	else if (onoff == 0)
		ret = _clk_buf_ctrl_bblpm_sw(false);

	if (ret)
		return ret;

	return count;
}

static ssize_t clk_buf_bblpm_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	u32 xo_stat[XO_NUMBER];
	u32 bblpm_stat[2];
	u32 bblpm_cond = 0;
	int len = 0;

	_clk_buf_get_xo_en(xo_stat);
	_clk_buf_get_bblpm_en(bblpm_stat);
	_clk_buf_get_enter_bblpm_cond(&bblpm_cond);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"EN_STAT=%d %d %d %d %d %d\n",
		xo_stat[XO_SOC],
		xo_stat[XO_WCN],
		xo_stat[XO_NFC],
		xo_stat[XO_CEL],
		xo_stat[XO_PD],
		xo_stat[XO_EXT]);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"bblpm en_stat(%d)\n",
		bblpm_stat[0]);

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

static int _clk_buf_fs_init(void)
{
	int r = 0;

	/* create /sys/kernel/clk_buf/xxx */
	r = sysfs_create_group(kernel_kobj, &clk_buf_attr_group);
	if (r)
		pr_notice("FAILED TO CREATE /sys/kernel/clk_buf (%d)\n", r);

	return r;
}
#else /* !CONFIG_PM */
static int _clk_buf_fs_init(void)
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
		CLK_BUF_OUTPUT_IMPEDANCE, XO_NUMBER);
	if (ret) {
		pr_info("[%s]: No impedance property read, continue without\n",
			__func__);
		_set_impedance_support(false);
	} else {
		_set_impedance_support(true);
	}

	ret = of_property_read_u32_array(node,
		"mediatek,clkbuf-controls-for-desense",
		CLK_BUF_CONTROLS_DESENSE, XO_NUMBER);
	if (ret) {
		pr_info("[%s]: No control desense property read, continue without\n",
			__func__);
		_set_desense_support(false);
	} else {
		_set_desense_support(true);
	}

	ret = of_property_read_u32_array(node,
		"mediatek,clkbuf-driving-current",
		CLK_BUF_DRIVING_CURRENT, XO_NUMBER);
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

static int _clk_buf_dts_init_internal(struct device_node *node, int idx)
{
	u32 interval = clkbuf_dts[idx].interval;
	u32 setclr = 0;
	u32 base;
	int ret = 0;
	int i, j;

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
			int val;

			val = base + (i * interval) + (setclr * interval * 2);
			cfg[idx].ofs[i] = val;
			cfg[idx].bit[i] = 0;

			for (j = 0; j < XO_NUMBER && idx == DCXO_CW; j++)
				if (cfg[XO_SW_EN].ofs[j] == val) {
					setclr++;
					break;
				}
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

static int _clk_buf_dts_init(struct platform_device *pdev)
{
	struct mt6397_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct device_node *node, *pmic_node;
	int start[] = {DCXO_START, PWRAP_START, SPM_START};
	int end[] = {DCXO_END, PWRAP_END, SPM_END};
	int ret;
	int i, j;

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,pmic_clock_buffer");
	if (IS_ERR_OR_NULL(node))
		goto no_compatible;

	pmic_node = of_find_compatible_node(NULL, NULL,
		"mediatek,clock_buffer");
	if (IS_ERR_OR_NULL(pmic_node))
		goto no_compatible;

	ret = get_pmic_clkbuf(pmic_node, &pmic_op);
	if (ret) {
		pr_info("[%s]: pmic op not found\n", __func__);
		goto no_pmic_op;
	}

	ret = of_property_read_u32_array(node,
		"mediatek,clkbuf-config",
		CLK_BUF_STATUS, XO_NUMBER);
	if (ret)
		goto no_property;

	_clk_buf_read_dts_misc_node(node);

	cfg = kzalloc(sizeof(struct reg_info) * DTS_NUM, GFP_KERNEL);
	if (!cfg)
		goto no_mem;

	for (i = 0; i < REGMAP_NUM; i++) {
		struct regmap *regmap;

		if (i == PMIC_R) {
			if (chip->regmap == NULL) {
				pr_info("%s: no pmic regmap\n", __func__);
				return -ENODEV;
			}
			regmap = chip->regmap;

			if (check_pmic_clkbuf_op())
				ret = pmic_op->pmic_clk_buf_dts_init(pmic_node,
								regmap);
			if (ret) {
				pr_info("[%s]: find pmic hw dependent dts property failed\n",
					__func__);
				goto pmic_dependent_property_failed;
			}
			pr_info("[%s]: pmic dts init done\n", __func__);
		} else {
			regmap = syscon_regmap_lookup_by_phandle(node,
						base_n[i]);
			if (IS_ERR(regmap)) {
				dev_err(&pdev->dev,
					"Cannot find %s controller: %ld\n",
					base_n[i],
					PTR_ERR(regmap));
				goto no_compatible;
			}
			pr_info("found %s regmap\n", base_n[i]);
		}

		for (j = start[i]; j < end[i]; j++) {
			cfg[j].regmap = regmap;
			if (i == PMIC_R)
				ret = _clk_buf_dts_init_internal(pmic_node, j);
			else
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
pmic_dependent_property_failed:
no_pmic_op:
	return -1;
}
#else /* !CONFIG_OF */
static int _clk_buf_dts_init_internal(struct device_node *node, int idx)
{
	return 0;
}

static int _clk_buf_dts_init(struct platform_device *pdev)
{
	return 0;
}
#endif

static bool _clk_buf_get_bringup_sta(void)
{
	if (is_clkbuf_bringup)
		pr_info("%s: skipped for bring up\n", __func__);

	return is_clkbuf_bringup;
}

static void _clk_buf_set_bringup_sta(bool enable)
{
	is_clkbuf_bringup = enable;
}

static void _clk_buf_xo_init(void)
{
	int val;
	int i;

	/* set disable flag to unused external hw */
	if (_get_ufs_dev_state() != DEV_ON)
		CLK_BUF_STATUS[XO_EXT] = CLOCK_BUFFER_DISABLE;

	if (_get_nfc_dev_state() != DEV_ON)
		CLK_BUF_STATUS[XO_NFC] = CLOCK_BUFFER_DISABLE;

	/* save setting after init done */
	for (i = 0; i < XO_NUMBER; i++) {
		if (CLK_BUF_STATUS[i] != CLOCK_BUFFER_DISABLE)
			clkbuf_read(XO_HW_SEL, i, &xo_mode_init[i]);
		else {
			_clk_buf_ctrl_internal(i, CLK_BUF_OFF);
			pmic_clk_buf_swctrl[i] = CLK_BUF_SW_DISABLE;
		}
	}

	clkbuf_read(PWRAP_DCXO_EN, 0, &val);
	pwrap_dcxo_en_init = val;
}

static struct clk_buf_op clkbuf_ctrl_ops = {
	.xo_init = _clk_buf_xo_init,
	.bblpm_init = _clk_buf_bblpm_init,
	.dts_init = _clk_buf_dts_init,
	.fs_init = _clk_buf_fs_init,
	.get_bringup_sta = _clk_buf_get_bringup_sta,
	.get_clkbuf_init_sta = _clk_buf_get_init_sta,
	.get_flight_mode = _clk_buf_get_flight_mode,
	.get_xo_sta = _clk_buf_get_xo_en_sta,
	.get_bblpm_enter_cond = _clk_buf_get_enter_bblpm_cond,
	.get_bblpm_sta = _clk_buf_get_bblpm_en_stat,
	.get_main_log = _clk_buf_show_status_info,
	.get_dws_log = _clk_buf_dump_dws_log,
	.get_misc_log = _clk_buf_dump_misc_log,
	.set_bringup_sta = _clk_buf_set_bringup_sta,
	.set_clkbuf_init_sta = _clk_buf_set_init_sta,
	.set_flight_mode = _clk_buf_set_flight_mode,
	.set_xo_sta = _clk_buf_ctrl,
	.set_xo_cmd = _clk_buf_ctrl_internal,
	.set_bblpm_sta = _clk_buf_ctrl_bblpm_sw,
	.set_bblpm_hw_mode = _clk_buf_set_bblpm_hw_en,
};

int clk_buf_hw_probe(struct platform_device *pdev)
{
	return mtk_register_clk_buf(&pdev->dev, &clkbuf_ctrl_ops);
}
