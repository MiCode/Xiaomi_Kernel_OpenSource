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
 *   Audio Driver clock control implement
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
 ******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/clk.h>
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-afe.h"
#include "mtk-soc-digital-type.h"

#include <linux/spinlock.h>
#include <linux/delay.h>
#if defined(_MT_IDLE_HEADER) && !defined(CONFIG_FPGA_EARLY_PORTING)
#include "mtk_idle.h"
#include "mtk_spm_resource_req.h"
#endif
#include <linux/err.h>
#include <linux/platform_device.h>

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/
#define PRINTK_AUD_CLK(format, args...)

static int APLL1Counter;
static int APLL2Counter;
static int Aud_APLL_DIV_APLL1_cntr;
static int Aud_APLL_DIV_APLL2_cntr;
static unsigned int MCLKFS = 128;
static unsigned int MCLKFS_HDMI = 256;

int Aud_Core_Clk_cntr;
int Aud_AFE_Clk_cntr;
int Aud_I2S_Clk_cntr;
int Aud_TDM_Clk_cntr;
int Aud_ADC_Clk_cntr;
int Aud_ADC2_Clk_cntr;
int Aud_HDMI_Clk_cntr;
int Aud_APLL22M_Clk_cntr;
int Aud_APLL24M_Clk_cntr;
int Aud_APLL1_Tuner_cntr;
int Aud_APLL2_Tuner_cntr;
static int Aud_EMI_cntr;

static DEFINE_SPINLOCK(auddrv_Clk_lock);

/* amp mutex lock */
static DEFINE_MUTEX(auddrv_pmic_mutex);
static DEFINE_MUTEX(audEMI_Clk_mutex);
static DEFINE_MUTEX(auddrv_clk_mutex);

/* audio_apll_divider_group may vary by chip!!! */
enum audio_apll_divider_group {
	AUDIO_APLL1_DIV0 = 0,
	AUDIO_APLL2_DIV0 = 1,
	AUDIO_APLL12_DIV0 = 2,
	AUDIO_APLL12_DIV1 = 3,
	AUDIO_APLL12_DIV2 = 4,
	AUDIO_APLL12_DIV3 = 5,
	AUDIO_APLL12_DIV4 = 6,
	AUDIO_APLL12_DIVB = 7,
	AUDIO_APLL_DIV_NUM
};

/* mI2SAPLLDivSelect may vary by chip!!! */
static const unsigned int mI2SAPLLDivSelect[Soc_Aud_I2S_CLKDIV_NUMBER] = {
	AUDIO_APLL1_DIV0,
	AUDIO_APLL2_DIV0,
	AUDIO_APLL12_DIV0,
	AUDIO_APLL12_DIV1,
	AUDIO_APLL12_DIV2,
	AUDIO_APLL12_DIV3,
	AUDIO_APLL12_DIV4,
	AUDIO_APLL12_DIVB
};

enum audio_system_clock_type {
	CLOCK_AFE = 0,
	CLOCK_DAC,
	CLOCK_DAC_PREDIS,
	CLOCK_ADC,
	CLOCK_ADC_ADDA6,
	CLOCK_TML,
	CLOCK_APLL22M,
	CLOCK_APLL24M,
	CLOCK_APLL1_TUNER,
	CLOCK_APLL2_TUNER,
	CLOCK_SCP_SYS_AUD,
	CLOCK_INFRA_SYS_AUDIO,
	CLOCK_MTKAIF_26M_CLK,
	CLOCK_MUX_AUDIO,
	CLOCK_MUX_AUDIOINTBUS,
	CLOCK_TOP_SYSPLL_D2_D4,
	/* apll related mux */
	CLOCK_TOP_MUX_AUD_1,
	CLOCK_TOP_APLL1_CK,
	CLOCK_TOP_MUX_AUD_2,
	CLOCK_TOP_APLL2_CK,
	CLOCK_TOP_MUX_AUD_ENG1,
	CLOCK_TOP_APLL1_D8,
	CLOCK_TOP_MUX_AUD_ENG2,
	CLOCK_TOP_APLL2_D8,
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
	[CLOCK_DAC] = {"aud_dac_clk", false, false, NULL},
	[CLOCK_DAC_PREDIS] = {"aud_dac_predis_clk", false, false, NULL},
	[CLOCK_ADC] = {"aud_adc_clk", false, false, NULL},
	[CLOCK_ADC_ADDA6] = {"aud_adc_adda6_clk", false, false, NULL},
	[CLOCK_TML] = {"aud_tml_clk", false, false, NULL},
	[CLOCK_APLL22M] = {"aud_apll22m_clk", false, false, NULL},
	[CLOCK_APLL24M] = {"aud_apll24m_clk", false, false, NULL},
	[CLOCK_APLL1_TUNER] = {"aud_apll1_tuner_clk", false, false, NULL},
	[CLOCK_APLL2_TUNER] = {"aud_apll2_tuner_clk", false, false, NULL},
	[CLOCK_SCP_SYS_AUD] = {"scp_sys_audio", false, false, NULL},
	[CLOCK_INFRA_SYS_AUDIO] = {"aud_infra_clk", false, false, NULL},
	[CLOCK_MTKAIF_26M_CLK] = {"mtkaif_26m_clk", false, false, NULL},
	[CLOCK_MUX_AUDIO] = {"top_mux_audio", false, false, NULL},
	[CLOCK_MUX_AUDIOINTBUS] = {"top_mux_audio_int", false, false, NULL},
	[CLOCK_TOP_SYSPLL_D2_D4] = {"top_syspll_d2_d4", false, false, NULL},
	[CLOCK_TOP_MUX_AUD_1] = {"top_mux_aud_1", false, false, NULL},
	[CLOCK_TOP_APLL1_CK] = {"top_apll1_ck", false, false, NULL},
	[CLOCK_TOP_MUX_AUD_2] = {"top_mux_aud_2", false, false, NULL},
	[CLOCK_TOP_APLL2_CK] = {"top_apll2_ck", false, false, NULL},
	[CLOCK_TOP_MUX_AUD_ENG1] = {"top_mux_aud_eng1", false, false, NULL},
	[CLOCK_TOP_APLL1_D8] = {"top_apll1_d8", false, false, NULL},
	[CLOCK_TOP_MUX_AUD_ENG2] = {"top_mux_aud_eng2", false, false, NULL},
	[CLOCK_TOP_APLL2_D8] = {"top_apll2_d8", false, false, NULL},
	[CLOCK_CLK26M] = {"top_clk26m_clk", false, false, NULL}
};

