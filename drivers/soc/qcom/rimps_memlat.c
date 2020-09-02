// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013 - 2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "rimps-memlat: " fmt

#include <linux/scmi_protocol.h>
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
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/of_fdt.h>
#include <linux/perf_event.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/cpu.h>
#include <linux/spinlock.h>

#define INST_EV 0x08
#define CYC_EV 0x11
#define MAX_NAME_LEN 50

#define DEFAULT_RATIO_CEIL 400
#define DEFAULT_STALL_FLOOR 100
#define DEFAULT_SAMPLE_MS 10
#define DEFAULT_L2WB_PCT 100
#define DEFAULT_L2WB_IPM_FILTER 25000
#define MAX_L3_ENTRIES 40U
#define INVALID_PMU_HW_IDX 0xFF
#define PERF_STATE_DESIRED_MASK 0x3F
#define MIN_LOG_LEVEL	0
#define MAX_LOG_LEVEL	0xF
#define INVALID_PMU_EVENT_ID 0
#define MAX_PMU_CNTRS_RIMPS	8
#define MAX_EVCNTRS	6

#define to_cpu_data(cpu_grp, cpu) \
	(&cpu_grp->cpus_data[cpu - cpumask_first(&cpu_grp->cpus)])

#define to_mon_ev_data(mon, cpu) \
	(&mon->ev_data[(cpu - cpumask_first(&mon->cpus))])

#define to_common_pmu_map(d, cpu) \
	(&d->common_ev_map[(cpu - cpumask_first(&d->cpus)) * NUM_COMMON_EVS])

#define to_mon_pmu_map(mon, cpu) \
	(&mon->mon_ev_map[(cpu - cpumask_first(&mon->cpus)) * NUM_MON_EVS])

#define to_memlat_mon(k) container_of(k, struct memlat_mon, kobj)

#define to_cpu_grp(k) container_of(k, struct memlat_cpu_grp, kobj)

#define to_memlat_attr(a)	\
		container_of(a, struct memlat_attr, attr)

enum common_ev_idx {
	INST_IDX,
	CYC_IDX,
	STALL_IDX,
	NUM_COMMON_EVS
};

enum mon_ev_idx {
	MISS_IDX,
	L2WB_IDX,
	L3_ACCESS_IDX,
	NUM_MON_EVS
};

enum mon_type {
	MON_NONE,
	MEMLAT_CPU_GRP,
	L3_MEMLAT,
	MAX_MEMLAT_DEVICE_TYPE
};

struct core_dev_map {
	u32 core_mhz;
	u32 target_freq;
};

struct cpu_pmu_ctrs {
	uint32_t ccntr_lo;
	uint32_t ccntr_hi;
	uint32_t evcntr[MAX_EVCNTRS];
	uint32_t valid;
	uint32_t unused0;
};

struct pmu_map {
	u32 event_id;
	u32 hw_cntr_idx;
};

struct cpu_data {
	struct perf_event *common_evs[NUM_COMMON_EVS];
};

struct mon_data {
	struct perf_event *mon_evs[NUM_MON_EVS];
};

struct memlat_mon {
	cpumask_t cpus;
	u32 cpus_mpidr;
	u32 mon_type;
	u32 ratio_ceil;
	u32 stall_floor;
	u32 sample_ms;
	u32 l2wb_pct;
	u32 l2wb_filter;
	u32 min_freq;
	u32 max_freq;
	u32 mon_started;
	u32 num_freq_map_entries;
	u32 mon_ev_ids[NUM_MON_EVS];
	void __iomem *perf_base;
	char mon_name[MAX_NAME_LEN];
	struct mon_data	*ev_data;
	struct core_dev_map *freq_map;
	struct pmu_map *mon_ev_map;
	struct memlat_cpu_grp *cpu_grp;
	struct kobject kobj;
};

struct memlat_cpu_grp {
	cpumask_t cpus;
	u32 cpus_mpidr;
	u32 common_ev_ids[NUM_COMMON_EVS];
	u32 num_mons;
	u32 num_inited_mons;
	struct cpu_data	*cpus_data;
	struct memlat_mon *mons;
	const struct scmi_handle *handle;
	struct list_head node;
	struct device *dev;
	struct kobject kobj;
	struct pmu_map *common_ev_map;
	struct mutex mons_lock;
};

struct memlat_mon_spec {
	enum mon_type type;
};

struct memlat_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj, char *buf);
	ssize_t (*store)(struct kobject *kobj, const char *buf,
				size_t count);
};

static DEFINE_PER_CPU(struct memlat_cpu_grp *, per_cpu_grp);
static DEFINE_PER_CPU(bool, cpu_is_idle);
static DEFINE_PER_CPU(bool, cpu_is_hp);
static bool	use_cached_l3_freqs;
static bool lpm_hp_notifier_registered;
static u32 l3_pstates;
static u32 rimps_log_level;
static void __iomem *pmu_base;
static unsigned long l3_freqs[MAX_L3_ENTRIES];
static struct kobject *rimps_kobj;

#define memlat_mon_attr_rw(_name)		\
static struct memlat_attr _name =			\
__ATTR(_name, 0644, show_##_name, store_##_name)	\

#define memlat_mon_attr_ro(_name)		\
static struct memlat_attr _name =			\
__ATTR(_name, 0644, show_##_name, NULL)		\

#define show_attr(name)							\
static ssize_t show_##name(struct kobject *kobj,			\
				char *buf)				\
{									\
	struct memlat_mon *mon = to_memlat_mon(kobj);			\
	return scnprintf(buf, PAGE_SIZE, "%u\n", mon->name);		\
}									\

#define store_attr(name, _min, _max) \
static ssize_t store_##name(struct kobject *kobj,			\
			const char *buf,				\
			size_t count)					\
{									\
	int ret;							\
	unsigned int val;						\
	struct memlat_mon *mon = to_memlat_mon(kobj);			\
	struct memlat_cpu_grp *cpu_grp = mon->cpu_grp;			\
	struct scmi_memlat_vendor_ops *ops = cpu_grp->handle->memlat_ops;	\
	ret = kstrtouint(buf, 10, &val);				\
	if (ret < 0)							\
		return ret;						\
	mutex_lock(&cpu_grp->mons_lock);					\
	val = max(val, _min);						\
	val = min(val, _max);						\
	mon->name = val;						\
	if (mon->mon_started)						\
		ret = ops->name(cpu_grp->handle, mon->cpus_mpidr,		\
				mon->mon_type, mon->name);		\
	if (ret < 0) {							\
		pr_err("failed to set mon tunable %s, ret = %d\n",	\
						"name", ret);		\
		count = 0;						\
	}								\
	mutex_unlock(&cpu_grp->mons_lock);					\
	return count;							\
}									\

