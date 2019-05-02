// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/bitops.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/log2.h>
#include <linux/qpnp/qpnp-revid.h>

#include <dt-bindings/iio/qcom,spmi-vadc.h>

#include "qcom-vadc-common.h"

#define ADC_USR_STATUS1				0x8
#define ADC_USR_STATUS1_REQ_STS			BIT(1)
#define ADC_USR_STATUS1_EOC			BIT(0)
#define ADC_USR_STATUS1_REQ_STS_EOC_MASK	0x3

#define ADC_USR_STATUS2				0x9
#define ADC_USR_STATUS2_CONV_SEQ_MASK		0x70
#define ADC_USR_STATUS2_CONV_SEQ_MASK_SHIFT	0x5

#define ADC_USR_IBAT_MEAS			0xf
#define ADC_USR_IBAT_MEAS_SUPPORTED		BIT(0)

#define ADC_USR_DIG_PARAM			0x42
#define ADC_USR_DIG_PARAM_CAL_VAL		BIT(6)
#define ADC_USR_DIG_PARAM_CAL_VAL_SHIFT		6
#define ADC_USR_DIG_PARAM_CAL_SEL		0x30
#define ADC_USR_DIG_PARAM_CAL_SEL_SHIFT		4
#define ADC_USR_DIG_PARAM_DEC_RATIO_SEL		0xc
#define ADC_USR_DIG_PARAM_DEC_RATIO_SEL_SHIFT	2

#define ADC_USR_DIG_PARAM_ABS_CAL_VAL		0x28

#define ADC_USR_FAST_AVG_CTL			0x43
#define ADC_USR_FAST_AVG_CTL_EN			BIT(7)
#define ADC_USR_FAST_AVG_CTL_SAMPLES_MASK	0x7

#define ADC_USR_CH_SEL_CTL			0x44

#define ADC_USR_DELAY_CTL			0x45
#define ADC_USR_HW_SETTLE_DELAY_MASK		0xf

#define ADC_USR_EN_CTL1				0x46
#define ADC_USR_EN_CTL1_ADC_EN			BIT(7)

#define ADC_USR_CONV_REQ			0x47
#define ADC_USR_CONV_REQ_REQ			BIT(7)

#define ADC_USR_DATA0				0x50

#define ADC_USR_DATA1				0x51

#define ADC_USR_IBAT_DATA0			0x52

#define ADC_USR_IBAT_DATA1			0x53

#define ADC_CHAN_MIN				ADC_USBIN
#define ADC_CHAN_MAX				ADC_LR_MUX3_BUF_PU1_PU2_XO_THERM

/*
 * Conversion time varies between 139uS to 6827uS based on the decimation,
 * clock rate, fast average samples with no measurement in queue.
 * Set the timeout to a max of 100ms.
 */
#define ADC_CONV_TIME_MIN_US			263
#define ADC_CONV_TIME_MAX_US			264
#define ADC_CONV_TIME_RETRY			400
#define ADC_CONV_TIMEOUT			msecs_to_jiffies(100)

/* CAL peripheral */
#define ADC_CAL_DELAY_CTL			0x44
#define ADC_CAL_DELAY_CTL_VAL_256S		0x73
#define ADC_CAL_DELAY_CTL_VAL_125MS		0x3

enum adc_cal_method {
	ADC_NO_CAL = 0,
	ADC_RATIOMETRIC_CAL,
	ADC_ABSOLUTE_CAL
};

enum adc_cal_val {
	ADC_TIMER_CAL = 0,
	ADC_NEW_CAL
};

struct pmic_rev_data {
	int subtype;
	int rev4;
};

/**
 * struct adc_channel_prop - ADC channel property.
 * @channel: channel number, refer to the channel list.
 * @cal_method: calibration method.
 * @cal_val: calibration value
 * @decimation: sampling rate supported for the channel.
 * @prescale: channel scaling performed on the input signal.
 * @hw_settle_time: the time between AMUX being configured and the
 *	start of conversion.
 * @avg_samples: ability to provide single result from the ADC
 *	that is an average of multiple measurements.
 * @scale_fn_type: Represents the scaling function to convert voltage
 *	physical units desired by the client for the channel.
 */
struct adc_channel_prop {
	unsigned int			channel;
	enum adc_cal_method		cal_method;
	enum adc_cal_val		cal_val;
	unsigned int			decimation;
	unsigned int			prescale;
	unsigned int			hw_settle_time;
	unsigned int			avg_samples;
	/*lut_index is used only for bat_therm LUTs*/
	unsigned int			lut_index;
	enum vadc_scale_fn_type		scale_fn_type;
	const char			*datasheet_name;
};

