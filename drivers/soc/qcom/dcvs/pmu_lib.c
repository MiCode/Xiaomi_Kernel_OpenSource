// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "qcom-pmu: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/mutex.h>
#include <linux/cpu.h>
#include <linux/spinlock.h>
#include <linux/perf_event.h>
#include <linux/cpuidle.h>
#include <trace/events/power.h>
#include <trace/hooks/cpuidle.h>
#include <soc/qcom/pmu_lib.h>
#include <soc/qcom/qcom_llcc_pmu.h>

#define MAX_PMU_EVS	QCOM_PMU_MAX_EVS

struct event_data {
	u32			event_id;
	struct perf_event	*pevent;
	int			cpu;
	u64			cached_count;
	u32			ref_cnt;
	atomic_t		read_cnt;
	bool			active;
	spinlock_t		lock;
};

struct cpu_data {
	bool			is_idle;
	bool			is_hp;
	struct event_data	events[MAX_PMU_EVS];
	u32			num_evs;
	struct mutex		events_lock;
};

static DEFINE_PER_CPU(struct cpu_data *, cpu_ev_data);
static bool qcom_pmu_inited;
static LIST_HEAD(idle_notif_list);
static DEFINE_SPINLOCK(idle_list_lock);

static struct perf_event_attr *alloc_attr(void)
{
	struct perf_event_attr *attr;

	attr = kzalloc(sizeof(struct perf_event_attr), GFP_KERNEL);
	if (!attr)
		return attr;

	attr->size = sizeof(struct perf_event_attr);
	attr->pinned = 1;

	return attr;
}

static int set_event(struct event_data *ev, int cpu,
			     struct perf_event_attr *attr)
{
	struct perf_event *pevent;
	u32 type = PERF_TYPE_RAW;
	int ret;

	if (!ev->event_id)
		return 0;

	attr->config = ev->event_id;
	if (ev->event_id == QCOM_LLCC_PMU_RD_EV) {
		ret = qcom_llcc_pmu_hw_type(&type);
		if (ret < 0)
			return ret;
	}
	attr->type = type;
	pevent = perf_event_create_kernel_counter(attr, cpu, NULL, NULL, NULL);
	if (IS_ERR(pevent))
		return PTR_ERR(pevent);

	perf_event_enable(pevent);
	ev->pevent = pevent;
	ev->cpu = cpu;
	ev->active = true;

	return 0;
}

static inline void delete_event(struct event_data *event)
{
	spin_lock(&event->lock);
	event->active = false;
	spin_unlock(&event->lock);
	while (atomic_read(&event->read_cnt) > 0)
		udelay(10);
	perf_event_release_kernel(event->pevent);
	event->pevent = NULL;
}

static inline u64 read_event(struct event_data *event, bool local, bool force)
{
	u64 enabled, running, total = 0;
	int cpu = event->cpu;
	struct cpu_data *cpudata = per_cpu(cpu_ev_data, cpu);

	spin_lock(&event->lock);
	if (((cpudata->is_hp || cpudata->is_idle) && !force)
						|| !event->active) {
		spin_unlock(&event->lock);
		return event->cached_count;
	}
	atomic_inc(&event->read_cnt);
	spin_unlock(&event->lock);

	if (local)
		perf_event_read_local(event->pevent, &total, NULL, NULL);
	else
		total = perf_event_read_value(event->pevent, &enabled,
								&running);
	event->cached_count = total;
	atomic_dec(&event->read_cnt);

	return total;
}

static int __qcom_pmu_read(int cpu, u32 event_id, u64 *pmu_data, bool local)
{
	struct cpu_data *cpu_data;
	struct event_data *event;
	int i;

	if (!qcom_pmu_inited)
		return -ENODEV;

	if (!event_id || !pmu_data || cpu >= num_possible_cpus())
		return -EINVAL;

	cpu_data = per_cpu(cpu_ev_data, cpu);
	for (i = 0; i < MAX_PMU_EVS; i++) {
		event = &cpu_data->events[i];
		if (event->event_id == event_id)
			break;
	}
	if (i == MAX_PMU_EVS)
		return -ENOENT;

	*pmu_data = read_event(event, local, false);

	return 0;
}

