/*
 * drivers/input/touchscreen/doubletap2wake.c
 *
 *
 * Copyright (c) 2013, Dennis Rassmann <showp1984@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/input/doubletap2wake.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#ifdef CONFIG_POWERSUSPEND
#include <linux/powersuspend.h>
#endif
#include <linux/hrtimer.h>
#include <asm-generic/cputime.h>

#ifdef CONFIG_PSENSOR_ONDEMAND_STATE
#include <linux/input/ltr553.h>
extern int ltr553_ps_ondemand_state (void);
#endif

/* uncomment since no touchscreen defines android touch, do that here */
//#define ANDROID_TOUCH_DECLARED

/* if Sweep2Wake is compiled it will already have taken care of this */
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
#define ANDROID_TOUCH_DECLARED
#endif

/* Version, author, desc, etc */
#define DRIVER_AUTHOR "Dennis Rassmann <showp1984@gmail.com>"
#define DRIVER_DESCRIPTION "Doubletap2wake for almost any device"
#define DRIVER_VERSION "1.0"
#define LOGTAG "[doubletap2wake]: "

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPLv2");

/* Tuneables */
#define DT2W_DEFAULT		0

#define DT2W_PWRKEY_DUR		60
#define DT2W_TIME		700

#define DT2W_OFF 0
#define DT2W_ON 1

/* Resources */
int dt2w_switch = DT2W_DEFAULT;
bool dt2w_scr_suspended = false;
bool in_phone_call = false;
int dt2w_sent_play_pause = 0;
int dt2w_feather = 200, dt2w_feather_w = 1;
#ifdef CONFIG_PSENSOR_ONDEMAND_STATE
int dtw2_psensor_state = LTR553_ON_DEMAND_RESET;
#endif
static cputime64_t tap_time_pre = 0;
static int touch_x = 0, touch_y = 0, touch_nr = 0, x_pre = 0, y_pre = 0;
static bool touch_x_called = false, touch_y_called = false, touch_cnt = false;
static bool exec_count = true;
static struct input_dev * doubletap2wake_pwrdev;
static DEFINE_MUTEX(pwrkeyworklock);
static struct workqueue_struct *dt2w_input_wq;
static struct work_struct dt2w_input_work;

/* Read cmdline for dt2w */
static int __init read_dt2w_cmdline(char *dt2w)
{
	if (strcmp(dt2w, "1") == 0) {
		pr_info("[cmdline_dt2w]: DoubleTap2Wake enabled. | dt2w='%s'\n", dt2w);
		dt2w_switch = 1;
	} else if (strcmp(dt2w, "0") == 0) {
		pr_info("[cmdline_dt2w]: DoubleTap2Wake disabled. | dt2w='%s'\n", dt2w);
		dt2w_switch = 0;
	} else {
		pr_info("[cmdline_dt2w]: No valid input found. Going with default: | dt2w='%u'\n", dt2w_switch);
	}
	return 1;
}
__setup("dt2w=", read_dt2w_cmdline);

static int __init read_dt2w_feather_cmdline(char *feather)
{
	if (strcmp(feather, "1") == 0)
		dt2w_feather_w = 1;
	else if (strcmp(feather, "2") == 0)
		dt2w_feather_w = 2;
	else if (strcmp(feather, "3") == 0)
		dt2w_feather_w = 3;
	else {
		pr_info("[dt2w_feather]: Input sensitivity not set. Going with default. | feather='%s'\n", feather);
		dt2w_feather_w = 1;
	}
	pr_info("[dt2w_feather]: Input sensitivity set. | feather='%s'\n", feather);
	return 1;
}
__setup("feather=", read_dt2w_feather_cmdline);

/* reset on finger release */
static void doubletap2wake_reset(void) {
	exec_count = true;
	touch_nr = 0;
	tap_time_pre = 0;
	x_pre = 0;
	y_pre = 0;
}

