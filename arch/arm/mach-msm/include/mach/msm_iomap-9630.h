/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MSM_IOMAP_MDM9630_H
#define __ASM_ARCH_MSM_IOMAP_MDM9630_H

/* Physical base address and size of peripherals.
 * Ordered by the virtual base addresses they will be mapped at.
 *
 * If you add or remove entries here, you'll want to edit the
 * io desc array in arch/arm/mach-msm/io.c to reflect your
 * changes.
 *
 */

#define MDM9630_SHARED_RAM_PHYS		0x01100000

#define MDM9630_TLMM_PHYS			0xFD510000
#define MDM9630_TLMM_SIZE			SZ_16K

#define MDM9630_MPM2_PSHOLD_PHYS		0xFC4AB000
#define MDM9630_MPM2_PSHOLD_SIZE		SZ_4K

#endif
