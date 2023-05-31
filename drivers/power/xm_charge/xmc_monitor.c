/*******************************************************************************************
* Description:	XIAOMI-BSP-CHARGE
* 		This xmc_monitor.c is the main charge control algos, include STEP/JEITA, and so on.
* ------------------------------ Revision History: --------------------------------
* <version>	<date>		<author>			<desc>
* 1.0		2022-02-22	chenyichun@xiaomi.com		Created for new architecture
********************************************************************************************/

#include "xmc_core.h"

#define TYPEC_BURN_TEMP		750
#define TYPEC_BURN_HYST		100

struct ntc_desc {
	int temp;	/* 0.1 deg.C */
	int res;	/* ohm */
};

static struct ntc_desc ts_ntc_table[] = {
	{-400, 4251000},
	{-350, 3005000},
	{-300, 2149000},
	{-250, 1554000},
	{-200, 1135000},
	{-150, 837800},
	{-100, 624100},
	{-50,  469100},
	{0,    355600},
	{50,   271800},
	{100,  209400},
	{150,  162500},
	{200,  127000},
	{250,  100000},
	{300,  79230},
	{350,  63180},
	{400,  50680},
	{450,  40900},
	{500,  33190},
	{550,  27090},
	{600,  22220},
	{650,  18320},
	{700,  15180},
	{750,  12640},
	{800,  10580},
	{850,  8887},
	{900,  7500},
	{950,  6357},
	{1000, 5410},
	{1050, 4623},
	{1100, 3966},
	{1150, 3415},
	{1200, 2952},
	{1250, 2561}
};

void xmc_step_jeita_get_index(struct step_jeita_cfg0 *cfg, int fallback_hyst, int forward_hyst, int value, int *index, bool ignore_hyst)
{
	int new_index = 0, i = 0;

	if (value < cfg[0].low_threshold) {
		index[0] = index[1] = 0;
		return;
	}

	if (value > cfg[STEP_JEITA_TUPLE_NUM - 1].high_threshold)
		new_index = STEP_JEITA_TUPLE_NUM - 1;

	for (i = 0; i < STEP_JEITA_TUPLE_NUM; i++) {
		if (is_between(cfg[i].low_threshold, cfg[i].high_threshold, value)) {
			new_index = i;
			break;
		}
	}

	if (ignore_hyst) {
		index[0] = index[1] = new_index;
	} else {
		if (new_index > index[0]) {
			if (value < (cfg[new_index].low_threshold + forward_hyst))
				new_index = index[0];
		} else if (new_index < index[0]) {
			if (value > (cfg[new_index].high_threshold - fallback_hyst))
				new_index = index[0];
		}
		index[1] = index[0];
		index[0] = new_index;
	}

	return;
}

static void xmc_control_vbus(struct charge_chip *chip, bool enable)
{
	if (enable) {
		regulator_set_voltage(chip->usb_typec.vbus_control, 3000000, 3000000);
		regulator_enable(chip->usb_typec.vbus_control);
	} else {
		regulator_disable(chip->usb_typec.vbus_control);
	}
}

static int xmc_get_connector_temp(struct charge_chip *chip)
{
	int result = 0, ts_volt = 0, res = 0, lower = 0, upper = 0, i = 0, ret = 0;
	int size = sizeof(ts_ntc_table) / sizeof(ts_ntc_table[0]);

	ret = xmc_ops_get_ts(chip->bbc_dev, &ts_volt);
	if (ret || ts_volt <= 10 || ts_volt >= 1800)
		return 250;

	res = 24900 * ts_volt / (1800 - ts_volt);

	if (res >= ts_ntc_table[0].res)
		return ts_ntc_table[0].temp;
	else if (res <= ts_ntc_table[size - 1].res)
		return ts_ntc_table[size - 1].temp;

	for (i = 0; i < size; i++) {
		if (res >= ts_ntc_table[i].res) {
			upper = i;
			break;
		}
		lower = i;
	}

	result = (ts_ntc_table[lower].temp * (res - ts_ntc_table[upper].res) + ts_ntc_table[upper].temp *
			(ts_ntc_table[lower].res - res)) / (ts_ntc_table[lower].res - ts_ntc_table[upper].res);

	xmc_dbg("[BURN_MONITOR] ts_volt = %d, lower = %d, upper = %d, result = %d, fake_temp = %d\n",
		ts_volt, lower, upper, result, chip->usb_typec.fake_temp);
	return result;
}

