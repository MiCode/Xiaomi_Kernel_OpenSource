/*
 *  H2W device detection driver.
 *
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2008 Google, Inc.
 *
 * Authors:
 *  Laurence Chen <Laurence_Chen@htc.com>
 *  Nick Pelly <npelly@google.com>
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

/*  For detecting HTC 2 Wire devices, such as wired headset.

    Logically, the H2W driver is always present, and H2W state (hi->state)
    indicates what is currently plugged into the H2W interface.

    When the headset is plugged in, CABLE_IN1 is pulled low. When the headset
    button is pressed, CABLE_IN2 is pulled low. These two lines are shared with
    the TX and RX (respectively) of UART3 - used for serial debugging.

    This headset driver keeps the CPLD configured as UART3 for as long as
    possible, so that we can do serial FIQ debugging even when the kernel is
    locked and this driver no longer runs. So it only configures the CPLD to
    GPIO while the headset is plugged in, and for 10ms during detection work.

    Unfortunately we can't leave the CPLD as UART3 while a headset is plugged
    in, UART3 is pullup on TX but the headset is pull-down, causing a 55 mA
    drain on sapphire.

    The headset detection work involves setting CPLD to GPIO, and then pulling
    CABLE_IN1 high with a stronger pullup than usual. A H2W headset will still
    pull this line low, whereas other attachments such as a serial console
    would get pulled up by this stronger pullup.

    Headset insertion/removal causes UEvent's to be sent, and
    /sys/class/switch/h2w/state to be updated.

    Button presses are interpreted as input event (KEY_MEDIA). Button presses
    are ignored if the headset is plugged in, so the buttons on 11 pin -> 3.5mm
    jack adapters do not work until a headset is plugged into the adapter. This
    is to avoid serial RX traffic causing spurious button press events.

    We tend to check the status of CABLE_IN1 a few more times than strictly
    necessary during headset detection, to avoid spurious headset insertion
    events caused by serial debugger TX traffic.
*/


#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <asm/atomic.h>
#include <mach/board.h>
#include <mach/vreg.h>
#include <asm/mach-types.h>
#include "board-sapphire.h"

#ifdef CONFIG_DEBUG_SAPPHIRE_H2W
#define H2W_DBG(fmt, arg...) printk(KERN_INFO "[H2W] %s " fmt "\n", __FUNCTION__, ## arg)
#else
#define H2W_DBG(fmt, arg...) do {} while (0)
#endif

static struct workqueue_struct *g_detection_work_queue;
static void detection_work(struct work_struct *work);
static DECLARE_WORK(g_detection_work, detection_work);
enum {
	NO_DEVICE	= 0,
	HTC_HEADSET	= 1,
};

enum {
	UART3		= 0,
	GPIO		= 1,
};

struct h2w_info {
	struct switch_dev sdev;
	struct input_dev *input;

	atomic_t btn_state;
	int ignore_btn;

	unsigned int irq;
	unsigned int irq_btn;

	struct hrtimer timer;
	ktime_t debounce_time;

	struct hrtimer btn_timer;
	ktime_t btn_debounce_time;
};
static struct h2w_info *hi;

static ssize_t sapphire_h2w_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(&hi->sdev)) {
	case NO_DEVICE:
		return sprintf(buf, "No Device\n");
	case HTC_HEADSET:
		return sprintf(buf, "Headset\n");
	}
	return -EINVAL;
}

static void configure_cpld(int route)
{
	H2W_DBG(" route = %s", route == UART3 ? "UART3" : "GPIO");
	switch (route) {
	case UART3:
		gpio_set_value(SAPPHIRE_GPIO_H2W_SEL0, 0);
		gpio_set_value(SAPPHIRE_GPIO_H2W_SEL1, 1);
		break;
	case GPIO:
		gpio_set_value(SAPPHIRE_GPIO_H2W_SEL0, 0);
		gpio_set_value(SAPPHIRE_GPIO_H2W_SEL1, 0);
		break;
	}
}

static void button_pressed(void)
{
	H2W_DBG("");
	atomic_set(&hi->btn_state, 1);
	input_report_key(hi->input, KEY_MEDIA, 1);
	input_sync(hi->input);
}

