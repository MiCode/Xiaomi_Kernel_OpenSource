/*
 * Copyright (c) 2016-2017, 2019-2020, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "RRADC: %s: " fmt, __func__

#include <linux/iio/iio.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/power_supply.h>

#define FG_ADC_RR_EN_CTL			0x46
#define FG_ADC_RR_SKIN_TEMP_LSB			0x50
#define FG_ADC_RR_SKIN_TEMP_MSB			0x51
#define FG_ADC_RR_RR_ADC_CTL			0x52
#define FG_ADC_RR_ADC_CTL_CONTINUOUS_SEL_MASK	0x8
#define FG_ADC_RR_ADC_CTL_CONTINUOUS_SEL	BIT(3)
#define FG_ADC_RR_ADC_LOG			0x53
#define FG_ADC_RR_ADC_LOG_CLR_CTRL		BIT(0)

#define FG_ADC_RR_FAKE_BATT_LOW_LSB		0x58
#define FG_ADC_RR_FAKE_BATT_LOW_MSB		0x59
#define FG_ADC_RR_FAKE_BATT_HIGH_LSB		0x5A
#define FG_ADC_RR_FAKE_BATT_HIGH_MSB		0x5B

#define FG_ADC_RR_BATT_ID_CTRL			0x60
#define FG_ADC_RR_BATT_ID_CTRL_CHANNEL_CONV	BIT(0)
#define FG_ADC_RR_BATT_ID_TRIGGER		0x61
#define FG_ADC_RR_BATT_ID_TRIGGER_CTL		BIT(0)
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
#define FG_ADC_RR_USB_IN_V_EVERY_CYCLE_MASK	0x80
#define FG_ADC_RR_USB_IN_V_EVERY_CYCLE		BIT(7)
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
#define FG_ADC_RR_DIE_TEMP_OFFSET		601400
#define FG_ADC_RR_DIE_TEMP_SLOPE		2
#define FG_ADC_RR_DIE_TEMP_OFFSET_MILLI_DEGC	25000

#define FG_ADC_RR_CHG_TEMP_GF_OFFSET_UV		1303168
#define FG_ADC_RR_CHG_TEMP_GF_SLOPE_UV_PER_C	3784
#define FG_ADC_RR_CHG_TEMP_SMIC_OFFSET_UV	1338433
#define FG_ADC_RR_CHG_TEMP_SMIC_SLOPE_UV_PER_C	3655
#define FG_ADC_RR_CHG_TEMP_660_GF_OFFSET_UV	1309001
#define FG_RR_CHG_TEMP_660_GF_SLOPE_UV_PER_C	3403
#define FG_ADC_RR_CHG_TEMP_660_SMIC_OFFSET_UV	1295898
#define FG_RR_CHG_TEMP_660_SMIC_SLOPE_UV_PER_C	3596
#define FG_ADC_RR_CHG_TEMP_660_MGNA_OFFSET_UV	1314779
#define FG_RR_CHG_TEMP_660_MGNA_SLOPE_UV_PER_C	3496
#define FG_ADC_RR_CHG_TEMP_OFFSET_MILLI_DEGC	25000
#define FG_ADC_RR_CHG_THRESHOLD_SCALE		4

#define FG_ADC_RR_VOLT_INPUT_FACTOR		8
#define FG_ADC_RR_CURR_INPUT_FACTOR		2000
#define FG_ADC_RR_CURR_USBIN_INPUT_FACTOR_MIL	1886
#define FG_ADC_RR_CURR_USBIN_660_FACTOR_MIL	9
#define FG_ADC_RR_CURR_USBIN_660_UV_VAL	579500

#define FG_ADC_SCALE_MILLI_FACTOR		1000
#define FG_ADC_KELVINMIL_CELSIUSMIL		273150

#define FG_ADC_RR_GPIO_FS_RANGE			5000
#define FG_RR_ADC_COHERENT_CHECK_RETRY		5
#define FG_RR_ADC_MAX_CONTINUOUS_BUFFER_LEN	16
#define FG_RR_ADC_STS_CHANNEL_READING_MASK	0x3
#define FG_RR_ADC_STS_CHANNEL_STS		0x2

#define FG_RR_CONV_CONTINUOUS_TIME_MIN_MS	50
#define FG_RR_CONV_MAX_RETRY_CNT		50
#define FG_RR_TP_REV_VERSION1		21
#define FG_RR_TP_REV_VERSION2		29
#define FG_RR_TP_REV_VERSION3		32

#define BATT_ID_SETTLE_SHIFT		5
#define RRADC_BATT_ID_DELAY_MAX		8

/*
 * The channel number is not a physical index in hardware,
 * rather it's a list of supported channels and an index to
 * select the respective channel properties such as scaling
 * the result. Add any new additional channels supported by
 * the RR ADC before RR_ADC_MAX.
 */
