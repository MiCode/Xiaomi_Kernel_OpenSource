/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "TADC: %s: " fmt, __func__

#include <linux/iio/iio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/pmic-voter.h>

#define USB_PRESENT_VOTER			"USB_PRESENT_VOTER"
#define SLEEP_VOTER				"SLEEP_VOTER"
#define SHUTDOWN_VOTER				"SHUTDOWN_VOTER"
#define TADC_REVISION1_REG			0x00
#define TADC_REVISION2_REG			0x01
#define TADC_REVISION3_REG			0x02
#define TADC_REVISION4_REG			0x03
#define TADC_PERPH_TYPE_REG			0x04
#define TADC_PERPH_SUBTYPE_REG			0x05

/* TADC register definitions */
#define TADC_SW_CH_CONV_REG(chip)		(chip->tadc_base + 0x06)
#define TADC_MBG_ERR_REG(chip)			(chip->tadc_base + 0x07)
#define TADC_EN_CTL_REG(chip)			(chip->tadc_base + 0x46)
#define TADC_CONV_REQ_REG(chip)			(chip->tadc_base + 0x51)
#define TADC_HWTRIG_CONV_CH_EN_REG(chip)	(chip->tadc_base + 0x52)
#define TADC_HW_SETTLE_DELAY_REG(chip)		(chip->tadc_base + 0x53)
#define TADC_LONG_HW_SETTLE_DLY_EN_REG(chip)	(chip->tadc_base + 0x54)
#define TADC_LONG_HW_SETTLE_DLY_REG(chip)	(chip->tadc_base + 0x55)
#define TADC_ADC_BUF_CH_REG(chip)		(chip->tadc_base + 0x56)
#define TADC_ADC_AAF_CH_REG(chip)		(chip->tadc_base + 0x57)
#define TADC_ADC_DATA_RDBK_REG(chip)		(chip->tadc_base + 0x58)
#define TADC_CH1_ADC_LO_REG(chip)		(chip->tadc_base + 0x60)
#define TADC_CH1_ADC_HI_REG(chip)		(chip->tadc_base + 0x61)
#define TADC_CH2_ADC_LO_REG(chip)		(chip->tadc_base + 0x62)
#define TADC_CH2_ADC_HI_REG(chip)		(chip->tadc_base + 0x63)
#define TADC_CH3_ADC_LO_REG(chip)		(chip->tadc_base + 0x64)
#define TADC_CH3_ADC_HI_REG(chip)		(chip->tadc_base + 0x65)
#define TADC_CH4_ADC_LO_REG(chip)		(chip->tadc_base + 0x66)
#define TADC_CH4_ADC_HI_REG(chip)		(chip->tadc_base + 0x67)
#define TADC_CH5_ADC_LO_REG(chip)		(chip->tadc_base + 0x68)
#define TADC_CH5_ADC_HI_REG(chip)		(chip->tadc_base + 0x69)
#define TADC_CH6_ADC_LO_REG(chip)		(chip->tadc_base + 0x70)
#define TADC_CH6_ADC_HI_REG(chip)		(chip->tadc_base + 0x71)
#define TADC_CH7_ADC_LO_REG(chip)		(chip->tadc_base + 0x72)
#define TADC_CH7_ADC_HI_REG(chip)		(chip->tadc_base + 0x73)
#define TADC_CH8_ADC_LO_REG(chip)		(chip->tadc_base + 0x74)
#define TADC_CH8_ADC_HI_REG(chip)		(chip->tadc_base + 0x75)
#define TADC_ADC_DIRECT_TST(chip)		(chip->tadc_base + 0xE7)

/* TADC_CMP register definitions */
#define TADC_CMP_THR1_CMP_REG(chip)		(chip->tadc_cmp_base + 0x51)
#define TADC_CMP_THR1_CH1_CMP_LO_REG(chip)	(chip->tadc_cmp_base + 0x52)
#define TADC_CMP_THR1_CH1_CMP_HI_REG(chip)	(chip->tadc_cmp_base + 0x53)
#define TADC_CMP_THR1_CH2_CMP_LO_REG(chip)	(chip->tadc_cmp_base + 0x54)
#define TADC_CMP_THR1_CH2_CMP_HI_REG(chip)	(chip->tadc_cmp_base + 0x55)
#define TADC_CMP_THR1_CH3_CMP_LO_REG(chip)	(chip->tadc_cmp_base + 0x56)
#define TADC_CMP_THR1_CH3_CMP_HI_REG(chip)	(chip->tadc_cmp_base + 0x57)
#define TADC_CMP_THR2_CMP_REG(chip)		(chip->tadc_cmp_base + 0x67)
#define TADC_CMP_THR2_CH1_CMP_LO_REG(chip)	(chip->tadc_cmp_base + 0x68)
#define TADC_CMP_THR2_CH1_CMP_HI_REG(chip)	(chip->tadc_cmp_base + 0x69)
#define TADC_CMP_THR2_CH2_CMP_LO_REG(chip)	(chip->tadc_cmp_base + 0x6A)
#define TADC_CMP_THR2_CH2_CMP_HI_REG(chip)	(chip->tadc_cmp_base + 0x6B)
#define TADC_CMP_THR2_CH3_CMP_LO_REG(chip)	(chip->tadc_cmp_base + 0x6C)
#define TADC_CMP_THR2_CH3_CMP_HI_REG(chip)	(chip->tadc_cmp_base + 0x6D)
#define TADC_CMP_THR3_CMP_REG(chip)		(chip->tadc_cmp_base + 0x7D)
#define TADC_CMP_THR3_CH1_CMP_LO_REG(chip)	(chip->tadc_cmp_base + 0x7E)
#define TADC_CMP_THR3_CH1_CMP_HI_REG(chip)	(chip->tadc_cmp_base + 0x7F)
#define TADC_CMP_THR3_CH2_CMP_LO_REG(chip)	(chip->tadc_cmp_base + 0x80)
#define TADC_CMP_THR3_CH2_CMP_HI_REG(chip)	(chip->tadc_cmp_base + 0x81)
#define TADC_CMP_THR3_CH3_CMP_LO_REG(chip)	(chip->tadc_cmp_base + 0x82)
#define TADC_CMP_THR3_CH3_CMP_HI_REG(chip)	(chip->tadc_cmp_base + 0x83)
#define TADC_CMP_THR4_CMP_REG(chip)		(chip->tadc_cmp_base + 0x93)
#define TADC_CMP_THR4_CH1_CMP_LO_REG(chip)	(chip->tadc_cmp_base + 0x94)
#define TADC_CMP_THR4_CH1_CMP_HI_REG(chip)	(chip->tadc_cmp_base + 0x95)
#define TADC_CMP_THR1_CH1_HYST_REG(chip)	(chip->tadc_cmp_base + 0xB0)
#define TADC_CMP_THR2_CH1_HYST_REG(chip)	(chip->tadc_cmp_base + 0xB1)
#define TADC_CMP_THR3_CH1_HYST_REG(chip)	(chip->tadc_cmp_base + 0xB2)
#define TADC_CMP_THR4_CH1_HYST_REG(chip)	(chip->tadc_cmp_base + 0xB3)