/* PowerKey work func */
static void doubletap2wake_presspwr(struct work_struct * doubletap2wake_presspwr_work) {
#ifdef CONFIG_PSENSOR_ONDEMAND_STATE
	if (dtw2_psensor_state == LTR553_ON_DEMAND_COVERED) {
	    DT2W_PRINFO("%s:%d -Proximity Sensor is covered, dt2w is ignored\n",
		    __func__, __LINE__);
		dtw2_psensor_state = LTR553_ON_DEMAND_RESET;
		return;
	}
	dtw2_psensor_state = LTR553_ON_DEMAND_RESET;
	DT2W_PRINFO("%s:%d -Proximity Sensor is not covered, dt2w can wakeup device\n",
		__func__, __LINE__);
#endif

	if (!mutex_trylock(&pwrkeyworklock))
                return;
	input_event(doubletap2wake_pwrdev, EV_KEY, KEY_POWER, 1);
	input_event(doubletap2wake_pwrdev, EV_SYN, 0, 0);
	msleep(DT2W_PWRKEY_DUR);
	input_event(doubletap2wake_pwrdev, EV_KEY, KEY_POWER, 0);
	input_event(doubletap2wake_pwrdev, EV_SYN, 0, 0);
	msleep(DT2W_PWRKEY_DUR);
        mutex_unlock(&pwrkeyworklock);
	return;
}
static DECLARE_WORK(doubletap2wake_presspwr_work, doubletap2wake_presspwr);

/* PowerKey trigger */
static void doubletap2wake_pwrtrigger(void) {
#ifdef CONFIG_PSENSOR_ONDEMAND_STATE
	/*
	 * Prema Chand Alugu (premaca@gmail.com)
	 * check the proximity sensor on demand.
	 * The returned state should be checked when the
	 * dt2w is actually performed.
	 */
	dtw2_psensor_state = ltr553_ps_ondemand_state();
#endif
	schedule_work(&doubletap2wake_presspwr_work);
        return;
}

/* unsigned */
static unsigned int calc_feather(int coord, int prev_coord) {
	int calc_coord = 0;
	calc_coord = coord-prev_coord;
	if (calc_coord < 0)
		calc_coord = calc_coord * (-1);
	return calc_coord;
}

/* init a new touch */
static void new_touch(int x, int y) {
	tap_time_pre = ktime_to_ms(ktime_get_real());
	x_pre = x;
	y_pre = y;
	touch_nr++;
}

/* Doubletap2wake main function */
static void detect_doubletap2wake(int x, int y, bool st)
{
        bool single_touch = st;
        DT2W_PRINFO(LOGTAG"x,y(%4d,%4d) single:%s dt2w_switch:%d exec_count:%s "
		"touch_cnt:%s touch_nr=%d dt2w_feather_w:%d\n",
                x, y, (single_touch) ? "true" : "false", dt2w_switch,
                (exec_count) ? "true" : "false", (touch_cnt) ? "true" : "false",
		touch_nr, dt2w_feather_w);
	if (dt2w_feather_w == 2)
		dt2w_feather = 100;
	else if (dt2w_feather_w == 3)
		dt2w_feather = 40;
	else
		dt2w_feather = 200;
	if ((single_touch) && (dt2w_switch > 0) && (exec_count) && (touch_cnt)) {
		touch_cnt = false;
		if (touch_nr == 0) {
			new_touch(x, y);
		} else if (touch_nr == 1) {
			if ((calc_feather(x, x_pre) < dt2w_feather) &&
			    (calc_feather(y, y_pre) < dt2w_feather) &&
			    ((ktime_to_ms(ktime_get_real())-tap_time_pre) < DT2W_TIME)) {
				touch_nr++;
				DT2W_PRINFO(LOGTAG"touch_nr is now %d\n",touch_nr);
			} else {
				doubletap2wake_reset();
				new_touch(x, y);
				DT2W_PRINFO(LOGTAG"feather/time check failed, "
					"reset&newtouch, touch_nr=%d\n",touch_nr);
			}
		} else {
			doubletap2wake_reset();
			new_touch(x, y);
			DT2W_PRINFO(LOGTAG"touch_nr was more than 1, "
				"reset&newtouch, touch_nr=%d\n",touch_nr);
		}
		if ((touch_nr > 1)) {
			pr_info(LOGTAG"ON\n");
			exec_count = false;
			doubletap2wake_pwrtrigger();
			doubletap2wake_reset();
		}
	}
}

