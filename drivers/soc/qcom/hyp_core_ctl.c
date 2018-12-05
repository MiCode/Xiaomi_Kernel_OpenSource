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

#define pr_fmt(fmt)	"hyp_core_ctl: " fmt

#include <linux/init.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/cpuhotplug.h>
#include <uapi/linux/sched/types.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/cpu_cooling.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>

#include <microvisor/microvisor.h>

#define MAX_RESERVE_CPUS (num_possible_cpus()/2)

/**
 * struct hyp_core_ctl_cpumap - vcpu to pcpu mapping for the other guest
 * @sid: System call id to be used while referring to this vcpu
 * @pcpu: The physical CPU number corresponding to this vcpu
 * @curr_pcpu: The current physical CPU number corresponding to this vcpu.
 *             The curr_pcu is set to another CPU when the original assigned
 *             CPU i.e pcpu can't be used due to thermal condition.
 *
 */
struct hyp_core_ctl_cpu_map {
	okl4_kcap_t sid;
	okl4_cpu_id_t pcpu;
	okl4_cpu_id_t curr_pcpu;
};

/**
 * struct hyp_core_ctl_data - The private data structure of this driver
 * @lock: spinlock to serialize task wakeup and enable/reserve_cpus
 * @task: task_struct pointer to the thread running the state machine
 * @pending: state machine work pending status
 * @reservation_enabled: status of the reservation
 * @reservation_mutex: synchronization between thermal handling and
 *                     reservation. The physical CPUs are re-assigned
 *                     during thermal conditions while reservation is
 *                     not enabled. So this synchronization is needed.
 * @reserve_cpus: The CPUs to be reserved. input.
 * @our_isolated_cpus: The CPUs isolated by hyp_core_ctl driver. output.
 * @final_reserved_cpus: The CPUs reserved for the Hypervisor. output.
 *
 * @syscall_id: The system call id for manipulating vcpu to pcpu mappings.
 * @cpumap: The vcpu to pcpu mapping table
 */
struct hyp_core_ctl_data {
	spinlock_t lock;
	struct task_struct *task;
	bool pending;
	bool reservation_enabled;
	struct mutex reservation_mutex;
	cpumask_t reserve_cpus;
	cpumask_t our_isolated_cpus;
	cpumask_t final_reserved_cpus;
	okl4_kcap_t syscall_id;
	struct hyp_core_ctl_cpu_map cpumap[NR_CPUS];
};

#define CREATE_TRACE_POINTS
#include <trace/events/hyp_core_ctl.h>

static struct hyp_core_ctl_data *the_hcd;

static inline void hyp_core_ctl_print_status(char *msg)
{
	trace_hyp_core_ctl_status(the_hcd, msg);

	pr_debug("%s: reserve=%*pbl reserved=%*pbl our_isolated=%*pbl online=%*pbl isolated=%*pbl thermal=%*pbl\n",
		msg, cpumask_pr_args(&the_hcd->reserve_cpus),
		cpumask_pr_args(&the_hcd->final_reserved_cpus),
		cpumask_pr_args(&the_hcd->our_isolated_cpus),
		cpumask_pr_args(cpu_online_mask),
		cpumask_pr_args(cpu_isolated_mask),
		cpumask_pr_args(cpu_cooling_get_max_level_cpumask()));
}

static void hyp_core_ctl_undo_reservation(struct hyp_core_ctl_data *hcd)
{
	int cpu, ret;

	hyp_core_ctl_print_status("undo_reservation_start");

	for_each_cpu(cpu, &hcd->our_isolated_cpus) {
		ret = sched_unisolate_cpu(cpu);
		if (ret < 0) {
			pr_err("fail to un-isolate CPU%d. ret=%d\n", cpu, ret);
			continue;
		}
		cpumask_clear_cpu(cpu, &hcd->our_isolated_cpus);
	}

	hyp_core_ctl_print_status("undo_reservation_end");
}

