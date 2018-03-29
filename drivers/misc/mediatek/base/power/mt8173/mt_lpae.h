/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT_LPAE_H__
#define __MT_LPAE_H__

#ifdef CONFIG_ARM_LPAE

#define INTERAL_MAPPING_OFFSET (0x40000000)
#define INTERAL_MAPPING_LIMIT (INTERAL_MAPPING_OFFSET + 0x80000000)

#define INFRA_LARGE_MEMORY_SETTING (CKSYS_BASE + 0x1F00)
#define IS_MT_LARGE_MEMORY_MODE  (readl(IOMEM(INFRA_LARGE_MEMORY_SETTING)) >> 13 & 0x1)
#define MT_OVERFLOW_ADDR_START 0x100000000ULL

/* For HW modules which support 33-bit address setting */
#define CROSS_OVERFLOW_ADDR_TRANSFER(phy_addr, size, ret) \
	do { \
		if (((phys_addr_t)phy_addr < MT_OVERFLOW_ADDR_START) && (((phys_addr_t)phy_addr + size) \
				>= MT_OVERFLOW_ADDR_START)) \
			ret = MT_OVERFLOW_ADDR_START - phy_addr; \
		else \
			ret = 0; \
	} while (0)

/* For SPM and MD32 only */
#define MAPPING_DRAM_ACCESS_ADDR(phy_addr) \
	do { \
		if (phy_addr >= INTERAL_MAPPING_OFFSET && phy_addr < INTERAL_MAPPING_LIMIT) \
			phy_addr += INTERAL_MAPPING_OFFSET; \
	} while (0)

#else /* !CONFIG_ARM_LPAE */

#define IS_MT_LARGE_MEMORY_MODE 0
#define CROSS_OVERFLOW_ADDR_TRANSFER(phy_addr, size, ret)
#define MAPPING_DRAM_ACCESS_ADDR(phy_addr)
#define MT_OVERFLOW_ADDR_START 0

#endif
#endif  /*!__MT_LPAE_H__ */
