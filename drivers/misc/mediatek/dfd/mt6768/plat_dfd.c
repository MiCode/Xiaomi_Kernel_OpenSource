/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
