/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/power_supply.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/spmi.h>
#include <linux/wakelock.h>
#include <linux/debugfs.h>

#include <linux/qpnp/power-on.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/of_batterydata.h>
#include <linux/batterydata-interface.h>
#include <uapi/linux/vm_bms.h>

#define _BMS_MASK(BITS, POS) \
	((unsigned char)(((1 << (BITS)) - 1) << (POS)))
#define BMS_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
		_BMS_MASK((LEFT_BIT_POS) - (RIGHT_BIT_POS) + 1, \
					(RIGHT_BIT_POS))

/* Config / Data registers */
#define REVISION1_REG			0x0
#define STATUS1_REG			0x8
#define FSM_STATE_MASK			BMS_MASK(5, 3)
#define FSM_STATE_SHIFT			3

#define STATUS2_REG			0x9
#define FIFO_CNT_SD_MASK		BMS_MASK(7, 4)
#define FIFO_CNT_SD_SHIFT		4

#define MODE_CTL_REG			0x40
#define FORCE_S3_MODE			BIT(0)
#define ENABLE_S3_MODE			BIT(1)
#define FORCE_S2_MODE			BIT(2)
#define ENABLE_S2_MODE			BIT(3)
#define S2_MODE_MASK			BMS_MASK(3, 2)
#define S3_MODE_MASK			BMS_MASK(1, 0)

#define DATA_CTL2_REG			0x43
#define FIFO_CNT_SD_CLR_BIT		BIT(2)
#define ACC_DATA_SD_CLR_BIT		BIT(1)
#define ACC_CNT_SD_CLR_BIT		BIT(0)

#define EN_CTL_REG			0x46
#define BMS_EN_BIT			BIT(7)

#define FIFO_LENGTH_REG			0x47
#define S1_FIFO_LENGTH_MASK		BMS_MASK(3, 0)
#define S2_FIFO_LENGTH_MASK		BMS_MASK(7, 4)
#define S2_FIFO_LENGTH_SHIFT		4

#define S1_SAMPLE_INTVL_REG		0x55
#define S2_SAMPLE_INTVL_REG		0x56
#define S3_SAMPLE_INTVL_REG		0x57

#define S1_ACC_CNT_REG			0x5E
#define S2_ACC_CNT_REG			0x5F
#define ACC_CNT_MASK			BMS_MASK(2, 0)

#define ACC_DATA0_SD_REG		0x63
#define ACC_CNT_SD_REG			0x67
#define OCV_DATA0_REG			0x6A
#define FIFO_0_LSB_REG			0xC0

#define BMS_SOC_REG			0xB0
#define BMS_OCV_REG			0xB1 /* B1 & B2 */
#define SOC_STORAGE_MASK		0xFE

#define SEC_ACCESS			0xD0

#define QPNP_CHARGER_PRESENT		BIT(7)

/* Constants */
#define MAX_SAMPLE_COUNT		256
#define MAX_SAMPLE_INTERVAL		2550
#define BMS_READ_TIMEOUT		3000
#define BMS_DEFAULT_TEMP		250
#define OCV_INVALID			0xFFFF
#define SOC_INVALID			0xFF
#define OCV_UNINITIALIZED		0xFFFF
#define VBATT_ERROR_MARGIN		20000
#define CV_DROP_MARGIN			10000

#define QPNP_VM_BMS_DEV_NAME		"qcom,qpnp-vm-bms"

/* indicates the state of BMS */
enum {
	IDLE_STATE,
	S1_STATE,
	S2_STATE,
	S3_STATE,
	S7_STATE,
};

struct bms_irq {
	int		irq;
	unsigned long	disabled;
};

struct bms_wakeup_source {
	struct wakeup_source	source;
	unsigned long		disabled;
};

struct bms_dt_cfg {
	bool				cfg_report_charger_eoc;
	bool				cfg_force_s3_on_suspend;
	bool				cfg_force_s2_in_charging;
	bool				cfg_ignore_shutdown_soc;
	bool				cfg_use_voltage_soc;
	int				cfg_v_cutoff_uv;
	int				cfg_max_voltage_uv;
	int				cfg_r_conn_mohm;
	int				cfg_shutdown_soc_valid_limit;
	int				cfg_low_soc_calc_threshold;
	int				cfg_low_soc_calculate_soc_ms;
	int				cfg_low_voltage_threshold;
	int				cfg_low_voltage_calculate_soc_ms;
	int				cfg_calculate_soc_ms;
	int				cfg_voltage_soc_timeout_ms;
	int				cfg_s1_sample_interval_ms;
	int				cfg_s2_sample_interval_ms;
	int				cfg_s1_sample_count;
	int				cfg_s2_sample_count;
	int				cfg_s1_fifo_length;
	int				cfg_s2_fifo_length;
};

struct qpnp_bms_chip {
	struct device			*dev;
	struct spmi_device		*spmi;
	dev_t				dev_no;
	u16				base;
	u8				revision[2];
	u32				batt_pres_addr;
	u32				chg_pres_addr;

	/* status variables */
	u8				current_fsm_state;
	bool				last_soc_invalid;
	bool				warm_reset;
	bool				bms_psy_registered;
	bool				battery_full;
	bool				bms_dev_open;
	bool				data_ready;
	bool				charging_while_suspended;
	bool				in_cv_state;
	int				battery_status;
	int				calculated_soc;
	int				current_now;
	int				prev_voltage_based_soc;
	int				calculate_soc_ms;
	int				voltage_soc_uv;
	int				battery_present;
	int				last_soc;
	int				last_soc_unbound;
	int				last_soc_change_sec;
	int				charge_start_tm_sec;
	int				catch_up_time_sec;
	int				delta_time_s;
	int				ocv_at_100;
	int				last_ocv_uv;
	unsigned int			vadc_v0625;
	unsigned int			vadc_v1250;
	unsigned long			tm_sec;
	u32				seq_num;
	u8				shutdown_soc;
	u16				last_ocv_raw;
	u16				shutdown_ocv;

	struct bms_battery_data		*batt_data;
	struct bms_dt_cfg		dt;

	struct dentry			*debug_root;
	struct bms_wakeup_source	vbms_lv_wake_source;
	struct bms_wakeup_source	vbms_cv_wake_source;
	struct bms_wakeup_source	vbms_soc_wake_source;
	wait_queue_head_t		bms_wait_q;
	struct delayed_work		monitor_soc_work;
	struct delayed_work		voltage_soc_timeout_work;
	struct mutex			bms_data_mutex;
	struct mutex			bms_device_mutex;
	struct mutex			last_soc_mutex;
	struct class			*bms_class;
	struct device			*bms_device;
	struct cdev			bms_cdev;
	struct qpnp_vm_bms_data		bms_data;
	struct qpnp_vadc_chip		*vadc_dev;
	struct bms_irq			fifo_update_done_irq;
	struct bms_irq			fsm_state_change_irq;
	struct power_supply		bms_psy;
	struct power_supply		*batt_psy;
};

static struct qpnp_bms_chip *the_chip;

static void enable_bms_irq(struct bms_irq *irq)
{
	if (__test_and_clear_bit(0, &irq->disabled)) {
		enable_irq(irq->irq);
		pr_debug("enabled irq %d\n", irq->irq);
	}
}

static void disable_bms_irq(struct bms_irq *irq)
{
	if (!__test_and_set_bit(0, &irq->disabled)) {
		disable_irq(irq->irq);
		pr_debug("disabled irq %d\n", irq->irq);
	}
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

static bool bms_wake_active(struct bms_wakeup_source *source)
{
	return !source->disabled;
}

static int bound_soc(int soc)
{
	soc = max(0, soc);
	soc = min(100, soc);

	return soc;
}

static char *qpnp_vm_bms_supplicants[] = {
	"battery",
};

static int qpnp_read_wrapper(struct qpnp_bms_chip *chip, u8 *val,
					u16 base, int count)
{
	int rc;
	struct spmi_device *spmi = chip->spmi;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, base, val, count);
	if (rc)
		pr_err("SPMI read failed rc=%d\n", rc);

	return rc;
}

static int qpnp_write_wrapper(struct qpnp_bms_chip *chip, u8 *val,
			u16 base, int count)
{
	int rc;
	struct spmi_device *spmi = chip->spmi;

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, base, val, count);
	if (rc)
		pr_err("SPMI write failed rc=%d\n", rc);

	return rc;
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
	if (rc)
		pr_err("write failed addr = %03X, val = %02x, mask = %02x, reg = %02x, rc = %d\n",
					addr, val, mask, reg, rc);

	return rc;
}

static int qpnp_secure_write_wrapper(struct qpnp_bms_chip *chip, u8 *val,
								u16 base)
{
	int rc;
	u8 reg;

	reg = 0xA5;
	rc = qpnp_write_wrapper(chip, &reg, chip->base + SEC_ACCESS, 1);
	if (rc) {
		pr_err("Error %d writing 0xA5 to 0x%x reg\n",
				rc, SEC_ACCESS);
		return rc;
	}
	rc = qpnp_write_wrapper(chip, val, base, 1);
	if (rc)
		pr_err("Error %d writing %d to 0x%x reg\n", rc, *val, base);

	return rc;
}

static int backup_ocv_soc(struct qpnp_bms_chip *chip, int ocv_uv, int soc)
{
	int rc;
	u16 ocv_mv = ocv_uv / 1000;

	rc = qpnp_write_wrapper(chip, (u8 *)&ocv_mv,
				chip->base + BMS_OCV_REG, 2);
	if (rc)
		pr_err("Unable to backup OCV rc=%d\n", rc);

	rc = qpnp_masked_write_base(chip, chip->base + BMS_SOC_REG,
				SOC_STORAGE_MASK, (soc + 1) << 1);
	if (rc)
		pr_err("Unable to backup SOC rc=%d\n", rc);

	return rc;
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

static bool is_battery_charging(struct qpnp_bms_chip *chip)
{
	union power_supply_propval ret = {0,};

	if (chip->batt_psy == NULL)
		chip->batt_psy = power_supply_get_by_name("battery");
	if (chip->batt_psy) {
		/* if battery has been registered, use the type property */
		chip->batt_psy->get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE, &ret);
		return ret.intval != POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	/* Default to false if the battery power supply is not registered. */
	pr_debug("battery power supply is not registered\n");
	return false;
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
		chip->batt_psy->get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_PRESENT, &ret);
		return ret.intval;
	}

	/* Default to false if the battery power supply is not registered. */
	pr_debug("battery power supply is not registered\n");
	return false;
}

static int force_fsm_state(struct qpnp_bms_chip *chip, u8 state)
{
	int rc;
	u8 mode_ctl = 0;

	rc = qpnp_read_wrapper(chip, &mode_ctl, chip->base + MODE_CTL_REG, 1);
	if (rc) {
		pr_err("Unable to read reg=%x rc=%d\n", MODE_CTL_REG, rc);
		return rc;
	}

	switch (state) {
	case S2_STATE:
		mode_ctl &= ~FORCE_S3_MODE;
		mode_ctl |= FORCE_S2_MODE;
		break;
	case S3_STATE:
		mode_ctl &= ~FORCE_S2_MODE;
		mode_ctl |= FORCE_S3_MODE;
		break;
	default:
		pr_debug("Invalid state %d\n", state);
		return -EINVAL;
	}

	rc = qpnp_secure_write_wrapper(chip, &mode_ctl,
					chip->base + MODE_CTL_REG);
	if (rc) {
		pr_err("Unable to write reg=%x rc=%d\n", MODE_CTL_REG, rc);
		return rc;
	}

	pr_debug("force_mode=%d  mode_cntl_reg=%x\n", state, mode_ctl);

	return 0;
}

