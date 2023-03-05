// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.

#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
struct pmif_mpu_data {
	const u32	*regs;
};

struct pmif_mpu {
	void __iomem	*base;
	const struct pmif_mpu_data *data;
};

struct pmif_mpu_timer {
	struct pmif_mpu *mpu_arb;
	struct platform_device *mpu_pdev;
	struct timer_list mpu_enable_timer;
};

enum pmif_mpu_regs {
	PMIF_MPU_CTRL,
	PMIF_PMIC_ALL_RGN_EN,
	PMIF_PMIC_ALL_RGN_EN_2,
};

static const u32 pmif_mpu_regs[] = {
	[PMIF_MPU_CTRL] =			0x000,
	[PMIF_PMIC_ALL_RGN_EN] =		0x0B0,
	[PMIF_PMIC_ALL_RGN_EN_2] =		0x430,
};

static struct pmif_mpu_timer mpu_timer;

static u32 pmif_mpu_readl(struct pmif_mpu *arb, enum pmif_mpu_regs reg)
{
	return readl(arb->base + arb->data->regs[reg]);
}

static void pmif_mpu_writel(struct pmif_mpu *arb, u32 val, enum pmif_mpu_regs reg)
{
	writel(val, arb->base + arb->data->regs[reg]);
}

static const struct pmif_mpu_data pmif_mpu_arb = {
	.regs = pmif_mpu_regs,
};

static void enable_kernel_mpu(void)
{
	u32 pmic_all_rgn_en = 0, rgn_en = 0, rgn_en_2 = 0;
	int err;

	struct pmif_mpu_timer *pmt = from_timer(pmt,
				&(mpu_timer.mpu_enable_timer), mpu_enable_timer);
	struct pmif_mpu *mpu_arb = pmt->mpu_arb;
	struct platform_device *mpu_pdev = pmt->mpu_pdev;

	if ((IS_ERR(pmt))) {
		err = PTR_ERR(pmt);
		dev_info(&mpu_pdev->dev, "MPU pmt ptr error err:0x%x\n", err);
		return;
	}

	if ((IS_ERR(mpu_arb))) {
		err = PTR_ERR(mpu_arb);
		dev_info(&mpu_pdev->dev, "MPU mpu_arb ptr error err:0x%x\n", err);
		return;
	}

	if ((IS_ERR(mpu_pdev))) {
		err = PTR_ERR(mpu_pdev);
		dev_info(&mpu_pdev->dev, "MPU mpu_pdev ptr error err:0x%x\n", err);
		return;
	}

	dev_info(&mpu_pdev->dev, "enable kernel stage mpu region\n");

	if (!of_property_read_u32(mpu_pdev->dev.of_node, "mediatek,pmic-all-rgn-en",
				  &pmic_all_rgn_en)) {
		rgn_en = pmif_mpu_readl(mpu_arb, PMIF_PMIC_ALL_RGN_EN);
		rgn_en |= pmic_all_rgn_en;
		pmif_mpu_writel(mpu_arb, rgn_en, PMIF_PMIC_ALL_RGN_EN);
		rgn_en = pmif_mpu_readl(mpu_arb, PMIF_PMIC_ALL_RGN_EN);
	}

	if (!of_property_read_u32(mpu_pdev->dev.of_node, "mediatek,pmic-all-rgn-en-2",
				  &pmic_all_rgn_en)) {
		rgn_en_2 = pmif_mpu_readl(mpu_arb, PMIF_PMIC_ALL_RGN_EN_2);
		rgn_en_2 |= pmic_all_rgn_en;
		pmif_mpu_writel(mpu_arb, rgn_en_2, PMIF_PMIC_ALL_RGN_EN_2);
		rgn_en_2 = pmif_mpu_readl(mpu_arb, PMIF_PMIC_ALL_RGN_EN_2);
	}

	pmif_mpu_writel(mpu_arb, 1, PMIF_MPU_CTRL);

	dev_info(&mpu_pdev->dev, "PMIC_ALL_RGN_EN=0x%x, PMIC_ALL_RGN_EN_2=0x%x MPU late init setting done\n",
		 rgn_en, rgn_en_2);
}

static void enable_kernel_mpu_handler(struct timer_list *t)
{
	enable_kernel_mpu();
}

static int mtk_spmi_pmif_mpu_probe(struct platform_device *pdev)
{
	struct pmif_mpu *arb = NULL;
	int err = 0;
	u32 disable_pmif_mpu = 0, mpu_delay_enable_time = 0;

	arb = devm_kzalloc(&pdev->dev, sizeof(*arb), GFP_KERNEL);
	if (!arb)
		return -ENOMEM;

	arb->data = of_device_get_match_data(&pdev->dev);
	if (!arb->data) {
		dev_info(&pdev->dev, "cannot get drv_data\n");
		return -EINVAL;
	}

	arb->base = devm_platform_ioremap_resource_byname(pdev, "pmif_mpu");
	if (IS_ERR(arb->base)) {
		err = PTR_ERR(arb->base);
		dev_info(&pdev->dev, "failed to get remapped address\n");
		return PTR_ERR(arb->base);
	}

	platform_set_drvdata(pdev, arb);

	if (!of_property_read_u32(pdev->dev.of_node, "disable", &disable_pmif_mpu)) {
		if (disable_pmif_mpu) {
			pmif_mpu_writel(arb, 0, PMIF_PMIC_ALL_RGN_EN);
			dev_info(&pdev->dev, "Disable PMIF MPU\n");
			return 0;
		}
	}

	mpu_timer.mpu_arb = arb;
	mpu_timer.mpu_pdev = pdev;

	if (!of_property_read_u32(pdev->dev.of_node, "mediatek,kernel-enable-time",
				&mpu_delay_enable_time)) {
		/* enable kernel stage MPU region after insert mpu.ko for dts defined time sec*/
		dev_info(&pdev->dev, "MPU delay enable %usec after mpu.ko insert\n",
				mpu_delay_enable_time);

		timer_setup(&mpu_timer.mpu_enable_timer, enable_kernel_mpu_handler, 0);
		mod_timer(&mpu_timer.mpu_enable_timer,
			(jiffies + msecs_to_jiffies(mpu_delay_enable_time*1000)));
	}

	return 0;
}

static int mtk_spmi_pmif_mpu_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mtk_spmi_pmif_mpu_match_table[] = {
	{
		.compatible = "mediatek,mt6835-spmi-pmif-mpu",
		.data = &pmif_mpu_arb,
	}, {
		.compatible = "mediatek,mt6886-spmi_pmif_mpu",
		.data = &pmif_mpu_arb,
	}, {
		.compatible = "mediatek,mt6895-spmi_pmif_mpu",
		.data = &pmif_mpu_arb,
	}, {
		.compatible = "mediatek,mt6983-spmi_pmif_mpu",
		.data = &pmif_mpu_arb,
	}, {
		.compatible = "mediatek,mt6985-spmi_pmif_mpu",
		.data = &pmif_mpu_arb,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mtk_spmi_pmif_mpu_match_table);

static struct platform_driver mtk_spmi_pmif_mpu_driver = {
	.driver		= {
		.name	= "spmi-mtk-mpu",
		.of_match_table = of_match_ptr(mtk_spmi_pmif_mpu_match_table),
	},
	.probe		= mtk_spmi_pmif_mpu_probe,
	.remove		= mtk_spmi_pmif_mpu_remove,
};
module_platform_driver(mtk_spmi_pmif_mpu_driver);

MODULE_DESCRIPTION("MediaTek SPMI PMIF MPU Driver");
MODULE_LICENSE("GPL");
