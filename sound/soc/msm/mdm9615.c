/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mfd/pm8xxx/pm8018.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <asm/mach-types.h>
#include <mach/socinfo.h>
#include "msm-pcm-routing.h"
#include "../codecs/wcd9310.h"
#include <mach/gpiomux.h>

/* 9615 machine driver */

#define PM8018_GPIO_BASE		NR_GPIO_IRQS
#define PM8018_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8018_GPIO_BASE)

#define MDM9615_SPK_ON 1
#define MDM9615_SPK_OFF 0

#define MDM9615_SLIM_0_RX_MAX_CHANNELS		2
#define MDM9615_SLIM_0_TX_MAX_CHANNELS		4

#define SAMPLE_RATE_8KHZ 8000
#define SAMPLE_RATE_16KHZ 16000

#define TOP_AND_BOTTOM_SPK_AMP_POS	0x1
#define TOP_AND_BOTTOM_SPK_AMP_NEG	0x2

#define GPIO_AUX_PCM_DOUT 23
#define GPIO_AUX_PCM_DIN 22
#define GPIO_AUX_PCM_SYNC 21
#define GPIO_AUX_PCM_CLK 20

#define GPIO_SEC_AUX_PCM_DOUT 28
#define GPIO_SEC_AUX_PCM_DIN 27
#define GPIO_SEC_AUX_PCM_SYNC 26
#define GPIO_SEC_AUX_PCM_CLK 25

#define TABLA_EXT_CLK_RATE 12288000

#define TABLA_MBHC_DEF_BUTTONS 8
#define TABLA_MBHC_DEF_RLOADS 5

#define PM8018_IRQ_BASE			(NR_MSM_IRQS + NR_GPIO_IRQS)
#define JACK_DETECT_GPIO 3
#define JACK_DETECT_INT PM8018_GPIO_IRQ(PM8018_IRQ_BASE, JACK_DETECT_GPIO)

/*
 * Added for I2S
 */
#define GPIO_SPKR_I2S_MCLK	24
#define GPIO_PRIM_I2S_SCK	20
#define GPIO_PRIM_I2S_DOUT	23
#define GPIO_PRIM_I2S_WS	21
#define GPIO_PRIM_I2S_DIN	22
#define GPIO_SEC_I2S_SCK	25
#define GPIO_SEC_I2S_WS		26
#define GPIO_SEC_I2S_DOUT	28
#define GPIO_SEC_I2S_DIN	27

static struct gpiomux_setting cdc_i2s_mclk = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting cdc_i2s_sclk = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting audio_sec_i2s[] = {
	/* Suspend state */
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv  = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	/* Active state */
	{
		.func = GPIOMUX_FUNC_2,
		.drv  = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	}
};

static struct gpiomux_setting cdc_i2s_dout = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting cdc_i2s_ws = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting cdc_i2s_din = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};


static struct msm_gpiomux_config msm9615_audio_prim_i2s_codec_configs[] = {
	{
		.gpio = GPIO_SPKR_I2S_MCLK,
		.settings = {
			[GPIOMUX_SUSPENDED] = &cdc_i2s_mclk,
		},
	},
	{
		.gpio = GPIO_PRIM_I2S_SCK,
		.settings = {
			[GPIOMUX_SUSPENDED] = &cdc_i2s_sclk,
		},
	},
	{
		.gpio = GPIO_PRIM_I2S_DOUT,
		.settings = {
			[GPIOMUX_SUSPENDED] = &cdc_i2s_dout,
		},
	},
	{
		.gpio = GPIO_PRIM_I2S_WS,
		.settings = {
			[GPIOMUX_SUSPENDED] = &cdc_i2s_ws,
		},
	},
	{
		.gpio = GPIO_PRIM_I2S_DIN,
		.settings = {
			[GPIOMUX_SUSPENDED] = &cdc_i2s_din,
		},
	},
};

static struct msm_gpiomux_config msm9615_audio_sec_i2s_codec_configs[] = {
	{
		.gpio = GPIO_SPKR_I2S_MCLK,
		.settings = {
			[GPIOMUX_SUSPENDED] = &cdc_i2s_mclk,
		},
	},
	{
		.gpio = GPIO_SEC_I2S_SCK,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_sec_i2s[0],
			[GPIOMUX_ACTIVE] = &audio_sec_i2s[1],
		},
	},
	{
		.gpio = GPIO_SEC_I2S_DOUT,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_sec_i2s[0],
			[GPIOMUX_ACTIVE] = &audio_sec_i2s[1],
		},
	},
	{
		.gpio = GPIO_SEC_I2S_WS,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_sec_i2s[0],
			[GPIOMUX_ACTIVE] = &audio_sec_i2s[1],
		},
	},
	{
		.gpio = GPIO_SEC_I2S_DIN,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_sec_i2s[0],
			[GPIOMUX_ACTIVE] = &audio_sec_i2s[1],
		},
	},
};
/* Physical address for LPA CSR
 * LPA SIF mux registers. These are
 * ioremap( ) for Virtual address.
 */
#define LPASS_CSR_BASE	0x28000000
#define LPA_IF_BASE		0x28100000
#define SIF_MUX_REG_BASE (LPASS_CSR_BASE + 0x00000000)
#define LPA_IF_REG_BASE (LPA_IF_BASE + 0x00000000)
#define LPASS_SIF_MUX_ADDR  (SIF_MUX_REG_BASE + 0x00004000)
#define LPAIF_SPARE_ADDR (LPA_IF_REG_BASE + 0x00000070)
#define SEC_PCM_PORT_SLC_ADDR 0x00802074
/* bits 2:0 should be updated with 100 to select SDC2 */
#define SEC_PCM_PORT_SLC_VALUE 0x4
/* SIF & SPARE MUX Values */
#define MSM_SIF_FUNC_PCM              0
#define MSM_SIF_FUNC_I2S_MIC        1
#define MSM_SIF_FUNC_I2S_SPKR      2
#define MSM_LPAIF_SPARE_DISABLE                 0x0
#define MSM_LPAIF_SPARE_BOTH_ENABLE     0x3

/* I2S INTF CTL */
#define MSM_INTF_PRIM        0
#define MSM_INTF_SECN        1
#define MSM_INTF_BOTH       2

/* I2S Dir CTL */
#define MSM_DIR_RX         0
#define MSM_DIR_TX         1
#define MSM_DIR_BOTH   2
#define MSM_DIR_MAX     3

/* I2S HW Params */
#define NO_OF_BITS_PER_SAMPLE  16
#define I2S_MIC_SCLK_RATE 1536000
static int msm9615_i2s_rx_ch = 1;
static int msm9615_i2s_tx_ch = 1;
static int msm9615_i2s_spk_control;

/* SIF mux bit mask & shift */
#define LPASS_SIF_MUX_CTL_PRI_MUX_SEL_BMSK                   0x30000
#define LPASS_SIF_MUX_CTL_PRI_MUX_SEL_SHFT                      0x10
#define LPASS_SIF_MUX_CTL_SEC_MUX_SEL_BMSK                       0x3
#define LPASS_SIF_MUX_CTL_SEC_MUX_SEL_SHFT                       0x0

#define LPAIF_SPARE_MUX_CTL_SEC_MUX_SEL_BMSK		0x3
#define LPAIF_SPARE_MUX_CTL_SEC_MUX_SEL_SHFT		0x2
#define LPAIF_SPARE_MUX_CTL_PRI_MUX_SEL_BMSK	0x3
#define LPAIF_SPARE_MUX_CTL_PRI_MUX_SEL_SHFT		0x0

static atomic_t msm9615_auxpcm_ref;
static atomic_t msm9615_sec_auxpcm_ref;

struct msm_i2s_mux_ctl {
	const u8 sifconfig;
	const u8 spareconfig;
};
struct msm_clk {
	struct clk *osr_clk;
	struct clk *bit_clk;
	int clk_enable;
};
struct msm_i2s_clk {
	struct msm_clk rx_clk;
	struct msm_clk tx_clk;
};
struct msm_i2s_ctl {
	struct msm_i2s_clk prim_clk;
	struct msm_i2s_clk sec_clk;
	struct msm_i2s_mux_ctl mux_ctl[MSM_DIR_MAX];
	u8 intf_status[MSM_INTF_BOTH][MSM_DIR_BOTH];
	void *sif_virt_addr;
	void *spare_virt_addr;
};
static struct msm_i2s_ctl msm9x15_i2s_ctl = {
	{{NULL, NULL, 0}, {NULL, NULL, 0} }, /* prim_clk */
	{{NULL, NULL, 0}, {NULL, NULL, 0} }, /* sec_clk */
	/* mux_ctl */
	{
		/* Rx path only */
		{ MSM_SIF_FUNC_I2S_SPKR, MSM_LPAIF_SPARE_DISABLE },
		/* Tx path only */
		{  MSM_SIF_FUNC_I2S_MIC, MSM_LPAIF_SPARE_DISABLE },
		/* Rx + Tx path only */
		{ MSM_SIF_FUNC_I2S_SPKR, MSM_LPAIF_SPARE_BOTH_ENABLE },
	},
	/* intf_status */
	{
		/* Prim I2S */
		{0, 0},
		/* Sec I2S */
		{0, 0}
	},
	/* sif_virt_addr */
	NULL,
	/* spare_virt_addr */
	NULL,
};

enum msm9x15_set_i2s_clk {
	MSM_I2S_CLK_SET_FALSE,
	MSM_I2S_CLK_SET_TRUE,
	MSM_I2S_CLK_SET_RATE0,
};
/*
 * Added for I2S
 */
