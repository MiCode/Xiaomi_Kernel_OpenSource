// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2017, 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/cpu.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/stat.h>
#include <soc/qcom/scm.h>
#include <linux/platform_device.h>

#define ENABLE_MASK_BITS	0x1

#define _VAL(z)			(ENABLE_MASK_BITS << z)
#define _VALUE(_val, z)		(_val<<(z))
#define _WRITE(x, y, z)		(((~(_VAL(z))) & y) | _VALUE(x, z))

#define MODULE_NAME	"gladiator_hang_detect"
#define MAX_THRES	0xFFFFFFFF
#define MAX_LEN_SYSFS 12

struct hang_detect {
	phys_addr_t *threshold;
	phys_addr_t config;
	int ACE_enable, IO_enable, M1_enable, M2_enable, PCIO_enable;
	int ACE_offset, IO_offset, M1_offset, M2_offset, PCIO_offset;
	uint32_t ACE_threshold, IO_threshold, M1_threshold, M2_threshold,
			 PCIO_threshold;
	struct kobject kobj;
	struct mutex lock;
};

/* interface for exporting attributes */
struct gladiator_hang_attr {
	struct attribute        attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	size_t (*store)(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count);
};

#define GLADIATOR_HANG_ATTR(_name, _mode, _show, _store)	\
	struct gladiator_hang_attr hang_attr_##_name =	\
			__ATTR(_name, _mode, _show, _store)

static inline struct hang_detect *to_gladiator_hang_dev(struct kobject *kobj)
{
	return container_of(kobj, struct hang_detect, kobj);
}

static inline struct gladiator_hang_attr *to_gladiator_attr(
							struct attribute *attr)
{
	return container_of(attr, struct gladiator_hang_attr, attr);
}

static void set_threshold(int offset, struct hang_detect *hang_dev,
		int32_t threshold_val)
{
	if (offset == hang_dev->ACE_offset)
		hang_dev->ACE_threshold = threshold_val;
	else if (offset == hang_dev->IO_offset)
		hang_dev->IO_threshold = threshold_val;
	else if (offset == hang_dev->M1_offset)
		hang_dev->M1_threshold = threshold_val;
	else if (offset == hang_dev->M2_offset)
		hang_dev->M2_threshold = threshold_val;
	else
		hang_dev->PCIO_threshold = threshold_val;
}

static void get_threshold(int offset, struct hang_detect *hang_dev,
		uint32_t *reg_value)
{
	if (offset == hang_dev->ACE_offset)
		*reg_value = hang_dev->ACE_threshold;
	else if (offset == hang_dev->IO_offset)
		*reg_value = hang_dev->IO_threshold;
	else if (offset == hang_dev->M1_offset)
		*reg_value = hang_dev->M1_threshold;
	else if (offset == hang_dev->M2_offset)
		*reg_value = hang_dev->M2_threshold;
	else
		*reg_value = hang_dev->PCIO_threshold;
}

static void set_enable(int offset, struct hang_detect *hang_dev,
		int enabled)
{
	if (offset == hang_dev->ACE_offset)
		hang_dev->ACE_enable = enabled;
	else if (offset == hang_dev->IO_offset)
		hang_dev->IO_enable = enabled;
	else if (offset == hang_dev->M1_offset)
		hang_dev->M1_enable = enabled;
	else if (offset == hang_dev->M2_offset)
		hang_dev->M2_enable = enabled;
	else
		hang_dev->PCIO_enable = enabled;
}

static void get_enable(int offset, struct hang_detect *hang_dev,
		uint32_t *reg_value)
{
	if (offset == hang_dev->ACE_offset)
		*reg_value = hang_dev->ACE_enable;
	else if (offset == hang_dev->IO_offset)
		*reg_value = hang_dev->IO_enable;
	else if (offset == hang_dev->M1_offset)
		*reg_value = hang_dev->M1_enable;
	else if (offset == hang_dev->M2_offset)
		*reg_value = hang_dev->M2_enable;
	else
		*reg_value = hang_dev->PCIO_enable;
}

static void scm_enable_write(int offset, struct hang_detect *hang_dev,
		int enabled, uint32_t reg_value, int *ret)
{
	*ret = scm_io_write(hang_dev->config,
			_WRITE(enabled, reg_value, offset));
}

