/*
 * palmas-adc.c -- TI PALMAS GPADC.
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * Author: Pradeep Goudagunta <pgoudagunta@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/mfd/palmas.h>
#include <linux/completion.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>

#define MOD_NAME		"palmas-gpadc"
#define ADC_CONVERTION_TIMEOUT	(msecs_to_jiffies(5000))
#define TO_BE_CALCULATED	0
#define PRECISION_MULTIPLIER	1000000LL

struct palmas_gpadc_info {
/* calibration codes and regs */
	int x1;
	int x2;
	u8 trim1_reg;
	u8 trim2_reg;
	s64 gain;
	s64 offset;
	bool is_correct_code;
};

#define PALMAS_ADC_INFO(_chan, _x1, _x2, _t1, _t2, _is_correct_code)	\
[PALMAS_ADC_CH_##_chan] = {						\
		.x1 = _x1,						\
		.x2 = _x2,						\
		.gain = TO_BE_CALCULATED,				\
		.offset = TO_BE_CALCULATED,				\
		.trim1_reg = PALMAS_GPADC_TRIM##_t1,			\
		.trim2_reg = PALMAS_GPADC_TRIM##_t2,			\
		.is_correct_code = _is_correct_code			\
	}

static struct palmas_gpadc_info palmas_gpadc_info[] = {
	PALMAS_ADC_INFO(IN0, 2064, 3112, 1, 2, false),
	PALMAS_ADC_INFO(IN1, 2064, 3112, 1, 2, false),
	PALMAS_ADC_INFO(IN2, 2064, 3112, 3, 4, false),
	PALMAS_ADC_INFO(IN3, 2064, 3112, 1, 2, false),
	PALMAS_ADC_INFO(IN4, 2064, 3112, 1, 2, false),
	PALMAS_ADC_INFO(IN5, 2064, 3112, 1, 2, false),
	PALMAS_ADC_INFO(IN6, 2064, 3112, 5, 6, false),
	PALMAS_ADC_INFO(IN7, 2064, 3112, 7, 8, false),
	PALMAS_ADC_INFO(IN8, 2064, 3112, 9, 10, false),
	PALMAS_ADC_INFO(IN9, 2064, 3112, 11, 12, false),
	PALMAS_ADC_INFO(IN10, 2064, 3112, 13, 14, false),
	PALMAS_ADC_INFO(IN11, 0, 0, INVALID, INVALID, true),
	PALMAS_ADC_INFO(IN12, 0, 0, INVALID, INVALID, true),
	PALMAS_ADC_INFO(IN13, 0, 0, INVALID, INVALID, true),
	PALMAS_ADC_INFO(IN14, 2064, 3112, 15, 16, false),
	PALMAS_ADC_INFO(IN15, 0, 0, INVALID, INVALID, true),
};

struct palmas_gpadc {
	struct device			*dev;
	struct palmas			*palmas;
	u8				ch0_current;
	u8				ch3_current;
	bool				ch3_dual_current;
	bool				extended_delay;
	int				irq;
	int				irq_auto_0;
	int				irq_auto_1;
	struct palmas_gpadc_info	*adc_info;
	struct completion		conv_completion;
	struct palmas_adc_wakeup_property wakeup1_data;
	struct palmas_adc_wakeup_property wakeup2_data;
	bool				wakeup1_enable;
	bool				wakeup2_enable;
	int				auto_conversion_period;
};

/*
 * GPADC lock issue in AUTO mode.
 * Impact: In AUTO mode, GPADC conversion can be locked after disabling AUTO
 *	   mode feature.
 * Details:
 *	When the AUTO mode is the only conversion mode enabled, if the AUTO
 *	mode feature is disabled with bit GPADC_AUTO_CTRL.  AUTO_CONV1_EN = 0
 *	or bit GPADC_AUTO_CTRL.  AUTO_CONV0_EN = 0 during a conversion, the
 *	conversion mechanism can be seen as locked meaning that all following
 *	conversion will give 0 as a result.  Bit GPADC_STATUS.GPADC_AVAILABLE
 *	will stay at 0 meaning that GPADC is busy.  An RT conversion can unlock
 *	the GPADC.
 *
 * Workaround(s):
 *	To avoid the lock mechanism, the workaround to follow before any stop
 *	conversion request is:
 *	Force the GPADC state machine to be ON by using the GPADC_CTRL1.
 *		GPADC_FORCE bit = 1
 *	Shutdown the GPADC AUTO conversion using
 *		GPADC_AUTO_CTRL.SHUTDOWN_CONV[01] = 0.
 *	After 100us, force the GPADC state machine to be OFF by using the
 *		GPADC_CTRL1.  GPADC_FORCE bit = 0
 */