static u32 top_and_bottom_spk_pamp_gpio = PM8018_GPIO_PM_TO_SYS(5);

void *sif_virt_addr;
void *secpcm_portslc_virt_addr;

static int mdm9615_spk_control;
static int mdm9615_ext_top_and_bottom_spk_pamp;
static int mdm9615_slim_0_rx_ch = 1;
static int mdm9615_slim_0_tx_ch = 1;

static int mdm9615_btsco_rate = SAMPLE_RATE_8KHZ;
static int mdm9615_btsco_ch = 1;

static int mdm9615_auxpcm_rate = SAMPLE_RATE_8KHZ;

static struct clk *codec_clk;
static int clk_users;

static struct snd_soc_jack hs_jack;
static struct snd_soc_jack button_jack;

static struct platform_device *mdm9615_snd_device_slim;
static struct platform_device *mdm9615_snd_device_i2s;

static u32 sif_reg_value   = 0x0000;
static u32 spare_reg_value = 0x0000;

static bool hs_detect_use_gpio;
module_param(hs_detect_use_gpio, bool, 0444);
MODULE_PARM_DESC(hs_detect_use_gpio, "Use GPIO for headset detection");

static bool hs_detect_use_firmware;
module_param(hs_detect_use_firmware, bool, 0444);
MODULE_PARM_DESC(hs_detect_use_firmware, "Use firmware for headset detection");

static int mdm9615_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
					bool dapm);
static struct tabla_mbhc_config mbhc_cfg = {
	.headset_jack = &hs_jack,
	.button_jack = &button_jack,
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = TABLA_MICBIAS2,
	.mclk_cb_fn = mdm9615_enable_codec_ext_clk,
	.mclk_rate = TABLA_EXT_CLK_RATE,
	.gpio = 0,
	.gpio_irq = 0,
	.gpio_level_insert = 1,
};

static void mdm9615_enable_ext_spk_amp_gpio(u32 spk_amp_gpio)
{
	int ret = 0;

	struct pm_gpio param = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 1,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = PM_GPIO_VIN_S4,
		.out_strength   = PM_GPIO_STRENGTH_MED,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	if (spk_amp_gpio == top_and_bottom_spk_pamp_gpio) {

		ret = gpio_request(top_and_bottom_spk_pamp_gpio,
				   "TOP_AND_BOTTOM_SPK_AMP");
		if (ret) {
			pr_err("%s: Error requesting TOP AND BOTTOM SPK AMP GPIO %u\n",
				__func__, top_and_bottom_spk_pamp_gpio);
			return;
		}
		ret = pm8xxx_gpio_config(top_and_bottom_spk_pamp_gpio, &param);
		if (ret)
			pr_err("%s: Failed to configure Top & Bottom Spk Ampl\n"
				"gpio %u\n", __func__,
				top_and_bottom_spk_pamp_gpio);
		else {
			pr_debug("%s: enable Top & Bottom spkr amp gpio\n",
				 __func__);
			gpio_direction_output(top_and_bottom_spk_pamp_gpio, 1);
		}

	} else {
		pr_err("%s: ERROR : Invalid External Speaker Ampl GPIO."
			" gpio = %u\n", __func__, spk_amp_gpio);
		return;
	}
}

static void mdm9615_ext_spk_power_amp_on(u32 spk)
{
	if (spk & (TOP_AND_BOTTOM_SPK_AMP_POS | TOP_AND_BOTTOM_SPK_AMP_NEG)) {
		if ((mdm9615_ext_top_and_bottom_spk_pamp &
		     TOP_AND_BOTTOM_SPK_AMP_POS) &&
		    (mdm9615_ext_top_and_bottom_spk_pamp &
		     TOP_AND_BOTTOM_SPK_AMP_NEG)) {
			pr_debug("%s() External Speaker Ampl already "
				"turned on. spk = 0x%08x\n", __func__, spk);
			return;
		}

		mdm9615_ext_top_and_bottom_spk_pamp |= spk;

		if ((mdm9615_ext_top_and_bottom_spk_pamp &
		     TOP_AND_BOTTOM_SPK_AMP_POS) &&
		    (mdm9615_ext_top_and_bottom_spk_pamp &
		     TOP_AND_BOTTOM_SPK_AMP_NEG)) {
			mdm9615_enable_ext_spk_amp_gpio(
					top_and_bottom_spk_pamp_gpio);
			pr_debug("%s: slepping 4 ms after turning on external\n"
				"Speaker Ampl\n", __func__);
			usleep_range(4000, 4000);
		}

	} else {
		pr_err("%s: ERROR : Invalid External Speaker Ampl. spk = 0x%08x\n",
			__func__, spk);
		return;
	}
}

static void mdm9615_ext_spk_power_amp_off(u32 spk)
{
	if (spk & (TOP_AND_BOTTOM_SPK_AMP_POS | TOP_AND_BOTTOM_SPK_AMP_NEG)) {

		if (!mdm9615_ext_top_and_bottom_spk_pamp)
			return;

		gpio_direction_output(top_and_bottom_spk_pamp_gpio, 0);
		gpio_free(top_and_bottom_spk_pamp_gpio);
		mdm9615_ext_top_and_bottom_spk_pamp = 0;

		pr_debug("%s: sleeping 4 ms after turning off external Bottom"
			" Speaker Ampl\n", __func__);

		usleep_range(4000, 4000);

	} else  {

		pr_err("%s: ERROR : Invalid Ext Spk Ampl. spk = 0x%08x\n",
			__func__, spk);
		return;
	}
}

static void mdm9615_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	pr_debug("%s: mdm9615_spk_control = %d", __func__, mdm9615_spk_control);
	if (mdm9615_spk_control == MDM9615_SPK_ON) {
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Pos");
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Neg");
	} else {
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Pos");
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Neg");
	}

	snd_soc_dapm_sync(dapm);
}

static int mdm9615_get_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm9615_spk_control = %d", __func__, mdm9615_spk_control);
	ucontrol->value.integer.value[0] = mdm9615_spk_control;
	return 0;
}
static int mdm9615_set_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (mdm9615_spk_control == ucontrol->value.integer.value[0])
		return 0;

	mdm9615_spk_control = ucontrol->value.integer.value[0];
	mdm9615_ext_control(codec);
	return 1;
}
static int mdm9615_spkramp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{
	pr_debug("%s() %x\n", __func__, SND_SOC_DAPM_EVENT_ON(event));

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (!strncmp(w->name, "Ext Spk Pos", 11))
			mdm9615_ext_spk_power_amp_on(
					TOP_AND_BOTTOM_SPK_AMP_POS);
		else if (!strncmp(w->name, "Ext Spk Neg", 11))
			mdm9615_ext_spk_power_amp_on(
					TOP_AND_BOTTOM_SPK_AMP_NEG);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			return -EINVAL;
		}

	} else {
		if (!strncmp(w->name, "Ext Spk Pos", 11))
			mdm9615_ext_spk_power_amp_off(
					TOP_AND_BOTTOM_SPK_AMP_POS);
		else if (!strncmp(w->name, "Ext Spk Neg", 11))
			mdm9615_ext_spk_power_amp_off(
					TOP_AND_BOTTOM_SPK_AMP_NEG);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			return -EINVAL;
		}
	}
	return 0;
}
static int mdm9615_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
					bool dapm)
{
	pr_debug("%s: enable = %d\n", __func__, enable);
	if (enable) {
		clk_users++;
		pr_debug("%s: clk_users = %d\n", __func__, clk_users);
		if (clk_users != 1)
			return 0;
		if (IS_ERR(codec_clk)) {

			pr_err("%s: Error setting Tabla MCLK\n", __func__);
			clk_users--;
			return -EINVAL;
		}
		clk_set_rate(codec_clk, TABLA_EXT_CLK_RATE);
		clk_prepare_enable(codec_clk);
		tabla_mclk_enable(codec, 1, dapm);
	} else {
		pr_debug("%s: clk_users = %d\n", __func__, clk_users);
		if (clk_users == 0)
			return 0;
		clk_users--;
		if (!clk_users) {
			pr_debug("%s: disabling MCLK. clk_users = %d\n",
					 __func__, clk_users);
			tabla_mclk_enable(codec, 0, dapm);
			clk_disable_unprepare(codec_clk);
		}
	}
	return 0;
}

static int mdm9615_mclk_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return mdm9615_enable_codec_ext_clk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return mdm9615_enable_codec_ext_clk(w->codec, 0, true);
	}
	return 0;
}

static const struct snd_soc_dapm_widget mdm9615_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	mdm9615_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Ext Spk Pos", mdm9615_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Neg", mdm9615_spkramp_event),

	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),

	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),

};

