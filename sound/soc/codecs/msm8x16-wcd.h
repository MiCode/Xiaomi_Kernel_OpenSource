/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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
#ifndef MSM8X16_WCD_H
#define MSM8X16_WCD_H

#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include "wcd-mbhc-v2.h"
#include "wcdcal-hwdep.h"

#define MSM8X16_WCD_NUM_REGISTERS	0x6FF
#define MSM8X16_WCD_MAX_REGISTER	(MSM8X16_WCD_NUM_REGISTERS-1)
#define MSM8X16_WCD_CACHE_SIZE		MSM8X16_WCD_NUM_REGISTERS
#define MSM8X16_WCD_NUM_IRQ_REGS	2
#define MAX_REGULATOR				7
#define MSM8X16_WCD_REG_VAL(reg, val)		{reg, 0, val}
#define MSM8X16_TOMBAK_LPASS_AUDIO_CORE_DIG_CODEC_CLK_SEL	0xFE03B004
#define MSM8X16_TOMBAK_LPASS_DIGCODEC_CMD_RCGR			0x0181C09C
#define MSM8X16_TOMBAK_LPASS_DIGCODEC_CFG_RCGR			0x0181C0A0
#define MSM8X16_TOMBAK_LPASS_DIGCODEC_M				0x0181C0A4
#define MSM8X16_TOMBAK_LPASS_DIGCODEC_N				0x0181C0A8
#define MSM8X16_TOMBAK_LPASS_DIGCODEC_D				0x0181C0AC
#define MSM8X16_TOMBAK_LPASS_DIGCODEC_CBCR			0x0181C0B0
#define MSM8X16_TOMBAK_LPASS_DIGCODEC_AHB_CBCR			0x0181C0B4

#ifdef WT_88047_L_AUDIO_PA_GPIO
#define EXT_SPK_AMP_GPIO	(902+117)

#define EXT_SPK_AMP_HEADSET_GPIO	(902+8)
#else
#define EXT_SPK_AMP_GPIO	(902+121)

#define EXT_SPK_AMP_HEADSET_GPIO	(902+8)
#endif

#define MSM8X16_CODEC_NAME "msm8x16_wcd_codec"

#define MSM8X16_WCD_IS_DIGITAL_REG(reg) \
	(((reg >= 0x200) && (reg <= 0x4FF)) ? 1 : 0)
#define MSM8X16_WCD_IS_TOMBAK_REG(reg) \
	(((reg >= 0x000) && (reg <= 0x1FF)) ? 1 : 0)
/*
 * MCLK activity indicators during suspend and resume call
 */
#define MCLK_SUS_DIS	1
#define MCLK_SUS_RSC	2
#define MCLK_SUS_NO_ACT	3

#define NUM_DECIMATORS	2
#define MSM89XX_VDD_SPKDRV_NAME "cdc-vdd-spkdrv"

extern const u8 msm8x16_wcd_reg_readable[MSM8X16_WCD_CACHE_SIZE];
extern const u8 msm8x16_wcd_reg_readonly[MSM8X16_WCD_CACHE_SIZE];
extern const u8 msm8x16_wcd_reset_reg_defaults[MSM8X16_WCD_CACHE_SIZE];

enum codec_versions {
	TOMBAK_1_0,
	TOMBAK_2_0,
	CONGA,
	UNSUPPORTED,
};

enum msm8x16_wcd_pid_current {
	MSM8X16_WCD_PID_MIC_2P5_UA,
	MSM8X16_WCD_PID_MIC_5_UA,
	MSM8X16_WCD_PID_MIC_10_UA,
	MSM8X16_WCD_PID_MIC_20_UA,
};

struct msm8x16_wcd_reg_mask_val {
	u16	reg;
	u8	mask;
	u8	val;
};

enum msm8x16_wcd_mbhc_analog_pwr_cfg {
	MSM8X16_WCD_ANALOG_PWR_COLLAPSED = 0,
	MSM8X16_WCD_ANALOG_PWR_ON,
	MSM8X16_WCD_NUM_ANALOG_PWR_CONFIGS,
};

/* Number of input and output I2S port */
enum {
	MSM8X16_WCD_RX1 = 0,
	MSM8X16_WCD_RX2,
	MSM8X16_WCD_RX3,
	MSM8X16_WCD_RX_MAX,
};

enum {
	MSM8X16_WCD_TX1 = 0,
	MSM8X16_WCD_TX2,
	MSM8X16_WCD_TX3,
	MSM8X16_WCD_TX4,
	MSM8X16_WCD_TX_MAX,
};

enum {
	/* INTR_REG 0 - Digital Periph */
	MSM8X16_WCD_IRQ_SPKR_CNP = 0,
	MSM8X16_WCD_IRQ_SPKR_CLIP,
	MSM8X16_WCD_IRQ_SPKR_OCP,
	MSM8X16_WCD_IRQ_MBHC_INSREM_DET1,
	MSM8X16_WCD_IRQ_MBHC_RELEASE,
	MSM8X16_WCD_IRQ_MBHC_PRESS,
	MSM8X16_WCD_IRQ_MBHC_INSREM_DET,
	MSM8X16_WCD_IRQ_MBHC_HS_DET,
	/* INTR_REG 1 - Analog Periph */
	MSM8X16_WCD_IRQ_EAR_OCP,
	MSM8X16_WCD_IRQ_HPHR_OCP,
	MSM8X16_WCD_IRQ_HPHL_OCP,
	MSM8X16_WCD_IRQ_EAR_CNP,
	MSM8X16_WCD_IRQ_HPHR_CNP,
	MSM8X16_WCD_IRQ_HPHL_CNP,
	MSM8X16_WCD_NUM_IRQS,
};

