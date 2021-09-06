/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __QCOM_MPM_H__
#define __QCOM_MPM_H__

#include <linux/irq.h>
#include <linux/device.h>

struct mpm_pin {
	int pin;
	irq_hw_number_t hwirq;
};

extern const struct mpm_pin mpm_bengal_gic_chip_data[];
extern const struct mpm_pin mpm_scuba_gic_chip_data[];
extern const struct mpm_pin mpm_sdm660_gic_chip_data[];
extern const struct mpm_pin mpm_msm8937_gic_chip_data[];
extern const struct mpm_pin mpm_msm8953_gic_chip_data[];
extern const struct mpm_pin mpm_khaje_gic_chip_data[];

#endif /* __QCOM_MPM_H__ */
