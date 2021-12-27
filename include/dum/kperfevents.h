#ifndef _LINUX_KPERFEVENTS_H
#define _LINUX_KPERFEVENTS_H

#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>

static __always_inline bool is_kperfevents_on(void)
{
	return false;
}

static __always_inline bool is_above_kperfevents_threshold_millis(
		u64 delta_millis)
{
	return false;
}

static __always_inline bool is_above_kperfevents_threshold_nanos(
		u64 delta_nanos)
{
	return false;
}

#endif /*_LINUX_KPERFEVENTS_H*/
