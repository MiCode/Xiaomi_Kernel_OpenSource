/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
 * If not, see <http://www.gnu.org/licenses/>.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Clk.c
 *
 * Project:
 * --------
 *   MT6583  Audio Driver clock control implement
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang (MTK02308)
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Afe.h"
#include <linux/spinlock.h>
#include <linux/delay.h>

#ifdef _MT_IDLE_HEADER
#include "mt_idle.h"
#endif

#ifdef COMMON_CLOCK_FRAMEWORK_API
#include <linux/clk.h>
#endif

#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif

enum audio_system_clock_type {
	CLOCK_INFRA_SYS_AUDIO = 0,
	CLOCK_MUX_AUDIO,
	CLOCK_MUX_AUD_INTBUS,
	CLOCK_MUX_AUD_1,
	CLOCK_MUX_AUD_2,
	CLOCK_TOP_APLL1_CK,
	CLOCK_TOP_APLL2_CK,
	CLOCK_CLK26M,
	CLOCK_NUM
};

#ifdef COMMON_CLOCK_FRAMEWORK_API

struct audio_clock_attr {
	const char *name;
	const bool prepare_once;
	bool clk_prepared;
	struct clk *clock;
};

static struct audio_clock_attr aud_clks[CLOCK_NUM] = {
	[CLOCK_INFRA_SYS_AUDIO] = {"aud_infra_clk", true, false, NULL},
	[CLOCK_MUX_AUDIO] = {"top_mux_audio", true, false, NULL},
	[CLOCK_MUX_AUD_INTBUS] = {"top_mux_audio_intbus", true, false, NULL},
	[CLOCK_MUX_AUD_1] = {"aud_mux1_clk", false, false, NULL},
	[CLOCK_MUX_AUD_2] = {"aud_mux2_clk", false, false, NULL},
	[CLOCK_TOP_APLL1_CK] = {"top_apll1_clk", false, false, NULL},
	[CLOCK_TOP_APLL2_CK] = {"top_apll2_clk", false, false, NULL},
	[CLOCK_CLK26M] = {"top_clk26m_clk", false, false, NULL}
};

#endif


/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

int Aud_Core_Clk_cntr = 0;
int Aud_AFE_Clk_cntr = 0;
int Aud_I2S_Clk_cntr = 0;
int Aud_ADC_Clk_cntr = 0;
int Aud_ADC2_Clk_cntr = 0;
int Aud_ADC3_Clk_cntr = 0;
int Aud_ANA_Clk_cntr = 0;
int Aud_HDMI_Clk_cntr = 0;
int Aud_APLL22M_Clk_cntr = 0;
int Aud_APLL24M_Clk_cntr = 0;
int Aud_APLL1_Tuner_cntr = 0;
int Aud_APLL2_Tuner_cntr = 0;
static int Aud_EMI_cntr;

static DEFINE_SPINLOCK(auddrv_Clk_lock);

/* amp mutex lock */
static DEFINE_MUTEX(auddrv_pmic_mutex);
static DEFINE_MUTEX(audEMI_Clk_mutex);
static DEFINE_MUTEX(auddrv_Clk_mutex);


/* extern void disable_dpidle_by_bit(int id); */
/* extern void disable_soidle_by_bit(int id); */
/* extern void enable_dpidle_by_bit(int id); */
/* extern void enable_soidle_by_bit(int id); */

void AudDrv_Clk_AllOn(void)
{
	unsigned long flags;

	pr_debug("AudDrv_Clk_AllOn\n");
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0xffffffff);
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void Auddrv_Bus_Init(void)
{
	unsigned long flags;

	pr_debug("%s\n", __func__);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	/* must set, system will default set bit14 to 0 */
	Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

int Auddrv_Clk_Init(void *dev)
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
			aud_clks[i].clk_prepared = true;
		}
	}
#endif
	return ret;
}

void Auddrv_Clk_Deinit(void *dev)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	size_t i;

	pr_debug("%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		if (aud_clks[i].clock && !IS_ERR(aud_clks[i].clock) && aud_clks[i].clk_prepared) {
			clk_unprepare(aud_clks[i].clock);
			aud_clks[i].clk_prepared = false;
		}
	}
#endif
}


/*****************************************************************************
 * FUNCTION
 *  AudDrv_Clk_Power_On / AudDrv_Clk_Power_Off
 *
 * DESCRIPTION
 *  Power on this function , then all register can be access and  set.
 *
 *****************************************************************************
 */

void AudDrv_Clk_Power_On(void)
{
	volatile uint32 *AFE_Register = (volatile uint32 *)Get_Afe_Powertop_Pointer();
	volatile uint32 val_tmp;

	pr_debug("%s\n", __func__);
	val_tmp = 0xd;
	mt_reg_sync_writel(val_tmp, AFE_Register);
}

void AudDrv_Clk_Power_Off(void)
{
}


/*****************************************************************************
 * FUNCTION
 *  AudDrv_Clk_On / AudDrv_Clk_Off
 *
 * DESCRIPTION
 *  Enable/Disable PLL(26M clock) \ AFE clock
 *
 *****************************************************************************
 */
