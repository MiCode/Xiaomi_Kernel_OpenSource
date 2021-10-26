/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <mt-plat/upmu_common.h>

#include "include/pmic.h"

#include "mtk_charger_intf.h"

struct mt6357_charger_desc {
	u32 ichg;		/* uA */
	u32 cv;			/* uV */
	u32 vcdt_hv_thres;	/* uV */
	u32 vbat_ov_thres;	/* uV */
};

static struct mt6357_charger_desc mt6357_chg_default_desc = {
	.ichg = 500000,
	.cv = 4350000,
	.vcdt_hv_thres = 7000000,
	.vbat_ov_thres = 4450000,
};

struct mt6357_charger {
	const char *charger_dev_name;
	struct charger_properties charger_prop;
	struct charger_device *charger_dev;
	struct mt6357_charger_desc *desc;
};

static const u32 CS_VTH[] = {
	2000000, 1600000, 1500000, 1350000,
	1200000, 1100000, 1000000, 900000,
	800000, 700000, 650000, 550000,
	450000, 300000, 200000, 70000,
};

static const u32 VBAT_CV_VTH[] = {
	3900000, 4000000, 4050000, 4100000,
	4150000, 4200000, 4212500, 4225000,
	4237500, 4250000, 4262500, 4275000,
	4287500, 4300000, 4312500, 4325000,
	4337500, 4350000, 4362500, 4375000,
	4387500, 4400000, 4412500, 4425000,
	4437500, 4450000, 4462500, 4475000,
	4487500, 4500000, 4600000,
};

static const u32 VCDT_HV_VTH[] = {
	3000000, 3900000, 4000000, 4100000,
	4150000, 4200000, 4250000, 4300000,
	4350000, 4400000, 4450000, 6000000,
	6500000, 7000000, 7500000, 8500000,
	9500000, 10500000, 11500000, 12500000,
	14000000,
};

static const u32 VBAT_OV_VTH[] = {
	4200000, 4200000, 4300000, 4400000,
	4450000, 4500000, 4600000, 4700000,
};

static u32 charging_value_to_parameter(const u32 *parameter,
					const u32 array_size,
					const u32 val)
{
	if (val < array_size)
		return parameter[val];

	chr_debug("Can't find the parameter\n");
	return parameter[0];

}

static u32 charging_parameter_to_value(const u32 *parameter,
					const u32 array_size,
					const u32 val)
{
	u32 i;

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	pr_info("no register value matched\n");
	return 0;
}

static u32 bmt_find_closest_level(const u32 *pList, u32 number, u32 level)
{
	int i;
	u32 max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = true;
	else
		max_value_in_last_element = false;

	if (max_value_in_last_element == true) {
		/* max value in the last element */
		for (i = (number - 1); i >= 0; i--) {
			if (pList[i] <= level) {
/* pr_debug("zzf_%d<=%d i=%d\n", pList[i], level, i); */
				return pList[i];
			}
		}

		chr_debug("Can't find closest level\n");
		return pList[0];
	}

	/* max value in the first element */
	for (i = 0; i < number; i++) {
		if (pList[i] <= level)
			return pList[i];
	}

	chr_debug("Can't find closest level\n");
	return pList[number - 1];
}

