/*
 * Copyright (C) 2009 Google, Inc.
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

#ifndef __ASM_ARCH_MSM_SERIAL_DEBUGGER_H
#define __ASM_ARCH_MSM_SERIAL_DEBUGGER_H

#if defined(CONFIG_MSM_SERIAL_DEBUGGER)
void msm_serial_debug_init(unsigned int base, int irq,
		struct device *clk_device, int signal_irq, int wakeup_irq);
#else
static inline void msm_serial_debug_init(unsigned int base, int irq,
		struct device *clk_device, int signal_irq, int wakeup_irq) {}
#endif

#endif
