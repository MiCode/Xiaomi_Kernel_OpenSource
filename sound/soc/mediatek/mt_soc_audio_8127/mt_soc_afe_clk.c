/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*#include "mt_soc_afe_def.h"*/
#include "mt_soc_afe_clk.h"
#include "mt_soc_afe_reg.h"
#include "mt_soc_afe_def.h"
#include <linux/spinlock.h>
#include <linux/delay.h>

#include <mt-plat/upmu_common.h>

#include <sync_write.h>
#ifdef _MT_IDLE_HEADER
#include <mt_idle.h>
#endif
#ifdef COMMON_CLOCK_FRAMEWORK_API
#include <linux/clk.h>
#endif

enum audio_system_clock_type {
	CLOCK_INFRA_SYS_AUDIO = 0,
	CLOCK_TOP_AUDIO_SEL,
	CLOCK_TOP_AUDINTBUS_SEL,
	CLOCK_TOP_APLL_SEL,
	CLOCK_TOP_AUDPLL,
	CLOCK_TOP_AUDPLL_D4,
	CLOCK_TOP_AUDPLL_D8,
	CLOCK_TOP_AUDPLL_D16,
	CLOCK_TOP_AUDPLL_D24,
/*	CLOCK_APMIXED_AUDPLL,*/
	CLOCK_CLK26M,
	CLOCK_NUM
};

#ifdef COMMON_CLOCK_FRAMEWORK_API

struct audio_clock_attr {
	const char *name;
	const bool prepare_once;
	bool is_prepared;
	struct clk *clock;
};

static struct audio_clock_attr aud_clks[CLOCK_NUM] = {
	[CLOCK_INFRA_SYS_AUDIO] = {"infra_sys_audio_clk", true, false, NULL},
	[CLOCK_TOP_AUDIO_SEL] = {"top_audio_sel", true, false, NULL},
	[CLOCK_TOP_AUDINTBUS_SEL] = {"top_audintbus_sel", true, false, NULL},/*top_pdn_audintbus*/
	[CLOCK_TOP_APLL_SEL] = {"top_apll_sel", false, false, NULL},/*top_pdn_audio*/
	[CLOCK_TOP_AUDPLL] = {"top_audpll", false, false, NULL},
	[CLOCK_TOP_AUDPLL_D4] = {"top_audpll_d4", false, false, NULL},
	[CLOCK_TOP_AUDPLL_D8] = {"top_audpll_d8", false, false, NULL},
	[CLOCK_TOP_AUDPLL_D16] = {"top_audpll_d16", false, false, NULL},
	[CLOCK_TOP_AUDPLL_D24] = {"top_audpll_d24", false, false, NULL},
/*	[CLOCK_APMIXED_AUDPLL] = {"apmixed_audpll", true, false, NULL},*/
	[CLOCK_CLK26M] = {"clk26m", false, false, NULL}
};

#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/
int aud_afe_clk_cntr;
int aud_ana_clk_cntr;
int aud_dac_clk_cntr;
int aud_adc_clk_cntr;
int aud_i2s_clk_cntr;
int aud_hdmi_clk_cntr;
int aud_spdif_clk_cntr;
int aud_apll_tuner_clk_cntr;
int aud_top_apll_clk_cntr;
int aud_apll1_tuner_cntr;
int aud_apll2_tuner_cntr;
int aud_emi_clk_cntr;

static DEFINE_SPINLOCK(afe_clk_lock);
static DEFINE_MUTEX(afe_clk_mutex);
static DEFINE_MUTEX(emi_clk_mutex);
static DEFINE_MUTEX(pmic_clk_mutex); /*auddrv_pmic_mutex*/


/*static function*/
#ifdef CONFIG_MTK_PMIC_MT6397
static void mt_afe_ana_top_on(void);
static void mt_afe_ana_top_off(void);
#endif

/*****************************************************************************
 *                         INTERNAL FUNCTION
 *****************************************************************************/
void turn_on_afe_clock(void)
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	if (enable_clock(MT_CG_INFRA_AUDIO, "AUDIO"))
		pr_err("%s Aud enable_clock MT_CG_INFRA_AUDIO fail", __func__);

	if (enable_clock(MT_CG_AUDIO_AFE, "AUDIO"))
		pr_err("%s Aud enable_clock MT_CG_AUDIO_AFE fail", __func__);

