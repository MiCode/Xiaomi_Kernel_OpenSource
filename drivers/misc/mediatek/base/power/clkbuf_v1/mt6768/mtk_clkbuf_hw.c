/*
 * Copyright (C) 2018 MediaTek Inc.
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

/*
 * @file    mtk_clk_buf_hw.c
 * @brief   Driver for clock buffer control of each platform
 *
 */

#include <mtk_spm.h>
#include <mtk_clkbuf_ctl.h>
#include <mtk_clkbuf_common.h>
#if defined(CONFIG_MTK_UFS_SUPPORT)
#include "ufs-mtk.h"
#endif

static void __iomem *pwrap_base;

#define PWRAP_REG(ofs)		(pwrap_base + ofs)

/* PMICWRAP Reg */
#define DCXO_ENABLE		PWRAP_REG(0x190)
#define DCXO_CONN_ADR0		PWRAP_REG(0x194)
#define DCXO_CONN_WDATA0	PWRAP_REG(0x198)
#define DCXO_CONN_ADR1		PWRAP_REG(0x19C)
#define DCXO_CONN_WDATA1	PWRAP_REG(0x1A0)
#define DCXO_NFC_ADR0		PWRAP_REG(0x1A4)
#define DCXO_NFC_WDATA0		PWRAP_REG(0x1A8)
#define DCXO_NFC_ADR1		PWRAP_REG(0x1AC)
#define DCXO_NFC_WDATA1		PWRAP_REG(0x1B0)

#define PMIC_DCXO_CW00		MT6358_DCXO_CW00
#define PMIC_DCXO_CW01		MT6358_DCXO_CW01
#define PMIC_DCXO_CW02		MT6358_DCXO_CW02
#define PMIC_DCXO_CW03		MT6358_DCXO_CW03
#define PMIC_DCXO_CW04		MT6358_DCXO_CW04
#define PMIC_DCXO_CW05		MT6358_DCXO_CW05
#define PMIC_DCXO_CW06		MT6358_DCXO_CW06
#define PMIC_DCXO_CW07		MT6358_DCXO_CW07
#define PMIC_DCXO_CW08		MT6358_DCXO_CW08
#define PMIC_DCXO_CW09		MT6358_DCXO_CW09
#define PMIC_DCXO_CW10		MT6358_DCXO_CW10
#define PMIC_DCXO_CW11		MT6358_DCXO_CW11
#define PMIC_DCXO_CW12		MT6358_DCXO_CW12
#define PMIC_DCXO_CW13		MT6358_DCXO_CW13
#define PMIC_DCXO_CW14		MT6358_DCXO_CW14
#define PMIC_DCXO_CW15		MT6358_DCXO_CW15
#define PMIC_DCXO_CW16		MT6358_DCXO_CW16
#define PMIC_DCXO_CW17		MT6358_DCXO_CW17
#define PMIC_DCXO_CW18		MT6358_DCXO_CW18
#define PMIC_DCXO_CW19		MT6358_DCXO_CW19
#define PMIC_DCXO_CW23		MT6358_DCXO_CW23

#define	DCXO_CONN_ENABLE	(0x1 << 1)
#define	DCXO_NFC_ENABLE		(0x1 << 0)

#define PMIC_REG_MASK		0xFFFF
#define PMIC_REG_SHIFT		0

/* TODO: BBLPM HW mode */
#define XO_BB_LPM_HW		(0x1 << 0)
#define XO_BUF2_BBLPM_EN_MASK	(0x1 << 2)
#define XO_BUF3_BBLPM_EN_MASK	(0x1 << 3)
#define XO_BUF4_BBLPM_EN_MASK	(0x1 << 4)
#define XO_BUF7_BBLPM_EN_MASK	(0x1 << 7)

/* TODO: marked this after driver is ready */
/* #define CLKBUF_BRINGUP */

/* #define CLKBUF_CONN_SUPPORT_CTRL_FROM_I1 */

#define BUF_MAN_M				0
#define EN_BB_M					1
#define SIG_CTRL_M				2
#define CO_BUF_M				3

#define BBLPM_RETRY 6

#define CLKBUF_STATUS_INFO_SIZE 2048

static unsigned int xo2_mode_set[4] = {WCN_EN_M,
			WCN_EN_BB_G,
			WCN_SRCLKEN_CONN,
			WCN_BUF24_EN};
static unsigned int xo3_mode_set[4] = {NFC_EN_M,
			NFC_EN_BB_G,
			NFC_CLK_SEL_G,
			NFC_BUF234_EN};
static unsigned int xo4_mode_set[4] = {CEL_EN_M,
			CEL_EN_BB_G,
			CEL_CLK_SEL_G,
			CEL_BUF24_EN};
static unsigned int xo7_mode_set[4] = {EXT_EN_M,
			EXT_EN_BB_G,
			EXT_CLK_SEL_G,
			EXT_BUF247_EN};

static unsigned int xo_mode_init[XO_NUMBER];

/* TODO: enable BBLPM if its function is ready (set as 1) */
/* #define CLK_BUF_HW_BBLPM_EN */
static unsigned int bblpm_switch = 2;

static unsigned int pwrap_dcxo_en_init;

static unsigned int clk_buf7_ctrl = true;

static unsigned int CLK_BUF1_STATUS = CLOCK_BUFFER_HW_CONTROL,
		    CLK_BUF2_STATUS = CLOCK_BUFFER_SW_CONTROL,
		    CLK_BUF3_STATUS = CLOCK_BUFFER_SW_CONTROL,
		    CLK_BUF4_STATUS = CLOCK_BUFFER_HW_CONTROL,
		    CLK_BUF5_STATUS = CLOCK_BUFFER_DISABLE,
		    CLK_BUF6_STATUS = CLOCK_BUFFER_DISABLE,
		    CLK_BUF7_STATUS = CLOCK_BUFFER_SW_CONTROL;
static int PMIC_CLK_BUF1_DRIVING_CURR = CLK_BUF_DRIVING_CURR_1,
	   PMIC_CLK_BUF2_DRIVING_CURR = CLK_BUF_DRIVING_CURR_1,
	   PMIC_CLK_BUF3_DRIVING_CURR = CLK_BUF_DRIVING_CURR_1,
	   PMIC_CLK_BUF4_DRIVING_CURR = CLK_BUF_DRIVING_CURR_1,
	   PMIC_CLK_BUF5_DRIVING_CURR = CLK_BUF_DRIVING_CURR_1,
	   PMIC_CLK_BUF6_DRIVING_CURR = CLK_BUF_DRIVING_CURR_1,
	   PMIC_CLK_BUF7_DRIVING_CURR = CLK_BUF_DRIVING_CURR_1;

static u8 clkbuf_drv_curr_auxout[CLKBUF_NUM];
static u8 xo_en_stat[CLKBUF_NUM];
static u8 xo_bb_lpm_en_stat;

#ifndef CLKBUF_BRINGUP
static enum CLK_BUF_SWCTRL_STATUS_T  pmic_clk_buf_swctrl[CLKBUF_NUM] = {
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_DISABLE,
	CLK_BUF_SW_DISABLE,
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_DISABLE,
	CLK_BUF_SW_DISABLE,
	CLK_BUF_SW_ENABLE
};
#else /* For Bring-up */
static enum CLK_BUF_SWCTRL_STATUS_T  pmic_clk_buf_swctrl[CLKBUF_NUM] = {
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_DISABLE,
	CLK_BUF_SW_ENABLE,
	CLK_BUF_SW_ENABLE
};
#endif

static u32 dcxo_dbg_read_auxout(u16 sel);

static void pmic_clk_buf_ctrl_ext(short on)
{
	if (on)
		pmic_config_interface(PMIC_DCXO_CW11_SET_ADDR, 0x1,
				      PMIC_XO_EXTBUF7_EN_M_MASK,
				      PMIC_XO_EXTBUF7_EN_M_SHIFT);
	else
		pmic_config_interface(PMIC_DCXO_CW11_CLR_ADDR, 0x1,
				      PMIC_XO_EXTBUF7_EN_M_MASK,
				      PMIC_XO_EXTBUF7_EN_M_SHIFT);
}

