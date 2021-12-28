// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#include <linux/module.h>
#include <linux/of_device.h>
#include "mvpu_plat_device.h"
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

static struct mvpu_plat_drv mt6983_drv = { .sw_preemption_level = 1, };

static struct mvpu_plat_drv mt8139_drv = { .sw_preemption_level = 1, };

static struct mvpu_plat_drv mt6879_drv = { .sw_preemption_level = 1, };

static struct mvpu_plat_drv mt6895_drv = { .sw_preemption_level = 1, };

static const struct of_device_id mvpu_of_match[] = {
	{
	.compatible = "mediatek, mt6983-mvpu",
	.data = &mt6983_drv
	},
	{
	.compatible = "mediatek, mt8139-mvpu",
	.data = &mt8139_drv
	},
	{
	.compatible = "mediatek, mt6879-mvpu",
	.data = &mt6879_drv
	},
	{
	.compatible = "mediatek, mt6895-mvpu",
	.data = &mt6895_drv
	},
	{
	/* end of list */

	},
};

MODULE_DEVICE_TABLE(of, mvpu_of_match);

const struct of_device_id *mvpu_plat_get_device(void)
{
	return mvpu_of_match;
}

int mvpu_plat_info_init(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &(pdev->dev);
	struct mvpu_plat_drv *plat_drv;

	of_property_read_u32(dev->of_node, "core_num", &nr_core_ids);

	dev_info(&pdev->dev, "nr_core_ids = %d\n", nr_core_ids);

	if (nr_core_ids > MAX_CORE_NUM) {
		dev_info(dev, "Invalid core number: %d\n", nr_core_ids);
		nr_core_ids = 1;
	}

	if (of_property_read_u32(pdev->dev.of_node, "version", &mvpu_ver) == 0)
		dev_info(&pdev->dev, "ver = %x\n", mvpu_ver);

	plat_drv = (struct mvpu_plat_drv *) of_device_get_match_data(dev);

	if (!plat_drv) {
		dev_info(dev, "%s: of_device_get_match_data mvpu plat_drv is null\n", __func__);
		return -1;
	}

	sw_preemption_level = plat_drv->sw_preemption_level;

	dev_info(dev, "core number = %d, sw_preemption_level = 0x%x\n",
		nr_core_ids, sw_preemption_level);
	return ret;

}
int mvpu_plat_init(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &(pdev->dev);
	uint64_t mask = 0;

	// get dma mask
	of_property_read_u64(dev->of_node, "mask", &mask);

	pr_info("%s mask 0x%llx\n", __func__, mask);

	ret = dma_set_mask_and_coherent(dev, mask);
	if (ret) {
		dev_info(&pdev->dev, "unable to set DMA mask coherent: %d\n", ret);
		return ret;
	}

	pr_info("%s dma_set_mask_and_coherent 0x%llx\n", __func__, mask);

	ret = dma_set_mask(dev, mask);
	if (ret) {
		dev_info(&pdev->dev, "unable to set DMA mask: %d\n", ret);
		return ret;
	}

	pr_info("%s dma_set_mask 0x%llx\n", __func__, mask);

	return ret;

}