enum rradc_channel_id {
	RR_ADC_BATT_ID = 0,
	RR_ADC_BATT_THERM,
	RR_ADC_SKIN_TEMP,
	RR_ADC_USBIN_I,
	RR_ADC_USBIN_V,
	RR_ADC_DCIN_I,
	RR_ADC_DCIN_V,
	RR_ADC_DIE_TEMP,
	RR_ADC_CHG_TEMP,
	RR_ADC_GPIO,
	RR_ADC_CHG_HOT_TEMP,
	RR_ADC_CHG_TOO_HOT_TEMP,
	RR_ADC_SKIN_HOT_TEMP,
	RR_ADC_SKIN_TOO_HOT_TEMP,
	RR_ADC_MAX
};

struct rradc_chip {
	struct device			*dev;
	struct mutex			lock;
	struct regmap			*regmap;
	u16				base;
	int				batt_id_delay;
	struct iio_chan_spec		*iio_chans;
	unsigned int			nchannels;
	struct rradc_chan_prop		*chan_props;
	struct device_node		*revid_dev_node;
	struct pmic_revid_data		*pmic_fab_id;
	int volt;
	struct power_supply		*usb_trig;
};

struct rradc_channels {
	const char			*datasheet_name;
	enum iio_chan_type		type;
	long				info_mask;
	u8				lsb;
	u8				msb;
	u8				sts;
	int (*scale)(struct rradc_chip *chip, struct rradc_chan_prop *prop,
					u16 adc_code, int *result);
};

struct rradc_chan_prop {
	enum rradc_channel_id		channel;
	uint32_t			channel_data;
	int (*scale)(struct rradc_chip *chip, struct rradc_chan_prop *prop,
					u16 adc_code, int *result);
};

static const int batt_id_delays[] = {0, 1, 4, 12, 20, 40, 60, 80};

static int rradc_masked_write(struct rradc_chip *rr_adc, u16 offset, u8 mask,
						u8 val)
{
	int rc;

	rc = regmap_update_bits(rr_adc->regmap, rr_adc->base + offset,
								mask, val);
	if (rc) {
		pr_err("spmi write failed: addr=%03X, rc=%d\n", offset, rc);
		return rc;
	}

	return rc;
}

static int rradc_read(struct rradc_chip *rr_adc, u16 offset, u8 *data, int len)
{
	int rc = 0, retry_cnt = 0, i = 0;
	u8 data_check[FG_RR_ADC_MAX_CONTINUOUS_BUFFER_LEN];
	bool coherent_err = false;

	if (len > FG_RR_ADC_MAX_CONTINUOUS_BUFFER_LEN) {
		pr_err("Increase the buffer length\n");
		return -EINVAL;
	}

	while (retry_cnt < FG_RR_ADC_COHERENT_CHECK_RETRY) {
		rc = regmap_bulk_read(rr_adc->regmap, rr_adc->base + offset,
							data, len);
		if (rc < 0) {
			pr_err("rr_adc reg 0x%x failed :%d\n", offset, rc);
			return rc;
		}

		rc = regmap_bulk_read(rr_adc->regmap, rr_adc->base + offset,
							data_check, len);
		if (rc < 0) {
			pr_err("rr_adc reg 0x%x failed :%d\n", offset, rc);
			return rc;
		}

		for (i = 0; i < len; i++) {
			if (data[i] != data_check[i])
				coherent_err = true;
		}

		if (coherent_err) {
			retry_cnt++;
			coherent_err = false;
			pr_debug("retry_cnt:%d\n", retry_cnt);
		} else {
			break;
		}
	}

	if (retry_cnt == FG_RR_ADC_COHERENT_CHECK_RETRY)
		pr_err("Retry exceeded for coherrency check\n");

	return rc;
}

static int rradc_post_process_batt_id(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_ohms)
{
	uint32_t current_value;
	int64_t r_id;

	current_value = prop->channel_data;
	r_id = ((int64_t)adc_code * FG_ADC_RR_FS_VOLTAGE_MV);
	r_id = div64_s64(r_id, (FG_MAX_ADC_READINGS * current_value));
	*result_ohms = (r_id * FG_ADC_SCALE_MILLI_FACTOR);

	return 0;
}

static int rradc_post_process_therm(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_millidegc)
{
	int64_t temp;

	/* K = code/4 */
	temp = ((int64_t)adc_code * FG_ADC_SCALE_MILLI_FACTOR);
	temp = div64_s64(temp, FG_ADC_RR_BATT_THERM_LSB_K);
	*result_millidegc = temp - FG_ADC_KELVINMIL_CELSIUSMIL;

	return 0;
}

static int rradc_post_process_volt(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_uv)
{
	int64_t uv = 0;

	/* 8x input attenuation; 2.5V ADC full scale */
	uv = ((int64_t)adc_code * FG_ADC_RR_VOLT_INPUT_FACTOR);
	uv *= (FG_ADC_RR_FS_VOLTAGE_MV * FG_ADC_SCALE_MILLI_FACTOR);
	uv = div64_s64(uv, FG_MAX_ADC_READINGS);
	*result_uv = uv;

	return 0;
}

