/*
 * SPRD cp dump driver in AP side.
 *
 * Copyright (C) 2021 Spreadtrum Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is used for CP dump in AP side for
 * Spreadtrum SoCs.
 */

#ifndef SPRD_CP_DUMP_LOADER_H
#define SPRD_CP_DUMP_LOADER_H

#include <../drivers/unisoc_platform/modem/power_manager/sprd_mpm.h>

/* cp region data define */
#define MAX_REGION_NAME_LEN	30
#define MAX_REGION_CNT		20


struct cp_region_info {
	u64	address;
	u32	size;
	u32	mini_dump_flag;
	char	name[MAX_REGION_NAME_LEN + 1];
};

struct cp_dump_info {
	u32	region_cnt;
	struct cp_region_info	regions[MAX_REGION_CNT];
};

struct pm_reg_ctrl {
	u32 reg_offset;	/* offset value */
	u32 reg_mask;	/* mask bit */
	u32 reg_save;	/* pre reg bit */
	struct regmap *ctrl_map;
};

struct cp_dump_device {
	struct cp_dump_info	*dump_info;

	u8	read_region;

	struct mutex	rd_mutex;	/* mutex for read lock */
	struct mutex	wt_mutex;	/* mutex for write lock */
	char	rd_lock_name[TASK_COMM_LEN];
	char	wt_lock_name[TASK_COMM_LEN];

	struct wakeup_source	*rd_ws;
	struct wakeup_source	*wt_ws;

	struct device		*p_dev;
	struct miscdevice	mdev;

	struct pm_reg_ctrl *sp_ctrl;
	struct pm_reg_ctrl *ch_ctrl;
};


#endif
