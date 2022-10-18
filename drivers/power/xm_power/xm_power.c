#define pr_fmt(fmt) "xm_power: " fmt

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
#include <linux/hashtable.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/rtmutex.h>
#include <linux/kobject.h>

static struct delayed_work power_info_work;

#define XM_POWER_POLLING_MS 10000

static void dump_active_wakeup_sources(void)
{
	struct wakeup_source *ws_debug;
	int srcuidx, active = 0;
	struct wakeup_source *last_activity_ws = NULL;
	struct wakeup_source *last_sec_activity_ws = NULL;
	ktime_t active_time;

	srcuidx = wakeup_sources_read_lock();
	ws_debug = wakeup_sources_walk_start();
	while (ws_debug != NULL) {
	        if (ws_debug->active) {
                        ktime_t now = ktime_get();
                        active_time = ktime_sub(now, ws_debug->last_time);
                        pr_info("active wake lock: %s, active_since: %lldms, active_count: %lu, pending suspend count: %lu\n",
                                ws_debug->name, ktime_to_ms(active_time), ws_debug->active_count, ws_debug->wakeup_count);
                        active = 1;
                } else if (!active && (!last_activity_ws ||
                                        ktime_compare(ws_debug->last_time, last_activity_ws->last_time))) {
                        if(last_activity_ws!=NULL)
                                last_sec_activity_ws = last_activity_ws;
                        last_activity_ws = ws_debug;
                }
                ws_debug = wakeup_sources_walk_next(ws_debug);
	}

	if (!active && last_activity_ws)
		pr_info("last active wakeup source: %s, last_time:%lldms, active_count: %lu, pending suspend count %lu\n",
                        last_activity_ws->name, ktime_to_ms(last_activity_ws->last_time),
                        last_activity_ws->active_count,last_activity_ws->wakeup_count);

	if (!active && last_sec_activity_ws)
		pr_info("last sec active wakeup source: %s, last_time:%lldms, active_count: %lu, pending suspend count %lu\n",
                        last_sec_activity_ws->name, ktime_to_ms(last_sec_activity_ws->last_time),
		        last_sec_activity_ws->active_count, last_sec_activity_ws->wakeup_count);

	wakeup_sources_read_unlock(srcuidx);
}

static void power_info_dump_func(struct work_struct *work)
{
        dump_active_wakeup_sources();
	schedule_delayed_work(&power_info_work, msecs_to_jiffies(XM_POWER_POLLING_MS));
}

static int __init powerdet_init(void)
{
        pr_info("xm_power module init");
	INIT_DELAYED_WORK(&power_info_work, power_info_dump_func);
        schedule_delayed_work(&power_info_work, msecs_to_jiffies(XM_POWER_POLLING_MS));

	return 0;
}

static void __exit powerdet_exit(void)
{
        pr_info("xm_power module exit");
	cancel_delayed_work(&power_info_work);
}

module_init(powerdet_init);
module_exit(powerdet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("XiaoMI inc.");
MODULE_DESCRIPTION("The Power detector");
