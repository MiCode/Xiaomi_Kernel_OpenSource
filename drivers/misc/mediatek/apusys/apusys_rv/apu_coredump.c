// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <apu.h>
#include <apu_excep.h>


void apu_setup_dump(struct mtk_apu *apu, dma_addr_t da)
{
	/* Set dump addr in mbox */
	apu->conf_buf->ramdump_offset = da;

	/* Set coredump type(AP dump by default) */
	apu->conf_buf->ramdump_type = 0;
}

int apu_coredump_init(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret = 0;

	if (apu->platdata->flags & F_SECURE_COREDUMP)
		return 0;

	apu->coredump_buf = dma_alloc_coherent(
		apu->dev, COREDUMP_SIZE,
			&apu->coredump_da, GFP_KERNEL);
	if (apu->coredump_buf == NULL || apu->coredump_da == 0) {
		dev_info(dev, "%s: dma_alloc_coherent fail\n", __func__);
		return -ENOMEM;
	}

	dev_info(dev, "%s: apu->coredump_buf = 0x%llx, apu->coredump_da = 0x%llx\n",
		__func__, (uint64_t) apu->coredump_buf,
		(uint64_t) apu->coredump_da);

	memset(apu->coredump_buf, 0, sizeof(struct apu_coredump));

	apu_setup_dump(apu, apu->coredump_da);

	return ret;
}

void apu_coredump_remove(struct mtk_apu *apu)
{
	if ((apu->platdata->flags & F_SECURE_COREDUMP) == 0)
		dma_free_coherent(
			apu->dev, COREDUMP_SIZE, apu->coredump_buf, apu->coredump_da);
}
