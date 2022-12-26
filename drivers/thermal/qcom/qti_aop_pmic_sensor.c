// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/thermal.h>
#include <linux/module.h>
#include <linux/regmap.h>

#define AOP_PMIC_SENSOR_DRIVER "aop-pmic-sensor-driver"
#define MBOX_NAMES "aop"
#define MSG_FORMAT "{class: pmic_data, dbu_id: %d, val: %s}"
#define MSG_MAX_LEN 64
#define MBOX_TOUT_MS 1000
#define PMIC_RAIL_NAME_LENGTH 10

struct pmic_rails {
	int pmic_id;
	int type;
	char resource_name[PMIC_RAIL_NAME_LENGTH];
};

struct pmic_stats {
	u32 pmic_id;
	u32 volt;
	u32 temp;
};

struct aop_pmic_sensor_device {
	void __iomem *regmap;
	struct device *dev;
	struct resource *res;
	struct mbox_chan *mbox_chan;
	struct mbox_client *client;
	struct qmp *qmp;
	char resource_name[PMIC_RAIL_NAME_LENGTH];
	int ss_count;
	struct mutex mutex;
	struct aop_pmic_sensor_hwmon_state *hwmon_st;
	struct pmic_rails pmic_rail[0];
};

struct aop_pmic_sensor_peripheral_data {
	int last_temp_reading;
	int last_volt_reading;
	struct pmic_rails *pmic_rail;
	struct thermal_zone_device *tz_dev;
	struct aop_pmic_sensor_device *dev;
};

struct aop_pmic_sensor_hwmon_state {
	int num_channels;
	struct attribute_group attr_group;
	const struct attribute_group *groups[2];
	struct aop_pmic_sensor_peripheral_data *aop_psens_perph;
	struct attribute **attrs;
};

struct aop_msg {
	uint32_t len;
	void *msg;
};

enum rail_type {
	TEMP_TYPE,
	VOLT_TYPE,
};

enum volt_type {
	MSS_VOLT = 1,
	MX_VOLT,
	CX_VOLT,
};

static int qmp_send_msg(struct aop_pmic_sensor_device *aop_psens_dev, const char *resource_name,
				int pmic_id)
{
	int ret = 0;
	struct aop_msg msg;
	char msg_buf[MSG_MAX_LEN] = {0};

	if (!aop_psens_dev->mbox_chan) {
		pr_err("mbox not initialized for resource:%s\n", aop_psens_dev->resource_name);
		return -EINVAL;
	}

	ret = scnprintf(msg_buf, MSG_MAX_LEN, MSG_FORMAT, pmic_id, resource_name);

	if (ret >= MSG_MAX_LEN) {
		pr_err("Message too long for resource:%s\n", resource_name);
		return -E2BIG;
	}
	msg.len = MSG_MAX_LEN;
	msg.msg = msg_buf;
	ret = mbox_send_message(aop_psens_dev->mbox_chan, &msg);

	return (ret < 0) ? ret : 0;
}

static int qmp_read_data(struct aop_pmic_sensor_peripheral_data *aop_psens_perph,
				const char *value, u32 offset)
{
	int ret = 0;
	int idx;
	struct aop_pmic_sensor_device *aop_psens_dev = aop_psens_perph->dev;

	mutex_lock(&aop_psens_dev->mutex);
	ret = qmp_send_msg(aop_psens_dev, value, aop_psens_perph->pmic_rail->pmic_id);
	if (ret < 0) {
		pr_err("failed to send the QMP message, ret =%d\n", ret);
		mutex_unlock(&aop_psens_dev->mutex);
		return ret;
	}

	idx = aop_psens_perph->pmic_rail->pmic_id - 1;

	ret = readl_relaxed(aop_psens_dev->regmap + (idx * sizeof(struct pmic_stats))
				+ offset);
	mutex_unlock(&aop_psens_dev->mutex);

	return ret;
}

static int aop_psens_read_temp(void *data, int *temp)
{
	int curr_temp;
	struct aop_pmic_sensor_peripheral_data *aop_psens_perph =
				(struct aop_pmic_sensor_peripheral_data *)data;

	curr_temp = qmp_read_data(aop_psens_perph, "temp", offsetof(struct pmic_stats, temp));
	if (curr_temp < 0) {
		*temp = aop_psens_perph->last_temp_reading;
	} else {
		/* converting temperature in millidegree */
		*temp = curr_temp * 1000;
	}
	aop_psens_perph->last_temp_reading = *temp;

	return 0;
}

static struct thermal_zone_of_device_ops aop_psens_temp_device_ops = {
	.get_temp = aop_psens_read_temp,
};

