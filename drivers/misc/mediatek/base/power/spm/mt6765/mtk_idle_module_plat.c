/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <mtk_idle_module.h>
#include <mtk_idle.h>
#include <mtk_lp_dts.h>
#include <mtk_idle_module_plat.h>
#include <mtk_idle_profile.h>

static int mtk_idle_module_get_init_data(struct mtk_idle_init_data *pData)
{
	struct device_node *idle_node = NULL;

	if (!pData)
		return MTK_IDLE_MOD_FAIL;

	/* Get dts of cpu's idle-state*/
	idle_node = GET_MTK_IDLE_STATES_DTS_NODE();

	if (idle_node) {
		int state = 0;

		/* Get dts of sodi*/
		state = GET_MTK_OF_PROPERTY_STATUS_SODI(idle_node);
		MTK_IDLE_FEATURE_DTS_STATE_CHECK_P(MTK_LP_FEATURE_DTS_SODI
						, state, pData);

		/* Get dts of dpidle*/
		state = GET_MTK_OF_PROPERTY_STATUS_DPIDLE(idle_node);
		MTK_IDLE_FEATURE_DTS_STATE_CHECK_P(MTK_LP_FEATURE_DTS_DP
						, state, pData);

		/* Get dts of IDLE sodi3*/
		state = GET_MTK_OF_PROPERTY_STATUS_SODI3(idle_node);
		MTK_IDLE_FEATURE_DTS_STATE_CHECK_P(MTK_LP_FEATURE_DTS_SODI3
						, state, pData);

		of_node_put(idle_node);
	}

	return MTK_IDLE_MOD_OK;
}

size_t mtk_idle_module_helper(char *buf, size_t sz)
{
	size_t mSize = 0;

	#define MTK_DEBUGFS_IDLE	"/d/cpuidle/idle_state"
	#define MTK_DEBUGFS_SODI	"/d/cpuidle/soidle_state"
	#define MTK_DEBUGFS_DPIDLE	"/d/cpuidle/dpidle_state"
	#define MTK_DEBUGFS_SODI3	"/d/cpuidle/soidle3_state"

	do {
		if (sz - mSize <= 0)
			break;
		mSize += scnprintf(buf + mSize, sz - mSize
		, "sodi help:          cat %s\n", MTK_DEBUGFS_SODI);
		if (sz - mSize <= 0)
			break;
		mSize += scnprintf(buf + mSize, sz - mSize
		, "dpidle help:        cat %s\n", MTK_DEBUGFS_DPIDLE);
		if (sz - mSize <= 0)
			break;
		mSize += scnprintf(buf + mSize, sz - mSize
		, "sodi3 help:        cat %s\n", MTK_DEBUGFS_SODI3);
	} while (0);

	return mSize;
}
struct MTK_IDLE_MODEL mod_sodi3 = {
	.clerk = {
		.name = "sodi3",
		.type = IDLE_TYPE_SO3,
		.time_critera = 30000,
	},
	.notify = {
		.id_enter = NOTIFY_SOIDLE3_ENTER,
		.id_leave = NOTIFY_SOIDLE3_LEAVE,
	},
	.policy = {
		.init = mtk_sodi3_init,
		.enter = mtk_sodi3_enter,
		.can_enter = sodi3_can_enter,
		.enabled = mtk_sodi3_enabled,
		.receiver = NULL,
	},
};

struct MTK_IDLE_MODEL mod_dpidle = {
	.clerk = {
		.name = "dpidle",
		.type = IDLE_TYPE_DP,
		.time_critera = 30000,
	},
	.notify = {
		.id_enter = NOTIFY_DPIDLE_ENTER,
		.id_leave = NOTIFY_DPIDLE_LEAVE,
	},
	.policy = {
		.init = mtk_dpidle_init,
		.enter = mtk_dpidle_enter,
		.can_enter = dpidle_can_enter,
		.enabled = mtk_dpidle_enabled,
		.receiver = NULL,
	},
};

struct MTK_IDLE_MODEL mod_sodi = {
	.clerk = {
		.name = "sodi",
		.type = IDLE_TYPE_SO,
		.time_critera = 30000,
	},
	.notify = {
		.id_enter = NOTIFY_SOIDLE_ENTER,
		.id_leave = NOTIFY_SOIDLE_LEAVE,
	},
	.policy = {
		.init = mtk_sodi_init,
		.enter = mtk_sodi_enter,
		.can_enter = sodi_can_enter,
		.enabled = mtk_sodi_enabled,
		.receiver = NULL,
	},
};

struct MTK_IDLE_MODEL *plat_mods[] = {
	&mod_sodi3,
	&mod_dpidle,
	&mod_sodi,
	NULL
};

struct MTK_IDLE_MODULE mtk_idle_module_plat = {
	.reg = {
		.get_init_data = mtk_idle_module_get_init_data,
		.get_helper_info = mtk_idle_module_helper,
	},
	.models = plat_mods,
};

static int __init mtk_idle_module_initialize_plat(void)
{
	mtk_idle_module_register(&mtk_idle_module_plat);
	return 0;
}
late_initcall(mtk_idle_module_initialize_plat);
