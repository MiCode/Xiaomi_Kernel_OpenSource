/*
 * drivers/misc/bluedroid_pm.c
 *
 * Copyright (c) 2012-2013, NVIDIA Corporation. All rights reserved.
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

#include <linux/err.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/pm_qos.h>

#define PROC_DIR	"bluetooth/sleep"

/* 5 seconds of Min CPU configurations during resume */
#define DEFAULT_RESUME_CPU_TIMEOUT	5000000

struct bluedroid_pm_data {
	int gpio_reset;
	int gpio_shutdown;
	int host_wake;
	int ext_wake;
	int is_blocked;
	int resume_min_frequency;
	unsigned host_wake_irq;
	struct regulator *vdd_3v3;
	struct regulator *vdd_1v8;
	struct rfkill *rfkill;
	struct wake_lock wake_lock;
	struct pm_qos_request resume_cpu_freq_req;
};

struct proc_dir_entry *proc_bt_dir, *bluetooth_sleep_dir;
static bool bluedroid_pm_blocked = 1;

static int create_bt_proc_interface(void *drv_data);
static void remove_bt_proc_interface(void);

static irqreturn_t bluedroid_pm_hostwake_isr(int irq, void *dev_id)
{
	/* schedule a tasklet to handle the change in the host wake line */
	return IRQ_HANDLED;
}

static int bluedroid_pm_rfkill_set_power(void *data, bool blocked)
{
	struct bluedroid_pm_data *bluedroid_pm = data;
	/*
	 * check if BT gpio_shutdown line status and current request are same.
	 * If same, then return, else perform requested operation.
	 */
	if (gpio_get_value(bluedroid_pm->gpio_shutdown) == !blocked)
		return 0;

	if (blocked) {
		if (bluedroid_pm->gpio_shutdown)
			gpio_set_value(bluedroid_pm->gpio_shutdown, 0);
		if (bluedroid_pm->gpio_reset)
			gpio_set_value(bluedroid_pm->gpio_reset, 0);
		if (bluedroid_pm->vdd_3v3)
			regulator_disable(bluedroid_pm->vdd_3v3);
		if (bluedroid_pm->vdd_1v8)
			regulator_disable(bluedroid_pm->vdd_1v8);
		if (bluedroid_pm->ext_wake)
			wake_unlock(&bluedroid_pm->wake_lock);
		pm_qos_remove_request(&bluedroid_pm->resume_cpu_freq_req);
	} else {
		if (bluedroid_pm->vdd_3v3)
			regulator_enable(bluedroid_pm->vdd_3v3);
		if (bluedroid_pm->vdd_1v8)
			regulator_enable(bluedroid_pm->vdd_1v8);
		if (bluedroid_pm->gpio_shutdown)
			gpio_set_value(bluedroid_pm->gpio_shutdown, 1);
		if (bluedroid_pm->gpio_reset)
			gpio_set_value(bluedroid_pm->gpio_reset, 1);
		pm_qos_add_request(&bluedroid_pm->resume_cpu_freq_req,
				PM_QOS_CPU_FREQ_MIN, PM_QOS_DEFAULT_VALUE);
	}
	bluedroid_pm->is_blocked = blocked;
	return 0;
}

static const struct rfkill_ops bluedroid_pm_rfkill_ops = {
	.set_block = bluedroid_pm_rfkill_set_power,
};

/*
 * This API is added to set block state by ext driver,
 * when bluedroid_pm rfkill is not used but host_wake functionality to be used.
 * Eg: btwilink driver
 */
void bluedroid_pm_set_ext_state(bool blocked)
{
	bluedroid_pm_blocked = blocked;
}
EXPORT_SYMBOL(bluedroid_pm_set_ext_state);