static void button_released(void)
{
	H2W_DBG("");
	atomic_set(&hi->btn_state, 0);
	input_report_key(hi->input, KEY_MEDIA, 0);
	input_sync(hi->input);
}

#ifdef CONFIG_MSM_SERIAL_DEBUGGER
extern void msm_serial_debug_enable(int);
#endif

static void insert_headset(void)
{
	unsigned long irq_flags;

	H2W_DBG("");

	switch_set_state(&hi->sdev, HTC_HEADSET);
	configure_cpld(GPIO);

#ifdef CONFIG_MSM_SERIAL_DEBUGGER
	msm_serial_debug_enable(false);
#endif


	/* On some non-standard headset adapters (usually those without a
	 * button) the btn line is pulled down at the same time as the detect
	 * line. We can check here by sampling the button line, if it is
	 * low then it is probably a bad adapter so ignore the button.
	 * If the button is released then we stop ignoring the button, so that
	 * the user can recover from the situation where a headset is plugged
	 * in with button held down.
	 */
	hi->ignore_btn = !gpio_get_value(SAPPHIRE_GPIO_CABLE_IN2);

	/* Enable button irq */
	local_irq_save(irq_flags);
	enable_irq(hi->irq_btn);
	local_irq_restore(irq_flags);

	hi->debounce_time = ktime_set(0, 20000000);  /* 20 ms */
}

static void remove_headset(void)
{
	unsigned long irq_flags;

	H2W_DBG("");

	switch_set_state(&hi->sdev, NO_DEVICE);
	configure_cpld(UART3);

	/* Disable button */
	local_irq_save(irq_flags);
	disable_irq(hi->irq_btn);
	local_irq_restore(irq_flags);

	if (atomic_read(&hi->btn_state))
		button_released();

	hi->debounce_time = ktime_set(0, 100000000);  /* 100 ms */
}

static void detection_work(struct work_struct *work)
{
	unsigned long irq_flags;
	int clk, cable_in1;

	H2W_DBG("");

	if (gpio_get_value(SAPPHIRE_GPIO_CABLE_IN1) != 0) {
		/* Headset not plugged in */
		if (switch_get_state(&hi->sdev) == HTC_HEADSET)
			remove_headset();
		return;
	}

	/* Something plugged in, lets make sure its a headset */

	/* Switch CPLD to GPIO to do detection */
	configure_cpld(GPIO);
	/* Disable headset interrupt while detecting.*/
	local_irq_save(irq_flags);
	disable_irq(hi->irq);
	local_irq_restore(irq_flags);

	/* Set GPIO_CABLE_IN1 as output high */
	gpio_direction_output(SAPPHIRE_GPIO_CABLE_IN1, 1);
	/* Delay 10ms for pin stable. */
	msleep(10);
	/* Save H2W_CLK */
	clk = gpio_get_value(SAPPHIRE_GPIO_H2W_CLK_GPI);
	/* Set GPIO_CABLE_IN1 as input */
	gpio_direction_input(SAPPHIRE_GPIO_CABLE_IN1);

	/* Restore IRQs */
	local_irq_save(irq_flags);
	enable_irq(hi->irq);
	local_irq_restore(irq_flags);

	cable_in1 = gpio_get_value(SAPPHIRE_GPIO_CABLE_IN1);

	if (cable_in1 == 0 && clk == 0) {
		if (switch_get_state(&hi->sdev) == NO_DEVICE)
			insert_headset();
	} else {
		configure_cpld(UART3);
		H2W_DBG("CABLE_IN1 was low, but not a headset "
			"(recent cable_in1 = %d, clk = %d)", cable_in1, clk);
	}
}

static enum hrtimer_restart button_event_timer_func(struct hrtimer *data)
{
	H2W_DBG("");