show_attr(ratio_ceil);
store_attr(ratio_ceil, 1U, 50000U);
show_attr(stall_floor);
store_attr(stall_floor, 1U, 100U);
show_attr(min_freq);
show_attr(max_freq);
show_attr(sample_ms);
store_attr(sample_ms, 1U, 50U);
show_attr(l2wb_pct);
store_attr(l2wb_pct, 1U, 100U);
show_attr(l2wb_filter);
store_attr(l2wb_filter, 1U, 50000U);

static ssize_t store_min_freq(struct kobject *kobj,
			const char *buf,
			size_t count)
{
	int ret;
	unsigned int val;
	struct memlat_mon *mon = to_memlat_mon(kobj);
	struct memlat_cpu_grp *cpu_grp = mon->cpu_grp;
	struct scmi_memlat_vendor_ops *ops = cpu_grp->handle->memlat_ops;
	unsigned int min_freq;
	unsigned int max_freq;

	if (mon->mon_type == L3_MEMLAT) {
		min_freq = l3_freqs[0];
		max_freq = l3_freqs[l3_pstates];
	} else {
		return -EINVAL; /* unsupported node */
	}

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;
	mutex_lock(&cpu_grp->mons_lock);
	val = max(val, min_freq);
	val = min(val, max_freq);
	val = min(val, mon->max_freq);
	mon->min_freq = val;
	if (mon->mon_started)
		ret = ops->min_freq(cpu_grp->handle, mon->cpus_mpidr,
					mon->mon_type, mon->min_freq);
	if (ret < 0) {
		pr_err("failed to set mon tunable %s, ret = %d\n",
						"name", ret);
		count = 0;
	}
	mutex_unlock(&cpu_grp->mons_lock);
	return count;
}

static ssize_t store_max_freq(struct kobject *kobj,
			const char *buf,
			size_t count)
{
	int ret;
	unsigned int val;
	struct memlat_mon *mon = to_memlat_mon(kobj);
	struct memlat_cpu_grp *cpu_grp = mon->cpu_grp;
	struct scmi_memlat_vendor_ops *ops = cpu_grp->handle->memlat_ops;
	unsigned int min_freq;
	unsigned int max_freq;

	if (mon->mon_type == L3_MEMLAT) {
		min_freq = l3_freqs[0];
		max_freq = l3_freqs[l3_pstates];
	} else {
		return -EINVAL; /* unsupported node */
	}

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;
	mutex_lock(&cpu_grp->mons_lock);
	val = max(val, min_freq);
	val = min(val, max_freq);
	val = max(val, mon->min_freq);
	mon->max_freq = val;
	if (mon->mon_started)
		ret = ops->max_freq(cpu_grp->handle, mon->cpus_mpidr,
				mon->mon_type, mon->max_freq);
	if (ret < 0) {
		pr_err("failed to set max_freq tunable %s, ret = %d\n", ret);
		count = 0;
	}

	mutex_unlock(&cpu_grp->mons_lock);
	return count;
}

static ssize_t show_cur_freq(struct kobject *kobj, char *buf)
{
	struct memlat_mon *mon = to_memlat_mon(kobj);
	u32 count = 0, index = 0;

	if (mon->mon_type == L3_MEMLAT) {
		index = readl_relaxed(mon->perf_base);
		index = index & PERF_STATE_DESIRED_MASK;
		index = min(index, l3_pstates);
		count = scnprintf(buf, PAGE_SIZE, "%u\n", l3_freqs[index]);
	}
	return count;
}

static ssize_t show_available_freq(struct kobject *kobj, char *buf)
{
	struct memlat_mon *mon = to_memlat_mon(kobj);
	u32 i = 0;
	char *tmp = buf;

	if (mon->mon_type == L3_MEMLAT) {
		for (i = 0; i <= l3_pstates; i++)
			tmp += scnprintf(tmp, PAGE_SIZE, "%u\n", l3_freqs[i]);
	}
	return (tmp - buf);
}

static ssize_t show_log_level(struct kobject *kobj, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", rimps_log_level);
}

static ssize_t store_log_level(struct kobject *kobj,
			const char *buf,
			size_t count)
{
	struct memlat_cpu_grp *cpu_grp;
	struct scmi_memlat_vendor_ops *ops = NULL;
	int ret = 0, val, cpu;

	for_each_possible_cpu(cpu) {
		cpu_grp = per_cpu(per_cpu_grp, cpu);
		if (cpu_grp && cpu_grp->handle) {
			ops = cpu_grp->handle->memlat_ops;
			break;
		}
	}

	if (!ops)
		return 0;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	val = max(val, MIN_LOG_LEVEL);
	val = min(val, MAX_LOG_LEVEL);
	ret = ops->set_log_level(cpu_grp->handle, val);
	if (ret < 0) {
		pr_err("failed to set log level ret = %d\n", ret);
		return 0;
	}
	rimps_log_level = val;
	return count;
}

static ssize_t memlat_show(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	struct memlat_attr *mem_attr = to_memlat_attr(attr);
	ssize_t ret;

	if (!mem_attr->show)
		return -EIO;

	ret = mem_attr->show(kobj, buf);

	return ret;
}

static ssize_t memlat_store(struct kobject *kobj,
				struct attribute *attr,
				const char *buf, size_t count)
{
	struct memlat_attr *mem_attr = to_memlat_attr(attr);
	ssize_t ret = -EINVAL;

	if (!mem_attr->store)
		return -EIO;

	ret = mem_attr->store(kobj, buf, count);

	return ret;
}