static void finalize_reservation(struct hyp_core_ctl_data *hcd, cpumask_t *temp)
{
	cpumask_t vcpu_adjust_mask;
	int i, orig_cpu, curr_cpu, replacement_cpu;
	okl4_error_t err;

	/*
	 * When thermal conditions are not present, we return
	 * from here.
	 */
	if (cpumask_equal(temp, &hcd->final_reserved_cpus))
		return;

	/*
	 * When we can't match with the original reserve CPUs request,
	 * don't change the existing scheme. We can't assign the
	 * same physical CPU to multiple virtual CPUs.
	 *
	 * This may only happen when thermal isolate more CPUs.
	 */
	if (cpumask_weight(temp) < cpumask_weight(&hcd->reserve_cpus)) {
		pr_debug("Fail to reserve some CPUs\n");
		return;
	}

	cpumask_copy(&hcd->final_reserved_cpus, temp);
	cpumask_clear(&vcpu_adjust_mask);

	/*
	 * In the first pass, we traverse all virtual CPUs and try
	 * to assign their original physical CPUs if they are
	 * reserved. if the original physical CPU is not reserved,
	 * then check the current physical CPU is reserved or not.
	 * so that we continue to use the current physical CPU.
	 *
	 * If both original CPU and the current CPU are not reserved,
	 * we have to find a replacement. These virtual CPUs are
	 * maintained in vcpu_adjust_mask and processed in the 2nd pass.
	 */
	for (i = 0; i < MAX_RESERVE_CPUS; i++) {
		if (hcd->cpumap[i].sid == 0)
			break;

		orig_cpu = hcd->cpumap[i].pcpu;
		curr_cpu = hcd->cpumap[i].curr_pcpu;

		if (cpumask_test_cpu(orig_cpu, &hcd->final_reserved_cpus)) {
			cpumask_clear_cpu(orig_cpu, temp);

			if (orig_cpu == curr_cpu)
				continue;

			/*
			 * The original pcpu corresponding to this vcpu i.e i
			 * is available in final_reserved_cpus. so restore
			 * the assignment.
			 */
			err = _okl4_sys_scheduler_affinity_set(hcd->syscall_id,
						hcd->cpumap[i].sid, orig_cpu);
			if (err != OKL4_ERROR_OK) {
				pr_err("fail to assign pcpu for vcpu#%d\n", i);
				continue;
			}

			hcd->cpumap[i].curr_pcpu = orig_cpu;
			pr_debug("err=%u vcpu=%d pcpu=%u curr_cpu=%u\n",
					err, i, hcd->cpumap[i].pcpu,
					hcd->cpumap[i].curr_pcpu);
			continue;
		}

		/*
		 * The original CPU is not available but the previously
		 * assigned CPU i.e curr_cpu is still available. so keep
		 * using it.
		 */
		if (cpumask_test_cpu(curr_cpu, &hcd->final_reserved_cpus)) {
			cpumask_clear_cpu(curr_cpu, temp);
			continue;
		}

		/*
		 * A replacement CPU is found in the 2nd pass below. Make
		 * a note of this virtual CPU for which both original and
		 * current physical CPUs are not available in the
		 * final_reserved_cpus.
		 */
		cpumask_set_cpu(i, &vcpu_adjust_mask);
	}

	/*
	 * The vcpu_adjust_mask contain the virtual CPUs that needs
	 * re-assignment. The temp CPU mask contains the remaining
	 * reserved CPUs. so we pick one by one from the remaining
	 * reserved CPUs and assign them to the pending virtual
	 * CPUs.
	 */
	for_each_cpu(i, &vcpu_adjust_mask) {
		replacement_cpu = cpumask_any(temp);
		cpumask_clear_cpu(replacement_cpu, temp);

		err = _okl4_sys_scheduler_affinity_set(hcd->syscall_id,
					hcd->cpumap[i].sid, replacement_cpu);
		if (err != OKL4_ERROR_OK) {
			pr_err("fail to assign pcpu for vcpu#%d\n", i);
			continue;
		}

		hcd->cpumap[i].curr_pcpu = replacement_cpu;
		pr_debug("adjust err=%u vcpu=%d pcpu=%u curr_cpu=%u\n",
				err, i, hcd->cpumap[i].pcpu,
				hcd->cpumap[i].curr_pcpu);

	}

	/* Did we reserve more CPUs than needed? */
	WARN_ON(!cpumask_empty(temp));
}

