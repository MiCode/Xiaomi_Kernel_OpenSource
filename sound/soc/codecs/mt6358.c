// SPDX-License-Identifier: GPL-2.0
//
// mt6358.c  --  mt6358 ALSA SoC audio codec driver
//
// Copyright (c) 2018 MediaTek Inc.
// Author: KaiChieh Chuang <kaichieh.chuang@mediatek.com>

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <linux/kthread.h>
#include <linux/sched.h>

#include <sound/soc.h>

#include <mach/upmu_hw.h>

#define ANALOG_HPTRIM

#ifdef CONFIG_MTK_ACCDET
#include "accdet.h"
#endif

#include <mach/mtk_pmic.h>
#include <mt-plat/mtk_auxadc_intf.h>

#ifndef CONFIG_MTK_PMIC_WRAP	/* y: use regmap, else use legacy api */
#ifdef CONFIG_MTK_PMIC_WRAP_HAL	/* y: legacy api is defined */
#include <mach/mtk_pmic_wrap.h>
#endif
#endif

#ifdef CONFIG_MTK_PMIC_WRAP
#include <linux/soc/mediatek/pmic_wrap.h>
#endif

#include "mt6358.h"

enum {
	AUDIO_ANALOG_VOLUME_HSOUTL,
	AUDIO_ANALOG_VOLUME_HSOUTR,
	AUDIO_ANALOG_VOLUME_HPOUTL,
	AUDIO_ANALOG_VOLUME_HPOUTR,
	AUDIO_ANALOG_VOLUME_LINEOUTL,
	AUDIO_ANALOG_VOLUME_LINEOUTR,
	AUDIO_ANALOG_VOLUME_MICAMP1,
	AUDIO_ANALOG_VOLUME_MICAMP2,
	AUDIO_ANALOG_VOLUME_TYPE_MAX
};

enum {
	MUX_ADC_L,
	MUX_ADC_R,
	MUX_PGA_L,
	MUX_PGA_R,
	MUX_MIC_TYPE,
	MUX_HP_L,
	MUX_HP_R,
	MUX_NUM,
};

enum {
	DEVICE_HP,
	DEVICE_LO,
	DEVICE_RCV,
	DEVICE_MIC1,
	DEVICE_MIC2,
	DEVICE_NUM
};

/* Supply widget subseq */
enum {
	/* common */
	SUPPLY_SEQ_CLK_BUF,
	SUPPLY_SEQ_AUD_GLB,
	SUPPLY_SEQ_CLKSQ,
	SUPPLY_SEQ_AUD_VOW,
	SUPPLY_SEQ_VOW_CLK,
	SUPPLY_SEQ_VOW_LDO,
	SUPPLY_SEQ_TOP_CK,
	SUPPLY_SEQ_TOP_CK_LAST,
	SUPPLY_SEQ_AUD_TOP,
	SUPPLY_SEQ_AUD_TOP_LAST,
	SUPPLY_SEQ_AFE,
	SUPPLY_SEQ_MIC_BIAS,
	/* capture */
	SUPPLY_SEQ_ADC_SUPPLY,
};

enum {
	CH_L = 0,
	CH_R,
	NUM_CH,
};

/* Auxadc average resolution */
enum {
	AUXADC_AVG_1 = 0,
	AUXADC_AVG_4,
	AUXADC_AVG_8,
	AUXADC_AVG_16,
	AUXADC_AVG_32,
	AUXADC_AVG_64,
	AUXADC_AVG_128,
	AUXADC_AVG_256,
};

enum {
	DBG_DCTRIM_BYPASS_4POLE = 0x1 << 0,
	DBG_DCTRIM_4POLE_LOG = 0x1 << 1,
};

#define REG_STRIDE 2

#ifdef ANALOG_HPTRIM
struct ana_offset {
	int enable;
	int hp_trim_code[NUM_CH];
	int hp_fine_trim[NUM_CH];
};
#endif

struct dc_trim_data {
	bool calibrated;
	int hp_offset[NUM_CH];
	int hp_trim_offset[NUM_CH];
	int spk_l_offset;
	int pre_comp_value[NUM_CH];
	int mic_vinp_mv;
#ifdef ANALOG_HPTRIM
	int dc_compensation_disabled;
	unsigned int hp_3_pole_trim_setting;
	unsigned int hp_4_pole_trim_setting;
	unsigned int spk_hp_3_pole_trim_setting;
	unsigned int spk_hp_4_pole_trim_setting;
	struct ana_offset hp_3_pole_ana_offset;
	struct ana_offset hp_4_pole_ana_offset;
	struct ana_offset spk_3_pole_ana_offset;
	struct ana_offset spk_4_pole_ana_offset;
#endif
};

struct mt6358_priv {
	struct device *dev;
	struct regmap *regmap;

	unsigned int dl_rate;
	unsigned int ul_rate;

	int ana_gain[AUDIO_ANALOG_VOLUME_TYPE_MAX];
	unsigned int mux_select[MUX_NUM];
	int dmic_one_wire_mode;

	int dev_counter[DEVICE_NUM];

	struct mt6358_codec_ops ops;
	struct dc_trim_data dc_trim;
	bool apply_n12db_gain;
	int hp_plugged;

	/* hp impedance */
	int hp_impedance;
	int hp_current_calibrate_val;

	int mtkaif_protocol;

	struct dentry *debugfs;
	unsigned int debug_flag;

	/* vow control */
	int vow_enable;
	int reg_afe_vow_cfg0;
	int reg_afe_vow_cfg1;
	int reg_afe_vow_cfg2;
	int reg_afe_vow_cfg3;
	int reg_afe_vow_cfg4;
	int reg_afe_vow_cfg5;
	int reg_afe_vow_periodic;
	/* vow dmic low power mode, 1: enable, 0: disable */
	int vow_dmic_lp;
};

/* static function declaration */
static int mt6358_print_register(struct mt6358_priv *priv)
{
	unsigned int value = 0;

	regmap_read(priv->regmap, MT6358_AUD_TOP_ID, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_REV0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_DBI, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_DXI, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_DXI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKPDN_TPM0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CKPDN_TPM0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKPDN_TPM1, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CKPDN_TPM1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKPDN_CON0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CKPDN_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKPDN_CON0_SET, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CKPDN_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKPDN_CON0_CLR, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CKPDN_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKSEL_CON0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CKSEL_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKSEL_CON0_SET, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CKSEL_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKSEL_CON0_CLR, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CKSEL_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKTST_CON0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CKTST_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CLK_HWEN_CON0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CLK_HWEN_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CLK_HWEN_CON0_SET, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CLK_HWEN_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CLK_HWEN_CON0_CLR, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_CLK_HWEN_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_RST_CON0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_RST_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_RST_CON0_SET, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_RST_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_RST_CON0_CLR, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_RST_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_RST_BANK_CON0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_RST_BANK_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_CON0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_INT_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_CON0_SET, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_INT_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_CON0_CLR, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_INT_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_MASK_CON0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_INT_MASK_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_MASK_CON0_SET, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_INT_MASK_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_MASK_CON0_CLR, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_INT_MASK_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_STATUS0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_INT_STATUS0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_RAW_STATUS0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_INT_RAW_STATUS0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_MISC_CON0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_INT_MISC_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDNCP_CLKDIV_CON0, &value);
	dev_info(priv->dev, "MT6358_AUDNCP_CLKDIV_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDNCP_CLKDIV_CON1, &value);
	dev_info(priv->dev, "MT6358_AUDNCP_CLKDIV_CON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDNCP_CLKDIV_CON2, &value);
	dev_info(priv->dev, "MT6358_AUDNCP_CLKDIV_CON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, &value);
	dev_info(priv->dev, "MT6358_AUDNCP_CLKDIV_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDNCP_CLKDIV_CON4, &value);
	dev_info(priv->dev, "MT6358_AUDNCP_CLKDIV_CON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_MON_CON0, &value);
	dev_info(priv->dev, "MT6358_AUD_TOP_MON_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_DSN_ID, &value);
	dev_info(priv->dev, "MT6358_AUDIO_DIG_DSN_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_DSN_REV0, &value);
	dev_info(priv->dev, "MT6358_AUDIO_DIG_DSN_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_DSN_DBI, &value);
	dev_info(priv->dev, "MT6358_AUDIO_DIG_DSN_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_DSN_DXI, &value);
	dev_info(priv->dev, "MT6358_AUDIO_DIG_DSN_DXI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_UL_DL_CON0, &value);
	dev_info(priv->dev, "MT6358_AFE_UL_DL_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_DL_SRC2_CON0_L, &value);
	dev_info(priv->dev, "MT6358_AFE_DL_SRC2_CON0_L = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_UL_SRC_CON0_H, &value);
	dev_info(priv->dev, "MT6358_AFE_UL_SRC_CON0_H = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_UL_SRC_CON0_L, &value);
	dev_info(priv->dev, "MT6358_AFE_UL_SRC_CON0_L = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_TOP_CON0, &value);
	dev_info(priv->dev, "MT6358_AFE_TOP_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_TOP_CON0, &value);
	dev_info(priv->dev, "MT6358_AUDIO_TOP_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_MON_DEBUG0, &value);
	dev_info(priv->dev, "MT6358_AFE_MON_DEBUG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON0, &value);
	dev_info(priv->dev, "MT6358_AFUNC_AUD_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON1, &value);
	dev_info(priv->dev, "MT6358_AFUNC_AUD_CON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON2, &value);
	dev_info(priv->dev, "MT6358_AFUNC_AUD_CON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON3, &value);
	dev_info(priv->dev, "MT6358_AFUNC_AUD_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON4, &value);
	dev_info(priv->dev, "MT6358_AFUNC_AUD_CON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON5, &value);
	dev_info(priv->dev, "MT6358_AFUNC_AUD_CON5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON6, &value);
	dev_info(priv->dev, "MT6358_AFUNC_AUD_CON6 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_MON0, &value);
	dev_info(priv->dev, "MT6358_AFUNC_AUD_MON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDRC_TUNE_MON0, &value);
	dev_info(priv->dev, "MT6358_AUDRC_TUNE_MON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_FIFO_CFG0, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_FIFO_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_FIFO_LOG_MON1, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_FIFO_LOG_MON1 = 0x%x\n",
		 value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_MON0, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_MON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_MON1, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_MON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_MON2, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_MON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_MON3, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_MON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_CFG0, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_RX_CFG0, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_RX_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_RX_CFG1, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_RX_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_RX_CFG2, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_RX_CFG2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_RX_CFG3, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_RX_CFG3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_TX_CFG1, &value);
	dev_info(priv->dev, "MT6358_AFE_ADDA_MTKAIF_TX_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_SGEN_CFG0, &value);
	dev_info(priv->dev, "MT6358_AFE_SGEN_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_SGEN_CFG1, &value);
	dev_info(priv->dev, "MT6358_AFE_SGEN_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADC_ASYNC_FIFO_CFG, &value);
	dev_info(priv->dev, "MT6358_AFE_ADC_ASYNC_FIFO_CFG = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_DCCLK_CFG0, &value);
	dev_info(priv->dev, "MT6358_AFE_DCCLK_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_DCCLK_CFG1, &value);
	dev_info(priv->dev, "MT6358_AFE_DCCLK_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_CFG, &value);
	dev_info(priv->dev, "MT6358_AUDIO_DIG_CFG = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_AUD_PAD_TOP, &value);
	dev_info(priv->dev, "MT6358_AFE_AUD_PAD_TOP = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_AUD_PAD_TOP_MON, &value);
	dev_info(priv->dev, "MT6358_AFE_AUD_PAD_TOP_MON = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_AUD_PAD_TOP_MON1, &value);
	dev_info(priv->dev, "MT6358_AFE_AUD_PAD_TOP_MON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_DL_NLE_CFG, &value);
	dev_info(priv->dev, "MT6358_AFE_DL_NLE_CFG = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_DL_NLE_MON, &value);
	dev_info(priv->dev, "MT6358_AFE_DL_NLE_MON = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_CG_EN_MON, &value);
	dev_info(priv->dev, "MT6358_AFE_CG_EN_MON = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_2ND_DSN_ID, &value);
	dev_info(priv->dev, "MT6358_AUDIO_DIG_2ND_DSN_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_2ND_DSN_REV0, &value);
	dev_info(priv->dev, "MT6358_AUDIO_DIG_2ND_DSN_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_2ND_DSN_DBI, &value);
	dev_info(priv->dev, "MT6358_AUDIO_DIG_2ND_DSN_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_2ND_DSN_DXI, &value);
	dev_info(priv->dev, "MT6358_AUDIO_DIG_2ND_DSN_DXI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_TOP, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_TOP = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG0, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG1, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG2, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_CFG2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG3, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_CFG3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG4, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_CFG4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG5, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_CFG5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG6, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_CFG6 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON0, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_MON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON1, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_MON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON2, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_MON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON3, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_MON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON4, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_MON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON5, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_MON5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_SN_INI_CFG, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_SN_INI_CFG = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_TGEN_CFG0, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_TGEN_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_POSDIV_CFG0, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_POSDIV_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_HPF_CFG0, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_HPF_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG0, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG1, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG2, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG3, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG4, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG5, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG6, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG6 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG7, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG7 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG8, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG8 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG9, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG9 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG10, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG10 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG11, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG11 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG12, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG12 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG13, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG13 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG14, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG14 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG15, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG15 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG16, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG16 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG17, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG17 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG18, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG18 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG19, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG19 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG20, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG20 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG21, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG21 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG22, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG22 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG23, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_CFG23 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_MON0, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_MON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_MON1, &value);
	dev_info(priv->dev, "MT6358_AFE_VOW_PERIODIC_MON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_DSN_ID, &value);
	dev_info(priv->dev, "MT6358_AUDENC_DSN_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_DSN_REV0, &value);
	dev_info(priv->dev, "MT6358_AUDENC_DSN_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_DSN_DBI, &value);
	dev_info(priv->dev, "MT6358_AUDENC_DSN_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_DSN_FPI, &value);
	dev_info(priv->dev, "MT6358_AUDENC_DSN_FPI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON0, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON1, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON2, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON3, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON4, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON5, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON6, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON6 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON7, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON7 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON8, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON8 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON9, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON9 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON10, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON10 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON11, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON11 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON12, &value);
	dev_info(priv->dev, "MT6358_AUDENC_ANA_CON12 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_DSN_ID, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_DSN_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_DSN_REV0, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_DSN_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_DSN_DBI, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_DSN_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_DSN_FPI, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_DSN_FPI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON0, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON1, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON2, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON3, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON4, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON5, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON6, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON6 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON7, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON7 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON8, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON8 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON9, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON9 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON10, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON10 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON11, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON11 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON12, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON12 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON13, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON13 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON14, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON14 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON15, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ANA_CON15 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_NUM, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ELR_NUM = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &value);
	dev_info(priv->dev, "MT6358_AUDDEC_ELR_0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDZCD_DSN_ID, &value);
	dev_info(priv->dev, "MT6358_AUDZCD_DSN_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDZCD_DSN_REV0, &value);
	dev_info(priv->dev, "MT6358_AUDZCD_DSN_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDZCD_DSN_DBI, &value);
	dev_info(priv->dev, "MT6358_AUDZCD_DSN_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDZCD_DSN_FPI, &value);
	dev_info(priv->dev, "MT6358_AUDZCD_DSN_FPI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON0, &value);
	dev_info(priv->dev, "MT6358_ZCD_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON1, &value);
	dev_info(priv->dev, "MT6358_ZCD_CON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON2, &value);
	dev_info(priv->dev, "MT6358_ZCD_CON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON3, &value);
	dev_info(priv->dev, "MT6358_ZCD_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON4, &value);
	dev_info(priv->dev, "MT6358_ZCD_CON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON5, &value);
	dev_info(priv->dev, "MT6358_ZCD_CON5 = 0x%x\n", value);

	regmap_read(priv->regmap, MT6358_DRV_CON3, &value);
	dev_info(priv->dev, "MT6358_DRV_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_GPIO_DIR0, &value);
	dev_info(priv->dev, "MT6358_GPIO_DIR0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_GPIO_MODE2, &value);
	dev_info(priv->dev, "MT6358_GPIO_MODE2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_GPIO_MODE3, &value);
	dev_info(priv->dev, "MT6358_GPIO_MODE3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_TOP_CKPDN_CON0, &value);
	dev_info(priv->dev, "MT6358_TOP_CKPDN_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_TOP_CKHWEN_CON0, &value);
	dev_info(priv->dev, "MT6358_TOP_CKHWEN_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_DCXO_CW13, &value);
	dev_info(priv->dev, "MT6358_DCXO_CW13 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_DCXO_CW14, &value);
	dev_info(priv->dev, "MT6358_DCXO_CW14 = 0x%x\n", value);

	return 0;
}

#ifndef ANALOG_HPTRIM
static int apply_dc_compensation(struct mt6358_priv *priv, bool enable);
#endif
static int dc_trim_thread(void *arg);

int mt6358_set_codec_ops(struct snd_soc_component *cmpnt,
			 struct mt6358_codec_ops *ops)
{
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	priv->ops.enable_dc_compensation = ops->enable_dc_compensation;
	priv->ops.set_lch_dc_compensation = ops->set_lch_dc_compensation;
	priv->ops.set_rch_dc_compensation = ops->set_rch_dc_compensation;
	priv->ops.adda_dl_gain_control = ops->adda_dl_gain_control;
	return 0;
}

int mt6358_set_mtkaif_protocol(struct snd_soc_component *cmpnt,
			       int mtkaif_protocol)
{
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	priv->mtkaif_protocol = mtkaif_protocol;
	return 0;
}

static void playback_gpio_set(struct mt6358_priv *priv)
{
	/* set gpio mosi mode */
	regmap_update_bits(priv->regmap, MT6358_GPIO_MODE2_CLR,
			   0x01f8, 0x01f8);
	regmap_update_bits(priv->regmap, MT6358_GPIO_MODE2_SET,
			   0xffff, 0x0249);
	regmap_update_bits(priv->regmap, MT6358_GPIO_MODE2,
			   0xffff, 0x0249);
	/* reset GPIO SMT mode */
	regmap_update_bits(priv->regmap, MT6358_SMT_CON1,
			   0x0ff0, 0x0ff0);
}

static void playback_gpio_reset(struct mt6358_priv *priv)
{
	/* set pad_aud_*_mosi to GPIO mode and dir input
	 * reason:
	 * pad_aud_dat_mosi*, because the pin is used as boot strap
	 * don't clean clk/sync, for mtkaif protocol 2
	 */
	regmap_update_bits(priv->regmap, MT6358_GPIO_MODE2_CLR,
			   0x01f8, 0x01f8);
	regmap_update_bits(priv->regmap, MT6358_GPIO_MODE2,
			   0x01f8, 0x0000);
	regmap_update_bits(priv->regmap, MT6358_GPIO_DIR0,
			   0xf << 8, 0x0);
	/* reset GPIO SMT mode */
	regmap_update_bits(priv->regmap, MT6358_SMT_CON1,
			   0x0ff0, 0x0000);
}

static void capture_gpio_set(struct mt6358_priv *priv)
{
	/* set gpio miso mode */
	regmap_update_bits(priv->regmap, MT6358_GPIO_MODE3_CLR,
			   0xffff, 0xffff);
	regmap_update_bits(priv->regmap, MT6358_GPIO_MODE3_SET,
			   0xffff, 0x0249);
	regmap_update_bits(priv->regmap, MT6358_GPIO_MODE3,
			   0xffff, 0x0249);
}

static void capture_gpio_reset(struct mt6358_priv *priv)
{
	/* set pad_aud_*_miso to GPIO mode and dir input
	 * reason:
	 * pad_aud_clk_miso, because when playback only the miso_clk
	 * will also have 26m, so will have power leak
	 * pad_aud_dat_miso*, because the pin is used as boot strap
	 */
	regmap_update_bits(priv->regmap, MT6358_GPIO_MODE3_CLR,
			   0xffff, 0xffff);
	regmap_update_bits(priv->regmap, MT6358_GPIO_MODE3,
			   0xffff, 0x0000);
	regmap_update_bits(priv->regmap, MT6358_GPIO_DIR0,
			   0xf << 12, 0x0);
}

/* use only when not govern by DAPM */
static int mt6358_set_dcxo(struct mt6358_priv *priv, bool enable)
{
	regmap_update_bits(priv->regmap, MT6358_DCXO_CW14,
			   0x1 << RG_XO_AUDIO_EN_M_SFT,
			   (enable ? 1 : 0) << RG_XO_AUDIO_EN_M_SFT);
	return 0;
}

/* use only when not govern by DAPM */
static int mt6358_set_clksq(struct mt6358_priv *priv, bool enable)
{
	/* audio clk source from internal dcxo */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON6,
			   RG_CLKSQ_IN_SEL_TEST_MASK_SFT,
			   0x0);

	/* Enable/disable CLKSQ 26MHz */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON6,
			   RG_CLKSQ_EN_MASK_SFT,
			   (enable ? 1 : 0) << RG_CLKSQ_EN_SFT);
	return 0;
}

/* use only when not govern by DAPM */
static int mt6358_set_aud_global_bias(struct mt6358_priv *priv, bool enable)
{
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13,
			   RG_AUDGLB_PWRDN_VA28_MASK_SFT,
			   (enable ? 0 : 1) << RG_AUDGLB_PWRDN_VA28_SFT);
	return 0;
}

/* use only when not govern by DAPM */
static int mt6358_set_topck(struct mt6358_priv *priv, bool enable)
{
	regmap_update_bits(priv->regmap, MT6358_AUD_TOP_CKPDN_CON0,
			   0x0066, enable ? 0x0 : 0x66);
	return 0;
}

static int mt6358_mtkaif_tx_enable(struct mt6358_priv *priv)
{
	switch (priv->mtkaif_protocol) {
	case MT6358_MTKAIF_PROTOCOL_2_CLK_P2:
		/* MTKAIF TX format setting */
		regmap_update_bits(priv->regmap,
				   MT6358_AFE_ADDA_MTKAIF_CFG0,
				   0xffff, 0x0010);
		/* enable aud_pad TX fifos */
		regmap_update_bits(priv->regmap,
				   MT6358_AFE_AUD_PAD_TOP,
				   0xff00, 0x3800);
		regmap_update_bits(priv->regmap,
				   MT6358_AFE_AUD_PAD_TOP,
				   0xff00, 0x3900);
		break;
	case MT6358_MTKAIF_PROTOCOL_2:
		/* MTKAIF TX format setting */
		regmap_update_bits(priv->regmap,
				   MT6358_AFE_ADDA_MTKAIF_CFG0,
				   0xffff, 0x0010);
		/* enable aud_pad TX fifos */
		regmap_update_bits(priv->regmap,
				   MT6358_AFE_AUD_PAD_TOP,
				   0xff00, 0x3100);
		break;
	case MT6358_MTKAIF_PROTOCOL_1:
	default:
		/* MTKAIF TX format setting */
		regmap_update_bits(priv->regmap,
				   MT6358_AFE_ADDA_MTKAIF_CFG0,
				   0xffff, 0x0000);
		/* enable aud_pad TX fifos */
		regmap_update_bits(priv->regmap,
				   MT6358_AFE_AUD_PAD_TOP,
				   0xff00, 0x3100);
		break;
	}
	return 0;
}

static int mt6358_mtkaif_tx_disable(struct mt6358_priv *priv)
{
	/* disable aud_pad TX fifos */
	regmap_update_bits(priv->regmap, MT6358_AFE_AUD_PAD_TOP,
			   0xff00, 0x3000);
	return 0;
}

int mt6358_mtkaif_calibration_enable(struct snd_soc_component *cmpnt)
{
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	playback_gpio_set(priv);
	capture_gpio_set(priv);
	mt6358_mtkaif_tx_enable(priv);

	mt6358_set_dcxo(priv, true);
	mt6358_set_aud_global_bias(priv, true);
	mt6358_set_clksq(priv, true);
	mt6358_set_topck(priv, true);

	/* set dat_miso_loopback on */
	regmap_update_bits(priv->regmap, MT6358_AUDIO_DIG_CFG,
			   RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_MASK_SFT,
			   1 << RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_SFT);
	regmap_update_bits(priv->regmap, MT6358_AUDIO_DIG_CFG,
			   RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_MASK_SFT,
			   1 << RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_SFT);
	return 0;
}

int mt6358_mtkaif_calibration_disable(struct snd_soc_component *cmpnt)
{
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	/* set dat_miso_loopback off */
	regmap_update_bits(priv->regmap, MT6358_AUDIO_DIG_CFG,
			   RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_MASK_SFT,
			   0 << RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_SFT);
	regmap_update_bits(priv->regmap, MT6358_AUDIO_DIG_CFG,
			   RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_MASK_SFT,
			   0 << RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_SFT);

	mt6358_set_topck(priv, false);
	mt6358_set_clksq(priv, false);
	mt6358_set_aud_global_bias(priv, false);
	mt6358_set_dcxo(priv, false);

	mt6358_mtkaif_tx_disable(priv);
	playback_gpio_reset(priv);
	capture_gpio_reset(priv);
	return 0;
}

int mt6358_set_mtkaif_calibration_phase(struct snd_soc_component *cmpnt,
					int phase_1, int phase_2)
{
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	regmap_update_bits(priv->regmap, MT6358_AUDIO_DIG_CFG,
			   RG_AUD_PAD_TOP_PHASE_MODE_MASK_SFT,
			   phase_1 << RG_AUD_PAD_TOP_PHASE_MODE_SFT);
	regmap_update_bits(priv->regmap, MT6358_AUDIO_DIG_CFG,
			   RG_AUD_PAD_TOP_PHASE_MODE2_MASK_SFT,
			   phase_2 << RG_AUD_PAD_TOP_PHASE_MODE2_SFT);
	return 0;
}

static int get_auxadc_audio(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	return pmic_get_auxadc_value(AUXADC_LIST_HPOFS_CAL);
#else
	return 1;
#endif
}

static int get_accdet_auxadc(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	return pmic_get_auxadc_value(AUXADC_LIST_ACCDET);
#else
	return 1;
#endif
}

/* dl pga gain */
enum {
	DL_GAIN_8DB = 0,
	DL_GAIN_0DB = 8,
	DL_GAIN_N_1DB = 9,
	DL_GAIN_N_10DB = 18,
	DL_GAIN_N_40DB = 0x1f,
};
#define DL_GAIN_N_10DB_REG (DL_GAIN_N_10DB << 7 | DL_GAIN_N_10DB)
#define DL_GAIN_N_40DB_REG (DL_GAIN_N_40DB << 7 | DL_GAIN_N_40DB)
#define DL_GAIN_REG_MASK 0x0f9f

/* reg idx for -40dB*/
#define PGA_MINUS_40_DB_REG_VAL 0x1f
#define HP_PGA_MINUS_40_DB_REG_VAL 0x3f
static const char *const dl_pga_gain[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db",
	"3Db", "2Db", "1Db", "0Db", "-1Db",
	"-2Db", "-3Db",	"-4Db", "-5Db", "-6Db",
	"-7Db", "-8Db", "-9Db", "-10Db", "-40Db"
};

static void zcd_enable(struct mt6358_priv *priv, bool enable, int device)
{
	if (enable) {
		switch (device) {
		case DEVICE_RCV:
			regmap_update_bits(priv->regmap,
					   MT6358_AUDDEC_ANA_CON11,
					   0x7, 0x2);
			break;
		case DEVICE_LO:
			regmap_update_bits(priv->regmap,
					   MT6358_AUDDEC_ANA_CON11,
					   0x7, 0x0);
			break;
		case DEVICE_HP:
		default:
			regmap_update_bits(priv->regmap,
					   MT6358_AUDDEC_ANA_CON11,
					   0x7, 0x1);
			break;
		}
		/* Enable ZCD, for minimize pop noise */
		/* when adjust gain during HP buffer on */
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON0,
				   0x7 << 8, 0x1 << 8);
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON0,
				   0x1 << 7, 0x0 << 7);
		/* timeout, 1 = 5ms, 0 = 30ms */
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON0,
				   0x1 << 6, 0x0 << 6);
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON0,
				   0x3 << 4, 0x0 << 4);
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON0,
				   0x7 << 1, 0x5 << 1);
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON0,
				   0x1 << 0, 0x1 << 0);
	} else {
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON11,
				   0x7, 0x4);
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON0,
				   0xffff, 0x0000);
	}
}

static void hp_main_output_ramp(struct mt6358_priv *priv, bool up)
{
	int i = 0, stage = 0;
	int target = 7;

	/* Enable/Reduce HPL/R main output stage step by step */
	for (i = 0; i <= target; i++) {
		stage = up ? i : target - i;
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
				   0x7 << 8, stage << 8);
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
				   0x7 << 11, stage << 11);
		udelay(600);
	}
}

static void hp_aux_feedback_loop_gain_ramp(struct mt6358_priv *priv, bool up)
{
	int i = 0, stage = 0;

	/* Reduce HP aux feedback loop gain step by step */
	for (i = 0; i <= 0xf; i++) {
		stage = up ? i : 0xf - i;
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
				   0xf << 12, stage << 12);
		udelay(600);
	}
}

static void hp_pull_down(struct mt6358_priv *priv, bool enable)
{
	int i;

	if (enable) {
		for (i = 0x0; i <= 0x6; i++) {
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
					   0x7, i);
			udelay(600);
		}
	} else {
		for (i = 0x6; i >= 0x0; i--) {
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
					   0x7, i);
			udelay(600);
		}
	}
}

static bool is_valid_hp_pga_idx(int reg_idx)
{
	return (reg_idx >= DL_GAIN_8DB && reg_idx <= DL_GAIN_N_10DB) ||
	       reg_idx == DL_GAIN_N_40DB;
}

static void headset_volume_ramp(struct mt6358_priv *priv,
				int from, int to)
{
	int offset = 0, count = 1, reg_idx;

	if (!is_valid_hp_pga_idx(from) || !is_valid_hp_pga_idx(to))
		dev_warn(priv->dev, "%s(), volume index is not valid, from %d, to %d\n",
			 __func__, from, to);


	dev_info(priv->dev, "%s(), from %d, to %d\n",
		 __func__, from, to);

	if (to > from)
		offset = to - from;
	else
		offset = from - to;

	while (offset > 0) {
		if (to > from)
			reg_idx = from + count;
		else
			reg_idx = from - count;

		if (is_valid_hp_pga_idx(reg_idx)) {
			regmap_update_bits(priv->regmap,
					   MT6358_ZCD_CON2,
					   DL_GAIN_REG_MASK,
					   (reg_idx << 7) | reg_idx);
			usleep_range(200, 300);
		}
		offset--;
		count++;
	}
}

static int dl_pga_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int id = kcontrol->id.device;
	int array_size, reg_minus_40db;

	array_size = ARRAY_SIZE(dl_pga_gain);
	reg_minus_40db = PGA_MINUS_40_DB_REG_VAL;

	ucontrol->value.integer.value[0] = priv->ana_gain[id];

	if (ucontrol->value.integer.value[0] == reg_minus_40db)
		ucontrol->value.integer.value[0] = array_size - 1;

	return 0;
}

static int dl_pga_set(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	int index = ucontrol->value.integer.value[0];
	unsigned int id = kcontrol->id.device;
	int array_size, reg_minus_40db;

	dev_info(priv->dev, "%s(), id %d, index %d\n", __func__, id, index);

	array_size = ARRAY_SIZE(dl_pga_gain);
	reg_minus_40db = PGA_MINUS_40_DB_REG_VAL;

	if (index >= array_size) {
		dev_warn(priv->dev, "return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (array_size - 1))
		index = reg_minus_40db;	/* reg idx for -40dB*/

	switch (id) {
	case AUDIO_ANALOG_VOLUME_HPOUTL:
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON2,
				   RG_AUDHPLGAIN_MASK_SFT,
				   index << RG_AUDHPLGAIN_SFT);
		break;
	case AUDIO_ANALOG_VOLUME_HPOUTR:
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON2,
				   RG_AUDHPRGAIN_MASK_SFT,
				   index << RG_AUDHPRGAIN_SFT);
		break;
	case AUDIO_ANALOG_VOLUME_HSOUTL:
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON3,
				   RG_AUDHSGAIN_MASK_SFT,
				   index << RG_AUDHSGAIN_SFT);
		break;
	case AUDIO_ANALOG_VOLUME_LINEOUTL:
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON1,
				   RG_AUDLOLGAIN_MASK_SFT,
				   index << RG_AUDLOLGAIN_SFT);
		break;
	case AUDIO_ANALOG_VOLUME_LINEOUTR:
		regmap_update_bits(priv->regmap, MT6358_ZCD_CON1,
				   RG_AUDLORGAIN_MASK_SFT,
				   index << RG_AUDLORGAIN_SFT);
		break;
	default:
		return 0;
	}

	priv->ana_gain[id] = index;
	return 0;
}

static const struct soc_enum dl_pga_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dl_pga_gain), dl_pga_gain),
};

#define MT_SOC_ENUM_EXT_ID(xname, xenum, xhandler_get, xhandler_put, id) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .device = id,\
	.info = snd_soc_info_enum_double, \
	.get = xhandler_get, .put = xhandler_put, \
	.private_value = (unsigned long)&xenum }

static const struct snd_kcontrol_new mt6358_snd_controls[] = {
	MT_SOC_ENUM_EXT_ID("Headset_PGAL_GAIN", dl_pga_enum[0],
			   dl_pga_get, dl_pga_set,
			   AUDIO_ANALOG_VOLUME_HPOUTL),
	MT_SOC_ENUM_EXT_ID("Headset_PGAR_GAIN", dl_pga_enum[0],
			   dl_pga_get, dl_pga_set,
			   AUDIO_ANALOG_VOLUME_HPOUTR),
	MT_SOC_ENUM_EXT_ID("Handset_PGA_GAIN", dl_pga_enum[0],
			   dl_pga_get, dl_pga_set,
			   AUDIO_ANALOG_VOLUME_HSOUTL),
	MT_SOC_ENUM_EXT_ID("Lineout_PGAL_GAIN", dl_pga_enum[0],
			   dl_pga_get, dl_pga_set,
			   AUDIO_ANALOG_VOLUME_LINEOUTL),
	MT_SOC_ENUM_EXT_ID("Lineout_PGAR_GAIN", dl_pga_enum[0],
			   dl_pga_get, dl_pga_set,
			   AUDIO_ANALOG_VOLUME_LINEOUTR),
};

/* ul pga gain */
static const char *const ul_pga_gain[] = {
	"0Db", "6Db", "12Db", "18Db", "24Db", "30Db"
};

static const struct soc_enum ul_pga_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ul_pga_gain), ul_pga_gain),
};

static int ul_pga_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	ucontrol->value.integer.value[0] =
		priv->ana_gain[kcontrol->id.device];
	return 0;
}

static int ul_pga_set(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	int index = ucontrol->value.integer.value[0];
	unsigned int id = kcontrol->id.device;

	dev_info(priv->dev, "%s(), id %d, index %d\n", __func__, id, index);
	if (index > ARRAY_SIZE(ul_pga_gain)) {
		dev_warn(priv->dev, "return -EINVAL\n");
		return -EINVAL;
	}

	switch (id) {
	case AUDIO_ANALOG_VOLUME_MICAMP1:
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDPREAMPLGAIN_MASK_SFT,
				   index << RG_AUDPREAMPLGAIN_SFT);
		break;
	case AUDIO_ANALOG_VOLUME_MICAMP2:
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   RG_AUDPREAMPRGAIN_MASK_SFT,
				   index << RG_AUDPREAMPRGAIN_SFT);
		break;
	default:
		return 0;
	}

	priv->ana_gain[id] = index;
	return 0;
}

static const struct snd_kcontrol_new mt6358_snd_ul_controls[] = {
	MT_SOC_ENUM_EXT_ID("Audio_PGA1_Setting", ul_pga_enum[0],
			   ul_pga_get, ul_pga_set,
			   AUDIO_ANALOG_VOLUME_MICAMP1),
	MT_SOC_ENUM_EXT_ID("Audio_PGA2_Setting", ul_pga_enum[0],
			   ul_pga_get, ul_pga_set,
			   AUDIO_ANALOG_VOLUME_MICAMP2),
};

/* MUX */

/* LOL MUX */
enum {
	LOL_MUX_OPEN = 0,
	LOL_MUX_MUTE,
	LOL_MUX_PLAYBACK,
	LOL_MUX_TEST_MODE,
	LOL_MUX_MASK = 0x3,
};

static const char * const lo_in_mux_map[] = {
	"Open", "Mute", "Playback", "Test Mode"
};

static int lo_in_mux_map_value[] = {
	LOL_MUX_OPEN,
	LOL_MUX_MUTE,
	LOL_MUX_PLAYBACK,
	LOL_MUX_TEST_MODE,
};

static SOC_VALUE_ENUM_SINGLE_DECL(lo_in_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  LOL_MUX_MASK,
				  lo_in_mux_map,
				  lo_in_mux_map_value);

static const struct snd_kcontrol_new lo_in_mux_control =
	SOC_DAPM_ENUM("In Select", lo_in_mux_map_enum);

/*HP MUX */
enum {
	HP_MUX_OPEN = 0,
	HP_MUX_HPSPK,
	HP_MUX_HP,
	HP_MUX_TEST_MODE,
	HP_MUX_HP_IMPEDANCE,
	HP_MUX_HP_DUALSPK,
	HP_MUX_MASK = 0x7,
};

static const char * const hp_in_mux_map[] = {
	"Open",
	"LoudSPK Playback",
	"Audio Playback",
	"Test Mode",
	"HP Impedance",
	"Loud DualSPK Playback",
	"undefined2",
	"undefined3",
};

static int hp_in_mux_map_value[] = {
	HP_MUX_OPEN,
	HP_MUX_HPSPK,
	HP_MUX_HP,
	HP_MUX_TEST_MODE,
	HP_MUX_HP_IMPEDANCE,
	HP_MUX_HP_DUALSPK,
	HP_MUX_OPEN,
	HP_MUX_OPEN,
};

static SOC_VALUE_ENUM_SINGLE_DECL(hpl_in_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  HP_MUX_MASK,
				  hp_in_mux_map,
				  hp_in_mux_map_value);

static const struct snd_kcontrol_new hpl_in_mux_control =
	SOC_DAPM_ENUM("HPL Select", hpl_in_mux_map_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(hpr_in_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  HP_MUX_MASK,
				  hp_in_mux_map,
				  hp_in_mux_map_value);

static const struct snd_kcontrol_new hpr_in_mux_control =
	SOC_DAPM_ENUM("HPR Select", hpr_in_mux_map_enum);

/* RCV MUX */
enum {
	RCV_MUX_OPEN = 0,
	RCV_MUX_MUTE,
	RCV_MUX_VOICE_PLAYBACK,
	RCV_MUX_TEST_MODE,
	RCV_MUX_MASK = 0x3,
};

static const char * const rcv_in_mux_map[] = {
	"Open", "Mute", "Voice Playback", "Test Mode"
};

static int rcv_in_mux_map_value[] = {
	RCV_MUX_OPEN,
	RCV_MUX_MUTE,
	RCV_MUX_VOICE_PLAYBACK,
	RCV_MUX_TEST_MODE,
};

static SOC_VALUE_ENUM_SINGLE_DECL(rcv_in_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  RCV_MUX_MASK,
				  rcv_in_mux_map,
				  rcv_in_mux_map_value);

static const struct snd_kcontrol_new rcv_in_mux_control =
	SOC_DAPM_ENUM("RCV Select", rcv_in_mux_map_enum);

/* DAC In MUX */
static const char * const dac_in_mux_map[] = {
	"Normal Path", "Sgen"
};

static int dac_in_mux_map_value[] = {
	0x0, 0x1,
};

static SOC_VALUE_ENUM_SINGLE_DECL(dac_in_mux_map_enum,
				  MT6358_AFE_TOP_CON0,
				  DL_SINE_ON_SFT,
				  DL_SINE_ON_MASK,
				  dac_in_mux_map,
				  dac_in_mux_map_value);

static const struct snd_kcontrol_new dac_in_mux_control =
	SOC_DAPM_ENUM("DAC Select", dac_in_mux_map_enum);

/* AIF Out MUX */
static SOC_VALUE_ENUM_SINGLE_DECL(aif_out_mux_map_enum,
				  MT6358_AFE_TOP_CON0,
				  UL_SINE_ON_SFT,
				  UL_SINE_ON_MASK,
				  dac_in_mux_map,
				  dac_in_mux_map_value);

static const struct snd_kcontrol_new aif_out_mux_control =
	SOC_DAPM_ENUM("AIF Out Select", aif_out_mux_map_enum);

/* Mic Type MUX */
enum {
	MIC_TYPE_MUX_IDLE = 0,
	MIC_TYPE_MUX_ACC,
	MIC_TYPE_MUX_DMIC,
	MIC_TYPE_MUX_DCC,
	MIC_TYPE_MUX_DCC_ECM_DIFF,
	MIC_TYPE_MUX_DCC_ECM_SINGLE,
	MIC_TYPE_MUX_MASK = 0xf,
};

#define IS_DCC_BASE(x) (x == MIC_TYPE_MUX_DCC || \
			x == MIC_TYPE_MUX_DCC_ECM_DIFF || \
			x == MIC_TYPE_MUX_DCC_ECM_SINGLE)


#define IS_AMIC_BASE(x) (x == MIC_TYPE_MUX_ACC || IS_DCC_BASE(x))

static const char * const mic_type_mux_map[] = {
	"Idle",
	"ACC",
	"DMIC",
	"DCC",
	"DCC_ECM_DIFF",
	"DCC_ECM_SINGLE",
};

static int mic_type_mux_map_value[] = {
	MIC_TYPE_MUX_IDLE,
	MIC_TYPE_MUX_ACC,
	MIC_TYPE_MUX_DMIC,
	MIC_TYPE_MUX_DCC,
	MIC_TYPE_MUX_DCC_ECM_DIFF,
	MIC_TYPE_MUX_DCC_ECM_SINGLE,
};

static SOC_VALUE_ENUM_SINGLE_DECL(mic_type_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  MIC_TYPE_MUX_MASK,
				  mic_type_mux_map,
				  mic_type_mux_map_value);

static const struct snd_kcontrol_new mic_type_mux_control =
	SOC_DAPM_ENUM("Mic Type Select", mic_type_mux_map_enum);

/* ADC L MUX */
enum {
	ADC_MUX_IDLE = 0,
	ADC_MUX_AIN0,
	ADC_MUX_PREAMPLIFIER,
	ADC_MUX_IDLE1,
	ADC_MUX_MASK = 0x3,
};

static const char * const adc_left_mux_map[] = {
	"Idle", "AIN0", "Left Preamplifier", "Idle_1"
};

static int adc_mux_map_value[] = {
	ADC_MUX_IDLE,
	ADC_MUX_AIN0,
	ADC_MUX_PREAMPLIFIER,
	ADC_MUX_IDLE1,
};

static SOC_VALUE_ENUM_SINGLE_DECL(adc_left_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  ADC_MUX_MASK,
				  adc_left_mux_map,
				  adc_mux_map_value);

static const struct snd_kcontrol_new adc_left_mux_control =
	SOC_DAPM_ENUM("ADC L Select", adc_left_mux_map_enum);

/* ADC R MUX */
static const char * const adc_right_mux_map[] = {
	"Idle", "AIN0", "Right Preamplifier", "Idle_1"
};

static SOC_VALUE_ENUM_SINGLE_DECL(adc_right_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  ADC_MUX_MASK,
				  adc_right_mux_map,
				  adc_mux_map_value);

static const struct snd_kcontrol_new adc_right_mux_control =
	SOC_DAPM_ENUM("ADC R Select", adc_right_mux_map_enum);

/* PGA L MUX */
enum {
	PGA_MUX_NONE = 0,
	PGA_MUX_AIN0,
	PGA_MUX_AIN1,
	PGA_MUX_AIN2,
	PGA_MUX_MASK = 0x3,
};

static const char * const pga_mux_map[] = {
	"None", "AIN0", "AIN1", "AIN2"
};

static int pga_mux_map_value[] = {
	PGA_MUX_NONE,
	PGA_MUX_AIN0,
	PGA_MUX_AIN1,
	PGA_MUX_AIN2,
};

static SOC_VALUE_ENUM_SINGLE_DECL(pga_left_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  PGA_MUX_MASK,
				  pga_mux_map,
				  pga_mux_map_value);

static const struct snd_kcontrol_new pga_left_mux_control =
	SOC_DAPM_ENUM("PGA L Select", pga_left_mux_map_enum);

/* PGA R MUX */
static SOC_VALUE_ENUM_SINGLE_DECL(pga_right_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  PGA_MUX_MASK,
				  pga_mux_map,
				  pga_mux_map_value);

static const struct snd_kcontrol_new pga_right_mux_control =
	SOC_DAPM_ENUM("PGA R Select", pga_right_mux_map_enum);

static int mt_clksq_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* audio clk source from internal dcxo */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON6,
				   RG_CLKSQ_IN_SEL_TEST_MASK_SFT,
				   0x0);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_sgen_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol,
			 int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* sdm audio fifo clock power on */
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0006);
		/* scrambler clock on enable */
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON0, 0xCBA1);
		/* sdm power on */
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0003);
		/* sdm fifo enable */
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x000B);

		regmap_update_bits(priv->regmap, MT6358_AFE_SGEN_CFG0,
				   0xff3f,
				   0x0000);
		regmap_update_bits(priv->regmap, MT6358_AFE_SGEN_CFG1,
				   0xffff,
				   0x0001);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* DL scrambler disabling sequence */
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0000);
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON0, 0xcba0);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_aif_in_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol,
			   int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event 0x%x, rate %d\n",
		 __func__, event, priv->dl_rate);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		playback_gpio_set(priv);

		/* sdm audio fifo clock power on */
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0006);
		/* scrambler clock on enable, invert left channel */
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON0, 0xCFA1);
		/* sdm power on */
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0003);
		/* sdm fifo enable */
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x000B);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* DL scrambler disabling sequence */
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0000);
		regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON0, 0xCFA0);

		playback_gpio_reset(priv);
		break;
	default:
		break;
	}

	return 0;
}

#define MIC_VINP_4POLE_THRES_MV 283
#define VINP_NORMALIZED_TO_MV 1700

#ifdef ANALOG_HPTRIM
#define HPTRIM_L_SHIFT 0
#define HPTRIM_R_SHIFT 4
#define HPFINETRIM_L_SHIFT 8
#define HPFINETRIM_R_SHIFT 10
#define HPTRIM_EN_SHIFT 12
#define HPTRIM_L_MASK (0xf << HPTRIM_L_SHIFT)
#define HPTRIM_R_MASK (0xf << HPTRIM_R_SHIFT)
#define HPFINETRIM_L_MASK (0x3 << HPFINETRIM_L_SHIFT)
#define HPFINETRIM_R_MASK (0x3 << HPFINETRIM_R_SHIFT)
#define HPTRIM_EN_MASK (0x1 << HPTRIM_EN_SHIFT)
#endif

enum {
	HP_INPUT_MUX_OPEN = 0,
	HP_INPUT_MUX_LOL,
	HP_INPUT_MUX_IDACR,
	HP_INPUT_MUX_HS,
};

static void set_hp_l_input_mux(struct mt6358_priv *priv, unsigned int mux)
{
	dev_info(priv->dev, "%s(), mux = %d\n", __func__, mux);
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			RG_AUDHPLMUXINPUTSEL_VAUDP15_MASK_SFT,
			mux << RG_AUDHPLMUXINPUTSEL_VAUDP15_SFT);
}

static void enable_lo_buffer(struct mt6358_priv *priv, bool enable)
{
	dev_info(priv->dev, "%s(), enable = %d\n", __func__, enable);
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON7, 0x0110);
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON7, 0x0112);
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON7, 0x0113);
}

static void set_speaker_gain(struct mt6358_priv *priv, int spk_gain)
{
	dev_info(priv->dev, "%s(), spk_gain = %d\n", __func__, spk_gain);
	regmap_update_bits(priv->regmap, MT6358_ZCD_CON1,
			DL_GAIN_REG_MASK, (spk_gain << 7) | spk_gain);
}

static int mtk_hp_enable(struct mt6358_priv *priv)
{
#ifdef ANALOG_HPTRIM
	unsigned int trim_setting = 0;
	unsigned int reg_value = 0;

	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), mic_vinp_mv = %d, Result AUDDEC_ELR_0 = 0x%x\n",
			__func__, priv->dc_trim.mic_vinp_mv, reg_value);

	if (priv->dc_trim.mic_vinp_mv > MIC_VINP_4POLE_THRES_MV &&
			((priv->debug_flag & DBG_DCTRIM_BYPASS_4POLE) == 0)) {
		trim_setting = priv->dc_trim.hp_4_pole_trim_setting;
		dev_info(priv->dev, "%s(), set 4 pole mic_vinp_mv = %d\n",
				__func__, priv->dc_trim.mic_vinp_mv);
	} else {
		trim_setting = priv->dc_trim.hp_3_pole_trim_setting;
	}

	dev_info(priv->dev, "%s(), trim_setting = %d",
			__func__, trim_setting);

	if (!priv->dc_trim.dc_compensation_disabled) {
		regmap_write(priv->regmap, MT6358_AUDDEC_ELR_0, trim_setting);
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), new AUDDEC_ELR_0 = 0x%x\n",
			 __func__, reg_value);
	} else {
		regmap_write(priv->regmap, MT6358_AUDDEC_ELR_0, 0x0);
		dev_info(priv->dev, "%s(), priv->dc_trim.compensation_disabled = true\n",
			 __func__);
	}
#endif
	dev_info(priv->dev, "+%s()\n", __func__);

	/* Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true);
	/* release HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			   0x1 << 6, 0x1 << 6);

	/* Disable headphone short-circuit protection */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x3000);
	/* Reduce ESD resistance of AU_REFN */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0xc000);
	/* Set HPR/HPL gain as minimum (~ -10dB) */
	regmap_write(priv->regmap, MT6358_ZCD_CON2, DL_GAIN_N_10DB_REG);

	/* Turn on DA_600K_NCP_VA18 */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON1, 0x0001);
	/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON2, 0x002c);
	/* Toggle RG_DIVCKS_CHG */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON0, 0x0001);
	/* Set NCP soft start mode as default mode: 150us */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON4, 0x0002);
	/* Enable NCP */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x0000);
	usleep_range(250, 270);

	/* Enable cap-less LDOs (1.5V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
			   0x1055, 0x1055);
	/* Enable NV regulator (-1.2V) */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x0001);
	usleep_range(100, 120);

	/* Enable AUD_ZCD */
	zcd_enable(priv, true, DEVICE_HP);

	/* Enable IBIST */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);

	/* Set HP DR bias current optimization, 010: 6uA */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON11,
			   0xff80, 0x4900);
	/* Set HP & ZCD bias current optimization */
	/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
	/* Set HPP/N STB enhance circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0xc033);

	/* Enable HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x000c);
	/* Enable HP aux feedback loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x003c);
	/* Enable HP aux CMFB loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0c00);
	/* Enable HP driver bias circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x30c0);
	/* Enable HP driver core circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x30f0);
	/* Short HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			   0x00ff, 0x00fc);

	/* Enable HP main CMFB loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0e00);
	/* Disable HP aux CMFB loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0200);

	/* Select CMFB resistor bulk to AC mode */
	/* Selec HS/LO cap size (6.5pF default) */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON10, 0x0000);

	/* Enable HP main output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x00ff);
	/* Enable HPR/L main output stage step by step */
	hp_main_output_ramp(priv, true);

	/* Reduce HP aux feedback loop gain */
	hp_aux_feedback_loop_gain_ramp(priv, true);
	/* Disable HP aux feedback loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fcf);

	/* apply volume setting */
	headset_volume_ramp(priv,
			    DL_GAIN_N_10DB,
			    priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL]);

	/* Disable HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fc3);
	/* Unshort HP main output to HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3f03);
	usleep_range(100, 120);

	/* Enable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x1);
	/* Enable Audio DAC  */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0xf, 0xf);
	/* Enable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x1, 0x1);
	usleep_range(100, 120);

	/* Switch HPL MUX to audio DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0x0f00, 0x0200);
	/* Switch HPR MUX to audio DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0x0f00, 0x0a00);

	/* disable L ch invert: MT6358_AFUNC_AUC_CON0[10] = 0 */
	regmap_update_bits(priv->regmap, MT6358_AFUNC_AUD_CON0,
			0x0400, 0x0);
