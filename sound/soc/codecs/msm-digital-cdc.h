/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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
#ifndef MSM_DIGITAL_CDC_H
#define MSM_DIGITAL_CDC_H

#include <linux/regmap.h>
#include "msm-digital-cdc-registers.h"
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <sound/apr_audio-v2.h>
#include "wcdcal-hwdep.h"

#define HPHL_PA_DISABLE (0x01 << 1)
#define HPHR_PA_DISABLE (0x01 << 2)
#define SPKR_PA_DISABLE (0x01 << 3)

#define NUM_DECIMATORS	2

#define MAX_REGULATOR	2

extern struct reg_default
		msm89xx_cdc_core_defaults[MSM89XX_CDC_CORE_CACHE_SIZE];

bool msm89xx_cdc_core_readable_reg(struct device *dev, unsigned int reg);
bool msm89xx_cdc_core_writeable_reg(struct device *dev, unsigned int reg);
bool msm89xx_cdc_core_volatile_reg(struct device *dev, unsigned int reg);

enum {
	MSM89XX_RX1 = 0,
	MSM89XX_RX2,
	MSM89XX_RX3,
	MSM89XX_RX_MAX,
};

enum {
	AIF1_PB = 0,
	AIF1_CAP,
	NUM_CODEC_DAIS,
};

/* Support different hph modes */
enum {
	NORMAL_MODE = 0,
	HD2_MODE,
};

enum {
	ON_DEMAND_DIGIT = 0,
	ON_DEMAND_SUPPLIES_MAX,
};

struct tx_mute_work {
	struct msm_dig_priv *dig_cdc;
	u32 decimator;
	struct delayed_work dwork;
};

struct msm_cdc_regulator {
	const char *name;
	int min_uv;
	int max_uv;
	int optimum_ua;
	bool ondemand;
	struct regulator *regulator;
};

struct on_demand_supply {
	struct regulator *supply;
	atomic_t ref;
	int min_uv;
	int max_uv;
	int optimum_ua;
};

struct msm_dig_ctrl_data {
	struct platform_device *dig_pdev;
};

struct msm_dig_priv {
	struct device *dev;
	struct snd_soc_codec *codec;
	s32 dmic_1_2_clk_cnt;
	bool dec_active[NUM_DECIMATORS];
	int version;
	char __iomem *dig_base;
	struct regmap *regmap;
	struct notifier_block nblock;
	u32 mute_mask;
	int dapm_bias_off;
	struct tx_mute_work tx_mute_dwork[NUM_DECIMATORS];
	struct msm_dig_ctrl_data *dig_ctrl_data;
	struct on_demand_supply on_demand_list[ON_DEMAND_SUPPLIES_MAX];
	u32 num_of_supplies;
	struct regulator_bulk_data *supplies;
	/* cal info for codec */
	struct fw_info *fw_data;
	/* Entry for version info */
	struct snd_info_entry *entry;
	struct snd_info_entry *version_entry;
};

struct msm_cdc_pdata {
	struct msm_cdc_regulator regulator[MAX_REGULATOR];
};
struct hpf_work {
	struct msm_dig_priv *dig_cdc;
	u32 decimator;
	u8 tx_hpf_cut_of_freq;
	struct delayed_work dwork;
};

enum {
	BAND1 = 0,
	BAND2,
	BAND3,
	BAND4,
	BAND5,
	BAND_MAX,
};

struct msm_asoc_mach_data {
	struct device_node *pdm_gpio_p; /* used by pinctrl API */
	struct device_node *dmic_gpio_p; /* used by pinctrl API */
	struct snd_soc_codec *codec;
	struct snd_info_entry *codec_root;
	int mclk_freq;
	bool native_clk_set;
	int lb_mode;
	int snd_card_val;
	atomic_t int_mclk0_rsc_ref;
	atomic_t int_mclk0_enabled;
	struct mutex cdc_mclk_mutex;
	struct delayed_work disable_int_mclk0_work;
	int afe_clk_ver;
	int ext_pa;
	atomic_t mclk_rsc_ref;
	atomic_t mclk_enabled;
	struct delayed_work disable_mclk_work;
	struct afe_digital_clk_cfg digital_cdc_clk;
	struct afe_clk_set digital_cdc_core_clk;
	void __iomem *vaddr_gpio_mux_spkr_ctl;
	void __iomem *vaddr_gpio_mux_mic_ctl;
	void __iomem *vaddr_gpio_mux_quin_ctl;
	void __iomem *vaddr_gpio_mux_pcm_ctl;
};

extern int msm_digcdc_mclk_enable(struct snd_soc_codec *codec,
			int mclk_enable, bool dapm);
int msm_dig_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					  struct snd_soc_codec *codec);
#endif