static void dt2w_input_callback(struct work_struct *unused) {

	detect_doubletap2wake(touch_x, touch_y, true);

	return;
}

static void dt2w_input_event(struct input_handle *handle, unsigned int type,
				unsigned int code, int value) {
	DT2W_PRINFO("doubletap2wake: code: %s|%u, val: %i\n",
		((code==ABS_MT_POSITION_X) ? "X" :
		(code==ABS_MT_POSITION_Y) ? "Y" :
		(code==ABS_MT_TRACKING_ID) ? "ID" :
		"undef"), code, value);
	if (in_phone_call)
		return;

	if (!dt2w_scr_suspended)
		return;

	if (code == ABS_MT_SLOT) {
		doubletap2wake_reset();
		return;
	}

	if (code == ABS_MT_TRACKING_ID && value == -1) {
		touch_cnt = true;
		if ((!touch_x_called) || (!touch_y_called)) {
		    return;
		}
	}

	if (code == ABS_MT_POSITION_X) {
		touch_x = value;
		touch_x_called = true;
	}

	if (code == ABS_MT_POSITION_Y) {
		touch_y = value;
		touch_y_called = true;
	}

	if ((touch_x_called || touch_y_called) && (touch_cnt)) {
		touch_x_called = false;
		touch_y_called = false;
		queue_work_on(0, dt2w_input_wq, &dt2w_input_work);
	}
}

static int input_dev_filter(struct input_dev *dev) {
	if (strstr(dev->name, "touch") ||
	    strstr(dev->name, "ft5x06")) {
		return 0;
	} else {
		return 1;
	}
}

static int dt2w_input_connect(struct input_handler *handler,
				struct input_dev *dev, const struct input_device_id *id) {
	struct input_handle *handle;
	int error;

	if (input_dev_filter(dev))
		return -ENODEV;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "dt2w";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void dt2w_input_disconnect(struct input_handle *handle) {
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id dt2w_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler dt2w_input_handler = {
	.event		= dt2w_input_event,
	.connect	= dt2w_input_connect,
	.disconnect	= dt2w_input_disconnect,
	.name		= "dt2w_inputreq",
	.id_table	= dt2w_ids,
};


#ifdef CONFIG_POWERSUSPEND
static void dt2w_power_suspend(struct power_suspend *h) {
	dt2w_scr_suspended = true;
}

static void dt2w_power_resume(struct power_suspend *h) {
	dt2w_scr_suspended = false;
}

static struct power_suspend dt2w_power_suspend_handler = {
	.suspend = dt2w_power_suspend,
	.resume = dt2w_power_resume,
};
#endif

/*
 * SYSFS stuff below here
 */
static ssize_t dt2w_doubletap2wake_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%d\n", dt2w_switch);

	return count;
}

static ssize_t dt2w_doubletap2wake_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int new_dt2w_switch;

	if (!sscanf(buf, "%du", &new_dt2w_switch))
		return -EINVAL;

	if (new_dt2w_switch == dt2w_switch)
		return count;

	switch (new_dt2w_switch) {
		case DT2W_OFF :
		case DT2W_ON :
			dt2w_switch = new_dt2w_switch;
			/* through 'adb shell' or by other means, if the toggle
			 * is done several times, 0-to-1, 1-to-0, we need to
			 * inform the toggle correctly
			 */
			pr_info("[dump_dt2w]: DoubleTap2Wake toggled. | "
					"dt2w='%d' \n", dt2w_switch);
			return count;
		default:
			return -EINVAL;
	}

	/* We should never get here */
	return -EINVAL;
}

