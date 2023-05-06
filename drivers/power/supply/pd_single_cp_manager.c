#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "pmic_voter.h"
#include "mtk_charger.h"
#include "adapter_class.h"
#include "pd_cp_manager.h"
#include "sc8561_reg.h"
#include "bq28z610.h"
#include "../../misc/hwid/hwid.h"

static struct platform_driver pdm_driver;
enum pdm_sm_state {
	PD_PM_STATE_ENTRY,
	PD_PM_STATE_INIT_VBUS,
	PD_PM_STATE_ENABLE_CP,
	PD_PM_STATE_TUNE,
	PD_PM_STATE_EXIT,
};

enum pdm_sm_status {
	PDM_SM_CONTINUE,
	PDM_SM_HOLD,
	PDM_SM_EXIT,
};

struct pdm_dts_config {
	int	fv;
	int	fv_ffc;
	int	max_fcc;
	int	max_vbus;
	int	max_ibus;
	int	fcc_low_hyst;
	int	fcc_high_hyst;
	int	low_tbat;
	int	high_tbat;
	int	high_vbat;
	int	high_soc;
	int	low_fcc;
	int	cv_vbat;
	int	cv_vbat_ffc;
	int	cv_ibat;
	int	min_pdo_vbus;
	int	max_pdo_vbus;
};

struct usbpd_pm {
	struct device *dev;
	struct charger_device *master_dev;
	struct charger_device *charger_dev;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;

	struct delayed_work main_sm_work;
	struct work_struct psy_change_work;
	struct notifier_block nb;

	spinlock_t psy_change_lock;

	struct pdm_dts_config dts_config;
	struct votable *fcc_votable;
	struct votable *fv_votable;
	struct votable *icl_votable;
	struct timespec64 last_time;

	enum pdm_sm_state state;
	enum pdm_sm_status sm_status;
	enum adapter_event pd_type;
	bool	pdm_active;
	bool	psy_change_running;
	bool	master_cp_enable;
	bool	master_cp_bypass;
	bool	switch2_1_enable;
	bool	bypass_support;
	bool	switch_mode;
	bool	ffc_enable;
	bool	charge_full;
	bool	input_suspend;
	bool	typec_burn;
	bool	no_delay;
	bool	pd_verify_done;
	bool	cp_4_1_mode;
	bool	suspend_recovery;
	bool	pd_verifed;
	bool	night_charging;
	int	master_cp_vbus;
	int	master_cp_vbatt;
	int	mt6375_vbus;
	int	master_cp_ibus;
	int	total_ibus;
	int	jeita_chg_index;
	int	soc;
	int	ibat;
	int	vbat;
	int	target_fcc;
	int	thermal_limit_fcc;
	int	step_chg_fcc;
	int	sw_cv;
	int	vbus_step;
	int	ibus_step;
	int	ibat_step;
	int	vbat_step;
	int	final_step;
	int	request_voltage;
	int	request_current;
	int	entry_vbus;
	int	entry_ibus;
	int	vbus_low_gap;
	int	vbus_high_gap;
	int	tune_vbus_count;
	int	adapter_adjust_count;
	int	enable_cp_count;
	int	taper_count;

	int	apdo_max_vbus;
	int	apdo_max_ibus;
	int	apdo_max_watt;
	int vbus_control_gpio;
	int cp_work_mode;
	int bms_i2c_error_count;
	int	ibus_gap;
	int switch2_1_count;
	int switch4_1_count;
	struct adapter_power_cap cap;
};

static const unsigned char *pm_str[] = {
	"PD_PM_STATE_ENTRY",
	"PD_PM_STATE_INIT_VBUS",
	"PD_PM_STATE_ENABLE_CP",
	"PD_PM_STATE_TUNE",
	"PD_PM_STATE_EXIT",
};

static struct mtk_charger *pinfo = NULL;
static int log_level = 1;


static int vbus_low_gap_div = 800;
module_param_named(vbus_low_gap_div, vbus_low_gap_div, int, 0600);

static int vbus_high_gap_div = 1000;
module_param_named(vbus_high_gap_div, vbus_high_gap_div, int, 0600);

static int vbus_low_gap_div_swit2_1 = 400;
module_param_named(vbus_low_gap_div_swit2_1, vbus_low_gap_div_swit2_1, int, 0600);

static int vbus_high_gap_div_swit2_1 = 600;
module_param_named(vbus_high_gap_div_swit2_1, vbus_high_gap_div_swit2_1, int, 0600);

static int ibus_gap_div = 450;
module_param_named(ibus_gap_div, ibus_gap_div, int, 0600);

static int switch2_1_enter_fcc_set = 6100;
module_param_named(switch2_1_enter_fcc_set, switch2_1_enter_fcc_set, int, 0600);

static int switch2_1_exit_fcc_set = 8000;
module_param_named(switch2_1_exit_fcc_set, switch2_1_exit_fcc_set, int, 0600);

