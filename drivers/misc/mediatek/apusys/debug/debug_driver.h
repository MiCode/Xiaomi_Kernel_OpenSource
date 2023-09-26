/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: JB Tsai <jb.tsai@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DEBUG_DRIVER_H__
#define __DEBUG_DRIVER_H__

#include "apusys_device.h"

#define DEBUG
#define DEBUG_PREFIX "[apusys_dbg]"

#define DBG_LOG_ERR(x, args...) \
	pr_info(DEBUG_PREFIX "[error] %s " x, __func__, ##args)
#define DBG_LOG_WARN(x, args...) \
	pr_info(DEBUG_PREFIX "[warn] %s " x, __func__, ##args)
#define DBG_LOG_INFO(x, args...) \
	pr_info(DEBUG_PREFIX "[info] %s " x, __func__, ##args)


/* print to console via seq file */
#define DBG_LOG_CON(s, x, args...) \
	{\
		if (s) \
			seq_printf(s, x, ##args); \
		else \
			DBG_LOG_INFO(x, ##args); \
	}

#define APUSYS_DEBUG_DIR "apusys_dbg"
#define APU_LOG_SIZE (512*1024)

void apu_dbg_print(const char *fmt, ...);
int apusys_dump_init(struct device *dev);
void apusys_dump_exit(struct device *dev);

#endif /* __DEBUG_DRIVER_H__ */
