// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pm.h>

#include "../inc/mt6360_pmu.h"
#include "../inc/mt6360_pmu_core.h"

struct mt6360_pmu_core_info {
	struct device *dev;
	struct mt6360_pmu_info *mpi;
};

static const struct mt6360_core_platform_data def_platform_data = {
	.i2cstmr_rst_en = 0,
	.i2cstmr_rst_tmr = 0,
	.mren = 0,
	.mrstb_tmr = 4,
	.mrstb_rst_sel = 0,
	.apwdtrst_en = 0,
	.apwdtrst_sel = 0,
	.cc_open_sel = 0,
	.i2c_cc_open_tsel = 0,
	.pd_mden = 1,
	.ship_rst_dis = 0,
	.ot_shdn_sel = 1,
	.vddaov_shdn_sel = 0,
	.otp0_en = 1,
	.otp1_en = 1,
	.otp1_lpfoff_en = 0,
	.ldo5_otp_en = 1,
	.ldo5_otp_lpfoff_en = 0,
	.shipping_mode_pass_clock = 0,
	.sda_sizesel = 0,
	.sda_drvsrsel = 0,
	.fon_enbase = 0,
	.fon_osc = 0,
};

static irqreturn_t mt6360_pmu_ap_wdtrst_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_core_info *mpci = data;

	dev_warn(mpci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_en_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_core_info *mpci = data;

	dev_dbg(mpci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_qonb_rst_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_core_info *mpci = data;

	dev_warn(mpci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_mrstb_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_core_info *mpci = data;

	dev_warn(mpci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_otp_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_core_info *mpci = data;

	dev_dbg(mpci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_vddaov_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_core_info *mpci = data;

	dev_dbg(mpci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_sysuv_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_core_info *mpci = data;

	dev_dbg(mpci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static struct mt6360_pmu_irq_desc mt6360_pmu_core_irq_desc[] = {
	MT6360_PMU_IRQDESC(ap_wdtrst_evt),
	MT6360_PMU_IRQDESC(en_evt),
	MT6360_PMU_IRQDESC(qonb_rst_evt),
	MT6360_PMU_IRQDESC(mrstb_evt),
	MT6360_PMU_IRQDESC(otp_evt),
	MT6360_PMU_IRQDESC(vddaov_evt),
	MT6360_PMU_IRQDESC(sysuv_evt),
};

static void mt6360_pmu_core_irq_register(struct platform_device *pdev)
{
	struct mt6360_pmu_irq_desc *irq_desc;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_core_irq_desc); i++) {
		irq_desc = mt6360_pmu_core_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		ret = platform_get_irq_byname(pdev, irq_desc->name);
		if (ret < 0)
			continue;
		irq_desc->irq = ret;
		ret = devm_request_threaded_irq(&pdev->dev, irq_desc->irq, NULL,
						irq_desc->irq_handler,
						IRQF_TRIGGER_FALLING,
						irq_desc->name,
						platform_get_drvdata(pdev));
		if (ret < 0)
			dev_err(&pdev->dev,
				"request %s irq fail\n", irq_desc->name);
	}
}

static const struct mt6360_pdata_prop mt6360_pdata_props[] = {
	MT6360_PDATA_VALPROP(i2cstmr_rst_en, struct mt6360_core_platform_data,
			     MT6360_PMU_CORE_CTRL1, 7, 0x80, NULL, 0),
	MT6360_PDATA_VALPROP(i2cstmr_rst_tmr, struct mt6360_core_platform_data,
			     MT6360_PMU_CORE_CTRL1, 5, 0x60, NULL, 0),
	MT6360_PDATA_VALPROP(mren, struct mt6360_core_platform_data,
			     MT6360_PMU_CORE_CTRL1, 4, 0x10, NULL, 0),
	MT6360_PDATA_VALPROP(mrstb_tmr, struct mt6360_core_platform_data,
			     MT6360_PMU_CORE_CTRL1, 1, 0x0e, NULL, 0),
	MT6360_PDATA_VALPROP(mrstb_rst_sel, struct mt6360_core_platform_data,
			     MT6360_PMU_CORE_CTRL1, 0, 0x01, NULL, 0),
	MT6360_PDATA_VALPROP(apwdtrst_en, struct mt6360_core_platform_data,
			     MT6360_PMU_CORE_CTRL2, 7, 0x80, NULL, 0),
	MT6360_PDATA_VALPROP(apwdtrst_sel, struct mt6360_core_platform_data,
			     MT6360_PMU_CORE_CTRL2, 6, 0x40, NULL, 0),
	MT6360_PDATA_VALPROP(cc_open_sel, struct mt6360_core_platform_data,
			     MT6360_PMU_CORE_CTRL2, 4, 0x30, NULL, 0),
	MT6360_PDATA_VALPROP(i2c_cc_open_tsel, struct mt6360_core_platform_data,
			     MT6360_PMU_CORE_CTRL2, 2, 0x0c, NULL, 0),
	MT6360_PDATA_VALPROP(pd_mden, struct mt6360_core_platform_data,
			     MT6360_PMU_CORE_CTRL2, 1, 0x02, NULL, 0),
	MT6360_PDATA_VALPROP(ship_rst_dis, struct mt6360_core_platform_data,
			     MT6360_PMU_CORE_CTRL2, 0, 0x01, NULL, 0),
	MT6360_PDATA_VALPROP(ot_shdn_sel, struct mt6360_core_platform_data,
			     MT6360_PMU_SHDN_CTRL, 7, 0x80, NULL, 0),
	MT6360_PDATA_VALPROP(vddaov_shdn_sel, struct mt6360_core_platform_data,
			     MT6360_PMU_SHDN_CTRL, 6, 0x40, NULL, 0),
	MT6360_PDATA_VALPROP(otp0_en, struct mt6360_core_platform_data,
			     MT6360_PMU_SHDN_CTRL, 5, 0x20, NULL, 0),
	MT6360_PDATA_VALPROP(otp1_en, struct mt6360_core_platform_data,
			     MT6360_PMU_SHDN_CTRL, 4, 0x10, NULL, 0),
	MT6360_PDATA_VALPROP(otp1_lpfoff_en, struct mt6360_core_platform_data,
			     MT6360_PMU_SHDN_CTRL, 3, 0x08, NULL, 0),
	MT6360_PDATA_VALPROP(ldo5_otp_en, struct mt6360_core_platform_data,
			     MT6360_PMU_SHDN_CTRL, 2, 0x04, NULL, 0),
	MT6360_PDATA_VALPROP(ldo5_otp_lpfoff_en,
			     struct mt6360_core_platform_data,
			     MT6360_PMU_SHDN_CTRL, 1, 0x02, NULL, 0),
	MT6360_PDATA_VALPROP(shipping_mode_pass_clock,
			     struct mt6360_core_platform_data,
			     MT6360_PMU_I2C_CTRL, 7, 0x80, NULL, 0),
	MT6360_PDATA_VALPROP(sda_sizesel, struct mt6360_core_platform_data,
			     MT6360_PMU_I2C_CTRL, 5, 0x60, NULL, 0),
	MT6360_PDATA_VALPROP(sda_drvsrsel, struct mt6360_core_platform_data,
			     MT6360_PMU_I2C_CTRL, 4, 0x10, NULL, 0),
	MT6360_PDATA_VALPROP(fon_enbase, struct mt6360_core_platform_data,
			     MT6360_PMU_I2C_CTRL, 1, 0x02, NULL, 0),
	MT6360_PDATA_VALPROP(fon_osc, struct mt6360_core_platform_data,
			     MT6360_PMU_I2C_CTRL, 0, 0x01, NULL, 0),
};

static int mt6360_core_apply_pdata(struct mt6360_pmu_core_info *mpci,
				   struct mt6360_core_platform_data *pdata)
{
	int ret;

	dev_dbg(mpci->dev, "%s ++\n", __func__);
	ret = mt6360_pdata_apply_helper(mpci->mpi, pdata, mt6360_pdata_props,
					ARRAY_SIZE(mt6360_pdata_props));
	if (ret < 0)
		return ret;
	dev_dbg(mpci->dev, "%s --\n", __func__);
	return 0;
}

static const struct mt6360_val_prop mt6360_val_props[] = {
	MT6360_DT_VALPROP(i2cstmr_rst_en, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(i2cstmr_rst_tmr, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(mren, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(mrstb_tmr, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(mrstb_rst_sel, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(apwdtrst_en, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(cc_open_sel, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(i2c_cc_open_tsel, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(pd_mden, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(ship_rst_dis, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(ot_shdn_sel, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(vddaov_shdn_sel, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(ldo5_otp_en, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(shipping_mode_pass_clock,
					      struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(sda_sizesel, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(sda_drvsrsel, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(fon_enbase, struct mt6360_core_platform_data),
	MT6360_DT_VALPROP(fon_osc, struct mt6360_core_platform_data),
};

static int mt6360_core_parse_dt_data(struct device *dev,
				     struct mt6360_core_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	dev_dbg(dev, "%s ++\n", __func__);
	memcpy(pdata, &def_platform_data, sizeof(*pdata));
	mt6360_dt_parser_helper(np, (void *)pdata,
				mt6360_val_props, ARRAY_SIZE(mt6360_val_props));
	dev_dbg(dev, "%s --\n", __func__);
	return 0;
}

static int mt6360_pmu_core_probe(struct platform_device *pdev)
{
	struct mt6360_core_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct mt6360_pmu_core_info *mpci;
	bool use_dt = pdev->dev.of_node;
	int ret;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (use_dt) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = mt6360_core_parse_dt_data(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "parse dt fail\n");
			return ret;
		}
		pdev->dev.platform_data = pdata;
	}
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data specified\n");
		return -EINVAL;
	}
	mpci = devm_kzalloc(&pdev->dev, sizeof(*mpci), GFP_KERNEL);
	if (!mpci)
		return -ENOMEM;
	mpci->dev = &pdev->dev;
	mpci->mpi = dev_get_drvdata(pdev->dev.parent);
	platform_set_drvdata(pdev, mpci);

	/* apply platform data */
	ret = mt6360_core_apply_pdata(mpci, pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "apply pdata fail\n");
		return ret;
	}
	/* irq register */
	mt6360_pmu_core_irq_register(pdev);
	dev_info(&pdev->dev, "%s: successfully probed\n", __func__);
	return 0;
}

static int mt6360_pmu_core_remove(struct platform_device *pdev)
{
	struct mt6360_pmu_core_info *mpci = platform_get_drvdata(pdev);

	dev_dbg(mpci->dev, "%s\n", __func__);
	return 0;
}

static int __maybe_unused mt6360_pmu_core_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused mt6360_pmu_core_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6360_pmu_core_pm_ops,
			 mt6360_pmu_core_suspend, mt6360_pmu_core_resume);

static const struct of_device_id __maybe_unused mt6360_pmu_core_of_id[] = {
	{ .compatible = "mediatek,mt6360_pmu_core", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_pmu_core_of_id);

static const struct platform_device_id mt6360_pmu_core_id[] = {
	{ "mt6360_pmu_core", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_pmu_core_id);

static struct platform_driver mt6360_pmu_core_driver = {
	.driver = {
		.name = "mt6360_pmu_core",
		.owner = THIS_MODULE,
		.pm = &mt6360_pmu_core_pm_ops,
		.of_match_table = of_match_ptr(mt6360_pmu_core_of_id),
	},
	.probe = mt6360_pmu_core_probe,
	.remove = mt6360_pmu_core_remove,
	.id_table = mt6360_pmu_core_id,
};
module_platform_driver(mt6360_pmu_core_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 PMU CORE Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