#define pdm_err(fmt, ...)						\
do {									\
	if (log_level >= 0)						\
		printk(KERN_ERR "[XMCHG_PDM] " fmt, ##__VA_ARGS__);	\
} while (0)

#define pdm_info(fmt, ...)						\
do {									\
	if (log_level >= 1)						\
		printk(KERN_ERR "[XMCHG_PDM] " fmt, ##__VA_ARGS__);	\
} while (0)

#define pdm_dbg(fmt, ...)						\
do {									\
	if (log_level >= 2)						\
		printk(KERN_ERR "[XMCHG_PDM] " fmt, ##__VA_ARGS__);	\
} while (0)

#define is_between(left, right, value)				\
		(((left) >= (right) && (left) >= (value)	\
			&& (value) >= (right))			\
		|| ((left) <= (right) && (left) <= (value)	\
			&& (value) <= (right)))

#define cut_cap(value, min, max)	((min > value) ? min : ((value > max) ? max : value))

static bool pdm_check_cp_dev(struct usbpd_pm *pdpm)
{
	if (!pdpm->master_dev)
		pdpm->master_dev = get_charger_by_name("cp_master");

	if (!pdpm->master_dev) {
		pdm_err("failed to get master_dev\n");
		return false;
	}

	if (!pdpm->charger_dev)
		pdpm->charger_dev = get_charger_by_name("primary_chg");

	if (!pdpm->charger_dev) {
		pdm_err("failed to get charger_dev\n");
		return false;
	}

	return true;
}

static bool pdm_check_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->usb_psy)
		pdpm->usb_psy = power_supply_get_by_name("usb");

	if (!pdpm->usb_psy) {
		pdm_err("failed to get usb_psy\n");
		return false;
	} else {
        pinfo = (struct mtk_charger *)power_supply_get_drvdata(pdpm->usb_psy);
	}

	if (!pdpm->bms_psy)
		pdpm->bms_psy = power_supply_get_by_name("bms");

	if (!pdpm->bms_psy) {
		pdm_err("failed to get bms_psy\n");
		return false;
	}

	if (!pdpm->batt_psy)
		pdpm->batt_psy = power_supply_get_by_name("battery");

	if (!pdpm->batt_psy) {
		pdm_err("failed to get batt_psy\n");
		return false;
	}

	return true;
}

static bool pdm_check_votable(struct usbpd_pm *pdpm)
{
	if (!pdpm->fcc_votable)
		pdpm->fcc_votable = find_votable("CHARGER_FCC");

	if (!pdpm->fcc_votable) {
		pdm_err("failed to get fcc_votable\n");
		return false;
	}

	if (!pdpm->fv_votable)
		pdpm->fv_votable = find_votable("CHARGER_FV");

	if (!pdpm->fv_votable) {
		pdm_err("failed to get fv_votable\n");
		return false;
	}

	if (!pdpm->icl_votable)
		pdpm->icl_votable = find_votable("CHARGER_ICL");

	if (!pdpm->icl_votable) {
		pdm_err("failed to get icl_votable\n");
		return false;
	}
	return true;
}

static bool pdm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
	bool legal_pdo = false;
	int ret = 0, i = 0;

	pdpm->apdo_max_vbus = 0;
	pdpm->apdo_max_ibus = 0;
	pdpm->apdo_max_watt = 0;

	ret = adapter_dev_get_cap(pinfo->pd_adapter, MTK_PD, &pdpm->cap);
	pdpm->target_fcc = get_effective_result(pdpm->fcc_votable);
	if (!pdpm->pd_verifed && pdpm->target_fcc > 5800)
		pdpm->target_fcc = 5800;

	for (i = 0; i < pdpm->cap.nr; i++) {
		pdm_info("dump PDO min_mv=%d, max_mv=%d, ibus=%d, type=%d\n", pdpm->cap.min_mv[i], pdpm->cap.max_mv[i], pdpm->cap.ma[i], pdpm->cap.type[i]);
		if (pdpm->cap.type[i] != MTK_PD_APDO || pdpm->cap.max_mv[i] < pdpm->dts_config.min_pdo_vbus || pdpm->cap.max_mv[i] > pdpm->dts_config.max_pdo_vbus)
			continue;

		if (pdpm->cap.max_mv[i] > MAX_VBUS_67W && pdpm->cap.ma[i] >= MIN_IBUS_67W_DAUMIER && !pdpm->switch2_1_enable && pdpm->target_fcc > 6000) {
			pdpm->apdo_max_vbus = pdpm->cap.max_mv[i];
			pdpm->apdo_max_ibus = pdpm->cap.ma[i];
			pdpm->apdo_max_watt = pdpm->cap.maxwatt[i];
			pdpm->cp_4_1_mode = true;
		} else if (pdpm->cap.maxwatt[i] > pdpm->apdo_max_watt) {
			pdpm->apdo_max_vbus = pdpm->cap.max_mv[i];
			pdpm->apdo_max_ibus = pdpm->cap.ma[i];
			pdpm->apdo_max_watt = pdpm->cap.maxwatt[i];
			pdpm->cp_4_1_mode = false;
		}

		legal_pdo = true;
	}
	pdm_info("pd_type = %d\n", pdpm->pd_type);
	if (legal_pdo) {
		if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
			if (pdpm->apdo_max_ibus > MAX_IBUS_67W)
				pdpm->apdo_max_ibus = MAX_IBUS_67W;

//			ret = adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO_START, DEFAULT_PDO_VBUS_1S, DEFAULT_PDO_IBUS_1S);
			pdm_info("MAX_PDO = [%d %d %d]\n", pdpm->apdo_max_vbus, pdpm->apdo_max_ibus, pdpm->apdo_max_watt / 1000000);
		} else {
			legal_pdo = false;
			pdm_err("legal_pdo false\n");
		}
	} else {
		for (i = 0; i <= pdpm->cap.nr - 1; i++) {
			if (pdpm->cap.type[i] == MTK_PD_APDO || !is_between(pdpm->dts_config.min_pdo_vbus, pdpm->dts_config.max_pdo_vbus, pdpm->cap.max_mv[i]))
				continue;

			if ( pdpm->cap.max_mv[i] == PD2_VBUS) {
				ret = adapter_dev_set_cap(pinfo->pd_adapter, MTK_PD_APDO, pdpm->cap.max_mv[i], pdpm->cap.ma[i]);
				pdm_info("MAX_fixed_PDO = [%d %d %d]\n", pdpm->cap.max_mv[i], pdpm->cap.ma[i], pdpm->cap.maxwatt[i] / 1000000);
			}
		}
	}
	pdm_info("legal_pdo = %d\n", legal_pdo);
	return legal_pdo;
}

static bool pdm_taper_charge(struct usbpd_pm *pdpm)
{
	int cv_vbat = 0, cv_vote = 0;
	cv_vote = get_effective_result(pdpm->fv_votable);
	cv_vbat = pdpm->ffc_enable ? pdpm->dts_config.cv_vbat_ffc : pdpm->dts_config.cv_vbat;
	cv_vbat = cv_vbat < (cv_vote - 20) ? cv_vbat : (cv_vote - 20);
	if (pdpm->charge_full)
		return true;

	if (pdpm->vbat > cv_vbat && (-pdpm->ibat) < pdpm->dts_config.cv_ibat)
		pdpm->taper_count++;
	else
		pdpm->taper_count = 0;
	pdm_err("pdm_taper_charge cv_vbat=%d, cv_ibat=%d, taper_count=%d\n", cv_vbat, pdpm->dts_config.cv_ibat, pdpm->taper_count);
	if (pdpm->taper_count > MAX_TAPER_COUNT)
		return true;
	else
		return false;
}

