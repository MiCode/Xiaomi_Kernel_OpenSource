/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/qpnp/power-on.h>
#include <linux/of_batterydata.h>

/* BMS Register Offsets */
#define REVISION1			0x0
#define REVISION2			0x1
#define BMS1_STATUS1			0x8
#define BMS1_MODE_CTL			0X40
/* Coulomb counter clear registers */
#define BMS1_CC_DATA_CTL		0x42
#define BMS1_CC_CLEAR_CTL		0x43
/* BMS Tolerances */
#define BMS1_TOL_CTL			0X44
/* OCV limit registers */
#define BMS1_OCV_USE_LOW_LIMIT_THR0	0x48
#define BMS1_OCV_USE_LOW_LIMIT_THR1	0x49
#define BMS1_OCV_USE_HIGH_LIMIT_THR0	0x4A
#define BMS1_OCV_USE_HIGH_LIMIT_THR1	0x4B
#define BMS1_OCV_USE_LIMIT_CTL		0x4C
/* Delay control */
#define BMS1_S1_DELAY_CTL		0x5A
/* OCV interrupt threshold */
#define BMS1_OCV_THR0			0x50
#define BMS1_S2_SAMP_AVG_CTL		0x61
/* SW CC interrupt threshold */
#define BMS1_SW_CC_THR0			0xA0
/* OCV for r registers */
#define BMS1_OCV_FOR_R_DATA0		0x80
#define BMS1_VSENSE_FOR_R_DATA0		0x82
/* Coulomb counter data */
#define BMS1_CC_DATA0			0x8A
/* Shadow Coulomb counter data */
#define BMS1_SW_CC_DATA0		0xA8
/* OCV for soc data */
#define BMS1_OCV_FOR_SOC_DATA0		0x90
#define BMS1_VSENSE_PON_DATA0		0x94
#define BMS1_VSENSE_AVG_DATA0		0x98
#define BMS1_VBAT_AVG_DATA0		0x9E
/* Extra bms registers */
#define SOC_STORAGE_REG			0xB0
#define IAVG_STORAGE_REG		0xB1
#define BMS_FCC_COUNT			0xB2
#define BMS_FCC_BASE_REG		0xB3 /* FCC updates - 0xB3 to 0xB7 */
#define BMS_CHGCYL_BASE_REG		0xB8 /* FCC chgcyl - 0xB8 to 0xBC */
#define CHARGE_INCREASE_STORAGE		0xBD
#define CHARGE_CYCLE_STORAGE_LSB	0xBE /* LSB=0xBE, MSB=0xBF */

/* IADC Channel Select */
#define IADC1_BMS_REVISION2		0x01
#define IADC1_BMS_ADC_CH_SEL_CTL	0x48
#define IADC1_BMS_ADC_INT_RSNSN_CTL	0x49
#define IADC1_BMS_FAST_AVG_EN		0x5B

/* Configuration for saving of shutdown soc/iavg */
#define IGNORE_SOC_TEMP_DECIDEG		50
#define IAVG_STEP_SIZE_MA		10
#define IAVG_INVALID			0xFF
#define SOC_INVALID			0x7E

#define IAVG_SAMPLES 16

/* FCC learning constants */
#define MAX_FCC_CYCLES				5
#define DELTA_FCC_PERCENT                       5
#define VALID_FCC_CHGCYL_RANGE                  50
#define CHGCYL_RESOLUTION			20
#define FCC_DEFAULT_TEMP			250

#define QPNP_BMS_DEV_NAME "qcom,qpnp-bms"

enum {
	SHDW_CC,
	CC
};

enum {
	NORESET,
	RESET
};

struct soc_params {
	int		fcc_uah;
	int		cc_uah;
	int		rbatt_mohm;
	int		iavg_ua;
	int		uuc_uah;
	int		ocv_charge_uah;
	int		delta_time_s;
};

struct raw_soc_params {
	uint16_t	last_good_ocv_raw;
	int64_t		cc;
	int64_t		shdw_cc;
	int		last_good_ocv_uv;
};

struct fcc_sample {
	int fcc_new;
	int chargecycles;
};

struct bms_irq {
	unsigned int	irq;
	unsigned long	disabled;
	bool		ready;
};

struct bms_wakeup_source {
	struct wakeup_source	source;
	unsigned long		disabled;
};

struct qpnp_bms_chip {
	struct device			*dev;
	struct power_supply		bms_psy;
	bool				bms_psy_registered;
	struct power_supply		*batt_psy;
	struct spmi_device		*spmi;
	wait_queue_head_t		bms_wait_queue;
	u16				base;
	u16				iadc_base;
	u16				batt_pres_addr;
	u16				soc_storage_addr;

	u8				revision1;
	u8				revision2;

	u8				iadc_bms_revision1;
	u8				iadc_bms_revision2;

	int				battery_present;
	int				battery_status;
	bool				batfet_closed;
	bool				new_battery;
	bool				done_charging;
	bool				last_soc_invalid;
	/* platform data */
	int				r_sense_uohm;
	unsigned int			v_cutoff_uv;
	int				max_voltage_uv;
	int				r_conn_mohm;
	int				shutdown_soc_valid_limit;
	int				adjust_soc_low_threshold;
	int				chg_term_ua;
	enum battery_type		batt_type;
	unsigned int			fcc_mah;
	struct single_row_lut		*fcc_temp_lut;
	struct single_row_lut		*fcc_sf_lut;
	struct pc_temp_ocv_lut		*pc_temp_ocv_lut;
	struct sf_lut			*pc_sf_lut;
	struct sf_lut			*rbatt_sf_lut;
	int				default_rbatt_mohm;
	int				rbatt_capacitive_mohm;
	int				rbatt_mohm;

	struct delayed_work		calculate_soc_delayed_work;
	struct work_struct		recalc_work;
	struct work_struct		batfet_open_work;

	struct mutex			bms_output_lock;
	struct mutex			last_ocv_uv_mutex;
	struct mutex			vbat_monitor_mutex;
	struct mutex			soc_invalidation_mutex;
	struct mutex			last_soc_mutex;
	struct mutex			status_lock;

	bool				use_external_rsense;
	bool				use_ocv_thresholds;

	bool				ignore_shutdown_soc;
	bool				shutdown_soc_invalid;
	int				shutdown_soc;
	int				shutdown_iavg_ma;

	struct wake_lock		low_voltage_wake_lock;
	int				low_voltage_threshold;
	int				low_soc_calc_threshold;
	int				low_soc_calculate_soc_ms;
	int				low_voltage_calculate_soc_ms;
	int				calculate_soc_ms;
	struct bms_wakeup_source	soc_wake_source;
	struct wake_lock		cv_wake_lock;

	uint16_t			ocv_reading_at_100;
	uint16_t			prev_last_good_ocv_raw;
	int				insertion_ocv_uv;
	int				last_ocv_uv;
	int				charging_adjusted_ocv;
	int				last_ocv_temp;
	int				last_cc_uah;
	unsigned long			last_soc_change_sec;
	unsigned long			tm_sec;
	unsigned long			report_tm_sec;
	bool				first_time_calc_soc;
	bool				first_time_calc_uuc;
	int64_t				software_cc_uah;
	int64_t				software_shdw_cc_uah;

	int				iavg_samples_ma[IAVG_SAMPLES];
	int				iavg_index;
	int				iavg_num_samples;
	struct timespec			t_soc_queried;
	int				last_soc;
	int				last_soc_est;
	int				last_soc_unbound;
	bool				was_charging_at_sleep;
	int				charge_start_tm_sec;
	int				catch_up_time_sec;
	struct single_row_lut		*adjusted_fcc_temp_lut;

	struct qpnp_adc_tm_btm_param	vbat_monitor_params;
	struct qpnp_adc_tm_btm_param	die_temp_monitor_params;
	int				temperature_margin;
	unsigned int			vadc_v0625;
	unsigned int			vadc_v1250;

	int				system_load_count;
	int				prev_uuc_iavg_ma;
	int				prev_pc_unusable;
	int				ibat_at_cv_ua;
	int				soc_at_cv;
	int				prev_chg_soc;
	int				calculated_soc;
	int				prev_voltage_based_soc;
	bool				use_voltage_soc;
	bool				in_cv_range;

	int				prev_batt_terminal_uv;
	int				high_ocv_correction_limit_uv;
	int				low_ocv_correction_limit_uv;
	int				flat_ocv_threshold_uv;
	int				hold_soc_est;

	int				ocv_high_threshold_uv;
	int				ocv_low_threshold_uv;
	unsigned long			last_recalc_time;

	struct fcc_sample		*fcc_learning_samples;
	u8				fcc_sample_count;
	int				enable_fcc_learning;
	int				min_fcc_learning_soc;
	int				min_fcc_ocv_pc;
	int				min_fcc_learning_samples;
	int				start_soc;
	int				end_soc;
	int				start_pc;
	int				start_cc_uah;
	int				start_real_soc;
	int				end_cc_uah;
	uint16_t			fcc_new_mah;
	int				fcc_new_batt_temp;
	uint16_t			charge_cycles;
	u8				charge_increase;
	int				fcc_resolution;
	bool				battery_removed;
	struct bms_irq			sw_cc_thr_irq;
	struct bms_irq			ocv_thr_irq;
	struct qpnp_vadc_chip		*vadc_dev;
	struct qpnp_iadc_chip		*iadc_dev;
	struct qpnp_adc_tm_chip		*adc_tm_dev;
};

static struct of_device_id qpnp_bms_match_table[] = {
	{ .compatible = QPNP_BMS_DEV_NAME },
	{}
};

static char *qpnp_bms_supplicants[] = {
	"battery"
};

static enum power_supply_property msm_bms_power_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};

static int discard_backup_fcc_data(struct qpnp_bms_chip *chip);
static void backup_charge_cycle(struct qpnp_bms_chip *chip);

static bool bms_reset;

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

static int qpnp_masked_write_base(struct qpnp_bms_chip *chip, u16 addr,
							u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = qpnp_read_wrapper(chip, &reg, addr, 1);
	if (rc) {
		pr_err("read failed addr = %03X, rc = %d\n", addr, rc);
		return rc;
	}
	reg &= ~mask;
	reg |= val & mask;
	rc = qpnp_write_wrapper(chip, &reg, addr, 1);
	if (rc) {
		pr_err("write failed addr = %03X, val = %02x, mask = %02x, reg = %02x, rc = %d\n",
					addr, val, mask, reg, rc);
		return rc;
	}
	return 0;
}

static int qpnp_masked_write_iadc(struct qpnp_bms_chip *chip, u16 addr,
							u8 mask, u8 val)
{
	return qpnp_masked_write_base(chip, chip->iadc_base + addr, mask, val);
}

static int qpnp_masked_write(struct qpnp_bms_chip *chip, u16 addr,
							u8 mask, u8 val)
{
	return qpnp_masked_write_base(chip, chip->base + addr, mask, val);
}

static void bms_stay_awake(struct bms_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(&source->source);
		pr_debug("enabled source %s\n", source->source.name);
	}
}

static void bms_relax(struct bms_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(&source->source);
		pr_debug("disabled source %s\n", source->source.name);
	}
}

static void enable_bms_irq(struct bms_irq *irq)
{
	if (irq->ready && __test_and_clear_bit(0, &irq->disabled)) {
		enable_irq(irq->irq);
		pr_debug("enabled irq %d\n", irq->irq);
	}
}

static void disable_bms_irq(struct bms_irq *irq)
{
	if (irq->ready && !__test_and_set_bit(0, &irq->disabled)) {
		disable_irq(irq->irq);
		pr_debug("disabled irq %d\n", irq->irq);
	}
}

static void disable_bms_irq_nosync(struct bms_irq *irq)
{
	if (irq->ready && !__test_and_set_bit(0, &irq->disabled)) {
		disable_irq_nosync(irq->irq);
		pr_debug("disabled irq %d\n", irq->irq);
	}
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

static int vadc_reading_to_uv(int reading)
{
	if (reading <= VADC_INTRINSIC_OFFSET)
		return 0;

	return (reading - VADC_INTRINSIC_OFFSET)
			* V_PER_BIT_MUL_FACTOR / V_PER_BIT_DIV_FACTOR;
}

#define VADC_CALIB_UV		625000
#define VBATT_MUL_FACTOR	3
static int adjust_vbatt_reading(struct qpnp_bms_chip *chip, int reading_uv)
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

	numerator = ((s64)reading_uv - chip->vadc_v0625) * VADC_CALIB_UV;
	denominator =  (s64)chip->vadc_v1250 - chip->vadc_v0625;
	if (denominator == 0)
		return reading_uv * VBATT_MUL_FACTOR;
	return (VADC_CALIB_UV + div_s64(numerator, denominator))
						* VBATT_MUL_FACTOR;
}

static int convert_vbatt_uv_to_raw(struct qpnp_bms_chip *chip,
					int unadjusted_vbatt)
{
	int scaled_vbatt = unadjusted_vbatt / VBATT_MUL_FACTOR;

	if (scaled_vbatt <= 0)
		return VADC_INTRINSIC_OFFSET;
	return ((scaled_vbatt * V_PER_BIT_DIV_FACTOR) / V_PER_BIT_MUL_FACTOR)
						+ VADC_INTRINSIC_OFFSET;
}

static inline int convert_vbatt_raw_to_uv(struct qpnp_bms_chip *chip,
					uint16_t reading, bool is_pon_ocv)
{
	int64_t uv;
	int rc;

	uv = vadc_reading_to_uv(reading);
	pr_debug("%u raw converted into %lld uv\n", reading, uv);
	uv = adjust_vbatt_reading(chip, uv);
	pr_debug("adjusted into %lld uv\n", uv);
	rc = qpnp_vbat_sns_comp_result(chip->vadc_dev, &uv, is_pon_ocv);
	if (rc)
		pr_debug("could not compensate vbatt\n");
	pr_debug("compensated into %lld uv\n", uv);
	return uv;
}

#define CC_READING_RESOLUTION_N	542535
#define CC_READING_RESOLUTION_D	100000
static s64 cc_reading_to_uv(s64 reading)
{
	return div_s64(reading * CC_READING_RESOLUTION_N,
					CC_READING_RESOLUTION_D);
}

#define QPNP_ADC_GAIN_IDEAL				3291LL
static s64 cc_adjust_for_gain(s64 uv, uint16_t gain)
{
	s64 result_uv;

	pr_debug("adjusting_uv = %lld\n", uv);
	if (gain == 0) {
		pr_debug("gain is %d, not adjusting\n", gain);
		return uv;
	}
	pr_debug("adjusting by factor: %lld/%hu = %lld%%\n",
			QPNP_ADC_GAIN_IDEAL, gain,
			div_s64(QPNP_ADC_GAIN_IDEAL * 100LL, (s64)gain));

	result_uv = div_s64(uv * QPNP_ADC_GAIN_IDEAL, (s64)gain);
	pr_debug("result_uv = %lld\n", result_uv);
	return result_uv;
}

static s64 cc_reverse_adjust_for_gain(struct qpnp_bms_chip *chip, s64 uv)
{
	struct qpnp_iadc_calib calibration;
	int gain;
	s64 result_uv;

	qpnp_iadc_get_gain_and_offset(chip->iadc_dev, &calibration);
	gain = (int)calibration.gain_raw - (int)calibration.offset_raw;

	pr_debug("reverse adjusting_uv = %lld\n", uv);
	if (gain == 0) {
		pr_debug("gain is %d, not adjusting\n", gain);
		return uv;
	}
	pr_debug("adjusting by factor: %hu/%lld = %lld%%\n",
			gain, QPNP_ADC_GAIN_IDEAL,
			div64_s64((s64)gain * 100LL,
				(s64)QPNP_ADC_GAIN_IDEAL));

	result_uv = div64_s64(uv * (s64)gain, QPNP_ADC_GAIN_IDEAL);
	pr_debug("result_uv = %lld\n", result_uv);
	return result_uv;
}

static int convert_vsense_to_uv(struct qpnp_bms_chip *chip,
					int16_t reading)
{
	struct qpnp_iadc_calib calibration;

	qpnp_iadc_get_gain_and_offset(chip->iadc_dev, &calibration);
	return cc_adjust_for_gain(cc_reading_to_uv(reading),
			calibration.gain_raw - calibration.offset_raw);
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
	int rc, vsense_uv = 0;
	int64_t temp_current;

	if (chip->r_sense_uohm == 0) {
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
	temp_current = div_s64((vsense_uv * 1000000LL),
				(int)chip->r_sense_uohm);

	*result_ua = temp_current;
	rc = qpnp_iadc_comp_result(chip->iadc_dev, &temp_current);
	if (rc)
		pr_debug("error compensation failed: %d\n", rc);

	pr_debug("%d uA err compensated ibat=%llduA\n",
			*result_ua, temp_current);
	*result_ua = temp_current;
	return 0;
}

static int get_battery_voltage(struct qpnp_bms_chip *chip, int *result_uv)
{
	int rc;
	struct qpnp_vadc_result adc_result;

	rc = qpnp_vadc_read(chip->vadc_dev, VBAT_SNS, &adc_result);
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

#define CC_36_BIT_MASK 0xFFFFFFFFFLL
static uint64_t convert_s64_to_s36(int64_t raw64)
{
	return (uint64_t) raw64 & CC_36_BIT_MASK;
}

#define SIGN_EXTEND_36_TO_64_MASK (-1LL ^ CC_36_BIT_MASK)
static int64_t convert_s36_to_s64(uint64_t raw36)
{
	raw36 = raw36 & CC_36_BIT_MASK;
	/* convert 36 bit signed value into 64 signed value */
	return (raw36 >> 35) == 0LL ?
		raw36 : (SIGN_EXTEND_36_TO_64_MASK | raw36);
}

static int read_cc_raw(struct qpnp_bms_chip *chip, int64_t *reading,
							int cc_type)
{
	int64_t raw_reading;
	int rc;

	if (cc_type == SHDW_CC)
		rc = qpnp_read_wrapper(chip, (u8 *)&raw_reading,
				chip->base + BMS1_SW_CC_DATA0, 5);
	else
		rc = qpnp_read_wrapper(chip, (u8 *)&raw_reading,
				chip->base + BMS1_CC_DATA0, 5);
	if (rc) {
		pr_err("Error reading cc: rc = %d\n", rc);
		return -ENXIO;
	}

	*reading = convert_s36_to_s64(raw_reading);

	return 0;
}

static int calib_vadc(struct qpnp_bms_chip *chip)
{
	int rc, raw_0625, raw_1250;
	struct qpnp_vadc_result result;

	rc = qpnp_vadc_read(chip->vadc_dev, REF_625MV, &result);
	if (rc) {
		pr_debug("vadc read failed with rc = %d\n", rc);
		return rc;
	}
	raw_0625 = result.adc_code;

	rc = qpnp_vadc_read(chip->vadc_dev, REF_125V, &result);
	if (rc) {
		pr_debug("vadc read failed with rc = %d\n", rc);
		return rc;
	}
	raw_1250 = result.adc_code;
	chip->vadc_v0625 = vadc_reading_to_uv(raw_0625);
	chip->vadc_v1250 = vadc_reading_to_uv(raw_1250);
	pr_debug("vadc calib: 0625 = %d raw (%d uv), 1250 = %d raw (%d uv)\n",
			raw_0625, chip->vadc_v0625,
			raw_1250, chip->vadc_v1250);
	return 0;
}

static void convert_and_store_ocv(struct qpnp_bms_chip *chip,
				struct raw_soc_params *raw,
				int batt_temp, bool is_pon_ocv)
{
	int rc;

	pr_debug("prev_last_good_ocv_raw = %d, last_good_ocv_raw = %d\n",
			chip->prev_last_good_ocv_raw,
			raw->last_good_ocv_raw);
	rc = calib_vadc(chip);
	if (rc)
		pr_err("Vadc reference voltage read failed, rc = %d\n", rc);
	chip->prev_last_good_ocv_raw = raw->last_good_ocv_raw;
	raw->last_good_ocv_uv = convert_vbatt_raw_to_uv(chip,
					raw->last_good_ocv_raw, is_pon_ocv);
	chip->last_ocv_uv = raw->last_good_ocv_uv;
	chip->last_ocv_temp = batt_temp;
	chip->software_cc_uah = 0;
	pr_debug("last_good_ocv_uv = %d\n", raw->last_good_ocv_uv);
}

#define CLEAR_CC			BIT(7)
#define CLEAR_SHDW_CC			BIT(6)
/**
 * reset both cc and sw-cc.
 * note: this should only be ever called from one thread
 * or there may be a race condition where CC is never enabled
 * again
 */
static void reset_cc(struct qpnp_bms_chip *chip, u8 flags)
{
	int rc;

	pr_debug("resetting cc manually with flags %hhu\n", flags);
	mutex_lock(&chip->bms_output_lock);
	rc = qpnp_masked_write(chip, BMS1_CC_CLEAR_CTL,
				flags,
				flags);
	if (rc)
		pr_err("cc reset failed: %d\n", rc);

	/* wait for 100us for cc to reset */
	udelay(100);

	rc = qpnp_masked_write(chip, BMS1_CC_CLEAR_CTL,
				flags, 0);
	if (rc)
		pr_err("cc reenable failed: %d\n", rc);
	mutex_unlock(&chip->bms_output_lock);
}

static int get_battery_status(struct qpnp_bms_chip *chip)
{
	union power_supply_propval ret = {0,};
	int rc;

	if (chip->batt_psy == NULL)
		chip->batt_psy = power_supply_get_by_name("battery");
	if (chip->batt_psy) {
		/* if battery has been registered, use the status property */
		rc = chip->batt_psy->get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_STATUS, &ret);
		if (rc) {
			pr_debug("Battery does not export status: %d\n", rc);
			return POWER_SUPPLY_STATUS_UNKNOWN;
		}
		return ret.intval;
	}

	/* Default to false if the battery power supply is not registered. */
	pr_debug("battery power supply is not registered\n");
	return POWER_SUPPLY_STATUS_UNKNOWN;
}

static bool is_battery_charging(struct qpnp_bms_chip *chip)
{
	return get_battery_status(chip) == POWER_SUPPLY_STATUS_CHARGING;
}

static bool is_battery_full(struct qpnp_bms_chip *chip)
{
	return get_battery_status(chip) == POWER_SUPPLY_STATUS_FULL;
}

#define BAT_PRES_BIT		BIT(7)
static bool is_battery_present(struct qpnp_bms_chip *chip)
{
	union power_supply_propval ret = {0,};
	int rc;
	u8 batt_pres;

	/* first try to use the batt_pres register if given */
	if (chip->batt_pres_addr) {
		rc = qpnp_read_wrapper(chip, &batt_pres,
				chip->batt_pres_addr, 1);
		if (!rc && (batt_pres & BAT_PRES_BIT))
			return true;
		else
			return false;
	}
	if (chip->batt_psy == NULL)
		chip->batt_psy = power_supply_get_by_name("battery");
	if (chip->batt_psy) {
		/* if battery has been registered, use the present property */
		rc = chip->batt_psy->get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_PRESENT, &ret);
		if (rc) {
			pr_debug("battery does not export present: %d\n", rc);
			return true;
		}
		return ret.intval;
	}

	/* Default to false if the battery power supply is not registered. */
	pr_debug("battery power supply is not registered\n");
	return false;
}

