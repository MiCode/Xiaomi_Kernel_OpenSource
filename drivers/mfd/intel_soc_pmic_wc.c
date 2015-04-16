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
#include <asm/intel_wcove_bcu.h>
#include <linux/iio/intel_wcove_gpadc.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/types.h>
#include <linux/power/intel_pmic_ccsm.h>
#include <linux/mfd/intel_soc_pmic_wcove.h>
#include <asm/intel_em_config.h>
#include "./intel_soc_pmic_core.h"
#include <linux/regulator/intel_whiskey_cove_pmic.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/lnw_gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/platform_device.h>


#define WHISKEY_COVE_IRQ_NUM	17

#define CHIPID		0x00
#define CHIPVER	0x01

#define IRQLVL1	0x02
#define PWRSRCIRQ	0x03
#define THRM0IRQ	0x04
#define THRM1IRQ	0x05
#define THRM2IRQ	0x06
#define BCUIRQ		0x07
#define ADCIRQ		0x08
#define THRM3IRQ	0xD9
#define CHGRIRQ		0x0A
#define CRITIRQ		0x37

#define MIRQLVL1	0x0E
#define MPWRSRCIRQ	0x0F
#define MTHRMIRQ0	0x0D
#define MTHRMIRQ1	0x12
#define MTHRMIRQ2	0x13
#define MTHRMIRQ3	0xDA
#define MCHGRIRQ	0x17

#define STHRMIRQ0	0x4F19
#define STHRMIRQ1	0x4F1A
#define STHRMIRQ2	0x4F1B

#define FPO0_USB_COMP_OFFSET 0x01

static bool wcove_init_done;

static struct gpadc_regmap_t whiskeycove_gpadc_regmaps[GPADC_NUM_CHANNELS] = {
	{"VBAT",	5, 0,	0x4F03, 0x4F04, 0xFF, 0xFF, 0xFF, 0xFF},
	{"BATID",	4, 1,	0x4F06, 0x4F07, 0xFF, 0xFF, 0xFF, 0xFF},
	{"PMICTEMP",	3, 2,	0x4F42,	0x4F43, 0x4F33, 0x4F34, 0x4F33, 0x4F34},
	{"BATTEMP0",	2, 3,	0x4F15, 0x4F16, 0xFF, 0xFF, 0xFF, 0xFF},
	{"BATTEMP1",	2, 3,	0x4F17, 0x4F18, 0xFF, 0xFF, 0xFF, 0xFF},
	{"SYSTEMP0",	3, 2,	0x4F38, 0x4F39, 0x4F23, 0x4F24, 0x4F25, 0x4F26},
	{"SYSTEMP1",	3, 2,	0x4F3A, 0x4F3B, 0x4F27, 0x4F28, 0x4F29, 0x4F2A},
	{"SYSTEMP2",	3, 2,	0x4F3C, 0x4F3D, 0x4F2B, 0x4F2C, 0x4F2D, 0x4F2E},
	{"USBID",	1, 5,	0x4F08, 0x4F09, 0xFF, 0xFF, 0xFF, 0xFF},
	{"PEAK",	7, 6,	0x4F13, 0x4F14, 0xFF, 0xFF, 0xFF, 0xFF},
	{"AGND",	6, 4,	0x4F0A, 0x4F0B, 0xFF, 0xFF, 0xFF, 0xFF},
	{"VREF",	6, 4,	0x4F0A, 0x4F0B, 0xFF, 0xFF, 0xFF, 0xFF},
};

static struct gpadc_regs_t whiskeycove_gpadc_regs = {
	.gpadcreq	=	0x4F02,
	.gpadcreq_irqen	=	0,
	.gpadcreq_busy	=	(1 << 0),
	.mirqlvl1	=	0x6e0E,
	.mirqlvl1_adc	=	(1 << 3),
	.adc1cntl	=	0x4F05,
	.adcirq		=	0x6E08,
	.madcirq	=	0x6E15,
	.thrmmonctl	=	0x4F1E,
	.batthermonctl =	0x4F1F,
	.vbatmonctl	=	0x4F20,
	.gpmonctl	=	0x4F21,
};

