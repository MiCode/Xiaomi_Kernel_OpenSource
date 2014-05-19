/*
 * intel_soc_pmic_crc.c - Device access for Crystal Cove PMIC
 *
 * Copyright (C) 2013, 2014 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: Yang, Bin <bin.yang@intel.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/acpi.h>
#include <linux/version.h>
#include <linux/mfd/intel_soc_pmic.h>
#include "intel_soc_pmic_core.h"

#define CRYSTAL_COVE_IRQ_NUM	7

#define CHIPID		0x00
#define CHIPVER		0x01
#define IRQLVL1		0x02
#define MIRQLVL1	0x0E
enum {
	PWRSRC_IRQ = 0,
	THRM_IRQ,
	BCU_IRQ,
	ADC_IRQ,
	CHGR_IRQ,
	GPIO_IRQ,
	VHDMIOCP_IRQ
};

static struct resource gpio_resources[] = {
	{
		.name	= "GPIO",
		.start	= GPIO_IRQ,
		.end	= GPIO_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource pwrsrc_resources[] = {
	{
		.name  = "PWRSRC",
		.start = PWRSRC_IRQ,
		.end   = PWRSRC_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource adc_resources[] = {
	{
		.name  = "ADC",
		.start = ADC_IRQ,
		.end   = ADC_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource thermal_resources[] = {
	{
		.name  = "THERMAL",
		.start = THRM_IRQ,
		.end   = THRM_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};
static struct resource bcu_resources[] = {
	{
		.name  = "BCU",
		.start = BCU_IRQ,
		.end   = BCU_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};
static struct mfd_cell crystal_cove_dev[] = {
	{
		.name = "crystal_cove_pwrsrc",
		.id = 0,
		.num_resources = ARRAY_SIZE(pwrsrc_resources),
		.resources = pwrsrc_resources,
	},
	{
		.name = "crystal_cove_adc",
		.id = 0,
		.num_resources = ARRAY_SIZE(adc_resources),
		.resources = adc_resources,
	},
	{
		.name = "crystal_cove_thermal",
		.id = 0,
		.num_resources = ARRAY_SIZE(thermal_resources),
		.resources = thermal_resources,
	},
	{
		.name = "crystal_cove_bcu",
		.id = 0,
		.num_resources = ARRAY_SIZE(bcu_resources),
		.resources = bcu_resources,
	},
	{
		.name = "crystal_cove_gpio",
		.id = 0,
		.num_resources = ARRAY_SIZE(gpio_resources),
		.resources = gpio_resources,
	},
	{NULL, },
};

#define	CRC_IRQREGMAP_VALUE(irq)	{	\
		{MIRQLVL1, irq, 1, 0},		\
		{IRQLVL1,  irq, 1, 0},		\
		INTEL_PMIC_REG_NULL,		\
	}

struct intel_pmic_irqregmap crystal_cove_irqregmap[] = {
	[PWRSRC_IRQ]	= CRC_IRQREGMAP_VALUE(PWRSRC_IRQ),
	[THRM_IRQ]	= CRC_IRQREGMAP_VALUE(THRM_IRQ),
	[BCU_IRQ]	= CRC_IRQREGMAP_VALUE(BCU_IRQ),
	[ADC_IRQ]	= CRC_IRQREGMAP_VALUE(ADC_IRQ),
	[CHGR_IRQ]	= CRC_IRQREGMAP_VALUE(CHGR_IRQ),
	[GPIO_IRQ]	= CRC_IRQREGMAP_VALUE(GPIO_IRQ),
	[VHDMIOCP_IRQ]	= CRC_IRQREGMAP_VALUE(VHDMIOCP_IRQ),
};

static int crystal_cove_init(void)
{
	pr_debug("Crystal Cove: ID 0x%02X, VERSION 0x%02X\n",
		 intel_soc_pmic_readb(CHIPID), intel_soc_pmic_readb(CHIPVER));
	return 0;
}

struct intel_soc_pmic crystal_cove_pmic = {
	.label		= "crystal cove",
	.irq_flags	= IRQF_TRIGGER_RISING | IRQF_ONESHOT,
	.init		= crystal_cove_init,
	.cell_dev	= crystal_cove_dev,
	.irq_regmap	= crystal_cove_irqregmap,
	.irq_num	= CRYSTAL_COVE_IRQ_NUM,
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yang, Bin <bin.yang@intel.com");
