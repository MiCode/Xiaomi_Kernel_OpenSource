/*
 * tegra30_dam.h - Tegra 30 DAM driver.
 *
 * Author: Nikesh Oswal <noswal@nvidia.com>
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
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
 *
 */

#ifndef __TEGRA30_DAM_H
#define __TEGRA30_DAM_H

/* Register offsets from TEGRA30_DAM*_BASE */
#define TEGRA30_DAM_CTRL				0
#define TEGRA30_DAM_CLIP				4
#define TEGRA30_DAM_CLIP_THRESHOLD			8
#define TEGRA30_DAM_AUDIOCIF_OUT_CTRL			0x0C
#define TEGRA30_DAM_CH0_CTRL				0x10
#define TEGRA30_DAM_CH0_CONV				0x14
#define TEGRA30_DAM_AUDIOCIF_CH0_CTRL			0x1C
#define TEGRA30_DAM_CH1_CTRL				0x20
#define TEGRA30_DAM_CH1_CONV				0x24
#define TEGRA30_DAM_AUDIOCIF_CH1_CTRL			0x2C
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
#define TEGRA30_DAM_CH0_BIQUAD_FIXED_COEF_0		0xf0
#define TEGRA30_DAM_FARROW_PARAM_0			0xf4
#define TEGRA30_DAM_AUDIORAMCTL_DAM_CTRL_0		0xf8
#define TEGRA30_DAM_AUDIORAMCTL_DAM_DATA_0		0xfc
#define TEGRA30_DAM_CTRL_REGINDEX			(TEGRA30_DAM_AUDIORAMCTL_DAM_DATA_0 >> 2)
#else
#define TEGRA30_DAM_CTRL_REGINDEX			(TEGRA30_DAM_AUDIOCIF_CH1_CTRL >> 2)
#endif
#define TEGRA30_DAM_CTRL_RSVD_6				6
#define TEGRA30_DAM_CTRL_RSVD_10			10

#define TEGRA30_NR_DAM_IFC				3

#define TEGRA30_DAM_NUM_INPUT_CHANNELS			2

/* Fields in TEGRA30_DAM_CTRL */
#define TEGRA30_DAM_CTRL_SOFT_RESET_ENABLE		(1 << 31)
#define TEGRA30_DAM_CTRL_FSOUT_SHIFT			4
#define TEGRA30_DAM_CTRL_FSOUT_MASK			(0xf << TEGRA30_DAM_CTRL_FSOUT_SHIFT)
#define TEGRA30_DAM_FS_8KHZ				0
#define TEGRA30_DAM_FS_16KHZ				1
#define TEGRA30_DAM_FS_44KHZ				2
#define TEGRA30_DAM_FS_48KHZ				3
#define TEGRA30_DAM_CTRL_FSOUT_FS8			(TEGRA30_DAM_FS_8KHZ << TEGRA30_DAM_CTRL_FSOUT_SHIFT)
#define TEGRA30_DAM_CTRL_FSOUT_FS16			(TEGRA30_DAM_FS_16KHZ << TEGRA30_DAM_CTRL_FSOUT_SHIFT)
#define TEGRA30_DAM_CTRL_FSOUT_FS44			(TEGRA30_DAM_FS_44KHZ << TEGRA30_DAM_CTRL_FSOUT_SHIFT)
#define TEGRA30_DAM_CTRL_FSOUT_FS48			(TEGRA30_DAM_FS_48KHZ << TEGRA30_DAM_CTRL_FSOUT_SHIFT)
#define TEGRA30_DAM_CTRL_CG_EN				(1 << 1)
#define TEGRA30_DAM_CTRL_DAM_EN				(1 << 0)
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
#define TEGRA30_DAM_CTRL_STEREO_MIXING_ENABLE	(1 << 3)
#endif


/* Fields in TEGRA30_DAM_CLIP */
#define TEGRA30_DAM_CLIP_COUNTER_ENABLE			(1 << 31)
#define TEGRA30_DAM_CLIP_COUNT_MASK			0x7fffffff