/* 10 bits of resolution */
#define TADC_RESOLUTION			1024
/* number of hardware channels */
#define TADC_NUM_CH			8

enum tadc_chan_id {
	TADC_THERM1 = 0,
	TADC_THERM2,
	TADC_DIE_TEMP,
	TADC_BATT_I,
	TADC_BATT_V,
	TADC_INPUT_I,
	TADC_INPUT_V,
	TADC_OTG_I,
	/* virtual channels */
	TADC_BATT_P,
	TADC_INPUT_P,
	TADC_THERM1_THR1,
	TADC_THERM1_THR2,
	TADC_THERM1_THR3,
	TADC_THERM1_THR4,
	TADC_THERM2_THR1,
	TADC_THERM2_THR2,
	TADC_THERM2_THR3,
	TADC_DIE_TEMP_THR1,
	TADC_DIE_TEMP_THR2,
	TADC_DIE_TEMP_THR3,
	TADC_CHAN_ID_MAX,
};

#define TADC_CHAN(_name, _type, _channel, _info_mask)	\
{							\
	.type			= _type,		\
	.channel		= _channel,		\
	.info_mask_separate	= _info_mask,		\
	.extend_name		= _name,		\
}

#define TADC_THERM_CHAN(_name, _channel)		\
TADC_CHAN(_name, IIO_TEMP, _channel,			\
	BIT(IIO_CHAN_INFO_RAW) |			\
	BIT(IIO_CHAN_INFO_PROCESSED))

#define TADC_TEMP_CHAN(_name, _channel)			\
TADC_CHAN(_name, IIO_TEMP, _channel,			\
	BIT(IIO_CHAN_INFO_RAW) |			\
	BIT(IIO_CHAN_INFO_PROCESSED) |			\
	BIT(IIO_CHAN_INFO_SCALE) |			\
	BIT(IIO_CHAN_INFO_OFFSET))

#define TADC_CURRENT_CHAN(_name, _channel)		\
TADC_CHAN(_name, IIO_CURRENT, _channel,			\
	BIT(IIO_CHAN_INFO_RAW) |			\
	BIT(IIO_CHAN_INFO_PROCESSED) |			\
	BIT(IIO_CHAN_INFO_SCALE))


#define TADC_VOLTAGE_CHAN(_name, _channel)		\
TADC_CHAN(_name, IIO_VOLTAGE, _channel,			\
	BIT(IIO_CHAN_INFO_RAW) |			\
	BIT(IIO_CHAN_INFO_PROCESSED) |			\
	BIT(IIO_CHAN_INFO_SCALE))

#define TADC_POWER_CHAN(_name, _channel)		\
TADC_CHAN(_name, IIO_POWER, _channel,			\
	BIT(IIO_CHAN_INFO_PROCESSED))

static const struct iio_chan_spec tadc_iio_chans[] = {
	[TADC_THERM1]		= TADC_THERM_CHAN(
					"batt", TADC_THERM1),
	[TADC_THERM2]		= TADC_THERM_CHAN(
					"skin", TADC_THERM2),
	[TADC_DIE_TEMP]		= TADC_TEMP_CHAN(
					"die", TADC_DIE_TEMP),
	[TADC_BATT_I]		= TADC_CURRENT_CHAN(
					"batt", TADC_BATT_I),
	[TADC_BATT_V]		= TADC_VOLTAGE_CHAN(
					"batt", TADC_BATT_V),
	[TADC_INPUT_I]		= TADC_CURRENT_CHAN(
					"input", TADC_INPUT_I),
	[TADC_INPUT_V]		= TADC_VOLTAGE_CHAN(
					"input", TADC_INPUT_V),
	[TADC_OTG_I]		= TADC_CURRENT_CHAN(
					"otg", TADC_OTG_I),
	[TADC_BATT_P]		= TADC_POWER_CHAN(
					"batt", TADC_BATT_P),
	[TADC_INPUT_P]		= TADC_POWER_CHAN(
					"input", TADC_INPUT_P),
	[TADC_THERM1_THR1]	= TADC_THERM_CHAN(
					"batt_warm", TADC_THERM1_THR1),
	[TADC_THERM1_THR2]	= TADC_THERM_CHAN(
					"batt_cool", TADC_THERM1_THR2),
	[TADC_THERM1_THR3]	= TADC_THERM_CHAN(
					"batt_cold", TADC_THERM1_THR3),
	[TADC_THERM1_THR4]	= TADC_THERM_CHAN(
					"batt_hot", TADC_THERM1_THR4),
	[TADC_THERM2_THR1]	= TADC_THERM_CHAN(
					"skin_lb", TADC_THERM2_THR1),
	[TADC_THERM2_THR2]	= TADC_THERM_CHAN(
					"skin_ub", TADC_THERM2_THR2),
	[TADC_THERM2_THR3]	= TADC_THERM_CHAN(
					"skin_rst", TADC_THERM2_THR3),
	[TADC_DIE_TEMP_THR1]	= TADC_THERM_CHAN(
					"die_lb", TADC_DIE_TEMP_THR1),
	[TADC_DIE_TEMP_THR2]	= TADC_THERM_CHAN(
					"die_ub", TADC_DIE_TEMP_THR2),
	[TADC_DIE_TEMP_THR3]	= TADC_THERM_CHAN(
					"die_rst", TADC_DIE_TEMP_THR3),
};

