/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_COM_H_
#define _SPRD_COM_H_

#include "../inc/sprd_defs.h"

#define FULL_MASK 0xFFFFFFFF

#define reg_write_dword(addr, value) writel_relaxed(value, (void __iomem *)addr)
#define reg_read_dword(addr)         readl_relaxed((void __iomem *)addr)


/*page is 4k alignment, left shift 12 bit*/
#define MMU_MAPING_PAGESIZE_SHIFFT   12

/*virt address must be 1M byte alignment, left shift 20 bit*/
#define MMU_MAPING_VIRT_ADDR_SHIFFT   20

/*page size is 4K */
#define MMU_MAPING_PAGESIZE   (1<<MMU_MAPING_PAGESIZE_SHIFFT)
#define MMU_MAPING_PAGE_MASK  (MMU_MAPING_PAGESIZE - 1)

#define VIR_TO_ENTRY_IDX(virt_addr, base_addr) \
	((virt_addr-base_addr)/MMU_MAPING_PAGESIZE)
#define MAP_SIZE_PAGE_ALIGN_UP(length) \
	((length + MMU_MAPING_PAGE_MASK) & (~MMU_MAPING_PAGE_MASK))
#define SIZE_TO_ENTRIES(size) (size/MMU_MAPING_PAGESIZE)


/*-----------------------------------------------------------------------*/
/*                          FUNCTIONS HEADERS                            */
/*-----------------------------------------------------------------------*/

void putbit(ulong reg_addr, u32 dst_value, u8 pos);
void putbits(ulong reg_addr, u32 dst_value, u8 highbitoffset, u8 lowbitoffset);
ulong sg_to_phys(struct scatterlist *sg);


#endif  /*END OF : define  _SPRD_COM_H_ */
