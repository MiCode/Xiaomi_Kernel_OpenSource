/*
 * TI Palmas MFD Driver
 *
 * Copyright 2011-2012 Texas Instruments Inc.
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/mfd/core.h>
#include <linux/mfd/palmas.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#define EXT_PWR_REQ (PALMAS_EXT_CONTROL_ENABLE1 |	\
			PALMAS_EXT_CONTROL_ENABLE2 |	\
			PALMAS_EXT_CONTROL_NSLEEP)

static const struct resource charger_resource[] = {
	{
		.name = "PALMAS-CHARGER",
		.start = PALMAS_CHARGER_IRQ,
		.end = PALMAS_CHARGER_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct resource thermal_resource[] = {
	{
		.name = "palmas-junction-temp",
		.start = PALMAS_HOTDIE_IRQ,
		.end = PALMAS_HOTDIE_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

enum palmas_ids {
	PALMAS_PIN_MUX_ID = 0,
	PALMAS_PMIC_ID,
	PALMAS_GPIO_ID,
	PALMAS_LEDS_ID,
	PALMAS_WDT_ID,
	PALMAS_RTC_ID,
	PALMAS_PWRBUTTON_ID,
	PALMAS_GPADC_ID,
	PALMAS_RESOURCE_ID,
	PALMAS_CLK_ID,
	PALMAS_PWM_ID,
	PALMAS_USB_ID,
	PALMAS_EXTCON_ID,
	PALMAS_BATTERY_GAUGE_ID,
	PALMAS_CHARGER_ID,
	PALMAS_SIM_ID,
	PALMAS_PM_ID,
	PALMAS_THERM_ID,
};

static struct resource palmas_rtc_resources[] = {
	{
		.start  = PALMAS_RTC_ALARM_IRQ,
		.end    = PALMAS_RTC_ALARM_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

#define TPS65913_SUB_MODULE	(BIT(PALMAS_PIN_MUX_ID) | 		\
		BIT(PALMAS_PMIC_ID) | BIT(PALMAS_GPIO_ID) | 		\
		BIT(PALMAS_LEDS_ID) | BIT(PALMAS_WDT_ID) |		\
		BIT(PALMAS_RTC_ID) | BIT(PALMAS_PWRBUTTON_ID) |		\
		BIT(PALMAS_GPADC_ID) |	BIT(PALMAS_RESOURCE_ID) |	\
		BIT(PALMAS_CLK_ID) | BIT(PALMAS_PWM_ID)		| 	\
		BIT(PALMAS_USB_ID) | BIT(PALMAS_EXTCON_ID)	|	\
		BIT(PALMAS_PM_ID) | BIT(PALMAS_THERM_ID))

#define TPS80036_SUB_MODULE	(TPS65913_SUB_MODULE |			\
		BIT(PALMAS_BATTERY_GAUGE_ID) | BIT(PALMAS_CHARGER_ID) |	\
		BIT(PALMAS_SIM_ID))

static unsigned int submodule_lists[PALMAS_MAX_CHIP_ID] = {
	[PALMAS]	= TPS65913_SUB_MODULE,
	[TWL6035]	= TPS65913_SUB_MODULE,
	[TWL6037]	= TPS65913_SUB_MODULE,
	[TPS65913]	= TPS65913_SUB_MODULE,
	[TPS80036]	= TPS80036_SUB_MODULE,
};

static const struct mfd_cell palmas_children[] = {
	{
		.name = "palmas-pinctrl",
		.id = PALMAS_PIN_MUX_ID,
	},
	{
		.name = "palmas-pmic",
		.id = PALMAS_PMIC_ID,
	},
	{
		.name = "palmas-gpio",
		.id = PALMAS_GPIO_ID,
	},
	{
		.name = "palmas-leds",
		.id = PALMAS_LEDS_ID,
	},
	{
		.name = "palmas-wdt",
		.id = PALMAS_WDT_ID,
	},
	{
		.name = "palmas-rtc",
		.id = PALMAS_RTC_ID,
		.resources = &palmas_rtc_resources[0],
		.num_resources = ARRAY_SIZE(palmas_rtc_resources),
	},
	{
		.name = "palmas-pwrbutton",
		.id = PALMAS_PWRBUTTON_ID,
	},
	{
		.name = "palmas-gpadc",
		.id = PALMAS_GPADC_ID,
	},
	{
		.name = "palmas-resource",
		.id = PALMAS_RESOURCE_ID,
	},
	{
		.name = "palmas-clk",
		.id = PALMAS_CLK_ID,
	},
	{
		.name = "palmas-pwm",
		.id = PALMAS_PWM_ID,
	},
	{
		.name = "palmas-usb",
		.id = PALMAS_USB_ID,
	},
	{
		.name = "palmas-extcon",
		.id = PALMAS_EXTCON_ID,
	},
	{
		.name = "palmas-battery-gauge",
		.id = PALMAS_BATTERY_GAUGE_ID,
	},
	{
		.name = "palmas-charger",
		.id = PALMAS_CHARGER_ID,
		.num_resources = ARRAY_SIZE(charger_resource),
		.resources = charger_resource,
	},
	{
		.name = "palmas-sim",
		.id = PALMAS_SIM_ID,
	},
	{
		.name = "palmas-pm",
		.id = PALMAS_PM_ID,
	},
	{
		.name = "palmas-thermal",
		.num_resources = ARRAY_SIZE(thermal_resource),
		.resources = thermal_resource,
		.id = PALMAS_THERM_ID,
	},
};

static bool is_volatile_palmas_func_reg(struct device *dev, unsigned int reg)
{
	if ((reg >= PALMAS_BASE_TO_REG(PALMAS_SMPS_BASE,
				       PALMAS_SMPS12_CTRL)) &&
	    (reg <= PALMAS_BASE_TO_REG(PALMAS_SMPS_BASE,
				       PALMAS_SMPS9_VOLTAGE))) {
		struct palmas *palmas = dev_get_drvdata(dev);
		int volatile_reg = test_bit(
			PALMAS_REG_TO_FN_ADDR(PALMAS_SMPS_BASE, reg),
			palmas->volatile_smps_registers);

		if (!volatile_reg)
			return false;
	}
	return true;
}

static void palmas_regmap_config0_lock(void *lock)
{
	struct palmas *palmas = lock;

	if (palmas->shutdown && (in_atomic() || irqs_disabled())) {
		dev_info(palmas->dev, "Xfer without lock\n");
		return;
	}

	mutex_lock(&palmas->mutex_config0);
}

static void palmas_regmap_config0_unlock(void *lock)
{
	struct palmas *palmas = lock;

	if (palmas->shutdown && (in_atomic() || irqs_disabled()))
		return;

	mutex_unlock(&palmas->mutex_config0);
}

static struct regmap_config palmas_regmap_config[PALMAS_NUM_CLIENTS] = {
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = PALMAS_BASE_TO_REG(PALMAS_PU_PD_OD_BASE,
					PALMAS_PRIMARY_SECONDARY_PAD4),
		.volatile_reg = is_volatile_palmas_func_reg,
		.lock = palmas_regmap_config0_lock,
		.unlock = palmas_regmap_config0_unlock,
		.cache_type  = REGCACHE_RBTREE,
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = PALMAS_BASE_TO_REG(PALMAS_GPADC_BASE,
					PALMAS_GPADC_SMPS_VSEL_MONITORING),
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = PALMAS_BASE_TO_REG(PALMAS_TRIM_GPADC_BASE,
					PALMAS_GPADC_TRIM16),
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register =	PALMAS_BASE_TO_REG(PALMAS_CHARGER_BASE,
					PALMAS_CHARGER_REG10),
	},
};

static const int palmas_i2c_ids[PALMAS_NUM_CLIENTS] = {
	0x58,
	0x59,
	0x5a,
	0x6a,
};

struct palmas_regs {
	int reg_base;
	int reg_add;
};

struct palmas_irq_regs {
	struct palmas_regs mask_reg[PALMAS_MAX_INTERRUPT_MASK_REG];
	struct palmas_regs status_reg[PALMAS_MAX_INTERRUPT_MASK_REG];
	struct palmas_regs edge_reg[PALMAS_MAX_INTERRUPT_EDGE_REG];
};

#define PALMAS_REGS(base, add)	{ .reg_base = base, .reg_add = add, }
static struct palmas_irq_regs palmas_irq_regs = {
	.mask_reg = {
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT1_MASK),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT2_MASK),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT3_MASK),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT4_MASK),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT5_MASK),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT6_MASK),
	},
	.status_reg = {
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT1_STATUS),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT2_STATUS),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT3_STATUS),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT4_STATUS),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT5_STATUS),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT6_STATUS),
	},
	.edge_reg = {
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT1_EDGE_DETECT1_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT1_EDGE_DETECT2_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT2_EDGE_DETECT1_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT2_EDGE_DETECT2_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT3_EDGE_DETECT1_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT3_EDGE_DETECT2_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT4_EDGE_DETECT1),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT4_EDGE_DETECT2),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT5_EDGE_DETECT1),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT5_EDGE_DETECT2),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT6_EDGE_DETECT1_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT6_EDGE_DETECT2_RESERVED),
	},
};

struct palmas_irq {
	unsigned int	interrupt_mask;
	unsigned int	rising_mask;
	unsigned int	falling_mask;
	unsigned int	edge_mask;
	unsigned int	mask_reg_index;
	unsigned int	edge_reg_index;
};

#define PALMAS_IRQ(_nr, _imask, _mrindex, _rmask, _fmask, _erindex)	\
[PALMAS_##_nr] = {							\
			.interrupt_mask = PALMAS_##_imask,		\
			.mask_reg_index = _mrindex,			\
			.rising_mask = _rmask,				\
			.falling_mask = _fmask,				\
			.edge_mask = _rmask | _fmask,			\
			.edge_reg_index = _erindex			\
		}

static struct palmas_irq palmas_irqs[] = {
	/* INT1 IRQs */
	PALMAS_IRQ(CHARG_DET_N_VBUS_OVV_IRQ,
			INT1_STATUS_CHARG_DET_N_VBUS_OVV, 0, 0, 0, 0),
	PALMAS_IRQ(PWRON_IRQ, INT1_STATUS_PWRON, 0, 0, 0, 0),
	PALMAS_IRQ(LONG_PRESS_KEY_IRQ, INT1_STATUS_LONG_PRESS_KEY, 0, 0, 0, 0),
	PALMAS_IRQ(RPWRON_IRQ, INT1_STATUS_RPWRON, 0, 0, 0, 0),
	PALMAS_IRQ(PWRDOWN_IRQ, INT1_STATUS_PWRDOWN, 0, 0, 0, 0),
	PALMAS_IRQ(HOTDIE_IRQ, INT1_STATUS_HOTDIE, 0, 0, 0, 0),
	PALMAS_IRQ(VSYS_MON_IRQ, INT1_STATUS_VSYS_MON, 0, 0, 0, 0),
	PALMAS_IRQ(VBAT_MON_IRQ, INT1_STATUS_VBAT_MON, 0, 0, 0, 0),
	/* INT2 IRQs */
	PALMAS_IRQ(RTC_ALARM_IRQ, INT2_STATUS_RTC_ALARM, 1, 0, 0, 0),
	PALMAS_IRQ(RTC_TIMER_IRQ, INT2_STATUS_RTC_TIMER, 1, 0, 0, 0),
	PALMAS_IRQ(WDT_IRQ, INT2_STATUS_WDT, 1, 0, 0, 0),
	PALMAS_IRQ(BATREMOVAL_IRQ, INT2_STATUS_BATREMOVAL, 1, 0, 0, 0),
	PALMAS_IRQ(RESET_IN_IRQ, INT2_STATUS_RESET_IN, 1, 0, 0, 0),
	PALMAS_IRQ(FBI_BB_IRQ, INT2_STATUS_FBI_BB, 1, 0, 0, 0),
	PALMAS_IRQ(SHORT_IRQ, INT2_STATUS_SHORT, 1, 0, 0, 0),
	PALMAS_IRQ(VAC_ACOK_IRQ, INT2_STATUS_VAC_ACOK, 1, 0, 0, 0),
	/* INT3 IRQs */
	PALMAS_IRQ(GPADC_AUTO_0_IRQ, INT3_STATUS_GPADC_AUTO_0, 2, 0, 0, 0),
	PALMAS_IRQ(GPADC_AUTO_1_IRQ, INT3_STATUS_GPADC_AUTO_1, 2, 0, 0, 0),
	PALMAS_IRQ(GPADC_EOC_SW_IRQ, INT3_STATUS_GPADC_EOC_SW, 2, 0, 0, 0),
	PALMAS_IRQ(GPADC_EOC_RT_IRQ, INT3_STATUS_GPADC_EOC_RT, 2, 0, 0, 0),
	PALMAS_IRQ(ID_OTG_IRQ, INT3_STATUS_ID_OTG, 2, 0, 0, 0),
	PALMAS_IRQ(ID_IRQ, INT3_STATUS_ID, 2, 0, 0, 0),
	PALMAS_IRQ(VBUS_OTG_IRQ, INT3_STATUS_VBUS_OTG, 2, 0, 0, 0),
	PALMAS_IRQ(VBUS_IRQ, INT3_STATUS_VBUS, 2, 0, 0, 0),
	/* INT4 IRQs */
	PALMAS_IRQ(GPIO_0_IRQ, INT4_STATUS_GPIO_0, 3,
			PALMAS_INT4_EDGE_DETECT1_GPIO_0_RISING,
			PALMAS_INT4_EDGE_DETECT1_GPIO_0_FALLING, 6),
	PALMAS_IRQ(GPIO_1_IRQ, INT4_STATUS_GPIO_1, 3,
			PALMAS_INT4_EDGE_DETECT1_GPIO_1_RISING,
			PALMAS_INT4_EDGE_DETECT1_GPIO_1_FALLING, 6),
	PALMAS_IRQ(GPIO_2_IRQ, INT4_STATUS_GPIO_2, 3,
			PALMAS_INT4_EDGE_DETECT1_GPIO_2_RISING,
			PALMAS_INT4_EDGE_DETECT1_GPIO_2_FALLING, 6),
	PALMAS_IRQ(GPIO_3_IRQ, INT4_STATUS_GPIO_3, 3,
			PALMAS_INT4_EDGE_DETECT1_GPIO_3_RISING,
			PALMAS_INT4_EDGE_DETECT1_GPIO_3_FALLING, 6),
	PALMAS_IRQ(GPIO_4_IRQ, INT4_STATUS_GPIO_4, 3,
			PALMAS_INT4_EDGE_DETECT2_GPIO_4_RISING,
			PALMAS_INT4_EDGE_DETECT2_GPIO_4_FALLING, 7),
	PALMAS_IRQ(GPIO_5_IRQ, INT4_STATUS_GPIO_5, 3,
			PALMAS_INT4_EDGE_DETECT2_GPIO_5_RISING,
			PALMAS_INT4_EDGE_DETECT2_GPIO_5_FALLING, 7),
	PALMAS_IRQ(GPIO_6_IRQ, INT4_STATUS_GPIO_6, 3,
			PALMAS_INT4_EDGE_DETECT2_GPIO_6_RISING,
			PALMAS_INT4_EDGE_DETECT2_GPIO_6_FALLING, 7),
	PALMAS_IRQ(GPIO_7_IRQ, INT4_STATUS_GPIO_7, 3,
			PALMAS_INT4_EDGE_DETECT2_GPIO_7_RISING,
			PALMAS_INT4_EDGE_DETECT2_GPIO_7_FALLING, 7),
	/* INT5 IRQs */
	PALMAS_IRQ(GPIO_8_IRQ, INT5_STATUS_GPIO_8, 4,
			PALMAS_INT5_EDGE_DETECT1_GPIO_8_RISING,
			PALMAS_INT5_EDGE_DETECT1_GPIO_8_FALLING, 8),
	PALMAS_IRQ(GPIO_9_IRQ, INT5_STATUS_GPIO_9, 4,
			PALMAS_INT5_EDGE_DETECT1_GPIO_9_RISING,
			PALMAS_INT5_EDGE_DETECT1_GPIO_9_FALLING, 8),
	PALMAS_IRQ(GPIO_10_IRQ, INT5_STATUS_GPIO_10, 4,
			PALMAS_INT5_EDGE_DETECT1_GPIO_10_RISING,
			PALMAS_INT5_EDGE_DETECT1_GPIO_10_FALLING, 8),
	PALMAS_IRQ(GPIO_11_IRQ, INT5_STATUS_GPIO_11, 4,
			PALMAS_INT5_EDGE_DETECT1_GPIO_11_RISING,
			PALMAS_INT5_EDGE_DETECT1_GPIO_11_FALLING, 8),
	PALMAS_IRQ(GPIO_12_IRQ, INT5_STATUS_GPIO_12, 4,
			PALMAS_INT5_EDGE_DETECT2_GPIO_12_RISING,
			PALMAS_INT5_EDGE_DETECT2_GPIO_12_FALLING, 9),
	PALMAS_IRQ(GPIO_13_IRQ, INT5_STATUS_GPIO_13, 4,
			PALMAS_INT5_EDGE_DETECT2_GPIO_13_RISING,
			PALMAS_INT5_EDGE_DETECT2_GPIO_13_FALLING, 9),
	PALMAS_IRQ(GPIO_14_IRQ, INT5_STATUS_GPIO_14, 4,
			PALMAS_INT5_EDGE_DETECT2_GPIO_14_RISING,
			PALMAS_INT5_EDGE_DETECT2_GPIO_14_FALLING, 9),
	PALMAS_IRQ(GPIO_15_IRQ, INT5_STATUS_GPIO_15, 4,
			PALMAS_INT5_EDGE_DETECT2_GPIO_15_RISING,
			PALMAS_INT5_EDGE_DETECT2_GPIO_15_FALLING, 9),
	/* INT6 IRQs */
	PALMAS_IRQ(SIM1_IRQ, INT6_STATUS_SIM1, 5, 0, 0, 0),
	PALMAS_IRQ(SIM2_IRQ, INT6_STATUS_SIM2, 5, 0, 0, 0),
};