static int get_battery_insertion_ocv_uv(struct qpnp_bms_chip *chip)
{
	union power_supply_propval ret = {0,};
	int rc, vbat;

	if (chip->batt_psy == NULL)
		chip->batt_psy = power_supply_get_by_name("battery");
	if (chip->batt_psy) {
		/* if battery has been registered, use the ocv property */
		rc = chip->batt_psy->get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_OCV, &ret);
		if (rc) {
			/*
			 * Default to vbatt if the battery OCV is not
			 * registered.
			 */
			pr_debug("Battery psy does not have voltage ocv\n");
			rc = get_battery_voltage(chip, &vbat);
			if (rc)
				return -EINVAL;
			return vbat;
		}
		return ret.intval;
	}

	pr_debug("battery power supply is not registered\n");
	return -EINVAL;
}

static bool is_batfet_closed(struct qpnp_bms_chip *chip)
{
	union power_supply_propval ret = {0,};
	int rc;

	if (chip->batt_psy == NULL)
		chip->batt_psy = power_supply_get_by_name("battery");
	if (chip->batt_psy) {
		/* if battery has been registered, use the online property */
		rc = chip->batt_psy->get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_ONLINE, &ret);
		if (rc) {
			pr_debug("Battery does not export online: %d\n", rc);
			return true;
		}
		return !!ret.intval;
	}

	/* Default to true if the battery power supply is not registered. */
	pr_debug("battery power supply is not registered\n");
	return true;
}

static int get_simultaneous_batt_v_and_i(struct qpnp_bms_chip *chip,
					int *ibat_ua, int *vbat_uv)
{
	struct qpnp_iadc_result i_result;
	struct qpnp_vadc_result v_result;
	enum qpnp_iadc_channels iadc_channel;
	int rc;

	iadc_channel = chip->use_external_rsense ?
				EXTERNAL_RSENSE : INTERNAL_RSENSE;
	if (is_battery_full(chip)) {
		rc = get_battery_current(chip, ibat_ua);
		if (rc) {
			pr_err("bms current read failed with rc: %d\n", rc);
			return rc;
		}
		rc = qpnp_vadc_read(chip->vadc_dev, VBAT_SNS, &v_result);
		if (rc) {
			pr_err("vadc read failed with rc: %d\n", rc);
			return rc;
		}
		*vbat_uv = (int)v_result.physical;
	} else {
		rc = qpnp_iadc_vadc_sync_read(chip->iadc_dev,
					iadc_channel, &i_result,
					VBAT_SNS, &v_result);
		if (rc) {
			pr_err("adc sync read failed with rc: %d\n", rc);
			return rc;
		}
		/*
		* reverse the current read by the iadc, since the bms uses
		* flipped battery current polarity.
		*/
		*ibat_ua = -1 * (int)i_result.result_ua;
		*vbat_uv = (int)v_result.physical;
	}

	return 0;
}

static int estimate_ocv(struct qpnp_bms_chip *chip)
{
	int ibat_ua, vbat_uv, ocv_est_uv;
	int rc;
	int rbatt_mohm = chip->default_rbatt_mohm + chip->r_conn_mohm
					+ chip->rbatt_capacitive_mohm;

	rc = get_simultaneous_batt_v_and_i(chip, &ibat_ua, &vbat_uv);
	if (rc) {
		pr_err("simultaneous failed rc = %d\n", rc);
		return rc;
	}

	ocv_est_uv = vbat_uv + (ibat_ua * rbatt_mohm) / 1000;
	pr_debug("estimated pon ocv = %d\n", ocv_est_uv);
	return ocv_est_uv;
}

#define MIN_IAVG_MA 250
static void reset_for_new_battery(struct qpnp_bms_chip *chip, int batt_temp)
{
	chip->last_ocv_uv = chip->insertion_ocv_uv;
	mutex_lock(&chip->last_soc_mutex);
	chip->last_soc = -EINVAL;
	chip->last_soc_invalid = true;
	mutex_unlock(&chip->last_soc_mutex);
	chip->soc_at_cv = -EINVAL;
	chip->shutdown_soc_invalid = true;
	chip->shutdown_soc = 0;
	chip->shutdown_iavg_ma = MIN_IAVG_MA;
	chip->prev_pc_unusable = -EINVAL;
	reset_cc(chip, CLEAR_CC | CLEAR_SHDW_CC);
	chip->software_cc_uah = 0;
	chip->software_shdw_cc_uah = 0;
	chip->last_cc_uah = INT_MIN;
	chip->last_ocv_temp = batt_temp;
	chip->prev_batt_terminal_uv = 0;
	if (chip->enable_fcc_learning) {
		chip->adjusted_fcc_temp_lut = NULL;
		chip->fcc_new_mah = -EINVAL;
		/* reset the charge-cycle and charge-increase registers */
		chip->charge_increase = 0;
		chip->charge_cycles = 0;
		backup_charge_cycle(chip);
		/* discard all the FCC learnt data and reset the local table */
		discard_backup_fcc_data(chip);
		memset(chip->fcc_learning_samples, 0,
			chip->min_fcc_learning_samples *
				sizeof(struct fcc_sample));
	}
}

#define SIGN(x) ((x) < 0 ? -1 : 1)
#define UV_PER_SPIN 50000
static int find_ocv_for_pc(struct qpnp_bms_chip *chip, int batt_temp, int pc)
{
	int new_pc;
	int ocv_mv;
	int delta_mv = 5;
	int max_spin_count;
	int count = 0;
	int sign, new_sign;

	ocv_mv = interpolate_ocv(chip->pc_temp_ocv_lut, batt_temp, pc);

	new_pc = interpolate_pc(chip->pc_temp_ocv_lut, batt_temp, ocv_mv);
	pr_debug("test revlookup pc = %d for ocv = %d\n", new_pc, ocv_mv);
	max_spin_count = 1 + (chip->max_voltage_uv - chip->v_cutoff_uv)
						/ UV_PER_SPIN;
	sign = SIGN(pc - new_pc);

	while (abs(new_pc - pc) != 0 && count < max_spin_count) {
		/*
		 * If the newly interpolated pc is larger than the lookup pc,
		 * the ocv should be reduced and vice versa
		 */
		new_sign = SIGN(pc - new_pc);
		/*
		 * If the sign has changed, then we have passed the lookup pc.
		 * reduce the ocv step size to get finer results.
		 *
		 * If we have already reduced the ocv step size and still
		 * passed the lookup pc, just stop and use the current ocv.
		 * This can only happen if the batterydata profile is
		 * non-monotonic anyways.
		 */
		if (new_sign != sign) {
			if (delta_mv > 1)
				delta_mv = 1;
			else
				break;
		}
		sign = new_sign;

		ocv_mv = ocv_mv + delta_mv * sign;
		new_pc = interpolate_pc(chip->pc_temp_ocv_lut,
				batt_temp, ocv_mv);
		pr_debug("test revlookup pc = %d for ocv = %d\n",
			new_pc, ocv_mv);
		count++;
	}

	return ocv_mv * 1000;
}

#define OCV_RAW_UNINITIALIZED	0xFFFF
#define MIN_OCV_UV		2000000
static int read_soc_params_raw(struct qpnp_bms_chip *chip,
				struct raw_soc_params *raw,
				int batt_temp)
{
	int warm_reset, rc;

	mutex_lock(&chip->bms_output_lock);

	lock_output_data(chip);

	rc = qpnp_read_wrapper(chip, (u8 *)&raw->last_good_ocv_raw,
			chip->base + BMS1_OCV_FOR_SOC_DATA0, 2);
	if (rc) {
		pr_err("Error reading ocv: rc = %d\n", rc);
		goto param_err;
	}

	rc = read_cc_raw(chip, &raw->cc, CC);
	rc = read_cc_raw(chip, &raw->shdw_cc, SHDW_CC);
	if (rc) {
		pr_err("Failed to read raw cc data, rc = %d\n", rc);
		goto param_err;
	}

	unlock_output_data(chip);
	mutex_unlock(&chip->bms_output_lock);

	if (chip->prev_last_good_ocv_raw == OCV_RAW_UNINITIALIZED) {
		convert_and_store_ocv(chip, raw, batt_temp, true);
		pr_debug("PON_OCV_UV = %d, cc = %llx\n",
				chip->last_ocv_uv, raw->cc);
		warm_reset = qpnp_pon_is_warm_reset();
		if (raw->last_good_ocv_uv < MIN_OCV_UV
				|| warm_reset > 0) {
			pr_debug("OCV is stale or bad, estimating new OCV.\n");
			chip->last_ocv_uv = estimate_ocv(chip);
			raw->last_good_ocv_uv = chip->last_ocv_uv;
			reset_cc(chip, CLEAR_CC | CLEAR_SHDW_CC);
			pr_debug("New PON_OCV_UV = %d, cc = %llx\n",
					chip->last_ocv_uv, raw->cc);
		}
	} else if (chip->new_battery) {
		/* if a new battery was inserted, estimate the ocv */
		reset_for_new_battery(chip, batt_temp);
		raw->cc = 0;
		raw->shdw_cc = 0;
		raw->last_good_ocv_uv = chip->last_ocv_uv;
		chip->new_battery = false;
	} else if (chip->done_charging) {
		chip->done_charging = false;
		/* if we just finished charging, reset CC and fake 100% */
		chip->ocv_reading_at_100 = raw->last_good_ocv_raw;
		chip->last_ocv_uv = find_ocv_for_pc(chip, batt_temp, 100);
		raw->last_good_ocv_uv = chip->last_ocv_uv;
		raw->cc = 0;
		raw->shdw_cc = 0;
		reset_cc(chip, CLEAR_CC | CLEAR_SHDW_CC);
		chip->last_ocv_temp = batt_temp;
		chip->software_cc_uah = 0;
		chip->software_shdw_cc_uah = 0;
		chip->last_cc_uah = INT_MIN;
		pr_debug("EOC Battery full ocv_reading = 0x%x\n",
				chip->ocv_reading_at_100);
	} else if (chip->prev_last_good_ocv_raw != raw->last_good_ocv_raw) {
		convert_and_store_ocv(chip, raw, batt_temp, false);
		/* forget the old cc value upon ocv */
		chip->last_cc_uah = INT_MIN;
	} else {
		raw->last_good_ocv_uv = chip->last_ocv_uv;
	}

	/* stop faking a high OCV if we get a new OCV */
	if (chip->ocv_reading_at_100 != raw->last_good_ocv_raw)
		chip->ocv_reading_at_100 = OCV_RAW_UNINITIALIZED;

	pr_debug("last_good_ocv_raw= 0x%x, last_good_ocv_uv= %duV\n",
			raw->last_good_ocv_raw, raw->last_good_ocv_uv);
	pr_debug("cc_raw= 0x%llx\n", raw->cc);
	return 0;

param_err:
	unlock_output_data(chip);
	mutex_unlock(&chip->bms_output_lock);
	return rc;
}

static int calculate_pc(struct qpnp_bms_chip *chip, int ocv_uv,
							int batt_temp)
{
	int pc;

	pc = interpolate_pc(chip->pc_temp_ocv_lut,
			batt_temp, ocv_uv / 1000);
	pr_debug("pc = %u %% for ocv = %d uv batt_temp = %d\n",
					pc, ocv_uv, batt_temp);
	/* Multiply the initial FCC value by the scale factor. */
	return pc;
}

static int calculate_fcc(struct qpnp_bms_chip *chip, int batt_temp)
{
	int fcc_uah;

	if (chip->adjusted_fcc_temp_lut == NULL) {
		/* interpolate_fcc returns a mv value. */
		fcc_uah = interpolate_fcc(chip->fcc_temp_lut,
						batt_temp) * 1000;
		pr_debug("fcc = %d uAh\n", fcc_uah);
		return fcc_uah;
	} else {
		return 1000 * interpolate_fcc(chip->adjusted_fcc_temp_lut,
				batt_temp);
	}
}

/* calculate remaining charge at the time of ocv */
static int calculate_ocv_charge(struct qpnp_bms_chip *chip,
						struct raw_soc_params *raw,
						int fcc_uah)
{
	int  ocv_uv, pc;

	ocv_uv = raw->last_good_ocv_uv;
	pc = calculate_pc(chip, ocv_uv, chip->last_ocv_temp);
	pr_debug("ocv_uv = %d pc = %d\n", ocv_uv, pc);
	return (fcc_uah * pc) / 100;
}

#define CC_READING_TICKS	56
#define SLEEP_CLK_HZ		32764
#define SECONDS_PER_HOUR	3600

static s64 cc_uv_to_pvh(s64 cc_uv)
{
	/* Note that it is necessary need to multiply by 1000000 to convert
	 * from uvh to pvh here.
	 * However, the maximum Coulomb Counter value is 2^35, which can cause
	 * an over flow.
	 * Multiply by 100000 first to perserve as much precision as possible
	 * then multiply by 10 after doing the division in order to avoid
	 * overflow on the maximum Coulomb Counter value.
	 */
	return div_s64(cc_uv * CC_READING_TICKS * 100000,
			SLEEP_CLK_HZ * SECONDS_PER_HOUR) * 10;
}

/**
 * calculate_cc() - converts a hardware coulomb counter reading into uah
 * @chip:		the bms chip pointer
 * @cc:			the cc reading from bms h/w
 * @cc_type:		calcualte cc from regular or shadow coulomb counter
 * @clear_cc:		whether this function should clear the hardware counter
 *			after reading
 *
 * Converts the 64 bit hardware coulomb counter into microamp-hour by taking
 * into account hardware resolution and adc errors.
 *
 * Return: the coulomb counter based charge in uAh (micro-amp hour)
 */
static int calculate_cc(struct qpnp_bms_chip *chip, int64_t cc,
					int cc_type, int clear_cc)
{
	struct qpnp_iadc_calib calibration;
	struct qpnp_vadc_result result;
	int64_t cc_voltage_uv, cc_pvh, cc_uah, *software_counter;
	int rc;

	software_counter = cc_type == SHDW_CC ?
			&chip->software_shdw_cc_uah : &chip->software_cc_uah;
	rc = qpnp_vadc_read(chip->vadc_dev, DIE_TEMP, &result);
	if (rc) {
		pr_err("could not read pmic die temperature: %d\n", rc);
		return *software_counter;
	}

	qpnp_iadc_get_gain_and_offset(chip->iadc_dev, &calibration);
	pr_debug("%scc = %lld, die_temp = %lld\n",
			cc_type == SHDW_CC ? "shdw_" : "",
			cc, result.physical);
	cc_voltage_uv = cc_reading_to_uv(cc);
	cc_voltage_uv = cc_adjust_for_gain(cc_voltage_uv,
					calibration.gain_raw
					- calibration.offset_raw);
	cc_pvh = cc_uv_to_pvh(cc_voltage_uv);
	cc_uah = div_s64(cc_pvh, chip->r_sense_uohm);
	rc = qpnp_iadc_comp_result(chip->iadc_dev, &cc_uah);
	if (rc)
		pr_debug("error compensation failed: %d\n", rc);
	if (clear_cc == RESET) {
		pr_debug("software_%scc = %lld, added cc_uah = %lld\n",
				cc_type == SHDW_CC ? "sw_" : "",
				*software_counter, cc_uah);
		*software_counter += cc_uah;
		reset_cc(chip, cc_type == SHDW_CC ? CLEAR_SHDW_CC : CLEAR_CC);
		return (int)*software_counter;
	} else {
		pr_debug("software_%scc = %lld, cc_uah = %lld, total = %lld\n",
				cc_type == SHDW_CC ? "shdw_" : "",
				*software_counter, cc_uah,
				*software_counter + cc_uah);
		return *software_counter + cc_uah;
	}
}

static int get_rbatt(struct qpnp_bms_chip *chip,
					int soc_rbatt_mohm, int batt_temp)
{
	int rbatt_mohm, scalefactor;

	rbatt_mohm = chip->default_rbatt_mohm;
	if (chip->rbatt_sf_lut == NULL)  {
		pr_debug("RBATT = %d\n", rbatt_mohm);
		return rbatt_mohm;
	}
	/* Convert the batt_temp to DegC from deciDegC */
	scalefactor = interpolate_scalingfactor(chip->rbatt_sf_lut,
						batt_temp, soc_rbatt_mohm);
	rbatt_mohm = (rbatt_mohm * scalefactor) / 100;

	rbatt_mohm += chip->r_conn_mohm;
	rbatt_mohm += chip->rbatt_capacitive_mohm;
	return rbatt_mohm;
}

#define IAVG_MINIMAL_TIME	2
static void calculate_iavg(struct qpnp_bms_chip *chip, int cc_uah,
				int *iavg_ua, int delta_time_s)
{
	int delta_cc_uah = 0;

	/*
	 * use the battery current if called too quickly
	 */
	if (delta_time_s < IAVG_MINIMAL_TIME
			|| chip->last_cc_uah == INT_MIN) {
		get_battery_current(chip, iavg_ua);
		goto out;
	}

	delta_cc_uah = cc_uah - chip->last_cc_uah;

	*iavg_ua = div_s64((s64)delta_cc_uah * 3600, delta_time_s);

out:
	pr_debug("delta_cc = %d iavg_ua = %d\n", delta_cc_uah, (int)*iavg_ua);

	/* remember cc_uah */
	chip->last_cc_uah = cc_uah;
}

