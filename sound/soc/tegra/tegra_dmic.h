/*
 * tegra_dmic.h - Tegra DMIC driver
 *
 * Author: Ankit Gupta <ankitgupta@nvidia.com>
 * Copyright (C) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef _TEGRA_DMIC_H
#define _TEGRA_DMIC_H

#include <sound/soc.h>
#include "tegra_pcm.h"

/* Offsets for DMIC registers. */

#define TEGRA_DMIC_CTRL			0x00
#define TEGRA_DMIC_FILTER_CTRL			0x04
#define TEGRA_DMIC_AUDIO_CIF_TX_CTRL		0x08
#define TEGRA_DMIC_DCR_FILTER_GAIN		0x0c

/* Shifts for DMIC_CTRL register. */

#define TEGRA_DMIC_ENABLE_SHIFT		0
#define TEGRA_DMIC_LRSEL_POLARITY_SHIFT	1
#define TEGRA_DMIC_CLOCKING_GATE_SHIFT		2
#define TEGRA_DMIC_OSR_SHIFT			4
#define TEGRA_DMIC_SINC_DEC_ORDER_SHIFT	8
#define TEGRA_DMIC_OUTPUT_WIDTH_SHIFT		12
#define TEGRA_DMIC_CHANNEL_SELECT_SHIFT	24
#define TEGRA_DMIC_DMICFILT_BYPASS_SHIFT	31

/* Shifts for DMIC_FILTER_CTRL register. */

#define TEGRA_DMIC_SOFT_RESET_SHIFT		0
#define TEGRA_DMIC_DCR_FILTER_SHIFT		4
#define TEGRA_DMIC_LP_FILTER_SHIFT		8
#define TEGRA_DMIC_SC_FILTER_SHIFT		12
#define TEGRA_DMIC_TRIMMER_SEL_SHIFT		16

/* Masks for DMIC_CTRL register. */

#define TEGRA_DMIC_ENABLE_MASK			~(0x1 << \
						TEGRA_DMIC_ENABLE_SHIFT)
#define TEGRA_DMIC_LRSEL_POLARITY_MASK		~(0x1 << \
						TEGRA_DMIC_LRSEL_POLARITY_SHIFT)
#define TEGRA_DMIC_CLOCKING_GATE_MASK		~(0x1 << \
						TEGRA_DMIC_CLOCKING_GATE_SHIFT)
#define TEGRA_DMIC_OSR_MASK			~(0x3 << TEGRA_DMIC_OSR_SHIFT)
#define TEGRA_DMIC_SINC_DEC_ORDER_MASK		~(0x1 << \
						TEGRA_DMIC_SINC_DEC_ORDER_SHIFT)
#define TEGRA_DMIC_OUTPUT_WIDTH_MASK		~(0x3 << \
						TEGRA_DMIC_OUTPUT_WIDTH_SHIFT)
#define TEGRA_DMIC_CHANNEL_SELECT_MASK		~(0x3 << \
						TEGRA_DMIC_CHANNEL_SELECT_SHIFT)
#define TEGRA_DMIC_DMICFILT_BYPASS_MASK	~(0x1 << \
					TEGRA_DMIC_DMICFILT_BYPASS_SHIFT)

/* Masks for DMIC_FILTER_CTRL register. */

#define TEGRA_DMIC_SOFT_RESET_MASK		~(0x1 << \
						TEGRA_DMIC_SOFT_RESET_SHIFT)
#define TEGRA_DMIC_DCR_FILTER_MASK		~(0x1 << \
						TEGRA_DMIC_DCR_FILTER_SHIFT)
#define TEGRA_DMIC_LP_FILTER_MASK		~(0x1 << \
						TEGRA_DMIC_LP_FILTER_SHIFT)
#define TEGRA_DMIC_SC_FILTER_MASK		~(0x1 << \
						TEGRA_DMIC_SC_FILTER_SHIFT)
#define TEGRA_DMIC_TRIMMER_SEL_MASK		~(0x1 << \
						TEGRA_DMIC_TRIMMER_SEL_SHIFT)

/* DMIC DAI ids. */
#define TEGRA_DMIC_FRONT			0
#define TEGRA_DMIC_BACK			1

/* DMIC Rates. */
#define TEGRA_DMIC_RATES			(SNDRV_PCM_RATE_8000 | \
						SNDRV_PCM_RATE_16000 | \
						SNDRV_PCM_RATE_44100 | \
						SNDRV_PCM_RATE_48000)
#define TEGRA_DMIC_FORMATS			(SNDRV_PCM_FMTBIT_S16_LE | \
						SNDRV_PCM_FMTBIT_S20_3LE | \
						SNDRV_PCM_FMTBIT_S24_LE)

#define TEGRA_DMIC_COUNT			2

struct tegra_dmic_cif {
	int					threshold;
	int					audio_channels;
	int					client_channels;
	int					audio_bits;
	int					client_bits;
	int					expand;
	int					stereo_conv;
	int					replicate;
	int					truncate;
	int					mono_conv;
};

void						tegra_dmic_set_acif(
						struct snd_soc_dai *dai,
					struct tegra_dmic_cif *cif_info);

void						tegra_dmic_set_rx_cif(
						struct snd_soc_dai *dai,
						struct tegra_dmic_cif *cif_info,
						u32 data_format);

#endif /*_TEGRA_DMIC_H*/
