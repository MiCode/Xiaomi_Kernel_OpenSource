/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/of.h>

#include <mtk_lpm.h>
#include <mtk_lpm_irqremain.h>

static int __init mtk_lpm_platform(void)
{
	struct device_node *devnp;

	devnp = of_find_compatible_node(NULL, NULL,
					MTK_LPM_DTS_COMPATIBLE);

	if (devnp) {
		mtk_lpm_irqremain_parsing(devnp);
		of_node_put(devnp);
	}
	return 0;
}
device_initcall(mtk_lpm_platform);