static int apll1_mux_setting(bool enable);
static int apll2_mux_setting(bool enable);

static int aud_clk_enable(const struct audio_clock_attr *clk)
{
	int ret;

	if (clk->clk_prepare) {
		ret = clk_enable(clk->clock);
		if (ret) {
			pr_err("%s(), clk_enable %s fail, ret %d\n",
			       __func__, clk->name, ret);
			return -EPERM;
		}
	} else {
		pr_err("%s(), clk %s, not prepared\n", __func__, clk->name);
		return -EINVAL;
	}

	return 0;
}


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
			pr_err("%s devm_clk_get %s fail %d\n",
				__func__, aud_clks[i].name, ret);
		} else {
			aud_clks[i].clk_status = true;
		}
	}

	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		if (i == CLOCK_SCP_SYS_AUD)  /* CLOCK_SCP_SYS_AUD is MTCMOS */
			continue;

		if (aud_clks[i].clk_status) {
			ret = clk_prepare(aud_clks[i].clock);
			if (ret) {
				pr_err("%s clk_prepare %s fail %d\n",
				       __func__, aud_clks[i].name, ret);
			} else {
				aud_clks[i].clk_prepare = true;
			}
		}
	}

	/* Set APLL1/APLL2 as disable state when boot */
	apll1_mux_setting(false);
	apll2_mux_setting(false);

	return ret;
}

void AudDrv_Clk_Deinit(void *dev)
{
	size_t i;

	pr_debug("%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		if (i == CLOCK_SCP_SYS_AUD)  /* CLOCK_SCP_SYS_AUD is MTCMOS */
			continue;

		if (aud_clks[i].clock && !IS_ERR(aud_clks[i].clock)
		    && aud_clks[i].clk_prepare) {
			clk_unprepare(aud_clks[i].clock);
			aud_clks[i].clk_prepare = false;
		}
	}
}

#if defined(_MT_IDLE_HEADER) && !defined(CONFIG_FPGA_EARLY_PORTING)
static int audio_idle_notify_call(struct notifier_block *nfb,
				  unsigned long id,
				  void *arg)
{
	switch (id) {
	case NOTIFY_DPIDLE_ENTER:
	case NOTIFY_SOIDLE_ENTER:
		/* intbus pll -> 26m */
		if (aud_clk_enable(&aud_clks[CLOCK_MUX_AUDIOINTBUS]))
			return NOTIFY_OK;
		/* don't use AudDrv_AUDINTBUS_Sel in notify_call */
		aud_intbus_mux_sel(0);

		if (aud_clks[CLOCK_MUX_AUDIOINTBUS].clk_prepare)
			clk_disable(aud_clks[CLOCK_MUX_AUDIOINTBUS].clock);

		break;
	case NOTIFY_DPIDLE_LEAVE:
	case NOTIFY_SOIDLE_LEAVE:
		/* Audio Clock: 26m -> pll */
		if (aud_clk_enable(&aud_clks[CLOCK_MUX_AUDIOINTBUS]))
			return NOTIFY_OK;
		/* don't use AudDrv_AUDINTBUS_Sel in notify_call */
		aud_intbus_mux_sel(1);

		if (aud_clks[CLOCK_MUX_AUDIOINTBUS].clk_prepare)
			clk_disable(aud_clks[CLOCK_MUX_AUDIOINTBUS].clock);

		break;
	case NOTIFY_SOIDLE3_ENTER:
	case NOTIFY_SOIDLE3_LEAVE:
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block audio_idle_nfb = {
	.notifier_call = audio_idle_notify_call,
};
#endif

void AudDrv_Clk_Global_Variable_Init(void)
{
	APLL1Counter = 0;
	APLL2Counter = 0;
	Aud_APLL_DIV_APLL1_cntr = 0;
	Aud_APLL_DIV_APLL2_cntr = 0;
	MCLKFS = 128;
	MCLKFS_HDMI = 256;

#if defined(_MT_IDLE_HEADER) && !defined(CONFIG_FPGA_EARLY_PORTING)
	mtk_idle_notifier_register(&audio_idle_nfb);
#endif
}

void AudDrv_Bus_Init(void)
{
	/* No need on 6771, system default set bit14 to 1 */
}

/* should only be used when auddrv_clk_on */
void AudDrv_AUDINTBUS_Sel(int parentidx)
{
	int ret = 0;

	if (!aud_clks[CLOCK_MUX_AUDIOINTBUS].clk_prepare ||
	    !aud_clks[CLOCK_TOP_SYSPLL_D2_D4].clk_prepare ||
	    !aud_clks[CLOCK_CLK26M].clk_prepare) {
		pr_warn("%s(), clk_prepare = false\n", __func__);
		goto EXIT;
	}

	PRINTK_AUD_CLK("+%s(), parentidx = %d\n", __func__, parentidx);
	if (parentidx == 1) {
		ret = clk_set_parent(aud_clks[CLOCK_MUX_AUDIOINTBUS].clock,
				     aud_clks[CLOCK_TOP_SYSPLL_D2_D4].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_MUX_AUDIOINTBUS].name,
			       aud_clks[CLOCK_TOP_SYSPLL_D2_D4].name, ret);
			goto EXIT;
		}
	} else if (parentidx == 0) {
		ret = clk_set_parent(aud_clks[CLOCK_MUX_AUDIOINTBUS].clock,
				     aud_clks[CLOCK_CLK26M].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_MUX_AUDIOINTBUS].name,
			       aud_clks[CLOCK_CLK26M].name, ret);
			goto EXIT;
		}
	}