struct tadc_therm_thr {
	int	addr_lo;
	int	addr_hi;
};

struct tadc_chan_data {
	s32			scale;
	s32			offset;
	u32			rbias;
	const struct tadc_pt	*table;
	size_t			tablesize;
	struct tadc_therm_thr	thr[4];
};

struct tadc_chip {
	struct device		*dev;
	struct regmap		*regmap;
	u32			tadc_base;
	u32			tadc_cmp_base;
	struct tadc_chan_data	chans[TADC_NUM_CH];
	struct completion	eoc_complete;
	struct mutex		write_lock;
	struct mutex		conv_lock;
	struct power_supply	*usb_psy;
	struct votable		*tadc_disable_votable;
	struct work_struct	status_change_work;
	struct notifier_block	nb;
	u8			hwtrig_conv;
};

struct tadc_pt {
	s32 x;
	s32 y;
};

/*
 * Thermistor tables are generated by the B-parameter equation which is a
 * simplifed version of the Steinhart-Hart equation.
 *
 * (1 / T) = (1 / T0) + (1 / B) * ln(R / R0)
 *
 * Where R0 is the resistance at temperature T0, and T0 is typically room
 * temperature (25C).
 */
static const struct tadc_pt tadc_therm_3450b_68k[] = {
	{ 4151,		120000 },
	{ 4648,		115000 },
	{ 5220,		110000 },
	{ 5880,		105000 },
	{ 6644,		100000 },
	{ 7533,		95000 },
	{ 8571,		90000 },
	{ 9786,		85000 },
	{ 11216,	80000 },
	{ 12906,	75000 },
	{ 14910,	70000 },
	{ 17300,	65000 },
	{ 20163,	60000 },
	{ 23609,	55000 },
	{ 27780,	50000 },
	{ 32855,	45000 },
	{ 39065,	40000 },
	{ 46712,	35000 },
	{ 56185,	30000 },
	{ 68000,	25000 },
	{ 82837,	20000 },
	{ 101604,	15000 },
	{ 125525,	10000 },
	{ 156261,	5000 },
	{ 196090,	0 },
	{ 248163,	-5000 },
	{ 316887,	-10000 },
	{ 408493,	-15000 },
	{ 531889,	-20000 },
	{ 699966,	-25000 },
	{ 931618,	-30000 },
	{ 1254910,	-35000 },
	{ 1712127,	-40000 },
};

static bool tadc_is_reg_locked(struct tadc_chip *chip, u16 reg)
{
	if ((reg & 0xFF00) == chip->tadc_cmp_base)
		return true;

	if (reg >= TADC_HWTRIG_CONV_CH_EN_REG(chip))
		return true;

	return false;
}

static int tadc_read(struct tadc_chip *chip, u16 reg, u8 *val, size_t count)
{
	int rc = 0;

	rc = regmap_bulk_read(chip->regmap, reg, val, count);
	if (rc < 0)
		pr_err("Couldn't read 0x%04x rc=%d\n", reg, rc);

	return rc;
}

static int tadc_write(struct tadc_chip *chip, u16 reg, u8 data)
{
	int rc = 0;

	mutex_lock(&chip->write_lock);
	if (tadc_is_reg_locked(chip, reg)) {
		rc = regmap_write(chip->regmap, (reg & 0xFF00) | 0xD0, 0xA5);
		if (rc < 0) {
			pr_err("Couldn't unlock secure register rc=%d\n", rc);
			goto unlock;
		}
	}

	rc = regmap_write(chip->regmap, reg, data);
	if (rc < 0) {
		pr_err("Couldn't write 0x%02x to 0x%04x rc=%d\n",
								data, reg, rc);
		goto unlock;
	}

unlock:
	mutex_unlock(&chip->write_lock);
	return rc;
}
static int tadc_bulk_write(struct tadc_chip *chip, u16 reg, u8 *data,
								size_t count)
{
	int rc = 0, i;

	mutex_lock(&chip->write_lock);
	for (i = 0; i < count; ++i, ++reg) {
		if (tadc_is_reg_locked(chip, reg)) {
			rc = regmap_write(chip->regmap,
						(reg & 0xFF00) | 0xD0, 0xA5);
			if (rc < 0) {
				pr_err("Couldn't unlock secure register rc=%d\n",
								rc);
				goto unlock;
			}
		}

		rc = regmap_write(chip->regmap, reg, data[i]);
		if (rc < 0) {
			pr_err("Couldn't write 0x%02x to 0x%04x rc=%d\n",
							data[i], reg, rc);
			goto unlock;
		}
	}

unlock:
	mutex_unlock(&chip->write_lock);
	return rc;
}

static int tadc_masked_write(struct tadc_chip *chip, u16 reg, u8 mask, u8 data)
{
	int rc = 0;

	mutex_lock(&chip->write_lock);
	if (tadc_is_reg_locked(chip, reg)) {
		rc = regmap_write(chip->regmap, (reg & 0xFF00) | 0xD0, 0xA5);
		if (rc < 0) {
			pr_err("Couldn't unlock secure register rc=%d\n", rc);
			goto unlock;
		}
	}

	rc = regmap_update_bits(chip->regmap, reg, mask, data);

unlock:
	mutex_unlock(&chip->write_lock);
	return rc;
}

