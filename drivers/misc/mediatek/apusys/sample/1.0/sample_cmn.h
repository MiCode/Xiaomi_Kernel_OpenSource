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

#ifndef __APUSYS_SAMPLE_COMMON_H__
#define __APUSYS_SAMPLE_COMMON_H__

#include <linux/printk.h>

#define APUSYS_SPL_PREFIX "[apusys_sample]"
#define spl_drv_err(x, args...) \
	pr_info(APUSYS_SPL_PREFIX "[error] %s " x, __func__, ##args)
#define spl_drv_warn(x, args...) \
	pr_info(APUSYS_SPL_PREFIX "[warn] %s " x, __func__, ##args)
#define spl_drv_info(x, args...) \
	pr_info(APUSYS_SPL_PREFIX "%s " x, __func__, ##args)
#if 0
#define spl_drv_dbg(x, args...) \
	pr_info(APUSYS_SPL_PREFIX "%s " x, __func__, ##args)
#else
#define spl_drv_dbg(x, args...)
#endif

#endif
