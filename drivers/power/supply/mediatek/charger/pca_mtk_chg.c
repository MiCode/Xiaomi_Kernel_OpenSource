/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mt-plat/charger_class.h>
#include <mt-plat/mtk_charger.h>
#include <mt-plat/mtk_battery.h>
#include <mt-plat/prop_chgalgo_class.h>

#define PCA_MTK_CHG_VERSION	"2.0.0_MTK"

enum mtk_chg_type {
	MTK_CHGTYP_SWCHG = 0,
	MTK_CHGTYP_LOADSW,
	MTK_CHGTYP_DVCHG,
	MTK_CHGTYP_DVCHG_SLAVE,
	MTK_CHGTYP_HV_DVCHG,
	MTK_CHGTYP_MAX,
};

static const char *mtk_chgtyp_name[MTK_CHGTYP_MAX] = {
	"primary_chg",
	"primary_load_switch",
	"primary_divider_chg",
	"secondary_divider_chg",
	"primary_hv_divider_chg",
};

struct pca_mtk_chg_info {
	struct device *dev;
	struct prop_chgalgo_device *pca[MTK_CHGTYP_MAX];
	struct charger_device *chgdev[MTK_CHGTYP_MAX];
	struct charger_consumer *chg_consumer;
};

static inline u32 milli_to_micro(u32 val)
{
	return val * 1000;
}

static inline u32 micro_to_milli(u32 val)
{
	return val / 1000;
}

static const struct prop_chgalgo_desc *pca_mtk_chg_desc_tbl[MTK_CHGTYP_MAX];
static inline int get_mtk_chgtyp(struct prop_chgalgo_device *pca)
{
	int i;

	for (i = 0; i < MTK_CHGTYP_MAX; i++) {
		if (strcmp(pca->desc->name, pca_mtk_chg_desc_tbl[i]->name) == 0)
			return i;
	}
	return -EINVAL;
}

static int pca_mtk_chg_enable_power_path(struct prop_chgalgo_device *pca,
					 bool en)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp != MTK_CHGTYP_SWCHG)
		return -EINVAL;
	return charger_manager_enable_power_path(info->chg_consumer,
						 MAIN_CHARGER, en);
}

static int pca_mtk_chg_enable_charging(struct prop_chgalgo_device *pca, bool en)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_enable(info->chgdev[chgtyp], en);
}

static int pca_mtk_chg_enable_chip(struct prop_chgalgo_device *pca, bool en)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_enable_chip(info->chgdev[chgtyp], en);
}

static int pca_mtk_chg_set_vbusovp(struct prop_chgalgo_device *pca, u32 mV)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_set_vbusovp(info->chgdev[chgtyp],
				       milli_to_micro(mV));
}

static int pca_mtk_chg_set_ibusocp(struct prop_chgalgo_device *pca, u32 mA)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_set_ibusocp(info->chgdev[chgtyp],
				       milli_to_micro(mA));
}

static int pca_mtk_chg_set_vbatovp(struct prop_chgalgo_device *pca, u32 mV)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_set_vbatovp(info->chgdev[chgtyp],
				       milli_to_micro(mV));
}

static int pca_mtk_chg_set_vbatovp_alarm(struct prop_chgalgo_device *pca,
					 u32 mV)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_set_vbatovp_alarm(info->chgdev[chgtyp],
					     milli_to_micro(mV));
}

static int pca_mtk_chg_reset_vbatovp_alarm(struct prop_chgalgo_device *pca)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_reset_vbatovp_alarm(info->chgdev[chgtyp]);
}

static int pca_mtk_chg_set_vbusovp_alarm(struct prop_chgalgo_device *pca,
					 u32 mV)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_set_vbusovp_alarm(info->chgdev[chgtyp],
					     milli_to_micro(mV));
}

static int pca_mtk_chg_reset_vbusovp_alarm(struct prop_chgalgo_device *pca)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_reset_vbusovp_alarm(info->chgdev[chgtyp]);
}

static int pca_mtk_chg_set_ibatocp(struct prop_chgalgo_device *pca, u32 mA)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_set_ibatocp(info->chgdev[chgtyp],
				       milli_to_micro(mA));
}