struct palmas_irq_chip_data {
	struct palmas		*palmas;
	int			irq_base;
	int			irq;
	struct mutex		irq_lock;
	struct irq_chip		irq_chip;
	struct irq_domain	*domain;

	struct palmas_irq_regs	*irq_regs;
	struct palmas_irq	*irqs;
	int			num_irqs;
	unsigned int		mask_value[PALMAS_MAX_INTERRUPT_MASK_REG];
	unsigned int		status_value[PALMAS_MAX_INTERRUPT_MASK_REG];
	unsigned int		edge_value[PALMAS_MAX_INTERRUPT_EDGE_REG];
	unsigned int		mask_def_value[PALMAS_MAX_INTERRUPT_MASK_REG];
	unsigned int		edge_def_value[PALMAS_MAX_INTERRUPT_EDGE_REG];
	int			num_mask_regs;
	int			num_edge_regs;
	int			wake_count;
};

static inline const struct palmas_irq *irq_to_palmas_irq(
	struct palmas_irq_chip_data *data, int irq)
{
	return &data->irqs[irq];
}

static void palmas_irq_lock(struct irq_data *data)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);

	mutex_lock(&d->irq_lock);
}

static void palmas_irq_sync_unlock(struct irq_data *data)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);
	int i, ret;

	for (i = 0; i < d->num_mask_regs; i++) {
		ret = palmas_update_bits(d->palmas,
				d->irq_regs->mask_reg[i].reg_base,
				d->irq_regs->mask_reg[i].reg_add,
				d->mask_def_value[i], d->mask_value[i]);
		if (ret < 0)
			dev_err(d->palmas->dev, "Failed to sync masks in %x\n",
					d->irq_regs->mask_reg[i].reg_add);
	}

	for (i = 0; i < d->num_edge_regs; i++) {
		if (!d->edge_def_value[i])
			continue;

		ret = palmas_update_bits(d->palmas,
				d->irq_regs->edge_reg[i].reg_base,
				d->irq_regs->edge_reg[i].reg_add,
				d->edge_def_value[i], d->edge_value[i]);
		if (ret < 0)
			dev_err(d->palmas->dev, "Failed to sync edge in %x\n",
					d->irq_regs->edge_reg[i].reg_add);
	}

	/* If we've changed our wakeup count propagate it to the parent */
	if (d->wake_count < 0)
		for (i = d->wake_count; i < 0; i++)
			irq_set_irq_wake(d->irq, 0);
	else if (d->wake_count > 0)
		for (i = 0; i < d->wake_count; i++)
			irq_set_irq_wake(d->irq, 1);

	d->wake_count = 0;

	mutex_unlock(&d->irq_lock);
}

