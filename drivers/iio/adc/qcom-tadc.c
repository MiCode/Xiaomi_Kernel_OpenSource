/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

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
	TADC_THERM2_THR1,
	TADC_DIE_TEMP_THR1,
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
					"batt_hot", TADC_THERM1_THR1),
	[TADC_THERM2_THR1]	= TADC_THERM_CHAN(
					"skin_hot", TADC_THERM2_THR1),
	[TADC_DIE_TEMP_THR1]	= TADC_THERM_CHAN(
					"die_hot", TADC_DIE_TEMP_THR1),
};

struct tadc_chan_data {
	s32 scale;
	s32 offset;
	u32 rbias;
	const struct tadc_pt *table;
	size_t tablesize;
};

struct tadc_chip {
	struct device		*dev;
	struct regmap		*regmap;
	u32			tadc_base;
	u32			tadc_cmp_base;
	struct tadc_chan_data	chans[TADC_NUM_CH];
	struct completion	eoc_complete;
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

static int tadc_read(struct tadc_chip *chip, u16 reg, u8 *val,
		     size_t val_count)
{
	int rc = 0;

	rc = regmap_bulk_read(chip->regmap, reg, val, val_count);
	if (rc < 0)
		pr_err("Couldn't read %04x rc=%d\n", reg, rc);

	return rc;
}

static int tadc_write(struct tadc_chip *chip, u16 reg, u8 data)
{
	int rc = 0;

	rc = regmap_write(chip->regmap, reg, data);
	if (rc < 0)
		pr_err("Couldn't write %02x to %04x rc=%d\n",
		       data, reg, rc);

	return rc;
}

static int tadc_lerp(const struct tadc_pt *pts, size_t tablesize, s32 input,
		     s32 *output)
{
	int i;
	s64 temp;

	if (pts == NULL) {
		pr_err("Table is NULL\n");
		return -EINVAL;
	}

	if (tablesize < 1) {
		pr_err("Table has no entries\n");
		return -ENOENT;
	}

	if (tablesize == 1) {
		*output = pts[0].y;
		return 0;
	}

	if (pts[0].x > pts[1].x) {
		pr_err("Table is not in acending order\n");
		return -EINVAL;
	}

	if (input <= pts[0].x) {
		*output = pts[0].y;
		return 0;
	}

	if (input >= pts[tablesize - 1].x) {
		*output = pts[tablesize - 1].y;
		return 0;
	}

	for (i = 1; i < tablesize; i++)
		if (input <= pts[i].x)
			break;

	temp = (s64)(pts[i].y - pts[i - 1].y) * (s64)(input - pts[i - 1].x);
	temp = div_s64(temp, pts[i].x - pts[i - 1].x);
	*output = temp + pts[i - 1].y;
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
static int tadc_process_therm(const struct tadc_chan_data *chan_data,
			      s16 adc, s32 *result)
{
	s64 rtherm;

	rtherm = (s64)adc * (s64)chan_data->rbias;
	rtherm = div_s64(rtherm, TADC_RESOLUTION - adc);
	return tadc_lerp(chan_data->table, chan_data->tablesize, rtherm,
			 result);
}

static int tadc_read_channel(struct tadc_chip *chip, u16 address, int *adc)
{
	u8 val[2];
	int rc;

	rc = tadc_read(chip, address, val, ARRAY_SIZE(val));
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read channel rc=%d\n", rc);
		return rc;
	}

	*adc = (s16)(val[0] | val[1] << BITS_PER_BYTE);
	return 0;
}

#define CONVERSION_TIMEOUT_MS 100
static int tadc_do_conversion(struct tadc_chip *chip, u8 channels, s16 *adc)
{
	unsigned long timeout, timeleft;
	u8 val[TADC_NUM_CH * 2];
	int rc, i;

	rc = tadc_read(chip, TADC_MBG_ERR_REG(chip), val, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read mbg error status rc=%d\n",
			rc);
		return rc;
	}