#ifndef ANALOG_HPTRIM
	/* Apply digital DC compensation value to DAC */
	apply_dc_compensation(priv, true);
#endif

	/* Disable Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, false);

	return 0;
}

static int mtk_hp_disable(struct mt6358_priv *priv)
{
	dev_info(priv->dev, "+%s()\n", __func__);

	/* Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true);

#ifndef ANALOG_HPTRIM
	apply_dc_compensation(priv, false);
#endif

	/* HPR/HPL mux to open */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   0x0f00, 0x0000);

	/* Disable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			   0x0001, 0x0000);

	/* Disable Audio DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   0x000f, 0x0000);

	/* Disable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x0);

	/* Short HP main output to HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fc3);
	/* Enable HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fcf);

	/* decrease HPL/R gain to normal gain step by step */
	headset_volume_ramp(priv,
			    priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL],
			    DL_GAIN_N_10DB);

	/* Enable HP aux feedback loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fff);

	/* Reduce HP aux feedback loop gain */
	hp_aux_feedback_loop_gain_ramp(priv, false);

	/* decrease HPR/L main output stage step by step */
	hp_main_output_ramp(priv, false);

	/* Disable HP main output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3, 0x0);

	/* Enable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x0f00, 0x0e00);
	/* Disable HP main CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x0f00, 0x0c00);
	/* Unshort HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			   0x3 << 6, 0x0);
	/* Disable HP driver core circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   0x3 << 4, 0x0);
	/* Disable HP driver bias circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   0x3 << 6, 0x0);

	/* Disable HP aux CMFB loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0000);
	/* Disable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			   0x3 << 4, 0x0);
	/* Disable HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x0000);

	/* Disable IBIST */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON12,
			   0x1 << 8, 0x1 << 8);

	/* Disable AUD_ZCD */
	zcd_enable(priv, false, DEVICE_HP);

	/* Disable NV regulator (-1.2V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x1, 0x0);
	/* Disable cap-less LDOs (1.5V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
			   0x1055, 0x0);
	/* Disable NCP */
	regmap_update_bits(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3,
			   0x1, 0x1);

	/* Set HPL/HPR gain to mute */
	regmap_write(priv->regmap, MT6358_ZCD_CON2, DL_GAIN_N_10DB_REG);

	/* Increase ESD resistance of AU_REFN */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON2,
			   0x1 << 14, 0x0);

	/* Set HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			   0x1 << 6, 0x0);
	/* disable Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, false);

	return 0;
}

static int mtk_hp_spk_enable(struct mt6358_priv *priv)
{
	unsigned int trim_setting = 0;
	unsigned int reg_value = 0;
#ifdef ANALOG_HPTRIM
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), current AUDDEC_ELR_0 = 0x%x, mic_vinp_mv = %d\n",
			__func__, reg_value, priv->dc_trim.mic_vinp_mv);

	if (priv->dc_trim.mic_vinp_mv > MIC_VINP_4POLE_THRES_MV &&
		((priv->debug_flag & DBG_DCTRIM_BYPASS_4POLE) == 0)) {
		trim_setting = priv->dc_trim.spk_hp_4_pole_trim_setting;
	} else {
		trim_setting = priv->dc_trim.spk_hp_3_pole_trim_setting;
	}

	if (!priv->dc_trim.dc_compensation_disabled) {
		regmap_write(priv->regmap, MT6358_AUDDEC_ELR_0, trim_setting);
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), new AUDDEC_ELR_0 = 0x%x\n",
			 __func__, reg_value);
	} else {
		regmap_write(priv->regmap, MT6358_AUDDEC_ELR_0, 0x0);
		dev_info(priv->dev, "%s(), priv->dc_trim.compensation_disabled = true\n",
			 __func__);
	}
#endif
	dev_info(priv->dev, "+%s()\n", __func__);

	/* Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true);
	/* release HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			0x1 << 6, 0x1 << 6);

	if (priv->apply_n12db_gain)
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				0xff, 0x0004);

	/* Audio left headphone input multiplexor selection : LOL */
	set_hp_l_input_mux(priv, HP_INPUT_MUX_LOL);

	/* Disable headphone short-circuit protection */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0xf0ff, 0x3000);
	/* Disable lineout short-ckt protection */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1 << 4, 0x1 << 4);

	/* Reduce ESD resistance of AU_REFN */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0xc000);
	/* Set HPL/HPR gain to -10dB */
	regmap_write(priv->regmap, MT6358_ZCD_CON2, DL_GAIN_N_10DB_REG);
	/* Set LOLR/LOLL gain to -10dB */
	regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_10DB_REG);

	/* Turn on DA_600K_NCP_VA18 */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON1, 0x0001);
	/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON2, 0x002c);
	/* Toggle RG_DIVCKS_CHG */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON0, 0x0001);
	/* Set NCP soft start mode as default mode: 150us */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON4, 0x0002);
	/* Enable NCP */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x0000);
	usleep_range(250, 270);

	/* Enable cap-less LDOs (1.5V), LCLDO local sense */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
			0x1055, 0x1055);
	/* Enable NV regulator (-1.2V) */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x0001);
	usleep_range(100, 120);

	/* Enable AUD_ZCD */
	zcd_enable(priv, true, DEVICE_HP);

	/* Enable IBIST */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
	/* Set HP DR bias current optimization, 010: 6uA */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON11,
			   0xff80, 0x4900);
	/* Set HP & ZCD bias current optimization */
	/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
	/* Set HPP/N STB enhance circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0xc033);

	if (priv->apply_n12db_gain) {
		/* HP IVBUF (Vin path) de-gain enable: -12dB */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON10,
				0xff, 0x04);
	}

	/* Set LO STB enhance circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1 << 8, 0x1 << 8);
	/* Enable LO driver bias circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1 << 1, 0x1 << 1);
	/* Enable LO driver core circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1, 0x1);
	/* Set LOL gain to normal gain step by step */
	regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_10DB_REG);

	/* Switch HPL MUX to Line-out */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0x3 << 8, 0x01 << 8);
	/* Switch HPR MUX to DAC-R */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0x3 << 10, 0x2 << 10);
	/* Enable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0xff, 0x0c);
	/* Enable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0xff, 0x3c);
	/* Enable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0xff00, 0x0c00);
	/* Enable HP driver bias circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0xf0, 0xc0);
	/* Enable HP driver core circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0xf0, 0xf0);
	/* Short HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0xff, 0xfc);
	/* Enable HP main CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0xff00, 0x0e00);
	/* Disable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0xff00, 0x0200);

	/* Select CMFB resistor bulk to AC mode */
	/* Selec HS/LO cap size (6.5pF default) */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON10, 0x0000);

	/* Enable HS driver bias circuits */
	/* Disable HS main output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0010);
	/* Enable LO driver bias circuits */
	/* Disable LO main output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON7, 0x0010);

	/* Enable LO main output stage */
	enable_lo_buffer(priv, true);
	set_speaker_gain(priv, priv->ana_gain[AUDIO_ANALOG_VOLUME_LINEOUTL]);

	/* Enable HP main output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x00ff, 0x00ff);
	/* Enable HPR/L main output stage step by step */
	hp_main_output_ramp(priv, true);
	/* Reduce HP aux feedback loop gain step by step */
	hp_aux_feedback_loop_gain_ramp(priv, true);
	/* Disable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x00ff, 0x00cf);
	/* apply volume setting */
	headset_volume_ramp(priv,
			DL_GAIN_N_10DB,
			priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL]);
	/* Disable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x00ff, 0x00c3);
	/* Unshort HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x00ff, 0x0003);
	udelay(1000);

	/* Enable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x1);
	/* Enable Audio DAC  */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0xf, 0xf);
	/* Enable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x1, 0x1);
	udelay(100);

	/* Switch LOL MUX to audio DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x3 << 2, 0x2 << 2);

	/* Disable Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, false);

	return 0;
}


static int mtk_hp_spk_disable(struct mt6358_priv *priv)
{
	dev_info(priv->dev, "+%s()\n", __func__);

	/* Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true);

	/* Disable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x1, 0x0);

	/* Disable Audio DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0xf, 0x0);

	/* Disable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x0);

	/* Short HP main output to HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fc3);
	/* Enable HP aux output stage*/
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fcf);

	/* decrease HPL/R gain to normal gain step by step */
	headset_volume_ramp(priv,
			priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL],
			DL_GAIN_N_10DB);
	/* decrease LOL gain to minimum gain step by step */
	regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_10DB_REG);

	/* set HP aux feedback loop gain to max */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0xff00, 0xf200);
	/* Enable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			   0xff, 0xff);
	/* Reduce HP aux feedback loop gain */
	hp_aux_feedback_loop_gain_ramp(priv, false);

	/* decrease HPR/L main output stage step by step */
	hp_main_output_ramp(priv, false);

	/* HPR/HPL mux to open */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   0xf << 8, 0x0 << 8);
	/* LOL mux to open */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			   0x3 << 2, 0x0 << 2);

	/* Disable HP main output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3, 0x0);
	/* Enable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x0f00, 0x0e00);
	/* Disable HP main CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x0f00, 0x0c00);
	/* Unshort HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x3 << 6, 0x0);
	/* Disable HP driver core circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x30c0);
	/* Disable HP driver bias circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x3000);
	/* Disable HP aux CMFB loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0);
	/* Open HP aux feedback loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0xc);
	/* Disable HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x0);

	/* decrease LOL gain to minimum gain step by step */
	regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_40DB_REG);
	/* Disable LO driver core circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1, 0x0);
	/* Disable LO driver bias circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1 << 1, 0x0 << 1);

	/* Disable IBIST */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON12,
			0x1 << 8, 0x1 << 8);

	/* Disable AUD_ZCD */
	zcd_enable(priv, false, DEVICE_HP);

	/* Disable NV regulator (-1.2V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x1, 0x0);
	/* Disable cap-less LDOs (1.5V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14, 0x1055, 0x0);
	/* Disable NCP */
	regmap_update_bits(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x1, 0x1);

	/* Set HPL/HPR gain to mute */
	regmap_write(priv->regmap, MT6358_ZCD_CON2, DL_GAIN_N_40DB_REG);
	/* Increase ESD resistance of AU_REFN */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON2,
			0x1 << 14, 0x0);

	/* Set HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			0x1 << 6, 0x0);
	/* disable Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, false);

	return 0;
}

static int mtk_hp_dual_spk_enable(struct mt6358_priv *priv)
{
	unsigned int trim_setting = 0;
	unsigned int reg_value = 0;
#ifdef ANALOG_HPTRIM
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), current AUDDEC_ELR_0 = 0x%x, mic_vinp_mv = %d\n",
			__func__, reg_value, priv->dc_trim.mic_vinp_mv);

	if (priv->dc_trim.mic_vinp_mv > MIC_VINP_4POLE_THRES_MV &&
		((priv->debug_flag & DBG_DCTRIM_BYPASS_4POLE) == 0)) {
		trim_setting = priv->dc_trim.spk_hp_4_pole_trim_setting;
	} else {
		trim_setting = priv->dc_trim.spk_hp_3_pole_trim_setting;
	}

	if (!priv->dc_trim.dc_compensation_disabled) {
		regmap_write(priv->regmap, MT6358_AUDDEC_ELR_0, trim_setting);
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), new AUDDEC_ELR_0 = 0x%x\n",
			 __func__, reg_value);
	} else {
		regmap_write(priv->regmap, MT6358_AUDDEC_ELR_0, 0x0);
		dev_info(priv->dev, "%s(), priv->dc_trim.compensation_disabled = true\n",
			 __func__);
	}
#endif
	dev_info(priv->dev, "+%s()\n", __func__);

	/* Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true);
	/* release HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			0x1 << 6, 0x1 << 6);

	if (priv->apply_n12db_gain)
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				0xff, 0x0004);

	/* Audio left headphone input multiplexor selection : LOL */
	set_hp_l_input_mux(priv, HP_INPUT_MUX_LOL);

	/* Disable headphone short-circuit protection */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0xf0ff, 0x3000);
	/* Disable lineout short-ckt protection */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1 << 4, 0x1 << 4);

	/* Reduce ESD resistance of AU_REFN */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0xc000);
	/* Set HPL/HPR gain to -10dB */
	regmap_write(priv->regmap, MT6358_ZCD_CON2, DL_GAIN_N_10DB_REG);
	/* Set LOLR/LOLL gain to -10dB */
	regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_10DB_REG);

	/* Turn on DA_600K_NCP_VA18 */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON1, 0x0001);
	/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON2, 0x002c);
	/* Toggle RG_DIVCKS_CHG */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON0, 0x0001);
	/* Set NCP soft start mode as default mode: 150us */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON4, 0x0002);
	/* Enable NCP */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x0000);
	usleep_range(250, 270);

	/* Enable cap-less LDOs (1.5V), LCLDO local sense */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
			0x1055, 0x1055);
	/* Enable NV regulator (-1.2V) */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x0001);
	usleep_range(100, 120);

	/* Disable AUD_ZCD */
	zcd_enable(priv, false, DEVICE_LO);

	/* Enable IBIST */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
	/* Set HP DR bias current optimization, 010: 6uA */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON11, 0x4900);
	/* Set HP & ZCD bias current optimization */
	/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
	/* Set HPP/N STB enhance circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0xc033);

	if (priv->apply_n12db_gain) {
		/* HP IVBUF (Vin path) de-gain enable: -12dB */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON10,
				0xff, 0x04);
	}

	/* Set LO STB enhance circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1 << 8, 0x1 << 8);
	/* Set HS STB enhance circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0090);
	/* Enable LO driver bias circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1 << 1, 0x1 << 1);
	/* Enable LO driver core circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1, 0x1);
	/* Set LOL gain to normal gain step by step */
	regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_10DB_REG);

	/* Set HS output stage (3'b011 = 4x) for stereo lineout seneario */
	regmap_update_bits(priv->regmap,
			   MT6358_AUDDEC_ANA_CON10,
			   RG_ABIDEC_RSVD2_VAUDP15_MASK_SFT,
			   0x30 << RG_ABIDEC_RSVD2_VAUDP15_SFT);

	/* Enable HS driver bias circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0092);
	/* Enable HS driver core circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0093);

	/* Set HS gain to normal gain step by step */
	regmap_write(priv->regmap, MT6358_ZCD_CON3,
		     priv->ana_gain[AUDIO_ANALOG_VOLUME_HSOUTL]);

	/* Switch HPL MUX to Line-out */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0x3 << 8, 0x03 << 8);
	/* Switch HPR MUX to HS */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0x3 << 10, 0x1 << 10);
	/* Enable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0xff, 0x0c);
	/* Enable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0xff, 0x3c);
	/* Enable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0xff00, 0x0c00);
	/* Enable HP driver bias circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0xf0, 0xc0);
	/* Enable HP driver core circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0xf0, 0xf0);
	/* Short HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0xff, 0xfc);
	/* Enable HP main CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0xff00, 0x0e00);
	/* Disable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0xff00, 0x0200);

	/* Select CMFB resistor bulk to AC mode */
	/* Selec HS/LO cap size (6.5pF default) */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON10, 0x0000);

	/* Enable LO main output stage */
	enable_lo_buffer(priv, true);
	set_speaker_gain(priv, priv->ana_gain[AUDIO_ANALOG_VOLUME_LINEOUTL]);

	/* Enable HP main output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x00ff, 0x00ff);
	/* Enable HPR/L main output stage step by step */
	hp_main_output_ramp(priv, true);
	/* Reduce HP aux feedback loop gain step by step */
	hp_aux_feedback_loop_gain_ramp(priv, true);
	/* Disable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x00ff, 0x00cf);
	/* apply volume setting */
	headset_volume_ramp(priv,
			DL_GAIN_N_10DB,
			priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL]);
	/* Disable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x00ff, 0x00c3);
	/* Unshort HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x00ff, 0x0003);
	udelay(1000);

	/* Enable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x1);
	/* Enable Audio DAC  */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0xf, 0xf);
	/* Enable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x1, 0x1);
	udelay(100);

	/* Switch LOL MUX to audio DAC R */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x3 << 2, 0x1 << 2);
	/* Switch HS MUX to audio DAC L */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON6,
			0x3 << 2, 0x2 << 2);

	/* Disable Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, false);

	return 0;
}


static int mtk_hp_dual_spk_disable(struct mt6358_priv *priv)
{
	dev_info(priv->dev, "+%s()\n", __func__);

	/* Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true);

	/* HPR/HPL mux to open */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   0xf << 8, 0x0 << 8);

	/* LOL mux to open */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			   0x3 << 2, 0x0 << 2);

	/* HS mux to open */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON6,
			   0x3 << 2, 0x0 << 2);

	/* Disable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x1, 0x0);

	/* Disable Audio DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0xf, 0x0);

	/* Disable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x0);

	/* Short HP main output to HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fc3);
	/* Enable HP aux output stage*/
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fcf);

	/* decrease HPL/R gain to normal gain step by step */
	headset_volume_ramp(priv,
			priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL],
			DL_GAIN_N_10DB);
	/* decrease LOL gain to minimum gain step by step */
	regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_10DB_REG);

	/* set HP aux feedback loop gain to max */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0xff00, 0xf200);
	/* Enable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			   0xff, 0xff);
	/* Reduce HP aux feedback loop gain */
	hp_aux_feedback_loop_gain_ramp(priv, false);

	/* decrease HPR/L main output stage step by step */
	hp_main_output_ramp(priv, false);

	/* Disable HP main output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3, 0x0);
	/* Enable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x0f00, 0x0e00);
	/* Disable HP main CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x0f00, 0x0c00);
	/* Unshort HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x3 << 6, 0x0);
	/* Disable HP driver core circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x30c0);
	/* Disable HP driver bias circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x3000);
	/* Disable HP aux CMFB loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0);
	/* Open HP aux feedback loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0xc);
	/* Disable HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x0);

	/* decrease LOL gain to minimum gain step by step */
	regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_40DB_REG);
	/* Disable LO driver core circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1, 0x0);
	/* Disable LO driver bias circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1 << 1, 0x0 << 1);

	/* Disable IBIST */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON12,
			0x1 << 8, 0x1 << 8);
	/* Disable NV regulator (-1.2V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x1, 0x0);
	/* Disable cap-less LDOs (1.5V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14, 0x1055, 0x0);
	/* Disable NCP */
	regmap_update_bits(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x1, 0x1);

	/* Set HPL/HPR gain to mute */
	regmap_write(priv->regmap, MT6358_ZCD_CON2, DL_GAIN_N_40DB_REG);
	/* Increase ESD resistance of AU_REFN */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON2,
			0x1 << 14, 0x0);

	/* Set HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			0x1 << 6, 0x0);
	/* disable Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, false);

	return 0;
}

static int mtk_hp_impedance_enable(struct mt6358_priv *priv)
{
	dev_info(priv->dev, "%s()\n", __func__);

	/* release HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			   0x1 << 6, 0x1 << 6);

	/* Disable headphone short-circuit protection */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x3000);

	/* Reduce ESD resistance of AU_REFN */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0x4000);

	/* Turn on DA_600K_NCP_VA18 */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON1, 0x0001);
	/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON2, 0x002c);
	/* Toggle RG_DIVCKS_CHG */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON0, 0x0001);
	/* Set NCP soft start mode as default mode: 150us */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON4, 0x0002);
	/* Enable NCP */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x0000);
	usleep_range(250, 270);

	/* Enable cap-less LDOs (1.5V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
			   0x1055, 0x1055);
	/* Enable NV regulator (-1.2V) */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x0001);
	usleep_range(100, 120);

	/* Enable AUD_ZCD */
	zcd_enable(priv, true, DEVICE_HP);

	/* Enable IBIST */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
	/* Disable HPR/L STB enhance circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0x4000);

	/* Disable Pull-down HPL/R to AVSS28_AUD */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON4, 0x0000);

	/* Enable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x1);
	/* Enable Audio L channel DAC */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x3009);

	/* Enable Trim buffer VA28 reference */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			   0x00ff, 0x0002);

	/* Enable HPDET circuit, */
	/* select DACLP as HPDET input and HPR as HPDET output */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON8, 0x1900);

	/* Enable TRIMBUF circuit, select HPR as TRIMBUF input */
	/* Set TRIMBUF gain as 18dB */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON8, 0x1972);

	return 0;
}