static const struct snd_soc_dapm_route common_audio_map[] = {

	{"RX_BIAS", NULL, "MCLK"},
	{"LDO_H", NULL, "MCLK"},

	/* Speaker path */
	{"Ext Spk Pos", NULL, "LINEOUT1"},
	{"Ext Spk Neg", NULL, "LINEOUT3"},

	{"Ext Spk Pos", NULL, "LINEOUT2"},
	{"Ext Spk Neg", NULL, "LINEOUT4"},

	/* Microphone path */
	{"AMIC1", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Handset Mic"},

	{"AMIC2", NULL, "MIC BIAS2 External"},
	{"MIC BIAS2 External", NULL, "Headset Mic"},

	/**
	 * AMIC3 and AMIC4 inputs are connected to ANC microphones
	 * These mics are biased differently on CDP and FLUID
	 * routing entries below are based on bias arrangement
	 * on FLUID.
	 */
	{"AMIC3", NULL, "MIC BIAS3 Internal1"},
	{"MIC BIAS3 Internal1", NULL, "ANCRight Headset Mic"},

	{"AMIC4", NULL, "MIC BIAS1 Internal2"},
	{"MIC BIAS1 Internal2", NULL, "ANCLeft Headset Mic"},

	{"HEADPHONE", NULL, "LDO_H"},

	/**
	 * The digital Mic routes are setup considering
	 * fluid as default device.
	 */

	/**
	 * Digital Mic1. Front Bottom left Digital Mic on Fluid and MTP.
	 * Digital Mic GM5 on CDP mainboard.
	 * Conncted to DMIC2 Input on Tabla codec.
	 */
	{"DMIC2", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic1"},

	/**
	 * Digital Mic2. Front Bottom right Digital Mic on Fluid and MTP.
	 * Digital Mic GM6 on CDP mainboard.
	 * Conncted to DMIC1 Input on Tabla codec.
	 */
	{"DMIC1", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic2"},

	/**
	 * Digital Mic3. Back Bottom Digital Mic on Fluid.
	 * Digital Mic GM1 on CDP mainboard.
	 * Conncted to DMIC4 Input on Tabla codec.
	 */
	{"DMIC4", NULL, "MIC BIAS3 External"},
	{"MIC BIAS3 External", NULL, "Digital Mic3"},

	/**
	 * Digital Mic4. Back top Digital Mic on Fluid.
	 * Digital Mic GM2 on CDP mainboard.
	 * Conncted to DMIC3 Input on Tabla codec.
	 */
	{"DMIC3", NULL, "MIC BIAS3 External"},
	{"MIC BIAS3 External", NULL, "Digital Mic4"},

	/**
	 * Digital Mic5. Front top Digital Mic on Fluid.
	 * Digital Mic GM3 on CDP mainboard.
	 * Conncted to DMIC5 Input on Tabla codec.
	 */
	{"DMIC5", NULL, "MIC BIAS4 External"},
	{"MIC BIAS4 External", NULL, "Digital Mic5"},

	/* Tabla digital Mic6 - back bottom digital Mic on Liquid and
	 * bottom mic on CDP. FLUID/MTP do not have dmic6 installed.
	 */
	{"DMIC6", NULL, "MIC BIAS4 External"},
	{"MIC BIAS4 External", NULL, "Digital Mic6"},
};

static const char *spk_function[] = {"Off", "On"};
static const char *slim0_rx_ch_text[] = {"One", "Two"};
static const char *slim0_tx_ch_text[] = {"One", "Two", "Three", "Four"};

static const struct soc_enum mdm9615_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, slim0_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(4, slim0_tx_ch_text),
};

static const char *btsco_rate_text[] = {"8000", "16000"};
static const struct soc_enum mdm9615_btsco_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, btsco_rate_text),
};

static const char * const auxpcm_rate_text[] = {"rate_8000", "rate_16000"};
static const struct soc_enum mdm9615_auxpcm_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
};

static int mdm9615_slim_0_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm9615_slim_0_rx_ch  = %d\n", __func__,
		 mdm9615_slim_0_rx_ch);
	ucontrol->value.integer.value[0] = mdm9615_slim_0_rx_ch - 1;
	return 0;
}

static int mdm9615_slim_0_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mdm9615_slim_0_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: mdm9615_slim_0_rx_ch = %d\n", __func__,
		 mdm9615_slim_0_rx_ch);
	return 1;
}

static int mdm9615_slim_0_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm9615_slim_0_tx_ch  = %d\n", __func__,
		 mdm9615_slim_0_tx_ch);
	ucontrol->value.integer.value[0] = mdm9615_slim_0_tx_ch - 1;
	return 0;
}

static int mdm9615_slim_0_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mdm9615_slim_0_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: mdm9615_slim_0_tx_ch = %d\n", __func__,
		 mdm9615_slim_0_tx_ch);
	return 1;
}

static int mdm9615_btsco_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm9615_btsco_rate  = %d", __func__, mdm9615_btsco_rate);
	ucontrol->value.integer.value[0] = mdm9615_btsco_rate;
	return 0;
}

static int mdm9615_btsco_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 8000:
		mdm9615_btsco_rate = SAMPLE_RATE_8KHZ;
		break;
	case 16000:
		mdm9615_btsco_rate = SAMPLE_RATE_16KHZ;
		break;
	default:
		mdm9615_btsco_rate = SAMPLE_RATE_8KHZ;
		break;
	}
	pr_debug("%s: mdm9615_btsco_rate = %d\n", __func__, mdm9615_btsco_rate);
	return 0;
}

static int mdm9615_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm9615_auxpcm_rate  = %d", __func__,
		mdm9615_auxpcm_rate);
	ucontrol->value.integer.value[0] = mdm9615_auxpcm_rate;
	return 0;
}

static int mdm9615_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm9615_auxpcm_rate = SAMPLE_RATE_8KHZ;
		break;
	case 1:
		mdm9615_auxpcm_rate = SAMPLE_RATE_16KHZ;
		break;
	default:
		mdm9615_auxpcm_rate = SAMPLE_RATE_8KHZ;
		break;
	}
	pr_debug("%s: mdm9615_auxpcm_rate = %d\n"
		 "ucontrol->value.integer.value[0] = %d\n", __func__,
		 mdm9615_auxpcm_rate,
		 (int)ucontrol->value.integer.value[0]);
	return 0;
}

static const struct snd_kcontrol_new tabla_mdm9615_controls[] = {
	SOC_ENUM_EXT("Speaker Function", mdm9615_enum[0], mdm9615_get_spk,
		mdm9615_set_spk),
	SOC_ENUM_EXT("SLIM_0_RX Channels", mdm9615_enum[1],
		mdm9615_slim_0_rx_ch_get, mdm9615_slim_0_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", mdm9615_enum[2],
		mdm9615_slim_0_tx_ch_get, mdm9615_slim_0_tx_ch_put),
	SOC_ENUM_EXT("Internal BTSCO SampleRate", mdm9615_btsco_enum[0],
		     mdm9615_btsco_rate_get, mdm9615_btsco_rate_put),
	SOC_ENUM_EXT("AUX PCM SampleRate", mdm9615_auxpcm_enum[0],
		mdm9615_auxpcm_rate_get, mdm9615_auxpcm_rate_put),
};

static void *def_tabla_mbhc_cal(void)
{
	void *tabla_cal;
	struct tabla_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;
	u8 *n_ready, *n_cic, *gain;

	tabla_cal = kzalloc(TABLA_MBHC_CAL_SIZE(TABLA_MBHC_DEF_BUTTONS,
						TABLA_MBHC_DEF_RLOADS),
			    GFP_KERNEL);
	if (!tabla_cal) {
		pr_err("%s: out of memory\n", __func__);
		return NULL;
	}

#define S(X, Y) ((TABLA_MBHC_CAL_GENERAL_PTR(tabla_cal)->X) = (Y))
	S(t_ldoh, 100);
	S(t_bg_fast_settle, 100);
	S(t_shutdown_plug_rem, 255);
	S(mbhc_nsa, 4);
	S(mbhc_navg, 4);
#undef S
#define S(X, Y) ((TABLA_MBHC_CAL_PLUG_DET_PTR(tabla_cal)->X) = (Y))
	S(mic_current, TABLA_PID_MIC_5_UA);
	S(hph_current, TABLA_PID_MIC_5_UA);
	S(t_mic_pid, 100);
	S(t_ins_complete, 250);
	S(t_ins_retry, 200);
#undef S
#define S(X, Y) ((TABLA_MBHC_CAL_PLUG_TYPE_PTR(tabla_cal)->X) = (Y))
	S(v_no_mic, 30);
	S(v_hs_max, 1550);
#undef S
#define S(X, Y) ((TABLA_MBHC_CAL_BTN_DET_PTR(tabla_cal)->X) = (Y))
	S(c[0], 62);
	S(c[1], 124);
	S(nc, 1);
	S(n_meas, 3);
	S(mbhc_nsc, 11);
	S(n_btn_meas, 1);
	S(n_btn_con, 2);
	S(num_btn, TABLA_MBHC_DEF_BUTTONS);
	S(v_btn_press_delta_sta, 100);
	S(v_btn_press_delta_cic, 50);
#undef S
	btn_cfg = TABLA_MBHC_CAL_BTN_DET_PTR(tabla_cal);
	btn_low = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_V_BTN_LOW);
	btn_high = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_V_BTN_HIGH);
	btn_low[0] = -50;
	btn_high[0] = 10;
	btn_low[1] = 11;
	btn_high[1] = 38;
	btn_low[2] = 39;
	btn_high[2] = 64;
	btn_low[3] = 65;
	btn_high[3] = 91;
	btn_low[4] = 92;
	btn_high[4] = 115;
	btn_low[5] = 116;
	btn_high[5] = 141;
	btn_low[6] = 142;
	btn_high[6] = 163;
	btn_low[7] = 164;
	btn_high[7] = 250;
	n_ready = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_N_READY);
	n_ready[0] = 48;
	n_ready[1] = 38;
	n_cic = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_N_CIC);
	n_cic[0] = 60;
	n_cic[1] = 47;
	gain = tabla_mbhc_cal_btn_det_mp(btn_cfg, TABLA_BTN_DET_GAIN);
	gain[0] = 11;
	gain[1] = 9;

	return tabla_cal;
}

static int msm9615_i2s_set_spk(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (msm9615_i2s_spk_control == ucontrol->value.integer.value[0])
		return 0;

	msm9615_i2s_spk_control = ucontrol->value.integer.value[0];
	mdm9615_ext_control(codec);
	return 1;
}

