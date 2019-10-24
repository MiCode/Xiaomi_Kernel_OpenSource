/* drivers/rtc/alarm-dev.c
 *
 * Copyright (C) 2007-2009 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/time.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/rtc.h>
#include <linux/alarmtimer.h>
#include "android_alarm.h"
#include <linux/ioctl.h>
#define LOG_MYTAG	"Power/Alarm"

#define ANDROID_ALARM_PRINT_INFO BIT(0)
#define ANDROID_ALARM_PRINT_IO BIT(1)
#define ANDROID_ALARM_PRINT_INT BIT(2)

static int debug_mask = ANDROID_ALARM_PRINT_INFO;
module_param_named(debug_mask, debug_mask, int, 0664);

#define alarm_dbg(debug_level_mask, fmt, args...) \
do {									\
	if (debug_mask & ANDROID_ALARM_PRINT_##debug_level_mask)	\
		pr_debug(LOG_MYTAG fmt, ##args); \
} while (0)

#define ANDROID_ALARM_WAKEUP_MASK ( \
	ANDROID_ALARM_RTC_WAKEUP_MASK | \
	ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP_MASK)

static int alarm_opened;
static DEFINE_SPINLOCK(alarm_slock);
static struct wakeup_source alarm_wake_lock;
static DECLARE_WAIT_QUEUE_HEAD(alarm_wait_queue);
static u32 alarm_pending;
static u32 alarm_enabled;
static u32 wait_pending;

struct devalarm {
	union {
		struct hrtimer hrt;
		struct alarm alrm;
	} u;
	enum android_alarm_type type;
};

static struct devalarm alarms[ANDROID_ALARM_TYPE_COUNT];

static int is_wakeup(enum android_alarm_type type)
{
	return (type == ANDROID_ALARM_RTC_WAKEUP ||
		type == ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP);
}

static void devalarm_start(struct devalarm *alrm, ktime_t exp)
{
	if (is_wakeup(alrm->type))
		alarm_start(&alrm->u.alrm, exp);
	else
		hrtimer_start(&alrm->u.hrt, exp, HRTIMER_MODE_ABS);
}

static int devalarm_try_to_cancel(struct devalarm *alrm)
{
	if (is_wakeup(alrm->type))
		return alarm_try_to_cancel(&alrm->u.alrm);
	return hrtimer_try_to_cancel(&alrm->u.hrt);
}

static void devalarm_cancel(struct devalarm *alrm)
{
	if (is_wakeup(alrm->type))
		alarm_cancel(&alrm->u.alrm);
	else
		hrtimer_cancel(&alrm->u.hrt);
}

void alarm_set_power_on(struct timespec new_pwron_time, bool logo)
{
	unsigned long pwron_time;
	struct rtc_wkalrm alm;
	struct rtc_device *alarm_rtc_dev;

	alarm_dbg(INFO, "alarm set power on\n");

#ifdef RTC_PWRON_SEC
	/* round down the second */
	new_pwron_time.tv_sec = (new_pwron_time.tv_sec / 60) * 60;
#endif
	if (new_pwron_time.tv_sec > 0) {
		pwron_time = new_pwron_time.tv_sec;
#ifdef RTC_PWRON_SEC
		pwron_time += RTC_PWRON_SEC;
#endif
		alm.enabled = (logo ? 3 : 2);
	} else {
		pwron_time = 0;
		alm.enabled = 4;
	}
	alarm_rtc_dev = alarmtimer_get_rtcdev();
	rtc_time_to_tm(pwron_time, &alm.time);
	rtc_set_alarm(alarm_rtc_dev, &alm);
	rtc_set_alarm_poweron(alarm_rtc_dev, &alm);
}

void alarm_get_power_on(struct rtc_wkalrm *alm)
{
	if (!alm)
		return;

	memset(alm, 0, sizeof(struct rtc_wkalrm));
#ifndef CONFIG_MTK_FPGA
	rtc_read_pwron_alarm(alm);
#endif
}

static void alarm_clear(enum android_alarm_type alarm_type, struct timespec *ts)
{
	u32 alarm_type_mask = 1U << alarm_type;
	unsigned long flags;

	alarm_dbg(IO, "alarm %d clear\n", alarm_type);
	if (alarm_type == ANDROID_ALARM_POWER_ON ||
	    alarm_type == ANDROID_ALARM_POWER_ON_LOGO) {
		ts->tv_sec = 0;
		alarm_set_power_on(*ts, false);
		return;
	}
	spin_lock_irqsave(&alarm_slock, flags);
	devalarm_try_to_cancel(&alarms[alarm_type]);
	if (alarm_pending) {
		alarm_pending &= ~alarm_type_mask;
		if (!alarm_pending && !wait_pending)
			__pm_relax(&alarm_wake_lock);
	}
	alarm_enabled &= ~alarm_type_mask;
	spin_unlock_irqrestore(&alarm_slock, flags);
}

