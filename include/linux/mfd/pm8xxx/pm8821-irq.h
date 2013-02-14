/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
/*
 * Qualcomm PMIC irq 8821 driver header file
 *
 */

#ifndef __MFD_PM8821_IRQ_H
#define __MFD_PM8821_IRQ_H

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mfd/pm8xxx/irq.h>

#ifdef CONFIG_MFD_PM8821_IRQ
int pm8821_get_irq_stat(struct pm_irq_chip *chip, int irq);
struct pm_irq_chip *pm8821_irq_init(struct device *dev,
				const struct pm8xxx_irq_platform_data *pdata);
int pm8821_irq_exit(struct pm_irq_chip *chip);
#else
static inline int pm8821_get_irq_stat(struct pm_irq_chip *chip, int irq)
{
	return -ENXIO;
}
static inline struct pm_irq_chip *pm8821_irq_init(const struct device *dev,
				const struct pm8xxx_irq_platform_data *pdata)
{
	return ERR_PTR(-ENXIO);
}
static inline int pm8821_irq_exit(struct pm_irq_chip *chip)
{
	return -ENXIO;
}
#endif /* CONFIG_MFD_PM8821_IRQ */
#endif /* __MFD_PM8821_IRQ_H */
