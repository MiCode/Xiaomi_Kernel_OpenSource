/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/watchdog.h>
#include <linux/of_device.h>

#define MASK_SIZE		32
#define QCOM_VM_WDT_HZ		32765
#define QCOM_VM_PET_TIMEOUT	9U
#define QCOM_VM_BARK_TIMEOUT	11U

enum qcom_vm_wdt_reg {
	WDT_RST,
	WDT_CTRL,
	WDT_STS,
	WDT_BARK_TIME,
	WDT_BITE_TIME,
};

static const u32 qcom_vm_wdt_reg_offset_data[] = {
	[WDT_RST] = 0x4,
	[WDT_CTRL] = 0x8,
	[WDT_STS] = 0xC,
	[WDT_BARK_TIME] = 0x10,
	[WDT_BITE_TIME] = 0x14,
};

struct qcom_vm_wdt {
	struct watchdog_device	wdd;
	void __iomem		*base;
	const u32		*layout;
	unsigned int bark_irq;
	unsigned int bite_irq;
	unsigned int bark_time;
	unsigned int pet_time;
	unsigned long long last_pet;
	struct device *dev;
	struct notifier_block panic_blk;
	struct timer_list pet_timer;
	bool timer_expired;
	wait_queue_head_t pet_complete;
	struct task_struct *watchdog_task;
	cpumask_t alive_mask;
	struct mutex disable_lock;
};

static int enable = 1;
module_param(enable, int, 0000);

/* Disable the watchdog in hypervisor */
static int hyp_enable = 1;
module_param(hyp_enable, int, 0000);

static void __iomem *qcom_vm_wdt_addr(struct qcom_vm_wdt *wdt,
					enum qcom_vm_wdt_reg reg)
{
	return wdt->base + wdt->layout[reg];
}

static void dump_cpu_alive_mask(struct qcom_vm_wdt *wdt)
{
	static char alive_mask_buf[MASK_SIZE];

	scnprintf(alive_mask_buf, MASK_SIZE, "%*pb1", cpumask_pr_args(
				&wdt->alive_mask));
	dev_info(wdt->dev, "cpu alive mask from last pet %s\n",
				alive_mask_buf);
}

static irqreturn_t qcom_vm_wdt_bark_handler(int irq, void *dev_id)
{
	struct qcom_vm_wdt *wdt = (struct qcom_vm_wdt *)dev_id;
	unsigned long long t = sched_clock();

	dev_info(wdt->dev, "Watchdog bark! Now = %lu\n",
			(unsigned long) t);
	dev_info(wdt->dev, "Watchdog last pet at %lu\n",
			(unsigned long) wdt->last_pet);
	dump_cpu_alive_mask(wdt);
	pr_info("Causing a watchdog bite!");
	__raw_writel(1 * QCOM_VM_WDT_HZ, qcom_vm_wdt_addr(wdt, WDT_BITE_TIME));
	/* Make sure bite time is written before we reset */
	mb();
	__raw_writel(1, qcom_vm_wdt_addr(wdt, WDT_RST));
	/* Make sure we wait only after reset */
	mb();
	/* Delay to make sure bite occurs */
	msleep(10000);

	panic("Failed to cause a watchdog bite! - Falling back to kernel panic!");
	return IRQ_HANDLED;
}

static int qcom_vm_wdt_suspend(struct device *dev)
{
	struct qcom_vm_wdt *wdt =
			(struct qcom_vm_wdt *)dev_get_drvdata(dev);

	if (!enable)
		return 0;

	__raw_writel(1, qcom_vm_wdt_addr(wdt, WDT_RST));
	/* Make sure watchdog is suspended before setting enable */
	mb();

	wdt->last_pet = sched_clock();
	return 0;
}