EXIT:
	PRINTK_AUD_CLK("-%s()\n", __func__);
}

static int apll1_mux_setting(bool enable)
{
	int ret = 0;

	if (!aud_clks[CLOCK_TOP_MUX_AUD_1].clk_prepare ||
	    !aud_clks[CLOCK_TOP_APLL1_CK].clk_prepare ||
	    !aud_clks[CLOCK_TOP_MUX_AUD_ENG1].clk_prepare ||
	    !aud_clks[CLOCK_TOP_APLL1_D8].clk_prepare ||
	    !aud_clks[CLOCK_CLK26M].clk_prepare) {
		pr_warn("%s(), clk_prepare = false\n", __func__);
		return -EINVAL;
	}

	PRINTK_AUD_CLK("+%s(), enable %d\n", __func__, enable);
	if (enable) {
		/* CLOCK_TOP_MUX_AUD_1 */
		if (aud_clk_enable(&aud_clks[CLOCK_TOP_MUX_AUD_1]))
			goto EXIT;

		ret = clk_set_parent(aud_clks[CLOCK_TOP_MUX_AUD_1].clock,
				     aud_clks[CLOCK_TOP_APLL1_CK].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_MUX_AUD_1].name,
			       aud_clks[CLOCK_TOP_APLL1_CK].name, ret);
			goto EXIT;
		}

		/* CLOCK_TOP_MUX_AUD_ENG1 */
		if (aud_clk_enable(&aud_clks[CLOCK_TOP_MUX_AUD_ENG1]))
			goto EXIT;

		ret = clk_set_parent(aud_clks[CLOCK_TOP_MUX_AUD_ENG1].clock,
				     aud_clks[CLOCK_TOP_APLL1_D8].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_MUX_AUD_ENG1].name,
			       aud_clks[CLOCK_TOP_APLL1_D8].name, ret);
			goto EXIT;
		}

	} else {
		/* CLOCK_TOP_MUX_AUD_ENG1 */
		ret = clk_set_parent(aud_clks[CLOCK_TOP_MUX_AUD_ENG1].clock,
				     aud_clks[CLOCK_CLK26M].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_MUX_AUD_ENG1].name,
			       aud_clks[CLOCK_CLK26M].name, ret);
			goto EXIT;
		}
		if (aud_clks[CLOCK_TOP_MUX_AUD_ENG1].clk_prepare)
			clk_disable(aud_clks[CLOCK_TOP_MUX_AUD_ENG1].clock);

		/* CLOCK_TOP_MUX_AUD_1 */
		ret = clk_set_parent(aud_clks[CLOCK_TOP_MUX_AUD_1].clock,
				     aud_clks[CLOCK_CLK26M].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_MUX_AUD_1].name,
			       aud_clks[CLOCK_CLK26M].name, ret);
			goto EXIT;
		}

		if (aud_clks[CLOCK_TOP_MUX_AUD_1].clk_prepare)
			clk_disable(aud_clks[CLOCK_TOP_MUX_AUD_1].clock);
	}

EXIT:
	PRINTK_AUD_CLK("-%s()\n", __func__);
	return 0;
}