memlat_mon_attr_rw(ratio_ceil);
memlat_mon_attr_rw(stall_floor);
memlat_mon_attr_rw(min_freq);
memlat_mon_attr_rw(max_freq);
memlat_mon_attr_rw(sample_ms);
memlat_mon_attr_rw(l2wb_pct);
memlat_mon_attr_rw(l2wb_filter);
memlat_mon_attr_rw(log_level);
memlat_mon_attr_ro(cur_freq);
memlat_mon_attr_ro(available_freq);

static struct attribute *mon_dev_attr[] = {
	&ratio_ceil.attr,
	&stall_floor.attr,
	&min_freq.attr,
	&max_freq.attr,
	&sample_ms.attr,
	&l2wb_pct.attr,
	&l2wb_filter.attr,
	&cur_freq.attr,
	&available_freq.attr,
	NULL,
};

static struct attribute *log_level_dev_attr[] = {
	&log_level.attr,
	NULL,
};

static struct perf_event_attr *alloc_attr(void)
{
	struct perf_event_attr *attr;

	attr = kzalloc(sizeof(struct perf_event_attr), GFP_KERNEL);
	if (!attr)
		return attr;

	attr->type = PERF_TYPE_RAW;
	attr->size = sizeof(struct perf_event_attr);
	attr->pinned = 1;

	return attr;
}

static struct perf_event *set_event(int event_id, unsigned int cpu,
				struct perf_event_attr *attr)
{
	struct perf_event *pevent;

	if (!event_id)
		return NULL;

	attr->config = event_id;
	pevent = perf_event_create_kernel_counter(attr, cpu, NULL, NULL, NULL);
	if (IS_ERR(pevent)) {
		pr_err("failed to set pmu event = %d, cpu = %d, ret = %d\n",
				event_id, cpu, PTR_ERR(pevent));

		return pevent;
	}

	perf_event_enable(pevent);

	return pevent;
}

static int setup_common_pmu_events(struct memlat_cpu_grp *cpu_grp,
				cpumask_t *mask)
{
	struct perf_event_attr *attr = alloc_attr();
	struct perf_event *pevent;
	unsigned int cpu;

	for_each_cpu(cpu, mask) {
		struct pmu_map *pmu = to_common_pmu_map(cpu_grp, cpu);
		struct cpu_data *cpus_data = to_cpu_data(cpu_grp, cpu);

		if (per_cpu(cpu_is_hp, cpu))
			continue;

		pevent = set_event(cpu_grp->common_ev_ids[INST_IDX],
						cpu, attr);
		if (IS_ERR(pevent))
			return -ENODEV;

		cpus_data->common_evs[INST_IDX] = pevent;
		pmu[INST_IDX].event_id = cpu_grp->common_ev_ids[INST_IDX];
		pmu[INST_IDX].hw_cntr_idx = pevent->hw.idx + 1;

		pevent = set_event(cpu_grp->common_ev_ids[CYC_IDX], cpu, attr);
		if (IS_ERR(pevent)) {
			perf_event_release_kernel(
					cpus_data->common_evs[INST_IDX]);
			return -ENODEV;
		}

		cpus_data->common_evs[CYC_IDX] = pevent;
		pmu[CYC_IDX].event_id = cpu_grp->common_ev_ids[CYC_IDX];
		pmu[CYC_IDX].hw_cntr_idx = pevent->hw.idx;

		if (cpu_grp->common_ev_ids[STALL_IDX] != INVALID_PMU_EVENT_ID) {
			pevent = set_event(cpu_grp->common_ev_ids[STALL_IDX], cpu, attr);
			if (IS_ERR(pevent)) {
				perf_event_release_kernel(
					cpus_data->common_evs[INST_IDX]);
				perf_event_release_kernel(
					cpus_data->common_evs[CYC_IDX]);
				return -ENODEV;
			}
			cpus_data->common_evs[STALL_IDX] = pevent;
			pmu[STALL_IDX].event_id = cpu_grp->common_ev_ids[STALL_IDX];
			pmu[STALL_IDX].hw_cntr_idx = pevent->hw.idx + 1;
		} else {
			cpus_data->common_evs[STALL_IDX] = NULL;
			pmu[STALL_IDX].event_id = INVALID_PMU_EVENT_ID;
			pmu[STALL_IDX].hw_cntr_idx = INVALID_PMU_HW_IDX;
		}
	}
	return 0;
}

static int setup_mon_pmu_events(struct memlat_mon *mon, cpumask_t *mask)
{
	struct perf_event_attr *attr = alloc_attr();
	struct perf_event *pevent;
	unsigned int cpu;

	for_each_cpu(cpu, mask) {
		struct pmu_map *pmu = to_mon_pmu_map(mon, cpu);
		struct mon_data *ev_data = to_mon_ev_data(mon, cpu);

		if (per_cpu(cpu_is_hp, cpu))
			continue;

		pevent = set_event(mon->mon_ev_ids[MISS_IDX], cpu, attr);
		if (IS_ERR(pevent))
			return -ENODEV;

		ev_data->mon_evs[MISS_IDX] = pevent;
		pmu[MISS_IDX].event_id = mon->mon_ev_ids[MISS_IDX];
		pmu[MISS_IDX].hw_cntr_idx = pevent->hw.idx + 1;

		if (mon->mon_ev_ids[L2WB_IDX] != INVALID_PMU_EVENT_ID) {
			pevent = set_event(mon->mon_ev_ids[L2WB_IDX],
						cpu, attr);
			if (IS_ERR(pevent)) {
				perf_event_release_kernel(
					ev_data->mon_evs[MISS_IDX]);
				return -ENODEV;
			}
			ev_data->mon_evs[L2WB_IDX] = pevent;
			pmu[L2WB_IDX].event_id = mon->mon_ev_ids[L2WB_IDX];
			pmu[L2WB_IDX].hw_cntr_idx = pevent->hw.idx + 1;
		} else {
			ev_data->mon_evs[L2WB_IDX] = NULL;
			pmu[L2WB_IDX].event_id = INVALID_PMU_EVENT_ID;
			pmu[L2WB_IDX].hw_cntr_idx = INVALID_PMU_HW_IDX;
		}

		if (mon->mon_ev_ids[L3_ACCESS_IDX] != INVALID_PMU_EVENT_ID) {

			pevent = set_event(mon->mon_ev_ids[L3_ACCESS_IDX],
						cpu, attr);
			if (IS_ERR(pevent)) {
				perf_event_release_kernel(
					ev_data->mon_evs[MISS_IDX]);
				if (ev_data->mon_evs[L2WB_IDX])
					perf_event_release_kernel(
						ev_data->mon_evs[L2WB_IDX]);
				return -ENODEV;
			}
			ev_data->mon_evs[L3_ACCESS_IDX] = pevent;
			pmu[L3_ACCESS_IDX].event_id =
						mon->mon_ev_ids[L3_ACCESS_IDX];
			pmu[L3_ACCESS_IDX].hw_cntr_idx = pevent->hw.idx + 1;
		} else {
			ev_data->mon_evs[L3_ACCESS_IDX] = NULL;
			pmu[L3_ACCESS_IDX].event_id = INVALID_PMU_EVENT_ID;
			pmu[L3_ACCESS_IDX].hw_cntr_idx = INVALID_PMU_HW_IDX;
		}
	}
	return 0;
}

