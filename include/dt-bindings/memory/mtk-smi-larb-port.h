/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 */
#ifndef __DTS_MTK_IOMMU_PORT_H_
#define __DTS_MTK_IOMMU_PORT_H_

#define MTK_LARB_NR_MAX			32

#define MTK_M4U_ID(larb, port)		(((larb) << 5) | (port))
#define MTK_M4U_TO_LARB(id)		(((id) >> 5) & 0xf)
#define MTK_M4U_TO_PORT(id)		((id) & 0x1f)

#endif
