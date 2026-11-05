// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/coresight.h>
#include <linux/suspend.h>

#include "coresight-qmi.h"
#include "coresight-trace-id.h"
#include "coresight-common.h"

#ifdef CONFIG_CORESIGHT_REMOTE_ETM_DEFAULT_ENABLE
static int boot_enable = CONFIG_CORESIGHT_REMOTE_ETM_DEFAULT_ENABLE;
#else
static int boot_enable;
#endif

DEFINE_CORESIGHT_DEVLIST(remote_etm_devs, "remote-etm");

struct remote_etm_drvdata {
	struct device			*dev;
	struct coresight_device		*csdev;
	struct mutex			mutex;
	bool				enable;
	u8				*traceids;
	u32				num_trcid;
	bool				static_atid;
};

/*
 * Remote ETM could be connected to a QMI device, which can send commmand
 * to subsystem via QMI. This is represented by the Output port of the remote
 * ETM connected to the input port of the QMI.
 *
 * Returns	: coresight_device ptr for the QMI device if a QMI is found.
 *		: NULL otherwise.
 */
static struct coresight_device *
remote_etm_get_qmi_device(struct remote_etm_drvdata *drvdata)
{
	int i;
	struct coresight_device *tmp, *etm = drvdata->csdev;

	if (!IS_ENABLED(CONFIG_CORESIGHT_QMI))
		return NULL;

	for (i = 0; i < etm->pdata->nr_outconns; i++) {
		tmp = etm->pdata->out_conns[i]->dest_dev;
		if (tmp && coresight_is_qmi_device(tmp))
			return tmp;
	}

	return NULL;
}

static int qmi_assign_remote_etm_atid(struct remote_etm_drvdata *drvdata)
{
	struct coresight_device *qmi = remote_etm_get_qmi_device(drvdata);
	struct  coresight_atid_assign_req_msg_v01 *atid_data;
	const char *trace_name = dev_name(drvdata->dev);
	int i, ret;

	ret = of_property_read_string(drvdata->dev->of_node,
			"trace-name", &trace_name);
	if (ret)
		return -EINVAL;

	atid_data = kzalloc(sizeof(*atid_data), GFP_KERNEL);
	if (!atid_data)
		return -ENOMEM;

	strscpy(atid_data->name, trace_name, CORESIGHT_QMI_TRACE_NAME_MAX_LEN);

	for (i = 0; i < drvdata->num_trcid; i++)
		atid_data->atids[i] = drvdata->traceids[i];
	atid_data->num_atids = drvdata->num_trcid;

	if (qmi)
		return coresight_qmi_assign_atid(qmi, atid_data);
	return 0;
}

static int qmi_enable_remote_etm(struct remote_etm_drvdata *drvdata)
{
	struct coresight_device *qmi = remote_etm_get_qmi_device(drvdata);

	if (qmi)
		return coresight_qmi_remote_etm_enable(qmi);
	return 0;
}

static int qmi_disable_remote_etm(struct remote_etm_drvdata *drvdata)
{
	struct coresight_device *qmi = remote_etm_get_qmi_device(drvdata);

	if (qmi)
		coresight_qmi_remote_etm_disable(qmi);
	return 0;
}

static int remote_etm_enable(struct coresight_device *csdev,
			     struct perf_event *event, enum cs_mode mode)
{
	struct remote_etm_drvdata *drvdata =
		dev_get_drvdata(csdev->dev.parent);
	int i, ret;

	mutex_lock(&drvdata->mutex);

	if (!drvdata->static_atid) {
		ret = qmi_assign_remote_etm_atid(drvdata);
		if (ret) {
			dev_err(drvdata->dev, "Assign remote etm atid fail\n");
			goto unlock_mutex;
		}
	} else {
		for (i = 0; i < drvdata->num_trcid; i++) {
			ret = coresight_trace_id_reserve_id(drvdata->traceids[i]);
			if (ret) {
				dev_err(drvdata->dev, "reserve atid: %d fail\n",
						drvdata->traceids[i]);
				break;
			}
		}
		if (i < drvdata->num_trcid) {
			for (; i > 0; i--)
				coresight_trace_id_free_reserved_id(drvdata->traceids[i - 1]);
			goto unlock_mutex;
		}
	}

	for (i = 0; i < drvdata->num_trcid; i++)
		coresight_csr_set_etr_atid(csdev, drvdata->traceids[i], true, NULL);

	ret = qmi_enable_remote_etm(drvdata);
	if (ret) {
		dev_err(drvdata->dev, "Enable remote etm fail\n");
		goto error;
	}

	dev_info(drvdata->dev, "Enable remote etm success\n");
	mutex_unlock(&drvdata->mutex);
	return 0;

error:
	for (i = 0; i < drvdata->num_trcid; i++) {
		coresight_csr_set_etr_atid(csdev, drvdata->traceids[i], false, NULL);
		if (drvdata->static_atid)
			coresight_trace_id_free_reserved_id(drvdata->traceids[i]);
	}

unlock_mutex:
	mutex_unlock(&drvdata->mutex);
	return ret;
}

static void remote_etm_disable(struct coresight_device *csdev,
			       struct perf_event *event)
{
	struct remote_etm_drvdata *drvdata =
		 dev_get_drvdata(csdev->dev.parent);
	int i;

	mutex_lock(&drvdata->mutex);

	qmi_disable_remote_etm(drvdata);

	for (i = 0; i < drvdata->num_trcid; i++)
		coresight_csr_set_etr_atid(csdev, drvdata->traceids[i], false, NULL);

	for (i = 0; i < drvdata->num_trcid; i++) {
		if (drvdata->static_atid)
			coresight_trace_id_free_reserved_id(drvdata->traceids[i]);
		else
			coresight_trace_id_put_system_id(drvdata->traceids[i]);
	}
	mutex_unlock(&drvdata->mutex);
}


