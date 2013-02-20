/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_MPM_H
#define __ARCH_ARM_MACH_MSM_MPM_H

#include <linux/types.h>
#include <linux/list.h>

#define MSM_MPM_NR_MPM_IRQS  64

struct msm_mpm_device_data {
	uint16_t *irqs_m2a;
	unsigned int irqs_m2a_size;
	uint16_t *bypassed_apps_irqs;
	unsigned int bypassed_apps_irqs_size;
	void __iomem *mpm_request_reg_base;
	void __iomem *mpm_status_reg_base;
	void __iomem *mpm_apps_ipc_reg;
	unsigned int mpm_apps_ipc_val;
	unsigned int mpm_ipc_irq;
};

extern struct msm_mpm_device_data msm8660_mpm_dev_data;
extern struct msm_mpm_device_data msm8960_mpm_dev_data;
extern struct msm_mpm_device_data msm9615_mpm_dev_data;
extern struct msm_mpm_device_data apq8064_mpm_dev_data;

void msm_mpm_irq_extn_init(struct msm_mpm_device_data *mpm_data);

#ifdef CONFIG_MSM_MPM
int msm_mpm_enable_pin(unsigned int pin, unsigned int enable);
int msm_mpm_set_pin_wake(unsigned int pin, unsigned int on);
int msm_mpm_set_pin_type(unsigned int pin, unsigned int flow_type);
bool msm_mpm_irqs_detectable(bool from_idle);
bool msm_mpm_gpio_irqs_detectable(bool from_idle);
void msm_mpm_enter_sleep(bool from_idle);
void msm_mpm_exit_sleep(bool from_idle);
#else
static inline int msm_mpm_enable_irq(unsigned int irq, unsigned int enable)
{ return -ENODEV; }
static inline int msm_mpm_set_irq_wake(unsigned int irq, unsigned int on)
{ return -ENODEV; }
static inline int msm_mpm_set_irq_type(unsigned int irq, unsigned int flow_type)
{ return -ENODEV; }
static inline int msm_mpm_enable_pin(unsigned int pin, unsigned int enable)
{ return -ENODEV; }
static inline int msm_mpm_set_pin_wake(unsigned int pin, unsigned int on)
{ return -ENODEV; }
static inline int msm_mpm_set_pin_type(unsigned int pin,
				       unsigned int flow_type)
{ return -ENODEV; }
static inline bool msm_mpm_irqs_detectable(bool from_idle)
{ return false; }
static inline bool msm_mpm_gpio_irqs_detectable(bool from_idle)
{ return false; }
static inline void msm_mpm_enter_sleep(bool from_idle) {}
static inline void msm_mpm_exit_sleep(bool from_idle) {}
#endif



#endif /* __ARCH_ARM_MACH_MSM_MPM_H */
