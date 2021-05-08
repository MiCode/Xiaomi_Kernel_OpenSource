/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __MTK_SPM_RESOURCE_CONSOLE_H__
#define __MTK_SPM_RESOURCE_CONSOLE_H__

#include <mtk_spm_resource_req_internal.h>

enum {
	SPM_RESOURCE_CONSOLE_REQ,
	SPM_RESOURCE_CONSOLE_RELEASE,
};

enum {
	MTK_SPM_RES_EX_DRAM_S1 = 0,
	MTK_SPM_RES_EX_DRAM_S0,
	MTK_SPM_RES_EX_MAINPLL,
	MTK_SPM_RES_EX_AXI_BUS,
	MTK_SPM_RES_EX_26M,
	MTK_SPM_RES_EX_MAX,
};
#define _RES_MASK(x)	(1<<x)

/* Compulsory method for spm resource requirement.
 * This function's implementation depend on platform
 * File: mtk_spm_reource_req_console.c
 */
int spm_resource_req_console(unsigned int req, unsigned int res_bitmask);
int spm_resource_req_console_by_id(
		int id, unsigned int req, unsigned int res_bitmask);

#endif