static int mtk_hp_impedance_disable(struct mt6358_priv *priv)
{
	dev_info(priv->dev, "%s()\n", __func__);

	/* disable HPDET circuit */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON8,
			   0xff00, 0x0000);

	/* Disable Trim buffer VA28 reference */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			   0x00ff, 0x0000);

	/* Disable Audio DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   0x000f, 0x0000);

	/* Disable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x0);

	/* Disable HP main output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3, 0x0);

	/* Disable HP driver core circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   0x3 << 4, 0x0);

	/* Disable HP driver bias circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   0x3 << 6, 0x0);

	/* Disable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			   0xff << 8, 0x0);

	/* Disable IBIST */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON12,
			   0x1 << 8, 0x1 << 8);

	/* Disable AUD_ZCD */
	zcd_enable(priv, false, DEVICE_HP);

	/* Disable NV regulator (-1.2V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x1, 0x0);
	/* Disable cap-less LDOs (1.5V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14, 0x1055, 0x0);
	/* Disable NCP */
	regmap_update_bits(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x1, 0x1);

	/* Set HPL/HPR gain to mute */
	regmap_write(priv->regmap, MT6358_ZCD_CON2, DL_GAIN_N_10DB_REG);

	/* Set HPP/N STB enhance circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0xff, 0x33);

	/* Increase ESD resistance of AU_REFN */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON2,
			   0x1 << 14, 0x0);

	/* Set HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			   0x1 << 6, 0x0);

	return 0;
}

static int mt_hp_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol,
		       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);
	int device = DEVICE_HP;

	dev_info(priv->dev, "%s(), event 0x%x, dev_counter[DEV_HP] %d, mux %u\n",
		 __func__,
		 event,
		 priv->dev_counter[device],
		 mux);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		priv->dev_counter[device]++;
		if (priv->dev_counter[device] > 1)
			break;	/* already enabled, do nothing */
		else if (priv->dev_counter[device] <= 0)
			dev_warn(priv->dev, "%s(), dev_counter[DEV_HP] %d <= 0\n",
				 __func__,
				 priv->dev_counter[device]);

		priv->mux_select[MUX_HP_L] = mux;

		if (mux == HP_MUX_HP)
			mtk_hp_enable(priv);
		else if (mux == HP_MUX_HPSPK)
			mtk_hp_spk_enable(priv);
		else if (mux == HP_MUX_HP_DUALSPK)
			mtk_hp_dual_spk_enable(priv);
		else if (mux == HP_MUX_HP_IMPEDANCE)
			mtk_hp_impedance_enable(priv);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		priv->dev_counter[device]--;
		if (priv->dev_counter[device] > 0) {
			break;	/* still being used, don't close */
		} else if (priv->dev_counter[device] < 0) {
			dev_warn(priv->dev, "%s(), dev_counter[DEV_HP] %d < 0\n",
				 __func__,
				 priv->dev_counter[device]);
			priv->dev_counter[device] = 0;
			break;
		}

		if (priv->mux_select[MUX_HP_L] == HP_MUX_HP)
			mtk_hp_disable(priv);
		else if (priv->mux_select[MUX_HP_L] == HP_MUX_HPSPK)
			mtk_hp_spk_disable(priv);
		else if (priv->mux_select[MUX_HP_L] == HP_MUX_HP_DUALSPK)
			mtk_hp_dual_spk_disable(priv);
		else if (priv->mux_select[MUX_HP_L] == HP_MUX_HP_IMPEDANCE)
			mtk_hp_impedance_disable(priv);

		priv->mux_select[MUX_HP_L] = mux;
		break;
	default:
		break;
	}

	return 0;
}

static int mt_lo_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,
			int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event 0x%x, mux %u\n",
		 __func__,
		 event,
		 dapm_kcontrol_get_value(w->kcontrols[0]));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Reduce ESD resistance of AU_REFN */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0x4000);

		/* Turn on DA_600K_NCP_VA18 */
		regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON1, 0x0001);
		/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
		regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON2, 0x002c);
		/* Toggle RG_DIVCKS_CHG */
		regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON0, 0x0001);
		/* Set NCP soft start mode as default mode: 100us */
		regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON4, 0x0003);
		/* Enable NCP */
		regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x0000);
		usleep_range(250, 270);

		/* Enable cap-less LDOs (1.5V) */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
				   0x1055, 0x1055);
		/* Enable NV regulator (-1.2V) */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x0001);
		usleep_range(100, 120);

		/* Enable AUD_ZCD */
		zcd_enable(priv, true, DEVICE_LO);

		/* Disable lineout short-ckt protection */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				   0x1 << 4, 0x1 << 4);

		/* Set LOLR/LOLL gain to -10dB */
		regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_10DB_REG);

		/* Enable IBIST */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
		/* Set HP DR bias current optimization, 010: 6uA */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON11,
				   0xff80, 0x4900);
		/* Set HP & ZCD bias current optimization */
		/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
		/* Set LO STB enhance circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				   0x1 << 8, 0x1 << 8);

		/* Disable HP main CMFB loop */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0000);
		/* Select CMFB resistor bulk to AC mode */
		/* Selec HS/LO cap size (6.5pF default) */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON10, 0x0000);

		/* Enable LO driver bias circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				   0x1 << 1, 0x1 << 1);
		/* Enable LO driver core circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				   0x1, 0x1);

		/* Set LO gain to normal gain step by step */
		set_speaker_gain(priv,
				 priv->ana_gain[AUDIO_ANALOG_VOLUME_LINEOUTL]);

		/* Enable AUD_CLK */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13,
				   0x1, 0x1);
		/* Enable Audio DAC R */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
				   RG_AUDDACRPWRUP_VAUDP15_MASK_SFT |
				   RG_AUD_DAC_PWR_UP_VA28_MASK_SFT, 0x0006);

		/* Enable low-noise mode of DAC */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0001);
		/* Switch LOL MUX to audio DAC R */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				   0x3 << 2, 0x1 << 2);

		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* Switch LOL MUX to open */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				   0x3 << 2, 0x0 << 2);
		/* Disable Audio DAC R */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
				   RG_AUDDACRPWRUP_VAUDP15_MASK_SFT |
				   RG_AUD_DAC_PWR_UP_VA28_MASK_SFT, 0x0000);

		/* Disable AUD_CLK */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13,
				   0x1, 0x0);

		/* decrease LO gain to minimum gain step by step */
		regmap_write(priv->regmap, MT6358_ZCD_CON1,
			     DL_GAIN_N_40DB_REG);

		/* Disable LO driver core circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				   0x1, 0x0);

		/* Disable LO driver bias circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				   0x1 << 1, 0x0 << 1);

		/* Disable HP aux CMFB loop */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
				   0xff << 8, 0x0);

		/* Enable HP main CMFB Switch */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
				   0xff << 8, 0x2 << 8);

		/* Disable IBIST */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON12,
				   0x1 << 8, 0x1 << 8);

		/* Disable AUD_ZCD */
		zcd_enable(priv, false, DEVICE_LO);

		/* Disable NV regulator (-1.2V) */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON15,
				   0x1, 0x0);
		/* Disable cap-less LDOs (1.5V) */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
				   0x1055, 0x0);
		/* Disable NCP */
		regmap_update_bits(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3,
				   0x1, 0x1);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_rcv_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,
			int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event 0x%x, mux %u\n",
		 __func__,
		 event,
		 dapm_kcontrol_get_value(w->kcontrols[0]));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Reduce ESD resistance of AU_REFN */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0x4000);

		/* Turn on DA_600K_NCP_VA18 */
		regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON1, 0x0001);
		/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
		regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON2, 0x002c);
		/* Toggle RG_DIVCKS_CHG */
		regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON0, 0x0001);
		/* Set NCP soft start mode as default mode: 100us */
		regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON4, 0x0003);
		/* Enable NCP */
		regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x0000);
		usleep_range(250, 270);

		/* Enable cap-less LDOs (1.5V) */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
				   0x1055, 0x1055);
		/* Enable NV regulator (-1.2V) */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x0001);
		usleep_range(100, 120);

		/* Enable AUD_ZCD */
		zcd_enable(priv, true, DEVICE_RCV);

		/* Disable handset short-circuit protection */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0010);

		/* Enable IBIST */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
		/* Set HP DR bias current optimization, 010: 6uA */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON11,
				   0xff80, 0x4900);
		/* Set HP & ZCD bias current optimization */
		/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
		/* Set HS STB enhance circuits */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0090);

		/* Disable HP main CMFB loop */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0000);
		/* Select CMFB resistor bulk to AC mode */
		/* Selec HS/LO cap size (6.5pF default) */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON10, 0x0000);

		/* Enable HS driver bias circuits */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0092);
		/* Enable HS driver core circuits */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0093);

		/* Set HS gain to normal gain step by step */
		regmap_write(priv->regmap, MT6358_ZCD_CON3,
			     priv->ana_gain[AUDIO_ANALOG_VOLUME_HSOUTL]);

		/* Enable AUD_CLK */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13,
				   0x1, 0x1);

		/* Enable Audio DAC L */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
				RG_AUDDACLPWRUP_VAUDP15_MASK_SFT |
				RG_AUD_DAC_PWL_UP_VA28_MASK_SFT, 0x0009);

		/* Enable low-noise mode of DAC */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0001);
		/* Switch HS MUX to audio DAC L */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x009B);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* HS mux to open */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON6,
				    RG_AUDHSMUXINPUTSEL_VAUDP15_MASK_SFT,
				    RCV_MUX_OPEN);
		/* Disable Audio DAC L */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
				RG_AUDDACLPWRUP_VAUDP15_MASK_SFT |
				RG_AUD_DAC_PWL_UP_VA28_MASK_SFT, 0x0000);

		/* Disable AUD_CLK */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13,
				   0x1, 0x0);

		/* decrease HS gain to minimum gain step by step */
		regmap_write(priv->regmap, MT6358_ZCD_CON3, DL_GAIN_N_40DB);

		/* Disable HS driver core circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON6,
				   0x1, 0x0);

		/* Disable HS driver bias circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON6,
				   0x1 << 1, 0x0000);

		/* Disable HP aux CMFB loop */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
				   0xff << 8, 0x0);

		/* Enable HP main CMFB Switch */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
				   0xff << 8, 0x2 << 8);

		/* Disable IBIST */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON12,
				   0x1 << 8, 0x1 << 8);

		/* Disable AUD_ZCD */
		zcd_enable(priv, false, DEVICE_RCV);

		/* Disable NV regulator (-1.2V) */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON15,
				   0x1, 0x0);
		/* Disable cap-less LDOs (1.5V) */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
				   0x1055, 0x0);
		/* Disable NCP */
		regmap_update_bits(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3,
				   0x1, 0x1);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_aif_out_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event 0x%x, rate %d\n",
		__func__, event, priv->ul_rate);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		capture_gpio_set(priv);
		break;
	case SND_SOC_DAPM_POST_PMD:
		capture_gpio_reset(priv);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_adc_supply_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol,
			       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mic_type = priv->mux_select[MUX_MIC_TYPE];

	dev_dbg(priv->dev, "%s(), event 0x%x, vow_enable: %d\n",
		__func__, event, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Enable audio ADC CLKGEN  */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13,
				   0x1 << 5, 0x1 << 5);
		if (IS_AMIC_BASE(mic_type) && priv->vow_enable) {
			/* ADC CLK from CLKGEN (3.25MHz) */
			dev_info(priv->dev, "%s(), vow mode\n", __func__);
			regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON3,
				     0x0009);
		} else {
			/* ADC CLK from CLKGEN (13MHz) */
			regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON3,
				     0x0000);
		}
		/* Enable  LCLDO_ENC 1P8V */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
				   0x2500, 0x0100);
		/* LCLDO_ENC remote sense */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
				   0x2500, 0x2500);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* LCLDO_ENC remote sense off */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
				   0x2500, 0x0100);
		/* disable LCLDO_ENC 1P8V */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
				   0x2500, 0x0000);

		/* ADC CLK from CLKGEN (13MHz) */
		regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON3, 0x0000);
		/* disable audio ADC CLKGEN  */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13,
				   0x1 << 5, 0x0 << 5);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_vow_ldo_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event 0x%x, MIC_TYPE %x\n",
		 __func__, event, priv->mux_select[MUX_MIC_TYPE]);

	if (!(IS_AMIC_BASE(priv->mux_select[MUX_MIC_TYPE]))) {
		dev_info(priv->dev, "%s(), no AMIC, return\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Enable audio uplink LPW mode */
		/* Enable Audio ADC 1st Stage LPW */
		/* Enable Audio ADC 2nd & 3rd LPW */
		/* Enable Audio ADC flash Audio ADC flash */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   RG_CLKSQ_EN_VOW_MASK_SFT,
				   0x1 << RG_CLKSQ_EN_VOW_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Disable audio uplink LPW mode */
		/* Disable Audio ADC 1st Stage LPW */
		/* Disable Audio ADC 2nd & 3rd LPW */
		/* Disable Audio ADC flash Audio ADC flash */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   RG_CLKSQ_EN_VOW_MASK_SFT,
				   0x0 << RG_CLKSQ_EN_VOW_SFT);
		dev_info(priv->dev, "%s(), set vow disable at the last vow capture widget\n",
			 __func__);
		priv->vow_enable = 0;
		break;
	default:
		break;
	}

	return 0;
}

static int mt_vow_out_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_WILL_PMU:
		priv->vow_enable = 1;
		break;
	case SND_SOC_DAPM_PRE_PMU:
		/* vow gpio set */
		regmap_update_bits(priv->regmap, MT6358_GPIO_MODE3_CLR,
				   0xffff, 0xffff);
		regmap_update_bits(priv->regmap, MT6358_GPIO_MODE3_SET,
				   0xffff, 0x0120);
		regmap_update_bits(priv->regmap, MT6358_GPIO_MODE3,
				   0xffff, 0x0120);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* vow gpio clear */
		regmap_update_bits(priv->regmap, MT6358_GPIO_MODE3_CLR,
				   0xffff, 0xffff);
		regmap_update_bits(priv->regmap, MT6358_GPIO_MODE3,
				   0xffff, 0x0000);
		break;
	default:
		break;
	}

	return 0;
}

static int mt6358_vow_cfg_enable(struct mt6358_priv *priv)
{
	unsigned int mic_type = priv->mux_select[MUX_MIC_TYPE];

	dev_info(priv->dev, "%s(), mic_type: %d, vow_dmic_lp: %d\n", __func__,
		 mic_type, priv->vow_dmic_lp);

	/* Enable vow cfg setting */
	regmap_write(priv->regmap, MT6358_AFE_VOW_CFG0,
		     priv->reg_afe_vow_cfg0);
	regmap_write(priv->regmap, MT6358_AFE_VOW_CFG1,
		     priv->reg_afe_vow_cfg1);
	regmap_write(priv->regmap, MT6358_AFE_VOW_CFG2,
		     priv->reg_afe_vow_cfg2);
	regmap_write(priv->regmap, MT6358_AFE_VOW_CFG3,
		     priv->reg_afe_vow_cfg3);
	regmap_update_bits(priv->regmap, MT6358_AFE_VOW_CFG4,
			   0x000f, priv->reg_afe_vow_cfg4);
	regmap_write(priv->regmap, MT6358_AFE_VOW_CFG5,
		     priv->reg_afe_vow_cfg5);

	if (mic_type == MIC_TYPE_MUX_DMIC) {
		if (priv->vow_dmic_lp) {
			regmap_update_bits(priv->regmap, MT6358_AFE_VOW_CFG4,
					   0xfff0, 0x024e);
			/* vow posdiv and cic mode configure*/
			/* LP DMIC settings : 812.5k */
			regmap_write(priv->regmap, MT6358_AFE_VOW_POSDIV_CFG0, 0x0c0a);
			/* VOW_DIGMIC_ON */
			regmap_update_bits(priv->regmap, MT6358_AFE_VOW_TOP,
				   0x20c0, 0x20c0);
		} else {
			regmap_update_bits(priv->regmap, MT6358_AFE_VOW_CFG4,
					   0xfff0, 0x029e);
			/* vow posdiv and cic mode configure */
			/* DMIC settings : 1600k */
			regmap_write(priv->regmap, MT6358_AFE_VOW_POSDIV_CFG0, 0x0c00);
			/* VOW_DIGMIC_ON */
			regmap_update_bits(priv->regmap, MT6358_AFE_VOW_TOP,
					   0x20c0, 0x20c0);
		}
	} else {
		regmap_update_bits(priv->regmap, MT6358_AFE_VOW_CFG4,
				   0xfff0, 0x029e);
		/* vow posdiv and cic mode configure */
		/* AMIC settings : 1600k */
		regmap_write(priv->regmap, MT6358_AFE_VOW_POSDIV_CFG0, 0x0c00);
		/* VOW_DIGMIC_OFF */
		regmap_update_bits(priv->regmap, MT6358_AFE_VOW_TOP,
				   0x20c0, 0x0000);
	}
	return 0;
}

static void mt6358_vow_cfg_disable(struct mt6358_priv *priv)
{
	unsigned int mic_type = priv->mux_select[MUX_MIC_TYPE];

	dev_info(priv->dev, "%s(), mic_type 0x%x\n", __func__, mic_type);

	/* VOW_DIGMIC_OFF */
	regmap_update_bits(priv->regmap, MT6358_AFE_VOW_TOP,
			   0x20c0, 0x0000);
}

static int mt6358_amic_enable(struct mt6358_priv *priv)
{
	unsigned int mic_type = priv->mux_select[MUX_MIC_TYPE];
	unsigned int mux_pga_l = priv->mux_select[MUX_PGA_L];
	unsigned int mux_pga_r = priv->mux_select[MUX_PGA_R];
	int mic_gain_l = priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP1];
	int mic_gain_r = priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP2];
	unsigned int reg_value = 0, rc_1 = 0, rc_2 = 0;

	dev_info(priv->dev, "%s(), mux, mic %u, pga l %u, pga r %u, mic_gain l %d, r %d\n",
		 __func__, mic_type, mux_pga_l, mux_pga_r,
		 mic_gain_l, mic_gain_r);

	if (IS_DCC_BASE(mic_type)) {
		/* DCC 50k CLK (from 26M) */
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2062);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2062);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2060);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2061);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG1, 0x0100);
	}

	/* mic bias 0 */
	if (mux_pga_l == PGA_MUX_AIN0 || mux_pga_l == PGA_MUX_AIN2 ||
	    mux_pga_r == PGA_MUX_AIN0 || mux_pga_r == PGA_MUX_AIN2) {
		switch (mic_type) {
		case MIC_TYPE_MUX_DCC_ECM_DIFF:
			regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON9,
					   0xff00, 0x7700);
			break;
		case MIC_TYPE_MUX_DCC_ECM_SINGLE:
			regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON9,
					   0xff00, 0x1100);
			break;
		default:
			regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON9,
					   0xff00, 0x0000);
			break;
		}
		/* Enable MICBIAS0, MISBIAS0 = 1P9V */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON9,
				   0xff, 0x21);
	}

	/* mic bias 1 */
	if (mux_pga_l == PGA_MUX_AIN1 || mux_pga_r == PGA_MUX_AIN1) {
		/* Enable MICBIAS1, MISBIAS1 = 2P6V */
		if (mic_type == MIC_TYPE_MUX_DCC_ECM_SINGLE)
			regmap_write(priv->regmap,
				     MT6358_AUDENC_ANA_CON10, 0x0161);
		else
			regmap_write(priv->regmap,
				     MT6358_AUDENC_ANA_CON10, 0x0061);
	}

	/* set mic pga gain */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   RG_AUDPREAMPLGAIN_MASK_SFT,
			   mic_gain_l << RG_AUDPREAMPLGAIN_SFT);
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
			   RG_AUDPREAMPRGAIN_MASK_SFT,
			   mic_gain_r << RG_AUDPREAMPRGAIN_SFT);

	if (IS_DCC_BASE(mic_type)) {
		/* Audio L/R preamplifier DCC precharge */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   0xf8ff, 0x0004);
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   0xf8ff, 0x0004);
	} else {
		/* reset reg */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   0xf8ff, 0x0000);
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   0xf8ff, 0x0000);
	}

	if (mux_pga_l != PGA_MUX_NONE) {
		/* L preamplifier input sel */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDPREAMPLINPUTSEL_MASK_SFT,
				   mux_pga_l << RG_AUDPREAMPLINPUTSEL_SFT);

		/* L preamplifier enable */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDPREAMPLON_MASK_SFT,
				   0x1 << RG_AUDPREAMPLON_SFT);

		if (IS_DCC_BASE(mic_type)) {
			/* L preamplifier DCCEN */
			regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
					   RG_AUDPREAMPLDCCEN_MASK_SFT,
					   0x1 << RG_AUDPREAMPLDCCEN_SFT);
		}

		usleep_range(1000, 1050);
		/* L ADC input sel : L PGA. Enable audio L ADC */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDADCLINPUTSEL_MASK_SFT,
				   ADC_MUX_PREAMPLIFIER <<
				   RG_AUDADCLINPUTSEL_SFT);
		usleep_range(1000, 1050);
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDADCLPWRUP_MASK_SFT,
				   0x1 << RG_AUDADCLPWRUP_SFT);
	}

	if (mux_pga_r != PGA_MUX_NONE) {
		/* R preamplifier input sel */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   RG_AUDPREAMPRINPUTSEL_MASK_SFT,
				   mux_pga_r << RG_AUDPREAMPRINPUTSEL_SFT);

		/* R preamplifier enable */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   RG_AUDPREAMPRON_MASK_SFT,
				   0x1 << RG_AUDPREAMPRON_SFT);
		usleep_range(1000, 1050);
		if (IS_DCC_BASE(mic_type)) {
			/* R preamplifier DCCEN */
			regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
					   RG_AUDPREAMPRDCCEN_MASK_SFT,
					   0x1 << RG_AUDPREAMPRDCCEN_SFT);
		}

		/* R ADC input sel : R PGA. Enable audio R ADC */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   RG_AUDADCRINPUTSEL_MASK_SFT,
				   ADC_MUX_PREAMPLIFIER <<
				   RG_AUDADCRINPUTSEL_SFT);
		usleep_range(1000, 1050);
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   RG_AUDADCRPWRUP_MASK_SFT,
				   0x1 << RG_AUDADCRPWRUP_SFT);
	}

	if (IS_DCC_BASE(mic_type)) {
		usleep_range(100, 150);
		/* Audio L preamplifier DCC precharge off */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDPREAMPLDCPRECHARGE_MASK_SFT, 0x0);
		/* Audio R preamplifier DCC precharge off */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   RG_AUDPREAMPRDCPRECHARGE_MASK_SFT, 0x0);

		/* Short body to ground in PGA */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON3,
				   0x1 << 12, 0x0);
	}
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON12, &reg_value);
	rc_1 = reg_value & 0x1f;
	rc_2 = (reg_value >> 8 ) & 0x1f;
	dev_dbg(priv->dev, "%s(), MT6358_AUDENC_CON12(rc2, rc1) = 0x%x(0x%x, 0x%x)\n",
		__func__, reg_value, rc_2, rc_1);
	if ((rc_2 == 0) || (rc_1 == 0)) {
		/* Disable audio L ADC */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDADCLPWRUP_MASK_SFT,
				   0x0 << RG_AUDADCLPWRUP_SFT);
		/* Disable audio R ADC */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   RG_AUDADCRPWRUP_MASK_SFT,
				   0x0 << RG_AUDADCRPWRUP_SFT);
		/* Enable audio L ADC */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDADCLPWRUP_MASK_SFT,
				   0x1 << RG_AUDADCLPWRUP_SFT);
		/* Enable audio R ADC */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   RG_AUDADCRPWRUP_MASK_SFT,
				   0x1 << RG_AUDADCRPWRUP_SFT);
	}
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON12, &reg_value);
	dev_dbg(priv->dev, "%s(), final: MT6358_AUDENC_CON12 = 0x%x\n",
		__func__, reg_value);
	/* here to set digital part */
	mt6358_mtkaif_tx_enable(priv);

	/* UL dmic setting off */
	regmap_write(priv->regmap, MT6358_AFE_UL_SRC_CON0_H, 0x0000);

	/* UL turn on */
	regmap_write(priv->regmap, MT6358_AFE_UL_SRC_CON0_L, 0x0001);

	return 0;
}

