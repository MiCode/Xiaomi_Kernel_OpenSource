/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef _DT_BINDINGS_MMDVFS_CLK_MT6983_H
#define _DT_BINDINGS_MMDVFS_CLK_MT6983_H

/* For clock consumer */
#define CLK_MMDVFS_DISP0	(0)
#define CLK_MMDVFS_DISP1	(1)
#define CLK_MMDVFS_MDP0		(2)
#define CLK_MMDVFS_MDP1		(3)
#define CLK_MMDVFS_CAM		(4)
#define CLK_MMDVFS_SENIF	(5)
#define CLK_MMDVFS_IMG		(6)
#define CLK_MMDVFS_MMINFRA	(7)
#define CLK_MMDVFS_VFMT		(8)
#define CLK_MMDVFS_JPEGENC	(9)
#define CLK_MMDVFS_NR		(10)

/* For clock provider */
#define PWR_VCORE	(0)
#define PWR_OTHER	(1)

#define USER_MMDVFS_DISP0	(0)
#define USER_MMDVFS_DISP1	(1)
#define USER_MMDVFS_MDP0	(2)
#define USER_MMDVFS_MDP1	(3)
#define USER_MMDVFS_CAM		(4)
#define USER_MMDVFS_SENIF	(5)
#define USER_MMDVFS_IMG		(6)
#define USER_MMDVFS_MMINFRA	(7)
#define USER_MMDVFS_VFMT	(8)
#define USER_MMDVFS_JPEGENC	(9)

#endif /* _DT_BINDINGS_MMDVFS_CLK_MT6983_H */
