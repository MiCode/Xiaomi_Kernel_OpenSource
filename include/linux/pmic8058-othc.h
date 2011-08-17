/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __PMIC8058_OTHC_H__
#define __PMIC8058_OTHC_H__

/* Accessory detecion flags */
#define OTHC_MICBIAS_DETECT	BIT(0)
#define OTHC_GPIO_DETECT	BIT(1)
#define OTHC_SWITCH_DETECT	BIT(2)
#define OTHC_ADC_DETECT		BIT(3)

enum othc_accessory_type {
	OTHC_NO_DEVICE = 0,
	OTHC_HEADSET = 1 << 0,
	OTHC_HEADPHONE = 1 << 1,
	OTHC_MICROPHONE = 1 << 2,
	OTHC_ANC_HEADSET = 1 << 3,
	OTHC_ANC_HEADPHONE = 1 << 4,
	OTHC_ANC_MICROPHONE = 1 << 5,
	OTHC_SVIDEO_OUT = 1 << 6,
};

struct accessory_adc_thres {
	int min_threshold;
	int max_threshold;
};

struct othc_accessory_info {
	unsigned int accessory;
	unsigned int detect_flags;
	unsigned int gpio;
	unsigned int active_low;
	unsigned int key_code;
	bool enabled;
	struct accessory_adc_thres adc_thres;
};

enum othc_headset_type {
	OTHC_HEADSET_NO,
	OTHC_HEADSET_NC,
};

struct othc_regulator_config {
	const char *regulator;
	unsigned int max_uV;
	unsigned int min_uV;
};

/* Signal control for OTHC module */
enum othc_micbias_enable {
	/* Turn off MICBIAS signal */
	OTHC_SIGNAL_OFF,
	/* Turn on MICBIAS signal when TCXO is enabled */
	OTHC_SIGNAL_TCXO,
	/* Turn on MICBIAS signal when PWM is high or TCXO is enabled */
	OTHC_SIGNAL_PWM_TCXO,
	/* MICBIAS always enabled */
	OTHC_SIGNAL_ALWAYS_ON,
};

/* Number of MICBIAS lines supported by PMIC8058 */
enum othc_micbias {
	OTHC_MICBIAS_0,
	OTHC_MICBIAS_1,
	OTHC_MICBIAS_2,
	OTHC_MICBIAS_MAX,
};

enum othc_micbias_capability {
	/* MICBIAS used only for BIAS with on/off capability */
	OTHC_MICBIAS,
	/* MICBIAS used to support HSED functionality */
	OTHC_MICBIAS_HSED,
};

struct othc_switch_info {
	u32 min_adc_threshold;
	u32 max_adc_threshold;
	u32 key_code;
};

struct othc_n_switch_config {
	u32 voltage_settling_time_ms;
	u8 num_adc_samples;
	uint32_t adc_channel;
	struct othc_switch_info *switch_info;
	u8 num_keys;
	bool default_sw_en;
	u8 default_sw_idx;
};

struct hsed_bias_config {
	enum othc_headset_type othc_headset;
	u16 othc_lowcurr_thresh_uA;
	u16 othc_highcurr_thresh_uA;
	u32 othc_hyst_prediv_us;
	u32 othc_period_clkdiv_us;
	u32 othc_hyst_clk_us;
	u32 othc_period_clk_us;
	int othc_wakeup;
};

/* Configuration data for HSED */
struct othc_hsed_config {
	struct hsed_bias_config *hsed_bias_config;
	unsigned long detection_delay_ms;
	/* Switch configuration */
	unsigned long switch_debounce_ms;
	bool othc_support_n_switch; /* Set if supporting > 1 switch */
	struct othc_n_switch_config *switch_config;
	/* Accessory configuration */
	bool accessories_support;
	bool accessories_adc_support;
	uint32_t accessories_adc_channel;
	struct othc_accessory_info *accessories;
	int othc_num_accessories;
	int video_out_gpio;
	int ir_gpio;
};

struct pmic8058_othc_config_pdata {
	enum othc_micbias micbias_select;
	enum othc_micbias_enable micbias_enable;
	enum othc_micbias_capability micbias_capability;
	struct othc_hsed_config *hsed_config;
	const char *hsed_name;
	struct othc_regulator_config *micbias_regulator;
};

int pm8058_micbias_enable(enum othc_micbias micbias,
			enum othc_micbias_enable enable);
int pm8058_othc_svideo_enable(enum othc_micbias micbias,
			bool enable);

#endif /* __PMIC8058_OTHC_H__ */
