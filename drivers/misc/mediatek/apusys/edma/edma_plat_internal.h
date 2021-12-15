/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __EDMA_PLAT_INTERNAL_H__
#define __EDMA_PLAT_INTERNAL_H__

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include "edma_driver.h"

struct edma_plat_drv {
	int (*exe_sub)(struct edma_sub *edma_sub, struct edma_request *req);
	void (*prt_error)(struct edma_sub *edma_sub, struct edma_request *req);
	irqreturn_t (*edma_isr)(int irq, void *edma_sub_info);
	unsigned int cmd_timeout_ms;
	unsigned int delay_power_off_ms;
};

void print_error_status(struct edma_sub *edma_sub,
				struct edma_request *req);

int edma_exe_v20(struct edma_sub *edma_sub, struct edma_request *req);

irqreturn_t edma_isr_handler(int irq, void *edma_sub_info);


void printV30_error_status(struct edma_sub *edma_sub,
				struct edma_request *req);

int edma_exe_v30(struct edma_sub *edma_sub, struct edma_request *req);

irqreturn_t edmaV30_isr_handler(int irq, void *edma_sub_info);

const struct of_device_id *edma_plat_get_device(void);


#endif /* __EDMA_PLAT_INTERNAL_H__ */

