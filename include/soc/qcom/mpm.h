/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __QCOM_MPM_H__
#define __QCOM_MPM_H__

#include <linux/irq.h>
#include <linux/device.h>

struct mpm_pin {
	int pin;
	irq_hw_number_t hwirq;
};

void msm_mpm_timer_write(uint32_t *expiry);
void msm_mpm_enter_sleep(struct cpumask *cpumask);
extern const struct mpm_pin mpm_holi_gic_chip_data[];
#endif /* __QCOM_MPM_H__ */