static int aop_psens_probe_temp(struct platform_device *pdev,
	struct aop_pmic_sensor_device *aop_psens_dev)
{
	int ret;
	int sensor_id = 0, i = 0;
	struct device *dev = &pdev->dev;
	struct aop_pmic_sensor_peripheral_data *aop_psens_temp;

	for (i = 0 ; i < aop_psens_dev->ss_count ; i++) {
		if (aop_psens_dev->pmic_rail[i].type == TEMP_TYPE) {
			aop_psens_temp = devm_kzalloc(dev, sizeof(*aop_psens_temp), GFP_KERNEL);
			if (!aop_psens_temp) {
				ret = -ENOMEM;
				return ret;
			}

			aop_psens_temp->dev = aop_psens_dev;
			aop_psens_temp->pmic_rail = &aop_psens_dev->pmic_rail[i];
			sensor_id = aop_psens_dev->pmic_rail[i].pmic_id;

			aop_psens_temp->tz_dev = devm_thermal_zone_of_sensor_register(&pdev->dev,
				sensor_id, aop_psens_temp, &aop_psens_temp_device_ops);
			if (IS_ERR(aop_psens_temp->tz_dev)) {
				pr_debug("aop pmic sensor [%s] thermal zone registration failed. %d\n",
				aop_psens_dev->pmic_rail[i].resource_name,
				PTR_ERR(aop_psens_temp->tz_dev));
				aop_psens_temp->tz_dev = NULL;
			}
		}
	}

	return 0;
}

static ssize_t aop_psens_volt_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int volt, pmic_id;
	struct sensor_device_attribute *sattr = to_sensor_dev_attr(attr);
	struct aop_pmic_sensor_hwmon_state *state = dev_get_drvdata(dev);

	pmic_id = sattr->index;
	state->aop_psens_perph->pmic_rail->pmic_id = pmic_id;

	volt = qmp_read_data(state->aop_psens_perph, "volt", offsetof(struct pmic_stats, volt));

	/* voltage is in milli volt */
	state->aop_psens_perph->last_volt_reading = volt;

	return scnprintf(buf, PAGE_SIZE, "%d\n", volt);
}

static int aop_psens_probe_volt(struct platform_device *pdev,
			struct aop_pmic_sensor_device *aop_psens_dev, int channels)
{
	int ret, i;
	int idx = 0;
	struct device *dev = &pdev->dev;
	struct aop_pmic_sensor_hwmon_state *st;
	struct sensor_device_attribute *a;
	struct device *hwmon_dev;