static int pca_mtk_chg_enable_hz(struct prop_chgalgo_device *pca, bool en)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_enable_hz(info->chgdev[chgtyp], en);
}

static inline int to_chgclass_adc(enum prop_chgalgo_adc_channel chan)
{
	switch (chan) {
	case PCA_ADCCHAN_VBUS:
		return ADC_CHANNEL_VBUS;
	case PCA_ADCCHAN_IBUS:
		return ADC_CHANNEL_IBUS;
	case PCA_ADCCHAN_VBAT:
		return ADC_CHANNEL_VBAT;
	case PCA_ADCCHAN_IBAT:
		return ADC_CHANNEL_IBAT;
	case PCA_ADCCHAN_TBAT:
		return ADC_CHANNEL_TBAT;
	case PCA_ADCCHAN_TCHG:
		return ADC_CHANNEL_TEMP_JC;
	case PCA_ADCCHAN_VOUT:
		return ADC_CHANNEL_VOUT;
	case PCA_ADCCHAN_VSYS:
		return ADC_CHANNEL_VSYS;
	default:
		break;
	}
	return -ENOTSUPP;
}

static int pca_mtk_chg_get_adc(struct prop_chgalgo_device *pca,
			       enum prop_chgalgo_adc_channel chan, int *min,
			       int *max)
{
	int ret;
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);
	int _chan = to_chgclass_adc(chan);
	bool hv_dvchg = false;

	if (chgtyp < 0 || _chan < 0)
		return -EINVAL;

	if (_chan == ADC_CHANNEL_TBAT) {
		*max = *min = battery_get_bat_temperature();
		return 0;
	}
	ret = charger_dev_get_adc(info->chgdev[chgtyp], _chan, min, max);
	if (ret == -ENOTSUPP) {
		/* Temporary solution */
		if (chgtyp == MTK_CHGTYP_HV_DVCHG) {
			hv_dvchg = true;
			ret = charger_dev_get_adc(
				info->chgdev[MTK_CHGTYP_DVCHG], _chan, min,
				max);
		}
	}
	if (ret < 0)
		return ret;
	if (_chan == ADC_CHANNEL_VBAT || _chan == ADC_CHANNEL_IBAT ||
	    _chan == ADC_CHANNEL_VBUS || _chan == ADC_CHANNEL_IBUS ||
	    _chan == ADC_CHANNEL_VOUT || _chan == ADC_CHANNEL_VSYS) {
		*max = micro_to_milli(*max);
		if (min != max)
			*min = micro_to_milli(*min);
	}
	if (hv_dvchg) {
		if (_chan == ADC_CHANNEL_VBUS) {
			*max *= 2;
			if (min != max)
				*min *= 2;
		} else if (_chan == ADC_CHANNEL_IBUS) {
			*max /= 2;
			if (min != max)
				*min /= 2;
		}
	}
	return 0;
}

static int pca_mtk_chg_get_soc(struct prop_chgalgo_device *pca, u32 *soc)
{
	int _soc;

	_soc = battery_get_soc();
	*soc = _soc < 0 ? 0 : _soc;
	return 0;
}

static int pca_mtk_chg_set_aicr(struct prop_chgalgo_device *pca, u32 mA)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_set_input_current(info->chgdev[chgtyp],
					     milli_to_micro(mA));
}

static int pca_mtk_chg_set_ichg(struct prop_chgalgo_device *pca, u32 mA)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_set_charging_current(info->chgdev[chgtyp],
						milli_to_micro(mA));
}

static int pca_mtk_chg_is_vbuslowerr(struct prop_chgalgo_device *pca, bool *err)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_is_vbuslowerr(info->chgdev[chgtyp], err);
}

static int pca_mtk_chg_is_charging_enabled(struct prop_chgalgo_device *pca,
					   bool *en)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_is_enabled(info->chgdev[chgtyp], en);
}

