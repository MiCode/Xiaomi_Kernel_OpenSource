/*
 *
 *  Copyright (C) 2011 Google, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/acpi.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/serial_hsu.h>

enum {
	gpio_wake_acpi_idx,
	gpio_enable_bt_acpi_idx,
	host_wake_acpi_idx
};
static struct gpio_desc *bt_lpm_gpiod;

static struct rfkill *bt_rfkill;
static bool bt_enabled;

#ifndef DBG_DISABLE_BT_LOW_POWER
static bool host_wake_uart_enabled;
static bool wake_uart_enabled;
static bool int_handler_enabled;
#endif /* !DBG_DISABLE_BT_LOW_POWER */


static void activate_irq_handler(void);

struct bluetooth_low_power_mode {
#ifndef DBG_DISABLE_BT_LOW_POWER
	unsigned int gpio_wake;
	unsigned int gpio_host_wake;
	unsigned int int_host_wake;
#endif /* !DBG_DISABLE_BT_LOW_POWER */
#ifdef CONFIG_PF450CL
	unsigned int gpio_reg_on;
	unsigned int gpio_reset;
#else
	unsigned int gpio_enable_bt;
#endif
#ifndef DBG_DISABLE_BT_LOW_POWER
	int wake;
	int host_wake;
#endif /* !DBG_DISABLE_BT_LOW_POWER */
	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

	struct device *tty_dev;
#ifndef DBG_DISABLE_BT_LOW_POWER
	struct wake_lock wake_lock;
	char wake_lock_name[100];
#endif /* !DBG_DISABLE_BT_LOW_POWER */
	int port;
} bt_lpm;

#ifndef DBG_DISABLE_BT_LOW_POWER
static void uart_enable(struct device *tty)
{
	pr_debug("%s: runtime get\n", __func__);
	/* Tell PM runtime to power on the tty device and block s0i3 */
	pm_runtime_get_sync(tty);
}

static void uart_disable(struct device *tty)
{
	pr_debug("%s: runtime put\n", __func__);
	/* Tell PM runtime to release tty device and allow s0i3 */
	pm_runtime_put_sync(tty);
}
#endif /* !DBG_DISABLE_BT_LOW_POWER */

static int bt_lpm_acpi_probe(struct platform_device *pdev)
{
	acpi_handle handle;
	acpi_integer port;

	/*
	 * Handle ACPI specific initializations.
	 */
	dev_dbg(&pdev->dev, "ACPI specific probe\n");

	bt_lpm_gpiod = gpiod_get_index(&pdev->dev, NULL,
						gpio_enable_bt_acpi_idx);
	bt_lpm.gpio_enable_bt = desc_to_gpio(bt_lpm_gpiod);
	if (!gpio_is_valid(bt_lpm.gpio_enable_bt)) {
		pr_err("%s: gpio %d for gpio_enable_bt not valid\n", __func__,
							bt_lpm.gpio_enable_bt);
		return -EINVAL;
	}

#ifndef DBG_DISABLE_BT_LOW_POWER
	bt_lpm_gpiod = gpiod_get_index(&pdev->dev, NULL,
						gpio_wake_acpi_idx);
	bt_lpm.gpio_wake = desc_to_gpio(bt_lpm_gpiod);
	if (!gpio_is_valid(bt_lpm.gpio_wake)) {
		pr_err("%s: gpio %d for gpio_wake not valid\n", __func__,
							bt_lpm.gpio_wake);
		return -EINVAL;
	}

	bt_lpm_gpiod = gpiod_get_index(&pdev->dev, NULL,
						host_wake_acpi_idx);
	bt_lpm.gpio_host_wake = desc_to_gpio(bt_lpm_gpiod);
	if (!gpio_is_valid(bt_lpm.gpio_host_wake)) {
		pr_err("%s: gpio %d for gpio_host_wake not valid\n", __func__,
							bt_lpm.gpio_host_wake);
		return -EINVAL;
	}

	bt_lpm.int_host_wake = gpio_to_irq(bt_lpm.gpio_host_wake);

	pr_debug("%s: gpio_wake %d, gpio_host_wake %d, int_host_wake %d\n",
							__func__,
							bt_lpm.gpio_wake,
							bt_lpm.gpio_host_wake,
							bt_lpm.int_host_wake);
#endif /* !DBG_DISABLE_BT_LOW_POWER */

	handle = ACPI_HANDLE(&pdev->dev);

	if (ACPI_FAILURE(acpi_evaluate_integer(handle, "UART", NULL, &port))) {
		dev_err(&pdev->dev, "Error evaluating UART port number\n");

		/* FIXME - Force port 0 if the information is missing from the
		 * ACPI table.
		 * That will be removed once the ACPI tables will all have been
		 * updated.
		 */
		 port = 0;
	}

	bt_lpm.port = port;
	pr_debug("%s: UART port %d\n", __func__, bt_lpm.port);

	return 0;
}