#define MSIC_ADC_MAP(_adc_channel_label,			\
		     _consumer_dev_name,                        \
		     _consumer_channel)                         \
	{                                                       \
		.adc_channel_label = _adc_channel_label,        \
		.consumer_dev_name = _consumer_dev_name,        \
		.consumer_channel = _consumer_channel,          \
	}

static struct iio_map wc_iio_maps[] = {
	MSIC_ADC_MAP("CH0", "VIBAT", "VBAT"),
	MSIC_ADC_MAP("CH1", "BATID", "BATID"),
	MSIC_ADC_MAP("CH2", "PMICTEMP", "PMICTEMP"),
	MSIC_ADC_MAP("CH3", "BATTEMP", "BATTEMP0"),
	MSIC_ADC_MAP("CH4", "BATTEMP", "BATTEMP1"),
	MSIC_ADC_MAP("CH5", "SYSTEMP", "SYSTEMP0"),
	MSIC_ADC_MAP("CH6", "SYSTEMP", "SYSTEMP1"),
	MSIC_ADC_MAP("CH7", "SYSTEMP", "SYSTEMP2"),
	MSIC_ADC_MAP("CH8", "USBID", "USBID"),
	MSIC_ADC_MAP("CH9", "PEAK", "PEAK"),
	MSIC_ADC_MAP("CH10", "GPMEAS", "AGND"),
	MSIC_ADC_MAP("CH11", "GPMEAS", "VREF"),
	{ },
};

#define MSIC_ADC_CHANNEL(_type, _channel, _datasheet_name) \
	{                               \
		.indexed = 1,           \
		.type = _type,          \
		.channel = _channel,    \
		.datasheet_name = _datasheet_name,      \
	}

static const struct iio_chan_spec const wc_adc_channels[] = {
	MSIC_ADC_CHANNEL(IIO_VOLTAGE, 0, "CH0"),
	MSIC_ADC_CHANNEL(IIO_VOLTAGE, 1, "CH1"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 2, "CH2"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 3, "CH3"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 4, "CH4"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 5, "CH5"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 6, "CH6"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 7, "CH7"),
	MSIC_ADC_CHANNEL(IIO_VOLTAGE, 8, "CH8"),
	MSIC_ADC_CHANNEL(IIO_VOLTAGE, 9, "CH9"),
	MSIC_ADC_CHANNEL(IIO_VOLTAGE, 10, "CH10"),
	MSIC_ADC_CHANNEL(IIO_VOLTAGE, 11, "CH11"),
};

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

