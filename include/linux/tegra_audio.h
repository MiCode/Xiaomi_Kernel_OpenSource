/* include/linux/tegra_audio.h
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *     Iliyan Malchev <malchev@google.com>
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

#ifndef _TEGRA_AUDIO_H
#define _TEGRA_AUDIO_H

#include <linux/ioctl.h>

#define TEGRA_AUDIO_MAGIC 't'

#define TEGRA_AUDIO_IN_START _IO(TEGRA_AUDIO_MAGIC, 0)
#define TEGRA_AUDIO_IN_STOP  _IO(TEGRA_AUDIO_MAGIC, 1)

struct tegra_audio_in_config {
	int rate;
	int stereo;
};

struct dam_srate {
	unsigned int in_sample_rate;
	unsigned int out_sample_rate;
	unsigned int audio_bits;
	unsigned int client_bits;
	unsigned int audio_channels;
	unsigned int client_channels;
	unsigned int apbif_chan;
};

#define TEGRA_AUDIO_IN_SET_CONFIG	_IOW(TEGRA_AUDIO_MAGIC, 2, \
			const struct tegra_audio_in_config *)
#define TEGRA_AUDIO_IN_GET_CONFIG	_IOR(TEGRA_AUDIO_MAGIC, 3, \
			struct tegra_audio_in_config *)

#define TEGRA_AUDIO_IN_SET_NUM_BUFS	_IOW(TEGRA_AUDIO_MAGIC, 4, \
			const unsigned int *)
#define TEGRA_AUDIO_IN_GET_NUM_BUFS	_IOW(TEGRA_AUDIO_MAGIC, 5, \
			unsigned int *)
#define TEGRA_AUDIO_OUT_SET_NUM_BUFS	_IOW(TEGRA_AUDIO_MAGIC, 6, \
			const unsigned int *)
#define TEGRA_AUDIO_OUT_GET_NUM_BUFS	_IOW(TEGRA_AUDIO_MAGIC, 7, \
			unsigned int *)

#define TEGRA_AUDIO_OUT_FLUSH		_IO(TEGRA_AUDIO_MAGIC, 10)

#define TEGRA_AUDIO_BIT_FORMAT_DEFAULT 0
#define TEGRA_AUDIO_BIT_FORMAT_DSP 1
#define TEGRA_AUDIO_SET_BIT_FORMAT	_IOW(TEGRA_AUDIO_MAGIC, 11, \
			const unsigned int *)
#define TEGRA_AUDIO_GET_BIT_FORMAT	_IOR(TEGRA_AUDIO_MAGIC, 12, \
			unsigned int *)

#define DAM_SRC_START	_IOW(TEGRA_AUDIO_MAGIC, 13, struct dam_srate *)
#define DAM_SRC_STOP	_IO(TEGRA_AUDIO_MAGIC, 14)
#define DAM_MIXING_START	_IOW(TEGRA_AUDIO_MAGIC, 15, struct dam_srate *)
#define DAM_MIXING_STOP	_IO(TEGRA_AUDIO_MAGIC, 16)
#define DAM_SET_MIXING_FLAG	_IO(TEGRA_AUDIO_MAGIC, 17)

#define I2S_START	_IOW(TEGRA_AUDIO_MAGIC, 21, struct i2s_pcm_format *)
#define I2S_STOP	_IOW(TEGRA_AUDIO_MAGIC, 22, struct i2s_pcm_format *)
#define I2S_DUAL_CODEC	_IOW(TEGRA_AUDIO_MAGIC, 23, unsigned int *)

#define SPDIF_START	_IO(TEGRA_AUDIO_MAGIC, 27)
#define SPDIF_STOP	_IO(TEGRA_AUDIO_MAGIC, 28)

#define AMX_APBIF_LOOPBACK_START	_IOW(TEGRA_AUDIO_MAGIC, 29, void *)
#define AMX_APBIF_LOOPBACK_STOP		_IOW(TEGRA_AUDIO_MAGIC, 30, void *)
#define AMX_CLOSE			_IOW(TEGRA_AUDIO_MAGIC, 31, void *)
#define AMX_I2S_SETUP			_IOW(TEGRA_AUDIO_MAGIC, 32, void *)
#define AMX_I2S_CLOSE			_IOW(TEGRA_AUDIO_MAGIC, 33, void *)

#define ADX_APBIF_LOOPBACK_START	_IOW(TEGRA_AUDIO_MAGIC, 34, void *)
#define ADX_APBIF_LOOPBACK_STOP		_IOW(TEGRA_AUDIO_MAGIC, 35, void *)
#define ADX_CLOSE			_IOW(TEGRA_AUDIO_MAGIC, 36, void *)
#define ADX_I2S_SETUP			_IOW(TEGRA_AUDIO_MAGIC, 37, void *)
#define ADX_I2S_CLOSE			_IOW(TEGRA_AUDIO_MAGIC, 38, void *)

#define AMX_ADX_APBIF_LOOPBACK_START	_IOW(TEGRA_AUDIO_MAGIC, 39, void *)
#define AMX_ADX_APBIF_LOOPBACK_STOP	_IOW(TEGRA_AUDIO_MAGIC, 40, void *)
#define AMX_ADX_CLOSE			_IOW(TEGRA_AUDIO_MAGIC, 41, void *)
#define AMX_ADX_I2S_SETUP		_IOW(TEGRA_AUDIO_MAGIC, 42, void *)
#define AMX_ADX_I2S_CLOSE		_IOW(TEGRA_AUDIO_MAGIC, 43, void *)


#ifdef CONFIG_SND_SOC_TEGRA
extern bool tegra_is_voice_call_active(void);
#else
static inline bool tegra_is_voice_call_active(void)
{
	return false;
}
#endif

#endif/*_CPCAP_AUDIO_H*/
