/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/percpu.h>
#include <linux/of.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/platform_device.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/watchdog.h>
#include <linux/kmemleak.h>

#define MODULE_NAME "msm_watchdog"
#define WDT0_ACCSCSSNBARK_INT 0
#define TCSR_WDT_CFG	0x30
#define WDT0_RST	0x04
#define WDT0_EN		0x08
#define WDT0_STS	0x0C
#define WDT0_BARK_TIME	0x10
#define WDT0_BITE_TIME	0x14

#define WDOG_ABSENT	0

#define EN		0
#define UNMASKED_INT_EN 1

#define MASK_SIZE		32
#define SCM_SET_REGSAVE_CMD	0x2
#define SCM_SVC_SEC_WDOG_DIS	0x7
#define MAX_CPU_CTX_SIZE	2048

static struct workqueue_struct *wdog_wq;
static struct msm_watchdog_data *wdog_data;

static int cpu_idle_pc_state[NR_CPUS];

struct msm_watchdog_data {
	unsigned int __iomem phys_base;
	size_t size;
	void __iomem *base;
	void __iomem *wdog_absent_base;
	struct device *dev;
	unsigned int pet_time;
	unsigned int bark_time;
	unsigned int bark_irq;
	unsigned int bite_irq;
	bool do_ipi_ping;
	bool wakeup_irq_enable;
	unsigned long long last_pet;
	unsigned min_slack_ticks;
	unsigned long long min_slack_ns;
	void *scm_regsave;
	cpumask_t alive_mask;
	struct mutex disable_lock;
	struct work_struct init_dogwork_struct;
	struct delayed_work dogwork_struct;
	bool irq_ppi;
	struct msm_watchdog_data __percpu **wdog_cpu_dd;
	struct notifier_block panic_blk;
	bool enabled;
};

/*
 * On the kernel command line specify
 * watchdog_v2.enable=1 to enable the watchdog
 * By default watchdog is turned on
 */
static int enable = 1;
module_param(enable, int, 0);

/*
 * On the kernel command line specify
 * watchdog_v2.WDT_HZ=<clock val in HZ> to set Watchdog
 * ticks. By default it is set to 32765.
 */
static long WDT_HZ = 32765;
module_param(WDT_HZ, long, 0);

/*
 * On the kernel command line specify
 * watchdog_v2.ipi_opt_en=1 to enable the watchdog ipi ping
 * optimization. By default it is turned off
 */
static int ipi_opt_en;
module_param(ipi_opt_en, int, 0);

static void pet_watchdog_work(struct work_struct *work);
static void init_watchdog_work(struct work_struct *work);

static void dump_cpu_alive_mask(struct msm_watchdog_data *wdog_dd)
{
	static char alive_mask_buf[MASK_SIZE];
	cpulist_scnprintf(alive_mask_buf, MASK_SIZE,
						&wdog_dd->alive_mask);
	printk(KERN_INFO "cpu alive mask from last pet %s\n", alive_mask_buf);
}

static int msm_watchdog_suspend(struct device *dev)
{
	struct msm_watchdog_data *wdog_dd =
			(struct msm_watchdog_data *)dev_get_drvdata(dev);
	if (!enable)
		return 0;
	__raw_writel(1, wdog_dd->base + WDT0_RST);
	if (wdog_dd->wakeup_irq_enable) {
		/* Make sure register write is complete before proceeding */
		mb();
		wdog_dd->last_pet = sched_clock();
		return 0;
	}
	__raw_writel(0, wdog_dd->base + WDT0_EN);
	mb();
	wdog_dd->enabled = false;
	wdog_dd->last_pet = sched_clock();
	return 0;
}

static int msm_watchdog_resume(struct device *dev)
{
	struct msm_watchdog_data *wdog_dd =
			(struct msm_watchdog_data *)dev_get_drvdata(dev);
	if (!enable)
		return 0;
	if (wdog_dd->wakeup_irq_enable) {
		__raw_writel(1, wdog_dd->base + WDT0_RST);
		/* Make sure register write is complete before proceeding */
		mb();
		wdog_dd->last_pet = sched_clock();
		return 0;
	}
	__raw_writel(1, wdog_dd->base + WDT0_EN);
	__raw_writel(1, wdog_dd->base + WDT0_RST);
	mb();
	wdog_dd->enabled = true;
	wdog_dd->last_pet = sched_clock();
	return 0;
}

