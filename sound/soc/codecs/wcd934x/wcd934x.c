/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/spi/spi.h>
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
#include "wcd934x-mbhc.h"
#include "wcd934x-routing.h"
#include "wcd934x-dsp-cntl.h"
#include "../wcd9xxx-common-v2.h"
#include "../wcd9xxx-resmgr-v2.h"
#include "../wcdcal-hwdep.h"
#include "wcd934x-dsd.h"

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

/* Macros for packing register writes into a U32 */
#define WCD934X_PACKED_REG_SIZE sizeof(u32)
#define WCD934X_CODEC_UNPACK_ENTRY(packed, reg, mask, val) \
	do { \
		((reg) = ((packed >> 16) & (0xffff))); \
		((mask) = ((packed >> 8) & (0xff))); \
		((val) = ((packed) & (0xff))); \
	} while (0)

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

#define WCD934X_INTERP_MUX_NUM_INPUTS 3
#define WCD934X_NUM_INTERPOLATORS 9
#define WCD934X_NUM_DECIMATORS 9
#define WCD934X_RX_PATH_CTL_OFFSET 20

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

#define WCD934X_DIG_CORE_REG_MIN  WCD934X_CDC_ANC0_CLK_RESET_CTL
#define WCD934X_DIG_CORE_REG_MAX  0xFFF

#define WCD934X_MAX_MICBIAS 4
#define DAPM_MICBIAS1_STANDALONE "MIC BIAS1 Standalone"
#define DAPM_MICBIAS2_STANDALONE "MIC BIAS2 Standalone"
#define DAPM_MICBIAS3_STANDALONE "MIC BIAS3 Standalone"
#define DAPM_MICBIAS4_STANDALONE "MIC BIAS4 Standalone"

#define  TX_HPF_CUT_OFF_FREQ_MASK	0x60
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

#define CPE_ERR_WDOG_BITE BIT(0)
#define CPE_FATAL_IRQS CPE_ERR_WDOG_BITE

#define WCD934X_MAD_AUDIO_FIRMWARE_PATH "wcd934x/wcd934x_mad_audio.bin"

#define TAVIL_VERSION_ENTRY_SIZE 17

#define WCD934X_DIG_CORE_COLLAPSE_TIMER_MS  (5 * 1000)

enum {
	POWER_COLLAPSE,
	POWER_RESUME,
};

static int dig_core_collapse_enable = 1;
module_param(dig_core_collapse_enable, int,
		S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(dig_core_collapse_enable, "enable/disable power gating");

/* dig_core_collapse timer in seconds */
static int dig_core_collapse_timer = (WCD934X_DIG_CORE_COLLAPSE_TIMER_MS/1000);
module_param(dig_core_collapse_timer, int,
		S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(dig_core_collapse_timer, "timer for power gating");

#define TAVIL_HPH_REG_RANGE_1  (WCD934X_HPH_R_DAC_CTL - WCD934X_HPH_CNP_EN + 1)
#define TAVIL_HPH_REG_RANGE_2  (WCD934X_HPH_NEW_ANA_HPH3 -\
				WCD934X_HPH_NEW_ANA_HPH2 + 1)
#define TAVIL_HPH_REG_RANGE_3  (WCD934X_HPH_NEW_INT_PA_RDAC_MISC3 -\
				WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL + 1)
#define TAVIL_HPH_TOTAL_REG    (TAVIL_HPH_REG_RANGE_1 + TAVIL_HPH_REG_RANGE_2 +\
				TAVIL_HPH_REG_RANGE_3)

enum {
	VI_SENSE_1,
	VI_SENSE_2,
	AUDIO_NOMINAL,
	HPH_PA_DELAY,
	CLSH_Z_CONFIG,
	ANC_MIC_AMIC1,
	ANC_MIC_AMIC2,
	ANC_MIC_AMIC3,
	ANC_MIC_AMIC4,
};

enum {
	AIF1_PB = 0,
	AIF1_CAP,
	AIF2_PB,
	AIF2_CAP,
	AIF3_PB,
	AIF3_CAP,
	AIF4_PB,
	AIF4_VIFEED,
	AIF4_MAD_TX,
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
	INTERP_MAIN_PATH,
	INTERP_MIX_PATH,
};

struct tavil_idle_detect_config {
	u8 hph_idle_thr;
	u8 hph_idle_detect_en;
};

static const struct intr_data wcd934x_intr_table[] = {
	{WCD9XXX_IRQ_SLIMBUS, false},
	{WCD934X_IRQ_MBHC_SW_DET, true},
	{WCD934X_IRQ_MBHC_BUTTON_PRESS_DET, true},
	{WCD934X_IRQ_MBHC_BUTTON_RELEASE_DET, true},
	{WCD934X_IRQ_MBHC_ELECT_INS_REM_DET, true},
	{WCD934X_IRQ_MBHC_ELECT_INS_REM_LEG_DET, true},
	{WCD934X_IRQ_MISC, false},
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

struct tavil_cpr_reg_defaults {
	int wr_data;
	int wr_addr;
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
	0,							/* AIF1_PB */
	BIT(AIF2_CAP) | BIT(AIF3_CAP) | BIT(AIF4_MAD_TX),	/* AIF1_CAP */
	0,							/* AIF2_PB */
	BIT(AIF1_CAP) | BIT(AIF3_CAP) | BIT(AIF4_MAD_TX),	/* AIF2_CAP */
	0,							/* AIF3_PB */
	BIT(AIF1_CAP) | BIT(AIF2_CAP) | BIT(AIF4_MAD_TX),	/* AIF3_CAP */
	0,							/* AIF4_PB */
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

enum {
	ASRC_IN_HPHL,
	ASRC_IN_LO1,
	ASRC_IN_HPHR,
	ASRC_IN_LO2,
	ASRC_IN_SPKR1,
	ASRC_IN_SPKR2,
	ASRC_INVALID,
};

enum {
	ASRC0,
	ASRC1,
	ASRC2,
	ASRC3,
	ASRC_MAX,
};

enum {
	CONV_88P2K_TO_384K,
	CONV_96K_TO_352P8K,
	CONV_352P8K_TO_384K,
	CONV_384K_TO_352P8K,
	CONV_384K_TO_384K,
	CONV_96K_TO_384K,
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
	{
		1,
		(WCD934X_REGISTER_START_OFFSET +
		 WCD934X_CDC_ANC0_IIR_ADAPT_CTL),
		AANC_FF_GAIN_ADAPTIVE, 0x4, WCD934X_REG_BITS, 0
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET +
		 WCD934X_CDC_ANC0_IIR_ADAPT_CTL),
		AANC_FFGAIN_ADAPTIVE_EN, 0x8, WCD934X_REG_BITS, 0
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET +
		 WCD934X_CDC_ANC0_FF_A_GAIN_CTL),
		AANC_GAIN_CONTROL, 0xFF, WCD934X_REG_BITS, 0
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET +
		 SB_PGD_TX_PORT_MULTI_CHANNEL_0(0)),
		SB_PGD_TX_PORTn_MULTI_CHNL_0, 0xFF, WCD934X_REG_BITS, 0x4
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET +
		 SB_PGD_TX_PORT_MULTI_CHANNEL_1(0)),
		SB_PGD_TX_PORTn_MULTI_CHNL_1, 0xFF, WCD934X_REG_BITS, 0x4
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET +
		 SB_PGD_RX_PORT_MULTI_CHANNEL_0(0x180, 0)),
		SB_PGD_RX_PORTn_MULTI_CHNL_0, 0xFF, WCD934X_REG_BITS, 0x4
	},
	{
		1,
		(WCD934X_REGISTER_START_OFFSET +
		 SB_PGD_RX_PORT_MULTI_CHANNEL_0(0x181, 0)),
		SB_PGD_RX_PORTn_MULTI_CHNL_1, 0xFF, WCD934X_REG_BITS, 0x4
	},
};

static struct afe_param_cdc_reg_cfg_data tavil_audio_reg_cfg = {
	.num_registers = ARRAY_SIZE(audio_reg_cfg),
	.reg_data = audio_reg_cfg,
};

static struct afe_param_id_cdc_aanc_version tavil_cdc_aanc_version = {
	.cdc_aanc_minor_version = AFE_API_VERSION_CDC_AANC_VERSION,
	.aanc_hw_version        = AANC_HW_BLOCK_VERSION_2,
};

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static const DECLARE_TLV_DB_SCALE(line_gain, 0, 7, 1);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);

#define WCD934X_TX_UNMUTE_DELAY_MS 40

static int tx_unmute_delay = WCD934X_TX_UNMUTE_DELAY_MS;
module_param(tx_unmute_delay, int,
	     S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(tx_unmute_delay, "delay to unmute the tx path");

static void tavil_codec_set_tx_hold(struct snd_soc_codec *, u16, bool);

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

#define WCD934X_SPK_ANC_EN_DELAY_MS 350
static int spk_anc_en_delay = WCD934X_SPK_ANC_EN_DELAY_MS;
module_param(spk_anc_en_delay, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(spk_anc_en_delay, "delay to enable anc in speaker path");

struct spk_anc_work {
	struct tavil_priv *tavil;
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
	s32 micb_ref[TAVIL_MAX_MICBIAS];
	s32 pullup_ref[TAVIL_MAX_MICBIAS];

	/* ANC related */
	u32 anc_slot;
	bool anc_func;

	/* compander */
	int comp_enabled[COMPANDER_MAX];
	int ear_spkr_gain;

	/* class h specific data */
	struct wcd_clsh_cdc_data clsh_d;
	/* Tavil Interpolator Mode Select for EAR, HPH_L and HPH_R */
	u32 hph_mode;

	/* Mad switch reference count */
	int mad_switch_cnt;

	/* track tavil interface type */
	u8 intf_type;

	/* to track the status */
	unsigned long status_mask;

	struct afe_param_cdc_slimbus_slave_cfg slimbus_slave_cfg;

	/* num of slim ports required */
	struct wcd9xxx_codec_dai_data  dai[NUM_CODEC_DAIS];
	/* Port values for Rx and Tx codec_dai */
	unsigned int rx_port_value[WCD934X_RX_MAX];
	unsigned int tx_port_value;

	struct wcd9xxx_resmgr_v2 *resmgr;
	struct wcd934x_swr swr;
	struct mutex micb_lock;

	struct delayed_work power_gate_work;
	struct mutex power_lock;

	struct clk *wcd_ext_clk;

	/* mbhc module */
	struct wcd934x_mbhc *mbhc;

	struct mutex codec_mutex;
	struct work_struct tavil_add_child_devices_work;
	struct hpf_work tx_hpf_work[WCD934X_NUM_DECIMATORS];
	struct tx_mute_work tx_mute_dwork[WCD934X_NUM_DECIMATORS];
	struct spk_anc_work spk_anc_dwork;

	unsigned int vi_feed_value;

	/* DSP control */
	struct wcd_dsp_cntl *wdsp_cntl;

	/* cal info for codec */
	struct fw_info *fw_data;

	/* Entry for version info */
	struct snd_info_entry *entry;
	struct snd_info_entry *version_entry;

	/* SVS voting related */
	struct mutex svs_mutex;
	int svs_ref_cnt;

	int native_clk_users;
	/* ASRC users count */
	int asrc_users[ASRC_MAX];
	int asrc_output_mode[ASRC_MAX];
	/* Main path clock users count */
	int main_clk_users[WCD934X_NUM_INTERPOLATORS];
	struct tavil_dsd_config *dsd_config;
	struct tavil_idle_detect_config idle_det_cfg;

	int power_active_ref;
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

static int __tavil_enable_efuse_sensing(struct tavil_priv *tavil);

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
	int rc, version = -1;

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

	if (id_major != TAVIL_MAJOR)
		goto version_unknown;

	/*
	 * As fine version info cannot be retrieved before tavil probe.
	 * Assign coarse versions for possible future use before tavil probe.
	 */
	if (id_minor == cpu_to_le16(0))
		version = TAVIL_VERSION_1_0;
	else if (id_minor == cpu_to_le16(0x01))
		version = TAVIL_VERSION_1_1;

version_unknown:
	if (version < 0)
		dev_err(wcd9xxx->dev, "%s: wcd934x version unknown\n",
			__func__);

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
	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_RST_CTL, 0x7);

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
		return &tavil_cdc_aanc_version;
	case AFE_CDC_REGISTER_PAGE_CONFIG:
		return &tavil_cdc_reg_page_cfg;
	default:
		dev_info(codec->dev, "%s: Unknown config_type 0x%x\n",
			__func__, config_type);
		return NULL;
	}
}
EXPORT_SYMBOL(tavil_get_afe_config);

static bool is_tavil_playback_dai(int dai_id)
{
	if ((dai_id == AIF1_PB) || (dai_id == AIF2_PB) ||
	    (dai_id == AIF3_PB) || (dai_id == AIF4_PB))
		return true;

	return false;
}

static int tavil_find_playback_dai_id_for_port(int port_id,
					       struct tavil_priv *tavil)
{
	struct wcd9xxx_codec_dai_data *dai;
	struct wcd9xxx_ch *ch;
	int i, slv_port_id;

	for (i = AIF1_PB; i < NUM_CODEC_DAIS; i++) {
		if (!is_tavil_playback_dai(i))
			continue;

		dai = &tavil->dai[i];
		list_for_each_entry(ch, &dai->wcd9xxx_ch_list, list) {
			slv_port_id = wcd9xxx_get_slave_port(ch->ch_num);
			if ((slv_port_id > 0) && (slv_port_id == port_id))
				return i;
		}
	}

	return -EINVAL;
}

static void tavil_vote_svs(struct tavil_priv *tavil, bool vote)
{
	struct wcd9xxx *wcd9xxx;

	wcd9xxx = tavil->wcd9xxx;

	mutex_lock(&tavil->svs_mutex);
	if (vote) {
		tavil->svs_ref_cnt++;
		if (tavil->svs_ref_cnt == 1)
			regmap_update_bits(wcd9xxx->regmap,
					   WCD934X_CPE_SS_PWR_SYS_PSTATE_CTL_0,
					   0x01, 0x01);
	} else {
		/* Do not decrement ref count if it is already 0 */
		if (tavil->svs_ref_cnt == 0)
			goto done;

		tavil->svs_ref_cnt--;
		if (tavil->svs_ref_cnt == 0)
			regmap_update_bits(wcd9xxx->regmap,
					   WCD934X_CPE_SS_PWR_SYS_PSTATE_CTL_0,
					   0x01, 0x00);
	}
done:
	dev_dbg(tavil->dev, "%s: vote = %s, updated ref cnt = %u\n", __func__,
		vote ? "vote" : "Unvote", tavil->svs_ref_cnt);
	mutex_unlock(&tavil->svs_mutex);
}

static int tavil_get_anc_slot(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tavil->anc_slot;
	return 0;
}

static int tavil_put_anc_slot(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	tavil->anc_slot = ucontrol->value.integer.value[0];
	return 0;
}

static int tavil_get_anc_func(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = (tavil->anc_func == true ? 1 : 0);
	return 0;
}

static int tavil_put_anc_func(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	mutex_lock(&tavil->codec_mutex);
	tavil->anc_func = (!ucontrol->value.integer.value[0] ? false : true);
	dev_dbg(codec->dev, "%s: anc_func %x", __func__, tavil->anc_func);

	if (tavil->anc_func == true) {
		snd_soc_dapm_enable_pin(dapm, "ANC EAR PA");
		snd_soc_dapm_enable_pin(dapm, "ANC EAR");
		snd_soc_dapm_enable_pin(dapm, "ANC SPK1 PA");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHL PA");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHR PA");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHL");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHR");
		snd_soc_dapm_disable_pin(dapm, "EAR PA");
		snd_soc_dapm_disable_pin(dapm, "EAR");
		snd_soc_dapm_disable_pin(dapm, "HPHL PA");
		snd_soc_dapm_disable_pin(dapm, "HPHR PA");
		snd_soc_dapm_disable_pin(dapm, "HPHL");
		snd_soc_dapm_disable_pin(dapm, "HPHR");
	} else {
		snd_soc_dapm_disable_pin(dapm, "ANC EAR PA");
		snd_soc_dapm_disable_pin(dapm, "ANC EAR");
		snd_soc_dapm_disable_pin(dapm, "ANC SPK1 PA");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHL PA");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHR PA");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHL");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHR");
		snd_soc_dapm_enable_pin(dapm, "EAR PA");
		snd_soc_dapm_enable_pin(dapm, "EAR");
		snd_soc_dapm_enable_pin(dapm, "HPHL");
		snd_soc_dapm_enable_pin(dapm, "HPHR");
		snd_soc_dapm_enable_pin(dapm, "HPHL PA");
		snd_soc_dapm_enable_pin(dapm, "HPHR PA");
	}
	mutex_unlock(&tavil->codec_mutex);

	snd_soc_dapm_sync(dapm);
	return 0;
}

static int tavil_codec_enable_anc(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	const char *filename;
	const struct firmware *fw;
	int i;
	int ret = 0;
	int num_anc_slots;
	struct wcd9xxx_anc_header *anc_head;
	struct firmware_cal *hwdep_cal = NULL;
	u32 anc_writes_size = 0;
	u32 anc_cal_size = 0;
	int anc_size_remaining;
	u32 *anc_ptr;
	u16 reg;
	u8 mask, val;
	size_t cal_size;
	const void *data;

	if (!tavil->anc_func)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		hwdep_cal = wcdcal_get_fw_cal(tavil->fw_data, WCD9XXX_ANC_CAL);
		if (hwdep_cal) {
			data = hwdep_cal->data;
			cal_size = hwdep_cal->size;
			dev_dbg(codec->dev, "%s: using hwdep calibration, cal_size %zd",
				__func__, cal_size);
		} else {
			filename = "WCD934X/WCD934X_anc.bin";
			ret = request_firmware(&fw, filename, codec->dev);
			if (IS_ERR_VALUE(ret)) {
				dev_err(codec->dev, "%s: Failed to acquire ANC data: %d\n",
					__func__, ret);
				return ret;
			}
			if (!fw) {
				dev_err(codec->dev, "%s: Failed to get anc fw\n",
					__func__);
				return -ENODEV;
			}
			data = fw->data;
			cal_size = fw->size;
			dev_dbg(codec->dev, "%s: using request_firmware calibration\n",
				__func__);
		}
		if (cal_size < sizeof(struct wcd9xxx_anc_header)) {
			dev_err(codec->dev, "%s: Invalid cal_size %zd\n",
				__func__, cal_size);
			ret = -EINVAL;
			goto err;
		}
		/* First number is the number of register writes */
		anc_head = (struct wcd9xxx_anc_header *)(data);
		anc_ptr = (u32 *)(data + sizeof(struct wcd9xxx_anc_header));
		anc_size_remaining = cal_size -
				     sizeof(struct wcd9xxx_anc_header);
		num_anc_slots = anc_head->num_anc_slots;

		if (tavil->anc_slot >= num_anc_slots) {
			dev_err(codec->dev, "%s: Invalid ANC slot selected\n",
				__func__);
			ret = -EINVAL;
			goto err;
		}
		for (i = 0; i < num_anc_slots; i++) {
			if (anc_size_remaining < WCD934X_PACKED_REG_SIZE) {
				dev_err(codec->dev, "%s: Invalid register format\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}
			anc_writes_size = (u32)(*anc_ptr);
			anc_size_remaining -= sizeof(u32);
			anc_ptr += 1;

			if ((anc_writes_size * WCD934X_PACKED_REG_SIZE) >
			    anc_size_remaining) {
				dev_err(codec->dev, "%s: Invalid register format\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}

			if (tavil->anc_slot == i)
				break;

			anc_size_remaining -= (anc_writes_size *
				WCD934X_PACKED_REG_SIZE);
			anc_ptr += anc_writes_size;
		}
		if (i == num_anc_slots) {
			dev_err(codec->dev, "%s: Selected ANC slot not present\n",
				__func__);
			ret = -EINVAL;
			goto err;
		}

		anc_cal_size = anc_writes_size;
		for (i = 0; i < anc_writes_size; i++) {
			WCD934X_CODEC_UNPACK_ENTRY(anc_ptr[i], reg, mask, val);
			snd_soc_write(codec, reg, (val & mask));
		}

		/* Rate converter clk enable and set bypass mode */
		if (!strcmp(w->name, "RX INT0 DAC") ||
		    !strcmp(w->name, "RX INT1 DAC") ||
		    !strcmp(w->name, "ANC SPK1 PA")) {
			snd_soc_update_bits(codec,
					    WCD934X_CDC_ANC0_RC_COMMON_CTL,
					    0x05, 0x05);
			if (!strcmp(w->name, "RX INT1 DAC")) {
				snd_soc_update_bits(codec,
					WCD934X_CDC_ANC0_FIFO_COMMON_CTL,
					0x66, 0x66);
			}
		} else if (!strcmp(w->name, "RX INT2 DAC")) {
			snd_soc_update_bits(codec,
					    WCD934X_CDC_ANC1_RC_COMMON_CTL,
					    0x05, 0x05);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_ANC1_FIFO_COMMON_CTL,
					    0x66, 0x66);
		}
		if (!strcmp(w->name, "RX INT1 DAC"))
			snd_soc_update_bits(codec,
				WCD934X_CDC_ANC0_CLK_RESET_CTL, 0x08, 0x08);
		else if (!strcmp(w->name, "RX INT2 DAC"))
			snd_soc_update_bits(codec,
				WCD934X_CDC_ANC1_CLK_RESET_CTL, 0x08, 0x08);

		if (!hwdep_cal)
			release_firmware(fw);
		break;

	case SND_SOC_DAPM_POST_PMU:
		if (!strcmp(w->name, "ANC HPHL PA") ||
		    !strcmp(w->name, "ANC HPHR PA")) {
			/* Remove ANC Rx from reset */
			snd_soc_update_bits(codec,
					    WCD934X_CDC_ANC0_CLK_RESET_CTL,
					    0x08, 0x00);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_ANC1_CLK_RESET_CTL,
					    0x08, 0x00);
		}

		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, WCD934X_CDC_ANC0_RC_COMMON_CTL,
				    0x05, 0x00);
		if (!strcmp(w->name, "ANC EAR PA") ||
		    !strcmp(w->name, "ANC SPK1 PA") ||
		    !strcmp(w->name, "ANC HPHL PA")) {
			snd_soc_update_bits(codec, WCD934X_CDC_ANC0_MODE_1_CTL,
					    0x30, 0x00);
			msleep(50);
			snd_soc_update_bits(codec, WCD934X_CDC_ANC0_MODE_1_CTL,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_ANC0_CLK_RESET_CTL,
					    0x38, 0x38);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_ANC0_CLK_RESET_CTL,
					    0x07, 0x00);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_ANC0_CLK_RESET_CTL,
					    0x38, 0x00);
		} else if (!strcmp(w->name, "ANC HPHR PA")) {
			snd_soc_update_bits(codec, WCD934X_CDC_ANC1_MODE_1_CTL,
					    0x30, 0x00);
			msleep(50);
			snd_soc_update_bits(codec, WCD934X_CDC_ANC1_MODE_1_CTL,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_ANC1_CLK_RESET_CTL,
					    0x38, 0x38);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_ANC1_CLK_RESET_CTL,
					    0x07, 0x00);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_ANC1_CLK_RESET_CTL,
					    0x38, 0x00);
		}
		break;
	}

	return 0;
err:
	if (!hwdep_cal)
		release_firmware(fw);
	return ret;
}

static int tavil_vi_feed_mixer_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tavil_priv *tavil_p = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tavil_p->vi_feed_value;

	return 0;
}

static int tavil_vi_feed_mixer_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tavil_priv *tavil_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 port_id = mixer->shift;
	u32 enable = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: enable: %d, port_id:%d, dai_id: %d\n",
		__func__, enable, port_id, dai_id);

	tavil_p->vi_feed_value = ucontrol->value.integer.value[0];

	mutex_lock(&tavil_p->codec_mutex);
	if (enable) {
		if (port_id == WCD934X_TX14 && !test_bit(VI_SENSE_1,
						&tavil_p->status_mask)) {
			list_add_tail(&core->tx_chs[WCD934X_TX14].list,
					&tavil_p->dai[dai_id].wcd9xxx_ch_list);
			set_bit(VI_SENSE_1, &tavil_p->status_mask);
		}
		if (port_id == WCD934X_TX15 && !test_bit(VI_SENSE_2,
						&tavil_p->status_mask)) {
			list_add_tail(&core->tx_chs[WCD934X_TX15].list,
					&tavil_p->dai[dai_id].wcd9xxx_ch_list);
			set_bit(VI_SENSE_2, &tavil_p->status_mask);
		}
	} else {
		if (port_id == WCD934X_TX14 && test_bit(VI_SENSE_1,
					&tavil_p->status_mask)) {
			list_del_init(&core->tx_chs[WCD934X_TX14].list);
			clear_bit(VI_SENSE_1, &tavil_p->status_mask);
		}
		if (port_id == WCD934X_TX15 && test_bit(VI_SENSE_2,
					&tavil_p->status_mask)) {
			list_del_init(&core->tx_chs[WCD934X_TX15].list);
			clear_bit(VI_SENSE_2, &tavil_p->status_mask);
		}
	}
	mutex_unlock(&tavil_p->codec_mutex);
	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, enable, NULL);

	return 0;
}

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
	case AIF4_MAD_TX:
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

	ucontrol->value.enumerated.item[0] =
				tavil_p->rx_port_value[widget->shift];
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
	unsigned int rx_port_value;
	u32 port_id = widget->shift;

	tavil_p->rx_port_value[port_id] = ucontrol->value.enumerated.item[0];
	rx_port_value = tavil_p->rx_port_value[port_id];

	mutex_lock(&tavil_p->codec_mutex);
	dev_dbg(codec->dev, "%s: wname %s cname %s value %u shift %d item %ld\n",
		__func__, widget->name, ucontrol->id.name,
		rx_port_value, widget->shift,
		ucontrol->value.integer.value[0]);

	/* value need to match the Virtual port and AIF number */
	switch (rx_port_value) {
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
		dev_err(codec->dev, "Unknown AIF %d\n", rx_port_value);
		goto err;
	}
rtn:
	mutex_unlock(&tavil_p->codec_mutex);
	snd_soc_dapm_mux_update_power(widget->dapm, kcontrol,
				      rx_port_value, e, update);

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

static void tavil_codec_mute_dsd(struct snd_soc_codec *codec,
				 struct list_head *ch_list)
{
	u8 dsd0_in;
	u8 dsd1_in;
	struct wcd9xxx_ch *ch;