static int qcom_vm_wdt_resume(struct device *dev)
{
	struct qcom_vm_wdt *wdt =
			(struct qcom_vm_wdt *)dev_get_drvdata(dev);

	if (!enable)
		return 0;

	__raw_writel(1, qcom_vm_wdt_addr(wdt, WDT_RST));
	/* Make sure watchdog is suspended before setting enable */
	mb();

	wdt->last_pet = sched_clock();
	return 0;
}

static int qcom_vm_wdt_panic_handler(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	struct qcom_vm_wdt *wdt = container_of(this, struct qcom_vm_wdt,
						panic_blk);

	__raw_writel(QCOM_VM_WDT_HZ * (panic_timeout + 10),
			qcom_vm_wdt_addr(wdt, WDT_BARK_TIME));
	__raw_writel(QCOM_VM_WDT_HZ * (panic_timeout + 10),
			qcom_vm_wdt_addr(wdt, WDT_BITE_TIME));
	__raw_writel(1, qcom_vm_wdt_addr(wdt, WDT_RST));
	/*
	 * Ensure that bark, bite times, and reset is done, before
	 * moving forward.
	 */
	mb();

	return NOTIFY_DONE;
}

static void qcom_vm_wdt_disable(struct qcom_vm_wdt *wdt)
{
	if (hyp_enable == 1)
		__raw_writel(0, qcom_vm_wdt_addr(wdt, WDT_CTRL));
	/* Make sure watchdog is disabled before proceeding */
	mb();
	devm_free_irq(wdt->dev, wdt->bark_irq, wdt);
	enable = 0;
	/*Ensure all cpus see update to enable*/
	smp_mb();
	atomic_notifier_chain_unregister(&panic_notifier_list,
						&wdt->panic_blk);
	del_timer_sync(&wdt->pet_timer);
	/* may be suspended after the first write above */
	if (hyp_enable == 1)
		__raw_writel(0, qcom_vm_wdt_addr(wdt, WDT_CTRL));
	/* Make sure watchdog is disabled before setting enable */
	mb();
	pr_info("QCOM VM Watchdog deactivated.\n");
}

static ssize_t qcom_vm_wdt_disable_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	struct qcom_vm_wdt *wdt = dev_get_drvdata(dev);

	mutex_lock(&wdt->disable_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", enable == 0 ? 1 : 0);
	mutex_unlock(&wdt->disable_lock);
	return ret;
}