static int mt6357_charger_parse_dt(struct mt6357_charger *info,
	struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct mt6357_charger_desc *desc = NULL;

	pr_info("%s: starts\n", __func__);

	if (!np) {
		chr_err("%s: no device node\n", __func__);
		return -EINVAL;
	}

	info->desc = &mt6357_chg_default_desc;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	memcpy(desc, &mt6357_chg_default_desc,
		sizeof(struct mt6357_charger_desc));

	if (of_property_read_string(np, "charger_name",
		&info->charger_dev_name) < 0) {
		chr_err("%s: no charger name\n", __func__);
		info->charger_dev_name = "primary_chg";
	}

	if (of_property_read_string(np, "alias_name",
		&info->charger_prop.alias_name) < 0) {
		chr_err("%s: no alias name\n", __func__);
		info->charger_prop.alias_name = "mt6357";
	}

	if (of_property_read_u32(np, "ichg", &desc->ichg) < 0)
		chr_err("%s: no ichg\n", __func__);

	if (of_property_read_u32(np, "cv", &desc->cv) < 0)
		chr_err("%s: no cv\n", __func__);

	if (of_property_read_u32(np, "vcdt_hv_thres", &desc->vcdt_hv_thres) < 0)
		chr_err("%s: no vcdt_hv_thres\n", __func__);

	if (of_property_read_u32(np, "vbat_ov_thres", &desc->vbat_ov_thres) < 0)
		chr_err("%s: no vbat_ov_thres\n", __func__);

	info->desc = desc;

	pr_info("chr name:%s alias:%s\n",
		info->charger_dev_name, info->charger_prop.alias_name);

	return 0;
}

static int mt6357_plug_in(struct charger_device *chg_dev)
{
	int ret = 0;

	pmic_set_register_value(PMIC_RG_ULC_DET_EN, 1);

	return ret;
}

static int mt6357_enable_charging(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	/* struct mt6357_charger *info = dev_get_drvdata(&chg_dev->dev); */

	chr_debug("[%s] en: %d\n", __func__, en);

	if (en) {
#if 0
		pmic_set_register_value(PMIC_RG_CSDAC_DLY, 4);
		pmic_set_register_value(PMIC_RG_CSDAC_STP, 1);
		pmic_set_register_value(PMIC_RG_CSDAC_STP_INC, 1);
		pmic_set_register_value(PMIC_RG_CSDAC_STP_DEC, 2);
#endif
		pmic_set_register_value(PMIC_RG_CSDAC_MODE, 1);
		pmic_set_register_value(PMIC_RG_CS_EN, 1);

		pmic_set_register_value(PMIC_RG_ULC_DET_EN, 1);
		pmic_set_register_value(PMIC_RG_HWCV_EN, 1);

		pmic_set_register_value(PMIC_RG_VBAT_CV_EN, 1);
		/* enable debug flag output */
		pmic_set_register_value(PMIC_RG_PCHR_FLAG_EN, 1);

		pmic_set_register_value(PMIC_RG_CSDAC_EN, 1);

		pmic_set_register_value(PMIC_RG_CHR_EN, 1);

		/* pmic_enable_interrupt(INT_WATCHDOG, 1, "PMIC"); */
	} else {
		/* pmic_set_register_value(PMIC_RG_INT_EN_WATCHDOG, 0);
		 * TODO: remove it
		 */
		/* pmic_enable_interrupt(INT_WATCHDOG, 0, "PMIC"); */
		pmic_set_register_value(PMIC_RG_CHRWDT_EN, 0);
#if 0
		pmic_set_register_value(PMIC_RG_CHRWDT_WR, 0); /* TODO */
		pmic_set_register_value(PMIC_RG_CSDAC_EN, 0);
		pmic_set_register_value(PMIC_RG_HWCV_EN, 0);
#endif
		pmic_set_register_value(PMIC_RG_CHR_EN, 0);
	}

	return ret;
}

static int mt6357_is_charging_enabled(struct charger_device *chg_dev, bool *en)
{
	unsigned short val;

	val = pmic_get_register_value(PMIC_RG_CHR_EN);
	*en = (val == 1) ? true : false;

	return 0;
}

static int mt6357_get_ichg(struct charger_device *chg_dev, u32 *ichg)
{
	int ret = 0;
	u32 array_size;
	u32 val;

	array_size = ARRAY_SIZE(CS_VTH);
	val = pmic_get_register_value(PMIC_RG_CS_VTH);
	*ichg = charging_value_to_parameter(CS_VTH, array_size, val);

	return ret;
}

