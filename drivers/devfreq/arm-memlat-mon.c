// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2018, 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "arm-memlat-mon: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/of_fdt.h>
#include "governor.h"
#include "governor_memlat.h"
#include <linux/perf_event.h>
#include <linux/of_device.h>
#include <linux/mutex.h>

enum common_ev_idx {
	INST_IDX,
	CYC_IDX,
	STALL_IDX,
	NUM_COMMON_EVS
};
#define INST_EV		0x08
#define CYC_EV		0x11

enum mon_type {
	MEMLAT_CPU_GRP,
	MEMLAT_MON,
	COMPUTE_MON,
	NUM_MON_TYPES
};

struct event_data {
	struct perf_event *pevent;
	unsigned long prev_count;
	unsigned long last_delta;
};

struct cpu_data {
	struct event_data common_evs[NUM_COMMON_EVS];
	unsigned long freq;
	unsigned long stall_pct;
};

/**
 * struct memlat_mon - A specific consumer of cpu_grp generic counters.
 *
 * @is_active:			Whether or not this mon is currently running
 *				memlat.
 * @cpus:			CPUs this mon votes on behalf of. Must be a
 *				subset of @cpu_grp's CPUs. If no CPUs provided,
 *				defaults to using all of @cpu_grp's CPUs.
 * @miss_ev_id:			The event code corresponding to the @miss_ev
 *				perf event. Will be 0 for compute.
 * @access_ev_id:		The event code corresponding to the @access_ev
 *				perf event. Optional - only needed for writeback
 *				percent.
 * @wb_ev_id:			The event code corresponding to the @wb_ev perf
 *				event. Optional - only needed for writeback
 *				percent.
 * @miss_ev:			The cache miss perf event exclusive to this
 *				mon. Will be NULL for compute.
 * @access_ev:			The cache access perf event exclusive to this
 *				mon. Optional - only needed for writeback
 *				percent.
 * @wb_ev:			The cache writeback perf event exclusive to this
 *				mon. Optional - only needed for writeback
 *				percent.
 * @requested_update_ms:	The mon's desired polling rate. The lowest
 *				@requested_update_ms of all mons determines
 *				@cpu_grp's update_ms.
 * @hw:				The memlat_hwmon struct corresponding to this
 *				mon's specific memlat instance.
 * @cpu_grp:			The cpu_grp who owns this mon.
 */
struct memlat_mon {
	bool			is_active;
	cpumask_t		cpus;
	unsigned int		miss_ev_id;
	unsigned int		access_ev_id;
	unsigned int		wb_ev_id;
	unsigned int		requested_update_ms;
	struct event_data	*miss_ev;
	struct event_data	*access_ev;
	struct event_data	*wb_ev;
	struct memlat_hwmon	hw;

	struct memlat_cpu_grp	*cpu_grp;
};

/**
 * struct memlat_cpu_grp - A coordinator of both HW reads and devfreq updates
 * for one or more memlat_mons.
 *
 * @cpus:			The CPUs this cpu_grp will read events from.
 * @common_ev_ids:		The event codes of the events all mons need.
 * @cpus_data:			The cpus data array of length #cpus. Includes
 *				event_data of all the events all mons need as
 *				well as common computed cpu data like freq.
 * @last_update_ts:		Used to avoid redundant reads.
 * @last_ts_delta_us:		The time difference between the most recent
 *				update and the one before that. Used to compute
 *				effective frequency.
 * @work:			The delayed_work used for handling updates.
 * @update_ms:			The frequency with which @work triggers.
 * @num_mons:		The number of @mons for this cpu_grp.
 * @num_inited_mons:	The number of @mons who have probed.
 * @num_active_mons:	The number of @mons currently running
 *				memlat.
 * @mons:			All of the memlat_mon structs representing
 *				the different voters who share this cpu_grp.
 * @mons_lock:		A lock used to protect the @mons.
 */
struct memlat_cpu_grp {
	cpumask_t		cpus;
	unsigned int		common_ev_ids[NUM_COMMON_EVS];
	struct cpu_data		*cpus_data;
	ktime_t			last_update_ts;
	unsigned long		last_ts_delta_us;

	struct delayed_work	work;
	unsigned int		update_ms;

	unsigned int		num_mons;
	unsigned int		num_inited_mons;
	unsigned int		num_active_mons;
	struct memlat_mon	*mons;
	struct mutex		mons_lock;
};