#else /*COMMON_CLOCK_FRAMEWORK_API*/
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	ret = clk_enable(aud_clks[CLOCK_TOP_AUDINTBUS_SEL].clock);
	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_TOP_AUDINTBUS_SEL].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 2 , 1 << 2);
	mt_afe_set_reg(AUDIO_TOP_CON0, 0x60004000, 0xffffffff);

	/*mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 2 | 1 <<14 | 1 <<29 | 1 <<30, 1 << 2 | 1 <<14 | 1 <<29 | 1 << 30);*/
#endif
#else
	/* mt_afe_topck_set_reg(AUDIO_CLK_CFG_4, 0 << 31, 1 << 31); */
	/* mt_afe_topck_set_reg(AUDIO_CLK_CFG_4, 0 << 23, 1 << 23); */
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 2 | 1 << 14 | 1 << 29 | 1 << 30, 1 << 2 | 1 << 14 | 1 << 29 | 1 << 30);
#endif
}

void turn_off_afe_clock(void)
{

#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	if (disable_clock(MT_CG_AUDIO_AFE, "AUDIO"))
		pr_err("%s disable_clock MT_CG_AUDIO_AFE fail", __func__);

	if (disable_clock(MT_CG_INFRA_AUDIO, "AUDIO"))
		pr_err("%s disable_clock MT_CG_INFRA_AUDIO fail", __func__);
#else /*COMMON_CLOCK_FRAMEWORK_API*/
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 2, 1 << 2);
	clk_disable(aud_clks[CLOCK_TOP_AUDINTBUS_SEL].clock);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 2, 1 << 2);
	/* mt_afe_topck_set_reg(AUDIO_CLK_CFG_4, 1 << 23, 1 << 23); */
	/* mt_afe_topck_set_reg(AUDIO_CLK_CFG_4, 1 << 31, 1 << 31); */
#endif
}

void turn_on_dac_clock(void)
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 25, 1 << 25);
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 26, 1 << 26);

#else /*COMMON_CLOCK_FRAMEWORK_API*/
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 25, 1 << 25);
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 26, 1 << 26);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 25, 1 << 25);
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 26, 1 << 26);
#endif
}

void turn_off_dac_clock(void)
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 26, 1 << 26);
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 25, 1 << 25);
#else /*COMMON_CLOCK_FRAMEWORK_API*/
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 26, 1 << 26);
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 25, 1 << 25);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 26, 1 << 26);
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 25, 1 << 25);
#endif
}

void turn_on_adc_clock(void) /*AudDrv_ADC_Clk_On*/
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 24, 1 << 24);
#else /*COMMON_CLOCK_FRAMEWORK_API*/
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 24, 1 << 24);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 24, 1 << 24);
#endif
}

void turn_off_adc_clock(void) /*AudDrv_ADC_Clk_Off*/
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 24, 1 << 24);
#else
     /*COMMON_CLOCK_FRAMEWORK_API*/
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 24, 1 << 24);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 24, 1 << 24);
#endif
}

void turn_on_i2s_clock(void)
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	if (enable_clock(MT_CG_AUDIO_I2S, "AUDIO"))
		pr_err("%s enable_clock MT65XX_PDN_AUDIO_I2S fail !!!\n", __func__);

#else /*COMMON_CLOCK_FRAMEWORK_API*/
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
	   __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 6, 1 << 6);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 6, 1 << 6);
#endif
}

void turn_off_i2s_clock(void)
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	if (disable_clock(MT_CG_AUDIO_I2S, "AUDIO"))
		pr_err("%s disable_clock MT65XX_PDN_AUDIO_I2S fail !!!\n", __func__);

#else /*COMMON_CLOCK_FRAMEWORK_API*/
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 6, 1 << 6);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 6, 1 << 6);
#endif
}

void turn_on_hdmi_clock(void)
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	if (enable_clock(MT_CG_AUDIO_HDMI_CK, "AUDIO"))
		pr_err("%s enable_clock MT_CG_AUDIO_HDMI_CK fail\n", __func__);
