/*
 * include/linux/platform_data/nvshm.h
 *
 * Copyright (C) 2012-2013 NVIDIA Corporation.
 *
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

#ifndef __PLATFORM_DATA_NVSHM_H
#define __PLATFORM_DATA_NVSHM_H

#include <linux/types.h>

/* NVSHM serial number size in bytes */
#define NVSHM_SERIAL_BYTE_SIZE 20

struct nvshm_platform_data {
	void *ipc_base_virt;
	size_t ipc_size;
	void *mb_base_virt;
	size_t mb_size;
	int bb_irq;
	struct platform_device *tegra_bb;
};

#endif
