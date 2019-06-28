/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include <soc/swr-wcd.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/info.h>
#include <asoc/wcd9360-registers.h>
#include "wcd9360.h"
#include "wcd9360-routing.h"
#include "wcd9360-dsp-cntl.h"
#include "wcd9360-irq.h"
#include "../core.h"
#include "../pdata.h"
#include "../wcd9xxx-irq.h"
#include "../wcd9xxx-common-v2.h"
#include "../wcd9xxx-resmgr-v2.h"
#include "../wcdcal-hwdep.h"
#include "../msm-cdc-supply.h"


#define WCD9360_RATES_MASK (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			    SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			    SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 |\
			    SNDRV_PCM_RATE_384000)
/* Fractional Rates */
#define WCD9360_FRAC_RATES_MASK (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 |\
				 SNDRV_PCM_RATE_176400)

#define WCD9360_FORMATS_S16_S24_LE (SNDRV_PCM_FMTBIT_S16_LE | \
				    SNDRV_PCM_FMTBIT_S24_LE)

#define WCD9360_FORMATS_S16_S24_S32_LE (SNDRV_PCM_FMTBIT_S16_LE | \
					SNDRV_PCM_FMTBIT_S24_LE | \
					SNDRV_PCM_FMTBIT_S32_LE)

#define WCD9360_FORMATS_S16_LE (SNDRV_PCM_FMTBIT_S16_LE)

/* Macros for packing register writes into a U32 */
#define WCD9360_PACKED_REG_SIZE sizeof(u32)
#define WCD9360_CODEC_UNPACK_ENTRY(packed, reg, mask, val) \
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
#define WCD9360_SLIM_CLOSE_TIMEOUT 1000
#define WCD9360_SLIM_IRQ_OVERFLOW (1 << 0)
#define WCD9360_SLIM_IRQ_UNDERFLOW (1 << 1)
#define WCD9360_SLIM_IRQ_PORT_CLOSED (1 << 2)

#define WCD9360_MCLK_CLK_9P6MHZ 9600000

#define WCD9360_INTERP_MUX_NUM_INPUTS 3
#define WCD9360_NUM_INTERPOLATORS 10
#define WCD9360_NUM_DECIMATORS 9
#define WCD9360_RX_PATH_CTL_OFFSET 20
#define WCD9360_TLMM_DMIC_PINCFG_OFFSET 15

#define BYTE_BIT_MASK(nr) (1 << ((nr) % BITS_PER_BYTE))

#define WCD9360_REG_BITS 8
#define WCD9360_MAX_VALID_ADC_MUX  11
#define WCD9360_INVALID_ADC_MUX 9

#define WCD9360_AMIC_PWR_LEVEL_LP 0
#define WCD9360_AMIC_PWR_LEVEL_DEFAULT 1
#define WCD9360_AMIC_PWR_LEVEL_HP 2
#define WCD9360_AMIC_PWR_LVL_MASK 0x60
#define WCD9360_AMIC_PWR_LVL_SHIFT 0x5

#define WCD9360_DEC_PWR_LVL_MASK 0x06
#define WCD9360_DEC_PWR_LVL_LP 0x02
#define WCD9360_DEC_PWR_LVL_HP 0x04
#define WCD9360_DEC_PWR_LVL_DF 0x00
#define WCD9360_STRING_LEN 100

#define WCD9360_CDC_SIDETONE_IIR_COEFF_MAX 5
#define WCD9360_CDC_REPEAT_WRITES_MAX 16
#define WCD9360_DIG_CORE_REG_MIN  WCD9360_CDC_ANC0_CLK_RESET_CTL
#define WCD9360_DIG_CORE_REG_MAX  0xFFF

#define WCD9360_CHILD_DEVICES_MAX	6

#define WCD9360_MAX_MICBIAS 4
#define DAPM_MICBIAS1_STANDALONE "MIC BIAS1 Standalone"
#define DAPM_MICBIAS2_STANDALONE "MIC BIAS2 Standalone"
#define DAPM_MICBIAS3_STANDALONE "MIC BIAS3 Standalone"
#define DAPM_MICBIAS4_STANDALONE "MIC BIAS4 Standalone"

#define WCD9360_LDO_RXTX_SUPPLY_NAME "cdc-vdd-ldo-rxtx"

#define  TX_HPF_CUT_OFF_FREQ_MASK	0x60
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

#define CPE_ERR_WDOG_BITE BIT(0)
#define CPE_FATAL_IRQS CPE_ERR_WDOG_BITE

#define WCD9360_MAD_AUDIO_FIRMWARE_PATH "wcd9360/wcd9360_mad_audio.bin"

#define PAHU_VERSION_ENTRY_SIZE 17

#define WCD9360_DIG_CORE_COLLAPSE_TIMER_MS  (5 * 1000)

enum {
	INTERP_EAR = 0,
	/* Headset and Lineout are not avalible in pahu */
	INTERP_HPHL_NA,
	INTERP_HPHR_NA,
	INTERP_LO1_NA,
	INTERP_LO2_NA,
	INTERP_LO3_NA,
	INTERP_LO4_NA,
	INTERP_SPKR1,
	INTERP_SPKR2,
	INTERP_AUX,
	INTERP_MAX,
};

enum {
	POWER_COLLAPSE,
	POWER_RESUME,
};

static int dig_core_collapse_enable = 1;
module_param(dig_core_collapse_enable, int, 0664);
MODULE_PARM_DESC(dig_core_collapse_enable, "enable/disable power gating");

/* dig_core_collapse timer in seconds */
static int dig_core_collapse_timer = (WCD9360_DIG_CORE_COLLAPSE_TIMER_MS/1000);
module_param(dig_core_collapse_timer, int, 0664);
MODULE_PARM_DESC(dig_core_collapse_timer, "timer for power gating");

enum {
	VI_SENSE_1,
	VI_SENSE_2,
	CLK_INTERNAL,
	CLK_MODE,
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
	I2S1_PB,
	I2S1_CAP,
	NUM_CODEC_DAIS,
};

enum {
	INTn_1_INP_SEL_ZERO = 0,
	INTn_1_INP_SEL_DEC0,
	INTn_1_INP_SEL_DEC1,
	INTn_1_INP_SEL_IIR0,
	INTn_1_INP_SEL_NA,
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

struct pahu_cpr_reg_defaults {
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

static const struct wcd9xxx_ch pahu_rx_chs[WCD9360_RX_MAX] = {
	WCD9XXX_CH(WCD9360_RX_PORT_START_NUMBER, 0),
	WCD9XXX_CH(WCD9360_RX_PORT_START_NUMBER + 1, 1),
	WCD9XXX_CH(WCD9360_RX_PORT_START_NUMBER + 2, 2),
	WCD9XXX_CH(WCD9360_RX_PORT_START_NUMBER + 3, 3),
	WCD9XXX_CH(WCD9360_RX_PORT_START_NUMBER + 4, 4),
	WCD9XXX_CH(WCD9360_RX_PORT_START_NUMBER + 5, 5),
	WCD9XXX_CH(WCD9360_RX_PORT_START_NUMBER + 6, 6),
	WCD9XXX_CH(WCD9360_RX_PORT_START_NUMBER + 7, 7),
};

static const struct wcd9xxx_ch pahu_tx_chs[WCD9360_TX_MAX] = {
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
	COMPANDER_0, /* EAR */
	COMPANDER_1, /* HPH_L */
	COMPANDER_2, /* HPH_R */
	COMPANDER_3, /* LO1_DIFF */
	COMPANDER_4, /* LO2_DIFF */
	COMPANDER_5, /* LO3_SE */
	COMPANDER_6, /* LO4_SE */
	COMPANDER_7, /* SWR SPK CH1 */
	COMPANDER_8, /* SWR SPK CH2 */
	COMPANDER_9, /* AUX */
	COMPANDER_MAX,
};

enum {
	ASRC_IN_SPKR1,
	ASRC_IN_SPKR2,
	ASRC_INVALID,
};

enum {
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

static struct afe_param_slimbus_slave_port_cfg pahu_slimbus_slave_port_cfg = {
	.minor_version = 1,
	.slimbus_dev_id = AFE_SLIMBUS_DEVICE_1,
	.slave_dev_pgd_la = 0,
	.slave_dev_intfdev_la = 0,
	.bit_width = 16,
	.data_format = 0,
	.num_channels = 1
};

static struct afe_param_cdc_reg_page_cfg pahu_cdc_reg_page_cfg = {
	.minor_version = AFE_API_VERSION_CDC_REG_PAGE_CFG,
	.enable = 1,
	.proc_id = AFE_CDC_REG_PAGE_ASSIGN_PROC_ID_1,
};

static struct afe_param_cdc_reg_cfg audio_reg_cfg[] = {
	{
		1,
		(WCD9360_REGISTER_START_OFFSET + WCD9360_SOC_MAD_MAIN_CTL_1),
		HW_MAD_AUDIO_ENABLE, 0x1, WCD9360_REG_BITS, 0
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET + WCD9360_SOC_MAD_AUDIO_CTL_3),
		HW_MAD_AUDIO_SLEEP_TIME, 0xF, WCD9360_REG_BITS, 0
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET + WCD9360_SOC_MAD_AUDIO_CTL_4),
		HW_MAD_TX_AUDIO_SWITCH_OFF, 0x1, WCD9360_REG_BITS, 0
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET + WCD9360_INTR_CFG),
		MAD_AUDIO_INT_DEST_SELECT_REG, 0x2, WCD9360_REG_BITS, 0
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET + WCD9360_INTR_PIN2_MASK3),
		MAD_AUDIO_INT_MASK_REG, 0x1, WCD9360_REG_BITS, 0
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET + WCD9360_INTR_PIN2_STATUS3),
		MAD_AUDIO_INT_STATUS_REG, 0x1, WCD9360_REG_BITS, 0
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET + WCD9360_INTR_PIN2_CLEAR3),
		MAD_AUDIO_INT_CLEAR_REG, 0x1, WCD9360_REG_BITS, 0
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET + WCD9360_SB_PGD_PORT_TX_BASE),
		SB_PGD_PORT_TX_WATERMARK_N, 0x1E, WCD9360_REG_BITS, 0x1
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET + WCD9360_SB_PGD_PORT_TX_BASE),
		SB_PGD_PORT_TX_ENABLE_N, 0x1, WCD9360_REG_BITS, 0x1
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET + WCD9360_SB_PGD_PORT_RX_BASE),
		SB_PGD_PORT_RX_WATERMARK_N, 0x1E, WCD9360_REG_BITS, 0x1
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET + WCD9360_SB_PGD_PORT_RX_BASE),
		SB_PGD_PORT_RX_ENABLE_N, 0x1, WCD9360_REG_BITS, 0x1
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET +
		 WCD9360_CDC_ANC0_IIR_ADAPT_CTL),
		AANC_FF_GAIN_ADAPTIVE, 0x4, WCD9360_REG_BITS, 0
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET +
		 WCD9360_CDC_ANC0_IIR_ADAPT_CTL),
		AANC_FFGAIN_ADAPTIVE_EN, 0x8, WCD9360_REG_BITS, 0
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET +
		 WCD9360_CDC_ANC0_FF_A_GAIN_CTL),
		AANC_GAIN_CONTROL, 0xFF, WCD9360_REG_BITS, 0
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET +
		 SB_PGD_TX_PORT_MULTI_CHANNEL_0(0)),
		SB_PGD_TX_PORTn_MULTI_CHNL_0, 0xFF, WCD9360_REG_BITS, 0x4
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET +
		 SB_PGD_TX_PORT_MULTI_CHANNEL_1(0)),
		SB_PGD_TX_PORTn_MULTI_CHNL_1, 0xFF, WCD9360_REG_BITS, 0x4
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET +
		 SB_PGD_RX_PORT_MULTI_CHANNEL_0(0x180, 0)),
		SB_PGD_RX_PORTn_MULTI_CHNL_0, 0xFF, WCD9360_REG_BITS, 0x4
	},
	{
		1,
		(WCD9360_REGISTER_START_OFFSET +
		 SB_PGD_RX_PORT_MULTI_CHANNEL_0(0x181, 0)),
		SB_PGD_RX_PORTn_MULTI_CHNL_1, 0xFF, WCD9360_REG_BITS, 0x4
	},
};

static struct afe_param_cdc_reg_cfg_data pahu_audio_reg_cfg = {
	.num_registers = ARRAY_SIZE(audio_reg_cfg),
	.reg_data = audio_reg_cfg,
};

static struct afe_param_id_cdc_aanc_version pahu_cdc_aanc_version = {
	.cdc_aanc_minor_version = AFE_API_VERSION_CDC_AANC_VERSION,
	.aanc_hw_version        = AANC_HW_BLOCK_VERSION_2,
};

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);

#define WCD9360_TX_UNMUTE_DELAY_MS 40

static int tx_unmute_delay = WCD9360_TX_UNMUTE_DELAY_MS;
module_param(tx_unmute_delay, int, 0664);
MODULE_PARM_DESC(tx_unmute_delay, "delay to unmute the tx path");

static void pahu_codec_set_tx_hold(struct snd_soc_codec *, u16, bool);

/* Hold instance to soundwire platform device */
struct pahu_swr_ctrl_data {
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
struct wcd9360_swr {
	struct pahu_swr_ctrl_data *ctrl_data;
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
	struct pahu_priv *pahu;
	u8 decimator;
	struct delayed_work dwork;
};

#define WCD9360_SPK_ANC_EN_DELAY_MS 550
static int spk_anc_en_delay = WCD9360_SPK_ANC_EN_DELAY_MS;
module_param(spk_anc_en_delay, int, 0664);
MODULE_PARM_DESC(spk_anc_en_delay, "delay to enable anc in speaker path");

struct spk_anc_work {
	struct pahu_priv *pahu;
	struct delayed_work dwork;
};

struct hpf_work {
	struct pahu_priv *pahu;
	u8 decimator;
	u8 hpf_cut_off_freq;
	struct delayed_work dwork;
};

struct pahu_priv {
	struct device *dev;
	struct wcd9xxx *wcd9xxx;
	struct snd_soc_codec *codec;
	s32 ldo_rxtx_cnt;
	s32 dmic_0_1_clk_cnt;
	s32 dmic_2_3_clk_cnt;
	s32 dmic_4_5_clk_cnt;
	s32 dmic_6_7_clk_cnt;
	s32 micb_ref[PAHU_MAX_MICBIAS];
	s32 pullup_ref[PAHU_MAX_MICBIAS];

	/* ANC related */
	u32 anc_slot;
	bool anc_func;

	/* compander */
	int comp_enabled[COMPANDER_MAX];
	int ear_spkr_gain;

	/* Mad switch reference count */
	int mad_switch_cnt;

	/* track pahu interface type */
	u8 intf_type;

	/* to track the status */
	unsigned long status_mask;

	struct afe_param_cdc_slimbus_slave_cfg slimbus_slave_cfg;

	/* num of slim ports required */
	struct wcd9xxx_codec_dai_data  dai[NUM_CODEC_DAIS];
	/* Port values for Rx and Tx codec_dai */
	unsigned int rx_port_value[WCD9360_RX_MAX];
	unsigned int tx_port_value;

	struct wcd9xxx_resmgr_v2 *resmgr;
	struct wcd9360_swr swr;
	struct mutex micb_lock;

	struct delayed_work power_gate_work;
	struct mutex power_lock;

	struct clk *wcd_ext_clk;

	struct mutex codec_mutex;
	struct work_struct pahu_add_child_devices_work;
	struct hpf_work tx_hpf_work[WCD9360_NUM_DECIMATORS];
	struct tx_mute_work tx_mute_dwork[WCD9360_NUM_DECIMATORS];
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
	int main_clk_users[WCD9360_NUM_INTERPOLATORS];

	int power_active_ref;
	u8 sidetone_coeff_array[IIR_MAX][BAND_MAX]
		[WCD9360_CDC_SIDETONE_IIR_COEFF_MAX * 4];

	struct spi_device *spi;
	struct platform_device *pdev_child_devices
		[WCD9360_CHILD_DEVICES_MAX];
	int child_count;
	int i2s_ref_cnt;
};

static const struct pahu_reg_mask_val pahu_spkr_default[] = {
	{WCD9360_CDC_COMPANDER7_CTL3, 0x80, 0x80},
	{WCD9360_CDC_COMPANDER8_CTL3, 0x80, 0x80},
	{WCD9360_CDC_COMPANDER7_CTL7, 0x01, 0x01},
	{WCD9360_CDC_COMPANDER8_CTL7, 0x01, 0x01},
	{WCD9360_CDC_BOOST0_BOOST_CTL, 0x7C, 0x50},
	{WCD9360_CDC_BOOST1_BOOST_CTL, 0x7C, 0x50},
};

static const struct pahu_reg_mask_val pahu_spkr_mode1[] = {
	{WCD9360_CDC_COMPANDER7_CTL3, 0x80, 0x00},
	{WCD9360_CDC_COMPANDER8_CTL3, 0x80, 0x00},
	{WCD9360_CDC_COMPANDER7_CTL7, 0x01, 0x00},
	{WCD9360_CDC_COMPANDER8_CTL7, 0x01, 0x00},
	{WCD9360_CDC_BOOST0_BOOST_CTL, 0x7C, 0x44},
	{WCD9360_CDC_BOOST1_BOOST_CTL, 0x7C, 0x44},
};

static int __pahu_enable_efuse_sensing(struct pahu_priv *pahu);

/**
 * pahu_set_spkr_gain_offset - offset the speaker path
 * gain with the given offset value.
 *
 * @codec: codec instance
 * @offset: Indicates speaker path gain offset value.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int pahu_set_spkr_gain_offset(struct snd_soc_codec *codec, int offset)
{
	struct pahu_priv *priv = snd_soc_codec_get_drvdata(codec);

	if (!priv)
		return -EINVAL;

	priv->swr.spkr_gain_offset = offset;
	return 0;
}
EXPORT_SYMBOL(pahu_set_spkr_gain_offset);

/**
 * pahu_set_spkr_mode - Configures speaker compander and smartboost
 * settings based on speaker mode.
 *
 * @codec: codec instance
 * @mode: Indicates speaker configuration mode.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int pahu_set_spkr_mode(struct snd_soc_codec *codec, int mode)
{
	struct pahu_priv *priv = snd_soc_codec_get_drvdata(codec);
	int i;
	const struct pahu_reg_mask_val *regs;
	int size;

	if (!priv)
		return -EINVAL;

	switch (mode) {
	case WCD9360_SPKR_MODE_1:
		regs = pahu_spkr_mode1;
		size = ARRAY_SIZE(pahu_spkr_mode1);
		break;
	default:
		regs = pahu_spkr_default;
		size = ARRAY_SIZE(pahu_spkr_default);
		break;
	}

	priv->swr.spkr_mode = mode;
	for (i = 0; i < size; i++)
		snd_soc_update_bits(codec, regs[i].reg,
				    regs[i].mask, regs[i].val);
	return 0;
}
EXPORT_SYMBOL(pahu_set_spkr_mode);

/**
 * pahu_get_afe_config - returns specific codec configuration to afe to write
 *
 * @codec: codec instance
 * @config_type: Indicates type of configuration to write.
 */
void *pahu_get_afe_config(struct snd_soc_codec *codec,
			   enum afe_config_type config_type)
{
	struct pahu_priv *priv = snd_soc_codec_get_drvdata(codec);

	switch (config_type) {
	case AFE_SLIMBUS_SLAVE_CONFIG:
		return &priv->slimbus_slave_cfg;
	case AFE_CDC_REGISTERS_CONFIG:
		return &pahu_audio_reg_cfg;
	case AFE_SLIMBUS_SLAVE_PORT_CONFIG:
		return &pahu_slimbus_slave_port_cfg;
	case AFE_AANC_VERSION:
		return &pahu_cdc_aanc_version;
	case AFE_CDC_REGISTER_PAGE_CONFIG:
		return &pahu_cdc_reg_page_cfg;
	default:
		dev_info(codec->dev, "%s: Unknown config_type 0x%x\n",
			__func__, config_type);
		return NULL;
	}
}
EXPORT_SYMBOL(pahu_get_afe_config);

static void pahu_vote_svs(struct pahu_priv *pahu, bool vote)
{
	struct wcd9xxx *wcd9xxx;

	wcd9xxx = pahu->wcd9xxx;

	mutex_lock(&pahu->svs_mutex);
	if (vote) {
		pahu->svs_ref_cnt++;
		if (pahu->svs_ref_cnt == 1)
			regmap_update_bits(wcd9xxx->regmap,
					   WCD9360_CPE_SS_PWR_SYS_PSTATE_CTL_0,
					   0x01, 0x01);
	} else {
		/* Do not decrement ref count if it is already 0 */
		if (pahu->svs_ref_cnt == 0)
			goto done;

		pahu->svs_ref_cnt--;
		if (pahu->svs_ref_cnt == 0)
			regmap_update_bits(wcd9xxx->regmap,
					   WCD9360_CPE_SS_PWR_SYS_PSTATE_CTL_0,
					   0x01, 0x00);
	}
done:
	dev_dbg(pahu->dev, "%s: vote = %s, updated ref cnt = %u\n", __func__,
		vote ? "vote" : "Unvote", pahu->svs_ref_cnt);
	mutex_unlock(&pahu->svs_mutex);
}

static int pahu_get_anc_slot(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = pahu->anc_slot;
	return 0;
}

static int pahu_put_anc_slot(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	pahu->anc_slot = ucontrol->value.integer.value[0];
	return 0;
}

static int pahu_get_anc_func(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = (pahu->anc_func == true ? 1 : 0);
	return 0;
}

static int pahu_put_anc_func(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	mutex_lock(&pahu->codec_mutex);
	pahu->anc_func = (!ucontrol->value.integer.value[0] ? false : true);
	dev_dbg(codec->dev, "%s: anc_func %x", __func__, pahu->anc_func);

	if (pahu->anc_func == true) {
		snd_soc_dapm_enable_pin(dapm, "ANC EAR PA");
		snd_soc_dapm_enable_pin(dapm, "ANC EAR");
		snd_soc_dapm_enable_pin(dapm, "ANC SPK1 PA");
		snd_soc_dapm_disable_pin(dapm, "EAR PA");
		snd_soc_dapm_disable_pin(dapm, "EAR");
	} else {
		snd_soc_dapm_disable_pin(dapm, "ANC EAR PA");
		snd_soc_dapm_disable_pin(dapm, "ANC EAR");
		snd_soc_dapm_disable_pin(dapm, "ANC SPK1 PA");
		snd_soc_dapm_enable_pin(dapm, "EAR PA");
		snd_soc_dapm_enable_pin(dapm, "EAR");
	}
	mutex_unlock(&pahu->codec_mutex);

	snd_soc_dapm_sync(dapm);
	return 0;
}

static int pahu_codec_enable_anc(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	const char *filename;
	const struct firmware *fw;
	int i;
	int ret = 0;
	int num_anc_slots;
	struct wcd9xxx_anc_header *anc_head;
	struct firmware_cal *hwdep_cal = NULL;
	u32 anc_writes_size = 0;
	int anc_size_remaining;
	u32 *anc_ptr;
	u16 reg;
	u8 mask, val;
	size_t cal_size;
	const void *data;

	if (!pahu->anc_func)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		hwdep_cal = wcdcal_get_fw_cal(pahu->fw_data, WCD9XXX_ANC_CAL);
		if (hwdep_cal) {
			data = hwdep_cal->data;
			cal_size = hwdep_cal->size;
			dev_dbg(codec->dev, "%s: using hwdep calibration, cal_size %zd",
				__func__, cal_size);
		} else {
			filename = "wcd9360/WCD9360_anc.bin";
			ret = request_firmware(&fw, filename, codec->dev);
			if (ret < 0) {
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

		if (pahu->anc_slot >= num_anc_slots) {
			dev_err(codec->dev, "%s: Invalid ANC slot selected\n",
				__func__);
			ret = -EINVAL;
			goto err;
		}
		for (i = 0; i < num_anc_slots; i++) {
			if (anc_size_remaining < WCD9360_PACKED_REG_SIZE) {
				dev_err(codec->dev, "%s: Invalid register format\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}
			anc_writes_size = (u32)(*anc_ptr);
			anc_size_remaining -= sizeof(u32);
			anc_ptr += 1;

			if ((anc_writes_size * WCD9360_PACKED_REG_SIZE) >
			    anc_size_remaining) {
				dev_err(codec->dev, "%s: Invalid register format\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}

			if (pahu->anc_slot == i)
				break;

			anc_size_remaining -= (anc_writes_size *
				WCD9360_PACKED_REG_SIZE);
			anc_ptr += anc_writes_size;
		}
		if (i == num_anc_slots) {
			dev_err(codec->dev, "%s: Selected ANC slot not present\n",
				__func__);
			ret = -EINVAL;
			goto err;
		}

		for (i = 0; i < anc_writes_size; i++) {
			WCD9360_CODEC_UNPACK_ENTRY(anc_ptr[i], reg, mask, val);
			snd_soc_write(codec, reg, (val & mask));
		}

		if (!hwdep_cal)
			release_firmware(fw);
		break;

	case SND_SOC_DAPM_POST_PMU:
		break;

	case SND_SOC_DAPM_POST_PMD:
		if (!strcmp(w->name, "ANC EAR PA") ||
		    !strcmp(w->name, "ANC SPK1 PA")) {
			snd_soc_update_bits(codec, WCD9360_CDC_ANC0_MODE_1_CTL,
					    0x30, 0x00);
			msleep(50);
			snd_soc_update_bits(codec, WCD9360_CDC_ANC0_MODE_1_CTL,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD9360_CDC_ANC0_CLK_RESET_CTL,
					    0x38, 0x38);
			snd_soc_update_bits(codec,
					    WCD9360_CDC_ANC0_CLK_RESET_CTL,
					    0x07, 0x00);
			snd_soc_update_bits(codec,
					    WCD9360_CDC_ANC0_CLK_RESET_CTL,
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

static int pahu_get_clkmode(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu_p = snd_soc_codec_get_drvdata(codec);

	if (test_bit(CLK_MODE, &pahu_p->status_mask))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;

	dev_dbg(codec->dev, "%s: is_low_power_clock: %s\n", __func__,
		test_bit(CLK_MODE, &pahu_p->status_mask) ? "true" : "false");

	return 0;
}

static int pahu_put_clkmode(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu_p = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.enumerated.item[0])
		set_bit(CLK_MODE, &pahu_p->status_mask);
	else
		clear_bit(CLK_MODE, &pahu_p->status_mask);

	dev_dbg(codec->dev, "%s: is_low_power_clock: %s\n", __func__,
		test_bit(CLK_MODE, &pahu_p->status_mask) ? "true" : "false");

	return 0;
}

static int pahu_vi_feed_mixer_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct pahu_priv *pahu_p = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = pahu_p->vi_feed_value;

	return 0;
}

static int pahu_vi_feed_mixer_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct pahu_priv *pahu_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 port_id = mixer->shift;
	u32 enable = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: enable: %d, port_id:%d, dai_id: %d\n",
		__func__, enable, port_id, dai_id);

	pahu_p->vi_feed_value = ucontrol->value.integer.value[0];

	mutex_lock(&pahu_p->codec_mutex);
	if (enable) {
		if (port_id == WCD9360_TX14 && !test_bit(VI_SENSE_1,
						&pahu_p->status_mask)) {
			list_add_tail(&core->tx_chs[WCD9360_TX14].list,
					&pahu_p->dai[dai_id].wcd9xxx_ch_list);
			set_bit(VI_SENSE_1, &pahu_p->status_mask);
		}
		if (port_id == WCD9360_TX15 && !test_bit(VI_SENSE_2,
						&pahu_p->status_mask)) {
			list_add_tail(&core->tx_chs[WCD9360_TX15].list,
					&pahu_p->dai[dai_id].wcd9xxx_ch_list);
			set_bit(VI_SENSE_2, &pahu_p->status_mask);
		}
	} else {
		if (port_id == WCD9360_TX14 && test_bit(VI_SENSE_1,
					&pahu_p->status_mask)) {
			list_del_init(&core->tx_chs[WCD9360_TX14].list);
			clear_bit(VI_SENSE_1, &pahu_p->status_mask);
		}
		if (port_id == WCD9360_TX15 && test_bit(VI_SENSE_2,
					&pahu_p->status_mask)) {
			list_del_init(&core->tx_chs[WCD9360_TX15].list);
			clear_bit(VI_SENSE_2, &pahu_p->status_mask);
		}
	}
	mutex_unlock(&pahu_p->codec_mutex);
	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, enable, NULL);

	return 0;
}

static int slim_tx_mixer_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct pahu_priv *pahu_p = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = pahu_p->tx_port_value;
	return 0;
}

static int slim_tx_mixer_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct pahu_priv *pahu_p = snd_soc_codec_get_drvdata(codec);
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
		widget->name, ucontrol->id.name, pahu_p->tx_port_value,
		widget->shift, ucontrol->value.integer.value[0]);

	mutex_lock(&pahu_p->codec_mutex);
	if (dai_id >= ARRAY_SIZE(vport_slim_check_table)) {
		dev_err(codec->dev, "%s: dai_id: %d, out of bounds\n",
			__func__, dai_id);
		mutex_unlock(&pahu_p->codec_mutex);
		return -EINVAL;
	}
	vtable = vport_slim_check_table[dai_id];

	switch (dai_id) {
	case AIF1_CAP:
	case AIF2_CAP:
	case AIF3_CAP:
		/* only add to the list if value not set */
		if (enable && !(pahu_p->tx_port_value & 1 << port_id)) {
			if (wcd9xxx_tx_vport_validation(vtable, port_id,
			    pahu_p->dai, NUM_CODEC_DAIS)) {
				dev_dbg(codec->dev, "%s: TX%u is used by other virtual port\n",
					__func__, port_id);
				mutex_unlock(&pahu_p->codec_mutex);
				return 0;
			}
			pahu_p->tx_port_value |= 1 << port_id;
			list_add_tail(&core->tx_chs[port_id].list,
				      &pahu_p->dai[dai_id].wcd9xxx_ch_list);
		} else if (!enable && (pahu_p->tx_port_value &
			   1 << port_id)) {
			pahu_p->tx_port_value &= ~(1 << port_id);
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
			mutex_unlock(&pahu_p->codec_mutex);
			return 0;
		}
		break;
	case AIF4_MAD_TX:
		break;
	default:
		dev_err(codec->dev, "Unknown AIF %d\n", dai_id);
		mutex_unlock(&pahu_p->codec_mutex);
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: name %s sname %s updated value %u shift %d\n",
		__func__, widget->name, widget->sname, pahu_p->tx_port_value,
		widget->shift);

	mutex_unlock(&pahu_p->codec_mutex);
	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, enable, update);

	return 0;
}

static int slim_rx_mux_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct pahu_priv *pahu_p = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] =
				pahu_p->rx_port_value[widget->shift];
	return 0;
}

