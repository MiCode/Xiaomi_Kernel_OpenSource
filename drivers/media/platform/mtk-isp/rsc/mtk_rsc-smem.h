/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MTK_RSC_SMEM_H__
#define __MTK_RSC_SMEM_H__

#include <linux/dma-mapping.h>

phys_addr_t mtk_rsc_smem_iova_to_phys(struct device *smem_dev,
				     dma_addr_t iova);
void mtk_rsc_smem_enable_mpu(struct device *smem_dev);

#endif /*__MTK_RSC_SMEM_H__*/
