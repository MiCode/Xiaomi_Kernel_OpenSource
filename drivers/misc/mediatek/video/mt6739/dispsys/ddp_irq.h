/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _DDP_IRQ_H_
#define _DDP_IRQ_H_

#include "ddp_info.h"
#include <linux/interrupt.h>

typedef void (*DDP_IRQ_CALLBACK)(enum DISP_MODULE_ENUM module,
				 unsigned int reg_value);

int disp_register_module_irq_callback(enum DISP_MODULE_ENUM module,
				      DDP_IRQ_CALLBACK cb);
int disp_unregister_module_irq_callback(enum DISP_MODULE_ENUM module,
					DDP_IRQ_CALLBACK cb);

int disp_register_irq_callback(DDP_IRQ_CALLBACK cb);
int disp_unregister_irq_callback(DDP_IRQ_CALLBACK cb);

void disp_register_irq(unsigned int irq_num, char *device_name);
int disp_init_irq(void);
irqreturn_t disp_irq_handler(int irq, void *dev_id);

extern atomic_t ESDCheck_byCPU;

int disp_irq_esd_cust_get(void);
void disp_irq_esd_cust_bycmdq(int enable);
#endif /* _DDP_IRQ_H_ */