static int enable_check(const char *buf, int *enabled_pt)
{
	int ret;

	ret = kstrtouint(buf, 0, enabled_pt);
	if (ret < 0)
		return ret;
	if (!(*enabled_pt == 0 || *enabled_pt == 1))
		return -EINVAL;
	return ret;
}


static inline ssize_t generic_enable_show(struct kobject *kobj,
		struct attribute *attr, char *buf, int offset)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);
	uint32_t reg_value;

	get_enable(offset, hang_dev, &reg_value);
	return scnprintf(buf, MAX_LEN_SYSFS, "%u\n", reg_value);
}

static inline ssize_t generic_threshold_show(struct kobject *kobj,
		struct attribute *attr, char *buf, int offset)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);
	uint32_t reg_value;

	get_threshold(offset, hang_dev, &reg_value);
	return scnprintf(buf, MAX_LEN_SYSFS, "0x%x\n", reg_value);
}

static inline size_t generic_threshold_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count,
		int offset)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);
	uint32_t threshold_val;
	int ret;

	ret = kstrtouint(buf, 0, &threshold_val);
	if (ret < 0)
		return ret;
	if (threshold_val <= 0)
		return -EINVAL;
	if (scm_io_write(hang_dev->threshold[offset],
				threshold_val)){
		pr_err("%s: Failed to set threshold for gladiator port\n",
				__func__);
		return -EIO;
	}
	set_threshold(offset, hang_dev, threshold_val);
	return count;
}

static inline size_t generic_enable_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count,
		int offset)
{
	int  ret, enabled;
	uint32_t reg_value;
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	ret = enable_check(buf, &enabled);
	if (ret < 0)
		return ret;
	get_threshold(offset, hang_dev, &reg_value);
	if (reg_value <= 0)
		return -EPERM;
	mutex_lock(&hang_dev->lock);
	reg_value = scm_io_read(hang_dev->config);

	scm_enable_write(offset, hang_dev, enabled, reg_value, &ret);

	if (ret) {
		pr_err("%s: Gladiator failed to set enable for port %s\n",
				__func__, "#_name");
		mutex_unlock(&hang_dev->lock);
		return -EIO;
	}
	mutex_unlock(&hang_dev->lock);
	set_enable(offset, hang_dev, enabled);
	return count;
}


static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct gladiator_hang_attr *gladiator_attr = to_gladiator_attr(attr);
	ssize_t ret = -EIO;

	if (gladiator_attr->show)
		ret = gladiator_attr->show(kobj, attr, buf);

	return ret;
}

static ssize_t attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct gladiator_hang_attr *gladiator_attr = to_gladiator_attr(attr);
	ssize_t ret = -EIO;

	if (gladiator_attr->store)
		ret = gladiator_attr->store(kobj, attr, buf, count);

	return ret;
}

static const struct sysfs_ops gladiator_sysfs_ops = {
	.show	= attr_show,
	.store	= attr_store,
};

static struct kobj_type gladiator_ktype = {
	.sysfs_ops	= &gladiator_sysfs_ops,
};

static ssize_t show_ace_threshold(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_threshold_show(kobj, attr, buf, hang_dev->ACE_offset);
}

static size_t store_ace_threshold(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_threshold_store(kobj, attr, buf, count,
					hang_dev->ACE_offset);
}
GLADIATOR_HANG_ATTR(ace_threshold, 0644, show_ace_threshold,
					store_ace_threshold);

static ssize_t show_io_threshold(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_threshold_show(kobj, attr, buf, hang_dev->IO_offset);
}

static size_t store_io_threshold(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_threshold_store(kobj, attr, buf, count,
					hang_dev->IO_offset);
}
GLADIATOR_HANG_ATTR(io_threshold, 0644, show_io_threshold,
					store_io_threshold);

static ssize_t show_m1_threshold(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_threshold_show(kobj, attr, buf, hang_dev->M1_offset);
}

static size_t store_m1_threshold(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_threshold_store(kobj, attr, buf, count,
					hang_dev->M1_offset);
}
GLADIATOR_HANG_ATTR(m1_threshold, 0644, show_m1_threshold,
					store_m1_threshold);