static void palmas_irq_enable(struct irq_data *data)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);
	const struct palmas_irq *irq_data = irq_to_palmas_irq(d, data->hwirq);

	d->mask_value[irq_data->mask_reg_index] &= ~irq_data->interrupt_mask;
}

static void palmas_irq_disable(struct irq_data *data)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);
	const struct palmas_irq *irq_data = irq_to_palmas_irq(d, data->hwirq);

	d->mask_value[irq_data->mask_reg_index] |= irq_data->interrupt_mask;
}

static int palmas_irq_set_type(struct irq_data *data, unsigned int type)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);
	const struct palmas_irq *irq_data = irq_to_palmas_irq(d, data->hwirq);
	unsigned int reg = irq_data->edge_reg_index;

	if (!irq_data->edge_mask)
		return 0;

	d->edge_value[reg] &= ~irq_data->edge_mask;
	switch (type) {
	case IRQ_TYPE_EDGE_FALLING:
		d->edge_value[reg] |= irq_data->falling_mask;
		break;

	case IRQ_TYPE_EDGE_RISING:
		d->edge_value[reg] |= irq_data->rising_mask;
		break;

	case IRQ_TYPE_EDGE_BOTH:
		d->edge_value[reg] |= irq_data->edge_mask;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int palmas_irq_set_wake(struct irq_data *data, unsigned int on)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);

	if (on)
		d->wake_count++;
	else
		d->wake_count--;

	return 0;
}