static ssize_t qcom_vm_wdt_disable_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u8 disable;
	struct qcom_vm_wdt *wdt = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 10, &disable);
	if (ret) {
		dev_err(wdt->dev, "invalid user input\n");
		return ret;
	}
	if (disable == 1) {
		mutex_lock(&wdt->disable_lock);
		if (enable == 0) {
			pr_info("QCOM VM Watchdog already disabled\n");
			mutex_unlock(&wdt->disable_lock);
			return count;
		}
		qcom_vm_wdt_disable(wdt);
		mutex_unlock(&wdt->disable_lock);
	} else {
		pr_err("invalid operation, only disable = 1 supported\n");
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(disable, 0600, qcom_vm_wdt_disable_get,
			qcom_vm_wdt_disable_set);

static void qcom_vm_wdt_hyp_disable(struct qcom_vm_wdt *wdt)
{
	__raw_writel(1 << 2, qcom_vm_wdt_addr(wdt, WDT_CTRL));
	hyp_enable = 0;
}

static ssize_t qcom_vm_wdt_hyp_disable_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	struct qcom_vm_wdt *wdt = dev_get_drvdata(dev);

	mutex_lock(&wdt->disable_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", hyp_enable == 0 ? 1 : 0);
	mutex_unlock(&wdt->disable_lock);
	return ret;
}

static ssize_t qcom_vm_wdt_hyp_disable_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u8 disable;
	struct qcom_vm_wdt *wdt = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 10, &disable);
	if (ret) {
		dev_err(wdt->dev, "invalid user input\n");
		return ret;
	}
	if (disable == 1) {
		mutex_lock(&wdt->disable_lock);
		if (hyp_enable == 0) {
			pr_info("QCOM VM Watchdog already disabled in Hyp\n");
			mutex_unlock(&wdt->disable_lock);
			return count;
		}
		qcom_vm_wdt_hyp_disable(wdt);
		mutex_unlock(&wdt->disable_lock);
	} else {
		pr_err("invalid operation, only hyp_disable = 1 supported\n");
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(hyp_disable, 0600, qcom_vm_wdt_hyp_disable_get,
			qcom_vm_wdt_hyp_disable_set);

static int qcom_vm_wdt_start(struct qcom_vm_wdt *wdt)
{
	__raw_writel(0, qcom_vm_wdt_addr(wdt, WDT_CTRL));
	__raw_writel(1, qcom_vm_wdt_addr(wdt, WDT_RST));
	__raw_writel((wdt->bark_time / 1000) * QCOM_VM_WDT_HZ,
			qcom_vm_wdt_addr(wdt, WDT_BARK_TIME));
	__raw_writel((wdt->bark_time / 1000 + 3) * QCOM_VM_WDT_HZ,
			qcom_vm_wdt_addr(wdt, WDT_BITE_TIME));
	__raw_writel(1, qcom_vm_wdt_addr(wdt, WDT_CTRL));
	return 0;
}

static void keep_alive_response(void *info)
{
	int cpu = smp_processor_id();
	struct qcom_vm_wdt *wdt = (struct qcom_vm_wdt *)info;

	cpumask_set_cpu(cpu, &wdt->alive_mask);
	/* Make sure alive mask is cleared and set in order */
	smp_mb();
}

static int qcom_vm_wdt_ping(struct qcom_vm_wdt *wdt)
{
	int cpu;

	cpumask_clear(&wdt->alive_mask);
	for_each_cpu(cpu, cpu_online_mask) {
		smp_call_function_single(cpu, keep_alive_response, wdt, 1);
	}
	__raw_writel(1, qcom_vm_wdt_addr(wdt, WDT_RST));
	wdt->last_pet = sched_clock();
	return 0;
}

static void vm_pet_task_wakeup(unsigned long data)
{
	struct qcom_vm_wdt *wdt = (struct qcom_vm_wdt *)data;

	wdt->timer_expired = true;
	wake_up(&wdt->pet_complete);
}

static __ref int vm_watchdog_kthread(void *arg)
{
	struct qcom_vm_wdt *wdt = (struct qcom_vm_wdt *)arg;

	while (!kthread_should_stop()) {
		while (wait_event_interruptible(
			wdt->pet_complete,
			wdt->timer_expired) != 0)
			;
		if (enable)
			qcom_vm_wdt_ping(wdt);
		wdt->timer_expired = false;
		mod_timer(&wdt->pet_timer, jiffies +
				msecs_to_jiffies(wdt->pet_time));
	}
	return 0;
}

static int qcom_vm_wdt_probe(struct platform_device *pdev)
{
	struct qcom_vm_wdt *wdt;
	struct resource *res;
	const u32 *regs;
	int ret, error;

	if (!pdev->dev.of_node || !enable)
		return -ENODEV;

	regs = of_device_get_match_data(&pdev->dev);
	if (!regs) {
		dev_err(&pdev->dev, "Unsupported QCOM WDT module\n");
		return -ENODEV;
	}

	wdt = devm_kzalloc(&pdev->dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wdt-base");
	if (!res) {
		ret = -ENODEV;
		goto err;
	}

	wdt->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(wdt->base)) {
		ret = PTR_ERR(wdt->base);
		goto err;
	}

	wdt->layout = regs;
	wdt->dev = &pdev->dev;

	wdt->bark_irq = platform_get_irq(pdev, 0);
	wdt->bite_irq = platform_get_irq(pdev, 1);
	wdt->last_pet = sched_clock();

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,bark-time",
					&wdt->bark_time);
	if (ret)
		wdt->bark_time = QCOM_VM_BARK_TIMEOUT * 1000;
	ret = of_property_read_u32(pdev->dev.of_node, "qcom,pet-time",
					&wdt->pet_time);
	if (ret)
		wdt->pet_time = QCOM_VM_PET_TIMEOUT * 1000;

	wdt->watchdog_task = kthread_create(vm_watchdog_kthread, wdt,
			"qcom_vm_watchdog");
	if (IS_ERR(wdt->watchdog_task)) {
		ret = PTR_ERR(wdt->watchdog_task);
		goto err;
	}

	init_waitqueue_head(&wdt->pet_complete);
	wdt->timer_expired = false;
	wake_up_process(wdt->watchdog_task);
	init_timer(&wdt->pet_timer);
	wdt->pet_timer.data = (unsigned long)wdt;
	wdt->pet_timer.function = vm_pet_task_wakeup;
	wdt->pet_timer.expires = jiffies +
				msecs_to_jiffies(wdt->pet_time);
	add_timer(&wdt->pet_timer);
	cpumask_clear(&wdt->alive_mask);

	wdt->panic_blk.notifier_call = qcom_vm_wdt_panic_handler;
	atomic_notifier_chain_register(&panic_notifier_list, &wdt->panic_blk);

	platform_set_drvdata(pdev, wdt);
	mutex_init(&wdt->disable_lock);

	error = device_create_file(wdt->dev, &dev_attr_disable);
	error |= device_create_file(wdt->dev, &dev_attr_hyp_disable);

	if (error)
		dev_err(wdt->dev, "cannot create sysfs attribute\n");

	qcom_vm_wdt_start(wdt);
	ret = devm_request_irq(&pdev->dev, wdt->bark_irq,
				qcom_vm_wdt_bark_handler, IRQF_TRIGGER_RISING,
						"apps_wdog_bark", wdt);

	if (hyp_enable == 0)
		qcom_vm_wdt_hyp_disable(wdt);
	return 0;

err:
	kfree(wdt);
	return ret;
}

