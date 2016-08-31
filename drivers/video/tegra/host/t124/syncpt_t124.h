/*
 * drivers/video/tegra/host/t124/syncpt_t124.h
 *
 * Tegra Graphics Host Syncpoints for T124
 *
 * Copyright (c) 2011, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __NVHOST_SYNCPT_T124_H
#define __NVHOST_SYNCPT_T124_H

#include <linux/nvhost.h>

#define NVSYNCPT_GK20A_BASE 64
/* following is base + number of gk20a channels. TODO: remove magic */
#define NVSYNCPT_GK20A_LAST (NVSYNCPT_GK20A_BASE + 127)

#define NV_VI_0_SYNCPTS { \
	NVSYNCPT_VI_0_0, \
	NVSYNCPT_VI_0_1, \
	NVSYNCPT_VI_0_2, \
	NVSYNCPT_VI_0_3, \
	NVSYNCPT_VI_0_4}

#define NV_VI_1_SYNCPTS { \
	NVSYNCPT_VI_1_0, \
	NVSYNCPT_VI_1_1, \
	NVSYNCPT_VI_1_2, \
	NVSYNCPT_VI_1_3, \
	NVSYNCPT_VI_1_4}

#define NV_ISP_0_SYNCPTS {\
	NVSYNCPT_ISP_0_0, \
	NVSYNCPT_ISP_0_1, \
	NVSYNCPT_ISP_0_2, \
	NVSYNCPT_ISP_0_3}

#define NV_ISP_1_SYNCPTS {\
	NVSYNCPT_ISP_1_0, \
	NVSYNCPT_ISP_1_1, \
	NVSYNCPT_ISP_1_2, \
	NVSYNCPT_ISP_1_3}

#define NVWAITBASE_3D   (3)
#define NVWAITBASE_MSENC  (4)
#define NVWAITBASE_TSEC   (5)

#endif /* __NVHOST_SYNCPT_T124_H */