static const struct irq_chip palmas_irq_chip = {
	.irq_bus_lock		= palmas_irq_lock,
	.irq_bus_sync_unlock	= palmas_irq_sync_unlock,
	.irq_disable		= palmas_irq_disable,
	.irq_enable		= palmas_irq_enable,
	.irq_set_type		= palmas_irq_set_type,
	.irq_set_wake		= palmas_irq_set_wake,
};

static irqreturn_t palmas_irq_thread(int irq, void *data)
{
	struct palmas_irq_chip_data *d = data;
	int ret, i;
	bool handled = false;

	for (i = 0; i < d->num_mask_regs; i++) {
		ret = palmas_read(d->palmas,
				d->irq_regs->status_reg[i].reg_base,
				d->irq_regs->status_reg[i].reg_add,
				&d->status_value[i]);

		if (ret != 0) {
			dev_err(d->palmas->dev,
				"Failed to read IRQ status: %d\n", ret);
			return IRQ_NONE;
		}
		d->status_value[i] &= ~d->mask_value[i];
	}

	for (i = 0; i < d->num_irqs; i++) {
		if (d->status_value[d->irqs[i].mask_reg_index] &
				d->irqs[i].interrupt_mask) {
			handle_nested_irq(irq_find_mapping(d->domain, i));
			handled = true;
		}
	}

	if (handled)
		return IRQ_HANDLED;
	else
		return IRQ_NONE;
}

static int palmas_irq_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hw)
{
	struct palmas_irq_chip_data *data = h->host_data;

	irq_set_chip_data(virq, data);
	irq_set_chip(virq, &data->irq_chip);
	irq_set_nested_thread(virq, 1);

	/* ARM needs us to explicitly flag the IRQ as valid
	 * and will set them noprobe when we do so. */
#ifdef CONFIG_ARM
	set_irq_flags(virq, IRQF_VALID);
#else
	irq_set_noprobe(virq);
#endif

	return 0;
}

static struct irq_domain_ops palmas_domain_ops = {
	.map	= palmas_irq_map,
	.xlate	= irq_domain_xlate_twocell,
};

