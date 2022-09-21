/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
	{ 0x1F000, 0x27000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x05000, 0x0f000, { &io_matrix[AA_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x0f000, 0x10000, { &io_matrix[NN_COMBO], &io_matrix[NN_COMBO] } },
	{ 0x13000, 0x17000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x17000, 0x1b000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x1b000, 0x1f000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x10000, 0x11000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x11000, 0x12000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x12000, 0x13000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x43000, 0x44000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x44000, 0x45000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x45000, 0x47000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x40000, 0x42000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x42000, 0x43000, { &io_matrix[AA_COMBO], &io_matrix[AN_COMBO] } },
	{ 0x50000, 0x60000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0x60000, 0x80000, { &io_matrix[AN_COMBO], &io_matrix[NN_COMBO] } },
	{ 0x80000, 0x81000, { &io_matrix[NN_COMBO], &io_matrix[NN_COMBO] } },
	{ 0x81000, 0x83000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0xa0000, 0xc0000, { &io_matrix[AN_COMBO], &io_matrix[AN_COMBO] } },
	{ 0xc0000, 0xc2000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
	{ 0xc2000, 0xd0000, { &io_matrix[AA_COMBO], &io_matrix[AA_COMBO] } },
};

#endif /* #if !defined(_IPA_ACCESS_CONTROL_H_) */