/**
 * struct adc_chip - ADC private structure.
 * @regmap: pointer to struct regmap.
 * @dev: pointer to struct device.
 * @base: base address for the ADC peripheral.
 * @cal_addr: base address for the CAL peripheral.
 * @nchannels: number of ADC channels.
 * @chan_props: array of ADC channel properties.
 * @iio_chans: array of IIO channels specification.
 * @poll_eoc: use polling instead of interrupt.
 * @complete: ADC result notification after interrupt is received.
 * @lock: ADC lock for access to the peripheral.
 * @data: software configuration data.
 */
struct adc_chip {
	struct regmap		*regmap;
	struct device		*dev;
	u16			base;
	u16			cal_addr;
	unsigned int		nchannels;
	struct adc_channel_prop	*chan_props;
	struct iio_chan_spec	*iio_chans;
	bool			poll_eoc;
	struct completion	complete;
	struct mutex		lock;
	bool			skip_usb_wa;
	struct pmic_revid_data	*pmic_rev_id;
	const struct adc_data	*data;
};

static const struct vadc_prescale_ratio adc_prescale_ratios[] = {
	{.num =  1, .den =  1},
	{.num =  1, .den =  3},
	{.num =  1, .den =  4},
	{.num =  1, .den =  6},
	{.num =  1, .den = 20},
	{.num =  1, .den =  8},
	{.num = 10, .den = 81},
	{.num =  1, .den = 10},
	{.num =  1, .den = 16}
};

static int adc_read(struct adc_chip *adc, u16 offset, u8 *data, int len)
{
	return regmap_bulk_read(adc->regmap, adc->base + offset, data, len);
}

static int adc_write(struct adc_chip *adc, u16 offset, u8 *data, int len)
{
	return regmap_bulk_write(adc->regmap, adc->base + offset, data, len);
}

static int adc_prescaling_from_dt(u32 num, u32 den)
{
	unsigned int pre;

	for (pre = 0; pre < ARRAY_SIZE(adc_prescale_ratios); pre++)
		if (adc_prescale_ratios[pre].num == num &&
		    adc_prescale_ratios[pre].den == den)
			break;

	if (pre == ARRAY_SIZE(adc_prescale_ratios))
		return -EINVAL;

	return pre;
}

static int adc_hw_settle_time_from_dt(u32 value,
					const unsigned int *hw_settle)
{
	uint32_t i;

	for (i = 0; i < VADC_HW_SETTLE_SAMPLES_MAX; i++) {
		if (value == hw_settle[i])
			return i;
	}

	return -EINVAL;
}

static int adc_avg_samples_from_dt(u32 value)
{
	if (!is_power_of_2(value) || value > ADC5_AVG_SAMPLES_MAX)
		return -EINVAL;

	return __ffs64(value);
}

static int adc_read_current_data(struct adc_chip *adc, u16 *data)
{
	int ret;
	u8 rslt_lsb = 0, rslt_msb = 0;

	ret = adc_read(adc, ADC_USR_IBAT_DATA0, &rslt_lsb, 1);
	if (ret)
		return ret;

	ret = adc_read(adc, ADC_USR_IBAT_DATA1, &rslt_msb, 1);
	if (ret)
		return ret;

	*data = (rslt_msb << 8) | rslt_lsb;

	if (*data == ADC_USR_DATA_CHECK) {
		pr_err("Invalid data:0x%x\n", *data);
		return -EINVAL;
	}

	return ret;
}

static int adc_read_voltage_data(struct adc_chip *adc, u16 *data)
{
	int ret;
	u8 rslt_lsb = 0, rslt_msb = 0;

	ret = adc_read(adc, ADC_USR_DATA0, &rslt_lsb, 1);
	if (ret)
		return ret;

	ret = adc_read(adc, ADC_USR_DATA1, &rslt_msb, 1);
	if (ret)
		return ret;

	*data = (rslt_msb << 8) | rslt_lsb;

	if (*data == ADC_USR_DATA_CHECK) {
		pr_err("Invalid data:0x%x\n", *data);
		return -EINVAL;
	}

	return ret;
}

static int adc_poll_wait_eoc(struct adc_chip *adc)
{
	unsigned int count, retry;
	u8 status1;
	int ret;

	retry = ADC_CONV_TIME_RETRY;

	for (count = 0; count < retry; count++) {
		ret = adc_read(adc, ADC_USR_STATUS1, &status1, 1);
		if (ret)
			return ret;

		status1 &= ADC_USR_STATUS1_REQ_STS_EOC_MASK;
		if (status1 == ADC_USR_STATUS1_EOC)
			return 0;
		usleep_range(ADC_CONV_TIME_MIN_US, ADC_CONV_TIME_MAX_US);
	}

	return -ETIMEDOUT;
}