static int bt_lpm_rfkill_set_power(void *data, bool blocked)
{
	/* rfkill_ops callback. Turn transmitter on when blocked is false */

	if (!blocked) {
#ifndef DBG_DISABLE_BT_LOW_POWER
		gpio_set_value(bt_lpm.gpio_wake, 1);
		/*
		* Delay advice by device vendor is min 2.5ns,
		* setting it between 10 and 50us for more confort
		*/
		usleep_range(10, 50);
#endif /* !DBG_DISABLE_BT_LOW_POWER */
#ifdef CONFIG_PF450CL
		gpio_set_value(bt_lpm.gpio_reg_on, 1);
		gpio_set_value(bt_lpm.gpio_reset, 1);
#else
		gpio_set_value(bt_lpm.gpio_enable_bt, 1);
#endif
		pr_debug("%s: turn BT on\n", __func__);
	} else {
#ifdef CONFIG_PF450CL
		gpio_set_value(bt_lpm.gpio_reg_on, 0);
		gpio_set_value(bt_lpm.gpio_reset, 0);
#else
		gpio_set_value(bt_lpm.gpio_enable_bt, 0);
#endif
		pr_debug("%s: turn BT off\n", __func__);
	}

	bt_enabled = !blocked;

	return 0;
}

static const struct rfkill_ops bt_lpm_rfkill_ops = {
	.set_block = bt_lpm_rfkill_set_power,
};

#ifndef DBG_DISABLE_BT_LOW_POWER
static void set_wake_locked(int wake)
{
	bt_lpm.wake = wake;

	if (!wake)
		wake_unlock(&bt_lpm.wake_lock);

	if (!wake_uart_enabled && wake) {
		WARN_ON(!bt_lpm.tty_dev);
		uart_enable(bt_lpm.tty_dev);
	}

	gpio_set_value(bt_lpm.gpio_wake, wake);

	if (wake_uart_enabled && !wake) {
		WARN_ON(!bt_lpm.tty_dev);
		uart_disable(bt_lpm.tty_dev);
	}
	wake_uart_enabled = wake;
}

static enum hrtimer_restart enter_lpm(struct hrtimer *timer)
{
	pr_debug("%s\n", __func__);

	set_wake_locked(0);

	return HRTIMER_NORESTART;
}


