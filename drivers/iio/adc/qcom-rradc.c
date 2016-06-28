/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/iio/iio.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define FG_ADC_RR_EN_CTL			0x46
#define FG_ADC_RR_SKIN_TEMP_LSB			0x50
#define FG_ADC_RR_SKIN_TEMP_MSB			0x51
#define FG_ADC_RR_RR_ADC_CTL			0x52

#define FG_ADC_RR_FAKE_BATT_LOW_LSB		0x58
#define FG_ADC_RR_FAKE_BATT_LOW_MSB		0x59
#define FG_ADC_RR_FAKE_BATT_HIGH_LSB		0x5A
#define FG_ADC_RR_FAKE_BATT_HIGH_MSB		0x5B

#define FG_ADC_RR_BATT_ID_CTRL			0x60
#define FG_ADC_RR_BATT_ID_TRIGGER		0x61
#define FG_ADC_RR_BATT_ID_STS			0x62
#define FG_ADC_RR_BATT_ID_CFG			0x63
#define FG_ADC_RR_BATT_ID_5_LSB			0x66
#define FG_ADC_RR_BATT_ID_5_MSB			0x67
#define FG_ADC_RR_BATT_ID_15_LSB		0x68
#define FG_ADC_RR_BATT_ID_15_MSB		0x69
#define FG_ADC_RR_BATT_ID_150_LSB		0x6A
#define FG_ADC_RR_BATT_ID_150_MSB		0x6B

#define FG_ADC_RR_BATT_THERM_CTRL		0x70
#define FG_ADC_RR_BATT_THERM_TRIGGER		0x71
#define FG_ADC_RR_BATT_THERM_STS		0x72
#define FG_ADC_RR_BATT_THERM_CFG		0x73
#define FG_ADC_RR_BATT_THERM_LSB		0x74
#define FG_ADC_RR_BATT_THERM_MSB		0x75
#define FG_ADC_RR_BATT_THERM_FREQ		0x76

#define FG_ADC_RR_AUX_THERM_CTRL		0x80
#define FG_ADC_RR_AUX_THERM_TRIGGER		0x81
#define FG_ADC_RR_AUX_THERM_STS			0x82
#define FG_ADC_RR_AUX_THERM_CFG			0x83
#define FG_ADC_RR_AUX_THERM_LSB			0x84
#define FG_ADC_RR_AUX_THERM_MSB			0x85

#define FG_ADC_RR_SKIN_HOT			0x86
#define FG_ADC_RR_SKIN_TOO_HOT			0x87

#define FG_ADC_RR_AUX_THERM_C1			0x88
#define FG_ADC_RR_AUX_THERM_C2			0x89
#define FG_ADC_RR_AUX_THERM_C3			0x8A
#define FG_ADC_RR_AUX_THERM_HALF_RANGE		0x8B

#define FG_ADC_RR_USB_IN_V_CTRL			0x90
#define FG_ADC_RR_USB_IN_V_TRIGGER		0x91
#define FG_ADC_RR_USB_IN_V_STS			0x92
#define FG_ADC_RR_USB_IN_V_LSB			0x94
#define FG_ADC_RR_USB_IN_V_MSB			0x95
#define FG_ADC_RR_USB_IN_I_CTRL			0x98
#define FG_ADC_RR_USB_IN_I_TRIGGER		0x99
#define FG_ADC_RR_USB_IN_I_STS			0x9A
#define FG_ADC_RR_USB_IN_I_LSB			0x9C
#define FG_ADC_RR_USB_IN_I_MSB			0x9D

#define FG_ADC_RR_DC_IN_V_CTRL			0xA0
#define FG_ADC_RR_DC_IN_V_TRIGGER		0xA1
#define FG_ADC_RR_DC_IN_V_STS			0xA2
#define FG_ADC_RR_DC_IN_V_LSB			0xA4
#define FG_ADC_RR_DC_IN_V_MSB			0xA5
#define FG_ADC_RR_DC_IN_I_CTRL			0xA8
#define FG_ADC_RR_DC_IN_I_TRIGGER		0xA9
#define FG_ADC_RR_DC_IN_I_STS			0xAA
#define FG_ADC_RR_DC_IN_I_LSB			0xAC
#define FG_ADC_RR_DC_IN_I_MSB			0xAD

