#define pr_fmt(fmt) "[dual-fuelgauge] %s: " fmt, __func__
#include <linux/init.h>
#include <linux/module.h>
#include <linux/param.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/pmic-voter.h>

#include "dual_fuel_gauge_class.h"

#define JEITA_COOL_THR_DEGREE 150
#define EXTREME_HIGH_DEGREE 1000
#define MASTER_DEFAULT_FCC_MAH 4500000
#define SLAVE_DEFAULT_FCC__MAH 4500000
#define FG_FULL 100
#define FG_RAW_FULL 10000
#define SHUTDOWN_DELAY_VOL 3300
#define BQ_MAXIUM_VOLTAGE_HYS  10000
#define BMS_FG_VERIFY		"BMS_FG_VERIFY"

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
enum print_reason {
	PR_INTERRUPT = BIT(0),
	PR_REGISTER = BIT(1),
	PR_OEM = BIT(2),
	PR_DEBUG = BIT(3),
};
enum reason {
	USER = BIT(0),
	THERMAL = BIT(1),
	CURRENT = BIT(2),
	SOC = BIT(3),
};
static int debug_mask = PR_OEM;
module_param_named(debug_mask, debug_mask, int, 0600);
static struct dual_fg_info *global_dual_fg_info = NULL;
struct dual_fg_chip {
	struct device *dev;
	char *model_name;
	struct mutex data_lock;

	int batt_st;
	int batt_temp;
	int batt_curr;
	int batt_volt;
	int term_curr;
	int ffc_term_curr;
	int recharge_volt;
	int batt_ttf;
	int batt_tte;
	int batt_fcc;
	int batt_soc;
	int batt_rm;
	int raw_soc;
	int batt_cyclecnt;
	int batt_resistance;
	int batt_dc;
	int batt_capacity_level;
	int batt_recharge_vol;
	int soc_decimal;
	int soc_decimal_rate;
	int constant_charge_current_max;
	bool verify_digest_success;
	bool fast_mode;
	int soh;

	int fake_volt;
	int fake_soc;
	int fake_chip_ok;
	int fake_temp;

	int *dec_rate_seq;
	int dec_rate_len;

	struct votable *fcc_votable;
	struct power_supply *fg_psy;
	struct power_supply_desc fg_psy_d;
	struct power_supply *fg_master_psy;
	struct power_supply *fg_slave_psy;
	struct power_supply *batt_psy;
	struct delayed_work monitor_work;

	/* workaround for debug or other purpose */
	bool ignore_digest_for_debug;
	bool old_hw;

	bool shutdown_delay;
	bool shutdown_delay_enable;

	/* move update charge full states  */
	bool charge_done;
	bool charge_full;
	int health;

	/* counter for low temp soc smooth */
	int master_soc_smooth_cnt;
	int slave_soc_smooth_cnt;
};