static int panic_wdog_handler(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	struct msm_watchdog_data *wdog_dd = container_of(this,
				struct msm_watchdog_data, panic_blk);
	if (panic_timeout == 0) {
		__raw_writel(0, wdog_dd->base + WDT0_EN);
		mb();
	} else {
		__raw_writel(WDT_HZ * (panic_timeout + 10),
				wdog_dd->base + WDT0_BARK_TIME);
		__raw_writel(WDT_HZ * (panic_timeout + 10),
				wdog_dd->base + WDT0_BITE_TIME);
		__raw_writel(1, wdog_dd->base + WDT0_RST);
	}
	return NOTIFY_DONE;
}

static void wdog_disable(struct msm_watchdog_data *wdog_dd)
{
	__raw_writel(0, wdog_dd->base + WDT0_EN);
	mb();
	if (wdog_dd->irq_ppi) {
		disable_percpu_irq(wdog_dd->bark_irq);
		free_percpu_irq(wdog_dd->bark_irq, wdog_dd->wdog_cpu_dd);
	} else
		devm_free_irq(wdog_dd->dev, wdog_dd->bark_irq, wdog_dd);
	enable = 0;
	/*Ensure all cpus see update to enable*/
	smp_mb();
	atomic_notifier_chain_unregister(&panic_notifier_list,
						&wdog_dd->panic_blk);
	cancel_delayed_work_sync(&wdog_dd->dogwork_struct);
	/* may be suspended after the first write above */
	__raw_writel(0, wdog_dd->base + WDT0_EN);
	mb();
	wdog_dd->enabled = false;
	pr_info("MSM Apps Watchdog deactivated.\n");
}

struct wdog_disable_work_data {
	struct work_struct work;
	struct completion complete;
	struct msm_watchdog_data *wdog_dd;
};

static void wdog_disable_work(struct work_struct *work)
{
	struct wdog_disable_work_data *work_data =
		container_of(work, struct wdog_disable_work_data, work);
	wdog_disable(work_data->wdog_dd);
	complete(&work_data->complete);
}

static ssize_t wdog_disable_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);

	mutex_lock(&wdog_dd->disable_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", enable == 0 ? 1 : 0);
	mutex_unlock(&wdog_dd->disable_lock);
	return ret;
}

