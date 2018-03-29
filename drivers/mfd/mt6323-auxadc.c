/*
 * Copyright (C) 2016 MediaTek, Inc.
 *
 * Author: Chen Zhong <chen.zhong@mediatek.com>
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

#include <linux/mfd/mt6323/registers.h>
#include <linux/mfd/mt6323/mt6323-auxadc.h>
#include <linux/mfd/mt6397/core.h>

#define AUXADC_TIME_OUT	100
#define VOLTAGE_FULL_RANGE	1800
#define MT6323_ADC_PRECISE	32768

/* AUXADC HW init register mask */
#define MT6323_AUXADC_RSTB_SW_MASK		(1 << 5)
#define MT6323_AUXADC_RSTB_SW_VAL(x)		((x) << 5)
#define MT6323_AUXADC_RSTB_SEL_MASK		(1 << 7)
#define MT6323_AUXADC_RSTB_SEL_VAL(x)		((x) << 7)
#define MT6323_AUXADC_CTL_CK_PDN_MASK		(1 << 5)
#define MT6323_AUXADC_CTL_CK_PDN_VAL(x)		((x) << 5)
#define MT6323_AUXADC_TRIM_CH6_SEL_MASK		(0x3 << 2)
#define MT6323_AUXADC_TRIM_CH6_SEL_VAL(x)	((x) << 2)
#define MT6323_AUXADC_TRIM_CH5_SEL_MASK		(0x3 << 4)
#define MT6323_AUXADC_TRIM_CH5_SEL_VAL(x)	((x) << 4)
#define MT6323_AUXADC_TRIM_CH4_SEL_MASK		(0x3 << 8)
#define MT6323_AUXADC_TRIM_CH4_SEL_VAL(x)	((x) << 8)
#define MT6323_AUXADC_TRIM_CH2_SEL_MASK		(0x3 << 10)
#define MT6323_AUXADC_TRIM_CH2_SEL_VAL(x)	((x) << 10)
#define MT6323_AUXADC_VREF18_ENB_MD_MASK	(0x1 << 15)
#define MT6323_AUXADC_VREF18_ENB_MD_VAL(x)	((x) << 15)
#define MT6323_AUXADC_GPS_STATUS_MASK		(0x1 << 1)
#define MT6323_AUXADC_GPS_STATUS_VAL(x)		((x) << 1)
#define MT6323_AUXADC_MD_STATUS_MASK		(0x1 << 0)
#define MT6323_AUXADC_MD_STATUS_VAL(x)		((x) << 0)
#define MT6323_AUXADC_VREF18_SELB_MASK		(0x1 << 1)
#define MT6323_AUXADC_VREF18_SELB_VAL(x)	((x) << 1)
#define MT6323_AUXADC_DECI_GDLY_SEL_MASK	(0x1 << 0)
#define MT6323_AUXADC_DECI_GDLY_SEL_VAL(x)	((x) << 0)
#define MT6323_AUXADC_OSR_MASK			(0x7 << 10)
#define MT6323_AUXADC_OSR_VAL(x)		((x) << 10)

#define MT6323_AUXADC_VBUF_EN_MASK		(0x1 << 4)
#define MT6323_AUXADC_VBUF_EN_VAL(x)		((x) << 4)

static const u32 mt6323_auxadc_regs[] = {
	MT6323_AUXADC_ADC6, MT6323_AUXADC_ADC11, MT6323_AUXADC_ADC5,
	MT6323_AUXADC_ADC4, MT6323_AUXADC_ADC2, MT6323_AUXADC_ADC3,
	MT6323_AUXADC_ADC1, MT6323_AUXADC_ADC0, MT6323_AUXADC_ADC7,
	MT6323_AUXADC_ADC17,
};

struct mt6323_auxadc {
	struct device *dev;
	struct regmap *regmap;
	struct mutex auxadc_lock;
	struct wake_lock wake_lock;
};

static struct mt6323_auxadc *auxadc;