static int mt6357_set_ichg(struct charger_device *chg_dev, u32 ichg)
{
	int ret = 0;
	u32 array_size;
	u32 set_ichg, register_value;

	array_size = ARRAY_SIZE(CS_VTH);
	set_ichg = bmt_find_closest_level(CS_VTH, array_size, ichg);
	register_value = charging_parameter_to_value(CS_VTH,
						array_size, set_ichg);
	ret = pmic_set_register_value(PMIC_RG_CS_VTH, register_value);
	chr_debug("%s: 0x%x %d %d\n", __func__, register_value, ichg, set_ichg);

	return ret;
}

static int mt6357_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
	u32 array_size;

	array_size = ARRAY_SIZE(CS_VTH);
	*uA = charging_value_to_parameter(CS_VTH, array_size, array_size - 1);

	return 0;
}

static int mt6357_get_cv(struct charger_device *chg_dev, u32 *cv)
{
	int ret = 0;
	u32 array_size;
	u32 val;

	array_size = ARRAY_SIZE(VBAT_CV_VTH);
	val = pmic_get_register_value(PMIC_RG_VBAT_CV_VTH);
	*cv = charging_value_to_parameter(VBAT_CV_VTH, array_size, val);

	return ret;
}

static int mt6357_set_cv(struct charger_device *chg_dev, u32 cv)
{
	int ret = 0;
	u32 array_size;
	u32 set_cv, register_value;

	array_size = ARRAY_SIZE(VBAT_CV_VTH);
	set_cv = bmt_find_closest_level(VBAT_CV_VTH, array_size, cv);
	register_value = charging_parameter_to_value(VBAT_CV_VTH,
						array_size, set_cv);
	pmic_set_register_value(PMIC_RG_VBAT_CV_VTH, register_value);
	chr_debug("%s: cv = %d mV (0x%x)\n", __func__, set_cv, register_value);

	return ret;
}

static int mt6357_kick_wdt(struct charger_device *chg_dev)
{
	int ret = 0;

	chr_debug("%s\n", __func__);
#if 0
	pmic_set_register_value(PMIC_RG_CHRWDT_TD, 3);   /* 32s */
	/* TODO: register interrupt callback!? */
	pmic_set_register_value(PMIC_RG_INT_EN_WATCHDOG, 1);
	pmic_set_register_value(PMIC_RG_CHRWDT_EN, 1);
#endif
	pmic_set_register_value(PMIC_RG_CHRWDT_WR, 1);

	return ret;
}

static int mt6357_dump_register(struct charger_device *chg_dev)
{
	int ret = 0;
	u32 i = 0;
	u32 ichg = 0, cv = 0;
	bool chg_en = false;

	ret = mt6357_get_ichg(chg_dev, &ichg);
	ret = mt6357_get_cv(chg_dev, &cv);
	ret = mt6357_is_charging_enabled(chg_dev, &chg_en);

	for (i = MT6357_CHR_TOP_CON0; i <= MT6357_PCHR_ELR1; i += 2)
		chr_debug("[0x%x]=0x%x\t", i, upmu_get_reg_value(i));
	chr_debug("\n");

	for (i = MT6357_CHR_CON0; i <= MT6357_CHR_CON9; i += 2)
		chr_debug("[0x%x]=0x%x\t", i, upmu_get_reg_value(i));
	chr_debug("\n");

	pr_info("ICHG = %dmA, CV = %dmV, CHG_EN = %d\n",
		ichg / 1000, cv / 1000, chg_en);

	return ret;
}

static int mt6357_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}
	return 0;
}

static struct charger_ops mt6357_charger_ops = {
	/* normal charging */
	.plug_in = mt6357_plug_in,
	/* .plug_out = mt6357_plug_out, */
	.enable = mt6357_enable_charging,
	.is_enabled = mt6357_is_charging_enabled,
	.get_charging_current = mt6357_get_ichg,
	.set_charging_current = mt6357_set_ichg,
	.get_min_charging_current = mt6357_get_min_ichg,
	.get_constant_voltage = mt6357_get_cv,
	.set_constant_voltage = mt6357_set_cv,
	.kick_wdt = mt6357_kick_wdt,
	.dump_registers = mt6357_dump_register,

	/* Event */
	.event = mt6357_do_event,
};