static ssize_t wdog_disable_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u8 disable;
	struct wdog_disable_work_data work_data;
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 10, &disable);
	if (ret) {
		dev_err(wdog_dd->dev, "invalid user input\n");
		return ret;
	}
	if (disable == 1) {
		mutex_lock(&wdog_dd->disable_lock);
		if (enable == 0) {
			pr_info("MSM Apps Watchdog already disabled\n");
			mutex_unlock(&wdog_dd->disable_lock);
			return count;
		}
		disable = 1;
		if (!is_scm_armv8()) {
			ret = scm_call(SCM_SVC_BOOT, SCM_SVC_SEC_WDOG_DIS,
				       &disable, sizeof(disable), NULL, 0);
		} else {
			struct scm_desc desc = {0};
			desc.args[0] = 1;
			desc.arginfo = SCM_ARGS(1);
			ret = scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT,
					SCM_SVC_SEC_WDOG_DIS), &desc);
		}
		if (ret) {
			dev_err(wdog_dd->dev,
					"Failed to deactivate secure wdog\n");
			mutex_unlock(&wdog_dd->disable_lock);
			return -EIO;
		}
		work_data.wdog_dd = wdog_dd;
		init_completion(&work_data.complete);
		INIT_WORK_ONSTACK(&work_data.work, wdog_disable_work);
		queue_work(wdog_wq, &work_data.work);
		wait_for_completion(&work_data.complete);
		mutex_unlock(&wdog_dd->disable_lock);
	} else {
		pr_err("invalid operation, only disable = 1 supported\n");
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(disable, S_IWUSR | S_IRUSR, wdog_disable_get,
							wdog_disable_set);

static void pet_watchdog(struct msm_watchdog_data *wdog_dd)
{
	int slack, i, count, prev_count = 0;
	unsigned long long time_ns;
	unsigned long long slack_ns;
	unsigned long long bark_time_ns = wdog_dd->bark_time * 1000000ULL;

	for (i = 0; i < 2; i++) {
		count = (__raw_readl(wdog_dd->base + WDT0_STS) >> 1) & 0xFFFFF;
		if (count != prev_count) {
			prev_count = count;
			i = 0;
		}
	}
	slack = ((wdog_dd->bark_time * WDT_HZ) / 1000) - count;
	if (slack < wdog_dd->min_slack_ticks)
		wdog_dd->min_slack_ticks = slack;
	__raw_writel(1, wdog_dd->base + WDT0_RST);
	time_ns = sched_clock();
	slack_ns = (wdog_dd->last_pet + bark_time_ns) - time_ns;
	if (slack_ns < wdog_dd->min_slack_ns)
		wdog_dd->min_slack_ns = slack_ns;
	wdog_dd->last_pet = time_ns;
}

static void keep_alive_response(void *info)
{
	int cpu = smp_processor_id();
	struct msm_watchdog_data *wdog_dd = (struct msm_watchdog_data *)info;
	cpumask_set_cpu(cpu, &wdog_dd->alive_mask);
	smp_mb();
}

/*
 * If this function does not return, it implies one of the
 * other cpu's is not responsive.
 */
static void ping_other_cpus(struct msm_watchdog_data *wdog_dd)
{
	int cpu;
	cpumask_clear(&wdog_dd->alive_mask);
	smp_mb();
	for_each_cpu(cpu, cpu_online_mask) {
		if (!cpu_idle_pc_state[cpu])
			smp_call_function_single(cpu, keep_alive_response,
						 wdog_dd, 1);
	}
}

static void pet_watchdog_work(struct work_struct *work)
{
	unsigned long delay_time;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct msm_watchdog_data *wdog_dd = container_of(delayed_work,
						struct msm_watchdog_data,
							dogwork_struct);
	delay_time = msecs_to_jiffies(wdog_dd->pet_time);
	if (enable) {
		if (wdog_dd->do_ipi_ping)
			ping_other_cpus(wdog_dd);
		pet_watchdog(wdog_dd);
	}
	/* Check again before scheduling *
	 * Could have been changed on other cpu */
	if (enable)
		queue_delayed_work(wdog_wq,
				&wdog_dd->dogwork_struct, delay_time);
}

static int wdog_cpu_pm_notify(struct notifier_block *self,
			      unsigned long action, void *v)
{
	int cpu;

	cpu = raw_smp_processor_id();

	switch (action) {
	case CPU_PM_ENTER:
		cpu_idle_pc_state[cpu] = 1;
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		cpu_idle_pc_state[cpu] = 0;
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block wdog_cpu_pm_nb = {
	.notifier_call = wdog_cpu_pm_notify,
};

static int msm_watchdog_remove(struct platform_device *pdev)
{
	struct wdog_disable_work_data work_data;
	struct msm_watchdog_data *wdog_dd =
			(struct msm_watchdog_data *)platform_get_drvdata(pdev);

	if (ipi_opt_en)
		cpu_pm_unregister_notifier(&wdog_cpu_pm_nb);

	mutex_lock(&wdog_dd->disable_lock);
	if (enable) {
		work_data.wdog_dd = wdog_dd;
		init_completion(&work_data.complete);
		INIT_WORK_ONSTACK(&work_data.work, wdog_disable_work);
		queue_work(wdog_wq, &work_data.work);
		wait_for_completion(&work_data.complete);
	}
	mutex_unlock(&wdog_dd->disable_lock);
	device_remove_file(wdog_dd->dev, &dev_attr_disable);
	if (wdog_dd->irq_ppi)
		free_percpu(wdog_dd->wdog_cpu_dd);
	printk(KERN_INFO "MSM Watchdog Exit - Deactivated\n");
	destroy_workqueue(wdog_wq);
	kfree(wdog_dd);
	return 0;
}

void msm_trigger_wdog_bite(void)
{
	if (!wdog_data)
		return;
	pr_info("Causing a watchdog bite!");
	__raw_writel(1, wdog_data->base + WDT0_BITE_TIME);
	mb();
	__raw_writel(1, wdog_data->base + WDT0_RST);
	mb();
	/* Delay to make sure bite occurs */
	mdelay(1);
	pr_err("Wdog - STS: 0x%x, CTL: 0x%x, BARK TIME: 0x%x, BITE TIME: 0x%x",
		__raw_readl(wdog_data->base + WDT0_STS),
		__raw_readl(wdog_data->base + WDT0_EN),
		__raw_readl(wdog_data->base + WDT0_BARK_TIME),
		__raw_readl(wdog_data->base + WDT0_BITE_TIME));
}

static irqreturn_t wdog_bark_handler(int irq, void *dev_id)
{
	struct msm_watchdog_data *wdog_dd = (struct msm_watchdog_data *)dev_id;
	unsigned long nanosec_rem;
	unsigned long long t = sched_clock();

	nanosec_rem = do_div(t, 1000000000);
	printk(KERN_INFO "Watchdog bark! Now = %lu.%06lu\n", (unsigned long) t,
		nanosec_rem / 1000);

	nanosec_rem = do_div(wdog_dd->last_pet, 1000000000);
	printk(KERN_INFO "Watchdog last pet at %lu.%06lu\n", (unsigned long)
		wdog_dd->last_pet, nanosec_rem / 1000);
	if (wdog_dd->do_ipi_ping)
		dump_cpu_alive_mask(wdog_dd);
	msm_trigger_wdog_bite();
	panic("Failed to cause a watchdog bite! - Falling back to kernel panic!");
	return IRQ_HANDLED;
}

static irqreturn_t wdog_ppi_bark(int irq, void *dev_id)
{
	struct msm_watchdog_data *wdog_dd =
			*(struct msm_watchdog_data **)(dev_id);
	return wdog_bark_handler(irq, wdog_dd);
}

static void configure_bark_dump(struct msm_watchdog_data *wdog_dd)
{
	int ret;
	struct msm_client_dump cpu_dump_entry;
	struct msm_dump_entry dump_entry;
	struct msm_dump_data *cpu_data;
	int cpu;
	void *cpu_buf;
	struct {
		unsigned addr;
		int len;
	} cmd_buf;
	struct scm_desc desc = {0};

	if (MSM_DUMP_MAJOR(msm_dump_table_version()) == 1) {
		wdog_dd->scm_regsave = (void *)__get_free_page(GFP_KERNEL);
		if (wdog_dd->scm_regsave) {
			/* scm_regsave may be a phys address > 4GB */
			desc.args[0] = virt_to_phys(wdog_dd->scm_regsave);
			cmd_buf.addr = virt_to_phys(wdog_dd->scm_regsave);
			desc.args[1] = cmd_buf.len  = PAGE_SIZE;
			desc.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);

			if (!is_scm_armv8())
				ret = scm_call(SCM_SVC_UTIL,
					       SCM_SET_REGSAVE_CMD, &cmd_buf,
					       sizeof(cmd_buf), NULL, 0);
			else
				ret = scm_call2(SCM_SIP_FNID(SCM_SVC_UTIL,
						SCM_SET_REGSAVE_CMD), &desc);
			if (ret)
				pr_err("Setting register save address failed.\n"
				       "Registers won't be dumped on a dog "
				       "bite\n");
			cpu_dump_entry.id = MSM_CPU_CTXT;
			cpu_dump_entry.start_addr =
					virt_to_phys(wdog_dd->scm_regsave);
			cpu_dump_entry.end_addr = cpu_dump_entry.start_addr +
						  PAGE_SIZE;
			ret = msm_dump_tbl_register(&cpu_dump_entry);
			if (ret)
				pr_err("Setting cpu dump region failed\n"
				"Registers wont be dumped during cpu hang\n");
		} else {
			pr_err("Allocating register save space failed\n"
			       "Registers won't be dumped on a dog bite\n");
			/*
			 * No need to bail if allocation fails. Simply don't
			 * send the command, and the secure side will reset
			 * without saving registers.
			 */
		}
	} else {
		cpu_data = kzalloc(sizeof(struct msm_dump_data) *
				   num_present_cpus(), GFP_KERNEL);
		if (!cpu_data) {
			pr_err("cpu dump data structure allocation failed\n");
			goto out0;
		}
		kmemleak_not_leak(cpu_data);
		cpu_buf = kzalloc(MAX_CPU_CTX_SIZE * num_present_cpus(),
				  GFP_KERNEL);
		if (!cpu_buf) {
			pr_err("cpu reg context space allocation failed\n");
			goto out1;
		}
		kmemleak_not_leak(cpu_buf);
		for_each_cpu(cpu, cpu_present_mask) {
			cpu_data[cpu].addr = virt_to_phys(cpu_buf +
							cpu * MAX_CPU_CTX_SIZE);
			cpu_data[cpu].len = MAX_CPU_CTX_SIZE;
			dump_entry.id = MSM_DUMP_DATA_CPU_CTX + cpu;
			dump_entry.addr = virt_to_phys(&cpu_data[cpu]);
			ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS,
						     &dump_entry);
			/*
			 * Don't free the buffers in case of error since
			 * registration may have succeeded for some cpus.
			 */
			if (ret)
				pr_err("cpu %d reg dump setup failed\n", cpu);
		}
	}

	return;
out1:
	kfree(cpu_data);
out0:
	return;
}


static void init_watchdog_work(struct work_struct *work)
{
	struct msm_watchdog_data *wdog_dd = container_of(work,
						struct msm_watchdog_data,
							init_dogwork_struct);
	unsigned long delay_time;
	uint32_t val;
	int error;
	u64 timeout;
	int ret;

	/*
	 * Disable the watchdog for cluster 1 so that cluster 0 watchdog will
	 * be mapped to the entire sub-system.
	 */
	if (wdog_dd->wdog_absent_base)
		__raw_writel(2, wdog_dd->wdog_absent_base + WDOG_ABSENT);

	if (wdog_dd->irq_ppi) {
		wdog_dd->wdog_cpu_dd = alloc_percpu(struct msm_watchdog_data *);
		if (!wdog_dd->wdog_cpu_dd) {
			dev_err(wdog_dd->dev, "fail to allocate cpu data\n");
			return;
		}
		*__this_cpu_ptr(wdog_dd->wdog_cpu_dd) = wdog_dd;
		ret = request_percpu_irq(wdog_dd->bark_irq, wdog_ppi_bark,
					"apps_wdog_bark",
					wdog_dd->wdog_cpu_dd);
		if (ret) {
			dev_err(wdog_dd->dev, "failed to request bark irq\n");
			free_percpu(wdog_dd->wdog_cpu_dd);
			return;
		}
	} else {
		ret = devm_request_irq(wdog_dd->dev, wdog_dd->bark_irq,
				wdog_bark_handler, IRQF_TRIGGER_RISING,
						"apps_wdog_bark", wdog_dd);
		if (ret) {
			dev_err(wdog_dd->dev, "failed to request bark irq\n");
			return;
		}
	}
	delay_time = msecs_to_jiffies(wdog_dd->pet_time);
	wdog_dd->min_slack_ticks = UINT_MAX;
	wdog_dd->min_slack_ns = ULLONG_MAX;
	configure_bark_dump(wdog_dd);
	timeout = (wdog_dd->bark_time * WDT_HZ)/1000;
	__raw_writel(timeout, wdog_dd->base + WDT0_BARK_TIME);
	__raw_writel(timeout + 3*WDT_HZ, wdog_dd->base + WDT0_BITE_TIME);

	wdog_dd->panic_blk.notifier_call = panic_wdog_handler;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &wdog_dd->panic_blk);
	mutex_init(&wdog_dd->disable_lock);
	queue_delayed_work(wdog_wq, &wdog_dd->dogwork_struct,
			delay_time);
	val = BIT(EN);
	if (wdog_dd->wakeup_irq_enable)
		val |= BIT(UNMASKED_INT_EN);
	__raw_writel(val, wdog_dd->base + WDT0_EN);
	__raw_writel(1, wdog_dd->base + WDT0_RST);
	wdog_dd->last_pet = sched_clock();
	wdog_dd->enabled = true;
	error = device_create_file(wdog_dd->dev, &dev_attr_disable);
	if (error)
		dev_err(wdog_dd->dev, "cannot create sysfs attribute\n");
	if (wdog_dd->irq_ppi)
		enable_percpu_irq(wdog_dd->bark_irq, 0);
	if (ipi_opt_en)
		cpu_pm_register_notifier(&wdog_cpu_pm_nb);
	dev_info(wdog_dd->dev, "MSM Watchdog Initialized\n");
	return;
}

