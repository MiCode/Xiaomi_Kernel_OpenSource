/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>

#include <mt-plat/upmu_common.h>
#include "pmic.h"
#include "mt635x-auxadc-internal.h"

#define AUXADC_DEF_R_RATIO		1
#define AUXADC_DEF_AVG_NUM		8

#define AUXADC_AVG_TIME_US		10
#define MAX_TIMEOUT_COUNT		160 /* 100us-200us * 160 = 16ms-32ms */
#define VOLT_FULL			1800

#define AUXADC_CHAN_MIN			AUXADC_BATADC
#define AUXADC_CHAN_MAX			AUXADC_VBIF

struct mt635x_auxadc_device {
	struct device		*dev;
	unsigned int		nchannels;
	struct iio_chan_spec	*iio_chans;
	struct mutex		lock;
};

/*
 * @res:	ADC resolution
 * @r_ratio:	resistance ratio, represented by r_ratio[0] / r_ratio[1]
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
	struct auxadc_regs *regs;
	void (*convert_fn)(unsigned char convert);
	int (*cali_fn)(int val, int precision_factor);
};

#define MT635x_AUXADC_CHANNEL(_ch_name, _ch_num, _res)	\
	[AUXADC_##_ch_name] = {						\
		.type = IIO_VOLTAGE,					\
		.info_mask = BIT(IIO_CHAN_INFO_RAW) |			\
			BIT(IIO_CHAN_INFO_PROCESSED),			\
		.ch_name = __stringify(_ch_name),			\
		.ch_num = _ch_num,					\
		.res = _res,						\
	}								\

/*
 * The array represents all possible ADC channels found in the supported PMICs.
 * Every index in the array is equal to the channel number per datasheet. The
 * gaps in the array should be treated as reserved channels.
 */
static struct auxadc_channels auxadc_chans[] = {
	MT635x_AUXADC_CHANNEL(BATADC, 0, 15),
	MT635x_AUXADC_CHANNEL(ISENSE, 0, 15),
	MT635x_AUXADC_CHANNEL(VCDT, 2, 12),
	MT635x_AUXADC_CHANNEL(BAT_TEMP, 3, 12),
	MT635x_AUXADC_CHANNEL(BATID, 3, 12),
	MT635x_AUXADC_CHANNEL(CHIP_TEMP, 4, 12),
	MT635x_AUXADC_CHANNEL(VCORE_TEMP, 4, 12),
	MT635x_AUXADC_CHANNEL(VPROC_TEMP, 4, 12),
	MT635x_AUXADC_CHANNEL(VGPU_TEMP, 4, 12),
	MT635x_AUXADC_CHANNEL(ACCDET, 5, 12),
	MT635x_AUXADC_CHANNEL(VDCXO, 6, 12),
	MT635x_AUXADC_CHANNEL(TSX_TEMP, 7, 15),
	MT635x_AUXADC_CHANNEL(HPOFS_CAL, 9, 15),
	MT635x_AUXADC_CHANNEL(DCXO_TEMP, 10, 15),
	MT635x_AUXADC_CHANNEL(VBIF, 11, 12),
};

static unsigned short auxadc_read(enum PMU_FLAGS_LIST auxadc_reg);
static unsigned short auxadc_write(enum PMU_FLAGS_LIST auxadc_reg,
				   unsigned int val);

void __attribute__ ((weak)) pmic_auxadc_chip_timeout_handler(
	struct device *dev, bool is_timeout, unsigned char ch_num)
{
	dev_notice(dev, "(%d)Time out!\n", ch_num);
}

/* Exported function for chip init */
void auxadc_set_regs(int channel, struct auxadc_regs *regs)
{
	auxadc_chans[channel].regs = regs;
}

void auxadc_set_convert_fn(int channel, void (*convert_fn)(unsigned char))
{
	auxadc_chans[channel].convert_fn = convert_fn;
}

void auxadc_set_cali_fn(int channel, int (*cali_fn)(int, int))
{
	auxadc_chans[channel].cali_fn = cali_fn;
}

int auxadc_priv_read_channel(int channel)
{
	int val = 0;
	unsigned int count = 0;
	const struct auxadc_channels *auxadc_chan;

	auxadc_chan = &auxadc_chans[channel];
	if (auxadc_chan->convert_fn)
		auxadc_chan->convert_fn(1);

	if (auxadc_chan->regs->ch_rqst != -1)
		auxadc_write(auxadc_chan->regs->ch_rqst, 1);
	udelay(auxadc_chan->avg_num * AUXADC_AVG_TIME_US);

	while (auxadc_read(auxadc_chan->regs->ch_rdy) != 1) {
		usleep_range(100, 200);
		if ((count++) > MAX_TIMEOUT_COUNT)
			break;
	}
	val = (int)auxadc_read(auxadc_chan->regs->ch_out);
	val = val * auxadc_chan->r_ratio[0] * VOLT_FULL;
	val = (val / auxadc_chan->r_ratio[1]) >> auxadc_chan->res;

	if (auxadc_chan->convert_fn)
		auxadc_chan->convert_fn(0);

	return val;
}

