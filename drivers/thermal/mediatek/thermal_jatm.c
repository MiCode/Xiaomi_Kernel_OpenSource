// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include "thermal_interface.h"
#include "thermal_jatm.h"
#include "thermal_trace.h"


static LIST_HEAD(jatm_policy_list);

struct jatm_info {
	bool turn_on;
	bool activated;
	int max_budget;
	int budget;
	int interval;
	int stop_deadline;
	int mode;
	bool fix_opp;
	bool delay_start;
	struct timespec64 last_jatm_enable;
	struct timespec64 last_frame_start;
};

static struct jatm_info j_info;

DEFINE_MUTEX(jatm_mutex);

static void jatm_stop_work_fn(struct work_struct *work);
static void jatm_start_work_fn(struct work_struct *work);
static DECLARE_DELAYED_WORK(jatm_stop_work, jatm_stop_work_fn);
static DECLARE_DELAYED_WORK(jatm_start_work, jatm_start_work_fn);

LIST_HEAD(jatm_record_list);

static void jatm_set_deadline(void)
{
	schedule_delayed_work(&jatm_stop_work,
		msecs_to_jiffies(j_info.stop_deadline));
}

static unsigned long calculate_timeval_diff(struct timespec64 *start, struct timespec64 *end)
{
	unsigned long ms;

	ms = (end->tv_sec - start->tv_sec) * 1000;
	ms += ((end->tv_nsec - start->tv_nsec) / 1000000);
	return ms;
}

static bool jatm_is_enabled(void)
{
	return j_info.activated;
}

static void cpu_force_max_freq(int enable)
{
	struct jatm_policy *j_policy;
	s32 freq_limit;

	if (j_info.fix_opp == false)
		return;

	if (enable == 1)
		freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;
	else
		freq_limit = FREQ_QOS_MIN_DEFAULT_VALUE;

	list_for_each_entry(j_policy, &jatm_policy_list, jatm_list)
		freq_qos_update_request(&j_policy->qos_req, freq_limit);
}

static void schedule_jatm_work(void)
{
	schedule_delayed_work(&jatm_stop_work, msecs_to_jiffies(j_info.budget));
}


static void enable_jatm(void)
{
	j_info.activated = true;
	/* 1. */
	set_ttj(JATM_ON);
	/* 2. */
	cpu_force_max_freq(1);
	/* 3. */

	pr_info("Enabled and remaining budget = %d\n", j_info.budget);
	trace_jatm_enable(j_info.budget);

	if (j_info.mode == BUDGET)
		schedule_jatm_work();
	else if (j_info.mode == STOP_DEADLINE)
		jatm_set_deadline();
	ktime_get_real_ts64(&(j_info.last_jatm_enable));
}

static void remove_all_records(void)
{
	struct jatm_record *record, *tmp;

	list_for_each_entry_safe(record, tmp, &jatm_record_list, list) {
		list_del(&record->list);
		kfree(record);
	}
}

static void recycle_jatm_budget(void)
{
	struct jatm_record *record, *tmp;
	struct timespec64 now_tv;
	unsigned long ms;

	if (likely(list_empty(&jatm_record_list)))
		return;

	ktime_get_real_ts64(&now_tv);
	record = list_last_entry(&jatm_record_list, struct jatm_record, list);
	ms = calculate_timeval_diff(&record->end_tv, &now_tv);

	/* elapsed more than jatm_interval since end time of last trigger,
	 * jatm budget is fully recovered, recycle all entries
	 */
	pr_debug("%s jatm_budget=%d elapsed=%d jatm_interval=%d\n", __func__,
		j_info.budget, ms, j_info.interval);
	if (ms >= j_info.interval) {
		pr_debug("Budget is fully recoveried\n");
		remove_all_records();
		j_info.budget = j_info.max_budget;
	} else {
		/* only recovery record ends more than jatm_interval */
		list_for_each_entry_safe(record, tmp, &jatm_record_list, list) {
			ms = calculate_timeval_diff(&record->end_tv, &now_tv);
			if (ms >= j_info.interval) {
				pr_debug("recycle jatm_budget=%d\n", record->usage);
				j_info.budget += record->usage;
				list_del(&record->list);
				kfree(record);
			} else {
				/* sort after current entry must newer than
				 * current entry, no need to traverse them
				 */
				break;
			}
		}
		if (j_info.budget > j_info.max_budget) {
			pr_notice("JATM budget(%d) > max_budget(%d)\n",
				j_info.budget, j_info.max_budget);
			j_info.budget = j_info.max_budget;
		}
	}
}