static void hyp_core_ctl_do_reservation(struct hyp_core_ctl_data *hcd)
{
	cpumask_t offline_cpus, iter_cpus, temp_reserved_cpus;
	int i, ret, iso_required, iso_done;
	const cpumask_t *thermal_cpus = cpu_cooling_get_max_level_cpumask();

	cpumask_clear(&offline_cpus);
	cpumask_clear(&temp_reserved_cpus);

	hyp_core_ctl_print_status("reservation_start");

	/*
	 * Iterate all reserve CPUs and isolate them if not done already.
	 * The offline CPUs can't be isolated but they are considered
	 * reserved. When an offline and reserved CPU comes online, it
	 * will be isolated to honor the reservation.
	 */
	cpumask_andnot(&iter_cpus, &hcd->reserve_cpus, &hcd->our_isolated_cpus);
	cpumask_andnot(&iter_cpus, &iter_cpus, thermal_cpus);

	for_each_cpu(i, &iter_cpus) {
		if (!cpu_online(i)) {
			cpumask_set_cpu(i, &offline_cpus);
			continue;
		}

		ret = sched_isolate_cpu(i);
		if (ret < 0) {
			pr_debug("fail to isolate CPU%d. ret=%d\n", i, ret);
			continue;
		}
		cpumask_set_cpu(i, &hcd->our_isolated_cpus);
	}

	cpumask_andnot(&iter_cpus, &hcd->reserve_cpus, &offline_cpus);
	iso_required = cpumask_weight(&iter_cpus);
	iso_done = cpumask_weight(&hcd->our_isolated_cpus);

	if (iso_done < iso_required) {
		int isolate_need;

		/*
		 * We have isolated fewer CPUs than required. This happens
		 * when some of the CPUs from the reserved_cpus mask
		 * are managed by thermal. Find the replacement CPUs and
		 * isolate them.
		 */
		isolate_need = iso_required - iso_done;

		/*
		 * Create a cpumask from which replacement CPUs can be
		 * picked. Exclude our isolated CPUs, thermal managed
		 * CPUs and offline CPUs, which are already considered
		 * as reserved.
		 */
		cpumask_andnot(&iter_cpus, cpu_possible_mask,
			       &hcd->our_isolated_cpus);
		cpumask_andnot(&iter_cpus, &iter_cpus, thermal_cpus);
		cpumask_andnot(&iter_cpus, &iter_cpus, &offline_cpus);

		/*
		 * Keep the replacement policy simple. The offline CPUs
		 * comes for free. so pick them first.
		 */
		for_each_cpu(i, &iter_cpus) {
			if (!cpu_online(i)) {
				cpumask_set_cpu(i, &offline_cpus);
				if (--isolate_need == 0)
					goto done;
			}
		}

		cpumask_andnot(&iter_cpus, &iter_cpus, &offline_cpus);

		for_each_cpu(i, &iter_cpus) {
			ret = sched_isolate_cpu(i);
			if (ret < 0) {
				pr_debug("fail to isolate CPU%d. ret=%d\n",
						i, ret);
				continue;
			}
			cpumask_set_cpu(i, &hcd->our_isolated_cpus);

			if (--isolate_need == 0)
				break;
		}
	} else if (iso_done > iso_required) {
		int unisolate_need;

		/*
		 * We have isolated more CPUs than required. Un-isolate
		 * the additional CPUs which are not part of the
		 * reserve_cpus mask.
		 *
		 * This happens in the following scenario.
		 *
		 * - Lets say reserve CPUs are CPU4 and CPU5. They are
		 *   isolated.
		 * - CPU4 is isolated by thermal. We found CPU0 as the
		 *   replacement CPU. Now CPU0 and CPU5 are isolated by
		 *   us.
		 * - CPU4 is un-isolated by thermal. We first isolate CPU4
		 *   since it is part of our reserve CPUs. Now CPU0, CPU4
		 *   and CPU5 are isolated by us.
		 * - Since iso_done (3) > iso_required (2), un-isolate
		 *   a CPU which is not part of the reserve CPU. i.e CPU0.
		 */
		unisolate_need = iso_done - iso_required;
		cpumask_andnot(&iter_cpus, &hcd->our_isolated_cpus,
			       &hcd->reserve_cpus);
		for_each_cpu(i, &iter_cpus) {
			ret = sched_unisolate_cpu(i);
			if (ret < 0) {
				pr_err("fail to unisolate CPU%d. ret=%d\n",
				       i, ret);
				continue;
			}
			cpumask_clear_cpu(i, &hcd->our_isolated_cpus);
			if (--unisolate_need == 0)
				break;
		}
	}

done:
	cpumask_or(&temp_reserved_cpus, &hcd->our_isolated_cpus, &offline_cpus);
	finalize_reservation(hcd, &temp_reserved_cpus);

	hyp_core_ctl_print_status("reservation_end");
}