#define FG_ADC_RR_PMI_DIE_TEMP_CTRL		0xB0
#define FG_ADC_RR_PMI_DIE_TEMP_TRIGGER		0xB1
#define FG_ADC_RR_PMI_DIE_TEMP_STS		0xB2
#define FG_ADC_RR_PMI_DIE_TEMP_CFG		0xB3
#define FG_ADC_RR_PMI_DIE_TEMP_LSB		0xB4
#define FG_ADC_RR_PMI_DIE_TEMP_MSB		0xB5

#define FG_ADC_RR_CHARGER_TEMP_CTRL		0xB8
#define FG_ADC_RR_CHARGER_TEMP_TRIGGER		0xB9
#define FG_ADC_RR_CHARGER_TEMP_STS		0xBA
#define FG_ADC_RR_CHARGER_TEMP_CFG		0xBB
#define FG_ADC_RR_CHARGER_TEMP_LSB		0xBC
#define FG_ADC_RR_CHARGER_TEMP_MSB		0xBD
#define FG_ADC_RR_CHARGER_HOT			0xBE
#define FG_ADC_RR_CHARGER_TOO_HOT		0xBF

#define FG_ADC_RR_GPIO_CTRL			0xC0
#define FG_ADC_RR_GPIO_TRIGGER			0xC1
#define FG_ADC_RR_GPIO_STS			0xC2
#define FG_ADC_RR_GPIO_LSB			0xC4
#define FG_ADC_RR_GPIO_MSB			0xC5

#define FG_ADC_RR_ATEST_CTRL			0xC8
#define FG_ADC_RR_ATEST_TRIGGER			0xC9
#define FG_ADC_RR_ATEST_STS			0xCA
#define FG_ADC_RR_ATEST_LSB			0xCC
#define FG_ADC_RR_ATEST_MSB			0xCD
#define FG_ADC_RR_SEC_ACCESS			0xD0

#define FG_ADC_RR_PERPH_RESET_CTL2		0xD9
#define FG_ADC_RR_PERPH_RESET_CTL3		0xDA
#define FG_ADC_RR_PERPH_RESET_CTL4		0xDB
#define FG_ADC_RR_INT_TEST1			0xE0
#define FG_ADC_RR_INT_TEST_VAL			0xE1

#define FG_ADC_RR_TM_TRIGGER_CTRLS		0xE2
#define FG_ADC_RR_TM_ADC_CTRLS			0xE3
#define FG_ADC_RR_TM_CNL_CTRL			0xE4
#define FG_ADC_RR_TM_BATT_ID_CTRL		0xE5
#define FG_ADC_RR_TM_THERM_CTRL			0xE6
#define FG_ADC_RR_TM_CONV_STS			0xE7
#define FG_ADC_RR_TM_ADC_READ_LSB		0xE8
#define FG_ADC_RR_TM_ADC_READ_MSB		0xE9
#define FG_ADC_RR_TM_ATEST_MUX_1		0xEA
#define FG_ADC_RR_TM_ATEST_MUX_2		0xEB
#define FG_ADC_RR_TM_REFERENCES			0xED
#define FG_ADC_RR_TM_MISC_CTL			0xEE
#define FG_ADC_RR_TM_RR_CTRL			0xEF

#define FG_ADC_RR_BATT_ID_5_MA			5
#define FG_ADC_RR_BATT_ID_15_MA			15
#define FG_ADC_RR_BATT_ID_150_MA		150
#define FG_ADC_RR_BATT_ID_RANGE			820

#define FG_ADC_BITS				10
#define FG_MAX_ADC_READINGS			(1 << FG_ADC_BITS)
#define FG_ADC_RR_FS_VOLTAGE_MV			2500

/* BATT_THERM 0.25K/LSB */
#define FG_ADC_RR_BATT_THERM_LSB_K		4

#define FG_ADC_RR_TEMP_FS_VOLTAGE_NUM		5000000
#define FG_ADC_RR_TEMP_FS_VOLTAGE_DEN		3
#define FG_ADC_RR_DIE_TEMP_OFFSET		600000
#define FG_ADC_RR_DIE_TEMP_SLOPE		2000
#define FG_ADC_RR_DIE_TEMP_OFFSET_DEGC		25

