// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/sort.h>
#include <linux/kernel_stat.h>
#include <linux/irq_cpustat.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/percpu.h>
#include <linux/of.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/minidump.h>
#include <soc/qcom/watchdog.h>
#include <linux/dma-mapping.h>
#include <linux/sched/clock.h>
#include <linux/cpumask.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/debug.h>
#ifdef CONFIG_QCOM_INITIAL_LOGBUF
#include <linux/kallsyms.h>
#include <linux/math64.h>
#endif

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
#define NR_TOP_HITTERS		10
#define COMPARE_RET		-1

typedef int (*compare_t) (const void *lhs, const void *rhs);

#ifdef CONFIG_QCOM_INITIAL_LOGBUF
#define LOGBUF_TIMEOUT		100000U

static struct delayed_work log_buf_work;
static char *init_log_buf;
static unsigned int *log_buf_size;
static dma_addr_t log_buf_paddr;
#endif

static struct msm_watchdog_data *wdog_data;

static int cpu_idle_pc_state[NR_CPUS];

struct irq_info {
	unsigned int irq;
	unsigned int total_count;
	unsigned int irq_counter[NR_CPUS];
};

/*
 * user_pet_enable:
 *	Require userspace to write to a sysfs file every pet_time milliseconds.
 *	Disabled by default on boot.
 */
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
	unsigned int min_slack_ticks;
	unsigned long long min_slack_ns;
	void *scm_regsave;
	cpumask_t alive_mask;
	struct mutex disable_lock;
	bool irq_ppi;
	struct msm_watchdog_data __percpu **wdog_cpu_dd;
	struct notifier_block panic_blk;

	bool enabled;
	bool user_pet_enabled;

	struct task_struct *watchdog_task;
	struct timer_list pet_timer;
	wait_queue_head_t pet_complete;

	bool timer_expired;
	bool user_pet_complete;
	unsigned long long timer_fired;
	unsigned long long thread_start;
	unsigned long long ping_start[NR_CPUS];
	unsigned long long ping_end[NR_CPUS];
	struct work_struct irq_counts_work;
	struct irq_info irq_counts[NR_TOP_HITTERS];
	struct irq_info ipi_counts[NR_IPI];
	unsigned int tot_irq_count[NR_CPUS];
	atomic_t irq_counts_running;
};

/*
 * On the kernel command line specify
 * watchdog_v2.enable=1 to enable the watchdog
 * By default watchdog is turned on
 */
static int enable = 1;
module_param(enable, int, 0000);

/*
 * On the kernel command line specify
 * watchdog_v2.WDT_HZ=<clock val in HZ> to set Watchdog
 * ticks. By default it is set to 32765.
 */
static long WDT_HZ = 32765;
module_param(WDT_HZ, long, 0000);

/*
 * Watchdog ipi optimization:
 * Does not ping cores in low power mode at pet time to save power.
 * This feature is enabled by default.
 *
 * On the kernel command line specify
 * watchdog_v2.ipi_en=1 to disable this optimization.
 * Or, can be turned off, by enabling CONFIG_QCOM_WDOG_IPI_ENABLE.
 */
#ifdef CONFIG_QCOM_WDOG_IPI_ENABLE
#define IPI_CORES_IN_LPM 1
#else
#define IPI_CORES_IN_LPM 0
#endif

static int ipi_en = IPI_CORES_IN_LPM;
module_param(ipi_en, int, 0444);

#ifdef CONFIG_FIRE_WATCHDOG
static int wdog_fire;
static int wdog_fire_set(const char *val, const struct kernel_param *kp);
module_param_call(wdog_fire, wdog_fire_set, param_get_int,
				&wdog_fire, 0644);

static int wdog_fire_set(const char *val, const struct kernel_param *kp)
{
	printk(KERN_INFO "trigger wdog_fire_set\n");
	local_irq_disable();
	while (1);
	return 0;
}
#endif