void AudDrv_Clk_On(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret;
#endif

	PRINTK_AUD_CLK("+AudDrv_Clk_On, Aud_AFE_Clk_cntr:%d\n", Aud_AFE_Clk_cntr);
	mutex_lock(&auddrv_Clk_mutex);
	if (Aud_AFE_Clk_cntr == 0) {
		PRINTK_AUD_CLK("-----------AudDrv_Clk_On, Aud_AFE_Clk_cntr:%d\n", Aud_AFE_Clk_cntr);

#if defined(COMMON_CLOCK_FRAMEWORK_API)
		ret = power_on_audsys();
		if (ret < 0)
			pr_err("%s power_on_audsys fail %d\n", __func__, ret);

		if (aud_clks[CLOCK_INFRA_SYS_AUDIO].clk_prepared) {
			ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
			if (ret)
				pr_err("%s clk_enable %s fail %d\n",
					__func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);
		} else {
			pr_err("%s [CCF]clk_prepare error clk_enable INFRA_SYS_AUDIO fail\n",
				__func__);
		}

		if (aud_clks[CLOCK_MUX_AUD_INTBUS].clk_prepared) {
			ret = clk_enable(aud_clks[CLOCK_MUX_AUD_INTBUS].clock);
			if (ret)
				pr_err("%s clk_enable %s fail %d\n",
					__func__, aud_clks[CLOCK_MUX_AUD_INTBUS].name, ret);
		} else {
			pr_err("%s [CCF]clk_prepare error clk_enable MUX_AUD_INTBUS fail\n",
				__func__);
		}

		if (aud_clks[CLOCK_MUX_AUDIO].clk_prepared) {
			ret = clk_enable(aud_clks[CLOCK_MUX_AUDIO].clock);
			if (ret)
				pr_err("%s clk_enable %s fail %d\n",
				__func__, aud_clks[CLOCK_MUX_AUDIO].name, ret);
		} else {
			pr_err("%s [CCF]clk_prepare error clk_enable MUX_AUDIO fail\n",
				__func__);
		}

		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 2, 1 << 2); /* enable AFE */
		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 25, 1 << 25); /* enable DAC */
		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 26, 1 << 26); /* enable DAC_PREDIS */
#else

#ifdef PM_MANAGER_API
		if (enable_clock(MT_CG_INFRA_AUDIO, "AUDIO"))
			PRINTK_AUD_CLK("%s Aud enable_clock MT_CG_INFRA_AUDIO fail\n", __func__);
		if (enable_clock(MT_CG_AUDIO_AFE, "AUDIO"))
			PRINTK_AUD_CLK("%s Aud enable_clock MT_CG_AUDIO_AFE fail\n", __func__);
		if (enable_clock(MT_CG_AUDIO_DAC, "AUDIO"))
			PRINTK_AUD_CLK("%s MT_CG_AUDIO_DAC fail", __func__);
		if (enable_clock(MT_CG_AUDIO_DAC_PREDIS, "AUDIO"))
			PRINTK_AUD_CLK("%s MT_CG_AUDIO_DAC_PREDIS fail", __func__);
#else
		/* bit 25=0, without 133m master and 66m slave bus clock cg gating */
		SetInfraCfg(AUDIO_CG_CLR, 0x2000000, 0x2000000);
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x4000, 0x06004044);
#endif

#endif
	}
	Aud_AFE_Clk_cntr++;
	mutex_unlock(&auddrv_Clk_mutex);
	PRINTK_AUD_CLK("-AudDrv_Clk_On, Aud_AFE_Clk_cntr:%d\n", Aud_AFE_Clk_cntr);
}
EXPORT_SYMBOL(AudDrv_Clk_On);

void AudDrv_Clk_Off(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret;
#endif

	PRINTK_AUD_CLK("+!! AudDrv_Clk_Off, Aud_AFE_Clk_cntr:%d\n", Aud_AFE_Clk_cntr);
	mutex_lock(&auddrv_Clk_mutex);
	Aud_AFE_Clk_cntr--;
	if (Aud_AFE_Clk_cntr == 0) {
		PRINTK_AUD_CLK("------------AudDrv_Clk_Off, Aud_AFE_Clk_cntr:%d\n", Aud_AFE_Clk_cntr);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 25, 1 << 25); /* disable DAC */
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 26, 1 << 26); /* disable DAC_PREDIS */
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 2, 1 << 2); /* disable AFE */

		if (aud_clks[CLOCK_MUX_AUDIO].clk_prepared)
			clk_disable(aud_clks[CLOCK_MUX_AUDIO].clock);
		if (aud_clks[CLOCK_MUX_AUD_INTBUS].clk_prepared)
			clk_disable(aud_clks[CLOCK_MUX_AUD_INTBUS].clock);
		if (aud_clks[CLOCK_INFRA_SYS_AUDIO].clk_prepared)
			clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

		ret = power_off_audsys();
		if (ret < 0)
			pr_err("%s power_off_audsys fail %d\n", __func__, ret);
#else

#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_AFE, "AUDIO"))
			PRINTK_AUD_CLK("%s disable_clock MT_CG_AUDIO_AFE fail\n", __func__);
		if (disable_clock(MT_CG_AUDIO_DAC, "AUDIO"))
			PRINTK_AUD_CLK("%s MT_CG_AUDIO_DAC fail\n", __func__);
		if (disable_clock(MT_CG_AUDIO_DAC_PREDIS, "AUDIO"))
			PRINTK_AUD_CLK("%s MT_CG_AUDIO_DAC_PREDIS fail\n", __func__);
		if (disable_clock(MT_CG_INFRA_AUDIO, "AUDIO"))
			PRINTK_AUD_CLK("%s disable_clock MT_CG_INFRA_AUDIO fail\n",
					       __func__);
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x06000044, 0x06000044);
			/* bit25=1, with 133m mastesr and 66m slave bus clock cg gating */
		SetInfraCfg(AUDIO_CG_SET, 0x2000000, 0x2000000);
#endif

#endif
	} else if (Aud_AFE_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_Clk_Off, Aud_AFE_Clk_cntr<0 (%d)\n", Aud_AFE_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_AFE_Clk_cntr = 0;
	}
	mutex_unlock(&auddrv_Clk_mutex);
	PRINTK_AUD_CLK("-!! AudDrv_Clk_Off, Aud_AFE_Clk_cntr:%d\n", Aud_AFE_Clk_cntr);
}
EXPORT_SYMBOL(AudDrv_Clk_Off);

/*****************************************************************************
 * FUNCTION
 *  AudDrv_ANA_Clk_On / AudDrv_ANA_Clk_Off
 *
 * DESCRIPTION
 *  Enable/Disable analog part clock
 *
 *****************************************************************************/
void AudDrv_ANA_Clk_On(void)
{
	mutex_lock(&auddrv_pmic_mutex);
	if (Aud_ANA_Clk_cntr == 0) {
		PRINTK_AUD_CLK("+AudDrv_ANA_Clk_On, Aud_ANA_Clk_cntr:%d\n", Aud_ANA_Clk_cntr);
		upmu_set_rg_clksq_en_aud(1);
	}
	Aud_ANA_Clk_cntr++;
	mutex_unlock(&auddrv_pmic_mutex);
	PRINTK_AUD_CLK("-AudDrv_ANA_Clk_Off, Aud_ANA_Clk_cntr:%d\n", Aud_ANA_Clk_cntr);
}
EXPORT_SYMBOL(AudDrv_ANA_Clk_On);