static int disable_force_fsm_state(struct qpnp_bms_chip *chip, u8 state)
{
	int rc;
	u8 mode_ctl = 0;

	rc = qpnp_read_wrapper(chip, &mode_ctl, chip->base + MODE_CTL_REG, 1);
	if (rc) {
		pr_err("Unable to read reg=%x rc=%d\n", MODE_CTL_REG, rc);
		return rc;
	}

	switch (state) {
	case S2_STATE:
		mode_ctl &= ~FORCE_S2_MODE;
		mode_ctl |= ENABLE_S2_MODE;
		break;
	case S3_STATE:
		mode_ctl &= ~FORCE_S3_MODE;
		mode_ctl |= ENABLE_S3_MODE;
		break;
	default:
		pr_debug("Invalid state %d\n", state);
		return -EINVAL;
	}

	rc = qpnp_secure_write_wrapper(chip, &mode_ctl,
					chip->base + MODE_CTL_REG);
	if (rc) {
		pr_err("Unable to write reg=%x rc=%d\n",
					MODE_CTL_REG, rc);
		return rc;
	}

	pr_debug("disable_force_mode=%d  mode_cntl_reg=%x\n", state, mode_ctl);

	return 0;
}

static int get_sample_interval(struct qpnp_bms_chip *chip,
				u8 fsm_state, u32 *interval)
{
	int rc;
	u8 val = 0, reg;

	*interval = 0;

	switch (fsm_state) {
	case S1_STATE:
		reg = S1_SAMPLE_INTVL_REG;
		break;
	case S2_STATE:
		reg = S2_SAMPLE_INTVL_REG;
		break;
	case S3_STATE:
		reg = S3_SAMPLE_INTVL_REG;
		break;
	default:
		pr_err("Invalid state %d\n", fsm_state);
		return -EINVAL;
	}

	rc = qpnp_read_wrapper(chip, &val, chip->base + reg, 1);
	if (rc) {
		pr_err("Failed to get state(%d) sample_interval, rc=%d\n",
						fsm_state, rc);
		return rc;
	}

	*interval = val * 10;

	return 0;
}

static int get_sample_count(struct qpnp_bms_chip *chip,
				u8 fsm_state, u32 *count)
{
	int rc;
	u8 val = 0, reg;

	*count = 0;

	switch (fsm_state) {
	case S1_STATE:
		reg = S1_ACC_CNT_REG;
		break;
	case S2_STATE:
		reg = S2_ACC_CNT_REG;
		break;
	default:
		pr_err("Invalid state %d\n", fsm_state);
		return -EINVAL;
	}

	rc = qpnp_read_wrapper(chip, &val, chip->base + reg, 1);
	if (rc) {
		pr_err("Failed to get state(%d) sample_count, rc=%d\n",
							fsm_state, rc);
		return rc;
	}
	val &= ACC_CNT_MASK;

	*count = val ? (1 << val) : 0;

	return 0;
}

static int get_fifo_length(struct qpnp_bms_chip *chip,
				u8 fsm_state, u32 *fifo_length)
{
	int rc;
	u8 val = 0, reg, mask = 0, shift = 0;

	*fifo_length = 0;

	switch (fsm_state) {
	case S1_STATE:
		reg = FIFO_LENGTH_REG;
		mask = S1_FIFO_LENGTH_MASK;
		shift = 0;
		break;
	case S2_STATE:
		reg = FIFO_LENGTH_REG;
		mask = S2_FIFO_LENGTH_MASK;
		shift = S2_FIFO_LENGTH_SHIFT;
		break;
	default:
		pr_err("Invalid state %d\n", fsm_state);
		return -EINVAL;
	}

	rc = qpnp_read_wrapper(chip, &val, chip->base + reg, 1);
	if (rc) {
		pr_err("Failed to get state(%d) fifo_length, rc=%d\n",
						fsm_state, rc);
		return rc;
	}

	val &= mask;
	val >>= shift;

	*fifo_length = val;

	return 0;
}

static int update_fsm_state(struct qpnp_bms_chip *chip)
{
	u8 val = 0;
	int rc;

	/* read the current FSM state */
	rc = qpnp_read_wrapper(chip, &val, chip->base + STATUS1_REG, 1);
	if (rc) {
		pr_err("Unable to read STATUS1_REG rc=%d\n", rc);
		return rc;
	}
	val = (val & FSM_STATE_MASK) >> FSM_STATE_SHIFT;

	chip->current_fsm_state = val;

	return 0;
}

static int lookup_soc_ocv(struct qpnp_bms_chip *chip, int ocv_uv, int batt_temp)
{
	int soc_ocv = 0, soc_cutoff = 0, soc_final = 0;

	soc_ocv = interpolate_pc(chip->batt_data->pc_temp_ocv_lut,
					batt_temp, ocv_uv / 1000);
	soc_cutoff = interpolate_pc(chip->batt_data->pc_temp_ocv_lut,
				batt_temp, chip->dt.cfg_v_cutoff_uv / 1000);

	soc_final = (100 * (soc_ocv - soc_cutoff)) / (100 - soc_cutoff);

	soc_final = bound_soc(soc_final);

	pr_debug("soc_final=%d soc_ocv=%d soc_cutoff=%d ocv_uv=%u batt_temp = %d\n",
			soc_final, soc_ocv, soc_cutoff, ocv_uv, batt_temp);

	return soc_final;
}

#define V_PER_BIT_MUL_FACTOR	97656
#define V_PER_BIT_DIV_FACTOR	1000
#define VADC_INTRINSIC_OFFSET	0x6000
static int vadc_reading_to_uv(int reading, bool vadc_bms)
{
	if (!vadc_bms) {
		/*
		 * All the BMS H/W VADC values are pre-compensated
		 * for VADC_INTRINSIC_OFFSET, subtract this offset
		 * only if this reading is not obtained from BMS
		 */

		if (reading <= VADC_INTRINSIC_OFFSET)
			return 0;

		reading -= VADC_INTRINSIC_OFFSET;
	}

	return (reading	* V_PER_BIT_MUL_FACTOR) / V_PER_BIT_DIV_FACTOR;
}

static int get_calculation_delay_ms(struct qpnp_bms_chip *chip)
{
	if (bms_wake_active(&chip->vbms_lv_wake_source))
		return chip->dt.cfg_low_voltage_calculate_soc_ms;
	if (chip->calculated_soc < chip->dt.cfg_low_soc_calc_threshold)
		return chip->dt.cfg_low_soc_calculate_soc_ms;
	else
		return chip->dt.cfg_calculate_soc_ms;
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

	chip->vadc_v0625 = vadc_reading_to_uv(raw_0625, false);
	chip->vadc_v1250 = vadc_reading_to_uv(raw_1250, false);

	pr_debug("vadc calib: 0625 = %d raw (%d uv), 1250 = %d raw (%d uv)\n",
			raw_0625, chip->vadc_v0625, raw_1250, chip->vadc_v1250);

	return 0;
}

static int convert_vbatt_raw_to_uv(struct qpnp_bms_chip *chip,
				u16 reading, bool is_pon_ocv)
{
	int64_t uv;

	uv = vadc_reading_to_uv(reading, true);
	pr_debug("%u raw converted into %lld uv\n", reading, uv);

	uv = adjust_vbatt_reading(chip, uv);
	pr_debug("adjusted into %lld uv\n", uv);

	/*
	 * TODO: add die-temp compensation once the ADC temp.
	 * coeffs are available
	 */
	return uv;
}

static void convert_and_store_ocv(struct qpnp_bms_chip *chip,
					int batt_temp, bool is_pon_ocv)
{
	int rc;

	rc = calib_vadc(chip);
	if (rc)
		pr_err("Vadc reference voltage read failed, rc = %d\n", rc);

	chip->last_ocv_uv = convert_vbatt_raw_to_uv(chip,
				chip->last_ocv_raw, is_pon_ocv);

	pr_debug("last_ocv_uv = %d\n", chip->last_ocv_uv);
}

static int read_and_update_ocv(struct qpnp_bms_chip *chip, int batt_temp,
							bool is_pon_ocv)
{
	int rc;
	u16 ocv_data = 0;

	/* read the BMS h/w OCV */
	rc = qpnp_read_wrapper(chip, (u8 *)&ocv_data,
				chip->base + OCV_DATA0_REG, 2);
	if (rc) {
		pr_err("Error reading ocv: rc = %d\n", rc);
		return -ENXIO;
	}

	if (chip->last_ocv_raw == OCV_UNINITIALIZED) {
		/* first time */
		chip->last_ocv_raw = ocv_data;
		convert_and_store_ocv(chip, batt_temp, is_pon_ocv);
	} else if (chip->last_ocv_raw != ocv_data) {
		/* a new OCV generated */
		pr_debug("new OCV!\n");
		chip->last_ocv_raw = ocv_data;
		convert_and_store_ocv(chip, batt_temp, is_pon_ocv);
	}

	pr_debug("ocv_raw=0x%x last_ocv_raw=0x%x last_ocv_uv=%d\n",
		ocv_data, chip->last_ocv_raw, chip->last_ocv_uv);

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

static int get_battery_status(struct qpnp_bms_chip *chip)
{
	union power_supply_propval ret = {0,};

	if (chip->batt_psy == NULL)
		chip->batt_psy = power_supply_get_by_name("battery");
	if (chip->batt_psy) {
		/* if battery has been registered, use the status property */
		chip->batt_psy->get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_STATUS, &ret);
		return ret.intval;
	}

	/* Default to false if the battery power supply is not registered. */
	pr_debug("battery power supply is not registered\n");
	return POWER_SUPPLY_STATUS_UNKNOWN;
}

static int get_batt_therm(struct qpnp_bms_chip *chip, int *batt_temp)
{
	int rc;
	struct qpnp_vadc_result result;

	rc = qpnp_vadc_read(chip->vadc_dev, LR_MUX1_BATT_THERM, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					LR_MUX1_BATT_THERM, rc);
		return rc;
	}
	pr_debug("batt_temp phy = %lld meas = 0x%llx\n",
			result.physical, result.measurement);

	*batt_temp = (int)result.physical;

	return 0;
}

static int get_prop_bms_rbatt(struct qpnp_bms_chip *chip)
{
	return chip->batt_data->default_rbatt_mohm;
}

static void charging_began(struct qpnp_bms_chip *chip)
{
	mutex_lock(&chip->last_soc_mutex);

	chip->charge_start_tm_sec = 0;
	chip->catch_up_time_sec = 0;

	mutex_unlock(&chip->last_soc_mutex);

	if (chip->dt.cfg_force_s2_in_charging) {
		pr_debug("Forcing S2 state\n");
		force_fsm_state(chip, S2_STATE);
	}
}

