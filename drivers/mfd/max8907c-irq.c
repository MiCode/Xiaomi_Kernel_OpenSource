/*
 * Battery driver for Maxim MAX8907C
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 * Based on driver/mfd/max8925-core.c, Copyright (C) 2009-2010 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max8907c.h>

struct max8907c_irq_data {
	int	reg;
	int	mask_reg;
	int	enable;		/* enable or not */
	int	offs;		/* bit offset in mask register */
	bool	is_rtc;
	int	wake;
};

static struct max8907c_irq_data max8907c_irqs[] = {
	[MAX8907C_IRQ_VCHG_DC_OVP] = {
		.reg		= MAX8907C_REG_CHG_IRQ1,
		.mask_reg	= MAX8907C_REG_CHG_IRQ1_MASK,
		.offs		= 1 << 0,
	},
	[MAX8907C_IRQ_VCHG_DC_F] = {
		.reg		= MAX8907C_REG_CHG_IRQ1,
		.mask_reg	= MAX8907C_REG_CHG_IRQ1_MASK,
		.offs		= 1 << 1,
	},
	[MAX8907C_IRQ_VCHG_DC_R] = {
		.reg		= MAX8907C_REG_CHG_IRQ1,
		.mask_reg	= MAX8907C_REG_CHG_IRQ1_MASK,
		.offs		= 1 << 2,
	},
	[MAX8907C_IRQ_VCHG_THM_OK_R] = {
		.reg		= MAX8907C_REG_CHG_IRQ2,
		.mask_reg	= MAX8907C_REG_CHG_IRQ2_MASK,
		.offs		= 1 << 0,
	},
	[MAX8907C_IRQ_VCHG_THM_OK_F] = {
		.reg		= MAX8907C_REG_CHG_IRQ2,
		.mask_reg	= MAX8907C_REG_CHG_IRQ2_MASK,
		.offs		= 1 << 1,
	},
	[MAX8907C_IRQ_VCHG_MBATTLOW_F] = {
		.reg		= MAX8907C_REG_CHG_IRQ2,
		.mask_reg	= MAX8907C_REG_CHG_IRQ2_MASK,
		.offs		= 1 << 2,
	},
	[MAX8907C_IRQ_VCHG_MBATTLOW_R] = {
		.reg		= MAX8907C_REG_CHG_IRQ2,
		.mask_reg	= MAX8907C_REG_CHG_IRQ2_MASK,
		.offs		= 1 << 3,
	},
	[MAX8907C_IRQ_VCHG_RST] = {
		.reg		= MAX8907C_REG_CHG_IRQ2,
		.mask_reg	= MAX8907C_REG_CHG_IRQ2_MASK,
		.offs		= 1 << 4,
	},
	[MAX8907C_IRQ_VCHG_DONE] = {
		.reg		= MAX8907C_REG_CHG_IRQ2,
		.mask_reg	= MAX8907C_REG_CHG_IRQ2_MASK,
		.offs		= 1 << 5,
	},
	[MAX8907C_IRQ_VCHG_TOPOFF] = {
		.reg		= MAX8907C_REG_CHG_IRQ2,
		.mask_reg	= MAX8907C_REG_CHG_IRQ2_MASK,
		.offs		= 1 << 6,
	},
	[MAX8907C_IRQ_VCHG_TMR_FAULT] = {
		.reg		= MAX8907C_REG_CHG_IRQ2,
		.mask_reg	= MAX8907C_REG_CHG_IRQ2_MASK,
		.offs		= 1 << 7,
	},
	[MAX8907C_IRQ_GPM_RSTIN] = {
		.reg		= MAX8907C_REG_ON_OFF_IRQ1,
		.mask_reg	= MAX8907C_REG_ON_OFF_IRQ1_MASK,
		.offs		= 1 << 0,
	},
	[MAX8907C_IRQ_GPM_MPL] = {
		.reg		= MAX8907C_REG_ON_OFF_IRQ1,
		.mask_reg	= MAX8907C_REG_ON_OFF_IRQ1_MASK,
		.offs		= 1 << 1,
	},
	[MAX8907C_IRQ_GPM_SW_3SEC] = {
		.reg		= MAX8907C_REG_ON_OFF_IRQ1,
		.mask_reg	= MAX8907C_REG_ON_OFF_IRQ1_MASK,
		.offs		= 1 << 2,
	},
	[MAX8907C_IRQ_GPM_EXTON_F] = {
		.reg		= MAX8907C_REG_ON_OFF_IRQ1,
		.mask_reg	= MAX8907C_REG_ON_OFF_IRQ1_MASK,
		.offs		= 1 << 3,
	},
	[MAX8907C_IRQ_GPM_EXTON_R] = {
		.reg		= MAX8907C_REG_ON_OFF_IRQ1,
		.mask_reg	= MAX8907C_REG_ON_OFF_IRQ1_MASK,
		.offs		= 1 << 4,
	},
	[MAX8907C_IRQ_GPM_SW_1SEC] = {
		.reg		= MAX8907C_REG_ON_OFF_IRQ1,
		.mask_reg	= MAX8907C_REG_ON_OFF_IRQ1_MASK,
		.offs		= 1 << 5,
	},
	[MAX8907C_IRQ_GPM_SW_F] = {
		.reg		= MAX8907C_REG_ON_OFF_IRQ1,
		.mask_reg	= MAX8907C_REG_ON_OFF_IRQ1_MASK,
		.offs		= 1 << 6,
	},
	[MAX8907C_IRQ_GPM_SW_R] = {
		.reg		= MAX8907C_REG_ON_OFF_IRQ1,
		.mask_reg	= MAX8907C_REG_ON_OFF_IRQ1_MASK,
		.offs		= 1 << 7,
	},
	[MAX8907C_IRQ_GPM_SYSCKEN_F] = {
		.reg		= MAX8907C_REG_ON_OFF_IRQ2,
		.mask_reg	= MAX8907C_REG_ON_OFF_IRQ2_MASK,
		.offs		= 1 << 0,
	},
	[MAX8907C_IRQ_GPM_SYSCKEN_R] = {
		.reg		= MAX8907C_REG_ON_OFF_IRQ2,
		.mask_reg	= MAX8907C_REG_ON_OFF_IRQ2_MASK,
		.offs		= 1 << 1,
	},
	[MAX8907C_IRQ_RTC_ALARM1] = {
		.reg		= MAX8907C_REG_RTC_IRQ,
		.mask_reg	= MAX8907C_REG_RTC_IRQ_MASK,
		.offs		= 1 << 2,
		.is_rtc		= true,
	},
	[MAX8907C_IRQ_RTC_ALARM0] = {
		.reg		= MAX8907C_REG_RTC_IRQ,
		.mask_reg	= MAX8907C_REG_RTC_IRQ_MASK,
		.offs		= 1 << 3,
		.is_rtc		= true,
	},
};