#else /*COMMON_CLOCK_FRAMEWORK_API*/
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 20, 1 << 20);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 20, 1 << 20);
#endif
}

void turn_off_hdmi_clock(void)
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	if (disable_clock(MT_CG_AUDIO_HDMI_CK, "AUDIO"))
		pr_err("%s disable_clock MT_CG_AUDIO_HDMI_CK fail\n", __func__);
#else /*COMMON_CLOCK_FRAMEWORK_API*/
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 20, 1 << 20);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 20, 1 << 20);
#endif
}

void turn_on_apll_tuner_clock(void)/*TURN_ON_APLL_TUNER_CLOCK*/
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	if (enable_clock(MT_CG_AUDIO_APLL_TUNER_CK, "AUDIO"))
		pr_err("%s enable_clock MT_CG_AUDIO_APLL_TUNER_CK fail\n", __func__);
#else /*COMMON_CLOCK_FRAMEWORK_API*/
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 19, 1 << 19);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 19, 1 << 19);
#endif
}

void turn_off_apll_tuner_clock(void) /*TURN_OFF_APLL_TUNER_CLOCK*/
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	if (disable_clock(MT_CG_AUDIO_APLL_TUNER_CK, "AUDIO"))
		pr_err("%s disable_clock MT_CG_AUDIO_APLL_TUNER_CK fail\n", __func__);
#else /*COMMON_CLOCK_FRAMEWORK_API*/
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 19, 1 << 19);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 19, 1 << 19);
#endif
}


void turn_on_top_apll_clock(void)
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
#else /*COMMON_CLOCK_FRAMEWORK_API*/
	int ret = clk_prepare_enable(aud_clks[CLOCK_TOP_APLL_SEL].clock);

	if (ret)
		pr_err("%s clk_prepare_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_TOP_APLL_SEL].name, ret);
#endif
#else
#endif
}

void turn_off_top_apll_clock(void)
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
    /*   Should be removed, MT8127 clk mngr will set MT_CG_TOP_PDN_APLL
      when MT_CG_AUDIO_APLL_TUNER_CK, MT_CG_AUDIO_HDMI_CK, MT_CG_AUDIO_SPDF_CK is set*/
#else /*COMMON_CLOCK_FRAMEWORK_API*/
	PRINTK_AUD_CLK("turn_off_top_apll_clock");
	clk_disable_unprepare(aud_clks[CLOCK_TOP_APLL_SEL].clock);
#endif
#else
#endif
}
void turn_on_spdif_clock(void)
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	if (enable_clock(MT_CG_AUDIO_SPDF_CK, "AUDIO"))
		pr_err("%s enable_clock MT_CG_AUDIO_SPDF_CK fail\n", __func__);
#else /*COMMON_CLOCK_FRAMEWORK_API*/
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 21, 1 << 21);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 21, 1 << 21);
#endif
}

void turn_off_spdif_clock(void)
{
#ifdef PM_MANAGER_API
#ifdef CONFIG_MTK_CLKMGR
	if (disable_clock(MT_CG_AUDIO_SPDF_CK, "AUDIO"))
		pr_err("%s disable_clock MT_CG_AUDIO_SPDF_CK fail\n", __func__);
#else /*COMMON_CLOCK_FRAMEWORK_API*/
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 21, 1 << 21);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#endif
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 21, 1 << 21);
#endif
}


void turn_on_apll1_tuner_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 19, 1 << 19);
	mt_afe_set_reg(AFE_APLL1_TUNER_CFG, 0x00008033, 0x0000FFF7);
	mt_afe_pll_set_reg(AP_PLL_CON5, 1 << 0, 1 << 0);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 19, 1 << 19);
	mt_afe_set_reg(AFE_APLL1_TUNER_CFG, 0x00008033, 0x0000FFF7);
	mt_afe_pll_set_reg(AP_PLL_CON5, 1 << 0, 1 << 0);
#endif
}

void mt_afe_apb_bus_init(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	mt_afe_set_reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);
	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