static int slim_rx_mux_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct pahu_priv *pahu_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_dapm_update *update = NULL;
	unsigned int rx_port_value;
	u32 port_id = widget->shift;

	pahu_p->rx_port_value[port_id] = ucontrol->value.enumerated.item[0];
	rx_port_value = pahu_p->rx_port_value[port_id];

	mutex_lock(&pahu_p->codec_mutex);
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
			WCD9360_RX_PORT_START_NUMBER,
			&pahu_p->dai[AIF1_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &pahu_p->dai[AIF1_PB].wcd9xxx_ch_list);
		break;
	case 2:
		if (wcd9xxx_rx_vport_validation(port_id +
			WCD9360_RX_PORT_START_NUMBER,
			&pahu_p->dai[AIF2_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &pahu_p->dai[AIF2_PB].wcd9xxx_ch_list);
		break;
	case 3:
		if (wcd9xxx_rx_vport_validation(port_id +
			WCD9360_RX_PORT_START_NUMBER,
			&pahu_p->dai[AIF3_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &pahu_p->dai[AIF3_PB].wcd9xxx_ch_list);
		break;
	case 4:
		if (wcd9xxx_rx_vport_validation(port_id +
			WCD9360_RX_PORT_START_NUMBER,
			&pahu_p->dai[AIF4_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &pahu_p->dai[AIF4_PB].wcd9xxx_ch_list);
		break;
	default:
		dev_err(codec->dev, "Unknown AIF %d\n", rx_port_value);
		goto err;
	}
rtn:
	mutex_unlock(&pahu_p->codec_mutex);
	snd_soc_dapm_mux_update_power(widget->dapm, kcontrol,
				      rx_port_value, e, update);

	return 0;
err:
	mutex_unlock(&pahu_p->codec_mutex);
	return -EINVAL;
}

static void pahu_codec_enable_slim_port_intr(
					struct wcd9xxx_codec_dai_data *dai,
					struct snd_soc_codec *codec)
{
	struct wcd9xxx_ch *ch;
	int port_num = 0;
	unsigned short reg = 0;
	u8 val = 0;
	struct pahu_priv *pahu_p;

	if (!dai || !codec) {
		pr_err("%s: Invalid params\n", __func__);
		return;
	}

	pahu_p = snd_soc_codec_get_drvdata(codec);
	list_for_each_entry(ch, &dai->wcd9xxx_ch_list, list) {
		if (ch->port >= WCD9360_RX_PORT_START_NUMBER) {
			port_num = ch->port - WCD9360_RX_PORT_START_NUMBER;
			reg = WCD9360_SLIM_PGD_PORT_INT_RX_EN0 + (port_num / 8);
			val = wcd9xxx_interface_reg_read(pahu_p->wcd9xxx,
				reg);
			if (!(val & BYTE_BIT_MASK(port_num))) {
				val |= BYTE_BIT_MASK(port_num);
				wcd9xxx_interface_reg_write(
					pahu_p->wcd9xxx, reg, val);
				val = wcd9xxx_interface_reg_read(
					pahu_p->wcd9xxx, reg);
			}
		} else {
			port_num = ch->port;
			reg = WCD9360_SLIM_PGD_PORT_INT_TX_EN0 + (port_num / 8);
			val = wcd9xxx_interface_reg_read(pahu_p->wcd9xxx,
				reg);
			if (!(val & BYTE_BIT_MASK(port_num))) {
				val |= BYTE_BIT_MASK(port_num);
				wcd9xxx_interface_reg_write(pahu_p->wcd9xxx,
					reg, val);
				val = wcd9xxx_interface_reg_read(
					pahu_p->wcd9xxx, reg);
			}
		}
	}
}

static int pahu_codec_enable_slim_chmask(struct wcd9xxx_codec_dai_data *dai,
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
						WCD9360_SLIM_CLOSE_TIMEOUT));
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

static int pahu_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct wcd9xxx *core;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu_p = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	struct wcd9xxx_codec_dai_data *dai;

	core = dev_get_drvdata(codec->dev->parent);

	dev_dbg(codec->dev, "%s: event called! codec name %s num_dai %d\n"
		"stream name %s event %d\n",
		__func__, codec->component.name,
		codec->component.num_dai, w->sname, event);

	dai = &pahu_p->dai[w->shift];
	dev_dbg(codec->dev, "%s: w->name %s w->shift %d event %d\n",
		 __func__, w->name, w->shift, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dai->bus_down_in_recovery = false;
		pahu_codec_enable_slim_port_intr(dai, codec);
		(void) pahu_codec_enable_slim_chmask(dai, true);
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
			ret = pahu_codec_enable_slim_chmask(dai, false);
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

static int pahu_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_codec_dai_data *dai;
	struct wcd9xxx *core;
	int ret = 0;

	dev_dbg(codec->dev,
		"%s: w->name %s, w->shift = %d, num_dai %d stream name %s\n",
		__func__, w->name, w->shift,
		codec->component.num_dai, w->sname);

	dai = &pahu_p->dai[w->shift];
	core = dev_get_drvdata(codec->dev->parent);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dai->bus_down_in_recovery = false;
		pahu_codec_enable_slim_port_intr(dai, codec);
		(void) pahu_codec_enable_slim_chmask(dai, true);
		ret = wcd9xxx_cfg_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
					      dai->rate, dai->bit_width,
					      &dai->grph);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wcd9xxx_close_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
						dai->grph);
		if (!dai->bus_down_in_recovery)
			ret = pahu_codec_enable_slim_chmask(dai, false);
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

static int pahu_codec_enable_slimvi_feedback(struct snd_soc_dapm_widget *w,
					      struct snd_kcontrol *kcontrol,
					      int event)
{
	struct wcd9xxx *core = NULL;
	struct snd_soc_codec *codec = NULL;
	struct pahu_priv *pahu_p = NULL;
	int ret = 0;
	struct wcd9xxx_codec_dai_data *dai = NULL;

	codec = snd_soc_dapm_to_codec(w->dapm);
	pahu_p = snd_soc_codec_get_drvdata(codec);
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
	dai = &pahu_p->dai[w->shift];
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (test_bit(VI_SENSE_1, &pahu_p->status_mask)) {
			dev_dbg(codec->dev, "%s: spkr1 enabled\n", __func__);
			/* Enable V&I sensing */
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x20);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX9_SPKR_PROT_PATH_CTL, 0x0F, 0x00);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX10_SPKR_PROT_PATH_CTL, 0x0F,
				0x00);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX9_SPKR_PROT_PATH_CTL, 0x10, 0x10);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX10_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x00);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
		}
		if (test_bit(VI_SENSE_2, &pahu_p->status_mask)) {
			pr_debug("%s: spkr2 enabled\n", __func__);
			/* Enable V&I sensing */
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX11_SPKR_PROT_PATH_CTL, 0x0F,
				0x00);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX12_SPKR_PROT_PATH_CTL, 0x0F,
				0x00);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX11_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX12_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
		}
		dai->bus_down_in_recovery = false;
		pahu_codec_enable_slim_port_intr(dai, codec);
		(void) pahu_codec_enable_slim_chmask(dai, true);
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
			ret = pahu_codec_enable_slim_chmask(dai, false);
		if (ret < 0) {
			ret = wcd9xxx_disconnect_port(core,
				&dai->wcd9xxx_ch_list,
				dai->grph);
			dev_dbg(codec->dev, "%s: Disconnect TX port, ret = %d\n",
				__func__, ret);
		}
		if (test_bit(VI_SENSE_1, &pahu_p->status_mask)) {
			/* Disable V&I sensing */
			dev_dbg(codec->dev, "%s: spkr1 disabled\n", __func__);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x20);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX9_SPKR_PROT_PATH_CTL, 0x10, 0x00);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX10_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
		}
		if (test_bit(VI_SENSE_2, &pahu_p->status_mask)) {
			/* Disable V&I sensing */
			dev_dbg(codec->dev, "%s: spkr2 disabled\n", __func__);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX11_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
			snd_soc_update_bits(codec,
				WCD9360_CDC_TX12_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
		}
		break;
	}
done:
	return ret;
}

static void pahu_codec_enable_i2s(struct snd_soc_codec *codec, bool enable)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	if (enable) {
		if (++pahu->i2s_ref_cnt == 1)
			snd_soc_update_bits(codec, WCD9360_DATA_HUB_I2S_1_CTL,
					    0x01, 0x01);
	} else {
		if (--pahu->i2s_ref_cnt == 0)
			snd_soc_update_bits(codec, WCD9360_DATA_HUB_I2S_1_CTL,
					    0x01, 0x00);
	}
}

static int pahu_i2s_aif_rx_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);
	switch(event) {
	case SND_SOC_DAPM_PRE_PMU:
		pahu_cdc_mclk_enable(codec, true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		pahu_codec_enable_i2s(codec, true);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		pahu_codec_enable_i2s(codec, false);
		break;
	case SND_SOC_DAPM_POST_PMD:
		pahu_cdc_mclk_enable(codec, false);
		break;
	}

	return 0;
}

static int pahu_i2s_aif_tx_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);
	switch(event) {
	case SND_SOC_DAPM_PRE_PMU:
		pahu_cdc_mclk_enable(codec, true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		pahu_codec_enable_i2s(codec, true);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		pahu_codec_enable_i2s(codec, false);
		break;
	case SND_SOC_DAPM_POST_PMD:
		pahu_cdc_mclk_enable(codec, false);
		break;
	}

	return 0;
}

static int pahu_codec_enable_ldo_rxtx(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_pdata *pdata = dev_get_platdata(codec->dev->parent);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		pahu->ldo_rxtx_cnt++;
		if (pahu->ldo_rxtx_cnt == 1) {
			/* Enable VDD_LDO_RxTx regulator */
			msm_cdc_enable_ondemand_supply(pahu->wcd9xxx->dev,
						pahu->wcd9xxx->supplies,
						pdata->regulator,
						pdata->num_supplies,
						WCD9360_LDO_RXTX_SUPPLY_NAME);

			snd_soc_update_bits(codec, WCD9360_LDORXTX_LDORXTX,
						0x80, 0x80);
			/*
			 * 200us sleep is required after LDO_RXTX is enabled as per
			 * HW requirement
			 */
			usleep_range(200, 250);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		pahu->ldo_rxtx_cnt--;
		if (pahu->ldo_rxtx_cnt < 0)
			pahu->ldo_rxtx_cnt = 0;

		if (!pahu->ldo_rxtx_cnt) {
			snd_soc_update_bits(codec, WCD9360_LDORXTX_LDORXTX,
						0x80, 0x00);
			/* Disable VDD_LDO_RxTx regulator */
			msm_cdc_disable_ondemand_supply(pahu->wcd9xxx->dev,
						pahu->wcd9xxx->supplies,
						pdata->regulator,
						pdata->num_supplies,
						WCD9360_LDO_RXTX_SUPPLY_NAME);
		}
		break;
	};
	dev_dbg(codec->dev, "%s: Current LDO RXTX user count: %d\n", __func__,
		pahu->ldo_rxtx_cnt);

	return 0;
}

static void pahu_spk_anc_update_callback(struct work_struct *work)
{
	struct spk_anc_work *spk_anc_dwork;
	struct pahu_priv *pahu;
	struct delayed_work *delayed_work;
	struct snd_soc_codec *codec;

	delayed_work = to_delayed_work(work);
	spk_anc_dwork = container_of(delayed_work, struct spk_anc_work, dwork);
	pahu = spk_anc_dwork->pahu;
	codec = pahu->codec;

	snd_soc_update_bits(codec, WCD9360_CDC_RX7_RX_PATH_CFG0, 0x10, 0x10);
}

static int pahu_codec_enable_spkr_anc(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	if (!pahu->anc_func)
		return 0;

	dev_dbg(codec->dev, "%s: w: %s event: %d anc: %d\n", __func__,
		w->name, event, pahu->anc_func);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = pahu_codec_enable_anc(w, kcontrol, event);
		schedule_delayed_work(&pahu->spk_anc_dwork.dwork,
				      msecs_to_jiffies(spk_anc_en_delay));
		break;
	case SND_SOC_DAPM_POST_PMD:
		cancel_delayed_work_sync(&pahu->spk_anc_dwork.dwork);
		snd_soc_update_bits(codec, WCD9360_CDC_RX7_RX_PATH_CFG0,
				    0x10, 0x00);
		ret = pahu_codec_enable_anc(w, kcontrol, event);
		break;
	}
	return ret;
}

static int pahu_codec_enable_aux_pa(struct snd_soc_dapm_widget *w,
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
		snd_soc_update_bits(codec, WCD9360_CDC_RX9_RX_PATH_CTL,
				    0x10, 0x00);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_read(codec, WCD9360_CDC_RX9_RX_PATH_MIX_CTL)) &
		     0x10)
			snd_soc_update_bits(codec,
					    WCD9360_CDC_RX9_RX_PATH_MIX_CTL,
					    0x10, 0x00);
		break;
	default:
		break;
	};

	return ret;
}

static int pahu_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
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
		snd_soc_update_bits(codec, WCD9360_CDC_RX0_RX_PATH_CTL,
				    0x10, 0x00);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_read(codec, WCD9360_CDC_RX0_RX_PATH_MIX_CTL)) &
		     0x10)
			snd_soc_update_bits(codec,
					    WCD9360_CDC_RX0_RX_PATH_MIX_CTL,
					    0x10, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * 5ms sleep is required after PA is disabled as per
		 * HW requirement
		 */
		usleep_range(5000, 5500);

		if (!(strcmp(w->name, "ANC EAR PA"))) {
			ret = pahu_codec_enable_anc(w, kcontrol, event);
			snd_soc_update_bits(codec, WCD9360_CDC_RX0_RX_PATH_CFG0,
					    0x10, 0x00);
		}
		break;
	};

	return ret;
}

static int pahu_codec_ear_dac_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (pahu->anc_func) {
			ret = pahu_codec_enable_anc(w, kcontrol, event);
			snd_soc_update_bits(codec, WCD9360_CDC_RX0_RX_PATH_CFG0,
					    0x10, 0x10);
		}
		break;
	default:
		break;
	};

	return ret;
}

static int pahu_codec_spk_boost_event(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 boost_path_ctl, boost_path_cfg1;
	u16 reg, reg_mix;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	if (!strcmp(w->name, "RX INT7 CHAIN")) {
		boost_path_ctl = WCD9360_CDC_BOOST0_BOOST_PATH_CTL;
		boost_path_cfg1 = WCD9360_CDC_RX7_RX_PATH_CFG1;
		reg = WCD9360_CDC_RX7_RX_PATH_CTL;
		reg_mix = WCD9360_CDC_RX7_RX_PATH_MIX_CTL;
	} else if (!strcmp(w->name, "RX INT8 CHAIN")) {
		boost_path_ctl = WCD9360_CDC_BOOST1_BOOST_PATH_CTL;
		boost_path_cfg1 = WCD9360_CDC_RX8_RX_PATH_CFG1;
		reg = WCD9360_CDC_RX8_RX_PATH_CTL;
		reg_mix = WCD9360_CDC_RX8_RX_PATH_MIX_CTL;
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

static int __pahu_codec_enable_swr(struct snd_soc_dapm_widget *w, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu;
	int ch_cnt = 0;

	pahu = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (((strnstr(w->name, "INT7_", sizeof("RX INT7_"))) ||
			(strnstr(w->name, "INT7 MIX2",
						sizeof("RX INT7 MIX2")))))
			pahu->swr.rx_7_count++;
		if ((strnstr(w->name, "INT8_", sizeof("RX INT8_"))) &&
		    !pahu->swr.rx_8_count)
			pahu->swr.rx_8_count++;
		ch_cnt = !!(pahu->swr.rx_7_count) + pahu->swr.rx_8_count;

		swrm_wcd_notify(pahu->swr.ctrl_data[0].swr_pdev,
				SWR_DEVICE_UP, NULL);
		swrm_wcd_notify(pahu->swr.ctrl_data[0].swr_pdev,
				SWR_SET_NUM_RX_CH, &ch_cnt);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if ((strnstr(w->name, "INT7_", sizeof("RX INT7_")))  ||
			(strnstr(w->name, "INT7 MIX2",
			sizeof("RX INT7 MIX2"))))
			pahu->swr.rx_7_count--;
		if ((strnstr(w->name, "INT8_", sizeof("RX INT8_"))) &&
		    pahu->swr.rx_8_count)
			pahu->swr.rx_8_count--;
		ch_cnt = !!(pahu->swr.rx_7_count) + pahu->swr.rx_8_count;

		swrm_wcd_notify(pahu->swr.ctrl_data[0].swr_pdev,
				SWR_SET_NUM_RX_CH, &ch_cnt);

		break;
	}
	dev_dbg(pahu->dev, "%s: %s: current swr ch cnt: %d\n",
		__func__, w->name, ch_cnt);

	return 0;
}

static int pahu_codec_enable_swr(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	return __pahu_codec_enable_swr(w, event);
}

static int pahu_codec_config_mad(struct snd_soc_codec *codec)
{
	int ret = 0;
	int idx;
	const struct firmware *fw;
	struct firmware_cal *hwdep_cal = NULL;
	struct wcd_mad_audio_cal *mad_cal = NULL;
	const void *data;
	const char *filename = WCD9360_MAD_AUDIO_FIRMWARE_PATH;
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	size_t cal_size;

	hwdep_cal = wcdcal_get_fw_cal(pahu->fw_data, WCD9XXX_MAD_CAL);
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

	snd_soc_write(codec, WCD9360_SOC_MAD_MAIN_CTL_2,
		      mad_cal->microphone_info.cycle_time);
	snd_soc_update_bits(codec, WCD9360_SOC_MAD_MAIN_CTL_1, 0xFF << 3,
			    ((uint16_t)mad_cal->microphone_info.settle_time)
			    << 3);

	/* Audio */
	snd_soc_write(codec, WCD9360_SOC_MAD_AUDIO_CTL_8,
		      mad_cal->audio_info.rms_omit_samples);
	snd_soc_update_bits(codec, WCD9360_SOC_MAD_AUDIO_CTL_1,
			    0x07 << 4, mad_cal->audio_info.rms_comp_time << 4);
	snd_soc_update_bits(codec, WCD9360_SOC_MAD_AUDIO_CTL_2, 0x03 << 2,
			    mad_cal->audio_info.detection_mechanism << 2);
	snd_soc_write(codec, WCD9360_SOC_MAD_AUDIO_CTL_7,
		      mad_cal->audio_info.rms_diff_threshold & 0x3F);
	snd_soc_write(codec, WCD9360_SOC_MAD_AUDIO_CTL_5,
		      mad_cal->audio_info.rms_threshold_lsb);
	snd_soc_write(codec, WCD9360_SOC_MAD_AUDIO_CTL_6,
		      mad_cal->audio_info.rms_threshold_msb);

	for (idx = 0; idx < ARRAY_SIZE(mad_cal->audio_info.iir_coefficients);
	     idx++) {
		snd_soc_update_bits(codec, WCD9360_SOC_MAD_AUDIO_IIR_CTL_PTR,
				    0x3F, idx);
		snd_soc_write(codec, WCD9360_SOC_MAD_AUDIO_IIR_CTL_VAL,
			      mad_cal->audio_info.iir_coefficients[idx]);
		dev_dbg(codec->dev, "%s:MAD Audio IIR Coef[%d] = 0X%x",
			__func__, idx,
			mad_cal->audio_info.iir_coefficients[idx]);
	}

	/* Beacon */
	snd_soc_write(codec, WCD9360_SOC_MAD_BEACON_CTL_8,
		      mad_cal->beacon_info.rms_omit_samples);
	snd_soc_update_bits(codec, WCD9360_SOC_MAD_BEACON_CTL_1,
			    0x07 << 4, mad_cal->beacon_info.rms_comp_time << 4);
	snd_soc_update_bits(codec, WCD9360_SOC_MAD_BEACON_CTL_2, 0x03 << 2,
			    mad_cal->beacon_info.detection_mechanism << 2);
	snd_soc_write(codec, WCD9360_SOC_MAD_BEACON_CTL_7,
		      mad_cal->beacon_info.rms_diff_threshold & 0x1F);
	snd_soc_write(codec, WCD9360_SOC_MAD_BEACON_CTL_5,
		      mad_cal->beacon_info.rms_threshold_lsb);
	snd_soc_write(codec, WCD9360_SOC_MAD_BEACON_CTL_6,
		      mad_cal->beacon_info.rms_threshold_msb);

	for (idx = 0; idx < ARRAY_SIZE(mad_cal->beacon_info.iir_coefficients);
	     idx++) {
		snd_soc_update_bits(codec, WCD9360_SOC_MAD_BEACON_IIR_CTL_PTR,
				    0x3F, idx);
		snd_soc_write(codec, WCD9360_SOC_MAD_BEACON_IIR_CTL_VAL,
			      mad_cal->beacon_info.iir_coefficients[idx]);
		dev_dbg(codec->dev, "%s:MAD Beacon IIR Coef[%d] = 0X%x",
			__func__, idx,
			mad_cal->beacon_info.iir_coefficients[idx]);
	}

	/* Ultrasound */
	snd_soc_update_bits(codec, WCD9360_SOC_MAD_ULTR_CTL_1,
			    0x07 << 4,
			    mad_cal->ultrasound_info.rms_comp_time << 4);
	snd_soc_update_bits(codec, WCD9360_SOC_MAD_ULTR_CTL_2, 0x03 << 2,
			    mad_cal->ultrasound_info.detection_mechanism << 2);
	snd_soc_write(codec, WCD9360_SOC_MAD_ULTR_CTL_7,
		      mad_cal->ultrasound_info.rms_diff_threshold & 0x1F);
	snd_soc_write(codec, WCD9360_SOC_MAD_ULTR_CTL_5,
		      mad_cal->ultrasound_info.rms_threshold_lsb);
	snd_soc_write(codec, WCD9360_SOC_MAD_ULTR_CTL_6,
		      mad_cal->ultrasound_info.rms_threshold_msb);

done:
	if (!hwdep_cal)
		release_firmware(fw);

	return ret;
}

static int __pahu_codec_enable_mad(struct snd_soc_codec *codec, bool enable)
{
	int rc = 0;

	/* Return if CPE INPUT is DEC1 */
	if (snd_soc_read(codec, WCD9360_CPE_SS_SVA_CFG) & 0x04) {
		dev_dbg(codec->dev, "%s: MAD is bypassed, skip mad %s\n",
			__func__, enable ? "enable" : "disable");
		return rc;
	}

	dev_dbg(codec->dev, "%s: enable = %s\n", __func__,
		enable ? "enable" : "disable");

	if (enable) {
		snd_soc_update_bits(codec, WCD9360_SOC_MAD_AUDIO_CTL_2,
				    0x03, 0x03);
		rc = pahu_codec_config_mad(codec);
		if (rc < 0) {
			snd_soc_update_bits(codec, WCD9360_SOC_MAD_AUDIO_CTL_2,
					    0x03, 0x00);
			goto done;
		}

		/* Turn on MAD clk */
		snd_soc_update_bits(codec, WCD9360_CPE_SS_MAD_CTL,
				    0x01, 0x01);

		/* Undo reset for MAD */
		snd_soc_update_bits(codec, WCD9360_CPE_SS_MAD_CTL,
				    0x02, 0x00);
		snd_soc_update_bits(codec, WCD9360_CODEC_RPM_CLK_MCLK_CFG,
					0x04, 0x04);
	} else {
		snd_soc_update_bits(codec, WCD9360_SOC_MAD_AUDIO_CTL_2,
				    0x03, 0x00);
		/* Reset the MAD block */
		snd_soc_update_bits(codec, WCD9360_CPE_SS_MAD_CTL,
				    0x02, 0x02);
		/* Turn off MAD clk */
		snd_soc_update_bits(codec, WCD9360_CPE_SS_MAD_CTL,
				    0x01, 0x00);
		snd_soc_update_bits(codec, WCD9360_CODEC_RPM_CLK_MCLK_CFG,
					0x04, 0x00);
	}
done:
	return rc;
}

static int pahu_codec_ape_enable_mad(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	int rc = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, WCD9360_CPE_SS_SVA_CFG, 0x40, 0x40);
		rc = __pahu_codec_enable_mad(codec, true);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, WCD9360_CPE_SS_SVA_CFG, 0x40, 0x00);
		__pahu_codec_enable_mad(codec, false);
		break;
	}

	dev_dbg(pahu->dev, "%s: event = %d\n", __func__, event);
	return rc;
}

static int pahu_codec_cpe_mad_ctl(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	int rc = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		pahu->mad_switch_cnt++;
		if (pahu->mad_switch_cnt != 1)
			goto done;

		snd_soc_update_bits(codec, WCD9360_CPE_SS_SVA_CFG, 0x20, 0x20);
		rc = __pahu_codec_enable_mad(codec, true);
		if (rc < 0) {
			pahu->mad_switch_cnt--;
			goto done;
		}

		break;
	case SND_SOC_DAPM_PRE_PMD:
		pahu->mad_switch_cnt--;
		if (pahu->mad_switch_cnt != 0)
			goto done;

		snd_soc_update_bits(codec, WCD9360_CPE_SS_SVA_CFG, 0x20, 0x00);
		__pahu_codec_enable_mad(codec, false);
		break;
	}
done:
	dev_dbg(pahu->dev, "%s: event = %d, mad_switch_cnt = %d\n",
		__func__, event, pahu->mad_switch_cnt);
	return rc;
}

static int pahu_get_asrc_mode(struct pahu_priv *pahu, int asrc,
			       u8 main_sr, u8 mix_sr)
{
	u8 asrc_output_mode;
	int asrc_mode = CONV_88P2K_TO_384K;

	if ((asrc < 0) || (asrc >= ASRC_MAX))
		return 0;

	asrc_output_mode = pahu->asrc_output_mode[asrc];

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

static int pahu_codec_wdma3_ctl(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Fix to 16KHz */
		snd_soc_update_bits(codec, WCD9360_DMA_WDMA_CTL_3,
				    0xF0, 0x10);
		/* Select mclk_1 */
		snd_soc_update_bits(codec, WCD9360_DMA_WDMA_CTL_3,
				    0x02, 0x00);
		/* Enable DMA */
		snd_soc_update_bits(codec, WCD9360_DMA_WDMA_CTL_3,
				    0x01, 0x01);
		break;

	case SND_SOC_DAPM_POST_PMD:
		/* Disable DMA */
		snd_soc_update_bits(codec, WCD9360_DMA_WDMA_CTL_3,
				    0x01, 0x00);
		break;

	};

	return 0;
}
static int pahu_codec_enable_asrc(struct snd_soc_codec *codec,
				   int asrc_in, int event)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	u16 cfg_reg, ctl_reg, clk_reg, asrc_ctl, mix_ctl_reg, paired_reg;
	int asrc, ret = 0;
	u8 main_sr, mix_sr, asrc_mode = 0;

	switch (asrc_in) {
	case ASRC_IN_SPKR1:
		cfg_reg = WCD9360_CDC_RX7_RX_PATH_CFG0;
		ctl_reg = WCD9360_CDC_RX7_RX_PATH_CTL;
		clk_reg = WCD9360_MIXING_ASRC2_CLK_RST_CTL;
		paired_reg = WCD9360_MIXING_ASRC2_CLK_RST_CTL;
		asrc_ctl = WCD9360_MIXING_ASRC2_CTL1;
		asrc = ASRC2;
		break;
	case ASRC_IN_SPKR2:
		cfg_reg = WCD9360_CDC_RX8_RX_PATH_CFG0;
		ctl_reg = WCD9360_CDC_RX8_RX_PATH_CTL;
		clk_reg = WCD9360_MIXING_ASRC3_CLK_RST_CTL;
		paired_reg = WCD9360_MIXING_ASRC3_CLK_RST_CTL;
		asrc_ctl = WCD9360_MIXING_ASRC3_CTL1;
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
		if (pahu->asrc_users[asrc] == 0) {
			if ((snd_soc_read(codec, clk_reg) & 0x02) ||
			    (snd_soc_read(codec, paired_reg) & 0x02)) {
					snd_soc_update_bits(codec, clk_reg,
							    0x02, 0x00);
					snd_soc_update_bits(codec, paired_reg,
							    0x02, 0x00);
			}
			snd_soc_update_bits(codec, cfg_reg, 0x80, 0x80);
			snd_soc_update_bits(codec, clk_reg, 0x01, 0x01);
			main_sr = snd_soc_read(codec, ctl_reg) & 0x0F;
			mix_ctl_reg = ctl_reg + 5;
			mix_sr = snd_soc_read(codec, mix_ctl_reg) & 0x0F;
			asrc_mode = pahu_get_asrc_mode(pahu, asrc,
							main_sr, mix_sr);
			dev_dbg(codec->dev, "%s: main_sr:%d mix_sr:%d asrc_mode %d\n",
				__func__, main_sr, mix_sr, asrc_mode);
			snd_soc_update_bits(codec, asrc_ctl, 0x07, asrc_mode);
		}
		pahu->asrc_users[asrc]++;
		break;
	case SND_SOC_DAPM_POST_PMD:
		pahu->asrc_users[asrc]--;
		if (pahu->asrc_users[asrc] <= 0) {
			pahu->asrc_users[asrc] = 0;
			snd_soc_update_bits(codec, asrc_ctl, 0x07, 0x00);
			snd_soc_update_bits(codec, cfg_reg, 0x80, 0x00);
			snd_soc_update_bits(codec, clk_reg, 0x03, 0x02);
		}
		break;
	};

	dev_dbg(codec->dev, "%s: ASRC%d, users: %d\n",
		__func__, asrc, pahu->asrc_users[asrc]);

done:
	return ret;
}

static int pahu_codec_enable_asrc_resampler(struct snd_soc_dapm_widget *w,
					     struct snd_kcontrol *kcontrol,
					     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;
	u8 cfg, asrc_in;

	cfg = snd_soc_read(codec, WCD9360_CDC_RX_INP_MUX_SPLINE_ASRC_CFG0);
	if (!(cfg & 0xFF)) {
		dev_err(codec->dev, "%s: ASRC%u input not selected\n",
			__func__, w->shift);
		return -EINVAL;
	}

	switch (w->shift) {
	case ASRC2:
		asrc_in = ((cfg & 0x30) == 0x20) ? ASRC_IN_SPKR1 : ASRC_INVALID;
		ret = pahu_codec_enable_asrc(codec, asrc_in, event);
		break;
	case ASRC3:
		asrc_in = ((cfg & 0xC0) == 0x80) ? ASRC_IN_SPKR2 : ASRC_INVALID;
		ret = pahu_codec_enable_asrc(codec, asrc_in, event);
		break;
	default:
		dev_err(codec->dev, "%s: Invalid asrc:%u\n", __func__,
			w->shift);
		ret = -EINVAL;
		break;
	};

	return ret;
}