static ssize_t show_m2_threshold(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_threshold_show(kobj, attr, buf, hang_dev->M2_offset);
}

static size_t store_m2_threshold(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_threshold_store(kobj, attr, buf, count,
					hang_dev->M2_offset);
}
GLADIATOR_HANG_ATTR(m2_threshold, 0644, show_m2_threshold,
					store_m2_threshold);

static ssize_t show_pcio_threshold(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_threshold_show(kobj, attr, buf, hang_dev->PCIO_offset);
}

static size_t store_pcio_threshold(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_threshold_store(kobj, attr, buf, count,
					hang_dev->PCIO_offset);
}
GLADIATOR_HANG_ATTR(pcio_threshold, 0644, show_pcio_threshold,
					store_pcio_threshold);

static ssize_t show_ace_enable(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_enable_show(kobj, attr, buf, hang_dev->ACE_offset);
}

static size_t store_ace_enable(struct kobject *kobj,
			struct attribute *attr, const char *buf, size_t count)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_enable_store(kobj, attr, buf, count,
					hang_dev->ACE_offset);
}
GLADIATOR_HANG_ATTR(ace_enable, 0644, show_ace_enable,
		store_ace_enable);

static ssize_t show_io_enable(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_enable_show(kobj, attr, buf, hang_dev->IO_offset);
}

static size_t store_io_enable(struct kobject *kobj,
			struct attribute *attr, const char *buf, size_t count)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_enable_store(kobj, attr, buf, count,
					hang_dev->IO_offset);
}
GLADIATOR_HANG_ATTR(io_enable, 0644,
		show_io_enable, store_io_enable);


static ssize_t show_m1_enable(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_enable_show(kobj, attr, buf, hang_dev->M1_offset);
}

static size_t store_m1_enable(struct kobject *kobj,
			struct attribute *attr, const char *buf, size_t count)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_enable_store(kobj, attr, buf, count,
					hang_dev->M1_offset);
}
GLADIATOR_HANG_ATTR(m1_enable, 0644,
		show_m1_enable, store_m1_enable);

static ssize_t show_m2_enable(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_enable_show(kobj, attr, buf, hang_dev->M2_offset);
}

static size_t store_m2_enable(struct kobject *kobj,
			struct attribute *attr, const char *buf, size_t count)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_enable_store(kobj, attr, buf, count,
					hang_dev->M2_offset);
}
GLADIATOR_HANG_ATTR(m2_enable, 0644,
		show_m2_enable, store_m2_enable);

static ssize_t show_pcio_enable(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_enable_show(kobj, attr, buf, hang_dev->PCIO_offset);
}

static size_t store_pcio_enable(struct kobject *kobj,
			struct attribute *attr, const char *buf, size_t count)
{
	struct hang_detect *hang_dev = to_gladiator_hang_dev(kobj);

	return generic_enable_store(kobj, attr, buf, count,
					hang_dev->PCIO_offset);
}
GLADIATOR_HANG_ATTR(pcio_enable, 0644,
		show_pcio_enable, store_pcio_enable);

static struct attribute *hang_attrs[] = {
	&hang_attr_ace_threshold.attr,
	&hang_attr_io_threshold.attr,
	&hang_attr_m1_threshold.attr,
	&hang_attr_m2_threshold.attr,
	&hang_attr_pcio_threshold.attr,
	&hang_attr_ace_enable.attr,
	&hang_attr_io_enable.attr,
	&hang_attr_m1_enable.attr,
	&hang_attr_m2_enable.attr,
	&hang_attr_pcio_enable.attr,
	NULL,
};

static struct attribute *hang_attrs_v2[] = {
	&hang_attr_ace_threshold.attr,
	&hang_attr_io_threshold.attr,
	&hang_attr_ace_enable.attr,
	&hang_attr_io_enable.attr,
	NULL,
};

static struct attribute *hang_attrs_v3[] = {
	&hang_attr_ace_threshold.attr,
	&hang_attr_ace_enable.attr,
	NULL,
};

static struct attribute_group hang_attr_group = {
	.attrs = hang_attrs,
};

