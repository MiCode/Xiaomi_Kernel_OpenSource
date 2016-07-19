/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-irq.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include <linux/mfd/wcd934x/registers.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <linux/regulator/consumer.h>
#include <linux/soundwire/swr-wcd.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/info.h>
#include "wcd934x.h"
#include "wcd934x-routing.h"
#include "../wcd9xxx-common-v2.h"
#include "../wcd9xxx-resmgr-v2.h"

#define WCD934X_RX_PORT_START_NUMBER  16

#define WCD934X_RATES_MASK (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			    SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			    SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 |\
			    SNDRV_PCM_RATE_384000)
/* Fractional Rates */
#define WCD934X_FRAC_RATES_MASK (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 |\
				 SNDRV_PCM_RATE_176400)

#define WCD934X_FORMATS_S16_S24_LE (SNDRV_PCM_FMTBIT_S16_LE | \
				    SNDRV_PCM_FMTBIT_S24_LE)

#define WCD934X_FORMATS_S16_S24_S32_LE (SNDRV_PCM_FMTBIT_S16_LE | \
					SNDRV_PCM_FMTBIT_S24_LE | \
					SNDRV_PCM_FMTBIT_S32_LE)

#define WCD934X_FORMATS_S16_LE (SNDRV_PCM_FMTBIT_S16_LE)

#define STRING(name) #name
#define WCD_DAPM_ENUM(name, reg, offset, text) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM(STRING(name), name##_enum)

#define WCD_DAPM_ENUM_EXT(name, reg, offset, text, getname, putname) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM_EXT(STRING(name), name##_enum, getname, putname)

#define WCD_DAPM_MUX(name, shift, kctl) \
		SND_SOC_DAPM_MUX(name, SND_SOC_NOPM, shift, 0, &kctl##_mux)

/*
 * Timeout in milli seconds and it is the wait time for
 * slim channel removal interrupt to receive.
 */
#define WCD934X_SLIM_CLOSE_TIMEOUT 1000
#define WCD934X_SLIM_IRQ_OVERFLOW (1 << 0)
#define WCD934X_SLIM_IRQ_UNDERFLOW (1 << 1)
#define WCD934X_SLIM_IRQ_PORT_CLOSED (1 << 2)
#define WCD934X_MCLK_CLK_12P288MHZ 12288000
#define WCD934X_MCLK_CLK_9P6MHZ 9600000

#define WCD934X_NUM_INTERPOLATORS 9
#define WCD934X_NUM_DECIMATORS 9

#define BYTE_BIT_MASK(nr) (1 << ((nr) % BITS_PER_BYTE))

#define WCD934X_REG_BITS 8
#define WCD934X_MAX_VALID_ADC_MUX  13
#define WCD934X_INVALID_ADC_MUX 9

#define WCD934X_AMIC_PWR_LEVEL_LP 0
#define WCD934X_AMIC_PWR_LEVEL_DEFAULT 1
#define WCD934X_AMIC_PWR_LEVEL_HP 2
#define WCD934X_AMIC_PWR_LVL_MASK 0x60
#define WCD934X_AMIC_PWR_LVL_SHIFT 0x5

#define WCD934X_DEC_PWR_LVL_MASK 0x06
#define WCD934X_DEC_PWR_LVL_LP 0x02
#define WCD934X_DEC_PWR_LVL_HP 0x04
#define WCD934X_DEC_PWR_LVL_DF 0x00
#define WCD934X_STRING_LEN 100

#define WCD934X_MAX_MICBIAS 4
#define DAPM_MICBIAS1_STANDALONE "MIC BIAS1 Standalone"
#define DAPM_MICBIAS2_STANDALONE "MIC BIAS2 Standalone"
#define DAPM_MICBIAS3_STANDALONE "MIC BIAS3 Standalone"
#define DAPM_MICBIAS4_STANDALONE "MIC BIAS4 Standalone"

#define  TX_HPF_CUT_OFF_FREQ_MASK	0x60
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

enum {
	AUDIO_NOMINAL,
	HPH_PA_DELAY,
};

enum {
	MIC_BIAS_1 = 1,
	MIC_BIAS_2,
	MIC_BIAS_3,
	MIC_BIAS_4
};

enum {
	MICB_PULLUP_ENABLE,
	MICB_PULLUP_DISABLE,
	MICB_ENABLE,
	MICB_DISABLE,
};

enum {
	AIF1_PB = 0,
	AIF1_CAP,
	AIF2_PB,
	AIF2_CAP,
	AIF3_PB,
	AIF3_CAP,
	AIF4_PB,
	NUM_CODEC_DAIS,
};

enum {
	INTn_1_INP_SEL_ZERO = 0,
	INTn_1_INP_SEL_DEC0,
	INTn_1_INP_SEL_DEC1,
	INTn_1_INP_SEL_IIR0,
	INTn_1_INP_SEL_IIR1,
	INTn_1_INP_SEL_RX0,
	INTn_1_INP_SEL_RX1,
	INTn_1_INP_SEL_RX2,
	INTn_1_INP_SEL_RX3,
	INTn_1_INP_SEL_RX4,
	INTn_1_INP_SEL_RX5,
	INTn_1_INP_SEL_RX6,
	INTn_1_INP_SEL_RX7,
};

enum {
	INTn_2_INP_SEL_ZERO = 0,
	INTn_2_INP_SEL_RX0,
	INTn_2_INP_SEL_RX1,
	INTn_2_INP_SEL_RX2,
	INTn_2_INP_SEL_RX3,
	INTn_2_INP_SEL_RX4,
	INTn_2_INP_SEL_RX5,
	INTn_2_INP_SEL_RX6,
	INTn_2_INP_SEL_RX7,
	INTn_2_INP_SEL_PROXIMITY,
};

enum {
	INTERP_EAR = 0,
	INTERP_HPHL,
	INTERP_HPHR,
	INTERP_LO1,
	INTERP_LO2,
	INTERP_LO3_NA, /* LO3 not avalible in Tavil*/
	INTERP_LO4_NA,
	INTERP_SPKR1,
	INTERP_SPKR2,
};

static const struct intr_data wcd934x_intr_table[] = {
	{WCD9XXX_IRQ_SLIMBUS, false},
	{WCD934X_IRQ_MBHC_SW_DET, true},
	{WCD934X_IRQ_MBHC_BUTTON_PRESS_DET, true},
	{WCD934X_IRQ_MBHC_BUTTON_RELEASE_DET, true},
	{WCD934X_IRQ_MBHC_ELECT_INS_REM_DET, true},
	{WCD934X_IRQ_MBHC_ELECT_INS_REM_LEG_DET, true},
	{WCD934X_IRQ_FLL_LOCK_LOSS, false},
	{WCD934X_IRQ_HPH_PA_CNPL_COMPLETE, false},
	{WCD934X_IRQ_HPH_PA_CNPR_COMPLETE, false},
	{WCD934X_IRQ_EAR_PA_CNP_COMPLETE, false},
	{WCD934X_IRQ_LINE_PA1_CNP_COMPLETE, false},
	{WCD934X_IRQ_LINE_PA2_CNP_COMPLETE, false},
	{WCD934X_IRQ_SLNQ_ANALOG_ERROR, false},
	{WCD934X_IRQ_RESERVED_3, false},
	{WCD934X_IRQ_HPH_PA_OCPL_FAULT, false},
	{WCD934X_IRQ_HPH_PA_OCPR_FAULT, false},
	{WCD934X_IRQ_EAR_PA_OCP_FAULT, false},
	{WCD934X_IRQ_SOUNDWIRE, false},
	{WCD934X_IRQ_VDD_DIG_RAMP_COMPLETE, false},
	{WCD934X_IRQ_RCO_ERROR, false},
	{WCD934X_IRQ_CPE_ERROR, false},
	{WCD934X_IRQ_MAD_AUDIO, false},
	{WCD934X_IRQ_MAD_BEACON, false},
	{WCD934X_IRQ_CPE1_INTR, true},
	{WCD934X_IRQ_RESERVED_4, false},
	{WCD934X_IRQ_MAD_ULTRASOUND, false},
	{WCD934X_IRQ_VBAT_ATTACK, false},
	{WCD934X_IRQ_VBAT_RESTORE, false},
};

struct interp_sample_rate {
	int sample_rate;
	int rate_val;
};

static struct interp_sample_rate sr_val_tbl[] = {
	{8000, 0x0}, {16000, 0x1}, {32000, 0x3}, {48000, 0x4}, {96000, 0x5},
	{192000, 0x6}, {384000, 0x7}, {44100, 0x9}, {88200, 0xA},
	{176400, 0xB}, {352800, 0xC},
};

static const struct wcd9xxx_ch tavil_rx_chs[WCD934X_RX_MAX] = {
	WCD9XXX_CH(WCD934X_RX_PORT_START_NUMBER, 0),
	WCD9XXX_CH(WCD934X_RX_PORT_START_NUMBER + 1, 1),
	WCD9XXX_CH(WCD934X_RX_PORT_START_NUMBER + 2, 2),
	WCD9XXX_CH(WCD934X_RX_PORT_START_NUMBER + 3, 3),
	WCD9XXX_CH(WCD934X_RX_PORT_START_NUMBER + 4, 4),
	WCD9XXX_CH(WCD934X_RX_PORT_START_NUMBER + 5, 5),
	WCD9XXX_CH(WCD934X_RX_PORT_START_NUMBER + 6, 6),
	WCD9XXX_CH(WCD934X_RX_PORT_START_NUMBER + 7, 7),
};

static const struct wcd9xxx_ch tavil_tx_chs[WCD934X_TX_MAX] = {
	WCD9XXX_CH(0, 0),
	WCD9XXX_CH(1, 1),
	WCD9XXX_CH(2, 2),
	WCD9XXX_CH(3, 3),
	WCD9XXX_CH(4, 4),
	WCD9XXX_CH(5, 5),
	WCD9XXX_CH(6, 6),
	WCD9XXX_CH(7, 7),
	WCD9XXX_CH(8, 8),
	WCD9XXX_CH(9, 9),
	WCD9XXX_CH(10, 10),
	WCD9XXX_CH(11, 11),
	WCD9XXX_CH(12, 12),
	WCD9XXX_CH(13, 13),
	WCD9XXX_CH(14, 14),
	WCD9XXX_CH(15, 15),
};

static const u32 vport_slim_check_table[NUM_CODEC_DAIS] = {
	0,				/* AIF1_PB */
	BIT(AIF2_CAP) | BIT(AIF3_CAP),	/* AIF1_CAP */
	0,				/* AIF2_PB */
	BIT(AIF1_CAP) | BIT(AIF3_CAP),	/* AIF2_CAP */
	0,				/* AIF3_PB */
	BIT(AIF1_CAP) | BIT(AIF2_CAP),	/* AIF3_CAP */
	0,				/* AIF4_PB */
};

/* Codec supports 2 IIR filters */
enum {
	IIR0 = 0,
	IIR1,
	IIR_MAX,
};

/* Each IIR has 5 Filter Stages */
enum {
	BAND1 = 0,
	BAND2,
	BAND3,
	BAND4,
	BAND5,
	BAND_MAX,
};

enum {
	COMPANDER_1, /* HPH_L */
	COMPANDER_2, /* HPH_R */
	COMPANDER_3, /* LO1_DIFF */
	COMPANDER_4, /* LO2_DIFF */
	COMPANDER_5, /* LO3_SE - not used in Tavil */
	COMPANDER_6, /* LO4_SE - not used in Tavil */
	COMPANDER_7, /* SWR SPK CH1 */
	COMPANDER_8, /* SWR SPK CH2 */
	COMPANDER_MAX,
};

static struct afe_param_slimbus_slave_port_cfg tavil_slimbus_slave_port_cfg = {
	.minor_version = 1,
	.slimbus_dev_id = AFE_SLIMBUS_DEVICE_1,
	.slave_dev_pgd_la = 0,
	.slave_dev_intfdev_la = 0,
	.bit_width = 16,
	.data_format = 0,
	.num_channels = 1
};

static struct afe_param_cdc_reg_page_cfg tavil_cdc_reg_page_cfg = {
	.minor_version = AFE_API_VERSION_CDC_REG_PAGE_CFG,
	.enable = 1,
	.proc_id = AFE_CDC_REG_PAGE_ASSIGN_PROC_ID_1,
};

static struct afe_param_cdc_reg_cfg audio_reg_cfg[] = {
	{
		1,
		(WCD934X_REGISTER_START_OFFSET + WCD934X_SOC_MAD_MAIN_CTL_1),
		HW_MAD_AUDIO_ENABLE, 0x1, WCD934X_REG_BITS, 0
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET + WCD934X_SOC_MAD_AUDIO_CTL_3),
		HW_MAD_AUDIO_SLEEP_TIME, 0xF, WCD934X_REG_BITS, 0
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET + WCD934X_SOC_MAD_AUDIO_CTL_4),
		HW_MAD_TX_AUDIO_SWITCH_OFF, 0x1, WCD934X_REG_BITS, 0
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET + WCD934X_INTR_CFG),
		MAD_AUDIO_INT_DEST_SELECT_REG, 0x2, WCD934X_REG_BITS, 0
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET + WCD934X_INTR_PIN2_MASK3),
		MAD_AUDIO_INT_MASK_REG, 0x1, WCD934X_REG_BITS, 0
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET + WCD934X_INTR_PIN2_STATUS3),
		MAD_AUDIO_INT_STATUS_REG, 0x1, WCD934X_REG_BITS, 0
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET + WCD934X_INTR_PIN2_CLEAR3),
		MAD_AUDIO_INT_CLEAR_REG, 0x1, WCD934X_REG_BITS, 0
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET + WCD934X_SB_PGD_PORT_TX_BASE),
		SB_PGD_PORT_TX_WATERMARK_N, 0x1E, WCD934X_REG_BITS, 0x1
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET + WCD934X_SB_PGD_PORT_TX_BASE),
		SB_PGD_PORT_TX_ENABLE_N, 0x1, WCD934X_REG_BITS, 0x1
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET + WCD934X_SB_PGD_PORT_RX_BASE),
		SB_PGD_PORT_RX_WATERMARK_N, 0x1E, WCD934X_REG_BITS, 0x1
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET + WCD934X_SB_PGD_PORT_RX_BASE),
		SB_PGD_PORT_RX_ENABLE_N, 0x1, WCD934X_REG_BITS, 0x1
	},
};

static struct afe_param_cdc_reg_cfg_data tavil_audio_reg_cfg = {
	.num_registers = ARRAY_SIZE(audio_reg_cfg),
	.reg_data = audio_reg_cfg,
};

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static const DECLARE_TLV_DB_SCALE(line_gain, 0, 7, 1);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);

#define WCD934X_TX_UNMUTE_DELAY_MS 25

static int tx_unmute_delay = WCD934X_TX_UNMUTE_DELAY_MS;
module_param(tx_unmute_delay, int,
	     S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(tx_unmute_delay, "delay to unmute the tx path");


/* Hold instance to soundwire platform device */
struct tavil_swr_ctrl_data {
	struct platform_device *swr_pdev;
};

struct wcd_swr_ctrl_platform_data {
	void *handle; /* holds codec private data */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*bulk_write)(void *handle, u32 *reg, u32 *val, size_t len);
	int (*clk)(void *handle, bool enable);
	int (*handle_irq)(void *handle,
			  irqreturn_t (*swrm_irq_handler)(int irq, void *data),
			  void *swrm_handle, int action);
};

/* Holds all Soundwire and speaker related information */
struct wcd934x_swr {
	struct tavil_swr_ctrl_data *ctrl_data;
	struct wcd_swr_ctrl_platform_data plat_data;
	struct mutex read_mutex;
	struct mutex write_mutex;
	struct mutex clk_mutex;
	int spkr_gain_offset;
	int spkr_mode;
	int clk_users;
	int rx_7_count;
	int rx_8_count;
};

struct tx_mute_work {
	struct tavil_priv *tavil;
	u8 decimator;
	struct delayed_work dwork;
};

struct hpf_work {
	struct tavil_priv *tavil;
	u8 decimator;
	u8 hpf_cut_off_freq;
	struct delayed_work dwork;
};

struct tavil_priv {
	struct device *dev;
	struct wcd9xxx *wcd9xxx;
	struct snd_soc_codec *codec;
	u32 rx_bias_count;
	s32 dmic_0_1_clk_cnt;
	s32 dmic_2_3_clk_cnt;
	s32 dmic_4_5_clk_cnt;

	/* compander */
	int comp_enabled[COMPANDER_MAX];
	/* class h specific data */
	struct wcd_clsh_cdc_data clsh_d;
	/* Tavil Interpolator Mode Select for EAR, HPH_L and HPH_R */
	u32 hph_mode;

	u16 prim_int_users[WCD934X_NUM_INTERPOLATORS];
	/* to track the status */
	unsigned long status_mask;

	struct afe_param_cdc_slimbus_slave_cfg slimbus_slave_cfg;

	/* num of slim ports required */
	struct wcd9xxx_codec_dai_data  dai[NUM_CODEC_DAIS];
	/* Port values for Rx and Tx codec_dai */
	unsigned int rx_port_value;
	unsigned int tx_port_value;

	struct wcd9xxx_resmgr_v2 *resmgr;
	struct wcd934x_swr swr;

	struct clk *wcd_ext_clk;

	struct mutex codec_mutex;
	struct work_struct tavil_add_child_devices_work;
	struct hpf_work tx_hpf_work[WCD934X_NUM_DECIMATORS];
	struct tx_mute_work tx_mute_dwork[WCD934X_NUM_DECIMATORS];
};

static const struct tavil_reg_mask_val tavil_spkr_default[] = {
	{WCD934X_CDC_COMPANDER7_CTL3, 0x80, 0x80},
	{WCD934X_CDC_COMPANDER8_CTL3, 0x80, 0x80},
	{WCD934X_CDC_COMPANDER7_CTL7, 0x01, 0x01},
	{WCD934X_CDC_COMPANDER8_CTL7, 0x01, 0x01},
	{WCD934X_CDC_BOOST0_BOOST_CTL, 0x7C, 0x50},
	{WCD934X_CDC_BOOST1_BOOST_CTL, 0x7C, 0x50},
};

static const struct tavil_reg_mask_val tavil_spkr_mode1[] = {
	{WCD934X_CDC_COMPANDER7_CTL3, 0x80, 0x00},
	{WCD934X_CDC_COMPANDER8_CTL3, 0x80, 0x00},
	{WCD934X_CDC_COMPANDER7_CTL7, 0x01, 0x00},
	{WCD934X_CDC_COMPANDER8_CTL7, 0x01, 0x00},
	{WCD934X_CDC_BOOST0_BOOST_CTL, 0x7C, 0x44},
	{WCD934X_CDC_BOOST1_BOOST_CTL, 0x7C, 0x44},
};

/*
 * wcd934x_get_codec_info: Get codec specific information
 *
 * @wcd9xxx: pointer to wcd9xxx structure
 * @wcd_type: pointer to wcd9xxx_codec_type structure
 *
 * Returns 0 for success or negative error code for failure
 */
int wcd934x_get_codec_info(struct wcd9xxx *wcd9xxx,
			   struct wcd9xxx_codec_type *wcd_type)
{
	u16 id_minor, id_major;
	struct regmap *wcd_regmap;
	int rc, version = 0;

	if (!wcd9xxx || !wcd_type)
		return -EINVAL;

	if (!wcd9xxx->regmap) {
		dev_err(wcd9xxx->dev, "%s: wcd9xxx regmap is null\n", __func__);
		return -EINVAL;
	}
	wcd_regmap = wcd9xxx->regmap;

	rc = regmap_bulk_read(wcd_regmap, WCD934X_CHIP_TIER_CTRL_CHIP_ID_BYTE0,
			      (u8 *)&id_minor, sizeof(u16));
	if (rc)
		return -EINVAL;

	rc = regmap_bulk_read(wcd_regmap, WCD934X_CHIP_TIER_CTRL_CHIP_ID_BYTE2,
			      (u8 *)&id_major, sizeof(u16));
	if (rc)
		return -EINVAL;

	dev_info(wcd9xxx->dev, "%s: wcd9xxx chip id major 0x%x, minor 0x%x\n",
		 __func__, id_major, id_minor);

	/* Version detection */
	version = 1.0;

	/* Fill codec type info */
	wcd_type->id_major = id_major;
	wcd_type->id_minor = id_minor;
	wcd_type->num_irqs = WCD934X_NUM_IRQS;
	wcd_type->version = version;
	wcd_type->slim_slave_type = WCD9XXX_SLIM_SLAVE_ADDR_TYPE_1;
	wcd_type->i2c_chip_status = 0x01;
	wcd_type->intr_tbl = wcd934x_intr_table;
	wcd_type->intr_tbl_size = ARRAY_SIZE(wcd934x_intr_table);

	wcd_type->intr_reg[WCD9XXX_INTR_STATUS_BASE] =
						WCD934X_INTR_PIN1_STATUS0;
	wcd_type->intr_reg[WCD9XXX_INTR_CLEAR_BASE] =
						WCD934X_INTR_PIN1_CLEAR0;
	wcd_type->intr_reg[WCD9XXX_INTR_MASK_BASE] =
						WCD934X_INTR_PIN1_MASK0;
	wcd_type->intr_reg[WCD9XXX_INTR_LEVEL_BASE] =
						WCD934X_INTR_LEVEL0;
	wcd_type->intr_reg[WCD9XXX_INTR_CLR_COMMIT] =
						WCD934X_INTR_CLR_COMMIT;

	return rc;
}
EXPORT_SYMBOL(wcd934x_get_codec_info);

/*
 * wcd934x_bringdown: Bringdown WCD Codec
 *
 * @wcd9xxx: Pointer to wcd9xxx structure
 *
 * Returns 0 for success or negative error code for failure
 */
int wcd934x_bringdown(struct wcd9xxx *wcd9xxx)
{
	if (!wcd9xxx || !wcd9xxx->regmap)
		return -EINVAL;

	regmap_write(wcd9xxx->regmap, WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
		     0x04);

	return 0;
}
EXPORT_SYMBOL(wcd934x_bringdown);

/*
 * wcd934x_bringup: Bringup WCD Codec
 *
 * @wcd9xxx: Pointer to the wcd9xxx structure
 *
 * Returns 0 for success or negative error code for failure
 */
int wcd934x_bringup(struct wcd9xxx *wcd9xxx)
{
	struct regmap *wcd_regmap;

	if (!wcd9xxx)
		return -EINVAL;

	if (!wcd9xxx->regmap) {
		dev_err(wcd9xxx->dev, "%s: wcd9xxx regmap is null!\n",
			__func__);
		return -EINVAL;
	}
	wcd_regmap = wcd9xxx->regmap;

	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_RST_CTL, 0x01);
	regmap_write(wcd_regmap, WCD934X_SIDO_NEW_VOUT_A_STARTUP, 0x19);
	regmap_write(wcd_regmap, WCD934X_SIDO_NEW_VOUT_D_STARTUP, 0x15);
	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x5);
	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x7);
	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x3);
	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_RST_CTL, 0x3);

	return 0;
}
EXPORT_SYMBOL(wcd934x_bringup);

/**
 * tavil_set_spkr_gain_offset - offset the speaker path
 * gain with the given offset value.
 *
 * @codec: codec instance
 * @offset: Indicates speaker path gain offset value.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int tavil_set_spkr_gain_offset(struct snd_soc_codec *codec, int offset)
{
	struct tavil_priv *priv = snd_soc_codec_get_drvdata(codec);

	if (!priv)
		return -EINVAL;

	priv->swr.spkr_gain_offset = offset;
	return 0;
}
EXPORT_SYMBOL(tavil_set_spkr_gain_offset);

/**
 * tavil_set_spkr_mode - Configures speaker compander and smartboost
 * settings based on speaker mode.
 *
 * @codec: codec instance
 * @mode: Indicates speaker configuration mode.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int tavil_set_spkr_mode(struct snd_soc_codec *codec, int mode)
{
	struct tavil_priv *priv = snd_soc_codec_get_drvdata(codec);
	int i;
	const struct tavil_reg_mask_val *regs;
	int size;

	if (!priv)
		return -EINVAL;

	switch (mode) {
	case WCD934X_SPKR_MODE_1:
		regs = tavil_spkr_mode1;
		size = ARRAY_SIZE(tavil_spkr_mode1);
		break;
	default:
		regs = tavil_spkr_default;
		size = ARRAY_SIZE(tavil_spkr_default);
		break;
	}

	priv->swr.spkr_mode = mode;
	for (i = 0; i < size; i++)
		snd_soc_update_bits(codec, regs[i].reg,
				    regs[i].mask, regs[i].val);
	return 0;
}
EXPORT_SYMBOL(tavil_set_spkr_mode);

/**
 * tavil_get_afe_config - returns specific codec configuration to afe to write
 *
 * @codec: codec instance
 * @config_type: Indicates type of configuration to write.
 */
void *tavil_get_afe_config(struct snd_soc_codec *codec,
			   enum afe_config_type config_type)
{
	struct tavil_priv *priv = snd_soc_codec_get_drvdata(codec);

	switch (config_type) {
	case AFE_SLIMBUS_SLAVE_CONFIG:
		return &priv->slimbus_slave_cfg;
	case AFE_CDC_REGISTERS_CONFIG:
		return &tavil_audio_reg_cfg;
	case AFE_SLIMBUS_SLAVE_PORT_CONFIG:
		return &tavil_slimbus_slave_port_cfg;
	case AFE_AANC_VERSION:
		return NULL;
	case AFE_CDC_REGISTER_PAGE_CONFIG:
		return &tavil_cdc_reg_page_cfg;
	default:
		dev_info(codec->dev, "%s: Unknown config_type 0x%x\n",
			__func__, config_type);
		return NULL;
	}
}
EXPORT_SYMBOL(tavil_get_afe_config);