unsigned char *auxadc_get_r_ratio(int channel)
{
	const struct auxadc_channels *auxadc_chan = &auxadc_chans[channel];

	return (unsigned char *)auxadc_chan->r_ratio;
}

#if defined CONFIG_MTK_PMIC_WRAP
#define AUXADC_MAP(_adc_channel_label)				\
	{							\
		.adc_channel_label = _adc_channel_label,	\
		.consumer_dev_name = "1000d000.pwrap:mt-pmic:mt635x-auxadc",\
		.consumer_channel = "AUXADC_"_adc_channel_label,\
	}
#else
#define AUXADC_MAP(_adc_channel_label)				\
	{							\
		.adc_channel_label = _adc_channel_label,	\
		.consumer_dev_name = "mt-pmic:mt635x-auxadc",	\
		.consumer_channel = "AUXADC_"_adc_channel_label,\
	}
#endif
/* for consumer drivers */
static struct iio_map mt635x_auxadc_default_maps[] = {
	AUXADC_MAP("BATADC"),
	AUXADC_MAP("ISENSE"),
	AUXADC_MAP("VCDT"),
	AUXADC_MAP("BAT_TEMP"),
	AUXADC_MAP("BATID"),
	AUXADC_MAP("CHIP_TEMP"),
	AUXADC_MAP("VCORE_TEMP"),
	AUXADC_MAP("VPROC_TEMP"),
	AUXADC_MAP("VGPU_TEMP"),
	AUXADC_MAP("ACCDET"),
	AUXADC_MAP("VDCXO"),
	AUXADC_MAP("TSX_TEMP"),
	AUXADC_MAP("HPOFS_CAL"),
	AUXADC_MAP("DCXO_TEMP"),
	AUXADC_MAP("VBIF"),
	{},
};

static unsigned short auxadc_read(enum PMU_FLAGS_LIST auxadc_reg)
{
	return pmic_get_register_value(auxadc_reg);
}

static unsigned short auxadc_write(
	enum PMU_FLAGS_LIST auxadc_reg, unsigned int val)
{
	return pmic_set_hk_reg_value(auxadc_reg, val);
}

/*
 * @adc_dev:	pointer to the struct mt635x_auxadc_device
 * @channel:	channel number, refer to the auxadc_chans index
		pass from struct iio_chan_spec.channel
 */
static unsigned short get_auxadc_out(struct mt635x_auxadc_device *adc_dev,
				     int channel)
{
	unsigned int count = 0;
	unsigned short auxadc_out = 0;
	bool is_timeout = false;
	const struct auxadc_channels *auxadc_chan;

	auxadc_chan = &auxadc_chans[channel];
	if (auxadc_chan->convert_fn)
		auxadc_chan->convert_fn(1);

	if (auxadc_chan->regs->ch_rqst != -1)
		auxadc_write(auxadc_chan->regs->ch_rqst, 1);
	udelay(auxadc_chan->avg_num * AUXADC_AVG_TIME_US);

	while (auxadc_read(auxadc_chan->regs->ch_rdy) != 1) {
		usleep_range(100, 200);
		if ((count++) > MAX_TIMEOUT_COUNT) {
			is_timeout = true;
			break;
		}
	}
	auxadc_out = auxadc_read(auxadc_chan->regs->ch_out);
	pmic_auxadc_chip_timeout_handler(
		adc_dev->dev,
		is_timeout,
		auxadc_chan->ch_num);

	if (auxadc_chan->convert_fn)
		auxadc_chan->convert_fn(0);

	return auxadc_out;
}

