/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"BMS: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/spmi.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/mfd/pm8xxx/batterydata-lib.h>

/* BMS Register Offsets */
#define BMS1_REVISION1			0x0
#define BMS1_REVISION2			0x1
#define BMS1_STATUS1			0x8
#define BMS1_MODE_CTL			0X40
/* Coulomb counter clear registers */
#define BMS1_CC_DATA_CTL		0x42
#define BMS1_CC_CLEAR_CTRL		0x43
/* OCV limit registers */
#define BMS1_OCV_USE_LOW_LIMIT_THR0	0x48
#define BMS1_OCV_USE_LOW_LIMIT_THR1	0x49
#define BMS1_OCV_USE_HIGH_LIMIT_THR0	0x4A
#define BMS1_OCV_USE_HIGH_LIMIT_THR1	0x4B
#define BMS1_OCV_USE_LIMIT_CTL		0x4C
/* CC interrupt threshold */
#define BMS1_CC_THR0			0x7A
#define BMS1_CC_THR1			0x7B
#define BMS1_CC_THR2			0x7C
#define BMS1_CC_THR3			0x7D
#define BMS1_CC_THR4			0x7E
/* OCV for r registers */
#define BMS1_OCV_FOR_R_DATA0		0x80
#define BMS1_OCV_FOR_R_DATA1		0x81
#define BMS1_VSENSE_FOR_R_DATA0		0x82
#define BMS1_VSENSE_FOR_R_DATA1		0x83
/* Coulomb counter data */
#define BMS1_CC_DATA0			0x8A
#define BMS1_CC_DATA1			0x8B
#define BMS1_CC_DATA2			0x8C
#define BMS1_CC_DATA3			0x8D
#define BMS1_CC_DATA4			0x8E
/* OCV for soc data */
#define BMS1_OCV_FOR_SOC_DATA0		0x90
#define BMS1_OCV_FOR_SOC_DATA1		0x91
#define BMS1_VSENSE_PON_DATA0		0x94
#define BMS1_VSENSE_PON_DATA1		0x95
#define BMS1_VSENSE_AVG_DATA0		0x98
#define BMS1_VSENSE_AVG_DATA1		0x99
#define BMS1_VBAT_AVG_DATA0		0x9E
#define BMS1_VBAT_AVG_DATA1		0x9F
/* Extra bms registers */
#define BMS1_BMS_DATA_REG_0		0xB0
#define IAVG_STORAGE_REG		0xB1
#define SOC_STORAGE_REG			0xB2
#define BMS1_BMS_DATA_REG_3		0xB3

/* Configuration for saving of shutdown soc/iavg */
#define IGNORE_SOC_TEMP_DECIDEG		50
#define IAVG_STEP_SIZE_MA		50
#define IAVG_START			600
#define SOC_ZERO			0xFF

#define QPNP_BMS_DEV_NAME "qcom,qpnp-bms"

struct soc_params {
	int		fcc_uah;
	int		cc_uah;
	int		rbatt;
	int		iavg_ua;
	int		uuc_uah;
	int		ocv_charge_uah;
};

struct raw_soc_params {
	uint16_t	last_good_ocv_raw;
	int64_t		cc;
	int		last_good_ocv_uv;
};

struct qpnp_bms_chip {
	struct device			*dev;
	struct power_supply		bms_psy;
	struct spmi_device		*spmi;
	u16				base;

	u8				revision1;
	u8				revision2;
	int				charger_status;
	bool				online;
	/* platform data */
	unsigned int			r_sense_mohm;
	unsigned int			v_cutoff_uv;
	unsigned int			max_voltage_uv;
	unsigned int			r_conn_mohm;
	int				shutdown_soc_valid_limit;
	int				adjust_soc_low_threshold;
	int				adjust_soc_high_threshold;
	int				chg_term_ua;
	enum battery_type		batt_type;
	unsigned int			fcc;
	struct single_row_lut		*fcc_temp_lut;
	struct single_row_lut		*fcc_sf_lut;
	struct pc_temp_ocv_lut		*pc_temp_ocv_lut;
	struct sf_lut			*pc_sf_lut;
	struct sf_lut			*rbatt_sf_lut;
	int				default_rbatt_mohm;

