/*
 * drivers/video/tegra/host/host1x/host1x_cdma.h
 *
 * Tegra Graphics Host Command DMA
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __NVHOST_HOST1X_HOST1X_CDMA_H
#define __NVHOST_HOST1X_HOST1X_CDMA_H

/* Number of gathers we allow to be queued up per channel. Must be a
 * power of two. Currently sized such that pushbuffer is 4KB (512*8B). */
#define NVHOST_GATHER_QUEUE_SIZE 512

/* 8 bytes per slot. (This number does not include the final RESTART.) */
#define PUSH_BUFFER_SIZE (NVHOST_GATHER_QUEUE_SIZE * 8)

/* 4K page containing GATHERed methods to increment channel syncpts
 * and replaces the original timed out contexts GATHER slots */
#define SYNCPT_INCR_BUFFER_SIZE_WORDS   (4096 / sizeof(u32))

#endif