static struct of_device_id msm_wdog_match_table[] = {
	{ .compatible = "qcom,msm-watchdog" },
	{}
};

static void dump_pdata(struct msm_watchdog_data *pdata)
{
	dev_dbg(pdata->dev, "wdog bark_time %d", pdata->bark_time);
	dev_dbg(pdata->dev, "wdog pet_time %d", pdata->pet_time);
	dev_dbg(pdata->dev, "wdog perform ipi ping %d", pdata->do_ipi_ping);
	dev_dbg(pdata->dev, "wdog base address is 0x%lx\n", (unsigned long)
								pdata->base);
}

static int msm_wdog_dt_to_pdata(struct platform_device *pdev,
					struct msm_watchdog_data *pdata)
{
	struct device_node *node = pdev->dev.of_node;
	struct resource *res;
	int ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wdt-base");
	if (!res)
		return -ENODEV;
	pdata->size = resource_size(res);
	pdata->phys_base = res->start;
	if (unlikely(!(devm_request_mem_region(&pdev->dev, pdata->phys_base,
					       pdata->size, "msm-watchdog")))) {

		dev_err(&pdev->dev, "%s cannot reserve watchdog region\n",
								__func__);
		return -ENXIO;
	}
	pdata->base  = devm_ioremap(&pdev->dev, pdata->phys_base,
							pdata->size);
	if (!pdata->base) {
		dev_err(&pdev->dev, "%s cannot map wdog register space\n",
				__func__);
		return -ENXIO;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "wdt-absent-base");
	if (res) {
		pdata->wdog_absent_base  = devm_ioremap(&pdev->dev, res->start,
							 resource_size(res));
		if (!pdata->wdog_absent_base) {
			dev_err(&pdev->dev,
				"cannot map wdog absent register space\n");
			return -ENXIO;
		}
	} else {
		dev_info(&pdev->dev, "wdog absent resource not present\n");
	}

