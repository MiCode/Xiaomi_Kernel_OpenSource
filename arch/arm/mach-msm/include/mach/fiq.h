/* linux/include/asm-arm/arch-msm/irqs.h
 *
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MSM_FIQ_H
#define __ASM_ARCH_MSM_FIQ_H

/* cause an interrupt to be an FIQ instead of a regular IRQ */
void msm_fiq_select(int number);
void msm_fiq_unselect(int number);

/* enable/disable an interrupt that is an FIQ (not safe from FIQ context) */
void msm_fiq_enable(int number);
void msm_fiq_disable(int number);

/* install an FIQ handler */
int msm_fiq_set_handler(void (*func)(void *data, void *regs), void *data);

/* cause an edge triggered interrupt to fire (safe from FIQ context */
void msm_trigger_irq(int number);

#endif