static inline struct max8907c_irq_data *irq_to_max8907c(struct max8907c *chip,
						      int irq)
{
	return &max8907c_irqs[irq - chip->irq_base];
}

static irqreturn_t max8907c_irq(int irq, void *data)
{
	struct max8907c *chip = data;
	struct max8907c_irq_data *irq_data;
	struct i2c_client *i2c;
	int read_reg = -1, value = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(max8907c_irqs); i++) {
		irq_data = &max8907c_irqs[i];

		if (irq_data->is_rtc)
			i2c = chip->i2c_rtc;
		else
			i2c = chip->i2c_power;

		if (read_reg != irq_data->reg) {
			read_reg = irq_data->reg;
			value = max8907c_reg_read(i2c, irq_data->reg);
		}

		if (value & irq_data->enable)
			handle_nested_irq(chip->irq_base + i);
	}
	return IRQ_HANDLED;
}

static void max8907c_irq_lock(struct irq_data *data)
{
	struct max8907c *chip = irq_data_get_irq_chip_data(data);

	mutex_lock(&chip->irq_lock);
}

static void max8907c_irq_sync_unlock(struct irq_data *data)
{
	struct max8907c *chip = irq_data_get_irq_chip_data(data);
	struct max8907c_irq_data *irq_data;
	unsigned char irq_chg[2], irq_on[2];
	unsigned char irq_rtc;
	int i;

	irq_chg[0] = irq_chg[1] = irq_on[0] = irq_on[1] = irq_rtc = 0xFF;

	for (i = 0; i < ARRAY_SIZE(max8907c_irqs); i++) {
		irq_data = &max8907c_irqs[i];
		/* 1 -- disable, 0 -- enable */
		switch (irq_data->mask_reg) {
		case MAX8907C_REG_CHG_IRQ1_MASK:
			irq_chg[0] &= ~irq_data->enable;
			break;
		case MAX8907C_REG_CHG_IRQ2_MASK:
			irq_chg[1] &= ~irq_data->enable;
			break;
		case MAX8907C_REG_ON_OFF_IRQ1_MASK:
			irq_on[0] &= ~irq_data->enable;
			break;
		case MAX8907C_REG_ON_OFF_IRQ2_MASK:
			irq_on[1] &= ~irq_data->enable;
			break;
		case MAX8907C_REG_RTC_IRQ_MASK:
			irq_rtc &= ~irq_data->enable;
			break;
		default:
			dev_err(chip->dev, "wrong IRQ\n");
			break;
		}
	}
	/* update mask into registers */
	if (chip->cache_chg[0] != irq_chg[0]) {
		chip->cache_chg[0] = irq_chg[0];
		max8907c_reg_write(chip->i2c_power, MAX8907C_REG_CHG_IRQ1_MASK,
			irq_chg[0]);
	}
	if (chip->cache_chg[1] != irq_chg[1]) {
		chip->cache_chg[1] = irq_chg[1];
		max8907c_reg_write(chip->i2c_power, MAX8907C_REG_CHG_IRQ2_MASK,
			irq_chg[1]);
	}
	if (chip->cache_on[0] != irq_on[0]) {
		chip->cache_on[0] = irq_on[0];
		max8907c_reg_write(chip->i2c_power, MAX8907C_REG_ON_OFF_IRQ1_MASK,
				irq_on[0]);
	}
	if (chip->cache_on[1] != irq_on[1]) {
		chip->cache_on[1] = irq_on[1];
		max8907c_reg_write(chip->i2c_power, MAX8907C_REG_ON_OFF_IRQ2_MASK,
				irq_on[1]);
	}
	if (chip->cache_rtc != irq_rtc) {
		chip->cache_rtc = irq_rtc;
		max8907c_reg_write(chip->i2c_rtc, MAX8907C_REG_RTC_IRQ_MASK,
				   irq_rtc);
	}

	mutex_unlock(&chip->irq_lock);
}