#define FG_ADC_RR_CHG_TEMP_OFFSET		1288000
#define FG_ADC_RR_CHG_TEMP_SLOPE		4000
#define FG_ADC_RR_CHG_TEMP_OFFSET_DEGC		27

#define FG_ADC_RR_VOLT_INPUT_FACTOR		8
#define FG_ADC_RR_CURR_INPUT_FACTOR		2
#define FG_ADC_SCALE_MILLI_FACTOR		1000
#define FG_ADC_KELVINMIL_CELSIUSMIL		273150

#define FG_ADC_RR_GPIO_FS_RANGE			5000

/*
 * The channel number is not a physical index in hardware,
 * rather it's a list of supported channels and an index to
 * select the respective channel properties such as scaling
 * the result. Add any new additional channels supported by
 * the RR ADC before RR_ADC_MAX.
 */
enum rradc_channel_id {
	RR_ADC_BATT_ID_5 = 0,
	RR_ADC_BATT_ID_15,
	RR_ADC_BATT_ID_150,
	RR_ADC_BATT_ID,
	RR_ADC_BATT_THERM,
	RR_ADC_SKIN_TEMP,
	RR_ADC_USBIN_V,
	RR_ADC_USBIN_I,
	RR_ADC_DCIN_V,
	RR_ADC_DCIN_I,
	RR_ADC_DIE_TEMP,
	RR_ADC_CHG_TEMP,
	RR_ADC_GPIO,
	RR_ADC_ATEST,
	RR_ADC_TM_ADC,
	RR_ADC_MAX
};

struct rradc_chip {
	struct device			*dev;
	struct mutex			lock;
	struct regmap			*regmap;
	u16				base;
	struct iio_chan_spec		*iio_chans;
	unsigned int			nchannels;
	struct rradc_chan_prop		*chan_props;
};

struct rradc_channels {
	const char			*datasheet_name;
	enum iio_chan_type		type;
	long				info_mask;
	u8				lsb;
	u8				msb;
	int (*scale)(struct rradc_chip *chip, struct rradc_chan_prop *prop,
					u16 adc_code, int *result);
};

struct rradc_chan_prop {
	enum rradc_channel_id		channel;
	int (*scale)(struct rradc_chip *chip, struct rradc_chan_prop *prop,
					u16 adc_code, int *result);
};

static int rradc_read(struct rradc_chip *rr_adc, u16 offset, u8 *data, int len)
{
	int rc = 0;

	rc = regmap_bulk_read(rr_adc->regmap, rr_adc->base + offset, data, len);
	if (rc < 0)
		pr_err("rr adc read reg %d failed with %d\n", offset, rc);

	return rc;
}

static int rradc_post_process_batt_id(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_mohms)
{
	uint32_t current_value;
	int64_t r_id;

	switch (prop->channel) {
	case RR_ADC_BATT_ID_5:
		current_value = FG_ADC_RR_BATT_ID_5_MA;
		break;
	case RR_ADC_BATT_ID_15:
		current_value = FG_ADC_RR_BATT_ID_15_MA;
		break;
	case RR_ADC_BATT_ID_150:
		current_value = FG_ADC_RR_BATT_ID_150_MA;
		break;
	default:
		return -EINVAL;
	}

	r_id = ((int64_t)adc_code * FG_ADC_RR_FS_VOLTAGE_MV);
	r_id = div64_s64(r_id, (FG_MAX_ADC_READINGS * current_value));
	*result_mohms = (r_id * FG_ADC_SCALE_MILLI_FACTOR);

	return 0;
}

static int rradc_post_process_therm(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_millidegc)
{
	int64_t temp;

	/* K = code/4 */
	temp = div64_s64(adc_code, FG_ADC_RR_BATT_THERM_LSB_K);
	temp *= FG_ADC_SCALE_MILLI_FACTOR;
	*result_millidegc = temp - FG_ADC_KELVINMIL_CELSIUSMIL;

	return 0;
}

