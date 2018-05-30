/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/clk/msm-clock-generic.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/habmm.h>
#include <soc/qcom/msm-clock-controller.h>
#include "virtclk-front.h"

struct virtclk_front_data virtclk_front_ctx;

static inline struct virtclk_front *to_virtclk_front(struct clk *clk)
{
	return container_of(clk, struct virtclk_front, c);
}

int virtclk_front_init_iface(void)
{
	int ret = 0;
	int handle;

	rt_mutex_lock(&virtclk_front_ctx.lock);

	if (virtclk_front_ctx.handle)
		goto out;

	ret = habmm_socket_open(&handle, MM_CLK_VM1, 0, 0);
	if (ret) {
		pr_err("open habmm socket failed (%d)\n", ret);
		goto out;
	}

	virtclk_front_ctx.handle = handle;

out:
	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;
}
EXPORT_SYMBOL(virtclk_front_init_iface);

static int virtclk_front_get_id(struct clk *clk)
{
	struct virtclk_front *v = to_virtclk_front(clk);
	struct clk_msg_getid msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	if (v->id)
		return ret;

	msg.header.cmd = CLK_MSG_GETID | v->flag;
	msg.header.len = sizeof(msg);
	strlcpy(msg.name, clk->dbg_name, sizeof(msg.name));

	rt_mutex_lock(&virtclk_front_ctx.lock);

	handle = virtclk_front_ctx.handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size,
			UINT_MAX, HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	if (rsp.rsp) {
		pr_err("%s: error response (%d)\n", clk->dbg_name, rsp.rsp);
		ret = -EIO;
	} else
		v->id = rsp.header.clk_id;

	rt_mutex_unlock(&virtclk_front_ctx.lock);

	return ret;

err_out:
	habmm_socket_close(handle);
	virtclk_front_ctx.handle = 0;
	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;
}

static int virtclk_front_prepare(struct clk *clk)
{
	struct virtclk_front *v = to_virtclk_front(clk);
	struct clk_msg_header msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	ret = virtclk_front_init_iface();
	if (ret)
		return ret;

	ret = virtclk_front_get_id(clk);
	if (ret)
		return ret;

	msg.clk_id = v->id;
	msg.cmd = CLK_MSG_ENABLE | v->flag;
	msg.len = sizeof(struct clk_msg_header);

	rt_mutex_lock(&virtclk_front_ctx.lock);

	handle = virtclk_front_ctx.handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	if (rsp.rsp) {
		pr_err("%s: error response (%d)\n", clk->dbg_name, rsp.rsp);
		ret = -EIO;
	}

	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;

err_out:
	habmm_socket_close(handle);
	virtclk_front_ctx.handle = 0;
	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;
}

static void virtclk_front_unprepare(struct clk *clk)
{
	struct virtclk_front *v = to_virtclk_front(clk);
	struct clk_msg_header msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	ret = virtclk_front_init_iface();
	if (ret)
		return;

	ret = virtclk_front_get_id(clk);
	if (ret)
		return;

	msg.clk_id = v->id;
	msg.cmd = CLK_MSG_DISABLE | v->flag;
	msg.len = sizeof(struct clk_msg_header);

	rt_mutex_lock(&virtclk_front_ctx.lock);

	handle = virtclk_front_ctx.handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	if (rsp.rsp)
		pr_err("%s: error response (%d)\n", clk->dbg_name, rsp.rsp);

	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return;

err_out:
	habmm_socket_close(handle);
	virtclk_front_ctx.handle = 0;
	rt_mutex_unlock(&virtclk_front_ctx.lock);
}

static int virtclk_front_reset(struct clk *clk, enum clk_reset_action action)
{
	struct virtclk_front *v = to_virtclk_front(clk);
	struct clk_msg_reset msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	ret = virtclk_front_init_iface();
	if (ret)
		return ret;

	ret = virtclk_front_get_id(clk);
	if (ret)
		return ret;

	msg.header.clk_id = v->id;
	msg.header.cmd = CLK_MSG_RESET | v->flag;
	msg.header.len = sizeof(struct clk_msg_header);
	msg.reset = action;

	rt_mutex_lock(&virtclk_front_ctx.lock);

	handle = virtclk_front_ctx.handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	if (rsp.rsp) {
		pr_err("%s: error response (%d)\n", clk->dbg_name, rsp.rsp);
		ret = -EIO;
	}

	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;

err_out:
	habmm_socket_close(handle);
	virtclk_front_ctx.handle = 0;
	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;
}

