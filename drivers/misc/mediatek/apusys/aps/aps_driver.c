// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
// linux kernel headers
#include "linux/of_device.h"
#include "linux/slab.h"

// aps driver headers
#include "aps_driver.h"
#include "aps_ipi.h"
#include "aps_sysfs.h"
#include "aps_utils.h"

int aps_init(struct apusys_core_info *info)
{
	int ret = 0;

	APS_INFO("%s +\n", __func__);
	ret = aps_ipi_init();
	if (ret != 0) {
		APS_ERR("failed to init aps ipi\n");
		return ret;
	}
	ret = aps_sysfs_init();
	if (ret != 0) {
		APS_ERR("failed to init aps sysfs\n");
		return ret;
	}

	return ret;
}

void aps_exit(void)
{
	// aps_drv_exit();
	aps_ipi_deinit();
	aps_sysfs_exit();
}