struct memlat_mon_spec {
	enum mon_type type;
};

#define to_cpu_data(cpu_grp, cpu) \
	(&cpu_grp->cpus_data[cpu - cpumask_first(&cpu_grp->cpus)])
#define to_common_evs(cpu_grp, cpu) \
	(cpu_grp->cpus_data[cpu - cpumask_first(&cpu_grp->cpus)].common_evs)
#define to_devstats(mon, cpu) \
	(&mon->hw.core_stats[cpu - cpumask_first(&mon->cpus)])
#define to_mon(hwmon) container_of(hwmon, struct memlat_mon, hw)

static struct workqueue_struct *memlat_wq;

#define MAX_COUNT_LIM 0xFFFFFFFFFFFFFFFF
static inline void read_event(struct event_data *event)
{
	unsigned long ev_count = 0;
	u64 total, enabled, running;

	if (!event->pevent)
		return;

	total = perf_event_read_value(event->pevent, &enabled, &running);
	ev_count = total - event->prev_count;
	event->prev_count = total;
	event->last_delta = ev_count;
}

static void update_counts(struct memlat_cpu_grp *cpu_grp)
{
	unsigned int cpu, i;
	struct memlat_mon *mon;
	ktime_t now = ktime_get();
	unsigned long delta = ktime_us_delta(now, cpu_grp->last_update_ts);

	cpu_grp->last_ts_delta_us = delta;
	cpu_grp->last_update_ts = now;

	for_each_cpu(cpu, &cpu_grp->cpus) {
		struct cpu_data *cpu_data = to_cpu_data(cpu_grp, cpu);
		struct event_data *common_evs = cpu_data->common_evs;

		for (i = 0; i < NUM_COMMON_EVS; i++)
			read_event(&common_evs[i]);

		if (!common_evs[STALL_IDX].pevent)
			common_evs[STALL_IDX].last_delta =
				common_evs[CYC_IDX].last_delta;

		cpu_data->freq = common_evs[CYC_IDX].last_delta / delta;
		cpu_data->stall_pct = mult_frac(100,
				common_evs[STALL_IDX].last_delta,
				common_evs[CYC_IDX].last_delta);
	}

	for (i = 0; i < cpu_grp->num_mons; i++) {
		mon = &cpu_grp->mons[i];

		if (!mon->is_active || !mon->miss_ev)
			continue;

		for_each_cpu(cpu, &mon->cpus) {
			unsigned int mon_idx =
				cpu - cpumask_first(&mon->cpus);
			read_event(&mon->miss_ev[mon_idx]);

			if (mon->wb_ev_id && mon->access_ev_id) {
				read_event(&mon->wb_ev[mon_idx]);
				read_event(&mon->access_ev[mon_idx]);
			}
		}
	}
}

static unsigned long get_cnt(struct memlat_hwmon *hw)
{
	struct memlat_mon *mon = to_mon(hw);
	struct memlat_cpu_grp *cpu_grp = mon->cpu_grp;
	unsigned int cpu;

	for_each_cpu(cpu, &mon->cpus) {
		struct cpu_data *cpu_data = to_cpu_data(cpu_grp, cpu);
		struct event_data *common_evs = cpu_data->common_evs;
		unsigned int mon_idx =
			cpu - cpumask_first(&mon->cpus);
		struct dev_stats *devstats = to_devstats(mon, cpu);

		devstats->freq = cpu_data->freq;
		devstats->stall_pct = cpu_data->stall_pct;
		devstats->inst_count = common_evs[INST_IDX].last_delta;

		if (mon->miss_ev)
			devstats->mem_count =
				mon->miss_ev[mon_idx].last_delta;
		else {
			devstats->inst_count = 0;
			devstats->mem_count = 1;
		}

		if (mon->access_ev_id && mon->wb_ev_id)
			devstats->wb_pct =
				mult_frac(100, mon->wb_ev[mon_idx].last_delta,
					  mon->access_ev[mon_idx].last_delta);
		else
			devstats->wb_pct = 0;
	}

	return 0;
}

static void delete_event(struct event_data *event)
{
	event->prev_count = event->last_delta = 0;
	if (event->pevent) {
		perf_event_release_kernel(event->pevent);
		event->pevent = NULL;
	}
}

static struct perf_event_attr *alloc_attr(void)
{
	struct perf_event_attr *attr;

