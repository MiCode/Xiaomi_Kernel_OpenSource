// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <mtk_lpm_platform.h>
#include <mtk_lpm_module.h>
#include <mtk_lpm_internal.h>

#include "mtk_lpm_resource_ctrl.h"

#define MTK_LPM_RESOURCE_NODE		"resource-ctrl"
#define MTK_LPM_RESOURCE_LISTNODE	"resource-ctrl-list"
#define MTK_LPM_RESOURCE_ID		"id"
#define MTK_LPM_RESOURCE_VALUE		"value"

int __init mtk_lpm_resource_ctrl_parsing(struct device_node *parent)
{
	int ret = 0, idx = 0;
	u32 value = 0, id = 0;
	struct device_node *np = NULL;

	while ((np = of_parse_phandle(parent, MTK_LPM_RESOURCE_NODE, idx))) {
		idx++;

		of_property_read_u32(np, MTK_LPM_RESOURCE_ID, &id);
		of_property_read_u32(np, MTK_LPM_RESOURCE_VALUE, &value);
		of_node_put(np);

		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_DOE_RESOURCE_CTRL,
				    MT_LPM_SMC_ACT_SET, id, value);
	}

	return ret;
}
