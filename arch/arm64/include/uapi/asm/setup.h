/*
 * Based on arch/arm/include/asm/setup.h
 *
 * Copyright (C) 1997-1999 Russell King
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_SETUP_H
#define __ASM_SETUP_H

#include <linux/types.h>

#define COMMAND_LINE_SIZE	2048
#define MBLOCK_RESERVED_NAME_SIZE 128
#define MBLOCK_RESERVED_NUM_MAX  128
#define MBLOCK_NUM_MAX 128
#define MBLOCK_MAGIC 0x99999999
#define MBLOCK_VERSION 0x2

/* general memory descriptor */
struct mem_desc {
	u64 start;
	u64 size;
};

/* mblock is used by CPU */
struct  mblock {
	u64 start;
	u64 size;
	u32 rank;	/* rank the mblock belongs to */
};

struct mblock_reserved {
	u64 start;
	u64 size;
	u32 mapping;   /* mapping or unmap*/
	char name[MBLOCK_RESERVED_NAME_SIZE];
};

struct mblock_info {
	u32 mblock_num;
	struct mblock mblock[MBLOCK_NUM_MAX];
	u32 mblock_magic;
	u32 mblock_version;
	u32 reserved_num;
	struct mblock_reserved reserved[MBLOCK_RESERVED_NUM_MAX];
};

struct dram_info {
	u32 rank_num;
	struct mem_desc rank_info[4];
};


#endif
