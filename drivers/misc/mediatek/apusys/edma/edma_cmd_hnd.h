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
void edma_power_on(struct edma_sub *edma_sub);
void edma_power_off(struct edma_sub *edma_sub);
#ifdef DEBUG
int edma_sync_normal_mode(struct edma_device *edma_device,
						struct edma_request *req);
int edma_sync_ext_mode(struct edma_device *edma_device,
						struct edma_request *req);
#endif
int edma_execute(struct edma_sub *edma_sub, struct edma_ext *edma_ext);
void edma_power_time_up(unsigned long data);

#endif /* __EDMA_CMD_HND_H__ */