static int pahu_enable_native_supply(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (++pahu->native_clk_users == 1) {
			snd_soc_update_bits(codec, WCD9360_CLK_SYS_PLL_ENABLES,
					    0x01, 0x01);
			usleep_range(100, 120);
			snd_soc_update_bits(codec, WCD9360_CLK_SYS_MCLK2_PRG1,
					    0x06, 0x02);
			snd_soc_update_bits(codec, WCD9360_CLK_SYS_MCLK2_PRG1,
					    0x01, 0x01);
			snd_soc_update_bits(codec, WCD9360_CODEC_RPM_CLK_GATE,
					    0x04, 0x00);
			/* Add sleep as per HW register sequence */
			usleep_range(30, 50);
			snd_soc_update_bits(codec,
					WCD9360_CDC_CLK_RST_CTRL_MCLK_CONTROL,
					0x02, 0x02);
			snd_soc_update_bits(codec,
					WCD9360_CDC_CLK_RST_CTRL_FS_CNT_CONTROL,
					0x10, 0x10);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (pahu->native_clk_users &&
		    (--pahu->native_clk_users == 0)) {
			snd_soc_update_bits(codec,
					WCD9360_CDC_CLK_RST_CTRL_FS_CNT_CONTROL,
					0x10, 0x00);
			snd_soc_update_bits(codec,
					WCD9360_CDC_CLK_RST_CTRL_MCLK_CONTROL,
					0x02, 0x00);
			snd_soc_update_bits(codec, WCD9360_CODEC_RPM_CLK_GATE,
					    0x04, 0x04);
			snd_soc_update_bits(codec, WCD9360_CLK_SYS_MCLK2_PRG1,
					    0x01, 0x00);
			snd_soc_update_bits(codec, WCD9360_CLK_SYS_MCLK2_PRG1,
					    0x06, 0x00);
			snd_soc_update_bits(codec, WCD9360_CLK_SYS_PLL_ENABLES,
					    0x01, 0x00);
		}
		break;
	}

	dev_dbg(codec->dev, "%s: native_clk_users: %d, event: %d\n",
		__func__, pahu->native_clk_users, event);

	return 0;
}

static int pahu_codec_config_ear_spkr_gain(struct snd_soc_codec *codec,
					    int event, int gain_reg)
{
	int comp_gain_offset, val;
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	switch (pahu->swr.spkr_mode) {
	/* Compander gain in SPKR_MODE1 case is 12 dB */
	case WCD9360_SPKR_MODE_1:
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
		if (pahu->comp_enabled[COMPANDER_7] &&
		    (gain_reg == WCD9360_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD9360_CDC_RX7_RX_VOL_MIX_CTL) &&
		    (pahu->ear_spkr_gain != 0)) {
			/* For example, val is -8(-12+5-1) for 4dB of gain */
			val = comp_gain_offset + pahu->ear_spkr_gain - 1;
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
		if (pahu->comp_enabled[COMPANDER_7] &&
		    (gain_reg == WCD9360_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD9360_CDC_RX7_RX_VOL_MIX_CTL) &&
		    (pahu->ear_spkr_gain != 0)) {
			snd_soc_write(codec, gain_reg, 0x0);

			dev_dbg(codec->dev, "%s: Reset RX7 Volume to 0 dB\n",
				__func__);
		}
		break;
	}

	return 0;
}

static int pahu_config_compander(struct snd_soc_codec *codec, int interp_n,
				  int event)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	int comp;
	u16 comp_ctl0_reg, rx_path_cfg0_reg;

	/* HPH, LO are not valid and AUX does not have compander */
	if (((interp_n >= INTERP_HPHL_NA) && (interp_n <= INTERP_LO4_NA)) ||
		(interp_n == INTERP_AUX))
		return 0;

	comp = interp_n;
	dev_dbg(codec->dev, "%s: event %d compander %d, enabled %d\n",
		__func__, event, comp, pahu->comp_enabled[comp]);

	if (!pahu->comp_enabled[comp])
		return 0;

	comp_ctl0_reg = WCD9360_CDC_COMPANDER0_CTL0 + (comp * 8);
	rx_path_cfg0_reg = WCD9360_CDC_RX0_RX_PATH_CFG0 + (comp * 20);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Enable Compander Clock */
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x01, 0x01);
		/* Soft reset */
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x02);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x00);
		/* Compander enable */
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

/**
 * pahu_codec_enable_interp_clk - Enable main path Interpolator
 * clock.
 *
 * @codec:    Codec instance
 * @event:    Indicates speaker path gain offset value
 * @intp_idx: Interpolator index
 * Returns number of main clock users
 */
int pahu_codec_enable_interp_clk(struct snd_soc_codec *codec,
				  int event, int interp_idx)
{
	struct pahu_priv *pahu;
	u16 main_reg;

	if (!codec) {
		pr_err("%s: codec is NULL\n", __func__);
		return -EINVAL;
	}

	pahu  = snd_soc_codec_get_drvdata(codec);

	main_reg = WCD9360_CDC_RX0_RX_PATH_CTL +
		(interp_idx * WCD9360_RX_PATH_CTL_OFFSET);

	if (interp_idx == INTERP_AUX)
		main_reg = WCD9360_CDC_RX9_RX_PATH_CTL;

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (pahu->main_clk_users[interp_idx] == 0) {
			/* Main path PGA mute enable */
			snd_soc_update_bits(codec, main_reg, 0x10, 0x10);
			/* Clk enable */
			snd_soc_update_bits(codec, main_reg, 0x20, 0x20);
			pahu_config_compander(codec, interp_idx, event);
		}
		pahu->main_clk_users[interp_idx]++;
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		pahu->main_clk_users[interp_idx]--;
		if (pahu->main_clk_users[interp_idx] <= 0) {
			pahu->main_clk_users[interp_idx] = 0;
			pahu_config_compander(codec, interp_idx, event);
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
		__func__,  event, pahu->main_clk_users[interp_idx]);

	return pahu->main_clk_users[interp_idx];
}
EXPORT_SYMBOL(pahu_codec_enable_interp_clk);

static int pahu_codec_enable_mix_path(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	u16 gain_reg, mix_reg;
	int offset_val = 0;
	int val = 0;

	if (w->shift >= WCD9360_NUM_INTERPOLATORS ||
	    ((w->shift >= INTERP_HPHL_NA) && (w->shift <= INTERP_LO4_NA))) {
		dev_err(codec->dev, "%s: Invalid Interpolator value %d for name %s\n",
			__func__, w->shift, w->name);
		return -EINVAL;
	};

	gain_reg = WCD9360_CDC_RX0_RX_VOL_MIX_CTL +
					(w->shift * WCD9360_RX_PATH_CTL_OFFSET);
	mix_reg = WCD9360_CDC_RX0_RX_PATH_MIX_CTL +
					(w->shift * WCD9360_RX_PATH_CTL_OFFSET);

	if (w->shift == INTERP_AUX) {
		gain_reg = WCD9360_CDC_RX9_RX_VOL_MIX_CTL;
		mix_reg = WCD9360_CDC_RX9_RX_PATH_MIX_CTL;
	}

	if (w->shift == INTERP_SPKR1 ||  w->shift == INTERP_SPKR2)
		__pahu_codec_enable_swr(w, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		pahu_codec_enable_interp_clk(codec, event, w->shift);
		/* Clk enable */
		snd_soc_update_bits(codec, mix_reg, 0x20, 0x20);
		break;
	case SND_SOC_DAPM_POST_PMU:
		if ((pahu->swr.spkr_gain_offset ==
		     WCD9360_RX_GAIN_OFFSET_M1P5_DB) &&
		    (pahu->comp_enabled[COMPANDER_7] ||
		     pahu->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD9360_CDC_RX7_RX_VOL_MIX_CTL ||
		     gain_reg == WCD9360_CDC_RX8_RX_VOL_MIX_CTL)) {
			snd_soc_update_bits(codec, WCD9360_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD9360_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			snd_soc_update_bits(codec, WCD9360_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD9360_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			offset_val = -2;
		}
		val = snd_soc_read(codec, gain_reg);
		val += offset_val;
		snd_soc_write(codec, gain_reg, val);
		pahu_codec_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Clk Disable */
		snd_soc_update_bits(codec, mix_reg, 0x20, 0x00);
		pahu_codec_enable_interp_clk(codec, event, w->shift);
		/* Reset enable and disable */
		snd_soc_update_bits(codec, mix_reg, 0x40, 0x40);
		snd_soc_update_bits(codec, mix_reg, 0x40, 0x00);

		if ((pahu->swr.spkr_gain_offset ==
		     WCD9360_RX_GAIN_OFFSET_M1P5_DB) &&
		    (pahu->comp_enabled[COMPANDER_7] ||
		     pahu->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD9360_CDC_RX7_RX_VOL_MIX_CTL ||
		     gain_reg == WCD9360_CDC_RX8_RX_VOL_MIX_CTL)) {
			snd_soc_update_bits(codec, WCD9360_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD9360_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			snd_soc_update_bits(codec, WCD9360_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD9360_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			offset_val = 2;
			val = snd_soc_read(codec, gain_reg);
			val += offset_val;
			snd_soc_write(codec, gain_reg, val);
		}
		pahu_codec_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	};
	dev_dbg(codec->dev, "%s event %d name %s\n", __func__, event, w->name);

	return 0;
}

static int pahu_codec_enable_main_path(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	u16 gain_reg;
	u16 reg;
	int val;
	int offset_val = 0;

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	if (w->shift >= WCD9360_NUM_INTERPOLATORS ||
	    ((w->shift >= INTERP_HPHL_NA) && (w->shift <= INTERP_LO4_NA))) {
		dev_err(codec->dev, "%s: Invalid Interpolator value %d for name %s\n",
			__func__, w->shift, w->name);
		return -EINVAL;
	};

	reg = WCD9360_CDC_RX0_RX_PATH_CTL + (w->shift *
					     WCD9360_RX_PATH_CTL_OFFSET);
	gain_reg = WCD9360_CDC_RX0_RX_VOL_CTL + (w->shift *
						 WCD9360_RX_PATH_CTL_OFFSET);

	if (w->shift == INTERP_AUX) {
		reg = WCD9360_CDC_RX9_RX_PATH_CTL;
		gain_reg = WCD9360_CDC_RX9_RX_VOL_CTL;
	}
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		pahu_codec_enable_interp_clk(codec, event, w->shift);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* apply gain after int clk is enabled */
		if ((pahu->swr.spkr_gain_offset ==
					WCD9360_RX_GAIN_OFFSET_M1P5_DB) &&
		    (pahu->comp_enabled[COMPANDER_7] ||
		     pahu->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD9360_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD9360_CDC_RX8_RX_VOL_CTL)) {
			snd_soc_update_bits(codec, WCD9360_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD9360_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			snd_soc_update_bits(codec, WCD9360_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    WCD9360_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			offset_val = -2;
		}
		val = snd_soc_read(codec, gain_reg);
		val += offset_val;
		snd_soc_write(codec, gain_reg, val);
		pahu_codec_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	case SND_SOC_DAPM_POST_PMD:
		pahu_codec_enable_interp_clk(codec, event, w->shift);

		if ((pahu->swr.spkr_gain_offset ==
					WCD9360_RX_GAIN_OFFSET_M1P5_DB) &&
		    (pahu->comp_enabled[COMPANDER_7] ||
		     pahu->comp_enabled[COMPANDER_8]) &&
		    (gain_reg == WCD9360_CDC_RX7_RX_VOL_CTL ||
		     gain_reg == WCD9360_CDC_RX8_RX_VOL_CTL)) {
			snd_soc_update_bits(codec, WCD9360_CDC_RX7_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD9360_CDC_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			snd_soc_update_bits(codec, WCD9360_CDC_RX8_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    WCD9360_CDC_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			offset_val = 2;
			val = snd_soc_read(codec, gain_reg);
			val += offset_val;
			snd_soc_write(codec, gain_reg, val);
		}
		pahu_codec_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	};

	return 0;
}

static int pahu_codec_set_iir_gain(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU: /* fall through */
	case SND_SOC_DAPM_PRE_PMD:
		if (strnstr(w->name, "IIR0", sizeof("IIR0"))) {
			snd_soc_write(codec,
				WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL,
			snd_soc_read(codec,
				WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL));
			snd_soc_write(codec,
				WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL,
			snd_soc_read(codec,
				WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL));
			snd_soc_write(codec,
				WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL,
			snd_soc_read(codec,
				WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL));
			snd_soc_write(codec,
				WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL,
			snd_soc_read(codec,
				WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL));
		}
		break;
	}
	return 0;
}

static int pahu_codec_find_amic_input(struct snd_soc_codec *codec,
				       int adc_mux_n)
{
	u16 mask, shift, adc_mux_in_reg;
	u16 amic_mux_sel_reg;
	bool is_amic;

	if (adc_mux_n < 0 || adc_mux_n > WCD9360_MAX_VALID_ADC_MUX ||
	    adc_mux_n == WCD9360_INVALID_ADC_MUX)
		return 0;

	if (adc_mux_n < 3) {
		adc_mux_in_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG1 +
				 2 * adc_mux_n;
		mask = 0x03;
		shift = 0;
		amic_mux_sel_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
				   2 * adc_mux_n;
	} else if (adc_mux_n < 4) {
		adc_mux_in_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX3_CFG1;
		mask = 0x03;
		shift = 0;
		amic_mux_sel_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
				   2 * adc_mux_n;
	} else if (adc_mux_n < 7) {
		adc_mux_in_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG1 +
				 2 * (adc_mux_n - 4);
		mask = 0x0C;
		shift = 2;
		amic_mux_sel_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX4_CFG0 +
				   adc_mux_n - 4;
	} else if (adc_mux_n < 8) {
		adc_mux_in_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX3_CFG1;
		mask = 0x0C;
		shift = 2;
		amic_mux_sel_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX4_CFG0 +
				   adc_mux_n - 4;
	} else if (adc_mux_n < 12) {
		adc_mux_in_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG1 +
				 2 * (((adc_mux_n == 8) ? (adc_mux_n - 8) :
				  (adc_mux_n - 9)));
		mask = 0x30;
		shift = 4;
		amic_mux_sel_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX8_CFG0 +
				   ((adc_mux_n == 8) ? (adc_mux_n - 8) :
				    (adc_mux_n - 9));
	}

	is_amic = (((snd_soc_read(codec, adc_mux_in_reg) & mask) >> shift)
		    == 1);
	if (!is_amic)
		return 0;

	return snd_soc_read(codec, amic_mux_sel_reg) & 0x07;
}

static void pahu_codec_set_tx_hold(struct snd_soc_codec *codec,
				    u16 amic_reg, bool set)
{
	u8 mask = 0x20;
	u8 val;

	if (amic_reg == WCD9360_ANA_AMIC1 ||
	    amic_reg == WCD9360_ANA_AMIC3)
		mask = 0x40;

	val = set ? mask : 0x00;

	switch (amic_reg) {
	case WCD9360_ANA_AMIC1:
	case WCD9360_ANA_AMIC2:
		snd_soc_update_bits(codec, WCD9360_ANA_AMIC2, mask, val);
		break;
	case WCD9360_ANA_AMIC3:
	case WCD9360_ANA_AMIC4:
		snd_soc_update_bits(codec, WCD9360_ANA_AMIC4, mask, val);
		break;
	default:
		dev_dbg(codec->dev, "%s: invalid amic: %d\n",
			__func__, amic_reg);
		break;
	}
}

static int pahu_codec_tx_adc_cfg(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	int adc_mux_n = w->shift;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int amic_n;

	dev_dbg(codec->dev, "%s: event: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		amic_n = pahu_codec_find_amic_input(codec, adc_mux_n);
		break;
	default:
		break;
	}

	return 0;
}

static u16 pahu_codec_get_amic_pwlvl_reg(struct snd_soc_codec *codec, int amic)
{
	u16 pwr_level_reg = 0;

	switch (amic) {
	case 1:
	case 2:
		pwr_level_reg = WCD9360_ANA_AMIC1;
		break;

	case 3:
	case 4:
		pwr_level_reg = WCD9360_ANA_AMIC3;
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

static void pahu_tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct pahu_priv *pahu;
	struct snd_soc_codec *codec;
	u16 dec_cfg_reg, amic_reg, go_bit_reg;
	u8 hpf_cut_off_freq;
	int amic_n;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	pahu = hpf_work->pahu;
	codec = pahu->codec;
	hpf_cut_off_freq = hpf_work->hpf_cut_off_freq;

	dec_cfg_reg = WCD9360_CDC_TX0_TX_PATH_CFG0 + 16 * hpf_work->decimator;
	go_bit_reg = dec_cfg_reg + 7;

	dev_dbg(codec->dev, "%s: decimator %u hpf_cut_of_freq 0x%x\n",
		__func__, hpf_work->decimator, hpf_cut_off_freq);

	amic_n = pahu_codec_find_amic_input(codec, hpf_work->decimator);
	if (amic_n) {
		amic_reg = WCD9360_ANA_AMIC1 + amic_n - 1;
		pahu_codec_set_tx_hold(codec, amic_reg, false);
	}
	snd_soc_update_bits(codec, dec_cfg_reg, TX_HPF_CUT_OFF_FREQ_MASK,
			    hpf_cut_off_freq << 5);
	snd_soc_update_bits(codec, go_bit_reg, 0x02, 0x02);
	/* Minimum 1 clk cycle delay is required as per HW spec */
	usleep_range(1000, 1010);
	snd_soc_update_bits(codec, go_bit_reg, 0x02, 0x00);
}

static void pahu_tx_mute_update_callback(struct work_struct *work)
{
	struct tx_mute_work *tx_mute_dwork;
	struct pahu_priv *pahu;
	struct delayed_work *delayed_work;
	struct snd_soc_codec *codec;
	u16 tx_vol_ctl_reg;

	delayed_work = to_delayed_work(work);
	tx_mute_dwork = container_of(delayed_work, struct tx_mute_work, dwork);
	pahu = tx_mute_dwork->pahu;
	codec = pahu->codec;

	tx_vol_ctl_reg = WCD9360_CDC_TX0_TX_PATH_CTL +
			 16 * tx_mute_dwork->decimator;
	snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x00);
}

static int pahu_codec_enable_rx_path_clk(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 sidetone_reg;

	dev_dbg(codec->dev, "%s %d %d\n", __func__, event, w->shift);
	sidetone_reg = WCD9360_CDC_RX0_RX_PATH_CFG1 + 0x14*(w->shift);

	if (w->shift == INTERP_AUX)
		sidetone_reg = WCD9360_CDC_RX9_RX_PATH_CFG1;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!strcmp(w->name, "RX INT7 MIX2 INP"))
			__pahu_codec_enable_swr(w, event);
		pahu_codec_enable_interp_clk(codec, event, w->shift);
		snd_soc_update_bits(codec, sidetone_reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, sidetone_reg, 0x10, 0x00);
		pahu_codec_enable_interp_clk(codec, event, w->shift);
		if (!strcmp(w->name, "RX INT7 MIX2 INP"))
			__pahu_codec_enable_swr(w, event);
		break;
	default:
		break;
	};
	return 0;
}

static int pahu_codec_enable_dec(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
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

	tx_vol_ctl_reg = WCD9360_CDC_TX0_TX_PATH_CTL + 16 * decimator;
	hpf_gate_reg = WCD9360_CDC_TX0_TX_PATH_SEC2 + 16 * decimator;
	dec_cfg_reg = WCD9360_CDC_TX0_TX_PATH_CFG0 + 16 * decimator;
	tx_gain_ctl_reg = WCD9360_CDC_TX0_TX_VOL_CTL + 16 * decimator;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		amic_n = pahu_codec_find_amic_input(codec, decimator);
		if (amic_n)
			pwr_level_reg = pahu_codec_get_amic_pwlvl_reg(codec,
								       amic_n);

		if (pwr_level_reg) {
			switch ((snd_soc_read(codec, pwr_level_reg) &
					      WCD9360_AMIC_PWR_LVL_MASK) >>
					      WCD9360_AMIC_PWR_LVL_SHIFT) {
			case WCD9360_AMIC_PWR_LEVEL_LP:
				snd_soc_update_bits(codec, dec_cfg_reg,
						    WCD9360_DEC_PWR_LVL_MASK,
						    WCD9360_DEC_PWR_LVL_LP);
				break;

			case WCD9360_AMIC_PWR_LEVEL_HP:
				snd_soc_update_bits(codec, dec_cfg_reg,
						    WCD9360_DEC_PWR_LVL_MASK,
						    WCD9360_DEC_PWR_LVL_HP);
				break;
			case WCD9360_AMIC_PWR_LEVEL_DEFAULT:
			default:
				snd_soc_update_bits(codec, dec_cfg_reg,
						    WCD9360_DEC_PWR_LVL_MASK,
						    WCD9360_DEC_PWR_LVL_DF);
				break;
			}
		}
		/* Enable TX PGA Mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMU:
		hpf_cut_off_freq = (snd_soc_read(codec, dec_cfg_reg) &
				   TX_HPF_CUT_OFF_FREQ_MASK) >> 5;

		pahu->tx_hpf_work[decimator].hpf_cut_off_freq =
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
		schedule_delayed_work(&pahu->tx_mute_dwork[decimator].dwork,
				      msecs_to_jiffies(tx_unmute_delay));
		if (pahu->tx_hpf_work[decimator].hpf_cut_off_freq !=
							CF_MIN_3DB_150HZ)
			schedule_delayed_work(
					&pahu->tx_hpf_work[decimator].dwork,
					msecs_to_jiffies(300));
		/* apply gain after decimator is enabled */
		snd_soc_write(codec, tx_gain_ctl_reg,
			      snd_soc_read(codec, tx_gain_ctl_reg));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		hpf_cut_off_freq =
			pahu->tx_hpf_work[decimator].hpf_cut_off_freq;
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x10);
		if (cancel_delayed_work_sync(
		    &pahu->tx_hpf_work[decimator].dwork)) {
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
				&pahu->tx_mute_dwork[decimator].dwork);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x00);
		snd_soc_update_bits(codec, dec_cfg_reg,
				    WCD9360_DEC_PWR_LVL_MASK,
				    WCD9360_DEC_PWR_LVL_DF);
		break;
	};
out:
	kfree(wname);
	return ret;
}

static u32 pahu_get_dmic_sample_rate(struct snd_soc_codec *codec,
				      unsigned int dmic,
				      struct wcd9xxx_pdata *pdata)
{
	u8 tx_stream_fs;
	u8 adc_mux_index = 0, adc_mux_sel = 0;
	bool dec_found = false;
	u16 adc_mux_ctl_reg, tx_fs_reg;
	u32 dmic_fs;

	while (dec_found == 0 && adc_mux_index < WCD9360_MAX_VALID_ADC_MUX) {
		if (adc_mux_index < 4) {
			adc_mux_ctl_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
						(adc_mux_index * 2);
		} else if (adc_mux_index < WCD9360_INVALID_ADC_MUX) {
			adc_mux_ctl_reg = WCD9360_CDC_TX_INP_MUX_ADC_MUX4_CFG0 +
						adc_mux_index - 4;
		} else if (adc_mux_index == WCD9360_INVALID_ADC_MUX) {
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
		tx_fs_reg = WCD9360_CDC_TX0_TX_PATH_CTL + (16 * adc_mux_index);
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

static u8 pahu_get_dmic_clk_val(struct snd_soc_codec *codec,
				 u32 dmic_clk_rate)
{
	u32 div_factor;
	u8 dmic_ctl_val = WCD9360_DMIC_CLK_DIV_2;

	dev_dbg(codec->dev, "%s: dmic_sample_rate = %d\n",
		__func__, dmic_clk_rate);

	if (dmic_clk_rate == 0) {
		dev_err(codec->dev, "%s: dmic_sample_rate cannot be 0\n",
			__func__);
		goto done;
	}

	div_factor = WCD9360_MCLK_CLK_9P6MHZ / dmic_clk_rate;
	switch (div_factor) {
	case 2:
		dmic_ctl_val = WCD9360_DMIC_CLK_DIV_2;
		break;
	case 3:
		dmic_ctl_val = WCD9360_DMIC_CLK_DIV_3;
		break;
	case 4:
		dmic_ctl_val = WCD9360_DMIC_CLK_DIV_4;
		break;
	case 6:
		dmic_ctl_val = WCD9360_DMIC_CLK_DIV_6;
		break;
	case 8:
		dmic_ctl_val = WCD9360_DMIC_CLK_DIV_8;
		break;
	case 16:
		dmic_ctl_val = WCD9360_DMIC_CLK_DIV_16;
		break;
	default:
		dev_err(codec->dev,
			"%s: Invalid div_factor %u, dmic_rate(%u)\n",
			__func__, div_factor, dmic_clk_rate);
		break;
	}

done:
	return dmic_ctl_val;
}

static int pahu_codec_enable_adc(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s: event:%d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		pahu_codec_set_tx_hold(codec, w->reg, true);
		break;
	default:
		break;
	}

	return 0;
}

static int pahu_codec_enable_dmic(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_pdata *pdata = dev_get_platdata(codec->dev->parent);
	u8  dmic_clk_en = 0x01;
	u16 dmic_clk_reg;
	s32 *dmic_clk_cnt;
	u8 dmic_rate_val, dmic_rate_shift = 1;
	unsigned int dmic;
	u32 dmic_sample_rate;

	dmic = w->shift;
	switch (dmic) {
	case 0:
	case 1:
		dmic_clk_cnt = &(pahu->dmic_0_1_clk_cnt);
		dmic_clk_reg = WCD9360_CPE_SS_DMIC0_CTL;
		break;
	case 2:
	case 3:
		dmic_clk_cnt = &(pahu->dmic_2_3_clk_cnt);
		dmic_clk_reg = WCD9360_CPE_SS_DMIC1_CTL;
		break;
	case 4:
	case 5:
		dmic_clk_cnt = &(pahu->dmic_4_5_clk_cnt);
		dmic_clk_reg = WCD9360_CPE_SS_DMIC2_CTL;
		break;
	case 6:
	case 7:
		dmic_clk_cnt = &(pahu->dmic_6_7_clk_cnt);
		dmic_clk_reg = WCD9360_CPE_SS_DMIC3_CTL;
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
		dmic_sample_rate = pahu_get_dmic_sample_rate(codec, dmic,
							      pdata);
		dmic_rate_val =
			pahu_get_dmic_clk_val(codec,
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
			pahu_get_dmic_clk_val(codec,
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
 * pahu_micbias_control: enable/disable micbias
 * @codec: handle to snd_soc_codec *
 * @micb_num: micbias to be enabled/disabled, e.g. micbias1 or micbias2
 * @req: control requested, enable/disable or pullup enable/disable
 * @is_dapm: triggered by dapm or not
 *
 * return 0 if control is success or error code in case of failure
 */
int pahu_micbias_control(struct snd_soc_codec *codec,
			  int micb_num, int req, bool is_dapm)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	int micb_index = micb_num - 1;
	u16 micb_reg;

	if ((micb_index < 0) || (micb_index > PAHU_MAX_MICBIAS - 1)) {
		dev_err(codec->dev, "%s: Invalid micbias index, micb_ind:%d\n",
			__func__, micb_index);
		return -EINVAL;
	}

	switch (micb_num) {
	case WCD9360_MIC_BIAS_1:
		micb_reg = WCD9360_ANA_MICB1;
		break;
	case WCD9360_MIC_BIAS_2:
		micb_reg = WCD9360_ANA_MICB2;
		break;
	case WCD9360_MIC_BIAS_3:
		micb_reg = WCD9360_ANA_MICB3;
		break;
	case WCD9360_MIC_BIAS_4:
		micb_reg = WCD9360_ANA_MICB4;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid micbias number: %d\n",
			__func__, micb_num);
		return -EINVAL;
	}
	mutex_lock(&pahu->micb_lock);

	switch (req) {
	case WCD9360_MICB_PULLUP_ENABLE:
		pahu->pullup_ref[micb_index]++;
		if ((pahu->pullup_ref[micb_index] == 1) &&
			(pahu->micb_ref[micb_index] == 0))
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x80);
		break;
	case WCD9360_MICB_PULLUP_DISABLE:
		if (pahu->pullup_ref[micb_index] > 0)
			pahu->pullup_ref[micb_index]--;
		if ((pahu->pullup_ref[micb_index] == 0) &&
			(pahu->micb_ref[micb_index] == 0))
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x00);
		break;
	case WCD9360_MICB_ENABLE:
		pahu->micb_ref[micb_index]++;
		if (pahu->micb_ref[micb_index] == 1)
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x40);
		break;
	case WCD9360_MICB_DISABLE:
		if (pahu->micb_ref[micb_index] > 0)
			pahu->micb_ref[micb_index]--;
		if ((pahu->micb_ref[micb_index] == 0) &&
			(pahu->pullup_ref[micb_index] > 0))
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x80);
		else if ((pahu->micb_ref[micb_index] == 0) &&
			 (pahu->pullup_ref[micb_index] == 0))
			snd_soc_update_bits(codec, micb_reg, 0xC0, 0x00);
		break;
	}

	dev_dbg(codec->dev, "%s: micb_num:%d, micb_ref: %d\n",
		__func__, micb_num, pahu->micb_ref[micb_index]);

	mutex_unlock(&pahu->micb_lock);

	return 0;
}
EXPORT_SYMBOL(pahu_micbias_control);

static int __pahu_codec_enable_micbias(struct snd_soc_dapm_widget *w,
					int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int micb_num;

	dev_dbg(codec->dev, "%s: wname: %s, event: %d\n",
		__func__, w->name, event);

	if (strnstr(w->name, "MIC BIAS1", sizeof("MIC BIAS1")))
		micb_num = WCD9360_MIC_BIAS_1;
	else if (strnstr(w->name, "MIC BIAS2", sizeof("MIC BIAS2")))
		micb_num = WCD9360_MIC_BIAS_2;
	else if (strnstr(w->name, "MIC BIAS3", sizeof("MIC BIAS3")))
		micb_num = WCD9360_MIC_BIAS_3;
	else if (strnstr(w->name, "MIC BIAS4", sizeof("MIC BIAS4")))
		micb_num = WCD9360_MIC_BIAS_4;
	else
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/*
		 * Use ref count to handle micbias pullup
		 * and enable requests
		 */
		pahu_micbias_control(codec, micb_num, WCD9360_MICB_ENABLE,
				     true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* wait for cnp time */
		usleep_range(1000, 1100);
		break;
	case SND_SOC_DAPM_POST_PMD:
		pahu_micbias_control(codec, micb_num, WCD9360_MICB_DISABLE,
				     true);
		break;
	};

	return 0;
}

/*
 * pahu_codec_enable_standalone_micbias - enable micbias standalone
 * @codec: pointer to codec instance
 * @micb_num: number of micbias to be enabled
 * @enable: true to enable micbias or false to disable
 *
 * This function is used to enable micbias (1, 2, 3 or 4) during
 * standalone independent of whether TX use-case is running or not
 *
 * Return: error code in case of failure or 0 for success
 */
int pahu_codec_enable_standalone_micbias(struct snd_soc_codec *codec,
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

	if ((micb_index < 0) || (micb_index > PAHU_MAX_MICBIAS - 1)) {
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
EXPORT_SYMBOL(pahu_codec_enable_standalone_micbias);

static int pahu_codec_force_enable_micbias(struct snd_soc_dapm_widget *w,
					    struct snd_kcontrol *kcontrol,
					    int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd_resmgr_enable_master_bias(pahu->resmgr);
		pahu_cdc_mclk_enable(codec, true);
		ret = __pahu_codec_enable_micbias(w, SND_SOC_DAPM_PRE_PMU);
		/* Wait for 1ms for better cnp */
		usleep_range(1000, 1100);
		pahu_cdc_mclk_enable(codec, false);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = __pahu_codec_enable_micbias(w, SND_SOC_DAPM_POST_PMD);
		wcd_resmgr_disable_master_bias(pahu->resmgr);
		break;
	}

	return ret;
}

static int pahu_codec_enable_micbias(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	return __pahu_codec_enable_micbias(w, event);
}

static void pahu_restore_iir_coeff(struct pahu_priv *pahu, int iir_idx,
					int band_idx)
{
	u16 reg_add;
	int no_of_reg = 0;

	regmap_write(pahu->wcd9xxx->regmap,
		(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		(band_idx * BAND_MAX * sizeof(uint32_t)) & 0x7F);
	reg_add = WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx;

	if (pahu->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		return;
	/*
	 * Since wcd9xxx_slim_write_repeat() supports only maximum of 16
	 * registers at a time, split total 20 writes(5 coefficients per
	 * band and 4 writes per coefficient) into 16 and 4.
	 */
	no_of_reg = WCD9360_CDC_REPEAT_WRITES_MAX;
	wcd9xxx_slim_write_repeat(pahu->wcd9xxx, reg_add, no_of_reg,
			&pahu->sidetone_coeff_array[iir_idx][band_idx][0]);

	no_of_reg = (WCD9360_CDC_SIDETONE_IIR_COEFF_MAX * 4) -
					WCD9360_CDC_REPEAT_WRITES_MAX;
	wcd9xxx_slim_write_repeat(pahu->wcd9xxx, reg_add, no_of_reg,
				&pahu->sidetone_coeff_array[iir_idx][band_idx]
				[WCD9360_CDC_REPEAT_WRITES_MAX]);
}

static int pahu_iir_enable_audio_mixer_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	/* IIR filter band registers are at integer multiples of 16 */
	u16 iir_reg = WCD9360_CDC_SIDETONE_IIR0_IIR_CTL + 16 * iir_idx;

	ucontrol->value.integer.value[0] = (snd_soc_read(codec, iir_reg) &
					    (1 << band_idx)) != 0;

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int pahu_iir_enable_audio_mixer_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	bool iir_band_en_status;
	int value = ucontrol->value.integer.value[0];
	u16 iir_reg = WCD9360_CDC_SIDETONE_IIR0_IIR_CTL + 16 * iir_idx;

	pahu_restore_iir_coeff(pahu, iir_idx, band_idx);

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
		(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t)) & 0x7F);

	value |= snd_soc_read(codec,
		(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx));

	snd_soc_write(codec,
		(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 1) & 0x7F);

	value |= (snd_soc_read(codec,
			       (WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				16 * iir_idx)) << 8);

	snd_soc_write(codec,
		(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 2) & 0x7F);

	value |= (snd_soc_read(codec,
			       (WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				16 * iir_idx)) << 16);

	snd_soc_write(codec,
		(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 3) & 0x7F);

	/* Mask bits top 2 bits since they are reserved */
	value |= ((snd_soc_read(codec,
				(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				 16 * iir_idx)) & 0x3F) << 24);

	return value;
}

static int pahu_iir_band_audio_mixer_get(struct snd_kcontrol *kcontrol,
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
		(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value & 0xFF));

	snd_soc_write(codec,
		(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_write(codec,
		(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value >> 16) & 0xFF);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL + 16 * iir_idx),
		(value >> 24) & 0x3F);
}