static DEVICE_ATTR(doubletap2wake, 0666,
	dt2w_doubletap2wake_show, dt2w_doubletap2wake_dump);

static ssize_t dt2w_feather_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%d\n", dt2w_feather_w);

	return count;
}

static ssize_t dt2w_feather_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] >= '1' && buf[0] <= '3' && buf[1] == '\n') {
		if (dt2w_feather_w != buf[0] - '0')
			dt2w_feather_w = buf[0] - '0';
	} else
		dt2w_feather_w = '1';
	return count;
}

static DEVICE_ATTR(doubletap2wake_feather, (S_IWUSR|S_IRUGO),
	dt2w_feather_show, dt2w_feather_dump);

static ssize_t dt2w_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%s\n", DRIVER_VERSION);

	return count;
}

static ssize_t dt2w_version_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(doubletap2wake_version, (S_IWUSR|S_IRUGO),
	dt2w_version_show, dt2w_version_dump);

/*
 * INIT / EXIT stuff below here
 */
#ifdef ANDROID_TOUCH_DECLARED
extern struct kobject *android_touch_kobj;
#else
struct kobject *android_touch_kobj;
EXPORT_SYMBOL_GPL(android_touch_kobj);
#endif
static int __init doubletap2wake_init(void)
{
	int rc = 0;

	doubletap2wake_pwrdev = input_allocate_device();
	if (!doubletap2wake_pwrdev) {
		pr_err("Can't allocate suspend autotest power button\n");
		goto err_alloc_dev;
	}

	input_set_capability(doubletap2wake_pwrdev, EV_KEY, KEY_POWER);
	doubletap2wake_pwrdev->name = "dt2w_pwrkey";
	doubletap2wake_pwrdev->phys = "dt2w_pwrkey/input0";

	rc = input_register_device(doubletap2wake_pwrdev);
	if (rc) {
		pr_err("%s: input_register_device err=%d\n", __func__, rc);
		goto err_input_dev;
	}

	dt2w_input_wq = create_workqueue("dt2wiwq");
	if (!dt2w_input_wq) {
		pr_err("%s: Failed to create dt2wiwq workqueue\n", __func__);
		return -EFAULT;
	}
	INIT_WORK(&dt2w_input_work, dt2w_input_callback);
	rc = input_register_handler(&dt2w_input_handler);
	if (rc)
		pr_err("%s: Failed to register dt2w_input_handler\n", __func__);

#ifdef CONFIG_POWERSUSPEND
	register_power_suspend(&dt2w_power_suspend_handler);
#endif

#ifndef ANDROID_TOUCH_DECLARED
	android_touch_kobj = kobject_create_and_add("android_touch", NULL) ;
	if (android_touch_kobj == NULL) {
		pr_warn("%s: android_touch_kobj create_and_add failed\n", __func__);
	}
#endif
	rc = sysfs_create_file(android_touch_kobj, &dev_attr_doubletap2wake.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for doubletap2wake\n", __func__);
	}
	rc = sysfs_create_file(android_touch_kobj, &dev_attr_doubletap2wake_version.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for doubletap2wake_version\n", __func__);
	}
	rc = sysfs_create_file(android_touch_kobj, &dev_attr_doubletap2wake_feather.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for doubletap2wake_feather\n", __func__);
	}

err_input_dev:
	input_free_device(doubletap2wake_pwrdev);
err_alloc_dev:
	pr_info(LOGTAG"%s done\n", __func__);

	return 0;
}

static void __exit doubletap2wake_exit(void)
{
#ifndef ANDROID_TOUCH_DECLARED
	kobject_del(android_touch_kobj);
#endif
	input_unregister_handler(&dt2w_input_handler);
	destroy_workqueue(dt2w_input_wq);
	input_unregister_device(doubletap2wake_pwrdev);
	input_free_device(doubletap2wake_pwrdev);
	return;
}

module_init(doubletap2wake_init);
module_exit(doubletap2wake_exit);