void AudDrv_ANA_Clk_Off(void)
{
	PRINTK_AUD_CLK("+AudDrv_ANA_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ANA_Clk_cntr);
	mutex_lock(&auddrv_pmic_mutex);
	Aud_ANA_Clk_cntr--;
	if (Aud_ANA_Clk_cntr == 0) {
		PRINTK_AUD_CLK("+AudDrv_ANA_Clk_Off disable_clock Ana clk(%x)\n", Aud_ANA_Clk_cntr);
		upmu_set_rg_clksq_en_aud(0);
		/* Disable ADC clock */
#ifdef PM_MANAGER_API
#else
		/* TODO:: open ADC clock.... */
#endif
	} else if (Aud_ANA_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_ANA_Clk_Off, Aud_ADC_Clk_cntr<0 (%d)\n",
				 Aud_ANA_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_ANA_Clk_cntr = 0;
	}
	mutex_unlock(&auddrv_pmic_mutex);
	PRINTK_AUD_CLK("-AudDrv_ANA_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ANA_Clk_cntr);
}
EXPORT_SYMBOL(AudDrv_ANA_Clk_Off);

/*****************************************************************************
 * FUNCTION
  *  AudDrv_ADC_Clk_On / AudDrv_ADC_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable analog part clock
  *
  *****************************************************************************/

void AudDrv_ADC_Clk_On(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+AudDrv_ADC_Clk_On, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_ADC_Clk_cntr == 0) {
		PRINTK_AUDDRV("+AudDrv_ADC_Clk_On enable_clock ADC clk(%x)\n", Aud_ADC_Clk_cntr);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 24, 1 << 24); /* enable ADC */
#else

#ifdef PM_MANAGER_API
		if (enable_clock(MT_CG_AUDIO_ADC, "AUDIO"))
			PRINTK_AUD_CLK("%s fail", __func__);
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 24, 1 << 24);
#endif

#endif
	}
	Aud_ADC_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-AudDrv_ADC_Clk_On, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr);
}

void AudDrv_ADC_Clk_Off(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+AudDrv_ADC_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_ADC_Clk_cntr--;
	if (Aud_ADC_Clk_cntr == 0) {
		PRINTK_AUDDRV("+AudDrv_ADC_Clk_On disable_clock ADC clk(%x)\n", Aud_ADC_Clk_cntr);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 24, 1 << 24); /* disable ADC */
#else

#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_ADC, "AUDIO"))
			PRINTK_AUD_CLK("%s fail", __func__);
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 24, 1 << 24);
#endif

#endif
	}
	if (Aud_ADC_Clk_cntr < 0) {
		PRINTK_AUDDRV("!! AudDrv_ADC_Clk_Off, Aud_ADC_Clk_cntr<0 (%d)\n", Aud_ADC_Clk_cntr);
		Aud_ADC_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-AudDrv_ADC_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr);
}

/*****************************************************************************
 * FUNCTION
  *  AudDrv_ADC2_Clk_On / AudDrv_ADC2_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable clock
  *
  *****************************************************************************/

void AudDrv_ADC2_Clk_On(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+%s %d\n", __func__, Aud_ADC2_Clk_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_ADC2_Clk_cntr == 0) {
		PRINTK_AUDDRV("+%s  enable_clock ADC2 clk(%x)\n", __func__, Aud_ADC2_Clk_cntr);
#if 0				/* removed */
#ifdef PM_MANAGER_API
		if (enable_clock(MT_CG_AUDIO_ADDA2, "AUDIO"))
			PRINTK_AUD_CLK("%s fail\n", __func__);
#else
		/* temp hard code setting, after confirm with enable clock usage, this could be removed. */
		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 23, 1 << 23);
#endif
#endif
	}
	Aud_ADC2_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-AudDrv_ADC2_Clk_On, Aud_ADC2_Clk_cntr:%d\n", Aud_ADC2_Clk_cntr);
}

void AudDrv_ADC2_Clk_Off(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+%s %d\n", __func__, Aud_ADC2_Clk_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_ADC2_Clk_cntr--;
	if (Aud_ADC2_Clk_cntr == 0) {
		PRINTK_AUDDRV("+%s disable_clock ADC2 clk(%x)\n", __func__, Aud_ADC2_Clk_cntr);
#if 0				/* removed */
#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_ADDA2, "AUDIO"))
			PRINTK_AUD_CLK("%s fail\n", __func__);
#else
		/* temp hard code setting, after confirm with enable clock usage, this could be removed. */
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 23, 1 << 23);
#endif
#endif
	}
	if (Aud_ADC2_Clk_cntr < 0) {
		pr_warn("%s  <0 (%d)\n", __func__, Aud_ADC2_Clk_cntr);
		Aud_ADC2_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-AudDrv_ADC2_Clk_Off, Aud_ADC2_Clk_cntr:%d\n", Aud_ADC2_Clk_cntr);
}


/*****************************************************************************
 * FUNCTION
  *  AudDrv_ADC3_Clk_On / AudDrv_ADC3_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable clock
  *
  *****************************************************************************/

void AudDrv_ADC3_Clk_On(void)
{
	PRINTK_AUD_CLK("+%s %d\n", __func__, Aud_ADC3_Clk_cntr);
	mutex_lock(&auddrv_pmic_mutex);
	if (Aud_ADC3_Clk_cntr == 0) {
		PRINTK_AUDDRV("+%s  enable_clock ADC3 clk(%x)\n", __func__, Aud_ADC3_Clk_cntr);
#if 0				/* removed */
#ifdef PM_MANAGER_API
		if (enable_clock(MT_CG_AUDIO_ADDA3, "AUDIO"))
			PRINTK_AUD_CLK("%s fail\n", __func__);
#endif
#endif
	}
	Aud_ADC2_Clk_cntr++;
	mutex_unlock(&auddrv_pmic_mutex);
}

