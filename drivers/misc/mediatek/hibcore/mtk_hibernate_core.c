#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/xlog.h>
#include <linux/mtk_ftrace.h>
#if !defined(CONFIG_CPU_FREQ_DEFAULT_GOV_HOTPLUG) && !defined(CONFIG_CPU_FREQ_DEFAULT_GOV_BALANCE)
#include <mach/mt_hotplug_strategy.h>
#endif

#define HIB_CORE_DEBUG 0
#define _TAG_HIB_M "HIB/CORE"
#if (HIB_CORE_DEBUG)
#undef hib_log
#define hib_log(fmt, ...)	xlog_printk(ANDROID_LOG_WARN, _TAG_HIB_M, fmt, ##__VA_ARGS__);
#else
#define hib_log(fmt, ...)
#endif
#undef hib_warn
#define hib_warn(fmt, ...)  xlog_printk(ANDROID_LOG_WARN, _TAG_HIB_M, fmt,  ##__VA_ARGS__);
#undef hib_err
#define hib_err(fmt, ...)   xlog_printk(ANDROID_LOG_ERROR, _TAG_HIB_M, fmt,  ##__VA_ARGS__);

#ifdef CONFIG_PM_AUTOSLEEP

/* kernel/power/autosleep.c */
extern int pm_autosleep_lock(void);
extern void pm_autosleep_unlock(void);
extern suspend_state_t pm_autosleep_state(void);

#else				/* !CONFIG_PM_AUTOSLEEP */

static inline int pm_autosleep_lock(void)
{
	return 0;
}

static inline void pm_autosleep_unlock(void)
{
}

static inline suspend_state_t pm_autosleep_state(void)
{
	return PM_SUSPEND_ON;
}

#endif				/* !CONFIG_PM_AUTOSLEEP */

#ifdef CONFIG_PM_WAKELOCKS
/* kernel/power/wakelock.c */
extern int pm_wake_lock(const char *buf);
extern int pm_wake_unlock(const char *buf);
#endif				/* !CONFIG_PM_WAKELOCKS */

/* HOTPLUG */
#if defined(CONFIG_CPU_FREQ_GOV_HOTPLUG) || defined(CONFIG_CPU_FREQ_GOV_BALANCE)
extern void hp_set_dynamic_cpu_hotplug_enable(int enable);
extern struct mutex hp_onoff_mutex;
#endif				/* CONFIG_CPU_FREQ_GOV_HOTPLUG || CONFIG_CPU_FREQ_GOV_BALANCE */

bool system_is_hibernating = false;
EXPORT_SYMBOL(system_is_hibernating);

enum {
	HIB_FAILED_TO_SHUTDOWN = 0,
	HIB_FAILED_TO_S2RAM,
};
static int hibernation_failed_action = HIB_FAILED_TO_S2RAM;

#define MAX_HIB_FAILED_CNT 5
static int hib_failed_cnt = 0;

#define HIB_UNPLUG_CORES	/* force unplug cores before hibernation flow */
#if defined(HIB_UNPLUG_CORES) && defined(CONFIG_HOTPLUG_CPU)
#define HIB_MULTIIO_CORES (1)
static void hib_unplug_cores(void)
{
	int i = 0;

	hib_warn("unplug cores\n");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_HOTPLUG
	mutex_lock(&hp_onoff_mutex);
#endif
	for (i = (num_possible_cpus() - 1); i > 0 && num_online_cpus() > HIB_MULTIIO_CORES; i--) {
		if (cpu_online(i)) {
			int err;
			hib_log("cpu %d down...\n", i);
			err = cpu_down(i);
			if (err < 0) {
				hib_warn("cpu %d down...failed(%d)\n", i, err);
			} else {
				hib_log("cpu %d down...done\n", i, err);
			}
		}
	}
#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_HOTPLUG
	mutex_unlock(&hp_onoff_mutex);
#endif
}
#else
static inline void hib_unplug_cores(void)
{
}
#endif