static void charging_ended(struct qpnp_bms_chip *chip)
{
	int status = get_battery_status(chip);

	mutex_lock(&chip->last_soc_mutex);

	chip->charge_start_tm_sec = 0;
	chip->catch_up_time_sec = 0;

	if (status == POWER_SUPPLY_STATUS_FULL)
		chip->last_soc_invalid = true;

	mutex_unlock(&chip->last_soc_mutex);

	if (chip->dt.cfg_force_s2_in_charging) {
		pr_debug("Unforcing S2 state\n");
		disable_force_fsm_state(chip, S2_STATE);
	}
}

static int estimate_ocv(struct qpnp_bms_chip *chip)
{
	int i, rc, vbatt = 0, vbatt_final = 0;

	for (i = 0; i < 5; i++) {
		rc = get_battery_voltage(chip, &vbatt);
		if (rc) {
			pr_err("Unable to read battery-voltage rc=%d\n", rc);
			return rc;
		}
		/*
		 * Conservatively select the lowest vbatt to avoid reporting
		 * a higher ocv due to variations in bootup current.
		 */

		if (i == 0)
			vbatt_final = vbatt;
		else if (vbatt < vbatt_final)
			vbatt_final = vbatt;

		msleep(20);
	}

	/*
	 * TODO: Revisit the OCV calcuations to use approximate ibatt
	 * and rbatt.
	 */
	return vbatt_final;
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
static int report_vm_bms_soc(struct qpnp_bms_chip *chip)
{
	int soc, soc_change;
	int time_since_last_change_sec, charge_time_sec = 0;
	unsigned long last_change_sec;
	bool charging;

	mutex_lock(&chip->last_soc_mutex);

	soc = chip->calculated_soc;

	last_change_sec = chip->last_soc_change_sec;
	calculate_delta_time(&last_change_sec, &time_since_last_change_sec);

	charging = is_battery_charging(chip);
	/*
	 * account for charge time - limit it to SOC_CATCHUP_SEC to
	 * avoid overflows when charging continues for extended periods
	 */
	if (charging && chip->last_soc != -EINVAL) {
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
		if (chip->last_soc < soc && !charging)
			soc = chip->last_soc;
		else if (chip->last_soc < soc && soc != 100)
			soc = scale_soc_while_chg(chip, charge_time_sec,
					chip->catch_up_time_sec,
					soc, chip->last_soc);

		/* if the battery is close to cutoff allow more change */
		if (bms_wake_active(&chip->vbms_lv_wake_source))
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

	pr_debug("last_soc = %d calculated_soc = %d, soc = %d, time_since_last_change = %d\n",
			chip->last_soc, chip->calculated_soc,
			soc, time_since_last_change_sec);

	chip->last_soc = bound_soc(soc);

	/*
	 * Backup the actual ocv (last_ocv_uv) and not the
	 * last_soc-interpolated ocv. This makes sure that
	 * the BMS algorithm always uses the correct ocv and
	 * can catch up on the last_soc (across reboots).
	 * We do not want the algorithm to be based of a wrong
	 * initial OCV.
	 */

	backup_ocv_soc(chip, chip->last_ocv_uv, chip->last_soc);

	pr_debug("Reported SOC=%d\n", chip->last_soc);

	mutex_unlock(&chip->last_soc_mutex);

	return chip->last_soc;
}

static int report_state_of_charge(struct qpnp_bms_chip *chip)
{
	int soc;

	if (chip->dt.cfg_use_voltage_soc)
		soc = report_voltage_based_soc(chip);
	else
		soc = report_vm_bms_soc(chip);

	return soc;
}

static void very_low_voltage_check(struct qpnp_bms_chip *chip, int vbat_uv)
{
	if (!bms_wake_active(&chip->vbms_lv_wake_source)
		&& (vbat_uv <= chip->dt.cfg_low_voltage_threshold)) {
		pr_debug("voltage = %d low, holding low voltage ws\n", vbat_uv);
		bms_stay_awake(&chip->vbms_lv_wake_source);
	} else if (bms_wake_active(&chip->vbms_lv_wake_source)
		&& (vbat_uv > chip->dt.cfg_low_voltage_threshold)) {
		pr_debug("voltage = %d releasing low voltage ws\n", vbat_uv);
		bms_relax(&chip->vbms_lv_wake_source);
	}
}

static void cv_voltage_check(struct qpnp_bms_chip *chip, int vbat_uv)
{
	if (bms_wake_active(&chip->vbms_cv_wake_source)) {
		if (vbat_uv < (chip->dt.cfg_max_voltage_uv -
			VBATT_ERROR_MARGIN + CV_DROP_MARGIN)) {
			pr_debug("Fell below CV, releasing cv ws\n");
			chip->in_cv_state = false;
			bms_relax(&chip->vbms_cv_wake_source);
		} else if (!is_battery_charging(chip)) {
			pr_debug("charging stopped, releasing cv ws\n");
			chip->in_cv_state = false;
			bms_relax(&chip->vbms_cv_wake_source);
		}
	} else if (!bms_wake_active(&chip->vbms_cv_wake_source)
			&& is_battery_charging(chip)
			&& (vbat_uv > (chip->dt.cfg_max_voltage_uv -
					VBATT_ERROR_MARGIN))) {
		pr_debug("CC_TO_CV voltage = %d holding cv ws\n", vbat_uv);
		chip->in_cv_state = true;
		bms_stay_awake(&chip->vbms_cv_wake_source);
	}
}

static int report_eoc(struct qpnp_bms_chip *chip)
{
	int rc = 0;
	union power_supply_propval ret;

	ret.intval = POWER_SUPPLY_STATUS_FULL;

	if (chip->batt_psy == NULL)
		chip->batt_psy = power_supply_get_by_name("battery");
	if (chip->batt_psy)
		rc = chip->batt_psy->set_property(chip->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &ret);
	else
		pr_err("battery psy not registered\n");

	return rc;
}

#define OCV_EOC_MARGIN		25000
static void check_eoc_condition(struct qpnp_bms_chip *chip)
{
	/*
	 * Store the OCV value at 100. If the new ocv is with in
	 * the OCV_EOC_MARGIN of ocv_at_100 report 100% SOC.
	 */
	if (chip->ocv_at_100 == -EINVAL) {
		if (chip->calculated_soc == 100) {
			chip->ocv_at_100 = chip->last_ocv_uv;
			pr_debug("Battery FULL\n");
			if (chip->dt.cfg_report_charger_eoc)
				report_eoc(chip);
		}
	} else {
		pr_debug("last_ocv_uv=%d ocv_at_100=%d\n",
			chip->last_ocv_uv, chip->ocv_at_100);
		if ((chip->last_ocv_uv + OCV_EOC_MARGIN) >= chip->ocv_at_100) {
			chip->calculated_soc = 100;
			pr_debug("last_ocv_uv within 100pc ocv margin\n");
		} else {
			pr_debug("last_ocv_uv failed 100pc ocv margin\n");
			chip->ocv_at_100 = -EINVAL;
		}
	}
}

static int calculate_soc_from_voltage(struct qpnp_bms_chip *chip)
{
	int voltage_range_uv, voltage_remaining_uv, voltage_based_soc;
	int rc, vbat_uv;

	/* check if we have the averaged fifo data */
	if (chip->voltage_soc_uv) {
		vbat_uv = chip->voltage_soc_uv;
	} else {
		rc = get_battery_voltage(chip, &vbat_uv);
		if (rc < 0) {
			pr_err("adc vbat failed err = %d\n", rc);
			return rc;
		}
		pr_debug("instant-voltage based voltage-soc\n");
	}

	voltage_range_uv = chip->dt.cfg_max_voltage_uv -
					chip->dt.cfg_v_cutoff_uv;
	voltage_remaining_uv = vbat_uv - chip->dt.cfg_v_cutoff_uv;
	voltage_based_soc = voltage_remaining_uv * 100 / voltage_range_uv;

	voltage_based_soc = clamp(voltage_based_soc, 0, 100);

	if (chip->prev_voltage_based_soc != voltage_based_soc
				&& chip->bms_psy_registered) {
		pr_debug("update bms_psy\n");
		power_supply_changed(&chip->bms_psy);
	}
	chip->prev_voltage_based_soc = voltage_based_soc;

	pr_debug("vbat used = %duv\n", vbat_uv);
	pr_debug("Calculated voltage based soc = %d\n", voltage_based_soc);

	if (voltage_based_soc == 100) {
		if (chip->dt.cfg_report_charger_eoc)
			report_eoc(chip);
	}

	return 0;
}

#define SLEEP_RECALC_INTERVAL	3
static void monitor_soc_work(struct work_struct *work)
{
	struct qpnp_bms_chip *chip = container_of(work,
				struct qpnp_bms_chip,
				monitor_soc_work.work);
	int rc, vbat_uv = 0, new_soc = 0, batt_temp;

	bms_stay_awake(&chip->vbms_soc_wake_source);

	calculate_delta_time(&chip->tm_sec, &chip->delta_time_s);
	pr_debug("elapsed_time = %d\n", chip->delta_time_s);
	if (chip->delta_time_s * 1000 >
		chip->dt.cfg_calculate_soc_ms * SLEEP_RECALC_INTERVAL) {
		chip->last_soc_unbound = true;
		pr_debug("last_soc unbound because elapsed time = %d\n",
						chip->delta_time_s);
	}

	mutex_lock(&chip->last_soc_mutex);

	if (!is_battery_present(chip)) {
		/* if battery is not preset report 100% SOC */
		pr_debug("battery gone, reporting 100\n");
		chip->last_soc_invalid = true;
		chip->last_soc = -EINVAL;
		new_soc = 100;
	} else {
		rc = get_battery_voltage(chip, &vbat_uv);
		if (rc < 0) {
			pr_err("simultaneous vbat ibat failed err = %d\n", rc);
			mutex_unlock(&chip->last_soc_mutex);
			goto out;
		}
		very_low_voltage_check(chip, vbat_uv);
		cv_voltage_check(chip, vbat_uv);

		if (chip->dt.cfg_use_voltage_soc) {
			calculate_soc_from_voltage(chip);
			mutex_unlock(&chip->last_soc_mutex);
		} else {
			rc = get_batt_therm(chip, &batt_temp);
			if (rc < 0) {
				pr_err("Unable to read batt temp rc=%d, using default=%d\n",
							rc, BMS_DEFAULT_TEMP);
				batt_temp = BMS_DEFAULT_TEMP;
			}
			if (chip->last_soc_invalid) {
				chip->last_soc_invalid = false;
				chip->last_soc = -EINVAL;
			}

			new_soc = lookup_soc_ocv(chip, chip->last_ocv_uv,
								batt_temp);

			if (chip->calculated_soc != new_soc) {
				pr_debug("SOC changed! new_soc=%d prev_soc=%d\n",
						new_soc, chip->calculated_soc);
				chip->calculated_soc = new_soc;
				/* check if we have hit EOC */
				check_eoc_condition(chip);
				mutex_unlock(&chip->last_soc_mutex);
				pr_debug("update bms_psy\n");
				power_supply_changed(&chip->bms_psy);
			} else {
				mutex_unlock(&chip->last_soc_mutex);
				report_vm_bms_soc(chip);
			}
		}
	}
out:
	schedule_delayed_work(&chip->monitor_soc_work,
			msecs_to_jiffies(get_calculation_delay_ms(chip)));

	bms_relax(&chip->vbms_soc_wake_source);
}

static void voltage_soc_timeout_work(struct work_struct *work)
{
	struct qpnp_bms_chip *chip = container_of(work,
				struct qpnp_bms_chip,
				voltage_soc_timeout_work.work);

	mutex_lock(&chip->bms_device_mutex);
	if (!chip->bms_dev_open) {
		pr_warn("BMS device not opened, using voltage based SOC\n");
		chip->dt.cfg_use_voltage_soc = true;
	}
	mutex_unlock(&chip->bms_device_mutex);
}

static int get_prop_bms_capacity(struct qpnp_bms_chip *chip)
{
	return report_state_of_charge(chip);
}

static enum power_supply_property bms_power_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_RESISTANCE_CAPACITIVE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_TEMP,
};

static int
qpnp_vm_bms_property_is_writeable(struct power_supply *psy,
				enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		return 1;
	default:
		break;
	}

	return 0;
}