void clk_buf_ctrl_bblpm_hw(short on)
{
	u32 bblpm_sel = 0, aac_sta = 0, i = 0;

	if (!is_pmic_clkbuf)
		return;

	mutex_lock(&clk_buf_ctrl_lock);
	if (on) {
		for (i = 0; i < BBLPM_RETRY; i++) {
			aac_sta = dcxo_dbg_read_auxout(51);
			/* aac status bit[4:1] means doing AAC */
			if (!(aac_sta & 0x1E))
				break;

			mdelay(1);
			pr_debug("retry=%d, aac_sta=0x%x\n", i, aac_sta);
		}

		if (i < BBLPM_RETRY) {
			pmic_config_interface(PMIC_XO_BB_LPM_CEL_ADDR, 0x1,
					      PMIC_XO_BB_LPM_CEL_MASK,
					      PMIC_XO_BB_LPM_CEL_SHIFT);
			pmic_read_interface_nolock(PMIC_XO_BB_LPM_CEL_ADDR,
				&bblpm_sel, PMIC_REG_MASK, PMIC_REG_SHIFT);
			pr_debug("%s(%u): bblpm_sel=0x%x\n", __func__,
				(on ? 1 : 0), bblpm_sel);
		} else
			pr_notice("%s: NOT set bblpm because aac_sta=0x%x\n",
				__func__, aac_sta);
	} else {
		pmic_config_interface(PMIC_XO_BB_LPM_CEL_ADDR, 0x0,
				      PMIC_XO_BB_LPM_CEL_MASK,
				      PMIC_XO_BB_LPM_CEL_SHIFT);
		pmic_read_interface_nolock(PMIC_XO_BB_LPM_CEL_ADDR, &bblpm_sel,
					PMIC_REG_MASK, PMIC_REG_SHIFT);
		pr_debug("%s(%u): bblpm_sel=0x%x\n", __func__, (on ? 1 : 0),
			bblpm_sel);
	}
	mutex_unlock(&clk_buf_ctrl_lock);
}

void clk_buf_control_bblpm(bool on)
{
#ifdef CLKBUF_USE_BBLPM
#ifndef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	u32 cw00 = 0;

	if (!is_clkbuf_initiated || !is_pmic_clkbuf)
		return;

	if (on) /* FPM -> BBLPM */
		pmic_config_interface_nolock(PMIC_DCXO_CW00_SET_ADDR, 0x1,
				      PMIC_XO_BB_LPM_EN_MASK,
				      PMIC_XO_BB_LPM_EN_SHIFT);
	else /* BBLPM -> FPM */
		pmic_config_interface_nolock(PMIC_DCXO_CW00_CLR_ADDR, 0x1,
				      PMIC_XO_BB_LPM_EN_MASK,
				      PMIC_XO_BB_LPM_EN_SHIFT);

	pmic_read_interface_nolock(PMIC_DCXO_CW00, &cw00,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);

	pr_debug("%s(%u): CW00=0x%x\n", __func__, (on ? 1 : 0), cw00);
#endif
#endif
}

#ifdef CLKBUF_USE_BBLPM
static void clk_buf_ctrl_bblpm_mask(enum clk_buf_id id, bool onoff)
{
	if (!is_pmic_clkbuf && (bblpm_switch != 2))
		return;

	mutex_lock(&clk_buf_ctrl_lock);

	switch (id) {
	case CLK_BUF_BB_MD:
		pmic_config_interface(PMIC_XO_BUF1_BBLPM_EN_MASK_ADDR, onoff,
				      PMIC_XO_BUF1_BBLPM_EN_MASK_MASK,
				      PMIC_XO_BUF1_BBLPM_EN_MASK_SHIFT);
		break;
	case CLK_BUF_CONN:
		pmic_config_interface(PMIC_XO_BUF2_BBLPM_EN_MASK_ADDR, onoff,
				      PMIC_XO_BUF2_BBLPM_EN_MASK_MASK,
				      PMIC_XO_BUF2_BBLPM_EN_MASK_SHIFT);
		break;
	case CLK_BUF_NFC:
		pmic_config_interface(PMIC_XO_BUF3_BBLPM_EN_MASK_ADDR, onoff,
				      PMIC_XO_BUF3_BBLPM_EN_MASK_MASK,
				      PMIC_XO_BUF3_BBLPM_EN_MASK_SHIFT);
		break;
	case CLK_BUF_RF:
		pmic_config_interface(PMIC_XO_BUF4_BBLPM_EN_MASK_ADDR, onoff,
				      PMIC_XO_BUF4_BBLPM_EN_MASK_MASK,
				      PMIC_XO_BUF4_BBLPM_EN_MASK_SHIFT);
		break;
	case CLK_BUF_UFS:
		pmic_config_interface(PMIC_XO_BUF7_BBLPM_EN_MASK_ADDR, onoff,
				      PMIC_XO_BUF7_BBLPM_EN_MASK_MASK,
				      PMIC_XO_BUF7_BBLPM_EN_MASK_SHIFT);
		break;
	default:
		pr_info("%s: id=%d isn't supported\n", __func__, id);
		break;
	}

	mutex_unlock(&clk_buf_ctrl_lock);
}
#endif

/*
 * Baseband Low Power Mode (BBLPM) for PMIC clkbuf
 * Condition: XO_CELL/XO_NFC/XO_WCN/XO_EXT OFF
 * Caller: deep idle, SODI2.5
 * Return: 0 if all conditions are matched & ready to enter BBLPM
 */
u32 clk_buf_bblpm_enter_cond(void)
{
	u32 bblpm_cond = 0;
#if defined(CONFIG_MTK_UFS_SUPPORT) && defined(CLKBUF_USE_BBLPM)
	int boot_type;
#endif

#ifdef CLKBUF_USE_BBLPM
	if (!is_clkbuf_initiated || !is_pmic_clkbuf || !bblpm_switch) {
		bblpm_cond |= BBLPM_COND_SKIP;
		return bblpm_cond;
	}

	/* if (pwr_sta & PWR_STATUS_MD) */
	if (!is_clk_buf_under_flightmode() &&
		(pmic_clk_buf_swctrl[XO_CEL] != CLK_BUF_SW_DISABLE))
		bblpm_cond |= BBLPM_COND_CEL;
	if (bblpm_switch != 2) {

		if ((pmic_clk_buf_swctrl[XO_WCN] == CLK_BUF_SW_ENABLE) ||
			(mtk_spm_read_register(SPM_PWRSTA) & PWR_STATUS_CONN))
			bblpm_cond |= BBLPM_COND_WCN;

		if (pmic_clk_buf_swctrl[XO_NFC] == CLK_BUF_SW_ENABLE)
			bblpm_cond |= BBLPM_COND_NFC;

#if defined(CONFIG_MTK_UFS_SUPPORT)
		boot_type = get_boot_type();
		if (boot_type == BOOTDEV_UFS) {
			if (ufs_mtk_deepidle_hibern8_check() < 0)
				bblpm_cond |= BBLPM_COND_EXT;
		}
#endif
	}
#else /* !CLKBUF_USE_BBLPM */
	bblpm_cond |= BBLPM_COND_SKIP;
#endif

	if (!bblpm_cond)
		bblpm_cnt++;

	return bblpm_cond;
}