	pdata->bark_irq = platform_get_irq(pdev, 0);
	pdata->bite_irq = platform_get_irq(pdev, 1);
	ret = of_property_read_u32(node, "qcom,bark-time", &pdata->bark_time);
	if (ret) {
		dev_err(&pdev->dev, "reading bark time failed\n");
		return -ENXIO;
	}
	ret = of_property_read_u32(node, "qcom,pet-time", &pdata->pet_time);
	if (ret) {
		dev_err(&pdev->dev, "reading pet time failed\n");
		return -ENXIO;
	}
	pdata->do_ipi_ping = of_property_read_bool(node, "qcom,ipi-ping");
	if (!pdata->bark_time) {
		dev_err(&pdev->dev, "%s watchdog bark time not setup\n",
								__func__);
		return -ENXIO;
	}
	if (!pdata->pet_time) {
		dev_err(&pdev->dev, "%s watchdog pet time not setup\n",
								__func__);
		return -ENXIO;
	}
	pdata->wakeup_irq_enable = of_property_read_bool(node,
							 "qcom,wakeup-enable");

	pdata->irq_ppi = irq_is_percpu(pdata->bark_irq);
	dump_pdata(pdata);
	return 0;
}

static int msm_watchdog_probe(struct platform_device *pdev)
{
	int ret;
	struct msm_watchdog_data *wdog_dd;

	wdog_wq = alloc_workqueue("wdog", WQ_HIGHPRI, 0);
	if (!wdog_wq) {
		pr_err("Failed to allocate watchdog workqueue\n");
		return -EIO;
	}

	if (!pdev->dev.of_node || !enable)
		return -ENODEV;
	wdog_dd = kzalloc(sizeof(struct msm_watchdog_data), GFP_KERNEL);
	if (!wdog_dd)
		return -EIO;
	ret = msm_wdog_dt_to_pdata(pdev, wdog_dd);
	if (ret)
		goto err;

	wdog_data = wdog_dd;
	wdog_dd->dev = &pdev->dev;
	platform_set_drvdata(pdev, wdog_dd);
	cpumask_clear(&wdog_dd->alive_mask);
	INIT_WORK(&wdog_dd->init_dogwork_struct, init_watchdog_work);
	INIT_DELAYED_WORK(&wdog_dd->dogwork_struct, pet_watchdog_work);
	queue_work(wdog_wq, &wdog_dd->init_dogwork_struct);
	return 0;
err:
	destroy_workqueue(wdog_wq);
	kzfree(wdog_dd);
	return ret;
}

static const struct dev_pm_ops msm_watchdog_dev_pm_ops = {
	.suspend_noirq = msm_watchdog_suspend,
	.resume_noirq = msm_watchdog_resume,
};

static struct platform_driver msm_watchdog_driver = {
	.probe = msm_watchdog_probe,
	.remove = msm_watchdog_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &msm_watchdog_dev_pm_ops,
		.of_match_table = msm_wdog_match_table,
	},
};

static int init_watchdog(void)
{
	return platform_driver_register(&msm_watchdog_driver);
}

pure_initcall(init_watchdog);
MODULE_DESCRIPTION("MSM Watchdog Driver");
MODULE_LICENSE("GPL v2");