static int palmas_disable_auto_conversion(struct palmas_gpadc *adc)
{
	int ret;

	ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_CTRL1,
			PALMAS_GPADC_CTRL1_GPADC_FORCE,
			PALMAS_GPADC_CTRL1_GPADC_FORCE);
	if (ret < 0) {
		dev_err(adc->dev, "GPADC_CTRL1 update failed: %d\n", ret);
		return ret;
	}

	ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_AUTO_CTRL,
			PALMAS_GPADC_AUTO_CTRL_SHUTDOWN_CONV1 |
			PALMAS_GPADC_AUTO_CTRL_SHUTDOWN_CONV0,
			0);
	if (ret < 0) {
		dev_err(adc->dev, "AUTO_CTRL update failed: %d\n", ret);
		return ret;
	}

	udelay(100);

	ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_CTRL1,
			PALMAS_GPADC_CTRL1_GPADC_FORCE, 0);
	if (ret < 0) {
		dev_err(adc->dev, "GPADC_CTRL1 update failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static irqreturn_t palmas_gpadc_irq(int irq, void *data)
{
	struct palmas_gpadc *adc = data;

	complete(&adc->conv_completion);
	return IRQ_HANDLED;
}

static irqreturn_t palmas_gpadc_irq_auto(int irq, void *data)
{
	struct palmas_gpadc *adc = data;

	dev_info(adc->dev, "Threshold interrupt %d occurs\n", irq);
	palmas_disable_auto_conversion(adc);
	return IRQ_HANDLED;
}

static int palmas_gpadc_start_mask_interrupt(struct palmas_gpadc *adc, int mask)
{
	int ret;

	if (!mask)
		ret = palmas_update_bits(adc->palmas, PALMAS_INTERRUPT_BASE,
					PALMAS_INT3_MASK,
					PALMAS_INT3_MASK_GPADC_EOC_SW, 0);
	else
		ret = palmas_update_bits(adc->palmas, PALMAS_INTERRUPT_BASE,
					PALMAS_INT3_MASK,
					PALMAS_INT3_MASK_GPADC_EOC_SW,
					PALMAS_INT3_MASK_GPADC_EOC_SW);
	if (ret < 0)
		dev_err(adc->dev, "GPADC INT MASK update failed: %d\n", ret);

	return ret;
}

static int palmas_gpadc_enable(struct palmas_gpadc *adc, int adc_chan,
			       int enable)
{
	unsigned int mask, val;
	int ret;

	if (enable) {
		val = (adc->extended_delay
			<< PALMAS_GPADC_RT_CTRL_EXTEND_DELAY_SHIFT);
		ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
					PALMAS_GPADC_RT_CTRL,
					PALMAS_GPADC_RT_CTRL_EXTEND_DELAY, val);
		if (ret < 0) {
			dev_err(adc->dev, "RT_CTRL update failed: %d\n", ret);
			return ret;
		}

		mask = (PALMAS_GPADC_CTRL1_CURRENT_SRC_CH0_MASK |
			PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_MASK |
			PALMAS_GPADC_CTRL1_GPADC_FORCE);
		val = (adc->ch0_current
			<< PALMAS_GPADC_CTRL1_CURRENT_SRC_CH0_SHIFT);
		val |= (adc->ch3_current
			<< PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_SHIFT);
		val |= PALMAS_GPADC_CTRL1_GPADC_FORCE;
		ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_CTRL1, mask, val);
		if (ret < 0) {
			dev_err(adc->dev,
				"Failed to update current setting: %d\n", ret);
			return ret;
		}

		mask = (PALMAS_GPADC_SW_SELECT_SW_CONV0_SEL_MASK |
			PALMAS_GPADC_SW_SELECT_SW_CONV_EN);
		val = (adc_chan | PALMAS_GPADC_SW_SELECT_SW_CONV_EN);
		ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_SW_SELECT, mask, val);
		if (ret < 0) {
			dev_err(adc->dev, "SW_SELECT update failed: %d\n", ret);
			return ret;
		}
	} else {
		ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_SW_SELECT, 0);
		if (ret < 0)
			dev_err(adc->dev, "SW_SELECT write failed: %d\n", ret);

		ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_CTRL1,
				PALMAS_GPADC_CTRL1_GPADC_FORCE, 0);
		if (ret < 0) {
			dev_err(adc->dev, "CTRL1 update failed: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int palmas_gpadc_read_prepare(struct palmas_gpadc *adc, int adc_chan)
{
	int ret;

	ret = palmas_gpadc_enable(adc, adc_chan, true);
	if (ret < 0)
		return ret;

	return palmas_gpadc_start_mask_interrupt(adc, 0);
}

static void palmas_gpadc_read_done(struct palmas_gpadc *adc, int adc_chan)
{
	palmas_gpadc_start_mask_interrupt(adc, 1);
	palmas_gpadc_enable(adc, adc_chan, false);
}

static int palmas_gpadc_calibrate(struct palmas_gpadc *adc, int adc_chan)
{
	s64 k;
	int d1;
	int d2;
	int ret;
	int x1 =  adc->adc_info[adc_chan].x1;
	int x2 =  adc->adc_info[adc_chan].x2;

	ret = palmas_read(adc->palmas, PALMAS_TRIM_GPADC_BASE,
				adc->adc_info[adc_chan].trim1_reg, &d1);
	if (ret < 0) {
		dev_err(adc->dev, "TRIM read failed: %d\n", ret);
		goto scrub;
	}

	ret = palmas_read(adc->palmas, PALMAS_TRIM_GPADC_BASE,
				adc->adc_info[adc_chan].trim2_reg, &d2);
	if (ret < 0) {
		dev_err(adc->dev, "TRIM read failed: %d\n", ret);
		goto scrub;
	}

	/* Gain Calculation */
	k = PRECISION_MULTIPLIER;
	k += div64_s64(PRECISION_MULTIPLIER * (d2 - d1), x2 - x1);
	adc->adc_info[adc_chan].gain = k;

	/* Offset Calculation */
	adc->adc_info[adc_chan].offset = (d1 * PRECISION_MULTIPLIER);
	adc->adc_info[adc_chan].offset -= ((k - PRECISION_MULTIPLIER) * x1);

scrub:
	return ret;
}

static int palmas_gpadc_start_convertion(struct palmas_gpadc *adc, int adc_chan)
{
	unsigned int val;
	int ret;

	INIT_COMPLETION(adc->conv_completion);
	ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_SW_SELECT,
				PALMAS_GPADC_SW_SELECT_SW_START_CONV0,
				PALMAS_GPADC_SW_SELECT_SW_START_CONV0);
	if (ret < 0) {
		dev_err(adc->dev, "ADC_SW_START write failed: %d\n", ret);
		return ret;
	}

	ret = wait_for_completion_timeout(&adc->conv_completion,
				ADC_CONVERTION_TIMEOUT);
	if (ret == 0) {
		dev_err(adc->dev, "ADC conversion not completed\n");
		ret = -ETIMEDOUT;
		return ret;
	}

	ret = palmas_bulk_read(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_SW_CONV0_LSB, &val, 2);
	if (ret < 0) {
		dev_err(adc->dev, "ADCDATA read failed: %d\n", ret);
		return ret;
	}

	ret = (val & 0xFFF);
	return ret;
}

static int palmas_gpadc_get_calibrated_code(struct palmas_gpadc *adc,
						int adc_chan, int val)
{
	s64 code = val * PRECISION_MULTIPLIER;

	if ((code - adc->adc_info[adc_chan].offset) < 0) {
		dev_err(adc->dev, "No Input Connected\n");
		return 0;
	}

	if (!(adc->adc_info[adc_chan].is_correct_code)) {
		code -= adc->adc_info[adc_chan].offset;
		code = div_s64(code, adc->adc_info[adc_chan].gain);
		return code;
	}

	return val;
}

static int palmas_gpadc_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct  palmas_gpadc *adc = iio_priv(indio_dev);
	int adc_chan = chan->channel;
	int ret = 0;

	if (adc_chan > PALMAS_ADC_CH_MAX)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
	case IIO_CHAN_INFO_PROCESSED:
		ret = palmas_gpadc_read_prepare(adc, adc_chan);
		if (ret < 0)
			goto out;

		ret = palmas_gpadc_start_convertion(adc, adc_chan);
		if (ret < 0) {
			dev_err(adc->dev,
			"ADC start coversion failed\n");
			goto out;
		}

		if (mask == IIO_CHAN_INFO_PROCESSED)
			ret = palmas_gpadc_get_calibrated_code(
							adc, adc_chan, ret);

		*val = ret;

		ret = IIO_VAL_INT;
		goto out;

	case IIO_CHAN_INFO_RAW_DUAL:
	case IIO_CHAN_INFO_PROCESSED_DUAL:
		ret = palmas_gpadc_read_prepare(adc, adc_chan);
		if (ret < 0)
			goto out;

		ret = palmas_gpadc_start_convertion(adc, adc_chan);
		if (ret < 0) {
			dev_err(adc->dev,
				"ADC start coversion failed\n");
			goto out;
		}

		if (mask == IIO_CHAN_INFO_PROCESSED_DUAL)
			ret = palmas_gpadc_get_calibrated_code(
							adc, adc_chan, ret);

		*val = ret;

		if ((adc_chan == PALMAS_ADC_CH_IN3)
				&& adc->ch3_dual_current && val2) {
			unsigned int reg_mask, reg_val;

			reg_mask = PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_MASK;
			reg_val = ((adc->ch3_current + 1)
				<< PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_SHIFT);
			ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
						PALMAS_GPADC_CTRL1,
						reg_mask, reg_val);
			if (ret < 0) {
				dev_err(adc->dev, "CTRL1 update failed\n");
				goto out;
			}

			ret = palmas_gpadc_start_convertion(adc, adc_chan);
			if (ret < 0) {
				dev_err(adc->dev,
					"ADC start coversion failed\n");
				goto out;
			}

			if (mask == IIO_CHAN_INFO_PROCESSED_DUAL)
				ret = palmas_gpadc_get_calibrated_code(
							adc, adc_chan, ret);

			*val2 = ret;
		}

		ret = IIO_VAL_INT;
		goto out;
	}

	mutex_unlock(&indio_dev->mlock);
	return ret;

