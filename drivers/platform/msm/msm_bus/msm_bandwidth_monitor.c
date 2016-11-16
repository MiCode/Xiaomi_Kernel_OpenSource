/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/thermal.h>
#include <dt-bindings/msm/msm-bus-ids.h>
#include <dt-bindings/msm/msm-bus-rule-ops.h>
#include <linux/msm_bus_rules.h>

#define BM_SENSORS_DRIVER_NAME "bm_sensors"

#define ALLOC_KMEM(dev, ptr, type, size, ret) \
	do { \
		ret = 0; \
		ptr = devm_kzalloc(dev, \
			sizeof(type) * size,\
			GFP_KERNEL); \
		if (!ptr) \
			ret = -ENOMEM; \
	} while (0)

/* Trips: HIGH and LOW */
enum bm_threshold_id {
	BM_HIGH_THRESHOLD = 0,
	BM_LOW_THRESHOLD,
	MAX_BM_THRESHOLD,
};
#define BM_WRITABLE_TRIPS_MASK ((1 << MAX_BM_THRESHOLD) - 1)

struct bm_thresh_state {
	enum thermal_trip_activation_mode state;
	u64                               trip_bw;
};

struct bm_tz_sensor {
	/* Sensor info from dt */
	char                         bm_sensor[THERMAL_NAME_LENGTH];
	int                          src_cnt;
	int                          *src_id;
	int                          src_field;
	int                          op;
	/* bus rule interface struct */
	struct bus_rule_type         thresh_bus_rule[MAX_BM_THRESHOLD];
	uint32_t                     bus_rule_action[MAX_BM_THRESHOLD];
	unsigned long                triggered[BITS_TO_LONGS(MAX_BM_THRESHOLD)];
	bool                         bus_reg_done;
	/* Common callback notifier*/
	struct notifier_block        bm_nb;
	/* thermal zone handling */
	struct thermal_zone_device   *tz_dev;
	enum thermal_device_mode     mode;
	struct bm_thresh_state       thresh_state[MAX_BM_THRESHOLD];
};

struct bm_sensors_drv_data {
	struct platform_device       *pdev;
	/* Workqueue Related */
	struct work_struct           bm_wq;
	/* Serialize workqueue handling */
	struct mutex                 rule_lock;
	struct bm_tz_sensor          *bm_tz_sensors;
	int                          bm_sensor_cnt;
};

static struct bm_sensors_drv_data *bmdev;

static void notify_bw_threshold_event(struct bm_tz_sensor *sensor,
		struct bus_rule_type *rule, uint32_t action,
		enum bm_threshold_id thresh_id)
{
	u64 bw = 0;
	int ret = 0;

	if (!sensor || !rule || thresh_id < 0 ||
		thresh_id >= MAX_BM_THRESHOLD) {
		pr_err("Invalid input parameters\n");
		return;
	}

	if (action == RULE_STATE_APPLIED) {
		sensor->thresh_state[thresh_id].state
			= THERMAL_TRIP_ACTIVATION_DISABLED;
		ret = msm_rule_query_bandwidth(rule,
			&bw, &sensor->bm_nb);
		if (ret < 0) {
			pr_err("Error in reading curr bw, ret:%d\n", ret);
			return;
		}
		thermal_sensor_trip(sensor->tz_dev,
			(thresh_id == BM_HIGH_THRESHOLD) ?
			THERMAL_TRIP_CONFIGURABLE_HI :
			THERMAL_TRIP_CONFIGURABLE_LOW,
			(long)bw);
	} else if (action != RULE_STATE_NOT_APPLIED) {
		pr_err_ratelimited("Error in high threshold interrupt\n");
	}
}

static void cleanup_bus_rule(struct bus_rule_type *rule)
{
	if (!rule) {
		pr_err("input is NULL\n");
		return;
	}

	if (rule->src_id)
		devm_kfree(&bmdev->pdev->dev, rule->src_id);
	if (rule->src_field)
		devm_kfree(&bmdev->pdev->dev, rule->src_field);
	if (rule->op)
		devm_kfree(&bmdev->pdev->dev, rule->op);
	if (rule->thresh)
		devm_kfree(&bmdev->pdev->dev, rule->thresh);
}

