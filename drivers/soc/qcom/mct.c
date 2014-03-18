/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt)	"%s: " fmt, KBUILD_MODNAME

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/cpu_pm.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/syscore_ops.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>

#define MCT_DEV_NAME "mct"
#define MCT_NAME_LENGTH 20
#define MCT_SYSFS_MAX_LENGTH 20
#define DIFFERENT_CONFIG "different_config"
#define MCT_ENABLE "enable"
#define MCT_TYPE_NAME "msm_mct"

static struct kobject *mct_kobj;
static bool mct_boot_enable, mct_deferred, mct_notify_pmic;
static uint32_t mct_ulim, mct_dcnt, mct_wr, mct_vxwr, mct_vlswr, mct_vaw;

/*
 * Maximum Current Throttling Weight Register
 */
#define write_mct_wr(val) asm("mcr p15, 7, %0, c15, c2, 7" : : "r" (val))

/*
 * Maximum Current Throttling Venum eXecution-pipe Weight Register
 */
#define write_mct_vxwr(val) asm("mcr p15, 7, %0, c15, c3, 1" : : "r" (val))

/*
 * Maximum Current Throttling Venum L/S-pipe Weight Register
 */
#define write_mct_vlswr(val) asm("mcr p15, 7, %0, c15, c3, 2" : : "r" (val))

/*
 * Maximum Current Throttling Control Register
 */
#define write_mct_cr(val) asm("mcr p15, 7, %0, c15, c2, 6" : : "r" (val))

/*
 * Maximum Current Throttling Count Register
 */
#define write_mct_cntr(val) asm("mcr p15, 7, %0, c15, c3, 0" : : "r" (val))

/*
 * Read MCT CNTR Register value
 */
#define read_mct_cntr(val) asm("mrc p15, 7, %0, c15, c3, 0" : "=r" (val))

/*
 * Default Maximum Current Throttling Weight Register Value
 */
#define MCT_DEFAULT_WR 0x14221120

/*
 * Default Maximum Current Throttling Venum eXecution-pipe Weight Register Value
 */
#define MCT_DEFAULT_VXWR 0xF8436430

/*
 * Default Maximum Current Throttling Venum L/S-pipe Weight Register Value
 */
#define MCT_DEFAULT_VLSWR 0xA5846330

/*
 * Default Maximum Current Throttling ULIM
 */
#define MCT_DEFAULT_ULIM 0x007F

/*
 * Default Maximum Current Throttling DCNT
 */
#define MCT_DEFAULT_DCNT 0x0C

/*
 * Default Maximum Current Throttling VAW
 */
#define MCT_DEFAULT_VAW 0x1

/*
 * Default Maximum Current Throttling Control Register Value for Enabling MCT
 */
#define MCT_DEFAULT_ENABLE_CR ((mct_ulim << 16) | \
				(mct_dcnt << 8) | \
				(mct_vaw << 4) | 0x3)

/*
 * Default Maximum Current Throttling Control Register Value for Disabling MCT
 */
#define MCT_DEFAULT_DISABLE_CR ((mct_ulim << 16) | \
				(mct_dcnt << 8) | \
				(mct_vaw << 4))

/*
 * Maximum Current Throttling Control Register Value From ULIM and DCNT
 */
#define MCT_CR_FROM_ULIM_DCNT(ulim, dcnt) ((ulim << 16) | (dcnt << 8) | \
				(mct_vaw << 4) | 0x3)

/**
 * MCT control block
 * @kobj:		Pointer to hold the Kobject of the per cpu instance.
 * @mct_enabled:	Indicates MCT mode enabled or not.
 * @mct_type:		holds the MCT type.
 * @mct_ulim:		holds the MCT upper Limit.
 * @mct_dcnt:		holds the MCT Decrement Count.
 * @mct_cntr:		holds the MCTCNTR register value.
 * @mct_regulator:	Pointer to the MCT regulator.
 * @mct_reg_enabled:    Indicates whether MCT regulator is enabled or not.
 */
