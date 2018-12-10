// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/esoc_client.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include "esoc.h"

static DEFINE_SPINLOCK(notify_lock);
static ATOMIC_NOTIFIER_HEAD(client_notify);

static void devm_esoc_desc_release(struct device *dev, void *res)
{
	struct esoc_desc *esoc_desc = res;

	kfree(esoc_desc->name);
	kfree(esoc_desc->link);
	put_esoc_clink(esoc_desc->priv);
}

static int devm_esoc_desc_match(struct device *dev, void *res, void *data)
{
	struct esoc_desc *esoc_desc = res;
	return esoc_desc == data;
}

struct esoc_desc *devm_register_esoc_client(struct device *dev,
							const char *name)
{
	int ret, index;
	const char *client_desc;
	char *esoc_prop;
	const __be32 *parp;
	struct device_node *esoc_node;
	struct device_node *np = dev->of_node;
	struct esoc_clink *esoc_clink;
	struct esoc_desc *desc;
	char *esoc_name, *esoc_link, *esoc_link_info;

	for (index = 0;; index++) {
		esoc_prop = kasprintf(GFP_KERNEL, "esoc-%d", index);
		if (IS_ERR_OR_NULL(esoc_prop))
			return ERR_PTR(-ENOMEM);
		parp = of_get_property(np, esoc_prop, NULL);
		if (parp == NULL) {
			dev_err(dev, "esoc device not present\n");
			kfree(esoc_prop);
			return NULL;
		}
		ret = of_property_read_string_index(np, "esoc-names", index,
								&client_desc);
		if (ret) {
			dev_err(dev, "cannot find matching string\n");
			kfree(esoc_prop);
			return NULL;
		}
		if (strcmp(client_desc, name)) {
			kfree(esoc_prop);
			continue;
		}
		kfree(esoc_prop);
		esoc_node = of_find_node_by_phandle(be32_to_cpup(parp));
		esoc_clink = get_esoc_clink_by_node(esoc_node);
		if (IS_ERR_OR_NULL(esoc_clink)) {
			dev_err(dev, "matching esoc clink not present\n");
			return ERR_PTR(-EPROBE_DEFER);
		}
		esoc_name = kasprintf(GFP_KERNEL, "esoc%d",
							esoc_clink->id);
		if (IS_ERR_OR_NULL(esoc_name)) {
			dev_err(dev, "unable to allocate esoc name\n");
			return ERR_PTR(-ENOMEM);
		}
		esoc_link = kasprintf(GFP_KERNEL, "%s", esoc_clink->link_name);
		if (IS_ERR_OR_NULL(esoc_link)) {
			dev_err(dev, "unable to allocate esoc link name\n");
			kfree(esoc_name);
			return ERR_PTR(-ENOMEM);
		}
		esoc_link_info = kasprintf(GFP_KERNEL, "%s",
					esoc_clink->link_info);
		if (IS_ERR_OR_NULL(esoc_link_info)) {
			dev_err(dev, "unable to alloc link info name\n");
			kfree(esoc_name);
			kfree(esoc_link);
			return ERR_PTR(-ENOMEM);
		}
		desc = devres_alloc(devm_esoc_desc_release,
						sizeof(*desc), GFP_KERNEL);
		if (IS_ERR_OR_NULL(desc)) {
			kfree(esoc_name);
			kfree(esoc_link);
			kfree(esoc_link_info);
			dev_err(dev, "unable to allocate esoc descriptor\n");
			return ERR_PTR(-ENOMEM);
		}
		desc->name = esoc_name;
		desc->link = esoc_link;
		desc->link_info = esoc_link_info;
		desc->priv = esoc_clink;
		devres_add(dev, desc);
		return desc;
	}
	return NULL;
}
EXPORT_SYMBOL(devm_register_esoc_client);

void devm_unregister_esoc_client(struct device *dev,
					struct esoc_desc *esoc_desc)
{
	int ret;

	ret = devres_release(dev, devm_esoc_desc_release,
				devm_esoc_desc_match, esoc_desc);
	WARN_ON(ret);
}
EXPORT_SYMBOL(devm_unregister_esoc_client);

int esoc_register_client_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&client_notify, nb);
}
EXPORT_SYMBOL(esoc_register_client_notifier);

void notify_esoc_clients(struct esoc_clink *esoc_clink, unsigned long evt)
{
	unsigned int id;
	unsigned long flags;

	spin_lock_irqsave(&notify_lock, flags);
	id = esoc_clink->id;
	atomic_notifier_call_chain(&client_notify, evt, &id);
	spin_unlock_irqrestore(&notify_lock, flags);
}
EXPORT_SYMBOL(notify_esoc_clients);

int esoc_register_client_hook(struct esoc_desc *desc,
				struct esoc_client_hook *client_hook)
{
	int i;
	struct esoc_clink *esoc_clink;

	if (IS_ERR_OR_NULL(desc) || IS_ERR_OR_NULL(client_hook)) {
		pr_debug("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	esoc_clink = desc->priv;
	if (IS_ERR_OR_NULL(esoc_clink)) {
		pr_debug("%s: Invalid esoc link\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ESOC_MAX_HOOKS; i++) {
		if (i == client_hook->prio &&
			esoc_clink->client_hook[i] == NULL) {
			esoc_clink->client_hook[i] = client_hook;
			dev_dbg(&esoc_clink->dev,
				"Client hook registration successful\n");
			return 0;
		}
	}

	dev_dbg(&esoc_clink->dev, "Client hook registration failed!\n");
	return -EINVAL;
}
EXPORT_SYMBOL(esoc_register_client_hook);

int esoc_unregister_client_hook(struct esoc_desc *desc,
				struct esoc_client_hook *client_hook)
{
	int i;
	struct esoc_clink *esoc_clink;

	if (IS_ERR_OR_NULL(desc) || IS_ERR_OR_NULL(client_hook)) {
		pr_debug("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	esoc_clink = desc->priv;
	if (IS_ERR_OR_NULL(esoc_clink)) {
		pr_debug("%s: Invalid esoc link\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ESOC_MAX_HOOKS; i++) {
		if (i == client_hook->prio &&
			esoc_clink->client_hook[i] != NULL) {
			esoc_clink->client_hook[i] = NULL;
			dev_dbg(&esoc_clink->dev,
				"Client hook unregistration successful\n");
			return 0;
		}
	}

	dev_dbg(&esoc_clink->dev, "Client hook unregistration failed!\n");
	return -EINVAL;
}
EXPORT_SYMBOL(esoc_unregister_client_hook);
