/* Copyright (c) 2014, 2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __VENUS_BOOT_H__
#define __VENUS_BOOT_H__
#include "msm_vidc_resources.h"

int venus_boot_init(struct msm_vidc_platform_resources *res,
		struct context_bank_info *cb);
void venus_boot_deinit(void);

#endif /* __VENUS_BOOT_H__ */