#define bq_dbg(reason, fmt, ...)                                               \
	do {                                                                   \
		if (debug_mask & (reason))                                     \
			pr_info(fmt, ##__VA_ARGS__);                           \
		else                                                           \
			pr_debug(fmt, ##__VA_ARGS__);                          \
	} while (0)

static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_SHUTDOWN_DELAY,
	POWER_SUPPLY_PROP_CAPACITY_RAW,
	POWER_SUPPLY_PROP_SOC_DECIMAL,
	POWER_SUPPLY_PROP_SOC_DECIMAL_RATE,
	POWER_SUPPLY_PROP_COLD_THERMAL_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	/*POWER_SUPPLY_PROP_HEALTH,*/ /*implement it in battery power_supply*/
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_UPDATE_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_FASTCHARGE_MODE,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TERMINATION_CURRENT,
	POWER_SUPPLY_PROP_FFC_TERMINATION_CURRENT,
	POWER_SUPPLY_PROP_RECHARGE_VBAT,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_SOH,
};
static int fg_read_temperature(struct dual_fg_chip *bq);
static int fg_read_system_soc(struct dual_fg_chip *bq);
static int fg_read_volt(struct dual_fg_chip *bq);
static int fg_read_current(struct dual_fg_chip *bq, int *curr);
static int fg_read_fcc(struct dual_fg_chip *bq);
static int fg_read_rm(struct dual_fg_chip *bq);

static int fg_get_batt_capacity_level(struct dual_fg_chip *bq);
static int fg_get_soc_decimal(struct dual_fg_chip *bq);
static int fg_get_soc_decimal_rate(struct dual_fg_chip *bq);
static int fg_get_cold_thermal_level(struct dual_fg_chip *bq);
static int fg_get_raw_soc(struct dual_fg_chip *bq);
static int fg_read_tte(struct dual_fg_chip *bq);
static int fg_read_ttf(struct dual_fg_chip *bq);
static int fg_read_cyclecount(struct dual_fg_chip *bq);
static int fg_read_soh(struct dual_fg_chip *bq);
static int fg_get_batt_status(struct dual_fg_chip *bq);

/* auto define interface for dual fg */
static int fg_get_DesignedCapcity(struct dual_fg_chip *bq);
static int fg_get_CurrentMax(struct dual_fg_chip *bq);
static int fg_get_constant_charge_CurrentMax(struct dual_fg_chip *bq);
static int fg_get_VoltageMax(struct dual_fg_chip *bq);
static bool fg_get_chipok(struct dual_fg_chip *bq);
static int fg_get_termination_current(struct dual_fg_chip *bq);
static int fg_get_FFC_termination_current(struct dual_fg_chip *bq);
static int fg_get_recharge_volt(struct dual_fg_chip *bq);
static void fg_check_I2C_status(struct dual_fg_chip *bq);

static void dual_fg_check_fg_matser_psy(struct dual_fg_chip *bq);
static void dual_fg_check_fg_slave_psy(struct dual_fg_chip *bq);
static void dual_fg_check_batt_psy(struct dual_fg_chip *bq);

static int fg_set_fastcharge_mode(struct dual_fg_chip *bq, bool enable);
static int fg_get_property(struct power_supply *psy,
			   enum power_supply_property psp,
			   union power_supply_propval *val);
static int fg_set_property(struct power_supply *psy,
			   enum power_supply_property prop,
			   const union power_supply_propval *val);
static int fg_prop_is_writeable(struct power_supply *psy,
				enum power_supply_property prop);
static void fg_monitor_workfunc(struct work_struct *work);
static void fg_update_status(struct dual_fg_chip *bq);
static int fg_update_charge_full(struct dual_fg_chip *bq);
// enable or diabled fg master or slave  by GPIO
static int fg_master_set_disable(bool disable);
static int fg_slave_set_disable(bool disable);
static int fg_master_set_disable(bool disable)
{
	if (disable) {
		if (global_dual_fg_info->fg_master_disable_gpio)
			gpio_set_value(
				global_dual_fg_info->fg_master_disable_gpio, 1);
		bq_dbg(PR_OEM, "fg_master_disable_gpio= %d \n", 1);
	} else {
		if (global_dual_fg_info->fg_master_disable_gpio)
			gpio_set_value(
				global_dual_fg_info->fg_master_disable_gpio, 0);
		bq_dbg(PR_OEM, "fg_master_disable_gpio= %d \n", 0);
	}
	return 0;
}

static int fg_slave_set_disable(bool disable)
{
	if (disable) {
		if (global_dual_fg_info->fg_slave_disable_gpio)
			gpio_set_value(
				global_dual_fg_info->fg_slave_disable_gpio, 1);
		bq_dbg(PR_OEM, "fg_slave_disable_gpio= %d \n", 1);
	} else {
		if (global_dual_fg_info->fg_slave_disable_gpio)
			gpio_set_value(
				global_dual_fg_info->fg_slave_disable_gpio, 0);
		bq_dbg(PR_OEM, "fg_slave_disable_gpio= %d \n", 0);
	}
	return 0;
}

int Dual_Fg_Check_Chg_Fg_Status_And_Disable_Chg_Path(void)
{
	int rc;
	int fg_master_soc, fg_slave_soc;
	union power_supply_propval pval = {
		0,
	};
	struct power_supply *batt_psy = NULL;
	int batt_chg_type;

	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		pr_err("battery not found ,so cannot disable fg master or slave \n");
		return 0;
	}
	rc = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CHARGE_TYPE,
				       &pval);
	batt_chg_type = pval.intval;
	if (batt_chg_type != POWER_SUPPLY_CHARGE_TYPE_TRICKLE &&
	    batt_chg_type != POWER_SUPPLY_CHARGE_TYPE_FAST &&
	    batt_chg_type != POWER_SUPPLY_CHARGE_TYPE_TAPER) {
		return 0;
	}
	rc = power_supply_get_property(global_dual_fg_info->gl_fg_master_psy,
				       POWER_SUPPLY_PROP_CAPACITY_RAW, &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get master fg SOC \n");
		return -EINVAL;
	}
	fg_master_soc = pval.intval;
	rc = power_supply_get_property(global_dual_fg_info->gl_fg_slave_psy,
				       POWER_SUPPLY_PROP_CAPACITY_RAW, &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get slave fg SOC \n");
		return -EINVAL;
	}
	fg_slave_soc = pval.intval;
	/* fg master or slave  */
	if (!global_dual_fg_info->fg1_batt_ctl_enabled &&
	    !global_dual_fg_info->fg2_batt_ctl_enabled) {
		if ((fg_master_soc >= fg_slave_soc) &&
		    fg_master_soc == FG_RAW_FULL) {
			rc = fg_master_set_disable(true);
			global_dual_fg_info->fg1_batt_ctl_enabled = true;
		} else if ((fg_master_soc < fg_slave_soc) &&
			   fg_slave_soc == FG_RAW_FULL) {
			rc = fg_slave_set_disable(true);
			global_dual_fg_info->fg2_batt_ctl_enabled = true;
		}
	}
	return 0;
}

int Dual_Fg_Reset_Batt_Ctrl_gpio_default(void)
{
	int fg_master_gpio, fg_slave_gpio;
	fg_master_gpio =
		gpio_get_value(global_dual_fg_info->fg_master_disable_gpio);
	fg_slave_gpio =
		gpio_get_value(global_dual_fg_info->fg_slave_disable_gpio);
	if (global_dual_fg_info->fg1_batt_ctl_enabled == true ||
	    fg_master_gpio == 1) {
		fg_master_set_disable(false);
		global_dual_fg_info->fg1_batt_ctl_enabled = false;
	}
	if (global_dual_fg_info->fg2_batt_ctl_enabled == true ||
	    fg_slave_gpio == 1) {
		fg_slave_set_disable(false);
		global_dual_fg_info->fg2_batt_ctl_enabled = false;
	}
	return 0;
}

int Dual_Fuel_Gauge_Batt_Ctrl_Init(void)
{
	if (global_dual_fg_info->fg_master_disable_gpio)
		gpio_set_value(global_dual_fg_info->fg_master_disable_gpio, 0);
	if (global_dual_fg_info->fg_slave_disable_gpio)
		gpio_set_value(global_dual_fg_info->fg_slave_disable_gpio, 0);
	return 0;
}
static void dual_fg_check_fg_matser_psy(struct dual_fg_chip *bq)
{
	if (!bq->fg_master_psy) {
		bq->fg_master_psy = power_supply_get_by_name("bms_master");
		if (!bq->fg_master_psy)
			pr_err("fg_master_psy not found\n");
	}
}
static void dual_fg_check_fg_slave_psy(struct dual_fg_chip *bq)
{
	if (!bq->fg_slave_psy) {
		bq->fg_slave_psy = power_supply_get_by_name("bms_slave");
		if (!bq->fg_slave_psy)
			pr_err("fg_slave_psy not found\n");
	}
}
static void dual_fg_check_batt_psy(struct dual_fg_chip *bq)
{
	if (!bq->batt_psy) {
		bq->batt_psy = power_supply_get_by_name("battery");
		if (!bq->batt_psy)
			pr_err("battery not found\n");
	}
}

