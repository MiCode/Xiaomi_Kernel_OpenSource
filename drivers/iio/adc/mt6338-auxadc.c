// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <dt-bindings/iio/mt635x-auxadc.h>

#define AUXADC_RDY_BIT			BIT(15)

#define AUXADC_DEF_R_RATIO		1
#define AUXADC_DEF_AVG_NUM		8

#define AUXADC_AVG_TIME_US		10
#define AUXADC_POLL_DELAY_US		100
#define AUXADC_TIMEOUT_US		32000
#define VOLT_FULL			1800
#define IMP_STOP_DELAY_US		150

struct mt6338_auxadc_device {
	struct regmap *regmap;
	struct device *dev;
	unsigned int nchannels;
	struct iio_chan_spec *iio_chans;
	struct mutex lock;
	const struct auxadc_info *info;
};

/*
 * @ch_name:	HW channel name
 * @ch_num:	HW channel number
 * @res:	ADC resolution
 * @r_ratio:	resistance ratio, represented by r_ratio[0] / r_ratio[1]
 * @avg_num:	sampling times of AUXADC measurments then average it
 * @regs:	request and data output registers for this channel
 * @has_regs:	determine if this channel has request and data output registers
 */
struct auxadc_channels {
	enum iio_chan_type type;
	long info_mask;
	/* AUXADC channel attribute */
	const char *ch_name;
	unsigned char ch_num;
	unsigned char res;
	unsigned char r_ratio[2];
	unsigned short avg_num;
	const struct auxadc_regs *regs;
	bool has_regs;
};

#define MT6338_AUXADC_CHANNEL(_ch_name, _ch_num, _res, _has_regs) \
	[AUXADC_##_ch_name] = {			\
		.type = IIO_VOLTAGE,			\
		.info_mask = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED), \
		.ch_name = __stringify(_ch_name),	\
		.ch_num = _ch_num,			\
		.res = _res,				\
		.has_regs = _has_regs,			\
	}

/*
 * The array represents all possible AUXADC channels found
 * in the supported PMICs.
 */
static struct auxadc_channels auxadc_chans[] = {
	MT6338_AUXADC_CHANNEL(CHIP_TEMP, 4, 12, true),
	MT6338_AUXADC_CHANNEL(ACCDET, 5, 12, true),
	MT6338_AUXADC_CHANNEL(HPOFS_CAL, 9, 15, true),
};

struct auxadc_regs {
	unsigned int rqst_reg;
	unsigned int rqst_shift;
	unsigned int rdy_reg;
	unsigned int out_reg;
};

#define MT6338_RG_TSENS_EN_ADDR			0x98b
#define MT6338_RG_TSENS_EN_SHIFT		7
#define MT6338_AUXADC_ADC4_L			0x1088
#define MT6338_AUXADC_ADC4_H			0x1089
#define MT6338_AUXADC_ADC5_L			0x108a
#define MT6338_AUXADC_ADC5_H			0x108b
#define MT6338_AUXADC_ADC9_L			0x108c
#define MT6338_AUXADC_ADC9_H			0x108d
#define MT6338_AUXADC_RQST0			0x1107
#define MT6338_AUXADC_RQST_CH4_SHIFT		0
#define MT6338_AUXADC_RQST_CH5_SHIFT		1
#define MT6338_AUXADC_RQST_CH9_SHIFT		2
#define MT6338_AUXADC_ADC_OUT_CH4_L_ADDR	MT6338_AUXADC_ADC4_L
#define MT6338_AUXADC_ADC_RDY_CH4_ADDR		MT6338_AUXADC_ADC4_H
#define MT6338_AUXADC_ADC_OUT_CH5_L_ADDR        MT6338_AUXADC_ADC5_L
#define MT6338_AUXADC_ADC_RDY_CH5_ADDR		MT6338_AUXADC_ADC5_H
#define MT6338_AUXADC_ADC_OUT_CH9_L_ADDR        MT6338_AUXADC_ADC9_L
#define MT6338_AUXADC_ADC_RDY_CH9_ADDR		MT6338_AUXADC_ADC9_H

