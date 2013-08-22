/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/power_supply.h>
#include <linux/mfd/pm8xxx/pm8921-bms.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#include <linux/mfd/pm8xxx/ccadc.h>
#include <linux/mfd/pm8xxx/batt-alarm.h>
#include <linux/batterydata-lib.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/rtc.h>

#define BMS_CONTROL		0x224
#define BMS_S1_DELAY		0x225
#define BMS_OUTPUT0		0x230
#define BMS_OUTPUT1		0x231
#define BMS_TOLERANCES		0x232
#define BMS_TEST1		0x237

#define ADC_ARB_SECP_CNTRL	0x190
#define ADC_ARB_SECP_AMUX_CNTRL	0x191
#define ADC_ARB_SECP_ANA_PARAM	0x192
#define ADC_ARB_SECP_DIG_PARAM	0x193
#define ADC_ARB_SECP_RSV	0x194
#define ADC_ARB_SECP_DATA1	0x195
#define ADC_ARB_SECP_DATA0	0x196

#define ADC_ARB_BMS_CNTRL	0x18D
#define AMUX_TRIM_2		0x322
#define TEST_PROGRAM_REV	0x339

#define TEMP_SOC_STORAGE	0x107

#define TEMP_IAVG_STORAGE	0x105
#define TEMP_IAVG_STORAGE_USE_MASK	0x0F

#define PON_CNTRL_6		0x018
#define WD_BIT			BIT(7)

#define BATT_ALARM_ACCURACY	50	/* 50mV */

enum pmic_bms_interrupts {
	PM8921_BMS_SBI_WRITE_OK,
	PM8921_BMS_CC_THR,
	PM8921_BMS_VSENSE_THR,
	PM8921_BMS_VSENSE_FOR_R,
	PM8921_BMS_OCV_FOR_R,
	PM8921_BMS_GOOD_OCV,
	PM8921_BMS_VSENSE_AVG,
	PM_BMS_MAX_INTS,
};

struct pm8921_soc_params {
	uint16_t	last_good_ocv_raw;
	int		cc;

	int		last_good_ocv_uv;
};

struct fcc_data {
	int fcc_new;
	int chargecycles;
	int batt_temp;
	int fcc_real;
	int temp_real;
};

/**
 * struct pm8921_bms_chip -
 * @bms_output_lock:	lock to prevent concurrent bms reads
 *
 * @last_ocv_uv_mutex:	mutex to protect simultaneous invocations of calculate
 *			state of charge, note that last_ocv_uv could be
 *			changed as soc is adjusted. This mutex protects
 *			simultaneous updates of last_ocv_uv as well. This mutex
 *			also protects changes to *_at_100 variables used in
 *			faking 100% SOC.
 */
struct pm8921_bms_chip {
	struct device		*dev;
	struct dentry		*dent;
	int			r_sense_uohm;
	unsigned int		v_cutoff;
	unsigned int		fcc;
	struct single_row_lut	*fcc_temp_lut;
	struct single_row_lut	*fcc_sf_lut;
	struct pc_temp_ocv_lut	*pc_temp_ocv_lut;
	struct sf_lut		*pc_sf_lut;
	struct sf_lut		*rbatt_sf_lut;
	int			delta_rbatt_mohm;
	struct work_struct	calib_hkadc_work;
	unsigned long		last_calib_time;
	int			last_calib_temp;
	struct mutex		calib_mutex;
	unsigned int		revision;
	unsigned int		xoadc_v0625_usb_present;
	unsigned int		xoadc_v0625_usb_absent;
	unsigned int		xoadc_v0625;
	unsigned int		xoadc_v125;
	unsigned int		batt_temp_channel;
	unsigned int		vbat_channel;
	unsigned int		ref625mv_channel;
	unsigned int		ref1p25v_channel;
	unsigned int		batt_id_channel;
	unsigned int		pmic_bms_irq[PM_BMS_MAX_INTS];
	DECLARE_BITMAP(enabled_irqs, PM_BMS_MAX_INTS);
	struct mutex		bms_output_lock;
	struct single_row_lut	*adjusted_fcc_temp_lut;
	unsigned int		charging_began;
	unsigned int		start_percent;
	unsigned int		end_percent;
	unsigned int		alarm_low_mv;
	unsigned int		alarm_high_mv;

	int			charge_time_us;
	int			catch_up_time_us;
	enum battery_type	batt_type;
	uint16_t		ocv_reading_at_100;
	int			max_voltage_uv;

	int			chg_term_ua;
	int			default_rbatt_mohm;
	int			amux_2_trim_delta;
	uint16_t		prev_last_good_ocv_raw;
	int			rconn_mohm;
	int			rbatt_capacitive_mohm;
	struct mutex		last_ocv_uv_mutex;
	int			last_ocv_uv;
	int			last_ocv_temp_decidegc;
	int			pon_ocv_uv;
	int			last_cc_uah;
	unsigned long		tm_sec;

	int			enable_fcc_learning;
	int			min_fcc_learning_soc;
	int			min_fcc_ocv_pc;
	int			min_fcc_learning_samples;
	struct			fcc_data *fcc_table;
	int			fcc_new;
	int			start_real_soc;
	int			pc_at_start_charge;

	int			shutdown_soc;
	int			shutdown_iavg_ua;
	struct delayed_work	calculate_soc_delayed_work;
	struct timespec		t_soc_queried;
	unsigned long		last_recalc_time;
	int			shutdown_soc_valid_limit;
	int			ignore_shutdown_soc;
	int			prev_iavg_ua;
	int			prev_uuc_iavg_ma;
	int			prev_pc_unusable;
	int			adjust_soc_low_threshold;

	int			ibat_at_cv_ua;
	int			soc_at_cv;
	int			prev_chg_soc;
	struct power_supply	*batt_psy;
	struct wake_lock	low_voltage_wake_lock;
	int			soc_calc_period;
	int			normal_voltage_calc_ms;
	int			low_voltage_calc_ms;
	int			imax_ua;
	struct wake_lock	soc_wake_lock;
	int			disable_flat_portion_ocv;
	int			ocv_dis_high_soc;
	int			ocv_dis_low_soc;
	int			high_ocv_correction_limit_uv;
	int			low_ocv_correction_limit_uv;
	int			hold_soc_est;
	int			prev_vbat_batt_terminal_uv;
	int			vbatt_cutoff_count;
	int			low_voltage_detect;
	int			vbatt_cutoff_retries;
	bool			first_report_after_suspend;
	bool			soc_updated_on_resume;
	int			last_soc_at_suspend;
};

/*
 * protects against simultaneous adjustment of ocv based on shutdown soc and
 * invalidating the shutdown soc
 */
static DEFINE_MUTEX(soc_invalidation_mutex);
static int shutdown_soc_invalid;
static struct pm8921_bms_chip *the_chip;

#define DEFAULT_RBATT_MOHMS		128
#define DEFAULT_OCV_MICROVOLTS		3900000
#define DEFAULT_CHARGE_CYCLES		0

#define DELTA_FCC_PERCENT			5
#define MIN_START_PERCENT_FOR_LEARNING		20
#define MIN_START_OCV_PERCENT_FOR_LEARNING	30
#define MAX_FCC_LEARNING_COUNT			5
#define VALID_FCC_CHGCYL_RANGE			50

static int last_usb_cal_delta_uv = 1800;
module_param(last_usb_cal_delta_uv, int, 0644);

static int last_chargecycles = DEFAULT_CHARGE_CYCLES;
static int last_charge_increase;
static int last_fcc_update_count;
static int min_fcc_cycles = -EINVAL;
module_param(last_chargecycles, int, 0644);
module_param(last_charge_increase, int, 0644);
module_param(last_fcc_update_count, int, 0644);

static int calculated_soc = -EINVAL;
static int last_soc = -EINVAL;
static int last_real_fcc_mah = -EINVAL;
static int last_real_fcc_batt_temp = -EINVAL;
static int battery_removed;

static int pm8921_battery_gauge_alarm_notify(struct notifier_block *nb,
				unsigned long status, void *unused);

static struct notifier_block alarm_notifier = {
	.notifier_call = pm8921_battery_gauge_alarm_notify,
};

static int bms_ro_ops_set(const char *val, const struct kernel_param *kp)
{
	return -EINVAL;
}

static struct kernel_param_ops bms_param_ops = {
	.set = bms_ro_ops_set,
	.get = param_get_int,
};
/* Make last_soc as read only as it is already calculated from shutdown_soc */
module_param_cb(last_soc, &bms_param_ops, &last_soc, 0644);
module_param_cb(battery_removed, &bms_param_ops, &battery_removed, 0644);
module_param_cb(min_fcc_cycles, &bms_param_ops, &min_fcc_cycles, 0644);

/*
 * bms_fake_battery is set in setups where a battery emulator is used instead
 * of a real battery. This makes the bms driver report a different/fake value
 * regardless of the calculated state of charge.
 */
static int bms_fake_battery = -EINVAL;
module_param(bms_fake_battery, int, 0644);

/* bms_start_XXX and bms_end_XXX are read only */
static int bms_start_percent;
static int bms_start_ocv_uv;
static int bms_start_cc_uah;
static int bms_end_percent;
static int bms_end_ocv_uv;
static int bms_end_cc_uah;

static struct kernel_param_ops bms_ro_param_ops = {
	.set = bms_ro_ops_set,
	.get = param_get_int,
};
module_param_cb(bms_start_percent, &bms_ro_param_ops, &bms_start_percent, 0644);
module_param_cb(bms_start_ocv_uv, &bms_ro_param_ops, &bms_start_ocv_uv, 0644);
module_param_cb(bms_start_cc_uah, &bms_ro_param_ops, &bms_start_cc_uah, 0644);

module_param_cb(bms_end_percent, &bms_ro_param_ops, &bms_end_percent, 0644);
module_param_cb(bms_end_ocv_uv, &bms_ro_param_ops, &bms_end_ocv_uv, 0644);
module_param_cb(bms_end_cc_uah, &bms_ro_param_ops, &bms_end_cc_uah, 0644);

static void readjust_fcc_table(void)
{
	struct single_row_lut *temp, *old;
	int i, fcc, ratio;

	if (!the_chip->enable_fcc_learning || battery_removed)
		return;

	if (!the_chip->fcc_temp_lut) {
		pr_err("The static fcc lut table is NULL\n");
		return;
	}

	temp = kzalloc(sizeof(struct single_row_lut), GFP_KERNEL);
	if (!temp) {
		pr_err("Cannot allocate memory for adjusted fcc table\n");
		return;
	}

	fcc = interpolate_fcc(the_chip->fcc_temp_lut, last_real_fcc_batt_temp);

	temp->cols = the_chip->fcc_temp_lut->cols;
	for (i = 0; i < the_chip->fcc_temp_lut->cols; i++) {
		temp->x[i] = the_chip->fcc_temp_lut->x[i];
		ratio = div_u64(the_chip->fcc_temp_lut->y[i] * 1000, fcc);
		temp->y[i] =  (ratio * last_real_fcc_mah);
		temp->y[i] /= 1000;
		pr_debug("temp=%d, staticfcc=%d, adjfcc=%d, ratio=%d\n",
				temp->x[i], the_chip->fcc_temp_lut->y[i],
				temp->y[i], ratio);
	}

	old = the_chip->adjusted_fcc_temp_lut;
	the_chip->adjusted_fcc_temp_lut = temp;
	kfree(old);
}

static int bms_last_real_fcc_set(const char *val,
				const struct kernel_param *kp)
{
	int rc = 0;

	if (battery_removed)
		return rc;

	if (last_real_fcc_mah == -EINVAL)
		rc = param_set_int(val, kp);
	if (rc) {
		pr_err("Failed to set last_real_fcc_mah rc=%d\n", rc);
		return rc;
	}
	if (last_real_fcc_batt_temp != -EINVAL)
		readjust_fcc_table();
	return rc;
}
static struct kernel_param_ops bms_last_real_fcc_param_ops = {
	.set = bms_last_real_fcc_set,
	.get = param_get_int,
};
module_param_cb(last_real_fcc_mah, &bms_last_real_fcc_param_ops,
					&last_real_fcc_mah, 0644);

static int bms_last_real_fcc_batt_temp_set(const char *val,
				const struct kernel_param *kp)
{
	int rc = 0;

	if (battery_removed)
		return rc;

	if (last_real_fcc_batt_temp == -EINVAL)
		rc = param_set_int(val, kp);
	if (rc) {
		pr_err("Failed to set last_real_fcc_batt_temp rc=%d\n", rc);
		return rc;
	}
	if (last_real_fcc_mah != -EINVAL)
		readjust_fcc_table();
	return rc;
}

static struct kernel_param_ops bms_last_real_fcc_batt_temp_param_ops = {
	.set = bms_last_real_fcc_batt_temp_set,
	.get = param_get_int,
};
module_param_cb(last_real_fcc_batt_temp, &bms_last_real_fcc_batt_temp_param_ops,
					&last_real_fcc_batt_temp, 0644);

static int pm_bms_get_rt_status(struct pm8921_bms_chip *chip, int irq_id)
{
	return pm8xxx_read_irq_stat(chip->dev->parent,
					chip->pmic_bms_irq[irq_id]);
}

static void pm8921_bms_enable_irq(struct pm8921_bms_chip *chip, int interrupt)
{
	if (!__test_and_set_bit(interrupt, chip->enabled_irqs)) {
		dev_dbg(chip->dev, "%s %d\n", __func__,
						chip->pmic_bms_irq[interrupt]);
		enable_irq(chip->pmic_bms_irq[interrupt]);
	}
}

static void pm8921_bms_disable_irq(struct pm8921_bms_chip *chip, int interrupt)
{
	if (__test_and_clear_bit(interrupt, chip->enabled_irqs)) {
		pr_debug("%d\n", chip->pmic_bms_irq[interrupt]);
		disable_irq_nosync(chip->pmic_bms_irq[interrupt]);
	}
}

static int pm_bms_masked_write(struct pm8921_bms_chip *chip, u16 addr,
							u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = pm8xxx_readb(chip->dev->parent, addr, &reg);
	if (rc) {
		pr_err("read failed addr = %03X, rc = %d\n", addr, rc);
		return rc;
	}
	reg &= ~mask;
	reg |= val & mask;
	rc = pm8xxx_writeb(chip->dev->parent, addr, reg);
	if (rc) {
		pr_err("write failed addr = %03X, rc = %d\n", addr, rc);
		return rc;
	}
	return 0;
}

static int usb_chg_plugged_in(struct pm8921_bms_chip *chip)
{
	int val = pm8921_is_usb_chg_plugged_in();

	/* if the charger driver was not initialized, use the restart reason */
	if (val == -EINVAL) {
		if (pm8xxx_restart_reason(chip->dev->parent)
				== PM8XXX_RESTART_CHG)
			val = 1;
		else
			val = 0;
	}

	return val;
}

static void pm8921_bms_low_voltage_config(struct pm8921_bms_chip *chip,
								int time_ms)
{
	int ms = 0;

	/* if work was pending and was cancelled, calculate SOC immediately */
	if (!cancel_delayed_work_sync(&chip->calculate_soc_delayed_work))
		ms = time_ms;

	chip->soc_calc_period = time_ms;
	schedule_delayed_work(&chip->calculate_soc_delayed_work,
						msecs_to_jiffies(ms));
}

