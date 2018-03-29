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
 *   MT6797  Audio Driver clock control implement
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

#ifndef CONFIG_MTK_CLKMGR
#include <linux/clk.h>
#else
#include <mach/mt_clkmgr.h>
#endif

/*#include <mach/mt_pm_ldo.h>*/
/*#include <mach/pmic_mt6325_sw.h>
#include <mach/upmu_common.h>
#include <mach/upmu_hw.h>*/

#include "AudDrv_Common.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Afe.h"
#include <linux/spinlock.h>
#include <linux/delay.h>
#ifdef _MT_IDLE_HEADER
#include "mt_idle.h"
#include "mt_clk_id.h"
#endif
#include <linux/err.h>
#include <linux/platform_device.h>

/* do not BUG on during FPGA or CCF not ready*/
#ifdef CONFIG_FPGA_EARLY_PORTING
#ifdef BUG
#undef BUG
#define BUG()
#endif
#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

int Aud_Core_Clk_cntr = 0;
int Aud_AFE_Clk_cntr = 0;
int Aud_I2S_Clk_cntr = 0;
int Aud_TDM_Clk_cntr = 0;
int Aud_ADC_Clk_cntr = 0;
int Aud_ADC2_Clk_cntr = 0;
int Aud_ADC3_Clk_cntr = 0;
int Aud_ADC_HIRES_Clk_cntr = 0;
int Aud_ANA_Clk_cntr = 0;
int Aud_HDMI_Clk_cntr = 0;
int Aud_APLL22M_Clk_cntr = 0;
int Aud_APLL24M_Clk_cntr = 0;
int Aud_APLL1_Tuner_cntr = 0;
int Aud_APLL2_Tuner_cntr = 0;
static int Aud_EMI_cntr;
int Aud_ANC_Clk_cntr = 0;

static DEFINE_SPINLOCK(auddrv_Clk_lock);

/* amp mutex lock */
static DEFINE_MUTEX(auddrv_pmic_mutex);
static DEFINE_MUTEX(audEMI_Clk_mutex);

enum audio_system_clock_type {
	CLOCK_AFE = 0,
/*	CLOCK_I2S,*/
	CLOCK_DAC,
	CLOCK_DAC_PREDIS,
	CLOCK_ADC,
	CLOCK_TML,
	CLOCK_APLL22M,
	CLOCK_APLL24M,
	CLOCK_APLL1_TUNER,
	CLOCK_APLL2_TUNER,
/*	CLOCK_TDM,*/
	CLOCK_ADC_HIRES,
	CLOCK_ADC_HIRES_TML,
	CLOCK_SCP_SYS_AUD,
	CLOCK_INFRA_SYS_AUDIO,
	CLOCK_INFRA_SYS_AUDIO_26M,
	CLOCK_INFRA_SYS_AUDIO_26M_PAD_TOP,
	CLOCK_INFRA_ANC_MD32,
	CLOCK_INFRA_ANC_MD32_32K,
	CLOCK_TOP_AUD_MUX1,
	CLOCK_TOP_AUD_MUX2,
	CLOCK_TOP_AD_APLL1_CK,
	CLOCK_TOP_AD_APLL2_CK,
	CLOCK_MUX_AUDIO,
	CLOCK_TOP_SYSPLL3_D4,
	CLOCK_MUX_AUDIOINTBUS,
	CLOCK_TOP_SYSPLL1_D4,
	CLOCK_TOP_MUX_ANC_MD32,
	CLOCK_TOP_SYSPLL1_D2,
	CLOCK_APMIXED_APLL1_CK,
	CLOCK_APMIXED_APLL2_CK,
	CLOCK_CLK26M,
	CLOCK_NUM
};

struct audio_clock_attr {
	const char *name;
	bool clk_prepare;
	bool clk_status;
	struct clk *clock;
};

static struct audio_clock_attr aud_clks[CLOCK_NUM] = {
	[CLOCK_AFE] = {"aud_afe_clk", false, false, NULL},
/*	[CLOCK_I2S] = {"aud_i2s_clk", false, false, NULL},*/		/* AudDrv_I2S_Clk_On, suspend */
	[CLOCK_DAC] = {"aud_dac_clk", false, false, NULL},			/* AudDrv_Clk_On only */
	[CLOCK_DAC_PREDIS] = {"aud_dac_predis_clk", false, false, NULL},	/* AudDrv_Clk_On only */
	[CLOCK_ADC] = {"aud_adc_clk", false, false, NULL},			/* AudDrv_ADC_Clk_On only */
	[CLOCK_TML] = {"aud_tml_clk", false, false, NULL},			/* NOT USED */
	[CLOCK_APLL22M] = {"aud_apll22m_clk", false, false, NULL},
	[CLOCK_APLL24M] = {"aud_apll24m_clk", false, false, NULL},
	[CLOCK_APLL1_TUNER] = {"aud_apll1_tuner_clk", false, false, NULL},
	[CLOCK_APLL2_TUNER] = {"aud_apll2_tuner_clk", false, false, NULL},
/*	[CLOCK_TDM] = {"aud_tdm_clk", false, false, NULL},*/
	[CLOCK_ADC_HIRES] = {"aud_adc_hires_clk", false, false, NULL}, /* use this clock when HIRES */
	[CLOCK_ADC_HIRES_TML] = {"aud_adc_hires_tml_clk", false, false, NULL}, /* use this clock when HIRES */
	[CLOCK_SCP_SYS_AUD] = {"scp_sys_audio", false, false, NULL},
	[CLOCK_INFRA_SYS_AUDIO] = {"aud_infra_clk", false, false, NULL},
	[CLOCK_INFRA_SYS_AUDIO_26M] = {"aud_infra_26m", false, false, NULL},
	[CLOCK_INFRA_SYS_AUDIO_26M_PAD_TOP] = {"aud_infra_26m_pad_top", false, false, NULL},
	[CLOCK_INFRA_ANC_MD32] = {"aud_infra_anc_md32", false, false, NULL},
	[CLOCK_INFRA_ANC_MD32_32K] = {"aud_infra_anc_md32_32k", false, false, NULL},
	[CLOCK_TOP_AUD_MUX1] = {"aud_mux1_clk", false, false, NULL},		/* select from 26 or apll1 */
	[CLOCK_TOP_AUD_MUX2] = {"aud_mux2_clk", false, false, NULL},		/* select from 26 or apll2 */
	[CLOCK_TOP_AD_APLL1_CK] = {"top_ad_apll1_clk", false, false, NULL},	/* parent of TOP_AUD_MUX1 */
	[CLOCK_TOP_AD_APLL2_CK] = {"top_ad_apll2_clk", false, false, NULL},
	[CLOCK_MUX_AUDIO] = {"top_mux_audio", false, false, NULL},
	[CLOCK_TOP_SYSPLL3_D4] = {"top_sys_pll3_d4", false, false, NULL},
	[CLOCK_MUX_AUDIOINTBUS] = {"top_mux_audio_int", false, false, NULL},	/* AudDrv_AUDINTBUS_Sel */
	[CLOCK_TOP_SYSPLL1_D4] = {"top_sys_pll1_d4", false, false, NULL},	/* AudDrv_AUDINTBUS_Sel */
	[CLOCK_TOP_MUX_ANC_MD32] = {"top_mux_anc_md32", false, false, NULL},
	[CLOCK_TOP_SYSPLL1_D2] = {"top_sys_pll1_d2", false, false, NULL},
	[CLOCK_APMIXED_APLL1_CK] = {"apmixed_apll1_clk", false, false, NULL},	/* APLL rate */
	[CLOCK_APMIXED_APLL2_CK] = {"apmixed_apll2_clk", false, false, NULL},	/* APLL rate */
	[CLOCK_CLK26M] = {"top_clk26m_clk", false, false, NULL}
};

