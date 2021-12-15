// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