static void clk_buf_ctrl_internal(enum clk_buf_id id, enum clk_buf_onff onoff)
{
	int pwrap_dcxo_en;

	if (!is_pmic_clkbuf)
		return;

	mutex_lock(&clk_buf_ctrl_lock);

	switch (id) {
	case CLK_BUF_CONN:
		if (onoff == CLK_BUF_FORCE_ON) {
			pwrap_dcxo_en =
				clkbuf_readl(DCXO_ENABLE) & ~DCXO_CONN_ENABLE;
			clkbuf_writel(DCXO_ENABLE, pwrap_dcxo_en);
			pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
				PMIC_XO_EXTBUF2_MODE_MASK,
				PMIC_XO_EXTBUF2_MODE_MASK,
				PMIC_XO_EXTBUF2_MODE_SHIFT);
			pmic_config_interface(PMIC_DCXO_CW00_SET_ADDR,
				PMIC_XO_EXTBUF2_EN_M_MASK,
				PMIC_XO_EXTBUF2_EN_M_MASK,
				PMIC_XO_EXTBUF2_EN_M_SHIFT);
			pmic_clk_buf_swctrl[XO_WCN] = 1;
		} else if (onoff == CLK_BUF_FORCE_OFF) {
			pwrap_dcxo_en =
				clkbuf_readl(DCXO_ENABLE) & ~DCXO_CONN_ENABLE;
			clkbuf_writel(DCXO_ENABLE, pwrap_dcxo_en);
			pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
				PMIC_XO_EXTBUF2_MODE_MASK,
				PMIC_XO_EXTBUF2_MODE_MASK,
				PMIC_XO_EXTBUF2_MODE_SHIFT);
			pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
				PMIC_XO_EXTBUF2_EN_M_MASK,
				PMIC_XO_EXTBUF2_EN_M_MASK,
				PMIC_XO_EXTBUF2_EN_M_SHIFT);
			pmic_clk_buf_swctrl[XO_WCN] = 0;
		} else if (onoff == CLK_BUF_INIT_SETTING) {
			pmic_config_interface(PMIC_XO_EXTBUF2_MODE_ADDR,
				xo_mode_init[XO_WCN],
				PMIC_XO_EXTBUF2_MODE_MASK,
				PMIC_XO_EXTBUF2_MODE_SHIFT);
			pwrap_dcxo_en =
				clkbuf_readl(DCXO_ENABLE) |
				(pwrap_dcxo_en_init & DCXO_CONN_ENABLE);
			clkbuf_writel(DCXO_ENABLE, pwrap_dcxo_en);
		}
		pr_info("%s: id=%d, onoff=%d, DCXO_ENABLE=0x%x\n",
			__func__, id, onoff, clkbuf_readl(DCXO_ENABLE));

		break;
	case CLK_BUF_NFC:
		if (onoff == CLK_BUF_FORCE_ON) {
			pwrap_dcxo_en =
				clkbuf_readl(DCXO_ENABLE) & ~DCXO_NFC_ENABLE;
			clkbuf_writel(DCXO_ENABLE, pwrap_dcxo_en);
			pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
				      PMIC_XO_EXTBUF3_MODE_MASK,
				      PMIC_XO_EXTBUF3_MODE_MASK,
				      PMIC_XO_EXTBUF3_MODE_SHIFT);
			pmic_config_interface(PMIC_DCXO_CW00_SET_ADDR,
				PMIC_XO_EXTBUF3_EN_M_MASK,
				PMIC_XO_EXTBUF3_EN_M_MASK,
				PMIC_XO_EXTBUF3_EN_M_SHIFT);
			pmic_clk_buf_swctrl[XO_NFC] = 1;
		} else if (onoff == CLK_BUF_FORCE_OFF) {
			pwrap_dcxo_en =
				clkbuf_readl(DCXO_ENABLE) & ~DCXO_NFC_ENABLE;
			clkbuf_writel(DCXO_ENABLE, pwrap_dcxo_en);
			pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
				PMIC_XO_EXTBUF3_MODE_MASK,
				PMIC_XO_EXTBUF3_MODE_MASK,
				PMIC_XO_EXTBUF3_MODE_SHIFT);
			pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
				PMIC_XO_EXTBUF3_EN_M_MASK,
				PMIC_XO_EXTBUF3_EN_M_MASK,
				PMIC_XO_EXTBUF3_EN_M_SHIFT);
			pmic_clk_buf_swctrl[XO_NFC] = 0;
		} else if (onoff == CLK_BUF_INIT_SETTING) {
			pmic_config_interface(PMIC_XO_EXTBUF3_MODE_ADDR,
				xo_mode_init[XO_NFC],
				PMIC_XO_EXTBUF3_MODE_MASK,
				PMIC_XO_EXTBUF3_MODE_SHIFT);
			pwrap_dcxo_en =
				clkbuf_readl(DCXO_ENABLE) |
				(pwrap_dcxo_en_init & DCXO_NFC_ENABLE);
			clkbuf_writel(DCXO_ENABLE, pwrap_dcxo_en);
		}
		pr_info("%s: id=%d, onoff=%d, DCXO_ENABLE=0x%x\n",
			__func__, id, onoff, clkbuf_readl(DCXO_ENABLE));
		break;
	case CLK_BUF_RF:
		if (onoff == CLK_BUF_FORCE_ON) {
			pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
				      PMIC_XO_EXTBUF4_MODE_MASK,
				      PMIC_XO_EXTBUF4_MODE_MASK,
				      PMIC_XO_EXTBUF4_MODE_SHIFT);
			pmic_config_interface(PMIC_DCXO_CW00_SET_ADDR,
				PMIC_XO_EXTBUF4_EN_M_MASK,
				PMIC_XO_EXTBUF4_EN_M_MASK,
				PMIC_XO_EXTBUF4_EN_M_SHIFT);
			pmic_clk_buf_swctrl[XO_CEL] = 1;
		} else if (onoff == CLK_BUF_FORCE_OFF) {
			pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
				PMIC_XO_EXTBUF4_MODE_MASK,
				PMIC_XO_EXTBUF4_MODE_MASK,
				PMIC_XO_EXTBUF4_MODE_SHIFT);
			pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
				PMIC_XO_EXTBUF4_EN_M_MASK,
				PMIC_XO_EXTBUF4_EN_M_MASK,
				PMIC_XO_EXTBUF4_EN_M_SHIFT);
			pmic_clk_buf_swctrl[XO_CEL] = 0;
		} else if (onoff == CLK_BUF_INIT_SETTING) {
			pmic_config_interface(PMIC_XO_EXTBUF4_MODE_ADDR,
				xo_mode_init[XO_CEL],
				PMIC_XO_EXTBUF4_MODE_MASK,
				PMIC_XO_EXTBUF4_MODE_SHIFT);
		}
		pr_info("%s: id=%d, onoff=%d\n", __func__, id, onoff);
		break;
	case CLK_BUF_UFS:
		if (onoff == CLK_BUF_FORCE_ON) {
			clk_buf7_ctrl = false;
			pmic_config_interface(PMIC_DCXO_CW11_CLR_ADDR,
				      PMIC_XO_EXTBUF7_MODE_MASK,
				      PMIC_XO_EXTBUF7_MODE_MASK,
				      PMIC_XO_EXTBUF7_MODE_SHIFT);
			pmic_config_interface(PMIC_DCXO_CW11_SET_ADDR,
				PMIC_XO_EXTBUF7_EN_M_MASK,
				PMIC_XO_EXTBUF7_EN_M_MASK,
				PMIC_XO_EXTBUF7_EN_M_SHIFT);
			pmic_clk_buf_swctrl[XO_EXT] = 1;
		} else if (onoff == CLK_BUF_FORCE_OFF) {
			clk_buf7_ctrl = false;
			pmic_config_interface(PMIC_DCXO_CW11_CLR_ADDR,
				PMIC_XO_EXTBUF7_MODE_MASK,
				PMIC_XO_EXTBUF7_MODE_MASK,
				PMIC_XO_EXTBUF7_MODE_SHIFT);
			pmic_config_interface(PMIC_DCXO_CW11_CLR_ADDR,
				PMIC_XO_EXTBUF7_EN_M_MASK,
				PMIC_XO_EXTBUF7_EN_M_MASK,
				PMIC_XO_EXTBUF7_EN_M_SHIFT);
			pmic_clk_buf_swctrl[XO_EXT] = 0;
		} else if (onoff == CLK_BUF_INIT_SETTING) {
			pmic_config_interface(PMIC_XO_EXTBUF7_MODE_ADDR,
				xo_mode_init[XO_EXT],
				PMIC_XO_EXTBUF7_MODE_MASK,
				PMIC_XO_EXTBUF7_MODE_SHIFT);
			clk_buf7_ctrl = true;
		}
		pr_info("%s: id=%d, onoff=%d\n", __func__, id, onoff);
		break;
	default:
		pr_info("%s: id=%d isn't supported\n", __func__, id);
		break;
	}

	mutex_unlock(&clk_buf_ctrl_lock);
}