static inline void store_event_val(u64 val, u8 idx, u8 cpu)
{
	struct cpu_pmu_ctrs *base = pmu_base
					+ (sizeof(struct cpu_pmu_ctrs) * cpu);

	if (idx == 0) {
		writel_relaxed((val & (0xFFFFFFFF)), &base->ccntr_lo);
		writel_relaxed(((val >> 32) & (0xFFFFFFFF)), &base->ccntr_hi);
	} else if (idx < MAX_PMU_CNTRS_RIMPS) {
		writel_relaxed((val & (0xFFFFFFFF)), &base->evcntr[idx - 2]);
	}
}

static inline void set_pmu_cache_flag(u32 val, u8 cpu)
{
	struct cpu_pmu_ctrs *base = pmu_base +
					(sizeof(struct cpu_pmu_ctrs) * cpu);

	writel_relaxed(val, &base->valid);
}

static void save_cpugrp_pmu_events(struct memlat_cpu_grp *cpu_grp, u8 cpu)
{
	u8 i = 0;
	u64 ev_count;
	struct pmu_map *pmu = to_common_pmu_map(cpu_grp, cpu);
	struct cpu_data *cpus_data = to_cpu_data(cpu_grp, cpu);

	for (i = 0; i < NUM_COMMON_EVS; i++) {
		u8 hw_id = pmu[i].hw_cntr_idx;

		if (hw_id == INVALID_PMU_HW_IDX)
			continue;
		perf_event_read_local(cpus_data->common_evs[i],
				&ev_count, NULL, NULL);
		store_event_val(ev_count, hw_id, cpu);
	}
}

static void save_mon_pmu_events(struct memlat_mon *mon, u8 cpu)
{

	u64 ev_count;
	struct mon_data *ev_data = to_mon_ev_data(mon, cpu);
	struct pmu_map *pmu = to_mon_pmu_map(mon, cpu);
	u8 i = 0;

	for (i = 0; i < NUM_MON_EVS; i++) {
		u8 hw_id = pmu[i].hw_cntr_idx;

		if (hw_id == INVALID_PMU_HW_IDX)
			continue;
		perf_event_read_local(ev_data->mon_evs[i],
				&ev_count, NULL, NULL);
		store_event_val(ev_count, hw_id, cpu);
	}
}

static void free_common_evs(struct memlat_cpu_grp *cpu_grp, cpumask_t *mask)
{
	unsigned int cpu, i;

	for_each_cpu(cpu, mask) {
		struct cpu_data *cpus_data = to_cpu_data(cpu_grp, cpu);

		for (i = 0; i < NUM_COMMON_EVS; i++) {
			if (!cpus_data->common_evs[i])
				continue;
			perf_event_release_kernel(cpus_data->common_evs[i]);
		}
	}
}

static void free_mon_evs(struct memlat_mon *mon, cpumask_t *mask)
{
	unsigned int cpu, i;

	for_each_cpu(cpu, mask) {
		struct mon_data *ev_data = to_mon_ev_data(mon, cpu);

		for (i = 0; i < NUM_MON_EVS; i++) {
			if (!ev_data->mon_evs[i])
				continue;

			perf_event_release_kernel(ev_data->mon_evs[i]);
		}
	}
}

static int memlat_hp_restart_events(unsigned int cpu, bool cpu_up)
{
	struct perf_event_attr *attr = alloc_attr();
	struct memlat_cpu_grp *cpu_grp = per_cpu(per_cpu_grp, cpu);
	struct scmi_memlat_vendor_ops *ops;
	struct memlat_mon *mon;
	int ret = 0;
	unsigned int i = 0;
	cpumask_t mask = CPU_MASK_NONE;

	if (!attr)
		return -ENOMEM;

	if (!cpu_grp)
		goto exit;

	ops = cpu_grp->handle->memlat_ops;

	cpumask_set_cpu(cpu, &mask);

	if (cpu_up) {
		ret = setup_common_pmu_events(cpu_grp, &mask);
		if (ret < 0) {
			pr_err("failed to setup common PMU cpu = %d ret %d\n",
					cpu, ret);
			goto exit;
		}
		ret = ops->common_pmu_map(cpu_grp->handle,
						cpu_grp->cpus_mpidr,
						MEMLAT_CPU_GRP,
						cpumask_weight(&cpu_grp->cpus)
						* NUM_COMMON_EVS,
						cpu_grp->common_ev_map);
		if (ret < 0) {
			pr_err("failed common_pmu_map cpu = %d\n", cpu);
			return ret;
		}
	} else {
		save_cpugrp_pmu_events(cpu_grp, cpu);
		free_common_evs(cpu_grp, &mask);
	}

	for (i = 0; i < cpu_grp->num_mons; i++) {
		mon = &cpu_grp->mons[i];

		if (!cpumask_test_cpu(cpu, &mon->cpus))
			continue;
		if (cpu_up) {
			ret = setup_mon_pmu_events(mon, &mask);
			if (ret < 0) {
				pr_err("mon events failed on cpu %d ret %d\n",
						cpu, ret);
				goto exit;
			}
			ret = ops->mon_pmu_map(cpu_grp->handle,
						mon->cpus_mpidr,
						mon->mon_type,
						cpumask_weight(&mon->cpus)
						* NUM_MON_EVS,
						mon->mon_ev_map);
			if (ret < 0) {
				pr_err("failed mon_pmu_map cpu = %d\n", cpu);
				return ret;
			}
		} else {
			save_mon_pmu_events(mon, cpu);
			free_mon_evs(mon, &mask);
		}
	}
	set_pmu_cache_flag(!cpu_up, cpu);
exit:
	kfree(attr);
	return ret;
}