out:
	palmas_gpadc_read_done(adc, adc_chan);
	mutex_unlock(&indio_dev->mlock);
	return ret;
}

static const struct iio_info palmas_gpadc_iio_info = {
	.read_raw = palmas_gpadc_read_raw,
	.driver_module = THIS_MODULE,
};

#define PALMAS_ADC_CHAN_IIO(chan)					\
{									\
	.datasheet_name = PALMAS_DATASHEET_NAME(chan),			\
	.type = IIO_VOLTAGE, 						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			\
			BIT(IIO_CHAN_INFO_CALIBSCALE),			\
	.indexed = 1,							\
	.channel = PALMAS_ADC_CH_##chan,				\
}

#define PALMAS_ADC_CHAN_DUAL_IIO(chan)					\
{									\
	.datasheet_name = PALMAS_DATASHEET_NAME(chan),			\
	.type = IIO_VOLTAGE,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			\
			BIT(IIO_CHAN_INFO_PROCESSED) |			\
			BIT(IIO_CHAN_INFO_RAW_DUAL) |			\
			BIT(IIO_CHAN_INFO_PROCESSED_DUAL),		\
	.indexed = 1,							\
	.channel = PALMAS_ADC_CH_##chan,				\
}

static const struct iio_chan_spec palmas_gpadc_iio_channel[] = {
	PALMAS_ADC_CHAN_IIO(IN0),
	PALMAS_ADC_CHAN_IIO(IN1),
	PALMAS_ADC_CHAN_IIO(IN2),
	PALMAS_ADC_CHAN_DUAL_IIO(IN3),
	PALMAS_ADC_CHAN_IIO(IN4),
	PALMAS_ADC_CHAN_IIO(IN5),
	PALMAS_ADC_CHAN_IIO(IN6),
	PALMAS_ADC_CHAN_IIO(IN7),
	PALMAS_ADC_CHAN_IIO(IN8),
	PALMAS_ADC_CHAN_IIO(IN9),
	PALMAS_ADC_CHAN_IIO(IN10),
	PALMAS_ADC_CHAN_IIO(IN11),
	PALMAS_ADC_CHAN_IIO(IN12),
	PALMAS_ADC_CHAN_IIO(IN13),
	PALMAS_ADC_CHAN_IIO(IN14),
	PALMAS_ADC_CHAN_IIO(IN15),
};