static void mt6358_amic_disable(struct mt6358_priv *priv)
{
	unsigned int mic_type = priv->mux_select[MUX_MIC_TYPE];
	unsigned int mux_pga_l = priv->mux_select[MUX_PGA_L];
	unsigned int mux_pga_r = priv->mux_select[MUX_PGA_R];
	int mic_gain_l = priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP1];
	int mic_gain_r = priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP2];

	dev_info(priv->dev, "%s(), mux, mic %u, pga l %u, pga r %u, mic_gain l %d, r %d\n",
		 __func__, mic_type, mux_pga_l, mux_pga_r,
		 mic_gain_l, mic_gain_r);

	/* UL turn off */
	regmap_update_bits(priv->regmap, MT6358_AFE_UL_SRC_CON0_L,
			   0x0001, 0x0000);

	/* disable aud_pad TX fifos */
	mt6358_mtkaif_tx_disable(priv);

	/* L ADC input sel : off, disable L ADC */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   0xf000, 0x0000);
	/* L preamplifier DCCEN */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   0x1 << 1, 0x0);
	/* L preamplifier input sel : off, L PGA 0 dB gain */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   0xfffb, 0x0000);

	/* disable L preamplifier DCC precharge */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   0x1 << 2, 0x0);

	/* R ADC input sel : off, disable R ADC */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
			   0xf000, 0x0000);
	/* R preamplifier DCCEN */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
			   0x1 << 1, 0x0);
	/* R preamplifier input sel : off, R PGA 0 dB gain */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
			   0x0ffb, 0x0000);

	/* disable R preamplifier DCC precharge */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
			   0x1 << 2, 0x0);

	/* mic bias */
	/* Disable MICBIAS0, MISBIAS0 = 1P7V */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON9, 0x0000);

	/* Disable MICBIAS1 */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON10,
			   0x0001, 0x0000);

	if (IS_DCC_BASE(mic_type)) {
		/* dcclk_gen_on=1'b0 */
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2060);
		/* dcclk_pdn=1'b1 */
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2062);
		/* dcclk_ref_ck_sel=2'b00 */
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2062);
		/* dcclk_div=11'b00100000011 */
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2062);
	}
}

static int mt6358_dmic_enable(struct mt6358_priv *priv)
{
	dev_info(priv->dev, "%s()\n", __func__);

	/* mic bias */
	/* Enable MICBIAS0, MISBIAS0 = 1P9V */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON9, 0x0021);

	/* RG_BANDGAPGEN=1'b0 */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON10,
			   0x1 << 12, 0x0);

	/* DMIC enable */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON8, 0x0005);

	/* here to set digital part */
	mt6358_mtkaif_tx_enable(priv);

	/* UL dmic setting */
	regmap_write(priv->regmap, MT6358_AFE_UL_SRC_CON0_H, 0x0080);

	/* UL turn on */
	regmap_write(priv->regmap, MT6358_AFE_UL_SRC_CON0_L, 0x0003);
	return 0;
}

static void mt6358_dmic_disable(struct mt6358_priv *priv)
{
	dev_info(priv->dev, "%s()\n", __func__);

	/* UL turn off */
	regmap_update_bits(priv->regmap, MT6358_AFE_UL_SRC_CON0_L,
			   0x0003, 0x0000);

	/* disable aud_pad TX fifos */
	mt6358_mtkaif_tx_disable(priv);

	/* DMIC disable */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON8, 0x0000);

	/* mic bias */
	/* MISBIAS0 = 1P7V */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON9, 0x0001);

	/* RG_BANDGAPGEN=1'b0 */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON10,
			   0x1 << 12, 0x0);

	/* MICBIA0 disable */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON9, 0x0000);
}

static int mt6358_vow_amic_enable(struct mt6358_priv *priv)
{
	unsigned int mic_type = priv->mux_select[MUX_MIC_TYPE];
	unsigned int mux_pga_l = priv->mux_select[MUX_PGA_L];

	dev_info(priv->dev, "%s(), mux, mic %u, pga l %u\n",
		 __func__, mic_type, mux_pga_l);

	if (IS_DCC_BASE(mic_type)) {
		/* DCC 50k CLK (from 26M) */
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2062);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2062);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2060);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2061);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG1, 0x0100);
	}

	/* mic bias 0 */
	if (mux_pga_l == PGA_MUX_AIN0) {
		switch (mic_type) {
		case MIC_TYPE_MUX_DCC_ECM_DIFF:
			regmap_update_bits(priv->regmap,
					   MT6358_AUDENC_ANA_CON9,
					   0xff00, 0x7700);
			break;
		case MIC_TYPE_MUX_DCC_ECM_SINGLE:
			regmap_update_bits(priv->regmap,
					   MT6358_AUDENC_ANA_CON9,
					   0xff00, 0x1100);
			break;
		default:
			regmap_update_bits(priv->regmap,
					   MT6358_AUDENC_ANA_CON9,
					   0xff00, 0x0000);
			break;
		}
		/* Enable MICBIAS0, MISBIAS0 = 1P9V */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON9,
				   0xff, 0x25);
	}
	/* mic bias 1 */
	if (mux_pga_l == PGA_MUX_AIN1) {
		/* Enable MICBIAS1, MISBIAS1 = 2P6V */
		if (mic_type == MIC_TYPE_MUX_DCC_ECM_SINGLE)
			regmap_write(priv->regmap,
				     MT6358_AUDENC_ANA_CON10, 0x0161);
		else
			regmap_write(priv->regmap,
				     MT6358_AUDENC_ANA_CON10, 0x0061);
	}
	/* mic bias 0 */
	if (mux_pga_l == PGA_MUX_AIN2) {
		switch (mic_type) {
		case MIC_TYPE_MUX_DCC_ECM_DIFF:
			regmap_update_bits(priv->regmap,
					   MT6358_AUDENC_ANA_CON9,
					   0xff00, 0x7700);
			break;
		case MIC_TYPE_MUX_DCC_ECM_SINGLE:
			regmap_update_bits(priv->regmap,
					   MT6358_AUDENC_ANA_CON9,
					   0xff00, 0x1100);
			break;
		default:
			regmap_update_bits(priv->regmap,
					   MT6358_AUDENC_ANA_CON9,
					   0xff00, 0x0000);
			break;
		}
		/* Enable MICBIAS0, MISBIAS0 = 1P9V */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON9,
				   0xff, 0x25);
	}
	/* set mic pga gain : Audio L PGA 24 dB gain*/
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   RG_AUDPREAMPLGAIN_MASK_SFT,
			   0x04 << RG_AUDPREAMPLGAIN_SFT);

	if (IS_DCC_BASE(mic_type)) {
		/* Audio L preamplifier DCC precharge */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   0xf8ff, 0x0004);
	} else {
		/* reset reg */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   0xf8ff, 0x0000);

	}
	if (mux_pga_l != PGA_MUX_NONE) {
		/* L preamplifier input sel */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDPREAMPLINPUTSEL_MASK_SFT,
				   mux_pga_l << RG_AUDPREAMPLINPUTSEL_SFT);

		/* L preamplifier enable */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDPREAMPLON_MASK_SFT,
				   0x1 << RG_AUDPREAMPLON_SFT);

		if (IS_DCC_BASE(mic_type)) {
			/* L preamplifier DCCEN */
			regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
					   RG_AUDPREAMPLDCCEN_MASK_SFT,
					   0x1 << RG_AUDPREAMPLDCCEN_SFT);
		}

		/* L ADC input sel : L PGA. Enable audio L ADC */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDADCLINPUTSEL_MASK_SFT,
				   ADC_MUX_PREAMPLIFIER <<
				   RG_AUDADCLINPUTSEL_SFT);
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDADCLPWRUP_MASK_SFT,
				   0x1 << RG_AUDADCLPWRUP_SFT);
	}
	if (IS_DCC_BASE(mic_type)) {
		usleep_range(100, 150);
		/* Audio L preamplifier DCC precharge off */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   RG_AUDPREAMPLDCPRECHARGE_MASK_SFT, 0x0);
		/* Short body to ground in PGA */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON3,
				   0x1 << 12, 0x0);
	}
	usleep_range(1000, 1200);
	/* Enable audio uplink LPW mode */
	/* Enable Audio ADC 1st Stage LPW */
	/* Enable Audio ADC 2nd & 3rd LPW */
	/* Enable Audio ADC flash Audio ADC flash */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON2,
			   0x0039, 0x0039);
	dev_info(priv->dev, "%s(), mt_vow_aud_lpw_enable 0x39\n",
		 __func__);
	return 0;
}

static int mt6358_vow_amic_disable(struct mt6358_priv *priv)
{
	unsigned int mic_type = priv->mux_select[MUX_MIC_TYPE];
	unsigned int mux_pga_l = priv->mux_select[MUX_PGA_L];

	dev_info(priv->dev, "%s(), mux, mic %u, pga l %u\n",
		 __func__, mic_type, mux_pga_l);
	/* Disable audio uplink LPW mode */
	/* Disable Audio ADC 1st Stage LPW */
	/* Disable Audio ADC 2nd & 3rd LPW */
	/* Disable Audio ADC flash Audio ADC flash */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON2,
			   0x0039, 0x0000);
	dev_info(priv->dev, "%s(), mt_vow_aud_lpw_disable 0x0\n",
		 __func__);
	/* L ADC input sel : off, disable L ADC */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   0xf000, 0x0000);
	/* L preamplifier DCCEN */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   0x1 << 1, 0x0);
	/* L preamplifier input sel : off, L PGA 0 dB gain */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   0xfffb, 0x0000);

	/* disable L preamplifier DCC precharge */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   0x1 << 2, 0x0);

	/* mic bias */
	/* Disable MICBIAS0, MISBIAS0 = 1P7V */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON9, 0x0000);
	/* Disable MICBIAS1, MISBIAS1 = 1P7V */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON10, 0x0000);

	if (IS_DCC_BASE(mic_type)) {
		/* dcclk_gen_on=1'b0 */
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2060);
		/* dcclk_pdn=1'b1 */
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2062);
		/* dcclk_ref_ck_sel=2'b00 */
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2062);
		/* dcclk_div=11'b00100000011 */
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2062);
	}
	return 0;
}

static int mt6358_vow_dmic_enable(struct mt6358_priv *priv)
{
	dev_info(priv->dev, "%s()\n", __func__);
	/* mic bias */
	/* Enable MICBIAS0, MISBIAS0 = 1P9V */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON9, 0x0025);

	/* DMIC enable */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON8, 0x0005);

	return 0;
}

static int mt6358_vow_dmic_disable(struct mt6358_priv *priv)
{
	dev_info(priv->dev, "%s()\n", __func__);
	/* DMIC disable */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON8, 0x0004);

	/* mic bias */
	/* MISBIAS0 = 1P7V */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON9, 0x0000);
	return 0;
}

static int mt_mic_bias_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol,
			     int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = priv->mux_select[MUX_MIC_TYPE];

	dev_dbg(priv->dev, "%s(), event 0x%x, mux %u, vow_enable: %d\n",
		__func__, event, mux, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->vow_enable) {
			if (mux == MIC_TYPE_MUX_DMIC) {
				mt6358_vow_dmic_enable(priv);
				mt6358_vow_cfg_enable(priv);
			} else {
				mt6358_vow_amic_enable(priv);
				mt6358_vow_cfg_enable(priv);
			}
		} else {
			if (mux == MIC_TYPE_MUX_DMIC)
				mt6358_dmic_enable(priv);
			else
				mt6358_amic_enable(priv);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (priv->vow_enable) {
			if (mux == MIC_TYPE_MUX_DMIC) {
				mt6358_vow_cfg_disable(priv);
				mt6358_vow_dmic_disable(priv);
			} else {
				mt6358_vow_cfg_disable(priv);
				mt6358_vow_amic_disable(priv);
			}
		} else {
			if (mux == MIC_TYPE_MUX_DMIC)
				mt6358_dmic_disable(priv);
			else
				mt6358_amic_disable(priv);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int mt_mic_type_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol,
			     int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);

	dev_dbg(priv->dev, "%s(), event 0x%x, mux %u\n", __func__, event, mux);

	switch (event) {
	case SND_SOC_DAPM_WILL_PMU:
		priv->mux_select[MUX_MIC_TYPE] = mux;
		break;
	case SND_SOC_DAPM_PRE_PMU:
		switch (mux) {
		case MIC_TYPE_MUX_DMIC:
			mt6358_dmic_enable(priv);
			break;
		default:
			mt6358_amic_enable(priv);
			break;
		}

		break;
	case SND_SOC_DAPM_POST_PMD:
		switch (priv->mux_select[MUX_MIC_TYPE]) {
		case MIC_TYPE_MUX_DMIC:
			mt6358_dmic_disable(priv);
			break;
		default:
			mt6358_amic_disable(priv);
			break;
		}

		priv->mux_select[MUX_MIC_TYPE] = mux;
		break;
	default:
		break;
	}

	return 0;
}

static int mt_adc_l_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);

	dev_dbg(priv->dev, "%s(), event = 0x%x, mux %u\n",
		__func__, event, mux);

	priv->mux_select[MUX_ADC_L] = mux;

	return 0;
}

static int mt_adc_r_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);

	dev_dbg(priv->dev, "%s(), event = 0x%x, mux %u\n",
		__func__, event, mux);

	priv->mux_select[MUX_ADC_R] = mux;

	return 0;
}

static int mt_pga_left_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol,
			     int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);

	dev_dbg(priv->dev, "%s(), event = 0x%x, mux %u\n",
		__func__, event, mux);

	priv->mux_select[MUX_PGA_L] = mux;

	return 0;
}

static int mt_pga_right_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);

	dev_dbg(priv->dev, "%s(), event = 0x%x, mux %u\n",
		__func__, event, mux);

	priv->mux_select[MUX_PGA_R] = mux;

	return 0;
}

static int mt_delay_250_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(250, 270);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		usleep_range(250, 270);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_dc_trim_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	struct dc_trim_data *dc_trim = &priv->dc_trim;

	dev_dbg(priv->dev, "%s(), event = 0x%x, dc_trim->calibrated %u\n",
		__func__, event, dc_trim->calibrated);

	if (dc_trim->calibrated)
		return 0;

	kthread_run(dc_trim_thread, priv, "dc_trim_thread");
	return 0;
}

static int mt_dmic_connect(struct snd_soc_dapm_widget *source,
			   struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	return priv->mux_select[MUX_MIC_TYPE] == MIC_TYPE_MUX_DMIC ?
	       true : false;
}

static int mt_amic_connect(struct snd_soc_dapm_widget *source,
			   struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	return priv->mux_select[MUX_MIC_TYPE] != MIC_TYPE_MUX_DMIC ?
	       true : false;
}

