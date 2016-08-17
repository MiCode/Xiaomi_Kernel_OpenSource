/*
 * include/linux/mfd/tps80031.c
 *
 * Core driver interface for TI TPS80031 PMIC
 *
 * Copyright (C) 2011 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef __LINUX_MFD_TPS80031_H
#define __LINUX_MFD_TPS80031_H

#include <linux/rtc.h>

/* Supported chips */
enum chips {
	TPS80031 = 0x00000001,
	TPS80032 = 0x00000002,
};

enum {
	TPS80031_INT_PWRON,
	TPS80031_INT_RPWRON,
	TPS80031_INT_SYS_VLOW,
	TPS80031_INT_RTC_ALARM,
	TPS80031_INT_RTC_PERIOD,
	TPS80031_INT_HOT_DIE,
	TPS80031_INT_VXX_SHORT,
	TPS80031_INT_SPDURATION,
	TPS80031_INT_WATCHDOG,
	TPS80031_INT_BAT,
	TPS80031_INT_SIM,
	TPS80031_INT_MMC,
	TPS80031_INT_RES,
	TPS80031_INT_GPADC_RT,
	TPS80031_INT_GPADC_SW2_EOC,
	TPS80031_INT_CC_AUTOCAL,
	TPS80031_INT_ID_WKUP,
	TPS80031_INT_VBUSS_WKUP,
	TPS80031_INT_ID,
	TPS80031_INT_VBUS,
	TPS80031_INT_CHRG_CTRL,
	TPS80031_INT_EXT_CHRG,
	TPS80031_INT_INT_CHRG,
	TPS80031_INT_RES2,
	TPS80031_INT_BAT_TEMP_OVRANGE,
	TPS80031_INT_BAT_REMOVED,
	TPS80031_INT_VBUS_DET,
	TPS80031_INT_VAC_DET,
	TPS80031_INT_FAULT_WDG,
	TPS80031_INT_LINCH_GATED,

	/* Last interrupt id to get the end number */
	TPS80031_INT_NR,
};

enum adc_channel {
	BATTERY_TYPE			= 0,  /* External ADC */
	BATTERY_TEMPERATURE		= 1,  /* External ADC */
	AUDIO_ACCESSORY			= 2,  /* External ADC */
	TEMPERATURE_EXTERNAL_DIODE	= 3,  /* External ADC */
	TEMPERATURE_MEASUREMENT		= 4,  /* External ADC */
	GENERAL_PURPOSE_1		= 5,  /* External ADC */
	GENERAL_PURPOSE_2		= 6,  /* External ADC */
	SYSTEM_SUPPLY			= 7,  /* Internal ADC */
	BACKUP_BATTERY			= 8,  /* Internal ADC */
	EXTERNAL_CHARGER_INPUT		= 9,  /* Internal ADC */
	VBUS				= 10, /* Internal ADC */
	VBUS_DCDC_OUTPUT_CURRENT	= 11, /* Internal ADC */
	DIE_TEMPERATURE_1		= 12, /* Internal ADC */
	DIE_TEMPERATURE_2		= 13, /* Internal ADC */
	USB_ID_LINE			= 14, /* Internal ADC */
	TEST_NETWORK_1			= 15, /* Internal ADC */
	TEST_NETWORK_2			= 16, /* Internal ADC */
	BATTERY_CHARGING_CURRENT	= 17, /* Internal ADC */
	BATTERY_VOLTAGE			= 18, /* Internal ADC */
};

enum TPS80031_GPIO {
	TPS80031_GPIO_REGEN1,
	TPS80031_GPIO_REGEN2,
	TPS80031_GPIO_SYSEN,

	/* Last entry */
	TPS80031_GPIO_NR,
};

enum TPS80031_CLOCK32K {
	TPS80031_CLOCK32K_AO,
	TPS80031_CLOCK32K_G,
	TPS80031_CLOCK32K_AUDIO,

	/* Last entry */
	TPS80031_CLOCK32K_NR,
};

enum {
	SLAVE_ID0 = 0,
	SLAVE_ID1 = 1,
	SLAVE_ID2 = 2,
	SLAVE_ID3 = 3,
};

