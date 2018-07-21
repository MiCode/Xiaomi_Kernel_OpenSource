/*
 * sched_clock.h: support for extending counters to full 64-bit ns counter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LINUX_SCHED_CLOCK
#define LINUX_SCHED_CLOCK

#ifdef CONFIG_GENERIC_SCHED_CLOCK
extern void sched_clock_postinit(void);

extern void sched_clock_register(u64 (*read)(void), int bits,
				 unsigned long rate);
extern int sched_clock_suspend(void);
extern void sched_clock_resume(void);
#else
static inline void sched_clock_postinit(void) { }

static inline void sched_clock_register(u64 (*read)(void), int bits,
					unsigned long rate)
{
	;
}
static inline int sched_clock_suspend(void) { return 0; }
static inline void sched_clock_resume(void) { }
#endif

#endif
