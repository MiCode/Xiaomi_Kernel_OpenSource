// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) "qti_dyntj_trip: " fmt

#include <linux/ipc_logging.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/qcom_scmi_vendor.h>
#include <linux/sched/clock.h>
#include <linux/scmi_protocol.h>
#include <linux/thermal.h>

#define DYN_TJ_SCMI_ALGO_STR "dyntj"
#define OL_TJ_THRESHOLD 0x1
#define IPC_LOGPAGES 2
#define QDHTT_DBG_DATA(dyn_th, msg, args...) do {		\
		dev_dbg(dyn_th->dev, "%s:" msg, __func__, args);\
		if ((dyn_th) && (dyn_th)->ipc_log) {		\
			ipc_log_string((dyn_th)->ipc_log,	\
			"[%s] "msg"\n",				\
			 current->comm, args);			\
		}						\
	} while (0)

struct dyn_hw_trip {
	char name[THERMAL_NAME_LENGTH];
	struct scmi_device *sdev;
	struct device *dev;
	const struct qcom_scmi_vendor_ops *ops;
	struct scmi_protocol_handle *ph;
	struct thermal_zone_device *tz;
	u32 cluster_id;
	void *ipc_log;
	int last_thresh_req;
	u64 last_req_ts;
};

struct dyn_hw_set_param_payload {
	u32 cluster_id;
	int32_t trip_temp;
} __packed;

struct dyn_hw_get_param_payload {
	int32_t data;
} __packed;

static int qcom_scmi_vp_set_trip_temp(struct dyn_hw_trip *dyn_th, int trip_temp)
{
	int ret = 0;
	u64 algo_str = 0;
	struct dyn_hw_set_param_payload trip_payload;

	if (!dyn_th->ops)
		return -EPERM;

	strscpy((char *)&algo_str, DYN_TJ_SCMI_ALGO_STR, sizeof(algo_str));

	trip_payload.cluster_id = dyn_th->cluster_id;
	trip_payload.trip_temp = trip_temp;

	ret = dyn_th->ops->set_param(dyn_th->ph, &trip_payload, algo_str,
			OL_TJ_THRESHOLD, sizeof(trip_payload));
	if (ret < 0)
		dev_info(dyn_th->dev,
			"Error to set dyntj for trip:%d for %s ret=%d\n",
			trip_temp, dyn_th->name, ret);
	return ret;
}

static int qcom_scmi_vp_get_trip_temp(struct dyn_hw_trip *dyn_th, int *trip_temp)
{
	u64 algo_str = 0;
	int ret = 0;
	struct dyn_hw_get_param_payload trip_payload = {0};

	if (!dyn_th->ops)
		return -EPERM;

	strscpy((char *)&algo_str, DYN_TJ_SCMI_ALGO_STR, sizeof(algo_str));

	trip_payload.data = dyn_th->cluster_id;

	ret = dyn_th->ops->get_param(dyn_th->ph, &trip_payload, algo_str,
			OL_TJ_THRESHOLD, sizeof(trip_payload),
			sizeof(trip_payload));
	if (ret < 0)
		dev_err(dyn_th->dev,
			"Error to get dyntj trip for %s ret=%d\n",
			dyn_th->name, ret);
	else
		*trip_temp = trip_payload.data;

	return ret;
}

static int qcom_dynamic_hw_get_cur_trip(struct thermal_zone_device *tzd, int *temp)
{
	struct dyn_hw_trip *dyn_th = thermal_zone_device_priv(tzd);

	return qcom_scmi_vp_get_trip_temp(dyn_th, temp);
}

static int qcom_dynamic_hw_set_trip_temp(struct thermal_zone_device *tzd,
			     const struct thermal_trip *trip, int temp)
{
	int ret = 0;
	struct dyn_hw_trip *dyn_th = thermal_zone_device_priv(tzd);

	ret = qcom_scmi_vp_set_trip_temp(dyn_th, temp);
	if (ret < 0)
		return ret;

	QDHTT_DBG_DATA(dyn_th, "Dynamic threshold request for %s: %d",
			dyn_th->name, temp);
	dyn_th->last_thresh_req = temp;
	dyn_th->last_req_ts = sched_clock();

	return 0;
}

