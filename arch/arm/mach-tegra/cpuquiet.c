/*
 * arch/arm/mach-tegra/cpuquiet.c
 *
 * Cpuquiet driver for Tegra CPUs
 *
 * Copyright (c) 2012-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/cpuquiet.h>
#include <linux/pm_qos.h>
#include <linux/debugfs.h>

#include "pm.h"
#include "cpu-tegra.h"
#include "clock.h"

#define INITIAL_STATE		TEGRA_CPQ_DISABLED
#define UP_DELAY_MS		70
#define DOWN_DELAY_MS		2000
#define HOTPLUG_DELAY_MS	100

static struct mutex *tegra_cpu_lock;
static DEFINE_MUTEX(tegra_cpq_lock_stats);

static struct workqueue_struct *cpuquiet_wq;
static struct work_struct cpuquiet_work;
static struct timer_list updown_timer;

static struct kobject *tegra_auto_sysfs_kobject;

static wait_queue_head_t wait_no_lp;
static wait_queue_head_t wait_enable;
static wait_queue_head_t wait_cpu;

/*
 * no_lp can be used to set what cluster cpuquiet uses
 *  - no_lp = 1: only G cluster
 *  - no_lp = 0: dynamically choose between both clusters (cpufreq based)
 *  - no_lp = -1: only LP cluster
 * Settings will be enforced directly upon write to no_lp
 */
static int no_lp;
static bool enable;
static unsigned long up_delay;
static unsigned long down_delay;
static unsigned long hotplug_timeout;
static int mp_overhead = 10;
static unsigned int idle_top_freq;
static unsigned int idle_bottom_freq;

static struct clk *cpu_clk;
static struct clk *cpu_g_clk;
static struct clk *cpu_lp_clk;

static struct cpumask cr_online_requests;
static struct cpumask cr_offline_requests;

enum {
	TEGRA_CPQ_DISABLED = 0,
	TEGRA_CPQ_ENABLED,
	TEGRA_CPQ_IDLE,
};

enum {
	TEGRA_CPQ_G = 0,
	TEGRA_CPQ_LP,
};

static int cpq_target_state;
static int cpq_target_cluster_state;

static int cpq_state;

static struct {
	cputime64_t time_up_total;
	u64 last_update;
	unsigned int up_down_count;
} hp_stats[CONFIG_NR_CPUS + 1];	/* Append LP CPU entry at the end */

static void hp_init_stats(void)
{
	int i;
	u64 cur_jiffies = get_jiffies_64();

	mutex_lock(&tegra_cpq_lock_stats);

	for (i = 0; i <= nr_cpu_ids; i++) {
		hp_stats[i].time_up_total = 0;
		hp_stats[i].last_update = cur_jiffies;

		hp_stats[i].up_down_count = 0;
		if (is_lp_cluster()) {
			if (i == nr_cpu_ids)
				hp_stats[i].up_down_count = 1;
		} else {
			if ((i < nr_cpu_ids) && cpu_online(i))
				hp_stats[i].up_down_count = 1;
		}
	}

	mutex_unlock(&tegra_cpq_lock_stats);
}

/* must be called with tegra_cpq_lock_stats held */
static void __hp_stats_update(unsigned int cpu, bool up)
{
	u64 cur_jiffies = get_jiffies_64();
	bool was_up;

	was_up = hp_stats[cpu].up_down_count & 0x1;

	if (was_up)
		hp_stats[cpu].time_up_total =
			hp_stats[cpu].time_up_total +
			(cur_jiffies - hp_stats[cpu].last_update);

	if (was_up != up) {
		hp_stats[cpu].up_down_count++;
		if ((hp_stats[cpu].up_down_count & 0x1) != up) {
			/* FIXME: sysfs user space CPU control breaks stats */
			pr_err("tegra hotplug stats out of sync with %s CPU%d",
			       (cpu < nr_cpu_ids) ? "G" : "LP",
			       (cpu < nr_cpu_ids) ?  cpu : 0);
			hp_stats[cpu].up_down_count ^=  0x1;
		}
	}
	hp_stats[cpu].last_update = cur_jiffies;
}

static void hp_stats_update(unsigned int cpu, bool up)
{
	mutex_lock(&tegra_cpq_lock_stats);

	__hp_stats_update(cpu, up);

	mutex_unlock(&tegra_cpq_lock_stats);
}

