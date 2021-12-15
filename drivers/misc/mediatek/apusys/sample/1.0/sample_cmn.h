// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