static int fg_set_fastcharge_mode(struct dual_fg_chip *bq, bool enable)
{
	int rc;
	union power_supply_propval pval = {
		0,
	};
	pval.intval = enable;
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return 0;
	}
	rc = power_supply_set_property(
		bq->fg_master_psy, POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
	rc = power_supply_set_property(
		bq->fg_slave_psy, POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
	bq->fast_mode = enable;
	return 0;
}

static int fg_read_temperature(struct dual_fg_chip *bq)
{
	int temp_master = 25, temp_slave = 25;
	int temp = 0;
	int rc;
	union power_supply_propval pval = {
		0,
	};

	if (bq->fake_temp > 0)
		return bq->fake_temp;
	/* get master and slave fg temperature*/
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get master fg batt temp\n");
		return -EINVAL;
	}
	temp_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy, POWER_SUPPLY_PROP_TEMP,
				       &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get slave fg batt temp\n");
		return -EINVAL;
	}
	temp_slave = pval.intval;
	if (temp_master <= JEITA_COOL_THR_DEGREE ||
	    temp_slave <= JEITA_COOL_THR_DEGREE)
		temp = MIN(temp_master, temp_slave);
	else if (MAX(temp_master, temp_slave) > EXTREME_HIGH_DEGREE)
		temp = MIN(temp_master, temp_slave);
	else
		temp = MAX(temp_master, temp_slave);
	return temp;
}

static int fg_read_volt(struct dual_fg_chip *bq)
{
	int batt_volt_master = 0, batt_volt_slave = 0;
	int volt;
	int rc;
	union power_supply_propval pval = {
		0,
	};

	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	/* get master and slave fg voltage*/
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get master fg batt voltage\n");
		return -EINVAL;
	}
	batt_volt_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get slave fg batt voltage\n");
		return -EINVAL;
	}
	batt_volt_slave = pval.intval;
	volt = MAX(batt_volt_master, batt_volt_slave);
	volt /= 1000;
	return volt;
}

static int fg_read_current(struct dual_fg_chip *bq, int *curr)
{
	int batt_curr_master = 0, batt_curr_slave = 0;
	int rc;
	union power_supply_propval pval = {
		0,
	};

	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	/* get master and slave fg current*/
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get master fg batt current\n");
		return -EINVAL;
	}
	batt_curr_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get slave fg batt current\n");
		return -EINVAL;
	}
	batt_curr_slave = pval.intval;

	*curr = (batt_curr_master + batt_curr_slave) / 1000;
	return 0;
}

static int fg_get_batt_status(struct dual_fg_chip *bq)
{
	int rc;
	int st;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_batt_psy(bq);
	if (!bq->batt_psy) {
		pr_err("battery not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_STATUS,
				       &pval);
	st = pval.intval;
	return st;
}
static int fg_get_DesignedCapcity(struct dual_fg_chip *bq)
{
	int DesignedCapa_master = 0, DesignedCapa_slave = 0;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	/* get master and slave fg current*/
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(
		bq->fg_master_psy, POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get master fg charge full design \n");
		return -EINVAL;
	}
	DesignedCapa_master = pval.intval;
	rc = power_supply_get_property(
		bq->fg_slave_psy, POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get slave fg charge full design\n");
		return -EINVAL;
	}
	DesignedCapa_slave = pval.intval;

	bq->batt_dc = DesignedCapa_master + DesignedCapa_slave;
	return 0;
}

static int fg_read_fcc(struct dual_fg_chip *bq)
{
	int learned_master = 0, learned_slave = 0;
	int rm_master = 0, rm_slave = 0;
	int rc, fcc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
	rm_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
	rm_slave = pval.intval;
	/* if rm of any battery(base or flip) decrease to 0, use default fcc of each other */
	if (!rm_master || !rm_slave) {
		global_dual_fg_info->fcc_master = MASTER_DEFAULT_FCC_MAH;
		global_dual_fg_info->fcc_slave = SLAVE_DEFAULT_FCC__MAH;
	} else {
		rc = power_supply_get_property(bq->fg_master_psy,
					       POWER_SUPPLY_PROP_CHARGE_FULL,
					       &pval);
		learned_master = pval.intval;
		if (global_dual_fg_info->fcc_master != learned_master)
			global_dual_fg_info->fcc_master = learned_master;

		rc = power_supply_get_property(
			bq->fg_slave_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &pval);
		learned_slave = pval.intval;
		if (global_dual_fg_info->fcc_slave != learned_slave)
			global_dual_fg_info->fcc_slave = learned_slave;
	}
	fcc = (learned_master + learned_slave) / 1000;
	return fcc;
}

static int fg_read_soh(struct dual_fg_chip *bq)
{
	int soh_master = 0, soh_slave, soh = 0;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->fg_master_psy, POWER_SUPPLY_PROP_SOH,
				       &pval);
	soh_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy, POWER_SUPPLY_PROP_SOH,
				       &pval);
	soh_slave = pval.intval;
	soh = MIN(soh_slave, soh_master);
	return soh;
}