/* Fields in TEGRA30_DAM_CH0_CTRL */
#define TEGRA30_STEP_RESET				1
#define TEGRA30_DAM_DATA_SYNC				1
#define TEGRA30_DAM_DATA_SYNC_SHIFT			4
#define TEGRA30_DAM_CH0_CTRL_FSIN_SHIFT			8
#define TEGRA30_DAM_CH0_CTRL_STEP_SHIFT			16
#define TEGRA30_DAM_CH0_CTRL_STEP_MASK			(0xffff << 16)
#define TEGRA30_DAM_CH0_CTRL_STEP_RESET			(TEGRA30_STEP_RESET << 16)
#define TEGRA30_DAM_CH0_CTRL_FSIN_MASK			(0xf << 8)
#define TEGRA30_DAM_CH0_CTRL_FSIN_FS8			(TEGRA30_DAM_FS_8KHZ << 8)
#define TEGRA30_DAM_CH0_CTRL_FSIN_FS16			(TEGRA30_DAM_FS_16KHZ << 8)
#define TEGRA30_DAM_CH0_CTRL_FSIN_FS44			(TEGRA30_DAM_FS_44KHZ << 8)
#define TEGRA30_DAM_CH0_CTRL_FSIN_FS48			(TEGRA30_DAM_FS_48KHZ << 8)
#define TEGRA30_DAM_CH0_CTRL_DATA_SYNC_MASK		(0xf << TEGRA30_DAM_DATA_SYNC_SHIFT)
#define TEGRA30_DAM_CH0_CTRL_DATA_SYNC			(TEGRA30_DAM_DATA_SYNC << TEGRA30_DAM_DATA_SYNC_SHIFT)
#define TEGRA30_DAM_CH0_CTRL_EN				(1 << 0)
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
#define TEGRA30_DAM_CH0_CTRL_COEFF_RAM_ENABLE		(1 << 15)
#define TEGRA30_DAM_CH0_CTRL_FILT_STAGES_SHIFT	16
#endif


/* Fields in TEGRA30_DAM_CH0_CONV */
#define TEGRA30_DAM_GAIN				1
#define TEGRA30_DAM_GAIN_SHIFT				0
#define TEGRA30_DAM_CH0_CONV_GAIN			(TEGRA30_DAM_GAIN << TEGRA30_DAM_GAIN_SHIFT)

/* Fields in TEGRA30_DAM_CH1_CTRL */
#define TEGRA30_DAM_CH1_CTRL_DATA_SYNC_MASK		(0xf << TEGRA30_DAM_DATA_SYNC_SHIFT)
#define TEGRA30_DAM_CH1_CTRL_DATA_SYNC			(TEGRA30_DAM_DATA_SYNC << TEGRA30_DAM_DATA_SYNC_SHIFT)
#define TEGRA30_DAM_CH1_CTRL_EN				(1 << 0)

/* Fields in TEGRA30_DAM_CH1_CONV */
#define TEGRA30_DAM_CH1_CONV_GAIN			(TEGRA30_DAM_GAIN << TEGRA30_DAM_GAIN_SHIFT)