static void update_host_wake_locked(int host_wake)
{
	if (host_wake == bt_lpm.host_wake)
		return;

	bt_lpm.host_wake = host_wake;

	if (host_wake) {
		wake_lock(&bt_lpm.wake_lock);
		if (!host_wake_uart_enabled) {
			WARN_ON(!bt_lpm.tty_dev);
			uart_enable(bt_lpm.tty_dev);
		}
	} else  {
		if (host_wake_uart_enabled) {
			WARN_ON(!bt_lpm.tty_dev);
			uart_disable(bt_lpm.tty_dev);
		}
		/*
		 * Take a timed wakelock, so that upper layers can take it.
		 * The chipset deasserts the hostwake lock, when there is no
		 * more data to send.
		 */
		wake_lock_timeout(&bt_lpm.wake_lock, HZ/2);
	}

	host_wake_uart_enabled = host_wake;

}

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;

	host_wake = gpio_get_value(bt_lpm.gpio_host_wake);

	pr_debug("%s: lpm %s\n", __func__, host_wake ? "off" : "on");

	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_FALLING :
							IRQF_TRIGGER_RISING);

	if (!bt_lpm.tty_dev) {
		bt_lpm.host_wake = host_wake;
		return IRQ_HANDLED;
	}

	update_host_wake_locked(host_wake);

	return IRQ_HANDLED;
}

static void activate_irq_handler(void)
{
	int ret;

	pr_debug("%s\n", __func__);

	ret = request_irq(bt_lpm.int_host_wake, host_wake_isr,
				IRQF_TRIGGER_RISING, "bt_host_wake", NULL);

	if (ret < 0) {
		pr_err("Error lpm request IRQ");
		gpio_free(bt_lpm.gpio_wake);
		gpio_free(bt_lpm.gpio_host_wake);
	}
}


static void bt_lpm_wake_peer(struct device *dev)
{
	bt_lpm.tty_dev = dev;

	/*
	 * the irq is enabled after the first host wake up signal.
	 * in the original code, the irq should be in levels but, since mfld
	 * does not support them, irq is triggering with edges.
	 */

	if (!int_handler_enabled) {
		int_handler_enabled = true;
		activate_irq_handler();
	}

	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer);

	set_wake_locked(1);

	hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
		HRTIMER_MODE_REL);

}