#define MT6338_AUXADC_REG(_ch_name, _chip, _rqst_reg, _rqst_shift, _rdy_reg, _out_reg) \
	[AUXADC_##_ch_name] = {			\
		.rqst_reg = _chip##_##_rqst_reg,	\
		.rqst_shift = _rqst_shift,		\
		.rdy_reg = _rdy_reg,			\
		.out_reg = _out_reg,			\
	}						\

static const struct auxadc_regs mt6338_auxadc_regs_tbl[] = {
	MT6338_AUXADC_REG(CHIP_TEMP, MT6338, AUXADC_RQST0, MT6338_AUXADC_RQST_CH4_SHIFT,
			  MT6338_AUXADC_ADC_RDY_CH4_ADDR, MT6338_AUXADC_ADC_OUT_CH4_L_ADDR),
	MT6338_AUXADC_REG(ACCDET, MT6338, AUXADC_RQST0, MT6338_AUXADC_RQST_CH5_SHIFT,
			  MT6338_AUXADC_ADC_RDY_CH5_ADDR, MT6338_AUXADC_ADC_OUT_CH5_L_ADDR),
	MT6338_AUXADC_REG(HPOFS_CAL, MT6338, AUXADC_RQST0, MT6338_AUXADC_RQST_CH9_SHIFT,
			  MT6338_AUXADC_ADC_RDY_CH9_ADDR, MT6338_AUXADC_ADC_OUT_CH9_L_ADDR),
};

#define MT6338_HK_TOP_RST_CON0                                0xf8d
#define MT6338_HK_TOP_WKEY_L                                  0xf94
#define MT6338_HK_TOP_WKEY_H                                  0xf95
static const unsigned int mt6338_rst_setting[][3] = {
	{
		MT6338_HK_TOP_WKEY_L, 0xFF, 0x38,
	}, {
		MT6338_HK_TOP_WKEY_H, 0xFF, 0x63,
	}, {
		MT6338_HK_TOP_RST_CON0, 0x9, 0x9,
	}, {
		MT6338_HK_TOP_RST_CON0, 0x9, 0,
	}, {
		MT6338_HK_TOP_WKEY_H, 0xFF, 0,
	}, {
		MT6338_HK_TOP_WKEY_L, 0xFF, 0,
	}
};

struct auxadc_info {
	const struct auxadc_regs *regs_tbl;
	const unsigned int (*rst_setting)[3];
	unsigned int num_rst_setting;
};

static const struct auxadc_info mt6338_info = {
	.regs_tbl = mt6338_auxadc_regs_tbl,
	.rst_setting = mt6338_rst_setting,
	.num_rst_setting = ARRAY_SIZE(mt6338_rst_setting),
};

#define mt6338_regmap_bulk_read_poll_timeout(map, addr, val, cond, sleep_us, timeout_us) \
({ \
	u64 __timeout_us = (timeout_us); \
	unsigned long __sleep_us = (sleep_us); \
	ktime_t __timeout = ktime_add_us(ktime_get(), __timeout_us); \
	int __ret; \
	might_sleep_if(__sleep_us); \
	for (;;) { \
		__ret = regmap_bulk_read((map), (addr), &(val), 2); \
		if (__ret) \
			break; \
		if (cond) \
			break; \
		if ((__timeout_us) && ktime_compare(ktime_get(), __timeout) > 0) { \
			__ret = regmap_bulk_read((map), (addr), (u8 *) &(val), 2); \
			break; \
		} \
		if (__sleep_us) \
			usleep_range((__sleep_us >> 2) + 1, __sleep_us); \
	} \
	__ret ?: ((cond) ? 0 : -ETIMEDOUT); \
})

/*
 * @adc_dev:	 pointer to the struct mt6338_auxadc_device
 * @auxadc_chan: pointer to the struct auxadc_channels, it represents specific
		 auxadc channel
 * @val:	 pointer to output value
 */
