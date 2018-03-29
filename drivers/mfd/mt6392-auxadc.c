/*
 * Copyright (C) 2016 MediaTek, Inc.
 *
 * Author: Dongdong Cheng <dongdong.cheng@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <linux/mfd/mt6392/registers.h>
#include <linux/mfd/mt6392/mt6392-auxadc.h>
#include <linux/mfd/mt6397/core.h>

static const u32 mt6392_auxadc_regs[] = {
	MT6392_AUXADC_ADC0, MT6392_AUXADC_ADC1, MT6392_AUXADC_ADC2,
	MT6392_AUXADC_ADC3, MT6392_AUXADC_ADC4, MT6392_AUXADC_ADC5,
	MT6392_AUXADC_ADC6, MT6392_AUXADC_ADC7, MT6392_AUXADC_ADC8,
	MT6392_AUXADC_ADC9, MT6392_AUXADC_ADC10, MT6392_AUXADC_ADC11,
	MT6392_AUXADC_ADC12, MT6392_AUXADC_ADC12, MT6392_AUXADC_ADC12,
	MT6392_AUXADC_ADC12,
};

struct mt6392_auxadc {
	struct device *dev;
	struct regmap *regmap;
	struct mutex auxadc_lock;
	struct wake_lock wake_lock;
};

static struct mt6392_auxadc *auxadc;

static int mt6392_auxadc_hw_init(struct mt6392_auxadc *mt6392_auxadc)
{
	int ret;

	ret = regmap_update_bits(mt6392_auxadc->regmap, MT6392_AUXADC_CON0,
			MT6392_AUXADC_CK_AON_MASK | MT6392_AUXADC_12M_CK_AON_MASK,
			MT6392_AUXADC_CK_AON_VAL(0x0) | MT6392_AUXADC_12M_CK_AON_VAL(0x0));
	if (ret < 0)
			return ret;

	ret = regmap_update_bits(mt6392_auxadc->regmap, MT6392_STRUP_CON10,
			MT6392_AUXADC_RSTB_SW_MASK | MT6392_AUXADC_RSTB_SEL_MASK,
			MT6392_AUXADC_RSTB_SW_VAL(0x1) | MT6392_AUXADC_RSTB_SEL_VAL(0x1));
	if (ret < 0)
			return ret;

	ret = regmap_update_bits(mt6392_auxadc->regmap, MT6392_AUXADC_CON1,
			MT6392_AUXADC_AVG_NUM_LARGE_MASK | MT6392_AUXADC_AVG_NUM_SMALL_MASK,
			MT6392_AUXADC_AVG_NUM_LARGE_VAL(0x6) | MT6392_AUXADC_AVG_NUM_SMALL_VAL(0x2));
	if (ret < 0)
			return ret;

	ret = regmap_update_bits(mt6392_auxadc->regmap, MT6392_AUXADC_CON2,
			MT6392_AUXADC_AVG_NUM_SEL_MASK, MT6392_AUXADC_RSTB_SW_VAL(0x3));
	if (ret < 0)
			return ret;

	ret = regmap_update_bits(mt6392_auxadc->regmap, MT6392_AUXADC_CON10,
			MT6392_AUXADC_VBUF_EN_MASK, MT6392_AUXADC_VBUF_EN_VAL(0x1));
	if (ret < 0)
			return ret;

	return 0;
}

static int mt6392_auxadc_get_rdy(unsigned int dwChannel, int deCount)
{
	int ret, regval;
	int count = 0;
	int adc_rdy = 0;

	if (dwChannel <= 15) {
		do {
			ret = regmap_read(auxadc->regmap, mt6392_auxadc_regs[dwChannel], &regval);
			if (ret < 0)
				return ret;
			adc_rdy = (regval & (0x1 << 15)) >> 15;
			if (adc_rdy == 1)
				return adc_rdy;
			count++;
		} while (count < deCount);
	}
	return adc_rdy;
}

static int mt6392_auxadc_get_adc_result_raw(unsigned int dwChannel)
{
	int ret, raw_data, regval;

	ret = regmap_read(auxadc->regmap, mt6392_auxadc_regs[dwChannel], &regval);
	if (ret < 0)
		return ret;

	switch (dwChannel) {
	case 0:
	case 1:
		raw_data = (regval & 0x7fff);
		break;
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
		raw_data = (regval & 0xfff);
		break;
	default:
		dev_err(auxadc->dev, "Invalid auxadc channel (%d) !\n", dwChannel);
		return -1;
	}
	return raw_data;
}

static int mt6392_auxadc_get_adc_value(unsigned int dwChannel, int raw_data)
{
	int ret, r_val_temp, adc_result;

	switch (dwChannel) {
	case 0:
	case 1:
		r_val_temp = 3;
		adc_result = (raw_data * r_val_temp * VOLTAGE_FULL_RANGE) / 32768;
		break;
	case 2:
	case 3:
	case 4:
		r_val_temp = 1;
		adc_result = (raw_data * r_val_temp * VOLTAGE_FULL_RANGE) / 4096;
		break;
	case 5:
	case 6:
	case 7:
	case 8:
		r_val_temp = 2;
		adc_result = (raw_data * r_val_temp * VOLTAGE_FULL_RANGE) / 4096;
		break;
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
		r_val_temp = 1;
		adc_result = (raw_data * r_val_temp * VOLTAGE_FULL_RANGE) / 4096;
		break;
	default:
		dev_err(auxadc->dev, "Invalid auxadc channel (%d) !\n", dwChannel);
		return ret;
	}

	return adc_result;
}

/**
 * PMIC_IMM_GetOneChannelValue() - Get voltage from a auxadc channel
 *
 * @dwChannel: Auxadc channel to choose
 *             0 : BATSNS
 *             0 : ISENSE
 *             2 : VCDT
 *             3 : BATON
 *             4 : PMIC_TEMP
 *             6 : TYPE-C
 *             8 : DP/DM
 *             9-11 :
 * @deCount: Count time to be averaged
 * @trimd: Auxadc value from trimmed register or not.
 *         mt6392 auxadc has no trimmed register, reserve it to align with mt6397
 *
 * This returns the current voltage of the specified channel in mV,
 * zero for not supported channel, a negative number on error.
 */