	struct delayed_work		calculate_soc_delayed_work;

	struct mutex			bms_output_lock;
	struct mutex			last_ocv_uv_mutex;

	unsigned int			start_percent;
	unsigned int			end_percent;
	bool				ignore_shutdown_soc;
	int				shutdown_soc_invalid;
	int				shutdown_soc;
	int				shutdown_iavg_ma;

	int				low_soc_calc_threshold;
	int				low_soc_calculate_soc_ms;
	int				calculate_soc_ms;

	unsigned int			vadc_v0625;
	unsigned int			vadc_v1250;

	int				prev_iavg_ua;
	int				prev_uuc_iavg_ma;
	int				prev_pc_unusable;
	int				ibat_at_cv_ua;
	int				soc_at_cv;
	int				prev_chg_soc;
	int				calculated_soc;
};

static struct of_device_id qpnp_bms_match_table[] = {
	{ .compatible = QPNP_BMS_DEV_NAME },
	{}
};

static char *qpnp_bms_supplicants[] = {
	"battery"
};

static enum power_supply_property msm_bms_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static int qpnp_read_wrapper(struct qpnp_bms_chip *chip, u8 *val,
			u16 base, int count)
{
	int rc;
	struct spmi_device *spmi = chip->spmi;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, base, val, count);
	if (rc) {
		pr_err("SPMI read failed rc=%d\n", rc);
		return rc;
	}
	return 0;
}

static int qpnp_write_wrapper(struct qpnp_bms_chip *chip, u8 *val,
			u16 base, int count)
{
	int rc;
	struct spmi_device *spmi = chip->spmi;

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, base, val, count);
	if (rc) {
		pr_err("SPMI write failed rc=%d\n", rc);
		return rc;
	}
	return 0;
}

static int qpnp_masked_write(struct qpnp_bms_chip *chip, u16 addr,
							u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = qpnp_read_wrapper(chip, &reg, chip->base + addr, 1);
	if (rc) {
		pr_err("read failed addr = %03X, rc = %d\n",
				chip->base + addr, rc);
		return rc;
	}
	reg &= ~mask;
	reg |= val & mask;
	rc = qpnp_write_wrapper(chip, &reg, chip->base + addr, 1);
	if (rc) {
		pr_err("write failed addr = %03X, val = %02x, mask = %02x, reg = %02x, rc = %d\n",
				chip->base + addr, val, mask, reg, rc);
		return rc;
	}
	return 0;
}

#define HOLD_OREG_DATA		BIT(0)
static int lock_output_data(struct qpnp_bms_chip *chip)
{
	int rc;

	rc = qpnp_masked_write(chip, BMS1_CC_DATA_CTL,
				HOLD_OREG_DATA, HOLD_OREG_DATA);
	if (rc) {
		pr_err("couldnt lock bms output rc = %d\n", rc);
		return rc;
	}
	return 0;
}

static int unlock_output_data(struct qpnp_bms_chip *chip)
{
	int rc;

	rc = qpnp_masked_write(chip, BMS1_CC_DATA_CTL, HOLD_OREG_DATA, 0);
	if (rc) {
		pr_err("fail to unlock BMS_CONTROL rc = %d\n", rc);
		return rc;
	}
	return 0;
}

#define V_PER_BIT_MUL_FACTOR	97656
#define V_PER_BIT_DIV_FACTOR	1000
#define VADC_INTRINSIC_OFFSET	0x6000

static int vadc_reading_to_uv(unsigned int reading)
{
	if (reading <= VADC_INTRINSIC_OFFSET)
		return 0;

	return (reading - VADC_INTRINSIC_OFFSET)
			* V_PER_BIT_MUL_FACTOR / V_PER_BIT_DIV_FACTOR;
}

