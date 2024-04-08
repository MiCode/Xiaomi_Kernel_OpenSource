
#include "inc/virtual_fg.h"
#define JEITA_COOL_THR_DEGREE 150
#define EXTREME_HIGH_DEGREE 1000
#define MASTER_DEFAULT_FCC_MAH 4500000
#define SLAVE_DEFAULT_FCC__MAH 4500000
#define FG_FULL 100
#define FG_RAW_FULL 10000
#define SHUTDOWN_DELAY_VOL 3300
#define BQ_MAXIUM_VOLTAGE_HYS  10000
#define BMS_FG_VERIFY		"BMS_FG_VERIFY"
#define SMART_BATTERY_FV    "SMART_BATTERY_FV"
#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef abs
#define abs(x) ((x) >0? (x) : -(x))
#endif
static int debug_mask = PR_OEM;
module_param_named(debug_mask, debug_mask, int, 0600);
struct vir_bq_fg_chip *global_vir_fg_info = NULL;

//static int log_level = 2;
#define virtualfg_err(fmt, ...)							\
do {										\
	if (log_level >= 0)							\
		printk(KERN_ERR "[virtual_FG] " fmt, ##__VA_ARGS__);	\
} while (0)
#define virtualfg_info(fmt, ...)							\
do {										\
	if (log_level >= 1)							\
		printk(KERN_ERR "[virtual_FG] " fmt, ##__VA_ARGS__);	\
} while (0)
#define virtualfg_dbg(fmt, ...)							\
do {										\
	if (log_level >= 2)							\
		printk(KERN_ERR "[virtual_FG] " fmt, ##__VA_ARGS__);	\
} while (0)

struct vir_bq_fg_chip* g_bq = NULL;

#if 0
static int fg_read_system_soc(struct dual_fg_chip *bq);
static int fg_read_volt(struct dual_fg_chip *bq);
static int fg_read_current(struct dual_fg_chip *bq, int *curr);
static int fg_get_batt_capacity_level(struct dual_fg_chip *bq);
static int fg_get_soc_decimal(struct dual_fg_chip *bq);
static int fg_get_soc_decimal_rate(struct dual_fg_chip *bq);
static int fg_get_cold_thermal_level(struct dual_fg_chip *bq);
static int fg_read_tte(struct dual_fg_chip *bq);
static int fg_read_ttf(struct dual_fg_chip *bq);
static int fg_read_cyclecount(struct dual_fg_chip *bq);
static int fg_read_soh(struct dual_fg_chip *bq);
static int fg_get_batt_status(struct dual_fg_chip *bq);
/* auto define interface for dual fg */
//static int fg_get_DesignedCapcity(struct dual_fg_chip *bq);
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
static int bq_battery_soc_smooth_tracking_new(struct dual_fg_chip *bq,int soc);
static int fg_get_property(struct power_supply *psy,
			   enum power_supply_property psp,
			   union power_supply_propval *val);
static int fg_set_property(struct power_supply *psy,
			   enum power_supply_property prop,
			   const union power_supply_propval *val);
static int fg_prop_is_writeable(struct power_supply *psy,
				enum power_supply_property prop);
#endif
static void fg_monitor_workfunc(struct work_struct *work);
static void fg_update_status(struct vir_bq_fg_chip *bq);
static int fg_update_charge_full(struct vir_bq_fg_chip *bq);
static int fg_read_fcc(struct vir_bq_fg_chip *bq);
static int fg_read_rm(struct vir_bq_fg_chip *bq);
static int fg_get_raw_soc(struct vir_bq_fg_chip *bq);
//static int fg_set_fastcharge_mode(struct vir_bq_fg_chip *bq, bool enable);
// enable or diabled fg master or slave  by GPIO
static int fg_master_set_disable(bool disable);
static int fg_slave_set_disable(bool disable);
//static bool check_qti_ops(struct vir_bq_fg_chip *bq);

static int fg_master_set_disable(bool disable)
{
	if (disable) {
		if (global_vir_fg_info->fg_master_disable_gpio)
			gpio_set_value(
				global_vir_fg_info->fg_master_disable_gpio, 1);
		virtualfg_info( "fg_master_disable_gpio= %d \n", 1);
	} else {
		if (global_vir_fg_info->fg_master_disable_gpio)
			gpio_set_value(
				global_vir_fg_info->fg_master_disable_gpio, 0);
		virtualfg_info( "fg_master_disable_gpio= %d \n", 0);
	}
	return 0;
}
static int fg_slave_set_disable(bool disable)
{
	if (disable) {
		if (global_vir_fg_info->fg_slave_disable_gpio)
			gpio_set_value(
				global_vir_fg_info->fg_slave_disable_gpio, 1);
		virtualfg_info( "fg_slave_disable_gpio= %d \n", 1);
	} else {
		if (global_vir_fg_info->fg_slave_disable_gpio)
			gpio_set_value(
				global_vir_fg_info->fg_slave_disable_gpio, 0);
		virtualfg_info( "fg_slave_disable_gpio= %d \n", 0);
	}
	return 0;
}
int Dual_Fg_Check_Chg_Fg_Status_And_Disable_Chg_Path(struct vir_bq_fg_chip *bq)
{
	#if 0
	int rc;
	int fg_master_soc, fg_slave_soc;
	int fg_master_volt, fg_slave_volt;
	int fg_master_curr, fg_slave_curr;
	int temp;
	bool effective_fv_client = false;
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
	rc = power_supply_get_property(global_vir_fg_info->gl_fg_master_psy,
				       POWER_SUPPLY_PROP_CAPACITY_RAW, &pval);
	if (rc < 0) {
		virtualfg_info( "failed get master fg SOC \n");
		return -EINVAL;
	}
	fg_master_soc = pval.intval;
	rc = power_supply_get_property(global_vir_fg_info->gl_fg_slave_psy,
				       POWER_SUPPLY_PROP_CAPACITY_RAW, &pval);
	if (rc < 0) {
		virtualfg_info( "failed get slave fg SOC \n");
		return -EINVAL;
	}
	fg_slave_soc = pval.intval;
	rc = power_supply_get_property(global_vir_fg_info->gl_fg_master_psy,
				       POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (rc < 0) {
		virtualfg_info( "failed get master fg volt \n");
		return -EINVAL;
	}
	fg_master_volt = pval.intval;
	rc = power_supply_get_property(global_vir_fg_info->gl_fg_slave_psy,
				       POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (rc < 0) {
		virtualfg_info( "failed get slave fg volt \n");
		return -EINVAL;
	}
	fg_slave_volt = pval.intval;
	rc = power_supply_get_property(global_vir_fg_info->gl_fg_master_psy,
				       POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (rc < 0) {
		virtualfg_info( "failed get master fg volt \n");
		return -EINVAL;
	}
	fg_master_curr = pval.intval;
	rc = power_supply_get_property(global_vir_fg_info->gl_fg_slave_psy,
				       POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (rc < 0) {
		virtualfg_info( "failed get slave fg volt \n");
		return -EINVAL;
	}
	fg_slave_curr = pval.intval;
	temp = fg_read_temperature(bq);
	if (!bq->fv_votable) {
		bq->fv_votable = find_votable("FV");
	}
	effective_fv_client = strcmp(get_effective_client(bq->fv_votable), SMART_BATTERY_FV);
	//virtualfg_info( "gpio master volt: %d,slave volt: %d, effective_fv_client:%d, bq->fast_mode:%d \n", fg_master_volt,
					//fg_slave_volt, effective_fv_client, bq->fast_mode);
	//virtualfg_info( "gpio master curr: %d,temp: %d\n", fg_master_curr, temp);
	/* fg master or slave  */
	if (!global_vir_fg_info->fg1_batt_ctl_enabled &&
	    !global_vir_fg_info->fg2_batt_ctl_enabled) {
		if (fg_master_soc >= fg_slave_soc) {
			if ((effective_fv_client || bq->fast_mode) && fg_master_soc == FG_RAW_FULL && fg_master_volt >= 4470000) {
				rc = fg_master_set_disable(true);
				global_vir_fg_info->fg1_batt_ctl_enabled = true;
			}
			if ((!effective_fv_client || !bq->fast_mode) && fg_master_soc == FG_RAW_FULL) {
				rc = fg_master_set_disable(true);
				global_vir_fg_info->fg1_batt_ctl_enabled = true;
			}
		} else if (fg_master_soc < fg_slave_soc) {
			if ((effective_fv_client || bq->fast_mode) && fg_slave_soc == FG_RAW_FULL && fg_slave_volt >= 4470000) {
				rc = fg_slave_set_disable(true);
				global_vir_fg_info->fg2_batt_ctl_enabled = true;
			}
			if ((!effective_fv_client || !bq->fast_mode) && fg_slave_soc == FG_RAW_FULL) {
				rc = fg_slave_set_disable(true);
				global_vir_fg_info->fg2_batt_ctl_enabled = true;
			}
		}
	}
#endif
	return 0;
}
int Dual_Fg_Reset_Batt_Ctrl_gpio_default(void)
{
	int fg_master_gpio, fg_slave_gpio;
	fg_master_gpio =
		gpio_get_value(global_vir_fg_info->fg_master_disable_gpio);
	fg_slave_gpio =
		gpio_get_value(global_vir_fg_info->fg_slave_disable_gpio);
	if (global_vir_fg_info->fg1_batt_ctl_enabled == true ||
	    fg_master_gpio == 1) {
		fg_master_set_disable(false);
		global_vir_fg_info->fg1_batt_ctl_enabled = false;
	}
	if (global_vir_fg_info->fg2_batt_ctl_enabled == true ||
	    fg_slave_gpio == 1) {
		fg_slave_set_disable(false);
		global_vir_fg_info->fg2_batt_ctl_enabled = false;
	}
	return 0;
}
int Dual_Fuel_Gauge_Batt_Ctrl_Init(void)
{
	if (global_vir_fg_info->fg_master_disable_gpio)
		gpio_set_value(global_vir_fg_info->fg_master_disable_gpio, 0);
	if (global_vir_fg_info->fg_slave_disable_gpio)
		gpio_set_value(global_vir_fg_info->fg_slave_disable_gpio, 0);
	return 0;
}

#if 0
static int fg_set_fastcharge_mode(struct vir_bq_fg_chip *bq, bool enable)
{
	int rc1, rc2;
	rc1 = global_vir_fg_info->fg_ops->set_fg1_fastcharge(enable);
	rc2 = global_vir_fg_info->fg_ops->set_fg2_fastcharge(enable);
	if (rc1 < 0 || rc2 < 0) {
		pr_err("set fast charge mode is fail!\n");
		return 0;
	}
	bq->fast_mode = enable;
	return 0;
}

static int calc_delta_time(ktime_t time_last, int *delta_time)
{
	ktime_t time_now;
	time_now = ktime_get();
	*delta_time = ktime_ms_delta(time_now, time_last);
	if (*delta_time < 0)
		*delta_time = 0;
	bq_dbg(PR_DEBUG,  "now:%ld, last:%ld, delta:%d\n", time_now, time_last, *delta_time);
	return 0;
}
#define STEP_CHG_VOTER		"STEP_CHG_VOTER"
#define STEP_CHG_LIMIT_VOTER   "STEP_CHG_LIMIT_VOTER"
static void fg_check_dual_current(struct dual_fg_chip *bq)
{
	int step_curr, curr_max, div, thres, diff_step;
	int batt_curr_master = 0, batt_curr_slave = 0;
	static int oc_count = 0,last_curr = 0,offset_Curr = 0;
	batt_curr_master = bq->batt_curr_m;
	batt_curr_slave = bq->batt_curr_s;
	if(!bq->fcc_votable)
		bq->fcc_votable = find_votable("FCC");
	step_curr = get_client_vote(bq->fcc_votable, STEP_CHG_VOTER);
	if(step_curr <= 0)
		return;
	if(step_curr != last_curr) {
		virtualfg_info("the step current:%d,last_curr: %d\n",step_curr,last_curr);
		last_curr = step_curr;
		oc_count = 0;
		diff_step = step_curr;
		vote(bq->fcc_votable, STEP_CHG_LIMIT_VOTER, false, diff_step);
	}
	thres = (step_curr == 12400000) ? 7000000 : step_curr/2;
	curr_max = abs(MIN(batt_curr_master, batt_curr_slave));
	if(curr_max >= thres) {
		oc_count++;
		offset_Curr = (offset_Curr > (curr_max - step_curr/2)) ? offset_Curr : (curr_max - step_curr/2);
	}
	virtualfg_info("oc_count:%d, before currmax:%d, half curr:%d.\n",oc_count, curr_max, thres);
	if(oc_count >= 4) {
		oc_count = 0;
		div = offset_Curr / 50000;
		if((offset_Curr % 50000) == 0)
			offset_Curr = div * 50000;
		else
			offset_Curr = (div+1) * 50000;
		diff_step -= offset_Curr * 2;
		vote(bq->fcc_votable, STEP_CHG_LIMIT_VOTER, true, diff_step);
		virtualfg_info("need down offset:%d, curr max:%d, div:%d, the diff current:%d\n", offset_Curr, curr_max, div, diff_step);
	}
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
		virtualfg_info( "failed get master fg batt voltage\n");
		return -EINVAL;
	}
	batt_volt_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (rc < 0) {
		virtualfg_info( "failed get slave fg batt voltage\n");
		return -EINVAL;
	}
	batt_volt_slave = pval.intval;
	bq->batt_volt_m = batt_volt_master;
	bq->batt_volt_s = batt_volt_slave;
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
		virtualfg_info( "failed get master fg batt current\n");
		return -EINVAL;
	}
	batt_curr_master = pval.intval;
	rc = power_supply_get_property(bq->fg_slave_psy,
				       POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (rc < 0) {
		virtualfg_info( "failed get slave fg batt current\n");
		return -EINVAL;
	}
	batt_curr_slave = pval.intval;
	bq->batt_curr_m = batt_curr_master;
	bq->batt_curr_s = batt_curr_slave;
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
		virtualfg_info( "failed get master fg charge full design \n");
		return -EINVAL;
	}
	DesignedCapa_master = pval.intval;
	rc = power_supply_get_property(
		bq->fg_slave_psy, POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &pval);
	if (rc < 0) {
		virtualfg_info( "failed get slave fg charge full design\n");
		return -EINVAL;
	}
	DesignedCapa_slave = pval.intval;
	bq->batt_dc = DesignedCapa_master + DesignedCapa_slave;
	return 0;
}
#endif

static int fg_read_fcc(struct vir_bq_fg_chip *bq)
{
	int learned_master = 0, learned_slave = 0;
	int fcc;

	if (check_qti_ops(&bq->battmg_dev)) {
		learned_master = battmngr_qtiops_get_fg1_fcc(bq->battmg_dev);
		learned_slave = battmngr_qtiops_get_fg2_fcc(bq->battmg_dev);
	} else {
		pr_err("virual fg ops is null!\n");
		return -EINVAL;
	}

	/* if rm of any battery(base or flip) decrease to 0, use default fcc of each other */
	if (!bq->batt_rm_m || !bq->batt_rm_s) {
		global_vir_fg_info->fcc_master = MASTER_DEFAULT_FCC_MAH;
		global_vir_fg_info->fcc_slave = SLAVE_DEFAULT_FCC__MAH;
	} else {
		if (global_vir_fg_info->fcc_master != learned_master)
			global_vir_fg_info->fcc_master = learned_master;
		if (global_vir_fg_info->fcc_slave != learned_slave)
			global_vir_fg_info->fcc_slave = learned_slave;
	}
	fcc = (learned_master + learned_slave) / 1000;
	return fcc;
}

#if 0
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
#define HW_REPORT_FULL_SOC 9700
#define SOC_PROPORTION 98
#define SOC_PROPORTION_C 97
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
	if (!global_vir_fg_info->fg1_batt_ctl_enabled) {
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
	if (!global_vir_fg_info->fg2_batt_ctl_enabled) {
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
	rate_of_master = global_vir_fg_info->fcc_master * 100 /
			 (global_vir_fg_info->fcc_master +
			  global_vir_fg_info->fcc_slave);
	rate_of_slave = 100 - rate_of_master;
	soc = (rate_of_master * batt_soc_master +
	       rate_of_slave * batt_soc_slave + 50) /
	      100;
	if (bq->charge_full)
		soc = 100;
	// add report 100% ahead of time function
	bq->raw_soc = fg_get_raw_soc(bq);
	soc = bq_battery_soc_smooth_tracking_new(bq,soc);
	return soc;
}
#define FFC_SMOOTH_LEN_DUAL			4
struct dual_ffc_smooth {
	int curr_lim;
	int time;
};
struct dual_ffc_smooth dual_ffc_dischg_smooth[FFC_SMOOTH_LEN_DUAL] = {
	{0,    300000},
	{300,  150000},
	{600,   72000},
	{1000,  50000},
};
static int bq_battery_soc_smooth_tracking_new(struct dual_fg_chip *bq,int soc)
{
	int rc;
	static int system_soc,last_system_soc;
	int status = 0,temp;
	int unit_time = 10000, delta_time = 0,soc_delta = 0;
	int change_delta = 0;
	static ktime_t last_change_time = -1;
	int soc_changed = 0;
	int batt_ma = 0,i;
	static int ibat_pos_count = 0;
	struct timespec64 time;
	ktime_t tmp_time = 0;
	union power_supply_propval pval = {
		0,
	};
	tmp_time = ktime_get_boottime();
	time = ktime_to_timespec64(tmp_time);
	rc = fg_read_current(bq,&batt_ma);
	temp = fg_read_temperature(bq);
	if((batt_ma > 0) && (ibat_pos_count < 10))
		ibat_pos_count++;
	else if(batt_ma <= 0)
		ibat_pos_count = 0;
	if (bq->batt_psy) {
		rc = power_supply_get_property(bq->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &pval);
		if(rc < 0) {
			virtualfg_info("Failed get battery status.\n");
			return -EINVAL;
		}
		status = pval.intval;
	}
	// Map soc value according to raw_soc
	if(bq->raw_soc > HW_REPORT_FULL_SOC)
		system_soc = 100;
	else {
		system_soc = ((bq->raw_soc + SOC_PROPORTION_C) / SOC_PROPORTION);
		if(system_soc > 99)
			system_soc = 99;
	}
	// Get the initial value for the first time
	if(last_change_time == -1) {
		last_change_time = ktime_get();
		if(system_soc != 0)
			last_system_soc = system_soc;
		else
			last_system_soc = soc;
	}
	if ((status == POWER_SUPPLY_STATUS_DISCHARGING || status == POWER_SUPPLY_STATUS_NOT_CHARGING)
		&& !bq->batt_rm && temp < 150 && last_system_soc >= 1) {
			for(i = FFC_SMOOTH_LEN_DUAL-1; i >= 0; i--) {
				if(batt_ma > dual_ffc_dischg_smooth[i].curr_lim) {
					unit_time = dual_ffc_dischg_smooth[i].time;
					break;
				}
			}
		virtualfg_info("enter low temperature smooth unit_time=%d batt_ma_now=%d\n", unit_time, batt_ma);
	}
	// If the soc jump, will smooth one cap every 10S
	soc_delta = abs(system_soc - last_system_soc);
	if(soc_delta >= 1 || (bq->batt_volt < 3300 && system_soc > 0) ||(unit_time != 10000 && soc_delta == 1)) {
		calc_delta_time(last_change_time, &change_delta);
		delta_time = change_delta / unit_time;
		if(delta_time < 0) {
			last_change_time = ktime_get();
			delta_time = 0;
		}
		soc_changed = min(1,delta_time);
		if(soc_changed) {
			if ((status == POWER_SUPPLY_STATUS_CHARGING) && (system_soc > last_system_soc))
				system_soc = last_system_soc + soc_changed;
			else if (status == POWER_SUPPLY_STATUS_DISCHARGING && system_soc < last_system_soc)
				system_soc = last_system_soc - soc_changed;
		} else {
			system_soc = last_system_soc;
		}
		virtualfg_info("system soc=%d,last system soc: %d,delta time: %d,soc_changed:%d,unit_time:%d,soc_delta:%d\n",
			system_soc,last_system_soc,delta_time,soc_changed,unit_time,soc_delta);
	}
	if(system_soc < last_system_soc)
		system_soc = last_system_soc - 1;
	// Avoid mismatches between charging status and soc changes
	if (((status != POWER_SUPPLY_STATUS_CHARGING) && (system_soc > last_system_soc))
	|| ((status == POWER_SUPPLY_STATUS_CHARGING) && (system_soc < last_system_soc) && (ibat_pos_count < 3) && ((time.tv_sec > 10))))
		system_soc = last_system_soc;
	if (system_soc != last_system_soc) {
		last_change_time = ktime_get();
		last_system_soc = system_soc;
	}
	if(system_soc > 100)
		system_soc =100;
	if(system_soc < 0)
		system_soc =0;
	if ((system_soc == 0) && ((bq->batt_volt >= 3400) || ((time.tv_sec <= 10)))) {
		system_soc = 1;
		virtualfg_info("uisoc::hold 1 when volt > 3400mv. \n");
	}
	if(bq->last_soc != system_soc){
		bq->last_soc = system_soc;
	}
	return system_soc;
}
#endif
static int fg_get_raw_soc(struct vir_bq_fg_chip *bq)
{
	int raw_soc_master, raw_soc_slave, raw_soc;
	int rate_of_master = 50, rate_of_slave = 50;

	if (check_qti_ops(&bq->battmg_dev)) {
		raw_soc_master = battmngr_qtiops_get_fg1_raw_soc(bq->battmg_dev);
		raw_soc_slave = battmngr_qtiops_get_fg2_raw_soc(bq->battmg_dev);
	} else {
		pr_err("virtual fg ops is null\n");
		return -EINVAL;
	}

	rate_of_master = global_vir_fg_info->fcc_master * 100 /
			 (global_vir_fg_info->fcc_master +
			  global_vir_fg_info->fcc_slave);
	rate_of_slave = 100 - rate_of_master;
	raw_soc = (rate_of_master * raw_soc_master +
		   rate_of_slave * raw_soc_slave + 50) /
		  100;

	bq->raw_soc_m = raw_soc_master;
	bq->raw_soc_s = raw_soc_slave;
    return raw_soc;
}
static int fg_read_rm(struct vir_bq_fg_chip *bq)
{
	int rm_master = 0, rm_slave = 0;
	int rm;

	if (check_qti_ops(&bq->battmg_dev)) {
		rm_master = battmngr_qtiops_get_fg1_rm(bq->battmg_dev);
		rm_slave = battmngr_qtiops_get_fg2_rm(bq->battmg_dev);
	} else {
		pr_err("qti virtual ops is null!\n");
		return -EINVAL;
	}

	bq->batt_rm_m = rm_master;
	bq->batt_rm_s = rm_slave;
	rm = (rm_master + rm_slave) / 1000;
	return rm;
}
#if 0
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
	if (!global_vir_fg_info->fg1_batt_ctl_enabled) {
		rc = power_supply_get_property(
			bq->fg_master_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		batt_soc_master = pval.intval;
	} else {
		batt_soc_master = FG_FULL;
	}
	if (!global_vir_fg_info->fg2_batt_ctl_enabled) {
		rc = power_supply_get_property(
			bq->fg_slave_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		batt_soc_slave = pval.intval;
	} else {
		batt_soc_slave = FG_FULL;
	}
	rate_of_master = global_vir_fg_info->fcc_master * 100 /
			 (global_vir_fg_info->fcc_master +
			  global_vir_fg_info->fcc_slave);
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
	if (global_vir_fg_info->fg1_batt_ctl_enabled || global_vir_fg_info->fg2_batt_ctl_enabled) {
		volt_max += BQ_MAXIUM_VOLTAGE_HYS;
		virtualfg_info( "add 10mv to fv\n");
	}
	if (!bq->fv_votable)
		bq->fv_votable = find_votable("FV");
	virtualfg_info( "dual_gauge voltage max:%d,effective_fv:%d\n", volt_max,get_effective_result(bq->fv_votable));
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
		virtualfg_info( "master fg or slave fg I2C failed\n");
	}
}
#endif
#if 0
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
		Dual_Fg_Check_Chg_Fg_Status_And_Disable_Chg_Path(bq);
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
					virtualfg_info( "shutdown_delay_cancel:%d, shutdown vbat_uv_2::%d\n",shutdown_delay_cancel,vbat_mv);
					if (vbat_mv > SHUTDOWN_DELAY_VOL + 50)
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
		virtualfg_info( "Failed to register fg_psy");
		return PTR_ERR(bq->fg_psy);
	}
	return 0;
}
#endif

// static bool check_qti_ops(struct vir_bq_fg_chip *bq)
// {
// 	if (bq->battmg_dev)
// 		return true;

// 	bq->battmg_dev = get_adapter_by_name("qti_ops");
// 	if (!bq->battmg_dev) {
// 		virtualfg_err("virtual fg get battmngr dev is fail");
// 		return false;
// 	}
// 	//virtualfg_info("virtual fg get battmngr dev is success");
// 	return true;
// }

/* periodically update fg data*/
static void fg_monitor_workfunc(struct work_struct *work)
{
	struct vir_bq_fg_chip *bq =
		container_of(work, struct vir_bq_fg_chip, monitor_work.work);

	if (!check_qti_ops(&bq->battmg_dev)) {
		schedule_delayed_work(&bq->monitor_work, 10 * HZ);
		return;
	}
	if (!bq->old_hw) {
		fg_update_status(bq);
		fg_update_charge_full(bq);
	}
	schedule_delayed_work(&bq->monitor_work, 10 * HZ);
}
static void fg_update_status(struct vir_bq_fg_chip *bq)
{
	static int last_st, last_soc, last_temp;
	//int vbus=0,vph=0;
	//int rc=0;
// ->fg_soc && global_vir_fg_info->fg_ops->fg_soc && global_vir_fg_info->fg_ops->fg_curr && 
			//global_vir_fg_info->fg_ops->fg_temp && global_vir_fg_info->fg_ops->charge_status
	mutex_lock(&bq->data_lock);
	pr_err("enter-1:%s\n", __func__);
	if (check_qti_ops(&bq->battmg_dev)) {
		//rc = global_vir_fg_info->fg_ops->fg_soc(&bq->batt_soc);
		bq->batt_soc = battmngr_qtiops_get_fg_soc(bq->battmg_dev);

		//rc = global_vir_fg_info->fg_ops->fg_volt(&bq->batt_volt);
		bq->batt_volt = battmngr_qtiops_get_fg_volt(bq->battmg_dev);

		//rc = global_vir_fg_info->fg_ops->fg_curr(&bq->batt_curr);
		bq->batt_curr = battmngr_qtiops_get_fg_curr(bq->battmg_dev);

		//rc = global_vir_fg_info->fg_ops->fg_temp(&bq->batt_temp);
		bq->batt_temp = battmngr_qtiops_get_fg_temp(bq->battmg_dev);

		//rc = global_vir_fg_info->fg_ops->charge_status(&bq->batt_st);
		bq->batt_st = battmngr_qtiops_get_charge_status(bq->battmg_dev);

	} else {
		pr_err("get fg ops is fail\n");
		return;
	}

	bq->batt_rm = fg_read_rm(bq);

	bq->batt_fcc = fg_read_fcc(bq);

	bq->raw_soc = fg_get_raw_soc(bq);

	//fg_get_DesignedCapcity(bq);
	//fg_check_dual_current(bq);
	// if (global_vir_fg_info->fg_ops->fg1_ibatt && global_vir_fg_info->fg_ops->fg1_volt && global_vir_fg_info->fg_ops->fg1_temp &&
	// 		global_vir_fg_info->fg_ops->fg2_ibatt && global_vir_fg_info->fg_ops->fg2_volt && global_vir_fg_info->fg_ops->fg2_temp) {

	bq->batt_curr_m = battmngr_qtiops_get_fg1_ibatt(bq->battmg_dev);

	bq->batt_volt_m = battmngr_qtiops_get_fg1_volt(bq->battmg_dev);

	bq->batt_temp_m = battmngr_qtiops_get_fg1_temp(bq->battmg_dev);

	bq->batt_curr_s = battmngr_qtiops_get_fg2_ibatt(bq->battmg_dev);

	bq->batt_volt_s = battmngr_qtiops_get_fg2_volt(bq->battmg_dev);

	bq->batt_temp_s = battmngr_qtiops_get_fg2_temp(bq->battmg_dev);

	mutex_unlock(&bq->data_lock);
	virtualfg_info("SOC:%d,Volt:%d,Cur:%d,Temp:%d,RM:%d,FC:%d,FAST:%d,Raw_soc:%d,Charging status:%d",
	       bq->batt_soc, bq->batt_volt, bq->batt_curr, bq->batt_temp,
	       bq->batt_rm, bq->batt_fcc, bq->fast_mode,bq->raw_soc,bq->batt_st);
	virtualfg_info("Print master info. Volt_m:%d, Curr_m:%d, Temp_m:%d, Raw_soc_m:%d\n",
		bq->batt_volt_m,bq->batt_curr_m,bq->batt_temp_m,bq->raw_soc_m);
	virtualfg_info("Print slave info. Volt_s:%d, Curr_s:%d, Temp_s:%d, Raw_soc_s:%d\n",
		bq->batt_volt_s,bq->batt_curr_s,bq->batt_temp_s,bq->raw_soc_s);
	if (!bq->usb_psy) {
		bq->usb_psy = power_supply_get_by_name("usb");
		if (!bq->usb_psy) {
			return;
		}
	}
	if (!bq->batt_psy) {
		bq->batt_psy = power_supply_get_by_name("battery");
		if (!bq->batt_psy) {
			return;
		}
	}
	// rc = power_supply_get_property(bq->usb_psy,
	// 			       POWER_SUPPLY_PROP_VOLTAGE_VPH, &prop);
	// vph = prop.intval;
	// rc = power_supply_get_property(bq->usb_psy,
	// 			       POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	// vbus = prop.intval;
	// virtualfg_info("Print Vbus:%d, Vph:%d\n",vbus,vph);
	if ((last_soc != bq->batt_soc) || (last_temp != bq->batt_temp) ||
	    (last_st != bq->batt_st)) {
		if (bq->batt_psy)
			power_supply_changed(bq->batt_psy);
	}
	last_soc = bq->batt_soc;
	last_temp = bq->batt_temp;
	last_st = bq->batt_st;
}
static int fg_update_charge_full(struct vir_bq_fg_chip *bq)
{
	//int rc;
	return 0;
// 	if (!bq->batt_psy) {
// 		bq->batt_psy = power_supply_get_by_name("battery");
// 		if (!bq->batt_psy) {
// 			return 0;
// 		}
// 	}
// 	rc = power_supply_get_property(bq->batt_psy,
// 				       POWER_SUPPLY_PROP_CHARGE_DONE, &prop);
// 	bq->charge_done = prop.intval;
// 	rc = power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_HEALTH,
// 				       &prop);
// 	bq->health = prop.intval;
// 	virtualfg_info( "raw:%d,done:%d,full:%d,health:%d\n", bq->raw_soc,
// 	       bq->charge_done, bq->charge_full, bq->health);
// 	if (bq->charge_done && !bq->charge_full) {
// 		if (bq->raw_soc >= BQ_REPORT_FULL_SOC) {
// 			virtualfg_info( "Setting charge_full to true\n");
// 			bq->charge_full = true;
// 		} else {
// 			virtualfg_info( "charging is done raw soc:%d\n",
// 			       bq->raw_soc);
// 		}
// 	} else if (bq->raw_soc <= BQ_CHARGE_FULL_SOC && !bq->charge_done &&
// 		   bq->charge_full) {
// 		rc = power_supply_get_property(
// 			bq->batt_psy, POWER_SUPPLY_PROP_CHARGE_DONE, &prop);
// 		bq->charge_done = prop.intval;
// 		if (bq->charge_done)
// 			goto out;
// 		bq->charge_full = false;
// 	}
// 	if ((bq->raw_soc <= BQ_RECHARGE_SOC) && bq->charge_done &&
// 	    bq->health != POWER_SUPPLY_HEALTH_WARM) {
// 		prop.intval = true;
// 		rc = power_supply_set_property(
// 			bq->batt_psy, POWER_SUPPLY_PROP_FORCE_RECHARGE, &prop);
// 		if (rc < 0) {
// 			virtualfg_info( "bq could not set force recharging!\n");
// 			return rc;
// 		}
// 		Dual_Fg_Reset_Batt_Ctrl_gpio_default();
// 	}
// 	//fix mtbf corner case
// 	if ((global_vir_fg_info->fg1_batt_ctl_enabled || global_vir_fg_info->fg2_batt_ctl_enabled)
// 		&& bq->raw_soc <= BQ_RESET_BATT_CTRL_SOC
// 		&& bq->health == POWER_SUPPLY_HEALTH_GOOD) {
// 		virtualfg_info( "reset batt ctrl\n");
// 		Dual_Fg_Reset_Batt_Ctrl_gpio_default();
// 	}
// out:
// 	return 0;
}

int battery_process_event_fg(struct battmngr_notify *noti_data)
{
	int dc_in = 0;

	dc_in = qti_get_DCIN_STATE();
	virtualfg_info("%s: dc_in %d\n", __func__, dc_in);

	if (dc_in)
		schedule_delayed_work(&g_bq->monitor_work, 10 * HZ);
	else
		cancel_delayed_work_sync(&g_bq->monitor_work);
	return 0;
}
EXPORT_SYMBOL(battery_process_event_fg);

static int bq_parse_dt(struct vir_bq_fg_chip *bq)
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
				virtualfg_info(
				       "invalid soc decimal rate seq\n");
				return -EINVAL;
			}
			of_property_read_u32_array(node, "bq,soc_decimal_rate",
						   bq->dec_rate_seq,
						   bq->dec_rate_len);
		} else {
			virtualfg_info(
			       "error allocating memory for dec_rate_seq\n");
		}
	}
	/* parse control fg master or slave gpio*/
	global_vir_fg_info->fg_master_disable_gpio = of_get_named_gpio_flags(
		node, "fg-master-disable-gpio", 0, &flags);
	if ((!gpio_is_valid(global_vir_fg_info->fg_master_disable_gpio))) {
		virtualfg_info(
		       "Failed to read node of fg-master-disable-gpio \n");
	} else {
		ret = gpio_request(global_vir_fg_info->fg_master_disable_gpio,
				   "fg_master_disable_gpio");
		if (ret) {
			virtualfg_info(
			       "%s: unable to request fg master gpio [%d]\n",
			       __func__,
			       global_vir_fg_info->fg_master_disable_gpio);
		}
		ret = gpio_direction_output(
			global_vir_fg_info->fg_master_disable_gpio, 0);
		if (ret) {
			virtualfg_info(
			       "%s: unable to set direction for fg master gpio[%d]\n",
			       __func__,
			       global_vir_fg_info->fg_master_disable_gpio);
		}
	}
	global_vir_fg_info->fg_slave_disable_gpio = of_get_named_gpio_flags(
		node, "fg-slave-disable-gpio", 0, &flags);
	if ((!gpio_is_valid(global_vir_fg_info->fg_slave_disable_gpio))) {
		virtualfg_err("Failed to read node of fg-slave-disable-gpio \n");
	} else {
		ret = gpio_request(global_vir_fg_info->fg_slave_disable_gpio,
				   "fg_slave_disable_gpio");
		if (ret) {
			virtualfg_info("%s: unable to request fg slave gpio [%d]\n", __func__,
			       global_vir_fg_info->fg_slave_disable_gpio);
		}
		ret = gpio_direction_output(
			global_vir_fg_info->fg_slave_disable_gpio, 0);
		if (ret) {
			virtualfg_info("%s: unable to set direction for fg slave gpio[%d]\n", __func__,
			       global_vir_fg_info->fg_slave_disable_gpio);
		}
	}
	return 0;
}