static int slim_tx_mixer_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tavil_priv *tavil_p = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tavil_p->tx_port_value;
	return 0;
}

static int slim_tx_mixer_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tavil_priv *tavil_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct snd_soc_dapm_update *update = NULL;
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 port_id = mixer->shift;
	u32 enable = ucontrol->value.integer.value[0];
	u32 vtable;

	dev_dbg(codec->dev, "%s: wname %s cname %s value %u shift %d item %ld\n",
		  __func__,
		widget->name, ucontrol->id.name, tavil_p->tx_port_value,
		widget->shift, ucontrol->value.integer.value[0]);

	mutex_lock(&tavil_p->codec_mutex);
	if (dai_id >= ARRAY_SIZE(vport_slim_check_table)) {
		dev_err(codec->dev, "%s: dai_id: %d, out of bounds\n",
			__func__, dai_id);
		mutex_unlock(&tavil_p->codec_mutex);
		return -EINVAL;
	}
	vtable = vport_slim_check_table[dai_id];

	switch (dai_id) {
	case AIF1_CAP:
	case AIF2_CAP:
	case AIF3_CAP:
		/* only add to the list if value not set */
		if (enable && !(tavil_p->tx_port_value & 1 << port_id)) {
			if (wcd9xxx_tx_vport_validation(vtable, port_id,
			    tavil_p->dai, NUM_CODEC_DAIS)) {
				dev_dbg(codec->dev, "%s: TX%u is used by other virtual port\n",
					__func__, port_id);
				mutex_unlock(&tavil_p->codec_mutex);
				return 0;
			}
			tavil_p->tx_port_value |= 1 << port_id;
			list_add_tail(&core->tx_chs[port_id].list,
				      &tavil_p->dai[dai_id].wcd9xxx_ch_list);
		} else if (!enable && (tavil_p->tx_port_value &
			   1 << port_id)) {
			tavil_p->tx_port_value &= ~(1 << port_id);
			list_del_init(&core->tx_chs[port_id].list);
		} else {
			if (enable)
				dev_dbg(codec->dev, "%s: TX%u port is used by\n"
					"this virtual port\n",
					__func__, port_id);
			else
				dev_dbg(codec->dev, "%s: TX%u port is not used by\n"
					"this virtual port\n",
					__func__, port_id);
			/* avoid update power function */
			mutex_unlock(&tavil_p->codec_mutex);
			return 0;
		}
		break;
	default:
		dev_err(codec->dev, "Unknown AIF %d\n", dai_id);
		mutex_unlock(&tavil_p->codec_mutex);
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: name %s sname %s updated value %u shift %d\n",
		__func__, widget->name, widget->sname, tavil_p->tx_port_value,
		widget->shift);

	mutex_unlock(&tavil_p->codec_mutex);
	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, enable, update);

	return 0;
}

static int slim_rx_mux_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tavil_priv *tavil_p = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = tavil_p->rx_port_value;
	return 0;
}

static int slim_rx_mux_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tavil_priv *tavil_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_dapm_update *update = NULL;
	u32 port_id = widget->shift;

	mutex_lock(&tavil_p->codec_mutex);
	tavil_p->rx_port_value = ucontrol->value.enumerated.item[0];
	dev_dbg(codec->dev, "%s: wname %s cname %s value %u shift %d item %ld\n",
		__func__, widget->name, ucontrol->id.name,
		tavil_p->rx_port_value,	widget->shift,
		ucontrol->value.integer.value[0]);

	/* value need to match the Virtual port and AIF number */
	switch (tavil_p->rx_port_value) {
	case 0:
		list_del_init(&core->rx_chs[port_id].list);
		break;
	case 1:
		if (wcd9xxx_rx_vport_validation(port_id +
			WCD934X_RX_PORT_START_NUMBER,
			&tavil_p->dai[AIF1_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tavil_p->dai[AIF1_PB].wcd9xxx_ch_list);
		break;
	case 2:
		if (wcd9xxx_rx_vport_validation(port_id +
			WCD934X_RX_PORT_START_NUMBER,
			&tavil_p->dai[AIF2_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tavil_p->dai[AIF2_PB].wcd9xxx_ch_list);
		break;
	case 3:
		if (wcd9xxx_rx_vport_validation(port_id +
			WCD934X_RX_PORT_START_NUMBER,
			&tavil_p->dai[AIF3_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tavil_p->dai[AIF3_PB].wcd9xxx_ch_list);
		break;
	case 4:
		if (wcd9xxx_rx_vport_validation(port_id +
			WCD934X_RX_PORT_START_NUMBER,
			&tavil_p->dai[AIF4_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tavil_p->dai[AIF4_PB].wcd9xxx_ch_list);
		break;
	default:
		dev_err(codec->dev, "Unknown AIF %d\n", tavil_p->rx_port_value);
		goto err;
	}
rtn:
	mutex_unlock(&tavil_p->codec_mutex);
	snd_soc_dapm_mux_update_power(widget->dapm, kcontrol,
				      tavil_p->rx_port_value, e, update);

	return 0;
err:
	mutex_unlock(&tavil_p->codec_mutex);
	return -EINVAL;
}

static void tavil_codec_enable_slim_port_intr(
					struct wcd9xxx_codec_dai_data *dai,
					struct snd_soc_codec *codec)
{
	struct wcd9xxx_ch *ch;
	int port_num = 0;
	unsigned short reg = 0;
	u8 val = 0;
	struct tavil_priv *tavil_p;

	if (!dai || !codec) {
		pr_err("%s: Invalid params\n", __func__);
		return;
	}

	tavil_p = snd_soc_codec_get_drvdata(codec);
	list_for_each_entry(ch, &dai->wcd9xxx_ch_list, list) {
		if (ch->port >= WCD934X_RX_PORT_START_NUMBER) {
			port_num = ch->port - WCD934X_RX_PORT_START_NUMBER;
			reg = WCD934X_SLIM_PGD_PORT_INT_RX_EN0 + (port_num / 8);
			val = wcd9xxx_interface_reg_read(tavil_p->wcd9xxx,
				reg);
			if (!(val & BYTE_BIT_MASK(port_num))) {
				val |= BYTE_BIT_MASK(port_num);
				wcd9xxx_interface_reg_write(
					tavil_p->wcd9xxx, reg, val);
				val = wcd9xxx_interface_reg_read(
					tavil_p->wcd9xxx, reg);
			}
		} else {
			port_num = ch->port;
			reg = WCD934X_SLIM_PGD_PORT_INT_TX_EN0 + (port_num / 8);
			val = wcd9xxx_interface_reg_read(tavil_p->wcd9xxx,
				reg);
			if (!(val & BYTE_BIT_MASK(port_num))) {
				val |= BYTE_BIT_MASK(port_num);
				wcd9xxx_interface_reg_write(tavil_p->wcd9xxx,
					reg, val);
				val = wcd9xxx_interface_reg_read(
					tavil_p->wcd9xxx, reg);
			}
		}
	}
}

static int tavil_codec_enable_slim_chmask(struct wcd9xxx_codec_dai_data *dai,
					  bool up)
{
	int ret = 0;
	struct wcd9xxx_ch *ch;

	if (up) {
		list_for_each_entry(ch, &dai->wcd9xxx_ch_list, list) {
			ret = wcd9xxx_get_slave_port(ch->ch_num);
			if (ret < 0) {
				pr_err("%s: Invalid slave port ID: %d\n",
				       __func__, ret);
				ret = -EINVAL;
			} else {
				set_bit(ret, &dai->ch_mask);
			}
		}
	} else {
		ret = wait_event_timeout(dai->dai_wait, (dai->ch_mask == 0),
					 msecs_to_jiffies(
						WCD934X_SLIM_CLOSE_TIMEOUT));
		if (!ret) {
			pr_err("%s: Slim close tx/rx wait timeout, ch_mask:0x%lx\n",
				__func__, dai->ch_mask);
			ret = -ETIMEDOUT;
		} else {
			ret = 0;
		}
	}
	return ret;
}

static int tavil_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct wcd9xxx *core;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil_p = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	struct wcd9xxx_codec_dai_data *dai;

	core = dev_get_drvdata(codec->dev->parent);

	dev_dbg(codec->dev, "%s: event called! codec name %s num_dai %d\n"
		"stream name %s event %d\n",
		__func__, codec->component.name,
		codec->component.num_dai, w->sname, event);

	dai = &tavil_p->dai[w->shift];
	dev_dbg(codec->dev, "%s: w->name %s w->shift %d event %d\n",
		 __func__, w->name, w->shift, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dai->bus_down_in_recovery = false;
		tavil_codec_enable_slim_port_intr(dai, codec);
		(void) tavil_codec_enable_slim_chmask(dai, true);
		ret = wcd9xxx_cfg_slim_sch_rx(core, &dai->wcd9xxx_ch_list,
					      dai->rate, dai->bit_width,
					      &dai->grph);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wcd9xxx_disconnect_port(core, &dai->wcd9xxx_ch_list,
					      dai->grph);
		dev_dbg(codec->dev, "%s: Disconnect RX port, ret = %d\n",
			__func__, ret);

		if (!dai->bus_down_in_recovery)
			ret = tavil_codec_enable_slim_chmask(dai, false);
		else
			dev_dbg(codec->dev,
				"%s: bus in recovery skip enable slim_chmask",
				__func__);
		ret = wcd9xxx_close_slim_sch_rx(core, &dai->wcd9xxx_ch_list,
						dai->grph);
		break;
	}
	return ret;
}

static int tavil_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_codec_dai_data *dai;
	struct wcd9xxx *core;
	int ret = 0;

	dev_dbg(codec->dev,
		"%s: w->name %s, w->shift = %d, num_dai %d stream name %s\n",
		__func__, w->name, w->shift,
		codec->component.num_dai, w->sname);

	dai = &tavil_p->dai[w->shift];
	core = dev_get_drvdata(codec->dev->parent);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dai->bus_down_in_recovery = false;
		tavil_codec_enable_slim_port_intr(dai, codec);
		(void) tavil_codec_enable_slim_chmask(dai, true);
		ret = wcd9xxx_cfg_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
					      dai->rate, dai->bit_width,
					      &dai->grph);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wcd9xxx_close_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
						dai->grph);
		if (!dai->bus_down_in_recovery)
			ret = tavil_codec_enable_slim_chmask(dai, false);
		if (ret < 0) {
			ret = wcd9xxx_disconnect_port(core,
						      &dai->wcd9xxx_ch_list,
						      dai->grph);
			dev_dbg(codec->dev, "%s: Disconnect RX port, ret = %d\n",
				 __func__, ret);
		}
		break;
	}
	return ret;
}

static int tavil_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tavil->rx_bias_count++;
		if (tavil->rx_bias_count == 1) {
			snd_soc_update_bits(codec, WCD934X_ANA_RX_SUPPLIES,
					    0x01, 0x01);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		tavil->rx_bias_count--;
		if (!tavil->rx_bias_count)
			snd_soc_update_bits(codec, WCD934X_ANA_RX_SUPPLIES,
					    0x01, 0x00);
		break;
	};
	dev_dbg(codec->dev, "%s: Current RX BIAS user count: %d\n", __func__,
		tavil->rx_bias_count);

	return 0;
}

static int tavil_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/*
		 * 5ms sleep is required after PA is enabled as per
		 * HW requirement
		 */
		usleep_range(5000, 5500);
		snd_soc_update_bits(codec, WCD934X_CDC_RX0_RX_PATH_CTL,
				    0x10, 0x00);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_read(codec, WCD934X_CDC_RX0_RX_PATH_MIX_CTL)) &
		     0x10)
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX0_RX_PATH_MIX_CTL,
					    0x10, 0x00);
		break;
	default:
		break;
	};

	return 0;
}

static int tavil_codec_enable_hphr_pa(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		set_bit(HPH_PA_DELAY, &tavil->status_mask);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/*
		 * 7ms sleep is required after PA is enabled as per
		 * HW requirement
		 */
		if (test_bit(HPH_PA_DELAY, &tavil->status_mask)) {
			usleep_range(7000, 7100);
			clear_bit(HPH_PA_DELAY, &tavil->status_mask);
		}
		/* Remove mute */
		snd_soc_update_bits(codec, WCD934X_CDC_RX2_RX_PATH_CTL,
				    0x10, 0x00);
		/* Enable AutoChop timer at the end of power up */
		snd_soc_update_bits(codec, WCD934X_HPH_NEW_INT_HPH_TIMER1,
				    0x02, 0x02);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_read(codec, WCD934X_CDC_RX2_RX_PATH_MIX_CTL)) &
				  0x10)
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX2_RX_PATH_MIX_CTL,
					    0x10, 0x00);
		break;
	default:
		break;
	};

	return 0;
}

static int tavil_codec_enable_hphl_pa(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		set_bit(HPH_PA_DELAY, &tavil->status_mask);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/*
		 * 7ms sleep is required after PA is enabled as per
		 * HW requirement
		 */
		if (test_bit(HPH_PA_DELAY, &tavil->status_mask)) {
			usleep_range(7000, 7100);
			clear_bit(HPH_PA_DELAY, &tavil->status_mask);
		}
		/* Remove Mute on primary path */
		snd_soc_update_bits(codec, WCD934X_CDC_RX1_RX_PATH_CTL,
				    0x10, 0x00);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_read(codec, WCD934X_CDC_RX1_RX_PATH_MIX_CTL)) &
				  0x10)
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX1_RX_PATH_MIX_CTL,
					    0x10, 0x00);
		break;
	default:
		break;
	};

	return 0;
}

static int tavil_codec_enable_lineout_pa(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 lineout_vol_reg = 0, lineout_mix_vol_reg = 0;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	if (w->reg == WCD934X_ANA_LO_1_2) {
		if (w->shift == 7) {
			lineout_vol_reg = WCD934X_CDC_RX3_RX_PATH_CTL;
			lineout_mix_vol_reg = WCD934X_CDC_RX3_RX_PATH_MIX_CTL;
		} else if (w->shift == 6) {
			lineout_vol_reg = WCD934X_CDC_RX4_RX_PATH_CTL;
			lineout_mix_vol_reg = WCD934X_CDC_RX4_RX_PATH_MIX_CTL;
		}
	} else {
		dev_err(codec->dev, "%s: Error enabling lineout PA\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/*
		 * 5ms sleep is required after PA is enabled as per
		 * HW requirement
		 */
		usleep_range(5000, 5500);
		snd_soc_update_bits(codec, lineout_vol_reg,
				    0x10, 0x00);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_read(codec, lineout_mix_vol_reg)) & 0x10)
			snd_soc_update_bits(codec,
					    lineout_mix_vol_reg,
					    0x10, 0x00);
		break;
	default:
		break;
	};

	return 0;
}

static int tavil_codec_ear_dac_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Disable AutoChop timer during power up */
		snd_soc_update_bits(codec, WCD934X_HPH_NEW_INT_HPH_TIMER1,
				    0x02, 0x00);
		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_EAR,
			     CLS_H_NORMAL);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_EAR,
			     CLS_H_NORMAL);
		break;
	default:
		break;
	};

	return 0;
}

static int tavil_codec_hphr_dac_event(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int hph_mode = tavil->hph_mode;
	u8 dem_inp;

	dev_dbg(codec->dev, "%s wname: %s event: %d hph_mode: %d\n", __func__,
		w->name, event, hph_mode);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Read DEM INP Select */
		dem_inp = snd_soc_read(codec, WCD934X_CDC_RX2_RX_PATH_SEC0) &
			  0x03;
		if (((hph_mode == CLS_H_HIFI) || (hph_mode == CLS_H_LOHIFI) ||
		     (hph_mode == CLS_H_LP)) && (dem_inp != 0x01)) {
			dev_err(codec->dev, "%s: DEM Input not set correctly, hph_mode: %d\n",
					__func__, hph_mode);
			return -EINVAL;
		}
		/* Disable AutoChop timer during power up */
		snd_soc_update_bits(codec, WCD934X_HPH_NEW_INT_HPH_TIMER1,
					0x02, 0x00);
		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_HPHR,
			     ((hph_mode == CLS_H_LOHIFI) ?
			       CLS_H_HIFI : hph_mode));
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 1000us required as per HW requirement */
		usleep_range(1000, 1100);
		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_HPHR,
			     ((hph_mode == CLS_H_LOHIFI) ?
			       CLS_H_HIFI : hph_mode));
		break;
	default:
		break;
	};

	return 0;
}

static int tavil_codec_hphl_dac_event(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int hph_mode = tavil->hph_mode;
	u8 dem_inp;
	int ret = 0;

	dev_dbg(codec->dev, "%s wname: %s event: %d hph_mode: %d\n", __func__,
		w->name, event, hph_mode);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Read DEM INP Select */
		dem_inp = snd_soc_read(codec, WCD934X_CDC_RX1_RX_PATH_SEC0) &
			  0x03;
		if (((hph_mode == CLS_H_HIFI) || (hph_mode == CLS_H_LOHIFI) ||
		     (hph_mode == CLS_H_LP)) && (dem_inp != 0x01)) {
			dev_err(codec->dev, "%s: DEM Input not set correctly, hph_mode: %d\n",
					__func__, hph_mode);
			return -EINVAL;
		}
		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_HPHL,
			     ((hph_mode == CLS_H_LOHIFI) ?
			       CLS_H_HIFI : hph_mode));
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 1000us required as per HW requirement */
		usleep_range(1000, 1100);
		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_HPHL,
			     ((hph_mode == CLS_H_LOHIFI) ?
			       CLS_H_HIFI : hph_mode));
		break;
	default:
		break;
	};

	return ret;
}

static int tavil_codec_lineout_dac_event(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_LO,
			     CLS_AB);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_LO,
			     CLS_AB);
		break;
	}

	return 0;
}

static int tavil_codec_spk_boost_event(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 boost_path_ctl, boost_path_cfg1;
	u16 reg, reg_mix;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	if (!strcmp(w->name, "RX INT7 CHAIN")) {
		boost_path_ctl = WCD934X_CDC_BOOST0_BOOST_PATH_CTL;
		boost_path_cfg1 = WCD934X_CDC_RX7_RX_PATH_CFG1;
		reg = WCD934X_CDC_RX7_RX_PATH_CTL;
		reg_mix = WCD934X_CDC_RX7_RX_PATH_MIX_CTL;
	} else if (!strcmp(w->name, "RX INT8 CHAIN")) {
		boost_path_ctl = WCD934X_CDC_BOOST1_BOOST_PATH_CTL;
		boost_path_cfg1 = WCD934X_CDC_RX8_RX_PATH_CFG1;
		reg = WCD934X_CDC_RX8_RX_PATH_CTL;
		reg_mix = WCD934X_CDC_RX8_RX_PATH_MIX_CTL;
	} else {
		dev_err(codec->dev, "%s: unknown widget: %s\n",
			__func__, w->name);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, boost_path_cfg1, 0x01, 0x01);
		snd_soc_update_bits(codec, boost_path_ctl, 0x10, 0x10);
		snd_soc_update_bits(codec, reg, 0x10, 0x00);
		if ((snd_soc_read(codec, reg_mix)) & 0x10)
			snd_soc_update_bits(codec, reg_mix, 0x10, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, boost_path_ctl, 0x10, 0x00);
		snd_soc_update_bits(codec, boost_path_cfg1, 0x01, 0x00);
		break;
	};

	return 0;
}

static int __tavil_codec_enable_swr(struct snd_soc_dapm_widget *w, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil;
	int ch_cnt;

	tavil = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if ((strnstr(w->name, "INT7_", sizeof("RX INT7_"))) &&
		    !tavil->swr.rx_7_count)
			tavil->swr.rx_7_count++;
		if ((strnstr(w->name, "INT8_", sizeof("RX INT8_"))) &&
		    !tavil->swr.rx_8_count)
			tavil->swr.rx_8_count++;
		ch_cnt = tavil->swr.rx_7_count + tavil->swr.rx_8_count;

		swrm_wcd_notify(tavil->swr.ctrl_data[0].swr_pdev,
				SWR_DEVICE_UP, NULL);
		swrm_wcd_notify(tavil->swr.ctrl_data[0].swr_pdev,
				SWR_SET_NUM_RX_CH, &ch_cnt);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if ((strnstr(w->name, "INT7_", sizeof("RX INT7_"))) &&
		    tavil->swr.rx_7_count)
			tavil->swr.rx_7_count--;
		if ((strnstr(w->name, "INT8_", sizeof("RX INT8_"))) &&
		    tavil->swr.rx_8_count)
			tavil->swr.rx_8_count--;
		ch_cnt = tavil->swr.rx_7_count + tavil->swr.rx_8_count;

		swrm_wcd_notify(tavil->swr.ctrl_data[0].swr_pdev,
				SWR_SET_NUM_RX_CH, &ch_cnt);

		break;
	}
	dev_dbg(tavil->dev, "%s: current swr ch cnt: %d\n",
		__func__, tavil->swr.rx_7_count + tavil->swr.rx_8_count);

	return 0;
}

static int tavil_codec_enable_swr(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	return __tavil_codec_enable_swr(w, event);
}

static u16 tavil_interp_get_primary_reg(u16 reg, u16 *ind)
{
	u16 prim_int_reg = 0;

	switch (reg) {
	case WCD934X_CDC_RX0_RX_PATH_CTL:
	case WCD934X_CDC_RX0_RX_PATH_MIX_CTL:
		prim_int_reg = WCD934X_CDC_RX0_RX_PATH_CTL;
		*ind = 0;
		break;
	case WCD934X_CDC_RX1_RX_PATH_CTL:
	case WCD934X_CDC_RX1_RX_PATH_MIX_CTL:
		prim_int_reg = WCD934X_CDC_RX1_RX_PATH_CTL;
		*ind = 1;
		break;
	case WCD934X_CDC_RX2_RX_PATH_CTL:
	case WCD934X_CDC_RX2_RX_PATH_MIX_CTL:
		prim_int_reg = WCD934X_CDC_RX2_RX_PATH_CTL;
		*ind = 2;
		break;
	case WCD934X_CDC_RX3_RX_PATH_CTL:
	case WCD934X_CDC_RX3_RX_PATH_MIX_CTL:
		prim_int_reg = WCD934X_CDC_RX3_RX_PATH_CTL;
		*ind = 3;
		break;
	case WCD934X_CDC_RX4_RX_PATH_CTL:
	case WCD934X_CDC_RX4_RX_PATH_MIX_CTL:
		prim_int_reg = WCD934X_CDC_RX4_RX_PATH_CTL;
		*ind = 4;
		break;
	case WCD934X_CDC_RX7_RX_PATH_CTL:
	case WCD934X_CDC_RX7_RX_PATH_MIX_CTL:
		prim_int_reg = WCD934X_CDC_RX7_RX_PATH_CTL;
		*ind = 7;
		break;
	case WCD934X_CDC_RX8_RX_PATH_CTL:
	case WCD934X_CDC_RX8_RX_PATH_MIX_CTL:
		prim_int_reg = WCD934X_CDC_RX8_RX_PATH_CTL;
		*ind = 8;
		break;
	};

	return prim_int_reg;
}

static void tavil_codec_hd2_control(struct snd_soc_codec *codec,
				    u16 prim_int_reg, int event)
{
	u16 hd2_scale_reg;
	u16 hd2_enable_reg = 0;

	if (prim_int_reg == WCD934X_CDC_RX1_RX_PATH_CTL) {
		hd2_scale_reg = WCD934X_CDC_RX1_RX_PATH_SEC3;
		hd2_enable_reg = WCD934X_CDC_RX1_RX_PATH_CFG0;
	}
	if (prim_int_reg == WCD934X_CDC_RX2_RX_PATH_CTL) {
		hd2_scale_reg = WCD934X_CDC_RX2_RX_PATH_SEC3;
		hd2_enable_reg = WCD934X_CDC_RX2_RX_PATH_CFG0;
	}

	if (hd2_enable_reg && SND_SOC_DAPM_EVENT_ON(event)) {
		snd_soc_update_bits(codec, hd2_scale_reg, 0x3C, 0x10);
		snd_soc_update_bits(codec, hd2_scale_reg, 0x03, 0x01);
		snd_soc_update_bits(codec, hd2_enable_reg, 0x04, 0x04);
	}

	if (hd2_enable_reg && SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, hd2_enable_reg, 0x04, 0x00);
		snd_soc_update_bits(codec, hd2_scale_reg, 0x03, 0x00);
		snd_soc_update_bits(codec, hd2_scale_reg, 0x3C, 0x00);
	}
}

static int tavil_codec_enable_prim_interpolator(struct snd_soc_codec *codec,
						u16 reg, int event)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	u16 prim_int_reg;
	u16 ind = 0;

	prim_int_reg = tavil_interp_get_primary_reg(reg, &ind);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tavil->prim_int_users[ind]++;
		if (tavil->prim_int_users[ind] == 1) {
			/* PGA Mute enable */
			snd_soc_update_bits(codec, prim_int_reg,
					    0x10, 0x10);
			tavil_codec_hd2_control(codec, prim_int_reg, event);
			/* RX path CLK enable */
			snd_soc_update_bits(codec, prim_int_reg,
					    1 << 0x5, 1 << 0x5);
		}
		if ((reg != prim_int_reg) &&
		    ((snd_soc_read(codec, prim_int_reg)) & 0x10))
			snd_soc_update_bits(codec, reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tavil->prim_int_users[ind]--;
		if (tavil->prim_int_users[ind] == 0) {
			snd_soc_update_bits(codec, prim_int_reg,
					    1 << 0x5, 0 << 0x5);
			snd_soc_update_bits(codec, prim_int_reg,
					    0x40, 0x40);
			snd_soc_update_bits(codec, prim_int_reg,
					    0x40, 0x00);
			tavil_codec_hd2_control(codec, prim_int_reg, event);
		}
		break;
	};

	dev_dbg(codec->dev, "%s: primary interpolator: INT%d, users: %d\n",
		__func__, ind, tavil->prim_int_users[ind]);
	return 0;
}

