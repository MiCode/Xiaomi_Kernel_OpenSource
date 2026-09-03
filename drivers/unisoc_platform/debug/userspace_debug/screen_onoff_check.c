// SPDX-License-Identifier: GPL-2.0
// SPDX-FileCopyrightText: 2023 Unisoc (Shanghai) Technologies Co., Ltd

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <uapi/linux/sched/types.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-screen_check: " fmt

struct screen_onoff_checker {
	struct device *dev;
	struct mutex lock;
	struct task_struct *task;
	struct backlight_device *bl_dev;
	struct device_node *np;
	wait_queue_head_t wait_q;
	u32 timeout_ms;
	u32 loop_interval_ms;
	char uevent[2][32];
	unsigned int seq;
	unsigned long press_jiffies;
	const char *powerkey_source;
};

static struct screen_onoff_checker timeout_checker;

static ssize_t timeout_event_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t ret;

	mutex_lock(&timeout_checker.lock);
	ret = snprintf(buf, sizeof(timeout_checker.uevent), "%s %s",
		       timeout_checker.uevent[0], timeout_checker.uevent[1]);
	mutex_unlock(&timeout_checker.lock);
	return ret;
}

static DEVICE_ATTR_RO(timeout_event);

static struct attribute *screen_checker_attrs[] = {
	&dev_attr_timeout_event.attr,
	NULL,
};

static const struct attribute_group screen_checker_group = {
	.attrs = screen_checker_attrs,
};

static int screen_onoff_checker_thread(void *data)
{
	struct sched_param param = {.sched_priority = 50};
	struct backlight_device *bl_dev;
	unsigned int last_seq, loop, max_loop, last_jiffies;
	bool screen_last, screen_now, long_pressed;
	int brightness;
	char *envp[3] = {NULL, NULL, NULL};

	envp[0] = timeout_checker.uevent[0];
	envp[1] = timeout_checker.uevent[1];

	sched_setscheduler(current, SCHED_RR, &param);

	bl_dev = timeout_checker.bl_dev;

	while (!kthread_should_stop()) {
		// read seq without lock for fast access
		last_seq = timeout_checker.seq;
		last_jiffies = timeout_checker.press_jiffies;
		long_pressed = false;
		wait_event_interruptible(timeout_checker.wait_q,
					 (timeout_checker.seq != last_seq));

		brightness = bl_dev->props.brightness;
		screen_last = (brightness == 0);
		screen_now = screen_last;

		// check brightness
		loop = 0;
		max_loop = timeout_checker.timeout_ms / timeout_checker.loop_interval_ms;

		while (screen_now == screen_last && loop < max_loop) {
			msleep(timeout_checker.loop_interval_ms);
			brightness = bl_dev->props.brightness;
			// long press: 500ms
			if (last_jiffies == timeout_checker.press_jiffies &&
			    (loop * timeout_checker.loop_interval_ms) >= 500) {
				long_pressed = true;
			}
			screen_now = (brightness == 0);
			loop++;
		}

		if (loop >= max_loop) {
			mutex_lock(&timeout_checker.lock);
			snprintf(envp[0], 32,
				 "TIMEOUT=SCREEN_%s%s",
				 screen_last ? "ON" : "OFF", long_pressed ? "_L" : "");
			snprintf(envp[1], 32, "SEQ=%d", timeout_checker.seq);
			mutex_unlock(&timeout_checker.lock);

			// notify user space the new event
			if (brightness > 0 && long_pressed == true) {
				pr_err("screen on and long pressed happen.\n");
			} else {
				pr_err("report screen onoff timeout event:%s %s\n",
					envp[0], envp[1]);
				kobject_uevent_env(&timeout_checker.dev->kobj, KOBJ_CHANGE, envp);
			}
		}
	}

	mutex_lock(&timeout_checker.lock);
	timeout_checker.task = NULL;
	mutex_unlock(&timeout_checker.lock);

	return 0;
}

void wakeup_screen_onoff_checker(void)
{
	struct task_struct *task;
	struct device_node *bl_np = NULL;

	if (!timeout_checker.bl_dev && timeout_checker.np) {
		bl_np = of_parse_phandle(timeout_checker.np, "sprd,backlight", 0);
		if (bl_np)
			timeout_checker.bl_dev = of_find_backlight_by_node(bl_np);
	}

	if (!timeout_checker.bl_dev) {
		pr_err("bl_dev is null, skip check, np:%p, bl_np:%p!!\n",
			timeout_checker.np, bl_np);
		return;
	}

	mutex_lock(&timeout_checker.lock);
	task = timeout_checker.task;
	mutex_unlock(&timeout_checker.lock);

	if (!task) {
		const char *name = "sceen-checker";

		task = kthread_run(screen_onoff_checker_thread,
				     NULL,
				     name);
		if (IS_ERR_OR_NULL(task)) {
			pr_err("fail to create %s.\n", name);
			task = NULL;
			return;
		}
	}

	mutex_lock(&timeout_checker.lock);
	timeout_checker.seq++;
	timeout_checker.task = task;
	mutex_unlock(&timeout_checker.lock);

	wake_up(&timeout_checker.wait_q);
}