static int palmas_gpadc_get_adc_dt_data(struct platform_device *pdev,
	struct palmas_gpadc_platform_data **gpadc_pdata)
{
	struct device_node *np = pdev->dev.of_node;
	struct palmas_gpadc_platform_data *gp_data;
	struct device_node *map_node;
	struct device_node *child;
	struct iio_map *palmas_iio_map;
	int ret;
	u32 pval;
	int nmap, nvalid_map;

	gp_data = devm_kzalloc(&pdev->dev, sizeof(*gp_data), GFP_KERNEL);
	if (!gp_data)
		return -ENOMEM;

	ret = of_property_read_u32(np, "ti,channel0-current-microamp", &pval);
	if (!ret)
		gp_data->ch0_current = pval;

	ret = of_property_read_u32(np, "ti,channel3-current-microamp", &pval);
	if (!ret)
		gp_data->ch3_current = pval;

	gp_data->ch3_dual_current = of_property_read_bool(np,
					"ti,enable-channel3-dual-current");

	gp_data->extended_delay = of_property_read_bool(np,
					"ti,enable-extended-delay");

	map_node = of_get_child_by_name(np, "iio_map");
	if (!map_node) {
		dev_warn(&pdev->dev, "IIO map table not found\n");
		goto done;
	}

	nmap = of_get_child_count(map_node);
	if (!nmap)
		goto done;

