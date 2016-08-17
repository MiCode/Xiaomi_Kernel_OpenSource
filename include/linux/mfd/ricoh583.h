/* include/linux/mfd/ricoh583.h
 *
 * Core driver interface to access RICOH583 power management chip.
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

#ifndef __LINUX_MFD_RICOH583_H
#define __LINUX_MFD_RICOH583_H

#include <linux/rtc.h>
/* RICOH583 IRQ definitions */
enum {
	RICOH583_IRQ_ONKEY,
	RICOH583_IRQ_ACOK,
	RICOH583_IRQ_LIDOPEN,
	RICOH583_IRQ_PREOT,
	RICOH583_IRQ_CLKSTP,
	RICOH583_IRQ_ONKEY_OFF,
	RICOH583_IRQ_WD,
	RICOH583_IRQ_EN_PWRREQ1,
	RICOH583_IRQ_EN_PWRREQ2,
	RICOH583_IRQ_PRE_VINDET,

	RICOH583_IRQ_DC0LIM,
	RICOH583_IRQ_DC1LIM,
	RICOH583_IRQ_DC2LIM,
	RICOH583_IRQ_DC3LIM,

	RICOH583_IRQ_CTC,
	RICOH583_IRQ_YALE,
	RICOH583_IRQ_DALE,
	RICOH583_IRQ_WALE,

	RICOH583_IRQ_AIN1L,
	RICOH583_IRQ_AIN2L,
	RICOH583_IRQ_AIN3L,
	RICOH583_IRQ_VBATL,
	RICOH583_IRQ_VIN3L,
	RICOH583_IRQ_VIN8L,
	RICOH583_IRQ_AIN1H,
	RICOH583_IRQ_AIN2H,
	RICOH583_IRQ_AIN3H,
	RICOH583_IRQ_VBATH,
	RICOH583_IRQ_VIN3H,
	RICOH583_IRQ_VIN8H,
	RICOH583_IRQ_ADCEND,

	RICOH583_IRQ_GPIO0,
	RICOH583_IRQ_GPIO1,
	RICOH583_IRQ_GPIO2,
	RICOH583_IRQ_GPIO3,
	RICOH583_IRQ_GPIO4,
	RICOH583_IRQ_GPIO5,
	RICOH583_IRQ_GPIO6,
	RICOH583_IRQ_GPIO7,
	RICOH583_NR_IRQS,
};

/* Ricoh583 gpio definitions */
enum {
	RICOH583_GPIO0,
	RICOH583_GPIO1,
	RICOH583_GPIO2,
	RICOH583_GPIO3,
	RICOH583_GPIO4,
	RICOH583_GPIO5,
	RICOH583_GPIO6,
	RICOH583_GPIO7,

	RICOH583_NR_GPIO,
};

enum ricoh583_deepsleep_control_id {
	RICOH583_DS_NONE,
	RICOH583_DS_DC0,
	RICOH583_DS_DC1,
	RICOH583_DS_DC2,
	RICOH583_DS_DC3,
	RICOH583_DS_LDO0,
	RICOH583_DS_LDO1,
	RICOH583_DS_LDO2,
	RICOH583_DS_LDO3,
	RICOH583_DS_LDO4,
	RICOH583_DS_LDO5,
	RICOH583_DS_LDO6,
	RICOH583_DS_LDO7,
	RICOH583_DS_LDO8,
	RICOH583_DS_LDO9,
	RICOH583_DS_PSO0,
	RICOH583_DS_PSO1,
	RICOH583_DS_PSO2,
	RICOH583_DS_PSO3,
	RICOH583_DS_PSO4,
	RICOH583_DS_PSO5,
	RICOH583_DS_PSO6,
	RICOH583_DS_PSO7,
};
enum ricoh583_ext_pwrreq_control {
	RICOH583_EXT_PWRREQ1_CONTROL = 0x1,
	RICOH583_EXT_PWRREQ2_CONTROL = 0x2,
};

struct ricoh583_subdev_info {
	int		id;
	const char	*name;
	void		*platform_data;
};

struct ricoh583_rtc_platform_data {
	int irq;
	struct rtc_time time;
};

struct ricoh583_gpio_init_data {
	unsigned pulldn_en:1;   /* Enable pull down */
	unsigned output_mode_en:1; /* Enable output mode during init */
	unsigned output_val:1;  /* Output value if it is in output mode */
	unsigned init_apply:1;  /* Apply init data on configuring gpios*/
};

struct ricoh583_platform_data {
	int		num_subdevs;
	struct	ricoh583_subdev_info *subdevs;
	int		gpio_base;
	int		irq_base;

	struct ricoh583_gpio_init_data *gpio_init_data;
	int num_gpioinit_data;
	bool enable_shutdown_pin;
};

extern int ricoh583_read(struct device *dev, uint8_t reg, uint8_t *val);
extern int ricoh583_bulk_reads(struct device *dev, u8 reg, u8 count,
				uint8_t *val);
extern int ricoh583_write(struct device *dev, u8 reg, uint8_t val);
extern int ricoh583_bulk_writes(struct device *dev, u8 reg, u8 count,
				uint8_t *val);
extern int ricoh583_set_bits(struct device *dev, u8 reg, uint8_t bit_mask);
extern int ricoh583_clr_bits(struct device *dev, u8 reg, uint8_t bit_mask);
extern int ricoh583_update(struct device *dev, u8 reg, uint8_t val,
					uint8_t mask);
extern int ricoh583_ext_power_req_config(struct device *dev,
		enum ricoh583_deepsleep_control_id control_id,
		enum ricoh583_ext_pwrreq_control ext_pwr_req,
		int deepsleep_slot_nr);
extern int ricoh583_power_off(void);

#endif
