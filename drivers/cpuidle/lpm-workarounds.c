/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/msm_thermal.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <soc/qcom/scm.h>

#define L2_HS_STS_SET	0x200

static struct work_struct lpm_wa_work;
static struct workqueue_struct *lpm_wa_wq;
static bool skip_l2_spm;
static bool enable_dynamic_clock_gating;
static bool is_l1_l2_gcc_secure;
static bool store_clock_gating;
int non_boot_cpu_index;
void __iomem *l2_pwr_sts;
void __iomem *l1_l2_gcc;
struct device_clnt_data      *hotplug_handle;
union device_request curr_req;
cpumask_t l1_l2_offline_mask;
cpumask_t offline_mask;
struct resource *l1_l2_gcc_res;
uint32_t l2_status = -1;

static int lpm_wa_callback(struct notifier_block *cpu_nb,
	unsigned long action, void *hcpu)
{
	unsigned long cpu = (unsigned long) hcpu;

	if ((action != CPU_POST_DEAD) && (action != CPU_ONLINE))
		return NOTIFY_OK;

	if (cpu >= non_boot_cpu_index)
		return NOTIFY_OK;


	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_POST_DEAD:
		cpumask_set_cpu(cpu, &offline_mask);
		break;
	case CPU_ONLINE:
		cpumask_clear_cpu(cpu, &offline_mask);
		break;
	default:
		break;
	}

	if (cpumask_equal(&offline_mask, &l1_l2_offline_mask)
					&& store_clock_gating)
		queue_work(lpm_wa_wq, &lpm_wa_work);

	return NOTIFY_OK;
}

static struct notifier_block __refdata lpm_wa_nblk = {
	.notifier_call = lpm_wa_callback,
};

static void process_lpm_workarounds(struct work_struct *w)
{
	int ret = 0, status = 0;


	/* MSM8952 have L1/L2 dynamic clock gating disabled in HW for
	 * performance cluster cores. Enable it via SW to reduce power
	 * impact.
	 */
	if (enable_dynamic_clock_gating) {

		/* Skip enabling L1/L2 clock gating if perf l2 is not in low
		 * power mode.
		 */

		status = (__raw_readl(l2_pwr_sts) & L2_HS_STS_SET)
							== L2_HS_STS_SET;
		if (status) {
			pr_err("%s: perf L2 is not in low power mode\n",
								__func__);
			queue_work(lpm_wa_wq, &lpm_wa_work);
			return;
		}

		l2_status = __raw_readl(l2_pwr_sts);

		pr_err("Set L1_L2_GCC from cpu%d when perf L2 status=0x%x\n",
			smp_processor_id(), l2_status);

		if (is_l1_l2_gcc_secure) {
			scm_io_write((u32)(l1_l2_gcc_res->start), 0x0);
			if (scm_io_read((u32)(l1_l2_gcc_res->start)) !=0)
				pr_err("Failed to set L1_L2_GCC\n");
		}
		else {
			__raw_writel(0x0, l1_l2_gcc);
			if (__raw_readl(l1_l2_gcc) != 0x0)
				pr_err("Failed to set L1_L2_GCC\n");
		}

		HOTPLUG_NO_MITIGATION(&curr_req.offline_mask);
		ret = devmgr_client_request_mitigation(
				hotplug_handle,
				HOTPLUG_MITIGATION_REQ,
				&curr_req);
		if (ret) {
			pr_err("hotplug request failed. err:%d\n", ret);
			return;
		}
		enable_dynamic_clock_gating = false;
		unregister_hotcpu_notifier(&lpm_wa_nblk);
	}
}

/*
 * lpm_wa_skip_l2_spm: Dont program the l2 SPM as TZ is programming the
 * L2 SPM as a workaround for SDI fix.
 */
bool lpm_wa_get_skip_l2_spm(void)
{
	return skip_l2_spm;
}
EXPORT_SYMBOL(lpm_wa_get_skip_l2_spm);

static ssize_t store_clock_gating_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0, val = 0;

	ret = kstrtoint(buf, 10, &val);
	if (ret || !enable_dynamic_clock_gating) {
		pr_err("Invalid input%s %s. err:%d\n", __func__, buf, ret);
		return count;
	}
	cpumask_copy(&curr_req.offline_mask, &l1_l2_offline_mask);
	ret = devmgr_client_request_mitigation(
			hotplug_handle,
			HOTPLUG_MITIGATION_REQ,
			&curr_req);
	if (ret) {
		pr_err("hotplug request failed. err:%d\n", ret);
		return count;
	}

	store_clock_gating = true;
	return count;
}

static struct kobj_attribute clock_gating_enabled_attr =
__ATTR(dynamic_clock_gating, 0644, NULL, store_clock_gating_enabled);