static int pca_mtk_chg_get_adc_accuracy(struct prop_chgalgo_device *pca,
					enum prop_chgalgo_adc_channel chan,
					int *min, int *max)
{
	int ret;
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);
	int _chan = to_chgclass_adc(chan);

	if (chgtyp < 0 || _chan < 0)
		return -EINVAL;
	ret = charger_dev_get_adc_accuracy(info->chgdev[chgtyp], _chan, min,
					   max);
	if (ret < 0)
		return ret;
	if (_chan == ADC_CHANNEL_VBAT || _chan == ADC_CHANNEL_IBAT ||
	    _chan == ADC_CHANNEL_VBUS || _chan == ADC_CHANNEL_IBUS ||
	    _chan == ADC_CHANNEL_VOUT || _chan == ADC_CHANNEL_VSYS) {
		*max = micro_to_milli(*max);
		if (min != max)
			*min = micro_to_milli(*min);
	}
	return 0;
}

static int pca_mtk_chg_init_chip(struct prop_chgalgo_device *pca)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_init_chip(info->chgdev[chgtyp]);
}

static int pca_mtk_chg_dump_registers(struct prop_chgalgo_device *pca)
{
	int ret;
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;

	ret = charger_dev_dump_registers(info->chgdev[chgtyp]);

	return ret;
}

static int pca_mtk_chg_enable_auto_trans(struct prop_chgalgo_device *pca,
					 bool en)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_enable_auto_trans(info->chgdev[chgtyp], en);
}

static int pca_mtk_chg_set_auto_trans(struct prop_chgalgo_device *pca, u32 mV,
				      bool en)
{
	struct pca_mtk_chg_info *info = prop_chgalgo_get_drvdata(pca);
	int chgtyp = get_mtk_chgtyp(pca);

	if (chgtyp < 0)
		return -EINVAL;
	return charger_dev_set_auto_trans(info->chgdev[chgtyp],
					  milli_to_micro(mV), en);
}

static struct prop_chgalgo_chg_ops pca_chg_ops = {
	.enable_power_path = pca_mtk_chg_enable_power_path,
	.enable_charging = pca_mtk_chg_enable_charging,
	.enable_chip = pca_mtk_chg_enable_chip,
	.enable_hz = pca_mtk_chg_enable_hz,
	.set_vbusovp = pca_mtk_chg_set_vbusovp,
	.set_ibusocp = pca_mtk_chg_set_ibusocp,
	.set_vbatovp = pca_mtk_chg_set_vbatovp,
	.set_vbatovp_alarm = pca_mtk_chg_set_vbatovp_alarm,
	.reset_vbatovp_alarm = pca_mtk_chg_reset_vbatovp_alarm,
	.set_vbusovp_alarm = pca_mtk_chg_set_vbusovp_alarm,
	.reset_vbusovp_alarm = pca_mtk_chg_reset_vbusovp_alarm,
	.set_ibatocp = pca_mtk_chg_set_ibatocp,
	.get_adc = pca_mtk_chg_get_adc,
	.get_soc = pca_mtk_chg_get_soc,
	.set_aicr = pca_mtk_chg_set_aicr,
	.set_ichg = pca_mtk_chg_set_ichg,
	.is_vbuslowerr = pca_mtk_chg_is_vbuslowerr,
	.is_charging_enabled = pca_mtk_chg_is_charging_enabled,
	.get_adc_accuracy = pca_mtk_chg_get_adc_accuracy,
	.init_chip = pca_mtk_chg_init_chip,
	.enable_auto_trans = pca_mtk_chg_enable_auto_trans,
	.set_auto_trans = pca_mtk_chg_set_auto_trans,
	.dump_registers = pca_mtk_chg_dump_registers,
};
static SIMPLE_PCA_CHG_DESC(pca_chg_swchg, pca_chg_ops);
static SIMPLE_PCA_CHG_DESC(pca_chg_loadsw, pca_chg_ops);
static SIMPLE_PCA_CHG_DESC(pca_chg_dvchg, pca_chg_ops);
static SIMPLE_PCA_CHG_DESC(pca_chg_dvchg_slave, pca_chg_ops);
static SIMPLE_PCA_CHG_DESC(pca_chg_hv_dvchg, pca_chg_ops);