	if (val[0] != 0) {
		tadc_write(chip, TADC_EN_CTL_REG(chip), 0);
		tadc_write(chip, TADC_EN_CTL_REG(chip), 0x80);
	}

	rc = tadc_write(chip, TADC_CONV_REQ_REG(chip), channels);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't write conversion request rc=%d\n",
			rc);
		return rc;
	}

	timeout = msecs_to_jiffies(CONVERSION_TIMEOUT_MS);
	timeleft = wait_for_completion_timeout(&chip->eoc_complete, timeout);

	if (timeleft == 0) {
		rc = tadc_read(chip, TADC_SW_CH_CONV_REG(chip), val, 1);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't read conversion status rc=%d\n",
				rc);
			return rc;
		}

		if (val[0] != channels) {
			dev_err(chip->dev, "Conversion timed out\n");
			return -ETIMEDOUT;
		}
	}

	rc = tadc_read(chip, TADC_CH1_ADC_LO_REG(chip), val, ARRAY_SIZE(val));
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read adc channels rc=%d\n",
			rc);
		return rc;
	}

	for (i = 0; i < TADC_NUM_CH; i++)
		adc[i] = (s16)(val[i * 2] | (u16)val[i * 2 + 1] << 8);

	return jiffies_to_msecs(timeout - timeleft);
}

static int tadc_read_raw(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan, int *val, int *val2,
			 long mask)
{
	struct tadc_chip *chip = iio_priv(indio_dev);
	const struct tadc_chan_data *chan_data = &chip->chans[chan->channel];
	int rc = 0, offset = 0, scale, scale2, scale_type;
	s16 adc[TADC_NUM_CH];

	switch (chan->channel) {
	case TADC_THERM1_THR1:
		chan_data = &chip->chans[TADC_THERM1];
		break;
	case TADC_THERM2_THR1:
		chan_data = &chip->chans[TADC_THERM2];
		break;
	case TADC_DIE_TEMP_THR1:
		chan_data = &chip->chans[TADC_DIE_TEMP];
		break;
	default:
		break;
	}

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->channel) {
		case TADC_THERM1_THR1:
			rc = tadc_read_channel(chip,
				TADC_CMP_THR1_CH1_CMP_LO_REG(chip), val);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't read THERM1 threshold rc=%d\n",
					rc);
				return rc;
			}
			break;
		case TADC_THERM2_THR1:
			rc = tadc_read_channel(chip,
				TADC_CMP_THR1_CH2_CMP_LO_REG(chip), val);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't read THERM2 threshold rc=%d\n",
					rc);
				return rc;
			}
			break;
		case TADC_DIE_TEMP_THR1:
			rc = tadc_read_channel(chip,
				TADC_CMP_THR1_CH3_CMP_LO_REG(chip), val);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't read DIE_TEMP threshold rc=%d\n",
					rc);
				return rc;
			}
			break;
		default:
			rc = tadc_do_conversion(chip, BIT(chan->channel), adc);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't read channel %d\n",
					chan->channel);
				return rc;
			}
			*val = adc[chan->channel];
			break;
		}
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_PROCESSED:
		switch (chan->channel) {
		case TADC_THERM1:
		case TADC_THERM2:
		case TADC_THERM1_THR1:
		case TADC_THERM2_THR1:
			rc = tadc_read_raw(indio_dev, chan, val, NULL,
					   IIO_CHAN_INFO_RAW);
			if (rc < 0)
				return rc;

			rc = tadc_process_therm(chan_data, *val, val);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't process 0x%04x from channel %d rc=%d\n",
					*val, chan->channel, rc);
				return rc;
			}
			break;
		case TADC_BATT_P:
			rc = tadc_do_conversion(chip,
				BIT(TADC_BATT_I) | BIT(TADC_BATT_V), adc);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't read battery current and voltage channels\n");
				return rc;
			}

			*val = adc[TADC_BATT_I] * adc[TADC_BATT_V];
			break;
		case TADC_INPUT_P:
			rc = tadc_do_conversion(chip,
				BIT(TADC_INPUT_I) | BIT(TADC_INPUT_V), adc);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't read input current and voltage channels\n");
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

