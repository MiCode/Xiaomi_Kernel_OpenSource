/*
 * include/linux/mfd/tps6591x.c
 * Core driver interface for TI TPS6591x PMIC family
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __LINUX_MFD_TPS6591X_H
#define __LINUX_MFD_TPS6591X_H

#include <linux/rtc.h>

enum {
	TPS6591X_INT_PWRHOLD_F,
	TPS6591X_INT_VMBHI,
	TPS6591X_INT_PWRON,
	TPS6591X_INT_PWRON_LP,
	TPS6591X_INT_PWRHOLD_R,
	TPS6591X_INT_HOTDIE,
	TPS6591X_INT_RTC_ALARM,
	TPS6591X_INT_RTC_PERIOD,
	TPS6591X_INT_GPIO0,
	TPS6591X_INT_GPIO1,
	TPS6591X_INT_GPIO2,
	TPS6591X_INT_GPIO3,
	TPS6591X_INT_GPIO4,
	TPS6591X_INT_GPIO5,
	TPS6591X_INT_WTCHDG,
	TPS6591X_INT_VMBCH2_H,
	TPS6591X_INT_VMBCH2_L,
	TPS6591X_INT_PWRDN,

	/* Last entry */
	TPS6591X_INT_NR,
};

/* Gpio definitions */
enum {
	TPS6591X_GPIO_GP0 = 0,
	TPS6591X_GPIO_GP1 = 1,
	TPS6591X_GPIO_GP2 = 2,
	TPS6591X_GPIO_GP3 = 3,
	TPS6591X_GPIO_GP4 = 4,
	TPS6591X_GPIO_GP5 = 5,
	TPS6591X_GPIO_GP6 = 6,
	TPS6591X_GPIO_GP7 = 7,
	TPS6591X_GPIO_GP8 = 8,

	/* Last entry */
	TPS6591X_GPIO_NR,
};

enum tps6591x_pup_flags {
	TPS6591X_PUP_NRESPWRON2P,
	TPS6591X_PUP_HDRSTP,
	TPS6591X_PUP_PWRHOLDP,
	TPS6591X_PUP_SLEEPP,
	TPS6591X_PUP_PWRONP,
	TPS6591X_PUP_I2CSRP,
	TPS6591X_PUP_I2CCTLP,
};

enum tps6591x_pup_val {
	TPS6591X_PUP_DIS,
	TPS6591X_PUP_EN,
	TPS6591X_PUP_DEFAULT,
};

struct tps6591x_subdev_info {
	int		id;
	const char	*name;
	void		*platform_data;
};

struct tps6591x_rtc_platform_data {
	int irq;
	struct rtc_time time;
};

struct tps6591x_sleep_keepon_data {
	/* set 1 to maintain the following on sleep mode */
	unsigned therm_keepon:1;	/* themal monitoring */
	unsigned clkout32k_keepon:1;	/* CLK32KOUT */
	unsigned vrtc_keepon:1;		/* LD0 full load capability */
	unsigned i2chs_keepon:1;	/* high speed internal clock */
};

struct tps6591x_gpio_init_data {
	unsigned sleep_en:1;	/* Enable sleep mode */
	unsigned pulldn_en:1;	/* Enable pull down */
	unsigned output_mode_en:1; /* Enable output mode during init */
	unsigned output_val:1;	/* Output value if it is in output mode */
	unsigned init_apply:1;	/* Apply init data on configuring gpios*/
};

struct tps6591x_pup_init_data {
	unsigned pin_id;
	unsigned pup_val;
};

struct tps6591x_platform_data {
	int gpio_base;
	int irq_base;

	int num_subdevs;
	struct tps6591x_subdev_info *subdevs;

	bool dev_slp_en;
	bool dev_slp_delayed; /* Set the SLEEP only before entering suspend */
	struct tps6591x_sleep_keepon_data *slp_keepon;

	struct tps6591x_gpio_init_data *gpio_init_data;
	int num_gpioinit_data;

	bool use_power_off;

	struct tps6591x_pup_init_data *pup_data;
	int num_pins;
};

/*
 * NOTE: the functions below are not intended for use outside
 * of the TPS6591X sub-device drivers
 */
extern int tps6591x_write(struct device *dev, int reg, uint8_t val);
extern int tps6591x_writes(struct device *dev, int reg, int len, uint8_t *val);
extern int tps6591x_read(struct device *dev, int reg, uint8_t *val);
extern int tps6591x_reads(struct device *dev, int reg, int len, uint8_t *val);
extern int tps6591x_set_bits(struct device *dev, int reg, uint8_t bit_mask);
extern int tps6591x_clr_bits(struct device *dev, int reg, uint8_t bit_mask);
extern int tps6591x_update(struct device *dev, int reg, uint8_t val,
			   uint8_t mask);

#endif /*__LINUX_MFD_TPS6591X_H */