struct mct_context {
	struct kobject		*kobj;
	bool			mct_enabled;
	char			mct_type[MCT_NAME_LENGTH];
	u32			mct_ulim;
	u32			mct_dcnt;
	u32			mct_cntr;
	struct regulator	*mct_regulator;
	bool			mct_reg_enabled;
};

static DEFINE_PER_CPU(struct mct_context *, gmct);

/*
 * Apply MCT CPU register
 */
static void mct_apply_cpu_register(void *arg)
{
	uint32_t value;
	unsigned int cpu_index = smp_processor_id();

	if (!per_cpu(gmct, cpu_index))
		return;

	if (per_cpu(gmct, cpu_index)->mct_enabled)  {
		write_mct_wr(mct_wr);
		write_mct_vxwr(mct_vxwr);
		write_mct_vlswr(mct_vlswr);
		write_mct_cntr(0);
		value = MCT_CR_FROM_ULIM_DCNT(
			per_cpu(gmct, cpu_index)->mct_ulim,
			per_cpu(gmct, cpu_index)->mct_dcnt);
		write_mct_cr(value);
	} else {
		write_mct_cr(MCT_DEFAULT_DISABLE_CR);
	}
}

/*
 * Read MCTCNTR Register value
 */
static void mct_read_cntr_register(void *arg)
{
	uint32_t value;
	unsigned int cpu_index = smp_processor_id();

	read_mct_cntr(value);
	per_cpu(gmct, cpu_index)->mct_cntr = value;
	return;
}

static int validate_and_show(const char *kobj_name, char *buf)
{
	unsigned int cpu_index = 0;

	if (!per_cpu(gmct, cpu_index)) {
		pr_err("%s: MCT variables not initalized\n", __func__);
		return -EPERM;
	}

	if (!strcmp(kobj_name, MCT_ENABLE)) {
		for_each_possible_cpu(cpu_index) {
			if (per_cpu(gmct, cpu_index)->mct_enabled !=
				per_cpu(gmct, 0)->mct_enabled)
				goto diff_mode;
		}
		cpu_index = 0;
	} else {
		sscanf(kobj_name, "cpu%u", &cpu_index);
	}

	return snprintf(buf, PAGE_SIZE, "%c\n",
		(per_cpu(gmct, cpu_index)->mct_enabled) ? 'Y' : 'N');
diff_mode:
	return snprintf(buf, PAGE_SIZE, "%s\n", DIFFERENT_CONFIG);
}

static ssize_t enable_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return validate_and_show(kobj->name, buf);
}

static ssize_t ulim_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	unsigned int cpu_index = 0;

	if (!per_cpu(gmct, cpu_index)) {
		pr_err("%s: MCT variables not initalized\n", __func__);
		return -EPERM;
	}

	if (!strcmp(kobj->name, "ulim")) {
		for_each_possible_cpu(cpu_index) {
			if (per_cpu(gmct, cpu_index)->mct_ulim !=
				per_cpu(gmct, 0)->mct_ulim)
				goto diff_ulim;
		}
		cpu_index = 0;
	} else {
		sscanf(kobj->name, "cpu%u", &cpu_index);
	}

	return snprintf(buf, PAGE_SIZE, "%x\n",
		per_cpu(gmct, cpu_index)->mct_ulim);
diff_ulim:
	return snprintf(buf, PAGE_SIZE, "%s\n", DIFFERENT_CONFIG);
}

static ssize_t dcnt_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	unsigned int cpu_index = 0;

	if (!per_cpu(gmct, cpu_index)) {
		pr_err("%s: MCT variables not initalized\n", __func__);
		return -EPERM;
	}

	if (!strcmp(kobj->name, "dcnt")) {
		for_each_possible_cpu(cpu_index) {
			if (per_cpu(gmct, cpu_index)->mct_dcnt !=
				per_cpu(gmct, 0)->mct_dcnt)
				goto diff_dcnt;
		}
		cpu_index = 0;
	} else {
		sscanf(kobj->name, "cpu%u", &cpu_index);
	}

	return snprintf(buf, PAGE_SIZE, "%x\n",
		per_cpu(gmct, cpu_index)->mct_dcnt);