static void max8907c_irq_enable(struct irq_data *data)
{
	struct max8907c *chip = irq_data_get_irq_chip_data(data);
	max8907c_irqs[data->irq - chip->irq_base].enable
		= max8907c_irqs[data->irq - chip->irq_base].offs;
}

static void max8907c_irq_disable(struct irq_data *data)
{
	struct max8907c *chip = irq_data_get_irq_chip_data(data);
	max8907c_irqs[data->irq - chip->irq_base].enable = 0;
}

static int max8907c_irq_set_wake(struct irq_data *data, unsigned int on)
{
	struct max8907c *chip = irq_data_get_irq_chip_data(data);
	if (on) {
		max8907c_irqs[data->irq - chip->irq_base].wake
			= max8907c_irqs[data->irq - chip->irq_base].enable;
	} else {
		max8907c_irqs[data->irq - chip->irq_base].wake = 0;
	}
	return 0;
}

static struct irq_chip max8907c_irq_chip = {
	.name			= "max8907c",
	.irq_bus_lock		= max8907c_irq_lock,
	.irq_bus_sync_unlock	= max8907c_irq_sync_unlock,
	.irq_enable		= max8907c_irq_enable,
	.irq_disable		= max8907c_irq_disable,
	.irq_set_wake		= max8907c_irq_set_wake,
};