int AudDrv_Clk_probe(void *dev)
{
	size_t i;
	int ret = 0;

	Aud_EMI_cntr = 0;

	pr_debug("%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		aud_clks[i].clock = devm_clk_get(dev, aud_clks[i].name);
		if (IS_ERR(aud_clks[i].clock)) {
			ret = PTR_ERR(aud_clks[i].clock);
			pr_err("%s devm_clk_get %s fail %d\n", __func__, aud_clks[i].name, ret);
			break;
		}
		aud_clks[i].clk_status = true;
	}

	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		if (i == CLOCK_SCP_SYS_AUD)	/* CLOCK_SCP_SYS_AUD is MTCMOS */
			continue;

		if (aud_clks[i].clk_status) {
			ret = clk_prepare(aud_clks[i].clock);
			if (ret) {
				pr_err("%s clk_prepare %s fail %d\n",
				       __func__, aud_clks[i].name, ret);
				break;
			}
			aud_clks[i].clk_prepare = true;
		}
	}

	return ret;
}

void AudDrv_Clk_Deinit(void *dev)
{
	size_t i;

	pr_debug("%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		if (i == CLOCK_SCP_SYS_AUD)	/* CLOCK_SCP_SYS_AUD is MTCMOS */
			continue;

		if (aud_clks[i].clock && !IS_ERR(aud_clks[i].clock) && aud_clks[i].clk_prepare) {
			clk_unprepare(aud_clks[i].clock);
			aud_clks[i].clk_prepare = false;
		}
	}
}

void Auddrv_Bus_Init(void)
{
	unsigned long flags = 0;

	pr_debug("%s\n", __func__);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000,
		    0x00004000);    /* must set, system will default set bit14 to 0 */
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
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
	/*volatile uint32 *AFE_Register = (volatile uint32 *)Get_Afe_Powertop_Pointer();*/
	volatile uint32 val_tmp;

	pr_debug("%s", __func__);
	val_tmp = 0x3330000d;
	/*mt_reg_sync_writel(val_tmp, AFE_Register);*/
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

void AudDrv_AUDINTBUS_Sel(int parentidx)
{
	int ret = 0;

	if (parentidx == 1) {
		if (aud_clks[CLOCK_MUX_AUDIOINTBUS].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_MUX_AUDIOINTBUS].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock CLOCK_MUX_AUDIOINTBUS fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_MUX_AUDIOINTBUS fail",
			       __func__);
			BUG();
			goto EXIT;
		}

		ret = clk_set_parent(aud_clks[CLOCK_MUX_AUDIOINTBUS].clock,
				     aud_clks[CLOCK_TOP_SYSPLL1_D4].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_MUX_AUDIOINTBUS].name,
			       aud_clks[CLOCK_TOP_SYSPLL1_D4].name, ret);
			BUG();
			goto EXIT;
		}
	} else if (parentidx == 0) {
		if (aud_clks[CLOCK_MUX_AUDIOINTBUS].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_MUX_AUDIOINTBUS].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock CLOCK_MUX_AUDIOINTBUS fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_MUX_AUDIOINTBUS fail",
			       __func__);
			BUG();
			goto EXIT;
		}

		ret = clk_set_parent(aud_clks[CLOCK_MUX_AUDIOINTBUS].clock,
				     aud_clks[CLOCK_CLK26M].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_MUX_AUDIOINTBUS].name,
			       aud_clks[CLOCK_CLK26M].name, ret);
			BUG();
			goto EXIT;
		}
	}
EXIT:
	pr_debug("-%s()\n", __func__);
}


/*****************************************************************************
 * FUNCTION
 *  AudDrv_AUD_Sel
 *
 * DESCRIPTION
 *  TOP_MUX_AUDIO select source
 *
 *****************************************************************************
*/

void AudDrv_AUD_Sel(int parentidx)
{
	int ret = 0;

	if (parentidx == 1) {
		if (aud_clks[CLOCK_MUX_AUDIO].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_MUX_AUDIO].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock CLOCK_MUX_AUDIO fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_MUX_AUDIO fail",
			       __func__);
			BUG();
			goto EXIT;
		}

		ret = clk_set_parent(aud_clks[CLOCK_MUX_AUDIO].clock,
				     aud_clks[CLOCK_TOP_SYSPLL3_D4].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_MUX_AUDIO].name,
			       aud_clks[CLOCK_TOP_SYSPLL3_D4].name, ret);
			BUG();
			goto EXIT;
		}
	} else if (parentidx == 0) {
		if (aud_clks[CLOCK_MUX_AUDIO].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_MUX_AUDIO].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock CLOCK_MUX_AUDIO fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_MUX_AUDIO fail",
			       __func__);
			BUG();
			goto EXIT;
		}

		ret = clk_set_parent(aud_clks[CLOCK_MUX_AUDIO].clock,
				     aud_clks[CLOCK_CLK26M].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_MUX_AUDIO].name,
			       aud_clks[CLOCK_CLK26M].name, ret);
			BUG();
			goto EXIT;
		}
	}
EXIT:
	pr_debug("-%s()\n", __func__);
}