static void dump_cpu_alive_mask(struct msm_watchdog_data *wdog_dd)
{
	static char alive_mask_buf[MASK_SIZE];

	scnprintf(alive_mask_buf, MASK_SIZE, "%*pb1", cpumask_pr_args(
				&wdog_dd->alive_mask));
	dev_info(wdog_dd->dev, "cpu alive mask from last pet %s\n",
				alive_mask_buf);
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
	/* Make sure watchdog is suspended before setting enable */
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
	/* Make sure watchdog is reset before setting enable */
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
		/* Make sure watchdog is enabled before notifying the caller */
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
	/* Make sure watchdog is disabled before proceeding */
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
	del_timer_sync(&wdog_dd->pet_timer);
	/* may be suspended after the first write above */
	__raw_writel(0, wdog_dd->base + WDT0_EN);
	/* Make sure watchdog is disabled before setting enable */
	mb();
	wdog_dd->enabled = false;
	pr_info("MSM Apps Watchdog deactivated.\n");
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
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);
	struct scm_desc desc = {0};

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

		desc.args[0] = 1;
		desc.arginfo = SCM_ARGS(1);
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT,
						SCM_SVC_SEC_WDOG_DIS), &desc);
		if (ret) {
			dev_err(wdog_dd->dev,
					"Failed to deactivate secure wdog\n");
			mutex_unlock(&wdog_dd->disable_lock);
			return -EIO;
		}
		wdog_disable(wdog_dd);
		mutex_unlock(&wdog_dd->disable_lock);
	} else {
		pr_err("invalid operation, only disable = 1 supported\n");
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(disable, 0600, wdog_disable_get, wdog_disable_set);

/*
 * Userspace Watchdog Support:
 * Write 1 to the "user_pet_enabled" file to enable hw support for a
 * userspace watchdog.
 * Userspace is required to pet the watchdog by continuing to write 1
 * to this file in the expected interval.
 * Userspace may disable this requirement by writing 0 to this same
 * file.
 */
static void __wdog_user_pet(struct msm_watchdog_data *wdog_dd)
{
	wdog_dd->user_pet_complete = true;
	wake_up(&wdog_dd->pet_complete);
}

static ssize_t wdog_user_pet_enabled_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
			wdog_dd->user_pet_enabled);
	return ret;
}

static ssize_t wdog_user_pet_enabled_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);

	ret = strtobool(buf, &wdog_dd->user_pet_enabled);
	if (ret) {
		dev_err(wdog_dd->dev, "invalid user input\n");
		return ret;
	}

	__wdog_user_pet(wdog_dd);

	return count;
}

static DEVICE_ATTR(user_pet_enabled, 0600, wdog_user_pet_enabled_get,
						wdog_user_pet_enabled_set);

static ssize_t wdog_pet_time_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", wdog_dd->pet_time);
	return ret;
}

static DEVICE_ATTR(pet_time, 0400, wdog_pet_time_get, NULL);

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
	wdog_dd->ping_end[cpu] = sched_clock();
	/* Make sure alive mask is cleared and set in order */
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
	/* Make sure alive mask is cleared and set in order */
	smp_mb();
	for_each_cpu(cpu, cpu_online_mask) {
		if (!cpu_idle_pc_state[cpu] && !cpu_isolated(cpu)) {
			wdog_dd->ping_start[cpu] = sched_clock();
			smp_call_function_single(cpu, keep_alive_response,
						 wdog_dd, 1);
		}
	}
}

static void pet_task_wakeup(struct timer_list *t)
{
	struct msm_watchdog_data *wdog_dd =
		from_timer(wdog_dd, t, pet_timer);
	wdog_dd->timer_expired = true;
	wdog_dd->timer_fired = sched_clock();
	wake_up(&wdog_dd->pet_complete);
}

static int cmp_irq_info_fn(const void *a, const void *b)
{
	struct irq_info *lhs = (struct irq_info *)a;
	struct irq_info *rhs = (struct irq_info *)b;

	if (lhs->total_count < rhs->total_count)
		return 1;

	if (lhs->total_count > rhs->total_count)
		return COMPARE_RET;

	return 0;
}

