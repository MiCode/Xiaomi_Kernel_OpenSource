// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: secure " fmt

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/export.h>
#if 0 //IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif

int mtk_iommu_sec_bk_init(uint32_t type, uint32_t id)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_sec_bk_init);

/*
 * Call iommu tf-a smc to dump iommu secure bank reg
 * Return 0 is fail, 1 is success.
 */
int mtk_iommu_sec_bk_tf(uint32_t type, uint32_t id, u64 *iova, u64 *pa, u32 *fault_id)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_sec_bk_tf);

static int mtk_iommu_sec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));
	return 0;
}

static const struct of_device_id mtk_iommu_bank_of_ids[] = {
	{ .compatible = "mediatek,common-disp-iommu-bank1"},
	{ .compatible = "mediatek,common-disp-iommu-bank2"},
	{ .compatible = "mediatek,common-disp-iommu-bank3"},
	{ .compatible = "mediatek,common-disp-iommu-bank4"},
	{ .compatible = "mediatek,common-mdp-iommu-bank1"},
	{ .compatible = "mediatek,common-mdp-iommu-bank2"},
	{ .compatible = "mediatek,common-mdp-iommu-bank3"},
	{ .compatible = "mediatek,common-mdp-iommu-bank4"},
	{ .compatible = "mediatek,common-apu-iommu0-bank1"},
	{ .compatible = "mediatek,common-apu-iommu0-bank2"},
	{ .compatible = "mediatek,common-apu-iommu0-bank3"},
	{ .compatible = "mediatek,common-apu-iommu0-bank4"},
	{ .compatible = "mediatek,common-apu-iommu1-bank1"},
	{ .compatible = "mediatek,common-apu-iommu1-bank2"},
	{ .compatible = "mediatek,common-apu-iommu1-bank3"},
	{ .compatible = "mediatek,common-apu-iommu1-bank4"},
	{ .compatible = "mediatek,common-peri-iommu-m4-bank1"},
	{ .compatible = "mediatek,common-peri-iommu-m4-bank2"},
	{ .compatible = "mediatek,common-peri-iommu-m4-bank3"},
	{ .compatible = "mediatek,common-peri-iommu-m4-bank4"},
	{ .compatible = "mediatek,common-peri-iommu-m6-bank1"},
	{ .compatible = "mediatek,common-peri-iommu-m6-bank2"},
	{ .compatible = "mediatek,common-peri-iommu-m6-bank3"},
	{ .compatible = "mediatek,common-peri-iommu-m6-bank4"},
	{ .compatible = "mediatek,common-peri-iommu-m7-bank1"},
	{ .compatible = "mediatek,common-peri-iommu-m7-bank2"},
	{ .compatible = "mediatek,common-peri-iommu-m7-bank3"},
	{ .compatible = "mediatek,common-peri-iommu-m7-bank4"},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_iommu_bank_of_ids);

static struct platform_driver disp_iommu_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "disp-iommu-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver disp_iommu_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "disp-iommu-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver disp_iommu_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "disp-iommu-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver disp_iommu_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "disp-iommu-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver mdp_iommu_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "mdp-iommu-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver mdp_iommu_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "mdp-iommu-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver mdp_iommu_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "mdp-iommu-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver mdp_iommu_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "mdp-iommu-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu0_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu0-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu0_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu0-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu0_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu0-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu0_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu0-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu1_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu1-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu1_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu1-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu1_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu1-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu1_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu1-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m4_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m4-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m4_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m4-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m4_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m4-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m4_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m4-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m6_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m6-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m6_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m6-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m6_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m6-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m6_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m6-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m7_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m7-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m7_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m7-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m7_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m7-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m7_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m7-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver *const mtk_iommu_bk_drivers[] = {
	&disp_iommu_bank1_driver,
	&disp_iommu_bank2_driver,
	&disp_iommu_bank3_driver,
	&disp_iommu_bank4_driver,
	&mdp_iommu_bank1_driver,
	&mdp_iommu_bank2_driver,
	&mdp_iommu_bank3_driver,
	&mdp_iommu_bank4_driver,
	&apu_iommu0_bank1_driver,
	&apu_iommu0_bank2_driver,
	&apu_iommu0_bank3_driver,
	&apu_iommu0_bank4_driver,
	&apu_iommu1_bank1_driver,
	&apu_iommu1_bank2_driver,
	&apu_iommu1_bank3_driver,
	&apu_iommu1_bank4_driver,
	&peri_iommu_m4_bank1_driver,
	&peri_iommu_m4_bank2_driver,
	&peri_iommu_m4_bank3_driver,
	&peri_iommu_m4_bank4_driver,
	&peri_iommu_m6_bank1_driver,
	&peri_iommu_m6_bank2_driver,
	&peri_iommu_m6_bank3_driver,
	&peri_iommu_m6_bank4_driver,
	&peri_iommu_m7_bank1_driver,
	&peri_iommu_m7_bank2_driver,
	&peri_iommu_m7_bank3_driver,
	&peri_iommu_m7_bank4_driver,
};

static int __init mtk_iommu_sec_init(void)
{
	int ret;
	int i;

	pr_info("%s+\n", __func__);
	for (i = 0; i < ARRAY_SIZE(mtk_iommu_bk_drivers); i++) {
		ret = platform_driver_register(mtk_iommu_bk_drivers[i]);
		if (ret < 0) {
			pr_err("Failed to register %s driver: %d\n",
				  mtk_iommu_bk_drivers[i]->driver.name, ret);
			goto err;
		}
	}
	pr_info("%s-\n", __func__);

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(mtk_iommu_bk_drivers[i]);

	return ret;
}

static void __exit mtk_iommu_sec_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(mtk_iommu_bk_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(mtk_iommu_bk_drivers[i]);
}

module_init(mtk_iommu_sec_init);
module_exit(mtk_iommu_sec_exit);
MODULE_LICENSE("GPL v2");
