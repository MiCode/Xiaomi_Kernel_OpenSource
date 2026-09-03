// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include "sprd_com.h"

void putbit(ulong reg_addr, u32 dst_value, u8 pos)
{
	u32 org_value = 0;
	u32 new_value = 0;
	u32 mask = 1U << pos;

	org_value = reg_read_dword(reg_addr);
	new_value = (org_value & ~mask) | ((dst_value << pos) & mask);

	reg_write_dword(reg_addr, new_value);
}

void putbits(ulong reg_addr, u32 dst_value, u8 highbitoffset, u8 lowbitoffset)
{
	u32 org_value = 0;
	u32 new_value = 0;
	u32 mask = (FULL_MASK >> (32 - (highbitoffset - lowbitoffset + 1)))
			<< lowbitoffset;

	org_value = reg_read_dword(reg_addr);
	new_value = (org_value & ~mask) | ((dst_value << lowbitoffset) & mask);

	reg_write_dword(reg_addr, new_value);
}


ulong sg_to_phys(struct scatterlist *sg)
{
	/*
	 * Try sg_dma_address first so that we can
	 * map carveout regions that do not have a
	 * struct page associated with them.
	 */
	ulong pa = sg_dma_address(sg);
	if (pa == 0)
		pa = sg_phys(sg);
	return pa;
}