static void swap_irq_info_fn(void *a, void *b, int size)
{
	struct irq_info temp;
	struct irq_info *lhs = (struct irq_info *)a;
	struct irq_info *rhs = (struct irq_info *)b;

	temp = *lhs;
	*lhs = *rhs;
	*rhs = temp;
}

static struct irq_info *search(struct irq_info *key, struct irq_info *base,
			       size_t num, compare_t cmp)
{
	struct irq_info *pivot = NULL;
	int result;

	while (num > 0) {
		pivot = base + (num >> 1);
		result = cmp(key, pivot);

		if (result == 0)
			goto out;

		if (result > 0) {
			base = pivot + 1;
			num--;
		}

		if (num)
			num >>= 1;
	}

out:
	return pivot;
}

static void print_irq_stat(struct msm_watchdog_data *wdog_dd)
{
	int index;
	int cpu;
	struct irq_info *info;


	pr_debug("(virq:irq_count)-\n");
	for (index = 0; index < NR_TOP_HITTERS; index++) {
		info = &wdog_dd->irq_counts[index];
		pr_debug("%u:%u\n", info->irq, info->total_count);
	}
	pr_debug("\n");

	pr_debug("(cpu:irq_count)-\n");
	for_each_possible_cpu(cpu)
		pr_debug("%u:%u\n", cpu, wdog_dd->tot_irq_count[cpu]);
	pr_debug("\n");

	pr_debug("(ipi:irq_count)-\n");
	for (index = 0; index < NR_IPI; index++) {
		info = &wdog_dd->ipi_counts[index];
		pr_debug("%u:%u\n", info->irq, info->total_count);
	}
	pr_debug("\n");
}

static void compute_irq_stat(struct work_struct *work)
{
	unsigned int count;
	int index = 0, cpu, irq;
	struct irq_desc *desc;
	struct irq_info *pos;
	struct irq_info *start;
	struct irq_info key = {0};
	unsigned int running;
	struct msm_watchdog_data *wdog_dd = container_of(work,
					    struct msm_watchdog_data,
					    irq_counts_work);

	size_t arr_size = ARRAY_SIZE(wdog_dd->irq_counts);

	/* avoid parallel execution from bark handler and queued
	 * irq_counts_work.
	 */
	running = atomic_xchg(&wdog_dd->irq_counts_running, 1);
	if (running)
		return;

	/* per irq counts */
	rcu_read_lock();
	for_each_irq_nr(irq) {
		desc = irq_to_desc(irq);
		if (!desc)
			continue;

		count = kstat_irqs_usr(irq);
		if (!count)
			continue;

		if (index < arr_size) {
			wdog_dd->irq_counts[index].irq = irq;
			wdog_dd->irq_counts[index].total_count = count;
			for_each_possible_cpu(cpu)
				wdog_dd->irq_counts[index].irq_counter[cpu] =
					*per_cpu_ptr(desc->kstat_irqs, cpu);

			index++;
			if (index == arr_size)
				sort(wdog_dd->irq_counts, arr_size,
				     sizeof(*pos), cmp_irq_info_fn,
				     swap_irq_info_fn);

			continue;
		}

		key.total_count = count;
		start = wdog_dd->irq_counts + (arr_size - 1);
		pos = search(&key, wdog_dd->irq_counts,
			     arr_size, cmp_irq_info_fn);
		pr_debug("*pos:%u key:%u\n",
				pos->total_count, key.total_count);
		if (pos && (pos->total_count >= key.total_count)) {
			if (pos < start)
				pos++;
			else
				pos = NULL;
		}

		pr_debug("count :%u irq:%u\n", count, irq);
		if (pos && pos < start) {
			start--;
			for (; start >= pos ; start--)
				*(start + 1) = *start;
		}

		if (pos) {
			pos->irq = irq;
			pos->total_count = count;
			for_each_possible_cpu(cpu)
				pos->irq_counter[cpu] =
					*per_cpu_ptr(desc->kstat_irqs, cpu);
		}
	}
	rcu_read_unlock();

	/* per cpu total irq counts */
	for_each_possible_cpu(cpu)
		wdog_dd->tot_irq_count[cpu] = kstat_cpu_irqs_sum(cpu);

	/* per IPI counts */
	for (index = 0; index < NR_IPI; index++) {
		wdog_dd->ipi_counts[index].total_count = 0;
		wdog_dd->ipi_counts[index].irq = index;
		for_each_possible_cpu(cpu) {
			wdog_dd->ipi_counts[index].irq_counter[cpu] =
				__IRQ_STAT(cpu, ipi_irqs[index]);
			wdog_dd->ipi_counts[index].total_count +=
				wdog_dd->ipi_counts[index].irq_counter[cpu];
		}
	}

	print_irq_stat(wdog_dd);
	atomic_xchg(&wdog_dd->irq_counts_running, 0);
}