static int hyp_core_ctl_thread(void *data)
{
	struct hyp_core_ctl_data *hcd = data;

	while (1) {
		spin_lock(&hcd->lock);
		if (!hcd->pending) {
			set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock(&hcd->lock);

			schedule();

			spin_lock(&hcd->lock);
			set_current_state(TASK_RUNNING);
		}
		hcd->pending = false;
		spin_unlock(&hcd->lock);

		if (kthread_should_stop())
			break;

		/*
		 * The reservation mutex synchronize the reservation
		 * happens in this thread against the thermal handling.
		 * The CPU re-assignment happens directly from the
		 * thermal callback context when the reservation is
		 * not enabled, since there is no need for isolating.
		 */
		mutex_lock(&hcd->reservation_mutex);
		if (hcd->reservation_enabled)
			hyp_core_ctl_do_reservation(hcd);
		else
			hyp_core_ctl_undo_reservation(hcd);
		mutex_unlock(&hcd->reservation_mutex);
	}

	return 0;
}

static void hyp_core_ctl_handle_thermal(struct hyp_core_ctl_data *hcd,
					int cpu, bool throttled)
{
	cpumask_t temp_mask, iter_cpus;
	const cpumask_t *thermal_cpus = cpu_cooling_get_max_level_cpumask();
	bool notify = false;
	int replacement_cpu;

	hyp_core_ctl_print_status("handle_thermal_start");

	/*
	 * Take a copy of the final_reserved_cpus and adjust the mask
	 * based on the notified CPU's thermal state.
	 */
	cpumask_copy(&temp_mask, &hcd->final_reserved_cpus);

	if (throttled) {
		/*
		 * Find a replacement CPU for this throttled CPU. Select
		 * any CPU that is not managed by thermal and not already
		 * part of the assigned CPUs.
		 */
		cpumask_andnot(&iter_cpus, cpu_possible_mask, thermal_cpus);
		cpumask_andnot(&iter_cpus, &iter_cpus,
			       &hcd->final_reserved_cpus);
		replacement_cpu = cpumask_any(&iter_cpus);

		if (replacement_cpu < nr_cpu_ids) {
			cpumask_clear_cpu(cpu, &temp_mask);
			cpumask_set_cpu(replacement_cpu, &temp_mask);
			notify = true;
		}
	} else {
		/*
		 * One of the original assigned CPU is unthrottled by thermal.
		 * Swap this CPU with any one of the replacement CPUs.
		 */
		cpumask_andnot(&iter_cpus, &hcd->final_reserved_cpus,
			       &hcd->reserve_cpus);
		replacement_cpu = cpumask_any(&iter_cpus);

		if (replacement_cpu < nr_cpu_ids) {
			cpumask_clear_cpu(replacement_cpu, &temp_mask);
			cpumask_set_cpu(cpu, &temp_mask);
			notify = true;
		}
	}

	if (notify)
		finalize_reservation(hcd, &temp_mask);

	hyp_core_ctl_print_status("handle_thermal_end");
}