static const struct thermal_zone_device_ops tzone_ops = {
	.get_temp = qcom_dynamic_hw_get_cur_trip,
	.set_trip_temp	= qcom_dynamic_hw_set_trip_temp,
};

static struct thermal_zone_params tzone_params = {
	.governor_name = "user_space",
	.no_hwmon = true,
};

static int qcom_dynamic_hw_thermal_trip_probe(struct platform_device *pdev)
{
	int ret;
	u32 cpu_cluster_id = 0;
	struct dyn_hw_trip *dyn_th = NULL;
	struct thermal_trip dyn_vendor_trip = {
		.type = THERMAL_TRIP_PASSIVE,
		.flags = THERMAL_TRIP_FLAG_RW_TEMP,
	};

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,cpu-cluster-type", &cpu_cluster_id);
	if (ret < 0)
		return -ENODEV;

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,trip-temperature", &dyn_vendor_trip.temperature);
	if (ret < 0)
		return -ENODEV;

	dyn_th = devm_kzalloc(&pdev->dev, sizeof(*dyn_th), GFP_KERNEL);
	if (!dyn_th)
		return -ENOMEM;

	dyn_th->dev = &pdev->dev;
	dyn_th->sdev = platform_get_qcom_scmi_device(QCOM_PLAT_CPUCP);
	if (IS_ERR(dyn_th->sdev)) {
		ret = PTR_ERR(dyn_th->sdev);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Error getting scmi_dev for cpu_cluster_%d ret=%d\n",
					cpu_cluster_id, ret);
		return ret;
	}
	dyn_th->ops = dyn_th->sdev->handle->devm_protocol_get(dyn_th->sdev,
			QCOM_SCMI_VENDOR_PROTOCOL, &dyn_th->ph);
	if (IS_ERR(dyn_th->ops)) {
		ret = PTR_ERR(dyn_th->ops);
		dyn_th->ops = NULL;
		dev_err(&pdev->dev, "Error getting vendor protocol ops: %d\n", ret);
		return ret;
	}

	snprintf(dyn_th->name, THERMAL_NAME_LENGTH, "cpu-hw-trip-%d", cpu_cluster_id);

	dyn_th->cluster_id = cpu_cluster_id;

	dyn_th->ipc_log = ipc_log_context_create(IPC_LOGPAGES, dyn_th->name, 0);
	if (!dyn_th->ipc_log)
		dev_err(dyn_th->dev, "%s: unable to create IPC Logging for %s\n",
					__func__, dyn_th->name);

	dyn_th->tz = thermal_zone_device_register_with_trips(dyn_th->name,
					&dyn_vendor_trip, 1, dyn_th,
					&tzone_ops, &tzone_params, 0, 0);
	if (IS_ERR(dyn_th->tz)) {
		ret = PTR_ERR(dyn_th->tz);
		dyn_th->tz = NULL;
		return ret;
	}
	platform_set_drvdata(pdev, dyn_th);

	qcom_dynamic_hw_set_trip_temp(dyn_th->tz, &dyn_vendor_trip, dyn_vendor_trip.temperature);

	return 0;
}

static void qcom_dynamic_hw_thermal_trip_remove(struct platform_device *pdev)
{
	struct dyn_hw_trip *dyn_th = platform_get_drvdata(pdev);

	if (dyn_th->tz)
		thermal_zone_device_unregister(dyn_th->tz);
	if (dyn_th->ipc_log)
		ipc_log_context_destroy(dyn_th->ipc_log);
}

static const struct of_device_id qcom_dynamic_hw_thermal_trip_table[] = {
	{ .compatible = "qcom,thermal-hw-trip" },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_dynamic_hw_thermal_trip_table);

static struct platform_driver qcom_dynamic_hw_thermal_trip_driver = {
	.driver = {
		.name = "qcom,thermal-hw-trip",
		.of_match_table = qcom_dynamic_hw_thermal_trip_table,
		.suppress_bind_attrs = true,
	},
	.probe = qcom_dynamic_hw_thermal_trip_probe,
	.remove = qcom_dynamic_hw_thermal_trip_remove,
};

module_platform_driver(qcom_dynamic_hw_thermal_trip_driver);

MODULE_SOFTDEP("pre: qcom_scmi_client");
MODULE_DESCRIPTION("QCOM Dynamic Hardware Thermal Trip Setting Driver");
MODULE_LICENSE("GPL");