static struct pmic_regs pmic_wcove_regmap = {
	.pmic_id = 0x00,
	.pmic_irqlvl1 = WC_IRQLVL1_ADDR,
	.pmic_mirqlvl1 = WC_IRQLVL1_MASK_ADDR,
	.pmic_chgrirq0 = WC_CHGRIRQ0_ADDR,
	.pmic_schgrirq0 = WC_SCHGRIRQ0_ADDR,
	.pmic_mchgrirq0 = WC_MCHGRIRQ0_ADDR,
	.pmic_chgrirq1 = WC_PWRSRC_ADDR,
	.pmic_schgrirq1 = WC_SPWRSRC_ADDR,
	.pmic_mchgrirq1 = WC_MPWRSRC_ADDR,
	.pmic_chgrctrl0 = WC_CHGRCTRL0_ADDR,
	.pmic_chgrctrl1 = WC_CHGRCTRL1_ADDR,
	.pmic_chgdisctrl = WC_CHGDISCTRL_ADDR,
	.pmic_lowbattdet0 = WC_LOWBATTDET0_ADDR,
	.pmic_lowbattdet1 = WC_LOWBATTDET1_ADDR,
	.pmic_battdetctrl = WC_BATTDETCTRL_ADDR,
	.pmic_vbusdetctrl = WC_VBUSDETCTRL_ADDR,
	.pmic_vdcindetctrl = WC_VDCINDETCTRL_ADDR,
	.pmic_chgrstatus = WC_CHGRSTATUS_ADDR,
	.pmic_usbidctrl = WC_USBIDCTRL_ADDR,
	.pmic_usbidstat = WC_USBIDSTAT_ADDR,
	.pmic_wakesrc = WC_WAKESRC_ADDR,
	.pmic_usbphyctrl = WC_USBPHYCTRL_ADDR,
	.pmic_dbg_usbbc1 = WC_DBGUSBBC1_ADDR,
	.pmic_dbg_usbbc2 = WC_DBGUSBBC2_ADDR,
	.pmic_dbg_usbbcstat = WC_DBGUSBBCSTAT_ADDR,
	.pmic_usbpath = WC_USBPATH_ADDR,
	.pmic_usbsrcdetstat = WC_USBSRCDETSTATUS_ADDR,
	.pmic_chrttaddr = WC_CHRTTADDR_ADDR,
	.pmic_chrttdata = WC_CHRTTDATA_ADDR,
	.pmic_thrmbatzone = WC_THRMBATZONE_ADDR,
	.pmic_thrmzn0h = WC_THRMZN0H_ADDR,
	.pmic_thrmzn0l = WC_THRMZN0L_ADDR,
	.pmic_thrmzn1h = WC_THRMZN1H_ADDR,
	.pmic_thrmzn1l = WC_THRMZN1L_ADDR,
	.pmic_thrmzn2h = WC_THRMZN2H_ADDR,
	.pmic_thrmzn2l = WC_THRMZN2L_ADDR,
	.pmic_thrmzn3h = WC_THRMZN3H_ADDR,
	.pmic_thrmzn3l = WC_THRMZN3L_ADDR,
	.pmic_thrmzn4h = WC_THRMZN4H_ADDR,
	.pmic_thrmzn4l = WC_THRMZN4L_ADDR,
	.pmic_thrmirq0 = WC_THRMIRQ0_ADDR,
	.pmic_mthrmirq0 = WC_MTHRMIRQ0_ADDR,
	.pmic_sthrmirq0 = WC_STHRMIRQ0_ADDR,
	.pmic_thrmirq1 = WC_THRMIRQ1_ADDR,
	.pmic_mthrmirq1 = WC_MTHRMIRQ1_ADDR,
	.pmic_sthrmirq1 = WC_STHRMIRQ1_ADDR,
	.pmic_thrmirq2 = WC_THRMIRQ2_ADDR,
	.pmic_mthrmirq2 = WC_MTHRMIRQ2_ADDR,
	.pmic_sthrmirq2 = WC_STHRMIRQ2_ADDR,
};

static struct pmic_ccsm_int_cfg wc_intmap[] = {
	{ PMIC_INT_VBUS,
		WC_PWRSRC_ADDR, WC_MPWRSRC_ADDR,
		WC_SPWRSRC_ADDR, 0x01 },
	{ PMIC_INT_DCIN,
		WC_PWRSRC_ADDR, WC_MPWRSRC_ADDR,
		WC_SPWRSRC_ADDR, 0x02 },
	{ PMIC_INT_BATTDET,
		WC_PWRSRC_ADDR, WC_MPWRSRC_ADDR,
		WC_SPWRSRC_ADDR, 0x04 },
	{ PMIC_INT_USBIDFLTDET,
		WC_PWRSRC_ADDR, WC_MPWRSRC_ADDR,
		WC_SPWRSRC_ADDR, 0x08 },
	{ PMIC_INT_USBIDGNDDET,
		WC_PWRSRC_ADDR, WC_MPWRSRC_ADDR,
		WC_SPWRSRC_ADDR, 0x10 },
	{ PMIC_INT_CTYP,
		WC_CHGRIRQ0_ADDR, WC_SCHGRIRQ0_ADDR,
		WC_MCHGRIRQ0_ADDR, 0x10 },
	{ PMIC_INT_BZIRQ,
		WC_THRMIRQ1_ADDR, WC_MTHRMIRQ1_ADDR,
		WC_STHRMIRQ1_ADDR, 0x80 },
	{ PMIC_INT_BATCRIT,
		WC_THRMIRQ1_ADDR, WC_MTHRMIRQ1_ADDR,
		WC_STHRMIRQ1_ADDR, 0x10 },
	{ PMIC_INT_BAT0ALRT0,
		WC_THRMIRQ2_ADDR, WC_MTHRMIRQ2_ADDR,
		WC_STHRMIRQ2_ADDR, 0x01 },
	{ PMIC_INT_BAT1ALRT0,
		WC_THRMIRQ2_ADDR, WC_MTHRMIRQ2_ADDR,
		WC_STHRMIRQ2_ADDR, 0x02 },
};