static int tadc_lerp(const struct tadc_pt *pts, size_t size, bool inv,
							s32 input, s32 *output)
{
	int i;
	s64 temp;
	bool ascending;

	if (pts == NULL) {
		pr_err("Table is NULL\n");
		return -EINVAL;
	}

	if (size < 1) {
		pr_err("Table has no entries\n");
		return -ENOENT;
	}

	if (size == 1) {
		*output = inv ? pts[0].x : pts[0].y;
		return 0;
	}

	ascending = inv ? (pts[0].y < pts[1].y) : (pts[0].x < pts[1].x);
	if (ascending ? (input <= (inv ? pts[0].y : pts[0].x)) :
			(input >= (inv ? pts[0].y : pts[0].x))) {
		*output = inv ? pts[0].x : pts[0].y;
		return 0;
	}

	if (ascending ? (input >= (inv ? pts[size - 1].y : pts[size - 1].x)) :
			(input <= (inv ? pts[size - 1].y : pts[size - 1].x))) {
		*output = inv ? pts[size - 1].x : pts[size - 1].y;
		return 0;
	}

	for (i = 1; i < size; i++)
		if (ascending ? (input <= (inv ? pts[i].y : pts[i].x)) :
				(input >= (inv ? pts[i].y : pts[i].x)))
			break;

	if (inv) {
		temp = (s64)(pts[i].x - pts[i - 1].x) *
						(s64)(input - pts[i - 1].y);
		temp = div_s64(temp, pts[i].y - pts[i - 1].y);
		*output = temp + pts[i - 1].x;
	} else {
		temp = (s64)(pts[i].y - pts[i - 1].y) *
						(s64)(input - pts[i - 1].x);
		temp = div_s64(temp, pts[i].x - pts[i - 1].x);
		*output = temp + pts[i - 1].y;
	}

	return 0;
}

/*
 * Process the result of a thermistor reading.
 *
 * The voltage input to the ADC is a result of a voltage divider circuit.
 * Vout = (Rtherm / (Rbias + Rtherm)) * Vbias
 *
 * The ADC value is based on the output voltage of the voltage divider, and the
 * bias voltage.
 * ADC = (Vin * 1024) / Vbias
 *
 * Combine these equations and solve for Rtherm
 * Rtherm = (ADC * Rbias) / (1024 - ADC)
 */
static int tadc_get_processed_therm(const struct tadc_chan_data *chan_data,
							s16 adc, s32 *result)
{
	s32 rtherm;

	rtherm = div_s64((s64)adc * chan_data->rbias, TADC_RESOLUTION - adc);
	return tadc_lerp(chan_data->table, chan_data->tablesize, false, rtherm,
									result);
}

static int tadc_get_raw_therm(const struct tadc_chan_data *chan_data,
							int mdegc, int *result)
{
	int rc;
	s32 rtherm;

	rc = tadc_lerp(chan_data->table, chan_data->tablesize, true, mdegc,
								&rtherm);
	if (rc < 0) {
		pr_err("Couldn't interpolate %d\n rc=%d", mdegc, rc);
		return rc;
	}

	*result = div64_s64((s64)rtherm * TADC_RESOLUTION,
						(s64)chan_data->rbias + rtherm);
	return 0;
}

static int tadc_read_channel(struct tadc_chip *chip, u16 address, int *adc)
{
	u8 val[2];
	int rc;

	rc = tadc_read(chip, address, val, ARRAY_SIZE(val));
	if (rc < 0) {
		pr_err("Couldn't read channel rc=%d\n", rc);
		return rc;
	}

	/* the 10th bit is the sign bit for all channels */
	*adc = sign_extend32(val[0] | val[1] << BITS_PER_BYTE, 10);
	return rc;
}

static int tadc_write_channel(struct tadc_chip *chip, u16 address, int adc)
{
	u8 val[2];
	int rc;

	/* the 10th bit is the sign bit for all channels */
	adc = sign_extend32(adc, 10);
	val[0] = (u8)adc;
	val[1] = (u8)(adc >> BITS_PER_BYTE);
	rc = tadc_bulk_write(chip, address, val, 2);
	if (rc < 0) {
		pr_err("Couldn't write to channel rc=%d\n", rc);
		return rc;
	}

	return rc;
}

#define CONVERSION_TIMEOUT_MS 100
static int tadc_do_conversion(struct tadc_chip *chip, u8 channels, s16 *adc)
{
	unsigned long timeout, timeleft;
	u8 val[TADC_NUM_CH * 2];
	int rc = 0, i;

	mutex_lock(&chip->conv_lock);
	rc = tadc_read(chip, TADC_MBG_ERR_REG(chip), val, 1);
	if (rc < 0) {
		pr_err("Couldn't read mbg error status rc=%d\n", rc);
		goto unlock;
	}

	reinit_completion(&chip->eoc_complete);

	if (get_effective_result(chip->tadc_disable_votable)) {
		/* leave it back in completed state */
		complete_all(&chip->eoc_complete);
		rc = -ENODATA;
		goto unlock;
	}

	if (val[0] != 0) {
		tadc_write(chip, TADC_EN_CTL_REG(chip), 0);
		tadc_write(chip, TADC_EN_CTL_REG(chip), 0x80);
	}

	rc = tadc_write(chip, TADC_CONV_REQ_REG(chip), channels);
	if (rc < 0) {
		pr_err("Couldn't write conversion request rc=%d\n", rc);
		goto unlock;
	}

	timeout = msecs_to_jiffies(CONVERSION_TIMEOUT_MS);
	timeleft = wait_for_completion_timeout(&chip->eoc_complete, timeout);

	if (timeleft == 0) {
		rc = tadc_read(chip, TADC_SW_CH_CONV_REG(chip), val, 1);
		if (rc < 0) {
			pr_err("Couldn't read conversion status rc=%d\n", rc);
			goto unlock;
		}

		/*
		 * check one last time if the channel we are requesting
		 * has completed conversion
		 */
		if (val[0] != channels) {
			rc = -ETIMEDOUT;
			goto unlock;
		}
	}

	rc = tadc_read(chip, TADC_CH1_ADC_LO_REG(chip), val, ARRAY_SIZE(val));
	if (rc < 0) {
		pr_err("Couldn't read adc channels rc=%d\n", rc);
		goto unlock;
	}

	for (i = 0; i < TADC_NUM_CH; i++)
		adc[i] = (s16)(val[i * 2] | (u16)val[i * 2 + 1] << 8);

	pr_debug("Conversion time for channels 0x%x = %dms\n", channels,
			jiffies_to_msecs(timeout - timeleft));

unlock:
	mutex_unlock(&chip->conv_lock);
	return rc;
}