static int mdm9615_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	unsigned int rx_ch_cnt = 0, tx_ch_cnt = 0;

	pr_debug("%s: ch=%d\n", __func__,
					mdm9615_slim_0_rx_ch);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
				mdm9615_slim_0_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
	} else {
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai,
				mdm9615_slim_0_tx_ch, tx_ch, 0 , 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
	}
end:
	return ret;
}

static int msm9615_i2s_rx_ch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm9615_i2s_rx_ch  = %d\n", __func__,
		 msm9615_i2s_rx_ch);
	ucontrol->value.integer.value[0] = msm9615_i2s_rx_ch - 1;
	return 0;
}

static int msm9615_i2s_rx_ch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	msm9615_i2s_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm9615_i2s_rx_ch = %d\n", __func__,
		 msm9615_i2s_rx_ch);
	return 1;
}

static int msm9615_i2s_tx_ch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm9615_i2s_tx_ch  = %d\n", __func__,
		 msm9615_i2s_tx_ch);
	ucontrol->value.integer.value[0] = msm9615_i2s_tx_ch - 1;
	return 0;
}

static int msm9615_i2s_tx_ch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	msm9615_i2s_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm9615_i2s_tx_ch = %d\n", __func__,
		 msm9615_i2s_tx_ch);
	return 1;
}

static int msm9615_i2s_get_spk(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm9615_spk_control = %d", __func__, mdm9615_spk_control);
	ucontrol->value.integer.value[0] = msm9615_i2s_spk_control;
	return 0;
}

static const struct snd_kcontrol_new tabla_msm9615_i2s_controls[] = {
	SOC_ENUM_EXT("Speaker Function", mdm9615_enum[0], msm9615_i2s_get_spk,
		     msm9615_i2s_set_spk),
	SOC_ENUM_EXT("PRI_RX Channels", mdm9615_enum[1],
		     msm9615_i2s_rx_ch_get, msm9615_i2s_rx_ch_put),
	SOC_ENUM_EXT("PRI_TX Channels", mdm9615_enum[2],
		     msm9615_i2s_tx_ch_get, msm9615_i2s_tx_ch_put),
	SOC_ENUM_EXT("SEC_RX Channels", mdm9615_enum[3],
			msm9615_i2s_rx_ch_get, msm9615_i2s_rx_ch_put),
	SOC_ENUM_EXT("SEC_TX Channels", mdm9615_enum[4],
			msm9615_i2s_tx_ch_get, msm9615_i2s_tx_ch_put),
};

static int msm9615_i2s_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	snd_soc_dapm_new_controls(dapm, mdm9615_dapm_widgets,
				  ARRAY_SIZE(mdm9615_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, common_audio_map,
		ARRAY_SIZE(common_audio_map));
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Pos");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Neg");

	snd_soc_dapm_sync(dapm);

	err = snd_soc_jack_new(codec, "Headset Jack",
			       (SND_JACK_HEADSET | SND_JACK_OC_HPHL|
			       SND_JACK_OC_HPHR), &hs_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	err = snd_soc_jack_new(codec, "Button Jack",
			       TABLA_JACK_BUTTON_MASK, &button_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	codec_clk = clk_get(cpu_dai->dev, "osr_clk");
	err = tabla_hs_detect(codec, &mbhc_cfg);
	msm_gpiomux_install(
			msm9615_audio_prim_i2s_codec_configs,
			ARRAY_SIZE(msm9615_audio_prim_i2s_codec_configs));
	return err;
}

static int msm9615_i2s_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm9615_i2s_rx_ch;

	return 0;
}

static int msm9615_i2s_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);
	rate->min = rate->max = 48000;

	channels->min = channels->max = msm9615_i2s_tx_ch;

	return 0;
}

static int mdm9615_i2s_free_gpios(u8 i2s_intf, u8 i2s_dir)
{
	struct msm_i2s_ctl *pintf = &msm9x15_i2s_ctl;
	if (i2s_intf == MSM_INTF_PRIM) {
		if (pintf->intf_status[i2s_intf][MSM_DIR_TX] == 0 &&
			pintf->intf_status[i2s_intf][MSM_DIR_RX] == 0) {
			gpio_free(GPIO_PRIM_I2S_DIN);
			gpio_free(GPIO_PRIM_I2S_DOUT);
			gpio_free(GPIO_PRIM_I2S_SCK);
			gpio_free(GPIO_PRIM_I2S_WS);
		}
	} else if (i2s_intf == MSM_INTF_SECN) {
		if (pintf->intf_status[i2s_intf][MSM_DIR_TX] == 0 &&
			pintf->intf_status[i2s_intf][MSM_DIR_RX] == 0) {
			gpio_free(GPIO_SEC_I2S_DOUT);
			gpio_free(GPIO_SEC_I2S_WS);
			gpio_free(GPIO_SEC_I2S_DIN);
			gpio_free(GPIO_SEC_I2S_SCK);
		}
	}
	return 0;
}

static int msm9615_i2s_intf_dir_sel(const char *cpu_dai_name,
			     u8 *i2s_intf, u8 *i2s_dir)
{
	int ret = 0;
	if (i2s_intf == NULL || i2s_dir == NULL || cpu_dai_name == NULL) {
		ret = 1;
		goto err;
	}
	if (!strncmp(cpu_dai_name, "msm-dai-q6.0", 12)) {
		*i2s_intf = MSM_INTF_PRIM;
		*i2s_dir = MSM_DIR_RX;
	} else if (!strncmp(cpu_dai_name, "msm-dai-q6.1", 12)) {
		*i2s_intf = MSM_INTF_PRIM;
		*i2s_dir = MSM_DIR_TX;
	} else if (!strncmp(cpu_dai_name, "msm-dai-q6.4", 12)) {
		*i2s_intf = MSM_INTF_SECN;
		*i2s_dir = MSM_DIR_RX;
	} else if (!strncmp(cpu_dai_name, "msm-dai-q6.5", 12)) {
		*i2s_intf = MSM_INTF_SECN;
		*i2s_dir = MSM_DIR_TX;
	} else {
		pr_err("Error in I2S cpu dai name\n");
		ret = 1;
	}
err:
	return ret;
}

static int msm9615_enable_i2s_gpio(u8 i2s_intf, u8 i2s_dir)
{
	u8 ret = 0;
	struct msm_i2s_ctl *pintf = &msm9x15_i2s_ctl;

	if (i2s_intf == MSM_INTF_PRIM) {
		if (pintf->intf_status[i2s_intf][MSM_DIR_TX] == 0 &&
		    pintf->intf_status[i2s_intf][MSM_DIR_RX] == 0) {

			ret = gpio_request(GPIO_PRIM_I2S_DOUT,
					   "I2S_PRIM_DOUT");
			if (ret) {
				pr_err("%s: Failed to request gpio %d\n",
					__func__, GPIO_PRIM_I2S_DOUT);
				goto err;
			}

			ret = gpio_request(GPIO_PRIM_I2S_DIN, "I2S_PRIM_DIN");
			if (ret) {
				pr_err("%s: Failed to request gpio %d\n",
					       __func__, GPIO_PRIM_I2S_DIN);
				goto err;
			}

			ret = gpio_request(GPIO_PRIM_I2S_SCK, "I2S_PRIM_SCK");
			if (ret) {
				pr_err("%s: Failed to request gpio %d\n",
				       __func__, GPIO_PRIM_I2S_SCK);
				goto err;
			}

			ret = gpio_request(GPIO_PRIM_I2S_WS, "I2S_PRIM_WS");
			if (ret) {
				pr_err("%s: Failed to request gpio %d\n",
				       __func__, GPIO_PRIM_I2S_WS);
				goto err;
			}
		}
	} else if (i2s_intf == MSM_INTF_SECN) {
		if (pintf->intf_status[i2s_intf][MSM_DIR_TX] == 0 &&
		    pintf->intf_status[i2s_intf][MSM_DIR_RX] == 0) {

			ret = gpio_request(GPIO_SEC_I2S_DIN, "I2S_SEC_DIN");
			if (ret) {
				pr_err("%s: Failed to request gpio %d\n",
				       __func__, GPIO_SEC_I2S_DIN);
				goto err;
			}

			ret = gpio_request(GPIO_SEC_I2S_DOUT, "I2S_SEC_DOUT");
			if (ret) {
				pr_err("%s: Failed to request gpio %d\n",
				       __func__, GPIO_SEC_I2S_DOUT);
				goto err;
			}

			ret = gpio_request(GPIO_SEC_I2S_SCK, "I2S_SEC_SCK");
			if (ret) {
				pr_err("%s: Failed to request gpio %d\n",
				       __func__, GPIO_SEC_I2S_SCK);
				goto err;
			}

			ret = gpio_request(GPIO_SEC_I2S_WS, "I2S_SEC_WS");
			if (ret) {
				pr_err("%s: Failed to request gpio %d\n",
				       __func__, GPIO_SEC_I2S_WS);
				goto err;
			}
		}
	}
err:
	return ret;
}

static int msm9615_set_i2s_osr_bit_clk(struct snd_soc_dai *cpu_dai,
				       u8 i2s_intf, u8 i2s_dir,
				       enum msm9x15_set_i2s_clk enable)
{

	struct msm_i2s_ctl *pintf = &msm9x15_i2s_ctl;
	struct msm_i2s_clk *pclk = &pintf->prim_clk;
	struct msm_clk *clk_ctl = &pclk->rx_clk;
	u8 ret = 0;
	pr_debug("Dev name %s Intf =%d, Dir = %d, Enable=%d\n",
		cpu_dai->name, i2s_intf, i2s_dir, enable);
	if (i2s_intf == MSM_INTF_PRIM)
		pclk = &pintf->prim_clk;
	else if (i2s_intf == MSM_INTF_SECN)
		pclk = &pintf->sec_clk;

