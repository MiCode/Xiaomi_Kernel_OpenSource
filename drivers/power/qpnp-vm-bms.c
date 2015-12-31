/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <linux/qpnp/qpnp-revid.h>
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

#define DATA_CTL1_REG			0x42
#define MASTER_HOLD_BIT			BIT(0)

#define DATA_CTL2_REG			0x43
#define FIFO_CNT_SD_CLR_BIT		BIT(2)
#define ACC_DATA_SD_CLR_BIT		BIT(1)
#define ACC_CNT_SD_CLR_BIT		BIT(0)

#define S3_OCV_TOL_CTL_REG		0x44

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

#define CHARGE_INCREASE_STORAGE		0xB3
#define CHARGE_CYCLE_STORAGE_LSB	0xB4 /* B4 & B5 */

#define SEC_ACCESS			0xD0

#define QPNP_CHARGER_PRESENT		BIT(7)

/* Constants */
#define OCV_TOL_LSB_UV			300
#define MAX_OCV_TOL_THRESHOLD		(OCV_TOL_LSB_UV * 0xFF)
#define MAX_SAMPLE_COUNT		256
#define MAX_SAMPLE_INTERVAL		2550
#define BMS_READ_TIMEOUT		500
#define BMS_DEFAULT_TEMP		250
#define OCV_INVALID			0xFFFF
#define SOC_INVALID			0xFF
#define OCV_UNINITIALIZED		0xFFFF
#define VBATT_ERROR_MARGIN		20000
#define CV_DROP_MARGIN			10000
#define MIN_OCV_UV			2000000
#define TIME_PER_PERCENT_UUC		60
#define IAVG_SAMPLES			16
#define MIN_SOC_UUC			3

#define QPNP_VM_BMS_DEV_NAME		"qcom,qpnp-vm-bms"

/* indicates the state of BMS */
enum {
	IDLE_STATE,
	S1_STATE,
	S2_STATE,
	S3_STATE,
	S7_STATE,
};

enum {
	WRKARND_PON_OCV_COMP = BIT(0),
};

struct bms_irq {
	int		irq;
	unsigned long	disabled;
};

struct bms_wakeup_source {
	struct wakeup_source	source;
	unsigned long		disabled;
};

struct temp_curr_comp_map {
	int temp_decideg;
	int current_ma;
};

struct bms_dt_cfg {
	bool				cfg_report_charger_eoc;
	bool				cfg_force_bms_active_on_charger;
	bool				cfg_force_s3_on_suspend;
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
	int				cfg_low_soc_fifo_length;
	int				cfg_calculate_soc_ms;
	int				cfg_voltage_soc_timeout_ms;
	int				cfg_s1_sample_interval_ms;
	int				cfg_s2_sample_interval_ms;
	int				cfg_s1_sample_count;
	int				cfg_s2_sample_count;
	int				cfg_s1_fifo_length;
	int				cfg_s2_fifo_length;
	int				cfg_disable_bms;
	int				cfg_s3_ocv_tol_uv;
	int				cfg_soc_resume_limit;
	int				cfg_low_temp_threshold;
	int				cfg_ibat_avg_samples;
	int				cfg_battery_aging_comp;
	bool				cfg_use_reported_soc;
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
	bool				apply_suspend_config;
	bool				in_cv_state;
	bool				low_soc_fifo_set;
	int				battery_status;
	int				calculated_soc;
	int				current_now;
	int				prev_current_now;
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
	int				uuc_delta_time_s;
	int				ocv_at_100;
	int				last_ocv_uv;
	int				s2_fifo_length;
	int				last_acc;
	int				hi_power_state;
	unsigned int			vadc_v0625;
	unsigned int			vadc_v1250;
	unsigned long			tm_sec;
	unsigned long			workaround_flag;
	unsigned long			uuc_tm_sec;
	u32				seq_num;
	u8				shutdown_soc;
	bool				shutdown_soc_invalid;
	u16				last_ocv_raw;
	u32				shutdown_ocv;
	bool				suspend_data_valid;
	int				iavg_num_samples;
	unsigned int			iavg_index;
	int				iavg_samples_ma[IAVG_SAMPLES];
	int				iavg_ma;
	int				prev_soc_uuc;
	int				eoc_reported;
	u8				charge_increase;
	u16				charge_cycles;
	unsigned int			start_soc;
	unsigned int			end_soc;
	unsigned int			chg_start_soc;

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
	struct mutex			state_change_mutex;
	struct class			*bms_class;
	struct device			*bms_device;
	struct cdev			bms_cdev;
	struct qpnp_vm_bms_data		bms_data;
	struct qpnp_vadc_chip		*vadc_dev;
	struct qpnp_adc_tm_chip		*adc_tm_dev;
	struct pmic_revid_data		*revid_data;
	struct qpnp_adc_tm_btm_param	vbat_monitor_params;
	struct bms_irq			fifo_update_done_irq;
	struct bms_irq			fsm_state_change_irq;
	struct power_supply		bms_psy;
	struct power_supply		*batt_psy;
	struct power_supply		*usb_psy;
	bool				reported_soc_in_use;
	bool				charger_removed_since_full;
	bool				charger_reinserted;
	bool				reported_soc_high_current;
	int				reported_soc;
	int				reported_soc_change_sec;
	int				reported_soc_delta;
};

static struct qpnp_bms_chip *the_chip;

static struct temp_curr_comp_map temp_curr_comp_lut[] = {
			{-300, 15},
			{250, 17},
			{850, 28},
};

static void disable_bms_irq(struct bms_irq *irq)
{
	if (!__test_and_set_bit(0, &irq->disabled)) {
		disable_irq(irq->irq);
		pr_debug("disabled irq %d\n", irq->irq);
	}
}

