/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _PLATFORM_MT6893_H_
#define _PLATFORM_MT6893_H_

enum conn_semaphore_type
{
	CONN_SEMA_CHIP_POWER_ON_INDEX = 0,
	CONN_SEMA_CALIBRATION_INDEX = 1,
	CONN_SEMA_FW_DL_INDEX = 2,
	CONN_SEMA_CLOCK_SWITCH_INDEX = 3,
	CONN_SEMA_CCIF_INDEX = 4,
	CONN_SEMA_COEX_INDEX = 5,
	CONN_SEMA_USB_EP0_INDEX = 6,
	CONN_SEMA_USB_SHARED_INFO_INDEX = 7,
	CONN_SEMA_USB_SUSPEND_INDEX = 8,
	CONN_SEMA_USB_RESUME_INDEX = 9,
	CONN_SEMA_PCIE_INDEX = 10,
	CONN_SEMA_RFSPI_INDEX = 11,
	CONN_SEMA_EFUSE_INDEX = 12,
	CONN_SEMA_THERMAL_INDEX = 13,
	CONN_SEMA_FLASH_INDEX = 14,
	CONN_SEMA_DEBUG_INDEX = 15,
	CONN_SEMA_WIFI_LP_INDEX = 16,
	CONN_SEMA_PATCH_DL_INDEX = 17,
	CONN_SEMA_SHARED_VAR_INDEX = 18,
	CONN_SEMA_CONN_INFRA_COMMON_SYSRAM_INDEX = 19,
	CONN_SEMA_BUS_CONTROL = 20,
	CONN_SEMA_NUM_MAX = 32 /* can't be omitted */
};

int consys_platform_spm_conn_ctrl(unsigned int enable);
int consys_co_clock_type(void);

struct consys_plat_thermal_data {
	int thermal_b;
	int slop_molecule;
	int offset;
};

void update_thermal_data(struct consys_plat_thermal_data* input);
#endif /* _PLATFORM_MT6893_H_ */