static int update_core_config(unsigned int cpunumber, bool up)
{
	int ret = 0;
	unsigned int nr_cpus = num_online_cpus();

	mutex_lock(tegra_cpu_lock);

	if (cpq_state == TEGRA_CPQ_DISABLED || cpunumber >= nr_cpu_ids) {
		mutex_unlock(tegra_cpu_lock);
		return -EINVAL;
	}


	if (up) {
		cpumask_set_cpu(cpunumber, &cr_online_requests);
		cpumask_clear_cpu(cpunumber, &cr_offline_requests);
		if (is_lp_cluster())
			ret = -EBUSY;
		else if (tegra_cpu_edp_favor_up(nr_cpus, mp_overhead))
			queue_work(cpuquiet_wq, &cpuquiet_work);
	} else {
		if (is_lp_cluster()) {
			ret = -EBUSY;
		} else {
			cpumask_set_cpu(cpunumber, &cr_offline_requests);
			cpumask_clear_cpu(cpunumber, &cr_online_requests);
			queue_work(cpuquiet_wq, &cpuquiet_work);
		}
	}

	mutex_unlock(tegra_cpu_lock);

	return ret;
}

static int tegra_quiesence_cpu(unsigned int cpunumber, bool sync)
{
	int err = 0;

	err = update_core_config(cpunumber, false);
	if (err || !sync)
		return err;

	err = wait_event_interruptible_timeout(wait_cpu,
					       !cpu_online(cpunumber),
					       hotplug_timeout);

	if (err < 0)
		return err;

	if (err > 0)
		return 0;
	else
		return -ETIMEDOUT;
}

static int tegra_wake_cpu(unsigned int cpunumber, bool sync)
{
	int err = 0;

	err = update_core_config(cpunumber, true);
	if (err || !sync)
		return err;

	err = wait_event_interruptible_timeout(wait_cpu, cpu_online(cpunumber),
					       hotplug_timeout);

	if (err < 0)
		return err;

	if (err > 0)
		return 0;
	else
		return -ETIMEDOUT;
}

static struct cpuquiet_driver tegra_cpuquiet_driver = {
        .name                   = "tegra",
        .quiesence_cpu          = tegra_quiesence_cpu,
        .wake_cpu               = tegra_wake_cpu,
};

static void updown_handler(unsigned long data)
{
	queue_work(cpuquiet_wq, &cpuquiet_work);
}

/* must be called from worker function */
static int __apply_cluster_config(int state, int target_state)
{
	int new_state = state;
	unsigned long speed;

	mutex_lock(tegra_cpu_lock);

	if (state == TEGRA_CPQ_LP) {
		if (target_state == TEGRA_CPQ_G && no_lp != -1) {
			/* make sure cpu rate is within g-mode range before
			   switching */
			speed = max((unsigned long)tegra_getspeed(0),
					clk_get_min_rate(cpu_g_clk) / 1000);
			tegra_update_cpu_speed(speed);

			if (!tegra_cluster_switch(cpu_clk, cpu_g_clk)) {
				hp_stats_update(nr_cpu_ids, false);
				hp_stats_update(0, true);
				new_state = TEGRA_CPQ_G;
			}
		}
	} else if (target_state == TEGRA_CPQ_LP && no_lp != 1 &&
			num_online_cpus() == 1) {

		speed = min((unsigned long)tegra_getspeed(0),
			    clk_get_max_rate(cpu_lp_clk) / 1000);
		tegra_update_cpu_speed(speed);

		if (!tegra_cluster_switch(cpu_clk, cpu_lp_clk)) {
			hp_stats_update(nr_cpu_ids, true);
			hp_stats_update(0, false);
			new_state = TEGRA_CPQ_LP;
		}
	}

	wake_up_interruptible(&wait_no_lp);

	mutex_unlock(tegra_cpu_lock);

	return new_state;
}

