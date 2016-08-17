/*
 * arch/arm/mach-tegra/include/mach/tegra_asoc_pdata.h
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define	HIFI_CODEC		0
#define	BASEBAND		1
#define	BT_SCO			2
#define	VOICE_CODEC		3
#define	NUM_I2S_DEVICES		4

#define	TEGRA_DAIFMT_DSP_A		0
#define	TEGRA_DAIFMT_DSP_B		1
#define	TEGRA_DAIFMT_I2S		2
#define	TEGRA_DAIFMT_RIGHT_J		3
#define	TEGRA_DAIFMT_LEFT_J		4

struct i2s_config {
	int audio_port_id;
	int is_i2s_master;
	int i2s_mode;
	int sample_size;
	int rate;
	int channels;
	int bit_clk;
};

enum tegra_speaker_edp_states {
	TEGRA_SPK_EDP_NEG_1,
	TEGRA_SPK_EDP_ZERO,
	TEGRA_SPK_EDP_1,
	TEGRA_SPK_EDP_NUM_STATES,
};

struct tegra_asoc_platform_data {
	const char *codec_name;
	const char *codec_dai_name;
	int gpio_spkr_en;
	int gpio_hp_det;
	int gpio_hp_mute;
	int gpio_int_mic_en;
	int gpio_ext_mic_en;
	int gpio_ldo1_en;
	int gpio_codec1;
	int gpio_codec2;
	int gpio_codec3;
	bool micbias_gpio_absent;
	bool use_codec_jd_irq;
	unsigned int debounce_time_hp;
	bool edp_support;
	unsigned int edp_states[TEGRA_SPK_EDP_NUM_STATES];
	int edp_vol[TEGRA_SPK_EDP_NUM_STATES];
	struct i2s_config i2s_param[NUM_I2S_DEVICES];
};