	if (switch_get_state(&hi->sdev) == HTC_HEADSET) {
		if (gpio_get_value(SAPPHIRE_GPIO_CABLE_IN2)) {
			if (hi->ignore_btn)
				hi->ignore_btn = 0;
			else if (atomic_read(&hi->btn_state))
				button_released();
		} else {
			if (!hi->ignore_btn && !atomic_read(&hi->btn_state))
				button_pressed();
		}
	}

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart detect_event_timer_func(struct hrtimer *data)
{
	H2W_DBG("");

	queue_work(g_detection_work_queue, &g_detection_work);
	return HRTIMER_NORESTART;
}

static irqreturn_t detect_irq_handler(int irq, void *dev_id)
{
	int value1, value2;
	int retry_limit = 10;

	H2W_DBG("");
	do {
		value1 = gpio_get_value(SAPPHIRE_GPIO_CABLE_IN1);
		set_irq_type(hi->irq, value1 ?
				IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);
		value2 = gpio_get_value(SAPPHIRE_GPIO_CABLE_IN1);
	} while (value1 != value2 && retry_limit-- > 0);

	H2W_DBG("value2 = %d (%d retries)", value2, (10-retry_limit));

	if ((switch_get_state(&hi->sdev) == NO_DEVICE) ^ value2) {
		if (switch_get_state(&hi->sdev) == HTC_HEADSET)
			hi->ignore_btn = 1;
		/* Do the rest of the work in timer context */
		hrtimer_start(&hi->timer, hi->debounce_time, HRTIMER_MODE_REL);
	}

	return IRQ_HANDLED;
}

static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
	int value1, value2;
	int retry_limit = 10;

	H2W_DBG("");
	do {
		value1 = gpio_get_value(SAPPHIRE_GPIO_CABLE_IN2);
		set_irq_type(hi->irq_btn, value1 ?
				IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);
		value2 = gpio_get_value(SAPPHIRE_GPIO_CABLE_IN2);
	} while (value1 != value2 && retry_limit-- > 0);

	H2W_DBG("value2 = %d (%d retries)", value2, (10-retry_limit));

	hrtimer_start(&hi->btn_timer, hi->btn_debounce_time, HRTIMER_MODE_REL);

	return IRQ_HANDLED;
}

#if defined(CONFIG_DEBUG_FS)
static void h2w_debug_set(void *data, u64 val)
{
	switch_set_state(&hi->sdev, (int)val);
}