int __qcom_pmu_read_all(int cpu, struct qcom_pmu_data *data, bool local)
{
	struct cpu_data *cpu_data;
	struct event_data *event;
	int i, cnt = 0;

	if (!qcom_pmu_inited)
		return -ENODEV;

	if (!data || cpu >= num_possible_cpus())
		return -EINVAL;

	cpu_data = per_cpu(cpu_ev_data, cpu);
	for (i = 0; i < MAX_PMU_EVS; i++) {
		event = &cpu_data->events[i];
		if (!event->event_id)
			continue;
		data->event_ids[cnt] = event->event_id;
		data->ev_data[cnt] = read_event(event, local, false);
		cnt++;
	}
	data->num_evs = cnt;

	return 0;
}

int qcom_pmu_create(u32 event_id, int cpu)
{
	struct cpu_data *cpu_data;
	struct event_data *event, *new_event = NULL;
	struct perf_event_attr *attr = alloc_attr();
	int i, ret = 0;

	if (!attr)
		return -ENOMEM;

	if (!qcom_pmu_inited) {
		ret = -EPROBE_DEFER;
		goto out;
	}

	if (!event_id || cpu >= num_possible_cpus()) {
		ret = -EINVAL;
		goto out;
	}

	cpu_data = per_cpu(cpu_ev_data, cpu);
	mutex_lock(&cpu_data->events_lock);
	if (cpu_data->num_evs >= MAX_PMU_EVS) {
		ret = -ENOSPC;
		goto unlock_out;
	}
	for (i = 0; i < MAX_PMU_EVS; i++) {
		event = &cpu_data->events[i];
		if (event->event_id == event_id) {
			event->ref_cnt++;
			goto unlock_out;
		} else if (!event->event_id && !new_event)
			new_event = event;
	}

	if (!new_event) {
		ret = -ENOSPC;
		goto unlock_out;
	}

	new_event->event_id = event_id;
	ret = set_event(new_event, cpu, attr);
	if (ret < 0) {
		new_event->event_id = 0;
		goto unlock_out;
	} else {
		cpu_data->num_evs++;
		new_event->ref_cnt = 1;
	}

unlock_out:
	mutex_unlock(&cpu_data->events_lock);
out:
	kfree(attr);
	return ret;
}
EXPORT_SYMBOL(qcom_pmu_create);

int qcom_pmu_delete(u32 event_id, int cpu)
{
	struct cpu_data *cpu_data;
	struct event_data *event;
	int i, ret = 0;

	if (!qcom_pmu_inited)
		return -ENODEV;

	if (!event_id || cpu >= num_possible_cpus())
		return -EINVAL;

	cpu_data = per_cpu(cpu_ev_data, cpu);
	mutex_lock(&cpu_data->events_lock);
	if (cpu_data->num_evs == 0) {
		ret = -ENOENT;
		goto out;
	}
	for (i = 0; i < MAX_PMU_EVS; i++) {
		event = &cpu_data->events[i];
		if (event->event_id == event_id)
			break;
	}
	if (i == MAX_PMU_EVS) {
		ret = -ENOENT;
		goto out;
	}
	event->ref_cnt--;
	if (event->ref_cnt <= 0) {
		event->event_id = 0;
		delete_event(event);
		event->cached_count = 0;
		event->ref_cnt = 0;
		cpu_data->num_evs--;
	}

out:
	mutex_unlock(&cpu_data->events_lock);
	return ret;
}
EXPORT_SYMBOL(qcom_pmu_delete);

int qcom_pmu_read(int cpu, u32 event_id, u64 *pmu_data)
{
	return __qcom_pmu_read(cpu, event_id, pmu_data, false);
}
EXPORT_SYMBOL(qcom_pmu_read);

int qcom_pmu_read_local(u32 event_id, u64 *pmu_data)
{
	int this_cpu = smp_processor_id();

	return __qcom_pmu_read(this_cpu, event_id, pmu_data, true);
}
EXPORT_SYMBOL(qcom_pmu_read_local);

int qcom_pmu_read_all(int cpu, struct qcom_pmu_data *data)
{
	return __qcom_pmu_read_all(cpu, data, false);
}
EXPORT_SYMBOL(qcom_pmu_read_all);

