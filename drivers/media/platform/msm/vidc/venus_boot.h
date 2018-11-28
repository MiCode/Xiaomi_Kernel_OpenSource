/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 */

#ifndef __VENUS_BOOT_H__
#define __VENUS_BOOT_H__
#include "msm_vidc_resources.h"

int venus_boot_init(struct msm_vidc_platform_resources *res,
		struct context_bank_info *cb);
void venus_boot_deinit(void);

#endif /* __VENUS_BOOT_H__ */