/* en: 1 enable, en: 0 disable */
static void hib_hotplug_mode(int en)
{
	static int g_hp_disable = 0;
	if (en) {
		if (1 == g_hp_disable) {
			hib_warn("enable hotplug\n");
#if defined(CONFIG_CPU_FREQ_DEFAULT_GOV_HOTPLUG) || defined(CONFIG_CPU_FREQ_DEFAULT_GOV_BALANCE)
			hp_set_dynamic_cpu_hotplug_enable(1);
#else
			hps_set_enabled(1);
#endif
			g_hp_disable = 0;
		}
	} else if (!en) {
		if (0 == g_hp_disable) {
			hib_warn("disable hotplug\n");
#if defined(CONFIG_CPU_FREQ_DEFAULT_GOV_HOTPLUG) || defined(CONFIG_CPU_FREQ_DEFAULT_GOV_BALANCE)
			hp_set_dynamic_cpu_hotplug_enable(0);
#else
			hps_set_enabled(0);
#endif
			hib_unplug_cores();
			g_hp_disable = 1;
		}
	}
}

static void hib_ftrace_buffer(int en)
{
#if defined(CONFIG_MTK_SCHED_TRACERS)
	int fterr = 0;

	hib_warn("%s ftrace mode\n", (en ? "enable" : "disable"));
	fterr = resize_ring_buffer_for_hibernation(en);
	if (fterr < 0)
		hib_warn("calling resize_ring_buffer_for_hibernation() failed (%d)\n", fterr);
#endif
}

static void hibernate_recover(void)
{
	hib_ftrace_buffer(1);

	hib_hotplug_mode(1);
}

static void hibernate_restore(void)
{
	hib_ftrace_buffer(1);

	hib_hotplug_mode(1);

	hib_warn("start trigger ipod\n");
}