static int rradc_post_process_usbin_curr(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_ua)
{
	int64_t ua = 0, scale = 0;

	if (!prop)
		return -EINVAL;
	if (chip->revid_dev_node) {
		switch (chip->pmic_fab_id->pmic_subtype) {
		case PM660_SUBTYPE:
			if (((chip->pmic_fab_id->tp_rev
				>= FG_RR_TP_REV_VERSION1)
			&& (chip->pmic_fab_id->tp_rev
				<= FG_RR_TP_REV_VERSION2))
			|| (chip->pmic_fab_id->tp_rev
				>= FG_RR_TP_REV_VERSION3)) {
				chip->volt = div64_s64(chip->volt, 1000);
				chip->volt = chip->volt *
					FG_ADC_RR_CURR_USBIN_660_FACTOR_MIL;
				chip->volt = FG_ADC_RR_CURR_USBIN_660_UV_VAL -
					(chip->volt);
				chip->volt = div64_s64(1000000000, chip->volt);
				scale = chip->volt;
			} else
				scale = FG_ADC_RR_CURR_USBIN_INPUT_FACTOR_MIL;
			break;
		case PMI8998_SUBTYPE:
			scale = FG_ADC_RR_CURR_USBIN_INPUT_FACTOR_MIL;
			break;
		default:
			pr_err("No PMIC subtype found\n");
			return -EINVAL;
		}
	}

	/* scale * V/A; 2.5V ADC full scale */
	ua = ((int64_t)adc_code * scale);
	ua *= (FG_ADC_RR_FS_VOLTAGE_MV * FG_ADC_SCALE_MILLI_FACTOR);
	ua = div64_s64(ua, (FG_MAX_ADC_READINGS * 1000));
	*result_ua = ua;

	return 0;
}

static int rradc_post_process_dcin_curr(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_ua)
{
	int64_t ua = 0;

	if (!prop)
		return -EINVAL;

	/* 0.5 V/A; 2.5V ADC full scale */
	ua = ((int64_t)adc_code * FG_ADC_RR_CURR_INPUT_FACTOR);
	ua *= (FG_ADC_RR_FS_VOLTAGE_MV * FG_ADC_SCALE_MILLI_FACTOR);
	ua = div64_s64(ua, (FG_MAX_ADC_READINGS * 1000));
	*result_ua = ua;

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
	temp += FG_ADC_RR_DIE_TEMP_OFFSET_MILLI_DEGC;
	*result_millidegc = temp;

	return 0;
}

static int rradc_get_660_fab_coeff(struct rradc_chip *chip,
		int64_t *offset, int64_t *slope)
{
	switch (chip->pmic_fab_id->fab_id) {
	case PM660_FAB_ID_GF:
		*offset = FG_ADC_RR_CHG_TEMP_660_GF_OFFSET_UV;
		*slope = FG_RR_CHG_TEMP_660_GF_SLOPE_UV_PER_C;
		break;
	case PM660_FAB_ID_TSMC:
		*offset = FG_ADC_RR_CHG_TEMP_660_SMIC_OFFSET_UV;
		*slope = FG_RR_CHG_TEMP_660_SMIC_SLOPE_UV_PER_C;
		break;
	default:
		*offset = FG_ADC_RR_CHG_TEMP_660_MGNA_OFFSET_UV;
		*slope = FG_RR_CHG_TEMP_660_MGNA_SLOPE_UV_PER_C;
	}

	return 0;
}

