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
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/mfd/wcd9310/core.h>
#include <linux/mfd/wcd9310/registers.h>
#include <linux/interrupt.h>

#define BYTE_BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_BYTE))
#define BIT_BYTE(nr)			((nr) / BITS_PER_BYTE)

struct tabla_irq {
	bool level;
};

static struct tabla_irq tabla_irqs[TABLA_NUM_IRQS] = {
	[0] = { .level = 1},
/* All other tabla interrupts are edge triggered */
};

static inline int irq_to_tabla_irq(struct tabla *tabla, int irq)
{
	return irq - tabla->irq_base;
}

static void tabla_irq_lock(struct irq_data *data)
{
	struct tabla *tabla = irq_data_get_irq_chip_data(data);
	mutex_lock(&tabla->irq_lock);
}

static void tabla_irq_sync_unlock(struct irq_data *data)
{
	struct tabla *tabla = irq_data_get_irq_chip_data(data);
	int i;

	for (i = 0; i < ARRAY_SIZE(tabla->irq_masks_cur); i++) {
		/* If there's been a change in the mask write it back
		 * to the hardware.
		 */
		if (tabla->irq_masks_cur[i] != tabla->irq_masks_cache[i]) {
			tabla->irq_masks_cache[i] = tabla->irq_masks_cur[i];
			tabla_reg_write(tabla, TABLA_A_INTR_MASK0+i,
				tabla->irq_masks_cur[i]);
		}
	}

	mutex_unlock(&tabla->irq_lock);
}

static void tabla_irq_enable(struct irq_data *data)
{
	struct tabla *tabla = irq_data_get_irq_chip_data(data);
	int tabla_irq = irq_to_tabla_irq(tabla, data->irq);
	tabla->irq_masks_cur[BIT_BYTE(tabla_irq)] &=
		~(BYTE_BIT_MASK(tabla_irq));
}

static void tabla_irq_disable(struct irq_data *data)
{
	struct tabla *tabla = irq_data_get_irq_chip_data(data);
	int tabla_irq = irq_to_tabla_irq(tabla, data->irq);
	tabla->irq_masks_cur[BIT_BYTE(tabla_irq)] |= BYTE_BIT_MASK(tabla_irq);
}

static struct irq_chip tabla_irq_chip = {
	.name = "tabla",
	.irq_bus_lock = tabla_irq_lock,
	.irq_bus_sync_unlock = tabla_irq_sync_unlock,
	.irq_disable = tabla_irq_disable,
	.irq_enable = tabla_irq_enable,
};

static irqreturn_t tabla_irq_thread(int irq, void *data)
{
	int ret;
	struct tabla *tabla = data;
	u8 status[TABLA_NUM_IRQ_REGS];
	unsigned int i;

	ret = tabla_bulk_read(tabla, TABLA_A_INTR_STATUS0,
			       TABLA_NUM_IRQ_REGS, status);
	if (ret < 0) {
		dev_err(tabla->dev, "Failed to read interrupt status: %d\n",
			ret);
		return IRQ_NONE;
	}
	/* Apply masking */
	for (i = 0; i < TABLA_NUM_IRQ_REGS; i++)
		status[i] &= ~tabla->irq_masks_cur[i];

	/* Find out which interrupt was triggered and call that interrupt's
	 * handler function
	 */
	for (i = 0; i < TABLA_NUM_IRQS; i++) {
		if (status[BIT_BYTE(i)] & BYTE_BIT_MASK(i)) {
			if ((i <= TABLA_IRQ_MBHC_INSERTION) &&
				(i >= TABLA_IRQ_MBHC_REMOVAL)) {
				tabla_reg_write(tabla, TABLA_A_INTR_CLEAR0 +
					BIT_BYTE(i), BYTE_BIT_MASK(i));
				if (tabla_get_intf_type() ==
					TABLA_INTERFACE_TYPE_I2C)
					tabla_reg_write(tabla,
						TABLA_A_INTR_MODE, 0x02);
				handle_nested_irq(tabla->irq_base + i);
			} else {
				handle_nested_irq(tabla->irq_base + i);
				tabla_reg_write(tabla, TABLA_A_INTR_CLEAR0 +
					BIT_BYTE(i), BYTE_BIT_MASK(i));
				if (tabla_get_intf_type() ==
					TABLA_INTERFACE_TYPE_I2C)
					tabla_reg_write(tabla,
						TABLA_A_INTR_MODE, 0x02);
			}
			break;
		}
	}

	return IRQ_HANDLED;
}

int tabla_irq_init(struct tabla *tabla)
{
	int ret;
	unsigned int i, cur_irq;

	mutex_init(&tabla->irq_lock);

	if (!tabla->irq) {
		dev_warn(tabla->dev,
			 "No interrupt specified, no interrupts\n");
		tabla->irq_base = 0;
		return 0;
	}

	if (!tabla->irq_base) {
		dev_err(tabla->dev,
			"No interrupt base specified, no interrupts\n");
		return 0;
	}
	/* Mask the individual interrupt sources */
	for (i = 0, cur_irq = tabla->irq_base; i < TABLA_NUM_IRQS; i++,
		cur_irq++) {

		irq_set_chip_data(cur_irq, tabla);

		if (tabla_irqs[i].level)
			irq_set_chip_and_handler(cur_irq, &tabla_irq_chip,
					 handle_level_irq);
		else
			irq_set_chip_and_handler(cur_irq, &tabla_irq_chip,
					 handle_edge_irq);

		irq_set_nested_thread(cur_irq, 1);

		/* ARM needs us to explicitly flag the IRQ as valid
		 * and will set them noprobe when we do so. */
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		set_irq_noprobe(cur_irq);
#endif

		tabla->irq_masks_cur[BIT_BYTE(i)] |= BYTE_BIT_MASK(i);
		tabla->irq_masks_cache[BIT_BYTE(i)] |= BYTE_BIT_MASK(i);
		tabla->irq_level[BIT_BYTE(i)] |= tabla_irqs[i].level <<
			(i % BITS_PER_BYTE);
	}
	for (i = 0; i < TABLA_NUM_IRQ_REGS; i++) {
		/* Initialize interrupt mask and level registers */
		tabla_reg_write(tabla, TABLA_A_INTR_LEVEL0 + i,
			tabla->irq_level[i]);
		tabla_reg_write(tabla, TABLA_A_INTR_MASK0 + i,
			tabla->irq_masks_cur[i]);
	}

	ret = request_threaded_irq(tabla->irq, NULL, tabla_irq_thread,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   "tabla", tabla);
	if (ret != 0)
		dev_err(tabla->dev, "Failed to request IRQ %d: %d\n",
			tabla->irq, ret);
	else {
		ret = enable_irq_wake(tabla->irq);
		if (ret == 0) {
			ret = device_init_wakeup(tabla->dev, 1);
			if (ret) {
				dev_err(tabla->dev, "Failed to init device"
					"wakeup : %d\n", ret);
				disable_irq_wake(tabla->irq);
			}
		} else
			dev_err(tabla->dev, "Failed to set wake interrupt on"
				" IRQ %d: %d\n", tabla->irq, ret);
		if (ret)
			free_irq(tabla->irq, tabla);
	}

	if (ret)
		mutex_destroy(&tabla->irq_lock);

	return ret;
}

void tabla_irq_exit(struct tabla *tabla)
{
	if (tabla->irq) {
		disable_irq_wake(tabla->irq);
		free_irq(tabla->irq, tabla);
		device_init_wakeup(tabla->dev, 0);
	}
	mutex_destroy(&tabla->irq_lock);
}