	/* Read DSD Input Ports */
	dsd0_in = (snd_soc_read(codec, WCD934X_CDC_DSD0_CFG0) & 0x3C) >> 2;
	dsd1_in = (snd_soc_read(codec, WCD934X_CDC_DSD1_CFG0) & 0x3C) >> 2;

	if ((dsd0_in == 0) && (dsd1_in == 0))
		return;

	/*
	 * Check if the ports getting disabled are connected to DSD inputs.
	 * If connected, enable DSD mute to avoid DC entering into DSD Filter
	 */
	list_for_each_entry(ch, ch_list, list) {
		if (ch->port == (dsd0_in + WCD934X_RX_PORT_START_NUMBER - 1))
			snd_soc_update_bits(codec, WCD934X_CDC_DSD0_CFG2,
					    0x04, 0x04);
		if (ch->port == (dsd1_in + WCD934X_RX_PORT_START_NUMBER - 1))
			snd_soc_update_bits(codec, WCD934X_CDC_DSD1_CFG2,
					    0x04, 0x04);
	}
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
	struct tavil_dsd_config *dsd_conf = tavil_p->dsd_config;

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
		if (dsd_conf)
			tavil_codec_mute_dsd(codec, &dai->wcd9xxx_ch_list);

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

static int tavil_codec_enable_slimvi_feedback(struct snd_soc_dapm_widget *w,
					      struct snd_kcontrol *kcontrol,
					      int event)
{
	struct wcd9xxx *core = NULL;
	struct snd_soc_codec *codec = NULL;
	struct tavil_priv *tavil_p = NULL;
	int ret = 0;
	struct wcd9xxx_codec_dai_data *dai = NULL;

	codec = snd_soc_dapm_to_codec(w->dapm);
	tavil_p = snd_soc_codec_get_drvdata(codec);
	core = dev_get_drvdata(codec->dev->parent);

	dev_dbg(codec->dev,
		"%s: num_dai %d stream name %s w->name %s event %d shift %d\n",
		__func__, codec->component.num_dai, w->sname,
		w->name, event, w->shift);

	if (w->shift != AIF4_VIFEED) {
		pr_err("%s Error in enabling the tx path\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	dai = &tavil_p->dai[w->shift];
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (test_bit(VI_SENSE_1, &tavil_p->status_mask)) {
			dev_dbg(codec->dev, "%s: spkr1 enabled\n", __func__);
			/* Enable V&I sensing */
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x20);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX9_SPKR_PROT_PATH_CTL, 0x0F, 0x00);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX10_SPKR_PROT_PATH_CTL, 0x0F,
				0x00);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX9_SPKR_PROT_PATH_CTL, 0x10, 0x10);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX10_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x00);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
		}
		if (test_bit(VI_SENSE_2, &tavil_p->status_mask)) {
			pr_debug("%s: spkr2 enabled\n", __func__);
			/* Enable V&I sensing */
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX11_SPKR_PROT_PATH_CTL, 0x0F,
				0x00);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX12_SPKR_PROT_PATH_CTL, 0x0F,
				0x00);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX11_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX12_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
		}
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
		if (ret)
			dev_err(codec->dev, "%s error in close_slim_sch_tx %d\n",
				__func__, ret);
		if (!dai->bus_down_in_recovery)
			ret = tavil_codec_enable_slim_chmask(dai, false);
		if (ret < 0) {
			ret = wcd9xxx_disconnect_port(core,
				&dai->wcd9xxx_ch_list,
				dai->grph);
			dev_dbg(codec->dev, "%s: Disconnect TX port, ret = %d\n",
				__func__, ret);
		}
		if (test_bit(VI_SENSE_1, &tavil_p->status_mask)) {
			/* Disable V&I sensing */
			dev_dbg(codec->dev, "%s: spkr1 disabled\n", __func__);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x20);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX9_SPKR_PROT_PATH_CTL, 0x10, 0x00);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX10_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
		}
		if (test_bit(VI_SENSE_2, &tavil_p->status_mask)) {
			/* Disable V&I sensing */
			dev_dbg(codec->dev, "%s: spkr2 disabled\n", __func__);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX11_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
			snd_soc_update_bits(codec,
				WCD934X_CDC_TX12_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
		}
		break;
	}
done:
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

static void tavil_spk_anc_update_callback(struct work_struct *work)
{
	struct spk_anc_work *spk_anc_dwork;
	struct tavil_priv *tavil;
	struct delayed_work *delayed_work;
	struct snd_soc_codec *codec;

	delayed_work = to_delayed_work(work);
	spk_anc_dwork = container_of(delayed_work, struct spk_anc_work, dwork);
	tavil = spk_anc_dwork->tavil;
	codec = tavil->codec;

	snd_soc_update_bits(codec, WCD934X_CDC_RX7_RX_PATH_CFG0, 0x10, 0x10);
}

static int tavil_codec_enable_spkr_anc(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	if (!tavil->anc_func)
		return 0;

	dev_dbg(codec->dev, "%s: w: %s event: %d anc: %d\n", __func__,
		w->name, event, tavil->anc_func);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = tavil_codec_enable_anc(w, kcontrol, event);
		schedule_delayed_work(&tavil->spk_anc_dwork.dwork,
				      msecs_to_jiffies(spk_anc_en_delay));
		break;
	case SND_SOC_DAPM_POST_PMD:
		cancel_delayed_work_sync(&tavil->spk_anc_dwork.dwork);
		snd_soc_update_bits(codec, WCD934X_CDC_RX7_RX_PATH_CFG0,
				    0x10, 0x00);
		ret = tavil_codec_enable_anc(w, kcontrol, event);
		break;
	}
	return ret;
}

static int tavil_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	int ret = 0;
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
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * 5ms sleep is required after PA is disabled as per
		 * HW requirement
		 */
		usleep_range(5000, 5500);

		if (!(strcmp(w->name, "ANC EAR PA"))) {
			ret = tavil_codec_enable_anc(w, kcontrol, event);
			snd_soc_update_bits(codec, WCD934X_CDC_RX0_RX_PATH_CFG0,
					    0x10, 0x00);
		}
		break;
	};

	return ret;
}

static void tavil_codec_override(struct snd_soc_codec *codec, int mode,
				 int event)
{
	if (mode == CLS_AB || mode == CLS_AB_HIFI) {
		switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
		case SND_SOC_DAPM_POST_PMU:
			snd_soc_update_bits(codec,
				WCD9XXX_A_ANA_RX_SUPPLIES, 0x02, 0x02);
		break;
		case SND_SOC_DAPM_POST_PMD:
			snd_soc_update_bits(codec,
				WCD9XXX_A_ANA_RX_SUPPLIES, 0x02, 0x00);
		break;
		}
	}
}

static void tavil_codec_clear_anc_tx_hold(struct tavil_priv *tavil)
{
	if (test_and_clear_bit(ANC_MIC_AMIC1, &tavil->status_mask))
		tavil_codec_set_tx_hold(tavil->codec, WCD934X_ANA_AMIC1, false);
	if (test_and_clear_bit(ANC_MIC_AMIC2, &tavil->status_mask))
		tavil_codec_set_tx_hold(tavil->codec, WCD934X_ANA_AMIC2, false);
	if (test_and_clear_bit(ANC_MIC_AMIC3, &tavil->status_mask))
		tavil_codec_set_tx_hold(tavil->codec, WCD934X_ANA_AMIC3, false);
	if (test_and_clear_bit(ANC_MIC_AMIC4, &tavil->status_mask))
		tavil_codec_set_tx_hold(tavil->codec, WCD934X_ANA_AMIC4, false);
}

static int tavil_codec_enable_hphr_pa(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	struct tavil_dsd_config *dsd_conf = tavil->dsd_config;
	int ret = 0;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (TAVIL_IS_1_0(tavil->wcd9xxx))
			snd_soc_update_bits(codec, WCD934X_HPH_REFBUFF_LP_CTL,
					    0x06, (0x03 << 1));

		if ((!(strcmp(w->name, "ANC HPHR PA"))) &&
		    (test_bit(HPH_PA_DELAY, &tavil->status_mask)))
			snd_soc_update_bits(codec, WCD934X_ANA_HPH, 0xC0, 0xC0);

		set_bit(HPH_PA_DELAY, &tavil->status_mask);
		if (dsd_conf &&
		    (snd_soc_read(codec, WCD934X_CDC_DSD1_PATH_CTL) & 0x01)) {
			/* Set regulator mode to AB if DSD is enabled */
			snd_soc_update_bits(codec, WCD934X_ANA_RX_SUPPLIES,
					    0x02, 0x02);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		if ((!(strcmp(w->name, "ANC HPHR PA")))) {
			if ((snd_soc_read(codec, WCD934X_ANA_HPH) & 0xC0)
					!= 0xC0)
				/*
				 * If PA_EN is not set (potentially in ANC case)
				 * then do nothing for POST_PMU and let left
				 * channel handle everything.
				 */
				break;
		}
		/*
		 * 7ms sleep is required after PA is enabled as per
		 * HW requirement. If compander is disabled, then
		 * 20ms delay is needed.
		 */
		if (test_bit(HPH_PA_DELAY, &tavil->status_mask)) {
			if (!tavil->comp_enabled[COMPANDER_2])
				usleep_range(20000, 20100);
			else
				usleep_range(7000, 7100);
			clear_bit(HPH_PA_DELAY, &tavil->status_mask);
		}
		if (tavil->anc_func) {
			/* Clear Tx FE HOLD if both PAs are enabled */
			if ((snd_soc_read(tavil->codec, WCD934X_ANA_HPH) &
					0xC0) == 0xC0)
				tavil_codec_clear_anc_tx_hold(tavil);
		}

		snd_soc_update_bits(codec, WCD934X_HPH_R_TEST, 0x01, 0x01);

		/* Remove mute */
		snd_soc_update_bits(codec, WCD934X_CDC_RX2_RX_PATH_CTL,
				    0x10, 0x00);
		/* Enable GM3 boost */
		snd_soc_update_bits(codec, WCD934X_HPH_CNP_WG_CTL,
				    0x80, 0x80);
		/* Enable AutoChop timer at the end of power up */
		snd_soc_update_bits(codec, WCD934X_HPH_NEW_INT_HPH_TIMER1,
				    0x02, 0x02);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_read(codec, WCD934X_CDC_RX2_RX_PATH_MIX_CTL)) &
				  0x10)
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX2_RX_PATH_MIX_CTL,
					    0x10, 0x00);
		if (dsd_conf &&
		    (snd_soc_read(codec, WCD934X_CDC_DSD1_PATH_CTL) & 0x01))
			snd_soc_update_bits(codec, WCD934X_CDC_DSD1_CFG2,
					    0x04, 0x00);
		if (!(strcmp(w->name, "ANC HPHR PA"))) {
			pr_debug("%s:Do everything needed for left channel\n",
				__func__);
			/* Do everything needed for left channel */
			snd_soc_update_bits(codec, WCD934X_HPH_L_TEST,
					    0x01, 0x01);

			/* Remove mute */
			snd_soc_update_bits(codec, WCD934X_CDC_RX1_RX_PATH_CTL,
					    0x10, 0x00);

			/* Remove mix path mute if it is enabled */
			if ((snd_soc_read(codec,
					WCD934X_CDC_RX1_RX_PATH_MIX_CTL)) &
					0x10)
				snd_soc_update_bits(codec,
					WCD934X_CDC_RX1_RX_PATH_MIX_CTL,
					0x10, 0x00);

			if (dsd_conf && (snd_soc_read(codec,
						WCD934X_CDC_DSD0_PATH_CTL) &
						0x01))
				snd_soc_update_bits(codec,
						    WCD934X_CDC_DSD0_CFG2,
						    0x04, 0x00);
			/* Remove ANC Rx from reset */
			ret = tavil_codec_enable_anc(w, kcontrol, event);
		}
		tavil_codec_override(codec, tavil->hph_mode, event);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		blocking_notifier_call_chain(&tavil->mbhc->notifier,
					     WCD_EVENT_PRE_HPHR_PA_OFF,
					     &tavil->mbhc->wcd_mbhc);
		/* Enable DSD Mute before PA disable */
		if (dsd_conf &&
		    (snd_soc_read(codec, WCD934X_CDC_DSD1_PATH_CTL) & 0x01))
			snd_soc_update_bits(codec, WCD934X_CDC_DSD1_CFG2,
					    0x04, 0x04);
		snd_soc_update_bits(codec, WCD934X_HPH_R_TEST, 0x01, 0x00);
		snd_soc_update_bits(codec, WCD934X_CDC_RX2_RX_PATH_CTL,
				    0x10, 0x10);
		snd_soc_update_bits(codec, WCD934X_CDC_RX2_RX_PATH_MIX_CTL,
				    0x10, 0x10);
		if (!(strcmp(w->name, "ANC HPHR PA")))
			snd_soc_update_bits(codec, WCD934X_ANA_HPH, 0x40, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * 5ms sleep is required after PA disable. If compander is
		 * disabled, then 20ms delay is needed after PA disable.
		 */
		if (!tavil->comp_enabled[COMPANDER_2])
			usleep_range(20000, 20100);
		else
			usleep_range(5000, 5100);
		tavil_codec_override(codec, tavil->hph_mode, event);
		blocking_notifier_call_chain(&tavil->mbhc->notifier,
					     WCD_EVENT_POST_HPHR_PA_OFF,
					     &tavil->mbhc->wcd_mbhc);
		if (TAVIL_IS_1_0(tavil->wcd9xxx))
			snd_soc_update_bits(codec, WCD934X_HPH_REFBUFF_LP_CTL,
					    0x06, 0x0);
		if (!(strcmp(w->name, "ANC HPHR PA"))) {
			ret = tavil_codec_enable_anc(w, kcontrol, event);
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX2_RX_PATH_CFG0,
					    0x10, 0x00);
		}
		break;
	};

	return ret;
}

static int tavil_codec_enable_hphl_pa(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	struct tavil_dsd_config *dsd_conf = tavil->dsd_config;
	int ret = 0;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (TAVIL_IS_1_0(tavil->wcd9xxx))
			snd_soc_update_bits(codec, WCD934X_HPH_REFBUFF_LP_CTL,
					    0x06, (0x03 << 1));
		if ((!(strcmp(w->name, "ANC HPHL PA"))) &&
		    (test_bit(HPH_PA_DELAY, &tavil->status_mask)))
			snd_soc_update_bits(codec, WCD934X_ANA_HPH,
					    0xC0, 0xC0);
		set_bit(HPH_PA_DELAY, &tavil->status_mask);
		if (dsd_conf &&
		    (snd_soc_read(codec, WCD934X_CDC_DSD0_PATH_CTL) & 0x01)) {
			/* Set regulator mode to AB if DSD is enabled */
			snd_soc_update_bits(codec, WCD934X_ANA_RX_SUPPLIES,
					    0x02, 0x02);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (!(strcmp(w->name, "ANC HPHL PA"))) {
			if ((snd_soc_read(codec, WCD934X_ANA_HPH) & 0xC0)
								!= 0xC0)
				/*
				 * If PA_EN is not set (potentially in ANC
				 * case) then do nothing for POST_PMU and
				 * let right channel handle everything.
				 */
				break;
		}
		/*
		 * 7ms sleep is required after PA is enabled as per
		 * HW requirement. If compander is disabled, then
		 * 20ms delay is needed.
		 */
		if (test_bit(HPH_PA_DELAY, &tavil->status_mask)) {
			if (!tavil->comp_enabled[COMPANDER_1])
				usleep_range(20000, 20100);
			else
				usleep_range(7000, 7100);
			clear_bit(HPH_PA_DELAY, &tavil->status_mask);
		}
		if (tavil->anc_func) {
			/* Clear Tx FE HOLD if both PAs are enabled */
			if ((snd_soc_read(tavil->codec, WCD934X_ANA_HPH) &
					0xC0) == 0xC0)
				tavil_codec_clear_anc_tx_hold(tavil);
		}

		snd_soc_update_bits(codec, WCD934X_HPH_L_TEST, 0x01, 0x01);
		/* Remove Mute on primary path */
		snd_soc_update_bits(codec, WCD934X_CDC_RX1_RX_PATH_CTL,
				    0x10, 0x00);
		/* Enable GM3 boost */
		snd_soc_update_bits(codec, WCD934X_HPH_CNP_WG_CTL,
				    0x80, 0x80);
		/* Enable AutoChop timer at the end of power up */
		snd_soc_update_bits(codec, WCD934X_HPH_NEW_INT_HPH_TIMER1,
				    0x02, 0x02);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_read(codec, WCD934X_CDC_RX1_RX_PATH_MIX_CTL)) &
				  0x10)
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX1_RX_PATH_MIX_CTL,
					    0x10, 0x00);
		if (dsd_conf &&
		    (snd_soc_read(codec, WCD934X_CDC_DSD0_PATH_CTL) & 0x01))
			snd_soc_update_bits(codec, WCD934X_CDC_DSD0_CFG2,
					    0x04, 0x00);
		if (!(strcmp(w->name, "ANC HPHL PA"))) {
			pr_debug("%s:Do everything needed for right channel\n",
				__func__);

			/* Do everything needed for right channel */
			snd_soc_update_bits(codec, WCD934X_HPH_R_TEST,
					    0x01, 0x01);

			/* Remove mute */
			snd_soc_update_bits(codec, WCD934X_CDC_RX2_RX_PATH_CTL,
						0x10, 0x00);

			/* Remove mix path mute if it is enabled */
			if ((snd_soc_read(codec,
					WCD934X_CDC_RX2_RX_PATH_MIX_CTL)) &
					0x10)
				snd_soc_update_bits(codec,
						WCD934X_CDC_RX2_RX_PATH_MIX_CTL,
						0x10, 0x00);
			if (dsd_conf && (snd_soc_read(codec,
					WCD934X_CDC_DSD1_PATH_CTL) & 0x01))
				snd_soc_update_bits(codec,
						    WCD934X_CDC_DSD1_CFG2,
						    0x04, 0x00);
			/* Remove ANC Rx from reset */
			ret = tavil_codec_enable_anc(w, kcontrol, event);
		}
		tavil_codec_override(codec, tavil->hph_mode, event);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		blocking_notifier_call_chain(&tavil->mbhc->notifier,
					     WCD_EVENT_PRE_HPHL_PA_OFF,
					     &tavil->mbhc->wcd_mbhc);
		/* Enable DSD Mute before PA disable */
		if (dsd_conf &&
		    (snd_soc_read(codec, WCD934X_CDC_DSD0_PATH_CTL) & 0x01))
			snd_soc_update_bits(codec, WCD934X_CDC_DSD0_CFG2,
					    0x04, 0x04);

		snd_soc_update_bits(codec, WCD934X_HPH_L_TEST, 0x01, 0x00);
		snd_soc_update_bits(codec, WCD934X_CDC_RX1_RX_PATH_CTL,
				    0x10, 0x10);
		snd_soc_update_bits(codec, WCD934X_CDC_RX1_RX_PATH_MIX_CTL,
				    0x10, 0x10);
		if (!(strcmp(w->name, "ANC HPHL PA")))
			snd_soc_update_bits(codec, WCD934X_ANA_HPH,
					    0x80, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * 5ms sleep is required after PA disable. If compander is
		 * disabled, then 20ms delay is needed after PA disable.
		 */
		if (!tavil->comp_enabled[COMPANDER_1])
			usleep_range(20000, 20100);
		else
			usleep_range(5000, 5100);
		tavil_codec_override(codec, tavil->hph_mode, event);
		blocking_notifier_call_chain(&tavil->mbhc->notifier,
					     WCD_EVENT_POST_HPHL_PA_OFF,
					     &tavil->mbhc->wcd_mbhc);
		if (TAVIL_IS_1_0(tavil->wcd9xxx))
			snd_soc_update_bits(codec, WCD934X_HPH_REFBUFF_LP_CTL,
					    0x06, 0x0);
		if (!(strcmp(w->name, "ANC HPHL PA"))) {
			ret = tavil_codec_enable_anc(w, kcontrol, event);
			snd_soc_update_bits(codec,
				WCD934X_CDC_RX1_RX_PATH_CFG0, 0x10, 0x00);
		}
		break;
	};

	return ret;
}

static int tavil_codec_enable_lineout_pa(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 lineout_vol_reg = 0, lineout_mix_vol_reg = 0;
	u16 dsd_mute_reg = 0, dsd_clk_reg = 0;
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	struct tavil_dsd_config *dsd_conf = tavil->dsd_config;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	if (w->reg == WCD934X_ANA_LO_1_2) {
		if (w->shift == 7) {
			lineout_vol_reg = WCD934X_CDC_RX3_RX_PATH_CTL;
			lineout_mix_vol_reg = WCD934X_CDC_RX3_RX_PATH_MIX_CTL;
			dsd_mute_reg = WCD934X_CDC_DSD0_CFG2;
			dsd_clk_reg = WCD934X_CDC_DSD0_PATH_CTL;
		} else if (w->shift == 6) {
			lineout_vol_reg = WCD934X_CDC_RX4_RX_PATH_CTL;
			lineout_mix_vol_reg = WCD934X_CDC_RX4_RX_PATH_MIX_CTL;
			dsd_mute_reg = WCD934X_CDC_DSD1_CFG2;
			dsd_clk_reg = WCD934X_CDC_DSD1_PATH_CTL;
		}
	} else {
		dev_err(codec->dev, "%s: Error enabling lineout PA\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tavil_codec_override(codec, CLS_AB, event);
		break;
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
		if (dsd_conf && (snd_soc_read(codec, dsd_clk_reg) & 0x01))
			snd_soc_update_bits(codec, dsd_mute_reg, 0x04, 0x00);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (dsd_conf && (snd_soc_read(codec, dsd_clk_reg) & 0x01))
			snd_soc_update_bits(codec, dsd_mute_reg, 0x04, 0x04);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * 5ms sleep is required after PA is disabled as per
		 * HW requirement
		 */
		usleep_range(5000, 5500);
		tavil_codec_override(codec, CLS_AB, event);
	default:
		break;
	};

	return 0;
}

static int tavil_codec_ear_dac_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Disable AutoChop timer during power up */
		snd_soc_update_bits(codec, WCD934X_HPH_NEW_INT_HPH_TIMER1,
				    0x02, 0x00);

		if (tavil->anc_func)
			ret = tavil_codec_enable_anc(w, kcontrol, event);

		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_EAR,
			     CLS_H_NORMAL);
		if (tavil->anc_func)
			snd_soc_update_bits(codec, WCD934X_CDC_RX0_RX_PATH_CFG0,
					    0x10, 0x10);
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

	return ret;
}

static int tavil_codec_hphr_dac_event(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int hph_mode = tavil->hph_mode;
	u8 dem_inp;
	struct tavil_dsd_config *dsd_conf = tavil->dsd_config;
	int ret = 0;

	dev_dbg(codec->dev, "%s wname: %s event: %d hph_mode: %d\n", __func__,
		w->name, event, hph_mode);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (tavil->anc_func) {
			ret = tavil_codec_enable_anc(w, kcontrol, event);
			/* 40 msec delay is needed to avoid click and pop */
			msleep(40);
		}
		/* Read DEM INP Select */
		dem_inp = snd_soc_read(codec, WCD934X_CDC_RX2_RX_PATH_SEC0) &
			  0x03;
		if (((hph_mode == CLS_H_HIFI) || (hph_mode == CLS_H_LOHIFI) ||
		     (hph_mode == CLS_H_LP)) && (dem_inp != 0x01)) {
			dev_err(codec->dev, "%s: DEM Input not set correctly, hph_mode: %d\n",
					__func__, hph_mode);
			return -EINVAL;
		}
		if ((hph_mode != CLS_H_LP) && (hph_mode != CLS_H_ULP))
			/* Ripple freq control enable */
			snd_soc_update_bits(codec,
					     WCD934X_SIDO_NEW_VOUT_D_FREQ2,
					     0x01, 0x01);
		/* Disable AutoChop timer during power up */
		snd_soc_update_bits(codec, WCD934X_HPH_NEW_INT_HPH_TIMER1,
				    0x02, 0x00);
		/* Set RDAC gain */
		if (TAVIL_IS_1_0(tavil->wcd9xxx))
			snd_soc_update_bits(codec,
					    WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL,
					    0xF0, 0x40);
		if (dsd_conf &&
		    (snd_soc_read(codec, WCD934X_CDC_DSD1_PATH_CTL) & 0x01))
			hph_mode = CLS_H_HIFI;

		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_HPHR,
			     hph_mode);
		if (tavil->anc_func)
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX2_RX_PATH_CFG0,
					    0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 1000us required as per HW requirement */
		usleep_range(1000, 1100);
		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_HPHR,
			     hph_mode);
		if ((hph_mode != CLS_H_LP) && (hph_mode != CLS_H_ULP))
			/* Ripple freq control disable */
			snd_soc_update_bits(codec,
					    WCD934X_SIDO_NEW_VOUT_D_FREQ2,
					    0x01, 0x0);
		/* Re-set RDAC gain */
		if (TAVIL_IS_1_0(tavil->wcd9xxx))
			snd_soc_update_bits(codec,
					    WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL,
					    0xF0, 0x0);
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
	struct tavil_dsd_config *dsd_conf = tavil->dsd_config;
	uint32_t impedl = 0, impedr = 0;

	dev_dbg(codec->dev, "%s wname: %s event: %d hph_mode: %d\n", __func__,
		w->name, event, hph_mode);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (tavil->anc_func) {
			ret = tavil_codec_enable_anc(w, kcontrol, event);
			/* 40 msec delay is needed to avoid click and pop */
			msleep(40);
		}
		/* Read DEM INP Select */
		dem_inp = snd_soc_read(codec, WCD934X_CDC_RX1_RX_PATH_SEC0) &
			  0x03;
		if (((hph_mode == CLS_H_HIFI) || (hph_mode == CLS_H_LOHIFI) ||
		     (hph_mode == CLS_H_LP)) && (dem_inp != 0x01)) {
			dev_err(codec->dev, "%s: DEM Input not set correctly, hph_mode: %d\n",
					__func__, hph_mode);
			return -EINVAL;
		}
		if ((hph_mode != CLS_H_LP) && (hph_mode != CLS_H_ULP))
			/* Ripple freq control enable */
			snd_soc_update_bits(codec,
					     WCD934X_SIDO_NEW_VOUT_D_FREQ2,
					     0x01, 0x01);
		/* Disable AutoChop timer during power up */
		snd_soc_update_bits(codec, WCD934X_HPH_NEW_INT_HPH_TIMER1,
				    0x02, 0x00);
		/* Set RDAC gain */
		if (TAVIL_IS_1_0(tavil->wcd9xxx))
			snd_soc_update_bits(codec,
					    WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL,
					    0xF0, 0x40);
		if (dsd_conf &&
		    (snd_soc_read(codec, WCD934X_CDC_DSD0_PATH_CTL) & 0x01))
			hph_mode = CLS_H_HIFI;

		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_HPHL,
			     hph_mode);

		if (tavil->anc_func)
			snd_soc_update_bits(codec,
					    WCD934X_CDC_RX1_RX_PATH_CFG0,
					    0x10, 0x10);

		ret = tavil_mbhc_get_impedance(tavil->mbhc,
					       &impedl, &impedr);
		if (!ret) {
			wcd_clsh_imped_config(codec, impedl, false);
			set_bit(CLSH_Z_CONFIG, &tavil->status_mask);
		} else {
			dev_dbg(codec->dev, "%s: Failed to get mbhc impedance %d\n",
				__func__, ret);
			ret = 0;
		}

		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 1000us required as per HW requirement */
		usleep_range(1000, 1100);
		wcd_clsh_fsm(codec, &tavil->clsh_d,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_HPHL,
			     hph_mode);
		if ((hph_mode != CLS_H_LP) && (hph_mode != CLS_H_ULP))
			/* Ripple freq control disable */
			snd_soc_update_bits(codec,
					    WCD934X_SIDO_NEW_VOUT_D_FREQ2,
					    0x01, 0x0);
		/* Re-set RDAC gain */
		if (TAVIL_IS_1_0(tavil->wcd9xxx))
			snd_soc_update_bits(codec,
					    WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL,
					    0xF0, 0x0);

		if (test_bit(CLSH_Z_CONFIG, &tavil->status_mask)) {
			wcd_clsh_imped_config(codec, impedl, true);
			clear_bit(CLSH_Z_CONFIG, &tavil->status_mask);
		}
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
	int ch_cnt = 0;

	tavil = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (((strnstr(w->name, "INT7_", sizeof("RX INT7_"))) ||
			(strnstr(w->name, "INT7 MIX2",
						sizeof("RX INT7 MIX2")))))
			tavil->swr.rx_7_count++;
		if ((strnstr(w->name, "INT8_", sizeof("RX INT8_"))) &&
		    !tavil->swr.rx_8_count)
			tavil->swr.rx_8_count++;
		ch_cnt = !!(tavil->swr.rx_7_count) + tavil->swr.rx_8_count;

		swrm_wcd_notify(tavil->swr.ctrl_data[0].swr_pdev,
				SWR_DEVICE_UP, NULL);
		swrm_wcd_notify(tavil->swr.ctrl_data[0].swr_pdev,
				SWR_SET_NUM_RX_CH, &ch_cnt);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if ((strnstr(w->name, "INT7_", sizeof("RX INT7_")))  ||
			(strnstr(w->name, "INT7 MIX2",
			sizeof("RX INT7 MIX2"))))
			tavil->swr.rx_7_count--;
		if ((strnstr(w->name, "INT8_", sizeof("RX INT8_"))) &&
		    tavil->swr.rx_8_count)
			tavil->swr.rx_8_count--;
		ch_cnt = !!(tavil->swr.rx_7_count) + tavil->swr.rx_8_count;

		swrm_wcd_notify(tavil->swr.ctrl_data[0].swr_pdev,
				SWR_SET_NUM_RX_CH, &ch_cnt);

		break;
	}
	dev_dbg(tavil->dev, "%s: %s: current swr ch cnt: %d\n",
		__func__, w->name, ch_cnt);

	return 0;
}