int mt_afe_init_clock(void *dev)
{
	int ret = 0;
#ifdef COMMON_CLOCK_FRAMEWORK_API
	size_t i;

	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		aud_clks[i].clock = devm_clk_get(dev, aud_clks[i].name);
		if (IS_ERR(aud_clks[i].clock)) {
			ret = PTR_ERR(aud_clks[i].clock);
			pr_err("%s devm_clk_get %s fail %d\n", __func__, aud_clks[i].name, ret);
			break;
		}
	}

	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		if (aud_clks[i].prepare_once) {
			ret = clk_prepare(aud_clks[i].clock);
			if (ret) {
				pr_err("%s clk_prepare %s fail %d\n",
				       __func__, aud_clks[i].name, ret);
				break;
			}
			aud_clks[i].is_prepared = true;
		}
	}
#endif
	return ret;
}

void mt_afe_deinit_clock(void *dev)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	size_t i;

	pr_debug("%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		if (aud_clks[i].clock && !IS_ERR(aud_clks[i].clock) && aud_clks[i].is_prepared) {
			clk_unprepare(aud_clks[i].clock);
			aud_clks[i].is_prepared = false;
		}
	}
#endif
}

void mt_afe_power_off_default_clock(void)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 19, 0xf000b42);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#endif
}

void mt_afe_main_clk_on(void) /*AudDrv_Clk_On*/
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_afe_clk_cntr:%d\n", __func__, aud_afe_clk_cntr);

	if (aud_afe_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_afe_clock();
#endif
	}
	aud_afe_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_main_clk_off(void)/*AudDrv_Clk_Off*/
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_afe_clk_cntr:%d\n", __func__, aud_afe_clk_cntr);

	aud_afe_clk_cntr--;
	if (aud_afe_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_afe_clock();
#endif
	} else if (aud_afe_clk_cntr < 0) {
		pr_err("%s aud_afe_clk_cntr:%d<0\n", __func__, aud_afe_clk_cntr);
		AUDIO_ASSERT(true);
		aud_afe_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}


/*****************************************************************************
 * FUNCTION
 *  mt_afe_ana_clk_on / mt_afe_ana_clk_off
 *
 * DESCRIPTION
 *  Enable/Disable analog part clock
 *
 *****************************************************************************/
void mt_afe_ana_clk_on(void) /*AudDrv_ANA_Clk_On*/
{
	mutex_lock(&pmic_clk_mutex);
	PRINTK_AUD_CLK("+%s, aud_ana_clk_cntr:%d\n", __func__, aud_ana_clk_cntr);
	if (aud_ana_clk_cntr == 0) {
#ifdef CONFIG_MTK_PMIC_MT6397
		upmu_set_rg_clksq_en(1);
		mt_afe_ana_top_on();
#else
		upmu_set_rg_clksq_en_aud(1);
#endif
	}
	aud_ana_clk_cntr++;
	mutex_unlock(&pmic_clk_mutex);

}

void mt_afe_ana_clk_off(void)/*AudDrv_ANA_Clk_Off*/
{
	mutex_lock(&pmic_clk_mutex);
	PRINTK_AUD_CLK("+%s, aud_ana_clk_cntr:%d\n", __func__, aud_ana_clk_cntr);
	aud_ana_clk_cntr--;
	if (aud_ana_clk_cntr == 0) {
		/* Disable ADC clock*/
#ifdef PM_MANAGER_API
	#ifdef CONFIG_MTK_PMIC_MT6397
		upmu_set_rg_clksq_en(0);
		mt_afe_ana_top_off();
	#else
		upmu_set_rg_clksq_en_aud(0);
	#endif
#endif
	} else if (aud_ana_clk_cntr < 0) {
		pr_debug("+%s,aud_afe_clk_cntr:%d<0)\n", __func__, aud_ana_clk_cntr);
		AUDIO_ASSERT(true);
		aud_ana_clk_cntr = 0;
	}
	mutex_unlock(&pmic_clk_mutex);

}


#ifdef CONFIG_MTK_PMIC_MT6397
/*****************************************************************************
 * FUNCTION
 *  mt_afe_ana_top_on / mt_afe_ana_top_off
 *
 * DESCRIPTION
 *  Enable/Disable analog part clock
 *
 *****************************************************************************/