static int memlat_event_hotplug_coming_up(unsigned int cpu)
{
	per_cpu(cpu_is_hp, cpu) = false;
	return memlat_hp_restart_events(cpu, true);
}

static int memlat_event_hotplug_going_down(unsigned int cpu)
{
	per_cpu(cpu_is_hp, cpu) = true;
	return memlat_hp_restart_events(cpu, false);
}

static int memlat_event_cpu_hp_init(void)
{
	int ret = 0;

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
				"MEMLAT_EVENT",
				memlat_event_hotplug_coming_up,
				memlat_event_hotplug_going_down);
	if (ret < 0)
		pr_err("memlat: failed to register CPU hotplug notifier: %d\n",
									ret);
	else
		ret = 0;
	return ret;
}

static int memlat_idle_notif(struct notifier_block *nb,
					unsigned long action,
					void *data)
{
	int ret = NOTIFY_OK;
	int cpu = smp_processor_id();
	struct memlat_cpu_grp *cpu_grp = per_cpu(per_cpu_grp, cpu);
	struct memlat_mon *mon;
	unsigned int i = 0;

	if (!cpu_grp)
		return NOTIFY_OK;

	switch (action) {
	case CPU_PM_ENTER:
		__this_cpu_write(cpu_is_idle, true);
		if (per_cpu(cpu_is_hp, cpu)) {
			goto idle_exit;
		} else {
			save_cpugrp_pmu_events(cpu_grp, cpu);
			for (i = 0; i < cpu_grp->num_mons; i++) {
				mon = &cpu_grp->mons[i];
				save_mon_pmu_events(mon, cpu);
			}
			set_pmu_cache_flag(true, cpu);
		}
		break;
	case CPU_PM_EXIT:
		__this_cpu_write(cpu_is_idle, false);
		set_pmu_cache_flag(false, cpu);
		break;
	}
idle_exit:
	return ret;
}

static struct notifier_block memlat_event_idle_nb = {
	.notifier_call = memlat_idle_notif,
};

static int get_mask_from_dev_handle(struct platform_device *pdev,
				cpumask_t *mask, u32 *cpus_mpidr)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_phandle;
	struct device *cpu_dev;
	int cpu, i = 0, physical_cpu;
	int ret = -ENOENT;

	dev_phandle = of_parse_phandle(dev->of_node, "qcom,cpulist", i++);
	while (dev_phandle) {
		for_each_possible_cpu(cpu) {
			cpu_dev = get_cpu_device(cpu);
			if (cpu_dev && cpu_dev->of_node == dev_phandle) {
				cpumask_set_cpu(cpu, mask);
				physical_cpu = MPIDR_AFFINITY_LEVEL(
							cpu_logical_map(cpu),
							 1);
				*cpus_mpidr |= (0x1 << physical_cpu);
				ret = 0;
				break;
			}
		}
		dev_phandle = of_parse_phandle(dev->of_node,
						"qcom,cpulist", i++);
	}
	return ret;
}

#define NUM_COLS	2
#define INIT_HZ			300000000UL
#define XO_HZ			19200000UL
#define FTBL_ROW_SIZE		4
#define SRC_MASK		GENMASK(31, 30)
#define SRC_SHIFT		30
#define MULT_MASK		GENMASK(7, 0)

static struct core_dev_map *init_core_dev_map(struct device *dev, u32 *cnt)
{
	int len, nf, i, j;
	u32 data;
	struct core_dev_map *tbl;
	int ret;

	if (!of_find_property(dev->of_node, "qcom,core-dev-table", &len)) {
		dev_err(dev, "failed to read property qcom,core-dev-table\n");
		return NULL;
	}
	len /= sizeof(data);

	if (len % NUM_COLS || len == 0)
		return NULL;
	nf = len / NUM_COLS;

	tbl = devm_kzalloc(dev, (nf + 1) * sizeof(struct core_dev_map),
			GFP_KERNEL);
	if (!tbl)
		return NULL;

	for (i = 0, j = 0; i < nf; i++, j += 2) {
		ret = of_property_read_u32_index(dev->of_node,
					"qcom,core-dev-table", j,
					&data);
		if (ret < 0)
			return NULL;
		tbl[i].core_mhz = data / 1000;

		ret = of_property_read_u32_index(dev->of_node,
					"qcom,core-dev-table", j + 1,
					&data);
		if (ret < 0)
			return NULL;
		tbl[i].target_freq = data;
		pr_debug("Entry%d CPU:%u, Dev:%u\n", i, tbl[i].core_mhz,
				tbl[i].target_freq);
	}
	*cnt = i;
	tbl[i].core_mhz = 0;

	return tbl;
}

