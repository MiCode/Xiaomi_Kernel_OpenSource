// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: owen.chen <owen.chen@mediatek.com>
 */

/*
 * @file    mtk-srclken-bridge.c
 * @brief   Bridge Driver for SRCLKEN RC Control
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <mtk-srclken-bridge.h>

/*******************************************************************************
 * Bridging from platform -> srclken.ko
 ******************************************************************************/
static struct srclken_bridge bridge;

void srclken_export_platform_bridge_register(struct srclken_bridge *cb)
{
	if (unlikely(!cb))
		return;

	bridge.get_stage_cb = cb->get_stage_cb;
	bridge.dump_sta_cb = cb->dump_sta_cb;
	bridge.dump_cfg_cb = cb->dump_cfg_cb;
	bridge.dump_last_sta_cb = cb->dump_last_sta_cb;
}
EXPORT_SYMBOL(srclken_export_platform_bridge_register);

void srclken_export_platform_bridge_unregister(void)
{
	memset(&bridge, 0, sizeof(struct srclken_bridge));
}
EXPORT_SYMBOL(srclken_export_platform_bridge_unregister);

enum srclken_config srclken_get_stage(void)
{
	if (unlikely(!bridge.get_stage_cb)) {
		pr_err("get stage not registered\n");
		return SRCLKEN_NOT_SUPPORT;
	}

	return bridge.get_stage_cb();
}
EXPORT_SYMBOL(srclken_get_stage);

enum srclken_config srclken_dump_sta_log(void)
{
	if (unlikely(!bridge.dump_sta_cb)) {
		pr_err("dump sta log not registered\n");
		return SRCLKEN_NOT_SUPPORT;
	}

	bridge.dump_last_sta_cb();

	return SRCLKEN_OK;
}
EXPORT_SYMBOL(srclken_dump_sta_log);

enum srclken_config srclken_dump_cfg_log(void)
{
	if (unlikely(!bridge.dump_cfg_cb)) {
		pr_err("dump cfg log not registered\n");
		return SRCLKEN_NOT_SUPPORT;
	}

	bridge.dump_cfg_cb();

	return SRCLKEN_OK;
}
EXPORT_SYMBOL(srclken_dump_cfg_log);

enum srclken_config srclken_dump_last_sta_log(void)
{
	if (unlikely(!bridge.dump_last_sta_cb)) {
		pr_err("dump last sta log not registered\n");
		return SRCLKEN_NOT_SUPPORT;
	}

	bridge.dump_last_sta_cb();

	return SRCLKEN_OK;
}
EXPORT_SYMBOL(srclken_dump_last_sta_log);

