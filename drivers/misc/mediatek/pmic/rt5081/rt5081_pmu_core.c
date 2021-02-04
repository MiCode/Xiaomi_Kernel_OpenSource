/*
 *  Copyright (C) 2016 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/reboot.h>
#include <linux/notifier.h>

#include "inc/rt5081_pmu.h"
#include "inc/rt5081_pmu_core.h"

struct rt5081_pmu_core_data {
	struct rt5081_pmu_chip *chip;
	struct device *dev;
	struct notifier_block nb;
};

static int rt5081_pmu_core_notifier_call(struct notifier_block *this,
	unsigned long code, void *unused)
{
	struct rt5081_pmu_core_data *core_data =
			container_of(this, struct rt5081_pmu_core_data, nb);
	int ret = 0;

	dev_dbg(core_data->dev, "%s: code %lu\n", __func__, code);
	switch (code) {
	case SYS_RESTART:
	case SYS_HALT:
	case SYS_POWER_OFF:
		ret = rt5081_pmu_reg_write(core_data->chip,
					   RT5081_PMU_REG_RSTPASCODE1, 0xA9);
		if (ret < 0)
			dev_dbg(core_data->dev, "set passcode1 fail\n");
		ret = rt5081_pmu_reg_write(core_data->chip,
					   RT5081_PMU_REG_RSTPASCODE2, 0x96);
		if (ret < 0)
			dev_dbg(core_data->dev, "set passcode2 fail\n");
		/* reset all chg/fled/ldo/rgb/bl/db reg and logic */
		ret = rt5081_pmu_reg_set_bit(core_data->chip,
					     RT5081_PMU_REG_CORECTRL2, 0x7E);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return (ret < 0 ? ret : NOTIFY_DONE);
}

