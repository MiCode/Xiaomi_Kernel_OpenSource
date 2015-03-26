/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_MPM_IRQ_H
#define __MSM_MPM_IRQ_H

#include <linux/types.h>
#include <linux/list.h>

#define MSM_MPM_NR_MPM_IRQS  64

#if defined(CONFIG_MSM_MPM_OF)
/**
 * msm_mpm_enable_pin() -  Enable/Disable a MPM pin for idle wakeups.
 *
 * @pin:	MPM pin to set
 * @enable:	enable/disable the pin
 *
 * returns 0 on success or errorno
 *
 * Drivers can call the function to configure MPM pins for wakeup from idle low
 * power modes. The API provides a direct access to the configuring MPM pins
 * that are not connected to a IRQ/GPIO
 */
int msm_mpm_enable_pin(unsigned int pin, unsigned int enable);

/**
 * msm_mpm_set_pin_wake() -  Enable/Disable a MPM pin during suspend
 *
 * @pin:	MPM pin to set
 * @enable:	enable/disable the pin as wakeup
 *
 * returns 0 on success or errorno
 *
 * Drivers can call the function to configure MPM pins for wakeup from suspend
 * low power modes. The API provides a direct access to the configuring MPM pins
 * that are not connected to a IRQ/GPIO
 */
int msm_mpm_set_pin_wake(unsigned int pin, unsigned int on);
/**
 * msm_mpm_set_pin_type() - Set the flowtype of a MPM pin.
 *
 * @pin:	MPM pin to configure
 * @flow_type:	flowtype of the MPM pin.
 *
 * returns 0 on success or errorno
 *
 * Drivers can call the function to configure the flowtype of the MPM pins
 * The API provides a direct access to the configuring MPM pins that are not
 * connected to a IRQ/GPIO
 */
int msm_mpm_set_pin_type(unsigned int pin, unsigned int flow_type);
/**
 * msm_mpm_irqs_detectable() - Check if active irqs can be monitored by MPM
 *
 * @from_idle: indicates if the sytem is entering low power mode as a part of
 *		suspend/idle task.
 *
 * returns true if all active interrupts can be monitored by the MPM
 *
 * Low power management code calls into this API to check if all active
 * interrupts can be monitored by MPM and choose a level such that all active
 * interrupts can wake the system up from low power mode.
 */
bool msm_mpm_irqs_detectable(bool from_idle);
/**
 * msm_mpm_gpio_detectable() - Check if active gpio irqs can be monitored by
 *				MPM
 *
 * @from_idle: indicates if the sytem is entering low power mode as a part of
 *		suspend/idle task.
 *
 * returns true if all active GPIO interrupts can be monitored by the MPM
 *
 * Low power management code calls into this API to check if all active
 * GPIO interrupts can be monitored by MPM and choose a level such that all
 * active interrupts can wake the system up from low power mode.
 */
bool msm_mpm_gpio_irqs_detectable(bool from_idle);
/**
 * msm_mpm_enter_sleep() -Called from PM code before entering low power mode
 *
 * @sclk_count: wakeup time in sclk counts for programmed RPM wakeup
 * @from_idle: indicates if the sytem is entering low power mode as a part of
 *		suspend/idle task.
 * @cpumask: the next cpu to wakeup.
 *
 * Low power management code calls into this API to configure the MPM to
 * monitor the active irqs before going to sleep.
 */
void msm_mpm_enter_sleep(uint32_t sclk_count, bool from_idle,
		const struct cpumask *cpumask);
/**
 * msm_mpm_exit_sleep() -Called from PM code after resuming from low power mode
 *
 * @from_idle: indicates if the sytem is entering low power mode as a part of
 *		suspend/idle task.
 *
 * Low power management code calls into this API to query the MPM for the
 * wakeup source and retriggering the appropriate interrupt.
 */
void msm_mpm_exit_sleep(bool from_idle);
/**
 * of_mpm_init() - Device tree initialization function
 *
 * The initialization function is called after * GPIO/GIC device initialization
 * routines are called and before any device irqs are requested. MPM driver
 * keeps track of all enabled/wakeup interrupts in the system to be able to
 * configure MPM when entering a system wide low power mode. The MPM is a
 * alway-on low power hardware block that monitors 64 wakeup interrupts when the
 * system is in a low power mode. The initialization function constructs the MPM
 * mapping between the IRQs and the MPM pin based on data in the device tree.
 */
void __init of_mpm_init(void);
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
static inline void msm_mpm_enter_sleep(uint32_t sclk_count, bool from_idle,
		const struct cpumask *cpumask) {}
static inline void msm_mpm_exit_sleep(bool from_idle) {}
static inline void __init of_mpm_init(void) {}
#endif
#ifdef CONFIG_MSM_MPM_OF
/** msm_mpm_suspend_prepare() - Called at prepare_late() op during suspend
 *
 *
 *  When called the MPM driver checks if the wakeup interrupts can be monitored
 *  by MPM hardware and program them accordingly. If wake up interrupts cannot
 *  be monitored then it disallows system low power modes.
 */
void msm_mpm_suspend_prepare(void);
/** msm_mpm_suspend_wake - Called during wake() op in suspend.
 *
 *  When called MPM drivers sets the vote for system low power modes depending
 *  on the active interrupts.
 */
void msm_mpm_suspend_wake(void);
#else
static inline void msm_mpm_suspend_prepare(void) {}
static inline void msm_mpm_suspend_wake(void) {}
#endif
#endif /* __MSM_MPM_IRQ_H */