static int calculate_termination_uuc(struct qpnp_bms_chip *chip,
					struct soc_params *params,
					int batt_temp, int uuc_iavg_ma,
					int *ret_pc_unusable)
{
	int unusable_uv, pc_unusable, uuc_uah;
	int i = 0;
	int ocv_mv;
	int rbatt_mohm;
	int delta_uv;
	int prev_delta_uv = 0;
	int prev_rbatt_mohm = 0;
	int uuc_rbatt_mohm;

	for (i = 0; i <= 100; i++) {
		ocv_mv = interpolate_ocv(chip->pc_temp_ocv_lut,
				batt_temp, i);
		rbatt_mohm = get_rbatt(chip, i, batt_temp);
		unusable_uv = (rbatt_mohm * uuc_iavg_ma)
							+ (chip->v_cutoff_uv);
		delta_uv = ocv_mv * 1000 - unusable_uv;

		if (delta_uv > 0)
			break;

		prev_delta_uv = delta_uv;
		prev_rbatt_mohm = rbatt_mohm;
	}

	uuc_rbatt_mohm = linear_interpolate(rbatt_mohm, delta_uv,
					prev_rbatt_mohm, prev_delta_uv,
					0);

	unusable_uv = (uuc_rbatt_mohm * uuc_iavg_ma) + (chip->v_cutoff_uv);

	pc_unusable = calculate_pc(chip, unusable_uv, batt_temp);
	uuc_uah = (params->fcc_uah * pc_unusable) / 100;
	pr_debug("For uuc_iavg_ma = %d, unusable_rbatt = %d unusable_uv = %d unusable_pc = %d rbatt_pc = %d uuc = %d\n",
					uuc_iavg_ma,
					uuc_rbatt_mohm, unusable_uv,
					pc_unusable, i, uuc_uah);
	*ret_pc_unusable = pc_unusable;
	return uuc_uah;
}

#define TIME_PER_PERCENT_UUC			60
static int adjust_uuc(struct qpnp_bms_chip *chip,
			struct soc_params *params,
			int new_pc_unusable,
			int new_uuc_uah,
			int batt_temp)
{
	int new_unusable_mv, new_iavg_ma;
	int max_percent_change;

	max_percent_change = max(params->delta_time_s
				/ TIME_PER_PERCENT_UUC, 1);

	if (chip->first_time_calc_uuc || chip->prev_pc_unusable == -EINVAL
		|| abs(chip->prev_pc_unusable - new_pc_unusable)
			<= max_percent_change) {
		chip->prev_pc_unusable = new_pc_unusable;
		return new_uuc_uah;
	}

	/* the uuc is trying to change more than 1% restrict it */
	if (new_pc_unusable > chip->prev_pc_unusable)
		chip->prev_pc_unusable += max_percent_change;
	else
		chip->prev_pc_unusable -= max_percent_change;

	new_uuc_uah = (params->fcc_uah * chip->prev_pc_unusable) / 100;

	/* also find update the iavg_ma accordingly */
	new_unusable_mv = interpolate_ocv(chip->pc_temp_ocv_lut,
			batt_temp, chip->prev_pc_unusable);
	if (new_unusable_mv < chip->v_cutoff_uv/1000)
		new_unusable_mv = chip->v_cutoff_uv/1000;

	new_iavg_ma = (new_unusable_mv * 1000 - chip->v_cutoff_uv)
						/ params->rbatt_mohm;
	if (new_iavg_ma == 0)
		new_iavg_ma = 1;
	chip->prev_uuc_iavg_ma = new_iavg_ma;
	pr_debug("Restricting UUC to %d (%d%%) unusable_mv = %d iavg_ma = %d\n",
					new_uuc_uah, chip->prev_pc_unusable,
					new_unusable_mv, new_iavg_ma);

	return new_uuc_uah;
}

static int calculate_unusable_charge_uah(struct qpnp_bms_chip *chip,
					struct soc_params *params,
					int batt_temp)
{
	int uuc_uah_iavg;
	int i;
	int uuc_iavg_ma = params->iavg_ua / 1000;
	int pc_unusable;

	/*
	 * if called first time, fill all the samples with
	 * the shutdown_iavg_ma
	 */
	if (chip->first_time_calc_uuc && chip->shutdown_iavg_ma != 0) {
		pr_debug("Using shutdown_iavg_ma = %d in all samples\n",
				chip->shutdown_iavg_ma);
		for (i = 0; i < IAVG_SAMPLES; i++)
			chip->iavg_samples_ma[i] = chip->shutdown_iavg_ma;

		chip->iavg_index = 0;
		chip->iavg_num_samples = IAVG_SAMPLES;
	}

	if (params->delta_time_s >= IAVG_MINIMAL_TIME) {
		/*
		* if charging use a nominal avg current to keep
		* a reasonable UUC while charging
		*/
		if (uuc_iavg_ma < MIN_IAVG_MA)
			uuc_iavg_ma = MIN_IAVG_MA;
		chip->iavg_samples_ma[chip->iavg_index] = uuc_iavg_ma;
		chip->iavg_index = (chip->iavg_index + 1) % IAVG_SAMPLES;
		chip->iavg_num_samples++;
		if (chip->iavg_num_samples >= IAVG_SAMPLES)
			chip->iavg_num_samples = IAVG_SAMPLES;
	}

	/* now that this sample is added calcualte the average */
	uuc_iavg_ma = 0;
	if (chip->iavg_num_samples != 0) {
		for (i = 0; i < chip->iavg_num_samples; i++) {
			pr_debug("iavg_samples_ma[%d] = %d\n", i,
					chip->iavg_samples_ma[i]);
			uuc_iavg_ma += chip->iavg_samples_ma[i];
		}

		uuc_iavg_ma = DIV_ROUND_CLOSEST(uuc_iavg_ma,
						chip->iavg_num_samples);
	}

	/*
	 * if we're in bms reset mode, force uuc to be 3% of fcc
	 */
	if (bms_reset)
		return (params->fcc_uah * 3) / 100;

	uuc_uah_iavg = calculate_termination_uuc(chip, params, batt_temp,
						uuc_iavg_ma, &pc_unusable);
	pr_debug("uuc_iavg_ma = %d uuc with iavg = %d\n",
						uuc_iavg_ma, uuc_uah_iavg);

	chip->prev_uuc_iavg_ma = uuc_iavg_ma;
	/* restrict the uuc such that it can increase only by one percent */
	uuc_uah_iavg = adjust_uuc(chip, params, pc_unusable,
					uuc_uah_iavg, batt_temp);

	return uuc_uah_iavg;
}

static s64 find_ocv_charge_for_soc(struct qpnp_bms_chip *chip,
				struct soc_params *params, int soc)
{
	return div_s64((s64)soc * (params->fcc_uah - params->uuc_uah),
			100) + params->cc_uah + params->uuc_uah;
}

static int find_pc_for_soc(struct qpnp_bms_chip *chip,
			struct soc_params *params, int soc)
{
	int ocv_charge_uah = find_ocv_charge_for_soc(chip, params, soc);
	int pc;

	pc = DIV_ROUND_CLOSEST((int)ocv_charge_uah * 100, params->fcc_uah);
	pc = clamp(pc, 0, 100);
	pr_debug("soc = %d, fcc = %d uuc = %d rc = %d pc = %d\n",
			soc, params->fcc_uah, params->uuc_uah,
			ocv_charge_uah, pc);
	return pc;
}

static int get_current_time(unsigned long *now_tm_sec)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return -EINVAL;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Error reading rtc device (%s) : %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}
	rtc_tm_to_time(&tm, now_tm_sec);

close_time:
	rtc_class_close(rtc);
	return rc;
}

/* Returns estimated battery resistance */
static int get_prop_bms_batt_resistance(struct qpnp_bms_chip *chip)
{
	return chip->rbatt_mohm * 1000;
}

/* Returns instantaneous current in uA */
static int get_prop_bms_current_now(struct qpnp_bms_chip *chip)
{
	int rc, result_ua;

	rc = get_battery_current(chip, &result_ua);
	if (rc) {
		pr_err("failed to get current: %d\n", rc);
		return rc;
	}
	return result_ua;
}

/* Returns coulomb counter in uAh */
static int get_prop_bms_charge_counter(struct qpnp_bms_chip *chip)
{
	int64_t cc_raw;

	mutex_lock(&chip->bms_output_lock);
	lock_output_data(chip);
	read_cc_raw(chip, &cc_raw, CC);
	unlock_output_data(chip);
	mutex_unlock(&chip->bms_output_lock);

	return calculate_cc(chip, cc_raw, CC, NORESET);
}

/* Returns shadow coulomb counter in uAh */
static int get_prop_bms_charge_counter_shadow(struct qpnp_bms_chip *chip)
{
	int64_t cc_raw;

	mutex_lock(&chip->bms_output_lock);
	lock_output_data(chip);
	read_cc_raw(chip, &cc_raw, SHDW_CC);
	unlock_output_data(chip);
	mutex_unlock(&chip->bms_output_lock);

	return calculate_cc(chip, cc_raw, SHDW_CC, NORESET);
}

/* Returns full charge design in uAh */
static int get_prop_bms_charge_full_design(struct qpnp_bms_chip *chip)
{
	return chip->fcc_mah * 1000;
}

/* Returns the current full charge in uAh */
static int get_prop_bms_charge_full(struct qpnp_bms_chip *chip)
{
	int rc;
	struct qpnp_vadc_result result;

	rc = qpnp_vadc_read(chip->vadc_dev, LR_MUX1_BATT_THERM, &result);
	if (rc) {
		pr_err("Unable to read battery temperature\n");
		return rc;
	}

	return calculate_fcc(chip, (int)result.physical);
}

static int calculate_delta_time(unsigned long *time_stamp, int *delta_time_s)
{
	unsigned long now_tm_sec = 0;

	/* default to delta time = 0 if anything fails */
	*delta_time_s = 0;

	if (get_current_time(&now_tm_sec)) {
		pr_err("RTC read failed\n");
		return 0;
	}

	*delta_time_s = (now_tm_sec - *time_stamp);

	/* remember this time */
	*time_stamp = now_tm_sec;
	return 0;
}

static void calculate_soc_params(struct qpnp_bms_chip *chip,
						struct raw_soc_params *raw,
						struct soc_params *params,
						int batt_temp)
{
	int soc_rbatt, shdw_cc_uah;

	calculate_delta_time(&chip->tm_sec, &params->delta_time_s);
	pr_debug("tm_sec = %ld, delta_s = %d\n",
		chip->tm_sec, params->delta_time_s);
	params->fcc_uah = calculate_fcc(chip, batt_temp);
	pr_debug("FCC = %uuAh batt_temp = %d\n", params->fcc_uah, batt_temp);

	/* calculate remainging charge */
	params->ocv_charge_uah = calculate_ocv_charge(
						chip, raw,
						params->fcc_uah);
	pr_debug("ocv_charge_uah = %uuAh\n", params->ocv_charge_uah);

	/* calculate cc micro_volt_hour */
	params->cc_uah = calculate_cc(chip, raw->cc, CC, RESET);
	shdw_cc_uah = calculate_cc(chip, raw->shdw_cc, SHDW_CC, RESET);
	pr_debug("cc_uah = %duAh raw->cc = %llx, shdw_cc_uah = %duAh raw->shdw_cc = %llx\n",
			params->cc_uah, raw->cc,
			shdw_cc_uah, raw->shdw_cc);

	soc_rbatt = ((params->ocv_charge_uah - params->cc_uah) * 100)
							/ params->fcc_uah;
	if (soc_rbatt < 0)
		soc_rbatt = 0;
	params->rbatt_mohm = get_rbatt(chip, soc_rbatt, batt_temp);
	pr_debug("rbatt_mohm = %d\n", params->rbatt_mohm);

	if (params->rbatt_mohm != chip->rbatt_mohm) {
		chip->rbatt_mohm = params->rbatt_mohm;
		if (chip->bms_psy_registered)
			power_supply_changed(&chip->bms_psy);
	}

	calculate_iavg(chip, params->cc_uah, &params->iavg_ua,
						params->delta_time_s);

	params->uuc_uah = calculate_unusable_charge_uah(chip, params,
							batt_temp);
	pr_debug("UUC = %uuAh\n", params->uuc_uah);
}

static int bound_soc(int soc)
{
	soc = max(0, soc);
	soc = min(100, soc);
	return soc;
}

#define IBAT_TOL_MASK		0x0F
#define OCV_TOL_MASK		0xF0
#define IBAT_TOL_DEFAULT	0x03
#define IBAT_TOL_NOCHG		0x0F
#define OCV_TOL_DEFAULT		0x20
#define OCV_TOL_NO_OCV		0x00
static int stop_ocv_updates(struct qpnp_bms_chip *chip)
{
	pr_debug("stopping ocv updates\n");
	return qpnp_masked_write(chip, BMS1_TOL_CTL,
			OCV_TOL_MASK, OCV_TOL_NO_OCV);
}

static int reset_bms_for_test(struct qpnp_bms_chip *chip)
{
	int ibat_ua = 0, vbat_uv = 0, rc;
	int ocv_est_uv;

	if (!chip) {
		pr_err("BMS driver has not been initialized yet!\n");
		return -EINVAL;
	}

	rc = get_simultaneous_batt_v_and_i(chip, &ibat_ua, &vbat_uv);

	/*
	 * Don't include rbatt and rbatt_capacitative since we expect this to
	 * be used with a fake battery which does not have internal resistances
	 */
	ocv_est_uv = vbat_uv + (ibat_ua * chip->r_conn_mohm) / 1000;
	pr_debug("forcing ocv to be %d due to bms reset mode\n", ocv_est_uv);
	chip->last_ocv_uv = ocv_est_uv;
	mutex_lock(&chip->last_soc_mutex);
	chip->last_soc = -EINVAL;
	chip->last_soc_invalid = true;
	mutex_unlock(&chip->last_soc_mutex);
	reset_cc(chip, CLEAR_CC | CLEAR_SHDW_CC);
	chip->software_cc_uah = 0;
	chip->software_shdw_cc_uah = 0;
	chip->last_cc_uah = INT_MIN;
	stop_ocv_updates(chip);

	pr_debug("bms reset to ocv = %duv vbat_ua = %d ibat_ua = %d\n",
			chip->last_ocv_uv, vbat_uv, ibat_ua);

	return rc;
}

static int bms_reset_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	rc = param_set_bool(val, kp);
	if (rc) {
		pr_err("Unable to set bms_reset: %d\n", rc);
		return rc;
	}

	if (*(bool *)kp->arg) {
		struct power_supply *bms_psy = power_supply_get_by_name("bms");
		struct qpnp_bms_chip *chip = container_of(bms_psy,
					struct qpnp_bms_chip, bms_psy);

		rc = reset_bms_for_test(chip);
		if (rc) {
			pr_err("Unable to modify bms_reset: %d\n", rc);
			return rc;
		}
	}
	return 0;
}

static struct kernel_param_ops bms_reset_ops = {
	.set = bms_reset_set,
	.get = param_get_bool,
};

module_param_cb(bms_reset, &bms_reset_ops, &bms_reset, 0644);

#define SOC_STORAGE_MASK	0xFE
static void backup_soc_and_iavg(struct qpnp_bms_chip *chip, int batt_temp,
				int soc)
{
	u8 temp;
	int rc;
	int iavg_ma = chip->prev_uuc_iavg_ma;

	if (iavg_ma > MIN_IAVG_MA)
		temp = (iavg_ma - MIN_IAVG_MA) / IAVG_STEP_SIZE_MA;
	else
		temp = 0;

	rc = qpnp_write_wrapper(chip, &temp, chip->base + IAVG_STORAGE_REG, 1);

	/* store an invalid soc if temperature is below 5degC */
	if (batt_temp > IGNORE_SOC_TEMP_DECIDEG)
		qpnp_masked_write_base(chip, chip->soc_storage_addr,
				SOC_STORAGE_MASK, (soc + 1) << 1);
	else
		qpnp_masked_write_base(chip, chip->soc_storage_addr,
				SOC_STORAGE_MASK, SOC_STORAGE_MASK);
}

static int scale_soc_while_chg(struct qpnp_bms_chip *chip, int chg_time_sec,
				int catch_up_sec, int new_soc, int prev_soc)
{
	int scaled_soc;
	int numerator;

	/*
	 * Don't report a high value immediately slowly scale the
	 * value from prev_soc to the new soc based on a charge time
	 * weighted average
	 */
	pr_debug("cts = %d catch_up_sec = %d\n", chg_time_sec, catch_up_sec);
	if (catch_up_sec == 0)
		return new_soc;

	if (chg_time_sec > catch_up_sec)
		return new_soc;

	numerator = (catch_up_sec - chg_time_sec) * prev_soc
			+ chg_time_sec * new_soc;
	scaled_soc = numerator / catch_up_sec;

	pr_debug("cts = %d new_soc = %d prev_soc = %d scaled_soc = %d\n",
			chg_time_sec, new_soc, prev_soc, scaled_soc);

	return scaled_soc;
}

/*
 * bms_fake_battery is set in setups where a battery emulator is used instead
 * of a real battery. This makes the bms driver report a different/fake value
 * regardless of the calculated state of charge.
 */
static int bms_fake_battery = -EINVAL;
module_param(bms_fake_battery, int, 0644);

static int report_voltage_based_soc(struct qpnp_bms_chip *chip)
{
	pr_debug("Reported voltage based soc = %d\n",
			chip->prev_voltage_based_soc);
	return chip->prev_voltage_based_soc;
}