static void pdm_update_status(struct usbpd_pm *pdpm)
{
	union power_supply_propval val = {0,};
	int temp = 0;

    charger_dev_get_vbus(pdpm->master_dev, &pdpm->master_cp_vbus);
	charger_dev_get_ibus(pdpm->master_dev, &pdpm->master_cp_ibus);
	charger_dev_cp_get_vbatt(pdpm->master_dev, &pdpm->master_cp_vbatt);
	charger_dev_is_enabled(pdpm->master_dev, &pdpm->master_cp_enable);

    charger_dev_get_vbus(pdpm->charger_dev, &pdpm->mt6375_vbus);
	pdpm->total_ibus = pdpm->master_cp_ibus;
	charger_dev_cp_get_bypass_support(pdpm->master_dev, &pdpm->bypass_support);

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	pdpm->ibat = val.intval / 1000;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	pdpm->vbat = val.intval / 1000;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	pdpm->soc = val.intval;

	usb_get_property(USB_PROP_JEITA_CHG_INDEX, &temp);
	pdpm->jeita_chg_index = temp;

	usb_get_property(USB_PROP_FFC_ENABLE, &temp);
	pdpm->ffc_enable = temp;

	usb_get_property(USB_PROP_CHARGE_FULL, &temp);
	pdpm->charge_full = temp;

	usb_get_property(USB_PROP_SW_CV, &temp);
	pdpm->sw_cv = temp;

	usb_get_property(USB_PROP_INPUT_SUSPEND, &temp);
	pdpm->input_suspend = temp;

	usb_get_property(USB_PROP_TYPEC_BURN, &temp);
	pdpm->typec_burn = temp;

	usb_get_property(USB_PROP_PD_AUTHENTICATION, &temp);
	pdpm->pd_verifed = temp;

	bms_get_property(BMS_PROP_I2C_ERROR_COUNT, &temp);
	pdpm->bms_i2c_error_count = temp;

	pdpm->night_charging = night_charging_get_flag();

	pdpm->step_chg_fcc = get_client_vote(pdpm->fcc_votable, STEP_CHARGE_VOTER);
	pdpm->thermal_limit_fcc = min(get_client_vote(pdpm->fcc_votable, THERMAL_VOTER), pdpm->dts_config.max_fcc);
	pdpm->target_fcc = get_effective_result(pdpm->fcc_votable);
	if (!pdpm->pd_verifed && pdpm->target_fcc > 5800)
		pdpm->target_fcc = 5800;

	pdm_info("mt6375 vbus= %d, master_vbus=%d, master_ibus=%d, master_vbatt=%d, total_ibus=%d, master_enable=%d\n",
		pdpm->mt6375_vbus, pdpm->master_cp_vbus, pdpm->master_cp_ibus, pdpm->master_cp_vbatt, pdpm->total_ibus,
		pdpm->master_cp_enable);
	pdm_info("soc=%d, vbatt=%d, ibatt=%d, jeita_index=%d, vbatt_step=%d, ibatt_step=%d, vbus_step=%d, ibus_step=%d, final_step=%d, target_fcc=%d, sw_cv=%d, charge_full=%d, input_suspend=%d, typec_burn=%d, i2c_error_count=%d\n",
		pdpm->soc, pdpm->vbat, pdpm->ibat, pdpm->jeita_chg_index,
		pdpm->vbat_step, pdpm->ibat_step, pdpm->vbus_step, pdpm->ibus_step, pdpm->final_step,
		pdpm->target_fcc, pdpm->sw_cv, pdpm->charge_full, pdpm->input_suspend, pdpm->typec_burn, pdpm->bms_i2c_error_count);
	pdm_info("master_cp_bypass=%d, bypass_support=%d, thermal_limit_fcc=%d\n", pdpm->master_cp_bypass, pdpm->bypass_support, pdpm->thermal_limit_fcc);
}

static bool judge_whether_switch_mode(struct usbpd_pm *pdpm)
{
	if(pdpm->switch2_1_enable)
	{
		if(pdpm->target_fcc < MIN_2_1_CHARGE_CURRENT)
			return true;
		if(pdpm->target_fcc >= switch2_1_exit_fcc_set)
		{
			pdpm->switch4_1_count++;
			//2:1 can rearch 12400ma ,can remove
			if(pdpm->switch4_1_count < 10)
				pdpm->target_fcc = switch2_1_exit_fcc_set;
		}
		else
		{
			pdpm->switch4_1_count = 0;
		}
		pdm_info("switch2_1 count=%d\n",  pdpm->switch4_1_count);
		if(pdpm->switch4_1_count >= 10)
			return true;
	}
	else
	{
		if(pdpm->target_fcc <= MIN_4_1_CHARGE_CURRENT)
			return true;
		if(pdpm->target_fcc <= switch2_1_enter_fcc_set)
		{
			pdpm->switch2_1_count++;
		}
		else
		{
			pdpm->switch2_1_count = 0;
		}
		pdm_info("switch4_1 count=%d\n", pdpm->switch2_1_count);
		if(pdpm->switch2_1_count >= 10)
			return true;
	}
	pdm_info("count=[%d %d] target_fcc = %d\n", pdpm->switch2_1_count, pdpm->switch4_1_count, pdpm->target_fcc);
	return false;
	}

static void pdm_bypass_check(struct usbpd_pm *pdpm)
{
	int res = 0;
	bool switch_enable = false;
	if (pdpm->cp_work_mode == SC8561_FORWARD_4_1_CHARGER_MODE)
			res = 4;
	else if(pdpm->cp_work_mode ==SC8561_FORWARD_2_1_CHARGER_MODE)
			res = 2;
	else
			res = 1;
	switch_enable = judge_whether_switch_mode(pdpm);
	pdm_info("%s switch_enable=%d\n", __func__, switch_enable);
	if(pdpm->apdo_max_watt / 1000000 < 45)
	{
		pdpm->switch2_1_enable = true;
		pdm_info("charge max_watt < 45w, can not switch 4:1 charge\n");
	}else if(pdpm->switch2_1_enable)
	{
		if (pdpm->target_fcc >= switch2_1_exit_fcc_set && switch_enable) {
				pdpm->switch_mode = true;
				pdpm->switch2_1_enable = false;
			}else {
				pdpm->switch_mode = false;
			}
	}else if(pdpm->target_fcc <= switch2_1_enter_fcc_set && switch_enable)
	{
		pdpm->switch_mode = true;
		pdpm->switch2_1_enable = true;
	}else
	{
		pdpm->switch_mode = false;
		pdpm->switch2_1_enable = false;
	}

	pdpm->vbus_low_gap = pdpm->switch2_1_enable ? vbus_low_gap_div_swit2_1 : vbus_low_gap_div;
	pdpm->vbus_high_gap = pdpm->switch2_1_enable ? vbus_high_gap_div_swit2_1 : vbus_high_gap_div;
	pdpm->ibus_gap = ibus_gap_div;

	pdpm->entry_vbus = min(min(((pdpm->vbat * res) + pdpm->vbus_low_gap), pdpm->dts_config.max_vbus), pdpm->apdo_max_vbus);
	pdpm->entry_ibus = min(min(((pdpm->target_fcc / res) + pdpm->ibus_gap), pdpm->dts_config.max_ibus), pdpm->apdo_max_ibus);
	pdm_info("%s [%d, %d , %d , %d ] [%d, %d, %d ]\n", __func__, pdpm->switch2_1_enable, pdpm->switch_mode, pdpm->vbus_low_gap,
	pdpm->vbus_high_gap, pdpm->ibus_gap, pdpm->entry_vbus, pdpm->entry_ibus);
}