void AudDrv_Clk_On(void)
{
	unsigned long flags = 0;
	int ret = 0;

	PRINTK_AUD_CLK("+AudDrv_Clk_On, Aud_AFE_Clk_cntr:%d\n", Aud_AFE_Clk_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_AFE_Clk_cntr++;
	if (Aud_AFE_Clk_cntr == 1) {
#ifdef PM_MANAGER_API
		if (aud_clks[CLOCK_INFRA_SYS_AUDIO].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock %s fail\n", __func__,
				       aud_clks[CLOCK_INFRA_SYS_AUDIO].name);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock MT_CG_INFRA_AUDIO fail\n",
			       __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_INFRA_SYS_AUDIO_26M].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO_26M].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock %s fail\n", __func__,
				       aud_clks[CLOCK_INFRA_SYS_AUDIO_26M].name);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_INFRA_SYS_AUDIO_26M fail\n",
			       __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_INFRA_SYS_AUDIO_26M_PAD_TOP].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO_26M_PAD_TOP].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock %s fail\n", __func__,
				       aud_clks[CLOCK_INFRA_SYS_AUDIO_26M_PAD_TOP].name);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_INFRA_SYS_AUDIO_26M_PAD_TOP fail\n",
			       __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_AFE].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_AFE].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock %s fail\n", __func__,
				       aud_clks[CLOCK_AFE].name);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock MT_CG_AUDIO_AFE fail\n",
			       __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_DAC].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_DAC].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock MT_CG_AUDIO_DAC fail\n", __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_status error Aud enable_clock MT_CG_AUDIO_DAC fail\n",
			       __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_DAC_PREDIS].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_DAC_PREDIS].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock MT_CG_AUDIO_DAC_PREDIS fail\n",
				       __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err
			("%s [CCF]clk_status error Aud enable_clock MT_CG_AUDIO_DAC_PREDIS fail\n",
			 __func__);
			BUG();
			goto EXIT;
		}

		spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
		/* CLOCK_SCP_SYS_AUD is MTCMOS */
		if (aud_clks[CLOCK_SCP_SYS_AUD].clk_status) {
			ret = clk_prepare_enable(aud_clks[CLOCK_SCP_SYS_AUD].clock);
			if (ret) {
				pr_err("%s [CCF]Aud clk_prepare_enable %s fail\n", __func__,
					aud_clks[CLOCK_SCP_SYS_AUD].name);
				BUG();
				goto EXIT_SKIP_UNLOCK;
			}
		}

		if (aud_clks[CLOCK_APMIXED_APLL1_CK].clk_prepare) {
			/* set half first, then correct, for CCF not setting reg */
			ret = clk_set_rate(aud_clks[CLOCK_APMIXED_APLL1_CK].clock, 180633600/2);
			if (ret) {
				pr_err("%s clk_set_rate %s-180633600/2 fail %d\n",
				       __func__, aud_clks[CLOCK_APMIXED_APLL1_CK].name, ret);
				BUG();
				goto EXIT;
			}

			ret = clk_set_rate(aud_clks[CLOCK_APMIXED_APLL1_CK].clock, 180633600);
			if (ret) {
				pr_err("%s clk_set_rate %s-180633600 fail %d\n",
				       __func__, aud_clks[CLOCK_APMIXED_APLL1_CK].name, ret);
				BUG();
				goto EXIT;
			}
		}

		if (aud_clks[CLOCK_APMIXED_APLL2_CK].clk_prepare) {
			/* set half first, then correct, for CCF not setting reg */
			ret = clk_set_rate(aud_clks[CLOCK_APMIXED_APLL2_CK].clock, 196608000 / 2);
			if (ret) {
				pr_err("%s clk_set_rate %s-196607998/2 fail %d\n",
				       __func__, aud_clks[CLOCK_APMIXED_APLL2_CK].name, ret);
				BUG();
				goto EXIT;
			}

			ret = clk_set_rate(aud_clks[CLOCK_APMIXED_APLL2_CK].clock, 196608000);
			if (ret) {
				pr_err("%s clk_set_rate %s-196607998 fail %d\n",
				       __func__, aud_clks[CLOCK_APMIXED_APLL2_CK].name, ret);
				BUG();
				goto EXIT;
			}
		}

		ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_MUX1].clock,
				     aud_clks[CLOCK_TOP_AD_APLL1_CK].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_AUD_MUX1].name,
			       aud_clks[CLOCK_TOP_AD_APLL1_CK].name, ret);
			BUG();
			goto EXIT;
		}

		ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_MUX2].clock,
				     aud_clks[CLOCK_TOP_AD_APLL2_CK].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_AUD_MUX2].name,
			       aud_clks[CLOCK_TOP_AD_APLL2_CK].name, ret);
			BUG();
			goto EXIT;
		}

		goto EXIT_SKIP_UNLOCK;
#else
		SetInfraCfg(AUDIO_CG_CLR, 0x2000000, 0x2000000);
		/* bit 25=0, without 133m master and 66m slave bus clock cg gating */
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x4000, 0x06004044);
#endif
	}
EXIT:
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
EXIT_SKIP_UNLOCK:
	PRINTK_AUD_CLK("-AudDrv_Clk_On, Aud_AFE_Clk_cntr:%d\n", Aud_AFE_Clk_cntr);
}
EXPORT_SYMBOL(AudDrv_Clk_On);

void AudDrv_Clk_Off(void)
{
	unsigned long flags = 0;

	PRINTK_AUD_CLK("+!! AudDrv_Clk_Off, Aud_AFE_Clk_cntr:%d\n", Aud_AFE_Clk_cntr);
	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	Aud_AFE_Clk_cntr--;
	if (Aud_AFE_Clk_cntr == 0) {
		/* Disable AFE clock */
#ifdef PM_MANAGER_API
		/* Make sure all IRQ status is cleared */
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 0xffff, 0xffff);

		if (aud_clks[CLOCK_AFE].clk_prepare)
			clk_disable(aud_clks[CLOCK_AFE].clock);

		if (aud_clks[CLOCK_DAC].clk_prepare)
			clk_disable(aud_clks[CLOCK_DAC].clock);

		if (aud_clks[CLOCK_DAC_PREDIS].clk_prepare)
			clk_disable(aud_clks[CLOCK_DAC_PREDIS].clock);

		if (aud_clks[CLOCK_INFRA_SYS_AUDIO_26M_PAD_TOP].clk_prepare)
			clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO_26M_PAD_TOP].clock);

		if (aud_clks[CLOCK_INFRA_SYS_AUDIO_26M].clk_prepare)
			clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO_26M].clock);

		if (aud_clks[CLOCK_INFRA_SYS_AUDIO].clk_prepare)
			clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

		spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
		/* CLOCK_SCP_SYS_AUD is MTCMOS */
		if (aud_clks[CLOCK_SCP_SYS_AUD].clk_status)
			clk_disable_unprepare(aud_clks[CLOCK_SCP_SYS_AUD].clock);
		goto EXIT_SKIP_UNLOCK;
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x06000044, 0x06000044);
		SetInfraCfg(AUDIO_CG_SET, 0x2000000, 0x2000000);
		/* bit25=1, with 133m mastesr and 66m slave bus clock cg gating */
#endif
	} else if (Aud_AFE_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_Clk_Off, Aud_AFE_Clk_cntr<0 (%d)\n",
				 Aud_AFE_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_AFE_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
EXIT_SKIP_UNLOCK:
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
	if (Aud_ANA_Clk_cntr == 0)
		PRINTK_AUD_CLK("+AudDrv_ANA_Clk_On, Aud_ANA_Clk_cntr:%d\n", Aud_ANA_Clk_cntr);

	Aud_ANA_Clk_cntr++;
	mutex_unlock(&auddrv_pmic_mutex);
	/* PRINTK_AUD_CLK("-AudDrv_ANA_Clk_Off, Aud_ANA_Clk_cntr:%d\n",Aud_ANA_Clk_cntr); */
}
EXPORT_SYMBOL(AudDrv_ANA_Clk_On);

void AudDrv_ANA_Clk_Off(void)
{
	/* PRINTK_AUD_CLK("+AudDrv_ANA_Clk_Off, Aud_ADC_Clk_cntr:%d\n",  Aud_ANA_Clk_cntr); */
	mutex_lock(&auddrv_pmic_mutex);
	Aud_ANA_Clk_cntr--;
	if (Aud_ANA_Clk_cntr == 0) {
		PRINTK_AUD_CLK("+AudDrv_ANA_Clk_Off disable_clock Ana clk(%x)\n",
			       Aud_ANA_Clk_cntr);
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
	/* PRINTK_AUD_CLK("-AudDrv_ANA_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ANA_Clk_cntr); */
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
	/* PRINTK_AUDDRV("+AudDrv_ADC_Clk_On, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr); */
	int ret = 0;

	mutex_lock(&auddrv_pmic_mutex);

	if (Aud_ADC_Clk_cntr == 0) {
		PRINTK_AUDDRV("+AudDrv_ADC_Clk_On enable_clock ADC clk(%x)\n",
			      Aud_ADC_Clk_cntr);
		/* Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 24 , 1 << 24); */
#ifdef PM_MANAGER_API
		if (aud_clks[CLOCK_ADC].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_ADC].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock enable_clock ADC fail", __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock ADC fail", __func__);
			BUG();
			goto EXIT;
		}
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 24, 1 << 24);
#endif
	}
	Aud_ADC_Clk_cntr++;