diff_dcnt:
	return snprintf(buf, PAGE_SIZE, "%s\n", DIFFERENT_CONFIG);
}

static ssize_t cntr_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	unsigned int cpu_index;

	sscanf(kobj->name, "cpu%u", &cpu_index);
	if (cpu_online(cpu_index))
		smp_call_function_single(cpu_index,
			mct_read_cntr_register,
			NULL, 1);
	else
		per_cpu(gmct, cpu_index)->mct_cntr = 0;

	return snprintf(buf, PAGE_SIZE, "%u\n",
		per_cpu(gmct, cpu_index)->mct_cntr);
}

static ssize_t type_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	if (per_cpu(gmct, 0)) {
		return snprintf(buf, PAGE_SIZE, "%s\n",
			per_cpu(gmct, 0)->mct_type);
	} else {
		pr_err("%s: MCT var. not initialized\n", __func__);
		return -EPERM;
	}
}

static void mct_update_regulator(uint32_t cpu_index)
{
	int ret = 0;
	bool enable = per_cpu(gmct, cpu_index)->mct_enabled;

	if (!per_cpu(gmct, cpu_index)->mct_regulator)
		return;

	if (!mct_notify_pmic)
		return;

	if (enable && !per_cpu(gmct, cpu_index)->mct_reg_enabled)
		ret = regulator_enable(
			per_cpu(gmct, cpu_index)->mct_regulator);
	else if (!enable && per_cpu(gmct, cpu_index)->mct_reg_enabled)
		ret = regulator_disable(
			per_cpu(gmct, cpu_index)->mct_regulator);
	else
		goto reg_update_exit;

	if (ret) {
		pr_err("%s: regulator %s failed for CPU%d. err:%d\n",
			__func__, (enable) ? "enable" : "disable",
			cpu_index, ret);
		goto reg_update_exit;
	} else {
		pr_debug("%s: regulator %s for CPU%d.\n",
			__func__, (enable) ? "enabled" : "disabled",
			cpu_index);
		per_cpu(gmct, cpu_index)->mct_reg_enabled = enable;
	}

reg_update_exit:
	return;
}

static void update_enable(uint32_t cpu_index)
{
	if (per_cpu(gmct, cpu_index)->mct_enabled) {
		if (cpu_online(cpu_index))
			smp_call_function_single(cpu_index,
				mct_apply_cpu_register, NULL, 1);
		mct_update_regulator(cpu_index);
	} else {
		mct_update_regulator(cpu_index);
		if (cpu_online(cpu_index))
			smp_call_function_single(cpu_index,
				mct_apply_cpu_register, NULL, 1);
	}
}

static int validate_and_store(const char *kobj_name, const char *buf)
{
	int ret = 0, cpu_index = 0;
	struct kernel_param kp;
	bool enable;

	if (!per_cpu(gmct, cpu_index)) {
		ret = -EPERM;
		pr_err("%s: MCT var. not initialized\n", __func__);
		goto store_exit;
	}

	kp.arg = &enable;
	ret = param_set_bool(buf, &kp);
	if (ret) {
		pr_err("%s: Invalid input:%s. err:%d\n", __func__, buf, ret);
		goto store_exit;
	}

	if (!strcmp(kobj_name, MCT_ENABLE)) {
		for_each_possible_cpu(cpu_index) {
			per_cpu(gmct, cpu_index)->mct_enabled = enable;
			update_enable(cpu_index);
		}
	} else {
		sscanf(kobj_name, "cpu%u", &cpu_index);
		per_cpu(gmct, cpu_index)->mct_enabled = enable;
		update_enable(cpu_index);
	}

store_exit:
	return ret;
}

static ssize_t enable_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;

	ret = validate_and_store(kobj->name, buf);
	return (ret) ? ret : count;
}