static int pdm_tune_pdo(struct usbpd_pm *pdpm)
{
	int fv = 0, ibus_limit = 0, vbus_limit = 0, request_voltage = 0, request_current = 0, final_step = 0, ret = 0, res = 0, fv_vote = 0;

    if (pdpm->cp_work_mode == SC8561_FORWARD_4_1_CHARGER_MODE)
			res = 4;
	else if(pdpm->cp_work_mode ==SC8561_FORWARD_2_1_CHARGER_MODE)
			res = 2;
	else
			res = 1;

	fv_vote = get_effective_result(pdpm->fv_votable);
	if (pdpm->sw_cv >= 4000)
	{
		fv = pdpm->sw_cv;
	}
	else
	{
		fv = pdpm->ffc_enable ? pdpm->dts_config.fv_ffc : pdpm->dts_config.fv;
		fv = fv < fv_vote ? fv : fv_vote;
	}
	if (pdpm->request_voltage > pdpm->master_cp_vbus + pdpm->total_ibus * MAX_CABLE_RESISTANCE / 1000)
	{
		pdpm->request_voltage = pdpm->master_cp_vbus + pdpm->total_ibus * MAX_CABLE_RESISTANCE / 1000;
		pdm_info("request_voltage is too over\n");
	}

	// /* reduce bus current in cv loop */
	// if(pdpm->sw_cv < 4000 && pdpm->vbat > fv - debug_fv_diff)
	// {
	// 	if(pdpm->vabt_gt_cv_count++ > debug_gt_cv_counts)
	// 	{
	// 		pdpm->vabt_gt_cv_count = 0;
	// 		pdpm->target_fcc -= BQ_TAPER_DECREASE_STEP_MA;
	// 		pdm_err("pd set taper fcc to : %d ma", pdpm->target_fcc);
	// 		if(pdpm->fcc_votable)
	// 		{
	// 			if(pdpm->target_fcc >= debug_lower_fcc){
	// 				vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER,true, pdpm->target_fcc);
	// 				pdm_err("BQ_TAPER_FCC_VOTER \n");
	// 			}
	// 		}
	// 	}
	// }
	// else
	// {
	// 	pdpm->vabt_gt_cv_count = 0;
	// }

	pdpm->ibat_step = pdpm->vbat_step = pdpm->ibus_step = pdpm->vbus_step = 0;
	ibus_limit = min(min((pdpm->target_fcc / res + pdpm->ibus_gap), pdpm->apdo_max_ibus), pdpm->dts_config.max_ibus);
	if (pdpm->apdo_max_ibus <= 3000)
		ibus_limit = min(ibus_limit, pdpm->apdo_max_ibus - 200);

	vbus_limit = min(pdpm->dts_config.max_vbus, pdpm->apdo_max_vbus);

	if ((-pdpm->ibat) < (pdpm->target_fcc - pdpm->dts_config.fcc_low_hyst)) {
		if (((pdpm->target_fcc - pdpm->dts_config.fcc_low_hyst) - (-pdpm->ibat)) > LARGE_IBAT_DIFF)
			pdpm->ibat_step = LARGE_STEP;
		else if (((pdpm->target_fcc - pdpm->dts_config.fcc_low_hyst) - (-pdpm->ibat)) > MEDIUM_IBAT_DIFF)
			pdpm->ibat_step = MEDIUM_STEP;
		else
			pdpm->ibat_step = SMALL_STEP;
	} else if ((-pdpm->ibat) > (pdpm->target_fcc + pdpm->dts_config.fcc_high_hyst)) {
		if (((-pdpm->ibat) - (pdpm->target_fcc + pdpm->dts_config.fcc_high_hyst)) > LARGE_IBAT_DIFF)
			pdpm->ibat_step = -LARGE_STEP;
		else if (((-pdpm->ibat) - (pdpm->target_fcc + pdpm->dts_config.fcc_high_hyst)) > MEDIUM_IBAT_DIFF)
			pdpm->ibat_step = -MEDIUM_STEP;
		else
			pdpm->ibat_step = -SMALL_STEP;
	} else {
		pdpm->ibat_step = 0;
	}

	if (fv - pdpm->vbat > LARGE_VBAT_DIFF)
		pdpm->vbat_step = LARGE_STEP;
	else if (fv - pdpm->vbat > MEDIUM_VBAT_DIFF)
		pdpm->vbat_step = MEDIUM_STEP;
	else if (fv - pdpm->vbat > 5)
		pdpm->vbat_step = SMALL_STEP;
	else if (fv - pdpm->vbat < -1)
		pdpm->vbat_step = -MEDIUM_STEP;
	else if (fv - pdpm->vbat <= 0)
		pdpm->vbat_step = -SMALL_STEP;
	if (ibus_limit - pdpm->total_ibus > LARGE_IBUS_DIFF)
		pdpm->ibus_step = LARGE_STEP;
	else if (ibus_limit - pdpm->total_ibus > MEDIUM_IBUS_DIFF)
		pdpm->ibus_step = MEDIUM_STEP;
	else if (ibus_limit - pdpm->total_ibus > -MEDIUM_IBUS_DIFF)
		pdpm->ibus_step = SMALL_STEP;
	else if (ibus_limit - pdpm->total_ibus < -(MEDIUM_IBUS_DIFF + 50))
		pdpm->ibus_step = -SMALL_STEP;

	if (vbus_limit - pdpm->master_cp_vbus > LARGE_VBUS_DIFF)
		pdpm->vbus_step = LARGE_STEP;
	else if (vbus_limit - pdpm->master_cp_vbus > MEDIUM_VBUS_DIFF)
		pdpm->vbus_step = MEDIUM_STEP;
	else if (vbus_limit - pdpm->master_cp_vbus > 0)
		pdpm->vbus_step = SMALL_STEP;
	else
		pdpm->vbus_step = -SMALL_STEP;

	final_step = min(min(pdpm->ibat_step, pdpm->vbat_step), min(pdpm->ibus_step, pdpm->vbus_step));
	if (pdpm->step_chg_fcc != pdpm->dts_config.max_fcc || pdpm->sw_cv ) {
		if ((pdpm->final_step == SMALL_STEP && final_step == SMALL_STEP) || (pdpm->final_step == -SMALL_STEP && final_step == -SMALL_STEP))
			final_step = 0;
			pdm_err("tune PDO enter retune final step = %d\n", final_step);
	}
	pdpm->final_step = final_step;

	if (pdpm->final_step) {
		request_voltage = min(pdpm->request_voltage + pdpm->final_step * STEP_MV, vbus_limit);
		request_current = ibus_limit;
		pdm_err("tune PDO vbus=%d, ibus=%d\n", request_voltage, request_current);
		ret = adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, request_voltage, request_current);
		if (!ret) {
			msleep(PDM_SM_DELAY_200MS);
			pdpm->request_voltage = request_voltage;
			pdpm->request_current = request_current;
		} else {
			pdm_err("failed to tune PDO\n");
		}
	}

	return ret;
}