	if (i2s_dir == MSM_DIR_TX)
		clk_ctl = &pclk->tx_clk;
	else if (i2s_dir == MSM_DIR_RX)
		clk_ctl = &pclk->rx_clk;

	if (enable == MSM_I2S_CLK_SET_TRUE ||
	    enable == MSM_I2S_CLK_SET_RATE0) {
		if (clk_ctl->clk_enable != 0) {
			pr_info("%s: I2S Clk is already enabled"
				"clk users %d\n", __func__,
				clk_ctl->clk_enable);
			ret = 0;
			goto err;
		}
		clk_ctl->osr_clk = clk_get(cpu_dai->dev, "osr_clk");
		if (IS_ERR(clk_ctl->osr_clk)) {
			pr_err("%s: Fail to get OSR CLK\n", __func__);
			ret = -EINVAL;
			goto err;
		}
		ret = clk_prepare(clk_ctl->osr_clk);
		if (ret != 0) {
			pr_err("Unable to prepare i2s_spkr_osr_clk\n");
			goto err;
		}
		clk_set_rate(clk_ctl->osr_clk, TABLA_EXT_CLK_RATE);
		ret = clk_enable(clk_ctl->osr_clk);
		if (ret != 0) {
			pr_err("Fail to enable i2s_spkr_osr_clk\n");
			clk_unprepare(clk_ctl->osr_clk);
			goto err;
		}
		clk_ctl->bit_clk = clk_get(cpu_dai->dev, "bit_clk");
		if (IS_ERR(clk_ctl->bit_clk)) {
			pr_err("Fail to get i2s_spkr_bit_clk\n");
			clk_disable(clk_ctl->osr_clk);
			clk_unprepare(clk_ctl->osr_clk);
			clk_put(clk_ctl->osr_clk);
			ret = -EINVAL;
			goto err;
		}
		ret = clk_prepare(clk_ctl->bit_clk);
		if (ret != 0) {
			clk_disable(clk_ctl->osr_clk);
			clk_unprepare(clk_ctl->osr_clk);
			clk_put(clk_ctl->osr_clk);
			pr_err("Fail to prepare i2s_spkr_osr_clk\n");
			goto err;
		}
		if (enable == MSM_I2S_CLK_SET_RATE0)
			clk_set_rate(clk_ctl->bit_clk, 0);
		else
			clk_set_rate(clk_ctl->bit_clk, 8);
		ret = clk_enable(clk_ctl->bit_clk);
		if (ret != 0) {
			clk_disable(clk_ctl->osr_clk);
			clk_unprepare(clk_ctl->osr_clk);
			clk_put(clk_ctl->osr_clk);
			clk_unprepare(clk_ctl->bit_clk);
			pr_err("Unable to enable i2s_spkr_osr_clk\n");
			goto err;
		}
		clk_ctl->clk_enable++;
	} else if (enable == MSM_I2S_CLK_SET_FALSE &&
		   clk_ctl->clk_enable != 0) {
		clk_disable(clk_ctl->osr_clk);
		clk_disable(clk_ctl->bit_clk);
		clk_unprepare(clk_ctl->osr_clk);
		clk_unprepare(clk_ctl->bit_clk);
		clk_put(clk_ctl->bit_clk);
		clk_put(clk_ctl->osr_clk);
		clk_ctl->bit_clk = NULL;
		clk_ctl->osr_clk = NULL;
		clk_ctl->clk_enable--;
		ret = 0;
	}
err:
	return ret;
}

static void msm9615_config_i2s_sif_mux(u8 value, u8 i2s_intf)
{
	struct msm_i2s_ctl *pintf = &msm9x15_i2s_ctl;
	u32 sif_shadow = 0x0000;

	pr_debug("%s() Value = 0x%x intf = 0x%x\n", __func__, value, i2s_intf);
	if (i2s_intf == MSM_INTF_PRIM) {
		sif_shadow = (sif_shadow & LPASS_SIF_MUX_CTL_PRI_MUX_SEL_BMSK) |
			     (value << LPASS_SIF_MUX_CTL_PRI_MUX_SEL_SHFT);
		pr_debug("%s() Sif shadow = 0x%x\n", __func__, sif_shadow);
		sif_reg_value =
			((sif_reg_value & LPASS_SIF_MUX_CTL_SEC_MUX_SEL_BMSK) |
			 sif_shadow);
	}
	if (i2s_intf == MSM_INTF_SECN) {
		sif_shadow = (sif_shadow & LPASS_SIF_MUX_CTL_SEC_MUX_SEL_BMSK) |
				(value << LPASS_SIF_MUX_CTL_SEC_MUX_SEL_SHFT);
		pr_debug("%s() Sif shadow = 0x%x\n", __func__, sif_shadow);
		sif_reg_value =
			((sif_reg_value & LPASS_SIF_MUX_CTL_PRI_MUX_SEL_BMSK) |
			sif_shadow);
	}
	if (pintf->sif_virt_addr != NULL)
		iowrite32(sif_reg_value, pintf->sif_virt_addr);
	/* Dont read SIF register. Device crashes. */
	pr_debug("%s() SIF Reg = 0x%x\n", __func__, sif_reg_value);
}

static void msm9615_config_i2s_spare_mux(u8 value, u8 i2s_intf)
{
	struct msm_i2s_ctl *pintf = &msm9x15_i2s_ctl;
	u32 spare_shadow = 0x0000;

	pr_debug("%s() Value = 0x%x intf = 0x%x\n", __func__, value, i2s_intf);
	if (i2s_intf == MSM_INTF_PRIM) {
		/* Configure Primary SIF */
		spare_shadow =
			(spare_shadow & LPAIF_SPARE_MUX_CTL_PRI_MUX_SEL_BMSK) |
			(value << LPAIF_SPARE_MUX_CTL_PRI_MUX_SEL_SHFT);
		pr_debug("%s() Spare shadow = 0x%x\n", __func__, spare_shadow);
		spare_reg_value =
			((spare_shadow & LPAIF_SPARE_MUX_CTL_SEC_MUX_SEL_BMSK) |
			spare_shadow);
	}
	if (i2s_intf == MSM_INTF_SECN) {
		/*Secondary interface configuration*/
		spare_shadow =
			(spare_shadow & LPAIF_SPARE_MUX_CTL_SEC_MUX_SEL_BMSK) |
			(value << LPAIF_SPARE_MUX_CTL_SEC_MUX_SEL_SHFT);
		pr_debug("%s() Spare shadow = 0x%x\n", __func__, spare_shadow);
		spare_reg_value =
			((spare_shadow & LPAIF_SPARE_MUX_CTL_PRI_MUX_SEL_BMSK) |
			spare_shadow);
	}
	if (pintf->spare_virt_addr != NULL)
		iowrite32(spare_reg_value, pintf->spare_virt_addr);
	/* Dont read SPARE register. Device crashes. */
	pr_debug("%s( ): SPARE Reg =0x%x\n", __func__, spare_reg_value);
}

static int msm9615_i2s_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	int rate = params_rate(params);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct msm_i2s_ctl *pintf = &msm9x15_i2s_ctl;
	struct msm_i2s_clk *pclk = &pintf->prim_clk;
	struct msm_clk *clk_ctl = &pclk->rx_clk;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int bit_clk_set = 0;
	u8 i2s_intf, i2s_dir;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (!msm9615_i2s_intf_dir_sel(cpu_dai->name,
		    &i2s_intf, &i2s_dir)) {
			bit_clk_set = TABLA_EXT_CLK_RATE /
				      (rate * 2 * NO_OF_BITS_PER_SAMPLE);
			 if (bit_clk_set != 8) {
				if (i2s_intf == MSM_INTF_PRIM)
					pclk = &pintf->prim_clk;
				else if (i2s_intf == MSM_INTF_SECN)
					pclk = &pintf->sec_clk;
				clk_ctl = &pclk->rx_clk;
				pr_debug("%s( ): New rate = %d",
					__func__, bit_clk_set);
				clk_set_rate(clk_ctl->bit_clk, bit_clk_set);
			 }
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		bit_clk_set = I2S_MIC_SCLK_RATE / (rate * 2 *
						NO_OF_BITS_PER_SAMPLE);
		/* Not required to modify TX rate.
		  * Speaker clock are looped back
		  * to Mic.
		  */
	}
	return 1;
}