	attr = kzalloc(sizeof(struct perf_event_attr), GFP_KERNEL);
	if (!attr)
		return attr;

	attr->type = PERF_TYPE_RAW;
	attr->size = sizeof(struct perf_event_attr);
	attr->pinned = 1;
	attr->exclude_idle = 1;

	return attr;
}

static int set_event(struct event_data *ev, int cpu, unsigned int event_id,
		     struct perf_event_attr *attr)
{
	struct perf_event *pevent;

	if (!event_id)
		return 0;

	attr->config = event_id;
	pevent = perf_event_create_kernel_counter(attr, cpu, NULL, NULL, NULL);
	if (IS_ERR(pevent))
		return PTR_ERR(pevent);

	ev->pevent = pevent;
	perf_event_enable(pevent);

	return 0;
}

static int init_common_evs(struct memlat_cpu_grp *cpu_grp,
			   struct perf_event_attr *attr)
{
	unsigned int cpu, i;
	int ret = 0;

	for_each_cpu(cpu, &cpu_grp->cpus) {
		struct event_data *common_evs = to_common_evs(cpu_grp, cpu);

		for (i = 0; i < NUM_COMMON_EVS; i++) {
			ret = set_event(&common_evs[i], cpu,
					cpu_grp->common_ev_ids[i], attr);
			if (ret)
				break;
		}
	}

	return ret;
}

static void free_common_evs(struct memlat_cpu_grp *cpu_grp)
{
	unsigned int cpu, i;

	for_each_cpu(cpu, &cpu_grp->cpus) {
		struct event_data *common_evs = to_common_evs(cpu_grp, cpu);

		for (i = 0; i < NUM_COMMON_EVS; i++)
			delete_event(&common_evs[i]);
	}
}

static void memlat_monitor_work(struct work_struct *work)
{
	int err;
	struct memlat_cpu_grp *cpu_grp =
		container_of(work, struct memlat_cpu_grp, work.work);
	struct memlat_mon *mon;
	unsigned int i;

	mutex_lock(&cpu_grp->mons_lock);
	if (!cpu_grp->num_active_mons)
		goto unlock_out;
	update_counts(cpu_grp);
	for (i = 0; i < cpu_grp->num_mons; i++) {
		struct devfreq *df;

		mon = &cpu_grp->mons[i];

		if (!mon->is_active)
			continue;

		df = mon->hw.df;
		mutex_lock(&df->lock);
		err = update_devfreq(df);
		if (err)
			dev_err(mon->hw.dev, "Memlat update failed: %d\n", err);
		mutex_unlock(&df->lock);
	}

	queue_delayed_work(memlat_wq, &cpu_grp->work,
			   msecs_to_jiffies(cpu_grp->update_ms));

unlock_out:
	mutex_unlock(&cpu_grp->mons_lock);
}

static int start_hwmon(struct memlat_hwmon *hw)
{
	int ret = 0;
	unsigned int cpu;
	struct memlat_mon *mon = to_mon(hw);
	struct memlat_cpu_grp *cpu_grp = mon->cpu_grp;
	bool should_init_cpu_grp;
	struct perf_event_attr *attr = alloc_attr();

	if (!attr)
		return -ENOMEM;

	mutex_lock(&cpu_grp->mons_lock);
	should_init_cpu_grp = !(cpu_grp->num_active_mons++);
	if (should_init_cpu_grp) {
		ret = init_common_evs(cpu_grp, attr);
		if (ret)
			goto unlock_out;

		INIT_DEFERRABLE_WORK(&cpu_grp->work, &memlat_monitor_work);
	}

	if (mon->miss_ev) {
		for_each_cpu(cpu, &mon->cpus) {
			unsigned int idx = cpu - cpumask_first(&mon->cpus);

			ret = set_event(&mon->miss_ev[idx], cpu,
					mon->miss_ev_id, attr);
			if (ret)
				goto unlock_out;

			if (mon->access_ev_id && mon->wb_ev_id) {
				ret = set_event(&mon->access_ev[idx], cpu,
						mon->access_ev_id, attr);
				if (ret)
					goto unlock_out;

				ret = set_event(&mon->wb_ev[idx], cpu,
						mon->wb_ev_id, attr);
				if (ret)
					goto unlock_out;
			}
		}
	}

	mon->is_active = true;

	if (should_init_cpu_grp)
		queue_delayed_work(memlat_wq, &cpu_grp->work,
				   msecs_to_jiffies(cpu_grp->update_ms));

unlock_out:
	mutex_unlock(&cpu_grp->mons_lock);
	kfree(attr);

	return ret;
}

