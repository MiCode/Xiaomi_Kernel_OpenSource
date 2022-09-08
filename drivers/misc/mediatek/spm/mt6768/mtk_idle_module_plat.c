// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <mtk_idle_module.h>
#include <mtk_idle.h>
#include <mtk_lp_dts.h>
#include <mtk_idle_module_plat.h>
#include <mtk_idle_profile.h>
#include <linux/module.h>

static int mtk_idle_module_get_init_data(struct mtk_idle_init_data *pData)
{
	struct device_node *idle_node = NULL;

	if (!pData)
		return MTK_IDLE_MOD_FAIL;

	/* Get dts of cpu's idle-state*/
	idle_node = GET_MTK_IDLE_STATES_DTS_NODE();

	if (idle_node) {
		int state = 0;

		/* Get dts of IDLE DRAM*/
		state = GET_MTK_OF_PROPERTY_STATUS_IDLEDRAM(idle_node);
		MTK_IDLE_FEATURE_DTS_STATE_CHECK_P(MTK_LP_FEATURE_DTS_IDLEDRAM
						, state, pData);

		/* Get dts of IDLE SYSPLL*/
		state = GET_MTK_OF_PROPERTY_STATUS_IDLESYSPLL(idle_node);
		MTK_IDLE_FEATURE_DTS_STATE_CHECK_P(MTK_LP_FEATURE_DTS_IDLESYSPLL
						, state, pData);

		/* Get dts of IDLE BUS26M*/
		state = GET_MTK_OF_PROPERTY_STATUS_IDLEBUS26M(idle_node);
		MTK_IDLE_FEATURE_DTS_STATE_CHECK_P(MTK_LP_FEATURE_DTS_IDLEBUS26M
						, state, pData);

		of_node_put(idle_node);
	}

	return MTK_IDLE_MOD_OK;
}

size_t mtk_idle_module_helper(char *buf, size_t sz)
{
	size_t mSize = 0;

	#define MTK_DEBUGFS_IDLE	"/d/cpuidle/idle_state"
	#define MTK_DEBUGFS_DRAM	"/d/cpuidle/IdleDram_state"
	#define MTK_DEBUGFS_SYSPLL	"/d/cpuidle/IdleSyspll_state"
	#define MTK_DEBUGFS_BUS26M	"/d/cpuidle/IdleBus26m_state"

	do {
		if (sz - mSize <= 0)
			break;
		mSize += scnprintf(buf + mSize, sz - mSize
		, "IdleDram help:	  cat %s\n", MTK_DEBUGFS_DRAM);
		if (sz - mSize <= 0)
			break;
		mSize += scnprintf(buf + mSize, sz - mSize
		, "IdleSyspll help:	cat %s\n", MTK_DEBUGFS_SYSPLL);
		if (sz - mSize <= 0)
			break;
		mSize += scnprintf(buf + mSize, sz - mSize
		, "IdleBus26m help:	cat %s\n", MTK_DEBUGFS_BUS26M);
	} while (0);

	return mSize;
}

struct MTK_IDLE_MODEL mod_bus26m = {
	.clerk = {
		.name = "IdleBus26m",
		.type = IDLE_MODEL_BUS26M,
		.time_critera = 30000,
	},
	.notify = {
		.id_enter = MTK_IDLE_26M_OFF,
		.id_leave = MTK_IDLE_26M_ON,
	},
	.policy = {
		.init = mtk_idle_bus26m_init,
		.enter = mtk_idle_bus26m_enter,
		.can_enter = mtk_idle_bus26m_can_enter,
		.enabled = mtk_idle_bus26m_enabled,
		.receiver = NULL,
	},
};

struct MTK_IDLE_MODEL mod_syspll = {
	.clerk = {
		.name = "IdleSyspll",
		.type = IDLE_MODEL_SYSPLL,
		.time_critera = 30000,
	},
	.notify = {
		.id_enter = MTK_IDLE_MAINPLL_OFF,
		.id_leave = MTK_IDLE_MAINPLL_ON,
	},
	.policy = {
		.init = mtk_idle_syspll_init,
		.enter = mtk_idle_syspll_enter,
		.can_enter = mtk_idle_syspll_can_enter,
		.enabled = mtk_idle_syspll_enabled,
		.receiver = NULL,
	},
};

struct MTK_IDLE_MODEL mod_dram = {
	.clerk = {
		.name = "IdleDram",
		.type = IDLE_MODEL_DRAM,
		.time_critera = 30000,
	},
	.notify = {
		.id_enter = MTK_IDLE_MAINPLL_OFF,
		.id_leave = MTK_IDLE_MAINPLL_ON,
	},
	.policy = {
		.init = mtk_idle_dram_init,
		.enter = mtk_idle_dram_enter,
		.can_enter = mtk_idle_dram_can_enter,
		.enabled = mtk_idle_dram_enabled,
		.receiver = NULL,
	},
};

struct MTK_IDLE_MODEL *plat_mods[] = {
	&mod_bus26m,
	&mod_syspll,
	&mod_dram,
	NULL
};

struct MTK_IDLE_MODULE mtk_idle_module_plat = {
	.reg = {
		.get_init_data = mtk_idle_module_get_init_data,
		.get_helper_info = mtk_idle_module_helper,
	},
	.models = plat_mods,
};

int mtk_idle_module_initialize_plat(void)
{
	mtk_idle_module_register(&mtk_idle_module_plat);
	return 0;
}

void mtk_idle_module_exit_plat(void)
{
}
//module_init(mtk_idle_module_initialize_plat);
//module_exit(mtk_idle_module_exit_plat);
MODULE_LICENSE("GPL");
