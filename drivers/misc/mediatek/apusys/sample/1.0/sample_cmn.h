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

#define APUSYS_LOG_PREFIX "[apusys_sample]"

#define LOG_ERR(x, args...) pr_info(APUSYS_LOG_PREFIX\
	"[error] %s " x, __func__, ##args)
#define LOG_WARN(x, args...) pr_info(APUSYS_LOG_PREFIX\
	"[warn] " x, ##args)
#define LOG_INFO(x, args...) pr_info(APUSYS_LOG_PREFIX\
	x, ##args)
#if 0
#define LOG_DEBUG(x, args...) pr_debug(APUSYS_LOG_PREFIX\
	"[debug] %s " x, __func__, ##args)
#else
#define LOG_DEBUG(x, args...)
#endif

#define DEBUG_TAG LOG_DEBUG("[%d]\n", __LINE__)

#endif
