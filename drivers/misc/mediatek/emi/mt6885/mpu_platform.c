/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/platform_device.h>

#include "mt_emi.h"

enum {
	MASTER_APM0 = 0,
	MASTER_APM1 = 1,
	MASTER_MM0 = 2,
	MASTER_MDMCU = 3,
	MASTER_MD = 4,
	MASTER_MM1 = 5,
	MASTER_GPU0_PERI = 6,
	MASTER_GPU1_LPDMA = 7,
	MASTER_ALL = 8
};

int is_md_master(unsigned int master_id, unsigned int domain_id)
{
	if ((master_id & 0x7) == MASTER_MDMCU)
		return 1;

	if ((master_id & 0x7) == MASTER_MD)
		return 1;

	return 0;
}

void set_ap_region_permission(unsigned int apc[EMI_MPU_DGROUP_NUM])
{
	SET_ACCESS_PERMISSION(apc, LOCK,
		FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
		SEC_R_NSEC_R, FORBIDDEN, NO_PROTECTION, NO_PROTECTION,
		FORBIDDEN, SEC_R_NSEC_RW, FORBIDDEN, NO_PROTECTION,
		FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION);
}