static void alarm_set(enum android_alarm_type alarm_type, struct timespec *ts)
{
	u32 alarm_type_mask = 1U << alarm_type;
	unsigned long flags;

	pr_notice("alarm %d set %ld.%09ld\n", alarm_type,
		  ts->tv_sec, ts->tv_nsec);
	if (alarm_type == ANDROID_ALARM_POWER_ON) {
		alarm_set_power_on(*ts, false);
		return;
	}
	if (alarm_type == ANDROID_ALARM_POWER_ON_LOGO) {
		alarm_set_power_on(*ts, true);
		return;
	}

	spin_lock_irqsave(&alarm_slock, flags);
	alarm_enabled |= alarm_type_mask;
	devalarm_start(&alarms[alarm_type], timespec_to_ktime(*ts));
	spin_unlock_irqrestore(&alarm_slock, flags);
}

static int alarm_wait(void)
{
	unsigned long flags;
	int rv = 0;

	spin_lock_irqsave(&alarm_slock, flags);
	alarm_dbg(IO, "alarm wait\n");
	if (!alarm_pending && wait_pending) {
		__pm_relax(&alarm_wake_lock);
		wait_pending = 0;
	}
	spin_unlock_irqrestore(&alarm_slock, flags);

	rv = wait_event_interruptible(alarm_wait_queue, alarm_pending);
	if (rv)
		return rv;

	spin_lock_irqsave(&alarm_slock, flags);
	rv = alarm_pending;
	wait_pending = 1;
	alarm_pending = 0;
	spin_unlock_irqrestore(&alarm_slock, flags);

	return rv;
}

static int alarm_set_rtc(struct timespec *ts)
{
	struct rtc_time new_rtc_tm;
	struct rtc_device *rtc_dev;
	unsigned long flags;
	int rv = 0;

	rtc_time_to_tm(ts->tv_sec, &new_rtc_tm);
	pr_notice("set rtc %ld %ld - rtc %02d:%02d:%02d %02d/%02d/%04d\n",
		  ts->tv_sec, ts->tv_nsec,
		  new_rtc_tm.tm_hour, new_rtc_tm.tm_min,
		  new_rtc_tm.tm_sec, new_rtc_tm.tm_mon + 1,
		  new_rtc_tm.tm_mday, new_rtc_tm.tm_year + 1900);
	rtc_dev = alarmtimer_get_rtcdev();
	rv = do_settimeofday(ts);
	if (rv < 0)
		return rv;
	if (rtc_dev)
		rv = rtc_set_time(rtc_dev, &new_rtc_tm);

	spin_lock_irqsave(&alarm_slock, flags);
	alarm_pending |= ANDROID_ALARM_TIME_CHANGE_MASK;
	wake_up(&alarm_wait_queue);
	spin_unlock_irqrestore(&alarm_slock, flags);

	return rv;
}

static int alarm_get_time(enum android_alarm_type alarm_type,
			  struct timespec *ts)
{
	int rv = 0;

	switch (alarm_type) {
	case ANDROID_ALARM_RTC_WAKEUP:
	case ANDROID_ALARM_RTC:
		getnstimeofday(ts);
		break;
	case ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP:
	case ANDROID_ALARM_ELAPSED_REALTIME:
		get_monotonic_boottime(ts);
		break;
	case ANDROID_ALARM_SYSTEMTIME:
		ktime_get_ts(ts);
		break;
	case ANDROID_ALARM_POWER_ON:
	case ANDROID_ALARM_POWER_ON_LOGO:
		break;
	default:
		rv = -EINVAL;
	}
	return rv;
}

static long alarm_do_ioctl(struct file *file, unsigned int cmd,
			   struct timespec *ts, struct rtc_wkalrm *alm)
{
	int rv = 0;
	unsigned long flags;
	enum android_alarm_type alarm_type = ANDROID_ALARM_IOCTL_TO_TYPE(cmd);

	alarm_dbg(INFO, "%s cmd:%d type:%d (%lu)\n", __func__,
		  cmd, alarm_type, (uintptr_t)file->private_data);

	if (alarm_type >= ANDROID_ALARM_TYPE_COUNT &&
	    alarm_type != ANDROID_ALARM_POWER_ON &&
	    alarm_type != ANDROID_ALARM_POWER_ON_LOGO) {
		return -EINVAL;
	}

