/*
 * arch/arm/mach-tegra/include/mach/tegra_asoc_vcm_pdata.h
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __MACH_TEGRA_P1852_PDATA_H
#define __MACH_TEGRA_P1852_PDATA_H

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define NUM_AUDIO_CONTROLLERS 2
#else
#define NUM_AUDIO_CONTROLLERS 4
#endif

/* data format supported */
enum i2s_data_format {
	format_i2s = 0x1,
	format_dsp = 0x2,
	format_rjm = 0x4,
	format_ljm = 0x8,
	format_tdm = 0x10
};

struct codec_info_s {
	/* Name of the Codec Dai on the system */
	char *codec_dai_name;
	/* Name of the I2S controller dai its connected to */
	char *cpu_dai_name;
	char *codec_name;	/* Name of the Codec Driver */
	char *name;			/* Name of the Codec-Dai-Link */
	char *pcm_driver;	/* Name of the PCM driver */
	enum i2s_data_format i2s_format;
	int master;			/* Codec is Master or Slave */
	/* TDM format setttings */
	int num_slots;		/* Number of TDM slots */
	int slot_width;		/* Width of each slot */
	int rx_mask;		/* Number of Rx Enabled slots */
	int tx_mask;		/* Number of Tx Enabled slots */

};

/* used for T20, to select the DAC/DAPs */
struct dac_info_s {
	int dac_id;
	int dap_id;
};

struct tegra_asoc_vcm_platform_data {
	struct codec_info_s codec_info[NUM_AUDIO_CONTROLLERS];
	/* Valid for Tegra2 */
	struct dac_info_s dac_info[NUM_AUDIO_CONTROLLERS];
};
#endif