static int tadc_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val, int *val2,
		long mask)
{
	struct tadc_chip *chip = iio_priv(indio_dev);
	struct tadc_chan_data *chan_data = NULL;
	int rc, offset = 0, scale, scale2, scale_type;
	s16 adc[TADC_NUM_CH];

	switch (chan->channel) {
	case TADC_THERM1_THR1:
	case TADC_THERM1_THR2:
	case TADC_THERM1_THR3:
	case TADC_THERM1_THR4:
		chan_data = &chip->chans[TADC_THERM1];
		break;
	case TADC_THERM2_THR1:
	case TADC_THERM2_THR2:
	case TADC_THERM2_THR3:
		chan_data = &chip->chans[TADC_THERM2];
		break;
	case TADC_DIE_TEMP_THR1:
	case TADC_DIE_TEMP_THR2:
	case TADC_DIE_TEMP_THR3:
		chan_data = &chip->chans[TADC_DIE_TEMP];
		break;
	default:
		if (chan->channel >= ARRAY_SIZE(chip->chans)) {
			pr_err("Channel %d is out of bounds\n", chan->channel);
			return -EINVAL;
		}

		chan_data = &chip->chans[chan->channel];
		break;
	}

	if (!chan_data)
		return -EINVAL;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->channel) {
		case TADC_THERM1_THR1:
		case TADC_THERM2_THR1:
		case TADC_DIE_TEMP_THR1:
			rc = tadc_read_channel(chip,
					chan_data->thr[0].addr_lo, val);
			break;
		case TADC_THERM1_THR2:
		case TADC_THERM2_THR2:
		case TADC_DIE_TEMP_THR2:
			rc = tadc_read_channel(chip,
					chan_data->thr[1].addr_lo, val);
			break;
		case TADC_THERM1_THR3:
		case TADC_THERM2_THR3:
		case TADC_DIE_TEMP_THR3:
			rc = tadc_read_channel(chip,
					chan_data->thr[2].addr_lo, val);
			break;
		case TADC_THERM1_THR4:
			rc = tadc_read_channel(chip,
					chan_data->thr[3].addr_lo, val);
			break;
		default:
			rc = tadc_do_conversion(chip, BIT(chan->channel), adc);
			if (rc < 0) {
				if (rc != -ENODATA)
					pr_err("Couldn't read battery current and voltage channels rc=%d\n",
									rc);
				return rc;
			}
			*val = adc[chan->channel];
			break;
		}

		if (rc < 0 && rc != -ENODATA) {
			pr_err("Couldn't read channel %d\n", chan->channel);
			return rc;
		}

		return IIO_VAL_INT;
	case IIO_CHAN_INFO_PROCESSED:
		switch (chan->channel) {
		case TADC_THERM1:
		case TADC_THERM2:
		case TADC_THERM1_THR1:
		case TADC_THERM1_THR2:
		case TADC_THERM1_THR3:
		case TADC_THERM1_THR4:
		case TADC_THERM2_THR1:
		case TADC_THERM2_THR2:
		case TADC_THERM2_THR3:
			rc = tadc_read_raw(indio_dev, chan, val, NULL,
							IIO_CHAN_INFO_RAW);
			if (rc < 0)
				return rc;

			rc = tadc_get_processed_therm(chan_data, *val, val);
			if (rc < 0) {
				pr_err("Couldn't process 0x%04x from channel %d rc=%d\n",
						*val, chan->channel, rc);
				return rc;
			}
			break;
		case TADC_BATT_P:
			rc = tadc_do_conversion(chip,
				BIT(TADC_BATT_I) | BIT(TADC_BATT_V), adc);
			if (rc < 0 && rc != -ENODATA) {
				pr_err("Couldn't read battery current and voltage channels rc=%d\n",
									rc);
				return rc;
			}

			*val = adc[TADC_BATT_I] * adc[TADC_BATT_V];
			break;
		case TADC_INPUT_P:
			rc = tadc_do_conversion(chip,
				BIT(TADC_INPUT_I) | BIT(TADC_INPUT_V), adc);
			if (rc < 0 && rc != -ENODATA) {
				pr_err("Couldn't read input current and voltage channels rc=%d\n",
									rc);
				return rc;
			}

			*val = adc[TADC_INPUT_I] * adc[TADC_INPUT_V];
			break;
		default:
			rc = tadc_read_raw(indio_dev, chan, val, NULL,
							IIO_CHAN_INFO_RAW);
			if (rc < 0)
				return rc;

			/* offset is optional */
			rc = tadc_read_raw(indio_dev, chan, &offset, NULL,
							IIO_CHAN_INFO_OFFSET);
			if (rc < 0)
				return rc;

			scale_type = tadc_read_raw(indio_dev, chan,
					&scale, &scale2, IIO_CHAN_INFO_SCALE);
			switch (scale_type) {
			case IIO_VAL_INT:
				*val = *val * scale + offset;
				break;
			case IIO_VAL_FRACTIONAL:
				*val = div_s64((s64)*val * scale + offset,
									scale2);
				break;
			default:
				return -EINVAL;
			}
			break;
		}

		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		switch (chan->channel) {
		case TADC_DIE_TEMP:
		case TADC_DIE_TEMP_THR1:
		case TADC_DIE_TEMP_THR2:
		case TADC_DIE_TEMP_THR3:
			*val = chan_data->scale;
			return IIO_VAL_INT;
		case TADC_BATT_I:
		case TADC_BATT_V:
		case TADC_INPUT_I:
		case TADC_INPUT_V:
		case TADC_OTG_I:
			*val = chan_data->scale;
			*val2 = TADC_RESOLUTION;
			return IIO_VAL_FRACTIONAL;
		}

		return -EINVAL;
	case IIO_CHAN_INFO_OFFSET:
		*val = chan_data->offset;
		return IIO_VAL_INT;
	}

	return -EINVAL;
}