void jatm_start_work_fn(struct work_struct *work)
{
	struct timespec64 now_tv;
	int target_tj;
	enum jatm_not_start_reason reason;

	reason = ENABLE;

	ktime_get_real_ts64(&now_tv);

	mutex_lock(&jatm_mutex);
	if (unlikely(jatm_is_enabled())) {
		/*In case reentant happens, don't reset jatm timer, since CPU*/
		/*is already being heat up because of opp0*/
		pr_notice("JATM cannot enable: already enabled\n");
		reason = ALREADY_ENABLED;
		goto done_unlock;
	}
	if (j_info.mode == STOP_DEADLINE) {
		target_tj = get_catm_ttj();
		if (unlikely(target_tj <= get_catm_min_ttj())) {
			pr_notice("JATM cannot enable: min TTJ\n");
			reason = MIN_TTJ;
			goto done_unlock;
		}

		if (get_jatm_suspend()) {
			pr_notice("JATM cannot enable: being suspended\n");
			reason = SUSPENDED;
			goto done_unlock;
		}
	}
	/*recycle_jatm_budget just before enabling instead of*/
	/*try_enable_jatm() to have maximum budget*/
	recycle_jatm_budget();


	if ((j_info.mode == STOP_DEADLINE) || (j_info.budget > 0))
		enable_jatm();
	else {
		pr_notice("JATM cannot enable: no budget\n");
		reason = NO_BUDGET;
	}


done_unlock:
	mutex_unlock(&jatm_mutex);
	trace_not_start_reason(reason);
}


static void try_enable_jatm(void)
{
	int delay, elapsed;
	struct timespec64 now_tv;

	mutex_lock(&jatm_mutex);
	ktime_get_real_ts64(&now_tv);
	/* delay based on jatm usage in the past one period */
	if (j_info.delay_start) {
		/* recycle here to get accurate jatm usage */
		recycle_jatm_budget();
		if (j_info.budget < 0)
			delay = j_info.interval;
		else
			delay = j_info.max_budget - j_info.budget;
	} else { /* delay based on jatm usage in the past one period */
		delay = 0;
	}
	elapsed = calculate_timeval_diff(&j_info.last_frame_start, &now_tv);
	pr_notice("Try to enable JATM after %d ms and delay start %d ms\n", elapsed, delay);
	trace_try_enable_jatm(elapsed, delay);

	if (delay > 0) {
		if (delayed_work_pending(&jatm_start_work))
			mod_delayed_work(system_wq, &jatm_start_work,
				msecs_to_jiffies(delay));
		else
			schedule_delayed_work(&jatm_start_work,
				msecs_to_jiffies(delay));

		mutex_unlock(&jatm_mutex);
	} else {
		if (delayed_work_pending(&jatm_start_work))
			cancel_delayed_work(&jatm_start_work);
		/* jatm_start_work_fn will hold the lock, release here */
		mutex_unlock(&jatm_mutex);
		jatm_start_work_fn(NULL);
	}

}


static void record_jatm_usage(struct timespec64 now, int elapsed_ms)
{
	struct jatm_record *record = kmalloc(sizeof(struct jatm_record), GFP_KERNEL);

	if (record == NULL)
		return;
	record->usage = elapsed_ms;
	record->end_tv = now;
	list_add_tail(&(record->list), &jatm_record_list);
}

static void disable_jatm(enum jatm_stop_reason reason)
{
	struct timespec64 now_tv;
	unsigned long jatm_usage, real_usage;
	unsigned long frame_length;

	mutex_lock(&jatm_mutex);
	ktime_get_real_ts64(&now_tv);
	frame_length = calculate_timeval_diff(&j_info.last_frame_start, &now_tv);
	/* the hook point in the fpsgo now is almost the start of the next frame */
	j_info.last_frame_start = now_tv;

	cancel_delayed_work(&jatm_start_work);
	/* cancel max freq if we are during JATM, false means JATM not enabled */
	if (unlikely(!jatm_is_enabled())) {
		mutex_unlock(&jatm_mutex);
		return;
	}

	j_info.activated = false;
	write_jatm_suspend(1);

	real_usage = calculate_timeval_diff(&j_info.last_jatm_enable, &now_tv);

	if (real_usage < 5) {
		pr_debug("JATM usage %d < min usage 5\n", real_usage);
		jatm_usage = 5;
	} else
		jatm_usage = real_usage;


	if (j_info.budget < jatm_usage)
		j_info.budget = 0;
	else
		j_info.budget -= jatm_usage;
	record_jatm_usage(now_tv, jatm_usage);
	cancel_delayed_work(&jatm_stop_work);
	set_ttj(JATM_OFF);

	cpu_force_max_freq(0);
	mutex_unlock(&jatm_mutex);
	pr_info("stop reason=%s, frame_length=%lu, real_usage=%lu, remaining=%d\n",
		jatm_stop_reason_string[reason], frame_length, real_usage, j_info.budget);
	trace_jatm_disable(reason, frame_length, real_usage, j_info.budget);
}