	if (ANDROID_ALARM_BASE_CMD(cmd) != ANDROID_ALARM_GET_TIME(0) &&
	    ANDROID_ALARM_BASE_CMD(cmd) != ANDROID_ALARM_SET_IPO(0) &&
	    ANDROID_ALARM_BASE_CMD(cmd) != ANDROID_ALARM_GET_POWER_ON_IPO) {
		if ((file->f_flags & O_ACCMODE) == O_RDONLY)
			return -EPERM;
		if (!file->private_data && cmd != ANDROID_ALARM_SET_RTC) {
			spin_lock_irqsave(&alarm_slock, flags);
			if (alarm_opened) {
				spin_unlock_irqrestore(&alarm_slock, flags);
				alarm_dbg(INFO, "%s EBUSY\n", __func__);
				file->private_data = NULL;
				return -EBUSY;
			}
			alarm_opened = 1;
			file->private_data = (void *)1;
			spin_unlock_irqrestore(&alarm_slock, flags);
			alarm_dbg(INFO, "%s opened\n", __func__);
		}
	}

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_CLEAR(0):
		alarm_clear(alarm_type, ts);
		break;
	case ANDROID_ALARM_SET(0):
		alarm_set(alarm_type, ts);
		break;
	case ANDROID_ALARM_SET_AND_WAIT(0):
		alarm_set(alarm_type, ts);
		/* fall though */
	case ANDROID_ALARM_WAIT:
		rv = alarm_wait();
		break;
	case ANDROID_ALARM_SET_IPO(0):
		alarm_set(alarm_type, ts);
		break;
	case ANDROID_ALARM_SET_AND_WAIT_IPO(0):
		alarm_set(alarm_type, ts);
		/* fall though */
	case ANDROID_ALARM_WAIT_IPO:
		rv = alarm_wait();
		break;
	case ANDROID_ALARM_SET_RTC:
		rv = alarm_set_rtc(ts);
		break;
	case ANDROID_ALARM_GET_TIME(0):
		rv = alarm_get_time(alarm_type, ts);
		break;
	case ANDROID_ALARM_GET_POWER_ON:
		alarm_get_power_on(alm);
		break;
	case ANDROID_ALARM_GET_POWER_ON_IPO:
		alarm_get_power_on(alm);
		break;

	default:
		rv = -EINVAL;
	}
	return rv;
}

static long alarm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct timespec ts = {0};
	struct rtc_wkalrm pwron_alm = {0};
	int rv;

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_SET_AND_WAIT(0):
	case ANDROID_ALARM_SET(0):
	case ANDROID_ALARM_SET_RTC:
	case ANDROID_ALARM_SET_IPO(0):
		if (copy_from_user(&ts, (void __user *)arg, sizeof(ts)))
			return -EFAULT;
		break;
	}

	rv = alarm_do_ioctl(file, cmd, &ts, &pwron_alm);
	if (rv)
		return rv;

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_GET_TIME(0):
		if (copy_to_user((void __user *)arg, &ts, sizeof(ts)))
			return -EFAULT;
		break;
	case ANDROID_ALARM_GET_POWER_ON:
	case ANDROID_ALARM_GET_POWER_ON_IPO:
		if (copy_to_user((void __user *)arg, &pwron_alm,
				 sizeof(struct rtc_wkalrm))) {
			rv = -EFAULT;
			return rv;
		}
		break;
	default:
		break;
	}

	return 0;
}

#ifdef CONFIG_COMPAT

static long alarm_compat_ioctl(struct file *file,
			       unsigned int cmd, unsigned long arg)
{
	struct timespec ts = {0};
	struct rtc_wkalrm pwron_alm = {0};
	int rv;

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_SET_AND_WAIT_COMPAT(0):
	case ANDROID_ALARM_SET_COMPAT(0):
	case ANDROID_ALARM_SET_RTC_COMPAT:
	case ANDROID_ALARM_SET_IPO_COMPAT(0):
		if (compat_get_timespec(&ts, (void __user *)arg))
			return -EFAULT;
		/* fall through */
	case ANDROID_ALARM_GET_TIME_COMPAT(0):
		cmd = ALARM_IOW(ANDROID_ALARM_IOCTL_NR(cmd),
				ANDROID_ALARM_IOCTL_TO_TYPE(cmd),
				struct timespec);
		break;
	}

	rv = alarm_do_ioctl(file, cmd, &ts, &pwron_alm);
	if (rv)
		return rv;

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_GET_TIME(0):	/* NOTE: we modified cmd above */
		if (compat_put_timespec(&ts, (void __user *)arg))
			return -EFAULT;
		break;

	case ANDROID_ALARM_GET_POWER_ON:
	case ANDROID_ALARM_GET_POWER_ON_IPO:
		if (copy_to_user((void __user *)arg, &pwron_alm,
				 sizeof(struct rtc_wkalrm))) {
			rv = -EFAULT;
			return rv;
		}
		break;
	}
	return 0;
}
#endif