static int mt6323_auxadc_hw_init(struct mt6323_auxadc *mt6323_auxadc)
{
	int ret;

	ret = regmap_update_bits(mt6323_auxadc->regmap, MT6323_STRUP_CON10,
			MT6323_AUXADC_RSTB_SW_MASK | MT6323_AUXADC_RSTB_SEL_MASK,
			MT6323_AUXADC_RSTB_SW_VAL(0x1) | MT6323_AUXADC_RSTB_SEL_VAL(0x1));
	if (ret < 0)
			return ret;

	ret = regmap_update_bits(mt6323_auxadc->regmap, MT6323_TOP_CKPDN2,
			MT6323_AUXADC_CTL_CK_PDN_MASK, MT6323_AUXADC_CTL_CK_PDN_VAL(0x1));
	if (ret < 0)
			return ret;

	ret = regmap_update_bits(mt6323_auxadc->regmap, MT6323_AUXADC_CON10,
			MT6323_AUXADC_TRIM_CH6_SEL_MASK |
			MT6323_AUXADC_TRIM_CH5_SEL_MASK |
			MT6323_AUXADC_TRIM_CH4_SEL_MASK |
			MT6323_AUXADC_TRIM_CH2_SEL_MASK,
			MT6323_AUXADC_TRIM_CH6_SEL_VAL(0x1) |
			MT6323_AUXADC_TRIM_CH5_SEL_VAL(0x1) |
			MT6323_AUXADC_TRIM_CH4_SEL_VAL(0x1) |
			MT6323_AUXADC_TRIM_CH2_SEL_VAL(0x1));
	if (ret < 0)
			return ret;

	ret = regmap_update_bits(mt6323_auxadc->regmap, MT6323_AUXADC_CON27,
			MT6323_AUXADC_VREF18_ENB_MD_MASK | MT6323_AUXADC_MD_STATUS_MASK,
			MT6323_AUXADC_VREF18_ENB_MD_VAL(0x1) | MT6323_AUXADC_MD_STATUS_VAL(0x1));
	if (ret < 0)
			return ret;

	ret = regmap_update_bits(mt6323_auxadc->regmap, MT6323_AUXADC_CON19,
			MT6323_AUXADC_GPS_STATUS_MASK, MT6323_AUXADC_GPS_STATUS_VAL(0x1));
	if (ret < 0)
			return ret;

	ret = regmap_update_bits(mt6323_auxadc->regmap, MT6323_AUXADC_CON26,
			MT6323_AUXADC_VREF18_SELB_MASK | MT6323_AUXADC_DECI_GDLY_SEL_MASK,
			MT6323_AUXADC_VREF18_SELB_VAL(0x1) | MT6323_AUXADC_DECI_GDLY_SEL_VAL(0x1));
	if (ret < 0)
			return ret;

	ret = regmap_update_bits(mt6323_auxadc->regmap, MT6323_AUXADC_CON9,
			MT6323_AUXADC_OSR_MASK, MT6323_AUXADC_OSR_VAL(0x3));
	if (ret < 0)
			return ret;

	return 0;
}

static int mt6323_auxadc_deci_gdly(void)
{
	int ret;
	u32 regval, deci_gldy;

	ret = regmap_read(auxadc->regmap, MT6323_AUXADC_CON19, &regval);
	if (ret < 0) {
		dev_err(auxadc->dev, "Failed to read decimation group delay: %d\n", ret);
		return ret;
	}

	deci_gldy = (regval >> 14) & 0x3;

	return (deci_gldy == 1) ? 1 : 0;
}