static int msm9615_i2s_startup(struct snd_pcm_substream *substream)
{
	u8 ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct msm_i2s_ctl *pintf = &msm9x15_i2s_ctl;
	u8 i2s_intf, i2s_dir;
	if (!msm9615_i2s_intf_dir_sel(cpu_dai->name, &i2s_intf, &i2s_dir)) {
		pr_debug("%s( ): cpu name = %s intf =%d dir = %d\n",
			 __func__, cpu_dai->name, i2s_intf, i2s_dir);
		pr_debug("%s( ): Enable status Rx =%d Tx = %d\n", __func__,
			 pintf->intf_status[i2s_intf][MSM_DIR_RX],
			 pintf->intf_status[i2s_intf][MSM_DIR_TX]);
		msm9615_enable_i2s_gpio(i2s_intf, i2s_dir);
		if (i2s_dir == MSM_DIR_TX) {
			if (pintf->intf_status[i2s_intf][MSM_DIR_RX] > 0) {
				/* This means that Rx is enabled before */
				ret = msm9615_set_i2s_osr_bit_clk(cpu_dai,
						    i2s_intf, i2s_dir,
						    MSM_I2S_CLK_SET_RATE0);
				if (ret != 0) {
					pr_err("%s: Fail enable I2S clock\n",
					       __func__);
					return -EINVAL;
				}
				msm9615_config_i2s_sif_mux(
				       pintf->mux_ctl[MSM_DIR_BOTH].sifconfig,
					i2s_intf);
				msm9615_config_i2s_spare_mux(
				      pintf->mux_ctl[MSM_DIR_BOTH].spareconfig,
				      i2s_intf);
				ret = snd_soc_dai_set_fmt(cpu_dai,
						       SND_SOC_DAIFMT_CBM_CFM);
				if (ret < 0)
					pr_err("set fmt cpu dai failed\n");
					ret = snd_soc_dai_set_fmt(codec_dai,
						       SND_SOC_DAIFMT_CBS_CFS);
				if (ret < 0)
					pr_err("set fmt codec dai failed\n");
			} else if (pintf->intf_status[i2s_intf][i2s_dir] == 0) {
				/* This means that Rx is
				 * not enabled before.
				 * only Tx will be used.
				 */
				ret = msm9615_set_i2s_osr_bit_clk(cpu_dai,
							i2s_intf, i2s_dir,
							MSM_I2S_CLK_SET_TRUE);
				if (ret != 0) {
					pr_err("%s: Fail Tx I2S clock\n",
					       __func__);
					return -EINVAL;
				}
				msm9615_config_i2s_sif_mux(
					pintf->mux_ctl[MSM_DIR_TX].sifconfig,
					i2s_intf);
				msm9615_config_i2s_spare_mux(
					pintf->mux_ctl[MSM_DIR_TX].spareconfig,
					i2s_intf);
				ret = snd_soc_dai_set_fmt(cpu_dai,
						       SND_SOC_DAIFMT_CBS_CFS);
				if (ret < 0)
					pr_err("set fmt cpu dai failed\n");
					ret = snd_soc_dai_set_fmt(codec_dai,
						       SND_SOC_DAIFMT_CBS_CFS);
				if (ret < 0)
					pr_err("set fmt codec dai failed\n");
	}
		} else if (i2s_dir == MSM_DIR_RX) {
			if (pintf->intf_status[i2s_intf][MSM_DIR_TX] > 0) {
				pr_err("%s: Error shutdown Tx first\n",
				       __func__);
				return -EINVAL;
			} else if (pintf->intf_status[i2s_intf][i2s_dir]
				   == 0) {
				ret = msm9615_set_i2s_osr_bit_clk(cpu_dai,
						i2s_intf, i2s_dir,
						MSM_I2S_CLK_SET_TRUE);
				if (ret != 0) {
					pr_err("%s: Fail Rx I2S clock\n",
						__func__);
					return -EINVAL;
				}
				msm9615_config_i2s_sif_mux(
					pintf->mux_ctl[MSM_DIR_RX].sifconfig,
					i2s_intf);
				msm9615_config_i2s_spare_mux(
					pintf->mux_ctl[MSM_DIR_RX].spareconfig,
					i2s_intf);
				ret = snd_soc_dai_set_fmt(cpu_dai,
						SND_SOC_DAIFMT_CBS_CFS);
				if (ret < 0)
					pr_err("set fmt cpu dai failed\n");
				ret = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_CBS_CFS);
				if (ret < 0)
					pr_err("set fmt codec dai failed\n");
			}
		}
		pintf->intf_status[i2s_intf][i2s_dir]++;
	} else {
		pr_err("%s: Err in i2s_intf_dir_sel\n", __func__);
		return -EINVAL;
	}
	pr_debug("Exit %s() Enable status Rx =%d Tx = %d\n", __func__,
		 pintf->intf_status[i2s_intf][MSM_DIR_RX],
		 pintf->intf_status[i2s_intf][MSM_DIR_TX]);
	return ret;
}

static void msm9615_i2s_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct msm_i2s_ctl *pintf = &msm9x15_i2s_ctl;
	u8 i2s_intf = 0, i2s_dir = 0, ret = 0;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	pr_debug("%s( ): Enable status Rx =%d Tx = %d\n",
		__func__, pintf->intf_status[i2s_intf][MSM_DIR_RX],
		pintf->intf_status[i2s_intf][MSM_DIR_TX]);
	if (!msm9615_i2s_intf_dir_sel(cpu_dai->name, &i2s_intf, &i2s_dir)) {
		pr_debug("%s( ): intf =%d dir = %d\n", __func__,
			 i2s_intf, i2s_dir);
		if (i2s_dir == MSM_DIR_RX)
			if (pintf->intf_status[i2s_intf][MSM_DIR_TX] > 0)
				pr_err("%s: Shutdown Tx First then by RX\n",
				       __func__);
		ret = msm9615_set_i2s_osr_bit_clk(cpu_dai, i2s_intf, i2s_dir,
							MSM_I2S_CLK_SET_FALSE);
		if (ret != 0)
			pr_err("%s: Cannot disable I2S clock\n",
			       __func__);
		pintf->intf_status[i2s_intf][i2s_dir]--;
		mdm9615_i2s_free_gpios(i2s_intf, i2s_dir);
	}
	pr_debug("%s( ): Enable status Rx =%d Tx = %d\n", __func__,
		 pintf->intf_status[i2s_intf][MSM_DIR_RX],
		 pintf->intf_status[i2s_intf][MSM_DIR_TX]);
}

void msm9615_config_port_select(void)
{
	iowrite32(SEC_PCM_PORT_SLC_VALUE, secpcm_portslc_virt_addr);
	pr_debug("%s() port select after updating = 0x%x\n",
		__func__, ioread32(secpcm_portslc_virt_addr));
}
static void  mdm9615_install_codec_i2s_gpio(struct snd_pcm_substream *substream)
{
	u8 i2s_intf, i2s_dir;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	if (!msm9615_i2s_intf_dir_sel(cpu_dai->name, &i2s_intf, &i2s_dir)) {
		pr_debug("%s( ): cpu name = %s intf =%d dir = %d\n",
			 __func__, cpu_dai->name, i2s_intf, i2s_dir);
		if (i2s_intf == MSM_INTF_PRIM) {
			msm_gpiomux_install(
			msm9615_audio_prim_i2s_codec_configs,
			ARRAY_SIZE(msm9615_audio_prim_i2s_codec_configs));
		} else if (i2s_intf == MSM_INTF_SECN) {
			msm_gpiomux_install(msm9615_audio_sec_i2s_codec_configs,
			ARRAY_SIZE(msm9615_audio_sec_i2s_codec_configs));
			msm9615_config_port_select();

		}
	}
}

static int msm9615_i2s_prepare(struct snd_pcm_substream *substream)
{
	u8 ret = 0;

	if (wcd9xxx_get_intf_type() < 0)
		ret = -ENODEV;
	else if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
		mdm9615_install_codec_i2s_gpio(substream);

	return ret;
}

static struct snd_soc_ops msm9615_i2s_be_ops = {
	.startup = msm9615_i2s_startup,
	.shutdown = msm9615_i2s_shutdown,
	.hw_params = msm9615_i2s_hw_params,
	.prepare = msm9615_i2s_prepare,
};

static int mdm9615_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct pm_gpio jack_gpio_cfg = {
		.direction = PM_GPIO_DIR_IN,
		.pull = PM_GPIO_PULL_NO,
		.function = PM_GPIO_FUNC_NORMAL,
		.vin_sel = 2,
		.inv_int_pol = 0,
	};
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	/* Tabla SLIMBUS configuration
	 * RX1, RX2, RX3, RX4, RX5, RX6, RX7
	 * TX1, TX2, TX3, TX4, TX5, TX6, TX7, TX8, TX9, TX10
	 */
	unsigned int rx_ch[TABLA_RX_MAX] = {138, 139, 140, 141, 142, 143, 144};
	unsigned int tx_ch[TABLA_TX_MAX]  = {128, 129, 130, 131, 132, 133, 134,
					     135, 136, 137};

	pr_debug("%s(), dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;

	snd_soc_dapm_new_controls(dapm, mdm9615_dapm_widgets,
				ARRAY_SIZE(mdm9615_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, common_audio_map,
		ARRAY_SIZE(common_audio_map));

	snd_soc_dapm_enable_pin(dapm, "Ext Spk Pos");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Neg");

	snd_soc_dapm_sync(dapm);

	err = snd_soc_jack_new(codec, "Headset Jack",
			       (SND_JACK_HEADSET | SND_JACK_OC_HPHL |
				SND_JACK_OC_HPHR),
			       &hs_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}

	err = snd_soc_jack_new(codec, "Button Jack",
			       TABLA_JACK_BUTTON_MASK, &button_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	codec_clk = clk_get(cpu_dai->dev, "osr_clk");

	if (hs_detect_use_gpio) {
		pr_debug("%s: GPIO Headset detection enabled\n", __func__);
		mbhc_cfg.gpio = PM8018_GPIO_PM_TO_SYS(JACK_DETECT_GPIO);
		mbhc_cfg.gpio_irq = JACK_DETECT_INT;
	}

	if (mbhc_cfg.gpio) {
		err = pm8xxx_gpio_config(mbhc_cfg.gpio, &jack_gpio_cfg);
		if (err) {
			pr_err("%s: pm8xxx_gpio_config JACK_DETECT failed %d\n",
			       __func__, err);
			return err;
		}
	}

	mbhc_cfg.read_fw_bin = hs_detect_use_firmware;

	err = tabla_hs_detect(codec, &mbhc_cfg);

	snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
				    tx_ch, ARRAY_SIZE(rx_ch), rx_ch);


	return err;
}

static int mdm9615_slim_0_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = mdm9615_slim_0_rx_ch;

	return 0;
}

