/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MT6877_DBG_FS_COMMON_H__
#define __MT6877_DBG_FS_COMMON_H__

/* Determine for user name handle */
#define MT_LP_RQ_USER_NAME_LEN	(4)
#define MT_LP_RQ_USER_CHAR_U	(8)
#define MT_LP_RQ_USER_CHAR_MASK	(0xFF)

/* Determine for resource usage id */
#define MT_LP_RQ_ID_ALL_USAGE	(-1)

int mt6877_dbg_lpm_init(void);

int mt6877_dbg_lpm_deinit(void);

int mt6877_dbg_lpm_fs_init(void);

int mt6877_dbg_lpm_fs_deinit(void);

int mt6877_dbg_spm_fs_init(void);

int mt6877_dbg_spm_fs_deinit(void);

#endif /* __MTK_LP_PLAT_FS_H__ */
