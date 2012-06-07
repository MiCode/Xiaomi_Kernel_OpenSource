/*
 *  Originally from linux/arch/arm/lib/delay.S
 *
 *  Copyright (C) 1995, 1996 Russell King
 *  Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *  Copyright (C) 1993 Linus Torvalds
 *  Copyright (C) 1997 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *  Copyright (C) 2005-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/timex.h>

/*
 * Oh, if only we had a cycle counter...
 */
void delay_loop(unsigned long loops)
{
	asm volatile(
	"1:	subs %0, %0, #1 \n"
	"	bhi 1b		\n"
	: /* No output */
	: "r" (loops)
	);
}

#ifdef ARCH_HAS_READ_CURRENT_TIMER
/*
 * Assuming read_current_timer() is monotonically increasing
 * across calls.
 */
void read_current_timer_delay_loop(unsigned long loops)
{
	unsigned long bclock, now;

	read_current_timer(&bclock);
	do {
		read_current_timer(&now);
	} while ((now - bclock) < loops);
}
#endif

static void (*delay_fn)(unsigned long) = delay_loop;

void set_delay_fn(void (*fn)(unsigned long))
{
	delay_fn = fn;
}

/*
 * loops = usecs * HZ * loops_per_jiffy / 1000000
 */
void __delay(unsigned long loops)
{
	delay_fn(loops);
}
EXPORT_SYMBOL(__delay);

/*
 * 0 <= xloops <= 0x7fffff06
 * loops_per_jiffy <= 0x01ffffff (max. 3355 bogomips)
 */
void __const_udelay(unsigned long xloops)
{
	unsigned long lpj;
	unsigned long loops;

	xloops >>= 14;			/* max = 0x01ffffff */
	lpj = loops_per_jiffy >> 10;	/* max = 0x0001ffff */
	loops = lpj * xloops;		/* max = 0x00007fff */
	loops >>= 6;			/* max = 2^32-1 */

	if (loops)
		__delay(loops);
}
EXPORT_SYMBOL(__const_udelay);

/*
 * usecs  <= 2000
 * HZ  <= 1000
 */
void __udelay(unsigned long usecs)
{
	__const_udelay(usecs * ((2199023UL*HZ)>>11));
}
EXPORT_SYMBOL(__udelay);
