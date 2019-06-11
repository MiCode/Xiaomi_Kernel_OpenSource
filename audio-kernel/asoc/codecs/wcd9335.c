/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include <soc/swr-wcd.h>
#include <soc/snd_event.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/info.h>
#include "core.h"
#include "pdata.h"
#include "wcd9335.h"
#include "wcd-mbhc-v2.h"
#include "wcd9xxx-common-v2.h"
#include "wcd9xxx-resmgr-v2.h"
#include "wcd9xxx-irq.h"
#include "wcd9335_registers.h"
#include "wcd9335_irq.h"
#include "wcd_cpe_core.h"
#include "wcdcal-hwdep.h"
#include "wcd-mbhc-v2-api.h"

#define TASHA_RX_PORT_START_NUMBER  16

#define WCD9335_RATES_MASK (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			    SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			    SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
/* Fractional Rates */
#define WCD9335_FRAC_RATES_MASK (SNDRV_PCM_RATE_44100)

#define WCD9335_MIX_RATES_MASK (SNDRV_PCM_RATE_48000 |\
				SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)

#define TASHA_FORMATS_S16_S24_LE (SNDRV_PCM_FMTBIT_S16_LE | \
				  SNDRV_PCM_FMTBIT_S24_LE | \
				  SNDRV_PCM_FMTBIT_S24_3LE)

#define TASHA_FORMATS_S16_S24_S32_LE (SNDRV_PCM_FMTBIT_S16_LE | \
				  SNDRV_PCM_FMTBIT_S24_LE | \
				  SNDRV_PCM_FMTBIT_S24_3LE | \
				  SNDRV_PCM_FMTBIT_S32_LE)

#define TASHA_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)

/*
 * Timeout in milli seconds and it is the wait time for
 * slim channel removal interrupt to receive.
 */
#define TASHA_SLIM_CLOSE_TIMEOUT 1000
#define TASHA_SLIM_IRQ_OVERFLOW (1 << 0)
#define TASHA_SLIM_IRQ_UNDERFLOW (1 << 1)
#define TASHA_SLIM_IRQ_PORT_CLOSED (1 << 2)
#define TASHA_MCLK_CLK_12P288MHZ 12288000
#define TASHA_MCLK_CLK_9P6MHZ 9600000

#define TASHA_SLIM_PGD_PORT_INT_TX_EN0 (TASHA_SLIM_PGD_PORT_INT_EN0 + 2)

#define TASHA_NUM_INTERPOLATORS 9
#define TASHA_NUM_DECIMATORS 9

#define WCD9335_CHILD_DEVICES_MAX	6

#define BYTE_BIT_MASK(nr) (1 << ((nr) % BITS_PER_BYTE))
#define TASHA_MAD_AUDIO_FIRMWARE_PATH "wcd9335/wcd9335_mad_audio.bin"
#define TASHA_CPE_SS_ERR_STATUS_MEM_ACCESS (1 << 0)
#define TASHA_CPE_SS_ERR_STATUS_WDOG_BITE (1 << 1)

#define TASHA_CPE_FATAL_IRQS \
	(TASHA_CPE_SS_ERR_STATUS_WDOG_BITE | \
	 TASHA_CPE_SS_ERR_STATUS_MEM_ACCESS)

#define SLIM_BW_CLK_GEAR_9 6200000
#define SLIM_BW_UNVOTE 0

#define CPE_FLL_CLK_75MHZ 75000000
#define CPE_FLL_CLK_150MHZ 150000000
#define WCD9335_REG_BITS 8

#define WCD9335_MAX_VALID_ADC_MUX  13
#define WCD9335_INVALID_ADC_MUX 9

#define TASHA_DIG_CORE_REG_MIN  WCD9335_CDC_ANC0_CLK_RESET_CTL
#define TASHA_DIG_CORE_REG_MAX  0xDFF

/* Convert from vout ctl to micbias voltage in mV */
#define WCD_VOUT_CTL_TO_MICB(v) (1000 + v * 50)

#define TASHA_ZDET_NUM_MEASUREMENTS 900
#define TASHA_MBHC_GET_C1(c)  ((c & 0xC000) >> 14)
#define TASHA_MBHC_GET_X1(x)  (x & 0x3FFF)
/* z value compared in milliOhm */
#define TASHA_MBHC_IS_SECOND_RAMP_REQUIRED(z) ((z > 400000) || (z < 32000))
#define TASHA_MBHC_ZDET_CONST  (86 * 16384)
#define TASHA_MBHC_MOISTURE_VREF  V_45_MV
#define TASHA_MBHC_MOISTURE_IREF  I_3P0_UA

#define TASHA_VERSION_ENTRY_SIZE 17

#define WCD9335_AMIC_PWR_LEVEL_LP 0
#define WCD9335_AMIC_PWR_LEVEL_DEFAULT 1
#define WCD9335_AMIC_PWR_LEVEL_HP 2
#define WCD9335_AMIC_PWR_LVL_MASK 0x60
#define WCD9335_AMIC_PWR_LVL_SHIFT 0x5

#define WCD9335_DEC_PWR_LVL_MASK 0x06
#define WCD9335_DEC_PWR_LVL_LP 0x02
#define WCD9335_DEC_PWR_LVL_HP 0x04
#define WCD9335_DEC_PWR_LVL_DF 0x00
#define WCD9335_STRING_LEN 100

#define CALCULATE_VOUT_D(req_mv) (((req_mv - 650) * 10) / 25)

static int cpe_debug_mode;

#define TASHA_MAX_MICBIAS 4
#define DAPM_MICBIAS1_STANDALONE "MIC BIAS1 Standalone"
#define DAPM_MICBIAS2_STANDALONE "MIC BIAS2 Standalone"
#define DAPM_MICBIAS3_STANDALONE "MIC BIAS3 Standalone"
#define DAPM_MICBIAS4_STANDALONE "MIC BIAS4 Standalone"

#define DAPM_LDO_H_STANDALONE "LDO_H"
module_param(cpe_debug_mode, int, 0664);
MODULE_PARM_DESC(cpe_debug_mode, "boot cpe in debug mode");

#define TASHA_DIG_CORE_COLLAPSE_TIMER_MS  (5 * 1000)

#define MAX_ON_DEMAND_SUPPLY_NAME_LENGTH    64

static char on_demand_supply_name[][MAX_ON_DEMAND_SUPPLY_NAME_LENGTH] = {
	"cdc-vdd-mic-bias",
};

enum {
	POWER_COLLAPSE,
	POWER_RESUME,
};

enum tasha_sido_voltage {
	SIDO_VOLTAGE_SVS_MV = 950,
	SIDO_VOLTAGE_NOMINAL_MV = 1100,
};

static enum codec_variant codec_ver;

static int dig_core_collapse_enable = 1;
module_param(dig_core_collapse_enable, int, 0664);
MODULE_PARM_DESC(dig_core_collapse_enable, "enable/disable power gating");

/* dig_core_collapse timer in seconds */
static int dig_core_collapse_timer = (TASHA_DIG_CORE_COLLAPSE_TIMER_MS/1000);
module_param(dig_core_collapse_timer, int, 0664);
MODULE_PARM_DESC(dig_core_collapse_timer, "timer for power gating");

/* SVS Scaling enable/disable */
static int svs_scaling_enabled = 1;
module_param(svs_scaling_enabled, int, 0664);
MODULE_PARM_DESC(svs_scaling_enabled, "enable/disable svs scaling");

/* SVS buck setting */
static int sido_buck_svs_voltage = SIDO_VOLTAGE_SVS_MV;
module_param(sido_buck_svs_voltage, int, 0664);
MODULE_PARM_DESC(sido_buck_svs_voltage,
			"setting for SVS voltage for SIDO BUCK");

#define TASHA_TX_UNMUTE_DELAY_MS	40

static int tx_unmute_delay = TASHA_TX_UNMUTE_DELAY_MS;
module_param(tx_unmute_delay, int, 0664);
MODULE_PARM_DESC(tx_unmute_delay, "delay to unmute the tx path");

static struct afe_param_slimbus_slave_port_cfg tasha_slimbus_slave_port_cfg = {
	.minor_version = 1,
	.slimbus_dev_id = AFE_SLIMBUS_DEVICE_1,
	.slave_dev_pgd_la = 0,
	.slave_dev_intfdev_la = 0,
	.bit_width = 16,
	.data_format = 0,
	.num_channels = 1
};

struct tasha_mbhc_zdet_param {
	u16 ldo_ctl;
	u16 noff;
	u16 nshift;
	u16 btn5;
	u16 btn6;
	u16 btn7;
};

static struct afe_param_cdc_reg_page_cfg tasha_cdc_reg_page_cfg = {
	.minor_version = AFE_API_VERSION_CDC_REG_PAGE_CFG,
	.enable = 1,
	.proc_id = AFE_CDC_REG_PAGE_ASSIGN_PROC_ID_1,
};

static struct afe_param_cdc_reg_cfg audio_reg_cfg[] = {
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_SOC_MAD_MAIN_CTL_1),
		HW_MAD_AUDIO_ENABLE, 0x1, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_SOC_MAD_AUDIO_CTL_3),
		HW_MAD_AUDIO_SLEEP_TIME, 0xF, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_SOC_MAD_AUDIO_CTL_4),
		HW_MAD_TX_AUDIO_SWITCH_OFF, 0x1, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_CFG),
		MAD_AUDIO_INT_DEST_SELECT_REG, 0x2, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_PIN2_MASK3),
		MAD_AUDIO_INT_MASK_REG, 0x1, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_PIN2_STATUS3),
		MAD_AUDIO_INT_STATUS_REG, 0x1, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_PIN2_CLEAR3),
		MAD_AUDIO_INT_CLEAR_REG, 0x1, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_CFG),
		VBAT_INT_DEST_SELECT_REG, 0x2, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_PIN2_MASK3),
		VBAT_INT_MASK_REG, 0x08, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_PIN2_STATUS3),
		VBAT_INT_STATUS_REG, 0x08, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_PIN2_CLEAR3),
		VBAT_INT_CLEAR_REG, 0x08, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_CFG),
		VBAT_RELEASE_INT_DEST_SELECT_REG, 0x2, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_PIN2_MASK3),
		VBAT_RELEASE_INT_MASK_REG, 0x10, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_PIN2_STATUS3),
		VBAT_RELEASE_INT_STATUS_REG, 0x10, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_INTR_PIN2_CLEAR3),
		VBAT_RELEASE_INT_CLEAR_REG, 0x10, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + TASHA_SB_PGD_PORT_TX_BASE),
		SB_PGD_PORT_TX_WATERMARK_N, 0x1E, WCD9335_REG_BITS, 0x1
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + TASHA_SB_PGD_PORT_TX_BASE),
		SB_PGD_PORT_TX_ENABLE_N, 0x1, WCD9335_REG_BITS, 0x1
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + TASHA_SB_PGD_PORT_RX_BASE),
		SB_PGD_PORT_RX_WATERMARK_N, 0x1E, WCD9335_REG_BITS, 0x1
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + TASHA_SB_PGD_PORT_RX_BASE),
		SB_PGD_PORT_RX_ENABLE_N, 0x1, WCD9335_REG_BITS, 0x1
	},
	{	1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_CDC_ANC0_IIR_ADAPT_CTL),
		AANC_FF_GAIN_ADAPTIVE, 0x4, WCD9335_REG_BITS, 0
	},
	{	1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_CDC_ANC0_IIR_ADAPT_CTL),
		AANC_FFGAIN_ADAPTIVE_EN, 0x8, WCD9335_REG_BITS, 0
	},
	{
		1,
		(TASHA_REGISTER_START_OFFSET + WCD9335_CDC_ANC0_FF_A_GAIN_CTL),
		AANC_GAIN_CONTROL, 0xFF, WCD9335_REG_BITS, 0
	},
};

static struct afe_param_cdc_reg_cfg_data tasha_audio_reg_cfg = {
	.num_registers = ARRAY_SIZE(audio_reg_cfg),
	.reg_data = audio_reg_cfg,
};

static struct afe_param_id_cdc_aanc_version tasha_cdc_aanc_version = {
	.cdc_aanc_minor_version = AFE_API_VERSION_CDC_AANC_VERSION,
	.aanc_hw_version        = AANC_HW_BLOCK_VERSION_2,
};

enum {
	VI_SENSE_1,
	VI_SENSE_2,
	AIF4_SWITCH_VALUE,
	AUDIO_NOMINAL,
	CPE_NOMINAL,
	HPH_PA_DELAY,
	ANC_MIC_AMIC1,
	ANC_MIC_AMIC2,
	ANC_MIC_AMIC3,
	ANC_MIC_AMIC4,
	ANC_MIC_AMIC5,
	ANC_MIC_AMIC6,
	CLASSH_CONFIG,
};

enum {
	AIF1_PB = 0,
	AIF1_CAP,
	AIF2_PB,
	AIF2_CAP,
	AIF3_PB,
	AIF3_CAP,
	AIF4_PB,
	AIF_MIX1_PB,
	AIF4_MAD_TX,
	AIF4_VIFEED,
	AIF5_CPE_TX,
	NUM_CODEC_DAIS,
};

enum {
	INTn_1_MIX_INP_SEL_ZERO = 0,
	INTn_1_MIX_INP_SEL_DEC0,
	INTn_1_MIX_INP_SEL_DEC1,
	INTn_1_MIX_INP_SEL_IIR0,
	INTn_1_MIX_INP_SEL_IIR1,
	INTn_1_MIX_INP_SEL_RX0,
	INTn_1_MIX_INP_SEL_RX1,
	INTn_1_MIX_INP_SEL_RX2,
	INTn_1_MIX_INP_SEL_RX3,
	INTn_1_MIX_INP_SEL_RX4,
	INTn_1_MIX_INP_SEL_RX5,
	INTn_1_MIX_INP_SEL_RX6,
	INTn_1_MIX_INP_SEL_RX7,

};

#define IS_VALID_NATIVE_FIFO_PORT(inp) \
	((inp >= INTn_1_MIX_INP_SEL_RX0) && \
	 (inp <= INTn_1_MIX_INP_SEL_RX3))

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
	INTERP_LO3,
	INTERP_LO4,
	INTERP_SPKR1,
	INTERP_SPKR2,
};

struct interp_sample_rate {
	int sample_rate;
	int rate_val;
};

static struct interp_sample_rate int_prim_sample_rate_val[] = {
	{8000, 0x0},	/* 8K */
	{16000, 0x1},	/* 16K */
	{24000, -EINVAL},/* 24K */
	{32000, 0x3},	/* 32K */
	{48000, 0x4},	/* 48K */
	{96000, 0x5},	/* 96K */
	{192000, 0x6},	/* 192K */
	{384000, 0x7},	/* 384K */
	{44100, 0x8}, /* 44.1K */
};

static struct interp_sample_rate int_mix_sample_rate_val[] = {
	{48000, 0x4},	/* 48K */
	{96000, 0x5},	/* 96K */
	{192000, 0x6},	/* 192K */
};

static const struct wcd9xxx_ch tasha_rx_chs[TASHA_RX_MAX] = {
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER, 0),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 1, 1),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 2, 2),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 3, 3),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 4, 4),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 5, 5),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 6, 6),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 7, 7),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 8, 8),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 9, 9),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 10, 10),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 11, 11),
	WCD9XXX_CH(TASHA_RX_PORT_START_NUMBER + 12, 12),
};

static const struct wcd9xxx_ch tasha_tx_chs[TASHA_TX_MAX] = {
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
	/* Needs to define in the same order of DAI enum definitions */
	0,
	BIT(AIF2_CAP) | BIT(AIF3_CAP) | BIT(AIF4_MAD_TX) | BIT(AIF5_CPE_TX),
	0,
	BIT(AIF1_CAP) | BIT(AIF3_CAP) | BIT(AIF4_MAD_TX) | BIT(AIF5_CPE_TX),
	0,
	BIT(AIF1_CAP) | BIT(AIF2_CAP) | BIT(AIF4_MAD_TX) | BIT(AIF5_CPE_TX),
	0,
	0,
	BIT(AIF1_CAP) | BIT(AIF2_CAP) | BIT(AIF3_CAP) | BIT(AIF5_CPE_TX),
	0,
	BIT(AIF1_CAP) | BIT(AIF2_CAP) | BIT(AIF3_CAP) | BIT(AIF4_MAD_TX),
};

static const u32 vport_i2s_check_table[NUM_CODEC_DAIS] = {
	0,			/* AIF1_PB */
	BIT(AIF2_CAP),		/* AIF1_CAP */
	0,			/* AIF2_PB */
	BIT(AIF1_CAP),		/* AIF2_CAP */
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
	COMPANDER_5, /* LO3_SE */
	COMPANDER_6, /* LO4_SE */
	COMPANDER_7, /* SWR SPK CH1 */
	COMPANDER_8, /* SWR SPK CH2 */
	COMPANDER_MAX,
};

enum {
	SRC_IN_HPHL,
	SRC_IN_LO1,
	SRC_IN_HPHR,
	SRC_IN_LO2,
	SRC_IN_SPKRL,
	SRC_IN_LO3,
	SRC_IN_SPKRR,
	SRC_IN_LO4,
};

enum {
	SPLINE_SRC0,
	SPLINE_SRC1,
	SPLINE_SRC2,
	SPLINE_SRC3,
	SPLINE_SRC_MAX,
};

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static const DECLARE_TLV_DB_SCALE(line_gain, 0, 7, 1);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);

static struct snd_soc_dai_driver tasha_dai[];
static int wcd9335_get_micb_vout_ctl_val(u32 micb_mv);

static int tasha_config_compander(struct snd_soc_codec *, int, int);
static void tasha_codec_set_tx_hold(struct snd_soc_codec *, u16, bool);
static int tasha_codec_internal_rco_ctrl(struct snd_soc_codec *codec,
				  bool enable);

/* Hold instance to soundwire platform device */
struct tasha_swr_ctrl_data {
	struct platform_device *swr_pdev;
	struct ida swr_ida;
};

struct wcd_swr_ctrl_platform_data {
	void *handle; /* holds codec private data */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*bulk_write)(void *handle, u32 *reg, u32 *val, size_t len);
	int (*clk)(void *handle, bool enable);
	int (*handle_irq)(void *handle,
			  irqreturn_t (*swrm_irq_handler)(int irq,
							  void *data),
			  void *swrm_handle,
			  int action);
};

static struct wcd_mbhc_register
	wcd_mbhc_registers[WCD_MBHC_REG_FUNC_MAX] = {
	WCD_MBHC_REGISTER("WCD_MBHC_L_DET_EN",
			  WCD9335_ANA_MBHC_MECH, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_DET_EN",
			  WCD9335_ANA_MBHC_MECH, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MECH_DETECTION_TYPE",
			  WCD9335_ANA_MBHC_MECH, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_CLAMP_CTL",
			  WCD9335_MBHC_PLUG_DETECT_CTL, 0x30, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_DETECTION_TYPE",
			  WCD9335_ANA_MBHC_ELECT, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_CTRL",
			  WCD9335_MBHC_PLUG_DETECT_CTL, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_COMP_CTRL",
			  WCD9335_ANA_MBHC_MECH, 0x04, 2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PLUG_TYPE",
			  WCD9335_ANA_MBHC_MECH, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_PLUG_TYPE",
			  WCD9335_ANA_MBHC_MECH, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SW_HPH_LP_100K_TO_GND",
			  WCD9335_ANA_MBHC_MECH, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_SCHMT_ISRC",
			  WCD9335_ANA_MBHC_ELECT, 0x06, 1, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_EN",
			  WCD9335_ANA_MBHC_ELECT, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_INSREM_DBNC",
			  WCD9335_MBHC_PLUG_DETECT_CTL, 0x0F, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_DBNC",
			  WCD9335_MBHC_CTL_1, 0x03, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_VREF",
			  WCD9335_MBHC_CTL_2, 0x03, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_COMP_RESULT",
			  WCD9335_ANA_MBHC_RESULT_3, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_IN2P_CLAMP_STATE",
			  WCD9335_ANA_MBHC_RESULT_3, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_SCHMT_RESULT",
			  WCD9335_ANA_MBHC_RESULT_3, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_SCHMT_RESULT",
			  WCD9335_ANA_MBHC_RESULT_3, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_SCHMT_RESULT",
			  WCD9335_ANA_MBHC_RESULT_3, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_OCP_FSM_EN",
			  WCD9335_HPH_OCP_CTL, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_RESULT",
			  WCD9335_ANA_MBHC_RESULT_3, 0x07, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_ISRC_CTL",
			  WCD9335_ANA_MBHC_ELECT, 0x70, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_RESULT",
			  WCD9335_ANA_MBHC_RESULT_3, 0xFF, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MICB_CTRL",
			  WCD9335_ANA_MICB2, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_CNP_WG_TIME",
			  WCD9335_HPH_CNP_WG_TIME, 0xFF, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_PA_EN",
			  WCD9335_ANA_HPH, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PA_EN",
			  WCD9335_ANA_HPH, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_PA_EN",
			  WCD9335_ANA_HPH, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SWCH_LEVEL_REMOVE",
			  WCD9335_ANA_MBHC_RESULT_3, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_PULLDOWN_CTRL",
			  0, 0, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ANC_DET_EN",
			  WCD9335_ANA_MBHC_ZDET, 0x01, 0, 0),
	/*
	 * MBHC FSM status register is only available in Tasha 2.0.
	 * So, init with 0 later once the version is known, then values
	 * will be updated.
	 */
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_STATUS",
			  0, 0, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MUX_CTL",
			  WCD9335_MBHC_CTL_2, 0x70, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MOISTURE_STATUS",
			  WCD9335_MBHC_FSM_STATUS, 0X20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_GND",
			  WCD9335_HPH_PA_CTL2, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_GND",
			  WCD9335_HPH_PA_CTL2, 0x10, 4, 0),
};

static const struct wcd_mbhc_intr intr_ids = {
	.mbhc_sw_intr =  WCD9335_IRQ_MBHC_SW_DET,
	.mbhc_btn_press_intr = WCD9335_IRQ_MBHC_BUTTON_PRESS_DET,
	.mbhc_btn_release_intr = WCD9335_IRQ_MBHC_BUTTON_RELEASE_DET,
	.mbhc_hs_ins_intr = WCD9335_IRQ_MBHC_ELECT_INS_REM_LEG_DET,
	.mbhc_hs_rem_intr = WCD9335_IRQ_MBHC_ELECT_INS_REM_DET,
	.hph_left_ocp = WCD9335_IRQ_HPH_PA_OCPL_FAULT,
	.hph_right_ocp = WCD9335_IRQ_HPH_PA_OCPR_FAULT,
};

struct wcd_vbat {
	bool is_enabled;
	bool adc_config;
	/* Variables to cache Vbat ADC output values */
	u16 dcp1;
	u16 dcp2;
};

struct hpf_work {
	struct tasha_priv *tasha;
	u8 decimator;
	u8 hpf_cut_off_freq;
	struct delayed_work dwork;
};

#define WCD9335_SPK_ANC_EN_DELAY_MS 350
static int spk_anc_en_delay = WCD9335_SPK_ANC_EN_DELAY_MS;
module_param(spk_anc_en_delay, int, 0664);
MODULE_PARM_DESC(spk_anc_en_delay, "delay to enable anc in speaker path");

struct spk_anc_work {
	struct tasha_priv *tasha;
	struct delayed_work dwork;
};

struct tx_mute_work {
	struct tasha_priv *tasha;
	u8 decimator;
	struct delayed_work dwork;
};

struct tasha_priv {
	struct device *dev;
	struct wcd9xxx *wcd9xxx;

	struct snd_soc_codec *codec;
	u32 adc_count;
	u32 rx_bias_count;
	s32 dmic_0_1_clk_cnt;
	s32 dmic_2_3_clk_cnt;
	s32 dmic_4_5_clk_cnt;
	s32 ldo_h_users;
	s32 micb_ref[TASHA_MAX_MICBIAS];
	s32 pullup_ref[TASHA_MAX_MICBIAS];

	u32 anc_slot;
	bool anc_func;
	bool is_wsa_attach;

	/* Vbat module */
	struct wcd_vbat vbat;

	/* cal info for codec */
	struct fw_info *fw_data;

	/*track tasha interface type*/
	u8 intf_type;

	/* num of slim ports required */
	struct wcd9xxx_codec_dai_data  dai[NUM_CODEC_DAIS];

	/* SoundWire data structure */
	struct tasha_swr_ctrl_data *swr_ctrl_data;
	int nr;

	/*compander*/
	int comp_enabled[COMPANDER_MAX];

	/* Maintain the status of AUX PGA */
	int aux_pga_cnt;
	u8 aux_l_gain;
	u8 aux_r_gain;

	bool spkr_pa_widget_on;
	struct regulator *spkdrv_reg;
	struct regulator *spkdrv2_reg;

	bool mbhc_started;
	/* class h specific data */
	struct wcd_clsh_cdc_data clsh_d;

	struct afe_param_cdc_slimbus_slave_cfg slimbus_slave_cfg;

	/*
	 * list used to save/restore registers at start and
	 * end of impedance measurement
	 */
	struct list_head reg_save_restore;

	/* handle to cpe core */
	struct wcd_cpe_core *cpe_core;
	u32 current_cpe_clk_freq;
	enum tasha_sido_voltage sido_voltage;
	int sido_ccl_cnt;

	u32 ana_rx_supplies;
	/* Multiplication factor used for impedance detection */
	int zdet_gain_mul_fact;

	/* to track the status */
	unsigned long status_mask;

	struct work_struct tasha_add_child_devices_work;
	struct wcd_swr_ctrl_platform_data swr_plat_data;

	/* Port values for Rx and Tx codec_dai */
	unsigned int rx_port_value[TASHA_RX_MAX];
	unsigned int tx_port_value;

	unsigned int vi_feed_value;
	/* Tasha Interpolator Mode Select for EAR, HPH_L and HPH_R */
	u32 hph_mode;

	u16 prim_int_users[TASHA_NUM_INTERPOLATORS];
	int spl_src_users[SPLINE_SRC_MAX];

	struct wcd9xxx_resmgr_v2 *resmgr;
	struct delayed_work power_gate_work;
	struct mutex power_lock;
	struct mutex sido_lock;

	/* mbhc module */
	struct wcd_mbhc mbhc;
	struct blocking_notifier_head notifier;
	struct mutex micb_lock;

	struct clk *wcd_ext_clk;
	struct clk *wcd_native_clk;
	struct mutex swr_read_lock;
	struct mutex swr_write_lock;
	struct mutex swr_clk_lock;
	int swr_clk_users;
	int native_clk_users;
	int (*zdet_gpio_cb)(struct snd_soc_codec *codec, bool high);

	struct snd_info_entry *entry;
	struct snd_info_entry *version_entry;
	int power_active_ref;

	struct on_demand_supply on_demand_list[ON_DEMAND_SUPPLIES_MAX];

	int (*machine_codec_event_cb)(struct snd_soc_codec *codec,
				      enum wcd9335_codec_event);
	int spkr_gain_offset;
	int spkr_mode;
	int ear_spkr_gain;
	struct hpf_work tx_hpf_work[TASHA_NUM_DECIMATORS];
	struct tx_mute_work tx_mute_dwork[TASHA_NUM_DECIMATORS];
	struct spk_anc_work spk_anc_dwork;
	struct mutex codec_mutex;
	int hph_l_gain;
	int hph_r_gain;
	int rx_7_count;
	int rx_8_count;
	bool clk_mode;
	bool clk_internal;
	/* Lock to prevent multiple functions voting at same time */
	struct mutex sb_clk_gear_lock;
	/* Count for functions voting or un-voting */
	u32 ref_count;
	/* Lock to protect mclk enablement */
	struct mutex mclk_lock;

	struct platform_device *pdev_child_devices
			[WCD9335_CHILD_DEVICES_MAX];
	int child_count;
};

static int tasha_codec_vote_max_bw(struct snd_soc_codec *codec,
				   bool vote);

static const struct tasha_reg_mask_val tasha_spkr_default[] = {
	{WCD9335_CDC_COMPANDER7_CTL3, 0x80, 0x80},
	{WCD9335_CDC_COMPANDER8_CTL3, 0x80, 0x80},
	{WCD9335_CDC_COMPANDER7_CTL7, 0x01, 0x01},
	{WCD9335_CDC_COMPANDER8_CTL7, 0x01, 0x01},
	{WCD9335_CDC_BOOST0_BOOST_CTL, 0x7C, 0x58},
	{WCD9335_CDC_BOOST1_BOOST_CTL, 0x7C, 0x58},
};

static const struct tasha_reg_mask_val tasha_spkr_mode1[] = {
	{WCD9335_CDC_COMPANDER7_CTL3, 0x80, 0x00},
	{WCD9335_CDC_COMPANDER8_CTL3, 0x80, 0x00},
	{WCD9335_CDC_COMPANDER7_CTL7, 0x01, 0x00},
	{WCD9335_CDC_COMPANDER8_CTL7, 0x01, 0x00},
	{WCD9335_CDC_BOOST0_BOOST_CTL, 0x7C, 0x44},
	{WCD9335_CDC_BOOST1_BOOST_CTL, 0x7C, 0x44},
};

/**
 * tasha_set_spkr_gain_offset - offset the speaker path
 * gain with the given offset value.
 *
 * @codec: codec instance
 * @offset: Indicates speaker path gain offset value.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int tasha_set_spkr_gain_offset(struct snd_soc_codec *codec, int offset)
{
	struct tasha_priv *priv = snd_soc_codec_get_drvdata(codec);

	if (!priv)
		return -EINVAL;

	priv->spkr_gain_offset = offset;
	return 0;
}
EXPORT_SYMBOL(tasha_set_spkr_gain_offset);

/**
 * tasha_set_spkr_mode - Configures speaker compander and smartboost
 * settings based on speaker mode.
 *
 * @codec: codec instance
 * @mode: Indicates speaker configuration mode.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int tasha_set_spkr_mode(struct snd_soc_codec *codec, int mode)
{
	struct tasha_priv *priv = snd_soc_codec_get_drvdata(codec);
	int i;
	const struct tasha_reg_mask_val *regs;
	int size;

	if (!priv)
		return -EINVAL;

	switch (mode) {
	case SPKR_MODE_1:
		regs = tasha_spkr_mode1;
		size = ARRAY_SIZE(tasha_spkr_mode1);
		break;
	default:
		regs = tasha_spkr_default;
		size = ARRAY_SIZE(tasha_spkr_default);
		break;
	}

	priv->spkr_mode = mode;
	for (i = 0; i < size; i++)
		snd_soc_update_bits(codec, regs[i].reg,
				    regs[i].mask, regs[i].val);
	return 0;
}
EXPORT_SYMBOL(tasha_set_spkr_mode);

static void tasha_enable_sido_buck(struct snd_soc_codec *codec)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, WCD9335_ANA_RCO, 0x80, 0x80);
	snd_soc_update_bits(codec, WCD9335_ANA_BUCK_CTL, 0x02, 0x02);
	/* 100us sleep needed after IREF settings */
	usleep_range(100, 110);
	snd_soc_update_bits(codec, WCD9335_ANA_BUCK_CTL, 0x04, 0x04);
	/* 100us sleep needed after VREF settings */
	usleep_range(100, 110);
	tasha->resmgr->sido_input_src = SIDO_SOURCE_RCO_BG;
}

static void tasha_cdc_sido_ccl_enable(struct tasha_priv *tasha, bool ccl_flag)
{
	struct snd_soc_codec *codec = tasha->codec;

	if (!codec)
		return;

	if (!TASHA_IS_2_0(tasha->wcd9xxx)) {
		dev_dbg(codec->dev, "%s: tasha version < 2p0, return\n",
			__func__);
		return;
	}
	dev_dbg(codec->dev, "%s: sido_ccl_cnt=%d, ccl_flag:%d\n",
			__func__, tasha->sido_ccl_cnt, ccl_flag);
	if (ccl_flag) {
		if (++tasha->sido_ccl_cnt == 1)
			snd_soc_update_bits(codec,
				WCD9335_SIDO_SIDO_CCL_10, 0xFF, 0x6E);
	} else {
		if (tasha->sido_ccl_cnt == 0) {
			dev_dbg(codec->dev, "%s: sido_ccl already disabled\n",
				__func__);
			return;
		}
		if (--tasha->sido_ccl_cnt == 0)
			snd_soc_update_bits(codec,
				WCD9335_SIDO_SIDO_CCL_10, 0xFF, 0x02);
	}
}

static bool tasha_cdc_is_svs_enabled(struct tasha_priv *tasha)
{
	if (TASHA_IS_2_0(tasha->wcd9xxx) &&
		svs_scaling_enabled)
		return true;

	return false;
}

static int tasha_cdc_req_mclk_enable(struct tasha_priv *tasha,
				     bool enable)
{
	int ret = 0;

	mutex_lock(&tasha->mclk_lock);
	if (enable) {
		tasha_cdc_sido_ccl_enable(tasha, true);
		ret = clk_prepare_enable(tasha->wcd_ext_clk);
		if (ret) {
			dev_err(tasha->dev, "%s: ext clk enable failed\n",
				__func__);
			goto unlock_mutex;
		}
		/* get BG */
		wcd_resmgr_enable_master_bias(tasha->resmgr);
		/* get MCLK */
		wcd_resmgr_enable_clk_block(tasha->resmgr, WCD_CLK_MCLK);
	} else {
		/* put MCLK */
		wcd_resmgr_disable_clk_block(tasha->resmgr, WCD_CLK_MCLK);
		/* put BG */
		wcd_resmgr_disable_master_bias(tasha->resmgr);
		clk_disable_unprepare(tasha->wcd_ext_clk);
		tasha_cdc_sido_ccl_enable(tasha, false);
	}
unlock_mutex:
	mutex_unlock(&tasha->mclk_lock);
	return ret;
}

static int tasha_cdc_check_sido_value(enum tasha_sido_voltage req_mv)
{
	if ((req_mv != SIDO_VOLTAGE_SVS_MV) &&
		(req_mv != SIDO_VOLTAGE_NOMINAL_MV))
		return -EINVAL;

	return 0;
}

static void tasha_codec_apply_sido_voltage(
				struct tasha_priv *tasha,
				enum tasha_sido_voltage req_mv)
{
	u32 vout_d_val;
	struct snd_soc_codec *codec = tasha->codec;
	int ret;

	if (!codec)
		return;

	if (!tasha_cdc_is_svs_enabled(tasha))
		return;

	if ((sido_buck_svs_voltage != SIDO_VOLTAGE_SVS_MV) &&
		(sido_buck_svs_voltage != SIDO_VOLTAGE_NOMINAL_MV))
		sido_buck_svs_voltage = SIDO_VOLTAGE_SVS_MV;

	ret = tasha_cdc_check_sido_value(req_mv);
	if (ret < 0) {
		dev_dbg(codec->dev, "%s: requested mv=%d not in range\n",
			__func__, req_mv);
		return;
	}
	if (req_mv == tasha->sido_voltage) {
		dev_dbg(codec->dev, "%s: Already at requested mv=%d\n",
			__func__, req_mv);
		return;
	}
	if (req_mv == sido_buck_svs_voltage) {
		if (test_bit(AUDIO_NOMINAL, &tasha->status_mask) ||
			test_bit(CPE_NOMINAL, &tasha->status_mask)) {
			dev_dbg(codec->dev,
				"%s: nominal client running, status_mask=%lu\n",
				__func__, tasha->status_mask);
			return;
		}
	}
	/* compute the vout_d step value */
	vout_d_val = CALCULATE_VOUT_D(req_mv);
	snd_soc_write(codec, WCD9335_ANA_BUCK_VOUT_D, vout_d_val & 0xFF);
	snd_soc_update_bits(codec, WCD9335_ANA_BUCK_CTL, 0x80, 0x80);

	/* 1 msec sleep required after SIDO Vout_D voltage change */
	usleep_range(1000, 1100);
	tasha->sido_voltage = req_mv;
	dev_dbg(codec->dev,
		"%s: updated SIDO buck Vout_D to %d, vout_d step = %u\n",
		__func__, tasha->sido_voltage, vout_d_val);

	snd_soc_update_bits(codec, WCD9335_ANA_BUCK_CTL,
				0x80, 0x00);
}

static int tasha_codec_update_sido_voltage(
				struct tasha_priv *tasha,
				enum tasha_sido_voltage req_mv)
{
	int ret = 0;

	if (!tasha_cdc_is_svs_enabled(tasha))
		return ret;

	mutex_lock(&tasha->sido_lock);
	/* enable mclk before setting SIDO voltage */
	ret = tasha_cdc_req_mclk_enable(tasha, true);
	if (ret) {
		dev_err(tasha->dev, "%s: ext clk enable failed\n",
			__func__);
		goto err;
	}
	tasha_codec_apply_sido_voltage(tasha, req_mv);
	tasha_cdc_req_mclk_enable(tasha, false);

err:
	mutex_unlock(&tasha->sido_lock);
	return ret;
}

int tasha_enable_efuse_sensing(struct snd_soc_codec *codec)
{
	struct tasha_priv *priv = snd_soc_codec_get_drvdata(codec);

	tasha_cdc_mclk_enable(codec, true, false);

	if (!TASHA_IS_2_0(priv->wcd9xxx))
		snd_soc_update_bits(codec, WCD9335_CHIP_TIER_CTRL_EFUSE_CTL,
				    0x1E, 0x02);
	snd_soc_update_bits(codec, WCD9335_CHIP_TIER_CTRL_EFUSE_CTL,
			    0x01, 0x01);
	/*
	 * 5ms sleep required after enabling efuse control
	 * before checking the status.
	 */
	usleep_range(5000, 5500);
	if (!(snd_soc_read(codec, WCD9335_CHIP_TIER_CTRL_EFUSE_STATUS) & 0x01))
		WARN(1, "%s: Efuse sense is not complete\n", __func__);

	if (TASHA_IS_2_0(priv->wcd9xxx)) {
		if (!(snd_soc_read(codec,
			WCD9335_CHIP_TIER_CTRL_EFUSE_VAL_OUT0) & 0x40))
			snd_soc_update_bits(codec, WCD9335_HPH_R_ATEST,
					    0x04, 0x00);
		tasha_enable_sido_buck(codec);
	}

	tasha_cdc_mclk_enable(codec, false, false);

	return 0;
}
EXPORT_SYMBOL(tasha_enable_efuse_sensing);

void *tasha_get_afe_config(struct snd_soc_codec *codec,
			   enum afe_config_type config_type)
{
	struct tasha_priv *priv = snd_soc_codec_get_drvdata(codec);

	switch (config_type) {
	case AFE_SLIMBUS_SLAVE_CONFIG:
		return &priv->slimbus_slave_cfg;
	case AFE_CDC_REGISTERS_CONFIG:
		return &tasha_audio_reg_cfg;
	case AFE_SLIMBUS_SLAVE_PORT_CONFIG:
		return &tasha_slimbus_slave_port_cfg;
	case AFE_AANC_VERSION:
		return &tasha_cdc_aanc_version;
	case AFE_CLIP_BANK_SEL:
		return NULL;
	case AFE_CDC_CLIP_REGISTERS_CONFIG:
		return NULL;
	case AFE_CDC_REGISTER_PAGE_CONFIG:
		return &tasha_cdc_reg_page_cfg;
	default:
		dev_err(codec->dev, "%s: Unknown config_type 0x%x\n",
			__func__, config_type);
		return NULL;
	}
}
EXPORT_SYMBOL(tasha_get_afe_config);

/*
 * tasha_event_register: Registers a machine driver callback
 * function with codec private data for post ADSP sub-system
 * restart (SSR). This callback function will be called from
 * codec driver once codec comes out of reset after ADSP SSR.
 *
 * @machine_event_cb: callback function from machine driver
 * @codec: Codec instance
 *
 * Return: none
 */
void tasha_event_register(
	int (*machine_event_cb)(struct snd_soc_codec *codec,
				enum wcd9335_codec_event),
	struct snd_soc_codec *codec)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	if (tasha)
		tasha->machine_codec_event_cb = machine_event_cb;
	else
		dev_dbg(codec->dev, "%s: Invalid tasha_priv data\n", __func__);
}
EXPORT_SYMBOL(tasha_event_register);

static int tasha_mbhc_request_irq(struct snd_soc_codec *codec,
				   int irq, irq_handler_t handler,
				   const char *name, void *data)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *wcd9xxx = tasha->wcd9xxx;
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;

	return wcd9xxx_request_irq(core_res, irq, handler, name, data);
}

static void tasha_mbhc_irq_control(struct snd_soc_codec *codec,
				   int irq, bool enable)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *wcd9xxx = tasha->wcd9xxx;
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;
	if (enable)
		wcd9xxx_enable_irq(core_res, irq);
	else
		wcd9xxx_disable_irq(core_res, irq);
}

static int tasha_mbhc_free_irq(struct snd_soc_codec *codec,
			       int irq, void *data)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *wcd9xxx = tasha->wcd9xxx;
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;

	wcd9xxx_free_irq(core_res, irq, data);
	return 0;
}

static void tasha_mbhc_clk_setup(struct snd_soc_codec *codec,
				 bool enable)
{
	if (enable)
		snd_soc_update_bits(codec, WCD9335_MBHC_CTL_1,
				    0x80, 0x80);
	else
		snd_soc_update_bits(codec, WCD9335_MBHC_CTL_1,
				    0x80, 0x00);
}

static int tasha_mbhc_btn_to_num(struct snd_soc_codec *codec)
{
	return snd_soc_read(codec, WCD9335_ANA_MBHC_RESULT_3) & 0x7;
}

static void tasha_mbhc_mbhc_bias_control(struct snd_soc_codec *codec,
					 bool enable)
{
	if (enable)
		snd_soc_update_bits(codec, WCD9335_ANA_MBHC_ELECT,
				    0x01, 0x01);
	else
		snd_soc_update_bits(codec, WCD9335_ANA_MBHC_ELECT,
				    0x01, 0x00);
}

static void tasha_mbhc_program_btn_thr(struct snd_soc_codec *codec,
				       s16 *btn_low, s16 *btn_high,
				       int num_btn, bool is_micbias)
{
	int i;
	int vth;

	if (num_btn > WCD_MBHC_DEF_BUTTONS) {
		dev_err(codec->dev, "%s: invalid number of buttons: %d\n",
			__func__, num_btn);
		return;
	}
	/*
	 * Tasha just needs one set of thresholds for button detection
	 * due to micbias voltage ramp to pullup upon button press. So
	 * btn_low and is_micbias are ignored and always program button
	 * thresholds using btn_high.
	 */
	for (i = 0; i < num_btn; i++) {
		vth = ((btn_high[i] * 2) / 25) & 0x3F;
		snd_soc_update_bits(codec, WCD9335_ANA_MBHC_BTN0 + i,
				    0xFC, vth << 2);
		dev_dbg(codec->dev, "%s: btn_high[%d]: %d, vth: %d\n",
			__func__, i, btn_high[i], vth);
	}
}

static bool tasha_mbhc_lock_sleep(struct wcd_mbhc *mbhc, bool lock)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *wcd9xxx = tasha->wcd9xxx;
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;
	if (lock)
		return wcd9xxx_lock_sleep(core_res);
	else {
		wcd9xxx_unlock_sleep(core_res);
		return 0;
	}
}

static int tasha_mbhc_register_notifier(struct wcd_mbhc *mbhc,
					struct notifier_block *nblock,
					bool enable)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	if (enable)
		return blocking_notifier_chain_register(&tasha->notifier,
							nblock);
	else
		return blocking_notifier_chain_unregister(&tasha->notifier,
							  nblock);
}

static bool tasha_mbhc_micb_en_status(struct wcd_mbhc *mbhc, int micb_num)
{
	u8 val;

	if (micb_num == MIC_BIAS_2) {
		val = (snd_soc_read(mbhc->codec, WCD9335_ANA_MICB2) >> 6);
		if (val == 0x01)
			return true;
	}
	return false;
}

static bool tasha_mbhc_hph_pa_on_status(struct snd_soc_codec *codec)
{
	return (snd_soc_read(codec, WCD9335_ANA_HPH) & 0xC0) ? true : false;
}

static void tasha_mbhc_hph_l_pull_up_control(struct snd_soc_codec *codec,
					enum mbhc_hs_pullup_iref pull_up_cur)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	if (!tasha)
		return;

	/* Default pull up current to 2uA */
	if (pull_up_cur < I_OFF || pull_up_cur > I_3P0_UA ||
	    pull_up_cur == I_DEFAULT)
		pull_up_cur = I_2P0_UA;

	dev_dbg(codec->dev, "%s: HS pull up current:%d\n",
		__func__, pull_up_cur);

	if (TASHA_IS_2_0(tasha->wcd9xxx))
		snd_soc_update_bits(codec, WCD9335_MBHC_PLUG_DETECT_CTL,
			    0xC0, pull_up_cur << 6);
	else
		snd_soc_update_bits(codec, WCD9335_MBHC_PLUG_DETECT_CTL,
			    0xC0, 0x40);
}

static int tasha_enable_ext_mb_source(struct wcd_mbhc *mbhc,
		bool turn_on)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	struct on_demand_supply *supply;

	if (!tasha)
		return -EINVAL;

	supply =  &tasha->on_demand_list[ON_DEMAND_MICBIAS];
	if (!supply->supply) {
		dev_dbg(codec->dev, "%s: warning supply not present ond for %s\n",
				__func__, "onDemand Micbias");
		return ret;
	}

	dev_dbg(codec->dev, "%s turn_on: %d count: %d\n", __func__, turn_on,
		supply->ondemand_supply_count);

	if (turn_on) {
		if (!(supply->ondemand_supply_count)) {
			ret = snd_soc_dapm_force_enable_pin(
				snd_soc_codec_get_dapm(codec),
				"MICBIAS_REGULATOR");
			snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));
		}
		supply->ondemand_supply_count++;
	} else {
		if (supply->ondemand_supply_count > 0)
			supply->ondemand_supply_count--;
		if (!(supply->ondemand_supply_count)) {
			ret = snd_soc_dapm_disable_pin(
				snd_soc_codec_get_dapm(codec),
				"MICBIAS_REGULATOR");
		snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));
		}
	}

	if (ret)
		dev_err(codec->dev, "%s: Failed to %s external micbias source\n",
			__func__, turn_on ? "enable" : "disabled");
	else
		dev_dbg(codec->dev, "%s: %s external micbias source\n",
			__func__, turn_on ? "Enabled" : "Disabled");

	return ret;
}

static int tasha_micbias_control(struct snd_soc_codec *codec,
				 int micb_num,
				 int req, bool is_dapm)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int micb_index = micb_num - 1;
	u16 micb_reg;
	int pre_off_event = 0, post_off_event = 0;
	int post_on_event = 0, post_dapm_off = 0;
	int post_dapm_on = 0;

	if ((micb_index < 0) || (micb_index > TASHA_MAX_MICBIAS - 1)) {
		dev_err(codec->dev, "%s: Invalid micbias index, micb_ind:%d\n",
			__func__, micb_index);
		return -EINVAL;
	}
	switch (micb_num) {
	case MIC_BIAS_1:
		micb_reg = WCD9335_ANA_MICB1;
		break;
	case MIC_BIAS_2:
		micb_reg = WCD9335_ANA_MICB2;
		pre_off_event = WCD_EVENT_PRE_MICBIAS_2_OFF;
		post_off_event = WCD_EVENT_POST_MICBIAS_2_OFF;
		post_on_event = WCD_EVENT_POST_MICBIAS_2_ON;
		post_dapm_on = WCD_EVENT_POST_DAPM_MICBIAS_2_ON;
		post_dapm_off = WCD_EVENT_POST_DAPM_MICBIAS_2_OFF;
		break;
	case MIC_BIAS_3:
		micb_reg = WCD9335_ANA_MICB3;
		break;
	case MIC_BIAS_4:
		micb_reg = WCD9335_ANA_MICB4;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid micbias number: %d\n",
			__func__, micb_num);
		return -EINVAL;
	}
	mutex_lock(&tasha->micb_lock);

	switch (req) {
	case MICB_PULLUP_ENABLE:
		tasha->pullup_ref[micb_index]++;
		if ((tasha->pullup_ref[micb_index] == 1) &&
		    (tasha->micb_ref[micb_index] == 0))
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x80);
		break;
	case MICB_PULLUP_DISABLE:
		if (tasha->pullup_ref[micb_index] > 0)
			tasha->pullup_ref[micb_index]--;
		if ((tasha->pullup_ref[micb_index] == 0) &&
		    (tasha->micb_ref[micb_index] == 0))
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x00);
		break;
	case MICB_ENABLE:
		tasha->micb_ref[micb_index]++;
		if (tasha->micb_ref[micb_index] == 1) {
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x40);
			if (post_on_event)
				blocking_notifier_call_chain(&tasha->notifier,
						post_on_event, &tasha->mbhc);
		}
		if (is_dapm && post_dapm_on)
			blocking_notifier_call_chain(&tasha->notifier,
					post_dapm_on, &tasha->mbhc);
		break;
	case MICB_DISABLE:
		if (tasha->micb_ref[micb_index] > 0)
			tasha->micb_ref[micb_index]--;
		if ((tasha->micb_ref[micb_index] == 0) &&
		    (tasha->pullup_ref[micb_index] > 0))
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x80);
		else if ((tasha->micb_ref[micb_index] == 0) &&
			 (tasha->pullup_ref[micb_index] == 0)) {
			if (pre_off_event)
				blocking_notifier_call_chain(&tasha->notifier,
						pre_off_event, &tasha->mbhc);
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x00);
			if (post_off_event)
				blocking_notifier_call_chain(&tasha->notifier,
						post_off_event, &tasha->mbhc);
		}
		if (is_dapm && post_dapm_off)
			blocking_notifier_call_chain(&tasha->notifier,
					post_dapm_off, &tasha->mbhc);
		break;
	};

	dev_dbg(codec->dev, "%s: micb_num:%d, micb_ref: %d, pullup_ref: %d\n",
		__func__, micb_num, tasha->micb_ref[micb_index],
		tasha->pullup_ref[micb_index]);

	mutex_unlock(&tasha->micb_lock);

	return 0;
}

static int tasha_mbhc_request_micbias(struct snd_soc_codec *codec,
				      int micb_num, int req)
{
	int ret;

	/*
	 * If micbias is requested, make sure that there
	 * is vote to enable mclk
	 */
	if (req == MICB_ENABLE)
		tasha_cdc_mclk_enable(codec, true, false);

	ret = tasha_micbias_control(codec, micb_num, req, false);

	/*
	 * Release vote for mclk while requesting for
	 * micbias disable
	 */
	if (req == MICB_DISABLE)
		tasha_cdc_mclk_enable(codec, false, false);

	return ret;
}

static void tasha_mbhc_micb_ramp_control(struct snd_soc_codec *codec,
					bool enable)
{
	if (enable) {
		snd_soc_update_bits(codec, WCD9335_ANA_MICB2_RAMP,
				    0x1C, 0x0C);
		snd_soc_update_bits(codec, WCD9335_ANA_MICB2_RAMP,
				    0x80, 0x80);
	} else {
		snd_soc_update_bits(codec, WCD9335_ANA_MICB2_RAMP,
				    0x80, 0x00);
		snd_soc_update_bits(codec, WCD9335_ANA_MICB2_RAMP,
				    0x1C, 0x00);
	}
}

static struct firmware_cal *tasha_get_hwdep_fw_cal(struct wcd_mbhc *mbhc,
						   enum wcd_cal_type type)
{
	struct tasha_priv *tasha;
	struct firmware_cal *hwdep_cal;
	struct snd_soc_codec *codec = mbhc->codec;

	if (!codec) {
		pr_err("%s: NULL codec pointer\n", __func__);
		return NULL;
	}
	tasha = snd_soc_codec_get_drvdata(codec);
	hwdep_cal = wcdcal_get_fw_cal(tasha->fw_data, type);
	if (!hwdep_cal)
		dev_err(codec->dev, "%s: cal not sent by %d\n",
			__func__, type);

	return hwdep_cal;
}

static int tasha_mbhc_micb_adjust_voltage(struct snd_soc_codec *codec,
					  int req_volt,
					  int micb_num)
{
	int cur_vout_ctl, req_vout_ctl;
	int micb_reg, micb_val, micb_en;

	switch (micb_num) {
	case MIC_BIAS_1:
		micb_reg = WCD9335_ANA_MICB1;
		break;
	case MIC_BIAS_2:
		micb_reg = WCD9335_ANA_MICB2;
		break;
	case MIC_BIAS_3:
		micb_reg = WCD9335_ANA_MICB3;
		break;
	case MIC_BIAS_4:
		micb_reg = WCD9335_ANA_MICB4;
		break;
	default:
		return -EINVAL;
	}

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

	req_vout_ctl = wcd9335_get_micb_vout_ctl_val(req_volt);
	if (req_vout_ctl < 0)
		return -EINVAL;
	if (cur_vout_ctl == req_vout_ctl)
		return 0;

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

	return 0;
}

static int tasha_mbhc_micb_ctrl_threshold_mic(struct snd_soc_codec *codec,
					      int micb_num, bool req_en)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_pdata *pdata = dev_get_platdata(codec->dev->parent);
	int rc, micb_mv;

	if (micb_num != MIC_BIAS_2)
		return -EINVAL;

	/*
	 * If device tree micbias level is already above the minimum
	 * voltage needed to detect threshold microphone, then do
	 * not change the micbias, just return.
	 */
	if (pdata->micbias.micb2_mv >= WCD_MBHC_THR_HS_MICB_MV)
		return 0;

	micb_mv = req_en ? WCD_MBHC_THR_HS_MICB_MV : pdata->micbias.micb2_mv;

	mutex_lock(&tasha->micb_lock);
	rc = tasha_mbhc_micb_adjust_voltage(codec, micb_mv, MIC_BIAS_2);
	mutex_unlock(&tasha->micb_lock);

	return rc;
}

static inline void tasha_mbhc_get_result_params(struct wcd9xxx *wcd9xxx,
						s16 *d1_a, u16 noff,
						int32_t *zdet)
{
	int i;
	int val, val1;
	s16 c1;
	s32 x1, d1;
	int32_t denom;
	int minCode_param[] = {
			3277, 1639, 820, 410, 205, 103, 52, 26
	};

	regmap_update_bits(wcd9xxx->regmap, WCD9335_ANA_MBHC_ZDET, 0x20, 0x20);
	for (i = 0; i < TASHA_ZDET_NUM_MEASUREMENTS; i++) {
		regmap_read(wcd9xxx->regmap, WCD9335_ANA_MBHC_RESULT_2, &val);
		if (val & 0x80)
			break;
	}
	val = val << 0x8;
	regmap_read(wcd9xxx->regmap, WCD9335_ANA_MBHC_RESULT_1, &val1);
	val |= val1;
	regmap_update_bits(wcd9xxx->regmap, WCD9335_ANA_MBHC_ZDET, 0x20, 0x00);
	x1 = TASHA_MBHC_GET_X1(val);
	c1 = TASHA_MBHC_GET_C1(val);
	/* If ramp is not complete, give additional 5ms */
	if ((c1 < 2) && x1)
		usleep_range(5000, 5050);

	if (!c1 || !x1) {
		dev_dbg(wcd9xxx->dev,
			"%s: Impedance detect ramp error, c1=%d, x1=0x%x\n",
			__func__, c1, x1);
		goto ramp_down;
	}
	d1 = d1_a[c1];
	denom = (x1 * d1) - (1 << (14 - noff));
	if (denom > 0)
		*zdet = (TASHA_MBHC_ZDET_CONST * 1000) / denom;
	else if (x1 < minCode_param[noff])
		*zdet = TASHA_ZDET_FLOATING_IMPEDANCE;

	dev_dbg(wcd9xxx->dev, "%s: d1=%d, c1=%d, x1=0x%x, z_val=%d(milliOhm)\n",
		__func__, d1, c1, x1, *zdet);
ramp_down:
	i = 0;
	while (x1) {
		regmap_bulk_read(wcd9xxx->regmap,
				 WCD9335_ANA_MBHC_RESULT_1, (u8 *)&val, 2);
		x1 = TASHA_MBHC_GET_X1(val);
		i++;
		if (i == TASHA_ZDET_NUM_MEASUREMENTS)
			break;
	}
}

/*
 * tasha_mbhc_zdet_gpio_ctrl: Register callback function for
 * controlling the switch on hifi amps. Default switch state
 * will put a 51ohm load in parallel to the hph load. So,
 * impedance detection function will pull the gpio high
 * to make the switch open.
 *
 * @zdet_gpio_cb: callback function from machine driver
 * @codec: Codec instance
 *
 * Return: none
 */
void tasha_mbhc_zdet_gpio_ctrl(
		int (*zdet_gpio_cb)(struct snd_soc_codec *codec, bool high),
		struct snd_soc_codec *codec)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	tasha->zdet_gpio_cb = zdet_gpio_cb;
}
EXPORT_SYMBOL(tasha_mbhc_zdet_gpio_ctrl);

static void tasha_mbhc_zdet_ramp(struct snd_soc_codec *codec,
				 struct tasha_mbhc_zdet_param *zdet_param,
				 int32_t *zl, int32_t *zr, s16 *d1_a)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	int32_t zdet = 0;

	snd_soc_update_bits(codec, WCD9335_MBHC_ZDET_ANA_CTL, 0x70,
			    zdet_param->ldo_ctl << 4);
	snd_soc_update_bits(codec, WCD9335_ANA_MBHC_BTN5, 0xFC,
			    zdet_param->btn5);
	snd_soc_update_bits(codec, WCD9335_ANA_MBHC_BTN6, 0xFC,
			    zdet_param->btn6);
	snd_soc_update_bits(codec, WCD9335_ANA_MBHC_BTN7, 0xFC,
			    zdet_param->btn7);
	snd_soc_update_bits(codec, WCD9335_MBHC_ZDET_ANA_CTL, 0x0F,
			    zdet_param->noff);
	snd_soc_update_bits(codec, WCD9335_MBHC_ZDET_RAMP_CTL, 0x0F,
			    zdet_param->nshift);

	if (!zl)
		goto z_right;
	/* Start impedance measurement for HPH_L */
	regmap_update_bits(wcd9xxx->regmap,
			   WCD9335_ANA_MBHC_ZDET, 0x80, 0x80);
	dev_dbg(wcd9xxx->dev, "%s: ramp for HPH_L, noff = %d\n",
					__func__, zdet_param->noff);
	tasha_mbhc_get_result_params(wcd9xxx, d1_a, zdet_param->noff, &zdet);
	regmap_update_bits(wcd9xxx->regmap,
			   WCD9335_ANA_MBHC_ZDET, 0x80, 0x00);

	*zl = zdet;

z_right:
	if (!zr)
		return;
	/* Start impedance measurement for HPH_R */
	regmap_update_bits(wcd9xxx->regmap,
			   WCD9335_ANA_MBHC_ZDET, 0x40, 0x40);
	dev_dbg(wcd9xxx->dev, "%s: ramp for HPH_R, noff = %d\n",
					__func__, zdet_param->noff);
	tasha_mbhc_get_result_params(wcd9xxx, d1_a, zdet_param->noff, &zdet);
	regmap_update_bits(wcd9xxx->regmap,
			   WCD9335_ANA_MBHC_ZDET, 0x40, 0x00);

	*zr = zdet;
}

static inline void tasha_wcd_mbhc_qfuse_cal(struct snd_soc_codec *codec,
					int32_t *z_val, int flag_l_r)
{
	s16 q1;
	int q1_cal;

	if (*z_val < (TASHA_ZDET_VAL_400/1000))
		q1 = snd_soc_read(codec,
			WCD9335_CHIP_TIER_CTRL_EFUSE_VAL_OUT1 + (2 * flag_l_r));
	else
		q1 = snd_soc_read(codec,
			WCD9335_CHIP_TIER_CTRL_EFUSE_VAL_OUT2 + (2 * flag_l_r));
	if (q1 & 0x80)
		q1_cal = (10000 - ((q1 & 0x7F) * 25));
	else
		q1_cal = (10000 + (q1 * 25));
	if (q1_cal > 0)
		*z_val = ((*z_val) * 10000) / q1_cal;
}

static void tasha_wcd_mbhc_calc_impedance(struct wcd_mbhc *mbhc, uint32_t *zl,
					  uint32_t *zr)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *wcd9xxx = tasha->wcd9xxx;
	s16 reg0, reg1, reg2, reg3, reg4;
	int32_t z1L, z1R, z1Ls;
	int zMono, z_diff1, z_diff2;
	bool is_fsm_disable = false;
	bool is_change = false;
	struct tasha_mbhc_zdet_param zdet_param[] = {
		{4, 0, 4, 0x08, 0x14, 0x18}, /* < 32ohm */
		{2, 0, 3, 0x18, 0x7C, 0x90}, /* 32ohm < Z < 400ohm */
		{1, 4, 5, 0x18, 0x7C, 0x90}, /* 400ohm < Z < 1200ohm */
		{1, 6, 7, 0x18, 0x7C, 0x90}, /* >1200ohm */
	};
	struct tasha_mbhc_zdet_param *zdet_param_ptr = NULL;
	s16 d1_a[][4] = {
		{0, 30, 90, 30},
		{0, 30, 30, 5},
		{0, 30, 30, 5},
		{0, 30, 30, 5},
	};
	s16 *d1 = NULL;

	if (!TASHA_IS_2_0(wcd9xxx)) {
		dev_dbg(codec->dev, "%s: Z-det is not supported for this codec version\n",
					__func__);
		*zl = 0;
		*zr = 0;
		return;
	}
	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	if (tasha->zdet_gpio_cb)
		is_change = tasha->zdet_gpio_cb(codec, true);

	reg0 = snd_soc_read(codec, WCD9335_ANA_MBHC_BTN5);
	reg1 = snd_soc_read(codec, WCD9335_ANA_MBHC_BTN6);
	reg2 = snd_soc_read(codec, WCD9335_ANA_MBHC_BTN7);
	reg3 = snd_soc_read(codec, WCD9335_MBHC_CTL_1);
	reg4 = snd_soc_read(codec, WCD9335_MBHC_ZDET_ANA_CTL);

	if (snd_soc_read(codec, WCD9335_ANA_MBHC_ELECT) & 0x80) {
		is_fsm_disable = true;
		regmap_update_bits(wcd9xxx->regmap,
				   WCD9335_ANA_MBHC_ELECT, 0x80, 0x00);
	}

	/* For NO-jack, disable L_DET_EN before Z-det measurements */
	if (mbhc->hphl_swh)
		regmap_update_bits(wcd9xxx->regmap,
				   WCD9335_ANA_MBHC_MECH, 0x80, 0x00);

	/* Enable AZ */
	snd_soc_update_bits(codec, WCD9335_MBHC_CTL_1, 0x0C, 0x04);
	/* Turn off 100k pull down on HPHL */
	regmap_update_bits(wcd9xxx->regmap,
			   WCD9335_ANA_MBHC_MECH, 0x01, 0x00);

	/* First get impedance on Left */
	d1 = d1_a[1];
	zdet_param_ptr = &zdet_param[1];
	tasha_mbhc_zdet_ramp(codec, zdet_param_ptr, &z1L, NULL, d1);

	if (!TASHA_MBHC_IS_SECOND_RAMP_REQUIRED(z1L))
		goto left_ch_impedance;

	/* second ramp for left ch */
	if (z1L < TASHA_ZDET_VAL_32) {
		zdet_param_ptr = &zdet_param[0];
		d1 = d1_a[0];
	} else if ((z1L > TASHA_ZDET_VAL_400) && (z1L <= TASHA_ZDET_VAL_1200)) {
		zdet_param_ptr = &zdet_param[2];
		d1 = d1_a[2];
	} else if (z1L > TASHA_ZDET_VAL_1200) {
		zdet_param_ptr = &zdet_param[3];
		d1 = d1_a[3];
	}
	tasha_mbhc_zdet_ramp(codec, zdet_param_ptr, &z1L, NULL, d1);

left_ch_impedance:
	if ((z1L == TASHA_ZDET_FLOATING_IMPEDANCE) ||
		(z1L > TASHA_ZDET_VAL_100K)) {
		*zl = TASHA_ZDET_FLOATING_IMPEDANCE;
		zdet_param_ptr = &zdet_param[1];
		d1 = d1_a[1];
	} else {
		*zl = z1L/1000;
		tasha_wcd_mbhc_qfuse_cal(codec, zl, 0);
	}
	dev_dbg(codec->dev, "%s: impedance on HPH_L = %d(ohms)\n",
				__func__, *zl);

	/* start of right impedance ramp and calculation */
	tasha_mbhc_zdet_ramp(codec, zdet_param_ptr, NULL, &z1R, d1);
	if (TASHA_MBHC_IS_SECOND_RAMP_REQUIRED(z1R)) {
		if (((z1R > TASHA_ZDET_VAL_1200) &&
			(zdet_param_ptr->noff == 0x6)) ||
			((*zl) != TASHA_ZDET_FLOATING_IMPEDANCE))
			goto right_ch_impedance;
		/* second ramp for right ch */
		if (z1R < TASHA_ZDET_VAL_32) {
			zdet_param_ptr = &zdet_param[0];
			d1 = d1_a[0];
		} else if ((z1R > TASHA_ZDET_VAL_400) &&
			(z1R <= TASHA_ZDET_VAL_1200)) {
			zdet_param_ptr = &zdet_param[2];
			d1 = d1_a[2];
		} else if (z1R > TASHA_ZDET_VAL_1200) {
			zdet_param_ptr = &zdet_param[3];
			d1 = d1_a[3];
		}
		tasha_mbhc_zdet_ramp(codec, zdet_param_ptr, NULL, &z1R, d1);
	}
right_ch_impedance:
	if ((z1R == TASHA_ZDET_FLOATING_IMPEDANCE) ||
		(z1R > TASHA_ZDET_VAL_100K)) {
		*zr = TASHA_ZDET_FLOATING_IMPEDANCE;
	} else {
		*zr = z1R/1000;
		tasha_wcd_mbhc_qfuse_cal(codec, zr, 1);
	}
	dev_dbg(codec->dev, "%s: impedance on HPH_R = %d(ohms)\n",
				__func__, *zr);

	/* mono/stereo detection */
	if ((*zl == TASHA_ZDET_FLOATING_IMPEDANCE) &&
		(*zr == TASHA_ZDET_FLOATING_IMPEDANCE)) {
		dev_dbg(codec->dev,
			"%s: plug type is invalid or extension cable\n",
			__func__);
		goto zdet_complete;
	}
	if ((*zl == TASHA_ZDET_FLOATING_IMPEDANCE) ||
	    (*zr == TASHA_ZDET_FLOATING_IMPEDANCE) ||
	    ((*zl < WCD_MONO_HS_MIN_THR) && (*zr > WCD_MONO_HS_MIN_THR)) ||
	    ((*zl > WCD_MONO_HS_MIN_THR) && (*zr < WCD_MONO_HS_MIN_THR))) {
		dev_dbg(codec->dev,
			"%s: Mono plug type with one ch floating or shorted to GND\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_MONO;
		goto zdet_complete;
	}
	snd_soc_update_bits(codec, WCD9335_HPH_R_ATEST, 0x02, 0x02);
	snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x40, 0x01);
	if (*zl < (TASHA_ZDET_VAL_32/1000))
		tasha_mbhc_zdet_ramp(codec, &zdet_param[0], &z1Ls, NULL, d1);
	else
		tasha_mbhc_zdet_ramp(codec, &zdet_param[1], &z1Ls, NULL, d1);
	snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x40, 0x00);
	snd_soc_update_bits(codec, WCD9335_HPH_R_ATEST, 0x02, 0x00);
	z1Ls /= 1000;
	tasha_wcd_mbhc_qfuse_cal(codec, &z1Ls, 0);
	/* parallel of left Z and 9 ohm pull down resistor */
	zMono = ((*zl) * 9) / ((*zl) + 9);
	z_diff1 = (z1Ls > zMono) ? (z1Ls - zMono) : (zMono - z1Ls);
	z_diff2 = ((*zl) > z1Ls) ? ((*zl) - z1Ls) : (z1Ls - (*zl));
	if ((z_diff1 * (*zl + z1Ls)) > (z_diff2 * (z1Ls + zMono))) {
		dev_dbg(codec->dev, "%s: stereo plug type detected\n",
				__func__);
		mbhc->hph_type = WCD_MBHC_HPH_STEREO;
	} else {
		dev_dbg(codec->dev, "%s: MONO plug type detected\n",
			 __func__);
		mbhc->hph_type = WCD_MBHC_HPH_MONO;
	}

zdet_complete:
	snd_soc_write(codec, WCD9335_ANA_MBHC_BTN5, reg0);
	snd_soc_write(codec, WCD9335_ANA_MBHC_BTN6, reg1);
	snd_soc_write(codec, WCD9335_ANA_MBHC_BTN7, reg2);
	/* Turn on 100k pull down on HPHL */
	regmap_update_bits(wcd9xxx->regmap,
			   WCD9335_ANA_MBHC_MECH, 0x01, 0x01);

	/* For NO-jack, re-enable L_DET_EN after Z-det measurements */
	if (mbhc->hphl_swh)
		regmap_update_bits(wcd9xxx->regmap,
				   WCD9335_ANA_MBHC_MECH, 0x80, 0x80);

	snd_soc_write(codec, WCD9335_MBHC_ZDET_ANA_CTL, reg4);
	snd_soc_write(codec, WCD9335_MBHC_CTL_1, reg3);
	if (is_fsm_disable)
		regmap_update_bits(wcd9xxx->regmap,
				   WCD9335_ANA_MBHC_ELECT, 0x80, 0x80);
	if (tasha->zdet_gpio_cb && is_change)
		tasha->zdet_gpio_cb(codec, false);
}

static void tasha_mbhc_gnd_det_ctrl(struct snd_soc_codec *codec, bool enable)
{
	if (enable) {
		snd_soc_update_bits(codec, WCD9335_ANA_MBHC_MECH,
				    0x02, 0x02);
		snd_soc_update_bits(codec, WCD9335_ANA_MBHC_MECH,
				    0x40, 0x40);
	} else {
		snd_soc_update_bits(codec, WCD9335_ANA_MBHC_MECH,
				    0x40, 0x00);
		snd_soc_update_bits(codec, WCD9335_ANA_MBHC_MECH,
				    0x02, 0x00);
	}
}

static void tasha_mbhc_hph_pull_down_ctrl(struct snd_soc_codec *codec,
					  bool enable)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	if (enable) {
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2,
				    0x40, 0x40);
		if (TASHA_IS_2_0(tasha->wcd9xxx))
			snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2,
					    0x10, 0x10);
	} else {
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2,
				    0x40, 0x00);
		if (TASHA_IS_2_0(tasha->wcd9xxx))
			snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2,
					    0x10, 0x00);
	}
}

static void tasha_mbhc_moisture_config(struct wcd_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;

	if (mbhc->moist_vref == V_OFF)
		return;

	/* Donot enable moisture detection if jack type is NC */
	if (!mbhc->hphl_swh) {
		dev_dbg(codec->dev, "%s: disable moisture detection for NC\n",
			__func__);
		return;
	}

	snd_soc_update_bits(codec, WCD9335_MBHC_CTL_2,
			    0x0C, mbhc->moist_vref << 2);
	tasha_mbhc_hph_l_pull_up_control(codec, mbhc->moist_iref);
}

static void tasha_update_anc_state(struct snd_soc_codec *codec, bool enable,
				   int anc_num)
{
	if (enable)
		snd_soc_update_bits(codec, WCD9335_CDC_RX1_RX_PATH_CFG0 +
				(20 * anc_num), 0x10, 0x10);
	else
		snd_soc_update_bits(codec, WCD9335_CDC_RX1_RX_PATH_CFG0 +
				(20 * anc_num), 0x10, 0x00);
}

static bool tasha_is_anc_on(struct wcd_mbhc *mbhc)
{
	bool anc_on = false;
	u16 ancl, ancr;

	ancl =
	(snd_soc_read(mbhc->codec, WCD9335_CDC_RX1_RX_PATH_CFG0)) & 0x10;
	ancr =
	(snd_soc_read(mbhc->codec, WCD9335_CDC_RX2_RX_PATH_CFG0)) & 0x10;

	anc_on = !!(ancl | ancr);

	return anc_on;
}

static const struct wcd_mbhc_cb mbhc_cb = {
	.request_irq = tasha_mbhc_request_irq,
	.irq_control = tasha_mbhc_irq_control,
	.free_irq = tasha_mbhc_free_irq,
	.clk_setup = tasha_mbhc_clk_setup,
	.map_btn_code_to_num = tasha_mbhc_btn_to_num,
	.enable_mb_source = tasha_enable_ext_mb_source,
	.mbhc_bias = tasha_mbhc_mbhc_bias_control,
	.set_btn_thr = tasha_mbhc_program_btn_thr,
	.lock_sleep = tasha_mbhc_lock_sleep,
	.register_notifier = tasha_mbhc_register_notifier,
	.micbias_enable_status = tasha_mbhc_micb_en_status,
	.hph_pa_on_status = tasha_mbhc_hph_pa_on_status,
	.hph_pull_up_control = tasha_mbhc_hph_l_pull_up_control,
	.mbhc_micbias_control = tasha_mbhc_request_micbias,
	.mbhc_micb_ramp_control = tasha_mbhc_micb_ramp_control,
	.get_hwdep_fw_cal = tasha_get_hwdep_fw_cal,
	.mbhc_micb_ctrl_thr_mic = tasha_mbhc_micb_ctrl_threshold_mic,
	.compute_impedance = tasha_wcd_mbhc_calc_impedance,
	.mbhc_gnd_det_ctrl = tasha_mbhc_gnd_det_ctrl,
	.hph_pull_down_ctrl = tasha_mbhc_hph_pull_down_ctrl,
	.mbhc_moisture_config = tasha_mbhc_moisture_config,
	.update_anc_state = tasha_update_anc_state,
	.is_anc_on = tasha_is_anc_on,
};

static int tasha_get_anc_slot(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tasha->anc_slot;
	return 0;
}

static int tasha_put_anc_slot(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	tasha->anc_slot = ucontrol->value.integer.value[0];
	return 0;
}

static int tasha_get_anc_func(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = (tasha->anc_func == true ? 1 : 0);
	return 0;
}

static int tasha_put_anc_func(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	mutex_lock(&tasha->codec_mutex);
	tasha->anc_func = (!ucontrol->value.integer.value[0] ? false : true);

	dev_dbg(codec->dev, "%s: anc_func %x", __func__, tasha->anc_func);

	if (tasha->anc_func == true) {
		snd_soc_dapm_enable_pin(dapm, "ANC LINEOUT2 PA");
		snd_soc_dapm_enable_pin(dapm, "ANC LINEOUT2");
		snd_soc_dapm_enable_pin(dapm, "ANC LINEOUT1 PA");
		snd_soc_dapm_enable_pin(dapm, "ANC LINEOUT1");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHR PA");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHR");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHL PA");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHL");
		snd_soc_dapm_enable_pin(dapm, "ANC EAR PA");
		snd_soc_dapm_enable_pin(dapm, "ANC EAR");
		snd_soc_dapm_enable_pin(dapm, "ANC SPK1 PA");
		snd_soc_dapm_disable_pin(dapm, "LINEOUT2");
		snd_soc_dapm_disable_pin(dapm, "LINEOUT2 PA");
		snd_soc_dapm_disable_pin(dapm, "LINEOUT1");
		snd_soc_dapm_disable_pin(dapm, "LINEOUT1 PA");
		snd_soc_dapm_disable_pin(dapm, "HPHR");
		snd_soc_dapm_disable_pin(dapm, "HPHL");
		snd_soc_dapm_disable_pin(dapm, "HPHR PA");
		snd_soc_dapm_disable_pin(dapm, "HPHL PA");
		snd_soc_dapm_disable_pin(dapm, "EAR PA");
		snd_soc_dapm_disable_pin(dapm, "EAR");
	} else {
		snd_soc_dapm_disable_pin(dapm, "ANC LINEOUT2 PA");
		snd_soc_dapm_disable_pin(dapm, "ANC LINEOUT2");
		snd_soc_dapm_disable_pin(dapm, "ANC LINEOUT1 PA");
		snd_soc_dapm_disable_pin(dapm, "ANC LINEOUT1");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHR");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHL");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHR PA");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHL PA");
		snd_soc_dapm_disable_pin(dapm, "ANC EAR PA");
		snd_soc_dapm_disable_pin(dapm, "ANC EAR");
		snd_soc_dapm_disable_pin(dapm, "ANC SPK1 PA");
		snd_soc_dapm_enable_pin(dapm, "LINEOUT2");
		snd_soc_dapm_enable_pin(dapm, "LINEOUT2 PA");
		snd_soc_dapm_enable_pin(dapm, "LINEOUT1");
		snd_soc_dapm_enable_pin(dapm, "LINEOUT1 PA");
		snd_soc_dapm_enable_pin(dapm, "HPHR");
		snd_soc_dapm_enable_pin(dapm, "HPHL");
		snd_soc_dapm_enable_pin(dapm, "HPHR PA");
		snd_soc_dapm_enable_pin(dapm, "HPHL PA");
		snd_soc_dapm_enable_pin(dapm, "EAR PA");
		snd_soc_dapm_enable_pin(dapm, "EAR");
	}
	mutex_unlock(&tasha->codec_mutex);
	snd_soc_dapm_sync(dapm);
	return 0;
}

static int tasha_get_clkmode(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = tasha->clk_mode;
	dev_dbg(codec->dev, "%s: clk_mode: %d\n", __func__, tasha->clk_mode);

	return 0;
}

static int tasha_put_clkmode(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	tasha->clk_mode = ucontrol->value.enumerated.item[0];
	dev_dbg(codec->dev, "%s: clk_mode: %d\n", __func__, tasha->clk_mode);

	return 0;
}

static int tasha_get_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	/* IIR filter band registers are at integer multiples of 16 */
	u16 iir_reg = WCD9335_CDC_SIDETONE_IIR0_IIR_CTL + 16 * iir_idx;

	ucontrol->value.integer.value[0] = (snd_soc_read(codec, iir_reg) &
					    (1 << band_idx)) != 0;

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int tasha_hph_impedance_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	uint32_t zl, zr;
	bool hphr;
	struct soc_multi_mixer_control *mc;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *priv = snd_soc_codec_get_drvdata(codec);

	mc = (struct soc_multi_mixer_control *)(kcontrol->private_value);
	hphr = mc->shift;
	wcd_mbhc_get_impedance(&priv->mbhc, &zl, &zr);
	dev_dbg(codec->dev, "%s: zl=%u(ohms), zr=%u(ohms)\n", __func__, zl, zr);
	ucontrol->value.integer.value[0] = hphr ? zr : zl;

	return 0;
}

static const struct snd_kcontrol_new impedance_detect_controls[] = {
	SOC_SINGLE_EXT("HPHL Impedance", 0, 0, UINT_MAX, 0,
		       tasha_hph_impedance_get, NULL),
	SOC_SINGLE_EXT("HPHR Impedance", 0, 1, UINT_MAX, 0,
		       tasha_hph_impedance_get, NULL),
};

static int tasha_get_hph_type(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct wcd_mbhc *mbhc;

	if (!priv) {
		dev_dbg(codec->dev, "%s: wcd9335 private data is NULL\n",
				__func__);
		return 0;
	}

	mbhc = &priv->mbhc;
	if (!mbhc) {
		dev_dbg(codec->dev, "%s: mbhc not initialized\n", __func__);
		return 0;
	}

	ucontrol->value.integer.value[0] = (u32) mbhc->hph_type;
	dev_dbg(codec->dev, "%s: hph_type = %u\n", __func__, mbhc->hph_type);

	return 0;
}

static const struct snd_kcontrol_new hph_type_detect_controls[] = {
	SOC_SINGLE_EXT("HPH Type", 0, 0, UINT_MAX, 0,
		       tasha_get_hph_type, NULL),
};

static int tasha_vi_feed_mixer_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tasha_p->vi_feed_value;

	return 0;
}

static int tasha_vi_feed_mixer_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = tasha_p->wcd9xxx;
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 port_id = mixer->shift;
	u32 enable = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: enable: %d, port_id:%d, dai_id: %d\n",
		__func__, enable, port_id, dai_id);

	tasha_p->vi_feed_value = ucontrol->value.integer.value[0];

	mutex_lock(&tasha_p->codec_mutex);
	if (enable) {
		if (port_id == TASHA_TX14 && !test_bit(VI_SENSE_1,
						&tasha_p->status_mask)) {
			list_add_tail(&core->tx_chs[TASHA_TX14].list,
					&tasha_p->dai[dai_id].wcd9xxx_ch_list);
			set_bit(VI_SENSE_1, &tasha_p->status_mask);
		}
		if (port_id == TASHA_TX15 && !test_bit(VI_SENSE_2,
						&tasha_p->status_mask)) {
			list_add_tail(&core->tx_chs[TASHA_TX15].list,
					&tasha_p->dai[dai_id].wcd9xxx_ch_list);
			set_bit(VI_SENSE_2, &tasha_p->status_mask);
		}
	} else {
		if (port_id == TASHA_TX14 && test_bit(VI_SENSE_1,
					&tasha_p->status_mask)) {
			list_del_init(&core->tx_chs[TASHA_TX14].list);
			clear_bit(VI_SENSE_1, &tasha_p->status_mask);
		}
		if (port_id == TASHA_TX15 && test_bit(VI_SENSE_2,
					&tasha_p->status_mask)) {
			list_del_init(&core->tx_chs[TASHA_TX15].list);
			clear_bit(VI_SENSE_2, &tasha_p->status_mask);
		}
	}
	mutex_unlock(&tasha_p->codec_mutex);
	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, enable, NULL);

	return 0;
}

/* virtual port entries */
static int slim_tx_mixer_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tasha_p->tx_port_value;
	return 0;
}

static int slim_tx_mixer_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);
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
		widget->name, ucontrol->id.name, tasha_p->tx_port_value,
		widget->shift, ucontrol->value.integer.value[0]);

	mutex_lock(&tasha_p->codec_mutex);

	if (tasha_p->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		if (dai_id >= ARRAY_SIZE(vport_slim_check_table)) {
			dev_err(codec->dev, "%s: dai_id: %d, out of bounds\n",
				__func__, dai_id);
			mutex_unlock(&tasha_p->codec_mutex);
			return -EINVAL;
		}
		vtable = vport_slim_check_table[dai_id];
	} else {
		if (dai_id >= ARRAY_SIZE(vport_i2s_check_table)) {
			dev_err(codec->dev, "%s: dai_id: %d, out of bounds\n",
				__func__, dai_id);
			return -EINVAL;
		}
		vtable = vport_i2s_check_table[dai_id];
	}
	switch (dai_id) {
	case AIF1_CAP:
	case AIF2_CAP:
	case AIF3_CAP:
		/* only add to the list if value not set */
		if (enable && !(tasha_p->tx_port_value & 1 << port_id)) {

			if (wcd9xxx_tx_vport_validation(vtable, port_id,
					tasha_p->dai, NUM_CODEC_DAIS)) {
				dev_dbg(codec->dev, "%s: TX%u is used by other virtual port\n",
					__func__, port_id);
				mutex_unlock(&tasha_p->codec_mutex);
				return 0;
			}
			tasha_p->tx_port_value |= 1 << port_id;
			list_add_tail(&core->tx_chs[port_id].list,
			      &tasha_p->dai[dai_id].wcd9xxx_ch_list
					      );
		} else if (!enable && (tasha_p->tx_port_value &
					1 << port_id)) {
			tasha_p->tx_port_value &= ~(1 << port_id);
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
			mutex_unlock(&tasha_p->codec_mutex);
			return 0;
		}
		break;
	case AIF4_MAD_TX:
	case AIF5_CPE_TX:
		break;
	default:
		pr_err("Unknown AIF %d\n", dai_id);
		mutex_unlock(&tasha_p->codec_mutex);
		return -EINVAL;
	}
	pr_debug("%s: name %s sname %s updated value %u shift %d\n", __func__,
		widget->name, widget->sname, tasha_p->tx_port_value,
		widget->shift);

	mutex_unlock(&tasha_p->codec_mutex);
	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, enable, update);

	return 0;
}

static int slim_rx_mux_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] =
			tasha_p->rx_port_value[widget->shift];
	return 0;
}

static const char *const slim_rx_mux_text[] = {
	"ZERO", "AIF1_PB", "AIF2_PB", "AIF3_PB", "AIF4_PB", "AIF_MIX1_PB"
};

static int slim_rx_mux_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_dapm_update *update = NULL;
	unsigned int rx_port_value;
	u32 port_id = widget->shift;

	tasha_p->rx_port_value[port_id] = ucontrol->value.enumerated.item[0];
	rx_port_value = tasha_p->rx_port_value[port_id];

	pr_debug("%s: wname %s cname %s value %u shift %d item %ld\n", __func__,
		widget->name, ucontrol->id.name, rx_port_value,
		widget->shift, ucontrol->value.integer.value[0]);

	mutex_lock(&tasha_p->codec_mutex);

	if (tasha_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		if (rx_port_value > 2) {
			dev_err(codec->dev, "%s: invalid AIF for I2C mode\n",
				__func__);
			goto err;
		}
	}
	/* value need to match the Virtual port and AIF number */
	switch (rx_port_value) {
	case 0:
		list_del_init(&core->rx_chs[port_id].list);
		break;
	case 1:
		if (wcd9xxx_rx_vport_validation(port_id +
			TASHA_RX_PORT_START_NUMBER,
			&tasha_p->dai[AIF1_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tasha_p->dai[AIF1_PB].wcd9xxx_ch_list);
		break;
	case 2:
		if (wcd9xxx_rx_vport_validation(port_id +
			TASHA_RX_PORT_START_NUMBER,
			&tasha_p->dai[AIF2_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tasha_p->dai[AIF2_PB].wcd9xxx_ch_list);
		break;
	case 3:
		if (wcd9xxx_rx_vport_validation(port_id +
			TASHA_RX_PORT_START_NUMBER,
			&tasha_p->dai[AIF3_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tasha_p->dai[AIF3_PB].wcd9xxx_ch_list);
		break;
	case 4:
		if (wcd9xxx_rx_vport_validation(port_id +
			TASHA_RX_PORT_START_NUMBER,
			&tasha_p->dai[AIF4_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tasha_p->dai[AIF4_PB].wcd9xxx_ch_list);
		break;
	case 5:
		if (wcd9xxx_rx_vport_validation(port_id +
			TASHA_RX_PORT_START_NUMBER,
			&tasha_p->dai[AIF_MIX1_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tasha_p->dai[AIF_MIX1_PB].wcd9xxx_ch_list);
		break;
	default:
		pr_err("Unknown AIF %d\n", rx_port_value);
		goto err;
	}
rtn:
	mutex_unlock(&tasha_p->codec_mutex);
	snd_soc_dapm_mux_update_power(widget->dapm, kcontrol,
					rx_port_value, e, update);

	return 0;
err:
	mutex_unlock(&tasha_p->codec_mutex);
	return -EINVAL;
}

static const struct soc_enum slim_rx_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(slim_rx_mux_text), slim_rx_mux_text);

static const struct snd_kcontrol_new slim_rx_mux[TASHA_RX_MAX] = {
	SOC_DAPM_ENUM_EXT("SLIM RX0 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX1 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX2 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX3 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX4 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX5 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX6 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX7 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
};

static const struct snd_kcontrol_new aif4_vi_mixer[] = {
	SOC_SINGLE_EXT("SPKR_VI_1", SND_SOC_NOPM, TASHA_TX14, 1, 0,
			tasha_vi_feed_mixer_get, tasha_vi_feed_mixer_put),
	SOC_SINGLE_EXT("SPKR_VI_2", SND_SOC_NOPM, TASHA_TX15, 1, 0,
			tasha_vi_feed_mixer_get, tasha_vi_feed_mixer_put),
};

static const struct snd_kcontrol_new aif1_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX0", SND_SOC_NOPM, TASHA_TX0, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, TASHA_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, TASHA_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, TASHA_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, TASHA_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, TASHA_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX6", SND_SOC_NOPM, TASHA_TX6, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX7", SND_SOC_NOPM, TASHA_TX7, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX8", SND_SOC_NOPM, TASHA_TX8, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX9", SND_SOC_NOPM, TASHA_TX9, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX10", SND_SOC_NOPM, TASHA_TX10, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX11", SND_SOC_NOPM, TASHA_TX11, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX13", SND_SOC_NOPM, TASHA_TX13, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static const struct snd_kcontrol_new aif2_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX0", SND_SOC_NOPM, TASHA_TX0, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, TASHA_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, TASHA_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, TASHA_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, TASHA_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, TASHA_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX6", SND_SOC_NOPM, TASHA_TX6, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX7", SND_SOC_NOPM, TASHA_TX7, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX8", SND_SOC_NOPM, TASHA_TX8, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX9", SND_SOC_NOPM, TASHA_TX9, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX10", SND_SOC_NOPM, TASHA_TX10, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX11", SND_SOC_NOPM, TASHA_TX11, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX13", SND_SOC_NOPM, TASHA_TX13, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static const struct snd_kcontrol_new aif3_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX0", SND_SOC_NOPM, TASHA_TX0, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, TASHA_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, TASHA_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, TASHA_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, TASHA_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, TASHA_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX6", SND_SOC_NOPM, TASHA_TX6, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX7", SND_SOC_NOPM, TASHA_TX7, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX8", SND_SOC_NOPM, TASHA_TX8, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX9", SND_SOC_NOPM, TASHA_TX9, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX10", SND_SOC_NOPM, TASHA_TX10, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX11", SND_SOC_NOPM, TASHA_TX11, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX13", SND_SOC_NOPM, TASHA_TX13, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static const struct snd_kcontrol_new aif4_mad_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX12", SND_SOC_NOPM, TASHA_TX12, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX13", SND_SOC_NOPM, TASHA_TX13, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, 0, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),

};

static const struct snd_kcontrol_new rx_int1_spline_mix_switch[] = {
	SOC_DAPM_SINGLE("HPHL Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new rx_int2_spline_mix_switch[] = {
	SOC_DAPM_SINGLE("HPHR Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new rx_int3_spline_mix_switch[] = {
	SOC_DAPM_SINGLE("LO1 Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new rx_int4_spline_mix_switch[] = {
	SOC_DAPM_SINGLE("LO2 Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new rx_int5_spline_mix_switch[] = {
	SOC_DAPM_SINGLE("LO3 Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new rx_int6_spline_mix_switch[] = {
	SOC_DAPM_SINGLE("LO4 Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new rx_int7_spline_mix_switch[] = {
	SOC_DAPM_SINGLE("SPKRL Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new rx_int8_spline_mix_switch[] = {
	SOC_DAPM_SINGLE("SPKRR Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new rx_int5_vbat_mix_switch[] = {
	SOC_DAPM_SINGLE("LO3 VBAT Enable", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new rx_int6_vbat_mix_switch[] = {
	SOC_DAPM_SINGLE("LO4 VBAT Enable", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new rx_int7_vbat_mix_switch[] = {
	SOC_DAPM_SINGLE("SPKRL VBAT Enable", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new rx_int8_vbat_mix_switch[] = {
	SOC_DAPM_SINGLE("SPKRR VBAT Enable", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new cpe_in_mix_switch[] = {
	SOC_DAPM_SINGLE("MAD_BYPASS", SND_SOC_NOPM, 0, 1, 0)
};



static int tasha_put_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	bool iir_band_en_status;
	int value = ucontrol->value.integer.value[0];
	u16 iir_reg = WCD9335_CDC_SIDETONE_IIR0_IIR_CTL + 16 * iir_idx;

	/* Mask first 5 bits, 6-8 are reserved */
	snd_soc_update_bits(codec, iir_reg, (1 << band_idx),
			    (value << band_idx));

	iir_band_en_status = ((snd_soc_read(codec, iir_reg) &
			      (1 << band_idx)) != 0);
	pr_debug("%s: IIR #%d band #%d enable %d\n", __func__,
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
		(WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t)) & 0x7F);

	value |= snd_soc_read(codec,
		(WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx));

	snd_soc_write(codec,
		(WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 1) & 0x7F);

	value |= (snd_soc_read(codec,
			       (WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				16 * iir_idx)) << 8);

	snd_soc_write(codec,
		(WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 2) & 0x7F);

	value |= (snd_soc_read(codec,
			       (WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				16 * iir_idx)) << 16);

	snd_soc_write(codec,
		(WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 3) & 0x7F);

	/* Mask bits top 2 bits since they are reserved */
	value |= ((snd_soc_read(codec,
				(WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				 16 * iir_idx)) & 0x3F) << 24);

	return value;
}

static int tasha_get_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
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

	pr_debug("%s: IIR #%d band #%d b0 = 0x%x\n"
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
		(WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value & 0xFF));

	snd_soc_write(codec,
		(WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_write(codec,
		(WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value >> 16) & 0xFF);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value >> 24) & 0x3F);
}

static void tasha_codec_enable_int_port(struct wcd9xxx_codec_dai_data *dai,
					struct snd_soc_codec *codec)
{
	struct wcd9xxx_ch *ch;
	int port_num = 0;
	unsigned short reg = 0;
	u8 val = 0;
	struct tasha_priv *tasha_p;

	if (!dai || !codec) {
		pr_err("%s: Invalid params\n", __func__);
		return;
	}

	tasha_p = snd_soc_codec_get_drvdata(codec);
	list_for_each_entry(ch, &dai->wcd9xxx_ch_list, list) {
		if (ch->port >= TASHA_RX_PORT_START_NUMBER) {
			port_num = ch->port - TASHA_RX_PORT_START_NUMBER;
			reg = TASHA_SLIM_PGD_PORT_INT_EN0 + (port_num / 8);
			val = wcd9xxx_interface_reg_read(tasha_p->wcd9xxx,
				reg);
			if (!(val & BYTE_BIT_MASK(port_num))) {
				val |= BYTE_BIT_MASK(port_num);
				wcd9xxx_interface_reg_write(
					tasha_p->wcd9xxx, reg, val);
				val = wcd9xxx_interface_reg_read(
					tasha_p->wcd9xxx, reg);
			}
		} else {
			port_num = ch->port;
			reg = TASHA_SLIM_PGD_PORT_INT_TX_EN0 + (port_num / 8);
			val = wcd9xxx_interface_reg_read(tasha_p->wcd9xxx,
				reg);
			if (!(val & BYTE_BIT_MASK(port_num))) {
				val |= BYTE_BIT_MASK(port_num);
				wcd9xxx_interface_reg_write(tasha_p->wcd9xxx,
					reg, val);
				val = wcd9xxx_interface_reg_read(
					tasha_p->wcd9xxx, reg);
			}
		}
	}
}

static int tasha_codec_enable_slim_chmask(struct wcd9xxx_codec_dai_data *dai,
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
						TASHA_SLIM_CLOSE_TIMEOUT));
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

static int tasha_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct wcd9xxx *core;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	struct wcd9xxx_codec_dai_data *dai;

	core = dev_get_drvdata(codec->dev->parent);

	dev_dbg(codec->dev, "%s: event called! codec name %s num_dai %d\n"
		"stream name %s event %d\n",
		__func__, codec->component.name,
		codec->component.num_dai, w->sname, event);

	/* Execute the callback only if interface type is slimbus */
	if (tasha_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		return 0;

	dai = &tasha_p->dai[w->shift];
	dev_dbg(codec->dev, "%s: w->name %s w->shift %d event %d\n",
		 __func__, w->name, w->shift, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dai->bus_down_in_recovery = false;
		tasha_codec_enable_int_port(dai, codec);
		(void) tasha_codec_enable_slim_chmask(dai, true);
		ret = wcd9xxx_cfg_slim_sch_rx(core, &dai->wcd9xxx_ch_list,
					      dai->rate, dai->bit_width,
					      &dai->grph);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		tasha_codec_vote_max_bw(codec, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wcd9xxx_disconnect_port(core, &dai->wcd9xxx_ch_list,
					      dai->grph);
		dev_dbg(codec->dev, "%s: Disconnect RX port, ret = %d\n",
			__func__, ret);

		if (!dai->bus_down_in_recovery)
			ret = tasha_codec_enable_slim_chmask(dai, false);
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

static int tasha_codec_enable_slimvi_feedback(struct snd_soc_dapm_widget *w,
					      struct snd_kcontrol *kcontrol,
					      int event)
{
	struct wcd9xxx *core = NULL;
	struct snd_soc_codec *codec = NULL;
	struct tasha_priv *tasha_p = NULL;
	int ret = 0;
	struct wcd9xxx_codec_dai_data *dai = NULL;

	if (!w) {
		pr_err("%s invalid params\n", __func__);
		return -EINVAL;
	}
	codec = snd_soc_dapm_to_codec(w->dapm);
	tasha_p = snd_soc_codec_get_drvdata(codec);
	core = tasha_p->wcd9xxx;

	dev_dbg(codec->dev, "%s: num_dai %d stream name %s\n",
		__func__, codec->component.num_dai, w->sname);

	/* Execute the callback only if interface type is slimbus */
	if (tasha_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		dev_err(codec->dev, "%s Interface is not correct", __func__);
		return 0;
	}

	dev_dbg(codec->dev, "%s(): w->name %s event %d w->shift %d\n",
		__func__, w->name, event, w->shift);
	if (w->shift != AIF4_VIFEED) {
		pr_err("%s Error in enabling the tx path\n", __func__);
		ret = -EINVAL;
		goto out_vi;
	}
	dai = &tasha_p->dai[w->shift];
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (test_bit(VI_SENSE_1, &tasha_p->status_mask)) {
			dev_dbg(codec->dev, "%s: spkr1 enabled\n", __func__);
			/* Enable V&I sensing */
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x20);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX9_SPKR_PROT_PATH_CTL, 0x0F, 0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX10_SPKR_PROT_PATH_CTL, 0x0F,
				0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX9_SPKR_PROT_PATH_CTL, 0x10, 0x10);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX10_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
		}
		if (test_bit(VI_SENSE_2, &tasha_p->status_mask)) {
			pr_debug("%s: spkr2 enabled\n", __func__);
			/* Enable V&I sensing */
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX11_SPKR_PROT_PATH_CTL, 0x0F,
				0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX12_SPKR_PROT_PATH_CTL, 0x0F,
				0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX11_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX12_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
		}
		dai->bus_down_in_recovery = false;
		tasha_codec_enable_int_port(dai, codec);
		(void) tasha_codec_enable_slim_chmask(dai, true);
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
			ret = tasha_codec_enable_slim_chmask(dai, false);
		if (ret < 0) {
			ret = wcd9xxx_disconnect_port(core,
				&dai->wcd9xxx_ch_list,
				dai->grph);
			dev_dbg(codec->dev, "%s: Disconnect TX port, ret = %d\n",
				__func__, ret);
		}
		if (test_bit(VI_SENSE_1, &tasha_p->status_mask)) {
			/* Disable V&I sensing */
			dev_dbg(codec->dev, "%s: spkr1 disabled\n", __func__);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x20);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX9_SPKR_PROT_PATH_CTL, 0x10, 0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX10_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
		}
		if (test_bit(VI_SENSE_2, &tasha_p->status_mask)) {
			/* Disable V&I sensing */
			dev_dbg(codec->dev, "%s: spkr2 disabled\n", __func__);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX11_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_TX12_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
		}
		break;
	}
out_vi:
	return ret;
}

/*
 * __tasha_codec_enable_slimtx: Enable the slimbus slave port
 *				 for TX path
 * @codec: Handle to the codec for which the slave port is to be
 *	   enabled.
 * @dai_data: The dai specific data for dai which is enabled.
 */
static int __tasha_codec_enable_slimtx(struct snd_soc_codec *codec,
		int event, struct wcd9xxx_codec_dai_data *dai)
{
	struct wcd9xxx *core;
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	/* Execute the callback only if interface type is slimbus */
	if (tasha_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		return 0;

	dev_dbg(codec->dev,
		"%s: event = %d\n", __func__, event);
	core = dev_get_drvdata(codec->dev->parent);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dai->bus_down_in_recovery = false;
		tasha_codec_enable_int_port(dai, codec);
		(void) tasha_codec_enable_slim_chmask(dai, true);
		ret = wcd9xxx_cfg_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
					      dai->rate, dai->bit_width,
					      &dai->grph);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wcd9xxx_close_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
						dai->grph);
		if (!dai->bus_down_in_recovery)
			ret = tasha_codec_enable_slim_chmask(dai, false);
		if (ret < 0) {
			ret = wcd9xxx_disconnect_port(core,
						      &dai->wcd9xxx_ch_list,
						      dai->grph);
			pr_debug("%s: Disconnect TX port, ret = %d\n",
				 __func__, ret);
		}

		break;
	}

	return ret;
}

static int tasha_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_codec_dai_data *dai;

	dev_dbg(codec->dev,
		"%s: w->name %s, w->shift = %d, num_dai %d stream name %s\n",
		__func__, w->name, w->shift,
		codec->component.num_dai, w->sname);

	dai = &tasha_p->dai[w->shift];
	return __tasha_codec_enable_slimtx(codec, event, dai);
}

static void tasha_codec_cpe_pp_set_cfg(struct snd_soc_codec *codec, int event)
{
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_codec_dai_data *dai;
	u8 bit_width, rate, buf_period;

	dai = &tasha_p->dai[AIF4_MAD_TX];
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		switch (dai->bit_width) {
		case 32:
			bit_width = 0xF;
			break;
		case 24:
			bit_width = 0xE;
			break;
		case 20:
			bit_width = 0xD;
			break;
		case 16:
		default:
			bit_width = 0x0;
			break;
		}
		snd_soc_update_bits(codec, WCD9335_CPE_SS_TX_PP_CFG, 0x0F,
				    bit_width);

		switch (dai->rate) {
		case 384000:
			rate = 0x30;
			break;
		case 192000:
			rate = 0x20;
			break;
		case 48000:
			rate = 0x10;
			break;
		case 16000:
		default:
			rate = 0x00;
			break;
		}
		snd_soc_update_bits(codec, WCD9335_CPE_SS_TX_PP_CFG, 0x70,
				    rate);

		buf_period = (dai->rate * (dai->bit_width/8)) / (16*1000);
		snd_soc_update_bits(codec, WCD9335_CPE_SS_TX_PP_BUF_INT_PERIOD,
				    0xFF, buf_period);
		dev_dbg(codec->dev, "%s: PP buffer period= 0x%x\n",
			__func__, buf_period);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_write(codec, WCD9335_CPE_SS_TX_PP_CFG, 0x3C);
		snd_soc_write(codec, WCD9335_CPE_SS_TX_PP_BUF_INT_PERIOD, 0x60);
		break;

	default:
		break;
	}
}

/*
 * tasha_codec_get_mad_port_id: Callback function that will be invoked
 *	to get the port ID for MAD.
 * @codec: Handle to the codec
 * @port_id: cpe port_id needs to enable
 */
static int tasha_codec_get_mad_port_id(struct snd_soc_codec *codec,
				       u16 *port_id)
{
	struct tasha_priv *tasha_p;
	struct wcd9xxx_codec_dai_data *dai;
	struct wcd9xxx_ch *ch;

	if (!port_id || !codec)
		return -EINVAL;

	tasha_p = snd_soc_codec_get_drvdata(codec);
	if (!tasha_p)
		return -EINVAL;

	dai = &tasha_p->dai[AIF4_MAD_TX];
	list_for_each_entry(ch, &dai->wcd9xxx_ch_list, list) {
		if (ch->port == TASHA_TX12)
			*port_id = WCD_CPE_AFE_OUT_PORT_2;
		else if (ch->port == TASHA_TX13)
			*port_id = WCD_CPE_AFE_OUT_PORT_4;
		else {
			dev_err(codec->dev, "%s: invalid mad_port = %d\n",
					__func__, ch->port);
			return -EINVAL;
		}
	}
	dev_dbg(codec->dev, "%s: port_id = %d\n", __func__, *port_id);

	return 0;
}

/*
 * tasha_codec_enable_slimtx_mad: Callback function that will be invoked
 *	to setup the slave port for MAD.
 * @codec: Handle to the codec
 * @event: Indicates whether to enable or disable the slave port
 */
static int tasha_codec_enable_slimtx_mad(struct snd_soc_codec *codec,
					 u8 event)
{
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_codec_dai_data *dai;
	struct wcd9xxx_ch *ch;
	int dapm_event = SND_SOC_DAPM_POST_PMU;
	u16 port = 0;
	int ret = 0;

	dai = &tasha_p->dai[AIF4_MAD_TX];

	if (event == 0)
		dapm_event = SND_SOC_DAPM_POST_PMD;

	dev_dbg(codec->dev,
		"%s: mad_channel, event = 0x%x\n",
		 __func__, event);

	list_for_each_entry(ch, &dai->wcd9xxx_ch_list, list) {
		dev_dbg(codec->dev, "%s: mad_port = %d, event = 0x%x\n",
			__func__, ch->port, event);
		if (ch->port == TASHA_TX13) {
			tasha_codec_cpe_pp_set_cfg(codec, dapm_event);
			port = TASHA_TX13;
			break;
		}
	}

	ret = __tasha_codec_enable_slimtx(codec, dapm_event, dai);

	if (port == TASHA_TX13) {
		switch (dapm_event) {
		case SND_SOC_DAPM_POST_PMU:
			snd_soc_update_bits(codec,
				WCD9335_CODEC_RPM_PWR_CPE_DRAM1_SHUTDOWN,
				0x20, 0x00);
			snd_soc_update_bits(codec,
				WCD9335_DATA_HUB_DATA_HUB_SB_TX13_INP_CFG,
				0x03, 0x02);
			snd_soc_update_bits(codec, WCD9335_CPE_SS_CFG,
					    0x80, 0x80);
			break;
		case SND_SOC_DAPM_POST_PMD:
			snd_soc_update_bits(codec,
				WCD9335_CODEC_RPM_PWR_CPE_DRAM1_SHUTDOWN,
				0x20, 0x20);
			snd_soc_update_bits(codec,
				WCD9335_DATA_HUB_DATA_HUB_SB_TX13_INP_CFG,
				0x03, 0x00);
			snd_soc_update_bits(codec, WCD9335_CPE_SS_CFG,
					    0x80, 0x00);
			break;
		}
	}

	return ret;
}

static int tasha_put_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
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
		(WCD9335_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
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

static int tasha_get_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tasha->comp_enabled[comp];
	return 0;
}

static int tasha_set_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	pr_debug("%s: Compander %d enable current %d, new %d\n",
		 __func__, comp + 1, tasha->comp_enabled[comp], value);
	tasha->comp_enabled[comp] = value;

	/* Any specific register configuration for compander */
	switch (comp) {
	case COMPANDER_1:
		/* Set Gain Source Select based on compander enable/disable */
		snd_soc_update_bits(codec, WCD9335_HPH_L_EN, 0x20,
				(value ? 0x00:0x20));
		break;
	case COMPANDER_2:
		snd_soc_update_bits(codec, WCD9335_HPH_R_EN, 0x20,
				(value ? 0x00:0x20));
		break;
	case COMPANDER_3:
		break;
	case COMPANDER_4:
		break;
	case COMPANDER_5:
		snd_soc_update_bits(codec, WCD9335_SE_LO_LO3_GAIN, 0x20,
				(value ? 0x00:0x20));
		break;
	case COMPANDER_6:
		snd_soc_update_bits(codec, WCD9335_SE_LO_LO4_GAIN, 0x20,
				(value ? 0x00:0x20));
		break;
	case COMPANDER_7:
		break;
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

static void tasha_codec_init_flyback(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, WCD9335_HPH_L_EN, 0xC0, 0x00);
	snd_soc_update_bits(codec, WCD9335_HPH_R_EN, 0xC0, 0x00);
	snd_soc_update_bits(codec, WCD9335_RX_BIAS_FLYB_BUFF, 0x0F, 0x00);
	snd_soc_update_bits(codec, WCD9335_RX_BIAS_FLYB_BUFF, 0xF0, 0x00);
}

static int tasha_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tasha->rx_bias_count++;
		if (tasha->rx_bias_count == 1) {
			if (TASHA_IS_2_0(tasha->wcd9xxx))
				tasha_codec_init_flyback(codec);
			snd_soc_update_bits(codec, WCD9335_ANA_RX_SUPPLIES,
					    0x01, 0x01);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		tasha->rx_bias_count--;
		if (!tasha->rx_bias_count)
			snd_soc_update_bits(codec, WCD9335_ANA_RX_SUPPLIES,
					    0x01, 0x00);
		break;
	};
	dev_dbg(codec->dev, "%s: Current RX BIAS user count: %d\n", __func__,
		tasha->rx_bias_count);

	return 0;
}

static void tasha_realign_anc_coeff(struct snd_soc_codec *codec,
				    u16 reg1, u16 reg2)
{
	u8 val1, val2, tmpval1, tmpval2;

	snd_soc_write(codec, reg1, 0x00);
	tmpval1 = snd_soc_read(codec, reg2);
	tmpval2 = snd_soc_read(codec, reg2);
	snd_soc_write(codec, reg1, 0x00);
	snd_soc_write(codec, reg2, 0xFF);
	snd_soc_write(codec, reg1, 0x01);
	snd_soc_write(codec, reg2, 0xFF);

	snd_soc_write(codec, reg1, 0x00);
	val1 = snd_soc_read(codec, reg2);
	val2 = snd_soc_read(codec, reg2);

	if (val1 == 0x0F && val2 == 0xFF) {
		dev_dbg(codec->dev, "%s: ANC0 co-eff index re-aligned\n",
			__func__);
		snd_soc_read(codec, reg2);
		snd_soc_write(codec, reg1, 0x00);
		snd_soc_write(codec, reg2, tmpval2);
		snd_soc_write(codec, reg1, 0x01);
		snd_soc_write(codec, reg2, tmpval1);
	} else if (val1 == 0xFF && val2 == 0x0F) {
		dev_dbg(codec->dev, "%s: ANC1 co-eff index already aligned\n",
			__func__);
		snd_soc_write(codec, reg1, 0x00);
		snd_soc_write(codec, reg2, tmpval1);
		snd_soc_write(codec, reg1, 0x01);
		snd_soc_write(codec, reg2, tmpval2);
	} else {
		dev_err(codec->dev, "%s: ANC0 co-eff index not aligned\n",
			__func__);
	}
}

static int tasha_codec_enable_anc(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
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

	if (!tasha->anc_func)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		hwdep_cal = wcdcal_get_fw_cal(tasha->fw_data, WCD9XXX_ANC_CAL);
		if (hwdep_cal) {
			data = hwdep_cal->data;
			cal_size = hwdep_cal->size;
			dev_dbg(codec->dev, "%s: using hwdep calibration\n",
				__func__);
		} else {
			filename = "wcd9335/wcd9335_anc.bin";
			ret = request_firmware(&fw, filename, codec->dev);
			if (ret != 0) {
				dev_err(codec->dev,
				"Failed to acquire ANC data: %d\n", ret);
				return -ENODEV;
			}
			if (!fw) {
				dev_err(codec->dev, "failed to get anc fw");
				return -ENODEV;
			}
			data = fw->data;
			cal_size = fw->size;
			dev_dbg(codec->dev,
			"%s: using request_firmware calibration\n", __func__);
		}
		if (cal_size < sizeof(struct wcd9xxx_anc_header)) {
			dev_err(codec->dev, "Not enough data\n");
			ret = -ENOMEM;
			goto err;
		}
		/* First number is the number of register writes */
		anc_head = (struct wcd9xxx_anc_header *)(data);
		anc_ptr = (u32 *)(data +
				  sizeof(struct wcd9xxx_anc_header));
		anc_size_remaining = cal_size -
				     sizeof(struct wcd9xxx_anc_header);
		num_anc_slots = anc_head->num_anc_slots;

		if (tasha->anc_slot >= num_anc_slots) {
			dev_err(codec->dev, "Invalid ANC slot selected\n");
			ret = -EINVAL;
			goto err;
		}
		for (i = 0; i < num_anc_slots; i++) {
			if (anc_size_remaining < TASHA_PACKED_REG_SIZE) {
				dev_err(codec->dev,
					"Invalid register format\n");
				ret = -EINVAL;
				goto err;
			}
			anc_writes_size = (u32)(*anc_ptr);
			anc_size_remaining -= sizeof(u32);
			anc_ptr += 1;

			if (anc_writes_size * TASHA_PACKED_REG_SIZE
				> anc_size_remaining) {
				dev_err(codec->dev,
					"Invalid register format\n");
				ret = -EINVAL;
				goto err;
			}

			if (tasha->anc_slot == i)
				break;

			anc_size_remaining -= (anc_writes_size *
				TASHA_PACKED_REG_SIZE);
			anc_ptr += anc_writes_size;
		}
		if (i == num_anc_slots) {
			dev_err(codec->dev, "Selected ANC slot not present\n");
			ret = -EINVAL;
			goto err;
		}

		i = 0;
		anc_cal_size = anc_writes_size;

		if (!strcmp(w->name, "RX INT0 DAC") ||
		    !strcmp(w->name, "ANC SPK1 PA"))
			tasha_realign_anc_coeff(codec,
					WCD9335_CDC_ANC0_IIR_COEFF_1_CTL,
					WCD9335_CDC_ANC0_IIR_COEFF_2_CTL);

		if (!strcmp(w->name, "RX INT1 DAC") ||
			!strcmp(w->name, "RX INT3 DAC")) {
			tasha_realign_anc_coeff(codec,
					WCD9335_CDC_ANC0_IIR_COEFF_1_CTL,
					WCD9335_CDC_ANC0_IIR_COEFF_2_CTL);
			anc_writes_size = anc_cal_size / 2;
			snd_soc_update_bits(codec,
			WCD9335_CDC_ANC0_CLK_RESET_CTL, 0x39, 0x39);
		} else if (!strcmp(w->name, "RX INT2 DAC") ||
				!strcmp(w->name, "RX INT4 DAC")) {
			tasha_realign_anc_coeff(codec,
					WCD9335_CDC_ANC1_IIR_COEFF_1_CTL,
					WCD9335_CDC_ANC1_IIR_COEFF_2_CTL);
			i = anc_cal_size / 2;
			snd_soc_update_bits(codec,
			WCD9335_CDC_ANC1_CLK_RESET_CTL, 0x39, 0x39);
		}

		for (; i < anc_writes_size; i++) {
			TASHA_CODEC_UNPACK_ENTRY(anc_ptr[i], reg, mask, val);
			snd_soc_write(codec, reg, (val & mask));
		}
		if (!strcmp(w->name, "RX INT1 DAC") ||
			!strcmp(w->name, "RX INT3 DAC")) {
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC0_CLK_RESET_CTL, 0x08, 0x08);
		} else if (!strcmp(w->name, "RX INT2 DAC") ||
				!strcmp(w->name, "RX INT4 DAC")) {
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC1_CLK_RESET_CTL, 0x08, 0x08);
		}

		if (!hwdep_cal)
			release_firmware(fw);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Remove ANC Rx from reset */
		snd_soc_update_bits(codec, WCD9335_CDC_ANC0_CLK_RESET_CTL,
				    0x08, 0x00);
		snd_soc_update_bits(codec, WCD9335_CDC_ANC1_CLK_RESET_CTL,
				    0x08, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (!strcmp(w->name, "ANC HPHL PA") ||
		    !strcmp(w->name, "ANC EAR PA") ||
		    !strcmp(w->name, "ANC SPK1 PA") ||
		    !strcmp(w->name, "ANC LINEOUT1 PA")) {
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC0_MODE_1_CTL, 0x30, 0x00);
			msleep(50);
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC0_MODE_1_CTL, 0x01, 0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC0_CLK_RESET_CTL, 0x38, 0x38);
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC0_CLK_RESET_CTL, 0x07, 0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC0_CLK_RESET_CTL, 0x38, 0x00);
		} else if (!strcmp(w->name, "ANC HPHR PA") ||
			   !strcmp(w->name, "ANC LINEOUT2 PA")) {
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC1_MODE_1_CTL, 0x30, 0x00);
			msleep(50);
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC1_MODE_1_CTL, 0x01, 0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC1_CLK_RESET_CTL, 0x38, 0x38);
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC1_CLK_RESET_CTL, 0x07, 0x00);
			snd_soc_update_bits(codec,
				WCD9335_CDC_ANC1_CLK_RESET_CTL, 0x38, 0x00);
		}
		break;
	}

	return 0;
err:
	if (!hwdep_cal)
		release_firmware(fw);
	return ret;
}

static void tasha_codec_clear_anc_tx_hold(struct tasha_priv *tasha)
{
	if (test_and_clear_bit(ANC_MIC_AMIC1, &tasha->status_mask))
		tasha_codec_set_tx_hold(tasha->codec, WCD9335_ANA_AMIC1, false);
	if (test_and_clear_bit(ANC_MIC_AMIC2, &tasha->status_mask))
		tasha_codec_set_tx_hold(tasha->codec, WCD9335_ANA_AMIC2, false);
	if (test_and_clear_bit(ANC_MIC_AMIC3, &tasha->status_mask))
		tasha_codec_set_tx_hold(tasha->codec, WCD9335_ANA_AMIC3, false);
	if (test_and_clear_bit(ANC_MIC_AMIC4, &tasha->status_mask))
		tasha_codec_set_tx_hold(tasha->codec, WCD9335_ANA_AMIC4, false);
	if (test_and_clear_bit(ANC_MIC_AMIC5, &tasha->status_mask))
		tasha_codec_set_tx_hold(tasha->codec, WCD9335_ANA_AMIC5, false);
	if (test_and_clear_bit(ANC_MIC_AMIC6, &tasha->status_mask))
		tasha_codec_set_tx_hold(tasha->codec, WCD9335_ANA_AMIC6, false);
}

static void tasha_codec_hph_post_pa_config(struct tasha_priv *tasha,
					   int mode, int event)
{
	u8 scale_val = 0;

	if (!TASHA_IS_2_0(tasha->wcd9xxx))
		return;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		switch (mode) {
		case CLS_H_HIFI:
			scale_val = 0x3;
			break;
		case CLS_H_LOHIFI:
			scale_val = 0x1;
			break;
		}
		if (tasha->anc_func) {
			/* Clear Tx FE HOLD if both PAs are enabled */
			if ((snd_soc_read(tasha->codec, WCD9335_ANA_HPH) &
			     0xC0) == 0xC0) {
				tasha_codec_clear_anc_tx_hold(tasha);
			}
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		scale_val = 0x6;
		break;
	}

	if (scale_val)
		snd_soc_update_bits(tasha->codec, WCD9335_HPH_PA_CTL1, 0x0E,
				    scale_val << 1);
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (tasha->comp_enabled[COMPANDER_1] ||
		    tasha->comp_enabled[COMPANDER_2]) {
			snd_soc_update_bits(tasha->codec, WCD9335_HPH_L_EN,
					    0x20, 0x00);
			snd_soc_update_bits(tasha->codec, WCD9335_HPH_R_EN,
					    0x20, 0x00);
			snd_soc_update_bits(tasha->codec, WCD9335_HPH_AUTO_CHOP,
					    0x20, 0x20);
		}
		snd_soc_update_bits(tasha->codec, WCD9335_HPH_L_EN, 0x1F,
				    tasha->hph_l_gain);
		snd_soc_update_bits(tasha->codec, WCD9335_HPH_R_EN, 0x1F,
				    tasha->hph_r_gain);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(tasha->codec, WCD9335_HPH_AUTO_CHOP, 0x20,
				    0x00);
	}
}

static void tasha_codec_override(struct snd_soc_codec *codec,
				 int mode,
				 int event)
{
	if (mode == CLS_AB) {
		switch (event) {
		case SND_SOC_DAPM_POST_PMU:
			if (!(snd_soc_read(codec,
					WCD9335_CDC_RX2_RX_PATH_CTL) & 0x10) &&
				(!(snd_soc_read(codec,
					WCD9335_CDC_RX1_RX_PATH_CTL) & 0x10)))
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

static int tasha_codec_enable_hphr_pa(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int hph_mode = tasha->hph_mode;
	int ret = 0;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if ((!(strcmp(w->name, "ANC HPHR PA"))) &&
		    (test_bit(HPH_PA_DELAY, &tasha->status_mask))) {
			snd_soc_update_bits(codec, WCD9335_ANA_HPH, 0xC0, 0xC0);
		}
		set_bit(HPH_PA_DELAY, &tasha->status_mask);
		if (!(strcmp(w->name, "HPHR PA")))
			snd_soc_update_bits(codec, WCD9335_ANA_HPH, 0x40, 0x40);
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (!(strcmp(w->name, "ANC HPHR PA"))) {
			if ((snd_soc_read(codec, WCD9335_ANA_HPH) & 0xC0)
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
		 * HW requirement
		 */
		if (test_bit(HPH_PA_DELAY, &tasha->status_mask)) {
			usleep_range(7000, 7100);
			clear_bit(HPH_PA_DELAY, &tasha->status_mask);
		}
		tasha_codec_hph_post_pa_config(tasha, hph_mode, event);
		snd_soc_update_bits(codec, WCD9335_CDC_RX2_RX_PATH_CTL,
				    0x10, 0x00);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_read(codec, WCD9335_CDC_RX2_RX_PATH_MIX_CTL)) &
				  0x10)
			snd_soc_update_bits(codec,
					    WCD9335_CDC_RX2_RX_PATH_MIX_CTL,
					    0x10, 0x00);

		if (!(strcmp(w->name, "ANC HPHR PA"))) {
			/* Do everything needed for left channel */
			snd_soc_update_bits(codec, WCD9335_CDC_RX1_RX_PATH_CTL,
					    0x10, 0x00);
			/* Remove mix path mute if it is enabled */
			if ((snd_soc_read(codec,
					  WCD9335_CDC_RX1_RX_PATH_MIX_CTL)) &
					  0x10)
				snd_soc_update_bits(codec,
						WCD9335_CDC_RX1_RX_PATH_MIX_CTL,
						0x10, 0x00);
			/* Remove ANC Rx from reset */
			ret = tasha_codec_enable_anc(w, kcontrol, event);
		}
		tasha_codec_override(codec, hph_mode, event);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		blocking_notifier_call_chain(&tasha->notifier,
					WCD_EVENT_PRE_HPHR_PA_OFF,
					&tasha->mbhc);
		tasha_codec_hph_post_pa_config(tasha, hph_mode, event);
		if (!(strcmp(w->name, "ANC HPHR PA")) ||
		    !(strcmp(w->name, "HPHR PA")))
			snd_soc_update_bits(codec, WCD9335_ANA_HPH, 0x40, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 5ms sleep is required after PA is disabled as per
		 * HW requirement
		 */
		usleep_range(5000, 5500);
		tasha_codec_override(codec, hph_mode, event);
		blocking_notifier_call_chain(&tasha->notifier,
					WCD_EVENT_POST_HPHR_PA_OFF,
					&tasha->mbhc);

		if (!(strcmp(w->name, "ANC HPHR PA"))) {
			ret = tasha_codec_enable_anc(w, kcontrol, event);
			snd_soc_update_bits(codec,
				WCD9335_CDC_RX2_RX_PATH_CFG0, 0x10, 0x00);
		}
		break;
	};

	return ret;
}

static int tasha_codec_enable_hphl_pa(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int hph_mode = tasha->hph_mode;
	int ret = 0;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if ((!(strcmp(w->name, "ANC HPHL PA"))) &&
		    (test_bit(HPH_PA_DELAY, &tasha->status_mask))) {
			snd_soc_update_bits(codec, WCD9335_ANA_HPH, 0xC0, 0xC0);
		}
		if (!(strcmp(w->name, "HPHL PA")))
			snd_soc_update_bits(codec, WCD9335_ANA_HPH, 0x80, 0x80);
		set_bit(HPH_PA_DELAY, &tasha->status_mask);
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (!(strcmp(w->name, "ANC HPHL PA"))) {
			if ((snd_soc_read(codec, WCD9335_ANA_HPH) & 0xC0)
								!= 0xC0)
				/*
				 * If PA_EN is not set (potentially in ANC case)
				 * then do nothing for POST_PMU and let right
				 * channel handle everything.
				 */
				break;
		}
		/*
		 * 7ms sleep is required after PA is enabled as per
		 * HW requirement
		 */
		if (test_bit(HPH_PA_DELAY, &tasha->status_mask)) {
			usleep_range(7000, 7100);
			clear_bit(HPH_PA_DELAY, &tasha->status_mask);
		}

		tasha_codec_hph_post_pa_config(tasha, hph_mode, event);
		snd_soc_update_bits(codec, WCD9335_CDC_RX1_RX_PATH_CTL,
				    0x10, 0x00);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_read(codec, WCD9335_CDC_RX1_RX_PATH_MIX_CTL)) &
				  0x10)
			snd_soc_update_bits(codec,
					    WCD9335_CDC_RX1_RX_PATH_MIX_CTL,
					    0x10, 0x00);

		if (!(strcmp(w->name, "ANC HPHL PA"))) {
			/* Do everything needed for right channel */
			snd_soc_update_bits(codec, WCD9335_CDC_RX2_RX_PATH_CTL,
					    0x10, 0x00);
			/* Remove mix path mute if it is enabled */
			if ((snd_soc_read(codec,
					  WCD9335_CDC_RX2_RX_PATH_MIX_CTL)) &
					  0x10)
				snd_soc_update_bits(codec,
						WCD9335_CDC_RX2_RX_PATH_MIX_CTL,
						0x10, 0x00);

			/* Remove ANC Rx from reset */
			ret = tasha_codec_enable_anc(w, kcontrol, event);
		}
		tasha_codec_override(codec, hph_mode, event);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		blocking_notifier_call_chain(&tasha->notifier,
					WCD_EVENT_PRE_HPHL_PA_OFF,
					&tasha->mbhc);
		tasha_codec_hph_post_pa_config(tasha, hph_mode, event);
		if (!(strcmp(w->name, "ANC HPHL PA")) ||
		    !(strcmp(w->name, "HPHL PA")))
			snd_soc_update_bits(codec, WCD9335_ANA_HPH, 0x80, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 5ms sleep is required after PA is disabled as per
		 * HW requirement
		 */
		usleep_range(5000, 5500);
		tasha_codec_override(codec, hph_mode, event);
		blocking_notifier_call_chain(&tasha->notifier,
					WCD_EVENT_POST_HPHL_PA_OFF,
					&tasha->mbhc);

		if (!(strcmp(w->name, "ANC HPHL PA"))) {
			ret = tasha_codec_enable_anc(w, kcontrol, event);
			snd_soc_update_bits(codec,
				WCD9335_CDC_RX1_RX_PATH_CFG0, 0x10, 0x00);
		}
		break;
	};

	return ret;
}

static int tasha_codec_enable_lineout_pa(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 lineout_vol_reg = 0, lineout_mix_vol_reg = 0;
	int ret = 0;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	if (w->reg == WCD9335_ANA_LO_1_2) {
		if (w->shift == 7) {
			lineout_vol_reg = WCD9335_CDC_RX3_RX_PATH_CTL;
			lineout_mix_vol_reg = WCD9335_CDC_RX3_RX_PATH_MIX_CTL;
		} else if (w->shift == 6) {
			lineout_vol_reg = WCD9335_CDC_RX4_RX_PATH_CTL;
			lineout_mix_vol_reg = WCD9335_CDC_RX4_RX_PATH_MIX_CTL;
		}
	} else if (w->reg == WCD9335_ANA_LO_3_4) {
		if (w->shift == 7) {
			lineout_vol_reg = WCD9335_CDC_RX5_RX_PATH_CTL;
			lineout_mix_vol_reg = WCD9335_CDC_RX5_RX_PATH_MIX_CTL;
		} else if (w->shift == 6) {
			lineout_vol_reg = WCD9335_CDC_RX6_RX_PATH_CTL;
			lineout_mix_vol_reg = WCD9335_CDC_RX6_RX_PATH_MIX_CTL;
		}
	} else {
		dev_err(codec->dev, "%s: Error enabling lineout PA\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* 5ms sleep is required after PA is enabled as per
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
		if (!(strcmp(w->name, "ANC LINEOUT1 PA")) ||
		    !(strcmp(w->name, "ANC LINEOUT2 PA")))
			ret = tasha_codec_enable_anc(w, kcontrol, event);
		tasha_codec_override(codec, CLS_AB, event);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 5ms sleep is required after PA is disabled as per
		 * HW requirement
		 */
		usleep_range(5000, 5500);
		tasha_codec_override(codec, CLS_AB, event);
		if (!(strcmp(w->name, "ANC LINEOUT1 PA")) ||
			!(strcmp(w->name, "ANC LINEOUT2 PA"))) {
			ret = tasha_codec_enable_anc(w, kcontrol, event);
			if (!(strcmp(w->name, "ANC LINEOUT1 PA")))
				snd_soc_update_bits(codec,
				WCD9335_CDC_RX3_RX_PATH_CFG0, 0x10, 0x10);
			else
				snd_soc_update_bits(codec,
				WCD9335_CDC_RX4_RX_PATH_CFG0, 0x10, 0x10);
		}
		break;
	};

	return ret;
}

static void tasha_spk_anc_update_callback(struct work_struct *work)
{
	struct spk_anc_work *spk_anc_dwork;
	struct tasha_priv *tasha;
	struct delayed_work *delayed_work;
	struct snd_soc_codec *codec;

	delayed_work = to_delayed_work(work);
	spk_anc_dwork = container_of(delayed_work, struct spk_anc_work, dwork);
	tasha = spk_anc_dwork->tasha;
	codec = tasha->codec;

	snd_soc_update_bits(codec, WCD9335_CDC_RX7_RX_PATH_CFG0, 0x10, 0x10);
}

static int tasha_codec_enable_spk_anc(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d %d\n", __func__, w->name, event,
		tasha->anc_func);

	if (!tasha->anc_func)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = tasha_codec_enable_anc(w, kcontrol, event);
		schedule_delayed_work(&tasha->spk_anc_dwork.dwork,
				      msecs_to_jiffies(spk_anc_en_delay));
		break;
	case SND_SOC_DAPM_POST_PMD:
		cancel_delayed_work_sync(&tasha->spk_anc_dwork.dwork);
		snd_soc_update_bits(codec, WCD9335_CDC_RX7_RX_PATH_CFG0,
				    0x10, 0x00);
		ret = tasha_codec_enable_anc(w, kcontrol, event);
		break;
	}
	return ret;
}

static int tasha_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* 5ms sleep is required after PA is enabled as per
		 * HW requirement
		 */
		usleep_range(5000, 5500);
		snd_soc_update_bits(codec, WCD9335_CDC_RX0_RX_PATH_CTL,
				    0x10, 0x00);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_read(codec, WCD9335_CDC_RX0_RX_PATH_MIX_CTL)) &
		     0x10)
			snd_soc_update_bits(codec,
					    WCD9335_CDC_RX0_RX_PATH_MIX_CTL,
					    0x10, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 5ms sleep is required after PA is disabled as per
		 * HW requirement
		 */
		usleep_range(5000, 5500);

		if (!(strcmp(w->name, "ANC EAR PA"))) {
			ret = tasha_codec_enable_anc(w, kcontrol, event);
			snd_soc_update_bits(codec,
				WCD9335_CDC_RX0_RX_PATH_CFG0, 0x10, 0x00);
		}
		break;
	};

	return ret;
}

static void tasha_codec_hph_mode_gain_opt(struct snd_soc_codec *codec,
					  u8 gain)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u8 hph_l_en, hph_r_en;
	u8 l_val, r_val;
	u8 hph_pa_status;
	bool is_hphl_pa, is_hphr_pa;

	hph_pa_status = snd_soc_read(codec, WCD9335_ANA_HPH);
	is_hphl_pa = hph_pa_status >> 7;
	is_hphr_pa = (hph_pa_status & 0x40) >> 6;

	hph_l_en = snd_soc_read(codec, WCD9335_HPH_L_EN);
	hph_r_en = snd_soc_read(codec, WCD9335_HPH_R_EN);

	l_val = (hph_l_en & 0xC0) | 0x20 | gain;
	r_val = (hph_r_en & 0xC0) | 0x20 | gain;

	/*
	 * Set HPH_L & HPH_R gain source selection to REGISTER
	 * for better click and pop only if corresponding PAs are
	 * not enabled. Also cache the values of the HPHL/R
	 * PA gains to be applied after PAs are enabled
	 */
	if ((l_val != hph_l_en) && !is_hphl_pa) {
		snd_soc_write(codec, WCD9335_HPH_L_EN, l_val);
		tasha->hph_l_gain = hph_l_en & 0x1F;
	}

	if ((r_val != hph_r_en) && !is_hphr_pa) {
		snd_soc_write(codec, WCD9335_HPH_R_EN, r_val);
		tasha->hph_r_gain = hph_r_en & 0x1F;
	}
}

static void tasha_codec_hph_lohifi_config(struct snd_soc_codec *codec,
					  int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		snd_soc_update_bits(codec, WCD9335_RX_BIAS_HPH_PA, 0x0F, 0x06);
		snd_soc_update_bits(codec, WCD9335_RX_BIAS_HPH_RDACBUFF_CNP2,
				    0xF0, 0x40);
		snd_soc_update_bits(codec, WCD9335_HPH_CNP_WG_CTL, 0x07, 0x03);
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x08, 0x08);
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL1, 0x0E, 0x0C);
		tasha_codec_hph_mode_gain_opt(codec, 0x11);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x08, 0x00);
		snd_soc_update_bits(codec, WCD9335_HPH_CNP_WG_CTL, 0x07, 0x02);
		snd_soc_write(codec, WCD9335_RX_BIAS_HPH_RDACBUFF_CNP2, 0x8A);
		snd_soc_update_bits(codec, WCD9335_RX_BIAS_HPH_PA, 0x0F, 0x0A);
	}
}

static void tasha_codec_hph_lp_config(struct snd_soc_codec *codec,
				      int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL1, 0x0E, 0x0C);
		tasha_codec_hph_mode_gain_opt(codec, 0x10);
		snd_soc_update_bits(codec, WCD9335_HPH_CNP_WG_CTL, 0x07, 0x03);
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x08, 0x08);
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x04, 0x04);
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x20, 0x20);
		snd_soc_update_bits(codec, WCD9335_HPH_RDAC_LDO_CTL, 0x07,
				    0x01);
		snd_soc_update_bits(codec, WCD9335_HPH_RDAC_LDO_CTL, 0x70,
				    0x10);
		snd_soc_update_bits(codec, WCD9335_RX_BIAS_HPH_RDAC_LDO,
				    0x0F, 0x01);
		snd_soc_update_bits(codec, WCD9335_RX_BIAS_HPH_RDAC_LDO,
				    0xF0, 0x10);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_write(codec, WCD9335_RX_BIAS_HPH_RDAC_LDO, 0x88);
		snd_soc_write(codec, WCD9335_HPH_RDAC_LDO_CTL, 0x33);
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x20, 0x00);
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x04, 0x00);
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x08, 0x00);
		snd_soc_update_bits(codec, WCD9335_HPH_CNP_WG_CTL, 0x07, 0x02);
		snd_soc_update_bits(codec, WCD9335_HPH_R_EN, 0xC0, 0x80);
		snd_soc_update_bits(codec, WCD9335_HPH_L_EN, 0xC0, 0x80);
	}
}

static void tasha_codec_hph_hifi_config(struct snd_soc_codec *codec,
					int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		snd_soc_update_bits(codec, WCD9335_HPH_CNP_WG_CTL, 0x07, 0x03);
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x08, 0x08);
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL1, 0x0E, 0x0C);
		tasha_codec_hph_mode_gain_opt(codec, 0x11);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, WCD9335_HPH_PA_CTL2, 0x08, 0x00);
		snd_soc_update_bits(codec, WCD9335_HPH_CNP_WG_CTL, 0x07, 0x02);
	}
}

static void tasha_codec_hph_mode_config(struct snd_soc_codec *codec,
					int event, int mode)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	if (!TASHA_IS_2_0(tasha->wcd9xxx))
		return;

	switch (mode) {
	case CLS_H_LP:
		tasha_codec_hph_lp_config(codec, event);
		break;
	case CLS_H_LOHIFI:
		tasha_codec_hph_lohifi_config(codec, event);
		break;
	case CLS_H_HIFI:
		tasha_codec_hph_hifi_config(codec, event);
		break;
	}
}

static int tasha_codec_hphr_dac_event(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	int hph_mode = tasha->hph_mode;
	u8 dem_inp;
	int ret = 0;

	dev_dbg(codec->dev, "%s wname: %s event: %d hph_mode: %d\n", __func__,
		w->name, event, hph_mode);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (tasha->anc_func) {
			ret = tasha_codec_enable_anc(w, kcontrol, event);
			/* 40 msec delay is needed to avoid click and pop */
			msleep(40);
		}

		/* Read DEM INP Select */
		dem_inp = snd_soc_read(codec, WCD9335_CDC_RX2_RX_PATH_SEC0) &
			  0x03;
		if (((hph_mode == CLS_H_HIFI) || (hph_mode == CLS_H_LOHIFI) ||
		     (hph_mode == CLS_H_LP)) && (dem_inp != 0x01)) {
			dev_err(codec->dev, "%s: DEM Input not set correctly, hph_mode: %d\n",
					__func__, hph_mode);
			return -EINVAL;
		}
		wcd_clsh_fsm(codec, &tasha->clsh_d,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_HPHR,
			     ((hph_mode == CLS_H_LOHIFI) ?
			       CLS_H_HIFI : hph_mode));

		if (!(strcmp(w->name, "RX INT2 DAC")))
			snd_soc_update_bits(codec, WCD9335_ANA_HPH, 0x10, 0x10);

		tasha_codec_hph_mode_config(codec, event, hph_mode);

		if (tasha->anc_func)
			snd_soc_update_bits(codec,
				WCD9335_CDC_RX2_RX_PATH_CFG0, 0x10, 0x10);

		break;
	case SND_SOC_DAPM_POST_PMU:
		/* 1000us required as per HW requirement */
		usleep_range(1000, 1100);
		if ((hph_mode == CLS_H_LP) &&
		   (TASHA_IS_1_1(wcd9xxx))) {
			snd_soc_update_bits(codec, WCD9335_HPH_L_DAC_CTL,
					    0x03, 0x03);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if ((hph_mode == CLS_H_LP) &&
		   (TASHA_IS_1_1(wcd9xxx))) {
			snd_soc_update_bits(codec, WCD9335_HPH_L_DAC_CTL,
					    0x03, 0x00);
		}
		if (!(strcmp(w->name, "RX INT2 DAC")))
			snd_soc_update_bits(codec, WCD9335_ANA_HPH, 0x10, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 1000us required as per HW requirement */
		usleep_range(1000, 1100);

		if (!(wcd_clsh_get_clsh_state(&tasha->clsh_d) &
		     WCD_CLSH_STATE_HPHL))
			tasha_codec_hph_mode_config(codec, event, hph_mode);

		wcd_clsh_fsm(codec, &tasha->clsh_d,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_HPHR,
			     ((hph_mode == CLS_H_LOHIFI) ?
			       CLS_H_HIFI : hph_mode));
		break;
	};

	return ret;
}

static int tasha_codec_hphl_dac_event(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	int hph_mode = tasha->hph_mode;
	u8 dem_inp;
	int ret = 0;
	uint32_t impedl = 0, impedr = 0;

	dev_dbg(codec->dev, "%s wname: %s event: %d hph_mode: %d\n", __func__,
		w->name, event, hph_mode);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (tasha->anc_func) {
			ret = tasha_codec_enable_anc(w, kcontrol, event);
			/* 40 msec delay is needed to avoid click and pop */
			msleep(40);
		}

		/* Read DEM INP Select */
		dem_inp = snd_soc_read(codec, WCD9335_CDC_RX1_RX_PATH_SEC0) &
			  0x03;
		if (((hph_mode == CLS_H_HIFI) || (hph_mode == CLS_H_LOHIFI) ||
		     (hph_mode == CLS_H_LP)) && (dem_inp != 0x01)) {
			dev_err(codec->dev, "%s: DEM Input not set correctly, hph_mode: %d\n",
					__func__, hph_mode);
			return -EINVAL;
		}
		wcd_clsh_fsm(codec, &tasha->clsh_d,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_HPHL,
			     ((hph_mode == CLS_H_LOHIFI) ?
			       CLS_H_HIFI : hph_mode));

		if (!(strcmp(w->name, "RX INT1 DAC")))
			snd_soc_update_bits(codec, WCD9335_ANA_HPH, 0x20, 0x20);

		tasha_codec_hph_mode_config(codec, event, hph_mode);

		if (tasha->anc_func)
			snd_soc_update_bits(codec,
				WCD9335_CDC_RX1_RX_PATH_CFG0, 0x10, 0x10);

		ret = wcd_mbhc_get_impedance(&tasha->mbhc,
					&impedl, &impedr);
		if (!ret) {
			wcd_clsh_imped_config(codec, impedl, false);
			set_bit(CLASSH_CONFIG, &tasha->status_mask);
		} else {
			dev_dbg(codec->dev, "%s: Failed to get mbhc impedance %d\n",
						__func__, ret);
			ret = 0;
		}


		break;
	case SND_SOC_DAPM_POST_PMU:
		/* 1000us required as per HW requirement */
		usleep_range(1000, 1100);
		if ((hph_mode == CLS_H_LP) &&
		   (TASHA_IS_1_1(wcd9xxx))) {
			snd_soc_update_bits(codec, WCD9335_HPH_L_DAC_CTL,
					    0x03, 0x03);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (!(strcmp(w->name, "RX INT1 DAC")))
			snd_soc_update_bits(codec, WCD9335_ANA_HPH, 0x20, 0x00);
		if ((hph_mode == CLS_H_LP) &&
		   (TASHA_IS_1_1(wcd9xxx))) {
			snd_soc_update_bits(codec, WCD9335_HPH_L_DAC_CTL,
					    0x03, 0x00);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 1000us required as per HW requirement */
		usleep_range(1000, 1100);

		if (!(wcd_clsh_get_clsh_state(&tasha->clsh_d) &
		     WCD_CLSH_STATE_HPHR))
			tasha_codec_hph_mode_config(codec, event, hph_mode);
		wcd_clsh_fsm(codec, &tasha->clsh_d,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_HPHL,
			     ((hph_mode == CLS_H_LOHIFI) ?
			       CLS_H_HIFI : hph_mode));

		if (test_bit(CLASSH_CONFIG, &tasha->status_mask)) {
			wcd_clsh_imped_config(codec, impedl, true);
			clear_bit(CLASSH_CONFIG, &tasha->status_mask);
		} else
			dev_dbg(codec->dev, "%s: Failed to get mbhc impedance %d\n",
						__func__, ret);


		break;
	};

	return ret;
}

static int tasha_codec_lineout_dac_event(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (tasha->anc_func &&
			(!strcmp(w->name, "RX INT3 DAC") ||
				!strcmp(w->name, "RX INT4 DAC")))
			ret = tasha_codec_enable_anc(w, kcontrol, event);

		wcd_clsh_fsm(codec, &tasha->clsh_d,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_LO,
			     CLS_AB);

		if (tasha->anc_func) {
			if (!strcmp(w->name, "RX INT3 DAC"))
				snd_soc_update_bits(codec,
				WCD9335_CDC_RX3_RX_PATH_CFG0, 0x10, 0x10);
			else if (!strcmp(w->name, "RX INT4 DAC"))
				snd_soc_update_bits(codec,
				WCD9335_CDC_RX4_RX_PATH_CFG0, 0x10, 0x10);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd_clsh_fsm(codec, &tasha->clsh_d,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_LO,
			     CLS_AB);
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget tasha_dapm_i2s_widgets[] = {
	SND_SOC_DAPM_SUPPLY("RX_I2S_CTL", WCD9335_DATA_HUB_DATA_HUB_RX_I2S_CTL,
	0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("TX_I2S_CTL", WCD9335_DATA_HUB_DATA_HUB_TX_I2S_CTL,
	0, 0, NULL, 0),
};

static int tasha_codec_ear_dac_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (tasha->anc_func)
			ret = tasha_codec_enable_anc(w, kcontrol, event);

		wcd_clsh_fsm(codec, &tasha->clsh_d,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_EAR,
			     CLS_H_NORMAL);
		if (tasha->anc_func)
			snd_soc_update_bits(codec,
				WCD9335_CDC_RX0_RX_PATH_CFG0, 0x10, 0x10);

		break;
	case SND_SOC_DAPM_POST_PMU:
		break;
	case SND_SOC_DAPM_PRE_PMD:
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd_clsh_fsm(codec, &tasha->clsh_d,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_EAR,
			     CLS_H_NORMAL);
		break;
	};

	return ret;
}

static int tasha_codec_spk_boost_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 boost_path_ctl, boost_path_cfg1;
	u16 reg, reg_mix;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	if (!strcmp(w->name, "RX INT7 CHAIN")) {
		boost_path_ctl = WCD9335_CDC_BOOST0_BOOST_PATH_CTL;
		boost_path_cfg1 = WCD9335_CDC_RX7_RX_PATH_CFG1;
		reg = WCD9335_CDC_RX7_RX_PATH_CTL;
		reg_mix = WCD9335_CDC_RX7_RX_PATH_MIX_CTL;
	} else if (!strcmp(w->name, "RX INT8 CHAIN")) {
		boost_path_ctl = WCD9335_CDC_BOOST1_BOOST_PATH_CTL;
		boost_path_cfg1 = WCD9335_CDC_RX8_RX_PATH_CFG1;
		reg = WCD9335_CDC_RX8_RX_PATH_CTL;
		reg_mix = WCD9335_CDC_RX8_RX_PATH_MIX_CTL;
	} else {
		dev_err(codec->dev, "%s: unknown widget: %s\n",
			__func__, w->name);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, boost_path_ctl, 0x10, 0x10);
		snd_soc_update_bits(codec, boost_path_cfg1, 0x01, 0x01);
		snd_soc_update_bits(codec, reg, 0x10, 0x00);
		if ((snd_soc_read(codec, reg_mix)) & 0x10)
			snd_soc_update_bits(codec, reg_mix, 0x10, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, boost_path_cfg1, 0x01, 0x00);
		snd_soc_update_bits(codec, boost_path_ctl, 0x10, 0x00);
		break;
	};

	return 0;
}

static u16 tasha_interp_get_primary_reg(u16 reg, u16 *ind)
{
	u16 prim_int_reg = 0;

	switch (reg) {
	case WCD9335_CDC_RX0_RX_PATH_CTL:
	case WCD9335_CDC_RX0_RX_PATH_MIX_CTL:
		prim_int_reg = WCD9335_CDC_RX0_RX_PATH_CTL;
		*ind = 0;
		break;
	case WCD9335_CDC_RX1_RX_PATH_CTL:
	case WCD9335_CDC_RX1_RX_PATH_MIX_CTL:
		prim_int_reg = WCD9335_CDC_RX1_RX_PATH_CTL;
		*ind = 1;
		break;
	case WCD9335_CDC_RX2_RX_PATH_CTL:
	case WCD9335_CDC_RX2_RX_PATH_MIX_CTL:
		prim_int_reg = WCD9335_CDC_RX2_RX_PATH_CTL;
		*ind = 2;
		break;
	case WCD9335_CDC_RX3_RX_PATH_CTL:
	case WCD9335_CDC_RX3_RX_PATH_MIX_CTL:
		prim_int_reg = WCD9335_CDC_RX3_RX_PATH_CTL;
		*ind = 3;
		break;
	case WCD9335_CDC_RX4_RX_PATH_CTL:
	case WCD9335_CDC_RX4_RX_PATH_MIX_CTL:
		prim_int_reg = WCD9335_CDC_RX4_RX_PATH_CTL;
		*ind = 4;
		break;
	case WCD9335_CDC_RX5_RX_PATH_CTL:
	case WCD9335_CDC_RX5_RX_PATH_MIX_CTL:
		prim_int_reg = WCD9335_CDC_RX5_RX_PATH_CTL;
		*ind = 5;
		break;
	case WCD9335_CDC_RX6_RX_PATH_CTL:
	case WCD9335_CDC_RX6_RX_PATH_MIX_CTL:
		prim_int_reg = WCD9335_CDC_RX6_RX_PATH_CTL;
		*ind = 6;
		break;
	case WCD9335_CDC_RX7_RX_PATH_CTL:
	case WCD9335_CDC_RX7_RX_PATH_MIX_CTL:
		prim_int_reg = WCD9335_CDC_RX7_RX_PATH_CTL;
		*ind = 7;
		break;
	case WCD9335_CDC_RX8_RX_PATH_CTL:
	case WCD9335_CDC_RX8_RX_PATH_MIX_CTL:
		prim_int_reg = WCD9335_CDC_RX8_RX_PATH_CTL;
		*ind = 8;
		break;
	};

	return prim_int_reg;
}

static void tasha_codec_hd2_control(struct snd_soc_codec *codec,
				    u16 prim_int_reg, int event)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u16 hd2_scale_reg;
	u16 hd2_enable_reg = 0;

	if (!TASHA_IS_2_0(tasha->wcd9xxx))
		return;

	if (prim_int_reg == WCD9335_CDC_RX1_RX_PATH_CTL) {
		hd2_scale_reg = WCD9335_CDC_RX1_RX_PATH_SEC3;
		hd2_enable_reg = WCD9335_CDC_RX1_RX_PATH_CFG0;
	}
	if (prim_int_reg == WCD9335_CDC_RX2_RX_PATH_CTL) {
		hd2_scale_reg = WCD9335_CDC_RX2_RX_PATH_SEC3;
		hd2_enable_reg = WCD9335_CDC_RX2_RX_PATH_CFG0;
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

static int tasha_codec_enable_prim_interpolator(
				struct snd_soc_codec *codec,
				u16 reg, int event)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u16 prim_int_reg;
	u16 ind = 0;

	prim_int_reg = tasha_interp_get_primary_reg(reg, &ind);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tasha->prim_int_users[ind]++;
		if (tasha->prim_int_users[ind] == 1) {
			snd_soc_update_bits(codec, prim_int_reg,
					    0x10, 0x10);
			tasha_codec_hd2_control(codec, prim_int_reg, event);
			snd_soc_update_bits(codec, prim_int_reg,
					    1 << 0x5, 1 << 0x5);
		}
		if ((reg != prim_int_reg) &&
		    ((snd_soc_read(codec, prim_int_reg)) & 0x10))
			snd_soc_update_bits(codec, reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tasha->prim_int_users[ind]--;
		if (tasha->prim_int_users[ind] == 0) {
			snd_soc_update_bits(codec, prim_int_reg,
					1 << 0x5, 0 << 0x5);
			snd_soc_update_bits(codec, prim_int_reg,
					0x40, 0x40);
			snd_soc_update_bits(codec, prim_int_reg,
					0x40, 0x00);
			tasha_codec_hd2_control(codec, prim_int_reg, event);
		}
		break;
	};

	dev_dbg(codec->dev, "%s: primary interpolator: INT%d, users: %d\n",
		__func__, ind, tasha->prim_int_users[ind]);
	return 0;
}

static int tasha_codec_enable_spline_src(struct snd_soc_codec *codec,
					 int src_num,
					 int event)
{
	u16 src_paired_reg = 0;
	struct tasha_priv *tasha;
	u16 rx_path_cfg_reg = WCD9335_CDC_RX1_RX_PATH_CFG0;
	u16 rx_path_ctl_reg = WCD9335_CDC_RX1_RX_PATH_CTL;
	int *src_users, count, spl_src = SPLINE_SRC0;
	u16 src_clk_reg = WCD9335_SPLINE_SRC0_CLK_RST_CTL_0;

	tasha = snd_soc_codec_get_drvdata(codec);

	switch (src_num) {
	case SRC_IN_HPHL:
		rx_path_cfg_reg = WCD9335_CDC_RX1_RX_PATH_CFG0;
		src_clk_reg = WCD9335_SPLINE_SRC0_CLK_RST_CTL_0;
		src_paired_reg = WCD9335_SPLINE_SRC1_CLK_RST_CTL_0;
		rx_path_ctl_reg = WCD9335_CDC_RX1_RX_PATH_CTL;
		spl_src = SPLINE_SRC0;
		break;
	case SRC_IN_LO1:
		rx_path_cfg_reg = WCD9335_CDC_RX3_RX_PATH_CFG0;
		src_clk_reg = WCD9335_SPLINE_SRC0_CLK_RST_CTL_0;
		src_paired_reg = WCD9335_SPLINE_SRC1_CLK_RST_CTL_0;
		rx_path_ctl_reg = WCD9335_CDC_RX3_RX_PATH_CTL;
		spl_src = SPLINE_SRC0;
		break;
	case SRC_IN_HPHR:
		rx_path_cfg_reg = WCD9335_CDC_RX2_RX_PATH_CFG0;
		src_clk_reg = WCD9335_SPLINE_SRC1_CLK_RST_CTL_0;
		src_paired_reg = WCD9335_SPLINE_SRC0_CLK_RST_CTL_0;
		rx_path_ctl_reg = WCD9335_CDC_RX2_RX_PATH_CTL;
		spl_src = SPLINE_SRC1;
		break;
	case SRC_IN_LO2:
		rx_path_cfg_reg = WCD9335_CDC_RX4_RX_PATH_CFG0;
		src_clk_reg = WCD9335_SPLINE_SRC1_CLK_RST_CTL_0;
		src_paired_reg = WCD9335_SPLINE_SRC0_CLK_RST_CTL_0;
		rx_path_ctl_reg = WCD9335_CDC_RX4_RX_PATH_CTL;
		spl_src = SPLINE_SRC1;
		break;
	case SRC_IN_SPKRL:
		rx_path_cfg_reg = WCD9335_CDC_RX7_RX_PATH_CFG0;
		src_clk_reg = WCD9335_SPLINE_SRC2_CLK_RST_CTL_0;
		src_paired_reg = WCD9335_SPLINE_SRC3_CLK_RST_CTL_0;
		rx_path_ctl_reg = WCD9335_CDC_RX7_RX_PATH_CTL;
		spl_src = SPLINE_SRC2;
		break;
	case SRC_IN_LO3:
		rx_path_cfg_reg = WCD9335_CDC_RX5_RX_PATH_CFG0;
		src_clk_reg = WCD9335_SPLINE_SRC2_CLK_RST_CTL_0;
		src_paired_reg = WCD9335_SPLINE_SRC3_CLK_RST_CTL_0;
		rx_path_ctl_reg = WCD9335_CDC_RX5_RX_PATH_CTL;
		spl_src = SPLINE_SRC2;
		break;
	case SRC_IN_SPKRR:
		rx_path_cfg_reg = WCD9335_CDC_RX8_RX_PATH_CFG0;
		src_clk_reg = WCD9335_SPLINE_SRC3_CLK_RST_CTL_0;
		src_paired_reg = WCD9335_SPLINE_SRC2_CLK_RST_CTL_0;
		rx_path_ctl_reg = WCD9335_CDC_RX8_RX_PATH_CTL;
		spl_src = SPLINE_SRC3;
		break;
	case SRC_IN_LO4:
		rx_path_cfg_reg = WCD9335_CDC_RX6_RX_PATH_CFG0;
		src_clk_reg = WCD9335_SPLINE_SRC3_CLK_RST_CTL_0;
		src_paired_reg = WCD9335_SPLINE_SRC2_CLK_RST_CTL_0;
		rx_path_ctl_reg = WCD9335_CDC_RX6_RX_PATH_CTL;
		spl_src = SPLINE_SRC3;
		break;
	};

	src_users = &tasha->spl_src_users[spl_src];

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		count = *src_users;
		count++;
		if (count == 1) {
			if ((snd_soc_read(codec, src_clk_reg) & 0x02) ||
			    (snd_soc_read(codec, src_paired_reg) & 0x02)) {
				snd_soc_update_bits(codec, src_clk_reg, 0x02,
						    0x00);
				snd_soc_update_bits(codec, src_paired_reg,
						    0x02, 0x00);
			}
			snd_soc_update_bits(codec, src_clk_reg,	0x01, 0x01);
			snd_soc_update_bits(codec, rx_path_cfg_reg, 0x80,
					    0x80);
		}
		*src_users = count;
		break;
	case SND_SOC_DAPM_POST_PMD:
		count = *src_users;
		count--;
		if (count == 0) {
			snd_soc_update_bits(codec, rx_path_cfg_reg, 0x80,
					    0x00);
			snd_soc_update_bits(codec, src_clk_reg, 0x03, 0x02);
			/* default sample rate */
			snd_soc_update_bits(codec, rx_path_ctl_reg, 0x0f,
					    0x04);
		}
		*src_users = count;
		break;
	};

	dev_dbg(codec->dev, "%s: Spline SRC%d, users: %d\n",
		__func__, spl_src, *src_users);
	return 0;
}

static int tasha_codec_enable_spline_resampler(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;
	u8 src_in;

	src_in = snd_soc_read(codec, WCD9335_CDC_RX_INP_MUX_SPLINE_SRC_CFG0);
	if (!(src_in & 0xFF)) {
		dev_err(codec->dev, "%s: Spline SRC%u input not selected\n",
			__func__, w->shift);
		return -EINVAL;
	}

	switch (w->shift) {
	case SPLINE_SRC0:
		ret = tasha_codec_enable_spline_src(codec,
			((src_in & 0x03) == 1) ? SRC_IN_HPHL : SRC_IN_LO1,
			event);
		break;
	case SPLINE_SRC1:
		ret = tasha_codec_enable_spline_src(codec,
			((src_in & 0x0C) == 4) ? SRC_IN_HPHR : SRC_IN_LO2,
			event);
		break;
	case SPLINE_SRC2:
		ret = tasha_codec_enable_spline_src(codec,
			((src_in & 0x30) == 0x10) ? SRC_IN_LO3 : SRC_IN_SPKRL,
			event);
		break;
	case SPLINE_SRC3:
		ret = tasha_codec_enable_spline_src(codec,
			((src_in & 0xC0) == 0x40) ? SRC_IN_LO4 : SRC_IN_SPKRR,
			event);
		break;
	default:
		dev_err(codec->dev, "%s: Invalid spline src:%u\n", __func__,
			w->shift);
		ret = -EINVAL;
	};

	return ret;
}

static int tasha_codec_enable_swr(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha;
	int i, ch_cnt;

	tasha = snd_soc_codec_get_drvdata(codec);

	if (!tasha->nr)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if ((strnstr(w->name, "INT7_", sizeof("RX INT7_"))) &&
		    !tasha->rx_7_count)
			tasha->rx_7_count++;
		if ((strnstr(w->name, "INT8_", sizeof("RX INT8_"))) &&
		    !tasha->rx_8_count)
			tasha->rx_8_count++;
		ch_cnt = tasha->rx_7_count + tasha->rx_8_count;

		for (i = 0; i < tasha->nr; i++) {
			swrm_wcd_notify(tasha->swr_ctrl_data[i].swr_pdev,
					SWR_DEVICE_UP, NULL);
			swrm_wcd_notify(tasha->swr_ctrl_data[i].swr_pdev,
					SWR_SET_NUM_RX_CH, &ch_cnt);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if ((strnstr(w->name, "INT7_", sizeof("RX INT7_"))) &&
		    tasha->rx_7_count)
			tasha->rx_7_count--;
		if ((strnstr(w->name, "INT8_", sizeof("RX INT8_"))) &&
		    tasha->rx_8_count)
			tasha->rx_8_count--;
		ch_cnt = tasha->rx_7_count + tasha->rx_8_count;

		for (i = 0; i < tasha->nr; i++)
			swrm_wcd_notify(tasha->swr_ctrl_data[i].swr_pdev,
					SWR_SET_NUM_RX_CH, &ch_cnt);

		break;
	}
	dev_dbg(tasha->dev, "%s: current swr ch cnt: %d\n",
		__func__, tasha->rx_7_count + tasha->rx_8_count);

	return 0;
}

static int tasha_codec_config_ear_spkr_gain(struct snd_soc_codec *codec,
					    int event, int gain_reg)
{
	int comp_gain_offset, val;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	switch (tasha->spkr_mode) {
	/* Compander gain in SPKR_MODE1 case is 12 dB */
	case SPKR_MODE_1:
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
		if (tasha->comp_enabled[COMPANDER_7] &&
		    (gain_reg == WCD9335_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD9335_CDC_RX7_RX_VOL_MIX_CTL) &&
		    (tasha->ear_spkr_gain != 0)) {
			/* For example, val is -8(-12+5-1) for 4dB of gain */
			val = comp_gain_offset + tasha->ear_spkr_gain - 1;
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
		if (tasha->comp_enabled[COMPANDER_7] &&
		    (gain_reg == WCD9335_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD9335_CDC_RX7_RX_VOL_MIX_CTL) &&
		    (tasha->ear_spkr_gain != 0)) {
			snd_soc_write(codec, gain_reg, 0x0);

			dev_dbg(codec->dev, "%s: Reset RX7 Volume to 0 dB\n",
				__func__);
		}
		break;
	}

	return 0;
}

static int tasha_codec_enable_mix_path(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u16 gain_reg;
	int offset_val = 0;
	int val = 0;

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	switch (w->reg) {
	case WCD9335_CDC_RX0_RX_PATH_MIX_CTL:
		gain_reg = WCD9335_CDC_RX0_RX_VOL_MIX_CTL;
		break;
	case WCD9335_CDC_RX1_RX_PATH_MIX_CTL:
		gain_reg = WCD9335_CDC_RX1_RX_VOL_MIX_CTL;
		break;
	case WCD9335_CDC_RX2_RX_PATH_MIX_CTL:
		gain_reg = WCD9335_CDC_RX2_RX_VOL_MIX_CTL;
		break;
	case WCD9335_CDC_RX3_RX_PATH_MIX_CTL:
		gain_reg = WCD9335_CDC_RX3_RX_VOL_MIX_CTL;
		break;
	case WCD9335_CDC_RX4_RX_PATH_MIX_CTL:
		gain_reg = WCD9335_CDC_RX4_RX_VOL_MIX_CTL;
		break;
	case WCD9335_CDC_RX5_RX_PATH_MIX_CTL:
		gain_reg = WCD9335_CDC_RX5_RX_VOL_MIX_CTL;
		break;
	case WCD9335_CDC_RX6_RX_PATH_MIX_CTL:
		gain_reg = WCD9335_CDC_RX6_RX_VOL_MIX_CTL;
		break;
	case WCD9335_CDC_RX7_RX_PATH_MIX_CTL:
		gain_reg = WCD9335_CDC_RX7_RX_VOL_MIX_CTL;
		break;
	case WCD9335_CDC_RX8_RX_PATH_MIX_CTL:
		gain_reg = WCD9335_CDC_RX8_RX_VOL_MIX_CTL;
		break;
	default:
		dev_err(codec->dev, "%s: No gain register avail for %s\n",
			__func__, w->name);
		return 0;
	};

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if ((tasha->spkr_gain_offset == RX_GAIN_OFFSET_M1P5_DB) &&
		    (tasha->comp_enabled[COMPANDER_7] ||
		     tasha->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD9335_CDC_RX7_RX_VOL_MIX_CTL ||
		     gain_reg == WCD9335_CDC_RX8_RX_VOL_MIX_CTL)) {
			snd_soc_update_bits(codec, WCD9335_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD9335_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			snd_soc_update_bits(codec, WCD9335_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD9335_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			offset_val = -2;
		}
		val = snd_soc_read(codec, gain_reg);
		val += offset_val;
		snd_soc_write(codec, gain_reg, val);
		tasha_codec_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if ((tasha->spkr_gain_offset == RX_GAIN_OFFSET_M1P5_DB) &&
		    (tasha->comp_enabled[COMPANDER_7] ||
		     tasha->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD9335_CDC_RX7_RX_VOL_MIX_CTL ||
		     gain_reg == WCD9335_CDC_RX8_RX_VOL_MIX_CTL)) {
			snd_soc_update_bits(codec, WCD9335_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD9335_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			snd_soc_update_bits(codec, WCD9335_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD9335_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			offset_val = 2;
			val = snd_soc_read(codec, gain_reg);
			val += offset_val;
			snd_soc_write(codec, gain_reg, val);
		}
		tasha_codec_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	};

	return 0;
}

static int __tasha_cdc_native_clk_enable(struct tasha_priv *tasha,
					 bool enable)
{
	int ret = 0;
	struct snd_soc_codec *codec = tasha->codec;

	if (!tasha->wcd_native_clk) {
		dev_err(tasha->dev, "%s: wcd native clock is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(tasha->dev, "%s: native_clk_enable = %u\n", __func__, enable);

	if (enable) {
		ret = clk_prepare_enable(tasha->wcd_native_clk);
		if (ret) {
			dev_err(tasha->dev, "%s: native clk enable failed\n",
				__func__);
			goto err;
		}
		if (++tasha->native_clk_users == 1) {
			snd_soc_update_bits(codec, WCD9335_CLOCK_TEST_CTL,
					    0x10, 0x10);
			snd_soc_update_bits(codec, WCD9335_CLOCK_TEST_CTL,
					    0x80, 0x80);
			snd_soc_update_bits(codec, WCD9335_CODEC_RPM_CLK_GATE,
					    0x04, 0x00);
			snd_soc_update_bits(codec,
					WCD9335_CDC_CLK_RST_CTRL_MCLK_CONTROL,
					0x02, 0x02);
		}
	} else {
		if (tasha->native_clk_users &&
		    (--tasha->native_clk_users == 0)) {
			snd_soc_update_bits(codec,
					WCD9335_CDC_CLK_RST_CTRL_MCLK_CONTROL,
					0x02, 0x00);
			snd_soc_update_bits(codec, WCD9335_CODEC_RPM_CLK_GATE,
					    0x04, 0x04);
			snd_soc_update_bits(codec, WCD9335_CLOCK_TEST_CTL,
					    0x80, 0x00);
			snd_soc_update_bits(codec, WCD9335_CLOCK_TEST_CTL,
					    0x10, 0x00);
		}
		clk_disable_unprepare(tasha->wcd_native_clk);
	}

	dev_dbg(codec->dev, "%s: native_clk_users: %d\n", __func__,
		tasha->native_clk_users);
err:
	return ret;
}

static int tasha_codec_get_native_fifo_sync_mask(struct snd_soc_codec *codec,
						 int interp_n)
{
	int mask = 0;
	u16 reg;
	u8 val1, val2, inp0 = 0;
	u8 inp1 = 0, inp2 = 0;

	reg = WCD9335_CDC_RX_INP_MUX_RX_INT1_CFG0 + (2 * interp_n) - 2;

	val1 = snd_soc_read(codec, reg);
	val2 = snd_soc_read(codec, reg + 1);

	inp0 = val1 & 0x0F;
	inp1 = (val1 >> 4) & 0x0F;
	inp2 = (val2 >> 4) & 0x0F;

	if (IS_VALID_NATIVE_FIFO_PORT(inp0))
		mask |= (1 << (inp0 - 5));
	if (IS_VALID_NATIVE_FIFO_PORT(inp1))
		mask |= (1 << (inp1 - 5));
	if (IS_VALID_NATIVE_FIFO_PORT(inp2))
		mask |= (1 << (inp2 - 5));

	dev_dbg(codec->dev, "%s: native fifo mask: 0x%x\n", __func__, mask);
	if (!mask)
		dev_err(codec->dev, "native fifo err,int:%d,inp0:%d,inp1:%d,inp2:%d\n",
			interp_n, inp0, inp1, inp2);
	return mask;
}

static int tasha_enable_native_supply(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	int mask;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u16 interp_reg;

	dev_dbg(codec->dev, "%s: event: %d, shift:%d\n", __func__, event,
		w->shift);

	if (w->shift < INTERP_HPHL || w->shift > INTERP_LO2)
		return -EINVAL;

	interp_reg = WCD9335_CDC_RX1_RX_PATH_CTL + 20 * (w->shift - 1);

	mask = tasha_codec_get_native_fifo_sync_mask(codec, w->shift);
	if (!mask)
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Adjust interpolator rate to 44P1_NATIVE */
		snd_soc_update_bits(codec, interp_reg, 0x0F, 0x09);
		__tasha_cdc_native_clk_enable(tasha, true);
		snd_soc_update_bits(codec, WCD9335_DATA_HUB_NATIVE_FIFO_SYNC,
				    mask, mask);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, WCD9335_DATA_HUB_NATIVE_FIFO_SYNC,
				    mask, 0x0);
		__tasha_cdc_native_clk_enable(tasha, false);
		/* Adjust interpolator rate to default */
		snd_soc_update_bits(codec, interp_reg, 0x0F, 0x04);
		break;
	}

	return 0;
}

static int tasha_codec_enable_interpolator(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u16 gain_reg;
	u16 reg;
	int val;
	int offset_val = 0;

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	if (!(strcmp(w->name, "RX INT0 INTERP"))) {
		reg = WCD9335_CDC_RX0_RX_PATH_CTL;
		gain_reg = WCD9335_CDC_RX0_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT1 INTERP"))) {
		reg = WCD9335_CDC_RX1_RX_PATH_CTL;
		gain_reg = WCD9335_CDC_RX1_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT2 INTERP"))) {
		reg = WCD9335_CDC_RX2_RX_PATH_CTL;
		gain_reg = WCD9335_CDC_RX2_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT3 INTERP"))) {
		reg = WCD9335_CDC_RX3_RX_PATH_CTL;
		gain_reg = WCD9335_CDC_RX3_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT4 INTERP"))) {
		reg = WCD9335_CDC_RX4_RX_PATH_CTL;
		gain_reg = WCD9335_CDC_RX4_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT5 INTERP"))) {
		reg = WCD9335_CDC_RX5_RX_PATH_CTL;
		gain_reg = WCD9335_CDC_RX5_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT6 INTERP"))) {
		reg = WCD9335_CDC_RX6_RX_PATH_CTL;
		gain_reg = WCD9335_CDC_RX6_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT7 INTERP"))) {
		reg = WCD9335_CDC_RX7_RX_PATH_CTL;
		gain_reg = WCD9335_CDC_RX7_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT8 INTERP"))) {
		reg = WCD9335_CDC_RX8_RX_PATH_CTL;
		gain_reg = WCD9335_CDC_RX8_RX_VOL_CTL;
	} else {
		dev_err(codec->dev, "%s: Interpolator reg not found\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tasha_codec_vote_max_bw(codec, true);
		/* Reset if needed */
		tasha_codec_enable_prim_interpolator(codec, reg, event);
		break;
	case SND_SOC_DAPM_POST_PMU:
		tasha_config_compander(codec, w->shift, event);
		/* apply gain after int clk is enabled */
		if ((tasha->spkr_gain_offset == RX_GAIN_OFFSET_M1P5_DB) &&
		    (tasha->comp_enabled[COMPANDER_7] ||
		     tasha->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD9335_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD9335_CDC_RX8_RX_VOL_CTL)) {
			snd_soc_update_bits(codec, WCD9335_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD9335_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			snd_soc_update_bits(codec, WCD9335_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD9335_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			offset_val = -2;
		}
		val = snd_soc_read(codec, gain_reg);
		val += offset_val;
		snd_soc_write(codec, gain_reg, val);
		tasha_codec_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tasha_config_compander(codec, w->shift, event);
		tasha_codec_enable_prim_interpolator(codec, reg, event);
		if ((tasha->spkr_gain_offset == RX_GAIN_OFFSET_M1P5_DB) &&
		    (tasha->comp_enabled[COMPANDER_7] ||
		     tasha->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD9335_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD9335_CDC_RX8_RX_VOL_CTL)) {
			snd_soc_update_bits(codec, WCD9335_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD9335_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			snd_soc_update_bits(codec, WCD9335_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD9335_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			offset_val = 2;
			val = snd_soc_read(codec, gain_reg);
			val += offset_val;
			snd_soc_write(codec, gain_reg, val);
		}
		tasha_codec_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	};

	return 0;
}

static int tasha_codec_set_iir_gain(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU: /* fall through */
	case SND_SOC_DAPM_PRE_PMD:
		if (strnstr(w->name, "IIR0", sizeof("IIR0"))) {
			snd_soc_write(codec,
				WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL,
				snd_soc_read(codec,
				WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL));
			snd_soc_write(codec,
				WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL,
				snd_soc_read(codec,
				WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL));
			snd_soc_write(codec,
				WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL,
				snd_soc_read(codec,
				WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL));
			snd_soc_write(codec,
				WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL,
				snd_soc_read(codec,
				WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL));
		} else {
			snd_soc_write(codec,
				WCD9335_CDC_SIDETONE_IIR1_IIR_GAIN_B1_CTL,
				snd_soc_read(codec,
				WCD9335_CDC_SIDETONE_IIR1_IIR_GAIN_B1_CTL));
			snd_soc_write(codec,
				WCD9335_CDC_SIDETONE_IIR1_IIR_GAIN_B2_CTL,
				snd_soc_read(codec,
				WCD9335_CDC_SIDETONE_IIR1_IIR_GAIN_B2_CTL));
			snd_soc_write(codec,
				WCD9335_CDC_SIDETONE_IIR1_IIR_GAIN_B3_CTL,
				snd_soc_read(codec,
				WCD9335_CDC_SIDETONE_IIR1_IIR_GAIN_B3_CTL));
		}
		break;
	}
	return 0;
}

static int tasha_codec_enable_on_demand_supply(
	struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct on_demand_supply *supply;

	if (w->shift >= ON_DEMAND_SUPPLIES_MAX) {
		dev_err(codec->dev, "%s: error index > MAX Demand supplies",
			__func__);
		ret = -EINVAL;
		goto out;
	}

	dev_dbg(codec->dev, "%s: supply: %s event: %d\n",
		__func__, on_demand_supply_name[w->shift], event);

	supply = &tasha->on_demand_list[w->shift];
	WARN_ONCE(!supply->supply, "%s isn't defined\n",
		on_demand_supply_name[w->shift]);
	if (!supply->supply) {
		dev_err(codec->dev, "%s: err supply not present ond for %d",
			__func__, w->shift);
		goto out;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = regulator_enable(supply->supply);
		if (ret)
			dev_err(codec->dev, "%s: Failed to enable %s\n",
				__func__,
				on_demand_supply_name[w->shift]);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = regulator_disable(supply->supply);
		if (ret)
			dev_err(codec->dev, "%s: Failed to disable %s\n",
				__func__,
				on_demand_supply_name[w->shift]);
		break;
	default:
		break;
	};

out:
	return ret;
}

static int tasha_codec_find_amic_input(struct snd_soc_codec *codec,
				       int adc_mux_n)
{
	u16 mask, shift, adc_mux_in_reg;
	u16 amic_mux_sel_reg;
	bool is_amic;

	if (adc_mux_n < 0 || adc_mux_n > WCD9335_MAX_VALID_ADC_MUX ||
	    adc_mux_n == WCD9335_INVALID_ADC_MUX)
		return 0;

	/* Check whether adc mux input is AMIC or DMIC */
	if (adc_mux_n < 4) {
		adc_mux_in_reg = WCD9335_CDC_TX_INP_MUX_ADC_MUX0_CFG1 +
				 2 * adc_mux_n;
		amic_mux_sel_reg = WCD9335_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
				   2 * adc_mux_n;
		mask = 0x03;
		shift = 0;
	} else {
		adc_mux_in_reg = WCD9335_CDC_TX_INP_MUX_ADC_MUX4_CFG0 +
				 adc_mux_n - 4;
		amic_mux_sel_reg = adc_mux_in_reg;
		mask = 0xC0;
		shift = 6;
	}
	is_amic = (((snd_soc_read(codec, adc_mux_in_reg) & mask) >> shift)
		    == 1);
	if (!is_amic)
		return 0;

	return snd_soc_read(codec, amic_mux_sel_reg) & 0x07;
}

static void tasha_codec_set_tx_hold(struct snd_soc_codec *codec,
				    u16 amic_reg, bool set)
{
	u8 mask = 0x20;
	u8 val;

	if (amic_reg == WCD9335_ANA_AMIC1 ||
	    amic_reg == WCD9335_ANA_AMIC3 ||
	    amic_reg == WCD9335_ANA_AMIC5)
		mask = 0x40;

	val = set ? mask : 0x00;

	switch (amic_reg) {
	case WCD9335_ANA_AMIC1:
	case WCD9335_ANA_AMIC2:
		snd_soc_update_bits(codec, WCD9335_ANA_AMIC2, mask, val);
		break;
	case WCD9335_ANA_AMIC3:
	case WCD9335_ANA_AMIC4:
		snd_soc_update_bits(codec, WCD9335_ANA_AMIC4, mask, val);
		break;
	case WCD9335_ANA_AMIC5:
	case WCD9335_ANA_AMIC6:
		snd_soc_update_bits(codec, WCD9335_ANA_AMIC6, mask, val);
		break;
	default:
		dev_dbg(codec->dev, "%s: invalid amic: %d\n",
			__func__, amic_reg);
		break;
	}
}

static int tasha_codec_tx_adc_cfg(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	int adc_mux_n = w->shift;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int amic_n;

	dev_dbg(codec->dev, "%s: event: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		amic_n = tasha_codec_find_amic_input(codec, adc_mux_n);
		if (amic_n) {
			/*
			 * Prevent ANC Rx pop by leaving Tx FE in HOLD
			 * state until PA is up. Track AMIC being used
			 * so we can release the HOLD later.
			 */
			set_bit(ANC_MIC_AMIC1 + amic_n - 1,
				&tasha->status_mask);
		}
		break;
	default:
		break;
	}

	return 0;
}

static u16 tasha_codec_get_amic_pwlvl_reg(struct snd_soc_codec *codec, int amic)
{
	u16 pwr_level_reg = 0;

	switch (amic) {
	case 1:
	case 2:
		pwr_level_reg = WCD9335_ANA_AMIC1;
		break;

	case 3:
	case 4:
		pwr_level_reg = WCD9335_ANA_AMIC3;
		break;

	case 5:
	case 6:
		pwr_level_reg = WCD9335_ANA_AMIC5;
		break;
	default:
		dev_dbg(codec->dev, "%s: invalid amic: %d\n",
			__func__, amic);
		break;
	}

	return pwr_level_reg;
}

#define  TX_HPF_CUT_OFF_FREQ_MASK	0x60
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

static void tasha_tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct tasha_priv *tasha;
	struct snd_soc_codec *codec;
	u16 dec_cfg_reg, amic_reg;
	u8 hpf_cut_off_freq;
	int amic_n;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	tasha = hpf_work->tasha;
	codec = tasha->codec;
	hpf_cut_off_freq = hpf_work->hpf_cut_off_freq;

	dec_cfg_reg = WCD9335_CDC_TX0_TX_PATH_CFG0 + 16 * hpf_work->decimator;

	dev_dbg(codec->dev, "%s: decimator %u hpf_cut_of_freq 0x%x\n",
		__func__, hpf_work->decimator, hpf_cut_off_freq);

	amic_n = tasha_codec_find_amic_input(codec, hpf_work->decimator);
	if (amic_n) {
		amic_reg = WCD9335_ANA_AMIC1 + amic_n - 1;
		tasha_codec_set_tx_hold(codec, amic_reg, false);
	}
	tasha_codec_vote_max_bw(codec, true);
	snd_soc_update_bits(codec, dec_cfg_reg, TX_HPF_CUT_OFF_FREQ_MASK,
			    hpf_cut_off_freq << 5);
	tasha_codec_vote_max_bw(codec, false);
}

static void tasha_tx_mute_update_callback(struct work_struct *work)
{
	struct tx_mute_work *tx_mute_dwork;
	struct tasha_priv *tasha;
	struct delayed_work *delayed_work;
	struct snd_soc_codec *codec;
	u16 tx_vol_ctl_reg, hpf_gate_reg;

	delayed_work = to_delayed_work(work);
	tx_mute_dwork = container_of(delayed_work, struct tx_mute_work, dwork);
	tasha = tx_mute_dwork->tasha;
	codec = tasha->codec;

	tx_vol_ctl_reg = WCD9335_CDC_TX0_TX_PATH_CTL +
					16 * tx_mute_dwork->decimator;
	hpf_gate_reg = WCD9335_CDC_TX0_TX_PATH_SEC2 +
					16 * tx_mute_dwork->decimator;
	snd_soc_update_bits(codec, hpf_gate_reg, 0x01, 0x01);
	snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x00);
}

static int tasha_codec_enable_dec(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	unsigned int decimator;
	char *dec_adc_mux_name = NULL;
	char *widget_name = NULL;
	char *wname;
	int ret = 0, amic_n;
	u16 tx_vol_ctl_reg, pwr_level_reg = 0, dec_cfg_reg, hpf_gate_reg;
	u16 tx_gain_ctl_reg;
	char *dec;
	u8 hpf_cut_off_freq;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

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

	tx_vol_ctl_reg = WCD9335_CDC_TX0_TX_PATH_CTL + 16 * decimator;
	hpf_gate_reg = WCD9335_CDC_TX0_TX_PATH_SEC2 + 16 * decimator;
	dec_cfg_reg = WCD9335_CDC_TX0_TX_PATH_CFG0 + 16 * decimator;
	tx_gain_ctl_reg = WCD9335_CDC_TX0_TX_VOL_CTL + 16 * decimator;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		amic_n = tasha_codec_find_amic_input(codec, decimator);
		if (amic_n)
			pwr_level_reg = tasha_codec_get_amic_pwlvl_reg(codec,
								       amic_n);

		if (pwr_level_reg) {
			switch ((snd_soc_read(codec, pwr_level_reg) &
					      WCD9335_AMIC_PWR_LVL_MASK) >>
					      WCD9335_AMIC_PWR_LVL_SHIFT) {
			case WCD9335_AMIC_PWR_LEVEL_LP:
				snd_soc_update_bits(codec, dec_cfg_reg,
						    WCD9335_DEC_PWR_LVL_MASK,
						    WCD9335_DEC_PWR_LVL_LP);
				break;

			case WCD9335_AMIC_PWR_LEVEL_HP:
				snd_soc_update_bits(codec, dec_cfg_reg,
						    WCD9335_DEC_PWR_LVL_MASK,
						    WCD9335_DEC_PWR_LVL_HP);
				break;
			case WCD9335_AMIC_PWR_LEVEL_DEFAULT:
			default:
				snd_soc_update_bits(codec, dec_cfg_reg,
						    WCD9335_DEC_PWR_LVL_MASK,
						    WCD9335_DEC_PWR_LVL_DF);
				break;
			}
		}
		hpf_cut_off_freq = (snd_soc_read(codec, dec_cfg_reg) &
				   TX_HPF_CUT_OFF_FREQ_MASK) >> 5;
		tasha->tx_hpf_work[decimator].hpf_cut_off_freq =
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

		if (decimator == 0) {
			snd_soc_write(codec, WCD9335_MBHC_ZDET_RAMP_CTL, 0x83);
			snd_soc_write(codec, WCD9335_MBHC_ZDET_RAMP_CTL, 0xA3);
			snd_soc_write(codec, WCD9335_MBHC_ZDET_RAMP_CTL, 0x83);
			snd_soc_write(codec, WCD9335_MBHC_ZDET_RAMP_CTL, 0x03);
		}
		/* schedule work queue to Remove Mute */
		schedule_delayed_work(&tasha->tx_mute_dwork[decimator].dwork,
				      msecs_to_jiffies(tx_unmute_delay));
		if (tasha->tx_hpf_work[decimator].hpf_cut_off_freq !=
							CF_MIN_3DB_150HZ)
			schedule_delayed_work(
					&tasha->tx_hpf_work[decimator].dwork,
					msecs_to_jiffies(300));
		/* apply gain after decimator is enabled */
		snd_soc_write(codec, tx_gain_ctl_reg,
			      snd_soc_read(codec, tx_gain_ctl_reg));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		hpf_cut_off_freq =
			tasha->tx_hpf_work[decimator].hpf_cut_off_freq;
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x10);
		if (cancel_delayed_work_sync(
		    &tasha->tx_hpf_work[decimator].dwork)) {
			if (hpf_cut_off_freq != CF_MIN_3DB_150HZ) {
				tasha_codec_vote_max_bw(codec, true);
				snd_soc_update_bits(codec, dec_cfg_reg,
						    TX_HPF_CUT_OFF_FREQ_MASK,
						    hpf_cut_off_freq << 5);
				tasha_codec_vote_max_bw(codec, false);
			}
		}
		cancel_delayed_work_sync(
				&tasha->tx_mute_dwork[decimator].dwork);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x00);
		break;
	};
out:
	kfree(wname);
	return ret;
}

static u32 tasha_get_dmic_sample_rate(struct snd_soc_codec *codec,
				unsigned int dmic, struct wcd9xxx_pdata *pdata)
{
	u8 tx_stream_fs;
	u8 adc_mux_index = 0, adc_mux_sel = 0;
	bool dec_found = false;
	u16 adc_mux_ctl_reg, tx_fs_reg;
	u32 dmic_fs;

	while (dec_found == 0 && adc_mux_index < WCD9335_MAX_VALID_ADC_MUX) {
		if (adc_mux_index < 4) {
			adc_mux_ctl_reg = WCD9335_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
						(adc_mux_index * 2);
			adc_mux_sel = ((snd_soc_read(codec, adc_mux_ctl_reg) &
						0x78) >> 3) - 1;
		} else if (adc_mux_index < 9) {
			adc_mux_ctl_reg = WCD9335_CDC_TX_INP_MUX_ADC_MUX4_CFG0 +
						((adc_mux_index - 4) * 1);
			adc_mux_sel = ((snd_soc_read(codec, adc_mux_ctl_reg) &
						0x38) >> 3) - 1;
		} else if (adc_mux_index == 9) {
			++adc_mux_index;
			continue;
		}
		if (adc_mux_sel == dmic)
			dec_found = true;
		else
			++adc_mux_index;
	}

	if (dec_found == true && adc_mux_index <= 8) {
		tx_fs_reg = WCD9335_CDC_TX0_TX_PATH_CTL + (16 * adc_mux_index);
		tx_stream_fs = snd_soc_read(codec, tx_fs_reg) & 0x0F;
		dmic_fs = tx_stream_fs <= 4 ? WCD9XXX_DMIC_SAMPLE_RATE_2P4MHZ :
					WCD9XXX_DMIC_SAMPLE_RATE_4P8MHZ;

		/*
		 * Check for ECPP path selection and DEC1 not connected to
		 * any other audio path to apply ECPP DMIC sample rate
		 */
		if ((adc_mux_index == 1) &&
		    ((snd_soc_read(codec, WCD9335_CPE_SS_US_EC_MUX_CFG)
				   & 0x0F) == 0x0A) &&
		    ((snd_soc_read(codec, WCD9335_CDC_IF_ROUTER_TX_MUX_CFG0)
				   & 0x0C) == 0x00)) {
			dmic_fs = pdata->ecpp_dmic_sample_rate;
		}
	} else {
		dmic_fs = pdata->dmic_sample_rate;
	}

	return dmic_fs;
}

static u8 tasha_get_dmic_clk_val(struct snd_soc_codec *codec,
				 u32 mclk_rate, u32 dmic_clk_rate)
{
	u32 div_factor;
	u8 dmic_ctl_val;

	dev_dbg(codec->dev,
		"%s: mclk_rate = %d, dmic_sample_rate = %d\n",
		__func__, mclk_rate, dmic_clk_rate);

	/* Default value to return in case of error */
	if (mclk_rate == TASHA_MCLK_CLK_9P6MHZ)
		dmic_ctl_val = WCD9335_DMIC_CLK_DIV_2;
	else
		dmic_ctl_val = WCD9335_DMIC_CLK_DIV_3;

	if (dmic_clk_rate == 0) {
		dev_err(codec->dev,
			"%s: dmic_sample_rate cannot be 0\n",
			__func__);
		goto done;
	}

	div_factor = mclk_rate / dmic_clk_rate;
	switch (div_factor) {
	case 2:
		dmic_ctl_val = WCD9335_DMIC_CLK_DIV_2;
		break;
	case 3:
		dmic_ctl_val = WCD9335_DMIC_CLK_DIV_3;
		break;
	case 4:
		dmic_ctl_val = WCD9335_DMIC_CLK_DIV_4;
		break;
	case 6:
		dmic_ctl_val = WCD9335_DMIC_CLK_DIV_6;
		break;
	case 8:
		dmic_ctl_val = WCD9335_DMIC_CLK_DIV_8;
		break;
	case 16:
		dmic_ctl_val = WCD9335_DMIC_CLK_DIV_16;
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

static int tasha_codec_enable_adc(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s: event:%d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tasha_codec_set_tx_hold(codec, w->reg, true);
		break;
	default:
		break;
	}

	return 0;
}

static int tasha_codec_enable_dmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
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
		dmic_clk_cnt = &(tasha->dmic_0_1_clk_cnt);
		dmic_clk_reg = WCD9335_CPE_SS_DMIC0_CTL;
		break;
	case 2:
	case 3:
		dmic_clk_cnt = &(tasha->dmic_2_3_clk_cnt);
		dmic_clk_reg = WCD9335_CPE_SS_DMIC1_CTL;
		break;
	case 4:
	case 5:
		dmic_clk_cnt = &(tasha->dmic_4_5_clk_cnt);
		dmic_clk_reg = WCD9335_CPE_SS_DMIC2_CTL;
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
		dmic_sample_rate = tasha_get_dmic_sample_rate(codec, dmic,
						pdata);
		dmic_rate_val =
			tasha_get_dmic_clk_val(codec,
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
			tasha_get_dmic_clk_val(codec,
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

static int __tasha_codec_enable_micbias(struct snd_soc_dapm_widget *w,
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
		tasha_micbias_control(codec, micb_num, MICB_ENABLE, true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* wait for cnp time */
		usleep_range(1000, 1100);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tasha_micbias_control(codec, micb_num, MICB_DISABLE, true);
		break;
	};

	return 0;
}

static int tasha_codec_ldo_h_control(struct snd_soc_dapm_widget *w,
				     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		tasha->ldo_h_users++;

		if (tasha->ldo_h_users == 1)
			snd_soc_update_bits(codec, WCD9335_LDOH_MODE,
					    0x80, 0x80);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		tasha->ldo_h_users--;

		if (tasha->ldo_h_users < 0)
			tasha->ldo_h_users = 0;

		if (tasha->ldo_h_users == 0)
			snd_soc_update_bits(codec, WCD9335_LDOH_MODE,
					    0x80, 0x00);
	}

	return 0;
}

static int tasha_codec_force_enable_ldo_h(struct snd_soc_dapm_widget *w,
					  struct snd_kcontrol *kcontrol,
					  int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd_resmgr_enable_master_bias(tasha->resmgr);
		tasha_codec_ldo_h_control(w, event);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tasha_codec_ldo_h_control(w, event);
		wcd_resmgr_disable_master_bias(tasha->resmgr);
		break;
	}

	return 0;
}

static int tasha_codec_force_enable_micbias(struct snd_soc_dapm_widget *w,
					    struct snd_kcontrol *kcontrol,
					    int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd_resmgr_enable_master_bias(tasha->resmgr);
		tasha_cdc_mclk_enable(codec, true, true);
		ret = __tasha_codec_enable_micbias(w, SND_SOC_DAPM_PRE_PMU);
		/* Wait for 1ms for better cnp */
		usleep_range(1000, 1100);
		tasha_cdc_mclk_enable(codec, false, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = __tasha_codec_enable_micbias(w, SND_SOC_DAPM_POST_PMD);
		wcd_resmgr_disable_master_bias(tasha->resmgr);
		break;
	}

	return ret;
}

static int tasha_codec_enable_micbias(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	return __tasha_codec_enable_micbias(w, event);
}

static int tasha_codec_enable_standalone_ldo_h(struct snd_soc_codec *codec,
					       bool enable)
{
	int rc;

	if (enable)
		rc = snd_soc_dapm_force_enable_pin(
					snd_soc_codec_get_dapm(codec),
					DAPM_LDO_H_STANDALONE);
	else
		rc = snd_soc_dapm_disable_pin(
					snd_soc_codec_get_dapm(codec),
					DAPM_LDO_H_STANDALONE);

	if (!rc)
		snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));
	else
		dev_err(codec->dev, "%s: ldo_h force %s pin failed\n",
			__func__, (enable ? "enable" : "disable"));

	return rc;
}

/*
 * tasha_codec_enable_standalone_micbias - enable micbias standalone
 * @codec: pointer to codec instance
 * @micb_num: number of micbias to be enabled
 * @enable: true to enable micbias or false to disable
 *
 * This function is used to enable micbias (1, 2, 3 or 4) during
 * standalone independent of whether TX use-case is running or not
 *
 * Return: error code in case of failure or 0 for success
 */
int tasha_codec_enable_standalone_micbias(struct snd_soc_codec *codec,
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

	if ((micb_index < 0) || (micb_index > TASHA_MAX_MICBIAS - 1)) {
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
EXPORT_SYMBOL(tasha_codec_enable_standalone_micbias);

static const char *const tasha_anc_func_text[] = {"OFF", "ON"};
static const struct soc_enum tasha_anc_func_enum =
		SOC_ENUM_SINGLE_EXT(2, tasha_anc_func_text);

static const char *const tasha_clkmode_text[] = {"EXTERNAL", "INTERNAL"};
static SOC_ENUM_SINGLE_EXT_DECL(tasha_clkmode_enum, tasha_clkmode_text);

/* Cutoff frequency for high pass filter */
static const char * const cf_text[] = {
	"CF_NEG_3DB_4HZ", "CF_NEG_3DB_75HZ", "CF_NEG_3DB_150HZ"
};

static const char * const rx_cf_text[] = {
	"CF_NEG_3DB_4HZ", "CF_NEG_3DB_75HZ", "CF_NEG_3DB_150HZ",
	"CF_NEG_3DB_0P48HZ"
};

static const struct soc_enum cf_dec0_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX0_TX_PATH_CFG0, 5, 3, cf_text);

static const struct soc_enum cf_dec1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX1_TX_PATH_CFG0, 5, 3, cf_text);

static const struct soc_enum cf_dec2_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX2_TX_PATH_CFG0, 5, 3, cf_text);

static const struct soc_enum cf_dec3_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX3_TX_PATH_CFG0, 5, 3, cf_text);

static const struct soc_enum cf_dec4_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX4_TX_PATH_CFG0, 5, 3, cf_text);

static const struct soc_enum cf_dec5_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX5_TX_PATH_CFG0, 5, 3, cf_text);

static const struct soc_enum cf_dec6_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX6_TX_PATH_CFG0, 5, 3, cf_text);

static const struct soc_enum cf_dec7_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX7_TX_PATH_CFG0, 5, 3, cf_text);

static const struct soc_enum cf_dec8_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX8_TX_PATH_CFG0, 5, 3, cf_text);

static const struct soc_enum cf_int0_1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX0_RX_PATH_CFG2, 0, 4, rx_cf_text);

static SOC_ENUM_SINGLE_DECL(cf_int0_2_enum, WCD9335_CDC_RX0_RX_PATH_MIX_CFG, 2,
		     rx_cf_text);

static const struct soc_enum cf_int1_1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX1_RX_PATH_CFG2, 0, 4, rx_cf_text);

static SOC_ENUM_SINGLE_DECL(cf_int1_2_enum, WCD9335_CDC_RX1_RX_PATH_MIX_CFG, 2,
		     rx_cf_text);

static const struct soc_enum cf_int2_1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX2_RX_PATH_CFG2, 0, 4, rx_cf_text);

static SOC_ENUM_SINGLE_DECL(cf_int2_2_enum, WCD9335_CDC_RX2_RX_PATH_MIX_CFG, 2,
		     rx_cf_text);

static const struct soc_enum cf_int3_1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX3_RX_PATH_CFG2, 0, 4, rx_cf_text);

static SOC_ENUM_SINGLE_DECL(cf_int3_2_enum, WCD9335_CDC_RX3_RX_PATH_MIX_CFG, 2,
		     rx_cf_text);

static const struct soc_enum cf_int4_1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX4_RX_PATH_CFG2, 0, 4, rx_cf_text);

static SOC_ENUM_SINGLE_DECL(cf_int4_2_enum, WCD9335_CDC_RX4_RX_PATH_MIX_CFG, 2,
		     rx_cf_text);

static const struct soc_enum cf_int5_1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX5_RX_PATH_CFG2, 0, 4, rx_cf_text);

static SOC_ENUM_SINGLE_DECL(cf_int5_2_enum, WCD9335_CDC_RX5_RX_PATH_MIX_CFG, 2,
		     rx_cf_text);

static const struct soc_enum cf_int6_1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX6_RX_PATH_CFG2, 0, 4, rx_cf_text);

static SOC_ENUM_SINGLE_DECL(cf_int6_2_enum, WCD9335_CDC_RX6_RX_PATH_MIX_CFG, 2,
		     rx_cf_text);

static const struct soc_enum cf_int7_1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX7_RX_PATH_CFG2, 0, 4, rx_cf_text);

static SOC_ENUM_SINGLE_DECL(cf_int7_2_enum, WCD9335_CDC_RX7_RX_PATH_MIX_CFG, 2,
		     rx_cf_text);

static const struct soc_enum cf_int8_1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX8_RX_PATH_CFG2, 0, 4, rx_cf_text);

static SOC_ENUM_SINGLE_DECL(cf_int8_2_enum, WCD9335_CDC_RX8_RX_PATH_MIX_CFG, 2,
		     rx_cf_text);

static const struct snd_soc_dapm_route audio_i2s_map[] = {
	{"SLIM RX0 MUX", NULL, "RX_I2S_CTL"},
	{"SLIM RX1 MUX", NULL, "RX_I2S_CTL"},
	{"SLIM RX2 MUX", NULL, "RX_I2S_CTL"},
	{"SLIM RX3 MUX", NULL, "RX_I2S_CTL"},

	{"SLIM TX6 MUX", NULL, "TX_I2S_CTL"},
	{"SLIM TX7 MUX", NULL, "TX_I2S_CTL"},
	{"SLIM TX8 MUX", NULL, "TX_I2S_CTL"},
	{"SLIM TX11 MUX", NULL, "TX_I2S_CTL"},
};

static const struct snd_soc_dapm_route audio_map[] = {

	/* MAD */
	{"MAD_SEL MUX", "SPE", "MAD_CPE_INPUT"},
	{"MAD_SEL MUX", "MSM", "MADINPUT"},
	{"MADONOFF", "Switch", "MAD_SEL MUX"},
	{"MAD_BROADCAST", "Switch", "MAD_SEL MUX"},
	{"TX13 INP MUX", "CPE_TX_PP", "MADONOFF"},

	/* CPE HW MAD bypass */
	{"CPE IN Mixer", "MAD_BYPASS", "SLIM TX1 MUX"},

	{"AIF4_MAD Mixer", "SLIM TX1", "CPE IN Mixer"},
	{"AIF4_MAD Mixer", "SLIM TX12", "MADONOFF"},
	{"AIF4_MAD Mixer", "SLIM TX13", "TX13 INP MUX"},
	{"AIF4 MAD", NULL, "AIF4_MAD Mixer"},
	{"AIF4 MAD", NULL, "AIF4"},

	{"EC BUF MUX INP", "DEC1", "ADC MUX1"},
	{"AIF5 CPE", NULL, "EC BUF MUX INP"},

	/* SLIMBUS Connections */
	{"AIF1 CAP", NULL, "AIF1_CAP Mixer"},
	{"AIF2 CAP", NULL, "AIF2_CAP Mixer"},
	{"AIF3 CAP", NULL, "AIF3_CAP Mixer"},

	/* VI Feedback */
	{"AIF4_VI Mixer", "SPKR_VI_1", "VIINPUT"},
	{"AIF4_VI Mixer", "SPKR_VI_2", "VIINPUT"},
	{"AIF4 VI", NULL, "AIF4_VI Mixer"},

	/* SLIM_MIXER("AIF1_CAP Mixer"),*/
	{"AIF1_CAP Mixer", "SLIM TX0", "SLIM TX0 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX1", "SLIM TX1 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX2", "SLIM TX2 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX3", "SLIM TX3 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX4", "SLIM TX4 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX5", "SLIM TX5 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX6", "SLIM TX6 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX7", "SLIM TX7 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX8", "SLIM TX8 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX9", "SLIM TX9 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX10", "SLIM TX10 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX11", "SLIM TX11 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX13", "TX13 INP MUX"},
	/* SLIM_MIXER("AIF2_CAP Mixer"),*/
	{"AIF2_CAP Mixer", "SLIM TX0", "SLIM TX0 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX1", "SLIM TX1 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX2", "SLIM TX2 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX3", "SLIM TX3 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX4", "SLIM TX4 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX5", "SLIM TX5 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX6", "SLIM TX6 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX7", "SLIM TX7 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX8", "SLIM TX8 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX9", "SLIM TX9 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX10", "SLIM TX10 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX11", "SLIM TX11 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX13", "TX13 INP MUX"},
	/* SLIM_MIXER("AIF3_CAP Mixer"),*/
	{"AIF3_CAP Mixer", "SLIM TX0", "SLIM TX0 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX1", "SLIM TX1 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX2", "SLIM TX2 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX3", "SLIM TX3 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX4", "SLIM TX4 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX5", "SLIM TX5 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX6", "SLIM TX6 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX7", "SLIM TX7 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX8", "SLIM TX8 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX9", "SLIM TX9 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX10", "SLIM TX10 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX11", "SLIM TX11 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX13", "TX13 INP MUX"},

	{"SLIM TX0 MUX", "DEC0", "ADC MUX0"},
	{"SLIM TX0 MUX", "RX_MIX_TX0", "RX MIX TX0 MUX"},
	{"SLIM TX0 MUX", "DEC0_192", "ADC US MUX0"},

	{"SLIM TX1 MUX", "DEC1", "ADC MUX1"},
	{"SLIM TX1 MUX", "RX_MIX_TX1", "RX MIX TX1 MUX"},
	{"SLIM TX1 MUX", "DEC1_192", "ADC US MUX1"},

	{"SLIM TX2 MUX", "DEC2", "ADC MUX2"},
	{"SLIM TX2 MUX", "RX_MIX_TX2", "RX MIX TX2 MUX"},
	{"SLIM TX2 MUX", "DEC2_192", "ADC US MUX2"},

	{"SLIM TX3 MUX", "DEC3", "ADC MUX3"},
	{"SLIM TX3 MUX", "RX_MIX_TX3", "RX MIX TX3 MUX"},
	{"SLIM TX3 MUX", "DEC3_192", "ADC US MUX3"},

	{"SLIM TX4 MUX", "DEC4", "ADC MUX4"},
	{"SLIM TX4 MUX", "RX_MIX_TX4", "RX MIX TX4 MUX"},
	{"SLIM TX4 MUX", "DEC4_192", "ADC US MUX4"},

	{"SLIM TX5 MUX", "DEC5", "ADC MUX5"},
	{"SLIM TX5 MUX", "RX_MIX_TX5", "RX MIX TX5 MUX"},
	{"SLIM TX5 MUX", "DEC5_192", "ADC US MUX5"},

	{"SLIM TX6 MUX", "DEC6", "ADC MUX6"},
	{"SLIM TX6 MUX", "RX_MIX_TX6", "RX MIX TX6 MUX"},
	{"SLIM TX6 MUX", "DEC6_192", "ADC US MUX6"},

	{"SLIM TX7 MUX", "DEC7", "ADC MUX7"},
	{"SLIM TX7 MUX", "RX_MIX_TX7", "RX MIX TX7 MUX"},
	{"SLIM TX7 MUX", "DEC7_192", "ADC US MUX7"},

	{"SLIM TX8 MUX", "DEC8", "ADC MUX8"},
	{"SLIM TX8 MUX", "RX_MIX_TX8", "RX MIX TX8 MUX"},
	{"SLIM TX8 MUX", "DEC8_192", "ADC US MUX8"},

	{"SLIM TX9 MUX", "DEC7", "ADC MUX7"},
	{"SLIM TX9 MUX", "DEC7_192", "ADC US MUX7"},
	{"SLIM TX10 MUX", "DEC6", "ADC MUX6"},
	{"SLIM TX10 MUX", "DEC6_192", "ADC US MUX6"},

	{"SLIM TX11 MUX", "DEC_0_5", "SLIM TX11 INP1 MUX"},
	{"SLIM TX11 MUX", "DEC_9_12", "SLIM TX11 INP1 MUX"},
	{"SLIM TX11 INP1 MUX", "DEC0", "ADC MUX0"},
	{"SLIM TX11 INP1 MUX", "DEC1", "ADC MUX1"},
	{"SLIM TX11 INP1 MUX", "DEC2", "ADC MUX2"},
	{"SLIM TX11 INP1 MUX", "DEC3", "ADC MUX3"},
	{"SLIM TX11 INP1 MUX", "DEC4", "ADC MUX4"},
	{"SLIM TX11 INP1 MUX", "DEC5", "ADC MUX5"},
	{"SLIM TX11 INP1 MUX", "RX_MIX_TX5", "RX MIX TX5 MUX"},

	{"TX13 INP MUX", "MAD_BRDCST", "MAD_BROADCAST"},
	{"TX13 INP MUX", "CDC_DEC_5", "SLIM TX13 MUX"},
	{"SLIM TX13 MUX", "DEC5", "ADC MUX5"},

	{"RX MIX TX0 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX0 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX0 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX MIX TX0 MUX", "RX_MIX3", "RX INT3 SEC MIX"},
	{"RX MIX TX0 MUX", "RX_MIX4", "RX INT4 SEC MIX"},
	{"RX MIX TX0 MUX", "RX_MIX5", "RX INT5 SEC MIX"},
	{"RX MIX TX0 MUX", "RX_MIX6", "RX INT6 SEC MIX"},
	{"RX MIX TX0 MUX", "RX_MIX7", "RX INT7 SEC MIX"},
	{"RX MIX TX0 MUX", "RX_MIX8", "RX INT8 SEC MIX"},
	{"RX MIX TX0 MUX", "RX_MIX_VBAT5", "RX INT5 VBAT"},
	{"RX MIX TX0 MUX", "RX_MIX_VBAT6", "RX INT6 VBAT"},
	{"RX MIX TX0 MUX", "RX_MIX_VBAT7", "RX INT7 VBAT"},
	{"RX MIX TX0 MUX", "RX_MIX_VBAT8", "RX INT8 VBAT"},

	{"RX MIX TX1 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX3", "RX INT3 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX4", "RX INT4 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX5", "RX INT5 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX6", "RX INT6 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX7", "RX INT7 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX8", "RX INT8 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX_VBAT5", "RX INT5 VBAT"},
	{"RX MIX TX1 MUX", "RX_MIX_VBAT6", "RX INT6 VBAT"},
	{"RX MIX TX1 MUX", "RX_MIX_VBAT7", "RX INT7 VBAT"},
	{"RX MIX TX1 MUX", "RX_MIX_VBAT8", "RX INT8 VBAT"},

	{"RX MIX TX2 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX3", "RX INT3 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX4", "RX INT4 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX5", "RX INT5 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX6", "RX INT6 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX7", "RX INT7 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX8", "RX INT8 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX_VBAT5", "RX INT5 VBAT"},
	{"RX MIX TX2 MUX", "RX_MIX_VBAT6", "RX INT6 VBAT"},
	{"RX MIX TX2 MUX", "RX_MIX_VBAT7", "RX INT7 VBAT"},
	{"RX MIX TX2 MUX", "RX_MIX_VBAT8", "RX INT8 VBAT"},

	{"RX MIX TX3 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX3 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX3 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX MIX TX3 MUX", "RX_MIX3", "RX INT3 SEC MIX"},
	{"RX MIX TX3 MUX", "RX_MIX4", "RX INT4 SEC MIX"},
	{"RX MIX TX3 MUX", "RX_MIX5", "RX INT5 SEC MIX"},
	{"RX MIX TX3 MUX", "RX_MIX6", "RX INT6 SEC MIX"},
	{"RX MIX TX3 MUX", "RX_MIX7", "RX INT7 SEC MIX"},
	{"RX MIX TX3 MUX", "RX_MIX8", "RX INT8 SEC MIX"},
	{"RX MIX TX3 MUX", "RX_MIX_VBAT5", "RX INT5 VBAT"},
	{"RX MIX TX3 MUX", "RX_MIX_VBAT6", "RX INT6 VBAT"},
	{"RX MIX TX3 MUX", "RX_MIX_VBAT7", "RX INT7 VBAT"},
	{"RX MIX TX3 MUX", "RX_MIX_VBAT8", "RX INT8 VBAT"},

	{"RX MIX TX4 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX4 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX4 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX MIX TX4 MUX", "RX_MIX3", "RX INT3 SEC MIX"},
	{"RX MIX TX4 MUX", "RX_MIX4", "RX INT4 SEC MIX"},
	{"RX MIX TX4 MUX", "RX_MIX5", "RX INT5 SEC MIX"},
	{"RX MIX TX4 MUX", "RX_MIX6", "RX INT6 SEC MIX"},
	{"RX MIX TX4 MUX", "RX_MIX7", "RX INT7 SEC MIX"},
	{"RX MIX TX4 MUX", "RX_MIX8", "RX INT8 SEC MIX"},
	{"RX MIX TX4 MUX", "RX_MIX_VBAT5", "RX INT5 VBAT"},
	{"RX MIX TX4 MUX", "RX_MIX_VBAT6", "RX INT6 VBAT"},
	{"RX MIX TX4 MUX", "RX_MIX_VBAT7", "RX INT7 VBAT"},
	{"RX MIX TX4 MUX", "RX_MIX_VBAT8", "RX INT8 VBAT"},

	{"RX MIX TX5 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX5 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX5 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX MIX TX5 MUX", "RX_MIX3", "RX INT3 SEC MIX"},
	{"RX MIX TX5 MUX", "RX_MIX4", "RX INT4 SEC MIX"},
	{"RX MIX TX5 MUX", "RX_MIX5", "RX INT5 SEC MIX"},
	{"RX MIX TX5 MUX", "RX_MIX6", "RX INT6 SEC MIX"},
	{"RX MIX TX5 MUX", "RX_MIX7", "RX INT7 SEC MIX"},
	{"RX MIX TX5 MUX", "RX_MIX8", "RX INT8 SEC MIX"},
	{"RX MIX TX5 MUX", "RX_MIX_VBAT5", "RX INT5 VBAT"},
	{"RX MIX TX5 MUX", "RX_MIX_VBAT6", "RX INT6 VBAT"},
	{"RX MIX TX5 MUX", "RX_MIX_VBAT7", "RX INT7 VBAT"},
	{"RX MIX TX5 MUX", "RX_MIX_VBAT8", "RX INT8 VBAT"},

	{"RX MIX TX6 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX6 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX6 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX MIX TX6 MUX", "RX_MIX3", "RX INT3 SEC MIX"},
	{"RX MIX TX6 MUX", "RX_MIX4", "RX INT4 SEC MIX"},
	{"RX MIX TX6 MUX", "RX_MIX5", "RX INT5 SEC MIX"},
	{"RX MIX TX6 MUX", "RX_MIX6", "RX INT6 SEC MIX"},
	{"RX MIX TX6 MUX", "RX_MIX7", "RX INT7 SEC MIX"},
	{"RX MIX TX6 MUX", "RX_MIX8", "RX INT8 SEC MIX"},
	{"RX MIX TX6 MUX", "RX_MIX_VBAT5", "RX INT5 VBAT"},
	{"RX MIX TX6 MUX", "RX_MIX_VBAT6", "RX INT6 VBAT"},
	{"RX MIX TX6 MUX", "RX_MIX_VBAT7", "RX INT7 VBAT"},
	{"RX MIX TX6 MUX", "RX_MIX_VBAT8", "RX INT8 VBAT"},

	{"RX MIX TX7 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX7 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX7 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX MIX TX7 MUX", "RX_MIX3", "RX INT3 SEC MIX"},
	{"RX MIX TX7 MUX", "RX_MIX4", "RX INT4 SEC MIX"},
	{"RX MIX TX7 MUX", "RX_MIX5", "RX INT5 SEC MIX"},
	{"RX MIX TX7 MUX", "RX_MIX6", "RX INT6 SEC MIX"},
	{"RX MIX TX7 MUX", "RX_MIX7", "RX INT7 SEC MIX"},
	{"RX MIX TX7 MUX", "RX_MIX8", "RX INT8 SEC MIX"},
	{"RX MIX TX7 MUX", "RX_MIX_VBAT5", "RX INT5 VBAT"},
	{"RX MIX TX7 MUX", "RX_MIX_VBAT6", "RX INT6 VBAT"},
	{"RX MIX TX7 MUX", "RX_MIX_VBAT7", "RX INT7 VBAT"},
	{"RX MIX TX7 MUX", "RX_MIX_VBAT8", "RX INT8 VBAT"},

	{"RX MIX TX8 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX8 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX8 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX MIX TX8 MUX", "RX_MIX3", "RX INT3 SEC MIX"},
	{"RX MIX TX8 MUX", "RX_MIX4", "RX INT4 SEC MIX"},
	{"RX MIX TX8 MUX", "RX_MIX5", "RX INT5 SEC MIX"},
	{"RX MIX TX8 MUX", "RX_MIX6", "RX INT6 SEC MIX"},
	{"RX MIX TX8 MUX", "RX_MIX7", "RX INT7 SEC MIX"},
	{"RX MIX TX8 MUX", "RX_MIX8", "RX INT8 SEC MIX"},
	{"RX MIX TX8 MUX", "RX_MIX_VBAT5", "RX INT5 VBAT"},
	{"RX MIX TX8 MUX", "RX_MIX_VBAT6", "RX INT6 VBAT"},
	{"RX MIX TX8 MUX", "RX_MIX_VBAT7", "RX INT7 VBAT"},
	{"RX MIX TX8 MUX", "RX_MIX_VBAT8", "RX INT8 VBAT"},

	{"ADC US MUX0", "US_Switch", "ADC MUX0"},
	{"ADC US MUX1", "US_Switch", "ADC MUX1"},
	{"ADC US MUX2", "US_Switch", "ADC MUX2"},
	{"ADC US MUX3", "US_Switch", "ADC MUX3"},
	{"ADC US MUX4", "US_Switch", "ADC MUX4"},
	{"ADC US MUX5", "US_Switch", "ADC MUX5"},
	{"ADC US MUX6", "US_Switch", "ADC MUX6"},
	{"ADC US MUX7", "US_Switch", "ADC MUX7"},
	{"ADC US MUX8", "US_Switch", "ADC MUX8"},
	{"ADC MUX0", "DMIC", "DMIC MUX0"},
	{"ADC MUX0", "AMIC", "AMIC MUX0"},
	{"ADC MUX1", "DMIC", "DMIC MUX1"},
	{"ADC MUX1", "AMIC", "AMIC MUX1"},
	{"ADC MUX2", "DMIC", "DMIC MUX2"},
	{"ADC MUX2", "AMIC", "AMIC MUX2"},
	{"ADC MUX3", "DMIC", "DMIC MUX3"},
	{"ADC MUX3", "AMIC", "AMIC MUX3"},
	{"ADC MUX4", "DMIC", "DMIC MUX4"},
	{"ADC MUX4", "AMIC", "AMIC MUX4"},
	{"ADC MUX5", "DMIC", "DMIC MUX5"},
	{"ADC MUX5", "AMIC", "AMIC MUX5"},
	{"ADC MUX6", "DMIC", "DMIC MUX6"},
	{"ADC MUX6", "AMIC", "AMIC MUX6"},
	{"ADC MUX7", "DMIC", "DMIC MUX7"},
	{"ADC MUX7", "AMIC", "AMIC MUX7"},
	{"ADC MUX8", "DMIC", "DMIC MUX8"},
	{"ADC MUX8", "AMIC", "AMIC MUX8"},
	{"ADC MUX10", "DMIC", "DMIC MUX10"},
	{"ADC MUX10", "AMIC", "AMIC MUX10"},
	{"ADC MUX11", "DMIC", "DMIC MUX11"},
	{"ADC MUX11", "AMIC", "AMIC MUX11"},
	{"ADC MUX12", "DMIC", "DMIC MUX12"},
	{"ADC MUX12", "AMIC", "AMIC MUX12"},
	{"ADC MUX13", "DMIC", "DMIC MUX13"},
	{"ADC MUX13", "AMIC", "AMIC MUX13"},

	{"ADC MUX0", "ANC_FB_TUNE1", "ADC MUX10"},
	{"ADC MUX0", "ANC_FB_TUNE1", "ADC MUX11"},
	{"ADC MUX0", "ANC_FB_TUNE2", "ADC MUX12"},
	{"ADC MUX0", "ANC_FB_TUNE2", "ADC MUX13"},
	{"ADC MUX1", "ANC_FB_TUNE1", "ADC MUX10"},
	{"ADC MUX1", "ANC_FB_TUNE1", "ADC MUX11"},
	{"ADC MUX1", "ANC_FB_TUNE2", "ADC MUX12"},
	{"ADC MUX1", "ANC_FB_TUNE2", "ADC MUX13"},
	{"ADC MUX2", "ANC_FB_TUNE1", "ADC MUX10"},
	{"ADC MUX2", "ANC_FB_TUNE1", "ADC MUX11"},
	{"ADC MUX2", "ANC_FB_TUNE2", "ADC MUX12"},
	{"ADC MUX2", "ANC_FB_TUNE2", "ADC MUX13"},
	{"ADC MUX3", "ANC_FB_TUNE1", "ADC MUX10"},
	{"ADC MUX3", "ANC_FB_TUNE1", "ADC MUX11"},
	{"ADC MUX3", "ANC_FB_TUNE2", "ADC MUX12"},
	{"ADC MUX3", "ANC_FB_TUNE2", "ADC MUX13"},
	{"ADC MUX4", "ANC_FB_TUNE1", "ADC MUX10"},
	{"ADC MUX4", "ANC_FB_TUNE1", "ADC MUX11"},
	{"ADC MUX4", "ANC_FB_TUNE2", "ADC MUX12"},
	{"ADC MUX4", "ANC_FB_TUNE2", "ADC MUX13"},
	{"ADC MUX5", "ANC_FB_TUNE1", "ADC MUX10"},
	{"ADC MUX5", "ANC_FB_TUNE1", "ADC MUX11"},
	{"ADC MUX5", "ANC_FB_TUNE2", "ADC MUX12"},
	{"ADC MUX5", "ANC_FB_TUNE2", "ADC MUX13"},
	{"ADC MUX6", "ANC_FB_TUNE1", "ADC MUX10"},
	{"ADC MUX6", "ANC_FB_TUNE1", "ADC MUX11"},
	{"ADC MUX6", "ANC_FB_TUNE2", "ADC MUX12"},
	{"ADC MUX6", "ANC_FB_TUNE2", "ADC MUX13"},
	{"ADC MUX7", "ANC_FB_TUNE1", "ADC MUX10"},
	{"ADC MUX7", "ANC_FB_TUNE1", "ADC MUX11"},
	{"ADC MUX7", "ANC_FB_TUNE2", "ADC MUX12"},
	{"ADC MUX7", "ANC_FB_TUNE2", "ADC MUX13"},
	{"ADC MUX8", "ANC_FB_TUNE1", "ADC MUX10"},
	{"ADC MUX8", "ANC_FB_TUNE1", "ADC MUX11"},
	{"ADC MUX8", "ANC_FB_TUNE2", "ADC MUX12"},
	{"ADC MUX8", "ANC_FB_TUNE2", "ADC MUX13"},

	{"DMIC MUX0", "DMIC0", "DMIC0"},
	{"DMIC MUX0", "DMIC1", "DMIC1"},
	{"DMIC MUX0", "DMIC2", "DMIC2"},
	{"DMIC MUX0", "DMIC3", "DMIC3"},
	{"DMIC MUX0", "DMIC4", "DMIC4"},
	{"DMIC MUX0", "DMIC5", "DMIC5"},
	{"AMIC MUX0", "ADC1", "ADC1"},
	{"AMIC MUX0", "ADC2", "ADC2"},
	{"AMIC MUX0", "ADC3", "ADC3"},
	{"AMIC MUX0", "ADC4", "ADC4"},
	{"AMIC MUX0", "ADC5", "ADC5"},
	{"AMIC MUX0", "ADC6", "ADC6"},

	{"DMIC MUX1", "DMIC0", "DMIC0"},
	{"DMIC MUX1", "DMIC1", "DMIC1"},
	{"DMIC MUX1", "DMIC2", "DMIC2"},
	{"DMIC MUX1", "DMIC3", "DMIC3"},
	{"DMIC MUX1", "DMIC4", "DMIC4"},
	{"DMIC MUX1", "DMIC5", "DMIC5"},
	{"AMIC MUX1", "ADC1", "ADC1"},
	{"AMIC MUX1", "ADC2", "ADC2"},
	{"AMIC MUX1", "ADC3", "ADC3"},
	{"AMIC MUX1", "ADC4", "ADC4"},
	{"AMIC MUX1", "ADC5", "ADC5"},
	{"AMIC MUX1", "ADC6", "ADC6"},

	{"DMIC MUX2", "DMIC0", "DMIC0"},
	{"DMIC MUX2", "DMIC1", "DMIC1"},
	{"DMIC MUX2", "DMIC2", "DMIC2"},
	{"DMIC MUX2", "DMIC3", "DMIC3"},
	{"DMIC MUX2", "DMIC4", "DMIC4"},
	{"DMIC MUX2", "DMIC5", "DMIC5"},
	{"AMIC MUX2", "ADC1", "ADC1"},
	{"AMIC MUX2", "ADC2", "ADC2"},
	{"AMIC MUX2", "ADC3", "ADC3"},
	{"AMIC MUX2", "ADC4", "ADC4"},
	{"AMIC MUX2", "ADC5", "ADC5"},
	{"AMIC MUX2", "ADC6", "ADC6"},

	{"DMIC MUX3", "DMIC0", "DMIC0"},
	{"DMIC MUX3", "DMIC1", "DMIC1"},
	{"DMIC MUX3", "DMIC2", "DMIC2"},
	{"DMIC MUX3", "DMIC3", "DMIC3"},
	{"DMIC MUX3", "DMIC4", "DMIC4"},
	{"DMIC MUX3", "DMIC5", "DMIC5"},
	{"AMIC MUX3", "ADC1", "ADC1"},
	{"AMIC MUX3", "ADC2", "ADC2"},
	{"AMIC MUX3", "ADC3", "ADC3"},
	{"AMIC MUX3", "ADC4", "ADC4"},
	{"AMIC MUX3", "ADC5", "ADC5"},
	{"AMIC MUX3", "ADC6", "ADC6"},

	{"DMIC MUX4", "DMIC0", "DMIC0"},
	{"DMIC MUX4", "DMIC1", "DMIC1"},
	{"DMIC MUX4", "DMIC2", "DMIC2"},
	{"DMIC MUX4", "DMIC3", "DMIC3"},
	{"DMIC MUX4", "DMIC4", "DMIC4"},
	{"DMIC MUX4", "DMIC5", "DMIC5"},
	{"AMIC MUX4", "ADC1", "ADC1"},
	{"AMIC MUX4", "ADC2", "ADC2"},
	{"AMIC MUX4", "ADC3", "ADC3"},
	{"AMIC MUX4", "ADC4", "ADC4"},
	{"AMIC MUX4", "ADC5", "ADC5"},
	{"AMIC MUX4", "ADC6", "ADC6"},

	{"DMIC MUX5", "DMIC0", "DMIC0"},
	{"DMIC MUX5", "DMIC1", "DMIC1"},
	{"DMIC MUX5", "DMIC2", "DMIC2"},
	{"DMIC MUX5", "DMIC3", "DMIC3"},
	{"DMIC MUX5", "DMIC4", "DMIC4"},
	{"DMIC MUX5", "DMIC5", "DMIC5"},
	{"AMIC MUX5", "ADC1", "ADC1"},
	{"AMIC MUX5", "ADC2", "ADC2"},
	{"AMIC MUX5", "ADC3", "ADC3"},
	{"AMIC MUX5", "ADC4", "ADC4"},
	{"AMIC MUX5", "ADC5", "ADC5"},
	{"AMIC MUX5", "ADC6", "ADC6"},

	{"DMIC MUX6", "DMIC0", "DMIC0"},
	{"DMIC MUX6", "DMIC1", "DMIC1"},
	{"DMIC MUX6", "DMIC2", "DMIC2"},
	{"DMIC MUX6", "DMIC3", "DMIC3"},
	{"DMIC MUX6", "DMIC4", "DMIC4"},
	{"DMIC MUX6", "DMIC5", "DMIC5"},
	{"AMIC MUX6", "ADC1", "ADC1"},
	{"AMIC MUX6", "ADC2", "ADC2"},
	{"AMIC MUX6", "ADC3", "ADC3"},
	{"AMIC MUX6", "ADC4", "ADC4"},
	{"AMIC MUX6", "ADC5", "ADC5"},
	{"AMIC MUX6", "ADC6", "ADC6"},

	{"DMIC MUX7", "DMIC0", "DMIC0"},
	{"DMIC MUX7", "DMIC1", "DMIC1"},
	{"DMIC MUX7", "DMIC2", "DMIC2"},
	{"DMIC MUX7", "DMIC3", "DMIC3"},
	{"DMIC MUX7", "DMIC4", "DMIC4"},
	{"DMIC MUX7", "DMIC5", "DMIC5"},
	{"AMIC MUX7", "ADC1", "ADC1"},
	{"AMIC MUX7", "ADC2", "ADC2"},
	{"AMIC MUX7", "ADC3", "ADC3"},
	{"AMIC MUX7", "ADC4", "ADC4"},
	{"AMIC MUX7", "ADC5", "ADC5"},
	{"AMIC MUX7", "ADC6", "ADC6"},

	{"DMIC MUX8", "DMIC0", "DMIC0"},
	{"DMIC MUX8", "DMIC1", "DMIC1"},
	{"DMIC MUX8", "DMIC2", "DMIC2"},
	{"DMIC MUX8", "DMIC3", "DMIC3"},
	{"DMIC MUX8", "DMIC4", "DMIC4"},
	{"DMIC MUX8", "DMIC5", "DMIC5"},
	{"AMIC MUX8", "ADC1", "ADC1"},
	{"AMIC MUX8", "ADC2", "ADC2"},
	{"AMIC MUX8", "ADC3", "ADC3"},
	{"AMIC MUX8", "ADC4", "ADC4"},
	{"AMIC MUX8", "ADC5", "ADC5"},
	{"AMIC MUX8", "ADC6", "ADC6"},

	{"DMIC MUX10", "DMIC0", "DMIC0"},
	{"DMIC MUX10", "DMIC1", "DMIC1"},
	{"DMIC MUX10", "DMIC2", "DMIC2"},
	{"DMIC MUX10", "DMIC3", "DMIC3"},
	{"DMIC MUX10", "DMIC4", "DMIC4"},
	{"DMIC MUX10", "DMIC5", "DMIC5"},
	{"AMIC MUX10", "ADC1", "ADC1"},
	{"AMIC MUX10", "ADC2", "ADC2"},
	{"AMIC MUX10", "ADC3", "ADC3"},
	{"AMIC MUX10", "ADC4", "ADC4"},
	{"AMIC MUX10", "ADC5", "ADC5"},
	{"AMIC MUX10", "ADC6", "ADC6"},

	{"DMIC MUX11", "DMIC0", "DMIC0"},
	{"DMIC MUX11", "DMIC1", "DMIC1"},
	{"DMIC MUX11", "DMIC2", "DMIC2"},
	{"DMIC MUX11", "DMIC3", "DMIC3"},
	{"DMIC MUX11", "DMIC4", "DMIC4"},
	{"DMIC MUX11", "DMIC5", "DMIC5"},
	{"AMIC MUX11", "ADC1", "ADC1"},
	{"AMIC MUX11", "ADC2", "ADC2"},
	{"AMIC MUX11", "ADC3", "ADC3"},
	{"AMIC MUX11", "ADC4", "ADC4"},
	{"AMIC MUX11", "ADC5", "ADC5"},
	{"AMIC MUX11", "ADC6", "ADC6"},

	{"DMIC MUX12", "DMIC0", "DMIC0"},
	{"DMIC MUX12", "DMIC1", "DMIC1"},
	{"DMIC MUX12", "DMIC2", "DMIC2"},
	{"DMIC MUX12", "DMIC3", "DMIC3"},
	{"DMIC MUX12", "DMIC4", "DMIC4"},
	{"DMIC MUX12", "DMIC5", "DMIC5"},
	{"AMIC MUX12", "ADC1", "ADC1"},
	{"AMIC MUX12", "ADC2", "ADC2"},
	{"AMIC MUX12", "ADC3", "ADC3"},
	{"AMIC MUX12", "ADC4", "ADC4"},
	{"AMIC MUX12", "ADC5", "ADC5"},
	{"AMIC MUX12", "ADC6", "ADC6"},

	{"DMIC MUX13", "DMIC0", "DMIC0"},
	{"DMIC MUX13", "DMIC1", "DMIC1"},
	{"DMIC MUX13", "DMIC2", "DMIC2"},
	{"DMIC MUX13", "DMIC3", "DMIC3"},
	{"DMIC MUX13", "DMIC4", "DMIC4"},
	{"DMIC MUX13", "DMIC5", "DMIC5"},
	{"AMIC MUX13", "ADC1", "ADC1"},
	{"AMIC MUX13", "ADC2", "ADC2"},
	{"AMIC MUX13", "ADC3", "ADC3"},
	{"AMIC MUX13", "ADC4", "ADC4"},
	{"AMIC MUX13", "ADC5", "ADC5"},
	{"AMIC MUX13", "ADC6", "ADC6"},
	/* ADC Connections */
	{"ADC1", NULL, "AMIC1"},
	{"ADC2", NULL, "AMIC2"},
	{"ADC3", NULL, "AMIC3"},
	{"ADC4", NULL, "AMIC4"},
	{"ADC5", NULL, "AMIC5"},
	{"ADC6", NULL, "AMIC6"},

	{"RX INT0_1 MIX1", NULL, "RX INT0_1 MIX1 INP0"},
	{"RX INT0_1 MIX1", NULL, "RX INT0_1 MIX1 INP1"},
	{"RX INT0_1 MIX1", NULL, "RX INT0_1 MIX1 INP2"},
	{"RX INT1_1 MIX1", NULL, "RX INT1_1 MIX1 INP0"},
	{"RX INT1_1 MIX1", NULL, "RX INT1_1 MIX1 INP1"},
	{"RX INT1_1 MIX1", NULL, "RX INT1_1 MIX1 INP2"},
	{"RX INT2_1 MIX1", NULL, "RX INT2_1 MIX1 INP0"},
	{"RX INT2_1 MIX1", NULL, "RX INT2_1 MIX1 INP1"},
	{"RX INT2_1 MIX1", NULL, "RX INT2_1 MIX1 INP2"},
	{"RX INT3_1 MIX1", NULL, "RX INT3_1 MIX1 INP0"},
	{"RX INT3_1 MIX1", NULL, "RX INT3_1 MIX1 INP1"},
	{"RX INT3_1 MIX1", NULL, "RX INT3_1 MIX1 INP2"},
	{"RX INT4_1 MIX1", NULL, "RX INT4_1 MIX1 INP0"},
	{"RX INT4_1 MIX1", NULL, "RX INT4_1 MIX1 INP1"},
	{"RX INT4_1 MIX1", NULL, "RX INT4_1 MIX1 INP2"},
	{"RX INT5_1 MIX1", NULL, "RX INT5_1 MIX1 INP0"},
	{"RX INT5_1 MIX1", NULL, "RX INT5_1 MIX1 INP1"},
	{"RX INT5_1 MIX1", NULL, "RX INT5_1 MIX1 INP2"},
	{"RX INT6_1 MIX1", NULL, "RX INT6_1 MIX1 INP0"},
	{"RX INT6_1 MIX1", NULL, "RX INT6_1 MIX1 INP1"},
	{"RX INT6_1 MIX1", NULL, "RX INT6_1 MIX1 INP2"},
	{"RX INT7_1 MIX1", NULL, "RX INT7_1 MIX1 INP0"},
	{"RX INT7_1 MIX1", NULL, "RX INT7_1 MIX1 INP1"},
	{"RX INT7_1 MIX1", NULL, "RX INT7_1 MIX1 INP2"},
	{"RX INT8_1 MIX1", NULL, "RX INT8_1 MIX1 INP0"},
	{"RX INT8_1 MIX1", NULL, "RX INT8_1 MIX1 INP1"},
	{"RX INT8_1 MIX1", NULL, "RX INT8_1 MIX1 INP2"},

	{"RX INT0 SEC MIX", NULL, "RX INT0_1 MIX1"},
	{"RX INT0 MIX2", NULL, "RX INT0 SEC MIX"},
	{"RX INT0 MIX2", NULL, "RX INT0 MIX2 INP"},
	{"RX INT0 INTERP", NULL, "RX INT0 MIX2"},
	{"RX INT0 DEM MUX", "CLSH_DSM_OUT", "RX INT0 INTERP"},
	{"RX INT0 DAC", NULL, "RX INT0 DEM MUX"},
	{"RX INT0 DAC", NULL, "RX_BIAS"},
	{"EAR PA", NULL, "RX INT0 DAC"},
	{"EAR", NULL, "EAR PA"},

	{"SPL SRC0 MUX", "SRC_IN_HPHL", "RX INT1_1 MIX1"},
	{"RX INT1 SPLINE MIX", NULL, "RX INT1_1 MIX1"},
	{"RX INT1 SPLINE MIX", "HPHL Switch", "SPL SRC0 MUX"},
	{"RX INT1_1 NATIVE MUX", "ON", "RX INT1_1 MIX1"},
	{"RX INT1 SPLINE MIX", NULL, "RX INT1_1 NATIVE MUX"},
	{"RX INT1_1 NATIVE MUX", NULL, "RX INT1 NATIVE SUPPLY"},
	{"RX INT1 SEC MIX", NULL, "RX INT1 SPLINE MIX"},
	{"RX INT1 MIX2", NULL, "RX INT1 SEC MIX"},
	{"RX INT1 MIX2", NULL, "RX INT1 MIX2 INP"},
	{"RX INT1 INTERP", NULL, "RX INT1 MIX2"},
	{"RX INT1 DEM MUX", "CLSH_DSM_OUT", "RX INT1 INTERP"},
	{"RX INT1 DAC", NULL, "RX INT1 DEM MUX"},
	{"RX INT1 DAC", NULL, "RX_BIAS"},
	{"HPHL PA", NULL, "RX INT1 DAC"},
	{"HPHL", NULL, "HPHL PA"},

	{"SPL SRC1 MUX", "SRC_IN_HPHR", "RX INT2_1 MIX1"},
	{"RX INT2 SPLINE MIX", NULL, "RX INT2_1 MIX1"},
	{"RX INT2 SPLINE MIX", "HPHR Switch", "SPL SRC1 MUX"},
	{"RX INT2_1 NATIVE MUX", "ON", "RX INT2_1 MIX1"},
	{"RX INT2 SPLINE MIX", NULL, "RX INT2_1 NATIVE MUX"},
	{"RX INT2_1 NATIVE MUX", NULL, "RX INT2 NATIVE SUPPLY"},
	{"RX INT2 SEC MIX", NULL, "RX INT2 SPLINE MIX"},
	{"RX INT2 MIX2", NULL, "RX INT2 SEC MIX"},
	{"RX INT2 MIX2", NULL, "RX INT2 MIX2 INP"},
	{"RX INT2 INTERP", NULL, "RX INT2 MIX2"},
	{"RX INT2 DEM MUX", "CLSH_DSM_OUT", "RX INT2 INTERP"},
	{"RX INT2 DAC", NULL, "RX INT2 DEM MUX"},
	{"RX INT2 DAC", NULL, "RX_BIAS"},
	{"HPHR PA", NULL, "RX INT2 DAC"},
	{"HPHR", NULL, "HPHR PA"},

	{"SPL SRC0 MUX", "SRC_IN_LO1", "RX INT3_1 MIX1"},
	{"RX INT3 SPLINE MIX", NULL, "RX INT3_1 MIX1"},
	{"RX INT3 SPLINE MIX", "LO1 Switch", "SPL SRC0 MUX"},
	{"RX INT3_1 NATIVE MUX", "ON", "RX INT3_1 MIX1"},
	{"RX INT3 SPLINE MIX", NULL, "RX INT3_1 NATIVE MUX"},
	{"RX INT3_1 NATIVE MUX", NULL, "RX INT3 NATIVE SUPPLY"},
	{"RX INT3 SEC MIX", NULL, "RX INT3 SPLINE MIX"},
	{"RX INT3 MIX2", NULL, "RX INT3 SEC MIX"},
	{"RX INT3 MIX2", NULL, "RX INT3 MIX2 INP"},
	{"RX INT3 INTERP", NULL, "RX INT3 MIX2"},
	{"RX INT3 DAC", NULL, "RX INT3 INTERP"},
	{"RX INT3 DAC", NULL, "RX_BIAS"},
	{"LINEOUT1 PA", NULL, "RX INT3 DAC"},
	{"LINEOUT1", NULL, "LINEOUT1 PA"},

	{"SPL SRC1 MUX", "SRC_IN_LO2", "RX INT4_1 MIX1"},
	{"RX INT4 SPLINE MIX", NULL, "RX INT4_1 MIX1"},
	{"RX INT4 SPLINE MIX", "LO2 Switch", "SPL SRC1 MUX"},
	{"RX INT4_1 NATIVE MUX", "ON", "RX INT4_1 MIX1"},
	{"RX INT4 SPLINE MIX", NULL, "RX INT4_1 NATIVE MUX"},
	{"RX INT4_1 NATIVE MUX", NULL, "RX INT4 NATIVE SUPPLY"},
	{"RX INT4 SEC MIX", NULL, "RX INT4 SPLINE MIX"},
	{"RX INT4 MIX2", NULL, "RX INT4 SEC MIX"},
	{"RX INT4 MIX2", NULL, "RX INT4 MIX2 INP"},
	{"RX INT4 INTERP", NULL, "RX INT4 MIX2"},
	{"RX INT4 DAC", NULL, "RX INT4 INTERP"},
	{"RX INT4 DAC", NULL, "RX_BIAS"},
	{"LINEOUT2 PA", NULL, "RX INT4 DAC"},
	{"LINEOUT2", NULL, "LINEOUT2 PA"},

	{"SPL SRC2 MUX", "SRC_IN_LO3", "RX INT5_1 MIX1"},
	{"RX INT5 SPLINE MIX", NULL, "RX INT5_1 MIX1"},
	{"RX INT5 SPLINE MIX", "LO3 Switch", "SPL SRC2 MUX"},
	{"RX INT5 SEC MIX", NULL, "RX INT5 SPLINE MIX"},
	{"RX INT5 MIX2", NULL, "RX INT5 SEC MIX"},
	{"RX INT5 INTERP", NULL, "RX INT5 MIX2"},

	{"RX INT5 VBAT", "LO3 VBAT Enable", "RX INT5 INTERP"},
	{"RX INT5 DAC", NULL, "RX INT5 VBAT"},

	{"RX INT5 DAC", NULL, "RX INT5 INTERP"},
	{"RX INT5 DAC", NULL, "RX_BIAS"},
	{"LINEOUT3 PA", NULL, "RX INT5 DAC"},
	{"LINEOUT3", NULL, "LINEOUT3 PA"},

	{"SPL SRC3 MUX", "SRC_IN_LO4", "RX INT6_1 MIX1"},
	{"RX INT6 SPLINE MIX", NULL, "RX INT6_1 MIX1"},
	{"RX INT6 SPLINE MIX", "LO4 Switch", "SPL SRC3 MUX"},
	{"RX INT6 SEC MIX", NULL, "RX INT6 SPLINE MIX"},
	{"RX INT6 MIX2", NULL, "RX INT6 SEC MIX"},
	{"RX INT6 INTERP", NULL, "RX INT6 MIX2"},

	{"RX INT6 VBAT", "LO4 VBAT Enable", "RX INT6 INTERP"},
	{"RX INT6 DAC", NULL, "RX INT6 VBAT"},

	{"RX INT6 DAC", NULL, "RX INT6 INTERP"},
	{"RX INT6 DAC", NULL, "RX_BIAS"},
	{"LINEOUT4 PA", NULL, "RX INT6 DAC"},
	{"LINEOUT4", NULL, "LINEOUT4 PA"},

	{"SPL SRC2 MUX", "SRC_IN_SPKRL", "RX INT7_1 MIX1"},
	{"RX INT7 SPLINE MIX", NULL, "RX INT7_1 MIX1"},
	{"RX INT7 SPLINE MIX", "SPKRL Switch", "SPL SRC2 MUX"},
	{"RX INT7 SEC MIX", NULL, "RX INT7 SPLINE MIX"},
	{"RX INT7 MIX2", NULL, "RX INT7 SEC MIX"},
	{"RX INT7 MIX2", NULL, "RX INT7 MIX2 INP"},

	{"RX INT7 INTERP", NULL, "RX INT7 MIX2"},

	{"RX INT7 VBAT", "SPKRL VBAT Enable", "RX INT7 INTERP"},
	{"RX INT7 CHAIN", NULL, "RX INT7 VBAT"},

	{"RX INT7 CHAIN", NULL, "RX INT7 INTERP"},
	{"RX INT7 CHAIN", NULL, "RX_BIAS"},
	{"SPK1 OUT", NULL, "RX INT7 CHAIN"},

	{"ANC SPKR PA Enable", "Switch", "RX INT7 CHAIN"},
	{"ANC SPK1 PA", NULL, "ANC SPKR PA Enable"},
	{"SPK1 OUT", NULL, "ANC SPK1 PA"},

	{"SPL SRC3 MUX", "SRC_IN_SPKRR", "RX INT8_1 MIX1"},
	{"RX INT8 SPLINE MIX", NULL, "RX INT8_1 MIX1"},
	{"RX INT8 SPLINE MIX", "SPKRR Switch", "SPL SRC3 MUX"},
	{"RX INT8 SEC MIX", NULL, "RX INT8 SPLINE MIX"},
	{"RX INT8 INTERP", NULL, "RX INT8 SEC MIX"},

	{"RX INT8 VBAT", "SPKRR VBAT Enable", "RX INT8 INTERP"},
	{"RX INT8 CHAIN", NULL, "RX INT8 VBAT"},

	{"RX INT8 CHAIN", NULL, "RX INT8 INTERP"},
	{"RX INT8 CHAIN", NULL, "RX_BIAS"},
	{"SPK2 OUT", NULL, "RX INT8 CHAIN"},

	{"ANC0 FB MUX", "ANC_IN_EAR", "RX INT0 MIX2"},
	{"ANC0 FB MUX", "ANC_IN_HPHL", "RX INT1 MIX2"},
	{"ANC0 FB MUX", "ANC_IN_LO1", "RX INT3 MIX2"},
	{"ANC0 FB MUX", "ANC_IN_EAR_SPKR", "RX INT7 MIX2"},
	{"ANC1 FB MUX", "ANC_IN_HPHR", "RX INT2 MIX2"},
	{"ANC1 FB MUX", "ANC_IN_LO2", "RX INT4 MIX2"},

	{"ANC HPHL Enable", "Switch", "ADC MUX10"},
	{"ANC HPHL Enable", "Switch", "ADC MUX11"},
	{"RX INT1 MIX2", NULL, "ANC HPHL Enable"},

	{"ANC HPHR Enable", "Switch", "ADC MUX12"},
	{"ANC HPHR Enable", "Switch", "ADC MUX13"},
	{"RX INT2 MIX2", NULL, "ANC HPHR Enable"},

	{"ANC EAR Enable", "Switch", "ADC MUX10"},
	{"ANC EAR Enable", "Switch", "ADC MUX11"},
	{"RX INT0 MIX2", NULL, "ANC EAR Enable"},

	{"ANC OUT EAR SPKR Enable", "Switch", "ADC MUX10"},
	{"ANC OUT EAR SPKR Enable", "Switch", "ADC MUX11"},
	{"RX INT7 MIX2", NULL, "ANC OUT EAR SPKR Enable"},

	{"ANC LINEOUT1 Enable", "Switch", "ADC MUX10"},
	{"ANC LINEOUT1 Enable", "Switch", "ADC MUX11"},
	{"RX INT3 MIX2", NULL, "ANC LINEOUT1 Enable"},

	{"ANC LINEOUT2 Enable", "Switch", "ADC MUX12"},
	{"ANC LINEOUT2 Enable", "Switch", "ADC MUX13"},
	{"RX INT4 MIX2", NULL, "ANC LINEOUT2 Enable"},

	{"ANC EAR PA", NULL, "RX INT0 DAC"},
	{"ANC EAR", NULL, "ANC EAR PA"},
	{"ANC HPHL PA", NULL, "RX INT1 DAC"},
	{"ANC HPHL", NULL, "ANC HPHL PA"},
	{"ANC HPHR PA", NULL, "RX INT2 DAC"},
	{"ANC HPHR", NULL, "ANC HPHR PA"},
	{"ANC LINEOUT1 PA", NULL, "RX INT3 DAC"},
	{"ANC LINEOUT1", NULL, "ANC LINEOUT1 PA"},
	{"ANC LINEOUT2 PA", NULL, "RX INT4 DAC"},
	{"ANC LINEOUT2", NULL, "ANC LINEOUT2 PA"},

	/* SLIM_MUX("AIF1_PB", "AIF1 PB"),*/
	{"SLIM RX0 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX1 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX2 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX3 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX4 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX5 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX6 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX7 MUX", "AIF1_PB", "AIF1 PB"},
	/* SLIM_MUX("AIF2_PB", "AIF2 PB"),*/
	{"SLIM RX0 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX1 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX2 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX3 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX4 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX5 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX6 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX7 MUX", "AIF2_PB", "AIF2 PB"},
	/* SLIM_MUX("AIF3_PB", "AIF3 PB"),*/
	{"SLIM RX0 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX1 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX2 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX3 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX4 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX5 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX6 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX7 MUX", "AIF3_PB", "AIF3 PB"},
	/* SLIM_MUX("AIF4_PB", "AIF4 PB"),*/
	{"SLIM RX0 MUX", "AIF4_PB", "AIF4 PB"},
	{"SLIM RX1 MUX", "AIF4_PB", "AIF4 PB"},
	{"SLIM RX2 MUX", "AIF4_PB", "AIF4 PB"},
	{"SLIM RX3 MUX", "AIF4_PB", "AIF4 PB"},
	{"SLIM RX4 MUX", "AIF4_PB", "AIF4 PB"},
	{"SLIM RX5 MUX", "AIF4_PB", "AIF4 PB"},
	{"SLIM RX6 MUX", "AIF4_PB", "AIF4 PB"},
	{"SLIM RX7 MUX", "AIF4_PB", "AIF4 PB"},

	/* SLIM_MUX("AIF_MIX1_PB", "AIF MIX1 PB"),*/
	{"SLIM RX0 MUX", "AIF_MIX1_PB", "AIF MIX1 PB"},
	{"SLIM RX1 MUX", "AIF_MIX1_PB", "AIF MIX1 PB"},
	{"SLIM RX2 MUX", "AIF_MIX1_PB", "AIF MIX1 PB"},
	{"SLIM RX3 MUX", "AIF_MIX1_PB", "AIF MIX1 PB"},
	{"SLIM RX4 MUX", "AIF_MIX1_PB", "AIF MIX1 PB"},
	{"SLIM RX5 MUX", "AIF_MIX1_PB", "AIF MIX1 PB"},
	{"SLIM RX6 MUX", "AIF_MIX1_PB", "AIF MIX1 PB"},
	{"SLIM RX7 MUX", "AIF_MIX1_PB", "AIF MIX1 PB"},

	{"SLIM RX0", NULL, "SLIM RX0 MUX"},
	{"SLIM RX1", NULL, "SLIM RX1 MUX"},
	{"SLIM RX2", NULL, "SLIM RX2 MUX"},
	{"SLIM RX3", NULL, "SLIM RX3 MUX"},
	{"SLIM RX4", NULL, "SLIM RX4 MUX"},
	{"SLIM RX5", NULL, "SLIM RX5 MUX"},
	{"SLIM RX6", NULL, "SLIM RX6 MUX"},
	{"SLIM RX7", NULL, "SLIM RX7 MUX"},

	{"RX INT0_1 MIX1 INP0", "RX0", "SLIM RX0"},
	{"RX INT0_1 MIX1 INP0", "RX1", "SLIM RX1"},
	{"RX INT0_1 MIX1 INP0", "RX2", "SLIM RX2"},
	{"RX INT0_1 MIX1 INP0", "RX3", "SLIM RX3"},
	{"RX INT0_1 MIX1 INP0", "RX4", "SLIM RX4"},
	{"RX INT0_1 MIX1 INP0", "RX5", "SLIM RX5"},
	{"RX INT0_1 MIX1 INP0", "RX6", "SLIM RX6"},
	{"RX INT0_1 MIX1 INP0", "RX7", "SLIM RX7"},
	{"RX INT0_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT0_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT0_1 MIX1 INP1", "RX0", "SLIM RX0"},
	{"RX INT0_1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX INT0_1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX INT0_1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX INT0_1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX INT0_1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX INT0_1 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX INT0_1 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX INT0_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT0_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT0_1 MIX1 INP2", "RX0", "SLIM RX0"},
	{"RX INT0_1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX INT0_1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX INT0_1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX INT0_1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX INT0_1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX INT0_1 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX INT0_1 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX INT0_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT0_1 MIX1 INP2", "IIR1", "IIR1"},

	/* MIXing path INT0 */
	{"RX INT0_2 MUX", "RX0", "SLIM RX0"},
	{"RX INT0_2 MUX", "RX1", "SLIM RX1"},
	{"RX INT0_2 MUX", "RX2", "SLIM RX2"},
	{"RX INT0_2 MUX", "RX3", "SLIM RX3"},
	{"RX INT0_2 MUX", "RX4", "SLIM RX4"},
	{"RX INT0_2 MUX", "RX5", "SLIM RX5"},
	{"RX INT0_2 MUX", "RX6", "SLIM RX6"},
	{"RX INT0_2 MUX", "RX7", "SLIM RX7"},
	{"RX INT0 SEC MIX", NULL, "RX INT0_2 MUX"},

	/* MIXing path INT1 */
	{"RX INT1_2 MUX", "RX0", "SLIM RX0"},
	{"RX INT1_2 MUX", "RX1", "SLIM RX1"},
	{"RX INT1_2 MUX", "RX2", "SLIM RX2"},
	{"RX INT1_2 MUX", "RX3", "SLIM RX3"},
	{"RX INT1_2 MUX", "RX4", "SLIM RX4"},
	{"RX INT1_2 MUX", "RX5", "SLIM RX5"},
	{"RX INT1_2 MUX", "RX6", "SLIM RX6"},
	{"RX INT1_2 MUX", "RX7", "SLIM RX7"},
	{"RX INT1 SEC MIX", NULL, "RX INT1_2 MUX"},

	/* MIXing path INT2 */
	{"RX INT2_2 MUX", "RX0", "SLIM RX0"},
	{"RX INT2_2 MUX", "RX1", "SLIM RX1"},
	{"RX INT2_2 MUX", "RX2", "SLIM RX2"},
	{"RX INT2_2 MUX", "RX3", "SLIM RX3"},
	{"RX INT2_2 MUX", "RX4", "SLIM RX4"},
	{"RX INT2_2 MUX", "RX5", "SLIM RX5"},
	{"RX INT2_2 MUX", "RX6", "SLIM RX6"},
	{"RX INT2_2 MUX", "RX7", "SLIM RX7"},
	{"RX INT2 SEC MIX", NULL, "RX INT2_2 MUX"},

	/* MIXing path INT3 */
	{"RX INT3_2 MUX", "RX0", "SLIM RX0"},
	{"RX INT3_2 MUX", "RX1", "SLIM RX1"},
	{"RX INT3_2 MUX", "RX2", "SLIM RX2"},
	{"RX INT3_2 MUX", "RX3", "SLIM RX3"},
	{"RX INT3_2 MUX", "RX4", "SLIM RX4"},
	{"RX INT3_2 MUX", "RX5", "SLIM RX5"},
	{"RX INT3_2 MUX", "RX6", "SLIM RX6"},
	{"RX INT3_2 MUX", "RX7", "SLIM RX7"},
	{"RX INT3 SEC MIX", NULL, "RX INT3_2 MUX"},

	/* MIXing path INT4 */
	{"RX INT4_2 MUX", "RX0", "SLIM RX0"},
	{"RX INT4_2 MUX", "RX1", "SLIM RX1"},
	{"RX INT4_2 MUX", "RX2", "SLIM RX2"},
	{"RX INT4_2 MUX", "RX3", "SLIM RX3"},
	{"RX INT4_2 MUX", "RX4", "SLIM RX4"},
	{"RX INT4_2 MUX", "RX5", "SLIM RX5"},
	{"RX INT4_2 MUX", "RX6", "SLIM RX6"},
	{"RX INT4_2 MUX", "RX7", "SLIM RX7"},
	{"RX INT4 SEC MIX", NULL, "RX INT4_2 MUX"},

	/* MIXing path INT5 */
	{"RX INT5_2 MUX", "RX0", "SLIM RX0"},
	{"RX INT5_2 MUX", "RX1", "SLIM RX1"},
	{"RX INT5_2 MUX", "RX2", "SLIM RX2"},
	{"RX INT5_2 MUX", "RX3", "SLIM RX3"},
	{"RX INT5_2 MUX", "RX4", "SLIM RX4"},
	{"RX INT5_2 MUX", "RX5", "SLIM RX5"},
	{"RX INT5_2 MUX", "RX6", "SLIM RX6"},
	{"RX INT5_2 MUX", "RX7", "SLIM RX7"},
	{"RX INT5 SEC MIX", NULL, "RX INT5_2 MUX"},

	/* MIXing path INT6 */
	{"RX INT6_2 MUX", "RX0", "SLIM RX0"},
	{"RX INT6_2 MUX", "RX1", "SLIM RX1"},
	{"RX INT6_2 MUX", "RX2", "SLIM RX2"},
	{"RX INT6_2 MUX", "RX3", "SLIM RX3"},
	{"RX INT6_2 MUX", "RX4", "SLIM RX4"},
	{"RX INT6_2 MUX", "RX5", "SLIM RX5"},
	{"RX INT6_2 MUX", "RX6", "SLIM RX6"},
	{"RX INT6_2 MUX", "RX7", "SLIM RX7"},
	{"RX INT6 SEC MIX", NULL, "RX INT6_2 MUX"},

	/* MIXing path INT7 */
	{"RX INT7_2 MUX", "RX0", "SLIM RX0"},
	{"RX INT7_2 MUX", "RX1", "SLIM RX1"},
	{"RX INT7_2 MUX", "RX2", "SLIM RX2"},
	{"RX INT7_2 MUX", "RX3", "SLIM RX3"},
	{"RX INT7_2 MUX", "RX4", "SLIM RX4"},
	{"RX INT7_2 MUX", "RX5", "SLIM RX5"},
	{"RX INT7_2 MUX", "RX6", "SLIM RX6"},
	{"RX INT7_2 MUX", "RX7", "SLIM RX7"},
	{"RX INT7 SEC MIX", NULL, "RX INT7_2 MUX"},

	/* MIXing path INT8 */
	{"RX INT8_2 MUX", "RX0", "SLIM RX0"},
	{"RX INT8_2 MUX", "RX1", "SLIM RX1"},
	{"RX INT8_2 MUX", "RX2", "SLIM RX2"},
	{"RX INT8_2 MUX", "RX3", "SLIM RX3"},
	{"RX INT8_2 MUX", "RX4", "SLIM RX4"},
	{"RX INT8_2 MUX", "RX5", "SLIM RX5"},
	{"RX INT8_2 MUX", "RX6", "SLIM RX6"},
	{"RX INT8_2 MUX", "RX7", "SLIM RX7"},
	{"RX INT8 SEC MIX", NULL, "RX INT8_2 MUX"},

	{"RX INT1_1 MIX1 INP0", "RX0", "SLIM RX0"},
	{"RX INT1_1 MIX1 INP0", "RX1", "SLIM RX1"},
	{"RX INT1_1 MIX1 INP0", "RX2", "SLIM RX2"},
	{"RX INT1_1 MIX1 INP0", "RX3", "SLIM RX3"},
	{"RX INT1_1 MIX1 INP0", "RX4", "SLIM RX4"},
	{"RX INT1_1 MIX1 INP0", "RX5", "SLIM RX5"},
	{"RX INT1_1 MIX1 INP0", "RX6", "SLIM RX6"},
	{"RX INT1_1 MIX1 INP0", "RX7", "SLIM RX7"},
	{"RX INT1_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT1_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT1_1 MIX1 INP1", "RX0", "SLIM RX0"},
	{"RX INT1_1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX INT1_1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX INT1_1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX INT1_1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX INT1_1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX INT1_1 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX INT1_1 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX INT1_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT1_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT1_1 MIX1 INP2", "RX0", "SLIM RX0"},
	{"RX INT1_1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX INT1_1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX INT1_1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX INT1_1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX INT1_1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX INT1_1 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX INT1_1 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX INT1_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT1_1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX INT2_1 MIX1 INP0", "RX0", "SLIM RX0"},
	{"RX INT2_1 MIX1 INP0", "RX1", "SLIM RX1"},
	{"RX INT2_1 MIX1 INP0", "RX2", "SLIM RX2"},
	{"RX INT2_1 MIX1 INP0", "RX3", "SLIM RX3"},
	{"RX INT2_1 MIX1 INP0", "RX4", "SLIM RX4"},
	{"RX INT2_1 MIX1 INP0", "RX5", "SLIM RX5"},
	{"RX INT2_1 MIX1 INP0", "RX6", "SLIM RX6"},
	{"RX INT2_1 MIX1 INP0", "RX7", "SLIM RX7"},
	{"RX INT2_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT2_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT2_1 MIX1 INP1", "RX0", "SLIM RX0"},
	{"RX INT2_1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX INT2_1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX INT2_1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX INT2_1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX INT2_1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX INT2_1 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX INT2_1 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX INT2_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT2_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT2_1 MIX1 INP2", "RX0", "SLIM RX0"},
	{"RX INT2_1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX INT2_1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX INT2_1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX INT2_1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX INT2_1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX INT2_1 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX INT2_1 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX INT2_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT2_1 MIX1 INP2", "IIR1", "IIR1"},

	{"RX INT3_1 MIX1 INP0", "RX0", "SLIM RX0"},
	{"RX INT3_1 MIX1 INP0", "RX1", "SLIM RX1"},
	{"RX INT3_1 MIX1 INP0", "RX2", "SLIM RX2"},
	{"RX INT3_1 MIX1 INP0", "RX3", "SLIM RX3"},
	{"RX INT3_1 MIX1 INP0", "RX4", "SLIM RX4"},
	{"RX INT3_1 MIX1 INP0", "RX5", "SLIM RX5"},
	{"RX INT3_1 MIX1 INP0", "RX6", "SLIM RX6"},
	{"RX INT3_1 MIX1 INP0", "RX7", "SLIM RX7"},
	{"RX INT3_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT3_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT3_1 MIX1 INP1", "RX0", "SLIM RX0"},
	{"RX INT3_1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX INT3_1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX INT3_1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX INT3_1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX INT3_1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX INT3_1 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX INT3_1 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX INT3_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT3_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT3_1 MIX1 INP2", "RX0", "SLIM RX0"},
	{"RX INT3_1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX INT3_1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX INT3_1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX INT3_1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX INT3_1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX INT3_1 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX INT3_1 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX INT3_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT3_1 MIX1 INP2", "IIR1", "IIR1"},

	{"RX INT4_1 MIX1 INP0", "RX0", "SLIM RX0"},
	{"RX INT4_1 MIX1 INP0", "RX1", "SLIM RX1"},
	{"RX INT4_1 MIX1 INP0", "RX2", "SLIM RX2"},
	{"RX INT4_1 MIX1 INP0", "RX3", "SLIM RX3"},
	{"RX INT4_1 MIX1 INP0", "RX4", "SLIM RX4"},
	{"RX INT4_1 MIX1 INP0", "RX5", "SLIM RX5"},
	{"RX INT4_1 MIX1 INP0", "RX6", "SLIM RX6"},
	{"RX INT4_1 MIX1 INP0", "RX7", "SLIM RX7"},
	{"RX INT4_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT4_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT4_1 MIX1 INP1", "RX0", "SLIM RX0"},
	{"RX INT4_1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX INT4_1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX INT4_1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX INT4_1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX INT4_1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX INT4_1 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX INT4_1 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX INT4_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT4_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT4_1 MIX1 INP2", "RX0", "SLIM RX0"},
	{"RX INT4_1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX INT4_1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX INT4_1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX INT4_1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX INT4_1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX INT4_1 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX INT4_1 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX INT4_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT4_1 MIX1 INP2", "IIR1", "IIR1"},

	{"RX INT5_1 MIX1 INP0", "RX0", "SLIM RX0"},
	{"RX INT5_1 MIX1 INP0", "RX1", "SLIM RX1"},
	{"RX INT5_1 MIX1 INP0", "RX2", "SLIM RX2"},
	{"RX INT5_1 MIX1 INP0", "RX3", "SLIM RX3"},
	{"RX INT5_1 MIX1 INP0", "RX4", "SLIM RX4"},
	{"RX INT5_1 MIX1 INP0", "RX5", "SLIM RX5"},
	{"RX INT5_1 MIX1 INP0", "RX6", "SLIM RX6"},
	{"RX INT5_1 MIX1 INP0", "RX7", "SLIM RX7"},
	{"RX INT5_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT5_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT5_1 MIX1 INP1", "RX0", "SLIM RX0"},
	{"RX INT5_1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX INT5_1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX INT5_1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX INT5_1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX INT5_1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX INT5_1 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX INT5_1 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX INT5_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT5_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT5_1 MIX1 INP2", "RX0", "SLIM RX0"},
	{"RX INT5_1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX INT5_1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX INT5_1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX INT5_1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX INT5_1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX INT5_1 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX INT5_1 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX INT5_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT5_1 MIX1 INP2", "IIR1", "IIR1"},

	{"RX INT6_1 MIX1 INP0", "RX0", "SLIM RX0"},
	{"RX INT6_1 MIX1 INP0", "RX1", "SLIM RX1"},
	{"RX INT6_1 MIX1 INP0", "RX2", "SLIM RX2"},
	{"RX INT6_1 MIX1 INP0", "RX3", "SLIM RX3"},
	{"RX INT6_1 MIX1 INP0", "RX4", "SLIM RX4"},
	{"RX INT6_1 MIX1 INP0", "RX5", "SLIM RX5"},
	{"RX INT6_1 MIX1 INP0", "RX6", "SLIM RX6"},
	{"RX INT6_1 MIX1 INP0", "RX7", "SLIM RX7"},
	{"RX INT6_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT6_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT6_1 MIX1 INP1", "RX0", "SLIM RX0"},
	{"RX INT6_1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX INT6_1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX INT6_1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX INT6_1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX INT6_1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX INT6_1 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX INT6_1 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX INT6_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT6_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT6_1 MIX1 INP2", "RX0", "SLIM RX0"},
	{"RX INT6_1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX INT6_1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX INT6_1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX INT6_1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX INT6_1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX INT6_1 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX INT6_1 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX INT6_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT6_1 MIX1 INP2", "IIR1", "IIR1"},

	{"RX INT7_1 MIX1 INP0", "RX0", "SLIM RX0"},
	{"RX INT7_1 MIX1 INP0", "RX1", "SLIM RX1"},
	{"RX INT7_1 MIX1 INP0", "RX2", "SLIM RX2"},
	{"RX INT7_1 MIX1 INP0", "RX3", "SLIM RX3"},
	{"RX INT7_1 MIX1 INP0", "RX4", "SLIM RX4"},
	{"RX INT7_1 MIX1 INP0", "RX5", "SLIM RX5"},
	{"RX INT7_1 MIX1 INP0", "RX6", "SLIM RX6"},
	{"RX INT7_1 MIX1 INP0", "RX7", "SLIM RX7"},
	{"RX INT7_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT7_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT7_1 MIX1 INP1", "RX0", "SLIM RX0"},
	{"RX INT7_1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX INT7_1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX INT7_1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX INT7_1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX INT7_1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX INT7_1 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX INT7_1 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX INT7_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT7_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT7_1 MIX1 INP2", "RX0", "SLIM RX0"},
	{"RX INT7_1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX INT7_1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX INT7_1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX INT7_1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX INT7_1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX INT7_1 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX INT7_1 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX INT7_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT7_1 MIX1 INP2", "IIR1", "IIR1"},

	{"RX INT8_1 MIX1 INP0", "RX0", "SLIM RX0"},
	{"RX INT8_1 MIX1 INP0", "RX1", "SLIM RX1"},
	{"RX INT8_1 MIX1 INP0", "RX2", "SLIM RX2"},
	{"RX INT8_1 MIX1 INP0", "RX3", "SLIM RX3"},
	{"RX INT8_1 MIX1 INP0", "RX4", "SLIM RX4"},
	{"RX INT8_1 MIX1 INP0", "RX5", "SLIM RX5"},
	{"RX INT8_1 MIX1 INP0", "RX6", "SLIM RX6"},
	{"RX INT8_1 MIX1 INP0", "RX7", "SLIM RX7"},
	{"RX INT8_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT8_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT8_1 MIX1 INP1", "RX0", "SLIM RX0"},
	{"RX INT8_1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX INT8_1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX INT8_1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX INT8_1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX INT8_1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX INT8_1 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX INT8_1 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX INT8_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT8_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT8_1 MIX1 INP2", "RX0", "SLIM RX0"},
	{"RX INT8_1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX INT8_1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX INT8_1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX INT8_1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX INT8_1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX INT8_1 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX INT8_1 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX INT8_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT8_1 MIX1 INP2", "IIR1", "IIR1"},

	/* SRC0, SRC1 inputs to Sidetone RX Mixer
	 * on RX0, RX1, RX2, RX3, RX4 and RX7 chains
	 */
	{"IIR0", NULL, "IIR0 INP0 MUX"},
	{"IIR0 INP0 MUX", "DEC0", "ADC MUX0"},
	{"IIR0 INP0 MUX", "DEC1", "ADC MUX1"},
	{"IIR0 INP0 MUX", "DEC2", "ADC MUX2"},
	{"IIR0 INP0 MUX", "DEC3", "ADC MUX3"},
	{"IIR0 INP0 MUX", "DEC4", "ADC MUX4"},
	{"IIR0 INP0 MUX", "DEC5", "ADC MUX5"},
	{"IIR0 INP0 MUX", "DEC6", "ADC MUX6"},
	{"IIR0 INP0 MUX", "DEC7", "ADC MUX7"},
	{"IIR0 INP0 MUX", "DEC8", "ADC MUX8"},
	{"IIR0 INP0 MUX", "RX0", "SLIM RX0"},
	{"IIR0 INP0 MUX", "RX1", "SLIM RX1"},
	{"IIR0 INP0 MUX", "RX2", "SLIM RX2"},
	{"IIR0 INP0 MUX", "RX3", "SLIM RX3"},
	{"IIR0 INP0 MUX", "RX4", "SLIM RX4"},
	{"IIR0 INP0 MUX", "RX5", "SLIM RX5"},
	{"IIR0 INP0 MUX", "RX6", "SLIM RX6"},
	{"IIR0 INP0 MUX", "RX7", "SLIM RX7"},
	{"IIR0", NULL, "IIR0 INP1 MUX"},
	{"IIR0 INP1 MUX", "DEC0", "ADC MUX0"},
	{"IIR0 INP1 MUX", "DEC1", "ADC MUX1"},
	{"IIR0 INP1 MUX", "DEC2", "ADC MUX2"},
	{"IIR0 INP1 MUX", "DEC3", "ADC MUX3"},
	{"IIR0 INP1 MUX", "DEC4", "ADC MUX4"},
	{"IIR0 INP1 MUX", "DEC5", "ADC MUX5"},
	{"IIR0 INP1 MUX", "DEC6", "ADC MUX6"},
	{"IIR0 INP1 MUX", "DEC7", "ADC MUX7"},
	{"IIR0 INP1 MUX", "DEC8", "ADC MUX8"},
	{"IIR0 INP1 MUX", "RX0", "SLIM RX0"},
	{"IIR0 INP1 MUX", "RX1", "SLIM RX1"},
	{"IIR0 INP1 MUX", "RX2", "SLIM RX2"},
	{"IIR0 INP1 MUX", "RX3", "SLIM RX3"},
	{"IIR0 INP1 MUX", "RX4", "SLIM RX4"},
	{"IIR0 INP1 MUX", "RX5", "SLIM RX5"},
	{"IIR0 INP1 MUX", "RX6", "SLIM RX6"},
	{"IIR0 INP1 MUX", "RX7", "SLIM RX7"},
	{"IIR0", NULL, "IIR0 INP2 MUX"},
	{"IIR0 INP2 MUX", "DEC0", "ADC MUX0"},
	{"IIR0 INP2 MUX", "DEC1", "ADC MUX1"},
	{"IIR0 INP2 MUX", "DEC2", "ADC MUX2"},
	{"IIR0 INP2 MUX", "DEC3", "ADC MUX3"},
	{"IIR0 INP2 MUX", "DEC4", "ADC MUX4"},
	{"IIR0 INP2 MUX", "DEC5", "ADC MUX5"},
	{"IIR0 INP2 MUX", "DEC6", "ADC MUX6"},
	{"IIR0 INP2 MUX", "DEC7", "ADC MUX7"},
	{"IIR0 INP2 MUX", "DEC8", "ADC MUX8"},
	{"IIR0 INP2 MUX", "RX0", "SLIM RX0"},
	{"IIR0 INP2 MUX", "RX1", "SLIM RX1"},
	{"IIR0 INP2 MUX", "RX2", "SLIM RX2"},
	{"IIR0 INP2 MUX", "RX3", "SLIM RX3"},
	{"IIR0 INP2 MUX", "RX4", "SLIM RX4"},
	{"IIR0 INP2 MUX", "RX5", "SLIM RX5"},
	{"IIR0 INP2 MUX", "RX6", "SLIM RX6"},
	{"IIR0 INP2 MUX", "RX7", "SLIM RX7"},
	{"IIR0", NULL, "IIR0 INP3 MUX"},
	{"IIR0 INP3 MUX", "DEC0", "ADC MUX0"},
	{"IIR0 INP3 MUX", "DEC1", "ADC MUX1"},
	{"IIR0 INP3 MUX", "DEC2", "ADC MUX2"},
	{"IIR0 INP3 MUX", "DEC3", "ADC MUX3"},
	{"IIR0 INP3 MUX", "DEC4", "ADC MUX4"},
	{"IIR0 INP3 MUX", "DEC5", "ADC MUX5"},
	{"IIR0 INP3 MUX", "DEC6", "ADC MUX6"},
	{"IIR0 INP3 MUX", "DEC7", "ADC MUX7"},
	{"IIR0 INP3 MUX", "DEC8", "ADC MUX8"},
	{"IIR0 INP3 MUX", "RX0", "SLIM RX0"},
	{"IIR0 INP3 MUX", "RX1", "SLIM RX1"},
	{"IIR0 INP3 MUX", "RX2", "SLIM RX2"},
	{"IIR0 INP3 MUX", "RX3", "SLIM RX3"},
	{"IIR0 INP3 MUX", "RX4", "SLIM RX4"},
	{"IIR0 INP3 MUX", "RX5", "SLIM RX5"},
	{"IIR0 INP3 MUX", "RX6", "SLIM RX6"},
	{"IIR0 INP3 MUX", "RX7", "SLIM RX7"},

	{"IIR1", NULL, "IIR1 INP0 MUX"},
	{"IIR1 INP0 MUX", "DEC0", "ADC MUX0"},
	{"IIR1 INP0 MUX", "DEC1", "ADC MUX1"},
	{"IIR1 INP0 MUX", "DEC2", "ADC MUX2"},
	{"IIR1 INP0 MUX", "DEC3", "ADC MUX3"},
	{"IIR1 INP0 MUX", "DEC4", "ADC MUX4"},
	{"IIR1 INP0 MUX", "DEC5", "ADC MUX5"},
	{"IIR1 INP0 MUX", "DEC6", "ADC MUX6"},
	{"IIR1 INP0 MUX", "DEC7", "ADC MUX7"},
	{"IIR1 INP0 MUX", "DEC8", "ADC MUX8"},
	{"IIR1 INP0 MUX", "RX0", "SLIM RX0"},
	{"IIR1 INP0 MUX", "RX1", "SLIM RX1"},
	{"IIR1 INP0 MUX", "RX2", "SLIM RX2"},
	{"IIR1 INP0 MUX", "RX3", "SLIM RX3"},
	{"IIR1 INP0 MUX", "RX4", "SLIM RX4"},
	{"IIR1 INP0 MUX", "RX5", "SLIM RX5"},
	{"IIR1 INP0 MUX", "RX6", "SLIM RX6"},
	{"IIR1 INP0 MUX", "RX7", "SLIM RX7"},
	{"IIR1", NULL, "IIR1 INP1 MUX"},
	{"IIR1 INP1 MUX", "DEC0", "ADC MUX0"},
	{"IIR1 INP1 MUX", "DEC1", "ADC MUX1"},
	{"IIR1 INP1 MUX", "DEC2", "ADC MUX2"},
	{"IIR1 INP1 MUX", "DEC3", "ADC MUX3"},
	{"IIR1 INP1 MUX", "DEC4", "ADC MUX4"},
	{"IIR1 INP1 MUX", "DEC5", "ADC MUX5"},
	{"IIR1 INP1 MUX", "DEC6", "ADC MUX6"},
	{"IIR1 INP1 MUX", "DEC7", "ADC MUX7"},
	{"IIR1 INP1 MUX", "DEC8", "ADC MUX8"},
	{"IIR1 INP1 MUX", "RX0", "SLIM RX0"},
	{"IIR1 INP1 MUX", "RX1", "SLIM RX1"},
	{"IIR1 INP1 MUX", "RX2", "SLIM RX2"},
	{"IIR1 INP1 MUX", "RX3", "SLIM RX3"},
	{"IIR1 INP1 MUX", "RX4", "SLIM RX4"},
	{"IIR1 INP1 MUX", "RX5", "SLIM RX5"},
	{"IIR1 INP1 MUX", "RX6", "SLIM RX6"},
	{"IIR1 INP1 MUX", "RX7", "SLIM RX7"},
	{"IIR1", NULL, "IIR1 INP2 MUX"},
	{"IIR1 INP2 MUX", "DEC0", "ADC MUX0"},
	{"IIR1 INP2 MUX", "DEC1", "ADC MUX1"},
	{"IIR1 INP2 MUX", "DEC2", "ADC MUX2"},
	{"IIR1 INP2 MUX", "DEC3", "ADC MUX3"},
	{"IIR1 INP2 MUX", "DEC4", "ADC MUX4"},
	{"IIR1 INP2 MUX", "DEC5", "ADC MUX5"},
	{"IIR1 INP2 MUX", "DEC6", "ADC MUX6"},
	{"IIR1 INP2 MUX", "DEC7", "ADC MUX7"},
	{"IIR1 INP2 MUX", "DEC8", "ADC MUX8"},
	{"IIR1 INP2 MUX", "RX0", "SLIM RX0"},
	{"IIR1 INP2 MUX", "RX1", "SLIM RX1"},
	{"IIR1 INP2 MUX", "RX2", "SLIM RX2"},
	{"IIR1 INP2 MUX", "RX3", "SLIM RX3"},
	{"IIR1 INP2 MUX", "RX4", "SLIM RX4"},
	{"IIR1 INP2 MUX", "RX5", "SLIM RX5"},
	{"IIR1 INP2 MUX", "RX6", "SLIM RX6"},
	{"IIR1 INP2 MUX", "RX7", "SLIM RX7"},
	{"IIR1", NULL, "IIR1 INP3 MUX"},
	{"IIR1 INP3 MUX", "DEC0", "ADC MUX0"},
	{"IIR1 INP3 MUX", "DEC1", "ADC MUX1"},
	{"IIR1 INP3 MUX", "DEC2", "ADC MUX2"},
	{"IIR1 INP3 MUX", "DEC3", "ADC MUX3"},
	{"IIR1 INP3 MUX", "DEC4", "ADC MUX4"},
	{"IIR1 INP3 MUX", "DEC5", "ADC MUX5"},
	{"IIR1 INP3 MUX", "DEC6", "ADC MUX6"},
	{"IIR1 INP3 MUX", "DEC7", "ADC MUX7"},
	{"IIR1 INP3 MUX", "DEC8", "ADC MUX8"},
	{"IIR1 INP3 MUX", "RX0", "SLIM RX0"},
	{"IIR1 INP3 MUX", "RX1", "SLIM RX1"},
	{"IIR1 INP3 MUX", "RX2", "SLIM RX2"},
	{"IIR1 INP3 MUX", "RX3", "SLIM RX3"},
	{"IIR1 INP3 MUX", "RX4", "SLIM RX4"},
	{"IIR1 INP3 MUX", "RX5", "SLIM RX5"},
	{"IIR1 INP3 MUX", "RX6", "SLIM RX6"},
	{"IIR1 INP3 MUX", "RX7", "SLIM RX7"},

	{"SRC0", NULL, "IIR0"},
	{"SRC1", NULL, "IIR1"},
	{"RX INT0 MIX2 INP", "SRC0", "SRC0"},
	{"RX INT0 MIX2 INP", "SRC1", "SRC1"},
	{"RX INT1 MIX2 INP", "SRC0", "SRC0"},
	{"RX INT1 MIX2 INP", "SRC1", "SRC1"},
	{"RX INT2 MIX2 INP", "SRC0", "SRC0"},
	{"RX INT2 MIX2 INP", "SRC1", "SRC1"},
	{"RX INT3 MIX2 INP", "SRC0", "SRC0"},
	{"RX INT3 MIX2 INP", "SRC1", "SRC1"},
	{"RX INT4 MIX2 INP", "SRC0", "SRC0"},
	{"RX INT4 MIX2 INP", "SRC1", "SRC1"},
	{"RX INT7 MIX2 INP", "SRC0", "SRC0"},
	{"RX INT7 MIX2 INP", "SRC1", "SRC1"},
};

static int tasha_amic_pwr_lvl_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u16 amic_reg;

	if (!strcmp(kcontrol->id.name, "AMIC_1_2 PWR MODE"))
		amic_reg = WCD9335_ANA_AMIC1;
	if (!strcmp(kcontrol->id.name, "AMIC_3_4 PWR MODE"))
		amic_reg = WCD9335_ANA_AMIC3;
	if (!strcmp(kcontrol->id.name, "AMIC_5_6 PWR MODE"))
		amic_reg = WCD9335_ANA_AMIC5;

	ucontrol->value.integer.value[0] =
		(snd_soc_read(codec, amic_reg) & WCD9335_AMIC_PWR_LVL_MASK) >>
			     WCD9335_AMIC_PWR_LVL_SHIFT;

	return 0;
}

static int tasha_amic_pwr_lvl_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u32 mode_val;
	u16 amic_reg;

	mode_val = ucontrol->value.enumerated.item[0];

	dev_dbg(codec->dev, "%s: mode: %d\n",
		__func__, mode_val);

	if (!strcmp(kcontrol->id.name, "AMIC_1_2 PWR MODE"))
		amic_reg = WCD9335_ANA_AMIC1;
	if (!strcmp(kcontrol->id.name, "AMIC_3_4 PWR MODE"))
		amic_reg = WCD9335_ANA_AMIC3;
	if (!strcmp(kcontrol->id.name, "AMIC_5_6 PWR MODE"))
		amic_reg = WCD9335_ANA_AMIC5;

	snd_soc_update_bits(codec, amic_reg, WCD9335_AMIC_PWR_LVL_MASK,
			    mode_val << WCD9335_AMIC_PWR_LVL_SHIFT);

	return 0;
}

static int tasha_rx_hph_mode_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tasha->hph_mode;
	return 0;
}

static int tasha_rx_hph_mode_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u32 mode_val;

	mode_val = ucontrol->value.enumerated.item[0];

	dev_dbg(codec->dev, "%s: mode: %d\n",
		__func__, mode_val);

	if (mode_val == 0) {
		dev_warn(codec->dev, "%s:Invalid HPH Mode, default to Cls-H HiFi\n",
			__func__);
		mode_val = CLS_H_HIFI;
	}
	tasha->hph_mode = mode_val;
	return 0;
}

static const char *const tasha_conn_mad_text[] = {
	"NOTUSED1", "ADC1", "ADC2", "ADC3", "ADC4", "ADC5", "ADC6",
	"NOTUSED2", "DMIC0", "DMIC1", "DMIC2", "DMIC3", "DMIC4",
	"DMIC5", "NOTUSED3", "NOTUSED4"
};

static const struct soc_enum tasha_conn_mad_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tasha_conn_mad_text),
			    tasha_conn_mad_text);

static int tasha_enable_ldo_h_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u8 val = 0;

	if (codec)
		val = snd_soc_read(codec, WCD9335_LDOH_MODE) & 0x80;

	ucontrol->value.integer.value[0] = !!val;

	return 0;
}

static int tasha_enable_ldo_h_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int value = ucontrol->value.integer.value[0];
	bool enable;

	enable = !!value;
	if (codec)
		tasha_codec_enable_standalone_ldo_h(codec, enable);

	return 0;
}

static int tasha_mad_input_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	u8 tasha_mad_input;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	tasha_mad_input = snd_soc_read(codec,
				WCD9335_SOC_MAD_INP_SEL) & 0x0F;
	ucontrol->value.integer.value[0] = tasha_mad_input;

	dev_dbg(codec->dev,
		"%s: tasha_mad_input = %s\n", __func__,
		tasha_conn_mad_text[tasha_mad_input]);
	return 0;
}

static int tasha_mad_input_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	u8 tasha_mad_input;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_card *card = codec->component.card;
	char mad_amic_input_widget[6];
	const char *mad_input_widget;
	const char *source_widget = NULL;
	u32 adc, i, mic_bias_found = 0;
	int ret = 0;
	char *mad_input;

	tasha_mad_input = ucontrol->value.integer.value[0];

	if (tasha_mad_input >= ARRAY_SIZE(tasha_conn_mad_text)) {
		dev_err(codec->dev,
			"%s: tasha_mad_input = %d out of bounds\n",
			__func__, tasha_mad_input);
		return -EINVAL;
	}

	if (!strcmp(tasha_conn_mad_text[tasha_mad_input], "NOTUSED1") ||
	    !strcmp(tasha_conn_mad_text[tasha_mad_input], "NOTUSED2") ||
	    !strcmp(tasha_conn_mad_text[tasha_mad_input], "NOTUSED3") ||
	    !strcmp(tasha_conn_mad_text[tasha_mad_input], "NOTUSED4")) {
		dev_err(codec->dev,
			"%s: Unsupported tasha_mad_input = %s\n",
			__func__, tasha_conn_mad_text[tasha_mad_input]);
		return -EINVAL;
	}

	if (strnstr(tasha_conn_mad_text[tasha_mad_input],
		    "ADC", sizeof("ADC"))) {
		mad_input = strpbrk(tasha_conn_mad_text[tasha_mad_input],
				    "123456");
		if (!mad_input) {
			dev_err(codec->dev, "%s: Invalid MAD input %s\n",
				__func__,
				tasha_conn_mad_text[tasha_mad_input]);
			return -EINVAL;
		}
		ret = kstrtouint(mad_input, 10, &adc);
		if ((ret < 0) || (adc > 6)) {
			dev_err(codec->dev,
				"%s: Invalid ADC = %s\n", __func__,
				tasha_conn_mad_text[tasha_mad_input]);
			ret =  -EINVAL;
		}

		snprintf(mad_amic_input_widget, 6, "%s%u", "AMIC", adc);

		mad_input_widget = mad_amic_input_widget;
	} else {
		/* DMIC type input widget*/
		mad_input_widget = tasha_conn_mad_text[tasha_mad_input];
	}

	dev_dbg(codec->dev,
		"%s: tasha input widget = %s\n", __func__,
		mad_input_widget);

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
		dev_err(codec->dev,
			"%s: mic bias source not found for input = %s\n",
			__func__, mad_input_widget);
		return -EINVAL;
	}

	dev_dbg(codec->dev,
		"%s: mic_bias found = %d\n", __func__,
		mic_bias_found);

	snd_soc_update_bits(codec, WCD9335_SOC_MAD_INP_SEL,
			    0x0F, tasha_mad_input);
	snd_soc_update_bits(codec, WCD9335_ANA_MAD_SETUP,
			    0x07, mic_bias_found);

	return 0;
}

static int tasha_pinctl_mode_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u16 ctl_reg;
	u8 reg_val, pinctl_position;

	pinctl_position = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	switch (pinctl_position >> 3) {
	case 0:
		ctl_reg = WCD9335_TEST_DEBUG_PIN_CTL_OE_0;
		break;
	case 1:
		ctl_reg = WCD9335_TEST_DEBUG_PIN_CTL_OE_1;
		break;
	case 2:
		ctl_reg = WCD9335_TEST_DEBUG_PIN_CTL_OE_2;
		break;
	case 3:
		ctl_reg = WCD9335_TEST_DEBUG_PIN_CTL_OE_3;
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

static int tasha_pinctl_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u16 ctl_reg, cfg_reg;
	u8 ctl_val, cfg_val, pinctl_position, pinctl_mode, mask;

	/* 1- high or low; 0- high Z */
	pinctl_mode = ucontrol->value.integer.value[0];
	pinctl_position = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	switch (pinctl_position >> 3) {
	case 0:
		ctl_reg = WCD9335_TEST_DEBUG_PIN_CTL_OE_0;
		break;
	case 1:
		ctl_reg = WCD9335_TEST_DEBUG_PIN_CTL_OE_1;
		break;
	case 2:
		ctl_reg = WCD9335_TEST_DEBUG_PIN_CTL_OE_2;
		break;
	case 3:
		ctl_reg = WCD9335_TEST_DEBUG_PIN_CTL_OE_3;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid pinctl position = %d\n",
			__func__, pinctl_position);
		return -EINVAL;
	}

	ctl_val = pinctl_mode << (pinctl_position & 0x07);
	mask = 1 << (pinctl_position & 0x07);
	snd_soc_update_bits(codec, ctl_reg, mask, ctl_val);

	cfg_reg = WCD9335_TLMM_BIST_MODE_PINCFG + pinctl_position;
	if (!pinctl_mode) {
		if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
			cfg_val = 0x4;
		else
			cfg_val = 0xC;
	} else {
		cfg_val = 0;
	}
	snd_soc_update_bits(codec, cfg_reg, 0x07, cfg_val);

	dev_dbg(codec->dev, "%s: reg=0x%x mask=0x%x val=%d reg=0x%x val=%d\n",
			__func__, ctl_reg, mask, ctl_val, cfg_reg, cfg_val);

	return 0;
}

static void wcd_vbat_adc_out_config_2_0(struct wcd_vbat *vbat,
					struct snd_soc_codec *codec)
{
	u8 val1, val2;

	/*
	 * Measure dcp1 by using "ALT" branch of band gap
	 * voltage(Vbg) and use it in FAST mode
	 */
	snd_soc_update_bits(codec, WCD9335_BIAS_CTL, 0x82, 0x82);
	snd_soc_update_bits(codec, WCD9335_CDC_VBAT_VBAT_PATH_CTL, 0x10, 0x10);
	snd_soc_update_bits(codec, WCD9335_CDC_VBAT_VBAT_DEBUG1, 0x01, 0x01);
	snd_soc_update_bits(codec, WCD9335_ANA_VBADC, 0x80, 0x80);
	snd_soc_update_bits(codec, WCD9335_VBADC_SUBBLOCK_EN, 0x20, 0x00);

	snd_soc_update_bits(codec, WCD9335_VBADC_FE_CTRL, 0x20, 0x20);
	/* Wait 100 usec after calibration select as Vbg */
	usleep_range(100, 110);

	snd_soc_update_bits(codec, WCD9335_VBADC_ADC_IO, 0x40, 0x40);
	val1 = snd_soc_read(codec, WCD9335_VBADC_ADC_DOUTMSB);
	val2 = snd_soc_read(codec, WCD9335_VBADC_ADC_DOUTLSB);
	snd_soc_update_bits(codec, WCD9335_VBADC_ADC_IO, 0x40, 0x00);

	vbat->dcp1 = (((val1 & 0xFF) << 3) | (val2 & 0x07));

	snd_soc_update_bits(codec, WCD9335_BIAS_CTL, 0x40, 0x40);
	/* Wait 100 usec after selecting Vbg as 1.05V */
	usleep_range(100, 110);

	snd_soc_update_bits(codec, WCD9335_VBADC_ADC_IO, 0x40, 0x40);
	val1 = snd_soc_read(codec, WCD9335_VBADC_ADC_DOUTMSB);
	val2 = snd_soc_read(codec, WCD9335_VBADC_ADC_DOUTLSB);
	snd_soc_update_bits(codec, WCD9335_VBADC_ADC_IO, 0x40, 0x00);

	vbat->dcp2 = (((val1 & 0xFF) << 3) | (val2 & 0x07));

	dev_dbg(codec->dev, "%s: dcp1:0x%x, dcp2:0x%x\n",
		__func__, vbat->dcp1, vbat->dcp2);

	snd_soc_write(codec, WCD9335_BIAS_CTL, 0x28);
	/* Wait 100 usec after selecting Vbg as 0.85V */
	usleep_range(100, 110);

	snd_soc_update_bits(codec, WCD9335_VBADC_FE_CTRL, 0x20, 0x00);
	snd_soc_update_bits(codec, WCD9335_VBADC_SUBBLOCK_EN, 0x20, 0x20);
	snd_soc_update_bits(codec, WCD9335_ANA_VBADC, 0x80, 0x00);

	snd_soc_update_bits(codec, WCD9335_CDC_VBAT_VBAT_PATH_CTL, 0x10, 0x00);
	snd_soc_update_bits(codec, WCD9335_CDC_VBAT_VBAT_DEBUG1, 0x01, 0x00);
}

static void wcd_vbat_adc_out_config_1_x(struct wcd_vbat *vbat,
					struct snd_soc_codec *codec)
{
	u8 val1, val2;

	/*
	 * Measure dcp1 by applying band gap voltage(Vbg)
	 * of 0.85V
	 */
	snd_soc_write(codec, WCD9335_ANA_BIAS, 0x20);
	snd_soc_write(codec, WCD9335_BIAS_CTL, 0x28);
	snd_soc_write(codec, WCD9335_BIAS_VBG_FINE_ADJ, 0x05);
	snd_soc_write(codec, WCD9335_ANA_BIAS, 0xA0);
	/* Wait 2 sec after enabling band gap bias */
	usleep_range(2000000, 2000100);

	snd_soc_write(codec, WCD9335_ANA_CLK_TOP, 0x82);
	snd_soc_write(codec, WCD9335_ANA_CLK_TOP, 0x87);
	snd_soc_update_bits(codec, WCD9335_CDC_VBAT_VBAT_PATH_CTL, 0x10, 0x10);
	snd_soc_write(codec, WCD9335_CDC_VBAT_VBAT_CFG, 0x0D);
	snd_soc_write(codec, WCD9335_CDC_VBAT_VBAT_DEBUG1, 0x01);

	snd_soc_write(codec, WCD9335_ANA_VBADC, 0x80);
	snd_soc_write(codec, WCD9335_VBADC_SUBBLOCK_EN, 0xDE);
	snd_soc_write(codec, WCD9335_VBADC_FE_CTRL, 0x3C);
	/* Wait 1 msec after calibration select as Vbg */
	usleep_range(1000, 1100);

	snd_soc_write(codec, WCD9335_VBADC_ADC_IO, 0xC0);
	val1 = snd_soc_read(codec, WCD9335_VBADC_ADC_DOUTMSB);
	val2 = snd_soc_read(codec, WCD9335_VBADC_ADC_DOUTLSB);
	snd_soc_write(codec, WCD9335_VBADC_ADC_IO, 0x80);

	vbat->dcp1 = (((val1 & 0xFF) << 3) | (val2 & 0x07));

	/*
	 * Measure dcp2 by applying band gap voltage(Vbg)
	 * of 1.05V
	 */
	snd_soc_write(codec, WCD9335_ANA_BIAS, 0x80);
	snd_soc_write(codec, WCD9335_ANA_BIAS, 0xC0);
	snd_soc_write(codec, WCD9335_BIAS_CTL, 0x68);
	/* Wait 2 msec after selecting Vbg as 1.05V */
	usleep_range(2000, 2100);

	snd_soc_write(codec, WCD9335_ANA_BIAS, 0x80);
	/* Wait 1 sec after enabling band gap bias */
	usleep_range(1000000, 1000100);

	snd_soc_write(codec, WCD9335_VBADC_ADC_IO, 0xC0);
	val1 = snd_soc_read(codec, WCD9335_VBADC_ADC_DOUTMSB);
	val2 = snd_soc_read(codec, WCD9335_VBADC_ADC_DOUTLSB);
	snd_soc_write(codec, WCD9335_VBADC_ADC_IO, 0x80);

	vbat->dcp2 = (((val1 & 0xFF) << 3) | (val2 & 0x07));

	dev_dbg(codec->dev, "%s: dcp1:0x%x, dcp2:0x%x\n",
		__func__, vbat->dcp1, vbat->dcp2);

	/* Reset the Vbat ADC configuration */
	snd_soc_write(codec, WCD9335_ANA_BIAS, 0x80);
	snd_soc_write(codec, WCD9335_ANA_BIAS, 0xC0);

	snd_soc_write(codec, WCD9335_BIAS_CTL, 0x28);
	/* Wait 2 msec after selecting Vbg as 0.85V */
	usleep_range(2000, 2100);

	snd_soc_write(codec, WCD9335_ANA_BIAS, 0xA0);
	/* Wait 1 sec after enabling band gap bias */
	usleep_range(1000000, 1000100);

	snd_soc_write(codec, WCD9335_VBADC_FE_CTRL, 0x1C);
	snd_soc_write(codec, WCD9335_VBADC_SUBBLOCK_EN, 0xFE);
	snd_soc_write(codec, WCD9335_VBADC_ADC_IO, 0x80);
	snd_soc_write(codec, WCD9335_ANA_VBADC, 0x00);

	snd_soc_write(codec, WCD9335_CDC_VBAT_VBAT_DEBUG1, 0x00);
	snd_soc_write(codec, WCD9335_CDC_VBAT_VBAT_PATH_CTL, 0x00);
	snd_soc_write(codec, WCD9335_CDC_VBAT_VBAT_CFG, 0x0A);
}

static void wcd_vbat_adc_out_config(struct wcd_vbat *vbat,
				struct snd_soc_codec *codec)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);

	if (!vbat->adc_config) {
		tasha_cdc_mclk_enable(codec, true, false);

		if (TASHA_IS_2_0(wcd9xxx))
			wcd_vbat_adc_out_config_2_0(vbat, codec);
		else
			wcd_vbat_adc_out_config_1_x(vbat, codec);

		tasha_cdc_mclk_enable(codec, false, false);
		vbat->adc_config = true;
	}
}

static int tasha_update_vbat_reg_config(struct snd_soc_codec *codec)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct firmware_cal *hwdep_cal = NULL;
	struct vbat_monitor_reg *vbat_reg_ptr = NULL;
	const void *data;
	size_t cal_size, vbat_size_remaining;
	int ret = 0, i;
	u32 vbat_writes_size = 0;
	u16 reg;
	u8 mask, val, old_val;

	hwdep_cal = wcdcal_get_fw_cal(tasha->fw_data, WCD9XXX_VBAT_CAL);
	if (hwdep_cal) {
		data = hwdep_cal->data;
		cal_size = hwdep_cal->size;
		dev_dbg(codec->dev, "%s: using hwdep calibration\n",
			__func__);
	} else {
		dev_err(codec->dev, "%s: Vbat cal not received\n",
			__func__);
		ret = -EINVAL;
		goto done;
	}

	if (cal_size < sizeof(*vbat_reg_ptr)) {
		dev_err(codec->dev,
			"%s: Incorrect size %zd for Vbat Cal, expected %zd\n",
			__func__, cal_size, sizeof(*vbat_reg_ptr));
		ret = -EINVAL;
		goto done;
	}

	vbat_reg_ptr = (struct vbat_monitor_reg *) (data);

	if (!vbat_reg_ptr) {
		dev_err(codec->dev,
			"%s: Invalid calibration data for Vbat\n",
			__func__);
		ret = -EINVAL;
		goto done;
	}

	vbat_writes_size = vbat_reg_ptr->size;
	vbat_size_remaining = cal_size - sizeof(u32);
	dev_dbg(codec->dev, "%s: vbat_writes_sz: %d, vbat_sz_remaining: %zd\n",
			__func__, vbat_writes_size, vbat_size_remaining);

	if ((vbat_writes_size * TASHA_PACKED_REG_SIZE)
					> vbat_size_remaining) {
		pr_err("%s: Incorrect Vbat calibration data\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	for (i = 0 ; i < vbat_writes_size; i++) {
		TASHA_CODEC_UNPACK_ENTRY(vbat_reg_ptr->writes[i],
					reg, mask, val);
		old_val = snd_soc_read(codec, reg);
		snd_soc_write(codec, reg, (old_val & ~mask) | (val & mask));
	}

done:
	return ret;
}

static int tasha_vbat_adc_data_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	wcd_vbat_adc_out_config(&tasha->vbat, codec);

	ucontrol->value.integer.value[0] = tasha->vbat.dcp1;
	ucontrol->value.integer.value[1] = tasha->vbat.dcp2;

	dev_dbg(codec->dev,
		"%s: Vbat ADC output values, Dcp1 : %lu, Dcp2: %lu\n",
		__func__, ucontrol->value.integer.value[0],
		ucontrol->value.integer.value[1]);

	return 0;
}

static const char * const tasha_vbat_gsm_mode_text[] = {
	"OFF", "ON"};

static const struct soc_enum tasha_vbat_gsm_mode_enum =
	SOC_ENUM_SINGLE_EXT(2, tasha_vbat_gsm_mode_text);

static int tasha_vbat_gsm_mode_func_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	ucontrol->value.integer.value[0] =
		((snd_soc_read(codec, WCD9335_CDC_VBAT_VBAT_CFG) & 0x04) ?
		  1 : 0);

	dev_dbg(codec->dev, "%s: value: %lu\n", __func__,
		ucontrol->value.integer.value[0]);

	return 0;
}

static int tasha_vbat_gsm_mode_func_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	dev_dbg(codec->dev, "%s: value: %lu\n", __func__,
		ucontrol->value.integer.value[0]);

	/* Set Vbat register configuration for GSM mode bit based on value */
	if (ucontrol->value.integer.value[0])
		snd_soc_update_bits(codec, WCD9335_CDC_VBAT_VBAT_CFG,
						0x04, 0x04);
	else
		snd_soc_update_bits(codec, WCD9335_CDC_VBAT_VBAT_CFG,
						0x04, 0x00);

	return 0;
}

static int tasha_codec_vbat_enable_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u16 vbat_path_ctl, vbat_cfg, vbat_path_cfg;

	vbat_path_ctl = WCD9335_CDC_VBAT_VBAT_PATH_CTL;
	vbat_cfg = WCD9335_CDC_VBAT_VBAT_CFG;
	vbat_path_cfg = WCD9335_CDC_RX8_RX_PATH_CFG1;

	if (!strcmp(w->name, "RX INT8 VBAT"))
		vbat_path_cfg = WCD9335_CDC_RX8_RX_PATH_CFG1;
	else if (!strcmp(w->name, "RX INT7 VBAT"))
		vbat_path_cfg = WCD9335_CDC_RX7_RX_PATH_CFG1;
	else if (!strcmp(w->name, "RX INT6 VBAT"))
		vbat_path_cfg = WCD9335_CDC_RX6_RX_PATH_CFG1;
	else if (!strcmp(w->name, "RX INT5 VBAT"))
		vbat_path_cfg = WCD9335_CDC_RX5_RX_PATH_CFG1;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = tasha_update_vbat_reg_config(codec);
		if (ret) {
			dev_dbg(codec->dev,
				"%s : VBAT isn't calibrated, So not enabling it\n",
				__func__);
			return 0;
		}
		snd_soc_write(codec, WCD9335_ANA_VBADC, 0x80);
		snd_soc_update_bits(codec, vbat_path_cfg, 0x02, 0x02);
		snd_soc_update_bits(codec, vbat_path_ctl, 0x10, 0x10);
		snd_soc_update_bits(codec, vbat_cfg, 0x01, 0x01);
		tasha->vbat.is_enabled = true;
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (tasha->vbat.is_enabled) {
			snd_soc_update_bits(codec, vbat_cfg, 0x01, 0x00);
			snd_soc_update_bits(codec, vbat_path_ctl, 0x10, 0x00);
			snd_soc_update_bits(codec, vbat_path_cfg, 0x02, 0x00);
			snd_soc_write(codec, WCD9335_ANA_VBADC, 0x00);
			tasha->vbat.is_enabled = false;
		}
		break;
	};

	return ret;
}

static const char * const rx_hph_mode_mux_text[] = {
	"CLS_H_INVALID", "CLS_H_HIFI", "CLS_H_LP", "CLS_AB", "CLS_H_LOHIFI"
};

static const struct soc_enum rx_hph_mode_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_hph_mode_mux_text),
			    rx_hph_mode_mux_text);

static const char * const amic_pwr_lvl_text[] = {
	"LOW_PWR", "DEFAULT", "HIGH_PERF"
};

static const struct soc_enum amic_pwr_lvl_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amic_pwr_lvl_text),
			    amic_pwr_lvl_text);

static const struct snd_kcontrol_new tasha_snd_controls[] = {
	SOC_SINGLE_SX_TLV("RX0 Digital Volume", WCD9335_CDC_RX0_RX_VOL_CTL,
		0, -84, 40, digital_gain), /* -84dB min - 40dB max */
	SOC_SINGLE_SX_TLV("RX1 Digital Volume", WCD9335_CDC_RX1_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX2 Digital Volume", WCD9335_CDC_RX2_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX3 Digital Volume", WCD9335_CDC_RX3_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX4 Digital Volume", WCD9335_CDC_RX4_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX5 Digital Volume", WCD9335_CDC_RX5_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX6 Digital Volume", WCD9335_CDC_RX6_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX7 Digital Volume", WCD9335_CDC_RX7_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX8 Digital Volume", WCD9335_CDC_RX8_RX_VOL_CTL,
		0, -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("RX0 Mix Digital Volume",
			  WCD9335_CDC_RX0_RX_VOL_MIX_CTL,
			  0, -84, 40, digital_gain), /* -84dB min - 40dB max */
	SOC_SINGLE_SX_TLV("RX1 Mix Digital Volume",
			  WCD9335_CDC_RX1_RX_VOL_MIX_CTL,
			  0, -84, 40, digital_gain), /* -84dB min - 40dB max */
	SOC_SINGLE_SX_TLV("RX2 Mix Digital Volume",
			  WCD9335_CDC_RX2_RX_VOL_MIX_CTL,
			  0, -84, 40, digital_gain), /* -84dB min - 40dB max */
	SOC_SINGLE_SX_TLV("RX3 Mix Digital Volume",
			  WCD9335_CDC_RX3_RX_VOL_MIX_CTL,
			  0, -84, 40, digital_gain), /* -84dB min - 40dB max */
	SOC_SINGLE_SX_TLV("RX4 Mix Digital Volume",
			  WCD9335_CDC_RX4_RX_VOL_MIX_CTL,
			  0, -84, 40, digital_gain), /* -84dB min - 40dB max */
	SOC_SINGLE_SX_TLV("RX5 Mix Digital Volume",
			  WCD9335_CDC_RX5_RX_VOL_MIX_CTL,
			  0, -84, 40, digital_gain), /* -84dB min - 40dB max */
	SOC_SINGLE_SX_TLV("RX6 Mix Digital Volume",
			  WCD9335_CDC_RX6_RX_VOL_MIX_CTL,
			  0, -84, 40, digital_gain), /* -84dB min - 40dB max */
	SOC_SINGLE_SX_TLV("RX7 Mix Digital Volume",
			  WCD9335_CDC_RX7_RX_VOL_MIX_CTL,
			  0, -84, 40, digital_gain), /* -84dB min - 40dB max */
	SOC_SINGLE_SX_TLV("RX8 Mix Digital Volume",
			  WCD9335_CDC_RX8_RX_VOL_MIX_CTL,
			  0, -84, 40, digital_gain), /* -84dB min - 40dB max */

	SOC_SINGLE_SX_TLV("DEC0 Volume", WCD9335_CDC_TX0_TX_VOL_CTL, 0,
					  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC1 Volume", WCD9335_CDC_TX1_TX_VOL_CTL, 0,
					  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC2 Volume", WCD9335_CDC_TX2_TX_VOL_CTL, 0,
					  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC3 Volume", WCD9335_CDC_TX3_TX_VOL_CTL, 0,
					  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC4 Volume", WCD9335_CDC_TX4_TX_VOL_CTL, 0,
					  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC5 Volume", WCD9335_CDC_TX5_TX_VOL_CTL, 0,
					  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC6 Volume", WCD9335_CDC_TX6_TX_VOL_CTL, 0,
					  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC7 Volume", WCD9335_CDC_TX7_TX_VOL_CTL, 0,
					  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC8 Volume", WCD9335_CDC_TX8_TX_VOL_CTL, 0,
					  -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("IIR0 INP0 Volume",
			  WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL, 0, -84,
			  40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR0 INP1 Volume",
			  WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL, 0, -84,
			  40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR0 INP2 Volume",
			  WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL, 0, -84,
			  40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR0 INP3 Volume",
			  WCD9335_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL, 0, -84,
			  40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP0 Volume",
			  WCD9335_CDC_SIDETONE_IIR1_IIR_GAIN_B1_CTL, 0, -84,
			  40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP1 Volume",
			  WCD9335_CDC_SIDETONE_IIR1_IIR_GAIN_B2_CTL, 0, -84,
			  40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP2 Volume",
			  WCD9335_CDC_SIDETONE_IIR1_IIR_GAIN_B3_CTL, 0, -84,
			  40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP3 Volume",
			  WCD9335_CDC_SIDETONE_IIR1_IIR_GAIN_B4_CTL, 0, -84,
			  40, digital_gain),

	SOC_SINGLE_EXT("ANC Slot", SND_SOC_NOPM, 0, 100, 0, tasha_get_anc_slot,
		       tasha_put_anc_slot),
	SOC_ENUM_EXT("ANC Function", tasha_anc_func_enum, tasha_get_anc_func,
		     tasha_put_anc_func),

	SOC_ENUM_EXT("CLK MODE", tasha_clkmode_enum, tasha_get_clkmode,
		     tasha_put_clkmode),

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
	SOC_ENUM("RX INT5_1 HPF cut off", cf_int5_1_enum),
	SOC_ENUM("RX INT5_2 HPF cut off", cf_int5_2_enum),
	SOC_ENUM("RX INT6_1 HPF cut off", cf_int6_1_enum),
	SOC_ENUM("RX INT6_2 HPF cut off", cf_int6_2_enum),
	SOC_ENUM("RX INT7_1 HPF cut off", cf_int7_1_enum),
	SOC_ENUM("RX INT7_2 HPF cut off", cf_int7_2_enum),
	SOC_ENUM("RX INT8_1 HPF cut off", cf_int8_1_enum),
	SOC_ENUM("RX INT8_2 HPF cut off", cf_int8_2_enum),

	SOC_SINGLE_EXT("IIR0 Enable Band1", IIR0, BAND1, 1, 0,
	tasha_get_iir_enable_audio_mixer, tasha_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR0 Enable Band2", IIR0, BAND2, 1, 0,
	tasha_get_iir_enable_audio_mixer, tasha_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR0 Enable Band3", IIR0, BAND3, 1, 0,
	tasha_get_iir_enable_audio_mixer, tasha_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR0 Enable Band4", IIR0, BAND4, 1, 0,
	tasha_get_iir_enable_audio_mixer, tasha_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR0 Enable Band5", IIR0, BAND5, 1, 0,
	tasha_get_iir_enable_audio_mixer, tasha_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
	tasha_get_iir_enable_audio_mixer, tasha_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
	tasha_get_iir_enable_audio_mixer, tasha_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
	tasha_get_iir_enable_audio_mixer, tasha_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
	tasha_get_iir_enable_audio_mixer, tasha_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
	tasha_get_iir_enable_audio_mixer, tasha_put_iir_enable_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR0 Band1", IIR0, BAND1, 255, 0, 5,
	tasha_get_iir_band_audio_mixer, tasha_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR0 Band2", IIR0, BAND2, 255, 0, 5,
	tasha_get_iir_band_audio_mixer, tasha_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR0 Band3", IIR0, BAND3, 255, 0, 5,
	tasha_get_iir_band_audio_mixer, tasha_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR0 Band4", IIR0, BAND4, 255, 0, 5,
	tasha_get_iir_band_audio_mixer, tasha_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR0 Band5", IIR0, BAND5, 255, 0, 5,
	tasha_get_iir_band_audio_mixer, tasha_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
	tasha_get_iir_band_audio_mixer, tasha_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
	tasha_get_iir_band_audio_mixer, tasha_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
	tasha_get_iir_band_audio_mixer, tasha_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
	tasha_get_iir_band_audio_mixer, tasha_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
	tasha_get_iir_band_audio_mixer, tasha_put_iir_band_audio_mixer),

	SOC_SINGLE_EXT("COMP1 Switch", SND_SOC_NOPM, COMPANDER_1, 1, 0,
		       tasha_get_compander, tasha_set_compander),
	SOC_SINGLE_EXT("COMP2 Switch", SND_SOC_NOPM, COMPANDER_2, 1, 0,
		       tasha_get_compander, tasha_set_compander),
	SOC_SINGLE_EXT("COMP3 Switch", SND_SOC_NOPM, COMPANDER_3, 1, 0,
		       tasha_get_compander, tasha_set_compander),
	SOC_SINGLE_EXT("COMP4 Switch", SND_SOC_NOPM, COMPANDER_4, 1, 0,
		       tasha_get_compander, tasha_set_compander),
	SOC_SINGLE_EXT("COMP5 Switch", SND_SOC_NOPM, COMPANDER_5, 1, 0,
		       tasha_get_compander, tasha_set_compander),
	SOC_SINGLE_EXT("COMP6 Switch", SND_SOC_NOPM, COMPANDER_6, 1, 0,
		       tasha_get_compander, tasha_set_compander),
	SOC_SINGLE_EXT("COMP7 Switch", SND_SOC_NOPM, COMPANDER_7, 1, 0,
		       tasha_get_compander, tasha_set_compander),
	SOC_SINGLE_EXT("COMP8 Switch", SND_SOC_NOPM, COMPANDER_8, 1, 0,
		       tasha_get_compander, tasha_set_compander),

	SOC_ENUM_EXT("RX HPH Mode", rx_hph_mode_mux_enum,
		       tasha_rx_hph_mode_get, tasha_rx_hph_mode_put),

	SOC_ENUM_EXT("MAD Input", tasha_conn_mad_enum,
		     tasha_mad_input_get, tasha_mad_input_put),
	SOC_SINGLE_EXT("LDO_H Enable", SND_SOC_NOPM, 0, 1, 0,
			tasha_enable_ldo_h_get, tasha_enable_ldo_h_put),

	SOC_SINGLE_EXT("DMIC1_CLK_PIN_MODE", SND_SOC_NOPM, 17, 1, 0,
		       tasha_pinctl_mode_get, tasha_pinctl_mode_put),

	SOC_SINGLE_EXT("DMIC1_DATA_PIN_MODE", SND_SOC_NOPM, 18, 1, 0,
		       tasha_pinctl_mode_get, tasha_pinctl_mode_put),

	SOC_SINGLE_EXT("DMIC2_CLK_PIN_MODE", SND_SOC_NOPM, 19, 1, 0,
		       tasha_pinctl_mode_get, tasha_pinctl_mode_put),

	SOC_SINGLE_EXT("DMIC2_DATA_PIN_MODE", SND_SOC_NOPM, 20, 1, 0,
		       tasha_pinctl_mode_get, tasha_pinctl_mode_put),

	SOC_SINGLE_EXT("DMIC3_CLK_PIN_MODE", SND_SOC_NOPM, 21, 1, 0,
		       tasha_pinctl_mode_get, tasha_pinctl_mode_put),

	SOC_SINGLE_EXT("DMIC3_DATA_PIN_MODE", SND_SOC_NOPM, 22, 1, 0,
		       tasha_pinctl_mode_get, tasha_pinctl_mode_put),
	SOC_ENUM_EXT("AMIC_1_2 PWR MODE", amic_pwr_lvl_enum,
		       tasha_amic_pwr_lvl_get, tasha_amic_pwr_lvl_put),
	SOC_ENUM_EXT("AMIC_3_4 PWR MODE", amic_pwr_lvl_enum,
		       tasha_amic_pwr_lvl_get, tasha_amic_pwr_lvl_put),
	SOC_ENUM_EXT("AMIC_5_6 PWR MODE", amic_pwr_lvl_enum,
		       tasha_amic_pwr_lvl_get, tasha_amic_pwr_lvl_put),

	SOC_SINGLE_MULTI_EXT("Vbat ADC data", SND_SOC_NOPM, 0, 0xFFFF, 0, 2,
			tasha_vbat_adc_data_get, NULL),

	SOC_ENUM_EXT("GSM mode Enable", tasha_vbat_gsm_mode_enum,
			tasha_vbat_gsm_mode_func_get,
			tasha_vbat_gsm_mode_func_put),
};

static int tasha_put_dec_enum(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	u16 mic_sel_reg;
	u8 mic_sel;

	val = ucontrol->value.enumerated.item[0];
	if (val > e->items - 1)
		return -EINVAL;

	dev_dbg(codec->dev, "%s: wname: %s, val: 0x%x\n", __func__,
		widget->name, val);

	switch (e->reg) {
	case WCD9335_CDC_TX_INP_MUX_ADC_MUX0_CFG1:
		mic_sel_reg = WCD9335_CDC_TX0_TX_PATH_CFG0;
		break;
	case WCD9335_CDC_TX_INP_MUX_ADC_MUX1_CFG1:
		mic_sel_reg = WCD9335_CDC_TX1_TX_PATH_CFG0;
		break;
	case WCD9335_CDC_TX_INP_MUX_ADC_MUX2_CFG1:
		mic_sel_reg = WCD9335_CDC_TX2_TX_PATH_CFG0;
		break;
	case WCD9335_CDC_TX_INP_MUX_ADC_MUX3_CFG1:
		mic_sel_reg = WCD9335_CDC_TX3_TX_PATH_CFG0;
		break;
	case WCD9335_CDC_TX_INP_MUX_ADC_MUX4_CFG0:
		mic_sel_reg = WCD9335_CDC_TX4_TX_PATH_CFG0;
		break;
	case WCD9335_CDC_TX_INP_MUX_ADC_MUX5_CFG0:
		mic_sel_reg = WCD9335_CDC_TX5_TX_PATH_CFG0;
		break;
	case WCD9335_CDC_TX_INP_MUX_ADC_MUX6_CFG0:
		mic_sel_reg = WCD9335_CDC_TX6_TX_PATH_CFG0;
		break;
	case WCD9335_CDC_TX_INP_MUX_ADC_MUX7_CFG0:
		mic_sel_reg = WCD9335_CDC_TX7_TX_PATH_CFG0;
		break;
	case WCD9335_CDC_TX_INP_MUX_ADC_MUX8_CFG0:
		mic_sel_reg = WCD9335_CDC_TX8_TX_PATH_CFG0;
		break;
	default:
		dev_err(codec->dev, "%s: e->reg: 0x%x not expected\n",
			__func__, e->reg);
		return -EINVAL;
	}

	/* ADC: 0, DMIC: 1 */
	mic_sel = val ? 0x0 : 0x1;
	snd_soc_update_bits(codec, mic_sel_reg, 1 << 7, mic_sel << 7);

	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static int tasha_int_dem_inp_mux_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	unsigned short look_ahead_dly_reg = WCD9335_CDC_RX0_RX_PATH_CFG0;

	val = ucontrol->value.enumerated.item[0];
	if (val >= e->items)
		return -EINVAL;

	dev_dbg(codec->dev, "%s: wname: %s, val: 0x%x\n", __func__,
		widget->name, val);

	if (e->reg == WCD9335_CDC_RX0_RX_PATH_SEC0)
		look_ahead_dly_reg = WCD9335_CDC_RX0_RX_PATH_CFG0;
	else if (e->reg == WCD9335_CDC_RX1_RX_PATH_SEC0)
		look_ahead_dly_reg = WCD9335_CDC_RX1_RX_PATH_CFG0;
	else if (e->reg == WCD9335_CDC_RX2_RX_PATH_SEC0)
		look_ahead_dly_reg = WCD9335_CDC_RX2_RX_PATH_CFG0;

	/* Set Look Ahead Delay */
	snd_soc_update_bits(codec, look_ahead_dly_reg,
			    0x08, (val ? 0x08 : 0x00));
	/* Set DEM INP Select */
	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static int tasha_ear_pa_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	ear_pa_gain = snd_soc_read(codec, WCD9335_ANA_EAR);

	ear_pa_gain = (ear_pa_gain & 0x70) >> 4;

	ucontrol->value.integer.value[0] = ear_pa_gain;

	dev_dbg(codec->dev, "%s: ear_pa_gain = 0x%x\n", __func__,
		ear_pa_gain);

	return 0;
}

static int tasha_ear_pa_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			__func__, ucontrol->value.integer.value[0]);

	ear_pa_gain =  ucontrol->value.integer.value[0] << 4;

	snd_soc_update_bits(codec, WCD9335_ANA_EAR, 0x70, ear_pa_gain);
	return 0;
}

static int tasha_ear_spkr_pa_gain_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tasha->ear_spkr_gain;

	dev_dbg(codec->dev, "%s: ear_spkr_gain = %ld\n", __func__,
		ucontrol->value.integer.value[0]);

	return 0;
}

static int tasha_ear_spkr_pa_gain_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	tasha->ear_spkr_gain =  ucontrol->value.integer.value[0];

	return 0;
}

static int tasha_spkr_left_boost_stage_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max = 0;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	bst_state_max = snd_soc_read(codec, WCD9335_CDC_BOOST0_BOOST_CTL);
	bst_state_max = (bst_state_max & 0x0c) >> 2;
	ucontrol->value.integer.value[0] = bst_state_max;
	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int tasha_spkr_left_boost_stage_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	bst_state_max =  ucontrol->value.integer.value[0] << 2;
	snd_soc_update_bits(codec, WCD9335_CDC_BOOST0_BOOST_CTL,
		0x0c, bst_state_max);

	return 0;
}

static int tasha_spkr_right_boost_stage_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max = 0;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	bst_state_max = snd_soc_read(codec, WCD9335_CDC_BOOST1_BOOST_CTL);
	bst_state_max = (bst_state_max & 0x0c) >> 2;
	ucontrol->value.integer.value[0] = bst_state_max;
	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int tasha_spkr_right_boost_stage_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	bst_state_max =  ucontrol->value.integer.value[0] << 2;
	snd_soc_update_bits(codec, WCD9335_CDC_BOOST1_BOOST_CTL,
		0x0c, bst_state_max);

	return 0;
}

static int tasha_config_compander(struct snd_soc_codec *codec, int interp_n,
				  int event)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int comp;
	u16 comp_ctl0_reg, rx_path_cfg0_reg;

	/* EAR does not have compander */
	if (!interp_n)
		return 0;

	comp = interp_n - 1;
	dev_dbg(codec->dev, "%s: event %d compander %d, enabled %d\n",
		__func__, event, comp + 1, tasha->comp_enabled[comp]);

	if (!tasha->comp_enabled[comp])
		return 0;

	comp_ctl0_reg = WCD9335_CDC_COMPANDER1_CTL0 + (comp * 8);
	rx_path_cfg0_reg = WCD9335_CDC_RX1_RX_PATH_CFG0 + (comp * 20);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Enable Compander Clock */
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x01, 0x01);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x02);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, rx_path_cfg0_reg, 0x02, 0x02);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x04, 0x04);
		snd_soc_update_bits(codec, rx_path_cfg0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x02);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x01, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x04, 0x00);
	}

	return 0;
}

static int tasha_codec_config_mad(struct snd_soc_codec *codec)
{
	int ret = 0;
	int idx;
	const struct firmware *fw;
	struct firmware_cal *hwdep_cal = NULL;
	struct wcd_mad_audio_cal *mad_cal = NULL;
	const void *data;
	const char *filename = TASHA_MAD_AUDIO_FIRMWARE_PATH;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	size_t cal_size;

	hwdep_cal = wcdcal_get_fw_cal(tasha->fw_data, WCD9XXX_MAD_CAL);
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

	snd_soc_write(codec, WCD9335_SOC_MAD_MAIN_CTL_2,
		      mad_cal->microphone_info.cycle_time);
	snd_soc_update_bits(codec, WCD9335_SOC_MAD_MAIN_CTL_1, 0xFF << 3,
			    ((uint16_t)mad_cal->microphone_info.settle_time)
			    << 3);

	/* Audio */
	snd_soc_write(codec, WCD9335_SOC_MAD_AUDIO_CTL_8,
		      mad_cal->audio_info.rms_omit_samples);
	snd_soc_update_bits(codec, WCD9335_SOC_MAD_AUDIO_CTL_1,
			    0x07 << 4, mad_cal->audio_info.rms_comp_time << 4);
	snd_soc_update_bits(codec, WCD9335_SOC_MAD_AUDIO_CTL_2, 0x03 << 2,
			    mad_cal->audio_info.detection_mechanism << 2);
	snd_soc_write(codec, WCD9335_SOC_MAD_AUDIO_CTL_7,
		      mad_cal->audio_info.rms_diff_threshold & 0x3F);
	snd_soc_write(codec, WCD9335_SOC_MAD_AUDIO_CTL_5,
		      mad_cal->audio_info.rms_threshold_lsb);
	snd_soc_write(codec, WCD9335_SOC_MAD_AUDIO_CTL_6,
		      mad_cal->audio_info.rms_threshold_msb);

	for (idx = 0; idx < ARRAY_SIZE(mad_cal->audio_info.iir_coefficients);
	     idx++) {
		snd_soc_update_bits(codec, WCD9335_SOC_MAD_AUDIO_IIR_CTL_PTR,
				    0x3F, idx);
		snd_soc_write(codec, WCD9335_SOC_MAD_AUDIO_IIR_CTL_VAL,
			      mad_cal->audio_info.iir_coefficients[idx]);
		dev_dbg(codec->dev, "%s:MAD Audio IIR Coef[%d] = 0X%x",
			__func__, idx,
			mad_cal->audio_info.iir_coefficients[idx]);
	}

	/* Beacon */
	snd_soc_write(codec, WCD9335_SOC_MAD_BEACON_CTL_8,
		      mad_cal->beacon_info.rms_omit_samples);
	snd_soc_update_bits(codec, WCD9335_SOC_MAD_BEACON_CTL_1,
			    0x07 << 4, mad_cal->beacon_info.rms_comp_time << 4);
	snd_soc_update_bits(codec, WCD9335_SOC_MAD_BEACON_CTL_2, 0x03 << 2,
			    mad_cal->beacon_info.detection_mechanism << 2);
	snd_soc_write(codec, WCD9335_SOC_MAD_BEACON_CTL_7,
		      mad_cal->beacon_info.rms_diff_threshold & 0x1F);
	snd_soc_write(codec, WCD9335_SOC_MAD_BEACON_CTL_5,
		      mad_cal->beacon_info.rms_threshold_lsb);
	snd_soc_write(codec, WCD9335_SOC_MAD_BEACON_CTL_6,
		      mad_cal->beacon_info.rms_threshold_msb);

	for (idx = 0; idx < ARRAY_SIZE(mad_cal->beacon_info.iir_coefficients);
	     idx++) {
		snd_soc_update_bits(codec, WCD9335_SOC_MAD_BEACON_IIR_CTL_PTR,
				    0x3F, idx);
		snd_soc_write(codec, WCD9335_SOC_MAD_BEACON_IIR_CTL_VAL,
			      mad_cal->beacon_info.iir_coefficients[idx]);
		dev_dbg(codec->dev, "%s:MAD Beacon IIR Coef[%d] = 0X%x",
			__func__, idx,
			mad_cal->beacon_info.iir_coefficients[idx]);
	}

	/* Ultrasound */
	snd_soc_update_bits(codec, WCD9335_SOC_MAD_ULTR_CTL_1,
			    0x07 << 4,
			    mad_cal->ultrasound_info.rms_comp_time << 4);
	snd_soc_update_bits(codec, WCD9335_SOC_MAD_ULTR_CTL_2, 0x03 << 2,
			    mad_cal->ultrasound_info.detection_mechanism << 2);
	snd_soc_write(codec, WCD9335_SOC_MAD_ULTR_CTL_7,
		      mad_cal->ultrasound_info.rms_diff_threshold & 0x1F);
	snd_soc_write(codec, WCD9335_SOC_MAD_ULTR_CTL_5,
		      mad_cal->ultrasound_info.rms_threshold_lsb);
	snd_soc_write(codec, WCD9335_SOC_MAD_ULTR_CTL_6,
		      mad_cal->ultrasound_info.rms_threshold_msb);

done:
	if (!hwdep_cal)
		release_firmware(fw);

	return ret;
}

static int tasha_codec_enable_mad(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	dev_dbg(codec->dev,
		"%s: event = %d\n", __func__, event);

	/* Return if CPE INPUT is DEC1 */
	if (snd_soc_read(codec, WCD9335_CPE_SS_SVA_CFG) & 0x01)
		return ret;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:

		/* Turn on MAD clk */
		snd_soc_update_bits(codec, WCD9335_CPE_SS_MAD_CTL,
				    0x01, 0x01);

		/* Undo reset for MAD */
		snd_soc_update_bits(codec, WCD9335_CPE_SS_MAD_CTL,
				    0x02, 0x00);
		ret = tasha_codec_config_mad(codec);
		if (ret)
			dev_err(codec->dev,
				"%s: Failed to config MAD, err = %d\n",
				__func__, ret);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Reset the MAD block */
		snd_soc_update_bits(codec, WCD9335_CPE_SS_MAD_CTL,
				    0x02, 0x02);
		/* Turn off MAD clk */
		snd_soc_update_bits(codec, WCD9335_CPE_SS_MAD_CTL,
				    0x01, 0x00);
		break;
	}

	return ret;
}

static int tasha_codec_configure_cpe_input(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev,
		"%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Configure CPE input as DEC1 */
		snd_soc_update_bits(codec, WCD9335_CPE_SS_SVA_CFG,
				    0x01, 0x01);

		/* Configure DEC1 Tx out with sample rate as 16K */
		snd_soc_update_bits(codec, WCD9335_CDC_TX1_TX_PATH_CTL,
				    0x0F, 0x01);

		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Reset DEC1 Tx out sample rate */
		snd_soc_update_bits(codec, WCD9335_CDC_TX1_TX_PATH_CTL,
				    0x0F, 0x04);
		snd_soc_update_bits(codec, WCD9335_CPE_SS_SVA_CFG,
				    0x01, 0x00);

		break;
	}

	return 0;
}


static int tasha_codec_aif4_mixer_switch_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);

	if (test_bit(AIF4_SWITCH_VALUE, &tasha_p->status_mask))
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;

	dev_dbg(codec->dev, "%s: AIF4 switch value = %ld\n",
		__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int tasha_codec_aif4_mixer_switch_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_dapm_update *update = NULL;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: AIF4 switch value = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0]) {
		snd_soc_dapm_mixer_update_power(widget->dapm,
						kcontrol, 1, update);
		set_bit(AIF4_SWITCH_VALUE, &tasha_p->status_mask);
	} else {
		snd_soc_dapm_mixer_update_power(widget->dapm,
						kcontrol, 0, update);
		clear_bit(AIF4_SWITCH_VALUE, &tasha_p->status_mask);
	}

	return 1;
}

static const char * const tasha_ear_pa_gain_text[] = {
	"G_6_DB", "G_4P5_DB", "G_3_DB", "G_1P5_DB",
	"G_0_DB", "G_M2P5_DB", "UNDEFINED", "G_M12_DB"
};

static const char * const tasha_ear_spkr_pa_gain_text[] = {
	"G_DEFAULT", "G_0_DB", "G_1_DB", "G_2_DB", "G_3_DB", "G_4_DB",
	"G_5_DB", "G_6_DB"
};

static const char * const tasha_speaker_boost_stage_text[] = {
	"NO_MAX_STATE", "MAX_STATE_1", "MAX_STATE_2"
};

static const struct soc_enum tasha_ear_pa_gain_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tasha_ear_pa_gain_text),
			tasha_ear_pa_gain_text);

static const struct soc_enum tasha_ear_spkr_pa_gain_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tasha_ear_spkr_pa_gain_text),
			    tasha_ear_spkr_pa_gain_text);

static const struct soc_enum tasha_spkr_boost_stage_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tasha_speaker_boost_stage_text),
			    tasha_speaker_boost_stage_text);

static const struct snd_kcontrol_new tasha_analog_gain_controls[] = {
	SOC_ENUM_EXT("EAR PA Gain", tasha_ear_pa_gain_enum,
		tasha_ear_pa_gain_get, tasha_ear_pa_gain_put),

	SOC_SINGLE_TLV("HPHL Volume", WCD9335_HPH_L_EN, 0, 20, 1,
		line_gain),
	SOC_SINGLE_TLV("HPHR Volume", WCD9335_HPH_R_EN, 0, 20, 1,
		line_gain),
	SOC_SINGLE_TLV("LINEOUT1 Volume", WCD9335_DIFF_LO_LO1_COMPANDER,
			3, 16, 1, line_gain),
	SOC_SINGLE_TLV("LINEOUT2 Volume", WCD9335_DIFF_LO_LO2_COMPANDER,
			3, 16, 1, line_gain),
	SOC_SINGLE_TLV("LINEOUT3 Volume", WCD9335_SE_LO_LO3_GAIN, 0, 20, 1,
			line_gain),
	SOC_SINGLE_TLV("LINEOUT4 Volume", WCD9335_SE_LO_LO4_GAIN, 0, 20, 1,
			line_gain),

	SOC_SINGLE_TLV("ADC1 Volume", WCD9335_ANA_AMIC1, 0, 20, 0,
			analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", WCD9335_ANA_AMIC2, 0, 20, 0,
			analog_gain),
	SOC_SINGLE_TLV("ADC3 Volume", WCD9335_ANA_AMIC3, 0, 20, 0,
			analog_gain),
	SOC_SINGLE_TLV("ADC4 Volume", WCD9335_ANA_AMIC4, 0, 20, 0,
			analog_gain),
	SOC_SINGLE_TLV("ADC5 Volume", WCD9335_ANA_AMIC5, 0, 20, 0,
			analog_gain),
	SOC_SINGLE_TLV("ADC6 Volume", WCD9335_ANA_AMIC6, 0, 20, 0,
			analog_gain),
};

static const struct snd_kcontrol_new tasha_spkr_wsa_controls[] = {
	SOC_ENUM_EXT("EAR SPKR PA Gain", tasha_ear_spkr_pa_gain_enum,
		     tasha_ear_spkr_pa_gain_get, tasha_ear_spkr_pa_gain_put),

	SOC_ENUM_EXT("SPKR Left Boost Max State", tasha_spkr_boost_stage_enum,
			tasha_spkr_left_boost_stage_get,
			tasha_spkr_left_boost_stage_put),

	SOC_ENUM_EXT("SPKR Right Boost Max State", tasha_spkr_boost_stage_enum,
			tasha_spkr_right_boost_stage_get,
			tasha_spkr_right_boost_stage_put),
};

static const char * const spl_src0_mux_text[] = {
	"ZERO", "SRC_IN_HPHL", "SRC_IN_LO1",
};

static const char * const spl_src1_mux_text[] = {
	"ZERO", "SRC_IN_HPHR", "SRC_IN_LO2",
};

static const char * const spl_src2_mux_text[] = {
	"ZERO", "SRC_IN_LO3", "SRC_IN_SPKRL",
};

static const char * const spl_src3_mux_text[] = {
	"ZERO", "SRC_IN_LO4", "SRC_IN_SPKRR",
};

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

static const char * const sb_tx0_mux_text[] = {
	"ZERO", "RX_MIX_TX0", "DEC0", "DEC0_192"
};

static const char * const sb_tx1_mux_text[] = {
	"ZERO", "RX_MIX_TX1", "DEC1", "DEC1_192"
};

static const char * const sb_tx2_mux_text[] = {
	"ZERO", "RX_MIX_TX2", "DEC2", "DEC2_192"
};

static const char * const sb_tx3_mux_text[] = {
	"ZERO", "RX_MIX_TX3", "DEC3", "DEC3_192"
};

static const char * const sb_tx4_mux_text[] = {
	"ZERO", "RX_MIX_TX4", "DEC4", "DEC4_192"
};

static const char * const sb_tx5_mux_text[] = {
	"ZERO", "RX_MIX_TX5", "DEC5", "DEC5_192"
};

static const char * const sb_tx6_mux_text[] = {
	"ZERO", "RX_MIX_TX6", "DEC6", "DEC6_192"
};

static const char * const sb_tx7_mux_text[] = {
	"ZERO", "RX_MIX_TX7", "DEC7", "DEC7_192"
};

static const char * const sb_tx8_mux_text[] = {
	"ZERO", "RX_MIX_TX8", "DEC8", "DEC8_192"
};

static const char * const sb_tx9_mux_text[] = {
	"ZERO", "DEC7", "DEC7_192"
};

static const char * const sb_tx10_mux_text[] = {
	"ZERO", "DEC6", "DEC6_192"
};

static const char * const sb_tx11_mux_text[] = {
	"DEC_0_5", "DEC_9_12", "MAD_AUDIO", "MAD_BRDCST"
};

static const char * const sb_tx11_inp1_mux_text[] = {
	"ZERO", "DEC0", "DEC1", "DEC2", "DEC3", "DEC4",
	"DEC5", "RX_MIX_TX5", "DEC9_10", "DEC11_12"
};

static const char * const sb_tx13_mux_text[] = {
	"ZERO", "DEC5", "DEC5_192"
};

static const char * const tx13_inp_mux_text[] = {
	"CDC_DEC_5", "MAD_BRDCST", "CPE_TX_PP"
};

static const char * const iir_inp_mux_text[] = {
	"ZERO", "DEC0", "DEC1", "DEC2", "DEC3", "DEC4", "DEC5", "DEC6",
	"DEC7", "DEC8",	"RX0", "RX1", "RX2", "RX3", "RX4", "RX5", "RX6", "RX7"
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

static const char * const rx_int5_interp_mux_text[] = {
	"ZERO", "RX INT5 MIX2",
};

static const char * const rx_int6_interp_mux_text[] = {
	"ZERO", "RX INT6 MIX2",
};

static const char * const rx_int7_interp_mux_text[] = {
	"ZERO", "RX INT7 MIX2",
};

static const char * const rx_int8_interp_mux_text[] = {
	"ZERO", "RX INT8 SEC MIX"
};

static const char * const mad_sel_text[] = {
	"SPE", "MSM"
};

static const char * const adc_mux_text[] = {
	"DMIC", "AMIC", "ANC_FB_TUNE1", "ANC_FB_TUNE2"
};

static const char * const dmic_mux_text[] = {
	"ZERO", "DMIC0", "DMIC1", "DMIC2", "DMIC3", "DMIC4", "DMIC5",
	"SMIC0", "SMIC1", "SMIC2", "SMIC3"
};

static const char * const dmic_mux_alt_text[] = {
	"ZERO", "DMIC0", "DMIC1", "DMIC2", "DMIC3", "DMIC4", "DMIC5",
};

static const char * const amic_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "ADC4", "ADC5", "ADC6"
};

static const char * const rx_echo_mux_text[] = {
	"ZERO", "RX_MIX0", "RX_MIX1", "RX_MIX2", "RX_MIX3", "RX_MIX4",
	"RX_MIX5", "RX_MIX6", "RX_MIX7", "RX_MIX8", "RX_MIX_VBAT5",
	"RX_MIX_VBAT6",	"RX_MIX_VBAT7", "RX_MIX_VBAT8"
};

static const char * const anc0_fb_mux_text[] = {
	"ZERO", "ANC_IN_HPHL", "ANC_IN_EAR", "ANC_IN_EAR_SPKR",
	"ANC_IN_LO1"
};

static const char * const anc1_fb_mux_text[] = {
	"ZERO", "ANC_IN_HPHR", "ANC_IN_LO2"
};

static const char * const native_mux_text[] = {
	"OFF", "ON",
};

static const struct soc_enum spl_src0_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_SPLINE_SRC_CFG0, 0, 3,
			spl_src0_mux_text);

static const struct soc_enum spl_src1_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_SPLINE_SRC_CFG0, 2, 3,
			spl_src1_mux_text);

static const struct soc_enum spl_src2_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_SPLINE_SRC_CFG0, 4, 3,
			spl_src2_mux_text);

static const struct soc_enum spl_src3_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_SPLINE_SRC_CFG0, 6, 3,
			spl_src3_mux_text);

static const struct soc_enum rx_int0_2_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT0_CFG1, 0, 10,
			rx_int0_7_mix_mux_text);

static const struct soc_enum rx_int1_2_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT1_CFG1, 0, 9,
			rx_int_mix_mux_text);

static const struct soc_enum rx_int2_2_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT2_CFG1, 0, 9,
			rx_int_mix_mux_text);

static const struct soc_enum rx_int3_2_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT3_CFG1, 0, 9,
			rx_int_mix_mux_text);

static const struct soc_enum rx_int4_2_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT4_CFG1, 0, 9,
			rx_int_mix_mux_text);

static const struct soc_enum rx_int5_2_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT5_CFG1, 0, 9,
			rx_int_mix_mux_text);

static const struct soc_enum rx_int6_2_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT6_CFG1, 0, 9,
			rx_int_mix_mux_text);

static const struct soc_enum rx_int7_2_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT7_CFG1, 0, 10,
			rx_int0_7_mix_mux_text);

static const struct soc_enum rx_int8_2_mux_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT8_CFG1, 0, 9,
			rx_int_mix_mux_text);

static const struct soc_enum int1_1_native_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(native_mux_text),
			native_mux_text);

static const struct soc_enum int2_1_native_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(native_mux_text),
			native_mux_text);

static const struct soc_enum int3_1_native_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(native_mux_text),
			native_mux_text);

static const struct soc_enum int4_1_native_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(native_mux_text),
			native_mux_text);

static const struct soc_enum rx_int0_1_mix_inp0_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT0_CFG0, 0, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int0_1_mix_inp1_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT0_CFG0, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int0_1_mix_inp2_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT0_CFG1, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int1_1_mix_inp0_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT1_CFG0, 0, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int1_1_mix_inp1_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT1_CFG0, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int1_1_mix_inp2_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT1_CFG1, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int2_1_mix_inp0_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT2_CFG0, 0, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int2_1_mix_inp1_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT2_CFG0, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int2_1_mix_inp2_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT2_CFG1, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int3_1_mix_inp0_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT3_CFG0, 0, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int3_1_mix_inp1_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT3_CFG0, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int3_1_mix_inp2_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT3_CFG1, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int4_1_mix_inp0_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT4_CFG0, 0, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int4_1_mix_inp1_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT4_CFG0, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int4_1_mix_inp2_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT4_CFG1, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int5_1_mix_inp0_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT5_CFG0, 0, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int5_1_mix_inp1_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT5_CFG0, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int5_1_mix_inp2_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT5_CFG1, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int6_1_mix_inp0_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT6_CFG0, 0, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int6_1_mix_inp1_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT6_CFG0, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int6_1_mix_inp2_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT6_CFG1, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int7_1_mix_inp0_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT7_CFG0, 0, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int7_1_mix_inp1_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT7_CFG0, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int7_1_mix_inp2_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT7_CFG1, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int8_1_mix_inp0_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT8_CFG0, 0, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int8_1_mix_inp1_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT8_CFG0, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int8_1_mix_inp2_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_INT8_CFG1, 4, 13,
			rx_prim_mix_text);

static const struct soc_enum rx_int0_sidetone_mix_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 0, 4,
			rx_sidetone_mix_text);

static const struct soc_enum rx_int1_sidetone_mix_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 2, 4,
			rx_sidetone_mix_text);

static const struct soc_enum rx_int2_sidetone_mix_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 4, 4,
			rx_sidetone_mix_text);

static const struct soc_enum rx_int3_sidetone_mix_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 6, 4,
			rx_sidetone_mix_text);

static const struct soc_enum rx_int4_sidetone_mix_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_SIDETONE_SRC_CFG1, 0, 4,
			rx_sidetone_mix_text);

static const struct soc_enum rx_int7_sidetone_mix_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_SIDETONE_SRC_CFG1, 2, 4,
			rx_sidetone_mix_text);

static const struct soc_enum tx_adc_mux0_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX0_CFG1, 0, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux1_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX1_CFG1, 0, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux2_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX2_CFG1, 0, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux3_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX3_CFG1, 0, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux4_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX4_CFG0, 6, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux5_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX5_CFG0, 6, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux6_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX6_CFG0, 6, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux7_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX7_CFG0, 6, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux8_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX8_CFG0, 6, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux10_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX10_CFG0, 6, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux11_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX11_CFG0, 6, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux12_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX12_CFG0, 6, 4,
			adc_mux_text);

static const struct soc_enum tx_adc_mux13_chain_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX13_CFG0, 6, 4,
			adc_mux_text);

static const struct soc_enum tx_dmic_mux0_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX0_CFG0, 3, 11,
			dmic_mux_text);

static const struct soc_enum tx_dmic_mux1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX1_CFG0, 3, 11,
			dmic_mux_text);

static const struct soc_enum tx_dmic_mux2_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX2_CFG0, 3, 11,
			dmic_mux_text);

static const struct soc_enum tx_dmic_mux3_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX3_CFG0, 3, 11,
			dmic_mux_text);

static const struct soc_enum tx_dmic_mux4_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX4_CFG0, 3, 7,
			dmic_mux_alt_text);

static const struct soc_enum tx_dmic_mux5_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX5_CFG0, 3, 7,
			dmic_mux_alt_text);

static const struct soc_enum tx_dmic_mux6_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX6_CFG0, 3, 7,
			dmic_mux_alt_text);

static const struct soc_enum tx_dmic_mux7_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX7_CFG0, 3, 7,
			dmic_mux_alt_text);

static const struct soc_enum tx_dmic_mux8_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX8_CFG0, 3, 7,
			dmic_mux_alt_text);

static const struct soc_enum tx_dmic_mux10_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX10_CFG0, 3, 7,
			dmic_mux_alt_text);

static const struct soc_enum tx_dmic_mux11_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX11_CFG0, 3, 7,
			dmic_mux_alt_text);

static const struct soc_enum tx_dmic_mux12_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX12_CFG0, 3, 7,
			dmic_mux_alt_text);

static const struct soc_enum tx_dmic_mux13_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX13_CFG0, 3, 7,
			dmic_mux_alt_text);

static const struct soc_enum tx_amic_mux0_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX0_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux1_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX1_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux2_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX2_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux3_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX3_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux4_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX4_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux5_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX5_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux6_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX6_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux7_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX7_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux8_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX8_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux10_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX10_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux11_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX11_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux12_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX12_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum tx_amic_mux13_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_TX_INP_MUX_ADC_MUX13_CFG0, 0, 7,
			amic_mux_text);

static const struct soc_enum sb_tx0_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG0, 0, 4,
			sb_tx0_mux_text);

static const struct soc_enum sb_tx1_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG0, 2, 4,
			sb_tx1_mux_text);

static const struct soc_enum sb_tx2_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG0, 4, 4,
			sb_tx2_mux_text);

static const struct soc_enum sb_tx3_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG0, 6, 4,
			sb_tx3_mux_text);

static const struct soc_enum sb_tx4_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG1, 0, 4,
			sb_tx4_mux_text);

static const struct soc_enum sb_tx5_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG1, 2, 4,
			sb_tx5_mux_text);

static const struct soc_enum sb_tx6_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG1, 4, 4,
			sb_tx6_mux_text);

static const struct soc_enum sb_tx7_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG1, 6, 4,
			sb_tx7_mux_text);

static const struct soc_enum sb_tx8_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG2, 0, 4,
			sb_tx8_mux_text);

static const struct soc_enum sb_tx9_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG2, 2, 3,
			sb_tx9_mux_text);

static const struct soc_enum sb_tx10_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG2, 4, 3,
			sb_tx10_mux_text);

static const struct soc_enum sb_tx11_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_DATA_HUB_DATA_HUB_SB_TX11_INP_CFG, 0, 4,
			sb_tx11_mux_text);

static const struct soc_enum sb_tx11_inp1_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG3, 0, 10,
			sb_tx11_inp1_mux_text);

static const struct soc_enum sb_tx13_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_IF_ROUTER_TX_MUX_CFG3, 4, 3,
			sb_tx13_mux_text);

static const struct soc_enum tx13_inp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_DATA_HUB_DATA_HUB_SB_TX13_INP_CFG, 0, 3,
			tx13_inp_mux_text);

static const struct soc_enum rx_mix_tx0_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_MIX_CFG0, 0, 14,
			rx_echo_mux_text);

static const struct soc_enum rx_mix_tx1_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_MIX_CFG0, 4, 14,
			rx_echo_mux_text);

static const struct soc_enum rx_mix_tx2_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_MIX_CFG1, 0, 14,
			rx_echo_mux_text);

static const struct soc_enum rx_mix_tx3_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_MIX_CFG1, 4, 14,
			rx_echo_mux_text);

static const struct soc_enum rx_mix_tx4_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_MIX_CFG2, 0, 14,
			rx_echo_mux_text);

static const struct soc_enum rx_mix_tx5_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_MIX_CFG2, 4, 14,
			rx_echo_mux_text);

static const struct soc_enum rx_mix_tx6_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_MIX_CFG3, 0, 14,
			rx_echo_mux_text);

static const struct soc_enum rx_mix_tx7_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_MIX_CFG3, 4, 14,
			rx_echo_mux_text);

static const struct soc_enum rx_mix_tx8_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_RX_MIX_CFG4, 0, 14,
			rx_echo_mux_text);

static const struct soc_enum iir0_inp0_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG0, 0, 18,
			iir_inp_mux_text);

static const struct soc_enum iir0_inp1_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG1, 0, 18,
			iir_inp_mux_text);

static const struct soc_enum iir0_inp2_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG2, 0, 18,
			iir_inp_mux_text);

static const struct soc_enum iir0_inp3_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG3, 0, 18,
			iir_inp_mux_text);

static const struct soc_enum iir1_inp0_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_SIDETONE_IIR_INP_MUX_IIR1_MIX_CFG0, 0, 18,
			iir_inp_mux_text);

static const struct soc_enum iir1_inp1_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_SIDETONE_IIR_INP_MUX_IIR1_MIX_CFG1, 0, 18,
			iir_inp_mux_text);

static const struct soc_enum iir1_inp2_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_SIDETONE_IIR_INP_MUX_IIR1_MIX_CFG2, 0, 18,
			iir_inp_mux_text);

static const struct soc_enum iir1_inp3_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_SIDETONE_IIR_INP_MUX_IIR1_MIX_CFG3, 0, 18,
			iir_inp_mux_text);

static const struct soc_enum rx_int0_dem_inp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX0_RX_PATH_SEC0, 0,
			ARRAY_SIZE(rx_int_dem_inp_mux_text),
			rx_int_dem_inp_mux_text);

static const struct soc_enum rx_int1_dem_inp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX1_RX_PATH_SEC0, 0,
			ARRAY_SIZE(rx_int_dem_inp_mux_text),
			rx_int_dem_inp_mux_text);

static const struct soc_enum rx_int2_dem_inp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX2_RX_PATH_SEC0, 0,
			ARRAY_SIZE(rx_int_dem_inp_mux_text),
			rx_int_dem_inp_mux_text);

static const struct soc_enum rx_int0_interp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX0_RX_PATH_CTL, 5, 2,
			rx_int0_interp_mux_text);

static const struct soc_enum rx_int1_interp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX1_RX_PATH_CTL, 5, 2,
			rx_int1_interp_mux_text);

static const struct soc_enum rx_int2_interp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX2_RX_PATH_CTL, 5, 2,
			rx_int2_interp_mux_text);

static const struct soc_enum rx_int3_interp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX3_RX_PATH_CTL, 5, 2,
			rx_int3_interp_mux_text);

static const struct soc_enum rx_int4_interp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX4_RX_PATH_CTL, 5, 2,
			rx_int4_interp_mux_text);

static const struct soc_enum rx_int5_interp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX5_RX_PATH_CTL, 5, 2,
			rx_int5_interp_mux_text);

static const struct soc_enum rx_int6_interp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX6_RX_PATH_CTL, 5, 2,
			rx_int6_interp_mux_text);

static const struct soc_enum rx_int7_interp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX7_RX_PATH_CTL, 5, 2,
			rx_int7_interp_mux_text);

static const struct soc_enum rx_int8_interp_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX8_RX_PATH_CTL, 5, 2,
			rx_int8_interp_mux_text);

static const struct soc_enum mad_sel_enum =
	SOC_ENUM_SINGLE(WCD9335_CPE_SS_CFG, 0, 2, mad_sel_text);

static const struct soc_enum anc0_fb_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_ANC_CFG0, 0, 5,
			anc0_fb_mux_text);

static const struct soc_enum anc1_fb_mux_enum =
	SOC_ENUM_SINGLE(WCD9335_CDC_RX_INP_MUX_ANC_CFG0, 3, 3,
			anc1_fb_mux_text);

static const struct snd_kcontrol_new rx_int0_dem_inp_mux =
	SOC_DAPM_ENUM_EXT("RX INT0 DEM MUX Mux", rx_int0_dem_inp_mux_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_int_dem_inp_mux_put);

static const struct snd_kcontrol_new rx_int1_dem_inp_mux =
	SOC_DAPM_ENUM_EXT("RX INT1 DEM MUX Mux", rx_int1_dem_inp_mux_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_int_dem_inp_mux_put);

static const struct snd_kcontrol_new rx_int2_dem_inp_mux =
	SOC_DAPM_ENUM_EXT("RX INT2 DEM MUX Mux", rx_int2_dem_inp_mux_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_int_dem_inp_mux_put);

static const struct snd_kcontrol_new spl_src0_mux =
	SOC_DAPM_ENUM("SPL SRC0 MUX Mux", spl_src0_mux_chain_enum);

static const struct snd_kcontrol_new spl_src1_mux =
	SOC_DAPM_ENUM("SPL SRC1 MUX Mux", spl_src1_mux_chain_enum);

static const struct snd_kcontrol_new spl_src2_mux =
	SOC_DAPM_ENUM("SPL SRC2 MUX Mux", spl_src2_mux_chain_enum);

static const struct snd_kcontrol_new spl_src3_mux =
	SOC_DAPM_ENUM("SPL SRC3 MUX Mux", spl_src3_mux_chain_enum);

static const struct snd_kcontrol_new rx_int0_2_mux =
	SOC_DAPM_ENUM("RX INT0_2 MUX Mux", rx_int0_2_mux_chain_enum);

static const struct snd_kcontrol_new rx_int1_2_mux =
	SOC_DAPM_ENUM("RX INT1_2 MUX Mux", rx_int1_2_mux_chain_enum);

static const struct snd_kcontrol_new rx_int2_2_mux =
	SOC_DAPM_ENUM("RX INT2_2 MUX Mux", rx_int2_2_mux_chain_enum);

static const struct snd_kcontrol_new rx_int3_2_mux =
	SOC_DAPM_ENUM("RX INT3_2 MUX Mux", rx_int3_2_mux_chain_enum);

static const struct snd_kcontrol_new rx_int4_2_mux =
	SOC_DAPM_ENUM("RX INT4_2 MUX Mux", rx_int4_2_mux_chain_enum);

static const struct snd_kcontrol_new rx_int5_2_mux =
	SOC_DAPM_ENUM("RX INT5_2 MUX Mux", rx_int5_2_mux_chain_enum);

static const struct snd_kcontrol_new rx_int6_2_mux =
	SOC_DAPM_ENUM("RX INT6_2 MUX Mux", rx_int6_2_mux_chain_enum);

static const struct snd_kcontrol_new rx_int7_2_mux =
	SOC_DAPM_ENUM("RX INT7_2 MUX Mux", rx_int7_2_mux_chain_enum);

static const struct snd_kcontrol_new rx_int8_2_mux =
	SOC_DAPM_ENUM("RX INT8_2 MUX Mux", rx_int8_2_mux_chain_enum);

static const struct snd_kcontrol_new int1_1_native_mux =
	SOC_DAPM_ENUM("RX INT1_1 NATIVE MUX Mux", int1_1_native_enum);

static const struct snd_kcontrol_new int2_1_native_mux =
	SOC_DAPM_ENUM("RX INT2_1 NATIVE MUX Mux", int2_1_native_enum);

static const struct snd_kcontrol_new int3_1_native_mux =
	SOC_DAPM_ENUM("RX INT3_1 NATIVE MUX Mux", int3_1_native_enum);

static const struct snd_kcontrol_new int4_1_native_mux =
	SOC_DAPM_ENUM("RX INT4_1 NATIVE MUX Mux", int4_1_native_enum);

static const struct snd_kcontrol_new rx_int0_1_mix_inp0_mux =
	SOC_DAPM_ENUM("RX INT0_1 MIX1 INP0 Mux", rx_int0_1_mix_inp0_chain_enum);

static const struct snd_kcontrol_new rx_int0_1_mix_inp1_mux =
	SOC_DAPM_ENUM("RX INT0_1 MIX1 INP1 Mux", rx_int0_1_mix_inp1_chain_enum);

static const struct snd_kcontrol_new rx_int0_1_mix_inp2_mux =
	SOC_DAPM_ENUM("RX INT0_1 MIX1 INP2 Mux", rx_int0_1_mix_inp2_chain_enum);

static const struct snd_kcontrol_new rx_int1_1_mix_inp0_mux =
	SOC_DAPM_ENUM("RX INT1_1 MIX1 INP0 Mux", rx_int1_1_mix_inp0_chain_enum);

static const struct snd_kcontrol_new rx_int1_1_mix_inp1_mux =
	SOC_DAPM_ENUM("RX INT1_1 MIX1 INP1 Mux", rx_int1_1_mix_inp1_chain_enum);

static const struct snd_kcontrol_new rx_int1_1_mix_inp2_mux =
	SOC_DAPM_ENUM("RX INT1_1 MIX1 INP2 Mux", rx_int1_1_mix_inp2_chain_enum);

static const struct snd_kcontrol_new rx_int2_1_mix_inp0_mux =
	SOC_DAPM_ENUM("RX INT2_1 MIX1 INP0 Mux", rx_int2_1_mix_inp0_chain_enum);

static const struct snd_kcontrol_new rx_int2_1_mix_inp1_mux =
	SOC_DAPM_ENUM("RX INT2_1 MIX1 INP1 Mux", rx_int2_1_mix_inp1_chain_enum);

static const struct snd_kcontrol_new rx_int2_1_mix_inp2_mux =
	SOC_DAPM_ENUM("RX INT2_1 MIX1 INP2 Mux", rx_int2_1_mix_inp2_chain_enum);

static const struct snd_kcontrol_new rx_int3_1_mix_inp0_mux =
	SOC_DAPM_ENUM("RX INT3_1 MIX1 INP0 Mux", rx_int3_1_mix_inp0_chain_enum);

static const struct snd_kcontrol_new rx_int3_1_mix_inp1_mux =
	SOC_DAPM_ENUM("RX INT3_1 MIX1 INP1 Mux", rx_int3_1_mix_inp1_chain_enum);

static const struct snd_kcontrol_new rx_int3_1_mix_inp2_mux =
	SOC_DAPM_ENUM("RX INT3_1 MIX1 INP2 Mux", rx_int3_1_mix_inp2_chain_enum);

static const struct snd_kcontrol_new rx_int4_1_mix_inp0_mux =
	SOC_DAPM_ENUM("RX INT4_1 MIX1 INP0 Mux", rx_int4_1_mix_inp0_chain_enum);

static const struct snd_kcontrol_new rx_int4_1_mix_inp1_mux =
	SOC_DAPM_ENUM("RX INT4_1 MIX1 INP1 Mux", rx_int4_1_mix_inp1_chain_enum);

static const struct snd_kcontrol_new rx_int4_1_mix_inp2_mux =
	SOC_DAPM_ENUM("RX INT4_1 MIX1 INP2 Mux", rx_int4_1_mix_inp2_chain_enum);

static const struct snd_kcontrol_new rx_int5_1_mix_inp0_mux =
	SOC_DAPM_ENUM("RX INT5_1 MIX1 INP0 Mux", rx_int5_1_mix_inp0_chain_enum);

static const struct snd_kcontrol_new rx_int5_1_mix_inp1_mux =
	SOC_DAPM_ENUM("RX INT5_1 MIX1 INP1 Mux", rx_int5_1_mix_inp1_chain_enum);

static const struct snd_kcontrol_new rx_int5_1_mix_inp2_mux =
	SOC_DAPM_ENUM("RX INT5_1 MIX1 INP2 Mux", rx_int5_1_mix_inp2_chain_enum);

static const struct snd_kcontrol_new rx_int6_1_mix_inp0_mux =
	SOC_DAPM_ENUM("RX INT6_1 MIX1 INP0 Mux", rx_int6_1_mix_inp0_chain_enum);

static const struct snd_kcontrol_new rx_int6_1_mix_inp1_mux =
	SOC_DAPM_ENUM("RX INT6_1 MIX1 INP1 Mux", rx_int6_1_mix_inp1_chain_enum);

static const struct snd_kcontrol_new rx_int6_1_mix_inp2_mux =
	SOC_DAPM_ENUM("RX INT6_1 MIX1 INP2 Mux", rx_int6_1_mix_inp2_chain_enum);

static const struct snd_kcontrol_new rx_int7_1_mix_inp0_mux =
	SOC_DAPM_ENUM("RX INT7_1 MIX1 INP0 Mux", rx_int7_1_mix_inp0_chain_enum);

static const struct snd_kcontrol_new rx_int7_1_mix_inp1_mux =
	SOC_DAPM_ENUM("RX INT7_1 MIX1 INP1 Mux", rx_int7_1_mix_inp1_chain_enum);

static const struct snd_kcontrol_new rx_int7_1_mix_inp2_mux =
	SOC_DAPM_ENUM("RX INT7_1 MIX1 INP2 Mux", rx_int7_1_mix_inp2_chain_enum);

static const struct snd_kcontrol_new rx_int8_1_mix_inp0_mux =
	SOC_DAPM_ENUM("RX INT8_1 MIX1 INP0 Mux", rx_int8_1_mix_inp0_chain_enum);

static const struct snd_kcontrol_new rx_int8_1_mix_inp1_mux =
	SOC_DAPM_ENUM("RX INT8_1 MIX1 INP1 Mux", rx_int8_1_mix_inp1_chain_enum);

static const struct snd_kcontrol_new rx_int8_1_mix_inp2_mux =
	SOC_DAPM_ENUM("RX INT8_1 MIX1 INP2 Mux", rx_int8_1_mix_inp2_chain_enum);

static const struct snd_kcontrol_new rx_int0_mix2_inp_mux =
	SOC_DAPM_ENUM("RX INT0 MIX2 INP Mux", rx_int0_sidetone_mix_chain_enum);

static const struct snd_kcontrol_new rx_int1_mix2_inp_mux =
	SOC_DAPM_ENUM("RX INT1 MIX2 INP Mux", rx_int1_sidetone_mix_chain_enum);

static const struct snd_kcontrol_new rx_int2_mix2_inp_mux =
	SOC_DAPM_ENUM("RX INT2 MIX2 INP Mux", rx_int2_sidetone_mix_chain_enum);

static const struct snd_kcontrol_new rx_int3_mix2_inp_mux =
	SOC_DAPM_ENUM("RX INT3 MIX2 INP Mux", rx_int3_sidetone_mix_chain_enum);

static const struct snd_kcontrol_new rx_int4_mix2_inp_mux =
	SOC_DAPM_ENUM("RX INT4 MIX2 INP Mux", rx_int4_sidetone_mix_chain_enum);

static const struct snd_kcontrol_new rx_int7_mix2_inp_mux =
	SOC_DAPM_ENUM("RX INT7 MIX2 INP Mux", rx_int7_sidetone_mix_chain_enum);

static const struct snd_kcontrol_new tx_adc_mux0 =
	SOC_DAPM_ENUM_EXT("ADC MUX0 Mux", tx_adc_mux0_chain_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_put_dec_enum);

static const struct snd_kcontrol_new tx_adc_mux1 =
	SOC_DAPM_ENUM_EXT("ADC MUX1 Mux", tx_adc_mux1_chain_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_put_dec_enum);

static const struct snd_kcontrol_new tx_adc_mux2 =
	SOC_DAPM_ENUM_EXT("ADC MUX2 Mux", tx_adc_mux2_chain_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_put_dec_enum);

static const struct snd_kcontrol_new tx_adc_mux3 =
	SOC_DAPM_ENUM_EXT("ADC MUX3 Mux", tx_adc_mux3_chain_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_put_dec_enum);

static const struct snd_kcontrol_new tx_adc_mux4 =
	SOC_DAPM_ENUM_EXT("ADC MUX4 Mux", tx_adc_mux4_chain_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_put_dec_enum);

static const struct snd_kcontrol_new tx_adc_mux5 =
	SOC_DAPM_ENUM_EXT("ADC MUX5 Mux", tx_adc_mux5_chain_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_put_dec_enum);

static const struct snd_kcontrol_new tx_adc_mux6 =
	SOC_DAPM_ENUM_EXT("ADC MUX6 Mux", tx_adc_mux6_chain_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_put_dec_enum);

static const struct snd_kcontrol_new tx_adc_mux7 =
	SOC_DAPM_ENUM_EXT("ADC MUX7 Mux", tx_adc_mux7_chain_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_put_dec_enum);

static const struct snd_kcontrol_new tx_adc_mux8 =
	SOC_DAPM_ENUM_EXT("ADC MUX8 Mux", tx_adc_mux8_chain_enum,
			  snd_soc_dapm_get_enum_double,
			  tasha_put_dec_enum);

static const struct snd_kcontrol_new tx_adc_mux10 =
	SOC_DAPM_ENUM("ADC MUX10 Mux", tx_adc_mux10_chain_enum);

static const struct snd_kcontrol_new tx_adc_mux11 =
	SOC_DAPM_ENUM("ADC MUX11 Mux", tx_adc_mux11_chain_enum);

static const struct snd_kcontrol_new tx_adc_mux12 =
	SOC_DAPM_ENUM("ADC MUX12 Mux", tx_adc_mux12_chain_enum);

static const struct snd_kcontrol_new tx_adc_mux13 =
	SOC_DAPM_ENUM("ADC MUX13 Mux", tx_adc_mux13_chain_enum);

static const struct snd_kcontrol_new tx_dmic_mux0 =
	SOC_DAPM_ENUM("DMIC MUX0 Mux", tx_dmic_mux0_enum);

static const struct snd_kcontrol_new tx_dmic_mux1 =
	SOC_DAPM_ENUM("DMIC MUX1 Mux", tx_dmic_mux1_enum);

static const struct snd_kcontrol_new tx_dmic_mux2 =
	SOC_DAPM_ENUM("DMIC MUX2 Mux", tx_dmic_mux2_enum);

static const struct snd_kcontrol_new tx_dmic_mux3 =
	SOC_DAPM_ENUM("DMIC MUX3 Mux", tx_dmic_mux3_enum);

static const struct snd_kcontrol_new tx_dmic_mux4 =
	SOC_DAPM_ENUM("DMIC MUX4 Mux", tx_dmic_mux4_enum);

static const struct snd_kcontrol_new tx_dmic_mux5 =
	SOC_DAPM_ENUM("DMIC MUX5 Mux", tx_dmic_mux5_enum);

static const struct snd_kcontrol_new tx_dmic_mux6 =
	SOC_DAPM_ENUM("DMIC MUX6 Mux", tx_dmic_mux6_enum);

static const struct snd_kcontrol_new tx_dmic_mux7 =
	SOC_DAPM_ENUM("DMIC MUX7 Mux", tx_dmic_mux7_enum);

static const struct snd_kcontrol_new tx_dmic_mux8 =
	SOC_DAPM_ENUM("DMIC MUX8 Mux", tx_dmic_mux8_enum);

static const struct snd_kcontrol_new tx_dmic_mux10 =
	SOC_DAPM_ENUM("DMIC MUX10 Mux", tx_dmic_mux10_enum);

static const struct snd_kcontrol_new tx_dmic_mux11 =
	SOC_DAPM_ENUM("DMIC MUX11 Mux", tx_dmic_mux11_enum);

static const struct snd_kcontrol_new tx_dmic_mux12 =
	SOC_DAPM_ENUM("DMIC MUX12 Mux", tx_dmic_mux12_enum);

static const struct snd_kcontrol_new tx_dmic_mux13 =
	SOC_DAPM_ENUM("DMIC MUX13 Mux", tx_dmic_mux13_enum);

static const struct snd_kcontrol_new tx_amic_mux0 =
	SOC_DAPM_ENUM("AMIC MUX0 Mux", tx_amic_mux0_enum);

static const struct snd_kcontrol_new tx_amic_mux1 =
	SOC_DAPM_ENUM("AMIC MUX1 Mux", tx_amic_mux1_enum);

static const struct snd_kcontrol_new tx_amic_mux2 =
	SOC_DAPM_ENUM("AMIC MUX2 Mux", tx_amic_mux2_enum);

static const struct snd_kcontrol_new tx_amic_mux3 =
	SOC_DAPM_ENUM("AMIC MUX3 Mux", tx_amic_mux3_enum);

static const struct snd_kcontrol_new tx_amic_mux4 =
	SOC_DAPM_ENUM("AMIC MUX4 Mux", tx_amic_mux4_enum);

static const struct snd_kcontrol_new tx_amic_mux5 =
	SOC_DAPM_ENUM("AMIC MUX5 Mux", tx_amic_mux5_enum);

static const struct snd_kcontrol_new tx_amic_mux6 =
	SOC_DAPM_ENUM("AMIC MUX6 Mux", tx_amic_mux6_enum);

static const struct snd_kcontrol_new tx_amic_mux7 =
	SOC_DAPM_ENUM("AMIC MUX7 Mux", tx_amic_mux7_enum);

static const struct snd_kcontrol_new tx_amic_mux8 =
	SOC_DAPM_ENUM("AMIC MUX8 Mux", tx_amic_mux8_enum);

static const struct snd_kcontrol_new tx_amic_mux10 =
	SOC_DAPM_ENUM("AMIC MUX10 Mux", tx_amic_mux10_enum);

static const struct snd_kcontrol_new tx_amic_mux11 =
	SOC_DAPM_ENUM("AMIC MUX11 Mux", tx_amic_mux11_enum);

static const struct snd_kcontrol_new tx_amic_mux12 =
	SOC_DAPM_ENUM("AMIC MUX12 Mux", tx_amic_mux12_enum);

static const struct snd_kcontrol_new tx_amic_mux13 =
	SOC_DAPM_ENUM("AMIC MUX13 Mux", tx_amic_mux13_enum);

static const struct snd_kcontrol_new sb_tx0_mux =
	SOC_DAPM_ENUM("SLIM TX0 MUX Mux", sb_tx0_mux_enum);

static const struct snd_kcontrol_new sb_tx1_mux =
	SOC_DAPM_ENUM("SLIM TX1 MUX Mux", sb_tx1_mux_enum);

static const struct snd_kcontrol_new sb_tx2_mux =
	SOC_DAPM_ENUM("SLIM TX2 MUX Mux", sb_tx2_mux_enum);

static const struct snd_kcontrol_new sb_tx3_mux =
	SOC_DAPM_ENUM("SLIM TX3 MUX Mux", sb_tx3_mux_enum);

static const struct snd_kcontrol_new sb_tx4_mux =
	SOC_DAPM_ENUM("SLIM TX4 MUX Mux", sb_tx4_mux_enum);

static const struct snd_kcontrol_new sb_tx5_mux =
	SOC_DAPM_ENUM("SLIM TX5 MUX Mux", sb_tx5_mux_enum);

static const struct snd_kcontrol_new sb_tx6_mux =
	SOC_DAPM_ENUM("SLIM TX6 MUX Mux", sb_tx6_mux_enum);

static const struct snd_kcontrol_new sb_tx7_mux =
	SOC_DAPM_ENUM("SLIM TX7 MUX Mux", sb_tx7_mux_enum);

static const struct snd_kcontrol_new sb_tx8_mux =
	SOC_DAPM_ENUM("SLIM TX8 MUX Mux", sb_tx8_mux_enum);

static const struct snd_kcontrol_new sb_tx9_mux =
	SOC_DAPM_ENUM("SLIM TX9 MUX Mux", sb_tx9_mux_enum);

static const struct snd_kcontrol_new sb_tx10_mux =
	SOC_DAPM_ENUM("SLIM TX10 MUX Mux", sb_tx10_mux_enum);

static const struct snd_kcontrol_new sb_tx11_mux =
	SOC_DAPM_ENUM("SLIM TX11 MUX Mux", sb_tx11_mux_enum);

static const struct snd_kcontrol_new sb_tx11_inp1_mux =
	SOC_DAPM_ENUM("SLIM TX11 INP1 MUX Mux", sb_tx11_inp1_mux_enum);

static const struct snd_kcontrol_new sb_tx13_mux =
	SOC_DAPM_ENUM("SLIM TX13 MUX Mux", sb_tx13_mux_enum);

static const struct snd_kcontrol_new tx13_inp_mux =
	SOC_DAPM_ENUM("TX13 INP MUX Mux", tx13_inp_mux_enum);

static const struct snd_kcontrol_new rx_mix_tx0_mux =
	SOC_DAPM_ENUM("RX MIX TX0 MUX Mux", rx_mix_tx0_mux_enum);

static const struct snd_kcontrol_new rx_mix_tx1_mux =
	SOC_DAPM_ENUM("RX MIX TX1 MUX Mux", rx_mix_tx1_mux_enum);

static const struct snd_kcontrol_new rx_mix_tx2_mux =
	SOC_DAPM_ENUM("RX MIX TX2 MUX Mux", rx_mix_tx2_mux_enum);

static const struct snd_kcontrol_new rx_mix_tx3_mux =
	SOC_DAPM_ENUM("RX MIX TX3 MUX Mux", rx_mix_tx3_mux_enum);

static const struct snd_kcontrol_new rx_mix_tx4_mux =
	SOC_DAPM_ENUM("RX MIX TX4 MUX Mux", rx_mix_tx4_mux_enum);

static const struct snd_kcontrol_new rx_mix_tx5_mux =
	SOC_DAPM_ENUM("RX MIX TX5 MUX Mux", rx_mix_tx5_mux_enum);

static const struct snd_kcontrol_new rx_mix_tx6_mux =
	SOC_DAPM_ENUM("RX MIX TX6 MUX Mux", rx_mix_tx6_mux_enum);

static const struct snd_kcontrol_new rx_mix_tx7_mux =
	SOC_DAPM_ENUM("RX MIX TX7 MUX Mux", rx_mix_tx7_mux_enum);

static const struct snd_kcontrol_new rx_mix_tx8_mux =
	SOC_DAPM_ENUM("RX MIX TX8 MUX Mux", rx_mix_tx8_mux_enum);

static const struct snd_kcontrol_new iir0_inp0_mux =
	SOC_DAPM_ENUM("IIR0 INP0 Mux", iir0_inp0_mux_enum);

static const struct snd_kcontrol_new iir0_inp1_mux =
	SOC_DAPM_ENUM("IIR0 INP1 Mux", iir0_inp1_mux_enum);

static const struct snd_kcontrol_new iir0_inp2_mux =
	SOC_DAPM_ENUM("IIR0 INP2 Mux", iir0_inp2_mux_enum);

static const struct snd_kcontrol_new iir0_inp3_mux =
	SOC_DAPM_ENUM("IIR0 INP3 Mux", iir0_inp3_mux_enum);

static const struct snd_kcontrol_new iir1_inp0_mux =
	SOC_DAPM_ENUM("IIR1 INP0 Mux", iir1_inp0_mux_enum);

static const struct snd_kcontrol_new iir1_inp1_mux =
	SOC_DAPM_ENUM("IIR1 INP1 Mux", iir1_inp1_mux_enum);

static const struct snd_kcontrol_new iir1_inp2_mux =
	SOC_DAPM_ENUM("IIR1 INP2 Mux", iir1_inp2_mux_enum);

static const struct snd_kcontrol_new iir1_inp3_mux =
	SOC_DAPM_ENUM("IIR1 INP3 Mux", iir1_inp3_mux_enum);

static const struct snd_kcontrol_new rx_int0_interp_mux =
	SOC_DAPM_ENUM("RX INT0 INTERP Mux", rx_int0_interp_mux_enum);

static const struct snd_kcontrol_new rx_int1_interp_mux =
	SOC_DAPM_ENUM("RX INT1 INTERP Mux", rx_int1_interp_mux_enum);

static const struct snd_kcontrol_new rx_int2_interp_mux =
	SOC_DAPM_ENUM("RX INT2 INTERP Mux", rx_int2_interp_mux_enum);

static const struct snd_kcontrol_new rx_int3_interp_mux =
	SOC_DAPM_ENUM("RX INT3 INTERP Mux", rx_int3_interp_mux_enum);

static const struct snd_kcontrol_new rx_int4_interp_mux =
	SOC_DAPM_ENUM("RX INT4 INTERP Mux", rx_int4_interp_mux_enum);

static const struct snd_kcontrol_new rx_int5_interp_mux =
	SOC_DAPM_ENUM("RX INT5 INTERP Mux", rx_int5_interp_mux_enum);

static const struct snd_kcontrol_new rx_int6_interp_mux =
	SOC_DAPM_ENUM("RX INT6 INTERP Mux", rx_int6_interp_mux_enum);

static const struct snd_kcontrol_new rx_int7_interp_mux =
	SOC_DAPM_ENUM("RX INT7 INTERP Mux", rx_int7_interp_mux_enum);

static const struct snd_kcontrol_new rx_int8_interp_mux =
	SOC_DAPM_ENUM("RX INT8 INTERP Mux", rx_int8_interp_mux_enum);

static const struct snd_kcontrol_new mad_sel_mux =
	SOC_DAPM_ENUM("MAD_SEL MUX Mux", mad_sel_enum);

static const struct snd_kcontrol_new aif4_mad_switch =
	SOC_DAPM_SINGLE("Switch", WCD9335_CPE_SS_CFG, 5, 1, 0);

static const struct snd_kcontrol_new mad_brdcst_switch =
	SOC_DAPM_SINGLE("Switch", WCD9335_CPE_SS_CFG, 6, 1, 0);

static const struct snd_kcontrol_new aif4_switch_mixer_controls =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM,
			0, 1, 0, tasha_codec_aif4_mixer_switch_get,
			tasha_codec_aif4_mixer_switch_put);

static const struct snd_kcontrol_new anc_hphl_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_hphr_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_ear_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_ear_spkr_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_lineout1_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_lineout2_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_spkr_pa_switch =
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

static const struct snd_kcontrol_new anc0_fb_mux =
	SOC_DAPM_ENUM("ANC0 FB MUX Mux", anc0_fb_mux_enum);

static const struct snd_kcontrol_new anc1_fb_mux =
	SOC_DAPM_ENUM("ANC1 FB MUX Mux", anc1_fb_mux_enum);

static int tasha_codec_ec_buf_mux_enable(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s: event = %d name = %s\n",
		__func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_write(codec, WCD9335_CPE_SS_EC_BUF_INT_PERIOD, 0x3B);
		snd_soc_update_bits(codec, WCD9335_CPE_SS_CFG, 0x08, 0x08);
		snd_soc_update_bits(codec, WCD9335_CDC_IF_ROUTER_TX_MUX_CFG0,
				    0x08, 0x08);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, WCD9335_CDC_IF_ROUTER_TX_MUX_CFG0,
				    0x08, 0x00);
		snd_soc_update_bits(codec, WCD9335_CPE_SS_CFG, 0x08, 0x00);
		snd_soc_write(codec, WCD9335_CPE_SS_EC_BUF_INT_PERIOD, 0x00);
		break;
	}

	return 0;
};

static const char * const ec_buf_mux_text[] = {
	"ZERO", "RXMIXEC", "SB_RX0", "SB_RX1", "SB_RX2", "SB_RX3",
	"I2S_RX_SD0_L", "I2S_RX_SD0_R", "I2S_RX_SD1_L", "I2S_RX_SD1_R",
	"DEC1"
};

static SOC_ENUM_SINGLE_DECL(ec_buf_mux_enum, WCD9335_CPE_SS_US_EC_MUX_CFG,
			    0, ec_buf_mux_text);

static const struct snd_kcontrol_new ec_buf_mux =
	SOC_DAPM_ENUM("EC BUF Mux", ec_buf_mux_enum);

static const struct snd_soc_dapm_widget tasha_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("EAR"),
	SND_SOC_DAPM_OUTPUT("ANC EAR"),
	SND_SOC_DAPM_AIF_IN_E("AIF1 PB", "AIF1 Playback", 0, SND_SOC_NOPM,
				AIF1_PB, 0, tasha_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF2 PB", "AIF2 Playback", 0, SND_SOC_NOPM,
				AIF2_PB, 0, tasha_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF3 PB", "AIF3 Playback", 0, SND_SOC_NOPM,
				AIF3_PB, 0, tasha_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF4 PB", "AIF4 Playback", 0, SND_SOC_NOPM,
				AIF4_PB, 0, tasha_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF MIX1 PB", "AIF Mix Playback", 0,
			       SND_SOC_NOPM, AIF_MIX1_PB, 0,
			       tasha_codec_enable_slimrx,
			       SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
			       SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM RX0 MUX", SND_SOC_NOPM, TASHA_RX0, 0,
				&slim_rx_mux[TASHA_RX0]),
	SND_SOC_DAPM_MUX("SLIM RX1 MUX", SND_SOC_NOPM, TASHA_RX1, 0,
				&slim_rx_mux[TASHA_RX1]),
	SND_SOC_DAPM_MUX("SLIM RX2 MUX", SND_SOC_NOPM, TASHA_RX2, 0,
				&slim_rx_mux[TASHA_RX2]),
	SND_SOC_DAPM_MUX("SLIM RX3 MUX", SND_SOC_NOPM, TASHA_RX3, 0,
				&slim_rx_mux[TASHA_RX3]),
	SND_SOC_DAPM_MUX("SLIM RX4 MUX", SND_SOC_NOPM, TASHA_RX4, 0,
				&slim_rx_mux[TASHA_RX4]),
	SND_SOC_DAPM_MUX("SLIM RX5 MUX", SND_SOC_NOPM, TASHA_RX5, 0,
				&slim_rx_mux[TASHA_RX5]),
	SND_SOC_DAPM_MUX("SLIM RX6 MUX", SND_SOC_NOPM, TASHA_RX6, 0,
				&slim_rx_mux[TASHA_RX6]),
	SND_SOC_DAPM_MUX("SLIM RX7 MUX", SND_SOC_NOPM, TASHA_RX7, 0,
				&slim_rx_mux[TASHA_RX7]),

	SND_SOC_DAPM_MIXER("SLIM RX0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX6", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX7", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("SPL SRC0 MUX", SND_SOC_NOPM, SPLINE_SRC0, 0,
			 &spl_src0_mux, tasha_codec_enable_spline_resampler,
			 SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPL SRC1 MUX", SND_SOC_NOPM, SPLINE_SRC1, 0,
			 &spl_src1_mux, tasha_codec_enable_spline_resampler,
			 SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPL SRC2 MUX", SND_SOC_NOPM, SPLINE_SRC2, 0,
			 &spl_src2_mux, tasha_codec_enable_spline_resampler,
			 SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPL SRC3 MUX", SND_SOC_NOPM, SPLINE_SRC3, 0,
			 &spl_src3_mux, tasha_codec_enable_spline_resampler,
			 SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("RX INT0_2 MUX", WCD9335_CDC_RX0_RX_PATH_MIX_CTL,
			5, 0, &rx_int0_2_mux, tasha_codec_enable_mix_path,
			SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT1_2 MUX", WCD9335_CDC_RX1_RX_PATH_MIX_CTL,
			5, 0, &rx_int1_2_mux, tasha_codec_enable_mix_path,
			SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT2_2 MUX", WCD9335_CDC_RX2_RX_PATH_MIX_CTL,
			5, 0, &rx_int2_2_mux, tasha_codec_enable_mix_path,
			SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT3_2 MUX", WCD9335_CDC_RX3_RX_PATH_MIX_CTL,
			5, 0, &rx_int3_2_mux, tasha_codec_enable_mix_path,
			SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT4_2 MUX", WCD9335_CDC_RX4_RX_PATH_MIX_CTL,
			5, 0, &rx_int4_2_mux, tasha_codec_enable_mix_path,
			SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT5_2 MUX", WCD9335_CDC_RX5_RX_PATH_MIX_CTL,
			5, 0, &rx_int5_2_mux, tasha_codec_enable_mix_path,
			SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT6_2 MUX", WCD9335_CDC_RX6_RX_PATH_MIX_CTL,
			5, 0, &rx_int6_2_mux, tasha_codec_enable_mix_path,
			SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT7_2 MUX", WCD9335_CDC_RX7_RX_PATH_MIX_CTL,
			5, 0, &rx_int7_2_mux, tasha_codec_enable_mix_path,
			SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("RX INT8_2 MUX", WCD9335_CDC_RX8_RX_PATH_MIX_CTL,
			5, 0, &rx_int8_2_mux, tasha_codec_enable_mix_path,
			SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX("RX INT0_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int0_1_mix_inp0_mux),
	SND_SOC_DAPM_MUX("RX INT0_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int0_1_mix_inp1_mux),
	SND_SOC_DAPM_MUX("RX INT0_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int0_1_mix_inp2_mux),
	SND_SOC_DAPM_MUX("RX INT1_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int1_1_mix_inp0_mux),
	SND_SOC_DAPM_MUX("RX INT1_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int1_1_mix_inp1_mux),
	SND_SOC_DAPM_MUX("RX INT1_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int1_1_mix_inp2_mux),
	SND_SOC_DAPM_MUX("RX INT2_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int2_1_mix_inp0_mux),
	SND_SOC_DAPM_MUX("RX INT2_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int2_1_mix_inp1_mux),
	SND_SOC_DAPM_MUX("RX INT2_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int2_1_mix_inp2_mux),
	SND_SOC_DAPM_MUX("RX INT3_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int3_1_mix_inp0_mux),
	SND_SOC_DAPM_MUX("RX INT3_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int3_1_mix_inp1_mux),
	SND_SOC_DAPM_MUX("RX INT3_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int3_1_mix_inp2_mux),
	SND_SOC_DAPM_MUX("RX INT4_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int4_1_mix_inp0_mux),
	SND_SOC_DAPM_MUX("RX INT4_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int4_1_mix_inp1_mux),
	SND_SOC_DAPM_MUX("RX INT4_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int4_1_mix_inp2_mux),
	SND_SOC_DAPM_MUX("RX INT5_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int5_1_mix_inp0_mux),
	SND_SOC_DAPM_MUX("RX INT5_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int5_1_mix_inp1_mux),
	SND_SOC_DAPM_MUX("RX INT5_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int5_1_mix_inp2_mux),
	SND_SOC_DAPM_MUX("RX INT6_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int6_1_mix_inp0_mux),
	SND_SOC_DAPM_MUX("RX INT6_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int6_1_mix_inp1_mux),
	SND_SOC_DAPM_MUX("RX INT6_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int6_1_mix_inp2_mux),
	SND_SOC_DAPM_MUX_E("RX INT7_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int7_1_mix_inp0_mux, tasha_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int7_1_mix_inp1_mux, tasha_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int7_1_mix_inp2_mux, tasha_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int8_1_mix_inp0_mux, tasha_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int8_1_mix_inp1_mux, tasha_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int8_1_mix_inp2_mux, tasha_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("RX INT0_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT0 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT1_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT1 SPLINE MIX", SND_SOC_NOPM, 0, 0,
			rx_int1_spline_mix_switch,
			ARRAY_SIZE(rx_int1_spline_mix_switch)),
	SND_SOC_DAPM_MIXER("RX INT1 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT2_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT2 SPLINE MIX", SND_SOC_NOPM, 0, 0,
			rx_int2_spline_mix_switch,
			ARRAY_SIZE(rx_int2_spline_mix_switch)),
	SND_SOC_DAPM_MIXER("RX INT2 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT3_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT3 SPLINE MIX", SND_SOC_NOPM, 0, 0,
			rx_int3_spline_mix_switch,
			ARRAY_SIZE(rx_int3_spline_mix_switch)),
	SND_SOC_DAPM_MIXER("RX INT3 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT4_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT4 SPLINE MIX", SND_SOC_NOPM, 0, 0,
			rx_int4_spline_mix_switch,
			ARRAY_SIZE(rx_int4_spline_mix_switch)),
	SND_SOC_DAPM_MIXER("RX INT4 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT5_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT5 SPLINE MIX", SND_SOC_NOPM, 0, 0,
			rx_int5_spline_mix_switch,
			ARRAY_SIZE(rx_int5_spline_mix_switch)),
	SND_SOC_DAPM_MIXER("RX INT5 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("RX INT6_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT6 SPLINE MIX", SND_SOC_NOPM, 0, 0,
			rx_int6_spline_mix_switch,
			ARRAY_SIZE(rx_int6_spline_mix_switch)),
	SND_SOC_DAPM_MIXER("RX INT6 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("RX INT7_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT7 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT7 SPLINE MIX", SND_SOC_NOPM, 0, 0,
			rx_int7_spline_mix_switch,
			ARRAY_SIZE(rx_int7_spline_mix_switch)),

	SND_SOC_DAPM_MIXER("RX INT8_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT8 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT8 SPLINE MIX", SND_SOC_NOPM, 0, 0,
			rx_int8_spline_mix_switch,
			ARRAY_SIZE(rx_int8_spline_mix_switch)),

	SND_SOC_DAPM_MIXER("RX INT0 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT1 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT2 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT3 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT4 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT5 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT6 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT7 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("RX INT7 CHAIN", SND_SOC_NOPM, 0, 0,
			NULL, 0, tasha_codec_spk_boost_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX INT8 CHAIN", SND_SOC_NOPM, 0, 0,
			NULL, 0, tasha_codec_spk_boost_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("RX INT5 VBAT", SND_SOC_NOPM, 0, 0,
			rx_int5_vbat_mix_switch,
			ARRAY_SIZE(rx_int5_vbat_mix_switch),
			tasha_codec_vbat_enable_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX INT6 VBAT", SND_SOC_NOPM, 0, 0,
			rx_int6_vbat_mix_switch,
			ARRAY_SIZE(rx_int6_vbat_mix_switch),
			tasha_codec_vbat_enable_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX INT7 VBAT", SND_SOC_NOPM, 0, 0,
			rx_int7_vbat_mix_switch,
			ARRAY_SIZE(rx_int7_vbat_mix_switch),
			tasha_codec_vbat_enable_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX INT8 VBAT", SND_SOC_NOPM, 0, 0,
			rx_int8_vbat_mix_switch,
			ARRAY_SIZE(rx_int8_vbat_mix_switch),
			tasha_codec_vbat_enable_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("RX INT0 MIX2 INP", WCD9335_CDC_RX0_RX_PATH_CFG1, 4,
			   0, &rx_int0_mix2_inp_mux),
	SND_SOC_DAPM_MUX("RX INT1 MIX2 INP", WCD9335_CDC_RX1_RX_PATH_CFG1, 4,
			   0, &rx_int1_mix2_inp_mux),
	SND_SOC_DAPM_MUX("RX INT2 MIX2 INP", WCD9335_CDC_RX2_RX_PATH_CFG1, 4,
			   0, &rx_int2_mix2_inp_mux),
	SND_SOC_DAPM_MUX("RX INT3 MIX2 INP", WCD9335_CDC_RX3_RX_PATH_CFG1, 4,
			   0, &rx_int3_mix2_inp_mux),
	SND_SOC_DAPM_MUX("RX INT4 MIX2 INP", WCD9335_CDC_RX4_RX_PATH_CFG1, 4,
			   0, &rx_int4_mix2_inp_mux),
	SND_SOC_DAPM_MUX("RX INT7 MIX2 INP", WCD9335_CDC_RX7_RX_PATH_CFG1, 4,
			   0, &rx_int7_mix2_inp_mux),

	SND_SOC_DAPM_MUX("SLIM TX0 MUX", SND_SOC_NOPM, TASHA_TX0, 0,
		&sb_tx0_mux),
	SND_SOC_DAPM_MUX("SLIM TX1 MUX", SND_SOC_NOPM, TASHA_TX1, 0,
		&sb_tx1_mux),
	SND_SOC_DAPM_MUX("SLIM TX2 MUX", SND_SOC_NOPM, TASHA_TX2, 0,
		&sb_tx2_mux),
	SND_SOC_DAPM_MUX("SLIM TX3 MUX", SND_SOC_NOPM, TASHA_TX3, 0,
		&sb_tx3_mux),
	SND_SOC_DAPM_MUX("SLIM TX4 MUX", SND_SOC_NOPM, TASHA_TX4, 0,
		&sb_tx4_mux),
	SND_SOC_DAPM_MUX("SLIM TX5 MUX", SND_SOC_NOPM, TASHA_TX5, 0,
		&sb_tx5_mux),
	SND_SOC_DAPM_MUX("SLIM TX6 MUX", SND_SOC_NOPM, TASHA_TX6, 0,
		&sb_tx6_mux),
	SND_SOC_DAPM_MUX("SLIM TX7 MUX", SND_SOC_NOPM, TASHA_TX7, 0,
		&sb_tx7_mux),
	SND_SOC_DAPM_MUX("SLIM TX8 MUX", SND_SOC_NOPM, TASHA_TX8, 0,
		&sb_tx8_mux),
	SND_SOC_DAPM_MUX("SLIM TX9 MUX", SND_SOC_NOPM, TASHA_TX9, 0,
		&sb_tx9_mux),
	SND_SOC_DAPM_MUX("SLIM TX10 MUX", SND_SOC_NOPM, TASHA_TX10, 0,
		&sb_tx10_mux),
	SND_SOC_DAPM_MUX("SLIM TX11 MUX", SND_SOC_NOPM, TASHA_TX11, 0,
		&sb_tx11_mux),
	SND_SOC_DAPM_MUX("SLIM TX11 INP1 MUX", SND_SOC_NOPM, TASHA_TX11, 0,
		&sb_tx11_inp1_mux),
	SND_SOC_DAPM_MUX("SLIM TX13 MUX", SND_SOC_NOPM, TASHA_TX13, 0,
		&sb_tx13_mux),
	SND_SOC_DAPM_MUX("TX13 INP MUX", SND_SOC_NOPM, 0, 0,
			 &tx13_inp_mux),

	SND_SOC_DAPM_MUX_E("ADC MUX0", WCD9335_CDC_TX0_TX_PATH_CTL, 5, 0,
			   &tx_adc_mux0, tasha_codec_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX1", WCD9335_CDC_TX1_TX_PATH_CTL, 5, 0,
			   &tx_adc_mux1, tasha_codec_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX2", WCD9335_CDC_TX2_TX_PATH_CTL, 5, 0,
			   &tx_adc_mux2, tasha_codec_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX3", WCD9335_CDC_TX3_TX_PATH_CTL, 5, 0,
			   &tx_adc_mux3, tasha_codec_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX4", WCD9335_CDC_TX4_TX_PATH_CTL, 5, 0,
			   &tx_adc_mux4, tasha_codec_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX5", WCD9335_CDC_TX5_TX_PATH_CTL, 5, 0,
			   &tx_adc_mux5, tasha_codec_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX6", WCD9335_CDC_TX6_TX_PATH_CTL, 5, 0,
			   &tx_adc_mux6, tasha_codec_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX7", WCD9335_CDC_TX7_TX_PATH_CTL, 5, 0,
			   &tx_adc_mux7, tasha_codec_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX8", WCD9335_CDC_TX8_TX_PATH_CTL, 5, 0,
			   &tx_adc_mux8, tasha_codec_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX10", SND_SOC_NOPM, 10, 0,
			 &tx_adc_mux10, tasha_codec_tx_adc_cfg,
			 SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("ADC MUX11", SND_SOC_NOPM, 11, 0,
			 &tx_adc_mux11, tasha_codec_tx_adc_cfg,
			 SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("ADC MUX12", SND_SOC_NOPM, 12, 0,
			 &tx_adc_mux12, tasha_codec_tx_adc_cfg,
			 SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("ADC MUX13", SND_SOC_NOPM, 13, 0,
			 &tx_adc_mux13, tasha_codec_tx_adc_cfg,
			 SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX("DMIC MUX0", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux0),
	SND_SOC_DAPM_MUX("DMIC MUX1", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux1),
	SND_SOC_DAPM_MUX("DMIC MUX2", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux2),
	SND_SOC_DAPM_MUX("DMIC MUX3", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux3),
	SND_SOC_DAPM_MUX("DMIC MUX4", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux4),
	SND_SOC_DAPM_MUX("DMIC MUX5", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux5),
	SND_SOC_DAPM_MUX("DMIC MUX6", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux6),
	SND_SOC_DAPM_MUX("DMIC MUX7", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux7),
	SND_SOC_DAPM_MUX("DMIC MUX8", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux8),
	SND_SOC_DAPM_MUX("DMIC MUX10", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux10),
	SND_SOC_DAPM_MUX("DMIC MUX11", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux11),
	SND_SOC_DAPM_MUX("DMIC MUX12", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux12),
	SND_SOC_DAPM_MUX("DMIC MUX13", SND_SOC_NOPM, 0, 0,
		&tx_dmic_mux13),

	SND_SOC_DAPM_MUX("AMIC MUX0", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux0),
	SND_SOC_DAPM_MUX("AMIC MUX1", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux1),
	SND_SOC_DAPM_MUX("AMIC MUX2", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux2),
	SND_SOC_DAPM_MUX("AMIC MUX3", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux3),
	SND_SOC_DAPM_MUX("AMIC MUX4", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux4),
	SND_SOC_DAPM_MUX("AMIC MUX5", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux5),
	SND_SOC_DAPM_MUX("AMIC MUX6", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux6),
	SND_SOC_DAPM_MUX("AMIC MUX7", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux7),
	SND_SOC_DAPM_MUX("AMIC MUX8", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux8),
	SND_SOC_DAPM_MUX("AMIC MUX10", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux10),
	SND_SOC_DAPM_MUX("AMIC MUX11", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux11),
	SND_SOC_DAPM_MUX("AMIC MUX12", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux12),
	SND_SOC_DAPM_MUX("AMIC MUX13", SND_SOC_NOPM, 0, 0,
		&tx_amic_mux13),

	SND_SOC_DAPM_ADC_E("ADC1", NULL, WCD9335_ANA_AMIC1, 7, 0,
			   tasha_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("ADC2", NULL, WCD9335_ANA_AMIC2, 7, 0,
			   tasha_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("ADC3", NULL, WCD9335_ANA_AMIC3, 7, 0,
			   tasha_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("ADC4", NULL, WCD9335_ANA_AMIC4, 7, 0,
			   tasha_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("ADC5", NULL, WCD9335_ANA_AMIC5, 7, 0,
			   tasha_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("ADC6", NULL, WCD9335_ANA_AMIC6, 7, 0,
			   tasha_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_SUPPLY("RX INT1 NATIVE SUPPLY", SND_SOC_NOPM,
			    INTERP_HPHL, 0, tasha_enable_native_supply,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("RX INT2 NATIVE SUPPLY", SND_SOC_NOPM,
			    INTERP_HPHR, 0, tasha_enable_native_supply,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("RX INT3 NATIVE SUPPLY", SND_SOC_NOPM,
			    INTERP_LO1, 0, tasha_enable_native_supply,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("RX INT4 NATIVE SUPPLY", SND_SOC_NOPM,
			    INTERP_LO2, 0, tasha_enable_native_supply,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1", SND_SOC_NOPM, 0, 0,
			       tasha_codec_enable_micbias,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2", SND_SOC_NOPM, 0, 0,
			       tasha_codec_enable_micbias,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3", SND_SOC_NOPM, 0, 0,
			       tasha_codec_enable_micbias,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS4", SND_SOC_NOPM, 0, 0,
			       tasha_codec_enable_micbias,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS1_STANDALONE, SND_SOC_NOPM, 0, 0,
			       tasha_codec_force_enable_micbias,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS2_STANDALONE, SND_SOC_NOPM, 0, 0,
			       tasha_codec_force_enable_micbias,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS3_STANDALONE, SND_SOC_NOPM, 0, 0,
			       tasha_codec_force_enable_micbias,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS4_STANDALONE, SND_SOC_NOPM, 0, 0,
			       tasha_codec_force_enable_micbias,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY(DAPM_LDO_H_STANDALONE, SND_SOC_NOPM, 0, 0,
			    tasha_codec_force_enable_ldo_h,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("ANC0 FB MUX", SND_SOC_NOPM, 0, 0, &anc0_fb_mux),
	SND_SOC_DAPM_MUX("ANC1 FB MUX", SND_SOC_NOPM, 0, 0, &anc1_fb_mux),

	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_INPUT("AMIC3"),
	SND_SOC_DAPM_INPUT("AMIC4"),
	SND_SOC_DAPM_INPUT("AMIC5"),
	SND_SOC_DAPM_INPUT("AMIC6"),

	SND_SOC_DAPM_AIF_OUT_E("AIF1 CAP", "AIF1 Capture", 0, SND_SOC_NOPM,
		AIF1_CAP, 0, tasha_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AIF2 CAP", "AIF2 Capture", 0, SND_SOC_NOPM,
		AIF2_CAP, 0, tasha_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AIF3 CAP", "AIF3 Capture", 0, SND_SOC_NOPM,
		AIF3_CAP, 0, tasha_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AIF4 VI", "VIfeed", 0, SND_SOC_NOPM,
		AIF4_VIFEED, 0, tasha_codec_enable_slimvi_feedback,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("AIF4_VI Mixer", SND_SOC_NOPM, AIF4_VIFEED, 0,
		aif4_vi_mixer, ARRAY_SIZE(aif4_vi_mixer)),

	SND_SOC_DAPM_MIXER("AIF1_CAP Mixer", SND_SOC_NOPM, AIF1_CAP, 0,
		aif1_cap_mixer, ARRAY_SIZE(aif1_cap_mixer)),

	SND_SOC_DAPM_MIXER("AIF2_CAP Mixer", SND_SOC_NOPM, AIF2_CAP, 0,
		aif2_cap_mixer, ARRAY_SIZE(aif2_cap_mixer)),

	SND_SOC_DAPM_MIXER("AIF3_CAP Mixer", SND_SOC_NOPM, AIF3_CAP, 0,
		aif3_cap_mixer, ARRAY_SIZE(aif3_cap_mixer)),

	SND_SOC_DAPM_MIXER("AIF4_MAD Mixer", SND_SOC_NOPM, AIF4_MAD_TX, 0,
		aif4_mad_mixer, ARRAY_SIZE(aif4_mad_mixer)),

	SND_SOC_DAPM_INPUT("VIINPUT"),

	SND_SOC_DAPM_AIF_OUT("AIF5 CPE", "AIF5 CPE TX", 0, SND_SOC_NOPM,
			     AIF5_CPE_TX, 0),

	SND_SOC_DAPM_MUX_E("EC BUF MUX INP", SND_SOC_NOPM, 0, 0, &ec_buf_mux,
		tasha_codec_ec_buf_mux_enable,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* Digital Mic Inputs */
	SND_SOC_DAPM_ADC_E("DMIC0", NULL, SND_SOC_NOPM, 0, 0,
		tasha_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		tasha_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		tasha_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC3", NULL, SND_SOC_NOPM, 0, 0,
		tasha_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC4", NULL, SND_SOC_NOPM, 0, 0,
		tasha_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC5", NULL, SND_SOC_NOPM, 0, 0,
		tasha_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("IIR0 INP0 MUX", SND_SOC_NOPM, 0, 0, &iir0_inp0_mux),
	SND_SOC_DAPM_MUX("IIR0 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir0_inp1_mux),
	SND_SOC_DAPM_MUX("IIR0 INP2 MUX", SND_SOC_NOPM, 0, 0, &iir0_inp2_mux),
	SND_SOC_DAPM_MUX("IIR0 INP3 MUX", SND_SOC_NOPM, 0, 0, &iir0_inp3_mux),
	SND_SOC_DAPM_MUX("IIR1 INP0 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp0_mux),
	SND_SOC_DAPM_MUX("IIR1 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp1_mux),
	SND_SOC_DAPM_MUX("IIR1 INP2 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp2_mux),
	SND_SOC_DAPM_MUX("IIR1 INP3 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp3_mux),

	SND_SOC_DAPM_MIXER_E("IIR0", WCD9335_CDC_SIDETONE_IIR0_IIR_PATH_CTL,
			     4, 0, NULL, 0, tasha_codec_set_iir_gain,
			     SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("IIR1", WCD9335_CDC_SIDETONE_IIR1_IIR_PATH_CTL,
			     4, 0, NULL, 0, tasha_codec_set_iir_gain,
			     SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER("SRC0", WCD9335_CDC_SIDETONE_SRC0_ST_SRC_PATH_CTL,
			     4, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SRC1", WCD9335_CDC_SIDETONE_SRC1_ST_SRC_PATH_CTL,
			     4, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("CPE IN Mixer", SND_SOC_NOPM, 0, 0,
				cpe_in_mix_switch,
				ARRAY_SIZE(cpe_in_mix_switch),
				tasha_codec_configure_cpe_input,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("RX INT1_1 NATIVE MUX", SND_SOC_NOPM, 0, 0,
		&int1_1_native_mux),
	SND_SOC_DAPM_MUX("RX INT2_1 NATIVE MUX", SND_SOC_NOPM, 0, 0,
		&int2_1_native_mux),
	SND_SOC_DAPM_MUX("RX INT3_1 NATIVE MUX", SND_SOC_NOPM, 0, 0,
		&int3_1_native_mux),
	SND_SOC_DAPM_MUX("RX INT4_1 NATIVE MUX", SND_SOC_NOPM, 0, 0,
		&int4_1_native_mux),
	SND_SOC_DAPM_MUX("RX MIX TX0 MUX", SND_SOC_NOPM, 0, 0,
		&rx_mix_tx0_mux),
	SND_SOC_DAPM_MUX("RX MIX TX1 MUX", SND_SOC_NOPM, 0, 0,
		&rx_mix_tx1_mux),
	SND_SOC_DAPM_MUX("RX MIX TX2 MUX", SND_SOC_NOPM, 0, 0,
		&rx_mix_tx2_mux),
	SND_SOC_DAPM_MUX("RX MIX TX3 MUX", SND_SOC_NOPM, 0, 0,
		&rx_mix_tx3_mux),
	SND_SOC_DAPM_MUX("RX MIX TX4 MUX", SND_SOC_NOPM, 0, 0,
		&rx_mix_tx4_mux),
	SND_SOC_DAPM_MUX("RX MIX TX5 MUX", SND_SOC_NOPM, 0, 0,
		&rx_mix_tx5_mux),
	SND_SOC_DAPM_MUX("RX MIX TX6 MUX", SND_SOC_NOPM, 0, 0,
		&rx_mix_tx6_mux),
	SND_SOC_DAPM_MUX("RX MIX TX7 MUX", SND_SOC_NOPM, 0, 0,
		&rx_mix_tx7_mux),
	SND_SOC_DAPM_MUX("RX MIX TX8 MUX", SND_SOC_NOPM, 0, 0,
		&rx_mix_tx8_mux),

	SND_SOC_DAPM_MUX("RX INT0 DEM MUX", SND_SOC_NOPM, 0, 0,
		&rx_int0_dem_inp_mux),
	SND_SOC_DAPM_MUX("RX INT1 DEM MUX", SND_SOC_NOPM, 0, 0,
		&rx_int1_dem_inp_mux),
	SND_SOC_DAPM_MUX("RX INT2 DEM MUX", SND_SOC_NOPM, 0, 0,
		&rx_int2_dem_inp_mux),

	SND_SOC_DAPM_MUX_E("RX INT0 INTERP", SND_SOC_NOPM,
		INTERP_EAR, 0, &rx_int0_interp_mux,
		tasha_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT1 INTERP", SND_SOC_NOPM,
		INTERP_HPHL, 0, &rx_int1_interp_mux,
		tasha_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT2 INTERP", SND_SOC_NOPM,
		INTERP_HPHR, 0, &rx_int2_interp_mux,
		tasha_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT3 INTERP", SND_SOC_NOPM,
		INTERP_LO1, 0, &rx_int3_interp_mux,
		tasha_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT4 INTERP", SND_SOC_NOPM,
		INTERP_LO2, 0, &rx_int4_interp_mux,
		tasha_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT5 INTERP", SND_SOC_NOPM,
		INTERP_LO3, 0, &rx_int5_interp_mux,
		tasha_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT6 INTERP", SND_SOC_NOPM,
		INTERP_LO4, 0, &rx_int6_interp_mux,
		tasha_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7 INTERP", SND_SOC_NOPM,
		INTERP_SPKR1, 0, &rx_int7_interp_mux,
		tasha_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8 INTERP", SND_SOC_NOPM,
		INTERP_SPKR2, 0, &rx_int8_interp_mux,
		tasha_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("RX INT0 DAC", NULL, SND_SOC_NOPM,
		0, 0, tasha_codec_ear_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RX INT1 DAC", NULL, SND_SOC_NOPM,
		0, 0, tasha_codec_hphl_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RX INT2 DAC", NULL, SND_SOC_NOPM,
		0, 0, tasha_codec_hphr_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RX INT3 DAC", NULL, SND_SOC_NOPM,
		0, 0, tasha_codec_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RX INT4 DAC", NULL, SND_SOC_NOPM,
		0, 0, tasha_codec_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RX INT5 DAC", NULL, SND_SOC_NOPM,
		0, 0, tasha_codec_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RX INT6 DAC", NULL, SND_SOC_NOPM,
		0, 0, tasha_codec_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("HPHL PA", SND_SOC_NOPM, 0, 0, NULL, 0,
			   tasha_codec_enable_hphl_pa,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("HPHR PA", SND_SOC_NOPM, 0, 0, NULL, 0,
			   tasha_codec_enable_hphr_pa,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("EAR PA", WCD9335_ANA_EAR, 7, 0, NULL, 0,
			   tasha_codec_enable_ear_pa,
			   SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT1 PA", WCD9335_ANA_LO_1_2, 7, 0, NULL, 0,
			   tasha_codec_enable_lineout_pa,
			   SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT2 PA", WCD9335_ANA_LO_1_2, 6, 0, NULL, 0,
			   tasha_codec_enable_lineout_pa,
			   SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT3 PA", WCD9335_ANA_LO_3_4, 7, 0, NULL, 0,
			   tasha_codec_enable_lineout_pa,
			   SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT4 PA", WCD9335_ANA_LO_3_4, 6, 0, NULL, 0,
			   tasha_codec_enable_lineout_pa,
			   SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC EAR PA", WCD9335_ANA_EAR, 7, 0, NULL, 0,
			   tasha_codec_enable_ear_pa,
			   SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC HPHL PA", SND_SOC_NOPM, 0, 0, NULL, 0,
			   tasha_codec_enable_hphl_pa,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC HPHR PA", SND_SOC_NOPM, 0, 0, NULL, 0,
			   tasha_codec_enable_hphr_pa,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC LINEOUT1 PA", WCD9335_ANA_LO_1_2,
				7, 0, NULL, 0,
				tasha_codec_enable_lineout_pa,
				SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC LINEOUT2 PA", WCD9335_ANA_LO_1_2,
				6, 0, NULL, 0,
				tasha_codec_enable_lineout_pa,
				SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC SPK1 PA", SND_SOC_NOPM, 0, 0, NULL, 0,
			   tasha_codec_enable_spk_anc,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("HPHL"),
	SND_SOC_DAPM_OUTPUT("HPHR"),
	SND_SOC_DAPM_OUTPUT("ANC HPHL"),
	SND_SOC_DAPM_OUTPUT("ANC HPHR"),
	SND_SOC_DAPM_SUPPLY("RX_BIAS", SND_SOC_NOPM, 0, 0,
		tasha_codec_enable_rx_bias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUTPUT("SPK1 OUT"),
	SND_SOC_DAPM_OUTPUT("SPK2 OUT"),
	SND_SOC_DAPM_OUTPUT("LINEOUT1"),
	SND_SOC_DAPM_OUTPUT("LINEOUT2"),
	SND_SOC_DAPM_OUTPUT("LINEOUT3"),
	SND_SOC_DAPM_OUTPUT("LINEOUT4"),
	SND_SOC_DAPM_OUTPUT("ANC LINEOUT1"),
	SND_SOC_DAPM_OUTPUT("ANC LINEOUT2"),
	SND_SOC_DAPM_SUPPLY("MICBIAS_REGULATOR", SND_SOC_NOPM,
		ON_DEMAND_MICBIAS, 0,
		tasha_codec_enable_on_demand_supply,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SWITCH("ADC US MUX0", WCD9335_CDC_TX0_TX_PATH_192_CTL, 0,
			    0, &adc_us_mux0_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX1", WCD9335_CDC_TX1_TX_PATH_192_CTL, 0,
			    0, &adc_us_mux1_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX2", WCD9335_CDC_TX2_TX_PATH_192_CTL, 0,
			    0, &adc_us_mux2_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX3", WCD9335_CDC_TX3_TX_PATH_192_CTL, 0,
			    0, &adc_us_mux3_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX4", WCD9335_CDC_TX4_TX_PATH_192_CTL, 0,
			    0, &adc_us_mux4_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX5", WCD9335_CDC_TX5_TX_PATH_192_CTL, 0,
			    0, &adc_us_mux5_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX6", WCD9335_CDC_TX6_TX_PATH_192_CTL, 0,
			    0, &adc_us_mux6_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX7", WCD9335_CDC_TX7_TX_PATH_192_CTL, 0,
			    0, &adc_us_mux7_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX8", WCD9335_CDC_TX8_TX_PATH_192_CTL, 0,
			    0, &adc_us_mux8_switch),
	/* MAD related widgets */
	SND_SOC_DAPM_AIF_OUT_E("AIF4 MAD", "AIF4 MAD TX", 0,
			       SND_SOC_NOPM, 0, 0,
			       tasha_codec_enable_mad,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("MAD_SEL MUX", SND_SOC_NOPM, 0, 0,
			 &mad_sel_mux),
	SND_SOC_DAPM_INPUT("MAD_CPE_INPUT"),
	SND_SOC_DAPM_INPUT("MADINPUT"),
	SND_SOC_DAPM_SWITCH("MADONOFF", SND_SOC_NOPM, 0, 0,
			    &aif4_mad_switch),
	SND_SOC_DAPM_SWITCH("MAD_BROADCAST", SND_SOC_NOPM, 0, 0,
			    &mad_brdcst_switch),
	SND_SOC_DAPM_SWITCH("AIF4", SND_SOC_NOPM, 0, 0,
			    &aif4_switch_mixer_controls),
	SND_SOC_DAPM_SWITCH("ANC HPHL Enable", SND_SOC_NOPM, 0, 0,
			&anc_hphl_switch),
	SND_SOC_DAPM_SWITCH("ANC HPHR Enable", SND_SOC_NOPM, 0, 0,
			&anc_hphr_switch),
	SND_SOC_DAPM_SWITCH("ANC EAR Enable", SND_SOC_NOPM, 0, 0,
			&anc_ear_switch),
	SND_SOC_DAPM_SWITCH("ANC OUT EAR SPKR Enable", SND_SOC_NOPM, 0, 0,
			    &anc_ear_spkr_switch),
	SND_SOC_DAPM_SWITCH("ANC LINEOUT1 Enable", SND_SOC_NOPM, 0, 0,
			&anc_lineout1_switch),
	SND_SOC_DAPM_SWITCH("ANC LINEOUT2 Enable", SND_SOC_NOPM, 0, 0,
			&anc_lineout2_switch),
	SND_SOC_DAPM_SWITCH("ANC SPKR PA Enable", SND_SOC_NOPM, 0, 0,
			    &anc_spkr_pa_switch),
};

static int tasha_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)
{
	struct tasha_priv *tasha_p = snd_soc_codec_get_drvdata(dai->codec);
	u32 i = 0;
	struct wcd9xxx_ch *ch;

	switch (dai->id) {
	case AIF1_PB:
	case AIF2_PB:
	case AIF3_PB:
	case AIF4_PB:
	case AIF_MIX1_PB:
		if (!rx_slot || !rx_num) {
			pr_err("%s: Invalid rx_slot %pK or rx_num %pK\n",
				 __func__, rx_slot, rx_num);
			return -EINVAL;
		}
		list_for_each_entry(ch, &tasha_p->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			pr_debug("%s: slot_num %u ch->ch_num %d\n",
				 __func__, i, ch->ch_num);
			rx_slot[i++] = ch->ch_num;
		}
		pr_debug("%s: rx_num %d\n", __func__, i);
		*rx_num = i;
		break;
	case AIF1_CAP:
	case AIF2_CAP:
	case AIF3_CAP:
	case AIF4_MAD_TX:
	case AIF4_VIFEED:
		if (!tx_slot || !tx_num) {
			pr_err("%s: Invalid tx_slot %pK or tx_num %pK\n",
				 __func__, tx_slot, tx_num);
			return -EINVAL;
		}
		list_for_each_entry(ch, &tasha_p->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			pr_debug("%s: slot_num %u ch->ch_num %d\n",
				 __func__, i,  ch->ch_num);
			tx_slot[i++] = ch->ch_num;
		}
		pr_debug("%s: tx_num %d\n", __func__, i);
		*tx_num = i;
		break;

	default:
		pr_err("%s: Invalid DAI ID %x\n", __func__, dai->id);
		break;
	}

	return 0;
}

static int tasha_set_channel_map(struct snd_soc_dai *dai,
				 unsigned int tx_num, unsigned int *tx_slot,
				 unsigned int rx_num, unsigned int *rx_slot)
{
	struct tasha_priv *tasha;
	struct wcd9xxx *core;
	struct wcd9xxx_codec_dai_data *dai_data = NULL;

	if (!dai) {
		pr_err("%s: dai is empty\n", __func__);
		return -EINVAL;
	}
	tasha = snd_soc_codec_get_drvdata(dai->codec);
	core = dev_get_drvdata(dai->codec->dev->parent);

	if (!tx_slot || !rx_slot) {
		pr_err("%s: Invalid tx_slot=%pK, rx_slot=%pK\n",
			__func__, tx_slot, rx_slot);
		return -EINVAL;
	}
	pr_debug("%s(): dai_name = %s DAI-ID %x tx_ch %d rx_ch %d\n"
		 "tasha->intf_type %d\n",
		 __func__, dai->name, dai->id, tx_num, rx_num,
		 tasha->intf_type);

	if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		wcd9xxx_init_slimslave(core, core->slim->laddr,
					   tx_num, tx_slot, rx_num, rx_slot);
		/* Reserve TX12/TX13 for MAD data channel */
		dai_data = &tasha->dai[AIF4_MAD_TX];
		if (dai_data) {
			if (TASHA_IS_2_0(tasha->wcd9xxx))
				list_add_tail(&core->tx_chs[TASHA_TX13].list,
					      &dai_data->wcd9xxx_ch_list);
			else
				list_add_tail(&core->tx_chs[TASHA_TX12].list,
					      &dai_data->wcd9xxx_ch_list);
		}
	}
	return 0;
}

static int tasha_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	return 0;
}

static void tasha_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(dai->codec);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_I2C)
		return;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		tasha_codec_vote_max_bw(dai->codec, false);
}

static int tasha_set_decimator_rate(struct snd_soc_dai *dai,
				    u8 tx_fs_rate_reg_val, u32 sample_rate)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wcd9xxx_ch *ch;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u32 tx_port = 0;
	u8 shift = 0, shift_val = 0, tx_mux_sel = 0;
	int decimator = -1;
	u16 tx_port_reg = 0, tx_fs_reg = 0;

	list_for_each_entry(ch, &tasha->dai[dai->id].wcd9xxx_ch_list, list) {
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
			tx_port_reg = WCD9335_CDC_IF_ROUTER_TX_MUX_CFG0;
			shift = (tx_port << 1);
			shift_val = 0x03;
		} else if ((tx_port >= 4) && (tx_port < 8)) {
			tx_port_reg = WCD9335_CDC_IF_ROUTER_TX_MUX_CFG1;
			shift = ((tx_port - 4) << 1);
			shift_val = 0x03;
		} else if ((tx_port >= 8) && (tx_port < 11)) {
			tx_port_reg = WCD9335_CDC_IF_ROUTER_TX_MUX_CFG2;
			shift = ((tx_port - 8) << 1);
			shift_val = 0x03;
		} else if (tx_port == 11) {
			tx_port_reg = WCD9335_CDC_IF_ROUTER_TX_MUX_CFG3;
			shift = 0;
			shift_val = 0x0F;
		} else if (tx_port == 13) {
			tx_port_reg = WCD9335_CDC_IF_ROUTER_TX_MUX_CFG3;
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
			tx_fs_reg = WCD9335_CDC_TX0_TX_PATH_CTL +
				    16 * decimator;
			dev_dbg(codec->dev, "%s: set DEC%u (-> SLIM_TX%u) rate to %u\n",
				__func__, decimator, tx_port, sample_rate);
			snd_soc_update_bits(codec, tx_fs_reg, 0x0F,
					    tx_fs_rate_reg_val);
		} else if ((tx_port <= 8) && (tx_mux_sel == 0x01)) {
			/* Check if the TX Mux input is RX MIX TXn */
			dev_dbg(codec->dev, "%s: RX_MIX_TX%u going to SLIM TX%u\n",
					__func__, tx_port, tx_port);
		} else {
			dev_err(codec->dev, "%s: ERROR: Invalid decimator: %d\n",
				__func__, decimator);
			return -EINVAL;
		}
	}
	return 0;
}

static int tasha_set_mix_interpolator_rate(struct snd_soc_dai *dai,
					   u8 int_mix_fs_rate_reg_val,
					   u32 sample_rate)
{
	u8 int_2_inp;
	u32 j;
	u16 int_mux_cfg1, int_fs_reg;
	u8 int_mux_cfg1_val;
	struct snd_soc_codec *codec = dai->codec;
	struct wcd9xxx_ch *ch;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	list_for_each_entry(ch, &tasha->dai[dai->id].wcd9xxx_ch_list, list) {
		int_2_inp = ch->port + INTn_2_INP_SEL_RX0 -
				  TASHA_RX_PORT_START_NUMBER;
		if ((int_2_inp < INTn_2_INP_SEL_RX0) ||
		   (int_2_inp > INTn_2_INP_SEL_RX7)) {
			pr_err("%s: Invalid RX%u port, Dai ID is %d\n",
				__func__,
				(ch->port - TASHA_RX_PORT_START_NUMBER),
				dai->id);
			return -EINVAL;
		}

		int_mux_cfg1 = WCD9335_CDC_RX_INP_MUX_RX_INT0_CFG1;
		for (j = 0; j < TASHA_NUM_INTERPOLATORS; j++) {
			int_mux_cfg1_val = snd_soc_read(codec, int_mux_cfg1) &
						0x0F;
			if (int_mux_cfg1_val == int_2_inp) {
				int_fs_reg = WCD9335_CDC_RX0_RX_PATH_MIX_CTL +
						20 * j;
				pr_debug("%s: AIF_MIX_PB DAI(%d) connected to INT%u_2\n",
					  __func__, dai->id, j);
				pr_debug("%s: set INT%u_2 sample rate to %u\n",
					__func__, j, sample_rate);
				snd_soc_update_bits(codec, int_fs_reg,
						0x0F, int_mix_fs_rate_reg_val);
			}
			int_mux_cfg1 += 2;
		}
	}
	return 0;
}

static int tasha_set_prim_interpolator_rate(struct snd_soc_dai *dai,
					    u8 int_prim_fs_rate_reg_val,
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
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	list_for_each_entry(ch, &tasha->dai[dai->id].wcd9xxx_ch_list, list) {
		int_1_mix1_inp = ch->port + INTn_1_MIX_INP_SEL_RX0 -
				  TASHA_RX_PORT_START_NUMBER;
		if ((int_1_mix1_inp < INTn_1_MIX_INP_SEL_RX0) ||
		   (int_1_mix1_inp > INTn_1_MIX_INP_SEL_RX7)) {
			pr_err("%s: Invalid RX%u port, Dai ID is %d\n",
				__func__,
				(ch->port - TASHA_RX_PORT_START_NUMBER),
				dai->id);
			return -EINVAL;
		}

		int_mux_cfg0 = WCD9335_CDC_RX_INP_MUX_RX_INT0_CFG0;

		/*
		 * Loop through all interpolator MUX inputs and find out
		 * to which interpolator input, the slim rx port
		 * is connected
		 */
		for (j = 0; j < TASHA_NUM_INTERPOLATORS; j++) {
			int_mux_cfg1 = int_mux_cfg0 + 1;

			int_mux_cfg0_val = snd_soc_read(codec, int_mux_cfg0);
			int_mux_cfg1_val = snd_soc_read(codec, int_mux_cfg1);
			inp0_sel = int_mux_cfg0_val & 0x0F;
			inp1_sel = (int_mux_cfg0_val >> 4) & 0x0F;
			inp2_sel = (int_mux_cfg1_val >> 4) & 0x0F;
			if ((inp0_sel == int_1_mix1_inp) ||
			    (inp1_sel == int_1_mix1_inp) ||
			    (inp2_sel == int_1_mix1_inp)) {
				int_fs_reg = WCD9335_CDC_RX0_RX_PATH_CTL +
					     20 * j;
				pr_debug("%s: AIF_PB DAI(%d) connected to INT%u_1\n",
					  __func__, dai->id, j);
				pr_debug("%s: set INT%u_1 sample rate to %u\n",
					__func__, j, sample_rate);
				/* sample_rate is in Hz */
				if ((j == 0) && (sample_rate == 44100)) {
					pr_info("%s: Cannot set 44.1KHz on INT0\n",
						__func__);
				} else
					snd_soc_update_bits(codec, int_fs_reg,
						0x0F, int_prim_fs_rate_reg_val);
			}
			int_mux_cfg0 += 2;
		}
	}

	return 0;
}


static int tasha_set_interpolator_rate(struct snd_soc_dai *dai,
				       u32 sample_rate)
{
	int rate_val = 0;
	int i, ret;

	/* set mixing path rate */
	for (i = 0; i < ARRAY_SIZE(int_mix_sample_rate_val); i++) {
		if (sample_rate ==
				int_mix_sample_rate_val[i].sample_rate) {
			rate_val =
				int_mix_sample_rate_val[i].rate_val;
			break;
		}
	}
	if ((i == ARRAY_SIZE(int_mix_sample_rate_val)) ||
			(rate_val < 0))
		goto prim_rate;
	ret = tasha_set_mix_interpolator_rate(dai,
			(u8) rate_val, sample_rate);
prim_rate:
	/* set primary path sample rate */
	for (i = 0; i < ARRAY_SIZE(int_prim_sample_rate_val); i++) {
		if (sample_rate ==
				int_prim_sample_rate_val[i].sample_rate) {
			rate_val =
				int_prim_sample_rate_val[i].rate_val;
			break;
		}
	}
	if ((i == ARRAY_SIZE(int_prim_sample_rate_val)) ||
			(rate_val < 0))
		return -EINVAL;
	ret = tasha_set_prim_interpolator_rate(dai,
			(u8) rate_val, sample_rate);
	return ret;
}

static int tasha_prepare(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		tasha_codec_vote_max_bw(dai->codec, false);
	return 0;
}

static int tasha_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(dai->codec);
	int ret;
	int tx_fs_rate = -EINVAL;
	int rx_fs_rate = -EINVAL;
	int i2s_bit_mode;
	struct snd_soc_codec *codec = dai->codec;

	pr_debug("%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n", __func__,
		 dai->name, dai->id, params_rate(params),
		 params_channels(params));

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = tasha_set_interpolator_rate(dai, params_rate(params));
		if (ret) {
			pr_err("%s: cannot set sample rate: %u\n",
				__func__, params_rate(params));
			return ret;
		}
		switch (params_width(params)) {
		case 16:
			tasha->dai[dai->id].bit_width = 16;
			i2s_bit_mode = 0x01;
			break;
		case 24:
			tasha->dai[dai->id].bit_width = 24;
			i2s_bit_mode = 0x00;
			break;
		default:
			return -EINVAL;
		}
		tasha->dai[dai->id].rate = params_rate(params);
		if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			switch (params_rate(params)) {
			case 8000:
				rx_fs_rate = 0;
				break;
			case 16000:
				rx_fs_rate = 1;
				break;
			case 32000:
				rx_fs_rate = 2;
				break;
			case 48000:
				rx_fs_rate = 3;
				break;
			case 96000:
				rx_fs_rate = 4;
				break;
			case 192000:
				rx_fs_rate = 5;
				break;
			default:
				dev_err(tasha->dev,
				"%s: Invalid RX sample rate: %d\n",
				__func__, params_rate(params));
				return -EINVAL;
			};
			snd_soc_update_bits(codec,
					WCD9335_DATA_HUB_DATA_HUB_RX_I2S_CTL,
					0x20, i2s_bit_mode << 5);
			snd_soc_update_bits(codec,
					WCD9335_DATA_HUB_DATA_HUB_RX_I2S_CTL,
					0x1c, (rx_fs_rate << 2));
		}
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		switch (params_rate(params)) {
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
		case 384000:
			tx_fs_rate = 7;
			break;
		default:
			dev_err(tasha->dev, "%s: Invalid TX sample rate: %d\n",
				__func__, params_rate(params));
			return -EINVAL;

		};
		if (dai->id != AIF4_VIFEED &&
		    dai->id != AIF4_MAD_TX) {
			ret = tasha_set_decimator_rate(dai, tx_fs_rate,
					params_rate(params));
			if (ret < 0) {
				dev_err(tasha->dev, "%s: cannot set TX Decimator rate: %d\n",
					__func__, tx_fs_rate);
				return ret;
			}
		}
		tasha->dai[dai->id].rate = params_rate(params);
		switch (params_width(params)) {
		case 16:
			tasha->dai[dai->id].bit_width = 16;
			i2s_bit_mode = 0x01;
			break;
		case 24:
			tasha->dai[dai->id].bit_width = 24;
			i2s_bit_mode = 0x00;
			break;
		case 32:
			tasha->dai[dai->id].bit_width = 32;
			i2s_bit_mode = 0x00;
			break;
		default:
			dev_err(tasha->dev, "%s: Invalid format 0x%x\n",
				__func__, params_width(params));
			return -EINVAL;
		};
		if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			snd_soc_update_bits(codec,
				WCD9335_DATA_HUB_DATA_HUB_TX_I2S_CTL,
				0x20, i2s_bit_mode << 5);
			if (tx_fs_rate > 1)
				tx_fs_rate--;
			snd_soc_update_bits(codec,
				WCD9335_DATA_HUB_DATA_HUB_TX_I2S_CTL,
				0x1c, tx_fs_rate << 2);
			snd_soc_update_bits(codec,
				WCD9335_DATA_HUB_DATA_HUB_TX_I2S_SD0_L_CFG,
				0x05, 0x05);

			snd_soc_update_bits(codec,
				WCD9335_DATA_HUB_DATA_HUB_TX_I2S_SD0_R_CFG,
				0x05, 0x05);

			snd_soc_update_bits(codec,
				WCD9335_DATA_HUB_DATA_HUB_TX_I2S_SD1_L_CFG,
				0x05, 0x05);

			snd_soc_update_bits(codec,
				WCD9335_DATA_HUB_DATA_HUB_TX_I2S_SD1_R_CFG,
				0x05, 0x05);
		}
		break;
	default:
		pr_err("%s: Invalid stream type %d\n", __func__,
			substream->stream);
		return -EINVAL;
	};
	if (dai->id == AIF4_VIFEED)
		tasha->dai[dai->id].bit_width = 32;

	return 0;
}

static int tasha_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(dai->codec);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		/* CPU is master */
		if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			if (dai->id == AIF1_CAP)
				snd_soc_update_bits(dai->codec,
					WCD9335_DATA_HUB_DATA_HUB_TX_I2S_CTL,
					0x2, 0);
			else if (dai->id == AIF1_PB)
				snd_soc_update_bits(dai->codec,
					WCD9335_DATA_HUB_DATA_HUB_RX_I2S_CTL,
					0x2, 0);
		}
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		/* CPU is slave */
		if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			if (dai->id == AIF1_CAP)
				snd_soc_update_bits(dai->codec,
					WCD9335_DATA_HUB_DATA_HUB_TX_I2S_CTL,
					0x2, 0x2);
			else if (dai->id == AIF1_PB)
				snd_soc_update_bits(dai->codec,
					WCD9335_DATA_HUB_DATA_HUB_RX_I2S_CTL,
					0x2, 0x2);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int tasha_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static struct snd_soc_dai_ops tasha_dai_ops = {
	.startup = tasha_startup,
	.shutdown = tasha_shutdown,
	.hw_params = tasha_hw_params,
	.prepare = tasha_prepare,
	.set_sysclk = tasha_set_dai_sysclk,
	.set_fmt = tasha_set_dai_fmt,
	.set_channel_map = tasha_set_channel_map,
	.get_channel_map = tasha_get_channel_map,
};

static struct snd_soc_dai_driver tasha_dai[] = {
	{
		.name = "tasha_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9335_RATES_MASK | WCD9335_FRAC_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9335_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_rx2",
		.id = AIF2_PB,
		.playback = {
			.stream_name = "AIF2 Playback",
			.rates = WCD9335_RATES_MASK | WCD9335_FRAC_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_tx2",
		.id = AIF2_CAP,
		.capture = {
			.stream_name = "AIF2 Capture",
			.rates = WCD9335_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_rx3",
		.id = AIF3_PB,
		.playback = {
			.stream_name = "AIF3 Playback",
			.rates = WCD9335_RATES_MASK | WCD9335_FRAC_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_tx3",
		.id = AIF3_CAP,
		.capture = {
			.stream_name = "AIF3 Capture",
			.rates = WCD9335_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_rx4",
		.id = AIF4_PB,
		.playback = {
			.stream_name = "AIF4 Playback",
			.rates = WCD9335_RATES_MASK | WCD9335_FRAC_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_mix_rx1",
		.id = AIF_MIX1_PB,
		.playback = {
			.stream_name = "AIF Mix Playback",
			.rates = WCD9335_RATES_MASK | WCD9335_FRAC_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_mad1",
		.id = AIF4_MAD_TX,
		.capture = {
			.stream_name = "AIF4 MAD TX",
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000 |
				 SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_384000,
			.formats = TASHA_FORMATS_S16_S24_S32_LE,
			.rate_min = 16000,
			.rate_max = 384000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_vifeedback",
		.id = AIF4_VIFEED,
		.capture = {
			.stream_name = "VIfeed",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_48000,
			.formats = TASHA_FORMATS_S16_S24_S32_LE,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		 },
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_cpe",
		.id = AIF5_CPE_TX,
		.capture = {
			.stream_name = "AIF5 CPE TX",
			.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.formats = TASHA_FORMATS_S16_S24_S32_LE,
			.rate_min = 16000,
			.rate_max = 48000,
			.channels_min = 1,
			.channels_max = 1,
		},
	},
};

static struct snd_soc_dai_driver tasha_i2s_dai[] = {
	{
		.name = "tasha_i2s_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9335_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_i2s_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9335_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_i2s_rx2",
		.id = AIF2_PB,
		.playback = {
			.stream_name = "AIF2 Playback",
			.rates = WCD9335_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tasha_dai_ops,
	},
	{
		.name = "tasha_i2s_tx2",
		.id = AIF2_CAP,
		.capture = {
			.stream_name = "AIF2 Capture",
			.rates = WCD9335_RATES_MASK,
			.formats = TASHA_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tasha_dai_ops,
	},
};

static void tasha_codec_power_gate_digital_core(struct tasha_priv *tasha)
{
	struct snd_soc_codec *codec = tasha->codec;

	if (!codec)
		return;

	mutex_lock(&tasha->power_lock);
	dev_dbg(codec->dev, "%s: Entering power gating function, %d\n",
		__func__, tasha->power_active_ref);

	if (tasha->power_active_ref > 0)
		goto exit;

	wcd9xxx_set_power_state(tasha->wcd9xxx,
			WCD_REGION_POWER_COLLAPSE_BEGIN,
			WCD9XXX_DIG_CORE_REGION_1);
	snd_soc_update_bits(codec, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
			0x04, 0x04);
	snd_soc_update_bits(codec, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
			0x01, 0x00);
	snd_soc_update_bits(codec, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
			0x02, 0x00);
	clear_bit(AUDIO_NOMINAL, &tasha->status_mask);
	tasha_codec_update_sido_voltage(tasha, sido_buck_svs_voltage);
	wcd9xxx_set_power_state(tasha->wcd9xxx, WCD_REGION_POWER_DOWN,
				WCD9XXX_DIG_CORE_REGION_1);
exit:
	dev_dbg(codec->dev, "%s: Exiting power gating function, %d\n",
		__func__, tasha->power_active_ref);
	mutex_unlock(&tasha->power_lock);
}

static void tasha_codec_power_gate_work(struct work_struct *work)
{
	struct tasha_priv *tasha;
	struct delayed_work *dwork;
	struct snd_soc_codec *codec;

	dwork = to_delayed_work(work);
	tasha = container_of(dwork, struct tasha_priv, power_gate_work);
	codec = tasha->codec;

	if (!codec)
		return;

	tasha_codec_power_gate_digital_core(tasha);
}

/* called under power_lock acquisition */
static int tasha_dig_core_remove_power_collapse(struct snd_soc_codec *codec)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	tasha_codec_vote_max_bw(codec, true);
	snd_soc_write(codec, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x5);
	snd_soc_write(codec, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x7);
	snd_soc_write(codec, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x3);
	snd_soc_update_bits(codec, WCD9335_CODEC_RPM_RST_CTL, 0x02, 0x00);
	snd_soc_update_bits(codec, WCD9335_CODEC_RPM_RST_CTL, 0x02, 0x02);

	wcd9xxx_set_power_state(tasha->wcd9xxx,
			WCD_REGION_POWER_COLLAPSE_REMOVE,
			WCD9XXX_DIG_CORE_REGION_1);
	regcache_mark_dirty(codec->component.regmap);
	regcache_sync_region(codec->component.regmap,
			     TASHA_DIG_CORE_REG_MIN, TASHA_DIG_CORE_REG_MAX);
	tasha_codec_vote_max_bw(codec, false);

	return 0;
}

static int tasha_dig_core_power_collapse(struct tasha_priv *tasha,
					 int req_state)
{
	struct snd_soc_codec *codec;
	int cur_state;

	/* Exit if feature is disabled */
	if (!dig_core_collapse_enable)
		return 0;

	mutex_lock(&tasha->power_lock);
	if (req_state == POWER_COLLAPSE)
		tasha->power_active_ref--;
	else if (req_state == POWER_RESUME)
		tasha->power_active_ref++;
	else
		goto unlock_mutex;

	if (tasha->power_active_ref < 0) {
		dev_dbg(tasha->dev, "%s: power_active_ref is negative\n",
			__func__);
		goto unlock_mutex;
	}

	codec = tasha->codec;
	if (!codec)
		goto unlock_mutex;

	if (req_state == POWER_COLLAPSE) {
		if (tasha->power_active_ref == 0) {
			schedule_delayed_work(&tasha->power_gate_work,
			msecs_to_jiffies(dig_core_collapse_timer * 1000));
		}
	} else if (req_state == POWER_RESUME) {
		if (tasha->power_active_ref == 1) {
			/*
			 * At this point, there can be two cases:
			 * 1. Core already in power collapse state
			 * 2. Timer kicked in and still did not expire or
			 * waiting for the power_lock
			 */
			cur_state = wcd9xxx_get_current_power_state(
						tasha->wcd9xxx,
						WCD9XXX_DIG_CORE_REGION_1);
			if (cur_state == WCD_REGION_POWER_DOWN)
				tasha_dig_core_remove_power_collapse(codec);
			else {
				mutex_unlock(&tasha->power_lock);
				cancel_delayed_work_sync(
						&tasha->power_gate_work);
				mutex_lock(&tasha->power_lock);
			}
		}
	}

unlock_mutex:
	mutex_unlock(&tasha->power_lock);

	return 0;
}

static int __tasha_cdc_mclk_enable_locked(struct tasha_priv *tasha,
					  bool enable)
{
	int ret = 0;

	if (!tasha->wcd_ext_clk) {
		dev_err(tasha->dev, "%s: wcd ext clock is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(tasha->dev, "%s: mclk_enable = %u\n", __func__, enable);

	if (enable) {
		tasha_dig_core_power_collapse(tasha, POWER_RESUME);
		ret = tasha_cdc_req_mclk_enable(tasha, true);
		if (ret)
			goto err;

		set_bit(AUDIO_NOMINAL, &tasha->status_mask);
		tasha_codec_apply_sido_voltage(tasha,
				SIDO_VOLTAGE_NOMINAL_MV);
	} else {
		if (!dig_core_collapse_enable) {
			clear_bit(AUDIO_NOMINAL, &tasha->status_mask);
			tasha_codec_update_sido_voltage(tasha,
						sido_buck_svs_voltage);
		}
		tasha_cdc_req_mclk_enable(tasha, false);
		tasha_dig_core_power_collapse(tasha, POWER_COLLAPSE);
	}

err:
	return ret;
}

static int __tasha_cdc_mclk_enable(struct tasha_priv *tasha,
				   bool enable)
{
	int ret;

	WCD9XXX_V2_BG_CLK_LOCK(tasha->resmgr);
	ret = __tasha_cdc_mclk_enable_locked(tasha, enable);
	WCD9XXX_V2_BG_CLK_UNLOCK(tasha->resmgr);

	return ret;
}

int tasha_cdc_mclk_enable(struct snd_soc_codec *codec, int enable, bool dapm)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	return __tasha_cdc_mclk_enable(tasha, enable);
}
EXPORT_SYMBOL(tasha_cdc_mclk_enable);

int tasha_cdc_mclk_tx_enable(struct snd_soc_codec *codec, int enable, bool dapm)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(tasha->dev, "%s: clk_mode: %d, enable: %d, clk_internal: %d\n",
		__func__, tasha->clk_mode, enable, tasha->clk_internal);
	if (tasha->clk_mode || tasha->clk_internal) {
		if (enable) {
			tasha_cdc_sido_ccl_enable(tasha, true);
			wcd_resmgr_enable_master_bias(tasha->resmgr);
			tasha_dig_core_power_collapse(tasha, POWER_RESUME);
			snd_soc_update_bits(codec,
					WCD9335_CDC_CLK_RST_CTRL_FS_CNT_CONTROL,
					0x01, 0x01);
			snd_soc_update_bits(codec,
					WCD9335_CDC_CLK_RST_CTRL_MCLK_CONTROL,
					0x01, 0x01);
			set_bit(CPE_NOMINAL, &tasha->status_mask);
			tasha_codec_update_sido_voltage(tasha,
						SIDO_VOLTAGE_NOMINAL_MV);
			tasha->clk_internal = true;
		} else {
			tasha->clk_internal = false;
			clear_bit(CPE_NOMINAL, &tasha->status_mask);
			tasha_codec_update_sido_voltage(tasha,
						sido_buck_svs_voltage);
			tasha_dig_core_power_collapse(tasha, POWER_COLLAPSE);
			wcd_resmgr_disable_master_bias(tasha->resmgr);
			tasha_cdc_sido_ccl_enable(tasha, false);
		}
	} else {
		ret = __tasha_cdc_mclk_enable(tasha, enable);
	}
	return ret;
}
EXPORT_SYMBOL(tasha_cdc_mclk_tx_enable);

static ssize_t tasha_codec_version_read(struct snd_info_entry *entry,
			       void *file_private_data, struct file *file,
			       char __user *buf, size_t count, loff_t pos)
{
	struct tasha_priv *tasha;
	struct wcd9xxx *wcd9xxx;
	char buffer[TASHA_VERSION_ENTRY_SIZE];
	int len = 0;

	tasha = (struct tasha_priv *) entry->private_data;
	if (!tasha) {
		pr_err("%s: tasha priv is null\n", __func__);
		return -EINVAL;
	}

	wcd9xxx = tasha->wcd9xxx;

	if (wcd9xxx->codec_type->id_major == TASHA_MAJOR) {
		if (TASHA_IS_1_0(wcd9xxx))
			len = snprintf(buffer, sizeof(buffer), "WCD9335_1_0\n");
		else if (TASHA_IS_1_1(wcd9xxx))
			len = snprintf(buffer, sizeof(buffer), "WCD9335_1_1\n");
		else
			snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	} else if (wcd9xxx->codec_type->id_major == TASHA2P0_MAJOR) {
		len = snprintf(buffer, sizeof(buffer), "WCD9335_2_0\n");
	} else
		len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops tasha_codec_info_ops = {
	.read = tasha_codec_version_read,
};

/*
 * tasha_codec_info_create_codec_entry - creates wcd9335 module
 * @codec_root: The parent directory
 * @codec: Codec instance
 *
 * Creates wcd9335 module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int tasha_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					struct snd_soc_codec *codec)
{
	struct snd_info_entry *version_entry;
	struct tasha_priv *tasha;
	struct snd_soc_card *card;

	if (!codec_root || !codec)
		return -EINVAL;

	tasha = snd_soc_codec_get_drvdata(codec);
	card = codec->component.card;
	tasha->entry = snd_info_create_subdir(codec_root->module,
					      "tasha", codec_root);
	if (!tasha->entry) {
		dev_dbg(codec->dev, "%s: failed to create wcd9335 entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   tasha->entry);
	if (!version_entry) {
		dev_dbg(codec->dev, "%s: failed to create wcd9335 version entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry->private_data = tasha;
	version_entry->size = TASHA_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &tasha_codec_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		return -ENOMEM;
	}
	tasha->version_entry = version_entry;

	return 0;
}
EXPORT_SYMBOL(tasha_codec_info_create_codec_entry);

static int __tasha_codec_internal_rco_ctrl(
	struct snd_soc_codec *codec, bool enable)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	if (enable) {
		tasha_cdc_sido_ccl_enable(tasha, true);
		if (wcd_resmgr_get_clk_type(tasha->resmgr) ==
		    WCD_CLK_RCO) {
			ret = wcd_resmgr_enable_clk_block(tasha->resmgr,
							  WCD_CLK_RCO);
		} else {
			ret = tasha_cdc_req_mclk_enable(tasha, true);
			ret |= wcd_resmgr_enable_clk_block(tasha->resmgr,
							   WCD_CLK_RCO);
			ret |= tasha_cdc_req_mclk_enable(tasha, false);
		}

	} else {
		ret = wcd_resmgr_disable_clk_block(tasha->resmgr,
						   WCD_CLK_RCO);
		tasha_cdc_sido_ccl_enable(tasha, false);
	}

	if (ret) {
		dev_err(codec->dev, "%s: Error in %s RCO\n",
			__func__, (enable ? "enabling" : "disabling"));
		ret = -EINVAL;
	}

	return ret;
}

/*
 * tasha_codec_internal_rco_ctrl()
 * Make sure that the caller does not acquire
 * BG_CLK_LOCK.
 */
static int tasha_codec_internal_rco_ctrl(struct snd_soc_codec *codec,
				  bool enable)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	WCD9XXX_V2_BG_CLK_LOCK(tasha->resmgr);
	ret = __tasha_codec_internal_rco_ctrl(codec, enable);
	WCD9XXX_V2_BG_CLK_UNLOCK(tasha->resmgr);
	return ret;
}

/*
 * tasha_mbhc_hs_detect: starts mbhc insertion/removal functionality
 * @codec: handle to snd_soc_codec *
 * @mbhc_cfg: handle to mbhc configuration structure
 * return 0 if mbhc_start is success or error code in case of failure
 */
int tasha_mbhc_hs_detect(struct snd_soc_codec *codec,
			 struct wcd_mbhc_config *mbhc_cfg)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	return wcd_mbhc_start(&tasha->mbhc, mbhc_cfg);
}
EXPORT_SYMBOL(tasha_mbhc_hs_detect);

/*
 * tasha_mbhc_hs_detect_exit: stop mbhc insertion/removal functionality
 * @codec: handle to snd_soc_codec *
 */
void tasha_mbhc_hs_detect_exit(struct snd_soc_codec *codec)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	wcd_mbhc_stop(&tasha->mbhc);
}
EXPORT_SYMBOL(tasha_mbhc_hs_detect_exit);

static int wcd9335_get_micb_vout_ctl_val(u32 micb_mv)
{
	/* min micbias voltage is 1V and maximum is 2.85V */
	if (micb_mv < 1000 || micb_mv > 2850) {
		pr_err("%s: unsupported micbias voltage\n", __func__);
		return -EINVAL;
	}

	return (micb_mv - 1000) / 50;
}

static const struct tasha_reg_mask_val tasha_reg_update_reset_val_1_1[] = {
	{WCD9335_RCO_CTRL_2, 0xFF, 0x47},
	{WCD9335_FLYBACK_VNEG_DAC_CTRL_4, 0xFF, 0x60},
};

static const struct tasha_reg_mask_val tasha_codec_reg_init_val_1_1[] = {
	{WCD9335_FLYBACK_VNEG_DAC_CTRL_1, 0xFF, 0x65},
	{WCD9335_FLYBACK_VNEG_DAC_CTRL_2, 0xFF, 0x52},
	{WCD9335_FLYBACK_VNEG_DAC_CTRL_3, 0xFF, 0xAF},
	{WCD9335_FLYBACK_VNEG_DAC_CTRL_4, 0xFF, 0x60},
	{WCD9335_FLYBACK_VNEG_CTRL_3, 0xFF, 0xF4},
	{WCD9335_FLYBACK_VNEG_CTRL_9, 0xFF, 0x40},
	{WCD9335_FLYBACK_VNEG_CTRL_2, 0xFF, 0x4F},
	{WCD9335_FLYBACK_EN, 0xFF, 0x6E},
	{WCD9335_CDC_RX2_RX_PATH_SEC0, 0xF8, 0xF8},
	{WCD9335_CDC_RX1_RX_PATH_SEC0, 0xF8, 0xF8},
};

static const struct tasha_reg_mask_val tasha_codec_reg_init_val_1_0[] = {
	{WCD9335_FLYBACK_VNEG_CTRL_3, 0xFF, 0x54},
	{WCD9335_CDC_RX2_RX_PATH_SEC0, 0xFC, 0xFC},
	{WCD9335_CDC_RX1_RX_PATH_SEC0, 0xFC, 0xFC},
};

static const struct tasha_reg_mask_val tasha_codec_reg_init_val_2_0[] = {
	{WCD9335_RCO_CTRL_2, 0x0F, 0x08},
	{WCD9335_RX_BIAS_FLYB_MID_RST, 0xF0, 0x10},
	{WCD9335_FLYBACK_CTRL_1, 0x20, 0x20},
	{WCD9335_HPH_OCP_CTL, 0xFF, 0x7A},
	{WCD9335_HPH_L_TEST, 0x01, 0x01},
	{WCD9335_HPH_R_TEST, 0x01, 0x01},
	{WCD9335_CDC_BOOST0_BOOST_CFG1, 0x3F, 0x12},
	{WCD9335_CDC_BOOST0_BOOST_CFG2, 0x1C, 0x08},
	{WCD9335_CDC_COMPANDER7_CTL7, 0x1E, 0x18},
	{WCD9335_CDC_BOOST1_BOOST_CFG1, 0x3F, 0x12},
	{WCD9335_CDC_BOOST1_BOOST_CFG2, 0x1C, 0x08},
	{WCD9335_CDC_COMPANDER8_CTL7, 0x1E, 0x18},
	{WCD9335_CDC_TX0_TX_PATH_SEC7, 0xFF, 0x45},
	{WCD9335_CDC_RX0_RX_PATH_SEC0, 0xFC, 0xF4},
	{WCD9335_HPH_REFBUFF_LP_CTL, 0x08, 0x08},
	{WCD9335_HPH_REFBUFF_LP_CTL, 0x06, 0x02},
	{WCD9335_DIFF_LO_CORE_OUT_PROG, 0xFC, 0xA0},
	{WCD9335_SE_LO_COM1, 0xFF, 0xC0},
	{WCD9335_CDC_RX3_RX_PATH_SEC0, 0xFC, 0xF4},
	{WCD9335_CDC_RX4_RX_PATH_SEC0, 0xFC, 0xF4},
	{WCD9335_CDC_RX5_RX_PATH_SEC0, 0xFC, 0xF8},
	{WCD9335_CDC_RX6_RX_PATH_SEC0, 0xFC, 0xF8},
};

static const struct tasha_reg_mask_val tasha_codec_reg_defaults[] = {
	{WCD9335_CODEC_RPM_CLK_GATE, 0x03, 0x00},
	{WCD9335_CODEC_RPM_CLK_MCLK_CFG, 0x03, 0x01},
	{WCD9335_CODEC_RPM_CLK_MCLK_CFG, 0x04, 0x04},
};

static const struct tasha_reg_mask_val tasha_codec_reg_i2c_defaults[] = {
	{WCD9335_ANA_CLK_TOP, 0x20, 0x20},
	{WCD9335_CODEC_RPM_CLK_GATE, 0x03, 0x01},
	{WCD9335_CODEC_RPM_CLK_MCLK_CFG, 0x03, 0x00},
	{WCD9335_CODEC_RPM_CLK_MCLK_CFG, 0x05, 0x05},
	{WCD9335_DATA_HUB_DATA_HUB_RX0_INP_CFG, 0x01, 0x01},
	{WCD9335_DATA_HUB_DATA_HUB_RX1_INP_CFG, 0x01, 0x01},
	{WCD9335_DATA_HUB_DATA_HUB_RX2_INP_CFG, 0x01, 0x01},
	{WCD9335_DATA_HUB_DATA_HUB_RX3_INP_CFG, 0x01, 0x01},
	{WCD9335_DATA_HUB_DATA_HUB_TX_I2S_SD0_L_CFG, 0x05, 0x05},
	{WCD9335_DATA_HUB_DATA_HUB_TX_I2S_SD0_R_CFG, 0x05, 0x05},
	{WCD9335_DATA_HUB_DATA_HUB_TX_I2S_SD1_L_CFG, 0x05, 0x05},
	{WCD9335_DATA_HUB_DATA_HUB_TX_I2S_SD1_R_CFG, 0x05, 0x05},
};

static const struct tasha_reg_mask_val tasha_codec_reg_init_common_val[] = {
	/* Rbuckfly/R_EAR(32) */
	{WCD9335_CDC_CLSH_K2_MSB, 0x0F, 0x00},
	{WCD9335_CDC_CLSH_K2_LSB, 0xFF, 0x60},
	{WCD9335_CPE_SS_DMIC_CFG, 0x80, 0x00},
	{WCD9335_CDC_BOOST0_BOOST_CTL, 0x7C, 0x58},
	{WCD9335_CDC_BOOST1_BOOST_CTL, 0x7C, 0x58},
	{WCD9335_CDC_RX7_RX_PATH_CFG1, 0x08, 0x08},
	{WCD9335_CDC_RX8_RX_PATH_CFG1, 0x08, 0x08},
	{WCD9335_ANA_LO_1_2, 0x3C, 0X3C},
	{WCD9335_DIFF_LO_COM_SWCAP_REFBUF_FREQ, 0x70, 0x00},
	{WCD9335_SOC_MAD_AUDIO_CTL_2, 0x03, 0x03},
	{WCD9335_CDC_TOP_TOP_CFG1, 0x02, 0x02},
	{WCD9335_CDC_TOP_TOP_CFG1, 0x01, 0x01},
	{WCD9335_EAR_CMBUFF, 0x08, 0x00},
	{WCD9335_CDC_TX9_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_TX10_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_TX11_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_TX12_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_COMPANDER7_CTL3, 0x80, 0x80},
	{WCD9335_CDC_COMPANDER8_CTL3, 0x80, 0x80},
	{WCD9335_CDC_COMPANDER7_CTL7, 0x01, 0x01},
	{WCD9335_CDC_COMPANDER8_CTL7, 0x01, 0x01},
	{WCD9335_CDC_RX0_RX_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_RX1_RX_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_RX2_RX_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_RX3_RX_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_RX4_RX_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_RX5_RX_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_RX6_RX_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_RX7_RX_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_RX8_RX_PATH_CFG0, 0x01, 0x01},
	{WCD9335_CDC_RX0_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD9335_CDC_RX1_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD9335_CDC_RX2_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD9335_CDC_RX3_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD9335_CDC_RX4_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD9335_CDC_RX5_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD9335_CDC_RX6_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD9335_CDC_RX7_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD9335_CDC_RX8_RX_PATH_MIX_CFG, 0x01, 0x01},
	{WCD9335_VBADC_IBIAS_FE, 0x0C, 0x08},
};

static const struct tasha_reg_mask_val tasha_codec_reg_init_1_x_val[] = {
	/* Enable TX HPF Filter & Linear Phase */
	{WCD9335_CDC_TX0_TX_PATH_CFG0, 0x11, 0x11},
	{WCD9335_CDC_TX1_TX_PATH_CFG0, 0x11, 0x11},
	{WCD9335_CDC_TX2_TX_PATH_CFG0, 0x11, 0x11},
	{WCD9335_CDC_TX3_TX_PATH_CFG0, 0x11, 0x11},
	{WCD9335_CDC_TX4_TX_PATH_CFG0, 0x11, 0x11},
	{WCD9335_CDC_TX5_TX_PATH_CFG0, 0x11, 0x11},
	{WCD9335_CDC_TX6_TX_PATH_CFG0, 0x11, 0x11},
	{WCD9335_CDC_TX7_TX_PATH_CFG0, 0x11, 0x11},
	{WCD9335_CDC_TX8_TX_PATH_CFG0, 0x11, 0x11},
	{WCD9335_CDC_RX0_RX_PATH_SEC0, 0xF8, 0xF8},
	{WCD9335_CDC_RX0_RX_PATH_SEC1, 0x08, 0x08},
	{WCD9335_CDC_RX1_RX_PATH_SEC1, 0x08, 0x08},
	{WCD9335_CDC_RX2_RX_PATH_SEC1, 0x08, 0x08},
	{WCD9335_CDC_RX3_RX_PATH_SEC1, 0x08, 0x08},
	{WCD9335_CDC_RX4_RX_PATH_SEC1, 0x08, 0x08},
	{WCD9335_CDC_RX5_RX_PATH_SEC1, 0x08, 0x08},
	{WCD9335_CDC_RX6_RX_PATH_SEC1, 0x08, 0x08},
	{WCD9335_CDC_RX7_RX_PATH_SEC1, 0x08, 0x08},
	{WCD9335_CDC_RX8_RX_PATH_SEC1, 0x08, 0x08},
	{WCD9335_CDC_RX0_RX_PATH_MIX_SEC0, 0x08, 0x08},
	{WCD9335_CDC_RX1_RX_PATH_MIX_SEC0, 0x08, 0x08},
	{WCD9335_CDC_RX2_RX_PATH_MIX_SEC0, 0x08, 0x08},
	{WCD9335_CDC_RX3_RX_PATH_MIX_SEC0, 0x08, 0x08},
	{WCD9335_CDC_RX4_RX_PATH_MIX_SEC0, 0x08, 0x08},
	{WCD9335_CDC_RX5_RX_PATH_MIX_SEC0, 0x08, 0x08},
	{WCD9335_CDC_RX6_RX_PATH_MIX_SEC0, 0x08, 0x08},
	{WCD9335_CDC_RX7_RX_PATH_MIX_SEC0, 0x08, 0x08},
	{WCD9335_CDC_RX8_RX_PATH_MIX_SEC0, 0x08, 0x08},
	{WCD9335_CDC_TX0_TX_PATH_SEC2, 0x01, 0x01},
	{WCD9335_CDC_TX1_TX_PATH_SEC2, 0x01, 0x01},
	{WCD9335_CDC_TX2_TX_PATH_SEC2, 0x01, 0x01},
	{WCD9335_CDC_TX3_TX_PATH_SEC2, 0x01, 0x01},
	{WCD9335_CDC_TX4_TX_PATH_SEC2, 0x01, 0x01},
	{WCD9335_CDC_TX5_TX_PATH_SEC2, 0x01, 0x01},
	{WCD9335_CDC_TX6_TX_PATH_SEC2, 0x01, 0x01},
	{WCD9335_CDC_TX7_TX_PATH_SEC2, 0x01, 0x01},
	{WCD9335_CDC_TX8_TX_PATH_SEC2, 0x01, 0x01},
	{WCD9335_CDC_RX3_RX_PATH_SEC0, 0xF8, 0xF0},
	{WCD9335_CDC_RX4_RX_PATH_SEC0, 0xF8, 0xF0},
	{WCD9335_CDC_RX5_RX_PATH_SEC0, 0xF8, 0xF8},
	{WCD9335_CDC_RX6_RX_PATH_SEC0, 0xF8, 0xF8},
	{WCD9335_RX_OCP_COUNT, 0xFF, 0xFF},
	{WCD9335_HPH_OCP_CTL, 0xF0, 0x70},
	{WCD9335_CPE_SS_CPAR_CFG, 0xFF, 0x00},
	{WCD9335_FLYBACK_VNEG_CTRL_1, 0xFF, 0x63},
	{WCD9335_FLYBACK_VNEG_CTRL_4, 0xFF, 0x7F},
	{WCD9335_CLASSH_CTRL_VCL_1, 0xFF, 0x60},
	{WCD9335_CLASSH_CTRL_CCL_5, 0xFF, 0x40},
	{WCD9335_RX_TIMER_DIV, 0xFF, 0x32},
	{WCD9335_SE_LO_COM2, 0xFF, 0x01},
	{WCD9335_MBHC_ZDET_ANA_CTL, 0x0F, 0x07},
	{WCD9335_RX_BIAS_HPH_PA, 0xF0, 0x60},
	{WCD9335_HPH_RDAC_LDO_CTL, 0x88, 0x88},
	{WCD9335_HPH_L_EN, 0x20, 0x20},
	{WCD9335_HPH_R_EN, 0x20, 0x20},
	{WCD9335_DIFF_LO_CORE_OUT_PROG, 0xFC, 0xD8},
	{WCD9335_CDC_RX5_RX_PATH_SEC3, 0xBD, 0xBD},
	{WCD9335_CDC_RX6_RX_PATH_SEC3, 0xBD, 0xBD},
	{WCD9335_DIFF_LO_COM_PA_FREQ, 0x70, 0x40},
};

static void tasha_update_reg_reset_values(struct snd_soc_codec *codec)
{
	u32 i;
	struct wcd9xxx *tasha_core = dev_get_drvdata(codec->dev->parent);

	if (TASHA_IS_1_1(tasha_core)) {
		for (i = 0; i < ARRAY_SIZE(tasha_reg_update_reset_val_1_1);
		     i++)
			snd_soc_write(codec,
				      tasha_reg_update_reset_val_1_1[i].reg,
				      tasha_reg_update_reset_val_1_1[i].val);
	}
}

static void tasha_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);

	for (i = 0; i < ARRAY_SIZE(tasha_codec_reg_init_common_val); i++)
		snd_soc_update_bits(codec,
				tasha_codec_reg_init_common_val[i].reg,
				tasha_codec_reg_init_common_val[i].mask,
				tasha_codec_reg_init_common_val[i].val);

	if (TASHA_IS_1_1(wcd9xxx) ||
	    TASHA_IS_1_0(wcd9xxx))
		for (i = 0; i < ARRAY_SIZE(tasha_codec_reg_init_1_x_val); i++)
			snd_soc_update_bits(codec,
					tasha_codec_reg_init_1_x_val[i].reg,
					tasha_codec_reg_init_1_x_val[i].mask,
					tasha_codec_reg_init_1_x_val[i].val);

	if (TASHA_IS_1_1(wcd9xxx)) {
		for (i = 0; i < ARRAY_SIZE(tasha_codec_reg_init_val_1_1); i++)
			snd_soc_update_bits(codec,
					tasha_codec_reg_init_val_1_1[i].reg,
					tasha_codec_reg_init_val_1_1[i].mask,
					tasha_codec_reg_init_val_1_1[i].val);
	} else if (TASHA_IS_1_0(wcd9xxx)) {
		for (i = 0; i < ARRAY_SIZE(tasha_codec_reg_init_val_1_0); i++)
			snd_soc_update_bits(codec,
					tasha_codec_reg_init_val_1_0[i].reg,
					tasha_codec_reg_init_val_1_0[i].mask,
					tasha_codec_reg_init_val_1_0[i].val);
	} else if (TASHA_IS_2_0(wcd9xxx)) {
		for (i = 0; i < ARRAY_SIZE(tasha_codec_reg_init_val_2_0); i++)
			snd_soc_update_bits(codec,
					tasha_codec_reg_init_val_2_0[i].reg,
					tasha_codec_reg_init_val_2_0[i].mask,
					tasha_codec_reg_init_val_2_0[i].val);
	}
}

static void tasha_update_reg_defaults(struct tasha_priv *tasha)
{
	u32 i;
	struct wcd9xxx *wcd9xxx;

	wcd9xxx = tasha->wcd9xxx;
	for (i = 0; i < ARRAY_SIZE(tasha_codec_reg_defaults); i++)
		regmap_update_bits(wcd9xxx->regmap,
				   tasha_codec_reg_defaults[i].reg,
				   tasha_codec_reg_defaults[i].mask,
				   tasha_codec_reg_defaults[i].val);

	tasha->intf_type = wcd9xxx_get_intf_type();
	if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_I2C)
		for (i = 0; i < ARRAY_SIZE(tasha_codec_reg_i2c_defaults); i++)
			regmap_update_bits(wcd9xxx->regmap,
					   tasha_codec_reg_i2c_defaults[i].reg,
					   tasha_codec_reg_i2c_defaults[i].mask,
					   tasha_codec_reg_i2c_defaults[i].val);

}

static void tasha_slim_interface_init_reg(struct snd_soc_codec *codec)
{
	int i;
	struct tasha_priv *priv = snd_soc_codec_get_drvdata(codec);

	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++)
		wcd9xxx_interface_reg_write(priv->wcd9xxx,
					    TASHA_SLIM_PGD_PORT_INT_EN0 + i,
					    0xFF);
}

static irqreturn_t tasha_slimbus_irq(int irq, void *data)
{
	struct tasha_priv *priv = data;
	unsigned long status = 0;
	int i, j, port_id, k;
	u32 bit;
	u8 val, int_val = 0;
	bool tx, cleared;
	unsigned short reg = 0;

	for (i = TASHA_SLIM_PGD_PORT_INT_STATUS_RX_0, j = 0;
	     i <= TASHA_SLIM_PGD_PORT_INT_STATUS_TX_1; i++, j++) {
		val = wcd9xxx_interface_reg_read(priv->wcd9xxx, i);
		status |= ((u32)val << (8 * j));
	}

	for_each_set_bit(j, &status, 32) {
		tx = (j >= 16 ? true : false);
		port_id = (tx ? j - 16 : j);
		val = wcd9xxx_interface_reg_read(priv->wcd9xxx,
				TASHA_SLIM_PGD_PORT_INT_RX_SOURCE0 + j);
		if (val) {
			if (!tx)
				reg = TASHA_SLIM_PGD_PORT_INT_EN0 +
					(port_id / 8);
			else
				reg = TASHA_SLIM_PGD_PORT_INT_TX_EN0 +
					(port_id / 8);
			int_val = wcd9xxx_interface_reg_read(
				priv->wcd9xxx, reg);
			/*
			 * Ignore interrupts for ports for which the
			 * interrupts are not specifically enabled.
			 */
			if (!(int_val & (1 << (port_id % 8))))
				continue;
		}
		if (val & TASHA_SLIM_IRQ_OVERFLOW)
			pr_err_ratelimited(
			   "%s: overflow error on %s port %d, value %x\n",
			   __func__, (tx ? "TX" : "RX"), port_id, val);
		if (val & TASHA_SLIM_IRQ_UNDERFLOW)
			pr_err_ratelimited(
			   "%s: underflow error on %s port %d, value %x\n",
			   __func__, (tx ? "TX" : "RX"), port_id, val);
		if ((val & TASHA_SLIM_IRQ_OVERFLOW) ||
			(val & TASHA_SLIM_IRQ_UNDERFLOW)) {
			if (!tx)
				reg = TASHA_SLIM_PGD_PORT_INT_EN0 +
					(port_id / 8);
			else
				reg = TASHA_SLIM_PGD_PORT_INT_TX_EN0 +
					(port_id / 8);
			int_val = wcd9xxx_interface_reg_read(
				priv->wcd9xxx, reg);
			if (int_val & (1 << (port_id % 8))) {
				int_val = int_val ^ (1 << (port_id % 8));
				wcd9xxx_interface_reg_write(priv->wcd9xxx,
					reg, int_val);
			}
		}
		if (val & TASHA_SLIM_IRQ_PORT_CLOSED) {
			/*
			 * INT SOURCE register starts from RX to TX
			 * but port number in the ch_mask is in opposite way
			 */
			bit = (tx ? j - 16 : j + 16);
			pr_debug("%s: %s port %d closed value %x, bit %u\n",
				 __func__, (tx ? "TX" : "RX"), port_id, val,
				 bit);
			for (k = 0, cleared = false; k < NUM_CODEC_DAIS; k++) {
				pr_debug("%s: priv->dai[%d].ch_mask = 0x%lx\n",
					 __func__, k, priv->dai[k].ch_mask);
				if (test_and_clear_bit(bit,
						       &priv->dai[k].ch_mask)) {
					cleared = true;
					if (!priv->dai[k].ch_mask)
						wake_up(&priv->dai[k].dai_wait);
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
		wcd9xxx_interface_reg_write(priv->wcd9xxx,
					    TASHA_SLIM_PGD_PORT_INT_CLR_RX_0 +
					    (j / 8),
					    1 << (j % 8));
	}

	return IRQ_HANDLED;
}

static int tasha_setup_irqs(struct tasha_priv *tasha)
{
	int ret = 0;
	struct snd_soc_codec *codec = tasha->codec;
	struct wcd9xxx *wcd9xxx = tasha->wcd9xxx;
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_SLIMBUS,
				  tasha_slimbus_irq, "SLIMBUS Slave", tasha);
	if (ret)
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_SLIMBUS);
	else
		tasha_slim_interface_init_reg(codec);

	return ret;
}

static void tasha_init_slim_slave_cfg(struct snd_soc_codec *codec)
{
	struct tasha_priv *priv = snd_soc_codec_get_drvdata(codec);
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

static void tasha_cleanup_irqs(struct tasha_priv *tasha)
{
	struct wcd9xxx *wcd9xxx = tasha->wcd9xxx;
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;

	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_SLIMBUS, tasha);
}

static int tasha_handle_pdata(struct tasha_priv *tasha,
			      struct wcd9xxx_pdata *pdata)
{
	struct snd_soc_codec *codec = tasha->codec;
	u8 dmic_ctl_val, mad_dmic_ctl_val;
	u8 anc_ctl_value;
	u32 def_dmic_rate, dmic_clk_drv;
	int vout_ctl_1, vout_ctl_2, vout_ctl_3, vout_ctl_4;
	int rc = 0;

	if (!pdata) {
		dev_err(codec->dev, "%s: NULL pdata\n", __func__);
		return -ENODEV;
	}

	/* set micbias voltage */
	vout_ctl_1 = wcd9335_get_micb_vout_ctl_val(pdata->micbias.micb1_mv);
	vout_ctl_2 = wcd9335_get_micb_vout_ctl_val(pdata->micbias.micb2_mv);
	vout_ctl_3 = wcd9335_get_micb_vout_ctl_val(pdata->micbias.micb3_mv);
	vout_ctl_4 = wcd9335_get_micb_vout_ctl_val(pdata->micbias.micb4_mv);
	if (vout_ctl_1 < 0 || vout_ctl_2 < 0 ||
	    vout_ctl_3 < 0 || vout_ctl_4 < 0) {
		rc = -EINVAL;
		goto done;
	}
	snd_soc_update_bits(codec, WCD9335_ANA_MICB1, 0x3F, vout_ctl_1);
	snd_soc_update_bits(codec, WCD9335_ANA_MICB2, 0x3F, vout_ctl_2);
	snd_soc_update_bits(codec, WCD9335_ANA_MICB3, 0x3F, vout_ctl_3);
	snd_soc_update_bits(codec, WCD9335_ANA_MICB4, 0x3F, vout_ctl_4);

	/* Set the DMIC sample rate */
	switch (pdata->mclk_rate) {
	case TASHA_MCLK_CLK_9P6MHZ:
		def_dmic_rate = WCD9XXX_DMIC_SAMPLE_RATE_4P8MHZ;
		break;
	case TASHA_MCLK_CLK_12P288MHZ:
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
	if (pdata->ecpp_dmic_sample_rate ==
	    WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED) {
		dev_info(codec->dev,
			 "%s: ecpp_dmic_rate invalid default = %d\n",
			 __func__, def_dmic_rate);
		/*
		 * use dmic_sample_rate as the default for ECPP DMIC
		 * if ecpp dmic sample rate is undefined
		 */
		pdata->ecpp_dmic_sample_rate = pdata->dmic_sample_rate;
	}

	if (pdata->dmic_clk_drv ==
	    WCD9XXX_DMIC_CLK_DRIVE_UNDEFINED) {
		pdata->dmic_clk_drv = WCD9335_DMIC_CLK_DRIVE_DEFAULT;
		dev_info(codec->dev,
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

	snd_soc_update_bits(codec, WCD9335_TEST_DEBUG_PAD_DRVCTL,
			    0x0C, dmic_clk_drv << 2);

	/*
	 * Default the DMIC clk rates to mad_dmic_sample_rate,
	 * whereas, the anc/txfe dmic rates to dmic_sample_rate
	 * since the anc/txfe are independent of mad block.
	 */
	mad_dmic_ctl_val = tasha_get_dmic_clk_val(tasha->codec,
				pdata->mclk_rate,
				pdata->mad_dmic_sample_rate);
	snd_soc_update_bits(codec, WCD9335_CPE_SS_DMIC0_CTL,
		0x0E, mad_dmic_ctl_val << 1);
	snd_soc_update_bits(codec, WCD9335_CPE_SS_DMIC1_CTL,
		0x0E, mad_dmic_ctl_val << 1);
	snd_soc_update_bits(codec, WCD9335_CPE_SS_DMIC2_CTL,
		0x0E, mad_dmic_ctl_val << 1);

	dmic_ctl_val = tasha_get_dmic_clk_val(tasha->codec,
				pdata->mclk_rate,
				pdata->dmic_sample_rate);

	if (dmic_ctl_val == WCD9335_DMIC_CLK_DIV_2)
		anc_ctl_value = WCD9335_ANC_DMIC_X2_FULL_RATE;
	else
		anc_ctl_value = WCD9335_ANC_DMIC_X2_HALF_RATE;

	snd_soc_update_bits(codec, WCD9335_CDC_ANC0_MODE_2_CTL,
			    0x40, anc_ctl_value << 6);
	snd_soc_update_bits(codec, WCD9335_CDC_ANC0_MODE_2_CTL,
			    0x20, anc_ctl_value << 5);
	snd_soc_update_bits(codec, WCD9335_CDC_ANC1_MODE_2_CTL,
			    0x40, anc_ctl_value << 6);
	snd_soc_update_bits(codec, WCD9335_CDC_ANC1_MODE_2_CTL,
			    0x20, anc_ctl_value << 5);
done:
	return rc;
}

static struct wcd_cpe_core *tasha_codec_get_cpe_core(
		struct snd_soc_codec *codec)
{
	struct tasha_priv *priv = snd_soc_codec_get_drvdata(codec);

	return priv->cpe_core;
}

static int tasha_codec_cpe_fll_update_divider(
	struct snd_soc_codec *codec, u32 cpe_fll_rate)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	u32 div_val = 0, l_val = 0;
	u32 computed_cpe_fll;

	if (cpe_fll_rate != CPE_FLL_CLK_75MHZ &&
	    cpe_fll_rate != CPE_FLL_CLK_150MHZ) {
		dev_err(codec->dev,
			"%s: Invalid CPE fll rate request %u\n",
			__func__, cpe_fll_rate);
		return -EINVAL;
	}

	if (wcd9xxx->mclk_rate == TASHA_MCLK_CLK_12P288MHZ) {
		/* update divider to 10 and enable 5x divider */
		snd_soc_write(codec, WCD9335_CPE_FLL_USER_CTL_1,
			      0x55);
		div_val = 10;
	} else if (wcd9xxx->mclk_rate == TASHA_MCLK_CLK_9P6MHZ) {
		/* update divider to 8 and enable 2x divider */
		snd_soc_update_bits(codec, WCD9335_CPE_FLL_USER_CTL_0,
				    0x7C, 0x70);
		snd_soc_update_bits(codec, WCD9335_CPE_FLL_USER_CTL_1,
				    0xE0, 0x20);
		div_val = 8;
	} else {
		dev_err(codec->dev,
			"%s: Invalid MCLK rate %u\n",
			__func__, wcd9xxx->mclk_rate);
		return -EINVAL;
	}

	l_val = ((cpe_fll_rate / 1000) * div_val) /
		 (wcd9xxx->mclk_rate / 1000);

	/* If l_val was integer truncated, increment l_val once */
	computed_cpe_fll = (wcd9xxx->mclk_rate / div_val) * l_val;
	if (computed_cpe_fll < cpe_fll_rate)
		l_val++;


	/* update L value LSB and MSB */
	snd_soc_write(codec, WCD9335_CPE_FLL_L_VAL_CTL_0,
		      (l_val & 0xFF));
	snd_soc_write(codec, WCD9335_CPE_FLL_L_VAL_CTL_1,
		      ((l_val >> 8) & 0xFF));

	tasha->current_cpe_clk_freq = cpe_fll_rate;
	dev_dbg(codec->dev,
		"%s: updated l_val to %u for cpe_clk %u and mclk %u\n",
		__func__, l_val, cpe_fll_rate, wcd9xxx->mclk_rate);

	return 0;
}

static int __tasha_cdc_change_cpe_clk(struct snd_soc_codec *codec,
		u32 clk_freq)
{
	int ret = 0;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	if (!tasha_cdc_is_svs_enabled(tasha)) {
		dev_dbg(codec->dev,
			"%s: SVS not enabled or tasha is not 2p0, return\n",
			__func__);
		return 0;
	}
	dev_dbg(codec->dev, "%s: clk_freq = %u\n", __func__, clk_freq);

	if (clk_freq == CPE_FLL_CLK_75MHZ) {
		/* Change to SVS */
		snd_soc_update_bits(codec, WCD9335_CPE_FLL_FLL_MODE,
				    0x08, 0x08);
		if (tasha_codec_cpe_fll_update_divider(codec, clk_freq)) {
			ret = -EINVAL;
			goto done;
		}

		snd_soc_update_bits(codec, WCD9335_CPE_FLL_FLL_MODE,
				    0x10, 0x10);

		clear_bit(CPE_NOMINAL, &tasha->status_mask);
		tasha_codec_update_sido_voltage(tasha, sido_buck_svs_voltage);

	} else if (clk_freq == CPE_FLL_CLK_150MHZ) {
		/* change to nominal */
		snd_soc_update_bits(codec, WCD9335_CPE_FLL_FLL_MODE,
				    0x08, 0x08);

		set_bit(CPE_NOMINAL, &tasha->status_mask);
		tasha_codec_update_sido_voltage(tasha, SIDO_VOLTAGE_NOMINAL_MV);

		if (tasha_codec_cpe_fll_update_divider(codec, clk_freq)) {
			ret = -EINVAL;
			goto done;
		}
		snd_soc_update_bits(codec, WCD9335_CPE_FLL_FLL_MODE,
				    0x10, 0x10);
	} else {
		dev_err(codec->dev,
			"%s: Invalid clk_freq request %d for CPE FLL\n",
			__func__, clk_freq);
		ret = -EINVAL;
	}

done:
	snd_soc_update_bits(codec, WCD9335_CPE_FLL_FLL_MODE,
			    0x10, 0x00);
	snd_soc_update_bits(codec, WCD9335_CPE_FLL_FLL_MODE,
			    0x08, 0x00);
	return ret;
}


static int tasha_codec_cpe_fll_enable(struct snd_soc_codec *codec,
				   bool enable)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u8 clk_sel_reg_val = 0x00;

	dev_dbg(codec->dev, "%s: enable = %s\n",
			__func__, enable ? "true" : "false");

	if (enable) {
		if (tasha_cdc_is_svs_enabled(tasha)) {
			/* FLL enable is always at SVS */
			if (__tasha_cdc_change_cpe_clk(codec,
					CPE_FLL_CLK_75MHZ)) {
				dev_err(codec->dev,
					"%s: clk change to %d failed\n",
					__func__, CPE_FLL_CLK_75MHZ);
				return -EINVAL;
			}
		} else {
			if (tasha_codec_cpe_fll_update_divider(codec,
							CPE_FLL_CLK_75MHZ)) {
				dev_err(codec->dev,
					"%s: clk change to %d failed\n",
					__func__, CPE_FLL_CLK_75MHZ);
				return -EINVAL;
			}
		}

		if (TASHA_IS_1_0(wcd9xxx)) {
			tasha_cdc_mclk_enable(codec, true, false);
			clk_sel_reg_val = 0x02;
		}

		/* Setup CPE reference clk */
		snd_soc_update_bits(codec, WCD9335_ANA_CLK_TOP,
				    0x02, clk_sel_reg_val);

		/* enable CPE FLL reference clk */
		snd_soc_update_bits(codec, WCD9335_ANA_CLK_TOP,
				    0x01, 0x01);

		/* program the PLL */
		snd_soc_update_bits(codec, WCD9335_CPE_FLL_USER_CTL_0,
				    0x01, 0x01);

		/* TEST clk setting */
		snd_soc_update_bits(codec, WCD9335_CPE_FLL_TEST_CTL_0,
				    0x80, 0x80);
		/* set FLL mode to HW controlled */
		snd_soc_update_bits(codec, WCD9335_CPE_FLL_FLL_MODE,
				    0x60, 0x00);
		snd_soc_write(codec, WCD9335_CPE_FLL_FLL_MODE, 0x80);
	} else {
		/* disable CPE FLL reference clk */
		snd_soc_update_bits(codec, WCD9335_ANA_CLK_TOP,
				    0x01, 0x00);
		/* undo TEST clk setting */
		snd_soc_update_bits(codec, WCD9335_CPE_FLL_TEST_CTL_0,
				    0x80, 0x00);
		/* undo FLL mode to HW control */
		snd_soc_write(codec, WCD9335_CPE_FLL_FLL_MODE, 0x00);
		snd_soc_update_bits(codec, WCD9335_CPE_FLL_FLL_MODE,
				    0x60, 0x20);
		/* undo the PLL */
		snd_soc_update_bits(codec, WCD9335_CPE_FLL_USER_CTL_0,
				    0x01, 0x00);

		if (TASHA_IS_1_0(wcd9xxx))
			tasha_cdc_mclk_enable(codec, false, false);

		/*
		 * FLL could get disabled while at nominal,
		 * scale it back to SVS
		 */
		if (tasha_cdc_is_svs_enabled(tasha))
			__tasha_cdc_change_cpe_clk(codec,
						CPE_FLL_CLK_75MHZ);
	}

	return 0;

}

static void tasha_cdc_query_cpe_clk_plan(void *data,
		struct cpe_svc_cfg_clk_plan *clk_freq)
{
	struct snd_soc_codec *codec = data;
	struct tasha_priv *tasha;
	u32 cpe_clk_khz;

	if (!codec) {
		pr_err("%s: Invalid codec handle\n",
			__func__);
		return;
	}

	tasha = snd_soc_codec_get_drvdata(codec);
	cpe_clk_khz = tasha->current_cpe_clk_freq / 1000;

	dev_dbg(codec->dev,
		"%s: current_clk_freq = %u\n",
		__func__, tasha->current_cpe_clk_freq);

	clk_freq->current_clk_feq = cpe_clk_khz;
	clk_freq->num_clk_freqs = 2;

	if (tasha_cdc_is_svs_enabled(tasha)) {
		clk_freq->clk_freqs[0] = CPE_FLL_CLK_75MHZ / 1000;
		clk_freq->clk_freqs[1] = CPE_FLL_CLK_150MHZ / 1000;
	} else {
		clk_freq->clk_freqs[0] = CPE_FLL_CLK_75MHZ;
		clk_freq->clk_freqs[1] = CPE_FLL_CLK_150MHZ;
	}
}

static void tasha_cdc_change_cpe_clk(void *data,
		u32 clk_freq)
{
	struct snd_soc_codec *codec = data;
	struct tasha_priv *tasha;
	u32 cpe_clk_khz, req_freq = 0;

	if (!codec) {
		pr_err("%s: Invalid codec handle\n",
			__func__);
		return;
	}

	tasha = snd_soc_codec_get_drvdata(codec);
	cpe_clk_khz = tasha->current_cpe_clk_freq / 1000;

	if (tasha_cdc_is_svs_enabled(tasha)) {
		if ((clk_freq * 1000) <= CPE_FLL_CLK_75MHZ)
			req_freq = CPE_FLL_CLK_75MHZ;
		else
			req_freq = CPE_FLL_CLK_150MHZ;
	}

	dev_dbg(codec->dev,
		"%s: requested clk_freq = %u, current clk_freq = %u\n",
		__func__, clk_freq * 1000,
		tasha->current_cpe_clk_freq);

	if (tasha_cdc_is_svs_enabled(tasha)) {
		if (__tasha_cdc_change_cpe_clk(codec, req_freq))
			dev_err(codec->dev,
				"%s: clock/voltage scaling failed\n",
				__func__);
	}
}

static int tasha_codec_slim_reserve_bw(struct snd_soc_codec *codec,
		u32 bw_ops, bool commit)
{
	struct wcd9xxx *wcd9xxx;

	if (!codec) {
		pr_err("%s: Invalid handle to codec\n",
			__func__);
		return -EINVAL;
	}

	wcd9xxx = dev_get_drvdata(codec->dev->parent);

	if (!wcd9xxx) {
		dev_err(codec->dev, "%s: Invalid parent drv_data\n",
			__func__);
		return -EINVAL;
	}

	return wcd9xxx_slim_reserve_bw(wcd9xxx, bw_ops, commit);
}

static int tasha_codec_vote_max_bw(struct snd_soc_codec *codec,
			bool vote)
{
	u32 bw_ops;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);

	if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_I2C)
		return 0;

	mutex_lock(&tasha->sb_clk_gear_lock);
	if (vote) {
		tasha->ref_count++;
		if (tasha->ref_count == 1) {
			bw_ops = SLIM_BW_CLK_GEAR_9;
			tasha_codec_slim_reserve_bw(codec,
				bw_ops, true);
		}
	} else if (!vote && tasha->ref_count > 0) {
		tasha->ref_count--;
		if (tasha->ref_count == 0) {
			bw_ops = SLIM_BW_UNVOTE;
			tasha_codec_slim_reserve_bw(codec,
				bw_ops, true);
		}
	};

	dev_dbg(codec->dev, "%s Value of counter after vote or un-vote is %d\n",
		__func__, tasha->ref_count);

	mutex_unlock(&tasha->sb_clk_gear_lock);

	return 0;
}

static int tasha_cpe_err_irq_control(struct snd_soc_codec *codec,
	enum cpe_err_irq_cntl_type cntl_type, u8 *status)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	u8 irq_bits;

	if (TASHA_IS_2_0(tasha->wcd9xxx))
		irq_bits = 0xFF;
	else
		irq_bits = 0x3F;

	if (status)
		irq_bits = (*status) & irq_bits;

	switch (cntl_type) {
	case CPE_ERR_IRQ_MASK:
		snd_soc_update_bits(codec,
				    WCD9335_CPE_SS_SS_ERROR_INT_MASK,
				    irq_bits, irq_bits);
		break;
	case CPE_ERR_IRQ_UNMASK:
		snd_soc_update_bits(codec,
				    WCD9335_CPE_SS_SS_ERROR_INT_MASK,
				    irq_bits, 0x00);
		break;
	case CPE_ERR_IRQ_CLEAR:
		snd_soc_write(codec, WCD9335_CPE_SS_SS_ERROR_INT_CLEAR,
			      irq_bits);
		break;
	case CPE_ERR_IRQ_STATUS:
		if (!status)
			return -EINVAL;
		*status = snd_soc_read(codec,
				       WCD9335_CPE_SS_SS_ERROR_INT_STATUS);
		break;
	}

	return 0;
}

static const struct wcd_cpe_cdc_cb cpe_cb = {
	.cdc_clk_en = tasha_codec_internal_rco_ctrl,
	.cpe_clk_en = tasha_codec_cpe_fll_enable,
	.get_afe_out_port_id = tasha_codec_get_mad_port_id,
	.lab_cdc_ch_ctl = tasha_codec_enable_slimtx_mad,
	.cdc_ext_clk = tasha_cdc_mclk_enable,
	.bus_vote_bw = tasha_codec_vote_max_bw,
	.cpe_err_irq_control = tasha_cpe_err_irq_control,
};

static struct cpe_svc_init_param cpe_svc_params = {
	.version = CPE_SVC_INIT_PARAM_V1,
	.query_freq_plans_cb = tasha_cdc_query_cpe_clk_plan,
	.change_freq_plan_cb = tasha_cdc_change_cpe_clk,
};

static int tasha_cpe_initialize(struct snd_soc_codec *codec)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct wcd_cpe_params cpe_params;

	memset(&cpe_params, 0,
	       sizeof(struct wcd_cpe_params));
	cpe_params.codec = codec;
	cpe_params.get_cpe_core = tasha_codec_get_cpe_core;
	cpe_params.cdc_cb = &cpe_cb;
	cpe_params.dbg_mode = cpe_debug_mode;
	cpe_params.cdc_major_ver = CPE_SVC_CODEC_WCD9335;
	cpe_params.cdc_minor_ver = CPE_SVC_CODEC_V1P0;
	cpe_params.cdc_id = CPE_SVC_CODEC_WCD9335;

	cpe_params.cdc_irq_info.cpe_engine_irq =
			WCD9335_IRQ_SVA_OUTBOX1;
	cpe_params.cdc_irq_info.cpe_err_irq =
			WCD9335_IRQ_SVA_ERROR;
	cpe_params.cdc_irq_info.cpe_fatal_irqs =
			TASHA_CPE_FATAL_IRQS;

	cpe_svc_params.context = codec;
	cpe_params.cpe_svc_params = &cpe_svc_params;

	tasha->cpe_core = wcd_cpe_init("cpe_9335", codec,
					&cpe_params);
	if (IS_ERR_OR_NULL(tasha->cpe_core)) {
		dev_err(codec->dev,
			"%s: Failed to enable CPE\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

static const struct wcd_resmgr_cb tasha_resmgr_cb = {
	.cdc_rco_ctrl = __tasha_codec_internal_rco_ctrl,
};

static int tasha_device_down(struct wcd9xxx *wcd9xxx)
{
	struct snd_soc_codec *codec;
	struct tasha_priv *priv;
	int count;
	int i = 0;

	codec = (struct snd_soc_codec *)(wcd9xxx->ssr_priv);
	priv = snd_soc_codec_get_drvdata(codec);
	snd_event_notify(priv->dev->parent, SND_EVENT_DOWN);
	wcd_cpe_ssr_event(priv->cpe_core, WCD_CPE_BUS_DOWN_EVENT);

	if (!priv->swr_ctrl_data)
		return -EINVAL;

	for (i = 0; i < priv->nr; i++) {
		if (is_snd_event_fwk_enabled())
			swrm_wcd_notify(
				priv->swr_ctrl_data[i].swr_pdev,
				SWR_DEVICE_SSR_DOWN, NULL);
		swrm_wcd_notify(priv->swr_ctrl_data[i].swr_pdev,
				SWR_DEVICE_DOWN, NULL);
	}

	if (!is_snd_event_fwk_enabled())
		snd_soc_card_change_online_state(codec->component.card, 0);
	for (count = 0; count < NUM_CODEC_DAIS; count++)
		priv->dai[count].bus_down_in_recovery = true;

	priv->resmgr->sido_input_src = SIDO_SOURCE_INTERNAL;

	return 0;
}

static int tasha_post_reset_cb(struct wcd9xxx *wcd9xxx)
{
	int i, ret = 0;
	struct wcd9xxx *control;
	struct snd_soc_codec *codec;
	struct tasha_priv *tasha;
	struct wcd9xxx_pdata *pdata;

	codec = (struct snd_soc_codec *)(wcd9xxx->ssr_priv);
	tasha = snd_soc_codec_get_drvdata(codec);
	control = dev_get_drvdata(codec->dev->parent);

	wcd9xxx_set_power_state(tasha->wcd9xxx,
				WCD_REGION_POWER_COLLAPSE_REMOVE,
				WCD9XXX_DIG_CORE_REGION_1);

	mutex_lock(&tasha->codec_mutex);

	tasha_slimbus_slave_port_cfg.slave_dev_intfdev_la =
		control->slim_slave->laddr;
	tasha_slimbus_slave_port_cfg.slave_dev_pgd_la =
		control->slim->laddr;
	tasha_init_slim_slave_cfg(codec);
	if (tasha->machine_codec_event_cb)
		tasha->machine_codec_event_cb(codec,
				WCD9335_CODEC_EVENT_CODEC_UP);
	if (!is_snd_event_fwk_enabled())
		snd_soc_card_change_online_state(codec->component.card, 1);

	/* Class-H Init*/
	wcd_clsh_init(&tasha->clsh_d);

	for (i = 0; i < TASHA_MAX_MICBIAS; i++)
		tasha->micb_ref[i] = 0;

	tasha_update_reg_defaults(tasha);

	tasha->codec = codec;

	dev_dbg(codec->dev, "%s: MCLK Rate = %x\n",
		__func__, control->mclk_rate);

	if (control->mclk_rate == TASHA_MCLK_CLK_12P288MHZ)
		snd_soc_update_bits(codec, WCD9335_CODEC_RPM_CLK_MCLK_CFG,
				    0x03, 0x00);
	else if (control->mclk_rate == TASHA_MCLK_CLK_9P6MHZ)
		snd_soc_update_bits(codec, WCD9335_CODEC_RPM_CLK_MCLK_CFG,
				    0x03, 0x01);
	tasha_codec_init_reg(codec);

	wcd_resmgr_post_ssr_v2(tasha->resmgr);

	tasha_enable_efuse_sensing(codec);

	regcache_mark_dirty(codec->component.regmap);
	regcache_sync(codec->component.regmap);

	pdata = dev_get_platdata(codec->dev->parent);
	ret = tasha_handle_pdata(tasha, pdata);
	if (ret < 0)
		dev_err(codec->dev, "%s: invalid pdata\n", __func__);

	/* Reset reference counter for voting for max bw */
	tasha->ref_count = 0;
	/* MBHC Init */
	wcd_mbhc_deinit(&tasha->mbhc);
	tasha->mbhc_started = false;

	/* Initialize MBHC module */
	ret = wcd_mbhc_init(&tasha->mbhc, codec, &mbhc_cb, &intr_ids,
		      wcd_mbhc_registers, TASHA_ZDET_SUPPORTED);
	if (ret)
		dev_err(codec->dev, "%s: mbhc initialization failed\n",
			__func__);
	else
		tasha_mbhc_hs_detect(codec, tasha->mbhc.mbhc_cfg);

	tasha_cleanup_irqs(tasha);
	ret = tasha_setup_irqs(tasha);
	if (ret) {
		dev_err(codec->dev, "%s: tasha irq setup failed %d\n",
			__func__, ret);
		goto err;
	}

	if (!tasha->swr_ctrl_data) {
		ret = -EINVAL;
		goto err;
	}

	if (is_snd_event_fwk_enabled()) {
		for (i = 0; i < tasha->nr; i++)
			swrm_wcd_notify(
				tasha->swr_ctrl_data[i].swr_pdev,
				SWR_DEVICE_SSR_UP, NULL);
	}

	tasha_set_spkr_mode(codec, tasha->spkr_mode);
	wcd_cpe_ssr_event(tasha->cpe_core, WCD_CPE_BUS_UP_EVENT);
	snd_event_notify(tasha->dev->parent, SND_EVENT_UP);
err:
	mutex_unlock(&tasha->codec_mutex);
	return ret;
}

static struct regulator *tasha_codec_find_ondemand_regulator(
		struct snd_soc_codec *codec, const char *name)
{
	int i;
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *wcd9xxx = tasha->wcd9xxx;
	struct wcd9xxx_pdata *pdata = dev_get_platdata(codec->dev->parent);

	for (i = 0; i < wcd9xxx->num_of_supplies; ++i) {
		if (pdata->regulator[i].ondemand &&
			wcd9xxx->supplies[i].supply &&
			!strcmp(wcd9xxx->supplies[i].supply, name))
			return wcd9xxx->supplies[i].consumer;
	}

	dev_dbg(tasha->dev, "Warning: regulator not found:%s\n",
		name);
	return NULL;
}

static void tasha_ssr_disable(struct device *dev, void *data)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(dev);
	struct tasha_priv *tasha;
	struct snd_soc_codec *codec;
	int count = 0;

	if (!wcd9xxx) {
		dev_dbg(dev, "%s: wcd9xxx pointer NULL.\n", __func__);
		return;
	}
	codec = (struct snd_soc_codec *)(wcd9xxx->ssr_priv);
	tasha = snd_soc_codec_get_drvdata(codec);

	for (count = 0; count < NUM_CODEC_DAIS; count++)
		tasha->dai[count].bus_down_in_recovery = true;
}

static const struct snd_event_ops tasha_ssr_ops = {
	.disable = tasha_ssr_disable,
};

static int tasha_codec_probe(struct snd_soc_codec *codec)
{
	struct wcd9xxx *control;
	struct tasha_priv *tasha;
	struct wcd9xxx_pdata *pdata;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	int i, ret;
	void *ptr = NULL;
	struct regulator *supply;

	control = dev_get_drvdata(codec->dev->parent);

	dev_info(codec->dev, "%s()\n", __func__);
	tasha = snd_soc_codec_get_drvdata(codec);
	tasha->intf_type = wcd9xxx_get_intf_type();

	if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		control->dev_down = tasha_device_down;
		control->post_reset = tasha_post_reset_cb;
		control->ssr_priv = (void *)codec;
	}

	/* Resource Manager post Init */
	ret = wcd_resmgr_post_init(tasha->resmgr, &tasha_resmgr_cb, codec);
	if (ret) {
		dev_err(codec->dev, "%s: wcd resmgr post init failed\n",
			__func__);
		goto err;
	}
	/* Class-H Init*/
	wcd_clsh_init(&tasha->clsh_d);
	/* Default HPH Mode to Class-H HiFi */
	tasha->hph_mode = CLS_H_HIFI;

	tasha->codec = codec;
	for (i = 0; i < COMPANDER_MAX; i++)
		tasha->comp_enabled[i] = 0;

	tasha->spkr_gain_offset = RX_GAIN_OFFSET_0_DB;
	tasha->intf_type = wcd9xxx_get_intf_type();
	tasha_update_reg_reset_values(codec);
	pr_debug("%s: MCLK Rate = %x\n", __func__, control->mclk_rate);
	if (control->mclk_rate == TASHA_MCLK_CLK_12P288MHZ)
		snd_soc_update_bits(codec, WCD9335_CODEC_RPM_CLK_MCLK_CFG,
				    0x03, 0x00);
	else if (control->mclk_rate == TASHA_MCLK_CLK_9P6MHZ)
		snd_soc_update_bits(codec, WCD9335_CODEC_RPM_CLK_MCLK_CFG,
				    0x03, 0x01);
	tasha_codec_init_reg(codec);

	tasha_enable_efuse_sensing(codec);

	pdata = dev_get_platdata(codec->dev->parent);
	ret = tasha_handle_pdata(tasha, pdata);
	if (ret < 0) {
		pr_err("%s: bad pdata\n", __func__);
		goto err;
	}

	supply = tasha_codec_find_ondemand_regulator(codec,
		on_demand_supply_name[ON_DEMAND_MICBIAS]);
	if (supply) {
		tasha->on_demand_list[ON_DEMAND_MICBIAS].supply = supply;
		tasha->on_demand_list[ON_DEMAND_MICBIAS].ondemand_supply_count =
				0;
	}

	tasha->fw_data = devm_kzalloc(codec->dev,
				      sizeof(*(tasha->fw_data)), GFP_KERNEL);
	if (!tasha->fw_data)
		goto err;
	set_bit(WCD9XXX_ANC_CAL, tasha->fw_data->cal_bit);
	set_bit(WCD9XXX_MBHC_CAL, tasha->fw_data->cal_bit);
	set_bit(WCD9XXX_MAD_CAL, tasha->fw_data->cal_bit);
	set_bit(WCD9XXX_VBAT_CAL, tasha->fw_data->cal_bit);

	ret = wcd_cal_create_hwdep(tasha->fw_data,
				   WCD9XXX_CODEC_HWDEP_NODE, codec);
	if (ret < 0) {
		dev_err(codec->dev, "%s hwdep failed %d\n", __func__, ret);
		goto err_hwdep;
	}

	/* Initialize MBHC module */
	if (TASHA_IS_2_0(tasha->wcd9xxx)) {
		wcd_mbhc_registers[WCD_MBHC_FSM_STATUS].reg =
			WCD9335_MBHC_FSM_STATUS;
		wcd_mbhc_registers[WCD_MBHC_FSM_STATUS].mask = 0x01;
	}
	ret = wcd_mbhc_init(&tasha->mbhc, codec, &mbhc_cb, &intr_ids,
		      wcd_mbhc_registers, TASHA_ZDET_SUPPORTED);
	if (ret) {
		pr_err("%s: mbhc initialization failed\n", __func__);
		goto err_hwdep;
	}

	ptr = devm_kzalloc(codec->dev, (sizeof(tasha_rx_chs) +
			   sizeof(tasha_tx_chs)), GFP_KERNEL);
	if (!ptr) {
		ret = -ENOMEM;
		goto err_hwdep;
	}

	if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
		snd_soc_dapm_new_controls(dapm, tasha_dapm_i2s_widgets,
			ARRAY_SIZE(tasha_dapm_i2s_widgets));
		snd_soc_dapm_add_routes(dapm, audio_i2s_map,
			ARRAY_SIZE(audio_i2s_map));
		for (i = 0; i < ARRAY_SIZE(tasha_i2s_dai); i++) {
			INIT_LIST_HEAD(&tasha->dai[i].wcd9xxx_ch_list);
			init_waitqueue_head(&tasha->dai[i].dai_wait);
		}
	} else if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		for (i = 0; i < NUM_CODEC_DAIS; i++) {
			INIT_LIST_HEAD(&tasha->dai[i].wcd9xxx_ch_list);
			init_waitqueue_head(&tasha->dai[i].dai_wait);
		}
		tasha_slimbus_slave_port_cfg.slave_dev_intfdev_la =
					control->slim_slave->laddr;
		tasha_slimbus_slave_port_cfg.slave_dev_pgd_la =
					control->slim->laddr;
		tasha_slimbus_slave_port_cfg.slave_port_mapping[0] =
					TASHA_TX13;
		tasha_init_slim_slave_cfg(codec);
	}

	snd_soc_add_codec_controls(codec, impedance_detect_controls,
				   ARRAY_SIZE(impedance_detect_controls));
	snd_soc_add_codec_controls(codec, hph_type_detect_controls,
				   ARRAY_SIZE(hph_type_detect_controls));

	snd_soc_add_codec_controls(codec,
			tasha_analog_gain_controls,
			ARRAY_SIZE(tasha_analog_gain_controls));
	if (tasha->is_wsa_attach)
		snd_soc_add_codec_controls(codec,
				tasha_spkr_wsa_controls,
				ARRAY_SIZE(tasha_spkr_wsa_controls));
	control->num_rx_port = TASHA_RX_MAX;
	control->rx_chs = ptr;
	memcpy(control->rx_chs, tasha_rx_chs, sizeof(tasha_rx_chs));
	control->num_tx_port = TASHA_TX_MAX;
	control->tx_chs = ptr + sizeof(tasha_rx_chs);
	memcpy(control->tx_chs, tasha_tx_chs, sizeof(tasha_tx_chs));

	snd_soc_dapm_ignore_suspend(dapm, "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AIF1 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2 Capture");

	if (tasha->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		snd_soc_dapm_ignore_suspend(dapm, "AIF3 Playback");
		snd_soc_dapm_ignore_suspend(dapm, "AIF3 Capture");
		snd_soc_dapm_ignore_suspend(dapm, "AIF4 Playback");
		snd_soc_dapm_ignore_suspend(dapm, "AIF Mix Playback");
		snd_soc_dapm_ignore_suspend(dapm, "AIF4 MAD TX");
		snd_soc_dapm_ignore_suspend(dapm, "VIfeed");
		snd_soc_dapm_ignore_suspend(dapm, "AIF5 CPE TX");
	}

	snd_soc_dapm_sync(dapm);

	ret = tasha_setup_irqs(tasha);
	if (ret) {
		pr_err("%s: tasha irq setup failed %d\n", __func__, ret);
		goto err_pdata;
	}

	ret = tasha_cpe_initialize(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s: cpe initialization failed, err = %d\n",
			__func__, ret);
		/* Do not fail probe if CPE failed */
		ret = 0;
	}

	for (i = 0; i < TASHA_NUM_DECIMATORS; i++) {
		tasha->tx_hpf_work[i].tasha = tasha;
		tasha->tx_hpf_work[i].decimator = i;
		INIT_DELAYED_WORK(&tasha->tx_hpf_work[i].dwork,
			tasha_tx_hpf_corner_freq_callback);
	}

	for (i = 0; i < TASHA_NUM_DECIMATORS; i++) {
		tasha->tx_mute_dwork[i].tasha = tasha;
		tasha->tx_mute_dwork[i].decimator = i;
		INIT_DELAYED_WORK(&tasha->tx_mute_dwork[i].dwork,
			  tasha_tx_mute_update_callback);
	}

	tasha->spk_anc_dwork.tasha = tasha;
	INIT_DELAYED_WORK(&tasha->spk_anc_dwork.dwork,
			  tasha_spk_anc_update_callback);

	mutex_lock(&tasha->codec_mutex);
	snd_soc_dapm_disable_pin(dapm, "ANC LINEOUT1");
	snd_soc_dapm_disable_pin(dapm, "ANC LINEOUT2");
	snd_soc_dapm_disable_pin(dapm, "ANC LINEOUT1 PA");
	snd_soc_dapm_disable_pin(dapm, "ANC LINEOUT2 PA");
	snd_soc_dapm_disable_pin(dapm, "ANC HPHL");
	snd_soc_dapm_disable_pin(dapm, "ANC HPHR");
	snd_soc_dapm_disable_pin(dapm, "ANC HPHL PA");
	snd_soc_dapm_disable_pin(dapm, "ANC HPHR PA");
	snd_soc_dapm_disable_pin(dapm, "ANC EAR PA");
	snd_soc_dapm_disable_pin(dapm, "ANC EAR");
	snd_soc_dapm_disable_pin(dapm, "ANC SPK1 PA");
	mutex_unlock(&tasha->codec_mutex);
	snd_soc_dapm_sync(dapm);

	return ret;

err_pdata:
	devm_kfree(codec->dev, ptr);
	control->rx_chs = NULL;
	control->tx_chs = NULL;
err_hwdep:
	devm_kfree(codec->dev, tasha->fw_data);
	tasha->fw_data = NULL;
err:
	return ret;
}

static int tasha_codec_remove(struct snd_soc_codec *codec)
{
	struct tasha_priv *tasha = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *control;

	control = dev_get_drvdata(codec->dev->parent);
	control->num_rx_port = 0;
	control->num_tx_port = 0;
	control->rx_chs = NULL;
	control->tx_chs = NULL;

	tasha_cleanup_irqs(tasha);
	/* Cleanup MBHC */
	wcd_mbhc_deinit(&tasha->mbhc);
	/* Cleanup resmgr */

	return 0;
}

static struct regmap *tasha_get_regmap(struct device *dev)
{
	struct wcd9xxx *control = dev_get_drvdata(dev->parent);

	return control->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_tasha = {
	.probe = tasha_codec_probe,
	.remove = tasha_codec_remove,
	.get_regmap = tasha_get_regmap,
	.component_driver = {
		.controls = tasha_snd_controls,
		.num_controls = ARRAY_SIZE(tasha_snd_controls),
		.dapm_widgets = tasha_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(tasha_dapm_widgets),
		.dapm_routes = audio_map,
		.num_dapm_routes = ARRAY_SIZE(audio_map),
	},
};

#ifdef CONFIG_PM
static int tasha_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tasha_priv *tasha = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s: system suspend\n", __func__);
	if (cancel_delayed_work_sync(&tasha->power_gate_work))
		tasha_codec_power_gate_digital_core(tasha);

	return 0;
}

static int tasha_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tasha_priv *tasha = platform_get_drvdata(pdev);

	if (!tasha) {
		dev_err(dev, "%s: tasha private data is NULL\n", __func__);
		return -EINVAL;
	}
	dev_dbg(dev, "%s: system resume\n", __func__);
	return 0;
}

static const struct dev_pm_ops tasha_pm_ops = {
	.suspend = tasha_suspend,
	.resume = tasha_resume,
};
#endif

static int tasha_swrm_read(void *handle, int reg)
{
	struct tasha_priv *tasha;
	struct wcd9xxx *wcd9xxx;
	unsigned short swr_rd_addr_base;
	unsigned short swr_rd_data_base;
	int val, ret;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	tasha = (struct tasha_priv *)handle;
	wcd9xxx = tasha->wcd9xxx;

	dev_dbg(tasha->dev, "%s: Reading soundwire register, 0x%x\n",
		__func__, reg);
	swr_rd_addr_base = WCD9335_SWR_AHB_BRIDGE_RD_ADDR_0;
	swr_rd_data_base = WCD9335_SWR_AHB_BRIDGE_RD_DATA_0;
	/* read_lock */
	mutex_lock(&tasha->swr_read_lock);
	ret = regmap_bulk_write(wcd9xxx->regmap, swr_rd_addr_base,
				(u8 *)&reg, 4);
	if (ret < 0) {
		pr_err("%s: RD Addr Failure\n", __func__);
		goto err;
	}
	/* Check for RD status */
	ret = regmap_bulk_read(wcd9xxx->regmap, swr_rd_data_base,
			       (u8 *)&val, 4);
	if (ret < 0) {
		pr_err("%s: RD Data Failure\n", __func__);
		goto err;
	}
	ret = val;
err:
	/* read_unlock */
	mutex_unlock(&tasha->swr_read_lock);
	return ret;
}

static int tasha_swrm_i2s_bulk_write(struct wcd9xxx *wcd9xxx,
				struct wcd9xxx_reg_val *bulk_reg,
				size_t len)
{
	int i, ret = 0;
	unsigned short swr_wr_addr_base;
	unsigned short swr_wr_data_base;

	swr_wr_addr_base = WCD9335_SWR_AHB_BRIDGE_WR_ADDR_0;
	swr_wr_data_base = WCD9335_SWR_AHB_BRIDGE_WR_DATA_0;

	for (i = 0; i < (len * 2); i += 2) {
		/* First Write the Data to register */
		ret = regmap_bulk_write(wcd9xxx->regmap,
			swr_wr_data_base, bulk_reg[i].buf, 4);
		if (ret < 0) {
			dev_err(wcd9xxx->dev, "%s: WR Data Failure\n",
				__func__);
			break;
		}
		/* Next Write Address */
		ret = regmap_bulk_write(wcd9xxx->regmap,
			swr_wr_addr_base, bulk_reg[i+1].buf, 4);
		if (ret < 0) {
			dev_err(wcd9xxx->dev, "%s: WR Addr Failure\n",
				__func__);
			break;
		}
	}
	return ret;
}

static int tasha_swrm_bulk_write(void *handle, u32 *reg, u32 *val, size_t len)
{
	struct tasha_priv *tasha;
	struct wcd9xxx *wcd9xxx;
	struct wcd9xxx_reg_val *bulk_reg;
	unsigned short swr_wr_addr_base;
	unsigned short swr_wr_data_base;
	int i, j, ret;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	if (len <= 0) {
		pr_err("%s: Invalid size: %zu\n", __func__, len);
		return -EINVAL;
	}
	tasha = (struct tasha_priv *)handle;
	wcd9xxx = tasha->wcd9xxx;

	swr_wr_addr_base = WCD9335_SWR_AHB_BRIDGE_WR_ADDR_0;
	swr_wr_data_base = WCD9335_SWR_AHB_BRIDGE_WR_DATA_0;

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
	mutex_lock(&tasha->swr_write_lock);

	if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C) {
		ret = tasha_swrm_i2s_bulk_write(wcd9xxx, bulk_reg, len);
		if (ret) {
			dev_err(tasha->dev, "%s: i2s bulk write failed, ret: %d\n",
				__func__, ret);
		}
	} else {
		ret = wcd9xxx_slim_bulk_write(wcd9xxx, bulk_reg,
				 (len * 2), false);
		if (ret) {
			dev_err(tasha->dev, "%s: swrm bulk write failed, ret: %d\n",
				__func__, ret);
		}
	}

	mutex_unlock(&tasha->swr_write_lock);
	kfree(bulk_reg);

	return ret;
}

static int tasha_swrm_write(void *handle, int reg, int val)
{
	struct tasha_priv *tasha;
	struct wcd9xxx *wcd9xxx;
	unsigned short swr_wr_addr_base;
	unsigned short swr_wr_data_base;
	struct wcd9xxx_reg_val bulk_reg[2];
	int ret;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	tasha = (struct tasha_priv *)handle;
	wcd9xxx = tasha->wcd9xxx;

	swr_wr_addr_base = WCD9335_SWR_AHB_BRIDGE_WR_ADDR_0;
	swr_wr_data_base = WCD9335_SWR_AHB_BRIDGE_WR_DATA_0;

	/* First Write the Data to register */
	bulk_reg[0].reg = swr_wr_data_base;
	bulk_reg[0].buf = (u8 *)(&val);
	bulk_reg[0].bytes = 4;
	bulk_reg[1].reg = swr_wr_addr_base;
	bulk_reg[1].buf = (u8 *)(&reg);
	bulk_reg[1].bytes = 4;

	mutex_lock(&tasha->swr_write_lock);

	if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C) {
		ret = tasha_swrm_i2s_bulk_write(wcd9xxx, bulk_reg, 1);
		if (ret) {
			dev_err(tasha->dev, "%s: i2s swrm write failed, ret: %d\n",
				__func__, ret);
		}
	} else {
		ret = wcd9xxx_slim_bulk_write(wcd9xxx, bulk_reg, 2, false);
		if (ret < 0)
			pr_err("%s: WR Data Failure\n", __func__);
	}

	mutex_unlock(&tasha->swr_write_lock);
	return ret;
}

static int tasha_swrm_clock(void *handle, bool enable)
{
	struct tasha_priv *tasha = (struct tasha_priv *) handle;

	mutex_lock(&tasha->swr_clk_lock);

	dev_dbg(tasha->dev, "%s: swrm clock %s\n",
		__func__, (enable?"enable" : "disable"));
	if (enable) {
		tasha->swr_clk_users++;
		if (tasha->swr_clk_users == 1) {
			if (TASHA_IS_2_0(tasha->wcd9xxx))
				regmap_update_bits(
					tasha->wcd9xxx->regmap,
					WCD9335_TEST_DEBUG_NPL_DLY_TEST_1,
					0x10, 0x00);
			__tasha_cdc_mclk_enable(tasha, true);
			regmap_update_bits(tasha->wcd9xxx->regmap,
				WCD9335_CDC_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x01);
		}
	} else {
		tasha->swr_clk_users--;
		if (tasha->swr_clk_users == 0) {
			regmap_update_bits(tasha->wcd9xxx->regmap,
				WCD9335_CDC_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x00);
			__tasha_cdc_mclk_enable(tasha, false);
			if (TASHA_IS_2_0(tasha->wcd9xxx))
				regmap_update_bits(
					tasha->wcd9xxx->regmap,
					WCD9335_TEST_DEBUG_NPL_DLY_TEST_1,
					0x10, 0x10);
		}
	}
	dev_dbg(tasha->dev, "%s: swrm clock users %d\n",
		__func__, tasha->swr_clk_users);
	mutex_unlock(&tasha->swr_clk_lock);
	return 0;
}

static int tasha_swrm_handle_irq(void *handle,
				   irqreturn_t (*swrm_irq_handler)(int irq,
								   void *data),
				    void *swrm_handle,
				    int action)
{
	struct tasha_priv *tasha;
	int ret = 0;
	struct wcd9xxx *wcd9xxx;

	if (!handle) {
		pr_err("%s: null handle received\n", __func__);
		return -EINVAL;
	}
	tasha = (struct tasha_priv *) handle;
	wcd9xxx = tasha->wcd9xxx;

	if (action) {
		ret = wcd9xxx_request_irq(&wcd9xxx->core_res,
					  WCD9335_IRQ_SOUNDWIRE,
					  swrm_irq_handler,
					  "Tasha SWR Master", swrm_handle);
		if (ret)
			dev_err(tasha->dev, "%s: Failed to request irq %d\n",
				__func__, WCD9335_IRQ_SOUNDWIRE);
	} else
		wcd9xxx_free_irq(&wcd9xxx->core_res, WCD9335_IRQ_SOUNDWIRE,
				 swrm_handle);

	return ret;
}

static void tasha_add_child_devices(struct work_struct *work)
{
	struct tasha_priv *tasha;
	struct platform_device *pdev;
	struct device_node *node;
	struct wcd9xxx *wcd9xxx;
	struct tasha_swr_ctrl_data *swr_ctrl_data = NULL, *temp;
	int ret, ctrl_num = 0;
	struct wcd_swr_ctrl_platform_data *platdata;
	char plat_dev_name[WCD9335_STRING_LEN];

	tasha = container_of(work, struct tasha_priv,
			     tasha_add_child_devices_work);
	if (!tasha) {
		pr_err("%s: Memory for WCD9335 does not exist\n",
			__func__);
		return;
	}
	wcd9xxx = tasha->wcd9xxx;
	if (!wcd9xxx) {
		pr_err("%s: Memory for WCD9XXX does not exist\n",
			__func__);
		return;
	}
	if (!wcd9xxx->dev->of_node) {
		pr_err("%s: DT node for wcd9xxx does not exist\n",
			__func__);
		return;
	}

	platdata = &tasha->swr_plat_data;
	tasha->child_count = 0;

	for_each_child_of_node(wcd9xxx->dev->of_node, node) {
		if (!strcmp(node->name, "swr_master"))
			strlcpy(plat_dev_name, "tasha_swr_ctrl",
				(WCD9335_STRING_LEN - 1));
		else if (strnstr(node->name, "msm_cdc_pinctrl",
				 strlen("msm_cdc_pinctrl")) != NULL)
			strlcpy(plat_dev_name, node->name,
				(WCD9335_STRING_LEN - 1));
		else
			continue;

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(wcd9xxx->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = tasha->dev;
		pdev->dev.of_node = node;

		if (!strcmp(node->name, "swr_master")) {
			ret = platform_device_add_data(pdev, platdata,
						       sizeof(*platdata));
			if (ret) {
				dev_err(&pdev->dev,
					"%s: cannot add plat data ctrl:%d\n",
					__func__, ctrl_num);
				goto fail_pdev_add;
			}
			tasha->is_wsa_attach = true;
		}

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Cannot add platform device\n",
				__func__);
			goto fail_pdev_add;
		}

		if (!strcmp(node->name, "swr_master")) {
			temp = krealloc(swr_ctrl_data,
					(ctrl_num + 1) * sizeof(
					struct tasha_swr_ctrl_data),
					GFP_KERNEL);
			if (!temp) {
				dev_err(wcd9xxx->dev, "out of memory\n");
				ret = -ENOMEM;
				goto err;
			}
			swr_ctrl_data = temp;
			swr_ctrl_data[ctrl_num].swr_pdev = pdev;
			ctrl_num++;
			dev_dbg(&pdev->dev,
				"%s: Added soundwire ctrl device(s)\n",
				__func__);
			tasha->nr = ctrl_num;
			tasha->swr_ctrl_data = swr_ctrl_data;
		}

		if (tasha->child_count < WCD9335_CHILD_DEVICES_MAX)
			tasha->pdev_child_devices[tasha->child_count++] = pdev;
		else
			goto err;
	}

	return;
fail_pdev_add:
	platform_device_put(pdev);
err:
	return;
}

/*
 * tasha_codec_ver: to get tasha codec version
 * @codec: handle to snd_soc_codec *
 * return enum codec_variant - version
 */
enum codec_variant tasha_codec_ver(void)
{
	return codec_ver;
}
EXPORT_SYMBOL(tasha_codec_ver);

static int __tasha_enable_efuse_sensing(struct tasha_priv *tasha)
{
	int val, rc;

	__tasha_cdc_mclk_enable(tasha, true);

	regmap_update_bits(tasha->wcd9xxx->regmap,
			   WCD9335_CHIP_TIER_CTRL_EFUSE_CTL, 0x1E, 0x20);
	regmap_update_bits(tasha->wcd9xxx->regmap,
			   WCD9335_CHIP_TIER_CTRL_EFUSE_CTL, 0x01, 0x01);

	/*
	 * 5ms sleep required after enabling efuse control
	 * before checking the status.
	 */
	usleep_range(5000, 5500);
	rc = regmap_read(tasha->wcd9xxx->regmap,
			 WCD9335_CHIP_TIER_CTRL_EFUSE_STATUS, &val);

	if (rc || (!(val & 0x01)))
		WARN(1, "%s: Efuse sense is not complete\n", __func__);

	__tasha_cdc_mclk_enable(tasha, false);

	return rc;
}

void tasha_get_codec_ver(struct tasha_priv *tasha)
{
	int i;
	int val;
	struct tasha_reg_mask_val codec_reg[] = {
		{WCD9335_CHIP_TIER_CTRL_EFUSE_VAL_OUT10, 0xFF, 0xFF},
		{WCD9335_CHIP_TIER_CTRL_EFUSE_VAL_OUT11, 0xFF, 0x83},
		{WCD9335_CHIP_TIER_CTRL_EFUSE_VAL_OUT12, 0xFF, 0x0A},
	};

	__tasha_enable_efuse_sensing(tasha);
	for (i = 0; i < ARRAY_SIZE(codec_reg); i++) {
		regmap_read(tasha->wcd9xxx->regmap, codec_reg[i].reg, &val);
		if (!(val && codec_reg[i].val)) {
			codec_ver = WCD9335;
			goto ret;
		}
	}
	codec_ver = WCD9326;
ret:
	pr_debug("%s: codec is %d\n", __func__, codec_ver);
}
EXPORT_SYMBOL(tasha_get_codec_ver);

static int tasha_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tasha_priv *tasha;
	struct clk *wcd_ext_clk, *wcd_native_clk;
	struct wcd9xxx_resmgr_v2 *resmgr;
	struct wcd9xxx_power_region *cdc_pwr;

	if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C) {
		if (apr_get_subsys_state() == APR_SUBSYS_DOWN) {
			dev_err(&pdev->dev, "%s: dsp down\n", __func__);
			return -EPROBE_DEFER;
		}
	}

	tasha = devm_kzalloc(&pdev->dev, sizeof(struct tasha_priv),
			    GFP_KERNEL);
	if (!tasha)
		return -ENOMEM;
	platform_set_drvdata(pdev, tasha);

	tasha->wcd9xxx = dev_get_drvdata(pdev->dev.parent);
	tasha->dev = &pdev->dev;
	INIT_DELAYED_WORK(&tasha->power_gate_work, tasha_codec_power_gate_work);
	mutex_init(&tasha->power_lock);
	mutex_init(&tasha->sido_lock);
	INIT_WORK(&tasha->tasha_add_child_devices_work,
		  tasha_add_child_devices);
	BLOCKING_INIT_NOTIFIER_HEAD(&tasha->notifier);
	mutex_init(&tasha->micb_lock);
	mutex_init(&tasha->swr_read_lock);
	mutex_init(&tasha->swr_write_lock);
	mutex_init(&tasha->swr_clk_lock);
	mutex_init(&tasha->sb_clk_gear_lock);
	mutex_init(&tasha->mclk_lock);

	cdc_pwr = devm_kzalloc(&pdev->dev, sizeof(struct wcd9xxx_power_region),
			       GFP_KERNEL);
	if (!cdc_pwr) {
		ret = -ENOMEM;
		goto err_cdc_pwr;
	}
	tasha->wcd9xxx->wcd9xxx_pwr[WCD9XXX_DIG_CORE_REGION_1] = cdc_pwr;
	cdc_pwr->pwr_collapse_reg_min = TASHA_DIG_CORE_REG_MIN;
	cdc_pwr->pwr_collapse_reg_max = TASHA_DIG_CORE_REG_MAX;
	wcd9xxx_set_power_state(tasha->wcd9xxx,
				WCD_REGION_POWER_COLLAPSE_REMOVE,
				WCD9XXX_DIG_CORE_REGION_1);

	mutex_init(&tasha->codec_mutex);
	/*
	 * Init resource manager so that if child nodes such as SoundWire
	 * requests for clock, resource manager can honor the request
	 */
	resmgr = wcd_resmgr_init(&tasha->wcd9xxx->core_res, NULL);
	if (IS_ERR(resmgr)) {
		ret = PTR_ERR(resmgr);
		dev_err(&pdev->dev, "%s: Failed to initialize wcd resmgr\n",
			__func__);
		goto err_resmgr;
	}
	tasha->resmgr = resmgr;
	tasha->swr_plat_data.handle = (void *) tasha;
	tasha->swr_plat_data.read = tasha_swrm_read;
	tasha->swr_plat_data.write = tasha_swrm_write;
	tasha->swr_plat_data.bulk_write = tasha_swrm_bulk_write;
	tasha->swr_plat_data.clk = tasha_swrm_clock;
	tasha->swr_plat_data.handle_irq = tasha_swrm_handle_irq;

	/* Register for Clock */
	wcd_ext_clk = clk_get(tasha->wcd9xxx->dev, "wcd_clk");
	if (IS_ERR(wcd_ext_clk)) {
		dev_err(tasha->wcd9xxx->dev, "%s: clk get %s failed\n",
			__func__, "wcd_ext_clk");
		goto err_clk;
	}
	tasha->wcd_ext_clk = wcd_ext_clk;
	tasha->sido_voltage = SIDO_VOLTAGE_NOMINAL_MV;
	set_bit(AUDIO_NOMINAL, &tasha->status_mask);
	tasha->sido_ccl_cnt = 0;

	/* Register native clk for 44.1 playback */
	wcd_native_clk = clk_get(tasha->wcd9xxx->dev, "wcd_native_clk");
	if (IS_ERR(wcd_native_clk))
		dev_dbg(tasha->wcd9xxx->dev, "%s: clk get %s failed\n",
			__func__, "wcd_native_clk");
	else
		tasha->wcd_native_clk = wcd_native_clk;

	if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_tasha,
					     tasha_dai, ARRAY_SIZE(tasha_dai));
	else if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
		ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_tasha,
					     tasha_i2s_dai,
					     ARRAY_SIZE(tasha_i2s_dai));
	else
		ret = -EINVAL;
	if (ret) {
		dev_err(&pdev->dev, "%s: Codec registration failed, ret = %d\n",
			__func__, ret);
		goto err_cdc_reg;
	}
	/* Update codec register default values */
	tasha_update_reg_defaults(tasha);
	schedule_work(&tasha->tasha_add_child_devices_work);
	tasha_get_codec_ver(tasha);
	ret = snd_event_client_register(pdev->dev.parent, &tasha_ssr_ops, NULL);
	if (!ret) {
		snd_event_notify(pdev->dev.parent, SND_EVENT_UP);
	} else {
		pr_err("%s: Registration with SND event fwk failed ret = %d\n",
			   __func__, ret);
		ret = 0;
	}

	dev_info(&pdev->dev, "%s: Tasha driver probe done\n", __func__);
	return ret;

err_cdc_reg:
	clk_put(tasha->wcd_ext_clk);
	if (tasha->wcd_native_clk)
		clk_put(tasha->wcd_native_clk);
err_clk:
	wcd_resmgr_remove(tasha->resmgr);
err_resmgr:
	devm_kfree(&pdev->dev, cdc_pwr);
err_cdc_pwr:
	mutex_destroy(&tasha->mclk_lock);
	devm_kfree(&pdev->dev, tasha);
	return ret;
}

static int tasha_remove(struct platform_device *pdev)
{
	struct tasha_priv *tasha;
	int count = 0;

	tasha = platform_get_drvdata(pdev);

	if (!tasha)
		return -EINVAL;

	snd_event_client_deregister(pdev->dev.parent);
	for (count = 0; count < tasha->child_count &&
		count < WCD9335_CHILD_DEVICES_MAX; count++)
		platform_device_unregister(tasha->pdev_child_devices[count]);

	mutex_destroy(&tasha->codec_mutex);
	clk_put(tasha->wcd_ext_clk);
	if (tasha->wcd_native_clk)
		clk_put(tasha->wcd_native_clk);
	mutex_destroy(&tasha->mclk_lock);
	mutex_destroy(&tasha->sb_clk_gear_lock);
	snd_soc_unregister_codec(&pdev->dev);
	devm_kfree(&pdev->dev, tasha);
	return 0;
}

static struct platform_driver tasha_codec_driver = {
	.probe = tasha_probe,
	.remove = tasha_remove,
	.driver = {
		.name = "tasha_codec",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &tasha_pm_ops,
#endif
	},
};

module_platform_driver(tasha_codec_driver);

MODULE_DESCRIPTION("Tasha Codec driver");
MODULE_LICENSE("GPL v2");