static int mt635x_auxadc_read_raw(struct iio_dev *indio_dev,
				  struct iio_chan_spec const *chan,
				  int *val,
				  int *val2,
				  long mask)
{
	struct mt635x_auxadc_device *adc_dev = iio_priv(indio_dev);
	const struct auxadc_channels *auxadc_chan;
	unsigned short auxadc_out;
	int ret;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 5);

	mutex_lock(&adc_dev->lock);
	pm_stay_awake(adc_dev->dev);

	auxadc_out = get_auxadc_out(adc_dev, chan->channel);

	pm_relax(adc_dev->dev);
	mutex_unlock(&adc_dev->lock);

	auxadc_chan = &auxadc_chans[chan->channel];
	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		*val = (int)(auxadc_out * auxadc_chan->r_ratio[0] * VOLT_FULL);
		*val = (*val / auxadc_chan->r_ratio[1]) >> auxadc_chan->res;
		if (auxadc_chan->cali_fn)
			*val = auxadc_chan->cali_fn(*val, 10);
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_RAW:
		*val = (int)auxadc_out;
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
	} else {
		PMICLOG("name:%s, channel=%d, adc_out=0x%x, adc_result=%d\n",
			auxadc_chan->ch_name, auxadc_chan->ch_num,
			auxadc_out, *val);
	}
	return ret;
}

static int mt635x_auxadc_of_xlate(struct iio_dev *indio_dev,
	const struct of_phandle_args *iiospec)
{
	int i;

	for (i = 0; i < indio_dev->num_channels; i++) {
		if (indio_dev->channels[i].channel == iiospec->args[0])
			return i;
	}

	return -EINVAL;
}

static const struct iio_info mt635x_auxadc_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &mt635x_auxadc_read_raw,
	.of_xlate = &mt635x_auxadc_of_xlate,
};

static int auxadc_get_data_from_dt(struct mt635x_auxadc_device *adc_dev,
	unsigned int *channel,
	struct device_node *node)
{
	struct auxadc_channels *auxadc_chan;
	unsigned int value = 0, val_arr[2] = {0};
	int ret;

	ret = of_property_read_u32(node, "channel", channel);
	if (ret) {
		dev_err(adc_dev->dev,
			"invalid channel in node:%s\n", node->name);
		return ret;
	}
	if (*channel < AUXADC_CHAN_MIN || *channel > AUXADC_CHAN_MAX) {
		dev_err(adc_dev->dev,
			"invalid channel number %d in node:%s\n",
			*channel, node->name);
		return ret;
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

static int auxadc_parse_dt(struct mt635x_auxadc_device *adc_dev,
	struct device_node *node)
{
	const struct auxadc_channels *auxadc_chan;
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
		auxadc_chan = &auxadc_chans[channel];

		iio_chan->channel = channel;
		iio_chan->datasheet_name = auxadc_chan->ch_name;
		iio_chan->extend_name = auxadc_chan->ch_name;
		iio_chan->info_mask_separate = auxadc_chan->info_mask;
		iio_chan->type = auxadc_chan->type;
		iio_chan->indexed = 1;
		iio_chan->address = index++;

		iio_chan++;
	}

	return 0;
}

static int mt635x_auxadc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct mt635x_auxadc_device *adc_dev;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*adc_dev));
	if (!indio_dev)
		return -ENOMEM;

	adc_dev = iio_priv(indio_dev);
	adc_dev->dev = &pdev->dev;
	mutex_init(&adc_dev->lock);
	device_init_wakeup(&pdev->dev, true);

	ret = auxadc_parse_dt(adc_dev, node);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"auxadc_parse_dt fail, ret=%d\n", ret);
		return ret;
	}

	indio_dev->dev.parent = &pdev->dev;
	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->info = &mt635x_auxadc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = adc_dev->iio_chans;
	indio_dev->num_channels = adc_dev->nchannels;

	ret = iio_map_array_register(indio_dev, mt635x_auxadc_default_maps);
	if (ret) {
		dev_err(&pdev->dev, "failed to register IIO maps: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, indio_dev);

	ret = iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register iio device!\n");
		iio_map_array_unregister(indio_dev);
		return ret;
	}
	dev_info(&pdev->dev, "%s done\n", __func__);

	ret = pmic_auxadc_chip_init(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"pmic_auxadc_chip_init fail, ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int mt635x_auxadc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	iio_device_unregister(indio_dev);
	iio_map_array_unregister(indio_dev);

	return 0;
}

static const struct of_device_id mt635x_auxadc_of_match[] = {
	{ .compatible = "mediatek,mt635x-auxadc", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt635x_auxadc_of_match);

static struct platform_driver mt635x_auxadc_driver = {
	.driver = {
		.name = "mt635x-auxadc",
		.of_match_table = mt635x_auxadc_of_match,
	},
	.probe	= mt635x_auxadc_probe,
	.remove	= mt635x_auxadc_remove,
};
#if 1 /* internal version */
static int __init mt635x_auxadc_driver_init(void)
{
	return platform_driver_register(&mt635x_auxadc_driver);
}
fs_initcall(mt635x_auxadc_driver_init);
#else
module_platform_driver(mt635x_auxadc_driver);
#endif

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MediaTek PMIC AUXADC Driver for MT635x PMIC");
