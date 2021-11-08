// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#define pr_fmt(fmt) "oemvm_core_ctl: " fmt

#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/gunyah/gh_errno.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/soc/qcom/oemvm_core_ctl.h>

#include "hcall_core_ctl.h"

struct oemvm_core_ctl_cpu_map {
	gh_capid_t cap_id;
	gh_label_t pcpu;
	gh_label_t curr_pcpu;
};

static struct oemvm_core_ctl_cpu_map oem_cpumap[NR_CPUS];
static int nr_vcpus;
static bool is_vcpu_info_populated;
static spinlock_t lock;
static cpumask_t used_cpus;
static cpumask_t final_used_cpus;

static inline bool is_oemvm(gh_vmid_t vmid)
{
	gh_vmid_t oem_vmid;

	if (!gh_rm_get_vmid(GH_OEM_VM, &oem_vmid) && oem_vmid == vmid)
		return true;

	return false;
}

static ssize_t status_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	ssize_t count;
	int i;

	count = scnprintf(buf, PAGE_SIZE, "online_cpus=%*pbl\n",
			  cpumask_pr_args(cpu_online_mask));

	count +=
		scnprintf(buf + count, PAGE_SIZE - count, "active_cpus=%*pbl\n",
			  cpumask_pr_args(cpu_active_mask));

	count += scnprintf(buf + count, PAGE_SIZE - count,
			   "Vcpu to Pcpu mappings:\n");

	for (i = 0; i < num_possible_cpus(); i++) {
		if (oem_cpumap[i].cap_id == GH_CAPID_INVAL)
			break;

		count += scnprintf(buf + count, PAGE_SIZE - count,
				   "vcpu=%d pcpu=%u curr_pcpu=%u\n", i,
				   oem_cpumap[i].pcpu, oem_cpumap[i].curr_pcpu);
	}

	return count;
}

static DEVICE_ATTR_RO(status);

static struct attribute *oemvm_core_ctl_attrs[] = { &dev_attr_status.attr,
						    NULL };

static struct attribute_group oemvm_core_ctl_attr_group = {
	.attrs = oemvm_core_ctl_attrs,
	.name = "oemvm_core_ctl",
};

int oemvm_core_ctl_yield(int vcpu)
{
	unsigned long flags;
	int pcpu, ret;

	if (!cpu_possible(vcpu))
		return -EINVAL;

	spin_lock_irqsave(&lock, flags);

	if (oem_cpumap[vcpu].cap_id == GH_CAPID_INVAL) {
		ret = -EINVAL;
		goto out;
	}

	pcpu = raw_smp_processor_id();
	/* if vCPU in not running on current pCPU, need migrate */
	if (oem_cpumap[vcpu].curr_pcpu != pcpu) {
		ret = gh_hcall_vcpu_affinity_set(oem_cpumap[vcpu].cap_id, pcpu);
		if (ret != GH_ERROR_OK) {
			pr_err("fail to assign pcpu for vcpu#%d err=%d cap_id=%llu cpu=%d\n",
			       vcpu, ret, oem_cpumap[vcpu].cap_id, pcpu);
			ret = gh_remap_error(ret);
			goto out;
		} else {
			cpumask_clear_cpu(oem_cpumap[vcpu].curr_pcpu,
					  &final_used_cpus);
			cpumask_set_cpu(pcpu, &final_used_cpus);
			oem_cpumap[vcpu].curr_pcpu = pcpu;
		}
	}
	ret = gh_hcall_vcpu_yield(0x01, oem_cpumap[vcpu].cap_id);
	if (ret != GH_ERROR_OK) {
		pr_err("fail to yield to vcpu#%d err=%d cap_id=%llu cpu=%d\n",
		       vcpu, ret, oem_cpumap[vcpu].cap_id, pcpu);
		ret = gh_remap_error(ret);
	}

out:
	spin_unlock_irqrestore(&lock, flags);
	return ret;
}
EXPORT_SYMBOL(oemvm_core_ctl_yield);

int oemvm_core_ctl_restore_vcpu(int vcpu)
{
	unsigned long flags;
	int ret = 0;

	if (!cpu_possible(vcpu))
		return -EINVAL;

	spin_lock_irqsave(&lock, flags);

	if (oem_cpumap[vcpu].cap_id == GH_CAPID_INVAL) {
		ret = -EINVAL;
		goto out;
	}
	if (oem_cpumap[vcpu].curr_pcpu != oem_cpumap[vcpu].pcpu) {
		ret = gh_hcall_vcpu_affinity_set(oem_cpumap[vcpu].cap_id,
						 oem_cpumap[vcpu].pcpu);
		if (ret != GH_ERROR_OK) {
			pr_err("fail to assign pcpu for vcpu#%d err=%d cap_id=%llu cpu=%d\n",
			       vcpu, ret, oem_cpumap[vcpu].cap_id,
			       oem_cpumap[vcpu].pcpu);
			ret = gh_remap_error(ret);
		} else {
			cpumask_clear_cpu(oem_cpumap[vcpu].curr_pcpu,
					  &final_used_cpus);
			cpumask_set_cpu(oem_cpumap[vcpu].pcpu,
					&final_used_cpus);
			oem_cpumap[vcpu].curr_pcpu = oem_cpumap[vcpu].pcpu;
		}
	}

out:
	spin_unlock_irqrestore(&lock, flags);
	return ret;
}
EXPORT_SYMBOL(oemvm_core_ctl_restore_vcpu);