static int rradc_post_process_volt(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_mv)
{
	int64_t mv = 0;

	/* 8x input attenuation; 2.5V ADC full scale */
	mv = ((int64_t)adc_code * FG_ADC_RR_VOLT_INPUT_FACTOR);
	mv *= FG_ADC_RR_FS_VOLTAGE_MV;
	mv = div64_s64(mv, FG_MAX_ADC_READINGS);
	*result_mv = mv;

	return 0;
}

static int rradc_post_process_curr(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_ma)
{
	int64_t ma = 0;

	/* 0.5 V/A; 2.5V ADC full scale */
	ma = ((int64_t)adc_code * FG_ADC_RR_CURR_INPUT_FACTOR);
	ma *= FG_ADC_RR_FS_VOLTAGE_MV;
	ma = div64_s64(ma, FG_MAX_ADC_READINGS);
	*result_ma = ma;

	return 0;
}

static int rradc_post_process_die_temp(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_millidegc)
{
	int64_t temp = 0;

	temp = ((int64_t)adc_code * FG_ADC_RR_TEMP_FS_VOLTAGE_NUM);
	temp = div64_s64(temp, (FG_ADC_RR_TEMP_FS_VOLTAGE_DEN *
					FG_MAX_ADC_READINGS));
	temp -= FG_ADC_RR_DIE_TEMP_OFFSET;
	temp = div64_s64(temp, FG_ADC_RR_DIE_TEMP_SLOPE);
	temp += FG_ADC_RR_DIE_TEMP_OFFSET_DEGC;
	*result_millidegc = (temp * FG_ADC_SCALE_MILLI_FACTOR);

	return 0;
}

static int rradc_post_process_chg_temp(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_millidegc)
{
	int64_t temp = 0;

	temp = ((int64_t) adc_code * FG_ADC_RR_TEMP_FS_VOLTAGE_NUM);
	temp = div64_s64(temp, (FG_ADC_RR_TEMP_FS_VOLTAGE_DEN *
					FG_MAX_ADC_READINGS));
	temp = FG_ADC_RR_CHG_TEMP_OFFSET - temp;
	temp = div64_s64(temp, FG_ADC_RR_CHG_TEMP_SLOPE);
	temp = temp + FG_ADC_RR_CHG_TEMP_OFFSET_DEGC;
	*result_millidegc = (temp * FG_ADC_SCALE_MILLI_FACTOR);

	return 0;
}

static int rradc_post_process_gpio(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_mv)
{
	int64_t mv = 0;

	/* 5V ADC full scale, 10 bit */
	mv = ((int64_t)adc_code * FG_ADC_RR_GPIO_FS_RANGE);
	mv = div64_s64(mv, FG_MAX_ADC_READINGS);
	*result_mv = mv;

	return 0;
}

#define RR_ADC_CHAN(_dname, _type, _mask, _scale, _lsb, _msb)		\
	{								\
		.datasheet_name = __stringify(_dname),			\
		.type = _type,						\
		.info_mask = _mask,					\
		.scale = _scale,					\
		.lsb = _lsb,						\
		.msb = _msb,						\
	},								\

#define RR_ADC_CHAN_TEMP(_dname, _scale, _lsb, _msb)			\
	RR_ADC_CHAN(_dname, IIO_TEMP,					\
		BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),	\
		_scale, _lsb, _msb)					\

#define RR_ADC_CHAN_VOLT(_dname, _scale, _lsb, _msb)			\
	RR_ADC_CHAN(_dname, IIO_VOLTAGE,				\
		  BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),\
		  _scale, _lsb, _msb)					\

#define RR_ADC_CHAN_CURRENT(_dname, _scale, _lsb, _msb)			\
	RR_ADC_CHAN(_dname, IIO_CURRENT,				\
		  BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),\
		  _scale, _lsb, _msb)					\

#define RR_ADC_CHAN_RESISTANCE(_dname, _scale, _lsb, _msb)		\
	RR_ADC_CHAN(_dname, IIO_RESISTANCE,				\
		  BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),\
		  _scale, _lsb, _msb)					\