	nmap++;
	palmas_iio_map = devm_kzalloc(&pdev->dev,
				sizeof(*palmas_iio_map) * nmap, GFP_KERNEL);
	if (!palmas_iio_map)
		goto done;

	nvalid_map = 0;
	for_each_child_of_node(map_node, child) {
		ret = of_property_read_u32(child, "ti,adc-channel-number",
					&pval);
		if (!ret && pval < ARRAY_SIZE(palmas_gpadc_iio_channel))
			palmas_iio_map[nvalid_map].adc_channel_label =
				palmas_gpadc_iio_channel[pval].datasheet_name;
		of_property_read_string(child, "ti,adc-consumer-device",
				&palmas_iio_map[nvalid_map].consumer_dev_name);
		of_property_read_string(child, "ti,adc-consumer-channel",
				&palmas_iio_map[nvalid_map].consumer_channel);
		dev_dbg(&pdev->dev,
			"Channel %s consumer dev %s and consumer channel %s\n",
				palmas_iio_map[nvalid_map].adc_channel_label,
				palmas_iio_map[nvalid_map].consumer_dev_name,
				palmas_iio_map[nvalid_map].consumer_channel);
		nvalid_map++;
	}
	palmas_iio_map[nvalid_map].adc_channel_label = NULL;
	palmas_iio_map[nvalid_map].consumer_dev_name = NULL;
	palmas_iio_map[nvalid_map].consumer_channel = NULL;

	gp_data->iio_maps = palmas_iio_map;

done:
	*gpadc_pdata = gp_data;
	return 0;
}

