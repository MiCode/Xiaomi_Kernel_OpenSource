/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/of.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <soc/qcom/subsystem_notif.h>
#include <linux/rpmsg/qcom_glink.h>
#include <linux/rpmsg.h>
#include <linux/ipc_logging.h>


#define GLINK_PROBE_LOG_PAGE_CNT 4
static void *glink_ilc;

#define GLINK_INFO(x, ...)						       \
do {									       \
	if (glink_ilc)							       \
		ipc_log_string(glink_ilc, "[%s]: "x, __func__, ##__VA_ARGS__); \
} while (0)

#define GLINK_ERR(dev, x, ...)						       \
do {									       \
	dev_err(dev, "[%s]: "x, __func__, ##__VA_ARGS__);		       \
	if (glink_ilc)							       \
		ipc_log_string(glink_ilc, "[%s]: "x, __func__, ##__VA_ARGS__); \
} while (0)


#define GLINK_SSR_DO_CLEANUP	0
#define GLINK_SSR_CLEANUP_DONE	1
#define GLINK_SSR_PRIORITY	1
#define GLINK_SSR_REPLY_TIMEOUT	HZ

struct do_cleanup_msg {
	__le32 version;
	__le32 command;
	__le32 seq_num;
	__le32 name_len;
	char name[32];
};

struct cleanup_done_msg {
	__le32 version;
	__le32 response;
	__le32 seq_num;
};

struct glink_ssr_nb {
	struct list_head list;
	struct glink_ssr *ssr;
	void *ssr_register_handle;

	const char *glink_label;
	const char *ssr_label;

	struct notifier_block nb;
};

struct glink_ssr {
	struct device *dev;
	struct rpmsg_endpoint *ept;

	struct list_head notify_list;

	u32 seq_num;
	struct completion completion;
	struct work_struct unreg_work;
};

struct edge_info {
	struct list_head list;
	struct device *dev;
	struct device_node *node;

	const char *glink_label;
	const char *ssr_label;
	void *glink;

	int (*register_fn)(struct edge_info *);
	void (*unregister_fn)(struct edge_info *);
	struct notifier_block nb;
};
LIST_HEAD(edge_infos);

static void glink_ssr_ssr_unreg_work(struct work_struct *work)
{
	struct glink_ssr *ssr = container_of(work, struct glink_ssr,
					     unreg_work);
	struct glink_ssr_nb *nb, *tmp;

	list_for_each_entry_safe(nb, tmp, &ssr->notify_list, list) {
		subsys_notif_unregister_notifier(nb->ssr_register_handle,
						 &nb->nb);
		kfree(nb);
	}
	kfree(ssr);
}

static int glink_ssr_ssr_cb(struct notifier_block *this,
			    unsigned long code, void *data)
{
	struct glink_ssr_nb *nb = container_of(this, struct glink_ssr_nb, nb);
	struct glink_ssr *ssr = nb->ssr;
	struct device *dev = ssr->dev;
	struct do_cleanup_msg msg;
	int ret;

	if (!dev || !ssr->ept)
		return NOTIFY_DONE;

	if (code == SUBSYS_AFTER_SHUTDOWN) {
		ssr->seq_num++;
		reinit_completion(&ssr->completion);

		memset(&msg, 0, sizeof(msg));
		msg.command = cpu_to_le32(GLINK_SSR_DO_CLEANUP);
		msg.seq_num = cpu_to_le32(ssr->seq_num);
		msg.name_len = cpu_to_le32(strlen(nb->glink_label));
		strlcpy(msg.name, nb->glink_label, sizeof(msg.name));

		GLINK_INFO("%s: notify of %s seq_num:%d\n",
			   dev->parent->of_node->name, nb->glink_label,
			   ssr->seq_num);

		ret = rpmsg_send(ssr->ept, &msg, sizeof(msg));
		if (ret) {
			GLINK_ERR(dev, "fail to send do cleanup to %s %d\n",
				  nb->ssr_label, ret);
			return NOTIFY_DONE;
		}

		ret = wait_for_completion_timeout(&ssr->completion, HZ);
		if (!ret)
			GLINK_ERR(dev, "timeout waiting for cleanup resp\n");

	}
	return NOTIFY_DONE;
}

static int glink_ssr_callback(struct rpmsg_device *rpdev,
			      void *data, int len, void *priv, u32 addr)
{
	struct cleanup_done_msg *msg = data;
	struct glink_ssr *ssr = dev_get_drvdata(&rpdev->dev);

	if (len < sizeof(*msg)) {
		GLINK_ERR(ssr->dev, "message too short\n");
		return -EINVAL;
	}

	if (le32_to_cpu(msg->version) != 0) {
		GLINK_ERR(ssr->dev, "invalid version\n");
		return -EINVAL;
	}

	if (le32_to_cpu(msg->response) != GLINK_SSR_CLEANUP_DONE)
		return 0;

	if (le32_to_cpu(msg->seq_num) != ssr->seq_num) {
		GLINK_ERR(ssr->dev, "invalid response sequence number %d\n",
			  msg->seq_num);
		return -EINVAL;
	}

	complete(&ssr->completion);

	GLINK_INFO("%s: received seq_num:%d\n", ssr->dev->parent->of_node->name,
		   le32_to_cpu(msg->seq_num));

	return 0;
}

static void glink_ssr_init_notify(struct glink_ssr *ssr)
{
	struct device *dev = ssr->dev;
	struct device_node *node;
	struct glink_ssr_nb *nb;
	void *handle;
	int ret;
	int i = 0;

	while (1) {
		node = of_parse_phandle(dev->of_node, "qcom,notify-edges", i++);
		if (!node)
			break;

		nb = kzalloc(sizeof(*nb), GFP_KERNEL);
		if (!nb)
			return;

		ret = of_property_read_string(node, "label", &nb->ssr_label);
		if (ret < 0)
			nb->ssr_label = node->name;

		ret = of_property_read_string(node, "qcom,glink-label",
					      &nb->glink_label);
		if (ret < 0) {
			GLINK_ERR(dev, "no qcom,glink-label for %s\n",
				  nb->ssr_label);
			kfree(nb);
			continue;
		}

		nb->nb.notifier_call = glink_ssr_ssr_cb;
		nb->nb.priority = GLINK_SSR_PRIORITY;

		handle = subsys_notif_register_notifier(nb->ssr_label, &nb->nb);
		if (IS_ERR_OR_NULL(handle)) {
			GLINK_ERR(dev, "register fail for %s SSR notifier\n",
				  nb->ssr_label);
			kfree(nb);
			continue;
		}

		nb->ssr = ssr;
		nb->ssr_register_handle = handle;
		list_add_tail(&nb->list, &ssr->notify_list);
	}
}

static int glink_ssr_probe(struct rpmsg_device *rpdev)
{
	struct glink_ssr *ssr;

	ssr = kzalloc(sizeof(*ssr), GFP_KERNEL);
	if (!ssr)
		return -ENOMEM;

	INIT_LIST_HEAD(&ssr->notify_list);
	init_completion(&ssr->completion);
	INIT_WORK(&ssr->unreg_work, glink_ssr_ssr_unreg_work);

	ssr->dev = &rpdev->dev;
	ssr->ept = rpdev->ept;

	glink_ssr_init_notify(ssr);

	dev_set_drvdata(&rpdev->dev, ssr);

	return 0;
}

static void glink_ssr_remove(struct rpmsg_device *rpdev)
{
	struct glink_ssr *ssr = dev_get_drvdata(&rpdev->dev);

	ssr->dev = NULL;
	ssr->ept = NULL;
	dev_set_drvdata(&rpdev->dev, NULL);

	schedule_work(&ssr->unreg_work);
}

static const struct rpmsg_device_id glink_ssr_match[] = {
	{ "glink_ssr" },
	{}
};

static struct rpmsg_driver glink_ssr_driver = {
	.probe = glink_ssr_probe,
	.remove = glink_ssr_remove,
	.callback = glink_ssr_callback,
	.id_table = glink_ssr_match,
	.drv = {
		.name = "glink_ssr",
	},
};
module_rpmsg_driver(glink_ssr_driver);

static int glink_probe_ssr_cb(struct notifier_block *this,
			      unsigned long code, void *data)
{
	struct edge_info *einfo = container_of(this, struct edge_info, nb);

	GLINK_INFO("received %d for %s", code, einfo->ssr_label);

	switch (code) {
	case SUBSYS_AFTER_POWERUP:
		einfo->register_fn(einfo);
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		einfo->unregister_fn(einfo);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static int glink_probe_smem_reg(struct edge_info *einfo)
{
	struct device *dev = einfo->dev;

	einfo->glink = qcom_glink_smem_register(dev, einfo->node);
	if (IS_ERR_OR_NULL(einfo->glink)) {
		GLINK_ERR(dev, "register failed for %s\n", einfo->ssr_label);
		einfo->glink = NULL;
	}
	GLINK_INFO("register successful for %s\n", einfo->ssr_label);

	return 0;
}

static void glink_probe_smem_unreg(struct edge_info *einfo)
{
	if (einfo->glink)
		qcom_glink_smem_unregister(einfo->glink);

	einfo->glink = NULL;
	GLINK_INFO("unregister for %s\n", einfo->ssr_label);

}

static int glink_probe_spss_reg(struct edge_info *einfo)
{
	struct device *dev = einfo->dev;

	einfo->glink = qcom_glink_spss_register(dev, einfo->node);
	if (IS_ERR_OR_NULL(einfo->glink)) {
		GLINK_ERR(dev, "register failed for %s\n", einfo->ssr_label);
		einfo->glink = NULL;
	}
	GLINK_INFO("register successful for %s\n", einfo->ssr_label);

	return 0;
}

static void glink_probe_spss_unreg(struct edge_info *einfo)
{
	if (einfo->glink)
		qcom_glink_spss_unregister(einfo->glink);

	einfo->glink = NULL;
	GLINK_INFO("unregister for %s\n", einfo->ssr_label);
}

static void probe_subsystem(struct device *dev, struct device_node *np)
{
	struct edge_info *einfo;
	const char *transport;
	void *handle;
	int ret;

	einfo = devm_kzalloc(dev, sizeof(*einfo), GFP_KERNEL);
	if (!einfo)
		return;

	ret = of_property_read_string(np, "label", &einfo->ssr_label);
	if (ret < 0)
		einfo->ssr_label = np->name;

	ret = of_property_read_string(np, "qcom,glink-label",
				      &einfo->glink_label);
	if (ret < 0) {
		GLINK_ERR(dev, "no qcom,glink-label for %s\n",
			  einfo->ssr_label);
		goto free_einfo;
	}

	einfo->dev = dev;
	einfo->node = np;

	ret = of_property_read_string(np, "transport", &transport);
	if (ret < 0) {
		GLINK_ERR(dev, "%s missing transport\n", einfo->ssr_label);
		goto free_einfo;
	}

	if (!strcmp(transport, "smem")) {
		einfo->register_fn = glink_probe_smem_reg;
		einfo->unregister_fn = glink_probe_smem_unreg;
	} else if (!strcmp(transport, "spss")) {
		einfo->register_fn = glink_probe_spss_reg;
		einfo->unregister_fn = glink_probe_spss_unreg;
	} else if (!strcmp(transport, "spi")) {
		/* SPI SSR is self contained */
		einfo->glink = qcom_glink_spi_register(dev, np);
		if (IS_ERR_OR_NULL(einfo->glink)) {
			GLINK_ERR(dev, "%s failed\n", einfo->ssr_label);
			goto free_einfo;
		}
		list_add_tail(&einfo->list, &edge_infos);
		return;
	}

	einfo->nb.notifier_call = glink_probe_ssr_cb;

	handle = subsys_notif_register_notifier(einfo->ssr_label, &einfo->nb);
	if (IS_ERR_OR_NULL(handle)) {
		GLINK_ERR(dev, "could not register for SSR notifier for %s\n",
			  einfo->ssr_label);
		goto free_einfo;
	}

	list_add_tail(&einfo->list, &edge_infos);
	GLINK_INFO("probe successful for %s\n", einfo->ssr_label);

	return;

free_einfo:
	devm_kfree(dev, einfo);
	return;
}

static int glink_probe(struct platform_device *pdev)
{
	struct device_node *pn = pdev->dev.of_node;
	struct device_node *cn;

	for_each_available_child_of_node(pn, cn) {
		probe_subsystem(&pdev->dev, cn);
	}
	return 0;
}

static const struct of_device_id glink_match_table[] = {
	{ .compatible = "qcom,glink" },
	{},
};

static struct platform_driver glink_probe_driver = {
	.probe = glink_probe,
	.driver = {
		.name = "msm_glink",
		.owner = THIS_MODULE,
		.of_match_table = glink_match_table,
	},
};

static int __init glink_probe_init(void)
{
	int ret;

	glink_ilc = ipc_log_context_create(GLINK_PROBE_LOG_PAGE_CNT,
					   "glink_probe", 0);

	ret = platform_driver_register(&glink_probe_driver);
	if (ret) {
		pr_err("%s: glink_probe register failed %d\n",
			__func__, ret);
		return ret;
	}

	return 0;
}
arch_initcall(glink_probe_init);

MODULE_DESCRIPTION("Qualcomm GLINK probe helper driver");
MODULE_LICENSE("GPL v2");