EXIT:
	mutex_unlock(&auddrv_pmic_mutex);
}

void AudDrv_ADC_Clk_Off(void)
{
	/* PRINTK_AUDDRV("+AudDrv_ADC_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr); */
	mutex_lock(&auddrv_pmic_mutex);
	Aud_ADC_Clk_cntr--;
	if (Aud_ADC_Clk_cntr == 0) {
		PRINTK_AUDDRV("+AudDrv_ADC_Clk_On disable_clock ADC clk(%x)\n",
			      Aud_ADC_Clk_cntr);
#ifdef PM_MANAGER_API
		if (aud_clks[CLOCK_ADC].clk_prepare)
			clk_disable(aud_clks[CLOCK_ADC].clock);
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 24, 1 << 24);
#endif
	}
	if (Aud_ADC_Clk_cntr < 0) {
		PRINTK_AUDDRV("!! AudDrv_ADC_Clk_Off, Aud_ADC_Clk_cntr<0 (%d)\n",
			      Aud_ADC_Clk_cntr);
		Aud_ADC_Clk_cntr = 0;
	}
	mutex_unlock(&auddrv_pmic_mutex);
	/* PRINTK_AUDDRV("-AudDrv_ADC_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr); */
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
	PRINTK_AUD_CLK("+%s %d\n", __func__, Aud_ADC2_Clk_cntr);
	mutex_lock(&auddrv_pmic_mutex);

	if (Aud_ADC2_Clk_cntr == 0)
		PRINTK_AUDDRV("+%s  enable_clock ADC2 clk(%x)\n", __func__, Aud_ADC2_Clk_cntr);

	Aud_ADC2_Clk_cntr++;

	mutex_unlock(&auddrv_pmic_mutex);
}

void AudDrv_ADC2_Clk_Off(void)
{
	/* PRINTK_AUDDRV("+%s %d\n", __func__,Aud_ADC2_Clk_cntr); */
	mutex_lock(&auddrv_pmic_mutex);
	Aud_ADC2_Clk_cntr--;
	if (Aud_ADC2_Clk_cntr == 0)
		PRINTK_AUDDRV("+%s disable_clock ADC clk(%x)\n", __func__, Aud_ADC2_Clk_cntr);


	if (Aud_ADC2_Clk_cntr < 0) {
		PRINTK_AUDDRV("%s  <0 (%d)\n", __func__, Aud_ADC2_Clk_cntr);
		Aud_ADC2_Clk_cntr = 0;
	}
	mutex_unlock(&auddrv_pmic_mutex);
	/* PRINTK_AUDDRV("-AudDrv_ADC_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr); */
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

	if (Aud_ADC3_Clk_cntr == 0)
		PRINTK_AUDDRV("+%s  enable_clock ADC clk(%x)\n", __func__, Aud_ADC3_Clk_cntr);

	Aud_ADC3_Clk_cntr++;
	mutex_unlock(&auddrv_pmic_mutex);
}

void AudDrv_ADC3_Clk_Off(void)
{
	/* PRINTK_AUDDRV("+%s %d\n", __func__,Aud_ADC2_Clk_cntr); */
	mutex_lock(&auddrv_pmic_mutex);
	Aud_ADC3_Clk_cntr--;

	if (Aud_ADC3_Clk_cntr == 0)
		PRINTK_AUDDRV("+%s disable_clock ADC clk(%x)\n", __func__, Aud_ADC3_Clk_cntr);


	if (Aud_ADC3_Clk_cntr < 0) {
		PRINTK_AUDDRV("%s  <0 (%d)\n", __func__, Aud_ADC3_Clk_cntr);
		Aud_ADC3_Clk_cntr = 0;
	}

	mutex_unlock(&auddrv_pmic_mutex);
	/* PRINTK_AUDDRV("-AudDrv_ADC_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr); */
}

/*****************************************************************************
 * FUNCTION
  *  AudDrv_ADC_Hires_Clk_On / AudDrv_ADC_Hires_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable analog part clock
  *
  *****************************************************************************/

void AudDrv_ADC_Hires_Clk_On(void)
{
	unsigned long flags = 0;
	int ret = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	if (Aud_ADC_HIRES_Clk_cntr == 0) {
		PRINTK_AUDDRV("+AudDrv_ADC_Hires_Clk_On enable_clock ADC clk(%x)\n",
			      Aud_ADC_HIRES_Clk_cntr);
#ifdef PM_MANAGER_API
		if (aud_clks[CLOCK_ADC_HIRES].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_ADC_HIRES].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock ADC_HIRES fail", __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error ADC_HIRES fail", __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_ADC_HIRES_TML].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_ADC_HIRES_TML].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock ADC_HIRES_TML fail", __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error ADC_HIRES_TML fail", __func__);
			BUG();
			goto EXIT;
		}
#else
		Afe_Set_Reg(AUDIO_TOP_CON1, 0 << 16, 1 << 16);
		Afe_Set_Reg(AUDIO_TOP_CON1, 0 << 17, 1 << 17);
#endif
	}
	Aud_ADC_HIRES_Clk_cntr++;
EXIT:
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_ADC_Hires_Clk_Off(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_ADC_HIRES_Clk_cntr--;
	if (Aud_ADC_HIRES_Clk_cntr == 0) {
		PRINTK_AUDDRV("+AudDrv_ADC_Hires_Clk_Off disable_clock ADC_HIRES clk(%x)\n",
			      Aud_ADC_HIRES_Clk_cntr);
		/* Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 24 , 1 << 24); */
#ifdef PM_MANAGER_API
		if (aud_clks[CLOCK_ADC_HIRES_TML].clk_prepare)
			clk_disable(aud_clks[CLOCK_ADC_HIRES_TML].clock);

		if (aud_clks[CLOCK_ADC_HIRES].clk_prepare)
			clk_disable(aud_clks[CLOCK_ADC_HIRES].clock);
#else
		Afe_Set_Reg(AUDIO_TOP_CON1, 1 << 17, 1 << 17);
		Afe_Set_Reg(AUDIO_TOP_CON1, 1 << 16, 1 << 16);
#endif
	}
	if (Aud_ADC_HIRES_Clk_cntr < 0) {
		PRINTK_AUDDRV("!! AudDrv_ADC_Hires_Clk_Off, Aud_ADC_Clk_cntr<0 (%d)\n",
			      Aud_ADC_HIRES_Clk_cntr);
		Aud_ADC_HIRES_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
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
	int ret = 0;
	unsigned long flags = 0;

	PRINTK_AUD_CLK("+%s counter = %d\n", __func__, Aud_APLL22M_Clk_cntr);

	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	if (Aud_APLL22M_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		/* pdn_aud_1 => power down hf_faud_1_ck, hf_faud_1_ck is mux of 26M and APLL1_CK */
		/* pdn_aud_2 => power down hf_faud_2_ck, hf_faud_2_ck is mux of 26M and APLL2_CK (D1 is WHPLL) */

		if (aud_clks[CLOCK_TOP_AD_APLL1_CK].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_TOP_AD_APLL1_CK].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock CLOCK_TOP_AD_APLL1_CK fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud CLOCK_TOP_AD_APLL1_CK fail",
			       __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_TOP_AUD_MUX1].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_TOP_AUD_MUX1].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock CLOCK_TOP_AUD_MUX1 fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_TOP_AUD_MUX1 fail",
			       __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_APLL22M].clk_prepare) {

			ret = clk_enable(aud_clks[CLOCK_APLL22M].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock enable_clock aud_apll22m_clk fail",
				       __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock aud_apll22m_clk fail",
			       __func__);
			BUG();
			goto EXIT;
		}