static int mdm9615_slim_0_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = mdm9615_slim_0_tx_ch;

	return 0;
}

static int mdm9615_btsco_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = mdm9615_btsco_rate;
	channels->min = channels->max = mdm9615_btsco_ch;

	return 0;
}
static int mdm9615_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = mdm9615_auxpcm_rate;
	/* PCM only supports mono output */
	channels->min = channels->max = 1;

	return 0;
}

static int mdm9615_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;

	return 0;
}

static int mdm9615_aux_pcm_get_gpios(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	ret = gpio_request(GPIO_AUX_PCM_DOUT, "AUX PCM DOUT");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM DOUT",
				__func__, GPIO_AUX_PCM_DOUT);
		goto fail_dout;
	}

	ret = gpio_request(GPIO_AUX_PCM_DIN, "AUX PCM DIN");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM DIN",
				__func__, GPIO_AUX_PCM_DIN);
		goto fail_din;
	}

	ret = gpio_request(GPIO_AUX_PCM_SYNC, "AUX PCM SYNC");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM SYNC",
				__func__, GPIO_AUX_PCM_SYNC);
		goto fail_sync;
	}
	ret = gpio_request(GPIO_AUX_PCM_CLK, "AUX PCM CLK");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM CLK",
				__func__, GPIO_AUX_PCM_CLK);
		goto fail_clk;
	}

	return 0;

fail_clk:
	gpio_free(GPIO_AUX_PCM_SYNC);
fail_sync:
	gpio_free(GPIO_AUX_PCM_DIN);
fail_din:
	gpio_free(GPIO_AUX_PCM_DOUT);
fail_dout:

	return ret;
}

static int mdm9615_aux_pcm_free_gpios(void)
{
	gpio_free(GPIO_AUX_PCM_DIN);
	gpio_free(GPIO_AUX_PCM_DOUT);
	gpio_free(GPIO_AUX_PCM_SYNC);
	gpio_free(GPIO_AUX_PCM_CLK);

	return 0;
}

static int mdm9615_sec_aux_pcm_get_gpios(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	ret = gpio_request(GPIO_SEC_AUX_PCM_DOUT, "SEC_AUX PCM DOUT");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): SEC_AUX PCM DOUT",
		       __func__, GPIO_SEC_AUX_PCM_DOUT);
		goto fail_dout;
	}

	ret = gpio_request(GPIO_SEC_AUX_PCM_DIN, "SEC_AUX PCM DIN");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): SEC_AUX PCM DIN",
		       __func__, GPIO_SEC_AUX_PCM_DIN);
		goto fail_din;
	}

	ret = gpio_request(GPIO_SEC_AUX_PCM_SYNC, "SEC_AUX PCM SYNC");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): SEC_AUX PCM SYNC",
		       __func__, GPIO_SEC_AUX_PCM_SYNC);
		goto fail_sync;
	}

	ret = gpio_request(GPIO_SEC_AUX_PCM_CLK, "SEC_AUX PCM CLK");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): SEC_AUX PCM CLK",
		       __func__, GPIO_SEC_AUX_PCM_CLK);
		goto fail_clk;
	}

	return 0;

fail_clk:
	gpio_free(GPIO_SEC_AUX_PCM_SYNC);
fail_sync:
	gpio_free(GPIO_SEC_AUX_PCM_DIN);
fail_din:
	gpio_free(GPIO_SEC_AUX_PCM_DOUT);
fail_dout:

	return ret;
}

static int mdm9615_sec_aux_pcm_free_gpios(void)
{
	gpio_free(GPIO_SEC_AUX_PCM_DIN);
	gpio_free(GPIO_SEC_AUX_PCM_DOUT);
	gpio_free(GPIO_SEC_AUX_PCM_SYNC);
	gpio_free(GPIO_SEC_AUX_PCM_CLK);

	return 0;
}

static int mdm9615_startup(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return 0;
}

void msm9615_config_sif_mux(u8 value)
{
	u32 sif_shadow  = 0x00000;

	sif_shadow = (sif_shadow & LPASS_SIF_MUX_CTL_SEC_MUX_SEL_BMSK) |
		     (value << LPASS_SIF_MUX_CTL_SEC_MUX_SEL_SHFT);
	iowrite32(sif_shadow, sif_virt_addr);
	/* Dont read SIF register. Device crashes. */
	pr_debug("%s() SIF Reg = 0x%x\n", __func__, sif_shadow);
}

static int mdm9615_auxpcm_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;

	pr_debug("%s(): substream = %s\n", __func__, substream->name);
	if (atomic_inc_return(&msm9615_auxpcm_ref) == 1) {
		ret = mdm9615_aux_pcm_get_gpios();
		if (ret < 0) {
			pr_err("%s: Aux PCM GPIO request failed\n", __func__);
			return -EINVAL;
		}
	}
	return 0;
}

static void mdm9615_auxpcm_shutdown(struct snd_pcm_substream *substream)
{

	pr_debug("%s(): substream = %s\n", __func__, substream->name);
	if (atomic_dec_return(&msm9615_auxpcm_ref) == 0)
		mdm9615_aux_pcm_free_gpios();
}

static int mdm9615_sec_auxpcm_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;

	pr_debug("%s(): substream = %s\n", __func__, substream->name);
	if (atomic_inc_return(&msm9615_sec_auxpcm_ref) == 1) {
		ret = mdm9615_sec_aux_pcm_get_gpios();
		if (ret < 0) {
			pr_err("%s: SEC Aux PCM GPIO request failed\n",
			       __func__);
			return -EINVAL;
		}
		msm9615_config_sif_mux(MSM_SIF_FUNC_PCM);
		msm9615_config_port_select();
	}
	return 0;
}

static void mdm9615_sec_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s\n", __func__, substream->name);
	if (atomic_dec_return(&msm9615_sec_auxpcm_ref) == 0)
		mdm9615_sec_aux_pcm_free_gpios();
}

static void mdm9615_shutdown(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
}

static struct snd_soc_ops mdm9615_be_ops = {
	.startup = mdm9615_startup,
	.hw_params = mdm9615_hw_params,
	.shutdown = mdm9615_shutdown,
};

static struct snd_soc_ops mdm9615_auxpcm_be_ops = {
	.startup = mdm9615_auxpcm_startup,
	.shutdown = mdm9615_auxpcm_shutdown,
};

static struct snd_soc_ops mdm9615_sec_auxpcm_be_ops = {
	.startup = mdm9615_sec_auxpcm_startup,
	.shutdown = mdm9615_sec_auxpcm_shutdown,
};


/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mdm9615_dai_common[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MDM9615 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "MDM9615 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name	= "MultiMedia2",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	/* Hostless PMC purpose */
	{
		.name = "SLIMBUS_0 Hostless",
		.stream_name = "SLIMBUS_0 Hostless",
		.cpu_dai_name	= "SLIMBUS0_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* .be_id = do not care */
	},
	{
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
	},
	{
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
	},
	{
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name	= "AUXPCM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
	},
	{
		.name = "VoLTE",
		.stream_name = "VoLTE",
		.cpu_dai_name   = "VoLTE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.be_id = MSM_FRONTEND_DAI_VOLTE,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
	},
	{
		.name = "DTMF RX Hostless",
		.stream_name = "DTMF RX Hostless",
		.cpu_dai_name	= "DTMF_RX_HOSTLESS",
		.platform_name  = "msm-pcm-dtmf",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.be_id = MSM_FRONTEND_DAI_DTMF_RX,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
	},
	{
		.name = "DTMF TX",
		.stream_name = "DTMF TX",
		.cpu_dai_name = "msm-dai-stub",
		.platform_name  = "msm-pcm-dtmf",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
	},
	{
		.name = "CS-VOICE HOST RX CAPTURE",
		.stream_name = "CS-VOICE HOST RX CAPTURE",
		.cpu_dai_name = "msm-dai-stub",
		.platform_name  = "msm-host-pcm-voice",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "CS-VOICE HOST RX PLAYBACK",
		.stream_name = "CS-VOICE HOST RX PLAYBACK",
		.cpu_dai_name = "msm-dai-stub",
		.platform_name  = "msm-host-pcm-voice",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.ignore_suspend = 1,
	},
	{
		.name = "CS-VOICE HOST TX CAPTURE",
		.stream_name = "CS-VOICE HOST TX CAPTURE",
		.cpu_dai_name = "msm-dai-stub",
		.platform_name  = "msm-host-pcm-voice",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "CS-VOICE HOST TX PLAYBACK",
		.stream_name = "CS-VOICE HOST TX PLAYBACK",
		.cpu_dai_name = "msm-dai-stub",
		.platform_name  = "msm-host-pcm-voice",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.ignore_suspend = 1,
	},

	/* Backend BT DAI Links */
	{
		.name = LPASS_BE_INT_BT_SCO_RX,
		.stream_name = "Internal BT-SCO Playback",
		.cpu_dai_name = "msm-dai-q6.12288",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_RX,
		.be_hw_params_fixup = mdm9615_btsco_be_hw_params_fixup,
	},
	{
		.name = LPASS_BE_INT_BT_SCO_TX,
		.stream_name = "Internal BT-SCO Capture",
		.cpu_dai_name = "msm-dai-q6.12289",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_TX,
		.be_hw_params_fixup = mdm9615_btsco_be_hw_params_fixup,
	},

	/* Backend AFE DAI Links */
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.cpu_dai_name = "msm-dai-q6.224",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.cpu_dai_name = "msm-dai-q6.225",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
	},
	/* AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = mdm9615_auxpcm_be_params_fixup,
		.ops = &mdm9615_auxpcm_be_ops,
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = mdm9615_auxpcm_be_params_fixup,
		.ops = &mdm9615_auxpcm_be_ops,
	},

	/* SECONDARY AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_SEC_AUXPCM_RX,
		.stream_name = "SEC AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6.12",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_RX,
		.be_hw_params_fixup = mdm9615_auxpcm_be_params_fixup,
		.ops = &mdm9615_sec_auxpcm_be_ops,
	},
	{
		.name = LPASS_BE_SEC_AUXPCM_TX,
		.stream_name = "SEC AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6.13",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_TX,
		.be_hw_params_fixup = mdm9615_auxpcm_be_params_fixup,
		.ops = &mdm9615_sec_auxpcm_be_ops,
	},
	/* Incall Music BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE_PLAYBACK_TX,
		.stream_name = "Voice Farend Playback",
		.cpu_dai_name = "msm-dai-q6.32773",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = mdm9615_be_hw_params_fixup,
	},
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.cpu_dai_name = "msm-dai-q6.32772",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = mdm9615_be_hw_params_fixup,
	},
	/* Incall Record Downlink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_RX,
		.stream_name = "Voice Downlink Capture",
		.cpu_dai_name = "msm-dai-q6.32771",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = mdm9615_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* this dailink has playback support */
	},
};