static int tavil_codec_enable_mix_path(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	u16 gain_reg;
	int offset_val = 0;
	int val = 0;

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	switch (w->reg) {
	case WCD934X_CDC_RX0_RX_PATH_MIX_CTL:
		gain_reg = WCD934X_CDC_RX0_RX_VOL_MIX_CTL;
		break;
	case WCD934X_CDC_RX1_RX_PATH_MIX_CTL:
		gain_reg = WCD934X_CDC_RX1_RX_VOL_MIX_CTL;
		break;
	case WCD934X_CDC_RX2_RX_PATH_MIX_CTL:
		gain_reg = WCD934X_CDC_RX2_RX_VOL_MIX_CTL;
		break;
	case WCD934X_CDC_RX3_RX_PATH_MIX_CTL:
		gain_reg = WCD934X_CDC_RX3_RX_VOL_MIX_CTL;
		break;
	case WCD934X_CDC_RX4_RX_PATH_MIX_CTL:
		gain_reg = WCD934X_CDC_RX4_RX_VOL_MIX_CTL;
		break;
	case WCD934X_CDC_RX7_RX_PATH_MIX_CTL:
		__tavil_codec_enable_swr(w, event);
		gain_reg = WCD934X_CDC_RX7_RX_VOL_MIX_CTL;
		break;
	case WCD934X_CDC_RX8_RX_PATH_MIX_CTL:
		__tavil_codec_enable_swr(w, event);
		gain_reg = WCD934X_CDC_RX8_RX_VOL_MIX_CTL;
		break;
	default:
		dev_err(codec->dev, "%s: No gain register avail for %s\n",
			__func__, w->name);
		return 0;
	};

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if ((tavil->swr.spkr_gain_offset ==
					WCD934X_RX_GAIN_OFFSET_M1P5_DB) &&
		    (tavil->comp_enabled[COMPANDER_7] ||
		     tavil->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD934X_CDC_RX7_RX_VOL_MIX_CTL ||
		     gain_reg == WCD934X_CDC_RX8_RX_VOL_MIX_CTL)) {
			snd_soc_update_bits(codec, WCD934X_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			snd_soc_update_bits(codec, WCD934X_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			offset_val = -2;
		}
		val = snd_soc_read(codec, gain_reg);
		val += offset_val;
		snd_soc_write(codec, gain_reg, val);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if ((tavil->swr.spkr_gain_offset ==
					WCD934X_RX_GAIN_OFFSET_M1P5_DB) &&
		    (tavil->comp_enabled[COMPANDER_7] ||
		     tavil->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD934X_CDC_RX7_RX_VOL_MIX_CTL ||
		     gain_reg == WCD934X_CDC_RX8_RX_VOL_MIX_CTL)) {
			snd_soc_update_bits(codec, WCD934X_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			snd_soc_update_bits(codec, WCD934X_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			offset_val = 2;
			val = snd_soc_read(codec, gain_reg);
			val += offset_val;
			snd_soc_write(codec, gain_reg, val);
		}
		break;
	};

	return 0;
}

static int tavil_config_compander(struct snd_soc_codec *codec, int interp_n,
				  int event)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int comp;
	u16 comp_ctl0_reg, rx_path_cfg0_reg;

	/* EAR does not have compander */
	if (!interp_n)
		return 0;

	comp = interp_n - 1;
	dev_dbg(codec->dev, "%s: event %d compander %d, enabled %d\n",
		__func__, event, comp + 1, tavil->comp_enabled[comp]);

	if (!tavil->comp_enabled[comp])
		return 0;

	comp_ctl0_reg = WCD934X_CDC_COMPANDER1_CTL0 + (comp * 8);
	rx_path_cfg0_reg = WCD934X_CDC_RX1_RX_PATH_CFG0 + (comp * 20);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Enable Compander Clock */
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x01, 0x01);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x02);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, rx_path_cfg0_reg, 0x02, 0x02);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x04, 0x04);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x01, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x04, 0x00);
	}

	return 0;
}

static int tavil_codec_enable_interpolator(struct snd_soc_dapm_widget *w,
					   struct snd_kcontrol *kcontrol,
					   int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	u16 gain_reg;
	u16 reg;
	int val;
	int offset_val = 0;

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	if (!(strcmp(w->name, "RX INT0 INTERP"))) {
		reg = WCD934X_CDC_RX0_RX_PATH_CTL;
		gain_reg = WCD934X_CDC_RX0_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT1 INTERP"))) {
		reg = WCD934X_CDC_RX1_RX_PATH_CTL;
		gain_reg = WCD934X_CDC_RX1_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT2 INTERP"))) {
		reg = WCD934X_CDC_RX2_RX_PATH_CTL;
		gain_reg = WCD934X_CDC_RX2_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT3 INTERP"))) {
		reg = WCD934X_CDC_RX3_RX_PATH_CTL;
		gain_reg = WCD934X_CDC_RX3_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT4 INTERP"))) {
		reg = WCD934X_CDC_RX4_RX_PATH_CTL;
		gain_reg = WCD934X_CDC_RX4_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT7 INTERP"))) {
		reg = WCD934X_CDC_RX7_RX_PATH_CTL;
		gain_reg = WCD934X_CDC_RX7_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT8 INTERP"))) {
		reg = WCD934X_CDC_RX8_RX_PATH_CTL;
		gain_reg = WCD934X_CDC_RX8_RX_VOL_CTL;
	} else {
		dev_err(codec->dev, "%s: Interpolator reg not found\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Reset if needed */
		tavil_codec_enable_prim_interpolator(codec, reg, event);
		break;
	case SND_SOC_DAPM_POST_PMU:
		tavil_config_compander(codec, w->shift, event);
		/* apply gain after int clk is enabled */
		if ((tavil->swr.spkr_gain_offset ==
					WCD934X_RX_GAIN_OFFSET_M1P5_DB) &&
		    (tavil->comp_enabled[COMPANDER_7] ||
		     tavil->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD934X_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD934X_CDC_RX8_RX_VOL_CTL)) {
			snd_soc_update_bits(codec, WCD934X_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			snd_soc_update_bits(codec, WCD934X_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			offset_val = -2;
		}
		val = snd_soc_read(codec, gain_reg);
		val += offset_val;
		snd_soc_write(codec, gain_reg, val);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tavil_config_compander(codec, w->shift, event);
		tavil_codec_enable_prim_interpolator(codec, reg, event);
		if ((tavil->swr.spkr_gain_offset ==
					WCD934X_RX_GAIN_OFFSET_M1P5_DB) &&
		    (tavil->comp_enabled[COMPANDER_7] ||
		     tavil->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD934X_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD934X_CDC_RX8_RX_VOL_CTL)) {
			snd_soc_update_bits(codec, WCD934X_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			snd_soc_update_bits(codec, WCD934X_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			offset_val = 2;
			val = snd_soc_read(codec, gain_reg);
			val += offset_val;
			snd_soc_write(codec, gain_reg, val);
		}
		break;
	};

	return 0;
}

static int tavil_codec_set_iir_gain(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU: /* fall through */
	case SND_SOC_DAPM_PRE_PMD:
		if (strnstr(w->name, "IIR0", sizeof("IIR0"))) {
			snd_soc_write(codec,
				WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL,
			snd_soc_read(codec,
				WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL));
			snd_soc_write(codec,
				WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL,
			snd_soc_read(codec,
				WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL));
			snd_soc_write(codec,
				WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL,
			snd_soc_read(codec,
				WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL));
			snd_soc_write(codec,
				WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL,
			snd_soc_read(codec,
				WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL));
		} else {
			snd_soc_write(codec,
				WCD934X_CDC_SIDETONE_IIR1_IIR_GAIN_B1_CTL,
			snd_soc_read(codec,
				WCD934X_CDC_SIDETONE_IIR1_IIR_GAIN_B1_CTL));
			snd_soc_write(codec,
				WCD934X_CDC_SIDETONE_IIR1_IIR_GAIN_B2_CTL,
			snd_soc_read(codec,
				WCD934X_CDC_SIDETONE_IIR1_IIR_GAIN_B2_CTL));
			snd_soc_write(codec,
				WCD934X_CDC_SIDETONE_IIR1_IIR_GAIN_B3_CTL,
			snd_soc_read(codec,
				WCD934X_CDC_SIDETONE_IIR1_IIR_GAIN_B3_CTL));
		}
		break;
	}
	return 0;
}

static int tavil_codec_find_amic_input(struct snd_soc_codec *codec,
				       int adc_mux_n)
{
	u16 mask, shift, adc_mux_in_reg;
	u16 amic_mux_sel_reg;
	bool is_amic;

	if (adc_mux_n < 0 || adc_mux_n > WCD934X_MAX_VALID_ADC_MUX ||
	    adc_mux_n == WCD934X_INVALID_ADC_MUX)
		return 0;

	if (adc_mux_n < 3) {
		adc_mux_in_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG1 +
				 adc_mux_n;
		mask = 0x03;
		shift = 0;
		amic_mux_sel_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
				   2 * adc_mux_n;
	} else if (adc_mux_n < 4) {
		adc_mux_in_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX3_CFG1;
		mask = 0x03;
		shift = 0;
		amic_mux_sel_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
				   2 * adc_mux_n;
	} else if (adc_mux_n < 7) {
		adc_mux_in_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG1 +
				 (adc_mux_n - 4);
		mask = 0x0C;
		shift = 2;
		amic_mux_sel_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX4_CFG0 +
				   adc_mux_n - 4;
	} else if (adc_mux_n < 8) {
		adc_mux_in_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX3_CFG1;
		mask = 0x0C;
		shift = 2;
		amic_mux_sel_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX4_CFG0 +
				   adc_mux_n - 4;
	} else if (adc_mux_n < 12) {
		adc_mux_in_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG1 +
				 ((adc_mux_n == 8) ? (adc_mux_n - 8) :
				  (adc_mux_n - 9));
		mask = 0x30;
		shift = 4;
		amic_mux_sel_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX4_CFG0 +
				   adc_mux_n - 4;
	} else if (adc_mux_n < 13) {
		adc_mux_in_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX3_CFG1;
		mask = 0x30;
		shift = 4;
		amic_mux_sel_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX4_CFG0 +
				   adc_mux_n - 4;
	} else {
		adc_mux_in_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG1;
		mask = 0xC0;
		shift = 6;
		amic_mux_sel_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX4_CFG0 +
				   adc_mux_n - 4;
	}

	is_amic = (((snd_soc_read(codec, adc_mux_in_reg) & mask) >> shift)
		    == 1);
	if (!is_amic)
		return 0;

	return snd_soc_read(codec, amic_mux_sel_reg) & 0x07;
}

static void tavil_codec_set_tx_hold(struct snd_soc_codec *codec,
				    u16 amic_reg, bool set)
{
	u8 mask = 0x20;
	u8 val;

	if (amic_reg == WCD934X_ANA_AMIC1 ||
	    amic_reg == WCD934X_ANA_AMIC3)
		mask = 0x40;

	val = set ? mask : 0x00;

	switch (amic_reg) {
	case WCD934X_ANA_AMIC1:
	case WCD934X_ANA_AMIC2:
		snd_soc_update_bits(codec, WCD934X_ANA_AMIC2, mask, val);
		break;
	case WCD934X_ANA_AMIC3:
	case WCD934X_ANA_AMIC4:
		snd_soc_update_bits(codec, WCD934X_ANA_AMIC4, mask, val);
		break;
	default:
		dev_dbg(codec->dev, "%s: invalid amic: %d\n",
			__func__, amic_reg);
		break;
	}
}

static int tavil_codec_tx_adc_cfg(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	int adc_mux_n = w->shift;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int amic_n;
	u16 amic_reg;

	dev_dbg(codec->dev, "%s: event: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		amic_n = tavil_codec_find_amic_input(codec, adc_mux_n);
		if (amic_n) {
			amic_reg = WCD934X_ANA_AMIC1 + amic_n - 1;
			tavil_codec_set_tx_hold(codec, amic_reg, false);
		}
		break;
	default:
		break;
	}

	return 0;
}

static u16 tavil_codec_get_amic_pwlvl_reg(struct snd_soc_codec *codec, int amic)
{
	u16 pwr_level_reg = 0;

	switch (amic) {
	case 1:
	case 2:
		pwr_level_reg = WCD934X_ANA_AMIC1;
		break;

	case 3:
	case 4:
		pwr_level_reg = WCD934X_ANA_AMIC3;
		break;
	default:
		dev_dbg(codec->dev, "%s: invalid amic: %d\n",
			__func__, amic);
		break;
	}

	return pwr_level_reg;
}

#define  TX_HPF_CUT_OFF_FREQ_MASK 0x60
#define  CF_MIN_3DB_4HZ     0x0
#define  CF_MIN_3DB_75HZ    0x1
#define  CF_MIN_3DB_150HZ   0x2

static void tavil_tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct tavil_priv *tavil;
	struct snd_soc_codec *codec;
	u16 dec_cfg_reg, amic_reg;
	u8 hpf_cut_off_freq;
	int amic_n;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	tavil = hpf_work->tavil;
	codec = tavil->codec;
	hpf_cut_off_freq = hpf_work->hpf_cut_off_freq;

	dec_cfg_reg = WCD934X_CDC_TX0_TX_PATH_CFG0 + 16 * hpf_work->decimator;

	dev_dbg(codec->dev, "%s: decimator %u hpf_cut_of_freq 0x%x\n",
		__func__, hpf_work->decimator, hpf_cut_off_freq);

	amic_n = tavil_codec_find_amic_input(codec, hpf_work->decimator);
	if (amic_n) {
		amic_reg = WCD934X_ANA_AMIC1 + amic_n - 1;
		tavil_codec_set_tx_hold(codec, amic_reg, false);
	}
	snd_soc_update_bits(codec, dec_cfg_reg, TX_HPF_CUT_OFF_FREQ_MASK,
			    hpf_cut_off_freq << 5);
}

static void tavil_tx_mute_update_callback(struct work_struct *work)
{
	struct tx_mute_work *tx_mute_dwork;
	struct tavil_priv *tavil;
	struct delayed_work *delayed_work;
	struct snd_soc_codec *codec;
	u16 tx_vol_ctl_reg, hpf_gate_reg;

	delayed_work = to_delayed_work(work);
	tx_mute_dwork = container_of(delayed_work, struct tx_mute_work, dwork);
	tavil = tx_mute_dwork->tavil;
	codec = tavil->codec;

	tx_vol_ctl_reg = WCD934X_CDC_TX0_TX_PATH_CTL +
			 16 * tx_mute_dwork->decimator;
	hpf_gate_reg = WCD934X_CDC_TX0_TX_PATH_SEC2 +
		       16 * tx_mute_dwork->decimator;
	snd_soc_update_bits(codec, hpf_gate_reg, 0x01, 0x01);
	snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x00);
}

static int tavil_codec_enable_dec(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	unsigned int decimator;
	char *dec_adc_mux_name = NULL;
	char *widget_name = NULL;
	char *wname;
	int ret = 0, amic_n;
	u16 tx_vol_ctl_reg, pwr_level_reg = 0, dec_cfg_reg, hpf_gate_reg;
	char *dec;
	u8 hpf_cut_off_freq;

	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name)
		return -ENOMEM;

	wname = widget_name;
	dec_adc_mux_name = strsep(&widget_name, " ");
	if (!dec_adc_mux_name) {
		dev_err(codec->dev, "%s: Invalid decimator = %s\n",
			__func__, w->name);
		ret =  -EINVAL;
		goto out;
	}
	dec_adc_mux_name = widget_name;

	dec = strpbrk(dec_adc_mux_name, "012345678");
	if (!dec) {
		dev_err(codec->dev, "%s: decimator index not found\n",
			__func__);
		ret =  -EINVAL;
		goto out;
	}

	ret = kstrtouint(dec, 10, &decimator);
	if (ret < 0) {
		dev_err(codec->dev, "%s: Invalid decimator = %s\n",
			__func__, wname);
		ret =  -EINVAL;
		goto out;
	}

	dev_dbg(codec->dev, "%s(): widget = %s decimator = %u\n", __func__,
			w->name, decimator);

	tx_vol_ctl_reg = WCD934X_CDC_TX0_TX_PATH_CTL + 16 * decimator;
	hpf_gate_reg = WCD934X_CDC_TX0_TX_PATH_SEC2 + 16 * decimator;
	dec_cfg_reg = WCD934X_CDC_TX0_TX_PATH_CFG0 + 16 * decimator;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		amic_n = tavil_codec_find_amic_input(codec, decimator);
		if (amic_n)
			pwr_level_reg = tavil_codec_get_amic_pwlvl_reg(codec,
								       amic_n);

		if (pwr_level_reg) {
			switch ((snd_soc_read(codec, pwr_level_reg) &
					      WCD934X_AMIC_PWR_LVL_MASK) >>
					      WCD934X_AMIC_PWR_LVL_SHIFT) {
			case WCD934X_AMIC_PWR_LEVEL_LP:
				snd_soc_update_bits(codec, dec_cfg_reg,
						    WCD934X_DEC_PWR_LVL_MASK,
						    WCD934X_DEC_PWR_LVL_LP);
				break;

			case WCD934X_AMIC_PWR_LEVEL_HP:
				snd_soc_update_bits(codec, dec_cfg_reg,
						    WCD934X_DEC_PWR_LVL_MASK,
						    WCD934X_DEC_PWR_LVL_HP);
				break;
			case WCD934X_AMIC_PWR_LEVEL_DEFAULT:
			default:
				snd_soc_update_bits(codec, dec_cfg_reg,
						    WCD934X_DEC_PWR_LVL_MASK,
						    WCD934X_DEC_PWR_LVL_DF);
				break;
			}
		}
		hpf_cut_off_freq = (snd_soc_read(codec, dec_cfg_reg) &
				   TX_HPF_CUT_OFF_FREQ_MASK) >> 5;

		tavil->tx_hpf_work[decimator].hpf_cut_off_freq =
							hpf_cut_off_freq;
		if (hpf_cut_off_freq != CF_MIN_3DB_150HZ)
			snd_soc_update_bits(codec, dec_cfg_reg,
					    TX_HPF_CUT_OFF_FREQ_MASK,
					    CF_MIN_3DB_150HZ << 5);
		/* Enable TX PGA Mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, hpf_gate_reg, 0x01, 0x00);
		/* schedule work queue to Remove Mute */
		schedule_delayed_work(&tavil->tx_mute_dwork[decimator].dwork,
				      msecs_to_jiffies(tx_unmute_delay));
		if (tavil->tx_hpf_work[decimator].hpf_cut_off_freq !=
							CF_MIN_3DB_150HZ)
			schedule_delayed_work(
					&tavil->tx_hpf_work[decimator].dwork,
					msecs_to_jiffies(300));

		break;
	case SND_SOC_DAPM_PRE_PMD:
		hpf_cut_off_freq =
			tavil->tx_hpf_work[decimator].hpf_cut_off_freq;
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x10);
		if (cancel_delayed_work_sync(
		    &tavil->tx_hpf_work[decimator].dwork)) {
			if (hpf_cut_off_freq != CF_MIN_3DB_150HZ)
				snd_soc_update_bits(codec, dec_cfg_reg,
						    TX_HPF_CUT_OFF_FREQ_MASK,
						    hpf_cut_off_freq << 5);
		}
		cancel_delayed_work_sync(
				&tavil->tx_mute_dwork[decimator].dwork);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x00);
		break;
	};
out:
	kfree(wname);
	return ret;
}

static u8 tavil_get_dmic_clk_val(struct snd_soc_codec *codec,
				 u32 mclk_rate, u32 dmic_clk_rate)
{
	u32 div_factor;
	u8 dmic_ctl_val;

	dev_dbg(codec->dev,
		"%s: mclk_rate = %d, dmic_sample_rate = %d\n",
		__func__, mclk_rate, dmic_clk_rate);

	/* Default value to return in case of error */
	if (mclk_rate == WCD934X_MCLK_CLK_9P6MHZ)
		dmic_ctl_val = WCD934X_DMIC_CLK_DIV_2;
	else
		dmic_ctl_val = WCD934X_DMIC_CLK_DIV_3;

	if (dmic_clk_rate == 0) {
		dev_err(codec->dev,
			"%s: dmic_sample_rate cannot be 0\n",
			__func__);
		goto done;
	}

	div_factor = mclk_rate / dmic_clk_rate;
	switch (div_factor) {
	case 2:
		dmic_ctl_val = WCD934X_DMIC_CLK_DIV_2;
		break;
	case 3:
		dmic_ctl_val = WCD934X_DMIC_CLK_DIV_3;
		break;
	case 4:
		dmic_ctl_val = WCD934X_DMIC_CLK_DIV_4;
		break;
	case 6:
		dmic_ctl_val = WCD934X_DMIC_CLK_DIV_6;
		break;
	case 8:
		dmic_ctl_val = WCD934X_DMIC_CLK_DIV_8;
		break;
	case 16:
		dmic_ctl_val = WCD934X_DMIC_CLK_DIV_16;
		break;
	default:
		dev_err(codec->dev,
			"%s: Invalid div_factor %u, clk_rate(%u), dmic_rate(%u)\n",
			__func__, div_factor, mclk_rate, dmic_clk_rate);
		break;
	}

done:
	return dmic_ctl_val;
}

static int tavil_codec_enable_adc(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s: event:%d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tavil_codec_set_tx_hold(codec, w->reg, true);
		break;
	default:
		break;
	}

	return 0;
}

static int tavil_codec_enable_dmic(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_pdata *pdata = dev_get_platdata(codec->dev->parent);
	u8  dmic_clk_en = 0x01;
	u16 dmic_clk_reg;
	s32 *dmic_clk_cnt;
	u8 dmic_rate_val, dmic_rate_shift = 1;
	unsigned int dmic;
	int ret;
	char *wname;

	wname = strpbrk(w->name, "012345");
	if (!wname) {
		dev_err(codec->dev, "%s: widget not found\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(wname, 10, &dmic);
	if (ret < 0) {
		dev_err(codec->dev, "%s: Invalid DMIC line on the codec\n",
			__func__);
		return -EINVAL;
	}

	switch (dmic) {
	case 0:
	case 1:
		dmic_clk_cnt = &(tavil->dmic_0_1_clk_cnt);
		dmic_clk_reg = WCD934X_CPE_SS_DMIC0_CTL;
		break;
	case 2:
	case 3:
		dmic_clk_cnt = &(tavil->dmic_2_3_clk_cnt);
		dmic_clk_reg = WCD934X_CPE_SS_DMIC1_CTL;
		break;
	case 4:
	case 5:
		dmic_clk_cnt = &(tavil->dmic_4_5_clk_cnt);
		dmic_clk_reg = WCD934X_CPE_SS_DMIC2_CTL;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid DMIC Selection\n",
			__func__);
		return -EINVAL;
	};
	dev_dbg(codec->dev, "%s: event %d DMIC%d dmic_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dmic_rate_val =
			tavil_get_dmic_clk_val(codec,
					       pdata->mclk_rate,
					       pdata->dmic_sample_rate);

		(*dmic_clk_cnt)++;
		if (*dmic_clk_cnt == 1) {
			snd_soc_update_bits(codec, dmic_clk_reg,
					    0x07 << dmic_rate_shift,
					    dmic_rate_val << dmic_rate_shift);
			snd_soc_update_bits(codec, dmic_clk_reg,
					    dmic_clk_en, dmic_clk_en);
		}

		break;
	case SND_SOC_DAPM_POST_PMD:
		dmic_rate_val =
			tavil_get_dmic_clk_val(codec,
					       pdata->mclk_rate,
					       pdata->mad_dmic_sample_rate);
		(*dmic_clk_cnt)--;
		if (*dmic_clk_cnt  == 0) {
			snd_soc_update_bits(codec, dmic_clk_reg,
					    dmic_clk_en, 0);
			snd_soc_update_bits(codec, dmic_clk_reg,
					    0x07 << dmic_rate_shift,
					    dmic_rate_val << dmic_rate_shift);
		}
		break;
	};

	return 0;
}

static int tavil_micbias_control(struct snd_soc_codec *codec,
				 int micb_num,
				 int req, bool is_dapm)
{


	u16 micb_reg;

	switch (micb_num) {
	case MIC_BIAS_1:
		micb_reg = WCD934X_ANA_MICB1;
		break;
	case MIC_BIAS_2:
		micb_reg = WCD934X_ANA_MICB2;
		break;
	case MIC_BIAS_3:
		micb_reg = WCD934X_ANA_MICB3;
		break;
	case MIC_BIAS_4:
		micb_reg = WCD934X_ANA_MICB4;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid micbias number: %d\n",
			__func__, micb_num);
		return -EINVAL;
	}

	switch (req) {
	case MICB_ENABLE:
		snd_soc_update_bits(codec, micb_reg, 0xC0, 0x40);
		break;
	case MICB_DISABLE:
		snd_soc_update_bits(codec, micb_reg, 0xC0, 0x80);
	};

	return 0;
}

static int __tavil_codec_enable_micbias(struct snd_soc_dapm_widget *w,
					int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int micb_num;

	dev_dbg(codec->dev, "%s: wname: %s, event: %d\n",
		__func__, w->name, event);

	if (strnstr(w->name, "MIC BIAS1", sizeof("MIC BIAS1")))
		micb_num = MIC_BIAS_1;
	else if (strnstr(w->name, "MIC BIAS2", sizeof("MIC BIAS2")))
		micb_num = MIC_BIAS_2;
	else if (strnstr(w->name, "MIC BIAS3", sizeof("MIC BIAS3")))
		micb_num = MIC_BIAS_3;
	else if (strnstr(w->name, "MIC BIAS4", sizeof("MIC BIAS4")))
		micb_num = MIC_BIAS_4;
	else
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/*
		 * MIC BIAS can also be requested by MBHC,
		 * so use ref count to handle micbias pullup
		 * and enable requests
		 */
		tavil_micbias_control(codec, micb_num, MICB_ENABLE, true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* wait for cnp time */
		usleep_range(1000, 1100);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tavil_micbias_control(codec, micb_num, MICB_DISABLE, true);
		break;
	};

	return 0;
}