static int mt6323_auxadc_polling_channel(void)
{
	int ret;
	u32 regval;

	while (mt6323_auxadc_deci_gdly()) {
		ret = regmap_read(auxadc->regmap, MT6323_AUXADC_ADC19, &regval);
		if (ret < 0)
			return ret;

		if (((regval >> 1) & 0x7FFF) == 0) {
			ret = regmap_update_bits(auxadc->regmap, MT6323_AUXADC_CON19, 0x3 << 14, 0x0);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int mt6323_auxadc_ap_request(int dwChannel)
{
	int ret;
	u32 regval, adc_regval;

	if (dwChannel < 9) {
		ret = regmap_update_bits(auxadc->regmap, MT6323_AUXADC_CON11,
				MT6323_AUXADC_VBUF_EN_MASK, MT6323_AUXADC_VBUF_EN_VAL(0x1));
		if (ret < 0)
			return ret;

		/* set to 0 */
		ret = regmap_read(auxadc->regmap, MT6323_AUXADC_CON22, &regval);
		if (ret < 0)
			return ret;

		adc_regval = regval & 0x1FF;
		adc_regval = adc_regval & (~(1 << dwChannel));

		ret = regmap_update_bits(auxadc->regmap, MT6323_AUXADC_CON22,
				0x1FF << 0, adc_regval << 0);
		if (ret < 0)
			return ret;

		/* set to 1 */
		ret = regmap_read(auxadc->regmap, MT6323_AUXADC_CON22, &regval);
		if (ret < 0)
			return ret;

		adc_regval = regval & 0x1FF;
		adc_regval = adc_regval | (1 << dwChannel);

		ret = regmap_update_bits(auxadc->regmap, MT6323_AUXADC_CON22,
				0x1FF << 0, adc_regval << 0);
		if (ret < 0)
			return ret;

	} else if ((dwChannel >= 9) && (dwChannel <= 16)) {
		/* set to 0 */
		ret = regmap_read(auxadc->regmap, MT6323_AUXADC_CON23, &regval);
		if (ret < 0)
			return ret;

		adc_regval = regval & 0xFF;
		adc_regval = adc_regval & (~(1 << (dwChannel - 9)));

		ret = regmap_update_bits(auxadc->regmap, MT6323_AUXADC_CON23,
				0xFF << 0, adc_regval << 0);
		if (ret < 0)
			return ret;

		/* set to 1 */
		ret = regmap_read(auxadc->regmap, MT6323_AUXADC_CON23, &regval);
		if (ret < 0)
			return ret;

		adc_regval = regval & 0xFF;
		adc_regval = adc_regval | (1 << (dwChannel - 9));

		ret = regmap_update_bits(auxadc->regmap, MT6323_AUXADC_CON23,
				0xFF << 0, adc_regval << 0);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int mt6323_auxadc_get_rdy(unsigned int reg)
{
	int ret;
	u32 regval, adc_rdy;

	ret = regmap_read(auxadc->regmap, reg, &regval);
	if (ret < 0)
		return ret;

	adc_rdy = (regval & BIT(15)) >> 15;

	return (adc_rdy == 1) ? 1 : 0;
}

static int mt6323_auxadc_read_adc_out(unsigned int reg)
{
	int ret;
	u32 regval, adc_rdy;
	int count = 0;

	adc_rdy = mt6323_auxadc_get_rdy(reg);

	while (adc_rdy != 1) {
		if (adc_rdy < 0) {
			dev_err(auxadc->dev, "Failed to read adc ready register: %d\n", ret);
			return ret;
		}

		usleep_range(1000, 2000);
		if ((count++) > AUXADC_TIME_OUT) {
			dev_err(auxadc->dev, "Get auxadc channel (0x%x) raw data time out!\n", reg);
			break;
		}
		adc_rdy = mt6323_auxadc_get_rdy(reg);
	}

	ret = regmap_read(auxadc->regmap, reg, &regval);
	if (ret < 0) {
		dev_err(auxadc->dev, "Failed to read adc out register: %d\n", ret);
		return ret;
	}

	return (regval & 0x7FFF);
}

static int mt6323_auxadc_raw_to_voltage(int dwChannel, u32 adc_result_raw)
{
	u32 multi;
	u32 adc_result;

	switch (dwChannel) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 8:
		multi = 1;
		adc_result = (adc_result_raw * multi * VOLTAGE_FULL_RANGE) / MT6323_ADC_PRECISE;
		break;
	case 6:
	case 7:
		multi = 4;
		adc_result = (adc_result_raw * multi * VOLTAGE_FULL_RANGE) / MT6323_ADC_PRECISE;
		break;
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
		adc_result = adc_result_raw;
		break;
	default:
		dev_err(auxadc->dev, "Invalid auxadc channel (%d) !\n", dwChannel);
		return -1;
	}

	return adc_result;
}

/**
 * PMIC_IMM_GetOneChannelValue() - Get voltage from a auxadc channel
 *
 * @dwChannel: Auxadc channel to choose
 *             0 : BATON2 **
 *             1 : CH6
 *             2 : THR SENSE2 **
 *             3 : THR SENSE1
 *             4 : VCDT
 *             5 : BATON1
 *             6 : ISENSE
 *             7 : BATSNS
 *             8 : ACCDET
 *             9-16 : audio
 * @deCount: Count time to be averaged
 * @trimd: Auxadc value from trimmed register or not.
 *         mt6323 auxadc has no trimmed register, reserve it to align with mt6397
 *
 * This returns the current voltage of the specified channel in mV,
 * zero for not supported channel, a negative number on error.
 */
int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd)
{
	int ret;
	int sample_times = 0;
	u32 adc_out = 0;
	u32 raw_data = 0;
	u32 adc_result_raw;
	u32 adc_result;

	if (!auxadc)
		return -EPROBE_DEFER;

	/* do not support BATON2 and THR SENSE2 for sw workaround */
	if (dwChannel == 0 || dwChannel == 2)
		return 0;

	wake_lock(&auxadc->wake_lock);

	do {
		mutex_lock(&auxadc->auxadc_lock);

		ret = mt6323_auxadc_polling_channel();
		if (ret < 0)
			goto out1;

		mt6323_auxadc_ap_request(dwChannel);
		if (ret < 0)
			goto out1;

		mutex_unlock(&auxadc->auxadc_lock);

		/* delay for HW limitation to wait for auxadc ready */
		udelay(300);

		if (dwChannel < 9)
			adc_out = mt6323_auxadc_read_adc_out(mt6323_auxadc_regs[dwChannel]);
		else if ((dwChannel >= 9) && (dwChannel <= 16))
			adc_out = mt6323_auxadc_read_adc_out(mt6323_auxadc_regs[9]);

		if (adc_out < 0)
			goto out2;

		raw_data += adc_out;
		sample_times++;
	} while (sample_times < deCount);

	adc_result_raw = raw_data / deCount;

	adc_result = mt6323_auxadc_raw_to_voltage(dwChannel, adc_result_raw);
	if (adc_result < 0)
		goto out2;

	wake_unlock(&auxadc->wake_lock);

	return adc_result;

out1:
	mutex_unlock(&auxadc->auxadc_lock);
out2:
	wake_unlock(&auxadc->wake_lock);
	return ret;
}

static const struct of_device_id mt6323_auxadc_of_match[] = {
	{ .compatible = "mediatek,mt6323-auxadc", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6323_auxadc_of_match);

static int mt6323_auxadc_probe(struct platform_device *pdev)
{
	int ret;
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6323_auxadc *mt6323_auxadc;

	mt6323_auxadc = devm_kzalloc(&pdev->dev, sizeof(struct mt6323_auxadc), GFP_KERNEL);
	if (!mt6323_auxadc)
		return -ENOMEM;

	mt6323_auxadc->dev = &pdev->dev;
	mt6323_auxadc->regmap = mt6397_chip->regmap;

	mutex_init(&mt6323_auxadc->auxadc_lock);
	wake_lock_init(&mt6323_auxadc->wake_lock, WAKE_LOCK_SUSPEND, "mt6323 auxadc irq wakelock");

	auxadc = mt6323_auxadc;
	platform_set_drvdata(pdev, mt6323_auxadc);

	ret = mt6323_auxadc_hw_init(mt6323_auxadc);
	if (ret < 0)
		return ret;

	return 0;
}

static struct platform_driver mt6323_auxadc_driver = {
	.probe = mt6323_auxadc_probe,
	.driver = {
		   .name = "mt6323-auxadc",
		   .owner = THIS_MODULE,
		   .of_match_table = mt6323_auxadc_of_match,
	},
};

module_platform_driver(mt6323_auxadc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Chen Zhong <chen.zhong@mediatek.com>");
MODULE_DESCRIPTION("AUXADC Driver for MediaTek MT6323 PMIC");
MODULE_ALIAS("platform:mt6323-auxadc");
