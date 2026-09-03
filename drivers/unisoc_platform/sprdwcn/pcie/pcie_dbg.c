// SPDX-License-Identifier: GPL-2.0-only
/*
 * pcie_dbg.c - Unisoc platform driver
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include "pcie_dbg.h"

int pcie_hexdump(char *name, char *buf, int len)
{
	int i, count;
	unsigned int *p;

	count = len / 32;
	count += 1;
	WCN_INFO("hexdump %s hex(len=%d):\n", name, len);
	for (i = 0; i < count; i++) {
		p = (unsigned int *)(buf + i * 32);
		WCN_INFO(
			 "mem[0x%04x] 0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x\n",
			 i * 32, p[0], p[1], p[2], p[3], p[4], p[5],
			 p[6], p[7]);
	}

	return 0;
}
