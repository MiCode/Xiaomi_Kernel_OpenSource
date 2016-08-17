/* include/linux/tegra_mediaserver.h
 *
 * Media Server driver for NVIDIA Tegra SoCs
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef _TEGRA_MEDIASERVER_H
#define _TEGRA_MEDIASERVER_H

#include <linux/ioctl.h>

#define TEGRA_MEDIASERVER_MAGIC 'm'
#define TEGRA_MEDIASERVER_IOCTL_ALLOC \
	_IOWR(TEGRA_MEDIASERVER_MAGIC, 0x40, \
	      union tegra_mediaserver_alloc_info)

enum tegra_mediaserver_resource_type {
	TEGRA_MEDIASERVER_RESOURCE_BLOCK = 0,
	TEGRA_MEDIASERVER_RESOURCE_IRAM,
};

enum tegra_mediaserver_block_type {
	TEGRA_MEDIASERVER_BLOCK_AUDDEC = 0,
	TEGRA_MEDIASERVER_BLOCK_VIDDEC,
};

enum tegra_mediaserver_iram_type {
	TEGRA_MEDIASERVER_IRAM_SCRATCH = 0,
	TEGRA_MEDIASERVER_IRAM_SHARED,
};


struct tegra_mediaserver_block_info {
	int nvmm_block_handle;
	int avp_block_handle;
	int avp_block_library_handle;
	int service_handle;
	int service_library_handle;
};

struct tegra_mediaserver_iram_info {
	unsigned long rm_handle;
	int physical_address;
};

union tegra_mediaserver_alloc_info {
	struct {
		int tegra_mediaserver_resource_type;

		union {
			struct tegra_mediaserver_block_info block;

			struct {
				int tegra_mediaserver_iram_type;
				int alignment;
				size_t size;
			} iram;
		} u;
	} in;

	struct {
		union {
			struct {
				int count;
			} block;

			struct tegra_mediaserver_iram_info iram;
		} u;
	} out;
};


#define TEGRA_MEDIASERVER_IOCTL_FREE \
	_IOR(TEGRA_MEDIASERVER_MAGIC, 0x41, union tegra_mediaserver_free_info)

union tegra_mediaserver_free_info {
	struct {
		int tegra_mediaserver_resource_type;

		union {
			int nvmm_block_handle;
			int iram_rm_handle;
		} u;
	} in;
};


#define TEGRA_MEDIASERVER_IOCTL_UPDATE_BLOCK_INFO	\
	_IOR(TEGRA_MEDIASERVER_MAGIC, 0x45, \
	     union tegra_mediaserver_update_block_info)

union tegra_mediaserver_update_block_info {
	struct tegra_mediaserver_block_info in;
};
#endif