static void update_ulim(int cpu_index, uint32_t value)
{
	per_cpu(gmct, cpu_index)->mct_ulim = value;
	if (cpu_online(cpu_index))
		smp_call_function_single(cpu_index,
			mct_apply_cpu_register, NULL, 1);
}

static ssize_t ulim_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int cpu_index = 0, ret = 0;
	uint32_t value;
	struct kernel_param kp;

	if (!per_cpu(gmct, cpu_index)) {
		ret = -EPERM;
		pr_err("%s: MCT var. not initialized\n", __func__);
		goto ulim_fail;
	}

	kp.arg = &value;
	ret = param_set_uint(buf, &kp);
	if (ret) {
		pr_err("%s: Invalid input:%s. err:%d\n", __func__, buf, ret);
		goto ulim_fail;
	}

	if (!strcmp(kobj->name, "ulim")) {
		for_each_possible_cpu(cpu_index) {
			if (!per_cpu(gmct, cpu_index)->mct_enabled) {
				pr_err("%s: MCT is disabled for cpu%d.\n",
					__func__, cpu_index);
				ret = -EINVAL;
				continue;
			}
			update_ulim(cpu_index, value);
		}
	} else {
		sscanf(kobj->name, "cpu%u", &cpu_index);
		if (!per_cpu(gmct, cpu_index)->mct_enabled) {
			pr_err("%s: MCT is disabled for cpu%d.\n",
				__func__, cpu_index);
			ret = -EINVAL;
			goto ulim_fail;
		}
		update_ulim(cpu_index, value);
	}

ulim_fail:
	return (ret) ? ret : count;
}

static void update_dcnt(int cpu_index, uint32_t value)
{
	per_cpu(gmct, cpu_index)->mct_dcnt = value;
	if (cpu_online(cpu_index))
		smp_call_function_single(cpu_index,
			mct_apply_cpu_register, NULL, 1);
}

static ssize_t dcnt_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int cpu_index = 0, ret = 0;
	uint32_t value;
	struct kernel_param kp;

	if (!per_cpu(gmct, cpu_index)) {
		ret = -EPERM;
		pr_err("%s: MCT var. not initialized\n", __func__);
		goto dcnt_fail;
	}

	kp.arg = &value;
	ret = param_set_uint(buf, &kp);
	if (ret) {
		pr_err("%s: Invalid input:%s. err:%d\n", __func__, buf, ret);
		goto dcnt_fail;
	}

	if (!strcmp(kobj->name, "dcnt")) {
		for_each_possible_cpu(cpu_index) {
			if (!per_cpu(gmct, cpu_index)->mct_enabled) {
				pr_err("%s: MCT is disabled for cpu%d.\n",
					__func__, cpu_index);
				ret = -EINVAL;
				continue;
			}
			update_dcnt(cpu_index, value);
		}
	} else {
		sscanf(kobj->name, "cpu%u", &cpu_index);
		if (!per_cpu(gmct, cpu_index)->mct_enabled) {
			pr_err("%s: MCT is disabled for cpu%d.\n",
				__func__, cpu_index);
			ret = -EINVAL;
			goto dcnt_fail;
		}
		update_dcnt(cpu_index, value);
	}

dcnt_fail:
	return (ret) ? ret : count;
}

static int mct_enable_store(const char *val, const struct kernel_param *kp)
{
	return validate_and_store(MCT_ENABLE, val);
}

static int mct_enable_show(char *buf, const struct kernel_param *kp)
{
	return validate_and_show(MCT_ENABLE, buf);
}

static struct kernel_param_ops pmic_notify_ops = {
	.set = param_set_bool,
	.get = param_get_bool,
};

static struct kernel_param_ops module_ops = {
	.set = mct_enable_store,
	.get = mct_enable_show,
};

module_param_cb(enable, &module_ops, &mct_boot_enable, 0644);
MODULE_PARM_DESC(enable, "Enable maximum current throttling feature");

