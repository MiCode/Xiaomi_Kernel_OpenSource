/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __EDMA_DBGFS_H__
#define __EDMA_DBGFS_H__

int edma_create_sysfs(struct device *dev);
void edma_remove_sysfs(struct device *dev);
int edma_dbg_check_ststus(int check_status);

enum edma_dbg_cfg {
	EDMA_DBG_DISABLE_PWR_OFF = 0x1,
	EDMA_DBG_ALL = 0xFFFFFFF,
};

enum {
	EDMA_LOG_WARN,
	EDMA_LOG_INFO,
	EDMA_LOG_DEBUG,
};

extern u8 g_edma_log_lv;


#endif /* __EDMA_DBGFS_H__ */