/*		if (aud_clks[CLOCK_APLL1_TUNER].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_APLL1_TUNER].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock aud_apll1_tuner_clk fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock aud_apll1_tuner_clk fail",
			       __func__);
			BUG();
			goto EXIT;
		}*/


#endif
	}
	Aud_APLL22M_Clk_cntr++;
EXIT:
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_APLL22M_Clk_Off(void)
{
	unsigned long flags = 0;

	PRINTK_AUD_CLK("+%s counter = %d\n", __func__, Aud_APLL22M_Clk_cntr);

	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	Aud_APLL22M_Clk_cntr--;

	if (Aud_APLL22M_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		if (aud_clks[CLOCK_APLL22M].clk_prepare)
			clk_disable(aud_clks[CLOCK_APLL22M].clock);

/*		if (aud_clks[CLOCK_APLL1_TUNER].clk_prepare)
			clk_disable(aud_clks[CLOCK_APLL1_TUNER].clock);*/

/*		ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_MUX1].clock,
				     aud_clks[CLOCK_CLK26M].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_AUD_MUX1].name,
			       aud_clks[CLOCK_CLK26M].name, ret);
			BUG();
			goto EXIT;
		}*/

		if (aud_clks[CLOCK_TOP_AUD_MUX1].clk_prepare) {
			clk_disable(aud_clks[CLOCK_TOP_AUD_MUX1].clock);
			pr_debug("%s [CCF]Aud clk_disable CLOCK_TOP_AUD_MUX1",
				 __func__);

		} else {
			pr_err
			("%s [CCF]clk_prepare error clk_disable CLOCK_TOP_AUD_MUX1 fail",
			 __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_TOP_AD_APLL1_CK].clk_prepare) {
			clk_disable(aud_clks[CLOCK_TOP_AD_APLL1_CK].clock);
			pr_debug("%s [CCF]Aud clk_disable CLOCK_TOP_AD_APLL1_CK",
				 __func__);

		} else {
			pr_err
			("%s [CCF]clk_prepare error CLOCK_TOP_AD_APLL1_CK fail",
			 __func__);
			BUG();
			goto EXIT;
		}

#endif
	}

EXIT:
	if (Aud_APLL22M_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("err, %s <0 (%d)\n", __func__,
				 Aud_APLL22M_Clk_cntr);
		Aud_APLL22M_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
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
	int ret = 0;
	unsigned long flags = 0;

	PRINTK_AUD_CLK("+%s counter = %d\n", __func__, Aud_APLL24M_Clk_cntr);

	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	if (Aud_APLL24M_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		if (aud_clks[CLOCK_TOP_AD_APLL2_CK].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_TOP_AD_APLL2_CK].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock CLOCK_TOP_AD_APLL2_CK fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud CLOCK_TOP_AD_APLL2_CK fail",
			       __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_TOP_AUD_MUX2].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_TOP_AUD_MUX2].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock CLOCK_TOP_AUD_MUX2 fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_TOP_AUD_MUX2 fail",
			       __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_APLL24M].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_APLL24M].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock enable_clock aud_apll24m_clk fail",
				       __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock aud_apll24m_clk fail",
			       __func__);
			BUG();
			goto EXIT;
		}

/*		if (aud_clks[CLOCK_APLL2_TUNER].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_APLL2_TUNER].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock aud_apll2_tuner_clk fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock aud_apll2_tuner_clk fail",
			       __func__);
			BUG();
			goto EXIT;
		}*/
#endif
	}
	Aud_APLL24M_Clk_cntr++;
EXIT:
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_APLL24M_Clk_Off(void)
{
	unsigned long flags = 0;

	PRINTK_AUD_CLK("+%s counter = %d\n", __func__, Aud_APLL24M_Clk_cntr);

	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	Aud_APLL24M_Clk_cntr--;

	if (Aud_APLL24M_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		if (aud_clks[CLOCK_APLL24M].clk_prepare)
			clk_disable(aud_clks[CLOCK_APLL24M].clock);

/*		if (aud_clks[CLOCK_APLL2_TUNER].clk_prepare)
			clk_disable(aud_clks[CLOCK_APLL2_TUNER].clock);*/

/*		ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_MUX2].clock,
				     aud_clks[CLOCK_CLK26M].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_AUD_MUX2].name,
			       aud_clks[CLOCK_CLK26M].name, ret);
			BUG();
			goto EXIT;
		}*/

		if (aud_clks[CLOCK_TOP_AUD_MUX2].clk_prepare) {
			clk_disable(aud_clks[CLOCK_TOP_AUD_MUX2].clock);

			pr_err("%s [CCF]Aud clk_disable CLOCK_TOP_AUD_MUX2 fail",
			       __func__);

		} else {
			pr_err
			("%s [CCF]clk_prepare error clk_disable CLOCK_TOP_AUD_MUX2 fail",
			 __func__);
			BUG();
			goto EXIT;
		}

		if (aud_clks[CLOCK_TOP_AD_APLL2_CK].clk_prepare) {
			clk_disable(aud_clks[CLOCK_TOP_AD_APLL2_CK].clock);
			pr_debug("%s [CCF]Aud clk_disable CLOCK_TOP_AD_APLL2_CK fail",
				 __func__);

		} else {
			pr_err
			("%s [CCF]clk_prepare error CLOCK_TOP_AD_APLL2_CK fail",
			 __func__);
			BUG();
			goto EXIT;
		}

#endif
	}
EXIT:
	if (Aud_APLL24M_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("%s <0 (%d)\n", __func__,
				 Aud_APLL24M_Clk_cntr);
		Aud_APLL24M_Clk_cntr = 0;
	}

	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

/*****************************************************************************
  * FUNCTION
  *  AudDrv_I2S_Clk_On / AudDrv_I2S_Clk_Off
  *
  * DESCRIPTION
  * Enable I2S In clock (bck)
  * This should be enabled in slave i2s mode.
  *
  *****************************************************************************/
void aud_top_con_pdn_i2s(bool _pdn)
{
	if (_pdn)
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x1 << 6, 0x1 << 6); /* power off I2S clock */
	else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x0 << 6, 0x1 << 6); /* power on I2S clock */
}

void AudDrv_I2S_Clk_On(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	if (Aud_I2S_Clk_cntr == 0)
		aud_top_con_pdn_i2s(false);

	Aud_I2S_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}
EXPORT_SYMBOL(AudDrv_I2S_Clk_On);

void AudDrv_I2S_Clk_Off(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_I2S_Clk_cntr--;
	if (Aud_I2S_Clk_cntr == 0) {
		aud_top_con_pdn_i2s(true);
	} else if (Aud_I2S_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_I2S_Clk_Off, Aud_I2S_Clk_cntr<0 (%d)\n",
				 Aud_I2S_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_I2S_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}
EXPORT_SYMBOL(AudDrv_I2S_Clk_Off);

/*****************************************************************************
  * FUNCTION
  *  AudDrv_TDM_Clk_On / AudDrv_TDM_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable TDM clock
  *
  *****************************************************************************/
void aud_top_con_pdn_tdm_ck(bool _pdn)
{
	if (_pdn)
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x1 << 20, 0x1 << 20); /* power off I2S clock */
	else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x0 << 20, 0x1 << 20); /* power on I2S clock */
}