#define VADC_CALIB_UV		625000
#define VBATT_MUL_FACTOR	3

static int adjust_vbatt_reading(struct qpnp_bms_chip *chip,
						unsigned int reading_uv)
{
	s64 numerator, denominator;

	if (reading_uv == 0)
		return 0;

	/* don't adjust if not calibrated */
	if (chip->vadc_v0625 == 0 || chip->vadc_v1250 == 0) {
		pr_debug("No cal yet return %d\n",
				VBATT_MUL_FACTOR * reading_uv);
		return VBATT_MUL_FACTOR * reading_uv;
	}

	numerator = ((s64)reading_uv - chip->vadc_v0625)
							* VADC_CALIB_UV;
	denominator =  (s64)chip->vadc_v1250 - chip->vadc_v0625;
	if (denominator == 0)
		return reading_uv * VBATT_MUL_FACTOR;
	return (VADC_CALIB_UV + div_s64(numerator, denominator))
						* VBATT_MUL_FACTOR;
}

static inline int convert_vbatt_raw_to_uv(struct qpnp_bms_chip *chip,
					uint16_t reading)
{
	return adjust_vbatt_reading(chip, vadc_reading_to_uv(reading));
}

#define CC_READING_RESOLUTION_N	542535
#define CC_READING_RESOLUTION_D	100000
static int cc_reading_to_uv(int16_t reading)
{
	return div_s64(reading * CC_READING_RESOLUTION_N,
					CC_READING_RESOLUTION_D);
}

#define QPNP_ADC_GAIN_NV				17857LL
static s64 cc_adjust_for_gain(s64 uv, s64 gain)
{
	s64 result_uv;

	pr_debug("adjusting_uv = %lld\n", uv);
	pr_debug("adjusting by factor: %lld/%lld = %lld%%\n",
			QPNP_ADC_GAIN_NV, gain,
			div_s64(QPNP_ADC_GAIN_NV * 100LL, gain));

	result_uv = div_s64(uv * QPNP_ADC_GAIN_NV, gain);
	pr_debug("result_uv = %lld\n", result_uv);
	return result_uv;
}

static int convert_vsense_to_uv(struct qpnp_bms_chip *chip,
					int16_t reading)
{
	return cc_adjust_for_gain(cc_reading_to_uv(reading), QPNP_ADC_GAIN_NV);
}

static int read_vsense_avg(struct qpnp_bms_chip *chip, int *result_uv)
{
	int rc;
	int16_t reading;

	rc = qpnp_read_wrapper(chip, (u8 *)&reading,
			chip->base + BMS1_VSENSE_AVG_DATA0, 2);

	if (rc) {
		pr_err("fail to read VSENSE_AVG rc = %d\n", rc);
		return rc;
	}

	*result_uv = convert_vsense_to_uv(chip, reading);
	return 0;
}

static int get_battery_current(struct qpnp_bms_chip *chip, int *result_ua)
{
	int vsense_uv = 0;

	if (chip->r_sense_mohm == 0) {
		pr_err("r_sense is zero\n");
		return -EINVAL;
	}

	mutex_lock(&chip->bms_output_lock);
	lock_output_data(chip);
	read_vsense_avg(chip, &vsense_uv);
	unlock_output_data(chip);
	mutex_unlock(&chip->bms_output_lock);

	pr_debug("vsense_uv=%duV\n", vsense_uv);
	/* cast for signed division */
	*result_ua = vsense_uv * 1000 / (int)chip->r_sense_mohm;
	pr_debug("ibat=%duA\n", *result_ua);
	return 0;
}

static int get_battery_voltage(int *result_uv)
{
	int rc;
	struct qpnp_vadc_result adc_result;

	rc = qpnp_vadc_read(VBAT_SNS, &adc_result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					VBAT_SNS, rc);
		return rc;
	}
	pr_debug("mvolts phy = %lld meas = 0x%llx\n", adc_result.physical,
						adc_result.measurement);
	*result_uv = (int)adc_result.physical;
	return 0;
}