static int bluedroid_pm_probe(struct platform_device *pdev)
{
	static struct bluedroid_pm_data *bluedroid_pm;
	struct rfkill *rfkill;
	struct resource *res;
	int ret;
	bool enable = false;  /* off */

	bluedroid_pm = kzalloc(sizeof(*bluedroid_pm), GFP_KERNEL);
	if (!bluedroid_pm)
		return -ENOMEM;

	bluedroid_pm->vdd_3v3 = regulator_get(&pdev->dev, "vdd_bt_3v3");
	if (IS_ERR_OR_NULL(bluedroid_pm->vdd_3v3)) {
		pr_warn("%s: regulator vdd_bt_3v3 not available\n", __func__);
		bluedroid_pm->vdd_3v3 = NULL;
	}
	bluedroid_pm->vdd_1v8 = regulator_get(&pdev->dev, "vddio_bt_1v8");
	if (IS_ERR_OR_NULL(bluedroid_pm->vdd_1v8)) {
		pr_warn("%s: regulator vddio_bt_1v8 not available\n", __func__);
		bluedroid_pm->vdd_1v8 = NULL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IO, "reset_gpio");
	if (res) {
		bluedroid_pm->gpio_reset = res->start;
		ret = gpio_request(bluedroid_pm->gpio_reset, "reset_gpio");
		if (ret) {
			pr_err("%s: Failed to get reset gpio\n", __func__);
			goto free_res;
		}
		gpio_direction_output(bluedroid_pm->gpio_reset, enable);
	} else {
		pr_debug("%s: Reset gpio not registered.\n", __func__);
		bluedroid_pm->gpio_reset = 0;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
						"shutdown_gpio");
	if (res) {
		bluedroid_pm->gpio_shutdown = res->start;
		ret = gpio_request(bluedroid_pm->gpio_shutdown,
						"shutdown_gpio");
		if (ret) {
			pr_err("%s: Failed to get shutdown gpio\n", __func__);
			goto free_res;
		}
		gpio_direction_output(bluedroid_pm->gpio_shutdown, enable);
	} else {
		pr_debug("%s: shutdown gpio not registered\n", __func__);
		bluedroid_pm->gpio_shutdown = 0;
	}

	/*
	 * make sure at-least one of the GPIO or regulators avaiable to
	 * register with rfkill is defined
	 */
	if (bluedroid_pm->gpio_reset || bluedroid_pm->gpio_shutdown ||
		bluedroid_pm->vdd_1v8 || bluedroid_pm->vdd_3v3) {
		rfkill = rfkill_alloc(pdev->name, &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bluedroid_pm_rfkill_ops,
				bluedroid_pm);

		if (unlikely(!rfkill))
			goto free_res;

		bluedroid_pm->is_blocked = !enable;
		rfkill_set_states(rfkill, bluedroid_pm->is_blocked, false);

		ret = rfkill_register(rfkill);

		if (unlikely(ret)) {
			rfkill_destroy(rfkill);
			kfree(rfkill);
			goto free_res;
		}
		bluedroid_pm->rfkill = rfkill;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_host_wake");
	if (res) {
		bluedroid_pm->host_wake = res->start;
		ret = gpio_request(bluedroid_pm->host_wake, "bt_host_wake");
		if (ret) {
			pr_err("%s: Failed to get host_wake gpio\n", __func__);
			goto free_res;
		}
		/* configure host_wake as input */
		gpio_direction_input(bluedroid_pm->host_wake);
	} else {
		pr_debug("%s: gpio_host_wake not registered\n", __func__);
		bluedroid_pm->host_wake = 0;
	}


	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "host_wake");
	if (res) {
		pr_err("%s : found host_wake irq\n", __func__);
		bluedroid_pm->host_wake_irq = res->start;
		ret = request_irq(bluedroid_pm->host_wake_irq,
					bluedroid_pm_hostwake_isr,
					IRQF_DISABLED | IRQF_TRIGGER_RISING,
					"bluetooth hostwake", bluedroid_pm);
		if (ret) {
			pr_err("%s: Failed to get host_wake irq\n", __func__);
			goto free_res;
		}
	} else {
		pr_debug("%s: host_wake not registered\n", __func__);
		bluedroid_pm->host_wake_irq = 0;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_ext_wake");
	if (res) {
		bluedroid_pm->ext_wake = res->start;
		ret = gpio_request(bluedroid_pm->ext_wake, "bt_ext_wake");
		if (ret) {
			pr_err("%s: Failed to get ext_wake gpio\n", __func__);
			goto free_res;
		}
		/* configure ext_wake as output mode*/
		gpio_direction_output(bluedroid_pm->ext_wake, 1);
		if (create_bt_proc_interface(bluedroid_pm)) {
			pr_err("%s: Failed to create proc interface", __func__);
			goto free_res;
		}
		/* initialize wake lock */
		wake_lock_init(&bluedroid_pm->wake_lock, WAKE_LOCK_SUSPEND,
								"bluedroid_pm");
	} else {
		pr_debug("%s: gpio_ext_wake not registered\n", __func__);
		bluedroid_pm->ext_wake = 0;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"min_cpu_freq");
	if (res)
		bluedroid_pm->resume_min_frequency = res->start;

	platform_set_drvdata(pdev, bluedroid_pm);
	pr_debug("RFKILL BT driver successfully registered");
	return 0;

free_res:
	if (bluedroid_pm->vdd_3v3)
		regulator_put(bluedroid_pm->vdd_3v3);
	if (bluedroid_pm->vdd_1v8)
		regulator_put(bluedroid_pm->vdd_1v8);
	if (bluedroid_pm->gpio_shutdown)
		gpio_free(bluedroid_pm->gpio_shutdown);
	if (bluedroid_pm->gpio_reset)
		gpio_free(bluedroid_pm->gpio_reset);
	if (bluedroid_pm->ext_wake)
		gpio_free(bluedroid_pm->ext_wake);
	if (bluedroid_pm->host_wake)
		gpio_free(bluedroid_pm->host_wake);
	if (bluedroid_pm->rfkill) {
		rfkill_unregister(bluedroid_pm->rfkill);
		rfkill_destroy(bluedroid_pm->rfkill);
		kfree(bluedroid_pm->rfkill);
	}
	kfree(bluedroid_pm);
	return -ENODEV;
}

static int bluedroid_pm_remove(struct platform_device *pdev)
{
	struct bluedroid_pm_data *bluedroid_pm = platform_get_drvdata(pdev);

	if (bluedroid_pm->host_wake)
		gpio_free(bluedroid_pm->host_wake);
	if (bluedroid_pm->host_wake_irq)
		free_irq(bluedroid_pm->host_wake_irq, NULL);
	if (bluedroid_pm->ext_wake) {
		wake_lock_destroy(&bluedroid_pm->wake_lock);
		gpio_free(bluedroid_pm->ext_wake);
		remove_bt_proc_interface();
	}
	if (bluedroid_pm->gpio_reset || bluedroid_pm->gpio_shutdown ||
		bluedroid_pm->vdd_1v8 || bluedroid_pm->vdd_3v3) {
		rfkill_unregister(bluedroid_pm->rfkill);
		rfkill_destroy(bluedroid_pm->rfkill);
		kfree(bluedroid_pm->rfkill);
	}
	if (bluedroid_pm->gpio_shutdown)
		gpio_free(bluedroid_pm->gpio_shutdown);
	if (bluedroid_pm->gpio_reset)
		gpio_free(bluedroid_pm->gpio_reset);
	if (bluedroid_pm->vdd_3v3)
		regulator_put(bluedroid_pm->vdd_3v3);
	if (bluedroid_pm->vdd_1v8)
		regulator_put(bluedroid_pm->vdd_1v8);
	kfree(bluedroid_pm);

	return 0;
}

static int bluedroid_pm_suspend(struct platform_device *pdev,
						pm_message_t state)
{
	struct bluedroid_pm_data *bluedroid_pm = platform_get_drvdata(pdev);
	if (bluedroid_pm->host_wake)
		if (!bluedroid_pm->is_blocked || !bluedroid_pm_blocked)
			enable_irq_wake(bluedroid_pm->host_wake_irq);

	return 0;
}

static int bluedroid_pm_resume(struct platform_device *pdev)
{
	struct bluedroid_pm_data *bluedroid_pm = platform_get_drvdata(pdev);
	if (bluedroid_pm->host_wake)
		if (!bluedroid_pm->is_blocked || !bluedroid_pm_blocked) {
			disable_irq_wake(bluedroid_pm->host_wake_irq);
			if (bluedroid_pm->resume_min_frequency)
				pm_qos_update_request_timeout(
					&bluedroid_pm->resume_cpu_freq_req,
					bluedroid_pm->resume_min_frequency,
					DEFAULT_RESUME_CPU_TIMEOUT);
		}

	return 0;
}
static struct platform_driver bluedroid_pm_driver = {
	.probe = bluedroid_pm_probe,
	.remove = bluedroid_pm_remove,
	.suspend = bluedroid_pm_suspend,
	.resume = bluedroid_pm_resume,
	.driver = {
		   .name = "bluedroid_pm",
		   .owner = THIS_MODULE,
	},
};

static int lpm_read_proc(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	*eof = 1;
	return sprintf(page, "lpm_read");
}

static int lpm_write_proc(struct file *file, const char *buffer,
					unsigned long count, void *data)
{
	char *buf;
	struct bluedroid_pm_data *bluedroid_pm = data;

	if (count < 1)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}

	if (!bluedroid_pm->is_blocked) {
		if (buf[0] == '0') {
			gpio_set_value(bluedroid_pm->ext_wake, 0);
			wake_unlock(&bluedroid_pm->wake_lock);
		} else if (buf[0] == '1') {
			gpio_set_value(bluedroid_pm->ext_wake, 1);
			wake_lock(&bluedroid_pm->wake_lock);
		} else {
			kfree(buf);
			return -EINVAL;
		}
	}

	kfree(buf);
	return count;
}