static const struct rradc_channels rradc_chans[] = {
	RR_ADC_CHAN_RESISTANCE("batt_id_5", rradc_post_process_batt_id,
			FG_ADC_RR_BATT_ID_5_LSB, FG_ADC_RR_BATT_ID_5_MSB)
	RR_ADC_CHAN_RESISTANCE("batt_id_15", rradc_post_process_batt_id,
			FG_ADC_RR_BATT_ID_15_LSB, FG_ADC_RR_BATT_ID_15_MSB)
	RR_ADC_CHAN_RESISTANCE("batt_id_150", rradc_post_process_batt_id,
			FG_ADC_RR_BATT_ID_150_LSB, FG_ADC_RR_BATT_ID_150_MSB)
	RR_ADC_CHAN_RESISTANCE("batt_id", rradc_post_process_batt_id,
			FG_ADC_RR_BATT_ID_5_LSB, FG_ADC_RR_BATT_ID_5_MSB)
	RR_ADC_CHAN_TEMP("batt_therm", &rradc_post_process_therm,
			FG_ADC_RR_BATT_THERM_LSB, FG_ADC_RR_BATT_THERM_MSB)
	RR_ADC_CHAN_TEMP("skin_temp", &rradc_post_process_therm,
			FG_ADC_RR_SKIN_TEMP_LSB, FG_ADC_RR_SKIN_TEMP_MSB)
	RR_ADC_CHAN_CURRENT("usbin_i", &rradc_post_process_curr,
			FG_ADC_RR_USB_IN_V_LSB, FG_ADC_RR_USB_IN_V_MSB)
	RR_ADC_CHAN_VOLT("usbin_v", &rradc_post_process_volt,
			FG_ADC_RR_USB_IN_I_LSB, FG_ADC_RR_USB_IN_I_MSB)
	RR_ADC_CHAN_CURRENT("dcin_i", &rradc_post_process_curr,
			FG_ADC_RR_DC_IN_V_LSB, FG_ADC_RR_DC_IN_V_MSB)
	RR_ADC_CHAN_VOLT("dcin_v", &rradc_post_process_volt,
			FG_ADC_RR_DC_IN_I_LSB, FG_ADC_RR_DC_IN_I_MSB)
	RR_ADC_CHAN_TEMP("die_temp", &rradc_post_process_die_temp,
			FG_ADC_RR_PMI_DIE_TEMP_LSB, FG_ADC_RR_PMI_DIE_TEMP_MSB)
	RR_ADC_CHAN_TEMP("chg_temp", &rradc_post_process_chg_temp,
			FG_ADC_RR_CHARGER_TEMP_LSB, FG_ADC_RR_CHARGER_TEMP_MSB)
	RR_ADC_CHAN_VOLT("gpio", &rradc_post_process_gpio,
			FG_ADC_RR_GPIO_LSB, FG_ADC_RR_GPIO_MSB)
};

static int rradc_do_conversion(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 *data)
{
	int rc = 0, bytes_to_read = 0;
	u8 buf[6];
	u16 offset = 0, batt_id_5 = 0, batt_id_15 = 0, batt_id_150 = 0;

	mutex_lock(&chip->lock);

	offset = rradc_chans[prop->channel].lsb;
	if (prop->channel == RR_ADC_BATT_ID)
		bytes_to_read = 6;
	else
		bytes_to_read = 2;

	rc = rradc_read(chip, offset, buf, bytes_to_read);
	if (rc) {
		pr_err("read data failed\n");
		goto fail;
	}

	if (prop->channel == RR_ADC_BATT_ID) {
		batt_id_150 = (buf[4] << 8) | buf[5];
		batt_id_15 = (buf[2] << 8) | buf[3];
		batt_id_5 = (buf[0] << 8) | buf[1];
		if (batt_id_150 <= FG_ADC_RR_BATT_ID_RANGE) {
			pr_debug("Batt_id_150 is chosen\n");
			*data = batt_id_150;
		} else if (batt_id_15 <= FG_ADC_RR_BATT_ID_RANGE) {
			pr_debug("Batt_id_15 is chosen\n");
			*data = batt_id_15;
		} else {
			pr_debug("Batt_id_5 is chosen\n");
			*data = batt_id_5;
		}
	} else {
		*data = (buf[1] << 8) | buf[0];
	}
fail:
	mutex_unlock(&chip->lock);

	return rc;
}

