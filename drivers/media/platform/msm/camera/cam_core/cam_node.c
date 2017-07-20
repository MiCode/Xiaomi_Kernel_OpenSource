/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>

#include "cam_node.h"
#include "cam_trace.h"

static void  __cam_node_handle_shutdown(struct cam_node *node)
{
	if (node->hw_mgr_intf.hw_close)
		node->hw_mgr_intf.hw_close(node->hw_mgr_intf.hw_mgr_priv,
			NULL);
}

static int __cam_node_handle_query_cap(struct cam_node *node,
	struct cam_query_cap_cmd *query)
{
	int rc = -EFAULT;

	if (!query) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	if (node->hw_mgr_intf.hw_get_caps) {
		rc = node->hw_mgr_intf.hw_get_caps(
			node->hw_mgr_intf.hw_mgr_priv, query);
	}

	return rc;
}

static int __cam_node_handle_acquire_dev(struct cam_node *node,
	struct cam_acquire_dev_cmd *acquire)
{
	int rc = 0;
	struct cam_context *ctx = NULL;

	if (!acquire)
		return -EINVAL;

	mutex_lock(&node->list_mutex);
	if (!list_empty(&node->free_ctx_list)) {
		ctx = list_first_entry(&node->free_ctx_list,
			struct cam_context, list);
		list_del_init(&ctx->list);
	}
	mutex_unlock(&node->list_mutex);
	if (!ctx) {
		rc = -ENOMEM;
		goto err;
	}

	rc = cam_context_handle_acquire_dev(ctx, acquire);
	if (rc) {
		pr_err("%s: Acquire device failed\n", __func__);
		goto free_ctx;
	}

	return 0;
free_ctx:
	mutex_lock(&node->list_mutex);
	list_add_tail(&ctx->list, &node->free_ctx_list);
	mutex_unlock(&node->list_mutex);
err:
	return rc;
}

static int __cam_node_handle_start_dev(struct cam_node *node,
	struct cam_start_stop_dev_cmd *start)
{
	struct cam_context *ctx = NULL;

	if (!start)
		return -EINVAL;

	if (start->dev_handle <= 0) {
		pr_err("Invalid device handle for context\n");
		return -EINVAL;
	}

	if (start->session_handle <= 0) {
		pr_err("Invalid session handle for context\n");
		return -EINVAL;
	}

	ctx = (struct cam_context *)cam_get_device_priv(start->dev_handle);
	if (!ctx) {
		pr_err("%s: Can not get context for handle %d\n",
			__func__, start->dev_handle);
		return -EINVAL;
	}

	return cam_context_handle_start_dev(ctx, start);
}

static int __cam_node_handle_stop_dev(struct cam_node *node,
	struct cam_start_stop_dev_cmd *stop)
{
	struct cam_context *ctx = NULL;

	if (!stop)
		return -EINVAL;

	if (stop->dev_handle <= 0) {
		pr_err("Invalid device handle for context\n");
		return -EINVAL;
	}

	if (stop->session_handle <= 0) {
		pr_err("Invalid session handle for context\n");
		return -EINVAL;
	}

	ctx = (struct cam_context *)cam_get_device_priv(stop->dev_handle);
	if (!ctx) {
		pr_err("%s: Can not get context for handle %d\n",
			__func__, stop->dev_handle);
		return -EINVAL;
	}

	return cam_context_handle_stop_dev(ctx, stop);
}

static int __cam_node_handle_config_dev(struct cam_node *node,
	struct cam_config_dev_cmd *config)
{
	struct cam_context *ctx = NULL;

	if (!config)
		return -EINVAL;

	if (config->dev_handle <= 0) {
		pr_err("Invalid device handle for context\n");
		return -EINVAL;
	}

	if (config->session_handle <= 0) {
		pr_err("Invalid session handle for context\n");
		return -EINVAL;
	}

	ctx = (struct cam_context *)cam_get_device_priv(config->dev_handle);
	if (!ctx) {
		pr_err("%s: Can not get context for handle %d\n",
			__func__, config->dev_handle);
		return -EINVAL;
	}

	return cam_context_handle_config_dev(ctx, config);
}

static int __cam_node_handle_release_dev(struct cam_node *node,
	struct cam_release_dev_cmd *release)
{
	int rc = 0;
	struct cam_context *ctx = NULL;