static int rradc_get_8998_fab_coeff(struct rradc_chip *chip,
		int64_t *offset, int64_t *slope)
{
	switch (chip->pmic_fab_id->fab_id) {
	case PMI8998_FAB_ID_GF:
		*offset = FG_ADC_RR_CHG_TEMP_GF_OFFSET_UV;
		*slope = FG_ADC_RR_CHG_TEMP_GF_SLOPE_UV_PER_C;
		break;
	case PMI8998_FAB_ID_SMIC:
		*offset = FG_ADC_RR_CHG_TEMP_SMIC_OFFSET_UV;
		*slope = FG_ADC_RR_CHG_TEMP_SMIC_SLOPE_UV_PER_C;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rradc_post_process_chg_temp_hot(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_millidegc)
{
	int64_t uv = 0, offset = 0, slope = 0;
	int rc = 0;

	if (chip->revid_dev_node) {
		switch (chip->pmic_fab_id->pmic_subtype) {
		case PM660_SUBTYPE:
			rc = rradc_get_660_fab_coeff(chip, &offset, &slope);
			if (rc < 0) {
				pr_err("Unable to get fab id coefficients\n");
				return -EINVAL;
			}
			break;
		case PMI8998_SUBTYPE:
			rc = rradc_get_8998_fab_coeff(chip, &offset, &slope);
			if (rc < 0) {
				pr_err("Unable to get fab id coefficients\n");
				return -EINVAL;
			}
			break;
		default:
			pr_err("No PMIC subtype found\n");
			return -EINVAL;
		}
	} else {
		pr_err("No temperature scaling coefficients\n");
		return -EINVAL;
	}

	uv = (int64_t) adc_code * FG_ADC_RR_CHG_THRESHOLD_SCALE;
	uv = uv * FG_ADC_RR_TEMP_FS_VOLTAGE_NUM;
	uv = div64_s64(uv, (FG_ADC_RR_TEMP_FS_VOLTAGE_DEN *
					FG_MAX_ADC_READINGS));
	uv = offset - uv;
	uv = div64_s64((uv * FG_ADC_SCALE_MILLI_FACTOR), slope);
	uv = uv + FG_ADC_RR_CHG_TEMP_OFFSET_MILLI_DEGC;
	*result_millidegc = uv;

	return 0;
}

static int rradc_post_process_skin_temp_hot(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_millidegc)
{
	int64_t temp = 0;

	temp = (int64_t) adc_code;
	temp = div64_s64(temp, 2);
	temp = temp - 30;
	temp *= FG_ADC_SCALE_MILLI_FACTOR;
	*result_millidegc = temp;

	return 0;
}

static int rradc_post_process_chg_temp(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 adc_code,
			int *result_millidegc)
{
	int64_t uv = 0, offset = 0, slope = 0;
	int rc = 0;

	if (chip->revid_dev_node) {
		switch (chip->pmic_fab_id->pmic_subtype) {
		case PM660_SUBTYPE:
			rc = rradc_get_660_fab_coeff(chip, &offset, &slope);
			if (rc < 0) {
				pr_err("Unable to get fab id coefficients\n");
				return -EINVAL;
			}
			break;
		case PMI8998_SUBTYPE:
			rc = rradc_get_8998_fab_coeff(chip, &offset, &slope);
			if (rc < 0) {
				pr_err("Unable to get fab id coefficients\n");
				return -EINVAL;
			}
			break;
		default:
			pr_err("No PMIC subtype found\n");
			return -EINVAL;
		}
	} else {
		pr_err("No temperature scaling coefficients\n");
		return -EINVAL;
	}

	uv = ((int64_t) adc_code * FG_ADC_RR_TEMP_FS_VOLTAGE_NUM);
	uv = div64_s64(uv, (FG_ADC_RR_TEMP_FS_VOLTAGE_DEN *
					FG_MAX_ADC_READINGS));
	uv = offset - uv;
	uv = div64_s64((uv * FG_ADC_SCALE_MILLI_FACTOR), slope);
	uv += FG_ADC_RR_CHG_TEMP_OFFSET_MILLI_DEGC;
	*result_millidegc = uv;

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

#define RR_ADC_CHAN(_dname, _type, _mask, _scale, _lsb, _msb, _sts)	\
	{								\
		.datasheet_name = (_dname),				\
		.type = _type,						\
		.info_mask = _mask,					\
		.scale = _scale,					\
		.lsb = _lsb,						\
		.msb = _msb,						\
		.sts = _sts,						\
	},								\

#define RR_ADC_CHAN_TEMP(_dname, _scale, mask, _lsb, _msb, _sts)	\
	RR_ADC_CHAN(_dname, IIO_TEMP,					\
		mask,							\
		_scale, _lsb, _msb, _sts)				\

#define RR_ADC_CHAN_VOLT(_dname, _scale, _lsb, _msb, _sts)		\
	RR_ADC_CHAN(_dname, IIO_VOLTAGE,				\
		  BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),\
		  _scale, _lsb, _msb, _sts)				\

#define RR_ADC_CHAN_CURRENT(_dname, _scale, _lsb, _msb, _sts)		\
	RR_ADC_CHAN(_dname, IIO_CURRENT,				\
		  BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),\
		  _scale, _lsb, _msb, _sts)				\

#define RR_ADC_CHAN_RESISTANCE(_dname, _scale, _lsb, _msb, _sts)	\
	RR_ADC_CHAN(_dname, IIO_RESISTANCE,				\
		  BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),\
		  _scale, _lsb, _msb, _sts)				\

