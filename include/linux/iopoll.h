/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_IOPOLL_H
#define _LINUX_IOPOLL_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <asm-generic/errno.h>
#include <asm/io.h>

/**
 * readl_poll_timeout - Periodically poll an address until a condition is met or a timeout occurs
 * @addr: Address to poll
 * @val: Variable to read the value into
 * @cond: Break condition (usually involving @val)
 * @sleep_us: Maximum time to sleep between reads in us (0 tight-loops)
 * @timeout_us: Timeout in us, 0 means never timeout
 *
 * Returns 0 on success and -ETIMEDOUT upon a timeout. In either
 * case, the last read value at @addr is stored in @val. Must not
 * be called from atomic context if sleep_us or timeout_us are used.
 */
#define readl_poll_timeout(addr, val, cond, sleep_us, timeout_us) \
({ \
	ktime_t timeout = ktime_add_us(ktime_get(), timeout_us); \
	might_sleep_if(timeout_us); \
	for (;;) { \
		(val) = readl(addr); \
		if (cond) \
			break; \
		if (timeout_us && ktime_compare(ktime_get(), timeout) > 0) { \
			(val) = readl(addr); \
			break; \
		} \
		if (sleep_us) \
			usleep_range(DIV_ROUND_UP(sleep_us, 4), sleep_us); \
	} \
	(cond) ? 0 : -ETIMEDOUT; \
})

/**
 * readl_poll_timeout_noirq - Periodically poll an address until a condition is met or a timeout occurs
 * @addr: Address to poll
 * @val: Variable to read the value into
 * @cond: Break condition (usually involving @val)
 * @max_reads: Maximum number of reads before giving up
 * @time_between_us: Time to udelay() between successive reads
 *
 * Returns 0 on success and -ETIMEDOUT upon a timeout.
 */
#define readl_poll_timeout_noirq(addr, val, cond, max_reads, time_between_us) \
({ \
	int count; \
	for (count = (max_reads); count > 0; count--) { \
		(val) = readl(addr); \
		if (cond) \
			break; \
		udelay(time_between_us); \
	} \
	(cond) ? 0 : -ETIMEDOUT; \
})

/**
 * readl_poll - Periodically poll an address until a condition is met
 * @addr: Address to poll
 * @val: Variable to read the value into
 * @cond: Break condition (usually involving @val)
 * @sleep_us: Maximum time to sleep between reads in us (0 tight-loops)
 *
 * Must not be called from atomic context if sleep_us is used.
 */
#define readl_poll(addr, val, cond, sleep_us) \
	readl_poll_timeout(addr, val, cond, sleep_us, 0)

/**
 * readl_tight_poll_timeout - Tight-loop on an address until a condition is met or a timeout occurs
 * @addr: Address to poll
 * @val: Variable to read the value into
 * @cond: Break condition (usually involving @val)
 * @timeout_us: Timeout in us, 0 means never timeout
 *
 * Returns 0 on success and -ETIMEDOUT upon a timeout. In either
 * case, the last read value at @addr is stored in @val. Must not
 * be called from atomic context if timeout_us is used.
 */
#define readl_tight_poll_timeout(addr, val, cond, timeout_us) \
	readl_poll_timeout(addr, val, cond, 0, timeout_us)

/**
 * readl_tight_poll - Tight-loop on an address until a condition is met
 * @addr: Address to poll
 * @val: Variable to read the value into
 * @cond: Break condition (usually involving @val)
 *
 * May be called from atomic context.
 */
#define readl_tight_poll(addr, val, cond) \
	readl_poll_timeout(addr, val, cond, 0, 0)

#endif /* _LINUX_IOPOLL_H */