static int create_bus_rule(struct bus_rule_type *rule, int src_cnt,
		int *src_ids, int src_field, int op, u64 val,
		unsigned int *thresh_action)
{
	int i = 0, ret = 0;

	if (!rule->src_id) {
		ALLOC_KMEM(&bmdev->pdev->dev, rule->src_id,
			int, src_cnt, ret);
		if (ret)
			goto rule_exit;
	}
	if (!rule->src_field) {
		ALLOC_KMEM(&bmdev->pdev->dev, rule->src_field,
			int, 1, ret);
		if (ret)
			goto rule_exit;
	}
	if (!rule->op) {
		ALLOC_KMEM(&bmdev->pdev->dev, rule->op,
			int, 1, ret);
		if (ret)
			goto rule_exit;
	}
	if (!rule->thresh) {
		ALLOC_KMEM(&bmdev->pdev->dev, rule->thresh,
			u64, 1, ret);
		if (ret)
			goto rule_exit;
	}

	rule->num_src = src_cnt;
	for (; i < src_cnt; i++)
		rule->src_id[i] = src_ids[i];
	*(rule->src_field) = src_field;
	*(rule->op) = op;
	rule->num_thresh = 1;
	*(rule->thresh) = val;
	rule->num_dst = 0;
	rule->dst_node = NULL;
	rule->mode = THROTTLE_OFF;
	rule->client_data = (void *)thresh_action;
	rule->dst_bw = 0;
	rule->combo_op = 0;

rule_exit:
	if (ret)
		cleanup_bus_rule(rule);

	return ret;
}

static void bm_tz_notify_wq(struct work_struct *work)
{
	int i = 0, j = 0;

	mutex_lock(&bmdev->rule_lock);

	/* Notify bw threshold events to thermal zone */
	for (i = 0; i < bmdev->bm_sensor_cnt; i++) {
		if (!bitmap_weight(bmdev->bm_tz_sensors[i].triggered,
			MAX_BM_THRESHOLD))
			continue;

		for_each_set_bit(j, bmdev->bm_tz_sensors[i].triggered,
			MAX_BM_THRESHOLD) {
			notify_bw_threshold_event(&bmdev->bm_tz_sensors[i],
				&bmdev->bm_tz_sensors[i].thresh_bus_rule[j],
				bmdev->bm_tz_sensors[i].bus_rule_action[j], j);
			clear_bit(j, bmdev->bm_tz_sensors[i].triggered);
		}
	}
	mutex_unlock(&bmdev->rule_lock);
}

static int bm_threshold_cb(struct notifier_block *nb, unsigned long action,
		void *data)
{
	int thresh_id = 0;
	struct bm_tz_sensor *bm_sensor =
		container_of(nb, struct bm_tz_sensor, bm_nb);
	struct bus_rule_type *rule = data;
	unsigned int *bus_rule_action = (unsigned int *)(rule->client_data);

	for (; thresh_id < MAX_BM_THRESHOLD; thresh_id++)
		if (&bm_sensor->bus_rule_action[thresh_id] == bus_rule_action)
			break;

	if (thresh_id >= MAX_BM_THRESHOLD) {
		pr_err("No matching client data. id:%d\n", thresh_id);
		return 0;
	}
	pr_debug(
	"srcid:%d, srcfld:%d op:%d thresh:%llu threshid:%d action:%lu curr_bw:%llu\n",
	*(rule->src_id), *(rule->src_field), *(rule->op), *(rule->thresh),
	thresh_id, action, rule->curr_bw);
	set_bit(thresh_id, bm_sensor->triggered);
	bm_sensor->bus_rule_action[thresh_id] = action;
	queue_work(system_freezable_wq, &bmdev->bm_wq);

	return 0;
}

static int bm_tz_get_curr_bw(struct thermal_zone_device *bmdev,
			     unsigned long *bw)
{
	struct bm_tz_sensor *bm_sensor = bmdev->devdata;
	int ret = 0;

	if (!bm_sensor || bm_sensor->mode != THERMAL_DEVICE_ENABLED || !bw)
		return -EINVAL;

	ret = msm_rule_query_bandwidth(
		&bm_sensor->thresh_bus_rule[BM_HIGH_THRESHOLD],
		(u64 *)bw,
		&bm_sensor->bm_nb);
	if (ret < 0) {
		pr_err("Error in reading curr bw, ret:%d\n", ret);
		return -EINVAL;
	}
	pr_debug("bm sensor[%s] curr bw:%lu\n", bm_sensor->bm_sensor, *bw);

	return 0;
}

static int bm_tz_get_mode(struct thermal_zone_device *bm,
			      enum thermal_device_mode *mode)
{
	struct bm_tz_sensor *bm_sensor = bm->devdata;