static int qcom_vm_wdt_remove(struct platform_device *pdev)
{
	struct qcom_vm_wdt *wdt = platform_get_drvdata(pdev);

	mutex_lock(&wdt->disable_lock);
	if (enable)
		qcom_vm_wdt_disable(wdt);
	mutex_unlock(&wdt->disable_lock);
	device_remove_file(wdt->dev, &dev_attr_disable);
	device_remove_file(wdt->dev, &dev_attr_hyp_disable);
	dev_info(wdt->dev, "QCOM VM Watchdog Exit - Deactivated\n");
	del_timer_sync(&wdt->pet_timer);
	kthread_stop(wdt->watchdog_task);
	kfree(wdt);
	return 0;
}

static const struct of_device_id qcom_vm_wdt_of_table[] = {
	{ .compatible = "qcom,vm-wdt", .data = qcom_vm_wdt_reg_offset_data },
	{ },
};
MODULE_DEVICE_TABLE(of, qcom_wdt_of_table);

static const struct dev_pm_ops qcom_vm_wdt_dev_pm_ops = {
	.suspend_noirq = qcom_vm_wdt_suspend,
	.resume_noirq = qcom_vm_wdt_resume,
};

static struct platform_driver qcom_vm_watchdog_driver = {
	.probe	= qcom_vm_wdt_probe,
	.remove	= qcom_vm_wdt_remove,
	.driver	= {
		.name		= KBUILD_MODNAME,
		.pm = &qcom_vm_wdt_dev_pm_ops,
		.of_match_table	= qcom_vm_wdt_of_table,
	},
};
module_platform_driver(qcom_vm_watchdog_driver);

MODULE_DESCRIPTION("QCOM VM Watchdog Driver");
MODULE_LICENSE("GPL v2");