static int populate_opp_table(struct device *dev)
{
	int idx, ret;
	u32 data, src, mult, i;
	unsigned long freq, prev_freq = 0;
	struct resource res;
	void __iomem *ftbl_base;
	unsigned int ftbl_row_size = FTBL_ROW_SIZE;

	idx = of_property_match_string(dev->of_node, "reg-names", "ftbl-base");
	if (idx < 0) {
		dev_err(dev, "Unable to find ftbl-base: %d\n", idx);
		return -EINVAL;
	}

	ret = of_address_to_resource(dev->of_node, idx, &res);
	if (ret < 0) {
		dev_err(dev, "Unable to get resource from address: %d\n", ret);
		return -EINVAL;
	}

	ftbl_base = devm_ioremap(dev, res.start, resource_size(&res));
	if (!ftbl_base) {
		dev_err(dev, "Unable to map ftbl-base!\n");
		return -ENOMEM;
	}

	of_property_read_u32(dev->of_node, "qcom,ftbl-row-size",
						&ftbl_row_size);

	for (i = 0; i < MAX_L3_ENTRIES; i++) {
		data = readl_relaxed(ftbl_base + i * ftbl_row_size);
		src = ((data & SRC_MASK) >> SRC_SHIFT);
		mult = (data & MULT_MASK);
		freq = src ? XO_HZ * mult : INIT_HZ;

		/* Two of the same frequencies means end of table */
		if (i > 0 && prev_freq == freq)
			break;

		l3_freqs[i] = freq;
		prev_freq = freq;
	}
	l3_pstates = i - 1;
	use_cached_l3_freqs = true;

	return 0;
}

static int configure_rimps(struct memlat_cpu_grp *cpu_grp)
{
	struct scmi_memlat_vendor_ops *ops = cpu_grp->handle->memlat_ops;
	int num_cpus = cpumask_weight(&cpu_grp->cpus);
	int i = 0;
	int ret = 0;

	mutex_lock(&cpu_grp->mons_lock);
	ret = ops->set_cpu_grp(cpu_grp->handle, cpu_grp->cpus_mpidr, MEMLAT_CPU_GRP);
	if (ret < 0) {
		dev_err(cpu_grp->dev, "failed to configure cpu_grp\n");
		goto out;
	}

	ret = ops->common_pmu_map(cpu_grp->handle, cpu_grp->cpus_mpidr,
				MEMLAT_CPU_GRP, num_cpus * NUM_COMMON_EVS,
				cpu_grp->common_ev_map);
	if (ret < 0) {
		dev_err(cpu_grp->dev, "failed to configure common_pmu_map\n");
		goto out;
	}

	for (i = 0; i < cpu_grp->num_mons; i++) {
		struct memlat_mon *mon = &cpu_grp->mons[i];

		ret = ops->set_mon(cpu_grp->handle, mon->cpus_mpidr,
					mon->mon_type);
		if (ret < 0) {
			pr_err("%s: failed to configure monitor\n", mon->mon_name);
			goto out;
		}

		ret = ops->mon_pmu_map(cpu_grp->handle, mon->cpus_mpidr,
					mon->mon_type,
					cpumask_weight(&mon->cpus)
					* NUM_MON_EVS,
					mon->mon_ev_map);
		if (ret < 0) {
			pr_err("%s: failed to configure mon_pmu_map\n",
					mon->mon_name);
			goto out;
		}

		ret = ops->ratio_ceil(cpu_grp->handle, mon->cpus_mpidr,
					mon->mon_type, mon->ratio_ceil);
		if (ret < 0) {
			pr_err("%s: failed to configure ceil ratio\n",
					mon->mon_name);
			goto out;
		}

		ret = ops->stall_floor(cpu_grp->handle, mon->cpus_mpidr,
					mon->mon_type, mon->stall_floor);
		if (ret < 0) {
			pr_err("%s: failed to configure stall floor\n",
					mon->mon_name);
			goto out;
		}

		ret = ops->sample_ms(cpu_grp->handle, mon->cpus_mpidr,
					mon->mon_type, mon->sample_ms);
		if (ret < 0) {
			pr_err("%s: failed to configure sample_ms\n",
					mon->mon_name);
			goto out;
		}

		if (mon->mon_type == L3_MEMLAT) {
			ret = ops->l2wb_pct(cpu_grp->handle, mon->cpus_mpidr,
					mon->mon_type, mon->l2wb_pct);
			if (ret < 0) {
				pr_err("%s: failed to configure l2wb pct\n",
					mon->mon_name);
				goto out;
			}
			ret = ops->l2wb_filter(cpu_grp->handle,
					mon->cpus_mpidr,
					mon->mon_type, mon->l2wb_filter);
			if (ret < 0) {
				pr_err("%s: failed to configure l2wb_filter\n",
					mon->mon_name);
				goto out;
			}
		}

		ret = ops->freq_map(cpu_grp->handle, mon->cpus_mpidr,
					mon->mon_type,
					mon->num_freq_map_entries,
					mon->freq_map);
		if (ret < 0) {
			pr_err("%s: failed to configure freq_map\n",
					mon->mon_name);
			goto out;
		}

		ret = ops->min_freq(cpu_grp->handle, mon->cpus_mpidr,
					mon->mon_type, mon->min_freq);
		if (ret < 0) {
			pr_err("%s: failed to configure min_freq\n",
					mon->mon_name);
			goto out;
		}

		ret = ops->max_freq(cpu_grp->handle, mon->cpus_mpidr,
					mon->mon_type, mon->max_freq);
		if (ret < 0) {
			pr_err("%s: failed to configure max_freq\n",
					mon->mon_name);
			goto out;
		}

		ret = ops->start_monitor(cpu_grp->handle,
				mon->cpus_mpidr, mon->mon_type);
		if (ret < 0) {
			pr_err("%s: failed to start monitor\n",
				mon->mon_name);
			goto out;
		}
		mon->mon_started = true;
	}
out:
	mutex_unlock(&cpu_grp->mons_lock);
	return ret;
}

static void mon_sysfs_release(struct kobject *kobj)
{
	pr_debug("Clearing RIMS sysfs entry\n");
}


static const struct sysfs_ops sysfs_ops = {
	.show	= memlat_show,
	.store	= memlat_store,
};

static struct kobj_type ktype_mon = {
	.sysfs_ops	= &sysfs_ops,
	.default_attrs	= mon_dev_attr,
	.release	= mon_sysfs_release,
};

static struct kobj_type ktype_log_level = {
	.sysfs_ops	= &sysfs_ops,
	.default_attrs	= log_level_dev_attr,
	.release	= mon_sysfs_release,
};

static struct kobj_type ktype_cpugrp = {
	.sysfs_ops	= &sysfs_ops,
	.release	= mon_sysfs_release,
};