static void watchdog_int_handler(void)
{
	pr_notice("mt6357 CHRWDT IRQ\n");
}

static int mt6357_charger_init_setting(struct mt6357_charger *info)
{
	int ret = 0;
	unsigned short val;

	val = pmic_get_register_value(PMIC_RG_VBAT_CV_VTH);
	pr_info("[%s] VBAT_CV_VTH: 0x%x\n", __func__, val);

#if 0 /* TODO */
	ret = mt6357_set_ichg(info);
#endif

	pmic_set_register_value(PMIC_RG_CHRWDT_TD, 3);  /* 32s */
	pmic_set_register_value(PMIC_RG_CHRWDT_EN, 1);
	pmic_set_register_value(PMIC_RG_CHRWDT_WR, 1);

#if 1 /* TODO: Default value */
	pmic_set_register_value(PMIC_RG_VCDT_MODE, 0);
	pmic_set_register_value(PMIC_RG_VCDT_HV_EN, 1);
 /* force leave USBDL mode  */
	pmic_set_register_value(PMIC_RG_USBDL_SET, 0);
	pmic_set_register_value(PMIC_RG_USBDL_RST, 1);

	pmic_set_register_value(PMIC_RG_BC11_BB_CTRL, 1);
	pmic_set_register_value(PMIC_RG_BC11_RST, 1);

	pmic_set_register_value(PMIC_RG_VBAT_OV_EN, 1);
#endif
	pmic_set_register_value(PMIC_RG_CSDAC_MODE, 1); /* CC mode */

	/* TODO */
	pmic_set_register_value(PMIC_RG_VBAT_OV_VTH, 4); /* 4450mV */
	pmic_set_register_value(PMIC_RG_VCDT_HV_VTH, 0xd); /* 7000mV */

	pmic_set_register_value(PMIC_RG_ULC_DET_EN, 1);
	/* pmic_set_register_value(PMIC_RG_LOW_ICH_DB, 1); 16ms */
	/* TODO */

	pmic_register_interrupt_callback(INT_WATCHDOG, watchdog_int_handler);
	pmic_enable_interrupt(INT_WATCHDOG, 1, "PMIC");

	return ret;
}

static int mt6357_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mt6357_charger *info = NULL;

	pr_info("%s: starts\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mt6357_charger_parse_dt(info, &pdev->dev);
	platform_set_drvdata(pdev, info);

	/* Register charger device */
	info->charger_dev = charger_device_register(info->charger_dev_name,
		&pdev->dev, info, &mt6357_charger_ops, &info->charger_prop);
	if (IS_ERR_OR_NULL(info->charger_dev)) {
		ret = PTR_ERR(info->charger_dev);
		goto err_register_charger_dev;
	}

	info->charger_dev->is_polling_mode = true;

	dev_set_drvdata(&info->charger_dev->dev, info);

	mt6357_charger_init_setting(info);

err_register_charger_dev:
	devm_kfree(&pdev->dev, info);
	return ret;
}

static int mt6357_charger_remove(struct platform_device *pdev)
{
	struct mt6357_charger *info = platform_get_drvdata(pdev);

	if (info)
		devm_kfree(&pdev->dev, info);

	return 0;
}

static const struct of_device_id mt6357_charger_of_match[] = {
	{.compatible = "mediatek,mt6357_charger",},
	{},
};

MODULE_DEVICE_TABLE(of, mt6357_charger_of_match);

static struct platform_driver mt6357_charger_driver = {
	.probe = mt6357_charger_probe,
	.remove = mt6357_charger_remove,
	.driver = {
		   .name = "mt6357_charger",
		   .of_match_table = mt6357_charger_of_match,
		   },
};

static int __init mt6357_charger_init(void)
{
	return platform_driver_register(&mt6357_charger_driver);
}
module_init(mt6357_charger_init);

static void __exit mt6357_charger_exit(void)
{
	platform_driver_unregister(&mt6357_charger_driver);
}
module_exit(mt6357_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mediatek");