static struct wcove_bcu_platform_data wc_bcu_pdata = {
	.config = {
		{VWARNA_CFG_REG,	0xFF},
		{VWARNB_CFG_REG,	0xFC},
		{VCRIT_CFG_REG,		0xFD},
		{ICCMAXVCC_CFG_REG,	0x06},
		{ICCMAXVNN_CFG_REG,	0x06},
		{ICCMAXVGG_CFG_REG,	0x06},
		{BCUDISB_BEH_REG,	0x01},
		{BCUDISCRIT_BEH_REG,	0x01},
		{BCUVSYS_DRP_BEH_REG,	0x00},
		{MBCUIRQ_REG,		0x18},
	},
	.num_regs = MAX_BCUCFG_REGS,
};

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
	{
		.start = BATALRT_IRQ,
		.end   = BATALRT_IRQ,
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
		.name = "wcove_gpadc",
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
		.name = "wcove_ccsm",
		.id = 0,
		.num_resources = ARRAY_SIZE(pmic_ccsm_resources),
		.resources = pmic_ccsm_resources,
	},
	{
		.name = "wcove_pmic_i2c",
		.id = 0,
		.num_resources = ARRAY_SIZE(pmic_i2c_resources),
		.resources = pmic_i2c_resources,
	},
	{
		.name = "ext-charger",
		.id = 0,
		.num_resources = ARRAY_SIZE(charger_resources),
		.resources = charger_resources,
	},
	{
		.name = "wcove_bcu",
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
		.name = "wcove_regulator",
		.id = WCOVE_ID_V3P3SD + 1,
		.num_resources = 0,
		.resources = NULL,
	},
	{
		.name = "wcove_regulator",
		.id = WCOVE_ID_VSDIO + 1,
		.num_resources = 0,
		.resources = NULL,
	},
	{
		.name = "wcove_regulator",
		.id = WCOVE_ID_V1P8SX + 1,
		.num_resources = 0,
		.resources = NULL,
	},
	{
		.name = "wcove_regulator",
		.id = WCOVE_ID_V2P8SX + 1,
		.num_resources = 0,
		.resources = NULL,
	},
	{
		.name = "whiskey_cove_region",
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
		{BCUIRQ, 0, 0x1F, INTEL_PMIC_REG_W1C},
		{BCUIRQ, 0, 0x1F, INTEL_PMIC_REG_W1C},
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
		{IRQLVL1, CRIT_IRQ, 1, INTEL_PMIC_REG_W1C},
		{CRITIRQ, 0, 0x03, INTEL_PMIC_REG_W1C}
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
		{MCHGRIRQ, 0, 0x1, 0},
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

static struct trip_config_map str0_trip_config[] = {
	{
		.irq_reg = THRM0IRQ,
		.irq_mask = 0x01,
		.irq_en = MTHRMIRQ0,
		.irq_en_mask = 0x01,
		.evt_stat = STHRMIRQ0,
		.evt_mask = 0x01,
		.trip_num = 0
	},
	{
		.irq_reg = THRM0IRQ,
		.irq_mask = 0x10,
		.irq_en = MTHRMIRQ0,
		.irq_en_mask = 0x10,
		.evt_stat = STHRMIRQ0,
		.evt_mask = 0x10,
		.trip_num = 1
	}
};

static struct trip_config_map str1_trip_config[] = {
	{
		.irq_reg = THRM0IRQ,
		.irq_mask = 0x02,
		.irq_en = MTHRMIRQ0,
		.irq_en_mask = 0x02,
		.evt_stat = STHRMIRQ0,
		.evt_mask = 0x02,
		.trip_num = 0
	},
	{
		.irq_reg = THRM0IRQ,
		.irq_mask = 0x20,
		.irq_en = MTHRMIRQ0,
		.irq_en_mask = 0x20,
		.evt_stat = STHRMIRQ0,
		.evt_mask = 0x20,
		.trip_num = 1
	},
};

static struct trip_config_map str2_trip_config[] = {
	{
		.irq_reg = THRM0IRQ,
		.irq_mask = 0x04,
		.irq_en = MTHRMIRQ0,
		.irq_en_mask = 0x04,
		.evt_stat = STHRMIRQ0,
		.evt_mask = 0x04,
		.trip_num = 0
	},
	{
		.irq_reg = THRM0IRQ,
		.irq_mask = 0x40,
		.irq_en = MTHRMIRQ0,
		.irq_en_mask = 0x40,
		.evt_stat = STHRMIRQ0,
		.evt_mask = 0x40,
		.trip_num = 1
	},
};

static struct trip_config_map str3_trip_config[] = {
	{
		.irq_reg = THRM2IRQ,
		.irq_mask = 0x01,
		.irq_en = MTHRMIRQ2,
		.irq_en_mask = 0x01,
		.evt_stat = STHRMIRQ2,
		.evt_mask = 0x10,
		.trip_num = 1
	},
};

static struct thermal_irq_map wc_thermal_irq_map[] = {
	{
		.handle = "STR0",
		.trip_config = str0_trip_config,
		.num_trips = ARRAY_SIZE(str0_trip_config),
	},
	{
		.handle = "STR1",
		.trip_config = str1_trip_config,
		.num_trips = ARRAY_SIZE(str1_trip_config),
	},
	{
		.handle = "STR2",
		.trip_config = str2_trip_config,
		.num_trips = ARRAY_SIZE(str2_trip_config),
	},
	{
		.handle = "STR3",
		.trip_config = str3_trip_config,
		.num_trips = ARRAY_SIZE(str3_trip_config),
	},
};

static struct pmic_gpio_data whiskey_cove_gpio_data = {
	.type = WHISKEY_COVE,
	.num_gpio = 10,
	.num_vgpio = 0x5e,
};

static struct pmic_thermal_data whiskey_cove_thermal_data = {
	.maps = wc_thermal_irq_map,
	.num_maps = ARRAY_SIZE(wc_thermal_irq_map),
};

static void wc_set_thermal_pdata(void)
{
	intel_soc_pmic_set_pdata("whiskey_cove_thermal",
				(void *)&whiskey_cove_thermal_data,
				sizeof(whiskey_cove_thermal_data), 0);
}

static struct regulator_consumer_supply v1p8sx_consumer[] = {
	REGULATOR_SUPPLY("v1p8sx", "INT33BE:00"),
	REGULATOR_SUPPLY("v1p8sx", "INT33FB:00"),
};

static struct regulator_consumer_supply v2p8sx_consumer[] = {
	REGULATOR_SUPPLY("v2p8sx", "INT33BE:00"),
	REGULATOR_SUPPLY("v2p8sx", "INT33FB:00"),
};

/* v1p8sx regulator */
static struct regulator_init_data v1p8sx_data = {
	.constraints = {
		.min_uV = 1620000,
		.max_uV = 1980000,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
	},
	.num_consumer_supplies	= ARRAY_SIZE(v1p8sx_consumer),
	.consumer_supplies	= v1p8sx_consumer,
};

/* v2p8sx regulator */
static struct regulator_init_data v2p8sx_data = {
	.constraints = {
		.min_uV			= 2565000,
		.max_uV			= 3300000,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
	},
	.num_consumer_supplies	= ARRAY_SIZE(v2p8sx_consumer),
	.consumer_supplies	= v2p8sx_consumer,
};


/*************************************************************
*
* WCOVE Camera related regulator
*
*************************************************************/
static struct regulator_init_data wcove_v1p8sx_data;
static struct regulator_init_data wcove_v2p8sx_data;

static struct wcove_regulator_info wcove_v1p8sx_info = {
	.init_data = &wcove_v1p8sx_data,
};

static struct wcove_regulator_info wcove_v2p8sx_info = {
	.init_data = &wcove_v2p8sx_data,
};

static void wc_set_v1p8_pdata(void)
{
	memcpy((void *)&wcove_v1p8sx_data, (void *)&v1p8sx_data,
			sizeof(struct regulator_init_data));

	intel_soc_pmic_set_pdata("wcove_regulator", &wcove_v1p8sx_info,
		sizeof(struct wcove_regulator_info), WCOVE_ID_V1P8SX + 1);

}

static void wc_set_v2p8_pdata(void)
{
	memcpy((void *)&wcove_v2p8sx_data, (void *)&v2p8sx_data,
			sizeof(struct regulator_init_data));

	/* register camera regulator for whiskey cove PMIC */
	intel_soc_pmic_set_pdata("wcove_regulator", &wcove_v2p8sx_info,
		sizeof(struct wcove_regulator_info), WCOVE_ID_V2P8SX + 1);
}

static void wc_set_gpio_pdata(void)
{
	intel_soc_pmic_set_pdata("whiskey_cove_gpio",
				(void *)&whiskey_cove_gpio_data,
				sizeof(whiskey_cove_gpio_data), 0);
}

static void wc_set_adc_pdata(void)
{
	static struct intel_wcove_gpadc_platform_data wc_adc_pdata;
	wc_adc_pdata.channel_num = GPADC_NUM_CHANNELS;
	wc_adc_pdata.intr_mask = MUSBID | MPEAK | MBATTEMP
		| MSYSTEMP | MBATT | MVIBATT | MGPMEAS | MCCTICK;
	wc_adc_pdata.gpadc_iio_maps = wc_iio_maps;
	wc_adc_pdata.gpadc_regmaps = whiskeycove_gpadc_regmaps;
	wc_adc_pdata.gpadc_regs = &whiskeycove_gpadc_regs;
	wc_adc_pdata.gpadc_channels = wc_adc_channels;
	wc_adc_pdata.thrmmon_val = 0x1B;
	wc_adc_pdata.battthermmon_val = 0x7B;
	wc_adc_pdata.vbatmon_val = 0x1B;
	wc_adc_pdata.gpmon_val = 0x00;

	intel_soc_pmic_set_pdata("wcove_gpadc", (void *)&wc_adc_pdata,
			sizeof(wc_adc_pdata), 0);
}

static acpi_handle pmic_handle(void)
{
	return ACPI_HANDLE(intel_soc_pmic_dev());
}

static int acpi_get_lpat_table(int **lpat)
{
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj_p, *obj_e;
	int i, ret = 0;
	acpi_status status;

	if (!acpi_has_method(pmic_handle(), "LPAT"))
		return -ENODEV;

	status = acpi_evaluate_object(pmic_handle(), "LPAT", NULL, &buffer);
	if (ACPI_FAILURE(status)) {
		dev_err(intel_soc_pmic_dev(), "evaluate LPAT failed\n");
		return -EINVAL;
	}
	obj_p = (union acpi_object *)buffer.pointer;
	if (!obj_p || (obj_p->type != ACPI_TYPE_PACKAGE) ||
	    (obj_p->package.count % 2) || (obj_p->package.count < 4)) {
		dev_err(intel_soc_pmic_dev(), "Invalid LPAT data\n");
		ret = -ENODEV;
		goto err;
	}

	*lpat = devm_kmalloc(intel_soc_pmic_dev(),
			    sizeof(**lpat) * obj_p->package.count, GFP_KERNEL);
	if (!*lpat) {
		dev_err(intel_soc_pmic_dev(), "No mem for lpat\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < obj_p->package.count; i++) {
		obj_e = &obj_p->package.elements[i];
		if (obj_e->type != ACPI_TYPE_INTEGER) {
			dev_err(intel_soc_pmic_dev(), "LPAT invalid data\n");
			ret = -EINVAL;
			goto err;
		}
		(*lpat)[i] = obj_e->integer.value;
	}

	ret = i;

err:
	kfree(buffer.pointer);
	return ret;
}

static bool is_usb_compliant_charging(void)
{
	struct em_config_oem1_data em_config;


	/* On error return true to keep usb compliance and charge
	*  with 100mA until enumerated.
	*/
	if (em_config_get_oem1_data(&em_config) < 0)
		return true;

	/* 0 - usb compliance, 1 - no usb compliance */
	return !(em_config.fpo_0 & FPO0_USB_COMP_OFFSET);
}

static void wcove_set_ccsm_config(void)
{
	static struct intel_pmic_ccsm_platform_data pdata;
	int *lpat;
	int adc_tbl_cnt;
	pdata.intmap = wc_intmap;
	pdata.intmap_size = ARRAY_SIZE(wc_intmap);
	pdata.reg_map = &pmic_wcove_regmap;

	adc_tbl_cnt = acpi_get_lpat_table(&lpat);

	if (adc_tbl_cnt > 0) {
		pdata.max_tbl_row_cnt = adc_tbl_cnt;
		pdata.adc_tbl = (struct temp_lookup *)lpat;
	}

	intel_soc_pmic_set_pdata("wcove_ccsm", &pdata,
		sizeof(pdata), 0);

	pdata.usb_compliance = is_usb_compliant_charging();
}

static void wcove_set_bcu_pdata(void)
{
	intel_soc_pmic_set_pdata("wcove_bcu", (void *)&wc_bcu_pdata,
			sizeof(struct wcove_bcu_platform_data), 0);
}

static int whiskey_cove_init(void)
{
	pr_info("Whiskey Cove: ID 0x%02X, VERSION 0x%02X\n",
		intel_soc_pmic_readb(CHIPID), intel_soc_pmic_readb(CHIPVER));

	wcove_set_ccsm_config();
	wcove_set_bcu_pdata();
	wc_set_adc_pdata();
	wc_set_gpio_pdata();
	wc_set_v1p8_pdata();
	wc_set_v2p8_pdata();
	wc_set_thermal_pdata();
	wcove_init_done = true;

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

#define TT_I2CDADDR_ADDR		0x00
static u8 pmic_read_tt(u8 addr)
{
	int ret;

	ret = intel_soc_pmic_writeb(pmic_wcove_regmap.pmic_chrttaddr,
			addr);

	/* Delay the TT read by 2ms to ensure that the data is populated
	 * in data register
	 */
	usleep_range(2000, 3000);

	return intel_soc_pmic_readb(pmic_wcove_regmap.pmic_chrttdata);
}


static void __init register_external_charger(void)
{
	static struct i2c_board_info i2c_info;

	if (!wcove_init_done)
		return;

	strncpy(i2c_info.type, "ext-charger", I2C_NAME_SIZE);
	i2c_info.addr = pmic_read_tt(TT_I2CDADDR_ADDR);
	i2c_info.irq = whiskey_cove_pmic.irq_base + CHGR_IRQ;
	i2c_new_device(wcove_pmic_i2c_adapter, &i2c_info);
}
late_initcall(register_external_charger);

MODULE_LICENSE("GPL V2");
MODULE_AUTHOR("Yang Bin <bin.yang@intel.com");