int mvpu_config_init(struct mtk_apu *apu)
{
	int id, level = 0;

	struct mvpu_preempt_data *info;
	dma_addr_t mvpu_da_itcm;
	dma_addr_t mvpu_da_l1;
	uint32_t *addr0;
	uint32_t *addr1;

	pr_info("%s core number = %d, sw_preemption_level = 0x%x\n", __func__,
			nr_core_ids, sw_preemption_level);

	info = (struct mvpu_preempt_data *) get_apu_config_user_ptr(
		apu->conf_buf, eMVPU_PREEMPT_DATA);

	for (id = 0; id < nr_core_ids; id++) {

		for (level = 0; level < sw_preemption_level; level++) {
			if (id == 0) {
				addr0 = dma_alloc_coherent(apu->dev, PREEMPT_ITCM_BUFFER,
					&mvpu_da_itcm, GFP_KERNEL);
				itcm_kernel_addr_core_0[level] = addr0;

				info->itcm_buffer_core_0[level] = (uint32_t) mvpu_da_itcm;

				if (addr0 == NULL || mvpu_da_itcm == 0) {
					pr_info("%s: dma_alloc_coherent fail\n", __func__);
					return -ENOMEM;
				}

				pr_info("core 0 itcm kernel va = 0x%llx, core 0 itcm iova = 0x%llx\n",
					addr0, mvpu_da_itcm);

				memset(addr0, 0, PREEMPT_ITCM_BUFFER);

				addr1 = dma_alloc_coherent(apu->dev, PREEMPT_L1_BUFFER,
					&mvpu_da_l1, GFP_KERNEL);
				l1_kernel_addr_core_0[level] = addr1;
				info->l1_buffer_core_0[level] = mvpu_da_l1;

				if (addr1 == NULL || mvpu_da_l1 == 0) {
					pr_info("dma_alloc_coherent fail\n");

					dma_free_coherent(apu->dev, PREEMPT_ITCM_BUFFER,
							addr0, mvpu_da_itcm);

					return -ENOMEM;
				}

				pr_info("core 0 L1 kernel va = 0x%llx, core 0 L1 iova = 0x%llx\n",
					addr1, mvpu_da_l1);

				memset(addr1, 0, PREEMPT_L1_BUFFER);

			} else if (id == 1) {

				addr0 = dma_alloc_coherent(apu->dev, PREEMPT_ITCM_BUFFER,
					&mvpu_da_itcm, GFP_KERNEL);
				itcm_kernel_addr_core_1[level] = addr0;

				info->itcm_buffer_core_1[level] = (uint32_t) mvpu_da_itcm;

				if (addr0 == NULL || mvpu_da_itcm == 0) {
					pr_info("dma_alloc_coherent fail\n");
					return -ENOMEM;
				}

				pr_info("addr0 = 0x%llx, mvpu_da_itcm = 0x%llx\n",
					addr0, mvpu_da_itcm);

				memset(addr0, 0, PREEMPT_ITCM_BUFFER);

				addr1 = dma_alloc_coherent(apu->dev, PREEMPT_L1_BUFFER,
					&mvpu_da_l1, GFP_KERNEL);

				l1_kernel_addr_core_1[level] = addr1;
				info->l1_buffer_core_1[level] = mvpu_da_l1;

				if (addr1 == NULL || mvpu_da_l1 == 0) {
					pr_info("dma_alloc_coherent fail\n");
					dma_free_coherent(apu->dev, PREEMPT_ITCM_BUFFER,
						addr0, mvpu_da_itcm);
					return -ENOMEM;
				}

				pr_info("addr0 = 0x%llx, mvpu_da_itcm = 0x%llx\n",
					addr1, mvpu_da_l1);

				memset(addr1, 0, PREEMPT_L1_BUFFER);

			} else {
				pr_info("nr_core_ids error\n");
			}

	}
	}
	return 0;
}

int mvpu_config_remove(struct mtk_apu *apu)
{
	int id, level = 0;
	struct mvpu_preempt_data *info;

	info = (struct mvpu_preempt_data *) get_apu_config_user_ptr(
		apu->conf_buf, eMVPU_PREEMPT_DATA);

	for (id = 0; id < nr_core_ids; id++) {
		for (level = 0; level < sw_preemption_level; level++) {

			if (id == 0) {

				if (!l1_kernel_addr_core_0[level] || !info->l1_buffer_core_0
					[level]) {
					dma_free_coherent(apu->dev, PREEMPT_L1_BUFFER,
					l1_kernel_addr_core_0[level], info->l1_buffer_core_0
					[level]);
				}

				if (!itcm_kernel_addr_core_0[level] || !info->itcm_buffer_core_0
					[level]) {
					dma_free_coherent(apu->dev, PREEMPT_ITCM_BUFFER,
					itcm_kernel_addr_core_0[level], info->itcm_buffer_core_0
					[level]);
				}

			} else {
				if (!l1_kernel_addr_core_1[level] || !info->l1_buffer_core_1
					[level]) {
					dma_free_coherent(apu->dev, PREEMPT_L1_BUFFER,
					l1_kernel_addr_core_1[level], info->l1_buffer_core_1
					[level]);
				}

				if (!itcm_kernel_addr_core_1[level] || !info->itcm_buffer_core_1
					[level]) {
					dma_free_coherent(apu->dev, PREEMPT_ITCM_BUFFER,
					itcm_kernel_addr_core_1[level], info->itcm_buffer_core_1
					[level]);
				}
			}

		}
	}
	return 0;
}