static int adc_wait_eoc(struct adc_chip *adc)
{
	int ret;

	if (adc->poll_eoc) {
		ret = adc_poll_wait_eoc(adc);
		if (ret < 0) {
			pr_err("EOC bit not set\n");
			return ret;
		}
	} else {
		ret = wait_for_completion_timeout(&adc->complete,
							ADC_CONV_TIMEOUT);
		if (!ret) {
			pr_debug("Did not get completion timeout.\n");
			ret = adc_poll_wait_eoc(adc);
			if (ret < 0) {
				pr_err("EOC bit not set\n");
				return ret;
			}
		}
	}

	return ret;
}

static void adc_update_dig_param(struct adc_chip *adc,
			struct adc_channel_prop *prop, u8 *data)
{
	/* Update calibration value */
	*data &= ~ADC_USR_DIG_PARAM_CAL_VAL;
	*data |= (prop->cal_val << ADC_USR_DIG_PARAM_CAL_VAL_SHIFT);

	/* Update calibration select */
	*data &= ~ADC_USR_DIG_PARAM_CAL_SEL;
	*data |= (prop->cal_method << ADC_USR_DIG_PARAM_CAL_SEL_SHIFT);

	/* Update decimation ratio select */
	*data &= ~ADC_USR_DIG_PARAM_DEC_RATIO_SEL;
	*data |= (prop->decimation << ADC_USR_DIG_PARAM_DEC_RATIO_SEL_SHIFT);
}

static int adc_channel_check(struct adc_chip *adc, u8 buf)
{
	int ret = 0;
	u8 chno = 0;

	ret = adc_read(adc, ADC_USR_CH_SEL_CTL, &chno, 1);
	if (ret)
		return ret;

	if (buf != chno) {
		pr_debug("Channel write fails once: written:0x%x actual:0x%x\n",
			chno, buf);

		ret = adc_write(adc, ADC_USR_CH_SEL_CTL, &buf, 1);
		if (ret)
			return ret;

		ret = adc_read(adc, ADC_USR_CH_SEL_CTL, &chno, 1);
		if (ret)
			return ret;

		if (chno != buf) {
			pr_err("Write fails twice: written: 0x%x\n", chno);
			return -EINVAL;
		}
	}
	return 0;
}

static int adc_post_configure_usb_in_read(struct adc_chip *adc,
					struct adc_channel_prop *prop)
{
	u8 data;

	if ((prop->channel == ADC_USB_IN_V_16) && adc->cal_addr &&
			!adc->skip_usb_wa) {
		data = ADC_CAL_DELAY_CTL_VAL_125MS;
		/* Set calibration measurement interval to 125ms */
		return regmap_bulk_write(adc->regmap,
					adc->cal_addr + ADC_CAL_DELAY_CTL,
					&data, 1);
	}

	return 0;
}

static int adc_pre_configure_usb_in_read(struct adc_chip *adc)
{
	int ret;
	u8 data = ADC_CAL_DELAY_CTL_VAL_256S;
	bool channel_check = false;

	if (adc->pmic_rev_id)
		if (adc->pmic_rev_id->pmic_subtype == PMI632_SUBTYPE)
			channel_check = true;

	/* Increase calibration measurement interval to 256s */
	ret = regmap_bulk_write(adc->regmap,
				adc->cal_addr + ADC_CAL_DELAY_CTL, &data, 1);
	if (ret)
		return ret;

	/* Add delay of 20ms to allow completion of pending conversions */
	msleep(20);

	/* Select REF_GND and start a conversion */
	data = ADC_REF_GND;
	ret = adc_write(adc, ADC_USR_CH_SEL_CTL, &data, 1);
	if (ret)
		return ret;

	if (channel_check) {
		ret = adc_channel_check(adc, data);
		if (ret)
			return ret;
	}

	data = ADC_USR_EN_CTL1_ADC_EN;
	ret = adc_write(adc, ADC_USR_EN_CTL1, &data, 1);
	if (ret)
		return ret;

	if (!adc->poll_eoc)
		reinit_completion(&adc->complete);

	data = ADC_USR_CONV_REQ_REQ;
	ret = adc_write(adc, ADC_USR_CONV_REQ, &data, 1);
	if (ret)
		return ret;

	/* Select DIG PARAM and CH_SEL for USBIN */
	data = ADC_USR_DIG_PARAM_ABS_CAL_VAL;
	ret = adc_write(adc, ADC_USR_DIG_PARAM, &data, 1);
	if (ret)
		return ret;

	data = ADC_USB_IN_V_16;
	ret = adc_write(adc, ADC_USR_CH_SEL_CTL, &data, 1);
	if (ret)
		return ret;

	if (channel_check) {
		ret = adc_channel_check(adc, data);
		if (ret)
			return ret;
	}

	/* Check EOC for GND conversion */
	ret = adc_wait_eoc(adc);
	if (ret < 0)
		return ret;

	if (!adc->poll_eoc)
		reinit_completion(&adc->complete);

	/* Conversion request for USB_IN */
	data = ADC_USR_CONV_REQ_REQ;
	return adc_write(adc, ADC_USR_CONV_REQ, &data, 1);
}