static int tavil_codec_enable_swr(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	return __tavil_codec_enable_swr(w, event);
}

static int tavil_codec_config_mad(struct snd_soc_codec *codec)
{
	int ret = 0;
	int idx;
	const struct firmware *fw;
	struct firmware_cal *hwdep_cal = NULL;
	struct wcd_mad_audio_cal *mad_cal = NULL;
	const void *data;
	const char *filename = WCD934X_MAD_AUDIO_FIRMWARE_PATH;
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	size_t cal_size;

	hwdep_cal = wcdcal_get_fw_cal(tavil->fw_data, WCD9XXX_MAD_CAL);
	if (hwdep_cal) {
		data = hwdep_cal->data;
		cal_size = hwdep_cal->size;
		dev_dbg(codec->dev, "%s: using hwdep calibration\n",
			__func__);
	} else {
		ret = request_firmware(&fw, filename, codec->dev);
		if (ret || !fw) {
			dev_err(codec->dev,
				"%s: MAD firmware acquire failed, err = %d\n",
				__func__, ret);
			return -ENODEV;
		}
		data = fw->data;
		cal_size = fw->size;
		dev_dbg(codec->dev, "%s: using request_firmware calibration\n",
			__func__);
	}

	if (cal_size < sizeof(*mad_cal)) {
		dev_err(codec->dev,
			"%s: Incorrect size %zd for MAD Cal, expected %zd\n",
			__func__, cal_size, sizeof(*mad_cal));
		ret = -ENOMEM;
		goto done;
	}

	mad_cal = (struct wcd_mad_audio_cal *) (data);
	if (!mad_cal) {
		dev_err(codec->dev,
			"%s: Invalid calibration data\n",
			__func__);
		ret = -EINVAL;
		goto done;
	}

	snd_soc_write(codec, WCD934X_SOC_MAD_MAIN_CTL_2,
		      mad_cal->microphone_info.cycle_time);
	snd_soc_update_bits(codec, WCD934X_SOC_MAD_MAIN_CTL_1, 0xFF << 3,
			    ((uint16_t)mad_cal->microphone_info.settle_time)
			    << 3);

	/* Audio */
	snd_soc_write(codec, WCD934X_SOC_MAD_AUDIO_CTL_8,
		      mad_cal->audio_info.rms_omit_samples);
	snd_soc_update_bits(codec, WCD934X_SOC_MAD_AUDIO_CTL_1,
			    0x07 << 4, mad_cal->audio_info.rms_comp_time << 4);
	snd_soc_update_bits(codec, WCD934X_SOC_MAD_AUDIO_CTL_2, 0x03 << 2,
			    mad_cal->audio_info.detection_mechanism << 2);
	snd_soc_write(codec, WCD934X_SOC_MAD_AUDIO_CTL_7,
		      mad_cal->audio_info.rms_diff_threshold & 0x3F);
	snd_soc_write(codec, WCD934X_SOC_MAD_AUDIO_CTL_5,
		      mad_cal->audio_info.rms_threshold_lsb);
	snd_soc_write(codec, WCD934X_SOC_MAD_AUDIO_CTL_6,
		      mad_cal->audio_info.rms_threshold_msb);

	for (idx = 0; idx < ARRAY_SIZE(mad_cal->audio_info.iir_coefficients);
	     idx++) {
		snd_soc_update_bits(codec, WCD934X_SOC_MAD_AUDIO_IIR_CTL_PTR,
				    0x3F, idx);
		snd_soc_write(codec, WCD934X_SOC_MAD_AUDIO_IIR_CTL_VAL,
			      mad_cal->audio_info.iir_coefficients[idx]);
		dev_dbg(codec->dev, "%s:MAD Audio IIR Coef[%d] = 0X%x",
			__func__, idx,
			mad_cal->audio_info.iir_coefficients[idx]);
	}

	/* Beacon */
	snd_soc_write(codec, WCD934X_SOC_MAD_BEACON_CTL_8,
		      mad_cal->beacon_info.rms_omit_samples);
	snd_soc_update_bits(codec, WCD934X_SOC_MAD_BEACON_CTL_1,
			    0x07 << 4, mad_cal->beacon_info.rms_comp_time << 4);
	snd_soc_update_bits(codec, WCD934X_SOC_MAD_BEACON_CTL_2, 0x03 << 2,
			    mad_cal->beacon_info.detection_mechanism << 2);
	snd_soc_write(codec, WCD934X_SOC_MAD_BEACON_CTL_7,
		      mad_cal->beacon_info.rms_diff_threshold & 0x1F);
	snd_soc_write(codec, WCD934X_SOC_MAD_BEACON_CTL_5,
		      mad_cal->beacon_info.rms_threshold_lsb);
	snd_soc_write(codec, WCD934X_SOC_MAD_BEACON_CTL_6,
		      mad_cal->beacon_info.rms_threshold_msb);

	for (idx = 0; idx < ARRAY_SIZE(mad_cal->beacon_info.iir_coefficients);
	     idx++) {
		snd_soc_update_bits(codec, WCD934X_SOC_MAD_BEACON_IIR_CTL_PTR,
				    0x3F, idx);
		snd_soc_write(codec, WCD934X_SOC_MAD_BEACON_IIR_CTL_VAL,
			      mad_cal->beacon_info.iir_coefficients[idx]);
		dev_dbg(codec->dev, "%s:MAD Beacon IIR Coef[%d] = 0X%x",
			__func__, idx,
			mad_cal->beacon_info.iir_coefficients[idx]);
	}

	/* Ultrasound */
	snd_soc_update_bits(codec, WCD934X_SOC_MAD_ULTR_CTL_1,
			    0x07 << 4,
			    mad_cal->ultrasound_info.rms_comp_time << 4);
	snd_soc_update_bits(codec, WCD934X_SOC_MAD_ULTR_CTL_2, 0x03 << 2,
			    mad_cal->ultrasound_info.detection_mechanism << 2);
	snd_soc_write(codec, WCD934X_SOC_MAD_ULTR_CTL_7,
		      mad_cal->ultrasound_info.rms_diff_threshold & 0x1F);
	snd_soc_write(codec, WCD934X_SOC_MAD_ULTR_CTL_5,
		      mad_cal->ultrasound_info.rms_threshold_lsb);
	snd_soc_write(codec, WCD934X_SOC_MAD_ULTR_CTL_6,
		      mad_cal->ultrasound_info.rms_threshold_msb);

done:
	if (!hwdep_cal)
		release_firmware(fw);

	return ret;
}

static int __tavil_codec_enable_mad(struct snd_soc_codec *codec, bool enable)
{
	int rc = 0;

	/* Return if CPE INPUT is DEC1 */
	if (snd_soc_read(codec, WCD934X_CPE_SS_SVA_CFG) & 0x04) {
		dev_dbg(codec->dev, "%s: MAD is bypassed, skip mad %s\n",
			__func__, enable ? "enable" : "disable");
		return rc;
	}

	dev_dbg(codec->dev, "%s: enable = %s\n", __func__,
		enable ? "enable" : "disable");

	if (enable) {
		snd_soc_update_bits(codec, WCD934X_SOC_MAD_AUDIO_CTL_2,
				    0x03, 0x03);
		rc = tavil_codec_config_mad(codec);
		if (IS_ERR_VALUE(rc)) {
			snd_soc_update_bits(codec, WCD934X_SOC_MAD_AUDIO_CTL_2,
					    0x03, 0x00);
			goto done;
		}

		/* Turn on MAD clk */
		snd_soc_update_bits(codec, WCD934X_CPE_SS_MAD_CTL,
				    0x01, 0x01);

		/* Undo reset for MAD */
		snd_soc_update_bits(codec, WCD934X_CPE_SS_MAD_CTL,
				    0x02, 0x00);
		snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_MCLK_CFG,
					0x04, 0x04);
	} else {
		snd_soc_update_bits(codec, WCD934X_SOC_MAD_AUDIO_CTL_2,
				    0x03, 0x00);
		/* Reset the MAD block */
		snd_soc_update_bits(codec, WCD934X_CPE_SS_MAD_CTL,
				    0x02, 0x02);
		/* Turn off MAD clk */
		snd_soc_update_bits(codec, WCD934X_CPE_SS_MAD_CTL,
				    0x01, 0x00);
		snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_MCLK_CFG,
					0x04, 0x00);
	}
done:
	return rc;
}

static int tavil_codec_ape_enable_mad(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int rc = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, WCD934X_CPE_SS_SVA_CFG, 0x40, 0x40);
		rc = __tavil_codec_enable_mad(codec, true);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, WCD934X_CPE_SS_SVA_CFG, 0x40, 0x00);
		__tavil_codec_enable_mad(codec, false);
		break;
	}

	dev_dbg(tavil->dev, "%s: event = %d\n", __func__, event);
	return rc;
}

static int tavil_codec_cpe_mad_ctl(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int rc = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tavil->mad_switch_cnt++;
		if (tavil->mad_switch_cnt != 1)
			goto done;

		snd_soc_update_bits(codec, WCD934X_CPE_SS_SVA_CFG, 0x20, 0x20);
		rc = __tavil_codec_enable_mad(codec, true);
		if (IS_ERR_VALUE(rc)) {
			tavil->mad_switch_cnt--;
			goto done;
		}

		break;
	case SND_SOC_DAPM_PRE_PMD:
		tavil->mad_switch_cnt--;
		if (tavil->mad_switch_cnt != 0)
			goto done;

		snd_soc_update_bits(codec, WCD934X_CPE_SS_SVA_CFG, 0x20, 0x00);
		__tavil_codec_enable_mad(codec, false);
		break;
	}
done:
	dev_dbg(tavil->dev, "%s: event = %d, mad_switch_cnt = %d\n",
		__func__, event, tavil->mad_switch_cnt);
	return rc;
}

static int tavil_get_asrc_mode(struct tavil_priv *tavil, int asrc,
			       u8 main_sr, u8 mix_sr)
{
	u8 asrc_output_mode;
	int asrc_mode = CONV_88P2K_TO_384K;

	if ((asrc < 0) || (asrc >= ASRC_MAX))
		return 0;

	asrc_output_mode = tavil->asrc_output_mode[asrc];

	if (asrc_output_mode) {
		/*
		 * If Mix sample rate is < 96KHz, use 96K to 352.8K
		 * conversion, or else use 384K to 352.8K conversion
		 */
		if (mix_sr < 5)
			asrc_mode = CONV_96K_TO_352P8K;
		else
			asrc_mode = CONV_384K_TO_352P8K;
	} else {
		/* Integer main and Fractional mix path */
		if (main_sr < 8 && mix_sr > 9) {
			asrc_mode = CONV_352P8K_TO_384K;
		} else if (main_sr > 8 && mix_sr < 8) {
			/* Fractional main and Integer mix path */
			if (mix_sr < 5)
				asrc_mode = CONV_96K_TO_352P8K;
			else
				asrc_mode = CONV_384K_TO_352P8K;
		} else if (main_sr < 8 && mix_sr < 8) {
			/* Integer main and Integer mix path */
			asrc_mode = CONV_96K_TO_384K;
		}
	}

	return asrc_mode;
}

static int tavil_codec_enable_asrc(struct snd_soc_codec *codec,
				   int asrc_in, int event)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	u16 cfg_reg, ctl_reg, clk_reg, asrc_ctl, mix_ctl_reg;
	int asrc, ret = 0;
	u8 main_sr, mix_sr, asrc_mode = 0;

	switch (asrc_in) {
	case ASRC_IN_HPHL:
		cfg_reg = WCD934X_CDC_RX1_RX_PATH_CFG0;
		ctl_reg = WCD934X_CDC_RX1_RX_PATH_CTL;
		clk_reg = WCD934X_MIXING_ASRC0_CLK_RST_CTL;
		asrc_ctl = WCD934X_MIXING_ASRC0_CTL1;
		asrc = ASRC0;
		break;
	case ASRC_IN_LO1:
		cfg_reg = WCD934X_CDC_RX3_RX_PATH_CFG0;
		ctl_reg = WCD934X_CDC_RX3_RX_PATH_CTL;
		clk_reg = WCD934X_MIXING_ASRC0_CLK_RST_CTL;
		asrc_ctl = WCD934X_MIXING_ASRC0_CTL1;
		asrc = ASRC0;
		break;
	case ASRC_IN_HPHR:
		cfg_reg = WCD934X_CDC_RX2_RX_PATH_CFG0;
		ctl_reg = WCD934X_CDC_RX2_RX_PATH_CTL;
		clk_reg = WCD934X_MIXING_ASRC1_CLK_RST_CTL;
		asrc_ctl = WCD934X_MIXING_ASRC1_CTL1;
		asrc = ASRC1;
		break;
	case ASRC_IN_LO2:
		cfg_reg = WCD934X_CDC_RX4_RX_PATH_CFG0;
		ctl_reg = WCD934X_CDC_RX4_RX_PATH_CTL;
		clk_reg = WCD934X_MIXING_ASRC1_CLK_RST_CTL;
		asrc_ctl = WCD934X_MIXING_ASRC1_CTL1;
		asrc = ASRC1;
		break;
	case ASRC_IN_SPKR1:
		cfg_reg = WCD934X_CDC_RX7_RX_PATH_CFG0;
		ctl_reg = WCD934X_CDC_RX7_RX_PATH_CTL;
		clk_reg = WCD934X_MIXING_ASRC2_CLK_RST_CTL;
		asrc_ctl = WCD934X_MIXING_ASRC2_CTL1;
		asrc = ASRC2;
		break;
	case ASRC_IN_SPKR2:
		cfg_reg = WCD934X_CDC_RX8_RX_PATH_CFG0;
		ctl_reg = WCD934X_CDC_RX8_RX_PATH_CTL;
		clk_reg = WCD934X_MIXING_ASRC3_CLK_RST_CTL;
		asrc_ctl = WCD934X_MIXING_ASRC3_CTL1;
		asrc = ASRC3;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid asrc input :%d\n", __func__,
			asrc_in);
		ret = -EINVAL;
		goto done;
	};

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (tavil->asrc_users[asrc] == 0) {
			snd_soc_update_bits(codec, cfg_reg, 0x80, 0x80);
			snd_soc_update_bits(codec, clk_reg, 0x01, 0x01);
			main_sr = snd_soc_read(codec, ctl_reg) & 0x0F;
			mix_ctl_reg = ctl_reg + 5;
			mix_sr = snd_soc_read(codec, mix_ctl_reg) & 0x0F;
			asrc_mode = tavil_get_asrc_mode(tavil, asrc,
							main_sr, mix_sr);
			dev_dbg(codec->dev, "%s: main_sr:%d mix_sr:%d asrc_mode %d\n",
				__func__, main_sr, mix_sr, asrc_mode);
			snd_soc_update_bits(codec, asrc_ctl, 0x07, asrc_mode);
		}
		tavil->asrc_users[asrc]++;
		break;
	case SND_SOC_DAPM_POST_PMD:
		tavil->asrc_users[asrc]--;
		if (tavil->asrc_users[asrc] <= 0) {
			tavil->asrc_users[asrc] = 0;
			snd_soc_update_bits(codec, asrc_ctl, 0x07, 0x00);
			snd_soc_update_bits(codec, cfg_reg, 0x80, 0x00);
			snd_soc_update_bits(codec, clk_reg, 0x01, 0x00);
		}
		break;
	};

	dev_dbg(codec->dev, "%s: ASRC%d, users: %d\n",
		__func__, asrc, tavil->asrc_users[asrc]);

done:
	return ret;
}

static int tavil_codec_enable_asrc_resampler(struct snd_soc_dapm_widget *w,
					     struct snd_kcontrol *kcontrol,
					     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;
	u8 cfg, asrc_in;

	cfg = snd_soc_read(codec, WCD934X_CDC_RX_INP_MUX_SPLINE_ASRC_CFG0);
	if (!(cfg & 0xFF)) {
		dev_err(codec->dev, "%s: ASRC%u input not selected\n",
			__func__, w->shift);
		return -EINVAL;
	}

	switch (w->shift) {
	case ASRC0:
		asrc_in = ((cfg & 0x03) == 1) ? ASRC_IN_HPHL : ASRC_IN_LO1;
		ret = tavil_codec_enable_asrc(codec, asrc_in, event);
		break;
	case ASRC1:
		asrc_in = ((cfg & 0x0C) == 4) ? ASRC_IN_HPHR : ASRC_IN_LO2;
		ret = tavil_codec_enable_asrc(codec, asrc_in, event);
		break;
	case ASRC2:
		asrc_in = ((cfg & 0x30) == 0x20) ? ASRC_IN_SPKR1 : ASRC_INVALID;
		ret = tavil_codec_enable_asrc(codec, asrc_in, event);
		break;
	case ASRC3:
		asrc_in = ((cfg & 0xC0) == 0x80) ? ASRC_IN_SPKR2 : ASRC_INVALID;
		ret = tavil_codec_enable_asrc(codec, asrc_in, event);
		break;
	default:
		dev_err(codec->dev, "%s: Invalid asrc:%u\n", __func__,
			w->shift);
		ret = -EINVAL;
		break;
	};

	return ret;
}

static int tavil_enable_native_supply(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (++tavil->native_clk_users == 1) {
			snd_soc_update_bits(codec, WCD934X_CLK_SYS_PLL_ENABLES,
					    0x01, 0x01);
			usleep_range(100, 120);
			snd_soc_update_bits(codec, WCD934X_CLK_SYS_MCLK2_PRG1,
					    0x06, 0x02);
			snd_soc_update_bits(codec, WCD934X_CLK_SYS_MCLK2_PRG1,
					    0x01, 0x01);
			snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_GATE,
					    0x04, 0x00);
			usleep_range(30, 50);
			snd_soc_update_bits(codec,
					WCD934X_CDC_CLK_RST_CTRL_MCLK_CONTROL,
					0x02, 0x02);
			snd_soc_update_bits(codec,
					WCD934X_CDC_CLK_RST_CTRL_FS_CNT_CONTROL,
					0x10, 0x10);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (tavil->native_clk_users &&
		    (--tavil->native_clk_users == 0)) {
			snd_soc_update_bits(codec,
					WCD934X_CDC_CLK_RST_CTRL_FS_CNT_CONTROL,
					0x10, 0x00);
			snd_soc_update_bits(codec,
					WCD934X_CDC_CLK_RST_CTRL_MCLK_CONTROL,
					0x02, 0x00);
			snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_GATE,
					    0x04, 0x04);
			snd_soc_update_bits(codec, WCD934X_CLK_SYS_MCLK2_PRG1,
					    0x01, 0x00);
			snd_soc_update_bits(codec, WCD934X_CLK_SYS_MCLK2_PRG1,
					    0x06, 0x00);
			snd_soc_update_bits(codec, WCD934X_CLK_SYS_PLL_ENABLES,
					    0x01, 0x00);
		}
		break;
	}

	dev_dbg(codec->dev, "%s: native_clk_users: %d, event: %d\n",
		__func__, tavil->native_clk_users, event);

	return 0;
}

static void tavil_codec_hphdelay_lutbypass(struct snd_soc_codec *codec,
				    u16 interp_idx, int event)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	u8 hph_dly_mask;
	u16 hph_lut_bypass_reg = 0;
	u16 hph_comp_ctrl7 = 0;


	switch (interp_idx) {
	case INTERP_HPHL:
		hph_dly_mask = 1;
		hph_lut_bypass_reg = WCD934X_CDC_TOP_HPHL_COMP_LUT;
		hph_comp_ctrl7 = WCD934X_CDC_COMPANDER1_CTL7;
		break;
	case INTERP_HPHR:
		hph_dly_mask = 2;
		hph_lut_bypass_reg = WCD934X_CDC_TOP_HPHR_COMP_LUT;
		hph_comp_ctrl7 = WCD934X_CDC_COMPANDER2_CTL7;
		break;
	default:
		break;
	}

	if (hph_lut_bypass_reg && SND_SOC_DAPM_EVENT_ON(event)) {
		snd_soc_update_bits(codec, WCD934X_CDC_CLSH_TEST0,
				    hph_dly_mask, 0x0);
		snd_soc_update_bits(codec, hph_lut_bypass_reg, 0x80, 0x80);
		if (tavil->hph_mode == CLS_H_ULP)
			snd_soc_update_bits(codec, hph_comp_ctrl7, 0x20, 0x20);
	}

	if (hph_lut_bypass_reg && SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, WCD934X_CDC_CLSH_TEST0,
				    hph_dly_mask, hph_dly_mask);
		snd_soc_update_bits(codec, hph_lut_bypass_reg, 0x80, 0x00);
		snd_soc_update_bits(codec, hph_comp_ctrl7, 0x20, 0x0);
	}
}

static void tavil_codec_hd2_control(struct tavil_priv *priv,
				    u16 interp_idx, int event)
{
	u16 hd2_scale_reg;
	u16 hd2_enable_reg = 0;
	struct snd_soc_codec *codec = priv->codec;

	if (TAVIL_IS_1_1(priv->wcd9xxx))
		return;

	switch (interp_idx) {
	case INTERP_HPHL:
		hd2_scale_reg = WCD934X_CDC_RX1_RX_PATH_SEC3;
		hd2_enable_reg = WCD934X_CDC_RX1_RX_PATH_CFG0;
		break;
	case INTERP_HPHR:
		hd2_scale_reg = WCD934X_CDC_RX2_RX_PATH_SEC3;
		hd2_enable_reg = WCD934X_CDC_RX2_RX_PATH_CFG0;
		break;
	}

	if (hd2_enable_reg && SND_SOC_DAPM_EVENT_ON(event)) {
		snd_soc_update_bits(codec, hd2_scale_reg, 0x3C, 0x14);
		snd_soc_update_bits(codec, hd2_enable_reg, 0x04, 0x04);
	}

	if (hd2_enable_reg && SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, hd2_enable_reg, 0x04, 0x00);
		snd_soc_update_bits(codec, hd2_scale_reg, 0x3C, 0x00);
	}
}

static int tavil_codec_config_ear_spkr_gain(struct snd_soc_codec *codec,
					    int event, int gain_reg)
{
	int comp_gain_offset, val;
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	switch (tavil->swr.spkr_mode) {
	/* Compander gain in SPKR_MODE1 case is 12 dB */
	case WCD934X_SPKR_MODE_1:
		comp_gain_offset = -12;
		break;
	/* Default case compander gain is 15 dB */
	default:
		comp_gain_offset = -15;
		break;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* Apply ear spkr gain only if compander is enabled */
		if (tavil->comp_enabled[COMPANDER_7] &&
		    (gain_reg == WCD934X_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD934X_CDC_RX7_RX_VOL_MIX_CTL) &&
		    (tavil->ear_spkr_gain != 0)) {
			/* For example, val is -8(-12+5-1) for 4dB of gain */
			val = comp_gain_offset + tavil->ear_spkr_gain - 1;
			snd_soc_write(codec, gain_reg, val);

			dev_dbg(codec->dev, "%s: RX7 Volume %d dB\n",
				__func__, val);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * Reset RX7 volume to 0 dB if compander is enabled and
		 * ear_spkr_gain is non-zero.
		 */
		if (tavil->comp_enabled[COMPANDER_7] &&
		    (gain_reg == WCD934X_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD934X_CDC_RX7_RX_VOL_MIX_CTL) &&
		    (tavil->ear_spkr_gain != 0)) {
			snd_soc_write(codec, gain_reg, 0x0);

			dev_dbg(codec->dev, "%s: Reset RX7 Volume to 0 dB\n",
				__func__);
		}
		break;
	}

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
		snd_soc_update_bits(codec, rx_path_cfg0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x04, 0x04);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x02);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x01, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x04, 0x00);
	}

	return 0;
}