static void remove_bt_proc_interface(void)
{
	remove_proc_entry("lpm", bluetooth_sleep_dir);
	remove_proc_entry("sleep", proc_bt_dir);
	remove_proc_entry("bluetooth", 0);
}

static int create_bt_proc_interface(void *drv_data)
{
	int retval;
	struct proc_dir_entry *ent;

	proc_bt_dir = proc_mkdir("bluetooth", NULL);
	if (proc_bt_dir == NULL) {
		pr_err("Unable to create /proc/bluetooth directory");
		return -ENOMEM;
	}

	bluetooth_sleep_dir = proc_mkdir("sleep", proc_bt_dir);
	if (proc_bt_dir == NULL) {
		pr_err("Unable to create /proc/bluetooth directory");
		return -ENOMEM;
	}

	/* Creating read/write "btwake" entry */
	ent = create_proc_entry("lpm", 0622, bluetooth_sleep_dir);
	if (ent == NULL) {
		pr_err("Unable to create /proc/%s/btwake entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}
	ent->read_proc	= lpm_read_proc;
	ent->write_proc = lpm_write_proc;
	ent->data	= drv_data;
	return 0;
fail:
	remove_proc_entry("lpm", bluetooth_sleep_dir);
	remove_proc_entry("sleep", proc_bt_dir);
	remove_proc_entry("bluetooth", 0);
	return retval;
}

static int __init bluedroid_pm_init(void)
{
	return platform_driver_register(&bluedroid_pm_driver);
}

static void __exit bluedroid_pm_exit(void)
{
	platform_driver_unregister(&bluedroid_pm_driver);
}

module_init(bluedroid_pm_init);
module_exit(bluedroid_pm_exit);

MODULE_DESCRIPTION("bluedroid PM");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL");