static int get_auxadc_out(struct mt6338_auxadc_device *adc_dev,
			  const struct auxadc_channels *auxadc_chan, int *val)
{
	int ret;

	regmap_write(adc_dev->regmap,
		     auxadc_chan->regs->rqst_reg,
		     BIT(auxadc_chan->regs->rqst_shift));
	usleep_range(auxadc_chan->avg_num * AUXADC_AVG_TIME_US,
		     (auxadc_chan->avg_num + 1) * AUXADC_AVG_TIME_US);

	ret = mt6338_regmap_bulk_read_poll_timeout(adc_dev->regmap, auxadc_chan->regs->out_reg,
						   *val, (*val & AUXADC_RDY_BIT),
						   AUXADC_POLL_DELAY_US, AUXADC_TIMEOUT_US);
	*val &= BIT(auxadc_chan->res) - 1;
	if (ret == -ETIMEDOUT)
		dev_info(adc_dev->dev, "(%d)Time out!\n", auxadc_chan->ch_num);

	return ret;
}

static int mt6338_auxadc_read_raw(struct iio_dev *indio_dev,
				  struct iio_chan_spec const *chan,
				  int *val,
				  int *val2,
				  long mask)
{
	struct mt6338_auxadc_device *adc_dev = iio_priv(indio_dev);
	const struct auxadc_channels *auxadc_chan;
	int auxadc_out = 0;
	int ret;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 5);

	if (chan->channel == AUXADC_CHIP_TEMP)
		regmap_update_bits(adc_dev->regmap, MT6338_RG_TSENS_EN_ADDR,
				   0x1 << MT6338_RG_TSENS_EN_SHIFT,
				   0x1 << MT6338_RG_TSENS_EN_SHIFT);
	mutex_lock(&adc_dev->lock);
	pm_stay_awake(adc_dev->dev);

	auxadc_chan = &auxadc_chans[chan->channel];
	if (auxadc_chan->regs)
		ret = get_auxadc_out(adc_dev, auxadc_chan, &auxadc_out);
	else
		ret = -EINVAL;

	pm_relax(adc_dev->dev);
	mutex_unlock(&adc_dev->lock);
	if (chan->channel == AUXADC_CHIP_TEMP)
		regmap_update_bits(adc_dev->regmap, MT6338_RG_TSENS_EN_ADDR,
				   0x1 << MT6338_RG_TSENS_EN_SHIFT, 0);
	if (ret != -ETIMEDOUT && ret < 0)
		goto err;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		*val = auxadc_out * auxadc_chan->r_ratio[0] * VOLT_FULL;
		*val = (*val / auxadc_chan->r_ratio[1]) >> auxadc_chan->res;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_RAW:
		*val = auxadc_out;
		ret = IIO_VAL_INT;
		break;
	default:
		return -EINVAL;
	}
	if (__ratelimit(&ratelimit)) {
		dev_info(adc_dev->dev,
			"name:%s, channel=%d, adc_out=0x%x, adc_result=%d\n",
			auxadc_chan->ch_name, auxadc_chan->ch_num,
			auxadc_out, *val);
	}

err:
	return ret;
}

static int mt6338_auxadc_of_xlate(struct iio_dev *indio_dev,
				  const struct of_phandle_args *iiospec)
{
	int i;

	for (i = 0; i < indio_dev->num_channels; i++) {
		if (indio_dev->channels[i].channel == iiospec->args[0])
			return i;
	}

	return -EINVAL;
}

static const struct iio_info mt6338_auxadc_info = {
	.read_raw = &mt6338_auxadc_read_raw,
	.of_xlate = &mt6338_auxadc_of_xlate,
};

static int auxadc_get_data_from_dt(struct mt6338_auxadc_device *adc_dev,
				   unsigned int *channel,
				   struct device_node *node)
{
	struct auxadc_channels *auxadc_chan;
	unsigned int value = 0, val_arr[2] = {0};
	int ret;

	ret = of_property_read_u32(node, "channel", channel);
	if (ret) {
		dev_notice(adc_dev->dev,
			"invalid channel in node:%s\n", node->name);
		return ret;
	}
	if (*channel < AUXADC_CHAN_MIN || *channel > AUXADC_CHAN_MAX) {
		dev_notice(adc_dev->dev,
			"invalid channel number %d in node:%s\n",
			*channel, node->name);
		return -EINVAL;
	}
	if (*channel >= ARRAY_SIZE(auxadc_chans)) {
		dev_notice(adc_dev->dev, "channel number %d in node:%s not exists\n",
			   *channel, node->name);
		return -EINVAL;
	}
	auxadc_chan = &auxadc_chans[*channel];