/* get MonotonicSoc*/
static int fg_read_system_soc(struct dual_fg_chip *bq)
{
	int batt_soc_master = 15, batt_soc_slave = 15;
	int rate_of_master = 50, rate_of_slave = 50;
	static int last_batt_soc_master, last_batt_soc_slave;
	int rc, soc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	if (!global_dual_fg_info->fg1_batt_ctl_enabled) {
		rc = power_supply_get_property(
			bq->fg_master_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		batt_soc_master = pval.intval;
		if (bq->batt_temp < 0 && bq->charge_done)
			batt_soc_master = last_batt_soc_master;
		else
			last_batt_soc_master = batt_soc_master;
	} else {
		//fix low temp soc jump
		if (last_batt_soc_master < FG_FULL && bq->batt_temp < 0
				&& bq->master_soc_smooth_cnt++ > 30) {
			batt_soc_master = ++last_batt_soc_master;
			bq->master_soc_smooth_cnt = 0;
		} else if (last_batt_soc_master < FG_FULL && bq->batt_temp < 0) {
			batt_soc_master = last_batt_soc_master;
		} else {
			batt_soc_master = FG_FULL;
			bq->master_soc_smooth_cnt = 0;
		}
	}
	if (!global_dual_fg_info->fg2_batt_ctl_enabled) {
		rc = power_supply_get_property(
			bq->fg_slave_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		batt_soc_slave = pval.intval;
		if (bq->batt_temp < 0 && bq->charge_done)
			batt_soc_slave = last_batt_soc_slave;
		else
			last_batt_soc_slave = batt_soc_slave;
	} else {
		//fix low temp soc jump
		if (last_batt_soc_slave < FG_FULL && bq->batt_temp < 0
				&& bq->slave_soc_smooth_cnt++ > 30) {
			batt_soc_slave = ++last_batt_soc_slave;
			bq->slave_soc_smooth_cnt = 0;
		} else if (last_batt_soc_slave < FG_FULL && bq->batt_temp < 0) {
			batt_soc_slave = last_batt_soc_slave;
		} else {
			batt_soc_slave = FG_FULL;
			bq->slave_soc_smooth_cnt = 0;
		}
	}
	rate_of_master = global_dual_fg_info->fcc_master * 100 /
			 (global_dual_fg_info->fcc_master +
			  global_dual_fg_info->fcc_slave);
	rate_of_slave = 100 - rate_of_master;
	soc = (rate_of_master * batt_soc_master +
	       rate_of_slave * batt_soc_slave + 50) /
	      100;
	if (bq->charge_full)
		soc = 100;
	return soc;
}
static int fg_get_raw_soc(struct dual_fg_chip *bq)
{
	int raw_soc_master, raw_soc_slave, raw_soc;
	int rate_of_master = 50, rate_of_slave = 50;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_CAPACITY_RAW, &pval);
	raw_soc_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_CAPACITY_RAW, &pval);
	raw_soc_slave = pval.intval;
	rate_of_master = global_dual_fg_info->fcc_master * 100 /
			 (global_dual_fg_info->fcc_master +
			  global_dual_fg_info->fcc_slave);
	rate_of_slave = 100 - rate_of_master;
	raw_soc = (rate_of_master * raw_soc_master +
		   rate_of_slave * raw_soc_slave + 50) /
		  100;
	return raw_soc;
}
static int fg_read_rm(struct dual_fg_chip *bq)
{
	int rm_master = 0, rm_slave = 0;
	int rc, rm;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
	rm_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
	rm_slave = pval.intval;
	rm = (rm_master + rm_slave) / 1000;
	return rm;
}
static int fg_get_batt_capacity_level(struct dual_fg_chip *bq)
{
	int capacity_level_master = 3, capacity_level_slave = 3;
	int rc, capacity_level;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_CAPACITY_LEVEL, &pval);
	capacity_level_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_CAPACITY_LEVEL, &pval);
	capacity_level_slave = pval.intval;
	/* dual battery is same  for k81*/
	capacity_level = (capacity_level_master + capacity_level_slave + 1) / 2;
	return capacity_level;
}
static int fg_get_soc_decimal(struct dual_fg_chip *bq)
{
	int soc_decimal = 50;
	int batt_soc_master = 0, batt_soc_slave = 0;
	int rate_of_master = 50, rate_of_slave = 50;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	if (!global_dual_fg_info->fg1_batt_ctl_enabled) {
		rc = power_supply_get_property(
			bq->fg_master_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		batt_soc_master = pval.intval;
	} else {
		batt_soc_master = FG_FULL;
	}

	if (!global_dual_fg_info->fg2_batt_ctl_enabled) {
		rc = power_supply_get_property(
			bq->fg_slave_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		batt_soc_slave = pval.intval;
	} else {
		batt_soc_slave = FG_FULL;
	}
	rate_of_master = global_dual_fg_info->fcc_master * 100 /
			 (global_dual_fg_info->fcc_master +
			  global_dual_fg_info->fcc_slave);
	rate_of_slave = 100 - rate_of_master;
	soc_decimal = (rate_of_master * batt_soc_master +
		       rate_of_slave * batt_soc_slave + 50) %
		      100;
	return soc_decimal;
}
static int fg_get_soc_decimal_rate(struct dual_fg_chip *bq)
{
	int soc, i;
	if (bq->dec_rate_len <= 0)
		return 0;
	soc = fg_read_system_soc(bq);
	for (i = 0; i < bq->dec_rate_len; i += 2) {
		if (soc < bq->dec_rate_seq[i]) {
			return bq->dec_rate_seq[i - 1];
		}
	}
	return bq->dec_rate_seq[bq->dec_rate_len - 1];
}

static int fg_get_cold_thermal_level(struct dual_fg_chip *bq)
{
	int cold_master = 0, cold_slave = 0, cold = 0;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(
		bq->fg_master_psy, POWER_SUPPLY_PROP_COLD_THERMAL_LEVEL, &pval);
	cold_master = pval.intval;
	rc = power_supply_get_property(
		bq->fg_slave_psy, POWER_SUPPLY_PROP_COLD_THERMAL_LEVEL, &pval);
	cold_slave = pval.intval;
	cold = MIN(cold_master, cold_slave);
	return cold;
}

static int fg_read_tte(struct dual_fg_chip *bq)
{
	int tte_master, tte_slave, tte;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(
		bq->fg_master_psy, POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW, &pval);
	tte_master = pval.intval;
	rc = power_supply_get_property(
		bq->fg_slave_psy, POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW, &pval);
	tte_slave = pval.intval;

	tte = MAX(tte_master, tte_slave);
	return tte;
}
static int fg_read_ttf(struct dual_fg_chip *bq)
{
	int ttf_master, ttf_slave, ttf;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(
		bq->fg_master_psy, POWER_SUPPLY_PROP_TIME_TO_FULL_AVG, &pval);
	ttf_master = pval.intval;
	rc = power_supply_get_property(
		bq->fg_slave_psy, POWER_SUPPLY_PROP_TIME_TO_FULL_AVG, &pval);
	ttf_slave = pval.intval;
	ttf = MAX(ttf_master, ttf_slave);
	return ttf;
}

static int fg_read_cyclecount(struct dual_fg_chip *bq)
{
	u16 cc;
	int cc_master = 0, cc_slave = 0;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	cc_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	cc_slave = pval.intval;

	cc = MIN(cc_master, cc_slave);
	return cc;
}

static int fg_get_CurrentMax(struct dual_fg_chip *bq)
{
	int curr_max = 0, curr_max_master, curr_max_slave;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
	curr_max_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
	curr_max_slave = pval.intval;
	curr_max = curr_max_master + curr_max_slave;
	return curr_max;
}
static int fg_get_constant_charge_CurrentMax(struct dual_fg_chip *bq)
{
	int curr_max = 0, curr_max_master, curr_max_slave;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(
		bq->fg_master_psy,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
	curr_max_master = pval.intval;
	rc = power_supply_get_property(
		bq->fg_slave_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
		&pval);
	curr_max_slave = pval.intval;
	curr_max = curr_max_master + curr_max_slave;
	return curr_max;
}
static int fg_get_VoltageMax(struct dual_fg_chip *bq)
{
	int volt_max = 0, volt_max_master, volt_max_slave;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
	volt_max_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
	volt_max_slave = pval.intval;
	if (volt_max_master && volt_max_slave)
		volt_max = MIN(volt_max_master, volt_max_slave);
	else
		volt_max = MAX(volt_max_master, volt_max_slave);
	if (global_dual_fg_info->fg1_batt_ctl_enabled || global_dual_fg_info->fg2_batt_ctl_enabled) {
		volt_max += BQ_MAXIUM_VOLTAGE_HYS;
		bq_dbg(PR_OEM, "add 10mv to fv\n");
	}
	bq_dbg(PR_DEBUG, "dual_gauge voltage max:%d\n", volt_max);
	return volt_max;
}

static bool fg_get_chipok(struct dual_fg_chip *bq)
{
	bool chip_master, chip_slave;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return false;
	}
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_CHIP_OK, &pval);
	chip_master = !!pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_CHIP_OK, &pval);
	chip_slave = !!pval.intval;
	if (chip_master == true && chip_slave == true)
		return true;
	else
		return false;
}