	if (!release)
		return -EINVAL;

	if (release->dev_handle <= 0) {
		pr_err("Invalid device handle for context\n");
		return -EINVAL;
	}

	if (release->session_handle <= 0) {
		pr_err("Invalid session handle for context\n");
		return -EINVAL;
	}

	ctx = (struct cam_context *)cam_get_device_priv(release->dev_handle);
	if (!ctx) {
		pr_err("%s: Can not get context for handle %d\n",
			__func__, release->dev_handle);
		return -EINVAL;
	}

	rc = cam_context_handle_release_dev(ctx, release);
	if (rc)
		pr_err("%s: context release failed\n", __func__);

	rc = cam_destroy_device_hdl(release->dev_handle);
	if (rc)
		pr_err("%s: destroy device handle is failed\n", __func__);

	mutex_lock(&node->list_mutex);
	list_add_tail(&ctx->list, &node->free_ctx_list);
	mutex_unlock(&node->list_mutex);
	return rc;
}

static int __cam_node_crm_get_dev_info(struct cam_req_mgr_device_info *info)
{
	struct cam_context *ctx = NULL;

	if (!info)
		return -EINVAL;

	ctx = (struct cam_context *) cam_get_device_priv(info->dev_hdl);
	if (!ctx) {
		pr_err("%s: Can not get context  for handle %d\n",
			__func__, info->dev_hdl);
		return -EINVAL;
	}
	return cam_context_handle_crm_get_dev_info(ctx, info);
}

static int __cam_node_crm_link_setup(
	struct cam_req_mgr_core_dev_link_setup *setup)
{
	int rc;
	struct cam_context *ctx = NULL;

	if (!setup)
		return -EINVAL;

	ctx = (struct cam_context *) cam_get_device_priv(setup->dev_hdl);
	if (!ctx) {
		pr_err("%s: Can not get context for handle %d\n",
			__func__, setup->dev_hdl);
		return -EINVAL;
	}

	if (setup->link_enable)
		rc = cam_context_handle_crm_link(ctx, setup);
	else
		rc = cam_context_handle_crm_unlink(ctx, setup);

	return rc;
}

static int __cam_node_crm_apply_req(struct cam_req_mgr_apply_request *apply)
{
	struct cam_context *ctx = NULL;

	if (!apply)
		return -EINVAL;

	ctx = (struct cam_context *) cam_get_device_priv(apply->dev_hdl);
	if (!ctx) {
		pr_err("%s: Can not get context for handle %d\n",
			__func__, apply->dev_hdl);
		return -EINVAL;
	}

	trace_cam_apply_req("Node", apply);

	return cam_context_handle_crm_apply_req(ctx, apply);
}

static int __cam_node_crm_flush_req(struct cam_req_mgr_flush_request *flush)
{
	struct cam_context *ctx = NULL;

	if (!flush) {
		pr_err("%s: Invalid flush request payload\n", __func__);
		return -EINVAL;
	}

	ctx = (struct cam_context *) cam_get_device_priv(flush->dev_hdl);
	if (!ctx) {
		pr_err("%s: Can not get context for handle %d\n",
			__func__, flush->dev_hdl);
		return -EINVAL;
	}

	return cam_context_handle_crm_flush_req(ctx, flush);
}

int cam_node_deinit(struct cam_node *node)
{
	if (node)
		memset(node, 0, sizeof(*node));

	pr_debug("%s: deinit complete!\n", __func__);

	return 0;
}

int cam_node_init(struct cam_node *node, struct cam_hw_mgr_intf *hw_mgr_intf,
	struct cam_context *ctx_list, uint32_t ctx_size, char *name)
{
	int rc = 0;
	int i;

	if (!node || !hw_mgr_intf ||
		sizeof(node->hw_mgr_intf) != sizeof(*hw_mgr_intf)) {
		return -EINVAL;
	}

	memset(node, 0, sizeof(*node));

	strlcpy(node->name, name, sizeof(node->name));

	memcpy(&node->hw_mgr_intf, hw_mgr_intf, sizeof(node->hw_mgr_intf));
	node->crm_node_intf.apply_req = __cam_node_crm_apply_req;
	node->crm_node_intf.get_dev_info = __cam_node_crm_get_dev_info;
	node->crm_node_intf.link_setup = __cam_node_crm_link_setup;
	node->crm_node_intf.flush_req = __cam_node_crm_flush_req;