/* must be called from worker function */
static void __cpuinit __apply_core_config(void)
{
	int count = -1;
	unsigned int cpu;
	int nr_cpus;
	struct cpumask online, offline, cpu_online;
	int max_cpus = pm_qos_request(PM_QOS_MAX_ONLINE_CPUS) ? :
				num_present_cpus();
	int min_cpus = pm_qos_request(PM_QOS_MIN_ONLINE_CPUS);

	mutex_lock(tegra_cpu_lock);

	online = cr_online_requests;
	offline = cr_offline_requests;

	mutex_unlock(tegra_cpu_lock);

	/* always keep CPU0 online */
	cpumask_set_cpu(0, &online);
	cpu_online = *cpu_online_mask;

	if (no_lp == -1) {
		max_cpus = 1;
		min_cpus = 0;
	} else if (max_cpus < min_cpus)
		max_cpus = min_cpus;

	nr_cpus = cpumask_weight(&online);
	if (nr_cpus < min_cpus) {
		cpu = 0;
		count = min_cpus - nr_cpus;
		for (; count > 0; count--) {
			cpu = cpumask_next_zero(cpu, &online);
			cpumask_set_cpu(cpu, &online);
			cpumask_clear_cpu(cpu, &offline);
		}
	} else if (nr_cpus > max_cpus) {
		count = nr_cpus - max_cpus;
		cpu = 0;
		for (; count > 0; count--) {
			/* CPU0 should always be online */
			cpu = cpumask_next(cpu, &online);
			cpumask_set_cpu(cpu, &offline);
			cpumask_clear_cpu(cpu, &online);
		}
	}

	cpumask_andnot(&online, &online, &cpu_online);
	for_each_cpu(cpu, &online) {
		cpu_up(cpu);
		hp_stats_update(cpu, true);
	}

	cpumask_and(&offline, &offline, &cpu_online);
	for_each_cpu(cpu, &offline) {
		cpu_down(cpu);
		hp_stats_update(cpu, false);
	}
	wake_up_interruptible(&wait_cpu);
}

static void __cpuinit tegra_cpuquiet_work_func(struct work_struct *work)
{
	int new_cluster, current_cluster, action;

	mutex_lock(tegra_cpu_lock);

	current_cluster = is_lp_cluster();
	action = cpq_target_state;
	new_cluster = cpq_target_cluster_state;

	if (action == TEGRA_CPQ_ENABLED) {
		hp_init_stats();
		cpuquiet_device_free();
		pr_info("Tegra cpuquiet clusterswitch enabled\n");
		cpq_state = TEGRA_CPQ_ENABLED;
		cpq_target_state = TEGRA_CPQ_IDLE;
		cpq_target_cluster_state = is_lp_cluster();
		wake_up_interruptible(&wait_enable);
	}

	if (cpq_state == TEGRA_CPQ_DISABLED) {
		mutex_unlock(tegra_cpu_lock);
		return;
	}

	if (action == TEGRA_CPQ_DISABLED) {
		cpq_state = TEGRA_CPQ_DISABLED;
		mutex_unlock(tegra_cpu_lock);
		cpuquiet_device_busy();
		pr_info("Tegra cpuquiet clusterswitch disabled\n");
		wake_up_interruptible(&wait_enable);
		return;
	}

	mutex_unlock(tegra_cpu_lock);
	if (current_cluster == TEGRA_CPQ_G)
		__apply_core_config();

	if (current_cluster != new_cluster) {
		current_cluster = __apply_cluster_config(current_cluster,
					new_cluster);

		tegra_cpu_set_speed_cap(NULL);

		if (current_cluster == TEGRA_CPQ_LP)
			cpuquiet_device_busy();
		else
			cpuquiet_device_free();

	}

}

static int min_cpus_notify(struct notifier_block *nb, unsigned long n, void *p)
{
	mutex_lock(tegra_cpu_lock);

	if (cpq_state == TEGRA_CPQ_DISABLED) {
		mutex_unlock(tegra_cpu_lock);
		return NOTIFY_OK;
	}

	if (n > 1)
		cpq_target_cluster_state = TEGRA_CPQ_G;

	queue_work(cpuquiet_wq, &cpuquiet_work);

	mutex_unlock(tegra_cpu_lock);

	return NOTIFY_OK;
}

static int max_cpus_notify(struct notifier_block *nb, unsigned long n, void *p)
{
	mutex_lock(tegra_cpu_lock);

	if (cpq_state != TEGRA_CPQ_DISABLED)
		queue_work(cpuquiet_wq, &cpuquiet_work);

	mutex_unlock(tegra_cpu_lock);

	return NOTIFY_OK;
}