static int qpnp_vm_bms_power_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct qpnp_bms_chip *chip = container_of(psy,
				struct qpnp_bms_chip, bms_psy);
	int value = 0, rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_prop_bms_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->battery_status;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		val->intval = get_prop_bms_rbatt(chip);
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_CAPACITIVE:
		if (chip->batt_data->rbatt_capacitive_mohm > 0)
			val->intval = chip->batt_data->rbatt_capacitive_mohm;
		if (chip->dt.cfg_r_conn_mohm > 0)
			val->intval += chip->dt.cfg_r_conn_mohm;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = chip->current_now;
		break;
	case POWER_SUPPLY_PROP_BATTERY_TYPE:
		val->strval = chip->batt_data->battery_type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = chip->last_ocv_uv;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = get_batt_therm(chip, &value);
		if (rc < 0)
			value = BMS_DEFAULT_TEMP;
		val->intval = value;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int qpnp_vm_bms_power_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct qpnp_bms_chip *chip = container_of(psy,
				struct qpnp_bms_chip, bms_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		chip->current_now = val->intval;
		pr_debug("IBATT = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		cancel_delayed_work_sync(&chip->monitor_soc_work);
		chip->last_ocv_uv = val->intval;
		pr_debug("OCV = %d\n", val->intval);
		monitor_soc_work(&chip->monitor_soc_work.work);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void bms_new_battery_setup(struct qpnp_bms_chip *chip)
{
	int rc;

	mutex_lock(&chip->bms_data_mutex);

	chip->last_soc_invalid = true;
	/*
	 * disable and re-enable the BMS hardware to reset
	 * the realtime-FIFO data and restart accumulation
	 */
	rc = qpnp_masked_write_base(chip, chip->base + EN_CTL_REG,
							BMS_EN_BIT, 0);
	/* delay for the BMS hardware to reset its state */
	msleep(200);
	rc |= qpnp_masked_write_base(chip, chip->base + EN_CTL_REG,
							BMS_EN_BIT, 1);
	/* delay for the BMS hardware to re-start */
	msleep(200);
	if (rc)
		pr_err("Unable to reset BMS rc=%d\n", rc);

	chip->last_ocv_uv = estimate_ocv(chip);

	memset(&chip->bms_data, 0, sizeof(chip->bms_data));

	/* update the sequence number */
	chip->bms_data.seq_num = chip->seq_num++;

	/* signal the read thread */
	chip->data_ready = 1;
	wake_up_interruptible(&chip->bms_wait_q);

	/* hold a wake lock until the read thread is scheduled */
	if (chip->bms_dev_open)
		pm_stay_awake(chip->dev);

	mutex_unlock(&chip->bms_data_mutex);
}

static void battery_insertion_check(struct qpnp_bms_chip *chip)
{
	int present = (int)is_battery_present(chip);

	if (chip->battery_present != present) {
		pr_debug("shadow_sts=%d status=%d\n",
			chip->battery_present, present);
		if (chip->battery_present != -EINVAL) {
			if (present) {
				/* new battery inserted */
				bms_new_battery_setup(chip);
				pr_debug("New battery inserted!\n");
			} else {
				/* battery removed */
				pr_debug("Battery removed\n");
			}
		}
		chip->battery_present = present;
	}
}

static void battery_status_check(struct qpnp_bms_chip *chip)
{
	int status = get_battery_status(chip);

	if (chip->battery_status != status) {
		if (status == POWER_SUPPLY_STATUS_CHARGING) {
			pr_debug("charging started\n");
			charging_began(chip);
		} else if (chip->battery_status ==
				POWER_SUPPLY_STATUS_CHARGING) {
			pr_debug("charging stopped\n");
			charging_ended(chip);
		}

		if (status == POWER_SUPPLY_STATUS_FULL) {
			pr_debug("battery full\n");
			chip->battery_full = true;
		} else if (chip->battery_status == POWER_SUPPLY_STATUS_FULL) {
			pr_debug("battery not-full anymore\n");
			chip->battery_full = false;
		}
		chip->battery_status = status;
	}
}

static void qpnp_vm_bms_ext_power_changed(struct power_supply *psy)
{
	struct qpnp_bms_chip *chip = container_of(psy, struct qpnp_bms_chip,
								bms_psy);

	pr_debug("Triggered!\n");
	battery_status_check(chip);
	battery_insertion_check(chip);
}


static void dump_bms_data(const char *func, struct qpnp_bms_chip *chip)
{
	int i;

	pr_debug("%s: fifo_count=%d acc_count=%d seq_num=%d\n",
				func, chip->bms_data.num_fifo,
				chip->bms_data.acc_count,
				chip->bms_data.seq_num);

	for (i = 0; i < chip->bms_data.num_fifo; i++)
		pr_debug("fifo=%d fifo_uv=%d sample_interval=%d sample_count=%d\n",
			i, chip->bms_data.fifo_uv[i],
			chip->bms_data.sample_interval_ms,
			chip->bms_data.sample_count);
	pr_debug("avg_acc_data=%d\n", chip->bms_data.acc_uv);
}

static int read_and_populate_fifo_data(struct qpnp_bms_chip *chip)
{
	u8 fifo_count = 0, val = 0;
	u16 fifo_data_raw[MAX_FIFO_REGS];
	int rc, i;
	int64_t voltage_soc_avg = 0;

	/* read the completed FIFO count */
	rc = qpnp_read_wrapper(chip, &val, chip->base + STATUS2_REG, 1);
	if (rc) {
		pr_err("Unable to read STATUS2 register rc=%d\n", rc);
		return rc;
	}
	fifo_count = (val & FIFO_CNT_SD_MASK) >> FIFO_CNT_SD_SHIFT;
	pr_debug("fifo_count=%d\n", fifo_count);
	if (!fifo_count) {
		pr_debug("No data in FIFO\n");
		return 0;
	}

	/* read the FIFO data */
	rc = qpnp_read_wrapper(chip, (u8 *)&fifo_data_raw,
		chip->base + FIFO_0_LSB_REG, fifo_count * 2);
	if (rc) {
		pr_err("Unable to read FIFO registers rc=%d\n", rc);
		return rc;
	}

	/* populate the structure */
	chip->bms_data.num_fifo = fifo_count;

	rc = get_sample_interval(chip, chip->current_fsm_state,
				&chip->bms_data.sample_interval_ms);
	if (rc) {
		pr_err("Unable to read state=%d sample_interval rc=%d\n",
					chip->current_fsm_state, rc);
		return rc;
	}

	rc = get_sample_count(chip, chip->current_fsm_state,
					&chip->bms_data.sample_count);
	if (rc) {
		pr_err("Unable to read state=%d sample_count rc=%d\n",
					chip->current_fsm_state, rc);
		return rc;
	}

	for (i = 0; i < fifo_count; i++) {
		chip->bms_data.fifo_uv[i] = convert_vbatt_raw_to_uv(chip,
							fifo_data_raw[i], 0);
		voltage_soc_avg += chip->bms_data.fifo_uv[i];
	}
	/* store the fifo average for voltage-based-soc */
	chip->voltage_soc_uv = div_u64(voltage_soc_avg, fifo_count);

	return 0;
}

static int read_and_populate_acc_data(struct qpnp_bms_chip *chip)
{
	int rc;
	u32 acc_data_sd = 0, acc_count_sd = 0, avg_acc_data = 0;

	/* read ACC SD count */
	rc = qpnp_read_wrapper(chip, (u8 *)&acc_count_sd,
				chip->base + ACC_CNT_SD_REG, 1);
	if (rc) {
		pr_err("Unable to read ACC_CNT_SD_REG rc=%d\n", rc);
		return rc;
	}
	if (!acc_count_sd) {
		pr_debug("No data in accumulator\n");
		return 0;
	}
	/* read ACC SD data */
	rc = qpnp_read_wrapper(chip, (u8 *)&acc_data_sd,
				chip->base + ACC_DATA0_SD_REG, 3);
	if (rc) {
		pr_err("Unable to read ACC_DATA0_SD_REG rc=%d\n", rc);
		return rc;
	}
	avg_acc_data = div_u64(acc_data_sd, acc_count_sd);

	chip->bms_data.acc_uv = convert_vbatt_raw_to_uv(chip,
						avg_acc_data, 0);
	chip->bms_data.acc_count = acc_count_sd;

	rc = get_sample_interval(chip, chip->current_fsm_state,
				&chip->bms_data.sample_interval_ms);
	if (rc) {
		pr_err("Unable to read state=%d sample_interval rc=%d\n",
					chip->current_fsm_state, rc);
		return rc;
	}

	rc = get_sample_count(chip, chip->current_fsm_state,
				&chip->bms_data.sample_count);
	if (rc) {
		pr_err("Unable to read state=%d sample_count rc=%d\n",
					chip->current_fsm_state, rc);
		return rc;
	}

	return 0;
}

static int clear_fifo_acc_data(struct qpnp_bms_chip *chip)
{
	int rc;
	u8 reg = 0;

	reg = FIFO_CNT_SD_CLR_BIT | ACC_DATA_SD_CLR_BIT | ACC_CNT_SD_CLR_BIT;
	rc = qpnp_masked_write_base(chip, chip->base + DATA_CTL2_REG, reg, reg);
	if (rc)
		pr_err("Unable to write DATA_CTL2_REG rc=%d\n", rc);

	return rc;
}

static irqreturn_t bms_fifo_update_done_irq_handler(int irq, void *_chip)
{
	int rc;
	struct qpnp_bms_chip *chip = _chip;

	pr_debug("fifo_update_done triggered\n");

	mutex_lock(&chip->bms_data_mutex);

	rc = calib_vadc(chip);
	if (rc)
		pr_err("Unable to calibrate vadc rc=%d\n", rc);

	/* clear old data */
	memset(&chip->bms_data, 0, sizeof(chip->bms_data));
	/*
	 * 1. Read FIFO and populate the bms_data
	 * 2. Clear FIFO data
	 * 3. Notify userspace
	 */
	rc = update_fsm_state(chip);
	if (rc) {
		pr_err("Unable to read FSM state rc=%d\n", rc);
		goto fail_fifo;
	}
	pr_debug("fsm_state=%d\n", chip->current_fsm_state);

	rc = read_and_populate_fifo_data(chip);
	if (rc) {
		pr_err("Unable to read FIFO data rc=%d\n", rc);
		goto fail_fifo;
	}

	rc = clear_fifo_acc_data(chip);
	if (rc)
		pr_err("Unable to clear FIFO/ACC data rc=%d\n", rc);

	/* update the sequence number */
	chip->bms_data.seq_num = chip->seq_num++;

	dump_bms_data(__func__, chip);

	/* signal the read thread */
	chip->data_ready = 1;
	wake_up_interruptible(&chip->bms_wait_q);

	/* hold a wake lock until the read thread is scheduled */
	if (chip->bms_dev_open)
		pm_stay_awake(chip->dev);
fail_fifo:
	mutex_unlock(&chip->bms_data_mutex);
	return IRQ_HANDLED;
}

static irqreturn_t bms_fsm_state_change_irq_handler(int irq, void *_chip)
{
	int rc;
	struct qpnp_bms_chip *chip = _chip;

	pr_debug("fsm_state_changed triggered\n");

	mutex_lock(&chip->bms_data_mutex);

	rc = calib_vadc(chip);
	if (rc)
		pr_err("Unable to calibrate vadc rc=%d\n", rc);

	/* clear old data */
	memset(&chip->bms_data, 0, sizeof(chip->bms_data));
	/*
	 * 1. Read FIFO and ACC_DATA and populate the bms_data
	 * 2. Clear FIFO & ACC data
	 * 3. Notify userspace
	 */
	pr_debug("prev_fsm_state=%d\n", chip->current_fsm_state);

	rc = read_and_populate_fifo_data(chip);
	if (rc) {
		pr_err("Unable to read FIFO data rc=%d\n", rc);
		goto fail_state;
	}

	/* read accumulator data */
	rc = read_and_populate_acc_data(chip);
	if (rc) {
		pr_err("Unable to read ACC_SD data rc=%d\n", rc);
		goto fail_state;
	}

	rc = update_fsm_state(chip);
	if (rc) {
		pr_err("Unable to read FSM state rc=%d\n", rc);
		goto fail_state;
	}

	rc = clear_fifo_acc_data(chip);
	if (rc)
		pr_err("Unable to clear FIFO/ACC data rc=%d\n", rc);

	/* update the sequence number */
	chip->bms_data.seq_num = chip->seq_num++;

	dump_bms_data(__func__, chip);

	/* signal the read thread */
	chip->data_ready = 1;
	wake_up_interruptible(&chip->bms_wait_q);

	/* hold a wake lock until the read thread is scheduled */
	if (chip->bms_dev_open)
		pm_stay_awake(chip->dev);
fail_state:
	mutex_unlock(&chip->bms_data_mutex);
	return IRQ_HANDLED;
}

static int read_shutdown_ocv_soc(struct qpnp_bms_chip *chip)
{
	u8 stored_soc = 0;
	u16 stored_ocv = 0;
	int rc;

	rc = qpnp_read_wrapper(chip, (u8 *)&stored_ocv,
				chip->base + BMS_OCV_REG, 2);
	if (rc) {
		pr_err("failed to read addr = %d %d\n",
				chip->base + BMS_OCV_REG, rc);
		return -EINVAL;
	}

	/* if shutdwon ocv is invalid, reject shutdown soc too */
	if (!stored_ocv || (stored_ocv == OCV_INVALID)) {
		pr_debug("shutdown OCV %x - invalid\n", stored_ocv);
		chip->shutdown_ocv = OCV_INVALID;
		chip->shutdown_soc = SOC_INVALID;
		return -EINVAL;
	}
	chip->shutdown_ocv = stored_ocv;

	/*
	 * The previous SOC is stored in the first 7 bits of the register as
	 * (Shutdown SOC + 1). This allows for register reset values of both
	 * 0x00 and 0xFF.
	 */
	rc = qpnp_read_wrapper(chip, &stored_soc, chip->base + BMS_SOC_REG, 1);
	if (rc) {
		pr_err("failed to read addr = %d %d\n",
				chip->base + BMS_SOC_REG, rc);
		return -EINVAL;
	}

	if (!stored_soc || stored_soc == SOC_INVALID) {
		chip->shutdown_soc = SOC_INVALID;
		chip->shutdown_ocv = OCV_INVALID;
		return -EINVAL;
	} else {
		chip->shutdown_soc = (stored_soc >> 1) - 1;
	}

	pr_debug("shutdown OCV=%x shutdown_soc=%d\n",
			chip->shutdown_ocv, chip->shutdown_soc);

	return 0;
}

static int calculate_initial_soc(struct qpnp_bms_chip *chip)
{
	int rc, batt_temp = 0, est_ocv = 0, shutdown_soc = 0;
	int shutdown_soc_invalid = 0;

	rc = get_batt_therm(chip, &batt_temp);
	if (rc < 0) {
		pr_err("Unable to read batt temp, using default=%d\n",
						BMS_DEFAULT_TEMP);
		batt_temp = BMS_DEFAULT_TEMP;
	}

	rc = read_and_update_ocv(chip, batt_temp, true);
	if (rc) {
		pr_err("Unable to read PON OCV rc=%d\n", rc);
		return rc;
	}

	rc = read_shutdown_ocv_soc(chip);
	if (rc < 0  || chip->dt.cfg_ignore_shutdown_soc)
		shutdown_soc_invalid = 1;

	if (chip->warm_reset) {
		/*
		 * if we have powered on from warm reset -
		 * Always use shutdown SOC. If shudown SOC is invalid then
		 * estimate OCV
		 */
		if (shutdown_soc_invalid) {
			pr_debug("Estimate OCV\n");
			est_ocv = estimate_ocv(chip);
			if (est_ocv <= 0) {
				pr_err("Unable to estimate OCV rc=%d\n",
								est_ocv);
				return -EINVAL;
			}
			chip->last_ocv_uv = est_ocv;
			chip->calculated_soc = lookup_soc_ocv(chip, est_ocv,
								batt_temp);
		} else {
			chip->last_ocv_uv = chip->shutdown_ocv;
			chip->calculated_soc = chip->shutdown_soc;
			pr_debug("Using shutdown SOC\n");
		}
	} else {
		 /* !warm_reset use PON OCV only if shutdown SOC is invalid */
		chip->calculated_soc = lookup_soc_ocv(chip,
					chip->last_ocv_uv, batt_temp);
		if (!shutdown_soc_invalid &&
			(abs(shutdown_soc - chip->calculated_soc) <
				chip->dt.cfg_shutdown_soc_valid_limit)) {
			chip->last_ocv_uv = chip->shutdown_ocv;
			chip->calculated_soc = chip->shutdown_soc;
			pr_debug("Using shutdown SOC\n");
		} else {
			pr_debug("Using PON SOC\n");
		}
	}
	/* store the start-up OCV for voltage-based-soc */
	chip->voltage_soc_uv = chip->last_ocv_uv;

	pr_info("warm_reset=%d est_ocv=%d  shutdown_soc_invalid=%d shutdown_ocv=%d shutdown_soc=%d calculated_soc=%d last_ocv_uv=%d\n",
		chip->warm_reset, est_ocv, shutdown_soc_invalid,
		chip->shutdown_ocv, chip->shutdown_soc,
		chip->calculated_soc, chip->last_ocv_uv);

	return 0;
}

static int bms_load_hw_defaults(struct qpnp_bms_chip *chip)
{
	u8 val, interval[2], count[2];
	int rc;

	if (chip->dt.cfg_s1_sample_count >= 0 &&
		chip->dt.cfg_s1_sample_count <= MAX_SAMPLE_COUNT) {
		val = chip->dt.cfg_s1_sample_count ?
			ilog2(chip->dt.cfg_s1_sample_count) : 0;
		rc = qpnp_masked_write_base(chip,
			chip->base + S1_ACC_CNT_REG,
				ACC_CNT_MASK, val);
		pr_err("Unable to write s1 sample count rc=%d\n", rc);
		return rc;
	}

	if (chip->dt.cfg_s2_sample_count >= 0 &&
		chip->dt.cfg_s2_sample_count <= MAX_SAMPLE_COUNT) {
		val = chip->dt.cfg_s2_sample_count ?
			ilog2(chip->dt.cfg_s2_sample_count) : 0;
		rc = qpnp_masked_write_base(chip,
			chip->base + S2_ACC_CNT_REG,
				ACC_CNT_MASK, val);
		pr_err("Unable to write s2 sample count rc=%d\n", rc);
		return rc;
	}

	if (chip->dt.cfg_s1_sample_interval_ms >= 0 &&
		chip->dt.cfg_s1_sample_interval_ms <= MAX_SAMPLE_INTERVAL) {
		val = chip->dt.cfg_s1_sample_interval_ms / 10;
		rc = qpnp_write_wrapper(chip, &val,
				chip->base + S1_SAMPLE_INTVL_REG, 1);
		pr_err("Unable to write s1 sample inteval rc=%d\n", rc);
		return rc;
	}

	if (chip->dt.cfg_s2_sample_interval_ms >= 0 &&
		chip->dt.cfg_s2_sample_interval_ms <= MAX_SAMPLE_INTERVAL) {
		val = chip->dt.cfg_s2_sample_interval_ms / 10;
		rc = qpnp_write_wrapper(chip, &val,
				chip->base + S2_SAMPLE_INTVL_REG, 1);
		pr_err("Unable to write s2 sample inteval rc=%d\n", rc);
		return rc;
	}

	if (chip->dt.cfg_s1_fifo_length >= 0 &&
			chip->dt.cfg_s1_fifo_length <= MAX_FIFO_REGS) {
		rc = qpnp_masked_write_base(chip, chip->base + FIFO_LENGTH_REG,
					S1_FIFO_LENGTH_MASK,
					chip->dt.cfg_s1_fifo_length);
		pr_err("Unable to write s1 fifo length rc=%d\n", rc);
		return rc;
	}

	if (chip->dt.cfg_s2_fifo_length >= 0 &&
			chip->dt.cfg_s2_fifo_length <= MAX_FIFO_REGS) {
		rc = qpnp_masked_write_base(chip, chip->base +
			FIFO_LENGTH_REG, S2_FIFO_LENGTH_MASK,
			chip->dt.cfg_s2_fifo_length
				<< S2_FIFO_LENGTH_SHIFT);
		pr_err("Unable to write s2 fifo length rc=%d\n", rc);
		return rc;
	}

	/* read S1/S2 sample interval */
	rc = qpnp_read_wrapper(chip, interval,
			chip->base + S1_SAMPLE_INTVL_REG, 2);
	if (rc) {
		pr_err("Unable to read S1_SAMPLE_INTVL_REG rc=%d\n", rc);
		return rc;
	}

	/* read S1/S2 accumulator count threshold */
	rc = qpnp_read_wrapper(chip, count, chip->base + S1_ACC_CNT_REG, 2);
	if (rc) {
		pr_err("Unable to read S1_ACC_CNT_REG rc=%d\n", rc);
		return rc;
	}
	count[0] &= ACC_CNT_MASK;
	count[1] &= ACC_CNT_MASK;

	rc = update_fsm_state(chip);
	if (rc) {
		pr_err("Unable to read FSM state rc=%d\n", rc);
		return rc;
	}

	pr_info("s1_sample_interval=%d s2_sample_interval=%d s1_acc_threshold=%d, s2_acc_threshold=%d, initial_fsm_state=%d\n",
			interval[0] * 10, interval[1] * 10,
			count[0] ? (1 << count[0]) : 0,
			count[1] ? (1 << count[1]) : 0,
			chip->current_fsm_state);

	return 0;
}

static int vm_bms_read(struct file *file, char __user *buf, size_t count,
			  loff_t *ppos)
{
	int rc;
	struct qpnp_bms_chip *chip = file->private_data;

	if (!chip->data_ready && (file->f_flags & O_NONBLOCK)) {
		rc = -EAGAIN;
		goto fail_read;
	}

	rc = wait_event_interruptible(chip->bms_wait_q, chip->data_ready);
	if (rc) {
		pr_debug("wait failed! rc=%d\n", rc);
		goto fail_read;
	}

	if (!chip->data_ready) {
		pr_debug("No Data, false wakeup\n");
		rc = -EFAULT;
		goto fail_read;
	}

	mutex_lock(&chip->bms_data_mutex);

	if (copy_to_user(buf, &chip->bms_data, sizeof(chip->bms_data))) {
		pr_err("Failed in copy_to_user\n");
		mutex_unlock(&chip->bms_data_mutex);
		rc = -EFAULT;
		goto fail_read;
	}
	pr_debug("Data copied!!\n");
	chip->data_ready = 0;

	mutex_unlock(&chip->bms_data_mutex);
	/* wakelock-timeout for userspace to pick up */
	pm_wakeup_event(chip->dev, BMS_READ_TIMEOUT);

	return sizeof(chip->bms_data);

fail_read:
	pm_relax(chip->dev);
	return rc;
}

static int vm_bms_open(struct inode *inode, struct file *file)
{
	struct qpnp_bms_chip *chip = container_of(inode->i_cdev,
				struct qpnp_bms_chip, bms_cdev);

	mutex_lock(&chip->bms_device_mutex);

	if (chip->bms_dev_open) {
		pr_debug("BMS device already open\n");
		mutex_unlock(&chip->bms_device_mutex);
		return -EBUSY;
	}

	chip->bms_dev_open = true;
	file->private_data = chip;
	pr_debug("BMS device opened\n");

	mutex_unlock(&chip->bms_device_mutex);

	return 0;
}

static int vm_bms_release(struct inode *inode, struct file *file)
{
	struct qpnp_bms_chip *chip = container_of(inode->i_cdev,
				struct qpnp_bms_chip, bms_cdev);

	mutex_lock(&chip->bms_device_mutex);

	chip->bms_dev_open = false;
	pm_relax(chip->dev);
	pr_debug("BMS device closed\n");

	mutex_unlock(&chip->bms_device_mutex);

	return 0;
}

static const struct file_operations bms_fops = {
	.owner		= THIS_MODULE,
	.open		= vm_bms_open,
	.read		= vm_bms_read,
	.release	= vm_bms_release,
};

static void bms_init_defaults(struct qpnp_bms_chip *chip)
{
	chip->data_ready = 0;
	chip->last_ocv_raw = OCV_UNINITIALIZED;
	chip->battery_status = POWER_SUPPLY_STATUS_UNKNOWN;
	chip->battery_present = -EINVAL;
	chip->calculated_soc = -EINVAL;
	chip->last_soc = -EINVAL;
	chip->vbms_lv_wake_source.disabled = 1;
	chip->vbms_cv_wake_source.disabled = 1;
	chip->vbms_soc_wake_source.disabled = 1;
	chip->ocv_at_100 = -EINVAL;
}

#define SPMI_REQUEST_IRQ(chip, rc, irq_name)				\
do {									\
	rc = devm_request_threaded_irq(chip->dev,			\
			chip->irq_name##_irq.irq, NULL,			\
			bms_##irq_name##_irq_handler,			\
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,		\
			#irq_name, chip);				\
	if (rc < 0)							\
		pr_err("Unable to request " #irq_name " irq: %d\n", rc);\
} while (0)

#define SPMI_FIND_IRQ(chip, irq_name, rc)				\
do {									\
	chip->irq_name##_irq.irq = spmi_get_irq_byname(chip->spmi,	\
					resource, #irq_name);		\
	if (chip->irq_name##_irq.irq < 0) {				\
		rc = chip->irq_name##_irq.irq;				\
		pr_err("Unable to get " #irq_name " irq rc=%d\n", rc);	\
	}								\
} while (0)

static int bms_request_irqs(struct qpnp_bms_chip *chip)
{
	int rc;

	SPMI_REQUEST_IRQ(chip, rc, fifo_update_done);
	if (rc < 0)
		return rc;
	SPMI_REQUEST_IRQ(chip, rc, fsm_state_change);
	if (rc < 0)
		return rc;

	enable_irq_wake(chip->fifo_update_done_irq.irq);
	enable_irq_wake(chip->fsm_state_change_irq.irq);

	return 0;
}

static int bms_find_irqs(struct qpnp_bms_chip *chip,
				struct spmi_resource *resource)
{
	int rc = 0;

	SPMI_FIND_IRQ(chip, fifo_update_done, rc);
	if (rc < 0)
		return rc;
	SPMI_FIND_IRQ(chip, fsm_state_change, rc);
	if (rc < 0)
		return rc;

	return 0;
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

static int show_bms_config(struct seq_file *m, void *data)
{
	struct qpnp_bms_chip *chip = m->private;
	int s1_sample_interval, s2_sample_interval;
	int s1_sample_count, s2_sample_count;
	int s1_fifo_length, s2_fifo_length;

	get_sample_interval(chip, S1_STATE, &s1_sample_interval);
	get_sample_interval(chip, S2_STATE, &s2_sample_interval);
	get_sample_count(chip, S1_STATE, &s1_sample_count);
	get_sample_count(chip, S2_STATE, &s2_sample_count);
	get_fifo_length(chip, S1_STATE, &s1_fifo_length);
	get_fifo_length(chip, S2_STATE, &s2_fifo_length);

	seq_printf(m, "r_conn_mohm\t=\t%d\n"
			"v_cutoff_uv\t=\t%d\n"
			"max_voltage_uv\t=\t%d\n"
			"use_voltage_soc\t=\t%d\n"
			"low_soc_calc_threshold\t=\t%d\n"
			"low_soc_calculate_soc_ms\t=\t%d\n"
			"low_voltage_threshold\t=\t%d\n"
			"low_voltage_calculate_soc_ms\t=\t%d\n"
			"calculate_soc_ms\t=\t%d\n"
			"voltage_soc_timeout_ms\t=\t%d\n"
			"ignore_shutdown_soc\t=\t%d\n"
			"shutdown_soc_valid_limit\t=\t%d\n"
			"force_s3_on_suspend\t=\t%d\n"
			"force_s2_in_charging\t=\t%d\n"
			"report_charger_eoc\t=\t%d\n"
			"s1_sample_interval_ms\t=\t%d\n"
			"s2_sample_interval_ms\t=\t%d\n"
			"s1_sample_count\t=\t%d\n"
			"s2_sample_count\t=\t%d\n"
			"s1_fifo_length\t=\t%d\n"
			"s2_fifo_length\t=\t%d\n",
			chip->dt.cfg_r_conn_mohm,
			chip->dt.cfg_v_cutoff_uv,
			chip->dt.cfg_max_voltage_uv,
			chip->dt.cfg_use_voltage_soc,
			chip->dt.cfg_low_soc_calc_threshold,
			chip->dt.cfg_low_soc_calculate_soc_ms,
			chip->dt.cfg_low_voltage_threshold,
			chip->dt.cfg_low_voltage_calculate_soc_ms,
			chip->dt.cfg_calculate_soc_ms,
			chip->dt.cfg_voltage_soc_timeout_ms,
			chip->dt.cfg_ignore_shutdown_soc,
			chip->dt.cfg_shutdown_soc_valid_limit,
			chip->dt.cfg_force_s3_on_suspend,
			chip->dt.cfg_force_s2_in_charging,
			chip->dt.cfg_report_charger_eoc,
			s1_sample_interval,
			s2_sample_interval,
			s1_sample_count,
			s2_sample_count,
			s1_fifo_length,
			s2_fifo_length);

	return 0;
}

static int bms_config_open(struct inode *inode, struct file *file)
{
	struct qpnp_bms_chip *chip = inode->i_private;

	return single_open(file, show_bms_config, chip);
}

static const struct file_operations bms_config_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= bms_config_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_bms_status(struct seq_file *m, void *data)
{
	struct qpnp_bms_chip *chip = m->private;

	seq_printf(m, "bms_psy_registered\t=\t%d\n"
			"bms_dev_open\t=\t%d\n"
			"warm_reset\t=\t%d\n"
			"battery_status\t=\t%d\n"
			"battery_present\t=\t%d\n"
			"in_cv_state\t=\t%d\n"
			"calculated_soc\t=\t%d\n"
			"last_soc\t=\t%d\n"
			"last_ocv_uv\t=\t%d\n"
			"last_ocv_raw\t=\t%d\n"
			"last_soc_unbound\t=\t%d\n"
			"current_fsm_state\t=\t%d\n"
			"current_now\t=\t%d\n"
			"ocv_at_100\t=\t%d\n"
			"low_voltage_ws_active\t=\t%d\n"
			"cv_ws_active\t=\t%d\n",
			chip->bms_psy_registered,
			chip->bms_dev_open,
			chip->warm_reset,
			chip->battery_status,
			chip->battery_present,
			chip->in_cv_state,
			chip->calculated_soc,
			chip->last_soc,
			chip->last_ocv_uv,
			chip->last_ocv_raw,
			chip->last_soc_unbound,
			chip->current_fsm_state,
			chip->current_now,
			chip->ocv_at_100,
			bms_wake_active(&chip->vbms_lv_wake_source),
			bms_wake_active(&chip->vbms_cv_wake_source));
	return 0;
}

static int bms_status_open(struct inode *inode, struct file *file)
{
	struct qpnp_bms_chip *chip = inode->i_private;

	return single_open(file, show_bms_status, chip);
}

static const struct file_operations bms_status_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= bms_status_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_bms_data(struct seq_file *m, void *data)
{
	struct qpnp_bms_chip *chip = m->private;
	int i;

	mutex_lock(&chip->bms_data_mutex);

	seq_printf(m, "seq_num=%d\n", chip->bms_data.seq_num);
	for (i = 0; i < chip->bms_data.num_fifo; i++)
		seq_printf(m, "fifo_uv[%d]=%d sample_count=%d interval_ms=%d\n",
				i, chip->bms_data.fifo_uv[i],
				chip->bms_data.sample_count,
				chip->bms_data.sample_interval_ms);
	seq_printf(m, "acc_uv=%d sample_count=%d sample_interval=%d\n",
			chip->bms_data.acc_uv, chip->bms_data.acc_count,
			chip->bms_data.sample_interval_ms);

	mutex_unlock(&chip->bms_data_mutex);

	return 0;
}

static int bms_data_open(struct inode *inode, struct file *file)
{
	struct qpnp_bms_chip *chip = inode->i_private;

	return single_open(file, show_bms_data, chip);
}

static const struct file_operations bms_data_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= bms_data_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int set_battery_data(struct qpnp_bms_chip *chip)
{
	int64_t battery_id;
	int rc = 0;
	struct bms_battery_data *batt_data;
	struct device_node *node;

	battery_id = read_battery_id(chip);
	if (battery_id < 0) {
		pr_err("cannot read battery id err = %lld\n", battery_id);
		return battery_id;
	}
	node = of_find_node_by_name(chip->spmi->dev.of_node,
					"qcom,battery-data");
	if (!node) {
			pr_err("No available batterydata\n");
			return -EINVAL;
	}

	batt_data = devm_kzalloc(chip->dev,
			sizeof(struct bms_battery_data), GFP_KERNEL);
	if (!batt_data) {
		pr_err("Could not alloc battery data\n");
		return -EINVAL;
	}

	batt_data->fcc_temp_lut = devm_kzalloc(chip->dev,
		sizeof(struct single_row_lut), GFP_KERNEL);
	batt_data->pc_temp_ocv_lut = devm_kzalloc(chip->dev,
			sizeof(struct pc_temp_ocv_lut), GFP_KERNEL);
	batt_data->rbatt_sf_lut = devm_kzalloc(chip->dev,
				sizeof(struct sf_lut), GFP_KERNEL);

	batt_data->max_voltage_uv = -1;
	batt_data->cutoff_uv = -1;
	batt_data->iterm_ua = -1;

	/*
	 * if the alloced luts are 0s, of_batterydata_read_data ignores
	 * them.
	 */
	rc = of_batterydata_read_data(node, batt_data, battery_id);
	if (rc || !batt_data->pc_temp_ocv_lut
		|| !batt_data->fcc_temp_lut
		|| !batt_data->rbatt_sf_lut) {
		pr_err("battery data load failed\n");
		devm_kfree(chip->dev, batt_data->fcc_temp_lut);
		devm_kfree(chip->dev, batt_data->pc_temp_ocv_lut);
		devm_kfree(chip->dev, batt_data->rbatt_sf_lut);
		devm_kfree(chip->dev, batt_data);
		return rc;
	}

	if (batt_data->pc_temp_ocv_lut == NULL) {
		pr_err("temp ocv lut table has not been loaded\n");
		devm_kfree(chip->dev, batt_data->fcc_temp_lut);
		devm_kfree(chip->dev, batt_data->pc_temp_ocv_lut);
		devm_kfree(chip->dev, batt_data->rbatt_sf_lut);
		devm_kfree(chip->dev, batt_data);

		return -EINVAL;
	}

	/* Override battery properties if specified in the battery profile */
	if (batt_data->max_voltage_uv >= 0)
		chip->dt.cfg_max_voltage_uv = batt_data->max_voltage_uv;
	if (batt_data->cutoff_uv >= 0)
		chip->dt.cfg_v_cutoff_uv = batt_data->cutoff_uv;

	chip->batt_data = batt_data;

	return 0;
}

static int parse_spmi_dt_properties(struct qpnp_bms_chip *chip,
				struct spmi_device *spmi)
{
	struct spmi_resource *spmi_resource;
	struct resource *resource;
	int rc;

	chip->dev = &(spmi->dev);
	chip->spmi = spmi;

	spmi_for_each_container_dev(spmi_resource, spmi) {
		if (!spmi_resource) {
			pr_err("qpnp_vm_bms: spmi resource absent\n");
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
		}

		if (strcmp("qcom,qpnp-chg-pres",
					spmi_resource->of_node->name) == 0) {
			chip->chg_pres_addr = resource->start;
			continue;
		}

		chip->base = resource->start;
		rc = bms_find_irqs(chip, spmi_resource);
		if (rc) {
			pr_err("Could not find irqs rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->base == 0) {
		dev_err(&spmi->dev, "BMS peripheral was not registered\n");
		return -EINVAL;
	}

	pr_debug("bms-base = 0x%04x, bat-pres-reg = 0x%04x\n",
				chip->base, chip->batt_pres_addr);

	return 0;
}

#define SPMI_PROP_READ(chip_prop, qpnp_spmi_property, retval)		\
do {									\
	if (retval)							\
		break;							\
	retval = of_property_read_u32(chip->spmi->dev.of_node,		\
				"qcom," qpnp_spmi_property,		\
					&chip->dt.chip_prop);		\
	if (retval) {							\
		pr_err("Error reading " #qpnp_spmi_property		\
					" property %d\n", retval);	\
	}								\
} while (0)

#define SPMI_PROP_READ_OPTIONAL(chip_prop, qpnp_spmi_property, retval)	\
do {									\
	retval = of_property_read_u32(chip->spmi->dev.of_node,		\
				"qcom," qpnp_spmi_property,		\
					&chip->dt.chip_prop);		\
	if (retval)							\
		chip->dt.chip_prop = -EINVAL;				\
} while (0)

static int parse_bms_dt_properties(struct qpnp_bms_chip *chip)
{
	int rc = 0;

	SPMI_PROP_READ(cfg_v_cutoff_uv, "v-cutoff-uv", rc);
	SPMI_PROP_READ(cfg_max_voltage_uv, "max-voltage-uv", rc);
	SPMI_PROP_READ(cfg_r_conn_mohm, "r-conn-mohm", rc);
	SPMI_PROP_READ(cfg_shutdown_soc_valid_limit,
			"shutdown-soc-valid-limit", rc);
	SPMI_PROP_READ(cfg_low_soc_calc_threshold,
			"low-soc-calculate-soc-threshold", rc);
	SPMI_PROP_READ(cfg_low_soc_calculate_soc_ms,
			"low-soc-calculate-soc-ms", rc);
	SPMI_PROP_READ(cfg_low_voltage_calculate_soc_ms,
			"low-voltage-calculate-soc-ms", rc);
	SPMI_PROP_READ(cfg_calculate_soc_ms, "calculate-soc-ms", rc);
	SPMI_PROP_READ(cfg_low_voltage_threshold, "low-voltage-threshold", rc);
	SPMI_PROP_READ(cfg_voltage_soc_timeout_ms,
			"volatge-soc-timeout-ms", rc);

	if (rc) {
		pr_err("Missing required properties rc=%d\n", rc);
		return rc;
	}

	SPMI_PROP_READ_OPTIONAL(cfg_s1_sample_interval_ms,
				"s1-sample-interval-ms", rc);
	SPMI_PROP_READ_OPTIONAL(cfg_s2_sample_interval_ms,
				"s2-sample-interval-ms", rc);
	SPMI_PROP_READ_OPTIONAL(cfg_s1_sample_count, "s1-sample-count", rc);
	SPMI_PROP_READ_OPTIONAL(cfg_s2_sample_count, "s2-sample-count", rc);
	SPMI_PROP_READ_OPTIONAL(cfg_s1_fifo_length, "s1-fifo-length", rc);
	SPMI_PROP_READ_OPTIONAL(cfg_s2_fifo_length, "s2-fifo-length", rc);

	chip->dt.cfg_ignore_shutdown_soc = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,ignore-shutdown-soc");
	chip->dt.cfg_use_voltage_soc = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,use-voltage-soc");
	chip->dt.cfg_force_s3_on_suspend = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,force-s3-on-suspend");
	chip->dt.cfg_report_charger_eoc = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,report-charger-eoc");
	chip->dt.cfg_force_s2_in_charging = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,force-s2-in-charging");

	pr_debug("v_cutoff_uv:%d, max_v:%d\n", chip->dt.cfg_v_cutoff_uv,
					chip->dt.cfg_max_voltage_uv);
	pr_debug("r_conn:%d, shutdown_soc_valid_limit: %d\n",
					chip->dt.cfg_r_conn_mohm,
			chip->dt.cfg_shutdown_soc_valid_limit);
	pr_debug("ignore_shutdown_soc:%d, use_voltage_soc:%d\n",
				chip->dt.cfg_ignore_shutdown_soc,
				chip->dt.cfg_use_voltage_soc);

	return 0;
}

static int bms_get_adc(struct qpnp_bms_chip *chip,
				struct spmi_device *spmi)
{
	int rc = 0;

	chip->vadc_dev = qpnp_get_vadc(&spmi->dev, "bms");
	if (IS_ERR(chip->vadc_dev)) {
		rc = PTR_ERR(chip->vadc_dev);
		if (rc == -EPROBE_DEFER)
			pr_err("vadc not found - defer probe rc=%d\n", rc);
		else
			pr_err("vadc property missing, rc=%d\n", rc);
	}

	return rc;
}

static int register_bms_char_device(struct qpnp_bms_chip *chip)
{
	int rc;

	rc = alloc_chrdev_region(&chip->dev_no, 0, 1, "vm_bms");
	if (rc) {
		pr_err("Unable to allocate chrdev rc=%d\n", rc);
		return rc;
	}
	cdev_init(&chip->bms_cdev, &bms_fops);
	rc = cdev_add(&chip->bms_cdev, chip->dev_no, 1);
	if (rc) {
		pr_err("Unable to add bms_cdev rc=%d\n", rc);
		goto unregister_chrdev;
	}

	chip->bms_class = class_create(THIS_MODULE, "vm_bms");
	if (IS_ERR_OR_NULL(chip->bms_class)) {
		pr_err("Fail to create bms class\n");
		rc = -EINVAL;
		goto delete_cdev;
	}
	chip->bms_device = device_create(chip->bms_class,
					NULL, chip->dev_no,
					NULL, "vm_bms");
	if (IS_ERR(chip->bms_device)) {
		pr_err("Fail to create bms_device device\n");
		rc = -EINVAL;
		goto delete_cdev;
	}

	return 0;

delete_cdev:
	cdev_del(&chip->bms_cdev);
unregister_chrdev:
	unregister_chrdev_region(chip->dev_no, 1);
	return rc;
}

static int qpnp_vm_bms_probe(struct spmi_device *spmi)
{
	struct qpnp_bms_chip *chip;
	int rc, vbatt = 0;
	u8 reg = 0;

	chip = devm_kzalloc(&spmi->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}

	rc = bms_get_adc(chip, spmi);
	if (rc < 0) {
		pr_err("Failed to get adc rc=%d\n", rc);
		return rc;
	}

	rc = qpnp_pon_is_warm_reset();
	if (rc < 0) {
		pr_err("Error reading warm reset status rc=%d\n", rc);
		return rc;
	}
	chip->warm_reset = !!rc;

	rc = parse_spmi_dt_properties(chip, spmi);
	if (rc) {
		pr_err("Error registering spmi resource rc=%d\n", rc);
		return rc;
	}

	rc = parse_bms_dt_properties(chip);
	if (rc) {
		pr_err("Unable to read all bms properties, rc = %d\n", rc);
		return rc;
	}

	if (chip->chg_pres_addr) {
		/* check if we have an external charger */
		rc = qpnp_read_wrapper(chip, &reg, chip->chg_pres_addr, 1);
		if (rc) {
			pr_err("Error reading chg_pres register rc=%d\n", rc);
			return rc;
		}
		if (!(reg & QPNP_CHARGER_PRESENT)) {
			pr_info("External charger, VM BMS not supported\n");
			devm_kfree(&spmi->dev, chip);
			return 0;
		}
	}

	rc = qpnp_read_wrapper(chip, chip->revision,
				chip->base + REVISION1_REG, 2);
	if (rc) {
		pr_err("Error reading version register rc=%d\n", rc);
		return rc;
	}

	pr_debug("BMS version: %hhu.%hhu\n",
			chip->revision[1], chip->revision[0]);

	dev_set_drvdata(&spmi->dev, chip);
	device_init_wakeup(&spmi->dev, 1);
	mutex_init(&chip->bms_data_mutex);
	mutex_init(&chip->bms_device_mutex);
	mutex_init(&chip->last_soc_mutex);
	init_waitqueue_head(&chip->bms_wait_q);

	/* read battery-id and select the battery profile */
	rc = set_battery_data(chip);
	if (rc) {
		pr_err("Unable to read battery data %d\n", rc);
		return rc;
	}

	/* set the battery profile */
	rc = config_battery_data(chip->batt_data);
	if (rc) {
		pr_err("Unable to config battery data %d\n", rc);
		return rc;
	}

	wakeup_source_init(&chip->vbms_lv_wake_source.source, "vbms_lv_wake");
	wakeup_source_init(&chip->vbms_cv_wake_source.source, "vbms_cv_wake");
	wakeup_source_init(&chip->vbms_soc_wake_source.source, "vbms_soc_wake");
	INIT_DELAYED_WORK(&chip->monitor_soc_work, monitor_soc_work);
	INIT_DELAYED_WORK(&chip->voltage_soc_timeout_work,
					voltage_soc_timeout_work);

	bms_init_defaults(chip);
	bms_load_hw_defaults(chip);
	battery_status_check(chip);

	/* character device to pass data to the userspace */
	rc = register_bms_char_device(chip);
	if (rc) {
		pr_err("Unable to regiter '/dev/vm_bms' rc=%d\n", rc);
		goto fail_bms_device;
	}

	the_chip = chip;
	calculate_initial_soc(chip);

	/* setup & register the battery power supply */
	chip->bms_psy.name = "bms";
	chip->bms_psy.type = POWER_SUPPLY_TYPE_BMS;
	chip->bms_psy.properties = bms_power_props;
	chip->bms_psy.num_properties = ARRAY_SIZE(bms_power_props);
	chip->bms_psy.get_property = qpnp_vm_bms_power_get_property;
	chip->bms_psy.set_property = qpnp_vm_bms_power_set_property;
	chip->bms_psy.external_power_changed = qpnp_vm_bms_ext_power_changed;
	chip->bms_psy.property_is_writeable = qpnp_vm_bms_property_is_writeable;
	chip->bms_psy.supplied_to = qpnp_vm_bms_supplicants;
	chip->bms_psy.num_supplicants = ARRAY_SIZE(qpnp_vm_bms_supplicants);

	rc = power_supply_register(chip->dev, &chip->bms_psy);
	if (rc < 0) {
		pr_err("power_supply_register bms failed rc = %d\n", rc);
		goto fail_psy;
	}
	chip->bms_psy_registered = true;

	rc = get_battery_voltage(chip, &vbatt);
	if (rc) {
		pr_err("error reading vbat_sns adc channel=%d, rc=%d\n",
							VBAT_SNS, rc);
		goto fail_get_vtg;
	}

	rc = bms_request_irqs(chip);
	if (rc) {
		pr_err("error requesting bms irqs, rc = %d\n", rc);
		goto fail_get_vtg;
	}

	chip->debug_root = debugfs_create_dir("qpnp_vmbms", NULL);
	if (!chip->debug_root)
		pr_err("Couldn't create debug dir\n");

	if (chip->debug_root) {
		struct dentry *ent;

		ent = debugfs_create_file("bms_data", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &bms_data_debugfs_ops);
		if (!ent)
			pr_err("Couldn't create bms_data debug file\n");

		ent = debugfs_create_file("bms_config", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &bms_config_debugfs_ops);
		if (!ent)
			pr_err("Couldn't create bms_config debug file\n");

		ent = debugfs_create_file("bms_status", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &bms_status_debugfs_ops);
		if (!ent)
			pr_err("Couldn't create bms_status debug file\n");
	}

	schedule_delayed_work(&chip->monitor_soc_work,
			msecs_to_jiffies(get_calculation_delay_ms(chip)));

	/*
	 * schedule a work to check if the userspace vmbms module
	 * has registered. Fall-back to voltage-based-soc reporting
	 * if it has not.
	 */
	schedule_delayed_work(&chip->voltage_soc_timeout_work,
		msecs_to_jiffies(chip->dt.cfg_voltage_soc_timeout_ms));

	pr_info("probe success: soc=%d vbatt=%d ocv=%d warm_reset=%d\n",
					get_prop_bms_capacity(chip), vbatt,
					chip->last_ocv_uv, chip->warm_reset);

	return rc;

fail_get_vtg:
	power_supply_unregister(&chip->bms_psy);
fail_psy:
	device_destroy(chip->bms_class, chip->dev_no);
	cdev_del(&chip->bms_cdev);
	unregister_chrdev_region(chip->dev_no, 1);
fail_bms_device:
	chip->bms_psy_registered = false;
	the_chip = NULL;
	return rc;
}

static int qpnp_vm_bms_remove(struct spmi_device *spmi)
{
	struct qpnp_bms_chip *chip = dev_get_drvdata(&spmi->dev);

	cancel_delayed_work_sync(&chip->monitor_soc_work);
	debugfs_remove_recursive(chip->debug_root);
	device_destroy(chip->bms_class, chip->dev_no);
	cdev_del(&chip->bms_cdev);
	unregister_chrdev_region(chip->dev_no, 1);
	mutex_destroy(&chip->bms_data_mutex);
	mutex_destroy(&chip->last_soc_mutex);
	mutex_destroy(&chip->bms_device_mutex);
	power_supply_unregister(&chip->bms_psy);
	dev_set_drvdata(&spmi->dev, NULL);
	the_chip = NULL;

	return 0;
}

static void process_suspended_data(struct qpnp_bms_chip *chip)
{
	int rc, batt_temp = 0;
	int old_ocv = 0;
	bool update_data = false;

	rc = get_batt_therm(chip, &batt_temp);
	if (rc < 0) {
		pr_err("Unable to read batt temp, using default=%d\n",
						BMS_DEFAULT_TEMP);
		batt_temp = BMS_DEFAULT_TEMP;
	}

	mutex_lock(&chip->bms_data_mutex);
	/*
	 * We can only get a h/w OCV update when the sleep_b
	 * is low, which is possible only when APPS is suspended.
	 * So check for an OCV update only in bms_resume
	 */
	old_ocv = chip->last_ocv_uv;
	rc = read_and_update_ocv(chip, batt_temp, false);
	if (rc)
		pr_err("Unable to read/upadate OCV rc=%d\n", rc);

	if (old_ocv != chip->last_ocv_uv) {
		update_data = true;
		chip->calculated_soc = lookup_soc_ocv(chip,
				chip->last_ocv_uv, batt_temp);
		pr_debug("New OCV in sleep - SOC = %d\n",
					chip->calculated_soc);
		chip->last_soc_unbound = true;
		chip->voltage_soc_uv = chip->last_ocv_uv;
		pr_debug("update bms_psy\n");
		power_supply_changed(&chip->bms_psy);
	}

	memset(&chip->bms_data, 0, sizeof(chip->bms_data));

	/* Check if there is data to be sent */
	rc = read_and_populate_fifo_data(chip);
	if (rc)
		pr_err("Unable to read FIFO data rc=%d\n", rc);

	rc = read_and_populate_acc_data(chip);
	if (rc)
		pr_err("Unable to read ACC_SD data rc=%d\n", rc);

	rc = clear_fifo_acc_data(chip);
	if (rc)
		pr_err("Unable to clear FIFO/ACC data rc=%d\n", rc);

	rc = update_fsm_state(chip);
	if (rc)
		pr_err("Unable to read FSM state rc=%d\n", rc);

	if (chip->bms_data.num_fifo || chip->bms_data.acc_count)
		update_data = true;

	if (update_data) {
		/* there is data to be sent */
		chip->bms_data.seq_num = chip->seq_num++;
		dump_bms_data(__func__, chip);

		chip->data_ready = 1;
		wake_up_interruptible(&chip->bms_wait_q);
		if (chip->bms_dev_open)
			pm_stay_awake(chip->dev);

	}
	mutex_unlock(&chip->bms_data_mutex);
}

static int bms_suspend(struct device *dev)
{
	struct qpnp_bms_chip *chip = dev_get_drvdata(dev);

	chip->charging_while_suspended = is_battery_charging(chip);

	if (!chip->charging_while_suspended) {
		/*
		 * if we are not charging disable state
		 * change IRQ to avoid S1->S3 oscillations
		 */
		disable_irq_wake(chip->fsm_state_change_irq.irq);
		disable_bms_irq(&chip->fsm_state_change_irq);

		if (chip->dt.cfg_force_s3_on_suspend) {
			pr_debug("Forcing S3 state\n");
			force_fsm_state(chip, S3_STATE);
		}
	}

	cancel_delayed_work_sync(&chip->monitor_soc_work);

	return 0;
}

static int bms_resume(struct device *dev)
{
	struct qpnp_bms_chip *chip = dev_get_drvdata(dev);

	if (!chip->charging_while_suspended) {
		enable_bms_irq(&chip->fsm_state_change_irq);
		enable_irq_wake(chip->fsm_state_change_irq.irq);

		if (chip->dt.cfg_force_s3_on_suspend) {
			pr_debug("Unforcing S3 state\n");
			disable_force_fsm_state(chip, S3_STATE);
		}
	}

	process_suspended_data(chip);

	/* start the soc_monitor */
	bms_stay_awake(&chip->vbms_soc_wake_source);
	schedule_delayed_work(&chip->monitor_soc_work, 0);

	return 0;
}

static const struct dev_pm_ops qpnp_vm_bms_pm_ops = {
	.suspend	= bms_suspend,
	.resume		= bms_resume,
};

static struct of_device_id qpnp_vm_bms_match_table[] = {
	{ .compatible = QPNP_VM_BMS_DEV_NAME },
	{}
};

static struct spmi_driver qpnp_vm_bms_driver = {
	.probe		= qpnp_vm_bms_probe,
	.remove		= qpnp_vm_bms_remove,
	.driver		= {
		.name		= QPNP_VM_BMS_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= qpnp_vm_bms_match_table,
		.pm		= &qpnp_vm_bms_pm_ops,
	},
};

static int __init qpnp_vm_bms_init(void)
{
	return spmi_driver_register(&qpnp_vm_bms_driver);
}
module_init(qpnp_vm_bms_init);

static void __exit qpnp_vm_bms_exit(void)
{
	return spmi_driver_unregister(&qpnp_vm_bms_driver);
}
module_exit(qpnp_vm_bms_exit);

MODULE_DESCRIPTION("QPNP VM-BMS Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_VM_BMS_DEV_NAME);