static int tavil_codec_force_enable_micbias(struct snd_soc_dapm_widget *w,
					    struct snd_kcontrol *kcontrol,
					    int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd_resmgr_enable_master_bias(tavil->resmgr);
		tavil_cdc_mclk_enable(codec, true);
		ret = __tavil_codec_enable_micbias(w, SND_SOC_DAPM_PRE_PMU);
		/* Wait for 1ms for better cnp */
		usleep_range(1000, 1100);
		tavil_cdc_mclk_enable(codec, false);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = __tavil_codec_enable_micbias(w, SND_SOC_DAPM_POST_PMD);
		wcd_resmgr_disable_master_bias(tavil->resmgr);
		break;
	}

	return ret;
}

static int tavil_codec_enable_micbias(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	return __tavil_codec_enable_micbias(w, event);
}

static int tavil_iir_enable_audio_mixer_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	/* IIR filter band registers are at integer multiples of 16 */
	u16 iir_reg = WCD934X_CDC_SIDETONE_IIR0_IIR_CTL + 16 * iir_idx;

	ucontrol->value.integer.value[0] = (snd_soc_read(codec, iir_reg) &
					    (1 << band_idx)) != 0;

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int tavil_iir_enable_audio_mixer_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	bool iir_band_en_status;
	int value = ucontrol->value.integer.value[0];
	u16 iir_reg = WCD934X_CDC_SIDETONE_IIR0_IIR_CTL + 16 * iir_idx;

	/* Mask first 5 bits, 6-8 are reserved */
	snd_soc_update_bits(codec, iir_reg, (1 << band_idx),
			    (value << band_idx));

	iir_band_en_status = ((snd_soc_read(codec, iir_reg) &
			      (1 << band_idx)) != 0);
	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx, iir_band_en_status);
	return 0;
}

static uint32_t get_iir_band_coeff(struct snd_soc_codec *codec,
				   int iir_idx, int band_idx,
				   int coeff_idx)
{
	uint32_t value = 0;

	/* Address does not automatically update if reading */
	snd_soc_write(codec,
		(WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t)) & 0x7F);

	value |= snd_soc_read(codec,
		(WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx));

	snd_soc_write(codec,
		(WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 1) & 0x7F);

	value |= (snd_soc_read(codec,
			       (WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				16 * iir_idx)) << 8);

	snd_soc_write(codec,
		(WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 2) & 0x7F);

	value |= (snd_soc_read(codec,
			       (WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				16 * iir_idx)) << 16);

	snd_soc_write(codec,
		(WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 3) & 0x7F);

	/* Mask bits top 2 bits since they are reserved */
	value |= ((snd_soc_read(codec,
				(WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				 16 * iir_idx)) & 0x3F) << 24);

	return value;
}

static int tavil_iir_band_audio_mixer_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 0);
	ucontrol->value.integer.value[1] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 1);
	ucontrol->value.integer.value[2] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 2);
	ucontrol->value.integer.value[3] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 3);
	ucontrol->value.integer.value[4] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 4);

	dev_dbg(codec->dev, "%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[1],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[2],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[3],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[4]);
	return 0;
}

static void set_iir_band_coeff(struct snd_soc_codec *codec,
			       int iir_idx, int band_idx,
			       uint32_t value)
{
	snd_soc_write(codec,
		(WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value & 0xFF));

	snd_soc_write(codec,
		(WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_write(codec,
		(WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value >> 16) & 0xFF);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value >> 24) & 0x3F);
}

static int tavil_iir_band_audio_mixer_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	/*
	 * Mask top bit it is reserved
	 * Updates addr automatically for each B2 write
	 */
	snd_soc_write(codec,
		(WCD934X_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		(band_idx * BAND_MAX * sizeof(uint32_t)) & 0x7F);

	set_iir_band_coeff(codec, iir_idx, band_idx,
				ucontrol->value.integer.value[0]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
				ucontrol->value.integer.value[1]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
				ucontrol->value.integer.value[2]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
				ucontrol->value.integer.value[3]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
				ucontrol->value.integer.value[4]);

	pr_debug("%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 0),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 1),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 2),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 3),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 4));
	return 0;
}

static int tavil_compander_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tavil->comp_enabled[comp];
	return 0;
}

static int tavil_compander_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: Compander %d enable current %d, new %d\n",
		 __func__, comp + 1, tavil->comp_enabled[comp], value);
	tavil->comp_enabled[comp] = value;

	/* Any specific register configuration for compander */
	switch (comp) {
	case COMPANDER_1:
		/* Set Gain Source Select based on compander enable/disable */
		snd_soc_update_bits(codec, WCD934X_HPH_L_EN, 0x20,
				(value ? 0x00:0x20));
		break;
	case COMPANDER_2:
		snd_soc_update_bits(codec, WCD934X_HPH_R_EN, 0x20,
				(value ? 0x00:0x20));
		break;
	case COMPANDER_3:
	case COMPANDER_4:
	case COMPANDER_7:
	case COMPANDER_8:
		break;
	default:
		/*
		 * if compander is not enabled for any interpolator,
		 * it does not cause any audio failure, so do not
		 * return error in this case, but just print a log
		 */
		dev_warn(codec->dev, "%s: unknown compander: %d\n",
			__func__, comp);
	};
	return 0;
}

static int tavil_dmic_pin_mode_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u16 ctl_reg;
	u8 reg_val, pinctl_position;

	pinctl_position = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	switch (pinctl_position >> 3) {
	case 0:
		ctl_reg = WCD934X_TEST_DEBUG_PIN_CTL_OE_0;
		break;
	case 1:
		ctl_reg = WCD934X_TEST_DEBUG_PIN_CTL_OE_1;
		break;
	case 2:
		ctl_reg = WCD934X_TEST_DEBUG_PIN_CTL_OE_2;
		break;
	case 3:
		ctl_reg = WCD934X_TEST_DEBUG_PIN_CTL_OE_3;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid pinctl position = %d\n",
			__func__, pinctl_position);
		return -EINVAL;
	}

	reg_val = snd_soc_read(codec, ctl_reg);
	reg_val = (reg_val >> (pinctl_position & 0x07)) & 0x1;
	ucontrol->value.integer.value[0] = reg_val;

	return 0;
}

static int tavil_dmic_pin_mode_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u16 ctl_reg, cfg_reg;
	u8 ctl_val, cfg_val, pinctl_position, pinctl_mode, mask;

	/* 1- high or low; 0- high Z */
	pinctl_mode = ucontrol->value.integer.value[0];
	pinctl_position = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	switch (pinctl_position >> 3) {
	case 0:
		ctl_reg = WCD934X_TEST_DEBUG_PIN_CTL_OE_0;
		break;
	case 1:
		ctl_reg = WCD934X_TEST_DEBUG_PIN_CTL_OE_1;
		break;
	case 2:
		ctl_reg = WCD934X_TEST_DEBUG_PIN_CTL_OE_2;
		break;
	case 3:
		ctl_reg = WCD934X_TEST_DEBUG_PIN_CTL_OE_3;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid pinctl position = %d\n",
			__func__, pinctl_position);
		return -EINVAL;
	}

	ctl_val = pinctl_mode << (pinctl_position & 0x07);
	mask = 1 << (pinctl_position & 0x07);
	snd_soc_update_bits(codec, ctl_reg, mask, ctl_val);

	cfg_reg = WCD934X_TLMM_BIST_MODE_PINCFG + pinctl_position;
	if (!pinctl_mode)
		cfg_val = 0x4;
	else
		cfg_val = 0;
	snd_soc_update_bits(codec, cfg_reg, 0x07, cfg_val);

	dev_dbg(codec->dev, "%s: reg=0x%x mask=0x%x val=%d reg=0x%x val=%d\n",
			__func__, ctl_reg, mask, ctl_val, cfg_reg, cfg_val);

	return 0;
}

static int tavil_amic_pwr_lvl_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u16 amic_reg;

	if (!strcmp(kcontrol->id.name, "AMIC_1_2 PWR MODE"))
		amic_reg = WCD934X_ANA_AMIC1;
	if (!strcmp(kcontrol->id.name, "AMIC_3_4 PWR MODE"))
		amic_reg = WCD934X_ANA_AMIC3;
	else
		goto ret;

	ucontrol->value.integer.value[0] =
		(snd_soc_read(codec, amic_reg) & WCD934X_AMIC_PWR_LVL_MASK) >>
			     WCD934X_AMIC_PWR_LVL_SHIFT;
ret:
	return 0;
}

static int tavil_amic_pwr_lvl_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u32 mode_val;
	u16 amic_reg;

	mode_val = ucontrol->value.enumerated.item[0];

	dev_dbg(codec->dev, "%s: mode: %d\n", __func__, mode_val);

	if (!strcmp(kcontrol->id.name, "AMIC_1_2 PWR MODE"))
		amic_reg = WCD934X_ANA_AMIC1;
	if (!strcmp(kcontrol->id.name, "AMIC_3_4 PWR MODE"))
		amic_reg = WCD934X_ANA_AMIC3;
	else
		goto ret;

	snd_soc_update_bits(codec, amic_reg, WCD934X_AMIC_PWR_LVL_MASK,
			    mode_val << WCD934X_AMIC_PWR_LVL_SHIFT);

ret:
	return 0;
}

static int tavil_ear_pa_gain_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	ear_pa_gain = snd_soc_read(codec, WCD934X_ANA_EAR);

	ear_pa_gain = (ear_pa_gain & 0x70) >> 4;

	ucontrol->value.integer.value[0] = ear_pa_gain;

	dev_dbg(codec->dev, "%s: ear_pa_gain = 0x%x\n", __func__,
		ear_pa_gain);

	return 0;
}

static int tavil_ear_pa_gain_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			__func__, ucontrol->value.integer.value[0]);

	ear_pa_gain =  ucontrol->value.integer.value[0] << 4;

	snd_soc_update_bits(codec, WCD934X_ANA_EAR, 0x70, ear_pa_gain);
	return 0;
}

/* Cutoff frequency for high pass filter */
static const char * const cf_text[] = {
	"CF_NEG_3DB_4HZ", "CF_NEG_3DB_75HZ", "CF_NEG_3DB_150HZ"
};

static const char * const rx_cf_text[] = {
	"CF_NEG_3DB_4HZ", "CF_NEG_3DB_75HZ", "CF_NEG_3DB_150HZ",
	"CF_NEG_3DB_0P48HZ"
};

static const char * const amic_pwr_lvl_text[] = {
	"LOW_PWR", "DEFAULT", "HIGH_PERF"
};

static const char * const tavil_ear_pa_gain_text[] = {
	"G_6_DB", "G_4P5_DB", "G_3_DB", "G_1P5_DB",
	"G_0_DB", "G_M2P5_DB", "UNDEFINED", "G_M12_DB"
};

static SOC_ENUM_SINGLE_EXT_DECL(tavil_ear_pa_gain_enum, tavil_ear_pa_gain_text);
static SOC_ENUM_SINGLE_EXT_DECL(amic_pwr_lvl_enum, amic_pwr_lvl_text);
static SOC_ENUM_SINGLE_DECL(cf_dec0_enum, WCD934X_CDC_TX0_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec1_enum, WCD934X_CDC_TX1_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec2_enum, WCD934X_CDC_TX2_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec3_enum, WCD934X_CDC_TX3_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec4_enum, WCD934X_CDC_TX4_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec5_enum, WCD934X_CDC_TX5_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec6_enum, WCD934X_CDC_TX6_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec7_enum, WCD934X_CDC_TX7_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec8_enum, WCD934X_CDC_TX8_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int0_1_enum, WCD934X_CDC_RX0_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int0_2_enum, WCD934X_CDC_RX0_RX_PATH_MIX_CFG, 2,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int1_1_enum, WCD934X_CDC_RX1_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int1_2_enum, WCD934X_CDC_RX1_RX_PATH_MIX_CFG, 2,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int2_1_enum, WCD934X_CDC_RX2_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int2_2_enum, WCD934X_CDC_RX2_RX_PATH_MIX_CFG, 2,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int3_1_enum, WCD934X_CDC_RX3_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int3_2_enum, WCD934X_CDC_RX3_RX_PATH_MIX_CFG, 2,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int4_1_enum, WCD934X_CDC_RX4_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int4_2_enum, WCD934X_CDC_RX4_RX_PATH_MIX_CFG, 2,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int7_1_enum, WCD934X_CDC_RX7_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int7_2_enum, WCD934X_CDC_RX7_RX_PATH_MIX_CFG, 2,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int8_1_enum, WCD934X_CDC_RX8_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int8_2_enum, WCD934X_CDC_RX8_RX_PATH_MIX_CFG, 2,
							rx_cf_text);

