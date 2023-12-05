#include <linux/time64.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/pm_wakeup.h>
#include <linux/rculist.h>
#include <linux/ktime.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/sched/loadavg.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
#include <linux/jiffies.h>
#include <linux/hashtable.h> // hashtable API
#include <linux/string.h>    // strcpy, strcmp
#include <linux/types.h>     // u32 etc.
#include <linux/slab.h>
#include <linux/rtmutex.h>
#include <linux/kobject.h>

extern void ktime_get_real_ts64(struct timespec64 *tv);
extern int wakeup_sources_read_lock(void);
extern void wakeup_sources_read_unlock(int idx);
//extern void task_cputime_adjusted(struct task_struct *p, u64 *ut, u64 *st);
#define power_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

u64 div64_u64_bak(u64 dividend, u64 divisor)
{
	u32 high = divisor >> 32;
	u64 quot;

	if (high == 0) {
		quot = div_u64(dividend, divisor);
	} else {
		int n = fls(high);
		quot = div_u64(dividend >> n, divisor >> n);

		if (quot != 0)
			quot--;
		if ((dividend - quot * divisor) >= divisor)
			quot++;
	}

	return quot;
}

u64 div64_u64_rem_bak(u64 dividend, u64 divisor, u64 *remainder)
{
	u32 high = divisor >> 32;
	u64 quot;

	if (high == 0) {
		u32 rem32;
		quot = div_u64_rem(dividend, divisor, &rem32);
		*remainder = rem32;
	} else {
		int n = fls(high);
		quot = div_u64(dividend >> n, divisor >> n);

		if (quot != 0)
			quot--;

		*remainder = dividend - quot * divisor;
		if (*remainder >= divisor) {
			quot++;
			*remainder -= divisor;
		}
	}

	return quot;
}

u64 mul_u64_u64_div_u64_bak(u64 a, u64 b, u64 c)
{
	u64 res = 0, div, rem;
	int shift;

	/* can a * b overflow ? */
	if (ilog2(a) + ilog2(b) > 62) {
		div = div64_u64_rem_bak(b, c, &rem);
		res = div * a;
		b = rem;

		shift = ilog2(a) + ilog2(b) - 62;
		if (shift > 0) {
			/* drop precision */
			b >>= shift;
			c >>= shift;
			if (!c)
				return res;
		}
	}

	return res + div64_u64_bak(a * b, c);
}