static __ref int watchdog_kthread(void *arg)
{
	struct msm_watchdog_data *wdog_dd =
		(struct msm_watchdog_data *)arg;
	unsigned long delay_time = 0;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO-1};
	int ret, cpu;

	sched_setscheduler(current, SCHED_FIFO, &param);
	while (!kthread_should_stop()) {
		do {
			ret = wait_event_interruptible(wdog_dd->pet_complete,
						wdog_dd->timer_expired);
		} while (ret != 0);

		wdog_dd->thread_start = sched_clock();
		for_each_cpu(cpu, cpu_present_mask)
			wdog_dd->ping_start[cpu] = wdog_dd->ping_end[cpu] = 0;

		if (wdog_dd->do_ipi_ping)
			ping_other_cpus(wdog_dd);

		do {
			ret = wait_event_interruptible(wdog_dd->pet_complete,
						wdog_dd->user_pet_complete);
		} while (ret != 0);

		wdog_dd->timer_expired = false;
		wdog_dd->user_pet_complete = !wdog_dd->user_pet_enabled;

		if (enable) {
			delay_time = msecs_to_jiffies(wdog_dd->pet_time);
			pet_watchdog(wdog_dd);
		}
		/* Check again before scheduling
		 * Could have been changed on other cpu
		 */
		mod_timer(&wdog_dd->pet_timer, jiffies + delay_time);
		queue_work(system_unbound_wq, &wdog_dd->irq_counts_work);
	}
	return 0;
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

#ifdef CONFIG_QCOM_INITIAL_LOGBUF
static void log_buf_remove(void)
{
	flush_delayed_work(&log_buf_work);
	dma_free_coherent(wdog_data->dev, *log_buf_size,
			  init_log_buf, log_buf_paddr);
}
#else
static void log_buf_remove(void) { return; }
#endif

static int msm_watchdog_remove(struct platform_device *pdev)
{
	struct msm_watchdog_data *wdog_dd =
			(struct msm_watchdog_data *)platform_get_drvdata(pdev);

	if (!ipi_en)
		cpu_pm_unregister_notifier(&wdog_cpu_pm_nb);

	mutex_lock(&wdog_dd->disable_lock);
	if (enable)
		wdog_disable(wdog_dd);

	mutex_unlock(&wdog_dd->disable_lock);
	device_remove_file(wdog_dd->dev, &dev_attr_disable);
	if (wdog_dd->irq_ppi)
		free_percpu(wdog_dd->wdog_cpu_dd);
	dev_info(wdog_dd->dev, "MSM Watchdog Exit - Deactivated\n");
	del_timer_sync(&wdog_dd->pet_timer);
	kthread_stop(wdog_dd->watchdog_task);
	flush_work(&wdog_dd->irq_counts_work);
	log_buf_remove();
	kfree(wdog_dd);
	return 0;
}