static void stop_hwmon(struct memlat_hwmon *hw)
{
	unsigned int cpu;
	struct memlat_mon *mon = to_mon(hw);
	struct memlat_cpu_grp *cpu_grp = mon->cpu_grp;

	mutex_lock(&cpu_grp->mons_lock);
	mon->is_active = false;
	cpu_grp->num_active_mons--;

	for_each_cpu(cpu, &mon->cpus) {
		unsigned int idx = cpu - cpumask_first(&mon->cpus);
		struct dev_stats *devstats = to_devstats(mon, cpu);

		if (mon->miss_ev)
			delete_event(&mon->miss_ev[idx]);
		devstats->inst_count = 0;
		devstats->mem_count = 0;
		devstats->freq = 0;
		devstats->stall_pct = 0;
		devstats->wb_pct = 0;
	}

	if (!cpu_grp->num_active_mons) {
		cancel_delayed_work(&cpu_grp->work);
		free_common_evs(cpu_grp);
	}
	mutex_unlock(&cpu_grp->mons_lock);
}

/**
 * We should set update_ms to the lowest requested_update_ms of all of the
 * active mons, or 0 (i.e. stop polling) if ALL active mons have 0.
 * This is expected to be called with cpu_grp->mons_lock taken.
 */
static void set_update_ms(struct memlat_cpu_grp *cpu_grp)
{
	struct memlat_mon *mon;
	unsigned int i, new_update_ms = UINT_MAX;

	for (i = 0; i < cpu_grp->num_mons; i++) {
		mon = &cpu_grp->mons[i];
		if (mon->is_active && mon->requested_update_ms)
			new_update_ms =
				min(new_update_ms, mon->requested_update_ms);
	}

	if (new_update_ms == UINT_MAX) {
		cancel_delayed_work(&cpu_grp->work);
	} else if (cpu_grp->update_ms == UINT_MAX) {
		queue_delayed_work(memlat_wq, &cpu_grp->work,
				   msecs_to_jiffies(new_update_ms));
	} else if (new_update_ms > cpu_grp->update_ms) {
		cancel_delayed_work(&cpu_grp->work);
		queue_delayed_work(memlat_wq, &cpu_grp->work,
				   msecs_to_jiffies(new_update_ms));
	}

	cpu_grp->update_ms = new_update_ms;
}

static void request_update_ms(struct memlat_hwmon *hw, unsigned int update_ms)
{
	struct devfreq *df = hw->df;
	struct memlat_mon *mon = to_mon(hw);
	struct memlat_cpu_grp *cpu_grp = mon->cpu_grp;

	mutex_lock(&df->lock);
	df->profile->polling_ms = update_ms;
	mutex_unlock(&df->lock);

	mutex_lock(&cpu_grp->mons_lock);
	mon->requested_update_ms = update_ms;
	set_update_ms(cpu_grp);
	mutex_unlock(&cpu_grp->mons_lock);
}

static int get_mask_from_dev_handle(struct platform_device *pdev,
					cpumask_t *mask)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_phandle;
	struct device *cpu_dev;
	int cpu, i = 0;
	int ret = -ENOENT;

	dev_phandle = of_parse_phandle(dev->of_node, "qcom,cpulist", i++);
	while (dev_phandle) {
		for_each_possible_cpu(cpu) {
			cpu_dev = get_cpu_device(cpu);
			if (cpu_dev && cpu_dev->of_node == dev_phandle) {
				cpumask_set_cpu(cpu, mask);
				ret = 0;
				break;
			}
		}
		dev_phandle = of_parse_phandle(dev->of_node,
						"qcom,cpulist", i++);
	}

	return ret;
}

static struct device_node *parse_child_nodes(struct device *dev)
{
	struct device_node *of_child;
	int ddr_type_of = -1;
	int ddr_type = of_fdt_get_ddrtype();
	int ret;

	for_each_child_of_node(dev->of_node, of_child) {
		ret = of_property_read_u32(of_child, "qcom,ddr-type",
							&ddr_type_of);
		if (!ret && (ddr_type == ddr_type_of)) {
			dev_dbg(dev,
				"ddr-type = %d, is matching DT entry\n",
				ddr_type_of);
			return of_child;
		}
	}
	return NULL;
}

