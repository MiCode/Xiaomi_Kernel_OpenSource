/*
 * include/linux/mfd/max77663-core.h
 *
 * Copyright 2011 Maxim Integrated Products, Inc.
 * Copyright (C) 2011-2012 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#ifndef __LINUX_MFD_MAX77663_CORE_H__
#define __LINUX_MFD_MAX77663_CORE_H__

#include <linux/irq.h>
#include <linux/mfd/core.h>

/*
 * Interrupts
 */
enum {
	MAX77663_IRQ_INT_TOP_GPIO,	/* TOP GPIO internal int to max77663 */
	MAX77663_IRQ_LBT_LB,		/* Low-Battery */
	MAX77663_IRQ_LBT_THERM_ALRM1,	/* Thermal alarm status, > 120C */
	MAX77663_IRQ_LBT_THERM_ALRM2,	/* Thermal alarm status, > 140C */

	MAX77663_IRQ_GPIO0,		/* GPIO0 edge detection */
	MAX77663_IRQ_GPIO1,		/* GPIO1 edge detection */
	MAX77663_IRQ_GPIO2,		/* GPIO2 edge detection */
	MAX77663_IRQ_GPIO3,		/* GPIO3 edge detection */
	MAX77663_IRQ_GPIO4,		/* GPIO4 edge detection */
	MAX77663_IRQ_GPIO5,		/* GPIO5 edge detection */
	MAX77663_IRQ_GPIO6,		/* GPIO6 edge detection */
	MAX77663_IRQ_GPIO7,		/* GPIO7 edge detection */

	MAX77663_IRQ_ONOFF_HRDPOWRN,	/* Hard power off warnning */
	MAX77663_IRQ_ONOFF_EN0_1SEC,	/* EN0 active for 1s */
	MAX77663_IRQ_ONOFF_EN0_FALLING,	/* EN0 falling */
	MAX77663_IRQ_ONOFF_EN0_RISING,	/* EN0 rising */
	MAX77663_IRQ_ONOFF_LID_FALLING,	/* LID falling */
	MAX77663_IRQ_ONOFF_LID_RISING,	/* LID rising */
	MAX77663_IRQ_ONOFF_ACOK_FALLING,/* ACOK falling */
	MAX77663_IRQ_ONOFF_ACOK_RISING,	/* ACOK rising */

	MAX77663_IRQ_RTC,		/* RTC */
	MAX77663_IRQ_SD_PF,		/* SD power fail */
	MAX77663_IRQ_LDO_PF,		/* LDO power fail */
	MAX77663_IRQ_32K,		/* 32kHz oscillator */
	MAX77663_IRQ_NVER,		/* Non-Volatile Event Recorder */

	MAX77663_IRQ_NR,
};

/*
 *GPIOs
 */
enum {
	MAX77663_GPIO0,
	MAX77663_GPIO1,
	MAX77663_GPIO2,
	MAX77663_GPIO3,
	MAX77663_GPIO4,
	MAX77663_GPIO5,
	MAX77663_GPIO6,
	MAX77663_GPIO7,

	MAX77663_GPIO_NR,
};

/* Direction */
enum max77663_gpio_dir {
	GPIO_DIR_DEF,
	GPIO_DIR_IN,
	GPIO_DIR_OUT,
};

/* Data output */
enum max77663_gpio_data_out {
	GPIO_DOUT_DEF,
	GPIO_DOUT_HIGH,
	GPIO_DOUT_LOW,
};

/* Output drive */
enum max77663_gpio_out_drv {
	GPIO_OUT_DRV_DEF,
	GPIO_OUT_DRV_PUSH_PULL,
	GPIO_OUT_DRV_OPEN_DRAIN,
};

/* Pull-up */
enum max77663_gpio_pull_up {
	GPIO_PU_DEF,
	GPIO_PU_ENABLE,
	GPIO_PU_DISABLE,
};

/* Pull-down */
enum max77663_gpio_pull_down {
	GPIO_PD_DEF,
	GPIO_PD_ENABLE,
	GPIO_PD_DISABLE,
};

/* Alternate */
enum max77663_gpio_alt {
	GPIO_ALT_DEF,
	GPIO_ALT_ENABLE,
	GPIO_ALT_DISABLE,
};

enum max77663_chip_version {
	MAX77663_DRV_NOT_DEFINED,
	MAX77663_DRV_10,
	MAX77663_DRV_12,
	MAX77663_DRV_1A,
	MAX77663_DRV_1B,
	MAX77663_DRV_1E,
	MAX77663_DRV_20,
	MAX77663_DRV_22,
	MAX77663_DRV_24,
};
/*
 * Flags
 */
#define SLP_LPM_ENABLE		0x01

struct max77663_gpio_config {
	int gpio;	/* gpio number */
	enum max77663_gpio_dir dir;
	enum max77663_gpio_data_out dout;
	enum max77663_gpio_out_drv out_drv;
	enum max77663_gpio_pull_up pull_up;
	enum max77663_gpio_pull_down pull_down;
	enum max77663_gpio_alt alternate;
};

struct max77663_platform_data {
	int irq_base;
	int gpio_base;

	int num_gpio_cfgs;
	struct max77663_gpio_config *gpio_cfgs;

	int num_subdevs;
	struct mfd_cell *sub_devices;

	struct max77663_regulator_platform_data **regulator_pdata;
	int num_regulator_pdata;

	unsigned int flags;

	unsigned char rtc_i2c_addr;

	bool use_power_off;
};

#if defined(CONFIG_MFD_MAX77663)
int max77663_read(struct device *dev, u8 addr, void *values, u32 len,
		  bool is_rtc);
int max77663_write(struct device *dev, u8 addr, void *values, u32 len,
		   bool is_rtc);
int max77663_set_bits(struct device *dev, u8 addr, u8 mask, u8 value,
		      bool is_rtc);
int max77663_gpio_set_alternate(int gpio, int alternate);
int max77663_read_chip_version(struct device *dev, u8 *val);
#else
static inline int max77663_read(struct device *dev, u8 addr, void *values,
				u32 len, bool is_rtc)
{
	return 0;
}

static inline int max77663_write(struct device *dev, u8 addr, void *values,
				 u32 len, bool is_rtc)
{
	return 0;
}

static inline int max77663_set_bits(struct device *dev, u8 addr, u8 mask,
				    u8 value, bool is_rtc)
{
	return 0;
}

static inline int max77663_gpio_set_alternate(int gpio, int alternate)
{
	return 0;
}

static inline int max77663_read_chip_version(struct device *dev, u8 *val)
{
	return 0;
}
#endif /* defined(CONFIG_MFD_MAX77663) */

#endif /* __LINUX_MFD_MAX77663_CORE_H__ */
