// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <memory/mediatek/emi.h>

DEFINE_SPINLOCK(emidbg_lock);
static struct platform_device *emicen_pdev;

static int emicen_probe(struct platform_device *pdev)
{
	struct device_node *emicen_node = pdev->dev.of_node;
	struct device_node *emichn_node =
		of_parse_phandle(emicen_node, "mediatek,emi-reg", 0);
	struct emicen_dev_t *emicen_dev_ptr;
	unsigned int i;
	int ret;

	pr_info("%s: module probe.\n", __func__);
	emicen_dev_ptr = devm_kmalloc(&pdev->dev,
		sizeof(struct emicen_dev_t), GFP_KERNEL);
	if (!emicen_dev_ptr)
		return -ENOMEM;

	ret = of_property_read_u32(emicen_node,
		"ch_cnt", &(emicen_dev_ptr->ch_cnt));
	if (ret) {
		pr_info("%s: get ch_cnt fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(emicen_node,
		"rk_cnt", &(emicen_dev_ptr->rk_cnt));
	if (ret) {
		pr_info("%s: get rk_cnt fail\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: %s(%d), %s(%d)\n", __func__,
		"ch_cnt", emicen_dev_ptr->ch_cnt,
		"rk_cnt", emicen_dev_ptr->rk_cnt);

	emicen_dev_ptr->rk_size = devm_kmalloc_array(&pdev->dev,
		emicen_dev_ptr->rk_cnt, sizeof(unsigned long long),
		GFP_KERNEL);
	if (!(emicen_dev_ptr->rk_size))
		return -ENOMEM;
	ret = of_property_read_u64_array(emicen_node,
		"rk_size", emicen_dev_ptr->rk_size, emicen_dev_ptr->rk_cnt);

	for (i = 0; i < emicen_dev_ptr->rk_cnt; i++)
		pr_info("%s: rk_size%d(0x%llx)\n", __func__,
			i, emicen_dev_ptr->rk_size[i]);

	emicen_dev_ptr->emi_cen_cnt = of_property_count_elems_of_size(
		emicen_node, "reg", sizeof(unsigned int) * 4);
	if (emicen_dev_ptr->emi_cen_cnt <= 0) {
		pr_info("%s: get emi_cen_cnt fail\n", __func__);
		return -EINVAL;
	}
	emicen_dev_ptr->emi_cen_base = devm_kmalloc_array(&pdev->dev,
		emicen_dev_ptr->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(emicen_dev_ptr->emi_cen_base))
		return -ENOMEM;
	for (i = 0; i < emicen_dev_ptr->emi_cen_cnt; i++)
		emicen_dev_ptr->emi_cen_base[i] = of_iomap(emicen_node, i);

	emicen_dev_ptr->emi_chn_base = devm_kmalloc_array(&pdev->dev,
		emicen_dev_ptr->ch_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(emicen_dev_ptr->emi_chn_base))
		return -ENOMEM;
	for (i = 0; i < emicen_dev_ptr->ch_cnt; i++)
		emicen_dev_ptr->emi_chn_base[i] = of_iomap(emichn_node, i);

	platform_set_drvdata(pdev, emicen_dev_ptr);
	emicen_pdev = pdev;

	return ret;
}

static int emicen_remove(struct platform_device *dev)
{
	return 0;
}

static const struct of_device_id emicen_of_ids[] = {
	{.compatible = "mediatek,common-emicen",},
	{}
};

static struct platform_driver emicen_drv = {
	.probe = emicen_probe,
	.remove = emicen_remove,
	.driver = {
		.name = "emicen_drv",
		.owner = THIS_MODULE,
		.of_match_table = emicen_of_ids,
	},
};

static int __init emicen_drv_init(void)
{
	int ret;

	ret = platform_driver_register(&emicen_drv);
	if (ret) {
		pr_info("%s: init fail, ret 0x%x\n", __func__, ret);
		return ret;
	}

	return ret;
}

static void __exit emicen_drv_exit(void)
{
	platform_driver_unregister(&emicen_drv);
}

module_init(emicen_drv_init);
module_exit(emicen_drv_exit);

/*
 * mtk_emicen_get_ch_cnt - get the channel count
 *
 * Returns the channel count
 */
unsigned int mtk_emicen_get_ch_cnt(void)
{
	struct emicen_dev_t *emicen_dev_ptr;

	if (!emicen_pdev)
		return 0;

	emicen_dev_ptr =
		(struct emicen_dev_t *)platform_get_drvdata(emicen_pdev);

	return emicen_dev_ptr->ch_cnt;
}
EXPORT_SYMBOL(mtk_emicen_get_ch_cnt);

/*
 * mtk_emicen_get_rk_cnt - get the rank count
 *
 * Returns the rank count
 */
unsigned int mtk_emicen_get_rk_cnt(void)
{
	struct emicen_dev_t *emicen_dev_ptr;

	if (!emicen_pdev)
		return 0;

	emicen_dev_ptr =
		(struct emicen_dev_t *)platform_get_drvdata(emicen_pdev);

	return emicen_dev_ptr->rk_cnt;
}
EXPORT_SYMBOL(mtk_emicen_get_rk_cnt);

/*
 * mtk_emicen_get_rk_size - get the rank size of target rank
 * @rk_id:	the id of target rank
 *
 * Returns the rank size of target rank
 */
unsigned int mtk_emicen_get_rk_size(unsigned int rk_id)
{
	struct emicen_dev_t *emicen_dev_ptr;

	if (!emicen_pdev)
		return 0;

	emicen_dev_ptr =
		(struct emicen_dev_t *)platform_get_drvdata(emicen_pdev);

	if (rk_id < emicen_dev_ptr->rk_cnt)
		return emicen_dev_ptr->rk_size[rk_id];

	return 0;
}
EXPORT_SYMBOL(mtk_emicen_get_rk_size);

/*
 * mtk_emidbg_dump - dump emi full status to atf log
 *
 */
void mtk_emidbg_dump(void)
{
	unsigned long spinlock_save_flags;
	struct arm_smccc_res smc_res;

	spin_lock_irqsave(&emidbg_lock, spinlock_save_flags);

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIDBG_DUMP,
		0, 0, 0, 0, 0, 0, &smc_res);
	while (smc_res.a0 > 0) {
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIDBG_MSG,
		0, 0, 0, 0, 0, 0, &smc_res);

		pr_info("%s: %d, 0x%x, 0x%x, 0x%x\n", __func__,
			(int)smc_res.a0,
			(unsigned int)smc_res.a1,
			(unsigned int)smc_res.a2,
			(unsigned int)smc_res.a3);
	}

	spin_unlock_irqrestore(&emidbg_lock, spinlock_save_flags);
}
EXPORT_SYMBOL(mtk_emidbg_dump);

MODULE_DESCRIPTION("MediaTek EMICEN Driver v0.1");

