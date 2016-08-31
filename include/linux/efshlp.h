/*
 * Copyright (C) 2008-2013, NVIDIA Corporation.  All rights reserved.
 *
 * Author:
 * Ashutosh Patel <ashutoshp@nvidia.com>
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
 *
 * include/linux/efshlp.h  - EFS filesystem helper Header file
 *
 */

#ifndef _NVEFSHLP_H
#define _NVEFSHLP_H

#include <linux/ioctl.h>

#define MAX_BANKS 20
#define KB 1024UL
#define MB (KB*1024)

#define EFS_IOC_MAGIC 0x82

#define EFSHLP_INIT             _IO(EFS_IOC_MAGIC, 1)
#define EFSHLP_DO_DMA           _IO(EFS_IOC_MAGIC, 2)
#define EFSHLP_ALLOCBUF         _IO(EFS_IOC_MAGIC, 3)
#define EFSHLP_BANKSWITCH       _IO(EFS_IOC_MAGIC, 4)
#define EFSHLP_GET_ALLOC_SIZE   _IO(EFS_IOC_MAGIC, 5)

#define FLASHDMA_DMABUFSIZE (4*KB)
#define FLASHDMA_TIMEOUT (60*HZ)

#define FLASH_DMA_NUM_PRIO	2
#define FLASH_DMA_PRIO_NORMAL	0
#define FLASH_DMA_PRIO_HIGH	1
#define FLASH_DMA_PRIO_DEFAULT	FLASH_DMA_PRIO_NORMAL

struct efs_flash_info {
	unsigned long long total_flash_size;
	unsigned long long boot_part_ofs;
	unsigned long boot_part_size;
	unsigned long long kern_part_ofs;
	unsigned long kern_part_size;
	unsigned long user_part_ofs;
	unsigned long long user_part_size;
	unsigned long nbanks;
	unsigned long bank_addr[MAX_BANKS];
	unsigned long bank_size[MAX_BANKS];
	unsigned long bank_switch_id[MAX_BANKS];
	unsigned long erase_size;
};

struct efshlp_dma {
	unsigned long bank;
	unsigned long size;
	unsigned long offset;
	void *buf;
	unsigned long prio;
};

struct efs_dma_handle {
	dma_addr_t phyaddr;
	void *virtaddr;
	size_t size;
	int mapcount;
	short signature;
};

#endif