static int pdm_check_condition(struct usbpd_pm *pdpm)
{
	int min_thermal_limit_fcc = 0;
	if(pdpm->switch2_1_enable)
		min_thermal_limit_fcc = MIN_2_1_CHARGE_CURRENT;
	else
		min_thermal_limit_fcc = MIN_4_1_CHARGE_CURRENT;
	pdm_err("min_thermal_limit_fcc =%d\n", min_thermal_limit_fcc);

	if (pdpm->state == PD_PM_STATE_TUNE && pdm_taper_charge(pdpm)) {
		pdm_err("PDM_SM_EXIT pdm_taper_charge state=%d\n", pdpm->state);
		return PDM_SM_EXIT;
	} else if (pdpm->state == PD_PM_STATE_TUNE && (!pdpm->master_cp_enable)) {
		pdm_err("PDM_SM_HOLD state=%d,cp_enable= %d\n", pdpm->state, pdpm->master_cp_enable);
		return PDM_SM_HOLD;
	}else if (pdpm->state == PD_PM_STATE_TUNE && pdpm->switch_mode) {
		pdm_err("PDM_SM_HOLD state=%d,switch_mode=%d\n", pdpm->state, pdpm->switch_mode);
		return PDM_SM_HOLD;
	} else if (pdpm->input_suspend || pdpm->typec_burn) {
		pdm_err("PDM_SM_HOLD input_suspend=%d,typec_burn=%d\n", pdpm->input_suspend, pdpm->typec_burn);
		return PDM_SM_HOLD;
	} else if (!is_between(MIN_JEITA_CHG_INDEX, MAX_JEITA_CHG_INDEX, pdpm->jeita_chg_index)) {
		pdm_err("PDM_SM_HOLD for jeita jeita_chg_index=%d\n", pdpm->jeita_chg_index);
		return PDM_SM_HOLD;
	} else if (pdpm->thermal_limit_fcc < min_thermal_limit_fcc) {
		pdm_err("PDM_SM_HOLD for fcc thermal_limit_fcc=%d\n", pdpm->thermal_limit_fcc);
		return PDM_SM_HOLD;
	} else if (pdpm->state == PD_PM_STATE_ENTRY && pdpm->soc > pdpm->dts_config.high_soc) {
		pdm_err("PDM_SM_EXIT state=%d,soc=%d,vbat=%d\n", pdpm->state, pdpm->soc, pdpm->vbat);
		return PDM_SM_EXIT;
	}else if(pdpm->night_charging && pdpm->soc >= 80)
	{
		pdm_err("PDM_SM_HOLD state=%d,soc=%d\n", pdpm->state, pdpm->soc);
		return PDM_SM_HOLD;
	}
	else if(pdpm->bms_i2c_error_count >= 10)
	{
		pdm_err("i2c_error_count=%d\n", pdpm->bms_i2c_error_count);
		return PDM_SM_EXIT;
	}
	else
		return PDM_SM_CONTINUE;
}

static void pdm_move_sm(struct usbpd_pm *pdpm, enum pdm_sm_state state)
{
	pdm_info("state change:%s -> %s\n", pm_str[pdpm->state], pm_str[state]);
	pdpm->state = state;
	pdpm->no_delay = true;
}