static int pm8921_bms_enable_batt_alarm(struct pm8921_bms_chip *chip)
{
	int rc = 0;

	rc = pm8xxx_batt_alarm_enable(PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
	if (!rc)
		rc = pm8xxx_batt_alarm_disable(
				PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
	if (rc) {
		pr_err("unable to set batt alarm state rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int pm8921_bms_configure_batt_alarm(struct pm8921_bms_chip *chip)
{
	int rc = 0;

	rc = pm8xxx_batt_alarm_disable(PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
	if (!rc)
		rc = pm8xxx_batt_alarm_disable(
			PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
	if (rc) {
		pr_err("unable to set batt alarm state rc=%d\n", rc);
		return rc;
	}

	/*
	* The batt-alarm driver requires sane values for both min / max,
	* regardless of whether they're both activated.
	*/
	rc = pm8xxx_batt_alarm_threshold_set(
			PM8XXX_BATT_ALARM_LOWER_COMPARATOR,
					chip->alarm_low_mv);
	if (!rc)
		rc = pm8xxx_batt_alarm_threshold_set(
			PM8XXX_BATT_ALARM_UPPER_COMPARATOR,
					chip->alarm_high_mv);
	if (rc) {
		pr_err("unable to set batt alarm threshold rc=%d\n", rc);
		return rc;
	}

	rc = pm8xxx_batt_alarm_hold_time_set(
			PM8XXX_BATT_ALARM_HOLD_TIME_16_MS);
	if (rc) {
		pr_err("unable to set batt alarm hold time rc=%d\n", rc);
		return rc;
	}

	/* PWM enabled at 2Hz */
	rc = pm8xxx_batt_alarm_pwm_rate_set(1, 7, 4);
	if (rc) {
		pr_err("unable to set batt alarm pwm rate rc=%d\n", rc);
		return rc;
	}

	rc = pm8xxx_batt_alarm_register_notifier(&alarm_notifier);
	if (rc) {
		pr_err("unable to register alarm notifier rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int pm8921_battery_gauge_alarm_notify(struct notifier_block *nb,
		unsigned long status, void *unused)
{
	int rc;

	if (!the_chip) {
		pr_err("not initialized\n");
		return -EINVAL;
	}

	switch (status) {
	case 0:
		pr_debug("spurious interrupt\n");
		break;
	case 1:
		pr_debug("Low voltage alarm triggered\n");
		/*
		 * hold the low voltage wakelock until the soc
		 * work finds it appropriate to release it.
		 */
		if (!wake_lock_active(&the_chip->low_voltage_wake_lock)) {
			pr_debug("Holding low voltage wakelock\n");
			wake_lock(&the_chip->low_voltage_wake_lock);
			pm8921_bms_low_voltage_config(the_chip,
					the_chip->low_voltage_calc_ms);
		}

		rc = pm8xxx_batt_alarm_disable(
				PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
		if (!rc)
			rc = pm8xxx_batt_alarm_enable(
				PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
		if (rc)
			pr_err("unable to set alarm state rc=%d\n", rc);
		break;
	case 2:
		rc = pm8xxx_batt_alarm_disable(
			PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
		if (!rc)
			rc = pm8xxx_batt_alarm_enable(
				PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
		if (rc)
			pr_err("unable to set alarm state rc=%d\n", rc);

		break;
	default:
		pr_err("error received\n");
		break;
	}

	return 0;
};


#define HOLD_OREG_DATA		BIT(1)
static int pm_bms_lock_output_data(struct pm8921_bms_chip *chip)
{
	int rc;

	rc = pm_bms_masked_write(chip, BMS_CONTROL, HOLD_OREG_DATA,
					HOLD_OREG_DATA);
	if (rc) {
		pr_err("couldnt lock bms output rc = %d\n", rc);
		return rc;
	}
	return 0;
}

static int pm_bms_unlock_output_data(struct pm8921_bms_chip *chip)
{
	int rc;

	rc = pm_bms_masked_write(chip, BMS_CONTROL, HOLD_OREG_DATA, 0);
	if (rc) {
		pr_err("fail to unlock BMS_CONTROL rc = %d\n", rc);
		return rc;
	}
	return 0;
}

#define SELECT_OUTPUT_DATA	0x1C
#define SELECT_OUTPUT_TYPE_SHIFT	2
#define OCV_FOR_RBATT		0x0
#define VSENSE_FOR_RBATT	0x1
#define VBATT_FOR_RBATT		0x2
#define CC_MSB			0x3
#define CC_LSB			0x4
#define LAST_GOOD_OCV_VALUE	0x5
#define VSENSE_AVG		0x6
#define VBATT_AVG		0x7

static int pm_bms_read_output_data(struct pm8921_bms_chip *chip, int type,
						int16_t *result)
{
	int rc;
	u8 reg;

	if (!result) {
		pr_err("result pointer null\n");
		return -EINVAL;
	}
	*result = 0;
	if (type < OCV_FOR_RBATT || type > VBATT_AVG) {
		pr_err("invalid type %d asked to read\n", type);
		return -EINVAL;
	}

	rc = pm_bms_masked_write(chip, BMS_CONTROL, SELECT_OUTPUT_DATA,
					type << SELECT_OUTPUT_TYPE_SHIFT);
	if (rc) {
		pr_err("fail to select %d type in BMS_CONTROL rc = %d\n",
						type, rc);
		return rc;
	}

	rc = pm8xxx_readb(chip->dev->parent, BMS_OUTPUT0, &reg);
	if (rc) {
		pr_err("fail to read BMS_OUTPUT0 for type %d rc = %d\n",
			type, rc);
		return rc;
	}
	*result = reg;
	rc = pm8xxx_readb(chip->dev->parent, BMS_OUTPUT1, &reg);
	if (rc) {
		pr_err("fail to read BMS_OUTPUT1 for type %d rc = %d\n",
			type, rc);
		return rc;
	}
	*result |= reg << 8;
	pr_debug("type %d result %x", type, *result);
	return 0;
}

#define V_PER_BIT_MUL_FACTOR	97656
#define V_PER_BIT_DIV_FACTOR	1000
#define XOADC_INTRINSIC_OFFSET	0x6000
static int xoadc_reading_to_microvolt(unsigned int a)
{
	if (a <= XOADC_INTRINSIC_OFFSET)
		return 0;

	return (a - XOADC_INTRINSIC_OFFSET)
			* V_PER_BIT_MUL_FACTOR / V_PER_BIT_DIV_FACTOR;
}

#define XOADC_CALIB_UV		625000
#define VBATT_MUL_FACTOR	3
static int adjust_xo_vbatt_reading(struct pm8921_bms_chip *chip,
					int usb_chg, unsigned int uv)
{
	s64 numerator, denominator;
	int local_delta;

	if (uv == 0)
		return 0;

	/* dont adjust if not calibrated */
	if (chip->xoadc_v0625 == 0 || chip->xoadc_v125 == 0) {
		pr_debug("No cal yet return %d\n", VBATT_MUL_FACTOR * uv);
		return VBATT_MUL_FACTOR * uv;
	}

	if (usb_chg)
		local_delta = last_usb_cal_delta_uv;
	else
		local_delta = 0;

	pr_debug("using delta = %d\n", local_delta);
	numerator = ((s64)uv - chip->xoadc_v0625 - local_delta)
							* XOADC_CALIB_UV;
	denominator =  (s64)chip->xoadc_v125 - chip->xoadc_v0625 - local_delta;
	if (denominator == 0)
		return uv * VBATT_MUL_FACTOR;
	return (XOADC_CALIB_UV + local_delta + div_s64(numerator, denominator))
						* VBATT_MUL_FACTOR;
}

#define CC_RESOLUTION_N		868056
#define CC_RESOLUTION_D		10000

static s64 cc_to_microvolt(struct pm8921_bms_chip *chip, s64 cc)
{
	return div_s64(cc * CC_RESOLUTION_N, CC_RESOLUTION_D);
}

#define CC_READING_TICKS	56
#define SLEEP_CLK_HZ		32764
#define SECONDS_PER_HOUR	3600
/**
 * ccmicrovolt_to_pvh -
 * @cc_uv:  coulumb counter converted to uV
 *
 * RETURNS:	coulumb counter based charge in pVh
 *		(pico Volt Hour)
 */
static s64 ccmicrovolt_to_pvh(s64 cc_uv)
{
	return div_s64(cc_uv * CC_READING_TICKS * 1000000L,
			SLEEP_CLK_HZ * SECONDS_PER_HOUR);
}

/* returns the signed value read from the hardware */
static int read_cc(struct pm8921_bms_chip *chip, int *result)
{
	int rc;
	uint16_t msw, lsw;

	*result = 0;
	rc = pm_bms_read_output_data(chip, CC_LSB, &lsw);
	if (rc) {
		pr_err("fail to read CC_LSB rc = %d\n", rc);
		return rc;
	}
	rc = pm_bms_read_output_data(chip, CC_MSB, &msw);
	if (rc) {
		pr_err("fail to read CC_MSB rc = %d\n", rc);
		return rc;
	}
	*result = msw << 16 | lsw;
	pr_debug("msw = %04x lsw = %04x cc = %d\n", msw, lsw, *result);
	return 0;
}

static int adjust_xo_vbatt_reading_for_mbg(struct pm8921_bms_chip *chip,
						int result)
{
	int64_t numerator;
	int64_t denominator;

	if (chip->amux_2_trim_delta == 0)
		return result;

	numerator = (s64)result * 1000000;
	denominator = (1000000 + (410 * (s64)chip->amux_2_trim_delta));
	return div_s64(numerator, denominator);
}

static int convert_vbatt_raw_to_uv(struct pm8921_bms_chip *chip,
					int usb_chg,
					uint16_t reading, int *result)
{
	*result = xoadc_reading_to_microvolt(reading);
	pr_debug("raw = %04x vbatt = %u\n", reading, *result);
	*result = adjust_xo_vbatt_reading(chip, usb_chg, *result);
	pr_debug("after adj vbatt = %u\n", *result);
	*result = adjust_xo_vbatt_reading_for_mbg(chip, *result);
	return 0;
}

static int convert_vsense_to_uv(struct pm8921_bms_chip *chip,
					int16_t reading, int *result)
{
	*result = pm8xxx_ccadc_reading_to_microvolt(chip->revision, reading);
	pr_debug("raw = %04x vsense = %d\n", reading, *result);
	*result = pm8xxx_cc_adjust_for_gain(*result);
	pr_debug("after adj vsense = %d\n", *result);
	return 0;
}

static int read_vsense_avg(struct pm8921_bms_chip *chip, int *result)
{
	int rc;
	int16_t reading;

	rc = pm_bms_read_output_data(chip, VSENSE_AVG, &reading);
	if (rc) {
		pr_err("fail to read VSENSE_AVG rc = %d\n", rc);
		return rc;
	}

	convert_vsense_to_uv(chip, reading, result);
	return 0;
}

static int get_batt_temp(struct pm8921_bms_chip *chip, int *batt_temp)
{
	int rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(chip->batt_temp_channel, &result);
	if (rc) {
		pr_err("error reading batt_temp_channel = %d, rc = %d\n",
					chip->batt_temp_channel, rc);
		return rc;
	}
	*batt_temp = result.physical;
	pr_debug("batt_temp phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	return 0;
}

#define BMS_MODE_BIT	BIT(6)
#define EN_VBAT_BIT	BIT(5)
#define OVERRIDE_MODE_DELAY_MS	20
int override_mode_simultaneous_battery_voltage_and_current(int *ibat_ua,
								int *vbat_uv)
{
	int16_t vsense_raw;
	int16_t vbat_raw;
	int vsense_uv;
	int usb_chg;
	int batt_temp;

	mutex_lock(&the_chip->bms_output_lock);

	pm8xxx_writeb(the_chip->dev->parent, BMS_S1_DELAY, 0x00);
	pm_bms_masked_write(the_chip, BMS_CONTROL,
			BMS_MODE_BIT | EN_VBAT_BIT, BMS_MODE_BIT | EN_VBAT_BIT);

	msleep(OVERRIDE_MODE_DELAY_MS);

	pm_bms_lock_output_data(the_chip);
	pm_bms_read_output_data(the_chip, VSENSE_AVG, &vsense_raw);
	pm_bms_read_output_data(the_chip, VBATT_AVG, &vbat_raw);
	pm_bms_unlock_output_data(the_chip);
	pm_bms_masked_write(the_chip, BMS_CONTROL,
		BMS_MODE_BIT | EN_VBAT_BIT, 0);

	pm8xxx_writeb(the_chip->dev->parent, BMS_S1_DELAY, 0x0B);

	mutex_unlock(&the_chip->bms_output_lock);

	get_batt_temp(the_chip, &batt_temp);
	usb_chg = usb_chg_plugged_in(the_chip);

	convert_vbatt_raw_to_uv(the_chip, usb_chg, vbat_raw, vbat_uv);
	convert_vsense_to_uv(the_chip, vsense_raw, &vsense_uv);
	*ibat_ua = div_s64((s64)vsense_uv * 1000000LL, the_chip->r_sense_uohm);

	pr_debug("vsense_raw = 0x%x vbat_raw = 0x%x"
			" ibat_ua = %d vbat_uv = %d\n",
			(uint16_t)vsense_raw, (uint16_t)vbat_raw,
			*ibat_ua, *vbat_uv);
	return 0;
}

#define MBG_TRANSIENT_ERROR_UV 15000
static void adjust_pon_ocv(struct pm8921_bms_chip *chip, int *uv)
{
	/*
	 * In 8921 parts the PON ocv is taken when the MBG is not settled.
	 * decrease the pon ocv by 15mV raw value to account for it
	 * Since a 1/3rd  of vbatt is supplied to the adc the raw value
	 * needs to be adjusted by 5mV worth bits
	 */
	if (*uv >= MBG_TRANSIENT_ERROR_UV)
		*uv -= MBG_TRANSIENT_ERROR_UV;
}

#define SEL_ALT_OREG_BIT  BIT(2)
static int ocv_ir_compensation(struct pm8921_bms_chip *chip, int ocv)
{
	int compensated_ocv;
	int ibatt_ua;
	int rbatt_mohm = chip->default_rbatt_mohm + chip->rconn_mohm;

	pm_bms_masked_write(chip, BMS_TEST1,
			SEL_ALT_OREG_BIT, SEL_ALT_OREG_BIT);

	/* since the SEL_ALT_OREG_BIT is set this will give us VSENSE_OCV */
	pm8921_bms_get_battery_current(&ibatt_ua);
	compensated_ocv = ocv + div_s64((s64)ibatt_ua * rbatt_mohm, 1000);
	pr_debug("comp ocv = %d, ocv = %d, ibatt_ua = %d, rbatt_mohm = %d\n",
			compensated_ocv, ocv, ibatt_ua, rbatt_mohm);

	pm_bms_masked_write(chip, BMS_TEST1, SEL_ALT_OREG_BIT, 0);
	return compensated_ocv;
}

#define RESET_CC_BIT BIT(3)
static int reset_cc(struct pm8921_bms_chip *chip)
{
	int rc;

	rc = pm_bms_masked_write(chip, BMS_TEST1, RESET_CC_BIT, RESET_CC_BIT);
	if (rc < 0) {
		pr_err("err setting cc reset rc = %d\n", rc);
		return rc;
	}

	/* sleep 100uS for the coulomb counter to reset */
	udelay(100);

	rc = pm_bms_masked_write(chip, BMS_TEST1, RESET_CC_BIT, 0);
	if (rc < 0)
		pr_err("err clearing cc reset rc = %d\n", rc);
	return rc;
}

static int estimate_ocv(struct pm8921_bms_chip *chip)
{
	int ibat_ua, vbat_uv, ocv_est_uv;
	int rc;
	int rbatt_mohm = chip->default_rbatt_mohm + chip->rconn_mohm
				+ chip->rbatt_capacitive_mohm;

	rc = pm8921_bms_get_simultaneous_battery_voltage_and_current(
							&ibat_ua,
							&vbat_uv);
	if (rc) {
		pr_err("simultaneous failed rc = %d\n", rc);
		return rc;
	}

	ocv_est_uv = vbat_uv + (ibat_ua * rbatt_mohm) / 1000;
	pr_debug("estimated pon ocv = %d\n", ocv_est_uv);
	return ocv_est_uv;
}

static bool is_warm_restart(struct pm8921_bms_chip *chip)
{
	u8 reg;
	int rc;

	rc = pm8xxx_readb(chip->dev->parent, PON_CNTRL_6, &reg);
	if (rc) {
		pr_err("err reading pon 6 rc = %d\n", rc);
		return false;
	}
	return reg & WD_BIT;
}

#define IBAT_TOL_MASK		0x0F
#define OCV_TOL_MASK			0xF0
#define IBAT_TOL_DEFAULT	0x03
#define IBAT_TOL_NOCHG		0x0F
#define OCV_TOL_DEFAULT		0x20
#define OCV_TOL_NO_OCV		0x00
static int pm8921_bms_stop_ocv_updates(void)
{
	if (!the_chip) {
		pr_err("BMS driver has not been initialized yet!\n");
		return -EINVAL;
	}
	pr_debug("stopping ocv updates\n");
	return pm_bms_masked_write(the_chip, BMS_TOLERANCES,
			OCV_TOL_MASK, OCV_TOL_NO_OCV);
}

static int pm8921_bms_start_ocv_updates(void)
{
	if (!the_chip) {
		pr_err("BMS driver has not been initialized yet!\n");
		return -EINVAL;
	}
	pr_debug("starting ocv updates\n");
	return pm_bms_masked_write(the_chip, BMS_TOLERANCES,
			OCV_TOL_MASK, OCV_TOL_DEFAULT);
}

static int reset_bms_for_test(void)
{
	int ibat_ua, vbat_uv, rc;
	int ocv_est_uv;

	if (!the_chip) {
		pr_err("BMS driver has not been initialized yet!\n");
		return -EINVAL;
	}

	rc = pm8921_bms_get_simultaneous_battery_voltage_and_current(
							&ibat_ua,
							&vbat_uv);
	/*
	 * don't include rbatt and rbatt_capacitve since we expect this to
	 * be used with a fake battery which does not have internal resistnaces
	 */
	ocv_est_uv = vbat_uv + (ibat_ua * the_chip->rconn_mohm) / 1000;
	pr_debug("forcing ocv to be %d due to bms reset mode\n", ocv_est_uv);
	the_chip->last_ocv_uv = ocv_est_uv;
	last_soc = -EINVAL;
	reset_cc(the_chip);
	the_chip->last_cc_uah = 0;
	pm8921_bms_stop_ocv_updates();

	pr_debug("bms reset to ocv = %duv vbat_ua = %d ibat_ua = %d\n",
			the_chip->last_ocv_uv, vbat_uv, ibat_ua);

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

	if (*val == 'Y') {
		rc = reset_bms_for_test();
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

static bool bms_reset;
module_param_cb(bms_reset, &bms_reset_ops, &bms_reset, 0644);
/*
 * This reflects what should the CC readings should be for
 * a 5mAh discharge. This value is dependent on
 * CC_RESOLUTION_N, CC_RESOLUTION_D, CC_READING_TICKS
 * and rsense
 */
#define CC_RAW_5MAH		0x00110000
#define MIN_OCV_UV		2000000
#define OCV_RAW_UNINITIALIZED	0xFFFF
static int read_soc_params_raw(struct pm8921_bms_chip *chip,
				struct pm8921_soc_params *raw,
				int batt_temp_decidegc)
{
	int usb_chg;
	int est_ocv_uv;

	mutex_lock(&chip->bms_output_lock);
	pm_bms_lock_output_data(chip);

	pm_bms_read_output_data(chip,
			LAST_GOOD_OCV_VALUE, &raw->last_good_ocv_raw);
	read_cc(chip, &raw->cc);

	pm_bms_unlock_output_data(chip);
	mutex_unlock(&chip->bms_output_lock);

	usb_chg =  usb_chg_plugged_in(chip);

	if (chip->prev_last_good_ocv_raw == OCV_RAW_UNINITIALIZED) {
		chip->prev_last_good_ocv_raw = raw->last_good_ocv_raw;

		convert_vbatt_raw_to_uv(chip, usb_chg,
			raw->last_good_ocv_raw, &raw->last_good_ocv_uv);
		adjust_pon_ocv(chip, &raw->last_good_ocv_uv);
		raw->last_good_ocv_uv = ocv_ir_compensation(chip,
						raw->last_good_ocv_uv);
		chip->last_ocv_uv = raw->last_good_ocv_uv;

		if (is_warm_restart(chip)
			|| raw->cc > CC_RAW_5MAH
			|| (raw->last_good_ocv_uv < MIN_OCV_UV
			&& raw->cc > 0)) {
			/*
			 * The CC value is higher than 5mAh.
			 * The phone started without going through a pon
			 * sequence
			 * OR
			 * The ocv was very small and there was no
			 * charging in the bootloader
			 * - reset the CC and take an ocv again
			 */
			pr_debug("cc_raw = 0x%x may be > 5mAh(0x%x)\n",
				       raw->cc,	CC_RAW_5MAH);
			pr_debug("ocv_uv = %d ocv_raw = 0x%x may be < 2V\n",
				       chip->last_ocv_uv,
				       raw->last_good_ocv_raw);
			est_ocv_uv = estimate_ocv(chip);
			if (est_ocv_uv > 0) {
				raw->last_good_ocv_uv = est_ocv_uv;
				chip->last_ocv_uv = est_ocv_uv;
				reset_cc(chip);
				raw->cc = 0;
			}
		}
		chip->last_ocv_temp_decidegc = batt_temp_decidegc;
		pr_debug("PON_OCV_UV = %d\n", chip->last_ocv_uv);
	} else if (chip->prev_last_good_ocv_raw != raw->last_good_ocv_raw) {
		chip->prev_last_good_ocv_raw = raw->last_good_ocv_raw;
		convert_vbatt_raw_to_uv(chip, usb_chg,
			raw->last_good_ocv_raw, &raw->last_good_ocv_uv);
		chip->last_ocv_uv = raw->last_good_ocv_uv;
		chip->last_ocv_temp_decidegc = batt_temp_decidegc;
		/* forget the old cc value upon ocv */
		chip->last_cc_uah = 0;
	} else {
		raw->last_good_ocv_uv = chip->last_ocv_uv;
	}

	/* stop faking 100% after an OCV event */
	if (chip->ocv_reading_at_100 != raw->last_good_ocv_raw)
		chip->ocv_reading_at_100 = OCV_RAW_UNINITIALIZED;
	pr_debug("0p625 = %duV\n", chip->xoadc_v0625);
	pr_debug("1p25 = %duV\n", chip->xoadc_v125);
	pr_debug("last_good_ocv_raw= 0x%x, last_good_ocv_uv= %duV\n",
			raw->last_good_ocv_raw, raw->last_good_ocv_uv);
	pr_debug("cc_raw= 0x%x\n", raw->cc);
	return 0;
}

static int get_rbatt(struct pm8921_bms_chip *chip, int soc_rbatt, int batt_temp)
{
	int rbatt, scalefactor;

	rbatt = chip->default_rbatt_mohm;
	pr_debug("rbatt before scaling = %d\n", rbatt);
	if (chip->rbatt_sf_lut == NULL)  {
		pr_debug("RBATT = %d\n", rbatt);
		return rbatt;
	}
	/* Convert the batt_temp to DegC from deciDegC */
	batt_temp = batt_temp / 10;
	scalefactor = interpolate_scalingfactor(chip->rbatt_sf_lut,
							batt_temp, soc_rbatt);
	pr_debug("rbatt sf = %d for batt_temp = %d, soc_rbatt = %d\n",
				scalefactor, batt_temp, soc_rbatt);
	rbatt = (rbatt * scalefactor) / 100;

	rbatt += the_chip->rconn_mohm;
	pr_debug("adding rconn_mohm = %d rbatt = %d\n",
				the_chip->rconn_mohm, rbatt);

	rbatt += the_chip->rbatt_capacitive_mohm;
	pr_debug("adding rbatt_capacitive_mohm = %d rbatt = %d\n",
				the_chip->rbatt_capacitive_mohm, rbatt);

	if (is_between(20, 10, soc_rbatt))
		rbatt = rbatt
			+ ((20 - soc_rbatt) * chip->delta_rbatt_mohm) / 10;
	else
		if (is_between(10, 0, soc_rbatt))
			rbatt = rbatt + chip->delta_rbatt_mohm;

	pr_debug("RBATT = %d\n", rbatt);
	return rbatt;
}

static int calculate_fcc_uah(struct pm8921_bms_chip *chip, int batt_temp,
							int chargecycles)
{
	int initfcc, result, scalefactor = 0;

	if (chip->adjusted_fcc_temp_lut == NULL) {
		initfcc = interpolate_fcc(chip->fcc_temp_lut, batt_temp);

		scalefactor = interpolate_scalingfactor_fcc(chip->fcc_sf_lut,
				chargecycles);

		/* Multiply the initial FCC value by the scale factor. */
		result = (initfcc * scalefactor * 1000) / 100;
		pr_debug("fcc = %d uAh\n", result);
		return result;
	} else {
		return 1000 * interpolate_fcc(chip->adjusted_fcc_temp_lut,
				batt_temp);
	}
}

static int get_battery_uvolts(struct pm8921_bms_chip *chip, int *uvolts)
{
	int rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(chip->vbat_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					chip->vbat_channel, rc);
		return rc;
	}
	pr_debug("mvolts phy = %lld meas = 0x%llx", result.physical,
						result.measurement);
	*uvolts = (int)result.physical;
	return 0;
}

static int adc_based_ocv(struct pm8921_bms_chip *chip, int *ocv)
{
	int vbatt, rbatt, ibatt_ua, rc;

	rc = get_battery_uvolts(chip, &vbatt);
	if (rc) {
		pr_err("failed to read vbatt from adc rc = %d\n", rc);
		return rc;
	}

	rc =  pm8921_bms_get_battery_current(&ibatt_ua);
	if (rc) {
		pr_err("failed to read batt current rc = %d\n", rc);
		return rc;
	}

	rbatt = chip->default_rbatt_mohm;
	*ocv = vbatt + (ibatt_ua * rbatt)/1000;
	return 0;
}

static int calculate_pc(struct pm8921_bms_chip *chip, int ocv_uv,
				int batt_temp_decidegc, int chargecycles)
{
	int pc, scalefactor;

	pc = interpolate_pc(chip->pc_temp_ocv_lut,
			batt_temp_decidegc / 10, ocv_uv / 1000);
	pr_debug("pc = %u for ocv = %dmicroVolts batt_temp = %d\n",
					pc, ocv_uv, batt_temp_decidegc);

	scalefactor = interpolate_scalingfactor(chip->pc_sf_lut,
			chargecycles, pc);
	pr_debug("scalefactor = %u batt_temp = %d\n",
					scalefactor, batt_temp_decidegc);

	/* Multiply the initial FCC value by the scale factor. */
	pc = (pc * scalefactor) / 100;
	return pc;
}

/**
 * calculate_cc_uah -
 * @chip:		the bms chip pointer
 * @cc:			the cc reading from bms h/w
 * @val:		return value
 * @coulumb_counter:	adjusted coulumb counter for 100%
 *
 * RETURNS: in val pointer coulumb counter based charger in uAh
 *          (micro Amp hour)
 */
static void calculate_cc_uah(struct pm8921_bms_chip *chip, int cc, int *val)
{
	int64_t cc_voltage_uv, cc_pvh, cc_uah;

	cc_voltage_uv = cc;
	pr_debug("cc = %d\n", cc);
	cc_voltage_uv = cc_to_microvolt(chip, cc_voltage_uv);
	cc_voltage_uv = pm8xxx_cc_adjust_for_gain(cc_voltage_uv);
	pr_debug("cc_voltage_uv = %lld microvolts\n", cc_voltage_uv);
	cc_pvh = ccmicrovolt_to_pvh(cc_voltage_uv);
	pr_debug("cc_pvh = %lld pico_volt_hour\n", cc_pvh);
	cc_uah = div_s64(cc_pvh, chip->r_sense_uohm);
	*val = cc_uah;
}

int pm8921_bms_cc_uah(int *cc_uah)
{
	int cc;

	*cc_uah = 0;

	if (!the_chip)
		return -EINVAL;

	read_cc(the_chip, &cc);
	calculate_cc_uah(the_chip, cc, cc_uah);

	return 0;
}
EXPORT_SYMBOL(pm8921_bms_cc_uah);

static int calculate_termination_uuc(struct pm8921_bms_chip *chip,
				 int batt_temp, int chargecycles,
				int fcc_uah, int i_ma,
				int *ret_pc_unusable)
{
	int unusable_uv, pc_unusable, uuc;
	int i = 0;
	int ocv_mv;
	int batt_temp_degc = batt_temp / 10;
	int rbatt_mohm;
	int delta_uv;
	int prev_delta_uv = 0;
	int prev_rbatt_mohm = 0;
	int prev_ocv_mv = 0;
	int uuc_rbatt_uv;

	for (i = 0; i <= 100; i++) {
		ocv_mv = interpolate_ocv(chip->pc_temp_ocv_lut,
				batt_temp_degc, i);
		rbatt_mohm = get_rbatt(chip, i, batt_temp);
		unusable_uv = (rbatt_mohm * i_ma) + (chip->v_cutoff * 1000);
		delta_uv = ocv_mv * 1000 - unusable_uv;

		pr_debug("soc = %d ocv = %d rbat = %d u_uv = %d delta_v = %d\n",
				i, ocv_mv, rbatt_mohm, unusable_uv, delta_uv);

		if (delta_uv > 0)
			break;

		prev_delta_uv = delta_uv;
		prev_rbatt_mohm = rbatt_mohm;
		prev_ocv_mv = ocv_mv;
	}

	uuc_rbatt_uv = linear_interpolate(rbatt_mohm, delta_uv,
					prev_rbatt_mohm, prev_delta_uv,
					0);

	unusable_uv = (uuc_rbatt_uv * i_ma) + (chip->v_cutoff * 1000);

	pc_unusable = calculate_pc(chip, unusable_uv, batt_temp, chargecycles);
	uuc = (fcc_uah * pc_unusable) / 100;
	pr_debug("For i_ma = %d, unusable_rbatt = %d unusable_uv = %d unusable_pc = %d uuc = %d\n",
					i_ma, uuc_rbatt_uv, unusable_uv,
					pc_unusable, uuc);
	*ret_pc_unusable = pc_unusable;
	return uuc;
}

#define TIME_PER_PERCENT_UUC			60
static int adjust_uuc(struct pm8921_bms_chip *chip, int fcc_uah,
			int new_pc_unusable,
			int new_uuc,
			int batt_temp,
			int rbatt,
			int *iavg_ma,
			int delta_time_s)
{
	int new_unusable_mv;
	int batt_temp_degc = batt_temp / 10;
	int max_percent_change;

	max_percent_change = max(delta_time_s / TIME_PER_PERCENT_UUC, 1);

	if (chip->prev_pc_unusable == -EINVAL
		|| abs(chip->prev_pc_unusable - new_pc_unusable)
			<= max_percent_change) {
		chip->prev_pc_unusable = new_pc_unusable;
		return new_uuc;
	}

	/* the uuc is trying to change more than 1% restrict it */
	if (new_pc_unusable > chip->prev_pc_unusable)
		chip->prev_pc_unusable += max_percent_change;
	else
		chip->prev_pc_unusable -= max_percent_change;
	chip->prev_pc_unusable = clamp(chip->prev_pc_unusable, 0, 100);
	new_uuc = (fcc_uah * chip->prev_pc_unusable) / 100;

	/* also find update the iavg_ma accordingly */
	new_unusable_mv = interpolate_ocv(chip->pc_temp_ocv_lut,
			batt_temp_degc, chip->prev_pc_unusable);
	if (new_unusable_mv < chip->v_cutoff)
		new_unusable_mv = chip->v_cutoff;

	*iavg_ma = (new_unusable_mv - chip->v_cutoff) * 1000 / rbatt;
	if (*iavg_ma == 0)
		*iavg_ma = 1;
	pr_debug("Restricting UUC to %d (%d%%) unusable_mv = %d iavg_ma = %d\n",
					new_uuc, chip->prev_pc_unusable,
					new_unusable_mv, *iavg_ma);

	return new_uuc;
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
		return rc;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		return rc;
	}
	rtc_tm_to_time(&tm, now_tm_sec);

	return 0;
}

static int calculate_delta_time(struct pm8921_bms_chip *chip, int *delta_time_s)
{
	unsigned long now_tm_sec = 0;

	/* default to delta time = 0 if anything fails */
	*delta_time_s = 0;

	get_current_time(&now_tm_sec);

	*delta_time_s = (now_tm_sec - chip->tm_sec);
	pr_debug("tm_sec = %ld, now_tm_sec = %ld delta_s = %d\n",
		chip->tm_sec, now_tm_sec, *delta_time_s);

	/* remember this time */
	chip->tm_sec = now_tm_sec;
	return 0;
}

static void calculate_iavg_ua(struct pm8921_bms_chip *chip, int cc_uah,
				int *iavg_ua, int delta_time_s)
{
	int delta_cc_uah = 0;

	/* if anything fails report the previous iavg_ua */
	*iavg_ua = chip->prev_iavg_ua;

	if (chip->last_cc_uah == INT_MIN) {
		pm8921_bms_get_battery_current(iavg_ua);
		goto out;
	}

	/* use the previous iavg if called within 15 seconds */
	if (delta_time_s < 15) {
		*iavg_ua = chip->prev_iavg_ua;
		goto out;
	}

	delta_cc_uah = cc_uah - chip->last_cc_uah;

	*iavg_ua = div_s64((s64)delta_cc_uah * 3600, delta_time_s);

out:
	pr_debug("delta_cc = %d iavg_ua = %d\n", delta_cc_uah, (int)*iavg_ua);
	/* remember the iavg */
	chip->prev_iavg_ua = *iavg_ua;

	/* remember cc_uah */
	chip->last_cc_uah = cc_uah;
}

#define IAVG_SAMPLES 16
#define MIN_IAVG_MA 250
#define MIN_SECONDS_FOR_VALID_SAMPLE	20
static int calculate_unusable_charge_uah(struct pm8921_bms_chip *chip,
				int rbatt, int fcc_uah, int cc_uah,
				int soc_rbatt, int batt_temp, int chargecycles,
				int iavg_ua, int delta_time_s)
{
	int uuc_uah_iavg;
	int i;
	int iavg_ma = iavg_ua / 1000;
	static int iavg_samples[IAVG_SAMPLES];
	static int iavg_index;
	static int iavg_num_samples;
	static int firsttime = 1;
	int pc_unusable;

	/*
	 * if we are called first time fill all the
	 * samples with the the shutdown_iavg_ua
	 */
	if (firsttime && chip->shutdown_iavg_ua != 0) {
		pr_debug("Using shutdown_iavg_ua = %d in all samples\n",
				chip->shutdown_iavg_ua);
		for (i = 0; i < IAVG_SAMPLES; i++)
			iavg_samples[i] = chip->shutdown_iavg_ua;

		iavg_index = 0;
		iavg_num_samples = IAVG_SAMPLES;
	}

	/*
	 * if we are charging use a nominal avg current so that we keep
	 * a reasonable UUC while charging
	 */
	if (iavg_ma < MIN_IAVG_MA)
		iavg_ma = MIN_IAVG_MA;
	iavg_samples[iavg_index] = iavg_ma;
	iavg_index = (iavg_index + 1) % IAVG_SAMPLES;
	iavg_num_samples++;
	if (iavg_num_samples >= IAVG_SAMPLES)
		iavg_num_samples = IAVG_SAMPLES;

	/* now that this sample is added calcualte the average */
	iavg_ma = 0;
	if (iavg_num_samples != 0) {
		for (i = 0; i < iavg_num_samples; i++) {
			pr_debug("iavg_samples[%d] = %d\n", i, iavg_samples[i]);
			iavg_ma += iavg_samples[i];
		}

		iavg_ma = DIV_ROUND_CLOSEST(iavg_ma, iavg_num_samples);
	}

	/*
	 * if we're in bms reset mode, force uuc to be 3% of fcc
	 */
	if (bms_reset)
		return (fcc_uah * 3) / 100;

	uuc_uah_iavg = calculate_termination_uuc(chip,
					batt_temp, chargecycles,
					fcc_uah, iavg_ma,
					&pc_unusable);
	pr_debug("iavg = %d uuc_iavg = %d\n", iavg_ma, uuc_uah_iavg);

	/* restrict the uuc change to one percent per 60 seconds */
	uuc_uah_iavg = adjust_uuc(chip, fcc_uah, pc_unusable, uuc_uah_iavg,
				batt_temp, rbatt, &iavg_ma, delta_time_s);

	/* find out what the avg current should be for this uuc */
	chip->prev_uuc_iavg_ma = iavg_ma;

	firsttime = 0;
	return uuc_uah_iavg;
}

/* calculate remainging charge at the time of ocv */
static int calculate_remaining_charge_uah(struct pm8921_bms_chip *chip,
						struct pm8921_soc_params *raw,
						int fcc_uah, int batt_temp,
						int chargecycles)
{
	int  ocv, pc, batt_temp_decidegc;

	ocv = raw->last_good_ocv_uv;
	batt_temp_decidegc = chip->last_ocv_temp_decidegc;
	pc = calculate_pc(chip, ocv, batt_temp_decidegc, chargecycles);
	pr_debug("ocv = %d pc = %d\n", ocv, pc);
	return (fcc_uah * pc) / 100;
}

static void calculate_soc_params(struct pm8921_bms_chip *chip,
						struct pm8921_soc_params *raw,
						int batt_temp, int chargecycles,
						int *fcc_uah,
						int *unusable_charge_uah,
						int *remaining_charge_uah,
						int *cc_uah,
						int *rbatt,
						int *iavg_ua)
{
	int soc_rbatt;
	int delta_time_s;
	int rc;

	rc = calculate_delta_time(chip, &delta_time_s);
	if (rc) {
		pr_err("Failed to get delta time from RTC: %d\n", rc);
		delta_time_s = 0;
	}
	*fcc_uah = calculate_fcc_uah(chip, batt_temp, chargecycles);
	pr_debug("FCC = %uuAh batt_temp = %d, cycles = %d\n",
					*fcc_uah, batt_temp, chargecycles);


	/* calculate remainging charge */
	*remaining_charge_uah = calculate_remaining_charge_uah(chip, raw,
					*fcc_uah, batt_temp, chargecycles);
	pr_debug("RC = %uuAh\n", *remaining_charge_uah);

	/* calculate cc micro_volt_hour */
	calculate_cc_uah(chip, raw->cc, cc_uah);
	pr_debug("cc_uah = %duAh raw->cc = %x\n", *cc_uah, raw->cc);

	soc_rbatt = ((*remaining_charge_uah - *cc_uah) * 100) / *fcc_uah;
	if (soc_rbatt < 0)
		soc_rbatt = 0;
	*rbatt = get_rbatt(chip, soc_rbatt, batt_temp);

	calculate_iavg_ua(chip, *cc_uah, iavg_ua, delta_time_s);

	*unusable_charge_uah = calculate_unusable_charge_uah(chip, *rbatt,
					*fcc_uah, *cc_uah, soc_rbatt,
					batt_temp, chargecycles, *iavg_ua,
					delta_time_s);
	pr_debug("UUC = %uuAh\n", *unusable_charge_uah);
}

int pm8921_bms_get_simultaneous_battery_voltage_and_current(int *ibat_ua,
								int *vbat_uv)
{
	int rc;

	if (the_chip == NULL) {
		pr_err("Called too early\n");
		return -EINVAL;
	}

	if (pm8921_is_batfet_closed()) {
		return override_mode_simultaneous_battery_voltage_and_current(
								ibat_ua,
								vbat_uv);
	} else {
		pr_debug("batfet is open using separate vbat and ibat meas\n");
		rc = get_battery_uvolts(the_chip, vbat_uv);
		if (rc < 0) {
			pr_err("adc vbat failed err = %d\n", rc);
			return rc;
		}
		rc = pm8921_bms_get_battery_current(ibat_ua);
		if (rc < 0) {
			pr_err("bms ibat failed err = %d\n", rc);
			return rc;
		}
	}

	return 0;
}
EXPORT_SYMBOL(pm8921_bms_get_simultaneous_battery_voltage_and_current);

#define SIGN(x) ((x) < 0 ? -1 : 1)

static void find_ocv_for_soc(struct pm8921_bms_chip *chip,
			int batt_temp,
			int chargecycles,
			int fcc_uah,
			int uuc_uah,
			int cc_uah,
			int shutdown_soc,
			int *rc_uah,
			int *ocv_uv)
{
	s64 rc;
	int pc, new_pc;
	int batt_temp_degc = batt_temp / 10;
	int ocv;

	rc = (s64)shutdown_soc * (fcc_uah - uuc_uah);
	rc = div_s64(rc, 100) + cc_uah + uuc_uah;
	pc = DIV_ROUND_CLOSEST((int)rc * 100, fcc_uah);
	pc = clamp(pc, 0, 100);

	ocv = interpolate_ocv(chip->pc_temp_ocv_lut, batt_temp_degc, pc);

	pr_debug("s_soc = %d, fcc = %d uuc = %d rc = %d, pc = %d, ocv mv = %d\n",
			shutdown_soc, fcc_uah, uuc_uah, (int)rc, pc, ocv);
	new_pc = interpolate_pc(chip->pc_temp_ocv_lut, batt_temp_degc, ocv);
	pr_debug("test revlookup pc = %d for ocv = %d\n", new_pc, ocv);

	if (abs(new_pc - pc) > 0) {
		/* Maximum spins to make in while-loop when searching in
		 * full resolution.
		 */
		const unsigned int max_spin_count =
			chip->max_voltage_uv / 1000 - chip->v_cutoff + 1;
		unsigned int count = 0;
		int delta_mv = 5;
		int diff = abs(new_pc - pc);
		char sign = SIGN(new_pc - pc);
		char old_sign;
		int old_diff;
		int old_ocv;

		do {
			count++;
			old_ocv = ocv;
			old_diff = diff;
			old_sign = sign;

			if (new_pc > pc)
				ocv -= delta_mv;
			else
				ocv += delta_mv;

			new_pc = interpolate_pc(chip->pc_temp_ocv_lut,
							batt_temp_degc, ocv);
			pr_debug("test revlookup pc = %d for ocv = %d\n",
				new_pc, ocv);
			diff = abs(new_pc - pc);
			sign = SIGN(new_pc - pc);

			if (sign != old_sign) {
				if (delta_mv == 5) {
					/*
					 * we crossed our desired PC probably
					 * becuase we were overcorrecting
					 */
					delta_mv = 1;
				} else {
					/* we crossed our desired PC even with
					 * 1mV steps, choose the best of two */
					if (diff > old_diff)
						ocv = old_ocv;

					break;
				}
			}
		} while (count <= max_spin_count && diff > 0);
	}

	*ocv_uv = ocv * 1000;
	*rc_uah = (int)rc;
}

static void adjust_rc_and_uuc_for_specific_soc(
						struct pm8921_bms_chip *chip,
						int batt_temp,
						int chargecycles,
						int soc,
						int fcc_uah,
						int uuc_uah,
						int cc_uah,
						int rc_uah,
						int rbatt,
						int *ret_ocv,
						int *ret_rc,
						int *ret_uuc,
						int *ret_rbatt)
{
	int ocv_uv;

	find_ocv_for_soc(chip, batt_temp, chargecycles,
					fcc_uah, uuc_uah, cc_uah,
					soc,
					&rc_uah, &ocv_uv);

	*ret_ocv = ocv_uv;
	*ret_rbatt = rbatt;
	*ret_rc = rc_uah;
	*ret_uuc = uuc_uah;
}

static void calc_current_max(struct pm8921_bms_chip *chip, int ocv_uv,
		int rbatt_mohm)
{
	chip->imax_ua = 1000 * (ocv_uv - chip->v_cutoff * 1000) / rbatt_mohm;
}

static int bound_soc(int soc)
{
	soc = max(0, soc);
	soc = min(100, soc);
	return soc;
}

static int charging_adjustments(struct pm8921_bms_chip *chip,
				int soc, int vbat_uv, int ibat_ua,
				int batt_temp, int chargecycles,
				int fcc_uah, int cc_uah, int uuc_uah)
{
	int chg_soc;
	int vbat_batt_terminal_uv = vbat_uv
					+ (ibat_ua * chip->rconn_mohm) / 1000;

	if (chip->soc_at_cv == -EINVAL) {
		/* In constant current charging return the calc soc */
		if (vbat_batt_terminal_uv <= chip->max_voltage_uv)
			pr_debug("CC CHG SOC %d\n", soc);

		/* Note the CC to CV point */
		if (vbat_batt_terminal_uv >= chip->max_voltage_uv) {
			chip->soc_at_cv = soc;
			chip->prev_chg_soc = soc;
			chip->ibat_at_cv_ua = ibat_ua;
			pr_debug("CC_TO_CV ibat_ua = %d CHG SOC %d\n",
					ibat_ua, soc);
		}

		chip->prev_vbat_batt_terminal_uv = vbat_batt_terminal_uv;
		return soc;
	}

	/*
	 * battery is in CV phase - begin liner inerpolation of soc based on
	 * battery charge current
	 */

	/*
	 * if the battery terminal voltage lessened (possibly because of
	 * a sudden increase in system load) keep reporting the prev chg soc
	 */
	if (vbat_batt_terminal_uv < chip->prev_vbat_batt_terminal_uv) {
		pr_debug("vbat_terminals %d < prev = %d CC CHG SOC %d\n",
			vbat_batt_terminal_uv,
			chip->prev_vbat_batt_terminal_uv,
			chip->prev_chg_soc);
		chip->prev_vbat_batt_terminal_uv = vbat_batt_terminal_uv;
		return chip->prev_chg_soc;
	}

	chg_soc = linear_interpolate(chip->soc_at_cv, chip->ibat_at_cv_ua,
					100, -1 * chip->chg_term_ua,
					ibat_ua);
	chg_soc = bound_soc(chg_soc);

	/* always report a higher soc */
	if (chg_soc > chip->prev_chg_soc) {
		int new_ocv_uv;
		int new_rc;

		chip->prev_chg_soc = chg_soc;

		find_ocv_for_soc(chip, batt_temp, chargecycles,
				fcc_uah, uuc_uah, cc_uah,
				chg_soc,
				&new_rc, &new_ocv_uv);
		the_chip->last_ocv_uv = new_ocv_uv;
		pr_debug("CC CHG ADJ OCV = %d CHG SOC %d\n",
				new_ocv_uv,
				chip->prev_chg_soc);
	}

	pr_debug("Reporting CHG SOC %d\n", chip->prev_chg_soc);
	chip->prev_vbat_batt_terminal_uv = vbat_batt_terminal_uv;
	return chip->prev_chg_soc;
}

static void very_low_voltage_check(struct pm8921_bms_chip *chip,
					int ibat_ua, int vbat_uv)
{
	int rc;
	/*
	 * if battery is very low (v_cutoff voltage + 20mv) hold
	 * a wakelock untill soc = 0%
	 */
	if (vbat_uv <= (chip->alarm_low_mv + 20) * 1000 &&
		!wake_lock_active(&the_chip->low_voltage_wake_lock)) {
		pr_debug("voltage = %d low holding wakelock\n", vbat_uv);
		wake_lock(&chip->low_voltage_wake_lock);
		chip->soc_calc_period = chip->low_voltage_calc_ms;
	}

	if (vbat_uv > (chip->alarm_low_mv + 20 + BATT_ALARM_ACCURACY) * 1000
			&& wake_lock_active(&the_chip->low_voltage_wake_lock)) {
		pr_debug("voltage = %d releasing wakelock\n", vbat_uv);
		chip->vbatt_cutoff_count = 0;
		chip->soc_calc_period = chip->normal_voltage_calc_ms;
		rc = pm8921_bms_enable_batt_alarm(chip);
		if (rc)
			pr_err("Unable to enable batt alarm\n");
		wake_unlock(&chip->low_voltage_wake_lock);
	}
}

static bool is_voltage_below_cutoff_window(struct pm8921_bms_chip *chip,
						int ibat_ua, int vbat_uv)
{
	if (vbat_uv < (chip->v_cutoff * 1000) && ibat_ua > 0) {
		chip->vbatt_cutoff_count++;
		if (chip->vbatt_cutoff_count >= chip->vbatt_cutoff_retries) {
			pr_debug("cutoff_count >= %d\n",
					chip->vbatt_cutoff_retries);
			return true;
		}
	} else {
		chip->vbatt_cutoff_count = 0;
	}

	return false;
}

static int last_soc_est = -EINVAL;
static int adjust_soc(struct pm8921_bms_chip *chip, int soc,
		int batt_temp, int chargecycles,
		int rbatt, int fcc_uah, int uuc_uah, int cc_uah)
{
	int ibat_ua = 0, vbat_uv = 0;
	int ocv_est_uv = 0, soc_est = 0, pc_est = 0, pc = 0;
	int delta_ocv_uv = 0;
	int n = 0;
	int rc_new_uah = 0;
	int pc_new = 0;
	int soc_new = 0;
	int m = 0;
	int rc = 0;
	int delta_ocv_uv_limit = 0;
	int correction_limit_uv = 0;

	rc = pm8921_bms_get_simultaneous_battery_voltage_and_current(
							&ibat_ua,
							&vbat_uv);
	if (rc < 0) {
		pr_err("simultaneous vbat ibat failed err = %d\n", rc);
		goto out;
	}

	very_low_voltage_check(chip, ibat_ua, vbat_uv);

	if (chip->low_voltage_detect &&
		wake_lock_active(&chip->low_voltage_wake_lock)) {
		if (is_voltage_below_cutoff_window(chip, ibat_ua, vbat_uv)) {
			soc = 0;
			pr_info("Voltage below cutoff, setting soc to 0\n");
			goto out;
		}
	}

	delta_ocv_uv_limit = DIV_ROUND_CLOSEST(ibat_ua, 1000);

	ocv_est_uv = vbat_uv + (ibat_ua * rbatt)/1000;
	calc_current_max(chip, ocv_est_uv, rbatt);
	pc_est = calculate_pc(chip, ocv_est_uv, batt_temp, last_chargecycles);
	soc_est = div_s64((s64)fcc_uah * pc_est - uuc_uah*100,
						(s64)fcc_uah - uuc_uah);
	soc_est = bound_soc(soc_est);

	/* never adjust during bms reset mode */
	if (bms_reset) {
		pr_debug("bms reset mode, SOC adjustment skipped\n");
		goto out;
	}

	if (ibat_ua < 0 && pm8921_is_batfet_closed()) {
		soc = charging_adjustments(chip, soc, vbat_uv, ibat_ua,
				batt_temp, chargecycles,
				fcc_uah, cc_uah, uuc_uah);
		goto out;
	}

	/*
	 * do not adjust
	 * if soc_est is same as what bms calculated
	 * OR if soc_est > 15
	 * OR if soc it is above 90 because we might pull it low
	 * and  cause a bad user experience
	 */
	if (soc_est == soc
		|| soc_est > 15
		|| soc >= 90)
		goto out;

	if (last_soc_est == -EINVAL)
		last_soc_est = soc;

	n = min(200, max(1 , soc + soc_est + last_soc_est));
	/* remember the last soc_est in last_soc_est */
	last_soc_est = soc_est;

	pc = calculate_pc(chip, chip->last_ocv_uv,
			chip->last_ocv_temp_decidegc, last_chargecycles);
	if (pc > 0) {
		pc_new = calculate_pc(chip, chip->last_ocv_uv - (++m * 1000),
					chip->last_ocv_temp_decidegc,
					last_chargecycles);
		while (pc_new == pc) {
			/* start taking 10mV steps */
			m = m + 10;
			pc_new = calculate_pc(chip,
						chip->last_ocv_uv - (m * 1000),
						chip->last_ocv_temp_decidegc,
						last_chargecycles);
		}
	} else {
		/*
		 * pc is already at the lowest point,
		 * assume 1 millivolt translates to 1% pc
		 */
		pc = 1;
		pc_new = 0;
		m = 1;
	}

	delta_ocv_uv = div_s64((soc - soc_est) * (s64)m * 1000,
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
		pr_debug("Low Voltage, apply only ibat limited corrections\n");
		goto skip_limiting_corrections;
	}

	if (chip->last_ocv_uv > 3800000)
		correction_limit_uv = the_chip->high_ocv_correction_limit_uv;
	else
		correction_limit_uv = the_chip->low_ocv_correction_limit_uv;

	if (abs(delta_ocv_uv) > correction_limit_uv) {
		pr_debug("limiting delta ocv %d limit = %d\n", delta_ocv_uv,
				correction_limit_uv);

		if (delta_ocv_uv > 0)
			delta_ocv_uv = correction_limit_uv;
		else
			delta_ocv_uv = -1 * correction_limit_uv;
		pr_debug("new delta ocv = %d\n", delta_ocv_uv);
	}

skip_limiting_corrections:
	chip->last_ocv_uv -= delta_ocv_uv;

	if (chip->last_ocv_uv >= chip->max_voltage_uv)
		chip->last_ocv_uv = chip->max_voltage_uv;

	/* calculate the soc based on this new ocv */
	pc_new = calculate_pc(chip, chip->last_ocv_uv,
			chip->last_ocv_temp_decidegc, last_chargecycles);
	rc_new_uah = (fcc_uah * pc_new) / 100;
	soc_new = (rc_new_uah - cc_uah - uuc_uah)*100 / (fcc_uah - uuc_uah);
	soc_new = bound_soc(soc_new);

	/*
	 * if soc_new is ZERO force it higher so that phone doesnt report soc=0
	 * soc = 0 should happen only when soc_est == 0
	 */
	if (soc_new == 0 && soc_est >= the_chip->hold_soc_est)
		soc_new = 1;

	soc = soc_new;

out:
	pr_debug("ibat_ua = %d, vbat_uv = %d, ocv_est_uv = %d, pc_est = %d, "
		"soc_est = %d, n = %d, delta_ocv_uv = %d, last_ocv_uv = %d, "
		"pc_new = %d, soc_new = %d, rbatt = %d, m = %d\n",
		ibat_ua, vbat_uv, ocv_est_uv, pc_est,
		soc_est, n, delta_ocv_uv, chip->last_ocv_uv,
		pc_new, soc_new, rbatt, m);

	return soc;
}

#define IGNORE_SOC_TEMP_DECIDEG		50
#define IAVG_STEP_SIZE_MA	50
#define IAVG_START		600
#define SOC_ZERO		0xFF
static void backup_soc_and_iavg(struct pm8921_bms_chip *chip, int batt_temp,
				int soc)
{
	u8 temp;
	int iavg_ma = chip->prev_uuc_iavg_ma;

	if (iavg_ma > IAVG_START)
		temp = (iavg_ma - IAVG_START) / IAVG_STEP_SIZE_MA;
	else
		temp = 0;

	pm_bms_masked_write(chip, TEMP_IAVG_STORAGE,
			TEMP_IAVG_STORAGE_USE_MASK, temp);

	/* since only 6 bits are available for SOC, we store half the soc */
	if (soc == 0)
		temp = SOC_ZERO;
	else
		temp = soc;

	/* don't store soc if temperature is below 5degC */
	if (batt_temp > IGNORE_SOC_TEMP_DECIDEG)
		pm8xxx_writeb(the_chip->dev->parent, TEMP_SOC_STORAGE, temp);
}

static void read_shutdown_soc_and_iavg(struct pm8921_bms_chip *chip)
{
	int rc;
	u8 temp;

	rc = pm8xxx_readb(chip->dev->parent, TEMP_IAVG_STORAGE, &temp);
	if (rc) {
		pr_err("failed to read addr = %d %d assuming %d\n",
				TEMP_IAVG_STORAGE, rc, IAVG_START);
		chip->shutdown_iavg_ua = IAVG_START;
	} else {
		temp &= TEMP_IAVG_STORAGE_USE_MASK;

		if (temp == 0) {
			chip->shutdown_iavg_ua = IAVG_START;
		} else {
			chip->shutdown_iavg_ua = IAVG_START
					+ IAVG_STEP_SIZE_MA * (temp + 1);
		}
	}

	rc = pm8xxx_readb(chip->dev->parent, TEMP_SOC_STORAGE, &temp);
	if (rc) {
		pr_err("failed to read addr = %d %d\n", TEMP_SOC_STORAGE, rc);
	} else {
		chip->shutdown_soc = temp;

		if (chip->shutdown_soc == 0) {
			pr_debug("No shutdown soc available\n");
			shutdown_soc_invalid = 1;
			chip->shutdown_iavg_ua = 0;
		} else if (chip->shutdown_soc == SOC_ZERO) {
			chip->shutdown_soc = 0;
		}
	}

	if (chip->ignore_shutdown_soc) {
		shutdown_soc_invalid = 1;
		chip->shutdown_soc = 0;
		chip->shutdown_iavg_ua = 0;
	}

	pr_debug("shutdown_soc = %d shutdown_iavg = %d shutdown_soc_invalid = %d\n",
			chip->shutdown_soc,
			chip->shutdown_iavg_ua,
			shutdown_soc_invalid);
}

#define SOC_CATCHUP_SEC_MAX		600
#define SOC_CATCHUP_SEC_PER_PERCENT	60
#define MAX_CATCHUP_SOC	(SOC_CATCHUP_SEC_MAX/SOC_CATCHUP_SEC_PER_PERCENT)
static int scale_soc_while_chg(struct pm8921_bms_chip *chip,
				int delta_time_us, int new_soc, int prev_soc)
{
	int chg_time_sec;
	int catch_up_sec;
	int scaled_soc;
	int numerator;

	/*
	 * The device must be charging for reporting a higher soc, if
	 * not ignore this soc and continue reporting the prev_soc.
	 * Also don't report a high value immediately slowly scale the
	 * value from prev_soc to the new soc based on a charge time
	 * weighted average
	 */

	/* if we are not charging return last soc */
	if (the_chip->start_percent == -EINVAL)
		return prev_soc;

	/* do not scale at 100 */
	if (new_soc == 100)
		return new_soc;

	chg_time_sec = DIV_ROUND_UP(the_chip->charge_time_us, USEC_PER_SEC);
	catch_up_sec = DIV_ROUND_UP(the_chip->catch_up_time_us, USEC_PER_SEC);
	if (catch_up_sec == 0)
		return new_soc;
	pr_debug("cts= %d catch_up_sec = %d\n", chg_time_sec, catch_up_sec);

	/*
	 * if we have been charging for more than catch_up time simply return
	 * new soc
	 */
	if (chg_time_sec > catch_up_sec)
		return new_soc;

	numerator = (catch_up_sec - chg_time_sec) * prev_soc
			+ chg_time_sec * new_soc;
	scaled_soc = numerator / catch_up_sec;

	pr_debug("cts = %d new_soc = %d prev_soc = %d scaled_soc = %d\n",
			chg_time_sec, new_soc, prev_soc, scaled_soc);

	return scaled_soc;
}

static bool is_shutdown_soc_within_limits(struct pm8921_bms_chip *chip, int soc)
{
	if (shutdown_soc_invalid) {
		pr_debug("NOT forcing shutdown soc = %d\n", chip->shutdown_soc);
		return 0;
	}

	if (abs(chip->shutdown_soc - soc) > chip->shutdown_soc_valid_limit) {
		pr_debug("rejecting shutdown soc = %d, soc = %d limit = %d\n",
			chip->shutdown_soc, soc,
			chip->shutdown_soc_valid_limit);
		shutdown_soc_invalid = 1;
		return 0;
	}

	return 1;
}

static void update_power_supply(struct pm8921_bms_chip *chip)
{
	if (chip->batt_psy == NULL || chip->batt_psy < 0)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (chip->batt_psy > 0)
		power_supply_changed(chip->batt_psy);
}

#define MIN_DELTA_625_UV	1000
static void calib_hkadc(struct pm8921_bms_chip *chip)
{
	int voltage, rc;
	struct pm8xxx_adc_chan_result result;
	int usb_chg;
	int this_delta;

	mutex_lock(&chip->calib_mutex);
	rc = pm8xxx_adc_read(the_chip->ref1p25v_channel, &result);
	if (rc) {
		pr_err("ADC failed for 1.25volts rc = %d\n", rc);
		goto out;
	}
	voltage = xoadc_reading_to_microvolt(result.adc_code);

	pr_debug("result 1.25v = 0x%x, voltage = %duV adc_meas = %lld\n",
				result.adc_code, voltage, result.measurement);

	chip->xoadc_v125 = voltage;

	rc = pm8xxx_adc_read(the_chip->ref625mv_channel, &result);
	if (rc) {
		pr_err("ADC failed for 1.25volts rc = %d\n", rc);
		goto out;
	}
	voltage = xoadc_reading_to_microvolt(result.adc_code);

	usb_chg = usb_chg_plugged_in(chip);
	pr_debug("result 0.625V = 0x%x, voltage = %duV adc_meas = %lld usb_chg = %d\n",
				result.adc_code, voltage, result.measurement,
				usb_chg);

	if (usb_chg)
		chip->xoadc_v0625_usb_present = voltage;
	else
		chip->xoadc_v0625_usb_absent = voltage;

	chip->xoadc_v0625 = voltage;
	if (chip->xoadc_v0625_usb_present && chip->xoadc_v0625_usb_absent) {
		this_delta = chip->xoadc_v0625_usb_present
						- chip->xoadc_v0625_usb_absent;
		pr_debug("this_delta= %duV\n", this_delta);
		if (this_delta > MIN_DELTA_625_UV)
			last_usb_cal_delta_uv = this_delta;
		pr_debug("625V_present= %d, 625V_absent= %d, delta = %duV\n",
			chip->xoadc_v0625_usb_present,
			chip->xoadc_v0625_usb_absent,
			last_usb_cal_delta_uv);
	}
	pr_debug("calibration batt_temp = %d\n", chip->last_calib_temp);
out:
	mutex_unlock(&chip->calib_mutex);
}

#define HKADC_CALIB_DELAY_S	600
#define HKADC_CALIB_DELTA_TEMP	20
static void calib_hkadc_check(struct pm8921_bms_chip *chip, int batt_temp)
{
	unsigned long time_since_last_calib;
	unsigned long tm_now_sec;
	int delta_temp;
	int rc;

	rc = get_current_time(&tm_now_sec);
	if (rc) {
		pr_err("Could not read current time: %d\n", rc);
		return;
	}
	if (tm_now_sec > chip->last_calib_time) {
		time_since_last_calib = tm_now_sec - chip->last_calib_time;
		delta_temp = abs(chip->last_calib_temp - batt_temp);
		pr_debug("time since last calib: %lu, temp diff = %d\n",
				time_since_last_calib, delta_temp);
		if (time_since_last_calib >= HKADC_CALIB_DELAY_S
			|| delta_temp > HKADC_CALIB_DELTA_TEMP) {
			chip->last_calib_temp = batt_temp;
			chip->last_calib_time = tm_now_sec;
			calib_hkadc(chip);
		}
	}
}

/*
 * Remaining Usable Charge = remaining_charge (charge at ocv instance)
 *				- coloumb counter charge
 *				- unusable charge (due to battery resistance)
 * SOC% = (remaining usable charge/ fcc - usable_charge);
 */
static int calculate_state_of_charge(struct pm8921_bms_chip *chip,
					struct pm8921_soc_params *raw,
					int batt_temp, int chargecycles)
{
	int remaining_usable_charge_uah, fcc_uah, unusable_charge_uah;
	int remaining_charge_uah, soc;
	int cc_uah;
	int rbatt;
	int iavg_ua;
	int new_ocv;
	int new_rc_uah;
	int new_ucc_uah;
	int new_rbatt;
	int shutdown_soc;
	int new_calculated_soc;
	static int firsttime = 1;

	calculate_soc_params(chip, raw, batt_temp, chargecycles,
						&fcc_uah,
						&unusable_charge_uah,
						&remaining_charge_uah,
						&cc_uah,
						&rbatt,
						&iavg_ua);

	/* calculate remaining usable charge */
	remaining_usable_charge_uah = remaining_charge_uah
					- cc_uah
					- unusable_charge_uah;

	pr_debug("RUC = %duAh\n", remaining_usable_charge_uah);
	if (fcc_uah - unusable_charge_uah <= 0) {
		pr_debug("FCC = %duAh, UUC = %duAh forcing soc = 0\n",
						fcc_uah, unusable_charge_uah);
		soc = 0;
	} else {
		soc = DIV_ROUND_CLOSEST((remaining_usable_charge_uah * 100),
					(fcc_uah - unusable_charge_uah));
	}

	if (firsttime && soc < 0) {
		/*
		 * first time calcualtion and the pon ocv  is too low resulting
		 * in a bad soc. Adjust ocv such that we get 0 soc
		 */
		pr_debug("soc is %d, adjusting pon ocv to make it 0\n", soc);
		adjust_rc_and_uuc_for_specific_soc(
						chip,
						batt_temp, chargecycles,
						0,
						fcc_uah, unusable_charge_uah,
						cc_uah, remaining_charge_uah,
						rbatt,
						&new_ocv,
						&new_rc_uah, &new_ucc_uah,
						&new_rbatt);
		chip->last_ocv_uv = new_ocv;
		remaining_charge_uah = new_rc_uah;
		unusable_charge_uah = new_ucc_uah;
		rbatt = new_rbatt;

		remaining_usable_charge_uah = remaining_charge_uah
					- cc_uah
					- unusable_charge_uah;

		soc = DIV_ROUND_CLOSEST((remaining_usable_charge_uah * 100),
					(fcc_uah - unusable_charge_uah));
		pr_debug("DONE for O soc is %d, pon ocv adjusted to %duV\n",
				soc, chip->last_ocv_uv);
	}

	if (soc > 100)
		soc = 100;

	if (soc < 0) {
		pr_debug("bad rem_usb_chg = %d rem_chg %d,"
				"cc_uah %d, unusb_chg %d\n",
				remaining_usable_charge_uah,
				remaining_charge_uah,
				cc_uah, unusable_charge_uah);

		pr_debug("for bad rem_usb_chg last_ocv_uv = %d"
				"chargecycles = %d, batt_temp = %d"
				"fcc = %d soc =%d\n",
				chip->last_ocv_uv, chargecycles, batt_temp,
				fcc_uah, soc);
		soc = 0;
	}

	mutex_lock(&soc_invalidation_mutex);
	shutdown_soc = chip->shutdown_soc;

	if (firsttime && soc != shutdown_soc
			&& is_shutdown_soc_within_limits(chip, soc)) {
		/*
		 * soc for the first time - use shutdown soc
		 * to adjust pon ocv since it is a small percent away from
		 * the real soc
		 */
		pr_debug("soc = %d before forcing shutdown_soc = %d\n",
							soc, shutdown_soc);
		adjust_rc_and_uuc_for_specific_soc(
						chip,
						batt_temp, chargecycles,
						shutdown_soc,
						fcc_uah, unusable_charge_uah,
						cc_uah, remaining_charge_uah,
						rbatt,
						&new_ocv,
						&new_rc_uah, &new_ucc_uah,
						&new_rbatt);

		chip->pon_ocv_uv = chip->last_ocv_uv;
		chip->last_ocv_uv = new_ocv;
		remaining_charge_uah = new_rc_uah;
		unusable_charge_uah = new_ucc_uah;
		rbatt = new_rbatt;

		remaining_usable_charge_uah = remaining_charge_uah
					- cc_uah
					- unusable_charge_uah;

		soc = DIV_ROUND_CLOSEST((remaining_usable_charge_uah * 100),
					(fcc_uah - unusable_charge_uah));

		pr_debug("DONE for shutdown_soc = %d soc is %d, adjusted ocv to %duV\n",
				shutdown_soc, soc, chip->last_ocv_uv);
	}
	mutex_unlock(&soc_invalidation_mutex);

	pr_debug("SOC before adjustment = %d\n", soc);
	new_calculated_soc = adjust_soc(chip, soc, batt_temp, chargecycles,
			rbatt, fcc_uah, unusable_charge_uah, cc_uah);

	pr_debug("calculated SOC = %d\n", new_calculated_soc);
	if (new_calculated_soc != calculated_soc) {
		calculated_soc = new_calculated_soc;
		update_power_supply(chip);
	}

	firsttime = 0;
	get_current_time(&chip->last_recalc_time);

	if (chip->disable_flat_portion_ocv) {
		if (is_between(chip->ocv_dis_high_soc, chip->ocv_dis_low_soc,
					calculated_soc)) {
			pm8921_bms_stop_ocv_updates();
		} else {
			pm8921_bms_start_ocv_updates();
		}
	}
	return calculated_soc;
}

static int recalculate_soc(struct pm8921_bms_chip *chip)
{
	int batt_temp;
	struct pm8921_soc_params raw;
	int soc;

	wake_lock(&the_chip->soc_wake_lock);
	get_batt_temp(chip, &batt_temp);

	mutex_lock(&chip->last_ocv_uv_mutex);
	calib_hkadc_check(chip, batt_temp);
	read_soc_params_raw(chip, &raw, batt_temp);

	soc = calculate_state_of_charge(chip, &raw,
					batt_temp, last_chargecycles);
	mutex_unlock(&chip->last_ocv_uv_mutex);
	wake_unlock(&the_chip->soc_wake_lock);
	return soc;
}

static void calculate_soc_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct pm8921_bms_chip *chip = container_of(dwork,
				struct pm8921_bms_chip,
				calculate_soc_delayed_work);

	recalculate_soc(chip);
	schedule_delayed_work(&chip->calculate_soc_delayed_work,
			round_jiffies_relative(msecs_to_jiffies
			(chip->soc_calc_period)));
}

static int report_state_of_charge(struct pm8921_bms_chip *chip)
{
	int soc = calculated_soc;
	int delta_time_us;
	struct timespec now;
	int batt_temp;

	if (bms_fake_battery != -EINVAL) {
		pr_debug("Returning Fake SOC = %d%%\n", bms_fake_battery);
		return bms_fake_battery;
	}

	get_batt_temp(chip, &batt_temp);

	do_posix_clock_monotonic_gettime(&now);
	if (chip->t_soc_queried.tv_sec != 0) {
		delta_time_us
		= (now.tv_sec - chip->t_soc_queried.tv_sec) * USEC_PER_SEC
			+ (now.tv_nsec - chip->t_soc_queried.tv_nsec) / 1000;
	} else {
		/* calculation for the first time */
		delta_time_us = 0;
	}

	/*
	 * account for charge time - limit it to SOC_CATCHUP_SEC to
	 * avoid overflows when charging continues for extended periods
	 */
	if (the_chip->start_percent != -EINVAL) {
		if (the_chip->charge_time_us == 0) {
			/*
			 * calculating soc for the first time
			 * after start of chg. Initialize catchup time
			 */
			if (abs(soc - last_soc) < MAX_CATCHUP_SOC)
				the_chip->catch_up_time_us =
				(soc - last_soc) * SOC_CATCHUP_SEC_PER_PERCENT
					 * USEC_PER_SEC;
			else
				the_chip->catch_up_time_us =
				SOC_CATCHUP_SEC_MAX * USEC_PER_SEC;

			if (the_chip->catch_up_time_us < 0)
				the_chip->catch_up_time_us = 0;
		}

		/* add charge time */
		if (the_chip->charge_time_us
				< SOC_CATCHUP_SEC_MAX * USEC_PER_SEC)
			chip->charge_time_us += delta_time_us;

		/* end catchup if calculated soc and last soc are same */
		if (last_soc == soc)
			the_chip->catch_up_time_us = 0;
	}

	/* last_soc < soc  ... scale and catch up */
	if (last_soc != -EINVAL && last_soc < soc)
		soc = scale_soc_while_chg(chip, delta_time_us, soc, last_soc);

	if (last_soc != -EINVAL) {
		if (chip->first_report_after_suspend) {
			chip->first_report_after_suspend = false;
			if (chip->soc_updated_on_resume) {
				/*  coming here after a long suspend */
				chip->soc_updated_on_resume = false;
				if (last_soc < soc)
					/* if soc has falsely increased during
					 * suspend, set the soc_at_suspend
					 */
					soc = chip->last_soc_at_suspend;
			} else {
				/*
				 * suspended for a short time
				 * report the last_soc before suspend
				 */
				soc = chip->last_soc_at_suspend;
			}
		} else if (soc < last_soc && soc != 0) {
			soc = last_soc - 1;
		} else if (soc > last_soc && soc != 100) {
			soc = last_soc + 1;
		}
	}

	last_soc = bound_soc(soc);
	backup_soc_and_iavg(chip, batt_temp, last_soc);
	pr_debug("Reported SOC = %d\n", last_soc);
	chip->t_soc_queried = now;

	return last_soc;
}

void pm8921_bms_battery_removed(void)
{
	if (!the_chip) {
		pr_err("called before initialization\n");
		return;
	}
	pr_info("Battery Removed Cleaning up\n");

	cancel_delayed_work_sync(&the_chip->calculate_soc_delayed_work);
	calculated_soc = 0;
	the_chip->start_percent = -EINVAL;
	the_chip->end_percent = -EINVAL;
	/* cleanup for charge time catchup */
	the_chip->charge_time_us = 0;
	the_chip->catch_up_time_us = 0;
	/* cleanup for charge time adjusting */
	the_chip->soc_at_cv = -EINVAL;
	the_chip->soc_at_cv = -EINVAL;
	the_chip->prev_chg_soc = -EINVAL;
	the_chip->ibat_at_cv_ua = 0;
	the_chip->prev_vbat_batt_terminal_uv = 0;
	/* ocv cleanups */
	the_chip->ocv_reading_at_100 = OCV_RAW_UNINITIALIZED;
	the_chip->prev_last_good_ocv_raw = OCV_RAW_UNINITIALIZED;
	the_chip->last_ocv_temp_decidegc = -EINVAL;

	/* cleanup delta time */
	the_chip->tm_sec = 0;

	/* cc and avg current cleanups */
	the_chip->prev_iavg_ua = 0;
	the_chip->last_cc_uah = INT_MIN;

	/* report SOC cleanups */
	the_chip->t_soc_queried.tv_sec = 0;
	the_chip->t_soc_queried.tv_nsec = 0;

	last_soc = -EINVAL;
	/* store invalid soc */
	pm8xxx_writeb(the_chip->dev->parent, TEMP_SOC_STORAGE, 0);

	/* fcc learning cleanup */
	if (the_chip->enable_fcc_learning) {
		battery_removed = 1;
		sysfs_notify(&the_chip->dev->kobj, NULL, "fcc_data");
	}

	/* UUC related data is left as is - use the same historical load avg */
	update_power_supply(the_chip);
}
EXPORT_SYMBOL(pm8921_bms_battery_removed);

void pm8921_bms_battery_inserted(void)
{
	if (!the_chip) {
		pr_err("called before initialization\n");
		return;
	}

	pr_info("Battery Inserted\n");
	the_chip->last_ocv_uv = estimate_ocv(the_chip);
	schedule_delayed_work(&the_chip->calculate_soc_delayed_work, 0);
}
EXPORT_SYMBOL(pm8921_bms_battery_inserted);

void pm8921_bms_invalidate_shutdown_soc(void)
{
	int calculate_soc = 0;
	struct pm8921_bms_chip *chip = the_chip;

	/* clean up the fcc learning table */
	if (!the_chip)
		the_chip->adjusted_fcc_temp_lut = NULL;
	last_fcc_update_count = 0;
	last_real_fcc_mah = -EINVAL;
	last_real_fcc_batt_temp = -EINVAL;
	battery_removed = 1;

	pr_debug("Invalidating shutdown soc - the battery was removed\n");
	if (shutdown_soc_invalid)
		return;

	mutex_lock(&soc_invalidation_mutex);
	shutdown_soc_invalid = 1;
	last_soc = -EINVAL;
	if (the_chip) {
		/* reset to pon ocv undoing what the adjusting did */
		if (the_chip->pon_ocv_uv) {
			the_chip->last_ocv_uv = the_chip->pon_ocv_uv;
			calculate_soc = 1;
			pr_debug("resetting ocv to pon_ocv = %d\n",
						the_chip->pon_ocv_uv);
		}
	}
	mutex_unlock(&soc_invalidation_mutex);
	if (!calculate_soc)
		return;
	recalculate_soc(chip);
}
EXPORT_SYMBOL(pm8921_bms_invalidate_shutdown_soc);

static void calibrate_hkadc_work(struct work_struct *work)
{
	struct pm8921_bms_chip *chip = container_of(work,
				struct pm8921_bms_chip, calib_hkadc_work);

	calib_hkadc(chip);
}

void pm8921_bms_calibrate_hkadc(void)
{
	schedule_work(&the_chip->calib_hkadc_work);
}

int pm8921_bms_get_vsense_avg(int *result)
{
	int rc = -EINVAL;

	if (the_chip) {
		mutex_lock(&the_chip->bms_output_lock);
		pm_bms_lock_output_data(the_chip);
		rc = read_vsense_avg(the_chip, result);
		pm_bms_unlock_output_data(the_chip);
		mutex_unlock(&the_chip->bms_output_lock);
	}

	pr_err("called before initialization\n");
	return rc;
}
EXPORT_SYMBOL(pm8921_bms_get_vsense_avg);

int pm8921_bms_get_battery_current(int *result_ua)
{
	int vsense_uv;
	int rc = 0;

	*result_ua = 0;
	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}
	if (the_chip->r_sense_uohm == 0) {
		pr_err("r_sense is zero\n");
		return -EINVAL;
	}

	mutex_lock(&the_chip->bms_output_lock);
	pm_bms_lock_output_data(the_chip);
	rc = read_vsense_avg(the_chip, &vsense_uv);
	pm_bms_unlock_output_data(the_chip);
	mutex_unlock(&the_chip->bms_output_lock);
	if (rc) {
		pr_err("Unable to read vsense average\n");
		goto error_vsense;
	}
	pr_debug("vsense=%duV\n", vsense_uv);
	/* cast for signed division */
	*result_ua = div_s64(vsense_uv * 1000000LL, the_chip->r_sense_uohm);
	pr_debug("ibat=%duA\n", *result_ua);

error_vsense:
	return rc;
}
EXPORT_SYMBOL(pm8921_bms_get_battery_current);

int pm8921_bms_get_percent_charge(void)
{
	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}

	return report_state_of_charge(the_chip);
}
EXPORT_SYMBOL_GPL(pm8921_bms_get_percent_charge);

int pm8921_bms_get_current_max(void)
{
	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}
	return the_chip->imax_ua;
}
EXPORT_SYMBOL_GPL(pm8921_bms_get_current_max);

int pm8921_bms_get_fcc(void)
{
	int batt_temp;

	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}

	get_batt_temp(the_chip, &batt_temp);
	return calculate_fcc_uah(the_chip, batt_temp, last_chargecycles);
}
EXPORT_SYMBOL_GPL(pm8921_bms_get_fcc);

static void calculate_real_soc(struct pm8921_bms_chip *chip, int *soc,
		int batt_temp, struct pm8921_soc_params *raw, int cc_uah)
{
	int fcc_uah = 0, rc_uah = 0;

	fcc_uah = calculate_fcc_uah(chip, batt_temp, last_chargecycles);
	rc_uah = calculate_remaining_charge_uah(chip, raw,
				fcc_uah, batt_temp, last_chargecycles);
	*soc = ((rc_uah - cc_uah) * 100) / fcc_uah;
	pr_debug("fcc = %d, rc = %d, cc = %d Real SOC = %d\n",
				fcc_uah, rc_uah, cc_uah, *soc);
}

void pm8921_bms_charging_began(void)
{
	struct pm8921_soc_params raw;
	int batt_temp;

	get_batt_temp(the_chip, &batt_temp);

	mutex_lock(&the_chip->last_ocv_uv_mutex);
	calib_hkadc_check(the_chip, batt_temp);
	read_soc_params_raw(the_chip, &raw, batt_temp);
	mutex_unlock(&the_chip->last_ocv_uv_mutex);

	the_chip->start_percent = report_state_of_charge(the_chip);

	bms_start_percent = the_chip->start_percent;
	bms_start_ocv_uv = raw.last_good_ocv_uv;
	calculate_cc_uah(the_chip, raw.cc, &bms_start_cc_uah);
	pm_bms_masked_write(the_chip, BMS_TOLERANCES,
			IBAT_TOL_MASK, IBAT_TOL_DEFAULT);
	the_chip->charge_time_us = 0;
	the_chip->catch_up_time_us = 0;

	the_chip->soc_at_cv = -EINVAL;
	the_chip->prev_chg_soc = -EINVAL;
	if (the_chip->enable_fcc_learning) {
		calculate_real_soc(the_chip, &the_chip->start_real_soc,
				batt_temp, &raw, bms_start_cc_uah);
		the_chip->pc_at_start_charge =
			interpolate_pc(the_chip->pc_temp_ocv_lut, batt_temp,
						bms_start_ocv_uv / 1000);
		pr_debug("Start real soc = %d, start pc = %d\n",
			the_chip->start_real_soc, the_chip->pc_at_start_charge);
	}

	pr_debug("start_percent = %u%%\n", the_chip->start_percent);
}
EXPORT_SYMBOL_GPL(pm8921_bms_charging_began);

static void invalidate_fcc(struct pm8921_bms_chip *chip)
{
	memset(chip->fcc_table, 0, chip->min_fcc_learning_samples *
					sizeof(*(chip->fcc_table)));
	last_fcc_update_count = 0;
	chip->adjusted_fcc_temp_lut = NULL;
	last_real_fcc_mah = -EINVAL;
	last_real_fcc_batt_temp = -EINVAL;
	last_chargecycles = 0;
	last_charge_increase = 0;
}

static void update_fcc_table_for_temp(struct pm8921_bms_chip *chip,
						int batt_temp_final)
{
	int i, fcc_t1, fcc_t2, fcc_final;
	struct fcc_data *ft;

	/* Interpolate all the FCC entries to the same temperature */
	for (i = 0; i < chip->min_fcc_learning_samples; i++) {
		ft = &chip->fcc_table[i];
		if (ft->batt_temp == batt_temp_final)
			continue;
		fcc_t1 = interpolate_fcc(chip->fcc_temp_lut, ft->batt_temp);
		fcc_t2 = interpolate_fcc(chip->fcc_temp_lut, batt_temp_final);
		fcc_final = (ft->fcc_new / fcc_t1) * fcc_t2;
		ft->fcc_new = fcc_final;
		ft->batt_temp = batt_temp_final;
	}
}

static void update_fcc_learning_table(struct pm8921_bms_chip *chip,
		int fcc_uah, int new_fcc_uah, int chargecycles, int batt_temp)
{
	int i, temp_fcc_avg = 0, new_fcc_avg = 0, temp_fcc_delta = 0, count;
	struct fcc_data *ft;

	count = last_fcc_update_count % chip->min_fcc_learning_samples;
	ft = &chip->fcc_table[count];
	ft->fcc_new = ft->fcc_real = new_fcc_uah;
	ft->batt_temp = ft->temp_real = batt_temp;
	ft->chargecycles = chargecycles;
	chip->fcc_new = new_fcc_uah;
	last_fcc_update_count++;
	/* update userspace with the new data */
	sysfs_notify(&chip->dev->kobj, NULL, "fcc_data");

	pr_debug("Updated fcc table. new_fcc=%d, chargecycle=%d, temp=%d fcc_update_count=%d\n",
		new_fcc_uah, chargecycles, batt_temp, last_fcc_update_count);

	if (last_fcc_update_count < chip->min_fcc_learning_samples) {
		pr_debug("Not enough FCC samples. Current count = %d\n",
						last_fcc_update_count);
		return; /* Not enough samples to update fcc */
	}

	/* reject entries if they are > 50 chargecycles apart */
	for (i = 0; i < chip->min_fcc_learning_samples; i++) {
		if ((chip->fcc_table[i].chargecycles + VALID_FCC_CHGCYL_RANGE)
							< chargecycles) {
			pr_debug("Charge cycle too old (> %d cycles apart)\n",
							VALID_FCC_CHGCYL_RANGE);
			return; /* Samples old, > 50 cycles apart*/
		}
	}
	/* update the fcc table for temperature difference*/
	update_fcc_table_for_temp(chip, batt_temp);

	/* Calculate the avg. and SD for all the fcc entries */
	for (i = 0; i < chip->min_fcc_learning_samples; i++)
		temp_fcc_avg += chip->fcc_table[i].fcc_new;

	temp_fcc_avg /= chip->min_fcc_learning_samples;
	temp_fcc_delta = div_u64(temp_fcc_avg * DELTA_FCC_PERCENT, 100);

	/* fix the fcc if its an outlier i.e. > 5% of the average */
	for (i = 0; i < chip->min_fcc_learning_samples; i++) {
		ft = &chip->fcc_table[i];
		if (abs(ft->fcc_new - temp_fcc_avg) > temp_fcc_delta)
			ft->fcc_new = temp_fcc_avg;
		new_fcc_avg += ft->fcc_new;
	}
	new_fcc_avg /= chip->min_fcc_learning_samples;

	last_real_fcc_mah = new_fcc_avg/1000;
	last_real_fcc_batt_temp = batt_temp;

	pr_debug("FCC update: last_real_fcc_mah=%d, last_real_fcc_batt_temp=%d\n",
						new_fcc_avg, batt_temp);
	readjust_fcc_table();
	sysfs_notify(&chip->dev->kobj, NULL, "fcc_data");
}

static bool is_new_fcc_valid(int new_fcc_uah, int fcc_uah)
{
	/* reject the new fcc if < 50% and > 105% of nominal fcc */
	if ((new_fcc_uah >= (fcc_uah / 2)) &&
			((new_fcc_uah * 100) <= (fcc_uah * 105)))
		return true;

	pr_debug("FCC rejected - not within valid limit\n");
	return false;
}

void pm8921_bms_charging_end(int is_battery_full)
{
	int batt_temp;
	struct pm8921_soc_params raw;

	if (the_chip == NULL)
		return;

	get_batt_temp(the_chip, &batt_temp);

	mutex_lock(&the_chip->last_ocv_uv_mutex);

	calib_hkadc_check(the_chip, batt_temp);
	read_soc_params_raw(the_chip, &raw, batt_temp);

	calculate_cc_uah(the_chip, raw.cc, &bms_end_cc_uah);

	bms_end_ocv_uv = raw.last_good_ocv_uv;

	pr_debug("battery_full = %d, fcc_learning = %d, pc_start_chg = %d\n",
				is_battery_full, the_chip->enable_fcc_learning,
						the_chip->pc_at_start_charge);
	if (is_battery_full && the_chip->enable_fcc_learning &&
		(the_chip->start_percent <= the_chip->min_fcc_learning_soc) &&
		(the_chip->pc_at_start_charge <= the_chip->min_fcc_ocv_pc)) {

		int fcc_uah, new_fcc_uah, delta_cc_uah, delta_soc;
		/* new_fcc = (cc_end - cc_start) / (end_soc - start_soc) */
		delta_soc = 100 - the_chip->start_real_soc;
		delta_cc_uah = abs(bms_end_cc_uah - bms_start_cc_uah);
		new_fcc_uah = div_u64(delta_cc_uah * 100, delta_soc);

		fcc_uah = calculate_fcc_uah(the_chip, batt_temp,
						last_chargecycles);
		pr_info("start_real_soc = %d, end_real_soc = 100, start_cc = %d, end_cc = %d, nominal_fcc = %d, new_fcc = %d\n",
			the_chip->start_real_soc, bms_start_cc_uah,
			bms_end_cc_uah, fcc_uah, new_fcc_uah);
		if (is_new_fcc_valid(new_fcc_uah, fcc_uah))
			update_fcc_learning_table(the_chip, fcc_uah,
				new_fcc_uah, last_chargecycles, batt_temp);
	}

	if (is_battery_full) {
		the_chip->ocv_reading_at_100 = raw.last_good_ocv_raw;

		the_chip->last_ocv_uv = the_chip->max_voltage_uv;
		raw.last_good_ocv_uv = the_chip->max_voltage_uv;
		raw.cc = 0;
		/* reset the cc in h/w */
		reset_cc(the_chip);
		the_chip->last_ocv_temp_decidegc = batt_temp;
		/*
		 * since we are treating this as an ocv event
		 * forget the old cc value
		 */
		the_chip->last_cc_uah = 0;
		pr_debug("EOC BATT_FULL ocv_reading = 0x%x\n",
				the_chip->ocv_reading_at_100);
	}

	the_chip->end_percent = calculate_state_of_charge(the_chip, &raw,
					batt_temp, last_chargecycles);
	mutex_unlock(&the_chip->last_ocv_uv_mutex);

	bms_end_percent = the_chip->end_percent;

	if (the_chip->end_percent > the_chip->start_percent) {
		last_charge_increase +=
			the_chip->end_percent - the_chip->start_percent;
		if (last_charge_increase > 100) {
			last_chargecycles++;
			last_charge_increase = last_charge_increase % 100;
		}
	}
	pr_debug("end_percent = %u%% last_charge_increase = %d"
			"last_chargecycles = %d\n",
			the_chip->end_percent,
			last_charge_increase,
			last_chargecycles);
	the_chip->start_percent = -EINVAL;
	the_chip->end_percent = -EINVAL;
	the_chip->charge_time_us = 0;
	the_chip->catch_up_time_us = 0;
	the_chip->soc_at_cv = -EINVAL;
	the_chip->prev_chg_soc = -EINVAL;
	pm_bms_masked_write(the_chip, BMS_TOLERANCES,
				IBAT_TOL_MASK, IBAT_TOL_NOCHG);
}
EXPORT_SYMBOL_GPL(pm8921_bms_charging_end);

static irqreturn_t pm8921_bms_sbi_write_ok_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_cc_thr_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_vsense_thr_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_vsense_for_r_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_ocv_for_r_handler(int irq, void *data)
{
	struct pm8921_bms_chip *chip = data;

	pr_debug("irq = %d triggered", irq);
	schedule_work(&chip->calib_hkadc_work);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_good_ocv_handler(int irq, void *data)
{
	struct pm8921_bms_chip *chip = data;

	pr_debug("irq = %d triggered", irq);
	schedule_work(&chip->calib_hkadc_work);
	return IRQ_HANDLED;
}

struct pm_bms_irq_init_data {
	unsigned int	irq_id;
	char		*name;
	unsigned long	flags;
	irqreturn_t	(*handler)(int, void *);
};

#define BMS_IRQ(_id, _flags, _handler) \
{ \
	.irq_id		= _id, \
	.name		= #_id, \
	.flags		= _flags, \
	.handler	= _handler, \
}

struct pm_bms_irq_init_data bms_irq_data[] = {
	BMS_IRQ(PM8921_BMS_SBI_WRITE_OK, IRQF_TRIGGER_RISING,
				pm8921_bms_sbi_write_ok_handler),
	BMS_IRQ(PM8921_BMS_CC_THR, IRQF_TRIGGER_RISING,
				pm8921_bms_cc_thr_handler),
	BMS_IRQ(PM8921_BMS_VSENSE_THR, IRQF_TRIGGER_RISING,
				pm8921_bms_vsense_thr_handler),
	BMS_IRQ(PM8921_BMS_VSENSE_FOR_R, IRQF_TRIGGER_RISING,
				pm8921_bms_vsense_for_r_handler),
	BMS_IRQ(PM8921_BMS_OCV_FOR_R, IRQF_TRIGGER_RISING,
				pm8921_bms_ocv_for_r_handler),
	BMS_IRQ(PM8921_BMS_GOOD_OCV, IRQF_TRIGGER_RISING,
				pm8921_bms_good_ocv_handler),
};

static void free_irqs(struct pm8921_bms_chip *chip)
{
	int i;

	for (i = 0; i < PM_BMS_MAX_INTS; i++)
		if (chip->pmic_bms_irq[i]) {
			free_irq(chip->pmic_bms_irq[i], NULL);
			chip->pmic_bms_irq[i] = 0;
		}
}

static int __devinit request_irqs(struct pm8921_bms_chip *chip,
					struct platform_device *pdev)
{
	struct resource *res;
	int ret, i;

	ret = 0;
	bitmap_fill(chip->enabled_irqs, PM_BMS_MAX_INTS);

	for (i = 0; i < ARRAY_SIZE(bms_irq_data); i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				bms_irq_data[i].name);
		if (res == NULL) {
			pr_err("couldn't find %s\n", bms_irq_data[i].name);
			goto err_out;
		}
		ret = request_irq(res->start, bms_irq_data[i].handler,
			bms_irq_data[i].flags,
			bms_irq_data[i].name, chip);
		if (ret < 0) {
			pr_err("couldn't request %d (%s) %d\n", res->start,
					bms_irq_data[i].name, ret);
			goto err_out;
		}
		chip->pmic_bms_irq[bms_irq_data[i].irq_id] = res->start;
		pm8921_bms_disable_irq(chip, bms_irq_data[i].irq_id);
	}
	return 0;

err_out:
	free_irqs(chip);
	return -EINVAL;
}

#define EN_BMS_BIT	BIT(7)
#define EN_PON_HS_BIT	BIT(0)
static int __devinit pm8921_bms_hw_init(struct pm8921_bms_chip *chip)
{
	int rc;

	rc = pm_bms_masked_write(chip, BMS_CONTROL,
			EN_BMS_BIT | EN_PON_HS_BIT, EN_BMS_BIT | EN_PON_HS_BIT);
	if (rc) {
		pr_err("failed to enable pon and bms addr = %d %d",
				BMS_CONTROL, rc);
	}

	/* The charger will call start charge later if usb is present */
	pm_bms_masked_write(chip, BMS_TOLERANCES,
				IBAT_TOL_MASK, IBAT_TOL_NOCHG);
	return 0;
}

static void check_initial_ocv(struct pm8921_bms_chip *chip)
{
	int ocv_uv, rc;
	int16_t ocv_raw;
	int usb_chg;

	/*
	 * Check if a ocv is available in bms hw,
	 * if not compute it here at boot time and save it
	 * in the last_ocv_uv.
	 */
	ocv_uv = 0;
	pm_bms_read_output_data(chip, LAST_GOOD_OCV_VALUE, &ocv_raw);
	usb_chg = usb_chg_plugged_in(chip);
	rc = convert_vbatt_raw_to_uv(chip, usb_chg, ocv_raw, &ocv_uv);
	if (rc || ocv_uv == 0) {
		rc = adc_based_ocv(chip, &ocv_uv);
		if (rc) {
			pr_err("failed to read adc based ocv_uv rc = %d\n", rc);
			ocv_uv = DEFAULT_OCV_MICROVOLTS;
		}
	}
	chip->last_ocv_uv = ocv_uv;
	pr_debug("ocv_uv = %d last_ocv_uv = %d\n", ocv_uv, chip->last_ocv_uv);
}

static int64_t read_battery_id(struct pm8921_bms_chip *chip)
{
	int rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(chip->batt_id_channel, &result);
	if (rc) {
		pr_err("error reading batt id channel = %d, rc = %d\n",
					chip->vbat_channel, rc);
		return rc;
	}
	pr_debug("batt_id phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	return result.adc_code;
}

#define PALLADIUM_ID_MIN	0x7F40
#define PALLADIUM_ID_MAX	0x7F5A
#define DESAY_5200_ID_MIN	0x7F7F
#define DESAY_5200_ID_MAX	0x802F
static int set_battery_data(struct pm8921_bms_chip *chip)
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
		chip->delta_rbatt_mohm = palladium_1500_data.delta_rbatt_mohm;
		chip->rbatt_capacitive_mohm
			= palladium_1500_data.rbatt_capacitive_mohm;
		return 0;
desay:
		chip->fcc = desay_5200_data.fcc;
		chip->fcc_temp_lut = desay_5200_data.fcc_temp_lut;
		chip->pc_temp_ocv_lut = desay_5200_data.pc_temp_ocv_lut;
		chip->pc_sf_lut = desay_5200_data.pc_sf_lut;
		chip->rbatt_sf_lut = desay_5200_data.rbatt_sf_lut;
		chip->default_rbatt_mohm = desay_5200_data.default_rbatt_mohm;
		chip->delta_rbatt_mohm = desay_5200_data.delta_rbatt_mohm;
		chip->rbatt_capacitive_mohm
			= desay_5200_data.rbatt_capacitive_mohm;
		return 0;
}

enum bms_request_operation {
	CALC_FCC,
	CALC_PC,
	CALC_SOC,
	CALIB_HKADC,
	CALIB_CCADC,
	GET_VBAT_VSENSE_SIMULTANEOUS,
	STOP_OCV,
	START_OCV,
	SET_OCV,
	BATT_PRESENT,
};

static int test_batt_temp = 5;
static int test_chargecycle = 150;
static int test_ocv = 3900000;
enum {
	TEST_BATT_TEMP,
	TEST_CHARGE_CYCLE,
	TEST_OCV,
};
static int get_test_param(void *data, u64 * val)
{
	switch ((int)data) {
	case TEST_BATT_TEMP:
		*val = test_batt_temp;
		break;
	case TEST_CHARGE_CYCLE:
		*val = test_chargecycle;
		break;
	case TEST_OCV:
		*val = test_ocv;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static int set_test_param(void *data, u64  val)
{
	switch ((int)data) {
	case TEST_BATT_TEMP:
		test_batt_temp = (int)val;
		break;
	case TEST_CHARGE_CYCLE:
		test_chargecycle = (int)val;
		break;
	case TEST_OCV:
		test_ocv = (int)val;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(temp_fops, get_test_param, set_test_param, "%llu\n");

static int get_calc(void *data, u64 * val)
{
	int param = (int)data;
	int ret = 0;
	int ibat_ua, vbat_uv;
	struct pm8921_soc_params raw;

	read_soc_params_raw(the_chip, &raw, 300);

	*val = 0;

	/* global irq number passed in via data */
	switch (param) {
	case CALC_FCC:
		*val = calculate_fcc_uah(the_chip, test_batt_temp,
							test_chargecycle);
		break;
	case CALC_PC:
		*val = calculate_pc(the_chip, test_ocv, test_batt_temp,
							test_chargecycle);
		break;
	case CALC_SOC:
		*val = calculate_state_of_charge(the_chip, &raw,
					test_batt_temp, test_chargecycle);
		break;
	case CALIB_HKADC:
		/* reading this will trigger calibration */
		*val = 0;
		calib_hkadc(the_chip);
		break;
	case CALIB_CCADC:
		/* reading this will trigger calibration */
		*val = 0;
		pm8xxx_calib_ccadc();
		break;
	case GET_VBAT_VSENSE_SIMULTANEOUS:
		/* reading this will call simultaneous vbat and vsense */
		*val =
		pm8921_bms_get_simultaneous_battery_voltage_and_current(
			&ibat_ua,
			&vbat_uv);
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int set_calc(void *data, u64 val)
{
	int param = (int)data;
	int ret = 0;

	switch (param) {
	case STOP_OCV:
		pm8921_bms_stop_ocv_updates();
		break;
	case START_OCV:
		pm8921_bms_start_ocv_updates();
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(calc_fops, get_calc, set_calc, "%llu\n");

static int get_reading(void *data, u64 * val)
{
	int param = (int)data;
	int ret = 0;
	struct pm8921_soc_params raw;

	mutex_lock(&the_chip->last_ocv_uv_mutex);
	read_soc_params_raw(the_chip, &raw, 300);
	mutex_lock(&the_chip->last_ocv_uv_mutex);

	*val = 0;

	switch (param) {
	case CC_MSB:
	case CC_LSB:
		*val = raw.cc;
		break;
	case LAST_GOOD_OCV_VALUE:
		*val = raw.last_good_ocv_uv;
		break;
	case VSENSE_AVG:
		read_vsense_avg(the_chip, (uint *)val);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(reading_fops, get_reading, NULL, "%lld\n");

static int get_rt_status(void *data, u64 * val)
{
	int i = (int)data;
	int ret;

	/* global irq number passed in via data */
	ret = pm_bms_get_rt_status(the_chip, i);
	*val = ret;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rt_fops, get_rt_status, NULL, "%llu\n");

static int get_reg(void *data, u64 * val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	ret = pm8xxx_readb(the_chip->dev->parent, addr, &temp);
	if (ret) {
		pr_err("pm8xxx_readb to %x value = %d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	temp = (u8) val;
	ret = pm8xxx_writeb(the_chip->dev->parent, addr, temp);
	if (ret) {
		pr_err("pm8xxx_writeb to %x value = %d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

static void create_debugfs_entries(struct pm8921_bms_chip *chip)
{
	int i;

	chip->dent = debugfs_create_dir("pm8921-bms", NULL);

	if (IS_ERR(chip->dent)) {
		pr_err("pmic bms couldnt create debugfs dir\n");
		return;
	}

	debugfs_create_file("BMS_CONTROL", 0644, chip->dent,
			(void *)BMS_CONTROL, &reg_fops);
	debugfs_create_file("BMS_OUTPUT0", 0644, chip->dent,
			(void *)BMS_OUTPUT0, &reg_fops);
	debugfs_create_file("BMS_OUTPUT1", 0644, chip->dent,
			(void *)BMS_OUTPUT1, &reg_fops);
	debugfs_create_file("BMS_TEST1", 0644, chip->dent,
			(void *)BMS_TEST1, &reg_fops);

	debugfs_create_file("test_batt_temp", 0644, chip->dent,
				(void *)TEST_BATT_TEMP, &temp_fops);
	debugfs_create_file("test_chargecycle", 0644, chip->dent,
				(void *)TEST_CHARGE_CYCLE, &temp_fops);
	debugfs_create_file("test_ocv", 0644, chip->dent,
				(void *)TEST_OCV, &temp_fops);

	debugfs_create_file("read_cc", 0644, chip->dent,
				(void *)CC_MSB, &reading_fops);
	debugfs_create_file("read_last_good_ocv", 0644, chip->dent,
				(void *)LAST_GOOD_OCV_VALUE, &reading_fops);
	debugfs_create_file("read_vbatt_for_rbatt", 0644, chip->dent,
				(void *)VBATT_FOR_RBATT, &reading_fops);
	debugfs_create_file("read_vsense_for_rbatt", 0644, chip->dent,
				(void *)VSENSE_FOR_RBATT, &reading_fops);
	debugfs_create_file("read_ocv_for_rbatt", 0644, chip->dent,
				(void *)OCV_FOR_RBATT, &reading_fops);
	debugfs_create_file("read_vsense_avg", 0644, chip->dent,
				(void *)VSENSE_AVG, &reading_fops);

	debugfs_create_file("show_fcc", 0644, chip->dent,
				(void *)CALC_FCC, &calc_fops);
	debugfs_create_file("show_pc", 0644, chip->dent,
				(void *)CALC_PC, &calc_fops);
	debugfs_create_file("show_soc", 0644, chip->dent,
				(void *)CALC_SOC, &calc_fops);
	debugfs_create_file("calib_hkadc", 0644, chip->dent,
				(void *)CALIB_HKADC, &calc_fops);
	debugfs_create_file("calib_ccadc", 0644, chip->dent,
				(void *)CALIB_CCADC, &calc_fops);
	debugfs_create_file("stop_ocv", 0644, chip->dent,
				(void *)STOP_OCV, &calc_fops);
	debugfs_create_file("start_ocv", 0644, chip->dent,
				(void *)START_OCV, &calc_fops);
	debugfs_create_file("set_ocv", 0644, chip->dent,
				(void *)SET_OCV, &calc_fops);
	debugfs_create_file("batt_present", 0644, chip->dent,
				(void *)BATT_PRESENT, &calc_fops);

	debugfs_create_file("simultaneous", 0644, chip->dent,
			(void *)GET_VBAT_VSENSE_SIMULTANEOUS, &calc_fops);

	for (i = 0; i < ARRAY_SIZE(bms_irq_data); i++) {
		if (chip->pmic_bms_irq[bms_irq_data[i].irq_id])
			debugfs_create_file(bms_irq_data[i].name, 0444,
				chip->dent,
				(void *)bms_irq_data[i].irq_id,
				&rt_fops);
	}
}

static ssize_t fcc_data_set(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct pm8921_bms_chip *chip = dev_get_drvdata(dev);
	static int i;
	int fcc_new = 0, rc;

	if (battery_removed) {
		pr_debug("Invalid FCC table. Possible battery removal\n");
		last_fcc_update_count = 0;
		return count;
	}

	i %= chip->min_fcc_learning_samples;
	rc = sscanf(buf, "%d", &fcc_new);
	if (rc != 1)
		return -EINVAL;
	chip->fcc_table[i].fcc_new = fcc_new;
	chip->fcc_table[i].fcc_real = fcc_new;
	pr_debug("Rcvd: [%d] fcc_new=%d\n", i, fcc_new);
	i++;

	return count;
}

static ssize_t fcc_data_get(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	int count = 0;
	struct pm8921_bms_chip *chip = dev_get_drvdata(dev);

	if (battery_removed) {
		pr_debug("Invalidate the fcc table\n");
		invalidate_fcc(chip);
		battery_removed = 0;
		return count;
	}

	count = snprintf(buf, PAGE_SIZE, "%d", chip->fcc_new);

	pr_debug("Sent: fcc_new=%d\n", chip->fcc_new);

	return count;
}

static ssize_t fcc_temp_set(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	static int i;
	int batt_temp = 0, rc;
	struct pm8921_bms_chip *chip = dev_get_drvdata(dev);

	i %= chip->min_fcc_learning_samples;
	rc = sscanf(buf, "%d", &batt_temp);
	if (rc != 1)
		return -EINVAL;
	chip->fcc_table[i].batt_temp = batt_temp;
	chip->fcc_table[i].temp_real = batt_temp;
	pr_debug("Rcvd: [%d] batt_temp=%d\n", i, batt_temp);
	i++;

	return count;
}

static ssize_t fcc_chgcyl_set(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	static int i;
	int chargecycle = 0, rc;
	struct pm8921_bms_chip *chip = dev_get_drvdata(dev);

	i %= chip->min_fcc_learning_samples;
	rc = sscanf(buf, "%d", &chargecycle);
	if (rc != 1)
		return -EINVAL;
	chip->fcc_table[i].chargecycles = chargecycle;
	pr_debug("Rcvd: [%d] chargecycle=%d\n", i, chargecycle);
	i++;

	return count;
}

static ssize_t fcc_list_get(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	struct pm8921_bms_chip *chip = dev_get_drvdata(dev);
	struct fcc_data *ft;
	int i = 0, j, count = 0;

	if (last_fcc_update_count < chip->min_fcc_learning_samples)
		i = last_fcc_update_count;
	else
		i = chip->min_fcc_learning_samples;

	for (j = 0; j < i; j++) {
		ft = &chip->fcc_table[j];
		count += snprintf(buf + count, PAGE_SIZE - count,
			"%d %d %d %d %d\n", ft->fcc_new, ft->chargecycles,
			ft->batt_temp, ft->fcc_real, ft->temp_real);
	}

	return count;
}

static DEVICE_ATTR(fcc_data, 0664, fcc_data_get, fcc_data_set);
static DEVICE_ATTR(fcc_temp, 0664, NULL, fcc_temp_set);
static DEVICE_ATTR(fcc_chgcyl, 0664, NULL, fcc_chgcyl_set);
static DEVICE_ATTR(fcc_list, 0664, fcc_list_get, NULL);

static struct attribute *fcc_attrs[] = {
	&dev_attr_fcc_data.attr,
	&dev_attr_fcc_temp.attr,
	&dev_attr_fcc_chgcyl.attr,
	&dev_attr_fcc_list.attr,
	NULL
};

static const struct attribute_group fcc_attr_group = {
	.attrs = fcc_attrs,
};

#define REG_SBI_CONFIG		0x04F
#define PAGE3_ENABLE_MASK	0x6
#define PROGRAM_REV_MASK	0x0F
#define PROGRAM_REV		0x9
static int read_ocv_trim(struct pm8921_bms_chip *chip)
{
	int rc;
	u8 reg, sbi_config;

	rc = pm8xxx_readb(chip->dev->parent, REG_SBI_CONFIG, &sbi_config);
	if (rc) {
		pr_err("error = %d reading sbi config reg\n", rc);
		return rc;
	}

	reg = sbi_config | PAGE3_ENABLE_MASK;
	rc = pm8xxx_writeb(chip->dev->parent, REG_SBI_CONFIG, reg);
	if (rc) {
		pr_err("error = %d writing sbi config reg\n", rc);
		return rc;
	}

	rc = pm8xxx_readb(chip->dev->parent, TEST_PROGRAM_REV, &reg);
	if (rc)
		pr_err("Error %d reading %d addr %d\n",
			rc, reg, TEST_PROGRAM_REV);
	pr_err("program rev reg is 0x%x\n", reg);
	reg &= PROGRAM_REV_MASK;

	/* If the revision is equal or higher do not adjust trim delta */
	if (reg >= PROGRAM_REV) {
		chip->amux_2_trim_delta = 0;
		goto restore_sbi_config;
	}

	rc = pm8xxx_readb(chip->dev->parent, AMUX_TRIM_2, &reg);
	if (rc) {
		pr_err("error = %d reading trim reg\n", rc);
		return rc;
	}

	pr_err("trim reg is 0x%x\n", reg);
	chip->amux_2_trim_delta = abs(0x49 - reg);
	pr_err("trim delta is %d\n", chip->amux_2_trim_delta);

restore_sbi_config:
	rc = pm8xxx_writeb(chip->dev->parent, REG_SBI_CONFIG, sbi_config);
	if (rc) {
		pr_err("error = %d writing sbi config reg\n", rc);
		return rc;
	}

	return 0;
}

static int __devinit pm8921_bms_probe(struct platform_device *pdev)
{
	int rc = 0;
	int vbatt;
	struct pm8921_bms_chip *chip;
	const struct pm8921_bms_platform_data *pdata
				= pdev->dev.platform_data;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct pm8921_bms_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("Cannot allocate pm_bms_chip\n");
		return -ENOMEM;
	}

	mutex_init(&chip->bms_output_lock);
	mutex_init(&chip->last_ocv_uv_mutex);
	chip->dev = &pdev->dev;
	chip->r_sense_uohm = pdata->r_sense_uohm;
	chip->v_cutoff = pdata->v_cutoff;
	chip->max_voltage_uv = pdata->max_voltage_uv;
	chip->chg_term_ua = pdata->chg_term_ua;
	chip->batt_type = pdata->battery_type;
	chip->rconn_mohm = pdata->rconn_mohm;
	chip->start_percent = -EINVAL;
	chip->end_percent = -EINVAL;
	chip->last_cc_uah = INT_MIN;
	chip->ocv_reading_at_100 = OCV_RAW_UNINITIALIZED;
	chip->prev_last_good_ocv_raw = OCV_RAW_UNINITIALIZED;
	chip->shutdown_soc_valid_limit = pdata->shutdown_soc_valid_limit;
	chip->adjust_soc_low_threshold = pdata->adjust_soc_low_threshold;

	chip->normal_voltage_calc_ms = pdata->normal_voltage_calc_ms;
	chip->low_voltage_calc_ms = pdata->low_voltage_calc_ms;

	chip->soc_calc_period = pdata->normal_voltage_calc_ms;

	if (chip->adjust_soc_low_threshold >= 45)
		chip->adjust_soc_low_threshold = 45;

	chip->prev_pc_unusable = -EINVAL;
	chip->soc_at_cv = -EINVAL;
	chip->imax_ua = -EINVAL;

	chip->ignore_shutdown_soc = pdata->ignore_shutdown_soc;
	rc = set_battery_data(chip);
	if (rc) {
		pr_err("%s bad battery data %d\n", __func__, rc);
		goto free_chip;
	}

	if (chip->pc_temp_ocv_lut == NULL) {
		pr_err("temp ocv lut table is NULL\n");
		rc = -EINVAL;
		goto free_chip;
	}

	/* set defaults in the battery data */
	if (chip->default_rbatt_mohm <= 0)
		chip->default_rbatt_mohm = DEFAULT_RBATT_MOHMS;

	chip->batt_temp_channel = pdata->bms_cdata.batt_temp_channel;
	chip->vbat_channel = pdata->bms_cdata.vbat_channel;
	chip->ref625mv_channel = pdata->bms_cdata.ref625mv_channel;
	chip->ref1p25v_channel = pdata->bms_cdata.ref1p25v_channel;
	chip->batt_id_channel = pdata->bms_cdata.batt_id_channel;
	chip->revision = pm8xxx_get_revision(chip->dev->parent);
	chip->enable_fcc_learning = pdata->enable_fcc_learning;
	chip->min_fcc_learning_soc = pdata->min_fcc_learning_soc;
	chip->min_fcc_ocv_pc = pdata->min_fcc_ocv_pc;
	chip->min_fcc_learning_samples = pdata->min_fcc_learning_samples;
	if (chip->enable_fcc_learning) {
		if (!chip->min_fcc_learning_soc)
			chip->min_fcc_learning_soc =
					MIN_START_PERCENT_FOR_LEARNING;
		if (!chip->min_fcc_ocv_pc)
			chip->min_fcc_ocv_pc =
					MIN_START_OCV_PERCENT_FOR_LEARNING;
		if (!chip->min_fcc_learning_samples ||
			chip->min_fcc_learning_samples > MAX_FCC_LEARNING_COUNT)
			chip->min_fcc_learning_samples = MAX_FCC_LEARNING_COUNT;

		min_fcc_cycles = chip->min_fcc_learning_samples;
		chip->fcc_table = kzalloc(sizeof(struct fcc_data) *
				chip->min_fcc_learning_samples, GFP_KERNEL);
		if (!chip->fcc_table) {
			pr_err("Unable to allocate table for fcc learning\n");
			rc = -ENOMEM;
			goto free_chip;
		}
		rc = sysfs_create_group(&pdev->dev.kobj, &fcc_attr_group);
		if (rc) {
			pr_err("Unable to create sysfs entries\n");
			goto free_chip;
		}
	}

	chip->disable_flat_portion_ocv = pdata->disable_flat_portion_ocv;
	chip->ocv_dis_high_soc = pdata->ocv_dis_high_soc;
	chip->ocv_dis_low_soc = pdata->ocv_dis_low_soc;

	chip->high_ocv_correction_limit_uv
					= pdata->high_ocv_correction_limit_uv;
	chip->low_ocv_correction_limit_uv = pdata->low_ocv_correction_limit_uv;
	chip->hold_soc_est = pdata->hold_soc_est;

	chip->alarm_low_mv = pdata->alarm_low_mv;
	chip->alarm_high_mv = pdata->alarm_high_mv;
	chip->low_voltage_detect = pdata->low_voltage_detect;
	chip->vbatt_cutoff_retries = pdata->vbatt_cutoff_retries;

	mutex_init(&chip->calib_mutex);
	INIT_WORK(&chip->calib_hkadc_work, calibrate_hkadc_work);

	INIT_DELAYED_WORK(&chip->calculate_soc_delayed_work,
			calculate_soc_work);

	wake_lock_init(&chip->soc_wake_lock,
			WAKE_LOCK_SUSPEND, "pm8921_soc_lock");
	rc = request_irqs(chip, pdev);
	if (rc) {
		pr_err("couldn't register interrupts rc = %d\n", rc);
		goto destroy_soc_wl;
	}

	wake_lock_init(&chip->low_voltage_wake_lock,
			WAKE_LOCK_SUSPEND, "pm8921_bms_low");

	rc = pm8921_bms_hw_init(chip);
	if (rc) {
		pr_err("couldn't init hardware rc = %d\n", rc);
		goto free_irqs;
	}

	read_shutdown_soc_and_iavg(chip);

	platform_set_drvdata(pdev, chip);
	the_chip = chip;
	create_debugfs_entries(chip);

	rc = read_ocv_trim(chip);
	if (rc) {
		pr_err("couldn't adjust ocv_trim rc= %d\n", rc);
		goto free_irqs;
	}
	check_initial_ocv(chip);

	/* enable the vbatt reading interrupts for scheduling hkadc calib */
	pm8921_bms_enable_irq(chip, PM8921_BMS_GOOD_OCV);
	pm8921_bms_enable_irq(chip, PM8921_BMS_OCV_FOR_R);

	rc = pm8921_bms_configure_batt_alarm(chip);
	if (rc) {
		pr_err("Couldn't configure battery alarm! rc=%d\n", rc);
		goto free_irqs;
	}

	rc = pm8921_bms_enable_batt_alarm(chip);
	if (rc) {
		pr_err("Couldn't enable battery alarm! rc=%d\n", rc);
		goto free_irqs;
	}

	calculate_soc_work(&(chip->calculate_soc_delayed_work.work));

	rc = get_battery_uvolts(chip, &vbatt);
	if (!rc)
		pr_info("OK battery_capacity_at_boot=%d volt = %d ocv = %d\n",
				pm8921_bms_get_percent_charge(),
				vbatt, chip->last_ocv_uv);
	else
		pr_info("Unable to read battery voltage at boot\n");

	return 0;

free_irqs:
	wake_lock_destroy(&chip->low_voltage_wake_lock);
	free_irqs(chip);
destroy_soc_wl:
	wake_lock_destroy(&chip->soc_wake_lock);
free_chip:
	kfree(chip);
	return rc;
}

static int __devexit pm8921_bms_remove(struct platform_device *pdev)
{
	struct pm8921_bms_chip *chip = platform_get_drvdata(pdev);

	free_irqs(chip);
	kfree(chip->adjusted_fcc_temp_lut);
	platform_set_drvdata(pdev, NULL);
	the_chip = NULL;
	kfree(chip);
	return 0;
}

static int pm8921_bms_suspend(struct device *dev)
{
	struct pm8921_bms_chip *chip = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&chip->calculate_soc_delayed_work);

	chip->last_soc_at_suspend = last_soc;

	return 0;
}

static int pm8921_bms_resume(struct device *dev)
{
	int rc;
	unsigned long time_since_last_recalc;
	unsigned long tm_now_sec;
	struct pm8921_bms_chip *chip = dev_get_drvdata(dev);

	rc = get_current_time(&tm_now_sec);
	if (rc) {
		pr_err("Could not read current time: %d\n", rc);
		return 0;
	}

	if (tm_now_sec > chip->last_recalc_time) {
		time_since_last_recalc = tm_now_sec -
				chip->last_recalc_time;
		pr_debug("Time since last recalc: %lu\n",
				time_since_last_recalc);
		if ((time_since_last_recalc * 1000) >=
					chip->soc_calc_period) {
			chip->last_recalc_time = tm_now_sec;
			recalculate_soc(chip);
			chip->soc_updated_on_resume = true;
		}
	}
	chip->first_report_after_suspend = true;
	update_power_supply(chip);
	schedule_delayed_work(&chip->calculate_soc_delayed_work,
				msecs_to_jiffies(chip->soc_calc_period));

	return 0;
}

static const struct dev_pm_ops pm8921_bms_pm_ops = {
	.resume		= pm8921_bms_resume,
	.suspend	= pm8921_bms_suspend,
};

static struct platform_driver pm8921_bms_driver = {
	.probe	= pm8921_bms_probe,
	.remove	= __devexit_p(pm8921_bms_remove),
	.driver	= {
		.name	= PM8921_BMS_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &pm8921_bms_pm_ops,
	},
};

static int __init pm8921_bms_init(void)
{
	return platform_driver_register(&pm8921_bms_driver);
}

static void __exit pm8921_bms_exit(void)
{
	platform_driver_unregister(&pm8921_bms_driver);
}

late_initcall(pm8921_bms_init);
module_exit(pm8921_bms_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8921 bms driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8921_BMS_DEV_NAME);