/* DAPM Widgets */
static const struct snd_soc_dapm_widget mt6358_dapm_widgets[] = {
	/* Global Supply*/
	SND_SOC_DAPM_SUPPLY_S("CLK_BUF", SUPPLY_SEQ_CLK_BUF,
			      MT6358_DCXO_CW14,
			      RG_XO_AUDIO_EN_M_SFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDGLB", SUPPLY_SEQ_AUD_GLB,
			      MT6358_AUDDEC_ANA_CON13,
			      RG_AUDGLB_PWRDN_VA28_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLKSQ Audio", SUPPLY_SEQ_CLKSQ,
			      MT6358_AUDENC_ANA_CON6,
			      RG_CLKSQ_EN_SFT, 0,
			      mt_clksq_event,
			      SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY_S("AUDNCP_CK", SUPPLY_SEQ_TOP_CK,
			      MT6358_AUD_TOP_CKPDN_CON0,
			      RG_AUDNCP_CK_PDN_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ZCD13M_CK", SUPPLY_SEQ_TOP_CK,
			      MT6358_AUD_TOP_CKPDN_CON0,
			      RG_ZCD13M_CK_PDN_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUD_CK", SUPPLY_SEQ_TOP_CK_LAST,
			      MT6358_AUD_TOP_CKPDN_CON0,
			      RG_AUD_CK_PDN_SFT, 1,
			      mt_delay_250_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY_S("AUDIF_CK", SUPPLY_SEQ_TOP_CK,
			      MT6358_AUD_TOP_CKPDN_CON0,
			      RG_AUDIF_CK_PDN_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("MIC_BIAS", SUPPLY_SEQ_MIC_BIAS,
			      SND_SOC_NOPM, 0, 0,
			      mt_mic_bias_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* vow */
	SND_SOC_DAPM_SUPPLY_S("AUD_VOW", SUPPLY_SEQ_AUD_VOW,
			      MT6358_AUDENC_ANA_CON1,
			      RG_AUDIO_VOW_EN_SFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("VOW_CLK", SUPPLY_SEQ_VOW_CLK,
			      MT6358_DCXO_CW13,
			      RG_XO_VOW_EN_SFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("VOW_LDO", SUPPLY_SEQ_VOW_LDO,
			      SND_SOC_NOPM, 0, 0,
			      mt_vow_ldo_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Digital Clock */
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_AFE_CTL", SUPPLY_SEQ_AUD_TOP_LAST,
			      MT6358_AUDIO_TOP_CON0,
			      PDN_AFE_CTL_SFT, 1,
			      mt_delay_250_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_DAC_CTL", SUPPLY_SEQ_AUD_TOP,
			      MT6358_AUDIO_TOP_CON0,
			      PDN_DAC_CTL_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_ADC_CTL", SUPPLY_SEQ_AUD_TOP,
			      MT6358_AUDIO_TOP_CON0,
			      PDN_ADC_CTL_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_I2S_DL", SUPPLY_SEQ_AUD_TOP,
			      MT6358_AUDIO_TOP_CON0,
			      PDN_I2S_DL_CTL_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_PWR_CLK", SUPPLY_SEQ_AUD_TOP,
			      MT6358_AUDIO_TOP_CON0,
			      PWR_CLK_DIS_CTL_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_PDN_AFE_TESTMODEL", SUPPLY_SEQ_AUD_TOP,
			      MT6358_AUDIO_TOP_CON0,
			      PDN_AFE_TESTMODEL_CTL_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_PDN_RESERVED", SUPPLY_SEQ_AUD_TOP,
			      MT6358_AUDIO_TOP_CON0,
			      PDN_RESERVED_SFT, 1, NULL, 0),

	SND_SOC_DAPM_SUPPLY("DL Digital Clock", SND_SOC_NOPM,
			    0, 0, NULL, 0),

	/* AFE ON */
	SND_SOC_DAPM_SUPPLY_S("AFE_ON", SUPPLY_SEQ_AFE,
			      MT6358_AFE_UL_DL_CON0, AFE_ON_SFT, 0,
			      NULL, 0),

	/* AIF Rx*/
	SND_SOC_DAPM_AIF_IN_E("AIF_RX", "AIF1 Playback", 0,
			      MT6358_AFE_DL_SRC2_CON0_L,
			      DL_2_SRC_ON_TMP_CTL_PRE_SFT, 0,
			      mt_aif_in_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* DL Supply */
	SND_SOC_DAPM_SUPPLY("DL Power Supply", SND_SOC_NOPM,
			    0, 0, NULL, 0),

	/* DAC */
	SND_SOC_DAPM_MUX("DAC In Mux", SND_SOC_NOPM, 0, 0, &dac_in_mux_control),

	SND_SOC_DAPM_DAC("DACL", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_DAC("DACR", NULL, SND_SOC_NOPM, 0, 0),

	/* LOL */
	SND_SOC_DAPM_MUX_E("LOL Mux", SND_SOC_NOPM, 0, 0,
			   &lo_in_mux_control,
			   mt_lo_event,
			   SND_SOC_DAPM_PRE_PMU |
			   SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("LO Stability Enh", MT6358_AUDDEC_ANA_CON7,
			    RG_LOOUTPUTSTBENH_VAUDP15_SFT, 0, NULL, 0),

	SND_SOC_DAPM_OUT_DRV("LOL Buffer", MT6358_AUDDEC_ANA_CON7,
			     RG_AUDLOLPWRUP_VAUDP15_SFT, 0, NULL, 0),

	/* Headphone */
	SND_SOC_DAPM_MUX_E("HPL Mux", SND_SOC_NOPM, 0, 0,
			   &hpl_in_mux_control,
			   mt_hp_event,
			   SND_SOC_DAPM_PRE_PMU |
			   SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MUX_E("HPR Mux", SND_SOC_NOPM, 0, 0,
			   &hpr_in_mux_control,
			   mt_hp_event,
			   SND_SOC_DAPM_PRE_PMU |
			   SND_SOC_DAPM_PRE_PMD),

	/* Receiver */
	SND_SOC_DAPM_MUX_E("RCV Mux", SND_SOC_NOPM, 0, 0,
			   &rcv_in_mux_control,
			   mt_rcv_event,
			   SND_SOC_DAPM_PRE_PMU |
			   SND_SOC_DAPM_PRE_PMD),

	/* Outputs */
	SND_SOC_DAPM_OUTPUT("Receiver"),
	SND_SOC_DAPM_OUTPUT("Headphone L"),
	SND_SOC_DAPM_OUTPUT("Headphone R"),
	SND_SOC_DAPM_OUTPUT("Headphone L Ext Spk Amp"),
	SND_SOC_DAPM_OUTPUT("Headphone R Ext Spk Amp"),
	SND_SOC_DAPM_OUTPUT("LINEOUT L"),
	SND_SOC_DAPM_OUTPUT("LINEOUT L HSSPK"),

	/* SGEN */
	SND_SOC_DAPM_SUPPLY("SGEN DL Enable", MT6358_AFE_SGEN_CFG0,
			    SGEN_DAC_EN_CTL_SFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("SGEN MUTE", MT6358_AFE_SGEN_CFG0,
			    SGEN_MUTE_SW_CTL_SFT, 1,
			    mt_sgen_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("SGEN DL SRC", MT6358_AFE_DL_SRC2_CON0_L,
			    DL_2_SRC_ON_TMP_CTL_PRE_SFT, 0, NULL, 0),
			/* tricky, same reg/bit as "AIF_RX", reconsider */

	SND_SOC_DAPM_INPUT("SGEN DL"),

	/* Uplinks */
	SND_SOC_DAPM_AIF_OUT_E("AIF1TX", "AIF1 Capture", 0,
			       SND_SOC_NOPM, 0, 0,
			       mt_aif_out_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("ADC Supply", SUPPLY_SEQ_ADC_SUPPLY,
			      SND_SOC_NOPM, 0, 0,
			      mt_adc_supply_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Uplinks MUX */
	SND_SOC_DAPM_MUX("AIF Out Mux", SND_SOC_NOPM, 0, 0,
			 &aif_out_mux_control),

	SND_SOC_DAPM_MUX_E("Mic Type Mux", SND_SOC_NOPM, 0, 0,
			   &mic_type_mux_control,
			   mt_mic_type_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD |
			   SND_SOC_DAPM_WILL_PMU),

	SND_SOC_DAPM_MUX_E("ADC L Mux", SND_SOC_NOPM, 0, 0,
			   &adc_left_mux_control,
			   mt_adc_l_event,
			   SND_SOC_DAPM_WILL_PMU),
	SND_SOC_DAPM_MUX_E("ADC R Mux", SND_SOC_NOPM, 0, 0,
			   &adc_right_mux_control,
			   mt_adc_r_event,
			   SND_SOC_DAPM_WILL_PMU),

	SND_SOC_DAPM_ADC("ADC L", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC R", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MUX_E("PGA L Mux", SND_SOC_NOPM, 0, 0,
			   &pga_left_mux_control,
			   mt_pga_left_event,
			   SND_SOC_DAPM_WILL_PMU),
	SND_SOC_DAPM_MUX_E("PGA R Mux", SND_SOC_NOPM, 0, 0,
			   &pga_right_mux_control,
			   mt_pga_right_event,
			   SND_SOC_DAPM_WILL_PMU),

	SND_SOC_DAPM_PGA("PGA L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PGA R", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* UL input */
	SND_SOC_DAPM_INPUT("AIN0"),
	SND_SOC_DAPM_INPUT("AIN1"),
	SND_SOC_DAPM_INPUT("AIN2"),

	SND_SOC_DAPM_INPUT("AIN0_DMIC"),
	SND_SOC_DAPM_INPUT("AIN3_DMIC"),

	/* DC trim : trigger dc trim flow because set the reg when init_reg */
	/* this must be at the last widget */
	SND_SOC_DAPM_SUPPLY("DC Trim", MT6358_AUDDEC_ANA_CON8,
			    RG_AUDTRIMBUF_EN_VAUDP15_SFT, 0,
			    mt_dc_trim_event, SND_SOC_DAPM_POST_PMD),

	/* VOW */
	SND_SOC_DAPM_AIF_OUT_E("VOW TX", "VOW Capture", 0,
			       SND_SOC_NOPM, 0, 0,
			       mt_vow_out_event,
			       SND_SOC_DAPM_WILL_PMU |
			       SND_SOC_DAPM_PRE_PMU |
			       SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route mt6358_dapm_routes[] = {
	/* Capture */
	{"AIF1TX", NULL, "AIF Out Mux"},
	{"AIF1TX", NULL, "CLK_BUF"},
	{"AIF1TX", NULL, "AUDGLB"},
	{"AIF1TX", NULL, "CLKSQ Audio"},
	{"AIF1TX", NULL, "AUD_CK"},
	{"AIF1TX", NULL, "AUDIF_CK"},
	{"AIF1TX", NULL, "AUDIO_TOP_AFE_CTL"},
	{"AIF1TX", NULL, "AUDIO_TOP_ADC_CTL"},
	{"AIF1TX", NULL, "AUDIO_TOP_PWR_CLK"},
	{"AIF1TX", NULL, "AUDIO_TOP_PDN_RESERVED"},
	{"AIF1TX", NULL, "AUDIO_TOP_I2S_DL"},
	{"AIF1TX", NULL, "AFE_ON"},

	{"AIF Out Mux", NULL, "Mic Type Mux"},
	{"AIF Out Mux", NULL, "AIN0_DMIC"},
	{"AIF Out Mux", NULL, "AIN2_DMIC"},
	{"AIF Out Mux", NULL, "ADC L"},
	{"AIF Out Mux", NULL, "ADC R"},

	{"ADC L", NULL, "ADC L Mux"},
	{"ADC L", NULL, "ADC Supply"},
	{"ADC R", NULL, "ADC R Mux"},
	{"ADC R", NULL, "ADC Supply"},

	{"ADC L Mux", "Left Preamplifier", "PGA L"},
	{"ADC R Mux", "Right Preamplifier", "PGA R"},

	{"PGA L", NULL, "PGA L Mux"},
	{"PGA R", NULL, "PGA R Mux"},

	{"PGA L Mux", "AIN0", "AIN0"},
	{"PGA L Mux", "AIN1", "AIN1"},
	{"PGA L Mux", "AIN2", "AIN2"},

	{"PGA R Mux", "AIN0", "AIN0"},
	{"PGA R Mux", "AIN1", "AIN1"},
	{"PGA R Mux", "AIN2", "AIN2"},

	/* DL Supply */
	{"DL Power Supply", NULL, "CLK_BUF"},
	{"DL Power Supply", NULL, "AUDGLB"},
	{"DL Power Supply", NULL, "CLKSQ Audio"},
	{"DL Power Supply", NULL, "AUDNCP_CK"},
	{"DL Power Supply", NULL, "ZCD13M_CK"},
	{"DL Power Supply", NULL, "AUD_CK"},
	{"DL Power Supply", NULL, "AUDIF_CK"},

	/* DL Digital Supply */
	{"DL Digital Clock", NULL, "AUDIO_TOP_AFE_CTL"},
	{"DL Digital Clock", NULL, "AUDIO_TOP_DAC_CTL"},
	{"DL Digital Clock", NULL, "AUDIO_TOP_PWR_CLK"},
	{"DL Digital Clock", NULL, "AFE_ON"},

	{"AIF_RX", NULL, "DL Digital Clock"},

	/* DL Path */
	{"DAC In Mux", "Normal Path", "AIF_RX"},
	{"DAC In Mux", "Sgen", "SGEN DL"},

	{"SGEN DL", NULL, "SGEN DL SRC"},
	{"SGEN DL", NULL, "SGEN MUTE"},
	{"SGEN DL", NULL, "SGEN DL Enable"},
	{"SGEN DL", NULL, "DL Digital Clock"},
	{"SGEN DL", NULL, "AUDIO_TOP_PDN_AFE_TESTMODEL"},

	{"DACL", NULL, "DAC In Mux"},
	{"DACL", NULL, "DL Power Supply"},

	{"DACR", NULL, "DAC In Mux"},
	{"DACR", NULL, "DL Power Supply"},

	/* Lineout Path */
	{"LOL Mux", "Playback", "DACR"},

	{"LOL Buffer", NULL, "LOL Mux"},
	{"LOL Buffer", NULL, "LO Stability Enh"},

	{"LINEOUT L", NULL, "LOL Buffer"},

	/* Headphone Path */
	{"HPL Mux", "Audio Playback", "DACL"},
	{"HPR Mux", "Audio Playback", "DACR"},
	{"HPL Mux", "HP Impedance", "DACL"},
	{"HPR Mux", "HP Impedance", "DACR"},
	{"HPL Mux", "LoudSPK Playback", "DACL"},
	{"HPR Mux", "LoudSPK Playback", "DACR"},
	{"HPL Mux", "Loud DualSPK Playback", "DACL"},
	{"HPR Mux", "Loud DualSPK Playback", "DACR"},

	{"Headphone L", NULL, "HPL Mux"},
	{"Headphone R", NULL, "HPR Mux"},
	{"Headphone L Ext Spk Amp", NULL, "HPL Mux"},
	{"Headphone R Ext Spk Amp", NULL, "HPR Mux"},
	{"LINEOUT L HSSPK", NULL, "HPL Mux"},

	/* Receiver Path */
	{"RCV Mux", "Voice Playback", "DACL"},
	{"Receiver", NULL, "RCV Mux"},

	/* VOW */
	{"VOW TX", NULL, "Mic Type Mux"},
	{"VOW TX", NULL, "CLK_BUF"},
	{"VOW TX", NULL, "AUDGLB"},
	{"VOW TX", NULL, "AUD_CK"},
	{"VOW TX", NULL, "VOW_CLK"},
	{"VOW TX", NULL, "AUD_VOW"},
	{"VOW TX", NULL, "VOW_LDO"},
	{"VOW TX", NULL, "ADC L"},
	{"VOW TX", NULL, "AIN0_DMIC"},

	/* mic bias */
	{"AIN0", NULL, "MIC_BIAS", mt_amic_connect},
	{"AIN1", NULL, "MIC_BIAS", mt_amic_connect},
	{"AIN2", NULL, "MIC_BIAS", mt_amic_connect},
	{"AIN0_DMIC", NULL, "MIC_BIAS", mt_dmic_connect},
	{"AIN2_DMIC", NULL, "MIC_BIAS", mt_dmic_connect},
};

static int mt6358_codec_dai_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params,
				      struct snd_soc_dai *dai)
{
	struct snd_soc_component *cmpnt = dai->component;
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int rate = params_rate(params);


	dev_info(priv->dev, "%s(), substream->stream %d, rate %d, number %d\n",
		 __func__,
		 substream->stream,
		 rate,
		 substream->number);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		priv->dl_rate = rate;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		priv->ul_rate = rate;

	return 0;
}

static const struct snd_soc_dai_ops mt6358_codec_dai_ops = {
	.hw_params = mt6358_codec_dai_hw_params,
};

#define MT6358_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |\
			SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_U16_BE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE |\
			SNDRV_PCM_FMTBIT_U24_LE | SNDRV_PCM_FMTBIT_U24_BE |\
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE |\
			SNDRV_PCM_FMTBIT_U32_LE | SNDRV_PCM_FMTBIT_U32_BE)

static struct snd_soc_dai_driver mt6358_dai_driver[] = {
	{
		.name = "mt6358-snd-codec-aif1",
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000 |
				 SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = MT6358_FORMATS,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000,
			.formats = MT6358_FORMATS,
		},
		.ops = &mt6358_codec_dai_ops,
	},
	{
		.name = "mt6358-snd-codec-vow",
		.capture = {
			.stream_name = "VOW Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_16000,
			.formats = MT6358_FORMATS,
		},
	},
};

/* trim buffer */
enum {
	TRIM_BUF_MUX_OPEN = 0,
	TRIM_BUF_MUX_HPL,
	TRIM_BUF_MUX_HPR,
	TRIM_BUF_MUX_HSP,
	TRIM_BUF_MUX_HSN,
	TRIM_BUF_MUX_LOLP,
	TRIM_BUF_MUX_LOLN,
	TRIM_MUX_AU_REFN,
	TRIM_MUX_AVSS28,
	TRIM_MUX_AVSS28_2,
	TRIM_MUX_UNUSED,
	TRIM_BUF_MUX_GROUND,
};

enum {
	TRIM_BUF_GAIN_0DB = 0,
	TRIM_BUF_GAIN_6DB,
	TRIM_BUF_GAIN_12DB,
	TRIM_BUF_GAIN_18DB,
};

static void set_trim_buf_in_mux(struct mt6358_priv *priv, int mux)
{
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON8,
			    RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP15_MASK,
			    mux);
}

static void set_trim_buf_gain(struct mt6358_priv *priv, unsigned int gain)
{
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON8,
			   RG_AUDTRIMBUF_GAINSEL_VAUDP15_MASK_SFT,
			   gain << RG_AUDTRIMBUF_GAINSEL_VAUDP15_SFT);
}

static void enable_trim_buf(struct mt6358_priv *priv, bool enable)
{
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON8,
			   RG_AUDTRIMBUF_EN_VAUDP15_MASK_SFT,
			   (enable ? 1 : 0) << RG_AUDTRIMBUF_EN_VAUDP15_SFT);
}

/* 1 / (10 ^ (dB / 20)) * db_denominator */
static const int db_denominator = 8192;
static const int db_numerator[32] = {
	3261, 3659, 4106, 4607,
	5169, 5799, 6507, 7301,
	8192, 9192, 10313, 11572,
	12983, 14568, 16345, 18340,
	20577, 23088, 25905, 819200,
	819200, 819200, 819200, 819200,
	819200, 819200, 819200, 819200,
	819200, 819200, 819200, 819200,
};

enum {
	MIC_BIAS_1P7 = 0,
	MIC_BIAS_1P8,
	MIC_BIAS_1P9,
	MIC_BIAS_2P0,
	MIC_BIAS_2P1,
	MIC_BIAS_2P5,
	MIC_BIAS_2P6,
	MIC_BIAS_2P7,
};

#ifndef ANALOG_HPTRIM
static int get_mic_bias_mv(struct mt6358_priv *priv)
{
	unsigned int mic_bias;

	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON10, &mic_bias);
	mic_bias = (mic_bias >> 4) & 0x7;

	switch (mic_bias) {
	case MIC_BIAS_1P7:
		return 1700;
	case MIC_BIAS_1P8:
		return 1800;
	case MIC_BIAS_1P9:
		return 1900;
	case MIC_BIAS_2P0:
		return 2000;
	case MIC_BIAS_2P1:
		return 2100;
	case MIC_BIAS_2P5:
		return 2500;
	case MIC_BIAS_2P6:
		return 2600;
	case MIC_BIAS_2P7:
		return 2700;
	default:
		dev_warn(priv->dev, "%s(), invalid mic_bias %d\n",
			 __func__, mic_bias);
		return 2600;
	};
}

static int convert_offset_to_comp(struct mt6358_priv *priv,
				  int offset, int vol_type)
{
	int gain = priv->ana_gain[vol_type];
	int mic_vinp_mv = priv->dc_trim.mic_vinp_mv;
	int real_mic_vinp_mv;
	int mic_bias_mv;
	int offset_scale = DIV_ROUND_CLOSEST(offset * db_numerator[gain],
					     db_denominator);

	if (mic_vinp_mv > MIC_VINP_4POLE_THRES_MV &&
	    ((priv->debug_flag & DBG_DCTRIM_BYPASS_4POLE) == 0)) {
		int v_diff_bias_vinp;
		int v_diff_bias_vinp_scale;

		/* refine mic bias influence on 4 pole headset */
		mic_bias_mv = get_mic_bias_mv(priv);
		real_mic_vinp_mv = DIV_ROUND_CLOSEST(mic_vinp_mv * mic_bias_mv,
						     VINP_NORMALIZED_TO_MV);

		v_diff_bias_vinp = mic_bias_mv - real_mic_vinp_mv;
		v_diff_bias_vinp_scale = DIV_ROUND_CLOSEST((v_diff_bias_vinp) *
							   db_numerator[gain],
							   db_denominator);

		if ((priv->debug_flag & DBG_DCTRIM_4POLE_LOG) != 0) {
			dev_info(priv->dev, "%s(), mic_bias_mv %d, mic_vinp_mv %d, real_mic_vinp_mv %d\n",
				 __func__,
				 mic_bias_mv, mic_vinp_mv, real_mic_vinp_mv);

			dev_info(priv->dev, "%s(), a %d, b %d\n", __func__,
				 DIV_ROUND_CLOSEST(offset_scale * 2804225,
						   32768),
				 DIV_ROUND_CLOSEST(v_diff_bias_vinp_scale *
						   1782,
						   1800));
		}

		return DIV_ROUND_CLOSEST(offset_scale * 2804225, 32768) -
		       DIV_ROUND_CLOSEST(v_diff_bias_vinp_scale * 1782, 1800);
	} else {
		/* The formula is from DE programming guide */
		/* should be mantain by pmic owner */
		return DIV_ROUND_CLOSEST(offset_scale * 2804225, 32768);
	}
}

static int get_dc_ramp_step(int gain)
{
	/* each step should be smaller than 100uV */
	/* 1 pcm of dc compensation = 0.0808uV HP buffer voltage @ 0dB*/
	/* 80uV / 0.0808uV(0dB) = 990.099 */
	int step_0db = 990;

	/* scale for specific gain */
	return step_0db * db_numerator[gain] / db_denominator;
}

static int apply_dc_compensation(struct mt6358_priv *priv, bool enable)
{
	struct mt6358_codec_ops *ops = &priv->ops;
	struct dc_trim_data *dc_trim = &priv->dc_trim;

	int lch_value = 0, rch_value = 0, tmp_ramp = 0;
	int times = 0, i = 0;
	int sign_lch = 0, sign_rch = 0;
	int abs_lch = 0, abs_rch = 0;
	int diff_lch = 0, diff_rch = 0, ramp_l = 0, ramp_r = 0;
	int index_lgain = priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL];
	int ramp_step = get_dc_ramp_step(index_lgain);
	int *pre_lch_comp = &dc_trim->pre_comp_value[CH_L];
	int *pre_rch_comp = &dc_trim->pre_comp_value[CH_R];

	if (ops->enable_dc_compensation == NULL ||
	    ops->set_lch_dc_compensation == NULL ||
	    ops->set_rch_dc_compensation == NULL) {
		dev_warn(priv->dev, "%s(), function not ready, enable %p, lch %p, rch %p\n",
			 __func__,
			 ops->enable_dc_compensation,
			 ops->set_lch_dc_compensation,
			 ops->set_rch_dc_compensation);
		return -EFAULT;
	}

	if (enable && index_lgain == DL_GAIN_N_40DB) {
		dev_info(priv->dev, "%s(), -40dB skip dc compensation\n",
			 __func__);
		return 0;
	}

	lch_value = convert_offset_to_comp(priv, dc_trim->hp_offset[CH_L],
					   AUDIO_ANALOG_VOLUME_HPOUTL);
	rch_value = convert_offset_to_comp(priv, dc_trim->hp_offset[CH_R],
					   AUDIO_ANALOG_VOLUME_HPOUTR);

	diff_lch = enable ? lch_value - *pre_lch_comp : lch_value;
	diff_rch = enable ? rch_value - *pre_rch_comp : rch_value;
	sign_lch = diff_lch < 0 ? -1 : 1;
	sign_rch = diff_rch < 0 ? -1 : 1;
	abs_lch = sign_lch * diff_lch;
	abs_rch = sign_rch * diff_rch;
	times = abs_lch > abs_rch ?
		(abs_lch / ramp_step) : (abs_rch / ramp_step);

	dev_info(priv->dev, "%s(), enable = %d, index_gain = %d, times = %d, lch_value = %d -> %d, rch_value = %d -> %d, ramp_step %d, mic_vinp_mv %d\n",
		 __func__, enable, index_lgain, times,
		 *pre_lch_comp, lch_value,
		 *pre_rch_comp, rch_value, ramp_step, dc_trim->mic_vinp_mv);

	if (enable) {
		ops->enable_dc_compensation(true);
		for (i = 1; i <= times; i++) {
			tmp_ramp = i * ramp_step;
			if (tmp_ramp < abs_lch) {
				ramp_l = *pre_lch_comp + sign_lch * tmp_ramp;
				ops->set_lch_dc_compensation(ramp_l << 8);
			}
			if (tmp_ramp < abs_rch) {
				ramp_r = *pre_rch_comp + sign_rch * tmp_ramp;
				ops->set_rch_dc_compensation(ramp_r << 8);
			}
			udelay(600);
		}
		ops->set_lch_dc_compensation(lch_value << 8);
		ops->set_rch_dc_compensation(rch_value << 8);
		*pre_lch_comp = lch_value;
		*pre_rch_comp = rch_value;
	} else {
		for (i = times; i >= 0; i--) {
			tmp_ramp = i * ramp_step;
			if (tmp_ramp < abs_lch) {
				ramp_l = sign_lch * tmp_ramp;
				ops->set_lch_dc_compensation(ramp_l << 8);
			}

			if (tmp_ramp < abs_rch) {
				ramp_r = sign_rch * tmp_ramp;
				ops->set_rch_dc_compensation(ramp_r << 8);
			}
			udelay(600);
		}
		ops->set_lch_dc_compensation(0);
		ops->set_rch_dc_compensation(0);
		ops->enable_dc_compensation(false);
		*pre_lch_comp = 0;
		*pre_rch_comp = 0;
	}

	return 0;
}
#endif

#ifndef CONFIG_FPGA_EARLY_PORTING
static int calculate_trim_result(struct mt6358_priv *priv,
		int *on_value, int *off_value,
		int trim_times, int discard_num, int useful_num)
{
	int i = 0, j = 0, tmp = 0, offset = 0;
	/* sort */
	for (i = 0; i < trim_times - 1; i++) {
		for (j = 0; j < trim_times - 1 - i; j++) {
			if (on_value[j] > on_value[j + 1]) {
				tmp = on_value[j + 1];
				on_value[j + 1] = on_value[j];
				on_value[j] = tmp;
			}
			if (off_value[j] > off_value[j + 1]) {
				tmp = off_value[j + 1];
				off_value[j + 1] = off_value[j];
				off_value[j] = tmp;
			}
		}
	}
	/* calculate result */
	for (i = discard_num; i < trim_times - discard_num; i++) {
		offset += on_value[i] - off_value[i];
		dev_info(priv->dev, "%s(), offset diff = %d, on = %d, off = %d\n",
			 __func__,
			 on_value[i] - off_value[i], on_value[i], off_value[i]);
	}
	return DIV_ROUND_CLOSEST(offset, useful_num);
}

static void start_trim_hardware(struct mt6358_priv *priv, bool buffer_on)
{
	dev_info(priv->dev, "%s(), ++\n", __func__);

	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON10,
			0xff, 0xa0);

	/* Set playback gpio (mosi/clk/sync) */
	playback_gpio_set(priv);

	/* Enable AUDGLB */
	mt6358_set_aud_global_bias(priv, true);

	/* Pull-down HPL/R to AVSS30_AUD */
	hp_pull_down(priv, true);

	/* release HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			   0x1 << 6, 0x1 << 6);

	/* enable clk buf */
	mt6358_set_dcxo(priv, true);

	// TODO: MT6771 no
#if 0
	/* Enable HP main CMFB Switch */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0200);
	/* Pull-down HPL/R, HS, LO to AVSS28_AUD */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON10, 0x00a8);
	/* Dsiable HP main CMFB Switch */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0000);
	/* Release HP CMFB pull down */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON10, 0x00a0);
#endif

	/* Enable CLKSQ 26MHz */
	mt6358_set_clksq(priv, true);

	/* Enable ZCD13M_CK, AUD_CK, AUDIF_CK, AUDNCP_CK*/
	mt6358_set_topck(priv, true);
	usleep_range(250, 270);

	regmap_update_bits(priv->regmap, MT6358_AUDIO_TOP_CON0, 0x00c4, 0x0);
	usleep_range(250, 270);

	/* sdm audio fifo clock power on */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0006);
	/* scrambler clock on enable */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON0, 0xCBA1);
	/* sdm power on */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0003);
	/* sdm fifo enable */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x000B);

	/* afe enable */
	regmap_update_bits(priv->regmap, MT6358_AFE_UL_DL_CON0,
			AFE_ON_MASK_SFT, 0x1);
	/* turn on dl */
	regmap_write(priv->regmap, MT6358_AFE_DL_SRC2_CON0_L, 0x1);
	/* set DL in normal path, not from sine gen table */
	regmap_write(priv->regmap, MT6358_AFE_TOP_CON0, 0x0);

	/* sdm output mute enable */
	/* regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON1, 0x0000); */
	/* regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON0, 0xcbad); */

	/* The above is turning on DAC */

	/* Disable headphone short-circuit protection */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x3000);
	/* Disable handset short-circuit protection */
	/* regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0010); */
	/* Disable linout short-circuit protection */
	/* regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON7, 0x0010); */
	/* Reduce ESD resistance of AU_REFN */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0xc000);
	/* Set HPL/HPR gain to -10dB*/
	regmap_write(priv->regmap, MT6358_ZCD_CON2, DL_GAIN_N_10DB_REG);

	/* Turn on DA_600K_NCP_VA18 */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON1, 0x0001);
	/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON2, 0x002c);
	/* Toggle RG_DIVCKS_CHG */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON0, 0x0001);
	/* Set NCP soft start mode as default mode: 150us */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON4, 0x0002);
	/* Enable NCP */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x0000);
	usleep_range(250, 270);

	/* Enable cap-less LDOs (1.5V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
			0x1055, 0x1055);
	/* Enable NV regulator (-1.2V) */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x0001);
	usleep_range(100, 120);

	/* Enable AUD_ZCD */
	zcd_enable(priv, true, DEVICE_HP);

	/* Enable IBIST */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
	/* Set HP DR bias current optimization, 010: 6uA */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON11,
			   0xff80, 0x4900);
	/* Set HP & ZCD bias current optimization */
	/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
	/* Set HPP/N STB enhance circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0xc033);
	if (buffer_on) {
		/* Enable HP aux output stage */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x000c);
		/* Enable HP aux feedback loop */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x003c);
		/* Enable HP aux CMFB loop */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0c80);

		/* Enable HP driver bias circuits */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x30c0);
		/* Enable HP driver core circuits */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x30f0);
		/* Short HP main output to HP aux output stage */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
				0x00ff, 0x00fc);
		/* Enable HP main CMFB loop */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0e80);
		/* Disable HP aux CMFB loop */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0280);

		/* Select CMFB resistor bulk to AC mode */
		/* Selec HS/LO cap size (6.5pF default) */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON10, 0x0000);

		/* Enable HS driver bias circuits */
		/* Disable HS main output stage */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0010);
		/* Enable LO driver bias circuits */
		/* Disable LO main output stage */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON7, 0x0010);

		/* Enable HP main output stage */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
				0x00ff, 0x00ff);
		/* Enable HPR/L main output stage step by step */
		hp_main_output_ramp(priv, true);
		/* Reduce HP aux feedback loop gain step by step */
		hp_aux_feedback_loop_gain_ramp(priv, true);
		/* Disable HP aux feedback loop */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fcf);
		/* Increase HPL/HPR gain to normal gain step by step */
		headset_volume_ramp(priv,
				DL_GAIN_N_10DB,
				priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL]);
		/* Disable HP aux output stage */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fc3);
		/* Unshort HP main output to HP aux output stage */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3f03);
		udelay(1000);

		/* Disable AUD_CLK */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13,
				0x1, 0x0);
		/* Disable Audio DAC */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
				0xf, 0x0);
		/* Disable low-noise mode of DAC*/
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
				0x1, 0x0);

		/* Switch HPL/HPR MUX to open */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
				0x0f00, 0x0000);

		/* Disable Pull-down HPL/R to AVSS28_AUD*/
		hp_pull_down(priv, false);

		/* Enable Trim buffer VA28 reference */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
				0x1 << 1, 0x1 << 1);
	} else {
		/* Enable HP driver bias circuits */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x30c0);
		/* Disable HP main output stage */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x0000);
		/* Enable HS driver bias circuits */
		/* Disable HS main output stage */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0012);
		/* Enable LO driver bias circuits */
		/* Disable LO main output stage */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON7, 0x0012);
	}
	udelay(100);

	dev_info(priv->dev, "%s(), --\n", __func__);
}

static void stop_trim_hardware(struct mt6358_priv *priv)
{
	dev_info(priv->dev, "%s(), ++\n", __func__);

	hp_pull_down(priv, true);

	/* HPR/HPL mux to open */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0x0f00, 0x0000);
	/* Disable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x1, 0x0);

	/* Disable Audio DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0x000f, 0x0000);
	/* Disable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x0);

	/* Short HP main output to HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fc3);
	/* Enable HP aux output stage*/
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fcf);

	/* decrease HPL/R gain to normal gain step by step */
	headset_volume_ramp(priv,
			priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL],
			DL_GAIN_N_10DB);

	/* Enable HP aux feedback loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fff);
	/* set HP aux feedback loop gain to max step by step */
	hp_aux_feedback_loop_gain_ramp(priv, false);
	/* decrease HPR/L main output stage step by step */
	hp_main_output_ramp(priv, false);
	/* Disable HP main output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3, 0x0);

	/* Enable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x0f00, 0x0e00);
	/* Disable HP main CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x0f00, 0x0c00);
	/* Unshort HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x3 << 6, 0x0);
	/* Disable HP driver core circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x30c0);
	/* Disable HP driver bias circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x3000);
	/* Disable HP aux CMFB loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0);
	/* Open HP aux feedback loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0xc);
	/* Disable HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x0);

	/* Disable IBIST */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON12,
			0x1 << 8, 0x1 << 8);

	/* Disable AUD_ZCD */
	zcd_enable(priv, false, DEVICE_HP);

	/* Disable NV regulator (-1.2V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x1, 0x0);
	/* Disable cap-less LDOs (1.5V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14, 0x1055, 0x0);
	/* Disable NCP */
	regmap_update_bits(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x1, 0x1);

	/* Set HPL/HPR gain to mute */
	regmap_write(priv->regmap, MT6358_ZCD_CON2, DL_GAIN_N_40DB_REG);
	/* Increase ESD resistance of AU_REFN */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON2,
			0x1 << 14, 0x0);

	/* The below is turning off DAC */

	/* Turn off down-link */
	regmap_write(priv->regmap, MT6358_AFE_DL_SRC2_CON0_L, 0x0);

	/* DL scrambler disabling sequence */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0);
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON0, 0xcba0);

	/* turn off afe */
	regmap_update_bits(priv->regmap, MT6358_AFE_UL_DL_CON0,
			AFE_ON_MASK_SFT, 0x0);
	/* all power down */
	regmap_update_bits(priv->regmap, MT6358_AUDIO_TOP_CON0,
			0x00ff, 0x00ff);
	usleep_range(250, 270);

	/* disable ZCD13M_CK, AUD_CK, AUDIF_CK, AUDNCP_CK*/
	mt6358_set_topck(priv, false);
	/* disable CLKSQ 26MHz */
	mt6358_set_clksq(priv, false);

	/* Set HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			   0x1 << 6, 0x0);

	/* Disable Pull-down HPL/R to AVSS30_AUD  */
	hp_pull_down(priv, false);

	/* disable AUDGLB */
	mt6358_set_aud_global_bias(priv, false);

	/* disable clk buf */
	mt6358_set_dcxo(priv, false);

	/* Reset playback gpio (mosi/clk/sync) */
	playback_gpio_reset(priv);

	dev_info(priv->dev, "%s(), --\n", __func__);
}

static void start_trim_hardware_with_lo(struct mt6358_priv *priv,
		bool buffer_on)
{
	/* Set playback gpio (mosi/clk/sync) */
	playback_gpio_set(priv);

	/* Enable AUDGLB */
	mt6358_set_aud_global_bias(priv, true);

	/* Pull-down HPL/R to AVSS30_AUD */
	hp_pull_down(priv, true);

	/* release HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			   0x1 << 6, 0x1 << 6);

	/* enable clk buf */
	mt6358_set_dcxo(priv, true);

	/* Enable CLKSQ 26MHz */
	mt6358_set_clksq(priv, true);

	/* Enable ZCD13M_CK, AUD_CK, AUDIF_CK, AUDNCP_CK*/
	mt6358_set_topck(priv, true);
	usleep_range(250, 270);

	regmap_update_bits(priv->regmap, MT6358_AUDIO_TOP_CON0, 0x00c4, 0x0);
	usleep_range(250, 270);

	/* sdm audio fifo clock power on */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0006);
	/* scrambler clock on enable */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON0, 0xCBA1);
	/* sdm power on */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0003);
	/* sdm fifo enable */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x000B);

	/* afe enable */
	regmap_update_bits(priv->regmap, MT6358_AFE_UL_DL_CON0,
			AFE_ON_MASK_SFT, 0x1);
	/* turn on dl */
	regmap_write(priv->regmap, MT6358_AFE_DL_SRC2_CON0_L, 0x1);
	/* set DL in normal path, not from sine gen table */
	regmap_write(priv->regmap, MT6358_AFE_TOP_CON0, 0x0);

	/* The above is turning on DAC */

	/* HP IVBUF (Vin path) de-gain enable: -12dB */
	if (priv->apply_n12db_gain)
		regmap_update_bits(priv->regmap,
				MT6358_AUDDEC_ANA_CON7,
				0xff, 0x0004);

	/* Audio left headphone input multiplexor selection : LOL*/
	set_hp_l_input_mux(priv, HP_INPUT_MUX_LOL);

	/* Disable headphone short-circuit protection */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			0xf0ff, 0x3000);
	/* Disable lineout short-ckt protection */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1 << 4, 0x1 << 4);

	/* Reduce ESD resistance of AU_REFN */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON2,
			0xffff, 0xc000);

	/* Set HPL/HPR gain to -10dB*/
	regmap_write(priv->regmap, MT6358_ZCD_CON2, DL_GAIN_N_10DB_REG);
	/* Set LOLR/LOLL gain to -10dB */
	regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_10DB_REG);

	/* Turn on DA_600K_NCP_VA18 */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON1, 0x0001);
	/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON2, 0x002c);
	/* Toggle RG_DIVCKS_CHG */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON0, 0x0001);
	/* Set NCP soft start mode as default mode: 150us */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON4, 0x0002);
	/* Enable NCP */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x0000);
	udelay(250);

	/* Enable cap-less LDOs (1.5V), LCLDO local sense */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
			0x1055, 0x1055);
	/* Enable NV regulator (-1.2V) */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x0001);
	udelay(100);

	/* Enable AUD_ZCD */
	zcd_enable(priv, true, DEVICE_HP);

	/* Enable IBIST */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);

	/* Set HP DR bias current optimization, 010: 6uA */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON11,
			   0xff80, 0x4900);
	/* Set HP & ZCD bias current optimization */
	/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
	/* Set HPP/N STB enhance circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0xc033);

	if (buffer_on) {
		if (priv->apply_n12db_gain) {
			/* HP IVBUF (Vin path) de-gain enable: -12dB */
			regmap_update_bits(priv->regmap,
					MT6358_AUDDEC_ANA_CON10,
					0xff, 0x04);
		}
		/* Set LO STB enhance circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				0x1 << 8, 0x1 << 8);
		/* Enable LO driver bias circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				0x1 << 1, 0x1 << 1);
		/* Enable LO driver core circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
				0x1, 0x1);
		/* Set LOL gain to normal gain step by step */
		regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_10DB_REG);

		/* Switch HPL MUX to Line-out */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
				0x3 << 8, 0x01 << 8);
		/* Switch HPR MUX to Line-out */
		/* Ana_Set_Reg(AUDDEC_ANA_CON0, 0x01 << 10, 0x3 << 10); */
		/* Enable HP aux output stage */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
				0xff, 0x0c);
		/* Enable HP aux feedback loop */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
				0xff, 0x3c);
		/* Enable HP aux CMFB loop */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
				0xff00, 0x0c00);
		/* Enable HP driver bias circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
				0xf0, 0xc0);
		/* Enable HP driver core circuits */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
				0xf0, 0xf0);
		/* Short HP main output to HP aux output stage */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
				0xff, 0xfc);
		/* Enable HP main CMFB loop */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
				0xff00, 0x0e00);
		/* Disable HP aux CMFB loop */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
				0xff00, 0x0200);

		/* Select CMFB resistor bulk to AC mode */
		/* Selec HS/LO cap size (6.5pF default) */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON10, 0x0000);

		/* Enable HS driver bias circuits */
		/* Disable HS main output stage */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0010);
		/* Enable LO driver bias circuits */
		/* Disable LO main output stage */
		regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON7, 0x0010);

		/* Enable LO main output stage */
		enable_lo_buffer(priv, true);
		set_speaker_gain(priv, DL_GAIN_0DB);

		/* Enable HP main output stage */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
				0x00ff, 0x00ff);
		/* Enable HPR/L main output stage step by step */
		hp_main_output_ramp(priv, true);
		/* Reduce HP aux feedback loop gain step by step */
		hp_aux_feedback_loop_gain_ramp(priv, true);
		/* Disable HP aux feedback loop */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
				0x00ff, 0x00cf);

		/* Increase HPL/HPR gain to normal gain step by step */
		headset_volume_ramp(priv,
				DL_GAIN_N_10DB,
				priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL]);

		/* Disable HP aux output stage */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
				0x00ff, 0x00c3);
		/* Unshort HP main output to HP aux output stage */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
				0x00ff, 0x0003);
		udelay(1000);

	}
	/* Enable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x1);
	/* Enable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x1, 0x1);
	udelay(100);

	/* Switch LOL MUX to audio DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x3 << 2, 0x2 << 2);
	/* Disable Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, false);

	/* Enable Trim buffer VA28 reference */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x1 << 1, 0x1 << 1);
}

static void stop_trim_hardware_with_lo(struct mt6358_priv *priv)
{
	/* Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true);

	/* Disable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x1, 0x0);
	/* Disable Audio DAC */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0xf, 0x0);
	/* Disable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x0);

	/* Short HP main output to HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fc3);
	/* Enable HP aux output stage*/
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3fcf);

	/* decrease HPL/R gain to normal gain step by step */
	headset_volume_ramp(priv,
			    priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL],
			    DL_GAIN_N_10DB);
	regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_10DB_REG);

	/* set HP aux feedback loop gain to max */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			   0xff00, 0xf200);

	/* Enable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0xff, 0xff);

	/* Reduce HP aux feedback loop gain */
	hp_aux_feedback_loop_gain_ramp(priv, false);

	/* decrease HPR/L main output stage step by step */
	hp_main_output_ramp(priv, false);

	/* Switch HPL/HPR MUX to open */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   0xf << 8, 0x0 << 8);
	/* Switch LOL MUX to open */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			   0x3 << 2, 0x0 << 2);

	/* Disable HP main output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x3, 0x0);
	/* Enable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x0f00, 0x0e00);
	/* Disable HP main CMFB loop */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON9,
			0x0f00, 0x0c00);
	/* Unshort HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON1,
			0x3 << 6, 0x0);
	/* Disable HP driver core circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x30c0);
	/* Disable HP driver bias circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x3000);
	/* Disable HP aux CMFB loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0);
	/* Open HP aux feedback loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0xc);
	/* Disable HP aux output stage */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON1, 0x0);

	/* decrease LOL gain to minimum gain step by step */
	regmap_write(priv->regmap, MT6358_ZCD_CON1, DL_GAIN_N_40DB_REG);
	/* Disable LO driver core circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7, 0x1, 0x0);
	/* Disable LO driver bias circuits */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			0x1 << 1, 0x0 << 1);

	/* Disable IBIST */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON12,
			0x1 << 8, 0x1 << 8);

	/* Disable AUD_ZCD */
	zcd_enable(priv, false, DEVICE_HP);

	/* Disable NV regulator (-1.2V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x1, 0x0);
	/* Disable cap-less LDOs (1.5V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
			0x1055, 0x0);
	/* Disable NCP */
	regmap_update_bits(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x1, 0x1);

	/* Set HPL/HPR gain to mute */
	regmap_write(priv->regmap, MT6358_ZCD_CON2, DL_GAIN_N_40DB_REG);

	/* Increase ESD resistance of AU_REFN */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON2,
			0x1 << 14, 0x0);

	/* The below is turning off DAC */

	/* Turn off down-link */
	regmap_write(priv->regmap, MT6358_AFE_DL_SRC2_CON0_L, 0x0);

	/* DL scrambler disabling sequence */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0);
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON0, 0xcba0);

	/* turn off afe */
	regmap_update_bits(priv->regmap, MT6358_AFE_UL_DL_CON0,
			AFE_ON_MASK_SFT, 0x0);
	/* all power down */
	regmap_update_bits(priv->regmap, MT6358_AUDIO_TOP_CON0,
			0x00ff, 0x00ff);
	usleep_range(250, 270);

	/* disable ZCD13M_CK, AUD_CK, AUDIF_CK, AUDNCP_CK*/
	mt6358_set_topck(priv, false);
	/* disable CLKSQ 26MHz */
	mt6358_set_clksq(priv, false);

	/* Set HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			   0x1 << 6, 0x0);

	/* Disable Pull-down HPL/R to AVSS30_AUD  */
	hp_pull_down(priv, false);

	/* disable AUDGLB */
	mt6358_set_aud_global_bias(priv, false);

	/* disable clk buf */
	mt6358_set_dcxo(priv, false);

	/* Reset playback gpio (mosi/clk/sync) */
	playback_gpio_reset(priv);
}
#endif

#ifdef ANALOG_HPTRIM
#define TRIM_TIMES 7
#else
#define TRIM_TIMES 26
#endif
#define TRIM_DISCARD_NUM 1
#define TRIM_USEFUL_NUM (TRIM_TIMES - (TRIM_DISCARD_NUM * 2))