void AudDrv_ADC3_Clk_Off(void)
{
	PRINTK_AUD_CLK("+%s %d\n", __func__, Aud_ADC3_Clk_cntr);
	mutex_lock(&auddrv_pmic_mutex);
	Aud_ADC3_Clk_cntr--;
	if (Aud_ADC3_Clk_cntr == 0) {
		PRINTK_AUDDRV("+%s disable_clock ADC3 clk(%x)\n", __func__, Aud_ADC3_Clk_cntr);
#if 0				/* removed */
#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_ADDA3, "AUDIO"))
			PRINTK_AUD_CLK("%s fail\n", __func__);
#endif
#endif
	}
	if (Aud_ADC3_Clk_cntr < 0) {
		pr_warn("%s  <0 (%d)\n", __func__, Aud_ADC3_Clk_cntr);
		Aud_ADC3_Clk_cntr = 0;
	}
	mutex_unlock(&auddrv_pmic_mutex);
	PRINTK_AUD_CLK("-AudDrv_ADC3_Clk_Off, Aud_ADC3_Clk_cntr:%d\n", Aud_ADC3_Clk_cntr);
}


/*****************************************************************************
 * FUNCTION
  *  AudDrv_APLL22M_Clk_On / AudDrv_APLL22M_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable clock
  *
  *****************************************************************************/

void AudDrv_APLL22M_Clk_On(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret;
#endif

	PRINTK_AUD_CLK("+%s %d\n", __func__, Aud_APLL22M_Clk_cntr);
	mutex_lock(&auddrv_Clk_mutex);
	if (Aud_APLL22M_Clk_cntr == 0) {
		PRINTK_AUDDRV("+%s  enable_clock APLL22M clk(%x)\n", __func__, Aud_APLL22M_Clk_cntr);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		ret = clk_prepare_enable(aud_clks[CLOCK_MUX_AUD_1].clock);
		if (ret)
			pr_err("%s clk_enable %s fail %d\n",
				__func__, aud_clks[CLOCK_MUX_AUD_1].name, ret);

		ret = clk_set_parent(aud_clks[CLOCK_MUX_AUD_1].clock,
			aud_clks[CLOCK_TOP_APLL1_CK].clock);

		if (ret)
			pr_err("%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLOCK_MUX_AUD_1].name,
				aud_clks[CLOCK_TOP_APLL1_CK].name, ret);

		ret = clk_set_rate(aud_clks[CLOCK_TOP_APLL1_CK].clock, 90316800);
		if (ret) {
			pr_err("%s clk_set_rate %s-90316800 fail %d\n",
				__func__, aud_clks[CLOCK_TOP_APLL1_CK].name, ret);
		}

		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 8, 1 << 8); /* enable 22M */
#else

#ifdef PM_MANAGER_API
#if 0				/* Todo now direct set registe in EnableApll */
		/* MT_MUX_AUD1  CLK_CFG_6 => [7]: pdn_aud_1 [15]: ,MT_MUX_AUD2: pdn_aud_2 */
		enable_mux(MT_MUX_AUD1, "AUDIO");
		/* select APLL1 ,hf_faud_1_ck is mux of 26M and APLL1_CK */
		clkmux_sel(MT_MUX_AUD1, 1, "AUDIO");
#endif
		pll_fsel(APLL1, 0xb7945ea6);	/* APLL1 90.3168M */
		/* pdn_aud_1 => power down hf_faud_1_ck, hf_faud_1_ck is mux of 26M and APLL1_CK */
		/* pdn_aud_2 => power down hf_faud_2_ck, hf_faud_2_ck is mux of 26M and APLL2_CK (D1 is WHPLL) */
		if (enable_clock(MT_CG_AUDIO_22M, "AUDIO"))
			PRINTK_AUD_CLK("%s fail\n", __func__);
#endif

#endif
	}
	Aud_APLL22M_Clk_cntr++;
	mutex_unlock(&auddrv_Clk_mutex);
	PRINTK_AUD_CLK("-%s %d\n", __func__, Aud_APLL22M_Clk_cntr);
}

void AudDrv_APLL22M_Clk_Off(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret;
#endif

	PRINTK_AUD_CLK("+%s %d\n", __func__, Aud_APLL22M_Clk_cntr);
	mutex_lock(&auddrv_Clk_mutex);
	Aud_APLL22M_Clk_cntr--;
	if (Aud_APLL22M_Clk_cntr == 0) {
		PRINTK_AUDDRV("+%s disable_clock APLL22M clk(%x)\n", __func__, Aud_APLL22M_Clk_cntr);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 8, 1 << 8); /* disable 22M */

		ret = clk_set_parent(aud_clks[CLOCK_MUX_AUD_1].clock, aud_clks[CLOCK_CLK26M].clock);
		if (ret)
			pr_err("%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLOCK_MUX_AUD_1].name,
				aud_clks[CLOCK_CLK26M].name, ret);

		clk_disable_unprepare(aud_clks[CLOCK_MUX_AUD_1].clock);
#else

#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_22M, "AUDIO"))
			PRINTK_AUD_CLK("%s fail\n", __func__);
#if 0				/* Todo now direct set registe in EnableApll */
		clkmux_sel(MT_MUX_AUD1, 0, "AUDIO");	/* select 26M */
		disable_mux(MT_MUX_AUD1, "AUDIO");
#endif
#endif

#endif
	}
	if (Aud_APLL22M_Clk_cntr < 0) {
		pr_warn("%s  <0 (%d)\n", __func__, Aud_APLL22M_Clk_cntr);
		Aud_APLL22M_Clk_cntr = 0;
	}
	mutex_unlock(&auddrv_Clk_mutex);
	PRINTK_AUD_CLK("-%s %d\n", __func__, Aud_APLL22M_Clk_cntr);
}


/*****************************************************************************
 * FUNCTION
  *  AudDrv_APLL24M_Clk_On / AudDrv_APLL24M_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable clock
  *
  *****************************************************************************/