static int palmas_add_irq_chip(struct palmas *palmas, int irq, int irq_flags,
			int irq_base, struct palmas_irq_chip_data **data)
{
	struct palmas_irq_chip_data *d;
	int i;
	int ret;
	unsigned int status_value;
	int num_irqs = ARRAY_SIZE(palmas_irqs);

	if (irq_base) {
		irq_base = irq_alloc_descs(irq_base, 0, num_irqs, 0);
		if (irq_base < 0) {
			dev_warn(palmas->dev, "Failed to allocate IRQs: %d\n",
				 irq_base);
			return irq_base;
		}
	}

	d = devm_kzalloc(palmas->dev, sizeof(*d), GFP_KERNEL);
	if (!d) {
		dev_err(palmas->dev, "mem alloc for d failed\n");
		return -ENOMEM;
	}

	d->palmas = palmas;
	d->irq = irq;
	d->irq_base = irq_base;
	mutex_init(&d->irq_lock);
	d->irq_chip = palmas_irq_chip;
	d->irq_chip.name = dev_name(palmas->dev);
	d->irq_regs = &palmas_irq_regs;

	d->irqs = palmas_irqs;
	d->num_mask_regs = PALMAS_MAX_INTERRUPT_MASK_REG;
	d->num_edge_regs = PALMAS_MAX_INTERRUPT_EDGE_REG;

	if (palmas->id != TPS80036) {
		d->num_mask_regs = 4;
		d->num_edge_regs = 8;
		num_irqs = num_irqs - 8;
	}

	d->num_irqs = num_irqs;
	d->wake_count = 0;
	*data = d;

	for (i = 0; i < d->num_irqs; i++) {
		d->mask_def_value[d->irqs[i].mask_reg_index] |=
						d->irqs[i].interrupt_mask;
		d->edge_def_value[d->irqs[i].edge_reg_index] |=
						d->irqs[i].edge_mask;
	}

	/* Mask all interrupts */
	for (i = 0; i < d->num_mask_regs; i++) {
		d->mask_value[i] = d->mask_def_value[i];
		ret = palmas_update_bits(d->palmas,
				d->irq_regs->mask_reg[i].reg_base,
				d->irq_regs->mask_reg[i].reg_add,
				d->mask_def_value[i], d->mask_value[i]);
		if (ret < 0)
			dev_err(d->palmas->dev, "Failed to update masks in %x\n",
					d->irq_regs->mask_reg[i].reg_add);
	}

	/* Set edge registers */
	for (i = 0; i < d->num_edge_regs; i++) {
		if (!d->edge_def_value[i])
			continue;

		ret = palmas_update_bits(d->palmas,
				d->irq_regs->edge_reg[i].reg_base,
				d->irq_regs->edge_reg[i].reg_add,
				d->edge_def_value[i], 0);
		if (ret < 0)
			dev_err(palmas->dev, "Failed to sync edge in %x\n",
					d->irq_regs->edge_reg[i].reg_add);
	}

	/* Clear all interrupts */
	for (i = 0; i < d->num_mask_regs; i++) {
		ret = palmas_read(d->palmas,
				d->irq_regs->status_reg[i].reg_base,
				d->irq_regs->status_reg[i].reg_add,
				&status_value);

		if (ret != 0) {
			dev_err(palmas->dev, "Failed to read status %x\n",
				d->irq_regs->status_reg[i].reg_add);
		}
	}

	if (irq_base)
		d->domain = irq_domain_add_legacy(palmas->dev->of_node,
						  num_irqs, irq_base, 0,
						  &palmas_domain_ops, d);
	else
		d->domain = irq_domain_add_linear(palmas->dev->of_node,
						  num_irqs,
						  &palmas_domain_ops, d);
	if (!d->domain) {
		dev_err(palmas->dev, "Failed to create IRQ domain\n");
		return -ENOMEM;
	}

	ret = request_threaded_irq(irq, NULL, palmas_irq_thread, irq_flags,
				   dev_name(palmas->dev), d);
	if (ret != 0) {
		dev_err(palmas->dev,
			"Failed to request IRQ %d: %d\n", irq, ret);
		return ret;
	}

	return 0;
}

static void palmas_del_irq_chip(int irq, struct palmas_irq_chip_data *d)
{
	if (d)
		free_irq(irq, d);
}

int palmas_irq_get_virq(struct palmas *palmas, int irq)
{
	struct palmas_irq_chip_data *data = palmas->irq_chip_data;

	if (!data->irqs[irq].interrupt_mask)
		return -EINVAL;

	return irq_create_mapping(data->domain, irq);
}
EXPORT_SYMBOL_GPL(palmas_irq_get_virq);

struct palmas_sleep_requestor_info {
	int id;
	int reg_offset;
	int bit_pos;
};

#define SLEEP_REQUESTOR(_id, _offset, _pos)		\
	[PALMAS_EXTERNAL_REQSTR_ID_##_id] = {		\
		.id = PALMAS_EXTERNAL_REQSTR_ID_##_id,	\
		.reg_offset = _offset,			\
		.bit_pos = _pos,			\
	}