static int oem_vcpu_populate_affinity_info(gh_vmid_t vmid, gh_label_t cpu_idx,
					   gh_capid_t cap_id)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	if (nr_vcpus >= num_possible_cpus()) {
		pr_err("Exceeded max vcpus in the system %d\n", nr_vcpus);
		ret = -ENXIO;
		goto out;
	}
	if (!is_vcpu_info_populated) {
		oem_cpumap[nr_vcpus].cap_id = cap_id;
		oem_cpumap[nr_vcpus].pcpu = cpu_idx;
		oem_cpumap[nr_vcpus].curr_pcpu = cpu_idx;

		nr_vcpus++;
		pr_debug("cpu_index:%u vcpu_cap_id:%llu nr_vcpus:%d\n", cpu_idx,
		       cap_id, nr_vcpus);
	}

out:
	spin_unlock_irqrestore(&lock, flags);
	return ret;
}

static void oemvm_core_ctl_init_reserve_cpus(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	for (i = 0; i < num_possible_cpus(); i++) {
		if (oem_cpumap[i].cap_id == GH_CAPID_INVAL)
			break;
		cpumask_set_cpu(oem_cpumap[i].pcpu, &used_cpus);
		pr_debug("vcpu%u map to pcpu%u\n", i, oem_cpumap[i].pcpu);
	}
	cpumask_copy(&final_used_cpus, &used_cpus);

	spin_unlock_irqrestore(&lock, flags);
	pr_debug("init: used_cpus=%*pbl\n", cpumask_pr_args(&final_used_cpus));
}

static int gh_vcpu_done_populate_affinity_info(struct notifier_block *nb,
					       unsigned long cmd, void *data)
{
	struct gh_rm_notif_vm_status_payload *vm_status_payload = data;
	u8 vm_status = vm_status_payload->vm_status;

	if (!is_oemvm(vm_status_payload->vmid)) {
		pr_debug("Reservation scheme skipped for other VM vmid%d\n",
			vm_status_payload->vmid);
		goto out;
	}

	if (cmd == GH_RM_NOTIF_VM_STATUS &&
	    vm_status == GH_RM_VM_STATUS_RUNNING && !is_vcpu_info_populated) {
		oemvm_core_ctl_init_reserve_cpus();
		is_vcpu_info_populated = true;
	}

out:
	return NOTIFY_DONE;
}

static struct notifier_block gh_vcpu_nb = {
	.notifier_call = gh_vcpu_done_populate_affinity_info,
};

static int oemvm_core_ctl_probe(struct platform_device *pdev)
{
	int ret, i;

	for (i = 0; i < num_possible_cpus(); i++)
		oem_cpumap[i].cap_id = GH_CAPID_INVAL;
	spin_lock_init(&lock);

	ret = gh_rm_set_vcpu_affinity_cb(GH_OEM_VM,
					 &oem_vcpu_populate_affinity_info);
	if (ret)
		return ret;

	ret = gh_rm_register_notifier(&gh_vcpu_nb);
	if (ret) {
		pr_err("fail to register gh_rm_notifier\n");
		return ret;
	}

	ret = sysfs_create_group(&cpu_subsys.dev_root->kobj,
				 &oemvm_core_ctl_attr_group);
	if (ret < 0) {
		pr_err("Fail to create sysfs files. ret=%d\n", ret);
		goto out;
	}

	return ret;
out:
	gh_rm_unregister_notifier(&gh_vcpu_nb);
	return ret;
}

static const struct of_device_id oemvm_core_ctl_match_table[] = {
	{ .compatible = "qcom,oemvm-core-ctl" },
	{},
};
MODULE_DEVICE_TABLE(of, oemvm_core_ctl_match_table);

static struct platform_driver oemvm_core_ctl_driver = {
	.probe = oemvm_core_ctl_probe,
	.driver = {
		.name = "oemvm_core_ctl",
		.owner = THIS_MODULE,
		.of_match_table = oemvm_core_ctl_match_table,
	 },
};

builtin_platform_driver(oemvm_core_ctl_driver);
MODULE_DESCRIPTION("OEM Core Control for Hypervisor");
MODULE_LICENSE("GPL v2");