static bool pdm_handle_sm(struct usbpd_pm *pdpm)
{
	int ret = 0, res = 0;
	bool power_path_en = true;
	switch (pdpm->state) {
	case PD_PM_STATE_ENTRY:
		pdpm->tune_vbus_count = 0;
		pdpm->adapter_adjust_count = 0;
		pdpm->enable_cp_count = 0;
		pdpm->taper_count = 0;
		pdpm->final_step = 0;
		pdpm->step_chg_fcc = 0;

		pdpm->sm_status = pdm_check_condition(pdpm);
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdm_info("PDM_SM_EXIT, don't start sm\n");
			return true;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			break;
		} else {
			pdm_move_sm(pdpm, PD_PM_STATE_INIT_VBUS);
			if (pdpm->target_fcc > 6000 && pdpm->cp_4_1_mode && !pdpm->switch2_1_enable) {
				pdpm->cp_work_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_4_TO_1);

			}else {
			    pdpm->cp_work_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_2_TO_1);
				charger_dev_enable_powerpath(pdpm->charger_dev, false);
				msleep(200);
				charger_dev_is_powerpath_enabled(pdpm->charger_dev, &power_path_en);
				if(!pdpm->switch2_1_enable)
					pdpm->switch2_1_enable = true;
				if (power_path_en) {
					charger_dev_enable(pdpm->charger_dev, false);
					charger_dev_enable_powerpath(pdpm->charger_dev, false);
					msleep(200);
				}
			}
			charger_dev_cp_enable_adc(pdpm->master_dev, true);
			pdm_info("enter pdpm_state_entry cp_work_mode=%d\n", pdpm->cp_work_mode);
		}
		break;
	case PD_PM_STATE_INIT_VBUS:
		if (pdpm->cp_work_mode == SC8561_FORWARD_4_1_CHARGER_MODE)
			res = 4;
		else if(pdpm->cp_work_mode ==SC8561_FORWARD_2_1_CHARGER_MODE)
			res = 2;
		else
			res = 1;
		pdpm->tune_vbus_count++;
		if (pdpm->tune_vbus_count == 1) {
			if (pdpm->mt6375_vbus > 3600000 && gpio_is_valid(pdpm->vbus_control_gpio)) {
				gpio_direction_output(pdpm->vbus_control_gpio, 1);
				gpio_set_value(pdpm->vbus_control_gpio, 1); 
				pdm_err("set gpio value\n");
			}
			pdpm->request_voltage = pdpm->entry_vbus;
			pdpm->request_current = pdpm->entry_ibus;
			if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
				adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO_START, pdpm->request_voltage, pdpm->request_current);
			pdm_info("request first PDO = [%d %d]\n", pdpm->request_voltage, pdpm->request_current);
			break;
		}

		if (pdpm->tune_vbus_count >= MAX_VBUS_TUNE_COUNT) {
			pdm_err("failed to tune VBUS to target window, exit PDM\n");
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		} else if (pdpm->adapter_adjust_count >= MAX_ADAPTER_ADJUST_COUNT) {
			pdm_err("failed to request PDO, exit PDM\n");
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}

		if (pdpm->master_cp_vbus <= pdpm->vbat * res + pdpm->vbus_low_gap) {
			pdpm->request_voltage += res * STEP_MV;
		} else if (pdpm->master_cp_vbus >= pdpm->vbat * res + pdpm->vbus_high_gap) {
			pdpm->request_voltage -= res * STEP_MV;
		} else {
			pdm_info("success to tune VBUS to target window\n");
			pdm_move_sm(pdpm, PD_PM_STATE_ENABLE_CP);
			break;
		}

		pdpm->request_current = min(min(pdpm->target_fcc / res, pdpm->apdo_max_ibus), pdpm->dts_config.max_ibus);
		pdm_info("init vbus request_voltage=%d reauest_current=%d ININ_VBUS time=%d\n", pdpm->request_voltage, pdpm->request_current, pdpm->tune_vbus_count);
		if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
			ret = adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, pdpm->request_voltage, pdpm->request_current);
		if (ret == MTK_ADAPTER_REJECT) {
			pdpm->adapter_adjust_count++;
			pdm_err("failed to request PDO, try again\n");
			break;
		}
		break;
	case PD_PM_STATE_ENABLE_CP:
		pdpm->enable_cp_count++;
		if (pdpm->enable_cp_count >= MAX_ENABLE_CP_COUNT) {
			pdm_err("failed to enable charge pump, exit PDM\n");
			pdpm->sm_status = PDM_SM_EXIT;
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}
		if (!pdpm->master_cp_enable)
			charger_dev_enable(pdpm->master_dev, true);

		// if (pdpm->master_cp_bypass != pdpm->bypass_enable)
		// 	charger_dev_cp_set_mode(pdpm->master_dev, pdpm->cp_work_mode);

		if (pdpm->master_cp_enable) {
			pdm_info("success to enable charge pump\n");
			charger_dev_enable_termination(pdpm->charger_dev, false);
			pdm_move_sm(pdpm, PD_PM_STATE_TUNE);
		} else {
			pdm_err("failed to enable charge pump, master_cp =%d, try again\n", pdpm->master_cp_enable);
			break;
		}
		break;
	case PD_PM_STATE_TUNE:
		pdpm->sm_status = pdm_check_condition(pdpm);
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdm_err("taper charge done\n");
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			pdm_err("PDM_SM_HOLD\n");
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
		} else {
			ret = pdm_tune_pdo(pdpm);
			if (ret == MTK_ADAPTER_ERROR) {
				pdpm->sm_status = PDM_SM_HOLD;
				pdm_err("MTK_ADAPTER_ERROR\n");
				pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			}
		}
		break;
	case PD_PM_STATE_EXIT:
		pdpm->tune_vbus_count = 0;
		pdpm->adapter_adjust_count = 0;
		pdpm->enable_cp_count = 0;
		pdpm->taper_count = 0;
		pdpm->switch2_1_count = 0;
		pdpm->switch4_1_count = 0;

		charger_dev_enable(pdpm->master_dev, false);
		msleep(100);
#if defined(CONFIG_TARGET_PRODUCT_XAGA)
            	if (!pdpm->cp_4_1_mode) {
			charger_dev_enable_powerpath(pdpm->charger_dev, false);
			charger_dev_set_input_current(pdpm->charger_dev, 100000);
		}
#endif
		charger_dev_enable(pdpm->master_dev, false);
		if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
			adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, DEFAULT_PDO_VBUS_1S, DEFAULT_PDO_IBUS_1S);
		msleep(500);
		charger_dev_get_vbus(pdpm->charger_dev, &pdpm->mt6375_vbus);
		pdm_err("pdm exit vbus=%d\n", pdpm->mt6375_vbus);
		if (pdpm->mt6375_vbus < 10000000 && gpio_is_valid(pdpm->vbus_control_gpio) && !pdpm->night_charging)
			gpio_set_value(pdpm->vbus_control_gpio, 0);

		if (!pdpm->input_suspend && !pdpm->night_charging) {
			charger_dev_enable_powerpath(pdpm->charger_dev, true);
			if(pdpm->bms_i2c_error_count >= 3)
				vote(pinfo->icl_votable, ICL_VOTER, true, 500);
			else
				vote(pinfo->icl_votable, ICL_VOTER, true, 3000);
		}
		pdm_err("icl_effective_result = %d\n", get_effective_result(pdpm->icl_votable));
		msleep(2000);
		charger_dev_enable_termination(pdpm->charger_dev, true);

		if (pdpm->charge_full) {
			msleep(1000);
			pdm_err("charge_full pdm exit, disable charging\n");
			charger_dev_enable(pdpm->charger_dev, false);
		}
		if (pdpm->sm_status == PDM_SM_EXIT) {
			return true;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			pdm_evaluate_src_caps(pdpm);
			pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
		}

		break;
	default:
		pdm_err("not supportted pdm_sm_state\n");
		break;
	}

	return false;
}

static void pdm_main_sm(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, main_sm_work.work);
	int internal = PDM_SM_DELAY_500MS;

	pdm_update_status(pdpm);
	pdm_bypass_check(pdpm);

	if (!pdm_handle_sm(pdpm) && pdpm->pdm_active) {
		if (pdpm->no_delay) {
			internal = 0;
			pdpm->no_delay = false;
		} else {
			switch (pdpm->state) {
			case PD_PM_STATE_ENTRY:
			case PD_PM_STATE_EXIT:
			case PD_PM_STATE_INIT_VBUS:
				internal = PDM_SM_DELAY_300MS;
				break;
			case PD_PM_STATE_ENABLE_CP:
				internal = PDM_SM_DELAY_300MS;
				break;
			case PD_PM_STATE_TUNE:
				internal = PDM_SM_DELAY_500MS;
				break;
			default:
				pdm_err("not supportted pdm_sm_state\n");
				break;
			}
		}
		schedule_delayed_work(&pdpm->main_sm_work, msecs_to_jiffies(internal));
	}
}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
	cancel_delayed_work_sync(&pdpm->main_sm_work);
	charger_dev_enable(pdpm->master_dev, false);
	// if(pdpm->bypass_support)
	// {
	// 	charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
	// }
	charger_dev_cp_enable_adc(pdpm->master_dev, false);
	if (!pdpm->cp_4_1_mode && !pdpm->input_suspend) {
		charger_dev_enable_powerpath(pdpm->charger_dev, true);
		charger_dev_set_input_current(pdpm->charger_dev, 3000000);
	}

	pdpm->pd_type = MTK_PD_CONNECT_NONE;
	pdpm->pd_verify_done = false;
	pdpm->switch2_1_enable = false;
	pdpm->tune_vbus_count = 0;
	pdpm->adapter_adjust_count = 0;
	pdpm->enable_cp_count = 0;
	pdpm->apdo_max_vbus = 0;
	pdpm->apdo_max_ibus = 0;
	pdpm->apdo_max_watt = 0;
	pdpm->final_step = 0;
	pdpm->step_chg_fcc = 0;
	pdpm->thermal_limit_fcc = 0;
	pdpm->bms_i2c_error_count = 0;
	pdpm->last_time.tv_sec = 0;
	pdpm->switch2_1_count = 0;
	pdpm->switch4_1_count = 0;
	memset(&pdpm->cap, 0, sizeof(struct adapter_power_cap));

	usb_set_property(USB_PROP_APDO_MAX, 0);
	if (gpio_is_valid(pdpm->vbus_control_gpio)) {
		gpio_set_value(pdpm->vbus_control_gpio, 0);
		pdm_err("pdm exit clear gpio set\n");
	}
	pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
}