static struct palmas_sleep_requestor_info sleep_reqt_info[] = {
	SLEEP_REQUESTOR(REGEN1, 0, 0),
	SLEEP_REQUESTOR(REGEN2, 0, 1),
	SLEEP_REQUESTOR(SYSEN1, 0, 2),
	SLEEP_REQUESTOR(SYSEN2, 0, 3),
	SLEEP_REQUESTOR(CLK32KG, 0, 4),
	SLEEP_REQUESTOR(CLK32KGAUDIO, 0, 5),
	SLEEP_REQUESTOR(REGEN3, 0, 6),
	SLEEP_REQUESTOR(SMPS12, 1, 0),
	SLEEP_REQUESTOR(SMPS3, 1, 1),
	SLEEP_REQUESTOR(SMPS45, 1, 2),
	SLEEP_REQUESTOR(SMPS6, 1, 3),
	SLEEP_REQUESTOR(SMPS7, 1, 4),
	SLEEP_REQUESTOR(SMPS8, 1, 5),
	SLEEP_REQUESTOR(SMPS9, 1, 6),
	SLEEP_REQUESTOR(SMPS10, 1, 7),
	SLEEP_REQUESTOR(LDO1, 2, 0),
	SLEEP_REQUESTOR(LDO2, 2, 1),
	SLEEP_REQUESTOR(LDO3, 2, 2),
	SLEEP_REQUESTOR(LDO4, 2, 3),
	SLEEP_REQUESTOR(LDO5, 2, 4),
	SLEEP_REQUESTOR(LDO6, 2, 5),
	SLEEP_REQUESTOR(LDO7, 2, 6),
	SLEEP_REQUESTOR(LDO8, 2, 7),
	SLEEP_REQUESTOR(LDO9, 3, 0),
	SLEEP_REQUESTOR(LDOLN, 3, 1),
	SLEEP_REQUESTOR(LDOUSB, 3, 2),
	SLEEP_REQUESTOR(LDO10, 3, 3),
	SLEEP_REQUESTOR(LDO11, 3, 4),
	SLEEP_REQUESTOR(LDO12, 3, 5),
	SLEEP_REQUESTOR(LDO13, 3, 6),
	SLEEP_REQUESTOR(LDO14, 3, 7),
	SLEEP_REQUESTOR(REGEN4, 0, 7),
	SLEEP_REQUESTOR(REGEN5, 18, 0),
	SLEEP_REQUESTOR(REGEN7, 18, 2),
};

static int palmas_resource_write(struct palmas *palmas, unsigned int reg,
	unsigned int value)
{
	unsigned int addr = PALMAS_BASE_TO_REG(PALMAS_RESOURCE_BASE, reg);

	return regmap_write(palmas->regmap[0], addr, value);
}

static int palmas_resource_update(struct palmas *palmas, unsigned int reg,
	unsigned int mask, unsigned int value)
{
	unsigned int addr = PALMAS_BASE_TO_REG(PALMAS_RESOURCE_BASE, reg);

	return regmap_update_bits(palmas->regmap[0], addr, mask, value);
}

static int palmas_control_update(struct palmas *palmas, unsigned int reg,
	unsigned int mask, unsigned int value)
{
	unsigned int addr = PALMAS_BASE_TO_REG(PALMAS_PMU_CONTROL_BASE, reg);

	return regmap_update_bits(palmas->regmap[0], addr, mask, value);
}

