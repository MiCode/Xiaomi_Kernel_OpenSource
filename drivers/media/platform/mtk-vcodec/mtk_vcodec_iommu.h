/*
* Copyright (c) 2015 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*         Tiffany Lin <tiffany.lin@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#ifndef MTK_VCODEC_IOMMU_H_
#define MTK_VCODEC_IOMMU_H_

#include "mtk_vcodec_drv.h"

int mtk_vcodec_iommu_init(struct device *dev);
void mtk_vcodec_iommu_deinit(struct device *dev);

#endif
