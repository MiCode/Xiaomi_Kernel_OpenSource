/*
 * Fuel gauge driver for Maxim 17042 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2011 Samsung Electronics
 * MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max17040_battery.c
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/power/max17042_battery.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/acpi.h>
#include <linux/power/battery_id.h>
#include <linux/thermal.h>

/* Status register bits */
#define STATUS_POR_BIT         (1 << 1)
#define STATUS_BST_BIT         (1 << 3)
#define STATUS_VMN_BIT         (1 << 8)
#define STATUS_TMN_BIT         (1 << 9)
#define STATUS_SMN_BIT         (1 << 10)
#define STATUS_BI_BIT          (1 << 11)
#define STATUS_VMX_BIT         (1 << 12)
#define STATUS_TMX_BIT         (1 << 13)
#define STATUS_SMX_BIT         (1 << 14)
#define STATUS_BR_BIT          (1 << 15)

/* Interrupt mask bits */
#define CONFIG_ALRT_BIT_ENBL	(1 << 2)
#define STATUS_INTR_SOCMIN_BIT	(1 << 10)
#define STATUS_INTR_SOCMAX_BIT	(1 << 14)

#define VFSOC0_LOCK		0x0000
#define VFSOC0_UNLOCK		0x0080
#define MODEL_UNLOCK1	0X0059
#define MODEL_UNLOCK2	0X00C4
#define MODEL_LOCK1		0X0000
#define MODEL_LOCK2		0X0000

#define dQ_ACC_DIV	0x4
#define dP_ACC_100	0x1900
#define dP_ACC_200	0x3200

#define SOC_ROUNDOFF_MASK      0x80

#define MAX17042_TEX_BIT_ENBL	(1 << 8)

#define MAX17042_TEMP_REG_SHIFT	8
#define MAX17042_VOLTAGE_CONV_FCTR	625
#define MAX17042_CHRG_CONV_FCTR	500
#define MAX17042_CURR_CONV_FCTR	1562500
#define MAX17042_VMAX_OFFSET	50
#define BATTID_UNKNOWN		"UNKNOWNB"
#define BATTID_LENGTH		8

#define MAX17042_IC_VERSION	0x0092
#define MAX17047_IC_VERSION	0x00AC	/* same for max17050 */
#define MC_TO_DEGREE(mC) (mC / 1000)
#define DEGREE_TO_TENTHS_DEGREE(c) (c * 10)
#define ACPI_BATTERY_SENSOR_NAME "STR3"
#define MAX17042_DEFAULT_TEMP_MAX 450 /* 45 Degree Celcius */

#define MAX17042_SOFT_POR_CMD	0x000F	/* Maxim soft POR command */
#define MAXIM_FGCONFIG_ACPI_TABLE_NAME	"BCFG"
#define ACPI_FG_NAME_LEN	8

struct max17042_chip {
	struct i2c_client *client;
	struct regmap *regmap;
	struct power_supply battery;
	enum max170xx_chip_type chip_type;
	struct max17042_platform_data *pdata;
	struct work_struct work;
	int    init_complete;
	int    status;
	int    health;
	int    ext_set_cap;
};

static enum power_supply_property max17042_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
	POWER_SUPPLY_PROP_TEMP_ALERT_MIN,
	POWER_SUPPLY_PROP_TEMP_ALERT_MAX,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
};


static int max17042_get_temperature(struct max17042_chip *chip, int *temp)
{
	int ret;
	u32 data;
	struct regmap *map = chip->regmap;
#ifdef CONFIG_ACPI
	u32 config, val;
	struct thermal_zone_device *tzd;
	unsigned long temp_mC;

	tzd = thermal_zone_get_zone_by_name(ACPI_BATTERY_SENSOR_NAME);
	if (!IS_ERR_OR_NULL(tzd)) {
		tzd->ops->get_temp(tzd, &temp_mC);
		*temp = MC_TO_DEGREE(temp_mC);

		regmap_read(chip->regmap, MAX17042_CONFIG, &config);
		if (config & MAX17042_TEX_BIT_ENBL) {
			if (*temp < 0) {
				val = (*temp + 0xff + 1);
				val <<= 8;
			} else {
				val = *temp;
				val <<= 8;
			}
			regmap_write(chip->regmap, MAX17042_TEMP, val);
		}

		*temp = DEGREE_TO_TENTHS_DEGREE(*temp);
		return 0;
	}
#endif
	ret = regmap_read(map, MAX17042_TEMP, &data);
	if (ret < 0)
		return ret;

	*temp = data;

	/* The value is signed. */
	if (*temp & 0x8000) {
		*temp = (0x7fff & ~*temp) + 1;
		*temp *= -1;
	}
	/* Units of LSB = 1 / 256 degree Celsius */
	*temp >>= MAX17042_TEMP_REG_SHIFT;
	/* The value is converted into deci-centigrade scale */
	*temp *= 10;
	return 0;
}

