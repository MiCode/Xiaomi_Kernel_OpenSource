// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mailbox_client.h>

#define REG_CDEV_DRIVER "reg-aop-cooling-device"
#define REG_MSG_FORMAT "{class:volt_flr, event:zero_temp, res:%s, value:%s}"
#define REG_CDEV_MAX_STATE 1
#define MBOX_TOUT_MS 1000
#define REG_MSG_MAX_LEN 100

struct reg_cooling_device {
	struct thermal_cooling_device	*cdev;
	unsigned int			min_state;
	const char			*resource_name;
	struct mbox_chan		*qmp_chan;
	struct mbox_client		*client;
};

struct aop_msg {
	uint32_t len;
	void *msg;
};

enum regulator_rail_type {
	REG_COOLING_CX,
	REG_COOLING_MX,
	REG_COOLING_EBI,
	REG_COOLING_NR,
};

static char *regulator_rail[REG_COOLING_NR] = {
	"cx",
	"mx",
	"ebi",
};

static int aop_send_msg(struct reg_cooling_device *reg_dev, int min_state)
{
	char msg_buf[REG_MSG_MAX_LEN] = {0};
	int ret = 0;
	struct aop_msg msg;

	if (!reg_dev->qmp_chan) {
		pr_err("mbox not initialized for resource:%s\n",
				reg_dev->resource_name);
		return -EINVAL;
	}

	ret = snprintf(msg_buf, REG_MSG_MAX_LEN, REG_MSG_FORMAT,
			reg_dev->resource_name,
			(min_state == REG_CDEV_MAX_STATE) ? "off" : "on");
	if (ret >= REG_MSG_MAX_LEN) {
		pr_err("Message too long for resource:%s\n",
				reg_dev->resource_name);
		return -E2BIG;
	}
	msg.len = REG_MSG_MAX_LEN;
	msg.msg = msg_buf;
	ret = mbox_send_message(reg_dev->qmp_chan, &msg);

	return (ret < 0) ? ret : 0;
}

static int reg_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = REG_CDEV_MAX_STATE;
	return 0;
}

static int reg_get_min_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	struct reg_cooling_device *reg_dev = cdev->devdata;

	*state = reg_dev->min_state;
	return 0;
}

static int reg_send_min_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	struct reg_cooling_device *reg_dev = cdev->devdata;
	int ret = 0;

	if (state > REG_CDEV_MAX_STATE)
		return -EINVAL;

	if (reg_dev->min_state == state)
		return ret;

	ret = aop_send_msg(reg_dev, state);
	if (ret) {
		pr_err("regulator:%s switching to floor %lu error. err:%d\n",
			reg_dev->resource_name, state, ret);
	} else {
		pr_debug("regulator:%s switched to %lu from %d\n",
			reg_dev->resource_name, state, reg_dev->min_state);
		reg_dev->min_state = state;
	}

	return ret;
}

static int reg_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = 0;
	return 0;
}

static int reg_send_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	if (state > REG_CDEV_MAX_STATE)
		return -EINVAL;

	return 0;
}

static struct thermal_cooling_device_ops reg_dev_ops = {
	.get_max_state = reg_get_max_state,
	.get_cur_state = reg_get_cur_state,
	.set_cur_state = reg_send_cur_state,
	.set_min_state = reg_send_min_state,
	.get_min_state = reg_get_min_state,
};

static int reg_init_mbox(struct platform_device *pdev,
			struct reg_cooling_device *reg_dev)
{
	reg_dev->client = devm_kzalloc(&pdev->dev, sizeof(*reg_dev->client),
					GFP_KERNEL);
	if (!reg_dev->client)
		return -ENOMEM;

	reg_dev->client->dev = &pdev->dev;
	reg_dev->client->tx_block = true;
	reg_dev->client->tx_tout = MBOX_TOUT_MS;
	reg_dev->client->knows_txdone = false;

	reg_dev->qmp_chan = mbox_request_channel(reg_dev->client, 0);
	if (IS_ERR(reg_dev->qmp_chan)) {
		dev_err(&pdev->dev, "Mbox request failed. err:%ld\n",
				PTR_ERR(reg_dev->qmp_chan));
		return PTR_ERR(reg_dev->qmp_chan);
	}

	return 0;
}

static int reg_dev_probe(struct platform_device *pdev)
{
	int ret = 0, idx = 0;
	struct reg_cooling_device *reg_dev = NULL;

	reg_dev = devm_kzalloc(&pdev->dev, sizeof(*reg_dev), GFP_KERNEL);
	if (!reg_dev)
		return -ENOMEM;

	ret = reg_init_mbox(pdev, reg_dev);
	if (ret)
		return ret;

	ret = of_property_read_string(pdev->dev.of_node,
			"qcom,reg-resource-name",
			&reg_dev->resource_name);
	if (ret) {
		dev_err(&pdev->dev, "Error reading resource name. err:%d\n",
			ret);
		goto mbox_free;
	}

	for (idx = 0; idx < REG_COOLING_NR; idx++) {
		if (!strcmp(reg_dev->resource_name, regulator_rail[idx]))
			break;
	}
	if (idx == REG_COOLING_NR) {
		dev_err(&pdev->dev, "Invalid regulator resource name:%s\n",
				reg_dev->resource_name);
		ret = -EINVAL;
		goto mbox_free;
	}
	reg_dev->min_state = REG_CDEV_MAX_STATE;
	reg_dev->cdev = thermal_of_cooling_device_register(
				pdev->dev.of_node,
				(char *)reg_dev->resource_name,
				reg_dev, &reg_dev_ops);
	if (IS_ERR(reg_dev->cdev))
		goto mbox_free;

	return ret;

mbox_free:
	mbox_free_channel(reg_dev->qmp_chan);

	return ret;
}

static const struct of_device_id reg_dev_of_match[] = {
	{.compatible = "qcom,rpmh-reg-cdev", },
	{}
};

static struct platform_driver reg_dev_driver = {
	.driver = {
		.name = REG_CDEV_DRIVER,
		.of_match_table = reg_dev_of_match,
	},
	.probe = reg_dev_probe,
};
builtin_platform_driver(reg_dev_driver);
