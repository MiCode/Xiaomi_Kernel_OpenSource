#ifndef __ASMARM_ARCH_TIMER_H
#define __ASMARM_ARCH_TIMER_H

#include <linux/ioport.h>
#include <linux/clocksource.h>
#include <asm/errno.h>

struct arch_timer {
	struct resource	res[3];
};

#ifdef CONFIG_ARM_ARCH_TIMER
int arch_timer_register(struct arch_timer *);
int arch_timer_of_register(void);
cycle_t arch_counter_get_cntpct(void);

static inline void __cpuinit arch_counter_set_user_access(void)
{
	u32 cntkctl;

	asm volatile("mrc p15, 0, %0, c14, c1, 0" : "=r" (cntkctl));

	/* disable user access to everything */
	cntkctl &= ~((3 << 8) | (7 << 0));

	asm volatile("mcr p15, 0, %0, c14, c1, 0" : : "r" (cntkctl));
}
#else
static inline int arch_timer_register(struct arch_timer *at)
{
	return -ENXIO;
}

static inline int arch_timer_of_register(void)
{
	return -ENXIO;
}

static inline cycle_t arch_counter_get_cntpct(void)
{
	return 0;
}
#endif

#endif