static void tavil_codec_idle_detect_control(struct snd_soc_codec *codec,
					    int interp, int event)
{
	int reg = 0, mask, val;
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	if (!tavil->idle_det_cfg.hph_idle_detect_en)
		return;

	if (interp == INTERP_HPHL) {
		reg = WCD934X_CDC_RX_IDLE_DET_PATH_CTL;
		mask = 0x01;
		val = 0x01;
	}
	if (interp == INTERP_HPHR) {
		reg = WCD934X_CDC_RX_IDLE_DET_PATH_CTL;
		mask = 0x02;
		val = 0x02;
	}

	if (reg && SND_SOC_DAPM_EVENT_ON(event))
		snd_soc_update_bits(codec, reg, mask, val);

	if (reg && SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, reg, mask, 0x00);
		tavil->idle_det_cfg.hph_idle_thr = 0;
		snd_soc_write(codec, WCD934X_CDC_RX_IDLE_DET_CFG3, 0x0);
	}
}

/**
 * tavil_codec_enable_interp_clk - Enable main path Interpolator
 * clock.
 *
 * @codec:    Codec instance
 * @event:    Indicates speaker path gain offset value
 * @intp_idx: Interpolator index
 * Returns number of main clock users
 */
int tavil_codec_enable_interp_clk(struct snd_soc_codec *codec,
				  int event, int interp_idx)
{
	struct tavil_priv *tavil;
	u16 main_reg;

	if (!codec) {
		pr_err("%s: codec is NULL\n", __func__);
		return -EINVAL;
	}

	tavil  = snd_soc_codec_get_drvdata(codec);
	main_reg = WCD934X_CDC_RX0_RX_PATH_CTL + (interp_idx * 20);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (tavil->main_clk_users[interp_idx] == 0) {
			/* Main path PGA mute enable */
			snd_soc_update_bits(codec, main_reg, 0x10, 0x10);
			/* Clk enable */
			snd_soc_update_bits(codec, main_reg, 0x20, 0x20);
			tavil_codec_idle_detect_control(codec, interp_idx,
							event);
			tavil_codec_hd2_control(tavil, interp_idx, event);
			tavil_codec_hphdelay_lutbypass(codec, interp_idx,
						       event);
			tavil_config_compander(codec, interp_idx, event);
		}
		tavil->main_clk_users[interp_idx]++;
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		tavil->main_clk_users[interp_idx]--;
		if (tavil->main_clk_users[interp_idx] <= 0) {
			tavil->main_clk_users[interp_idx] = 0;
			tavil_config_compander(codec, interp_idx, event);
			tavil_codec_hphdelay_lutbypass(codec, interp_idx,
						       event);
			tavil_codec_hd2_control(tavil, interp_idx, event);
			tavil_codec_idle_detect_control(codec, interp_idx,
							event);
			/* Clk Disable */
			snd_soc_update_bits(codec, main_reg, 0x20, 0x00);
			/* Reset enable and disable */
			snd_soc_update_bits(codec, main_reg, 0x40, 0x40);
			snd_soc_update_bits(codec, main_reg, 0x40, 0x00);
			/* Reset rate to 48K*/
			snd_soc_update_bits(codec, main_reg, 0x0F, 0x04);
		}
	}

	dev_dbg(codec->dev, "%s event %d main_clk_users %d\n",
		__func__,  event, tavil->main_clk_users[interp_idx]);

	return tavil->main_clk_users[interp_idx];
}
EXPORT_SYMBOL(tavil_codec_enable_interp_clk);

static int tavil_anc_out_switch_cb(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	tavil_codec_enable_interp_clk(codec, event, w->shift);

	return 0;
}
static int tavil_codec_set_idle_detect_thr(struct snd_soc_codec *codec,
					   int interp, int path_type)
{
	int port_id[4] = { 0, 0, 0, 0 };
	int *port_ptr, num_ports;
	int bit_width = 0, i;
	int mux_reg, mux_reg_val;
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int dai_id, idle_thr;

	if ((interp != INTERP_HPHL) && (interp != INTERP_HPHR))
		return 0;

	if (!tavil->idle_det_cfg.hph_idle_detect_en)
		return 0;

	port_ptr = &port_id[0];
	num_ports = 0;

	/*
	 * Read interpolator MUX input registers and find
	 * which slimbus port is connected and store the port
	 * numbers in port_id array.
	 */
	if (path_type == INTERP_MIX_PATH) {
		mux_reg = WCD934X_CDC_RX_INP_MUX_RX_INT1_CFG1 +
			  2 * (interp - 1);
		mux_reg_val = snd_soc_read(codec, mux_reg) & 0x0f;

		if ((mux_reg_val >= INTn_2_INP_SEL_RX0) &&
		   (mux_reg_val < INTn_2_INP_SEL_PROXIMITY)) {
			*port_ptr++ = mux_reg_val +
				      WCD934X_RX_PORT_START_NUMBER - 1;
			num_ports++;
		}
	}

	if (path_type == INTERP_MAIN_PATH) {
		mux_reg = WCD934X_CDC_RX_INP_MUX_RX_INT1_CFG0 +
			  2 * (interp - 1);
		mux_reg_val = snd_soc_read(codec, mux_reg) & 0x0f;
		i = WCD934X_INTERP_MUX_NUM_INPUTS;

		while (i) {
			if ((mux_reg_val >= INTn_1_INP_SEL_RX0) &&
			    (mux_reg_val <= INTn_1_INP_SEL_RX7)) {
				*port_ptr++ = mux_reg_val +
					WCD934X_RX_PORT_START_NUMBER -
					INTn_1_INP_SEL_RX0;
				num_ports++;
			}
			mux_reg_val = (snd_soc_read(codec, mux_reg) &
						    0xf0) >> 4;
			mux_reg += 1;
			i--;
		}
	}

	dev_dbg(codec->dev, "%s: num_ports: %d, ports[%d %d %d %d]\n",
		__func__, num_ports, port_id[0], port_id[1],
		port_id[2], port_id[3]);

	i = 0;
	while (num_ports) {
		dai_id = tavil_find_playback_dai_id_for_port(port_id[i++],
							     tavil);

		if ((dai_id >= 0) && (dai_id < NUM_CODEC_DAIS)) {
			dev_dbg(codec->dev, "%s: dai_id: %d bit_width: %d\n",
				__func__, dai_id,
				tavil->dai[dai_id].bit_width);

			if (tavil->dai[dai_id].bit_width > bit_width)
				bit_width = tavil->dai[dai_id].bit_width;
		}

		num_ports--;
	}

	switch (bit_width) {
	case 16:
		idle_thr = 0xff; /* F16 */
		break;
	case 24:
	case 32:
		idle_thr = 0x03; /* F22 */
		break;
	default:
		idle_thr = 0x00;
		break;
	}

	dev_dbg(codec->dev, "%s: (new) idle_thr: %d, (cur) idle_thr: %d\n",
		__func__, idle_thr, tavil->idle_det_cfg.hph_idle_thr);

	if ((tavil->idle_det_cfg.hph_idle_thr == 0) ||
	    (idle_thr < tavil->idle_det_cfg.hph_idle_thr)) {
		snd_soc_write(codec, WCD934X_CDC_RX_IDLE_DET_CFG3, idle_thr);
		tavil->idle_det_cfg.hph_idle_thr = idle_thr;
	}

	return 0;
}

static int tavil_codec_enable_mix_path(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	u16 gain_reg, mix_reg;
	int offset_val = 0;
	int val = 0;

	if (w->shift >= WCD934X_NUM_INTERPOLATORS ||
	    w->shift == INTERP_LO3_NA || w->shift == INTERP_LO4_NA) {
		dev_err(codec->dev, "%s: Invalid Interpolator value %d for name %s\n",
			__func__, w->shift, w->name);
		return -EINVAL;
	};

	gain_reg = WCD934X_CDC_RX0_RX_VOL_MIX_CTL +
					(w->shift * WCD934X_RX_PATH_CTL_OFFSET);
	mix_reg = WCD934X_CDC_RX0_RX_PATH_MIX_CTL +
					(w->shift * WCD934X_RX_PATH_CTL_OFFSET);

	if (w->shift == INTERP_SPKR1 ||  w->shift == INTERP_SPKR2)
		__tavil_codec_enable_swr(w, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tavil_codec_set_idle_detect_thr(codec, w->shift,
						INTERP_MIX_PATH);
		tavil_codec_enable_interp_clk(codec, event, w->shift);
		/* Clk enable */
		snd_soc_update_bits(codec, mix_reg, 0x20, 0x20);
		break;
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
		tavil_codec_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Clk Disable */
		snd_soc_update_bits(codec, mix_reg, 0x20, 0x00);
		tavil_codec_enable_interp_clk(codec, event, w->shift);
		/* Reset enable and disable */
		snd_soc_update_bits(codec, mix_reg, 0x40, 0x40);
		snd_soc_update_bits(codec, mix_reg, 0x40, 0x00);

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
		tavil_codec_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	};
	dev_dbg(codec->dev, "%s event %d name %s\n", __func__, event, w->name);

	return 0;
}

/**
 * tavil_get_dsd_config - Get pointer to dsd config structure
 *
 * @codec: pointer to snd_soc_codec structure
 *
 * Returns pointer to tavil_dsd_config structure
 */
struct tavil_dsd_config *tavil_get_dsd_config(struct snd_soc_codec *codec)
{
	struct tavil_priv *tavil;

	if (!codec)
		return NULL;

	tavil = snd_soc_codec_get_drvdata(codec);

	if (!tavil)
		return NULL;

	return tavil->dsd_config;
}
EXPORT_SYMBOL(tavil_get_dsd_config);

static int tavil_codec_enable_main_path(struct snd_soc_dapm_widget *w,
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

	if (w->shift >= WCD934X_NUM_INTERPOLATORS ||
	    w->shift == INTERP_LO3_NA || w->shift == INTERP_LO4_NA) {
		dev_err(codec->dev, "%s: Invalid Interpolator value %d for name %s\n",
			__func__, w->shift, w->name);
		return -EINVAL;
	};

	reg = WCD934X_CDC_RX0_RX_PATH_CTL + (w->shift *
					     WCD934X_RX_PATH_CTL_OFFSET);
	gain_reg = WCD934X_CDC_RX0_RX_VOL_CTL + (w->shift *
						 WCD934X_RX_PATH_CTL_OFFSET);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tavil_codec_set_idle_detect_thr(codec, w->shift,
						INTERP_MAIN_PATH);
		tavil_codec_enable_interp_clk(codec, event, w->shift);
		break;
	case SND_SOC_DAPM_POST_PMU:
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
		tavil_codec_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tavil_codec_enable_interp_clk(codec, event, w->shift);

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
		tavil_codec_config_ear_spkr_gain(codec, event, gain_reg);
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
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int amic_n;

	dev_dbg(codec->dev, "%s: event: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		amic_n = tavil_codec_find_amic_input(codec, adc_mux_n);
		if (amic_n) {
			/*
			 * Prevent ANC Rx pop by leaving Tx FE in HOLD
			 * state until PA is up. Track AMIC being used
			 * so we can release the HOLD later.
			 */
			set_bit(ANC_MIC_AMIC1 + amic_n - 1,
				&tavil->status_mask);
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
	u16 dec_cfg_reg, amic_reg, go_bit_reg;
	u8 hpf_cut_off_freq;
	int amic_n;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	tavil = hpf_work->tavil;
	codec = tavil->codec;
	hpf_cut_off_freq = hpf_work->hpf_cut_off_freq;

	dec_cfg_reg = WCD934X_CDC_TX0_TX_PATH_CFG0 + 16 * hpf_work->decimator;
	go_bit_reg = dec_cfg_reg + 7;

	dev_dbg(codec->dev, "%s: decimator %u hpf_cut_of_freq 0x%x\n",
		__func__, hpf_work->decimator, hpf_cut_off_freq);

	amic_n = tavil_codec_find_amic_input(codec, hpf_work->decimator);
	if (amic_n) {
		amic_reg = WCD934X_ANA_AMIC1 + amic_n - 1;
		tavil_codec_set_tx_hold(codec, amic_reg, false);
	}
	snd_soc_update_bits(codec, dec_cfg_reg, TX_HPF_CUT_OFF_FREQ_MASK,
			    hpf_cut_off_freq << 5);
	snd_soc_update_bits(codec, go_bit_reg, 0x02, 0x02);
	/* Minimum 1 clk cycle delay is required as per HW spec */
	usleep_range(1000, 1010);
	snd_soc_update_bits(codec, go_bit_reg, 0x02, 0x00);
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
	snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x00);
}

static int tavil_codec_enable_rx_path_clk(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 sidetone_reg;

	dev_dbg(codec->dev, "%s %d %d\n", __func__, event, w->shift);
	sidetone_reg = WCD934X_CDC_RX0_RX_PATH_CFG1 + 0x14*(w->shift);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!strcmp(w->name, "RX INT7 MIX2 INP"))
			__tavil_codec_enable_swr(w, event);
		tavil_codec_enable_interp_clk(codec, event, w->shift);
		snd_soc_update_bits(codec, sidetone_reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, sidetone_reg, 0x10, 0x00);
		tavil_codec_enable_interp_clk(codec, event, w->shift);
		if (!strcmp(w->name, "RX INT7 MIX2 INP"))
			__tavil_codec_enable_swr(w, event);
		break;
	default:
		break;
	};
	return 0;
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
	u16 tx_gain_ctl_reg;
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
	tx_gain_ctl_reg = WCD934X_CDC_TX0_TX_VOL_CTL + 16 * decimator;

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
		/* Enable TX PGA Mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMU:
		hpf_cut_off_freq = (snd_soc_read(codec, dec_cfg_reg) &
				   TX_HPF_CUT_OFF_FREQ_MASK) >> 5;

		tavil->tx_hpf_work[decimator].hpf_cut_off_freq =
							hpf_cut_off_freq;
		if (hpf_cut_off_freq != CF_MIN_3DB_150HZ) {
			snd_soc_update_bits(codec, dec_cfg_reg,
					    TX_HPF_CUT_OFF_FREQ_MASK,
					    CF_MIN_3DB_150HZ << 5);
			snd_soc_update_bits(codec, hpf_gate_reg, 0x02, 0x02);
			/*
			 * Minimum 1 clk cycle delay is required as per
			 * HW spec.
			 */
			usleep_range(1000, 1010);
			snd_soc_update_bits(codec, hpf_gate_reg, 0x02, 0x00);
		}
		/* schedule work queue to Remove Mute */
		schedule_delayed_work(&tavil->tx_mute_dwork[decimator].dwork,
				      msecs_to_jiffies(tx_unmute_delay));
		if (tavil->tx_hpf_work[decimator].hpf_cut_off_freq !=
							CF_MIN_3DB_150HZ)
			schedule_delayed_work(
					&tavil->tx_hpf_work[decimator].dwork,
					msecs_to_jiffies(300));
		/* apply gain after decimator is enabled */
		snd_soc_write(codec, tx_gain_ctl_reg,
			      snd_soc_read(codec, tx_gain_ctl_reg));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		hpf_cut_off_freq =
			tavil->tx_hpf_work[decimator].hpf_cut_off_freq;
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x10);
		if (cancel_delayed_work_sync(
		    &tavil->tx_hpf_work[decimator].dwork)) {
			if (hpf_cut_off_freq != CF_MIN_3DB_150HZ) {
				snd_soc_update_bits(codec, dec_cfg_reg,
						    TX_HPF_CUT_OFF_FREQ_MASK,
						    hpf_cut_off_freq << 5);
				snd_soc_update_bits(codec, hpf_gate_reg,
						    0x02, 0x02);
				/*
				 * Minimum 1 clk cycle delay is required as per
				 * HW spec.
				 */
				usleep_range(1000, 1010);
				snd_soc_update_bits(codec, hpf_gate_reg,
						    0x02, 0x00);
			}
		}
		cancel_delayed_work_sync(
				&tavil->tx_mute_dwork[decimator].dwork);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x00);
		snd_soc_update_bits(codec, dec_cfg_reg,
				    WCD934X_DEC_PWR_LVL_MASK,
				    WCD934X_DEC_PWR_LVL_DF);
		break;
	};
out:
	kfree(wname);
	return ret;
}

static u32 tavil_get_dmic_sample_rate(struct snd_soc_codec *codec,
				      unsigned int dmic,
				      struct wcd9xxx_pdata *pdata)
{
	u8 tx_stream_fs;
	u8 adc_mux_index = 0, adc_mux_sel = 0;
	bool dec_found = false;
	u16 adc_mux_ctl_reg, tx_fs_reg;
	u32 dmic_fs;

	while (dec_found == 0 && adc_mux_index < WCD934X_MAX_VALID_ADC_MUX) {
		if (adc_mux_index < 4) {
			adc_mux_ctl_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
						(adc_mux_index * 2);
		} else if (adc_mux_index < WCD934X_INVALID_ADC_MUX) {
			adc_mux_ctl_reg = WCD934X_CDC_TX_INP_MUX_ADC_MUX4_CFG0 +
						adc_mux_index - 4;
		} else if (adc_mux_index == WCD934X_INVALID_ADC_MUX) {
			++adc_mux_index;
			continue;
		}
		adc_mux_sel = ((snd_soc_read(codec, adc_mux_ctl_reg) &
					0xF8) >> 3) - 1;

		if (adc_mux_sel == dmic) {
			dec_found = true;
			break;
		}

		++adc_mux_index;
	}

	if (dec_found && adc_mux_index <= 8) {
		tx_fs_reg = WCD934X_CDC_TX0_TX_PATH_CTL + (16 * adc_mux_index);
		tx_stream_fs = snd_soc_read(codec, tx_fs_reg) & 0x0F;
		if (tx_stream_fs <= 4)  {
			if (pdata->dmic_sample_rate <=
					WCD9XXX_DMIC_SAMPLE_RATE_2P4MHZ)
				dmic_fs = pdata->dmic_sample_rate;
			else
				dmic_fs = WCD9XXX_DMIC_SAMPLE_RATE_2P4MHZ;
		} else
			dmic_fs = WCD9XXX_DMIC_SAMPLE_RATE_4P8MHZ;
	} else {
		dmic_fs = pdata->dmic_sample_rate;
	}

	return dmic_fs;
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
	u32 dmic_sample_rate;
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
		dmic_sample_rate = tavil_get_dmic_sample_rate(codec, dmic,
							      pdata);
		dmic_rate_val =
			tavil_get_dmic_clk_val(codec,
					       pdata->mclk_rate,
					       dmic_sample_rate);

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

/*
 * tavil_mbhc_micb_adjust_voltage: adjust specific micbias voltage
 * @codec: handle to snd_soc_codec *
 * @req_volt: micbias voltage to be set
 * @micb_num: micbias to be set, e.g. micbias1 or micbias2
 *
 * return 0 if adjustment is success or error code in case of failure
 */
int tavil_mbhc_micb_adjust_voltage(struct snd_soc_codec *codec,
				   int req_volt, int micb_num)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int cur_vout_ctl, req_vout_ctl;
	int micb_reg, micb_val, micb_en;
	int ret = 0;

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
		return -EINVAL;
	}
	mutex_lock(&tavil->micb_lock);

	/*
	 * If requested micbias voltage is same as current micbias
	 * voltage, then just return. Otherwise, adjust voltage as
	 * per requested value. If micbias is already enabled, then
	 * to avoid slow micbias ramp-up or down enable pull-up
	 * momentarily, change the micbias value and then re-enable
	 * micbias.
	 */
	micb_val = snd_soc_read(codec, micb_reg);
	micb_en = (micb_val & 0xC0) >> 6;
	cur_vout_ctl = micb_val & 0x3F;

	req_vout_ctl = wcd934x_get_micb_vout_ctl_val(req_volt);
	if (IS_ERR_VALUE(req_vout_ctl)) {
		ret = -EINVAL;
		goto exit;
	}
	if (cur_vout_ctl == req_vout_ctl) {
		ret = 0;
		goto exit;
	}

	dev_dbg(codec->dev, "%s: micb_num: %d, cur_mv: %d, req_mv: %d, micb_en: %d\n",
		 __func__, micb_num, WCD_VOUT_CTL_TO_MICB(cur_vout_ctl),
		 req_volt, micb_en);

	if (micb_en == 0x1)
		snd_soc_update_bits(codec, micb_reg, 0xC0, 0x80);

	snd_soc_update_bits(codec, micb_reg, 0x3F, req_vout_ctl);

	if (micb_en == 0x1) {
		snd_soc_update_bits(codec, micb_reg, 0xC0, 0x40);
		/*
		 * Add 2ms delay as per HW requirement after enabling
		 * micbias
		 */
		usleep_range(2000, 2100);
	}
exit:
	mutex_unlock(&tavil->micb_lock);
	return ret;
}
EXPORT_SYMBOL(tavil_mbhc_micb_adjust_voltage);

/*
 * tavil_micbias_control: enable/disable micbias
 * @codec: handle to snd_soc_codec *
 * @micb_num: micbias to be enabled/disabled, e.g. micbias1 or micbias2
 * @req: control requested, enable/disable or pullup enable/disable
 * @is_dapm: triggered by dapm or not
 *
 * return 0 if control is success or error code in case of failure
 */
int tavil_micbias_control(struct snd_soc_codec *codec,
			  int micb_num, int req, bool is_dapm)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int micb_index = micb_num - 1;
	u16 micb_reg;
	int pre_off_event = 0, post_off_event = 0;
	int post_on_event = 0, post_dapm_off = 0;
	int post_dapm_on = 0;

	if ((micb_index < 0) || (micb_index > TAVIL_MAX_MICBIAS - 1)) {
		dev_err(codec->dev, "%s: Invalid micbias index, micb_ind:%d\n",
			__func__, micb_index);
		return -EINVAL;
	}

	switch (micb_num) {
	case MIC_BIAS_1:
		micb_reg = WCD934X_ANA_MICB1;
		break;
	case MIC_BIAS_2:
		micb_reg = WCD934X_ANA_MICB2;
		pre_off_event = WCD_EVENT_PRE_MICBIAS_2_OFF;
		post_off_event = WCD_EVENT_POST_MICBIAS_2_OFF;
		post_on_event = WCD_EVENT_POST_MICBIAS_2_ON;
		post_dapm_on = WCD_EVENT_POST_DAPM_MICBIAS_2_ON;
		post_dapm_off = WCD_EVENT_POST_DAPM_MICBIAS_2_OFF;
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
	mutex_lock(&tavil->micb_lock);

	switch (req) {
	case MICB_PULLUP_ENABLE:
		tavil->pullup_ref[micb_index]++;
		if ((tavil->pullup_ref[micb_index] == 1) &&
		    (tavil->micb_ref[micb_index] == 0))
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x80);
		break;
	case MICB_PULLUP_DISABLE:
		if (tavil->pullup_ref[micb_index] > 0)
			tavil->pullup_ref[micb_index]--;
		if ((tavil->pullup_ref[micb_index] == 0) &&
		    (tavil->micb_ref[micb_index] == 0))
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x00);
		break;
	case MICB_ENABLE:
		tavil->micb_ref[micb_index]++;
		if (tavil->micb_ref[micb_index] == 1) {
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x40);
			if (post_on_event && tavil->mbhc)
				blocking_notifier_call_chain(
						&tavil->mbhc->notifier,
						post_on_event,
						&tavil->mbhc->wcd_mbhc);
		}
		if (is_dapm && post_dapm_on && tavil->mbhc)
			blocking_notifier_call_chain(&tavil->mbhc->notifier,
					post_dapm_on, &tavil->mbhc->wcd_mbhc);
		break;
	case MICB_DISABLE:
		if (tavil->micb_ref[micb_index] > 0)
			tavil->micb_ref[micb_index]--;
		if ((tavil->micb_ref[micb_index] == 0) &&
		    (tavil->pullup_ref[micb_index] > 0))
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x80);
		else if ((tavil->micb_ref[micb_index] == 0) &&
			 (tavil->pullup_ref[micb_index] == 0)) {
			if (pre_off_event && tavil->mbhc)
				blocking_notifier_call_chain(
						&tavil->mbhc->notifier,
						pre_off_event,
						&tavil->mbhc->wcd_mbhc);
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x00);
			if (post_off_event && tavil->mbhc)
				blocking_notifier_call_chain(
						&tavil->mbhc->notifier,
						post_off_event,
						&tavil->mbhc->wcd_mbhc);
		}
		if (is_dapm && post_dapm_off && tavil->mbhc)
			blocking_notifier_call_chain(&tavil->mbhc->notifier,
					post_dapm_off, &tavil->mbhc->wcd_mbhc);
		break;
	};

	dev_dbg(codec->dev, "%s: micb_num:%d, micb_ref: %d, pullup_ref: %d\n",
		__func__, micb_num, tavil->micb_ref[micb_index],
		tavil->pullup_ref[micb_index]);

	mutex_unlock(&tavil->micb_lock);

	return 0;
}
EXPORT_SYMBOL(tavil_micbias_control);

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

/*
 * tavil_codec_enable_standalone_micbias - enable micbias standalone
 * @codec: pointer to codec instance
 * @micb_num: number of micbias to be enabled
 * @enable: true to enable micbias or false to disable
 *
 * This function is used to enable micbias (1, 2, 3 or 4) during
 * standalone independent of whether TX use-case is running or not
 *
 * Return: error code in case of failure or 0 for success
 */
