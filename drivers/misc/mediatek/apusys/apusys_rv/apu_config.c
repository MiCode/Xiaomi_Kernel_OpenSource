// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

#include "apu.h"
#include "apu_hw.h"
#include "apu_config.h"
#include "sw_logger.h"
#include "hw_logger.h"
#include "mvpu_plat_device.h"

void apu_config_user_ptr_init(const struct mtk_apu *apu)
{
	struct device *dev;
	struct config_v1 *config;
	struct config_v1_entry_table *entry_table;

	if (!apu || !apu->conf_buf) {
		pr_info("%s: error\n", __func__);
		return;
	}
	dev = apu->dev;

	config = apu->conf_buf;

	config->header_magic = 0xc0de0101;
	config->header_rev = 0x1;
	config->entry_offset = offsetof(struct config_v1, entry_tbl);
	config->config_size = sizeof(struct config_v1);

	entry_table = (struct config_v1_entry_table *)((void *)config +
		config->entry_offset);

	entry_table->user_entry[0] = offsetof(struct config_v1, user0_data);
	entry_table->user_entry[1] = offsetof(struct config_v1, user1_data);
	entry_table->user_entry[2] = offsetof(struct config_v1, user2_data);
	entry_table->user_entry[3] = offsetof(struct config_v1, user3_data);
	entry_table->user_entry[4] = offsetof(struct config_v1, user4_data);
	entry_table->user_entry[5] = offsetof(struct config_v1, user5_data);
}

int apu_config_setup(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned long flags;
	int ret;

	apu->conf_buf = dma_alloc_coherent(apu->dev, CONFIG_SIZE,
					&apu->conf_da, GFP_KERNEL);

	if (apu->conf_buf == NULL || apu->conf_da == 0) {
		dev_info(dev, "%s: dma_alloc_coherent fail\n", __func__);
		return -ENOMEM;
	}

	dev_info(dev, "%s: apu->conf_buf = 0x%llx, apu->conf_da = 0x%llx\n",
		__func__, (uint64_t) apu->conf_buf, (uint64_t) apu->conf_da);

	memset(apu->conf_buf, 0, CONFIG_SIZE);

	apu_config_user_ptr_init(apu);

	spin_lock_irqsave(&apu->reg_lock, flags);
	/* Set config addr in mbox */
	iowrite32((u32)apu->conf_da,
		apu->apu_mbox + MBOX_HOST_CONFIG_ADDR);
	spin_unlock_irqrestore(&apu->reg_lock, flags);

	apu->conf_buf->time_offset = sched_clock();

	ret = apu_ipi_config_init(apu);
	if (ret) {
		dev_info(dev, "apu ipi config init failed\n");
		goto out;
	}

	//@@@ret = reviser_set_init_info(apu);
	//@@@if (ret) {
	//@@@	dev_info(apu->dev, "apu reviser config init failed\n");
	//@@@	goto out;
	//@@@}

	//@@@ret = vpu_set_init_info(apu);
	//@@@if (ret) {
	//@@@	dev_info(apu->dev, "apu vpu config init failed\n");
	//@@@	goto out;
	//@@@}

	//@@@ret = power_set_chip_info(apu);
	//@@@if (ret) {
	//@@@	dev_info(apu->dev, "set chip info fail ret:%d\n", ret);
	//@@@	goto out;
	//@@@}

	ret = sw_logger_config_init(apu);
	if (ret) {
		dev_info(dev, "sw logger config init failed\n");
		goto out;
	}

	ret = hw_logger_config_init(apu);
	if (ret) {
		dev_info(apu->dev, "hw logger config init failed\n");
		goto out;
	}

	ret = mvpu_config_init(apu);
	if (ret) {
		dev_info(apu->dev, "mvpu config init failed\n");
		goto out;
	}

	return 0;

out:
	return ret;
}

void apu_config_remove(struct mtk_apu *apu)
{
	apu_ipi_config_remove(apu);
	mvpu_config_remove(apu);

	dma_free_coherent(apu->dev, CONFIG_SIZE,
		apu->conf_buf, apu->conf_da);
}
