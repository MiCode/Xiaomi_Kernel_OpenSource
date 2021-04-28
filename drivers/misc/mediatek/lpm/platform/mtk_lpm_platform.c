/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/of.h>

#include <mtk_lpm.h>
#include <mtk_lpm_irqremain.h>
#include <mtk_lpm_resource_ctrl.h>
#include <mtk_lpm_rc.h>
#include <mtk_lpm_trace.h>

int mtk_lpm_platform_trace_get(int type, struct MTK_LPM_PLAT_TRACE *trace)
{
	return mtk_lpm_trace_instance_get(type, trace);
}

static int __init mtk_lpm_platform(void)
{
	struct device_node *devnp;

	devnp = of_find_compatible_node(NULL, NULL,
					MTK_LPM_DTS_COMPATIBLE);

	if (devnp) {
		mtk_lpm_irqremain_parsing(devnp);
		mtk_lpm_resource_ctrl_parsing(devnp);
		mtk_lpm_rc_parsing(devnp);

		mtk_lpm_trace_parsing(devnp);
		of_node_put(devnp);
	}
	return 0;
}
device_initcall(mtk_lpm_platform);

