#ifndef __ASMARM_ARCH_TIMER_H
#define __ASMARM_ARCH_TIMER_H

#include <linux/ioport.h>
#include <linux/clocksource.h>
#include <asm/errno.h>

#define ARCH_TIMER_USR_PCT_ACCESS_EN	(1 << 0) /* physical counter */
#define ARCH_TIMER_USR_VCT_ACCESS_EN	(1 << 1) /* virtual counter */
#define ARCH_TIMER_VIRT_EVT_EN		(1 << 2)
#define ARCH_TIMER_EVT_TRIGGER_SHIFT	(4)
#define ARCH_TIMER_EVT_TRIGGER_MASK	(0xF << ARCH_TIMER_EVT_TRIGGER_SHIFT)
#define ARCH_TIMER_USR_VT_ACCESS_EN	(1 << 8) /* virtual timer registers */
#define ARCH_TIMER_USR_PT_ACCESS_EN	(1 << 9) /* physical timer registers */

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

	/* Disable user access to the timers and the physical counter */
	/* Also disable virtual event stream */
	cntkctl &= ~(ARCH_TIMER_USR_PT_ACCESS_EN
			| ARCH_TIMER_USR_VT_ACCESS_EN
			| ARCH_TIMER_VIRT_EVT_EN
			| ARCH_TIMER_USR_PCT_ACCESS_EN);


	/* Enable user access to the virtual counter */
	cntkctl |= ARCH_TIMER_USR_VCT_ACCESS_EN;

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