static void pmic_clk_buf_ctrl(enum CLK_BUF_SWCTRL_STATUS_T *status)
{
	u32 pmic_cw00 = 0, pmic_cw11 = 0;

	if (!is_clkbuf_initiated)
		return;

	clk_buf_ctrl_internal(CLK_BUF_CONN, status[XO_WCN] % 3);
	clk_buf_ctrl_internal(CLK_BUF_NFC, status[XO_NFC] % 3);
	clk_buf_ctrl_internal(CLK_BUF_RF, status[XO_CEL] % 3);
	clk_buf_ctrl_internal(CLK_BUF_UFS, status[XO_EXT] % 3);

	pmic_read_interface(PMIC_DCXO_CW00, &pmic_cw00,
		PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface(PMIC_DCXO_CW11, &pmic_cw11,
		PMIC_REG_MASK, PMIC_REG_SHIFT);
	pr_info("%s DCXO_CW00=0x%x, CW11=0x%x, clk_buf_swctrl=[%u %u %u %u 0 0 %u]\n",
		__func__, pmic_cw00, pmic_cw11, status[XO_SOC], status[XO_WCN],
		status[XO_NFC], status[XO_CEL], status[XO_EXT]);
}

static int clk_buf_mode_set(enum clk_buf_id id)
{
	unsigned int val = 0;
	int ret = 0;

	switch (id) {
	case CLK_BUF_BB_MD:
		break;
	case CLK_BUF_CONN:
		pmic_read_interface(PMIC_XO_EXTBUF2_MODE_ADDR,
			&val,
			PMIC_XO_EXTBUF2_MODE_MASK,
			PMIC_XO_EXTBUF2_MODE_SHIFT);
#ifdef CLKBUF_CONN_SUPPORT_CTRL_FROM_I1
		ret = val - BUF_MAN_M;
#else
		ret = val - SIG_CTRL_M;
#endif
		break;
	case CLK_BUF_NFC:
		pmic_read_interface(PMIC_XO_EXTBUF3_MODE_ADDR,
				&val,
				PMIC_XO_EXTBUF3_MODE_MASK,
				PMIC_XO_EXTBUF3_MODE_SHIFT);
		ret = val - BUF_MAN_M;
		break;
	case CLK_BUF_RF:
		pmic_read_interface(PMIC_XO_EXTBUF4_MODE_ADDR,
				&val,
				PMIC_XO_EXTBUF4_MODE_MASK,
				PMIC_XO_EXTBUF4_MODE_SHIFT);
		ret = val - SIG_CTRL_M;
		break;
	case CLK_BUF_UFS:
		pmic_read_interface(PMIC_XO_EXTBUF7_MODE_ADDR,
				&val,
				PMIC_XO_EXTBUF7_MODE_MASK,
				PMIC_XO_EXTBUF7_MODE_SHIFT);
		ret = val - BUF_MAN_M;
		break;
	default:
		ret = -5;
		pr_info("%s: id=%d isn't supported\n", __func__, id);
		break;
	}

	return ret;
}

bool clk_buf_ctrl_combine(enum clk_buf_id id, bool onoff)
{
	short ret = 0, no_lock = 0;
	int val = 0;

	if (!is_clkbuf_initiated)
		return false;

	if (!is_pmic_clkbuf)
		return false;

	clk_buf_pr_dbg("%s: id=%d, onoff=%d, clkbuf_ctrl_stat=0x%x\n",
		__func__, id, onoff, clkbuf_ctrl_stat);

	if (preempt_count() > 0 || irqs_disabled()
			|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&clk_buf_ctrl_lock);

	switch (id) {
	case CLK_BUF_BB_MD:
		if (CLK_BUF1_STATUS != CLOCK_BUFFER_SW_CONTROL) {
			ret = -1;
			pr_info("%s: id=%d isn't controlled by SW\n",
				__func__, id);
			break;
		}
		break;
	case CLK_BUF_CONN:
		if (onoff)
			pmic_config_interface(PMIC_DCXO_CW00_SET_ADDR,
			xo2_mode_set[CO_BUF_M],
			PMIC_XO_EXTBUF2_MODE_MASK,
			PMIC_XO_EXTBUF2_MODE_SHIFT);
		else {
			val = clk_buf_mode_set(CLK_BUF_CONN);
			if (val > 0)
				pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
					val,
					PMIC_XO_EXTBUF2_MODE_MASK,
					PMIC_XO_EXTBUF2_MODE_SHIFT);
			else if (val < 0) {
				val = 0 - val;
				pmic_config_interface(PMIC_DCXO_CW00_SET_ADDR,
					val,
					PMIC_XO_EXTBUF2_MODE_MASK,
					PMIC_XO_EXTBUF2_MODE_SHIFT);
			}
		}
		break;
	case CLK_BUF_NFC:
		if (onoff)
			pmic_config_interface(PMIC_DCXO_CW00_SET_ADDR,
				xo3_mode_set[CO_BUF_M],
				PMIC_XO_EXTBUF3_MODE_MASK,
				PMIC_XO_EXTBUF3_MODE_SHIFT);
		else {
			val = clk_buf_mode_set(CLK_BUF_NFC);
			if (val > 0)
				pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
					val,
					PMIC_XO_EXTBUF3_MODE_MASK,
					PMIC_XO_EXTBUF3_MODE_SHIFT);
			else if (val < 0) {
				val = 0 - val;
				pmic_config_interface(PMIC_DCXO_CW00_SET_ADDR,
					val,
					PMIC_XO_EXTBUF3_MODE_MASK,
					PMIC_XO_EXTBUF3_MODE_SHIFT);
			}
		}
		break;
	case CLK_BUF_RF:
		if (onoff)
			pmic_config_interface(PMIC_DCXO_CW00_SET_ADDR,
				xo4_mode_set[CO_BUF_M],
				PMIC_XO_EXTBUF4_MODE_MASK,
				PMIC_XO_EXTBUF4_MODE_SHIFT);
		else {
			val = clk_buf_mode_set(CLK_BUF_RF);
			if (val > 0)
				pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
					val,
					PMIC_XO_EXTBUF4_MODE_MASK,
					PMIC_XO_EXTBUF4_MODE_SHIFT);
			else if (val < 0) {
				val = 0 - val;
				pr_info("%s val = %d\n", __func__, val);
				pmic_config_interface(PMIC_DCXO_CW00_SET_ADDR,
					val,
					PMIC_XO_EXTBUF4_MODE_MASK,
					PMIC_XO_EXTBUF4_MODE_SHIFT);
			}
		}
		break;
	case CLK_BUF_UFS:
		if (onoff)
			pmic_config_interface(PMIC_DCXO_CW11_SET_ADDR,
					xo7_mode_set[CO_BUF_M],
					PMIC_XO_EXTBUF7_MODE_MASK,
					PMIC_XO_EXTBUF7_MODE_SHIFT);
		else {
			val = clk_buf_mode_set(CLK_BUF_UFS);
			if (val > 0)
				pmic_config_interface(PMIC_DCXO_CW11_CLR_ADDR,
					val,
					PMIC_XO_EXTBUF7_MODE_MASK,
					PMIC_XO_EXTBUF7_MODE_SHIFT);
			else if (val < 0) {
				val = 0 - val;
				pmic_config_interface(PMIC_DCXO_CW11_SET_ADDR,
					val,
					PMIC_XO_EXTBUF7_MODE_MASK,
					PMIC_XO_EXTBUF7_MODE_SHIFT);
			}
		}
		break;
	default:
		ret = -1;
		pr_info("%s: id=%d isn't supported\n", __func__, id);
		break;
	}

	if (!no_lock)
		mutex_unlock(&clk_buf_ctrl_lock);

	if (ret)
		return false;
	else
		return true;
}
EXPORT_SYMBOL(clk_buf_ctrl_combine);

bool clk_buf_ctrl(enum clk_buf_id id, bool onoff)
{
	short ret = 0, no_lock = 0;

	if (!is_clkbuf_initiated)
		return false;

	if (!is_pmic_clkbuf)
		return false;

	clk_buf_pr_dbg("%s: id=%d, onoff=%d, clkbuf_ctrl_stat=0x%x\n",
		__func__, id, onoff, clkbuf_ctrl_stat);

	if (preempt_count() > 0 || irqs_disabled()
			|| system_state != SYSTEM_RUNNING || oops_in_progress)
		no_lock = 1;

	if (!no_lock)
		mutex_lock(&clk_buf_ctrl_lock);

	switch (id) {
	case CLK_BUF_BB_MD:
		if (CLK_BUF1_STATUS != CLOCK_BUFFER_SW_CONTROL) {
			ret = -1;
			pr_info("%s: id=%d isn't controlled by SW\n",
				__func__, id);
			break;
		}
		break;
	case CLK_BUF_CONN:
		if (CLK_BUF2_STATUS != CLOCK_BUFFER_SW_CONTROL) {
			ret = -1;
			pr_info("%s: id=%d isn't controlled by SW\n",
				__func__, id);
			break;
		}
		/* record the status of CONN from caller for checking BBLPM */
		pmic_clk_buf_swctrl[XO_WCN] = onoff;
		break;
	case CLK_BUF_NFC:
		if (CLK_BUF3_STATUS != CLOCK_BUFFER_SW_CONTROL) {
			ret = -1;
			pr_info("%s: id=%d isn't controlled by SW\n",
				__func__, id);
			break;
		}
		/* record the status of NFC from caller for checking BBLPM */
		pmic_clk_buf_swctrl[XO_NFC] = onoff;
		break;
	case CLK_BUF_RF:
		if (CLK_BUF4_STATUS != CLOCK_BUFFER_SW_CONTROL) {
			ret = -1;
			pr_info("%s: id=%d isn't controlled by SW\n",
				__func__, id);
			break;
		}
		break;
	case CLK_BUF_UFS:
		if ((CLK_BUF7_STATUS != CLOCK_BUFFER_SW_CONTROL) ||
			(clk_buf7_ctrl != true)) {
			ret = -1;
			pr_info("%s: id=%d isn't controlled by SW\n",
				__func__, id);
			break;
		}
		pmic_clk_buf_ctrl_ext(onoff);
		pmic_clk_buf_swctrl[XO_EXT] = onoff;
		break;
	default:
		ret = -1;
		pr_info("%s: id=%d isn't supported\n", __func__, id);
		break;
	}

	if (!no_lock)
		mutex_unlock(&clk_buf_ctrl_lock);

	if (ret)
		return false;
	else
		return true;
}
EXPORT_SYMBOL(clk_buf_ctrl);