extern int hybrid_sleep_mode(void);
/* NOTICE: this function MUST be called under autosleep_lock (in autosleep.c) is locked!! */
int mtk_hibernate_via_autosleep(suspend_state_t *autosleep_state)
{
	int err = 0;
	hib_log("entering hibernation state(%d)\n", *autosleep_state);
	err = hibernate();
	if (err) {
		hib_warn
		    ("@@@@@@@@@@@@@@@@@@@@@@@@@\n@@_Hibernation Failed_@@@\n@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		/* enhanced error handling */
#ifdef CONFIG_TOI_ENHANCE
		if (toi_hibernate_fatalerror()) {
			kernel_power_off();
			kernel_halt();
			BUG();
		}
#endif
		if (hibernation_failed_action == HIB_FAILED_TO_SHUTDOWN) {
			kernel_power_off();
			kernel_halt();
			BUG();
		} else if (hibernation_failed_action == HIB_FAILED_TO_S2RAM) {
			hib_warn("hibernation failed: so changing state(%d->%d) err(%d)\n",
				 *autosleep_state, PM_SUSPEND_MEM, err);
			if (++hib_failed_cnt >= MAX_HIB_FAILED_CNT)
				hibernation_failed_action = HIB_FAILED_TO_SHUTDOWN;
			hibernate_recover();
			*autosleep_state = PM_SUSPEND_MEM;
			pm_wake_lock("IPOD_HIB_WAKELOCK");
			system_is_hibernating = false;
		} else {
			hib_err("@@@@@@@@@@@@@@@@@@\n@_FATAL ERROR !!!_\n@@@@@@@@@@@@@@@@@@@\n");
			BUG();
		}
	} else {
		if (hybrid_sleep_mode()) {
			hib_warn("hybrid sleep mode so changing state(%d->%d)\n", *autosleep_state,
				 PM_SUSPEND_MEM);
			*autosleep_state = PM_SUSPEND_MEM;	/* continue suspend to ram if hybrid sleep mode */
		} else {
			hib_warn("hibernation succeeded: so changing state(%d->%d) err(%d)\n",
				 *autosleep_state, PM_SUSPEND_ON, err);
			hibernate_restore();
			*autosleep_state = PM_SUSPEND_ON;	/* if this is not set, it will recursively do hibernating!! */
		}
		hib_failed_cnt = 0;
		pm_wake_lock("IPOD_HIB_WAKELOCK");
		system_is_hibernating = false;
	}

	return err;
}
EXPORT_SYMBOL(mtk_hibernate_via_autosleep);

/* called by echo "disk" > /sys/power/state */
int mtk_hibernate(void)
{
	int err = 0;

	hib_log("entering hibernation\n");
	err = hibernate();
	if (err) {
		hib_warn
		    ("@@@@@@@@@@@@@@@@@@@@@@@@@\n@@_Hibernation Failed_@@@\n@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		/* enhanced error handling */
#ifdef CONFIG_TOI_ENHANCE
		if (toi_hibernate_fatalerror()) {
			kernel_power_off();
			kernel_halt();
			BUG();
		}
#endif
		if (hibernation_failed_action == HIB_FAILED_TO_SHUTDOWN) {
			kernel_power_off();
			kernel_halt();
			BUG();
		} else if (hibernation_failed_action == HIB_FAILED_TO_S2RAM) {
			hib_warn("hibernation failed, suspend to ram instead!\n");
			if (++hib_failed_cnt >= MAX_HIB_FAILED_CNT)
				hibernation_failed_action = HIB_FAILED_TO_SHUTDOWN;
			hibernate_recover();
			pm_wake_lock("IPOD_HIB_WAKELOCK");
			system_is_hibernating = false;
		} else {
			hib_err("@@@@@@@@@@@@@@@@@@\n@_FATAL ERROR !!!_\n@@@@@@@@@@@@@@@@@@@\n");
			BUG();
		}
	} else {
		if (!hybrid_sleep_mode()) {
			hibernate_restore();
		}
		hib_failed_cnt = 0;
		pm_wake_lock("IPOD_HIB_WAKELOCK");
		system_is_hibernating = false;
	}

	return err;
}
EXPORT_SYMBOL(mtk_hibernate);

#define HIB_PAGE_FREE_DELTA ((60*1024*1024) >> (PAGE_SHIFT))
int bad_memory_status(void)
{
	struct zone *zone;
	unsigned long free_pages, min_pages;

	for_each_populated_zone(zone) {
		if (!strcmp(zone->name, "Normal")) {
			free_pages = zone_page_state(zone, NR_FREE_PAGES);
			min_pages = min_wmark_pages(zone);
			if (free_pages < (min_pages + HIB_PAGE_FREE_DELTA)) {
				hib_warn("abort hibernate due to %s memory status: (%lu:%lu)\n",
					 zone->name, free_pages, min_pages);
				return -1;
			} else {
				hib_warn("%s memory status: (%lu:%lu)\n", zone->name, free_pages,
					 min_pages);
			}
		}
	}
	return 0;
}

int pre_hibernate(void)
{
	int err = 0;

	hib_ftrace_buffer(0);

	/* check free memory status. */
	if (bad_memory_status()) {
		err = -1;
		hib_ftrace_buffer(1);
		goto ERR;
	}

	/* Adding userspace program stuffs here before hibernation start */
	/* ... */
	/* end of adding userspace program stuffs */

	/* CAUTION: put any stuff of actions before this line !! */
	if (!err) {
		/* flag to prevent suspend to ram */
		system_is_hibernating = true;
		/* hotplug disable */
		hib_hotplug_mode(0);
	}

 ERR:
	pm_wake_unlock("IPOD_HIB_WAKELOCK");
	return err;
}
EXPORT_SYMBOL(pre_hibernate);

int mtk_hibernate_abort(void)
{
#ifdef CONFIG_TOI_ENHANCE
	toi_abort_hibernate();
#endif
	return 0;
}
EXPORT_SYMBOL(mtk_hibernate_abort);
