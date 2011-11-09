/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __PMIC8901_H__
#define __PMIC8901_H__
/*
 * Qualcomm PMIC8901 driver header file
 *
 */

#include <linux/irq.h>
#include <linux/mfd/core.h>

/* PM8901 interrupt numbers */

#define PM8901_MPPS		4

#define PM8901_IRQ_BLOCK_BIT(block, bit) ((block) * 8 + (bit))

/* MPPs [0,N) */
#define PM8901_MPP_IRQ(base, mpp)	((base) + \
					PM8901_IRQ_BLOCK_BIT(6, (mpp)))

#define PM8901_TEMP_ALARM_IRQ(base)	((base) + PM8901_IRQ_BLOCK_BIT(6, 4))
#define PM8901_TEMP_HI_ALARM_IRQ(base)	((base) + PM8901_IRQ_BLOCK_BIT(6, 5))

struct pm8901_chip;

struct pm8901_platform_data {
	/* This table is only needed for misc interrupts. */
	int		irq_base;
	int		irq;

	int		num_subdevs;
	struct mfd_cell *sub_devices;
	int		irq_trigger_flags;
};

struct pm8901_gpio_platform_data {
	int	gpio_base;
	int	irq_base;
};

/* chip revision */
#define PM_8901_REV_1p0			0xF1
#define PM_8901_REV_1p1			0xF2
#define PM_8901_REV_2p0			0xF3

int pm8901_read(struct pm8901_chip *pm_chip, u16 addr, u8 *values,
		unsigned int len);
int pm8901_write(struct pm8901_chip *pm_chip, u16 addr, u8 *values,
		 unsigned int len);

int pm8901_rev(struct pm8901_chip *pm_chip);

int pm8901_irq_get_rt_status(struct pm8901_chip *pm_chip, int irq);

#ifdef CONFIG_PMIC8901
int pm8901_reset_pwr_off(int reset);
#else
static inline int pm8901_reset_pwr_off(int reset) { return 0; }
#endif

#endif /* __PMIC8901_H__ */