void msm_trigger_wdog_bite(void)
{
	if (!wdog_data)
		return;

	compute_irq_stat(&wdog_data->irq_counts_work);
	pr_info("Causing a watchdog bite!");
	__raw_writel(1, wdog_data->base + WDT0_BITE_TIME);
	/* Mke sure bite time is written before we reset */
	mb();
	__raw_writel(1, wdog_data->base + WDT0_RST);
	/* Make sure we wait only after reset */
	mb();
	/* Delay to make sure bite occurs */
	mdelay(10000);
	pr_err("Wdog - STS: 0x%x, CTL: 0x%x, BARK TIME: 0x%x, BITE TIME: 0x%x",
		__raw_readl(wdog_data->base + WDT0_STS),
		__raw_readl(wdog_data->base + WDT0_EN),
		__raw_readl(wdog_data->base + WDT0_BARK_TIME),
		__raw_readl(wdog_data->base + WDT0_BITE_TIME));
	/*
	 * This function induces the non-secure bite and control
	 * should not return to the calling function. Non-secure
	 * bite interrupt is affined to all the cores and it may
	 * not be handled by the same cores which configured
	 * non-secure bite. So add forever loop here.
	 */
	while (1)
		udelay(1);
}

static irqreturn_t wdog_bark_handler(int irq, void *dev_id)
{
	struct msm_watchdog_data *wdog_dd = (struct msm_watchdog_data *)dev_id;
	unsigned long nanosec_rem;
	unsigned long long t = sched_clock();

	nanosec_rem = do_div(t, 1000000000);
	dev_info(wdog_dd->dev, "Watchdog bark! Now = %lu.%06lu\n",
			(unsigned long) t, nanosec_rem / 1000);

	nanosec_rem = do_div(wdog_dd->last_pet, 1000000000);
	dev_info(wdog_dd->dev, "Watchdog last pet at %lu.%06lu\n",
			(unsigned long) wdog_dd->last_pet, nanosec_rem / 1000);
	show_state_filter(TASK_UNINTERRUPTIBLE);
	if (wdog_dd->do_ipi_ping)
		dump_cpu_alive_mask(wdog_dd);

	msm_trigger_wdog_bite();
	return IRQ_HANDLED;
}

static irqreturn_t wdog_ppi_bark(int irq, void *dev_id)
{
	struct msm_watchdog_data *wdog_dd =
			*(struct msm_watchdog_data **)(dev_id);
	return wdog_bark_handler(irq, wdog_dd);
}

static int init_watchdog_sysfs(struct msm_watchdog_data *wdog_dd)
{
	int error = 0;

	error |= device_create_file(wdog_dd->dev, &dev_attr_disable);

	if (of_property_read_bool(wdog_dd->dev->of_node,
					"qcom,userspace-watchdog")) {
		error |= device_create_file(wdog_dd->dev, &dev_attr_pet_time);
		error |= device_create_file(wdog_dd->dev,
					    &dev_attr_user_pet_enabled);
	}

	if (error)
		dev_err(wdog_dd->dev, "cannot create sysfs attribute\n");

	return error;
}

#ifdef CONFIG_QCOM_INITIAL_LOGBUF
static void minidump_reg_init_log_buf(void)
{
	struct md_region md_entry;

	/* Register init_log_buf info to minidump table */
	strlcpy(md_entry.name, "KBOOT_LOG", sizeof(md_entry.name));
	md_entry.virt_addr = (uintptr_t)init_log_buf;
	md_entry.phys_addr = log_buf_paddr;
	md_entry.size = *log_buf_size;
	md_entry.id = MINIDUMP_DEFAULT_ID;
	if (msm_minidump_add_region(&md_entry) < 0)
		pr_err("Failed to add init_log_buf in Minidump\n");
}

static void log_buf_work_fn(struct work_struct *work)
{
	char **addr = NULL;

	addr = (char **)kallsyms_lookup_name("log_buf");
	if (!addr) {
		dev_err(wdog_data->dev, "log_buf symbol not found\n");
		goto out;
	}

	log_buf_size = (unsigned int *)kallsyms_lookup_name("log_buf_len");
	if (!log_buf_size) {
		dev_err(wdog_data->dev, "log_buf_len symbol not found\n");
		goto out;
	}

	init_log_buf = dma_alloc_coherent(wdog_data->dev, *log_buf_size,
					  &log_buf_paddr, GFP_KERNEL);
	if (!init_log_buf) {
		dev_err(wdog_data->dev, "log_buf dma_alloc_coherent failed\n");
		goto out;
	}

	minidump_reg_init_log_buf();
	memcpy(init_log_buf, *addr, (size_t)(*log_buf_size));
	pr_info("boot log copy done\n");
out:
	return;
}