static int __cpuinit cpu_online_notify(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_POST_DEAD:
		if (no_lp != 1 &&
		    num_online_cpus() == 1 &&
		    tegra_getspeed(0) <= idle_bottom_freq) {
			mutex_lock(tegra_cpu_lock);

			cpq_target_cluster_state = TEGRA_CPQ_LP;

			/* Explicit LP cluster request: force switch now. */
			if (no_lp == -1)
				queue_work(cpuquiet_wq, &cpuquiet_work);
			else
				mod_timer(&updown_timer, jiffies + down_delay);

			mutex_unlock(tegra_cpu_lock);
		}
		break;
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		if (cpq_target_cluster_state == TEGRA_CPQ_LP) {
			mutex_lock(tegra_cpu_lock);

			if (cpq_target_cluster_state == TEGRA_CPQ_LP) {
				cpq_target_cluster_state = TEGRA_CPQ_G;
				del_timer(&updown_timer);
			}

			mutex_unlock(tegra_cpu_lock);
		}
		break;
	}

	return NOTIFY_OK;
}

/* must be called with tegra_cpu_lock held */
void tegra_auto_hotplug_governor(unsigned int cpu_freq, bool suspend)
{
	if (!is_g_cluster_present() || no_lp)
		return;

	if (cpq_state == TEGRA_CPQ_DISABLED)
		return;

	if (suspend) {
		/* Switch to fast cluster if suspend rate is high enough */
		if (cpu_freq >= idle_bottom_freq) {

			/* Force switch now */
			cpq_target_cluster_state = TEGRA_CPQ_G;
			queue_work(cpuquiet_wq, &cpuquiet_work);
		}
		return;
	}

	/*
	 * If there is more then 1 CPU online, we must be on the fast cluster
	 * and we can't switch.
	 */
	if (num_online_cpus() > 1)
		return;

	if (is_lp_cluster()) {
		if (cpu_freq >= idle_top_freq &&
			cpq_target_cluster_state != TEGRA_CPQ_G) {

			/* Switch to fast cluster after up_delay */
			cpq_target_cluster_state = TEGRA_CPQ_G;
			mod_timer(&updown_timer, jiffies + up_delay);
		} else if (cpu_freq < idle_top_freq &&
				cpq_target_cluster_state == TEGRA_CPQ_G) {

			/*
			 * CPU frequency dropped below idle_top_freq while
			 * waiting for up_delay, Cancel switch request.
			 */
			cpq_target_cluster_state = TEGRA_CPQ_LP;
			del_timer(&updown_timer);
		}
	} else {
		if (cpu_freq <= idle_bottom_freq &&
			cpq_target_cluster_state != TEGRA_CPQ_LP) {

			/* Switch to slow cluster after down_delay */
			cpq_target_cluster_state = TEGRA_CPQ_LP;
			mod_timer(&updown_timer, jiffies + down_delay);
		} else if (cpu_freq > idle_bottom_freq &&
			cpq_target_cluster_state == TEGRA_CPQ_LP) {

			/*
			 * CPU frequency raised again above idle_bottom_freq.
			 * Stay on the fast cluster.
			 */
			cpq_target_cluster_state = TEGRA_CPQ_G;
			del_timer(&updown_timer);
		}
	}
}

static struct notifier_block min_cpus_notifier = {
	.notifier_call = min_cpus_notify,
};

static struct notifier_block max_cpus_notifier = {
	.notifier_call = max_cpus_notify,
};

static struct notifier_block __cpuinitdata cpu_online_notifier = {
	.notifier_call = cpu_online_notify,
};

static void delay_callback(struct cpuquiet_attribute *attr)
{
	unsigned long val;

	if (attr) {
		val = (*((unsigned long *)(attr->param)));
		(*((unsigned long *)(attr->param))) = msecs_to_jiffies(val);
	}
}

static void enable_callback(struct cpuquiet_attribute *attr)
{
	int target_state = enable ? TEGRA_CPQ_ENABLED : TEGRA_CPQ_DISABLED;

	mutex_lock(tegra_cpu_lock);

	if (cpq_state != target_state) {
		cpq_target_state = target_state;
		queue_work(cpuquiet_wq, &cpuquiet_work);
	}

	mutex_unlock(tegra_cpu_lock);

	wait_event_interruptible(wait_enable, cpq_state == target_state);
}