#ifndef CONFIG_FPGA_EARLY_PORTING
static void hp_trim_offset(struct mt6358_priv *priv)
{
	int on_value_l[TRIM_TIMES], on_value_r[TRIM_TIMES];
	int off_value_l[TRIM_TIMES], off_value_r[TRIM_TIMES];
	int i;

	regmap_update_bits(priv->regmap, MT6358_AUXADC_CON10,
			   0x7, AUXADC_AVG_256);

	/* Step1. turn on buffer */
	start_trim_hardware(priv, true);

	/* L Channel */

	/* Step2. Enable TRIMBUF circuit */
	/* select HPL as TRIMBUF input and set TRIMBUF gain as 18dB  */
	set_trim_buf_in_mux(priv, TRIM_BUF_MUX_HPL);
	set_trim_buf_gain(priv, TRIM_BUF_GAIN_18DB);
	enable_trim_buf(priv, true);

	/* Step3. Request AuxADC to get HP on dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		on_value_l[i] = get_auxadc_audio();

	/* Step4. select AUDREFN as TRIMBUF input */
	set_trim_buf_in_mux(priv, TRIM_MUX_AU_REFN);

	/* Step5. Request AuxADC to get HP off dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		off_value_l[i] = get_auxadc_audio();

	/* Step6. Enable TRIMBUF circuit */
	/* select HPR as TRIMBUF input and set TRIMBUF gain as 18dB */

	/* R Channel */
	set_trim_buf_in_mux(priv, TRIM_BUF_MUX_HPR);

	/* Step7. Request AuxADC to get HP on dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		on_value_r[i] = get_auxadc_audio();

	/* Step8. select AUDREFN as TRIMBUF input */
	set_trim_buf_in_mux(priv, TRIM_MUX_AU_REFN);

	/* Step9. Request AuxADC to get HP off dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		off_value_r[i] = get_auxadc_audio();

	/* Step10. turn off buffer */
	enable_trim_buf(priv, false);
	stop_trim_hardware(priv);
	priv->dc_trim.hp_trim_offset[CH_L] =
			calculate_trim_result(priv, on_value_l, off_value_l,
			TRIM_TIMES, TRIM_DISCARD_NUM, TRIM_USEFUL_NUM);
	priv->dc_trim.hp_trim_offset[CH_R] =
			calculate_trim_result(priv, on_value_r, off_value_r,
			TRIM_TIMES, TRIM_DISCARD_NUM, TRIM_USEFUL_NUM);
	dev_info(priv->dev, "%s(), trim offset L = %d, trim offset R = %d\n",
			__func__,
			priv->dc_trim.hp_trim_offset[CH_L],
			priv->dc_trim.hp_trim_offset[CH_R]);
}
#endif

#if 0
static int hp_trim_offset(struct mt6358_priv *priv, int channel)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#define TRIM_TIMES 26
#define TRIM_DISCARD_NUM 3
#define TRIM_USEFUL_NUM (TRIM_TIMES - (TRIM_DISCARD_NUM * 2))

	int on_value[TRIM_TIMES];
	int off_value[TRIM_TIMES];
	int offset = 0;
	int i, j, tmp;

	if (channel != TRIM_BUF_MUX_HPL &&
	    channel != TRIM_BUF_MUX_HPR){
		dev_warn(priv->dev, "%s(), channel %d not support\n",
			 __func__, channel);
		return 0;
	}

	regmap_update_bits(priv->regmap, MT6358_AUXADC_CON10,
			   0x7, AUXADC_AVG_256);

	/* trimming buffer gain selection 18db*/
	set_trim_buf_gain(priv, TRIM_BUF_GAIN_18DB);
	/* trimming buffer mux selection */
	set_trim_buf_in_mux(priv, channel);

	/* get buffer on auxadc value  */
	start_trim_hardware(priv, true);

	enable_trim_buf(priv, true);
	usleep_range(1 * 1000, 10 * 1000);

	for (i = 0; i < TRIM_TIMES; i++)
		on_value[i] = get_auxadc_audio();

	enable_trim_buf(priv, false);

	stop_trim_hardware(priv);

	/* get buffer off auxadc value */
	start_trim_hardware(priv, false);

	enable_trim_buf(priv, true);

	usleep_range(1 * 1000, 10 * 1000);

	for (i = 0; i < TRIM_TIMES; i++)
		off_value[i] = get_auxadc_audio();

	enable_trim_buf(priv, false);

	stop_trim_hardware(priv);

	/* reset trimming buffer mux to ground */
	set_trim_buf_in_mux(priv, TRIM_BUF_MUX_GROUND);

	/* sort */
	for (i = 0; i < TRIM_TIMES - 1; i++) {
		for (j = 0; j < TRIM_TIMES - 1 - i; j++) {
			if (on_value[j] > on_value[j + 1]) {
				tmp = on_value[j + 1];
				on_value[j + 1] = on_value[j];
				on_value[j] = tmp;
			}
			if (off_value[j] > off_value[j + 1]) {
				tmp = off_value[j + 1];
				off_value[j + 1] = off_value[j];
				off_value[j] = tmp;
			}
		}
	}


	/* calculate result */
	for (i = TRIM_DISCARD_NUM; i < TRIM_TIMES - TRIM_DISCARD_NUM; i++) {
		offset += on_value[i] - off_value[i];
		dev_info(priv->dev, "%s(), offset diff %d, on %d, off %d\n",
			 __func__,
			 on_value[i] - off_value[i], on_value[i], off_value[i]);
	}

	offset = DIV_ROUND_CLOSEST(offset, TRIM_USEFUL_NUM);

	dev_info(priv->dev, "%s(), channel = %d, offset = %d\n",
		 __func__, channel, offset);

	return offset;
#else
	return 0;
#endif

}
#endif

static int spk_trim_offset(struct mt6358_priv *priv, int channel)
{
	int offset = 0;
#ifndef CONFIG_FPGA_EARLY_PORTING
	int on_value[TRIM_TIMES];
	int off_value[TRIM_TIMES];
	int i;

	regmap_update_bits(priv->regmap, MT6358_AUXADC_CON10,
			   0x7, AUXADC_AVG_256);

	/* Step1. turn on buffer */
	start_trim_hardware_with_lo(priv, true);

	/* Step2. Enable TRIMBUF circuit and set TRIMBUF gain as 18dB */
	set_trim_buf_in_mux(priv, channel);
	set_trim_buf_gain(priv, TRIM_BUF_GAIN_18DB);
	enable_trim_buf(priv, true);

	/* Step3. Request AuxADC to get HP on dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		on_value[i] = get_auxadc_audio();

	/* Step4. select AUDREFN as TRIMBUF input */
	set_trim_buf_in_mux(priv, TRIM_MUX_AU_REFN);

	/* Step5. Request AuxADC to get HP off dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		off_value[i] = get_auxadc_audio();

	/* Step6. turn off buffer */
	enable_trim_buf(priv, false);
	stop_trim_hardware_with_lo(priv);

	offset = calculate_trim_result(priv, on_value, off_value,
			TRIM_TIMES, TRIM_DISCARD_NUM, TRIM_USEFUL_NUM);
	dev_info(priv->dev, "%s(), channel = %d, offset = %d\n",
			__func__, channel, offset);
#endif
	return offset;
}

#ifdef ANALOG_HPTRIM
static int pick_hp_finetrim(int offset_base,
				int offset_finetrim_1,
				int offset_finetrim_3)
{
	if (abs(offset_base) < abs(offset_finetrim_1)) {
		if (abs(offset_base) < abs(offset_finetrim_3))
			return 0x0;
		else
			return 0x3;
	} else {
		if (abs(offset_finetrim_1) < abs(offset_finetrim_3))
			return 0x1;
		else
			return 0x3;
	}
}

static int pick_spk_finetrim(int offset_base,
				 int offset_finetrim_2,
				 int offset_finetrim_3)
{
	if (abs(offset_base) < abs(offset_finetrim_2)) {
		if (abs(offset_base) < abs(offset_finetrim_3))
			return 0x0;
		else
			return 0x3;
	} else {
		if (abs(offset_finetrim_2) < abs(offset_finetrim_3))
			return 0x2;
		else
			return 0x3;
	}
}

static void set_lr_trim_code(struct mt6358_priv *priv)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int hpl_base = 0, hpr_base = 0;
	int hpl_min = 0, hpr_min = 0;
	int hpl_ceiling = 0, hpr_ceiling = 0;
	int hpl_floor = 0, hpr_floor = 0;
	int hpl_finetrim_1 = 0, hpr_finetrim_1 = 0;
	int hpl_finetrim_3 = 0, hpr_finetrim_3 = 0;
	int trimcodel = 0, trimcoder = 0;
	int trimcodel_ceiling = 0, trimcoder_ceiling = 0;
	int trimcodel_floor = 0, trimcoder_floor = 0;
	int finetriml = 0, finetrimr = 0;
	int trimcode_tmpl = 0, trimcode_tmpr = 0;
	int tmp = 0;
	unsigned int hp_3_pole_trim_setting = 0;
	unsigned int hp_4_pole_trim_setting = 0;
	bool code_change = false;
	unsigned int reg_value = 0;
	struct ana_offset *offset_3_pole = NULL;
	struct ana_offset *offset_4_pole = NULL;

	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), Start DCtrim Calibrating, AUDDEC_ELR_0 = 0x%x\n",
		 __func__, reg_value);

	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPTRIM_EN_MASK, 0x1 << HPTRIM_EN_SHIFT);
	priv->dc_trim.hp_3_pole_ana_offset.enable = 1;
	priv->dc_trim.hp_4_pole_ana_offset.enable = 1;
	/* channel L */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPTRIM_L_MASK, 0x0 << HPTRIM_L_SHIFT);
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPFINETRIM_L_MASK, 0x0 << HPFINETRIM_L_SHIFT);
	/* channel R */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPTRIM_R_MASK, 0x0 << HPTRIM_R_SHIFT);
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPFINETRIM_R_MASK, 0x0 << HPFINETRIM_R_SHIFT);

	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), AUDDEC_ELR_0 = 0x%x\n", __func__, reg_value);
	hp_trim_offset(priv);
	hpl_base = priv->dc_trim.hp_trim_offset[CH_L];
	hpr_base = priv->dc_trim.hp_trim_offset[CH_R];
	mdelay(10);

	/* Step1: get trim code */
	if (hpl_base == 0)
		goto EXIT;
	if (hpr_base == 0)
		goto EXIT;
	if (hpl_base > 0 || hpr_base > 0) {
		if (hpl_base > 0) {
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_L_MASK, 0x2 << HPTRIM_L_SHIFT);
			code_change = true;
		}
		if (hpr_base > 0) {
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_R_MASK, 0x2 << HPTRIM_R_SHIFT);
			code_change = true;
		}
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), step1 > 0 set 4 level AUDDEC_ELR_0 = 0x%x, trimcode(L/R) = %d / %d\n",
				__func__, reg_value, trimcodel, trimcoder);
		if (code_change) {
			hp_trim_offset(priv);
			code_change  = false;
			hpl_min = priv->dc_trim.hp_trim_offset[CH_L];
			hpr_min = priv->dc_trim.hp_trim_offset[CH_R];
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_L_MASK, 0x0 << HPTRIM_L_SHIFT);
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_R_MASK, 0x0 << HPTRIM_R_SHIFT);
			mdelay(10);

			/* Check floor & ceiling to avoid rounding error */
			if (hpl_base > 0) {
				trimcodel_floor = (abs(hpl_base)*3) /
						(abs(hpl_base-hpl_min));
				trimcodel_ceiling = trimcodel_floor + 1;
			}
			if (hpr_base > 0) {
				trimcoder_floor = (abs(hpr_base)*3) /
						(abs(hpr_base-hpr_min));
				trimcoder_ceiling = trimcoder_floor + 1;
			}
		}
	}
	if (hpl_base < 0 || hpr_base < 0) {
		if (hpl_base < 0) {
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_L_MASK, 0xa << HPTRIM_L_SHIFT);
			code_change = true;
		}
		if (hpr_base < 0) {
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_R_MASK, 0xa << HPTRIM_R_SHIFT);
			code_change = true;
		}
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), step1 < 0, set 4 level AUDDEC_ELR_0 = 0x%x, trimcode(L/R) = %d / %d\n",
				__func__, reg_value, trimcodel, trimcoder);
		if (code_change) {
			hp_trim_offset(priv);
			code_change  = false;
			hpl_min = priv->dc_trim.hp_trim_offset[CH_L];
			hpr_min = priv->dc_trim.hp_trim_offset[CH_R];
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_L_MASK, 0x0 << HPTRIM_L_SHIFT);
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_R_MASK, 0x0 << HPTRIM_R_SHIFT);
			mdelay(10);
			/* Check floor & ceiling to avoid rounding error */
			if (hpl_base < 0) {
				trimcodel_floor = (abs(hpl_base)*3) /
						(abs(hpl_base-hpl_min)) + 8;
				trimcodel_ceiling = trimcodel_floor + 1;
			}
			if (hpr_base < 0) {
				trimcoder_floor = (abs(hpr_base)*3) /
						(abs(hpr_base-hpr_min)) + 8;
				trimcoder_ceiling = trimcoder_floor + 1;
			}
		}
	}
	/* Get the best trim code from floor and ceiling value */
	/* Get floor trim code */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			   HPTRIM_L_MASK, trimcodel_floor << HPTRIM_L_SHIFT);
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			   HPTRIM_R_MASK, trimcoder_floor << HPTRIM_R_SHIFT);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), step1 floor AUDDEC_ELR_0 = 0x%x, trimcode(L/R) = %d / %d\n",
		 __func__, reg_value,
		 trimcodel_floor, trimcoder_floor);

	hp_trim_offset(priv);
	hpl_floor = priv->dc_trim.hp_trim_offset[CH_L];
	hpr_floor = priv->dc_trim.hp_trim_offset[CH_R];
	mdelay(10);
	/* Get ceiling trim code */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			   HPTRIM_L_MASK, trimcodel_ceiling << HPTRIM_L_SHIFT);
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			   HPTRIM_R_MASK, trimcoder_ceiling << HPTRIM_R_SHIFT);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), step1 ceiling AUDDEC_ELR_0 = 0x%x, trimcode(L/R) = %d / %d\n",
		 __func__, reg_value,
		 trimcodel_ceiling, trimcoder_ceiling);

	hp_trim_offset(priv);
	hpl_ceiling = priv->dc_trim.hp_trim_offset[CH_L];
	hpr_ceiling = priv->dc_trim.hp_trim_offset[CH_R];
	mdelay(10);
	/* Choose the best */
	if (abs(hpl_ceiling) < abs(hpl_floor)) {
		hpl_base = hpl_ceiling;
		trimcodel = trimcodel_ceiling;
	} else {
		hpl_base = hpl_floor;
		trimcodel = trimcodel_floor;
	}
	if (abs(hpr_ceiling) < abs(hpr_floor)) {
		hpr_base = hpr_ceiling;
		trimcoder = trimcoder_ceiling;
	} else {
		hpr_base = hpr_floor;
		trimcoder = trimcoder_floor;
	}

	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			   HPTRIM_L_MASK, trimcodel << HPTRIM_L_SHIFT);
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			   HPTRIM_R_MASK, trimcoder << HPTRIM_R_SHIFT);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), step1 result: AUDDEC_ELR_0 = 0x%x, hp_base(L/R) = %d / %d, trimcode(L/R) = %d / %d\n",
		 __func__, reg_value,
		 hpl_base, hpr_base, trimcodel, trimcoder);

	/* Step2: Trim code refine +1/0/-1 */
	trimcode_tmpl = trimcodel;
	trimcode_tmpr = trimcoder;
	mdelay(10);
	if (hpl_base == 0)
		goto EXIT;
	if (hpr_base == 0)
		goto EXIT;
	if (hpl_base > 0 || hpr_base > 0) {
		if ((hpl_base > 0) &&
		    (trimcodel != 0x7) && (trimcodel != 0x8)) {
			tmp = trimcodel + ((trimcodel > 7) ? -1 : 1);
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_L_MASK, tmp << HPTRIM_L_SHIFT);
			code_change = true;
		}
		if ((hpr_base > 0) &&
		    (trimcoder != 0x7) && (trimcoder != 0x8)) {
			tmp = trimcoder + ((trimcoder > 7) ? -1 : 1);
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_R_MASK, tmp << HPTRIM_R_SHIFT);
			code_change = true;
		}
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), step2 > 0, AUDDEC_ELR_0 = 0x%x, trimcode_tmp = %d\n",
				__func__, reg_value, tmp);
		if (code_change) {
			hp_trim_offset(priv);
			code_change = false;
			hpl_min = priv->dc_trim.hp_trim_offset[CH_L];
			hpr_min = priv->dc_trim.hp_trim_offset[CH_R];
			mdelay(10);
			if ((hpl_base > 0) &&
			    (hpl_min >= 0 || abs(hpl_min) < abs(hpl_base))) {
				if ((trimcodel != 0x7) && (trimcodel != 0x8)) {
					trimcode_tmpl =
						trimcodel +
						((trimcodel > 7) ? -1 : 1);
				} else {
					trimcode_tmpl = trimcodel;
					dev_info(priv->dev, "%s(), [Step2][L > 0, bit-overflow!!], don't refine, trimcodel = %d\n",
						 __func__, trimcodel);
				}
			}
			if ((hpr_base > 0) &&
			    (hpr_min >= 0 || abs(hpr_min) < abs(hpr_base))) {
				if ((trimcoder != 0x7) && (trimcoder != 0x8)) {
					trimcode_tmpr =
						trimcoder +
						((trimcoder > 7) ? -1 : 1);
				} else {
					trimcode_tmpr = trimcoder;
					dev_info(priv->dev, "%s(), [Step2][R > 0, bit-overflow!!], don't refine, trimcoder = %d\n",
						 __func__, trimcoder);
				}
			}
		}
	}
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPTRIM_L_MASK, trimcodel << HPTRIM_L_SHIFT);
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPTRIM_R_MASK, trimcoder << HPTRIM_R_SHIFT);
	if (hpl_base < 0 || hpr_base < 0) {
		if ((hpl_base < 0) && (trimcodel != 0) && (trimcodel != 0xf)) {
			tmp = trimcodel - ((trimcodel > 7) ? -1 : 1);
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_L_MASK, tmp << HPTRIM_L_SHIFT);
			code_change = true;
		}
		if ((hpr_base < 0) && (trimcoder != 0) && (trimcoder != 0xf)) {
			tmp = trimcoder - ((trimcoder > 7) ? -1 : 1);
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPTRIM_R_MASK, tmp << HPTRIM_R_SHIFT);
			code_change = true;
		}
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), step2 < 0 AUDDEC_ELR_0 = 0x%x, trimcode_tmp(L/R) = %d / %d\n",
				__func__,
				reg_value, trimcode_tmpl, trimcode_tmpr);
		if (code_change) {
			hp_trim_offset(priv);
			code_change = false;
			hpl_min = priv->dc_trim.hp_trim_offset[CH_L];
			hpr_min = priv->dc_trim.hp_trim_offset[CH_R];
			mdelay(10);
			if ((hpl_base < 0) &&
			    (hpl_min <= 0 || abs(hpl_min) < abs(hpl_base))) {
				if ((trimcodel != 0) && (trimcodel != 0xf)) {
					trimcode_tmpl =
						trimcodel -
						((trimcodel > 7) ? -1 : 1);
				} else {
					trimcode_tmpl = trimcodel;
					pr_debug("%s(), [Step2][L < 0, bit-overflow!!], don't refine, trimcodel = %d\n",
						 __func__, trimcodel);
				}
			}
			if ((hpr_base < 0) &&
			    (hpr_min <= 0 || abs(hpr_min) < abs(hpr_base))) {
				if ((trimcoder != 0) && (trimcoder != 0xf)) {
					trimcode_tmpr =
						trimcoder -
						((trimcoder > 7) ? -1 : 1);
				} else {
					trimcode_tmpr = trimcoder;
					pr_debug("%s(), [Step2][R < 0, bit-overflow!!], don't refine, trimcoder = %d\n",
						 __func__, trimcoder);
				}
			}
		}
	}
	trimcodel = trimcode_tmpl;
	trimcoder = trimcode_tmpr;
	/* channel L */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPTRIM_L_MASK, trimcodel << HPTRIM_L_SHIFT);
	/* channel R */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPTRIM_R_MASK, trimcoder << HPTRIM_R_SHIFT);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), step2 result AUDDEC_ELR_0 = 0x%x, trimcode(L/R) = %d / %d\n",
			__func__, reg_value, trimcode_tmpl, trimcode_tmpr);

	/*Step3: Trim code fine tune*/
	hp_trim_offset(priv);
	hpl_base = priv->dc_trim.hp_trim_offset[CH_L];
	hpr_base = priv->dc_trim.hp_trim_offset[CH_R];
	mdelay(10);
	if (hpl_base == 0)
		goto EXIT;
	if (hpr_base == 0)
		goto EXIT;
	if (hpl_base > 0 || hpr_base > 0) {
		if (hpl_base > 0) {
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPFINETRIM_L_MASK,
					0x1 << HPFINETRIM_L_SHIFT);
			code_change = true;
		}
		if (hpr_base > 0) {
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPFINETRIM_R_MASK,
					0x1 << HPFINETRIM_R_SHIFT);
			code_change = true;
		}
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), step3 > 0, AUDDEC_ELR_0 = 0x%x, hp_base(L/R) = %d / %d\n",
				__func__, reg_value, hpl_base, hpr_base);
		if (code_change) {
			hp_trim_offset(priv);
			code_change = false;
			/* HP finetrim=3 compensates negative DC value */
			/* Choose the best fine trim */
			hpl_finetrim_1 = priv->dc_trim.hp_trim_offset[CH_L];
			hpr_finetrim_1 = priv->dc_trim.hp_trim_offset[CH_R];
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPFINETRIM_L_MASK,
					0x0 << HPFINETRIM_L_SHIFT);
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPFINETRIM_R_MASK,
					0x0 << HPFINETRIM_R_SHIFT);
			mdelay(10);
			if ((hpl_base > 0) &&
			    ((hpl_finetrim_1 >= 0) &&
			    (abs(hpl_finetrim_1) < abs(hpl_base))))
				finetriml = 0x1;
			if ((hpr_base > 0) &&
			    ((hpr_finetrim_1 >= 0) &&
			    (abs(hpr_finetrim_1) < abs(hpr_base))))
				finetrimr = 0x1;
			if (hpl_finetrim_1 < 0 || hpr_finetrim_1 < 0) {
				/* base and finetrim=1 across zero. */
				/* Choose base, finetrim=1, and finetrim=3 */
				if (hpl_finetrim_1 < 0 && hpl_base > 0) {
					/* channel L */
					regmap_update_bits(priv->regmap,
						MT6358_AUDDEC_ELR_0,
						HPFINETRIM_L_MASK,
						0x3 << HPFINETRIM_L_SHIFT);
					code_change = true;
				}
				if (hpr_finetrim_1 < 0 && hpr_base > 0) {
					/* channel R */
					regmap_update_bits(priv->regmap,
						MT6358_AUDDEC_ELR_0,
						HPFINETRIM_R_MASK,
						0x3 << HPFINETRIM_R_SHIFT);
					code_change = true;
				}
				regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0,
						&reg_value);
				dev_info(priv->dev, "%s(), step3_2 > 0, AUDDEC_ELR_0 = 0x%x, code_change = %d\n",
						__func__,
						reg_value, code_change);
				if (code_change) {
					hp_trim_offset(priv);
					code_change = false;
					hpl_finetrim_3 =
						priv->dc_trim.hp_trim_offset
							[CH_L];
					hpr_finetrim_3 =
						priv->dc_trim.hp_trim_offset
							[CH_R];
					regmap_update_bits(priv->regmap,
						MT6358_AUDDEC_ELR_0,
						HPFINETRIM_L_MASK,
						0x0 << HPFINETRIM_L_SHIFT);
					regmap_update_bits(priv->regmap,
						MT6358_AUDDEC_ELR_0,
						HPFINETRIM_R_MASK,
						0x0 << HPFINETRIM_R_SHIFT);
					mdelay(10);

					if (hpl_base > 0) {
						finetriml =
						pick_hp_finetrim(hpl_base,
							hpl_finetrim_1,
							hpl_finetrim_3);
						dev_info(priv->dev, "%s(), [Step3] refine finetriml = %d\n",
							 __func__, finetriml);
					}
					if (hpr_base > 0) {
						finetrimr =
						pick_hp_finetrim(hpr_base,
							hpr_finetrim_1,
							hpr_finetrim_3);
						dev_info(priv->dev, "%s(), [Step3] refine finetrimr = %d\n",
							 __func__, finetrimr);
					}
				}
			}
		}
	}
	if (hpl_base < 0 || hpr_base < 0) {
		if (hpl_base < 0) {
			regmap_update_bits(priv->regmap,
				MT6358_AUDDEC_ELR_0,
				HPFINETRIM_L_MASK, 0x2 << HPFINETRIM_L_SHIFT);
			code_change = true;
		}
		if (hpr_base < 0) {
			regmap_update_bits(priv->regmap,
				MT6358_AUDDEC_ELR_0,
				HPFINETRIM_R_MASK, 0x2 << HPFINETRIM_R_SHIFT);
			code_change = true;
		}
		regmap_read(priv->regmap,
				MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), step3 < 0, AUDDEC_ELR_0 = 0x%x\n",
				__func__, reg_value);
		if (code_change) {
			hp_trim_offset(priv);
			code_change = false;
			hpl_min =
				priv->dc_trim.hp_trim_offset
				[CH_L];
			hpr_min =
				priv->dc_trim.hp_trim_offset
				[CH_R];
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPFINETRIM_L_MASK,
					0x0 << HPFINETRIM_L_SHIFT);
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					HPFINETRIM_R_MASK,
					0x0 << HPFINETRIM_R_SHIFT);
			mdelay(10);
			if ((hpl_base < 0) &&
			    (hpl_min <= 0 || abs(hpl_min) < abs(hpl_base)))
				finetriml = 0x2;
			if ((hpr_base < 0) &&
			    (hpr_min <= 0 || abs(hpr_min) < abs(hpr_base)))
				finetrimr = 0x2;
		}
	}
	/* channel L */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPFINETRIM_L_MASK, finetriml << HPFINETRIM_L_SHIFT);
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPFINETRIM_R_MASK, finetrimr << HPFINETRIM_R_SHIFT);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), step3 result AUDDEC_ELR_0 = 0x%x\n",
			__func__, reg_value);
EXIT:
	/* 4 pole fine trim */
	hp_trim_offset(priv);
	hpl_min = priv->dc_trim.hp_trim_offset[CH_L];
	hpr_min = priv->dc_trim.hp_trim_offset[CH_R];
	mdelay(10);
	priv->dc_trim.hp_3_pole_ana_offset.hp_trim_code[CH_L] = trimcodel;
	priv->dc_trim.hp_3_pole_ana_offset.hp_fine_trim[CH_L] = finetriml;
	priv->dc_trim.hp_3_pole_ana_offset.hp_trim_code[CH_R] = trimcoder;
	priv->dc_trim.hp_3_pole_ana_offset.hp_fine_trim[CH_R] = finetrimr;

	/* check trimcode is valid */
	if ((trimcodel < 0 || trimcodel > 0xf) ||
		(finetriml < 0 || finetriml > 0x3) ||
		(trimcoder < 0 || trimcoder > 0xf) ||
		(finetrimr < 0 || finetrimr > 0x3))
		dev_info(priv->dev, "%s(), [Warning], invalid trimcode(3 pole), trimcodel = %d, finetriml = %d, trimcoder = %d, finetrimr = %d\n",
			 __func__, trimcodel, finetriml, trimcoder, finetrimr);

	if ((hpl_min < 0) && (finetriml == 0x0)) {
		finetriml = 0x2;
	} else if ((hpl_min < 0) && (finetriml == 0x2)) {
		if ((trimcodel != 0) && (trimcodel != 0xf)) {
			finetriml = 0x0;
			trimcodel = trimcodel - ((trimcodel > 7) ? -1 : 1);
		} else {
			dev_info(priv->dev, "%s(), [Step4][bit-overflow!!], don't refine, trimcodel = %d, finetriml = %d\n",
				 __func__, trimcodel, finetriml);
		}
	}
	if ((hpr_min < 0) && (finetrimr == 0x0)) {
		finetrimr = 0x2;
	} else if ((hpr_min < 0) && (finetrimr == 0x2)) {
		if ((trimcoder != 0) && (trimcoder != 0xf)) {
			finetrimr = 0x0;
			trimcoder = trimcoder - ((trimcoder > 7) ? -1 : 1);
		} else {
			dev_info(priv->dev, "%s(), [Step4][bit-overflow!!], don't refine, trimcoder = %d, finetrimr = %d\n",
				 __func__, trimcoder, finetrimr);
		}
	}
	priv->dc_trim.hp_4_pole_ana_offset.hp_trim_code[CH_L] = trimcodel;
	priv->dc_trim.hp_4_pole_ana_offset.hp_fine_trim[CH_L] = finetriml;
	priv->dc_trim.hp_4_pole_ana_offset.hp_trim_code[CH_R] = trimcoder;
	priv->dc_trim.hp_4_pole_ana_offset.hp_fine_trim[CH_R] = finetrimr;

	/* check trimcode is valid */
	if ((trimcodel < 0 || trimcodel > 0xf) ||
		(finetriml < 0 || finetriml > 0x3) ||
		(trimcoder < 0 || trimcoder > 0xf) ||
		(finetrimr < 0 || finetrimr > 0x3))
		dev_info(priv->dev, "%s(), [Warning], invalid trimcode(4 pole), trimcodel = %d, finetriml = %d, trimcoder = %d, finetrimr = %d\n",
			 __func__, trimcodel, finetriml, trimcoder, finetrimr);

	offset_3_pole = &(priv->dc_trim.hp_3_pole_ana_offset);
	offset_4_pole = &(priv->dc_trim.hp_4_pole_ana_offset);
	hp_3_pole_trim_setting =
		(offset_3_pole->enable << HPTRIM_EN_SHIFT) |
		(offset_3_pole->hp_fine_trim[CH_R] << HPFINETRIM_R_SHIFT) |
		(offset_3_pole->hp_fine_trim[CH_L] << HPFINETRIM_L_SHIFT) |
		(offset_3_pole->hp_trim_code[CH_R] << HPTRIM_R_SHIFT) |
		(offset_3_pole->hp_trim_code[CH_L] << HPTRIM_L_SHIFT);
	hp_4_pole_trim_setting =
		(offset_4_pole->enable << HPTRIM_EN_SHIFT) |
		(offset_4_pole->hp_fine_trim[CH_R] << HPFINETRIM_R_SHIFT) |
		(offset_4_pole->hp_fine_trim[CH_L] << HPFINETRIM_L_SHIFT) |
		(offset_4_pole->hp_trim_code[CH_R] << HPTRIM_R_SHIFT) |
		(offset_4_pole->hp_trim_code[CH_L] << HPTRIM_L_SHIFT);
	priv->dc_trim.hp_3_pole_trim_setting = hp_3_pole_trim_setting;
	priv->dc_trim.hp_4_pole_trim_setting = hp_4_pole_trim_setting;

	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), Final result AUDDEC_ELR_0 = 0x%x, hp_3_pole_trim_setting = 0x%x, hp_4_pole_trim_setting = 0x%x\n",
		 __func__,
		 reg_value,
		 hp_3_pole_trim_setting, hp_4_pole_trim_setting);
	dev_info(priv->dev, "%s(), get hp offset L = %d, R = %d\n",
		 __func__,
		 priv->dc_trim.hp_trim_offset[CH_L],
		 priv->dc_trim.hp_trim_offset[CH_R]);
#endif
}