void AudDrv_APLL24M_Clk_On(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret;
#endif

	PRINTK_AUD_CLK("+%s %d\n", __func__, Aud_APLL24M_Clk_cntr);
	mutex_lock(&auddrv_Clk_mutex);
	if (Aud_APLL24M_Clk_cntr == 0) {
		PRINTK_AUDDRV("+%s  enable_clock APLL24M clk(%x)\n", __func__, Aud_APLL24M_Clk_cntr);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		ret = clk_prepare_enable(aud_clks[CLOCK_MUX_AUD_2].clock);
		if (ret)
			pr_err("%s clk_enable %s fail %d\n",
				__func__, aud_clks[CLOCK_MUX_AUD_2].name, ret);

		ret = clk_set_parent(aud_clks[CLOCK_MUX_AUD_2].clock,
			     aud_clks[CLOCK_TOP_APLL2_CK].clock);
		if (ret)
			pr_err("%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLOCK_MUX_AUD_2].name,
				aud_clks[CLOCK_TOP_APLL2_CK].name, ret);

		ret = clk_set_rate(aud_clks[CLOCK_TOP_APLL2_CK].clock, 98303999);
		if (ret) {
			pr_err("%s clk_set_rate %s-98303000 fail %d\n",
				__func__, aud_clks[CLOCK_TOP_APLL2_CK].name, ret);
		}

		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 9, 1 << 9); /* enable 24M */
#else

#ifdef PM_MANAGER_API
#if 0				/* Todo, now directly set register */
		enable_mux(MT_MUX_AUD1, "AUDIO");
		clkmux_sel(MT_MUX_AUD1, 1, "AUDIO");	/* hf_faud_1_ck apll1_ck */
#endif
		pll_fsel(APLL1, 0xbc7ea932);	/* ALPP1 98.304M */
		if (enable_clock(MT_CG_AUDIO_24M, "AUDIO"))
			PRINTK_AUD_CLK("%s fail\n", __func__);
#endif

#endif
	}
	Aud_APLL24M_Clk_cntr++;
	mutex_unlock(&auddrv_Clk_mutex);
	PRINTK_AUD_CLK("-%s %d\n", __func__, Aud_APLL24M_Clk_cntr);
}

void AudDrv_APLL24M_Clk_Off(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret;
#endif

	PRINTK_AUD_CLK("+%s %d\n", __func__, Aud_APLL24M_Clk_cntr);
	mutex_lock(&auddrv_Clk_mutex);
	Aud_APLL24M_Clk_cntr--;
	if (Aud_APLL24M_Clk_cntr == 0) {
		PRINTK_AUDDRV("+%s disable_clock APLL24M clk(%x)\n", __func__, Aud_APLL24M_Clk_cntr);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 9, 1 << 9); /* disable 24M */

		ret = clk_set_parent(aud_clks[CLOCK_MUX_AUD_2].clock, aud_clks[CLOCK_CLK26M].clock);

		if (ret)
			pr_err("%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLOCK_MUX_AUD_2].name,
				aud_clks[CLOCK_CLK26M].name, ret);

		clk_disable_unprepare(aud_clks[CLOCK_MUX_AUD_2].clock);
#else

#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_24M, "AUDIO"))
			PRINTK_AUD_CLK("%s fail\n", __func__);
#if 0				/* Todo, now directly set register */
		clkmux_sel(MT_MUX_AUD1, 0, "AUDIO");	/* select 26M */
		disable_mux(MT_MUX_AUD1, "AUDIO");
#endif
#endif

#endif
	}
	if (Aud_APLL24M_Clk_cntr < 0) {
		PRINTK_AUDDRV("%s  <0 (%d)\n", __func__, Aud_APLL24M_Clk_cntr);
		Aud_APLL24M_Clk_cntr = 0;
	}
	mutex_unlock(&auddrv_Clk_mutex);
	PRINTK_AUD_CLK("-%s %d\n", __func__, Aud_APLL24M_Clk_cntr);
}


/*****************************************************************************
  * FUNCTION
  *  AudDrv_I2S_Clk_On / AudDrv_I2S_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable analog part clock
  *
  *****************************************************************************/
void AudDrv_I2S_Clk_On(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+AudDrv_I2S_Clk_On, Aud_I2S_Clk_cntr:%d\n", Aud_I2S_Clk_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_I2S_Clk_cntr == 0) {
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 6, 1 << 6);	/* enable I2S clock */
#else

#ifdef PM_MANAGER_API
		if (enable_clock(MT_CG_AUDIO_I2S, "AUDIO"))
			PRINTK_AUD_ERROR("Aud enable_clock MT65XX_PDN_AUDIO_I2S fail !!!\n");
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00000000, 0x00000040);	/* power on I2S clock */
#endif

#endif
	}
	Aud_I2S_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-AudDrv_I2S_Clk_Off, Aud_I2S_Clk_cntr:%d\n", Aud_I2S_Clk_cntr);
}
EXPORT_SYMBOL(AudDrv_I2S_Clk_On);

void AudDrv_I2S_Clk_Off(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+AudDrv_I2S_Clk_Off, Aud_I2S_Clk_cntr:%d\n", Aud_I2S_Clk_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_I2S_Clk_cntr--;
	if (Aud_I2S_Clk_cntr == 0) {
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 6, 1 << 6);	/* disable I2S clock */
#else

#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_I2S, "AUDIO"))
			PRINTK_AUD_ERROR("disable_clock MT_CG_AUDIO_I2S fail\n");
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00000040, 0x00000040);	/* power off I2S clock */
#endif

#endif
	} else if (Aud_I2S_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_I2S_Clk_Off, Aud_I2S_Clk_cntr<0 (%d)\n",
				 Aud_I2S_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_I2S_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-AudDrv_I2S_Clk_Off, Aud_I2S_Clk_cntr:%d\n", Aud_I2S_Clk_cntr);
}
EXPORT_SYMBOL(AudDrv_I2S_Clk_Off);

/*****************************************************************************
  * FUNCTION
  *  AudDrv_Core_Clk_On / AudDrv_Core_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable analog part clock
  *
  *****************************************************************************/

void AudDrv_Core_Clk_On(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr);

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_Core_Clk_cntr == 0) {
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 2, 1 << 2); /* enable AFE */
#else
#ifdef PM_MANAGER_API
		if (enable_clock(MT_CG_AUDIO_AFE, "AUDIO"))
			PRINTK_AUD_ERROR
			    ("AudDrv_Core_Clk_On Aud enable_clock MT_CG_AUDIO_AFE fail !!!\n");
