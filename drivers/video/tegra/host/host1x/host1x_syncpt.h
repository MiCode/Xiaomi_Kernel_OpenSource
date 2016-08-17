/*
 * drivers/video/tegra/host/host1x/host1x_syncpt.h
 *
 * Tegra Graphics Host Syncpoints for HOST1X
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_HOST1X_HOST1X_SYNCPT_H
#define __NVHOST_HOST1X_HOST1X_SYNCPT_H


/* FIXME:
 * Sync point ids are now split into 2 files.
 * 1 is this one and other is in include/linux/nvhost.h
 * So if someone decides to add new sync point in future
 * please check both the header files
 */
#define NVSYNCPT_DISP0_D		     (5)
#define NVSYNCPT_DISP0_H		     (6)
#define NVSYNCPT_DISP1_H		     (7)
#define NVSYNCPT_CSI_VI_0		     (11)
#define NVSYNCPT_CSI_VI_1		     (12)
#define NVSYNCPT_VI_ISP_0		     (13)
#define NVSYNCPT_VI_ISP_1		     (14)
#define NVSYNCPT_VI_ISP_2		     (15)
#define NVSYNCPT_VI_ISP_3		     (16)
#define NVSYNCPT_VI_ISP_4		     (17)
#define NVSYNCPT_2D_0			     (18)
#define NVSYNCPT_2D_1			     (19)
#define NVSYNCPT_3D			     (22)
#define NVSYNCPT_MPE			     (23)
#define NVSYNCPT_MPE_EBM_EOF		     (28)
#define NVSYNCPT_MPE_WR_SAFE		     (29)

/* sync points that are wholly managed by the client */
#define NVSYNCPTS_CLIENT_MANAGED ( \
	BIT(NVSYNCPT_DISP0_A) | BIT(NVSYNCPT_DISP1_A) | \
	BIT(NVSYNCPT_DISP0_B) | BIT(NVSYNCPT_DISP1_B) | \
	BIT(NVSYNCPT_DISP0_C) | BIT(NVSYNCPT_DISP1_C) | \
	BIT(NVSYNCPT_DSI) | \
	BIT(NVSYNCPT_VBLANK0) | BIT(NVSYNCPT_VBLANK1) | \
	BIT(NVSYNCPT_CSI_VI_0) | \
	BIT(NVSYNCPT_VI_ISP_1) | BIT(NVSYNCPT_VI_ISP_2) | \
	BIT(NVSYNCPT_VI_ISP_3) | BIT(NVSYNCPT_VI_ISP_4) | \
	BIT(NVSYNCPT_MPE_EBM_EOF) | BIT(NVSYNCPT_MPE_WR_SAFE) | \
	BIT(NVSYNCPT_2D_1) | BIT(NVSYNCPT_AVP_0))

#define NVWAITBASE_2D_0 (1)
#define NVWAITBASE_2D_1 (2)
#define NVWAITBASE_3D   (3)
#define NVWAITBASE_MPE  (4)

#endif