enum {
	I2C_ID0_ADDR = 0x12,
	I2C_ID1_ADDR = 0x48,
	I2C_ID2_ADDR = 0x49,
	I2C_ID3_ADDR = 0x4A,
};

/* External controls requests */
enum tps80031_ext_control {
	PWR_REQ_INPUT_NONE	= 0x00000000,
	PWR_REQ_INPUT_PREQ1	= 0x00000001,
	PWR_REQ_INPUT_PREQ2	= 0x00000002,
	PWR_REQ_INPUT_PREQ3	= 0x00000004,
	PWR_OFF_ON_SLEEP	= 0x00000008,
	PWR_ON_ON_SLEEP		= 0x00000010,
};

enum tps80031_pupd_pins {
	TPS80031_PREQ1 = 0,
	TPS80031_PREQ2A,
	TPS80031_PREQ2B,
	TPS80031_PREQ2C,
	TPS80031_PREQ3,
	TPS80031_NRES_WARM,
	TPS80031_PWM_FORCE,
	TPS80031_CHRG_EXT_CHRG_STATZ,
	TPS80031_SIM,
	TPS80031_MMC,
	TPS80031_GPADC_START,
	TPS80031_DVSI2C_SCL,
	TPS80031_DVSI2C_SDA,
	TPS80031_CTLI2C_SCL,
	TPS80031_CTLI2C_SDA,
};

enum tps80031_pupd_settings {
	TPS80031_PUPD_NORMAL,
	TPS80031_PUPD_PULLDOWN,
	TPS80031_PUPD_PULLUP,
};

struct tps80031_subdev_info {
	int		id;
	const char	*name;
	void		*platform_data;
};

struct tps80031_rtc_platform_data {
	int irq;
	struct rtc_time time;
	int msecure_gpio;
};

struct tps80031_clk32k_init_data {
	int clk32k_nr;
	bool enable;
	unsigned long ext_ctrl_flag;
};

struct tps80031_gpio_init_data {
	int gpio_nr;
	unsigned long ext_ctrl_flag;
};

struct tps80031_pupd_init_data {
	int input_pin;
	int setting;
};

struct tps80031_bg_platform_data {
	int irq_base;
	int battery_present;
};

struct tps80031_platform_data {
	int gpio_base;
	int irq_base;
	struct tps80031_32kclock_plat_data *clk32k_pdata;
	struct tps80031_gpio_init_data *gpio_init_data;
	int gpio_init_data_size;
	struct tps80031_clk32k_init_data *clk32k_init_data;
	int clk32k_init_data_size;
	bool use_power_off;
	struct tps80031_pupd_init_data *pupd_init_data;
	int pupd_init_data_size;
	struct tps80031_regulator_platform_data **regulator_pdata;
	int num_regulator_pdata;
	struct tps80031_rtc_platform_data *rtc_pdata;
	struct tps80031_bg_platform_data *bg_pdata;
	struct tps80031_charger_platform_data *battery_charger_pdata;
};


/*
 * NOTE: the functions below are not intended for use outside
 * of the TPS80031 sub-device drivers
 */
extern int tps80031_write(struct device *dev, int sid, int reg, uint8_t val);
extern int tps80031_writes(struct device *dev, int sid, int reg, int len,
				uint8_t *val);
extern int tps80031_read(struct device *dev, int sid, int reg, uint8_t *val);
extern int tps80031_reads(struct device *dev, int sid, int reg, int len,
				uint8_t *val);
extern int tps80031_set_bits(struct device *dev, int sid, int reg,
				uint8_t bit_mask);
extern int tps80031_clr_bits(struct device *dev, int sid, int reg,
				uint8_t bit_mask);
extern int tps80031_update(struct device *dev, int sid, int reg, uint8_t val,
			   uint8_t mask);
extern int tps80031_force_update(struct device *dev, int sid, int reg,
				 uint8_t val, uint8_t mask);
extern int tps80031_ext_power_req_config(struct device *dev,
		unsigned long ext_ctrl_flag, int preq_bit,
		int state_reg_add, int trans_reg_add);

extern unsigned long tps80031_get_chip_info(struct device *dev);

extern int tps80031_gpadc_conversion(int channle_no);

extern int tps80031_get_pmu_version(struct device *dev);

#endif /*__LINUX_MFD_TPS80031_H */
