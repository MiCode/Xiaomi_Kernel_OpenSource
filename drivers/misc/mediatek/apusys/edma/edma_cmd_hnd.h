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

#ifndef __EDMA_CMD_HND_H__
#define __EDMA_CMD_HND_H__

#include <linux/interrupt.h>
#include "edma_ioctl.h"
#include "edma_dbgfs.h"


#if 0
#define EDMA_PREFIX "[edma]"

extern u8 g_edma_log_lv;

#define LOG_ERR(x, args...) \
	pr_info(EDMA_PREFIX "[error] %s " x, __func__, ##args)
#define LOG_WARN(x, args...) \
	pr_info(EDMA_PREFIX "[warn] %s " x, __func__, ##args)
#define LOG_INF(x, args...) \
	pr_info(EDMA_PREFIX "%s " x, __func__, ##args)
#define LOG_DBG(x, args...) \
	{ \
		if (g_edma_log_lv >= EDMA_LOG_DEBUG) \
			pr_info(EDMA_PREFIX "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}
#endif
#define EDMA_TAG "[edma]"
#define EDMA_DEBUG
#ifdef EDMA_DEBUG
//#define LOG_DBG(format, args...)    pr_debug(EDMA_TAG " " format, ##args)

#define LOG_DBG(x, args...) \
	{ \
		if (g_edma_log_lv >= EDMA_LOG_DEBUG) \
			pr_info(EDMA_TAG "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

#else
#define LOG_DBG(format, args...)
#endif
#define LOG_INF(format, args...)    pr_info(EDMA_TAG " " format, ##args)
#define LOG_WRN(format, args...)    pr_info(EDMA_TAG "[warn] " format, ##args)
#define LOG_ERR(format, args...)    pr_info(EDMA_TAG "[error] " format, ##args)

irqreturn_t edma_isr_handler(int irq, void *edma_sub_info);

void edma_enable_sequence(struct edma_sub *edma_sub);
int edma_trigger_internal_mode(void __iomem *base_addr,
					struct edma_request *req);

int edma_normal_mode(struct edma_sub *edma_sub, struct edma_request *req);
int edma_fill_mode(struct edma_sub *edma_sub, struct edma_request *req);
int edma_numerical_mode(struct edma_sub *edma_sub, struct edma_request *req);
int edma_format_mode(struct edma_sub *edma_sub, struct edma_request *req);
int edma_compress_mode(struct edma_sub *edma_sub, struct edma_request *req);
int edma_decompress_mode(struct edma_sub *edma_sub, struct edma_request *req);
int edma_raw_mode(struct edma_sub *edma_sub, struct edma_request *req);
int edma_ext_mode(struct edma_sub *edma_sub, struct edma_request *req);
int edma_power_on(struct edma_sub *edma_sub);
int edma_power_off(struct edma_sub *edma_sub, u8 force);
#ifdef DEBUG
int edma_sync_normal_mode(struct edma_device *edma_device,
						struct edma_request *req);
int edma_sync_ext_mode(struct edma_device *edma_device,
						struct edma_request *req);
#endif
int edma_execute(struct edma_sub *edma_sub, struct edma_ext *edma_ext);
void edma_power_time_up(unsigned long data);
void edma_start_power_off(struct work_struct *work);
void edma_sw_reset(struct edma_sub *edma_sub);


#endif /* __EDMA_CMD_HND_H__ */
