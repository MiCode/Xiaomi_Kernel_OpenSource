/*
 * Copyright (c) 2014-2015 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
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
#ifndef MTK_IOMMU_SMI_H
#define MTK_IOMMU_SMI_H

#include <linux/device.h>

#ifdef CONFIG_MTK_SMI

/*
 * Record the iommu info for each port in the local arbiter.
 * It is only for iommu.
 *
 * Returns 0 if successfully, others if failed.
 */
int mtk_smi_config_port(struct device *larbdev, unsigned int larbportid,
			bool enable);
/*
 * The two function below config iommu and enable/disable the clock
 * for the larb.
 *
 * mtk_smi_larb_get must be called before the multimedia HW work.
 * mtk_smi_larb_put must be called after HW done.
 * Both should be called in non-atomic context.
 *
 * Returns 0 if successfully, others if failed.
 */
int mtk_smi_larb_get(struct device *larbdev);
void mtk_smi_larb_put(struct device *larbdev);

#else

static int
mtk_smi_config_port(struct device *larbdev, unsigned int larbportid,
		    bool enable)
{
	return 0;
}

static inline int mtk_smi_larb_get(struct device *larbdev)
{
	return 0;
}

static inline void mtk_smi_larb_put(struct device *larbdev) { }

#endif

#endif
