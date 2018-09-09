/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/reset-controller.h>
#include <linux/clk/msm-clk.h>
#include <linux/habmm.h>
#include "virt-reset-front.h"
#include "virtclk-front.h"

static int virtrc_front_get_clk_id(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	struct virtrc_front *rst;
	struct virt_reset_map *map;
	struct clk_msg_getid msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	rst = to_virtrc_front(rcdev);
	map = &rst->reset_map[id];
	msg.header.cmd = CLK_MSG_GETID;
	msg.header.len = sizeof(msg);
	strlcpy(msg.name, map->clk_name, sizeof(msg.name));

	rt_mutex_lock(&virtclk_front_ctx.lock);

	handle = virtclk_front_ctx.handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n", map->clk_name,
				ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n", map->clk_name,
				ret);
		goto err_out;
	}

	if (rsp.rsp) {
		pr_err("%s: error response (%d)\n", map->clk_name, rsp.rsp);
		ret = -EIO;
	} else
		map->clk_id = rsp.header.clk_id;

	rt_mutex_unlock(&virtclk_front_ctx.lock);

	return ret;

err_out:
	habmm_socket_close(handle);
	virtclk_front_ctx.handle = 0;
	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;
}

static int __virtrc_front_reset(struct reset_controller_dev *rcdev,
		unsigned long id, enum clk_reset_action action)
{
	struct virtrc_front *rst;
	struct virt_reset_map *map;
	struct clk_msg_reset msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	rst = to_virtrc_front(rcdev);
	map = &rst->reset_map[id];

	ret = virtclk_front_init_iface();
	if (ret)
		return ret;

	ret = virtrc_front_get_clk_id(rcdev, id);
	if (ret)
		return ret;

	msg.header.clk_id = map->clk_id;
	msg.header.cmd = CLK_MSG_RESET;
	msg.header.len = sizeof(struct clk_msg_header);
	msg.reset = action;

	rt_mutex_lock(&virtclk_front_ctx.lock);

	handle = virtclk_front_ctx.handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n", map->clk_name,
				ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n", map->clk_name,
				ret);
		goto err_out;
	}

	if (rsp.rsp) {
		pr_err("%s: error response (%d)\n", map->clk_name, rsp.rsp);
		ret = -EIO;
	}

	rt_mutex_unlock(&virtclk_front_ctx.lock);

	pr_debug("%s(%lu): do %s\n", map->clk_name, id,
			action == CLK_RESET_ASSERT ? "assert" : "deassert");

	return ret;

err_out:
	habmm_socket_close(handle);
	virtclk_front_ctx.handle = 0;
	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;
}

static int virtrc_front_reset(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	int ret = 0;

	ret = __virtrc_front_reset(rcdev, id, CLK_RESET_ASSERT);
	if (ret)
		return ret;

	udelay(1);

	return __virtrc_front_reset(rcdev, id, CLK_RESET_DEASSERT);
}

static int virtrc_front_reset_assert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	return __virtrc_front_reset(rcdev, id, CLK_RESET_ASSERT);
}

static int virtrc_front_reset_deassert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	return __virtrc_front_reset(rcdev, id, CLK_RESET_DEASSERT);
}

struct reset_control_ops virtrc_front_ops = {
	.reset = virtrc_front_reset,
	.assert = virtrc_front_reset_assert,
	.deassert = virtrc_front_reset_deassert,
};

int msm_virtrc_front_register(struct platform_device *pdev,
	struct virt_reset_map *map, unsigned int num_resets)
{
	struct virtrc_front *reset;
	int ret = 0;

	reset = devm_kzalloc(&pdev->dev, sizeof(*reset), GFP_KERNEL);
	if (!reset)
		return -ENOMEM;

	reset->rcdev.of_node = pdev->dev.of_node;
	reset->rcdev.ops = &virtrc_front_ops;
	reset->rcdev.owner = pdev->dev.driver->owner;
	reset->rcdev.nr_resets = num_resets;
	reset->reset_map = map;

	ret = reset_controller_register(&reset->rcdev);
	if (ret)
		dev_err(&pdev->dev, "Failed to register with reset controller\n");

	return ret;
}
EXPORT_SYMBOL(msm_virtrc_front_register);