int tavil_codec_enable_standalone_micbias(struct snd_soc_codec *codec,
					  int micb_num,
					  bool enable)
{
	const char * const micb_names[] = {
		DAPM_MICBIAS1_STANDALONE, DAPM_MICBIAS2_STANDALONE,
		DAPM_MICBIAS3_STANDALONE, DAPM_MICBIAS4_STANDALONE
	};
	int micb_index = micb_num - 1;
	int rc;

	if (!codec) {
		pr_err("%s: Codec memory is NULL\n", __func__);
		return -EINVAL;
	}

	if ((micb_index < 0) || (micb_index > TAVIL_MAX_MICBIAS - 1)) {
		dev_err(codec->dev, "%s: Invalid micbias index, micb_ind:%d\n",
			__func__, micb_index);
		return -EINVAL;
	}

	if (enable)
		rc = snd_soc_dapm_force_enable_pin(
						snd_soc_codec_get_dapm(codec),
						micb_names[micb_index]);
	else
		rc = snd_soc_dapm_disable_pin(snd_soc_codec_get_dapm(codec),
					      micb_names[micb_index]);

	if (!rc)
		snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));
	else
		dev_err(codec->dev, "%s: micbias%d force %s pin failed\n",
			__func__, micb_num, (enable ? "enable" : "disable"));

	return rc;
}
EXPORT_SYMBOL(tavil_codec_enable_standalone_micbias);

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


static const struct reg_sequence tavil_hph_reset_tbl[] = {
	{ WCD934X_HPH_CNP_EN, 0x80 },
	{ WCD934X_HPH_CNP_WG_CTL, 0x9A },
	{ WCD934X_HPH_CNP_WG_TIME, 0x14 },
	{ WCD934X_HPH_OCP_CTL, 0x28 },
	{ WCD934X_HPH_AUTO_CHOP, 0x16 },
	{ WCD934X_HPH_CHOP_CTL, 0x83 },
	{ WCD934X_HPH_PA_CTL1, 0x46 },
	{ WCD934X_HPH_PA_CTL2, 0x50 },
	{ WCD934X_HPH_L_EN, 0x80 },
	{ WCD934X_HPH_L_TEST, 0xE0 },
	{ WCD934X_HPH_L_ATEST, 0x50 },
	{ WCD934X_HPH_R_EN, 0x80 },
	{ WCD934X_HPH_R_TEST, 0xE0 },
	{ WCD934X_HPH_R_ATEST, 0x54 },
	{ WCD934X_HPH_RDAC_CLK_CTL1, 0x99 },
	{ WCD934X_HPH_RDAC_CLK_CTL2, 0x9B },
	{ WCD934X_HPH_RDAC_LDO_CTL, 0x33 },
	{ WCD934X_HPH_RDAC_CHOP_CLK_LP_CTL, 0x00 },
	{ WCD934X_HPH_REFBUFF_UHQA_CTL, 0xA8 },
};

static const struct reg_sequence tavil_hph_reset_tbl_1_0[] = {
	{ WCD934X_HPH_REFBUFF_LP_CTL, 0x0A },
	{ WCD934X_HPH_L_DAC_CTL, 0x00 },
	{ WCD934X_HPH_R_DAC_CTL, 0x00 },
	{ WCD934X_HPH_NEW_ANA_HPH2, 0x00 },
	{ WCD934X_HPH_NEW_ANA_HPH3, 0x00 },
	{ WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL, 0x00 },
	{ WCD934X_HPH_NEW_INT_RDAC_HD2_CTL, 0xA0 },
	{ WCD934X_HPH_NEW_INT_RDAC_VREF_CTL, 0x10 },
	{ WCD934X_HPH_NEW_INT_RDAC_OVERRIDE_CTL, 0x00 },
	{ WCD934X_HPH_NEW_INT_RDAC_MISC1, 0x00 },
	{ WCD934X_HPH_NEW_INT_PA_MISC1, 0x22 },
	{ WCD934X_HPH_NEW_INT_PA_MISC2, 0x00 },
	{ WCD934X_HPH_NEW_INT_PA_RDAC_MISC, 0x00 },
	{ WCD934X_HPH_NEW_INT_HPH_TIMER1, 0xFE },
	{ WCD934X_HPH_NEW_INT_HPH_TIMER2, 0x2 },
	{ WCD934X_HPH_NEW_INT_HPH_TIMER3, 0x4e},
	{ WCD934X_HPH_NEW_INT_HPH_TIMER4, 0x54 },
	{ WCD934X_HPH_NEW_INT_PA_RDAC_MISC2, 0x00 },
	{ WCD934X_HPH_NEW_INT_PA_RDAC_MISC3, 0x00 },
};

static const struct reg_sequence tavil_hph_reset_tbl_1_1[] = {
	{ WCD934X_HPH_REFBUFF_LP_CTL, 0x0E },
	{ WCD934X_HPH_L_DAC_CTL, 0x00 },
	{ WCD934X_HPH_R_DAC_CTL, 0x00 },
	{ WCD934X_HPH_NEW_ANA_HPH2, 0x00 },
	{ WCD934X_HPH_NEW_ANA_HPH3, 0x00 },
	{ WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL, 0x40 },
	{ WCD934X_HPH_NEW_INT_RDAC_HD2_CTL, 0x81 },
	{ WCD934X_HPH_NEW_INT_RDAC_VREF_CTL, 0x10 },
	{ WCD934X_HPH_NEW_INT_RDAC_OVERRIDE_CTL, 0x00 },
	{ WCD934X_HPH_NEW_INT_RDAC_MISC1, 0x81 },
	{ WCD934X_HPH_NEW_INT_PA_MISC1, 0x22 },
	{ WCD934X_HPH_NEW_INT_PA_MISC2, 0x00 },
	{ WCD934X_HPH_NEW_INT_PA_RDAC_MISC, 0x00 },
	{ WCD934X_HPH_NEW_INT_HPH_TIMER1, 0xFE },
	{ WCD934X_HPH_NEW_INT_HPH_TIMER2, 0x2 },
	{ WCD934X_HPH_NEW_INT_HPH_TIMER3, 0x4e},
	{ WCD934X_HPH_NEW_INT_HPH_TIMER4, 0x54 },
	{ WCD934X_HPH_NEW_INT_PA_RDAC_MISC2, 0x00 },
	{ WCD934X_HPH_NEW_INT_PA_RDAC_MISC3, 0x00 },
};

static const struct tavil_reg_mask_val tavil_pa_disable[] = {
	{ WCD934X_CDC_RX1_RX_PATH_CTL, 0x30, 0x10 }, /* RX1 mute enable */
	{ WCD934X_CDC_RX2_RX_PATH_CTL, 0x30, 0x10 }, /* RX2 mute enable */
	{ WCD934X_HPH_CNP_WG_CTL, 0x80, 0x00 }, /* GM3 boost disable */
	{ WCD934X_ANA_HPH, 0x80, 0x00 }, /* HPHL PA disable */
	{ WCD934X_ANA_HPH, 0x40, 0x00 }, /* HPHR PA disable */
	{ WCD934X_ANA_HPH, 0x20, 0x00 }, /* HPHL REF dsable */
	{ WCD934X_ANA_HPH, 0x10, 0x00 }, /* HPHR REF disable */
};

static const struct tavil_reg_mask_val tavil_ocp_en_seq[] = {
	{ WCD934X_RX_OCP_CTL, 0x0F, 0x02 }, /* OCP number of attempts is 2 */
	{ WCD934X_HPH_OCP_CTL, 0xFA, 0x3A }, /* OCP current limit */
	{ WCD934X_HPH_L_TEST, 0x01, 0x01 }, /* Enable HPHL OCP */
	{ WCD934X_HPH_R_TEST, 0x01, 0x01 }, /* Enable HPHR OCP */
};

static const struct tavil_reg_mask_val tavil_ocp_en_seq_1[] = {
	{ WCD934X_RX_OCP_CTL, 0x0F, 0x02 }, /* OCP number of attempts is 2 */
	{ WCD934X_HPH_OCP_CTL, 0xFA, 0x3A }, /* OCP current limit */
};

/* LO-HIFI */
static const struct tavil_reg_mask_val tavil_pre_pa_en_lohifi[] = {
	{ WCD934X_HPH_NEW_INT_HPH_TIMER1, 0x02, 0x00 },
	{ WCD934X_FLYBACK_VNEG_CTRL_4, 0xf0, 0x80 },
	{ WCD934X_HPH_NEW_INT_PA_MISC2, 0x20, 0x20 },
	{ WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL, 0xf0, 0x40 },
	{ WCD934X_HPH_CNP_WG_CTL, 0x80, 0x00 },
	{ WCD934X_RX_BIAS_HPH_LOWPOWER, 0xf0, 0xc0 },
	{ WCD934X_HPH_PA_CTL1, 0x0e, 0x02 },
	{ WCD934X_HPH_REFBUFF_LP_CTL, 0x06, 0x06 },
};

static const struct tavil_reg_mask_val tavil_pre_pa_en[] = {
	{ WCD934X_HPH_NEW_INT_HPH_TIMER1, 0x02, 0x00 },
	{ WCD934X_HPH_NEW_INT_PA_MISC2, 0x20, 0x0 },
	{ WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL, 0xf0, 0x40 },
	{ WCD934X_HPH_CNP_WG_CTL, 0x80, 0x00 },
	{ WCD934X_RX_BIAS_HPH_LOWPOWER, 0xf0, 0x80 },
	{ WCD934X_HPH_PA_CTL1, 0x0e, 0x06 },
	{ WCD934X_HPH_REFBUFF_LP_CTL, 0x06, 0x06 },
};

static const struct tavil_reg_mask_val tavil_post_pa_en[] = {
	{ WCD934X_HPH_L_TEST, 0x01, 0x01 }, /* Enable HPHL OCP */
	{ WCD934X_HPH_R_TEST, 0x01, 0x01 }, /* Enable HPHR OCP */
	{ WCD934X_CDC_RX1_RX_PATH_CTL, 0x30, 0x20 }, /* RX1 mute disable */
	{ WCD934X_CDC_RX2_RX_PATH_CTL, 0x30, 0x20 }, /* RX2 mute disable */
	{ WCD934X_HPH_CNP_WG_CTL, 0x80, 0x80 }, /* GM3 boost enable */
	{ WCD934X_HPH_NEW_INT_HPH_TIMER1, 0x02, 0x02 },
};

static void tavil_codec_hph_reg_range_read(struct regmap *map, u8 *buf)
{
	regmap_bulk_read(map, WCD934X_HPH_CNP_EN, buf, TAVIL_HPH_REG_RANGE_1);
	regmap_bulk_read(map, WCD934X_HPH_NEW_ANA_HPH2,
			 buf + TAVIL_HPH_REG_RANGE_1, TAVIL_HPH_REG_RANGE_2);
	regmap_bulk_read(map, WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL,
			 buf + TAVIL_HPH_REG_RANGE_1 + TAVIL_HPH_REG_RANGE_2,
			 TAVIL_HPH_REG_RANGE_3);
}

static void tavil_codec_hph_reg_recover(struct tavil_priv *tavil,
					struct regmap *map, int pa_status)
{
	int i;
	unsigned int reg;

	blocking_notifier_call_chain(&tavil->mbhc->notifier,
				     WCD_EVENT_OCP_OFF,
				     &tavil->mbhc->wcd_mbhc);

	if (pa_status & 0xC0)
		goto pa_en_restore;

	dev_dbg(tavil->dev, "%s: HPH PA in disable state (0x%x)\n",
		__func__, pa_status);

	regmap_write_bits(map, WCD934X_CDC_RX1_RX_PATH_CTL, 0x10, 0x10);
	regmap_write_bits(map, WCD934X_CDC_RX2_RX_PATH_CTL, 0x10, 0x10);
	regmap_write_bits(map, WCD934X_ANA_HPH, 0xC0, 0x00);
	regmap_write_bits(map, WCD934X_ANA_HPH, 0x30, 0x00);
	regmap_write_bits(map, WCD934X_CDC_RX1_RX_PATH_CTL, 0x10, 0x00);
	regmap_write_bits(map, WCD934X_CDC_RX2_RX_PATH_CTL, 0x10, 0x00);

	/* Restore to HW defaults */
	regmap_multi_reg_write(map, tavil_hph_reset_tbl,
			       ARRAY_SIZE(tavil_hph_reset_tbl));
	if (TAVIL_IS_1_1(tavil->wcd9xxx))
		regmap_multi_reg_write(map, tavil_hph_reset_tbl_1_1,
				ARRAY_SIZE(tavil_hph_reset_tbl_1_1));
	if (TAVIL_IS_1_0(tavil->wcd9xxx))
		regmap_multi_reg_write(map, tavil_hph_reset_tbl_1_0,
				ARRAY_SIZE(tavil_hph_reset_tbl_1_0));

	for (i = 0; i < ARRAY_SIZE(tavil_ocp_en_seq); i++)
		regmap_write_bits(map, tavil_ocp_en_seq[i].reg,
				  tavil_ocp_en_seq[i].mask,
				  tavil_ocp_en_seq[i].val);
	goto end;


pa_en_restore:
	dev_dbg(tavil->dev, "%s: HPH PA in enable state (0x%x)\n",
		__func__, pa_status);

	/* Disable PA and other registers before restoring */
	for (i = 0; i < ARRAY_SIZE(tavil_pa_disable); i++) {
		if (TAVIL_IS_1_1(tavil->wcd9xxx) &&
		    (tavil_pa_disable[i].reg == WCD934X_HPH_CNP_WG_CTL))
			continue;
		regmap_write_bits(map, tavil_pa_disable[i].reg,
				  tavil_pa_disable[i].mask,
				  tavil_pa_disable[i].val);
	}

	regmap_multi_reg_write(map, tavil_hph_reset_tbl,
			       ARRAY_SIZE(tavil_hph_reset_tbl));
	if (TAVIL_IS_1_1(tavil->wcd9xxx))
		regmap_multi_reg_write(map, tavil_hph_reset_tbl_1_1,
				ARRAY_SIZE(tavil_hph_reset_tbl_1_1));
	if (TAVIL_IS_1_0(tavil->wcd9xxx))
		regmap_multi_reg_write(map, tavil_hph_reset_tbl_1_0,
				ARRAY_SIZE(tavil_hph_reset_tbl_1_0));

	for (i = 0; i < ARRAY_SIZE(tavil_ocp_en_seq_1); i++)
		regmap_write_bits(map, tavil_ocp_en_seq_1[i].reg,
				  tavil_ocp_en_seq_1[i].mask,
				  tavil_ocp_en_seq_1[i].val);

	if (tavil->hph_mode == CLS_H_LOHIFI) {
		for (i = 0; i < ARRAY_SIZE(tavil_pre_pa_en_lohifi); i++) {
			reg = tavil_pre_pa_en_lohifi[i].reg;
			if ((TAVIL_IS_1_1(tavil->wcd9xxx)) &&
			    ((reg == WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL) ||
			     (reg == WCD934X_HPH_CNP_WG_CTL) ||
			     (reg == WCD934X_HPH_REFBUFF_LP_CTL)))
				continue;
			regmap_write_bits(map,
					  tavil_pre_pa_en_lohifi[i].reg,
					  tavil_pre_pa_en_lohifi[i].mask,
					  tavil_pre_pa_en_lohifi[i].val);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(tavil_pre_pa_en); i++) {
			reg = tavil_pre_pa_en[i].reg;
			if ((TAVIL_IS_1_1(tavil->wcd9xxx)) &&
			    ((reg == WCD934X_HPH_NEW_INT_RDAC_GAIN_CTL) ||
			     (reg == WCD934X_HPH_CNP_WG_CTL) ||
			     (reg == WCD934X_HPH_REFBUFF_LP_CTL)))
				continue;
			regmap_write_bits(map, tavil_pre_pa_en[i].reg,
					  tavil_pre_pa_en[i].mask,
					  tavil_pre_pa_en[i].val);
		}
	}

	if (TAVIL_IS_1_1(tavil->wcd9xxx)) {
		regmap_write(map, WCD934X_HPH_NEW_INT_RDAC_HD2_CTL_L, 0x84);
		regmap_write(map, WCD934X_HPH_NEW_INT_RDAC_HD2_CTL_R, 0x84);
	}

	regmap_write_bits(map, WCD934X_ANA_HPH, 0x0C, pa_status & 0x0C);
	regmap_write_bits(map, WCD934X_ANA_HPH, 0x30, 0x30);
	/* wait for 100usec after HPH DAC is enabled */
	usleep_range(100, 110);
	regmap_write(map, WCD934X_ANA_HPH, pa_status);
	/* Sleep for 7msec after PA is enabled */
	usleep_range(7000, 7100);

	for (i = 0; i < ARRAY_SIZE(tavil_post_pa_en); i++) {
		if ((TAVIL_IS_1_1(tavil->wcd9xxx)) &&
		    (tavil_post_pa_en[i].reg == WCD934X_HPH_CNP_WG_CTL))
			continue;
		regmap_write_bits(map, tavil_post_pa_en[i].reg,
				  tavil_post_pa_en[i].mask,
				  tavil_post_pa_en[i].val);
	}

end:
	tavil->mbhc->is_hph_recover = true;
	blocking_notifier_call_chain(
			&tavil->mbhc->notifier,
			WCD_EVENT_OCP_ON,
			&tavil->mbhc->wcd_mbhc);
}

static int tavil_codec_reset_hph_registers(struct snd_soc_dapm_widget *w,
					   struct snd_kcontrol *kcontrol,
					   int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	u8 cache_val[TAVIL_HPH_TOTAL_REG];
	u8 hw_val[TAVIL_HPH_TOTAL_REG];
	int pa_status;
	int ret;

	dev_dbg(wcd9xxx->dev, "%s: event: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		memset(cache_val, 0, TAVIL_HPH_TOTAL_REG);
		memset(hw_val, 0, TAVIL_HPH_TOTAL_REG);

		regmap_read(wcd9xxx->regmap, WCD934X_ANA_HPH, &pa_status);

		tavil_codec_hph_reg_range_read(wcd9xxx->regmap, cache_val);

		/* Read register values from HW directly */
		regcache_cache_bypass(wcd9xxx->regmap, true);
		tavil_codec_hph_reg_range_read(wcd9xxx->regmap, hw_val);
		regcache_cache_bypass(wcd9xxx->regmap, false);

		/* compare both the registers to know if there is corruption */
		ret = memcmp(cache_val, hw_val, TAVIL_HPH_TOTAL_REG);

		/* If both the values are same, it means no corruption */
		if (ret) {
			dev_dbg(codec->dev, "%s: cache and hw reg are not same\n",
				__func__);
			tavil_codec_hph_reg_recover(tavil, wcd9xxx->regmap,
						    pa_status);
		} else {
			dev_dbg(codec->dev, "%s: cache and hw reg are same\n",
				__func__);
			tavil->mbhc->is_hph_recover = false;
		}
		break;
	default:
		break;
	};

	return 0;
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

static int tavil_hph_asrc_mode_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int index = -EINVAL;

	if (!strcmp(kcontrol->id.name, "ASRC0 Output Mode"))
		index = ASRC0;
	if (!strcmp(kcontrol->id.name, "ASRC1 Output Mode"))
		index = ASRC1;

	if (tavil && (index >= 0) && (index < ASRC_MAX))
		tavil->asrc_output_mode[index] =
			ucontrol->value.integer.value[0];

	return 0;
}

static int tavil_hph_asrc_mode_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int val = 0;
	int index = -EINVAL;

	if (!strcmp(kcontrol->id.name, "ASRC0 Output Mode"))
		index = ASRC0;
	if (!strcmp(kcontrol->id.name, "ASRC1 Output Mode"))
		index = ASRC1;

	if (tavil && (index >= 0) && (index < ASRC_MAX))
		val = tavil->asrc_output_mode[index];

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int tavil_hph_idle_detect_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int val = 0;

	if (tavil)
		val = tavil->idle_det_cfg.hph_idle_detect_en;

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int tavil_hph_idle_detect_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	if (tavil)
		tavil->idle_det_cfg.hph_idle_detect_en =
			ucontrol->value.integer.value[0];

	return 0;
}

static int tavil_dmic_pin_mode_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u16 dmic_pin;
	u8 reg_val, pinctl_position;

	pinctl_position = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	dmic_pin = pinctl_position & 0x07;
	reg_val = snd_soc_read(codec,
			WCD934X_TLMM_DMIC1_CLK_PINCFG + dmic_pin - 1);

	ucontrol->value.integer.value[0] = !!reg_val;

	return 0;
}

static int tavil_dmic_pin_mode_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	u16 ctl_reg, cfg_reg, dmic_pin;
	u8 ctl_val, cfg_val, pinctl_position, pinctl_mode, mask;

	/* 0- high or low; 1- high Z */
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

	ctl_val = ~(pinctl_mode << (pinctl_position & 0x07));
	mask = 1 << (pinctl_position & 0x07);
	snd_soc_update_bits(codec, ctl_reg, mask, ctl_val);

	dmic_pin = pinctl_position & 0x07;
	cfg_reg = WCD934X_TLMM_DMIC1_CLK_PINCFG + dmic_pin - 1;
	if (pinctl_mode) {
		if (tavil->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
			cfg_val = 0x6;
		else
			cfg_val = 0xD;
	} else
		cfg_val = 0;
	snd_soc_update_bits(codec, cfg_reg, 0x1F, cfg_val);

	dev_dbg(codec->dev, "%s: reg=0x%x mask=0x%x val=%d reg=0x%x val=%d\n",
			__func__, ctl_reg, mask, ctl_val, cfg_reg, cfg_val);

	return 0;
}

static int tavil_amic_pwr_lvl_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u16 amic_reg = 0;

	if (!strcmp(kcontrol->id.name, "AMIC_1_2 PWR MODE"))
		amic_reg = WCD934X_ANA_AMIC1;
	if (!strcmp(kcontrol->id.name, "AMIC_3_4 PWR MODE"))
		amic_reg = WCD934X_ANA_AMIC3;

	if (amic_reg)
		ucontrol->value.integer.value[0] =
			(snd_soc_read(codec, amic_reg) &
			 WCD934X_AMIC_PWR_LVL_MASK) >>
			  WCD934X_AMIC_PWR_LVL_SHIFT;
	return 0;
}

static int tavil_amic_pwr_lvl_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u32 mode_val;
	u16 amic_reg = 0;

	mode_val = ucontrol->value.enumerated.item[0];

	dev_dbg(codec->dev, "%s: mode: %d\n", __func__, mode_val);

	if (!strcmp(kcontrol->id.name, "AMIC_1_2 PWR MODE"))
		amic_reg = WCD934X_ANA_AMIC1;
	if (!strcmp(kcontrol->id.name, "AMIC_3_4 PWR MODE"))
		amic_reg = WCD934X_ANA_AMIC3;

	if (amic_reg)
		snd_soc_update_bits(codec, amic_reg, WCD934X_AMIC_PWR_LVL_MASK,
				    mode_val << WCD934X_AMIC_PWR_LVL_SHIFT);
	return 0;
}

static const char *const tavil_conn_mad_text[] = {
	"NOTUSED1", "ADC1", "ADC2", "ADC3", "ADC4", "NOTUSED5",
	"NOTUSED6", "NOTUSED2", "DMIC0", "DMIC1", "DMIC2", "DMIC3",
	"DMIC4", "DMIC5", "NOTUSED3", "NOTUSED4"
};

static const struct soc_enum tavil_conn_mad_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tavil_conn_mad_text),
			    tavil_conn_mad_text);

static int tavil_mad_input_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u8 tavil_mad_input;

	tavil_mad_input = snd_soc_read(codec, WCD934X_SOC_MAD_INP_SEL) & 0x0F;
	ucontrol->value.integer.value[0] = tavil_mad_input;

	dev_dbg(codec->dev, "%s: tavil_mad_input = %s\n", __func__,
		tavil_conn_mad_text[tavil_mad_input]);

	return 0;
}

static int tavil_mad_input_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_card *card = codec->component.card;
	u8 tavil_mad_input;
	char mad_amic_input_widget[6];
	const char *mad_input_widget;
	const char *source_widget = NULL;
	u32 adc, i, mic_bias_found = 0;
	int ret = 0;
	char *mad_input;
	bool is_adc_input = false;

	tavil_mad_input = ucontrol->value.integer.value[0];

	if (tavil_mad_input >= sizeof(tavil_conn_mad_text)/
	    sizeof(tavil_conn_mad_text[0])) {
		dev_err(codec->dev,
			"%s: tavil_mad_input = %d out of bounds\n",
			__func__, tavil_mad_input);
		return -EINVAL;
	}

	if (strnstr(tavil_conn_mad_text[tavil_mad_input], "NOTUSED",
				sizeof("NOTUSED"))) {
		dev_dbg(codec->dev,
			"%s: Unsupported tavil_mad_input = %s\n",
			__func__, tavil_conn_mad_text[tavil_mad_input]);
		/* Make sure the MAD register is updated */
		snd_soc_update_bits(codec, WCD934X_ANA_MAD_SETUP,
				    0x88, 0x00);
		return -EINVAL;
	}

	if (strnstr(tavil_conn_mad_text[tavil_mad_input],
		    "ADC", sizeof("ADC"))) {
		mad_input = strpbrk(tavil_conn_mad_text[tavil_mad_input],
				    "1234");
		if (!mad_input) {
			dev_err(codec->dev, "%s: Invalid MAD input %s\n",
				__func__, tavil_conn_mad_text[tavil_mad_input]);
			return -EINVAL;
		}

		ret = kstrtouint(mad_input, 10, &adc);
		if ((ret < 0) || (adc > 4)) {
			dev_err(codec->dev, "%s: Invalid ADC = %s\n", __func__,
				tavil_conn_mad_text[tavil_mad_input]);
			return -EINVAL;
		}

		/*AMIC4 and AMIC5 share ADC4*/
		if ((adc == 4) &&
		    (snd_soc_read(codec, WCD934X_TX_NEW_AMIC_4_5_SEL) & 0x10))
			adc = 5;

		snprintf(mad_amic_input_widget, 6, "%s%u", "AMIC", adc);

		mad_input_widget = mad_amic_input_widget;
		is_adc_input = true;
	} else {
		/* DMIC type input widget*/
		mad_input_widget = tavil_conn_mad_text[tavil_mad_input];
	}

	dev_dbg(codec->dev,
		"%s: tavil input widget = %s, adc_input = %s\n", __func__,
		mad_input_widget, is_adc_input ? "true" : "false");

	for (i = 0; i < card->num_of_dapm_routes; i++) {
		if (!strcmp(card->of_dapm_routes[i].sink, mad_input_widget)) {
			source_widget = card->of_dapm_routes[i].source;
			if (!source_widget) {
				dev_err(codec->dev,
					"%s: invalid source widget\n",
					__func__);
				return -EINVAL;
			}

			if (strnstr(source_widget,
				"MIC BIAS1", sizeof("MIC BIAS1"))) {
				mic_bias_found = 1;
				break;
			} else if (strnstr(source_widget,
				"MIC BIAS2", sizeof("MIC BIAS2"))) {
				mic_bias_found = 2;
				break;
			} else if (strnstr(source_widget,
				"MIC BIAS3", sizeof("MIC BIAS3"))) {
				mic_bias_found = 3;
				break;
			} else if (strnstr(source_widget,
				"MIC BIAS4", sizeof("MIC BIAS4"))) {
				mic_bias_found = 4;
				break;
			}
		}
	}

	if (!mic_bias_found) {
		dev_err(codec->dev, "%s: mic bias not found for input %s\n",
			__func__, mad_input_widget);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s: mic_bias found = %d\n", __func__,
		mic_bias_found);

	snd_soc_update_bits(codec, WCD934X_SOC_MAD_INP_SEL,
			    0x0F, tavil_mad_input);
	snd_soc_update_bits(codec, WCD934X_ANA_MAD_SETUP,
			    0x07, mic_bias_found);
	/* for all adc inputs, mad should be in micbias mode with BG enabled */
	if (is_adc_input)
		snd_soc_update_bits(codec, WCD934X_ANA_MAD_SETUP,
				    0x88, 0x88);
	else
		snd_soc_update_bits(codec, WCD934X_ANA_MAD_SETUP,
				    0x88, 0x00);
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