static void jatm_stop_work_fn(struct work_struct *work)
{
	disable_jatm(BUDGET_RUNNING_OUT);
}

void jatm_notify_fp_cb(int enable)
{

	if (j_info.turn_on != true)
		return;

	if (enable == 1)
		try_enable_jatm();
	else
		disable_jatm(FRAME_COMPLETE);
}

static ssize_t jatm_info_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d, %d, %d, %d, %d, %d, %d %d\n",
		j_info.max_budget,
		j_info.budget,
		j_info.interval,
		j_info.fix_opp,
		jatm_is_enabled(),
		j_info.stop_deadline,
		j_info.delay_start,
		j_info.mode
	);

	return len;
}

static ssize_t jatm_info_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[10];
	int max_budget, budget, interval, fix_opp;
	int jatm_stop_deadline, jatm_delay_start;
	int mode;

	if (sscanf(buf, "%5s %u %u %u %u %u %u %u", cmd,
			&max_budget, &budget, &interval, &fix_opp,
			&jatm_stop_deadline, &jatm_delay_start, &mode) == 8) {

		if (strncmp(cmd, "JATM", 4) != 0) {
			pr_info("[jatm_info] invalid input\n");
			return -EINVAL;
		}
		if ((budget < 0) || (budget > 50000)) {
			pr_info("[jatm_info] budget should be 0 ~ 50000\n");
			return -EINVAL;
		}
		if ((max_budget < 0) || (max_budget > 50000)) {
			pr_info("[jatm_info] max_budget should be 0 ~ 50000\n");
			return -EINVAL;
		}
		if (max_budget < budget) {
			pr_info("[jatm_info] max_budget should be >= budget\n");
			return -EINVAL;
		}
		if ((interval < 0) || (interval > 10000))  {
			pr_info("[jatm_info] interval should be 0 ~ 10000\n");
			return -EINVAL;
		}

		if ((jatm_stop_deadline < 0) || (jatm_stop_deadline > 500))  {
			pr_info("[jatm_info] jatm_stop_deadline should be 0 ~ 500\n");
			return -EINVAL;
		}
		j_info.max_budget = max_budget;
		j_info.budget = budget;
		j_info.interval = interval;
		if (fix_opp == 1)
			j_info.fix_opp = true;
		else
			j_info.fix_opp = false;
		j_info.stop_deadline = jatm_stop_deadline;
		if (jatm_delay_start == 1)
			j_info.delay_start = true;
		else
			j_info.delay_start = false;

		j_info.mode = mode;
		return count;

	}

	pr_info("[jatm_info] invalid input\n");

	return -EINVAL;
}

static ssize_t turn_on_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
		j_info.turn_on);

	return len;
}

static ssize_t turn_on_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[10];
	int activate;
	int turn_on;

	if (sscanf(buf, "%8s %d", cmd, &activate) == 2) {
		if (strncmp(cmd, "JATM_UT", 7) == 0) {
			if (activate == 1)
				try_enable_jatm();
			else if (activate == 0)
				disable_jatm(FRAME_COMPLETE);
			return count;
		}
	}
	if (kstrtoint(buf, 10, &turn_on) == 0) {
		if (turn_on == 1)
			j_info.turn_on = true;
		else if (turn_on == 0)
			j_info.turn_on = false;

		pr_info("[jatm_enable] turn_on %d\n", j_info.turn_on);
		return count;
	}

	pr_info("[jatm_enable] invalid input\n");

	return -EINVAL;
}