#endif
#endif
	}
	Aud_Core_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr);
}


void AudDrv_Core_Clk_Off(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_Core_Clk_cntr == 0) {
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 2, 1 << 2); /* disable AFE */
#else
#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_AFE, "AUDIO"))
			PRINTK_AUD_ERROR
			    ("AudDrv_Core_Clk_On Aud disable_clock MT_CG_AUDIO_AFE fail !!!\n");
#endif
#endif
	}
	Aud_Core_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr);
}

void AudDrv_APLL1Tuner_Clk_On(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+%s, Aud_APLL1_Tuner_cntr:%d\n", __func__, Aud_APLL1_Tuner_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_APLL1_Tuner_cntr == 0) {
		PRINTK_AUDDRV("+AudDrv_APLLTuner_Clk_On, Aud_APLL1_Tuner_cntr:%d\n",
			       Aud_APLL1_Tuner_cntr);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 19, 1 << 19);
#else
#ifdef PM_MANAGER_API
		if (enable_clock(MT_CG_AUDIO_APLL_TUNER, "AUDIO"))
			PRINTK_AUD_CLK("%s fail", __func__);
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x0 << 19, 0x1 << 19);
#endif
#endif
		Afe_Set_Reg(AFE_APLL1_TUNER_CFG, 0x00008033, 0x0000FFF7);
		SetpllCfg(AP_PLL_CON5, 1 << 0, 1 << 0);
	}
	Aud_APLL1_Tuner_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-%s, Aud_APLL1_Tuner_cntr:%d\n", __func__, Aud_APLL1_Tuner_cntr);
}

void AudDrv_APLL1Tuner_Clk_Off(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+%s, Aud_APLL1_Tuner_cntr:%d\n", __func__, Aud_APLL1_Tuner_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_APLL1_Tuner_cntr--;
	if (Aud_APLL1_Tuner_cntr == 0) {
		PRINTK_AUDDRV("+%s, Aud_APLL1_Tuner_cntr:%d\n", __func__,
			       Aud_APLL1_Tuner_cntr);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 19, 1 << 19);
#else
#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_APLL_TUNER, "AUDIO"))
			PRINTK_AUD_CLK("%s fail\n", __func__);
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x1 << 19, 0x1 << 19);
#endif
#endif
		SetpllCfg(AP_PLL_CON5, 0 << 0, 1 << 0);
		/* Afe_Set_Reg(AFE_APLL1_TUNER_CFG, 0x00000033, 0x1 << 19); */
	}
	/* handle for clock error */
	else if (Aud_APLL1_Tuner_cntr < 0) {
		pr_err("!! AudDrv_APLLTuner_Clk_Off, Aud_APLL1_Tuner_cntr<0 (%d)\n",
				 Aud_APLL1_Tuner_cntr);
		Aud_APLL1_Tuner_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-%s, Aud_APLL1_Tuner_cntr:%d\n", __func__, Aud_APLL1_Tuner_cntr);
}


void AudDrv_APLL2Tuner_Clk_On(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+%s, Aud_APLL2_Tuner_cntr:%d\n", __func__, Aud_APLL2_Tuner_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_APLL2_Tuner_cntr == 0) {
		PRINTK_AUDDRV("+%s, Aud_APLL2_Tuner_cntr:%d\n", __func__,
			       Aud_APLL2_Tuner_cntr);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 18, 1 << 18);
#else
#ifdef PM_MANAGER_API
		if (enable_clock(MT_CG_AUDIO_APLL2_TUNER, "AUDIO"))
			PRINTK_AUD_CLK("%s fail\n", __func__);
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x0 << 18, 0x1 << 18);
#endif
#endif
		Afe_Set_Reg(AFE_APLL2_TUNER_CFG, 0x00000035, 0x0000FFF7);
		SetpllCfg(AP_PLL_CON5, 1 << 1, 1 << 1);
	}
	Aud_APLL2_Tuner_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-%s, Aud_APLL2_Tuner_cntr:%d\n", __func__, Aud_APLL2_Tuner_cntr);
}

void AudDrv_APLL2Tuner_Clk_Off(void)
{
	unsigned long flags;

	PRINTK_AUD_CLK("+%s, Aud_APLL2_Tuner_cntr:%d\n", __func__, Aud_APLL2_Tuner_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_APLL2_Tuner_cntr--;
	if (Aud_APLL2_Tuner_cntr == 0) {
		PRINTK_AUDDRV("+%s, Aud_APLL2_Tuner_cntr:%d\n", __func__,
			       Aud_APLL2_Tuner_cntr);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 18, 1 << 18);
#else
#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_APLL2_TUNER, "AUDIO"))
			PRINTK_AUD_CLK("%s fail\n", __func__);
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x1 << 18, 0x1 << 18);
#endif
#endif
		SetpllCfg(AP_PLL_CON5, 0 << 0, 0 << 0);
	}
	/* handle for clock error */
	else if (Aud_APLL2_Tuner_cntr < 0) {
		pr_err("!! AudDrv_APLL2Tuner_Clk_Off, Aud_APLL1_Tuner_cntr<0 (%d)\n",
				 Aud_APLL2_Tuner_cntr);
		Aud_APLL2_Tuner_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("-%s, Aud_APLL2_Tuner_cntr:%d\n", __func__, Aud_APLL2_Tuner_cntr);
}

/*****************************************************************************
  * FUNCTION
  *  AudDrv_HDMI_Clk_On / AudDrv_HDMI_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable analog part clock
  *
  *****************************************************************************/

void AudDrv_HDMI_Clk_On(void)
{
	PRINTK_AUD_CLK("+AudDrv_HDMI_Clk_On, Aud_HDMI_Clk_cntr:%d\n", Aud_HDMI_Clk_cntr);
	if (Aud_HDMI_Clk_cntr == 0) {
		AudDrv_ANA_Clk_On();
		AudDrv_Clk_On();
	}
	Aud_HDMI_Clk_cntr++;
}