static int hyp_core_ctl_cpu_cooling_cb(struct notifier_block *nb,
				       unsigned long val, void *data)
{
	int cpu = (long) data;
	const cpumask_t *thermal_cpus = cpu_cooling_get_max_level_cpumask();

	if (!the_hcd)
		return NOTIFY_DONE;

	mutex_lock(&the_hcd->reservation_mutex);

	pr_debug("CPU%d is %s by thermal\n", cpu,
		 val ? "throttled" : "unthrottled");

	if (val) {
		/*
		 * The thermal mitigated CPU is not part of our reserved
		 * CPUs. So nothing to do.
		 */
		if (!cpumask_test_cpu(cpu, &the_hcd->final_reserved_cpus))
			goto out;

		/*
		 * The thermal mitigated CPU is part of our reserved CPUs.
		 *
		 * If it is isolated by us, unisolate it. If it is not
		 * isolated, probably it is offline. In both cases, kick
		 * the state machine to find a replacement CPU.
		 */
		if (cpumask_test_cpu(cpu, &the_hcd->our_isolated_cpus)) {
			sched_unisolate_cpu(cpu);
			cpumask_clear_cpu(cpu, &the_hcd->our_isolated_cpus);
		}
	} else {
		/*
		 * A CPU is unblocked by thermal. We are interested if
		 *
		 * (1) This CPU is part of the original reservation request
		 *     In this case, this CPU should be swapped with one of
		 *     the replacement CPU that is currently reserved.
		 * (2) When some of the thermal mitigated CPUs are currently
		 *     reserved due to unavailability of CPUs. Now that
		 *     thermal unblocked a CPU, swap this with one of the
		 *     thermal mitigated CPU that is currently reserved.
		 */
		if (!cpumask_test_cpu(cpu, &the_hcd->reserve_cpus) &&
		    !cpumask_intersects(&the_hcd->final_reserved_cpus,
		    thermal_cpus))
			goto out;
	}

	if (the_hcd->reservation_enabled) {
		spin_lock(&the_hcd->lock);
		the_hcd->pending = true;
		wake_up_process(the_hcd->task);
		spin_unlock(&the_hcd->lock);
	} else {
		/*
		 * When the reservation is enabled, the state machine
		 * takes care of finding the new replacement CPU or
		 * isolating the unthrottled CPU. However when the
		 * reservation is not enabled, we still want to
		 * re-assign another CPU for a throttled CPU.
		 */
		hyp_core_ctl_handle_thermal(the_hcd, cpu, val);
	}
out:
	mutex_unlock(&the_hcd->reservation_mutex);
	return NOTIFY_OK;
}

static struct notifier_block hyp_core_ctl_nb = {
	.notifier_call = hyp_core_ctl_cpu_cooling_cb,
};

static int hyp_core_ctl_hp_offline(unsigned int cpu)
{
	if (!the_hcd || !the_hcd->reservation_enabled)
		return 0;

	/*
	 * A CPU can't be left in isolated state while it is
	 * going offline. So unisolate the CPU if it is
	 * isolated by us. An offline CPU is considered
	 * as reserved. So no further action is needed.
	 */
	if (cpumask_test_and_clear_cpu(cpu, &the_hcd->our_isolated_cpus))
		sched_unisolate_cpu_unlocked(cpu);

	return 0;
}

static int hyp_core_ctl_hp_online(unsigned int cpu)
{
	if (!the_hcd || !the_hcd->reservation_enabled)
		return 0;

	/*
	 * A reserved CPU is coming online. It should be isolated
	 * to honor the reservation. So kick the state machine.
	 */
	spin_lock(&the_hcd->lock);
	if (cpumask_test_cpu(cpu, &the_hcd->final_reserved_cpus)) {
		the_hcd->pending = true;
		wake_up_process(the_hcd->task);
	}
	spin_unlock(&the_hcd->lock);

	return 0;
}

static int hyp_core_ctl_init_reserve_cpus(struct hyp_core_ctl_data *hcd)
{
	struct _okl4_sys_scheduler_affinity_get_return result;
	int i, ret = 0;

	cpumask_clear(&hcd->reserve_cpus);

	for (i = 0; i < MAX_RESERVE_CPUS; i++) {
		if (hcd->cpumap[i].sid == 0)
			break;

		result = _okl4_sys_scheduler_affinity_get(hcd->syscall_id,
							  hcd->cpumap[i].sid);
		if (result.error != OKL4_ERROR_OK) {
			pr_err("fail to get pcpu for vcpu%d. err=%u\n",
					i, result.error);
			ret = -EPERM;
			break;
		}
		hcd->cpumap[i].pcpu = result.cpu_index;
		hcd->cpumap[i].curr_pcpu = result.cpu_index;
		cpumask_set_cpu(hcd->cpumap[i].pcpu, &hcd->reserve_cpus);
		pr_debug("vcpu%u map to pcpu%u\n", i, result.cpu_index);
	}

	cpumask_copy(&hcd->final_reserved_cpus, &hcd->reserve_cpus);
	pr_info("reserve_cpus=%*pbl ret=%d\n",
		 cpumask_pr_args(&hcd->reserve_cpus), ret);

	return ret;
}

static int hyp_core_ctl_parse_dt(struct platform_device *pdev,
				 struct hyp_core_ctl_data *hcd)
{
	struct device_node *np = pdev->dev.of_node;
	int len, ret, i;
	u32 *reg_values;

	len = of_property_count_u32_elems(np, "reg");
	if (len < 2 || len > MAX_RESERVE_CPUS + 1) {
		pr_err("incorrect reg dt param. err=%d\n", len);
		return -EINVAL;
	}

