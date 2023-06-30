#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/sched/signal.h>
#include <linux/capability.h>
#include <linux/export.h>
#include <linux/suspend.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/pm_wakeirq.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/wakeup_reason.h>
#include <trace/events/power.h>

#define POWER_MONITOR_PERIOD_MS	10000

extern int wakeup_sources_read_lock(void);
extern void wakeup_sources_read_unlock(int idx);

struct delayed_work power_debug_work;

void global_print_active_locks(void)
{
	struct wakeup_source *ws_debug;
	int srcuidx;

	srcuidx = wakeup_sources_read_lock();

	ws_debug = wakeup_sources_walk_start();
	while (ws_debug != NULL) {
		if (ws_debug->active) {
			printk("active wake lock : %s,last_time:%lld\n", ws_debug->name,ktime_to_ms(ws_debug->last_time));
		}
		ws_debug = wakeup_sources_walk_next(ws_debug);
	}

	wakeup_sources_read_unlock(srcuidx);
}

static void power_debug_work_func(struct work_struct *work)
{

	printk("power_debug_work: start \n");

	global_print_active_locks();

	schedule_delayed_work(&power_debug_work,
			  round_jiffies_relative(msecs_to_jiffies
						(POWER_MONITOR_PERIOD_MS)));
}

static int power_debug_init(void)
{

	INIT_DELAYED_WORK(&power_debug_work,  power_debug_work_func);

	schedule_delayed_work(&power_debug_work,
			  round_jiffies_relative(msecs_to_jiffies
						(POWER_MONITOR_PERIOD_MS)));
	return 0;
}

static void __exit power_debug_exit(void)
{
    cancel_delayed_work(&power_debug_work);
}

module_init(power_debug_init);

module_exit(power_debug_exit);

MODULE_AUTHOR("hdlxm");
MODULE_DESCRIPTION("xm power debug driver");
MODULE_LICENSE("GPL");

