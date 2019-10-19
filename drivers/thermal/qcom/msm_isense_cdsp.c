// SPDX-License-Identifier: GPL-2.0-only
/*
 *Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/smem.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#define LIMITS_SMEM_CONFIG        619
#define PARTITION_SIZE_BYTES      4096

struct limits_isense_cdsp_sysfs {
	struct kobj_attribute  attr;
	struct module_kobject *m_kobj;
};

struct limits_isense_cdsp_smem_data {
	uint8_t  subsys_cal_done;
	uint8_t  store_data_in_partition;
	uint16_t size_of_partition_data;
} __packed;

static struct limits_isense_cdsp_smem_data  *limits_isense_cdsp_data;
static struct limits_isense_cdsp_sysfs      *limits_isense_cdsp_sysfs;

static ssize_t limits_isense_cdsp_data_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	if (!limits_isense_cdsp_data)
		return -ENODATA;

	if (!limits_isense_cdsp_data->subsys_cal_done)
		return -EAGAIN;

	if (limits_isense_cdsp_data->store_data_in_partition) {
		if (limits_isense_cdsp_data->size_of_partition_data >
		    PARTITION_SIZE_BYTES)
			return -ENOBUFS;

		memcpy(buf,
		       ((void *)
		       (&limits_isense_cdsp_data->size_of_partition_data) + 2),
		       limits_isense_cdsp_data->size_of_partition_data);

		return limits_isense_cdsp_data->size_of_partition_data;
	}

	return -ENODATA;
}

static int limits_create_msm_limits_cdsp_sysfs(struct platform_device *pdev)
{
	int err = 0;
	struct module_kobject *m_kobj;

	limits_isense_cdsp_sysfs = devm_kcalloc(&pdev->dev, 1,
			sizeof(*limits_isense_cdsp_sysfs), GFP_KERNEL);
	if (!limits_isense_cdsp_sysfs)
		return PTR_ERR(limits_isense_cdsp_sysfs);

	m_kobj = devm_kcalloc(&pdev->dev, 1,
			sizeof(*m_kobj), GFP_KERNEL);
	if (!m_kobj)
		return PTR_ERR(m_kobj);

	limits_isense_cdsp_sysfs->m_kobj = m_kobj;

	m_kobj->mod = THIS_MODULE;
	m_kobj->kobj.kset = module_kset;

	err = kobject_init_and_add(&m_kobj->kobj, &module_ktype, NULL,
				   "%s", KBUILD_MODNAME);
	if (err) {
		dev_err(&pdev->dev,
			"%s: cannot create kobject for %s\n",
			__func__, KBUILD_MODNAME);
		goto exit_handler;
	}

	kobject_get(&m_kobj->kobj);

	if (IS_ERR_OR_NULL(&m_kobj->kobj)) {
		err = PTR_ERR(&m_kobj->kobj);
		goto exit_handler;
	}

	sysfs_attr_init(&limits_isense_cdsp_sysfs->attr.attr);
	limits_isense_cdsp_sysfs->attr.attr.name = "data";
	limits_isense_cdsp_sysfs->attr.attr.mode = 0444;
	limits_isense_cdsp_sysfs->attr.show = limits_isense_cdsp_data_show;
	limits_isense_cdsp_sysfs->attr.store = NULL;

	sysfs_create_file(&m_kobj->kobj, &limits_isense_cdsp_sysfs->attr.attr);
	if (err) {
		dev_err(&pdev->dev, "cannot create sysfs file\n");
		goto exit_handler;
	}

	return err;

exit_handler:
	kobject_del(&m_kobj->kobj);

	return err;
}

static int limits_isense_cdsp_probe(struct platform_device *pdev)
{
	void *smem_ptr = NULL;
	size_t size = 0;
	int err;

	if (limits_isense_cdsp_data)
		return 0;

	smem_ptr = qcom_smem_get(QCOM_SMEM_HOST_ANY, LIMITS_SMEM_CONFIG, &size);
	if (!smem_ptr || !size) {
		dev_err(&pdev->dev,
			"Failed to get limits SMEM Address\n");
		return PTR_ERR(smem_ptr);
	}

	limits_isense_cdsp_data =
			(struct limits_isense_cdsp_smem_data *) smem_ptr;

	err = limits_create_msm_limits_cdsp_sysfs(pdev);

	return err;
}

static int limits_isense_cdsp_remove(struct platform_device *pdev)
{
	limits_isense_cdsp_data = NULL;

	if (limits_isense_cdsp_sysfs && limits_isense_cdsp_sysfs->m_kobj) {
		sysfs_remove_file(&limits_isense_cdsp_sysfs->m_kobj->kobj,
				  &limits_isense_cdsp_sysfs->attr.attr);
		kobject_del(&limits_isense_cdsp_sysfs->m_kobj->kobj);
	}

	return 0;
}

static const struct of_device_id limits_isense_cdsp_match[] = {
	{ .compatible = "qcom,msm-limits-cdsp", },
	{},
};

static struct platform_driver limits_isense_cdsp_driver = {
	.probe = limits_isense_cdsp_probe,
	.remove = limits_isense_cdsp_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = limits_isense_cdsp_match,
	},
};

int __init limits_isense_cdsp_late_init(void)
{
	int err;

	err = platform_driver_register(&limits_isense_cdsp_driver);
	if (err)
		pr_err("Failed to register limits_isense_cdsp platform driver: %d\n",
			err);

	return err;
}
late_initcall(limits_isense_cdsp_late_init);
