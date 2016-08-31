/*
 * as3720.h definitions
 *
 * Copyright (C) 2012 ams
 *
 * Author: Bernhard Breinbauer <bernhard.breinbauer@ams.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __LINUX_MFD_AS3720_H
#define __LINUX_MFD_AS3720_H

#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/irq.h>
#include <linux/regmap.h>
#include <linux/regulator/machine.h>

#define AS3720_DEVICE_ID	0x88
#define AS3720_REGISTER_COUNT			0x92
#define AS3720_NUM_REGULATORS			19
#define AS3720_NUM_STEPDOWN_REGULATORS		7
#define AS3720_GPIO0				0
#define AS3720_GPIO1				1
#define AS3720_GPIO2				2
#define AS3720_GPIO3				3
#define AS3720_GPIO4				4
#define AS3720_GPIO5				5
#define AS3720_GPIO6				6
#define AS3720_GPIO7				7
#define AS3720_NUM_GPIO				8
#define AS3720_GPIO_IRQ_BASE			0

#define AS3720_REG_INIT(reg_offset, reg_value)  \
{						\
	.reg     = (reg_offset),		\
	.val     = (reg_value),			\
}

#define AS3720_REG_INIT_TERMINATE		0xFF

#define AS3720_LDO0				0
#define AS3720_LDO1				1
#define AS3720_LDO2				2
#define AS3720_LDO3				3
#define AS3720_LDO4				4
#define AS3720_LDO5				5
#define AS3720_LDO6				6
#define AS3720_LDO7				7
#define AS3720_LDO8				8
#define AS3720_LDO9				9
#define AS3720_LDO10				10
#define AS3720_LDO11				11
#define AS3720_SD0				12
#define AS3720_SD1				13
#define AS3720_SD2				14
#define AS3720_SD3				15
#define AS3720_SD4				16
#define AS3720_SD5				17
#define AS3720_SD6				18

#define AS3720_ADDR_ASIC_ID1			0x90
#define AS3720_ADDR_ASIC_ID2			0x91

#define AS3720_LDO0_VOLTAGE_REG			0x10
#define AS3720_LDO1_VOLTAGE_REG			0x11
#define AS3720_LDO2_VOLTAGE_REG			0x12
#define AS3720_LDO3_VOLTAGE_REG			0x13
#define AS3720_LDO4_VOLTAGE_REG			0x14
#define AS3720_LDO5_VOLTAGE_REG			0x15
#define AS3720_LDO6_VOLTAGE_REG			0x16
#define AS3720_LDO7_VOLTAGE_REG			0x17
#define AS3720_LDO8_VOLTAGE_REG			0x18
#define AS3720_LDO9_VOLTAGE_REG			0x19
#define AS3720_LDO10_VOLTAGE_REG		0x1A
#define AS3720_LDO11_VOLTAGE_REG		0x1B
#define AS3720_LDOCONTROL0_REG			0x4E
#define AS3720_LDOCONTROL1_REG			0x4F
#define AS3720_SD0_VOLTAGE_REG			0x00
#define AS3720_SD1_VOLTAGE_REG			0x01
#define AS3720_SD2_VOLTAGE_REG			0x02
#define AS3720_SD3_VOLTAGE_REG			0x03
#define AS3720_SD4_VOLTAGE_REG			0x04
#define AS3720_SD5_VOLTAGE_REG			0x05
#define AS3720_SD6_VOLTAGE_REG			0x06
#define AS3720_GPIO0_CONTROL_REG		0x08
#define AS3720_GPIO1_CONTROL_REG		0x09
#define AS3720_GPIO2_CONTROL_REG		0x0A
#define AS3720_GPIO3_CONTROL_REG		0x0B
#define AS3720_GPIO4_CONTROL_REG		0x0C
#define AS3720_GPIO5_CONTROL_REG		0x0D
#define AS3720_GPIO6_CONTROL_REG		0x0E
#define AS3720_GPIO7_CONTROL_REG		0x0F
#define AS3720_GPIO_SIGNAL_OUT_REG		0x20
#define AS3720_GPIO_SIGNAL_IN_REG		0x21
#define AS3720_CTRL1_REG			0x58
#define AS3720_CTRL2_REG			0x59
#define AS3720_RTC_CONTROL_REG			0x60
#define AS3720_RTC_SECOND_REG			0x61
#define AS3720_RTC_MINUTE1_REG			0x62
#define AS3720_RTC_MINUTE2_REG			0x63
#define AS3720_RTC_MINUTE3_REG			0x64
#define AS3720_RTC_ALARM_SECOND_REG		0x65
#define AS3720_RTC_ALARM_MINUTE1_REG		0x66
#define AS3720_RTC_ALARM_MINUTE2_REG		0x67
#define AS3720_RTC_ALARM_MINUTE3_REG		0x68

#define AS3720_LDO_ILIMIT_MASK			(1 << 7)
#define AS3720_LDO_ILIMIT_BIT			(1 << 7)
#define AS3720_LDO0_VSEL_MASK			0x1F
#define AS3720_LDO0_VSEL_MIN			0x01
#define AS3720_LDO0_VSEL_MAX			0x12
#define AS3720_LDO3_VSEL_MASK			0x7F
#define AS3720_LDO3_VSEL_MIN			0x01
#define AS3720_LDO3_VSEL_MAX			0x5A
#define AS3720_LDO_VSEL_MASK			0x7F
#define AS3720_LDO_VSEL_MIN			0x01
#define AS3720_LDO_VSEL_MAX			0x7F
#define AS3720_LDO_VSEL_DNU_MIN			0x25
#define AS3720_LDO_VSEL_DNU_MAX			0x3F
#define AS3720_LDO_NUM_VOLT			100

#define AS3720_LDO0_ON				(1 << 0)
#define AS3720_LDO0_OFF				(0 << 0)
#define AS3720_LDO0_CTRL_MASK			(1 << 0)
#define AS3720_LDO1_ON				(1 << 1)
#define AS3720_LDO1_OFF				(0 << 1)
#define AS3720_LDO1_CTRL_MASK			(1 << 1)
#define AS3720_LDO2_ON				(1 << 2)
#define AS3720_LDO2_OFF				(0 << 2)
#define AS3720_LDO2_CTRL_MASK			(1 << 2)
#define AS3720_LDO3_ON				(1 << 3)
#define AS3720_LDO3_OFF				(0 << 3)
#define AS3720_LDO3_CTRL_MASK			(1 << 3)
#define AS3720_LDO4_ON				(1 << 4)
#define AS3720_LDO4_OFF				(0 << 4)
#define AS3720_LDO4_CTRL_MASK			(1 << 4)
#define AS3720_LDO5_ON				(1 << 5)
#define AS3720_LDO5_OFF				(0 << 5)
#define AS3720_LDO5_CTRL_MASK			(1 << 5)
#define AS3720_LDO6_ON				(1 << 6)
#define AS3720_LDO6_OFF				(0 << 6)
#define AS3720_LDO6_CTRL_MASK			(1 << 6)
#define AS3720_LDO7_ON				(1 << 7)
#define AS3720_LDO7_OFF				(0 << 7)
#define AS3720_LDO7_CTRL_MASK			(1 << 7)
#define AS3720_LDO8_ON				(1 << 0)
#define AS3720_LDO8_OFF				(0 << 0)
#define AS3720_LDO8_CTRL_MASK			(1 << 0)
#define AS3720_LDO9_ON				(1 << 1)
#define AS3720_LDO9_OFF				(0 << 1)
#define AS3720_LDO9_CTRL_MASK			(1 << 1)
#define AS3720_LDO10_ON				(1 << 2)
#define AS3720_LDO10_OFF			(0 << 2)
#define AS3720_LDO10_CTRL_MASK			(1 << 2)
#define AS3720_LDO11_ON				(1 << 3)
#define AS3720_LDO11_OFF			(0 << 3)
#define AS3720_LDO11_CTRL_MASK			(1 << 3)

#define AS3720_SD_CONTROL_REG			0x4D
#define AS3720_SD0_CONTROL_REG			0x29
#define AS3720_SD1_CONTROL_REG			0x2A
#define AS3720_SDmph_CONTROL_REG		0x2B
#define AS3720_SD23_CONTROL_REG			0x2C
#define AS3720_SD4_CONTROL_REG			0x2D
#define AS3720_SD5_CONTROL_REG			0x2E
#define AS3720_SD6_CONTROL_REG			0x2F

#define AS3720_SD_VSEL_MASK			0x7F
#define AS3720_SD0_VSEL_MIN			0x01
#define AS3720_SD0_VSEL_MAX			0x5A
#define AS3720_SD2_VSEL_MIN			0x01
#define AS3720_SD2_VSEL_MAX			0x7F
#define AS3720_SD0_ON				(1 << 0)
#define AS3720_SD0_OFF				(0 << 0)
#define AS3720_SD0_CTRL_MASK			(1 << 0)
#define AS3720_SD1_ON				(1 << 1)
#define AS3720_SD1_OFF				(0 << 1)
#define AS3720_SD1_CTRL_MASK			(1 << 1)
#define AS3720_SD2_ON				(1 << 2)
#define AS3720_SD2_OFF				(0 << 2)
#define AS3720_SD2_CTRL_MASK			(1 << 2)
#define AS3720_SD3_ON				(1 << 3)
#define AS3720_SD3_OFF				(0 << 3)
#define AS3720_SD3_CTRL_MASK			(1 << 3)
#define AS3720_SD4_ON				(1 << 4)
#define AS3720_SD4_OFF				(0 << 4)
#define AS3720_SD4_CTRL_MASK			(1 << 4)
#define AS3720_SD5_ON				(1 << 5)
#define AS3720_SD5_OFF				(0 << 5)
#define AS3720_SD5_CTRL_MASK			(1 << 5)
#define AS3720_SD6_ON				(1 << 6)
#define AS3720_SD6_OFF				(0 << 6)
#define AS3720_SD6_CTRL_MASK			(1 << 6)

#define AS3720_SD0_MODE_FAST			(1 << 4)
#define AS3720_SD0_MODE_NORMAL			(0 << 4)
#define AS3720_SD0_MODE_MASK			(1 << 4)
#define AS3720_SD1_MODE_FAST			(1 << 4)
#define AS3720_SD1_MODE_NORMAL			(0 << 4)
#define AS3720_SD1_MODE_MASK			(1 << 4)
#define AS3720_SD2_MODE_FAST			(1 << 2)
#define AS3720_SD2_MODE_NORMAL			(0 << 2)
#define AS3720_SD2_MODE_MASK			(1 << 2)
#define AS3720_SD3_MODE_FAST			(1 << 6)
#define AS3720_SD3_MODE_NORMAL			(0 << 6)
#define AS3720_SD3_MODE_MASK			(1 << 6)
#define AS3720_SD4_MODE_FAST			(1 << 2)
#define AS3720_SD4_MODE_NORMAL			(0 << 2)
#define AS3720_SD4_MODE_MASK			(1 << 2)
#define AS3720_SD5_MODE_FAST			(1 << 2)
#define AS3720_SD5_MODE_NORMAL			(0 << 2)
#define AS3720_SD5_MODE_MASK			(1 << 2)
#define AS3720_SD6_MODE_FAST			(1 << 4)
#define AS3720_SD6_MODE_NORMAL			(0 << 4)
#define AS3720_SD6_MODE_MASK			(1 << 4)

#define AS3720_INTERRUPTMASK1_REG		0x74
#define AS3720_INTERRUPTMASK2_REG		0x75
#define AS3720_INTERRUPTMASK3_REG		0x76
#define AS3720_INTERRUPTSTATUS1_REG		0x77
#define AS3720_INTERRUPTSTATUS2_REG		0x78
#define AS3720_INTERRUPTSTATUS3_REG		0x79

#define AS3720_IRQ_MAX_HANDLER			10
#define AS3720_IRQ_LID				0
#define AS3720_IRQ_ACOK				1
#define AS3720_IRQ_CORE_PWRREQ			2
#define AS3720_IRQ_OCURR_ACOK			3
#define AS3720_IRQ_ONKEY_LONG			4
#define AS3720_IRQ_ONKEY			5
#define AS3720_IRQ_OVTMP			6
#define AS3720_IRQ_LOWBAT			7
#define AS3720_IRQ_RTC_REP			8
#define AS3720_IRQ_RTC_ALARM			9
#define AS3720_IRQ_SD0				10

#define AS3720_IRQ_MASK_LID			(1 << 0)
#define AS3720_IRQ_MASK_ACOK			(1 << 1)
#define AS3720_IRQ_MASK_CORE_PWRREQ		(1 << 2)
#define AS3720_IRQ_MASK_OCURR_ACOK		(1 << 3)
#define AS3720_IRQ_MASK_ONKEY_LONG		(1 << 4)
#define AS3720_IRQ_MASK_ONKEY			(1 << 5)
#define AS3720_IRQ_MASK_OVTMP			(1 << 6)
#define AS3720_IRQ_MASK_LOWBAT			(1 << 7)

#define AS3720_IRQ_MASK_SD0			(1 << 0)

#define AS3720_IRQ_MASK_RTC_REP			(1 << 7)

#define AS3720_IRQ_MASK_RTC_ALARM		(1 << 0)

#define AS3720_IRQ_BIT_LID			(1 << 0)
#define AS3720_IRQ_BIT_ACOK			(1 << 1)
#define AS3720_IRQ_BIT_CORE_PWRREQ		(1 << 2)
#define AS3720_IRQ_BIT_SD0			(1 << 3)
#define AS3720_IRQ_BIT_ONKEY_LONG		(1 << 4)
#define AS3720_IRQ_BIT_ONKEY			(1 << 5)
#define AS3720_IRQ_BIT_OVTMP			(1 << 6)
#define AS3720_IRQ_BIT_LOWBAT			(1 << 7)

#define AS3720_IRQ_BIT_RTC_REP			(1 << 7)

#define AS3720_IRQ_BIT_RTC_ALARM		(1 << 0)

#define AS3720_ADC_CONTROL_REG			0x70
#define AS3720_ADC_MSB_RESULT_REG		0x71
#define AS3720_ADC_LSB_RESULT_REG		0x72
#define AS3720_ADC_MASK_CONV_START		(1 << 7)
#define AS3720_ADC_BIT_CONV_START		(1 << 7)
#define AS3720_ADC_MASK_CONV_NOTREADY		(1 << 7)
#define AS3720_ADC_BIT_CONV_NOTREADY		(1 << 7)
#define AS3720_ADC_MASK_SOURCE_SELECT		0x0F
#define AS3720_ADC_SOURCE_I_SD0			0
#define AS3720_ADC_SOURCE_I_SD1			1
#define AS3720_ADC_SOURCE_I_SD6			2
#define AS3720_ADC_SOURCE_TEMP			3
#define AS3720_ADC_SOURCE_VSUP			4
#define AS3720_ADC_SOURCE_GPIO1			5
#define AS3720_ADC_SOURCE_GPIO2			6
#define AS3720_ADC_SOURCE_GPIO3			7
#define AS3720_ADC_SOURCE_GPIO4			8
#define AS3720_ADC_SOURCE_GPIO6			9
#define AS3720_ADC_SOURCE_GPIO7			10
#define AS3720_ADC_SOURCE_VBAT			11

#define AS3720_GPIO_INV_MASK			0x80
#define AS3720_GPIO_INV				0x80
#define AS3720_GPIO_IOSF_MASK			0x78
#define AS3720_GPIO_IOSF_NORMAL			0
#define AS3720_GPIO_IOSF_INTERRUPT_OUT		(1 << 3)
#define AS3720_GPIO_IOSF_VSUP_LOW_OUT		(2 << 3)
#define AS3720_GPIO_IOSF_GPIO_INTERRUPT_IN	(3 << 3)
#define AS3720_GPIO_IOSF_ISINK_PWM_IN		(4 << 3)
#define AS3720_GPIO_IOSF_VOLTAGE_STBY		(5 << 3)
#define AS3720_GPIO_IOSF_PWR_GOOD_OUT		(7 << 3)
#define AS3720_GPIO_IOSF_Q32K_OUT		(8 << 3)
#define AS3720_GPIO_IOSF_WATCHDOG_IN		(9 << 3)
#define AS3720_GPIO_IOSF_SOFT_RESET_IN		(11 << 3)
#define AS3720_GPIO_IOSF_PWM_OUT		(12 << 3)
#define AS3720_GPIO_IOSF_VSUP_LOW_DEB_OUT	(13 << 3)
#define AS3720_GPIO_IOSF_SD6_LOW_VOLT_LOW	(14 << 3)
#define AS3720_GPIO_MODE_MASK			0x07
#define AS3720_GPIO_MODE_INPUT			0
#define AS3720_GPIO_MODE_OUTPUT_VDDH		1
#define AS3720_GPIO_MODE_IO_OPEN_DRAIN		2
#define AS3720_GPIO_MODE_ADC_IN			3
#define AS3720_GPIO_MODE_INPUT_W_PULLUP		4
#define AS3720_GPIO_MODE_INPUT_W_PULLDOWN	5
#define AS3720_GPIO_MODE_IO_OPEN_DRAIN_PULLUP	6
#define AS3720_GPIO_MODE_OUTPUT_VDDL		7
#define AS3720_GPIO0_SIGNAL_MASK		(1 << 0)
#define AS3720_GPIO1_SIGNAL_MASK		(1 << 1)
#define AS3720_GPIO2_SIGNAL_MASK		(1 << 2)
#define AS3720_GPIO3_SIGNAL_MASK		(1 << 3)
#define AS3720_GPIO4_SIGNAL_MASK		(1 << 4)
#define AS3720_GPIO5_SIGNAL_MASK		(1 << 5)
#define AS3720_GPIO6_SIGNAL_MASK		(1 << 6)
#define AS3720_GPIO7_SIGNAL_MASK		(1 << 7)

#define AS3720_RTC_REP_WAKEUP_EN_MASK		(1 << 0)
#define AS3720_RTC_ALARM_WAKEUP_EN_MASK		(1 << 1)
#define AS3720_RTC_ON_MASK			(1 << 2)
#define AS3720_RTC_IRQMODE_MASK			(3 << 3)

struct as3720_reg_init {
	u32 reg;
	u32 val;
};

struct as3720_rtc {
	struct rtc_device *rtc;
	int alarm_enabled;      /* used for suspend/resume */
};