	reg_values = kmalloc_array(len, sizeof(*reg_values), GFP_KERNEL);
	if (!reg_values)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "reg", reg_values, len);
	if (ret < 0) {
		pr_err("fail to read reg dt param. err=%d\n", ret);
		return -EINVAL;
	}

	hcd->syscall_id = reg_values[0];

	ret = 0;
	for (i = 1; i < len; i++) {
		if (reg_values[i] == 0) {
			ret = -EINVAL;
			pr_err("incorrect sid for vcpu%d\n", i);
		}

		hcd->cpumap[i-1].sid = reg_values[i];
		pr_debug("vcpu=%d sid=%u\n", i-1, hcd->cpumap[i-1].sid);
	}

	kfree(reg_values);
	return ret;
}

static void hyp_core_ctl_enable(bool enable)
{
	spin_lock(&the_hcd->lock);
	if (enable == the_hcd->reservation_enabled)
		goto out;

	trace_hyp_core_ctl_enable(enable);
	pr_debug("reservation %s\n", enable ? "enabled" : "disabled");

	the_hcd->reservation_enabled = enable;
	the_hcd->pending = true;
	wake_up_process(the_hcd->task);
out:
	spin_unlock(&the_hcd->lock);
}

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret < 0)
		return -EINVAL;

	hyp_core_ctl_enable(enable);

	return count;
}

static ssize_t enable_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", the_hcd->reservation_enabled);
}

static DEVICE_ATTR_RW(enable);

static ssize_t status_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct hyp_core_ctl_data *hcd = the_hcd;
	ssize_t count;
	int i;

	mutex_lock(&hcd->reservation_mutex);

	count = scnprintf(buf, PAGE_SIZE, "enabled=%d\n",
			  hcd->reservation_enabled);

	count += scnprintf(buf + count, PAGE_SIZE - count,
			   "reserve_cpus=%*pbl\n",
			   cpumask_pr_args(&hcd->reserve_cpus));

	count += scnprintf(buf + count, PAGE_SIZE - count,
			   "reserved_cpus=%*pbl\n",
			   cpumask_pr_args(&hcd->final_reserved_cpus));

	count += scnprintf(buf + count, PAGE_SIZE - count,
			   "our_isolated_cpus=%*pbl\n",
			   cpumask_pr_args(&hcd->our_isolated_cpus));

	count += scnprintf(buf + count, PAGE_SIZE - count,
			   "online_cpus=%*pbl\n",
			   cpumask_pr_args(cpu_online_mask));

	count += scnprintf(buf + count, PAGE_SIZE - count,
			   "isolated_cpus=%*pbl\n",
			   cpumask_pr_args(cpu_isolated_mask));

	count += scnprintf(buf + count, PAGE_SIZE - count,
		   "thermal_cpus=%*pbl\n",
		   cpumask_pr_args(cpu_cooling_get_max_level_cpumask()));

	count += scnprintf(buf + count, PAGE_SIZE - count,
			   "Vcpu to Pcpu mappings:\n");

	for (i = 0; i < MAX_RESERVE_CPUS; i++) {
		struct _okl4_sys_scheduler_affinity_get_return result;

		if (hcd->cpumap[i].sid == 0)
			break;

		result = _okl4_sys_scheduler_affinity_get(hcd->syscall_id,
							  hcd->cpumap[i].sid);
		if (result.error != OKL4_ERROR_OK)
			continue;

		count += scnprintf(buf + count, PAGE_SIZE - count,
			 "vcpu=%d pcpu=%u curr_pcpu=%u hyp_pcpu=%u\n",
			 i, hcd->cpumap[i].pcpu, hcd->cpumap[i].curr_pcpu,
			 result.cpu_index);

	}

	mutex_unlock(&hcd->reservation_mutex);

	return count;
}

static DEVICE_ATTR_RO(status);

static struct attribute *hyp_core_ctl_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_status.attr,
	NULL
};

static struct attribute_group hyp_core_ctl_attr_group = {
	.attrs = hyp_core_ctl_attrs,
	.name = "hyp_core_ctl",
};

#define CPULIST_SZ 32
static ssize_t read_reserve_cpus(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	char kbuf[CPULIST_SZ];
	int ret;

	ret = scnprintf(kbuf, CPULIST_SZ, "%*pbl\n",
			cpumask_pr_args(&the_hcd->reserve_cpus));

	return simple_read_from_buffer(ubuf, count, ppos, kbuf, ret);
}

