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
	xmc_info("[XMC_MONITOR] [BURN_MONITOR] control VBUS, burn = %d\n", enable);

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
	if (ret)
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

	xmc_dbg("[XMC_MONITOR] [BURN_MONITOR] ts_volt = %d, lower = %d, upper = %d, result = %d, fake_temp = %d\n",
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

	if (temp >= TYPEC_BURN_TEMP && !chip->usb_typec.burn_detect) {
		if (recheck_count < 3) {
			msleep(150);
			recheck_count++;
			goto recheck;
		}
		__pm_stay_awake(chip->usb_typec.burn_wakelock);
		chip->usb_typec.burn_detect = true;
		xmc_control_vbus(chip, true);
	} else if (temp <= TYPEC_BURN_TEMP - TYPEC_BURN_HYST && chip->usb_typec.burn_detect) {
		chip->usb_typec.burn_detect = false;
		xmc_control_vbus(chip, false);
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

	icl = chip->icl[index];
	fcc = chip->fcc[index];
	mivr = chip->mivr[index];

#ifdef CONFIG_FACTORY_BUILD
	if (!chip->usb_typec.pd_type && chip->usb_typec.bc12_type == XMC_BC12_TYPE_SDP)
		icl = 600;
#endif

	vote(chip->bbc_icl_votable, CHARGER_TYPE_VOTER, true, icl);
	vote(chip->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, fcc);
	vote(chip->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, mivr);
}

static void xmc_jeita_charge(struct charge_chip *chip)
{
	static bool jeita_vbat_low = true;

	xmc_step_jeita_get_index(chip->jeita_fv_cfg, chip->jeita_fallback_hyst, chip->jeita_forward_hyst, chip->battery.tbat, chip->jeita_index, false);
	vote(chip->bbc_fv_votable, JEITA_CHARGE_VOTER, true, chip->jeita_fv_cfg[chip->jeita_index[0]].value);

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
	xmc_step_jeita_get_index(chip->step_chg_cfg, chip->step_fallback_hyst, chip->step_forward_hyst, chip->battery.vbat, chip->step_index, false);
	vote(chip->bbc_fcc_votable, STEP_CHARGE_VOTER, true, chip->step_chg_cfg[chip->step_index[0]].value);
}

static void xmc_gauge_monitor(struct charge_chip *chip)
{
	union power_supply_propval pval = {0,};
	int ret = 0;

	ret = power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	chip->battery.rsoc = pval.intval;

	ret = power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	chip->battery.vbat = pval.intval / 1000;

	ret = power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	chip->battery.ibat = pval.intval / 1000;

	ret = power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	chip->battery.tbat = pval.intval;

	return;
}

static void xmc_charger_type_plug(struct charge_chip *chip, bool present)
{
	xmc_info("[XMC_MONITOR] charger type plug = %d\n", present);

	if (present) {
		xmc_ops_powerpath_enable(chip->bbc_dev, true);
		vote(chip->bbc_en_votable, CHARGER_TYPE_VOTER, true, 0);
		schedule_delayed_work(&chip->burn_monitor_work, 0);
	} else {
		xmc_ops_powerpath_enable(chip->bbc_dev, false);
		vote(chip->bbc_en_votable, CHARGER_TYPE_VOTER, false, 0);
		xmc_reset_charge_parameter(chip);
		xmc_parse_step_chg_config(chip, false);

		if (!chip->usb_typec.burn_detect)
			cancel_delayed_work_sync(&chip->burn_monitor_work);
	}
}

static void xmc_main_monitor_func(struct work_struct *work)
{
	struct charge_chip *chip = container_of(work, struct charge_chip, main_monitor_work.work);
	static bool charger_present = false;

	if (charger_present != (chip->usb_typec.bc12_type || chip->usb_typec.qc_type || chip->usb_typec.pd_type)) {
		charger_present = (chip->usb_typec.bc12_type || chip->usb_typec.qc_type || chip->usb_typec.pd_type);
		xmc_charger_type_plug(chip, charger_present);
	}

	xmc_gauge_monitor(chip);

	xmc_ops_get_charge_enable(chip->bbc_dev, &chip->bbc.charge_enable);
	xmc_ops_get_powerpath_enable(chip->bbc_dev, &chip->bbc.input_enable);
	xmc_ops_get_vbus(chip->bbc_dev, &chip->bbc.vbus);
	xmc_ops_get_ibus(chip->bbc_dev, &chip->bbc.ibus);

	xmc_jeita_charge(chip);
	xmc_step_charge(chip);

	xmc_info("[XMC_MONITOR] [BAT][RSOC VBAT IBAT TBAT AUTH] = [%d %d %d %d %d], [BBC][EN INPUT VBUS IBUS] = [%d %d %d %d], [TYPE][BC12 QC PD AUTH] = [%d %d %d %d], [INDEX] = [%d %d %d %d]\n",
		chip->battery.rsoc, chip->battery.vbat, chip->battery.ibat, chip->battery.tbat, chip->battery.authenticate,
		chip->bbc.charge_enable, chip->bbc.input_enable, chip->bbc.vbus, chip->bbc.ibus,
		chip->usb_typec.bc12_type, chip->usb_typec.qc_type, chip->usb_typec.pd_type, chip->adapter.authenticate_success,
		chip->jeita_index[0], chip->jeita_index[1], chip->step_index[0], chip->step_index[1]);

	if (chip->bbc.input_enable == chip->usb_typec.cmd_input_suspend) {
		xmc_ops_powerpath_enable(chip->bbc_dev, !chip->usb_typec.cmd_input_suspend);
		power_supply_changed(chip->usb_psy);
	}

	if (charger_present) {
		xmc_set_charge_parameter(chip);
	}

	schedule_delayed_work(&chip->main_monitor_work, msecs_to_jiffies(chip->main_monitor_delay * (charger_present ? 1 : 7)));
	return;
}

static void xmc_second_monitor_func(struct work_struct *work)
{
	struct charge_chip *chip = container_of(work, struct charge_chip, second_monitor_work.work);

	schedule_delayed_work(&chip->second_monitor_work, msecs_to_jiffies(chip->second_monitor_delay));
	return;
}

bool xmc_monitor_init(struct charge_chip *chip)
{
	INIT_DELAYED_WORK(&chip->main_monitor_work, xmc_main_monitor_func);
	INIT_DELAYED_WORK(&chip->second_monitor_work, xmc_second_monitor_func);
	INIT_DELAYED_WORK(&chip->burn_monitor_work, xmc_burn_monitor_func);
	xmc_reset_charge_parameter(chip);

	return true;
}