int palmas_ext_power_req_config(struct palmas *palmas,
		int id, int ext_pwr_ctrl, bool enable)
{
	int preq_mask_bit = 0;
	int ret;
	int base_reg = 0;
	int bit_pos;

	if (!(ext_pwr_ctrl & EXT_PWR_REQ))
		return 0;

	if (id >= PALMAS_EXTERNAL_REQSTR_ID_MAX)
		return 0;

	if (palmas->id == TPS80036 && id == PALMAS_EXTERNAL_REQSTR_ID_REGEN3)
		return 0;

	if (palmas->id != TPS80036 && id > PALMAS_EXTERNAL_REQSTR_ID_LDO14)
		return 0;

	if (ext_pwr_ctrl & PALMAS_EXT_CONTROL_NSLEEP) {
		if (id <= PALMAS_EXTERNAL_REQSTR_ID_REGEN4)
			base_reg = PALMAS_NSLEEP_RES_ASSIGN;
		else
			base_reg = PALMAS_NSLEEP_RES_ASSIGN2;
		preq_mask_bit = 0;
	} else if (ext_pwr_ctrl & PALMAS_EXT_CONTROL_ENABLE1) {
		if (id <= PALMAS_EXTERNAL_REQSTR_ID_REGEN4)
			base_reg = PALMAS_ENABLE1_RES_ASSIGN;
		else
			base_reg = PALMAS_ENABLE1_RES_ASSIGN2;
		preq_mask_bit = 1;
	} else if (ext_pwr_ctrl & PALMAS_EXT_CONTROL_ENABLE2) {
		if (id <= PALMAS_EXTERNAL_REQSTR_ID_REGEN4)
			base_reg = PALMAS_ENABLE2_RES_ASSIGN;
		else
			base_reg = PALMAS_ENABLE2_RES_ASSIGN2;
		preq_mask_bit = 2;
	}

	bit_pos = sleep_reqt_info[id].bit_pos;
	base_reg += sleep_reqt_info[id].reg_offset;
	if (enable)
		ret = palmas_resource_update(palmas, base_reg,
				BIT(bit_pos), BIT(bit_pos));
	else
		ret = palmas_resource_update(palmas, base_reg,
				BIT(bit_pos), 0);
	if (ret < 0) {
		dev_err(palmas->dev, "Update on resource reg failed\n");
		return ret;
	}

	/* Unmask the PREQ */
	ret = palmas_control_update(palmas, PALMAS_POWER_CTRL,
				BIT(preq_mask_bit), 0);
	if (ret < 0) {
		dev_err(palmas->dev, "Power control register update fails\n");
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(palmas_ext_power_req_config);

static void palmas_init_ext_control(struct palmas *palmas)
{
	int ret;
	int i;

	/* Clear all external control for this rail */
	for (i = 0; i < 12; ++i) {
		ret = palmas_resource_write(palmas,
				PALMAS_NSLEEP_RES_ASSIGN + i, 0);
		if (ret < 0)
			dev_err(palmas->dev,
				"Error in clearing res assign register\n");
	}

	/* Mask the PREQ */
	ret = palmas_control_update(palmas, PALMAS_POWER_CTRL, 0x7, 0x7);
	if (ret < 0)
		dev_err(palmas->dev, "Power control reg write failed\n");
}
static int palmas_set_pdata_irq_flag(struct i2c_client *i2c,
		struct palmas_platform_data *pdata)
{
	struct irq_data *irq_data = irq_get_irq_data(i2c->irq);
	if (!irq_data) {
		dev_err(&i2c->dev, "Invalid IRQ: %d\n", i2c->irq);
		return -EINVAL;
	}

	pdata->irq_flags = irqd_get_trigger_type(irq_data);
	dev_info(&i2c->dev, "Irq flag is 0x%08x\n", pdata->irq_flags);
	return 0;
}

static int palmas_read_version_information(struct palmas *palmas)
{
	unsigned int sw_rev, des_rev;
	int ret;

	ret = palmas_read(palmas, PALMAS_PMU_CONTROL_BASE,
				PALMAS_SW_REVISION, &sw_rev);
	if (ret < 0) {
		dev_err(palmas->dev, "SW_REVISION read failed: %d\n", ret);
		return ret;
	}

	ret = palmas_read(palmas, PALMAS_PAGE3_BASE,
				PALMAS_INTERNAL_DESIGNREV, &des_rev);
	if (ret < 0) {
		dev_err(palmas->dev,
			"INTERNAL_DESIGNREV read failed: %d\n", ret);
		return ret;
	}

	palmas->sw_otp_version = sw_rev;

	dev_info(palmas->dev, "Internal DesignRev 0x%02X, SWRev 0x%02X\n",
			des_rev, sw_rev);
	des_rev = PALMAS_INTERNAL_DESIGNREV_DESIGNREV(des_rev);
	switch (des_rev) {
	case 0:
		palmas->es_major_version = 1;
		palmas->es_minor_version = 0;
		palmas->design_revision = 0xA0;
		break;
	case 1:
		palmas->es_major_version = 2;
		palmas->es_minor_version = 0;
		palmas->design_revision = 0xB0;
		break;
	case 2:
		palmas->es_major_version = 2;
		palmas->es_minor_version = 1;
		palmas->design_revision = 0xB1;
		break;
	case 3:
		palmas->es_major_version = 2;
		palmas->es_minor_version = 2;
		palmas->design_revision = 0xB2;
		break;
	case 4:
		palmas->es_major_version = 2;
		palmas->es_minor_version = 3;
		palmas->design_revision = 0xB3;
		break;
	default:
		dev_err(palmas->dev, "Invalid design revision\n");
		return -EINVAL;
	}

	dev_info(palmas->dev, "ES version %d.%d: ChipRevision 0x%02X%02X\n",
		palmas->es_major_version, palmas->es_minor_version,
		palmas->design_revision, palmas->sw_otp_version);
	return 0;
}

static void palmas_dt_to_pdata(struct i2c_client *i2c,
		struct palmas_platform_data *pdata)
{
	if (i2c->irq)
		palmas_set_pdata_irq_flag(i2c, pdata);
}

static const struct of_device_id of_palmas_match_tbl[] = {
	{ .compatible = "ti,palmas", .data = (void *)PALMAS, },
	{ .compatible = "ti,tps80036", .data = (void *)TPS80036, },
	{ .compatible = "ti,twl6035", .data = (void *)TWL6035, },
	{ .compatible = "ti,twl6037", .data = (void *)TWL6037, },
	{ .compatible = "ti,tps65913", .data = (void *)TPS65913, },
	{ },
};
MODULE_DEVICE_TABLE(of, of_palmas_match_tbl);

static int palmas_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct palmas *palmas;
	struct palmas_platform_data *pdata;
	struct device_node *node = i2c->dev.of_node;
	int ret = 0, i;
	unsigned int reg, addr;
	int slave;
	struct mfd_cell *children;
	int child_count;
	const struct of_device_id *match;

	pdata = dev_get_platdata(&i2c->dev);

	if (node && !pdata) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(*pdata), GFP_KERNEL);

		if (!pdata)
			return -ENOMEM;

		palmas_dt_to_pdata(i2c, pdata);
	}

	if (!pdata)
		return -EINVAL;

	palmas = devm_kzalloc(&i2c->dev, sizeof(struct palmas), GFP_KERNEL);
	if (palmas == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, palmas);
	palmas->dev = &i2c->dev;
	palmas->irq = i2c->irq;

	if (node) {
		match = of_match_device(of_match_ptr(of_palmas_match_tbl),
				&i2c->dev);
		if (!match)
			return -ENODATA;
		palmas->id = (unsigned int)match->data;
	} else {
		palmas->id = id->driver_data;
	}

	palmas->submodule_lists = submodule_lists[palmas->id];
	mutex_init(&palmas->mutex_config0);
	palmas_regmap_config[0].lock_arg = palmas;

	for (i = 0; i < PALMAS_NUM_CLIENTS; i++) {
		if (i == 0)
			palmas->i2c_clients[i] = i2c;
		else {
			palmas->i2c_clients[i] =
					i2c_new_dummy(i2c->adapter,
						palmas_i2c_ids[i]);
			if (!palmas->i2c_clients[i]) {
				dev_err(palmas->dev,
					"can't attach client %d\n", i);
				ret = -ENOMEM;
				goto free_i2c_client;
			}
			if (node)
				palmas->i2c_clients[i]->dev.of_node = of_node_get(node);
		}
		palmas->regmap[i] = devm_regmap_init_i2c(palmas->i2c_clients[i],
				(const struct regmap_config *)&palmas_regmap_config[i]);
		if (IS_ERR(palmas->regmap[i])) {
			ret = PTR_ERR(palmas->regmap[i]);
			dev_err(palmas->dev,
				"Failed to allocate regmap %d, err: %d\n",
				i, ret);
			goto free_i2c_client;
		}
	}

	ret = palmas_read_version_information(palmas);
	if (ret < 0)
		goto free_i2c_client;

	/* Change interrupt line output polarity */
	if (pdata->irq_flags & IRQ_TYPE_LEVEL_HIGH)
		reg = PALMAS_POLARITY_CTRL_INT_POLARITY;
	else
		reg = 0;
	ret = palmas_update_bits(palmas, PALMAS_PU_PD_OD_BASE,
			PALMAS_POLARITY_CTRL, PALMAS_POLARITY_CTRL_INT_POLARITY,
			reg);
	if (ret < 0) {
		dev_err(palmas->dev, "POLARITY_CTRL updat failed: %d\n", ret);
		goto free_i2c_client;
	}

	/* Change IRQ into clear on read mode for efficiency */
	slave = PALMAS_BASE_TO_SLAVE(PALMAS_INTERRUPT_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT_CTRL);
	reg = PALMAS_INT_CTRL_INT_CLEAR;

	regmap_write(palmas->regmap[slave], addr, reg);

	ret = palmas_add_irq_chip(palmas, palmas->irq,
			IRQF_ONESHOT | pdata->irq_flags, pdata->irq_base,
			&palmas->irq_chip_data);
	if (ret < 0)
		goto free_i2c_client;

	if (palmas->id != TPS80036)
		palmas->ngpio = 8;
	else
		palmas->ngpio = 16;

	reg = pdata->power_ctrl;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_PMU_CONTROL_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_PMU_CONTROL_BASE, PALMAS_POWER_CTRL);

	ret = regmap_write(palmas->regmap[slave], addr, reg);
	if (ret)
		goto free_irq;

	/*
	 * If we are probing with DT do this the DT way and return here
	 * otherwise continue and add devices using mfd helpers.
	 */
	if (node) {
		ret = of_platform_populate(node, NULL, NULL, &i2c->dev);
		if (ret < 0)
			goto free_irq;
		else
			return ret;
	}

	/*
	 * Programming the Long-Press shutdown delay register.
	 * Using "slave" from previous assignment as this register
	 * too belongs to PALMAS_PMU_CONTROL_BASE block.
	 */
	if (pdata->long_press_delay != PALMAS_LONG_PRESS_KEY_TIME_DEFAULT) {
		ret = palmas_update_bits(palmas, PALMAS_PMU_CONTROL_BASE,
					PALMAS_LONG_PRESS_KEY,
					PALMAS_LONG_PRESS_KEY_LPK_TIME_MASK,
					pdata->long_press_delay <<
					PALMAS_LONG_PRESS_KEY_LPK_TIME_SHIFT);
		if (ret) {
			dev_err(palmas->dev,
				"Failed to update palmas long press delay"
				"(hard shutdown delay), err: %d\n", ret);
			goto free_irq;
		}
	}

	palmas_init_ext_control(palmas);


	/*
	 * If we are probing with DT do this the DT way and return here
	 * otherwise continue and add devices using mfd helpers.
	 */
	if (node) {
		ret = of_platform_populate(node, NULL, NULL, &i2c->dev);
		if (ret < 0) {
			dev_err(palmas->dev,
				"Couldn't populate Palmas devices, %d\n", ret);
			goto free_irq;
		}
	} else {
		children = kzalloc(sizeof(*children) * ARRAY_SIZE(palmas_children),
				GFP_KERNEL);
		if (!children) {
			ret = -ENOMEM;
			goto free_irq;
		}

		child_count = 0;
		for (i = 0; i< ARRAY_SIZE(palmas_children); ++i) {
			if (palmas->submodule_lists & BIT(palmas_children[i].id))
				children[child_count++] = palmas_children[i];
		}

		children[PALMAS_PMIC_ID].platform_data = pdata->pmic_pdata;
		children[PALMAS_PMIC_ID].pdata_size = sizeof(*pdata->pmic_pdata);

		children[PALMAS_GPADC_ID].platform_data = pdata->gpadc_pdata;
		children[PALMAS_GPADC_ID].pdata_size = sizeof(*pdata->gpadc_pdata);

		children[PALMAS_RESOURCE_ID].platform_data = pdata->resource_pdata;
		children[PALMAS_RESOURCE_ID].pdata_size =
				sizeof(*pdata->resource_pdata);

		children[PALMAS_USB_ID].platform_data = pdata->usb_pdata;
		children[PALMAS_USB_ID].pdata_size = sizeof(*pdata->usb_pdata);

		children[PALMAS_CLK_ID].platform_data = pdata->clk_pdata;
		children[PALMAS_CLK_ID].pdata_size = sizeof(*pdata->clk_pdata);

		ret = mfd_add_devices(palmas->dev, -1,
					  children, child_count,
					  NULL, palmas->irq_chip_data->irq_base,
					  palmas->irq_chip_data->domain);
		kfree(children);

		if (ret < 0)
			goto free_irq;
	}

	if (pdata->auto_ldousb_en)
		/* VBUS detection enables the LDOUSB */
		palmas_control_update(palmas, PALMAS_EXT_CHRG_CTRL, 1,
					PALMAS_EXT_CHRG_CTRL_AUTO_LDOUSB_EN);

	return 0;