void AudDrv_HDMI_Clk_Off(void)
{
	PRINTK_AUD_CLK("+AudDrv_HDMI_Clk_Off, Aud_HDMI_Clk_cntr:%d\n", Aud_HDMI_Clk_cntr);
	Aud_HDMI_Clk_cntr--;
	if (Aud_HDMI_Clk_cntr == 0) {
		AudDrv_ANA_Clk_Off();
		AudDrv_Clk_Off();
	} else if (Aud_HDMI_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_HDMI_Clk_Off, Aud_HDMI_Clk_cntr<0 (%d)\n",
				 Aud_HDMI_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_HDMI_Clk_cntr = 0;
	}
	PRINTK_AUD_CLK("-AudDrv_HDMI_Clk_Off, Aud_HDMI_Clk_cntr:%d\n", Aud_HDMI_Clk_cntr);
}

/*****************************************************************************
* FUNCTION
*  AudDrv_Suspend_Clk_Off / AudDrv_Suspend_Clk_On
*
* DESCRIPTION
*  Enable/Disable AFE clock for suspend
*
*****************************************************************************
*/

void AudDrv_Suspend_Clk_Off(void)
{
	mutex_lock(&auddrv_Clk_mutex);
	if (Aud_Core_Clk_cntr > 0) {
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		if (Aud_AFE_Clk_cntr > 0) {
			Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 25, 1 << 25); /* disable DAC */
			Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 26, 1 << 26); /* disable DAC_PREDIS */
			Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 2, 1 << 2); /* disable AFE */
		}

		if (Aud_I2S_Clk_cntr > 0)
			Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 6, 1 << 6);	/* disable I2S clock */

		if (Aud_ADC_Clk_cntr > 0)
			Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 24, 1 << 24);

		if (Aud_APLL22M_Clk_cntr > 0) {
			int ret;

			Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 8, 1 << 8); /* disable 22M */
			Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 19, 1 << 19); /* disable APLL1 Tuner */

			ret = clk_set_parent(aud_clks[CLOCK_MUX_AUD_1].clock, aud_clks[CLOCK_CLK26M].clock);
			if (ret)
				pr_err("%s clk_set_parent %s-%s fail %d\n",
					__func__, aud_clks[CLOCK_MUX_AUD_1].name,
					aud_clks[CLOCK_CLK26M].name, ret);

			clk_disable_unprepare(aud_clks[CLOCK_MUX_AUD_1].clock);
		}
		if (Aud_APLL24M_Clk_cntr > 0) {
			int ret;

			Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 9, 1 << 9); /* disable 24M */
			Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 18, 1 << 18); /* disable APLL2 Tuner */

			ret = clk_set_parent(aud_clks[CLOCK_MUX_AUD_2].clock, aud_clks[CLOCK_CLK26M].clock);

			if (ret)
				pr_err("%s clk_set_parent %s-%s fail %d\n",
					__func__, aud_clks[CLOCK_MUX_AUD_2].name,
					aud_clks[CLOCK_CLK26M].name, ret);

			clk_disable_unprepare(aud_clks[CLOCK_MUX_AUD_2].clock);
		}
#else
#ifdef PM_MANAGER_API
		if (Aud_AFE_Clk_cntr > 0) {
			if (disable_clock(MT_CG_AUDIO_AFE, "AUDIO"))
				PRINTK_AUD_ERROR("Aud enable_clock MT_CG_AUDIO_AFE fail !!!\n");
		}
		if (Aud_I2S_Clk_cntr > 0) {
			if (disable_clock(MT_CG_AUDIO_I2S, "AUDIO"))
				PRINTK_AUD_ERROR("disable_clock MT_CG_AUDIO_I2S fail\n");
		}
		if (Aud_ADC_Clk_cntr > 0)
			Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 24, 1 << 24);
		if (Aud_ADC2_Clk_cntr > 0) {
#if 0				/* 6752 removed */
			if (disable_clock(MT_CG_AUDIO_ADDA2, "AUDIO"))
				PRINTK_AUD_CLK("%s fail", __func__);
#endif
		}
		if (Aud_ADC3_Clk_cntr > 0) {
#if 0				/* 6752 removed */
			if (disable_clock(MT_CG_AUDIO_ADDA3, "AUDIO"))
				PRINTK_AUD_CLK("%s fail", __func__);
#endif
		}

		if (Aud_APLL22M_Clk_cntr > 0) {
			if (disable_clock(MT_CG_AUDIO_22M, "AUDIO"))
				PRINTK_AUD_CLK("%s fail\n", __func__);
			if (disable_clock(MT_CG_AUDIO_APLL_TUNER, "AUDIO"))
				PRINTK_AUD_CLK("%s fail\n", __func__);
			clkmux_sel(MT_MUX_AUD1, 0, "AUDIO");	/* select 26M */
			disable_mux(MT_MUX_AUD1, "AUDIO");
		}
		if (Aud_APLL24M_Clk_cntr > 0) {
			if (disable_clock(MT_CG_AUDIO_24M, "AUDIO"))
				PRINTK_AUD_CLK("%s fail\n", __func__);
			if (disable_clock(MT_CG_AUDIO_APLL2_TUNER, "AUDIO"))
				PRINTK_AUD_CLK("%s fail\n", __func__);
			clkmux_sel(MT_MUX_AUD2, 0, "AUDIO");	/* select 26M */
			disable_mux(MT_MUX_AUD2, "AUDIO");
		}
#endif
#endif
	}
	mutex_unlock(&auddrv_Clk_mutex);
}

