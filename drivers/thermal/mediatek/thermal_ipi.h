/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_THERMAL_IPI_H__
#define __MTK_THERMAL_IPI_H__

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_ipi_id.h>
#endif
#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
#include <mcupm_ipi_id.h>
#endif

#define THERMAL_TARGET_NAME_LEN	(10)
#define THERMAL_SLOT_NUM	(4)

enum thermal_ipi_target {
	IPI_TARGET_SSPM,
	IPI_TARGET_MCUPM,
	NUM_THERMAL_IPI_TARGET,

	IPI_TARGET_ALL = 0xFF,
};

enum thermal_ipi_reply_data {
	IPI_SUCCESS,
	IPI_FAIL,
	IPI_NOT_SUPPORT,
	IPI_WRONG_MSG_TYPE,
	NUM_THERMAL_IPI_REPLY
};

enum thermal_ipi_msg_type {
	THERMAL_THROTTLE_DISABLE = 100,
};

struct thermal_ipi_data {
	unsigned int cmd;
	int arg[THERMAL_SLOT_NUM - 1];
};

struct thermal_ipi_config {
	struct mtk_ipi_device *dev;
	int id;
	int use_platform_ipi;
	int opt;
	int ack_data;
};

struct thermal_ipi_target_data {
	const char name[THERMAL_TARGET_NAME_LEN];
	int is_registered;
	struct thermal_ipi_config config;
};

#endif
