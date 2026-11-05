// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/coresight.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include "coresight-priv.h"
#include "coresight-trace-id.h"
#include "coresight-common.h"
#include "coresight-qmi.h"
struct dummy_drvdata {
	struct device			*dev;
	struct coresight_device		*csdev;
	u8				traceid;
	bool				static_atid;
};

DEFINE_CORESIGHT_DEVLIST(source_devs, "dummy_source");
DEFINE_CORESIGHT_DEVLIST(sink_devs, "dummy_sink");

/*
 * Dummy source could be connected to a QMI device, which can send commmand
 * to subsystem via QMI. This is represented by the Output port of the dummy
 * source connected to the input port of the QMI.
 *
 * Returns	: coresight_device ptr for the QMI device if a QMI is found.
 *		: NULL otherwise.
 */
static struct coresight_device *
dummy_source_get_qmi_device(struct dummy_drvdata *drvdata)
{
	int i;
	struct coresight_device *tmp, *dummy = drvdata->csdev;

	if (!IS_ENABLED(CONFIG_CORESIGHT_QMI))
		return NULL;

	for (i = 0; i < dummy->pdata->nr_outconns; i++) {
		tmp = dummy->pdata->out_conns[i]->dest_dev;
		if (tmp && coresight_is_qmi_device(tmp))
			return tmp;
	}

	return NULL;
}

/* qmi_assign_dummy_source_atid: assign atid to subsystem via qmi
 * device. if there is no qmi helper device connection, retunr 0
 * and exit.
 *
 * Returns : 0 on success
 */

static int qmi_assign_dummy_source_atid(struct dummy_drvdata *drvdata)
{
	struct coresight_device *qmi = dummy_source_get_qmi_device(drvdata);
	struct  coresight_atid_assign_req_msg_v01 *atid_data;
	const char *trace_name;
	int ret;

	ret = of_property_read_string(drvdata->dev->of_node,
			"trace-name", &trace_name);
	if (ret)
		return -EINVAL;

	atid_data = kzalloc(sizeof(*atid_data), GFP_KERNEL);
	if (!atid_data)
		return -ENOMEM;

	strscpy(atid_data->name, trace_name, CORESIGHT_QMI_TRACE_NAME_MAX_LEN);

	atid_data->atids[0] = drvdata->traceid;
	atid_data->num_atids = 1;

	if (qmi)
		return coresight_qmi_assign_atid(qmi, atid_data);
	return 0;
}

static int dummy_source_enable(struct coresight_device *csdev,
			       struct perf_event *event, enum cs_mode mode)
{
	int ret;
	int trace_id;
	struct dummy_drvdata *drvdata =
		 dev_get_drvdata(csdev->dev.parent);

	if (!drvdata->static_atid) {
		trace_id = coresight_trace_id_get_system_id();
		if (trace_id < 0)
			return trace_id;

		drvdata->traceid = (u8)trace_id;
		ret = qmi_assign_dummy_source_atid(drvdata);
		if (ret) {
			coresight_trace_id_put_system_id(trace_id);
			dev_err(drvdata->dev, "Assign dummy source atid fail\n");
			return ret;
		}
	}

	coresight_csr_set_etr_atid(csdev, drvdata->traceid, true, NULL);
	dev_dbg(csdev->dev.parent, "Dummy source enabled\n");

	return 0;
}

static void dummy_source_disable(struct coresight_device *csdev,
				 struct perf_event *event)
{
	struct dummy_drvdata *drvdata =
		 dev_get_drvdata(csdev->dev.parent);
	coresight_csr_set_etr_atid(csdev, drvdata->traceid, false, NULL);
	if (drvdata->static_atid && drvdata->traceid < CORESIGHT_TRACE_ID_RES_TOP)
		coresight_trace_id_free_reserved_id(drvdata->traceid);
	else
		coresight_trace_id_put_system_id(drvdata->traceid);
	dev_dbg(csdev->dev.parent, "Dummy source disabled\n");
}