#define ADC5_MULTI_TRANSFER	5

static int adc_configure(struct adc_chip *adc,
			struct adc_channel_prop *prop)
{
	int ret;
	u8 buf[ADC5_MULTI_TRANSFER];
	u8 conv_req = 0;
	bool channel_check = false;

	if (adc->pmic_rev_id)
		if (adc->pmic_rev_id->pmic_subtype == PMI632_SUBTYPE)
			channel_check = true;

	/* Read registers 0x42 through 0x46 */
	ret = adc_read(adc, ADC_USR_DIG_PARAM, buf, ADC5_MULTI_TRANSFER);
	if (ret < 0)
		return ret;

	/* Digital param selection */
	adc_update_dig_param(adc, prop, &buf[0]);

	/* Update fast average sample value */
	buf[1] &= (u8) ~ADC_USR_FAST_AVG_CTL_SAMPLES_MASK;
	buf[1] |= prop->avg_samples;

	/* Select ADC channel */
	buf[2] = prop->channel;

	/* Select HW settle delay for channel */
	buf[3] &= (u8) ~ADC_USR_HW_SETTLE_DELAY_MASK;
	buf[3] |= prop->hw_settle_time;

	/* Select ADC enable */
	buf[4] |= ADC_USR_EN_CTL1_ADC_EN;

	/* Select CONV request */
	conv_req = ADC_USR_CONV_REQ_REQ;

	if (!adc->poll_eoc)
		reinit_completion(&adc->complete);

	ret = adc_write(adc, ADC_USR_DIG_PARAM, buf, ADC5_MULTI_TRANSFER);
	if (ret)
		return ret;

	if (channel_check) {
		ret = adc_channel_check(adc, buf[2]);
		if (ret)
			return ret;
	}

	ret = adc_write(adc, ADC_USR_CONV_REQ, &conv_req, 1);

	return ret;
}

static int adc_do_conversion(struct adc_chip *adc,
			struct adc_channel_prop *prop,
			struct iio_chan_spec const *chan,
			u16 *data_volt, u16 *data_cur)
{
	int ret;

	mutex_lock(&adc->lock);

	if ((prop->channel == ADC_USB_IN_V_16) && adc->cal_addr &&
			!adc->skip_usb_wa) {
		ret = adc_pre_configure_usb_in_read(adc);
		if (ret) {
			pr_err("ADC configure failed with %d\n", ret);
			goto unlock;
		}
	} else {
		ret = adc_configure(adc, prop);
		if (ret) {
			pr_err("ADC configure failed with %d\n", ret);
			goto unlock;
		}
	}

	ret = adc_wait_eoc(adc);
	if (ret < 0)
		goto unlock;

	if ((chan->type == IIO_VOLTAGE) || (chan->type == IIO_TEMP))
		ret = adc_read_voltage_data(adc, data_volt);
	else if (chan->type == IIO_POWER) {
		ret = adc_read_voltage_data(adc, data_volt);
		if (ret)
			goto unlock;

		ret = adc_read_current_data(adc, data_cur);
	}

	ret = adc_post_configure_usb_in_read(adc, prop);
unlock:
	mutex_unlock(&adc->lock);

	return ret;
}

static irqreturn_t adc_isr(int irq, void *dev_id)
{
	struct adc_chip *adc = dev_id;

	complete(&adc->complete);

	return IRQ_HANDLED;
}