static void log_buf_init(void)
{
	/* keep granularity of milli seconds */
	unsigned int curr_time_msec = div_u64(sched_clock(), NSEC_PER_MSEC);
	unsigned int timeout_msec = LOGBUF_TIMEOUT - curr_time_msec;

	INIT_DELAYED_WORK(&log_buf_work, log_buf_work_fn);
	queue_delayed_work(system_unbound_wq, &log_buf_work,
			   msecs_to_jiffies(timeout_msec));
}
#else
static void log_buf_init(void)  { return; }
#endif

static void init_watchdog_data(struct msm_watchdog_data *wdog_dd)
{
	unsigned long delay_time;
	uint32_t val;
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
		*raw_cpu_ptr(wdog_dd->wdog_cpu_dd) = wdog_dd;
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

	INIT_WORK(&wdog_dd->irq_counts_work, compute_irq_stat);
	atomic_set(&wdog_dd->irq_counts_running, 0);
	delay_time = msecs_to_jiffies(wdog_dd->pet_time);
	wdog_dd->min_slack_ticks = UINT_MAX;
	wdog_dd->min_slack_ns = ULLONG_MAX;
	timeout = (wdog_dd->bark_time * WDT_HZ)/1000;
	__raw_writel(timeout, wdog_dd->base + WDT0_BARK_TIME);
	__raw_writel(timeout + 10*WDT_HZ, wdog_dd->base + WDT0_BITE_TIME);

	wdog_dd->panic_blk.notifier_call = panic_wdog_handler;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &wdog_dd->panic_blk);
	mutex_init(&wdog_dd->disable_lock);
	init_waitqueue_head(&wdog_dd->pet_complete);
	wdog_dd->timer_expired = false;
	wdog_dd->user_pet_complete = true;
	wdog_dd->user_pet_enabled = false;
	wake_up_process(wdog_dd->watchdog_task);
	timer_setup(&wdog_dd->pet_timer, pet_task_wakeup, 0);
	wdog_dd->pet_timer.expires = jiffies + delay_time;
	add_timer(&wdog_dd->pet_timer);

	val = BIT(EN);
	if (wdog_dd->wakeup_irq_enable)
		val |= BIT(UNMASKED_INT_EN);
	__raw_writel(val, wdog_dd->base + WDT0_EN);
	__raw_writel(1, wdog_dd->base + WDT0_RST);
	wdog_dd->last_pet = sched_clock();
	wdog_dd->enabled = true;

	init_watchdog_sysfs(wdog_dd);

	if (wdog_dd->irq_ppi)
		enable_percpu_irq(wdog_dd->bark_irq, 0);
	if (!ipi_en)
		cpu_pm_register_notifier(&wdog_cpu_pm_nb);
	dev_info(wdog_dd->dev, "MSM Watchdog Initialized\n");
}

static const struct of_device_id msm_wdog_match_table[] = {
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
	struct md_region md_entry;

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
	wdog_dd->watchdog_task = kthread_create(watchdog_kthread, wdog_dd,
			"msm_watchdog");
	if (IS_ERR(wdog_dd->watchdog_task)) {
		ret = PTR_ERR(wdog_dd->watchdog_task);
		goto err;
	}
	init_watchdog_data(wdog_dd);

	log_buf_init();

	/* Add wdog info to minidump table */
	strlcpy(md_entry.name, "KWDOGDATA", sizeof(md_entry.name));
	md_entry.virt_addr = (uintptr_t)wdog_dd;
	md_entry.phys_addr = virt_to_phys(wdog_dd);
	md_entry.size = sizeof(*wdog_dd);
	md_entry.id = MINIDUMP_DEFAULT_ID;
	if (msm_minidump_add_region(&md_entry) < 0)
		pr_info("Failed to add Watchdog data in Minidump\n");

	return 0;
err:
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