static ssize_t write_reserve_cpus(struct file *file, const char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	char kbuf[CPULIST_SZ];
	int ret;
	cpumask_t temp_mask;

	ret = simple_write_to_buffer(kbuf, CPULIST_SZ - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	kbuf[ret] = '\0';
	ret = cpulist_parse(kbuf, &temp_mask);
	if (ret < 0)
		return ret;

	if (cpumask_weight(&temp_mask) !=
			cpumask_weight(&the_hcd->reserve_cpus)) {
		pr_err("incorrect reserve CPU count. expected=%u\n",
				cpumask_weight(&the_hcd->reserve_cpus));
		return -EINVAL;
	}

	spin_lock(&the_hcd->lock);
	if (the_hcd->reservation_enabled) {
		count = -EPERM;
		pr_err("reservation is enabled, can't change reserve_cpus\n");
	} else {
		cpumask_copy(&the_hcd->reserve_cpus, &temp_mask);
	}
	spin_unlock(&the_hcd->lock);

	return count;
}

static const struct file_operations debugfs_reserve_cpus_ops = {
	.read = read_reserve_cpus,
	.write = write_reserve_cpus,
};

static void hyp_core_ctl_debugfs_init(void)
{
	struct dentry *dir, *file;

	dir = debugfs_create_dir("hyp_core_ctl", NULL);
	if (IS_ERR_OR_NULL(dir))
		return;

	file = debugfs_create_file("reserve_cpus", 0644, dir, NULL,
				   &debugfs_reserve_cpus_ops);
	if (!file)
		debugfs_remove(dir);
}

static int hyp_core_ctl_probe(struct platform_device *pdev)
{
	int ret;
	struct hyp_core_ctl_data *hcd;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	hcd = kzalloc(sizeof(*hcd), GFP_KERNEL);
	if (!hcd) {
		ret = -ENOMEM;
		goto out;
	}

	ret = hyp_core_ctl_parse_dt(pdev, hcd);
	if (ret < 0) {
		pr_err("Fail to parse dt. ret=%d\n", ret);
		goto free_hcd;
	}

	ret = hyp_core_ctl_init_reserve_cpus(hcd);
	if (ret < 0) {
		pr_err("Fail to get reserve CPUs from Hyp. ret=%d\n", ret);
		goto free_hcd;
	}

	spin_lock_init(&hcd->lock);
	mutex_init(&hcd->reservation_mutex);
	hcd->task = kthread_run(hyp_core_ctl_thread, (void *) hcd,
				"hyp_core_ctl");

	if (IS_ERR(hcd->task)) {
		ret = PTR_ERR(hcd->task);
		goto free_hcd;
	}

	sched_setscheduler_nocheck(hcd->task, SCHED_FIFO, &param);

	ret = sysfs_create_group(&cpu_subsys.dev_root->kobj,
				 &hyp_core_ctl_attr_group);
	if (ret < 0) {
		pr_err("Fail to create sysfs files. ret=%d\n", ret);
		goto stop_task;
	}

	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
				  "qcom/hyp_core_ctl:online",
				  hyp_core_ctl_hp_online, NULL);

	cpuhp_setup_state_nocalls(CPUHP_HYP_CORE_CTL_ISOLATION_DEAD,
				  "qcom/hyp_core_ctl:dead",
				  NULL, hyp_core_ctl_hp_offline);

	cpu_cooling_max_level_notifier_register(&hyp_core_ctl_nb);
	hyp_core_ctl_debugfs_init();

	the_hcd = hcd;
	return 0;

stop_task:
	kthread_stop(hcd->task);
free_hcd:
	kfree(hcd);
out:
	return ret;
}

static const struct of_device_id hyp_core_ctl_match_table[] = {
	{ .compatible = "qcom,hyp-core-ctl" },
	{},
};

static struct platform_driver hyp_core_ctl_driver = {
	.probe = hyp_core_ctl_probe,
	.driver = {
		.name = "hyp_core_ctl",
		.owner = THIS_MODULE,
		.of_match_table = hyp_core_ctl_match_table,
	 },
};

builtin_platform_driver(hyp_core_ctl_driver);
MODULE_DESCRIPTION("Core Control for Hypervisor");
MODULE_LICENSE("GPL v2");
