#ifndef __ASM_IRQ_H
#define __ASM_IRQ_H

#include <asm-generic/irq.h>

struct pt_regs;

extern void set_handle_irq(void (*handle_irq)(struct pt_regs *));

static inline int nr_legacy_irqs(void)
{
	return 0;
}

void arch_trigger_all_cpu_backtrace(void);
#define arch_trigger_all_cpu_backtrace arch_trigger_all_cpu_backtrace

#endif
