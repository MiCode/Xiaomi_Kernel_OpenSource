/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __LINUX_RT5734_SPI_H
#define __LINUX_RT5734_SPI_H

#include <linux/mutex.h>

struct rt5734_chip {
	struct spi_device *spi;
	struct device *dev;
	struct mutex io_lock;
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *regmap_dev;
#endif /* CONFIG_RT_REGMAP */
	int irq;
};


#define RT5734_CHIPNAME	(0x01)

/* register map */
#define RT5734_CHIPNAME_R		(0x01)
#define RT5734_FLT_RECORDTEMP_R		(0x13)
#define RT5734_IRQ_MASK_R		(0x32)
#define RT5734_BUCK1_DCM_R		(0x3E)
#define RT5734_BUCK1_R			(0x48)
#define RT5734_BUCK1_MODE_R		(0x49)
#define RT5734_BUCK1_RSPCFG1_R		(0x54)
#define RT5734_BUCK2_DCM_R		(0x5B)
#define RT5734_BUCK2_R			(0x62)
#define RT5734_BUCK2_MODE_R		(0x63)
#define RT5734_BUCK2_RSPCFG1_R		(0x6E)
#define RT5734_BUCK3_DCM_R		(0x75)
#define RT5734_BUCK3_R			(0x7C)
#define RT5734_BUCK3_MODE_R		(0x7D)
#define RT5734_BUCK3_RSPCFG1_R		(0x88)

/* 6.Interrupt : OTP/OCP/OV */
#define FLT_RECORDTEMP_FLT_TEMPSDR_M        (0x08)
#define FLT_RECORDTEMP_FLT_TEMPWARNR_M      (0x04)
#define FLT_RECORDTEMP_FLT_TEMPWARNF_M      (0x02)
#define FLT_RECORDTEMP_FLT_TEMPSDF_M        (0x01)
#define FLT_RECORDBUCK1_FLT_BUCK1_WOC_M     (0x40)
#define FLT_RECORDBUCK1_FLT_BUCK1_OV_M      (0x20)
#define FLT_RECORDBUCK1_FLT_BUCK1_UV_M      (0x10)
#define FLT_RECORDBUCK2_FLT_BUCK2_WOC_M     (0x40)
#define FLT_RECORDBUCK2_FLT_BUCK2_OV_M      (0x20)
#define FLT_RECORDBUCK2_FLT_BUCK2_UV_M      (0x10)

extern int rt5734_read_byte(void *client, uint32_t addr, uint32_t *val);
extern int rt5734_write_byte(void *client, uint32_t addr, uint32_t value);
extern int rt5734_assign_bit(void *client, uint32_t reg,
					uint32_t mask, uint32_t data);
extern int rt5734_regulator_init(struct rt5734_chip *chip);
extern int rt5734_regulator_deinit(struct rt5734_chip *chip);

#define rt5734_set_bit(spi, reg, mask) \
	rt5734_assign_bit(spi, reg, mask, mask)

#define rt5734_clr_bit(spi, reg, mask) \
	rt5734_assign_bit(spi, reg, mask, 0x00)

#define RT5734_INFO(format, args...) pr_info(format, ##args)
#define RT5734_pr_notice(format, args...)	pr_notice(format, ##args)

#endif /* __LINUX_RT5734_SPI_H */