int qcom_pmu_read_all_local(struct qcom_pmu_data *data)
{
	int this_cpu = smp_processor_id();

	return __qcom_pmu_read_all(this_cpu, data, true);
}
EXPORT_SYMBOL(qcom_pmu_read_all_local);

int qcom_pmu_idle_register(struct qcom_pmu_notif_node *idle_node)
{
	struct qcom_pmu_notif_node *tmp_node;

	if (!idle_node || !idle_node->idle_cb)
		return -EINVAL;

	spin_lock(&idle_list_lock);
	list_for_each_entry(tmp_node, &idle_notif_list, node)
		if (tmp_node->idle_cb == idle_node->idle_cb)
			goto out;
	list_add_tail(&idle_node->node, &idle_notif_list);
out:
	spin_unlock(&idle_list_lock);
	return 0;
}
EXPORT_SYMBOL(qcom_pmu_idle_register);

int qcom_pmu_idle_unregister(struct qcom_pmu_notif_node *idle_node)
{
	struct qcom_pmu_notif_node *tmp_node;
	int ret = -EINVAL;

	if (!idle_node || !idle_node->idle_cb)
		return ret;

	spin_lock(&idle_list_lock);
	list_for_each_entry(tmp_node, &idle_notif_list, node) {
		if (tmp_node->idle_cb == idle_node->idle_cb) {
			list_del(&tmp_node->node);
			ret = 0;
			break;
		}
	}
	spin_unlock(&idle_list_lock);
	return ret;
}
EXPORT_SYMBOL(qcom_pmu_idle_unregister);

static void qcom_pmu_idle_enter_notif(void *unused, int *state,
				      struct cpuidle_device *dev)
{
	struct cpu_data *cpu_data = per_cpu(cpu_ev_data, dev->cpu);
	struct qcom_pmu_data pmu_data;
	struct event_data *ev;
	struct qcom_pmu_notif_node *idle_node;
	int i, cnt = 0;

	if (cpu_data->is_idle || cpu_data->is_hp)
		return;
	cpu_data->is_idle = true;
	for (i = 0; i < MAX_PMU_EVS; i++) {
		ev = &cpu_data->events[i];
		if (!ev->event_id || !ev->pevent)
			continue;
		ev->cached_count = read_event(ev, true, true);
		pmu_data.event_ids[cnt] = ev->event_id;
		pmu_data.ev_data[cnt] = ev->cached_count;
		cnt++;
	}
	pmu_data.num_evs = cnt;

	/* send snapshot of pmu data to all registered idle clients */
	list_for_each_entry(idle_node, &idle_notif_list, node)
		idle_node->idle_cb(&pmu_data, dev->cpu, *state);
}

static void qcom_pmu_idle_exit_notif(void *unused, int state,
				     struct cpuidle_device *dev)
{
	struct cpu_data *cpu_data = per_cpu(cpu_ev_data, dev->cpu);

	cpu_data->is_idle = false;
}

#if IS_ENABLED(CONFIG_HOTPLUG_CPU)
static int qcom_pmu_hotplug_coming_up(unsigned int cpu)
{
	struct perf_event_attr *attr = alloc_attr();
	struct cpu_data *cpu_data = per_cpu(cpu_ev_data, cpu);
	int i, ret = 0;

	if (!attr)
		return -ENOMEM;

	if (!qcom_pmu_inited)
		goto out;

	mutex_lock(&cpu_data->events_lock);
	for (i = 0; i < MAX_PMU_EVS; i++) {
		ret = set_event(&cpu_data->events[i], cpu, attr);
		if (ret < 0) {
			pr_err("event %d not set for cpu %d ret %d\n",
				cpu_data->events[i].event_id, cpu, ret);
			break;
		}
	}
	cpu_data->is_hp = false;
	mutex_unlock(&cpu_data->events_lock);

out:
	kfree(attr);
	return 0;
}

static int qcom_pmu_hotplug_going_down(unsigned int cpu)
{
	struct cpu_data *cpu_data = per_cpu(cpu_ev_data, cpu);
	struct event_data *event;
	int i;

	if (!qcom_pmu_inited)
		return 0;

	mutex_lock(&cpu_data->events_lock);
	cpu_data->is_hp = true;
	for (i = 0; i < MAX_PMU_EVS; i++) {
		event = &cpu_data->events[i];
		if (!event->event_id || !event->pevent)
			continue;
		event->cached_count = read_event(event, false, true);
		delete_event(event);
	}
	mutex_unlock(&cpu_data->events_lock);

	return 0;
}