static int dummy_sink_enable(struct coresight_device *csdev, enum cs_mode mode,
				void *data)
{
	dev_dbg(csdev->dev.parent, "Dummy sink enabled\n");

	return 0;
}

static int dummy_sink_disable(struct coresight_device *csdev)
{
	dev_dbg(csdev->dev.parent, "Dummy sink disabled\n");

	return 0;
}

static const struct coresight_ops_source dummy_source_ops = {
	.enable	= dummy_source_enable,
	.disable = dummy_source_disable,
};

static const struct coresight_ops dummy_source_cs_ops = {
	.source_ops = &dummy_source_ops,
};

static const struct coresight_ops_sink dummy_sink_ops = {
	.enable	= dummy_sink_enable,
	.disable = dummy_sink_disable,
};

static const struct coresight_ops dummy_sink_cs_ops = {
	.sink_ops = &dummy_sink_ops,
};

static ssize_t traceid_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	unsigned long val;
	struct dummy_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->traceid;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(traceid);

static struct attribute *dummy_source_attrs[] = {
	&dev_attr_traceid.attr,
	NULL,
};

static struct attribute_group dummy_source_attr_grp = {
	.attrs = dummy_source_attrs,
};

static const struct attribute_group *dummy_source_attr_grps[] = {
	&dummy_source_attr_grp,
	NULL,
};

static int dummy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct coresight_platform_data *pdata;
	struct dummy_drvdata *drvdata;
	struct coresight_desc desc = { 0 };
	int trace_id;
	int ret;

	if (of_device_is_compatible(node, "arm,coresight-dummy-source")) {

		desc.name = coresight_alloc_device_name(&source_devs, dev);
		if (!desc.name)
			return -ENOMEM;

		desc.type = CORESIGHT_DEV_TYPE_SOURCE;
		desc.subtype.source_subtype =
					CORESIGHT_DEV_SUBTYPE_SOURCE_OTHERS;
		desc.ops = &dummy_source_cs_ops;
		desc.groups = dummy_source_attr_grps;
	} else if (of_device_is_compatible(node, "arm,coresight-dummy-sink")) {
		desc.name = coresight_alloc_device_name(&sink_devs, dev);
		if (!desc.name)
			return -ENOMEM;

		desc.type = CORESIGHT_DEV_TYPE_SINK;
		desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_DUMMY;
		desc.ops = &dummy_sink_cs_ops;
	} else {
		dev_err(dev, "Device type not set\n");
		return -EINVAL;
	}

	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	pdev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	desc.pdata = pdev->dev.platform_data;
	desc.dev = &pdev->dev;
	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	pm_runtime_enable(dev);

	if (of_device_is_compatible(node, "arm,coresight-dummy-source")) {
		if (!of_property_read_u32(pdev->dev.of_node, "atid", &trace_id)) {
			drvdata->static_atid = true;
			ret = coresight_trace_id_reserve_id(trace_id);
			if (ret) {
				dev_err(drvdata->dev, "Reserve atid: %d fail\n", drvdata->traceid);
				return ret;
			}
			drvdata->traceid = (u8)trace_id;
		}
	}



	dev_dbg(dev, "Dummy device initialized\n");

	return 0;
}

static int dummy_remove(struct platform_device *pdev)
{
	struct dummy_drvdata *drvdata = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	pm_runtime_disable(dev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static const struct of_device_id dummy_match[] = {
	{.compatible = "arm,coresight-dummy-source"},
	{.compatible = "arm,coresight-dummy-sink"},
	{},
};

static struct platform_driver dummy_driver = {
	.probe	= dummy_probe,
	.remove	= dummy_remove,
	.driver	= {
		.name   = "coresight-dummy",
		.of_match_table = dummy_match,
	},
};

module_platform_driver(dummy_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CoreSight dummy driver");