#define SOC_CATCHUP_SEC_MAX		600
#define SOC_CATCHUP_SEC_PER_PERCENT	60
#define MAX_CATCHUP_SOC	(SOC_CATCHUP_SEC_MAX / SOC_CATCHUP_SEC_PER_PERCENT)
#define SOC_CHANGE_PER_SEC		5
#define REPORT_SOC_WAIT_MS		10000
static int report_cc_based_soc(struct qpnp_bms_chip *chip)
{
	int soc, soc_change;
	int time_since_last_change_sec, charge_time_sec = 0;
	unsigned long last_change_sec;
	struct timespec now;
	struct qpnp_vadc_result result;
	int batt_temp;
	int rc;
	bool charging, charging_since_last_report;

	rc = wait_event_interruptible_timeout(chip->bms_wait_queue,
			chip->calculated_soc != -EINVAL,
			round_jiffies_relative(msecs_to_jiffies
			(REPORT_SOC_WAIT_MS)));

	if (rc == 0 && chip->calculated_soc == -EINVAL) {
		pr_debug("calculate soc timed out\n");
	} else if (rc == -ERESTARTSYS) {
		pr_err("Wait for SoC interrupted.\n");
		return rc;
	}

	rc = qpnp_vadc_read(chip->vadc_dev, LR_MUX1_BATT_THERM, &result);

	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					LR_MUX1_BATT_THERM, rc);
		return rc;
	}
	pr_debug("batt_temp phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	batt_temp = (int)result.physical;

	mutex_lock(&chip->last_soc_mutex);
	soc = chip->calculated_soc;

	last_change_sec = chip->last_soc_change_sec;
	calculate_delta_time(&last_change_sec, &time_since_last_change_sec);

	charging = chip->battery_status == POWER_SUPPLY_STATUS_CHARGING;
	charging_since_last_report = charging || (chip->last_soc_unbound
			&& chip->was_charging_at_sleep);
	/*
	 * account for charge time - limit it to SOC_CATCHUP_SEC to
	 * avoid overflows when charging continues for extended periods
	 */
	if (charging) {
		if (chip->charge_start_tm_sec == 0) {
			/*
			 * calculating soc for the first time
			 * after start of chg. Initialize catchup time
			 */
			if (abs(soc - chip->last_soc) < MAX_CATCHUP_SOC)
				chip->catch_up_time_sec =
				(soc - chip->last_soc)
					* SOC_CATCHUP_SEC_PER_PERCENT;
			else
				chip->catch_up_time_sec = SOC_CATCHUP_SEC_MAX;

			if (chip->catch_up_time_sec < 0)
				chip->catch_up_time_sec = 0;
			chip->charge_start_tm_sec = last_change_sec;
		}

		charge_time_sec = min(SOC_CATCHUP_SEC_MAX, (int)last_change_sec
				- chip->charge_start_tm_sec);

		/* end catchup if calculated soc and last soc are same */
		if (chip->last_soc == soc)
			chip->catch_up_time_sec = 0;
	}

	if (chip->last_soc != -EINVAL) {
		/*
		 * last_soc < soc  ... if we have not been charging at all
		 * since the last time this was called, report previous SoC.
		 * Otherwise, scale and catch up.
		 */
		if (chip->last_soc < soc && !charging_since_last_report)
			soc = chip->last_soc;
		else if (chip->last_soc < soc && soc != 100)
			soc = scale_soc_while_chg(chip, charge_time_sec,
					chip->catch_up_time_sec,
					soc, chip->last_soc);

		/* if the battery is close to cutoff allow more change */
		if (wake_lock_active(&chip->low_voltage_wake_lock))
			soc_change = min((int)abs(chip->last_soc - soc),
				time_since_last_change_sec);
		else
			soc_change = min((int)abs(chip->last_soc - soc),
				time_since_last_change_sec
					/ SOC_CHANGE_PER_SEC);

		if (chip->last_soc_unbound) {
			chip->last_soc_unbound = false;
		} else {
			/*
			 * if soc have not been unbound by resume,
			 * only change reported SoC by 1.
			 */
			soc_change = min(1, soc_change);
		}

		if (soc < chip->last_soc && soc != 0)
			soc = chip->last_soc - soc_change;
		if (soc > chip->last_soc && soc != 100)
			soc = chip->last_soc + soc_change;
	}

	if (chip->last_soc != soc && !chip->last_soc_unbound)
		chip->last_soc_change_sec = last_change_sec;

	pr_debug("last_soc = %d, calculated_soc = %d, soc = %d, time since last change = %d\n",
			chip->last_soc, chip->calculated_soc,
			soc, time_since_last_change_sec);
	chip->last_soc = bound_soc(soc);
	backup_soc_and_iavg(chip, batt_temp, chip->last_soc);
	pr_debug("Reported SOC = %d\n", chip->last_soc);
	chip->t_soc_queried = now;
	mutex_unlock(&chip->last_soc_mutex);

	return soc;
}

static int report_state_of_charge(struct qpnp_bms_chip *chip)
{
	if (bms_fake_battery != -EINVAL) {
		pr_debug("Returning Fake SOC = %d%%\n", bms_fake_battery);
		return bms_fake_battery;
	} else if (chip->use_voltage_soc)
		return report_voltage_based_soc(chip);
	else
		return report_cc_based_soc(chip);
}

#define VDD_MAX_ERR			5000
#define VDD_STEP_SIZE			10000
#define MAX_COUNT_BEFORE_RESET_TO_CC	3
static int charging_adjustments(struct qpnp_bms_chip *chip,
				struct soc_params *params, int soc,
				int vbat_uv, int ibat_ua, int batt_temp)
{
	int chg_soc, soc_ibat, batt_terminal_uv, weight_ibat, weight_cc;

	batt_terminal_uv = vbat_uv + (ibat_ua * chip->r_conn_mohm) / 1000;

	if (chip->soc_at_cv == -EINVAL) {
		if (batt_terminal_uv >= chip->max_voltage_uv - VDD_MAX_ERR) {
			chip->soc_at_cv = soc;
			chip->prev_chg_soc = soc;
			chip->ibat_at_cv_ua = params->iavg_ua;
			pr_debug("CC_TO_CV ibat_ua = %d CHG SOC %d\n",
					ibat_ua, soc);
		} else {
			/* In constant current charging return the calc soc */
			pr_debug("CC CHG SOC %d\n", soc);
		}

		chip->prev_batt_terminal_uv = batt_terminal_uv;
		chip->system_load_count = 0;
		return soc;
	} else if (ibat_ua > 0 && batt_terminal_uv
			< chip->max_voltage_uv - (VDD_MAX_ERR * 2)) {
		if (chip->system_load_count > MAX_COUNT_BEFORE_RESET_TO_CC) {
			chip->soc_at_cv = -EINVAL;
			pr_debug("Vbat below CV threshold, resetting CC_TO_CV\n");
			chip->system_load_count = 0;
		} else {
			chip->system_load_count += 1;
			pr_debug("Vbat below CV threshold, count: %d\n",
					chip->system_load_count);
		}
		return soc;
	} else if (ibat_ua > 0) {
		pr_debug("NOT CHARGING SOC %d\n", soc);
		chip->system_load_count = 0;
		chip->prev_chg_soc = soc;
		return soc;
	}

	chip->system_load_count = 0;
	/*
	 * battery is in CV phase - begin linear interpolation of soc based on
	 * battery charge current
	 */

	/*
	 * if voltage lessened (possibly because of a system load)
	 * keep reporting the prev chg soc
	 */
	if (batt_terminal_uv <= chip->prev_batt_terminal_uv - VDD_STEP_SIZE) {
		pr_debug("batt_terminal_uv %d < (max = %d - 10000); CC CHG SOC %d\n",
			batt_terminal_uv, chip->prev_batt_terminal_uv,
			chip->prev_chg_soc);
		chip->prev_batt_terminal_uv = batt_terminal_uv;
		return chip->prev_chg_soc;
	}

	soc_ibat = bound_soc(linear_interpolate(chip->soc_at_cv,
					chip->ibat_at_cv_ua,
					100, -1 * chip->chg_term_ua,
					params->iavg_ua));
	weight_ibat = bound_soc(linear_interpolate(1, chip->soc_at_cv,
					100, 100, chip->prev_chg_soc));
	weight_cc = 100 - weight_ibat;
	chg_soc = bound_soc(DIV_ROUND_CLOSEST(soc_ibat * weight_ibat
			+ weight_cc * soc, 100));

	pr_debug("weight_ibat = %d, weight_cc = %d, soc_ibat = %d, soc_cc = %d\n",
			weight_ibat, weight_cc, soc_ibat, soc);

	/* always report a higher soc */
	if (chg_soc > chip->prev_chg_soc) {
		chip->prev_chg_soc = chg_soc;

		chip->charging_adjusted_ocv = find_ocv_for_pc(chip, batt_temp,
				find_pc_for_soc(chip, params, chg_soc));
		pr_debug("CC CHG ADJ OCV = %d CHG SOC %d\n",
				chip->charging_adjusted_ocv,
				chip->prev_chg_soc);
	}

	pr_debug("Reporting CHG SOC %d\n", chip->prev_chg_soc);
	chip->prev_batt_terminal_uv = batt_terminal_uv;
	return chip->prev_chg_soc;
}

static void very_low_voltage_check(struct qpnp_bms_chip *chip, int vbat_uv)
{
	/*
	 * if battery is very low (v_cutoff voltage + 20mv) hold
	 * a wakelock untill soc = 0%
	 */
	if (vbat_uv <= chip->low_voltage_threshold
			&& !wake_lock_active(&chip->low_voltage_wake_lock)) {
		pr_debug("voltage = %d low holding wakelock\n", vbat_uv);
		wake_lock(&chip->low_voltage_wake_lock);
	} else if (vbat_uv > chip->low_voltage_threshold
			&& wake_lock_active(&chip->low_voltage_wake_lock)) {
		pr_debug("voltage = %d releasing wakelock\n", vbat_uv);
		wake_unlock(&chip->low_voltage_wake_lock);
	}
}

#define VBATT_ERROR_MARGIN	20000
static void cv_voltage_check(struct qpnp_bms_chip *chip, int vbat_uv)
{
	/*
	 * if battery is very low (v_cutoff voltage + 20mv) hold
	 * a wakelock untill soc = 0%
	 */
	if (wake_lock_active(&chip->cv_wake_lock)) {
		if (chip->soc_at_cv != -EINVAL) {
			pr_debug("hit CV, releasing cv wakelock\n");
			wake_unlock(&chip->cv_wake_lock);
		} else if (!is_battery_charging(chip)) {
			pr_debug("charging stopped, releasing cv wakelock\n");
			wake_unlock(&chip->cv_wake_lock);
		}
	} else if (vbat_uv > chip->max_voltage_uv - VBATT_ERROR_MARGIN
			&& chip->soc_at_cv == -EINVAL
			&& is_battery_charging(chip)
			&& !wake_lock_active(&chip->cv_wake_lock)) {
		pr_debug("voltage = %d holding cv wakelock\n", vbat_uv);
		wake_lock(&chip->cv_wake_lock);
	}
}

#define NO_ADJUST_HIGH_SOC_THRESHOLD	98
static int adjust_soc(struct qpnp_bms_chip *chip, struct soc_params *params,
							int soc, int batt_temp)
{
	int ibat_ua = 0, vbat_uv = 0;
	int ocv_est_uv = 0, soc_est = 0, pc_est = 0, pc = 0;
	int delta_ocv_uv = 0;
	int n = 0;
	int rc_new_uah = 0;
	int pc_new = 0;
	int soc_new = 0;
	int slope = 0;
	int rc = 0;
	int delta_ocv_uv_limit = 0;
	int correction_limit_uv = 0;

	rc = get_simultaneous_batt_v_and_i(chip, &ibat_ua, &vbat_uv);
	if (rc < 0) {
		pr_err("simultaneous vbat ibat failed err = %d\n", rc);
		goto out;
	}

	very_low_voltage_check(chip, vbat_uv);
	cv_voltage_check(chip, vbat_uv);

	delta_ocv_uv_limit = DIV_ROUND_CLOSEST(ibat_ua, 1000);

	ocv_est_uv = vbat_uv + (ibat_ua * params->rbatt_mohm)/1000;

	pc_est = calculate_pc(chip, ocv_est_uv, batt_temp);
	soc_est = div_s64((s64)params->fcc_uah * pc_est - params->uuc_uah*100,
				(s64)params->fcc_uah - params->uuc_uah);
	soc_est = bound_soc(soc_est);

	/* never adjust during bms reset mode */
	if (bms_reset) {
		pr_debug("bms reset mode, SOC adjustment skipped\n");
		goto out;
	}

	if (is_battery_charging(chip)) {
		soc = charging_adjustments(chip, params, soc, vbat_uv, ibat_ua,
				batt_temp);
		/* Skip adjustments if we are in CV or ibat is negative */
		if (chip->soc_at_cv != -EINVAL || ibat_ua < 0)
			goto out;
	}

	/*
	 * do not adjust
	 * if soc_est is same as what bms calculated
	 * OR if soc_est > adjust_soc_low_threshold
	 * OR if soc is above 90
	 * because we might pull it low
	 * and cause a bad user experience
	 */
	if (!wake_lock_active(&chip->low_voltage_wake_lock) &&
			(soc_est == soc
			|| soc_est > chip->adjust_soc_low_threshold
			|| soc >= NO_ADJUST_HIGH_SOC_THRESHOLD))
		goto out;

	if (chip->last_soc_est == -EINVAL)
		chip->last_soc_est = soc;

	n = min(200, max(1 , soc + soc_est + chip->last_soc_est));
	chip->last_soc_est = soc_est;

	pc = calculate_pc(chip, chip->last_ocv_uv, chip->last_ocv_temp);
	if (pc > 0) {
		pc_new = calculate_pc(chip,
				chip->last_ocv_uv - (++slope * 1000),
				chip->last_ocv_temp);
		while (pc_new == pc) {
			/* start taking 10mV steps */
			slope = slope + 10;
			pc_new = calculate_pc(chip,
				chip->last_ocv_uv - (slope * 1000),
				chip->last_ocv_temp);
		}
	} else {
		/*
		 * pc is already at the lowest point,
		 * assume 1 millivolt translates to 1% pc
		 */
		pc = 1;
		pc_new = 0;
		slope = 1;
	}

	delta_ocv_uv = div_s64((soc - soc_est) * (s64)slope * 1000,
							n * (pc - pc_new));

	if (abs(delta_ocv_uv) > delta_ocv_uv_limit) {
		pr_debug("limiting delta ocv %d limit = %d\n", delta_ocv_uv,
				delta_ocv_uv_limit);

		if (delta_ocv_uv > 0)
			delta_ocv_uv = delta_ocv_uv_limit;
		else
			delta_ocv_uv = -1 * delta_ocv_uv_limit;
		pr_debug("new delta ocv = %d\n", delta_ocv_uv);
	}

	if (wake_lock_active(&chip->low_voltage_wake_lock)) {
		/* when in the cutoff region, do not correct upwards */
		delta_ocv_uv = max(0, delta_ocv_uv);
		goto skip_limits;
	}

	if (chip->last_ocv_uv > chip->flat_ocv_threshold_uv)
		correction_limit_uv = chip->high_ocv_correction_limit_uv;
	else
		correction_limit_uv = chip->low_ocv_correction_limit_uv;

	if (abs(delta_ocv_uv) > correction_limit_uv) {
		pr_debug("limiting delta ocv %d limit = %d\n",
			delta_ocv_uv, correction_limit_uv);
		if (delta_ocv_uv > 0)
			delta_ocv_uv = correction_limit_uv;
		else
			delta_ocv_uv = -correction_limit_uv;
		pr_debug("new delta ocv = %d\n", delta_ocv_uv);
	}

skip_limits:

	chip->last_ocv_uv -= delta_ocv_uv;

	if (chip->last_ocv_uv >= chip->max_voltage_uv)
		chip->last_ocv_uv = chip->max_voltage_uv;

	/* calculate the soc based on this new ocv */
	pc_new = calculate_pc(chip, chip->last_ocv_uv, chip->last_ocv_temp);
	rc_new_uah = (params->fcc_uah * pc_new) / 100;
	soc_new = (rc_new_uah - params->cc_uah - params->uuc_uah)*100
					/ (params->fcc_uah - params->uuc_uah);
	soc_new = bound_soc(soc_new);

	/*
	 * if soc_new is ZERO force it higher so that phone doesnt report soc=0
	 * soc = 0 should happen only when soc_est is above a set value
	 */
	if (soc_new == 0 && soc_est >= chip->hold_soc_est)
		soc_new = 1;

	soc = soc_new;

out:
	pr_debug("ibat_ua = %d, vbat_uv = %d, ocv_est_uv = %d, pc_est = %d, soc_est = %d, n = %d, delta_ocv_uv = %d, last_ocv_uv = %d, pc_new = %d, soc_new = %d, rbatt = %d, slope = %d\n",
		ibat_ua, vbat_uv, ocv_est_uv, pc_est,
		soc_est, n, delta_ocv_uv, chip->last_ocv_uv,
		pc_new, soc_new, params->rbatt_mohm, slope);

	return soc;
}

static int clamp_soc_based_on_voltage(struct qpnp_bms_chip *chip, int soc)
{
	int rc, vbat_uv;

	rc = get_battery_voltage(chip, &vbat_uv);
	if (rc < 0) {
		pr_err("adc vbat failed err = %d\n", rc);
		return soc;
	}
	if (soc == 0 && vbat_uv > chip->v_cutoff_uv) {
		pr_debug("clamping soc to 1, vbat (%d) > cutoff (%d)\n",
						vbat_uv, chip->v_cutoff_uv);
		return 1;
	} else {
		pr_debug("not clamping, using soc = %d, vbat = %d and cutoff = %d\n",
				soc, vbat_uv, chip->v_cutoff_uv);
		return soc;
	}
}

static int64_t convert_cc_uah_to_raw(struct qpnp_bms_chip *chip, int64_t cc_uah)
{
	int64_t cc_uv, cc_pvh, cc_raw;

	cc_pvh = cc_uah * chip->r_sense_uohm;
	cc_uv = div_s64(cc_pvh * SLEEP_CLK_HZ * SECONDS_PER_HOUR,
				CC_READING_TICKS * 1000000LL);
	cc_raw = div_s64(cc_uv * CC_READING_RESOLUTION_D,
			CC_READING_RESOLUTION_N);
	return cc_raw;
}

#define CC_STEP_INCREMENT_UAH	1500
#define OCV_STEP_INCREMENT	0x10
static void configure_soc_wakeup(struct qpnp_bms_chip *chip,
				struct soc_params *params,
				int batt_temp, int target_soc)
{
	int target_ocv_uv;
	int64_t target_cc_uah, cc_raw_64, current_shdw_cc_raw_64;
	int64_t current_shdw_cc_uah, iadc_comp_factor;
	uint64_t cc_raw, current_shdw_cc_raw;
	int16_t ocv_raw, current_ocv_raw;

	current_shdw_cc_raw = 0;
	mutex_lock(&chip->bms_output_lock);
	lock_output_data(chip);
	qpnp_read_wrapper(chip, (u8 *)&current_ocv_raw,
			chip->base + BMS1_OCV_FOR_SOC_DATA0, 2);
	unlock_output_data(chip);
	mutex_unlock(&chip->bms_output_lock);
	current_shdw_cc_uah = get_prop_bms_charge_counter_shadow(chip);
	current_shdw_cc_raw_64 = convert_cc_uah_to_raw(chip,
			current_shdw_cc_uah);

	/*
	 * Calculate the target shadow coulomb counter threshold for when
	 * the SoC changes.
	 *
	 * Since the BMS driver resets the shadow coulomb counter every
	 * 20 seconds when the device is awake, calculate the threshold as
	 * a delta from the current shadow coulomb count.
	 */
	target_cc_uah = (100 - target_soc)
		* (params->fcc_uah - params->uuc_uah)
		/ 100 - current_shdw_cc_uah;
	if (target_cc_uah < 0) {
		/*
		 * If the target cc is below 0, that means we have already
		 * passed the point where SoC should have fallen.
		 * Set a wakeup in a few more mAh and check back again
		 */
		target_cc_uah = CC_STEP_INCREMENT_UAH;
	}
	iadc_comp_factor = 100000;
	qpnp_iadc_comp_result(chip->iadc_dev, &iadc_comp_factor);
	target_cc_uah = div64_s64(target_cc_uah * 100000, iadc_comp_factor);
	target_cc_uah = cc_reverse_adjust_for_gain(chip, target_cc_uah);
	cc_raw_64 = convert_cc_uah_to_raw(chip, target_cc_uah);
	cc_raw = convert_s64_to_s36(cc_raw_64);

	target_ocv_uv = find_ocv_for_pc(chip, batt_temp,
				find_pc_for_soc(chip, params, target_soc));
	ocv_raw = convert_vbatt_uv_to_raw(chip, target_ocv_uv);

	/*
	 * If the current_ocv_raw was updated since reaching 100% and is lower
	 * than the calculated target ocv threshold, set the new target
	 * threshold 1.5mAh lower in order to check if the SoC changed yet.
	 */
	if (current_ocv_raw != chip->ocv_reading_at_100
			&& current_ocv_raw < ocv_raw)
		ocv_raw = current_ocv_raw - OCV_STEP_INCREMENT;

	qpnp_write_wrapper(chip, (u8 *)&cc_raw,
			chip->base + BMS1_SW_CC_THR0, 5);
	qpnp_write_wrapper(chip, (u8 *)&ocv_raw,
			chip->base + BMS1_OCV_THR0, 2);

	enable_bms_irq(&chip->ocv_thr_irq);
	enable_bms_irq(&chip->sw_cc_thr_irq);
	pr_debug("current sw_cc_raw = 0x%llx, current ocv = 0x%hx\n",
			current_shdw_cc_raw, (uint16_t)current_ocv_raw);
	pr_debug("target_cc_uah = %lld, raw64 = 0x%llx, raw 36 = 0x%llx, ocv_raw = 0x%hx\n",
			target_cc_uah,
			(uint64_t)cc_raw_64, cc_raw,
			(uint16_t)ocv_raw);
}