static const struct prop_chgalgo_desc *pca_mtk_chg_desc_tbl[MTK_CHGTYP_MAX] = {
	&pca_chg_swchg_desc,
	&pca_chg_loadsw_desc,
	&pca_chg_dvchg_desc,
	&pca_chg_dvchg_slave_desc,
	&pca_chg_hv_dvchg_desc,
};

static int pca_mtk_chg_register(struct pca_mtk_chg_info *info)
{
	int i;

	dev_info(info->dev, "%s\n", __func__);

	for (i = 0; i < MTK_CHGTYP_MAX; i++) {
		if (!info->chgdev[i])
			continue;
		info->pca[i] =
			prop_chgalgo_device_register(info->dev,
						     pca_mtk_chg_desc_tbl[i],
						     info);
		if (IS_ERR_OR_NULL(info->pca[i]))
			return PTR_ERR(info->pca[i]);
	}

	return 0;
}

static int pca_mtk_chg_probe(struct platform_device *pdev)
{
	int ret, i;
	struct pca_mtk_chg_info *info;

	dev_info(&pdev->dev, "%s(%s)\n", __func__, PCA_MTK_CHG_VERSION);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

	for (i = 0; i < MTK_CHGTYP_MAX; i++) {
		info->chgdev[i] = get_charger_by_name(mtk_chgtyp_name[i]);
		if (!info->chgdev[i])
			dev_notice(info->dev, "%s get %s fail\n", __func__,
				mtk_chgtyp_name[i]);
	}
	if (!info->chgdev[MTK_CHGTYP_SWCHG])
		return -ENODEV;
	info->chg_consumer = charger_manager_get_by_name(info->dev,
							 "charger_port1");
	if (!info->chg_consumer) {
		dev_notice(info->dev, "%s get charger consumer fail\n", __func__);
		return -ENODEV;
	}

	ret = pca_mtk_chg_register(info);
	if (ret < 0) {
		dev_notice(info->dev, "%s register pca fail(%d)\n", __func__, ret);
		return ret;
	}

	dev_info(info->dev, "%s successfully\n", __func__);
	return 0;
}

static int pca_mtk_chg_remove(struct platform_device *pdev)
{
	int i;
	struct pca_mtk_chg_info *info = platform_get_drvdata(pdev);

	if (info) {
		for (i = 0; i < MTK_CHGTYP_MAX; i++) {
			if (info->pca[i])
				prop_chgalgo_device_unregister(info->pca[i]);
		}
	}
	return 0;
}

static struct platform_device pca_mtk_chg_platdev = {
	.name = "pca_mtk_chg",
	.id = PLATFORM_DEVID_NONE,
};

static struct platform_driver pca_mtk_chg_platdrv = {
	.probe = pca_mtk_chg_probe,
	.remove = pca_mtk_chg_remove,
	.driver = {
		.name = "pca_mtk_chg",
		.owner = THIS_MODULE,
	},
};

static int __init pca_mtk_chg_init(void)
{
	platform_device_register(&pca_mtk_chg_platdev);
	return platform_driver_register(&pca_mtk_chg_platdrv);
}

static void __exit pca_mtk_chg_exit(void)
{
	platform_driver_unregister(&pca_mtk_chg_platdrv);
	platform_device_unregister(&pca_mtk_chg_platdev);
}
device_initcall_sync(pca_mtk_chg_init);
module_exit(pca_mtk_chg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK Charger Interface for PCA");
MODULE_AUTHOR("ShuFan Lee <shufan_lee@richtek.com>");
MODULE_VERSION(PCA_MTK_CHG_VERSION);

/*
 * 2.0.0
 * (1) Adapt to new charger_class ops
 * (2) Adapt to prop_chgalgo_class v2.0.0
 * (3) Add high voltage divider charger
 * (4) Add enable/set_auto_trans ops
 *
 * Revision Note
 * 1.0.4
 * (1) Add init_chip ops
 *
 * 1.0.3
 * (1) Remove BIF support
 *
 * 1.0.2
 * (1) Add is_vbuslowerr API
 *
 * 1.0.1
 * (1) Add setting vbat/vbus ovp alarm APIs
 *
 * 1.0.0
 * Initial Release
 */
