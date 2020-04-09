/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __MTK_SWPM_INTERFACE_H__
#define __MTK_SWPM_INTERFACE_H__

#include <linux/types.h>

enum swpm_return_type {
	SWPM_SUCCESS = 0,
	SWPM_INIT_ERR = 1,
	SWPM_PLAT_ERR = 2,
	SWPM_ARGS_ERR = 3,
};

enum swpm_type {
	CPU_SWPM_TYPE,
	GPU_SWPM_TYPE,
	CORE_SWPM_TYPE,
	MEM_SWPM_TYPE,
	ISP_SWPM_TYPE,

	NR_SWPM_TYPE,
};

/* swpm interface to request share memory address by SWPM TYPE */
/* return:      0  (SWPM_SUCCESS)
 *              otherwise (ERROR)
 */
extern int swpm_mem_addr_request(enum swpm_type id,
				 phys_addr_t **ptr);

#endif /* __MTK_SWPM_INTERFACE_H__ */