enum wcd_notify_event {
	WCD_EVENT_INVALID,
	/* events for micbias ON and OFF */
	WCD_EVENT_PRE_MICBIAS_2_OFF,
	WCD_EVENT_POST_MICBIAS_2_OFF,
	WCD_EVENT_PRE_MICBIAS_2_ON,
	WCD_EVENT_POST_MICBIAS_2_ON,
	/* events for PA ON and OFF */
	WCD_EVENT_PRE_HPHL_PA_ON,
	WCD_EVENT_POST_HPHL_PA_OFF,
	WCD_EVENT_PRE_HPHR_PA_ON,
	WCD_EVENT_POST_HPHR_PA_OFF,
	WCD_EVENT_LAST,
};

enum {
	ON_DEMAND_MICBIAS = 0,
	ON_DEMAND_SPKDRV,
	ON_DEMAND_SUPPLIES_MAX,
};

/*
 * The delay list is per codec HW specification.
 * Please add delay in the list in the future instead
 * of magic number
 */
enum {
	CODEC_DELAY_1_MS = 1000,
	CODEC_DELAY_1_1_MS  = 1100,
};

struct msm8x16_wcd_regulator {
	const char *name;
	int min_uv;
	int max_uv;
	int optimum_ua;
	bool ondemand;
	struct regulator *regulator;
};

struct msm8916_asoc_mach_data {
	int codec_type;
	int ext_pa;
	int us_euro_gpio;
	int spk_ext_pa_gpio;
	int mclk_freq;
	int lb_mode;
	u8 micbias1_cap_mode;
	u8 micbias2_cap_mode;
	atomic_t mclk_rsc_ref;
	atomic_t mclk_enabled;
	struct mutex cdc_mclk_mutex;
	struct delayed_work disable_mclk_work;
	struct afe_digital_clk_cfg digital_cdc_clk;
	void __iomem *vaddr_gpio_mux_spkr_ctl;
	void __iomem *vaddr_gpio_mux_mic_ctl;
	void __iomem *vaddr_gpio_mux_pcm_ctl;
};

struct msm8x16_wcd_pdata {
	int irq;
	int irq_base;
	int num_irqs;
	int reset_gpio;
	void *msm8x16_wcd_ahb_base_vaddr;
	struct wcd9xxx_micbias_setting micbias;
	struct msm8x16_wcd_regulator regulator[MAX_REGULATOR];
	u32 mclk_rate;
};

enum msm8x16_wcd_micbias_num {
	MSM8X16_WCD_MICBIAS1 = 0,
};

struct msm8x16_wcd {
	struct device *dev;
	struct mutex io_lock;
	u8 version;

	int reset_gpio;
	int (*read_dev)(struct snd_soc_codec *codec,
			unsigned short reg);
	int (*write_dev)(struct snd_soc_codec *codec,
			 unsigned short reg, u8 val);

	u32 num_of_supplies;
	struct regulator_bulk_data *supplies;

	u8 idbyte[4];

	int num_irqs;
	u32 mclk_rate;
	char __iomem *dig_base;
};

struct on_demand_supply {
	struct regulator *supply;
	atomic_t ref;
};

struct msm8x16_wcd_priv {
	struct snd_soc_codec *codec;
	u16 pmic_rev;
	u16 codec_version;
	u32 boost_voltage;
	u32 adc_count;
	u32 rx_bias_count;
	s32 dmic_1_2_clk_cnt;
	u32 mute_mask;
	bool mclk_enabled;
	bool clock_active;
	bool config_mode_active;
	u16 boost_option;
	bool spk_boost_set;
	bool ear_pa_boost_set;
	bool ext_spk_boost_set;
	bool dec_active[NUM_DECIMATORS];
	struct on_demand_supply on_demand_list[ON_DEMAND_SUPPLIES_MAX];
	struct regulator *spkdrv_reg;
	/* mbhc module */
	struct wcd_mbhc mbhc;
	/* cal info for codec */
	struct fw_info *fw_data;
	struct blocking_notifier_head notifier;
	unsigned long status_mask;
	int (*codec_spk_ext_pa_cb)(struct snd_soc_codec *codec, int enable);
};

extern int msm8x16_wcd_mclk_enable(struct snd_soc_codec *codec, int mclk_enable,
			     bool dapm);

extern int msm8x16_wcd_hs_detect(struct snd_soc_codec *codec,
		    struct wcd_mbhc_config *mbhc_cfg);

extern void msm8x16_wcd_hs_detect_exit(struct snd_soc_codec *codec);

extern int msm8x16_register_notifier(struct snd_soc_codec *codec,
				     struct notifier_block *nblock);

extern int msm8x16_unregister_notifier(struct snd_soc_codec *codec,
					 struct notifier_block *nblock);
extern int msm8x16_wcd_restart_mbhc(struct snd_soc_codec *codec);
extern void msm8x16_wcd_spk_ext_pa_cb(
		int (*codec_spk_ext_pa)(struct snd_soc_codec *codec,
		int enable), struct snd_soc_codec *codec);
#endif

