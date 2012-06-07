/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_MPM_8625_H_
#define _ARCH_ARM_MACH_MSM_MPM_8625_H_

void msm_gic_irq_extn_init(void __iomem *, void __iomem *);

unsigned int msm_gic_spi_ppi_pending(void);
int msm_gic_irq_idle_sleep_allowed(void);
void msm_gic_irq_enter_sleep1(bool modem_wake, int from_idle, uint32_t
		*irq_mask);
int msm_gic_irq_enter_sleep2(bool modem_wake, int from_idle);
void msm_gic_irq_exit_sleep1(uint32_t irq_mask, uint32_t wakeup_reason,
		uint32_t pending_irqs);
void msm_gic_irq_exit_sleep2(uint32_t irq_mask, uint32_t wakeup_reason,
		uint32_t pending);
void msm_gic_irq_exit_sleep3(uint32_t irq_mask, uint32_t wakeup_reason,
		uint32_t pending_irqs);
#endif