#define BAD_SOC_THRESH	-10
static int calculate_raw_soc(struct qpnp_bms_chip *chip,
					struct raw_soc_params *raw,
					struct soc_params *params,
					int batt_temp)
{
	int soc, remaining_usable_charge_uah;

	/* calculate remaining usable charge */
	remaining_usable_charge_uah = params->ocv_charge_uah
					- params->cc_uah
					- params->uuc_uah;
	pr_debug("RUC = %duAh\n", remaining_usable_charge_uah);

	soc = DIV_ROUND_CLOSEST((remaining_usable_charge_uah * 100),
				(params->fcc_uah - params->uuc_uah));

	if (chip->first_time_calc_soc && soc > BAD_SOC_THRESH && soc < 0) {
		/*
		 * first time calcualtion and the pon ocv  is too low resulting
		 * in a bad soc. Adjust ocv to get 0 soc
		 */
		pr_debug("soc is %d, adjusting pon ocv to make it 0\n", soc);
		chip->last_ocv_uv = find_ocv_for_pc(chip, batt_temp,
				find_pc_for_soc(chip, params, 0));
		params->ocv_charge_uah = find_ocv_charge_for_soc(chip,
				params, 0);

		remaining_usable_charge_uah = params->ocv_charge_uah
					- params->cc_uah
					- params->uuc_uah;

		soc = DIV_ROUND_CLOSEST((remaining_usable_charge_uah * 100),
					(params->fcc_uah
						- params->uuc_uah));
		pr_debug("DONE for O soc is %d, pon ocv adjusted to %duV\n",
				soc, chip->last_ocv_uv);
	}

	if (soc > 100)
		soc = 100;

	if (soc > BAD_SOC_THRESH && soc < 0) {
		pr_debug("bad rem_usb_chg = %d rem_chg %d, cc_uah %d, unusb_chg %d\n",
				remaining_usable_charge_uah,
				params->ocv_charge_uah,
				params->cc_uah, params->uuc_uah);

		pr_debug("for bad rem_usb_chg last_ocv_uv = %d batt_temp = %d fcc = %d soc =%d\n",
				chip->last_ocv_uv, batt_temp,
				params->fcc_uah, soc);
		soc = 0;
	}

	return soc;
}

#define SLEEP_RECALC_INTERVAL	3
static int calculate_state_of_charge(struct qpnp_bms_chip *chip,
					struct raw_soc_params *raw,
					int batt_temp)
{
	struct soc_params params;
	int soc, previous_soc, shutdown_soc, new_calculated_soc;
	int remaining_usable_charge_uah;

	calculate_soc_params(chip, raw, &params, batt_temp);
	if (!is_battery_present(chip)) {
		pr_debug("battery gone, reporting 100\n");
		new_calculated_soc = 100;
		goto done_calculating;
	}

	if (params.fcc_uah - params.uuc_uah <= 0) {
		pr_debug("FCC = %duAh, UUC = %duAh forcing soc = 0\n",
						params.fcc_uah,
						params.uuc_uah);
		new_calculated_soc = 0;
		goto done_calculating;
	}

	soc = calculate_raw_soc(chip, raw, &params, batt_temp);

	mutex_lock(&chip->soc_invalidation_mutex);
	shutdown_soc = chip->shutdown_soc;

	if (chip->first_time_calc_soc && soc != shutdown_soc
			&& !chip->shutdown_soc_invalid) {
		/*
		 * soc for the first time - use shutdown soc
		 * to adjust pon ocv since it is a small percent away from
		 * the real soc
		 */
		pr_debug("soc = %d before forcing shutdown_soc = %d\n",
							soc, shutdown_soc);
		chip->last_ocv_uv = find_ocv_for_pc(chip, batt_temp,
				find_pc_for_soc(chip, &params, shutdown_soc));
		params.ocv_charge_uah = find_ocv_charge_for_soc(chip,
				&params, shutdown_soc);

		remaining_usable_charge_uah = params.ocv_charge_uah
					- params.cc_uah
					- params.uuc_uah;

		soc = DIV_ROUND_CLOSEST((remaining_usable_charge_uah * 100),
					(params.fcc_uah
						- params.uuc_uah));

		pr_debug("DONE for shutdown_soc = %d soc is %d, adjusted ocv to %duV\n",
				shutdown_soc, soc, chip->last_ocv_uv);
	}
	mutex_unlock(&chip->soc_invalidation_mutex);

	if (chip->first_time_calc_soc && !chip->shutdown_soc_invalid) {
		pr_debug("Skip adjustment when shutdown SOC has been forced\n");
		new_calculated_soc = soc;
	} else {
		pr_debug("SOC before adjustment = %d\n", soc);
		new_calculated_soc = adjust_soc(chip, &params, soc, batt_temp);
	}

	/* always clamp soc due to BMS hw/sw immaturities */
	new_calculated_soc = clamp_soc_based_on_voltage(chip,
					new_calculated_soc);
	/*
	 * If the battery is full, configure the cc threshold so the system
	 * wakes up after SoC changes
	 */
	if (is_battery_full(chip)) {
		configure_soc_wakeup(chip, &params,
				batt_temp, bound_soc(new_calculated_soc - 1));
	} else {
		disable_bms_irq(&chip->ocv_thr_irq);
		disable_bms_irq(&chip->sw_cc_thr_irq);
	}
done_calculating:
	mutex_lock(&chip->last_soc_mutex);
	previous_soc = chip->calculated_soc;
	chip->calculated_soc = new_calculated_soc;
	pr_debug("CC based calculated SOC = %d\n", chip->calculated_soc);
	if (chip->last_soc_invalid) {
		chip->last_soc_invalid = false;
		chip->last_soc = -EINVAL;
	}
	/*
	 * Check if more than a long time has passed since the last
	 * calculation (more than n times compared to the soc recalculation
	 * rate, where n is defined by SLEEP_RECALC_INTERVAL). If this is true,
	 * then the system must have gone through a long sleep, and SoC can be
	 * allowed to become unbounded by the last reported SoC
	 */
	if (params.delta_time_s * 1000 >
			chip->calculate_soc_ms * SLEEP_RECALC_INTERVAL
			&& !chip->first_time_calc_soc) {
		chip->last_soc_unbound = true;
		chip->last_soc_change_sec = chip->last_recalc_time;
		pr_debug("last_soc unbound because elapsed time = %d\n",
				params.delta_time_s);
	}
	mutex_unlock(&chip->last_soc_mutex);
	wake_up_interruptible(&chip->bms_wait_queue);

	if (new_calculated_soc != previous_soc && chip->bms_psy_registered) {
		power_supply_changed(&chip->bms_psy);
		pr_debug("power supply changed\n");
	} else {
		/*
		 * Call report state of charge anyways to periodically update
		 * reported SoC. This prevents reported SoC from being stuck
		 * when calculated soc doesn't change.
		 */
		report_state_of_charge(chip);
	}

	get_current_time(&chip->last_recalc_time);
	chip->first_time_calc_soc = 0;
	chip->first_time_calc_uuc = 0;
	return chip->calculated_soc;
}

static int calculate_soc_from_voltage(struct qpnp_bms_chip *chip)
{
	int voltage_range_uv, voltage_remaining_uv, voltage_based_soc;
	int rc, vbat_uv;

	rc = get_battery_voltage(chip, &vbat_uv);
	if (rc < 0) {
		pr_err("adc vbat failed err = %d\n", rc);
		return rc;
	}
	voltage_range_uv = chip->max_voltage_uv - chip->v_cutoff_uv;
	voltage_remaining_uv = vbat_uv - chip->v_cutoff_uv;
	voltage_based_soc = voltage_remaining_uv * 100 / voltage_range_uv;

	voltage_based_soc = clamp(voltage_based_soc, 0, 100);

	if (chip->prev_voltage_based_soc != voltage_based_soc
				&& chip->bms_psy_registered) {
		power_supply_changed(&chip->bms_psy);
		pr_debug("power supply changed\n");
	}
	chip->prev_voltage_based_soc = voltage_based_soc;

	pr_debug("vbat used = %duv\n", vbat_uv);
	pr_debug("Calculated voltage based soc = %d\n", voltage_based_soc);
	return voltage_based_soc;
}

static int recalculate_raw_soc(struct qpnp_bms_chip *chip)
{
	int batt_temp, rc, soc;
	struct qpnp_vadc_result result;
	struct raw_soc_params raw;
	struct soc_params params;

	bms_stay_awake(&chip->soc_wake_source);
	if (chip->use_voltage_soc) {
		soc = calculate_soc_from_voltage(chip);
	} else {
		if (!chip->batfet_closed)
			qpnp_iadc_calibrate_for_trim(chip->iadc_dev, false);
		rc = qpnp_vadc_read(chip->vadc_dev, LR_MUX1_BATT_THERM,
								&result);
		if (rc) {
			pr_err("error reading vadc LR_MUX1_BATT_THERM = %d, rc = %d\n",
						LR_MUX1_BATT_THERM, rc);
			soc = chip->calculated_soc;
		} else {
			pr_debug("batt_temp phy = %lld meas = 0x%llx\n",
							result.physical,
							result.measurement);
			batt_temp = (int)result.physical;

			mutex_lock(&chip->last_ocv_uv_mutex);
			read_soc_params_raw(chip, &raw, batt_temp);
			calculate_soc_params(chip, &raw, &params, batt_temp);
			if (!is_battery_present(chip)) {
				pr_debug("battery gone\n");
				soc = 0;
			} else if (params.fcc_uah - params.uuc_uah <= 0) {
				pr_debug("FCC = %duAh, UUC = %duAh forcing soc = 0\n",
							params.fcc_uah,
							params.uuc_uah);
				soc = 0;
			} else {
				soc = calculate_raw_soc(chip, &raw,
							&params, batt_temp);
			}
			mutex_unlock(&chip->last_ocv_uv_mutex);
		}
	}
	bms_relax(&chip->soc_wake_source);
	return soc;
}

static int recalculate_soc(struct qpnp_bms_chip *chip)
{
	int batt_temp, rc, soc;
	struct qpnp_vadc_result result;
	struct raw_soc_params raw;

	bms_stay_awake(&chip->soc_wake_source);
	mutex_lock(&chip->vbat_monitor_mutex);
	if (chip->vbat_monitor_params.state_request !=
			ADC_TM_HIGH_LOW_THR_DISABLE)
		qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
					&chip->vbat_monitor_params);
	mutex_unlock(&chip->vbat_monitor_mutex);
	if (chip->use_voltage_soc) {
		soc = calculate_soc_from_voltage(chip);
	} else {
		if (!chip->batfet_closed)
			qpnp_iadc_calibrate_for_trim(chip->iadc_dev, false);
		rc = qpnp_vadc_read(chip->vadc_dev, LR_MUX1_BATT_THERM,
								&result);
		if (rc) {
			pr_err("error reading vadc LR_MUX1_BATT_THERM = %d, rc = %d\n",
						LR_MUX1_BATT_THERM, rc);
			soc = chip->calculated_soc;
		} else {
			pr_debug("batt_temp phy = %lld meas = 0x%llx\n",
							result.physical,
							result.measurement);
			batt_temp = (int)result.physical;

			mutex_lock(&chip->last_ocv_uv_mutex);
			read_soc_params_raw(chip, &raw, batt_temp);
			soc = calculate_state_of_charge(chip, &raw, batt_temp);
			mutex_unlock(&chip->last_ocv_uv_mutex);
		}
	}
	bms_relax(&chip->soc_wake_source);
	return soc;
}

static void recalculate_work(struct work_struct *work)
{
	struct qpnp_bms_chip *chip = container_of(work,
				struct qpnp_bms_chip,
				recalc_work);

	recalculate_soc(chip);
}

static int get_calculation_delay_ms(struct qpnp_bms_chip *chip)
{
	if (wake_lock_active(&chip->low_voltage_wake_lock))
		return chip->low_voltage_calculate_soc_ms;
	else if (chip->calculated_soc < chip->low_soc_calc_threshold)
		return chip->low_soc_calculate_soc_ms;
	else
		return chip->calculate_soc_ms;
}

static void calculate_soc_work(struct work_struct *work)
{
	struct qpnp_bms_chip *chip = container_of(work,
				struct qpnp_bms_chip,
				calculate_soc_delayed_work.work);

	recalculate_soc(chip);
	schedule_delayed_work(&chip->calculate_soc_delayed_work,
		round_jiffies_relative(msecs_to_jiffies
		(get_calculation_delay_ms(chip))));
}

static void configure_vbat_monitor_low(struct qpnp_bms_chip *chip)
{
	mutex_lock(&chip->vbat_monitor_mutex);
	if (chip->vbat_monitor_params.state_request
			== ADC_TM_HIGH_LOW_THR_ENABLE) {
		/*
		 * Battery is now around or below v_cutoff
		 */
		pr_debug("battery entered cutoff range\n");
		if (!wake_lock_active(&chip->low_voltage_wake_lock)) {
			pr_debug("voltage low, holding wakelock\n");
			wake_lock(&chip->low_voltage_wake_lock);
			cancel_delayed_work_sync(
					&chip->calculate_soc_delayed_work);
			schedule_delayed_work(
					&chip->calculate_soc_delayed_work, 0);
		}
		chip->vbat_monitor_params.state_request =
					ADC_TM_HIGH_THR_ENABLE;
		chip->vbat_monitor_params.high_thr =
			(chip->low_voltage_threshold + VBATT_ERROR_MARGIN);
		pr_debug("set low thr to %d and high to %d\n",
				chip->vbat_monitor_params.low_thr,
				chip->vbat_monitor_params.high_thr);
		chip->vbat_monitor_params.low_thr = 0;
	} else if (chip->vbat_monitor_params.state_request
			== ADC_TM_LOW_THR_ENABLE) {
		/*
		 * Battery is in normal operation range.
		 */
		pr_debug("battery entered normal range\n");
		if (wake_lock_active(&chip->cv_wake_lock)) {
			wake_unlock(&chip->cv_wake_lock);
			pr_debug("releasing cv wake lock\n");
		}
		chip->in_cv_range = false;
		chip->vbat_monitor_params.state_request =
					ADC_TM_HIGH_LOW_THR_ENABLE;
		chip->vbat_monitor_params.high_thr = chip->max_voltage_uv
				- VBATT_ERROR_MARGIN;
		chip->vbat_monitor_params.low_thr =
				chip->low_voltage_threshold;
		pr_debug("set low thr to %d and high to %d\n",
				chip->vbat_monitor_params.low_thr,
				chip->vbat_monitor_params.high_thr);
	}
	qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
					&chip->vbat_monitor_params);
	mutex_unlock(&chip->vbat_monitor_mutex);
}

#define CV_LOW_THRESHOLD_HYST_UV 100000
static void configure_vbat_monitor_high(struct qpnp_bms_chip *chip)
{
	mutex_lock(&chip->vbat_monitor_mutex);
	if (chip->vbat_monitor_params.state_request
			== ADC_TM_HIGH_LOW_THR_ENABLE) {
		/*
		 * Battery is around vddmax
		 */
		pr_debug("battery entered vddmax range\n");
		chip->in_cv_range = true;
		if (!wake_lock_active(&chip->cv_wake_lock)) {
			wake_lock(&chip->cv_wake_lock);
			pr_debug("holding cv wake lock\n");
		}
		schedule_work(&chip->recalc_work);
		chip->vbat_monitor_params.state_request =
					ADC_TM_LOW_THR_ENABLE;
		chip->vbat_monitor_params.low_thr =
			(chip->max_voltage_uv - CV_LOW_THRESHOLD_HYST_UV);
		chip->vbat_monitor_params.high_thr = chip->max_voltage_uv * 2;
		pr_debug("set low thr to %d and high to %d\n",
				chip->vbat_monitor_params.low_thr,
				chip->vbat_monitor_params.high_thr);
	} else if (chip->vbat_monitor_params.state_request
			== ADC_TM_HIGH_THR_ENABLE) {
		/*
		 * Battery is in normal operation range.
		 */
		pr_debug("battery entered normal range\n");
		if (wake_lock_active(&chip->low_voltage_wake_lock)) {
			pr_debug("voltage high, releasing wakelock\n");
			wake_unlock(&chip->low_voltage_wake_lock);
		}
		chip->vbat_monitor_params.state_request =
					ADC_TM_HIGH_LOW_THR_ENABLE;
		chip->vbat_monitor_params.high_thr =
			chip->max_voltage_uv - VBATT_ERROR_MARGIN;
		chip->vbat_monitor_params.low_thr =
				chip->low_voltage_threshold;
		pr_debug("set low thr to %d and high to %d\n",
				chip->vbat_monitor_params.low_thr,
				chip->vbat_monitor_params.high_thr);
	}
	qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
					&chip->vbat_monitor_params);
	mutex_unlock(&chip->vbat_monitor_mutex);
}

static void btm_notify_vbat(enum qpnp_tm_state state, void *ctx)
{
	struct qpnp_bms_chip *chip = ctx;
	int vbat_uv;
	struct qpnp_vadc_result result;
	int rc;

	rc = qpnp_vadc_read(chip->vadc_dev, VBAT_SNS, &result);
	pr_debug("vbat = %lld, raw = 0x%x\n", result.physical, result.adc_code);

	get_battery_voltage(chip, &vbat_uv);
	pr_debug("vbat is at %d, state is at %d\n", vbat_uv, state);

	if (state == ADC_TM_LOW_STATE) {
		pr_debug("low voltage btm notification triggered\n");
		if (vbat_uv - VBATT_ERROR_MARGIN
				< chip->vbat_monitor_params.low_thr) {
			configure_vbat_monitor_low(chip);
		} else {
			pr_debug("faulty btm trigger, discarding\n");
			qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
					&chip->vbat_monitor_params);
		}
	} else if (state == ADC_TM_HIGH_STATE) {
		pr_debug("high voltage btm notification triggered\n");
		if (vbat_uv + VBATT_ERROR_MARGIN
				> chip->vbat_monitor_params.high_thr) {
			configure_vbat_monitor_high(chip);
		} else {
			pr_debug("faulty btm trigger, discarding\n");
			qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
					&chip->vbat_monitor_params);
		}
	} else {
		pr_debug("unknown voltage notification state: %d\n", state);
	}
	if (chip->bms_psy_registered)
		power_supply_changed(&chip->bms_psy);
}

static int reset_vbat_monitoring(struct qpnp_bms_chip *chip)
{
	int rc;

	chip->vbat_monitor_params.state_request = ADC_TM_HIGH_LOW_THR_DISABLE;

	rc = qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
						&chip->vbat_monitor_params);
	if (rc) {
		pr_err("tm disable failed: %d\n", rc);
		return rc;
	}
	if (wake_lock_active(&chip->low_voltage_wake_lock)) {
		pr_debug("battery removed, releasing wakelock\n");
		wake_unlock(&chip->low_voltage_wake_lock);
	}
	if (chip->in_cv_range) {
		pr_debug("battery removed, removing in_cv_range state\n");
		chip->in_cv_range = false;
	}
	return 0;
}

static int setup_vbat_monitoring(struct qpnp_bms_chip *chip)
{
	int rc;

	chip->vbat_monitor_params.low_thr = chip->low_voltage_threshold;
	chip->vbat_monitor_params.high_thr = chip->max_voltage_uv
							- VBATT_ERROR_MARGIN;
	chip->vbat_monitor_params.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
	chip->vbat_monitor_params.channel = VBAT_SNS;
	chip->vbat_monitor_params.btm_ctx = (void *)chip;
	chip->vbat_monitor_params.timer_interval = ADC_MEAS1_INTERVAL_1S;
	chip->vbat_monitor_params.threshold_notification = &btm_notify_vbat;
	pr_debug("set low thr to %d and high to %d\n",
			chip->vbat_monitor_params.low_thr,
			chip->vbat_monitor_params.high_thr);

	if (!is_battery_present(chip)) {
		pr_debug("no battery inserted, do not enable vbat monitoring\n");
		chip->vbat_monitor_params.state_request =
			ADC_TM_HIGH_LOW_THR_DISABLE;
	} else {
		rc = qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
						&chip->vbat_monitor_params);
		if (rc) {
			pr_err("tm setup failed: %d\n", rc);
		return rc;
		}
	}

	pr_debug("setup complete\n");
	return 0;
}

