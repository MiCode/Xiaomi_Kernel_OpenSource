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
#ifndef MSM_ANALOG_CDC_H
#define MSM_ANALOG_CDC_H

#include <sound/soc.h>
#include <sound/jack.h>
#include <dsp/q6afe-v2.h>
#include "../wcd-mbhc-v2.h"
#include "../wcdcal-hwdep.h"
#include "sdm660-cdc-registers.h"

#define MICBIAS_EXT_BYP_CAP 0x00
#define MICBIAS_NO_EXT_BYP_CAP 0x01

#define MSM89XX_NUM_IRQ_REGS	2
#define MAX_REGULATOR		7
#define MSM89XX_REG_VAL(reg, val)	{reg, 0, val}

#define MSM89XX_VDD_SPKDRV_NAME "cdc-vdd-spkdrv"

#define DEFAULT_MULTIPLIER 800
#define DEFAULT_GAIN 9
#define DEFAULT_OFFSET 100

extern const u8 msm89xx_pmic_cdc_reg_readable[MSM89XX_PMIC_CDC_CACHE_SIZE];
extern const u8 msm89xx_cdc_core_reg_readable[MSM89XX_CDC_CORE_CACHE_SIZE];
extern struct regmap_config msm89xx_cdc_core_regmap_config;
extern struct regmap_config msm89xx_pmic_cdc_regmap_config;

enum wcd_curr_ref {
	I_h4_UA = 0,
	I_pt5_UA,
	I_14_UA,
	I_l4_UA,
	I_1_UA,
};

enum wcd_mbhc_imp_det_pin {
	WCD_MBHC_DET_NONE = 0,
	WCD_MBHC_DET_HPHL,
	WCD_MBHC_DET_HPHR,
	WCD_MBHC_DET_BOTH,
};


/* Each micbias can be assigned to one of three cfilters
 * Vbatt_min >= .15V + ldoh_v
 * ldoh_v >= .15v + cfiltx_mv
 * If ldoh_v = 1.95 160 mv < cfiltx_mv < 1800 mv
 * If ldoh_v = 2.35 200 mv < cfiltx_mv < 2200 mv
 * If ldoh_v = 2.75 240 mv < cfiltx_mv < 2600 mv
 * If ldoh_v = 2.85 250 mv < cfiltx_mv < 2700 mv
 */

struct wcd_micbias_setting {
	u8 ldoh_v;
	u32 cfilt1_mv; /* in mv */
	u32 cfilt2_mv; /* in mv */
	u32 cfilt3_mv; /* in mv */
	/* Different WCD9xxx series codecs may not
	 * have 4 mic biases. If a codec has fewer
	 * mic biases, some of these properties will
	 * not be used.
	 */
	u8 bias1_cfilt_sel;
	u8 bias2_cfilt_sel;
	u8 bias3_cfilt_sel;
	u8 bias4_cfilt_sel;
	u8 bias1_cap_mode;
	u8 bias2_cap_mode;
	u8 bias3_cap_mode;
	u8 bias4_cap_mode;
	bool bias2_is_headset_only;
};

enum sdm660_cdc_pid_current {
	MSM89XX_PID_MIC_2P5_UA,
	MSM89XX_PID_MIC_5_UA,
	MSM89XX_PID_MIC_10_UA,
	MSM89XX_PID_MIC_20_UA,
};

struct sdm660_cdc_reg_mask_val {
	u16	reg;
	u8	mask;
	u8	val;
};