static void mt_afe_ana_top_on(void) /*AudDrv_ANA_Top_On*/
{
	pr_debug("%s\n", __func__);
	pmic_set_ana_reg(TOP_CKPDN_CLR, 0x0003 , 0x00000003);
}

static void mt_afe_ana_top_off(void)/*AudDrv_ANA_Top_Off*/
{
	pr_debug("%s\n", __func__);
	pmic_set_ana_reg(TOP_CKPDN_SET, 0x0003 , 0x00000003);
}
#endif

void mt_afe_dac_clk_on(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_dac_clk_cntr:%d\n", __func__, aud_dac_clk_cntr);

	if (aud_dac_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_dac_clock();
#endif
	}
	aud_dac_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_dac_clk_off(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_dac_clk_cntr:%d\n", __func__, aud_dac_clk_cntr);

	aud_dac_clk_cntr--;
	if (aud_dac_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_dac_clock();
#endif
	} else if (aud_dac_clk_cntr < 0) {
		pr_err("%s aud_dac_clk_cntr(%d)<0\n", __func__, aud_dac_clk_cntr);
		aud_dac_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_adc_clk_on(void) /*AudDrv_ADC_Clk_On*/
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_adc_clk_cntr:%d\n", __func__, aud_adc_clk_cntr);

	if (aud_adc_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_adc_clock();
#endif
	}
	aud_adc_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_adc_clk_off(void) /*AudDrv_ADC_Clk_Off*/
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_adc_clk_cntr:%d\n", __func__, aud_adc_clk_cntr);

	aud_adc_clk_cntr--;
	if (aud_adc_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_adc_clock();
#endif
	} else if (aud_adc_clk_cntr < 0) {
		pr_err("%s aud_adc_clk_cntr(%d)<0\n", __func__, aud_adc_clk_cntr);
		aud_adc_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_i2s_clk_on(void) /*AudDrv_I2S_Clk_On*/
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_i2s_clk_cntr:%d\n", __func__, aud_i2s_clk_cntr);

	if (aud_i2s_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_i2s_clock();
#endif
	}
	aud_i2s_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_i2s_clk_off(void) /*AudDrv_I2S_Clk_Off*/
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_i2s_clk_cntr:%d\n", __func__, aud_i2s_clk_cntr);

	aud_i2s_clk_cntr--;
	if (aud_i2s_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_i2s_clock();
#endif
	} else if (aud_i2s_clk_cntr < 0) {
		pr_err("%s aud_i2s_clk_cntr:%d<0\n", __func__, aud_i2s_clk_cntr);
		AUDIO_ASSERT(true);
		aud_i2s_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_hdmi_clk_on(void)/*AudDrv_HDMI_Clk_On*/
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_hdmi_clk_cntr:%d\n", __func__, aud_hdmi_clk_cntr);

	if (aud_hdmi_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_hdmi_clock();
#endif
	}
	aud_hdmi_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_hdmi_clk_off(void)/*AudDrv_HDMI_Clk_Off*/
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_hdmi_clk_cntr:%d\n", __func__, aud_hdmi_clk_cntr);

	aud_hdmi_clk_cntr--;
	if (aud_hdmi_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_hdmi_clock();
#endif
	} else if (aud_hdmi_clk_cntr < 0) {
		pr_err("%s aud_hdmi_clk_cntr:%d<0\n", __func__, aud_hdmi_clk_cntr);
		AUDIO_ASSERT(true);
		aud_hdmi_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}
/*****************************************************************************
  * FUNCTION
  *  mt_afe_set_hdmi_clock_source
  *
  * DESCRIPTION
  *  Set HDMI Source Clock
  *
  *****************************************************************************/
void mt_afe_set_hdmi_clock_source(uint32_t SampleRate, int apllclksel) /*AudDrv_SetHDMIClkSource*/
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	uint32_t all_clk_rate = 98304000 ; /*48000*2048*/
	int ret = 0;

	if ((SampleRate == 44100) || (SampleRate == 88200) || (SampleRate == 176400))
		all_clk_rate = 90316800; /*44100*2048*/

	switch (apllclksel) {
	case APLL_D4:
		ret = clk_set_parent(aud_clks[CLOCK_TOP_APLL_SEL].clock,
			aud_clks[CLOCK_TOP_AUDPLL_D4].clock);
		if (ret)
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLOCK_TOP_APLL_SEL].name,
			aud_clks[CLOCK_TOP_AUDPLL_D4].name, ret);
		break;
	case APLL_D8:
		ret = clk_set_parent(aud_clks[CLOCK_TOP_APLL_SEL].clock,
			aud_clks[CLOCK_TOP_AUDPLL_D8].clock);
		if (ret)
			pr_err("%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLOCK_TOP_APLL_SEL].name,
				aud_clks[CLOCK_TOP_AUDPLL_D8].name, ret);
		break;
	case APLL_D24:
		ret = clk_set_parent(aud_clks[CLOCK_TOP_APLL_SEL].clock,
			aud_clks[CLOCK_TOP_AUDPLL_D24].clock);
		if (ret)
			pr_err("%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLOCK_TOP_APLL_SEL].name,
				aud_clks[CLOCK_TOP_AUDPLL_D24].name, ret);
		break;
	case APLL_D16:
	default: /* default 48k */
	/* APLL_DIV : 2048/128=16 */
		ret = clk_set_parent(aud_clks[CLOCK_TOP_APLL_SEL].clock,
			aud_clks[CLOCK_TOP_AUDPLL_D16].clock);
		if (ret)
			pr_err("%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLOCK_TOP_APLL_SEL].name,
				aud_clks[CLOCK_TOP_AUDPLL_D16].name, ret);
		break;
	}

	/*replace pll_fsel*/
	ret = clk_set_rate(aud_clks[CLOCK_TOP_AUDPLL].clock, all_clk_rate);
	if (ret)
		pr_err("%s clk_set_rate %s-%x fail %d\n",
			__func__, aud_clks[CLOCK_TOP_AUDPLL].name, all_clk_rate, ret);
#else
	uint32_t APLL_TUNER_N_INFO = AUDPLL_TUNER_N_98M;
	uint32_t apll_sdm_pcw = AUDPLL_SDM_PCW_98M; /* apll tuner always equal to sdm+1 */
	uint32_t ck_apll = 0;
	uint32_t u4HDMI_BCK_DIV;
	uint32_t BitWidth = 3;    /* default = 32 bits */

	u4HDMI_BCK_DIV = (128 / ((BitWidth + 1) * 8 * 2) / 2) - 1;
	if ((u4HDMI_BCK_DIV < 0) || (u4HDMI_BCK_DIV > 63))
		pr_err("%s u4HDMI_BCK_DIV is out of range.\n", __func__);
	ck_apll = apllclksel;
	if ((SampleRate == 44100) || (SampleRate == 88200) || (SampleRate == 176400)) {
		APLL_TUNER_N_INFO = AUDPLL_TUNER_N_90M;
		apll_sdm_pcw = AUDPLL_SDM_PCW_90M;
	}

	switch (apllclksel) {
	case APLL_D4:
		clkmux_sel(MT_MUX_APLL, 2, "AUDIO");
		break;
	case APLL_D8:
		clkmux_sel(MT_MUX_APLL, 3, "AUDIO");
		break;
	case APLL_D24:
		clkmux_sel(MT_MUX_APLL, 5, "AUDIO");
		break;
	case APLL_D16:
	default: /* default 48k */
		/* APLL_DIV : 2048/128=16 */
		clkmux_sel(MT_MUX_APLL, 4, "AUDIO");
		break;
	}
	/* Set APLL source clock SDM PCW info */
#ifdef PM_MANAGER_API
	pll_fsel(AUDPLL, apll_sdm_pcw);

	/* Set HDMI BCK DIV  , replaced by set_hdmi_bck_div*/
	/*mt_afe_set_reg(AUDIO_TOP_CON3, u4HDMI_BCK_DIV << HDMI_BCK_DIV_POS,
	((0x1 << HDMI_BCK_DIV_LEN) - 1) << HDMI_BCK_DIV_POS);*/
#else
	mt_afe_pll_set_reg(AUDPLL_CON1, apll_sdm_pcw << AUDPLL_SDM_PCW_POS,
		(0x1 << AUDPLL_SDM_PCW_LEN - 1) << AUDPLL_SDM_PCW_POS);
	/* Set APLL tuner clock N info */
	mt_afe_pll_set_reg(AUDPLL_CON3, APLL_TUNER_N_INFO << AUDPLL_TUNER_N_INFO_POS,
		(0x1 << AUDPLL_TUNER_N_INFO_LEN - 1) << AUDPLL_TUNER_N_INFO_POS);
	/* Set MCLK clock */
	mt_afe_topck_set_reg(CLK_CFG_5, ck_apll << CLK_APLL_SEL_POS,
		(0x1 << CLK_APLL_SEL_LEN - 1) << CLK_APLL_SEL_POS);
	/* Set HDMI BCK DIV, replaced by set_hdmi_bck_div */
	/*mt_afe_set_reg(AUDIO_TOP_CON3, u4HDMI_BCK_DIV << HDMI_BCK_DIV_POS,
		(0x1 << HDMI_BCK_DIV_LEN - 1) << HDMI_BCK_DIV_POS);*/
	/* Turn on APLL source clock */
	mt_afe_pll_set_reg(AUDPLL_CON3, 0x1 << AUDPLL_TUNER_EN_POS,
		(0x1 << 0x1 - 1) << AUDPLL_TUNER_EN_POS);
	/* pdn_apll enable turn on */
	mt_afe_topck_set_reg(CLK_CFG_5, 0x1 << PDN_APLL_POS,
		(0x1 << 0x1 - 1) << PDN_APLL_POS);
#endif
#endif
}


void mt_afe_top_apll_clk_on(void)/*AudDrv_TOP_Apll_Clk_On*/
{
        mutex_lock(&afe_clk_mutex);
	PRINTK_AUD_CLK("%s aud_top_apll_clk_cntr:%d\n", __func__, aud_top_apll_clk_cntr);

	if (aud_top_apll_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_top_apll_clock();
#endif
	}
	aud_top_apll_clk_cntr++;

	mutex_unlock(&afe_clk_mutex);
}

void mt_afe_top_apll_clk_off(void)/*AudDrv_TOP_Apll_Clk_Off*/
{
	mutex_lock(&afe_clk_mutex);
	PRINTK_AUD_CLK("%s aud_top_apll_clk_cntr:%d\n", __func__, aud_top_apll_clk_cntr);

	aud_top_apll_clk_cntr--;
	if (aud_top_apll_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_top_apll_clock();
#endif
	} else if (aud_top_apll_clk_cntr < 0) {
		pr_err("%s aud_top_apll_clk_cntr:%d<0\n", __func__, aud_top_apll_clk_cntr);
		AUDIO_ASSERT(true);
		aud_top_apll_clk_cntr = 0;
	}

	mutex_unlock(&afe_clk_mutex);
}


void mt_afe_aplltuner_clk_on(void)/*AudDrv_APLL_TUNER_Clk_On*/
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_apll_tuner_clk_cntr:%d\n", __func__, aud_apll_tuner_clk_cntr);

	if (aud_apll_tuner_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_apll_tuner_clock();
#endif
	}
	aud_apll_tuner_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_aplltuner_clk_off(void)/*AudDrv_APLL_TUNER_Clk_Off*/
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_apll_tuner_clk_cntr:%d\n", __func__, aud_apll_tuner_clk_cntr);

	aud_apll_tuner_clk_cntr--;
	if (aud_apll_tuner_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_apll_tuner_clock();
#endif
	} else if (aud_apll_tuner_clk_cntr < 0) {
		pr_err("%s aud_apll_tuner_clk_cntr:%d<0\n", __func__, aud_apll_tuner_clk_cntr);
		AUDIO_ASSERT(true);
		aud_apll_tuner_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}


/*****************************************************************************
  * FUNCTION
  *  mt_afe_spdif_clk_on / mt_afe_spdif_clk_off
  *
  * DESCRIPTION
  *  Enable/Disable SPDIF clock
  *
  *****************************************************************************/

void mt_afe_spdif_clk_on(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_spdif_clk_cntr:%d\n", __func__, aud_spdif_clk_cntr);

	if (aud_spdif_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_spdif_clock();
#endif
	}
	aud_spdif_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_spdif_clk_off(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	PRINTK_AUD_CLK("%s aud_spdif_clk_cntr:%d\n", __func__, aud_spdif_clk_cntr);

	aud_spdif_clk_cntr--;
	if (aud_spdif_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_spdif_clock();
#endif
	} else if (aud_spdif_clk_cntr < 0) {
		pr_err("%s aud_spdif_clk_cntr:%d<0\n", __func__, aud_spdif_clk_cntr);
		AUDIO_ASSERT(true);
		aud_spdif_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_suspend_clk_on(void)/*AudDrv_Suspend_Clk_On*/
{
#ifdef PM_MANAGER_API
	unsigned long flags;

	PRINTK_AUD_CLK("%s aud_afe_clk_cntr:%d\n", __func__, aud_afe_clk_cntr);

	spin_lock_irqsave(&afe_clk_lock, flags);

	if (aud_afe_clk_cntr > 0)
		turn_on_afe_clock();

	if (aud_i2s_clk_cntr > 0)
		turn_on_i2s_clock();

	if (aud_dac_clk_cntr > 0)
		turn_on_dac_clock();

	if (aud_adc_clk_cntr > 0)
		turn_on_adc_clock();

	if (aud_hdmi_clk_cntr > 0)
		turn_on_hdmi_clock();

	if (aud_spdif_clk_cntr > 0)
		turn_on_spdif_clock();


	if (aud_ana_clk_cntr > 0)
		mt_afe_ana_clk_on();

	spin_unlock_irqrestore(&afe_clk_lock, flags);

	mutex_lock(&afe_clk_mutex);

	mutex_unlock(&afe_clk_mutex);
#endif
}

void mt_afe_suspend_clk_off(void)/*AudDrv_Suspend_Clk_Off*/
{
#ifdef PM_MANAGER_API
	unsigned long flags;

	PRINTK_AUD_CLK("%s aud_afe_clk_cntr:%d\n", __func__, aud_afe_clk_cntr);

	mutex_lock(&afe_clk_mutex);

	mutex_unlock(&afe_clk_mutex);

	spin_lock_irqsave(&afe_clk_lock, flags);

	if (aud_i2s_clk_cntr > 0)
		turn_off_i2s_clock();

	if (aud_dac_clk_cntr > 0)
		turn_off_dac_clock();

	if (aud_adc_clk_cntr > 0)
		turn_off_adc_clock();

	if (aud_hdmi_clk_cntr > 0)
		turn_off_hdmi_clock();

	if (aud_spdif_clk_cntr > 0)
		turn_off_spdif_clock();

	if (aud_afe_clk_cntr > 0)
		turn_off_afe_clock();

	if (aud_ana_clk_cntr > 0)
		mt_afe_ana_clk_off();

	spin_unlock_irqrestore(&afe_clk_lock, flags);
#endif
}

void mt_afe_emi_clk_on(void)
{
#ifdef _MT_IDLE_HEADER
	mutex_lock(&emi_clk_mutex);
	if (aud_emi_clk_cntr == 0) {
		disable_dpidle_by_bit(MT_CG_AUDIO_AFE);
		disable_soidle_by_bit(MT_CG_AUDIO_AFE);
	}
	aud_emi_clk_cntr++;
	mutex_unlock(&emi_clk_mutex);
#endif
}

void mt_afe_emi_clk_off(void)
{
#ifdef _MT_IDLE_HEADER
	mutex_lock(&emi_clk_mutex);
	aud_emi_clk_cntr--;
	if (aud_emi_clk_cntr == 0) {
		enable_dpidle_by_bit(MT_CG_AUDIO_AFE);
		enable_soidle_by_bit(MT_CG_AUDIO_AFE);
	} else if (aud_emi_clk_cntr < 0) {
		pr_err("%s aud_emi_clk_cntr:%d<0\n", __func__, aud_emi_clk_cntr);
		aud_emi_clk_cntr = 0;
	}
	mutex_unlock(&emi_clk_mutex);
#endif
}

