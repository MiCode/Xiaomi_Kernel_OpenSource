/*
 * SPRD SoC DP-Codec -- SpreadTrum SOC DP-Codec.
 *
 * Copyright (C) 2021 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SPRD_DP_CODEC_H__
#define __SPRD_DP_CODEC_H__

#include <linux/types.h>
#include <linux/device.h>

struct sprd_dp_codec_params {
	u32 channel;
	u32 rate;
	u32 data_width;
	u32 aif_type;
};

struct sprd_dp_codec_ops {
	int (*audio_startup)(struct device *dev);
	void (*audio_shutdown)(struct device *dev);
	int (*hw_params)(struct device *dev, void *data);
	int (*digital_mute)(struct device *dev, bool enable);
};

struct sprd_dp_codec_pdata {
	const struct sprd_dp_codec_ops *ops;
	uint i2s:1;
	uint spdif:1;
	int max_i2s_channels;
	void *private;
};

#define AIF_DP_I2S      0
#define AIF_DP_SPDIF    1

#define SPRD_DP_CODEC_DRV_NAME "sprd-dp-codec"

#endif //__SPRD_DP_CODEC_H__