static void set_lr_trim_code_spk(struct mt6358_priv *priv, int channel)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int hpl_base = 0;
	int hpl_min = 0;
	int hpl_ceiling = 0;
	int hpl_floor = 0;
	int hpl_finetrim_2 = 0;
	int hpl_finetrim_3 = 0;
	int trimcode = 0;
	int trimcode_ceiling = 0;
	int trimcode_floor = 0;
	int finetrim = 0;
	int trimcode_tmp = 0;
	unsigned int spk_hp_3_pole_trim_setting = 0;
	unsigned int spk_hp_4_pole_trim_setting = 0;
	int trim_shift = 0, trim_mask = 0;
	int fine_shift = 0, fine_mask = 0;
	struct ana_offset *offset_3_pole = NULL;
	struct ana_offset *offset_4_pole = NULL;
	unsigned int reg_value = 0;

	if (channel == TRIM_BUF_MUX_HPL) {
		trim_shift = HPTRIM_L_SHIFT;
		trim_mask = HPTRIM_L_MASK;
		fine_shift = HPFINETRIM_L_SHIFT;
		fine_mask = HPFINETRIM_L_MASK;
	} else if (channel == TRIM_BUF_MUX_HPR) {
		trim_shift = HPTRIM_R_SHIFT;
		trim_mask = HPTRIM_R_MASK;
		fine_shift = HPFINETRIM_R_SHIFT;
		fine_mask = HPFINETRIM_R_MASK;
	}

	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), Start DCtrim Calibrating, channel = %d, AUDDEC_ELR_0 = 0x%x\n",
		 __func__, channel, reg_value);

	/* Step1: get trim code */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			HPTRIM_EN_MASK, 0x1 << HPTRIM_EN_SHIFT);
	priv->dc_trim.spk_3_pole_ana_offset.enable = 0x1;
	priv->dc_trim.spk_4_pole_ana_offset.enable = 0x1;

	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			trim_mask, 0x0 << trim_shift);
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			fine_mask, 0x0 << fine_shift);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), AUDDEC_ELR_0 = 0x%x\n", __func__, reg_value);
	hpl_base = spk_trim_offset(priv, channel);
	mdelay(10);

	if (hpl_base == 0)
		goto EXIT;

	if (hpl_base > 0) {
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
				trim_mask, 0x2 << trim_shift);
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), step1 > 0, AUDDEC_ELR_0 = 0x%x, trimcode = %d\n",
				__func__, reg_value, trimcode);
		hpl_min = spk_trim_offset(priv, channel);
		mdelay(10);
		/* Check floor and ceiling value to avoid rounding error */
		trimcode_floor = (abs(hpl_base)*3)/abs(hpl_base-hpl_min);
		trimcode_ceiling = trimcode_floor + 1;
		dev_info(priv->dev, "%s(), step1 > 0, get trim level trimcode_floor = %d, trimcode_ceiling = %d\n",
				__func__, trimcode_floor, trimcode_ceiling);
	} else {
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
				trim_mask, 0xa << trim_shift);
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), step1 < 0, AUDDEC_ELR_0 = 0x%x, trimcode = %d\n",
				__func__, reg_value, trimcode);
		hpl_min = spk_trim_offset(priv, channel);
		mdelay(10);
		/* Check floor and ceiling value to avoid rounding error */
		trimcode_floor = (abs(hpl_base)*3)/abs(hpl_base-hpl_min) + 8;
		trimcode_ceiling = trimcode_floor + 1;
		dev_info(priv->dev, "%s(), step1 < 0, get trim level trimcode_floor = %d, trimcode_ceiling = %d\n",
				__func__, trimcode_floor, trimcode_ceiling);
	}
	/* Get the best trim code from floor and ceiling value */
	/* Get floor trim code */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			trim_mask, trimcode_floor << trim_shift);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), step1 floor AUDDEC_ELR_0 = 0x%x, trimcode_floor = %d\n",
			__func__, reg_value, trimcode_floor);
	hpl_floor = spk_trim_offset(priv, channel);
	mdelay(10);
	/* Get ceiling trim code */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			trim_mask, trimcode_ceiling << trim_shift);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), step1 ceiling AUDDEC_ELR_0 = 0x%x, trimcode_ceiling = %d\n",
			__func__, reg_value, trimcode_ceiling);
	hpl_ceiling = spk_trim_offset(priv, channel);
	mdelay(10);
	/* Choose the best */
	if (abs(hpl_ceiling) < abs(hpl_floor)) {
		hpl_base = hpl_ceiling;
		trimcode = trimcode_ceiling;
	} else {
		hpl_base = hpl_floor;
		trimcode = trimcode_floor;
	}

	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			   trim_mask, trimcode << trim_shift);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), step1 result: AUDDEC_ELR_0 = 0x%x, hp_base = %d, trimcode = %d\n",
		 __func__, reg_value, hpl_base, trimcode);

	/* Step2: Trim code refine +1/0/-1 */
	hpl_base = spk_trim_offset(priv, channel);
	mdelay(10);

	if (hpl_base == 0)
		goto EXIT;

	if (hpl_base > 0) {
		if ((trimcode != 0x7) && (trimcode != 0x8)) {
			trimcode_tmp = trimcode + ((trimcode > 7) ? -1 : 1);
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					   trim_mask,
					   trimcode_tmp << trim_shift);
			regmap_read(priv->regmap,
				    MT6358_AUDDEC_ELR_0, &reg_value);
			dev_info(priv->dev, "%s(), step2 > 0, AUDDEC_ELR_0 = 0x%x, trimcode_tmp = %d\n",
				 __func__, reg_value, trimcode_tmp);
			hpl_min = spk_trim_offset(priv, channel);
			mdelay(10);
			if (hpl_min >= 0 || abs(hpl_min) < abs(hpl_base)) {
				trimcode = trimcode_tmp;
				hpl_base = hpl_min;
			}
		}
	} else {
		if ((trimcode != 0) && (trimcode != 0xf)) {
			trimcode_tmp = trimcode - ((trimcode > 7) ? -1 : 1);
			regmap_update_bits(priv->regmap,
					MT6358_AUDDEC_ELR_0,
					trim_mask, trimcode_tmp << trim_shift);
			regmap_read(priv->regmap,
					MT6358_AUDDEC_ELR_0, &reg_value);
			dev_info(priv->dev, "%s(), step2 < 0, AUDDEC_ELR_0 = 0x%x, trimcode_tmp = %d\n",
					__func__, reg_value, trimcode_tmp);
			hpl_min = spk_trim_offset(priv, channel);
			mdelay(10);
			if (hpl_min <= 0 || abs(hpl_min) < abs(hpl_base)) {
				trimcode = trimcode_tmp;
				hpl_base = hpl_min;
			}
		}
	}
	regmap_update_bits(priv->regmap,
			MT6358_AUDDEC_ELR_0,
			trim_mask, trimcode << trim_shift);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), step2 result AUDDEC_ELR_0 = 0x%x, trimcode = %d\n",
			__func__, reg_value, trimcode);

	/* Step3: Trim code fine tune */
	mdelay(10);

	if (hpl_base == 0)
		goto EXIT;
	if (hpl_base > 0) {
		regmap_update_bits(priv->regmap,
				MT6358_AUDDEC_ELR_0,
				fine_mask, 0x1 << fine_shift);
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), step3 > 0, AUDDEC_ELR_0 = 0x%x\n",
				__func__, reg_value);
		hpl_min = spk_trim_offset(priv, channel);
		mdelay(10);

		if (hpl_min >= 0 ||
				abs(hpl_min) < abs(hpl_base)) {
			finetrim = 0x1;
			hpl_base = hpl_min;
		} else {
			regmap_update_bits(priv->regmap,
					MT6358_AUDDEC_ELR_0,
					fine_mask, 0x3 << fine_shift);
			regmap_read(priv->regmap,
					MT6358_AUDDEC_ELR_0, &reg_value);
			dev_info(priv->dev, "%s(), step3_2 > 0, AUDDEC_ELR_0 = 0x%x\n ",
					__func__, reg_value);
			hpl_min = spk_trim_offset(priv, channel);
			mdelay(10);
			if (hpl_min >= 0 && abs(hpl_min) < abs(hpl_base)) {
				finetrim = 0x3;
				hpl_base = hpl_min;
			}
		}
	} else {
		/* SPK+HP finetrim=3 compensates positive DC value */
		/* choose the best fine trim */
		regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
				fine_mask, 0x2 << fine_shift);
		regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
		dev_info(priv->dev, "%s(), step3 < 0, AUDDEC_ELR_0 = 0x%x\n",
				__func__, reg_value);
		hpl_finetrim_2 = spk_trim_offset(priv, channel);
		mdelay(10);
		if ((hpl_finetrim_2 <= 0) &&
		    (abs(hpl_finetrim_2) < abs(hpl_base))) {
			finetrim = 0x2;
			hpl_base = hpl_finetrim_2;
		} else {
			/* base and finetrim=2 across zero */
			/* Choose best from base, finetrim=2, and finetrim=3 */
			regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
					fine_mask, 0x3 << fine_shift);
			regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0,
					&reg_value);
			dev_info(priv->dev, "%s(), step3_2 < 0, AUDDEC_ELR_0 = 0x%x\n",
					__func__, reg_value);
			hpl_finetrim_3 = spk_trim_offset(priv, channel);
			mdelay(10);

			finetrim = pick_spk_finetrim(hpl_base,
						     hpl_finetrim_2,
						     hpl_finetrim_3);
			if (finetrim == 0x2)
				hpl_base = hpl_finetrim_2;
			else if (finetrim == 0x3)
				hpl_base = hpl_finetrim_3;
		}
	}

	dev_info(priv->dev, "%s(), refine finetrim = %d\n",
		 __func__, finetrim);

	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ELR_0,
			fine_mask, finetrim << fine_shift);
EXIT:
	priv->dc_trim.spk_3_pole_ana_offset.hp_trim_code[CH_L] = trimcode;
	priv->dc_trim.spk_3_pole_ana_offset.hp_fine_trim[CH_L] = finetrim;
	priv->dc_trim.spk_3_pole_ana_offset.hp_trim_code[CH_R] =
			priv->dc_trim.hp_3_pole_ana_offset.hp_trim_code[CH_R];
	priv->dc_trim.spk_3_pole_ana_offset.hp_fine_trim[CH_R] =
			priv->dc_trim.hp_3_pole_ana_offset.hp_fine_trim[CH_R];
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), step3 result AUDDEC_ELR_0 = 0x%x\n",
		 __func__, reg_value);

	/* check trimcode is valid */
	if ((trimcode < 0 || trimcode > 0xf) ||
	    (finetrim < 0 || finetrim > 0x3))
		dev_info(priv->dev, "%s(), [Warning], invalid trimcode(3pole), trimcode = %d, finetrim = %d\n",
			 __func__, trimcode, finetrim);

	/* 4 pole fine trim */
	hpl_base = spk_trim_offset(priv, channel);
	mdelay(10);

	if ((hpl_base < 0) && (finetrim == 0x0)) {
		finetrim = 0x2;
	} else if ((hpl_base < 0) && (finetrim == 0x2)) {
		if ((trimcode != 0) && (trimcode != 0xf)) {
			finetrim = 0x0;
			trimcode = trimcode - ((trimcode > 7) ? -1 : 1);
		}
	}

	priv->dc_trim.spk_4_pole_ana_offset.hp_trim_code[CH_L] = trimcode;
	priv->dc_trim.spk_4_pole_ana_offset.hp_fine_trim[CH_L] = finetrim;
	priv->dc_trim.spk_4_pole_ana_offset.hp_trim_code[CH_R] =
			priv->dc_trim.hp_4_pole_ana_offset.hp_trim_code[CH_R];
	priv->dc_trim.spk_4_pole_ana_offset.hp_fine_trim[CH_R] =
			priv->dc_trim.hp_4_pole_ana_offset.hp_fine_trim[CH_R];

	/* check trimcode is valid */
	if ((trimcode < 0 || trimcode > 0xf) ||
	    (finetrim < 0 || finetrim > 0x3))
		dev_info(priv->dev, "%s(), [Warning], invalid trimcode(4 pole), trimcode = %d, finetrim = %d\n",
			 __func__, trimcode, finetrim);

	offset_3_pole = &(priv->dc_trim.spk_3_pole_ana_offset);
	offset_4_pole = &(priv->dc_trim.spk_4_pole_ana_offset);
	spk_hp_3_pole_trim_setting =
		(offset_3_pole->enable << HPTRIM_EN_SHIFT) |
		(offset_3_pole->hp_fine_trim[CH_R] << HPFINETRIM_R_SHIFT) |
		(offset_3_pole->hp_fine_trim[CH_L] << HPFINETRIM_L_SHIFT) |
		(offset_3_pole->hp_trim_code[CH_R] << HPTRIM_R_SHIFT) |
		(offset_3_pole->hp_trim_code[CH_L] << HPTRIM_L_SHIFT);
	spk_hp_4_pole_trim_setting =
		(offset_4_pole->enable << HPTRIM_EN_SHIFT) |
		(offset_4_pole->hp_fine_trim[CH_R] << HPFINETRIM_R_SHIFT) |
		(offset_4_pole->hp_fine_trim[CH_L] << HPFINETRIM_L_SHIFT) |
		(offset_4_pole->hp_trim_code[CH_R]) |
		(offset_4_pole->hp_trim_code[CH_L] << HPTRIM_L_SHIFT);
	priv->dc_trim.spk_hp_3_pole_trim_setting = spk_hp_3_pole_trim_setting;
	priv->dc_trim.spk_hp_4_pole_trim_setting = spk_hp_4_pole_trim_setting;

	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &reg_value);
	dev_info(priv->dev, "%s(), Final result AUDDEC_ELR_0 = 0x%x, spk_hp_3_pole_trim_setting = 0x%x, spk_hp_4_pole_trim_setting = 0x%x\n",
			__func__,
			reg_value,
			spk_hp_3_pole_trim_setting, spk_hp_4_pole_trim_setting);
	dev_info(priv->dev, "%s(), get spkl offset = %d, channel = %d\n",
			__func__, spk_trim_offset(priv, channel), channel);
#endif
}
#endif

static void get_hp_trim_offset(struct mt6358_priv *priv, bool force)
{
	struct dc_trim_data *dc_trim = &priv->dc_trim;

	if (dc_trim->calibrated && !force)
		return;

	dev_info(priv->dev, "%s(), Start DCtrim Calibrating", __func__);
#ifdef ANALOG_HPTRIM
	set_lr_trim_code(priv);
	priv->dc_trim.hp_offset[CH_L] = priv->dc_trim.hp_trim_offset[CH_L];
	priv->dc_trim.hp_offset[CH_R] = priv->dc_trim.hp_trim_offset[CH_R];
	dev_info(priv->dev, "%s(), priv->dc_trim.hp_offset[CH_L]: %d, priv->dc_trim.hp_offset[CH_R]: %d\n",
		 __func__,
		 priv->dc_trim.hp_offset[CH_L],
		 priv->dc_trim.hp_offset[CH_R]);

	set_lr_trim_code_spk(priv, TRIM_BUF_MUX_HPL);
	priv->dc_trim.spk_l_offset = spk_trim_offset(priv, TRIM_BUF_MUX_HPL);
	dev_info(priv->dev, "%s(), priv->spkl_dc_offset: %d\n", __func__,
		 priv->dc_trim.spk_l_offset);
#else
	dc_trim->hp_offset[CH_L] = hp_trim_offset(priv, TRIM_BUF_MUX_HPL);
	dc_trim->hp_offset[CH_R] = hp_trim_offset(priv, TRIM_BUF_MUX_HPR);
#endif
	udelay(1000);
	dc_trim->calibrated = true;
	dev_info(priv->dev, "%s(), End DCtrim Calibrating, L: %d, R: %d",
		 __func__,
		 dc_trim->hp_offset[CH_L], dc_trim->hp_offset[CH_R]);
}

static int dc_trim_thread(void *arg)
{
	struct mt6358_priv *priv = arg;

	get_hp_trim_offset(priv, false);
#ifdef CONFIG_MTK_ACCDET
	accdet_late_init(0);
#endif
	do_exit(0);
	return 0;
}

/* Headphone Impedance Detection */
static int mtk_calculate_impedance_formula(int pcm_offset, int aux_diff)
{
	/* The formula is from DE programming guide */
	/* should be mantain by pmic owner */
	/* R = V /I */
	/* V = auxDiff * (1800mv /auxResolution)  /TrimBufGain */
	/* I =  pcmOffset * DAC_constant * Gsdm * Gibuf */

	return DIV_ROUND_CLOSEST(3600000 / pcm_offset * aux_diff, 7832);
}

static int calculate_impedance(struct mt6358_priv *priv,
			       int dc_init, int dc_input,
			       short pcm_offset,
			       const unsigned int detect_times)
{
	int dc_value;
	int r_tmp = 0;

	if (dc_input < dc_init) {
		dev_warn(priv->dev, "%s(), Wrong[%d] : dc_input(%d) < dc_init(%d)\n",
			 __func__, pcm_offset, dc_input, dc_init);
		return 0;
	}

	dc_value = dc_input - dc_init;
	r_tmp = mtk_calculate_impedance_formula(pcm_offset, dc_value);
	r_tmp = DIV_ROUND_CLOSEST(r_tmp, detect_times);

	/* Efuse calibration */
	if ((priv->hp_current_calibrate_val != 0) && (r_tmp != 0)) {
		dev_info(priv->dev, "%s(), Before Calibration from EFUSE: %d, R: %d\n",
			 __func__, priv->hp_current_calibrate_val, r_tmp);
		r_tmp = DIV_ROUND_CLOSEST(
				r_tmp * 128 + priv->hp_current_calibrate_val,
				128);
	}

	dev_dbg(priv->dev, "%s(), pcm_offset %d dcoffset %d detected resistor is %d\n",
		__func__, pcm_offset, dc_value, r_tmp);

	return r_tmp;
}

#define PARALLEL_OHM 0
static int detect_impedance(struct mt6358_priv *priv)
{
	const unsigned int num_detect = 8;
	int i;
	int dc_sum = 0, detect_sum = 0;
	int pick_impedance = 0, impedance = 0, phase_flag = 0;
	int cur_dc = 0;
	unsigned int value;

	/* params by chip */
	int auxcable_impedance = 5000;
	/* should little lower than auxadc max resolution */
	int auxadc_upper_bound = 32630;
	/* Dc ramp up and ramp down step */
	int dc_step = 96;
	/* Phase 0 : high impedance with worst resolution */
	int dc_phase0 = 288;
	/* Phase 1 : median impedance with normal resolution */
	int dc_phase1 = 1440;
	/* Phase 2 : low impedance with better resolution */
	int dc_phase2 = 6048;
	/* Resistance Threshold of phase 2 and phase 1 */
	int resistance_1st_threshold = 250;
	/* Resistance Threshold of phase 1 and phase 0 */
	int resistance_2nd_threshold = 1000;

	if (priv->ops.adda_dl_gain_control) {
		priv->ops.adda_dl_gain_control(true);
	} else {
		dev_warn(priv->dev, "%s(), adda_dl_gain_control ops not ready\n",
			 __func__);
		return 0;
	}

	if (priv->ops.enable_dc_compensation &&
	    priv->ops.set_lch_dc_compensation &&
	    priv->ops.set_rch_dc_compensation) {
		priv->ops.set_lch_dc_compensation(0);
		priv->ops.set_rch_dc_compensation(0);
		priv->ops.enable_dc_compensation(true);
	} else {
		dev_warn(priv->dev, "%s(), dc compensation ops not ready\n",
			 __func__);
		return 0;
	}

	regmap_update_bits(priv->regmap, MT6358_AUXADC_CON10,
			   0x7, AUXADC_AVG_64);

	set_trim_buf_in_mux(priv, TRIM_BUF_MUX_HPR);
	set_trim_buf_gain(priv, TRIM_BUF_GAIN_18DB);
	enable_trim_buf(priv, true);

	/* set hp gain 0dB */
	regmap_update_bits(priv->regmap, MT6358_ZCD_CON2,
			   RG_AUDHPRGAIN_MASK_SFT,
			   DL_GAIN_0DB << RG_AUDHPRGAIN_SFT);
	regmap_update_bits(priv->regmap, MT6358_ZCD_CON2,
			   RG_AUDHPLGAIN_MASK_SFT, DL_GAIN_0DB);

	for (cur_dc = 0; cur_dc <= dc_phase2; cur_dc += dc_step) {
		/* apply dc by dc compensation: 16bit MSB and negative value */
		priv->ops.set_lch_dc_compensation(-cur_dc << 16);
		priv->ops.set_rch_dc_compensation(-cur_dc << 16);

		/* save for DC = 0 offset */
		if (cur_dc == 0) {
			usleep_range(1 * 1000, 1 * 1000);
			dc_sum = 0;
			for (i = 0; i < num_detect; i++)
				dc_sum += get_auxadc_audio();

			if ((dc_sum / num_detect) > auxadc_upper_bound) {
				dev_info(priv->dev, "%s(), cur_dc == 0, auxadc value %d > auxadc_upper_bound %d\n",
					 __func__,
					 dc_sum / num_detect,
					 auxadc_upper_bound);
				impedance = auxcable_impedance;
				break;
			}
		}

		/* start checking */
		if (cur_dc == dc_phase0) {
			usleep_range(1 * 1000, 1 * 1000);
			detect_sum = 0;
			detect_sum = get_auxadc_audio();

			if ((dc_sum / num_detect) == detect_sum) {
				dev_info(priv->dev, "%s(), dc_sum / num_detect %d == detect_sum %d\n",
					 __func__,
					 dc_sum / num_detect, detect_sum);
				impedance = auxcable_impedance;
				break;
			}

			pick_impedance = calculate_impedance(
						priv,
						dc_sum / num_detect,
						detect_sum, cur_dc, 1);

			if (pick_impedance < resistance_1st_threshold) {
				phase_flag = 2;
				continue;
			} else if (pick_impedance < resistance_2nd_threshold) {
				phase_flag = 1;
				continue;
			}

			/* Phase 0 : detect range 1kohm to 5kohm impedance */
			for (i = 1; i < num_detect; i++)
				detect_sum += get_auxadc_audio();

			/* if auxadc > 32630 , the hpImpedance is over 5k ohm */
			if ((detect_sum / num_detect) > auxadc_upper_bound)
				impedance = auxcable_impedance;
			else
				impedance = calculate_impedance(priv,
								dc_sum,
								detect_sum,
								cur_dc,
								num_detect);
			break;
		}

		/* Phase 1 : detect range 250ohm to 1000ohm impedance */
		if (phase_flag == 1 && cur_dc == dc_phase1) {
			usleep_range(1 * 1000, 1 * 1000);
			detect_sum = 0;
			for (i = 0; i < num_detect; i++)
				detect_sum += get_auxadc_audio();

			impedance = calculate_impedance(priv,
							dc_sum, detect_sum,
							cur_dc, num_detect);
			break;
		}

		/* Phase 2 : detect under 250ohm impedance */
		if (phase_flag == 2 && cur_dc == dc_phase2) {
			usleep_range(1 * 1000, 1 * 1000);
			detect_sum = 0;
			for (i = 0; i < num_detect; i++)
				detect_sum += get_auxadc_audio();

			impedance = calculate_impedance(priv,
							dc_sum, detect_sum,
							cur_dc, num_detect);
			break;
		}
		usleep_range(1 * 200, 1 * 200);
	}

	if (PARALLEL_OHM != 0) {
		if (impedance < PARALLEL_OHM) {
			impedance = DIV_ROUND_CLOSEST(impedance * PARALLEL_OHM,
						      PARALLEL_OHM - impedance);
		} else {
			dev_warn(priv->dev, "%s(), PARALLEL_OHM %d <= impedance %d\n",
				 __func__, PARALLEL_OHM, impedance);
		}
	}

	regmap_read(priv->regmap, MT6358_AUXADC_CON10, &value);
	dev_info(priv->dev, "%s(), phase %d [dc,detect]Sum %d times [%d,%d], hp_impedance %d, pick_impedance %d, AUXADC_CON10 0x%x\n",
		 __func__, phase_flag, num_detect, dc_sum, detect_sum,
		 impedance, pick_impedance, value);

	/* Ramp-Down */
	while (cur_dc > 0) {
		cur_dc -= dc_step;
		/* apply dc by dc compensation: 16bit MSB and negative value */
		priv->ops.set_lch_dc_compensation(-cur_dc << 16);
		priv->ops.set_rch_dc_compensation(-cur_dc << 16);
		usleep_range(1 * 200, 1 * 200);
	}

	priv->ops.set_lch_dc_compensation(0);
	priv->ops.set_rch_dc_compensation(0);
	priv->ops.enable_dc_compensation(false);
	priv->ops.adda_dl_gain_control(false);

	set_trim_buf_in_mux(priv, TRIM_BUF_MUX_GROUND);
	enable_trim_buf(priv, false);

	return impedance;
}

static int hp_impedance_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	if (priv->dev_counter[DEVICE_HP] <= 0 ||
	    priv->mux_select[MUX_HP_L] != HP_MUX_HP_IMPEDANCE) {
		dev_warn(priv->dev, "%s(), counter %d <= 0 || mux_select[MUX_HP_L] %d != HP_MUX_HP_IMPEDANCE\n",
			 __func__,
			 priv->dev_counter[DEVICE_HP],
			 priv->mux_select[MUX_HP_L]);
		ucontrol->value.integer.value[0] = priv->hp_impedance;
		return 0;
	}

	priv->hp_impedance = detect_impedance(priv);

	ucontrol->value.integer.value[0] = priv->hp_impedance;

	dev_info(priv->dev, "%s(), hp_impedance = %d, efuse = %d\n",
		 __func__, priv->hp_impedance, priv->hp_current_calibrate_val);

	return 0;
}

/* vow control */
static int *get_vow_coeff_by_name(struct mt6358_priv *priv,
				 const char *name)
{
	if (strcmp(name, "Audio VOWCFG0 Data") == 0)
		return &(priv->reg_afe_vow_cfg0);
	else if (strcmp(name, "Audio VOWCFG1 Data") == 0)
		return &(priv->reg_afe_vow_cfg1);
	else if (strcmp(name, "Audio VOWCFG2 Data") == 0)
		return &(priv->reg_afe_vow_cfg2);
	else if (strcmp(name, "Audio VOWCFG3 Data") == 0)
		return &(priv->reg_afe_vow_cfg3);
	else if (strcmp(name, "Audio VOWCFG4 Data") == 0)
		return &(priv->reg_afe_vow_cfg4);
	else if (strcmp(name, "Audio VOWCFG5 Data") == 0)
		return &(priv->reg_afe_vow_cfg5);
	else if (strcmp(name, "Audio_VOW_Periodic") == 0)
		return &(priv->reg_afe_vow_periodic);
	else
		return NULL;
}

static int audio_vow_cfg_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	int *vow_cfg;

	vow_cfg = get_vow_coeff_by_name(priv, kcontrol->id.name);
	if (!vow_cfg) {
		dev_err(priv->dev, "%s(), vow_cfg == NULL\n", __func__);
		return -EINVAL;
	}
	dev_info(priv->dev, "%s(), %s = %d\n",
		 __func__, kcontrol->id.name, *vow_cfg);

	ucontrol->value.integer.value[0] = *vow_cfg;
	return 0;
}

static int audio_vow_cfg_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	int index = ucontrol->value.integer.value[0];
	int *vow_cfg;

	vow_cfg = get_vow_coeff_by_name(priv, kcontrol->id.name);
	if (!vow_cfg) {
		dev_err(priv->dev, "%s(), vow_cfg == NULL\n", __func__);
		return -EINVAL;
	}
	dev_info(priv->dev, "%s(), %s = %d\n",
		 __func__, kcontrol->id.name, index);

	*vow_cfg = index;
	return 0;
}

/* misc control */
static const char *const off_on_function[] = {"Off", "On"};

static int hp_plugged_in_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	ucontrol->value.integer.value[0] = priv->hp_plugged;
	return 0;
}

static int hp_plugged_in_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(off_on_function)) {
		dev_warn(priv->dev, "%s(), return -EINVAL\n", __func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 1) {
		priv->dc_trim.mic_vinp_mv = get_accdet_auxadc();
		dev_info(priv->dev, "%s(), mic_vinp_mv = %d\n",
			 __func__, priv->dc_trim.mic_vinp_mv);
	}

	priv->hp_plugged = ucontrol->value.integer.value[0];

	return 0;
}

#ifdef ANALOG_HPTRIM
static int disable_analog_dc_compensation_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	ucontrol->value.integer.value[0] =
			priv->dc_trim.dc_compensation_disabled;

	return 0;
}

static int disable_analog_dc_compensation_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(off_on_function)) {
		dev_warn(priv->dev, "%s(), return -EINVAL\n",
			 __func__);
		return -EINVAL;
	}

	priv->dc_trim.dc_compensation_disabled =
			ucontrol->value.integer.value[0];

	return 0;
}
#endif

static int mt6358_codec_debug_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt6358_codec_debug_set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	return mt6358_print_register(priv);
}

static const struct soc_enum misc_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(off_on_function), off_on_function),
};

static const char *const rcv_mic_function[] = {"Off", "ACC", "DCC"};

static const struct soc_enum rcv_mic_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rcv_mic_function), rcv_mic_function),
};

enum {
	RCV_MIC_OFF = 0,
	RCV_MIC_ACC,
	RCV_MIC_DCC,
};

static int mt6358_rcv_mic_set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	int rcv_mic_type = ucontrol->value.integer.value[0];

	dev_info(priv->dev, "%s(), rcv_mic_type = %d\n",
			__func__, rcv_mic_type);

	/* receiver downlink */
	playback_gpio_set(priv);
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13,
			   0x1 << 4, 0x0);
	/* Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true);
	/* release HP CMFB gate rstb */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON4,
			   0x1 << 6, 0x1 << 6);

	regmap_update_bits(priv->regmap, MT6358_DCXO_CW14,
			   0x1 << 13, 0x1 << 13);
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON6,
			   0x0003, 0x0001);
	regmap_update_bits(priv->regmap, MT6358_AUD_TOP_CKPDN_CON0,
			   0x66, 0x0);
	usleep_range(250, 270);
	/* Audio system digital clock power down release */
	regmap_update_bits(priv->regmap, MT6358_AUDIO_TOP_CON0,
			   0x00c5, 0x0000);
	usleep_range(250, 270);

	/* sdm audio fifo clock power on */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0006);
	/* scrambler clock on enable */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON0, 0xCBA1);
	/* sdm power on */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x0003);
	/* sdm fifo enable */
	regmap_write(priv->regmap, MT6358_AFUNC_AUD_CON2, 0x000B);

	/* afe enable, dl_lr_swap = 0 */
	regmap_update_bits(priv->regmap, MT6358_AFE_UL_DL_CON0,
			   0x4001, 0x0001);

	/* turn on dl */
	regmap_write(priv->regmap, MT6358_AFE_DL_SRC2_CON0_L, 0x0001);

	/* set DL in normal path, not from sine gen table */
	regmap_write(priv->regmap, MT6358_AFE_TOP_CON0, 0x0000);

	/* Reduce ESD resistance of AU_REFN */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON2, 0x4000);

	/* Turn on DA_600K_NCP_VA18 */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON1, 0x0001);
	/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON2, 0x002c);
	/* Toggle RG_DIVCKS_CHG */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON0, 0x0001);
	/* Set NCP soft start mode as default mode: 100us */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON4, 0x0003);
	/* Enable NCP */
	regmap_write(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, 0x0000);
	usleep_range(250, 270);

	/* Enable cap-less LDOs (1.5V) */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
			   0x1055, 0x1055);
	/* Enable NV regulator (-1.2V) */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON15, 0x0001);
	usleep_range(100, 120);

	/* Enable AUD_ZCD */
	zcd_enable(priv, true, DEVICE_RCV);

	/* Disable handset short-circuit protection */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0010);
	/* Enable IBIST */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
	/* Set HP DR bias current optimization, 010: 6uA */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON11,
			   0xff80, 0x4900);
	/* Set HP & ZCD bias current optimization */
	/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON12, 0x0055);
	/* Set HS STB enhance circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0090);

	/* Disable HP main CMFB loop */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0000);
	/* Select CMFB resistor bulk to AC mode */
	/* Selec HS/LO cap size (6.5pF default) */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON10, 0x0000);

	/* Enable HS driver bias circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0092);
	/* Enable HS driver core circuits */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x0093);

	/* Set HS gain to normal gain step by step */
	regmap_write(priv->regmap, MT6358_ZCD_CON3, 0x0);

	/* Enable AUD_CLK */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13, 0x1, 0x1);
	/* Enable Audio DAC  */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON0, 0x0009);
	/* Enable low-noise mode of DAC */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON9, 0x0001);
	/* Switch HS MUX to audio DAC */
	regmap_write(priv->regmap, MT6358_AUDDEC_ANA_CON6, 0x009b);

	/* phone mic */

	/* Enable audio ADC CLKGEN  */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON13,
			   0x1 << 5, 0x1 << 5);
	/* ADC CLK from CLKGEN (13MHz) */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON3, 0x0000);
	/* Enable  LCLDO_ENC 1P8V */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
			   0x2500, 0x0100);
	/* LCLDO_ENC remote sense */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON14,
			   0x2500, 0x2500);

	if (rcv_mic_type == RCV_MIC_DCC) {
		/* DCC 50k CLK (from 26M) */
		regmap_write(priv->regmap, MT6358_AUD_TOP_CKPDN_CON0, 0x0000);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2062);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2060);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG0, 0x2061);
		regmap_write(priv->regmap, MT6358_AFE_DCCLK_CFG1, 0x0100);
	}

	/* Enable MICBIAS0, MISBIAS0 = 1P9V */
	regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON9, 0x0021);

	if (rcv_mic_type == RCV_MIC_DCC) {
		/* Audio L preamplifier DCC precharge */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   0x1 << 2, 0x1 << 2);
		/* Audio R preamplifier DCC precharge */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   0x1 << 2, 0x1 << 2);
	}

	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   0x0700, 0x4 << 8);
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
			   0x0700, 0x4 << 8);

	/* "ADC1", main_mic */
	/* Audio L preamplifier input sel : AIN0. Enable audio L PGA */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   0xf0c1, 0x0041);
	if (rcv_mic_type == RCV_MIC_DCC) {
		/* Audio L preamplifier DCCEN */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				   0x1 << 1, 0x1 << 1);
	}
	/* Audio L ADC input sel : L PGA. Enable audio L ADC */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
			   0xf000, 0x5000);

	usleep_range(100, 150);

	/* ref mic */
	/* Audio R preamplifier input sel : AIN2. Enable audio R PGA */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
			   0xf0c1, 0x00c1);
	if (rcv_mic_type == RCV_MIC_DCC) {
		/* Audio R preamplifier DCCEN */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				   0x1 << 1, 0x1 << 1);
	}
	/* Audio R ADC input sel : R PGA. Enable audio R ADC */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
			   0xf000, 0x5000);

	if (rcv_mic_type == RCV_MIC_DCC) {
		/* Short body to ground in PGA */
		regmap_write(priv->regmap, MT6358_AUDENC_ANA_CON3, 0x0000);
	}

	usleep_range(100, 150);

	if (rcv_mic_type == RCV_MIC_DCC) {
		/* Audio L ADC input sel : L PGA. Enable audio L ADC */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON0,
				RG_AUDPREAMPLDCPRECHARGE_MASK_SFT, 0x0);
		/* Audio R ADC input sel : R PGA. Enable audio R ADC */
		regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON1,
				RG_AUDPREAMPLDCPRECHARGE_MASK_SFT, 0x0);
	}

	/* here to set digital part */

	/* set gpio miso mode */
	capture_gpio_set(priv);

	/* power on clock */
	regmap_update_bits(priv->regmap, MT6358_AUDIO_TOP_CON0,
			   0x00bf, 0x0000);

	/* configure ADC setting */
	regmap_write(priv->regmap, MT6358_AFE_TOP_CON0, 0x0000);

	/* [0] afe enable */
	regmap_update_bits(priv->regmap, MT6358_AFE_UL_DL_CON0,
			   0x0001, 0x0001);

	mt6358_mtkaif_tx_enable(priv);

	/* UL dmic setting */
	regmap_write(priv->regmap, MT6358_AFE_UL_SRC_CON0_H, 0x0000);

	/* UL turn on */
	regmap_write(priv->regmap, MT6358_AFE_UL_SRC_CON0_L, 0x0001);

	return 0;
}

