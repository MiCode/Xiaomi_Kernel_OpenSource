/*
 * arch/arm/mach-tegra/tegra14_scratch.h
 *
 * Bitmap definitions for pmc scratch register writes
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.
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
 */

#ifndef _MACH_TEGRA_TEGRA14_SCRATCH_H
#define _MACH_TEGRA_TEGRA14_SCRATCH_H

/* Bitmap which tells us which of the PMC_SCRATCH registers need to be
 * written.
 */
static u8 pmc_write_bitmap[] = {
	0xFC, 0xFF, 0xCF, 0x00, 0x00,
	0xF8, 0xDF, 0x28, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0xDF, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xF0, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0x03,
};

#define PMC_REGISTER_OFFSET(index, bit) \
	(((index * sizeof(pmc_write_bitmap[0]) * 8) + bit) * \
	sizeof(u32))

#endif