static int read_soc_params_raw(struct qpnp_bms_chip *chip,
				struct raw_soc_params *raw)
{
	/* TODO add real reads */
	return 0;
}

static int calculate_state_of_charge(struct qpnp_bms_chip *chip,
					struct raw_soc_params *raw,
					int batt_temp)
{
	chip->calculated_soc = 50;
	return chip->calculated_soc;
}

static void calculate_soc_work(struct work_struct *work)
{
	struct qpnp_bms_chip *chip = container_of(work,
				struct qpnp_bms_chip,
				calculate_soc_delayed_work.work);
	int batt_temp, rc, soc;
	struct qpnp_vadc_result result;
	struct raw_soc_params raw;

	rc = qpnp_vadc_read(LR_MUX1_BATT_THERM, &result);
	if (rc) {
		pr_err("error reading vadc LR_MUX1_BATT_THERM = %d, rc = %d\n",
					LR_MUX1_BATT_THERM, rc);
		return;
	}
	pr_debug("batt_temp phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	batt_temp = (int)result.physical;

	mutex_lock(&chip->last_ocv_uv_mutex);
	read_soc_params_raw(chip, &raw);
	soc = calculate_state_of_charge(chip, &raw, batt_temp);
	mutex_unlock(&chip->last_ocv_uv_mutex);

	if (soc < chip->low_soc_calc_threshold)
		schedule_delayed_work(&chip->calculate_soc_delayed_work,
			round_jiffies_relative(msecs_to_jiffies
			(chip->low_soc_calculate_soc_ms)));
	else
		schedule_delayed_work(&chip->calculate_soc_delayed_work,
			round_jiffies_relative(msecs_to_jiffies
			(chip->calculate_soc_ms)));
}

/* Returns capacity as a SoC percentage between 0 and 100 */
static int get_prop_bms_capacity(struct qpnp_bms_chip *chip)
{
	return chip->calculated_soc;
}

/* Returns instantaneous current in uA */
static int get_prop_bms_current_now(struct qpnp_bms_chip *chip)
{
	/* temporarily return 0 until a real algorithm is put in */
	int rc, result_ua;

	rc = get_battery_current(chip, &result_ua);
	if (rc) {
		pr_err("failed to get current: %d\n", rc);
		return rc;
	}
	return result_ua;
}

/* Returns full charge design in uAh */
static int get_prop_bms_charge_full_design(struct qpnp_bms_chip *chip)
{
	return chip->fcc;
}

static bool get_prop_bms_online(struct qpnp_bms_chip *chip)
{
	return chip->online;
}

static int get_prop_bms_status(struct qpnp_bms_chip *chip)
{
	return chip->charger_status;
}

static void set_prop_bms_online(struct qpnp_bms_chip *chip, bool online)
{
	chip->online = online;
}

static void set_prop_bms_status(struct qpnp_bms_chip *chip, int status)
{
	chip->charger_status = status;
}

static void qpnp_bms_external_power_changed(struct power_supply *psy)
{
}

static int qpnp_bms_power_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct qpnp_bms_chip *chip = container_of(psy, struct qpnp_bms_chip,
								bms_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_prop_bms_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_prop_bms_current_now(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = get_prop_bms_charge_full_design(chip);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = get_prop_bms_status(chip);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = get_prop_bms_online(chip);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int qpnp_bms_power_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct qpnp_bms_chip *chip = container_of(psy, struct qpnp_bms_chip,
								bms_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		set_prop_bms_online(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		set_prop_bms_status(chip, (bool)val->intval);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void read_shutdown_soc_and_iavg(struct qpnp_bms_chip *chip)
{
	int rc;
	u8 temp;

	if (chip->ignore_shutdown_soc) {
		chip->shutdown_soc_invalid = 1;
		chip->shutdown_soc = 0;
		chip->shutdown_iavg_ma = 0;
	} else {
		rc = qpnp_read_wrapper(chip, &temp,
				chip->base + IAVG_STORAGE_REG, 1);
		if (rc) {
			pr_err("failed to read addr = %d %d assuming %d\n",
					chip->base + IAVG_STORAGE_REG, rc,
					IAVG_START);
			chip->shutdown_iavg_ma = IAVG_START;
		} else {
			if (temp == 0) {
				chip->shutdown_iavg_ma = IAVG_START;
			} else {
				chip->shutdown_iavg_ma = IAVG_START
					+ IAVG_STEP_SIZE_MA * (temp + 1);
			}
		}

		rc = qpnp_read_wrapper(chip, &temp,
				chip->base + SOC_STORAGE_REG, 1);
		if (rc) {
			pr_err("failed to read addr = %d %d\n",
					chip->base + SOC_STORAGE_REG, rc);
		} else {
			chip->shutdown_soc = temp;

			if (chip->shutdown_soc == 0) {
				pr_debug("No shutdown soc available\n");
				chip->shutdown_soc_invalid = 1;
				chip->shutdown_iavg_ma = 0;
			} else if (chip->shutdown_soc == SOC_ZERO) {
				chip->shutdown_soc = 0;
			}
		}
	}

	pr_debug("shutdown_soc = %d shutdown_iavg = %d shutdown_soc_invalid = %d\n",
			chip->shutdown_soc,
			chip->shutdown_iavg_ma,
			chip->shutdown_soc_invalid);
}

#define PALLADIUM_ID_MIN	0x7F40
#define PALLADIUM_ID_MAX	0x7F5A
#define DESAY_5200_ID_MIN	0x7F7F
#define DESAY_5200_ID_MAX	0x802F
static int32_t read_battery_id(struct qpnp_bms_chip *chip)
{
	int rc;
	struct qpnp_vadc_result result;

	rc = qpnp_vadc_read(LR_MUX2_BAT_ID, &result);
	if (rc) {
		pr_err("error reading batt id channel = %d, rc = %d\n",
					LR_MUX2_BAT_ID, rc);
		return rc;
	}
	pr_debug("batt_id phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	pr_debug("raw_code = 0x%x\n", result.adc_code);
	return result.adc_code;
}

static int set_battery_data(struct qpnp_bms_chip *chip)
{
	int64_t battery_id;

	if (chip->batt_type == BATT_DESAY)
		goto desay;
	else if (chip->batt_type == BATT_PALLADIUM)
		goto palladium;

	battery_id = read_battery_id(chip);
	if (battery_id < 0) {
		pr_err("cannot read battery id err = %lld\n", battery_id);
		return battery_id;
	}

	if (is_between(PALLADIUM_ID_MIN, PALLADIUM_ID_MAX, battery_id)) {
		goto palladium;
	} else if (is_between(DESAY_5200_ID_MIN, DESAY_5200_ID_MAX,
				battery_id)) {
		goto desay;
	} else {
		pr_warn("invalid battid, palladium 1500 assumed batt_id %llx\n",
				battery_id);
		goto palladium;
	}

palladium:
		chip->fcc = palladium_1500_data.fcc;
		chip->fcc_temp_lut = palladium_1500_data.fcc_temp_lut;
		chip->fcc_sf_lut = palladium_1500_data.fcc_sf_lut;
		chip->pc_temp_ocv_lut = palladium_1500_data.pc_temp_ocv_lut;
		chip->pc_sf_lut = palladium_1500_data.pc_sf_lut;
		chip->rbatt_sf_lut = palladium_1500_data.rbatt_sf_lut;
		chip->default_rbatt_mohm
				= palladium_1500_data.default_rbatt_mohm;
		goto check_lut;
desay:
		chip->fcc = desay_5200_data.fcc;
		chip->fcc_temp_lut = desay_5200_data.fcc_temp_lut;
		chip->pc_temp_ocv_lut = desay_5200_data.pc_temp_ocv_lut;
		chip->pc_sf_lut = desay_5200_data.pc_sf_lut;
		chip->rbatt_sf_lut = desay_5200_data.rbatt_sf_lut;
		chip->default_rbatt_mohm = desay_5200_data.default_rbatt_mohm;
		goto check_lut;
check_lut:
		if (chip->pc_temp_ocv_lut == NULL) {
			pr_err("temp ocv lut table is NULL\n");
			return -EINVAL;
		}
		return 0;
}

#define SPMI_PROP_READ(chip_prop, qpnp_spmi_property, retval)		\
do {									\
	retval = of_property_read_u32(chip->spmi->dev.of_node,		\
				"qcom,bms-" qpnp_spmi_property,		\
					&chip->chip_prop);		\
	if (retval) {							\
		pr_err("Error reading " #qpnp_spmi_property		\
						" property %d\n", rc);	\
		return -EINVAL;						\
	}								\
} while (0)

static inline int bms_read_properties(struct qpnp_bms_chip *chip)
{
	int rc;

	SPMI_PROP_READ(r_sense_mohm, "r-sense-mohm", rc);
	SPMI_PROP_READ(v_cutoff_uv, "v-cutoff-uv", rc);
	SPMI_PROP_READ(max_voltage_uv, "max-voltage-uv", rc);
	SPMI_PROP_READ(r_conn_mohm, "r-conn-mohm", rc);
	SPMI_PROP_READ(chg_term_ua, "chg-term-ua", rc);
	SPMI_PROP_READ(shutdown_soc_valid_limit,
			"shutdown-soc-valid-limit", rc);
	SPMI_PROP_READ(adjust_soc_high_threshold,
			"adjust-soc-high-threshold", rc);
	SPMI_PROP_READ(adjust_soc_low_threshold,
			"adjust-soc-low-threshold", rc);
	SPMI_PROP_READ(batt_type, "batt-type", rc);
	SPMI_PROP_READ(low_soc_calc_threshold,
			"low-soc-calculate-soc-threshold", rc);
	SPMI_PROP_READ(low_soc_calculate_soc_ms,
			"low-soc-calculate-soc-ms", rc);
	SPMI_PROP_READ(calculate_soc_ms, "calculate-soc-ms", rc);
	chip->ignore_shutdown_soc = of_property_read_bool(
			chip->spmi->dev.of_node,
			"qcom,bms-ignore-shutdown-soc");

	if (chip->adjust_soc_low_threshold >= 45)
		chip->adjust_soc_low_threshold = 45;

	pr_debug("dts data: r_sense_mohm:%d, v_cutoff_uv:%d, max_v:%d\n",
			chip->r_sense_mohm, chip->v_cutoff_uv,
			chip->max_voltage_uv);
	pr_debug("r_conn:%d, shutdown_soc: %d, adjust_soc_low:%d\n",
			chip->r_conn_mohm, chip->shutdown_soc_valid_limit,
			chip->adjust_soc_low_threshold);
	pr_debug("adjust_soc_high:%d, chg_term_ua:%d, batt_type:%d\n",
			chip->adjust_soc_high_threshold, chip->chg_term_ua,
			chip->batt_type);
	pr_debug("ignore_shutdown_soc:%d\n",
			chip->ignore_shutdown_soc);

	return 0;
}

static inline void bms_initialize_constants(struct qpnp_bms_chip *chip)
{
	chip->start_percent = -EINVAL;
	chip->end_percent = -EINVAL;
	chip->prev_pc_unusable = -EINVAL;
	chip->soc_at_cv = -EINVAL;
	chip->calculated_soc = -EINVAL;
}

static int __devinit
qpnp_bms_probe(struct spmi_device *spmi)
{
	struct qpnp_bms_chip *chip;
	struct resource *bms_resource;
	int rc, vbatt;

	chip = kzalloc(sizeof *chip, GFP_KERNEL);

	if (chip == NULL) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}

	chip->dev = &(spmi->dev);
	chip->spmi = spmi;

	mutex_init(&chip->bms_output_lock);
	mutex_init(&chip->last_ocv_uv_mutex);

	bms_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!bms_resource) {
		dev_err(&spmi->dev, "Unable to get BMS base address\n");
		return -ENXIO;
	}
	chip->base = bms_resource->start;

	rc = qpnp_read_wrapper(chip, &chip->revision1,
			chip->base + BMS1_REVISION1, 1);
	if (rc) {
		pr_err("error reading version register %d\n", rc);
		goto error_read;
	}

	rc = qpnp_read_wrapper(chip, &chip->revision2,
			chip->base + BMS1_REVISION2, 1);
	if (rc) {
		pr_err("Error reading version register %d\n", rc);
		goto error_read;
	}

	rc = set_battery_data(chip);
	if (rc) {
		pr_err("Bad battery data %d\n", rc);
		goto error_read;
	}

	rc = bms_read_properties(chip);
	if (rc) {
		pr_err("Unable to read all bms properties, rc = %d\n", rc);
		goto error_read;
	}

	bms_initialize_constants(chip);

	INIT_DELAYED_WORK(&chip->calculate_soc_delayed_work,
			calculate_soc_work);

	read_shutdown_soc_and_iavg(chip);

	dev_set_drvdata(&spmi->dev, chip);
	device_init_wakeup(&spmi->dev, 1);

	calculate_soc_work(&(chip->calculate_soc_delayed_work.work));

	/* setup & register the battery power supply */
	chip->bms_psy.name = "bms";
	chip->bms_psy.type = POWER_SUPPLY_TYPE_BMS;
	chip->bms_psy.properties = msm_bms_power_props;
	chip->bms_psy.num_properties = ARRAY_SIZE(msm_bms_power_props);
	chip->bms_psy.get_property = qpnp_bms_power_get_property;
	chip->bms_psy.set_property = qpnp_bms_power_set_property;
	chip->bms_psy.external_power_changed =
		qpnp_bms_external_power_changed;
	chip->bms_psy.supplied_to = qpnp_bms_supplicants;
	chip->bms_psy.num_supplicants = ARRAY_SIZE(qpnp_bms_supplicants);

	rc = power_supply_register(chip->dev, &chip->bms_psy);

	if (rc < 0) {
		pr_err("power_supply_register bms failed rc = %d\n", rc);
		goto unregister_dc;
	}

	vbatt = 0;
	get_battery_voltage(&vbatt);

	pr_info("OK battery_capacity_at_boot=%d vbatt = %d\n",
				get_prop_bms_capacity(chip),
				vbatt);
	pr_info("probe success\n");
	return 0;

unregister_dc:
	power_supply_unregister(&chip->bms_psy);
	dev_set_drvdata(&spmi->dev, NULL);
error_read:
	kfree(chip);
	return rc;
}

static int __devexit
qpnp_bms_remove(struct spmi_device *spmi)
{
	struct qpnp_bms_chip *chip = dev_get_drvdata(&spmi->dev);

	dev_set_drvdata(&spmi->dev, NULL);
	kfree(chip);
	return 0;
}

static struct spmi_driver qpnp_bms_driver = {
	.probe		= qpnp_bms_probe,
	.remove		= __devexit_p(qpnp_bms_remove),
	.driver		= {
		.name		= QPNP_BMS_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= qpnp_bms_match_table,
	},
};

static int __init qpnp_bms_init(void)
{
	pr_info("QPNP BMS INIT\n");
	return spmi_driver_register(&qpnp_bms_driver);
}

static void __exit qpnp_bms_exit(void)
{
	pr_info("QPNP BMS EXIT\n");
	return spmi_driver_unregister(&qpnp_bms_driver);
}

module_init(qpnp_bms_init);
module_exit(qpnp_bms_exit);

MODULE_DESCRIPTION("QPNP BMS Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_BMS_DEV_NAME);