static int fg_get_termination_current(struct dual_fg_chip *bq)
{
	int current_master, current_slave;
	int rc, curr;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_TERMINATION_CURRENT,
				       &pval);
	current_master = pval.intval;
	rc = power_supply_get_property(
		bq->fg_slave_psy, POWER_SUPPLY_PROP_TERMINATION_CURRENT, &pval);
	current_slave = pval.intval;
	curr = current_master + current_slave;
	return curr;
}
static int fg_get_FFC_termination_current(struct dual_fg_chip *bq)
{
	int current_master, current_slave;
	int rc, curr;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(
		bq->fg_master_psy, POWER_SUPPLY_PROP_FFC_TERMINATION_CURRENT,
		&pval);
	current_master = pval.intval;
	rc = power_supply_get_property(
		bq->fg_slave_psy, POWER_SUPPLY_PROP_FFC_TERMINATION_CURRENT,
		&pval);
	current_slave = pval.intval;
	curr = current_master + current_slave;
	return curr;
}

static int fg_get_recharge_volt(struct dual_fg_chip *bq)
{
	int recharge_volt, recharge_volt_master, recharge_volt_slave;
	int rc;
	union power_supply_propval pval = {
		0,
	};
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	if (!bq->fg_master_psy || !bq->fg_slave_psy) {
		pr_err("bms_master or bms_slave not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_RECHARGE_VBAT, &pval);
	recharge_volt_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_RECHARGE_VBAT, &pval);
	recharge_volt_slave = pval.intval;
	recharge_volt = MAX(recharge_volt_master, recharge_volt_slave);
	return recharge_volt;
}
static void fg_check_I2C_status(struct dual_fg_chip *bq)
{
	union power_supply_propval pval = {
		0,
	};
	int rc;
	const char *master_model_name, *slave_model_name;
	if (!bq->fg_slave_psy || !bq->fg_master_psy)
		return;
	rc = power_supply_get_property(bq->fg_master_psy,
				       POWER_SUPPLY_PROP_MODEL_NAME, &pval);
	master_model_name = pval.strval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_MODEL_NAME, &pval);
	slave_model_name = pval.strval;
	if (strncmp(master_model_name, "unknown", 7) == 0 ||
	    strncmp(slave_model_name, "unknown", 7) == 0) {
		/* for draco p0 and p0.1 */
		if (bq->ignore_digest_for_debug)
			bq->old_hw = true;
		else
			bq->old_hw = false;
		bq_dbg(PR_OEM, "master fg or slave fg I2C failed\n");
	}
}
static int fg_get_property(struct power_supply *psy,
			   enum power_supply_property psp,
			   union power_supply_propval *val)
{
	struct dual_fg_chip *bq = power_supply_get_drvdata(psy);
	int ret, status;
	int vbat_mv;
	static bool shutdown_delay_cancel;
	static bool last_shutdown_delay;
	union power_supply_propval pval = {
		0,
	};