enum {
	/* INTR_REG 0 - Digital Periph */
	MSM89XX_IRQ_SPKR_CNP = 0,
	MSM89XX_IRQ_SPKR_CLIP,
	MSM89XX_IRQ_SPKR_OCP,
	MSM89XX_IRQ_MBHC_INSREM_DET1,
	MSM89XX_IRQ_MBHC_RELEASE,
	MSM89XX_IRQ_MBHC_PRESS,
	MSM89XX_IRQ_MBHC_INSREM_DET,
	MSM89XX_IRQ_MBHC_HS_DET,
	/* INTR_REG 1 - Analog Periph */
	MSM89XX_IRQ_EAR_OCP,
	MSM89XX_IRQ_HPHR_OCP,
	MSM89XX_IRQ_HPHL_OCP,
	MSM89XX_IRQ_EAR_CNP,
	MSM89XX_IRQ_HPHR_CNP,
	MSM89XX_IRQ_HPHL_CNP,
	MSM89XX_NUM_IRQS,
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

struct sdm660_cdc_regulator {
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

struct wcd_imped_i_ref {
	enum wcd_curr_ref curr_ref;
	int min_val;
	int multiplier;
	int gain_adj;
	int offset;
};

enum sdm660_cdc_micbias_num {
	MSM89XX_MICBIAS1 = 0,
};

/* Hold instance to digital codec platform device */
struct msm_dig_ctrl_data {
	struct platform_device *dig_pdev;
};

struct msm_dig_ctrl_platform_data {
	void *handle;
	void (*set_compander_mode)(void *handle, int val);
	void (*update_clkdiv)(void *handle, int val);
	int (*get_cdc_version)(void *handle);
	int (*register_notifier)(void *handle,
				 struct notifier_block *nblock,
				 bool enable);
};

struct sdm660_cdc_priv {
	struct device *dev;
	u32 num_of_supplies;
	struct regulator_bulk_data *supplies;
	struct snd_soc_codec *codec;
	struct work_struct msm_anlg_add_child_devices_work;
	struct msm_dig_ctrl_platform_data dig_plat_data;
	/* digital codec data structure */
	struct msm_dig_ctrl_data *dig_ctrl_data;
	struct blocking_notifier_head notifier;
	u16 pmic_rev;
	u16 codec_version;
	u16 analog_major_rev;
	u32 boost_voltage;
	u32 adc_count;
	u32 rx_bias_count;
	bool int_mclk0_enabled;
	u16 boost_option;
	/* mode to select hd2 */
	u32 hph_mode;
	/* compander used for each rx chain */
	bool spk_boost_set;
	bool ear_pa_boost_set;
	bool ext_spk_boost_set;
	struct on_demand_supply on_demand_list[ON_DEMAND_SUPPLIES_MAX];
	struct regulator *spkdrv_reg;
	struct blocking_notifier_head notifier_mbhc;
	/* mbhc module */
	struct wcd_mbhc mbhc;
	/* cal info for codec */
	struct fw_info *fw_data;
	struct notifier_block audio_ssr_nb;
	int (*codec_spk_ext_pa_cb)(struct snd_soc_codec *codec, int enable);
	unsigned long status_mask;
	struct wcd_imped_i_ref imped_i_ref;
	enum wcd_mbhc_imp_det_pin imped_det_pin;
	/* Entry for version info */
	struct snd_info_entry *entry;
	struct snd_info_entry *version_entry;
};

struct sdm660_cdc_pdata {
	struct wcd_micbias_setting micbias;
	struct sdm660_cdc_regulator regulator[MAX_REGULATOR];
};


extern int msm_anlg_cdc_mclk_enable(struct snd_soc_codec *codec,
				    int mclk_enable, bool dapm);

extern int msm_anlg_cdc_hs_detect(struct snd_soc_codec *codec,
		    struct wcd_mbhc_config *mbhc_cfg);

extern void msm_anlg_cdc_hs_detect_exit(struct snd_soc_codec *codec);

extern void sdm660_cdc_update_int_spk_boost(bool enable);

extern void msm_anlg_cdc_spk_ext_pa_cb(
		int (*codec_spk_ext_pa)(struct snd_soc_codec *codec,
		int enable), struct snd_soc_codec *codec);
int msm_anlg_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					   struct snd_soc_codec *codec);
#endif
