/*
 * Copyright (C) 2016 MediaTek Inc.
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
/*! \file
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/

#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/types.h>
#include "osal.h"

#include "emi_mng.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/


extern unsigned long long gConEmiSize;
extern phys_addr_t gConEmiPhyBase;

struct consys_platform_emi_ops* consys_platform_emi_ops = NULL;

struct consys_emi_addr_info connsys_emi_addr_info = {
	.emi_ap_phy_addr = 0,
	.emi_size = 0,
	.md_emi_phy_addr = 0,
	.md_emi_size = 0,
};

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/


int emi_mng_set_region_protection(void)
{
	if (consys_platform_emi_ops &&
		consys_platform_emi_ops->consys_ic_emi_mpu_set_region_protection)
		return consys_platform_emi_ops->consys_ic_emi_mpu_set_region_protection();
	return -1;
}

int emi_mng_set_remapping_reg(void)
{
	if (consys_platform_emi_ops &&
		consys_platform_emi_ops->consys_ic_emi_set_remapping_reg)
		return consys_platform_emi_ops->consys_ic_emi_set_remapping_reg(
			connsys_emi_addr_info.emi_ap_phy_addr,
			connsys_emi_addr_info.md_emi_phy_addr);
	return -1;
}

struct consys_emi_addr_info* emi_mng_get_phy_addr(void)
{
	return &connsys_emi_addr_info;
}


struct consys_platform_emi_ops* __weak get_consys_platform_emi_ops(void)
{
	pr_warn("No specify project\n");
	return NULL;
}

int emi_mng_init(void)
{
	if (consys_platform_emi_ops == NULL)
		consys_platform_emi_ops = get_consys_platform_emi_ops();

	pr_info("[emi_mng_init] gConEmiPhyBase = [0x%llx] size = [%llx] ops=[%p]",
			gConEmiPhyBase, gConEmiSize, consys_platform_emi_ops);

	if (gConEmiPhyBase) {
		connsys_emi_addr_info.emi_ap_phy_addr = gConEmiPhyBase;
		connsys_emi_addr_info.emi_size = gConEmiSize;
	} else {
		pr_err("consys emi memory address gConEmiPhyBase invalid\n");
	}

	if (consys_platform_emi_ops &&
		consys_platform_emi_ops->consys_ic_emi_get_md_shared_emi)
		consys_platform_emi_ops->consys_ic_emi_get_md_shared_emi(
			&connsys_emi_addr_info.md_emi_phy_addr,
			&connsys_emi_addr_info.md_emi_size);

	if (consys_platform_emi_ops &&
		consys_platform_emi_ops->consys_ic_emi_mpu_set_region_protection)
		consys_platform_emi_ops->consys_ic_emi_mpu_set_region_protection();

	return 0;
}

int emi_mng_deinit(void)
{
	return 0;
}