static void screen_checker_event(struct input_handle *handle,
				 unsigned int type, unsigned int code, int value)
{
	// wakeup screen_onoff_checker thread when powerkey pressed
	if (type == EV_KEY && code == 116 && value == 1)
		wakeup_screen_onoff_checker();
}

static int screen_checker_connect(struct input_handler *handler,
				  struct input_dev *dev,
				  const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "screen_onoff_checker";

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	return 0;

 err_unregister_handle:
	input_unregister_handle(handle);
 err_free_handle:
	kfree(handle);
	return error;
}

static void screen_checker_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static bool screen_checker_match(struct input_handler *handler, struct input_dev *dev)
{
	if (timeout_checker.powerkey_source &&
	    !strcmp(dev->name, timeout_checker.powerkey_source))
		return true;

	return false;
}

static const struct input_device_id screen_checker_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{},
};

static struct input_handler screen_checker_handler = {
	.name = "screen_onoff_checker",
	.event = screen_checker_event,
	.connect = screen_checker_connect,
	.disconnect = screen_checker_disconnect,
	.id_table = screen_checker_ids,
	.match = screen_checker_match,
};

static const struct of_device_id screen_checker_match_table[] = {
	{.compatible = "sprd,screen_onoff_check"},
	{ },
};

static int screen_checker_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *bl_np;
	int ret;
	u32 timeout_ms = 5000;
	u32 loop_interval_ms = 50;

	bl_np = of_parse_phandle(np, "sprd,backlight", 0);
	timeout_checker.np = np;

	if (bl_np) {
		timeout_checker.bl_dev = of_find_backlight_by_node(bl_np);
		dev_err(dev, "checker: bl_dev=%p\n", timeout_checker.bl_dev);
	}

	if (of_property_read_u32(np, "timeout-ms", &timeout_ms))
		return -ENOMEM;

	if (timeout_ms < 2000)
		timeout_ms = 2000;

	if (of_property_read_u32(np, "loop-interval-ms", &loop_interval_ms))
		return -ENOMEM;

	if (loop_interval_ms < 10)
		loop_interval_ms = 10;

	timeout_checker.timeout_ms = timeout_ms;
	timeout_checker.loop_interval_ms = loop_interval_ms;

	if (of_property_read_string(np, "powerkey-source",
				    &timeout_checker.powerkey_source))
		timeout_checker.powerkey_source = NULL;

	ret = sysfs_create_group(&(dev->kobj), &screen_checker_group);
	if (ret) {
		dev_err(dev, "create checker attr failed, ret=%d\n", ret);
		return ret;
	}

	timeout_checker.dev = dev;
	timeout_checker.task = NULL;
	timeout_checker.bl_dev = NULL;
	timeout_checker.seq = 0;

	init_waitqueue_head(&timeout_checker.wait_q);

	ret = input_register_handler(&screen_checker_handler);
	if (ret) {
		dev_err(dev, "failed to register input_handler, ret=%d\n", ret);
		sysfs_remove_group(&(dev->kobj), &screen_checker_group);
		return ret;
	}

	wakeup_screen_onoff_checker();

	return ret;
}

static int screen_checker_remove(struct platform_device *pdev)
{
	struct screen_checker_device *screen_checker = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	if (screen_checker)
		platform_set_drvdata(pdev, NULL);

	input_unregister_handler(&screen_checker_handler);
	sysfs_remove_group(&(dev->kobj), &screen_checker_group);
	if (timeout_checker.task)
		kthread_stop(timeout_checker.task);
	return 0;
}

static struct platform_driver screen_checker_driver = {
	.driver = {
		.name = "screen_checker",
		.of_match_table = screen_checker_match_table,
	},
	.probe = screen_checker_probe,
	.remove = screen_checker_remove,
};

static int __init screen_checker_init(void)
{
	return platform_driver_register(&screen_checker_driver);
}

static void __exit screen_checker_exit(void)
{
	platform_driver_unregister(&screen_checker_driver);
}

module_init(screen_checker_init);
module_exit(screen_checker_exit);

MODULE_DESCRIPTION("screen onoff timeout checker");
MODULE_LICENSE("GPL");