int max8907c_irq_init(struct max8907c *chip, int irq, int irq_base)
{
	unsigned long flags = IRQF_ONESHOT;
	struct irq_desc *desc;
	int i, ret;
	int __irq;

	if (!irq_base || !irq) {
		dev_warn(chip->dev, "No interrupt support\n");
		return -EINVAL;
	}
	/* clear all interrupts */
	max8907c_reg_read(chip->i2c_power, MAX8907C_REG_CHG_IRQ1);
	max8907c_reg_read(chip->i2c_power, MAX8907C_REG_CHG_IRQ2);
	max8907c_reg_read(chip->i2c_power, MAX8907C_REG_ON_OFF_IRQ1);
	max8907c_reg_read(chip->i2c_power, MAX8907C_REG_ON_OFF_IRQ2);
	max8907c_reg_read(chip->i2c_rtc, MAX8907C_REG_RTC_IRQ);
	/* mask all interrupts */
	max8907c_reg_write(chip->i2c_rtc, MAX8907C_REG_ALARM0_CNTL, 0);
	max8907c_reg_write(chip->i2c_rtc, MAX8907C_REG_ALARM1_CNTL, 0);
	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_CHG_IRQ1_MASK, 0xff);
	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_CHG_IRQ2_MASK, 0xff);
	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_ON_OFF_IRQ1_MASK, 0xff);
	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_ON_OFF_IRQ2_MASK, 0xff);
	max8907c_reg_write(chip->i2c_rtc, MAX8907C_REG_RTC_IRQ_MASK, 0xff);

	chip->cache_chg[0] = chip->cache_chg[1] =
		chip->cache_on[0] = chip->cache_on[1] =
		chip->cache_rtc = 0xFF;

	mutex_init(&chip->irq_lock);
	chip->core_irq = irq;
	chip->irq_base = irq_base;
	desc = irq_to_desc(chip->core_irq);

	/* register with genirq */
	for (i = 0; i < ARRAY_SIZE(max8907c_irqs); i++) {
		__irq = i + chip->irq_base;
		irq_set_chip_data(__irq, chip);
		irq_set_chip_and_handler(__irq, &max8907c_irq_chip,
					 handle_edge_irq);
		irq_set_nested_thread(__irq, 1);
#ifdef CONFIG_ARM
		/* ARM requires an extra step to clear IRQ_NOREQUEST, which it
		 * sets on behalf of every irq_chip.
		 */
		set_irq_flags(__irq, IRQF_VALID);
#else
		irq_set_noprobe(__irq);
#endif
	}

	ret = request_threaded_irq(irq, NULL, max8907c_irq, flags,
				   "max8907c", chip);
	if (ret) {
		dev_err(chip->dev, "Failed to request core IRQ: %d\n", ret);
		chip->core_irq = 0;
	}

	device_init_wakeup(chip->dev, 1);

	return ret;
}

int max8907c_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct max8907c *chip = i2c_get_clientdata(i2c);

	struct max8907c_irq_data *irq_data;
	unsigned char irq_chg[2], irq_on[2];
	unsigned char irq_rtc;
	int i;

	irq_chg[0] = irq_chg[1] = irq_on[0] = irq_on[1] = irq_rtc = 0xFF;

	for (i = 0; i < ARRAY_SIZE(max8907c_irqs); i++) {
		irq_data = &max8907c_irqs[i];
		/* 1 -- disable, 0 -- enable */
		switch (irq_data->mask_reg) {
		case MAX8907C_REG_CHG_IRQ1_MASK:
			irq_chg[0] &= ~irq_data->wake;
			break;
		case MAX8907C_REG_CHG_IRQ2_MASK:
			irq_chg[1] &= ~irq_data->wake;
			break;
		case MAX8907C_REG_ON_OFF_IRQ1_MASK:
			irq_on[0] &= ~irq_data->wake;
			break;
		case MAX8907C_REG_ON_OFF_IRQ2_MASK:
			irq_on[1] &= ~irq_data->wake;
			break;
		case MAX8907C_REG_RTC_IRQ_MASK:
			irq_rtc &= ~irq_data->wake;
			break;
		default:
			dev_err(chip->dev, "wrong IRQ\n");
			break;
		}
	}

	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_CHG_IRQ1_MASK, irq_chg[0]);
	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_CHG_IRQ2_MASK, irq_chg[1]);
	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_ON_OFF_IRQ1_MASK, irq_on[0]);
	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_ON_OFF_IRQ2_MASK, irq_on[1]);
	max8907c_reg_write(chip->i2c_rtc, MAX8907C_REG_RTC_IRQ_MASK, irq_rtc);

	if (device_may_wakeup(chip->dev))
		enable_irq_wake(chip->core_irq);
	else
		disable_irq(chip->core_irq);

	return 0;
}

int max8907c_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct max8907c *chip = i2c_get_clientdata(i2c);

	if (device_may_wakeup(chip->dev))
		disable_irq_wake(chip->core_irq);
	else
		enable_irq(chip->core_irq);

	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_CHG_IRQ1_MASK, chip->cache_chg[0]);
	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_CHG_IRQ2_MASK, chip->cache_chg[1]);
	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_ON_OFF_IRQ1_MASK, chip->cache_on[0]);
	max8907c_reg_write(chip->i2c_power, MAX8907C_REG_ON_OFF_IRQ2_MASK, chip->cache_on[1]);
	max8907c_reg_write(chip->i2c_rtc, MAX8907C_REG_RTC_IRQ_MASK, chip->cache_rtc);

	return 0;
}

void max8907c_irq_free(struct max8907c *chip)
{
	if (chip->core_irq)
		free_irq(chip->core_irq, chip);
}