	if (!bm_sensor || !mode)
		return -EINVAL;

	*mode = bm_sensor->mode;

	return 0;
}

static int bm_tz_get_trip_type(struct thermal_zone_device *bm,
				   int trip, enum thermal_trip_type *type)
{
	struct bm_tz_sensor *bm_sensor = bm->devdata;

	if (!bm_sensor || trip < 0 || !type)
		return -EINVAL;

	switch (trip) {
	case BM_HIGH_THRESHOLD:
		*type = THERMAL_TRIP_CONFIGURABLE_HI;
		break;
	case BM_LOW_THRESHOLD:
		*type = THERMAL_TRIP_CONFIGURABLE_LOW;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bm_tz_activate_trip_type(struct thermal_zone_device *bm,
			int trip, enum thermal_trip_activation_mode mode)
{
	struct bm_tz_sensor *bm_sensor = bm->devdata;
	struct bus_rule_type new_rule = {0};
	int op = -1;
	int ret = 0;
	u64 thresh = 0;

	if (!bm_sensor || trip < 0)
		return -EINVAL;

	switch (trip) {
	case BM_HIGH_THRESHOLD:
		bm_sensor->thresh_state[BM_HIGH_THRESHOLD].state = mode;
		if (mode != THERMAL_TRIP_ACTIVATION_DISABLED)
			thresh =
			bm_sensor->thresh_state[BM_HIGH_THRESHOLD].trip_bw;
		else
			thresh = ULLONG_MAX;
		op =  OP_GT;
		break;
	case BM_LOW_THRESHOLD:
		bm_sensor->thresh_state[BM_LOW_THRESHOLD].state = mode;
		if (mode != THERMAL_TRIP_ACTIVATION_DISABLED)
			thresh =
			bm_sensor->thresh_state[BM_LOW_THRESHOLD].trip_bw;
		else
			thresh = 0;
		op =  OP_LT;
		break;
	default:
		pr_err("Unknown trip %d\n", trip);
		return -EINVAL;
	}

	ret = create_bus_rule(&new_rule,
		bm_sensor->src_cnt, bm_sensor->src_id,
		bm_sensor->src_field, op, thresh,
		&bm_sensor->bus_rule_action[trip]);
	if (ret)
		goto exit;

	bm_sensor->bm_nb.notifier_call = bm_threshold_cb;
	mutex_lock(&bmdev->rule_lock);
	msm_rule_update(&bm_sensor->thresh_bus_rule[trip],
		&new_rule, &bm_sensor->bm_nb);
	*(bm_sensor->thresh_bus_rule[trip].thresh) = thresh;
	msm_rule_evaluate_rules(bm_sensor->thresh_bus_rule[trip].src_id[0]);
	cleanup_bus_rule(&new_rule);
	mutex_unlock(&bmdev->rule_lock);

	pr_debug(
	"srcid:%d, srcfld:%d op:%d thresh:%llu threshid:%d\n",
	*(bm_sensor->thresh_bus_rule[trip].src_id),
	*(bm_sensor->thresh_bus_rule[trip].src_field),
	*(bm_sensor->thresh_bus_rule[trip].op),
	*(bm_sensor->thresh_bus_rule[trip].thresh), trip);

exit:
	return 0;
};

static int bm_tz_get_trip_bw(struct thermal_zone_device *bmdev,
				   int trip, unsigned long *bw)
{
	struct bm_tz_sensor *bm_sensor = bmdev->devdata;

	if (!bm_sensor || trip < 0 || trip >= MAX_BM_THRESHOLD || !bw)
		return -EINVAL;

	*bw = bm_sensor->thresh_state[trip].trip_bw;

	return 0;
};

static int bm_tz_set_trip_bw(struct thermal_zone_device *bmdev,
				   int trip, unsigned long bw)
{
	struct bm_tz_sensor *bm_sensor = bmdev->devdata;

	if (!bm_sensor || trip < 0 || trip >= MAX_BM_THRESHOLD)
		return -EINVAL;

	bm_sensor->thresh_state[trip].trip_bw = bw;

	return 0;
};

static struct thermal_zone_device_ops bm_sensor_thermal_zone_ops = {
	.get_temp = bm_tz_get_curr_bw,
	.get_mode = bm_tz_get_mode,
	.get_trip_type = bm_tz_get_trip_type,
	.activate_trip_type = bm_tz_activate_trip_type,
	.get_trip_temp = bm_tz_get_trip_bw,
	.set_trip_temp = bm_tz_set_trip_bw,
};

static int bm_sensors_register_thermal_zone(void)
{
	int i = 0, ret = 0;

	for (i = 0; i < bmdev->bm_sensor_cnt; i++) {
		struct bm_tz_sensor *bm_sensor =
			&bmdev->bm_tz_sensors[i];

		bm_sensor->mode = THERMAL_DEVICE_ENABLED;
		bm_sensor->tz_dev =
			thermal_zone_device_register(bm_sensor->bm_sensor,
				MAX_BM_THRESHOLD, BM_WRITABLE_TRIPS_MASK,
				bm_sensor, &bm_sensor_thermal_zone_ops,
				NULL, 0, 0);
			if (IS_ERR(bm_sensor->tz_dev)) {
				ret = PTR_ERR(bm_sensor->tz_dev);
				pr_err(
				"%s:thermal zone register failed. ret:%d\n",
				__func__, ret);
				goto fail;
			}
	}
fail:
	return ret;
}

static void bm_sensors_unregister_thermal_zone(void)
{
	int i = 0;

	for (i = 0; i < bmdev->bm_sensor_cnt; i++) {
		struct bm_tz_sensor *bm_sensor =
			&bmdev->bm_tz_sensors[i];
		if (bm_sensor->tz_dev)
			thermal_zone_device_unregister(bm_sensor->tz_dev);
		bm_sensor->mode = THERMAL_DEVICE_DISABLED;
	}
}

static int read_device_tree_data(struct platform_device *pdev,
					struct bm_sensors_drv_data *bmdev)
{
	int bm_sensor_cnt = 0, i = 0, err = 0, idx = 0;
	char *key = NULL;
	const char *src_f = NULL;
	struct device_node *node = pdev->dev.of_node, *child_node = NULL;
	struct bm_tz_sensor *bm_tz_sensors = NULL;
	struct device_node *bus_node = NULL;

	bm_sensor_cnt = of_get_child_count(node);
	if (bm_sensor_cnt == 0) {
		pr_err("No child node found\n");
		err = -ENODEV;
		goto read_node_fail;
	}

	ALLOC_KMEM(&pdev->dev, bm_tz_sensors, struct bm_tz_sensor,
			bm_sensor_cnt, err);
	if (err)
		goto read_node_fail;

	for_each_child_of_node(node, child_node) {

		snprintf(bm_tz_sensors[i].bm_sensor,
			THERMAL_NAME_LENGTH, child_node->name);

		key = "qcom,bm-sensor";
		if (!of_get_property(child_node, key, &bm_tz_sensors[i].src_cnt)
			|| bm_tz_sensors[i].src_cnt <= 0) {
			pr_err("src id phandles are not defined\n");
			err = -ENODEV;
			goto read_node_fail;
		}

		bm_tz_sensors[i].src_cnt /= sizeof(__be32);
		ALLOC_KMEM(&pdev->dev, bm_tz_sensors[i].src_id, int,
			bm_tz_sensors[i].src_cnt, err);
		if (err)
			goto read_node_fail;

		for (idx = 0; idx < bm_tz_sensors[i].src_cnt; idx++) {
			bus_node = of_parse_phandle(child_node, key, idx);
			if (!bus_node) {
				pr_err(
				"No src id phandle is defined with index:%d\n",
				idx);
				err = -ENODEV;
				goto read_node_fail;
			}
			err = of_property_read_u32(bus_node, "cell-id",
				&bm_tz_sensors[i].src_id[idx]);
			if (err) {
				pr_err("Bus node is missing cell-id. err:%d\n",
					err);
				goto read_node_fail;
			}
		}

		key = "qcom,bm-sensor-field";
		err = of_property_read_string(child_node,
				key, &src_f);
		if (err) {
			pr_err("No %s field is found. err:%d\n", key, err);
			goto read_node_fail;
		}
		if (!strcmp(src_f, "ab")) {
			bm_tz_sensors[i].src_field = FLD_AB;
		} else if (!strcmp(src_f, "ib")) {
			bm_tz_sensors[i].src_field = FLD_IB;
		} else {
			pr_err("Unknown src field\n");
			err = -EINVAL;
			goto read_node_fail;
		}
		i++;
	}
	bmdev->bm_tz_sensors = bm_tz_sensors;
	bmdev->bm_sensor_cnt = bm_sensor_cnt;

read_node_fail:
	return err;
}

static void bus_rules_unregister(void)
{
	int i = 0;

	for (; i < bmdev->bm_sensor_cnt; i++) {
		struct bm_tz_sensor *bm_sens =
				&bmdev->bm_tz_sensors[i];

		if (bm_sens->bus_reg_done) {
			msm_rule_unregister(MAX_BM_THRESHOLD,
				bm_sens->thresh_bus_rule,
				&bm_sens->bm_nb);
			bm_sens->bus_reg_done = false;
		}
	}
}

static int init_bm_default_rules(void)
{
	int ret = 0, i = 0;

	for (i = 0; i < bmdev->bm_sensor_cnt; i++) {
		struct bm_tz_sensor *bm_sens = &bmdev->bm_tz_sensors[i];

		bitmap_zero(bm_sens->triggered, MAX_BM_THRESHOLD);
		ret = create_bus_rule(
			&bm_sens->thresh_bus_rule[BM_HIGH_THRESHOLD],
			bm_sens->src_cnt, bm_sens->src_id,
			bm_sens->src_field, OP_GT, ULLONG_MAX,
			&bm_sens->bus_rule_action[BM_HIGH_THRESHOLD]);
		if (ret) {
			pr_err("Creating bus rule is failed. ret:%d\n", ret);
			goto exit;
		}

		ret = create_bus_rule(
			&bm_sens->thresh_bus_rule[BM_LOW_THRESHOLD],
			bm_sens->src_cnt, bm_sens->src_id,
			bm_sens->src_field, OP_LT, 0,
			&bm_sens->bus_rule_action[BM_LOW_THRESHOLD]);
		if (ret) {
			pr_err("Creating bus rule is failed. ret:%d\n", ret);
			goto exit;
		}

		bm_sens->bm_nb.notifier_call = bm_threshold_cb;
		msm_rule_register(MAX_BM_THRESHOLD, bm_sens->thresh_bus_rule,
			&bm_sens->bm_nb);
		msm_rule_evaluate_rules(
			bm_sens->thresh_bus_rule[BM_HIGH_THRESHOLD].src_id[0]);
		bm_sens->bus_reg_done = true;
	}
exit:
	if (ret)
		bus_rules_unregister();
	return ret;
}

static void cleanup_drv_data(void)
{
	if (bmdev) {
		bm_sensors_unregister_thermal_zone();
		bus_rules_unregister();
		flush_work(&bmdev->bm_wq);
	}
}

static int bm_tz_sensors_probe(struct platform_device *pdev)
{
	int ret = 0;

	ALLOC_KMEM(&pdev->dev, bmdev, struct bm_sensors_drv_data,
			1, ret);
	if (ret)
		goto probe_exit;

	bmdev->pdev = pdev;
	ret = read_device_tree_data(pdev, bmdev);
	if (ret)
		goto probe_exit;

	mutex_init(&bmdev->rule_lock);
	INIT_WORK(&bmdev->bm_wq, bm_tz_notify_wq);

	ret = init_bm_default_rules();
	if (ret)
		goto probe_exit;

	ret = bm_sensors_register_thermal_zone();
	if (ret)
		goto probe_exit;

	platform_set_drvdata(pdev, bmdev);
probe_exit:
	if (ret)
		cleanup_drv_data();

	return ret;
}

static int bm_tz_sensors_remove(struct platform_device *pdev)
{
	cleanup_drv_data();
	return 0;
}

struct of_device_id bm_sensors_match[] = {
	{
		.compatible = "qcom,bm-sensors",
	},
	{},
};

static struct platform_driver bm_sensors_driver = {
	.probe  = bm_tz_sensors_probe,
	.remove = bm_tz_sensors_remove,
	.driver = {
		.name           = BM_SENSORS_DRIVER_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = bm_sensors_match,
	},
};

int __init bm_sensors_driver_init(void)
{
	return platform_driver_register(&bm_sensors_driver);
}

static void __exit bm_sensors_driver_exit(void)
{
	platform_driver_unregister(&bm_sensors_driver);
}

late_initcall(bm_sensors_driver_init);
module_exit(bm_sensors_driver_exit);

MODULE_DESCRIPTION("BANDWIDTH TZ SENSORS");
MODULE_ALIAS("platform:" BM_SENSORS_DRIVER_NAME);
MODULE_LICENSE("GPL v2");