static int qcom_pmu_cpu_hp_init(void)
{
	int ret = 0;

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
				"QCOM_PMU",
				qcom_pmu_hotplug_coming_up,
				qcom_pmu_hotplug_going_down);
	if (ret < 0)
		pr_err("qcom_pmu: CPU hotplug notifier error: %d\n", ret);
	else
		ret = 0;
	return ret;
}
#else
static int qcom_pmu_cpu_hp_init(void) { return 0; }
#endif

static int qcom_pmu_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	unsigned int cpu, i;
	struct cpu_data *cpu_data;

	get_online_cpus();
	for_each_possible_cpu(cpu) {
		cpu_data = devm_kzalloc(dev, sizeof(*cpu_data), GFP_KERNEL);
		if (!cpu_data) {
			ret = -ENOMEM;
			goto out;
		}
		if (!cpumask_test_cpu(cpu, cpu_online_mask))
			cpu_data->is_hp = true;
		mutex_init(&cpu_data->events_lock);
		for (i = 0; i < MAX_PMU_EVS; i++) {
			spin_lock_init(&cpu_data->events[i].lock);
			atomic_set(&cpu_data->events[i].read_cnt, 0);
		}
		per_cpu(cpu_ev_data, cpu) = cpu_data;
	}

	ret = qcom_pmu_cpu_hp_init();
	if (ret < 0) {
		dev_err(dev, "qcom pmu driver failed to probe: %d\n", ret);
		goto out;
	}
	register_trace_android_vh_cpu_idle_enter(qcom_pmu_idle_enter_notif, NULL);
	register_trace_android_vh_cpu_idle_exit(qcom_pmu_idle_exit_notif, NULL);
	qcom_pmu_inited = true;

out:
	put_online_cpus();
	return ret;
}

static int qcom_pmu_driver_remove(struct platform_device *pdev)
{
	struct cpu_data *cpu_data;
	struct event_data *event;
	int cpu, i;

	qcom_pmu_inited = false;
	cpuhp_remove_state_nocalls(CPUHP_AP_ONLINE_DYN);
	unregister_trace_android_vh_cpu_idle_enter(qcom_pmu_idle_enter_notif, NULL);
	unregister_trace_android_vh_cpu_idle_exit(qcom_pmu_idle_exit_notif, NULL);
	for_each_possible_cpu(cpu) {
		cpu_data = per_cpu(cpu_ev_data, cpu);
		cpu_data->is_hp = true;
		cpu_data->is_idle = true;
	}

	for_each_possible_cpu(cpu) {
		cpu_data = per_cpu(cpu_ev_data, cpu);
		mutex_lock(&cpu_data->events_lock);
		for (i = 0; i < MAX_PMU_EVS; i++) {
			event = &cpu_data->events[i];
			if (!event->event_id || !event->pevent)
				continue;
			event->event_id = 0;
			delete_event(event);
			event->cached_count = 0;
			event->ref_cnt = 0;
			cpu_data->num_evs--;
		}
		cpu_data->num_evs = 0;
		mutex_unlock(&cpu_data->events_lock);
	}

	return 0;
}

static const struct of_device_id pmu_match_table[] = {
	{ .compatible = "qcom,pmu" },
	{}
};

static struct platform_driver qcom_pmu_driver = {
	.probe = qcom_pmu_driver_probe,
	.remove = qcom_pmu_driver_remove,
	.driver = {
		.name = "qcom-pmu",
		.of_match_table = pmu_match_table,
		.suppress_bind_attrs = true,
	},
};

static int __init qcom_pmu_init(void)
{
	return platform_driver_register(&qcom_pmu_driver);
}
module_init(qcom_pmu_init);

static __exit void qcom_pmu_exit(void)
{
	platform_driver_unregister(&qcom_pmu_driver);
}
module_exit(qcom_pmu_exit);

MODULE_DESCRIPTION("QCOM PMU Driver");
MODULE_LICENSE("GPL v2");