static u64 h2w_debug_get(void *data)
{
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(h2w_debug_fops, h2w_debug_get, h2w_debug_set, "%llu\n");
static int __init h2w_debug_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("h2w", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("state", 0644, dent, NULL, &h2w_debug_fops);

	return 0;
}

device_initcall(h2w_debug_init);
#endif

static int sapphire_h2w_probe(struct platform_device *pdev)
{
	int ret;
	unsigned long irq_flags;

	printk(KERN_INFO "H2W: Registering H2W (headset) driver\n");
	hi = kzalloc(sizeof(struct h2w_info), GFP_KERNEL);
	if (!hi)
		return -ENOMEM;

	atomic_set(&hi->btn_state, 0);
	hi->ignore_btn = 0;

	hi->debounce_time = ktime_set(0, 100000000);  /* 100 ms */
	hi->btn_debounce_time = ktime_set(0, 10000000); /* 10 ms */
	hi->sdev.name = "h2w";
	hi->sdev.print_name = sapphire_h2w_print_name;

	ret = switch_dev_register(&hi->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	g_detection_work_queue = create_workqueue("detection");
	if (g_detection_work_queue == NULL) {
		ret = -ENOMEM;
		goto err_create_work_queue;
	}

	ret = gpio_request(SAPPHIRE_GPIO_CABLE_IN1, "h2w_detect");
	if (ret < 0)
		goto err_request_detect_gpio;

	ret = gpio_request(SAPPHIRE_GPIO_CABLE_IN2, "h2w_button");
	if (ret < 0)
		goto err_request_button_gpio;

	ret = gpio_direction_input(SAPPHIRE_GPIO_CABLE_IN1);
	if (ret < 0)
		goto err_set_detect_gpio;

	ret = gpio_direction_input(SAPPHIRE_GPIO_CABLE_IN2);
	if (ret < 0)
		goto err_set_button_gpio;

	hi->irq = gpio_to_irq(SAPPHIRE_GPIO_CABLE_IN1);
	if (hi->irq < 0) {
		ret = hi->irq;
		goto err_get_h2w_detect_irq_num_failed;
	}

	hi->irq_btn = gpio_to_irq(SAPPHIRE_GPIO_CABLE_IN2);
	if (hi->irq_btn < 0) {
		ret = hi->irq_btn;
		goto err_get_button_irq_num_failed;
	}

	/* Set CPLD MUX to H2W <-> CPLD GPIO */
	configure_cpld(UART3);
	/* Set the CPLD connected H2W GPIO's to input */
	gpio_set_value(SAPPHIRE_GPIO_H2W_CLK_DIR, 0);
	gpio_set_value(SAPPHIRE_GPIO_H2W_DAT_DIR, 0);

	hrtimer_init(&hi->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hi->timer.function = detect_event_timer_func;
	hrtimer_init(&hi->btn_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hi->btn_timer.function = button_event_timer_func;

	ret = request_irq(hi->irq, detect_irq_handler,
			  IRQF_TRIGGER_LOW, "h2w_detect", NULL);
	if (ret < 0)
		goto err_request_detect_irq;

	/* Disable button until plugged in */
	set_irq_flags(hi->irq_btn, IRQF_VALID | IRQF_NOAUTOEN);
	ret = request_irq(hi->irq_btn, button_irq_handler,
			  IRQF_TRIGGER_LOW, "h2w_button", NULL);
	if (ret < 0)
		goto err_request_h2w_headset_button_irq;

	ret = set_irq_wake(hi->irq, 1);
	if (ret < 0)
		goto err_request_input_dev;
	ret = set_irq_wake(hi->irq_btn, 1);
	if (ret < 0)
		goto err_request_input_dev;

	hi->input = input_allocate_device();
	if (!hi->input) {
		ret = -ENOMEM;
		goto err_request_input_dev;
	}

	hi->input->name = "h2w headset";
	hi->input->evbit[0] = BIT_MASK(EV_KEY);
	hi->input->keybit[BIT_WORD(KEY_MEDIA)] = BIT_MASK(KEY_MEDIA);

	ret = input_register_device(hi->input);
	if (ret < 0)
		goto err_register_input_dev;

	return 0;

err_register_input_dev:
	input_free_device(hi->input);
err_request_input_dev:
	free_irq(hi->irq_btn, 0);
err_request_h2w_headset_button_irq:
	free_irq(hi->irq, 0);
err_request_detect_irq:
err_get_button_irq_num_failed:
err_get_h2w_detect_irq_num_failed:
err_set_button_gpio:
err_set_detect_gpio:
	gpio_free(SAPPHIRE_GPIO_CABLE_IN2);
err_request_button_gpio:
	gpio_free(SAPPHIRE_GPIO_CABLE_IN1);
err_request_detect_gpio:
	destroy_workqueue(g_detection_work_queue);
err_create_work_queue:
	switch_dev_unregister(&hi->sdev);
err_switch_dev_register:
	printk(KERN_ERR "H2W: Failed to register driver\n");

	return ret;
}

static int sapphire_h2w_remove(struct platform_device *pdev)
{
	H2W_DBG("");
	if (switch_get_state(&hi->sdev))
		remove_headset();
	input_unregister_device(hi->input);
	gpio_free(SAPPHIRE_GPIO_CABLE_IN2);
	gpio_free(SAPPHIRE_GPIO_CABLE_IN1);
	free_irq(hi->irq_btn, 0);
	free_irq(hi->irq, 0);
	destroy_workqueue(g_detection_work_queue);
	switch_dev_unregister(&hi->sdev);

	return 0;
}

static struct platform_device sapphire_h2w_device = {
	.name		= "sapphire-h2w",
};

static struct platform_driver sapphire_h2w_driver = {
	.probe		= sapphire_h2w_probe,
	.remove		= sapphire_h2w_remove,
	.driver		= {
		.name		= "sapphire-h2w",
		.owner		= THIS_MODULE,
	},
};

static int __init sapphire_h2w_init(void)
{
	if (!machine_is_sapphire())
		return 0;
	int ret;
	H2W_DBG("");
	ret = platform_driver_register(&sapphire_h2w_driver);
	if (ret)
		return ret;
	return platform_device_register(&sapphire_h2w_device);
}

static void __exit sapphire_h2w_exit(void)
{
	platform_device_unregister(&sapphire_h2w_device);
	platform_driver_unregister(&sapphire_h2w_driver);
}

module_init(sapphire_h2w_init);
module_exit(sapphire_h2w_exit);

MODULE_AUTHOR("Laurence Chen <Laurence_Chen@htc.com>");
MODULE_DESCRIPTION("HTC 2 Wire detection driver for sapphire");
MODULE_LICENSE("GPL");
