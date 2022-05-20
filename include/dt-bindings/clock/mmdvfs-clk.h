/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef _DT_BINDINGS_MMDVFS_CLK_H
#define _DT_BINDINGS_MMDVFS_CLK_H

/* clock consumer */
#define CLK_MMDVFS_DISP		(0)
#define CLK_MMDVFS_MDP		(1)
#define CLK_MMDVFS_MML		(2)
#define CLK_MMDVFS_SMI_COMMON0	(3)
#define CLK_MMDVFS_SMI_COMMON1	(4)

#define CLK_MMDVFS_VENC		(5)
#define CLK_MMDVFS_JPEGENC	(6)

#define CLK_MMDVFS_VDEC		(7)
#define CLK_MMDVFS_VFMT		(8)
#define CLK_MMDVFS_JPEGDEC	(9)

#define CLK_MMDVFS_IMG		(10)
#define CLK_MMDVFS_IPE		(11)
#define CLK_MMDVFS_CAM		(12)
#define CLK_MMDVFS_CCU		(13)

#define CLK_MMDVFS_VCORE	(14)
#define CLK_MMDVFS_VMM		(15)
#define CLK_MMDVFS_VDISP	(16)

/* new clk append here */
#define CLK_MMDVFS_NUM		(17)

/* power supplier */
#define PWR_MMDVFS_VCORE	(0)
#define PWR_MMDVFS_VMM		(1)
#define PWR_MMDVFS_VDISP	(2)
#define PWR_MMDVFS_NUM		(3)

/* ipi type */
#define IPI_MMDVFS_VCP		(0)
#define IPI_MMDVFS_CCU		(1)

/* spec type */
#define SPEC_MMDVFS_NORMAL	(0)
#define SPEC_MMDVFS_ALONE	(1)

#endif /* _DT_BINDINGS_MMDVFS_CLK_H */