static int rradc_read_raw(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan, int *val, int *val2,
			 long mask)
{
	struct rradc_chip *chip = iio_priv(indio_dev);
	struct rradc_chan_prop *prop;
	u16 adc_code;
	int rc = 0;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		prop = &chip->chan_props[chan->address];
		rc = rradc_do_conversion(chip, prop, &adc_code);
		if (rc)
			break;

		prop->scale(chip, prop, adc_code, val);

		return IIO_VAL_INT;
	case IIO_CHAN_INFO_RAW:
		prop = &chip->chan_props[chan->address];
		rc = rradc_do_conversion(chip, prop, &adc_code);
		if (rc)
			break;

		*val = (int) adc_code;

		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = 1000;
		return IIO_VAL_INT_PLUS_MICRO;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static const struct iio_info rradc_info = {
	.read_raw	= &rradc_read_raw,
	.driver_module	= THIS_MODULE,
};

static int rradc_get_dt_data(struct rradc_chip *chip, struct device_node *node)
{
	const struct rradc_channels *rradc_chan;
	struct iio_chan_spec *iio_chan;
	struct device_node *child;
	unsigned int index = 0, chan, base;
	int rc = 0;
	struct rradc_chan_prop prop;

	chip->nchannels = of_get_available_child_count(node);
	if (!chip->nchannels || (chip->nchannels >= RR_ADC_MAX))
		return -EINVAL;

	chip->iio_chans = devm_kcalloc(chip->dev, chip->nchannels,
				       sizeof(*chip->iio_chans), GFP_KERNEL);
	if (!chip->iio_chans)
		return -ENOMEM;

	chip->chan_props = devm_kcalloc(chip->dev, chip->nchannels,
				       sizeof(*chip->chan_props), GFP_KERNEL);
	if (!chip->chan_props)
		return -ENOMEM;

	/* Get the peripheral address */
	rc = of_property_read_u32(node, "reg", &base);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't find reg in node = %s rc = %d\n",
			node->name, rc);
		return rc;
	}

	chip->base = base;
	iio_chan = chip->iio_chans;

	for_each_available_child_of_node(node, child) {
		rc = of_property_read_u32(child, "channel", &chan);
		if (rc) {
			dev_err(chip->dev, "invalid channel number %d\n", chan);
			return rc;
		}

		if (chan > RR_ADC_MAX || chan < RR_ADC_BATT_ID_5) {
			dev_err(chip->dev, "invalid channel number %d\n", chan);
			return -EINVAL;
		}

		prop.channel = chan;
		prop.scale = rradc_chans[chan].scale;
		chip->chan_props[index] = prop;

		rradc_chan = &rradc_chans[chan];

		iio_chan->channel = prop.channel;
		iio_chan->datasheet_name = rradc_chan->datasheet_name;
		iio_chan->extend_name = rradc_chan->datasheet_name;
		iio_chan->info_mask_separate = rradc_chan->info_mask;
		iio_chan->type = rradc_chan->type;
		iio_chan->indexed = 1;
		iio_chan->address = index++;
		iio_chan++;
	}

	return 0;
}

static int rradc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct iio_dev *indio_dev;
	struct rradc_chip *chip;
	int rc = 0;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*chip));
	if (!indio_dev)
		return -ENOMEM;

	chip = iio_priv(indio_dev);
	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	chip->dev = dev;
	mutex_init(&chip->lock);

	rc = rradc_get_dt_data(chip, node);
	if (rc)
		return rc;

	indio_dev->dev.parent = dev;
	indio_dev->dev.of_node = node;
	indio_dev->name = pdev->name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &rradc_info;
	indio_dev->channels = chip->iio_chans;
	indio_dev->num_channels = chip->nchannels;

	return devm_iio_device_register(dev, indio_dev);
}

static const struct of_device_id rradc_match_table[] = {
	{ .compatible = "qcom,rradc" },
	{ }
};
MODULE_DEVICE_TABLE(of, rradc_match_table);

static struct platform_driver rradc_driver = {
	.driver		= {
		.name		= "qcom-rradc",
		.of_match_table	= rradc_match_table,
	},
	.probe = rradc_probe,
};
module_platform_driver(rradc_driver);

MODULE_DESCRIPTION("QPNP PMIC RR ADC driver");
MODULE_LICENSE("GPL v2");