static int palmas_gpadc_probe(struct platform_device *pdev)
{
	struct palmas_gpadc *adc;
	struct palmas_platform_data *pdata;
	struct palmas_gpadc_platform_data *gpadc_pdata = NULL;
	struct iio_dev *iodev;
	int ret, i;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (pdata && pdata->gpadc_pdata)
		gpadc_pdata = pdata->gpadc_pdata;

	if (!gpadc_pdata && pdev->dev.of_node) {
		ret = palmas_gpadc_get_adc_dt_data(pdev, &gpadc_pdata);
		if (ret < 0)
			return ret;
	}
	if (!gpadc_pdata) {
		dev_err(&pdev->dev, "No platform data\n");
		return -ENODEV;
	}

	iodev = iio_device_alloc(sizeof(*adc));
	if (!iodev) {
		dev_err(&pdev->dev, "iio_device_alloc failed\n");
		return -ENOMEM;
	}

	if (gpadc_pdata->iio_maps) {
		ret = iio_map_array_register(iodev, gpadc_pdata->iio_maps);
		if (ret < 0) {
			dev_err(&pdev->dev, "iio_map_array_register failed\n");
			goto out;
		}
	}

	adc = iio_priv(iodev);
	adc->dev = &pdev->dev;
	adc->palmas = dev_get_drvdata(pdev->dev.parent);
	adc->adc_info = palmas_gpadc_info;
	init_completion(&adc->conv_completion);
	dev_set_drvdata(&pdev->dev, iodev);

	adc->auto_conversion_period = gpadc_pdata->auto_conversion_period_ms;
	adc->irq = palmas_irq_get_virq(adc->palmas, PALMAS_GPADC_EOC_SW_IRQ);
	ret = request_threaded_irq(adc->irq, NULL,
		palmas_gpadc_irq,
		IRQF_ONESHOT | IRQF_EARLY_RESUME, dev_name(adc->dev),
		adc);
	if (ret < 0) {
		dev_err(adc->dev,
			"request irq %d failed: %dn", adc->irq, ret);
		goto out_unregister_map;
	}

	if (gpadc_pdata->adc_wakeup1_data) {
		memcpy(&adc->wakeup1_data, gpadc_pdata->adc_wakeup1_data,
			sizeof(adc->wakeup1_data));
		adc->wakeup1_enable = true;
		adc->irq_auto_0 =  platform_get_irq(pdev, 1);
		ret = request_threaded_irq(adc->irq_auto_0, NULL,
				palmas_gpadc_irq_auto,
				IRQF_ONESHOT | IRQF_EARLY_RESUME,
				"palmas-adc-auto-0", adc);
		if (ret < 0) {
			dev_err(adc->dev, "request auto0 irq %d failed: %dn",
				adc->irq_auto_0, ret);
			goto out_irq_free;
		}
	}

	if (gpadc_pdata->adc_wakeup2_data) {
		memcpy(&adc->wakeup2_data, gpadc_pdata->adc_wakeup2_data,
				sizeof(adc->wakeup2_data));
		adc->wakeup2_enable = true;
		adc->irq_auto_1 =  platform_get_irq(pdev, 2);
		ret = request_threaded_irq(adc->irq_auto_1, NULL,
				palmas_gpadc_irq_auto,
				IRQF_ONESHOT | IRQF_EARLY_RESUME,
				"palmas-adc-auto-1", adc);
		if (ret < 0) {
			dev_err(adc->dev, "request auto1 irq %d failed: %dn",
				adc->irq_auto_1, ret);
			goto out_irq_auto0_free;
		}
	}

	if (gpadc_pdata->ch0_current == 0)
		adc->ch0_current = PALMAS_ADC_CH0_CURRENT_SRC_0;
	else if (gpadc_pdata->ch0_current <= 5)
		adc->ch0_current = PALMAS_ADC_CH0_CURRENT_SRC_5;
	else if (gpadc_pdata->ch0_current <= 15)
		adc->ch0_current = PALMAS_ADC_CH0_CURRENT_SRC_15;
	else
		adc->ch0_current = PALMAS_ADC_CH0_CURRENT_SRC_20;

	if (gpadc_pdata->ch3_current == 0)
		adc->ch3_current = PALMAS_ADC_CH3_CURRENT_SRC_0;
	else if (gpadc_pdata->ch3_current <= 10)
		adc->ch3_current = PALMAS_ADC_CH3_CURRENT_SRC_10;
	else if (gpadc_pdata->ch3_current <= 400)
		adc->ch3_current = PALMAS_ADC_CH3_CURRENT_SRC_400;
	else
		adc->ch3_current = PALMAS_ADC_CH3_CURRENT_SRC_800;

	/* If ch3_dual_current is true, it will measure ch3 input signal with
	 * ch3_current and the next current of ch3_current. */
	adc->ch3_dual_current = gpadc_pdata->ch3_dual_current;
	if (adc->ch3_dual_current &&
			(adc->ch3_current == PALMAS_ADC_CH3_CURRENT_SRC_800)) {
		dev_warn(adc->dev,
			"Disable ch3_dual_current by wrong current setting\n");
		adc->ch3_dual_current = false;
	}

	adc->extended_delay = gpadc_pdata->extended_delay;

	iodev->name = MOD_NAME;
	iodev->dev.parent = &pdev->dev;
	iodev->info = &palmas_gpadc_iio_info;
	iodev->modes = INDIO_DIRECT_MODE;
	iodev->channels = palmas_gpadc_iio_channel;
	iodev->num_channels = ARRAY_SIZE(palmas_gpadc_iio_channel);

	ret = iio_device_register(iodev);
	if (ret < 0) {
		dev_err(adc->dev, "iio_device_register() failed: %d\n", ret);
		goto out_irq_auto1_free;
	}

	device_set_wakeup_capable(&pdev->dev, 1);
	for (i = 0; i < PALMAS_ADC_CH_MAX; i++) {
		if (!(adc->adc_info[i].is_correct_code))
			palmas_gpadc_calibrate(adc, i);
	}

	if (adc->wakeup1_enable || adc->wakeup2_enable)
		device_wakeup_enable(&pdev->dev);
	return 0;

out_irq_auto1_free:
	if (gpadc_pdata->adc_wakeup2_data)
		free_irq(adc->irq_auto_1, adc);
out_irq_auto0_free:
	if (gpadc_pdata->adc_wakeup1_data)
		free_irq(adc->irq_auto_0, adc);
out_irq_free:
	free_irq(adc->irq, adc);
out_unregister_map:
	if (gpadc_pdata->iio_maps)
		iio_map_array_unregister(iodev);
out:
	iio_device_free(iodev);
	return ret;
}