static void readjust_fcc_table(struct qpnp_bms_chip *chip)
{
	struct single_row_lut *temp, *old;
	int i, fcc, ratio;

	if (!chip->enable_fcc_learning)
		return;

	if (!chip->fcc_temp_lut) {
		pr_err("The static fcc lut table is NULL\n");
		return;
	}

	temp = devm_kzalloc(chip->dev, sizeof(struct single_row_lut),
			GFP_KERNEL);
	if (!temp) {
		pr_err("Cannot allocate memory for adjusted fcc table\n");
		return;
	}

	fcc = interpolate_fcc(chip->fcc_temp_lut, chip->fcc_new_batt_temp);

	temp->cols = chip->fcc_temp_lut->cols;
	for (i = 0; i < chip->fcc_temp_lut->cols; i++) {
		temp->x[i] = chip->fcc_temp_lut->x[i];
		ratio = div_u64(chip->fcc_temp_lut->y[i] * 1000, fcc);
		temp->y[i] =  (ratio * chip->fcc_new_mah);
		temp->y[i] /= 1000;
	}

	old = chip->adjusted_fcc_temp_lut;
	chip->adjusted_fcc_temp_lut = temp;
	devm_kfree(chip->dev, old);
}

static int read_fcc_data_from_backup(struct qpnp_bms_chip *chip)
{
	int rc, i;
	u8 fcc = 0, chgcyl = 0;

	for (i = 0; i < chip->min_fcc_learning_samples; i++) {
		rc = qpnp_read_wrapper(chip, &fcc,
			chip->base + BMS_FCC_BASE_REG + i, 1);
		rc |= qpnp_read_wrapper(chip, &chgcyl,
			chip->base + BMS_CHGCYL_BASE_REG + i, 1);
		if (rc) {
			pr_err("Unable to read FCC data\n");
			return rc;
		}
		if (fcc == 0 || (fcc == 0xFF && chgcyl == 0xFF)) {
			/* FCC invalid/not present */
			chip->fcc_learning_samples[i].fcc_new = 0;
			chip->fcc_learning_samples[i].chargecycles = 0;
		} else {
			/* valid FCC data */
			chip->fcc_sample_count++;
			chip->fcc_learning_samples[i].fcc_new =
						fcc * chip->fcc_resolution;
			chip->fcc_learning_samples[i].chargecycles =
						chgcyl * CHGCYL_RESOLUTION;
		}
	}

	return 0;
}

static int discard_backup_fcc_data(struct qpnp_bms_chip *chip)
{
	int rc = 0, i;
	u8 temp_u8 = 0;

	chip->fcc_sample_count = 0;
	for (i = 0; i < chip->min_fcc_learning_samples; i++) {
		rc = qpnp_write_wrapper(chip, &temp_u8,
			chip->base + BMS_FCC_BASE_REG + i, 1);
		rc |= qpnp_write_wrapper(chip, &temp_u8,
			chip->base + BMS_CHGCYL_BASE_REG + i, 1);
		if (rc) {
			pr_err("Unable to clear FCC data\n");
			return rc;
		}
	}

	return 0;
}

static void
average_fcc_samples_and_readjust_fcc_table(struct qpnp_bms_chip *chip)
{
	int i, temp_fcc_avg = 0, temp_fcc_delta = 0, new_fcc_avg = 0;
	struct fcc_sample *ft;

	for (i = 0; i < chip->min_fcc_learning_samples; i++)
		temp_fcc_avg += chip->fcc_learning_samples[i].fcc_new;

	temp_fcc_avg /= chip->min_fcc_learning_samples;
	temp_fcc_delta = div_u64(temp_fcc_avg * DELTA_FCC_PERCENT, 100);

	/* fix the fcc if its an outlier i.e. > 5% of the average */
	for (i = 0; i < chip->min_fcc_learning_samples; i++) {
		ft = &chip->fcc_learning_samples[i];
		if (abs(ft->fcc_new - temp_fcc_avg) > temp_fcc_delta)
			new_fcc_avg += temp_fcc_avg;
		else
			new_fcc_avg += ft->fcc_new;
	}
	new_fcc_avg /= chip->min_fcc_learning_samples;

	chip->fcc_new_mah = new_fcc_avg;
	chip->fcc_new_batt_temp = FCC_DEFAULT_TEMP;
	pr_info("FCC update: New fcc_mah=%d, fcc_batt_temp=%d\n",
				new_fcc_avg, FCC_DEFAULT_TEMP);
	readjust_fcc_table(chip);
}

static void backup_charge_cycle(struct qpnp_bms_chip *chip)
{
	int rc = 0;

	if (chip->charge_increase >= 0) {
		rc = qpnp_write_wrapper(chip, &chip->charge_increase,
			chip->base + CHARGE_INCREASE_STORAGE, 1);
		if (rc)
			pr_err("Unable to backup charge_increase\n");
	}

	if (chip->charge_cycles >= 0) {
		rc = qpnp_write_wrapper(chip, (u8 *)&chip->charge_cycles,
				chip->base + CHARGE_CYCLE_STORAGE_LSB, 2);
		if (rc)
			pr_err("Unable to backup charge_cycles\n");
	}
}

static bool chargecycles_in_range(struct qpnp_bms_chip *chip)
{
	int i, min_cycle, max_cycle, valid_range;

	/* find the smallest and largest charge cycle */
	max_cycle = min_cycle = chip->fcc_learning_samples[0].chargecycles;
	for (i = 1; i < chip->min_fcc_learning_samples; i++) {
		if (min_cycle > chip->fcc_learning_samples[i].chargecycles)
			min_cycle = chip->fcc_learning_samples[i].chargecycles;
		if (max_cycle < chip->fcc_learning_samples[i].chargecycles)
			max_cycle = chip->fcc_learning_samples[i].chargecycles;
	}

	/* check if chargecyles are in range to continue with FCC update */
	valid_range = DIV_ROUND_UP(VALID_FCC_CHGCYL_RANGE,
					CHGCYL_RESOLUTION) * CHGCYL_RESOLUTION;
	if (abs(max_cycle - min_cycle) > valid_range)
		return false;

	return true;
}

static int read_chgcycle_data_from_backup(struct qpnp_bms_chip *chip)
{
	int rc;
	uint16_t temp_u16 = 0;
	u8 temp_u8 = 0;

	rc = qpnp_read_wrapper(chip, &temp_u8,
				chip->base + CHARGE_INCREASE_STORAGE, 1);
	if (!rc && temp_u8 != 0xFF)
		chip->charge_increase = temp_u8;

	rc = qpnp_read_wrapper(chip, (u8 *)&temp_u16,
				chip->base + CHARGE_CYCLE_STORAGE_LSB, 2);
	if (!rc && temp_u16 != 0xFFFF)
		chip->charge_cycles = temp_u16;

	return rc;
}

static void
attempt_learning_new_fcc(struct qpnp_bms_chip *chip)
{
	pr_debug("Total FCC sample count=%d\n", chip->fcc_sample_count);

	/* update FCC if we have the required samples */
	if ((chip->fcc_sample_count == chip->min_fcc_learning_samples) &&
						chargecycles_in_range(chip))
		average_fcc_samples_and_readjust_fcc_table(chip);
}

static int calculate_real_soc(struct qpnp_bms_chip *chip,
		int batt_temp, struct raw_soc_params *raw, int cc_uah)
{
	int fcc_uah, rc_uah;

	fcc_uah = calculate_fcc(chip, batt_temp);
	rc_uah = calculate_ocv_charge(chip, raw, fcc_uah);

	return ((rc_uah - cc_uah) * 100) / fcc_uah;
}

#define MAX_U8_VALUE		((u8)(~0U))

static int backup_new_fcc(struct qpnp_bms_chip *chip, int fcc_mah,
							int chargecycles)
{
	int rc, min_cycle, i;
	u8 fcc_new, chgcyl, pos = 0;
	struct fcc_sample *ft;

	if ((fcc_mah > (chip->fcc_resolution * MAX_U8_VALUE)) ||
		(chargecycles > (CHGCYL_RESOLUTION * MAX_U8_VALUE))) {
		pr_warn("FCC/Chgcyl beyond storage limit. FCC=%d, chgcyl=%d\n",
							fcc_mah, chargecycles);
		return -EINVAL;
	}

	if (chip->fcc_sample_count == chip->min_fcc_learning_samples) {
		/* search best location - oldest entry */
		min_cycle = chip->fcc_learning_samples[0].chargecycles;
		for (i = 1; i < chip->min_fcc_learning_samples; i++) {
			if (min_cycle >
				chip->fcc_learning_samples[i].chargecycles)
				pos = i;
		}
	} else {
		/* find an empty location */
		for (i = 0; i < chip->min_fcc_learning_samples; i++) {
			ft = &chip->fcc_learning_samples[i];
			if (ft->fcc_new == 0 || (ft->fcc_new == 0xFF &&
						ft->chargecycles == 0xFF)) {
				pos = i;
				break;
			}
		}
		chip->fcc_sample_count++;
	}
	chip->fcc_learning_samples[pos].fcc_new = fcc_mah;
	chip->fcc_learning_samples[pos].chargecycles = chargecycles;

	fcc_new = DIV_ROUND_UP(fcc_mah, chip->fcc_resolution);
	rc = qpnp_write_wrapper(chip, (u8 *)&fcc_new,
			chip->base + BMS_FCC_BASE_REG + pos, 1);
	if (rc)
		return rc;

	chgcyl = DIV_ROUND_UP(chargecycles, CHGCYL_RESOLUTION);
	rc = qpnp_write_wrapper(chip, (u8 *)&chgcyl,
			chip->base + BMS_CHGCYL_BASE_REG + pos, 1);
	if (rc)
		return rc;

	pr_debug("Backup new FCC: fcc_new=%d, chargecycle=%d, pos=%d\n",
						fcc_new, chgcyl, pos);

	return rc;
}

static void update_fcc_learning_table(struct qpnp_bms_chip *chip,
			int new_fcc_uah, int chargecycles, int batt_temp)
{
	int rc, fcc_default, fcc_temp;

	/* convert the fcc at batt_temp to new fcc at FCC_DEFAULT_TEMP */
	fcc_default = calculate_fcc(chip, FCC_DEFAULT_TEMP) / 1000;
	fcc_temp = calculate_fcc(chip, batt_temp) / 1000;
	new_fcc_uah = (new_fcc_uah / fcc_temp) * fcc_default;

	rc = backup_new_fcc(chip, new_fcc_uah / 1000, chargecycles);
	if (rc) {
		pr_err("Unable to backup new FCC\n");
		return;
	}
	/* check if FCC can be updated */
	attempt_learning_new_fcc(chip);
}

static bool is_new_fcc_valid(int new_fcc_uah, int fcc_uah)
{
	if ((new_fcc_uah >= (fcc_uah / 2)) &&
		((new_fcc_uah * 100) <= (fcc_uah * 105)))
		return true;

	pr_debug("FCC rejected - not within valid limit\n");
	return false;
}

static void fcc_learning_config(struct qpnp_bms_chip *chip, bool start)
{
	int rc, batt_temp;
	struct raw_soc_params raw;
	struct qpnp_vadc_result result;
	int fcc_uah, new_fcc_uah, delta_cc_uah, delta_soc;

	rc = qpnp_vadc_read(chip->vadc_dev, LR_MUX1_BATT_THERM, &result);
	if (rc) {
		pr_err("Unable to read batt_temp\n");
		return;
	} else {
		batt_temp = (int)result.physical;
	}

	rc = read_soc_params_raw(chip, &raw, batt_temp);
	if (rc) {
		pr_err("Unable to read CC, cannot update FCC\n");
		return;
	}

	if (start) {
		chip->start_pc = interpolate_pc(chip->pc_temp_ocv_lut,
			batt_temp, raw.last_good_ocv_uv / 1000);
		chip->start_cc_uah = calculate_cc(chip, raw.cc, CC, NORESET);
		chip->start_real_soc = calculate_real_soc(chip,
				batt_temp, &raw, chip->start_cc_uah);
		pr_debug("start_pc=%d, start_cc=%d, start_soc=%d real_soc=%d\n",
			chip->start_pc, chip->start_cc_uah,
			chip->start_soc, chip->start_real_soc);
	} else {
		chip->end_cc_uah = calculate_cc(chip, raw.cc, CC, NORESET);
		delta_soc = 100 - chip->start_real_soc;
		delta_cc_uah = abs(chip->end_cc_uah - chip->start_cc_uah);
		new_fcc_uah = div_u64(delta_cc_uah * 100, delta_soc);
		fcc_uah = calculate_fcc(chip, batt_temp);
		pr_debug("start_soc=%d, start_pc=%d, start_real_soc=%d, start_cc=%d, end_cc=%d, new_fcc=%d\n",
			chip->start_soc, chip->start_pc, chip->start_real_soc,
			chip->start_cc_uah, chip->end_cc_uah, new_fcc_uah);

		if (is_new_fcc_valid(new_fcc_uah, fcc_uah))
			update_fcc_learning_table(chip, new_fcc_uah,
					chip->charge_cycles, batt_temp);
	}
}

#define MAX_CAL_TRIES	200
#define MIN_CAL_UA	3000
static void batfet_open_work(struct work_struct *work)
{
	int i;
	int rc;
	int result_ua;
	u8 orig_delay, sample_delay;
	struct qpnp_bms_chip *chip = container_of(work,
				struct qpnp_bms_chip,
				batfet_open_work);

	rc = qpnp_read_wrapper(chip, &orig_delay,
			chip->base + BMS1_S1_DELAY_CTL, 1);

	sample_delay = 0x0;
	rc = qpnp_write_wrapper(chip, &sample_delay,
			chip->base + BMS1_S1_DELAY_CTL, 1);

	/*
	 * In certain PMICs there is a coupling issue which causes
	 * bad calibration value that result in a huge battery current
	 * even when the BATFET is open. Do continious calibrations until
	 * we hit reasonable cal values which result in low battery current
	 */

	for (i = 0; (!chip->batfet_closed) && i < MAX_CAL_TRIES; i++) {
		rc = qpnp_iadc_calibrate_for_trim(chip->iadc_dev, false);
		/*
		 * Wait 20mS after calibration and before reading battery
		 * current. The BMS h/w uses calibration values in the
		 * next sampling of vsense.
		 */
		msleep(20);
		rc |= get_battery_current(chip, &result_ua);
		if (rc == 0 && abs(result_ua) <= MIN_CAL_UA) {
			pr_debug("good cal at %d attempt\n", i);
			break;
		}
	}
	pr_debug("batfet_closed = %d i = %d result_ua = %d\n",
			chip->batfet_closed, i, result_ua);

	rc = qpnp_write_wrapper(chip, &orig_delay,
			chip->base + BMS1_S1_DELAY_CTL, 1);
}

static void charging_began(struct qpnp_bms_chip *chip)
{
	mutex_lock(&chip->last_soc_mutex);
	chip->charge_start_tm_sec = 0;
	chip->catch_up_time_sec = 0;
	mutex_unlock(&chip->last_soc_mutex);

	chip->start_soc = report_state_of_charge(chip);

	mutex_lock(&chip->last_ocv_uv_mutex);
	if (chip->enable_fcc_learning)
		fcc_learning_config(chip, true);
	chip->soc_at_cv = -EINVAL;
	chip->prev_chg_soc = -EINVAL;
	mutex_unlock(&chip->last_ocv_uv_mutex);
}

static void charging_ended(struct qpnp_bms_chip *chip)
{
	mutex_lock(&chip->last_soc_mutex);
	chip->charge_start_tm_sec = 0;
	chip->catch_up_time_sec = 0;
	mutex_unlock(&chip->last_soc_mutex);

	chip->end_soc = report_state_of_charge(chip);

	mutex_lock(&chip->last_ocv_uv_mutex);
	chip->soc_at_cv = -EINVAL;
	chip->prev_chg_soc = -EINVAL;

	/* update the chargecycles */
	if (chip->end_soc > chip->start_soc) {
		chip->charge_increase += (chip->end_soc - chip->start_soc);
		if (chip->charge_increase > 100) {
			chip->charge_cycles++;
			chip->charge_increase = chip->charge_increase % 100;
		}
		if (chip->enable_fcc_learning)
			backup_charge_cycle(chip);
	}

	if (get_battery_status(chip) == POWER_SUPPLY_STATUS_FULL) {
		if (chip->enable_fcc_learning &&
			(chip->start_soc <= chip->min_fcc_learning_soc) &&
			(chip->start_pc <= chip->min_fcc_ocv_pc))
			fcc_learning_config(chip, false);
		chip->done_charging = true;
		chip->last_soc_invalid = true;
	} else if (chip->charging_adjusted_ocv > 0) {
		pr_debug("Charging stopped before full, adjusted OCV = %d\n",
				chip->charging_adjusted_ocv);
		chip->last_ocv_uv = chip->charging_adjusted_ocv;
	}

	chip->charging_adjusted_ocv = -EINVAL;

	mutex_unlock(&chip->last_ocv_uv_mutex);
}

static void battery_status_check(struct qpnp_bms_chip *chip)
{
	int status = get_battery_status(chip);

	mutex_lock(&chip->status_lock);
	if (chip->battery_status != status) {
		pr_debug("status = %d, shadow status = %d\n",
				status, chip->battery_status);
		if (status == POWER_SUPPLY_STATUS_CHARGING) {
			pr_debug("charging started\n");
			charging_began(chip);
		} else if (chip->battery_status
				== POWER_SUPPLY_STATUS_CHARGING) {
			pr_debug("charging ended\n");
			charging_ended(chip);
		}

		if (status == POWER_SUPPLY_STATUS_FULL) {
			pr_debug("battery full\n");
			recalculate_soc(chip);
		} else if (chip->battery_status
				== POWER_SUPPLY_STATUS_FULL) {
			pr_debug("battery not full any more\n");
			disable_bms_irq(&chip->ocv_thr_irq);
			disable_bms_irq(&chip->sw_cc_thr_irq);
		}

		chip->battery_status = status;
		/* battery charge status has changed, so force a soc
		 * recalculation to update the SoC */
		schedule_work(&chip->recalc_work);
	}
	mutex_unlock(&chip->status_lock);
}

#define CALIB_WRKARND_DIG_MAJOR_MAX		0x03
static void batfet_status_check(struct qpnp_bms_chip *chip)
{
	bool batfet_closed;

	batfet_closed = is_batfet_closed(chip);
	if (chip->batfet_closed != batfet_closed) {
		chip->batfet_closed = batfet_closed;
		if (chip->iadc_bms_revision2 > CALIB_WRKARND_DIG_MAJOR_MAX)
			return;
		if (batfet_closed == false) {
			/* batfet opened */
			schedule_work(&chip->batfet_open_work);
			qpnp_iadc_skip_calibration(chip->iadc_dev);
		} else {
			/* batfet closed */
			qpnp_iadc_calibrate_for_trim(chip->iadc_dev, true);
			qpnp_iadc_resume_calibration(chip->iadc_dev);
		}
	}
}

static void battery_insertion_check(struct qpnp_bms_chip *chip)
{
	int present = (int)is_battery_present(chip);
	int insertion_ocv_uv = get_battery_insertion_ocv_uv(chip);
	int insertion_ocv_taken = (insertion_ocv_uv > 0);

	mutex_lock(&chip->vbat_monitor_mutex);
	if (chip->battery_present != present
			&& (present == insertion_ocv_taken
				|| chip->battery_present == -EINVAL)) {
		pr_debug("status = %d, shadow status = %d, insertion_ocv_uv = %d\n",
				present, chip->battery_present,
				insertion_ocv_uv);
		if (chip->battery_present != -EINVAL) {
			if (present) {
				chip->insertion_ocv_uv = insertion_ocv_uv;
				setup_vbat_monitoring(chip);
				chip->new_battery = true;
			} else {
				reset_vbat_monitoring(chip);
			}
		}
		chip->battery_present = present;
		/* a new battery was inserted or removed, so force a soc
		 * recalculation to update the SoC */
		schedule_work(&chip->recalc_work);
	}
	mutex_unlock(&chip->vbat_monitor_mutex);
}