	ret = of_property_read_u32_array(node, "resistance-ratio", val_arr, 2);
	if (!ret) {
		auxadc_chan->r_ratio[0] = val_arr[0];
		auxadc_chan->r_ratio[1] = val_arr[1];
	} else {
		auxadc_chan->r_ratio[0] = AUXADC_DEF_R_RATIO;
		auxadc_chan->r_ratio[1] = 1;
	}

	ret = of_property_read_u32(node, "avg-num", &value);
	if (!ret)
		auxadc_chan->avg_num = value;
	else
		auxadc_chan->avg_num = AUXADC_DEF_AVG_NUM;

	return 0;
}

static int auxadc_parse_dt(struct mt6338_auxadc_device *adc_dev,
			   struct device_node *node)
{
	struct iio_chan_spec *iio_chan;
	struct device_node *child;
	unsigned int channel = 0, index = 0;
	int ret;

	adc_dev->nchannels = of_get_available_child_count(node);
	if (!adc_dev->nchannels)
		return -EINVAL;

	adc_dev->iio_chans = devm_kcalloc(adc_dev->dev, adc_dev->nchannels,
		sizeof(*adc_dev->iio_chans), GFP_KERNEL);
	if (!adc_dev->iio_chans)
		return -ENOMEM;
	iio_chan = adc_dev->iio_chans;

	for_each_available_child_of_node(node, child) {
		ret = auxadc_get_data_from_dt(adc_dev, &channel, child);
		if (ret) {
			of_node_put(child);
			return ret;
		}
		if (auxadc_chans[channel].has_regs) {
			auxadc_chans[channel].regs =
				&adc_dev->info->regs_tbl[channel];
		}

		iio_chan->channel = channel;
		iio_chan->datasheet_name = auxadc_chans[channel].ch_name;
		iio_chan->extend_name = auxadc_chans[channel].ch_name;
		iio_chan->info_mask_separate = auxadc_chans[channel].info_mask;
		iio_chan->type = auxadc_chans[channel].type;
		iio_chan->indexed = 1;
		iio_chan->address = index++;

		iio_chan++;
	}

	return 0;
}

static int mt6338_auxadc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct mt6338_auxadc_device *adc_dev;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*adc_dev));
	if (!indio_dev)
		return -ENOMEM;

	adc_dev = iio_priv(indio_dev);
	adc_dev->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!adc_dev->regmap) {
		dev_info(&pdev->dev, "Faled to get parent regmap\n");
		return -ENODEV;
	}
	adc_dev->dev = &pdev->dev;
	mutex_init(&adc_dev->lock);
	device_init_wakeup(&pdev->dev, true);
	adc_dev->info = of_device_get_match_data(&pdev->dev);

	ret = auxadc_parse_dt(adc_dev, node);
	if (ret < 0) {
		dev_notice(&pdev->dev, "auxadc_parse_dt fail, ret=%d\n", ret);
		return ret;
	}

	indio_dev->dev.parent = &pdev->dev;
	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->info = &mt6338_auxadc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = adc_dev->iio_chans;
	indio_dev->num_channels = adc_dev->nchannels;
	indio_dev->dev.of_node = node;

	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret < 0) {
		dev_notice(&pdev->dev, "failed to register iio device!\n");
		return ret;
	}
	dev_info(&pdev->dev, "%s done\n", __func__);

	return 0;
}

static const struct of_device_id mt6338_auxadc_of_match[] = {
	{
		.compatible = "mediatek,mt6338-auxadc",
		.data = &mt6338_info,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, mt6338_auxadc_of_match);

static struct platform_driver mt6338_auxadc_driver = {
	.driver = {
		.name = "mt6338-auxadc",
		.of_match_table = mt6338_auxadc_of_match,
	},
	.probe = mt6338_auxadc_probe,
};
module_platform_driver(mt6338_auxadc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Wen Su <Wen.Su@mediatek.com>");
MODULE_DESCRIPTION("MediaTek PMIC AUXADC Driver for MT6338 PMIC");
