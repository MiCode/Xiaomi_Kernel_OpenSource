/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __EDMA_CMD_HND_H__
#define __EDMA_CMD_HND_H__

#include <linux/interrupt.h>
#include "edma_dbgfs.h"
#include "edma_driver.h"


#define EDMA_TAG "[edma]"
#define EDMA_DEBUG
#ifdef EDMA_DEBUG

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
void edma_power_time_up(struct timer_list *power_timer);
void edma_start_power_off(struct work_struct *work);


#endif /* __EDMA_CMD_HND_H__ */