void clk_buf_dump_dts_log(void)
{
	pr_info("%s: PMIC_CLK_BUF?_STATUS=%d %d %d %d %d %d %d\n", __func__,
		     CLK_BUF1_STATUS, CLK_BUF2_STATUS,
		     CLK_BUF3_STATUS, CLK_BUF4_STATUS,
		     CLK_BUF5_STATUS, CLK_BUF6_STATUS,
		     CLK_BUF7_STATUS);
	pr_info("%s: PMIC_CLK_BUF?_DRV_CURR=%d %d %d %d %d %d %d\n", __func__,
		     PMIC_CLK_BUF1_DRIVING_CURR,
		     PMIC_CLK_BUF2_DRIVING_CURR,
		     PMIC_CLK_BUF3_DRIVING_CURR,
		     PMIC_CLK_BUF4_DRIVING_CURR,
		     PMIC_CLK_BUF5_DRIVING_CURR,
		     PMIC_CLK_BUF6_DRIVING_CURR,
		     PMIC_CLK_BUF7_DRIVING_CURR);
}

void clk_buf_dump_clkbuf_log(void)
{
	u32 pmic_cw00 = 0, pmic_cw01 = 0, pmic_cw02 = 0, pmic_cw11 = 0,
		pmic_cw14 = 0, pmic_cw16 = 0, pmic_cw23 = 0, top_spi_con1 = 0;

	pmic_read_interface(PMIC_XO_EXTBUF1_MODE_ADDR, &pmic_cw00,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface(PMIC_XO_CLKSEL_EN_M_ADDR, &pmic_cw01,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface(PMIC_XO_BUFLDOK_EN_ADDR, &pmic_cw02,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface(PMIC_XO_EXTBUF6_MODE_ADDR, &pmic_cw11,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface(PMIC_XO_EXTBUF2_CLKSEL_MAN_ADDR, &pmic_cw14,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface(PMIC_XO_EXTBUF1_ISET_M_ADDR, &pmic_cw16,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface(PMIC_RG_SRCLKEN_IN3_EN_ADDR, &top_spi_con1,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface(PMIC_DCXO_CW23, &pmic_cw23,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pr_info("%s DCXO_CW00/01/02/11/14/16/23=0x%x %x %x %x %x %x %x\n",
		     __func__, pmic_cw00, pmic_cw01, pmic_cw02, pmic_cw11,
		     pmic_cw14, pmic_cw16, pmic_cw23);
	pr_info("%s top_spi_con1=0x%x\n", __func__, top_spi_con1);
}

static u32 dcxo_dbg_read_auxout(u16 sel)
{
	u32 rg_auxout = 0;

	pmic_config_interface(PMIC_XO_STATIC_AUXOUT_SEL_ADDR, sel,
			      PMIC_XO_STATIC_AUXOUT_SEL_MASK,
			      PMIC_XO_STATIC_AUXOUT_SEL_SHIFT);
	pmic_read_interface(PMIC_XO_STATIC_AUXOUT_ADDR, &rg_auxout,
			    PMIC_XO_STATIC_AUXOUT_MASK,
			    PMIC_XO_STATIC_AUXOUT_SHIFT);
	clk_buf_pr_dbg("%s: sel=%d, rg_auxout=0x%x\n",
		__func__, sel, rg_auxout);

	return rg_auxout;
}

static void clk_buf_get_xo_en(void)
{
	u32 rg_auxout = 0;

	rg_auxout = dcxo_dbg_read_auxout(5);
	pr_debug("%s: sel io_dbg4: rg_auxout=0x%x\n",
			__func__, rg_auxout);
	xo_en_stat[XO_SOC] = (rg_auxout & (0x1 << 0)) >> 0;
	xo_en_stat[XO_WCN] = (rg_auxout & (0x1 << 6)) >> 6;

	rg_auxout = dcxo_dbg_read_auxout(6);
	pr_debug("%s: sel io_dbg5: rg_auxout=0x%x\n", __func__, rg_auxout);
	xo_en_stat[XO_NFC] = (rg_auxout & (0x1 << 0)) >> 0;
	xo_en_stat[XO_CEL] = (rg_auxout & (0x1 << 6)) >> 6;
	xo_en_stat[XO_EXT] = (rg_auxout & (0x1 << 12)) >> 12;

	rg_auxout = dcxo_dbg_read_auxout(7);
	pr_debug("%s: sel io_dbg6: rg_auxout=0x%x\n", __func__, rg_auxout);
	/* xo_en_stat[XO_AUD] = (rg_auxout & (0x1 << 0)) >> 0; */
	xo_en_stat[XO_PD] = (rg_auxout & (0x1 << 6)) >> 6;

	pr_info("%s: PMIC_CLK_BUF?_EN_STAT=%d %d %d %d %d %d %d\n",
		__func__,
		xo_en_stat[XO_SOC],
		xo_en_stat[XO_WCN],
		xo_en_stat[XO_NFC],
		xo_en_stat[XO_CEL],
		xo_en_stat[XO_AUD],
		xo_en_stat[XO_PD],
		xo_en_stat[XO_EXT]);
}

static void clk_buf_get_drv_curr(void)
{
	u32 rg_auxout = 0;

	rg_auxout = dcxo_dbg_read_auxout(5);
	pr_debug("%s: sel io_dbg4: rg_auxout=0x%x\n", __func__, rg_auxout);
	clkbuf_drv_curr_auxout[XO_SOC] = (rg_auxout & (0x3 << 1)) >> 1;
	clkbuf_drv_curr_auxout[XO_WCN] = (rg_auxout & (0x3 << 7)) >> 7;

	rg_auxout = dcxo_dbg_read_auxout(6);
	pr_debug("%s: sel io_dbg5: rg_auxout=0x%x\n", __func__, rg_auxout);
	clkbuf_drv_curr_auxout[XO_NFC] = (rg_auxout & (0x3 << 1)) >> 1;
	clkbuf_drv_curr_auxout[XO_CEL] = (rg_auxout & (0x3 << 7)) >> 7;

	rg_auxout = dcxo_dbg_read_auxout(7);
	pr_debug("%s: sel io_dbg6: rg_auxout=0x%x\n", __func__, rg_auxout);
	clkbuf_drv_curr_auxout[XO_AUD] = (rg_auxout & (0x3 << 1)) >> 1;
	clkbuf_drv_curr_auxout[XO_PD] = (rg_auxout & (0x3 << 7)) >> 7;
	clkbuf_drv_curr_auxout[XO_EXT] = (rg_auxout & (0x3 << 12)) >> 12;

	pr_info("%s: PMIC_CLK_BUF?_DRV_CURR_AUXOUT=%d %d %d %d %d %d %d\n",
			__func__,
			clkbuf_drv_curr_auxout[XO_SOC],
			clkbuf_drv_curr_auxout[XO_WCN],
			clkbuf_drv_curr_auxout[XO_NFC],
			clkbuf_drv_curr_auxout[XO_CEL],
			clkbuf_drv_curr_auxout[XO_AUD],
			clkbuf_drv_curr_auxout[XO_PD],
			clkbuf_drv_curr_auxout[XO_EXT]);
}

static bool clk_buf_is_auto_calc_enabled(void)
{
	u32 autok = 0;

	pmic_read_interface(PMIC_XO_BUFLDOK_EN_ADDR, &autok,
			    PMIC_XO_BUFLDOK_EN_MASK, PMIC_XO_BUFLDOK_EN_SHIFT);
	if (autok)
		return true;
	else
		return false;
}

static void clk_buf_set_auto_calc(u8 onoff)
{
	if (onoff) {
		pmic_config_interface(PMIC_XO_BUFLDOK_EN_ADDR, 0,
				    PMIC_XO_BUFLDOK_EN_MASK,
				    PMIC_XO_BUFLDOK_EN_SHIFT);
		udelay(100);
		pmic_config_interface(PMIC_XO_BUFLDOK_EN_ADDR, 1,
				    PMIC_XO_BUFLDOK_EN_MASK,
				    PMIC_XO_BUFLDOK_EN_SHIFT);
		mdelay(1);
	} else
		pmic_config_interface(PMIC_XO_BUFLDOK_EN_ADDR, 0,
				    PMIC_XO_BUFLDOK_EN_MASK,
				    PMIC_XO_BUFLDOK_EN_SHIFT);
}

static void clk_buf_set_manual_drv_curr(u32 *drv_curr_vals)
{
	u32 drv_curr_val = 0, drv_curr_mask = 0, drv_curr_shift = 0;

	drv_curr_val =
		(drv_curr_vals[XO_SOC] << PMIC_XO_EXTBUF1_ISET_M_SHIFT) |
		(drv_curr_vals[XO_WCN] << PMIC_XO_EXTBUF2_ISET_M_SHIFT) |
		(drv_curr_vals[XO_NFC] << PMIC_XO_EXTBUF3_ISET_M_SHIFT) |
		(drv_curr_vals[XO_CEL] << PMIC_XO_EXTBUF4_ISET_M_SHIFT) |
		/* (drv_curr_vals[XO_AUD] << PMIC_XO_EXTBUF5_ISET_M_SHIFT) | */
		(drv_curr_vals[XO_PD] << PMIC_XO_EXTBUF6_ISET_M_SHIFT) |
		(drv_curr_vals[XO_EXT] << PMIC_XO_EXTBUF7_ISET_M_SHIFT);
	drv_curr_mask =
		(PMIC_XO_EXTBUF1_ISET_M_MASK << PMIC_XO_EXTBUF1_ISET_M_SHIFT) |
		(PMIC_XO_EXTBUF2_ISET_M_MASK << PMIC_XO_EXTBUF2_ISET_M_SHIFT) |
		(PMIC_XO_EXTBUF3_ISET_M_MASK << PMIC_XO_EXTBUF3_ISET_M_SHIFT) |
		(PMIC_XO_EXTBUF4_ISET_M_MASK << PMIC_XO_EXTBUF4_ISET_M_SHIFT) |
		(PMIC_XO_EXTBUF6_ISET_M_MASK << PMIC_XO_EXTBUF6_ISET_M_SHIFT) |
		(PMIC_XO_EXTBUF7_ISET_M_MASK << PMIC_XO_EXTBUF7_ISET_M_SHIFT);
	drv_curr_shift = PMIC_XO_EXTBUF1_ISET_M_SHIFT;

	pmic_config_interface(PMIC_XO_EXTBUF1_ISET_M_ADDR, drv_curr_val,
			      drv_curr_mask, drv_curr_shift);
	pr_info("%s: drv_curr_val/mask/shift=0x%x %x %x\n", __func__,
		     drv_curr_val, drv_curr_mask, drv_curr_shift);
}

static void clk_buf_get_bblpm_en(void)
{
	u32 rg_auxout = 0;

	rg_auxout = dcxo_dbg_read_auxout(24);
	clk_buf_pr_dbg("%s: sel ctrl_dbg7: rg_auxout=0x%x\n",
		__func__, rg_auxout);

	xo_bb_lpm_en_stat = (rg_auxout & (0x1 << 0)) >> 0;

	pr_info("%s: bblpm %d\n", __func__, xo_bb_lpm_en_stat);
}

void clk_buf_get_aux_out(void)
{
	clk_buf_get_xo_en();
	clk_buf_get_bblpm_en();
}

int clk_buf_ctrl_bblpm_sw(bool enable)
{
	clk_buf_ctrl_bblpm_hw(false);
	if (enable)
		pmic_config_interface(PMIC_DCXO_CW00_SET_ADDR,
				1,
				PMIC_XO_BB_LPM_EN_MASK,
				PMIC_XO_BB_LPM_EN_SHIFT);
	else
		pmic_config_interface(PMIC_DCXO_CW00_CLR_ADDR,
				1,
				PMIC_XO_BB_LPM_EN_MASK,
				PMIC_XO_BB_LPM_EN_SHIFT);

	clk_buf_get_bblpm_en();

	return 0;
}

static ssize_t clk_buf_show_status_info_internal(char *buf)
{
	int len = 0;
	u32 pmic_cw00 = 0, pmic_cw02 = 0, pmic_cw11 = 0, pmic_cw13 = 0,
	    pmic_cw14 = 0, pmic_cw15 = 0, pmic_cw16 = 0, pmic_cw23 = 0;
	u32 top_spi_con1 = 0;
	u32 buf2_mode, buf3_mode, buf4_mode, buf6_mode, buf7_mode;
	u32 buf2_en_m, buf3_en_m, buf4_en_m, buf6_en_m, buf7_en_m;

	clk_buf_get_drv_curr();
	clk_buf_get_xo_en();

	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
			"********** PMIC clock buffer state (%s) **********\n",
			(is_pmic_clkbuf ? "on" : "off"));
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"XO_SOC   SW(1)/HW(2) CTL: %d, Dis(0)/En(1): %d, RS: %u\n",
		CLK_BUF1_STATUS, pmic_clk_buf_swctrl[XO_SOC],
		xo_en_stat[XO_SOC]);
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"XO_WCN   SW(1)/HW(2) CTL: %d, Dis(0)/En(1): %d, RS: %u\n",
		CLK_BUF2_STATUS, pmic_clk_buf_swctrl[XO_WCN],
		xo_en_stat[XO_WCN]);
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"XO_NFC   SW(1)/HW(2) CTL: %d, Dis(0)/En(1): %d, RS: %u\n",
		CLK_BUF3_STATUS, pmic_clk_buf_swctrl[XO_NFC],
		xo_en_stat[XO_NFC]);
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"XO_CEL   SW(1)/HW(2) CTL: %d, Dis(0)/En(1): %d, RS: %u\n",
		CLK_BUF4_STATUS, pmic_clk_buf_swctrl[XO_CEL],
		xo_en_stat[XO_CEL]);
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"XO_AUD   SW(1)/HW(2) CTL: %d, Dis(0)/En(1): %d, RS: %u\n",
		CLK_BUF5_STATUS, pmic_clk_buf_swctrl[XO_AUD],
		xo_en_stat[XO_AUD]);
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"XO_PD    SW(1)/HW(2) CTL: %d, Dis(0)/En(1): %d, RS: %u\n",
		CLK_BUF6_STATUS, pmic_clk_buf_swctrl[XO_PD],
		xo_en_stat[XO_PD]);
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"XO_EXT   SW(1)/HW(2) CTL: %d, Dis(0)/En(1): %d, RS: %u\n",
		CLK_BUF7_STATUS, pmic_clk_buf_swctrl[XO_EXT],
		xo_en_stat[XO_EXT]);
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
			".********** clock buffer debug info **********\n");
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
			"pmic_drv_curr_vals=%d %d %d %d %d %d %d\n",
			PMIC_CLK_BUF1_DRIVING_CURR, PMIC_CLK_BUF2_DRIVING_CURR,
			PMIC_CLK_BUF3_DRIVING_CURR, PMIC_CLK_BUF4_DRIVING_CURR,
			PMIC_CLK_BUF5_DRIVING_CURR, PMIC_CLK_BUF6_DRIVING_CURR,
			PMIC_CLK_BUF7_DRIVING_CURR);
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
			"clkbuf_drv_curr_auxout=%d %d %d %d %d %d %d\n",
			clkbuf_drv_curr_auxout[XO_SOC],
			clkbuf_drv_curr_auxout[XO_WCN],
			clkbuf_drv_curr_auxout[XO_NFC],
			clkbuf_drv_curr_auxout[XO_CEL],
			clkbuf_drv_curr_auxout[XO_AUD],
			clkbuf_drv_curr_auxout[XO_PD],
			clkbuf_drv_curr_auxout[XO_EXT]);

	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
			"clkbuf_ctrl_stat=0x%x\n", clkbuf_ctrl_stat);

	pmic_read_interface_nolock(PMIC_DCXO_CW00, &pmic_cw00,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface_nolock(PMIC_DCXO_CW02, &pmic_cw02,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface_nolock(PMIC_DCXO_CW11, &pmic_cw11,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface(PMIC_RG_XO_RESERVED4_ADDR, &pmic_cw13,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface_nolock(PMIC_DCXO_CW14, &pmic_cw14,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface(PMIC_RG_XO_EXTBUF1_HD_ADDR, &pmic_cw15,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface_nolock(PMIC_DCXO_CW16, &pmic_cw16,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	pmic_read_interface_nolock(PMIC_DCXO_CW23, &pmic_cw23,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);

	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"DCXO_CW00/02/11/13/14/15/16/23=0x%x %x %x %x %x %x %x %x\n",
		pmic_cw00, pmic_cw02, pmic_cw11, pmic_cw13, pmic_cw14,
		pmic_cw15, pmic_cw16, pmic_cw23);

	buf2_mode = (pmic_cw00 >> PMIC_XO_EXTBUF2_MODE_SHIFT)
		& PMIC_XO_EXTBUF2_MODE_MASK;
	buf3_mode = (pmic_cw00 >> PMIC_XO_EXTBUF3_MODE_SHIFT)
		& PMIC_XO_EXTBUF3_MODE_MASK;
	buf4_mode = (pmic_cw00 >> PMIC_XO_EXTBUF4_MODE_SHIFT)
		& PMIC_XO_EXTBUF4_MODE_MASK;
	buf6_mode = (pmic_cw11 >> PMIC_XO_EXTBUF6_MODE_SHIFT)
		& PMIC_XO_EXTBUF6_MODE_MASK;
	buf7_mode = (pmic_cw11 >> PMIC_XO_EXTBUF7_MODE_SHIFT)
		& PMIC_XO_EXTBUF7_MODE_MASK;
	buf2_en_m = (pmic_cw00 >> PMIC_XO_EXTBUF2_EN_M_SHIFT)
		& PMIC_XO_EXTBUF2_EN_M_MASK;
	buf3_en_m = (pmic_cw00 >> PMIC_XO_EXTBUF3_EN_M_SHIFT)
		& PMIC_XO_EXTBUF3_EN_M_MASK;
	buf4_en_m = (pmic_cw00 >> PMIC_XO_EXTBUF4_EN_M_SHIFT)
		& PMIC_XO_EXTBUF4_EN_M_MASK;
	buf6_en_m = (pmic_cw11 >> PMIC_XO_EXTBUF6_EN_M_SHIFT)
		& PMIC_XO_EXTBUF6_EN_M_MASK;
	buf7_en_m = (pmic_cw11 >> PMIC_XO_EXTBUF7_EN_M_SHIFT)
		& PMIC_XO_EXTBUF7_EN_M_MASK;
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"buf2/3/4/6/7 mode=%d/%d/%d/%d/%d, buf2/3/4/6/7 en_m=%d/%d/%d/%d/%d\n",
		buf2_mode, buf3_mode, buf4_mode, buf6_mode, buf7_mode,
		buf2_en_m, buf3_en_m, buf4_en_m, buf6_en_m, buf7_en_m);
	pmic_read_interface_nolock(PMIC_RG_SRCLKEN_IN3_EN_ADDR, &top_spi_con1,
			    PMIC_REG_MASK, PMIC_REG_SHIFT);
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"SRCLKEN_IN3_EN(srclken_conn)=0x%x\n", top_spi_con1);

	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"DCXO_CONN_ADR0/WDATA0/ADR1/WDATA1=0x%x %x %x %x\n",
		clkbuf_readl(DCXO_CONN_ADR0),
		clkbuf_readl(DCXO_CONN_WDATA0),
		clkbuf_readl(DCXO_CONN_ADR1),
		clkbuf_readl(DCXO_CONN_WDATA1));
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"DCXO_NFC_ADR0/WDATA0/ADR1/WDATA1/EN=0x%x %x %x %x %x\n",
		clkbuf_readl(DCXO_NFC_ADR0),
		clkbuf_readl(DCXO_NFC_WDATA0),
		clkbuf_readl(DCXO_NFC_ADR1),
		clkbuf_readl(DCXO_NFC_WDATA1),
		clkbuf_readl(DCXO_ENABLE));

	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"bblpm_switch=%u, bblpm_cnt=%u, bblpm_cond=0x%x\n",
		bblpm_switch, bblpm_cnt, clk_buf_bblpm_enter_cond());

	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"MD1_PWR_CON=0x%x, PWR_STATUS=0x%x, PCM_REG13_DATA=0x%x,",
		mtk_spm_read_register(SPM_MD1_PWR_CON),
		mtk_spm_read_register(SPM_PWRSTA),
		mtk_spm_read_register(SPM_REG13));
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
		"SPARE_ACK_MASK=0x%x, flight mode = %d\n",
		mtk_spm_read_register(SPM_SPARE_ACK_MASK),
		is_clk_buf_under_flightmode());
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
			".********** clock buffer command help **********\n");
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
			"%s %s > %s\n",
			"PMIC switch on/off: echo pmic",
			"en1 en2 en3 en4 en5 en6 en7",
			"/sys/power/clk_buf/clk_buf_ctrl");
	len += snprintf(buf+len, CLKBUF_STATUS_INFO_SIZE-len,
			"%s > %s\n",
			"Set drv curr(0~3): echo drvcurr v1 v2 v3 v4 v5 v6 v7",
			"/sys/power/clk_buf/clk_buf_ctrl\n");

	return len;
}

