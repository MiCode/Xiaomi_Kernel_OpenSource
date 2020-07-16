// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

#include "board-dt.h"

void __init board_dt_populate(struct of_dev_auxdata *adata)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);

	/* Explicitly parent the /soc devices to the root node to preserve
	 * the kernel ABI (sysfs structure, etc) until userspace is updated
	 */
	of_platform_populate(of_find_node_by_path("/soc"),
			     of_default_bus_match_table, adata, NULL);
}
