/*
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA_DTV_H__
#define __TEGRA_DTV_H__

#include <linux/ioctl.h>

#define TEGRA_DTV_MAGIC 'v'

#define TEGRA_DTV_IOCTL_START           _IO(TEGRA_DTV_MAGIC, 0)
#define TEGRA_DTV_IOCTL_STOP            _IO(TEGRA_DTV_MAGIC, 1)

struct tegra_dtv_hw_config {
	int clk_edge; /*< clock edge to be used to sample DTV input signals */
	int byte_swz_enabled; /*< byte order during deserialization */
	int bit_swz_enabled;  /*< bit order during deserialization */

	int protocol_sel;   /*< VD pin configuration. */
	int clk_mode;       /*< input clock characteristics */
	int fec_size;       /*< FEC size */
	int body_size;      /*< BODY size */
	int body_valid_sel; /*< VALID signal gate */
	int start_sel;      /*< START of the package */
	int err_pol;        /*< ERROR pin polarity */
	int psync_pol;      /*< PSYNC pin polarity */
	int valid_pol;      /*< VALID pin polarity */
};

struct tegra_dtv_profile {
	unsigned int bufsize;     /*< SIZE of buffer */
	unsigned int bufnum;      /*< COUNT of buffer */
	unsigned int cpuboost;    /*< FREQUENC to boost up CPU */
	unsigned int bitrate;     /*< BITRATE of stream */
};

struct tegra_dtv_platform_data {
	int dma_req_selector;
};

#define TEGRA_DTV_IOCTL_SET_HW_CONFIG  _IOW(TEGRA_DTV_MAGIC, 2,		\
					   const struct tegra_dtv_hw_config *)
#define TEGRA_DTV_IOCTL_GET_HW_CONFIG  _IOR(TEGRA_DTV_MAGIC, 3,		\
					   struct tegra_dtv_hw_config *)
#define TEGRA_DTV_IOCTL_GET_PROFILE _IOR(TEGRA_DTV_MAGIC, 4,		\
					 struct tegra_dtv_profile *)
#define TEGRA_DTV_IOCTL_SET_PROFILE _IOW(TEGRA_DTV_MAGIC, 5,		\
					  const struct tegra_dtv_profile *)

/**
 * clock edge settings for clk_edge
 *
 * RISE_EDGE: sample input signal at rising edge
 * FALL_EDGE: sample input signal at falling edge
 */
enum {
	TEGRA_DTV_CLK_RISE_EDGE = 0,
	TEGRA_DTV_CLK_FALL_EDGE,
};

/**
 * swizzle settings for byte_swz and bit_swz
 *
 * ENABLE: enable swizzle during deserialization
 * DISABLE: disable swizzle during deserialization
 *
 * If swizzling is enabled then deserialized data will be re-ordered to
 * fit the required format for tegra.
 *
 * For example, if raw BGR data is inputed into DTV interface, the data
 * could be swizzled into RGB.
 *
 * For TS/MPEG-2 stream, please disable this feature.
 */
enum {
	TEGRA_DTV_SWZ_DISABLE = 0,
	TEGRA_DTV_SWZ_ENABLE,
};

/* for selecting the pin configuration for VD(valid).
 * NONE : ERROR is tied to 0, PSYNC is tied to 0
 * ERROR: ERROR is tied to VD, PSYNC is tied to 0
 * PSYNC: ERROR is tied to 0, PSYNC is tied to VD
 */
enum {
	TEGRA_DTV_PROTOCOL_NONE = 0,
	TEGRA_DTV_PROTOCOL_ERROR,
	TEGRA_DTV_PROTOCOL_PSYNC,
};

enum {
	TEGRA_DTV_CLK_DISCONTINUOUS = 0,
	TEGRA_DTV_CLK_CONTINUOUS,
};

enum {
	TEGRA_DTV_BODY_VALID_IGNORE = 0,
	TEGRA_DTV_BODY_VALID_GATE,
};

enum {
	TEGRA_DTV_START_RESERVED = 0, /* never use this */
	TEGRA_DTV_START_PSYNC,
	TEGRA_DTV_START_VALID,
	TEGRA_DTV_START_BOTH,
};

enum {
	TEGRA_DTV_ERROR_POLARITY_HIGH = 0,
	TEGRA_DTV_ERROR_POLARITY_LOW,
};

enum {
	TEGRA_DTV_PSYNC_POLARITY_HIGH = 0,
	TEGRA_DTV_PSYNC_POLARITY_LOW,
};

enum {
	TEGRA_DTV_VALID_POLARITY_HIGH = 0,
	TEGRA_DTV_VALID_POLARITY_LOW,
};

#endif /* __TEGRA_DTV_H__ */