void AudDrv_TDM_Clk_On(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_TDM_Clk_cntr == 0)
		aud_top_con_pdn_tdm_ck(false); /* enable HDMI CK */

	Aud_TDM_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}
EXPORT_SYMBOL(AudDrv_TDM_Clk_On);

void AudDrv_TDM_Clk_Off(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_TDM_Clk_cntr--;
	if (Aud_TDM_Clk_cntr == 0) {
		aud_top_con_pdn_tdm_ck(true); /* disable HDMI CK */
	} else if (Aud_TDM_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! %s(), Aud_TDM_Clk_cntr<0 (%d)\n",
				 __func__, Aud_TDM_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_TDM_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}
EXPORT_SYMBOL(AudDrv_TDM_Clk_Off);


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
	/* PRINTK_AUD_CLK("+AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr); */
	unsigned long flags = 0;
	int ret = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_Core_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		if (aud_clks[CLOCK_AFE].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_AFE].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock enable_clock aud_afe_clk fail",
				       __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock aud_afe_clk fail",
			       __func__);
			BUG();
			goto EXIT;
		}
#endif
	}
	Aud_Core_Clk_cntr++;
EXIT:
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	/* PRINTK_AUD_CLK("-AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr); */
}

void AudDrv_Core_Clk_Off(void)
{
	/* PRINTK_AUD_CLK("+AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr); */
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_Core_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		if (aud_clks[CLOCK_AFE].clk_prepare)
			clk_disable(aud_clks[CLOCK_AFE].clock);
#endif
	}
	Aud_Core_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	/* PRINTK_AUD_CLK("-AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr); */
}

void AudDrv_APLL1Tuner_Clk_On(void)
{
	unsigned long flags = 0;
#ifndef CONFIG_MTK_CLKMGR
	int ret = 0;
#endif
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_APLL1_Tuner_cntr == 0) {
		PRINTK_AUD_CLK("+AudDrv_APLLTuner_Clk_On, Aud_APLL1_Tuner_cntr:%d\n",
			       Aud_APLL1_Tuner_cntr);
#ifdef CONFIG_MTK_CLKMGR
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x0 << 19, 0x1 << 19);
		SetApmixedCfg(AP_PLL_CON5, 0x1, 0x1);
#else
		if (aud_clks[CLOCK_APLL1_TUNER].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_APLL1_TUNER].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock aud_apll1_tuner_clk fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock aud_apll1_tuner_clk fail",
			       __func__);
			BUG();
			goto EXIT;
		}
		SetApmixedCfg(AP_PLL_CON5, 0x1, 0x1);
#endif
	}
	Aud_APLL1_Tuner_cntr++;
EXIT:
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_APLL1Tuner_Clk_Off(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	Aud_APLL1_Tuner_cntr--;
	if (Aud_APLL1_Tuner_cntr == 0) {
#ifdef CONFIG_MTK_CLKMGR
		SetApmixedCfg(AP_PLL_CON5, 0x0, 0x1);
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x1 << 19, 0x1 << 19);
		/*Afe_Set_Reg(AFE_APLL1_TUNER_CFG, 0x00000033, 0x1 << 19);*/
#else
		SetApmixedCfg(AP_PLL_CON5, 0x0, 0x1);
		if (aud_clks[CLOCK_APLL1_TUNER].clk_prepare)
			clk_disable(aud_clks[CLOCK_APLL1_TUNER].clock);
#endif
	}
	/* handle for clock error */
	else if (Aud_APLL1_Tuner_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_APLLTuner_Clk_Off, Aud_APLL1_Tuner_cntr<0 (%d)\n",
				 Aud_APLL1_Tuner_cntr);
		Aud_APLL1_Tuner_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}


void AudDrv_APLL2Tuner_Clk_On(void)
{
	unsigned long flags = 0;
#ifndef CONFIG_MTK_CLKMGR
	int ret = 0;
#endif
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_APLL2_Tuner_cntr == 0) {
		PRINTK_AUD_CLK("+Aud_APLL2_Tuner_cntr, Aud_APLL2_Tuner_cntr:%d\n",
			       Aud_APLL2_Tuner_cntr);
#ifdef CONFIG_MTK_CLKMGR
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x0 << 18, 0x1 << 18);
		SetApmixedCfg(AP_PLL_CON5, 0x1 << 1, 0x1 << 1);
#else
		if (aud_clks[CLOCK_APLL2_TUNER].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_APLL2_TUNER].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock aud_apll2_tuner_clk fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock aud_apll2_tuner_clk fail",
			       __func__);
			BUG();
			goto EXIT;
		}
		SetApmixedCfg(AP_PLL_CON5, 0x1 << 1, 0x1 << 1);
#endif
	}
	Aud_APLL2_Tuner_cntr++;
EXIT:
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_APLL2Tuner_Clk_Off(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	Aud_APLL2_Tuner_cntr--;

	if (Aud_APLL2_Tuner_cntr == 0) {
#ifdef CONFIG_MTK_CLKMGR
		SetApmixedCfg(AP_PLL_CON5, 0x0 << 1, 0x1 << 1);
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x1 << 18, 0x1 << 18);
#else
		SetApmixedCfg(AP_PLL_CON5, 0x0 << 1, 0x1 << 1);
		if (aud_clks[CLOCK_APLL2_TUNER].clk_prepare)
			clk_disable(aud_clks[CLOCK_APLL2_TUNER].clock);
#endif
		PRINTK_AUD_CLK("AudDrv_APLL2Tuner_Clk_Off\n");
	}
	/* handle for clock error */
	else if (Aud_APLL2_Tuner_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_APLL2Tuner_Clk_Off, Aud_APLL1_Tuner_cntr<0 (%d)\n",
				 Aud_APLL2_Tuner_cntr);
		Aud_APLL2_Tuner_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
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
	PRINTK_AUD_CLK("+AudDrv_HDMI_Clk_On, Aud_I2S_Clk_cntr:%d\n", Aud_HDMI_Clk_cntr);
	if (Aud_HDMI_Clk_cntr == 0) {
		AudDrv_ANA_Clk_On();
		AudDrv_Clk_On();
	}
	Aud_HDMI_Clk_cntr++;
}