static const struct coresight_ops_source remote_etm_source_ops = {
	.enable		= remote_etm_enable,
	.disable	= remote_etm_disable,
};

static const struct coresight_ops remote_cs_ops = {
	.source_ops	= &remote_etm_source_ops,
};

static ssize_t traceid_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct remote_etm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (drvdata->num_trcid == 1)
		return scnprintf(buf, PAGE_SIZE, "%#x\n", drvdata->traceids[0]);
	else
		return scnprintf(buf, PAGE_SIZE, "%#x %#x\n",
			drvdata->traceids[0], drvdata->traceids[1]);
}
static DEVICE_ATTR_RO(traceid);

static struct attribute *remote_etm_attrs[] = {
	&dev_attr_traceid.attr,
	NULL,
};

static struct attribute_group remote_etm_attr_grp = {
	.attrs = remote_etm_attrs,
};

static const struct attribute_group *remote_etm_attr_grps[] = {
	&remote_etm_attr_grp,
	NULL,
};

static int remote_etm_get_traceid(struct remote_etm_drvdata *drvdata)
{
	int ret, i, trace_id;
	struct device *dev = drvdata->dev;
	u32 *atid;

	ret = of_property_count_u32_elems(dev->of_node, "atid");
	if (ret < 0) {
		ret = of_property_read_u32(dev->of_node, "qcom,atid-num",
				&drvdata->num_trcid);
		if (ret)
			return -EINVAL;
	} else {
		drvdata->num_trcid = ret;
		drvdata->static_atid = true;
	}

	atid = devm_kcalloc(dev, drvdata->num_trcid, sizeof(*atid), GFP_KERNEL);
	if (!atid)
		return -ENOMEM;

	if (drvdata->static_atid) {
		ret = of_property_read_u32_array(dev->of_node, "atid",
			atid, drvdata->num_trcid);
		if (ret)
			return ret;
	} else {
		for (i = 0; i < drvdata->num_trcid; i++) {
			trace_id = coresight_trace_id_get_system_id();
			if (trace_id < 0)
				return trace_id;

			atid[i] = trace_id;
		}
	}

	drvdata->traceids = devm_kcalloc(dev, drvdata->num_trcid,
					sizeof(u8), GFP_KERNEL);
	if (!drvdata->traceids)
		return -ENOMEM;

	for (i = 0; i < drvdata->num_trcid; i++)
		drvdata->traceids[i] = (u8)atid[i];

	return 0;
}

static int remote_etm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct remote_etm_drvdata *drvdata;
	struct coresight_desc desc = {0 };
	int ret;

	desc.name = coresight_alloc_device_name(&remote_etm_devs, dev);
	if (!desc.name)
		return -ENOMEM;
	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	pdev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	mutex_init(&drvdata->mutex);

	desc.type = CORESIGHT_DEV_TYPE_SOURCE;
	desc.subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_SOFTWARE;
	desc.ops = &remote_cs_ops;
	desc.pdata = pdev->dev.platform_data;
	desc.dev = &pdev->dev;
	desc.groups = remote_etm_attr_grps;
	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto err;
	}

	ret = remote_etm_get_traceid(drvdata);
	if (ret) {
		coresight_unregister(drvdata->csdev);
		return ret;
	}

	dev_info(dev, "Remote ETM initialized\n");

	if (boot_enable)
		coresight_enable(drvdata->csdev);

	return 0;

err:
	return ret;
}

static int remote_etm_remove(struct platform_device *pdev)
{
	struct remote_etm_drvdata *drvdata = platform_get_drvdata(pdev);
	int i;

	if (!drvdata->static_atid)
		for (i = 0; i < drvdata->num_trcid; i++)
			coresight_trace_id_put_system_id(drvdata->traceids[i]);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static const struct of_device_id remote_etm_match[] = {
	{.compatible = "qcom,coresight-remote-etm"},
	{}
};

#ifdef CONFIG_DEEPSLEEP
static int remote_etm_suspend(struct device *dev)
{
	struct remote_etm_drvdata *drvdata = dev_get_drvdata(dev);
	struct coresight_device	*csdev = drvdata->csdev;

	if (pm_suspend_target_state == PM_SUSPEND_MEM) {
		do {
			coresight_disable(csdev);
		} while (atomic_read(&csdev->refcnt));
	}

	return 0;
}
#else
static int remote_etm_suspend(struct device *dev)
{
	return 0;
}
#endif

#ifdef CONFIG_HIBERNATION
static int remote_etm_freeze(struct device *dev)
{
	struct remote_etm_drvdata *drvdata = dev_get_drvdata(dev);
	struct coresight_device	*csdev = drvdata->csdev;

	do {
		coresight_disable(csdev);
	} while (atomic_read(&csdev->refcnt));

	return 0;
}
#else
static int remote_etm_freeze(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops remote_etm_dev_pm_ops = {
	.suspend = remote_etm_suspend,
	.freeze  = remote_etm_freeze,
};

static struct platform_driver remote_etm_driver = {
	.probe          = remote_etm_probe,
	.remove         = remote_etm_remove,
	.driver         = {
		.name   = "coresight-remote-etm",
		.of_match_table = remote_etm_match,
		.pm = pm_ptr(&remote_etm_dev_pm_ops),
	},
};

int __init remote_etm_init(void)
{
	return platform_driver_register(&remote_etm_driver);
}
module_init(remote_etm_init);

void __exit remote_etm_exit(void)
{
	platform_driver_unregister(&remote_etm_driver);
}
module_exit(remote_etm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CoreSight Remote ETM driver");