static irqreturn_t handle_eoc(int irq, void *dev_id)
{
	struct tadc_chip *chip = dev_id;

	complete(&chip->eoc_complete);
	return IRQ_HANDLED;
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
			dev_err(chip->dev, "Couldn't find channel for %s rc=%d",
				child->name, rc);
			return rc;
		}

		if (chan_id > TADC_NUM_CH - 1) {
			dev_err(chip->dev, "Channel %d is out of range [0, %d]\n",
				chan_id, TADC_NUM_CH - 1);
			return -EINVAL;
		}

		chan_data = &chip->chans[chan_id];
		switch (chan_id) {
		case TADC_THERM1:
		case TADC_THERM2:
			rc = of_property_read_u32(child,
					"qcom,rbias", &chan_data->rbias);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't read qcom,rbias rc=%d\n",
					rc);
				return rc;
			}

			rc = of_property_read_u32(child,
					"qcom,beta-coefficient", &beta);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't read qcom,beta-coefficient rc=%d\n",
					rc);
				return rc;
			}

			rc = of_property_read_u32(child,
					"qcom,rtherm-at-25degc", &rtherm);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't read qcom,rtherm-at-25degc rc=%d\n",
					rc);
				return rc;
			}

			rc = tadc_set_therm_table(chan_data, beta, rtherm);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't set therm table rc=%d\n",
					rc);
				return rc;
			}
			break;
		default:
			rc = of_property_read_s32(child, "qcom,scale",
						  &chan_data->scale);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't read scale rc=%d\n",
					rc);
				return rc;
			}

			of_property_read_s32(child, "qcom,offset",
					     &chan_data->offset);
			break;
		}
	}

	return rc;
}

static const struct iio_info tadc_info = {
	.read_raw		= &tadc_read_raw,
	.driver_module		= THIS_MODULE,
};

static int tadc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct iio_dev *indio_dev;
	struct tadc_chip *chip;
	int rc = 0, irq;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*chip));
	if (!indio_dev)
		return -ENOMEM;

	chip = iio_priv(indio_dev);
	chip->dev = &pdev->dev;
	init_completion(&chip->eoc_complete);

	rc = of_property_read_u32(node, "reg", &chip->tadc_base);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read base address rc=%d\n", rc);
		return rc;
	}
	chip->tadc_cmp_base = chip->tadc_base + 0x100;

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

	irq = of_irq_get_byname(node, "eoc");
	if (irq < 0) {
		pr_err("Couldn't get eoc irq rc=%d\n", irq);
		return irq;
	}

	rc = devm_request_threaded_irq(chip->dev, irq, NULL, handle_eoc,
				       IRQF_ONESHOT, "eoc", chip);
	if (rc < 0) {
		pr_err("Couldn't request irq %d rc=%d\n", irq, rc);
		return rc;
	}

	indio_dev->dev.parent = chip->dev;
	indio_dev->name = pdev->name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &tadc_info;
	indio_dev->channels = tadc_iio_chans;
	indio_dev->num_channels = ARRAY_SIZE(tadc_iio_chans);

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't register IIO device rc=%d\n", rc);

	return rc;
}

static int tadc_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id tadc_match_table[] = {
	{ .compatible = "qcom,tadc" },
	{ }
};
MODULE_DEVICE_TABLE(of, tadc_match_table);

static struct platform_driver tadc_driver = {
	.driver	= {
		.name		= "qcom-tadc",
		.of_match_table	= tadc_match_table,
	},
	.probe	= tadc_probe,
	.remove	= tadc_remove,
};
module_platform_driver(tadc_driver);

MODULE_DESCRIPTION("Qualcomm Technologies Inc. TADC driver");
MODULE_LICENSE("GPL v2");
