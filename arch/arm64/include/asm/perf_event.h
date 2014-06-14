/*
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ASM_PERF_EVENT_H
#define __ASM_PERF_EVENT_H

#include <linux/irqreturn.h>
#include <linux/spinlock_types.h>

#ifdef CONFIG_HW_PERF_EVENTS
struct pt_regs;
extern unsigned long perf_instruction_pointer(struct pt_regs *regs);
extern unsigned long perf_misc_flags(struct pt_regs *regs);
irqreturn_t armv8pmu_handle_irq(int irq_num, void *dev);
void arm64_pmu_irq_handled_externally(void);
void arm64_pmu_lock(raw_spinlock_t *lock, unsigned long *flags);
void arm64_pmu_unlock(raw_spinlock_t *lock, unsigned long *flags);
#define perf_misc_flags(regs)	perf_misc_flags(regs)
#else
static inline irqreturn_t armv8pmu_handle_irq(int irq_num, void *dev)
{
	return IRQ_HANDLED;
}
static inline void arm64_pmu_irq_handled_externally(void) { }
static inline void arm64_pmu_lock(raw_spinlock_t *lock, unsigned long *flags)
{ }
static inline void arm64_pmu_unlock(raw_spinlock_t *lock, unsigned long *flags)
{ }
#endif

#endif