u8 clk_buf_get_xo_en_sta(enum xo_id id)
{
	if (id < 0 || id >= XO_NUMBER)
		return 0;
	clk_buf_get_xo_en();

	return xo_en_stat[id];
}

void clk_buf_show_status_info(void)
{
	int len;
	char *buf, *str, *str_sep;

	buf = vmalloc(CLKBUF_STATUS_INFO_SIZE);
	if (buf) {
		len = clk_buf_show_status_info_internal(buf);
		str = buf;
		while ((str_sep = strsep(&str, ".")) != NULL)
			pr_info("%s\n", str_sep);

		vfree(buf);
	} else
		pr_info("%s: allocate memory fail\n", __func__);
}

#ifdef CONFIG_PM
static ssize_t clk_buf_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 clk_buf_en[CLKBUF_NUM], i, pwrap_dcxo_en = 0;
	char cmd[32];

	if (sscanf(buf, "%31s %x %x %x %x %x %x %x", cmd, &clk_buf_en[XO_SOC],
		&clk_buf_en[XO_WCN], &clk_buf_en[XO_NFC], &clk_buf_en[XO_CEL],
		&clk_buf_en[XO_AUD], &clk_buf_en[XO_PD], &clk_buf_en[XO_EXT])
		!= (CLKBUF_NUM + 1))
		return -EPERM;

	if (!strcmp(cmd, "pmic")) {
		if (!is_pmic_clkbuf)
			return -EINVAL;

		for (i = 0; i < CLKBUF_NUM; i++)
			pmic_clk_buf_swctrl[i] = clk_buf_en[i];

		pmic_clk_buf_ctrl(pmic_clk_buf_swctrl);

		return count;
	} else if (!strcmp(cmd, "pwrap")) {
		if (!is_pmic_clkbuf)
			return -EINVAL;

		mutex_lock(&clk_buf_ctrl_lock);

		for (i = 0; i < CLKBUF_NUM; i++) {
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

		clkbuf_writel(DCXO_ENABLE, pwrap_dcxo_en);
		pr_info("%s: DCXO_ENABLE=0x%x, pwrap_dcxo_en=0x%x\n",
			__func__, clkbuf_readl(DCXO_ENABLE),
			pwrap_dcxo_en);

		mutex_unlock(&clk_buf_ctrl_lock);

		return count;
	} else if (!strcmp(cmd, "drvcurr")) {
		if (!is_pmic_clkbuf)
			return -EINVAL;

		mutex_lock(&clk_buf_ctrl_lock);

		if (clk_buf_is_auto_calc_enabled())
			clk_buf_set_auto_calc(0);
		clk_buf_set_manual_drv_curr(clk_buf_en);

		mutex_unlock(&clk_buf_ctrl_lock);

		return count;
	} else {
		return -EINVAL;
	}
}

