// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of_device.h>
#include <linux/dma-mapping.h>

#include "apu.h"

void apu_mem_remove(struct mtk_apu *apu)
{
}

int apu_mem_init(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret;

	if ((apu->platdata->flags & F_BYPASS_IOMMU) == 0) {
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
		if (ret) {
			dev_info(dev, "%s: dma_set_mask_and_coherent fail(%d)\n", __func__, ret);
			return -ENOMEM;
		}
	}

	return 0;
}
