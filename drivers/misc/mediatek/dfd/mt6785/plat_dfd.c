/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

unsigned int check_dfd_support(void)
{
	return 1;
}

unsigned int dfd_infra_base(void)
{
	return 0x390;
}

/* DFD_V30_BASE_ADDR_IN_INFRA
 * bit[9:0] : AP address bit[33:24]
 */
unsigned int dfd_ap_addr_offset(void)
{
	return 24;
}