static const struct rradc_channels rradc_chans[] = {
	RR_ADC_CHAN_RESISTANCE("batt_id", rradc_post_process_batt_id,
			FG_ADC_RR_BATT_ID_5_LSB, FG_ADC_RR_BATT_ID_5_MSB,
			FG_ADC_RR_BATT_ID_STS)
	RR_ADC_CHAN_TEMP("batt_therm", &rradc_post_process_therm,
			BIT(IIO_CHAN_INFO_RAW),
			FG_ADC_RR_BATT_THERM_LSB, FG_ADC_RR_BATT_THERM_MSB,
			FG_ADC_RR_BATT_THERM_STS)
	RR_ADC_CHAN_TEMP("skin_temp", &rradc_post_process_therm,
			BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),
			FG_ADC_RR_SKIN_TEMP_LSB, FG_ADC_RR_SKIN_TEMP_MSB,
			FG_ADC_RR_AUX_THERM_STS)
	RR_ADC_CHAN_CURRENT("usbin_i", &rradc_post_process_usbin_curr,
			FG_ADC_RR_USB_IN_I_LSB, FG_ADC_RR_USB_IN_I_MSB,
			FG_ADC_RR_USB_IN_I_STS)
	RR_ADC_CHAN_VOLT("usbin_v", &rradc_post_process_volt,
			FG_ADC_RR_USB_IN_V_LSB, FG_ADC_RR_USB_IN_V_MSB,
			FG_ADC_RR_USB_IN_V_STS)
	RR_ADC_CHAN_CURRENT("dcin_i", &rradc_post_process_dcin_curr,
			FG_ADC_RR_DC_IN_I_LSB, FG_ADC_RR_DC_IN_I_MSB,
			FG_ADC_RR_DC_IN_I_STS)
	RR_ADC_CHAN_VOLT("dcin_v", &rradc_post_process_volt,
			FG_ADC_RR_DC_IN_V_LSB, FG_ADC_RR_DC_IN_V_MSB,
			FG_ADC_RR_DC_IN_V_STS)
	RR_ADC_CHAN_TEMP("die_temp", &rradc_post_process_die_temp,
			BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),
			FG_ADC_RR_PMI_DIE_TEMP_LSB, FG_ADC_RR_PMI_DIE_TEMP_MSB,
			FG_ADC_RR_PMI_DIE_TEMP_STS)
	RR_ADC_CHAN_TEMP("chg_temp", &rradc_post_process_chg_temp,
			BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),
			FG_ADC_RR_CHARGER_TEMP_LSB, FG_ADC_RR_CHARGER_TEMP_MSB,
			FG_ADC_RR_CHARGER_TEMP_STS)
	RR_ADC_CHAN_VOLT("gpio", &rradc_post_process_gpio,
			FG_ADC_RR_GPIO_LSB, FG_ADC_RR_GPIO_MSB,
			FG_ADC_RR_GPIO_STS)
	RR_ADC_CHAN_TEMP("chg_temp_hot", &rradc_post_process_chg_temp_hot,
			BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),
			FG_ADC_RR_CHARGER_HOT, FG_ADC_RR_CHARGER_HOT,
			FG_ADC_RR_CHARGER_TEMP_STS)
	RR_ADC_CHAN_TEMP("chg_temp_too_hot", &rradc_post_process_chg_temp_hot,
			BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),
			FG_ADC_RR_CHARGER_TOO_HOT, FG_ADC_RR_CHARGER_TOO_HOT,
			FG_ADC_RR_CHARGER_TEMP_STS)
	RR_ADC_CHAN_TEMP("skin_temp_hot", &rradc_post_process_skin_temp_hot,
			BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),
			FG_ADC_RR_SKIN_HOT, FG_ADC_RR_SKIN_HOT,
			FG_ADC_RR_AUX_THERM_STS)
	RR_ADC_CHAN_TEMP("skin_temp_too_hot", &rradc_post_process_skin_temp_hot,
			BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_PROCESSED),
			FG_ADC_RR_SKIN_TOO_HOT, FG_ADC_RR_SKIN_TOO_HOT,
			FG_ADC_RR_AUX_THERM_STS)
};

static int rradc_enable_continuous_mode(struct rradc_chip *chip)
{
	int rc = 0;

	/* Clear channel log */
	rc = rradc_masked_write(chip, FG_ADC_RR_ADC_LOG,
			FG_ADC_RR_ADC_LOG_CLR_CTRL,
			FG_ADC_RR_ADC_LOG_CLR_CTRL);
	if (rc < 0) {
		pr_err("log ctrl update to clear failed:%d\n", rc);
		return rc;
	}

	rc = rradc_masked_write(chip, FG_ADC_RR_ADC_LOG,
		FG_ADC_RR_ADC_LOG_CLR_CTRL, 0);
	if (rc < 0) {
		pr_err("log ctrl update to not clear failed:%d\n", rc);
		return rc;
	}

	/* Switch to continuous mode */
	rc = rradc_masked_write(chip, FG_ADC_RR_RR_ADC_CTL,
		FG_ADC_RR_ADC_CTL_CONTINUOUS_SEL_MASK,
		FG_ADC_RR_ADC_CTL_CONTINUOUS_SEL);
	if (rc < 0) {
		pr_err("Update to continuous mode failed:%d\n", rc);
		return rc;
	}

	return rc;
}

static int rradc_disable_continuous_mode(struct rradc_chip *chip)
{
	int rc = 0;

	/* Switch to non continuous mode */
	rc = rradc_masked_write(chip, FG_ADC_RR_RR_ADC_CTL,
			FG_ADC_RR_ADC_CTL_CONTINUOUS_SEL_MASK, 0);
	if (rc < 0) {
		pr_err("Update to non-continuous mode failed:%d\n", rc);
		return rc;
	}

	return rc;
}