static ssize_t max_budget_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int max_budget;

	if (kstrtoint(buf, 10, &max_budget) == 0) {
		if (max_budget >= 0 && max_budget <= 50000) {
			j_info.max_budget = max_budget;
			j_info.budget = max_budget;
		} else {
			pr_info("[max_budget] should be 0 ~ 50000\n");
		}
		return count;
	}

	pr_info("[max_budget] invalid input\n");

	return -EINVAL;
}

static ssize_t interval_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int interval;

	if (kstrtoint(buf, 10, &interval) == 0) {
		if (interval >= 0 && interval <= 10000)
			j_info.interval = interval;
		else
			pr_info("[interval] should be 0 ~ 10000\n");
		return count;
	}

	pr_info("[interval] invalid input\n");

	return -EINVAL;
}

static ssize_t mode_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int mode;

	if (kstrtoint(buf, 10, &mode) == 0) {
		if (mode >= 0 && mode <= 1)
			j_info.mode = mode;
		else
			pr_info("[mode] should be 0 ~ 1\n");
		return count;
	}

	pr_info("[mode] invalid input\n");

	return -EINVAL;
}


static struct kobj_attribute jatm_info_attr = __ATTR_RW(jatm_info);
static struct kobj_attribute turn_on_attr = __ATTR_RW(turn_on);
static struct kobj_attribute max_budget_attr = __ATTR_WO(max_budget);
static struct kobj_attribute interval_attr = __ATTR_WO(interval);
static struct kobj_attribute mode_attr = __ATTR_WO(mode);


static struct attribute *jatm_attrs[] = {
	&jatm_info_attr.attr,
	&turn_on_attr.attr,
	&max_budget_attr.attr,
	&interval_attr.attr,
	&mode_attr.attr,
	NULL
};
static struct attribute_group jatm_attr_group = {
	.name	= "jatm",
	.attrs	= jatm_attrs,
};

static const struct of_device_id therm_jatm_of_match[] = {
	{ .compatible = "mediatek,therm_jatm", },
	{},
};
MODULE_DEVICE_TABLE(of, therm_jatm_of_match);

static int therm_jatm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cpufreq_policy *policy;
	struct jatm_policy *j_policy;
	int cpu, ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;

		if (policy->cpu == cpu) {
			j_policy = devm_kzalloc(dev, sizeof(*j_policy), GFP_KERNEL);
			if (!j_policy)
				return -ENOMEM;

			j_policy->policy = policy;
			j_policy->cpu = cpu;

			ret = freq_qos_add_request(&policy->constraints,
				&j_policy->qos_req, FREQ_QOS_MIN,
				FREQ_QOS_MIN_DEFAULT_VALUE);

			if (ret < 0) {
				dev_err(&pdev->dev, "%s: Fail to add freq constraint (%d)\n",
					__func__, ret);
				return ret;
			}
			list_add_tail(&j_policy->jatm_list, &jatm_policy_list);

		}
	}

	ret = sysfs_create_group(kernel_kobj, &jatm_attr_group);
	if (ret) {
		dev_info(&pdev->dev, "failed to create thermal sysfs, ret=%d!\n", ret);
		return ret;
	}

	j_info.max_budget = DEFAULT_JATM_INFINITE_BUDGET;
	j_info.budget = DEFAULT_JATM_INFINITE_BUDGET;
	j_info.interval = DEFAULT_JATM_INTERVAL;
	j_info.activated = false;
	j_info.fix_opp = false;
	/* jatm stop time out */
	j_info.stop_deadline = DEFAULT_JATM_STOP_DEADLINE;

	/* jatm_delay_start*/
	/* true  : delay jatm enable after certain delay (allocate for heavier frame)*/
	/* false : enable just after  */
	j_info.delay_start = false;

	j_info.mode = STOP_DEADLINE;

	jatm_notify_fp = jatm_notify_fp_cb;

	return 0;
}

static int therm_jatm_remove(struct platform_device *pdev)
{
	sysfs_remove_group(kernel_kobj, &jatm_attr_group);

	return 0;
}

static struct platform_driver therm_jatm_driver = {
	.probe = therm_jatm_probe,
	.remove = therm_jatm_remove,
	.driver = {
		.name = "mtk-thermal-jatm",
		.of_match_table = therm_jatm_of_match,
	},
};

module_platform_driver(therm_jatm_driver);

MODULE_AUTHOR("Henry Huang <henry.huang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek jank aware thermal management driver");
MODULE_LICENSE("GPL v2");