static int vir_fuelgauge_probe(struct platform_device *pdev)
{
	struct vir_bq_fg_chip* bq;
	static int probe_cnt = 0;
	int rc = 0;

	virtualfg_info("virtual fg probe enter: probe_cnt: %d", ++probe_cnt);
	bq = devm_kzalloc(&pdev->dev, sizeof(struct vir_bq_fg_chip), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->dev = &pdev->dev;
	platform_set_drvdata(pdev, bq);

	if (!g_bcdev) {
		virtualfg_err("%s: g_bcdev is null\n", __func__);
		rc = -EPROBE_DEFER;
		msleep(100);
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			goto g_bcdev_failure;
	}

	bq->model_name = "bq27z561_vir_fg";
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
	bq->last_soc = -ENODATA;
	bq->fake_soc = -EINVAL;
	bq->fake_temp = -EINVAL;
	bq->fake_volt = -EINVAL;
	bq->fake_chip_ok = -EINVAL;
	bq->fg_master_psy = NULL;
	bq->fg_slave_psy = NULL;

	global_vir_fg_info = bq;
	bq_parse_dt(bq);
	mutex_init(&bq->data_lock);
	//dual_fg_check_batt_psy(bq);
	//fg_update_status(bq);
	//fg_set_fastcharge_mode(bq, false);
	global_vir_fg_info->fg1_batt_ctl_enabled = false;
	global_vir_fg_info->fg2_batt_ctl_enabled = false;
	/* init gpio*/
	if (0)
		Dual_Fuel_Gauge_Batt_Ctrl_Init();
	bq->battmg_dev = get_adapter_by_name("qti_ops");
	if (!bq->battmg_dev) {
		virtualfg_err("virtual fg get battmngr dev is fail");
	}

	INIT_DELAYED_WORK(&bq->monitor_work, fg_monitor_workfunc);
	virtualfg_info("virtual fg probe successfully");
	g_bq = bq;

out:
	platform_set_drvdata(pdev, bq);
	pr_err("%s %s !!\n", __func__,
		rc == -EPROBE_DEFER ? "Over probe cnt max" : "OK");
	return 0;

g_bcdev_failure:
	return rc;
}
static int vir_fuelgauge_remove(struct platform_device *pdev)
{
	struct vir_bq_fg_chip *bq = platform_get_drvdata(pdev);

	mutex_destroy(&bq->data_lock);
	return 0;
}
static const struct of_device_id vir_fuelgauge_of_match[] = {
	{
		.compatible = "xiaomi,virtual-FuelGauge",
	},
	{},
};
static struct platform_driver vir_fuelgauge_driver = {
	.driver = {
		.name = "virtual-fuelgauge",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(vir_fuelgauge_of_match),
	},
	.probe = vir_fuelgauge_probe,
	.remove = vir_fuelgauge_remove,
};
static int __init virtual_fg_register(void)
{
	pr_err("%s: check-1\n", __func__);
	//return 0;
	return platform_driver_register(&vir_fuelgauge_driver);
}
//EXPORT_SYMBOL(virtual_fg_register);
void __exit virtual_fg_unregister(void)
{
	platform_driver_unregister(&vir_fuelgauge_driver);
}
//EXPORT_SYMBOL(virtual_fg_unregister);
module_init(virtual_fg_register);
module_exit(virtual_fg_unregister);
//MODULE_SOFTDEP("pre: qti_battery_charger");
MODULE_DESCRIPTION("Xiaomi virtual fuel gauge for bq27z561");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("litianpeng6<litianpeng6@xiaomi.com>");