static void enable_bms_irq(struct bms_irq *irq)
{
	if (__test_and_clear_bit(0, &irq->disabled)) {
		enable_irq(irq->irq);
		pr_debug("enable irq %d\n", irq->irq);
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

	pr_debug("ocv_mv=%d soc=%d\n", ocv_mv, soc);

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

static bool is_charger_present(struct qpnp_bms_chip *chip)
{
	union power_supply_propval ret = {0,};

	if (chip->usb_psy == NULL)
		chip->usb_psy = power_supply_get_by_name("usb");
	if (chip->usb_psy) {
		chip->usb_psy->get_property(chip->usb_psy,
					POWER_SUPPLY_PROP_PRESENT, &ret);
		return ret.intval;
	}

	return false;
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

static bool is_battery_taper_charging(struct qpnp_bms_chip *chip)
{
	union power_supply_propval ret = {0,};

	if (chip->batt_psy == NULL)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (chip->batt_psy) {
		chip->batt_psy->get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &ret);
		return ret.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER;
	}

	return false;
}

static int master_hold_control(struct qpnp_bms_chip *chip, bool enable)
{
	u8 reg = 0;
	int rc;

	reg = enable ? MASTER_HOLD_BIT : 0;

	rc = qpnp_secure_write_wrapper(chip, &reg,
				chip->base + DATA_CTL1_REG);
	if (rc)
		pr_err("Unable to write reg=%x rc=%d\n", DATA_CTL1_REG, rc);

	return rc;
}

static int force_fsm_state(struct qpnp_bms_chip *chip, u8 state)
{
	int rc;
	u8 mode_ctl = 0;

	switch (state) {
	case S2_STATE:
		mode_ctl = (FORCE_S2_MODE | ENABLE_S2_MODE);
		break;
	case S3_STATE:
		mode_ctl = (FORCE_S3_MODE | ENABLE_S3_MODE);
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
	/* delay for the FSM state to take affect in hardware */
	usleep_range(500, 600);

	pr_debug("force_mode=%d  mode_cntl_reg=%x\n", state, mode_ctl);

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

	*count = val ? (1 << (val + 1)) : 1;

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

static int set_fifo_length(struct qpnp_bms_chip *chip,
				u8 fsm_state, u32 fifo_length)
{
	int rc;
	u8 reg, mask = 0, shift = 0;

	/* fifo_length of 1 is not supported due to a hardware issue */
	if ((fifo_length <= 1) || (fifo_length > MAX_FIFO_REGS)) {
		pr_err("Invalid FIFO length = %d\n", fifo_length);
		return -EINVAL;
	}

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

	rc = master_hold_control(chip, true);
	if (rc)
		pr_err("Unable to apply master_hold rc=%d\n", rc);

	rc = qpnp_masked_write_base(chip, chip->base + reg, mask,
					fifo_length << shift);
	if (rc)
		pr_err("Unable to set fifo length rc=%d\n", rc);

	rc = master_hold_control(chip, false);
	if (rc)
		pr_err("Unable to apply master_hold rc=%d\n", rc);

	return rc;
}

static int get_fsm_state(struct qpnp_bms_chip *chip, u8 *state)
{
	int rc;

	/*
	 * To read the STATUS1 register, write a value(any) to this register,
	 * wait for 10ms and then read the register.
	 */
	*state = 0;
	rc = qpnp_write_wrapper(chip, state, chip->base + STATUS1_REG, 1);
	if (rc) {
		pr_err("Unable to write STATUS1_REG rc=%d\n", rc);
		return rc;
	}
	usleep_range(10000, 11000);

	/* read the current FSM state */
	rc = qpnp_read_wrapper(chip, state, chip->base + STATUS1_REG, 1);
	if (rc) {
		pr_err("Unable to read STATUS1_REG rc=%d\n", rc);
		return rc;
	}
	*state = (*state & FSM_STATE_MASK) >> FSM_STATE_SHIFT;

	return rc;
}

static int update_fsm_state(struct qpnp_bms_chip *chip)
{
	u8 state = 0;
	int rc;

	mutex_lock(&chip->state_change_mutex);
	rc = get_fsm_state(chip, &state);
	if (rc) {
		pr_err("Unable to get fsm_state rc=%d\n", rc);
		goto fail_fsm;
	}

	chip->current_fsm_state = state;

fail_fsm:
	mutex_unlock(&chip->state_change_mutex);
	return rc;
}

static int backup_charge_cycle(struct qpnp_bms_chip *chip)
{
	int rc = 0;

	if (chip->charge_increase >= 0) {
		rc = qpnp_write_wrapper(chip, &chip->charge_increase,
				chip->base + CHARGE_INCREASE_STORAGE, 1);
		if (rc)
			pr_err("Unable to backup charge_increase rc=%d\n", rc);
	}

	if (chip->charge_cycles >= 0) {
		rc = qpnp_write_wrapper(chip, (u8 *)&chip->charge_cycles,
				chip->base + CHARGE_CYCLE_STORAGE_LSB, 2);
		if (rc)
			pr_err("Unable to backup charge_cycles rc=%d\n", rc);
	}

	pr_debug("%s storing charge_increase=%u charge_cycle=%u\n",
			rc ? "Unable to" : "Sucessfully",
			chip->charge_increase, chip->charge_cycles);

	return rc;
}

static int read_chgcycle_data_from_backup(struct qpnp_bms_chip *chip)
{
	int rc;
	uint16_t temp_u16 = 0;
	u8 temp_u8 = 0;

	rc = qpnp_read_wrapper(chip, &temp_u8,
			chip->base + CHARGE_INCREASE_STORAGE, 1);
	if (rc) {
		pr_err("Unable to read charge_increase rc=%d\n", rc);
		return rc;
	}

	rc = qpnp_read_wrapper(chip, (u8 *)&temp_u16,
			chip->base + CHARGE_CYCLE_STORAGE_LSB, 2);
	if (rc) {
		pr_err("Unable to read charge_cycle rc=%d\n", rc);
		return rc;
	}

	if ((temp_u8 == 0xFF) || (temp_u16 == 0xFFFF)) {
		chip->charge_cycles = 0;
		chip->charge_increase = 0;
		pr_info("rejecting aging data charge_increase=%u charge_cycle=%u\n",
				temp_u8, temp_u16);
		rc = backup_charge_cycle(chip);
		if (rc)
			pr_err("Unable to reset charge cycles rc=%d\n", rc);
	} else {
		chip->charge_increase = temp_u8;
		chip->charge_cycles = temp_u16;
	}

	pr_debug("charge_increase=%u charge_cycle=%u\n",
				chip->charge_increase, chip->charge_cycles);
	return rc;
}

static int calculate_uuc_iavg(struct qpnp_bms_chip *chip)
{
	int i;
	int iavg_ma = chip->current_now / 1000;

	/* only continue if ibat has changed */
	if (chip->current_now == chip->prev_current_now)
		goto ibat_unchanged;
	else
		chip->prev_current_now = chip->current_now;

	chip->iavg_samples_ma[chip->iavg_index] = iavg_ma;
	chip->iavg_index = (chip->iavg_index + 1) %
				chip->dt.cfg_ibat_avg_samples;
	chip->iavg_num_samples++;
	if (chip->iavg_num_samples >= chip->dt.cfg_ibat_avg_samples)
		chip->iavg_num_samples = chip->dt.cfg_ibat_avg_samples;

	if (chip->iavg_num_samples) {
		iavg_ma = 0;
		/* maintain a 16 sample average of ibat */
		for (i = 0; i < chip->iavg_num_samples; i++) {
			pr_debug("iavg_samples_ma[%d] = %d\n", i,
					chip->iavg_samples_ma[i]);
			iavg_ma += chip->iavg_samples_ma[i];
		}

		chip->iavg_ma = DIV_ROUND_CLOSEST(iavg_ma,
					chip->iavg_num_samples);
	}

ibat_unchanged:
	pr_debug("current_now_ma=%d averaged_iavg_ma=%d\n",
			chip->current_now / 1000, chip->iavg_ma);

	return chip->iavg_ma;
}

static int adjust_uuc(struct qpnp_bms_chip *chip, int soc_uuc)
{
	int max_percent_change;

	calculate_delta_time(&chip->uuc_tm_sec, &chip->uuc_delta_time_s);

	/* make sure that the UUC changes 1% at a time */
	max_percent_change = max(chip->uuc_delta_time_s
				/ TIME_PER_PERCENT_UUC, 1);

	if (chip->prev_soc_uuc == -EINVAL) {
		/* start with a minimum UUC if the initial UUC is high */
		if (soc_uuc > MIN_SOC_UUC)
			chip->prev_soc_uuc = MIN_SOC_UUC;
		else
			chip->prev_soc_uuc = soc_uuc;
	} else {
		if (abs(chip->prev_soc_uuc - soc_uuc) <= max_percent_change)
			chip->prev_soc_uuc = soc_uuc;
		else if (soc_uuc > chip->prev_soc_uuc)
			chip->prev_soc_uuc += max_percent_change;
		else
			chip->prev_soc_uuc -= max_percent_change;
	}

	pr_debug("soc_uuc=%d new_soc_uuc=%d\n", soc_uuc, chip->prev_soc_uuc);

	return chip->prev_soc_uuc;
}

static int lookup_soc_ocv(struct qpnp_bms_chip *chip, int ocv_uv, int batt_temp)
{
	int soc_ocv = 0, soc_cutoff = 0, soc_final = 0;
	int fcc, acc, soc_uuc = 0, soc_acc = 0, iavg_ma = 0;

	soc_ocv = interpolate_pc(chip->batt_data->pc_temp_ocv_lut,
					batt_temp, ocv_uv / 1000);
	soc_cutoff = interpolate_pc(chip->batt_data->pc_temp_ocv_lut,
				batt_temp, chip->dt.cfg_v_cutoff_uv / 1000);

	soc_final = DIV_ROUND_CLOSEST(100 * (soc_ocv - soc_cutoff),
							(100 - soc_cutoff));

	if (chip->batt_data->ibat_acc_lut) {
		/* Apply  ACC logic only if we discharging */
		if (chip->current_now > 0) {

			/*
			 * IBAT averaging is disabled at low temp.
			 * allowing the SOC to catcup quickly.
			 */
			if (batt_temp > chip->dt.cfg_low_temp_threshold)
				iavg_ma = calculate_uuc_iavg(chip);
			else
				iavg_ma = chip->current_now / 1000;

			fcc = interpolate_fcc(chip->batt_data->fcc_temp_lut,
								batt_temp);
			acc = interpolate_acc(chip->batt_data->ibat_acc_lut,
							batt_temp, iavg_ma);
			if (acc <= 0) {
				if (chip->last_acc)
					acc = chip->last_acc;
				else
					acc = fcc;
			}
			soc_uuc = ((fcc - acc) * 100) / fcc;

			if (batt_temp > chip->dt.cfg_low_temp_threshold)
				soc_uuc = adjust_uuc(chip, soc_uuc);

			soc_acc = DIV_ROUND_CLOSEST(100 * (soc_ocv - soc_uuc),
							(100 - soc_uuc));

			pr_debug("fcc=%d acc=%d soc_final=%d soc_uuc=%d soc_acc=%d current_now=%d iavg_ma=%d\n",
				fcc, acc, soc_final, soc_uuc,
				soc_acc, chip->current_now / 1000, iavg_ma);

			soc_final = soc_acc;
			chip->last_acc = acc;
		} else {
			/* charging - reset all the counters */
			chip->last_acc = 0;
			chip->iavg_num_samples = 0;
			chip->iavg_index = 0;
			chip->iavg_ma = 0;
			chip->prev_current_now = 0;
			chip->prev_soc_uuc = -EINVAL;
		}
	}

	soc_final = bound_soc(soc_final);

	pr_debug("soc_final=%d soc_ocv=%d soc_cutoff=%d ocv_uv=%u batt_temp=%d\n",
			soc_final, soc_ocv, soc_cutoff, ocv_uv, batt_temp);

	return soc_final;
}

#define V_PER_BIT_MUL_FACTOR	97656
#define V_PER_BIT_DIV_FACTOR	1000
#define VADC_INTRINSIC_OFFSET	0x6000
static int vadc_reading_to_uv(int reading, bool vadc_bms)
{
	int64_t value;

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

	value = (reading * V_PER_BIT_MUL_FACTOR);

	return div_u64(value, (u32)V_PER_BIT_DIV_FACTOR);
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

	pr_debug("vadc calib: 0625=%d raw (%d uv), 1250=%d raw (%d uv)\n",
			raw_0625, chip->vadc_v0625, raw_1250, chip->vadc_v1250);

	return 0;
}

static int convert_vbatt_raw_to_uv(struct qpnp_bms_chip *chip,
				u16 reading, bool is_pon_ocv)
{
	int64_t uv, vbatt;
	int rc;

	uv = vadc_reading_to_uv(reading, true);
	pr_debug("%u raw converted into %lld uv\n", reading, uv);

	uv = adjust_vbatt_reading(chip, uv);
	pr_debug("adjusted into %lld uv\n", uv);

	vbatt = uv;
	rc = qpnp_vbat_sns_comp_result(chip->vadc_dev, &uv, is_pon_ocv);
	if (rc) {
		pr_debug("Vbatt compensation failed rc = %d\n", rc);
		uv = vbatt;
	} else {
		pr_debug("temp-compensated %lld into %lld uv\n", vbatt, uv);
	}

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
	int rc, ocv_uv;
	u16 ocv_data = 0;

	/* read the BMS h/w OCV */
	rc = qpnp_read_wrapper(chip, (u8 *)&ocv_data,
				chip->base + OCV_DATA0_REG, 2);
	if (rc) {
		pr_err("Error reading ocv: rc = %d\n", rc);
		return -ENXIO;
	}

	/* check if OCV is within limits */
	ocv_uv = convert_vbatt_raw_to_uv(chip, ocv_data, is_pon_ocv);
	if (ocv_uv < MIN_OCV_UV) {
		pr_err("OCV too low or invalid (%d)- rejecting it\n", ocv_uv);
		return 0;
	}

	if ((chip->last_ocv_raw == OCV_UNINITIALIZED) ||
			(chip->last_ocv_raw != ocv_data)) {
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
	pr_debug("mvolts phy=%lld meas=0x%llx\n", adc_result.physical,
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

static int get_rbatt(struct qpnp_bms_chip *chip, int soc, int batt_temp)
{
	int rbatt_mohm, scalefactor;

	rbatt_mohm = chip->batt_data->default_rbatt_mohm;
	if (chip->batt_data->rbatt_sf_lut == NULL)  {
		pr_debug("RBATT = %d\n", rbatt_mohm);
		return rbatt_mohm;
	}

	scalefactor = interpolate_scalingfactor(chip->batt_data->rbatt_sf_lut,
						batt_temp, soc);
	rbatt_mohm = (rbatt_mohm * scalefactor) / 100;

	if (chip->dt.cfg_r_conn_mohm > 0)
		rbatt_mohm += chip->dt.cfg_r_conn_mohm;

	return rbatt_mohm;
}

static void charging_began(struct qpnp_bms_chip *chip)
{
	int rc;
	u8 state;

	mutex_lock(&chip->last_soc_mutex);

	chip->charge_start_tm_sec = 0;
	chip->catch_up_time_sec = 0;
	chip->start_soc = chip->last_soc;

	/*
	 * reset ocv_at_100 to -EINVAL to indicate
	 * start of charging.
	 */
	chip->ocv_at_100 = -EINVAL;

	mutex_unlock(&chip->last_soc_mutex);

	/*
	 * If the BMS state is not in S2, force it in S2. Such
	 * a condition can only occur if we are coming out of
	 * suspend.
	 */
	mutex_lock(&chip->state_change_mutex);
	rc = get_fsm_state(chip, &state);
	if (rc)
		pr_err("Unable to get FSM state rc=%d\n", rc);
	if (rc || (state != S2_STATE)) {
		pr_debug("Forcing S2 state\n");
		rc = force_fsm_state(chip, S2_STATE);
		if (rc)
			pr_err("Unable to set FSM state rc=%d\n", rc);
	}
	mutex_unlock(&chip->state_change_mutex);
}

static void charging_ended(struct qpnp_bms_chip *chip)
{
	u8 state;
	int rc, status = get_battery_status(chip);

	mutex_lock(&chip->last_soc_mutex);

	chip->charge_start_tm_sec = 0;
	chip->catch_up_time_sec = 0;
	chip->end_soc = chip->last_soc;

	if (status == POWER_SUPPLY_STATUS_FULL)
		chip->last_soc_invalid = true;

	mutex_unlock(&chip->last_soc_mutex);

	/*
	 * If the BMS state is not in S2, force it in S2. Such
	 * a condition can only occur if we are coming out of
	 * suspend.
	 */
	mutex_lock(&chip->state_change_mutex);
	rc = get_fsm_state(chip, &state);
	if (rc)
		pr_err("Unable to get FSM state rc=%d\n", rc);
	if (rc || (state != S2_STATE)) {
		pr_debug("Forcing S2 state\n");
		rc = force_fsm_state(chip, S2_STATE);
		if (rc)
			pr_err("Unable to set FSM state rc=%d\n", rc);
	}
	mutex_unlock(&chip->state_change_mutex);

	/* Calculate charge accumulated and update charge cycle */
	if (chip->dt.cfg_battery_aging_comp &&
				(chip->end_soc > chip->start_soc)) {
		chip->charge_increase += (chip->end_soc - chip->start_soc);
		if (chip->charge_increase > 100) {
			chip->charge_cycles++;
			chip->charge_increase %= 100;
		}
		pr_debug("start_soc=%u end_soc=%u charge_cycles=%u charge_increase=%u\n",
				chip->start_soc, chip->end_soc,
				chip->charge_cycles, chip->charge_increase);
		rc = backup_charge_cycle(chip);
		if (rc)
			pr_err("Unable to store charge cycles rc=%d\n", rc);
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
	pr_debug("cts=%d catch_up_sec=%d\n", chg_time_sec, catch_up_sec);
	if (catch_up_sec == 0)
		return new_soc;

	if (chg_time_sec > catch_up_sec)
		return new_soc;

	numerator = (catch_up_sec - chg_time_sec) * prev_soc
			+ chg_time_sec * new_soc;
	scaled_soc = numerator / catch_up_sec;

	pr_debug("cts=%d new_soc=%d prev_soc=%d scaled_soc=%d\n",
			chg_time_sec, new_soc, prev_soc, scaled_soc);

	return scaled_soc;
}

static int report_eoc(struct qpnp_bms_chip *chip)
{
	int rc = -EINVAL;
	union power_supply_propval ret = {0,};

	if (chip->batt_psy == NULL)
		chip->batt_psy = power_supply_get_by_name("battery");
	if (chip->batt_psy) {
		rc = chip->batt_psy->get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &ret);
		if (rc) {
			pr_err("Unable to get battery 'STATUS' rc=%d\n", rc);
		} else if (ret.intval != POWER_SUPPLY_STATUS_FULL) {
			pr_debug("Report EOC to charger\n");
			ret.intval = POWER_SUPPLY_STATUS_FULL;
			rc = chip->batt_psy->set_property(chip->batt_psy,
					POWER_SUPPLY_PROP_STATUS, &ret);
			if (rc) {
				pr_err("Unable to set 'STATUS' rc=%d\n", rc);
				return rc;
			}
			chip->eoc_reported = true;
		}
	} else {
		pr_err("battery psy not registered\n");
	}

	return rc;
}

static void check_recharge_condition(struct qpnp_bms_chip *chip)
{
	int rc;
	union power_supply_propval ret = {0,};
	int status = get_battery_status(chip);

	if (chip->last_soc > chip->dt.cfg_soc_resume_limit)
		return;

	if (status == POWER_SUPPLY_STATUS_UNKNOWN) {
		pr_debug("Unable to read battery status\n");
		return;
	}

	/* Report recharge to charger for SOC based resume of charging */
	if ((status != POWER_SUPPLY_STATUS_CHARGING) && chip->eoc_reported) {
		ret.intval = POWER_SUPPLY_STATUS_CHARGING;
		rc = chip->batt_psy->set_property(chip->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &ret);
		if (rc < 0) {
			pr_err("Unable to set battery property rc=%d\n", rc);
		} else {
			pr_info("soc dropped below resume_soc soc=%d resume_soc=%d, restart charging\n",
					chip->last_soc,
					chip->dt.cfg_soc_resume_limit);
			chip->eoc_reported = false;
		}
	}
}

static void check_eoc_condition(struct qpnp_bms_chip *chip)
{
	int rc;
	int status = get_battery_status(chip);
	union power_supply_propval ret = {0,};

	if (status == POWER_SUPPLY_STATUS_UNKNOWN) {
		pr_err("Unable to read battery status\n");
		return;
	}

	/*
	 * Check battery status:
	 * if last_soc is 100 and battery status is still charging
	 * reset ocv_at_100 and force reporting of eoc to charger.
	 */
	if ((chip->last_soc == 100) &&
			(status == POWER_SUPPLY_STATUS_CHARGING))
		chip->ocv_at_100 = -EINVAL;

	/*
	 * Store the OCV value at 100. If the new ocv is greater than
	 * ocv_at_100 (battery settles), update ocv_at_100. Else
	 * if the SOC drops, reset ocv_at_100.
	 */
	if (chip->ocv_at_100 == -EINVAL) {
		if (chip->last_soc == 100) {
			if (chip->dt.cfg_report_charger_eoc) {
				rc = report_eoc(chip);
				if (!rc) {
					/*
					 * update ocv_at_100 only if EOC is
					 * reported successfully.
					 */
					chip->ocv_at_100 = chip->last_ocv_uv;
					pr_debug("Battery FULL\n");
				} else {
					pr_err("Unable to report eoc rc=%d\n",
							rc);
					chip->ocv_at_100 = -EINVAL;
				}
			}
			if (chip->dt.cfg_use_reported_soc) {
				/* begin reported_soc process */
				chip->reported_soc_in_use = true;
				chip->charger_removed_since_full = false;
				chip->charger_reinserted = false;
				chip->reported_soc = 100;
				pr_debug("Begin reported_soc process\n");
			}
		}
	} else {
		if (chip->last_ocv_uv >= chip->ocv_at_100) {
			pr_debug("new_ocv(%d) > ocv_at_100(%d) maintaining SOC to 100\n",
					chip->last_ocv_uv, chip->ocv_at_100);
			chip->ocv_at_100 = chip->last_ocv_uv;
			chip->last_soc = 100;
		} else if (chip->last_soc != 100) {
			/*
			 * Report that the battery is discharging.
			 * This gets called once when the SOC falls
			 * below 100.
			 */
			if (chip->reported_soc_in_use
					&& chip->reported_soc == 100) {
				pr_debug("reported_soc=100, last_soc=%d, do not send DISCHARING status\n",
						chip->last_soc);
			} else {
				ret.intval = POWER_SUPPLY_STATUS_DISCHARGING;
				chip->batt_psy->set_property(chip->batt_psy,
					POWER_SUPPLY_PROP_STATUS, &ret);
			}
			pr_debug("SOC dropped (%d) discarding ocv_at_100\n",
							chip->last_soc);
			chip->ocv_at_100 = -EINVAL;
		}
	}
}

static int report_voltage_based_soc(struct qpnp_bms_chip *chip)
{
	pr_debug("Reported voltage based soc = %d\n",
			chip->prev_voltage_based_soc);
	return chip->prev_voltage_based_soc;
}

static int prepare_reported_soc(struct qpnp_bms_chip *chip)
{
	if (chip->charger_removed_since_full == false) {
		/*
		 * charger is not removed since full,
		 * keep reported_soc as 100 and calculate the delta soc
		 * between reported_soc and last_soc
		 */
		chip->reported_soc = 100;
		chip->reported_soc_delta = 100 - chip->last_soc;
		pr_debug("Keep at reported_soc 100, reported_soc_delta=%d, last_soc=%d\n",
						chip->reported_soc_delta,
						chip->last_soc);
	} else {
		/* charger is removed since full */
		if (chip->charger_reinserted) {
			/*
			 * charger reinserted, keep the reported_soc
			 * until it equals to last_soc.
			 */
			if (chip->reported_soc == chip->last_soc) {
				chip->reported_soc_in_use = false;
				chip->reported_soc_high_current = false;
				pr_debug("reported_soc equals to last_soc, stop reported_soc process\n");
			}
			chip->reported_soc_change_sec = 0;
		}
	}
	pr_debug("Reporting reported_soc=%d, last_soc=%d\n",
					chip->reported_soc, chip->last_soc);
	return chip->reported_soc;
}

#define SOC_CATCHUP_SEC_MAX		600
#define SOC_CATCHUP_SEC_PER_PERCENT	60
#define MAX_CATCHUP_SOC	(SOC_CATCHUP_SEC_MAX / SOC_CATCHUP_SEC_PER_PERCENT)
#define SOC_CHANGE_PER_SEC		5
static int report_vm_bms_soc(struct qpnp_bms_chip *chip)
{
	int soc, soc_change, batt_temp, rc;
	int time_since_last_change_sec = 0, charge_time_sec = 0;
	unsigned long last_change_sec;
	bool charging;

	soc = chip->calculated_soc;

	last_change_sec = chip->last_soc_change_sec;
	calculate_delta_time(&last_change_sec, &time_since_last_change_sec);

	charging = is_battery_charging(chip);

	pr_debug("charging=%d last_soc=%d last_soc_unbound=%d\n",
		charging, chip->last_soc, chip->last_soc_unbound);
	/*
	 * account for charge time - limit it to SOC_CATCHUP_SEC to
	 * avoid overflows when charging continues for extended periods
	 */
	if (charging && chip->last_soc != -EINVAL) {
		if (chip->charge_start_tm_sec == 0 ||
			(chip->catch_up_time_sec == 0 &&
				(abs(soc - chip->last_soc) >= MIN_SOC_UUC))) {
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

			chip->chg_start_soc = chip->last_soc;

			if (chip->catch_up_time_sec < 0)
				chip->catch_up_time_sec = 0;
			chip->charge_start_tm_sec = last_change_sec;

			pr_debug("chg_start_soc=%d charge_start_tm_sec=%d catch_up_time_sec=%d\n",
				chip->chg_start_soc, chip->charge_start_tm_sec,
						chip->catch_up_time_sec);
		}

		charge_time_sec = min(SOC_CATCHUP_SEC_MAX, (int)last_change_sec
				- chip->charge_start_tm_sec);

		/* end catchup if calculated soc and last soc are same */
		if (chip->last_soc == soc) {
			chip->catch_up_time_sec = 0;
			chip->chg_start_soc = chip->last_soc;
		}
	}

	if (chip->last_soc != -EINVAL) {
		/*
		 * last_soc < soc  ... if we have not been charging at all
		 * since the last time this was called, report previous SoC.
		 * Otherwise, scale and catch up.
		 */
		rc = get_batt_therm(chip, &batt_temp);
		if (rc)
			batt_temp = BMS_DEFAULT_TEMP;

		if (chip->last_soc < soc && !charging)
			soc = chip->last_soc;
		else if (chip->last_soc < soc && soc != 100)
			soc = scale_soc_while_chg(chip, charge_time_sec,
					chip->catch_up_time_sec,
					soc, chip->chg_start_soc);

		/*
		 * if the battery is close to cutoff or if the batt_temp
		 * is under the low-temp threshold allow bigger change
		 */
		if (bms_wake_active(&chip->vbms_lv_wake_source) ||
			(batt_temp <= chip->dt.cfg_low_temp_threshold))
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

	/*
	 * Check/update eoc under following condition:
	 * if there is change in soc:
	 *	soc != chip->last_soc
	 * during bootup if soc is 100:
	 */
	soc = bound_soc(soc);
	if ((soc != chip->last_soc) || (soc == 100)) {
		chip->last_soc = soc;
		check_eoc_condition(chip);
		if ((chip->dt.cfg_soc_resume_limit > 0) && !charging)
			check_recharge_condition(chip);
	}

	pr_debug("last_soc=%d calculated_soc=%d soc=%d time_since_last_change=%d\n",
			chip->last_soc, chip->calculated_soc,
			soc, time_since_last_change_sec);

	/*
	 * Backup the actual ocv (last_ocv_uv) and not the
	 * last_soc-interpolated ocv. This makes sure that
	 * the BMS algorithm always uses the correct ocv and
	 * can catch up on the last_soc (across reboots).
	 * We do not want the algorithm to be based of a wrong
	 * initial OCV.
	 */

	backup_ocv_soc(chip, chip->last_ocv_uv, chip->last_soc);

	if (chip->reported_soc_in_use)
		return prepare_reported_soc(chip);

	pr_debug("Reported SOC=%d\n", chip->last_soc);

	return chip->last_soc;
}

static int report_state_of_charge(struct qpnp_bms_chip *chip)
{
	int soc;

	mutex_lock(&chip->last_soc_mutex);

	if (chip->dt.cfg_use_voltage_soc)
		soc = report_voltage_based_soc(chip);
	else
		soc = report_vm_bms_soc(chip);

	mutex_unlock(&chip->last_soc_mutex);

	return soc;
}

static void btm_notify_vbat(enum qpnp_tm_state state, void *ctx)
{
	struct qpnp_bms_chip *chip = ctx;
	int vbat_uv;
	int rc;

	rc = get_battery_voltage(chip, &vbat_uv);
	if (rc) {
		pr_err("error reading vbat_sns adc channel=%d, rc=%d\n",
							VBAT_SNS, rc);
		goto out;
	}

	pr_debug("vbat is at %d, state is at %d\n", vbat_uv, state);

	if (state == ADC_TM_LOW_STATE) {
		pr_debug("low voltage btm notification triggered\n");
		if (vbat_uv <= (chip->vbat_monitor_params.low_thr
					+ VBATT_ERROR_MARGIN)) {
			if (!bms_wake_active(&chip->vbms_lv_wake_source))
				bms_stay_awake(&chip->vbms_lv_wake_source);

			chip->vbat_monitor_params.state_request =
						ADC_TM_HIGH_THR_ENABLE;
		} else {
			pr_debug("faulty btm trigger, discarding\n");
			goto out;
		}
	} else if (state == ADC_TM_HIGH_STATE) {
		pr_debug("high voltage btm notification triggered\n");
		if (vbat_uv > chip->vbat_monitor_params.high_thr) {
			chip->vbat_monitor_params.state_request =
						ADC_TM_LOW_THR_ENABLE;
			if (bms_wake_active(&chip->vbms_lv_wake_source))
				bms_relax(&chip->vbms_lv_wake_source);
		} else {
			pr_debug("faulty btm trigger, discarding\n");
			goto out;
		}
	} else {
		pr_debug("unknown voltage notification state: %d\n", state);
		goto out;
	}

	if (chip->bms_psy_registered)
		power_supply_changed(&chip->bms_psy);

out:
	qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
					&chip->vbat_monitor_params);
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

	if (bms_wake_active(&chip->vbms_lv_wake_source))
		bms_relax(&chip->vbms_lv_wake_source);

	return 0;
}

static int setup_vbat_monitoring(struct qpnp_bms_chip *chip)
{
	int rc;

	chip->vbat_monitor_params.low_thr =
					chip->dt.cfg_low_voltage_threshold;
	chip->vbat_monitor_params.high_thr =
					chip->dt.cfg_low_voltage_threshold
					+ VBATT_ERROR_MARGIN;
	chip->vbat_monitor_params.state_request = ADC_TM_LOW_THR_ENABLE;
	chip->vbat_monitor_params.channel = VBAT_SNS;
	chip->vbat_monitor_params.btm_ctx = chip;
	chip->vbat_monitor_params.timer_interval = ADC_MEAS1_INTERVAL_1S;
	chip->vbat_monitor_params.threshold_notification = &btm_notify_vbat;
	pr_debug("set low thr to %d and high to %d\n",
			chip->vbat_monitor_params.low_thr,
			chip->vbat_monitor_params.high_thr);

	rc = qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
						&chip->vbat_monitor_params);
	if (rc) {
		pr_err("adc-tm setup failed: %d\n", rc);
		return rc;
	}

	pr_debug("vbat monitoring setup complete\n");
	return 0;
}

static void very_low_voltage_check(struct qpnp_bms_chip *chip, int vbat_uv)
{
	if (!bms_wake_active(&chip->vbms_lv_wake_source)
		&& (vbat_uv <= chip->dt.cfg_low_voltage_threshold)) {
		pr_debug("voltage=%d holding low voltage ws\n", vbat_uv);
		bms_stay_awake(&chip->vbms_lv_wake_source);
	} else if (bms_wake_active(&chip->vbms_lv_wake_source)
		&& (vbat_uv > chip->dt.cfg_low_voltage_threshold)) {
		pr_debug("voltage=%d releasing low voltage ws\n", vbat_uv);
		bms_relax(&chip->vbms_lv_wake_source);
	}
}

static void cv_voltage_check(struct qpnp_bms_chip *chip, int vbat_uv)
{
	if (bms_wake_active(&chip->vbms_cv_wake_source)) {
		if ((vbat_uv < (chip->dt.cfg_max_voltage_uv -
				VBATT_ERROR_MARGIN + CV_DROP_MARGIN))
			&& !is_battery_taper_charging(chip)) {
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
			&& ((vbat_uv > (chip->dt.cfg_max_voltage_uv -
					VBATT_ERROR_MARGIN))
				|| is_battery_taper_charging(chip))) {
		pr_debug("CC_TO_CV voltage=%d holding cv ws\n", vbat_uv);
		chip->in_cv_state = true;
		bms_stay_awake(&chip->vbms_cv_wake_source);
	}
}

static void low_soc_check(struct qpnp_bms_chip *chip)
{
	int rc;

	if (chip->dt.cfg_low_soc_fifo_length < 1)
		return;

	mutex_lock(&chip->state_change_mutex);

	if (chip->calculated_soc <= chip->dt.cfg_low_soc_calc_threshold) {
		if (!chip->low_soc_fifo_set) {
			pr_debug("soc=%d (low-soc) setting fifo_length to %d\n",
						chip->calculated_soc,
					chip->dt.cfg_low_soc_fifo_length);
			rc = get_fifo_length(chip, S2_STATE,
						&chip->s2_fifo_length);
			if (rc) {
				pr_err("Unable to get_fifo_length rc=%d", rc);
				goto low_soc_exit;
			}
			rc = set_fifo_length(chip, S2_STATE,
					chip->dt.cfg_low_soc_fifo_length);
			if (rc) {
				pr_err("Unable to set_fifo_length rc=%d", rc);
				goto low_soc_exit;
			}
			chip->low_soc_fifo_set = true;
		}
	} else {
		if (chip->low_soc_fifo_set) {
			pr_debug("soc=%d setting back fifo_length to %d\n",
						chip->calculated_soc,
						chip->s2_fifo_length);
			rc = set_fifo_length(chip, S2_STATE,
						chip->s2_fifo_length);
			if (rc) {
				pr_err("Unable to set_fifo_length rc=%d", rc);
				goto low_soc_exit;
			}
			chip->low_soc_fifo_set = false;
		}
	}

low_soc_exit:
	mutex_unlock(&chip->state_change_mutex);
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
	pr_debug("Calculated voltage based soc=%d\n", voltage_based_soc);

	if (voltage_based_soc == 100)
		if (chip->dt.cfg_report_charger_eoc)
			report_eoc(chip);

	return 0;
}

static void calculate_reported_soc(struct qpnp_bms_chip *chip)
{
	union power_supply_propval ret = {0,};

	if (chip->last_soc < 0) {
		pr_debug("last_soc is not ready, return\n");
		return;
	}

	if (chip->reported_soc > chip->last_soc) {
		/*send DISCHARGING status if the reported_soc drops from 100 */
		if (chip->reported_soc == 100) {
			ret.intval = POWER_SUPPLY_STATUS_DISCHARGING;
			chip->batt_psy->set_property(chip->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &ret);
			pr_debug("Report discharging status, reported_soc=%d, last_soc=%d\n",
					chip->reported_soc, chip->last_soc);
		}
		/*
		* reported_soc_delta is used to prevent
		* the big change in last_soc,
		* this is not used in high current mode
		*/
		if (chip->reported_soc_delta > 0)
			chip->reported_soc_delta--;

		if (chip->reported_soc_high_current)
			chip->reported_soc--;
		else
			chip->reported_soc = chip->last_soc
					+ chip->reported_soc_delta;

		pr_debug("New reported_soc=%d, last_soc is=%d\n",
					chip->reported_soc, chip->last_soc);
	} else {
		chip->reported_soc_in_use = false;
		chip->reported_soc_high_current = false;
		pr_debug("reported_soc equals last_soc,stop reported_soc process\n");
	}
	pr_debug("bms power_supply_changed\n");
	power_supply_changed(&chip->bms_psy);
}

static int clamp_soc_based_on_voltage(struct qpnp_bms_chip *chip, int soc)
{
	int rc, vbat_uv;

	rc = get_battery_voltage(chip, &vbat_uv);
	if (rc < 0) {
		pr_err("adc vbat failed err = %d\n", rc);
		return soc;
	}

	/* only clamp when discharging */
	if (is_battery_charging(chip))
		return soc;

	if (soc <= 0 && vbat_uv > chip->dt.cfg_v_cutoff_uv) {
		pr_debug("clamping soc to 1, vbat (%d) > cutoff (%d)\n",
					vbat_uv, chip->dt.cfg_v_cutoff_uv);
		return 1;
	} else {
		pr_debug("not clamping, using soc = %d, vbat = %d and cutoff = %d\n",
				soc, vbat_uv, chip->dt.cfg_v_cutoff_uv);
		return soc;
	}
}

static void battery_voltage_check(struct qpnp_bms_chip *chip)
{
	int rc, vbat_uv = 0;

	rc = get_battery_voltage(chip, &vbat_uv);
	if (rc < 0) {
		pr_err("Failed to read battery-voltage rc=%d\n", rc);
	} else {
		very_low_voltage_check(chip, vbat_uv);
		cv_voltage_check(chip, vbat_uv);
	}
}

#define UI_SOC_CATCHUP_TIME	(60)
static void monitor_soc_work(struct work_struct *work)
{
	struct qpnp_bms_chip *chip = container_of(work,
				struct qpnp_bms_chip,
				monitor_soc_work.work);
	int rc, new_soc = 0, batt_temp;

	bms_stay_awake(&chip->vbms_soc_wake_source);

	calculate_delta_time(&chip->tm_sec, &chip->delta_time_s);
	pr_debug("elapsed_time=%d\n", chip->delta_time_s);

	mutex_lock(&chip->last_soc_mutex);

	if (!is_battery_present(chip)) {
		/* if battery is not preset report 100% SOC */
		pr_debug("battery gone, reporting 100\n");
		chip->last_soc_invalid = true;
		chip->last_soc = -EINVAL;
		new_soc = 100;
	} else {
		battery_voltage_check(chip);

		if (chip->dt.cfg_use_voltage_soc) {
			calculate_soc_from_voltage(chip);
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
			/* clamp soc due to BMS hw/sw immaturities */
			new_soc = clamp_soc_based_on_voltage(chip, new_soc);

			if (chip->calculated_soc != new_soc) {
				pr_debug("SOC changed! new_soc=%d prev_soc=%d\n",
						new_soc, chip->calculated_soc);
				chip->calculated_soc = new_soc;
				/*
				 * To recalculate the catch-up time, clear it
				 * when SOC changes.
				 */
				chip->catch_up_time_sec = 0;

				if (chip->calculated_soc == 100)
					/* update last_soc immediately */
					report_vm_bms_soc(chip);

				pr_debug("update bms_psy\n");
				power_supply_changed(&chip->bms_psy);
			} else if (chip->last_soc != chip->calculated_soc) {
				pr_debug("update bms_psy\n");
				power_supply_changed(&chip->bms_psy);
			} else {
				report_vm_bms_soc(chip);
			}
		}
		/* low SOC configuration */
		low_soc_check(chip);
	}
	/*
	 * schedule the work only if last_soc has not caught up with
	 * the calculated soc or if we are using voltage based soc
	 */
	if ((chip->last_soc != chip->calculated_soc) ||
					chip->dt.cfg_use_voltage_soc)
		schedule_delayed_work(&chip->monitor_soc_work,
			msecs_to_jiffies(get_calculation_delay_ms(chip)));

	if (chip->reported_soc_in_use && chip->charger_removed_since_full
				&& !chip->charger_reinserted) {
		/* record the elapsed time after last reported_soc change */
		chip->reported_soc_change_sec += chip->delta_time_s;
		pr_debug("reported_soc_change_sec=%d\n",
					chip->reported_soc_change_sec);

		/* above the catch up time, calculate new reported_soc */
		if (chip->reported_soc_change_sec > UI_SOC_CATCHUP_TIME) {
			calculate_reported_soc(chip);
			chip->reported_soc_change_sec = 0;
		}
	}

	mutex_unlock(&chip->last_soc_mutex);

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

static bool is_hi_power_state_requested(struct qpnp_bms_chip *chip)
{

	pr_debug("hi_power_state=0x%x\n", chip->hi_power_state);

	if (chip->hi_power_state & VMBMS_IGNORE_ALL_BIT)
		return false;
	else
		return !!chip->hi_power_state;

}

static int qpnp_vm_bms_config_power_state(struct qpnp_bms_chip *chip,
				int usecase, bool hi_power_enable)
{
	if (usecase < 0) {
		pr_err("Invalid power-usecase %x\n", usecase);
		return -EINVAL;
	}

	if (hi_power_enable)
		chip->hi_power_state |= usecase;
	else
		chip->hi_power_state &= ~usecase;

	pr_debug("hi_power_state=%x usecase=%x hi_power_enable=%d\n",
			chip->hi_power_state, usecase, hi_power_enable);

	return 0;
}

static int get_prop_bms_current_now(struct qpnp_bms_chip *chip)
{
	return chip->current_now;
}

static enum power_supply_property bms_power_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_RESISTANCE_CAPACITIVE,
	POWER_SUPPLY_PROP_RESISTANCE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_HI_POWER,
	POWER_SUPPLY_PROP_LOW_POWER,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};

static int
qpnp_vm_bms_property_is_writeable(struct power_supply *psy,
				enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
	case POWER_SUPPLY_PROP_HI_POWER:
	case POWER_SUPPLY_PROP_LOW_POWER:
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

	val->intval = 0;

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
	case POWER_SUPPLY_PROP_RESISTANCE_NOW:
		rc = get_batt_therm(chip, &value);
		if (rc < 0)
			value = BMS_DEFAULT_TEMP;
		val->intval = get_rbatt(chip, chip->calculated_soc, value);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_prop_bms_current_now(chip);
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
	case POWER_SUPPLY_PROP_HI_POWER:
		val->intval = is_hi_power_state_requested(chip);
		break;
	case POWER_SUPPLY_PROP_LOW_POWER:
		val->intval = !is_hi_power_state_requested(chip);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		if (chip->dt.cfg_battery_aging_comp)
			val->intval = chip->charge_cycles;
		else
			val->intval = -EINVAL;
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
	int rc = 0;
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
		schedule_delayed_work(&chip->monitor_soc_work, 0);
		break;
	case POWER_SUPPLY_PROP_HI_POWER:
		rc = qpnp_vm_bms_config_power_state(chip, val->intval, true);
		if (rc)
			pr_err("Unable to set power-state rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_LOW_POWER:
		rc = qpnp_vm_bms_config_power_state(chip, val->intval, false);
		if (rc)
			pr_err("Unable to set power-state rc=%d\n", rc);
		break;
	default:
		return -EINVAL;
	}
	return rc;
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
						BMS_EN_BIT, BMS_EN_BIT);
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

	/* reset aging variables */
	if (chip->dt.cfg_battery_aging_comp) {
		chip->charge_cycles = 0;
		chip->charge_increase = 0;
		rc = backup_charge_cycle(chip);
		if (rc)
			pr_err("Unable to reset aging data rc=%d\n", rc);
	}
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
				setup_vbat_monitoring(chip);
				pr_debug("New battery inserted!\n");
			} else {
				/* battery removed */
				reset_vbat_monitoring(chip);
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

#define HIGH_CURRENT_TH 2
static void reported_soc_check_status(struct qpnp_bms_chip *chip)
{
	u8 present;

	present = is_charger_present(chip);
	pr_debug("usb_present=%d\n", present);

	if (!present && !chip->charger_removed_since_full) {
		chip->charger_removed_since_full = true;
		pr_debug("reported_soc: charger removed since full\n");
		return;
	}
	if (chip->reported_soc_high_current) {
		pr_debug("reported_soc in high current mode, return\n");
		return;
	}
	if ((chip->reported_soc - chip->last_soc) >
			(100 - chip->dt.cfg_soc_resume_limit
						+ HIGH_CURRENT_TH)) {
		chip->reported_soc_high_current = true;
		chip->charger_removed_since_full = true;
		chip->charger_reinserted = false;
		pr_debug("reported_soc enters high current mode\n");
		return;
	}
	if (present && chip->charger_removed_since_full) {
		chip->charger_reinserted = true;
		pr_debug("reported_soc: charger reinserted\n");
	}
	if (!present && chip->charger_removed_since_full) {
		chip->charger_reinserted = false;
		pr_debug("reported_soc: charger removed again\n");
	}
}

static void qpnp_vm_bms_ext_power_changed(struct power_supply *psy)
{
	struct qpnp_bms_chip *chip = container_of(psy, struct qpnp_bms_chip,
								bms_psy);

	pr_debug("Triggered!\n");
	battery_status_check(chip);
	battery_insertion_check(chip);

	mutex_lock(&chip->last_soc_mutex);
	battery_voltage_check(chip);
	mutex_unlock(&chip->last_soc_mutex);

	if (chip->reported_soc_in_use)
		reported_soc_check_status(chip);
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
	u8 fifo_data_raw[MAX_FIFO_REGS * 2];
	u16 fifo_data;
	int rc, i, j;
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
	} else if (fifo_count > MAX_FIFO_REGS) {
		pr_err("Invalid fifo-length %d rejecting data\n", fifo_count);
		chip->bms_data.num_fifo = 0;
		return 0;
	}

	/* read the FIFO data */
	for (i = 0; i < fifo_count * 2; i++) {
		rc = qpnp_read_wrapper(chip, &fifo_data_raw[i],
				chip->base + FIFO_0_LSB_REG + i, 1);
		if (rc) {
			pr_err("Unable to read FIFO register(%d) rc=%d\n",
								i, rc);
			return rc;
		}
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

	for (i = 0, j = 0; i < fifo_count * 2; i = i + 2, j++) {
		fifo_data = fifo_data_raw[i] | (fifo_data_raw[i + 1] << 8);
		chip->bms_data.fifo_uv[j] = convert_vbatt_raw_to_uv(chip,
							fifo_data, 0);
		voltage_soc_avg += chip->bms_data.fifo_uv[j];
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

	if (chip->suspend_data_valid) {
		pr_debug("Suspend data not processed yet\n");
		goto fail_fifo;
	}

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

	if (chip->suspend_data_valid) {
		pr_debug("Suspend data not processed yet\n");
		goto fail_state;
	}

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
		pr_debug("shutdown OCV %d - invalid\n", stored_ocv);
		chip->shutdown_ocv = OCV_INVALID;
		chip->shutdown_soc = SOC_INVALID;
		return -EINVAL;
	}
	chip->shutdown_ocv = stored_ocv * 1000;

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

	pr_debug("shutdown_ocv=%d shutdown_soc=%d\n",
			chip->shutdown_ocv, chip->shutdown_soc);

	return 0;
}

static int interpolate_current_comp(int die_temp)
{
	int i;
	int num_rows = ARRAY_SIZE(temp_curr_comp_lut);

	if (die_temp <= (temp_curr_comp_lut[0].temp_decideg))
		return temp_curr_comp_lut[0].current_ma;

	if (die_temp >= (temp_curr_comp_lut[num_rows - 1].temp_decideg))
		return temp_curr_comp_lut[num_rows - 1].current_ma;

	for (i = 0; i < num_rows - 1; i++)
		if (die_temp  <= (temp_curr_comp_lut[i].temp_decideg))
			break;

	if (die_temp == (temp_curr_comp_lut[i].temp_decideg))
		return temp_curr_comp_lut[i].current_ma;

	return linear_interpolate(
				temp_curr_comp_lut[i - 1].current_ma,
				temp_curr_comp_lut[i - 1].temp_decideg,
				temp_curr_comp_lut[i].current_ma,
				temp_curr_comp_lut[i].temp_decideg,
				die_temp);
}

static void adjust_pon_ocv(struct qpnp_bms_chip *chip, int batt_temp)
{
	int rc, current_ma, rbatt_mohm, die_temp, delta_uv, pc;
	struct qpnp_vadc_result result;

	rc = qpnp_vadc_read(chip->vadc_dev, DIE_TEMP, &result);
	if (rc) {
		pr_err("error reading adc channel=%d, rc=%d\n", DIE_TEMP, rc);
	} else {
		pc = interpolate_pc(chip->batt_data->pc_temp_ocv_lut,
					batt_temp, chip->last_ocv_uv / 1000);
		/*
		* For pc < 2, use the rbatt of pc = 2. This is to avoid
		* the huge rbatt values at pc < 2 which can disrupt the pon_ocv
		* calculations.
		*/
		if (pc < 2)
			pc = 2;
		rbatt_mohm = get_rbatt(chip, pc, batt_temp);
		/* convert die_temp to DECIDEGC */
		die_temp = (int)result.physical / 100;
		current_ma = interpolate_current_comp(die_temp);
		delta_uv = rbatt_mohm * current_ma;
		pr_debug("PON OCV changed from %d to %d pc=%d rbatt=%d current_ma=%d die_temp=%d batt_temp=%d delta_uv=%d\n",
			chip->last_ocv_uv, chip->last_ocv_uv + delta_uv, pc,
			rbatt_mohm, current_ma, die_temp, batt_temp, delta_uv);

		chip->last_ocv_uv += delta_uv;
	}
}

static int calculate_initial_soc(struct qpnp_bms_chip *chip)
{
	int rc, batt_temp = 0, est_ocv = 0;

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
		chip->shutdown_soc_invalid = true;

	if (chip->warm_reset) {
		/*
		 * if we have powered on from warm reset -
		 * Always use shutdown SOC. If shudown SOC is invalid then
		 * estimate OCV
		 */
		if (chip->shutdown_soc_invalid) {
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
			chip->last_soc = chip->shutdown_soc;
			chip->calculated_soc = lookup_soc_ocv(chip,
						chip->shutdown_ocv, batt_temp);
			pr_debug("Using shutdown SOC\n");
		}
	} else {
		/*
		 * In PM8916 2.0 PON OCV calculation is delayed due to
		 * change in the ordering of power-on sequence of LDO6.
		 * Adjust PON OCV to include current during PON.
		 */
		if (chip->workaround_flag & WRKARND_PON_OCV_COMP)
			adjust_pon_ocv(chip, batt_temp);

		 /* !warm_reset use PON OCV only if shutdown SOC is invalid */
		chip->calculated_soc = lookup_soc_ocv(chip,
					chip->last_ocv_uv, batt_temp);
		if (!chip->shutdown_soc_invalid &&
			(abs(chip->shutdown_soc - chip->calculated_soc) <
				chip->dt.cfg_shutdown_soc_valid_limit)) {
			chip->last_ocv_uv = chip->shutdown_ocv;
			chip->last_soc = chip->shutdown_soc;
			chip->calculated_soc = lookup_soc_ocv(chip,
						chip->shutdown_ocv, batt_temp);
			pr_debug("Using shutdown SOC\n");
		} else {
			chip->shutdown_soc_invalid = true;
			pr_debug("Using PON SOC\n");
		}
	}
	/* store the start-up OCV for voltage-based-soc */
	chip->voltage_soc_uv = chip->last_ocv_uv;

	pr_info("warm_reset=%d est_ocv=%d  shutdown_soc_invalid=%d shutdown_ocv=%d shutdown_soc=%d last_soc=%d calculated_soc=%d last_ocv_uv=%d\n",
		chip->warm_reset, est_ocv, chip->shutdown_soc_invalid,
		chip->shutdown_ocv, chip->shutdown_soc, chip->last_soc,
		chip->calculated_soc, chip->last_ocv_uv);

	return 0;
}

static int calculate_initial_aging_comp(struct qpnp_bms_chip *chip)
{
	int rc;
	bool battery_removed = is_battery_replaced_in_offmode(chip);

	if (battery_removed || chip->shutdown_soc_invalid) {
		pr_info("Clearing aging data battery_removed=%d shutdown_soc_invalid=%d\n",
				battery_removed, chip->shutdown_soc_invalid);
		chip->charge_cycles = 0;
		chip->charge_increase = 0;
		rc = backup_charge_cycle(chip);
		if (rc)
			pr_err("Unable to reset aging data rc=%d\n", rc);
	} else {
		rc = read_chgcycle_data_from_backup(chip);
		if (rc)
			pr_err("Unable to read aging data rc=%d\n", rc);
	}

	pr_debug("Initial aging data charge_cycles=%u charge_increase=%u\n",
			chip->charge_cycles, chip->charge_increase);
	return rc;
}

static int bms_load_hw_defaults(struct qpnp_bms_chip *chip)
{
	u8 val, bms_en = 0;
	u32 interval[2], count[2], fifo[2];
	int rc;

	/* S3 OCV tolerence threshold */
	if (chip->dt.cfg_s3_ocv_tol_uv >= 0 &&
		chip->dt.cfg_s3_ocv_tol_uv <= MAX_OCV_TOL_THRESHOLD) {
		val = chip->dt.cfg_s3_ocv_tol_uv / OCV_TOL_LSB_UV;
		rc = qpnp_masked_write_base(chip,
			chip->base + S3_OCV_TOL_CTL_REG, 0xFF, val);
		if (rc) {
			pr_err("Unable to write s3_ocv_tol_threshold rc=%d\n",
									rc);
			return rc;
		}
	}

	/* S1 accumulator threshold */
	if (chip->dt.cfg_s1_sample_count >= 1 &&
		chip->dt.cfg_s1_sample_count <= MAX_SAMPLE_COUNT) {
		val = (chip->dt.cfg_s1_sample_count > 1) ?
			(ilog2(chip->dt.cfg_s1_sample_count) - 1) : 0;
		rc = qpnp_masked_write_base(chip,
			chip->base + S1_ACC_CNT_REG,
				ACC_CNT_MASK, val);
		if (rc) {
			pr_err("Unable to write s1 sample count rc=%d\n", rc);
			return rc;
		}
	}

	/* S2 accumulator threshold */
	if (chip->dt.cfg_s2_sample_count >= 1 &&
		chip->dt.cfg_s2_sample_count <= MAX_SAMPLE_COUNT) {
		val = (chip->dt.cfg_s2_sample_count > 1) ?
			(ilog2(chip->dt.cfg_s2_sample_count) - 1) : 0;
		rc = qpnp_masked_write_base(chip,
			chip->base + S2_ACC_CNT_REG,
				ACC_CNT_MASK, val);
		if (rc) {
			pr_err("Unable to write s2 sample count rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.cfg_s1_sample_interval_ms >= 0 &&
		chip->dt.cfg_s1_sample_interval_ms <= MAX_SAMPLE_INTERVAL) {
		val = chip->dt.cfg_s1_sample_interval_ms / 10;
		rc = qpnp_write_wrapper(chip, &val,
				chip->base + S1_SAMPLE_INTVL_REG, 1);
		if (rc) {
			pr_err("Unable to write s1 sample inteval rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.cfg_s2_sample_interval_ms >= 0 &&
		chip->dt.cfg_s2_sample_interval_ms <= MAX_SAMPLE_INTERVAL) {
		val = chip->dt.cfg_s2_sample_interval_ms / 10;
		rc = qpnp_write_wrapper(chip, &val,
				chip->base + S2_SAMPLE_INTVL_REG, 1);
		if (rc) {
			pr_err("Unable to write s2 sample inteval rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.cfg_s1_fifo_length >= 0 &&
			chip->dt.cfg_s1_fifo_length <= MAX_FIFO_REGS) {
		rc = qpnp_masked_write_base(chip, chip->base + FIFO_LENGTH_REG,
					S1_FIFO_LENGTH_MASK,
					chip->dt.cfg_s1_fifo_length);
		if (rc) {
			pr_err("Unable to write s1 fifo length rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.cfg_s2_fifo_length >= 0 &&
			chip->dt.cfg_s2_fifo_length <= MAX_FIFO_REGS) {
		rc = qpnp_masked_write_base(chip, chip->base +
			FIFO_LENGTH_REG, S2_FIFO_LENGTH_MASK,
			chip->dt.cfg_s2_fifo_length
				<< S2_FIFO_LENGTH_SHIFT);
		if (rc) {
			pr_err("Unable to write s2 fifo length rc=%d\n", rc);
			return rc;
		}
	}

	get_sample_interval(chip, S1_STATE, &interval[0]);
	get_sample_interval(chip, S2_STATE, &interval[1]);
	get_sample_count(chip, S1_STATE, &count[0]);
	get_sample_count(chip, S2_STATE, &count[1]);
	get_fifo_length(chip, S1_STATE, &fifo[0]);
	get_fifo_length(chip, S2_STATE, &fifo[1]);

	/* Force the BMS state to S2 at boot-up */
	rc = force_fsm_state(chip, S2_STATE);
	if (rc) {
		pr_err("Unable to force S2 state rc=%d\n", rc);
		return rc;
	}

	rc = qpnp_read_wrapper(chip, &bms_en, chip->base + EN_CTL_REG, 1);
	if (rc) {
		pr_err("Unable to read BMS_EN state rc=%d\n", rc);
		return rc;
	}

	rc = update_fsm_state(chip);
	if (rc) {
		pr_err("Unable to read FSM state rc=%d\n", rc);
		return rc;
	}

	pr_info("BMS_EN=%d Sample_Interval-S1=[%d]S2=[%d]  Sample_Count-S1=[%d]S2=[%d] Fifo_Length-S1=[%d]S2=[%d] FSM_state=%d\n",
				!!bms_en, interval[0], interval[1], count[0],
				count[1], fifo[0], fifo[1],
				chip->current_fsm_state);

	return 0;
}

static ssize_t vm_bms_read(struct file *file, char __user *buf, size_t count,
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
	chip->prev_soc_uuc = -EINVAL;
	chip->charge_cycles = 0;
	chip->start_soc = 0;
	chip->end_soc = 0;
	chip->charge_increase = 0;
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

	/* Disable the state change IRQ */
	disable_bms_irq(&chip->fsm_state_change_irq);
	enable_irq_wake(chip->fifo_update_done_irq.irq);

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
			"report_charger_eoc\t=\t%d\n"
			"aging_compensation\t=\t%d\n"
			"use_reported_soc\t=\t%d\n"
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
			chip->dt.cfg_report_charger_eoc,
			chip->dt.cfg_battery_aging_comp,
			chip->dt.cfg_use_reported_soc,
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
	batt_data->ibat_acc_lut = devm_kzalloc(chip->dev,
				sizeof(struct ibat_temp_acc_lut), GFP_KERNEL);

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
		|| !batt_data->rbatt_sf_lut
		|| !batt_data->ibat_acc_lut) {
		pr_err("battery data load failed\n");
		devm_kfree(chip->dev, batt_data->fcc_temp_lut);
		devm_kfree(chip->dev, batt_data->pc_temp_ocv_lut);
		devm_kfree(chip->dev, batt_data->rbatt_sf_lut);
		devm_kfree(chip->dev, batt_data->ibat_acc_lut);
		devm_kfree(chip->dev, batt_data);
		return rc;
	}

	if (batt_data->pc_temp_ocv_lut == NULL) {
		pr_err("temp ocv lut table has not been loaded\n");
		devm_kfree(chip->dev, batt_data->fcc_temp_lut);
		devm_kfree(chip->dev, batt_data->pc_temp_ocv_lut);
		devm_kfree(chip->dev, batt_data->rbatt_sf_lut);
		devm_kfree(chip->dev, batt_data->ibat_acc_lut);
		devm_kfree(chip->dev, batt_data);

		return -EINVAL;
	}

	/* check if ibat_acc_lut is valid */
	if (!batt_data->ibat_acc_lut->rows) {
		pr_info("ibat_acc_lut not present\n");
		devm_kfree(chip->dev, batt_data->ibat_acc_lut);
		batt_data->ibat_acc_lut = NULL;
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

	pr_debug("bms-base=0x%04x bat-pres-reg=0x%04x qpnp-chg-pres=0x%04x\n",
		chip->base, chip->batt_pres_addr, chip->chg_pres_addr);

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
	SPMI_PROP_READ_OPTIONAL(cfg_s3_ocv_tol_uv, "s3-ocv-tolerence-uv", rc);
	SPMI_PROP_READ_OPTIONAL(cfg_low_soc_fifo_length,
						"low-soc-fifo-length", rc);
	SPMI_PROP_READ_OPTIONAL(cfg_soc_resume_limit, "resume-soc", rc);
	SPMI_PROP_READ_OPTIONAL(cfg_low_temp_threshold,
					"low-temp-threshold", rc);
	if (rc)
		chip->dt.cfg_low_temp_threshold = 0;

	SPMI_PROP_READ_OPTIONAL(cfg_ibat_avg_samples,
					"ibat-avg-samples", rc);
	if (rc || (chip->dt.cfg_ibat_avg_samples <= 0) ||
			(chip->dt.cfg_ibat_avg_samples > IAVG_SAMPLES))
		chip->dt.cfg_ibat_avg_samples = IAVG_SAMPLES;

	chip->dt.cfg_ignore_shutdown_soc = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,ignore-shutdown-soc");
	chip->dt.cfg_use_voltage_soc = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,use-voltage-soc");
	chip->dt.cfg_force_s3_on_suspend = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,force-s3-on-suspend");
	chip->dt.cfg_report_charger_eoc = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,report-charger-eoc");
	chip->dt.cfg_disable_bms = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,disable-bms");
	chip->dt.cfg_force_bms_active_on_charger = of_property_read_bool(
			chip->spmi->dev.of_node,
			"qcom,force-bms-active-on-charger");
	chip->dt.cfg_battery_aging_comp = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,batt-aging-comp");
	chip->dt.cfg_use_reported_soc = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,use-reported-soc");
	pr_debug("v_cutoff_uv=%d, max_v=%d\n", chip->dt.cfg_v_cutoff_uv,
					chip->dt.cfg_max_voltage_uv);
	pr_debug("r_conn=%d shutdown_soc_valid_limit=%d low_temp_threshold=%d ibat_avg_samples=%d\n",
					chip->dt.cfg_r_conn_mohm,
			chip->dt.cfg_shutdown_soc_valid_limit,
			chip->dt.cfg_low_temp_threshold,
			chip->dt.cfg_ibat_avg_samples);
	pr_debug("ignore_shutdown_soc=%d, use_voltage_soc=%d low_soc_fifo_length=%d\n",
				chip->dt.cfg_ignore_shutdown_soc,
				chip->dt.cfg_use_voltage_soc,
				chip->dt.cfg_low_soc_fifo_length);
	pr_debug("force-s3-on-suspend=%d report-charger-eoc=%d disable-bms=%d disable-suspend-on-usb=%d aging_compensation=%d\n",
			chip->dt.cfg_force_s3_on_suspend,
			chip->dt.cfg_report_charger_eoc,
			chip->dt.cfg_disable_bms,
			chip->dt.cfg_force_bms_active_on_charger,
			chip->dt.cfg_battery_aging_comp);
	pr_debug("use-reported-soc is %d\n",
			chip->dt.cfg_use_reported_soc);

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

		return rc;
	}

	chip->adc_tm_dev = qpnp_get_adc_tm(&spmi->dev, "bms");
	if (IS_ERR(chip->adc_tm_dev)) {
		rc = PTR_ERR(chip->adc_tm_dev);
		if (rc == -EPROBE_DEFER)
			pr_err("adc-tm not found - defer probe rc=%d\n", rc);
		else
			pr_err("adc-tm property missing, rc=%d\n", rc);
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
	struct device_node *revid_dev_node;
	int rc, vbatt = 0;

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

	revid_dev_node = of_parse_phandle(spmi->dev.of_node,
						"qcom,pmic-revid", 0);
	if (!revid_dev_node) {
		pr_err("Missing qcom,pmic-revid property\n");
		return -EINVAL;
	}

	chip->revid_data = get_revid_data(revid_dev_node);
	if (IS_ERR(chip->revid_data)) {
		pr_err("revid error rc = %ld\n", PTR_ERR(chip->revid_data));
		return -EINVAL;
	}
	if ((chip->revid_data->pmic_subtype == PM8916_V2P0_SUBTYPE) &&
				chip->revid_data->rev4 == PM8916_V2P0_REV4)
		chip->workaround_flag |= WRKARND_PON_OCV_COMP;

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

	if (chip->dt.cfg_disable_bms) {
		pr_info("VMBMS disabled (disable-bms = 1)\n");
		rc = qpnp_masked_write_base(chip, chip->base + EN_CTL_REG,
							BMS_EN_BIT, 0);
		if (rc)
			pr_err("Unable to disable VMBMS rc=%d\n", rc);
		return -ENODEV;
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
	mutex_init(&chip->state_change_mutex);
	init_waitqueue_head(&chip->bms_wait_q);

	/* read battery-id and select the battery profile */
	rc = set_battery_data(chip);
	if (rc) {
		pr_err("Unable to read battery data %d\n", rc);
		goto fail_init;
	}

	/* set the battery profile */
	rc = config_battery_data(chip->batt_data);
	if (rc) {
		pr_err("Unable to config battery data %d\n", rc);
		goto fail_init;
	}

	wakeup_source_init(&chip->vbms_lv_wake_source.source, "vbms_lv_wake");
	wakeup_source_init(&chip->vbms_cv_wake_source.source, "vbms_cv_wake");
	wakeup_source_init(&chip->vbms_soc_wake_source.source, "vbms_soc_wake");
	INIT_DELAYED_WORK(&chip->monitor_soc_work, monitor_soc_work);
	INIT_DELAYED_WORK(&chip->voltage_soc_timeout_work,
					voltage_soc_timeout_work);

	bms_init_defaults(chip);
	bms_load_hw_defaults(chip);

	if (is_battery_present(chip)) {
		rc = setup_vbat_monitoring(chip);
		if (rc) {
			pr_err("fail to configure vbat monitoring rc=%d\n",
					rc);
			goto fail_setup;
		}
	}

	rc = bms_request_irqs(chip);
	if (rc) {
		pr_err("error requesting bms irqs, rc = %d\n", rc);
		goto fail_irq;
	}

	battery_insertion_check(chip);
	battery_status_check(chip);

	/* character device to pass data to the userspace */
	rc = register_bms_char_device(chip);
	if (rc) {
		pr_err("Unable to regiter '/dev/vm_bms' rc=%d\n", rc);
		goto fail_bms_device;
	}

	the_chip = chip;
	calculate_initial_soc(chip);
	if (chip->dt.cfg_battery_aging_comp) {
		rc = calculate_initial_aging_comp(chip);
		if (rc)
			pr_err("Unable to calculate initial aging data rc=%d\n",
					rc);
	}

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

	schedule_delayed_work(&chip->monitor_soc_work, 0);

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
fail_irq:
	reset_vbat_monitoring(chip);
fail_setup:
	wakeup_source_trash(&chip->vbms_lv_wake_source.source);
	wakeup_source_trash(&chip->vbms_cv_wake_source.source);
	wakeup_source_trash(&chip->vbms_soc_wake_source.source);
fail_init:
	mutex_destroy(&chip->bms_data_mutex);
	mutex_destroy(&chip->last_soc_mutex);
	mutex_destroy(&chip->state_change_mutex);
	mutex_destroy(&chip->bms_device_mutex);
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
	reset_vbat_monitoring(chip);
	wakeup_source_trash(&chip->vbms_lv_wake_source.source);
	wakeup_source_trash(&chip->vbms_cv_wake_source.source);
	wakeup_source_trash(&chip->vbms_soc_wake_source.source);
	mutex_destroy(&chip->bms_data_mutex);
	mutex_destroy(&chip->last_soc_mutex);
	mutex_destroy(&chip->state_change_mutex);
	mutex_destroy(&chip->bms_device_mutex);
	power_supply_unregister(&chip->bms_psy);
	dev_set_drvdata(&spmi->dev, NULL);
	the_chip = NULL;

	return 0;
}

static void process_suspend_data(struct qpnp_bms_chip *chip)
{
	int rc;

	mutex_lock(&chip->bms_data_mutex);

	chip->suspend_data_valid = false;

	memset(&chip->bms_data, 0, sizeof(chip->bms_data));

	rc = read_and_populate_fifo_data(chip);
	if (rc)
		pr_err("Unable to read FIFO data rc=%d\n", rc);

	rc = read_and_populate_acc_data(chip);
	if (rc)
		pr_err("Unable to read ACC_SD data rc=%d\n", rc);

	rc = clear_fifo_acc_data(chip);
	if (rc)
		pr_err("Unable to clear FIFO/ACC data rc=%d\n", rc);

	if (chip->bms_data.num_fifo || chip->bms_data.acc_count) {
		pr_debug("suspend data valid\n");
		chip->suspend_data_valid = true;
	}

	mutex_unlock(&chip->bms_data_mutex);
}

static void process_resume_data(struct qpnp_bms_chip *chip)
{
	int rc, batt_temp = 0;
	int old_ocv = 0;
	bool ocv_updated = false;

	rc = get_batt_therm(chip, &batt_temp);
	if (rc < 0) {
		pr_err("Unable to read batt temp, using default=%d\n",
						BMS_DEFAULT_TEMP);
		batt_temp = BMS_DEFAULT_TEMP;
	}

	mutex_lock(&chip->bms_data_mutex);
	/*
	 * We can get a h/w OCV update when the sleep_b
	 * is low, which is possible when APPS is suspended.
	 * So check for an OCV update only in bms_resume
	 */
	old_ocv = chip->last_ocv_uv;
	rc = read_and_update_ocv(chip, batt_temp, false);
	if (rc)
		pr_err("Unable to read/upadate OCV rc=%d\n", rc);

	if (old_ocv != chip->last_ocv_uv) {
		ocv_updated = true;
		/* new OCV, clear suspended data */
		chip->suspend_data_valid = false;
		memset(&chip->bms_data, 0, sizeof(chip->bms_data));
		chip->calculated_soc = lookup_soc_ocv(chip,
				chip->last_ocv_uv, batt_temp);
		pr_debug("OCV in sleep SOC=%d\n", chip->calculated_soc);
		chip->last_soc_unbound = true;
		chip->voltage_soc_uv = chip->last_ocv_uv;
		pr_debug("update bms_psy\n");
		power_supply_changed(&chip->bms_psy);
	}

	if (ocv_updated || chip->suspend_data_valid) {
		/* there is data to be sent */
		pr_debug("ocv_updated=%d suspend_data_valid=%d\n",
				ocv_updated, chip->suspend_data_valid);
		chip->bms_data.seq_num = chip->seq_num++;
		dump_bms_data(__func__, chip);

		chip->data_ready = 1;
		wake_up_interruptible(&chip->bms_wait_q);
		if (chip->bms_dev_open)
			pm_stay_awake(chip->dev);

	}
	chip->suspend_data_valid = false;
	mutex_unlock(&chip->bms_data_mutex);
}

static int bms_suspend(struct device *dev)
{
	struct qpnp_bms_chip *chip = dev_get_drvdata(dev);
	bool battery_charging = is_battery_charging(chip);
	bool hi_power_state = is_hi_power_state_requested(chip);
	bool charger_present = is_charger_present(chip);
	bool bms_suspend_config;

	/*
	 * Keep BMS FSM active if 'cfg_force_bms_active_on_charger' property
	 * is present and charger inserted. This ensures that recharge
	 * starts once battery SOC falls below resume_soc.
	 */
	bms_suspend_config = chip->dt.cfg_force_bms_active_on_charger
						&& charger_present;

	chip->apply_suspend_config = false;
	if (!battery_charging && !hi_power_state && !bms_suspend_config)
		chip->apply_suspend_config = true;

	pr_debug("battery_charging=%d power_state=%s hi_power_state=0x%x apply_suspend_config=%d bms_suspend_config=%d usb_present=%d\n",
			battery_charging, hi_power_state ? "hi" : "low",
				chip->hi_power_state,
				chip->apply_suspend_config, bms_suspend_config,
				charger_present);

	if (chip->apply_suspend_config) {
		if (chip->dt.cfg_force_s3_on_suspend) {
			disable_bms_irq(&chip->fifo_update_done_irq);
			pr_debug("Forcing S3 state\n");
			mutex_lock(&chip->state_change_mutex);
			force_fsm_state(chip, S3_STATE);
			mutex_unlock(&chip->state_change_mutex);
			/* Store accumulated data if any */
			process_suspend_data(chip);
		}
	}

	cancel_delayed_work_sync(&chip->monitor_soc_work);

	return 0;
}

static int bms_resume(struct device *dev)
{
	u8 state = 0;
	int rc, monitor_soc_delay = 0;
	unsigned long tm_now_sec;
	struct qpnp_bms_chip *chip = dev_get_drvdata(dev);

	if (chip->apply_suspend_config) {
		if (chip->dt.cfg_force_s3_on_suspend) {
			/*
			 * Update the state to S2 only if we are in S3. There is
			 * a possibility of being in S2 if we resumed on
			 * a charger insertion
			 */
			mutex_lock(&chip->state_change_mutex);
			rc = get_fsm_state(chip, &state);
			if (rc)
				pr_err("Unable to get FSM state rc=%d\n", rc);
			if (rc || (state == S3_STATE)) {
				pr_debug("Unforcing S3 state, setting S2 state\n");
				force_fsm_state(chip, S2_STATE);
			}
			mutex_unlock(&chip->state_change_mutex);
			enable_bms_irq(&chip->fifo_update_done_irq);
			/*
			 * if we were charging while suspended, we will
			 * be woken up by the fifo done interrupt and no
			 * additional processing is needed.
			 */
			process_resume_data(chip);
		}
	}

	/* Start monitor_soc_work based on when it last executed */
	rc = get_current_time(&tm_now_sec);
	if (rc) {
		pr_err("Could not read current time: %d\n", rc);
	} else {
		monitor_soc_delay = get_calculation_delay_ms(chip) -
			((tm_now_sec - chip->tm_sec) * 1000);
		monitor_soc_delay = max(0, monitor_soc_delay);
	}
	pr_debug("monitor_soc_delay_sec=%d tm_now_sec=%ld chip->tm_sec=%ld\n",
			monitor_soc_delay / 1000, tm_now_sec, chip->tm_sec);
	schedule_delayed_work(&chip->monitor_soc_work,
				msecs_to_jiffies(monitor_soc_delay));

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
