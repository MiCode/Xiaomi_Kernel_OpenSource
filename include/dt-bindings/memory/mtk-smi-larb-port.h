/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 */
#ifndef __DTS_MTK_IOMMU_PORT_H_
#define __DTS_MTK_IOMMU_PORT_H_

#define SMI_LARB_PORT_NR_MAX		32
#define MTK_LARB_NR_MAX			32
#define MTK_M4U_DOM_NR_MAX		8

#define MTK_M4U_DOM_ID(dom, larb, port)	((dom & 0x7) << 12 |\
					 ((larb & 0x1f) << 5) | (port & 0x1f))

/* The default dom is 0. */
#define MTK_M4U_ID(larb, port)		MTK_M4U_DOM_ID(0, larb, port)
#define MTK_M4U_TO_LARB(id)		(((id) >> 5) & 0x1f)
#define MTK_M4U_TO_PORT(id)		((id) & 0x1f)
#define MTK_M4U_TO_DOM(id)		(((id) >> 12) & 0x7)

#endif