static int pahu_iir_band_audio_mixer_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int coeff_idx, idx = 0;

	/*
	 * Mask top bit it is reserved
	 * Updates addr automatically for each B2 write
	 */
	snd_soc_write(codec,
		(WCD9360_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL + 16 * iir_idx),
		(band_idx * BAND_MAX * sizeof(uint32_t)) & 0x7F);

	/* Store the coefficients in sidetone coeff array */
	for (coeff_idx = 0; coeff_idx < WCD9360_CDC_SIDETONE_IIR_COEFF_MAX;
		coeff_idx++) {
		uint32_t value = ucontrol->value.integer.value[coeff_idx];

		set_iir_band_coeff(codec, iir_idx, band_idx, value);

		/* Four 8 bit values(one 32 bit) per coefficient */
		pahu->sidetone_coeff_array[iir_idx][band_idx][idx++] =
							(value & 0xFF);
		pahu->sidetone_coeff_array[iir_idx][band_idx][idx++] =
							((value >> 8) & 0xFF);
		pahu->sidetone_coeff_array[iir_idx][band_idx][idx++] =
							((value >> 16) & 0xFF);
		pahu->sidetone_coeff_array[iir_idx][band_idx][idx++] =
							((value >> 24) & 0xFF);
	}

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

static int pahu_compander_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = pahu->comp_enabled[comp];
	return 0;
}

static int pahu_compander_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: Compander %d enable current %d, new %d\n",
		 __func__, comp + 1, pahu->comp_enabled[comp], value);
	pahu->comp_enabled[comp] = value;

	return 0;
}

static int pahu_dmic_pin_mode_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u16 offset;
	u8 reg_val, pinctl_position;

	pinctl_position = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	offset = pinctl_position - WCD9360_TLMM_DMIC_PINCFG_OFFSET;
	reg_val = snd_soc_read(codec,
			WCD9360_TLMM_DMIC1_CLK_PINCFG + offset);

	ucontrol->value.integer.value[0] = !!reg_val;

	return 0;
}

static int pahu_dmic_pin_mode_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	u16 ctl_reg, cfg_reg, offset;
	u8 ctl_val, cfg_val, pinctl_position, pinctl_mode, mask;

	/* 0- high or low; 1- high Z */
	pinctl_mode = ucontrol->value.integer.value[0];
	pinctl_position = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	switch (pinctl_position >> 3) {
	case 0:
		ctl_reg = WCD9360_TEST_DEBUG_PIN_CTL_OE_0;
		break;
	case 1:
		ctl_reg = WCD9360_TEST_DEBUG_PIN_CTL_OE_1;
		break;
	case 2:
		ctl_reg = WCD9360_TEST_DEBUG_PIN_CTL_OE_2;
		break;
	case 3:
		ctl_reg = WCD9360_TEST_DEBUG_PIN_CTL_OE_3;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid pinctl position = %d\n",
			__func__, pinctl_position);
		return -EINVAL;
	}

	ctl_val = ~(pinctl_mode << (pinctl_position & 0x07));
	mask = 1 << (pinctl_position & 0x07);
	snd_soc_update_bits(codec, ctl_reg, mask, ctl_val);

	offset = pinctl_position - WCD9360_TLMM_DMIC_PINCFG_OFFSET;
	cfg_reg = WCD9360_TLMM_DMIC1_CLK_PINCFG + offset;
	if (pinctl_mode) {
		if (pahu->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
			cfg_val = 0x5;
		else
			cfg_val = 0xD;
	} else
		cfg_val = 0;
	snd_soc_update_bits(codec, cfg_reg, 0x1F, cfg_val);

	dev_dbg(codec->dev, "%s: reg=0x%x mask=0x%x val=%d reg=0x%x val=%d\n",
			__func__, ctl_reg, mask, ctl_val, cfg_reg, cfg_val);

	return 0;
}

static int pahu_amic_pwr_lvl_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u16 amic_reg = 0;

	if (!strcmp(kcontrol->id.name, "AMIC_1_2 PWR MODE"))
		amic_reg = WCD9360_ANA_AMIC1;
	if (!strcmp(kcontrol->id.name, "AMIC_3_4 PWR MODE"))
		amic_reg = WCD9360_ANA_AMIC3;

	if (amic_reg)
		ucontrol->value.integer.value[0] =
			(snd_soc_read(codec, amic_reg) &
			 WCD9360_AMIC_PWR_LVL_MASK) >>
			  WCD9360_AMIC_PWR_LVL_SHIFT;
	return 0;
}

static int pahu_amic_pwr_lvl_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u32 mode_val;
	u16 amic_reg = 0;

	mode_val = ucontrol->value.enumerated.item[0];

	dev_dbg(codec->dev, "%s: mode: %d\n", __func__, mode_val);

	if (!strcmp(kcontrol->id.name, "AMIC_1_2 PWR MODE"))
		amic_reg = WCD9360_ANA_AMIC1;
	if (!strcmp(kcontrol->id.name, "AMIC_3_4 PWR MODE"))
		amic_reg = WCD9360_ANA_AMIC3;

	if (amic_reg)
		snd_soc_update_bits(codec, amic_reg, WCD9360_AMIC_PWR_LVL_MASK,
				    mode_val << WCD9360_AMIC_PWR_LVL_SHIFT);
	return 0;
}

static const char *const pahu_conn_mad_text[] = {
	"NOTUSED1", "ADC1", "ADC2", "ADC3", "ADC4", "NOTUSED5",
	"NOTUSED6", "NOTUSED2", "DMIC0", "DMIC1", "DMIC2", "DMIC3",
	"DMIC4", "DMIC5", "DMIC6", "DMIC7"
};

static const struct soc_enum pahu_conn_mad_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pahu_conn_mad_text),
			    pahu_conn_mad_text);

static int pahu_mad_input_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u8 pahu_mad_input;

	pahu_mad_input = snd_soc_read(codec, WCD9360_SOC_MAD_INP_SEL) & 0x0F;
	ucontrol->value.integer.value[0] = pahu_mad_input;

	dev_dbg(codec->dev, "%s: pahu_mad_input = %s\n", __func__,
		pahu_conn_mad_text[pahu_mad_input]);

	return 0;
}

static int pahu_mad_input_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_card *card = codec->component.card;
	u8 pahu_mad_input;
	char mad_amic_input_widget[6];
	const char *mad_input_widget;
	const char *source_widget = NULL;
	u32 adc, i, mic_bias_found = 0;
	int ret = 0;
	char *mad_input;
	bool is_adc_input = false;

	pahu_mad_input = ucontrol->value.integer.value[0];

	if (pahu_mad_input >= sizeof(pahu_conn_mad_text)/
	    sizeof(pahu_conn_mad_text[0])) {
		dev_err(codec->dev,
			"%s: pahu_mad_input = %d out of bounds\n",
			__func__, pahu_mad_input);
		return -EINVAL;
	}

	if (strnstr(pahu_conn_mad_text[pahu_mad_input], "NOTUSED",
				sizeof("NOTUSED"))) {
		dev_dbg(codec->dev,
			"%s: Unsupported pahu_mad_input = %s\n",
			__func__, pahu_conn_mad_text[pahu_mad_input]);
		/* Make sure the MAD register is updated */
		snd_soc_update_bits(codec, WCD9360_ANA_MAD_SETUP,
				    0x88, 0x00);
		return -EINVAL;
	}

	if (strnstr(pahu_conn_mad_text[pahu_mad_input],
		    "ADC", sizeof("ADC"))) {
		mad_input = strpbrk(pahu_conn_mad_text[pahu_mad_input],
				    "1234");
		if (!mad_input) {
			dev_err(codec->dev, "%s: Invalid MAD input %s\n",
				__func__, pahu_conn_mad_text[pahu_mad_input]);
			return -EINVAL;
		}

		ret = kstrtouint(mad_input, 10, &adc);
		if ((ret < 0) || (adc > 4)) {
			dev_err(codec->dev, "%s: Invalid ADC = %s\n", __func__,
				pahu_conn_mad_text[pahu_mad_input]);
			return -EINVAL;
		}

		snprintf(mad_amic_input_widget, 6, "%s%u", "AMIC", adc);

		mad_input_widget = mad_amic_input_widget;
		is_adc_input = true;
	} else {
		/* DMIC type input widget*/
		mad_input_widget = pahu_conn_mad_text[pahu_mad_input];
	}

	dev_dbg(codec->dev,
		"%s: pahu input widget = %s, adc_input = %s\n", __func__,
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

	snd_soc_update_bits(codec, WCD9360_SOC_MAD_INP_SEL,
			    0x0F, pahu_mad_input);
	snd_soc_update_bits(codec, WCD9360_ANA_MAD_SETUP,
			    0x07, mic_bias_found);
	/* for all adc inputs, mad should be in micbias mode with BG enabled */
	if (is_adc_input)
		snd_soc_update_bits(codec, WCD9360_ANA_MAD_SETUP,
				    0x88, 0x88);
	else
		snd_soc_update_bits(codec, WCD9360_ANA_MAD_SETUP,
				    0x88, 0x00);
	return 0;
}

static int pahu_ear_spkr_pa_gain_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = pahu->ear_spkr_gain;

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int pahu_ear_spkr_pa_gain_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	pahu->ear_spkr_gain =  ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: gain = %d\n", __func__, pahu->ear_spkr_gain);

	return 0;
}

static int pahu_spkr_left_boost_stage_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max = 0;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	bst_state_max = snd_soc_read(codec, WCD9360_CDC_BOOST0_BOOST_CTL);
	bst_state_max = (bst_state_max & 0x0c) >> 2;
	ucontrol->value.integer.value[0] = bst_state_max;
	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int pahu_spkr_left_boost_stage_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	bst_state_max =  ucontrol->value.integer.value[0] << 2;
	snd_soc_update_bits(codec, WCD9360_CDC_BOOST0_BOOST_CTL,
		0x0c, bst_state_max);

	return 0;
}

static int pahu_spkr_right_boost_stage_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max = 0;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	bst_state_max = snd_soc_read(codec, WCD9360_CDC_BOOST1_BOOST_CTL);
	bst_state_max = (bst_state_max & 0x0c) >> 2;
	ucontrol->value.integer.value[0] = bst_state_max;
	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int pahu_spkr_right_boost_stage_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	u8 bst_state_max;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	bst_state_max =  ucontrol->value.integer.value[0] << 2;
	snd_soc_update_bits(codec, WCD9360_CDC_BOOST1_BOOST_CTL,
		0x0c, bst_state_max);

	return 0;
}

static const char *const pahu_anc_func_text[] = {"OFF", "ON"};
static const struct soc_enum pahu_anc_func_enum =
	SOC_ENUM_SINGLE_EXT(2, pahu_anc_func_text);

static const char *const pahu_clkmode_text[] = {"EXTERNAL", "INTERNAL"};
static SOC_ENUM_SINGLE_EXT_DECL(pahu_clkmode_enum, pahu_clkmode_text);

/* Cutoff frequency for high pass filter */
static const char * const cf_text[] = {
	"CF_NEG_3DB_4HZ", "CF_NEG_3DB_75HZ", "CF_NEG_3DB_150HZ"
};

static const char * const rx_cf_text[] = {
	"CF_NEG_3DB_4HZ", "CF_NEG_3DB_75HZ", "CF_NEG_3DB_150HZ",
	"CF_NEG_3DB_0P48HZ"
};

static const char * const amic_pwr_lvl_text[] = {
	"LOW_PWR", "DEFAULT", "HIGH_PERF", "HYBRID"
};

static const char * const pahu_ear_pa_gain_text[] = {
	"G_6_DB", "G_4P5_DB", "G_3_DB", "G_1P5_DB",
	"G_0_DB", "G_M2P5_DB", "UNDEFINED", "G_M12_DB"
};

static const char * const pahu_ear_spkr_pa_gain_text[] = {
	"G_DEFAULT", "G_0_DB", "G_1_DB", "G_2_DB", "G_3_DB",
	"G_4_DB", "G_5_DB", "G_6_DB"
};

static const char * const pahu_speaker_boost_stage_text[] = {
	"NO_MAX_STATE", "MAX_STATE_1", "MAX_STATE_2"
};

static SOC_ENUM_SINGLE_EXT_DECL(pahu_ear_pa_gain_enum, pahu_ear_pa_gain_text);
static SOC_ENUM_SINGLE_EXT_DECL(pahu_ear_spkr_pa_gain_enum,
				pahu_ear_spkr_pa_gain_text);
static SOC_ENUM_SINGLE_EXT_DECL(pahu_spkr_boost_stage_enum,
			pahu_speaker_boost_stage_text);
static SOC_ENUM_SINGLE_EXT_DECL(amic_pwr_lvl_enum, amic_pwr_lvl_text);
static SOC_ENUM_SINGLE_DECL(cf_dec0_enum, WCD9360_CDC_TX0_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec1_enum, WCD9360_CDC_TX1_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec2_enum, WCD9360_CDC_TX2_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec3_enum, WCD9360_CDC_TX3_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec4_enum, WCD9360_CDC_TX4_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec5_enum, WCD9360_CDC_TX5_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec6_enum, WCD9360_CDC_TX6_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec7_enum, WCD9360_CDC_TX7_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec8_enum, WCD9360_CDC_TX8_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int0_1_enum, WCD9360_CDC_RX0_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int0_2_enum, WCD9360_CDC_RX0_RX_PATH_MIX_CFG, 2,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int7_1_enum, WCD9360_CDC_RX7_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int7_2_enum, WCD9360_CDC_RX7_RX_PATH_MIX_CFG, 2,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int8_1_enum, WCD9360_CDC_RX8_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int8_2_enum, WCD9360_CDC_RX8_RX_PATH_MIX_CFG, 2,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int9_1_enum, WCD9360_CDC_RX9_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int9_2_enum, WCD9360_CDC_RX9_RX_PATH_MIX_CFG, 2,
							rx_cf_text);