#define DEFAULT_UPDATE_MS 100
static int memlat_cpu_grp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct memlat_cpu_grp *cpu_grp;
	int ret = 0;
	unsigned int event_id, num_cpus, num_mons;

	cpu_grp = devm_kzalloc(dev, sizeof(*cpu_grp), GFP_KERNEL);
	if (!cpu_grp)
		return -ENOMEM;

	if (get_mask_from_dev_handle(pdev, &cpu_grp->cpus)) {
		dev_err(dev, "No CPUs specified.\n");
		return -ENODEV;
	}

	num_mons = of_get_available_child_count(dev->of_node);

	if (!num_mons) {
		dev_err(dev, "No mons provided.\n");
		return -ENODEV;
	}

	cpu_grp->num_mons = num_mons;
	cpu_grp->num_inited_mons = 0;

	cpu_grp->mons =
		devm_kzalloc(dev, num_mons * sizeof(*cpu_grp->mons),
			     GFP_KERNEL);
	if (!cpu_grp->mons)
		return -ENOMEM;

	ret = of_property_read_u32(dev->of_node, "qcom,inst-ev", &event_id);
	if (ret) {
		dev_dbg(dev, "Inst event not specified. Using def:0x%x\n",
			INST_EV);
		event_id = INST_EV;
	}
	cpu_grp->common_ev_ids[INST_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,cyc-ev", &event_id);
	if (ret) {
		dev_dbg(dev, "Cyc event not specified. Using def:0x%x\n",
			CYC_EV);
		event_id = CYC_EV;
	}
	cpu_grp->common_ev_ids[CYC_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,stall-ev", &event_id);
	if (ret)
		dev_dbg(dev, "Stall event not specified. Skipping.\n");
	else
		cpu_grp->common_ev_ids[STALL_IDX] = event_id;

	num_cpus = cpumask_weight(&cpu_grp->cpus);
	cpu_grp->cpus_data =
		devm_kzalloc(dev, num_cpus * sizeof(*cpu_grp->cpus_data),
			     GFP_KERNEL);
	if (!cpu_grp->cpus_data)
		return -ENOMEM;

	mutex_init(&cpu_grp->mons_lock);
	cpu_grp->update_ms = DEFAULT_UPDATE_MS;

	dev_set_drvdata(dev, cpu_grp);

	return 0;
}

static int memlat_mon_probe(struct platform_device *pdev, bool is_compute)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct memlat_cpu_grp *cpu_grp;
	struct memlat_mon *mon;
	struct memlat_hwmon *hw;
	unsigned int event_id, num_cpus, cpu;

	if (!memlat_wq)
		memlat_wq = create_freezable_workqueue("memlat_wq");

	if (!memlat_wq) {
		dev_err(dev, "Couldn't create memlat workqueue.\n");
		return -ENOMEM;
	}

	cpu_grp = dev_get_drvdata(dev->parent);
	if (!cpu_grp) {
		dev_err(dev, "Mon initialized without cpu_grp.\n");
		return -ENODEV;
	}

	mutex_lock(&cpu_grp->mons_lock);
	mon = &cpu_grp->mons[cpu_grp->num_inited_mons];
	mon->is_active = false;
	mon->requested_update_ms = 0;
	mon->cpu_grp = cpu_grp;

	if (get_mask_from_dev_handle(pdev, &mon->cpus)) {
		cpumask_copy(&mon->cpus, &cpu_grp->cpus);
	} else {
		if (!cpumask_subset(&mon->cpus, &cpu_grp->cpus)) {
			dev_err(dev,
				"Mon CPUs must be a subset of cpu_grp CPUs. mon=%*pbl cpu_grp=%*pbl\n",
				mon->cpus, cpu_grp->cpus);
			ret = -EINVAL;
			goto unlock_out;
		}
	}

	num_cpus = cpumask_weight(&mon->cpus);

	hw = &mon->hw;
	hw->of_node = of_parse_phandle(dev->of_node, "qcom,target-dev", 0);
	if (!hw->of_node) {
		dev_err(dev, "Couldn't find a target device.\n");
		ret = -ENODEV;
		goto unlock_out;
	}
	hw->dev = dev;
	hw->num_cores = num_cpus;
	hw->should_ignore_df_monitor = true;
	hw->core_stats = devm_kzalloc(dev, num_cpus * sizeof(*(hw->core_stats)),
				      GFP_KERNEL);
	if (!hw->core_stats) {
		ret = -ENOMEM;
		goto unlock_out;
	}

	for_each_cpu(cpu, &mon->cpus)
		to_devstats(mon, cpu)->id = cpu;

	hw->start_hwmon = &start_hwmon;
	hw->stop_hwmon = &stop_hwmon;
	hw->get_cnt = &get_cnt;
	if (of_get_child_count(dev->of_node))
		hw->get_child_of_node = &parse_child_nodes;
	hw->request_update_ms = &request_update_ms;

	/*
	 * Compute mons rely solely on common events.
	 */
	if (is_compute) {
		mon->miss_ev_id = 0;
		mon->access_ev_id = 0;
		mon->wb_ev_id = 0;
		ret = register_compute(dev, hw);
	} else {
		mon->miss_ev =
			devm_kzalloc(dev, num_cpus * sizeof(*mon->miss_ev),
				     GFP_KERNEL);
		if (!mon->miss_ev) {
			ret = -ENOMEM;
			goto unlock_out;
		}

		ret = of_property_read_u32(dev->of_node, "qcom,cachemiss-ev",
						&event_id);
		if (ret) {
			dev_err(dev, "Cache miss event missing for mon: %d\n",
					ret);
			ret = -EINVAL;
			goto unlock_out;
		}
		mon->miss_ev_id = event_id;

		ret = of_property_read_u32(dev->of_node, "qcom,access-ev",
					   &event_id);
		if (ret)
			dev_dbg(dev, "Access event not specified. Skipping.\n");
		else
			mon->access_ev_id = event_id;

		ret = of_property_read_u32(dev->of_node, "qcom,wb-ev",
					   &event_id);
		if (ret)
			dev_dbg(dev, "WB event not specified. Skipping.\n");
		else
			mon->wb_ev_id = event_id;

		if (mon->wb_ev_id && mon->access_ev_id) {
			mon->access_ev =
				devm_kzalloc(dev, num_cpus *
					     sizeof(*mon->access_ev),
					     GFP_KERNEL);
			if (!mon->access_ev) {
				ret = -ENOMEM;
				goto unlock_out;
			}

			mon->wb_ev =
				devm_kzalloc(dev, num_cpus *
					     sizeof(*mon->wb_ev), GFP_KERNEL);
			if (!mon->wb_ev) {
				ret = -ENOMEM;
				goto unlock_out;
			}
		}

		ret = register_memlat(dev, hw);
	}

	if (!ret)
		cpu_grp->num_inited_mons++;

unlock_out:
	mutex_unlock(&cpu_grp->mons_lock);
	return ret;
}