static const struct of_device_id msm_gladiator_hang_detect_table[] = {
	{ .compatible = "qcom,gladiator-hang-detect" },
	{ .compatible = "qcom,gladiator-hang-detect-v2" },
	{ .compatible = "qcom,gladiator-hang-detect-v3" },
	{}
};

static int msm_gladiator_hang_detect_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct hang_detect *hang_det = NULL;
	int i = 0, ret;
	u32 NR_GLA_REG = 0;
	u32 *treg;
	u32 creg;

	hang_det = devm_kzalloc(&pdev->dev,
			sizeof(*hang_det), GFP_KERNEL);

	if (!hang_det)
		return -ENOMEM;

	if (of_device_is_compatible(node, "qcom,gladiator-hang-detect")) {
		hang_det->ACE_offset = 0;
		hang_det->IO_offset = 2;
		hang_det->M1_offset = 3;
		hang_det->M2_offset = 4;
		hang_det->PCIO_offset = 5;
		NR_GLA_REG = 6;
	} else if (of_device_is_compatible(node,
			"qcom,gladiator-hang-detect-v2")) {
		hang_det->ACE_offset = 0;
		hang_det->IO_offset = 1;
		NR_GLA_REG = 2;
		hang_attr_group.attrs = hang_attrs_v2;
	} else if (of_device_is_compatible(node,
			"qcom,gladiator-hang-detect-v3")) {
		hang_det->ACE_offset = 0;
		NR_GLA_REG = 1;
		hang_attr_group.attrs = hang_attrs_v3;
	}

	hang_det->threshold = devm_kcalloc(&pdev->dev,
			NR_GLA_REG, sizeof(phys_addr_t), GFP_KERNEL);

	if (!hang_det->threshold)
		return -ENOMEM;

	treg = devm_kcalloc(&pdev->dev, NR_GLA_REG, sizeof(u32), GFP_KERNEL);

	if (!treg)
		return -ENOMEM;

	ret = of_property_read_u32_array(node, "qcom,threshold-arr",
			treg, NR_GLA_REG);
	if (ret) {
		pr_err("Can't get threshold-arr property\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "qcom,config-reg", &creg);
	if (ret) {
		pr_err("Can't get config-reg property\n");
		return -EINVAL;
	}

	for (i = 0 ; i < NR_GLA_REG ; i++)
		hang_det->threshold[i] = treg[i];

	hang_det->config = creg;

	ret = kobject_init_and_add(&hang_det->kobj, &gladiator_ktype,
		&cpu_subsys.dev_root->kobj, "%s", "gladiator_hang_detect");
	if (ret) {
		pr_err("%s:Error in creation kobject_add\n", __func__);
		goto out_put_kobj;
	}

	ret = sysfs_create_group(&hang_det->kobj, &hang_attr_group);
	if (ret) {
		pr_err("%s:Error in creation sysfs_create_group\n", __func__);
		goto out_del_kobj;
	}

	mutex_init(&hang_det->lock);
	platform_set_drvdata(pdev, hang_det);
	return 0;

out_del_kobj:
	kobject_del(&hang_det->kobj);
out_put_kobj:
	kobject_put(&hang_det->kobj);

	return ret;
}

static int msm_gladiator_hang_detect_remove(struct platform_device *pdev)
{
	struct hang_detect *hang_det = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	sysfs_remove_group(&hang_det->kobj, &hang_attr_group);
	kobject_del(&hang_det->kobj);
	kobject_put(&hang_det->kobj);
	return 0;
}

static struct platform_driver msm_gladiator_hang_detect_driver = {
	.probe = msm_gladiator_hang_detect_probe,
	.remove = msm_gladiator_hang_detect_remove,
	.driver = {
		.name = MODULE_NAME,
		.of_match_table = msm_gladiator_hang_detect_table,
	},
};

static int __init init_gladiator_hang_detect(void)
{
	return platform_driver_register(&msm_gladiator_hang_detect_driver);
}
module_init(init_gladiator_hang_detect);

static void __exit exit_gladiator_hang_detect(void)
{
	platform_driver_unregister(&msm_gladiator_hang_detect_driver);
}
module_exit(exit_gladiator_hang_detect);

MODULE_DESCRIPTION("MSM Gladiator Hang Detect Driver");
MODULE_LICENSE("GPL v2");