static int max17042_get_battery_health(struct max17042_chip *chip)
{
	int temp, ret;
	u32 val;

	/* Cannot judge health of an unknown battery */
	if (!strncmp(chip->pdata->battid, BATTID_UNKNOWN, BATTID_LENGTH))
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	ret = regmap_read(chip->regmap, MAX17042_AvgVCELL, &val);
	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	else {
		/* bits [0-3] unused */
		val *= MAX17042_VOLTAGE_CONV_FCTR / 8;
		/* Convert to milli volts */
		val /= 1000;
		if (val < chip->pdata->vmin)
			return POWER_SUPPLY_HEALTH_DEAD;

		if (val > chip->pdata->vmax + MAX17042_VMAX_OFFSET)
			return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}

	ret = max17042_get_temperature(chip, &temp);

	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	else if (temp <= chip->pdata->temp_min ||
		temp >= chip->pdata->temp_max)
		return POWER_SUPPLY_HEALTH_OVERHEAT;

	return POWER_SUPPLY_HEALTH_GOOD;
}

static int max17042_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17042_chip *chip = container_of(psy,
				struct max17042_chip, battery);
	struct regmap *map = chip->regmap;
	int ret;
	u32 data, ocv;

	if (!chip->init_complete)
		return -EAGAIN;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (chip->status < 0)
			return chip->status;
		else
			val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		chip->health = max17042_get_battery_health(chip);
		if (chip->health < 0)
			return chip->health;
		else
			val->intval = chip->health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		ret = regmap_read(map, MAX17042_STATUS, &data);
		if (ret < 0)
			return ret;

		if (data & MAX17042_STATUS_BattAbsent)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = regmap_read(map, MAX17042_Cycles, &data);
		if (ret < 0)
			return ret;

		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = regmap_read(map, MAX17042_MinMaxVolt, &data);
		if (ret < 0)
			return ret;

		val->intval = data >> 8;
		val->intval *= 20000; /* Units of LSB = 20mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		if (chip->chip_type == MAX17042)
			ret = regmap_read(map, MAX17042_V_empty, &data);
		else
			ret = regmap_read(map, MAX17047_V_empty, &data);
		if (ret < 0)
			return ret;

		val->intval = data >> 7;
		val->intval *= 10000; /* Units of LSB = 10mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = regmap_read(map, MAX17042_VCELL, &data);
		if (ret < 0)
			return ret;

		/* bits[0-3] don't care */
		val->intval = data * MAX17042_VOLTAGE_CONV_FCTR / 8;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = regmap_read(map, MAX17042_AvgVCELL, &data);
		if (ret < 0)
			return ret;

		/* bits[0-3] don't care */
		val->intval = data * MAX17042_VOLTAGE_CONV_FCTR / 8;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = regmap_read(map, MAX17042_OCVInternal, &data);
		if (ret < 0)
			return ret;

		/* bits[0-3] don't care */
		val->intval = data * MAX17042_VOLTAGE_CONV_FCTR / 8;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* Check whether the capacity is set externally or not (accepts
		 * value '0' only). If the capacity value is set externally, use
		 * same as a SOC value for the battery level usage.
		 */
		if (chip->ext_set_cap == 0) {
			val->intval = chip->ext_set_cap;
			break;
		}

		ret = regmap_read(map, MAX17042_OCVInternal, &data);
		if (ret < 0)
			return ret;
		/* bits[0-3] don't care */
		ocv = data * MAX17042_VOLTAGE_CONV_FCTR / 8;
		if (ocv < chip->pdata->vmin) {
			/* report 0% if voltage < vmin */
			val->intval = 0;
			break;
		}
		ret = regmap_read(map, MAX17042_RepSOC, &data);
		if (ret < 0)
			return ret;

		val->intval = data >> 8;
		/*
		 * Check if MSB of lower byte is set
		 * then round off the SOC to higher number
		 */
		if ((data & 0x80) && val->intval)
			val->intval++;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = regmap_read(map, MAX17042_FullCAP, &data);
		if (ret < 0)
			return ret;

		val->intval = data * MAX17042_CHRG_CONV_FCTR;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		ret = regmap_read(map, MAX17042_QH, &data);
		if (ret < 0)
			return ret;

		val->intval = data * MAX17042_CHRG_CONV_FCTR;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = max17042_get_temperature(chip, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (chip->pdata->enable_current_sense) {
			ret = regmap_read(map, MAX17042_Current, &data);
			if (ret < 0)
				return ret;

			val->intval = data;
			if (val->intval & 0x8000) {
				/* Negative */
				val->intval = ~val->intval & 0x7fff;
				val->intval++;
				val->intval *= -1;
			}
			val->intval *= MAX17042_CURR_CONV_FCTR /
							chip->pdata->r_sns;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (chip->pdata->enable_current_sense) {
			ret = regmap_read(map, MAX17042_AvgCurrent, &data);
			if (ret < 0)
				return ret;

			val->intval = data;
			if (val->intval & 0x8000) {
				/* Negative */
				val->intval = ~val->intval & 0x7fff;
				val->intval++;
				val->intval *= -1;
			}
			val->intval *= MAX17042_CURR_CONV_FCTR /
							chip->pdata->r_sns;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = regmap_read(map, MAX17042_RepCap, &data);
		if (ret < 0)
			return ret;
		val->intval = data * MAX17042_CHRG_CONV_FCTR;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		if (!strncmp(chip->pdata->battid, BATTID_UNKNOWN,
						BATTID_LENGTH))
			val->strval = chip->pdata->battid;
		else
			val->strval = chip->pdata->model_name;
		break;
	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		val->strval = chip->pdata->serial_num;
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
		ret = regmap_read(map, MAX17042_TALRT_Th, &data);
		if (ret < 0)
			return ret;
		/* LSB is Alert Minimum. In deci-centigrade */
		val->intval = (data & 0xff) * 10;
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
		ret = regmap_read(map, MAX17042_TALRT_Th, &data);
		if (ret < 0)
			return ret;
		/* MSB is Alert Maximum. In deci-centigrade */
		val->intval = (data >> 8) * 10;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = chip->pdata->technology;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17042_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct max17042_chip *chip = container_of(psy,
					struct max17042_chip, battery);
	struct regmap *map = chip->regmap;
	int ret = 0;
	u32 read_value;
	int8_t temp;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		chip->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* here, this property is to set the soc value '0' only in high
		 * peak current situation for shutting down the platform
		 * gracefully, to avoid components crash/critical hardware
		 * shutdown.
		 */
		if (val->intval == 0)
			chip->ext_set_cap = val->intval;
		else
			ret = -EINVAL;
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
		regmap_read(map, MAX17042_TALRT_Th, &read_value);
		/* Input in Deci-Centigrade, convert to centigrade */
		temp = val->intval / 10;
		/* force min < max */
		if (temp >= (int8_t)(read_value >> 8))
			temp = (int8_t)(read_value >> 8) - 1;
		/* Write both MAX and MIN ALERT */
		read_value = (read_value & 0xff00) + (uint8_t)temp;
		regmap_write(map, MAX17042_TALRT_Th, read_value);
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
		regmap_read(map, MAX17042_TALRT_Th, &read_value);
		/* Input in Deci-Centigrade, convert to centigrade */
		temp = val->intval / 10;
		/* force max > min */
		if (temp <= (int8_t)(read_value & 0xff))
			temp = (int8_t)(read_value & 0xff) + 1;
		/* Write both MAX and MIN ALERT */
		read_value = (read_value & 0xff) + (temp << 8);
		regmap_write(map, MAX17042_TALRT_Th, read_value);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int max17042_write_verify_reg(struct regmap *map, u8 reg, u32 value)
{
	int retries = 8;
	int ret;
	u32 read_value;

	do {
		ret = regmap_write(map, reg, value);
		regmap_read(map, reg, &read_value);
		if (read_value != value) {
			ret = -EIO;
			retries--;
		}
	} while (retries && read_value != value);

	if (ret < 0)
		pr_err("%s: err %d\n", __func__, ret);

	return ret;
}

static inline void max17042_override_por(struct regmap *map,
					 u8 reg, u16 value)
{
	if (value)
		regmap_write(map, reg, value);
}

static inline void max10742_unlock_model(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;
	regmap_write(map, MAX17042_MLOCKReg1, MODEL_UNLOCK1);
	regmap_write(map, MAX17042_MLOCKReg2, MODEL_UNLOCK2);
}

static inline void max10742_lock_model(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;

	regmap_write(map, MAX17042_MLOCKReg1, MODEL_LOCK1);
	regmap_write(map, MAX17042_MLOCKReg2, MODEL_LOCK2);
}

static inline void max17042_write_model_data(struct max17042_chip *chip,
					u8 addr, int size)
{
	struct regmap *map = chip->regmap;
	int i;
	for (i = 0; i < size; i++)
		regmap_write(map, addr + i,
			chip->pdata->config_data->cell_char_tbl[i]);
}

static inline void max17042_read_model_data(struct max17042_chip *chip,
					u8 addr, u16 *data, int size)
{
	struct regmap *map = chip->regmap;
	int i;
	unsigned int val;

	for (i = 0; i < size; i++) {
		regmap_read(map, addr + i, &val);
		data[i] = val;
	}
}

static inline int max17042_model_data_compare(struct max17042_chip *chip,
					u16 *data1, u16 *data2, int size)
{
	int i;

	if (memcmp(data1, data2, size)) {
		dev_err(&chip->client->dev, "%s compare failed\n", __func__);
		for (i = 0; i < size; i++)
			dev_info(&chip->client->dev, "0x%x, 0x%x",
				data1[i], data2[i]);
		dev_info(&chip->client->dev, "\n");
		return -EINVAL;
	}
	return 0;
}

static int max17042_init_model(struct max17042_chip *chip)
{
	int ret;
	int table_size = ARRAY_SIZE(chip->pdata->config_data->cell_char_tbl);
	u16 *temp_data;

	temp_data = kcalloc(table_size, sizeof(*temp_data), GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	max10742_unlock_model(chip);
	max17042_write_model_data(chip, MAX17042_MODELChrTbl,
				table_size);
	max17042_read_model_data(chip, MAX17042_MODELChrTbl, temp_data,
				table_size);

	ret = max17042_model_data_compare(
		chip,
		chip->pdata->config_data->cell_char_tbl,
		(u16 *)temp_data,
		table_size);

	max10742_lock_model(chip);
	kfree(temp_data);

	return ret;
}

static int max17042_verify_model_lock(struct max17042_chip *chip)
{
	int i;
	int table_size = ARRAY_SIZE(chip->pdata->config_data->cell_char_tbl);
	u16 *temp_data;
	int ret = 0;

	temp_data = kcalloc(table_size, sizeof(*temp_data), GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	max17042_read_model_data(chip, MAX17042_MODELChrTbl, temp_data,
				table_size);
	for (i = 0; i < table_size; i++)
		if (temp_data[i])
			ret = -EINVAL;

	kfree(temp_data);
	return ret;
}

static void max17042_write_config_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;

	regmap_write(map, MAX17042_CONFIG, config->config);
	regmap_write(map, MAX17042_LearnCFG, config->learn_cfg);
	regmap_write(map, MAX17042_FilterCFG,
			config->filter_cfg);
	regmap_write(map, MAX17042_RelaxCFG, config->relax_cfg);
	if (chip->chip_type == MAX17047)
		regmap_write(map, MAX17047_FullSOCThr,
						config->full_soc_thresh);
}

static void  max17042_write_custom_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;

	max17042_write_verify_reg(map, MAX17042_RCOMP0, config->rcomp0);
	max17042_write_verify_reg(map, MAX17042_TempCo,	config->tcompc0);
	max17042_write_verify_reg(map, MAX17042_ICHGTerm, config->ichgt_term);
	if (chip->chip_type == MAX17042) {
		regmap_write(map, MAX17042_EmptyTempCo,	config->empty_tempco);
		max17042_write_verify_reg(map, MAX17042_K_empty0,
					config->kempty0);
	} else {
		max17042_write_verify_reg(map, MAX17047_QRTbl00,
						config->qrtbl00);
		max17042_write_verify_reg(map, MAX17047_QRTbl10,
						config->qrtbl10);
		max17042_write_verify_reg(map, MAX17047_QRTbl20,
						config->qrtbl20);
		max17042_write_verify_reg(map, MAX17047_QRTbl30,
						config->qrtbl30);
	}
}

static void max17042_update_capacity_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;

	max17042_write_verify_reg(map, MAX17042_FullCAP,
				config->fullcap);
	regmap_write(map, MAX17042_DesignCap, config->design_cap);
	max17042_write_verify_reg(map, MAX17042_FullCAPNom,
				config->fullcapnom);
}

static void max17042_reset_vfsoc0_reg(struct max17042_chip *chip)
{
	unsigned int vfSoc;
	struct regmap *map = chip->regmap;

	regmap_read(map, MAX17042_VFSOC, &vfSoc);
	regmap_write(map, MAX17042_VFSOC0Enable, VFSOC0_UNLOCK);
	max17042_write_verify_reg(map, MAX17042_VFSOC0, vfSoc);
	regmap_write(map, MAX17042_VFSOC0Enable, VFSOC0_LOCK);
}


static void enable_soft_POR(struct max17042_chip *chip)
{
	unsigned int val = 0;
	struct regmap *map = chip->regmap;

	regmap_write(map, MAX17042_MLOCKReg1, val);
	regmap_write(map, MAX17042_MLOCKReg2, val);
	regmap_write(map, MAX17042_STATUS, val);

	regmap_read(map, MAX17042_MLOCKReg1, &val);
	if (val)
		dev_err(&chip->client->dev, "MLOCKReg1 read failed\n");

	regmap_read(map, MAX17042_MLOCKReg2, &val);
	if (val)
		dev_err(&chip->client->dev, "MLOCKReg2 read failed\n");

	regmap_read(map, MAX17042_STATUS, &val);
	if (val)
		dev_err(&chip->client->dev, "STATUS read failed\n");

	/* send POR command */
	regmap_write(map, MAX17042_VFSOC0Enable, MAX17042_SOFT_POR_CMD);
	mdelay(2);

	regmap_read(map, MAX17042_STATUS, &val);
	if (val & STATUS_POR_BIT)
		dev_info(&chip->client->dev, "SoftPOR done!\n");
	else
		dev_err(&chip->client->dev, "SoftPOR failed\n");
}

static void reset_max17042(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;

	/* do soft power reset */
	enable_soft_POR(chip);

	/* After Power up, the MAX17042 requires 500mS in order
	 * to perform signal debouncing and initial SOC reporting
	 */
	msleep(500);

	regmap_write(map, MAX17042_CONFIG, 0x2210);

	/* adjust Temperature gain and offset */
	regmap_write(map, MAX17042_TGAIN, chip->pdata->config_data->tgain);
	regmap_write(map, MAX17042_TOFF, chip->pdata->config_data->toff);
}

static void max17042_load_new_capacity_params(struct max17042_chip *chip)
{
	u32 full_cap0, rep_cap, dq_acc, vfSoc;
	u32 rem_cap;

	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;

	regmap_read(map, MAX17042_FullCAP0, &full_cap0);
	regmap_read(map, MAX17042_VFSOC, &vfSoc);

	/* fg_vfSoc needs to shifted by 8 bits to get the
	 * perc in 1% accuracy, to get the right rem_cap multiply
	 * full_cap0, fg_vfSoc and devide by 100
	 */
	rem_cap = ((vfSoc >> 8) * full_cap0) / 100;
	max17042_write_verify_reg(map, MAX17042_RemCap, rem_cap);

	rep_cap = rem_cap;
	max17042_write_verify_reg(map, MAX17042_RepCap, rep_cap);

	/* Write dQ_acc to 200% of Capacity and dP_acc to 200% */
	dq_acc = config->fullcap / dQ_ACC_DIV;
	max17042_write_verify_reg(map, MAX17042_dQacc, dq_acc);
	max17042_write_verify_reg(map, MAX17042_dPacc, dP_ACC_200);

	max17042_write_verify_reg(map, MAX17042_FullCAP,
			config->fullcap);
	regmap_write(map, MAX17042_DesignCap,
			config->design_cap);
	max17042_write_verify_reg(map, MAX17042_FullCAPNom,
			config->fullcapnom);
	/* Update SOC register with new SOC */
	regmap_write(map, MAX17042_RepSOC, vfSoc);
}

/*
 * Block write all the override values coming from platform data.
 * This function MUST be called before the POR initialization proceedure
 * specified by maxim.
 */
static inline void max17042_override_por_values(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;
	struct max17042_config_data *config = chip->pdata->config_data;

	max17042_override_por(map, MAX17042_TGAIN, config->tgain);
	max17042_override_por(map, MAX17042_TOFF, config->toff);
	max17042_override_por(map, MAX17042_CGAIN, config->cgain);
	max17042_override_por(map, MAX17042_COFF, config->coff);

	max17042_override_por(map, MAX17042_VALRT_Th, config->valrt_thresh);
	max17042_override_por(map, MAX17042_TALRT_Th, config->talrt_thresh);
	max17042_override_por(map, MAX17042_SALRT_Th,
						config->soc_alrt_thresh);
	max17042_override_por(map, MAX17042_CONFIG, config->config);
	max17042_override_por(map, MAX17042_SHDNTIMER, config->shdntimer);

	max17042_override_por(map, MAX17042_DesignCap, config->design_cap);
	max17042_override_por(map, MAX17042_ICHGTerm, config->ichgt_term);

	max17042_override_por(map, MAX17042_AtRate, config->at_rate);
	max17042_override_por(map, MAX17042_LearnCFG, config->learn_cfg);
	max17042_override_por(map, MAX17042_FilterCFG, config->filter_cfg);
	max17042_override_por(map, MAX17042_RelaxCFG, config->relax_cfg);
	max17042_override_por(map, MAX17042_MiscCFG, config->misc_cfg);
	max17042_override_por(map, MAX17042_MaskSOC, config->masksoc);

	max17042_override_por(map, MAX17042_FullCAP, config->fullcap);
	max17042_override_por(map, MAX17042_FullCAPNom, config->fullcapnom);
	if (chip->chip_type == MAX17042)
		max17042_override_por(map, MAX17042_SOC_empty,
						config->socempty);
	max17042_override_por(map, MAX17042_LAvg_empty, config->lavg_empty);
	max17042_override_por(map, MAX17042_dQacc, config->dqacc);
	max17042_override_por(map, MAX17042_dPacc, config->dpacc);

	if (chip->chip_type == MAX17042)
		max17042_override_por(map, MAX17042_V_empty, config->vempty);
	else
		max17042_override_por(map, MAX17047_V_empty, config->vempty);
	max17042_override_por(map, MAX17042_TempNom, config->temp_nom);
	max17042_override_por(map, MAX17042_TempLim, config->temp_lim);
	max17042_override_por(map, MAX17042_FCTC, config->fctc);
	max17042_override_por(map, MAX17042_RCOMP0, config->rcomp0);
	max17042_override_por(map, MAX17042_TempCo, config->tcompc0);
	if (chip->chip_type) {
		max17042_override_por(map, MAX17042_EmptyTempCo,
						config->empty_tempco);
		max17042_override_por(map, MAX17042_K_empty0,
						config->kempty0);
	}
}

static int max17042_init_chip(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;
	int ret;
	int val;

	/* reset the maxim chp */
	reset_max17042(chip);

	/* Set por values */
	max17042_override_por_values(chip);
	/* After Power up, the MAX17042 requires 500mS in order
	 * to perform signal debouncing and initial SOC reporting
	 */
	msleep(500);

	/* Initialize configaration */
	max17042_write_config_regs(chip);

	/* write cell characterization data */
	ret = max17042_init_model(chip);
	if (ret) {
		dev_err(&chip->client->dev, "%s init failed\n",
			__func__);
		return -EIO;
	}

	ret = max17042_verify_model_lock(chip);
	if (ret) {
		dev_err(&chip->client->dev, "%s lock verify failed\n",
			__func__);
		return -EIO;
	}
	/* write custom parameters */
	max17042_write_custom_regs(chip);

	/* update capacity params */
	max17042_update_capacity_regs(chip);

	/* delay must be atleast 350mS to allow VFSOC
	 * to be calculated from the new configuration
	 */
	msleep(350);

	/* reset vfsoc0 reg */
	max17042_reset_vfsoc0_reg(chip);

	/* load new capacity params */
	max17042_load_new_capacity_params(chip);

	/* Init complete, Clear the POR bit */
	regmap_read(map, MAX17042_STATUS, &val);
	regmap_write(map, MAX17042_STATUS, val & (~STATUS_POR_BIT));
	return 0;
}

static void max17042_set_soc_threshold(struct max17042_chip *chip, u16 off)
{
	struct regmap *map = chip->regmap;
	u32 soc, soc_tr;

	/* program interrupt thesholds such that we should
	 * get interrupt for every 'off' perc change in the soc
	 */
	regmap_read(map, MAX17042_RepSOC, &soc);
	if (soc & SOC_ROUNDOFF_MASK)
		soc = (soc >> 8) + 1;
	else
		soc = soc >> 8;
	soc_tr = (soc + off) << 8;
	soc_tr |= (soc - off);
	regmap_write(map, MAX17042_SALRT_Th, soc_tr);
}

static irqreturn_t max17042_thread_handler(int id, void *dev)
{
	struct max17042_chip *chip = dev;
	u32 stat;

	regmap_read(chip->regmap, MAX17042_STATUS, &stat);
	if ((stat & STATUS_INTR_SOCMIN_BIT) ||
		(stat & STATUS_INTR_SOCMAX_BIT)) {
		dev_info(&chip->client->dev, "SOC threshold INTR\n");
		max17042_set_soc_threshold(chip, 1);
	}

	chip->health = max17042_get_battery_health(chip);

	power_supply_changed(&chip->battery);
	return IRQ_HANDLED;
}

static void max17042_init_worker(struct work_struct *work)
{
	struct max17042_chip *chip = container_of(work,
				struct max17042_chip, work);
	int ret;
	u32 val;

	/* Initialize registers according to values from the platform data */
	if (chip->pdata->enable_por_init && chip->pdata->config_data) {
		ret = max17042_init_chip(chip);
		if (ret)
			return;
	}

	/* Enable and Configure IRQs after initializing the FG */
	if (chip->client->irq) {
		regmap_read(chip->regmap, MAX17042_CONFIG, &val);
		val |= CONFIG_ALRT_BIT_ENBL;
		regmap_write(chip->regmap, MAX17042_CONFIG, val);
		max17042_set_soc_threshold(chip, 1);
	}

	chip->init_complete = 1;
}



#ifdef CONFIG_ACPI

struct max170xx_acpi_fg_config {
	struct acpi_table_header acpi_header;
	char fg_name[ACPI_FG_NAME_LEN];
	char battid[BATTID_LEN];
	u16 size;
	u16 checksum;
	struct max17042_config_data cdata;
};

static struct max17042_config_data *
max17042_get_acpi_cdata(struct device *dev)
{
	struct max170xx_acpi_fg_config *acpi_tbl = NULL;
	struct max17042_config_data *cdata;
	char *name = MAXIM_FGCONFIG_ACPI_TABLE_NAME;
	acpi_size tbl_size;
	acpi_status status;

	/* read the fg config table from acpi */
	status = acpi_get_table_with_size(name , 0,
			(struct acpi_table_header **)&acpi_tbl, &tbl_size);
	if (ACPI_FAILURE(status)) {
		dev_err(dev, "%s:%s table not found!!\n", __func__, name);
		return NULL;
	}
	dev_info(dev, "%s: %s table found, size=%d\n",
				__func__, name, (int)tbl_size);

	/* validate the table size */
	if (tbl_size <  sizeof(struct max170xx_acpi_fg_config)) {
		dev_err(dev, "%s:%s table incomplete!!\n", __func__, name);
		dev_info(dev, "%s: table_size=%d, structure_size=%lu\n",
			__func__, (int)tbl_size,
			sizeof(struct max170xx_acpi_fg_config));
		return NULL;
	}

	cdata = devm_kzalloc(dev, sizeof(*cdata), GFP_KERNEL);
	if (!cdata) {
		dev_err(dev, "%s:Memory allocation failed\n", __func__);
		return NULL;
	}

	memcpy(cdata, &acpi_tbl->cdata, sizeof(struct max17042_config_data));

	return cdata;
}

static struct max17042_config_data *
max17042_get_fg_config_data(struct device *dev)
{
	struct max17042_config_data *cdata;

	cdata = max17042_get_acpi_cdata(dev);
	if (cdata)
		dev_info(dev, "%s: Got fg config data from acpi\n",
			__func__);
	else
		dev_err(dev, "%s:Failed to get acpi fg config\n",
		__func__);
	return cdata;
}
#else /* CONFIG_ACPI */
static struct max17042_config_data *
max17042_get_fg_config_data(struct device *dev)
{
	return NULL;
}
#endif /* CONFIG_ACPI */

#ifdef CONFIG_OF
static struct max17042_platform_data *
max17042_get_pdata(struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 prop;
	struct max17042_platform_data *pdata;

	if (!np)
		return dev->platform_data;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	/*
	 * Require current sense resistor value to be specified for
	 * current-sense functionality to be enabled at all.
	 */
	if (of_property_read_u32(np, "maxim,rsns-microohm", &prop) == 0) {
		pdata->r_sns = prop;
		pdata->enable_current_sense = true;
	}

	return pdata;
}
#else
static struct max17042_platform_data *
max17042_get_pdata(struct device *dev)
{
	struct max17042_platform_data *pdata;

#ifdef CONFIG_POWER_SUPPLY_CHARGER
	struct ps_batt_chg_prof batt_prof;
	struct ps_pse_mod_prof *pse_prof;
	int ret, i, temp_mon_ranges;
#endif
	if (!IS_ENABLED(CONFIG_ACPI))
		return dev->platform_data;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

#ifdef CONFIG_POWER_SUPPLY_CHARGER
	memset(&batt_prof, 0 , sizeof(batt_prof));
	ret = get_batt_prop(&batt_prof);
	/* Treat the battery as invalid if charge profile not found
	 * or type is CHRG_PROF_NONE.
	 */
	if (ret < 0 || batt_prof.chrg_prof_type == CHRG_PROF_NONE) {
		pdata->enable_current_sense = false;
		snprintf(pdata->battid, (BATTID_LEN+1),
			"%s", "UNKNOWNB");
		pdata->technology = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
	} else {
		pdata->enable_current_sense = true;
		pse_prof = (struct ps_pse_mod_prof *)batt_prof.batt_prof;
		if (pse_prof) {
			pdata->vmin = pse_prof->low_batt_mV;
			pdata->vmax = pse_prof->voltage_max;
			temp_mon_ranges = min_t(u16, pse_prof->temp_mon_ranges,
						BATT_TEMP_NR_RNG);
			for (i = 0; i < temp_mon_ranges; ++i) {
				if (pse_prof->temp_mon_range[i].full_chrg_cur)
					break;
			}
			if (i < temp_mon_ranges)
				pdata->temp_max = DEGREE_TO_TENTHS_DEGREE(
					pse_prof->temp_mon_range[i].
						temp_up_lim);
			else
				pdata->temp_max = MAX17042_DEFAULT_TEMP_MAX;

			pdata->temp_min = DEGREE_TO_TENTHS_DEGREE(
				pse_prof->temp_low_lim);
			snprintf(pdata->battid, (BATTID_LEN+1),
				"%s", pse_prof->batt_id);
		}
		pdata->technology = POWER_SUPPLY_TECHNOLOGY_LION;
	}
	snprintf(pdata->model_name, (MODEL_NAME_LEN + 1), "%s",
				pdata->battid);
	snprintf(pdata->serial_num, (SERIAL_NUM_LEN + 1), "%s",
				pdata->battid + MODEL_NAME_LEN);
#else
	pdata->vmin = 3300; /* 3.3V */
	pdata->vmax = 4350;
	pdata->temp_min = 0;
	pdata->temp_max = 600;
#endif
	pdata->config_data = max17042_get_fg_config_data(dev);
	if (pdata->config_data)
		pdata->enable_por_init = true;
	return pdata;
}
#endif

static int max17042_get_irq(struct i2c_client *client)
{
	struct gpio_desc *gpio_desc;
	int irq;
	struct device *dev = &client->dev;

	if (client->irq > 0)
		return client->irq;

	gpio_desc = devm_gpiod_get_index(dev, "fg_alert", 0);

	if (IS_ERR(gpio_desc))
		return client->irq;

	irq = gpiod_to_irq(gpio_desc);

	gpiod_put(gpio_desc);

	return irq;
}

static struct regmap_config max17042_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static int max17042_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17042_chip *chip;
	int ret;
	int i;
	u32 val;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->regmap = devm_regmap_init_i2c(client, &max17042_regmap_config);
	if (IS_ERR(chip->regmap)) {
		dev_err(&client->dev, "Failed to initialize regmap\n");
		return -EINVAL;
	}

	chip->pdata = max17042_get_pdata(&client->dev);
	if (!chip->pdata) {
		dev_err(&client->dev, "no platform data provided\n");
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_ACPI))
		client->irq = max17042_get_irq(client);

	i2c_set_clientdata(client, chip);

	regmap_read(chip->regmap, MAX17042_DevName, &val);
	if (val == MAX17042_IC_VERSION) {
		dev_dbg(&client->dev, "chip type max17042 detected\n");
		chip->chip_type = MAX17042;
	} else if (val == MAX17047_IC_VERSION) {
		dev_dbg(&client->dev, "chip type max17047/50 detected\n");
		chip->chip_type = MAX17047;
	} else {
		dev_err(&client->dev, "device version mismatch: %x\n", val);
		return -EIO;
	}

	chip->battery.name		= "max170xx_battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17042_get_property;
	chip->battery.set_property	= max17042_set_property;
	chip->battery.properties	= max17042_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17042_battery_props);

	/* When current is not measured,
	 * CURRENT_NOW and CURRENT_AVG properties should be invisible. */
	if (!chip->pdata->enable_current_sense)
		chip->battery.num_properties -= 2;

	if (chip->pdata->r_sns == 0)
		chip->pdata->r_sns = MAX17042_DEFAULT_SNS_RESISTOR;

	if (chip->pdata->init_data)
		for (i = 0; i < chip->pdata->num_init_data; i++)
			regmap_write(chip->regmap,
					chip->pdata->init_data[i].addr,
					chip->pdata->init_data[i].data);

	if (!chip->pdata->enable_current_sense) {
		regmap_write(chip->regmap, MAX17042_CGAIN, 0x0000);
		regmap_write(chip->regmap, MAX17042_MiscCFG, 0x0003);
		regmap_write(chip->regmap, MAX17042_LearnCFG, 0x0007);
	}

	chip->health = POWER_SUPPLY_HEALTH_GOOD;
	chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	chip->ext_set_cap = -EINVAL;

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		return ret;
	}

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
					max17042_thread_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					chip->battery.name, chip);
		if (!ret) {
			regmap_read(chip->regmap, MAX17042_CONFIG, &val);
			val |= CONFIG_ALRT_BIT_ENBL;
			regmap_write(chip->regmap, MAX17042_CONFIG, val);
			max17042_set_soc_threshold(chip, 1);
		} else {
			client->irq = 0;
			dev_err(&client->dev, "%s(): cannot get IRQ\n",
				__func__);
		}
	}

	regmap_read(chip->regmap, MAX17042_STATUS, &val);
	if (val & STATUS_POR_BIT) {
		INIT_WORK(&chip->work, max17042_init_worker);
		schedule_work(&chip->work);
	} else {
		chip->init_complete = 1;
	}

	return 0;
}