/* Returns capacity as a SoC percentage between 0 and 100 */
static int get_prop_bms_capacity(struct qpnp_bms_chip *chip)
{
	return report_state_of_charge(chip);
}

static void qpnp_bms_external_power_changed(struct power_supply *psy)
{
	struct qpnp_bms_chip *chip = container_of(psy, struct qpnp_bms_chip,
								bms_psy);

	battery_insertion_check(chip);
	batfet_status_check(chip);
	battery_status_check(chip);
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
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->battery_status;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_prop_bms_current_now(chip);
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		val->intval = get_prop_bms_batt_resistance(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = get_prop_bms_charge_counter(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW:
		val->intval = get_prop_bms_charge_counter_shadow(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = get_prop_bms_charge_full_design(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = get_prop_bms_charge_full(chip);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = chip->charge_cycles;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#define OCV_USE_LIMIT_EN		BIT(7)
static int set_ocv_voltage_thresholds(struct qpnp_bms_chip *chip,
					int low_voltage_threshold,
					int high_voltage_threshold)
{
	uint16_t low_voltage_raw, high_voltage_raw;
	int rc;

	low_voltage_raw = convert_vbatt_uv_to_raw(chip,
				low_voltage_threshold);
	high_voltage_raw = convert_vbatt_uv_to_raw(chip,
				high_voltage_threshold);
	rc = qpnp_write_wrapper(chip, (u8 *)&low_voltage_raw,
			chip->base + BMS1_OCV_USE_LOW_LIMIT_THR0, 2);
	if (rc) {
		pr_err("Failed to set ocv low voltage threshold: %d\n", rc);
		return rc;
	}
	rc = qpnp_write_wrapper(chip, (u8 *)&high_voltage_raw,
			chip->base + BMS1_OCV_USE_HIGH_LIMIT_THR0, 2);
	if (rc) {
		pr_err("Failed to set ocv high voltage threshold: %d\n", rc);
		return rc;
	}
	rc = qpnp_masked_write(chip, BMS1_OCV_USE_LIMIT_CTL,
				OCV_USE_LIMIT_EN, OCV_USE_LIMIT_EN);
	if (rc) {
		pr_err("Failed to enabled ocv voltage thresholds: %d\n", rc);
		return rc;
	}
	pr_debug("ocv low threshold set to %d uv or 0x%x raw\n",
				low_voltage_threshold, low_voltage_raw);
	pr_debug("ocv high threshold set to %d uv or 0x%x raw\n",
				high_voltage_threshold, high_voltage_raw);
	return 0;
}

static int read_shutdown_iavg_ma(struct qpnp_bms_chip *chip)
{
	u8 iavg;
	int rc;

	rc = qpnp_read_wrapper(chip, &iavg, chip->base + IAVG_STORAGE_REG, 1);
	if (rc) {
		pr_err("failed to read addr = %d %d assuming %d\n",
				chip->base + IAVG_STORAGE_REG, rc,
				MIN_IAVG_MA);
		return MIN_IAVG_MA;
	} else if (iavg == IAVG_INVALID) {
		pr_err("invalid iavg read from BMS1_DATA_REG_1, using %d\n",
				MIN_IAVG_MA);
		return MIN_IAVG_MA;
	} else {
		if (iavg == 0)
			return MIN_IAVG_MA;
		else
			return MIN_IAVG_MA + IAVG_STEP_SIZE_MA * iavg;
	}
}

static int read_shutdown_soc(struct qpnp_bms_chip *chip)
{
	u8 stored_soc;
	int rc, shutdown_soc;

	/*
	 * The previous SOC is stored in the first 7 bits of the register as
	 * (Shutdown SOC + 1). This allows for register reset values of both
	 * 0x00 and 0x7F.
	 */
	rc = qpnp_read_wrapper(chip, &stored_soc, chip->soc_storage_addr, 1);
	if (rc) {
		pr_err("failed to read addr = %d %d\n",
				chip->soc_storage_addr, rc);
		return SOC_INVALID;
	}

	if ((stored_soc >> 1) > 0)
		shutdown_soc = (stored_soc >> 1) - 1;
	else
		shutdown_soc = SOC_INVALID;

	pr_debug("stored soc = 0x%02x, shutdown_soc = %d\n",
			stored_soc, shutdown_soc);
	return shutdown_soc;
}

#define BAT_REMOVED_OFFMODE_BIT		BIT(6)
static bool is_battery_replaced_in_offmode(struct qpnp_bms_chip *chip)
{
	u8 batt_pres;
	int rc;

	if (chip->batt_pres_addr) {
		rc = qpnp_read_wrapper(chip, &batt_pres,
				chip->batt_pres_addr, 1);
		pr_debug("offmode removed: %02x\n", batt_pres);
		if (!rc && (batt_pres & BAT_REMOVED_OFFMODE_BIT))
			return true;
	}
	return false;
}

static void load_shutdown_data(struct qpnp_bms_chip *chip)
{
	int calculated_soc, shutdown_soc;
	bool invalid_stored_soc;
	bool offmode_battery_replaced;
	bool shutdown_soc_out_of_limit;

	/*
	 * Read the saved shutdown SoC from the configured register and
	 * check if the value has been reset
	 */
	shutdown_soc = read_shutdown_soc(chip);
	invalid_stored_soc = (shutdown_soc == SOC_INVALID);

	/*
	 * Do a quick run of SoC calculation to find whether the shutdown soc
	 * is close enough.
	 */
	chip->shutdown_iavg_ma = MIN_IAVG_MA;
	calculated_soc = recalculate_raw_soc(chip);
	shutdown_soc_out_of_limit = (abs(shutdown_soc - calculated_soc)
			> chip->shutdown_soc_valid_limit);
	pr_debug("calculated_soc = %d, valid_limit = %d\n",
			calculated_soc, chip->shutdown_soc_valid_limit);

	/*
	 * Check if the battery has been replaced while the system was powered
	 * down.
	 */
	offmode_battery_replaced = is_battery_replaced_in_offmode(chip);

	/* Invalidate the shutdown SoC if any of these conditions hold true */
	if (chip->ignore_shutdown_soc
			|| invalid_stored_soc
			|| offmode_battery_replaced
			|| shutdown_soc_out_of_limit) {
		chip->battery_removed = true;
		chip->shutdown_soc_invalid = true;
		chip->shutdown_iavg_ma = MIN_IAVG_MA;
		pr_debug("Ignoring shutdown SoC: invalid = %d, offmode = %d, out_of_limit = %d\n",
				invalid_stored_soc, offmode_battery_replaced,
				shutdown_soc_out_of_limit);
	} else {
		chip->shutdown_iavg_ma = read_shutdown_iavg_ma(chip);
		chip->shutdown_soc = shutdown_soc;
	}

	pr_debug("raw_soc = %d shutdown_soc = %d shutdown_iavg = %d shutdown_soc_invalid = %d, battery_removed = %d\n",
			calculated_soc,
			chip->shutdown_soc,
			chip->shutdown_iavg_ma,
			chip->shutdown_soc_invalid,
			chip->battery_removed);
}

static irqreturn_t bms_ocv_thr_irq_handler(int irq, void *_chip)
{
	struct qpnp_bms_chip *chip = _chip;

	pr_debug("ocv_thr irq triggered\n");
	bms_stay_awake(&chip->soc_wake_source);
	schedule_work(&chip->recalc_work);
	return IRQ_HANDLED;
}

static irqreturn_t bms_sw_cc_thr_irq_handler(int irq, void *_chip)
{
	struct qpnp_bms_chip *chip = _chip;

	pr_debug("sw_cc_thr irq triggered\n");
	disable_bms_irq_nosync(&chip->sw_cc_thr_irq);
	bms_stay_awake(&chip->soc_wake_source);
	schedule_work(&chip->recalc_work);
	return IRQ_HANDLED;
}

static int64_t read_battery_id(struct qpnp_bms_chip *chip)
{
	int rc;
	struct qpnp_vadc_result result;

	rc = qpnp_vadc_read(chip->vadc_dev, LR_MUX2_BAT_ID, &result);
	if (rc) {
		pr_err("error reading batt id channel = %d, rc = %d\n",
					LR_MUX2_BAT_ID, rc);
		return rc;
	}

	return result.physical;
}

static int set_battery_data(struct qpnp_bms_chip *chip)
{
	int64_t battery_id;
	int rc = 0, dt_data = false;
	struct bms_battery_data *batt_data;
	struct device_node *node;

	if (chip->batt_type == BATT_DESAY) {
		batt_data = &desay_5200_data;
	} else if (chip->batt_type == BATT_PALLADIUM) {
		batt_data = &palladium_1500_data;
	} else if (chip->batt_type == BATT_OEM) {
		batt_data = &oem_batt_data;
	} else if (chip->batt_type == BATT_QRD_4V35_2000MAH) {
		batt_data = &QRD_4v35_2000mAh_data;
	} else if (chip->batt_type == BATT_QRD_4V2_1300MAH) {
		batt_data = &qrd_4v2_1300mah_data;
	} else {
		battery_id = read_battery_id(chip);
		if (battery_id < 0) {
			pr_err("cannot read battery id err = %lld\n",
							battery_id);
			return battery_id;
		}

		node = of_find_node_by_name(chip->spmi->dev.of_node,
				"qcom,battery-data");
		if (!node) {
			pr_warn("No available batterydata, using palladium 1500\n");
			batt_data = &palladium_1500_data;
			goto assign_data;
		}
		batt_data = devm_kzalloc(chip->dev,
				sizeof(struct bms_battery_data), GFP_KERNEL);
		if (!batt_data) {
			pr_err("Could not alloc battery data\n");
			batt_data = &palladium_1500_data;
			goto assign_data;
		}
		batt_data->fcc_temp_lut = devm_kzalloc(chip->dev,
				sizeof(struct single_row_lut),
				GFP_KERNEL);
		batt_data->pc_temp_ocv_lut = devm_kzalloc(chip->dev,
				sizeof(struct pc_temp_ocv_lut),
				GFP_KERNEL);
		batt_data->rbatt_sf_lut = devm_kzalloc(chip->dev,
				sizeof(struct sf_lut),
				GFP_KERNEL);

		batt_data->max_voltage_uv = -1;
		batt_data->cutoff_uv = -1;
		batt_data->iterm_ua = -1;

		/*
		 * if the alloced luts are 0s, of_batterydata_read_data ignores
		 * them.
		 */
		rc = of_batterydata_read_data(node, batt_data, battery_id);
		if (rc == 0 && batt_data->fcc_temp_lut
				&& batt_data->pc_temp_ocv_lut
				&& batt_data->rbatt_sf_lut) {
			dt_data = true;
		} else {
			pr_err("battery data load failed, using palladium 1500\n");
			devm_kfree(chip->dev, batt_data->fcc_temp_lut);
			devm_kfree(chip->dev, batt_data->pc_temp_ocv_lut);
			devm_kfree(chip->dev, batt_data->rbatt_sf_lut);
			devm_kfree(chip->dev, batt_data);
			batt_data = &palladium_1500_data;
		}
	}

assign_data:
	chip->fcc_mah = batt_data->fcc;
	chip->fcc_temp_lut = batt_data->fcc_temp_lut;
	chip->fcc_sf_lut = batt_data->fcc_sf_lut;
	chip->pc_temp_ocv_lut = batt_data->pc_temp_ocv_lut;
	chip->pc_sf_lut = batt_data->pc_sf_lut;
	chip->rbatt_sf_lut = batt_data->rbatt_sf_lut;
	chip->default_rbatt_mohm = batt_data->default_rbatt_mohm;
	chip->rbatt_capacitive_mohm = batt_data->rbatt_capacitive_mohm;
	chip->flat_ocv_threshold_uv = batt_data->flat_ocv_threshold_uv;

	/* Override battery properties if specified in the battery profile */
	if (batt_data->max_voltage_uv >= 0 && dt_data)
		chip->max_voltage_uv = batt_data->max_voltage_uv;
	if (batt_data->cutoff_uv >= 0 && dt_data)
		chip->v_cutoff_uv = batt_data->cutoff_uv;
	if (batt_data->iterm_ua >= 0 && dt_data)
		chip->chg_term_ua = batt_data->iterm_ua;

	if (chip->pc_temp_ocv_lut == NULL) {
		pr_err("temp ocv lut table has not been loaded\n");
		if (dt_data) {
			devm_kfree(chip->dev, batt_data->fcc_temp_lut);
			devm_kfree(chip->dev, batt_data->pc_temp_ocv_lut);
			devm_kfree(chip->dev, batt_data->rbatt_sf_lut);
			devm_kfree(chip->dev, batt_data);
		}
		return -EINVAL;
	}

	if (dt_data)
		devm_kfree(chip->dev, batt_data);

	return 0;
}

static int bms_get_adc(struct qpnp_bms_chip *chip,
					struct spmi_device *spmi)
{
	int rc = 0;

	chip->vadc_dev = qpnp_get_vadc(&spmi->dev, "bms");
	if (IS_ERR(chip->vadc_dev)) {
		rc = PTR_ERR(chip->vadc_dev);
		if (rc != -EPROBE_DEFER)
			pr_err("vadc property missing, rc=%d\n", rc);
		return rc;
	}

	chip->iadc_dev = qpnp_get_iadc(&spmi->dev, "bms");
	if (IS_ERR(chip->iadc_dev)) {
		rc = PTR_ERR(chip->iadc_dev);
		if (rc != -EPROBE_DEFER)
			pr_err("iadc property missing, rc=%d\n", rc);
		return rc;
	}

	chip->adc_tm_dev = qpnp_get_adc_tm(&spmi->dev, "bms");
	if (IS_ERR(chip->adc_tm_dev)) {
		rc = PTR_ERR(chip->adc_tm_dev);
		if (rc != -EPROBE_DEFER)
			pr_err("adc-tm not ready, defer probe\n");
		return rc;
	}

	return 0;
}

#define SPMI_PROP_READ(chip_prop, qpnp_spmi_property, retval)		\
do {									\
	if (retval)							\
		break;							\
	retval = of_property_read_u32(chip->spmi->dev.of_node,		\
				"qcom," qpnp_spmi_property,		\
					&chip->chip_prop);		\
	if (retval) {							\
		pr_err("Error reading " #qpnp_spmi_property		\
						" property %d\n", rc);	\
	}								\
} while (0)

#define SPMI_PROP_READ_BOOL(chip_prop, qpnp_spmi_property)		\
do {									\
	chip->chip_prop = of_property_read_bool(chip->spmi->dev.of_node,\
				"qcom," qpnp_spmi_property);		\
} while (0)

static inline int bms_read_properties(struct qpnp_bms_chip *chip)
{
	int rc = 0;

	SPMI_PROP_READ(r_sense_uohm, "r-sense-uohm", rc);
	SPMI_PROP_READ(v_cutoff_uv, "v-cutoff-uv", rc);
	SPMI_PROP_READ(max_voltage_uv, "max-voltage-uv", rc);
	SPMI_PROP_READ(r_conn_mohm, "r-conn-mohm", rc);
	SPMI_PROP_READ(chg_term_ua, "chg-term-ua", rc);
	SPMI_PROP_READ(shutdown_soc_valid_limit,
			"shutdown-soc-valid-limit", rc);
	SPMI_PROP_READ(adjust_soc_low_threshold,
			"adjust-soc-low-threshold", rc);
	SPMI_PROP_READ(batt_type, "batt-type", rc);
	SPMI_PROP_READ(low_soc_calc_threshold,
			"low-soc-calculate-soc-threshold", rc);
	SPMI_PROP_READ(low_soc_calculate_soc_ms,
			"low-soc-calculate-soc-ms", rc);
	SPMI_PROP_READ(low_voltage_calculate_soc_ms,
			"low-voltage-calculate-soc-ms", rc);
	SPMI_PROP_READ(calculate_soc_ms, "calculate-soc-ms", rc);
	SPMI_PROP_READ(high_ocv_correction_limit_uv,
			"high-ocv-correction-limit-uv", rc);
	SPMI_PROP_READ(low_ocv_correction_limit_uv,
			"low-ocv-correction-limit-uv", rc);
	SPMI_PROP_READ(hold_soc_est,
			"hold-soc-est", rc);
	SPMI_PROP_READ(ocv_high_threshold_uv,
			"ocv-voltage-high-threshold-uv", rc);
	SPMI_PROP_READ(ocv_low_threshold_uv,
			"ocv-voltage-low-threshold-uv", rc);
	SPMI_PROP_READ(low_voltage_threshold, "low-voltage-threshold", rc);
	SPMI_PROP_READ(temperature_margin, "tm-temp-margin", rc);

	chip->use_external_rsense = of_property_read_bool(
			chip->spmi->dev.of_node,
			"qcom,use-external-rsense");
	chip->ignore_shutdown_soc = of_property_read_bool(
			chip->spmi->dev.of_node,
			"qcom,ignore-shutdown-soc");
	chip->use_voltage_soc = of_property_read_bool(chip->spmi->dev.of_node,
			"qcom,use-voltage-soc");
	chip->use_ocv_thresholds = of_property_read_bool(
			chip->spmi->dev.of_node,
			"qcom,use-ocv-thresholds");

	if (chip->adjust_soc_low_threshold >= 45)
		chip->adjust_soc_low_threshold = 45;

	SPMI_PROP_READ_BOOL(enable_fcc_learning, "enable-fcc-learning");
	if (chip->enable_fcc_learning) {
		SPMI_PROP_READ(min_fcc_learning_soc,
				"min-fcc-learning-soc", rc);
		SPMI_PROP_READ(min_fcc_ocv_pc,
				"min-fcc-ocv-pc", rc);
		SPMI_PROP_READ(min_fcc_learning_samples,
				"min-fcc-learning-samples", rc);
		SPMI_PROP_READ(fcc_resolution,
				"fcc-resolution", rc);
		if (chip->min_fcc_learning_samples > MAX_FCC_CYCLES)
			chip->min_fcc_learning_samples = MAX_FCC_CYCLES;
		chip->fcc_learning_samples = devm_kzalloc(&chip->spmi->dev,
				(sizeof(struct fcc_sample) *
				chip->min_fcc_learning_samples), GFP_KERNEL);
		if (chip->fcc_learning_samples == NULL)
			return -ENOMEM;
		pr_debug("min-fcc-soc=%d, min-fcc-pc=%d, min-fcc-cycles=%d\n",
			chip->min_fcc_learning_soc, chip->min_fcc_ocv_pc,
			chip->min_fcc_learning_samples);
	}

	if (rc) {
		pr_err("Missing required properties.\n");
		return rc;
	}

	pr_debug("dts data: r_sense_uohm:%d, v_cutoff_uv:%d, max_v:%d\n",
			chip->r_sense_uohm, chip->v_cutoff_uv,
			chip->max_voltage_uv);
	pr_debug("r_conn:%d, shutdown_soc: %d, adjust_soc_low:%d\n",
			chip->r_conn_mohm, chip->shutdown_soc_valid_limit,
			chip->adjust_soc_low_threshold);
	pr_debug("chg_term_ua:%d, batt_type:%d\n",
			chip->chg_term_ua,
			chip->batt_type);
	pr_debug("ignore_shutdown_soc:%d, use_voltage_soc:%d\n",
			chip->ignore_shutdown_soc, chip->use_voltage_soc);
	pr_debug("use external rsense: %d\n", chip->use_external_rsense);
	return 0;
}

static inline void bms_initialize_constants(struct qpnp_bms_chip *chip)
{
	chip->prev_pc_unusable = -EINVAL;
	chip->soc_at_cv = -EINVAL;
	chip->calculated_soc = -EINVAL;
	chip->last_soc = -EINVAL;
	chip->last_soc_est = -EINVAL;
	chip->battery_present = -EINVAL;
	chip->battery_status = POWER_SUPPLY_STATUS_UNKNOWN;
	chip->last_cc_uah = INT_MIN;
	chip->ocv_reading_at_100 = OCV_RAW_UNINITIALIZED;
	chip->prev_last_good_ocv_raw = OCV_RAW_UNINITIALIZED;
	chip->first_time_calc_soc = 1;
	chip->first_time_calc_uuc = 1;
}

#define SPMI_FIND_IRQ(chip, irq_name)					\
do {									\
	chip->irq_name##_irq.irq = spmi_get_irq_byname(chip->spmi,	\
					resource, #irq_name);		\
	if (chip->irq_name##_irq.irq < 0) {				\
		pr_err("Unable to get " #irq_name " irq\n");		\
		return -ENXIO;						\
	}								\
} while (0)

static int bms_find_irqs(struct qpnp_bms_chip *chip,
			struct spmi_resource *resource)
{
	SPMI_FIND_IRQ(chip, sw_cc_thr);
	SPMI_FIND_IRQ(chip, ocv_thr);
	return 0;
}

#define SPMI_REQUEST_IRQ(chip, rc, irq_name)				\
do {									\
	rc = devm_request_irq(chip->dev, chip->irq_name##_irq.irq,	\
			bms_##irq_name##_irq_handler,			\
			IRQF_TRIGGER_RISING, #irq_name, chip);		\
	if (rc < 0) {							\
		pr_err("Unable to request " #irq_name " irq: %d\n", rc);\
		return -ENXIO;						\
	}								\
	chip->irq_name##_irq.ready = true;				\
} while (0)

static int bms_request_irqs(struct qpnp_bms_chip *chip)
{
	int rc;

	SPMI_REQUEST_IRQ(chip, rc, sw_cc_thr);
	disable_bms_irq(&chip->sw_cc_thr_irq);
	enable_irq_wake(chip->sw_cc_thr_irq.irq);
	SPMI_REQUEST_IRQ(chip, rc, ocv_thr);
	disable_bms_irq(&chip->ocv_thr_irq);
	enable_irq_wake(chip->ocv_thr_irq.irq);
	return 0;
}

#define REG_OFFSET_PERP_TYPE			0x04
#define REG_OFFSET_PERP_SUBTYPE			0x05
#define BMS_BMS_TYPE				0xD
#define BMS_BMS1_SUBTYPE			0x1
#define BMS_IADC_TYPE				0x8
#define BMS_IADC1_SUBTYPE			0x3
#define BMS_IADC2_SUBTYPE			0x5

static int register_spmi(struct qpnp_bms_chip *chip, struct spmi_device *spmi)
{
	struct spmi_resource *spmi_resource;
	struct resource *resource;
	int rc;
	u8 type, subtype;

	chip->dev = &(spmi->dev);
	chip->spmi = spmi;

	spmi_for_each_container_dev(spmi_resource, spmi) {
		if (!spmi_resource) {
			pr_err("qpnp_bms: spmi resource absent\n");
			return -ENXIO;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			pr_err("node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			return -ENXIO;
		}

		pr_debug("Node name = %s\n", spmi_resource->of_node->name);

		if (strcmp("qcom,batt-pres-status",
					spmi_resource->of_node->name) == 0) {
			chip->batt_pres_addr = resource->start;
			continue;
		} else if (strcmp("qcom,soc-storage-reg",
					spmi_resource->of_node->name) == 0) {
			chip->soc_storage_addr = resource->start;
			continue;
		}

		rc = qpnp_read_wrapper(chip, &type,
				resource->start + REG_OFFSET_PERP_TYPE, 1);
		if (rc) {
			pr_err("Peripheral type read failed rc=%d\n", rc);
			return rc;
		}
		rc = qpnp_read_wrapper(chip, &subtype,
				resource->start + REG_OFFSET_PERP_SUBTYPE, 1);
		if (rc) {
			pr_err("Peripheral subtype read failed rc=%d\n", rc);
			return rc;
		}

		if (type == BMS_BMS_TYPE && subtype == BMS_BMS1_SUBTYPE) {
			chip->base = resource->start;
			rc = bms_find_irqs(chip, spmi_resource);
			if (rc) {
				pr_err("Could not find irqs\n");
				return rc;
			}
		} else if (type == BMS_IADC_TYPE
				&& (subtype == BMS_IADC1_SUBTYPE
				|| subtype == BMS_IADC2_SUBTYPE)) {
			chip->iadc_base = resource->start;
		} else {
			pr_err("Invalid peripheral start=0x%x type=0x%x, subtype=0x%x\n",
					resource->start, type, subtype);
		}
	}

	if (chip->base == 0) {
		dev_err(&spmi->dev, "BMS peripheral was not registered\n");
		return -EINVAL;
	}
	if (chip->iadc_base == 0) {
		dev_err(&spmi->dev, "BMS_IADC peripheral was not registered\n");
		return -EINVAL;
	}
	if (chip->soc_storage_addr == 0) {
		/* default to dvdd backed BMS data reg0 */
		chip->soc_storage_addr = chip->base + SOC_STORAGE_REG;
	}

	pr_debug("bms-base = 0x%04x, iadc-base = 0x%04x, bat-pres-reg = 0x%04x, soc-storage-reg = 0x%04x\n",
			chip->base, chip->iadc_base,
			chip->batt_pres_addr, chip->soc_storage_addr);
	return 0;
}

#define ADC_CH_SEL_MASK				0x7
#define ADC_INT_RSNSN_CTL_MASK			0x3
#define ADC_INT_RSNSN_CTL_VALUE_EXT_RENSE	0x2
#define FAST_AVG_EN_MASK			0x80
#define FAST_AVG_EN_VALUE_EXT_RSENSE		0x80
static int read_iadc_channel_select(struct qpnp_bms_chip *chip)
{
	u8 iadc_channel_select;
	int32_t rds_rsense_nohm;
	int rc;

	rc = qpnp_read_wrapper(chip, &iadc_channel_select,
			chip->iadc_base + IADC1_BMS_ADC_CH_SEL_CTL, 1);
	if (rc) {
		pr_err("Error reading bms_iadc channel register %d\n", rc);
		return rc;
	}

	iadc_channel_select &= ADC_CH_SEL_MASK;
	if (iadc_channel_select != EXTERNAL_RSENSE
			&& iadc_channel_select != INTERNAL_RSENSE) {
		pr_err("IADC1_BMS_IADC configured incorrectly. Selected channel = %d\n",
						iadc_channel_select);
		return -EINVAL;
	}

	if (chip->use_external_rsense) {
		pr_debug("External rsense selected\n");
		if (iadc_channel_select == INTERNAL_RSENSE) {
			pr_debug("Internal rsense detected; Changing rsense to external\n");
			rc = qpnp_masked_write_iadc(chip,
					IADC1_BMS_ADC_CH_SEL_CTL,
					ADC_CH_SEL_MASK,
					EXTERNAL_RSENSE);
			if (rc) {
				pr_err("Unable to set IADC1_BMS channel %x to %x: %d\n",
						IADC1_BMS_ADC_CH_SEL_CTL,
						EXTERNAL_RSENSE, rc);
				return rc;
			}
			reset_cc(chip, CLEAR_CC | CLEAR_SHDW_CC);
			chip->software_cc_uah = 0;
			chip->software_shdw_cc_uah = 0;
		}
	} else {
		pr_debug("Internal rsense selected\n");
		if (iadc_channel_select == EXTERNAL_RSENSE) {
			pr_debug("External rsense detected; Changing rsense to internal\n");
			rc = qpnp_masked_write_iadc(chip,
					IADC1_BMS_ADC_CH_SEL_CTL,
					ADC_CH_SEL_MASK,
					INTERNAL_RSENSE);
			if (rc) {
				pr_err("Unable to set IADC1_BMS channel %x to %x: %d\n",
						IADC1_BMS_ADC_CH_SEL_CTL,
						INTERNAL_RSENSE, rc);
				return rc;
			}
			reset_cc(chip, CLEAR_CC | CLEAR_SHDW_CC);
			chip->software_shdw_cc_uah = 0;
		}

		rc = qpnp_iadc_get_rsense(chip->iadc_dev, &rds_rsense_nohm);
		if (rc) {
			pr_err("Unable to read RDS resistance value from IADC; rc = %d\n",
								rc);
			return rc;
		}
		chip->r_sense_uohm = rds_rsense_nohm/1000;
		pr_debug("rds_rsense = %d nOhm, saved as %d uOhm\n",
					rds_rsense_nohm, chip->r_sense_uohm);
	}
	/* prevent shorting of leads by IADC_BMS when external Rsense is used */
	if (chip->use_external_rsense) {
		if (chip->iadc_bms_revision2 > CALIB_WRKARND_DIG_MAJOR_MAX) {
			rc = qpnp_masked_write_iadc(chip,
					IADC1_BMS_ADC_INT_RSNSN_CTL,
					ADC_INT_RSNSN_CTL_MASK,
					ADC_INT_RSNSN_CTL_VALUE_EXT_RENSE);
			if (rc) {
				pr_err("Unable to set batfet config %x to %x: %d\n",
					IADC1_BMS_ADC_INT_RSNSN_CTL,
					ADC_INT_RSNSN_CTL_VALUE_EXT_RENSE, rc);
				return rc;
			}
		} else {
			/* In older PMICS use FAST_AVG_EN register bit 7 */
			rc = qpnp_masked_write_iadc(chip,
					IADC1_BMS_FAST_AVG_EN,
					FAST_AVG_EN_MASK,
					FAST_AVG_EN_VALUE_EXT_RSENSE);
			if (rc) {
				pr_err("Unable to set batfet config %x to %x: %d\n",
					IADC1_BMS_FAST_AVG_EN,
					FAST_AVG_EN_VALUE_EXT_RSENSE, rc);
				return rc;
			}
		}
	}

	return 0;
}

static int refresh_die_temp_monitor(struct qpnp_bms_chip *chip)
{
	struct qpnp_vadc_result result;
	int rc;

	rc = qpnp_vadc_read(chip->vadc_dev, DIE_TEMP, &result);

	pr_debug("low = %lld, high = %lld\n",
			result.physical - chip->temperature_margin,
			result.physical + chip->temperature_margin);
	chip->die_temp_monitor_params.high_temp = result.physical
						+ chip->temperature_margin;
	chip->die_temp_monitor_params.low_temp = result.physical
						- chip->temperature_margin;
	chip->die_temp_monitor_params.state_request =
						ADC_TM_HIGH_LOW_THR_ENABLE;
	return qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
					&chip->die_temp_monitor_params);
}

static void btm_notify_die_temp(enum qpnp_tm_state state, void *ctx)
{
	struct qpnp_bms_chip *chip = ctx;
	struct qpnp_vadc_result result;
	int rc;

	rc = qpnp_vadc_read(chip->vadc_dev, DIE_TEMP, &result);

	if (state == ADC_TM_LOW_STATE)
		pr_debug("low state triggered\n");
	else if (state == ADC_TM_HIGH_STATE)
		pr_debug("high state triggered\n");
	pr_debug("die temp = %lld, raw = 0x%x\n",
			result.physical, result.adc_code);
	schedule_work(&chip->recalc_work);
	refresh_die_temp_monitor(chip);
}

static int setup_die_temp_monitoring(struct qpnp_bms_chip *chip)
{
	int rc;

	chip->die_temp_monitor_params.channel = DIE_TEMP;
	chip->die_temp_monitor_params.btm_ctx = (void *)chip;
	chip->die_temp_monitor_params.timer_interval = ADC_MEAS1_INTERVAL_1S;
	chip->die_temp_monitor_params.threshold_notification =
						&btm_notify_die_temp;
	rc = refresh_die_temp_monitor(chip);
	if (rc) {
		pr_err("tm setup failed: %d\n", rc);
		return rc;
	}
	pr_debug("setup complete\n");
	return 0;
}

static int __devinit qpnp_bms_probe(struct spmi_device *spmi)
{
	struct qpnp_bms_chip *chip;
	bool warm_reset;
	int rc, vbatt;

	chip = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_bms_chip),
			GFP_KERNEL);

	if (chip == NULL) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}

	rc = bms_get_adc(chip, spmi);
	if (rc < 0)
		goto error_read;

	mutex_init(&chip->bms_output_lock);
	mutex_init(&chip->last_ocv_uv_mutex);
	mutex_init(&chip->vbat_monitor_mutex);
	mutex_init(&chip->soc_invalidation_mutex);
	mutex_init(&chip->last_soc_mutex);
	mutex_init(&chip->status_lock);
	init_waitqueue_head(&chip->bms_wait_queue);

	warm_reset = qpnp_pon_is_warm_reset();
	rc = warm_reset;
	if (rc < 0)
		goto error_read;

	rc = register_spmi(chip, spmi);
	if (rc) {
		pr_err("error registering spmi resource %d\n", rc);
		goto error_resource;
	}

	rc = qpnp_read_wrapper(chip, &chip->revision1,
			chip->base + REVISION1, 1);
	if (rc) {
		pr_err("error reading version register %d\n", rc);
		goto error_read;
	}

	rc = qpnp_read_wrapper(chip, &chip->revision2,
			chip->base + REVISION2, 1);
	if (rc) {
		pr_err("Error reading version register %d\n", rc);
		goto error_read;
	}
	pr_debug("BMS version: %hhu.%hhu\n", chip->revision2, chip->revision1);

	rc = qpnp_read_wrapper(chip, &chip->iadc_bms_revision2,
			chip->iadc_base + REVISION2, 1);
	if (rc) {
		pr_err("Error reading version register %d\n", rc);
		goto error_read;
	}

	rc = qpnp_read_wrapper(chip, &chip->iadc_bms_revision1,
			chip->iadc_base + REVISION1, 1);
	if (rc) {
		pr_err("Error reading version register %d\n", rc);
		goto error_read;
	}
	pr_debug("IADC_BMS version: %hhu.%hhu\n",
			chip->iadc_bms_revision2, chip->iadc_bms_revision1);

	rc = bms_read_properties(chip);
	if (rc) {
		pr_err("Unable to read all bms properties, rc = %d\n", rc);
		goto error_read;
	}

	rc = read_iadc_channel_select(chip);
	if (rc) {
		pr_err("Unable to get iadc selected channel = %d\n", rc);
		goto error_read;
	}

	if (chip->use_ocv_thresholds) {
		rc = set_ocv_voltage_thresholds(chip,
				chip->ocv_low_threshold_uv,
				chip->ocv_high_threshold_uv);
		if (rc) {
			pr_err("Could not set ocv voltage thresholds: %d\n",
					rc);
			goto error_read;
		}
	}

	rc = set_battery_data(chip);
	if (rc) {
		pr_err("Bad battery data %d\n", rc);
		goto error_read;
	}

	bms_initialize_constants(chip);

	wakeup_source_init(&chip->soc_wake_source.source, "qpnp_soc_wake");
	wake_lock_init(&chip->low_voltage_wake_lock, WAKE_LOCK_SUSPEND,
			"qpnp_low_voltage_lock");
	wake_lock_init(&chip->cv_wake_lock, WAKE_LOCK_SUSPEND,
			"qpnp_cv_lock");
	INIT_DELAYED_WORK(&chip->calculate_soc_delayed_work,
			calculate_soc_work);
	INIT_WORK(&chip->recalc_work, recalculate_work);
	INIT_WORK(&chip->batfet_open_work, batfet_open_work);

	dev_set_drvdata(&spmi->dev, chip);
	device_init_wakeup(&spmi->dev, 1);

	load_shutdown_data(chip);

	if (chip->enable_fcc_learning) {
		if (chip->battery_removed) {
			rc = discard_backup_fcc_data(chip);
			if (rc)
				pr_err("Could not discard backed-up FCC data\n");
		} else {
			rc = read_chgcycle_data_from_backup(chip);
			if (rc)
				pr_err("Unable to restore charge-cycle data\n");

			rc = read_fcc_data_from_backup(chip);
			if (rc)
				pr_err("Unable to restore FCC-learning data\n");
			else
				attempt_learning_new_fcc(chip);
		}
	}

	rc = setup_vbat_monitoring(chip);
	if (rc < 0) {
		pr_err("failed to set up voltage notifications: %d\n", rc);
		goto error_setup;
	}

	rc = setup_die_temp_monitoring(chip);
	if (rc < 0) {
		pr_err("failed to set up die temp notifications: %d\n", rc);
		goto error_setup;
	}

	rc = bms_request_irqs(chip);
	if (rc) {
		pr_err("error requesting bms irqs, rc = %d\n", rc);
		goto error_setup;
	}

	battery_insertion_check(chip);
	batfet_status_check(chip);
	battery_status_check(chip);

	calculate_soc_work(&(chip->calculate_soc_delayed_work.work));

	/* setup & register the battery power supply */
	chip->bms_psy.name = "bms";
	chip->bms_psy.type = POWER_SUPPLY_TYPE_BMS;
	chip->bms_psy.properties = msm_bms_power_props;
	chip->bms_psy.num_properties = ARRAY_SIZE(msm_bms_power_props);
	chip->bms_psy.get_property = qpnp_bms_power_get_property;
	chip->bms_psy.external_power_changed =
		qpnp_bms_external_power_changed;
	chip->bms_psy.supplied_to = qpnp_bms_supplicants;
	chip->bms_psy.num_supplicants = ARRAY_SIZE(qpnp_bms_supplicants);

	rc = power_supply_register(chip->dev, &chip->bms_psy);

	if (rc < 0) {
		pr_err("power_supply_register bms failed rc = %d\n", rc);
		goto unregister_dc;
	}

	chip->bms_psy_registered = true;
	vbatt = 0;
	rc = get_battery_voltage(chip, &vbatt);
	if (rc) {
		pr_err("error reading vbat_sns adc channel = %d, rc = %d\n",
						VBAT_SNS, rc);
		goto unregister_dc;
	}

	pr_info("probe success: soc =%d vbatt = %d ocv = %d r_sense_uohm = %u warm_reset = %d\n",
			get_prop_bms_capacity(chip), vbatt, chip->last_ocv_uv,
			chip->r_sense_uohm, warm_reset);
	return 0;

unregister_dc:
	chip->bms_psy_registered = false;
	power_supply_unregister(&chip->bms_psy);
error_setup:
	dev_set_drvdata(&spmi->dev, NULL);
	wakeup_source_trash(&chip->soc_wake_source.source);
	wake_lock_destroy(&chip->low_voltage_wake_lock);
	wake_lock_destroy(&chip->cv_wake_lock);
error_resource:
error_read:
	return rc;
}

static int qpnp_bms_remove(struct spmi_device *spmi)
{
	dev_set_drvdata(&spmi->dev, NULL);
	return 0;
}

static int bms_suspend(struct device *dev)
{
	struct qpnp_bms_chip *chip = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&chip->calculate_soc_delayed_work);
	chip->was_charging_at_sleep = is_battery_charging(chip);
	return 0;
}

static int bms_resume(struct device *dev)
{
	int rc;
	int soc_calc_period;
	int time_until_next_recalc = 0;
	unsigned long time_since_last_recalc;
	unsigned long tm_now_sec;
	struct qpnp_bms_chip *chip = dev_get_drvdata(dev);

	rc = get_current_time(&tm_now_sec);
	if (rc) {
		pr_err("Could not read current time: %d\n", rc);
	} else {
		soc_calc_period = get_calculation_delay_ms(chip);
		time_since_last_recalc = tm_now_sec - chip->last_recalc_time;
		pr_debug("Time since last recalc: %lu\n",
				time_since_last_recalc);
		time_until_next_recalc = max(0, soc_calc_period
				- (int)(time_since_last_recalc * 1000));
	}

	if (time_until_next_recalc == 0)
		bms_stay_awake(&chip->soc_wake_source);
	schedule_delayed_work(&chip->calculate_soc_delayed_work,
		round_jiffies_relative(msecs_to_jiffies
		(time_until_next_recalc)));
	return 0;
}

static const struct dev_pm_ops qpnp_bms_pm_ops = {
	.resume		= bms_resume,
	.suspend	= bms_suspend,
};

static struct spmi_driver qpnp_bms_driver = {
	.probe		= qpnp_bms_probe,
	.remove		= __devexit_p(qpnp_bms_remove),
	.driver		= {
		.name		= QPNP_BMS_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= qpnp_bms_match_table,
		.pm		= &qpnp_bms_pm_ops,
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
