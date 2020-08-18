/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __MDP_BASE_H__
#define __MDP_BASE_H__

#define MDP_HW_CHECK

static u32 mdp_base[] = {
	0x1f000000,
	0x1f006000,
	0x1f007000,
	0x1f008000,
	0x1f009000,
	0x1f00a000,
	0x1f00b000,
	0x1f00c000,
	0x1f00d000,
	0x1f00e000,
	0x1f00f000,
	0x1f010000,
	0x1f011000,
	0x1f012000,
	0x1f013000,
	0x1f014000,
	0x1f015000,
	0x1f016000,
	0x1f017000,
	0x1f018000,
	0x1f019000,
	0x1f01a000,
	0x1f01b000,
	0x1f01c000,
	0x1f01d000,
	0x1f01e000,
	0x1f01f000,
	0x1f020000,
	0x1f021000,
	0x1f02c000,
	0x1f02d000,
	0x1f001000,
	0x15010000,
	0x15012000,
	0x15020000,
	0x15021000,
	0x15022000,
	0x15028000,
	0x15820000,
	0x15821000,
	0x15822000,
	0x15828000,
	0x15011000,
	0x15811000,
};

static u32 mdp_sub_base[] = {
	0x0004,
	0x1004,
	0x2004,
	0x8004,
};

#endif