	switch (psp) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		if (bq->old_hw) {
			val->strval = "unknown";
			break;
		}
		val->strval = bq->model_name;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (bq->fake_volt != -EINVAL) {
			val->intval = bq->fake_volt;
			break;
		}
		if (bq->old_hw) {
			val->intval = 3700 * 1000;
			break;
		}
		ret = fg_read_volt(bq);
		if (ret >= 0)
			bq->batt_volt = ret;
		val->intval = bq->batt_volt * 1000;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (bq->old_hw) {
			val->intval = -500 * 2;
			break;
		}
		fg_read_current(bq, &bq->batt_curr);
		val->intval = bq->batt_curr * 1000;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (bq->fake_soc >= 0) {
			val->intval = bq->fake_soc;
			break;
		}
		if (bq->old_hw) {
			val->intval = 15;
			break;
		}
		val->intval = fg_read_system_soc(bq);
		bq->batt_soc = val->intval;
		Dual_Fg_Check_Chg_Fg_Status_And_Disable_Chg_Path();
		//add shutdown delay feature
		if (bq->shutdown_delay_enable) {
			if (val->intval == 0) {
				vbat_mv = fg_read_volt(bq);
				if (bq->batt_psy) {
					power_supply_get_property(
						bq->batt_psy,
						POWER_SUPPLY_PROP_STATUS,
						&pval);
					status = pval.intval;
				}
				if (vbat_mv > SHUTDOWN_DELAY_VOL &&
				    status != POWER_SUPPLY_STATUS_CHARGING) {
					bq->shutdown_delay = true;
					val->intval = 1;
				} else if (status ==
						   POWER_SUPPLY_STATUS_CHARGING &&
					   bq->shutdown_delay) {
					bq->shutdown_delay = false;
					shutdown_delay_cancel = true;
					val->intval = 1;
				} else {
					bq->shutdown_delay = false;
					if (shutdown_delay_cancel)
						val->intval = 1;
				}
			} else {
				bq->shutdown_delay = false;
				shutdown_delay_cancel = false;
			}

			if (last_shutdown_delay != bq->shutdown_delay) {
				last_shutdown_delay = bq->shutdown_delay;
				if (bq->fg_psy)
					power_supply_changed(bq->fg_psy);
			}
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = fg_get_batt_capacity_level(bq);
		break;
	case POWER_SUPPLY_PROP_SHUTDOWN_DELAY:
		val->intval = bq->shutdown_delay;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_RAW:
		val->intval = bq->raw_soc;
		break;
	case POWER_SUPPLY_PROP_SOC_DECIMAL:
		val->intval = fg_get_soc_decimal(bq);
		break;
	case POWER_SUPPLY_PROP_SOC_DECIMAL_RATE:
		val->intval = fg_get_soc_decimal_rate(bq);
		break;
	case POWER_SUPPLY_PROP_COLD_THERMAL_LEVEL:
		val->intval = fg_get_cold_thermal_level(bq);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (bq->fake_temp != -EINVAL) {
			val->intval = bq->fake_temp;
			break;
		}
		if (bq->old_hw) {
			val->intval = 250;
			break;
		}
		val->intval = bq->batt_temp;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		if (bq->old_hw) {
			val->intval = bq->batt_tte;
			break;
		}
		ret = fg_read_tte(bq);
		if (ret >= 0)
			bq->batt_tte = ret;

		val->intval = bq->batt_tte;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		if (bq->old_hw) {
			val->intval = bq->batt_ttf;
			break;
		}
		ret = fg_read_ttf(bq);
		if (ret >= 0)
			bq->batt_ttf = ret;

		val->intval = bq->batt_ttf;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (bq->old_hw) {
			val->intval = 4050000;
			break;
		}
		val->intval = bq->batt_fcc * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = bq->batt_dc;
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		if (bq->old_hw) {
			val->intval = 1;
			break;
		}
		ret = fg_read_cyclecount(bq);
		if (ret >= 0)
			bq->batt_cyclecnt = ret;
		val->intval = bq->batt_cyclecnt;
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		val->intval = bq->batt_resistance;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		val->intval = 100000;
		break;
	case POWER_SUPPLY_PROP_UPDATE_NOW:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (bq->old_hw) {
			val->intval = 8000000;
			break;
		}
		val->intval = fg_get_CurrentMax(bq);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (bq->old_hw) {
			val->intval = 4450000;
			break;
		}
		val->intval = fg_get_VoltageMax(bq);
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		val->intval = bq->verify_digest_success;
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		if (bq->fake_chip_ok != -EINVAL) {
			val->intval = bq->fake_chip_ok;
			break;
		}
		val->intval = fg_get_chipok(bq);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		if (bq->constant_charge_current_max != 0)
			val->intval = bq->constant_charge_current_max;
		else {
			val->intval = fg_get_constant_charge_CurrentMax(bq);
		}
		break;
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		if (bq->old_hw) {
			val->intval = 0;
			break;
		}
		val->intval = bq->fast_mode;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		if (bq->old_hw) {
			val->intval = 4050000;
			break;
		}
		val->intval = bq->batt_rm * 1000;
		break;
	case POWER_SUPPLY_PROP_TERMINATION_CURRENT:
		val->intval = fg_get_termination_current(bq);
		break;
	case POWER_SUPPLY_PROP_FFC_TERMINATION_CURRENT:
		val->intval = fg_get_FFC_termination_current(bq);
		break;
	case POWER_SUPPLY_PROP_RECHARGE_VBAT:
		if (bq->batt_recharge_vol > 0)
			val->intval = bq->batt_recharge_vol;
		else
			val->intval = fg_get_recharge_volt(bq);
		break;
	case POWER_SUPPLY_PROP_SOH:
		val->intval = fg_read_soh(bq);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int fg_set_property(struct power_supply *psy,
			   enum power_supply_property prop,
			   const union power_supply_propval *val)
{
	struct dual_fg_chip *bq = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
		bq->fake_temp = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		bq->fake_soc = val->intval;
		power_supply_changed(bq->fg_psy);
		break;
	case POWER_SUPPLY_PROP_UPDATE_NOW:
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		bq->verify_digest_success = !!val->intval;
		if (!bq->fcc_votable)
			bq->fcc_votable = find_votable("FCC");
		vote(bq->fcc_votable, BMS_FG_VERIFY, !bq->verify_digest_success,
				!bq->verify_digest_success ? 2000000 : 0);
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		bq->fake_chip_ok = !!val->intval;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		bq->constant_charge_current_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		fg_set_fastcharge_mode(bq, !!val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		bq->fake_volt = val->intval;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fg_prop_is_writeable(struct power_supply *psy,
				enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_UPDATE_NOW:
	case POWER_SUPPLY_PROP_AUTHENTIC:
	case POWER_SUPPLY_PROP_CHIP_OK:
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int fg_psy_register(struct dual_fg_chip *bq)
{
	struct power_supply_config fg_psy_cfg = {};
	bq->fg_psy_d.name = "bms";
	bq->fg_psy_d.type = POWER_SUPPLY_TYPE_BMS;
	bq->fg_psy_d.properties = fg_props;
	bq->fg_psy_d.num_properties = ARRAY_SIZE(fg_props);
	bq->fg_psy_d.get_property = fg_get_property;
	bq->fg_psy_d.set_property = fg_set_property;
	bq->fg_psy_d.property_is_writeable = fg_prop_is_writeable;

	fg_psy_cfg.drv_data = bq;
	fg_psy_cfg.num_supplicants = 0;
	bq->fg_psy =
		devm_power_supply_register(bq->dev, &bq->fg_psy_d, &fg_psy_cfg);
	if (IS_ERR(bq->fg_psy)) {
		bq_dbg(PR_OEM, "Failed to register fg_psy");
		return PTR_ERR(bq->fg_psy);
	}

	return 0;
}
/* periodically update fg data*/
static void fg_monitor_workfunc(struct work_struct *work)
{
	struct dual_fg_chip *bq =
		container_of(work, struct dual_fg_chip, monitor_work.work);

	if (!bq->old_hw) {
		fg_update_status(bq);
		fg_update_charge_full(bq);
	}
	schedule_delayed_work(&bq->monitor_work, 10 * HZ);
}
static void fg_update_status(struct dual_fg_chip *bq)
{
	static int last_st, last_soc, last_temp;
	mutex_lock(&bq->data_lock);
	bq->batt_soc = fg_read_system_soc(bq);
	bq->batt_volt = fg_read_volt(bq);
	fg_read_current(bq, &bq->batt_curr);
	bq->batt_temp = fg_read_temperature(bq);
	bq->batt_st = fg_get_batt_status(bq);
	bq->batt_rm = fg_read_rm(bq);
	bq->batt_fcc = fg_read_fcc(bq);
	bq->raw_soc = fg_get_raw_soc(bq);
	fg_get_DesignedCapcity(bq);

	mutex_unlock(&bq->data_lock);

	bq_dbg(PR_OEM, "SOC:%d,Volt:%d,Cur:%d,Temp:%d,RM:%d,FC:%d,FAST:%d",
	       bq->batt_soc, bq->batt_volt, bq->batt_curr, bq->batt_temp,
	       bq->batt_rm, bq->batt_fcc, bq->fast_mode);

	if ((last_soc != bq->batt_soc) || (last_temp != bq->batt_temp) ||
	    (last_st != bq->batt_st)) {
		if (bq->fg_psy)
			power_supply_changed(bq->fg_psy);
	}
	last_soc = bq->batt_soc;
	last_temp = bq->batt_temp;
	last_st = bq->batt_st;
}

static int fg_update_charge_full(struct dual_fg_chip *bq)
{
	int rc;
	union power_supply_propval prop = {
		0,
	};

	if (!bq->batt_psy) {
		bq->batt_psy = power_supply_get_by_name("battery");
		if (!bq->batt_psy) {
			return 0;
		}
	}

	rc = power_supply_get_property(bq->batt_psy,
				       POWER_SUPPLY_PROP_CHARGE_DONE, &prop);
	bq->charge_done = prop.intval;

	rc = power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_HEALTH,
				       &prop);
	bq->health = prop.intval;

	bq_dbg(PR_OEM, "raw:%d,done:%d,full:%d,health:%d\n", bq->raw_soc,
	       bq->charge_done, bq->charge_full, bq->health);
	if (bq->charge_done && !bq->charge_full) {
		if (bq->raw_soc >= BQ_REPORT_FULL_SOC) {
			bq_dbg(PR_OEM, "Setting charge_full to true\n");
			bq->charge_full = true;
		} else {
			bq_dbg(PR_OEM, "charging is done raw soc:%d\n",
			       bq->raw_soc);
		}
	} else if (bq->raw_soc <= BQ_CHARGE_FULL_SOC && !bq->charge_done &&
		   bq->charge_full) {
		rc = power_supply_get_property(
			bq->batt_psy, POWER_SUPPLY_PROP_CHARGE_DONE, &prop);
		bq->charge_done = prop.intval;
		if (bq->charge_done)
			goto out;

		bq->charge_full = false;
	}

	if ((bq->raw_soc <= BQ_RECHARGE_SOC) && bq->charge_done &&
	    bq->health != POWER_SUPPLY_HEALTH_WARM) {
		prop.intval = true;
		rc = power_supply_set_property(
			bq->batt_psy, POWER_SUPPLY_PROP_FORCE_RECHARGE, &prop);
		if (rc < 0) {
			bq_dbg(PR_OEM, "bq could not set force recharging!\n");
			return rc;
		}
		Dual_Fg_Reset_Batt_Ctrl_gpio_default();
	}

	//fix mtbf corner case
	if ((global_dual_fg_info->fg1_batt_ctl_enabled || global_dual_fg_info->fg2_batt_ctl_enabled)
		&& bq->raw_soc <= BQ_RESET_BATT_CTRL_SOC
		&& bq->health == POWER_SUPPLY_HEALTH_GOOD) {
		bq_dbg(PR_OEM, "reset batt ctrl\n");
		Dual_Fg_Reset_Batt_Ctrl_gpio_default();
	}
out:
	return 0;
}

static void fg_psy_unregister(struct dual_fg_chip *bq)
{
	power_supply_unregister(bq->fg_psy);
}

static int bq_parse_dt(struct dual_fg_chip *bq)
{
	struct device_node *node = bq->dev->of_node;
	int ret, size;
	enum of_gpio_flags flags;

	bq->ignore_digest_for_debug =
		of_property_read_bool(node, "bq,ignore-digest-debug");
	bq->shutdown_delay_enable =
		of_property_read_bool(node, "bq,shutdown-delay-enable");
	ret = of_property_read_u32(node, "bq,recharge-voltage",
				   &bq->batt_recharge_vol);
	size = 0;
	of_get_property(node, "bq,soc_decimal_rate", &size);
	if (size) {
		bq->dec_rate_seq = devm_kzalloc(bq->dev, size, GFP_KERNEL);
		if (bq->dec_rate_seq) {
			bq->dec_rate_len = (size / sizeof(*bq->dec_rate_seq));
			if (bq->dec_rate_len % 2) {
				bq_dbg(PR_OEM,
				       "invalid soc decimal rate seq\n");
				return -EINVAL;
			}
			of_property_read_u32_array(node, "bq,soc_decimal_rate",
						   bq->dec_rate_seq,
						   bq->dec_rate_len);
		} else {
			bq_dbg(PR_OEM,
			       "error allocating memory for dec_rate_seq\n");
		}
	}
	/* parse control fg master or slave gpio*/
	global_dual_fg_info->fg_master_disable_gpio = of_get_named_gpio_flags(
		node, "fg-master-disable-gpio", 0, &flags);
	if ((!gpio_is_valid(global_dual_fg_info->fg_master_disable_gpio))) {
		bq_dbg(PR_OEM,
		       "Failed to read node of fg-master-disable-gpio \n");

	} else {
		ret = gpio_request(global_dual_fg_info->fg_master_disable_gpio,
				   "fg_master_disable_gpio");
		if (ret) {
			bq_dbg(PR_OEM,
			       "%s: unable to request fg master gpio [%d]\n",
			       __func__,
			       global_dual_fg_info->fg_master_disable_gpio);
		}
		ret = gpio_direction_output(
			global_dual_fg_info->fg_master_disable_gpio, 0);
		if (ret) {
			bq_dbg(PR_OEM,
			       "%s: unable to set direction for fg master gpio[%d]\n",
			       __func__,
			       global_dual_fg_info->fg_master_disable_gpio);
		}
	}
	global_dual_fg_info->fg_slave_disable_gpio = of_get_named_gpio_flags(
		node, "fg-slave-disable-gpio", 0, &flags);
	if ((!gpio_is_valid(global_dual_fg_info->fg_slave_disable_gpio))) {
		bq_dbg(PR_OEM,
		       "Failed to read node of fg-slave-disable-gpio \n");

	} else {
		ret = gpio_request(global_dual_fg_info->fg_slave_disable_gpio,
				   "fg_slave_disable_gpio");
		if (ret) {
			bq_dbg(PR_OEM,
			       "%s: unable to request fg slave gpio [%d]\n",
			       __func__,
			       global_dual_fg_info->fg_slave_disable_gpio);
		}
		ret = gpio_direction_output(
			global_dual_fg_info->fg_slave_disable_gpio, 0);
		if (ret) {
			bq_dbg(PR_OEM,
			       "%s: unable to set direction for fg slave gpio[%d]\n",
			       __func__,
			       global_dual_fg_info->fg_slave_disable_gpio);
		}
	}
	return 0;
}

static int dual_fuelgauge_probe(struct platform_device *pdev)
{
	struct dual_fg_chip *bq;

	global_dual_fg_info = devm_kzalloc(
		&pdev->dev, sizeof(struct dual_fg_info), GFP_KERNEL);
	if (!global_dual_fg_info)
		return -ENOMEM;

	bq = devm_kzalloc(&pdev->dev, sizeof(struct dual_fg_chip), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->dev = &pdev->dev;
	bq->model_name = "bq27z561_dual_fg";

	bq->batt_soc = -ENODATA;
	bq->batt_fcc = -ENODATA;
	bq->batt_rm = -ENODATA;
	bq->batt_dc = -ENODATA;
	bq->batt_volt = -ENODATA;
	bq->batt_temp = -ENODATA;
	bq->batt_curr = -ENODATA;
	bq->batt_cyclecnt = -ENODATA;
	bq->batt_tte = -ENODATA;
	bq->batt_ttf = -ENODATA;
	bq->raw_soc = -ENODATA;

	bq->fake_soc = -EINVAL;
	bq->fake_temp = -EINVAL;
	bq->fake_volt = -EINVAL;
	bq->fake_chip_ok = -EINVAL;

	bq->fg_master_psy = NULL;
	bq->fg_slave_psy = NULL;

	bq_parse_dt(bq);
	platform_set_drvdata(pdev, bq);
	mutex_init(&bq->data_lock);
	fg_psy_register(bq);
	dual_fg_check_fg_matser_psy(bq);
	dual_fg_check_fg_slave_psy(bq);
	dual_fg_check_batt_psy(bq);
	fg_check_I2C_status(bq);
	fg_update_status(bq);
	fg_set_fastcharge_mode(bq, false);
	if (bq->fg_master_psy)
		global_dual_fg_info->gl_fg_master_psy = bq->fg_master_psy;
	if (bq->fg_slave_psy)
		global_dual_fg_info->gl_fg_slave_psy = bq->fg_slave_psy;

	global_dual_fg_info->fg1_batt_ctl_enabled = false;
	global_dual_fg_info->fg2_batt_ctl_enabled = false;
	/* init gpio*/
	Dual_Fuel_Gauge_Batt_Ctrl_Init();

	INIT_DELAYED_WORK(&bq->monitor_work, fg_monitor_workfunc);
	schedule_delayed_work(&bq->monitor_work, 10 * HZ);
	bq_dbg(PR_OEM, "dual fg probe successfully");
	return 0;
}

static int dual_fuelgauge_remove(struct platform_device *pdev)
{
	struct dual_fg_chip *bq = platform_get_drvdata(pdev);
	mutex_destroy(&bq->data_lock);
	fg_psy_unregister(bq);
	return 0;
}

static const struct of_device_id dual_fuelgauge_of_match[] = {
	{
		.compatible = "xiaomi,dual-FuelGauge",
	},
	{},
};

static struct platform_driver dual_fuelgauge_driver = {
	.driver = {
		.name = "dual-fuelgauge",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(dual_fuelgauge_of_match),
	},
	.probe = dual_fuelgauge_probe,
	.remove = dual_fuelgauge_remove,
};

static int __init dual_fuelgauge_init(void)
{
	return platform_driver_register(&dual_fuelgauge_driver);
}

module_init(dual_fuelgauge_init);

static void __exit dual_fuelgauge_exit(void)
{
	return platform_driver_unregister(&dual_fuelgauge_driver);
}
module_exit(dual_fuelgauge_exit);

MODULE_DESCRIPTION("Xiaomi dual fuel gauge for bq27z561");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lvxiaofeng<lvxiaofeng@xiaomi.com>");
