/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include "debug.h"
#include "hif.h"
#include "htc.h"
#include "ce.h"
#include "snoc.h"
#include <soc/qcom/icnss.h>
#include <linux/of.h>
#include <linux/platform_device.h>

void ath10k_snoc_write32(void *ar, u32 offset, u32 value)
{
}

u32 ath10k_snoc_read32(void *ar, u32 offset)
{
	u32 val = 0;
	return val;
}

static int ath10k_snoc_hif_tx_sg(struct ath10k *ar, u8 pipe_id,
				 struct ath10k_hif_sg_item *items, int n_items)
{
	return 0;
}

static u16 ath10k_snoc_hif_get_free_queue_number(struct ath10k *ar, u8 pipe)
{
	return 0;
}

static void ath10k_snoc_hif_send_complete_check(struct ath10k *ar, u8 pipe,
						int force)
{
}

static int ath10k_snoc_hif_map_service_to_pipe(struct ath10k *ar,
					       u16 service_id,
					       u8 *ul_pipe, u8 *dl_pipe)
{
	return 0;
}

static void ath10k_snoc_hif_get_default_pipe(struct ath10k *ar,
					     u8 *ul_pipe, u8 *dl_pipe)
{
}

static void ath10k_snoc_hif_stop(struct ath10k *ar)
{
}

static void ath10k_snoc_hif_power_down(struct ath10k *ar)
{
}

static int ath10k_snoc_hif_start(struct ath10k *ar)
{
	return 0;
}

static int ath10k_snoc_hif_power_up(struct ath10k *ar)
{
	return 0;
}

static const struct ath10k_hif_ops ath10k_snoc_hif_ops = {
	.tx_sg			= ath10k_snoc_hif_tx_sg,
	.start			= ath10k_snoc_hif_start,
	.stop			= ath10k_snoc_hif_stop,
	.map_service_to_pipe	= ath10k_snoc_hif_map_service_to_pipe,
	.get_default_pipe	= ath10k_snoc_hif_get_default_pipe,
	.send_complete_check	= ath10k_snoc_hif_send_complete_check,
	.get_free_queue_number	= ath10k_snoc_hif_get_free_queue_number,
	.power_up		= ath10k_snoc_hif_power_up,
	.power_down		= ath10k_snoc_hif_power_down,
	.read32			= ath10k_snoc_read32,
	.write32		= ath10k_snoc_write32,
};

static int ath10k_snoc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct ath10k *ar;
	struct ath10k_snoc *ar_snoc;
	enum ath10k_hw_rev hw_rev;
	struct device *dev;

	dev = &pdev->dev;
	hw_rev = ATH10K_HW_WCN3990;
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(37));
	ar = ath10k_core_create(sizeof(*ar_snoc), dev, ATH10K_BUS_SNOC,
				hw_rev, &ath10k_snoc_hif_ops);
	if (!ar) {
		dev_err(dev, "failed to allocate core\n");
		return -ENOMEM;
	}
	ath10k_dbg(ar, ATH10K_DBG_SNOC, "%s:WCN3990 probed\n", __func__);

	return ret;
}

static int ath10k_snoc_remove(struct platform_device *pdev)
{
	struct ath10k *ar = platform_get_drvdata(pdev);
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	if (!ar)
		return -EINVAL;

	if (!ar_snoc)
		return -EINVAL;

	ath10k_core_destroy(ar);
	ath10k_dbg(ar, ATH10K_DBG_SNOC, "%s:WCN3990 removed\n", __func__);

	return 0;
}

static const struct of_device_id ath10k_snoc_dt_match[] = {
	{.compatible = "qcom,wcn3990-wifi"},
	{}
};
MODULE_DEVICE_TABLE(of, ath10k_snoc_dt_match);

static struct platform_driver ath10k_snoc_driver = {
		.probe  = ath10k_snoc_probe,
		.remove = ath10k_snoc_remove,
		.driver = {
			.name   = "ath10k_snoc",
			.owner = THIS_MODULE,
			.of_match_table = ath10k_snoc_dt_match,
		},
};

static int __init ath10k_snoc_init(void)
{
	int ret;

	ret = platform_driver_register(&ath10k_snoc_driver);
	if (ret)
		pr_err("failed to register ath10k snoc driver: %d\n",
		       ret);

	return ret;
}
module_init(ath10k_snoc_init);

static void __exit ath10k_snoc_exit(void)
{
	platform_driver_unregister(&ath10k_snoc_driver);
}
module_exit(ath10k_snoc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Driver support for Atheros WCN3990 SNOC devices");
