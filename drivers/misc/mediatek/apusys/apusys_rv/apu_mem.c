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
	int ret;

	ret = of_dma_configure(apu->dev, apu->dev->of_node, true);
	if (ret) {
		pr_info("%s: of_dma_configure fail(%d)\n", __func__, ret);
		return -ENOMEM;
	}

	ret = dma_set_mask_and_coherent(apu->dev, DMA_BIT_MASK(34));
	if (ret) {
		pr_info("%s: dma_set_mask_and_coherent fail(%d)\n", __func__, ret);
		return -ENOMEM;
	}

	return 0;
}