struct as3720 {
	struct device *dev;
	struct regmap *regmap;
	struct regmap_irq_chip_data *irq_data;
	struct regulator_dev *rdevs[AS3720_NUM_REGULATORS];
	struct as3720_rtc rtc;
	int chip_irq;
	int irq_base;
};

enum {
	AS3720_GPIO_CFG_NO_INVERT = 0,
	AS3720_GPIO_CFG_INVERT = 1,
};

enum {
	AS3720_GPIO_CFG_OUTPUT_DISABLED = 0,
	AS3720_GPIO_CFG_OUTPUT_ENABLED = 1,
};

struct as3720_gpio_config {
	int gpio;
	int mode;
	int invert;
	int iosf;
	int output_state;
};

struct as3720_platform_data {
	struct regulator_init_data *reg_init[AS3720_NUM_REGULATORS];

	/* register initialisation */
	struct as3720_reg_init *core_init_data;
	int gpio_base;
	int irq_base;
	int irq_type;
	int rtc_start_year;
	int num_gpio_cfgs;
	struct as3720_gpio_config *gpio_cfgs;
};

static inline int as3720_reg_read(struct as3720 *as3720, u32 reg, u32 *dest)
{
	return regmap_read(as3720->regmap, reg, dest);
}

static inline int as3720_reg_write(struct as3720 *as3720, u32 reg, u32 value)
{
	return regmap_write(as3720->regmap, reg, value);
}

static inline int as3720_block_read(struct as3720 *as3720, u32 reg,
	int count, u8 *buf)
{
	return regmap_bulk_read(as3720->regmap, reg, buf, count);
}

static inline int as3720_block_write(struct as3720 *as3720, u32 reg,
	int count, u8 *data)
{
	return regmap_bulk_write(as3720->regmap, reg, data, count);
}

static inline int as3720_set_bits(struct as3720 *as3720, u32 reg,
	u32 mask, u8 val)
{
	return regmap_update_bits(as3720->regmap, reg, mask, val);
}
#endif