static int mt6358_rcv_mic_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

/* vow control */
static const struct snd_kcontrol_new mt6358_snd_vow_controls[] = {
	SOC_SINGLE_EXT("Audio VOWCFG0 Data",
		       SND_SOC_NOPM, 0, 0x80000, 0,
		       audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio VOWCFG1 Data",
		       SND_SOC_NOPM, 0, 0x80000, 0,
		       audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio VOWCFG2 Data",
		       SND_SOC_NOPM, 0, 0x80000, 0,
		       audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio VOWCFG3 Data",
		       SND_SOC_NOPM, 0, 0x80000, 0,
		       audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio VOWCFG4 Data",
		       SND_SOC_NOPM, 0, 0x80000, 0,
		       audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio VOWCFG5 Data",
		       SND_SOC_NOPM, 0, 0x80000, 0,
		       audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio_VOW_Periodic",
		       SND_SOC_NOPM, 0, 0x80000, 0,
		       audio_vow_cfg_get, audio_vow_cfg_set),
};

static int dmic_used_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	ucontrol->value.integer.value[0] =
		priv->mux_select[MUX_MIC_TYPE] == MIC_TYPE_MUX_DMIC;

	return 0;
}

static const struct snd_kcontrol_new mt6358_snd_misc_controls[] = {
	SOC_ENUM_EXT("Headphone Plugged In", misc_control_enum[0],
		     hp_plugged_in_get, hp_plugged_in_set),
	SOC_SINGLE_EXT("Audio HP ImpeDance Setting",
		       SND_SOC_NOPM, 0, 0x10000, 0,
		       hp_impedance_get, NULL),
#ifdef ANALOG_HPTRIM
	SOC_ENUM_EXT("Disable Analog DC Compensation", misc_control_enum[0],
		     disable_analog_dc_compensation_get,
		     disable_analog_dc_compensation_set),
#endif
	SOC_ENUM_EXT("Audio_Codec_Debug_Setting", misc_control_enum[0],
		     mt6358_codec_debug_get, mt6358_codec_debug_set),
	SOC_ENUM_EXT("PMIC_REG_CLEAR", rcv_mic_enum[0],
		     mt6358_rcv_mic_get, mt6358_rcv_mic_set),
	SOC_ENUM_EXT("DMic Used", misc_control_enum[0], dmic_used_get, NULL),
};

static int mt6358_codec_init_reg(struct mt6358_priv *priv)
{
	int ret = 0;

	/* enable clk buf */
	regmap_update_bits(priv->regmap, MT6358_DCXO_CW14,
			   0x1 << RG_XO_AUDIO_EN_M_SFT,
			   0x1 << RG_XO_AUDIO_EN_M_SFT);

	/* set those not controlled by dapm widget */

	/* audio clk source from internal dcxo */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON6,
			   RG_CLKSQ_IN_SEL_TEST_MASK_SFT,
			   0x0);

	/* Disable HeadphoneL/HeadphoneR short circuit protection */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   RG_AUDHPLSCDISABLE_VAUDP15_MASK_SFT,
			   0x1 << RG_AUDHPLSCDISABLE_VAUDP15_SFT);
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON0,
			   RG_AUDHPRSCDISABLE_VAUDP15_MASK_SFT,
			   0x1 << RG_AUDHPRSCDISABLE_VAUDP15_SFT);
	/* Disable voice short circuit protection */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON6,
			   RG_AUDHSSCDISABLE_VAUDP15_MASK_SFT,
			   0x1 << RG_AUDHSSCDISABLE_VAUDP15_SFT);
	/* disable LO buffer left short circuit protection */
	regmap_update_bits(priv->regmap, MT6358_AUDDEC_ANA_CON7,
			   RG_AUDLOLSCDISABLE_VAUDP15_MASK_SFT,
			   0x1 << RG_AUDLOLSCDISABLE_VAUDP15_SFT);

	/* Set HP_EINT trigger level to 2.0v */
	regmap_update_bits(priv->regmap, MT6358_AUDENC_ANA_CON11,
			   RG_EINTCOMPVTH_MASK_SFT,
			   0x1 << RG_EINTCOMPVTH_SFT);

	/* gpio miso driving set to 4mA */
	regmap_write(priv->regmap, MT6358_DRV_CON3, 0x8888);

	/* set gpio */
	playback_gpio_reset(priv);
	capture_gpio_reset(priv);

	/* disable clk buf */
	regmap_update_bits(priv->regmap, MT6358_DCXO_CW14,
			   0x1 << RG_XO_AUDIO_EN_M_SFT,
			   0x0);

	/* this will trigger dctrim widgat power down event */
	enable_trim_buf(priv, true);
	return ret;
}

static int get_hp_current_calibrate_val(struct mt6358_priv *priv)
{
	int ret = 0;
	int value, sign;

	/* 1. enable efuse ctrl engine clock */
	regmap_update_bits(priv->regmap, MT6358_TOP_CKHWEN_CON0_CLR,
			   0x1 << 2, 0x1 << 2);
	regmap_update_bits(priv->regmap, MT6358_TOP_CKPDN_CON0_CLR,
			   0x1 << 4, 0x1 << 4);

	/* 2. set RG_OTP_RD_SW */
	regmap_update_bits(priv->regmap, MT6358_OTP_CON11, 0x0001, 0x0001);
#if defined(CONFIG_SND_SOC_MT6366)
	/* 3. set EFUSE addr */
	/* HPDET_COMP[6:0] @ efuse bit 1880 ~ 1886 */
	/* HPDET_COMP_SIGN @ efuse bit 1887 */
	/* 1880 / 8 = 235 --> 0xeb */
	regmap_update_bits(priv->regmap, MT6358_OTP_CON0, 0xff, 0xeb);
#else
	/* 3. set EFUSE addr */
	/* HPDET_COMP[6:0] @ efuse bit 1696 ~ 1702 */
	/* HPDET_COMP_SIGN @ efuse bit 1703 */
	/* 1696 / 8 = 212 --> 0xd4 */
	regmap_update_bits(priv->regmap, MT6358_OTP_CON0, 0xff, 0xd4);
#endif
	/* 4. Toggle RG_OTP_RD_TRIG */
	regmap_read(priv->regmap, MT6358_OTP_CON8, &ret);
	if (ret == 0)
		regmap_update_bits(priv->regmap, MT6358_OTP_CON8,
				   0x0001, 0x0001);
	else
		regmap_update_bits(priv->regmap, MT6358_OTP_CON8,
				   0x0001, 0x0000);

	/* 5. Polling RG_OTP_RD_BUSY */
	do {
		regmap_read(priv->regmap, MT6358_OTP_CON13, &ret);
		ret = ret & 0x0001;
		usleep_range(100, 200);
		dev_dbg(priv->dev, "%s(), polling MT6358_OTP_CON13 = 0x%x\n",
			__func__, ret);
	} while (ret == 1);

	/* Need to delay at least 1ms for 0xC1A and than can read */
	usleep_range(500, 1000);

	/* 6. Read RG_OTP_DOUT_SW */
	regmap_read(priv->regmap, MT6358_OTP_CON12, &ret);
	dev_dbg(priv->dev, "%s(), efuse = 0x%x\n",
		__func__, ret);

	sign = (ret >> 7) & 0x1;
	value = ret & 0x7f;
	value = sign ? -value : value;

	/* 7. Disables efuse_ctrl egine clock */
	regmap_update_bits(priv->regmap, MT6358_OTP_CON11, 0x0001, 0x0000);
	regmap_update_bits(priv->regmap, MT6358_TOP_CKPDN_CON0_SET,
			   0x1 << 4, 0x1 << 4);
	regmap_update_bits(priv->regmap, MT6358_TOP_CKHWEN_CON0_SET,
			   0x1 << 2, 0x1 << 2);

	dev_dbg(priv->dev, "%s(), efuse: %d\n", __func__, value);
	return value;
}

static int mt6358_codec_probe(struct snd_soc_component *cmpnt)
{
	struct mt6358_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	snd_soc_component_init_regmap(cmpnt, priv->regmap);

	/* add codec controls */
	snd_soc_add_component_controls(cmpnt,
				       mt6358_snd_controls,
				       ARRAY_SIZE(mt6358_snd_controls));
	snd_soc_add_component_controls(cmpnt,
				       mt6358_snd_ul_controls,
				       ARRAY_SIZE(mt6358_snd_ul_controls));
	snd_soc_add_component_controls(cmpnt,
				       mt6358_snd_misc_controls,
				       ARRAY_SIZE(mt6358_snd_misc_controls));
	snd_soc_add_component_controls(cmpnt,
				       mt6358_snd_vow_controls,
				       ARRAY_SIZE(mt6358_snd_vow_controls));

	mt6358_codec_init_reg(priv);

	priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL] = 8;
	priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTR] = 8;
	priv->ana_gain[AUDIO_ANALOG_VOLUME_LINEOUTL] = 8;
	priv->ana_gain[AUDIO_ANALOG_VOLUME_LINEOUTR] = 8;
	priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP1] = 3;
	priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP2] = 3;

	priv->hp_current_calibrate_val = get_hp_current_calibrate_val(priv);

	return 0;
}

static struct snd_soc_component_driver mt6358_soc_component_driver = {
	.name = CODEC_MT6358_NAME,
	.probe = mt6358_codec_probe,
	.dapm_widgets = mt6358_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6358_dapm_widgets),
	.dapm_routes = mt6358_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(mt6358_dapm_routes),
};

#ifdef CONFIG_DEBUG_FS
static void debug_write_reg(struct file *file, void *arg)
{
	struct mt6358_priv *priv = file->private_data;
	char *token1 = NULL;
	char *token2 = NULL;
	char *temp = arg;
	char delim[] = " ,";
	unsigned int reg_addr = 0;
	unsigned int reg_value = 0;
	int ret = 0;

	token1 = strsep(&temp, delim);
	token2 = strsep(&temp, delim);
	dev_info(priv->dev, "%s(), token1 = %s, token2 = %s, temp = %s\n",
		 __func__, token1, token2, temp);

	if ((token1 != NULL) && (token2 != NULL)) {
		ret = kstrtouint(token1, 16, &reg_addr);
		ret = kstrtouint(token2, 16, &reg_value);
		dev_info(priv->dev, "%s(), reg_addr = 0x%x, reg_value = 0x%x\n",
			 __func__,
			 reg_addr, reg_value);
		regmap_write(priv->regmap, reg_addr, reg_value);
		regmap_read(priv->regmap, reg_addr, &reg_value);
		dev_info(priv->dev, "%s(), reg_addr = 0x%x, reg_value = 0x%x\n",
			 __func__,
			 reg_addr, reg_value);
	} else {
		dev_err(priv->dev, "token1 or token2 is NULL!\n");
	}
}

static void debug_re_trim_offset(struct file *file,
				 void *arg __attribute__((unused)))
{
	struct mt6358_priv *priv = file->private_data;

	dev_info(priv->dev, "start %s\n", __func__);
	get_hp_trim_offset(priv, true);
	dev_info(priv->dev, "end %s\n", __func__);
}

static void debug_set_debug_flag(struct file *file, void *arg)
{
	struct mt6358_priv *priv = file->private_data;
	char *token1 = NULL;
	char *temp = arg;
	char delim[] = " ,";
	int ret = 0;
	unsigned int value;

	token1 = strsep(&temp, delim);
	dev_info(priv->dev, "%s(), token1 = %s, temp = %s\n",
		 __func__, token1, temp);

	if (token1 != NULL) {
		ret = kstrtouint(token1, 16, &value);
		priv->debug_flag = value;
	} else {
		dev_warn(priv->dev, "%s(), token1 is NULL!\n", __func__);
	}
}

struct command_function {
	const char *cmd;
	void (*fn)(struct file *file, void *arg);
};

#define CMD_FN(_cmd, _fn) {	\
	.cmd = _cmd,		\
	.fn = _fn,		\
}

static const struct command_function debug_cmds[] = {
	CMD_FN("write_reg", debug_write_reg),
	CMD_FN("re_trim_offset", debug_re_trim_offset),
	CMD_FN("set_debug_flag", debug_set_debug_flag),
	{}
};

static int mt6358_debugfs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mt6358_debugfs_read(struct file *file, char __user *buf,
				     size_t count, loff_t *pos)
{
	struct mt6358_priv *priv = file->private_data;
	const int size = 12288;
	char *buffer = NULL; /* for reduce kernel stack */
	int n = 0;
	unsigned int value;
	int ret = 0;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	n += scnprintf(buffer + n, size - n, "mtkaif_protocol = %d\n",
		       priv->mtkaif_protocol);
	n += scnprintf(buffer + n, size - n, "dc_trim_data:\n");
	n += scnprintf(buffer + n, size - n, "\tcalibrated = %d\n",
		       priv->dc_trim.calibrated);
	n += scnprintf(buffer + n, size - n, "\toffset_L = %d, offset_R = %d\n",
		       priv->dc_trim.hp_offset[CH_L],
		       priv->dc_trim.hp_offset[CH_R]);
	n += scnprintf(buffer + n, size - n, "\tmic_vinp_mv = %d\n",
		       priv->dc_trim.mic_vinp_mv);
	n += scnprintf(buffer + n, size - n,
		       "\thp_3_pole_trim_setting = 0x%x, hp_4_pole_trim_setting = 0x%x\n",
		       priv->dc_trim.hp_3_pole_trim_setting,
		       priv->dc_trim.hp_4_pole_trim_setting);
	n += scnprintf(buffer + n, size - n,
		       "\tspk_hp_3_pole_trim_setting = 0x%x, spk_hp_4_pole_trim_setting = 0x%x\n",
		       priv->dc_trim.spk_hp_3_pole_trim_setting,
		       priv->dc_trim.spk_hp_4_pole_trim_setting);

	n += scnprintf(buffer + n, size - n, "codec_ops:\n");
	n += scnprintf(buffer + n, size - n, "\tenable_dc_compensation = %p\n",
		       priv->ops.enable_dc_compensation);
	n += scnprintf(buffer + n, size - n, "\tset_lch_dc_compensation = %p\n",
		       priv->ops.set_lch_dc_compensation);
	n += scnprintf(buffer + n, size - n, "\tset_rch_dc_compensation = %p\n",
		       priv->ops.set_rch_dc_compensation);

	n += scnprintf(buffer + n, size - n, "debug_flag = 0x%x\n",
		       priv->debug_flag);

	n += scnprintf(buffer + n, size - n, "hp_impedance = %d\n",
		       priv->hp_impedance);
	n += scnprintf(buffer + n, size - n, "hp_current_calibrate_val = %d\n",
		       priv->hp_current_calibrate_val);

	regmap_read(priv->regmap, MT6358_DRV_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_DRV_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_GPIO_DIR0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_GPIO_DIR0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_GPIO_MODE2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_GPIO_MODE2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_GPIO_MODE3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_GPIO_MODE3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_TOP_CKPDN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_TOP_CKPDN_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_TOP_CKHWEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_TOP_CKHWEN_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_DCXO_CW13, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_DCXO_CW13 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_DCXO_CW14, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_DCXO_CW14 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUXADC_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUXADC_CON10 = 0x%x\n", value);

	regmap_read(priv->regmap, MT6358_AUD_TOP_ID, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_REV0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_DBI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_DXI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_DXI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKPDN_TPM0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CKPDN_TPM0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKPDN_TPM1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CKPDN_TPM1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKPDN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CKPDN_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKPDN_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CKPDN_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKPDN_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CKPDN_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKSEL_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CKSEL_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKSEL_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CKSEL_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKSEL_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CKSEL_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CKTST_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CKTST_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CLK_HWEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CLK_HWEN_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CLK_HWEN_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CLK_HWEN_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_CLK_HWEN_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_CLK_HWEN_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_RST_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_RST_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_RST_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_RST_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_RST_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_RST_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_RST_BANK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_RST_BANK_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_INT_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_INT_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_INT_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_MASK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_INT_MASK_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_MASK_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_INT_MASK_CON0_SET = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_MASK_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_INT_MASK_CON0_CLR = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_STATUS0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_INT_STATUS0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_RAW_STATUS0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_INT_RAW_STATUS0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_INT_MISC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_INT_MISC_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDNCP_CLKDIV_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDNCP_CLKDIV_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDNCP_CLKDIV_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDNCP_CLKDIV_CON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDNCP_CLKDIV_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDNCP_CLKDIV_CON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDNCP_CLKDIV_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDNCP_CLKDIV_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDNCP_CLKDIV_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDNCP_CLKDIV_CON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUD_TOP_MON_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUD_TOP_MON_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDIO_DIG_DSN_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDIO_DIG_DSN_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDIO_DIG_DSN_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDIO_DIG_DSN_DXI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_UL_DL_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_UL_DL_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_DL_SRC2_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_DL_SRC2_CON0_L = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_UL_SRC_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_UL_SRC_CON0_H = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_UL_SRC_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_UL_SRC_CON0_L = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_TOP_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDIO_TOP_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_MON_DEBUG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_MON_DEBUG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFUNC_AUD_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFUNC_AUD_CON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFUNC_AUD_CON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFUNC_AUD_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFUNC_AUD_CON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFUNC_AUD_CON5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFUNC_AUD_CON6 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFUNC_AUD_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFUNC_AUD_MON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDRC_TUNE_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDRC_TUNE_MON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_FIFO_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_FIFO_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_FIFO_LOG_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_FIFO_LOG_MON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_MON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_MON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_MON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_MON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_MON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_RX_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_RX_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_RX_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_RX_CFG2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_RX_CFG3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_RX_CFG3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADDA_MTKAIF_TX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADDA_MTKAIF_TX_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_SGEN_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_SGEN_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_SGEN_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_SGEN_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_ADC_ASYNC_FIFO_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_ADC_ASYNC_FIFO_CFG = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_DCCLK_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_DCCLK_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_DCCLK_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_DCCLK_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDIO_DIG_CFG = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_AUD_PAD_TOP, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_AUD_PAD_TOP = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_AUD_PAD_TOP_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_AUD_PAD_TOP_MON = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_AUD_PAD_TOP_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_AUD_PAD_TOP_MON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_DL_NLE_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_DL_NLE_CFG = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_DL_NLE_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_DL_NLE_MON = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_CG_EN_MON, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_CG_EN_MON = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_2ND_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDIO_DIG_2ND_DSN_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_2ND_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDIO_DIG_2ND_DSN_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_2ND_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDIO_DIG_2ND_DSN_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDIO_DIG_2ND_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDIO_DIG_2ND_DSN_DXI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_PMIC_NEWIF_CFG3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_PMIC_NEWIF_CFG3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_TOP, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_TOP = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_CFG2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_CFG3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG4, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_CFG4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG5, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_CFG5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_CFG6, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_CFG6 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_MON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_MON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_MON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_MON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_MON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_MON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_MON5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_SN_INI_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_SN_INI_CFG = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_TGEN_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_TGEN_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_POSDIV_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_POSDIV_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_HPF_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_HPF_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG4, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG5, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG6, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG6 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG7, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG7 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG8, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG8 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG9, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG9 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG10, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG10 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG11, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG11 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG12, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG12 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG13, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG13 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG14, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG14 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG15, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG15 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG16, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG16 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG17, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG17 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG18, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG18 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG19, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG19 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG20, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG20 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG21, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG21 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG22, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG22 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_CFG23, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_CFG23 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_MON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AFE_VOW_PERIODIC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AFE_VOW_PERIODIC_MON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_DSN_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_DSN_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_DSN_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_DSN_FPI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_DSN_FPI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON6 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON7 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON8 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON9 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON10 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON11 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDENC_ANA_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDENC_ANA_CON12 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_DSN_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_DSN_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_DSN_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_DSN_FPI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_DSN_FPI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON5 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON6 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON7 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON8 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON9 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON10 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON11 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON12 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON13 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON14 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ANA_CON15, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ANA_CON15 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_NUM, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ELR_NUM = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDDEC_ELR_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDDEC_ELR_0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDZCD_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDZCD_DSN_ID = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDZCD_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDZCD_DSN_REV0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDZCD_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDZCD_DSN_DBI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_AUDZCD_DSN_FPI, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_AUDZCD_DSN_FPI = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_ZCD_CON0 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_ZCD_CON1 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_ZCD_CON2 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_ZCD_CON3 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_ZCD_CON4 = 0x%x\n", value);
	regmap_read(priv->regmap, MT6358_ZCD_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6358_ZCD_CON5 = 0x%x\n", value);

	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	kfree(buffer);
	return ret;
}

static ssize_t mt6358_debugfs_write(struct file *f, const char __user *buf,
				    size_t count, loff_t *offset)
{
#define MAX_DEBUG_WRITE_INPUT 256
	struct mt6358_priv *priv = f->private_data;
	char input[MAX_DEBUG_WRITE_INPUT];
	char *temp = NULL;
	char *command = NULL;
	char *str_begin = NULL;
	char delim[] = " ,";
	const struct command_function *cf = NULL;

	if (!count) {
		dev_info(priv->dev, "%s(), count is 0, return directly\n",
			 __func__);
		goto exit;
	}

	if (count > MAX_DEBUG_WRITE_INPUT)
		count = MAX_DEBUG_WRITE_INPUT;

	memset((void *)input, 0, MAX_DEBUG_WRITE_INPUT);

	if (copy_from_user(input, buf, count))
		dev_warn(priv->dev, "%s(), copy_from_user fail, count = %zu\n",
			 __func__, count);

	str_begin = kstrndup(input, MAX_DEBUG_WRITE_INPUT - 1,
			     GFP_KERNEL);
	if (!str_begin) {
		dev_info(priv->dev, "%s(), kstrdup fail\n", __func__);
		goto exit;
	}
	temp = str_begin;

	command = strsep(&temp, delim);

	dev_info(priv->dev, "%s(), command %s, content %s\n",
		 __func__, command, temp);

	for (cf = debug_cmds; cf->cmd; cf++) {
		if (strcmp(cf->cmd, command) == 0) {
			cf->fn(f, temp);
			break;
		}
	}

	kfree(str_begin);
exit:
	return count;
}

static const struct file_operations mt6358_debugfs_ops = {
	.open = mt6358_debugfs_open,
	.write = mt6358_debugfs_write,
	.read = mt6358_debugfs_read,
};
#endif

#ifndef CONFIG_MTK_PMIC_WRAP
#ifdef CONFIG_MTK_PMIC_WRAP_HAL
static DEFINE_SPINLOCK(codec_set_reg_lock);
#endif
static unsigned int codec_get_reg(unsigned int offset)
{
#ifdef CONFIG_MTK_PMIC_WRAP_HAL
	int ret = 0;
	unsigned int data = 0;
#ifdef DEBUG_PMIC_WRAP
	pr_info("%s(), call pwrap_read, offset = 0x%x\n",
			__func__, offset);
#endif
	ret = pwrap_read(offset, &data);

	return data;
#else
	return 0;
#endif
}

static void codec_set_reg(unsigned int offset,
			unsigned int value,
			unsigned int mask)
{
	int ret = 0;
	unsigned int reg_value;
	unsigned long flags = 0;

#ifdef CONFIG_MTK_PMIC_WRAP_HAL
	spin_lock_irqsave(&codec_set_reg_lock, flags);
	reg_value = codec_get_reg(offset);
	reg_value &= (~mask);
	reg_value |= (value & mask);
#ifdef DEBUG_PMIC_WRAP
	pr_info("%s(), call pwrap_write, offset = 0x%x, value = 0x%x, mask = 0x%x\n",
			__func__, offset, value, mask);
#endif
	ret = pwrap_write(offset, reg_value);
	spin_unlock_irqrestore(&codec_set_reg_lock, flags);

	reg_value = codec_get_reg(offset);
	if ((reg_value & mask) != (value & mask))
		pr_warn("%s(), offset = 0x%x, mask = 0x%x, ret = %d, reg_value = 0x%x\n",
				__func__, offset, mask, ret, reg_value);
#endif
}

static bool is_writeable_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static bool is_volatile_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static bool is_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MT6358_DRV_CON3:
	case MT6358_GPIO_DIR0:
	case MT6358_GPIO_MODE2:
	case MT6358_GPIO_MODE3:
	case MT6358_TOP_CKPDN_CON0:
	case MT6358_TOP_CKHWEN_CON0:
	case MT6358_OTP_CON0:
	case MT6358_OTP_CON8:
	case MT6358_OTP_CON11:
	case MT6358_OTP_CON12:
	case MT6358_OTP_CON13:
	case MT6358_DCXO_CW13:
	case MT6358_DCXO_CW14:
	case MT6358_AUXADC_CON10:
		return true;
	default:
		break;
	};

	if (reg >= MT6358_AUD_TOP_ID && reg <= MT6358_ZCD_CON5)
		return true;

	return false;
}

static int reg_read(void *context, unsigned int reg, unsigned int *val)
{
	*val = codec_get_reg(reg);
	return 0;
}

static int reg_write(void *context, unsigned int reg, unsigned int val)
{
	codec_set_reg(reg, val, 0xffff);
	return 0;
}

#define REG_STRIDE 2
static const struct regmap_config mt6358_regmap = {
	.reg_bits = 16,
	.val_bits = 16,
	.reg_stride = REG_STRIDE,

	.max_register = MT6358_MAX_REGISTER,
	.writeable_reg = is_writeable_reg,
	.volatile_reg = is_volatile_reg,
	.readable_reg = is_readable_reg,

	.reg_read = reg_read,
	.reg_write = reg_write,

	.cache_type = REGCACHE_NONE,
};
#endif

static void mt6358_parse_dt(struct mt6358_priv *priv)
{
	int ret;
	struct device *dev = priv->dev;

	ret = of_property_read_u32(dev->of_node, "mediatek,dmic-mode",
				   &priv->dmic_one_wire_mode);
	if (ret) {
		dev_info(dev, "%s() failed to read dmic-mode, default 2 wire\n",
			 __func__);
		priv->dmic_one_wire_mode = 0;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,mic-type",
				   &priv->mux_select[MUX_MIC_TYPE]);
	if (ret) {
		dev_info(dev, "%s() failed to read mic-type, default ACC\n",
			 __func__);
		priv->mux_select[MUX_MIC_TYPE] = MIC_TYPE_MUX_ACC;
	}

	ret = of_property_read_bool(dev->of_node, "vow_dmic_lp");
	if (ret) {
		priv->vow_dmic_lp = 1;
	} else {
		dev_info(dev, "%s() vow_dmic_lp node not exist, default off.\n",
			 __func__);
		priv->vow_dmic_lp = 0;
	}
}

static int mt6358_platform_driver_probe(struct platform_device *pdev)
{
	struct mt6358_priv *priv;
#ifdef CONFIG_MTK_PMIC_WRAP
	struct device_node *pwrap_node = NULL;
#endif

	priv = devm_kzalloc(&pdev->dev,
			    sizeof(struct mt6358_priv),
			    GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, priv);

	priv->dev = &pdev->dev;

#ifndef CONFIG_MTK_PMIC_WRAP
	priv->regmap = devm_regmap_init(&pdev->dev, NULL, NULL, &mt6358_regmap);
#else
	pwrap_node = of_parse_phandle(pdev->dev.of_node,
					  "mediatek,pwrap-regmap", 0);
	if (pwrap_node) {
		priv->regmap = pwrap_node_to_regmap(pwrap_node);
		if (IS_ERR(priv->regmap))
			return PTR_ERR(priv->regmap);
	} else {
		dev_err(&pdev->dev, "get pwrap node fail\n");
		return -EINVAL;
	}
#endif

	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

#ifdef CONFIG_DEBUG_FS
	/* create debugfs file */
	priv->debugfs = debugfs_create_file("mtksocanaaudio",
					    S_IFREG | 0444, NULL,
					    priv, &mt6358_debugfs_ops);
#endif
	mt6358_parse_dt(priv);

	dev_info(priv->dev, "%s(), dev name %s\n",
		 __func__, dev_name(&pdev->dev));

	return snd_soc_register_component(&pdev->dev,
				      &mt6358_soc_component_driver,
				      mt6358_dai_driver,
				      ARRAY_SIZE(mt6358_dai_driver));
}

static int mt6358_platform_driver_remove(struct platform_device *pdev)
{
#ifdef CONFIG_DEBUG_FS
	struct mt6358_priv *priv = dev_get_drvdata(&pdev->dev);

	dev_info(&pdev->dev, "%s()\n", __func__);

	debugfs_remove(priv->debugfs);
#endif
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id mt6358_of_match[] = {
	{.compatible = "mediatek,mt6358-sound",},
	{.compatible = "mediatek,mt6366-sound",},
	{}
};
MODULE_DEVICE_TABLE(of, mt6358_of_match);

static struct platform_driver mt6358_platform_driver = {
	.driver = {
		.name = "mt6358-sound",
		.of_match_table = mt6358_of_match,
	},
	.probe = mt6358_platform_driver_probe,
	.remove = mt6358_platform_driver_remove,
};

module_platform_driver(mt6358_platform_driver)

/* Module information */
MODULE_DESCRIPTION("MT6358 ALSA SoC codec driver");
MODULE_AUTHOR("KaiChieh Chuang <kaichieh.chuang@mediatek.com>");
MODULE_LICENSE("GPL v2");