void AudDrv_HDMI_Clk_Off(void)
{
	PRINTK_AUD_CLK("+AudDrv_HDMI_Clk_Off, Aud_I2S_Clk_cntr:%d\n",
		       Aud_HDMI_Clk_cntr);
	Aud_HDMI_Clk_cntr--;
	if (Aud_HDMI_Clk_cntr == 0) {
		AudDrv_ANA_Clk_Off();
		AudDrv_Clk_Off();
	} else if (Aud_HDMI_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_Linein_Clk_Off, Aud_I2S_Clk_cntr<0 (%d)\n",
				 Aud_HDMI_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_HDMI_Clk_cntr = 0;
	}
	PRINTK_AUD_CLK("-AudDrv_I2S_Clk_Off, Aud_I2S_Clk_cntr:%d\n", Aud_HDMI_Clk_cntr);
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
	unsigned long flags = 0;
	int ret = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_Core_Clk_cntr > 0) {
		if (Aud_I2S_Clk_cntr > 0)
			aud_top_con_pdn_i2s(true);

		if (Aud_TDM_Clk_cntr > 0)
			aud_top_con_pdn_tdm_ck(true);

		if (Aud_ADC_Clk_cntr > 0) {
			if (aud_clks[CLOCK_ADC].clk_prepare)
				clk_disable(aud_clks[CLOCK_ADC].clock);
		}

		if (Aud_APLL22M_Clk_cntr > 0) {
			if (aud_clks[CLOCK_APLL22M].clk_prepare)
				clk_disable(aud_clks[CLOCK_APLL22M].clock);

			if (aud_clks[CLOCK_APLL1_TUNER].clk_prepare)
				clk_disable(aud_clks[CLOCK_APLL1_TUNER].clock);

			ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_MUX1].clock,
					     aud_clks[CLOCK_CLK26M].clock);
			if (ret) {
				pr_err("%s clk_set_parent %s-%s fail %d\n",
				       __func__, aud_clks[CLOCK_TOP_AUD_MUX1].name,
				       aud_clks[CLOCK_CLK26M].name, ret);
				BUG();
				goto EXIT;
			}

			if (aud_clks[CLOCK_TOP_AUD_MUX1].clk_prepare) {
				clk_disable(aud_clks[CLOCK_TOP_AUD_MUX1].clock);
				pr_debug("%s [CCF]Aud clk_disable CLOCK_TOP_AUD_MUX1 fail",
					 __func__);

			} else {
				pr_err
				("%s [CCF]clk_prepare error clk_disable CLOCK_TOP_AUD_MUX1 fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		}
		if (Aud_APLL24M_Clk_cntr > 0) {
			if (aud_clks[CLOCK_APLL24M].clk_prepare)
				clk_disable(aud_clks[CLOCK_APLL24M].clock);

			if (aud_clks[CLOCK_APLL2_TUNER].clk_prepare)
				clk_disable(aud_clks[CLOCK_APLL2_TUNER].clock);

			ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_MUX2].clock,
					     aud_clks[CLOCK_CLK26M].clock);
			if (ret) {
				pr_err("%s clk_set_parent %s-%s fail %d\n",
				       __func__, aud_clks[CLOCK_TOP_AUD_MUX2].name,
				       aud_clks[CLOCK_CLK26M].name, ret);
				BUG();
				goto EXIT;
			}

			if (aud_clks[CLOCK_TOP_AUD_MUX2].clk_prepare) {
				clk_disable(aud_clks[CLOCK_TOP_AUD_MUX2].clock);
				pr_debug("%s [CCF]Aud clk_disable CLOCK_TOP_AUD_MUX2 fail",
					 __func__);

			} else {
				pr_err
				("%s [CCF]clk_prepare error clk_disable CLOCK_TOP_AUD_MUX2 fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		}

		if (Aud_AFE_Clk_cntr > 0) {
			if (aud_clks[CLOCK_AFE].clk_prepare)
				clk_disable(aud_clks[CLOCK_AFE].clock);
		}
	}
EXIT:
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_Suspend_Clk_On(void)
{
	unsigned long flags = 0;
	int ret = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_Core_Clk_cntr > 0) {
		if (aud_clks[CLOCK_AFE].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_AFE].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock enable_clock aud_afe_clk fail",
				       __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock aud_afe_clk fail",
			       __func__);
			BUG();
			goto EXIT;
		}

		if (Aud_APLL22M_Clk_cntr > 0) {
			if (aud_clks[CLOCK_TOP_AUD_MUX1].clk_prepare) {
				ret = clk_enable(aud_clks[CLOCK_TOP_AUD_MUX1].clock);
				if (ret) {
					pr_err
					("%s [CCF]Aud enable_clock enable_clock CLOCK_TOP_AUD_MUX1 fail",
					 __func__);
					BUG();
					goto EXIT;
				}
			} else {
				pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_TOP_AUD_MUX1 fail",
				       __func__);
				BUG();
				goto EXIT;
			}

			ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_MUX1].clock,
					     aud_clks[CLOCK_TOP_AD_APLL1_CK].clock);
			if (ret) {
				pr_err("%s clk_set_parent %s-%s fail %d\n",
				       __func__, aud_clks[CLOCK_TOP_AUD_MUX1].name,
				       aud_clks[CLOCK_TOP_AD_APLL1_CK].name, ret);
				BUG();
				goto EXIT;
			}
/*
			if (aud_clks[CLOCK_APMIXED_APLL1_CK].clk_prepare) {

				ret = clk_set_rate(aud_clks[CLOCK_APMIXED_APLL1_CK].clock, 180633600);
				if (ret) {
					pr_err("%s clk_set_rate %s-180633600 fail %d\n",
					       __func__, aud_clks[CLOCK_APMIXED_APLL1_CK].name, ret);
					BUG();
					goto EXIT;
				}
			}
*/

			if (aud_clks[CLOCK_APLL22M].clk_prepare) {
				ret = clk_enable(aud_clks[CLOCK_APLL22M].clock);
				if (ret) {
					pr_err
					("%s [CCF]Aud enable_clock enable_clock aud_apll22m_clk fail",
					 __func__);
					BUG();
					goto EXIT;
				}
			} else {
				pr_err
				("%s [CCF]clk_prepare error Aud enable_clock aud_apll22m_clk fail",
				 __func__);
				BUG();
				goto EXIT;
			}

			if (aud_clks[CLOCK_APLL1_TUNER].clk_prepare) {
				ret = clk_enable(aud_clks[CLOCK_APLL1_TUNER].clock);
				if (ret) {
					pr_err
					("%s [CCF]Aud enable_clock enable_clock aud_apll1_tuner_clk fail",
					 __func__);
					BUG();
					goto EXIT;
				}
			} else {
				pr_err
				("%s [CCF]clk_prepare error Aud enable_clock aud_apll1_tuner_clk fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		}

		if (Aud_APLL24M_Clk_cntr > 0) {
			if (aud_clks[CLOCK_TOP_AUD_MUX2].clk_prepare) {
				ret = clk_enable(aud_clks[CLOCK_TOP_AUD_MUX2].clock);
				if (ret) {
					pr_err
					("%s [CCF]Aud enable_clock enable_clock CLOCK_TOP_AUD_MUX2 fail",
					 __func__);
					BUG();
					goto EXIT;
				}
			} else {
				pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_TOP_AUD_MUX2 fail",
				       __func__);
				BUG();
				goto EXIT;
			}

			ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_MUX2].clock,
					     aud_clks[CLOCK_TOP_AD_APLL2_CK].clock);
			if (ret) {
				pr_err("%s clk_set_parent %s-%s fail %d\n",
				       __func__, aud_clks[CLOCK_TOP_AUD_MUX2].name,
				       aud_clks[CLOCK_TOP_AD_APLL2_CK].name, ret);
				BUG();
				goto EXIT;
			}