	st = devm_kzalloc(dev, sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->num_channels = channels;
	st->attrs = devm_kcalloc(dev, st->num_channels, sizeof(*st->attrs), GFP_KERNEL);
	if (st->attrs == NULL)
		return -ENOMEM;

	for (i = 0; i < aop_psens_dev->ss_count; i++) {
		if (aop_psens_dev->pmic_rail[i].type == VOLT_TYPE) {
			a = devm_kzalloc(dev, sizeof(*a), GFP_KERNEL);
			if (a == NULL)
				return -ENOMEM;

			sysfs_attr_init(&a->dev_attr.attr);

			st->aop_psens_perph = devm_kzalloc(dev, sizeof(*st->aop_psens_perph),
							GFP_KERNEL);
			if (st->aop_psens_perph == NULL)
				return -ENOMEM;

			st->aop_psens_perph->dev = aop_psens_dev;
			st->aop_psens_perph->pmic_rail = &aop_psens_dev->pmic_rail[i];

			a->dev_attr.attr.name = devm_kasprintf(dev, GFP_KERNEL, "%s",
						aop_psens_dev->pmic_rail[i].resource_name);
			if (a->dev_attr.attr.name == NULL)
				return -ENOMEM;

			a->dev_attr.show = aop_psens_volt_read;
			a->dev_attr.attr.mode = 0444;
			a->index = idx + 1;
			st->attrs[idx] = &a->dev_attr.attr;

			idx++;
		}
	}

	st->attr_group.attrs = st->attrs;
	st->groups[0] = &st->attr_group;
	aop_psens_dev->hwmon_st = st;

	hwmon_dev = devm_hwmon_device_register_with_groups(dev, "qti_aop_pmic_sensor",
			st, st->groups);
	if (IS_ERR_OR_NULL(hwmon_dev)) {
		ret = PTR_ERR(hwmon_dev);
		pr_err("failed to register hwmon device for qti_aop_pmic_sensor, ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int aop_psens_init_mbox(struct platform_device *pdev,
				struct aop_pmic_sensor_device *aop_psens_dev)
{
	aop_psens_dev->client = devm_kzalloc(&pdev->dev, sizeof(*aop_psens_dev->client),
				GFP_KERNEL);
	if (!aop_psens_dev->client)
		return -ENOMEM;

	aop_psens_dev->client->dev = &pdev->dev;
	aop_psens_dev->client->tx_block = true;
	aop_psens_dev->client->tx_tout = MBOX_TOUT_MS;
	aop_psens_dev->client->knows_txdone = false;

	aop_psens_dev->mbox_chan = mbox_request_channel(aop_psens_dev->client, 0);
	if (IS_ERR(aop_psens_dev->mbox_chan)) {
		dev_err(&pdev->dev, "Mbox request failed. err:%ld\n",
				PTR_ERR(aop_psens_dev->mbox_chan));
		return PTR_ERR(aop_psens_dev->mbox_chan);
	}

	return 0;
}

static int aop_pmic_parse_dt(struct device *dev, struct platform_device *pdev,
		struct aop_pmic_sensor_device *aop_psens_dev, struct device_node *np,
		struct device_node *subsys_np)
{
	u32 val;
	int num_channels = 0, idx = 0, ret = 0;

	aop_psens_dev->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!aop_psens_dev->res) {
		dev_err(dev, "Couldn't get MEM resource\n");
		return -EINVAL;
	}

	for_each_available_child_of_node(np, subsys_np) {
		strscpy(aop_psens_dev->pmic_rail[idx].resource_name, subsys_np->name,
					PMIC_RAIL_NAME_LENGTH);

		ret = of_property_read_u32(subsys_np, "qcom,pmic-id", &val);
		if (ret < 0) {
			pr_err("Unable to parse the dt, ret = %d\n", ret);
			return ret;
		}

		aop_psens_dev->pmic_rail[idx].pmic_id = val;

		ret = of_property_read_u32(subsys_np, "qcom,type", &val);
		if (ret < 0) {
			pr_err("Unable to get type dt property, ret = %d\n", ret);
			return ret;
		}
		aop_psens_dev->pmic_rail[idx].type = val;

		if (aop_psens_dev->pmic_rail[idx].type == VOLT_TYPE)
			num_channels++;

		idx++;
	}

	return num_channels;
}

static int aop_pmic_sensor_probe(struct platform_device *pdev)
{
	int ret = 0, count = 0, num_channels = 0;
	struct device *dev = &pdev->dev;
	struct aop_pmic_sensor_device *aop_psens_dev = NULL;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *subsys_np = NULL;
	phys_addr_t pmic_stats_base;
	resource_size_t pmic_stats_size;

	count = of_get_available_child_count(np);
	if (!count) {
		dev_err(dev, "No child node to process\n");
		return -ENODEV;
	}

	aop_psens_dev = devm_kzalloc(&pdev->dev, struct_size(aop_psens_dev, pmic_rail, count),
			GFP_KERNEL);
	if (!aop_psens_dev)
		return -ENOMEM;

	ret = num_channels = aop_pmic_parse_dt(&pdev->dev, pdev, aop_psens_dev, np, subsys_np);
	if (ret < 0)
		return ret;

	aop_psens_dev->dev = &pdev->dev;
	aop_psens_dev->ss_count = count;
	mutex_init(&aop_psens_dev->mutex);
	pmic_stats_base = aop_psens_dev->res->start;
	pmic_stats_size = resource_size(aop_psens_dev->res);

	aop_psens_dev->regmap = devm_ioremap(dev, pmic_stats_base, pmic_stats_size);
	if (!aop_psens_dev->regmap) {
		dev_err(dev, "Couldn't get regmap\n");
		return -EINVAL;
	}

	ret = aop_psens_init_mbox(pdev, aop_psens_dev);
	if (ret)
		return ret;

	ret = aop_psens_probe_temp(pdev, aop_psens_dev);
	if (ret != 0) {
		pr_err("failed to register with thermal zone, ret = %d\n", ret);
		return ret;
	}

	ret = aop_psens_probe_volt(pdev, aop_psens_dev, num_channels);
	if (ret != 0) {
		pr_err("failed to register voltage hwmon device for qti-bmc, ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id aop_pmic_sensor_of_match[] = {
	{.compatible = "qcom,aop-pmic-sensor"},
	{}
};

static struct platform_driver aop_pmic_sensor_driver = {
	.driver = {
		.name = AOP_PMIC_SENSOR_DRIVER,
		.of_match_table = aop_pmic_sensor_of_match,
	},
	.probe = aop_pmic_sensor_probe,
};
module_platform_driver(aop_pmic_sensor_driver);
MODULE_DESCRIPTION("QTI AOP PMIC Sensor Driver");
MODULE_LICENSE("GPL v2");