static int apll2_mux_setting(bool enable)
{
	int ret = 0;

	if (!aud_clks[CLOCK_TOP_MUX_AUD_2].clk_prepare ||
	    !aud_clks[CLOCK_TOP_APLL2_CK].clk_prepare ||
	    !aud_clks[CLOCK_TOP_MUX_AUD_ENG2].clk_prepare ||
	    !aud_clks[CLOCK_TOP_APLL2_D8].clk_prepare ||
	    !aud_clks[CLOCK_CLK26M].clk_prepare) {
		pr_warn("%s(), clk_prepare = false\n", __func__);
		return -EINVAL;
	}

	PRINTK_AUD_CLK("+%s(), enable %d\n", __func__, enable);
	if (enable) {
		/* CLOCK_TOP_MUX_AUD_2 */
		if (aud_clk_enable(&aud_clks[CLOCK_TOP_MUX_AUD_2]))
			goto EXIT;

		ret = clk_set_parent(aud_clks[CLOCK_TOP_MUX_AUD_2].clock,
				     aud_clks[CLOCK_TOP_APLL2_CK].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_MUX_AUD_2].name,
			       aud_clks[CLOCK_TOP_APLL2_CK].name, ret);
			goto EXIT;
		}

		/* CLOCK_TOP_MUX_AUD_ENG2 */
		if (aud_clk_enable(&aud_clks[CLOCK_TOP_MUX_AUD_ENG2]))
			goto EXIT;

		ret = clk_set_parent(aud_clks[CLOCK_TOP_MUX_AUD_ENG2].clock,
				     aud_clks[CLOCK_TOP_APLL2_D8].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_MUX_AUD_ENG2].name,
			       aud_clks[CLOCK_TOP_APLL2_D8].name, ret);
			goto EXIT;
		}

	} else {
		/* CLOCK_TOP_MUX_AUD_ENG1 */
		ret = clk_set_parent(aud_clks[CLOCK_TOP_MUX_AUD_ENG2].clock,
				     aud_clks[CLOCK_CLK26M].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_MUX_AUD_ENG2].name,
			       aud_clks[CLOCK_CLK26M].name, ret);
			goto EXIT;
		}
		if (aud_clks[CLOCK_TOP_MUX_AUD_ENG2].clk_prepare)
			clk_disable(aud_clks[CLOCK_TOP_MUX_AUD_ENG2].clock);

		/* CLOCK_TOP_MUX_AUD_1 */
		ret = clk_set_parent(aud_clks[CLOCK_TOP_MUX_AUD_2].clock,
				     aud_clks[CLOCK_CLK26M].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_TOP_MUX_AUD_2].name,
			       aud_clks[CLOCK_CLK26M].name, ret);
			goto EXIT;
		}

		if (aud_clks[CLOCK_TOP_MUX_AUD_2].clk_prepare)
			clk_disable(aud_clks[CLOCK_TOP_MUX_AUD_2].clock);
	}

EXIT:
	PRINTK_AUD_CLK("-%s()\n", __func__);
	return 0;
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
	int ret = 0;

	pr_debug("+%s(), Aud_AFE_Clk_cntr:%d\n", __func__, Aud_AFE_Clk_cntr);
	mutex_lock(&auddrv_clk_mutex);
	Aud_AFE_Clk_cntr++;
	if (Aud_AFE_Clk_cntr == 1) {
		if (aud_clks[CLOCK_SCP_SYS_AUD].clk_status) {
			ret = clk_prepare_enable(
				aud_clks[CLOCK_SCP_SYS_AUD].clock);
			if (ret) {
				pr_err("%s [CCF]Aud clk_prepare_enable %s fail\n",
					__func__,
				       aud_clks[CLOCK_SCP_SYS_AUD].name);
				goto EXIT;
			}
		}

		if (aud_clk_enable(&aud_clks[CLOCK_INFRA_SYS_AUDIO]))
			goto EXIT;

		if (aud_clk_enable(&aud_clks[CLOCK_MUX_AUDIO]))
			goto EXIT;

		ret = clk_set_parent(aud_clks[CLOCK_MUX_AUDIO].clock,
				     aud_clks[CLOCK_CLK26M].clock);
		if (ret) {
			pr_err("%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLOCK_MUX_AUDIO].name,
			       aud_clks[CLOCK_CLK26M].name, ret);
		}

		if (aud_clk_enable(&aud_clks[CLOCK_MUX_AUDIOINTBUS]))
			goto EXIT;
		AudDrv_AUDINTBUS_Sel(1);

		if (aud_clk_enable(&aud_clks[CLOCK_AFE]))
			goto EXIT;

		if (aud_clk_enable(&aud_clks[CLOCK_DAC]))
			goto EXIT;

		if (aud_clk_enable(&aud_clks[CLOCK_DAC_PREDIS]))
			goto EXIT;

		if (aud_clk_enable(&aud_clks[CLOCK_MTKAIF_26M_CLK]))
			goto EXIT;

		/* enable audio sys DCM for power saving */
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x1 << 29, 0x1 << 29);

	}
EXIT:
	mutex_unlock(&auddrv_clk_mutex);
	PRINTK_AUD_CLK("-%s(), Aud_AFE_Clk_cntr:%d\n",
		__func__, Aud_AFE_Clk_cntr);
}
EXPORT_SYMBOL(AudDrv_Clk_On);