static struct snd_soc_dai_link mdm9615_dai_i2s_tabla[] = {
	/* Backend I2S DAI Links */
	{
		.name = LPASS_BE_PRI_I2S_RX,
		.stream_name = "Primary I2S Playback",
		.cpu_dai_name = "msm-dai-q6.0",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tabla_codec",
		.codec_dai_name	= "tabla_i2s_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_I2S_RX,
		.init = &msm9615_i2s_audrx_init,
		.be_hw_params_fixup = msm9615_i2s_rx_be_hw_params_fixup,
		.ops = &msm9615_i2s_be_ops,
	},
	{
		.name = LPASS_BE_PRI_I2S_TX,
		.stream_name = "Primary I2S Capture",
		.cpu_dai_name = "msm-dai-q6.1",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tabla_codec",
		.codec_dai_name	= "tabla_i2s_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_I2S_TX,
		.be_hw_params_fixup = msm9615_i2s_tx_be_hw_params_fixup,
		.ops = &msm9615_i2s_be_ops,
	},
	{
		.name = LPASS_BE_SEC_I2S_RX,
		.stream_name = "Secondary I2S Playback",
		.cpu_dai_name = "msm-dai-q6.4",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SEC_I2S_RX,
		.be_hw_params_fixup = msm9615_i2s_rx_be_hw_params_fixup,
		.ops = &msm9615_i2s_be_ops,
	},
	{
		.name = LPASS_BE_SEC_I2S_TX,
		.stream_name = "Secondary I2S Capture",
		.cpu_dai_name = "msm-dai-q6.5",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SEC_I2S_TX,
		.be_hw_params_fixup = msm9615_i2s_tx_be_hw_params_fixup,
		.ops = &msm9615_i2s_be_ops,
	},
};

static struct snd_soc_dai_link mdm9615_dai_slimbus_tabla[] = {
	/* Backend SlimBus DAI Links */
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tabla_codec",
		.codec_dai_name	= "tabla_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &mdm9615_audrx_init,
		.be_hw_params_fixup = mdm9615_slim_0_rx_be_hw_params_fixup,
		.ops = &mdm9615_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tabla_codec",
		.codec_dai_name	= "tabla_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = mdm9615_slim_0_tx_be_hw_params_fixup,
		.ops = &mdm9615_be_ops,
	},
};

static struct snd_soc_dai_link mdm9615_i2s_dai[
					 ARRAY_SIZE(mdm9615_dai_common) +
					 ARRAY_SIZE(mdm9615_dai_i2s_tabla)];

static struct snd_soc_dai_link mdm9615_slimbus_dai[
					 ARRAY_SIZE(mdm9615_dai_common) +
					 ARRAY_SIZE(mdm9615_dai_slimbus_tabla)];


static struct snd_soc_card snd_soc_card_mdm9615[] = {
	[0] = {
		.name = "mdm9615-tabla-snd-card",
		.controls = tabla_mdm9615_controls,
		.num_controls = ARRAY_SIZE(tabla_mdm9615_controls),
	},
	[1] = {
		.name = "mdm9615-tabla-snd-card-i2s",
		.controls = tabla_msm9615_i2s_controls,
		.num_controls = ARRAY_SIZE(tabla_msm9615_i2s_controls),
	},
};

static int __init mdm9615_audio_init(void)
{
	int ret;

	/* Set GPIO headset detection by default */
	hs_detect_use_gpio = true;

	if (!cpu_is_msm9615()) {
		pr_err("%s: Not the right machine type\n", __func__);
		return -ENODEV ;
	}

	mbhc_cfg.calibration = def_tabla_mbhc_cal();
	if (!mbhc_cfg.calibration) {
		pr_err("Calibration data allocation failed\n");
		return -ENOMEM;
	}
	mdm9615_snd_device_slim = platform_device_alloc("soc-audio", 0);
	if (!mdm9615_snd_device_slim) {
		pr_err("Platform device allocation failed\n");
		kfree(mbhc_cfg.calibration);
		return -ENOMEM;
	}

	/* Install SLIM specific links */
	memcpy(mdm9615_slimbus_dai, mdm9615_dai_common,
			sizeof(mdm9615_dai_common));
	memcpy(mdm9615_slimbus_dai + ARRAY_SIZE(mdm9615_dai_common),
		       mdm9615_dai_slimbus_tabla,
		       sizeof(mdm9615_dai_slimbus_tabla));
	snd_soc_card_mdm9615[0].dai_link = mdm9615_slimbus_dai;
	snd_soc_card_mdm9615[0].num_links =
				ARRAY_SIZE(mdm9615_slimbus_dai);

	mdm9615_snd_device_i2s = platform_device_alloc("soc-audio", 1);
	if (!mdm9615_snd_device_i2s) {
		pr_err("Platform device allocation failed\n");
		kfree(mbhc_cfg.calibration);
		return -ENOMEM;
	}
	pr_err("%s: Interface Type = %d\n", __func__,
			wcd9xxx_get_intf_type());

	/* Install I2S specific links */
	memcpy(mdm9615_i2s_dai, mdm9615_dai_common,
	       sizeof(mdm9615_dai_common));
	memcpy(mdm9615_i2s_dai + ARRAY_SIZE(mdm9615_dai_common),
	       mdm9615_dai_i2s_tabla,
	       sizeof(mdm9615_dai_i2s_tabla));
	snd_soc_card_mdm9615[1].dai_link = mdm9615_i2s_dai;
	snd_soc_card_mdm9615[1].num_links =
				ARRAY_SIZE(mdm9615_i2s_dai);
	platform_set_drvdata(mdm9615_snd_device_slim, &snd_soc_card_mdm9615[0]);
	ret = platform_device_add(mdm9615_snd_device_slim);
	if (ret) {
		pr_err("%s Slim platform_device_add fail\n", __func__);
		platform_device_put(mdm9615_snd_device_slim);
		kfree(mbhc_cfg.calibration);
		return ret;
	}
	platform_set_drvdata(mdm9615_snd_device_i2s, &snd_soc_card_mdm9615[1]);
	ret = platform_device_add(mdm9615_snd_device_i2s);
	if (ret) {
		pr_err("%s I2S platform_device_add fail\n", __func__);
		platform_device_put(mdm9615_snd_device_i2s);
		kfree(mbhc_cfg.calibration);
		return ret;
	}

	/*
	 * Irrespective of audio interface type get virtual address
	 * of LPAIF registers as it may not  be guaranted that I2S
	 * will probed successfully in Init.
	 */
	atomic_set(&msm9615_auxpcm_ref, 0);
	atomic_set(&msm9615_sec_auxpcm_ref, 0);
	msm9x15_i2s_ctl.sif_virt_addr = ioremap(LPASS_SIF_MUX_ADDR, 4);
	msm9x15_i2s_ctl.spare_virt_addr = ioremap(LPAIF_SPARE_ADDR, 4);
	if (msm9x15_i2s_ctl.spare_virt_addr == NULL ||
	    msm9x15_i2s_ctl.sif_virt_addr == NULL)
		pr_err("%s: SIF or Spare ptr are NULL", __func__);
	sif_virt_addr = ioremap(LPASS_SIF_MUX_ADDR, 4);
	secpcm_portslc_virt_addr = ioremap(SEC_PCM_PORT_SLC_ADDR, 4);

	return ret;
}
module_init(mdm9615_audio_init);

static void __exit mdm9615_audio_exit(void)
{
	if (!cpu_is_msm9615()) {
		pr_err("%s: Not the right machine type\n", __func__);
		return ;
	}
	platform_device_unregister(mdm9615_snd_device_slim);
	platform_device_unregister(mdm9615_snd_device_i2s);
	kfree(mbhc_cfg.calibration);
	iounmap(msm9x15_i2s_ctl.sif_virt_addr);
	iounmap(msm9x15_i2s_ctl.spare_virt_addr);
	iounmap(sif_virt_addr);
	iounmap(secpcm_portslc_virt_addr);

}
module_exit(mdm9615_audio_exit);

MODULE_DESCRIPTION("ALSA SoC MDM9615");
MODULE_LICENSE("GPL v2");