static int max17042_remove(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	if (client->irq)
		free_irq(client->irq, chip);
	power_supply_unregister(&chip->battery);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max17042_suspend(struct device *dev)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);

	/*
	 * disable the irq and enable irq_wake
	 * capability to the interrupt line.
	 */
	if (chip->client->irq) {
		disable_irq(chip->client->irq);
		enable_irq_wake(chip->client->irq);
	}

	return 0;
}

static int max17042_resume(struct device *dev)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);

	if (chip->client->irq) {
		disable_irq_wake(chip->client->irq);
		enable_irq(chip->client->irq);
		/* re-program the SOC thresholds to 1% change */
		max17042_set_soc_threshold(chip, 1);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(max17042_pm_ops, max17042_suspend,
			max17042_resume);

#ifdef CONFIG_OF
static const struct of_device_id max17042_dt_match[] = {
	{ .compatible = "maxim,max17042" },
	{ .compatible = "maxim,max17047" },
	{ .compatible = "maxim,max17050" },
	{ },
};
MODULE_DEVICE_TABLE(of, max17042_dt_match);
#endif

#ifdef CONFIG_ACPI
static struct acpi_device_id max17042_acpi_match[] = {
	{"MAX17042", 0},
	{"MAX17047", 0},
	{"MAX17050", 0},
};
MODULE_DEVICE_TABLE(acpi, max17042_acpi_match);
#endif

static const struct i2c_device_id max17042_id[] = {
	{ "max17042", 0 },
	{ "max17047", 0 },
	{ "max17050", 0 },
	{ "MAX17042", 0 },
	{ "MAX17047", 0 },
	{ "MAX17047:00", 0 },
	{ "MAX17050", 0 },
	{ "MAX17050:00", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17042_id);

static struct i2c_driver max17042_i2c_driver = {
	.driver	= {
		.name	= "max17042",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(max17042_dt_match),
#elif defined(CONFIG_ACPI)
		.acpi_match_table = ACPI_PTR(max17042_acpi_match),
#endif
		.pm	= &max17042_pm_ops,
	},
	.probe		= max17042_probe,
	.remove		= max17042_remove,
	.id_table	= max17042_id,
};
module_i2c_driver(max17042_i2c_driver);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("MAX17042 Fuel Gauge");
MODULE_LICENSE("GPL");
