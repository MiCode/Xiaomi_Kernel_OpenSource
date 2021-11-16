// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/sde_vm_event.h>
#include "msm_drv.h"

int msm_register_vm_event(struct device *dev, struct device *client_dev,
			  struct msm_vm_ops *ops, void *priv_data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *ddev = platform_get_drvdata(pdev);
	struct msm_drm_private *priv = ddev->dev_private;
	struct msm_vm_client_entry *client_entry;

	if (!client_dev || !ops)
		return -EINVAL;

	client_entry = kzalloc(sizeof(*client_entry), GFP_KERNEL);
	if (!client_entry)
		return -ENOMEM;

	mutex_lock(&priv->vm_client_lock);

	memcpy(&client_entry->ops, ops, sizeof(*ops));
	client_entry->dev = client_dev;
	client_entry->data = priv_data;

	list_add(&client_entry->list, &priv->vm_client_list);

	mutex_unlock(&priv->vm_client_lock);

	return 0;
}
EXPORT_SYMBOL(msm_register_vm_event);

void msm_unregister_vm_event(struct device *dev, struct device *client_dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *ddev = platform_get_drvdata(pdev);
	struct msm_drm_private *priv = ddev->dev_private;
	struct msm_vm_client_entry *client_entry, *tmp;

	mutex_lock(&priv->vm_client_lock);

	list_for_each_entry_safe(client_entry, tmp, &priv->vm_client_list,
				 list) {
		if (client_entry->dev == client_dev) {
			list_del(&client_entry->list);
			kfree(client_entry);
			break;
		}
	}

	mutex_unlock(&priv->vm_client_lock);
}
EXPORT_SYMBOL(msm_unregister_vm_event);