static bool rradc_is_usb_present(struct rradc_chip *chip)
{
	union power_supply_propval pval;
	int rc;
	bool usb_present = false;

	if (!chip->usb_trig) {
		pr_debug("USB property not present\n");
		return usb_present;
	}

	rc = power_supply_get_property(chip->usb_trig,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	usb_present = (rc < 0) ? 0 : pval.intval;

	return usb_present;
}

static int rradc_check_status_ready_with_retry(struct rradc_chip *chip,
		struct rradc_chan_prop *prop, u8 *buf, u16 status)
{
	int rc = 0, retry_cnt = 0, mask = 0;

	switch (prop->channel) {
	case RR_ADC_BATT_ID:
		/* BATT_ID STS bit does not get set initially */
		mask = FG_RR_ADC_STS_CHANNEL_STS;
		break;
	default:
		mask = FG_RR_ADC_STS_CHANNEL_READING_MASK;
		break;
	}

	while (((buf[0] & mask) != mask) &&
			(retry_cnt < FG_RR_CONV_MAX_RETRY_CNT)) {
		pr_debug("%s is not ready; nothing to read:0x%x\n",
			rradc_chans[prop->channel].datasheet_name, buf[0]);

		if (((prop->channel == RR_ADC_CHG_TEMP) ||
			(prop->channel == RR_ADC_SKIN_TEMP) ||
			(prop->channel == RR_ADC_USBIN_I)) &&
					((!rradc_is_usb_present(chip)))) {
			pr_debug("USB not present for %d\n", prop->channel);
			rc = -ENODATA;
			break;
		}

		msleep(FG_RR_CONV_CONTINUOUS_TIME_MIN_MS);
		retry_cnt++;
		rc = rradc_read(chip, status, buf, 1);
		if (rc < 0) {
			pr_err("status read failed:%d\n", rc);
			return rc;
		}
	}

	if (retry_cnt >= FG_RR_CONV_MAX_RETRY_CNT)
		rc = -ENODATA;

	return rc;
}

static int rradc_read_channel_with_continuous_mode(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u8 *buf)
{
	int rc = 0, ret = 0;
	u16 status = 0;

	rc = rradc_enable_continuous_mode(chip);
	if (rc < 0) {
		pr_err("Failed to switch to continuous mode\n");
		return rc;
	}

	status = rradc_chans[prop->channel].sts;
	rc = rradc_read(chip, status, buf, 1);
	if (rc < 0) {
		pr_err("status read failed:%d\n", rc);
		ret = rc;
		goto disable;
	}

	rc = rradc_check_status_ready_with_retry(chip, prop,
						buf, status);
	if (rc < 0) {
		pr_err("Status read failed:%d\n", rc);
		ret = rc;
	}

disable:
	rc = rradc_disable_continuous_mode(chip);
	if (rc < 0) {
		pr_err("Failed to switch to non continuous mode\n");
		ret = rc;
	}

	return ret;
}

static int rradc_enable_batt_id_channel(struct rradc_chip *chip, bool enable)
{
	int rc = 0;

	if (enable) {
		rc = rradc_masked_write(chip, FG_ADC_RR_BATT_ID_CTRL,
				FG_ADC_RR_BATT_ID_CTRL_CHANNEL_CONV,
				FG_ADC_RR_BATT_ID_CTRL_CHANNEL_CONV);
		if (rc < 0) {
			pr_err("Enabling BATT ID channel failed:%d\n", rc);
			return rc;
		}
	} else {
		rc = rradc_masked_write(chip, FG_ADC_RR_BATT_ID_CTRL,
				FG_ADC_RR_BATT_ID_CTRL_CHANNEL_CONV, 0);
		if (rc < 0) {
			pr_err("Disabling BATT ID channel failed:%d\n", rc);
			return rc;
		}
	}

	return rc;
}

static int rradc_do_batt_id_conversion(struct rradc_chip *chip,
		struct rradc_chan_prop *prop, u16 *data, u8 *buf)
{
	int rc = 0, ret = 0, batt_id_delay;

	rc = rradc_enable_batt_id_channel(chip, true);
	if (rc < 0) {
		pr_err("Enabling BATT ID channel failed:%d\n", rc);
		return rc;
	}

	if (chip->batt_id_delay != -EINVAL) {
		batt_id_delay = chip->batt_id_delay << BATT_ID_SETTLE_SHIFT;
		rc = rradc_masked_write(chip, FG_ADC_RR_BATT_ID_CFG,
				batt_id_delay, batt_id_delay);
		if (rc < 0)
			pr_err("BATT_ID settling time config failed:%d\n", rc);
	}

	rc = rradc_masked_write(chip, FG_ADC_RR_BATT_ID_TRIGGER,
				FG_ADC_RR_BATT_ID_TRIGGER_CTL,
				FG_ADC_RR_BATT_ID_TRIGGER_CTL);
	if (rc < 0) {
		pr_err("BATT_ID trigger set failed:%d\n", rc);
		ret = rc;
		rc = rradc_enable_batt_id_channel(chip, false);
		if (rc < 0)
			pr_err("Disabling BATT ID channel failed:%d\n", rc);
		return ret;
	}

	rc = rradc_read_channel_with_continuous_mode(chip, prop, buf);
	if (rc < 0) {
		pr_err("Error reading in continuous mode:%d\n", rc);
		ret = rc;
	}

	rc = rradc_masked_write(chip, FG_ADC_RR_BATT_ID_TRIGGER,
			FG_ADC_RR_BATT_ID_TRIGGER_CTL, 0);
	if (rc < 0) {
		pr_err("BATT_ID trigger re-set failed:%d\n", rc);
		ret = rc;
	}

	rc = rradc_enable_batt_id_channel(chip, false);
	if (rc < 0) {
		pr_err("Disabling BATT ID channel failed:%d\n", rc);
		ret = rc;
	}

	return ret;
}

static int rradc_do_conversion(struct rradc_chip *chip,
			struct rradc_chan_prop *prop, u16 *data)
{
	int rc = 0, bytes_to_read = 0;
	u8 buf[6];
	u16 offset = 0, batt_id_5 = 0, batt_id_15 = 0, batt_id_150 = 0;
	u16 status = 0;

	mutex_lock(&chip->lock);

	switch (prop->channel) {
	case RR_ADC_BATT_ID:
		rc = rradc_do_batt_id_conversion(chip, prop, data, buf);
		if (rc < 0) {
			pr_err("Battery ID conversion failed:%d\n", rc);
			goto fail;
		}
		break;
	case RR_ADC_USBIN_V:
		/* Force conversion every cycle */
		rc = rradc_masked_write(chip, FG_ADC_RR_USB_IN_V_TRIGGER,
				FG_ADC_RR_USB_IN_V_EVERY_CYCLE_MASK,
				FG_ADC_RR_USB_IN_V_EVERY_CYCLE);
		if (rc < 0) {
			pr_err("Force every cycle update failed:%d\n", rc);
			goto fail;
		}

		rc = rradc_read_channel_with_continuous_mode(chip, prop, buf);
		if (rc < 0) {
			pr_err("Error reading in continuous mode:%d\n", rc);
			goto fail;
		}

		/* Restore usb_in trigger */
		rc = rradc_masked_write(chip, FG_ADC_RR_USB_IN_V_TRIGGER,
				FG_ADC_RR_USB_IN_V_EVERY_CYCLE_MASK, 0);
		if (rc < 0) {
			pr_err("Restore every cycle update failed:%d\n", rc);
			goto fail;
		}
		break;
	case RR_ADC_DIE_TEMP:
		/* Force conversion every cycle */
		rc = rradc_masked_write(chip, FG_ADC_RR_PMI_DIE_TEMP_TRIGGER,
				FG_ADC_RR_USB_IN_V_EVERY_CYCLE_MASK,
				FG_ADC_RR_USB_IN_V_EVERY_CYCLE);
		if (rc < 0) {
			pr_err("Force every cycle update failed:%d\n", rc);
			goto fail;
		}

		rc = rradc_read_channel_with_continuous_mode(chip, prop, buf);
		if (rc < 0) {
			pr_err("Error reading in continuous mode:%d\n", rc);
			goto fail;
		}

		/* Restore aux_therm trigger */
		rc = rradc_masked_write(chip, FG_ADC_RR_PMI_DIE_TEMP_TRIGGER,
				FG_ADC_RR_USB_IN_V_EVERY_CYCLE_MASK, 0);
		if (rc < 0) {
			pr_err("Restore every cycle update failed:%d\n", rc);
			goto fail;
		}
		break;
	case RR_ADC_CHG_HOT_TEMP:
	case RR_ADC_CHG_TOO_HOT_TEMP:
	case RR_ADC_SKIN_HOT_TEMP:
	case RR_ADC_SKIN_TOO_HOT_TEMP:
		pr_debug("Read only the data registers\n");
		break;
	default:
		status = rradc_chans[prop->channel].sts;
		rc = rradc_read(chip, status, buf, 1);
		if (rc < 0) {
			pr_err("status read failed:%d\n", rc);
			goto fail;
		}

		rc = rradc_check_status_ready_with_retry(chip, prop,
						buf, status);
		if (rc < 0) {
			pr_debug("Status read failed:%d\n", rc);
			rc = -ENODATA;
			goto fail;
		}
		break;
	}

	offset = rradc_chans[prop->channel].lsb;
	if (prop->channel == RR_ADC_BATT_ID)
		bytes_to_read = 6;
	else if ((prop->channel == RR_ADC_CHG_HOT_TEMP) ||
		(prop->channel == RR_ADC_CHG_TOO_HOT_TEMP) ||
		(prop->channel == RR_ADC_SKIN_HOT_TEMP) ||
		(prop->channel == RR_ADC_SKIN_TOO_HOT_TEMP))
		bytes_to_read = 1;
	else
		bytes_to_read = 2;

	buf[0] = 0;
	rc = rradc_read(chip, offset, buf, bytes_to_read);
	if (rc) {
		pr_err("read data failed\n");
		goto fail;
	}

	if (prop->channel == RR_ADC_BATT_ID) {
		batt_id_150 = (buf[5] << 8) | buf[4];
		batt_id_15 = (buf[3] << 8) | buf[2];
		batt_id_5 = (buf[1] << 8) | buf[0];
		if ((!batt_id_150) && (!batt_id_15) && (!batt_id_5)) {
			pr_err("Invalid batt_id values with all zeros\n");
			rc = -EINVAL;
			goto fail;
		}

		if (batt_id_150 <= FG_ADC_RR_BATT_ID_RANGE) {
			pr_debug("Batt_id_150 is chosen\n");
			*data = batt_id_150;
			prop->channel_data = FG_ADC_RR_BATT_ID_150_MA;
		} else if (batt_id_15 <= FG_ADC_RR_BATT_ID_RANGE) {
			pr_debug("Batt_id_15 is chosen\n");
			*data = batt_id_15;
			prop->channel_data = FG_ADC_RR_BATT_ID_15_MA;
		} else {
			pr_debug("Batt_id_5 is chosen\n");
			*data = batt_id_5;
			prop->channel_data = FG_ADC_RR_BATT_ID_5_MA;
		}
	} else if ((prop->channel == RR_ADC_CHG_HOT_TEMP) ||
		(prop->channel == RR_ADC_CHG_TOO_HOT_TEMP) ||
		(prop->channel == RR_ADC_SKIN_HOT_TEMP) ||
		(prop->channel == RR_ADC_SKIN_TOO_HOT_TEMP)) {
		*data = buf[0];
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

	if (chan->address >= RR_ADC_MAX) {
		pr_err("Invalid channel index:%ld\n", chan->address);
		return -EINVAL;
	}

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		if (((chip->pmic_fab_id->tp_rev
				>= FG_RR_TP_REV_VERSION1)
		&& (chip->pmic_fab_id->tp_rev
				<= FG_RR_TP_REV_VERSION2))
		|| (chip->pmic_fab_id->tp_rev
				>= FG_RR_TP_REV_VERSION3)) {
			if (chan->address == RR_ADC_USBIN_I) {
				prop = &chip->chan_props[RR_ADC_USBIN_V];
				rc = rradc_do_conversion(chip, prop, &adc_code);
				if (rc)
					break;
				prop->scale(chip, prop, adc_code, &chip->volt);
			}
		}

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
	unsigned int i = 0, base;
	int rc = 0;
	struct rradc_chan_prop prop;

	chip->nchannels = RR_ADC_MAX;
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

	chip->batt_id_delay = -EINVAL;

	rc = of_property_read_u32(node, "qcom,batt-id-delay-ms",
			&chip->batt_id_delay);
	if (!rc) {
		for (i = 0; i < RRADC_BATT_ID_DELAY_MAX; i++) {
			if (chip->batt_id_delay == batt_id_delays[i])
				break;
		}
		if (i == RRADC_BATT_ID_DELAY_MAX)
			pr_err("Invalid batt_id_delay, rc=%d\n", rc);
		else
			chip->batt_id_delay = i;
	}

	chip->base = base;
	chip->revid_dev_node = of_parse_phandle(node, "qcom,pmic-revid", 0);
	if (chip->revid_dev_node) {
		chip->pmic_fab_id = get_revid_data(chip->revid_dev_node);
		if (IS_ERR(chip->pmic_fab_id)) {
			rc = PTR_ERR(chip->pmic_fab_id);
			if (rc != -EPROBE_DEFER)
				pr_err("Unable to get pmic_revid rc=%d\n", rc);
			return rc;
		}

		if (!chip->pmic_fab_id)
			return -EINVAL;

		if (chip->pmic_fab_id->fab_id == -EINVAL) {
			rc = chip->pmic_fab_id->fab_id;
			pr_debug("Unable to read fabid rc=%d\n", rc);
		}
	}

	iio_chan = chip->iio_chans;

	for (i = 0; i < RR_ADC_MAX; i++) {
		prop.channel = i;
		prop.scale = rradc_chans[i].scale;
		/* Private channel data used for selecting batt_id */
		prop.channel_data = 0;
		chip->chan_props[i] = prop;

		rradc_chan = &rradc_chans[i];

		iio_chan->channel = prop.channel;
		iio_chan->datasheet_name = rradc_chan->datasheet_name;
		iio_chan->extend_name = rradc_chan->datasheet_name;
		iio_chan->info_mask_separate = rradc_chan->info_mask;
		iio_chan->type = rradc_chan->type;
		iio_chan->address = i;
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

	chip->usb_trig = power_supply_get_by_name("usb");
	if (!chip->usb_trig)
		pr_debug("Error obtaining usb power supply\n");

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