static const struct snd_kcontrol_new tavil_snd_controls[] = {
	SOC_ENUM_EXT("EAR PA Gain", tavil_ear_pa_gain_enum,
		tavil_ear_pa_gain_get, tavil_ear_pa_gain_put),
	SOC_SINGLE_TLV("HPHL Volume", WCD934X_HPH_L_EN, 0, 20, 1, line_gain),
	SOC_SINGLE_TLV("HPHR Volume", WCD934X_HPH_R_EN, 0, 20, 1, line_gain),
	SOC_SINGLE_TLV("LINEOUT1 Volume", WCD934X_DIFF_LO_LO1_COMPANDER,
		3, 16, 1, line_gain),
	SOC_SINGLE_TLV("LINEOUT2 Volume", WCD934X_DIFF_LO_LO2_COMPANDER,
		3, 16, 1, line_gain),
	SOC_SINGLE_TLV("ADC1 Volume", WCD934X_ANA_AMIC1, 0, 20, 0, analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", WCD934X_ANA_AMIC2, 0, 20, 0, analog_gain),
	SOC_SINGLE_TLV("ADC3 Volume", WCD934X_ANA_AMIC3, 0, 20, 0, analog_gain),
	SOC_SINGLE_TLV("ADC4 Volume", WCD934X_ANA_AMIC4, 0, 20, 0, analog_gain),

	SOC_SINGLE_SX_TLV("RX0 Digital Volume", WCD934X_CDC_RX0_RX_VOL_CTL,
		0, -84, 40, digital_gain), /* -84dB min - 40dB max */
	SOC_SINGLE_SX_TLV("RX1 Digital Volume", WCD934X_CDC_RX1_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX2 Digital Volume", WCD934X_CDC_RX2_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX3 Digital Volume", WCD934X_CDC_RX3_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX4 Digital Volume", WCD934X_CDC_RX4_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX7 Digital Volume", WCD934X_CDC_RX7_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX8 Digital Volume", WCD934X_CDC_RX8_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX0 Mix Digital Volume",
		WCD934X_CDC_RX0_RX_VOL_MIX_CTL, 0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX1 Mix Digital Volume",
		WCD934X_CDC_RX1_RX_VOL_MIX_CTL, 0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX2 Mix Digital Volume",
		WCD934X_CDC_RX2_RX_VOL_MIX_CTL, 0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX3 Mix Digital Volume",
		WCD934X_CDC_RX3_RX_VOL_MIX_CTL, 0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX4 Mix Digital Volume",
		WCD934X_CDC_RX4_RX_VOL_MIX_CTL, 0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX7 Mix Digital Volume",
		WCD934X_CDC_RX7_RX_VOL_MIX_CTL, 0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX8 Mix Digital Volume",
		WCD934X_CDC_RX8_RX_VOL_MIX_CTL, 0, -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("DEC0 Volume", WCD934X_CDC_TX0_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC1 Volume", WCD934X_CDC_TX1_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC2 Volume", WCD934X_CDC_TX2_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC3 Volume", WCD934X_CDC_TX3_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC4 Volume", WCD934X_CDC_TX4_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC7 Volume", WCD934X_CDC_TX7_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC8 Volume", WCD934X_CDC_TX8_TX_VOL_CTL, 0,
		-84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("IIR0 INP0 Volume",
		WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("IIR0 INP1 Volume",
		WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("IIR0 INP2 Volume",
		WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("IIR0 INP3 Volume",
		WCD934X_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP0 Volume",
		WCD934X_CDC_SIDETONE_IIR1_IIR_GAIN_B1_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP1 Volume",
		WCD934X_CDC_SIDETONE_IIR1_IIR_GAIN_B2_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP2 Volume",
		WCD934X_CDC_SIDETONE_IIR1_IIR_GAIN_B3_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP3 Volume",
		WCD934X_CDC_SIDETONE_IIR1_IIR_GAIN_B4_CTL, 0, -84, 40,
		digital_gain),

	SOC_ENUM("TX0 HPF cut off", cf_dec0_enum),
	SOC_ENUM("TX1 HPF cut off", cf_dec1_enum),
	SOC_ENUM("TX2 HPF cut off", cf_dec2_enum),
	SOC_ENUM("TX3 HPF cut off", cf_dec3_enum),
	SOC_ENUM("TX4 HPF cut off", cf_dec4_enum),
	SOC_ENUM("TX5 HPF cut off", cf_dec5_enum),
	SOC_ENUM("TX6 HPF cut off", cf_dec6_enum),
	SOC_ENUM("TX7 HPF cut off", cf_dec7_enum),
	SOC_ENUM("TX8 HPF cut off", cf_dec8_enum),

	SOC_ENUM("RX INT0_1 HPF cut off", cf_int0_1_enum),
	SOC_ENUM("RX INT0_2 HPF cut off", cf_int0_2_enum),
	SOC_ENUM("RX INT1_1 HPF cut off", cf_int1_1_enum),
	SOC_ENUM("RX INT1_2 HPF cut off", cf_int1_2_enum),
	SOC_ENUM("RX INT2_1 HPF cut off", cf_int2_1_enum),
	SOC_ENUM("RX INT2_2 HPF cut off", cf_int2_2_enum),
	SOC_ENUM("RX INT3_1 HPF cut off", cf_int3_1_enum),
	SOC_ENUM("RX INT3_2 HPF cut off", cf_int3_2_enum),
	SOC_ENUM("RX INT4_1 HPF cut off", cf_int4_1_enum),
	SOC_ENUM("RX INT4_2 HPF cut off", cf_int4_2_enum),
	SOC_ENUM("RX INT7_1 HPF cut off", cf_int7_1_enum),
	SOC_ENUM("RX INT7_2 HPF cut off", cf_int7_2_enum),
	SOC_ENUM("RX INT8_1 HPF cut off", cf_int8_1_enum),
	SOC_ENUM("RX INT8_2 HPF cut off", cf_int8_2_enum),

	SOC_SINGLE_EXT("IIR0 Enable Band1", IIR0, BAND1, 1, 0,
		tavil_iir_enable_audio_mixer_get,
		tavil_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band2", IIR0, BAND2, 1, 0,
		tavil_iir_enable_audio_mixer_get,
		tavil_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band3", IIR0, BAND3, 1, 0,
		tavil_iir_enable_audio_mixer_get,
		tavil_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band4", IIR0, BAND4, 1, 0,
		tavil_iir_enable_audio_mixer_get,
		tavil_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band5", IIR0, BAND5, 1, 0,
		tavil_iir_enable_audio_mixer_get,
		tavil_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
		tavil_iir_enable_audio_mixer_get,
		tavil_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
		tavil_iir_enable_audio_mixer_get,
		tavil_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
		tavil_iir_enable_audio_mixer_get,
		tavil_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
		tavil_iir_enable_audio_mixer_get,
		tavil_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
		tavil_iir_enable_audio_mixer_get,
		tavil_iir_enable_audio_mixer_put),

	SOC_SINGLE_MULTI_EXT("IIR0 Band1", IIR0, BAND1, 255, 0, 5,
		tavil_iir_band_audio_mixer_get, tavil_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band2", IIR0, BAND2, 255, 0, 5,
		tavil_iir_band_audio_mixer_get, tavil_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band3", IIR0, BAND3, 255, 0, 5,
		tavil_iir_band_audio_mixer_get, tavil_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band4", IIR0, BAND4, 255, 0, 5,
		tavil_iir_band_audio_mixer_get, tavil_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band5", IIR0, BAND5, 255, 0, 5,
		tavil_iir_band_audio_mixer_get, tavil_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
		tavil_iir_band_audio_mixer_get, tavil_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
		tavil_iir_band_audio_mixer_get, tavil_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
		tavil_iir_band_audio_mixer_get, tavil_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
		tavil_iir_band_audio_mixer_get, tavil_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
		tavil_iir_band_audio_mixer_get, tavil_iir_band_audio_mixer_put),

	SOC_SINGLE_EXT("COMP1 Switch", SND_SOC_NOPM, COMPANDER_1, 1, 0,
		tavil_compander_get, tavil_compander_put),
	SOC_SINGLE_EXT("COMP2 Switch", SND_SOC_NOPM, COMPANDER_2, 1, 0,
		tavil_compander_get, tavil_compander_put),
	SOC_SINGLE_EXT("COMP3 Switch", SND_SOC_NOPM, COMPANDER_3, 1, 0,
		tavil_compander_get, tavil_compander_put),
	SOC_SINGLE_EXT("COMP4 Switch", SND_SOC_NOPM, COMPANDER_4, 1, 0,
		tavil_compander_get, tavil_compander_put),
	SOC_SINGLE_EXT("COMP7 Switch", SND_SOC_NOPM, COMPANDER_7, 1, 0,
		tavil_compander_get, tavil_compander_put),
	SOC_SINGLE_EXT("COMP8 Switch", SND_SOC_NOPM, COMPANDER_8, 1, 0,
		tavil_compander_get, tavil_compander_put),

	SOC_SINGLE_EXT("DMIC1_CLK_PIN_MODE", SND_SOC_NOPM, 17, 1, 0,
		tavil_dmic_pin_mode_get, tavil_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC1_DATA_PIN_MODE", SND_SOC_NOPM, 18, 1, 0,
		tavil_dmic_pin_mode_get, tavil_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC2_CLK_PIN_MODE", SND_SOC_NOPM, 19, 1, 0,
		tavil_dmic_pin_mode_get, tavil_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC2_DATA_PIN_MODE", SND_SOC_NOPM, 20, 1, 0,
		tavil_dmic_pin_mode_get, tavil_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC3_CLK_PIN_MODE", SND_SOC_NOPM, 21, 1, 0,
		tavil_dmic_pin_mode_get, tavil_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC3_DATA_PIN_MODE", SND_SOC_NOPM, 22, 1, 0,
		tavil_dmic_pin_mode_get, tavil_dmic_pin_mode_put),
	SOC_ENUM_EXT("AMIC_1_2 PWR MODE", amic_pwr_lvl_enum,
		tavil_amic_pwr_lvl_get, tavil_amic_pwr_lvl_put),
	SOC_ENUM_EXT("AMIC_3_4 PWR MODE", amic_pwr_lvl_enum,
		tavil_amic_pwr_lvl_get, tavil_amic_pwr_lvl_put),
	SOC_ENUM_EXT("AMIC_5_6 PWR MODE", amic_pwr_lvl_enum,
		tavil_amic_pwr_lvl_get, tavil_amic_pwr_lvl_put),
};

static int tavil_dec_enum_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	u16 mic_sel_reg = 0;
	u8 mic_sel;

	val = ucontrol->value.enumerated.item[0];
	if (val > e->items - 1)
		return -EINVAL;

	dev_dbg(codec->dev, "%s: wname: %s, val: 0x%x\n", __func__,
		widget->name, val);

	switch (e->reg) {
	case WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG1:
		if (e->shift_l == 0)
			mic_sel_reg = WCD934X_CDC_TX0_TX_PATH_CFG0;
		else if (e->shift_l == 2)
			mic_sel_reg = WCD934X_CDC_TX4_TX_PATH_CFG0;
		else if (e->shift_l == 4)
			mic_sel_reg = WCD934X_CDC_TX8_TX_PATH_CFG0;
		break;
	case WCD934X_CDC_TX_INP_MUX_ADC_MUX1_CFG1:
		if (e->shift_l == 0)
			mic_sel_reg = WCD934X_CDC_TX1_TX_PATH_CFG0;
		else if (e->shift_l == 2)
			mic_sel_reg = WCD934X_CDC_TX5_TX_PATH_CFG0;
		break;
	case WCD934X_CDC_TX_INP_MUX_ADC_MUX2_CFG1:
		if (e->shift_l == 0)
			mic_sel_reg = WCD934X_CDC_TX2_TX_PATH_CFG0;
		else if (e->shift_l == 2)
			mic_sel_reg = WCD934X_CDC_TX6_TX_PATH_CFG0;
		break;
	case WCD934X_CDC_TX_INP_MUX_ADC_MUX3_CFG1:
		if (e->shift_l == 0)
			mic_sel_reg = WCD934X_CDC_TX3_TX_PATH_CFG0;
		else if (e->shift_l == 2)
			mic_sel_reg = WCD934X_CDC_TX7_TX_PATH_CFG0;
		break;
	default:
		dev_err(codec->dev, "%s: e->reg: 0x%x not expected\n",
			__func__, e->reg);
		return -EINVAL;
	}

	/* ADC: 0, DMIC: 1 */
	mic_sel = val ? 0x0 : 0x1;
	if (mic_sel_reg)
		snd_soc_update_bits(codec, mic_sel_reg, 1 << 7, mic_sel << 7);

	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static int tavil_int_dem_inp_mux_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	unsigned short look_ahead_dly_reg = WCD934X_CDC_RX0_RX_PATH_CFG0;

	val = ucontrol->value.enumerated.item[0];
	if (val >= e->items)
		return -EINVAL;

	dev_dbg(codec->dev, "%s: wname: %s, val: 0x%x\n", __func__,
		widget->name, val);

	if (e->reg == WCD934X_CDC_RX0_RX_PATH_SEC0)
		look_ahead_dly_reg = WCD934X_CDC_RX0_RX_PATH_CFG0;
	else if (e->reg == WCD934X_CDC_RX1_RX_PATH_SEC0)
		look_ahead_dly_reg = WCD934X_CDC_RX1_RX_PATH_CFG0;
	else if (e->reg == WCD934X_CDC_RX2_RX_PATH_SEC0)
		look_ahead_dly_reg = WCD934X_CDC_RX2_RX_PATH_CFG0;

	/* Set Look Ahead Delay */
	snd_soc_update_bits(codec, look_ahead_dly_reg,
			    0x08, (val ? 0x08 : 0x00));
	/* Set DEM INP Select */
	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static const char * const rx_int0_7_mix_mux_text[] = {
	"ZERO", "RX0", "RX1", "RX2", "RX3", "RX4", "RX5",
	"RX6", "RX7", "PROXIMITY"
};

static const char * const rx_int_mix_mux_text[] = {
	"ZERO", "RX0", "RX1", "RX2", "RX3", "RX4", "RX5",
	"RX6", "RX7"
};

static const char * const rx_prim_mix_text[] = {
	"ZERO", "DEC0", "DEC1", "IIR0", "IIR1", "RX0", "RX1", "RX2",
	"RX3", "RX4", "RX5", "RX6", "RX7"
};

static const char * const rx_sidetone_mix_text[] = {
	"ZERO", "SRC0", "SRC1", "SRC_SUM"
};

static const char * const cdc_if_tx0_mux_text[] = {
	"ZERO", "RX_MIX_TX0", "DEC0", "DEC0_192"
};
static const char * const cdc_if_tx1_mux_text[] = {
	"ZERO", "RX_MIX_TX1", "DEC1", "DEC1_192"
};
static const char * const cdc_if_tx2_mux_text[] = {
	"ZERO", "RX_MIX_TX2", "DEC2", "DEC2_192"
};
static const char * const cdc_if_tx3_mux_text[] = {
	"ZERO", "RX_MIX_TX3", "DEC3", "DEC3_192"
};
static const char * const cdc_if_tx4_mux_text[] = {
	"ZERO", "RX_MIX_TX4", "DEC4", "DEC4_192"
};
static const char * const cdc_if_tx5_mux_text[] = {
	"ZERO", "RX_MIX_TX5", "DEC5", "DEC5_192"
};
static const char * const cdc_if_tx6_mux_text[] = {
	"ZERO", "RX_MIX_TX6", "DEC6", "DEC6_192"
};
static const char * const cdc_if_tx7_mux_text[] = {
	"ZERO", "RX_MIX_TX7", "DEC7", "DEC7_192"
};
static const char * const cdc_if_tx8_mux_text[] = {
	"ZERO", "RX_MIX_TX8", "DEC8", "DEC8_192"
};
static const char * const cdc_if_tx9_mux_text[] = {
	"ZERO", "DEC7", "DEC7_192"
};
static const char * const cdc_if_tx10_mux_text[] = {
	"ZERO", "DEC6", "DEC6_192"
};
static const char * const cdc_if_tx11_mux_text[] = {
	"DEC_0_5", "DEC_9_12", "MAD_AUDIO", "MAD_BRDCST"
};
static const char * const cdc_if_tx11_inp1_mux_text[] = {
	"ZERO", "DEC0", "DEC1", "DEC2", "DEC3", "DEC4",
	"DEC5", "RX_MIX_TX5", "DEC9_10", "DEC11_12"
};
static const char * const cdc_if_tx13_mux_text[] = {
	"CDC_DEC_5", "MAD_BRDCST"
};
static const char * const cdc_if_tx13_inp1_mux_text[] = {
	"ZERO", "DEC5", "DEC5_192"
};

static const char * const iir_inp_mux_text[] = {
	"ZERO", "DEC0", "DEC1", "DEC2", "DEC3", "DEC4", "DEC5", "DEC6",
	"DEC7", "DEC8", "RX0", "RX1", "RX2", "RX3", "RX4", "RX5", "RX6", "RX7"
};

static const char * const rx_int_dem_inp_mux_text[] = {
	"NORMAL_DSM_OUT", "CLSH_DSM_OUT",
};

static const char * const rx_int0_interp_mux_text[] = {
	"ZERO", "RX INT0 MIX2",
};

static const char * const rx_int1_interp_mux_text[] = {
	"ZERO", "RX INT1 MIX2",
};

static const char * const rx_int2_interp_mux_text[] = {
	"ZERO", "RX INT2 MIX2",
};

static const char * const rx_int3_interp_mux_text[] = {
	"ZERO", "RX INT3 MIX2",
};

static const char * const rx_int4_interp_mux_text[] = {
	"ZERO", "RX INT4 MIX2",
};

static const char * const rx_int7_interp_mux_text[] = {
	"ZERO", "RX INT7 MIX2",
};

static const char * const rx_int8_interp_mux_text[] = {
	"ZERO", "RX INT8 SEC MIX"
};

static const char * const adc_mux_text[] = {
	"DMIC", "AMIC", "ANC_FB_TUNE1", "ANC_FB_TUNE2"
};

static const char * const dmic_mux_text[] = {
	"ZERO", "DMIC0", "DMIC1", "DMIC2", "DMIC3", "DMIC4", "DMIC5"
};

static const char * const amic_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "ADC4"
};

static const char * const amic4_5_sel_text[] = {
	"AMIC4", "AMIC5"
};

static const char * const rx_echo_mux_text[] = {
	"ZERO", "RX_MIX0", "RX_MIX1", "RX_MIX2", "RX_MIX3", "RX_MIX4",
	"RX_MIX5", "RX_MIX6", "RX_MIX7", "RX_MIX8"
};

static const char *const slim_rx_mux_text[] = {
	"ZERO", "AIF1_PB", "AIF2_PB", "AIF3_PB", "AIF4_PB"
};

static const char *const cdc_if_rx0_mux_text[] = {
	"SLIM RX0", "I2S_0 RX0"
};
static const char *const cdc_if_rx1_mux_text[] = {
	"SLIM RX1", "I2S_0 RX1"
};
static const char *const cdc_if_rx2_mux_text[] = {
	"SLIM RX2", "I2S_0 RX2"
};
static const char *const cdc_if_rx3_mux_text[] = {
	"SLIM RX3", "I2S_0 RX3"
};
static const char *const cdc_if_rx4_mux_text[] = {
	"SLIM RX4", "I2S_0 RX4"
};
static const char *const cdc_if_rx5_mux_text[] = {
	"SLIM RX5", "I2S_0 RX5"
};
static const char *const cdc_if_rx6_mux_text[] = {
	"SLIM RX6", "I2S_0 RX6"
};
static const char *const cdc_if_rx7_mux_text[] = {
	"SLIM RX7", "I2S_0 RX7"
};

static const struct snd_kcontrol_new aif1_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX0", SND_SOC_NOPM, WCD934X_TX0, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, WCD934X_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, WCD934X_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, WCD934X_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, WCD934X_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, WCD934X_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX6", SND_SOC_NOPM, WCD934X_TX6, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX7", SND_SOC_NOPM, WCD934X_TX7, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX8", SND_SOC_NOPM, WCD934X_TX8, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX9", SND_SOC_NOPM, WCD934X_TX9, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX10", SND_SOC_NOPM, WCD934X_TX10, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX11", SND_SOC_NOPM, WCD934X_TX11, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX13", SND_SOC_NOPM, WCD934X_TX13, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static const struct snd_kcontrol_new aif2_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX0", SND_SOC_NOPM, WCD934X_TX0, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, WCD934X_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, WCD934X_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, WCD934X_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, WCD934X_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, WCD934X_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX6", SND_SOC_NOPM, WCD934X_TX6, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX7", SND_SOC_NOPM, WCD934X_TX7, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX8", SND_SOC_NOPM, WCD934X_TX8, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX9", SND_SOC_NOPM, WCD934X_TX9, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX10", SND_SOC_NOPM, WCD934X_TX10, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX11", SND_SOC_NOPM, WCD934X_TX11, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX13", SND_SOC_NOPM, WCD934X_TX13, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static const struct snd_kcontrol_new aif3_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX0", SND_SOC_NOPM, WCD934X_TX0, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, WCD934X_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, WCD934X_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, WCD934X_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, WCD934X_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, WCD934X_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX6", SND_SOC_NOPM, WCD934X_TX6, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX7", SND_SOC_NOPM, WCD934X_TX7, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX8", SND_SOC_NOPM, WCD934X_TX8, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX9", SND_SOC_NOPM, WCD934X_TX9, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX10", SND_SOC_NOPM, WCD934X_TX10, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX11", SND_SOC_NOPM, WCD934X_TX11, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX13", SND_SOC_NOPM, WCD934X_TX13, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

WCD_DAPM_ENUM_EXT(slim_rx0, SND_SOC_NOPM, 0, slim_rx_mux_text,
	slim_rx_mux_get, slim_rx_mux_put);
WCD_DAPM_ENUM_EXT(slim_rx1, SND_SOC_NOPM, 0, slim_rx_mux_text,
	slim_rx_mux_get, slim_rx_mux_put);
WCD_DAPM_ENUM_EXT(slim_rx2, SND_SOC_NOPM, 0, slim_rx_mux_text,
	slim_rx_mux_get, slim_rx_mux_put);
WCD_DAPM_ENUM_EXT(slim_rx3, SND_SOC_NOPM, 0, slim_rx_mux_text,
	slim_rx_mux_get, slim_rx_mux_put);
WCD_DAPM_ENUM_EXT(slim_rx4, SND_SOC_NOPM, 0, slim_rx_mux_text,
	slim_rx_mux_get, slim_rx_mux_put);
WCD_DAPM_ENUM_EXT(slim_rx5, SND_SOC_NOPM, 0, slim_rx_mux_text,
	slim_rx_mux_get, slim_rx_mux_put);
WCD_DAPM_ENUM_EXT(slim_rx6, SND_SOC_NOPM, 0, slim_rx_mux_text,
	slim_rx_mux_get, slim_rx_mux_put);
WCD_DAPM_ENUM_EXT(slim_rx7, SND_SOC_NOPM, 0, slim_rx_mux_text,
	slim_rx_mux_get, slim_rx_mux_put);

WCD_DAPM_ENUM(cdc_if_rx0, SND_SOC_NOPM, 0, cdc_if_rx0_mux_text);
WCD_DAPM_ENUM(cdc_if_rx1, SND_SOC_NOPM, 0, cdc_if_rx1_mux_text);
WCD_DAPM_ENUM(cdc_if_rx2, SND_SOC_NOPM, 0, cdc_if_rx2_mux_text);
WCD_DAPM_ENUM(cdc_if_rx3, SND_SOC_NOPM, 0, cdc_if_rx3_mux_text);
WCD_DAPM_ENUM(cdc_if_rx4, SND_SOC_NOPM, 0, cdc_if_rx4_mux_text);
WCD_DAPM_ENUM(cdc_if_rx5, SND_SOC_NOPM, 0, cdc_if_rx5_mux_text);
WCD_DAPM_ENUM(cdc_if_rx6, SND_SOC_NOPM, 0, cdc_if_rx6_mux_text);
WCD_DAPM_ENUM(cdc_if_rx7, SND_SOC_NOPM, 0, cdc_if_rx7_mux_text);

WCD_DAPM_ENUM(rx_int0_2, WCD934X_CDC_RX_INP_MUX_RX_INT0_CFG1, 0,
	rx_int0_7_mix_mux_text);
WCD_DAPM_ENUM(rx_int1_2, WCD934X_CDC_RX_INP_MUX_RX_INT1_CFG1, 0,
	rx_int_mix_mux_text);
WCD_DAPM_ENUM(rx_int2_2, WCD934X_CDC_RX_INP_MUX_RX_INT2_CFG1, 0,
	rx_int_mix_mux_text);
WCD_DAPM_ENUM(rx_int3_2, WCD934X_CDC_RX_INP_MUX_RX_INT3_CFG1, 0,
	rx_int_mix_mux_text);
WCD_DAPM_ENUM(rx_int4_2, WCD934X_CDC_RX_INP_MUX_RX_INT4_CFG1, 0,
	rx_int_mix_mux_text);
WCD_DAPM_ENUM(rx_int7_2, WCD934X_CDC_RX_INP_MUX_RX_INT7_CFG1, 0,
	rx_int0_7_mix_mux_text);
WCD_DAPM_ENUM(rx_int8_2, WCD934X_CDC_RX_INP_MUX_RX_INT8_CFG1, 0,
	rx_int_mix_mux_text);

WCD_DAPM_ENUM(rx_int0_1_mix_inp0, WCD934X_CDC_RX_INP_MUX_RX_INT0_CFG0, 0,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int0_1_mix_inp1, WCD934X_CDC_RX_INP_MUX_RX_INT0_CFG0, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int0_1_mix_inp2, WCD934X_CDC_RX_INP_MUX_RX_INT0_CFG1, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int1_1_mix_inp0, WCD934X_CDC_RX_INP_MUX_RX_INT1_CFG0, 0,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int1_1_mix_inp1, WCD934X_CDC_RX_INP_MUX_RX_INT1_CFG0, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int1_1_mix_inp2, WCD934X_CDC_RX_INP_MUX_RX_INT1_CFG1, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int2_1_mix_inp0, WCD934X_CDC_RX_INP_MUX_RX_INT2_CFG0, 0,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int2_1_mix_inp1, WCD934X_CDC_RX_INP_MUX_RX_INT2_CFG0, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int2_1_mix_inp2, WCD934X_CDC_RX_INP_MUX_RX_INT2_CFG1, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int3_1_mix_inp0, WCD934X_CDC_RX_INP_MUX_RX_INT3_CFG0, 0,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int3_1_mix_inp1, WCD934X_CDC_RX_INP_MUX_RX_INT3_CFG0, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int3_1_mix_inp2, WCD934X_CDC_RX_INP_MUX_RX_INT3_CFG1, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int4_1_mix_inp0, WCD934X_CDC_RX_INP_MUX_RX_INT4_CFG0, 0,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int4_1_mix_inp1, WCD934X_CDC_RX_INP_MUX_RX_INT4_CFG0, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int4_1_mix_inp2, WCD934X_CDC_RX_INP_MUX_RX_INT4_CFG1, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int7_1_mix_inp0, WCD934X_CDC_RX_INP_MUX_RX_INT7_CFG0, 0,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int7_1_mix_inp1, WCD934X_CDC_RX_INP_MUX_RX_INT7_CFG0, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int7_1_mix_inp2, WCD934X_CDC_RX_INP_MUX_RX_INT7_CFG1, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int8_1_mix_inp0, WCD934X_CDC_RX_INP_MUX_RX_INT8_CFG0, 0,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int8_1_mix_inp1, WCD934X_CDC_RX_INP_MUX_RX_INT8_CFG0, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int8_1_mix_inp2, WCD934X_CDC_RX_INP_MUX_RX_INT8_CFG1, 4,
	rx_prim_mix_text);

WCD_DAPM_ENUM(rx_int0_mix2_inp, WCD934X_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 0,
	rx_sidetone_mix_text);
WCD_DAPM_ENUM(rx_int1_mix2_inp, WCD934X_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 2,
	rx_sidetone_mix_text);
WCD_DAPM_ENUM(rx_int2_mix2_inp, WCD934X_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 4,
	rx_sidetone_mix_text);
WCD_DAPM_ENUM(rx_int3_mix2_inp, WCD934X_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 6,
	rx_sidetone_mix_text);
WCD_DAPM_ENUM(rx_int4_mix2_inp, WCD934X_CDC_RX_INP_MUX_SIDETONE_SRC_CFG1, 0,
	rx_sidetone_mix_text);
WCD_DAPM_ENUM(rx_int7_mix2_inp, WCD934X_CDC_RX_INP_MUX_SIDETONE_SRC_CFG1, 2,
	rx_sidetone_mix_text);

WCD_DAPM_ENUM(tx_adc_mux10, WCD934X_CDC_TX_INP_MUX_ADC_MUX1_CFG1, 4,
	adc_mux_text);
WCD_DAPM_ENUM(tx_adc_mux11, WCD934X_CDC_TX_INP_MUX_ADC_MUX2_CFG1, 4,
	adc_mux_text);
WCD_DAPM_ENUM(tx_adc_mux12, WCD934X_CDC_TX_INP_MUX_ADC_MUX3_CFG1, 4,
	adc_mux_text);
WCD_DAPM_ENUM(tx_adc_mux13, WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG1, 6,
	adc_mux_text);


WCD_DAPM_ENUM(tx_dmic_mux0, WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux1, WCD934X_CDC_TX_INP_MUX_ADC_MUX1_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux2, WCD934X_CDC_TX_INP_MUX_ADC_MUX2_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux3, WCD934X_CDC_TX_INP_MUX_ADC_MUX3_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux4, WCD934X_CDC_TX_INP_MUX_ADC_MUX4_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux5, WCD934X_CDC_TX_INP_MUX_ADC_MUX5_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux6, WCD934X_CDC_TX_INP_MUX_ADC_MUX6_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux7, WCD934X_CDC_TX_INP_MUX_ADC_MUX7_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux8, WCD934X_CDC_TX_INP_MUX_ADC_MUX8_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux10, WCD934X_CDC_TX_INP_MUX_ADC_MUX10_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux11, WCD934X_CDC_TX_INP_MUX_ADC_MUX11_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux12, WCD934X_CDC_TX_INP_MUX_ADC_MUX12_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux13, WCD934X_CDC_TX_INP_MUX_ADC_MUX13_CFG0, 3,
	dmic_mux_text);


WCD_DAPM_ENUM(tx_amic_mux0, WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux1, WCD934X_CDC_TX_INP_MUX_ADC_MUX1_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux2, WCD934X_CDC_TX_INP_MUX_ADC_MUX2_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux3, WCD934X_CDC_TX_INP_MUX_ADC_MUX3_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux4, WCD934X_CDC_TX_INP_MUX_ADC_MUX4_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux5, WCD934X_CDC_TX_INP_MUX_ADC_MUX5_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux6, WCD934X_CDC_TX_INP_MUX_ADC_MUX6_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux7, WCD934X_CDC_TX_INP_MUX_ADC_MUX7_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux8, WCD934X_CDC_TX_INP_MUX_ADC_MUX8_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux10, WCD934X_CDC_TX_INP_MUX_ADC_MUX10_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux11, WCD934X_CDC_TX_INP_MUX_ADC_MUX11_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux12, WCD934X_CDC_TX_INP_MUX_ADC_MUX12_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux13, WCD934X_CDC_TX_INP_MUX_ADC_MUX13_CFG0, 0,
	amic_mux_text);

WCD_DAPM_ENUM(tx_amic4_5, WCD934X_TX_NEW_AMIC_4_5_SEL, 7, amic4_5_sel_text);

WCD_DAPM_ENUM(cdc_if_tx0, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG0, 0,
	cdc_if_tx0_mux_text);
WCD_DAPM_ENUM(cdc_if_tx1, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG0, 2,
	cdc_if_tx1_mux_text);
WCD_DAPM_ENUM(cdc_if_tx2, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG0, 4,
	cdc_if_tx2_mux_text);
WCD_DAPM_ENUM(cdc_if_tx3, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG0, 6,
	cdc_if_tx3_mux_text);
WCD_DAPM_ENUM(cdc_if_tx4, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG1, 0,
	cdc_if_tx4_mux_text);
WCD_DAPM_ENUM(cdc_if_tx5, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG1, 2,
	cdc_if_tx5_mux_text);
WCD_DAPM_ENUM(cdc_if_tx6, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG1, 4,
	cdc_if_tx6_mux_text);
WCD_DAPM_ENUM(cdc_if_tx7, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG1, 6,
	cdc_if_tx7_mux_text);
WCD_DAPM_ENUM(cdc_if_tx8, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG2, 0,
	cdc_if_tx8_mux_text);
WCD_DAPM_ENUM(cdc_if_tx9, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG2, 2,
	cdc_if_tx9_mux_text);
WCD_DAPM_ENUM(cdc_if_tx10, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG2, 4,
	cdc_if_tx10_mux_text);
WCD_DAPM_ENUM(cdc_if_tx11_inp1, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG3, 0,
	cdc_if_tx11_inp1_mux_text);
WCD_DAPM_ENUM(cdc_if_tx11, WCD934X_DATA_HUB_SB_TX11_INP_CFG, 0,
	cdc_if_tx11_mux_text);
WCD_DAPM_ENUM(cdc_if_tx13_inp1, WCD934X_CDC_IF_ROUTER_TX_MUX_CFG3, 4,
	cdc_if_tx13_inp1_mux_text);
WCD_DAPM_ENUM(cdc_if_tx13, WCD934X_DATA_HUB_SB_TX13_INP_CFG, 0,
	cdc_if_tx13_mux_text);

WCD_DAPM_ENUM(rx_mix_tx0, WCD934X_CDC_RX_INP_MUX_RX_MIX_CFG0, 0,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx1, WCD934X_CDC_RX_INP_MUX_RX_MIX_CFG0, 4,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx2, WCD934X_CDC_RX_INP_MUX_RX_MIX_CFG1, 0,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx3, WCD934X_CDC_RX_INP_MUX_RX_MIX_CFG1, 4,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx4, WCD934X_CDC_RX_INP_MUX_RX_MIX_CFG2, 0,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx5, WCD934X_CDC_RX_INP_MUX_RX_MIX_CFG2, 4,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx6, WCD934X_CDC_RX_INP_MUX_RX_MIX_CFG3, 0,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx7, WCD934X_CDC_RX_INP_MUX_RX_MIX_CFG3, 4,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx8, WCD934X_CDC_RX_INP_MUX_RX_MIX_CFG4, 0,
	rx_echo_mux_text);

WCD_DAPM_ENUM(iir0_inp0, WCD934X_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG0, 0,
	iir_inp_mux_text);
WCD_DAPM_ENUM(iir0_inp1, WCD934X_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG1, 0,
	iir_inp_mux_text);
WCD_DAPM_ENUM(iir0_inp2, WCD934X_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG2, 0,
	iir_inp_mux_text);
WCD_DAPM_ENUM(iir0_inp3, WCD934X_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG3, 0,
	iir_inp_mux_text);
WCD_DAPM_ENUM(iir1_inp0, WCD934X_CDC_SIDETONE_IIR_INP_MUX_IIR1_MIX_CFG0, 0,
	iir_inp_mux_text);
WCD_DAPM_ENUM(iir1_inp1, WCD934X_CDC_SIDETONE_IIR_INP_MUX_IIR1_MIX_CFG1, 0,
	iir_inp_mux_text);
WCD_DAPM_ENUM(iir1_inp2, WCD934X_CDC_SIDETONE_IIR_INP_MUX_IIR1_MIX_CFG2, 0,
	iir_inp_mux_text);
WCD_DAPM_ENUM(iir1_inp3, WCD934X_CDC_SIDETONE_IIR_INP_MUX_IIR1_MIX_CFG3, 0,
	iir_inp_mux_text);

WCD_DAPM_ENUM(rx_int0_interp, WCD934X_CDC_RX0_RX_PATH_CTL, 5,
	rx_int0_interp_mux_text);
WCD_DAPM_ENUM(rx_int1_interp, WCD934X_CDC_RX1_RX_PATH_CTL, 5,
	rx_int1_interp_mux_text);
WCD_DAPM_ENUM(rx_int2_interp, WCD934X_CDC_RX2_RX_PATH_CTL, 5,
	rx_int2_interp_mux_text);
WCD_DAPM_ENUM(rx_int3_interp, WCD934X_CDC_RX3_RX_PATH_CTL, 5,
	rx_int3_interp_mux_text);
WCD_DAPM_ENUM(rx_int4_interp, WCD934X_CDC_RX4_RX_PATH_CTL, 5,
	rx_int4_interp_mux_text);
WCD_DAPM_ENUM(rx_int7_interp, WCD934X_CDC_RX7_RX_PATH_CTL, 5,
	rx_int7_interp_mux_text);
WCD_DAPM_ENUM(rx_int8_interp, WCD934X_CDC_RX8_RX_PATH_CTL, 5,
	rx_int8_interp_mux_text);

WCD_DAPM_ENUM_EXT(rx_int0_dem_inp, WCD934X_CDC_RX0_RX_PATH_SEC0, 0,
	rx_int_dem_inp_mux_text, snd_soc_dapm_get_enum_double,
	tavil_int_dem_inp_mux_put);
WCD_DAPM_ENUM_EXT(rx_int1_dem_inp, WCD934X_CDC_RX1_RX_PATH_SEC0, 0,
	rx_int_dem_inp_mux_text, snd_soc_dapm_get_enum_double,
	tavil_int_dem_inp_mux_put);
WCD_DAPM_ENUM_EXT(rx_int2_dem_inp, WCD934X_CDC_RX2_RX_PATH_SEC0, 0,
	rx_int_dem_inp_mux_text, snd_soc_dapm_get_enum_double,
	tavil_int_dem_inp_mux_put);

WCD_DAPM_ENUM_EXT(tx_adc_mux0, WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG1, 0,
	adc_mux_text, snd_soc_dapm_get_enum_double, tavil_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux1, WCD934X_CDC_TX_INP_MUX_ADC_MUX1_CFG1, 0,
	adc_mux_text, snd_soc_dapm_get_enum_double, tavil_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux2, WCD934X_CDC_TX_INP_MUX_ADC_MUX2_CFG1, 0,
	adc_mux_text, snd_soc_dapm_get_enum_double, tavil_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux3, WCD934X_CDC_TX_INP_MUX_ADC_MUX3_CFG1, 0,
	adc_mux_text, snd_soc_dapm_get_enum_double, tavil_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux4, WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG1, 2,
	adc_mux_text, snd_soc_dapm_get_enum_double, tavil_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux5, WCD934X_CDC_TX_INP_MUX_ADC_MUX1_CFG1, 2,
	adc_mux_text, snd_soc_dapm_get_enum_double, tavil_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux6, WCD934X_CDC_TX_INP_MUX_ADC_MUX2_CFG1, 2,
	adc_mux_text, snd_soc_dapm_get_enum_double, tavil_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux7, WCD934X_CDC_TX_INP_MUX_ADC_MUX3_CFG1, 2,
	adc_mux_text, snd_soc_dapm_get_enum_double, tavil_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux8, WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG1, 4,
	adc_mux_text, snd_soc_dapm_get_enum_double, tavil_dec_enum_put);

static const struct snd_kcontrol_new adc_us_mux0_switch =
	SOC_DAPM_SINGLE("US_Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new adc_us_mux1_switch =
	SOC_DAPM_SINGLE("US_Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new adc_us_mux2_switch =
	SOC_DAPM_SINGLE("US_Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new adc_us_mux3_switch =
	SOC_DAPM_SINGLE("US_Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new adc_us_mux4_switch =
	SOC_DAPM_SINGLE("US_Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new adc_us_mux5_switch =
	SOC_DAPM_SINGLE("US_Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new adc_us_mux6_switch =
	SOC_DAPM_SINGLE("US_Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new adc_us_mux7_switch =
	SOC_DAPM_SINGLE("US_Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new adc_us_mux8_switch =
	SOC_DAPM_SINGLE("US_Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_soc_dapm_widget tavil_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN_E("AIF1 PB", "AIF1 Playback", 0, SND_SOC_NOPM,
		AIF1_PB, 0, tavil_codec_enable_slimrx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF2 PB", "AIF2 Playback", 0, SND_SOC_NOPM,
		AIF2_PB, 0, tavil_codec_enable_slimrx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF3 PB", "AIF3 Playback", 0, SND_SOC_NOPM,
		AIF3_PB, 0, tavil_codec_enable_slimrx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF4 PB", "AIF4 Playback", 0, SND_SOC_NOPM,
		AIF4_PB, 0, tavil_codec_enable_slimrx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	WCD_DAPM_MUX("SLIM RX0 MUX", WCD934X_RX0, slim_rx0),
	WCD_DAPM_MUX("SLIM RX1 MUX", WCD934X_RX1, slim_rx1),
	WCD_DAPM_MUX("SLIM RX2 MUX", WCD934X_RX2, slim_rx2),
	WCD_DAPM_MUX("SLIM RX3 MUX", WCD934X_RX3, slim_rx3),
	WCD_DAPM_MUX("SLIM RX4 MUX", WCD934X_RX4, slim_rx4),
	WCD_DAPM_MUX("SLIM RX5 MUX", WCD934X_RX5, slim_rx5),
	WCD_DAPM_MUX("SLIM RX6 MUX", WCD934X_RX6, slim_rx6),
	WCD_DAPM_MUX("SLIM RX7 MUX", WCD934X_RX7, slim_rx7),

	SND_SOC_DAPM_MIXER("SLIM RX0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX6", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX7", SND_SOC_NOPM, 0, 0, NULL, 0),

	WCD_DAPM_MUX("CDC_IF RX0 MUX", WCD934X_RX0, cdc_if_rx0),
	WCD_DAPM_MUX("CDC_IF RX1 MUX", WCD934X_RX1, cdc_if_rx1),
	WCD_DAPM_MUX("CDC_IF RX2 MUX", WCD934X_RX2, cdc_if_rx2),
	WCD_DAPM_MUX("CDC_IF RX3 MUX", WCD934X_RX3, cdc_if_rx3),
	WCD_DAPM_MUX("CDC_IF RX4 MUX", WCD934X_RX4, cdc_if_rx4),
	WCD_DAPM_MUX("CDC_IF RX5 MUX", WCD934X_RX5, cdc_if_rx5),
	WCD_DAPM_MUX("CDC_IF RX6 MUX", WCD934X_RX6, cdc_if_rx6),
	WCD_DAPM_MUX("CDC_IF RX7 MUX", WCD934X_RX7, cdc_if_rx7),

	SND_SOC_DAPM_MUX_E("RX INT0_2 MUX", WCD934X_CDC_RX0_RX_PATH_MIX_CTL,
		5, 0, &rx_int0_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT1_2 MUX", WCD934X_CDC_RX1_RX_PATH_MIX_CTL,
		5, 0, &rx_int1_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT2_2 MUX", WCD934X_CDC_RX2_RX_PATH_MIX_CTL,
		5, 0, &rx_int2_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT3_2 MUX", WCD934X_CDC_RX3_RX_PATH_MIX_CTL,
		5, 0, &rx_int3_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT4_2 MUX", WCD934X_CDC_RX4_RX_PATH_MIX_CTL,
		5, 0, &rx_int4_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT7_2 MUX", WCD934X_CDC_RX7_RX_PATH_MIX_CTL,
		5, 0, &rx_int7_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT8_2 MUX", WCD934X_CDC_RX8_RX_PATH_MIX_CTL,
		5, 0, &rx_int8_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_POST_PMU),

	WCD_DAPM_MUX("RX INT0_1 MIX1 INP0", 0, rx_int0_1_mix_inp0),
	WCD_DAPM_MUX("RX INT0_1 MIX1 INP1", 0, rx_int0_1_mix_inp1),
	WCD_DAPM_MUX("RX INT0_1 MIX1 INP2", 0, rx_int0_1_mix_inp2),
	WCD_DAPM_MUX("RX INT1_1 MIX1 INP0", 0, rx_int1_1_mix_inp0),
	WCD_DAPM_MUX("RX INT1_1 MIX1 INP1", 0, rx_int1_1_mix_inp1),
	WCD_DAPM_MUX("RX INT1_1 MIX1 INP2", 0, rx_int1_1_mix_inp2),
	WCD_DAPM_MUX("RX INT2_1 MIX1 INP0", 0, rx_int2_1_mix_inp0),
	WCD_DAPM_MUX("RX INT2_1 MIX1 INP1", 0, rx_int2_1_mix_inp1),
	WCD_DAPM_MUX("RX INT2_1 MIX1 INP2", 0, rx_int2_1_mix_inp2),
	WCD_DAPM_MUX("RX INT3_1 MIX1 INP0", 0, rx_int3_1_mix_inp0),
	WCD_DAPM_MUX("RX INT3_1 MIX1 INP1", 0, rx_int3_1_mix_inp1),
	WCD_DAPM_MUX("RX INT3_1 MIX1 INP2", 0, rx_int3_1_mix_inp2),
	WCD_DAPM_MUX("RX INT4_1 MIX1 INP0", 0, rx_int4_1_mix_inp0),
	WCD_DAPM_MUX("RX INT4_1 MIX1 INP1", 0, rx_int4_1_mix_inp1),
	WCD_DAPM_MUX("RX INT4_1 MIX1 INP2", 0, rx_int4_1_mix_inp2),

	SND_SOC_DAPM_MUX_E("RX INT7_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int7_1_mix_inp0_mux, tavil_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int7_1_mix_inp1_mux, tavil_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int7_1_mix_inp2_mux, tavil_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int8_1_mix_inp0_mux, tavil_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int8_1_mix_inp1_mux, tavil_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int8_1_mix_inp2_mux, tavil_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("RX INT0_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT0 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT1_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT1 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT2_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT2 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT3_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT3 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT4_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT4 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT7_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT7 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT8_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT8 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("RX INT0 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT1 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT2 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT3 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT4 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT7 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("RX INT7 CHAIN", SND_SOC_NOPM, 0, 0,
		NULL, 0, tavil_codec_spk_boost_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX INT8 CHAIN", SND_SOC_NOPM, 0, 0,
		NULL, 0, tavil_codec_spk_boost_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("RX INT0 MIX2 INP", WCD934X_CDC_RX0_RX_PATH_CFG1, 4,
		0, &rx_int0_mix2_inp_mux),
	SND_SOC_DAPM_MUX("RX INT1 MIX2 INP", WCD934X_CDC_RX1_RX_PATH_CFG1, 4,
		0, &rx_int1_mix2_inp_mux),
	SND_SOC_DAPM_MUX("RX INT2 MIX2 INP", WCD934X_CDC_RX2_RX_PATH_CFG1, 4,
		0, &rx_int2_mix2_inp_mux),
	SND_SOC_DAPM_MUX("RX INT3 MIX2 INP", WCD934X_CDC_RX3_RX_PATH_CFG1, 4,
		0, &rx_int3_mix2_inp_mux),
	SND_SOC_DAPM_MUX("RX INT4 MIX2 INP", WCD934X_CDC_RX4_RX_PATH_CFG1, 4,
		0, &rx_int4_mix2_inp_mux),
	SND_SOC_DAPM_MUX("RX INT7 MIX2 INP", WCD934X_CDC_RX7_RX_PATH_CFG1, 4,
		0, &rx_int7_mix2_inp_mux),

	WCD_DAPM_MUX("CDC_IF TX0 MUX", WCD934X_TX0, cdc_if_tx0),
	WCD_DAPM_MUX("CDC_IF TX1 MUX", WCD934X_TX1, cdc_if_tx1),
	WCD_DAPM_MUX("CDC_IF TX2 MUX", WCD934X_TX2, cdc_if_tx2),
	WCD_DAPM_MUX("CDC_IF TX3 MUX", WCD934X_TX3, cdc_if_tx3),
	WCD_DAPM_MUX("CDC_IF TX4 MUX", WCD934X_TX4, cdc_if_tx4),
	WCD_DAPM_MUX("CDC_IF TX5 MUX", WCD934X_TX5, cdc_if_tx5),
	WCD_DAPM_MUX("CDC_IF TX6 MUX", WCD934X_TX6, cdc_if_tx6),
	WCD_DAPM_MUX("CDC_IF TX7 MUX", WCD934X_TX7, cdc_if_tx7),
	WCD_DAPM_MUX("CDC_IF TX8 MUX", WCD934X_TX8, cdc_if_tx8),
	WCD_DAPM_MUX("CDC_IF TX9 MUX", WCD934X_TX9, cdc_if_tx9),
	WCD_DAPM_MUX("CDC_IF TX10 MUX", WCD934X_TX10, cdc_if_tx10),
	WCD_DAPM_MUX("CDC_IF TX11 MUX", WCD934X_TX11, cdc_if_tx11),
	WCD_DAPM_MUX("CDC_IF TX11 INP1 MUX", WCD934X_TX11, cdc_if_tx11_inp1),
	WCD_DAPM_MUX("CDC_IF TX13 MUX", WCD934X_TX13, cdc_if_tx13),
	WCD_DAPM_MUX("CDC_IF TX13 INP1 MUX", WCD934X_TX13, cdc_if_tx13_inp1),

	SND_SOC_DAPM_MUX_E("ADC MUX0", WCD934X_CDC_TX0_TX_PATH_CTL, 5, 0,
		&tx_adc_mux0_mux, tavil_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX1", WCD934X_CDC_TX1_TX_PATH_CTL, 5, 0,
		&tx_adc_mux1_mux, tavil_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX2", WCD934X_CDC_TX2_TX_PATH_CTL, 5, 0,
		&tx_adc_mux2_mux, tavil_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX3", WCD934X_CDC_TX3_TX_PATH_CTL, 5, 0,
		&tx_adc_mux3_mux, tavil_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX4", WCD934X_CDC_TX4_TX_PATH_CTL, 5, 0,
		&tx_adc_mux4_mux, tavil_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX5", WCD934X_CDC_TX5_TX_PATH_CTL, 5, 0,
		&tx_adc_mux5_mux, tavil_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX6", WCD934X_CDC_TX6_TX_PATH_CTL, 5, 0,
		&tx_adc_mux6_mux, tavil_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX7", WCD934X_CDC_TX7_TX_PATH_CTL, 5, 0,
		&tx_adc_mux7_mux, tavil_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX8", WCD934X_CDC_TX8_TX_PATH_CTL, 5, 0,
		&tx_adc_mux8_mux, tavil_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX10", SND_SOC_NOPM, 10, 0, &tx_adc_mux10_mux,
		tavil_codec_tx_adc_cfg, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("ADC MUX11", SND_SOC_NOPM, 11, 0, &tx_adc_mux11_mux,
		tavil_codec_tx_adc_cfg, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("ADC MUX12", SND_SOC_NOPM, 12, 0, &tx_adc_mux12_mux,
		tavil_codec_tx_adc_cfg, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("ADC MUX13", SND_SOC_NOPM, 13, 0, &tx_adc_mux13_mux,
		tavil_codec_tx_adc_cfg, SND_SOC_DAPM_POST_PMU),

	WCD_DAPM_MUX("DMIC MUX0", 0, tx_dmic_mux0),
	WCD_DAPM_MUX("DMIC MUX1", 0, tx_dmic_mux1),
	WCD_DAPM_MUX("DMIC MUX2", 0, tx_dmic_mux2),
	WCD_DAPM_MUX("DMIC MUX3", 0, tx_dmic_mux3),
	WCD_DAPM_MUX("DMIC MUX4", 0, tx_dmic_mux4),
	WCD_DAPM_MUX("DMIC MUX5", 0, tx_dmic_mux5),
	WCD_DAPM_MUX("DMIC MUX6", 0, tx_dmic_mux6),
	WCD_DAPM_MUX("DMIC MUX7", 0, tx_dmic_mux7),
	WCD_DAPM_MUX("DMIC MUX8", 0, tx_dmic_mux8),
	WCD_DAPM_MUX("DMIC MUX10", 0, tx_dmic_mux10),
	WCD_DAPM_MUX("DMIC MUX11", 0, tx_dmic_mux11),
	WCD_DAPM_MUX("DMIC MUX12", 0, tx_dmic_mux12),
	WCD_DAPM_MUX("DMIC MUX13", 0, tx_dmic_mux13),

	WCD_DAPM_MUX("AMIC MUX0", 0, tx_amic_mux0),
	WCD_DAPM_MUX("AMIC MUX1", 0, tx_amic_mux1),
	WCD_DAPM_MUX("AMIC MUX2", 0, tx_amic_mux2),
	WCD_DAPM_MUX("AMIC MUX3", 0, tx_amic_mux3),
	WCD_DAPM_MUX("AMIC MUX4", 0, tx_amic_mux4),
	WCD_DAPM_MUX("AMIC MUX5", 0, tx_amic_mux5),
	WCD_DAPM_MUX("AMIC MUX6", 0, tx_amic_mux6),
	WCD_DAPM_MUX("AMIC MUX7", 0, tx_amic_mux7),
	WCD_DAPM_MUX("AMIC MUX8", 0, tx_amic_mux8),
	WCD_DAPM_MUX("AMIC MUX10", 0, tx_amic_mux10),
	WCD_DAPM_MUX("AMIC MUX11", 0, tx_amic_mux11),
	WCD_DAPM_MUX("AMIC MUX12", 0, tx_amic_mux12),
	WCD_DAPM_MUX("AMIC MUX13", 0, tx_amic_mux13),

	SND_SOC_DAPM_ADC_E("ADC1", NULL, WCD934X_ANA_AMIC1, 7, 0,
		tavil_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("ADC2", NULL, WCD934X_ANA_AMIC2, 7, 0,
		tavil_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("ADC3", NULL, WCD934X_ANA_AMIC3, 7, 0,
		tavil_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("ADC4", NULL, WCD934X_ANA_AMIC4, 7, 0,
		tavil_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),

	WCD_DAPM_MUX("AMIC4_5 SEL", 0, tx_amic4_5),

	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_INPUT("AMIC3"),
	SND_SOC_DAPM_INPUT("AMIC4"),
	SND_SOC_DAPM_INPUT("AMIC5"),

	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1", SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2", SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3", SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS4", SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS1_STANDALONE, SND_SOC_NOPM, 0, 0,
		tavil_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS2_STANDALONE, SND_SOC_NOPM, 0, 0,
		tavil_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS3_STANDALONE, SND_SOC_NOPM, 0, 0,
		tavil_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS4_STANDALONE, SND_SOC_NOPM, 0, 0,
		tavil_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AIF1 CAP", "AIF1 Capture", 0, SND_SOC_NOPM,
		AIF1_CAP, 0, tavil_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("AIF2 CAP", "AIF2 Capture", 0, SND_SOC_NOPM,
		AIF2_CAP, 0, tavil_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("AIF3 CAP", "AIF3 Capture", 0, SND_SOC_NOPM,
		AIF3_CAP, 0, tavil_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("AIF1_CAP Mixer", SND_SOC_NOPM, AIF1_CAP, 0,
		aif1_cap_mixer, ARRAY_SIZE(aif1_cap_mixer)),
	SND_SOC_DAPM_MIXER("AIF2_CAP Mixer", SND_SOC_NOPM, AIF2_CAP, 0,
		aif2_cap_mixer, ARRAY_SIZE(aif2_cap_mixer)),
	SND_SOC_DAPM_MIXER("AIF3_CAP Mixer", SND_SOC_NOPM, AIF3_CAP, 0,
		aif3_cap_mixer, ARRAY_SIZE(aif3_cap_mixer)),

	SND_SOC_DAPM_MIXER("SLIM TX0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX6", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX7", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX8", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX9", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX10", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX11", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM TX13", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Digital Mic Inputs */
	SND_SOC_DAPM_ADC_E("DMIC0", NULL, SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC3", NULL, SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC4", NULL, SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC5", NULL, SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	WCD_DAPM_MUX("IIR0 INP0 MUX", 0, iir0_inp0),
	WCD_DAPM_MUX("IIR0 INP1 MUX", 0, iir0_inp1),
	WCD_DAPM_MUX("IIR0 INP2 MUX", 0, iir0_inp2),
	WCD_DAPM_MUX("IIR0 INP3 MUX", 0, iir0_inp3),
	WCD_DAPM_MUX("IIR1 INP0 MUX", 0, iir1_inp0),
	WCD_DAPM_MUX("IIR1 INP1 MUX", 0, iir1_inp1),
	WCD_DAPM_MUX("IIR1 INP2 MUX", 0, iir1_inp2),
	WCD_DAPM_MUX("IIR1 INP3 MUX", 0, iir1_inp3),

	SND_SOC_DAPM_MIXER_E("IIR0", WCD934X_CDC_SIDETONE_IIR0_IIR_PATH_CTL,
		4, 0, NULL, 0, tavil_codec_set_iir_gain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("IIR1", WCD934X_CDC_SIDETONE_IIR1_IIR_PATH_CTL,
		4, 0, NULL, 0, tavil_codec_set_iir_gain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER("SRC0", WCD934X_CDC_SIDETONE_SRC0_ST_SRC_PATH_CTL,
		4, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SRC1", WCD934X_CDC_SIDETONE_SRC1_ST_SRC_PATH_CTL,
		4, 0, NULL, 0),

	WCD_DAPM_MUX("RX MIX TX0 MUX", 0, rx_mix_tx0),
	WCD_DAPM_MUX("RX MIX TX1 MUX", 0, rx_mix_tx1),
	WCD_DAPM_MUX("RX MIX TX2 MUX", 0, rx_mix_tx2),
	WCD_DAPM_MUX("RX MIX TX3 MUX", 0, rx_mix_tx3),
	WCD_DAPM_MUX("RX MIX TX4 MUX", 0, rx_mix_tx4),
	WCD_DAPM_MUX("RX MIX TX5 MUX", 0, rx_mix_tx5),
	WCD_DAPM_MUX("RX MIX TX6 MUX", 0, rx_mix_tx6),
	WCD_DAPM_MUX("RX MIX TX7 MUX", 0, rx_mix_tx7),
	WCD_DAPM_MUX("RX MIX TX8 MUX", 0, rx_mix_tx8),
	WCD_DAPM_MUX("RX INT0 DEM MUX", 0, rx_int0_dem_inp),
	WCD_DAPM_MUX("RX INT1 DEM MUX", 0, rx_int1_dem_inp),
	WCD_DAPM_MUX("RX INT2 DEM MUX", 0, rx_int2_dem_inp),

	SND_SOC_DAPM_MUX_E("RX INT0 INTERP", SND_SOC_NOPM, INTERP_EAR, 0,
		&rx_int0_interp_mux, tavil_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT1 INTERP", SND_SOC_NOPM, INTERP_HPHL, 0,
		&rx_int1_interp_mux, tavil_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT2 INTERP", SND_SOC_NOPM, INTERP_HPHR, 0,
		&rx_int2_interp_mux, tavil_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT3 INTERP", SND_SOC_NOPM, INTERP_LO1, 0,
		&rx_int3_interp_mux, tavil_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT4 INTERP", SND_SOC_NOPM, INTERP_LO2, 0,
		&rx_int4_interp_mux, tavil_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7 INTERP", SND_SOC_NOPM, INTERP_SPKR1, 0,
		&rx_int7_interp_mux, tavil_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8 INTERP", SND_SOC_NOPM, INTERP_SPKR2, 0,
		&rx_int8_interp_mux, tavil_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SWITCH("ADC US MUX0", WCD934X_CDC_TX0_TX_PATH_192_CTL, 0,
		0, &adc_us_mux0_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX1", WCD934X_CDC_TX1_TX_PATH_192_CTL, 0,
		0, &adc_us_mux1_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX2", WCD934X_CDC_TX2_TX_PATH_192_CTL, 0,
		0, &adc_us_mux2_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX3", WCD934X_CDC_TX3_TX_PATH_192_CTL, 0,
		0, &adc_us_mux3_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX4", WCD934X_CDC_TX4_TX_PATH_192_CTL, 0,
		0, &adc_us_mux4_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX5", WCD934X_CDC_TX5_TX_PATH_192_CTL, 0,
		0, &adc_us_mux5_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX6", WCD934X_CDC_TX6_TX_PATH_192_CTL, 0,
		0, &adc_us_mux6_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX7", WCD934X_CDC_TX7_TX_PATH_192_CTL, 0,
		0, &adc_us_mux7_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX8", WCD934X_CDC_TX8_TX_PATH_192_CTL, 0,
		0, &adc_us_mux8_switch),

	SND_SOC_DAPM_DAC_E("RX INT0 DAC", NULL, SND_SOC_NOPM,
		0, 0, tavil_codec_ear_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RX INT1 DAC", NULL, WCD934X_ANA_HPH,
		5, 0, tavil_codec_hphl_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RX INT2 DAC", NULL, WCD934X_ANA_HPH,
		4, 0, tavil_codec_hphr_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RX INT3 DAC", NULL, SND_SOC_NOPM,
		0, 0, tavil_codec_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RX INT4 DAC", NULL, SND_SOC_NOPM,
		0, 0, tavil_codec_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("EAR PA", WCD934X_ANA_EAR, 7, 0, NULL, 0,
		tavil_codec_enable_ear_pa,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("HPHL PA", WCD934X_ANA_HPH, 7, 0, NULL, 0,
		tavil_codec_enable_hphl_pa,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("HPHR PA", WCD934X_ANA_HPH, 6, 0, NULL, 0,
		tavil_codec_enable_hphr_pa,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT1 PA", WCD934X_ANA_LO_1_2, 7, 0, NULL, 0,
		tavil_codec_enable_lineout_pa,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT2 PA", WCD934X_ANA_LO_1_2, 6, 0, NULL, 0,
		tavil_codec_enable_lineout_pa,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("EAR"),
	SND_SOC_DAPM_OUTPUT("HPHL"),
	SND_SOC_DAPM_OUTPUT("HPHR"),
	SND_SOC_DAPM_OUTPUT("LINEOUT1"),
	SND_SOC_DAPM_OUTPUT("LINEOUT2"),
	SND_SOC_DAPM_OUTPUT("SPK1 OUT"),
	SND_SOC_DAPM_OUTPUT("SPK2 OUT"),

	SND_SOC_DAPM_SUPPLY("RX_BIAS", SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_rx_bias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static int tavil_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(dai->codec);
	u32 i = 0;
	struct wcd9xxx_ch *ch;
	int ret = 0;

	switch (dai->id) {
	case AIF1_PB:
	case AIF2_PB:
	case AIF3_PB:
	case AIF4_PB:
		if (!rx_slot || !rx_num) {
			dev_err(tavil->dev, "%s: Invalid rx_slot 0x%pK or rx_num 0x%pK\n",
				 __func__, rx_slot, rx_num);
			ret = -EINVAL;
			break;
		}
		list_for_each_entry(ch, &tavil->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			dev_dbg(tavil->dev, "%s: slot_num %u ch->ch_num %d\n",
				 __func__, i, ch->ch_num);
			rx_slot[i++] = ch->ch_num;
		}
		*rx_num = i;
		dev_dbg(tavil->dev, "%s: dai_name = %s dai_id = %x  rx_num = %d\n",
			__func__, dai->name, dai->id, i);
		if (*rx_num == 0) {
			dev_err(tavil->dev, "%s: Channel list empty for dai_name = %s dai_id = %x\n",
				__func__, dai->name, dai->id);
			ret = -EINVAL;
		}
		break;
	case AIF1_CAP:
	case AIF2_CAP:
	case AIF3_CAP:
		if (!tx_slot || !tx_num) {
			dev_err(tavil->dev, "%s: Invalid tx_slot 0x%pK or tx_num 0x%pK\n",
				 __func__, tx_slot, tx_num);
			ret = -EINVAL;
			break;
		}
		list_for_each_entry(ch, &tavil->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			dev_dbg(tavil->dev, "%s: slot_num %u ch->ch_num %d\n",
				 __func__, i,  ch->ch_num);
			tx_slot[i++] = ch->ch_num;
		}
		*tx_num = i;
		dev_dbg(tavil->dev, "%s: dai_name = %s dai_id = %x  tx_num = %d\n",
			 __func__, dai->name, dai->id, i);
		if (*tx_num == 0) {
			dev_err(tavil->dev, "%s: Channel list empty for dai_name = %s dai_id = %x\n",
				 __func__, dai->name, dai->id);
			ret = -EINVAL;
		}
		break;
	default:
		dev_err(tavil->dev, "%s: Invalid DAI ID %x\n",
			__func__, dai->id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int tavil_set_channel_map(struct snd_soc_dai *dai,
				 unsigned int tx_num, unsigned int *tx_slot,
				 unsigned int rx_num, unsigned int *rx_slot)
{
	struct tavil_priv *tavil;
	struct wcd9xxx *core;

	tavil = snd_soc_codec_get_drvdata(dai->codec);
	core = dev_get_drvdata(dai->codec->dev->parent);

	if (!tx_slot || !rx_slot) {
		dev_err(tavil->dev, "%s: Invalid tx_slot 0x%pK, rx_slot 0x%pK\n",
			__func__, tx_slot, rx_slot);
		return -EINVAL;
	}
	dev_dbg(tavil->dev, "%s(): dai_name = %s DAI-ID %x tx_ch %d rx_ch %d\n",
		 __func__, dai->name, dai->id, tx_num, rx_num);

	wcd9xxx_init_slimslave(core, core->slim->laddr,
				tx_num, tx_slot, rx_num, rx_slot);
	return 0;
}

static int tavil_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	return 0;
}

static void tavil_shutdown(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
}

static int tavil_set_decimator_rate(struct snd_soc_dai *dai,
				    u32 sample_rate)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wcd9xxx_ch *ch;
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	u32 tx_port = 0, tx_fs_rate = 0;
	u8 shift = 0, shift_val = 0, tx_mux_sel = 0;
	int decimator = -1;
	u16 tx_port_reg = 0, tx_fs_reg = 0;

	switch (sample_rate) {
	case 8000:
		tx_fs_rate = 0;
		break;
	case 16000:
		tx_fs_rate = 1;
		break;
	case 32000:
		tx_fs_rate = 3;
		break;
	case 48000:
		tx_fs_rate = 4;
		break;
	case 96000:
		tx_fs_rate = 5;
		break;
	case 192000:
		tx_fs_rate = 6;
		break;
	default:
		dev_err(tavil->dev, "%s: Invalid TX sample rate: %d\n",
			__func__, sample_rate);
		return -EINVAL;

	};

	list_for_each_entry(ch, &tavil->dai[dai->id].wcd9xxx_ch_list, list) {
		tx_port = ch->port;
		dev_dbg(codec->dev, "%s: dai->id = %d, tx_port = %d",
			__func__, dai->id, tx_port);

		if ((tx_port < 0) || (tx_port == 12) || (tx_port >= 14)) {
			dev_err(codec->dev, "%s: Invalid SLIM TX%u port. DAI ID: %d\n",
				__func__, tx_port, dai->id);
			return -EINVAL;
		}
		/* Find the SB TX MUX input - which decimator is connected */
		if (tx_port < 4) {
			tx_port_reg = WCD934X_CDC_IF_ROUTER_TX_MUX_CFG0;
			shift = (tx_port << 1);
			shift_val = 0x03;
		} else if ((tx_port >= 4) && (tx_port < 8)) {
			tx_port_reg = WCD934X_CDC_IF_ROUTER_TX_MUX_CFG1;
			shift = ((tx_port - 4) << 1);
			shift_val = 0x03;
		} else if ((tx_port >= 8) && (tx_port < 11)) {
			tx_port_reg = WCD934X_CDC_IF_ROUTER_TX_MUX_CFG2;
			shift = ((tx_port - 8) << 1);
			shift_val = 0x03;
		} else if (tx_port == 11) {
			tx_port_reg = WCD934X_CDC_IF_ROUTER_TX_MUX_CFG3;
			shift = 0;
			shift_val = 0x0F;
		} else if (tx_port == 13) {
			tx_port_reg = WCD934X_CDC_IF_ROUTER_TX_MUX_CFG3;
			shift = 4;
			shift_val = 0x03;
		}
		tx_mux_sel = snd_soc_read(codec, tx_port_reg) &
					  (shift_val << shift);
		tx_mux_sel = tx_mux_sel >> shift;

		if (tx_port <= 8) {
			if ((tx_mux_sel == 0x2) || (tx_mux_sel == 0x3))
				decimator = tx_port;
		} else if (tx_port <= 10) {
			if ((tx_mux_sel == 0x1) || (tx_mux_sel == 0x2))
				decimator = ((tx_port == 9) ? 7 : 6);
		} else if (tx_port == 11) {
			if ((tx_mux_sel >= 1) && (tx_mux_sel < 7))
				decimator = tx_mux_sel - 1;
		} else if (tx_port == 13) {
			if ((tx_mux_sel == 0x1) || (tx_mux_sel == 0x2))
				decimator = 5;
		}

		if (decimator >= 0) {
			tx_fs_reg = WCD934X_CDC_TX0_TX_PATH_CTL +
				    16 * decimator;
			dev_dbg(codec->dev, "%s: set DEC%u (-> SLIM_TX%u) rate to %u\n",
				__func__, decimator, tx_port, sample_rate);
			snd_soc_update_bits(codec, tx_fs_reg, 0x0F, tx_fs_rate);
		} else if ((tx_port <= 8) && (tx_mux_sel == 0x01)) {
			/* Check if the TX Mux input is RX MIX TXn */
			dev_dbg(codec->dev, "%s: RX_MIX_TX%u going to CDC_IF TX%u\n",
					__func__, tx_port, tx_port);
		} else {
			dev_err(codec->dev, "%s: ERROR: Invalid decimator: %d\n",
				__func__, decimator);
			return -EINVAL;
		}
	}
	return 0;
}

static int tavil_set_mix_interpolator_rate(struct snd_soc_dai *dai,
					   u8 rate_reg_val,
					   u32 sample_rate)
{
	u8 int_2_inp;
	u32 j;
	u16 int_mux_cfg1, int_fs_reg;
	u8 int_mux_cfg1_val;
	struct snd_soc_codec *codec = dai->codec;
	struct wcd9xxx_ch *ch;
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	list_for_each_entry(ch, &tavil->dai[dai->id].wcd9xxx_ch_list, list) {
		int_2_inp = INTn_2_INP_SEL_RX0 + ch->port -
						WCD934X_RX_PORT_START_NUMBER;
		if ((int_2_inp < INTn_2_INP_SEL_RX0) ||
		    (int_2_inp > INTn_2_INP_SEL_RX7)) {
			dev_err(codec->dev, "%s: Invalid RX%u port, Dai ID is %d\n",
				__func__,
				(ch->port - WCD934X_RX_PORT_START_NUMBER),
				dai->id);
			return -EINVAL;
		}

		int_mux_cfg1 = WCD934X_CDC_RX_INP_MUX_RX_INT0_CFG1;
		for (j = 0; j < WCD934X_NUM_INTERPOLATORS; j++) {
			/* Interpolators 5 and 6 are not aviliable in Tavil */
			if (j == INTERP_LO3_NA || j == INTERP_LO4_NA) {
				int_mux_cfg1 += 2;
				continue;
			}
			int_mux_cfg1_val = snd_soc_read(codec, int_mux_cfg1) &
									0x0F;
			if (int_mux_cfg1_val == int_2_inp) {
				/*
				 * Ear mix path supports only 48, 96, 192,
				 * 384KHz only
				 */
				if ((j == INTERP_EAR) &&
				    (rate_reg_val < 0x4 ||
				     rate_reg_val > 0x7)) {
					dev_err_ratelimited(codec->dev,
					"%s: Invalid rate for AIF_PB DAI(%d)\n",
					  __func__, dai->id);
					return -EINVAL;
				}

				int_fs_reg = WCD934X_CDC_RX0_RX_PATH_MIX_CTL +
									20 * j;
				dev_dbg(codec->dev, "%s: AIF_PB DAI(%d) connected to INT%u_2\n",
					  __func__, dai->id, j);
				dev_dbg(codec->dev, "%s: set INT%u_2 sample rate to %u\n",
					__func__, j, sample_rate);
				snd_soc_update_bits(codec, int_fs_reg, 0x0F,
						    rate_reg_val);
			}
			int_mux_cfg1 += 2;
		}
	}
	return 0;
}

static int tavil_set_prim_interpolator_rate(struct snd_soc_dai *dai,
					    u8 rate_reg_val,
					    u32 sample_rate)
{
	u8 int_1_mix1_inp;
	u32 j;
	u16 int_mux_cfg0, int_mux_cfg1;
	u16 int_fs_reg;
	u8 int_mux_cfg0_val, int_mux_cfg1_val;
	u8 inp0_sel, inp1_sel, inp2_sel;
	struct snd_soc_codec *codec = dai->codec;
	struct wcd9xxx_ch *ch;
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	list_for_each_entry(ch, &tavil->dai[dai->id].wcd9xxx_ch_list, list) {
		int_1_mix1_inp = INTn_1_INP_SEL_RX0 + ch->port -
						WCD934X_RX_PORT_START_NUMBER;
		if ((int_1_mix1_inp < INTn_1_INP_SEL_RX0) ||
		    (int_1_mix1_inp > INTn_1_INP_SEL_RX7)) {
			dev_err(codec->dev, "%s: Invalid RX%u port, Dai ID is %d\n",
				__func__,
				(ch->port - WCD934X_RX_PORT_START_NUMBER),
				dai->id);
			return -EINVAL;
		}

		int_mux_cfg0 = WCD934X_CDC_RX_INP_MUX_RX_INT0_CFG0;

		/*
		 * Loop through all interpolator MUX inputs and find out
		 * to which interpolator input, the slim rx port
		 * is connected
		 */
		for (j = 0; j < WCD934X_NUM_INTERPOLATORS; j++) {
			/* Interpolators 5 and 6 are not aviliable in Tavil */
			if (j == INTERP_LO3_NA || j == INTERP_LO4_NA) {
				int_mux_cfg0 += 2;
				continue;
			}
			int_mux_cfg1 = int_mux_cfg0 + 1;

			int_mux_cfg0_val = snd_soc_read(codec, int_mux_cfg0);
			int_mux_cfg1_val = snd_soc_read(codec, int_mux_cfg1);
			inp0_sel = int_mux_cfg0_val & 0x0F;
			inp1_sel = (int_mux_cfg0_val >> 4) & 0x0F;
			inp2_sel = (int_mux_cfg1_val >> 4) & 0x0F;
			if ((inp0_sel == int_1_mix1_inp) ||
			    (inp1_sel == int_1_mix1_inp) ||
			    (inp2_sel == int_1_mix1_inp)) {
				/*
				 * Ear and speaker primary path does not support
				 * native sample rates
				 */
				if ((j == INTERP_EAR || j == INTERP_SPKR1 ||
					j == INTERP_SPKR2) &&
					(rate_reg_val > 0x7)) {
					dev_err_ratelimited(codec->dev,
					"%s: Invalid rate for AIF_PB DAI(%d)\n",
					  __func__, dai->id);
					return -EINVAL;
				}

				int_fs_reg = WCD934X_CDC_RX0_RX_PATH_CTL +
									20 * j;
				dev_dbg(codec->dev,
				"%s: AIF_PB DAI(%d) connected to INT%u_1\n",
				  __func__, dai->id, j);
				dev_dbg(codec->dev,
					"%s: set INT%u_1 sample rate to %u\n",
					__func__, j, sample_rate);
				snd_soc_update_bits(codec, int_fs_reg, 0x0F,
						    rate_reg_val);
			}
			int_mux_cfg0 += 2;
		}
	}

	return 0;
}


static int tavil_set_interpolator_rate(struct snd_soc_dai *dai,
				       u32 sample_rate)
{
	struct snd_soc_codec *codec = dai->codec;
	int rate_val = 0;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(sr_val_tbl); i++) {
		if (sample_rate == sr_val_tbl[i].sample_rate) {
			rate_val = sr_val_tbl[i].rate_val;
			break;
		}
	}
	if ((i == ARRAY_SIZE(sr_val_tbl)) || (rate_val < 0)) {
		dev_err(codec->dev, "%s: Unsupported sample rate: %d\n",
			__func__, sample_rate);
		return -EINVAL;
	}

	ret = tavil_set_prim_interpolator_rate(dai, (u8)rate_val, sample_rate);
	if (ret)
		return ret;
	ret = tavil_set_mix_interpolator_rate(dai, (u8)rate_val, sample_rate);
	if (ret)
		return ret;

	return ret;
}

static int tavil_prepare(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return 0;
}

static int tavil_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(dai->codec);
	int ret;

	dev_dbg(tavil->dev, "%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n",
		 __func__, dai->name, dai->id, params_rate(params),
		 params_channels(params));

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = tavil_set_interpolator_rate(dai, params_rate(params));
		if (ret) {
			dev_err(tavil->dev, "%s: cannot set sample rate: %u\n",
				__func__, params_rate(params));
			return ret;
		}
		switch (params_width(params)) {
		case 16:
			tavil->dai[dai->id].bit_width = 16;
			break;
		case 24:
			tavil->dai[dai->id].bit_width = 24;
			break;
		case 32:
			tavil->dai[dai->id].bit_width = 32;
			break;
		default:
			return -EINVAL;
		}
		tavil->dai[dai->id].rate = params_rate(params);
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		ret = tavil_set_decimator_rate(dai, params_rate(params));
		if (ret) {
			dev_err(tavil->dev, "%s: cannot set TX Decimator rate: %d\n",
				__func__, ret);
			return ret;
		}
		switch (params_width(params)) {
		case 16:
			tavil->dai[dai->id].bit_width = 16;
			break;
		case 24:
			tavil->dai[dai->id].bit_width = 24;
			break;
		default:
			dev_err(tavil->dev, "%s: Invalid format 0x%x\n",
				__func__, params_width(params));
			return -EINVAL;
		};
		tavil->dai[dai->id].rate = params_rate(params);
		break;
	default:
		dev_err(tavil->dev, "%s: Invalid stream type %d\n", __func__,
			substream->stream);
		return -EINVAL;
	};

	return 0;
}

static struct snd_soc_dai_ops tavil_dai_ops = {
	.startup = tavil_startup,
	.shutdown = tavil_shutdown,
	.hw_params = tavil_hw_params,
	.prepare = tavil_prepare,
	.set_channel_map = tavil_set_channel_map,
	.get_channel_map = tavil_get_channel_map,
};

static struct snd_soc_dai_driver tavil_dai[] = {
	{
		.name = "tavil_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD934X_RATES_MASK | WCD934X_FRAC_RATES_MASK,
			.formats = WCD934X_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 384000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tavil_dai_ops,
	},
	{
		.name = "tavil_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD934X_RATES_MASK,
			.formats = WCD934X_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tavil_dai_ops,
	},
	{
		.name = "tavil_rx2",
		.id = AIF2_PB,
		.playback = {
			.stream_name = "AIF2 Playback",
			.rates = WCD934X_RATES_MASK | WCD934X_FRAC_RATES_MASK,
			.formats = WCD934X_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 384000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tavil_dai_ops,
	},
	{
		.name = "tavil_tx2",
		.id = AIF2_CAP,
		.capture = {
			.stream_name = "AIF2 Capture",
			.rates = WCD934X_RATES_MASK,
			.formats = WCD934X_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tavil_dai_ops,
	},
	{
		.name = "tavil_rx3",
		.id = AIF3_PB,
		.playback = {
			.stream_name = "AIF3 Playback",
			.rates = WCD934X_RATES_MASK | WCD934X_FRAC_RATES_MASK,
			.formats = WCD934X_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 384000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tavil_dai_ops,
	},
	{
		.name = "tavil_tx3",
		.id = AIF3_CAP,
		.capture = {
			.stream_name = "AIF3 Capture",
			.rates = WCD934X_RATES_MASK,
			.formats = WCD934X_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tavil_dai_ops,
	},
	{
		.name = "tavil_rx4",
		.id = AIF4_PB,
		.playback = {
			.stream_name = "AIF4 Playback",
			.rates = WCD934X_RATES_MASK | WCD934X_FRAC_RATES_MASK,
			.formats = WCD934X_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 384000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tavil_dai_ops,
	},
};

static int tavil_cdc_req_mclk_enable(struct tavil_priv *tavil,
				     bool enable)
{
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(tavil->wcd_ext_clk);
		if (ret) {
			dev_err(tavil->dev, "%s: ext clk enable failed\n",
				__func__);
			goto done;
		}
		/* get BG */
		wcd_resmgr_enable_master_bias(tavil->resmgr);
		/* get MCLK */
		wcd_resmgr_enable_clk_block(tavil->resmgr, WCD_CLK_MCLK);
	} else {
		/* put MCLK */
		wcd_resmgr_disable_clk_block(tavil->resmgr, WCD_CLK_MCLK);
		/* put BG */
		wcd_resmgr_disable_master_bias(tavil->resmgr);
		clk_disable_unprepare(tavil->wcd_ext_clk);
	}

done:
	return ret;
}

static int __tavil_cdc_mclk_enable_locked(struct tavil_priv *tavil,
					  bool enable)
{
	int ret = 0;

	if (!tavil->wcd_ext_clk) {
		dev_err(tavil->dev, "%s: wcd ext clock is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(tavil->dev, "%s: mclk_enable = %u\n", __func__, enable);

	if (enable) {
		ret = tavil_cdc_req_mclk_enable(tavil, true);
		if (ret)
			goto done;

		set_bit(AUDIO_NOMINAL, &tavil->status_mask);
	} else {
		tavil_cdc_req_mclk_enable(tavil, false);
	}

done:
	return ret;
}

static int __tavil_cdc_mclk_enable(struct tavil_priv *tavil,
				   bool enable)
{
	int ret;

	WCD9XXX_V2_BG_CLK_LOCK(tavil->resmgr);
	ret = __tavil_cdc_mclk_enable_locked(tavil, enable);
	WCD9XXX_V2_BG_CLK_UNLOCK(tavil->resmgr);

	return ret;
}

/**
 * tavil_cdc_mclk_enable - Enable/disable codec mclk
 *
 * @codec: codec instance
 * @enable: Indicates clk enable or disable
 *
 * Returns 0 on Success and error on failure
 */
int tavil_cdc_mclk_enable(struct snd_soc_codec *codec, bool enable)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	return __tavil_cdc_mclk_enable(tavil, enable);
}
EXPORT_SYMBOL(tavil_cdc_mclk_enable);

static const struct tavil_reg_mask_val tavil_codec_reg_defaults[] = {
	{WCD934X_BIAS_VBG_FINE_ADJ, 0xFF, 0x75},
	{WCD934X_CODEC_CPR_SVS_CX_VDD, 0xFF, 0x7C}, /* value in svs mode */
	{WCD934X_CODEC_CPR_SVS2_CX_VDD, 0xFF, 0x58}, /* value in svs2 mode */
	{WCD934X_SIDO_NEW_VOUT_D_FREQ2, 0x01, 0x01},
	{WCD934X_CDC_RX0_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_RX1_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_RX2_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_RX7_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_RX8_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_COMPANDER8_CTL7, 0x1E, 0x18},
	{WCD934X_CDC_COMPANDER7_CTL7, 0x1E, 0x18},
	{WCD934X_CDC_RX0_RX_PATH_SEC0, 0x08, 0x0},
	{WCD934X_CDC_CLSH_DECAY_CTRL, 0x03, 0x0},
	{WCD934X_MICB1_TEST_CTL_2, 0x07, 0x01},
};

static const struct tavil_reg_mask_val tavil_codec_reg_init_common_val[] = {
	{WCD934X_CDC_CLSH_K2_MSB, 0x0F, 0x00},
	{WCD934X_CDC_CLSH_K2_LSB, 0xFF, 0x60},
	{WCD934X_CPE_SS_DMIC_CFG, 0x80, 0x00},
	{WCD934X_CDC_BOOST0_BOOST_CTL, 0x70, 0x40},
	{WCD934X_CDC_BOOST1_BOOST_CTL, 0x70, 0x40},
	{WCD934X_CDC_RX7_RX_PATH_CFG1, 0x08, 0x08},
	{WCD934X_CDC_RX8_RX_PATH_CFG1, 0x08, 0x08},
	{WCD934X_CDC_TOP_TOP_CFG1, 0x02, 0x02},
	{WCD934X_CDC_TOP_TOP_CFG1, 0x01, 0x01},
	{WCD934X_CDC_TX9_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD934X_CDC_TX10_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD934X_CDC_TX11_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD934X_CDC_TX12_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD934X_CDC_RX0_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD934X_CDC_RX1_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD934X_CDC_RX2_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD934X_CDC_RX3_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD934X_CDC_RX4_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD934X_CDC_RX7_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD934X_CDC_RX8_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD934X_DATA_HUB_SB_TX11_INP_CFG, 0x01, 0x01},
	{WCD934X_CDC_CLK_RST_CTRL_FS_CNT_CONTROL, 0x01, 0x01},
};

static void tavil_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(tavil_codec_reg_init_common_val); i++)
		snd_soc_update_bits(codec,
				    tavil_codec_reg_init_common_val[i].reg,
				    tavil_codec_reg_init_common_val[i].mask,
				    tavil_codec_reg_init_common_val[i].val);
}

static void tavil_update_reg_defaults(struct tavil_priv *tavil)
{
	u32 i;
	struct wcd9xxx *wcd9xxx;

	wcd9xxx = tavil->wcd9xxx;
	for (i = 0; i < ARRAY_SIZE(tavil_codec_reg_defaults); i++)
		regmap_update_bits(wcd9xxx->regmap,
				   tavil_codec_reg_defaults[i].reg,
				   tavil_codec_reg_defaults[i].mask,
				   tavil_codec_reg_defaults[i].val);
}

static void tavil_slim_interface_init_reg(struct snd_soc_codec *codec)
{
	int i;
	struct tavil_priv *priv = snd_soc_codec_get_drvdata(codec);

	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++)
		wcd9xxx_interface_reg_write(priv->wcd9xxx,
				    WCD934X_SLIM_PGD_PORT_INT_RX_EN0 + i,
				    0xFF);
}

static irqreturn_t tavil_slimbus_irq(int irq, void *data)
{
	struct tavil_priv *tavil = data;
	unsigned long status = 0;
	int i, j, port_id, k;
	u32 bit;
	u8 val, int_val = 0;
	bool tx, cleared;
	unsigned short reg = 0;

	for (i = WCD934X_SLIM_PGD_PORT_INT_STATUS_RX_0, j = 0;
	     i <= WCD934X_SLIM_PGD_PORT_INT_STATUS_TX_1; i++, j++) {
		val = wcd9xxx_interface_reg_read(tavil->wcd9xxx, i);
		status |= ((u32)val << (8 * j));
	}

	for_each_set_bit(j, &status, 32) {
		tx = (j >= 16 ? true : false);
		port_id = (tx ? j - 16 : j);
		val = wcd9xxx_interface_reg_read(tavil->wcd9xxx,
				WCD934X_SLIM_PGD_PORT_INT_RX_SOURCE0 + j);
		if (val) {
			if (!tx)
				reg = WCD934X_SLIM_PGD_PORT_INT_RX_EN0 +
					(port_id / 8);
			else
				reg = WCD934X_SLIM_PGD_PORT_INT_TX_EN0 +
					(port_id / 8);
			int_val = wcd9xxx_interface_reg_read(
				tavil->wcd9xxx, reg);
			/*
			 * Ignore interrupts for ports for which the
			 * interrupts are not specifically enabled.
			 */
			if (!(int_val & (1 << (port_id % 8))))
				continue;
		}
		if (val & WCD934X_SLIM_IRQ_OVERFLOW)
			dev_err_ratelimited(tavil->dev, "%s: overflow error on %s port %d, value %x\n",
			   __func__, (tx ? "TX" : "RX"), port_id, val);
		if (val & WCD934X_SLIM_IRQ_UNDERFLOW)
			dev_err_ratelimited(tavil->dev, "%s: underflow error on %s port %d, value %x\n",
			   __func__, (tx ? "TX" : "RX"), port_id, val);
		if ((val & WCD934X_SLIM_IRQ_OVERFLOW) ||
			(val & WCD934X_SLIM_IRQ_UNDERFLOW)) {
			if (!tx)
				reg = WCD934X_SLIM_PGD_PORT_INT_RX_EN0 +
					(port_id / 8);
			else
				reg = WCD934X_SLIM_PGD_PORT_INT_TX_EN0 +
					(port_id / 8);
			int_val = wcd9xxx_interface_reg_read(
				tavil->wcd9xxx, reg);
			if (int_val & (1 << (port_id % 8))) {
				int_val = int_val ^ (1 << (port_id % 8));
				wcd9xxx_interface_reg_write(tavil->wcd9xxx,
					reg, int_val);
			}
		}
		if (val & WCD934X_SLIM_IRQ_PORT_CLOSED) {
			/*
			 * INT SOURCE register starts from RX to TX
			 * but port number in the ch_mask is in opposite way
			 */
			bit = (tx ? j - 16 : j + 16);
			dev_dbg(tavil->dev, "%s: %s port %d closed value %x, bit %u\n",
				 __func__, (tx ? "TX" : "RX"), port_id, val,
				 bit);
			for (k = 0, cleared = false; k < NUM_CODEC_DAIS; k++) {
				dev_dbg(tavil->dev, "%s: tavil->dai[%d].ch_mask = 0x%lx\n",
					 __func__, k, tavil->dai[k].ch_mask);
				if (test_and_clear_bit(bit,
						&tavil->dai[k].ch_mask)) {
					cleared = true;
					if (!tavil->dai[k].ch_mask)
						wake_up(
						      &tavil->dai[k].dai_wait);
					/*
					 * There are cases when multiple DAIs
					 * might be using the same slimbus
					 * channel. Hence don't break here.
					 */
				}
			}
			WARN(!cleared,
			     "Couldn't find slimbus %s port %d for closing\n",
			     (tx ? "TX" : "RX"), port_id);
		}
		wcd9xxx_interface_reg_write(tavil->wcd9xxx,
					    WCD934X_SLIM_PGD_PORT_INT_CLR_RX_0 +
					    (j / 8),
					    1 << (j % 8));
	}

	return IRQ_HANDLED;
}

static int tavil_setup_irqs(struct tavil_priv *tavil)
{
	int ret = 0;
	struct snd_soc_codec *codec = tavil->codec;
	struct wcd9xxx *wcd9xxx = tavil->wcd9xxx;
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_SLIMBUS,
				  tavil_slimbus_irq, "SLIMBUS Slave", tavil);
	if (ret)
		dev_err(codec->dev, "%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_SLIMBUS);
	else
		tavil_slim_interface_init_reg(codec);

	return ret;
}

static void tavil_init_slim_slave_cfg(struct snd_soc_codec *codec)
{
	struct tavil_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct afe_param_cdc_slimbus_slave_cfg *cfg;
	struct wcd9xxx *wcd9xxx = priv->wcd9xxx;
	uint64_t eaddr = 0;

	cfg = &priv->slimbus_slave_cfg;
	cfg->minor_version = 1;
	cfg->tx_slave_port_offset = 0;
	cfg->rx_slave_port_offset = 16;

	memcpy(&eaddr, &wcd9xxx->slim->e_addr, sizeof(wcd9xxx->slim->e_addr));
	WARN_ON(sizeof(wcd9xxx->slim->e_addr) != 6);
	cfg->device_enum_addr_lsw = eaddr & 0xFFFFFFFF;
	cfg->device_enum_addr_msw = eaddr >> 32;

	dev_dbg(codec->dev, "%s: slimbus logical address 0x%llx\n",
		__func__, eaddr);
}

static void tavil_cleanup_irqs(struct tavil_priv *tavil)
{
	struct wcd9xxx *wcd9xxx = tavil->wcd9xxx;
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;

	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_SLIMBUS, tavil);
}

static int wcd934x_get_micb_vout_ctl_val(u32 micb_mv)
{
	/* min micbias voltage is 1V and maximum is 2.85V */
	if (micb_mv < 1000 || micb_mv > 2850) {
		pr_err("%s: unsupported micbias voltage\n", __func__);
		return -EINVAL;
	}

	return (micb_mv - 1000) / 50;
}

static int tavil_handle_pdata(struct tavil_priv *tavil,
			      struct wcd9xxx_pdata *pdata)
{
	struct snd_soc_codec *codec = tavil->codec;
	int vout_ctl_1, vout_ctl_2, vout_ctl_3, vout_ctl_4;
	int rc = 0;

	if (!pdata) {
		dev_err(codec->dev, "%s: NULL pdata\n", __func__);
		return -ENODEV;
	}

	/* set micbias voltage */
	vout_ctl_1 = wcd934x_get_micb_vout_ctl_val(pdata->micbias.micb1_mv);
	vout_ctl_2 = wcd934x_get_micb_vout_ctl_val(pdata->micbias.micb2_mv);
	vout_ctl_3 = wcd934x_get_micb_vout_ctl_val(pdata->micbias.micb3_mv);
	vout_ctl_4 = wcd934x_get_micb_vout_ctl_val(pdata->micbias.micb4_mv);

	if (IS_ERR_VALUE(vout_ctl_1) || IS_ERR_VALUE(vout_ctl_2) ||
	    IS_ERR_VALUE(vout_ctl_3) || IS_ERR_VALUE(vout_ctl_4)) {
		rc = -EINVAL;
		goto done;
	}
	snd_soc_update_bits(codec, WCD934X_ANA_MICB1, 0x3F, vout_ctl_1);
	snd_soc_update_bits(codec, WCD934X_ANA_MICB2, 0x3F, vout_ctl_2);
	snd_soc_update_bits(codec, WCD934X_ANA_MICB3, 0x3F, vout_ctl_3);
	snd_soc_update_bits(codec, WCD934X_ANA_MICB4, 0x3F, vout_ctl_4);

done:
	return rc;
}

static void tavil_enable_sido_buck(struct snd_soc_codec *codec)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, WCD934X_ANA_RCO, 0x80, 0x80);
	usleep_range(100, 110);
	snd_soc_update_bits(codec, WCD934X_ANA_BUCK_CTL, 0x02, 0x02);
	usleep_range(100, 110);
	snd_soc_update_bits(codec, WCD934X_ANA_BUCK_CTL, 0x01, 0x01);
	usleep_range(100, 110);
	snd_soc_update_bits(codec, WCD934X_ANA_BUCK_CTL, 0x04, 0x04);
	usleep_range(100, 110);
	tavil->resmgr->sido_input_src = SIDO_SOURCE_RCO_BG;
}

static int tavil_soc_codec_probe(struct snd_soc_codec *codec)
{
	struct wcd9xxx *control;
	struct tavil_priv *tavil;
	struct wcd9xxx_pdata *pdata;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	int i, ret;
	void *ptr = NULL;

	control = dev_get_drvdata(codec->dev->parent);

	dev_info(codec->dev, "%s()\n", __func__);
	tavil = snd_soc_codec_get_drvdata(codec);

	/* Resource Manager post Init */
	ret = wcd_resmgr_post_init(tavil->resmgr, NULL, codec);
	if (ret) {
		dev_err(codec->dev, "%s: wcd resmgr post init failed\n",
			__func__);
		goto err;
	}
	/* Class-H Init */
	wcd_clsh_init(&tavil->clsh_d);
	/* Default HPH Mode to Class-H HiFi */
	tavil->hph_mode = CLS_H_HIFI;

	tavil->codec = codec;
	for (i = 0; i < COMPANDER_MAX; i++)
		tavil->comp_enabled[i] = 0;

	dev_dbg(codec->dev, "%s: MCLK Rate = %x\n", __func__,
			control->mclk_rate);
	if (control->mclk_rate == WCD934X_MCLK_CLK_12P288MHZ)
		snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_MCLK_CFG,
				    0x03, 0x00);
	else if (control->mclk_rate == WCD934X_MCLK_CLK_9P6MHZ)
		snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_MCLK_CFG,
				    0x03, 0x01);
	tavil_codec_init_reg(codec);
	tavil_enable_sido_buck(codec);

	pdata = dev_get_platdata(codec->dev->parent);
	ret = tavil_handle_pdata(tavil, pdata);
	if (IS_ERR_VALUE(ret)) {
		dev_err(codec->dev, "%s: bad pdata\n", __func__);
		goto err;
	}

	ptr = devm_kzalloc(codec->dev, (sizeof(tavil_rx_chs) +
			   sizeof(tavil_tx_chs)), GFP_KERNEL);
	if (!ptr) {
		ret = -ENOMEM;
		goto err;
	}

	snd_soc_dapm_add_routes(dapm, tavil_slim_audio_map,
			ARRAY_SIZE(tavil_slim_audio_map));
	for (i = 0; i < NUM_CODEC_DAIS; i++) {
		INIT_LIST_HEAD(&tavil->dai[i].wcd9xxx_ch_list);
		init_waitqueue_head(&tavil->dai[i].dai_wait);
	}
	tavil_slimbus_slave_port_cfg.slave_dev_intfdev_la =
				control->slim_slave->laddr;
	tavil_slimbus_slave_port_cfg.slave_dev_pgd_la =
				control->slim->laddr;
	tavil_slimbus_slave_port_cfg.slave_port_mapping[0] =
				WCD934X_TX13;
	tavil_init_slim_slave_cfg(codec);

	control->num_rx_port = WCD934X_RX_MAX;
	control->rx_chs = ptr;
	memcpy(control->rx_chs, tavil_rx_chs, sizeof(tavil_rx_chs));
	control->num_tx_port = WCD934X_TX_MAX;
	control->tx_chs = ptr + sizeof(tavil_rx_chs);
	memcpy(control->tx_chs, tavil_tx_chs, sizeof(tavil_tx_chs));

	ret = tavil_setup_irqs(tavil);
	if (ret) {
		dev_err(tavil->dev, "%s: tavil irq setup failed %d\n",
			__func__, ret);
		goto err_pdata;
	}

	for (i = 0; i < WCD934X_NUM_DECIMATORS; i++) {
		tavil->tx_hpf_work[i].tavil = tavil;
		tavil->tx_hpf_work[i].decimator = i;
		INIT_DELAYED_WORK(&tavil->tx_hpf_work[i].dwork,
				  tavil_tx_hpf_corner_freq_callback);

		tavil->tx_mute_dwork[i].tavil = tavil;
		tavil->tx_mute_dwork[i].decimator = i;
		INIT_DELAYED_WORK(&tavil->tx_mute_dwork[i].dwork,
				  tavil_tx_mute_update_callback);
	}
	snd_soc_dapm_sync(dapm);
	return ret;

err_pdata:
	devm_kfree(codec->dev, ptr);
err:
	return ret;
}

static int tavil_soc_codec_remove(struct snd_soc_codec *codec)
{
	struct wcd9xxx *control;
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	control = dev_get_drvdata(codec->dev->parent);
	devm_kfree(codec->dev, control->rx_chs);
	tavil_cleanup_irqs(tavil);

	return 0;
}

static struct regmap *tavil_get_regmap(struct device *dev)
{
	struct wcd9xxx *control = dev_get_drvdata(dev->parent);

	return control->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_tavil = {
	.probe = tavil_soc_codec_probe,
	.remove = tavil_soc_codec_remove,
	.controls = tavil_snd_controls,
	.num_controls = ARRAY_SIZE(tavil_snd_controls),
	.dapm_widgets = tavil_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tavil_dapm_widgets),
	.dapm_routes = tavil_audio_map,
	.num_dapm_routes = ARRAY_SIZE(tavil_audio_map),
	.get_regmap = tavil_get_regmap,
};

#ifdef CONFIG_PM
static int tavil_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tavil_priv *tavil = platform_get_drvdata(pdev);

	if (!tavil) {
		dev_err(dev, "%s: tavil private data is NULL\n", __func__);
		return -EINVAL;
	}
	dev_dbg(dev, "%s: system suspend\n", __func__);
	return 0;
}

static int tavil_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tavil_priv *tavil = platform_get_drvdata(pdev);

	if (!tavil) {
		dev_err(dev, "%s: tavil private data is NULL\n", __func__);
		return -EINVAL;
	}
	dev_dbg(dev, "%s: system resume\n", __func__);
	return 0;
}

static const struct dev_pm_ops tavil_pm_ops = {
	.suspend = tavil_suspend,
	.resume = tavil_resume,
};
#endif

static int tavil_swrm_read(void *handle, int reg)
{
	struct tavil_priv *tavil;
	struct wcd9xxx *wcd9xxx;
	unsigned short swr_rd_addr_base;
	unsigned short swr_rd_data_base;
	int val, ret;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	tavil = (struct tavil_priv *)handle;
	wcd9xxx = tavil->wcd9xxx;

	dev_dbg(tavil->dev, "%s: Reading soundwire register, 0x%x\n",
		__func__, reg);
	swr_rd_addr_base = WCD934X_SWR_AHB_BRIDGE_RD_ADDR_0;
	swr_rd_data_base = WCD934X_SWR_AHB_BRIDGE_RD_DATA_0;

	mutex_lock(&tavil->swr.read_mutex);
	ret = regmap_bulk_write(wcd9xxx->regmap, swr_rd_addr_base,
				 (u8 *)&reg, 4);
	if (ret < 0) {
		dev_err(tavil->dev, "%s: RD Addr Failure\n", __func__);
		goto done;
	}
	ret = regmap_bulk_read(wcd9xxx->regmap, swr_rd_data_base,
				(u8 *)&val, 4);
	if (ret < 0) {
		dev_err(tavil->dev, "%s: RD Data Failure\n", __func__);
		goto done;
	}
	ret = val;
done:
	mutex_unlock(&tavil->swr.read_mutex);

	return ret;
}

static int tavil_swrm_bulk_write(void *handle, u32 *reg, u32 *val, size_t len)
{
	struct tavil_priv *tavil;
	struct wcd9xxx *wcd9xxx;
	struct wcd9xxx_reg_val *bulk_reg;
	unsigned short swr_wr_addr_base;
	unsigned short swr_wr_data_base;
	int i, j, ret;

	if (!handle || !reg || !val) {
		pr_err("%s: NULL parameter\n", __func__);
		return -EINVAL;
	}
	if (len <= 0) {
		pr_err("%s: Invalid size: %zu\n", __func__, len);
		return -EINVAL;
	}
	tavil = (struct tavil_priv *)handle;
	wcd9xxx = tavil->wcd9xxx;

	swr_wr_addr_base = WCD934X_SWR_AHB_BRIDGE_WR_ADDR_0;
	swr_wr_data_base = WCD934X_SWR_AHB_BRIDGE_WR_DATA_0;

	bulk_reg = kzalloc((2 * len * sizeof(struct wcd9xxx_reg_val)),
			   GFP_KERNEL);
	if (!bulk_reg)
		return -ENOMEM;

	for (i = 0, j = 0; i < (len * 2); i += 2, j++) {
		bulk_reg[i].reg = swr_wr_data_base;
		bulk_reg[i].buf = (u8 *)(&val[j]);
		bulk_reg[i].bytes = 4;
		bulk_reg[i+1].reg = swr_wr_addr_base;
		bulk_reg[i+1].buf = (u8 *)(&reg[j]);
		bulk_reg[i+1].bytes = 4;
	}

	mutex_lock(&tavil->swr.write_mutex);
	ret = wcd9xxx_slim_bulk_write(wcd9xxx, bulk_reg,
			 (len * 2), false);
	if (ret) {
		dev_err(tavil->dev, "%s: swrm bulk write failed, ret: %d\n",
			__func__, ret);
	}
	mutex_unlock(&tavil->swr.write_mutex);

	kfree(bulk_reg);
	return ret;
}

static int tavil_swrm_write(void *handle, int reg, int val)
{
	struct tavil_priv *tavil;
	struct wcd9xxx *wcd9xxx;
	unsigned short swr_wr_addr_base;
	unsigned short swr_wr_data_base;
	struct wcd9xxx_reg_val bulk_reg[2];
	int ret;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	tavil = (struct tavil_priv *)handle;
	wcd9xxx = tavil->wcd9xxx;

	swr_wr_addr_base = WCD934X_SWR_AHB_BRIDGE_WR_ADDR_0;
	swr_wr_data_base = WCD934X_SWR_AHB_BRIDGE_WR_DATA_0;

	/* First Write the Data to register */
	bulk_reg[0].reg = swr_wr_data_base;
	bulk_reg[0].buf = (u8 *)(&val);
	bulk_reg[0].bytes = 4;
	bulk_reg[1].reg = swr_wr_addr_base;
	bulk_reg[1].buf = (u8 *)(&reg);
	bulk_reg[1].bytes = 4;

	mutex_lock(&tavil->swr.write_mutex);
	ret = wcd9xxx_slim_bulk_write(wcd9xxx, bulk_reg, 2, false);
	if (ret < 0)
		dev_err(tavil->dev, "%s: WR Data Failure\n", __func__);
	mutex_unlock(&tavil->swr.write_mutex);

	return ret;
}

static int tavil_swrm_clock(void *handle, bool enable)
{
	struct tavil_priv *tavil;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	tavil = (struct tavil_priv *)handle;

	mutex_lock(&tavil->swr.clk_mutex);
	dev_dbg(tavil->dev, "%s: swrm clock %s\n",
		__func__, (enable?"enable" : "disable"));
	if (enable) {
		tavil->swr.clk_users++;
		if (tavil->swr.clk_users == 1) {
			__tavil_cdc_mclk_enable(tavil, true);
			regmap_update_bits(tavil->wcd9xxx->regmap,
				WCD934X_CDC_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x01);
		}
	} else {
		tavil->swr.clk_users--;
		if (tavil->swr.clk_users == 0) {
			regmap_update_bits(tavil->wcd9xxx->regmap,
				WCD934X_CDC_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x00);
			__tavil_cdc_mclk_enable(tavil, false);
		}
	}
	dev_dbg(tavil->dev, "%s: swrm clock users %d\n",
		__func__, tavil->swr.clk_users);
	mutex_unlock(&tavil->swr.clk_mutex);

	return 0;
}

static int tavil_swrm_handle_irq(void *handle,
				 irqreturn_t (*swrm_irq_handler)(int irq,
								 void *data),
				 void *swrm_handle,
				 int action)
{
	struct tavil_priv *tavil;
	int ret = 0;
	struct wcd9xxx *wcd9xxx;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	tavil = (struct tavil_priv *) handle;
	wcd9xxx = tavil->wcd9xxx;

	if (action) {
		ret = wcd9xxx_request_irq(&wcd9xxx->core_res,
					  WCD934X_IRQ_SOUNDWIRE,
					  swrm_irq_handler,
					  "Tavil SWR Master", swrm_handle);
		if (ret)
			dev_err(tavil->dev, "%s: Failed to request irq %d\n",
				__func__, WCD934X_IRQ_SOUNDWIRE);
	} else
		wcd9xxx_free_irq(&wcd9xxx->core_res, WCD934X_IRQ_SOUNDWIRE,
				 swrm_handle);

	return ret;
}

static void tavil_add_child_devices(struct work_struct *work)
{
	struct tavil_priv *tavil;
	struct platform_device *pdev;
	struct device_node *node;
	struct wcd9xxx *wcd9xxx;
	struct tavil_swr_ctrl_data *swr_ctrl_data = NULL, *temp;
	int ret, ctrl_num = 0;
	struct wcd_swr_ctrl_platform_data *platdata;
	char plat_dev_name[WCD934X_STRING_LEN];

	tavil = container_of(work, struct tavil_priv,
			     tavil_add_child_devices_work);
	if (!tavil) {
		pr_err("%s: Memory for WCD934X does not exist\n",
			__func__);
		return;
	}
	wcd9xxx = tavil->wcd9xxx;
	if (!wcd9xxx) {
		pr_err("%s: Memory for WCD9XXX does not exist\n",
			__func__);
		return;
	}
	if (!wcd9xxx->dev->of_node) {
		dev_err(wcd9xxx->dev, "%s: DT node for wcd9xxx does not exist\n",
			__func__);
		return;
	}

	platdata = &tavil->swr.plat_data;

	for_each_child_of_node(wcd9xxx->dev->of_node, node) {
		if (!strcmp(node->name, "swr_master"))
			strlcpy(plat_dev_name, "tavil_swr_ctrl",
				(WCD934X_STRING_LEN - 1));
		else if (strnstr(node->name, "msm_cdc_pinctrl",
				 strlen("msm_cdc_pinctrl")) != NULL)
			strlcpy(plat_dev_name, node->name,
				(WCD934X_STRING_LEN - 1));
		else
			continue;

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(wcd9xxx->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err_mem;
		}
		pdev->dev.parent = tavil->dev;
		pdev->dev.of_node = node;

		if (strcmp(node->name, "swr_master") == 0) {
			ret = platform_device_add_data(pdev, platdata,
						       sizeof(*platdata));
			if (ret) {
				dev_err(&pdev->dev,
					"%s: cannot add plat data ctrl:%d\n",
					__func__, ctrl_num);
				goto err_pdev_add;
			}
		}

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Cannot add platform device\n",
				__func__);
			goto err_pdev_add;
		}

		if (strcmp(node->name, "swr_master") == 0) {
			temp = krealloc(swr_ctrl_data,
					(ctrl_num + 1) * sizeof(
					struct tavil_swr_ctrl_data),
					GFP_KERNEL);
			if (!temp) {
				dev_err(wcd9xxx->dev, "out of memory\n");
				ret = -ENOMEM;
				goto err_pdev_add;
			}
			swr_ctrl_data = temp;
			swr_ctrl_data[ctrl_num].swr_pdev = pdev;
			ctrl_num++;
			dev_dbg(&pdev->dev,
				"%s: Added soundwire ctrl device(s)\n",
				__func__);
			tavil->swr.ctrl_data = swr_ctrl_data;
		}
	}

	return;

err_pdev_add:
	platform_device_put(pdev);
err_mem:
	return;
}

static int __tavil_enable_efuse_sensing(struct tavil_priv *tavil)
{
	int val, rc;

	__tavil_cdc_mclk_enable(tavil, true);

	regmap_update_bits(tavil->wcd9xxx->regmap,
			WCD934X_CHIP_TIER_CTRL_EFUSE_CTL, 0x1E, 0x10);
	regmap_update_bits(tavil->wcd9xxx->regmap,
			WCD934X_CHIP_TIER_CTRL_EFUSE_CTL, 0x01, 0x01);

	/*
	 * 5ms sleep required after enabling efuse control
	 * before checking the status.
	 */
	usleep_range(5000, 5500);
	rc = regmap_read(tavil->wcd9xxx->regmap,
			 WCD934X_CHIP_TIER_CTRL_EFUSE_STATUS, &val);
	if (rc || (!(val & 0x01)))
		WARN(1, "%s: Efuse sense is not complete\n", __func__);

	__tavil_cdc_mclk_enable(tavil, false);

	return rc;
}

static int tavil_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tavil_priv *tavil;
	struct clk *wcd_ext_clk;
	struct wcd9xxx_resmgr_v2 *resmgr;
	int val1, val2, val3, val4;

	tavil = devm_kzalloc(&pdev->dev, sizeof(struct tavil_priv),
			    GFP_KERNEL);
	if (!tavil)
		return -ENOMEM;

	platform_set_drvdata(pdev, tavil);

	tavil->wcd9xxx = dev_get_drvdata(pdev->dev.parent);
	tavil->dev = &pdev->dev;
	INIT_WORK(&tavil->tavil_add_child_devices_work,
		  tavil_add_child_devices);
	mutex_init(&tavil->swr.read_mutex);
	mutex_init(&tavil->swr.write_mutex);
	mutex_init(&tavil->swr.clk_mutex);
	mutex_init(&tavil->codec_mutex);

	/*
	 * Init resource manager so that if child nodes such as SoundWire
	 * requests for clock, resource manager can honor the request
	 */
	resmgr = wcd_resmgr_init(&tavil->wcd9xxx->core_res, NULL);
	if (IS_ERR(resmgr)) {
		ret = PTR_ERR(resmgr);
		dev_err(&pdev->dev, "%s: Failed to initialize wcd resmgr\n",
			__func__);
		goto err_resmgr;
	}
	tavil->resmgr = resmgr;
	tavil->swr.plat_data.handle = (void *) tavil;
	tavil->swr.plat_data.read = tavil_swrm_read;
	tavil->swr.plat_data.write = tavil_swrm_write;
	tavil->swr.plat_data.bulk_write = tavil_swrm_bulk_write;
	tavil->swr.plat_data.clk = tavil_swrm_clock;
	tavil->swr.plat_data.handle_irq = tavil_swrm_handle_irq;

	/* Register for Clock */
	wcd_ext_clk = clk_get(tavil->wcd9xxx->dev, "wcd_clk");
	if (IS_ERR(wcd_ext_clk)) {
		dev_err(tavil->wcd9xxx->dev, "%s: clk get %s failed\n",
			__func__, "wcd_ext_clk");
		goto err_clk;
	}
	tavil->wcd_ext_clk = wcd_ext_clk;
	set_bit(AUDIO_NOMINAL, &tavil->status_mask);
	/* Update codec register default values */
	tavil_update_reg_defaults(tavil);
	__tavil_enable_efuse_sensing(tavil);

	regmap_read(tavil->wcd9xxx->regmap,
				WCD934X_CHIP_TIER_CTRL_EFUSE_VAL_OUT0, &val1);
	regmap_read(tavil->wcd9xxx->regmap,
				WCD934X_CHIP_TIER_CTRL_EFUSE_VAL_OUT14, &val2);
	regmap_read(tavil->wcd9xxx->regmap,
				WCD934X_CHIP_TIER_CTRL_EFUSE_VAL_OUT15, &val3);
	regmap_read(tavil->wcd9xxx->regmap,
				WCD934X_CHIP_TIER_CTRL_EFUSE_VAL_OUT9, &val4);
	dev_dbg(&pdev->dev, "%s: chip version :0x%x 0x:%x 0x:%x 0x:%x\n",
		  __func__, val1, val2, val3, val4);

	/* Register with soc framework */
	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_tavil,
				  tavil_dai, ARRAY_SIZE(tavil_dai));
	if (ret) {
		dev_err(&pdev->dev, "%s: Codec registration failed\n",
		 __func__);
		goto err_cdc_reg;
	}
	schedule_work(&tavil->tavil_add_child_devices_work);

	return ret;

err_cdc_reg:
	clk_put(tavil->wcd_ext_clk);
err_clk:
	wcd_resmgr_remove(tavil->resmgr);
err_resmgr:
	mutex_destroy(&tavil->codec_mutex);
	mutex_destroy(&tavil->swr.read_mutex);
	mutex_destroy(&tavil->swr.write_mutex);
	mutex_destroy(&tavil->swr.clk_mutex);
	devm_kfree(&pdev->dev, tavil);

	return ret;
}

static int tavil_remove(struct platform_device *pdev)
{
	struct tavil_priv *tavil;

	tavil = platform_get_drvdata(pdev);
	if (!tavil)
		return -EINVAL;

	mutex_destroy(&tavil->codec_mutex);
	mutex_destroy(&tavil->swr.read_mutex);
	mutex_destroy(&tavil->swr.write_mutex);
	mutex_destroy(&tavil->swr.clk_mutex);

	snd_soc_unregister_codec(&pdev->dev);
	clk_put(tavil->wcd_ext_clk);
	wcd_resmgr_remove(tavil->resmgr);
	devm_kfree(&pdev->dev, tavil);
	return 0;
}

static struct platform_driver tavil_codec_driver = {
	.probe = tavil_probe,
	.remove = tavil_remove,
	.driver = {
		.name = "tavil_codec",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &tavil_pm_ops,
#endif
	},
};

module_platform_driver(tavil_codec_driver);

MODULE_DESCRIPTION("Tavil Codec driver");
MODULE_LICENSE("GPL v2");
