// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "mpam_msc: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/string.h>
#include <linux/qcom_scmi_vendor.h>
#include <linux/scmi_protocol.h>
#include <soc/qcom/mpam_msc.h>
#include <soc/qcom/mpam_slc.h>

static LIST_HEAD(qcom_mpam_list);

struct qcom_mpam_msc *qcom_msc_lookup(uint32_t msc_id)
{
	struct qcom_mpam_msc *mpam_msc;

	list_for_each_entry(mpam_msc, &qcom_mpam_list, node) {
		if (mpam_msc->msc_id == msc_id)
			return mpam_msc;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(qcom_msc_lookup);

static struct qcom_mpam_msc *msc_param_verification(uint32_t msc_id, struct msc_query *query)
{
	struct qcom_mpam_msc *qcom_msc;

	qcom_msc = qcom_msc_lookup(msc_id);
	if (qcom_msc == NULL) {
		pr_err("msc_id is not correct %d\n", msc_id);
		return NULL;
	}

	if ((qcom_msc->mpam_available & MPAM_AVAILABLE) != MPAM_AVAILABLE) {
		pr_err("qcom MSC %d is not available!\n", msc_id);
		return NULL;
	}

	if (qcom_msc->qcom_msc_id.qcom_msc_type != query->qcom_msc_id.qcom_msc_type) {
		pr_err("Request data not matching!\n");
		return NULL;
	}

	return qcom_msc;
}

int msc_system_get_partition(uint32_t msc_id, void *arg1, void *arg2)
{
	struct qcom_mpam_msc *qcom_msc;

	if ((arg1 == NULL) || (arg2 == NULL))
		return -EINVAL;

	qcom_msc = msc_param_verification(msc_id, (struct msc_query *)arg1);
	if (qcom_msc == NULL)
		return -EINVAL;

	switch (qcom_msc->qcom_msc_id.qcom_msc_type) {
	case SLC:
		if (qcom_msc->ops->get_cache_partition)
			return qcom_msc->ops->get_cache_partition(qcom_msc->dev, arg1, arg2);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(msc_system_get_partition);

int msc_system_get_device_capability(uint32_t msc_id, void *arg1, void *arg2)
{
	struct qcom_mpam_msc *qcom_msc;

	if ((arg1 == NULL) || (arg2 == NULL))
		return -EINVAL;

	qcom_msc = msc_param_verification(msc_id, (struct msc_query *)arg1);
	if (qcom_msc == NULL)
		return -EINVAL;

	switch (qcom_msc->qcom_msc_id.qcom_msc_type) {
	case SLC:
		if (qcom_msc->ops->get_cache_partition_capability)
			return qcom_msc->ops->get_cache_partition_capability(qcom_msc->dev,
					arg1, arg2);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(msc_system_get_device_capability);

int msc_system_set_partition(uint32_t msc_id, void *arg1, void *arg2)
{
	struct qcom_mpam_msc *qcom_msc;

	if ((arg1 == NULL) || (arg2 == NULL))
		return -EINVAL;

	qcom_msc = msc_param_verification(msc_id, (struct msc_query *)arg1);
	if (qcom_msc == NULL)
		return -EINVAL;

	switch (qcom_msc->qcom_msc_id.qcom_msc_type) {
	case SLC:
		if (qcom_msc->ops->set_cache_partition)
			return qcom_msc->ops->set_cache_partition(qcom_msc->dev, arg1, arg2);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(msc_system_set_partition);

int msc_system_mon_config(uint32_t msc_id, void *arg1, void *arg2)
{
	struct qcom_mpam_msc *qcom_msc;

	if ((arg1 == NULL) || (arg2 == NULL))
		return -EINVAL;

	qcom_msc = msc_param_verification(msc_id, (struct msc_query *)arg1);
	if (qcom_msc == NULL)
		return -EINVAL;

	switch (qcom_msc->qcom_msc_id.qcom_msc_type) {
	case SLC:
		if (qcom_msc->ops->mon_config)
			return qcom_msc->ops->mon_config(qcom_msc->dev, arg1, arg2);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(msc_system_mon_config);

int msc_system_reset_partition(uint32_t msc_id, void *arg1, void *arg2)
{
	struct qcom_mpam_msc *qcom_msc;

	if ((arg1 == NULL) || (arg2 == NULL))
		return -EINVAL;

	qcom_msc = msc_param_verification(msc_id, (struct msc_query *)arg1);
	if (qcom_msc == NULL)
		return -EINVAL;

	switch (qcom_msc->qcom_msc_id.qcom_msc_type) {
	case SLC:
		if (qcom_msc->ops->reset_cache_partition)
			return qcom_msc->ops->reset_cache_partition(qcom_msc->dev, arg1, arg2);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(msc_system_reset_partition);

int msc_system_mon_alloc_info(uint32_t msc_id, void *arg1, void *arg2)
{
	struct qcom_mpam_msc *qcom_msc;
	union mon_values *mon_data;

	if ((arg1 == NULL) || (arg2 == NULL))
		return -EINVAL;

	qcom_msc = msc_param_verification(msc_id, (struct msc_query *)arg1);
	if (qcom_msc == NULL)
		return -EINVAL;

	mon_data = (union mon_values *)arg2;
	mon_data->ref.slc_mon_function = CACHE_CAPACITY_CONFIG;
	switch (qcom_msc->qcom_msc_id.qcom_msc_type) {
	case SLC:
		if (qcom_msc->ops->mon_stats_read)
			return qcom_msc->ops->mon_stats_read(qcom_msc->dev, arg1, arg2);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(msc_system_mon_alloc_info);

int msc_system_mon_read_miss_info(uint32_t msc_id, void *arg1, void *arg2)
{
	struct qcom_mpam_msc *qcom_msc;
	union mon_values *mon_data;

	if ((arg1 == NULL) || (arg2 == NULL))
		return -EINVAL;

	qcom_msc = msc_param_verification(msc_id, (struct msc_query *)arg1);
	if (qcom_msc == NULL)
		return -EINVAL;

	mon_data = (union mon_values *)arg2;
	mon_data->ref.slc_mon_function = CACHE_READ_MISS_CONFIG;
	switch (qcom_msc->qcom_msc_id.qcom_msc_type) {
	case SLC:
		if (qcom_msc->ops->mon_stats_read)
			return qcom_msc->ops->mon_stats_read(qcom_msc->dev, arg1, arg2);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(msc_system_mon_read_miss_info);

int attach_mpam_msc(struct device *dev, struct qcom_mpam_msc *qcom_msc,
		uint32_t msc_type)
{
	int ret = -EINVAL;

	if (qcom_msc == NULL)
		return ret;

	switch (msc_type) {
	case SLC:
		break;
	default:
		return ret;
	}


	list_add_tail(&qcom_msc->node, &qcom_mpam_list);
	return 0;
}
EXPORT_SYMBOL_GPL(attach_mpam_msc);

void detach_mpam_msc(struct device *dev, struct qcom_mpam_msc *qcom_msc, uint32_t msc_type)
{
	if (qcom_msc == NULL)
		return;

	switch (msc_type) {
	case SLC:
		break;
	default:
		return;
	}

	list_del(&qcom_msc->node);
}
EXPORT_SYMBOL_GPL(detach_mpam_msc);

static int mpam_msc_probe(struct platform_device *pdev)
{
	if (of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev) < 0)
		dev_err(&pdev->dev, "Mpam driver probe failed.!\n");

	return 0;
}

int mpam_msc_remove(struct platform_device *pdev)
{
	return 0;
}
static const struct of_device_id mpam_msc_table[] = {
	{ .compatible = "qcom,mpam-msc" },
	{}
};

MODULE_DEVICE_TABLE(of, mpam_msc_table);

static struct platform_driver mpam_msc_driver = {
	.driver = {
		.name = "mpam-msc",
		.of_match_table = mpam_msc_table,
		.suppress_bind_attrs = true,
	},
	.probe = mpam_msc_probe,
	.remove = mpam_msc_remove,
};

module_platform_driver(mpam_msc_driver);

MODULE_DESCRIPTION("QCOM MPAM MSC driver");
MODULE_LICENSE("GPL");
