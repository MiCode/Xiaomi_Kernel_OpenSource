// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: owen.chen <owen.chen@mediatek.com>
 */

/*
 * @file    mtk-clkbuf-bridge.c
 * @brief   Bridge Driver for Clock Buffer Control
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <mtk-clkbuf-bridge.h>

/*******************************************************************************
 * Bridging from platform -> clkbuf.ko
 ******************************************************************************/
static struct clk_buf_bridge bridge;

void clk_buf_export_platform_bridge_register(struct clk_buf_bridge *cb)
{
	if (unlikely(!cb))
		return;

	bridge.set_xo_ctrl_cb = cb->set_xo_ctrl_cb;
	bridge.set_flight_mode_cb = cb->set_flight_mode_cb;
	bridge.set_bblpm_cb = cb->set_bblpm_cb;
	bridge.get_xo_ctrl_cb = cb->get_xo_ctrl_cb;
	bridge.dump_log_cb = cb->dump_log_cb;
}
EXPORT_SYMBOL(clk_buf_export_platform_bridge_register);

void clk_buf_export_platform_bridge_unregister(void)
{
	memset(&bridge, 0, sizeof(struct clk_buf_bridge));
}
EXPORT_SYMBOL(clk_buf_export_platform_bridge_unregister);

enum clk_buf_ret_type clk_buf_ctrl(enum clk_buf_id id, bool onoff)
{
	if (unlikely(!bridge.set_xo_ctrl_cb)) {
		pr_err("set xo ctrl not registered\n");
		return CLK_BUF_NOT_SUPPORT;
	}

	if (bridge.set_xo_ctrl_cb(id, onoff))
		return CLK_BUF_OK;

	return CLK_BUF_FAIL;
}
EXPORT_SYMBOL(clk_buf_ctrl);

/* for ccci driver to notify this */
enum clk_buf_ret_type clk_buf_set_by_flightmode(bool on)
{
	if (unlikely(!bridge.set_flight_mode_cb)) {
		pr_err("set flight mdoe not registered\n");
		return CLK_BUF_NOT_SUPPORT;
	}

	if (bridge.set_flight_mode_cb(on))
		return CLK_BUF_OK;

	return CLK_BUF_FAIL;
}
EXPORT_SYMBOL(clk_buf_set_by_flightmode);

enum clk_buf_ret_type clk_buf_control_bblpm(bool on)
{

	if (unlikely(!bridge.set_bblpm_cb)) {
		pr_err("set bblpm not registered\n");
		return CLK_BUF_NOT_SUPPORT;
	}

	if (bridge.set_bblpm_cb(on) == 0)
		return CLK_BUF_OK;

	return CLK_BUF_FAIL;

}
EXPORT_SYMBOL(clk_buf_control_bblpm);

enum clk_buf_ret_type clk_buf_dump_clkbuf_log(void)
{
	if (unlikely(!bridge.dump_log_cb)) {
		pr_err("dump log not registered\n");
		return CLK_BUF_NOT_SUPPORT;
	}

	bridge.dump_log_cb();

	return CLK_BUF_OK;
}
EXPORT_SYMBOL(clk_buf_dump_clkbuf_log);

enum clk_buf_ret_type clk_buf_get_xo_en_sta(enum xo_id id)
{
	if (unlikely(!bridge.get_xo_ctrl_cb)) {
		pr_err("get xo ctrl not registered\n");
		return CLK_BUF_NOT_SUPPORT;
	}

	if (bridge.get_xo_ctrl_cb(id))
		return CLK_BUF_ENABLE;

	return CLK_BUF_DISABLE;

}
EXPORT_SYMBOL(clk_buf_get_xo_en_sta);