static void pdm_psy_change(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, psy_change_work); 
	int ret = 0, val = 0;
	pdm_err("pdm_psy_change\n");
	ret = usb_get_property(USB_PROP_PD_TYPE, &val);
	if (ret) {
		pdm_err("Failed to read pd type!\n");
		goto out;
	} else {
		pdpm->pd_type = val;
	}

	ret = usb_get_property(USB_PROP_PD_VERIFY_DONE, &val);
	if (ret) {
		pdm_err("Failed to read pd_verify_done!\n");
		goto out;
	} else {
		pdpm->pd_verify_done = !!val;
	}

	ret = usb_get_property(USB_PROP_CP_CHARGE_RECOVERY, &val);
	if (ret) {
                pdm_err("Failed to read suspend_recovery!\n");
                goto out;
        } else {
                pdpm->suspend_recovery = !!val;
        }

	pdm_info("[pd_type pd_verify_process pdm_active suspend_recovery] = [%d %d %d %d]\n", pdpm->pd_type, pdpm->pd_verify_done, pdpm->pdm_active, pdpm->suspend_recovery);

	if ((!pdpm->pdm_active || pdpm->suspend_recovery) && (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK || pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 || pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) && pdpm->pd_verify_done) {
		if (pdm_evaluate_src_caps(pdpm)) {
			pdpm->pdm_active = true;
			pdm_err("set main_sm_work\n");
			pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
			schedule_delayed_work(&pdpm->main_sm_work, msecs_to_jiffies(1000));
		}
	} else if (pdpm->pdm_active && pdpm->pd_type == MTK_PD_CONNECT_NONE) {
		pdpm->pdm_active = false;
		pdm_info("cancel state machine\n");
		usbpd_pm_disconnect(pdpm);
	}

out:
	pdpm->psy_change_running = false;
}

static int usbpdm_psy_notifier_cb(struct notifier_block *nb, unsigned long event, void *data)
{
	struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	spin_lock_irqsave(&pdpm->psy_change_lock, flags);
	if (strcmp(psy->desc->name, "usb") == 0 && !pdpm->psy_change_running) {
		pdpm->psy_change_running = true;
		schedule_work(&pdpm->psy_change_work);
	}
	spin_unlock_irqrestore(&pdpm->psy_change_lock, flags);

	return NOTIFY_OK;
}

static ssize_t pdm_show_log_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", log_level);
	pdm_info("show log_level = %d\n", log_level);

	return ret;
}

static ssize_t pdm_store_log_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = sscanf(buf, "%d", &log_level);
	pdm_info("store log_level = %d\n", log_level);

	return count;
}

static ssize_t pdm_show_request_vbus(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pdpm->request_voltage);

	return ret;
}

static ssize_t pdm_show_request_ibus(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pdpm->request_current);

	return ret;
}

static DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR, pdm_show_log_level, pdm_store_log_level);
static DEVICE_ATTR(request_vbus, S_IRUGO, pdm_show_request_vbus, NULL);
static DEVICE_ATTR(request_ibus, S_IRUGO, pdm_show_request_ibus, NULL);

static struct attribute *pdm_attributes[] = {
	&dev_attr_log_level.attr,
	&dev_attr_request_vbus.attr,
	&dev_attr_request_ibus.attr,
	NULL,
};

static const struct attribute_group pdm_attr_group = {
	.attrs = pdm_attributes,
};

static int pd_policy_parse_dt(struct usbpd_pm *pdpm)
{
	struct device_node *node = pdpm->dev->of_node;
	int rc = 0;

	if (!node) {
		pdm_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "fv_ffc", &pdpm->dts_config.fv_ffc);
	rc = of_property_read_u32(node, "fv", &pdpm->dts_config.fv);
	rc = of_property_read_u32(node, "max_fcc", &pdpm->dts_config.max_fcc);
	rc = of_property_read_u32(node, "max_vbus", &pdpm->dts_config.max_vbus);
	rc = of_property_read_u32(node, "max_ibus", &pdpm->dts_config.max_ibus);
	rc = of_property_read_u32(node, "fcc_low_hyst", &pdpm->dts_config.fcc_low_hyst);
	rc = of_property_read_u32(node, "fcc_high_hyst", &pdpm->dts_config.fcc_high_hyst);
	rc = of_property_read_u32(node, "low_tbat", &pdpm->dts_config.low_tbat);
	rc = of_property_read_u32(node, "high_tbat", &pdpm->dts_config.high_tbat);
	rc = of_property_read_u32(node, "high_vbat", &pdpm->dts_config.high_vbat);
	rc = of_property_read_u32(node, "high_soc", &pdpm->dts_config.high_soc);
	rc = of_property_read_u32(node, "low_fcc", &pdpm->dts_config.low_fcc);
	rc = of_property_read_u32(node, "cv_vbat", &pdpm->dts_config.cv_vbat);
	rc = of_property_read_u32(node, "cv_vbat_ffc", &pdpm->dts_config.cv_vbat_ffc);
	rc = of_property_read_u32(node, "cv_ibat", &pdpm->dts_config.cv_ibat);

	rc = of_property_read_u32(node, "vbus_low_gap_div", &vbus_low_gap_div);
	rc = of_property_read_u32(node, "vbus_high_gap_div", &vbus_high_gap_div);
	rc = of_property_read_u32(node, "min_pdo_vbus", &pdpm->dts_config.min_pdo_vbus);
	rc = of_property_read_u32(node, "max_pdo_vbus", &pdpm->dts_config.max_pdo_vbus);

	pdpm->vbus_control_gpio = of_get_named_gpio(node, "mt6375_control_gpio", 0);
	if (!gpio_is_valid(pdpm->vbus_control_gpio))
		pdm_err("failed to parse vbus_control_gpio\n");
#ifdef CONFIG_FACTORY_BUILD
	vbus_low_gap_div += 200;
	vbus_high_gap_div += 200;
#endif
	pdm_info("parse config, FV = %d, FV_FFC = %d, FCC = [%d %d %d], MAX_VBUS = %d, MAX_IBUS = %d, CV = [%d %d %d], ENTRY = [%d %d %d %d %d], PDO_GAP = [%d %d %d %d]\n",
			pdpm->dts_config.fv, pdpm->dts_config.fv_ffc, pdpm->dts_config.max_fcc, pdpm->dts_config.fcc_low_hyst, pdpm->dts_config.fcc_high_hyst,
			pdpm->dts_config.max_vbus, pdpm->dts_config.max_ibus, pdpm->dts_config.cv_vbat, pdpm->dts_config.cv_vbat_ffc, pdpm->dts_config.cv_ibat,
			pdpm->dts_config.low_tbat, pdpm->dts_config.high_tbat, pdpm->dts_config.high_vbat, pdpm->dts_config.high_soc, pdpm->dts_config.low_fcc,
			vbus_low_gap_div, vbus_high_gap_div, pdpm->dts_config.min_pdo_vbus, pdpm->dts_config.max_pdo_vbus);

	return rc;
}