static int bluetooth_lpm_init(struct platform_device *pdev)
{
	int ret;
	struct device *tty_dev;

	hrtimer_init(&bt_lpm.enter_lpm_timer, CLOCK_MONOTONIC,
							HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_delay = ktime_set(1, 0);  /* 1 sec */
	bt_lpm.enter_lpm_timer.function = enter_lpm;

	bt_lpm.host_wake = 0;

	if (bt_lpm.gpio_host_wake < 0) {
		pr_err("Error bt_lpm.gpio_host_wake\n");
		return -ENODEV;
	}

	ret = irq_set_irq_wake(bt_lpm.int_host_wake, 1);
	if (ret < 0) {
		pr_err("Error lpm set irq IRQ");
		gpio_free(bt_lpm.gpio_wake);
		gpio_free(bt_lpm.gpio_host_wake);
		return ret;
	}

	tty_dev = intel_mid_hsu_set_wake_peer(bt_lpm.port,
			bt_lpm_wake_peer);
	if (!tty_dev) {
		pr_err("Error no tty dev");
		gpio_free(bt_lpm.gpio_wake);
		gpio_free(bt_lpm.gpio_host_wake);
		return -ENODEV;
	}

	snprintf(bt_lpm.wake_lock_name, sizeof(bt_lpm.wake_lock_name),
			"BTLowPower");
	wake_lock_init(&bt_lpm.wake_lock, WAKE_LOCK_SUSPEND,
			 bt_lpm.wake_lock_name);

	bt_lpm_wake_peer(tty_dev);
	return 0;
}
#endif /* !DBG_DISABLE_BT_LOW_POWER */

static int bt_lpm_probe(struct platform_device *pdev)
{
	bool default_state = true;	/* off */
	int ret = 0;
#ifndef DBG_DISABLE_BT_LOW_POWER
	int_handler_enabled = false;
#endif /* !DBG_DISABLE_BT_LOW_POWER */
	if (ACPI_HANDLE(&pdev->dev)) {
		/*
		 * acpi specific probe
		 */
		pr_debug("%s for ACPI device %s\n", __func__,
							dev_name(&pdev->dev));
		if (bt_lpm_acpi_probe(pdev) < 0)
			ret = -EINVAL;
	} else
		ret = -ENODEV;

	if (ret < 0) {
		pr_err("%s: Cannot register platform data\n", __func__);
		goto err_data_probe;
	}

#ifdef CONFIG_PF450CL
	ret = gpio_request(bt_lpm.gpio_reg_on, pdev->name);
	if (ret < 0) {
		pr_err("%s: Unable to request gpio %d\n", __func__,
							bt_lpm.gpio_reg_on);
		goto err_gpio_enable_req;
	}

	ret = gpio_direction_output(bt_lpm.gpio_reg_on, 0);
	if (ret < 0) {
		pr_err("%s: Unable to set int direction for gpio %d\n",
					__func__, bt_lpm.gpio_reg_on);
		goto err_gpio_enable_dir;
	}

	ret = gpio_request(bt_lpm.gpio_reset, pdev->name);
	if (ret < 0) {
		pr_err("%s: Unable to request gpio %d\n", __func__,
							bt_lpm.gpio_reset);
		goto err_gpio_enable_req;
	}

	ret = gpio_direction_output(bt_lpm.gpio_reset, 0);
	if (ret < 0) {
		pr_err("%s: Unable to set int direction for gpio %d\n",
					__func__, bt_lpm.gpio_reset);
		goto err_gpio_enable_dir;
	}
#else

	ret = gpio_direction_output(bt_lpm.gpio_enable_bt, 0);
	if (ret < 0) {
		pr_err("%s: Unable to set int direction for gpio %d\n",
					__func__, bt_lpm.gpio_enable_bt);
		goto err_gpio_enable_dir;
	}
#endif

#ifndef DBG_DISABLE_BT_LOW_POWER
	ret = gpio_direction_input(bt_lpm.gpio_host_wake);
	if (ret < 0) {
		pr_err("%s: Unable to set direction for gpio %d\n", __func__,
							bt_lpm.gpio_host_wake);
		goto err_gpio_host_wake_dir;
	}

	ret =  gpio_direction_output(bt_lpm.gpio_wake, 0);
	if (ret < 0) {
		pr_err("%s: Unable to set direction for gpio %d\n", __func__,
							bt_lpm.gpio_wake);
		goto err_gpio_wake_dir;
	}

#ifdef CONFIG_PF450CL
	pr_debug("%s: gpio_reset=%d, gpio_reg_on=%d, gpio_wake=%d, gpio_host_wake=%d\n",
							__func__,
							bt_lpm.gpio_reset,
							bt_lpm.gpio_reg_on,
							bt_lpm.gpio_wake,
							bt_lpm.gpio_host_wake);
#else
	pr_debug("%s: gpio_enable=%d, gpio_wake=%d, gpio_host_wake=%d\n",
							__func__,
							bt_lpm.gpio_enable_bt,
							bt_lpm.gpio_wake,
							bt_lpm.gpio_host_wake);
#endif
#endif /* !DBG_DISABLE_BT_LOW_POWER */

	bt_rfkill = rfkill_alloc("Bluetooth LPM", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bt_lpm_rfkill_ops,
				NULL);
	if (unlikely(!bt_rfkill)) {
		ret = -ENOMEM;
		goto err_rfkill_alloc;
	}

	bt_lpm_rfkill_set_power(NULL, default_state);
	rfkill_init_sw_state(bt_rfkill, default_state);

	ret = rfkill_register(bt_rfkill);
	if (unlikely(ret))
		goto err_rfkill_register;

#ifndef DBG_DISABLE_BT_LOW_POWER
	ret = bluetooth_lpm_init(pdev);
	if (ret)
		goto err_lpm_init;
#endif /* !DBG_DISABLE_BT_LOW_POWER */

	return ret;

err_lpm_init:
	rfkill_unregister(bt_rfkill);
err_rfkill_register:
	rfkill_destroy(bt_rfkill);
err_rfkill_alloc:
#ifndef DBG_DISABLE_BT_LOW_POWER
err_gpio_wake_dir:
	gpio_free(bt_lpm.gpio_wake);
err_gpio_wake_req:
err_gpio_host_wake_dir:
	gpio_free(bt_lpm.gpio_host_wake);
err_gpio_host_wake_req:
#endif /* !DBG_DISABLE_BT_LOW_POWER */
err_gpio_enable_dir:
#ifdef CONFIG_PF450CL
	gpio_free(bt_lpm.gpio_reg_on);
	gpio_free(bt_lpm.gpio_reset);
#else
	gpio_free(bt_lpm.gpio_enable_bt);
#endif
err_gpio_enable_req:
err_data_probe:
	return ret;
}

static int bt_lpm_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

#ifdef CONFIG_PF450CL
	gpio_free(bt_lpm.gpio_reg_on);
	gpio_free(bt_lpm.gpio_reset);
#else
	gpio_free(bt_lpm.gpio_enable_bt);
#endif
#ifndef DBG_DISABLE_BT_LOW_POWER
	gpio_free(bt_lpm.gpio_wake);
	gpio_free(bt_lpm.gpio_host_wake);
	wake_lock_destroy(&bt_lpm.wake_lock);
#endif /* !DBG_DISABLE_BT_LOW_POWER */
	gpiod_put(bt_lpm_gpiod);
	return 0;
}
#ifndef DBG_DISABLE_BT_LOW_POWER
int bt_lpm_suspend(struct platform_device *pdev, pm_message_t state)
{
	int host_wake;

	pr_debug("%s\n", __func__);

	if (!bt_enabled)
		return 0;
	/*
	 * Disable interrupt in suspend process,
	 * to avoid spurious interrupts, preventing IA to go in S3.
	 * The GPIO itself is a wake source that will trigger the PMC
	 * which will wake up IA to call PM_runtime resume
	 * which will re-enable the interrupt.
	 */
	disable_irq(bt_lpm.int_host_wake);

	host_wake = gpio_get_value(bt_lpm.gpio_host_wake);
	if (host_wake) {
		enable_irq(bt_lpm.int_host_wake);
		pr_err("%s suspend error, gpio %d set\n", __func__,
							bt_lpm.gpio_host_wake);
		return -EBUSY;
	}

	return 0;
}

