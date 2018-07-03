/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __QCOM_MPM_H__
#define __QCOM_MPM_H__

#include <linux/irq.h>
#include <linux/device.h>

struct mpm_pin {
	int pin;
	irq_hw_number_t hwirq;
};

extern const struct mpm_pin mpm_msm8937_gic_chip_data[];
extern const struct mpm_pin mpm_msm8937_gpio_chip_data[];

extern const struct mpm_pin mpm_qcs405_gic_chip_data[];
extern const struct mpm_pin mpm_qcs405_gpio_chip_data[];
#endif /* __QCOM_MPM_H__ */