	mutex_init(&node->list_mutex);
	INIT_LIST_HEAD(&node->free_ctx_list);
	node->ctx_list = ctx_list;
	node->ctx_size = ctx_size;
	for (i = 0; i < ctx_size; i++) {
		if (!ctx_list[i].state_machine) {
			pr_err("%s: camera context %d is not initialized!",
				__func__, i);
			rc = -1;
			goto err;
		}
		INIT_LIST_HEAD(&ctx_list[i].list);
		list_add_tail(&ctx_list[i].list, &node->free_ctx_list);
	}

	node->state = CAM_NODE_STATE_INIT;
err:
	pr_debug("%s: Exit. (rc = %d)\n", __func__, rc);
	return rc;
}

int cam_node_handle_ioctl(struct cam_node *node, struct cam_control *cmd)
{
	int rc = 0;

	if (!cmd)
		return -EINVAL;

	pr_debug("%s: handle cmd %d\n", __func__, cmd->op_code);

	switch (cmd->op_code) {
	case CAM_QUERY_CAP: {
		struct cam_query_cap_cmd query;

		if (copy_from_user(&query, (void __user *)cmd->handle,
			sizeof(query))) {
			rc = -EFAULT;
			break;
		}

		rc = __cam_node_handle_query_cap(node, &query);
		if (rc) {
			pr_err("%s: querycap is failed(rc = %d)\n",
				__func__,  rc);
			break;
		}

		if (copy_to_user((void __user *)cmd->handle, &query,
			sizeof(query)))
			rc = -EFAULT;

		break;
	}
	case CAM_ACQUIRE_DEV: {
		struct cam_acquire_dev_cmd acquire;

		if (copy_from_user(&acquire, (void __user *)cmd->handle,
			sizeof(acquire))) {
			rc = -EFAULT;
			break;
		}
		rc = __cam_node_handle_acquire_dev(node, &acquire);
		if (rc) {
			pr_err("%s: acquire device failed(rc = %d)\n",
				__func__, rc);
			break;
		}
		if (copy_to_user((void __user *)cmd->handle, &acquire,
			sizeof(acquire)))
			rc = -EFAULT;
		break;
	}
	case CAM_START_DEV: {
		struct cam_start_stop_dev_cmd start;

		if (copy_from_user(&start, (void __user *)cmd->handle,
			sizeof(start)))
			rc = -EFAULT;
		else {
			rc = __cam_node_handle_start_dev(node, &start);
			if (rc)
				pr_err("%s: start device failed(rc = %d)\n",
					__func__, rc);
		}
		break;
	}
	case CAM_STOP_DEV: {
		struct cam_start_stop_dev_cmd stop;

		if (copy_from_user(&stop, (void __user *)cmd->handle,
			sizeof(stop)))
			rc = -EFAULT;
		else {
			rc = __cam_node_handle_stop_dev(node, &stop);
			if (rc)
				pr_err("%s: stop device failed(rc = %d)\n",
					__func__, rc);
		}
		break;
	}
	case CAM_CONFIG_DEV: {
		struct cam_config_dev_cmd config;

		if (copy_from_user(&config, (void __user *)cmd->handle,
			sizeof(config)))
			rc = -EFAULT;
		else {
			rc = __cam_node_handle_config_dev(node, &config);
			if (rc)
				pr_err("%s: config device failed(rc = %d)\n",
					__func__, rc);
		}
		break;
	}
	case CAM_RELEASE_DEV: {
		struct cam_release_dev_cmd release;

		if (copy_from_user(&release, (void __user *)cmd->handle,
			sizeof(release)))
			rc = -EFAULT;
		else {
			rc = __cam_node_handle_release_dev(node, &release);
			if (rc)
				pr_err("%s: release device failed(rc = %d)\n",
					__func__, rc);
		}
		break;
	}
	case CAM_SD_SHUTDOWN:
		__cam_node_handle_shutdown(node);
		break;
	default:
		pr_err("Unknown op code %d\n", cmd->op_code);
		rc = -EINVAL;
	}

	return rc;
}
