/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