static int tavil_ear_spkr_pa_gain_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tavil->ear_spkr_gain;

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int tavil_ear_spkr_pa_gain_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	tavil->ear_spkr_gain =  ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: gain = %d\n", __func__, tavil->ear_spkr_gain);

	return 0;
}

static int tavil_rx_hph_mode_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tavil->hph_mode;
	return 0;
}

static int tavil_rx_hph_mode_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	u32 mode_val;

	mode_val = ucontrol->value.enumerated.item[0];

	dev_dbg(codec->dev, "%s: mode: %d\n", __func__, mode_val);

	if (mode_val == 0) {
		dev_warn(codec->dev, "%s:Invalid HPH Mode, default to Cls-H LOHiFi\n",
			__func__);
		mode_val = CLS_H_LOHIFI;
	}
	tavil->hph_mode = mode_val;
	return 0;
}

static const char * const rx_hph_mode_mux_text[] = {
	"CLS_H_INVALID", "CLS_H_HIFI", "CLS_H_LP", "CLS_AB", "CLS_H_LOHIFI",
	"CLS_H_ULP", "CLS_AB_HIFI",
};

static const struct soc_enum rx_hph_mode_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_hph_mode_mux_text),
			    rx_hph_mode_mux_text);

static const char *const tavil_anc_func_text[] = {"OFF", "ON"};
static const struct soc_enum tavil_anc_func_enum =
	SOC_ENUM_SINGLE_EXT(2, tavil_anc_func_text);

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

static const char * const hph_idle_detect_text[] = {
	"OFF", "ON"
};

static const char * const asrc_mode_text[] = {
	"INT", "FRAC"
};

static const char * const tavil_ear_pa_gain_text[] = {
	"G_6_DB", "G_4P5_DB", "G_3_DB", "G_1P5_DB",
	"G_0_DB", "G_M2P5_DB", "UNDEFINED", "G_M12_DB"
};

static const char * const tavil_ear_spkr_pa_gain_text[] = {
	"G_DEFAULT", "G_0_DB", "G_1_DB", "G_2_DB", "G_3_DB",
	"G_4_DB", "G_5_DB", "G_6_DB"
};

static SOC_ENUM_SINGLE_EXT_DECL(tavil_ear_pa_gain_enum, tavil_ear_pa_gain_text);
static SOC_ENUM_SINGLE_EXT_DECL(tavil_ear_spkr_pa_gain_enum,
				tavil_ear_spkr_pa_gain_text);