static int lpm_wa_probe(struct platform_device *pdev)
{
	int ret = 0, c = 0, cpu = 0;
	struct kobject *module_kobj = NULL;
	struct kobject *module_lpm_wa = NULL;
	struct resource *res = NULL;
	unsigned long cpu_mask = 0;
	char *key;

	skip_l2_spm = of_property_read_bool(pdev->dev.of_node,
					"qcom,lpm-wa-skip-l2-spm");

	/*
	 * Enabling L1/L2 tag ram clock gating requires core and L2 to be
	 * in quiescent state. lpm-wa-dynamic-clock-gating flag specifies
	 * WA implementation in SW for perf core0 and L2.
	 */
	enable_dynamic_clock_gating = of_property_read_bool(pdev->dev.of_node,
					"qcom,lpm-wa-dynamic-clock-gating");
	if (!enable_dynamic_clock_gating)
		return ret;
	is_l1_l2_gcc_secure = of_property_read_bool(pdev->dev.of_node,
					"qcom,l1_l2_gcc_secure");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"l2-pwr-sts");
	if (!res) {
		pr_err("%s(): Missing perf l2-pwr-sts resource\n",
							 __func__);
		return -EINVAL;
	}

	l2_pwr_sts = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!l2_pwr_sts) {
		pr_err("%s: cannot ioremap l2_pwr_status %s\n",	__func__,
							KBUILD_MODNAME);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"l1-l2-gcc");
	if (!res) {
		pr_err("%s(): Missing L1/L2 GCC override resource\n",
								__func__);
		return -EINVAL;
	}
	l1_l2_gcc_res = res;

	l1_l2_gcc = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!l1_l2_gcc) {
		pr_err("%s: cannot ioremap L1/L2 GCC override %s\n",
						__func__, KBUILD_MODNAME);
		return -ENOMEM;
	}

	key = "qcom,non-boot-cpu-index";
	ret = of_property_read_u32(pdev->dev.of_node, key,
						&non_boot_cpu_index);
	if (!ret) {
		pr_err("%s: Missing qcom,non_boot_cpu_index property\n"
							, __func__);
	}

	key = "qcom,cpu-offline-mask";
	ret = of_property_read_u32(pdev->dev.of_node, key,
						(u32 *) &cpu_mask);
	if (!ret) {
		for_each_set_bit(c, &cpu_mask, num_possible_cpus()) {
			cpumask_set_cpu(c, &l1_l2_offline_mask);
		}
	}

	module_kobj = kset_find_obj(module_kset, "lpm_levels");
	if (!module_kobj) {
		pr_err("%s: cannot find kobject for module lpm_levels\n",
							__func__);
		ret = -ENOENT;
	}

	module_lpm_wa = kobject_create_and_add("lpm_workarounds",
							module_kobj);
	if (!module_lpm_wa) {
		pr_err("%s: cannot create kobject for module %s\n",
			__func__, KBUILD_MODNAME);
		ret = -ENOENT;
	}

	ret = sysfs_create_file(module_lpm_wa,
					&clock_gating_enabled_attr.attr);
	if (ret) {
		pr_err("cannot create attr group. err:%d\n", ret);
		return ret;
	}

	hotplug_handle = devmgr_register_mitigation_client(&pdev->dev,
						HOTPLUG_DEVICE,	NULL);
	if (IS_ERR_OR_NULL(hotplug_handle)) {
		ret = PTR_ERR(hotplug_handle);
		pr_err("Error registering for hotplug. ret:%d\n", ret);
		return ret;
	}

	INIT_WORK(&lpm_wa_work, process_lpm_workarounds);

	lpm_wa_wq = alloc_workqueue("lpm-wa",
			WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	if (IS_ERR_OR_NULL(lpm_wa_wq)) {
		ret = PTR_ERR(lpm_wa_wq);
		pr_err("%s: Failed to allocate workqueue\n", __func__);
		return ret;
	}

	for_each_present_cpu(cpu) {
		if (cpu < non_boot_cpu_index) {
			if (!cpumask_test_cpu(cpu, cpu_online_mask))
				cpumask_set_cpu(cpu, &offline_mask);
		}
	}
	register_hotcpu_notifier(&lpm_wa_nblk);
	return ret;
}

static int lpm_wa_remove(struct platform_device *pdev)
{
	int ret = 0;

	return ret;
}

static struct of_device_id lpm_wa_mtch_tbl[] = {
	{.compatible = "qcom,lpm-workarounds"},
	{},
};

static struct platform_driver lpm_wa_driver = {
	.probe = lpm_wa_probe,
	.remove = lpm_wa_remove,
	.driver = {
		.name = "lpm-workarounds",
		.owner = THIS_MODULE,
		.of_match_table = lpm_wa_mtch_tbl,
	},
};

static int __init lpm_wa_module_init(void)
{
	int ret;
	ret = platform_driver_register(&lpm_wa_driver);
	if (ret)
		pr_info("Error registering %s\n", lpm_wa_driver.driver.name);

	return ret;
}
late_initcall(lpm_wa_module_init);