module_param_cb(notify_pmic, &pmic_notify_ops, &mct_notify_pmic, 0644);
MODULE_PARM_DESC(enable, "Enable/disable MCT notification to PMIC");

/*
 * MCT device attributes
 */
static __refdata struct kobj_attribute type_attr =
	__ATTR(type, 0444, type_show, NULL);
static __refdata struct kobj_attribute ulim_attr =
	__ATTR(ulim, 0644, ulim_show, ulim_store);
static __refdata struct kobj_attribute dcnt_attr =
	__ATTR(dcnt, 0644, dcnt_show, dcnt_store);
static __refdata struct kobj_attribute enable_attr =
	__ATTR(enable, 0644, enable_show, enable_store);
static __refdata struct kobj_attribute cntr_attr =
	__ATTR(cntr, 0444, cntr_show, NULL);

static __refdata struct attribute *common_attrs[] = {
	&type_attr.attr,
	&ulim_attr.attr,
	&dcnt_attr.attr,
	NULL,
};
static __refdata struct attribute_group common_attr_group = {
	.attrs = common_attrs,
};

static __refdata struct attribute *per_cpu_attrs[] = {
	&ulim_attr.attr,
	&dcnt_attr.attr,
	&enable_attr.attr,
	&cntr_attr.attr,
	NULL,
};
static __refdata struct attribute_group per_cpu_attr_group = {
	.attrs = per_cpu_attrs,
};

static int create_mct_sysfs(void)
{
	int i;
	unsigned int cpu_index = 0;
	int ret = 0;

	mct_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!mct_kobj) {
		pr_err("%s: Unable to find kobject for MCT\n",
			__func__);
		ret = -ENODEV;
		goto create_sysfs_exit;
	}

	ret = sysfs_create_group(mct_kobj, &common_attr_group);
	if (ret) {
		pr_err("%s: cannot create attr group. err:%d\n",
			__func__, ret);
		goto create_sysfs_exit;
	}
	for_each_possible_cpu(cpu_index) {
		char cpu_node[10] = "";

		snprintf(cpu_node, sizeof(cpu_node), "cpu%u", cpu_index);
		per_cpu(gmct, cpu_index)->kobj =
			kobject_create_and_add(cpu_node, mct_kobj);
		if (!per_cpu(gmct, cpu_index)->kobj) {
			pr_err("%s: cannot create kobject [%s]\n",
				__func__, cpu_node);
			ret = -ENOMEM;
			goto sysfs_cleanup;
		}

		ret = sysfs_create_group(per_cpu(gmct,
			cpu_index)->kobj, &per_cpu_attr_group);
		if (ret) {
			pr_err("%s: cannot create per cpu attr group. err:%d\n",
				__func__, ret);
			goto sysfs_cleanup;
		}
	}
	goto create_sysfs_exit;

sysfs_cleanup:
	for (i = 0; i <= cpu_index; i++) {
		if (per_cpu(gmct, i)->kobj) {
			kobject_del(per_cpu(gmct, i)->kobj);
			per_cpu(gmct, i)->kobj = NULL;
		}
	}
	kobject_del(mct_kobj);
create_sysfs_exit:
	return ret;
}

static void remove_mct_sysfs(void)
{
	unsigned int cpu_index = 0;

	for_each_possible_cpu(cpu_index) {
		if (per_cpu(gmct, cpu_index)->kobj)
			sysfs_remove_group(
				per_cpu(gmct, cpu_index)->kobj,
				&per_cpu_attr_group);
	}

	if (mct_kobj)
		sysfs_remove_group(mct_kobj, &common_attr_group);

	return;
}

/*
 * Use the cpu notifier to reconfig MCT for the online CPU
 * when necessary.
 */
static int mct_cpu_up_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	/* Restore MCT register values after hotplug */
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
		mct_apply_cpu_register(NULL);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block mct_cpu_notifier = {
	.notifier_call = mct_cpu_up_callback,
};