void AudDrv_Clk_Off(void)
{
	pr_debug("+!! %s(), Aud_AFE_Clk_cntr:%d\n", __func__, Aud_AFE_Clk_cntr);
	mutex_lock(&auddrv_clk_mutex);

	Aud_AFE_Clk_cntr--;
	if (Aud_AFE_Clk_cntr == 0) {
		/* Disable AFE clock */

		if (aud_clks[CLOCK_MTKAIF_26M_CLK].clk_prepare)
			clk_disable(aud_clks[CLOCK_MTKAIF_26M_CLK].clock);

		/* Make sure all IRQ status is cleared */
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 0xffff, 0xffff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 0xffff, 0xffff);

		if (aud_clks[CLOCK_AFE].clk_prepare)
			clk_disable(aud_clks[CLOCK_AFE].clock);

		if (aud_clks[CLOCK_DAC].clk_prepare)
			clk_disable(aud_clks[CLOCK_DAC].clock);

		if (aud_clks[CLOCK_DAC_PREDIS].clk_prepare)
			clk_disable(aud_clks[CLOCK_DAC_PREDIS].clock);

		AudDrv_AUDINTBUS_Sel(0);
		if (aud_clks[CLOCK_MUX_AUDIOINTBUS].clk_prepare)
			clk_disable(aud_clks[CLOCK_MUX_AUDIOINTBUS].clock);

		if (aud_clks[CLOCK_MUX_AUDIO].clk_prepare)
			clk_disable(aud_clks[CLOCK_MUX_AUDIO].clock);

		if (aud_clks[CLOCK_INFRA_SYS_AUDIO].clk_prepare)
			clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

		if (aud_clks[CLOCK_SCP_SYS_AUD].clk_status)
			clk_disable_unprepare(
			aud_clks[CLOCK_SCP_SYS_AUD].clock);
	} else if (Aud_AFE_Clk_cntr < 0) {
		pr_warn("!! %s(), Aud_AFE_Clk_cntr<0 (%d)\n", __func__,
			Aud_AFE_Clk_cntr);
		Aud_AFE_Clk_cntr = 0;
	}
	mutex_unlock(&auddrv_clk_mutex);
	PRINTK_AUD_CLK("-!! %s(), Aud_AFE_Clk_cntr:%d\n", __func__,
		Aud_AFE_Clk_cntr);
}
EXPORT_SYMBOL(AudDrv_Clk_Off);

void AudDrv_ANA_Clk_On(void)
{
}
EXPORT_SYMBOL(AudDrv_ANA_Clk_On);

void AudDrv_ANA_Clk_Off(void)
{
}
EXPORT_SYMBOL(AudDrv_ANA_Clk_Off);

void AudDrv_ADC_Clk_On(void)
{
	int ret = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	if (Aud_ADC_Clk_cntr == 0) {
		if (aud_clks[CLOCK_ADC].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_ADC].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock %s fail",
					__func__, aud_clks[CLOCK_ADC].name);
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error %s fail",
				__func__, aud_clks[CLOCK_ADC].name);
			goto EXIT;
		}
	}
	Aud_ADC_Clk_cntr++;
EXIT:
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_ADC_Clk_Off(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_ADC_Clk_cntr--;
	if (Aud_ADC_Clk_cntr == 0) {
		if (aud_clks[CLOCK_ADC].clk_prepare)
			clk_disable(aud_clks[CLOCK_ADC].clock);
	}
	if (Aud_ADC_Clk_cntr < 0) {
		pr_warn("%s(), Aud_ADC_Clk_cntr %d < 0\n",
			__func__, Aud_ADC_Clk_cntr);
		Aud_ADC_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_ADC2_Clk_On(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	if (Aud_ADC2_Clk_cntr == 0) {
		if (aud_clk_enable(&aud_clks[CLOCK_ADC_ADDA6]))
			goto EXIT;
	}
	Aud_ADC2_Clk_cntr++;
EXIT:
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_ADC2_Clk_Off(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_ADC2_Clk_cntr--;
	if (Aud_ADC2_Clk_cntr == 0) {
		if (aud_clks[CLOCK_ADC_ADDA6].clk_prepare)
			clk_disable(aud_clks[CLOCK_ADC_ADDA6].clock);
	}
	if (Aud_ADC2_Clk_cntr < 0) {
		pr_warn("%s(), Aud_ADC2_Clk_cntr %d < 0\n",
			__func__, Aud_ADC2_Clk_cntr);
		Aud_ADC2_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_ADC3_Clk_On(void)
{
}

void AudDrv_ADC3_Clk_Off(void)
{
}

void AudDrv_ADC_Hires_Clk_On(void)
{
}

void AudDrv_ADC_Hires_Clk_Off(void)
{
}

void AudDrv_ADC2_Hires_Clk_On(void)
{

}

void AudDrv_ADC2_Hires_Clk_Off(void)
{
}

void AudDrv_APLL22M_Clk_On(void)
{
	int ret = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("+%s counter = %d\n", __func__, Aud_APLL22M_Clk_cntr);

	if (Aud_APLL22M_Clk_cntr == 0) {
		if (aud_clks[CLOCK_APLL22M].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_APLL22M].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock %s fail",
				       __func__, aud_clks[CLOCK_APLL22M].name);
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error %s fail",
			       __func__, aud_clks[CLOCK_APLL22M].name);
			goto EXIT;
		}
	}
	Aud_APLL22M_Clk_cntr++;
EXIT:
	PRINTK_AUD_CLK("-%s: counter = %d\n", __func__, Aud_APLL22M_Clk_cntr);
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_APLL22M_Clk_Off(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	PRINTK_AUD_CLK("+%s: counter = %d\n", __func__, Aud_APLL22M_Clk_cntr);

	Aud_APLL22M_Clk_cntr--;

	if (Aud_APLL22M_Clk_cntr == 0) {
		if (aud_clks[CLOCK_APLL22M].clk_prepare) {
			clk_disable(aud_clks[CLOCK_APLL22M].clock);
		} else {
			pr_err("%s [CCF]clk_prepare error %s fail",
			       __func__, aud_clks[CLOCK_APLL22M].name);
			goto EXIT;
		}
	}

EXIT:
	if (Aud_APLL22M_Clk_cntr < 0) {
		pr_warn("err, %s <0 (%d)\n", __func__,
			Aud_APLL22M_Clk_cntr);
		Aud_APLL22M_Clk_cntr = 0;
	}

	PRINTK_AUD_CLK("-%s: counter = %d\n", __func__, Aud_APLL22M_Clk_cntr);
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}


void AudDrv_APLL24M_Clk_On(void)
{
	int ret = 0;
	unsigned long flags = 0;

	PRINTK_AUD_CLK("+%s counter = %d\n", __func__, Aud_APLL24M_Clk_cntr);

	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	if (Aud_APLL24M_Clk_cntr == 0) {
		if (aud_clks[CLOCK_APLL24M].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_APLL24M].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock %s fail",
				       __func__, aud_clks[CLOCK_APLL24M].name);
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error %s fail",
			       __func__, aud_clks[CLOCK_APLL24M].name);
			goto EXIT;
		}
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
		if (aud_clks[CLOCK_APLL24M].clk_prepare) {
			clk_disable(aud_clks[CLOCK_APLL24M].clock);
		} else {
			pr_err("%s [CCF]clk_prepare error %s fail",
			       __func__, aud_clks[CLOCK_APLL24M].name);
			goto EXIT;
		}
	}
EXIT:
	if (Aud_APLL24M_Clk_cntr < 0) {
		pr_warn("%s <0 (%d)\n", __func__,
			Aud_APLL24M_Clk_cntr);
		Aud_APLL24M_Clk_cntr = 0;
	}

	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void aud_top_con_pdn_i2s(bool _pdn)
{
	if (_pdn)
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x1 << 6, 0x1 << 6);
	else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x0 << 6, 0x1 << 6);
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
		pr_warn("!! %s(), Aud_I2S_Clk_cntr<0 (%d)\n", __func__,
			Aud_I2S_Clk_cntr);
		Aud_I2S_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}