ssize_t store_no_lp(struct cpuquiet_attribute *attr,
		const char *buf,
		size_t count)
{
	int lp_req;
	int rv;

	rv = store_int_attribute(attr, buf, count);
	if (rv < 0)
		return rv;

	mutex_lock(tegra_cpu_lock);

	/* Force switch if necessary. */
	if (no_lp == 1 && is_lp_cluster()) {
		cpq_target_cluster_state = TEGRA_CPQ_G;
		queue_work(cpuquiet_wq, &cpuquiet_work);
		lp_req = 0;
	} else if (no_lp == -1 && !is_lp_cluster()) {
		cpq_target_cluster_state = TEGRA_CPQ_LP;
		queue_work(cpuquiet_wq, &cpuquiet_work);
		lp_req = 1;
	} else {
		lp_req = is_lp_cluster();
		/* Reevaluate speed to see if we are on the wrong cluster */
		tegra_cpu_set_speed_cap_locked(NULL);
	}

	mutex_unlock(tegra_cpu_lock);

	wait_event_interruptible_timeout(
		wait_no_lp,
		no_lp == 0 || is_lp_cluster() == lp_req,
		hotplug_timeout);

	if (no_lp == 0 || is_lp_cluster() == lp_req)
		return rv;
	else
		return -ETIMEDOUT;
}

CPQ_BASIC_ATTRIBUTE(idle_top_freq, 0644, uint);
CPQ_BASIC_ATTRIBUTE(idle_bottom_freq, 0644, uint);
CPQ_BASIC_ATTRIBUTE(mp_overhead, 0644, int);
CPQ_ATTRIBUTE_CUSTOM(no_lp, 0644, show_int_attribute, store_no_lp);
CPQ_ATTRIBUTE(up_delay, 0644, ulong, delay_callback);
CPQ_ATTRIBUTE(down_delay, 0644, ulong, delay_callback);
CPQ_ATTRIBUTE(hotplug_timeout, 0644, ulong, delay_callback);
CPQ_ATTRIBUTE(enable, 0644, bool, enable_callback);

static struct attribute *tegra_auto_attributes[] = {
	&no_lp_attr.attr,
	&up_delay_attr.attr,
	&down_delay_attr.attr,
	&idle_top_freq_attr.attr,
	&idle_bottom_freq_attr.attr,
	&mp_overhead_attr.attr,
	&enable_attr.attr,
	&hotplug_timeout_attr.attr,
	NULL,
};

static const struct sysfs_ops tegra_auto_sysfs_ops = {
	.show = cpuquiet_auto_sysfs_show,
	.store = cpuquiet_auto_sysfs_store,
};

static struct kobj_type ktype_sysfs = {
	.sysfs_ops = &tegra_auto_sysfs_ops,
	.default_attrs = tegra_auto_attributes,
};

static int tegra_auto_sysfs(void)
{
	int err;

	tegra_auto_sysfs_kobject = kzalloc(sizeof(*tegra_auto_sysfs_kobject),
					GFP_KERNEL);

	if (!tegra_auto_sysfs_kobject)
		return -ENOMEM;

	err = cpuquiet_kobject_init(tegra_auto_sysfs_kobject, &ktype_sysfs,
				"tegra_cpuquiet");

	if (err)
		kfree(tegra_auto_sysfs_kobject);

	return err;
}

#ifdef CONFIG_DEBUG_FS

static struct dentry *hp_debugfs_root;

static int hp_stats_show(struct seq_file *s, void *data)
{
	int i;
	u64 cur_jiffies = get_jiffies_64();

	mutex_lock(tegra_cpu_lock);

	mutex_lock(&tegra_cpq_lock_stats);

	if (cpq_state != TEGRA_CPQ_DISABLED) {
		for (i = 0; i <= nr_cpu_ids; i++) {
			bool was_up = (hp_stats[i].up_down_count & 0x1);
			__hp_stats_update(i, was_up);
		}
	}
	mutex_unlock(&tegra_cpq_lock_stats);

	mutex_unlock(tegra_cpu_lock);

	seq_printf(s, "%-15s ", "cpu:");
	for (i = 0; i < nr_cpu_ids; i++)
		seq_printf(s, "G%-9d ", i);
	seq_printf(s, "LP\n");

	seq_printf(s, "%-15s ", "transitions:");
	for (i = 0; i <= nr_cpu_ids; i++)
		seq_printf(s, "%-10u ", hp_stats[i].up_down_count);
	seq_printf(s, "\n");

	seq_printf(s, "%-15s ", "time plugged:");
	for (i = 0; i <= nr_cpu_ids; i++) {
		seq_printf(s, "%-10llu ",
			   cputime64_to_clock_t(hp_stats[i].time_up_total));
	}
	seq_printf(s, "\n");

	seq_printf(s, "%-15s %llu\n", "time-stamp:",
		   cputime64_to_clock_t(cur_jiffies));

	return 0;
}


