// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/of.h>

#include <lpm.h>
#include <lpm_irqremain.h>
#include <lpm_resource_ctrl.h>
#include <lpm_rc.h>
#include <lpm_trace.h>

int lpm_platform_trace_get(int type, struct LPM_PLAT_TRACE *trace)
{
	return lpm_trace_instance_get(type, trace);
}
EXPORT_SYMBOL(lpm_platform_trace_get);

int __init lpm_platform_init(void)
{
	struct device_node *devnp;

	devnp = of_find_compatible_node(NULL, NULL,
					MTK_LPM_DTS_COMPATIBLE);

	if (devnp) {
		lpm_irqremain_parsing(devnp);
		lpm_resource_ctrl_parsing(devnp);
		lpm_rc_parsing(devnp);

		lpm_trace_parsing(devnp);
		of_node_put(devnp);
	}
	return 0;
}