static ssize_t clk_buf_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = clk_buf_show_status_info_internal(buf);

	return len;
}

static ssize_t clk_buf_debug_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 onoff;
	char cmd[32] =  {'\0'}, xo_user[11] = {'\0'};

	if ((sscanf(buf, "%31s %10s %x", cmd, xo_user, &onoff) != 3))
		return -EPERM;
	if (!strcmp(cmd, "debug")) {
		if (onoff == 0)
			clkbuf_debug = false;
		else if (onoff == 1)
			clkbuf_debug = true;
		else
			goto ERROR_CMD;
	}  else if (!strcmp(cmd, "bblpm")) {
		if (onoff == 0)
			bblpm_switch = 0;
		else if (onoff == 1)
			bblpm_switch = 1;
		else if (onoff == 2)
			bblpm_switch = 2;
		else
			goto ERROR_CMD;
	} else {
		if (!strcmp(xo_user, "XO_WCN")) {
			if (!strcmp(cmd, "CO_BUFFER"))
				clk_buf_ctrl_combine(CLK_BUF_CONN, onoff);
			else if (!strcmp(cmd, "FORCE_ON"))
				clk_buf_ctrl_internal(CLK_BUF_CONN, onoff);
			else if (!strcmp(cmd, "TEST"))
				clk_buf_ctrl(CLK_BUF_CONN, onoff);
			else
				goto ERROR_CMD;
		} else if (!strcmp(xo_user, "XO_NFC")) {
			if (!strcmp(cmd, "CO_BUFFER"))
				clk_buf_ctrl_combine(CLK_BUF_NFC, onoff);
			else if (!strcmp(cmd, "FORCE_ON"))
				clk_buf_ctrl_internal(CLK_BUF_NFC, onoff);
			else if (!strcmp(cmd, "TEST"))
				clk_buf_ctrl(CLK_BUF_NFC, onoff);
			else
				goto ERROR_CMD;
		} else if (!strcmp(xo_user, "XO_CEL")) {
			if (!strcmp(cmd, "CO_BUFFER"))
				clk_buf_ctrl_combine(CLK_BUF_RF, onoff);
			else if (!strcmp(cmd, "FORCE_ON"))
				clk_buf_ctrl_internal(CLK_BUF_RF, onoff);
			else if (!strcmp(cmd, "TEST"))
				clk_buf_ctrl(CLK_BUF_RF, onoff);
			else
				goto ERROR_CMD;
		} else if (!strcmp(xo_user, "XO_EXT")) {
			if (!strcmp(cmd, "CO_BUFFER"))
				clk_buf_ctrl_combine(CLK_BUF_UFS, onoff);
			else if (!strcmp(cmd, "FORCE_ON"))
				clk_buf_ctrl_internal(CLK_BUF_UFS, onoff);
			else if (!strcmp(cmd, "TEST"))
				clk_buf_ctrl(CLK_BUF_UFS, onoff);
			else
				goto ERROR_CMD;
		} else
			if (strcmp(xo_user, "0"))
				goto ERROR_CMD;
	}

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
	u32 onoff = 0;
	int ret = 0;

	if ((kstrtouint(buf, 10, &onoff))) {
		pr_info("bblpm input error\n");
		return -EPERM;
	}
	pr_info("bblpm input = %d\n", onoff);
	if (onoff == 1)
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
	int len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
		"EN_STAT=%d %d %d %d %d %d\n",
		xo_en_stat[XO_SOC],
		xo_en_stat[XO_WCN],
		xo_en_stat[XO_NFC],
		xo_en_stat[XO_CEL],
		xo_en_stat[XO_PD],
		xo_en_stat[XO_EXT]);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"bblpm %d\n", xo_bb_lpm_en_stat);

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

	/* create /sys/power/clk_buf/xxx */
	r = sysfs_create_group(power_kobj, &clk_buf_attr_group);
	if (r)
		pr_notice("FAILED TO CREATE /sys/power/clk_buf (%d)\n", r);

	return r;
}
#else /* !CONFIG_PM */
int clk_buf_fs_init(void)
{
	return 0;
}
#endif /* CONFIG_PM */

