/*
 * Copyright (C)2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_DVFSRC_SMC_REG_H
#define __MTK_DVFSRC_SMC_REG_H

/**************************************
 * Define and Declare
 **************************************/

/* SPM REGISTER*/
#define POWERON_CONFIG_EN              (0x0000)
#define SPM_SW_FLAG_0                  (0x0600)
#define SPM_PC_STA                     (0x01A4)
#define SPM_DVFS_LEVEL                 (0x0708)
#define SPM_DVS_DFS_LEVEL              (0x07BC)
#define SPM_DVFS_STA                   (0x070C)
#define SPM_DVFS_MISC                  (0x04F4)
#define SPM_DVFS_MISC                  (0x04F4)
#define SPM_VCORE_DVFS_SHORTCUT00      (0x0770)
#define SPM_VCORE_DVFS_SHORTCUT_STA0   (0x07B0)
#define SPM_VCORE_DVFS_SHORTCUT_STA1   (0x07B4)
#define SPM_DVFS_HISTORY_STA0          (0x01C0)
#define SPM_DVFS_HISTORY_STA1          (0x01C4)
#define SPM_DVFS_CMD0                  (0x0710)
#define SPM_DVFS_CMD1                  (0x0714)
#define SPM_DVFS_CMD2                  (0x0718)
#define SPM_DVFS_CMD3                  (0x071C)


#endif /* __MTK_DVFSRC_REG_V2_H */