static SOC_ENUM_SINGLE_EXT_DECL(amic_pwr_lvl_enum, amic_pwr_lvl_text);
static SOC_ENUM_SINGLE_EXT_DECL(hph_idle_detect_enum, hph_idle_detect_text);
static SOC_ENUM_SINGLE_EXT_DECL(asrc_mode_enum, asrc_mode_text);
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
	SOC_ENUM_EXT("EAR SPKR PA Gain", tavil_ear_spkr_pa_gain_enum,
		     tavil_ear_spkr_pa_gain_get, tavil_ear_spkr_pa_gain_put),
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
	SOC_SINGLE_SX_TLV("DEC5 Volume", WCD934X_CDC_TX5_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC6 Volume", WCD934X_CDC_TX6_TX_VOL_CTL, 0,
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

	SOC_SINGLE_EXT("ANC Slot", SND_SOC_NOPM, 0, 100, 0, tavil_get_anc_slot,
		tavil_put_anc_slot),
	SOC_ENUM_EXT("ANC Function", tavil_anc_func_enum, tavil_get_anc_func,
		tavil_put_anc_func),

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

	SOC_ENUM_EXT("RX HPH Mode", rx_hph_mode_mux_enum,
		tavil_rx_hph_mode_get, tavil_rx_hph_mode_put),

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

	SOC_ENUM_EXT("ASRC0 Output Mode", asrc_mode_enum,
		tavil_hph_asrc_mode_get, tavil_hph_asrc_mode_put),
	SOC_ENUM_EXT("ASRC1 Output Mode", asrc_mode_enum,
		tavil_hph_asrc_mode_get, tavil_hph_asrc_mode_put),

	SOC_ENUM_EXT("HPH Idle Detect", hph_idle_detect_enum,
		tavil_hph_idle_detect_get, tavil_hph_idle_detect_put),

	SOC_ENUM_EXT("MAD Input", tavil_conn_mad_enum,
		     tavil_mad_input_get, tavil_mad_input_put),

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

static const char * const rx_int0_1_interp_mux_text[] = {
	"ZERO", "RX INT0_1 MIX1",
};

static const char * const rx_int1_1_interp_mux_text[] = {
	"ZERO", "RX INT1_1 MIX1",
};

static const char * const rx_int2_1_interp_mux_text[] = {
	"ZERO", "RX INT2_1 MIX1",
};

static const char * const rx_int3_1_interp_mux_text[] = {
	"ZERO", "RX INT3_1 MIX1",
};

static const char * const rx_int4_1_interp_mux_text[] = {
	"ZERO", "RX INT4_1 MIX1",
};

static const char * const rx_int7_1_interp_mux_text[] = {
	"ZERO", "RX INT7_1 MIX1",
};

static const char * const rx_int8_1_interp_mux_text[] = {
	"ZERO", "RX INT8_1 MIX1",
};

static const char * const rx_int0_2_interp_mux_text[] = {
	"ZERO", "RX INT0_2 MUX",
};

static const char * const rx_int1_2_interp_mux_text[] = {
	"ZERO", "RX INT1_2 MUX",
};

static const char * const rx_int2_2_interp_mux_text[] = {
	"ZERO", "RX INT2_2 MUX",
};

static const char * const rx_int3_2_interp_mux_text[] = {
	"ZERO", "RX INT3_2 MUX",
};

static const char * const rx_int4_2_interp_mux_text[] = {
	"ZERO", "RX INT4_2 MUX",
};

static const char * const rx_int7_2_interp_mux_text[] = {
	"ZERO", "RX INT7_2 MUX",
};

static const char * const rx_int8_2_interp_mux_text[] = {
	"ZERO", "RX INT8_2 MUX",
};

static const char * const mad_sel_txt[] = {
	"SPE", "MSM"
};

static const char * const mad_inp_mux_txt[] = {
	"MAD", "DEC1"
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

static const char * const anc0_fb_mux_text[] = {
	"ZERO", "ANC_IN_HPHL", "ANC_IN_EAR", "ANC_IN_EAR_SPKR",
	"ANC_IN_LO1"
};

static const char * const anc1_fb_mux_text[] = {
	"ZERO", "ANC_IN_HPHR", "ANC_IN_LO2"
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

static const char * const asrc0_mux_text[] = {
	"ZERO", "ASRC_IN_HPHL", "ASRC_IN_LO1",
};

static const char * const asrc1_mux_text[] = {
	"ZERO", "ASRC_IN_HPHR", "ASRC_IN_LO2",
};

static const char * const asrc2_mux_text[] = {
	"ZERO", "ASRC_IN_SPKR1",
};

static const char * const asrc3_mux_text[] = {
	"ZERO", "ASRC_IN_SPKR2",
};

static const char * const native_mux_text[] = {
	"OFF", "ON",
};

static const struct snd_kcontrol_new aif4_vi_mixer[] = {
	SOC_SINGLE_EXT("SPKR_VI_1", SND_SOC_NOPM, WCD934X_TX14, 1, 0,
			tavil_vi_feed_mixer_get, tavil_vi_feed_mixer_put),
	SOC_SINGLE_EXT("SPKR_VI_2", SND_SOC_NOPM, WCD934X_TX15, 1, 0,
			tavil_vi_feed_mixer_get, tavil_vi_feed_mixer_put),
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

static const struct snd_kcontrol_new aif4_mad_mixer[] = {
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

WCD_DAPM_ENUM(rx_int0_1_interp, SND_SOC_NOPM, 0, rx_int0_1_interp_mux_text);
WCD_DAPM_ENUM(rx_int1_1_interp, SND_SOC_NOPM, 0, rx_int1_1_interp_mux_text);
WCD_DAPM_ENUM(rx_int2_1_interp, SND_SOC_NOPM, 0, rx_int2_1_interp_mux_text);
WCD_DAPM_ENUM(rx_int3_1_interp, SND_SOC_NOPM, 0, rx_int3_1_interp_mux_text);
WCD_DAPM_ENUM(rx_int4_1_interp, SND_SOC_NOPM, 0, rx_int4_1_interp_mux_text);
WCD_DAPM_ENUM(rx_int7_1_interp, SND_SOC_NOPM, 0, rx_int7_1_interp_mux_text);
WCD_DAPM_ENUM(rx_int8_1_interp, SND_SOC_NOPM, 0, rx_int8_1_interp_mux_text);

WCD_DAPM_ENUM(rx_int0_2_interp, SND_SOC_NOPM, 0, rx_int0_2_interp_mux_text);
WCD_DAPM_ENUM(rx_int1_2_interp, SND_SOC_NOPM, 0, rx_int1_2_interp_mux_text);
WCD_DAPM_ENUM(rx_int2_2_interp, SND_SOC_NOPM, 0, rx_int2_2_interp_mux_text);
WCD_DAPM_ENUM(rx_int3_2_interp, SND_SOC_NOPM, 0, rx_int3_2_interp_mux_text);
WCD_DAPM_ENUM(rx_int4_2_interp, SND_SOC_NOPM, 0, rx_int4_2_interp_mux_text);
WCD_DAPM_ENUM(rx_int7_2_interp, SND_SOC_NOPM, 0, rx_int7_2_interp_mux_text);
WCD_DAPM_ENUM(rx_int8_2_interp, SND_SOC_NOPM, 0, rx_int8_2_interp_mux_text);

WCD_DAPM_ENUM(mad_sel, WCD934X_CPE_SS_SVA_CFG, 0,
	mad_sel_txt);

WCD_DAPM_ENUM(mad_inp_mux, WCD934X_CPE_SS_SVA_CFG, 2,
	mad_inp_mux_txt);

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

WCD_DAPM_ENUM(asrc0, WCD934X_CDC_RX_INP_MUX_SPLINE_ASRC_CFG0, 0,
	asrc0_mux_text);
WCD_DAPM_ENUM(asrc1, WCD934X_CDC_RX_INP_MUX_SPLINE_ASRC_CFG0, 2,
	asrc1_mux_text);
WCD_DAPM_ENUM(asrc2, WCD934X_CDC_RX_INP_MUX_SPLINE_ASRC_CFG0, 4,
	asrc2_mux_text);
WCD_DAPM_ENUM(asrc3, WCD934X_CDC_RX_INP_MUX_SPLINE_ASRC_CFG0, 6,
	asrc3_mux_text);

WCD_DAPM_ENUM(int1_1_native, SND_SOC_NOPM, 0, native_mux_text);
WCD_DAPM_ENUM(int2_1_native, SND_SOC_NOPM, 0, native_mux_text);
WCD_DAPM_ENUM(int3_1_native, SND_SOC_NOPM, 0, native_mux_text);
WCD_DAPM_ENUM(int4_1_native, SND_SOC_NOPM, 0, native_mux_text);

WCD_DAPM_ENUM(int1_2_native, SND_SOC_NOPM, 0, native_mux_text);
WCD_DAPM_ENUM(int2_2_native, SND_SOC_NOPM, 0, native_mux_text);
WCD_DAPM_ENUM(int3_2_native, SND_SOC_NOPM, 0, native_mux_text);
WCD_DAPM_ENUM(int4_2_native, SND_SOC_NOPM, 0, native_mux_text);
WCD_DAPM_ENUM(int7_2_native, SND_SOC_NOPM, 0, native_mux_text);
WCD_DAPM_ENUM(int8_2_native, SND_SOC_NOPM, 0, native_mux_text);

WCD_DAPM_ENUM(anc0_fb, WCD934X_CDC_RX_INP_MUX_ANC_CFG0, 0, anc0_fb_mux_text);
WCD_DAPM_ENUM(anc1_fb, WCD934X_CDC_RX_INP_MUX_ANC_CFG0, 3, anc1_fb_mux_text);

static const struct snd_kcontrol_new anc_ear_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_ear_spkr_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_spkr_pa_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_hphl_pa_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_hphr_pa_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new mad_cpe1_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new mad_cpe2_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new mad_brdcst_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

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

static const struct snd_kcontrol_new rx_int1_asrc_switch[] = {
	SOC_DAPM_SINGLE("HPHL Switch", SND_SOC_NOPM, 0, 1, 0),
};

static const struct snd_kcontrol_new rx_int2_asrc_switch[] = {
	SOC_DAPM_SINGLE("HPHR Switch", SND_SOC_NOPM, 0, 1, 0),
};

static const struct snd_kcontrol_new rx_int3_asrc_switch[] = {
	SOC_DAPM_SINGLE("LO1 Switch", SND_SOC_NOPM, 0, 1, 0),
};

static const struct snd_kcontrol_new rx_int4_asrc_switch[] = {
	SOC_DAPM_SINGLE("LO2 Switch", SND_SOC_NOPM, 0, 1, 0),
};

static int tavil_dsd_mixer_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
				snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct tavil_priv *tavil_p = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct tavil_dsd_config *dsd_conf = tavil_p->dsd_config;
	int val;

	val = tavil_dsd_get_current_mixer_value(dsd_conf, mc->shift);

	ucontrol->value.integer.value[0] = ((val < 0) ? 0 : val);

	return 0;
}

static int tavil_dsd_mixer_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct tavil_priv *tavil_p = snd_soc_codec_get_drvdata(codec);
	unsigned int wval = ucontrol->value.integer.value[0];
	struct tavil_dsd_config *dsd_conf = tavil_p->dsd_config;

	if (!dsd_conf)
		return 0;

	mutex_lock(&tavil_p->codec_mutex);

	tavil_dsd_set_out_select(dsd_conf, mc->shift);
	tavil_dsd_set_mixer_value(dsd_conf, mc->shift, wval);

	mutex_unlock(&tavil_p->codec_mutex);
	snd_soc_dapm_mixer_update_power(dapm, kcontrol, wval, NULL);

	return 0;
}

static const struct snd_kcontrol_new hphl_mixer[] = {
	SOC_SINGLE_EXT("DSD HPHL Switch", SND_SOC_NOPM, INTERP_HPHL, 1, 0,
			tavil_dsd_mixer_get, tavil_dsd_mixer_put),
};

static const struct snd_kcontrol_new hphr_mixer[] = {
	SOC_SINGLE_EXT("DSD HPHR Switch", SND_SOC_NOPM, INTERP_HPHR, 1, 0,
			tavil_dsd_mixer_get, tavil_dsd_mixer_put),
};

static const struct snd_kcontrol_new lo1_mixer[] = {
	SOC_SINGLE_EXT("DSD LO1 Switch", SND_SOC_NOPM, INTERP_LO1, 1, 0,
			tavil_dsd_mixer_get, tavil_dsd_mixer_put),
};

static const struct snd_kcontrol_new lo2_mixer[] = {
	SOC_SINGLE_EXT("DSD LO2 Switch", SND_SOC_NOPM, INTERP_LO2, 1, 0,
			tavil_dsd_mixer_get, tavil_dsd_mixer_put),
};

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

	SND_SOC_DAPM_MUX_E("RX INT0_2 MUX", SND_SOC_NOPM, INTERP_EAR, 0,
		&rx_int0_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT1_2 MUX", SND_SOC_NOPM, INTERP_HPHL, 0,
		&rx_int1_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT2_2 MUX", SND_SOC_NOPM, INTERP_HPHR, 0,
		&rx_int2_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT3_2 MUX", SND_SOC_NOPM, INTERP_LO1, 0,
		&rx_int3_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT4_2 MUX", SND_SOC_NOPM, INTERP_LO2, 0,
		&rx_int4_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7_2 MUX", SND_SOC_NOPM, INTERP_SPKR1, 0,
		&rx_int7_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_2 MUX", SND_SOC_NOPM, INTERP_SPKR2, 0,
		&rx_int8_2_mux, tavil_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

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
	SND_SOC_DAPM_MIXER("RX INT1 SEC MIX", SND_SOC_NOPM, 0, 0,
		rx_int1_asrc_switch, ARRAY_SIZE(rx_int1_asrc_switch)),
	SND_SOC_DAPM_MIXER("RX INT2_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT2 SEC MIX", SND_SOC_NOPM, 0, 0,
		rx_int2_asrc_switch, ARRAY_SIZE(rx_int2_asrc_switch)),
	SND_SOC_DAPM_MIXER("RX INT3_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT3 SEC MIX", SND_SOC_NOPM, 0, 0,
		rx_int3_asrc_switch, ARRAY_SIZE(rx_int3_asrc_switch)),
	SND_SOC_DAPM_MIXER("RX INT4_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT4 SEC MIX", SND_SOC_NOPM, 0, 0,
		rx_int4_asrc_switch, ARRAY_SIZE(rx_int4_asrc_switch)),
	SND_SOC_DAPM_MIXER("RX INT7_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT7 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT8_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT8 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("RX INT0 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT1 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT1 MIX3", SND_SOC_NOPM, 0, 0, hphl_mixer,
			   ARRAY_SIZE(hphl_mixer)),
	SND_SOC_DAPM_MIXER("RX INT2 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT2 MIX3", SND_SOC_NOPM, 0, 0, hphr_mixer,
			   ARRAY_SIZE(hphr_mixer)),
	SND_SOC_DAPM_MIXER("RX INT3 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT3 MIX3", SND_SOC_NOPM, 0, 0, lo1_mixer,
			   ARRAY_SIZE(lo1_mixer)),
	SND_SOC_DAPM_MIXER("RX INT4 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT4 MIX3", SND_SOC_NOPM, 0, 0, lo2_mixer,
			   ARRAY_SIZE(lo2_mixer)),
	SND_SOC_DAPM_MIXER("RX INT7 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("RX INT7 CHAIN", SND_SOC_NOPM, 0, 0,
		NULL, 0, tavil_codec_spk_boost_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX INT8 CHAIN", SND_SOC_NOPM, 0, 0,
		NULL, 0, tavil_codec_spk_boost_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("RX INT0 MIX2 INP", SND_SOC_NOPM, INTERP_EAR,
		0, &rx_int0_mix2_inp_mux, tavil_codec_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT1 MIX2 INP", SND_SOC_NOPM, INTERP_HPHL,
		0, &rx_int1_mix2_inp_mux, tavil_codec_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT2 MIX2 INP", SND_SOC_NOPM, INTERP_HPHR,
		0, &rx_int2_mix2_inp_mux, tavil_codec_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT3 MIX2 INP", SND_SOC_NOPM, INTERP_LO1,
		0, &rx_int3_mix2_inp_mux, tavil_codec_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT4 MIX2 INP", SND_SOC_NOPM, INTERP_LO2,
		0, &rx_int4_mix2_inp_mux, tavil_codec_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7 MIX2 INP", SND_SOC_NOPM, INTERP_SPKR1,
		0, &rx_int7_mix2_inp_mux, tavil_codec_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

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

	WCD_DAPM_MUX("ANC0 FB MUX", 0, anc0_fb),
	WCD_DAPM_MUX("ANC1 FB MUX", 0, anc1_fb),

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

	/*
	 * Not supply widget, this is used to recover HPH registers.
	 * It is not connected to any other widgets
	 */
	SND_SOC_DAPM_SUPPLY("RESET_HPH_REGISTERS", SND_SOC_NOPM,
		0, 0, tavil_codec_reset_hph_registers,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

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
	SND_SOC_DAPM_MIXER("AIF4_MAD Mixer", SND_SOC_NOPM, AIF4_MAD_TX, 0,
		aif4_mad_mixer, ARRAY_SIZE(aif4_mad_mixer)),

	SND_SOC_DAPM_AIF_OUT_E("AIF4 VI", "VIfeed", 0, SND_SOC_NOPM,
		AIF4_VIFEED, 0, tavil_codec_enable_slimvi_feedback,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT("AIF4 MAD", "AIF4 MAD TX", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MIXER("AIF4_VI Mixer", SND_SOC_NOPM, AIF4_VIFEED, 0,
		aif4_vi_mixer, ARRAY_SIZE(aif4_vi_mixer)),
	SND_SOC_DAPM_INPUT("VIINPUT"),

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

	SND_SOC_DAPM_MUX_E("RX INT0_1 INTERP", SND_SOC_NOPM, INTERP_EAR, 0,
		&rx_int0_1_interp_mux, tavil_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT1_1 INTERP", SND_SOC_NOPM, INTERP_HPHL, 0,
		&rx_int1_1_interp_mux, tavil_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT2_1 INTERP", SND_SOC_NOPM, INTERP_HPHR, 0,
		&rx_int2_1_interp_mux, tavil_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT3_1 INTERP", SND_SOC_NOPM, INTERP_LO1, 0,
		&rx_int3_1_interp_mux, tavil_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT4_1 INTERP", SND_SOC_NOPM, INTERP_LO2, 0,
		&rx_int4_1_interp_mux, tavil_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7_1 INTERP", SND_SOC_NOPM, INTERP_SPKR1, 0,
		&rx_int7_1_interp_mux, tavil_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_1 INTERP", SND_SOC_NOPM, INTERP_SPKR2, 0,
		&rx_int8_1_interp_mux, tavil_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	WCD_DAPM_MUX("RX INT0_2 INTERP", 0, rx_int0_2_interp),
	WCD_DAPM_MUX("RX INT1_2 INTERP", 0, rx_int1_2_interp),
	WCD_DAPM_MUX("RX INT2_2 INTERP", 0, rx_int2_2_interp),
	WCD_DAPM_MUX("RX INT3_2 INTERP", 0, rx_int3_2_interp),
	WCD_DAPM_MUX("RX INT4_2 INTERP", 0, rx_int4_2_interp),
	WCD_DAPM_MUX("RX INT7_2 INTERP", 0, rx_int7_2_interp),
	WCD_DAPM_MUX("RX INT8_2 INTERP", 0, rx_int8_2_interp),

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

	/* MAD related widgets */
	SND_SOC_DAPM_INPUT("MAD_CPE_INPUT"),
	SND_SOC_DAPM_INPUT("MADINPUT"),

	WCD_DAPM_MUX("MAD_SEL MUX", 0, mad_sel),
	WCD_DAPM_MUX("MAD_INP MUX", 0, mad_inp_mux),

	SND_SOC_DAPM_SWITCH_E("MAD_BROADCAST", SND_SOC_NOPM, 0, 0,
			      &mad_brdcst_switch, tavil_codec_ape_enable_mad,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SWITCH_E("MAD_CPE1", SND_SOC_NOPM, 0, 0,
			      &mad_cpe1_switch, tavil_codec_cpe_mad_ctl,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SWITCH_E("MAD_CPE2", SND_SOC_NOPM, 0, 0,
			      &mad_cpe2_switch, tavil_codec_cpe_mad_ctl,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUTPUT("MAD_CPE_OUT1"),
	SND_SOC_DAPM_OUTPUT("MAD_CPE_OUT2"),

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
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT2 PA", WCD934X_ANA_LO_1_2, 6, 0, NULL, 0,
		tavil_codec_enable_lineout_pa,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC EAR PA", WCD934X_ANA_EAR, 7, 0, NULL, 0,
		tavil_codec_enable_ear_pa, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC SPK1 PA", SND_SOC_NOPM, 0, 0, NULL, 0,
		tavil_codec_enable_spkr_anc,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC HPHL PA", SND_SOC_NOPM, 0, 0, NULL, 0,
		tavil_codec_enable_hphl_pa,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC HPHR PA", SND_SOC_NOPM, 0, 0, NULL, 0,
		tavil_codec_enable_hphr_pa,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("EAR"),
	SND_SOC_DAPM_OUTPUT("HPHL"),
	SND_SOC_DAPM_OUTPUT("HPHR"),
	SND_SOC_DAPM_OUTPUT("LINEOUT1"),
	SND_SOC_DAPM_OUTPUT("LINEOUT2"),
	SND_SOC_DAPM_OUTPUT("SPK1 OUT"),
	SND_SOC_DAPM_OUTPUT("SPK2 OUT"),
	SND_SOC_DAPM_OUTPUT("ANC EAR"),
	SND_SOC_DAPM_OUTPUT("ANC HPHL"),
	SND_SOC_DAPM_OUTPUT("ANC HPHR"),

	SND_SOC_DAPM_SWITCH("ANC OUT EAR Enable", SND_SOC_NOPM, 0, 0,
		&anc_ear_switch),
	SND_SOC_DAPM_SWITCH("ANC OUT EAR SPKR Enable", SND_SOC_NOPM, 0, 0,
		&anc_ear_spkr_switch),
	SND_SOC_DAPM_SWITCH("ANC SPKR PA Enable", SND_SOC_NOPM, 0, 0,
		&anc_spkr_pa_switch),

	SND_SOC_DAPM_SWITCH_E("ANC OUT HPHL Enable", SND_SOC_NOPM, INTERP_HPHL,
		0, &anc_hphl_pa_switch, tavil_anc_out_switch_cb,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	 SND_SOC_DAPM_SWITCH_E("ANC OUT HPHR Enable", SND_SOC_NOPM, INTERP_HPHR,
		0, &anc_hphr_pa_switch, tavil_anc_out_switch_cb,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("RX_BIAS", SND_SOC_NOPM, 0, 0,
		tavil_codec_enable_rx_bias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("RX INT1 NATIVE SUPPLY", SND_SOC_NOPM,
		INTERP_HPHL, 0, tavil_enable_native_supply,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("RX INT2 NATIVE SUPPLY", SND_SOC_NOPM,
		INTERP_HPHR, 0, tavil_enable_native_supply,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("RX INT3 NATIVE SUPPLY", SND_SOC_NOPM,
		INTERP_LO1, 0, tavil_enable_native_supply,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("RX INT4 NATIVE SUPPLY", SND_SOC_NOPM,
		INTERP_LO2, 0, tavil_enable_native_supply,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("RX INT7 NATIVE SUPPLY", SND_SOC_NOPM,
		INTERP_SPKR1, 0, tavil_enable_native_supply,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("RX INT8 NATIVE SUPPLY", SND_SOC_NOPM,
		INTERP_SPKR2, 0, tavil_enable_native_supply,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	WCD_DAPM_MUX("RX INT1_1 NATIVE MUX", 0, int1_1_native),
	WCD_DAPM_MUX("RX INT2_1 NATIVE MUX", 0, int2_1_native),
	WCD_DAPM_MUX("RX INT3_1 NATIVE MUX", 0, int3_1_native),
	WCD_DAPM_MUX("RX INT4_1 NATIVE MUX", 0, int4_1_native),

	WCD_DAPM_MUX("RX INT1_2 NATIVE MUX", 0, int1_2_native),
	WCD_DAPM_MUX("RX INT2_2 NATIVE MUX", 0, int2_2_native),
	WCD_DAPM_MUX("RX INT3_2 NATIVE MUX", 0, int3_2_native),
	WCD_DAPM_MUX("RX INT4_2 NATIVE MUX", 0, int4_2_native),
	WCD_DAPM_MUX("RX INT7_2 NATIVE MUX", 0, int7_2_native),
	WCD_DAPM_MUX("RX INT8_2 NATIVE MUX", 0, int8_2_native),

	SND_SOC_DAPM_MUX_E("ASRC0 MUX", SND_SOC_NOPM, ASRC0, 0,
		&asrc0_mux, tavil_codec_enable_asrc_resampler,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("ASRC1 MUX", SND_SOC_NOPM, ASRC1, 0,
		&asrc1_mux, tavil_codec_enable_asrc_resampler,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("ASRC2 MUX", SND_SOC_NOPM, ASRC2, 0,
		&asrc2_mux, tavil_codec_enable_asrc_resampler,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("ASRC3 MUX", SND_SOC_NOPM, ASRC3, 0,
		&asrc3_mux, tavil_codec_enable_asrc_resampler,
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
	case AIF4_MAD_TX:
	case AIF4_VIFEED:
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
	struct wcd9xxx_codec_dai_data *dai_data = NULL;

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
	/* Reserve TX13 for MAD data channel */
	dai_data = &tavil->dai[AIF4_MAD_TX];
	if (dai_data)
		list_add_tail(&core->tx_chs[WCD934X_TX13].list,
			      &dai_data->wcd9xxx_ch_list);

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
	struct tavil_dsd_config *dsd_conf = tavil->dsd_config;

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
		if (dsd_conf)
			tavil_dsd_set_interp_rate(dsd_conf, ch->port,
						  sample_rate, rate_reg_val);
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

static int tavil_vi_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *dai)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(dai->codec);

	dev_dbg(tavil->dev, "%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n",
		 __func__, dai->name, dai->id, params_rate(params),
		 params_channels(params));

	tavil->dai[dai->id].rate = params_rate(params);
	tavil->dai[dai->id].bit_width = 32;

	return 0;
}

static int tavil_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(dai->codec);
	int ret = 0;

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
		if (dai->id != AIF4_MAD_TX)
			ret = tavil_set_decimator_rate(dai,
						       params_rate(params));
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

static struct snd_soc_dai_ops tavil_vi_dai_ops = {
	.hw_params = tavil_vi_hw_params,
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
	{
		.name = "tavil_vifeedback",
		.id = AIF4_VIFEED,
		.capture = {
			.stream_name = "VIfeed",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_48000,
			.formats = WCD934X_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 48000,
			.channels_min = 1,
			.channels_max = 4,
		 },
		.ops = &tavil_vi_dai_ops,
	},
	{
		.name = "tavil_mad1",
		.id = AIF4_MAD_TX,
		.capture = {
			.stream_name = "AIF4 MAD TX",
			.rates = SNDRV_PCM_RATE_16000,
			.formats = WCD934X_FORMATS_S16_LE,
			.rate_min = 16000,
			.rate_max = 16000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &tavil_dai_ops,
	},
};

static void tavil_codec_power_gate_digital_core(struct tavil_priv *tavil)
{
	mutex_lock(&tavil->power_lock);
	dev_dbg(tavil->dev, "%s: Entering power gating function, %d\n",
		__func__, tavil->power_active_ref);

	if (tavil->power_active_ref > 0)
		goto exit;

	wcd9xxx_set_power_state(tavil->wcd9xxx,
			WCD_REGION_POWER_COLLAPSE_BEGIN,
			WCD9XXX_DIG_CORE_REGION_1);
	regmap_update_bits(tavil->wcd9xxx->regmap,
			   WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x04, 0x04);
	regmap_update_bits(tavil->wcd9xxx->regmap,
			   WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x01, 0x00);
	regmap_update_bits(tavil->wcd9xxx->regmap,
			   WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x02, 0x00);
	wcd9xxx_set_power_state(tavil->wcd9xxx, WCD_REGION_POWER_DOWN,
				WCD9XXX_DIG_CORE_REGION_1);
exit:
	dev_dbg(tavil->dev, "%s: Exiting power gating function, %d\n",
		__func__, tavil->power_active_ref);
	mutex_unlock(&tavil->power_lock);
}

static void tavil_codec_power_gate_work(struct work_struct *work)
{
	struct tavil_priv *tavil;
	struct delayed_work *dwork;

	dwork = to_delayed_work(work);
	tavil = container_of(dwork, struct tavil_priv, power_gate_work);

	tavil_codec_power_gate_digital_core(tavil);
}

/* called under power_lock acquisition */
static int tavil_dig_core_remove_power_collapse(struct tavil_priv *tavil)
{
	regmap_write(tavil->wcd9xxx->regmap,
		     WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x05);
	regmap_write(tavil->wcd9xxx->regmap,
		     WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x07);
	regmap_update_bits(tavil->wcd9xxx->regmap,
			   WCD934X_CODEC_RPM_RST_CTL, 0x02, 0x00);
	regmap_update_bits(tavil->wcd9xxx->regmap,
			   WCD934X_CODEC_RPM_RST_CTL, 0x02, 0x02);
	regmap_write(tavil->wcd9xxx->regmap,
		     WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x03);

	wcd9xxx_set_power_state(tavil->wcd9xxx,
			WCD_REGION_POWER_COLLAPSE_REMOVE,
			WCD9XXX_DIG_CORE_REGION_1);
	regcache_mark_dirty(tavil->wcd9xxx->regmap);
	regcache_sync_region(tavil->wcd9xxx->regmap,
			     WCD934X_DIG_CORE_REG_MIN,
			     WCD934X_DIG_CORE_REG_MAX);

	return 0;
}

static int tavil_dig_core_power_collapse(struct tavil_priv *tavil,
					 int req_state)
{
	int cur_state;

	/* Exit if feature is disabled */
	if (!dig_core_collapse_enable)
		return 0;

	mutex_lock(&tavil->power_lock);
	if (req_state == POWER_COLLAPSE)
		tavil->power_active_ref--;
	else if (req_state == POWER_RESUME)
		tavil->power_active_ref++;
	else
		goto unlock_mutex;

	if (tavil->power_active_ref < 0) {
		dev_dbg(tavil->dev, "%s: power_active_ref is negative\n",
			__func__);
		goto unlock_mutex;
	}

	if (req_state == POWER_COLLAPSE) {
		if (tavil->power_active_ref == 0) {
			schedule_delayed_work(&tavil->power_gate_work,
			msecs_to_jiffies(dig_core_collapse_timer * 1000));
		}
	} else if (req_state == POWER_RESUME) {
		if (tavil->power_active_ref == 1) {
			/*
			 * At this point, there can be two cases:
			 * 1. Core already in power collapse state
			 * 2. Timer kicked in and still did not expire or
			 * waiting for the power_lock
			 */
			cur_state = wcd9xxx_get_current_power_state(
						tavil->wcd9xxx,
						WCD9XXX_DIG_CORE_REGION_1);
			if (cur_state == WCD_REGION_POWER_DOWN) {
				tavil_dig_core_remove_power_collapse(tavil);
			} else {
				mutex_unlock(&tavil->power_lock);
				cancel_delayed_work_sync(
						&tavil->power_gate_work);
				mutex_lock(&tavil->power_lock);
			}
		}
	}

unlock_mutex:
	mutex_unlock(&tavil->power_lock);

	return 0;
}

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
		tavil_dig_core_power_collapse(tavil, POWER_RESUME);
		tavil_vote_svs(tavil, true);
		ret = tavil_cdc_req_mclk_enable(tavil, true);
		if (ret)
			goto done;
	} else {
		tavil_cdc_req_mclk_enable(tavil, false);
		tavil_vote_svs(tavil, false);
		tavil_dig_core_power_collapse(tavil, POWER_COLLAPSE);
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

static ssize_t tavil_codec_version_read(struct snd_info_entry *entry,
					void *file_private_data,
					struct file *file,
					char __user *buf, size_t count,
					loff_t pos)
{
	struct tavil_priv *tavil;
	struct wcd9xxx *wcd9xxx;
	char buffer[TAVIL_VERSION_ENTRY_SIZE];
	int len = 0;

	tavil = (struct tavil_priv *) entry->private_data;
	if (!tavil) {
		pr_err("%s: tavil priv is null\n", __func__);
		return -EINVAL;
	}

	wcd9xxx = tavil->wcd9xxx;

	switch (wcd9xxx->version) {
	case TAVIL_VERSION_WCD9340_1_0:
	    len = snprintf(buffer, sizeof(buffer), "WCD9340_1_0\n");
	    break;
	case TAVIL_VERSION_WCD9341_1_0:
	    len = snprintf(buffer, sizeof(buffer), "WCD9341_1_0\n");
	    break;
	case TAVIL_VERSION_WCD9340_1_1:
	    len = snprintf(buffer, sizeof(buffer), "WCD9340_1_1\n");
	    break;
	case TAVIL_VERSION_WCD9341_1_1:
	    len = snprintf(buffer, sizeof(buffer), "WCD9341_1_1\n");
	    break;
	default:
	    len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops tavil_codec_info_ops = {
	.read = tavil_codec_version_read,
};

/*
 * tavil_codec_info_create_codec_entry - creates wcd934x module
 * @codec_root: The parent directory
 * @codec: Codec instance
 *
 * Creates wcd934x module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int tavil_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					struct snd_soc_codec *codec)
{
	struct snd_info_entry *version_entry;
	struct tavil_priv *tavil;
	struct snd_soc_card *card;

	if (!codec_root || !codec)
		return -EINVAL;

	tavil = snd_soc_codec_get_drvdata(codec);
	card = codec->component.card;
	tavil->entry = snd_register_module_info(codec_root->module,
						"tavil",
						codec_root);
	if (!tavil->entry) {
		dev_dbg(codec->dev, "%s: failed to create wcd934x entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   tavil->entry);
	if (!version_entry) {
		dev_dbg(codec->dev, "%s: failed to create wcd934x version entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry->private_data = tavil;
	version_entry->size = TAVIL_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &tavil_codec_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		return -ENOMEM;
	}
	tavil->version_entry = version_entry;

	return 0;
}
EXPORT_SYMBOL(tavil_codec_info_create_codec_entry);

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

static int __tavil_codec_internal_rco_ctrl(struct snd_soc_codec *codec,
					   bool enable)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	if (enable) {
		if (wcd_resmgr_get_clk_type(tavil->resmgr) ==
		    WCD_CLK_RCO) {
			ret = wcd_resmgr_enable_clk_block(tavil->resmgr,
							  WCD_CLK_RCO);
		} else {
			ret = tavil_cdc_req_mclk_enable(tavil, true);
			if (ret) {
				dev_err(codec->dev,
					"%s: mclk_enable failed, err = %d\n",
					__func__, ret);
				goto done;
			}
			ret = wcd_resmgr_enable_clk_block(tavil->resmgr,
							   WCD_CLK_RCO);
			ret |= tavil_cdc_req_mclk_enable(tavil, false);
		}

	} else {
		ret = wcd_resmgr_disable_clk_block(tavil->resmgr,
						   WCD_CLK_RCO);
	}

	if (ret) {
		dev_err(codec->dev, "%s: Error in %s RCO\n",
			__func__, (enable ? "enabling" : "disabling"));
		ret = -EINVAL;
	}

done:
	return ret;
}

/*
 * tavil_codec_internal_rco_ctrl: Enable/Disable codec's RCO clock
 * @codec: Handle to the codec
 * @enable: Indicates whether clock should be enabled or disabled
 */
static int tavil_codec_internal_rco_ctrl(struct snd_soc_codec *codec,
					 bool enable)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	WCD9XXX_V2_BG_CLK_LOCK(tavil->resmgr);
	ret = __tavil_codec_internal_rco_ctrl(codec, enable);
	WCD9XXX_V2_BG_CLK_UNLOCK(tavil->resmgr);
	return ret;
}

static const struct wcd_resmgr_cb tavil_resmgr_cb = {
	.cdc_rco_ctrl = __tavil_codec_internal_rco_ctrl,
};

static const struct tavil_reg_mask_val tavil_codec_mclk2_1_1_defaults[] = {
	{WCD934X_CLK_SYS_MCLK2_PRG1, 0x60, 0x20},
};

static const struct tavil_reg_mask_val tavil_codec_mclk2_1_0_defaults[] = {
	/*
	 * PLL Settings:
	 * Clock Root: MCLK2,
	 * Clock Source: EXT_CLK,
	 * Clock Destination: MCLK2
	 * Clock Freq In: 19.2MHz,
	 * Clock Freq Out: 11.2896MHz
	 */
	{WCD934X_CLK_SYS_MCLK2_PRG1, 0x60, 0x20},
	{WCD934X_CLK_SYS_INT_POST_DIV_REG0, 0xFF, 0x5E},
	{WCD934X_CLK_SYS_INT_POST_DIV_REG1, 0x1F, 0x1F},
	{WCD934X_CLK_SYS_INT_REF_DIV_REG0, 0xFF, 0x54},
	{WCD934X_CLK_SYS_INT_REF_DIV_REG1, 0xFF, 0x01},
	{WCD934X_CLK_SYS_INT_FILTER_REG1, 0x07, 0x04},
	{WCD934X_CLK_SYS_INT_PLL_L_VAL, 0xFF, 0x93},
	{WCD934X_CLK_SYS_INT_PLL_N_VAL, 0xFF, 0xFA},
	{WCD934X_CLK_SYS_INT_TEST_REG0, 0xFF, 0x90},
	{WCD934X_CLK_SYS_INT_PFD_CP_DSM_PROG, 0xFF, 0x7E},
	{WCD934X_CLK_SYS_INT_VCO_PROG, 0xFF, 0xF8},
	{WCD934X_CLK_SYS_INT_TEST_REG1, 0xFF, 0x68},
	{WCD934X_CLK_SYS_INT_LDO_LOCK_CFG, 0xFF, 0x40},
	{WCD934X_CLK_SYS_INT_DIG_LOCK_DET_CFG, 0xFF, 0x32},
};

static const struct tavil_reg_mask_val tavil_codec_reg_defaults[] = {
	{WCD934X_BIAS_VBG_FINE_ADJ, 0xFF, 0x75},
	{WCD934X_CODEC_CPR_SVS_CX_VDD, 0xFF, 0x7C}, /* value in svs mode */
	{WCD934X_CODEC_CPR_SVS2_CX_VDD, 0xFF, 0x58}, /* value in svs2 mode */
	{WCD934X_CDC_RX0_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_RX1_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_RX2_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_RX3_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_RX4_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_RX7_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_RX8_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD934X_CDC_COMPANDER8_CTL7, 0x1E, 0x18},
	{WCD934X_CDC_COMPANDER7_CTL7, 0x1E, 0x18},
	{WCD934X_CDC_RX0_RX_PATH_SEC0, 0x08, 0x0},
	{WCD934X_CDC_CLSH_DECAY_CTRL, 0x03, 0x0},
	{WCD934X_MICB1_TEST_CTL_2, 0x07, 0x01},
	{WCD934X_CDC_BOOST0_BOOST_CFG1, 0x3F, 0x12},
	{WCD934X_CDC_BOOST0_BOOST_CFG2, 0x1C, 0x08},
	{WCD934X_CDC_BOOST1_BOOST_CFG1, 0x3F, 0x12},
	{WCD934X_CDC_BOOST1_BOOST_CFG2, 0x1C, 0x08},
	{WCD934X_CPE_SS_CPARMAD_BUFRDY_INT_PERIOD, 0x1F, 0x09},
	{WCD934X_CDC_TX0_TX_PATH_CFG1, 0x01, 0x00},
	{WCD934X_CDC_TX1_TX_PATH_CFG1, 0x01, 0x00},
	{WCD934X_CDC_TX2_TX_PATH_CFG1, 0x01, 0x00},
	{WCD934X_CDC_TX3_TX_PATH_CFG1, 0x01, 0x00},
	{WCD934X_CDC_TX4_TX_PATH_CFG1, 0x01, 0x00},
	{WCD934X_CDC_TX5_TX_PATH_CFG1, 0x01, 0x00},
	{WCD934X_CDC_TX6_TX_PATH_CFG1, 0x01, 0x00},
	{WCD934X_CDC_TX7_TX_PATH_CFG1, 0x01, 0x00},
	{WCD934X_CDC_TX8_TX_PATH_CFG1, 0x01, 0x00},
	{WCD934X_RX_OCP_CTL, 0x0F, 0x02}, /* OCP number of attempts is 2 */
	{WCD934X_HPH_OCP_CTL, 0xFF, 0x3A}, /* OCP current limit */
	{WCD934X_HPH_L_TEST, 0x01, 0x01},
	{WCD934X_HPH_R_TEST, 0x01, 0x01},
	{WCD934X_CPE_FLL_CONFIG_CTL_2, 0xFF, 0x20},
};

static const struct tavil_reg_mask_val tavil_codec_reg_init_1_1_val[] = {
	{WCD934X_CDC_COMPANDER1_CTL7, 0x1E, 0x06},
	{WCD934X_CDC_COMPANDER2_CTL7, 0x1E, 0x06},
	{WCD934X_HPH_NEW_INT_RDAC_HD2_CTL_L, 0xFF, 0x84},
	{WCD934X_HPH_NEW_INT_RDAC_HD2_CTL_R, 0xFF, 0x84},
	{WCD934X_CDC_RX3_RX_PATH_SEC0, 0xFC, 0xF4},
	{WCD934X_CDC_RX4_RX_PATH_SEC0, 0xFC, 0xF4},
};

static const struct tavil_cpr_reg_defaults cpr_defaults[] = {
	{ 0x00000820, 0x00000094 },
	{ 0x00000fC0, 0x00000048 },
	{ 0x0000f000, 0x00000044 },
	{ 0x0000bb80, 0xC0000178 },
	{ 0x00000000, 0x00000160 },
	{ 0x10854522, 0x00000060 },
	{ 0x10854509, 0x00000064 },
	{ 0x108544dd, 0x00000068 },
	{ 0x108544ad, 0x0000006C },
	{ 0x0000077E, 0x00000070 },
	{ 0x000007da, 0x00000074 },
	{ 0x00000000, 0x00000078 },
	{ 0x00000000, 0x0000007C },
	{ 0x00042029, 0x00000080 },
	{ 0x4002002A, 0x00000090 },
	{ 0x4002002B, 0x00000090 },
};

static const struct tavil_reg_mask_val tavil_codec_reg_init_common_val[] = {
	{WCD934X_CDC_CLSH_K2_MSB, 0x0F, 0x00},
	{WCD934X_CDC_CLSH_K2_LSB, 0xFF, 0x60},
	{WCD934X_CPE_SS_DMIC_CFG, 0x80, 0x00},
	{WCD934X_CDC_BOOST0_BOOST_CTL, 0x70, 0x50},
	{WCD934X_CDC_BOOST1_BOOST_CTL, 0x70, 0x50},
	{WCD934X_CDC_RX7_RX_PATH_CFG1, 0x08, 0x08},
	{WCD934X_CDC_RX8_RX_PATH_CFG1, 0x08, 0x08},
	{WCD934X_CDC_TOP_TOP_CFG1, 0x02, 0x02},
	{WCD934X_CDC_TOP_TOP_CFG1, 0x01, 0x01},
	{WCD934X_CDC_TX9_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD934X_CDC_TX10_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD934X_CDC_TX11_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD934X_CDC_TX12_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD934X_DATA_HUB_SB_TX11_INP_CFG, 0x01, 0x01},
	{WCD934X_CDC_CLK_RST_CTRL_FS_CNT_CONTROL, 0x01, 0x01},
	{WCD934X_CDC_COMPANDER7_CTL3, 0x80, 0x80},
	{WCD934X_CDC_COMPANDER8_CTL3, 0x80, 0x80},
	{WCD934X_CDC_COMPANDER7_CTL7, 0x01, 0x01},
	{WCD934X_CDC_COMPANDER8_CTL7, 0x01, 0x01},
	{WCD934X_CODEC_RPM_CLK_GATE, 0x08, 0x00},
	{WCD934X_TLMM_DMIC3_CLK_PINCFG, 0xFF, 0x0a},
	{WCD934X_TLMM_DMIC3_DATA_PINCFG, 0xFF, 0x0a},
	{WCD934X_CPE_SS_SVA_CFG, 0x60, 0x00},
};

static void tavil_codec_init_reg(struct tavil_priv *priv)
{
	struct snd_soc_codec *codec = priv->codec;
	u32 i;

	for (i = 0; i < ARRAY_SIZE(tavil_codec_reg_init_common_val); i++)
		snd_soc_update_bits(codec,
				    tavil_codec_reg_init_common_val[i].reg,
				    tavil_codec_reg_init_common_val[i].mask,
				    tavil_codec_reg_init_common_val[i].val);

	if (TAVIL_IS_1_1(priv->wcd9xxx)) {
		for (i = 0; i < ARRAY_SIZE(tavil_codec_reg_init_1_1_val); i++)
			snd_soc_update_bits(codec,
					tavil_codec_reg_init_1_1_val[i].reg,
					tavil_codec_reg_init_1_1_val[i].mask,
					tavil_codec_reg_init_1_1_val[i].val);
	}
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

static void tavil_update_cpr_defaults(struct tavil_priv *tavil)
{
	int i;
	struct wcd9xxx *wcd9xxx;

	wcd9xxx = tavil->wcd9xxx;
	if (!TAVIL_IS_1_1(wcd9xxx))
		return;

	__tavil_cdc_mclk_enable(tavil, true);

	regmap_write(wcd9xxx->regmap, WCD934X_CODEC_CPR_SVS2_MIN_CX_VDD, 0x2C);
	regmap_update_bits(wcd9xxx->regmap, WCD934X_CODEC_RPM_CLK_GATE,
			   0x10, 0x00);

	for (i = 0; i < ARRAY_SIZE(cpr_defaults); i++) {
		regmap_bulk_write(wcd9xxx->regmap,
				WCD934X_CODEC_CPR_WR_DATA_0,
				(u8 *)&cpr_defaults[i].wr_data, 4);
		regmap_bulk_write(wcd9xxx->regmap,
				WCD934X_CODEC_CPR_WR_ADDR_0,
				(u8 *)&cpr_defaults[i].wr_addr, 4);
	}

	__tavil_cdc_mclk_enable(tavil, false);
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

static irqreturn_t tavil_misc_irq(int irq, void *data)
{
	struct tavil_priv *tavil = data;
	int misc_val;

	/* Find source of interrupt */
	regmap_read(tavil->wcd9xxx->regmap, WCD934X_INTR_CODEC_MISC_STATUS,
		    &misc_val);

	if (misc_val & 0x08) {
		dev_info(tavil->dev, "%s: irq: %d, DSD DC detected!\n",
			 __func__, irq);
		/* DSD DC interrupt, reset DSD path */
		tavil_dsd_reset(tavil->dsd_config);
	} else {
		dev_err(tavil->dev, "%s: Codec misc irq: %d, val: 0x%x\n",
			__func__, irq, misc_val);
	}

	/* Clear interrupt status */
	regmap_update_bits(tavil->wcd9xxx->regmap,
			   WCD934X_INTR_CODEC_MISC_CLEAR, misc_val, 0x00);

	return IRQ_HANDLED;
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

	/* Register for misc interrupts as well */
	ret = wcd9xxx_request_irq(core_res, WCD934X_IRQ_MISC,
				  tavil_misc_irq, "CDC MISC Irq", tavil);
	if (ret)
		dev_err(codec->dev, "%s: Failed to request cdc misc irq\n",
			__func__);

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
	wcd9xxx_free_irq(core_res, WCD934X_IRQ_MISC, tavil);
}

/*
 * wcd934x_get_micb_vout_ctl_val: converts micbias from volts to register value
 * @micb_mv: micbias in mv
 *
 * return register value converted
 */
int wcd934x_get_micb_vout_ctl_val(u32 micb_mv)
{
	/* min micbias voltage is 1V and maximum is 2.85V */
	if (micb_mv < 1000 || micb_mv > 2850) {
		pr_err("%s: unsupported micbias voltage\n", __func__);
		return -EINVAL;
	}

	return (micb_mv - 1000) / 50;
}
EXPORT_SYMBOL(wcd934x_get_micb_vout_ctl_val);

static int tavil_handle_pdata(struct tavil_priv *tavil,
			      struct wcd9xxx_pdata *pdata)
{
	struct snd_soc_codec *codec = tavil->codec;
	u8 mad_dmic_ctl_val;
	u8 anc_ctl_value;
	u32 def_dmic_rate, dmic_clk_drv;
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

	/* Set the DMIC sample rate */
	switch (pdata->mclk_rate) {
	case WCD934X_MCLK_CLK_9P6MHZ:
		def_dmic_rate = WCD9XXX_DMIC_SAMPLE_RATE_4P8MHZ;
		break;
	case WCD934X_MCLK_CLK_12P288MHZ:
		def_dmic_rate = WCD9XXX_DMIC_SAMPLE_RATE_4P096MHZ;
		break;
	default:
		/* should never happen */
		dev_err(codec->dev, "%s: Invalid mclk_rate %d\n",
			__func__, pdata->mclk_rate);
		rc = -EINVAL;
		goto done;
	};

	if (pdata->dmic_sample_rate ==
	    WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED) {
		dev_info(codec->dev, "%s: dmic_rate invalid default = %d\n",
			__func__, def_dmic_rate);
		pdata->dmic_sample_rate = def_dmic_rate;
	}
	if (pdata->mad_dmic_sample_rate ==
	    WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED) {
		dev_info(codec->dev, "%s: mad_dmic_rate invalid default = %d\n",
			__func__, def_dmic_rate);
		/*
		 * use dmic_sample_rate as the default for MAD
		 * if mad dmic sample rate is undefined
		 */
		pdata->mad_dmic_sample_rate = pdata->dmic_sample_rate;
	}

	if (pdata->dmic_clk_drv ==
	    WCD9XXX_DMIC_CLK_DRIVE_UNDEFINED) {
		pdata->dmic_clk_drv = WCD934X_DMIC_CLK_DRIVE_DEFAULT;
		dev_dbg(codec->dev,
			 "%s: dmic_clk_strength invalid, default = %d\n",
			 __func__, pdata->dmic_clk_drv);
	}

	switch (pdata->dmic_clk_drv) {
	case 2:
		dmic_clk_drv = 0;
		break;
	case 4:
		dmic_clk_drv = 1;
		break;
	case 8:
		dmic_clk_drv = 2;
		break;
	case 16:
		dmic_clk_drv = 3;
		break;
	default:
		dev_err(codec->dev,
			"%s: invalid dmic_clk_drv %d, using default\n",
			__func__, pdata->dmic_clk_drv);
		dmic_clk_drv = 0;
		break;
	}

	snd_soc_update_bits(codec, WCD934X_TEST_DEBUG_PAD_DRVCTL_0,
			    0x0C, dmic_clk_drv << 2);

	/*
	 * Default the DMIC clk rates to mad_dmic_sample_rate,
	 * whereas, the anc/txfe dmic rates to dmic_sample_rate
	 * since the anc/txfe are independent of mad block.
	 */
	mad_dmic_ctl_val = tavil_get_dmic_clk_val(tavil->codec,
				pdata->mclk_rate,
				pdata->mad_dmic_sample_rate);
	snd_soc_update_bits(codec, WCD934X_CPE_SS_DMIC0_CTL,
		0x0E, mad_dmic_ctl_val << 1);
	snd_soc_update_bits(codec, WCD934X_CPE_SS_DMIC1_CTL,
		0x0E, mad_dmic_ctl_val << 1);
	snd_soc_update_bits(codec, WCD934X_CPE_SS_DMIC2_CTL,
		0x0E, mad_dmic_ctl_val << 1);

	if (dmic_clk_drv == WCD934X_DMIC_CLK_DIV_2)
		anc_ctl_value = WCD934X_ANC_DMIC_X2_FULL_RATE;
	else
		anc_ctl_value = WCD934X_ANC_DMIC_X2_HALF_RATE;

	snd_soc_update_bits(codec, WCD934X_CDC_ANC0_MODE_2_CTL,
			    0x40, anc_ctl_value << 6);
	snd_soc_update_bits(codec, WCD934X_CDC_ANC0_MODE_2_CTL,
			    0x20, anc_ctl_value << 5);
	snd_soc_update_bits(codec, WCD934X_CDC_ANC1_MODE_2_CTL,
			    0x40, anc_ctl_value << 6);
	snd_soc_update_bits(codec, WCD934X_CDC_ANC1_MODE_2_CTL,
			    0x20, anc_ctl_value << 5);

done:
	return rc;
}

static void tavil_cdc_vote_svs(struct snd_soc_codec *codec, bool vote)
{
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	return tavil_vote_svs(tavil, vote);
}

struct wcd_dsp_cdc_cb cdc_cb = {
	.cdc_clk_en = tavil_codec_internal_rco_ctrl,
	.cdc_vote_svs = tavil_cdc_vote_svs,
};

static int tavil_wdsp_initialize(struct snd_soc_codec *codec)
{
	struct wcd9xxx *control;
	struct tavil_priv *tavil;
	struct wcd_dsp_params params;
	int ret = 0;

	control = dev_get_drvdata(codec->dev->parent);
	tavil = snd_soc_codec_get_drvdata(codec);

	params.cb = &cdc_cb;
	params.irqs.cpe_ipc1_irq = WCD934X_IRQ_CPE1_INTR;
	params.irqs.cpe_err_irq = WCD934X_IRQ_CPE_ERROR;
	params.irqs.fatal_irqs = CPE_FATAL_IRQS;
	params.clk_rate = control->mclk_rate;
	params.dsp_instance = 0;

	wcd_dsp_cntl_init(codec, &params, &tavil->wdsp_cntl);
	if (!tavil->wdsp_cntl) {
		dev_err(tavil->dev, "%s: wcd-dsp-control init failed\n",
			__func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * tavil_soc_get_mbhc: get wcd934x_mbhc handle of corresponding codec
 * @codec: handle to snd_soc_codec *
 *
 * return wcd934x_mbhc handle or error code in case of failure
 */
struct wcd934x_mbhc *tavil_soc_get_mbhc(struct snd_soc_codec *codec)
{
	struct tavil_priv *tavil;

	if (!codec) {
		pr_err("%s: Invalid params, NULL codec\n", __func__);
		return NULL;
	}
	tavil = snd_soc_codec_get_drvdata(codec);

	if (!tavil) {
		pr_err("%s: Invalid params, NULL tavil\n", __func__);
		return NULL;
	}

	return tavil->mbhc;
}
EXPORT_SYMBOL(tavil_soc_get_mbhc);

static void tavil_mclk2_reg_defaults(struct tavil_priv *tavil)
{
	int i;
	struct snd_soc_codec *codec = tavil->codec;

	if (TAVIL_IS_1_0(tavil->wcd9xxx)) {
		/* MCLK2 configuration */
		for (i = 0; i < ARRAY_SIZE(tavil_codec_mclk2_1_0_defaults); i++)
			snd_soc_update_bits(codec,
					tavil_codec_mclk2_1_0_defaults[i].reg,
					tavil_codec_mclk2_1_0_defaults[i].mask,
					tavil_codec_mclk2_1_0_defaults[i].val);
	}
	if (TAVIL_IS_1_1(tavil->wcd9xxx)) {
		/* MCLK2 configuration */
		for (i = 0; i < ARRAY_SIZE(tavil_codec_mclk2_1_1_defaults); i++)
			snd_soc_update_bits(codec,
					tavil_codec_mclk2_1_1_defaults[i].reg,
					tavil_codec_mclk2_1_1_defaults[i].mask,
					tavil_codec_mclk2_1_1_defaults[i].val);
	}
}

static int tavil_device_down(struct wcd9xxx *wcd9xxx)
{
	struct snd_soc_codec *codec;
	struct tavil_priv *priv;
	int count;

	codec = (struct snd_soc_codec *)(wcd9xxx->ssr_priv);
	priv = snd_soc_codec_get_drvdata(codec);
	swrm_wcd_notify(priv->swr.ctrl_data[0].swr_pdev,
			SWR_DEVICE_DOWN, NULL);
	tavil_dsd_reset(priv->dsd_config);
	snd_soc_card_change_online_state(codec->component.card, 0);
	for (count = 0; count < NUM_CODEC_DAIS; count++)
		priv->dai[count].bus_down_in_recovery = true;
	wcd_dsp_ssr_event(priv->wdsp_cntl, WCD_CDC_DOWN_EVENT);
	wcd_resmgr_set_sido_input_src_locked(priv->resmgr,
					     SIDO_SOURCE_INTERNAL);

	return 0;
}

static int tavil_post_reset_cb(struct wcd9xxx *wcd9xxx)
{
	int i, ret = 0;
	struct wcd9xxx *control;
	struct snd_soc_codec *codec;
	struct tavil_priv *tavil;
	struct wcd9xxx_pdata *pdata;
	struct wcd_mbhc *mbhc;

	codec = (struct snd_soc_codec *)(wcd9xxx->ssr_priv);
	tavil = snd_soc_codec_get_drvdata(codec);
	control = dev_get_drvdata(codec->dev->parent);

	wcd9xxx_set_power_state(tavil->wcd9xxx,
				WCD_REGION_POWER_COLLAPSE_REMOVE,
				WCD9XXX_DIG_CORE_REGION_1);

	mutex_lock(&tavil->codec_mutex);

	tavil_vote_svs(tavil, true);
	tavil_slimbus_slave_port_cfg.slave_dev_intfdev_la =
				control->slim_slave->laddr;
	tavil_slimbus_slave_port_cfg.slave_dev_pgd_la =
					control->slim->laddr;
	tavil_init_slim_slave_cfg(codec);
	snd_soc_card_change_online_state(codec->component.card, 1);

	for (i = 0; i < TAVIL_MAX_MICBIAS; i++)
		tavil->micb_ref[i] = 0;

	dev_dbg(codec->dev, "%s: MCLK Rate = %x\n",
		__func__, control->mclk_rate);

	if (control->mclk_rate == WCD934X_MCLK_CLK_12P288MHZ)
		snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_MCLK_CFG,
				    0x03, 0x00);
	else if (control->mclk_rate == WCD934X_MCLK_CLK_9P6MHZ)
		snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_MCLK_CFG,
				    0x03, 0x01);
	wcd_resmgr_post_ssr_v2(tavil->resmgr);
	tavil_update_reg_defaults(tavil);
	tavil_codec_init_reg(tavil);
	__tavil_enable_efuse_sensing(tavil);
	tavil_mclk2_reg_defaults(tavil);

	__tavil_cdc_mclk_enable(tavil, true);
	regcache_mark_dirty(codec->component.regmap);
	regcache_sync(codec->component.regmap);
	__tavil_cdc_mclk_enable(tavil, false);

	tavil_update_cpr_defaults(tavil);

	pdata = dev_get_platdata(codec->dev->parent);
	ret = tavil_handle_pdata(tavil, pdata);
	if (IS_ERR_VALUE(ret))
		dev_err(codec->dev, "%s: invalid pdata\n", __func__);

	/* Initialize MBHC module */
	mbhc = &tavil->mbhc->wcd_mbhc;
	ret = tavil_mbhc_post_ssr_init(tavil->mbhc, codec);
	if (ret) {
		dev_err(codec->dev, "%s: mbhc initialization failed\n",
			__func__);
		goto done;
	} else {
		tavil_mbhc_hs_detect(codec, mbhc->mbhc_cfg);
	}

	/* DSD initialization */
	ret = tavil_dsd_post_ssr_init(tavil->dsd_config);
	if (ret)
		dev_dbg(tavil->dev, "%s: DSD init failed\n", __func__);

	tavil_cleanup_irqs(tavil);
	ret = tavil_setup_irqs(tavil);
	if (ret) {
		dev_err(codec->dev, "%s: tavil irq setup failed %d\n",
			__func__, ret);
		goto done;
	}

	tavil_set_spkr_mode(codec, tavil->swr.spkr_mode);
	/*
	 * Once the codec initialization is completed, the svs vote
	 * can be released allowing the codec to go to SVS2.
	 */
	tavil_vote_svs(tavil, false);
	wcd_dsp_ssr_event(tavil->wdsp_cntl, WCD_CDC_UP_EVENT);

done:
	mutex_unlock(&tavil->codec_mutex);
	return ret;
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
	tavil->intf_type = wcd9xxx_get_intf_type();

	control->dev_down = tavil_device_down;
	control->post_reset = tavil_post_reset_cb;
	control->ssr_priv = (void *)codec;

	/* Resource Manager post Init */
	ret = wcd_resmgr_post_init(tavil->resmgr, &tavil_resmgr_cb, codec);
	if (ret) {
		dev_err(codec->dev, "%s: wcd resmgr post init failed\n",
			__func__);
		goto err;
	}
	/* Class-H Init */
	wcd_clsh_init(&tavil->clsh_d);
	/* Default HPH Mode to Class-H Low HiFi */
	tavil->hph_mode = CLS_H_LOHIFI;

	tavil->fw_data = devm_kzalloc(codec->dev, sizeof(*(tavil->fw_data)),
				      GFP_KERNEL);
	if (!tavil->fw_data)
		goto err;

	set_bit(WCD9XXX_ANC_CAL, tavil->fw_data->cal_bit);
	set_bit(WCD9XXX_MBHC_CAL, tavil->fw_data->cal_bit);
	set_bit(WCD9XXX_MAD_CAL, tavil->fw_data->cal_bit);
	set_bit(WCD9XXX_VBAT_CAL, tavil->fw_data->cal_bit);

	ret = wcd_cal_create_hwdep(tavil->fw_data,
				   WCD9XXX_CODEC_HWDEP_NODE, codec);
	if (IS_ERR_VALUE(ret)) {
		dev_err(codec->dev, "%s hwdep failed %d\n", __func__, ret);
		goto err_hwdep;
	}

	/* Initialize MBHC module */
	ret = tavil_mbhc_init(&tavil->mbhc, codec, tavil->fw_data);
	if (ret) {
		pr_err("%s: mbhc initialization failed\n", __func__);
		goto err_hwdep;
	}

	tavil->codec = codec;
	for (i = 0; i < COMPANDER_MAX; i++)
		tavil->comp_enabled[i] = 0;

	tavil_codec_init_reg(tavil);

	pdata = dev_get_platdata(codec->dev->parent);
	ret = tavil_handle_pdata(tavil, pdata);
	if (IS_ERR_VALUE(ret)) {
		dev_err(codec->dev, "%s: bad pdata\n", __func__);
		goto err_hwdep;
	}

	ptr = devm_kzalloc(codec->dev, (sizeof(tavil_rx_chs) +
			   sizeof(tavil_tx_chs)), GFP_KERNEL);
	if (!ptr) {
		ret = -ENOMEM;
		goto err_hwdep;
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

	tavil->spk_anc_dwork.tavil = tavil;
	INIT_DELAYED_WORK(&tavil->spk_anc_dwork.dwork,
			  tavil_spk_anc_update_callback);

	tavil_mclk2_reg_defaults(tavil);

	/* DSD initialization */
	tavil->dsd_config = tavil_dsd_init(codec);
	if (IS_ERR_OR_NULL(tavil->dsd_config))
		dev_dbg(tavil->dev, "%s: DSD init failed\n", __func__);

	mutex_lock(&tavil->codec_mutex);
	snd_soc_dapm_disable_pin(dapm, "ANC EAR PA");
	snd_soc_dapm_disable_pin(dapm, "ANC EAR");
	snd_soc_dapm_disable_pin(dapm, "ANC HPHL PA");
	snd_soc_dapm_disable_pin(dapm, "ANC HPHR PA");
	snd_soc_dapm_disable_pin(dapm, "ANC HPHL");
	snd_soc_dapm_disable_pin(dapm, "ANC HPHR");
	snd_soc_dapm_enable_pin(dapm, "ANC SPK1 PA");
	mutex_unlock(&tavil->codec_mutex);

	snd_soc_dapm_ignore_suspend(dapm, "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AIF1 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "AIF3 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AIF3 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "AIF4 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AIF4 MAD TX");
	snd_soc_dapm_ignore_suspend(dapm, "VIfeed");

	snd_soc_dapm_sync(dapm);

	tavil_wdsp_initialize(codec);

	/*
	 * Once the codec initialization is completed, the svs vote
	 * can be released allowing the codec to go to SVS2.
	 */
	tavil_vote_svs(tavil, false);

	return ret;

err_pdata:
	devm_kfree(codec->dev, ptr);
	control->rx_chs = NULL;
	control->tx_chs = NULL;
err_hwdep:
	devm_kfree(codec->dev, tavil->fw_data);
	tavil->fw_data = NULL;
err:
	return ret;
}

static int tavil_soc_codec_remove(struct snd_soc_codec *codec)
{
	struct wcd9xxx *control;
	struct tavil_priv *tavil = snd_soc_codec_get_drvdata(codec);

	control = dev_get_drvdata(codec->dev->parent);
	devm_kfree(codec->dev, control->rx_chs);
	control->rx_chs = NULL;
	control->tx_chs = NULL;
	tavil_cleanup_irqs(tavil);

	if (tavil->wdsp_cntl)
		wcd_dsp_cntl_deinit(&tavil->wdsp_cntl);

	/* Deinitialize MBHC module */
	tavil_mbhc_deinit(codec);
	tavil->mbhc = NULL;

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
	if (delayed_work_pending(&tavil->power_gate_work) &&
	    cancel_delayed_work_sync(&tavil->power_gate_work))
		tavil_codec_power_gate_digital_core(tavil);
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
			regmap_update_bits(tavil->wcd9xxx->regmap,
					WCD934X_TEST_DEBUG_NPL_DLY_TEST_1,
					0x10, 0x00);
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
			regmap_update_bits(tavil->wcd9xxx->regmap,
					WCD934X_TEST_DEBUG_NPL_DLY_TEST_1,
					0x10, 0x10);
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

static void tavil_codec_add_spi_device(struct tavil_priv *tavil,
				       struct device_node *node)
{
	struct spi_master *master;
	struct spi_device *spi;
	u32 prop_value;
	int rc;

	/* Read the master bus num from DT node */
	rc = of_property_read_u32(node, "qcom,master-bus-num",
				  &prop_value);
	if (IS_ERR_VALUE(rc)) {
		dev_err(tavil->dev, "%s: prop %s not found in node %s",
			__func__, "qcom,master-bus-num", node->full_name);
		goto done;
	}

	/* Get the reference to SPI master */
	master = spi_busnum_to_master(prop_value);
	if (!master) {
		dev_err(tavil->dev, "%s: Invalid spi_master for bus_num %u\n",
			__func__, prop_value);
		goto done;
	}

	/* Allocate the spi device */
	spi = spi_alloc_device(master);
	if (!spi) {
		dev_err(tavil->dev, "%s: spi_alloc_device failed\n",
			__func__);
		goto err_spi_alloc_dev;
	}

	/* Initialize device properties */
	if (of_modalias_node(node, spi->modalias,
			     sizeof(spi->modalias)) < 0) {
		dev_err(tavil->dev, "%s: cannot find modalias for %s\n",
			__func__, node->full_name);
		goto err_dt_parse;
	}

	rc = of_property_read_u32(node, "qcom,chip-select",
				  &prop_value);
	if (IS_ERR_VALUE(rc)) {
		dev_err(tavil->dev, "%s: prop %s not found in node %s",
			__func__, "qcom,chip-select", node->full_name);
		goto err_dt_parse;
	}
	spi->chip_select = prop_value;

	rc = of_property_read_u32(node, "qcom,max-frequency",
				  &prop_value);
	if (IS_ERR_VALUE(rc)) {
		dev_err(tavil->dev, "%s: prop %s not found in node %s",
			__func__, "qcom,max-frequency", node->full_name);
		goto err_dt_parse;
	}
	spi->max_speed_hz = prop_value;

	spi->dev.of_node = node;

	rc = spi_add_device(spi);
	if (IS_ERR_VALUE(rc)) {
		dev_err(tavil->dev, "%s: spi_add_device failed\n", __func__);
		goto err_dt_parse;
	}

	/* Put the reference to SPI master */
	put_device(&master->dev);

	return;

err_dt_parse:
	spi_dev_put(spi);

err_spi_alloc_dev:
	/* Put the reference to SPI master */
	put_device(&master->dev);
done:
	return;
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

		/* Parse and add the SPI device node */
		if (!strcmp(node->name, "wcd_spi")) {
			tavil_codec_add_spi_device(tavil, node);
			continue;
		}

		/* Parse other child device nodes and add platform device */
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
		WARN(1, "%s: Efuse sense is not complete val=%x, ret=%d\n",
			__func__, val, rc);

	__tavil_cdc_mclk_enable(tavil, false);

	return rc;
}

static void ___tavil_get_codec_fine_version(struct tavil_priv *tavil)
{
	int val1, val2, version;
	struct regmap *regmap;
	u16 id_minor;
	u32 version_mask = 0;

	regmap = tavil->wcd9xxx->regmap;
	version = tavil->wcd9xxx->version;
	id_minor = tavil->wcd9xxx->codec_type->id_minor;

	regmap_read(regmap, WCD934X_CHIP_TIER_CTRL_EFUSE_VAL_OUT14, &val1);
	regmap_read(regmap, WCD934X_CHIP_TIER_CTRL_EFUSE_VAL_OUT15, &val2);

	dev_dbg(tavil->dev, "%s: chip version :0x%x 0x:%x\n",
		__func__, val1, val2);

	version_mask |= (!!((u8)val1 & 0x80)) << DSD_DISABLED_MASK;
	version_mask |= (!!((u8)val2 & 0x01)) << SLNQ_DISABLED_MASK;

	switch (version_mask) {
	case DSD_DISABLED | SLNQ_DISABLED:
	    if (id_minor == cpu_to_le16(0))
		version = TAVIL_VERSION_WCD9340_1_0;
	    else if (id_minor == cpu_to_le16(0x01))
		version = TAVIL_VERSION_WCD9340_1_1;
	    break;
	case SLNQ_DISABLED:
	    if (id_minor == cpu_to_le16(0))
		version = TAVIL_VERSION_WCD9341_1_0;
	    else if (id_minor == cpu_to_le16(0x01))
		version = TAVIL_VERSION_WCD9341_1_1;
	    break;
	}

	tavil->wcd9xxx->version = version;
	tavil->wcd9xxx->codec_type->version = version;
}

/*
 * tavil_get_wcd_dsp_cntl: Get the reference to wcd_dsp_cntl
 * @dev: Device pointer for codec device
 *
 * This API gets the reference to codec's struct wcd_dsp_cntl
 */
struct wcd_dsp_cntl *tavil_get_wcd_dsp_cntl(struct device *dev)
{
	struct platform_device *pdev;
	struct tavil_priv *tavil;

	if (!dev) {
		pr_err("%s: Invalid device\n", __func__);
		return NULL;
	}

	pdev = to_platform_device(dev);
	tavil = platform_get_drvdata(pdev);

	return tavil->wdsp_cntl;
}
EXPORT_SYMBOL(tavil_get_wcd_dsp_cntl);

static int tavil_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tavil_priv *tavil;
	struct clk *wcd_ext_clk;
	struct wcd9xxx_resmgr_v2 *resmgr;
	struct wcd9xxx_power_region *cdc_pwr;

	tavil = devm_kzalloc(&pdev->dev, sizeof(struct tavil_priv),
			    GFP_KERNEL);
	if (!tavil)
		return -ENOMEM;

	platform_set_drvdata(pdev, tavil);

	tavil->wcd9xxx = dev_get_drvdata(pdev->dev.parent);
	tavil->dev = &pdev->dev;
	INIT_DELAYED_WORK(&tavil->power_gate_work, tavil_codec_power_gate_work);
	mutex_init(&tavil->power_lock);
	INIT_WORK(&tavil->tavil_add_child_devices_work,
		  tavil_add_child_devices);
	mutex_init(&tavil->micb_lock);
	mutex_init(&tavil->swr.read_mutex);
	mutex_init(&tavil->swr.write_mutex);
	mutex_init(&tavil->swr.clk_mutex);
	mutex_init(&tavil->codec_mutex);
	mutex_init(&tavil->svs_mutex);

	/*
	 * Codec hardware by default comes up in SVS mode.
	 * Initialize the svs_ref_cnt to 1 to reflect the hardware
	 * state in the driver.
	 */
	tavil->svs_ref_cnt = 1;

	cdc_pwr = devm_kzalloc(&pdev->dev, sizeof(struct wcd9xxx_power_region),
				GFP_KERNEL);
	if (!cdc_pwr) {
		ret = -ENOMEM;
		goto err_resmgr;
	}
	tavil->wcd9xxx->wcd9xxx_pwr[WCD9XXX_DIG_CORE_REGION_1] = cdc_pwr;
	cdc_pwr->pwr_collapse_reg_min = WCD934X_DIG_CORE_REG_MIN;
	cdc_pwr->pwr_collapse_reg_max = WCD934X_DIG_CORE_REG_MAX;
	wcd9xxx_set_power_state(tavil->wcd9xxx,
				WCD_REGION_POWER_COLLAPSE_REMOVE,
				WCD9XXX_DIG_CORE_REGION_1);
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
	tavil->swr.spkr_gain_offset = WCD934X_RX_GAIN_OFFSET_0_DB;

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
	dev_dbg(&pdev->dev, "%s: MCLK Rate = %x\n", __func__,
		tavil->wcd9xxx->mclk_rate);
	if (tavil->wcd9xxx->mclk_rate == WCD934X_MCLK_CLK_12P288MHZ)
		regmap_update_bits(tavil->wcd9xxx->regmap,
				   WCD934X_CODEC_RPM_CLK_MCLK_CFG,
				   0x03, 0x00);
	else if (tavil->wcd9xxx->mclk_rate == WCD934X_MCLK_CLK_9P6MHZ)
		regmap_update_bits(tavil->wcd9xxx->regmap,
				   WCD934X_CODEC_RPM_CLK_MCLK_CFG,
				   0x03, 0x01);
	tavil_update_reg_defaults(tavil);
	__tavil_enable_efuse_sensing(tavil);
	___tavil_get_codec_fine_version(tavil);
	tavil_update_cpr_defaults(tavil);

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
	mutex_destroy(&tavil->micb_lock);
	mutex_destroy(&tavil->svs_mutex);
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

	mutex_destroy(&tavil->micb_lock);
	mutex_destroy(&tavil->svs_mutex);
	mutex_destroy(&tavil->codec_mutex);
	mutex_destroy(&tavil->swr.read_mutex);
	mutex_destroy(&tavil->swr.write_mutex);
	mutex_destroy(&tavil->swr.clk_mutex);

	snd_soc_unregister_codec(&pdev->dev);
	clk_put(tavil->wcd_ext_clk);
	wcd_resmgr_remove(tavil->resmgr);
	if (tavil->dsd_config) {
		tavil_dsd_deinit(tavil->dsd_config);
		tavil->dsd_config = NULL;
	}
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