#if defined(CONFIG_OF)
int clk_buf_dts_map(void)
{
	struct device_node *node;
	u32 vals[CLKBUF_NUM] = {0, 0, 0, 0, 0, 0, 0};
	int ret = -1;

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,pmic_clock_buffer");
	if (node) {
		ret = of_property_read_u32_array(node,
			"mediatek,clkbuf-config", vals, CLKBUF_NUM);
		if (!ret) {
			CLK_BUF1_STATUS = vals[0];
			CLK_BUF2_STATUS = vals[1];
			CLK_BUF3_STATUS = vals[2];
			CLK_BUF4_STATUS = vals[3];
			CLK_BUF5_STATUS = vals[4];
			CLK_BUF6_STATUS = vals[5];
			CLK_BUF7_STATUS = vals[6];
		}
		ret = of_property_read_u32_array(node,
				"mediatek,clkbuf-driving-current",
				vals, CLKBUF_NUM);
		if (!ret) {
			PMIC_CLK_BUF1_DRIVING_CURR = vals[0];
			PMIC_CLK_BUF2_DRIVING_CURR = vals[1];
			PMIC_CLK_BUF3_DRIVING_CURR = vals[2];
			PMIC_CLK_BUF4_DRIVING_CURR = vals[3];
			PMIC_CLK_BUF5_DRIVING_CURR = vals[4];
			PMIC_CLK_BUF6_DRIVING_CURR = vals[5];
			PMIC_CLK_BUF7_DRIVING_CURR = vals[6];
		}
	} else {
		pr_err("%s can't find compatible node for pmic_clock_buffer\n",
				__func__);
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6768-pwrap");
	if (node)
		pwrap_base = of_iomap(node, 0);
	else {
		pr_notice("%s can't find compatible node for pwrap\n",
			__func__);
		return -1;
	}

	return 0;
}
#else /* !CONFIG_OF */
int clk_buf_dts_map(void)
{
	return 0;
}
#endif

void clk_buf_init_pmic_clkbuf(void)
{
	clk_buf_dump_clkbuf_log();
}

void clk_buf_init_pmic_wrap(void)
{
}

void clk_buf_init_pmic_swctrl(void)
{
}

short is_clkbuf_bringup(void)
{
#ifdef CLKBUF_BRINGUP
	pr_info("%s: skipped for bring up\n", __func__);
	return 1;
#else
	return 0;
#endif
}

void clk_buf_post_init(void)
{
#if defined(CONFIG_MTK_UFS_SUPPORT)
	int boot_type;

	boot_type = get_boot_type();
	/* no need to use XO_EXT if storage is emmc */
	if (boot_type != BOOTDEV_UFS) {
		clk_buf_ctrl_internal(CLK_BUF_UFS, CLK_BUF_FORCE_OFF);
		CLK_BUF7_STATUS = CLOCK_BUFFER_DISABLE;
	}
#else
	clk_buf_ctrl_internal(CLK_BUF_UFS, CLK_BUF_FORCE_OFF);
	CLK_BUF7_STATUS = CLOCK_BUFFER_DISABLE;
#endif

#ifndef CONFIG_NFC_CHIP_SUPPORT
	/* no need to use XO_NFC if no NFC */
	clk_buf_ctrl_internal(CLK_BUF_NFC, CLK_BUF_FORCE_OFF);
	CLK_BUF3_STATUS = CLOCK_BUFFER_DISABLE;
#endif
#ifdef CLKBUF_USE_BBLPM
	if (bblpm_switch == 2) {
		clk_buf_ctrl_bblpm_mask(CLK_BUF_BB_MD, true);
		clk_buf_ctrl_bblpm_mask(CLK_BUF_UFS, false);
		if (CLK_BUF4_STATUS == CLOCK_BUFFER_DISABLE) {
			clk_buf_ctrl_bblpm_mask(CLK_BUF_RF, true);
			clk_buf_ctrl_bblpm_hw(true);
		} else
			clk_buf_ctrl_bblpm_hw(false);
	}
#endif

	/* save setting after init done */
	pmic_read_interface(PMIC_XO_EXTBUF2_MODE_ADDR,
		&xo_mode_init[XO_WCN],
		PMIC_XO_EXTBUF2_MODE_MASK,
		PMIC_XO_EXTBUF2_MODE_SHIFT);
	pmic_read_interface(PMIC_XO_EXTBUF3_MODE_ADDR,
		&xo_mode_init[XO_NFC],
		PMIC_XO_EXTBUF3_MODE_MASK,
		PMIC_XO_EXTBUF3_MODE_SHIFT);
	pmic_read_interface(PMIC_XO_EXTBUF4_MODE_ADDR,
		&xo_mode_init[XO_CEL],
		PMIC_XO_EXTBUF4_MODE_MASK,
		PMIC_XO_EXTBUF4_MODE_SHIFT);
	pmic_read_interface(PMIC_XO_EXTBUF7_MODE_ADDR,
		&xo_mode_init[XO_EXT],
		PMIC_XO_EXTBUF7_MODE_MASK,
		PMIC_XO_EXTBUF7_MODE_SHIFT);
	pwrap_dcxo_en_init = clkbuf_readl(DCXO_ENABLE);
}

