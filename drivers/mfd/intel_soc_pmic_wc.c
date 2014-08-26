/*
 * Whiskey Cove  --  Device access for Intel WhiskeyCove PMIC
 *
 * Copyright (C) 2013, 2014 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * Author: Yang Bin <bin.yang@intel.com>
 * Author: Kannappan <r.kannappan@intel.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/acpi.h>
#include <linux/version.h>
#include "./intel_soc_pmic_core.h"

#define WHISKEY_COVE_IRQ_NUM	17

#define CHIPID		0x00
#define CHIPVER	0x01

#define IRQLVL1	0x02
#define PWRSRCIRQ	0x03
#define THRM0IRQ	0x04
#define THRM1IRQ	0x05
#define THRM2IRQ	0x06
#define BCUIRQ		0x07
#define THRM3IRQ	0xD9
#define CHGRIRQ	0x0A

#define MIRQLVL1	0x0E
#define MPWRSRCIRQ	0x0F
#define MTHRMIRQ0	0x0D
#define MTHRMIRQ1	0x12
#define MTHRMIRQ2	0x13
#define MTHRMIRQ3	0xDA
#define MCHGRIRQ	0x17

enum {
	PWRSRC_LVL1 = 0,
	THRM_LVL1,
	BCU_IRQ,
	ADC_IRQ,
	CHGR_LVL1,
	GPIO_IRQ,
	CRIT_IRQ = 7,
	PWRSRC_IRQ,
	THRM1_IRQ,
	BATALRT_IRQ,
	BATZC_IRQ,
	CHGR_IRQ,
	THRM0_IRQ,
	PMICI2C_IRQ,
	THRM3_IRQ,
	CTYPE_IRQ,
};

struct intel_soc_pmic whiskey_cove_pmic;

static struct resource gpio_resources[] = {
	{
		.name	= "GPIO",
		.start	= GPIO_IRQ,
		.end	= GPIO_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource pmic_ccsm_resources[] = {
	{
		.start = PWRSRC_IRQ,
		.end   = PWRSRC_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = BATZC_IRQ,
		.end   = BATZC_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = BATALRT_IRQ,
		.end   = BATALRT_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = CTYPE_IRQ,
		.end   = CTYPE_IRQ,
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

static struct resource charger_resources[] = {
	{
		.name  = "CHARGER",
		.start = CHGR_IRQ,
		.end   = CHGR_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource pmic_i2c_resources[] = {
	{
		.name  = "PMIC_I2C",
		.start = PMICI2C_IRQ,
		.end   = PMICI2C_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource thermal_resources[] = {
	{
		.start = THRM0_IRQ,
		.end   = THRM0_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = THRM1_IRQ,
		.end   = THRM1_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = THRM3_IRQ,
		.end   = THRM3_IRQ,
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

static struct mfd_cell whiskey_cove_dev[] = {
	{
		.name = "whiskey_cove_adc",
		.id = 0,
		.num_resources = ARRAY_SIZE(adc_resources),
		.resources = adc_resources,
	},
	{
		.name = "whiskey_cove_thermal",
		.id = 0,
		.num_resources = ARRAY_SIZE(thermal_resources),
		.resources = thermal_resources,
	},
	{
		.name = "pmic_ccsm",
		.id = 0,
		.num_resources = ARRAY_SIZE(pmic_ccsm_resources),
		.resources = pmic_ccsm_resources,
	},
	{
		.name = "i2c_pmic_adap",
		.id = 0,
		.num_resources = ARRAY_SIZE(pmic_i2c_resources),
		.resources = pmic_i2c_resources,
	},
	{
		.name = "bd71621",
		.id = 0,
		.num_resources = ARRAY_SIZE(charger_resources),
		.resources = charger_resources,
	},
	{
		.name = "whiskey_cove_bcu",
		.id = 0,
		.num_resources = ARRAY_SIZE(bcu_resources),
		.resources = bcu_resources,
	},
	{
		.name = "whiskey_cove_gpio",
		.id = 0,
		.num_resources = ARRAY_SIZE(gpio_resources),
		.resources = gpio_resources,
	},
	{
		.name = "sw_fuel_gauge",
		.id = 0,
		.num_resources = 0,
		.resources = NULL,
	},
	{
		.name = "sw_fuel_gauge_ha",
		.id = 0,
		.num_resources = 0,
		.resources = NULL,
	},
	{NULL, },
};

struct intel_pmic_irqregmap whiskey_cove_irqregmap[] = {
	{
		{MIRQLVL1, PWRSRC_LVL1, 1, 0},
		{IRQLVL1, PWRSRC_LVL1, 1, 0},
		INTEL_PMIC_REG_NULL,
	},
	{
		{MIRQLVL1, THRM_LVL1, 1, 0},
		{IRQLVL1, THRM_LVL1, 1, 0},
		INTEL_PMIC_REG_NULL,
	},
	{
		{MIRQLVL1, BCU_IRQ, 1, 0},
		{BCUIRQ, 0, 7, 0},
		{BCUIRQ, 0, 7, 0},
	},
	{
		{MIRQLVL1, ADC_IRQ, 1, 0},
		{IRQLVL1, ADC_IRQ, 1, 0},
		INTEL_PMIC_REG_NULL,
	},
	{
		{MIRQLVL1, CHGR_LVL1, 1, 0},
		{IRQLVL1, CHGR_LVL1, 1, 0},
		INTEL_PMIC_REG_NULL,
	},
	{
		{MIRQLVL1, GPIO_IRQ, 1, 0},
		{IRQLVL1, GPIO_IRQ, 1, 0},
		INTEL_PMIC_REG_NULL,
	},
	{
		INTEL_PMIC_REG_NULL,
		INTEL_PMIC_REG_NULL,
		INTEL_PMIC_REG_NULL,
	},
	{
		{MIRQLVL1, CRIT_IRQ, 1, 0},
		{IRQLVL1, CRIT_IRQ, 1, 0},
		INTEL_PMIC_REG_NULL,
	},
	{
		{MIRQLVL1, 0, 0x1, 0},
		{PWRSRCIRQ, 0, 0x1F, INTEL_PMIC_REG_W1C},
		{PWRSRCIRQ, 0, 0x1F, INTEL_PMIC_REG_W1C},
	},
	{ /* THERM1 IRQ */
		{MIRQLVL1, 1, 0x1, 0},
		{THRM1IRQ, 0, 0xF, INTEL_PMIC_REG_W1C},
		{THRM1IRQ, 0, 0xF, INTEL_PMIC_REG_W1C},
	},
	{ /* THERM2 */
		{MIRQLVL1, 1, 0x1, 0},
		{THRM2IRQ, 0, 0xC3, INTEL_PMIC_REG_W1C},
		{THRM2IRQ, 0, 0xC3, INTEL_PMIC_REG_W1C},
	},
	{ /* BATZONE CHANGED */
		{MIRQLVL1, 1, 0x1, 0},
		{THRM1IRQ, 7, 1, INTEL_PMIC_REG_W1C},
		{THRM1IRQ, 7, 1, INTEL_PMIC_REG_W1C},
	},
	{ /* Ext. Chrgr */
		{MIRQLVL1, 4, 0x1, 0},
		{CHGRIRQ, 0, 1, INTEL_PMIC_REG_W1C},
		{CHGRIRQ, 0, 1, INTEL_PMIC_REG_W1C},
	},
	{ /* THERM0 IRQ */
		{MIRQLVL1, 1, 0x1, 0},
		{THRM0IRQ, 0, 0xFF, INTEL_PMIC_REG_W1C},
		{THRM0IRQ, 0, 0xFF, INTEL_PMIC_REG_W1C},
	},
	{ /* External I2C Transaction */
		{MIRQLVL1, 4, 0x1, 0},
		{CHGRIRQ, 1, 7, INTEL_PMIC_REG_W1C},
		{CHGRIRQ, 1, 7, INTEL_PMIC_REG_W1C},
	},
	{ /* THERM3 */
		{MIRQLVL1, 1, 0x1, 0},
		{THRM3IRQ, 0, 0xF0, INTEL_PMIC_REG_W1C},
		{THRM3IRQ, 0, 0xF0, INTEL_PMIC_REG_W1C},
	},
	{ /* CTYP */
		{MIRQLVL1, 4, 0x1, 0},
		{CHGRIRQ, 4, 1, INTEL_PMIC_REG_W1C},
		{CHGRIRQ, 4, 1, INTEL_PMIC_REG_W1C},
	},
};

static int whiskey_cove_init(void)
{
	pr_info("Whiskey Cove: ID 0x%02X, VERSION 0x%02X\n",
		intel_soc_pmic_readb(CHIPID), intel_soc_pmic_readb(CHIPVER));
	return 0;
}

struct intel_soc_pmic whiskey_cove_pmic = {
	.label		= "whiskey cove",
	.irq_flags	= IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
	.init		= whiskey_cove_init,
	.cell_dev	= whiskey_cove_dev,
	.irq_regmap	= whiskey_cove_irqregmap,
	.irq_num	= WHISKEY_COVE_IRQ_NUM,
};

MODULE_LICENSE("GPL V2");
MODULE_AUTHOR("Yang Bin <bin.yang@intel.com");