static int tadc_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val, int val2,
		long mask)
{
	struct tadc_chip *chip = iio_priv(indio_dev);
	const struct tadc_chan_data *chan_data;
	int rc, raw;
	s32 rem;

	switch (chan->channel) {
	case TADC_THERM1_THR1:
	case TADC_THERM1_THR2:
	case TADC_THERM1_THR3:
	case TADC_THERM1_THR4:
		chan_data = &chip->chans[TADC_THERM1];
		break;
	case TADC_THERM2_THR1:
	case TADC_THERM2_THR2:
	case TADC_THERM2_THR3:
		chan_data = &chip->chans[TADC_THERM2];
		break;
	case TADC_DIE_TEMP_THR1:
	case TADC_DIE_TEMP_THR2:
	case TADC_DIE_TEMP_THR3:
		chan_data = &chip->chans[TADC_DIE_TEMP];
		break;
	default:
		if (chan->channel >= ARRAY_SIZE(chip->chans)) {
			pr_err("Channel %d is out of bounds\n", chan->channel);
			return -EINVAL;
		}

		chan_data = &chip->chans[chan->channel];
		break;
	}

	if (!chan_data)
		return -EINVAL;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		switch (chan->channel) {
		case TADC_THERM1_THR1:
		case TADC_THERM1_THR2:
		case TADC_THERM1_THR3:
		case TADC_THERM1_THR4:
		case TADC_THERM2_THR1:
		case TADC_THERM2_THR2:
		case TADC_THERM2_THR3:
			rc = tadc_get_raw_therm(chan_data, val, &raw);
			if (rc < 0) {
				pr_err("Couldn't get raw value rc=%d\n", rc);
				return rc;
			}
			break;
		case TADC_DIE_TEMP_THR1:
		case TADC_DIE_TEMP_THR2:
		case TADC_DIE_TEMP_THR3:
			/* DIV_ROUND_CLOSEST does not like negative numbers */
			raw = div_s64_rem(val - chan_data->offset,
							chan_data->scale, &rem);
			if (abs(rem) >= abs(chan_data->scale / 2))
				raw++;
			break;
		default:
			return -EINVAL;
		}

		rc = tadc_write_raw(indio_dev, chan, raw, 0,
							IIO_CHAN_INFO_RAW);
		if (rc < 0) {
			pr_err("Couldn't write raw rc=%d\n", rc);
			return rc;
		}

		break;
	case IIO_CHAN_INFO_RAW:
		switch (chan->channel) {
		case TADC_THERM1_THR1:
		case TADC_THERM2_THR1:
		case TADC_DIE_TEMP_THR1:
			rc = tadc_write_channel(chip,
					chan_data->thr[0].addr_lo, val);
			break;
		case TADC_THERM1_THR2:
		case TADC_THERM2_THR2:
		case TADC_DIE_TEMP_THR2:
			rc = tadc_write_channel(chip,
					chan_data->thr[1].addr_lo, val);
			break;
		case TADC_THERM1_THR3:
		case TADC_THERM2_THR3:
		case TADC_DIE_TEMP_THR3:
			rc = tadc_write_channel(chip,
					chan_data->thr[2].addr_lo, val);
			break;
		case TADC_THERM1_THR4:
			rc = tadc_write_channel(chip,
					chan_data->thr[3].addr_lo, val);
			break;
		default:
			return -EINVAL;
		}

		if (rc < 0) {
			pr_err("Couldn't write channel %d\n", chan->channel);
			return rc;
		}

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t handle_eoc(int irq, void *dev_id)
{
	struct tadc_chip *chip = dev_id;

	complete_all(&chip->eoc_complete);
	return IRQ_HANDLED;
}

static int tadc_disable_vote_callback(struct votable *votable,
			void *data, int disable, const char *client)
{
	struct tadc_chip *chip = data;
	int rc;
	int timeout;
	unsigned long timeleft;

	if (disable) {
		timeout = msecs_to_jiffies(CONVERSION_TIMEOUT_MS);
		timeleft = wait_for_completion_timeout(&chip->eoc_complete,
				timeout);
		if (timeleft == 0)
			pr_err("Timed out waiting for eoc, disabling hw conversions regardless\n");

		rc = tadc_read(chip, TADC_HWTRIG_CONV_CH_EN_REG(chip),
							&chip->hwtrig_conv, 1);
		if (rc < 0) {
			pr_err("Couldn't save hw conversions rc=%d\n", rc);
			return rc;
		}
		rc = tadc_write(chip, TADC_HWTRIG_CONV_CH_EN_REG(chip), 0x00);
		if (rc < 0) {
			pr_err("Couldn't disable hw conversions rc=%d\n", rc);
			return rc;
		}
		rc = tadc_write(chip, TADC_ADC_DIRECT_TST(chip), 0x80);
		if (rc < 0) {
			pr_err("Couldn't enable direct test mode rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = tadc_write(chip, TADC_ADC_DIRECT_TST(chip), 0x00);
		if (rc < 0) {
			pr_err("Couldn't disable direct test mode rc=%d\n", rc);
			return rc;
		}
		rc = tadc_write(chip, TADC_HWTRIG_CONV_CH_EN_REG(chip),
							chip->hwtrig_conv);
		if (rc < 0) {
			pr_err("Couldn't restore hw conversions rc=%d\n", rc);
			return rc;
		}
	}

	pr_debug("client: %s disable: %d\n", client, disable);
	return 0;
}

static void status_change_work(struct work_struct *work)
{
	struct tadc_chip *chip = container_of(work,
			struct tadc_chip, status_change_work);
	union power_supply_propval pval = {0, };
	int rc;

	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");

	if (!chip->usb_psy) {
		/* treat usb is not present */
		vote(chip->tadc_disable_votable, USB_PRESENT_VOTER, true, 0);
		return;
	}

	rc = power_supply_get_property(chip->usb_psy,
		       POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0) {
		pr_err("Couldn't get present status rc=%d\n", rc);
		/* treat usb is not present */
		vote(chip->tadc_disable_votable, USB_PRESENT_VOTER, true, 0);
		return;
	}

	/* disable if usb is not present */
	vote(chip->tadc_disable_votable, USB_PRESENT_VOTER, !pval.intval, 0);
}

static int tadc_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct tadc_chip *chip = container_of(nb, struct tadc_chip, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "usb") == 0))
		schedule_work(&chip->status_change_work);

	return NOTIFY_OK;
}