static int hp_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, hp_stats_show, inode->i_private);
}


static const struct file_operations hp_stats_fops = {
	.open		= hp_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int __init tegra_cpuquiet_debug_init(void)
{

	hp_debugfs_root = debugfs_create_dir("tegra_hotplug", NULL);
	if (!hp_debugfs_root)
		return -ENOMEM;

	if (!debugfs_create_file(
		"stats", S_IRUGO, hp_debugfs_root, NULL, &hp_stats_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(hp_debugfs_root);
	return -ENOMEM;
}

late_initcall(tegra_cpuquiet_debug_init);

#endif /* CONFIG_DEBUG_FS */


int __cpuinit tegra_auto_hotplug_init(struct mutex *cpulock)
{
	int err;

	cpu_clk = clk_get_sys(NULL, "cpu");
	cpu_g_clk = clk_get_sys(NULL, "cpu_g");
	cpu_lp_clk = clk_get_sys(NULL, "cpu_lp");

	if (IS_ERR(cpu_clk) || IS_ERR(cpu_g_clk) || IS_ERR(cpu_lp_clk))
		return -ENOENT;

	tegra_cpu_lock = cpulock;

	init_waitqueue_head(&wait_no_lp);
	init_waitqueue_head(&wait_enable);
	init_waitqueue_head(&wait_cpu);

	/*
	 * Not bound to the issuer CPU (=> high-priority), has rescue worker
	 * task, single-threaded, freezable.
	 */
	cpuquiet_wq = alloc_workqueue(
		"cpuquiet", WQ_NON_REENTRANT | WQ_RESCUER | WQ_FREEZABLE, 1);

	if (!cpuquiet_wq)
		return -ENOMEM;

	INIT_WORK(&cpuquiet_work, tegra_cpuquiet_work_func);
	init_timer(&updown_timer);
	updown_timer.function = updown_handler;

	idle_top_freq = clk_get_max_rate(cpu_lp_clk) / 1000;
	idle_bottom_freq = clk_get_min_rate(cpu_g_clk) / 1000;

	up_delay = msecs_to_jiffies(UP_DELAY_MS);
	down_delay = msecs_to_jiffies(DOWN_DELAY_MS);
	hotplug_timeout = msecs_to_jiffies(HOTPLUG_DELAY_MS);
	cpumask_clear(&cr_online_requests);
	cpumask_clear(&cr_offline_requests);

	cpq_target_cluster_state = is_lp_cluster();
	cpq_state = INITIAL_STATE;
	enable = cpq_state == TEGRA_CPQ_DISABLED ? false : true;
	hp_init_stats();

	pr_info("Tegra cpuquiet initialized: %s\n",
		(cpq_state == TEGRA_CPQ_DISABLED) ? "disabled" : "enabled");

	if (pm_qos_add_notifier(PM_QOS_MIN_ONLINE_CPUS, &min_cpus_notifier))
		pr_err("%s: Failed to register min cpus PM QoS notifier\n",
			__func__);
	if (pm_qos_add_notifier(PM_QOS_MAX_ONLINE_CPUS, &max_cpus_notifier))
		pr_err("%s: Failed to register max cpus PM QoS notifier\n",
			__func__);

	register_hotcpu_notifier(&cpu_online_notifier);

	err = cpuquiet_register_driver(&tegra_cpuquiet_driver);
	if (err) {
		destroy_workqueue(cpuquiet_wq);
		return err;
	}

	err = tegra_auto_sysfs();
	if (err) {
		cpuquiet_unregister_driver(&tegra_cpuquiet_driver);
		destroy_workqueue(cpuquiet_wq);
	}

	return err;
}

void tegra_auto_hotplug_exit(void)
{
	destroy_workqueue(cpuquiet_wq);
        cpuquiet_unregister_driver(&tegra_cpuquiet_driver);
	kobject_put(tegra_auto_sysfs_kobject);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(hp_debugfs_root);
#endif
}
