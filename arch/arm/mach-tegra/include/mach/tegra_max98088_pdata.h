/*
 * arch/arm/mach-tegra/include/mach/tegra_max98088_pdata.h
 *
 * Copyright 2011 NVIDIA, Inc.
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
#define	NUM_I2S_DEVICES		3

struct baseband_config {
	int rate;
	int channels;
};

struct tegra_max98088_platform_data {
	int gpio_spkr_en;
	int gpio_hp_det;
	int gpio_hp_mute;
	int gpio_int_mic_en;
	int gpio_ext_mic_en;
	int audio_port_id[NUM_I2S_DEVICES];
	struct baseband_config baseband_param;
};