static int palmas_gpadc_remove(struct platform_device *pdev)
{
	struct iio_dev *iodev = dev_to_iio_dev(&pdev->dev);
	struct palmas_gpadc *adc = iio_priv(iodev);
	struct palmas_platform_data *pdata = dev_get_platdata(pdev->dev.parent);
	if (pdata->gpadc_pdata->iio_maps)
		iio_map_array_unregister(iodev);
	iio_device_unregister(iodev);
	free_irq(adc->irq, adc);
	if (adc->wakeup1_enable)
		free_irq(adc->irq_auto_0, adc);
	if (adc->wakeup2_enable)
		free_irq(adc->irq_auto_1, adc);
	iio_device_free(iodev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int palmas_adc_wakeup_configure(struct palmas_gpadc *adc)
{
	int adc_period, conv;
	int i;
	int ch0 = 0, ch1 = 0;
	int thres;
	int ret;

	adc_period = adc->auto_conversion_period;
	for (i = 0; i < 16; ++i) {
		if (((1000 * (1 << i))/32) < adc_period)
			continue;
	}
	if (i > 0)
		i--;
	adc_period = i;
	ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_AUTO_CTRL,
			PALMAS_GPADC_AUTO_CTRL_COUNTER_CONV_MASK,
			adc_period);
	if (ret < 0) {
		dev_err(adc->dev, "AUTO_CTRL write failed: %d\n", ret);
		return ret;
	}

	conv = 0;
	if (adc->wakeup1_enable) {
		int is_high;

		ch0 = adc->wakeup1_data.adc_channel_number;
		conv |= PALMAS_GPADC_AUTO_CTRL_AUTO_CONV0_EN;
		if (adc->wakeup1_data.adc_high_threshold > 0) {
			thres = adc->wakeup1_data.adc_high_threshold;
			is_high = 0;
		} else {
			thres = adc->wakeup1_data.adc_low_threshold;
			is_high = BIT(7);
		}

		ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_THRES_CONV0_LSB, thres & 0xFF);
		if (ret < 0) {
			dev_err(adc->dev,
				"THRES_CONV0_LSB write failed: %d\n", ret);
			return ret;
		}

		ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_THRES_CONV0_MSB,
				((thres >> 8) & 0xF) | is_high);
		if (ret < 0) {
			dev_err(adc->dev,
				"THRES_CONV0_MSB write failed: %d\n", ret);
			return ret;
		}
	}

	if (adc->wakeup2_enable) {
		int is_high;

		ch1 = adc->wakeup2_data.adc_channel_number;
		conv |= PALMAS_GPADC_AUTO_CTRL_AUTO_CONV1_EN;
		if (adc->wakeup2_data.adc_high_threshold > 0) {
			thres = adc->wakeup2_data.adc_high_threshold;
			is_high = 0;
		} else {
			thres = adc->wakeup2_data.adc_low_threshold;
			is_high = BIT(7);
		}

		ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_THRES_CONV1_LSB, thres & 0xFF);
		if (ret < 0) {
			dev_err(adc->dev,
				"THRES_CONV1_LSB write failed: %d\n", ret);
			return ret;
		}

		ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_THRES_CONV1_MSB,
				((thres >> 8) & 0xF) | is_high);
		if (ret < 0) {
			dev_err(adc->dev,
				"THRES_CONV1_MSB write failed: %d\n", ret);
			return ret;
		}
	}

	ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_AUTO_SELECT, (ch1 << 4) | ch0);
	if (ret < 0) {
		dev_err(adc->dev, "AUTO_SELECT write failed: %d\n", ret);
		return ret;
	}

	ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_AUTO_CTRL,
			PALMAS_GPADC_AUTO_CTRL_AUTO_CONV1_EN |
			PALMAS_GPADC_AUTO_CTRL_AUTO_CONV0_EN, conv);
	if (ret < 0) {
		dev_err(adc->dev, "AUTO_CTRL write failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static int palmas_adc_wakeup_reset(struct palmas_gpadc *adc)
{
	int ret;

	ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_AUTO_SELECT, 0);
	if (ret < 0) {
		dev_err(adc->dev, "AUTO_SELECT write failed: %d\n", ret);
		return ret;
	}

	ret = palmas_disable_auto_conversion(adc);
	if (ret < 0) {
		dev_err(adc->dev, "Disable auto conversion failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static int palmas_gpadc_suspend(struct device *dev)
{
	struct iio_dev *iodev = dev_to_iio_dev(dev);
	struct palmas_gpadc *adc = iio_priv(iodev);
	int wakeup = adc->wakeup1_enable || adc->wakeup2_enable;
	int ret;

	if (!device_may_wakeup(dev) || !wakeup)
		return 0;

	ret = palmas_adc_wakeup_configure(adc);
	if (ret < 0)
		return ret;

	if (adc->wakeup1_enable)
		enable_irq_wake(adc->irq_auto_0);

	if (adc->wakeup2_enable)
		enable_irq_wake(adc->irq_auto_1);
	return 0;
}

static int palmas_gpadc_resume(struct device *dev)
{
	struct iio_dev *iodev = dev_to_iio_dev(dev);
	struct palmas_gpadc *adc = iio_priv(iodev);
	int wakeup = adc->wakeup1_enable || adc->wakeup2_enable;
	int ret;

	if (!device_may_wakeup(dev) || !wakeup)
		return 0;

	ret = palmas_adc_wakeup_reset(adc);
	if (ret < 0)
		return ret;

	if (adc->wakeup1_enable)
		disable_irq_wake(adc->irq_auto_0);

	if (adc->wakeup2_enable)
		disable_irq_wake(adc->irq_auto_1);

	return 0;
};
#endif

static const struct dev_pm_ops palmas_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(palmas_gpadc_suspend,
				palmas_gpadc_resume)
};

static struct of_device_id of_palmas_gpadc_match_tbl[] = {
	{ .compatible = "ti,palmas-gpadc", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_palmas_gpadc_match_tbl);

static struct platform_driver palmas_gpadc_driver = {
	.probe = palmas_gpadc_probe,
	.remove = palmas_gpadc_remove,
	.driver = {
		.name = MOD_NAME,
		.owner = THIS_MODULE,
		.pm = &palmas_pm_ops,
		.of_match_table = of_palmas_gpadc_match_tbl,
	},
};

static int __init palmas_gpadc_init(void)
{
	return platform_driver_register(&palmas_gpadc_driver);
}
module_init(palmas_gpadc_init);

static void __exit palmas_gpadc_exit(void)
{
	platform_driver_unregister(&palmas_gpadc_driver);
}
module_exit(palmas_gpadc_exit);

MODULE_DESCRIPTION("palmas GPADC driver");
MODULE_AUTHOR("Pradeep Goudagunta<pgoudagunta@nvidia.com>");
MODULE_ALIAS("platform:palmas-gpadc");
MODULE_LICENSE("GPL v2");