static int alarm_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	alarm_dbg(INFO, "%s (%d:%d)\n", __func__, current->tgid, current->pid);
	return 0;
}

static int alarm_release(struct inode *inode, struct file *file)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&alarm_slock, flags);
	if (file->private_data) {
		for (i = 0; i < ANDROID_ALARM_TYPE_COUNT; i++) {
			u32 alarm_type_mask = 1U << i;

			if (alarm_enabled & alarm_type_mask) {
				alarm_dbg(INFO,
					  "%s: clear alarm, pending %d\n",
					  __func__,
					  !!(alarm_pending & alarm_type_mask));
				alarm_enabled &= ~alarm_type_mask;
			}
			spin_unlock_irqrestore(&alarm_slock, flags);
			devalarm_cancel(&alarms[i]);
			spin_lock_irqsave(&alarm_slock, flags);
		}
		if (alarm_pending | wait_pending) {
			if (alarm_pending)
				alarm_dbg(INFO, "%s: clear pending alarms %x\n",
					  __func__, alarm_pending);
			__pm_relax(&alarm_wake_lock);
			wait_pending = 0;
			alarm_pending = 0;
		}
		alarm_opened = 0;
	}
	spin_unlock_irqrestore(&alarm_slock, flags);
	alarm_dbg(INFO, "%s (%d:%d)(%lu)\n", __func__,
		  current->tgid, current->pid, (uintptr_t)file->private_data);
	return 0;
}

static void devalarm_triggered(struct devalarm *alarm)
{
	unsigned long flags;
	u32 alarm_type_mask = 1U << alarm->type;

	alarm_dbg(INT, "%s: type %d\n", __func__, alarm->type);
	spin_lock_irqsave(&alarm_slock, flags);
	if (alarm_enabled & alarm_type_mask) {
		__pm_wakeup_event(&alarm_wake_lock, 5000);	/* 5secs */
		alarm_enabled &= ~alarm_type_mask;
		alarm_pending |= alarm_type_mask;
		wake_up(&alarm_wait_queue);
	}
	spin_unlock_irqrestore(&alarm_slock, flags);
}

static enum hrtimer_restart devalarm_hrthandler(struct hrtimer *hrt)
{
	struct devalarm *devalrm = container_of(hrt, struct devalarm, u.hrt);

	alarm_dbg(INT, "%s\n", __func__);
	devalarm_triggered(devalrm);
	return HRTIMER_NORESTART;
}

static enum alarmtimer_restart devalarm_alarmhandler(struct alarm *alrm,
						     ktime_t now)
{
	struct devalarm *devalrm = container_of(alrm, struct devalarm, u.alrm);

	alarm_dbg(INT, "%s\n", __func__);
	devalarm_triggered(devalrm);
	return ALARMTIMER_NORESTART;
}

static const struct file_operations alarm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = alarm_ioctl,
	.open = alarm_open,
	.release = alarm_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = alarm_compat_ioctl,
#endif
};

static struct miscdevice alarm_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "alarm",
	.fops = &alarm_fops,
};

static int __init alarm_dev_init(void)
{
	int err;
	int i;

	err = misc_register(&alarm_device);
	if (err)
		return err;

	alarm_init(&alarms[ANDROID_ALARM_RTC_WAKEUP].u.alrm,
		   ALARM_REALTIME, devalarm_alarmhandler);
	hrtimer_init(&alarms[ANDROID_ALARM_RTC].u.hrt,
		     CLOCK_REALTIME, HRTIMER_MODE_ABS);
	alarm_init(&alarms[ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP].u.alrm,
		   ALARM_BOOTTIME, devalarm_alarmhandler);
	hrtimer_init(&alarms[ANDROID_ALARM_ELAPSED_REALTIME].u.hrt,
		     CLOCK_BOOTTIME, HRTIMER_MODE_ABS);
	hrtimer_init(&alarms[ANDROID_ALARM_SYSTEMTIME].u.hrt,
		     CLOCK_MONOTONIC, HRTIMER_MODE_ABS);

	for (i = 0; i < ANDROID_ALARM_TYPE_COUNT; i++) {
		alarms[i].type = i;
		if (!is_wakeup(i))
			alarms[i].u.hrt.function = devalarm_hrthandler;
	}

	wakeup_source_init(&alarm_wake_lock, "alarm");
	return 0;
}

static void __exit alarm_dev_exit(void)
{
	misc_deregister(&alarm_device);
	wakeup_source_trash(&alarm_wake_lock);
}

module_init(alarm_dev_init);
module_exit(alarm_dev_exit);