static int adc_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct adc_chip *adc = iio_priv(indio_dev);
	int i;

	for (i = 0; i < adc->nchannels; i++)
		if (adc->chan_props[i].channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static int adc_read_raw(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan, int *val, int *val2,
			 long mask)
{
	struct adc_chip *adc = iio_priv(indio_dev);
	struct adc_channel_prop *prop;
	u16 adc_code_volt, adc_code_cur;
	int ret;

	prop = &adc->chan_props[chan->address];

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		ret = adc_do_conversion(adc, prop, chan,
				&adc_code_volt, &adc_code_cur);
		if (ret)
			break;

		if ((chan->type == IIO_VOLTAGE) || (chan->type == IIO_TEMP))
			ret = qcom_vadc_hw_scale(prop->scale_fn_type,
				&adc_prescale_ratios[prop->prescale],
				adc->data, prop->lut_index,
				adc_code_volt, val);
		if (ret)
			break;

		if (chan->type == IIO_POWER) {
			ret = qcom_vadc_hw_scale(SCALE_HW_CALIB_DEFAULT,
				&adc_prescale_ratios[VADC_DEF_VBAT_PRESCALING],
				adc->data, prop->lut_index,
				adc_code_volt, val);
			if (ret)
				break;

			ret = qcom_vadc_hw_scale(prop->scale_fn_type,
				&adc_prescale_ratios[prop->prescale],
				adc->data, prop->lut_index,
				adc_code_cur, val2);
			if (ret)
				break;
		}

		if (chan->type == IIO_POWER)
			return IIO_VAL_INT_MULTIPLE;
		else
			return IIO_VAL_INT;
	case IIO_CHAN_INFO_RAW:
		ret = adc_do_conversion(adc, prop, chan,
				&adc_code_volt, &adc_code_cur);
		if (ret)
			break;

		*val = (int)adc_code_volt;
		*val2 = (int)adc_code_cur;
		if (chan->type == IIO_POWER)
			return IIO_VAL_INT_MULTIPLE;
		else
			return IIO_VAL_INT;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct iio_info adc_info = {
	.read_raw = adc_read_raw,
	.of_xlate = adc_of_xlate,
};

struct adc_channels {
	const char *datasheet_name;
	unsigned int prescale_index;
	enum iio_chan_type type;
	long info_mask;
	enum vadc_scale_fn_type scale_fn_type;
};

#define ADC_CHAN(_dname, _type, _mask, _pre, _scale)			\
	{								\
		.datasheet_name = (_dname),				\
		.prescale_index = _pre,					\
		.type = _type,						\
		.info_mask = _mask,					\
		.scale_fn_type = _scale,				\
	},								\

#define ADC_CHAN_TEMP(_dname, _pre, _scale)				\
	ADC_CHAN(_dname, IIO_TEMP,					\
		BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),	\
		_pre, _scale)						\

#define ADC_CHAN_VOLT(_dname, _pre, _scale)				\
	ADC_CHAN(_dname, IIO_VOLTAGE,					\
		  BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),\
		  _pre, _scale)						\

#define ADC_CHAN_POWER(_dname, _pre, _scale)				\
	ADC_CHAN(_dname, IIO_POWER,					\
		  BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),\
		  _pre, _scale)						\