int PMIC_IMM_GetOneChannelValue(unsigned int dwChannel, int deCount, int trimd)
{
	int ret, adc_rdy;
	int raw_data, adc_result;

	wake_lock(&auxadc->wake_lock);
	mutex_lock(&auxadc->auxadc_lock);
#if defined PMIC_DVT_TC_EN
	/* only used for PMIC_DVT */
	ret = regmap_update_bits(auxadc->regmap, MT6392_STRUP_CON10,
		MT6392_AUXADC_START_SEL_MASK | MT6392_AUXADC_RSTB_SW_MASK | MT6392_AUXADC_RSTB_SEL_MASK,
		MT6392_AUXADC_START_SEL_VAL(0x1) | MT6392_AUXADC_RSTB_SW_VAL(0x1) | MT6392_AUXADC_RSTB_SEL_VAL(0x1));
	if (ret < 0)
		return ret;
#endif
	ret = regmap_update_bits(auxadc->regmap, MT6392_AUXADC_RQST0_SET,
			MT6392_AUXADC_RQST0_SET_MASK, (MT6392_AUXADC_RQST0_SET_VAL(0x1) << dwChannel));
	if (ret < 0)
		return ret;
	/*the spec is 10us*/
	udelay(50);
	adc_rdy = mt6392_auxadc_get_rdy(dwChannel, deCount);
	if (adc_rdy != 1) {
		dev_err(auxadc->dev, "Failed to read adc ready register: %d\n", adc_rdy);
		return adc_rdy;
	}
	raw_data = mt6392_auxadc_get_adc_result_raw(dwChannel);
	if (raw_data < 0) {
		dev_err(auxadc->dev, "Failed to read adc raw data register: %d\n", raw_data);
		return raw_data;
	}
	adc_result = mt6392_auxadc_get_adc_value(dwChannel, raw_data);
	mutex_unlock(&auxadc->auxadc_lock);
	wake_unlock(&auxadc->wake_lock);

	return adc_result;
}

static const struct of_device_id mt6392_auxadc_of_match[] = {
	{ .compatible = "mediatek,mt6392-auxadc", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6392_auxadc_of_match);

static int mt6392_auxadc_probe(struct platform_device *pdev)
{
	int ret;
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6392_auxadc *mt6392_auxadc;

	mt6392_auxadc = devm_kzalloc(&pdev->dev, sizeof(struct mt6392_auxadc), GFP_KERNEL);
	if (!mt6392_auxadc)
		return -ENOMEM;

	mt6392_auxadc->dev = &pdev->dev;
	mt6392_auxadc->regmap = mt6397_chip->regmap;

	mutex_init(&mt6392_auxadc->auxadc_lock);
	wake_lock_init(&mt6392_auxadc->wake_lock, WAKE_LOCK_SUSPEND, "mt6392 auxadc irq wakelock");

	auxadc = mt6392_auxadc;
	platform_set_drvdata(pdev, mt6392_auxadc);

	ret = mt6392_auxadc_hw_init(mt6392_auxadc);
	if (ret < 0)
		return ret;

	return 0;
}

static struct platform_driver mt6392_auxadc_driver = {
	.probe = mt6392_auxadc_probe,
	.driver = {
		   .name = "mt6392-auxadc",
		   .owner = THIS_MODULE,
		   .of_match_table = mt6392_auxadc_of_match,
	},
};

module_platform_driver(mt6392_auxadc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dongdong Cheng <dongdong.cheng@mediatek.com>");
MODULE_DESCRIPTION("AUXADC Driver for MediaTek MT6392 PMIC");
MODULE_ALIAS("platform:mt6392-auxadc");
