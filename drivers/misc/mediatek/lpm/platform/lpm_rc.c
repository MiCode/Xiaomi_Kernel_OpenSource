// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <lpm_plat_common.h>
#include <lpm_module.h>
#include <lpm_internal.h>

#include "lpm_resource_ctrl.h"

#define LPM_RC_NODE	"constraints"
#define LPM_RC_LISTNODE	"constraint-list"
#define LPM_RC_ID	"id"
#define LPM_RC_VALUE	"value"

int __init lpm_rc_parsing(struct device_node *parent)
{
	int ret = 0, idx = 0;
	u32 value = 0, id = 0;
	struct device_node *np = NULL;

	while ((np = of_parse_phandle(parent, LPM_RC_NODE, idx))) {
		idx++;

		of_property_read_u32(np, LPM_RC_ID, &id);
		of_property_read_u32(np, LPM_RC_VALUE, &value);
		of_node_put(np);

		if (!!value)
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_DOE_RC,
					MT_LPM_SMC_ACT_SET, id, 0);
		else
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_DOE_RC,
					MT_LPM_SMC_ACT_CLR, id, 0);
	}

	return ret;
}
