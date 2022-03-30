/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_CMN_H__
#define __MTK_APU_MDW_CMN_H__

#include <linux/printk.h>
#include <linux/seq_file.h>
#include <linux/time.h>

#include "mdw.h"

extern u32 g_mdw_klog;
extern u8 cfg_apusys_trace;

enum {
	MDW_DBG_DRV = 0x01,
	MDW_DBG_FLW = 0x02,
	MDW_DBG_CMD = 0x04,
	MDW_DBG_MEM = 0x08,
	MDW_DBG_PEF = 0x10,
	MDW_DBG_SUB = 0x20,
};

extern struct mdw_device *mdw_dev;

static inline
int mdw_debug_on(int mask)
{
	return g_mdw_klog & mask;
}

#define mdw_debug(mask, x, ...) do { if (mdw_debug_on(mask)) \
		dev_info(mdw_dev->misc_dev->this_device, \
		"[debug] %s/%d " x, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define mdw_drv_err(x, args...) \
	dev_info(mdw_dev->misc_dev->this_device, \
	"[error] %s " x, __func__, ##args)
#define mdw_drv_warn(x, args...) \
	dev_info(mdw_dev->misc_dev->this_device, \
	"[warn] %s " x, __func__, ##args)
#define mdw_drv_info(x, args...) \
	dev_info(mdw_dev->misc_dev->this_device, \
	"%s " x, __func__, ##args)

#define mdw_drv_debug(x, ...) mdw_debug(MDW_DBG_DRV, x, ##__VA_ARGS__)
#define mdw_flw_debug(x, ...) mdw_debug(MDW_DBG_FLW, x, ##__VA_ARGS__)
#define mdw_cmd_debug(x, ...) mdw_debug(MDW_DBG_CMD, x, ##__VA_ARGS__)
#define mdw_mem_debug(x, ...) mdw_debug(MDW_DBG_MEM, x, ##__VA_ARGS__)
#define mdw_pef_debug(x, ...) mdw_debug(MDW_DBG_PEF, x, ##__VA_ARGS__)
#define mdw_sub_debug(x, ...) mdw_debug(MDW_DBG_SUB, x, ##__VA_ARGS__)

/* print to console via seq file */
#define mdw_con_info(s, x, args...) \
	{\
		if (s) \
			seq_printf(s, x, ##args); \
		else \
			mdw_drv_debug(x, ##args); \
	}

uint32_t mdw_cmn_get_time_diff(struct timespec64 *prev,
	struct timespec64 *next);

#endif