static int virtclk_front_set_rate(struct clk *clk, unsigned long rate)
{
	struct virtclk_front *v = to_virtclk_front(clk);
	struct clk_msg_setfreq msg;
	struct clk_msg_rsp rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	ret = virtclk_front_init_iface();
	if (ret)
		return ret;

	ret = virtclk_front_get_id(clk);
	if (ret)
		return ret;

	msg.header.clk_id = v->id;
	msg.header.cmd = CLK_MSG_SETFREQ | v->flag;
	msg.header.len = sizeof(msg);
	msg.freq = (u32)rate;

	rt_mutex_lock(&virtclk_front_ctx.lock);

	handle = virtclk_front_ctx.handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		pr_err("%s: habmm socket send failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		pr_err("%s: habmm socket receive failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	if (rsp.rsp) {
		pr_err("%s (%luHz): error response (%d)\n", clk->dbg_name,
				rate, rsp.rsp);
		ret = -EIO;
	}

	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;

err_out:
	habmm_socket_close(handle);
	virtclk_front_ctx.handle = 0;
	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;
}

static int virtclk_front_set_max_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static int virtclk_front_is_enabled(struct clk *clk)
{
	struct virtclk_front *v = to_virtclk_front(clk);

	return !!v->c.prepare_count;
}

static int virtclk_front_set_flags(struct clk *clk, unsigned flags)
{
	return 0;
}

static unsigned long virtclk_front_get_rate(struct clk *clk)
{
	struct virtclk_front *v = to_virtclk_front(clk);
	struct clk_msg_header msg;
	struct clk_msg_getfreq rsp;
	u32 rsp_size = sizeof(rsp);
	int handle;
	int ret = 0;

	ret = virtclk_front_init_iface();
	if (ret)
		return 0;

	ret = virtclk_front_get_id(clk);
	if (ret)
		return 0;

	msg.clk_id = v->id;
	msg.cmd = CLK_MSG_GETFREQ | v->flag;
	msg.len = sizeof(msg);

	rt_mutex_lock(&virtclk_front_ctx.lock);

	handle = virtclk_front_ctx.handle;
	ret = habmm_socket_send(handle, &msg, sizeof(msg), 0);
	if (ret) {
		ret = 0;
		pr_err("%s: habmm socket send failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	ret = habmm_socket_recv(handle, &rsp, &rsp_size, UINT_MAX,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		ret = 0;
		pr_err("%s: habmm socket receive failed (%d)\n", clk->dbg_name,
				ret);
		goto err_out;
	}

	if (rsp.rsp.rsp) {
		pr_err("%s: error response (%d)\n", clk->dbg_name, rsp.rsp.rsp);
		ret = 0;
	} else
		ret = rsp.freq;

	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;

err_out:
	habmm_socket_close(handle);
	virtclk_front_ctx.handle = 0;
	rt_mutex_unlock(&virtclk_front_ctx.lock);
	return ret;
}

static long virtclk_front_round_rate(struct clk *clk, unsigned long rate)
{
	return rate;
}

struct clk_ops virtclk_front_ops = {
	.prepare = virtclk_front_prepare,
	.unprepare = virtclk_front_unprepare,
	.reset = virtclk_front_reset,
	.set_rate = virtclk_front_set_rate,
	.set_max_rate = virtclk_front_set_max_rate,
	.is_enabled = virtclk_front_is_enabled,
	.set_flags = virtclk_front_set_flags,
	.get_rate = virtclk_front_get_rate,
	.round_rate = virtclk_front_round_rate,
};

int msm_virtclk_front_probe(struct platform_device *pdev,
		struct clk_lookup *table,
		size_t size)
{
	int ret;

	ret = of_msm_clock_register(pdev->dev.of_node, table, size);
	if (ret)
		return ret;

	rt_mutex_init(&virtclk_front_ctx.lock);

	dev_info(&pdev->dev, "Registered virtual clock provider.\n");

	return ret;
}
EXPORT_SYMBOL(msm_virtclk_front_probe);