static int arm_memlat_mon_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	const struct memlat_mon_spec *spec = of_device_get_match_data(dev);
	enum mon_type type = NUM_MON_TYPES;

	if (spec)
		type = spec->type;

	switch (type) {
	case MEMLAT_CPU_GRP:
		ret = memlat_cpu_grp_probe(pdev);
		if (of_get_available_child_count(dev->of_node))
			of_platform_populate(dev->of_node, NULL, NULL, dev);
		break;
	case MEMLAT_MON:
		ret = memlat_mon_probe(pdev, false);
		break;
	case COMPUTE_MON:
		ret = memlat_mon_probe(pdev, true);
		break;
	default:
		/*
		 * This should never happen.
		 */
		dev_err(dev, "Invalid memlat mon type specified: %u\n", type);
		return -EINVAL;
	}

	if (ret) {
		dev_err(dev, "Failure to probe memlat device: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct memlat_mon_spec spec[] = {
	[0] = { MEMLAT_CPU_GRP },
	[1] = { MEMLAT_MON },
	[2] = { COMPUTE_MON },
};

static const struct of_device_id memlat_match_table[] = {
	{ .compatible = "qcom,arm-memlat-cpugrp", .data = &spec[0] },
	{ .compatible = "qcom,arm-memlat-mon", .data = &spec[1] },
	{ .compatible = "qcom,arm-compute-mon", .data = &spec[2] },
	{}
};

static struct platform_driver arm_memlat_mon_driver = {
	.probe = arm_memlat_mon_driver_probe,
	.driver = {
		.name = "arm-memlat-mon",
		.of_match_table = memlat_match_table,
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(arm_memlat_mon_driver);