int bt_lpm_resume(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	if (bt_enabled)
		enable_irq(bt_lpm.int_host_wake);
	return 0;
}
#endif /* !DBG_DISABLE_BT_LOW_POWER */

static struct acpi_device_id bt_lpm_id_table[] = {
	/* ACPI IDs here */
	{ "BCM2E1A", 0 },
	{ "BCM2E3A", 0 },
	{ "OBDA8723", 0},
	{ }
};

MODULE_DEVICE_TABLE(acpi, bt_lpm_id_table);


static struct platform_driver bt_lpm_platform_driver = {
	.probe = bt_lpm_probe,
	.remove = bt_lpm_remove,
#ifndef DBG_DISABLE_BT_LOW_POWER
	.suspend = bt_lpm_suspend,
	.resume = bt_lpm_resume,
#endif /* !DBG_DISABLE_BT_LOW_POWER */
	.driver = {
		   .name = "bt_lpm",
		   .owner = THIS_MODULE,
		   .acpi_match_table = ACPI_PTR(bt_lpm_id_table),
		   },
};

static int __init bt_lpm_init(void)
{
	bt_enabled = false;
	return platform_driver_register(&bt_lpm_platform_driver);
}

static void __exit bt_lpm_exit(void)
{
	platform_driver_unregister(&bt_lpm_platform_driver);
}

module_init(bt_lpm_init);
module_exit(bt_lpm_exit);

MODULE_DESCRIPTION("bt_lpm");
MODULE_AUTHOR("Jaikumar Ganesh <jaikumar@google.com>");
MODULE_LICENSE("GPL");

