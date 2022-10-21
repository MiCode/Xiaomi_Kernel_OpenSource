#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#define POWER_MONITOR_PERIOD_MS	10000

struct delayed_work power_debug_work;

extern void global_print_active_locks(void);

static void power_debug_work_func(struct work_struct *work)
{

	//print wakelocks
	printk("power_debug_work: start \n");
	global_print_active_locks();
	//wakelock_stats_show_debug();
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

