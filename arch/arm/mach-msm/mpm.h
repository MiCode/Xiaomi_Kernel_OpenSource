/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

enum msm_mpm_pin {
	MSM_MPM_PIN_SDC3_DAT1 = 21,
	MSM_MPM_PIN_SDC3_DAT3 = 22,
	MSM_MPM_PIN_SDC4_DAT1 = 23,
	MSM_MPM_PIN_SDC4_DAT3 = 24,
};

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

#ifdef CONFIG_MSM_MPM
extern struct msm_mpm_device_data msm_mpm_dev_data;

int msm_mpm_enable_pin(enum msm_mpm_pin pin, unsigned int enable);
int msm_mpm_set_pin_wake(enum msm_mpm_pin pin, unsigned int on);
int msm_mpm_set_pin_type(enum msm_mpm_pin pin, unsigned int flow_type);
bool msm_mpm_irqs_detectable(bool from_idle);
bool msm_mpm_gpio_irqs_detectable(bool from_idle);
void msm_mpm_enter_sleep(bool from_idle);
void msm_mpm_exit_sleep(bool from_idle);
void msm_mpm_irq_extn_init(void);
#else

int msm_mpm_enable_irq(unsigned int irq, unsigned int enable)
{ return -ENODEV; }
int msm_mpm_set_irq_wake(unsigned int irq, unsigned int on)
{ return -ENODEV; }
int msm_mpm_set_irq_type(unsigned int irq, unsigned int flow_type)
{ return -ENODEV; }
int msm_mpm_enable_pin(enum msm_mpm_pin pin, unsigned int enable)
{ return -ENODEV; }
int msm_mpm_set_pin_wake(enum msm_mpm_pin pin, unsigned int on)
{ return -ENODEV; }
int msm_mpm_set_pin_type(enum msm_mpm_pin pin, unsigned int flow_type)
{ return -ENODEV; }
bool msm_mpm_irqs_detectable(bool from_idle)
{ return false; }
bool msm_mpm_gpio_irqs_detectable(bool from_idle)
{ return false; }
void msm_mpm_enter_sleep(bool from_idle) {}
void msm_mpm_exit_sleep(bool from_idle) {}
void msm_mpm_irq_extn_init(void) {}
#endif



#endif /* __ARCH_ARM_MACH_MSM_MPM_H */