static void xmc_burn_monitor_func(struct work_struct *work)
{
	struct charge_chip *chip = container_of(work, struct charge_chip, burn_monitor_work.work);
	int temp = 0, recheck_count = 0;

	if (IS_ERR(chip->usb_typec.vbus_control))
		return;

recheck:
	chip->usb_typec.temp = xmc_get_connector_temp(chip);
	temp = chip->usb_typec.fake_temp ? chip->usb_typec.fake_temp : chip->usb_typec.temp;

	if (temp >= TYPEC_BURN_TEMP && !chip->usb_typec.burn_detect && (chip->usb_typec.bc12_type || chip->usb_typec.qc_type || chip->usb_typec.pd_type || chip->usb_typec.otg_boost)) {
		if (recheck_count < 3) {
			msleep(150);
			recheck_count++;
			goto recheck;
		}
		__pm_stay_awake(chip->usb_typec.burn_wakelock);
		chip->usb_typec.burn_detect = true;
		xmc_info("[XMC_MONITOR] control VBUS, burn = %d, OTG = %d\n", chip->usb_typec.burn_detect, chip->usb_typec.otg_boost);
		if (chip->usb_typec.otg_boost) {
			xmc_ops_otg_vbus_enable(chip->bbc_dev, false);
			msleep(150);
		}
		if (chip->usb_typec.pd_type) {
			xmc_ops_set_cap(chip->adapter_dev, XMC_PDO_PD, 5000, 2000);
			msleep(150);
			xmc_ops_charge_enable(chip->master_cp_dev, false);
			xmc_ops_charge_enable(chip->slave_cp_dev, false);
			msleep(150);
		}
		if (chip->usb_typec.qc_type == XMC_QC_TYPE_HVCHG) {
			xmc_ops_set_dpdm_voltage(chip->bbc_dev, 0, 0);
			msleep(200);
		}
		xmc_ops_powerpath_enable(chip->bbc_dev, false);
		msleep(150);
		xmc_control_vbus(chip, true);
	} else if (temp <= TYPEC_BURN_TEMP - TYPEC_BURN_HYST && chip->usb_typec.burn_detect) {
		chip->usb_typec.burn_detect = false;
		xmc_info("[XMC_MONITOR] control VBUS, burn = %d, OTG = %d\n", chip->usb_typec.burn_detect, chip->usb_typec.otg_boost);
		xmc_control_vbus(chip, false);
		if (chip->usb_typec.otg_boost) {
			msleep(100);
			xmc_ops_otg_vbus_enable(chip->bbc_dev, true);
		}
		msleep(150);
		xmc_ops_bc12_enable(chip->bbc_dev, true);
		__pm_relax(chip->usb_typec.burn_wakelock);
	}

	schedule_delayed_work(&chip->burn_monitor_work, msecs_to_jiffies(1500));
}

static void xmc_reset_charge_parameter(struct charge_chip *chip)
{
	vote(chip->bbc_icl_votable, CHARGER_TYPE_VOTER, true, chip->icl[0]);
	vote(chip->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, chip->fcc[0]);
	vote(chip->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, chip->mivr[0]);
	vote(chip->bbc_fv_votable, FFC_VOTER, true, chip->fv);
	vote(chip->bbc_iterm_votable, FFC_VOTER, true, 200);
}

static void xmc_set_charge_parameter(struct charge_chip *chip)
{
	int index = 0;
	int icl = 0, fcc = 0, mivr = 0;

	if (chip->usb_typec.pd_type) {
		index = 8;
	} else if (chip->usb_typec.qc_type) {
		if (chip->bbc.vbus >= 7800)
			index = 4;
		else
			index = 1;
	} else {
		switch (chip->usb_typec.bc12_type) {
		case XMC_BC12_TYPE_NONE:
		case XMC_BC12_TYPE_SDP:
			index = 0;
			break;
		case XMC_BC12_TYPE_DCP:
			index = 1;
			break;
		case XMC_BC12_TYPE_CDP:
			index = 2;
			break;
		case XMC_BC12_TYPE_OCP:
		case XMC_BC12_TYPE_FLOAT:
			index = 3;
			break;
		default:
			break;
		}
	}

	icl = chip->icl[index] / (chip->monitor_count < 3 ? 2 : 1);
	fcc = chip->fcc[index] / (chip->monitor_count < 3 ? 2 : 1);
	mivr = chip->mivr[index];

#ifdef CONFIG_FACTORY_BUILD
	if (!chip->usb_typec.pd_type && chip->usb_typec.bc12_type == XMC_BC12_TYPE_SDP)
		icl = 600;
#endif

	if (chip->mtbf_test) {
		icl = max(1800, icl);
		fcc = max(1800, fcc);
		xmc_ops_charge_timer_enable(chip->bbc_dev, false);
	}

	if (chip->usb_typec.pd_type == XMC_PD_TYPE_PD3 && chip->usb_typec.bc12_type == XMC_BC12_TYPE_SDP) {
		icl = 1500;
		fcc = 1500;
		xmc_info("[XMC_MONITOR] usb C to C cable connect two phone status\n");
	}

	vote(chip->bbc_icl_votable, CHARGER_TYPE_VOTER, true, icl);
	vote(chip->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, fcc);
	vote(chip->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, mivr);
}