static int mct_cpu_pm_notifier(struct notifier_block *nb,
		unsigned long cmd, void *v)
{
	/* Restore MCT register values after Idle power collapse */
	switch (cmd) {
	case CPU_PM_EXIT:
		mct_apply_cpu_register(NULL);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block mct_cpupm_nb = {
	.notifier_call = mct_cpu_pm_notifier,
};

static void mct_cpu_suspend_resume(void)
{
	/* Restore MCT register value for cpu0 after suspend */
	mct_apply_cpu_register(NULL);
}

static struct syscore_ops mct_suspend_ops = {
	.resume = mct_cpu_suspend_resume,
};

static int mct_init_context(struct device *dev)
{
	unsigned int cpu_index = 0, ret = 0;
	struct mct_context *mct;

	for_each_possible_cpu(cpu_index) {
		mct = devm_kzalloc(dev, sizeof(struct mct_context), GFP_KERNEL);
		if (!mct) {
			pr_err("%s: Cannot allocate mct_context\n",
				__func__);
			return -ENOMEM;
		}

		mct->mct_enabled = mct_boot_enable;
		mct->mct_ulim = mct_ulim;
		mct->mct_dcnt = mct_dcnt;
		mct->mct_cntr = 0;
		strlcpy(mct->mct_type, MCT_TYPE_NAME, sizeof(mct->mct_type));
		mct->mct_regulator = NULL;

		per_cpu(gmct, cpu_index) = mct;
	}

	return ret;
}

static int probe_regulator(struct platform_device *pdev)
{
	int ret = 0, reg_cnt = 0, cpu_index = 0;
	char *key = NULL;
	struct device_node *node = pdev->dev.of_node;

	key = "qcom,mct-regulators";
	reg_cnt = of_property_count_strings(node, key);
	if ((reg_cnt > num_possible_cpus()) || (reg_cnt < 1)) {
		pr_err("Invalid regulator count:%d\n", reg_cnt);
		ret = -EINVAL;
		goto probe_exit;
	}

	for_each_possible_cpu(cpu_index) {
		const char *reg_name;
		struct mct_context *mct = per_cpu(gmct, cpu_index);

		if (!mct || mct->mct_regulator)
			continue;
		if (cpu_index >= reg_cnt)
			break;

		ret = of_property_read_string_index(node, key, cpu_index,
			&reg_name);
		if (ret) {
			pr_err("Error in read:%s index:%d err:%d\n",
				key, cpu_index, ret);
			continue;
		}
		if (!strlen(reg_name)) {
			pr_err("No regulator specified for cpu:%d\n",
				cpu_index);
			continue;
		}

		mct->mct_regulator = devm_regulator_get(&pdev->dev, reg_name);
		if (IS_ERR_OR_NULL(mct->mct_regulator)) {
			ret = PTR_ERR(mct->mct_regulator);
			mct->mct_regulator = NULL;
			if (ret != -EPROBE_DEFER) {
				pr_err("Failed to get regulator:%s err:%d\n",
					reg_name, ret);
				continue;
			} else {
				pr_err("probe deferred for regulator:%s\n",
					reg_name);
				break;
			}
		}
	}

probe_exit:
	return ret;
}

static int probe_deferrable_property(struct platform_device *pdev)
{
	int ret = 0;

	ret = mct_init_context(&pdev->dev);
	if (ret) {
		pr_err("%s: Error initializing MCT variables. err:%d\n",
			__func__, ret);
		goto defer_exit;
	}

	ret = probe_regulator(pdev);
	if (ret == -EPROBE_DEFER)
		mct_deferred = true;
	else
		ret = 0;

defer_exit:
	return ret;
}

static int mct_probe(struct platform_device *pdev)
{
	int ret = 0, cpu_index = 0;
	char *key = NULL;
	struct device_node *node = pdev->dev.of_node;

	if (mct_deferred)
		goto deferred_entry;

	key = "qcom,mct-wr-weight";
	ret = of_property_read_u32(node, key, &mct_wr);
	if (ret) {
		dev_err(&pdev->dev,
			"%s:Failed reading node:%s, Key:%s. MCT continues.\n",
			__func__, node->full_name, key);
		mct_wr = MCT_DEFAULT_WR;
	}

	key = "qcom,mct-vxwr-weight";
	ret = of_property_read_u32(node, key, &mct_vxwr);
	if (ret) {
		dev_err(&pdev->dev,
			"%s:Failed reading node:%s, Key:%s. MCT continues.\n",
			__func__, node->full_name, key);
		mct_vxwr = MCT_DEFAULT_VXWR;
	}

	key = "qcom,mct-vlswr-weight";
	ret = of_property_read_u32(node, key, &mct_vlswr);
	if (ret) {
		dev_err(&pdev->dev,
			"%s:Failed reading node:%s, Key:%s. MCT continues.\n",
			__func__, node->full_name, key);
		mct_vlswr = MCT_DEFAULT_VLSWR;
	}

	key = "qcom,mct-vaw-energy";
	ret = of_property_read_u32(node, key, &mct_vaw);
	if (ret) {
		dev_err(&pdev->dev,
			"%s:Failed reading node:%s, Key:%s. MCT continues.\n",
			__func__, node->full_name, key);
		mct_vaw = MCT_DEFAULT_VAW;
	}

	key = "qcom,mct-ulim";
	ret = of_property_read_u32(node, key, &mct_ulim);
	if (ret) {
		dev_err(&pdev->dev,
			"%s:Failed reading node:%s, Key:%s. MCT continues.\n",
			__func__, node->full_name, key);
		mct_ulim = MCT_DEFAULT_ULIM;
	}

	key = "qcom,mct-dcnt";
	ret = of_property_read_u32(node, key, &mct_dcnt);
	if (ret) {
		dev_err(&pdev->dev,
			"%s:Failed reading node:%s, Key:%s. MCT continues.\n",
			__func__, node->full_name, key);
		mct_dcnt = MCT_DEFAULT_DCNT;
	}
	mct_boot_enable = true;
	mct_notify_pmic = false;

deferred_entry:
	/* probe_deferrable_property will not return any error other than
	** -EPROBE_DEFER and error during variable init. Regulator is an
	** optional property. So if the regulator property is not defined
	** or if the regulator init fails, this function ignores the error
	** and returns 0.
	*/
	ret = probe_deferrable_property(pdev);
	if (ret)
		return ret;

	ret = create_mct_sysfs();
	if (ret) {
		pr_err("%s: Cannot create mct sysfs. err:%d\n",
			__func__, ret);
		goto mct_probe_exit;
	}
	cpu_pm_register_notifier(&mct_cpupm_nb);
	register_cpu_notifier(&mct_cpu_notifier);
	register_syscore_ops(&mct_suspend_ops);
	platform_set_drvdata(pdev, gmct);
	for_each_possible_cpu(cpu_index) {
		update_enable(cpu_index);
	}

mct_probe_exit:
	if (ret) {
		for_each_possible_cpu(cpu_index) {
			per_cpu(gmct, cpu_index) = NULL;
		}
	}
	return ret;
}

static int mct_remove(struct platform_device *pdev)
{
	unsigned int cpu_index = 0;

	remove_mct_sysfs();
	for_each_possible_cpu(cpu_index) {
		per_cpu(gmct, cpu_index) = NULL;
	}
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id mct_of_match_table[] = {
	{.compatible = "qcom,max-current-throttling"},
	{},
};
static struct platform_driver mct_driver = {
	.probe          = mct_probe,
	.remove         = mct_remove,
	.driver         = {
			.name		= MCT_DEV_NAME,
			.owner		= THIS_MODULE,
			.of_match_table = mct_of_match_table,
	},
};

static int __init mct_init(void)
{
	return platform_driver_register(&mct_driver);
}

static void __exit mct_exit(void)
{
	platform_driver_unregister(&mct_driver);
}

late_initcall(mct_init);
module_exit(mct_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("maximum current throttling");
MODULE_ALIAS("platform:" MCT_DEV_NAME);