static const struct adc_channels adc_chans_pmic5[ADC_MAX_CHANNEL] = {
	[ADC_REF_GND]		= ADC_CHAN_VOLT("ref_gnd", 1,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_1P25VREF]		= ADC_CHAN_VOLT("vref_1p25", 1,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_VPH_PWR]		= ADC_CHAN_VOLT("vph_pwr", 3,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_VBAT_SNS]		= ADC_CHAN_VOLT("vbat_sns", 3,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_VCOIN]		= ADC_CHAN_VOLT("vcoin", 3,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_DIE_TEMP]		= ADC_CHAN_TEMP("die_temp", 1,
					SCALE_HW_CALIB_PMIC_THERM)
	[ADC_USB_IN_I]		= ADC_CHAN_VOLT("usb_in_i_uv", 1,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_USB_IN_V_16]	= ADC_CHAN_VOLT("usb_in_v_div_16", 16,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_CHG_TEMP]		= ADC_CHAN_TEMP("chg_temp", 1,
					SCALE_HW_CALIB_PM5_CHG_TEMP)
	/* Charger prescales SBUx and MID_CHG to fit within 1.8V upper unit */
	[ADC_SBUx]		= ADC_CHAN_VOLT("chg_sbux", 3,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_MID_CHG_DIV6]	= ADC_CHAN_VOLT("chg_mid_chg", 6,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_XO_THERM_PU2]	= ADC_CHAN_TEMP("xo_therm", 1,
					SCALE_HW_CALIB_XOTHERM)
	[ADC_BAT_THERM_PU2]	= ADC_CHAN_TEMP("bat_therm_pu2", 1,
					SCALE_HW_CALIB_BATT_THERM_100K)
	[ADC_BAT_THERM_PU1]	= ADC_CHAN_TEMP("bat_therm_pu1", 1,
					SCALE_HW_CALIB_BATT_THERM_30K)
	[ADC_BAT_THERM_PU3]	= ADC_CHAN_TEMP("bat_therm_pu3", 1,
					SCALE_HW_CALIB_BATT_THERM_400K)
	[ADC_BAT_ID_PU2]	= ADC_CHAN_TEMP("bat_id", 1,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_AMUX_THM1_PU2]	= ADC_CHAN_TEMP("amux_thm1_pu2", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
	[ADC_AMUX_THM2_PU2]	= ADC_CHAN_TEMP("amux_thm2_pu2", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
	[ADC_AMUX_THM3_PU2]	= ADC_CHAN_TEMP("amux_thm3_pu2", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
	[ADC_AMUX_THM4_PU2]	= ADC_CHAN_TEMP("amux_thm4_pu2", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
	[ADC_INT_EXT_ISENSE_VBAT_VDATA]	= ADC_CHAN_POWER("int_ext_isense", 1,
					SCALE_HW_CALIB_CUR)
	[ADC_EXT_ISENSE_VBAT_VDATA]	= ADC_CHAN_POWER("ext_isense", 1,
					SCALE_HW_CALIB_CUR)
	[ADC_PARALLEL_ISENSE_VBAT_VDATA] = ADC_CHAN_POWER("parallel_isense", 1,
					SCALE_HW_CALIB_CUR)
	[ADC_AMUX_THM2]			= ADC_CHAN_TEMP("amux_thm2", 1,
					SCALE_HW_CALIB_PM5_SMB_TEMP)
	[ADC_AMUX_THM3]			= ADC_CHAN_TEMP("amux_thm3", 1,
					SCALE_HW_CALIB_PM5_SMB_TEMP)
	[ADC_GPIO1_PU2]	= ADC_CHAN_TEMP("gpio1_pu2", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
	[ADC_GPIO2_PU2]	= ADC_CHAN_TEMP("gpio2_pu2", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
	[ADC_GPIO3_PU2]	= ADC_CHAN_TEMP("gpio3_pu2", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
	[ADC_GPIO4_PU2]	= ADC_CHAN_TEMP("gpio4_pu2", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
};

static const struct adc_channels adc_chans_rev2[ADC_MAX_CHANNEL] = {
	[ADC_REF_GND]		= ADC_CHAN_VOLT("ref_gnd", 1,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_1P25VREF]		= ADC_CHAN_VOLT("vref_1p25", 1,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_VPH_PWR]		= ADC_CHAN_VOLT("vph_pwr", 3,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_VBAT_SNS]		= ADC_CHAN_VOLT("vbat_sns", 3,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_VCOIN]		= ADC_CHAN_VOLT("vcoin", 3,
					SCALE_HW_CALIB_DEFAULT)
	[ADC_DIE_TEMP]		= ADC_CHAN_TEMP("die_temp", 1,
					SCALE_HW_CALIB_PMIC_THERM)
	[ADC_AMUX_THM1_PU2]	= ADC_CHAN_TEMP("amux_thm1_pu2", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
	[ADC_AMUX_THM3_PU2]	= ADC_CHAN_TEMP("amux_thm3_pu2", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
	[ADC_AMUX_THM5_PU2]	= ADC_CHAN_TEMP("amux_thm5_pu2", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
	[ADC_XO_THERM_PU2]	= ADC_CHAN_TEMP("xo_therm", 1,
					SCALE_HW_CALIB_THERM_100K_PULLUP)
};

static int adc_get_dt_channel_data(struct device *dev,
				    struct adc_channel_prop *prop,
				    struct device_node *node,
				    const struct adc_data *data)
{
	const char *name = node->name, *channel_name;
	u32 chan, value, varr[2];
	int ret;

	ret = of_property_read_u32(node, "reg", &chan);
	if (ret) {
		dev_err(dev, "invalid channel number %s\n", name);
		return ret;
	}

	if (chan > ADC_PARALLEL_ISENSE_VBAT_IDATA) {
		dev_err(dev, "%s invalid channel number %d\n", name, chan);
		return -EINVAL;
	}

	/* the channel has DT description */
	prop->channel = chan;

	channel_name = of_get_property(node,
				"label", NULL) ? : node->name;
	if (!channel_name) {
		pr_err("Invalid channel name\n");
		return -EINVAL;
	}
	prop->datasheet_name = channel_name;

	ret = of_property_read_u32(node, "qcom,decimation", &value);
	if (!ret) {
		ret = qcom_adc5_decimation_from_dt(value, data->decimation);
		if (ret < 0) {
			dev_err(dev, "%02x invalid decimation %d\n",
				chan, value);
			return ret;
		}
		prop->decimation = ret;
	} else {
		prop->decimation = ADC_DECIMATION_DEFAULT;
	}

	ret = of_property_read_u32_array(node, "qcom,pre-scaling", varr, 2);
	if (!ret) {
		ret = adc_prescaling_from_dt(varr[0], varr[1]);
		if (ret < 0) {
			dev_err(dev, "%02x invalid pre-scaling <%d %d>\n",
				chan, varr[0], varr[1]);
			return ret;
		}
		prop->prescale = ret;
	}

	ret = of_property_read_u32(node, "qcom,hw-settle-time", &value);
	if (!ret) {
		ret = adc_hw_settle_time_from_dt(value, data->hw_settle);
		if (ret < 0) {
			dev_err(dev, "%02x invalid hw-settle-time %d us\n",
				chan, value);
			return ret;
		}
		prop->hw_settle_time = ret;
	} else {
		prop->hw_settle_time = VADC_DEF_HW_SETTLE_TIME;
	}

	ret = of_property_read_u32(node, "qcom,avg-samples", &value);
	if (!ret) {
		ret = adc_avg_samples_from_dt(value);
		if (ret < 0) {
			dev_err(dev, "%02x invalid avg-samples %d\n",
				chan, value);
			return ret;
		}
		prop->avg_samples = ret;
	} else {
		prop->avg_samples = VADC_DEF_AVG_SAMPLES;
	}

	prop->lut_index = VADC_DEF_LUT_INDEX;

	ret = of_property_read_u32(node, "qcom,lut-index", &value);
	if (!ret)
		prop->lut_index = value;

	if (of_property_read_bool(node, "qcom,ratiometric"))
		prop->cal_method = ADC_RATIOMETRIC_CAL;
	else
		prop->cal_method = ADC_ABSOLUTE_CAL;

	/*
	 * Default to using timer calibration. Using a fresh calibration value
	 * for every conversion will increase the overall time for a request.
	 */
	prop->cal_val = ADC_TIMER_CAL;

	dev_dbg(dev, "%02x name %s\n", chan, name);

	return 0;
}

const struct adc_data data_pmic5 = {
	.full_scale_code_volt = 0x70e4,
	/* On PM8150B, IBAT LSB = 10A/32767 */
	.full_scale_code_cur = 10000,
	.adc_chans = adc_chans_pmic5,
	.decimation = (unsigned int []) {250, 420, 840},
	.hw_settle = (unsigned int []) {15, 100, 200, 300, 400, 500, 600, 700,
					800, 900, 1, 2, 4, 6, 8, 10},
};

const struct adc_data data_pmic_rev2 = {
	.full_scale_code_volt = 0x4000,
	.full_scale_code_cur = 0x1800,
	.adc_chans = adc_chans_rev2,
	.decimation = (unsigned int []) {256, 512, 1024},
	.hw_settle = (unsigned int []) {0, 100, 200, 300, 400, 500, 600, 700,
					800, 900, 1, 2, 4, 6, 8, 10},
};

static const struct of_device_id adc_match_table[] = {
	{
		.compatible = "qcom,spmi-adc5",
		.data = &data_pmic5,
	},
	{
		.compatible = "qcom,spmi-adc-rev2",
		.data = &data_pmic_rev2,
	},
	{ }
};

static int adc_get_dt_data(struct adc_chip *adc, struct device_node *node)
{
	const struct adc_channels *adc_chan;
	struct iio_chan_spec *iio_chan;
	struct adc_channel_prop prop;
	struct device_node *child;
	unsigned int index = 0;
	const struct of_device_id *id;
	const struct adc_data *data;
	int ret;

	adc->nchannels = of_get_available_child_count(node);
	if (!adc->nchannels)
		return -EINVAL;

	adc->iio_chans = devm_kcalloc(adc->dev, adc->nchannels,
				       sizeof(*adc->iio_chans), GFP_KERNEL);
	if (!adc->iio_chans)
		return -ENOMEM;

	adc->chan_props = devm_kcalloc(adc->dev, adc->nchannels,
					sizeof(*adc->chan_props), GFP_KERNEL);
	if (!adc->chan_props)
		return -ENOMEM;

	iio_chan = adc->iio_chans;
	id = of_match_node(adc_match_table, node);
	if (id)
		data = id->data;
	else
		data = &data_pmic5;
	adc->data = data;

	for_each_available_child_of_node(node, child) {
		ret = adc_get_dt_channel_data(adc->dev, &prop, child, data);
		if (ret) {
			of_node_put(child);
			return ret;
		}

		prop.scale_fn_type =
			data->adc_chans[prop.channel].scale_fn_type;
		adc->chan_props[index] = prop;

		adc_chan = &data->adc_chans[prop.channel];

		iio_chan->channel = prop.channel;
		iio_chan->datasheet_name = prop.datasheet_name;
		iio_chan->extend_name = prop.datasheet_name;
		iio_chan->info_mask_separate = adc_chan->info_mask;
		iio_chan->type = adc_chan->type;
		iio_chan->address = index;
		iio_chan++;
		index++;
	}

	return 0;
}

static const struct pmic_rev_data pmic_data[] = {
	{PM6150_SUBTYPE,	1},
	{PM7250B_SUBTYPE,	0},
};

bool skip_usb_in_wa(struct pmic_revid_data *pmic_rev_id)
{
	int i = 0;
	uint32_t tablesize = ARRAY_SIZE(pmic_data);

	while (i < tablesize) {
		if (pmic_data[i].subtype == pmic_rev_id->pmic_subtype
		 && pmic_data[i].rev4 < pmic_rev_id->rev4) {
			return true;
		}
		i++;
	}
	return false;
}

static int adc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *revid_dev_node;
	struct pmic_revid_data	*pmic_rev_id = NULL;
	struct device *dev = &pdev->dev;
	struct iio_dev *indio_dev;
	struct adc_chip *adc;
	struct regmap *regmap;
	const __be32 *prop_addr;
	int ret, irq_eoc;
	u32 reg;
	bool skip_usb_wa = false;

	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap)
		return -ENODEV;

	ret = of_property_read_u32(node, "reg", &reg);
	if (ret < 0)
		return ret;

	revid_dev_node = of_parse_phandle(node, "qcom,pmic-revid", 0);
	if (revid_dev_node) {
		pmic_rev_id = get_revid_data(revid_dev_node);
		if (!(IS_ERR(pmic_rev_id)))
			skip_usb_wa = skip_usb_in_wa(pmic_rev_id);
		else {
			pr_err("Unable to get revid\n");
			pmic_rev_id = NULL;
		}
		of_node_put(revid_dev_node);
	}

	indio_dev = devm_iio_device_alloc(dev, sizeof(*adc));
	if (!indio_dev)
		return -ENOMEM;

	adc = iio_priv(indio_dev);
	adc->regmap = regmap;
	adc->dev = dev;
	adc->pmic_rev_id = pmic_rev_id;

	prop_addr = of_get_address(dev->of_node, 0, NULL, NULL);
	if (!prop_addr) {
		pr_err("invalid IO resources\n");
		return -EINVAL;
	}
	adc->base = be32_to_cpu(*prop_addr);

	prop_addr = of_get_address(dev->of_node, 1, NULL, NULL);
	if (!prop_addr)
		pr_debug("invalid cal IO resources\n");
	else
		adc->cal_addr = be32_to_cpu(*prop_addr);

	adc->skip_usb_wa = skip_usb_wa;

	init_completion(&adc->complete);
	mutex_init(&adc->lock);

	ret = adc_get_dt_data(adc, node);
	if (ret) {
		pr_err("adc get dt data failed\n");
		return ret;
	}

	irq_eoc = platform_get_irq(pdev, 0);
	if (irq_eoc < 0) {
		if (irq_eoc == -EPROBE_DEFER || irq_eoc == -EINVAL)
			return irq_eoc;
		adc->poll_eoc = true;
	} else {
		ret = devm_request_irq(dev, irq_eoc, adc_isr, 0,
				       "pm-adc5", adc);
		if (ret)
			return ret;
	}

	indio_dev->dev.parent = dev;
	indio_dev->dev.of_node = node;
	indio_dev->name = pdev->name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &adc_info;
	indio_dev->channels = adc->iio_chans;
	indio_dev->num_channels = adc->nchannels;

	return devm_iio_device_register(dev, indio_dev);
}

static struct platform_driver adc_driver = {
	.driver = {
		.name = "qcom-spmi-adc5.c",
		.of_match_table = adc_match_table,
	},
	.probe = adc_probe,
};
module_platform_driver(adc_driver);

MODULE_ALIAS("platform:qcom-spmi-adc5");
MODULE_DESCRIPTION("Qualcomm Technologies Inc. PMIC5 ADC driver");
MODULE_LICENSE("GPL v2");