static void xmc_tune_fixed_pdo(struct charge_chip *chip)
{
	int i = 0, ret = 0;

	/* Only process when Fixed PDO */
	if (!chip->usb_typec.pd_type || chip->usb_typec.pd_type == XMC_PD_TYPE_PPS)
		return;

	/* Only process after plugin 5-10s */
	if (chip->monitor_count != 6 && chip->monitor_count != 7)
		return;

	if (chip->adapter.cap.nr == 0) {
		tcpm_dpm_pd_get_source_cap(chip->tcpc_dev, NULL);
		xmc_ops_get_cap(chip->adapter_dev, &chip->adapter.cap);
		if (chip->adapter.cap.nr == 0)
			return;
	}

	for (i = 0; i < chip->adapter.cap.nr; i++) {
		if (chip->adapter.cap.type[i] == TCPM_POWER_CAP_VAL_TYPE_FIXED && chip->adapter.cap.max_mv[i] >= 8000 && chip->adapter.cap.max_mv[i] <= 10000) {
			ret = xmc_ops_set_cap(chip->adapter_dev, XMC_PDO_PD, chip->adapter.cap.max_mv[i], chip->adapter.cap.ma[i]);
			if (!ret)
				vote(chip->bbc_icl_votable, FIXED_PDO_VOTER, true, chip->adapter.cap.ma[i] - 100);
			break;
		}
	}
}

static void xmc_force_bc12_wa(struct charge_chip *chip)
{
	if (chip->monitor_count == 2 && !chip->usb_typec.bc12_type && !chip->usb_typec.qc_type && !chip->usb_typec.pd_type)
		xmc_ops_bc12_enable(chip->bbc_dev, true);
}

static void xmc_thermal_charge(struct charge_chip *chip)
{
	static int last_thermal_level = 0;
	int thermal_level = 0;

	if (chip->thermal_level < 0) {
		thermal_level = -1 - chip->thermal_level;
		vote(chip->bbc_fcc_votable, SIC_VOTER, true, chip->thermal_limit[THERMAL_TABLE_NUM - 1][0]);
	} else {
		thermal_level = chip->thermal_level;
		if (chip->feature_list.sic_support) {
			thermal_level = (thermal_level <= 13) ? 0 : thermal_level;
			vote(chip->bbc_fcc_votable, SIC_VOTER, true, chip->sic_current);
		}
	}

	if (last_thermal_level < 15 && thermal_level == 15) {
		xmc_info("[XMC_SMOOTH] disable TE\n");
		xmc_ops_terminate_enable(chip->bbc_dev, false);
		vote(chip->bbc_iterm_votable, ITERM_WA_VOTER, true, 200);
		msleep(150);
	}

	if (chip->usb_typec.pd_type) {
		vote(chip->bbc_fcc_votable, THERMAL_VOTER, true, chip->thermal_limit[5][thermal_level]);
	} else if (chip->usb_typec.qc_type) {
		switch (chip->usb_typec.qc_type) {
		case XMC_QC_TYPE_HVCHG:
		case XMC_QC_TYPE_HVDCP:
		case XMC_QC_TYPE_HVDCP_2:
			vote(chip->bbc_fcc_votable, THERMAL_VOTER, true, chip->thermal_limit[1][thermal_level]);
			break;
		case XMC_QC_TYPE_HVDCP_3:
		case XMC_QC_TYPE_HVDCP_3_18W:
			vote(chip->bbc_fcc_votable, THERMAL_VOTER, true, chip->thermal_limit[2][thermal_level]);
			break;
		case XMC_QC_TYPE_HVDCP_3_27W:
			vote(chip->bbc_fcc_votable, THERMAL_VOTER, true, chip->thermal_limit[3][thermal_level]);
			break;
		case XMC_QC_TYPE_HVDCP_3P5:
			vote(chip->bbc_fcc_votable, THERMAL_VOTER, true, chip->thermal_limit[4][thermal_level]);
			break;
		case XMC_QC_TYPE_NONE:
			break;
		}
	} else if (chip->usb_typec.bc12_type == XMC_BC12_TYPE_DCP) {
		vote(chip->bbc_fcc_votable, THERMAL_VOTER, true, chip->thermal_limit[0][thermal_level]);
	}

	if (last_thermal_level == 15 && thermal_level < 15) {
		xmc_info("[XMC_SMOOTH] enable TE\n");
		msleep(150);
		xmc_ops_terminate_enable(chip->bbc_dev, true);
		vote(chip->bbc_iterm_votable, ITERM_WA_VOTER, false, 0);
	}
	last_thermal_level = thermal_level;
}

