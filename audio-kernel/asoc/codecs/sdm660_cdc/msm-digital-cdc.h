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

#define HPHL_PA_DISABLE (0x01 << 1)
#define HPHR_PA_DISABLE (0x01 << 2)
#define SPKR_PA_DISABLE (0x01 << 3)
#define MAX_REGULATOR   7
#define NUM_DECIMATORS	5
/* Codec supports 1 compander */
enum {
	COMPANDER_NONE = 0,
	COMPANDER_1, /* HPHL/R */
	COMPANDER_MAX,
};

/* Number of output I2S port */
enum {
	MSM89XX_RX1 = 0,
	MSM89XX_RX2,
	MSM89XX_RX3,
	MSM89XX_RX_MAX,
};

enum {
	ON_DEMAND_DIGITAL = 0,
	ON_DEMAND_DIG_SUPPLIES_MAX,
};

struct on_demand_dig_supply {
	struct regulator *supply;
	atomic_t ref;
	int min_uv;
	int max_uv;
	int optimum_ua;
};

struct dig_cdc_regulator {
	const char *name;
	int min_uv;
	int max_uv;
	int optimum_ua;
	struct regulator *regulator;
};

struct tx_mute_work {
	struct msm_dig_priv *dig_cdc;
	u32 decimator;
	struct delayed_work dwork;
};

struct msm_cap_mode {
	u8 micbias1_cap_mode;
	u8 micbias2_cap_mode;
};

struct msm_dig_priv {
	struct snd_soc_codec *codec;
	struct device *dev;
	u32 comp_enabled[MSM89XX_RX_MAX];
	int (*codec_hph_comp_gpio)(bool enable, struct snd_soc_codec *codec);
	s32 dmic_1_2_clk_cnt;
	s32 dmic_3_4_clk_cnt;
	bool dec_active[NUM_DECIMATORS];
	int version;
	/* cal info for codec */
	struct fw_info *fw_data;
	bool no_analog_codec;
	/* Entry for version info */
	struct snd_info_entry *entry;
	struct snd_info_entry *version_entry;
	char __iomem *dig_base;
	struct regmap *regmap;
	struct notifier_block nblock;
	u32 mute_mask;
	int dapm_bias_off;
	void *handle;
	struct on_demand_dig_supply on_demand_list[ON_DEMAND_DIG_SUPPLIES_MAX];
	void (*set_compander_mode)(void *handle, int val);
	void (*update_clkdiv)(void *handle, int val);
	int (*get_cdc_version)(void *handle);
	int (*register_notifier)(void *handle,
				 struct notifier_block *nblock,
				 bool enable);
	struct tx_mute_work tx_mute_dwork[NUM_DECIMATORS];
	u32 num_of_supplies;
	struct regulator_bulk_data *supplies;
	struct dig_cdc_regulator regulator[MAX_REGULATOR];
};

struct dig_ctrl_platform_data {
	void *handle;
	void (*set_compander_mode)(void *handle, int val);
	void (*update_clkdiv)(void *handle, int val);
	int (*get_cdc_version)(void *handle);
	int (*register_notifier)(void *handle,
				 struct notifier_block *nblock,
				 bool enable);
};

struct hpf_work {
	struct msm_dig_priv *dig_cdc;
	u32 decimator;
	u8 tx_hpf_cut_of_freq;
	struct delayed_work dwork;
};

/* Codec supports 5 bands */
enum {
	BAND1 = 0,
	BAND2,
	BAND3,
	BAND4,
	BAND5,
	BAND_MAX,
};

#if (IS_ENABLED(CONFIG_SND_SOC_DIGITAL_CDC) || \
	 IS_ENABLED(CONFIG_SND_SOC_DIGITAL_CDC_LEGACY))
extern void msm_dig_cdc_hph_comp_cb(
		int (*codec_hph_comp_gpio)(
			bool enable, struct snd_soc_codec *codec),
		struct snd_soc_codec *codec);
int msm_dig_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					  struct snd_soc_codec *codec);
extern int msm_digcdc_mclk_enable(struct snd_soc_codec *codec,
				int mclk_enable, bool dapm);
#else /* CONFIG_SND_SOC_DIGITAL_CDC */
static inline void msm_dig_cdc_hph_comp_cb(
		int (*codec_hph_comp_gpio)(
			bool enable, struct snd_soc_codec *codec),
		struct snd_soc_codec *codec)
{

}
static inline int msm_dig_codec_info_create_codec_entry(
				struct snd_info_entry *codec_root,
				struct snd_soc_codec *codec)
{
	return 0;
}
static inline int msm_digcdc_mclk_enable(struct snd_soc_codec *codec,
				int mclk_enable, bool dapm)
{
	return 0;
}
#endif /* CONFIG_SND_SOC_DIGITAL_CDC */
#endif