free_irq:
		palmas_del_irq_chip(palmas->irq, palmas->irq_chip_data);
free_i2c_client:
		for (i = 1; i < PALMAS_NUM_CLIENTS; i++) {
		if (palmas->i2c_clients[i])
			i2c_unregister_device(palmas->i2c_clients[i]);
	}
	return ret;
}

static int palmas_i2c_remove(struct i2c_client *i2c)
{
	struct palmas *palmas = i2c_get_clientdata(i2c);
	int i;

	if (!i2c->dev.of_node)
		mfd_remove_devices(palmas->dev);

	palmas_del_irq_chip(palmas->irq, palmas->irq_chip_data);

	for (i = 1; i < PALMAS_NUM_CLIENTS; i++) {
		if (palmas->i2c_clients[i])
			i2c_unregister_device(palmas->i2c_clients[i]);
	}

	return 0;
}

static void palmas_i2c_shutdown(struct i2c_client *i2c)
{
	struct palmas *palmas = i2c_get_clientdata(i2c);

	palmas->shutdown = true;
}

static const struct i2c_device_id palmas_i2c_id[] = {
	{ "palmas", PALMAS},
	{ "twl6035", TWL6035},
	{ "twl6037", TWL6037},
	{ "tps65913", TPS65913},
	{ "tps80036", TPS80036},
	{ /* end */ }
};
MODULE_DEVICE_TABLE(i2c, palmas_i2c_id);

static struct i2c_driver palmas_i2c_driver = {
	.driver = {
		   .name = "palmas",
		   .of_match_table = of_palmas_match_tbl,
		   .owner = THIS_MODULE,
	},
	.probe = palmas_i2c_probe,
	.remove = palmas_i2c_remove,
	.shutdown = palmas_i2c_shutdown,
	.id_table = palmas_i2c_id,
};

static int __init palmas_i2c_init(void)
{
	return i2c_add_driver(&palmas_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(palmas_i2c_init);

static void __exit palmas_i2c_exit(void)
{
	i2c_del_driver(&palmas_i2c_driver);
}
module_exit(palmas_i2c_exit);

MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_DESCRIPTION("Palmas chip family multi-function driver");
MODULE_LICENSE("GPL");