#if defined(CONFIG_TARGET_PRODUCT_XAGA)
static const struct platform_device_id pdm_id[] = {
	{ "xagapro_PCM", 1},
	{ "pd_cp_manager", 2},
	{},
};
#else
static const struct platform_device_id pdm_id[] = {
	{ "pd_cp_manager", 0},
	{},
};
#endif
MODULE_DEVICE_TABLE(platform, pdm_id);

#if defined(CONFIG_TARGET_PRODUCT_XAGA)
static const struct of_device_id pdm_of_match[] = {
  	{ .compatible = "xagapro_PCM", .data = &pdm_id[0],},
	{ .compatible = "pd_cp_manager", .data = &pdm_id[1],},
	{},
};
#else
static const struct of_device_id pdm_of_match[] = {
	{ .compatible = "pd_cp_manager", },
	{},
};
#endif
MODULE_DEVICE_TABLE(of, pdm_of_match);

static int pdm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct usbpd_pm *pdpm;
	const struct of_device_id *of_id;
#if defined(CONFIG_TARGET_PRODUCT_XAGA)
	const char * buf = get_hw_sku();
	int project_no = 0;
	char *xaga = NULL;
	char *xagapro = strnstr(buf, "xagapro", strlen(buf));
	if(!xagapro)
		xaga = strnstr(buf, "xaga", strlen(buf));
	pdm_err("%s buf: %s, xaga = %d, xagapro = %d\n", __func__, buf, xaga ? 1 : 0, xagapro ? 1 : 0);
	if(xagapro){
		project_no = 1;
		pdm_err("%s ++\n", __func__);
	}
	else if(xaga){
		project_no = 2;
		pdm_err("%s exit\n", __func__);
	}
	else{
		project_no = 3;
		pdm_err("%s project_name error exit\n", __func__);
	}
#endif
	of_id = of_match_device(pdm_of_match, &pdev->dev);
	pdev->id_entry = of_id->data;
#if defined(CONFIG_TARGET_PRODUCT_XAGA)   
	if (pdev->id_entry->driver_data == project_no) {
		pdm_err("[ll] %s ++\n", __func__);
	} else {
		pdm_err("[ll] pdm_probe driver_data(%d) and cmdline(%d) not match, don't probe\n", pdev->id_entry->driver_data, project_no);
          	return -ENODEV;
	}
#endif
	pdpm = kzalloc(sizeof(struct usbpd_pm), GFP_KERNEL);
	if (!pdpm)
		return -ENOMEM;

	pdpm->dev = dev;
	pdpm->switch2_1_enable = false;
	pdpm->last_time.tv_sec = 0;
	pdpm->switch2_1_count = 0;
	pdpm->switch4_1_count = 0;
	spin_lock_init(&pdpm->psy_change_lock);
	platform_set_drvdata(pdev, pdpm);

	ret = pd_policy_parse_dt(pdpm);
	if (ret < 0) {
		pdm_err("success use pd_single_cp_manager\n");
		pdm_err("failed to parse DTS\n");
		return ret;
	}

	if (!pdm_check_cp_dev(pdpm)) {
		pdm_err("failed to check charger device\n");
		return -ENODEV;
	}

	if (!pdm_check_psy(pdpm)) {
		pdm_err("failed to check psy\n");
		return -ENODEV;
	}

    if (!pdm_check_votable(pdpm)) {
		pdm_err("failed to check votable\n");
		return -ENODEV;
	}

	if (gpio_is_valid(pdpm->vbus_control_gpio))
		gpio_direction_output(pdpm->vbus_control_gpio, 0);
	INIT_WORK(&pdpm->psy_change_work, pdm_psy_change);
	INIT_DELAYED_WORK(&pdpm->main_sm_work, pdm_main_sm);

	pdpm->nb.notifier_call = usbpdm_psy_notifier_cb;
	power_supply_reg_notifier(&pdpm->nb);
	pdpm->cp_work_mode = SC8561_FORWARD_2_1_CHARGER_MODE;

	ret = sysfs_create_group(&pdpm->dev->kobj, &pdm_attr_group);
	if (ret) {
		pdm_err("failed to register sysfs\n");
		return ret;
	}
	pdm_err("success use pd_single_cp_manager\n");
	pdm_err("PDM probe success\n");
	return ret;
}

static int pdm_remove(struct platform_device *pdev)
{
	struct usbpd_pm *pdpm = platform_get_drvdata(pdev);

	power_supply_unreg_notifier(&pdpm->nb);
	cancel_delayed_work(&pdpm->main_sm_work);
	cancel_work_sync(&pdpm->psy_change_work);

	return 0;
}

static struct platform_driver pdm_driver = {
	.driver = {
		.name = "pd_cp_manager",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pdm_of_match),
	},
	.probe = pdm_probe,
	.remove = pdm_remove,
	.id_table = pdm_id,
};

module_platform_driver(pdm_driver);
MODULE_AUTHOR("xiezhichang");
MODULE_DESCRIPTION("charge pump manager for PD");
MODULE_LICENSE("GPL");