void rimps_memlat_init(struct scmi_handle *handle)
{
	struct memlat_cpu_grp *cpu_grp;
	unsigned int cpu;
	int ret = 0;

	get_online_cpus();
	for_each_possible_cpu(cpu) {
		cpu_grp = per_cpu(per_cpu_grp, cpu);
		if (!cpu_grp || (cpu != cpumask_first(&cpu_grp->cpus)))
			continue;

		cpu_grp->handle = handle;
		ret = configure_rimps(cpu_grp);
		if (ret < 0)
			pr_err("failed to configure RIMPS ret = %d\n", ret);
	}
	put_online_cpus();
}
EXPORT_SYMBOL(rimps_memlat_init);

static int memlat_cpu_grp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct memlat_cpu_grp *cpu_grp;
	int ret = 0, idx = 0;
	unsigned int event_id, num_cpus, num_mons, cpu;
	struct resource res;

	cpu_grp = devm_kzalloc(dev, sizeof(*cpu_grp), GFP_KERNEL);
	if (!cpu_grp)
		return -ENOMEM;

	cpu_grp->dev = dev;
	if (!pmu_base) {
		idx = of_property_match_string(dev->of_node,
					"reg-names", "pmu-base");
		if (idx < 0) {
			dev_err(dev, "Unable to find pmu-base: %d\n", idx);
			return -EINVAL;
		}

		ret = of_address_to_resource(dev->of_node, idx, &res);
		if (ret < 0) {
			dev_err(dev, "failed to get resource %d\n", ret);
			return -EINVAL;
		}

		pmu_base = devm_ioremap(dev, res.start,
						resource_size(&res));
		if (!pmu_base) {
			dev_err(dev, "Unable to map pmu-base!\n");
			return -ENOMEM;
		}
	}

	if (get_mask_from_dev_handle(pdev,
				&cpu_grp->cpus, &cpu_grp->cpus_mpidr)) {
		dev_err(dev, "No CPUs specified.\n");
		return -ENODEV;
	}

	num_cpus = cpumask_weight(&cpu_grp->cpus);
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
	if (!cpu_grp->mons) {
		dev_err(dev, "failed to allocate the memory for mons\n");
		return -ENOMEM;
	}

	cpu_grp->common_ev_map = devm_kzalloc(dev,
					num_cpus *  NUM_COMMON_EVS *
					sizeof(*cpu_grp->common_ev_map),
					GFP_KERNEL);
	if (!cpu_grp->common_ev_map) {
		dev_err(dev, "failed to allocate memory for common_ev_map\n");
		return -ENOMEM;
	}

	cpu_grp->cpus_data = devm_kzalloc(dev,
				num_cpus * sizeof(*cpu_grp->cpus_data),
				GFP_KERNEL);
	if (!cpu_grp->cpus_data) {
		dev_err(dev, "failed to allocate the memory for ev_data\n");
		return -ENOMEM;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,inst-ev", &event_id);
	if (ret < 0) {
		dev_dbg(dev, "Inst event not specified. Using def:0x%x\n",
			INST_EV);
		event_id = INST_EV;
	}
	cpu_grp->common_ev_ids[INST_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,cyc-ev", &event_id);
	if (ret < 0) {
		dev_dbg(dev, "Cyc event not specified. Using def:0x%x\n",
			CYC_EV);
		event_id = CYC_EV;
	}
	cpu_grp->common_ev_ids[CYC_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,stall-ev", &event_id);
	if (ret < 0) {
		dev_dbg(dev, "Stall event not specified. Skipping.\n");
		cpu_grp->common_ev_ids[STALL_IDX] = INVALID_PMU_EVENT_ID;
	} else
		cpu_grp->common_ev_ids[STALL_IDX] = event_id;

	for_each_cpu(cpu, &cpu_grp->cpus)
		per_cpu(per_cpu_grp, cpu) = cpu_grp;
	dev_set_drvdata(dev, cpu_grp);
	mutex_init(&cpu_grp->mons_lock);

	for_each_possible_cpu(cpu) {
		if (!cpumask_test_cpu(cpu, cpu_online_mask))
			per_cpu(cpu_is_hp, cpu) = true;
	}

	if (!lpm_hp_notifier_registered) {
		get_online_cpus();
		ret = memlat_event_cpu_hp_init();
		if (ret < 0) {
			dev_err(dev, "failed to register to hp\n");
			return ret;
		}
		cpu_pm_register_notifier(&memlat_event_idle_nb);
		lpm_hp_notifier_registered = true;
		put_online_cpus();
	}

	ret = setup_common_pmu_events(cpu_grp, &cpu_grp->cpus);
	if (ret < 0) {
		dev_err(dev, "failed to setup common evs = %d\n", ret);
		return ret;
	}

	if (!rimps_kobj) {
		rimps_kobj = kobject_create();
		if (!rimps_kobj)
			dev_err(dev, "%s: failed to create rimps_kobj\n",
						__func__);

		ret = kobject_init_and_add(rimps_kobj, &ktype_log_level,
				   &cpu_subsys.dev_root->kobj, "memlat");
		if (ret) {
			dev_err(dev, "%s: failed to init cpugrp->kobj: %d\n",
						__func__, ret);
			/*
			 * The entire policy object will be freed below,
			 * but the extra memory allocated for the kobject
			 * name needs to be freed by releasing the kobject.
			 */
			kobject_put(rimps_kobj);
			return -ENODEV;
		}
	}
	ret = kobject_init_and_add(&cpu_grp->kobj, &ktype_cpugrp,
					rimps_kobj, "c%u_memlat",
					cpumask_first(&cpu_grp->cpus));
	if (ret) {
		dev_err(dev, "%s: failed to init cpugrp->kobj: %d\n",
					__func__, ret);
		kobject_put(&cpu_grp->kobj);
		return -ENODEV;
	}

	return 0;
}

static int memlat_mon_probe(struct platform_device *pdev, u32 mon_type)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct memlat_cpu_grp *cpu_grp;
	struct memlat_mon *mon;
	unsigned int event_id, num_cpus;
	struct resource *res;

	cpu_grp = dev_get_drvdata(dev->parent);
	if (!cpu_grp) {
		dev_err(dev, "Mon initialized without cpu_grp.\n");
		return -ENODEV;
	}
	mon = &cpu_grp->mons[cpu_grp->num_inited_mons];
	mon->cpu_grp = cpu_grp;
	mon->mon_type = mon_type;
	if (get_mask_from_dev_handle(pdev, &mon->cpus, &mon->cpus_mpidr)) {
		cpumask_copy(&mon->cpus, &cpu_grp->cpus);
		mon->cpus_mpidr = cpu_grp->cpus_mpidr;
	} else {
		if (!cpumask_subset(&mon->cpus, &cpu_grp->cpus)) {
			dev_err(dev,
				"Mon CPUs - subset of cpu_grp CPUs. mon=%*pbl cpu_grp=%*pbl\n",
				cpumask_pr_args(&mon->cpus),
				cpumask_pr_args(&cpu_grp->cpus));
			return -EINVAL;
		}
	}

	num_cpus = cpumask_weight(&mon->cpus);
	mon->ev_data =
		devm_kzalloc(dev, num_cpus * sizeof(*mon->ev_data),
				     GFP_KERNEL);
	if (!mon->ev_data) {
		dev_err(dev, "failed to allocate memory for pevents\n");
		return -ENOMEM;
	}

	mon->mon_ev_map = devm_kzalloc(dev,
				num_cpus * sizeof(*mon->mon_ev_map) * NUM_MON_EVS,
				GFP_KERNEL);
	if (!mon->mon_ev_map) {
		dev_err(dev, "failed to allocate memory for event map\n");
		return -ENOMEM;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,cachemiss-ev",
					&event_id);
	if (ret < 0) {
		dev_err(dev, "Cache miss event missing for mon: %d\n",
				ret);
		return -EINVAL;
	}
	mon->mon_ev_ids[MISS_IDX] = event_id;
	ret = of_property_read_u32(dev->of_node, "qcom,wb-ev",
					&event_id);
	if (ret < 0) {
		dev_err(dev, "l2wb event missing for mon: %d\n",
				ret);
		mon->mon_ev_ids[L2WB_IDX] = INVALID_PMU_EVENT_ID;
	} else {
		mon->mon_ev_ids[L2WB_IDX] = event_id;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,access-ev",
					&event_id);
	if (ret < 0) {
		dev_err(dev, "l2wb event missing for mon: %d\n",
				ret);
		mon->mon_ev_ids[L3_ACCESS_IDX] = INVALID_PMU_EVENT_ID;
	} else {
		mon->mon_ev_ids[L3_ACCESS_IDX] = event_id;
	}
	setup_mon_pmu_events(mon, &mon->cpus);
	if (mon->mon_type == L3_MEMLAT) {
		if (!use_cached_l3_freqs) {
			ret = populate_opp_table(dev);
			if (ret < 0) {
				dev_err(dev,
					"failed to read the OPP ret = %d\n",
					 ret);
				return ret;
			}
		}

		res = platform_get_resource_byname(pdev,
						IORESOURCE_MEM,
						"perf-base");
		if (!res) {
			dev_err(dev, "Unable to find perf-base!\n");
			return -EINVAL;
		}

		mon->perf_base = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
		if (IS_ERR(mon->perf_base)) {
			dev_err(dev, "Unable to map perf-base\n");
			return -ENOMEM;
		}

		mon->max_freq = l3_freqs[l3_pstates];
		mon->min_freq = l3_freqs[0];
		/* Initialize L3 to max until RIMPS takes over */
		writel_relaxed(l3_pstates, mon->perf_base);
		scnprintf(mon->mon_name, MAX_NAME_LEN,
				"cpu%d-cpu-l3-lat",
				cpumask_first(&mon->cpus));
	}
	mon->ratio_ceil = DEFAULT_RATIO_CEIL;
	mon->stall_floor = DEFAULT_STALL_FLOOR;
	mon->sample_ms = DEFAULT_SAMPLE_MS;
	mon->l2wb_pct = DEFAULT_L2WB_PCT;
	mon->l2wb_filter = DEFAULT_L2WB_IPM_FILTER;
	mon->freq_map = init_core_dev_map(dev, &mon->num_freq_map_entries);
	if (!mon->freq_map) {
		dev_err(dev, "failed to setup freq map\n");
		return -ENODEV;
	}

	ret = kobject_init_and_add(&mon->kobj, &ktype_mon,
			   &cpu_grp->kobj, "%s", mon->mon_name);
	if (ret < 0)
		return ret;

	cpu_grp->num_inited_mons++;
	return ret;

}

static int rimps_memlat_mon_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	const struct memlat_mon_spec *spec = of_device_get_match_data(dev);
	enum mon_type type = MAX_MEMLAT_DEVICE_TYPE;

	if (spec)
		type = spec->type;

	switch (type) {
	case MEMLAT_CPU_GRP:
		ret = memlat_cpu_grp_probe(pdev);
		if (of_get_available_child_count(dev->of_node))
			of_platform_populate(dev->of_node, NULL, NULL, dev);
		break;
	case L3_MEMLAT:
		ret = memlat_mon_probe(pdev, type);
		break;
	default:
		/*
		 * This should never happen.
		 */
		dev_err(dev, "Invalid memlat mon type specified: %u\n", type);
		return -EINVAL;
	}

	if (ret < 0) {
		dev_err(dev, "Failure to probe memlat device: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct memlat_mon_spec spec[] = {
	[0] = { MEMLAT_CPU_GRP },
	[1] = { L3_MEMLAT },
};

static const struct of_device_id memlat_match_table[] = {
	{ .compatible = "qcom,rimps-memlat-cpugrp", .data = &spec[0] },
	{ .compatible = "qcom,rimps-memlat-mon-l3", .data = &spec[1] },
	{}
};

static struct platform_driver rimps_memlat_mon_driver = {
	.probe = rimps_memlat_mon_driver_probe,
	.driver = {
		.name = "rimps-memlat-mon",
		.of_match_table = memlat_match_table,
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(rimps_memlat_mon_driver);
MODULE_LICENSE("GPL v2");
