/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MFD_TABLA_CORE_H__
#define __MFD_TABLA_CORE_H__

#include <linux/interrupt.h>

#define TABLA_NUM_IRQ_REGS 3

#define TABLA_SLIM_NUM_PORT_REG 3

enum {
	TABLA_IRQ_SLIMBUS = 0,
	TABLA_IRQ_MBHC_REMOVAL,
	TABLA_IRQ_MBHC_SHORT_TERM,
	TABLA_IRQ_MBHC_PRESS,
	TABLA_IRQ_MBHC_RELEASE,
	TABLA_IRQ_MBHC_POTENTIAL,
	TABLA_IRQ_MBHC_INSERTION,
	TABLA_IRQ_BG_PRECHARGE,
	TABLA_IRQ_PA1_STARTUP,
	TABLA_IRQ_PA2_STARTUP,
	TABLA_IRQ_PA3_STARTUP,
	TABLA_IRQ_PA4_STARTUP,
	TABLA_IRQ_PA5_STARTUP,
	TABLA_IRQ_MICBIAS1_PRECHARGE,
	TABLA_IRQ_MICBIAS2_PRECHARGE,
	TABLA_IRQ_MICBIAS3_PRECHARGE,
	TABLA_IRQ_HPH_PA_OCPL_FAULT,
	TABLA_IRQ_HPH_PA_OCPR_FAULT,
	TABLA_IRQ_EAR_PA_OCPL_FAULT,
	TABLA_IRQ_HPH_L_PA_STARTUP,
	TABLA_IRQ_HPH_R_PA_STARTUP,
	TABLA_IRQ_EAR_PA_STARTUP,
	TABLA_NUM_IRQS,
};

struct tabla {
	struct device *dev;
	struct slim_device *slim;
	struct slim_device *slim_slave;
	struct mutex io_lock;
	struct mutex xfer_lock;
	struct mutex irq_lock;

	unsigned int irq_base;
	unsigned int irq;
	u8 irq_masks_cur[TABLA_NUM_IRQ_REGS];
	u8 irq_masks_cache[TABLA_NUM_IRQ_REGS];
	u8 irq_level[TABLA_NUM_IRQ_REGS];

	int reset_gpio;

	int (*read_dev)(struct tabla *tabla, unsigned short reg,
			int bytes, void *dest, bool interface_reg);
	int (*write_dev)(struct tabla *tabla, unsigned short reg,
			 int bytes, void *src, bool interface_reg);

	struct regulator_bulk_data *supplies;
};

int tabla_reg_read(struct tabla *tabla, unsigned short reg);
int tabla_reg_write(struct tabla *tabla, unsigned short reg,
		u8 val);
int tabla_interface_reg_read(struct tabla *tabla, unsigned short reg);
int tabla_interface_reg_write(struct tabla *tabla, unsigned short reg,
		u8 val);
int tabla_bulk_read(struct tabla *tabla, unsigned short reg,
			int count, u8 *buf);
int tabla_bulk_write(struct tabla *tabla, unsigned short reg,
			int count, u8 *buf);
int tabla_irq_init(struct tabla *tabla);
void tabla_irq_exit(struct tabla *tabla);
int tabla_get_logical_addresses(u8 *pgd_la, u8 *inf_la);

static inline int tabla_request_irq(struct tabla *tabla, int irq,
				     irq_handler_t handler, const char *name,
				     void *data)
{
	if (!tabla->irq_base)
		return -EINVAL;
	return request_threaded_irq(tabla->irq_base + irq, NULL, handler,
				    IRQF_TRIGGER_RISING, name,
				    data);
}
static inline void tabla_free_irq(struct tabla *tabla, int irq, void *data)
{
	if (!tabla->irq_base)
		return;
	free_irq(tabla->irq_base + irq, data);
}
static inline void tabla_enable_irq(struct tabla *tabla, int irq)
{
	if (!tabla->irq_base)
		return;
	enable_irq(tabla->irq_base + irq);
}
static inline void tabla_disable_irq(struct tabla *tabla, int irq)
{
	if (!tabla->irq_base)
		return;
	disable_irq_nosync(tabla->irq_base + irq);
}

#endif
