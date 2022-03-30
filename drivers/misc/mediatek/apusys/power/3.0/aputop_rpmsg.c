// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/rpmsg.h>

#include "apu_top.h"
#include "aputop_rpmsg.h"

static struct aputop_rpmsg {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
	struct mutex send_lock;
	struct completion comp;
	enum aputop_rpmsg_cmd curr_rpmsg_cmd;
	int initialized;
} *top_rpmsg;

enum aputop_rpmsg_cmd get_curr_rpmsg_cmd(void)
{
	if (IS_ERR_OR_NULL(top_rpmsg) || !top_rpmsg->initialized) {
		pr_info("%s: rpmsg not ready yet\n", __func__);
		return APUTOP_RPMSG_CMD_MAX;
	}

	return top_rpmsg->curr_rpmsg_cmd;
}

/* send a top_rpmsg message to remote side */
int aputop_send_rpmsg(struct aputop_rpmsg_data *rpmsg_data, int timeout) // ms
{
	int ret;

	if (IS_ERR_OR_NULL(top_rpmsg) || !top_rpmsg->initialized) {
		pr_info("%s: failed to send msg to remote side\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&top_rpmsg->send_lock);

	reinit_completion(&top_rpmsg->comp);

	top_rpmsg->curr_rpmsg_cmd = rpmsg_data->cmd;
	ret = rpmsg_send(top_rpmsg->ept, (void *)rpmsg_data,
			sizeof(struct aputop_rpmsg_data));
	if (ret) {
		pr_info("%s: failed to send msg to remote side, ret=%d\n",
				__func__, ret);
		goto unlock;
	}

	if (timeout > 0) {
		ret = wait_for_completion_interruptible_timeout(
				&top_rpmsg->comp,
				msecs_to_jiffies(timeout));

		if (ret < 0) {
			pr_info("%s waiting for ack interrupted, ret : %d\n",
					__func__, ret);
			goto unlock;
		}

		if (ret == 0) {
			pr_info("%s waiting for ack timeout\n", __func__);
			ret = -ETIMEDOUT;
			goto unlock;
		}

		ret = 0;
	}

unlock:
	mutex_unlock(&top_rpmsg->send_lock);

	return ret;
}

/* receive reply data from remote */
static int aputop_rpmsg_callback(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(top_rpmsg) || !top_rpmsg->initialized) {
		pr_info("%s: failed to send msg to remote side\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (pwr_data != NULL &&
		pwr_data->plat_rpmsg_callback != NULL) {
		ret = pwr_data->plat_rpmsg_callback(
					(int)top_rpmsg->curr_rpmsg_cmd,
					data, len, priv, src);
	}
out:
	complete(&top_rpmsg->comp);
	return ret;
}

void test_ipi_wakeup_apu(void)
{
	struct aputop_rpmsg_data rpmsg_data;

	pr_info("%s ++\n", __func__);
	memset(&rpmsg_data, 0, sizeof(struct aputop_rpmsg_data));
	rpmsg_data.cmd = APUTOP_CURR_STATUS;
	rpmsg_data.data0 = 0x0; // pseudo data
	aputop_send_rpmsg(&rpmsg_data, 100);
	pr_info("%s --\n", __func__);
}

static int aputop_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_channel_info chinfo = {};
	struct rpmsg_endpoint *ept;
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n", __func__,
			rpdev->id.name, rpdev->src);

	top_rpmsg = devm_kzalloc(dev, sizeof(struct aputop_rpmsg), GFP_KERNEL);
	if (!top_rpmsg)
		return -ENOMEM;

	memset(top_rpmsg, 0, sizeof(struct aputop_rpmsg));

	strscpy(chinfo.name, rpdev->id.name, RPMSG_NAME_SIZE);
	chinfo.src = rpdev->src;
	chinfo.dst = RPMSG_ADDR_ANY;
	ept = rpmsg_create_ept(rpdev, aputop_rpmsg_callback, NULL, chinfo);
	if (!ept) {
		dev_info(dev, "failed to create ept\n");
		return -ENODEV;
	}

	init_completion(&top_rpmsg->comp);
	mutex_init(&top_rpmsg->send_lock);
	top_rpmsg->ept = ept;
	top_rpmsg->rpdev = rpdev;
	top_rpmsg->initialized = 1;

	dev_set_drvdata(dev, top_rpmsg);

	return 0;
}

static void aputop_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct aputop_rpmsg *top_rpmsg;

	top_rpmsg = dev_get_drvdata(&rpdev->dev);
	rpmsg_destroy_ept(top_rpmsg->ept);
}

static const struct of_device_id aputop_rpmsg_of_match[] = {
	{ .compatible = "mediatek,aputop-rpmsg", },
	{ },
};

static struct rpmsg_driver aputop_rpmsg_drv = {
	.drv	= {
		.name	= "apu_top_3_rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = aputop_rpmsg_of_match,
	},
	.probe = aputop_rpmsg_probe,
	.remove = aputop_rpmsg_remove,
};

int aputop_register_rpmsg(void)
{
	return register_rpmsg_driver(&aputop_rpmsg_drv);
}

void aputop_unregister_rpmsg(void)
{
	unregister_rpmsg_driver(&aputop_rpmsg_drv);
}