EXPORT_SYMBOL(AudDrv_I2S_Clk_Off);

void aud_top_con_pdn_tdm_ck(bool _pdn)
{
	Afe_Set_Reg(AUDIO_TOP_CON0, _pdn << 20, 0x1 << 20);
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
		pr_warn("!! %s(), Aud_TDM_Clk_cntr<0 (%d)\n",
			__func__, Aud_TDM_Clk_cntr);
		Aud_TDM_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}
EXPORT_SYMBOL(AudDrv_TDM_Clk_Off);

void AudDrv_APLL1Tuner_Clk_On(void)
{
	unsigned long flags = 0;
	int ret = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_APLL1_Tuner_cntr == 0) {
		PRINTK_AUD_CLK("+%s, Aud_APLL1_Tuner_cntr:%d\n", __func__,
			       Aud_APLL1_Tuner_cntr);
		if (aud_clks[CLOCK_APLL1_TUNER].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_APLL1_TUNER].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock %s fail\n",
				       __func__,
				       aud_clks[CLOCK_APLL1_TUNER].name);
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error %s fail\n",
			       __func__, aud_clks[CLOCK_APLL1_TUNER].name);
			goto EXIT;
		}

		SetApmixedCfg(AP_PLL_CON5, 0x1, 0x1);
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
		SetApmixedCfg(AP_PLL_CON5, 0x0, 0x1);
		if (aud_clks[CLOCK_APLL1_TUNER].clk_prepare)
			clk_disable(aud_clks[CLOCK_APLL1_TUNER].clock);
	} else if (Aud_APLL1_Tuner_cntr < 0) {
		pr_warn("!! %s, Aud_APLL1_Tuner_cntr<0 (%d)\n",
			__func__, Aud_APLL1_Tuner_cntr);
		Aud_APLL1_Tuner_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}


void AudDrv_APLL2Tuner_Clk_On(void)
{
	unsigned long flags = 0;
	int ret = 0;

	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_APLL2_Tuner_cntr == 0) {
		PRINTK_AUD_CLK("+%s, Aud_APLL2_Tuner_cntr:%d\n", __func__,
			       Aud_APLL2_Tuner_cntr);

		if (aud_clks[CLOCK_APLL2_TUNER].clk_prepare) {
			ret = clk_enable(aud_clks[CLOCK_APLL2_TUNER].clock);
			if (ret) {
				pr_err("%s [CCF]Aud enable_clock %s fail\n",
				       __func__,
				       aud_clks[CLOCK_APLL2_TUNER].name);
				goto EXIT;
			}
		} else {
			pr_err("%s [CCF]clk_prepare error %s fail\n",
			       __func__, aud_clks[CLOCK_APLL2_TUNER].name);
			goto EXIT;
		}

		SetApmixedCfg(AP_PLL_CON5, 0x1 << 1, 0x1 << 1);
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
		SetApmixedCfg(AP_PLL_CON5, 0x0 << 1, 0x1 << 1);
		if (aud_clks[CLOCK_APLL2_TUNER].clk_prepare)
			clk_disable(aud_clks[CLOCK_APLL2_TUNER].clock);
	} else if (Aud_APLL2_Tuner_cntr < 0) {
		pr_warn("!! %s, Aud_APLL1_Tuner_cntr<0 (%d)\n",
			__func__, Aud_APLL2_Tuner_cntr);
		Aud_APLL2_Tuner_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}