static void xmc_jeita_charge(struct charge_chip *chip)
{
	static bool jeita_vbat_low = true, first_init = true;

	xmc_step_jeita_get_index(chip->jeita_fv_cfg, chip->jeita_fallback_hyst, chip->jeita_forward_hyst, chip->battery.tbat, chip->jeita_index, first_init);
	first_init = false;
	vote(chip->bbc_fv_votable, JEITA_CHARGE_VOTER, true, chip->jeita_fv_cfg[chip->jeita_index[0]].value);

	chip->can_charge &= is_between(chip->jeita_fcc_cfg[0].low_threshold, chip->jeita_fcc_cfg[STEP_JEITA_TUPLE_NUM - 1].high_threshold, chip->battery.tbat);

	if (jeita_vbat_low) {
		if (chip->battery.vbat < (chip->jeita_fcc_cfg[chip->jeita_index[0]].extra_threshold + chip->jeita_hysteresis)) {
			vote(chip->bbc_fcc_votable, JEITA_CHARGE_VOTER, true, chip->jeita_fcc_cfg[chip->jeita_index[0]].low_value);
		} else {
			vote(chip->bbc_fcc_votable, JEITA_CHARGE_VOTER, true, chip->jeita_fcc_cfg[chip->jeita_index[0]].high_value);
			jeita_vbat_low = false;
		}
	} else {
		if (chip->battery.vbat < (chip->jeita_fcc_cfg[chip->jeita_index[0]].extra_threshold - chip->jeita_hysteresis)) {
			vote(chip->bbc_fcc_votable, JEITA_CHARGE_VOTER, true, chip->jeita_fcc_cfg[chip->jeita_index[0]].low_value);
			jeita_vbat_low = true;
		} else {
			vote(chip->bbc_fcc_votable, JEITA_CHARGE_VOTER, true, chip->jeita_fcc_cfg[chip->jeita_index[0]].high_value);
		}
	}
}

