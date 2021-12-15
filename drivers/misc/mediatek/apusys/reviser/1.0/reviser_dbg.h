// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_REVISER_DEBUG_H__
#define __APUSYS_REVISER_DEBUG_H__

#define REVISER_DBG_DIR "reviser"
#define REVISER_DBG_SUBDIR_HW "hw"
#define REVISER_DBG_SUBDIR_TABLE "table"
#define REVISER_DBG_SUBDIR_MEM "mem"
#define REVISER_DBG_SUBDIR_ERR "err"

int reviser_dbg_init(struct reviser_dev_info *reviser_device);
int reviser_dbg_destroy(struct reviser_dev_info *reviser_device);

#endif
