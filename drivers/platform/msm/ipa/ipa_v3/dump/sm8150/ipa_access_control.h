/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 and only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#if !defined(_IPA_ACCESS_CONTROL_H_)
#define _IPA_ACCESS_CONTROL_H_

#include "ipa_reg_dump.h"

/*
 * The following is target specific.
 */
static struct reg_mem_access_map_t mem_access_map[] = {
	/*------------------------------------------------------------*/
	/*      Range               Use when              Use when    */
	/*  Begin    End           SD_ENABLED           SD_DISABLED   */
	/*------------------------------------------------------------*/
	{ 0x04000, 0x05000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x07000, 0x0f000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x05000, 0x07000, { &io_matrix[AA_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x0f000, 0x10000, { &io_matrix[NN_COMBO], &io_matrix[NN_COMBO] } },
	{ 0x20000, 0x24000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x24000, 0x28000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x28000, 0x2c000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x10000, 0x11000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x11000, 0x12000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x12000, 0x13000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x43000, 0x44000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x44000, 0x45000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x45000, 0x47000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x40000, 0x42000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x42000, 0x43000, { &io_matrix[AA_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x47000, 0x4a000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x4a000, 0x5a000, { &io_matrix[AN_COMBO], &io_matrix[NN_COMBO] } },
	{ 0x5a000, 0x5c000, { &io_matrix[NN_COMBO], &io_matrix[NN_COMBO] } },
	{ 0x5e000, 0x60000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x60000, 0x70000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x70000, 0x72000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x72000, 0x80000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
};

#endif /* #if !defined(_IPA_ACCESS_CONTROL_H_) */