static irqreturn_t rt5081_pmu_otp_irq_handler(int irq, void *data)
{
	struct rt5081_pmu_core_data *core_data = data;

	dev_dbg(core_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_vdda_ovp_irq_handler(int irq, void *data)
{
	struct rt5081_pmu_core_data *core_data = data;

	dev_dbg(core_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_vdda_uv_irq_handler(int irq, void *data)
{
	struct rt5081_pmu_core_data *core_data = data;

	dev_dbg(core_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static struct rt5081_pmu_irq_desc rt5081_core_irq_desc[] = {
	RT5081_PMU_IRQDESC(otp),
	RT5081_PMU_IRQDESC(vdda_ovp),
	RT5081_PMU_IRQDESC(vdda_uv),
};

static void rt5081_pmu_core_irq_register(struct platform_device *pdev)
{
	struct resource *res;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(rt5081_core_irq_desc); i++) {
		if (!rt5081_core_irq_desc[i].name)
			continue;
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						rt5081_core_irq_desc[i].name);
		if (!res)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, res->start, NULL,
					rt5081_core_irq_desc[i].irq_handler,
					IRQF_TRIGGER_FALLING,
					rt5081_core_irq_desc[i].name,
					platform_get_drvdata(pdev));
		if (ret < 0) {
			dev_dbg(&pdev->dev, "request %s irq fail\n", res->name);
			continue;
		}
		rt5081_core_irq_desc[i].irq = res->start;
	}
}

static inline int rt5081_pmu_core_init_register(
	struct rt5081_pmu_core_data *core_data)
{
	struct rt5081_pmu_core_platdata *pdata =
					dev_get_platdata(core_data->dev);
	uint8_t reg_data = 0, reg_mask = 0;
	int ret = 0;

	reg_mask |= RT5081_I2CRST_ENMASK;
	reg_data |= (pdata->i2cstmr_rst_en << RT5081_I2CRST_ENSHFT);
	reg_mask |= RT5081_I2CRST_TMRMASK;
	reg_data |= (pdata->i2cstmr_rst_tmr << RT5081_I2CRST_TMRSHFT);
	reg_mask |= RT5081_MRSTB_ENMASK;
	reg_data |= (pdata->mrstb_en << RT5081_MRSTB_ENSHFT);
	reg_mask |= RT5081_MRSTB_TMRMASK;
	reg_data |= (pdata->mrstb_tmr << RT5081_MRSTB_TMRSHFT);
	ret = rt5081_pmu_reg_update_bits(core_data->chip,
					 RT5081_PMU_REG_CORECTRL1, reg_mask,
					 reg_data);
	if (ret < 0)
		return ret;

	reg_data = reg_mask = 0;
	reg_mask |= RT5081_INTWDT_TMRMASK;
	reg_data |= (pdata->int_wdt << RT5081_INTWDT_TMRSHFT);
	reg_mask |= RT5081_INTDEG_TIMEMASK;
	reg_data |= (pdata->int_deg << RT5081_INTDEG_TIMESHFT);
	ret = rt5081_pmu_reg_update_bits(core_data->chip, RT5081_PMU_REG_IRQSET,
					 reg_mask, reg_data);
	if (ret < 0)
		return ret;
	return 0;
}

static inline int rt_parse_dt(struct device *dev)
{
	struct rt5081_pmu_core_platdata *pdata = dev_get_platdata(dev);
	struct device_node *np = dev->of_node;
	u32 tmp = 0;

	if (of_property_read_bool(np, "i2cstmr_rst_en"))
		pdata->i2cstmr_rst_en = 1;
	if (of_property_read_u32(np, "i2cstmr_rst_tmr", &tmp) < 0)
		pdata->i2cstmr_rst_tmr = 0;
	else
		pdata->i2cstmr_rst_tmr = tmp;
	if (of_property_read_bool(np, "mrstb_en"))
		pdata->mrstb_en = 1;
	if (of_property_read_u32(np, "mrstb_tmr", &tmp) < 0)
		pdata->mrstb_tmr = 0;
	else
		pdata->mrstb_tmr = tmp;
	if (of_property_read_u32(np, "int_wdt", &tmp) < 0)
		pdata->int_wdt = 0;
	else
		pdata->int_wdt = tmp;
	if (of_property_read_u32(np, "int_deg", &tmp) < 0)
		pdata->int_deg = 0;
	else
		pdata->int_deg = tmp;
	return 0;
}

static int rt5081_pmu_core_probe(struct platform_device *pdev)
{
	struct rt5081_pmu_core_platdata *pdata = dev_get_platdata(&pdev->dev);
	struct rt5081_pmu_core_data *core_data;
	bool use_dt = pdev->dev.of_node;
	int ret = 0;

	core_data = devm_kzalloc(&pdev->dev, sizeof(*core_data), GFP_KERNEL);
	if (!core_data)
		return -ENOMEM;
	if (use_dt) {
		/* DTS used */
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto out_pdata;
		}
		pdev->dev.platform_data = pdata;
		ret = rt_parse_dt(&pdev->dev);
		if (ret < 0) {
			devm_kfree(&pdev->dev, pdata);
			goto out_pdata;
		}
	} else {
		if (!pdata) {
			ret = -EINVAL;
			goto out_pdata;
		}
	}
	core_data->chip = dev_get_drvdata(pdev->dev.parent);
	core_data->dev = &pdev->dev;
	platform_set_drvdata(pdev, core_data);

	ret = rt5081_pmu_core_init_register(core_data);
	if (ret < 0)
		goto out_init_reg;

	/* register reboot/shutdown/halt related notifier */
	core_data->nb.notifier_call = rt5081_pmu_core_notifier_call;
	ret = register_reboot_notifier(&core_data->nb);
	if (ret < 0)
		goto out_notifier;

	rt5081_pmu_core_irq_register(pdev);
	dev_info(&pdev->dev, "%s successfully\n", __func__);
	return 0;
out_notifier:
out_init_reg:
out_pdata:
	devm_kfree(&pdev->dev, core_data);
	return ret;
}

static int rt5081_pmu_core_remove(struct platform_device *pdev)
{
	struct rt5081_pmu_core_data *core_data = platform_get_drvdata(pdev);

	unregister_reboot_notifier(&core_data->nb);
	dev_info(core_data->dev, "%s successfully\n", __func__);
	return 0;
}

static const struct of_device_id rt_ofid_table[] = {
	{ .compatible = "richtek,rt5081_pmu_core", },
	{ },
};
MODULE_DEVICE_TABLE(of, rt_ofid_table);

static const struct platform_device_id rt_id_table[] = {
	{ "rt5081_pmu_core", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, rt_id_table);

static struct platform_driver rt5081_pmu_core = {
	.driver = {
		.name = "rt5081_pmu_core",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt_ofid_table),
	},
	.probe = rt5081_pmu_core_probe,
	.remove = rt5081_pmu_core_remove,
	.id_table = rt_id_table,
};
module_platform_driver(rt5081_pmu_core);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("cy_huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("Richtek RT5081 PMU Core");
MODULE_VERSION("1.0.0_G");