static int tadc_register_notifier(struct tadc_chip *chip)
{
	int rc;

	chip->nb.notifier_call = tadc_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static int tadc_suspend(struct device *dev)
{
	struct tadc_chip *chip = dev_get_drvdata(dev);

	vote(chip->tadc_disable_votable, SLEEP_VOTER, true, 0);
	return 0;
}

static int tadc_resume(struct device *dev)
{
	struct tadc_chip *chip = dev_get_drvdata(dev);

	vote(chip->tadc_disable_votable, SLEEP_VOTER, false, 0);
	return 0;
}

static int tadc_set_therm_table(struct tadc_chan_data *chan_data, u32 beta,
				u32 rtherm)
{
	if (beta == 3450 && rtherm == 68000) {
		chan_data->table = tadc_therm_3450b_68k;
		chan_data->tablesize = ARRAY_SIZE(tadc_therm_3450b_68k);
		return 0;
	}

	return -ENOENT;
}

static int tadc_parse_dt(struct tadc_chip *chip)
{
	struct device_node *child, *node;
	struct tadc_chan_data *chan_data;
	u32 chan_id, rtherm, beta;
	int rc = 0;

	node = chip->dev->of_node;
	for_each_available_child_of_node(node, child) {
		rc = of_property_read_u32(child, "reg", &chan_id);
		if (rc < 0) {
			pr_err("Couldn't find channel for %s rc=%d",
							child->name, rc);
			return rc;
		}

		if (chan_id > TADC_NUM_CH - 1) {
			pr_err("Channel %d is out of range [0, %d]\n",
						chan_id, TADC_NUM_CH - 1);
			return -EINVAL;
		}

		chan_data = &chip->chans[chan_id];
		if (chan_id == TADC_THERM1 || chan_id == TADC_THERM2) {
			rc = of_property_read_u32(child,
					"qcom,rbias", &chan_data->rbias);
			if (rc < 0) {
				pr_err("Couldn't read qcom,rbias rc=%d\n", rc);
				return rc;
			}

			rc = of_property_read_u32(child,
					"qcom,beta-coefficient", &beta);
			if (rc < 0) {
				pr_err("Couldn't read qcom,beta-coefficient rc=%d\n",
									rc);
				return rc;
			}

			rc = of_property_read_u32(child,
					"qcom,rtherm-at-25degc", &rtherm);
			if (rc < 0) {
				pr_err("Couldn't read qcom,rtherm-at-25degc rc=%d\n",
					rc);
				return rc;
			}

			rc = tadc_set_therm_table(chan_data, beta, rtherm);
			if (rc < 0) {
				pr_err("Couldn't set therm table rc=%d\n", rc);
				return rc;
			}
		} else {
			rc = of_property_read_s32(child, "qcom,scale",
							&chan_data->scale);
			if (rc < 0) {
				pr_err("Couldn't read scale rc=%d\n", rc);
				return rc;
			}

			of_property_read_s32(child, "qcom,offset",
							&chan_data->offset);
		}
	}

	return rc;
}

static int tadc_init_hw(struct tadc_chip *chip)
{
	int rc;

	chip->chans[TADC_THERM1].thr[0].addr_lo =
					TADC_CMP_THR1_CH1_CMP_LO_REG(chip);
	chip->chans[TADC_THERM1].thr[0].addr_hi =
					TADC_CMP_THR1_CH1_CMP_HI_REG(chip);
	chip->chans[TADC_THERM1].thr[1].addr_lo =
					TADC_CMP_THR2_CH1_CMP_LO_REG(chip);
	chip->chans[TADC_THERM1].thr[1].addr_hi =
					TADC_CMP_THR2_CH1_CMP_HI_REG(chip);
	chip->chans[TADC_THERM1].thr[2].addr_lo =
					TADC_CMP_THR3_CH1_CMP_LO_REG(chip);
	chip->chans[TADC_THERM1].thr[2].addr_hi =
					TADC_CMP_THR3_CH1_CMP_HI_REG(chip);
	chip->chans[TADC_THERM1].thr[3].addr_lo =
					TADC_CMP_THR4_CH1_CMP_LO_REG(chip);
	chip->chans[TADC_THERM1].thr[3].addr_hi =
					TADC_CMP_THR4_CH1_CMP_HI_REG(chip);

	chip->chans[TADC_THERM2].thr[0].addr_lo =
					TADC_CMP_THR1_CH2_CMP_LO_REG(chip);
	chip->chans[TADC_THERM2].thr[0].addr_hi =
					TADC_CMP_THR1_CH2_CMP_HI_REG(chip);
	chip->chans[TADC_THERM2].thr[1].addr_lo =
					TADC_CMP_THR2_CH2_CMP_LO_REG(chip);
	chip->chans[TADC_THERM2].thr[1].addr_hi =
					TADC_CMP_THR2_CH2_CMP_HI_REG(chip);
	chip->chans[TADC_THERM2].thr[2].addr_lo =
					TADC_CMP_THR3_CH2_CMP_LO_REG(chip);
	chip->chans[TADC_THERM2].thr[2].addr_hi =
					TADC_CMP_THR3_CH2_CMP_HI_REG(chip);

	chip->chans[TADC_DIE_TEMP].thr[0].addr_lo =
					TADC_CMP_THR1_CH3_CMP_LO_REG(chip);
	chip->chans[TADC_DIE_TEMP].thr[0].addr_hi =
					TADC_CMP_THR1_CH3_CMP_HI_REG(chip);
	chip->chans[TADC_DIE_TEMP].thr[1].addr_lo =
					TADC_CMP_THR2_CH3_CMP_LO_REG(chip);
	chip->chans[TADC_DIE_TEMP].thr[1].addr_hi =
					TADC_CMP_THR2_CH3_CMP_HI_REG(chip);
	chip->chans[TADC_DIE_TEMP].thr[2].addr_lo =
					TADC_CMP_THR3_CH3_CMP_LO_REG(chip);
	chip->chans[TADC_DIE_TEMP].thr[2].addr_hi =
					TADC_CMP_THR3_CH3_CMP_HI_REG(chip);

	rc = tadc_write(chip, TADC_CMP_THR1_CMP_REG(chip), 0);
	if (rc < 0) {
		pr_err("Couldn't enable hardware triggers rc=%d\n", rc);
		return rc;
	}

	rc = tadc_write(chip, TADC_CMP_THR2_CMP_REG(chip), 0);
	if (rc < 0) {
		pr_err("Couldn't enable hardware triggers rc=%d\n", rc);
		return rc;
	}

	rc = tadc_write(chip, TADC_CMP_THR3_CMP_REG(chip), 0);
	if (rc < 0) {
		pr_err("Couldn't enable hardware triggers rc=%d\n", rc);
		return rc;
	}

	/* enable connector and die temp hardware triggers */
	rc = tadc_masked_write(chip, TADC_HWTRIG_CONV_CH_EN_REG(chip),
					BIT(TADC_THERM2) | BIT(TADC_DIE_TEMP),
					BIT(TADC_THERM2) | BIT(TADC_DIE_TEMP));
	if (rc < 0) {
		pr_err("Couldn't enable hardware triggers rc=%d\n", rc);
		return rc;
	}

	/* save hw triggered conversion configuration */
	rc = tadc_read(chip, TADC_HWTRIG_CONV_CH_EN_REG(chip),
							&chip->hwtrig_conv, 1);
	if (rc < 0) {
		pr_err("Couldn't save hw conversions rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static const struct iio_info tadc_info = {
	.read_raw		= &tadc_read_raw,
	.write_raw		= &tadc_write_raw,
	.driver_module		= THIS_MODULE,
};

static int tadc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct iio_dev *indio_dev;
	struct tadc_chip *chip;
	int rc, irq;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*chip));
	if (!indio_dev)
		return -ENOMEM;

	chip = iio_priv(indio_dev);
	chip->dev = &pdev->dev;
	init_completion(&chip->eoc_complete);

	/*
	 * set the completion in "completed" state so disable of the tadc
	 * can progress
	 */
	complete_all(&chip->eoc_complete);

	rc = of_property_read_u32(node, "reg", &chip->tadc_base);
	if (rc < 0) {
		pr_err("Couldn't read base address rc=%d\n", rc);
		return rc;
	}
	chip->tadc_cmp_base = chip->tadc_base + 0x100;

	mutex_init(&chip->write_lock);
	mutex_init(&chip->conv_lock);
	INIT_WORK(&chip->status_change_work, status_change_work);
	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		pr_err("Couldn't get regmap\n");
		return -ENODEV;
	}

	rc = tadc_parse_dt(chip);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		return rc;
	}

	rc = tadc_init_hw(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize hardware rc=%d\n", rc);
		return rc;
	}

	chip->tadc_disable_votable = create_votable("SMB_TADC_DISABLE",
					VOTE_SET_ANY,
					tadc_disable_vote_callback,
					chip);
	if (IS_ERR(chip->tadc_disable_votable)) {
		rc = PTR_ERR(chip->tadc_disable_votable);
		return rc;
	}
	/* assume usb is not present */
	vote(chip->tadc_disable_votable, USB_PRESENT_VOTER, true, 0);
	vote(chip->tadc_disable_votable, SHUTDOWN_VOTER, false, 0);
	vote(chip->tadc_disable_votable, SLEEP_VOTER, false, 0);

	rc = tadc_register_notifier(chip);
	if (rc < 0) {
		pr_err("Couldn't register notifier=%d\n", rc);
		goto destroy_votable;
	}

	irq = of_irq_get_byname(node, "eoc");
	if (irq < 0) {
		pr_err("Couldn't get eoc irq rc=%d\n", irq);
		goto destroy_votable;
	}

	rc = devm_request_threaded_irq(chip->dev, irq, NULL, handle_eoc,
						IRQF_ONESHOT, "eoc", chip);
	if (rc < 0) {
		pr_err("Couldn't request irq %d rc=%d\n", irq, rc);
		goto destroy_votable;
	}

	indio_dev->dev.parent = chip->dev;
	indio_dev->name = pdev->name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &tadc_info;
	indio_dev->channels = tadc_iio_chans;
	indio_dev->num_channels = ARRAY_SIZE(tadc_iio_chans);

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc < 0) {
		pr_err("Couldn't register IIO device rc=%d\n", rc);
		goto destroy_votable;
	}

	platform_set_drvdata(pdev, chip);
	return 0;