static void xmc_step_charge(struct charge_chip *chip)
{
	union power_supply_propval pval = {0,};
	static bool first_init = true;
	int count = 4, i = 0;

	xmc_step_jeita_get_index(chip->step_chg_cfg, chip->step_fallback_hyst, chip->step_forward_hyst, chip->battery.vbat, chip->step_index, first_init);
	first_init = false;

	if (chip->step_index[0] > chip->step_index[1] && chip->step_chg_cfg[chip->step_index[0]].value != chip->step_chg_cfg[chip->step_index[1]].value)
		chip->step_cv = true;

	if (chip->step_cv && (-chip->battery.ibat) < chip->step_chg_cfg[chip->step_index[0]].value && (-chip->battery.ibat) > 100) {
		for (i = 0; i <= count; i++) {
			msleep(300);
			power_supply_get_property(chip->battery_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
			pval.intval /= 1000;
			if ((-pval.intval) >= chip->step_chg_cfg[chip->step_index[0]].value || (-pval.intval) > 100) {
				if (i == count) {
					chip->step_cv = false;
					vote(chip->bbc_fcc_votable, STEP_CHARGE_VOTER, true, chip->step_chg_cfg[chip->step_index[0]].value);
				}
			} else {
				break;
			}
		}
	}
}

static void xmc_check_bat_connector(struct charge_chip *chip)
{
#ifdef CONFIG_FACTORY_BUILD
	return;
#endif

	/* If main connector is moved, limit power to 15W */
	xmc_ops_get_vbat(chip->bbc_dev, &chip->bbc.vbat);
	if (chip->bbc.vbat <= 1000 || chip->battery.i2c_error_count >= 10) {
		chip->battery.connector_remove = true;
		if (chip->bbc.vbat <= 1000)
			vote(chip->bbc_fcc_votable, NAIN_BAT_CONN_VOTER, true, 3000);
		else
			vote(chip->bbc_fcc_votable, NAIN_BAT_CONN_VOTER, false, 0);

		/* If second connector is moved, limit power to 2W */
		if (chip->battery.i2c_error_count >= 10)
			vote(chip->bbc_fcc_votable, SECOND_BAT_CONN_VOTER, true, 200);
		else
			vote(chip->bbc_fcc_votable, SECOND_BAT_CONN_VOTER, false, 0);
	} else {
		chip->battery.connector_remove = false;
	}
}

static bool xmc_gauge_smooth_uisoc(struct charge_chip *chip)
{
	union power_supply_propval val = {0,};
	static struct timespec64 last_change_time;
	struct timespec64 time_now;
	ktime_t tmp_time = 0;
	int time_per_point = 0, system_soc = 0, ibat = 0;
	static bool init_done = false;

	tmp_time = ktime_get_boottime();
	time_now = ktime_to_timespec64(tmp_time);
	chip->battery.rawsoc = 10000 * chip->battery.rm / chip->battery.fcc;
	ibat = chip->battery.ibat ? chip->battery.ibat : 1;

	if (chip->battery.rsoc > 90) {
		time_per_point = 5;
	} else {
		time_per_point = 1800 * chip->battery.typical_capacity / 100 / abs(ibat);
		if (chip->battery.soh > 0 && chip->battery.soh <= 100)
			time_per_point = time_per_point * chip->battery.soh / 100;
	}

	if (chip->battery.tbat < 50 || chip->battery.rsoc < 15)
		time_per_point /= 2;

	time_per_point = cut_cap(time_per_point, 4, 180);

	if (chip->battery.rawsoc >= chip->battery.report_full_rawsoc) {
		system_soc = 100;
	} else {
		if (chip->battery.report_full_rawsoc == 9500)
			system_soc = (chip->battery.rawsoc + 94) / 95;
		else if (chip->battery.report_full_rawsoc == 9700)
			system_soc = (chip->battery.rawsoc + 97) / 98;
		else
			system_soc = chip->battery.rawsoc / 100;

		system_soc = cut_cap(system_soc, 0, 99);
	}

	if (chip->battery.rsoc == 0)
		system_soc = 0;

	if (!init_done) {
		init_done = true;
		chip->battery.uisoc = system_soc;
		tmp_time = ktime_get_boottime();
		last_change_time = ktime_to_timespec64(tmp_time);
	}

	xmc_info("[XMC_SMOOTH] [UISOC RSOC RAWSOC SYSTEMSOC SOH] = [%d %d %d %d %d], [NOW LAST DELTA] = [%d %d %d], [RM FCC] = [%d %d], DELAY = %d\n",
		chip->battery.uisoc, chip->battery.rsoc, chip->battery.rawsoc, system_soc, chip->battery.soh,
		time_now.tv_sec, last_change_time.tv_sec, time_per_point, chip->battery.rm, chip->battery.fcc, chip->battery.shutdown_delay);

	if (chip->battery.uisoc != system_soc && time_now.tv_sec - last_change_time.tv_sec >= time_per_point) {
		power_supply_get_property(chip->battery_psy, POWER_SUPPLY_PROP_STATUS, &val);
		if (chip->battery.uisoc < system_soc) {
			if (val.intval != POWER_SUPPLY_STATUS_CHARGING && val.intval != POWER_SUPPLY_STATUS_FULL)
				return false;
			else
				chip->battery.uisoc++;
		} else {
			if (val.intval != POWER_SUPPLY_STATUS_DISCHARGING && chip->battery.uisoc - system_soc < 2 && chip->battery.vbat > 3600)
				return false;
			else
				chip->battery.uisoc--;
		}
		tmp_time = ktime_get_boottime();
		last_change_time = ktime_to_timespec64(tmp_time);
		return true;
	}

	return false;
}

static void xmc_gauge_monitor(struct charge_chip *chip)
{
	union power_supply_propval pval = {0,};
	static int last_tbat_step = 0;
	bool uisoc_change = false;

	power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	chip->battery.rsoc = pval.intval;

	power_supply_get_property(chip->battery_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	chip->battery.vbat = pval.intval / 1000;

	power_supply_get_property(chip->battery_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	chip->battery.ibat = pval.intval / 1000;

	power_supply_get_property(chip->battery_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	chip->battery.tbat = pval.intval;

	power_supply_get_property(chip->battery_psy, POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
	chip->battery.rm = pval.intval / 1000;

	power_supply_get_property(chip->battery_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &pval);
	chip->battery.fcc = pval.intval / 1000;

	xmc_ops_get_gauge_soh(chip->gauge_dev, &chip->battery.soh);
	xmc_ops_get_gauge_full(chip->gauge_dev, &chip->battery.gauge_full);

	uisoc_change = xmc_gauge_smooth_uisoc(chip);

	if (uisoc_change || chip->battery.uisoc == 0 || (chip->battery.tbat / 5) != last_tbat_step) {
		power_supply_changed(chip->battery_psy);
		if ((chip->battery.tbat / 5) != last_tbat_step)
			xmc_sysfs_report_uevent(chip->usb_psy);
		/* TBAT change every 0.5C, update */
		last_tbat_step = chip->battery.tbat / 5;
	}

	if (chip->battery.authenticate)
		vote(chip->bbc_fcc_votable, BAT_VERIFY_VOTER, false, 0);
	else
		vote(chip->bbc_fcc_votable, BAT_VERIFY_VOTER, true, 3000);

	return;
}

static void xmc_check_ffc_charge(struct charge_chip *chip)
{
	if ((chip->adapter.authenticate_success || chip->usb_typec.qc_type >= XMC_QC_TYPE_HVDCP_3_27W) &&
		chip->battery.plugin_soc <= 95 && is_between(4, 5, chip->jeita_index[0]) && !chip->recharge)
		chip->ffc_enable = true;
	else
		chip->ffc_enable = false;

	if (chip->ffc_enable)  {
		chip->fv_effective = chip->fv_ffc;
		if (chip->jeita_index[0] == 4)
			chip->iterm_effective = chip->iterm_ffc_cool;
		else
			chip->iterm_effective = chip->iterm_ffc_warm;
	} else {
		chip->fv_effective = chip->fv;
		chip->iterm_effective = chip->iterm - (chip->jeita_index[0] == 6 ? 0 : 100); /* Ensure software terminate is before wardware terminate */
	}

	if (chip->smart_fv_shift <= 100)
		chip->fv_effective -= chip->smart_fv_shift;

	xmc_ops_set_gauge_fast_charge(chip->gauge_dev, chip->ffc_enable);
	vote(chip->bbc_fv_votable, FFC_VOTER, true, chip->fv_effective);
	vote(chip->bbc_iterm_votable, FFC_VOTER, true, min(chip->iterm_effective, 600));
}

static void xmc_check_full_recharge(struct charge_chip *chip)
{
	static int full_count = 0, recharge_count = 0, fake_full_count = 0;

	if (chip->charge_full) {
		full_count = 0;
		if (chip->battery.vbat <= chip->fv_effective - 120)
			recharge_count++;
		else
			recharge_count = 0;

		if (chip->battery.rsoc < 97 || recharge_count >= 4) {
			recharge_count = 0;
			chip->charge_full = false;
			chip->recharge = true;
			xmc_ops_powerpath_enable(chip->bbc_dev, false);
			msleep(250);
			xmc_ops_powerpath_enable(chip->bbc_dev, true);
		}
	} else {
		recharge_count = 0;
		if ((chip->battery.gauge_full && (-chip->battery.ibat <= chip->iterm_effective + 100)) ||
			(chip->bbc.charge_done && (-chip->battery.ibat <= chip->iterm_effective + 100)) ||
			(!chip->ffc_enable && chip->bbc.state == CHG_STAT_DONE && (-chip->battery.ibat <= chip->iterm_effective)) ||
			(!chip->ffc_enable && chip->battery.vbat >= chip->fv_effective - 20 && (-chip->battery.ibat <= chip->iterm_effective + 100))) {
			if (chip->battery.uisoc == 100)
				full_count++;
			else
				full_count = 0;
		} else {
			full_count = 0;
		}

		if (full_count >= 4) {
			full_count = 0;
			chip->charge_full = true;
			chip->recharge = false;
			power_supply_changed(chip->battery_psy);
		}
	}

	if (chip->fake_full) {
		fake_full_count = 0;
		if (chip->jeita_index[0] != 6)
			chip->fake_full = false;
	} else {
		if (chip->jeita_index[0] == 6) {
			if ((chip->bbc.state == CHG_STAT_DONE || chip->bbc.state == CHG_STAT_EOC
				|| (chip->bbc.state == CHG_STAT_FAULT && chip->battery.vbat >= chip->jeita_fv_cfg[chip->jeita_index[0]].value - 20))
				&& (-chip->battery.ibat <= chip->iterm_effective))
				fake_full_count++;
			else
				fake_full_count = 0;
		} else {
			fake_full_count = 0;
		}

		if (fake_full_count >= 3) {
			fake_full_count = 0;
			chip->fake_full = true;
		}
	}

	chip->can_charge &= (!chip->charge_full || chip->recharge);
	chip->can_charge &= !chip->fake_full;
}

static void xmc_smart_charge(struct charge_chip *chip)
{
	if (chip->night_charging && chip->battery.uisoc >= 80)
		chip->can_charge = false;
}

static void xmc_can_charge_control(struct charge_chip *chip, bool charger_present)
{
	if (chip->bbc.input_enable == chip->usb_typec.cmd_input_suspend && (chip->pdm.state == PDM_STATE_ENTRY || chip->pdm.state == PDM_STATE_EXIT)) {
		xmc_ops_powerpath_enable(chip->bbc_dev, !chip->usb_typec.cmd_input_suspend);
		power_supply_changed(chip->usb_psy);
	} else if (chip->usb_typec.pd_type < XMC_PD_TYPE_PD3 && chip->battery.uisoc <= 10 && !chip->bbc.input_enable && chip->charger_present) {
		/* If uisoc <= 10, we force to restore power path */
		chip->usb_typec.cmd_input_suspend = false;
		xmc_ops_powerpath_enable(chip->bbc_dev, true);
	}

	if (chip->bbc.charge_enable != chip->can_charge) {
		xmc_info("[XMC_MONITOR] set BBC_ENABLE to %d\n", chip->can_charge);
		vote(chip->bbc_en_votable, BBC_ENABLE_VOTER, chip->can_charge, 0);
		xmc_ops_charge_enable(chip->bbc_dev, chip->can_charge); /* use ops to guarantee success */
	}
}

static void xmc_charger_type_plug(struct charge_chip *chip, bool present)
{
	xmc_info("[XMC_DETECT] charger type plug = %d\n", present);

	if (present) {
		chip->battery.plugin_soc = chip->battery.uisoc;
		chip->charge_full = false;
		chip->recharge = false;
		xmc_step_jeita_get_index(chip->step_chg_cfg, chip->step_fallback_hyst, chip->step_forward_hyst, chip->battery.vbat, chip->step_index, true);
		vote(chip->bbc_fcc_votable, STEP_CHARGE_VOTER, true, chip->step_chg_cfg[chip->step_index[0]].value);
		xmc_ops_powerpath_enable(chip->bbc_dev, true);
		vote(chip->bbc_en_votable, BBC_ENABLE_VOTER, true, 0);
		schedule_delayed_work(&chip->burn_monitor_work, 0);
	} else {
		chip->battery.plugin_soc = 0;
		chip->charge_full = false;
		chip->recharge = false;
		xmc_ops_powerpath_enable(chip->bbc_dev, false);
		vote(chip->bbc_en_votable, BBC_ENABLE_VOTER, false, 0);
		xmc_reset_charge_parameter(chip);
		xmc_parse_step_chg_config(chip, false);
		chip->adapter.authenticate_done = false;

		if (!chip->usb_typec.burn_detect)
			cancel_delayed_work_sync(&chip->burn_monitor_work);
	}
}

static void xmc_main_monitor_func(struct work_struct *work)
{
	struct charge_chip *chip = container_of(work, struct charge_chip, main_monitor_work.work);
	int monitor_delay = 0;

	if (chip->charger_present != (chip->usb_typec.bc12_type || chip->usb_typec.qc_type || chip->usb_typec.pd_type || chip->fake_charger_present)) {
		chip->monitor_count = 0;
		chip->charger_present = (chip->usb_typec.bc12_type || chip->usb_typec.qc_type || chip->usb_typec.pd_type || chip->fake_charger_present);
		xmc_charger_type_plug(chip, chip->charger_present);
	}

	chip->monitor_count++;
	chip->can_charge = chip->charger_present;

	xmc_gauge_monitor(chip);
	xmc_ops_get_charge_enable(chip->bbc_dev, &chip->bbc.charge_enable);
	xmc_ops_get_powerpath_enable(chip->bbc_dev, &chip->bbc.input_enable);
	xmc_ops_get_charge_state(chip->bbc_dev, &chip->bbc.state);
	xmc_ops_charge_done(chip->bbc_dev, &chip->bbc.charge_done);
	xmc_ops_get_mivr_state(chip->bbc_dev, &chip->bbc.mivr);
	xmc_ops_get_vbus(chip->bbc_dev, &chip->bbc.vbus);
	xmc_ops_get_ibus(chip->bbc_dev, &chip->bbc.ibus);

	xmc_jeita_charge(chip);
	xmc_step_charge(chip);
	xmc_check_bat_connector(chip);

	if (chip->charger_present) {
		xmc_set_charge_parameter(chip);
		xmc_force_bc12_wa(chip);
		xmc_tune_fixed_pdo(chip);
		xmc_thermal_charge(chip);
		xmc_check_ffc_charge(chip);
		xmc_check_full_recharge(chip);
		xmc_smart_charge(chip);
	}

	xmc_info("[XMC_MONITOR] [UISOC VBAT IBAT TBAT PMIC_VBAT] = [%d %d %d %d %d], [CAN EN INPUT VBUS IBUS] = [%d %d %d %d %d], [FULL FG_FULL STATE CHARHE_DONE MIVR RECHARGE] = [%d %d %d %d %d %d], [BC12 QC PD AUTH] = [%d %d %d %d], [TL SIC STEP JEITA] = [%d %d %d %d], [FFC NIGHT_CHG FV_SHIFT] = [%d %d %d]\n",
		chip->battery.uisoc, chip->battery.vbat, chip->battery.ibat, chip->battery.tbat, chip->bbc.vbat,
		chip->can_charge, chip->bbc.charge_enable, chip->bbc.input_enable, chip->bbc.vbus, chip->bbc.ibus,
		chip->charge_full, chip->battery.gauge_full, chip->bbc.state, chip->bbc.charge_done, chip->bbc.mivr, chip->recharge,
		chip->usb_typec.bc12_type, chip->usb_typec.qc_type, chip->usb_typec.pd_type, chip->adapter.authenticate_success,
		chip->thermal_level, chip->sic_current, chip->step_index[0], chip->jeita_index[0],
		chip->ffc_enable, chip->night_charging, chip->smart_fv_shift);

	if (chip->usb_typec.burn_detect || chip->usb_typec.cmd_input_suspend || !chip->battery.authenticate || chip->battery.i2c_error_count || chip->night_charging || chip->mtbf_test)
		xmc_info("[XMC_MONITOR] [BURN CMD_SUSPEND BAT_AUTH BAT_IIC_ERR HIGHT_CHG MTBF] = [%d %d %d %d %d]\n",
			chip->usb_typec.burn_detect, chip->usb_typec.cmd_input_suspend, chip->battery.authenticate, chip->battery.i2c_error_count, chip->mtbf_test);

	xmc_can_charge_control(chip, chip->charger_present);

	if (chip->charger_present) {
		monitor_delay = CHARGE_MONITOR_DELAY;
	} else {
		if (chip->battery.rsoc < 8)
			monitor_delay = FAST_MONITOR_DELAY;
		else
			monitor_delay = NOTCHARGE_MONITOR_DELAY;
	}
	schedule_delayed_work(&chip->main_monitor_work, msecs_to_jiffies(monitor_delay));
	return;
}

bool xmc_monitor_init(struct charge_chip *chip)
{
	INIT_DELAYED_WORK(&chip->main_monitor_work, xmc_main_monitor_func);
	INIT_DELAYED_WORK(&chip->burn_monitor_work, xmc_burn_monitor_func);
	xmc_reset_charge_parameter(chip);

	return true;
}
