/*
 * Copyright (c) 2015, 2018, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/err.h>

#include <video/msm_dba.h>
#include <msm_dba_internal.h>

static DEFINE_MUTEX(register_mutex);

void *msm_dba_register_client(struct msm_dba_reg_info *info,
			      struct msm_dba_ops *ops)
{
	int rc = 0;
	struct msm_dba_device_info *device = NULL;
	struct msm_dba_client_info *client = NULL;

	pr_debug("%s: ENTER\n", __func__);

	if (!info || !ops) {
		pr_err("%s: Invalid params\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	mutex_lock(&register_mutex);

	pr_debug("%s: Client(%s) Chip(%s) Instance(%d)\n", __func__,
		 info->client_name, info->chip_name, info->instance_id);

	rc = msm_dba_get_probed_device(info, &device);
	if (rc) {
		pr_err("%s: Device not found (%s, %d)\n", __func__,
							 info->chip_name,
							 info->instance_id);
		mutex_unlock(&register_mutex);
		return ERR_PTR(rc);
	}

	pr_debug("%s: Client(%s) device found\n", __func__, info->client_name);

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client) {
		mutex_unlock(&register_mutex);
		return ERR_PTR(-ENOMEM);
	}

	memset(client, 0x0, sizeof(*client));
	client->dev = device;
	strlcpy(client->client_name, info->client_name,
		MSM_DBA_CLIENT_NAME_LEN);

	client->cb = info->cb;
	client->cb_data = info->cb_data;

	mutex_lock_nested(&device->dev_mutex, SINGLE_DEPTH_NESTING);
	list_add(&client->list, &device->client_list);
	*ops = device->client_ops;
	mutex_unlock(&device->dev_mutex);

	if (device->reg_fxn) {
		rc = device->reg_fxn(client);
		if (rc) {
			pr_err("%s: Client register failed (%s, %d)\n",
			       __func__, info->chip_name, info->instance_id);
			/* remove the client from list before freeing */
			mutex_lock_nested(&device->dev_mutex,
						SINGLE_DEPTH_NESTING);
			list_del(&client->list);
			mutex_unlock(&device->dev_mutex);
			kfree(client);
			mutex_unlock(&register_mutex);
			return ERR_PTR(rc);
		}
	}

	mutex_unlock(&register_mutex);

	pr_debug("%s: EXIT\n", __func__);
	return client;
}
EXPORT_SYMBOL(msm_dba_register_client);

int msm_dba_deregister_client(void *client)
{
	int rc = 0;
	struct msm_dba_client_info *handle = client;
	struct msm_dba_client_info *node = NULL;
	struct list_head *tmp = NULL;
	struct list_head *position = NULL;

	pr_debug("%s: ENTER\n", __func__);

	if (!handle) {
		pr_err("%s: Invalid Params\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&register_mutex);

	pr_debug("%s: Client(%s) Chip(%s) Instance(%d)\n", __func__,
		 handle->client_name, handle->dev->chip_name,
		 handle->dev->instance_id);

	if (handle->dev->dereg_fxn) {
		rc = handle->dev->dereg_fxn(handle);
		if (rc) {
			pr_err("%s: Client deregister failed (%s)\n",
			       __func__, handle->client_name);
		}
	}

	mutex_lock_nested(&handle->dev->dev_mutex, SINGLE_DEPTH_NESTING);

	list_for_each_safe(position, tmp, &handle->dev->client_list) {

		node = list_entry(position, struct msm_dba_client_info, list);

		if (node == handle) {
			list_del(&node->list);
			break;
		}
	}

	mutex_unlock(&handle->dev->dev_mutex);

	kfree(handle);

	mutex_unlock(&register_mutex);

	pr_debug("%s: EXIT (%d)\n", __func__, rc);
	return rc;
}
EXPORT_SYMBOL(msm_dba_deregister_client);