destroy_votable:
	destroy_votable(chip->tadc_disable_votable);
	return rc;
}

static int tadc_remove(struct platform_device *pdev)
{
	struct tadc_chip *chip = platform_get_drvdata(pdev);

	destroy_votable(chip->tadc_disable_votable);
	return 0;
}

static void tadc_shutdown(struct platform_device *pdev)
{
	struct tadc_chip *chip = platform_get_drvdata(pdev);

	vote(chip->tadc_disable_votable, SHUTDOWN_VOTER, true, 0);
}

static const struct dev_pm_ops tadc_pm_ops = {
	.resume		= tadc_resume,
	.suspend	= tadc_suspend,
};

static const struct of_device_id tadc_match_table[] = {
	{ .compatible = "qcom,tadc" },
	{ }
};
MODULE_DEVICE_TABLE(of, tadc_match_table);

static struct platform_driver tadc_driver = {
	.driver		= {
		.name		= "qcom-tadc",
		.of_match_table	= tadc_match_table,
		.pm		= &tadc_pm_ops,
	},
	.probe		= tadc_probe,
	.remove		= tadc_remove,
	.shutdown	= tadc_shutdown,
};
module_platform_driver(tadc_driver);

MODULE_DESCRIPTION("Qualcomm Technologies Inc. TADC driver");
MODULE_LICENSE("GPL v2");