void AudDrv_HDMI_Clk_On(void)
{
	PRINTK_AUD_CLK("+%s(), Aud_I2S_Clk_cntr:%d\n", __func__,
		Aud_HDMI_Clk_cntr);
	if (Aud_HDMI_Clk_cntr == 0) {
		AudDrv_ANA_Clk_On();
		AudDrv_Clk_On();
	}
	Aud_HDMI_Clk_cntr++;
}

void AudDrv_HDMI_Clk_Off(void)
{
	PRINTK_AUD_CLK("+%s(), Aud_I2S_Clk_cntr:%d\n", __func__,
		       Aud_HDMI_Clk_cntr);
	Aud_HDMI_Clk_cntr--;
	if (Aud_HDMI_Clk_cntr == 0) {
		AudDrv_ANA_Clk_Off();
		AudDrv_Clk_Off();
	} else if (Aud_HDMI_Clk_cntr < 0) {
		pr_warn("!! %s(), Aud_I2S_Clk_cntr<0 (%d)\n", __func__,
			Aud_HDMI_Clk_cntr);
		Aud_HDMI_Clk_cntr = 0;
	}
	PRINTK_AUD_CLK("-%s(), Aud_I2S_Clk_cntr:%d\n", __func__,
		Aud_HDMI_Clk_cntr);
}

void AudDrv_Emi_Clk_On(void)
{
	mutex_lock(&auddrv_pmic_mutex);
	if (Aud_EMI_cntr == 0) {
#if defined(_MT_IDLE_HEADER) && !defined(CONFIG_FPGA_EARLY_PORTING)
		/* mutex is used in these api */
		spm_resource_req(SPM_RESOURCE_USER_AUDIO, SPM_RESOURCE_ALL);
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
#if defined(_MT_IDLE_HEADER) && !defined(CONFIG_FPGA_EARLY_PORTING)
		/* mutex is used in these api */
		spm_resource_req(SPM_RESOURCE_USER_AUDIO, SPM_RESOURCE_RELEASE);
#endif
	}

	if (Aud_EMI_cntr < 0) {
		Aud_EMI_cntr = 0;
		pr_warn("Aud_EMI_cntr = %d\n", Aud_EMI_cntr);
	}
	mutex_unlock(&auddrv_pmic_mutex);
}

void AudDrv_ANC_Clk_On(void)
{
}

void AudDrv_ANC_Clk_Off(void)
{
}

unsigned int GetApllbySampleRate(unsigned int SampleRate)
{
	if (SampleRate == 176400 || SampleRate == 88200 ||
	    SampleRate == 44100 ||
	    SampleRate == 22050 || SampleRate == 11025)
		return Soc_Aud_APLL1;
	else
		return Soc_Aud_APLL2;
}

void SetckSel(unsigned int I2snum, unsigned int SampleRate)
{
	unsigned int ApllSource;

	if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1)
		ApllSource = 0;
	else
		ApllSource = 1;

	switch (I2snum) {
	case Soc_Aud_I2S0:
		clksys_set_reg(CLK_AUDDIV_0, ApllSource << 8, 1 << 8);
		break;
	case Soc_Aud_I2S1:
		clksys_set_reg(CLK_AUDDIV_0, ApllSource << 9, 1 << 9);
		break;
	case Soc_Aud_I2S2:
		clksys_set_reg(CLK_AUDDIV_0, ApllSource << 10, 1 << 10);
		break;
	case Soc_Aud_I2S3:
		clksys_set_reg(CLK_AUDDIV_0, ApllSource << 11, 1 << 11);
		break;
	default:
		pr_warn("%s(), not support I2snum %u\n", __func__, I2snum);
		break;
	}
	pr_debug("%s I2snum = %d ApllSource = %d\n",
		__func__, I2snum, ApllSource);
}

void EnableALLbySampleRate(unsigned int SampleRate)
{
	pr_debug("%s(), APLL1Counter = %d, APLL2Counter = %d, SampleRate = %d\n",
		 __func__, APLL1Counter, APLL2Counter, SampleRate);

	switch (GetApllbySampleRate(SampleRate)) {
	case Soc_Aud_APLL1:
		APLL1Counter++;
		if (APLL1Counter == 1) {
			EnableApll1(true);
			AudDrv_APLL1Tuner_Clk_On();
		}
		break;
	case Soc_Aud_APLL2:
		APLL2Counter++;
		if (APLL2Counter == 1) {
			EnableApll2(true);
			AudDrv_APLL2Tuner_Clk_On();
		}
		break;
	default:
		pr_warn("%s(), invalid SampleRate %d\n", __func__, SampleRate);
		break;
	}
}