/*
			if (aud_clks[CLOCK_APMIXED_APLL2_CK].clk_prepare) {

				ret = clk_set_rate(aud_clks[CLOCK_APMIXED_APLL2_CK].clock, 196607998);
				if (ret) {
					pr_err("%s clk_set_rate %s-196607998 fail %d\n",
					       __func__, aud_clks[CLOCK_APMIXED_APLL2_CK].name, ret);
					BUG();
					goto EXIT;
				}
			}
*/
			if (aud_clks[CLOCK_APLL24M].clk_prepare) {
				ret = clk_enable(aud_clks[CLOCK_APLL24M].clock);
				if (ret) {
					pr_err
					("%s [CCF]Aud enable_clock enable_clock aud_apll24m_clk fail",
					 __func__);
					BUG();
					goto EXIT;
				}
			} else {
				pr_err
				("%s [CCF]clk_prepare error Aud enable_clock aud_apll24m_clk fail",
				 __func__);
				BUG();
				goto EXIT;
			}


			if (aud_clks[CLOCK_APLL2_TUNER].clk_prepare) {
				ret = clk_enable(aud_clks[CLOCK_APLL2_TUNER].clock);
				if (ret) {
					pr_err
					("%s [CCF]Aud enable_clock enable_clock aud_apll2_tuner_clk fail",
					 __func__);
					BUG();
					goto EXIT;
				}
			} else {
				pr_err
				("%s [CCF]clk_prepare error Aud enable_clock aud_apll2_tuner_clk fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		}

		if (Aud_I2S_Clk_cntr > 0)
			aud_top_con_pdn_i2s(false);

		if (Aud_TDM_Clk_cntr > 0)
			aud_top_con_pdn_tdm_ck(false);

		if (Aud_ADC_Clk_cntr > 0) {
			if (aud_clks[CLOCK_ADC].clk_prepare) {
				ret = clk_enable(aud_clks[CLOCK_ADC].clock);
				if (ret) {
					pr_err("%s [CCF]Aud enable_clock enable_clock ADC fail", __func__);
					BUG();
					goto EXIT;
				}
			} else {
				pr_err("%s [CCF]clk_prepare error Aud enable_clock ADC fail", __func__);
				BUG();
				goto EXIT;
			}
		}
	}
EXIT:
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_Emi_Clk_On(void)
{
	mutex_lock(&auddrv_pmic_mutex);
	if (Aud_EMI_cntr == 0) {
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef _MT_IDLE_HEADER
		/* mutex is used in these api */
		disable_dpidle_by_bit(MT_CG_ID_AUDIO_AFE);
		disable_soidle_by_bit(MT_CG_ID_AUDIO_AFE);
#endif
#endif
	}
	Aud_EMI_cntr++;
	mutex_unlock(&auddrv_pmic_mutex);
}

void AudDrv_Emi_Clk_Off(void)
{
	mutex_lock(&auddrv_pmic_mutex);
	Aud_EMI_cntr--;
	if (Aud_EMI_cntr == 0) {
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef _MT_IDLE_HEADER
		/* mutex is used in these api */
		enable_dpidle_by_bit(MT_CG_ID_AUDIO_AFE);
		enable_soidle_by_bit(MT_CG_ID_AUDIO_AFE);
#endif
#endif
	}

	if (Aud_EMI_cntr < 0) {
		Aud_EMI_cntr = 0;
		PRINTK_AUD_ERROR("Aud_EMI_cntr = %d\n", Aud_EMI_cntr);
	}
	mutex_unlock(&auddrv_pmic_mutex);
}

/*****************************************************************************
 * FUNCTION
  *  AudDrv_ANC_Clk_On / AudDrv_ANC_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable ANC clock
  *
  *****************************************************************************/

void AudDrv_ANC_Clk_On(void)
{
	int ret = 0;

	mutex_lock(&auddrv_pmic_mutex);

	if (Aud_ANC_Clk_cntr == 0) {
		PRINTK_AUDDRV("+%s() Aud_ANC_Clk_cntr(%x)\n",
			      __func__,
			      Aud_ANC_Clk_cntr);
		if (aud_clks[CLOCK_INFRA_ANC_MD32].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_INFRA_ANC_MD32].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock enable_clock %s fail",
					__func__,
					aud_clks[CLOCK_INFRA_ANC_MD32].name);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error enable_clock %s fail",
				__func__,
				aud_clks[CLOCK_INFRA_ANC_MD32].name);
			BUG();
			goto EXIT;
		}
		if (aud_clks[CLOCK_INFRA_ANC_MD32_32K].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_INFRA_ANC_MD32_32K].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock enable_clock %s fail",
					__func__,
					aud_clks[CLOCK_INFRA_ANC_MD32_32K].name);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error enable_clock %s fail",
				__func__,
				aud_clks[CLOCK_INFRA_ANC_MD32_32K].name);
			BUG();
			goto EXIT;
		}

		/* ANC_MD32 TOP CLOCK MUX SELECT SYSPLL1_D2*/
		if (aud_clks[CLOCK_TOP_MUX_ANC_MD32].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_TOP_MUX_ANC_MD32].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock CLOCK_TOP_MUX_ANC_MD32 fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_TOP_MUX_ANC_MD32 fail",
			       __func__);
			BUG();
			goto EXIT;
		}

		ret = clk_set_parent(aud_clks[CLOCK_TOP_MUX_ANC_MD32].clock,
				     aud_clks[CLOCK_TOP_SYSPLL1_D2].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_MUX_ANC_MD32].name,
			       aud_clks[CLOCK_TOP_SYSPLL1_D2].name, ret);
			BUG();
			goto EXIT;
		}
	}
	Aud_ANC_Clk_cntr++;
EXIT:
	mutex_unlock(&auddrv_pmic_mutex);
}

void AudDrv_ANC_Clk_Off(void)
{
	int ret = 0;

	mutex_lock(&auddrv_pmic_mutex);
	Aud_ANC_Clk_cntr--;
	if (Aud_ANC_Clk_cntr == 0) {
		PRINTK_AUDDRV("+%s(), Aud_ANC_Clk_cntr(%x)\n",
			      __func__,
			      Aud_ANC_Clk_cntr);
		if (aud_clks[CLOCK_INFRA_ANC_MD32_32K].clk_prepare)
			clk_disable(aud_clks[CLOCK_INFRA_ANC_MD32_32K].clock);
		if (aud_clks[CLOCK_INFRA_ANC_MD32].clk_prepare)
			clk_disable(aud_clks[CLOCK_INFRA_ANC_MD32].clock);

		/* ANC_MD32 TOP CLOCK MUX SELECT 26M*/
		if (aud_clks[CLOCK_TOP_MUX_ANC_MD32].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_TOP_MUX_ANC_MD32].clock);
			if (ret) {
				pr_err
				("%s [CCF]Aud enable_clock enable_clock CLOCK_TOP_MUX_ANC_MD32 fail",
				 __func__);
				BUG();
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error Aud enable_clock CLOCK_TOP_MUX_ANC_MD32 fail",
			       __func__);
			BUG();
			goto EXIT;
		}

		ret = clk_set_parent(aud_clks[CLOCK_TOP_MUX_ANC_MD32].clock,
				     aud_clks[CLOCK_CLK26M].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_MUX_ANC_MD32].name,
			       aud_clks[CLOCK_CLK26M].name, ret);
			BUG();
			goto EXIT;
		}
	}
	if (Aud_ANC_Clk_cntr < 0) {
		PRINTK_AUDDRV("!! %s(), Aud_ADC_Clk_cntr (%d) < 0\n",
			      __func__,
			      Aud_ANC_Clk_cntr);
		Aud_ANC_Clk_cntr = 0;
	}
EXIT:
	mutex_unlock(&auddrv_pmic_mutex);
}