void AudDrv_Suspend_Clk_On(void)
{
	mutex_lock(&auddrv_Clk_mutex);
	if (Aud_Core_Clk_cntr > 0) {
#if defined(COMMON_CLOCK_FRAMEWORK_API)
		if (Aud_AFE_Clk_cntr > 0) {
			Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 2, 1 << 2); /* disable AFE */
			Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 25, 1 << 25); /* disable DAC */
			Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 26, 1 << 26); /* disable DAC_PREDIS */
		}

		if (Aud_I2S_Clk_cntr > 0)
			Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 6, 1 << 6);	/* disable I2S clock */

		if (Aud_ADC_Clk_cntr > 0)
			Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 24, 1 << 24);

		if (Aud_APLL22M_Clk_cntr > 0) {
			int ret = clk_prepare_enable(aud_clks[CLOCK_MUX_AUD_1].clock);

			if (ret)
				pr_err("%s clk_prepare_enable %s fail %d\n",
					__func__, aud_clks[CLOCK_MUX_AUD_1].name, ret);

			ret = clk_set_parent(aud_clks[CLOCK_MUX_AUD_1].clock,
				aud_clks[CLOCK_TOP_APLL1_CK].clock);

			if (ret)
				pr_err("%s clk_set_parent %s-%s fail %d\n",
					__func__, aud_clks[CLOCK_MUX_AUD_1].name,
					aud_clks[CLOCK_TOP_APLL1_CK].name, ret);

			ret = clk_set_rate(aud_clks[CLOCK_TOP_APLL1_CK].clock, 90316800);
			if (ret) {
				pr_err("%s clk_set_rate %s-90316800 fail %d\n",
					__func__, aud_clks[CLOCK_TOP_APLL1_CK].name, ret);
			}

			Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 8, 1 << 8); /* enable 22M */
			Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 19, 1 << 19); /* enable APLL1 Tuner */
		}
		if (Aud_APLL24M_Clk_cntr > 0) {
			int ret = clk_prepare_enable(aud_clks[CLOCK_MUX_AUD_2].clock);

			if (ret)
				pr_err("%s clk_prepare_enable %s fail %d\n",
					__func__, aud_clks[CLOCK_MUX_AUD_2].name, ret);

			ret = clk_set_parent(aud_clks[CLOCK_MUX_AUD_2].clock,
				aud_clks[CLOCK_TOP_APLL2_CK].clock);

			if (ret)
				pr_err("%s clk_set_parent %s-%s fail %d\n",
					__func__, aud_clks[CLOCK_MUX_AUD_2].name,
					aud_clks[CLOCK_TOP_APLL2_CK].name, ret);

			ret = clk_set_rate(aud_clks[CLOCK_TOP_APLL2_CK].clock, 98303999);
			if (ret) {
				pr_err("%s clk_set_rate %s-98303000 fail %d\n",
					__func__, aud_clks[CLOCK_TOP_APLL2_CK].name, ret);
			}

			Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 9, 1 << 9); /* enable 24M */
			Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 18, 1 << 18); /* enable APLL2 Tuner */
		}
#else
#ifdef PM_MANAGER_API
		if (Aud_AFE_Clk_cntr > 0) {
			if (enable_clock(MT_CG_AUDIO_AFE, "AUDIO"))
				PRINTK_AUD_ERROR("Aud enable_clock MT_CG_AUDIO_AFE fail !!!\n");
		}
		if (Aud_I2S_Clk_cntr > 0) {
			if (enable_clock(MT_CG_AUDIO_I2S, "AUDIO"))
				PRINTK_AUD_ERROR("enable_clock MT_CG_AUDIO_I2S fail\n");
		}

		if (Aud_ADC_Clk_cntr > 0)
			Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 24, 1 << 24);

		if (Aud_ADC2_Clk_cntr > 0) {
#if 0				/* 6752 removed */
			if (enable_clock(MT_CG_AUDIO_ADDA2, "AUDIO"))
				PRINTK_AUD_CLK("%s fail", __func__);
#endif
		}
		if (Aud_ADC3_Clk_cntr > 0) {
#if 0				/* 6752 removed */
			if (enable_clock(MT_CG_AUDIO_ADDA3, "AUDIO"))
				PRINTK_AUD_CLK("%s fail", __func__);
#endif
		}

		if (Aud_APLL22M_Clk_cntr > 0) {
			enable_mux(MT_MUX_AUD1, "AUDIO");
			clkmux_sel(MT_MUX_AUD1, 1, "AUDIO");	/* select APLL1 */
			if (enable_clock(MT_CG_AUDIO_22M, "AUDIO"))
				PRINTK_AUD_CLK("%s fail\n", __func__);
			if (enable_clock(MT_CG_AUDIO_APLL_TUNER, "AUDIO"))
				PRINTK_AUD_CLK("%s fail\n", __func__);
		}
		if (Aud_APLL24M_Clk_cntr > 0) {
			enable_mux(MT_MUX_AUD2, "AUDIO");
			clkmux_sel(MT_MUX_AUD2, 1, "AUDIO");	/* APLL2 */
			if (enable_clock(MT_CG_AUDIO_24M, "AUDIO"))
				PRINTK_AUD_CLK("%s fail\n", __func__);
			if (enable_clock(MT_CG_AUDIO_APLL2_TUNER, "AUDIO"))
				PRINTK_AUD_CLK("%s fail\n", __func__);
		}
#endif
#endif
	}
	mutex_unlock(&auddrv_Clk_mutex);
}

void AudDrv_Emi_Clk_On(void)
{
	PRINTK_AUD_CLK("+AudDrv_Emi_Clk_On, Aud_EMI_cntr:%d\n", Aud_EMI_cntr);
	mutex_lock(&auddrv_pmic_mutex);
	if (Aud_EMI_cntr == 0) {
#ifndef CONFIG_FPGA_EARLY_PORTING	/* george early porting disable */
#ifdef _MT_IDLE_HEADER
		disable_dpidle_by_bit(MT_CG_AUDIO_AFE);
		disable_soidle_by_bit(MT_CG_AUDIO_AFE);
#endif
#endif
	}
	Aud_EMI_cntr++;
	mutex_unlock(&auddrv_pmic_mutex);
}

void AudDrv_Emi_Clk_Off(void)
{
	PRINTK_AUD_CLK("+AudDrv_Emi_Clk_Off, Aud_EMI_cntr:%d\n", Aud_EMI_cntr);
	mutex_lock(&auddrv_pmic_mutex);
	Aud_EMI_cntr--;
	if (Aud_EMI_cntr == 0) {
#ifndef CONFIG_FPGA_EARLY_PORTING	/* george early porting disable */
#ifdef _MT_IDLE_HEADER
		enable_dpidle_by_bit(MT_CG_AUDIO_AFE);
		enable_soidle_by_bit(MT_CG_AUDIO_AFE);
#endif
#endif
	}
	if (Aud_EMI_cntr < 0) {
		Aud_EMI_cntr = 0;
		pr_debug("Aud_EMI_cntr = %d\n", Aud_EMI_cntr);
	}
	mutex_unlock(&auddrv_pmic_mutex);
}