#define TEGRA30_AUDIO_CHANNELS_SHIFT			24
#define TEGRA30_AUDIO_CHANNELS_MASK			(7 << TEGRA30_AUDIO_CHANNELS_SHIFT)
#define TEGRA30_CLIENT_CHANNELS_SHIFT			16
#define TEGRA30_CLIENT_CHANNELS_MASK			(7 << TEGRA30_CLIENT_CHANNELS_SHIFT)
#define TEGRA30_AUDIO_BITS_SHIFT			12
#define TEGRA30_AUDIO_BITS_MASK				(7 << TEGRA30_AUDIO_BITS_SHIFT)
#define TEGRA30_CLIENT_BITS_SHIFT			8
#define TEGRA30_CLIENT_BITS_MASK			(7 << TEGRA30_CLIENT_BITS_SHIFT)
#define TEGRA30_CIF_DIRECTION_TX			(0 << 2)
#define TEGRA30_CIF_DIRECTION_RX			(1 << 2)
#define TEGRA30_CIF_BIT24				5
#define TEGRA30_CIF_BIT16				3
#define TEGRA30_CIF_CH1					0
#define TEGRA30_CIF_MONOCONV_COPY			(1<<0)
#define TEGRA30_CIF_STEREOCONV_SHIFT		4
#define TEGRA30_CIF_STEREOCONV_MASK			(3 << TEGRA30_CIF_STEREOCONV_SHIFT)
#define TEGRA30_CIF_STEREOCONV_CH0			(0 << TEGRA30_CIF_STEREOCONV_SHIFT)
#define TEGRA30_CIF_STEREOCONV_CH1			(1 << TEGRA30_CIF_STEREOCONV_SHIFT)
#define TEGRA30_CIF_STEREOCONV_AVG			(2 << TEGRA30_CIF_STEREOCONV_SHIFT)

#ifndef CONFIG_ARCH_TEGRA_3x_SOC
/* TEGRA30_DAM_CH0_BIQUAD_FIXED_COEF_0 */
#define TEGRA30_DAM_CH0_BIQUAD_FIXED_COEF_0_VAL		0x00800000

/* TEGRA30_DAM_FARROW_PARAM_0 */
#define TEGRA30_FARROW_PARAM_RESET	0xdee9a0a0
#define TEGRA30_FARROW_PARAM_1	0
#define TEGRA30_FARROW_PARAM_2	0xdee993a0
#define TEGRA30_FARROW_PARAM_3	0xcccda093
#endif

/*
* Audio Samplerates
*/
#define TEGRA30_AUDIO_SAMPLERATE_8000			8000
#define TEGRA30_AUDIO_SAMPLERATE_16000			16000
#define TEGRA30_AUDIO_SAMPLERATE_44100			44100
#define TEGRA30_AUDIO_SAMPLERATE_48000			48000

#define TEGRA30_DAM_CHIN0_SRC				0
#define TEGRA30_DAM_CHIN1				1
#define TEGRA30_DAM_CHOUT				2
#define TEGRA30_DAM_ENABLE				1
#define TEGRA30_DAM_DISABLE				0

struct tegra30_dam_context {
	struct device *dev;
	int			outsamplerate;
	bool			ch_alloc[TEGRA30_DAM_NUM_INPUT_CHANNELS];
	int			ch_enable_refcnt[TEGRA30_DAM_NUM_INPUT_CHANNELS];
	int			ch_insamplerate[TEGRA30_DAM_NUM_INPUT_CHANNELS];
	struct clk		*dam_clk;
	bool			in_use;
	void __iomem		*damregs;
	struct dentry		*debug;
	struct regmap *regmap;
};

struct tegra30_dam_src_step_table {
	int insample;
	int outsample;
	int stepreset;
};

void tegra30_dam_disable_clock(int ifc);
int tegra30_dam_enable_clock(int ifc);
int tegra30_dam_allocate_controller(void);
int tegra30_dam_allocate_channel(int ifc, int chid);
int tegra30_dam_free_channel(int ifc, int chid);
int tegra30_dam_free_controller(int ifc);
void tegra30_dam_set_samplerate(int ifc, int chtype, int samplerate);
int tegra30_dam_set_gain(int ifc, int chtype, int gain);
int tegra30_dam_set_acif(int ifc, int chtype, unsigned int audio_channels,
	unsigned int audio_bits, unsigned int client_channels,
	unsigned int client_bits);
void tegra30_dam_enable(int ifc, int on, int chtype);
int tegra30_dam_set_acif_stereo_conv(int ifc, int chtype, int conv);
void tegra30_dam_ch0_set_datasync(int ifc, int datasync);
void tegra30_dam_ch1_set_datasync(int ifc, int datasync);
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
void tegra30_dam_enable_stereo_mixing(int ifc);
#endif

#endif