static const struct snd_kcontrol_new pahu_snd_controls[] = {
	SOC_ENUM_EXT("EAR SPKR PA Gain", pahu_ear_spkr_pa_gain_enum,
		     pahu_ear_spkr_pa_gain_get, pahu_ear_spkr_pa_gain_put),
	SOC_ENUM_EXT("SPKR Left Boost Max State", pahu_spkr_boost_stage_enum,
		     pahu_spkr_left_boost_stage_get,
		     pahu_spkr_left_boost_stage_put),
	SOC_ENUM_EXT("SPKR Right Boost Max State", pahu_spkr_boost_stage_enum,
		     pahu_spkr_right_boost_stage_get,
		     pahu_spkr_right_boost_stage_put),
	SOC_SINGLE_TLV("ADC1 Volume", WCD9360_ANA_AMIC1, 0, 20, 0, analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", WCD9360_ANA_AMIC2, 0, 20, 0, analog_gain),
	SOC_SINGLE_TLV("ADC3 Volume", WCD9360_ANA_AMIC3, 0, 20, 0, analog_gain),
	SOC_SINGLE_TLV("ADC4 Volume", WCD9360_ANA_AMIC4, 0, 20, 0, analog_gain),

	SOC_SINGLE_SX_TLV("RX0 Digital Volume", WCD9360_CDC_RX0_RX_VOL_CTL,
		0, -84, 40, digital_gain), /* -84dB min - 40dB max */
	SOC_SINGLE_SX_TLV("RX7 Digital Volume", WCD9360_CDC_RX7_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX8 Digital Volume", WCD9360_CDC_RX8_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX9 Digital Volume", WCD9360_CDC_RX9_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX0 Mix Digital Volume",
		WCD9360_CDC_RX0_RX_VOL_MIX_CTL, 0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX7 Mix Digital Volume",
		WCD9360_CDC_RX7_RX_VOL_MIX_CTL, 0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX8 Mix Digital Volume",
		WCD9360_CDC_RX8_RX_VOL_MIX_CTL, 0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX9 Mix Digital Volume",
		WCD9360_CDC_RX9_RX_VOL_MIX_CTL, 0, -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("DEC0 Volume", WCD9360_CDC_TX0_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC1 Volume", WCD9360_CDC_TX1_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC2 Volume", WCD9360_CDC_TX2_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC3 Volume", WCD9360_CDC_TX3_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC4 Volume", WCD9360_CDC_TX4_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC5 Volume", WCD9360_CDC_TX5_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC6 Volume", WCD9360_CDC_TX6_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC7 Volume", WCD9360_CDC_TX7_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC8 Volume", WCD9360_CDC_TX8_TX_VOL_CTL, 0,
		-84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("IIR0 INP0 Volume",
		WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("IIR0 INP1 Volume",
		WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("IIR0 INP2 Volume",
		WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("IIR0 INP3 Volume",
		WCD9360_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL, 0, -84, 40,
		digital_gain),

	SOC_SINGLE_EXT("ANC Slot", SND_SOC_NOPM, 0, 100, 0, pahu_get_anc_slot,
		pahu_put_anc_slot),
	SOC_ENUM_EXT("ANC Function", pahu_anc_func_enum, pahu_get_anc_func,
		pahu_put_anc_func),

	SOC_ENUM_EXT("CLK MODE", pahu_clkmode_enum, pahu_get_clkmode,
		     pahu_put_clkmode),

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
	SOC_ENUM("RX INT7_1 HPF cut off", cf_int7_1_enum),
	SOC_ENUM("RX INT7_2 HPF cut off", cf_int7_2_enum),
	SOC_ENUM("RX INT8_1 HPF cut off", cf_int8_1_enum),
	SOC_ENUM("RX INT8_2 HPF cut off", cf_int8_2_enum),
	SOC_ENUM("RX INT9_1 HPF cut off", cf_int9_1_enum),
	SOC_ENUM("RX INT9_2 HPF cut off", cf_int9_2_enum),

	SOC_SINGLE_EXT("IIR0 Enable Band1", IIR0, BAND1, 1, 0,
		pahu_iir_enable_audio_mixer_get,
		pahu_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band2", IIR0, BAND2, 1, 0,
		pahu_iir_enable_audio_mixer_get,
		pahu_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band3", IIR0, BAND3, 1, 0,
		pahu_iir_enable_audio_mixer_get,
		pahu_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band4", IIR0, BAND4, 1, 0,
		pahu_iir_enable_audio_mixer_get,
		pahu_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band5", IIR0, BAND5, 1, 0,
		pahu_iir_enable_audio_mixer_get,
		pahu_iir_enable_audio_mixer_put),

	SOC_SINGLE_MULTI_EXT("IIR0 Band1", IIR0, BAND1, 255, 0, 5,
		pahu_iir_band_audio_mixer_get, pahu_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band2", IIR0, BAND2, 255, 0, 5,
		pahu_iir_band_audio_mixer_get, pahu_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band3", IIR0, BAND3, 255, 0, 5,
		pahu_iir_band_audio_mixer_get, pahu_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band4", IIR0, BAND4, 255, 0, 5,
		pahu_iir_band_audio_mixer_get, pahu_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band5", IIR0, BAND5, 255, 0, 5,
		pahu_iir_band_audio_mixer_get, pahu_iir_band_audio_mixer_put),

	SOC_SINGLE_EXT("COMP0 Switch", SND_SOC_NOPM, COMPANDER_0, 1, 0,
		pahu_compander_get, pahu_compander_put),
	SOC_SINGLE_EXT("COMP7 Switch", SND_SOC_NOPM, COMPANDER_7, 1, 0,
		pahu_compander_get, pahu_compander_put),
	SOC_SINGLE_EXT("COMP8 Switch", SND_SOC_NOPM, COMPANDER_8, 1, 0,
		pahu_compander_get, pahu_compander_put),

	SOC_ENUM_EXT("MAD Input", pahu_conn_mad_enum,
		     pahu_mad_input_get, pahu_mad_input_put),

	SOC_SINGLE_EXT("DMIC1_CLK_PIN_MODE", SND_SOC_NOPM, 15, 1, 0,
		pahu_dmic_pin_mode_get, pahu_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC1_DATA_PIN_MODE", SND_SOC_NOPM, 16, 1, 0,
		pahu_dmic_pin_mode_get, pahu_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC2_CLK_PIN_MODE", SND_SOC_NOPM, 17, 1, 0,
		pahu_dmic_pin_mode_get, pahu_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC2_DATA_PIN_MODE", SND_SOC_NOPM, 18, 1, 0,
		pahu_dmic_pin_mode_get, pahu_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC3_CLK_PIN_MODE", SND_SOC_NOPM, 28, 1, 0,
		pahu_dmic_pin_mode_get, pahu_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC3_DATA_PIN_MODE", SND_SOC_NOPM, 29, 1, 0,
		pahu_dmic_pin_mode_get, pahu_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC4_CLK_PIN_MODE", SND_SOC_NOPM, 30, 1, 0,
		pahu_dmic_pin_mode_get, pahu_dmic_pin_mode_put),

	SOC_SINGLE_EXT("DMIC4_DATA_PIN_MODE", SND_SOC_NOPM, 31, 1, 0,
		pahu_dmic_pin_mode_get, pahu_dmic_pin_mode_put),

	SOC_ENUM_EXT("AMIC_1_2 PWR MODE", amic_pwr_lvl_enum,
		pahu_amic_pwr_lvl_get, pahu_amic_pwr_lvl_put),
	SOC_ENUM_EXT("AMIC_3_4 PWR MODE", amic_pwr_lvl_enum,
		pahu_amic_pwr_lvl_get, pahu_amic_pwr_lvl_put),
};

static int pahu_dec_enum_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
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
	case WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG1:
		if (e->shift_l == 0)
			mic_sel_reg = WCD9360_CDC_TX0_TX_PATH_CFG0;
		else if (e->shift_l == 2)
			mic_sel_reg = WCD9360_CDC_TX4_TX_PATH_CFG0;
		else if (e->shift_l == 4)
			mic_sel_reg = WCD9360_CDC_TX8_TX_PATH_CFG0;
		break;
	case WCD9360_CDC_TX_INP_MUX_ADC_MUX1_CFG1:
		if (e->shift_l == 0)
			mic_sel_reg = WCD9360_CDC_TX1_TX_PATH_CFG0;
		else if (e->shift_l == 2)
			mic_sel_reg = WCD9360_CDC_TX5_TX_PATH_CFG0;
		break;
	case WCD9360_CDC_TX_INP_MUX_ADC_MUX2_CFG1:
		if (e->shift_l == 0)
			mic_sel_reg = WCD9360_CDC_TX2_TX_PATH_CFG0;
		else if (e->shift_l == 2)
			mic_sel_reg = WCD9360_CDC_TX6_TX_PATH_CFG0;
		break;
	case WCD9360_CDC_TX_INP_MUX_ADC_MUX3_CFG1:
		if (e->shift_l == 0)
			mic_sel_reg = WCD9360_CDC_TX3_TX_PATH_CFG0;
		else if (e->shift_l == 2)
			mic_sel_reg = WCD9360_CDC_TX7_TX_PATH_CFG0;
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

static int pahu_int_dem_inp_mux_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	unsigned short look_ahead_dly_reg = WCD9360_CDC_RX0_RX_PATH_CFG0;

	val = ucontrol->value.enumerated.item[0];
	if (val >= e->items)
		return -EINVAL;

	dev_dbg(codec->dev, "%s: wname: %s, val: 0x%x\n", __func__,
		widget->name, val);

	if (e->reg == WCD9360_CDC_RX0_RX_PATH_SEC0)
		look_ahead_dly_reg = WCD9360_CDC_RX0_RX_PATH_CFG0;
	else if (e->reg == WCD9360_CDC_RX9_RX_PATH_SEC0)
		look_ahead_dly_reg = WCD9360_CDC_RX9_RX_PATH_CFG0;

	/* Set Look Ahead Delay */
	snd_soc_update_bits(codec, look_ahead_dly_reg,
			    0x08, (val ? 0x08 : 0x00));
	/* Set DEM INP Select */
	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static const char * const rx_int0_7_mix_mux_text[] = {
	"ZERO", "RX0", "RX1", "RX2", "RX3", "RX4", "RX5",
	"RX6", "RX7", "PROXIMITY", "IIR0"
};

static const char * const rx_int_mix_mux_text[] = {
	"ZERO", "RX0", "RX1", "RX2", "RX3", "RX4", "RX5",
	"RX6", "RX7", "NA", "IIR0"
};

static const char * const rx_prim_mix_text[] = {
	"ZERO", "DEC0", "DEC1", "IIR0", "INVALID", "RX0", "RX1", "RX2",
	"RX3", "RX4", "RX5", "RX6", "RX7"
};

static const char * const rx_sidetone_mix_text[] = {
	"ZERO", "SRC0"
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
static const char * const cdc_if_tx10_mux2_text[] = {
	"TX10_MUX1", "I2SRX1_0_BRDG"
};
static const char * const cdc_if_tx11_mux2_text[] = {
	"TX11_MUX1", "I2SRX1_1_BRDG", "SWR_PACKED_PDM"
};
static const char * const cdc_if_tx11_mux_text[] = {
	"RDMA_TX11", "DEC_0_5", "DEC_9_12", "MAD_AUDIO", "MAD_BRDCST"
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
	"NORMAL_DSM_OUT", "NOT_VALID", "ADC_LOOPBACK"
};

static const char * const rx_int0_1_interp_mux_text[] = {
	"ZERO", "RX INT0_1 MIX1",
};

static const char * const rx_int7_1_interp_mux_text[] = {
	"ZERO", "RX INT7_1 MIX1",
};

static const char * const rx_int8_1_interp_mux_text[] = {
	"ZERO", "RX INT8_1 MIX1",
};

static const char * const rx_int9_1_interp_mux_text[] = {
	"ZERO", "RX INT9_1 MIX1",
};

static const char * const rx_int0_2_interp_mux_text[] = {
	"ZERO", "RX INT0_2 MUX",
};

static const char * const rx_int7_2_interp_mux_text[] = {
	"ZERO", "RX INT7_2 MUX",
};

static const char * const rx_int8_2_interp_mux_text[] = {
	"ZERO", "RX INT8_2 MUX",
};

static const char * const rx_int9_2_interp_mux_text[] = {
	"ZERO", "RX INT9_2 MUX",
};

static const char * const mad_sel_txt[] = {
	"SPE", "MSM"
};

static const char * const mad_inp_mux_txt[] = {
	"MAD", "DEC1"
};

static const char * const adc_mux_text[] = {
	"DMIC", "AMIC", "ANC_FB_TUNE1"
};

static const char * const dmic_mux_text[] = {
	"ZERO", "DMIC0", "DMIC1", "DMIC2", "DMIC3", "DMIC4", "DMIC5",
	"NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "DMIC6",
	"DMIC7"
};

static const char * const amic_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "ADC4"
};

static const char * const adc2_in_text[] = {
	"AMIC2", "AMIC1"
};

static const char * const adc4_in_text[] = {
	"AMIC4", "AMIC3"
};

static const char * const anc0_fb_mux_text[] = {
	"ZERO", "INVALID", "ANC_IN_EAR", "ANC_IN_EAR_SPKR",
};

static const char * const rx_echo_mux_text[] = {
	"ZERO", "RX_MIX0", "NA", "NA", "NA", "NA", "NA", "NA",
	"RX_MIX7", "RX_MIX8", "NA", "NA", "NA", "NA", "RX_MIX9"
};

static const char *const slim_rx_mux_text[] = {
	"ZERO", "AIF1_PB", "AIF2_PB", "AIF3_PB", "AIF4_PB"
};

static const char *const cdc_if_rx0_mux_text[] = {
	"SLIM RX0", "I2S RX0"
};
static const char *const cdc_if_rx1_mux_text[] = {
	"SLIM RX1", "I2S RX1"
};
static const char *const cdc_if_rx2_mux_text[] = {
	"SLIM RX2", "I2SRX1_0", "I2SRX0_2"
};
static const char *const cdc_if_rx3_mux_text[] = {
	"SLIM RX3", "I2SRX1_1", "I2SRX0_3"
};
static const char *const cdc_if_rx4_mux_text[] = {
	"SLIM RX4", "I2S RX4"
};
static const char *const cdc_if_rx5_mux_text[] = {
	"SLIM RX5", "I2S RX5"
};
static const char *const cdc_if_rx6_mux_text[] = {
	"SLIM RX6", "I2S RX6"
};
static const char *const cdc_if_rx7_mux_text[] = {
	"SLIM RX7", "I2S RX7"
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

static const char *const wdma3_port0_text[] = {
	"RX_MIX_TX0", "DEC0"
};

static const char *const wdma3_port1_text[] = {
	"RX_MIX_TX1", "DEC1"
};

static const char *const wdma3_port2_text[] = {
	"RX_MIX_TX2", "DEC2"
};

static const char *const wdma3_port3_text[] = {
	"RX_MIX_TX3", "DEC3"
};

static const char *const wdma3_port4_text[] = {
	"RX_MIX_TX4", "DEC4"
};

static const char *const wdma3_port5_text[] = {
	"RX_MIX_TX5", "DEC5"
};

static const char *const wdma3_port6_text[] = {
	"RX_MIX_TX6", "DEC6"
};

static const char *const wdma3_ch_text[] = {
	"PORT_0", "PORT_1", "PORT_2", "PORT_3", "PORT_4",
	"PORT_5", "PORT_6", "PORT_7", "PORT_8",
};

static const struct snd_kcontrol_new aif4_vi_mixer[] = {
	SOC_SINGLE_EXT("SPKR_VI_1", SND_SOC_NOPM, WCD9360_TX14, 1, 0,
			pahu_vi_feed_mixer_get, pahu_vi_feed_mixer_put),
	SOC_SINGLE_EXT("SPKR_VI_2", SND_SOC_NOPM, WCD9360_TX15, 1, 0,
			pahu_vi_feed_mixer_get, pahu_vi_feed_mixer_put),
};

static const struct snd_kcontrol_new aif1_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX0", SND_SOC_NOPM, WCD9360_TX0, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, WCD9360_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, WCD9360_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, WCD9360_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, WCD9360_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, WCD9360_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX6", SND_SOC_NOPM, WCD9360_TX6, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX7", SND_SOC_NOPM, WCD9360_TX7, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX8", SND_SOC_NOPM, WCD9360_TX8, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX9", SND_SOC_NOPM, WCD9360_TX9, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX10", SND_SOC_NOPM, WCD9360_TX10, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX11", SND_SOC_NOPM, WCD9360_TX11, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX13", SND_SOC_NOPM, WCD9360_TX13, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static const struct snd_kcontrol_new aif2_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX0", SND_SOC_NOPM, WCD9360_TX0, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, WCD9360_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, WCD9360_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, WCD9360_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, WCD9360_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, WCD9360_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX6", SND_SOC_NOPM, WCD9360_TX6, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX7", SND_SOC_NOPM, WCD9360_TX7, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX8", SND_SOC_NOPM, WCD9360_TX8, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX9", SND_SOC_NOPM, WCD9360_TX9, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX10", SND_SOC_NOPM, WCD9360_TX10, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX11", SND_SOC_NOPM, WCD9360_TX11, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX13", SND_SOC_NOPM, WCD9360_TX13, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static const struct snd_kcontrol_new aif3_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX0", SND_SOC_NOPM, WCD9360_TX0, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, WCD9360_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, WCD9360_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, WCD9360_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, WCD9360_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, WCD9360_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX6", SND_SOC_NOPM, WCD9360_TX6, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX7", SND_SOC_NOPM, WCD9360_TX7, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX8", SND_SOC_NOPM, WCD9360_TX8, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX9", SND_SOC_NOPM, WCD9360_TX9, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX10", SND_SOC_NOPM, WCD9360_TX10, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX11", SND_SOC_NOPM, WCD9360_TX11, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX13", SND_SOC_NOPM, WCD9360_TX13, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static const struct snd_kcontrol_new aif4_mad_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX13", SND_SOC_NOPM, WCD9360_TX13, 1, 0,
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

WCD_DAPM_ENUM(rx_int0_2, WCD9360_CDC_RX_INP_MUX_RX_INT0_CFG1, 0,
	rx_int0_7_mix_mux_text);
WCD_DAPM_ENUM(rx_int7_2, WCD9360_CDC_RX_INP_MUX_RX_INT7_CFG1, 0,
	rx_int0_7_mix_mux_text);
WCD_DAPM_ENUM(rx_int8_2, WCD9360_CDC_RX_INP_MUX_RX_INT8_CFG1, 0,
	rx_int_mix_mux_text);
WCD_DAPM_ENUM(rx_int9_2, WCD9360_CDC_RX_INP_MUX_RX_INT9_CFG1, 0,
	rx_int0_7_mix_mux_text);

WCD_DAPM_ENUM(rx_int0_1_mix_inp0, WCD9360_CDC_RX_INP_MUX_RX_INT0_CFG0, 0,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int0_1_mix_inp1, WCD9360_CDC_RX_INP_MUX_RX_INT0_CFG0, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int0_1_mix_inp2, WCD9360_CDC_RX_INP_MUX_RX_INT0_CFG1, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int7_1_mix_inp0, WCD9360_CDC_RX_INP_MUX_RX_INT7_CFG0, 0,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int7_1_mix_inp1, WCD9360_CDC_RX_INP_MUX_RX_INT7_CFG0, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int7_1_mix_inp2, WCD9360_CDC_RX_INP_MUX_RX_INT7_CFG1, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int8_1_mix_inp0, WCD9360_CDC_RX_INP_MUX_RX_INT8_CFG0, 0,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int8_1_mix_inp1, WCD9360_CDC_RX_INP_MUX_RX_INT8_CFG0, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int8_1_mix_inp2, WCD9360_CDC_RX_INP_MUX_RX_INT8_CFG1, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int9_1_mix_inp0, WCD9360_CDC_RX_INP_MUX_RX_INT9_CFG0, 0,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int9_1_mix_inp1, WCD9360_CDC_RX_INP_MUX_RX_INT9_CFG0, 4,
	rx_prim_mix_text);
WCD_DAPM_ENUM(rx_int9_1_mix_inp2, WCD9360_CDC_RX_INP_MUX_RX_INT9_CFG1, 4,
	rx_prim_mix_text);

WCD_DAPM_ENUM(rx_int0_mix2_inp, WCD9360_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 0,
	rx_sidetone_mix_text);
WCD_DAPM_ENUM(rx_int7_mix2_inp, WCD9360_CDC_RX_INP_MUX_SIDETONE_SRC_CFG1, 2,
	rx_sidetone_mix_text);
WCD_DAPM_ENUM(rx_int9_mix2_inp, WCD9360_CDC_RX_INP_MUX_SIDETONE_SRC_CFG1, 4,
	rx_sidetone_mix_text);

WCD_DAPM_ENUM(tx_adc_mux10, WCD9360_CDC_TX_INP_MUX_ADC_MUX1_CFG1, 4,
	adc_mux_text);
WCD_DAPM_ENUM(tx_adc_mux11, WCD9360_CDC_TX_INP_MUX_ADC_MUX2_CFG1, 4,
	adc_mux_text);

WCD_DAPM_ENUM(tx_dmic_mux0, WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux1, WCD9360_CDC_TX_INP_MUX_ADC_MUX1_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux2, WCD9360_CDC_TX_INP_MUX_ADC_MUX2_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux3, WCD9360_CDC_TX_INP_MUX_ADC_MUX3_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux4, WCD9360_CDC_TX_INP_MUX_ADC_MUX4_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux5, WCD9360_CDC_TX_INP_MUX_ADC_MUX5_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux6, WCD9360_CDC_TX_INP_MUX_ADC_MUX6_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux7, WCD9360_CDC_TX_INP_MUX_ADC_MUX7_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux8, WCD9360_CDC_TX_INP_MUX_ADC_MUX8_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux10, WCD9360_CDC_TX_INP_MUX_ADC_MUX10_CFG0, 3,
	dmic_mux_text);
WCD_DAPM_ENUM(tx_dmic_mux11, WCD9360_CDC_TX_INP_MUX_ADC_MUX11_CFG0, 3,
	dmic_mux_text);

WCD_DAPM_ENUM(tx_amic_mux0, WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux1, WCD9360_CDC_TX_INP_MUX_ADC_MUX1_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux2, WCD9360_CDC_TX_INP_MUX_ADC_MUX2_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux3, WCD9360_CDC_TX_INP_MUX_ADC_MUX3_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux4, WCD9360_CDC_TX_INP_MUX_ADC_MUX4_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux5, WCD9360_CDC_TX_INP_MUX_ADC_MUX5_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux6, WCD9360_CDC_TX_INP_MUX_ADC_MUX6_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux7, WCD9360_CDC_TX_INP_MUX_ADC_MUX7_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux8, WCD9360_CDC_TX_INP_MUX_ADC_MUX8_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux10, WCD9360_CDC_TX_INP_MUX_ADC_MUX10_CFG0, 0,
	amic_mux_text);
WCD_DAPM_ENUM(tx_amic_mux11, WCD9360_CDC_TX_INP_MUX_ADC_MUX11_CFG0, 0,
	amic_mux_text);

WCD_DAPM_ENUM(tx_adc2_in, WCD9360_ANA_AMIC_INPUT_SWITCH_CTL, 7, adc2_in_text);
WCD_DAPM_ENUM(tx_adc4_in, WCD9360_ANA_AMIC_INPUT_SWITCH_CTL, 6, adc4_in_text);

WCD_DAPM_ENUM(cdc_if_tx0, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG0, 0,
	cdc_if_tx0_mux_text);
WCD_DAPM_ENUM(cdc_if_tx1, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG0, 2,
	cdc_if_tx1_mux_text);
WCD_DAPM_ENUM(cdc_if_tx2, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG0, 4,
	cdc_if_tx2_mux_text);
WCD_DAPM_ENUM(cdc_if_tx3, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG0, 6,
	cdc_if_tx3_mux_text);
WCD_DAPM_ENUM(cdc_if_tx4, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG1, 0,
	cdc_if_tx4_mux_text);
WCD_DAPM_ENUM(cdc_if_tx5, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG1, 2,
	cdc_if_tx5_mux_text);
WCD_DAPM_ENUM(cdc_if_tx6, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG1, 4,
	cdc_if_tx6_mux_text);
WCD_DAPM_ENUM(cdc_if_tx7, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG1, 6,
	cdc_if_tx7_mux_text);
WCD_DAPM_ENUM(cdc_if_tx8, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG2, 0,
	cdc_if_tx8_mux_text);
WCD_DAPM_ENUM(cdc_if_tx9, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG2, 2,
	cdc_if_tx9_mux_text);
WCD_DAPM_ENUM(cdc_if_tx10, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG2, 4,
	cdc_if_tx10_mux_text);
WCD_DAPM_ENUM(cdc_if_tx10_inp2, WCD9360_DATA_HUB_SB_TX10_INP_CFG, 3,
	cdc_if_tx10_mux2_text);
WCD_DAPM_ENUM(cdc_if_tx11_inp1, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG3, 0,
	cdc_if_tx11_inp1_mux_text);
WCD_DAPM_ENUM(cdc_if_tx11, WCD9360_DATA_HUB_SB_TX11_INP_CFG, 0,
	cdc_if_tx11_mux_text);
WCD_DAPM_ENUM(cdc_if_tx11_inp2, WCD9360_DATA_HUB_SB_TX11_INP_CFG, 3,
	cdc_if_tx11_mux2_text);
WCD_DAPM_ENUM(cdc_if_tx13_inp1, WCD9360_CDC_IF_ROUTER_TX_MUX_CFG3, 4,
	cdc_if_tx13_inp1_mux_text);
WCD_DAPM_ENUM(cdc_if_tx13, WCD9360_DATA_HUB_SB_TX13_INP_CFG, 0,
	cdc_if_tx13_mux_text);

WCD_DAPM_ENUM(rx_mix_tx0, WCD9360_CDC_RX_INP_MUX_RX_MIX_CFG0, 0,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx1, WCD9360_CDC_RX_INP_MUX_RX_MIX_CFG0, 4,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx2, WCD9360_CDC_RX_INP_MUX_RX_MIX_CFG1, 0,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx3, WCD9360_CDC_RX_INP_MUX_RX_MIX_CFG1, 4,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx4, WCD9360_CDC_RX_INP_MUX_RX_MIX_CFG2, 0,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx5, WCD9360_CDC_RX_INP_MUX_RX_MIX_CFG2, 4,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx6, WCD9360_CDC_RX_INP_MUX_RX_MIX_CFG3, 0,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx7, WCD9360_CDC_RX_INP_MUX_RX_MIX_CFG3, 4,
	rx_echo_mux_text);
WCD_DAPM_ENUM(rx_mix_tx8, WCD9360_CDC_RX_INP_MUX_RX_MIX_CFG4, 0,
	rx_echo_mux_text);

WCD_DAPM_ENUM(iir0_inp0, WCD9360_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG0, 0,
	iir_inp_mux_text);
WCD_DAPM_ENUM(iir0_inp1, WCD9360_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG1, 0,
	iir_inp_mux_text);
WCD_DAPM_ENUM(iir0_inp2, WCD9360_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG2, 0,
	iir_inp_mux_text);
WCD_DAPM_ENUM(iir0_inp3, WCD9360_CDC_SIDETONE_IIR_INP_MUX_IIR0_MIX_CFG3, 0,
	iir_inp_mux_text);

WCD_DAPM_ENUM(rx_int0_1_interp, SND_SOC_NOPM, 0, rx_int0_1_interp_mux_text);
WCD_DAPM_ENUM(rx_int7_1_interp, SND_SOC_NOPM, 0, rx_int7_1_interp_mux_text);
WCD_DAPM_ENUM(rx_int8_1_interp, SND_SOC_NOPM, 0, rx_int8_1_interp_mux_text);
WCD_DAPM_ENUM(rx_int9_1_interp, SND_SOC_NOPM, 0, rx_int9_1_interp_mux_text);

WCD_DAPM_ENUM(rx_int0_2_interp, SND_SOC_NOPM, 0, rx_int0_2_interp_mux_text);
WCD_DAPM_ENUM(rx_int7_2_interp, SND_SOC_NOPM, 0, rx_int7_2_interp_mux_text);
WCD_DAPM_ENUM(rx_int8_2_interp, SND_SOC_NOPM, 0, rx_int8_2_interp_mux_text);
WCD_DAPM_ENUM(rx_int9_2_interp, SND_SOC_NOPM, 0, rx_int9_2_interp_mux_text);

WCD_DAPM_ENUM(mad_sel, WCD9360_CPE_SS_SVA_CFG, 0,
	mad_sel_txt);

WCD_DAPM_ENUM(mad_inp_mux, WCD9360_CPE_SS_SVA_CFG, 2,
	mad_inp_mux_txt);

WCD_DAPM_ENUM_EXT(rx_int0_dem_inp, WCD9360_CDC_RX0_RX_PATH_SEC0, 0,
	rx_int_dem_inp_mux_text, snd_soc_dapm_get_enum_double,
	pahu_int_dem_inp_mux_put);

WCD_DAPM_ENUM_EXT(rx_int9_dem_inp, WCD9360_CDC_RX9_RX_PATH_SEC0, 0,
	rx_int_dem_inp_mux_text, snd_soc_dapm_get_enum_double,
	pahu_int_dem_inp_mux_put);

WCD_DAPM_ENUM_EXT(tx_adc_mux0, WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG1, 0,
	adc_mux_text, snd_soc_dapm_get_enum_double, pahu_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux1, WCD9360_CDC_TX_INP_MUX_ADC_MUX1_CFG1, 0,
	adc_mux_text, snd_soc_dapm_get_enum_double, pahu_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux2, WCD9360_CDC_TX_INP_MUX_ADC_MUX2_CFG1, 0,
	adc_mux_text, snd_soc_dapm_get_enum_double, pahu_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux3, WCD9360_CDC_TX_INP_MUX_ADC_MUX3_CFG1, 0,
	adc_mux_text, snd_soc_dapm_get_enum_double, pahu_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux4, WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG1, 2,
	adc_mux_text, snd_soc_dapm_get_enum_double, pahu_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux5, WCD9360_CDC_TX_INP_MUX_ADC_MUX1_CFG1, 2,
	adc_mux_text, snd_soc_dapm_get_enum_double, pahu_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux6, WCD9360_CDC_TX_INP_MUX_ADC_MUX2_CFG1, 2,
	adc_mux_text, snd_soc_dapm_get_enum_double, pahu_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux7, WCD9360_CDC_TX_INP_MUX_ADC_MUX3_CFG1, 2,
	adc_mux_text, snd_soc_dapm_get_enum_double, pahu_dec_enum_put);
WCD_DAPM_ENUM_EXT(tx_adc_mux8, WCD9360_CDC_TX_INP_MUX_ADC_MUX0_CFG1, 4,
	adc_mux_text, snd_soc_dapm_get_enum_double, pahu_dec_enum_put);

WCD_DAPM_ENUM(asrc2, WCD9360_CDC_RX_INP_MUX_SPLINE_ASRC_CFG0, 4,
	asrc2_mux_text);
WCD_DAPM_ENUM(asrc3, WCD9360_CDC_RX_INP_MUX_SPLINE_ASRC_CFG0, 6,
	asrc3_mux_text);

WCD_DAPM_ENUM(int7_2_native, SND_SOC_NOPM, 0, native_mux_text);
WCD_DAPM_ENUM(int8_2_native, SND_SOC_NOPM, 0, native_mux_text);

WCD_DAPM_ENUM(anc0_fb, WCD9360_CDC_RX_INP_MUX_ANC_CFG0, 0, anc0_fb_mux_text);

WCD_DAPM_ENUM(wdma3_port0, WCD9360_DMA_WDMA3_PRT_CFG, 0, wdma3_port0_text);
WCD_DAPM_ENUM(wdma3_port1, WCD9360_DMA_WDMA3_PRT_CFG, 1, wdma3_port1_text);
WCD_DAPM_ENUM(wdma3_port2, WCD9360_DMA_WDMA3_PRT_CFG, 2, wdma3_port2_text);
WCD_DAPM_ENUM(wdma3_port3, WCD9360_DMA_WDMA3_PRT_CFG, 3, wdma3_port3_text);
WCD_DAPM_ENUM(wdma3_port4, WCD9360_DMA_WDMA3_PRT_CFG, 4, wdma3_port4_text);
WCD_DAPM_ENUM(wdma3_port5, WCD9360_DMA_WDMA3_PRT_CFG, 5, wdma3_port5_text);
WCD_DAPM_ENUM(wdma3_port6, WCD9360_DMA_WDMA3_PRT_CFG, 6, wdma3_port6_text);

WCD_DAPM_ENUM(wdma3_ch0, WCD9360_DMA_CH_0_1_CFG_WDMA_3, 0, wdma3_ch_text);
WCD_DAPM_ENUM(wdma3_ch1, WCD9360_DMA_CH_0_1_CFG_WDMA_3, 4, wdma3_ch_text);
WCD_DAPM_ENUM(wdma3_ch2, WCD9360_DMA_CH_2_3_CFG_WDMA_3, 0, wdma3_ch_text);
WCD_DAPM_ENUM(wdma3_ch3, WCD9360_DMA_CH_2_3_CFG_WDMA_3, 4, wdma3_ch_text);

static const struct snd_kcontrol_new anc_ear_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_ear_spkr_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_spkr_pa_switch =
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

static const struct snd_kcontrol_new wdma3_onoff_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const char *const i2s_tx1_0_txt[] = {
	"ZERO", "SB_TX8", "SB_RX2", "SB_TX12"
};

static const char *const i2s_tx1_1_txt[] = {
	"ZERO", "SB_RX0", "SB_RX1", "SB_RX2", "SB_RX3", "SB_TX11"
};

WCD_DAPM_ENUM(i2s_tx1_0_inp, WCD9360_DATA_HUB_I2S_TX1_0_CFG, 0, i2s_tx1_0_txt);
WCD_DAPM_ENUM(i2s_tx1_1_inp, WCD9360_DATA_HUB_I2S_TX1_1_CFG, 0, i2s_tx1_1_txt);

static const struct snd_soc_dapm_widget pahu_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN_E("AIF1 PB", "AIF1 Playback", 0, SND_SOC_NOPM,
		AIF1_PB, 0, pahu_codec_enable_slimrx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF2 PB", "AIF2 Playback", 0, SND_SOC_NOPM,
		AIF2_PB, 0, pahu_codec_enable_slimrx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF3 PB", "AIF3 Playback", 0, SND_SOC_NOPM,
		AIF3_PB, 0, pahu_codec_enable_slimrx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF4 PB", "AIF4 Playback", 0, SND_SOC_NOPM,
		AIF4_PB, 0, pahu_codec_enable_slimrx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_IN_E("I2S1 PB", "I2S1 Playback", 0, SND_SOC_NOPM,
		I2S1_PB, 0, pahu_i2s_aif_rx_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	WCD_DAPM_MUX("SLIM RX0 MUX", WCD9360_RX0, slim_rx0),
	WCD_DAPM_MUX("SLIM RX1 MUX", WCD9360_RX1, slim_rx1),
	WCD_DAPM_MUX("SLIM RX2 MUX", WCD9360_RX2, slim_rx2),
	WCD_DAPM_MUX("SLIM RX3 MUX", WCD9360_RX3, slim_rx3),
	WCD_DAPM_MUX("SLIM RX4 MUX", WCD9360_RX4, slim_rx4),
	WCD_DAPM_MUX("SLIM RX5 MUX", WCD9360_RX5, slim_rx5),
	WCD_DAPM_MUX("SLIM RX6 MUX", WCD9360_RX6, slim_rx6),
	WCD_DAPM_MUX("SLIM RX7 MUX", WCD9360_RX7, slim_rx7),

	SND_SOC_DAPM_MIXER("SLIM RX0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX6", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX7", SND_SOC_NOPM, 0, 0, NULL, 0),

	WCD_DAPM_MUX("CDC_IF RX0 MUX", WCD9360_RX0, cdc_if_rx0),
	WCD_DAPM_MUX("CDC_IF RX1 MUX", WCD9360_RX1, cdc_if_rx1),
	WCD_DAPM_MUX("CDC_IF RX2 MUX", WCD9360_RX2, cdc_if_rx2),
	WCD_DAPM_MUX("CDC_IF RX3 MUX", WCD9360_RX3, cdc_if_rx3),
	WCD_DAPM_MUX("CDC_IF RX4 MUX", WCD9360_RX4, cdc_if_rx4),
	WCD_DAPM_MUX("CDC_IF RX5 MUX", WCD9360_RX5, cdc_if_rx5),
	WCD_DAPM_MUX("CDC_IF RX6 MUX", WCD9360_RX6, cdc_if_rx6),
	WCD_DAPM_MUX("CDC_IF RX7 MUX", WCD9360_RX7, cdc_if_rx7),

	SND_SOC_DAPM_MUX_E("RX INT0_2 MUX", SND_SOC_NOPM, INTERP_EAR, 0,
		&rx_int0_2_mux, pahu_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7_2 MUX", SND_SOC_NOPM, INTERP_SPKR1, 0,
		&rx_int7_2_mux, pahu_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_2 MUX", SND_SOC_NOPM, INTERP_SPKR2, 0,
		&rx_int8_2_mux, pahu_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT9_2 MUX", SND_SOC_NOPM, INTERP_AUX, 0,
		&rx_int9_2_mux, pahu_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	WCD_DAPM_MUX("RX INT0_1 MIX1 INP0", 0, rx_int0_1_mix_inp0),
	WCD_DAPM_MUX("RX INT0_1 MIX1 INP1", 0, rx_int0_1_mix_inp1),
	WCD_DAPM_MUX("RX INT0_1 MIX1 INP2", 0, rx_int0_1_mix_inp2),

	SND_SOC_DAPM_MUX_E("RX INT7_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int7_1_mix_inp0_mux, pahu_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int7_1_mix_inp1_mux, pahu_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int7_1_mix_inp2_mux, pahu_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_1 MIX1 INP0", SND_SOC_NOPM, 0, 0,
		&rx_int8_1_mix_inp0_mux, pahu_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_int8_1_mix_inp1_mux, pahu_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_int8_1_mix_inp2_mux, pahu_codec_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	WCD_DAPM_MUX("RX INT9_1 MIX1 INP0", 0, rx_int9_1_mix_inp0),
	WCD_DAPM_MUX("RX INT9_1 MIX1 INP1", 0, rx_int9_1_mix_inp1),
	WCD_DAPM_MUX("RX INT9_1 MIX1 INP2", 0, rx_int9_1_mix_inp2),

	SND_SOC_DAPM_MIXER("RX INT0_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT0 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT7_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT7 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT8_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT8 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT9_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT9 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("RX INT0 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT7 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT9 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER_E("RX INT7 CHAIN", SND_SOC_NOPM, 0, 0,
		NULL, 0, pahu_codec_spk_boost_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX INT8 CHAIN", SND_SOC_NOPM, 0, 0,
		NULL, 0, pahu_codec_spk_boost_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("RX INT0 MIX2 INP", SND_SOC_NOPM, INTERP_EAR,
		0, &rx_int0_mix2_inp_mux, pahu_codec_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7 MIX2 INP", SND_SOC_NOPM, INTERP_SPKR1,
		0, &rx_int7_mix2_inp_mux, pahu_codec_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT9 MIX2 INP", SND_SOC_NOPM, INTERP_AUX,
		0, &rx_int9_mix2_inp_mux, pahu_codec_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	WCD_DAPM_MUX("CDC_IF TX0 MUX", WCD9360_TX0, cdc_if_tx0),
	WCD_DAPM_MUX("CDC_IF TX1 MUX", WCD9360_TX1, cdc_if_tx1),
	WCD_DAPM_MUX("CDC_IF TX2 MUX", WCD9360_TX2, cdc_if_tx2),
	WCD_DAPM_MUX("CDC_IF TX3 MUX", WCD9360_TX3, cdc_if_tx3),
	WCD_DAPM_MUX("CDC_IF TX4 MUX", WCD9360_TX4, cdc_if_tx4),
	WCD_DAPM_MUX("CDC_IF TX5 MUX", WCD9360_TX5, cdc_if_tx5),
	WCD_DAPM_MUX("CDC_IF TX6 MUX", WCD9360_TX6, cdc_if_tx6),
	WCD_DAPM_MUX("CDC_IF TX7 MUX", WCD9360_TX7, cdc_if_tx7),
	WCD_DAPM_MUX("CDC_IF TX8 MUX", WCD9360_TX8, cdc_if_tx8),
	WCD_DAPM_MUX("CDC_IF TX9 MUX", WCD9360_TX9, cdc_if_tx9),
	WCD_DAPM_MUX("CDC_IF TX10 MUX", WCD9360_TX10, cdc_if_tx10),
	WCD_DAPM_MUX("CDC_IF TX11 MUX", WCD9360_TX11, cdc_if_tx11),
	WCD_DAPM_MUX("CDC_IF TX11 INP1 MUX", WCD9360_TX11, cdc_if_tx11_inp1),
	WCD_DAPM_MUX("CDC_IF TX13 MUX", WCD9360_TX13, cdc_if_tx13),
	WCD_DAPM_MUX("CDC_IF TX13 INP1 MUX", WCD9360_TX13, cdc_if_tx13_inp1),
	WCD_DAPM_MUX("CDC_IF TX10 MUX2", WCD9360_TX10, cdc_if_tx10_inp2),
	WCD_DAPM_MUX("CDC_IF TX11 MUX2", WCD9360_TX11, cdc_if_tx11_inp2),

	SND_SOC_DAPM_MUX_E("ADC MUX0", WCD9360_CDC_TX0_TX_PATH_CTL, 5, 0,
		&tx_adc_mux0_mux, pahu_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX1", WCD9360_CDC_TX1_TX_PATH_CTL, 5, 0,
		&tx_adc_mux1_mux, pahu_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX2", WCD9360_CDC_TX2_TX_PATH_CTL, 5, 0,
		&tx_adc_mux2_mux, pahu_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX3", WCD9360_CDC_TX3_TX_PATH_CTL, 5, 0,
		&tx_adc_mux3_mux, pahu_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX4", WCD9360_CDC_TX4_TX_PATH_CTL, 5, 0,
		&tx_adc_mux4_mux, pahu_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX5", WCD9360_CDC_TX5_TX_PATH_CTL, 5, 0,
		&tx_adc_mux5_mux, pahu_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX6", WCD9360_CDC_TX6_TX_PATH_CTL, 5, 0,
		&tx_adc_mux6_mux, pahu_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX7", WCD9360_CDC_TX7_TX_PATH_CTL, 5, 0,
		&tx_adc_mux7_mux, pahu_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX8", WCD9360_CDC_TX8_TX_PATH_CTL, 5, 0,
		&tx_adc_mux8_mux, pahu_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("ADC MUX10", SND_SOC_NOPM, 10, 0, &tx_adc_mux10_mux,
		pahu_codec_tx_adc_cfg, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("ADC MUX11", SND_SOC_NOPM, 11, 0, &tx_adc_mux11_mux,
		pahu_codec_tx_adc_cfg, SND_SOC_DAPM_POST_PMU),

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

	SND_SOC_DAPM_ADC_E("ADC1", NULL, WCD9360_ANA_AMIC1, 7, 0,
		pahu_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("ADC2", NULL, WCD9360_ANA_AMIC2, 7, 0,
		pahu_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("ADC3", NULL, WCD9360_ANA_AMIC3, 7, 0,
		pahu_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("ADC4", NULL, WCD9360_ANA_AMIC4, 7, 0,
		pahu_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),

	WCD_DAPM_MUX("ANC0 FB MUX", 0, anc0_fb),

	WCD_DAPM_MUX("ADC2_IN", 0, tx_adc2_in),
	WCD_DAPM_MUX("ADC4_IN", 0, tx_adc4_in),

	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_INPUT("AMIC3"),
	SND_SOC_DAPM_INPUT("AMIC4"),

	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1", SND_SOC_NOPM, 0, 0,
		pahu_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2", SND_SOC_NOPM, 0, 0,
		pahu_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3", SND_SOC_NOPM, 0, 0,
		pahu_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS4", SND_SOC_NOPM, 0, 0,
		pahu_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS1_STANDALONE, SND_SOC_NOPM, 0, 0,
		pahu_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS2_STANDALONE, SND_SOC_NOPM, 0, 0,
		pahu_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS3_STANDALONE, SND_SOC_NOPM, 0, 0,
		pahu_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS4_STANDALONE, SND_SOC_NOPM, 0, 0,
		pahu_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AIF1 CAP", "AIF1 Capture", 0, SND_SOC_NOPM,
		AIF1_CAP, 0, pahu_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("AIF2 CAP", "AIF2 Capture", 0, SND_SOC_NOPM,
		AIF2_CAP, 0, pahu_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("AIF3 CAP", "AIF3 Capture", 0, SND_SOC_NOPM,
		AIF3_CAP, 0, pahu_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	WCD_DAPM_MUX("I2S TX1_0 MUX", 0, i2s_tx1_0_inp),
	WCD_DAPM_MUX("I2S TX1_1 MUX", 0, i2s_tx1_1_inp),
	SND_SOC_DAPM_MIXER("I2S TX1 MIXER", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_AIF_OUT_E("I2S1 CAP", "I2S1 Capture", 0,
		SND_SOC_NOPM, I2S1_CAP, 0, pahu_i2s_aif_tx_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD |
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
		AIF4_VIFEED, 0, pahu_codec_enable_slimvi_feedback,
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
		pahu_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 1, 0,
		pahu_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 2, 0,
		pahu_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC3", NULL, SND_SOC_NOPM, 3, 0,
		pahu_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC4", NULL, SND_SOC_NOPM, 4, 0,
		pahu_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC5", NULL, SND_SOC_NOPM, 5, 0,
		pahu_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC6", NULL, SND_SOC_NOPM, 6, 0,
		pahu_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC7", NULL, SND_SOC_NOPM, 7, 0,
		pahu_codec_enable_dmic,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	WCD_DAPM_MUX("IIR0 INP0 MUX", 0, iir0_inp0),
	WCD_DAPM_MUX("IIR0 INP1 MUX", 0, iir0_inp1),
	WCD_DAPM_MUX("IIR0 INP2 MUX", 0, iir0_inp2),
	WCD_DAPM_MUX("IIR0 INP3 MUX", 0, iir0_inp3),

	SND_SOC_DAPM_MIXER_E("IIR0", WCD9360_CDC_SIDETONE_IIR0_IIR_PATH_CTL,
		4, 0, NULL, 0, pahu_codec_set_iir_gain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER("SRC0", WCD9360_CDC_SIDETONE_SRC0_ST_SRC_PATH_CTL,
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
	WCD_DAPM_MUX("RX INT9 DEM MUX", 0, rx_int9_dem_inp),

	SND_SOC_DAPM_MUX_E("RX INT0_1 INTERP", SND_SOC_NOPM, INTERP_EAR, 0,
		&rx_int0_1_interp_mux, pahu_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT7_1 INTERP", SND_SOC_NOPM, INTERP_SPKR1, 0,
		&rx_int7_1_interp_mux, pahu_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT8_1 INTERP", SND_SOC_NOPM, INTERP_SPKR2, 0,
		&rx_int8_1_interp_mux, pahu_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT9_1 INTERP", SND_SOC_NOPM, INTERP_AUX, 0,
		&rx_int9_1_interp_mux, pahu_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	WCD_DAPM_MUX("RX INT0_2 INTERP", 0, rx_int0_2_interp),
	WCD_DAPM_MUX("RX INT7_2 INTERP", 0, rx_int7_2_interp),
	WCD_DAPM_MUX("RX INT8_2 INTERP", 0, rx_int8_2_interp),
	WCD_DAPM_MUX("RX INT9_2 INTERP", 0, rx_int9_2_interp),

	SND_SOC_DAPM_SWITCH("ADC US MUX0", WCD9360_CDC_TX0_TX_PATH_192_CTL, 0,
		0, &adc_us_mux0_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX1", WCD9360_CDC_TX1_TX_PATH_192_CTL, 0,
		0, &adc_us_mux1_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX2", WCD9360_CDC_TX2_TX_PATH_192_CTL, 0,
		0, &adc_us_mux2_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX3", WCD9360_CDC_TX3_TX_PATH_192_CTL, 0,
		0, &adc_us_mux3_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX4", WCD9360_CDC_TX4_TX_PATH_192_CTL, 0,
		0, &adc_us_mux4_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX5", WCD9360_CDC_TX5_TX_PATH_192_CTL, 0,
		0, &adc_us_mux5_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX6", WCD9360_CDC_TX6_TX_PATH_192_CTL, 0,
		0, &adc_us_mux6_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX7", WCD9360_CDC_TX7_TX_PATH_192_CTL, 0,
		0, &adc_us_mux7_switch),
	SND_SOC_DAPM_SWITCH("ADC US MUX8", WCD9360_CDC_TX8_TX_PATH_192_CTL, 0,
		0, &adc_us_mux8_switch),

	/* MAD related widgets */
	SND_SOC_DAPM_INPUT("MAD_CPE_INPUT"),
	SND_SOC_DAPM_INPUT("MADINPUT"),

	WCD_DAPM_MUX("MAD_SEL MUX", 0, mad_sel),
	WCD_DAPM_MUX("MAD_INP MUX", 0, mad_inp_mux),

	SND_SOC_DAPM_SWITCH_E("MAD_BROADCAST", SND_SOC_NOPM, 0, 0,
			      &mad_brdcst_switch, pahu_codec_ape_enable_mad,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SWITCH_E("MAD_CPE1", SND_SOC_NOPM, 0, 0,
			      &mad_cpe1_switch, pahu_codec_cpe_mad_ctl,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SWITCH_E("MAD_CPE2", SND_SOC_NOPM, 0, 0,
			      &mad_cpe2_switch, pahu_codec_cpe_mad_ctl,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUTPUT("MAD_CPE_OUT1"),
	SND_SOC_DAPM_OUTPUT("MAD_CPE_OUT2"),

	SND_SOC_DAPM_DAC_E("RX INT0 DAC", NULL, SND_SOC_NOPM,
		0, 0, pahu_codec_ear_dac_event, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_PGA_E("EAR PA", WCD9360_ANA_EAR, 7, 0, NULL, 0,
		pahu_codec_enable_ear_pa,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC EAR PA", WCD9360_ANA_EAR, 7, 0, NULL, 0,
		pahu_codec_enable_ear_pa, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC SPK1 PA", SND_SOC_NOPM, 0, 0, NULL, 0,
		pahu_codec_enable_spkr_anc,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("EAR"),
	SND_SOC_DAPM_OUTPUT("SPK1 OUT"),
	SND_SOC_DAPM_OUTPUT("SPK2 OUT"),
	SND_SOC_DAPM_OUTPUT("ANC EAR"),

	SND_SOC_DAPM_SWITCH("ANC OUT EAR Enable", SND_SOC_NOPM, 0, 0,
		&anc_ear_switch),
	SND_SOC_DAPM_SWITCH("ANC OUT EAR SPKR Enable", SND_SOC_NOPM, 0, 0,
		&anc_ear_spkr_switch),
	SND_SOC_DAPM_SWITCH("ANC SPKR PA Enable", SND_SOC_NOPM, 0, 0,
		&anc_spkr_pa_switch),

	SND_SOC_DAPM_DAC("RX INT9 DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA_E("AUX PA", WCD9360_AUX_ANA_EAR, 7, 0, NULL, 0,
		pahu_codec_enable_aux_pa, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_OUTPUT("AUX"),


	SND_SOC_DAPM_SUPPLY("LDO_RXTX", SND_SOC_NOPM, 0, 0,
		pahu_codec_enable_ldo_rxtx,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("RX INT7 NATIVE SUPPLY", SND_SOC_NOPM,
		INTERP_SPKR1, 0, pahu_enable_native_supply,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("RX INT8 NATIVE SUPPLY", SND_SOC_NOPM,
		INTERP_SPKR2, 0, pahu_enable_native_supply,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	WCD_DAPM_MUX("RX INT7_2 NATIVE MUX", 0, int7_2_native),
	WCD_DAPM_MUX("RX INT8_2 NATIVE MUX", 0, int8_2_native),

	SND_SOC_DAPM_MUX_E("ASRC2 MUX", SND_SOC_NOPM, ASRC2, 0,
		&asrc2_mux, pahu_codec_enable_asrc_resampler,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("ASRC3 MUX", SND_SOC_NOPM, ASRC3, 0,
		&asrc3_mux, pahu_codec_enable_asrc_resampler,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* WDMA3 widgets */
	WCD_DAPM_MUX("WDMA3 PORT0 MUX", 0, wdma3_port0),
	WCD_DAPM_MUX("WDMA3 PORT1 MUX", 1, wdma3_port1),
	WCD_DAPM_MUX("WDMA3 PORT2 MUX", 2, wdma3_port2),
	WCD_DAPM_MUX("WDMA3 PORT3 MUX", 3, wdma3_port3),
	WCD_DAPM_MUX("WDMA3 PORT4 MUX", 4, wdma3_port4),
	WCD_DAPM_MUX("WDMA3 PORT5 MUX", 5, wdma3_port5),
	WCD_DAPM_MUX("WDMA3 PORT6 MUX", 6, wdma3_port6),

	WCD_DAPM_MUX("WDMA3 CH0 MUX", 0, wdma3_ch0),
	WCD_DAPM_MUX("WDMA3 CH1 MUX", 4, wdma3_ch1),
	WCD_DAPM_MUX("WDMA3 CH2 MUX", 0, wdma3_ch2),
	WCD_DAPM_MUX("WDMA3 CH3 MUX", 4, wdma3_ch3),

	SND_SOC_DAPM_MIXER("WDMA3_CH_MIXER", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SWITCH_E("WDMA3_ON_OFF", SND_SOC_NOPM, 0, 0,
			      &wdma3_onoff_switch, pahu_codec_wdma3_ctl,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("WDMA3_OUT"),
};

static int pahu_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(dai->codec);
	u32 i = 0;
	struct wcd9xxx_ch *ch;
	int ret = 0;

	switch (dai->id) {
	case AIF1_PB:
	case AIF2_PB:
	case AIF3_PB:
	case AIF4_PB:
		if (!rx_slot || !rx_num) {
			dev_err(pahu->dev, "%s: Invalid rx_slot 0x%pK or rx_num 0x%pK\n",
				 __func__, rx_slot, rx_num);
			ret = -EINVAL;
			break;
		}
		list_for_each_entry(ch, &pahu->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			dev_dbg(pahu->dev, "%s: slot_num %u ch->ch_num %d\n",
				 __func__, i, ch->ch_num);
			rx_slot[i++] = ch->ch_num;
		}
		*rx_num = i;
		dev_dbg(pahu->dev, "%s: dai_name = %s dai_id = %x  rx_num = %d\n",
			__func__, dai->name, dai->id, i);
		if (*rx_num == 0) {
			dev_err(pahu->dev, "%s: Channel list empty for dai_name = %s dai_id = %x\n",
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
			dev_err(pahu->dev, "%s: Invalid tx_slot 0x%pK or tx_num 0x%pK\n",
				 __func__, tx_slot, tx_num);
			ret = -EINVAL;
			break;
		}
		list_for_each_entry(ch, &pahu->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			dev_dbg(pahu->dev, "%s: slot_num %u ch->ch_num %d\n",
				 __func__, i,  ch->ch_num);
			tx_slot[i++] = ch->ch_num;
		}
		*tx_num = i;
		dev_dbg(pahu->dev, "%s: dai_name = %s dai_id = %x  tx_num = %d\n",
			 __func__, dai->name, dai->id, i);
		if (*tx_num == 0) {
			dev_err(pahu->dev, "%s: Channel list empty for dai_name = %s dai_id = %x\n",
				 __func__, dai->name, dai->id);
			ret = -EINVAL;
		}
		break;
	default:
		dev_err(pahu->dev, "%s: Invalid DAI ID %x\n",
			__func__, dai->id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int pahu_set_channel_map(struct snd_soc_dai *dai,
				 unsigned int tx_num, unsigned int *tx_slot,
				 unsigned int rx_num, unsigned int *rx_slot)
{
	struct pahu_priv *pahu;
	struct wcd9xxx *core;
	struct wcd9xxx_codec_dai_data *dai_data = NULL;

	pahu = snd_soc_codec_get_drvdata(dai->codec);
	core = dev_get_drvdata(dai->codec->dev->parent);

	if (!tx_slot || !rx_slot) {
		dev_err(pahu->dev, "%s: Invalid tx_slot 0x%pK, rx_slot 0x%pK\n",
			__func__, tx_slot, rx_slot);
		return -EINVAL;
	}
	dev_dbg(pahu->dev, "%s(): dai_name = %s DAI-ID %x tx_ch %d rx_ch %d\n",
		 __func__, dai->name, dai->id, tx_num, rx_num);

	wcd9xxx_init_slimslave(core, core->slim->laddr,
				tx_num, tx_slot, rx_num, rx_slot);
	/* Reserve TX13 for MAD data channel */
	dai_data = &pahu->dai[AIF4_MAD_TX];
	if (dai_data)
		list_add_tail(&core->tx_chs[WCD9360_TX13].list,
			      &dai_data->wcd9xxx_ch_list);

	return 0;
}

static int pahu_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	return 0;
}

static void pahu_shutdown(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
}

static int pahu_set_decimator_rate(struct snd_soc_dai *dai,
				    u32 sample_rate)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wcd9xxx_ch *ch;
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
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
		dev_err(pahu->dev, "%s: Invalid TX sample rate: %d\n",
			__func__, sample_rate);
		return -EINVAL;

	};

	list_for_each_entry(ch, &pahu->dai[dai->id].wcd9xxx_ch_list, list) {
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
			tx_port_reg = WCD9360_CDC_IF_ROUTER_TX_MUX_CFG0;
			shift = (tx_port << 1);
			shift_val = 0x03;
		} else if ((tx_port >= 4) && (tx_port < 8)) {
			tx_port_reg = WCD9360_CDC_IF_ROUTER_TX_MUX_CFG1;
			shift = ((tx_port - 4) << 1);
			shift_val = 0x03;
		} else if ((tx_port >= 8) && (tx_port < 11)) {
			tx_port_reg = WCD9360_CDC_IF_ROUTER_TX_MUX_CFG2;
			shift = ((tx_port - 8) << 1);
			shift_val = 0x03;
		} else if (tx_port == 11) {
			tx_port_reg = WCD9360_CDC_IF_ROUTER_TX_MUX_CFG3;
			shift = 0;
			shift_val = 0x0F;
		} else if (tx_port == 13) {
			tx_port_reg = WCD9360_CDC_IF_ROUTER_TX_MUX_CFG3;
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
			tx_fs_reg = WCD9360_CDC_TX0_TX_PATH_CTL +
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

static int pahu_set_mix_interpolator_rate(struct snd_soc_dai *dai,
					   u8 rate_reg_val,
					   u32 sample_rate)
{
	u8 int_2_inp;
	u32 j;
	u16 int_mux_cfg1, int_fs_reg;
	u8 int_mux_cfg1_val;
	struct snd_soc_codec *codec = dai->codec;
	struct wcd9xxx_ch *ch;
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	list_for_each_entry(ch, &pahu->dai[dai->id].wcd9xxx_ch_list, list) {
		int_2_inp = INTn_2_INP_SEL_RX0 + ch->port -
						WCD9360_RX_PORT_START_NUMBER;
		if ((int_2_inp < INTn_2_INP_SEL_RX0) ||
		    (int_2_inp > INTn_2_INP_SEL_RX7)) {
			dev_err(codec->dev, "%s: Invalid RX%u port, Dai ID is %d\n",
				__func__,
				(ch->port - WCD9360_RX_PORT_START_NUMBER),
				dai->id);
			return -EINVAL;
		}

		for (j = 0; j < WCD9360_NUM_INTERPOLATORS; j++) {
			if (j == INTERP_EAR) {
				int_mux_cfg1 =
					WCD9360_CDC_RX_INP_MUX_RX_INT0_CFG1;
				int_fs_reg = WCD9360_CDC_RX0_RX_PATH_MIX_CTL;
			} else if (j == INTERP_SPKR1) {
				int_mux_cfg1 =
					WCD9360_CDC_RX_INP_MUX_RX_INT7_CFG1;
				int_fs_reg = WCD9360_CDC_RX7_RX_PATH_MIX_CTL;
			} else if (j == INTERP_SPKR2) {
				int_mux_cfg1 =
					WCD9360_CDC_RX_INP_MUX_RX_INT8_CFG1;
				int_fs_reg = WCD9360_CDC_RX8_RX_PATH_MIX_CTL;
			} else if (j == INTERP_AUX) {
				int_mux_cfg1 =
					WCD9360_CDC_RX_INP_MUX_RX_INT9_CFG1;
				int_fs_reg = WCD9360_CDC_RX9_RX_PATH_MIX_CTL;
			} else {
				continue;
			}

			int_mux_cfg1_val = snd_soc_read(codec, int_mux_cfg1) &
									0x0F;
			if (int_mux_cfg1_val == int_2_inp) {
				/*
				 * Ear mix path supports only 48, 96, 192,
				 * 384KHz only
				 */
				if ((j == INTERP_EAR || j == INTERP_AUX) &&
				   (rate_reg_val < 0x4 || rate_reg_val > 0x7)) {
					dev_err_ratelimited(codec->dev,
					"%s: Invalid rate for AIF_PB DAI(%d)\n",
					  __func__, dai->id);
					return -EINVAL;
				}

				dev_dbg(codec->dev, "%s: AIF_PB DAI(%d) connected to INT%u_2\n",
					  __func__, dai->id, j);
				dev_dbg(codec->dev, "%s: set INT%u_2 sample rate to %u\n",
					__func__, j, sample_rate);
				snd_soc_update_bits(codec, int_fs_reg, 0x0F,
						    rate_reg_val);
			}
		}
	}
	return 0;
}

static int pahu_set_prim_interpolator_rate(struct snd_soc_dai *dai,
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
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	list_for_each_entry(ch, &pahu->dai[dai->id].wcd9xxx_ch_list, list) {
		int_1_mix1_inp = INTn_1_INP_SEL_RX0 + ch->port -
						WCD9360_RX_PORT_START_NUMBER;
		if ((int_1_mix1_inp < INTn_1_INP_SEL_RX0) ||
		    (int_1_mix1_inp > INTn_1_INP_SEL_RX7)) {
			dev_err(codec->dev, "%s: Invalid RX%u port, Dai ID is %d\n",
				__func__,
				(ch->port - WCD9360_RX_PORT_START_NUMBER),
				dai->id);
			return -EINVAL;
		}

		/*
		 * Loop through all interpolator MUX inputs and find out
		 * to which interpolator input, the slim rx port
		 * is connected
		 */
		for (j = 0; j < WCD9360_NUM_INTERPOLATORS; j++) {
			if (j == INTERP_EAR) {
				int_mux_cfg0 =
					WCD9360_CDC_RX_INP_MUX_RX_INT0_CFG0;
				int_fs_reg = WCD9360_CDC_RX0_RX_PATH_CTL;
			} else if (j == INTERP_SPKR1) {
				int_mux_cfg0 =
					WCD9360_CDC_RX_INP_MUX_RX_INT7_CFG0;
				int_fs_reg = WCD9360_CDC_RX7_RX_PATH_CTL;
			} else if (j == INTERP_SPKR2) {
				int_mux_cfg0 =
					WCD9360_CDC_RX_INP_MUX_RX_INT8_CFG0;
				int_fs_reg = WCD9360_CDC_RX8_RX_PATH_CTL;
			} else if (j == INTERP_AUX) {
				int_mux_cfg0 =
					WCD9360_CDC_RX_INP_MUX_RX_INT9_CFG0;
				int_fs_reg = WCD9360_CDC_RX9_RX_PATH_CTL;
			} else {
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
				 * Primary path does not support
				 * native sample rates
				 */
				if (rate_reg_val > 0x7) {
					dev_err_ratelimited(codec->dev,
					"%s: Invalid rate for AIF_PB DAI(%d)\n",
					  __func__, dai->id);
					return -EINVAL;
				}
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


static int pahu_set_interpolator_rate(struct snd_soc_dai *dai,
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

	ret = pahu_set_prim_interpolator_rate(dai, (u8)rate_val, sample_rate);
	if (ret)
		return ret;
	ret = pahu_set_mix_interpolator_rate(dai, (u8)rate_val, sample_rate);
	if (ret)
		return ret;

	return ret;
}

static int pahu_prepare(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return 0;
}

static int pahu_vi_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *dai)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(dai->codec);

	dev_dbg(pahu->dev, "%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n",
		 __func__, dai->name, dai->id, params_rate(params),
		 params_channels(params));

	pahu->dai[dai->id].rate = params_rate(params);
	pahu->dai[dai->id].bit_width = 32;

	return 0;
}

static int pahu_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(dai->codec);
	int ret = 0;

	dev_dbg(pahu->dev, "%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n",
		 __func__, dai->name, dai->id, params_rate(params),
		 params_channels(params));

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = pahu_set_interpolator_rate(dai, params_rate(params));
		if (ret) {
			dev_err(pahu->dev, "%s: cannot set sample rate: %u\n",
				__func__, params_rate(params));
			return ret;
		}
		switch (params_width(params)) {
		case 16:
			pahu->dai[dai->id].bit_width = 16;
			break;
		case 24:
			pahu->dai[dai->id].bit_width = 24;
			break;
		case 32:
			pahu->dai[dai->id].bit_width = 32;
			break;
		default:
			return -EINVAL;
		}
		pahu->dai[dai->id].rate = params_rate(params);
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		if (dai->id != AIF4_MAD_TX)
			ret = pahu_set_decimator_rate(dai,
						       params_rate(params));
		if (ret) {
			dev_err(pahu->dev, "%s: cannot set TX Decimator rate: %d\n",
				__func__, ret);
			return ret;
		}
		switch (params_width(params)) {
		case 16:
			pahu->dai[dai->id].bit_width = 16;
			break;
		case 24:
			pahu->dai[dai->id].bit_width = 24;
			break;
		default:
			dev_err(pahu->dev, "%s: Invalid format 0x%x\n",
				__func__, params_width(params));
			return -EINVAL;
		};
		pahu->dai[dai->id].rate = params_rate(params);
		break;
	default:
		dev_err(pahu->dev, "%s: Invalid stream type %d\n", __func__,
			substream->stream);
		return -EINVAL;
	};

	return 0;
}

static int pahu_i2s_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(dai->codec);

	dev_dbg(dai->dev, "%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n",
		 __func__, dai->name, dai->id, params_rate(params),
		 params_channels(params));

	pahu->dai[dai->id].rate = params_rate(params);
	pahu->dai[dai->id].bit_width = params_width(params);

	return 0;
}

static struct snd_soc_dai_ops pahu_dai_ops = {
	.startup = pahu_startup,
	.shutdown = pahu_shutdown,
	.hw_params = pahu_hw_params,
	.prepare = pahu_prepare,
	.set_channel_map = pahu_set_channel_map,
	.get_channel_map = pahu_get_channel_map,
};

static struct snd_soc_dai_ops pahu_vi_dai_ops = {
	.hw_params = pahu_vi_hw_params,
	.set_channel_map = pahu_set_channel_map,
	.get_channel_map = pahu_get_channel_map,
};

static struct snd_soc_dai_ops pahu_i2s_dai_ops = {
	.hw_params = pahu_i2s_hw_params,
};

static struct snd_soc_dai_driver pahu_dai[] = {
	{
		.name = "pahu_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9360_RATES_MASK | WCD9360_FRAC_RATES_MASK,
			.formats = WCD9360_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 384000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &pahu_dai_ops,
	},
	{
		.name = "pahu_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9360_RATES_MASK,
			.formats = WCD9360_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &pahu_dai_ops,
	},
	{
		.name = "pahu_rx2",
		.id = AIF2_PB,
		.playback = {
			.stream_name = "AIF2 Playback",
			.rates = WCD9360_RATES_MASK | WCD9360_FRAC_RATES_MASK,
			.formats = WCD9360_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 384000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &pahu_dai_ops,
	},
	{
		.name = "pahu_tx2",
		.id = AIF2_CAP,
		.capture = {
			.stream_name = "AIF2 Capture",
			.rates = WCD9360_RATES_MASK,
			.formats = WCD9360_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &pahu_dai_ops,
	},
	{
		.name = "pahu_rx3",
		.id = AIF3_PB,
		.playback = {
			.stream_name = "AIF3 Playback",
			.rates = WCD9360_RATES_MASK | WCD9360_FRAC_RATES_MASK,
			.formats = WCD9360_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 384000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &pahu_dai_ops,
	},
	{
		.name = "pahu_tx3",
		.id = AIF3_CAP,
		.capture = {
			.stream_name = "AIF3 Capture",
			.rates = WCD9360_RATES_MASK,
			.formats = WCD9360_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &pahu_dai_ops,
	},
	{
		.name = "pahu_rx4",
		.id = AIF4_PB,
		.playback = {
			.stream_name = "AIF4 Playback",
			.rates = WCD9360_RATES_MASK | WCD9360_FRAC_RATES_MASK,
			.formats = WCD9360_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 384000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &pahu_dai_ops,
	},
	{
		.name = "pahu_vifeedback",
		.id = AIF4_VIFEED,
		.capture = {
			.stream_name = "VIfeed",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_48000,
			.formats = WCD9360_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 48000,
			.channels_min = 1,
			.channels_max = 4,
		 },
		.ops = &pahu_vi_dai_ops,
	},
	{
		.name = "pahu_mad1",
		.id = AIF4_MAD_TX,
		.capture = {
			.stream_name = "AIF4 MAD TX",
			.rates = SNDRV_PCM_RATE_16000,
			.formats = WCD9360_FORMATS_S16_LE,
			.rate_min = 16000,
			.rate_max = 16000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &pahu_dai_ops,
	},
	{
		.name = "pahu_i2s1_rx",
		.id = I2S1_PB,
		.playback = {
			.stream_name = "I2S1 Playback",
			.rates = WCD9360_RATES_MASK | WCD9360_FRAC_RATES_MASK,
			.formats = WCD9360_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 384000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &pahu_i2s_dai_ops,
	},
	{
		.name = "pahu_i2s1_tx",
		.id = I2S1_CAP,
		.capture = {
			.stream_name = "I2S1 Capture",
			.rates = WCD9360_RATES_MASK | WCD9360_FRAC_RATES_MASK,
			.formats = WCD9360_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &pahu_i2s_dai_ops,
	},
};

static void pahu_codec_power_gate_digital_core(struct pahu_priv *pahu)
{
	mutex_lock(&pahu->power_lock);
	dev_dbg(pahu->dev, "%s: Entering power gating function, %d\n",
		__func__, pahu->power_active_ref);

	if (pahu->power_active_ref > 0)
		goto exit;

	wcd9xxx_set_power_state(pahu->wcd9xxx,
			WCD_REGION_POWER_COLLAPSE_BEGIN,
			WCD9XXX_DIG_CORE_REGION_1);
	regmap_update_bits(pahu->wcd9xxx->regmap,
			   WCD9360_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x04, 0x04);
	regmap_update_bits(pahu->wcd9xxx->regmap,
			   WCD9360_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x01, 0x00);
	regmap_update_bits(pahu->wcd9xxx->regmap,
			   WCD9360_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x02, 0x00);
	wcd9xxx_set_power_state(pahu->wcd9xxx, WCD_REGION_POWER_DOWN,
				WCD9XXX_DIG_CORE_REGION_1);
exit:
	dev_dbg(pahu->dev, "%s: Exiting power gating function, %d\n",
		__func__, pahu->power_active_ref);
	mutex_unlock(&pahu->power_lock);
}

static void pahu_codec_power_gate_work(struct work_struct *work)
{
	struct pahu_priv *pahu;
	struct delayed_work *dwork;

	dwork = to_delayed_work(work);
	pahu = container_of(dwork, struct pahu_priv, power_gate_work);

	pahu_codec_power_gate_digital_core(pahu);
}

/* called under power_lock acquisition */
static int pahu_dig_core_remove_power_collapse(struct pahu_priv *pahu)
{
	regmap_write(pahu->wcd9xxx->regmap,
		     WCD9360_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x05);
	regmap_write(pahu->wcd9xxx->regmap,
		     WCD9360_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x07);
	regmap_update_bits(pahu->wcd9xxx->regmap,
			   WCD9360_CODEC_RPM_RST_CTL, 0x02, 0x00);
	regmap_update_bits(pahu->wcd9xxx->regmap,
			   WCD9360_CODEC_RPM_RST_CTL, 0x02, 0x02);
	regmap_write(pahu->wcd9xxx->regmap,
		     WCD9360_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x03);

	wcd9xxx_set_power_state(pahu->wcd9xxx,
			WCD_REGION_POWER_COLLAPSE_REMOVE,
			WCD9XXX_DIG_CORE_REGION_1);
	regcache_mark_dirty(pahu->wcd9xxx->regmap);
	regcache_sync_region(pahu->wcd9xxx->regmap,
			     WCD9360_DIG_CORE_REG_MIN,
			     WCD9360_DIG_CORE_REG_MAX);
	return 0;
}

static int pahu_dig_core_power_collapse(struct pahu_priv *pahu,
					 int req_state)
{
	int cur_state;

	/* Exit if feature is disabled */
	if (!dig_core_collapse_enable)
		return 0;

	mutex_lock(&pahu->power_lock);
	if (req_state == POWER_COLLAPSE)
		pahu->power_active_ref--;
	else if (req_state == POWER_RESUME)
		pahu->power_active_ref++;
	else
		goto unlock_mutex;

	if (pahu->power_active_ref < 0) {
		dev_dbg(pahu->dev, "%s: power_active_ref is negative\n",
			__func__);
		goto unlock_mutex;
	}

	if (req_state == POWER_COLLAPSE) {
		if (pahu->power_active_ref == 0) {
			schedule_delayed_work(&pahu->power_gate_work,
			msecs_to_jiffies(dig_core_collapse_timer * 1000));
		}
	} else if (req_state == POWER_RESUME) {
		if (pahu->power_active_ref == 1) {
			/*
			 * At this point, there can be two cases:
			 * 1. Core already in power collapse state
			 * 2. Timer kicked in and still did not expire or
			 * waiting for the power_lock
			 */
			cur_state = wcd9xxx_get_current_power_state(
						pahu->wcd9xxx,
						WCD9XXX_DIG_CORE_REGION_1);
			if (cur_state == WCD_REGION_POWER_DOWN) {
				pahu_dig_core_remove_power_collapse(pahu);
			} else {
				mutex_unlock(&pahu->power_lock);
				cancel_delayed_work_sync(
						&pahu->power_gate_work);
				mutex_lock(&pahu->power_lock);
			}
		}
	}

unlock_mutex:
	mutex_unlock(&pahu->power_lock);

	return 0;
}

static int pahu_cdc_req_mclk_enable(struct pahu_priv *pahu,
				     bool enable)
{
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(pahu->wcd_ext_clk);
		if (ret) {
			dev_err(pahu->dev, "%s: ext clk enable failed\n",
				__func__);
			goto done;
		}
		/* get BG */
		wcd_resmgr_enable_master_bias(pahu->resmgr);
		/* get MCLK */
		wcd_resmgr_enable_clk_block(pahu->resmgr, WCD_CLK_MCLK);
	} else {
		/* put MCLK */
		wcd_resmgr_disable_clk_block(pahu->resmgr, WCD_CLK_MCLK);
		/* put BG */
		wcd_resmgr_disable_master_bias(pahu->resmgr);
		clk_disable_unprepare(pahu->wcd_ext_clk);
	}

done:
	return ret;
}

static int __pahu_cdc_mclk_enable_locked(struct pahu_priv *pahu,
					  bool enable)
{
	int ret = 0;

	if (!pahu->wcd_ext_clk) {
		dev_err(pahu->dev, "%s: wcd ext clock is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(pahu->dev, "%s: mclk_enable = %u\n", __func__, enable);

	if (enable) {
		pahu_dig_core_power_collapse(pahu, POWER_RESUME);
		pahu_vote_svs(pahu, true);
		ret = pahu_cdc_req_mclk_enable(pahu, true);
		if (ret)
			goto done;
	} else {
		pahu_cdc_req_mclk_enable(pahu, false);
		pahu_vote_svs(pahu, false);
		pahu_dig_core_power_collapse(pahu, POWER_COLLAPSE);
	}

done:
	return ret;
}

static int __pahu_cdc_mclk_enable(struct pahu_priv *pahu,
				   bool enable)
{
	int ret;

	WCD9XXX_V2_BG_CLK_LOCK(pahu->resmgr);
	ret = __pahu_cdc_mclk_enable_locked(pahu, enable);
	if (enable)
		wcd_resmgr_set_sido_input_src(pahu->resmgr,
						     SIDO_SOURCE_RCO_BG);
	WCD9XXX_V2_BG_CLK_UNLOCK(pahu->resmgr);

	return ret;
}

static ssize_t pahu_codec_version_read(struct snd_info_entry *entry,
					void *file_private_data,
					struct file *file,
					char __user *buf, size_t count,
					loff_t pos)
{
	struct pahu_priv *pahu;
	struct wcd9xxx *wcd9xxx;
	char buffer[PAHU_VERSION_ENTRY_SIZE];
	int len = 0;

	pahu = (struct pahu_priv *) entry->private_data;
	if (!pahu) {
		pr_err("%s: pahu priv is null\n", __func__);
		return -EINVAL;
	}

	wcd9xxx = pahu->wcd9xxx;

	switch (wcd9xxx->version) {
	case PAHU_VERSION_1_0:
		len = snprintf(buffer, sizeof(buffer), "WCD9360_1_0\n");
		break;
	default:
		len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops pahu_codec_info_ops = {
	.read = pahu_codec_version_read,
};

/*
 * pahu_codec_info_create_codec_entry - creates wcd9360 module
 * @codec_root: The parent directory
 * @codec: Codec instance
 *
 * Creates wcd9360 module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int pahu_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					struct snd_soc_codec *codec)
{
	struct snd_info_entry *version_entry;
	struct pahu_priv *pahu;
	struct snd_soc_card *card;

	if (!codec_root || !codec)
		return -EINVAL;

	pahu = snd_soc_codec_get_drvdata(codec);
	card = codec->component.card;
	pahu->entry = snd_info_create_subdir(codec_root->module,
					      "pahu", codec_root);
	if (!pahu->entry) {
		dev_dbg(codec->dev, "%s: failed to create wcd9360 entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   pahu->entry);
	if (!version_entry) {
		dev_dbg(codec->dev, "%s: failed to create wcd9360 version entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry->private_data = pahu;
	version_entry->size = PAHU_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &pahu_codec_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		return -ENOMEM;
	}
	pahu->version_entry = version_entry;

	return 0;
}
EXPORT_SYMBOL(pahu_codec_info_create_codec_entry);

/**
 * pahu_cdc_mclk_enable - Enable/disable codec mclk
 *
 * @codec: codec instance
 * @enable: Indicates clk enable or disable
 *
 * Returns 0 on Success and error on failure
 */
int pahu_cdc_mclk_enable(struct snd_soc_codec *codec, bool enable)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	return __pahu_cdc_mclk_enable(pahu, enable);
}
EXPORT_SYMBOL(pahu_cdc_mclk_enable);

static int __pahu_codec_internal_rco_ctrl(struct snd_soc_codec *codec,
					   bool enable)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	if (enable) {
		if (wcd_resmgr_get_clk_type(pahu->resmgr) ==
		    WCD_CLK_RCO) {
			ret = wcd_resmgr_enable_clk_block(pahu->resmgr,
							  WCD_CLK_RCO);
		} else {
			ret = pahu_cdc_req_mclk_enable(pahu, true);
			if (ret) {
				dev_err(codec->dev,
					"%s: mclk_enable failed, err = %d\n",
					__func__, ret);
				goto done;
			}
			wcd_resmgr_set_sido_input_src(pahu->resmgr,
							SIDO_SOURCE_RCO_BG);
			ret = wcd_resmgr_enable_clk_block(pahu->resmgr,
							   WCD_CLK_RCO);
			ret |= pahu_cdc_req_mclk_enable(pahu, false);
		}

	} else {
		ret = wcd_resmgr_disable_clk_block(pahu->resmgr,
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
 * pahu_codec_internal_rco_ctrl: Enable/Disable codec's RCO clock
 * @codec: Handle to the codec
 * @enable: Indicates whether clock should be enabled or disabled
 */
static int pahu_codec_internal_rco_ctrl(struct snd_soc_codec *codec,
					 bool enable)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	WCD9XXX_V2_BG_CLK_LOCK(pahu->resmgr);
	ret = __pahu_codec_internal_rco_ctrl(codec, enable);
	WCD9XXX_V2_BG_CLK_UNLOCK(pahu->resmgr);
	return ret;
}

/*
 * pahu_cdc_mclk_tx_enable: Enable/Disable codec's clock for TX path
 * @codec: Handle to codec
 * @enable: Indicates whether clock should be enabled or disabled
 */
int pahu_cdc_mclk_tx_enable(struct snd_soc_codec *codec, bool enable)
{
	struct pahu_priv *pahu_p;
	int ret = 0;
	bool clk_mode;
	bool clk_internal;

	if (!codec)
		return -EINVAL;

	pahu_p = snd_soc_codec_get_drvdata(codec);
	clk_mode = test_bit(CLK_MODE, &pahu_p->status_mask);
	clk_internal = test_bit(CLK_INTERNAL, &pahu_p->status_mask);

	dev_dbg(codec->dev, "%s: clkmode: %d, enable: %d, clk_internal: %d\n",
		__func__, clk_mode, enable, clk_internal);

	if (clk_mode || clk_internal) {
		if (enable) {
			wcd_resmgr_enable_master_bias(pahu_p->resmgr);
			pahu_dig_core_power_collapse(pahu_p, POWER_RESUME);
			pahu_vote_svs(pahu_p, true);
			ret = pahu_codec_internal_rco_ctrl(codec, enable);
			set_bit(CLK_INTERNAL, &pahu_p->status_mask);
		} else {
			clear_bit(CLK_INTERNAL, &pahu_p->status_mask);
			pahu_codec_internal_rco_ctrl(codec, enable);
			pahu_vote_svs(pahu_p, false);
			pahu_dig_core_power_collapse(pahu_p, POWER_COLLAPSE);
			wcd_resmgr_disable_master_bias(pahu_p->resmgr);
		}
	} else {
		ret = __pahu_cdc_mclk_enable(pahu_p, enable);
	}

	return ret;
}
EXPORT_SYMBOL(pahu_cdc_mclk_tx_enable);

static const struct wcd_resmgr_cb pahu_resmgr_cb = {
	.cdc_rco_ctrl = __pahu_codec_internal_rco_ctrl,
};

static const struct pahu_reg_mask_val pahu_codec_mclk2_1_0_defaults[] = {
	/*
	 * PLL Settings:
	 * Clock Root: MCLK2,
	 * Clock Source: EXT_CLK,
	 * Clock Destination: MCLK2
	 * Clock Freq In: 19.2MHz,
	 * Clock Freq Out: 11.2896MHz
	 */
	{WCD9360_CLK_SYS_MCLK2_PRG1, 0x60, 0x20},
	{WCD9360_CLK_SYS_INT_POST_DIV_REG0, 0xFF, 0x5E},
	{WCD9360_CLK_SYS_INT_POST_DIV_REG1, 0x1F, 0x1F},
	{WCD9360_CLK_SYS_INT_REF_DIV_REG0, 0xFF, 0x54},
	{WCD9360_CLK_SYS_INT_REF_DIV_REG1, 0xFF, 0x01},
	{WCD9360_CLK_SYS_INT_FILTER_REG1, 0x07, 0x04},
	{WCD9360_CLK_SYS_INT_PLL_L_VAL, 0xFF, 0x93},
	{WCD9360_CLK_SYS_INT_PLL_N_VAL, 0xFF, 0xFA},
	{WCD9360_CLK_SYS_INT_TEST_REG0, 0xFF, 0x90},
	{WCD9360_CLK_SYS_INT_PFD_CP_DSM_PROG, 0xFF, 0x7E},
	{WCD9360_CLK_SYS_INT_VCO_PROG, 0xFF, 0xF8},
	{WCD9360_CLK_SYS_INT_TEST_REG1, 0xFF, 0x68},
	{WCD9360_CLK_SYS_INT_LDO_LOCK_CFG, 0xFF, 0x40},
	{WCD9360_CLK_SYS_INT_DIG_LOCK_DET_CFG, 0xFF, 0x32},
};

static const struct pahu_reg_mask_val pahu_codec_reg_defaults[] = {
	{WCD9360_BIAS_VBG_FINE_ADJ, 0xFF, 0x75},
	{WCD9360_CODEC_RPM_CLK_MCLK_CFG, 0x03, 0x01},
	{WCD9360_CODEC_CPR_SVS_CX_VDD, 0xFF, 0x7C}, /* value in svs mode */
	{WCD9360_CODEC_CPR_SVS2_CX_VDD, 0xFF, 0x58}, /* value in svs2 mode */
	{WCD9360_CDC_RX0_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD9360_CDC_RX7_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD9360_CDC_RX8_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD9360_CDC_RX9_RX_PATH_DSMDEM_CTL, 0x01, 0x01},
	{WCD9360_CDC_COMPANDER8_CTL7, 0x1E, 0x18},
	{WCD9360_CDC_COMPANDER7_CTL7, 0x1E, 0x18},
	{WCD9360_CDC_RX0_RX_PATH_SEC0, 0x08, 0x00},
	{WCD9360_CDC_RX9_RX_PATH_SEC0, 0x08, 0x00},
	{WCD9360_MICB1_TEST_CTL_2, 0x07, 0x01},
	{WCD9360_CDC_BOOST0_BOOST_CFG1, 0x3F, 0x12},
	{WCD9360_CDC_BOOST0_BOOST_CFG2, 0x1C, 0x08},
	{WCD9360_CDC_BOOST1_BOOST_CFG1, 0x3F, 0x12},
	{WCD9360_CDC_BOOST1_BOOST_CFG2, 0x1C, 0x08},
	{WCD9360_CPE_SS_CPARMAD_BUFRDY_INT_PERIOD, 0x1F, 0x09},
	{WCD9360_CDC_TX0_TX_PATH_CFG1, 0x01, 0x00},
	{WCD9360_CDC_TX1_TX_PATH_CFG1, 0x01, 0x00},
	{WCD9360_CDC_TX2_TX_PATH_CFG1, 0x01, 0x00},
	{WCD9360_CDC_TX3_TX_PATH_CFG1, 0x01, 0x00},
	{WCD9360_CDC_TX4_TX_PATH_CFG1, 0x01, 0x00},
	{WCD9360_CDC_TX5_TX_PATH_CFG1, 0x01, 0x00},
	{WCD9360_CDC_TX6_TX_PATH_CFG1, 0x01, 0x00},
	{WCD9360_CDC_TX7_TX_PATH_CFG1, 0x01, 0x00},
	{WCD9360_CDC_TX8_TX_PATH_CFG1, 0x01, 0x00},
	{WCD9360_CPE_FLL_CONFIG_CTL_2, 0xFF, 0x20},
	{WCD9360_CPE_SS_DMIC_CFG, 0x80, 0x00},
	{WCD9360_CDC_BOOST0_BOOST_CTL, 0x70, 0x50},
	{WCD9360_CDC_BOOST1_BOOST_CTL, 0x70, 0x50},
	{WCD9360_CDC_RX7_RX_PATH_CFG1, 0x08, 0x08},
	{WCD9360_CDC_RX8_RX_PATH_CFG1, 0x08, 0x08},
	{WCD9360_CDC_TOP_TOP_CFG1, 0x02, 0x02},
	{WCD9360_CDC_TOP_TOP_CFG1, 0x01, 0x01},
	{WCD9360_CDC_TOP_EAR_COMP_LUT, 0x80, 0x80},
	{WCD9360_EAR_EAR_DAC_CON, 0x06, 0x02},
	{WCD9360_AUX_INT_AUX_DAC_CON, 0x06, 0x02},
	{WCD9360_CDC_TX9_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD9360_CDC_TX10_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD9360_CDC_TX11_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD9360_CDC_TX12_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{WCD9360_DATA_HUB_SB_TX11_INP_CFG, 0x01, 0x01},
	{WCD9360_CDC_CLK_RST_CTRL_FS_CNT_CONTROL, 0x01, 0x01},
	{WCD9360_CDC_COMPANDER7_CTL3, 0x80, 0x80},
	{WCD9360_CDC_COMPANDER8_CTL3, 0x80, 0x80},
	{WCD9360_CDC_COMPANDER7_CTL7, 0x01, 0x01},
	{WCD9360_CDC_COMPANDER8_CTL7, 0x01, 0x01},
	{WCD9360_CODEC_RPM_CLK_GATE, 0x08, 0x00},
	{WCD9360_CPE_SS_SVA_CFG, 0x60, 0x00},
	{WCD9360_CPE_SS_CPAR_CFG, 0x10, 0x10},
};

static const struct pahu_cpr_reg_defaults cpr_defaults[] = {
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

static void pahu_update_reg_defaults(struct pahu_priv *pahu)
{
	u32 i;
	struct wcd9xxx *wcd9xxx;

	wcd9xxx = pahu->wcd9xxx;
	for (i = 0; i < ARRAY_SIZE(pahu_codec_reg_defaults); i++)
		regmap_update_bits(wcd9xxx->regmap,
				   pahu_codec_reg_defaults[i].reg,
				   pahu_codec_reg_defaults[i].mask,
				   pahu_codec_reg_defaults[i].val);
}

static void pahu_update_cpr_defaults(struct pahu_priv *pahu)
{
	int i;
	struct wcd9xxx *wcd9xxx;

	wcd9xxx = pahu->wcd9xxx;

	__pahu_cdc_mclk_enable(pahu, true);

	regmap_update_bits(wcd9xxx->regmap, WCD9360_CODEC_RPM_CLK_GATE,
			   0x10, 0x00);

	for (i = 0; i < ARRAY_SIZE(cpr_defaults); i++) {
		regmap_bulk_write(wcd9xxx->regmap,
				WCD9360_CODEC_CPR_WR_DATA_0,
				(u8 *)&cpr_defaults[i].wr_data, 4);
		regmap_bulk_write(wcd9xxx->regmap,
				WCD9360_CODEC_CPR_WR_ADDR_0,
				(u8 *)&cpr_defaults[i].wr_addr, 4);
	}

	__pahu_cdc_mclk_enable(pahu, false);
}

static void pahu_slim_interface_init_reg(struct snd_soc_codec *codec)
{
	int i;
	struct pahu_priv *priv = snd_soc_codec_get_drvdata(codec);

	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++)
		wcd9xxx_interface_reg_write(priv->wcd9xxx,
				    WCD9360_SLIM_PGD_PORT_INT_RX_EN0 + i,
				    0xFF);
}

static irqreturn_t pahu_misc_irq(int irq, void *data)
{
	struct pahu_priv *pahu = data;
	int misc_val;

	/* Find source of interrupt */
	regmap_read(pahu->wcd9xxx->regmap, WCD9360_INTR_CODEC_MISC_STATUS,
		    &misc_val);

	dev_dbg(pahu->dev, "%s: Codec misc irq: %d, val: 0x%x\n",
		__func__, irq, misc_val);

	/* Clear interrupt status */
	regmap_update_bits(pahu->wcd9xxx->regmap,
			   WCD9360_INTR_CODEC_MISC_CLEAR, misc_val, 0x00);

	return IRQ_HANDLED;
}

static irqreturn_t pahu_slimbus_irq(int irq, void *data)
{
	struct pahu_priv *pahu = data;
	unsigned long status = 0;
	int i, j, port_id, k;
	u32 bit;
	u8 val, int_val = 0;
	bool tx, cleared;
	unsigned short reg = 0;

	for (i = WCD9360_SLIM_PGD_PORT_INT_STATUS_RX_0, j = 0;
	     i <= WCD9360_SLIM_PGD_PORT_INT_STATUS_TX_1; i++, j++) {
		val = wcd9xxx_interface_reg_read(pahu->wcd9xxx, i);
		status |= ((u32)val << (8 * j));
	}

	for_each_set_bit(j, &status, 32) {
		tx = (j >= 16 ? true : false);
		port_id = (tx ? j - 16 : j);
		val = wcd9xxx_interface_reg_read(pahu->wcd9xxx,
				WCD9360_SLIM_PGD_PORT_INT_RX_SOURCE0 + j);
		if (val) {
			if (!tx)
				reg = WCD9360_SLIM_PGD_PORT_INT_RX_EN0 +
					(port_id / 8);
			else
				reg = WCD9360_SLIM_PGD_PORT_INT_TX_EN0 +
					(port_id / 8);
			int_val = wcd9xxx_interface_reg_read(
				pahu->wcd9xxx, reg);
			/*
			 * Ignore interrupts for ports for which the
			 * interrupts are not specifically enabled.
			 */
			if (!(int_val & (1 << (port_id % 8))))
				continue;
		}
		if (val & WCD9360_SLIM_IRQ_OVERFLOW)
			dev_err_ratelimited(pahu->dev, "%s: overflow error on %s port %d, value %x\n",
			   __func__, (tx ? "TX" : "RX"), port_id, val);
		if (val & WCD9360_SLIM_IRQ_UNDERFLOW)
			dev_err_ratelimited(pahu->dev, "%s: underflow error on %s port %d, value %x\n",
			   __func__, (tx ? "TX" : "RX"), port_id, val);
		if ((val & WCD9360_SLIM_IRQ_OVERFLOW) ||
			(val & WCD9360_SLIM_IRQ_UNDERFLOW)) {
			if (!tx)
				reg = WCD9360_SLIM_PGD_PORT_INT_RX_EN0 +
					(port_id / 8);
			else
				reg = WCD9360_SLIM_PGD_PORT_INT_TX_EN0 +
					(port_id / 8);
			int_val = wcd9xxx_interface_reg_read(
				pahu->wcd9xxx, reg);
			if (int_val & (1 << (port_id % 8))) {
				int_val = int_val ^ (1 << (port_id % 8));
				wcd9xxx_interface_reg_write(pahu->wcd9xxx,
					reg, int_val);
			}
		}
		if (val & WCD9360_SLIM_IRQ_PORT_CLOSED) {
			/*
			 * INT SOURCE register starts from RX to TX
			 * but port number in the ch_mask is in opposite way
			 */
			bit = (tx ? j - 16 : j + 16);
			dev_dbg(pahu->dev, "%s: %s port %d closed value %x, bit %u\n",
				 __func__, (tx ? "TX" : "RX"), port_id, val,
				 bit);
			for (k = 0, cleared = false; k < NUM_CODEC_DAIS; k++) {
				dev_dbg(pahu->dev, "%s: pahu->dai[%d].ch_mask = 0x%lx\n",
					 __func__, k, pahu->dai[k].ch_mask);
				if (test_and_clear_bit(bit,
						&pahu->dai[k].ch_mask)) {
					cleared = true;
					if (!pahu->dai[k].ch_mask)
						wake_up(
						      &pahu->dai[k].dai_wait);
					/*
					 * There are cases when multiple DAIs
					 * might be using the same slimbus
					 * channel. Hence don't break here.
					 */
				}
			}
		}
		wcd9xxx_interface_reg_write(pahu->wcd9xxx,
					    WCD9360_SLIM_PGD_PORT_INT_CLR_RX_0 +
					    (j / 8),
					    1 << (j % 8));
	}

	return IRQ_HANDLED;
}

static int pahu_setup_irqs(struct pahu_priv *pahu)
{
	int ret = 0;
	struct snd_soc_codec *codec = pahu->codec;
	struct wcd9xxx *wcd9xxx = pahu->wcd9xxx;
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_SLIMBUS,
				  pahu_slimbus_irq, "SLIMBUS Slave", pahu);
	if (ret)
		dev_err(codec->dev, "%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_SLIMBUS);
	else
		pahu_slim_interface_init_reg(codec);

	/* Register for misc interrupts as well */
	ret = wcd9xxx_request_irq(core_res, WCD9360_IRQ_MISC,
				  pahu_misc_irq, "CDC MISC Irq", pahu);
	if (ret)
		dev_err(codec->dev, "%s: Failed to request cdc misc irq\n",
			__func__);

	return ret;
}

static void pahu_init_slim_slave_cfg(struct snd_soc_codec *codec)
{
	struct pahu_priv *priv = snd_soc_codec_get_drvdata(codec);
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

static void pahu_cleanup_irqs(struct pahu_priv *pahu)
{
	struct wcd9xxx *wcd9xxx = pahu->wcd9xxx;
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;

	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_SLIMBUS, pahu);
	wcd9xxx_free_irq(core_res, WCD9360_IRQ_MISC, pahu);
}

/*
 * wcd9360_get_micb_vout_ctl_val: converts micbias from volts to register value
 * @micb_mv: micbias in mv
 *
 * return register value converted
 */
int wcd9360_get_micb_vout_ctl_val(u32 micb_mv)
{
	/* min micbias voltage is 1V and maximum is 2.85V */
	if (micb_mv < 1000 || micb_mv > 2850) {
		pr_err("%s: unsupported micbias voltage\n", __func__);
		return -EINVAL;
	}

	return (micb_mv - 1000) / 50;
}
EXPORT_SYMBOL(wcd9360_get_micb_vout_ctl_val);

static int pahu_handle_pdata(struct pahu_priv *pahu,
			      struct wcd9xxx_pdata *pdata)
{
	struct snd_soc_codec *codec = pahu->codec;
	u8 mad_dmic_ctl_val;
	u8 anc_ctl_value;
	u32 dmic_clk_drv;
	int vout_ctl_1, vout_ctl_2, vout_ctl_3, vout_ctl_4;
	int rc = 0;

	if (!pdata) {
		dev_err(codec->dev, "%s: NULL pdata\n", __func__);
		return -ENODEV;
	}

	/* set micbias voltage */
	vout_ctl_1 = wcd9360_get_micb_vout_ctl_val(pdata->micbias.micb1_mv);
	vout_ctl_2 = wcd9360_get_micb_vout_ctl_val(pdata->micbias.micb2_mv);
	vout_ctl_3 = wcd9360_get_micb_vout_ctl_val(pdata->micbias.micb3_mv);
	vout_ctl_4 = wcd9360_get_micb_vout_ctl_val(pdata->micbias.micb4_mv);
	if (vout_ctl_1 < 0 || vout_ctl_2 < 0 ||
	    vout_ctl_3 < 0 || vout_ctl_4 < 0) {
		rc = -EINVAL;
		goto done;
	}
	snd_soc_update_bits(codec, WCD9360_ANA_MICB1, 0x3F, vout_ctl_1);
	snd_soc_update_bits(codec, WCD9360_ANA_MICB2, 0x3F, vout_ctl_2);
	snd_soc_update_bits(codec, WCD9360_ANA_MICB3, 0x3F, vout_ctl_3);
	snd_soc_update_bits(codec, WCD9360_ANA_MICB4, 0x3F, vout_ctl_4);

	if (pdata->dmic_sample_rate ==
	    WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED) {
		dev_info(codec->dev, "%s: dmic_rate invalid default = %d\n",
			__func__, WCD9XXX_DMIC_SAMPLE_RATE_4P8MHZ);
		pdata->dmic_sample_rate = WCD9XXX_DMIC_SAMPLE_RATE_4P8MHZ;
	}
	if (pdata->mad_dmic_sample_rate ==
	    WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED) {
		dev_info(codec->dev, "%s: mad_dmic_rate invalid default = %d\n",
			__func__, WCD9XXX_DMIC_SAMPLE_RATE_4P8MHZ);
		/*
		 * use dmic_sample_rate as the default for MAD
		 * if mad dmic sample rate is undefined
		 */
		pdata->mad_dmic_sample_rate = pdata->dmic_sample_rate;
	}

	if (pdata->dmic_clk_drv ==
	    WCD9XXX_DMIC_CLK_DRIVE_UNDEFINED) {
		pdata->dmic_clk_drv = WCD9360_DMIC_CLK_DRIVE_DEFAULT;
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

	snd_soc_update_bits(codec, WCD9360_TEST_DEBUG_PAD_DRVCTL_0,
			    0x0C, dmic_clk_drv << 2);

	/*
	 * Default the DMIC clk rates to mad_dmic_sample_rate,
	 * whereas, the anc/txfe dmic rates to dmic_sample_rate
	 * since the anc/txfe are independent of mad block.
	 */
	mad_dmic_ctl_val = pahu_get_dmic_clk_val(pahu->codec,
				pdata->mad_dmic_sample_rate);
	snd_soc_update_bits(codec, WCD9360_CPE_SS_DMIC0_CTL,
		0x0E, mad_dmic_ctl_val << 1);
	snd_soc_update_bits(codec, WCD9360_CPE_SS_DMIC1_CTL,
		0x0E, mad_dmic_ctl_val << 1);
	snd_soc_update_bits(codec, WCD9360_CPE_SS_DMIC2_CTL,
		0x0E, mad_dmic_ctl_val << 1);

	if (dmic_clk_drv == WCD9360_DMIC_CLK_DIV_2)
		anc_ctl_value = WCD9360_ANC_DMIC_X2_FULL_RATE;
	else
		anc_ctl_value = WCD9360_ANC_DMIC_X2_HALF_RATE;

	snd_soc_update_bits(codec, WCD9360_CDC_ANC0_MODE_2_CTL,
			    0x40, anc_ctl_value << 6);
	snd_soc_update_bits(codec, WCD9360_CDC_ANC0_MODE_2_CTL,
			    0x20, anc_ctl_value << 5);

done:
	return rc;
}

static void pahu_cdc_vote_svs(struct snd_soc_codec *codec, bool vote)
{
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	return pahu_vote_svs(pahu, vote);
}

static struct wcd_dsp_cdc_cb cdc_cb = {
	.cdc_clk_en = pahu_codec_internal_rco_ctrl,
	.cdc_vote_svs = pahu_cdc_vote_svs,
};

static int pahu_wdsp_initialize(struct snd_soc_codec *codec)
{
	struct wcd9xxx *control;
	struct pahu_priv *pahu;
	struct wcd_dsp_params params;
	int ret = 0;

	control = dev_get_drvdata(codec->dev->parent);
	pahu = snd_soc_codec_get_drvdata(codec);

	params.cb = &cdc_cb;
	params.irqs.cpe_ipc1_irq = WCD9360_IRQ_CPE1_INTR;
	params.irqs.cpe_err_irq = WCD9360_IRQ_CPE_ERROR;
	params.irqs.fatal_irqs = CPE_FATAL_IRQS;
	params.clk_rate = control->mclk_rate;
	params.dsp_instance = 0;

	wcd9360_dsp_cntl_init(codec, &params, &pahu->wdsp_cntl);
	if (!pahu->wdsp_cntl) {
		dev_err(pahu->dev, "%s: wcd-dsp-control init failed\n",
			__func__);
		ret = -EINVAL;
	}

	return ret;
}

static void pahu_mclk2_reg_defaults(struct pahu_priv *pahu)
{
	int i;
	struct snd_soc_codec *codec = pahu->codec;

	/* MCLK2 configuration */
	for (i = 0; i < ARRAY_SIZE(pahu_codec_mclk2_1_0_defaults); i++)
		snd_soc_update_bits(codec,
				pahu_codec_mclk2_1_0_defaults[i].reg,
				pahu_codec_mclk2_1_0_defaults[i].mask,
				pahu_codec_mclk2_1_0_defaults[i].val);
}

static int pahu_device_down(struct wcd9xxx *wcd9xxx)
{
	struct snd_soc_codec *codec;
	struct pahu_priv *priv;
	int count;

	codec = (struct snd_soc_codec *)(wcd9xxx->ssr_priv);
	priv = snd_soc_codec_get_drvdata(codec);
	if (priv->swr.ctrl_data)
		swrm_wcd_notify(priv->swr.ctrl_data[0].swr_pdev,
				SWR_DEVICE_DOWN, NULL);
	snd_soc_card_change_online_state(codec->component.card, 0);
	for (count = 0; count < NUM_CODEC_DAIS; count++)
		priv->dai[count].bus_down_in_recovery = true;
	wcd9360_dsp_ssr_event(priv->wdsp_cntl, WCD_CDC_DOWN_EVENT);
	wcd_resmgr_set_sido_input_src_locked(priv->resmgr,
					     SIDO_SOURCE_INTERNAL);

	return 0;
}

static int pahu_post_reset_cb(struct wcd9xxx *wcd9xxx)
{
	int i, ret = 0;
	struct wcd9xxx *control;
	struct snd_soc_codec *codec;
	struct pahu_priv *pahu;
	struct wcd9xxx_pdata *pdata;

	codec = (struct snd_soc_codec *)(wcd9xxx->ssr_priv);
	pahu = snd_soc_codec_get_drvdata(codec);
	control = dev_get_drvdata(codec->dev->parent);

	wcd9xxx_set_power_state(pahu->wcd9xxx,
				WCD_REGION_POWER_COLLAPSE_REMOVE,
				WCD9XXX_DIG_CORE_REGION_1);

	mutex_lock(&pahu->codec_mutex);

	pahu_vote_svs(pahu, true);
	pahu_slimbus_slave_port_cfg.slave_dev_intfdev_la =
				control->slim_slave->laddr;
	pahu_slimbus_slave_port_cfg.slave_dev_pgd_la =
					control->slim->laddr;
	pahu_init_slim_slave_cfg(codec);
	snd_soc_card_change_online_state(codec->component.card, 1);

	for (i = 0; i < PAHU_MAX_MICBIAS; i++)
		pahu->micb_ref[i] = 0;

	dev_dbg(codec->dev, "%s: MCLK Rate = %x\n",
		__func__, control->mclk_rate);

	pahu_update_reg_defaults(pahu);
	wcd_resmgr_post_ssr_v2(pahu->resmgr);
	__pahu_enable_efuse_sensing(pahu);
	pahu_mclk2_reg_defaults(pahu);

	__pahu_cdc_mclk_enable(pahu, true);
	regcache_mark_dirty(codec->component.regmap);
	regcache_sync(codec->component.regmap);
	__pahu_cdc_mclk_enable(pahu, false);

	pahu_update_cpr_defaults(pahu);

	pdata = dev_get_platdata(codec->dev->parent);
	ret = pahu_handle_pdata(pahu, pdata);
	if (ret < 0)
		dev_err(codec->dev, "%s: invalid pdata\n", __func__);

	pahu_cleanup_irqs(pahu);
	ret = pahu_setup_irqs(pahu);
	if (ret) {
		dev_err(codec->dev, "%s: pahu irq setup failed %d\n",
			__func__, ret);
		goto done;
	}

	pahu_set_spkr_mode(codec, pahu->swr.spkr_mode);
	/*
	 * Once the codec initialization is completed, the svs vote
	 * can be released allowing the codec to go to SVS2.
	 */
	pahu_vote_svs(pahu, false);
	wcd9360_dsp_ssr_event(pahu->wdsp_cntl, WCD_CDC_UP_EVENT);

done:
	mutex_unlock(&pahu->codec_mutex);
	return ret;
}

static int pahu_soc_codec_probe(struct snd_soc_codec *codec)
{
	struct wcd9xxx *control;
	struct pahu_priv *pahu;
	struct wcd9xxx_pdata *pdata;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	int i, ret;
	void *ptr = NULL;

	control = dev_get_drvdata(codec->dev->parent);

	dev_info(codec->dev, "%s()\n", __func__);
	pahu = snd_soc_codec_get_drvdata(codec);
	pahu->intf_type = wcd9xxx_get_intf_type();

	control->dev_down = pahu_device_down;
	control->post_reset = pahu_post_reset_cb;
	control->ssr_priv = (void *)codec;

	/* Resource Manager post Init */
	ret = wcd_resmgr_post_init(pahu->resmgr, &pahu_resmgr_cb, codec);
	if (ret) {
		dev_err(codec->dev, "%s: wcd resmgr post init failed\n",
			__func__);
		goto err;
	}

	pahu->fw_data = devm_kzalloc(codec->dev, sizeof(*(pahu->fw_data)),
				      GFP_KERNEL);
	if (!pahu->fw_data)
		goto err;

	set_bit(WCD9XXX_ANC_CAL, pahu->fw_data->cal_bit);
	set_bit(WCD9XXX_MAD_CAL, pahu->fw_data->cal_bit);

	ret = wcd_cal_create_hwdep(pahu->fw_data,
				   WCD9XXX_CODEC_HWDEP_NODE, codec);
	if (ret < 0) {
		dev_err(codec->dev, "%s hwdep failed %d\n", __func__, ret);
		goto err_hwdep;
	}

	pahu->codec = codec;
	for (i = 0; i < COMPANDER_MAX; i++)
		pahu->comp_enabled[i] = 0;

	pdata = dev_get_platdata(codec->dev->parent);
	ret = pahu_handle_pdata(pahu, pdata);
	if (ret < 0) {
		dev_err(codec->dev, "%s: bad pdata\n", __func__);
		goto err_hwdep;
	}

	ptr = devm_kzalloc(codec->dev, (sizeof(pahu_rx_chs) +
			   sizeof(pahu_tx_chs)), GFP_KERNEL);
	if (!ptr) {
		ret = -ENOMEM;
		goto err_hwdep;
	}

	snd_soc_dapm_add_routes(dapm, pahu_slim_audio_map,
			ARRAY_SIZE(pahu_slim_audio_map));
	for (i = 0; i < NUM_CODEC_DAIS; i++) {
		INIT_LIST_HEAD(&pahu->dai[i].wcd9xxx_ch_list);
		init_waitqueue_head(&pahu->dai[i].dai_wait);
	}
	pahu_slimbus_slave_port_cfg.slave_dev_intfdev_la =
				control->slim_slave->laddr;
	pahu_slimbus_slave_port_cfg.slave_dev_pgd_la =
				control->slim->laddr;
	pahu_slimbus_slave_port_cfg.slave_port_mapping[0] =
				WCD9360_TX13;
	pahu_init_slim_slave_cfg(codec);

	control->num_rx_port = WCD9360_RX_MAX;
	control->rx_chs = ptr;
	memcpy(control->rx_chs, pahu_rx_chs, sizeof(pahu_rx_chs));
	control->num_tx_port = WCD9360_TX_MAX;
	control->tx_chs = ptr + sizeof(pahu_rx_chs);
	memcpy(control->tx_chs, pahu_tx_chs, sizeof(pahu_tx_chs));

	ret = pahu_setup_irqs(pahu);
	if (ret) {
		dev_err(pahu->dev, "%s: pahu irq setup failed %d\n",
			__func__, ret);
		goto err_pdata;
	}

	for (i = 0; i < WCD9360_NUM_DECIMATORS; i++) {
		pahu->tx_hpf_work[i].pahu = pahu;
		pahu->tx_hpf_work[i].decimator = i;
		INIT_DELAYED_WORK(&pahu->tx_hpf_work[i].dwork,
				  pahu_tx_hpf_corner_freq_callback);

		pahu->tx_mute_dwork[i].pahu = pahu;
		pahu->tx_mute_dwork[i].decimator = i;
		INIT_DELAYED_WORK(&pahu->tx_mute_dwork[i].dwork,
				  pahu_tx_mute_update_callback);
	}

	pahu->spk_anc_dwork.pahu = pahu;
	INIT_DELAYED_WORK(&pahu->spk_anc_dwork.dwork,
			  pahu_spk_anc_update_callback);

	pahu_mclk2_reg_defaults(pahu);

	mutex_lock(&pahu->codec_mutex);
	snd_soc_dapm_disable_pin(dapm, "ANC EAR PA");
	snd_soc_dapm_disable_pin(dapm, "ANC EAR");
	snd_soc_dapm_enable_pin(dapm, "ANC SPK1 PA");
	mutex_unlock(&pahu->codec_mutex);

	snd_soc_dapm_ignore_suspend(dapm, "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AIF1 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "AIF3 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AIF3 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "AIF4 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AIF4 MAD TX");
	snd_soc_dapm_ignore_suspend(dapm, "VIfeed");
	snd_soc_dapm_ignore_suspend(dapm, "I2S1 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "I2S1 Capture");

	snd_soc_dapm_sync(dapm);

	pahu_wdsp_initialize(codec);

	/*
	 * Once the codec initialization is completed, the svs vote
	 * can be released allowing the codec to go to SVS2.
	 */
	pahu_vote_svs(pahu, false);

	return ret;

err_pdata:
	devm_kfree(codec->dev, ptr);
	control->rx_chs = NULL;
	control->tx_chs = NULL;
err_hwdep:
	devm_kfree(codec->dev, pahu->fw_data);
	pahu->fw_data = NULL;
err:
	return ret;
}

static int pahu_soc_codec_remove(struct snd_soc_codec *codec)
{
	struct wcd9xxx *control;
	struct pahu_priv *pahu = snd_soc_codec_get_drvdata(codec);

	control = dev_get_drvdata(codec->dev->parent);
	devm_kfree(codec->dev, control->rx_chs);
	/* slimslave deinit in wcd core looks for this value */
	control->num_rx_port = 0;
	control->num_tx_port = 0;
	control->rx_chs = NULL;
	control->tx_chs = NULL;
	pahu_cleanup_irqs(pahu);

	if (pahu->wdsp_cntl)
		wcd9360_dsp_cntl_deinit(&pahu->wdsp_cntl);

	return 0;
}

static struct regmap *pahu_get_regmap(struct device *dev)
{
	struct wcd9xxx *control = dev_get_drvdata(dev->parent);

	return control->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_pahu = {
	.probe = pahu_soc_codec_probe,
	.remove = pahu_soc_codec_remove,
	.get_regmap = pahu_get_regmap,
	.component_driver = {
		.controls = pahu_snd_controls,
		.num_controls = ARRAY_SIZE(pahu_snd_controls),
		.dapm_widgets = pahu_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(pahu_dapm_widgets),
		.dapm_routes = pahu_audio_map,
		.num_dapm_routes = ARRAY_SIZE(pahu_audio_map),
	},
};

#ifdef CONFIG_PM
static int pahu_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pahu_priv *pahu = platform_get_drvdata(pdev);

	if (!pahu) {
		dev_err(dev, "%s: pahu private data is NULL\n", __func__);
		return -EINVAL;
	}
	dev_dbg(dev, "%s: system suspend\n", __func__);
	if (delayed_work_pending(&pahu->power_gate_work) &&
	    cancel_delayed_work_sync(&pahu->power_gate_work))
		pahu_codec_power_gate_digital_core(pahu);
	return 0;
}

static int pahu_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pahu_priv *pahu = platform_get_drvdata(pdev);

	if (!pahu) {
		dev_err(dev, "%s: pahu private data is NULL\n", __func__);
		return -EINVAL;
	}
	dev_dbg(dev, "%s: system resume\n", __func__);
	return 0;
}

static const struct dev_pm_ops pahu_pm_ops = {
	.suspend = pahu_suspend,
	.resume = pahu_resume,
};
#endif

static int pahu_swrm_read(void *handle, int reg)
{
	struct pahu_priv *pahu;
	struct wcd9xxx *wcd9xxx;
	unsigned short swr_rd_addr_base;
	unsigned short swr_rd_data_base;
	int val, ret;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	pahu = (struct pahu_priv *)handle;
	wcd9xxx = pahu->wcd9xxx;

	dev_dbg(pahu->dev, "%s: Reading soundwire register, 0x%x\n",
		__func__, reg);
	swr_rd_addr_base = WCD9360_SWR_AHB_BRIDGE_RD_ADDR_0;
	swr_rd_data_base = WCD9360_SWR_AHB_BRIDGE_RD_DATA_0;

	mutex_lock(&pahu->swr.read_mutex);
	ret = regmap_bulk_write(wcd9xxx->regmap, swr_rd_addr_base,
				 (u8 *)&reg, 4);
	if (ret < 0) {
		dev_err(pahu->dev, "%s: RD Addr Failure\n", __func__);
		goto done;
	}
	ret = regmap_bulk_read(wcd9xxx->regmap, swr_rd_data_base,
				(u8 *)&val, 4);
	if (ret < 0) {
		dev_err(pahu->dev, "%s: RD Data Failure\n", __func__);
		goto done;
	}
	ret = val;
done:
	mutex_unlock(&pahu->swr.read_mutex);

	return ret;
}

static int pahu_swrm_bulk_write(void *handle, u32 *reg, u32 *val, size_t len)
{
	struct pahu_priv *pahu;
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
	pahu = (struct pahu_priv *)handle;
	wcd9xxx = pahu->wcd9xxx;

	swr_wr_addr_base = WCD9360_SWR_AHB_BRIDGE_WR_ADDR_0;
	swr_wr_data_base = WCD9360_SWR_AHB_BRIDGE_WR_DATA_0;

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

	mutex_lock(&pahu->swr.write_mutex);
	ret = wcd9xxx_slim_bulk_write(wcd9xxx, bulk_reg,
			 (len * 2), false);
	if (ret) {
		dev_err(pahu->dev, "%s: swrm bulk write failed, ret: %d\n",
			__func__, ret);
	}
	mutex_unlock(&pahu->swr.write_mutex);

	kfree(bulk_reg);
	return ret;
}

static int pahu_swrm_write(void *handle, int reg, int val)
{
	struct pahu_priv *pahu;
	struct wcd9xxx *wcd9xxx;
	unsigned short swr_wr_addr_base;
	unsigned short swr_wr_data_base;
	struct wcd9xxx_reg_val bulk_reg[2];
	int ret;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	pahu = (struct pahu_priv *)handle;
	wcd9xxx = pahu->wcd9xxx;

	swr_wr_addr_base = WCD9360_SWR_AHB_BRIDGE_WR_ADDR_0;
	swr_wr_data_base = WCD9360_SWR_AHB_BRIDGE_WR_DATA_0;

	/* First Write the Data to register */
	bulk_reg[0].reg = swr_wr_data_base;
	bulk_reg[0].buf = (u8 *)(&val);
	bulk_reg[0].bytes = 4;
	bulk_reg[1].reg = swr_wr_addr_base;
	bulk_reg[1].buf = (u8 *)(&reg);
	bulk_reg[1].bytes = 4;

	mutex_lock(&pahu->swr.write_mutex);
	ret = wcd9xxx_slim_bulk_write(wcd9xxx, bulk_reg, 2, false);
	if (ret < 0)
		dev_err(pahu->dev, "%s: WR Data Failure\n", __func__);
	mutex_unlock(&pahu->swr.write_mutex);

	return ret;
}

static int pahu_swrm_clock(void *handle, bool enable)
{
	struct pahu_priv *pahu;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	pahu = (struct pahu_priv *)handle;

	mutex_lock(&pahu->swr.clk_mutex);
	dev_dbg(pahu->dev, "%s: swrm clock %s\n",
		__func__, (enable?"enable" : "disable"));
	if (enable) {
		pahu->swr.clk_users++;
		if (pahu->swr.clk_users == 1) {
			regmap_update_bits(pahu->wcd9xxx->regmap,
					WCD9360_TEST_DEBUG_NPL_DLY_TEST_1,
					0x10, 0x00);
			__pahu_cdc_mclk_enable(pahu, true);
			regmap_update_bits(pahu->wcd9xxx->regmap,
				WCD9360_CDC_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x01);
		}
	} else {
		pahu->swr.clk_users--;
		if (pahu->swr.clk_users == 0) {
			regmap_update_bits(pahu->wcd9xxx->regmap,
				WCD9360_CDC_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x00);
			__pahu_cdc_mclk_enable(pahu, false);
			regmap_update_bits(pahu->wcd9xxx->regmap,
					WCD9360_TEST_DEBUG_NPL_DLY_TEST_1,
					0x10, 0x10);
		}
	}
	dev_dbg(pahu->dev, "%s: swrm clock users %d\n",
		__func__, pahu->swr.clk_users);
	mutex_unlock(&pahu->swr.clk_mutex);

	return 0;
}

static int pahu_swrm_handle_irq(void *handle,
				 irqreturn_t (*swrm_irq_handler)(int irq,
								 void *data),
				 void *swrm_handle,
				 int action)
{
	struct pahu_priv *pahu;
	int ret = 0;
	struct wcd9xxx *wcd9xxx;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	pahu = (struct pahu_priv *) handle;
	wcd9xxx = pahu->wcd9xxx;

	if (action) {
		ret = wcd9xxx_request_irq(&wcd9xxx->core_res,
					  WCD9360_IRQ_SOUNDWIRE,
					  swrm_irq_handler,
					  "Pahu SWR Master", swrm_handle);
		if (ret)
			dev_err(pahu->dev, "%s: Failed to request irq %d\n",
				__func__, WCD9360_IRQ_SOUNDWIRE);
	} else
		wcd9xxx_free_irq(&wcd9xxx->core_res, WCD9360_IRQ_SOUNDWIRE,
				 swrm_handle);

	return ret;
}

static void pahu_codec_add_spi_device(struct pahu_priv *pahu,
				       struct device_node *node)
{
	struct spi_master *master;
	struct spi_device *spi;
	u32 prop_value;
	int rc;

	/* Read the master bus num from DT node */
	rc = of_property_read_u32(node, "qcom,master-bus-num",
				  &prop_value);
	if (rc < 0) {
		dev_err(pahu->dev, "%s: prop %s not found in node %s",
			__func__, "qcom,master-bus-num", node->full_name);
		goto done;
	}

	/* Get the reference to SPI master */
	master = spi_busnum_to_master(prop_value);
	if (!master) {
		dev_err(pahu->dev, "%s: Invalid spi_master for bus_num %u\n",
			__func__, prop_value);
		goto done;
	}

	/* Allocate the spi device */
	spi = spi_alloc_device(master);
	if (!spi) {
		dev_err(pahu->dev, "%s: spi_alloc_device failed\n",
			__func__);
		goto err_spi_alloc_dev;
	}

	/* Initialize device properties */
	if (of_modalias_node(node, spi->modalias,
			     sizeof(spi->modalias)) < 0) {
		dev_err(pahu->dev, "%s: cannot find modalias for %s\n",
			__func__, node->full_name);
		goto err_dt_parse;
	}

	rc = of_property_read_u32(node, "qcom,chip-select",
				  &prop_value);
	if (rc < 0) {
		dev_err(pahu->dev, "%s: prop %s not found in node %s",
			__func__, "qcom,chip-select", node->full_name);
		goto err_dt_parse;
	}
	spi->chip_select = prop_value;

	rc = of_property_read_u32(node, "qcom,max-frequency",
				  &prop_value);
	if (rc < 0) {
		dev_err(pahu->dev, "%s: prop %s not found in node %s",
			__func__, "qcom,max-frequency", node->full_name);
		goto err_dt_parse;
	}
	spi->max_speed_hz = prop_value;

	spi->dev.of_node = node;

	rc = spi_add_device(spi);
	if (rc < 0) {
		dev_err(pahu->dev, "%s: spi_add_device failed\n", __func__);
		goto err_dt_parse;
	}

	pahu->spi = spi;
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

static void pahu_add_child_devices(struct work_struct *work)
{
	struct pahu_priv *pahu;
	struct platform_device *pdev;
	struct device_node *node;
	struct wcd9xxx *wcd9xxx;
	struct pahu_swr_ctrl_data *swr_ctrl_data = NULL, *temp;
	int ret, ctrl_num = 0;
	struct wcd_swr_ctrl_platform_data *platdata;
	char plat_dev_name[WCD9360_STRING_LEN];

	pahu = container_of(work, struct pahu_priv,
			     pahu_add_child_devices_work);
	if (!pahu) {
		pr_err("%s: Memory for wcd9360 does not exist\n",
			__func__);
		return;
	}
	wcd9xxx = pahu->wcd9xxx;
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

	platdata = &pahu->swr.plat_data;
	pahu->child_count = 0;

	for_each_child_of_node(wcd9xxx->dev->of_node, node) {

		/* Parse and add the SPI device node */
		if (!strcmp(node->name, "wcd_spi")) {
			pahu_codec_add_spi_device(pahu, node);
			continue;
		}

		/* Parse other child device nodes and add platform device */
		if (!strcmp(node->name, "swr_master"))
			strlcpy(plat_dev_name, "pahu_swr_ctrl",
				(WCD9360_STRING_LEN - 1));
		else if (strnstr(node->name, "msm_cdc_pinctrl",
				 strlen("msm_cdc_pinctrl")) != NULL)
			strlcpy(plat_dev_name, node->name,
				(WCD9360_STRING_LEN - 1));
		else
			continue;

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(wcd9xxx->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err_mem;
		}
		pdev->dev.parent = pahu->dev;
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
					struct pahu_swr_ctrl_data),
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
			pahu->swr.ctrl_data = swr_ctrl_data;
		}
		if (pahu->child_count < WCD9360_CHILD_DEVICES_MAX)
			pahu->pdev_child_devices[pahu->child_count++] = pdev;
		else
			goto err_mem;
	}

	return;

err_pdev_add:
	platform_device_put(pdev);
err_mem:
	return;
}

static int __pahu_enable_efuse_sensing(struct pahu_priv *pahu)
{
	int val, rc;

	WCD9XXX_V2_BG_CLK_LOCK(pahu->resmgr);
	__pahu_cdc_mclk_enable_locked(pahu, true);

	regmap_update_bits(pahu->wcd9xxx->regmap,
			WCD9360_CHIP_TIER_CTRL_EFUSE_CTL, 0x1E, 0x10);
	regmap_update_bits(pahu->wcd9xxx->regmap,
			WCD9360_CHIP_TIER_CTRL_EFUSE_CTL, 0x01, 0x01);
	/*
	 * 5ms sleep required after enabling efuse control
	 * before checking the status.
	 */
	usleep_range(5000, 5500);
	wcd_resmgr_set_sido_input_src(pahu->resmgr,
					     SIDO_SOURCE_RCO_BG);

	WCD9XXX_V2_BG_CLK_UNLOCK(pahu->resmgr);

	rc = regmap_read(pahu->wcd9xxx->regmap,
			 WCD9360_CHIP_TIER_CTRL_EFUSE_STATUS, &val);
	if (rc || (!(val & 0x01)))
		WARN(1, "%s: Efuse sense is not complete val=%x, ret=%d\n",
			__func__, val, rc);

	__pahu_cdc_mclk_enable(pahu, false);

	return rc;
}

/*
 * pahu_get_wcd_dsp_cntl: Get the reference to wcd_dsp_cntl
 * @dev: Device pointer for codec device
 *
 * This API gets the reference to codec's struct wcd_dsp_cntl
 */
void *pahu_get_wcd_dsp_cntl(struct device *dev)
{
	struct platform_device *pdev;
	struct pahu_priv *pahu;

	if (!dev) {
		pr_err("%s: Invalid device\n", __func__);
		return NULL;
	}

	pdev = to_platform_device(dev);
	pahu = platform_get_drvdata(pdev);

	return pahu->wdsp_cntl;
}
EXPORT_SYMBOL(pahu_get_wcd_dsp_cntl);

static int pahu_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pahu_priv *pahu;
	struct clk *wcd_ext_clk;
	struct wcd9xxx_resmgr_v2 *resmgr;
	struct wcd9xxx_power_region *cdc_pwr;

	pahu = devm_kzalloc(&pdev->dev, sizeof(struct pahu_priv),
			    GFP_KERNEL);
	if (!pahu)
		return -ENOMEM;

	platform_set_drvdata(pdev, pahu);

	pahu->wcd9xxx = dev_get_drvdata(pdev->dev.parent);
	pahu->dev = &pdev->dev;
	INIT_DELAYED_WORK(&pahu->power_gate_work, pahu_codec_power_gate_work);
	mutex_init(&pahu->power_lock);
	INIT_WORK(&pahu->pahu_add_child_devices_work,
		  pahu_add_child_devices);
	mutex_init(&pahu->micb_lock);
	mutex_init(&pahu->swr.read_mutex);
	mutex_init(&pahu->swr.write_mutex);
	mutex_init(&pahu->swr.clk_mutex);
	mutex_init(&pahu->codec_mutex);
	mutex_init(&pahu->svs_mutex);

	/*
	 * Codec hardware by default comes up in SVS mode.
	 * Initialize the svs_ref_cnt to 1 to reflect the hardware
	 * state in the driver.
	 */
	pahu->svs_ref_cnt = 1;

	cdc_pwr = devm_kzalloc(&pdev->dev, sizeof(struct wcd9xxx_power_region),
				GFP_KERNEL);
	if (!cdc_pwr) {
		ret = -ENOMEM;
		goto err_resmgr;
	}
	pahu->wcd9xxx->wcd9xxx_pwr[WCD9XXX_DIG_CORE_REGION_1] = cdc_pwr;
	cdc_pwr->pwr_collapse_reg_min = WCD9360_DIG_CORE_REG_MIN;
	cdc_pwr->pwr_collapse_reg_max = WCD9360_DIG_CORE_REG_MAX;
	wcd9xxx_set_power_state(pahu->wcd9xxx,
				WCD_REGION_POWER_COLLAPSE_REMOVE,
				WCD9XXX_DIG_CORE_REGION_1);
	/*
	 * Init resource manager so that if child nodes such as SoundWire
	 * requests for clock, resource manager can honor the request
	 */
	resmgr = wcd_resmgr_init(&pahu->wcd9xxx->core_res, NULL);
	if (IS_ERR(resmgr)) {
		ret = PTR_ERR(resmgr);
		dev_err(&pdev->dev, "%s: Failed to initialize wcd resmgr\n",
			__func__);
		goto err_resmgr;
	}
	pahu->resmgr = resmgr;
	pahu->swr.plat_data.handle = (void *) pahu;
	pahu->swr.plat_data.read = pahu_swrm_read;
	pahu->swr.plat_data.write = pahu_swrm_write;
	pahu->swr.plat_data.bulk_write = pahu_swrm_bulk_write;
	pahu->swr.plat_data.clk = pahu_swrm_clock;
	pahu->swr.plat_data.handle_irq = pahu_swrm_handle_irq;
	pahu->swr.spkr_gain_offset = WCD9360_RX_GAIN_OFFSET_0_DB;

	/* Register for Clock */
	wcd_ext_clk = clk_get(pahu->wcd9xxx->dev, "wcd_clk");
	if (IS_ERR(wcd_ext_clk)) {
		dev_err(pahu->wcd9xxx->dev, "%s: clk get %s failed\n",
			__func__, "wcd_ext_clk");
		goto err_clk;
	}
	pahu->wcd_ext_clk = wcd_ext_clk;
	dev_dbg(&pdev->dev, "%s: MCLK Rate = %x\n", __func__,
		pahu->wcd9xxx->mclk_rate);
	/* Probe defer if mlck is failed */
	ret = clk_prepare_enable(pahu->wcd_ext_clk);
	if (ret) {
		dev_dbg(pahu->dev, "%s: ext clk enable failed\n",
			__func__);
		ret = -EPROBE_DEFER;
		goto err_cdc_reg;
	}
	clk_disable_unprepare(pahu->wcd_ext_clk);

	/* Update codec register default values */
	pahu_update_reg_defaults(pahu);
	__pahu_enable_efuse_sensing(pahu);
	pahu_update_cpr_defaults(pahu);

	/* Register with soc framework */
	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_pahu,
				  pahu_dai, ARRAY_SIZE(pahu_dai));
	if (ret) {
		dev_err(&pdev->dev, "%s: Codec registration failed\n",
		 __func__);
		goto err_cdc_reg;
	}
	schedule_work(&pahu->pahu_add_child_devices_work);

	return ret;

err_cdc_reg:
	clk_put(pahu->wcd_ext_clk);
err_clk:
	wcd_resmgr_remove(pahu->resmgr);
err_resmgr:
	mutex_destroy(&pahu->micb_lock);
	mutex_destroy(&pahu->svs_mutex);
	mutex_destroy(&pahu->codec_mutex);
	mutex_destroy(&pahu->swr.read_mutex);
	mutex_destroy(&pahu->swr.write_mutex);
	mutex_destroy(&pahu->swr.clk_mutex);
	devm_kfree(&pdev->dev, pahu);

	return ret;
}

static int pahu_remove(struct platform_device *pdev)
{
	struct pahu_priv *pahu;
	int count = 0;

	pahu = platform_get_drvdata(pdev);
	if (!pahu)
		return -EINVAL;

	if (pahu->spi)
		spi_unregister_device(pahu->spi);
	for (count = 0; count < pahu->child_count &&
				count < WCD9360_CHILD_DEVICES_MAX; count++)
		platform_device_unregister(pahu->pdev_child_devices[count]);

	mutex_destroy(&pahu->micb_lock);
	mutex_destroy(&pahu->svs_mutex);
	mutex_destroy(&pahu->codec_mutex);
	mutex_destroy(&pahu->swr.read_mutex);
	mutex_destroy(&pahu->swr.write_mutex);
	mutex_destroy(&pahu->swr.clk_mutex);

	snd_soc_unregister_codec(&pdev->dev);
	clk_put(pahu->wcd_ext_clk);
	wcd_resmgr_remove(pahu->resmgr);
	devm_kfree(&pdev->dev, pahu);
	return 0;
}

static struct platform_driver pahu_codec_driver = {
	.probe = pahu_probe,
	.remove = pahu_remove,
	.driver = {
		.name = "pahu_codec",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &pahu_pm_ops,
#endif
	},
};

module_platform_driver(pahu_codec_driver);

MODULE_DESCRIPTION("Pahu Codec driver");
MODULE_LICENSE("GPL v2");