void DisableALLbySampleRate(unsigned int SampleRate)
{
	pr_debug("%s, APLL1Counter = %d, APLL2Counter = %d, SampleRate = %d\n",
		 __func__, APLL1Counter, APLL2Counter, SampleRate);

	switch (GetApllbySampleRate(SampleRate)) {
	case Soc_Aud_APLL1:
		APLL1Counter--;
		if (APLL1Counter == 0) {
			AudDrv_APLL1Tuner_Clk_Off();
			EnableApll1(false);
		} else if (APLL1Counter < 0) {
			pr_warn("%s(), APLL1Counter %d < 0\n",
				__func__, APLL1Counter);
			APLL1Counter = 0;
		}
		break;
	case Soc_Aud_APLL2:
		APLL2Counter--;
		if (APLL2Counter == 0) {
			AudDrv_APLL2Tuner_Clk_Off();
			EnableApll2(false);
		} else if (APLL2Counter < 0) {
			pr_warn("%s(), APLL2Counter %d < 0\n",
				__func__, APLL2Counter);
			APLL2Counter = 0;
		}
		break;
	default:
		pr_warn("%s(), invalid SampleRate %d\n", __func__, SampleRate);
		break;
	}
}

void EnableI2SDivPower(unsigned int Diveder_name, bool bEnable)
{
	pr_debug("%s, bEnable = %d, Diveder_name %u\n",
		 __func__, bEnable, Diveder_name);

	if (bEnable)
		clksys_set_reg(CLK_AUDDIV_0, 0 << Diveder_name,
			1 << Diveder_name);
	else
		clksys_set_reg(CLK_AUDDIV_0, 1 << Diveder_name,
			1 << Diveder_name);
}

void EnableI2SCLKDiv(unsigned int I2snum, bool bEnable)
{
	pr_debug("%s mI2SAPLLDivSelect = %d, i2snum = %d\n", __func__,
		 mI2SAPLLDivSelect[I2snum], I2snum);
	EnableI2SDivPower(mI2SAPLLDivSelect[I2snum], bEnable);
}

void EnableApll1(bool enable)
{
	pr_debug("%s enable = %d\n", __func__, enable);

	if (enable) {
		if (Aud_APLL_DIV_APLL1_cntr == 0) {
			/* 180.6336 / 8 = 22.5792MHz */
			clksys_set_reg(CLK_AUDDIV_0, 7 << 24, 0xf << 24);
			AudDrv_APLL22M_Clk_On();
			apll1_mux_setting(true);
			Afe_Set_Reg(AFE_HD_ENGEN_ENABLE, 0x1 << 0, 0x1 << 0);
		}

		Aud_APLL_DIV_APLL1_cntr++;
	} else {
		Aud_APLL_DIV_APLL1_cntr--;

		if (Aud_APLL_DIV_APLL1_cntr == 0) {
			Afe_Set_Reg(AFE_HD_ENGEN_ENABLE, 0x0 << 0, 0x1 << 0);
			apll1_mux_setting(false);
			AudDrv_APLL22M_Clk_Off();
		}
	}
}

void EnableApll2(bool enable)
{
	pr_debug("%s enable = %d\n", __func__, enable);

	if (enable) {
		if (Aud_APLL_DIV_APLL2_cntr == 0) {
			/* 196.608 / 8 = 24.576MHz */
			clksys_set_reg(CLK_AUDDIV_0, 7 << 28, 0xf << 28);
			AudDrv_APLL24M_Clk_On();
			apll2_mux_setting(true);
			Afe_Set_Reg(AFE_HD_ENGEN_ENABLE, 0x1 << 1, 0x1 << 1);
		}
		Aud_APLL_DIV_APLL2_cntr++;
	} else {
		Aud_APLL_DIV_APLL2_cntr--;
		if (Aud_APLL_DIV_APLL2_cntr == 0) {
			Afe_Set_Reg(AFE_HD_ENGEN_ENABLE, 0x0 << 1, 0x1 << 1);
			apll2_mux_setting(false);
			AudDrv_APLL24M_Clk_Off();
		}
	}
}

unsigned int SetCLkMclk(unsigned int I2snum, unsigned int SampleRate)
{
	unsigned int I2S_APll = 0;
	unsigned int I2s_ck_div = 0;

	if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1)
		I2S_APll = APLL_44K_BASE;
	else
		I2S_APll = APLL_48K_BASE;

	SetckSel(I2snum, SampleRate);	/* set I2Sx mck source */

	switch (I2snum) {
	case Soc_Aud_I2S0:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		clksys_set_reg(CLK_AUDDIV_1, I2s_ck_div << 0, 0xff << 0);
		break;
	case Soc_Aud_I2S1:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		clksys_set_reg(CLK_AUDDIV_1, I2s_ck_div << 8, 0xff << 8);
		break;
	case Soc_Aud_I2S2:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		clksys_set_reg(CLK_AUDDIV_1, I2s_ck_div << 16, 0xff << 16);
		break;
	case Soc_Aud_I2S3:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		clksys_set_reg(CLK_AUDDIV_1, I2s_ck_div << 24, 0xff << 24);
		break;
	default:
		pr_warn("[AudioWarn] %s(): I2snum = %d not recognized\n",
			__func__, I2snum);
		break;
	}

	pr_debug("%s I2snum = %d I2s_ck_div = %d I2S_APll = %d\n",
		 __func__, I2snum, I2s_ck_div, I2S_APll);

	return I2s_ck_div;
}

void SetCLkBclk(unsigned int MckDiv, unsigned int SampleRate,
		unsigned int Channels, unsigned int Wlength)
{
}

void PowerDownAllI2SDiv(void)
{
	int i = 0;

	for (i = AUDIO_APLL1_DIV0; i < AUDIO_APLL_DIV_NUM; i++)
		EnableI2SDivPower(i, false);
}
