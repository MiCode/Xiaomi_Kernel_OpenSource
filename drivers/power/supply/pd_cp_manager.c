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
#include "mtk_battery.h"

#define QUICK_RAISE_VOLT_STEP_DELTA 2
static int pd_tune_count = 0;
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
	struct charger_device *slave_dev;
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
	struct timespec64 last_time;

	enum pdm_sm_state state;
	enum pdm_sm_status sm_status;
	enum adapter_event pd_type;
	bool	pdm_active;
	bool	psy_change_running;
	bool	master_cp_enable;
	int	master_cp_mode;
	bool	slave_cp_enable;
	int	slave_cp_mode;
	bool	bypass_support;
	bool	switch_mode;
	bool	ffc_enable;
	bool	charge_full;
	bool	input_suspend;
	bool	typec_burn;
	bool	no_delay;
	bool	pd_soft_reset;
	bool	pd_verify_done;
	bool	cp_4_1_mode;
	bool	pd_nobypass;
	bool	suspend_recovery;
	bool	pd_verifed;
	bool	night_charging;
	bool	switch2_1_enable;
	bool	switch2_1_single_enable;
	bool	switch1_1_enable;
	bool	switch1_1_single_enable;
	bool	hold_pdm_use_pmic;
	bool	cp_force_2_1_enable;
	int master_cp_vbus;
	int 	master_cp_vbus_last;
	int	master_cp_vbatt;
	int	mt6375_vbus;
	int	master_cp_ibus;
	int	slave_cp_ibus;
	int	total_ibus;
	int	jeita_chg_index;
	int	soc;
	int rsoc;
	int ibat;
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
	bool	pdm_slow_tuning;

	int	apdo_max_vbus;
	int	apdo_max_ibus;
	int	apdo_min_vbus;
	int	apdo_max_watt;
	int vbus_control_gpio;
	int cp_work_mode;
	int bms_i2c_error_count;
	int bms_slave_connect_error;
	int	ibus_gap;
	int div_rate;
	int sconfig;
	int master_chip_id;
	int slave_chip_id;
	bool support_4_1;
	struct adapter_power_cap cap;
	int cycle_count;
	int iterm;

	struct smart_chg *smart_chg;

	struct mtk_battery *batt_drvdata;
};

__maybe_unused const unsigned char *pm_str[] = {
	"PD_PM_STATE_ENTRY",
	"PD_PM_STATE_INIT_VBUS",
	"PD_PM_STATE_ENABLE_CP",
	"PD_PM_STATE_TUNE",
	"PD_PM_STATE_EXIT",
};

static struct mtk_charger *pinfo = NULL;
static int log_level = 1;

static int vbus_low_gap_div = 600;
module_param_named(vbus_low_gap_div, vbus_low_gap_div, int, 0600);

static int vbus_high_gap_div = 800;
module_param_named(vbus_high_gap_div, vbus_high_gap_div, int, 0600);

static int ibus_gap_div = 500;
module_param_named(ibus_gap_div, ibus_gap_div, int, 0600);

static int vbus_low_gap_bypass = 200;
module_param_named(vbus_low_gap_bypass, vbus_low_gap_bypass, int, 0600);

static int vbus_high_gap_bypass = 260;
module_param_named(vbus_high_gap_bypass, vbus_high_gap_bypass, int, 0600);

static int ibus_gap_bypass = 400;
module_param_named(ibus_gap_bypass, ibus_gap_bypass, int, 0600);

static int bypass_final_step = -3;
module_param_named(bypass_final_step, bypass_final_step, int, 0600);

//vbus compensation
static int pdm_vbus_low_gap = 300;
module_param_named(pdm_vbus_low_gap, pdm_vbus_low_gap, int, 0600);

static int pdm_vbus_high_gap = 500;
module_param_named(pdm_vbus_high_gap, pdm_vbus_high_gap, int, 0600);

static int pdm_vbus_low_gap_41 = 400;
module_param_named(pdm_vbus_low_gap_41, pdm_vbus_low_gap_41, int, 0600);

static int pdm_vbus_high_gap_41 = 600;
module_param_named(pdm_vbus_high_gap_41, pdm_vbus_high_gap_41, int, 0600);

static int pdm_vbus_low_gap_41_sc = 500;
module_param_named(pdm_vbus_low_gap_41_sc, pdm_vbus_low_gap_41_sc, int, 0600);

static int pdm_vbus_high_gap_41_sc = 700;
module_param_named(pdm_vbus_high_gap_41_sc, pdm_vbus_high_gap_41_sc, int, 0600);

static int pdm_vbus_low_gap_21 = 300;
module_param_named(pdm_vbus_low_gap_21, pdm_vbus_low_gap_21, int, 0600);

static int pdm_vbus_high_gap_21 = 400;
module_param_named(pdm_vbus_high_gap_21, pdm_vbus_high_gap_21, int, 0600);

static int pdm_vbus_low_gap_bypass = 250;
module_param_named(pdm_vbus_low_gap_bypass, pdm_vbus_low_gap_bypass, int, 0600);

static int pdm_vbus_high_gap_bypass = 350;
module_param_named(pdm_vbus_high_gap_bypass, pdm_vbus_high_gap_bypass, int, 0600);

static int switch2_1_enter_fcc_set = 5000;
module_param_named(switch2_1_enter_fcc_set, switch2_1_enter_fcc_set, int, 0600);

static int switch2_1_exit_fcc_set = 10000;
module_param_named(switch2_1_exit_fcc_set, switch2_1_exit_fcc_set, int, 0600);

static int switch2_1_bypass_enter_fcc_set = 3000;
module_param_named(switch2_1_bypass_enter_fcc_set, switch2_1_bypass_enter_fcc_set, int, 0600);

static int switch2_1_bypass_exit_fcc_set = 6000;
module_param_named(switch2_1_bypass_exit_fcc_set, switch2_1_bypass_exit_fcc_set, int, 0600);

static int switch1_1_single_enter_fcc_set = 1200;
module_param_named(switch1_1_single_enter_fcc_set, switch1_1_single_enter_fcc_set, int, 0600);

static int switch1_1_single_exit_fcc_set = 3000;
module_param_named(switch1_1_single_exit_fcc_set, switch1_1_single_exit_fcc_set, int, 0600);


static int pdm_ibus_gap_41 = 300;
module_param_named(pdm_ibus_gap_41, pdm_ibus_gap_41, int, 0600);

static int pdm_ibus_gap_21 = 500;
module_param_named(pdm_ibus_gap_21, pdm_ibus_gap_21, int, 0600);

static int pdm_ibus_gap_bypass = 600;
module_param_named(pdm_ibus_gap_bypass, pdm_ibus_gap_bypass, int, 0600);

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
		return false;
	}

	if (!pdpm->slave_dev)
		pdpm->slave_dev = get_charger_by_name("cp_slave");

	if (!pdpm->slave_dev) {
		return false;
	}

	if (!pdpm->charger_dev)
		pdpm->charger_dev = get_charger_by_name("primary_chg");

	if (!pdpm->charger_dev) {
		return false;
	}

	return true;
}

static bool pdm_check_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->usb_psy)
		pdpm->usb_psy = power_supply_get_by_name("usb");

	if (!pdpm->usb_psy) {
		return false;
	} else {
        pinfo = (struct mtk_charger *)power_supply_get_drvdata(pdpm->usb_psy);
	}

	if (!pdpm->bms_psy)
		pdpm->bms_psy = power_supply_get_by_name("bms");

	if (!pdpm->bms_psy) {
		return false;
	}

	if (!pdpm->batt_psy)
		pdpm->batt_psy = power_supply_get_by_name("battery");

	if (!pdpm->batt_psy) {
		return false;
	}

	return true;
}

static bool pdm_check_votable(struct usbpd_pm *pdpm)
{
	if (!pdpm->fcc_votable)
		pdpm->fcc_votable = find_votable("CHARGER_FCC");

	if (!pdpm->fcc_votable) {
		return false;
	}

	if (!pdpm->fv_votable)
		pdpm->fv_votable = find_votable("CHARGER_FV");

	if (!pdpm->fv_votable) {
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

	pdpm->cp_4_1_mode = false;
	pdpm->pd_nobypass = 1;
	ret = adapter_dev_get_cap(pinfo->pd_adapter, MTK_PD, &pdpm->cap);

	for (i = 0; i < pdpm->cap.nr; i++) {
		if (pdpm->cap.type[i] == MTK_PD_APDO && (pdpm->cap.min_mv[i] >= 5000 || pdpm->cap.ma[i] <= 1750 || (pdpm->cap.maxwatt[i]/1000000) == 33 || (pdpm->cap.maxwatt[i]/1000000) == 35)) {
		} else if(pdpm->cap.type[i] == MTK_PD_APDO && pdpm->pd_verifed){
			pdpm->pd_nobypass = 0;
		}
		if (pdpm->cap.type[i] != MTK_PD_APDO || pdpm->cap.max_mv[i] < pdpm->dts_config.min_pdo_vbus || pdpm->cap.max_mv[i] > pdpm->dts_config.max_pdo_vbus)
		{
			pdpm->div_rate = 0;
			continue;
		}
		if (pdpm->cap.max_mv[i] > MAX_VBUS_120W && pdpm->cap.ma[i] > MIN_IBUS_120W && (pdpm->cap.maxwatt[i]/1000000) != 105) {
			pdpm->cp_4_1_mode = true;
		}

		if (pdpm->cap.max_mv[i] > MAX_VBUS_120W && pdpm->cap.ma[i] > MIN_IBUS_120W && !(pdpm->switch1_1_enable || pdpm->switch1_1_single_enable || pdpm->switch2_1_enable)) {
			pdpm->apdo_max_vbus = pdpm->cap.max_mv[i];
			pdpm->apdo_max_ibus = pdpm->cap.ma[i];
			pdpm->apdo_max_watt = pdpm->cap.maxwatt[i];
			pdpm->div_rate =4;
			pdpm->apdo_min_vbus = pdpm->cap.min_mv[i];
		}else if (pdpm->cap.maxwatt[i] > pdpm->apdo_max_watt && pdpm->cap.ma[i] >= pdpm->apdo_max_ibus) {
			pdpm->apdo_max_vbus = pdpm->cap.max_mv[i];
			pdpm->apdo_max_ibus = pdpm->cap.ma[i];
			pdpm->apdo_max_watt = pdpm->cap.maxwatt[i];
			pdpm->apdo_min_vbus = pdpm->cap.min_mv[i];
			if(pdpm->switch1_1_enable || pdpm->switch1_1_single_enable || pdpm->switch2_1_enable)
				pdpm->div_rate = 1;
			else
				pdpm->div_rate = 2;
		}

		legal_pdo = true;
		set_soft_reset_status(false);
	}
	if (legal_pdo) {
		if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
			if (pdpm->apdo_max_ibus > MAX_IBUS_67W)
				pdpm->apdo_max_ibus = MAX_IBUS_67W;

		} else {
			legal_pdo = false;
		}
	} else {
		for (i = 0; i <= pdpm->cap.nr - 1; i++) {
			if (pdpm->cap.type[i] == MTK_PD_APDO || !is_between(pdpm->dts_config.min_pdo_vbus, pdpm->dts_config.max_pdo_vbus, pdpm->cap.max_mv[i]))
				continue;

			if ( pdpm->cap.max_mv[i] == PD2_VBUS) {
				ret = adapter_dev_set_cap(pinfo->pd_adapter, MTK_PD_APDO, pdpm->cap.max_mv[i], pdpm->cap.ma[i]);
			}
		}
	}
	return legal_pdo;
}

static bool pdm_taper_charge(struct usbpd_pm *pdpm)
{
	int cv_vbat = 0, cv_vote = 0;
	int cv_ibat = 0;
	if(pdpm->switch1_1_enable || pdpm->switch1_1_single_enable)
		cv_ibat = 600;
	else if(pdpm->switch2_1_enable)
		cv_ibat = 1500;
	else
		cv_ibat = pdpm->dts_config.cv_ibat;
	cv_vote = get_effective_result(pdpm->fv_votable);
	cv_vbat = pdpm->ffc_enable ? pdpm->dts_config.cv_vbat_ffc : pdpm->dts_config.cv_vbat;
	cv_vbat = cv_vbat < (cv_vote -10) ? cv_vbat : (cv_vote - 10);
	if (pdpm->charge_full)
		return true;

	if (pdpm->vbat > cv_vbat && (-pdpm->ibat) < cv_ibat)
		pdpm->taper_count++;
	else
		pdpm->taper_count = 0;
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
	charger_dev_get_ibus(pdpm->slave_dev, &pdpm->slave_cp_ibus);
	charger_dev_is_enabled(pdpm->master_dev, &pdpm->master_cp_enable);
	charger_dev_is_enabled(pdpm->slave_dev, &pdpm->slave_cp_enable);
	charger_dev_cp_get_mode(pdpm->master_dev, &pdpm->master_cp_mode);
	charger_dev_cp_get_mode(pdpm->slave_dev, &pdpm->slave_cp_mode);
	charger_dev_cp_get_chip_vendor(pdpm->master_dev, &pdpm->master_chip_id);
	charger_dev_cp_get_chip_vendor(pdpm->master_dev, &pdpm->slave_chip_id);

    charger_dev_get_vbus(pdpm->charger_dev, &pdpm->mt6375_vbus);
	pdpm->total_ibus = pdpm->master_cp_ibus + pdpm->slave_cp_ibus;
	charger_dev_cp_get_bypass_support(pdpm->master_dev, &pdpm->bypass_support);

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	pdpm->ibat = val.intval / 1000;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	pdpm->vbat = val.intval / 1000;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	pdpm->soc = val.intval;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
	pdpm->cycle_count = val.intval;

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

	bms_get_property(BMS_PROP_RSOC, &temp);
	pdpm->rsoc = temp;

	usb_get_property(USB_PROP_BATTCONT_ONLINE, &temp);
	pdpm->bms_slave_connect_error = temp;

	usb_get_property(USB_PROP_SCONFIG, &temp);
	pdpm->sconfig = temp;

	pdpm->pd_soft_reset = get_soft_reset_status();

	pdpm->night_charging = night_charging_get_flag();

	pdpm->step_chg_fcc = get_client_vote(pdpm->fcc_votable, STEP_CHARGE_VOTER);
	pdpm->thermal_limit_fcc = min(get_client_vote(pdpm->fcc_votable, THERMAL_VOTER), pdpm->dts_config.max_fcc);
	pdpm->target_fcc = get_effective_result(pdpm->fcc_votable);
	if (!pdpm->pd_verifed && pdpm->target_fcc > 5800)
		pdpm->target_fcc = 5800;

	pdpm->smart_chg = pinfo->smart_chg;
	pdpm->iterm = pinfo->iterm;
}

static void pdm_multi_mode_switch(struct usbpd_pm *pdm)
{
	if(pdm->soc <= pdm->dts_config.high_soc)
	{
	if(pdm->switch2_1_enable)
	{
		if (pdm->target_fcc >= switch2_1_exit_fcc_set && pdm->cp_4_1_mode) {
				pdm->switch_mode = true;
				pdm->switch2_1_enable = false;
			}else if(pdm->target_fcc <= switch1_1_single_enter_fcc_set && !pdm->pd_nobypass && pdm->soc > 10 && !pdm->cp_force_2_1_enable)
			{
				pdm->switch_mode = true;
				pdm->switch2_1_enable = false;
				pdm->switch1_1_single_enable = true;
			}else if(pdm->target_fcc <= switch2_1_bypass_enter_fcc_set && !pdm->pd_nobypass && pdm->soc > 10 && !pdm->cp_force_2_1_enable)
			{
				pdm->switch_mode = true;
				pdm->switch2_1_enable = false;
				pdm->switch1_1_enable = true;
			}else {
				pdm->switch_mode = false;
			}
	}else if(pdm->switch1_1_enable)
	{
		if(pdm->target_fcc >= switch2_1_exit_fcc_set && pdm->cp_4_1_mode)
		{
			pdm->switch_mode = true;
			pdm->switch2_1_enable = false;
			pdm->switch1_1_enable = false;
		}else if(pdm->target_fcc >= switch2_1_bypass_exit_fcc_set || pdm->cp_force_2_1_enable)
		{
			pdm->switch_mode = true;
			pdm->switch2_1_enable = true;
			pdm->switch1_1_enable = false;
		}else if(pdm->target_fcc <= switch1_1_single_enter_fcc_set)
		{
			pdm->switch_mode = true;
			pdm->switch1_1_single_enable = true;
			pdm->switch1_1_enable = false;
		}else
		{
			pdm->switch_mode = false;
		}
	}else if(pdm->switch1_1_single_enable)
	{
		if(pdm->target_fcc >= switch2_1_exit_fcc_set && pdm->cp_4_1_mode)
		{
			pdm->switch_mode = true;
			pdm->switch1_1_single_enable = false;
		}else if(pdm->target_fcc >= switch2_1_enter_fcc_set || pdm->cp_force_2_1_enable)
		{
			pdm->switch_mode = true;
			pdm->switch1_1_single_enable = false;
			pdm->switch2_1_enable = true;
		}else if(pdm->target_fcc >= switch1_1_single_exit_fcc_set)
		{
			pdm->switch_mode = true;
			pdm->switch1_1_single_enable = false;
			pdm->switch1_1_enable = true;
		}else{
			pdm->switch_mode = false;
		}
	}else if(pdm->target_fcc <= switch2_1_enter_fcc_set)
	{
		if(pdm->cp_force_2_1_enable)
		{
			pdm->switch_mode = true;
			pdm->switch2_1_enable = true;
		}else if(pdm->target_fcc <= switch1_1_single_enter_fcc_set && !pdm->pd_nobypass && pdm->soc > 10)
		{
			pdm->switch_mode = true;
			pdm->switch1_1_single_enable = true;
		}else if(pdm->target_fcc <= switch2_1_bypass_enter_fcc_set && !pdm->pd_nobypass && pdm->soc > 10)
		{
			pdm->switch_mode = true;
			pdm->switch1_1_enable = true;
		}else
		{
			pdm->switch_mode = true;
			pdm->switch2_1_enable = true;
		}
	}else
	{
		pdm->switch_mode = false;
		pdm->switch2_1_enable = false;
		pdm->switch1_1_enable = false;
		pdm->switch1_1_single_enable = false;
	}
	}

	if(pinfo->product_name == ZIRCON){
		if (((pdm->switch1_1_single_enable || pdm->switch1_1_enable) && (pdm->sconfig == SCCONFIG_PHONE)) ||
			(pdm->rsoc >= 98 && pdm->target_fcc <= pdm->iterm)){
		pdm->hold_pdm_use_pmic = true;
		} else {
			pdm->hold_pdm_use_pmic = false;
		}
	}

	if(pdm->batt_drvdata->hifi_connected){
		if(pdm->switch1_1_single_enable || pdm->switch1_1_enable){
			pdm->switch_mode = true;
			pdm->cp_force_2_1_enable = true;	//force cp mode to 2-1 mode if it's about to be 1-1 mode
			pdm->switch1_1_enable = false;
			pdm->switch1_1_single_enable = false;
			pdm->switch2_1_enable = true;
		}
	}else {
		pdm->cp_force_2_1_enable = false;
	}

	if(pdm->master_chip_id == SC8561 && pdm->slave_chip_id == SC8561)
	{
		pdm->vbus_low_gap = (pdm->switch1_1_enable || pdm->switch1_1_single_enable) ? pdm_vbus_low_gap_bypass : pdm->switch2_1_enable ? pdm_vbus_low_gap_21 : pdm_vbus_low_gap_41_sc;
		pdm->vbus_high_gap =  (pdm->switch1_1_enable || pdm->switch1_1_single_enable) ? pdm_vbus_high_gap_bypass : pdm->switch2_1_enable ? pdm_vbus_high_gap_21 : pdm_vbus_high_gap_41_sc;
	}else{
		pdm->vbus_low_gap = (pdm->switch1_1_enable || pdm->switch1_1_single_enable) ? pdm_vbus_low_gap_bypass : pdm->switch2_1_enable ? pdm_vbus_low_gap_21 : pdm_vbus_low_gap_41;
		pdm->vbus_high_gap =  (pdm->switch1_1_enable || pdm->switch1_1_single_enable) ? pdm_vbus_high_gap_bypass : pdm->switch2_1_enable ? pdm_vbus_high_gap_21 : pdm_vbus_high_gap_41;
	}
	pdm->ibus_gap =  (pdm->switch1_1_enable || pdm->switch1_1_single_enable) ? pdm_ibus_gap_bypass : pdm->switch2_1_enable ? pdm_ibus_gap_21 : pdm_ibus_gap_41;
	if(pdm->sconfig == 18 || pdm->sconfig == 17 || pdm->sconfig == 20 || pdm->sconfig == 9 || pdm->sconfig == 23 ||pdm->sconfig == 4 || pdm->sconfig == 34) //8K\4K\mgame\tgame\yuanshen\videochat\per-videochat
	{
		pdm->ibus_gap = pdm->ibus_gap*2;
		if((pdm->ibus_gap * pdm->div_rate) > 1200)
		{
			pdm->ibus_gap = 1200 / pdm->div_rate;
		}
	}

	pdm->entry_vbus = min(min(((pdm->vbat * pdm->div_rate) + pdm->vbus_low_gap), pdm->dts_config.max_vbus), pdm->apdo_max_vbus);
	pdm->entry_ibus = min(min(((pdm->target_fcc / pdm->div_rate) + pdm->ibus_gap), pdm->dts_config.max_ibus), pdm->apdo_max_ibus);
	if (pdm->soc > 85)
		pdm->entry_vbus = min(min(pdm->entry_vbus - 360, pdm->dts_config.max_vbus), pdm->apdo_max_vbus);
}

static int pdm_tune_pdo(struct usbpd_pm *pdpm)
{
	int fv = 0, ibus_limit = 0, vbus_limit = 0, request_voltage = 0, request_current = 0, final_step = 0, ret = 0, fv_vote = 0;
	int apdo_max_ibus_temp = pdpm->apdo_max_ibus;
	int fcc_ibatt_diff = 0;
	int ibus_limit_thr = 0;

	fv_vote = get_effective_result(pdpm->fv_votable);
	if (pdpm->sw_cv >= 4000)
	{
		fv = pdpm->sw_cv;
	}
	else
	{
		fv = pdpm->ffc_enable ? pdpm->dts_config.fv_ffc : pdpm->dts_config.fv;
		fv = fv < fv_vote ? fv : (pdpm->cycle_count > 100 ? fv_vote - 5 : fv_vote);
	}

	if (pdpm->request_voltage > pdpm->master_cp_vbus + pdpm->total_ibus * MAX_CABLE_RESISTANCE / 1000)
	{
		if(pdpm->master_cp_vbus_last - pdpm->master_cp_vbus <= 500)
			pdpm->request_voltage = pdpm->master_cp_vbus + pdpm->total_ibus * MAX_CABLE_RESISTANCE / 1000;
	}
	pdpm->master_cp_vbus_last = pdpm->master_cp_vbus;

	if(pdpm->switch1_1_enable || pdpm->switch1_1_single_enable)
		apdo_max_ibus_temp = pdpm->apdo_max_ibus*80/100;
	pdpm->ibat_step = pdpm->vbat_step = pdpm->ibus_step = pdpm->vbus_step = 0;
	ibus_limit = min(min((pdpm->target_fcc / pdpm->div_rate + pdpm->ibus_gap), apdo_max_ibus_temp), pdpm->dts_config.max_ibus);
	vbus_limit = min(pdpm->dts_config.max_vbus, pdpm->apdo_max_vbus);
	if(ibus_limit == pdpm->apdo_max_ibus)
	{
		ibus_limit_thr = -50;
	}else
	{
		ibus_limit_thr = MEDIUM_IBUS_DIFF;
	}

	if ((-pdpm->ibat) < (pdpm->target_fcc - pdpm->dts_config.fcc_low_hyst)) {
		if (((pdpm->target_fcc - pdpm->dts_config.fcc_low_hyst) - (-pdpm->ibat)) > LARGE_IBAT_DIFF_POS)
			pdpm->ibat_step = LARGE_STEP;
		else if (((pdpm->target_fcc - pdpm->dts_config.fcc_low_hyst) - (-pdpm->ibat)) > MEDIUM_IBAT_DIFF_POS)
			pdpm->ibat_step = MEDIUM_STEP;
		else
			pdpm->ibat_step = SMALL_STEP;
	} else if ((-pdpm->ibat) > (pdpm->target_fcc - pdpm->dts_config.fcc_high_hyst)) {
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
	else if (fv - pdpm->vbat < -2)
		pdpm->vbat_step = -MEDIUM_STEP;
	else if (fv - pdpm->vbat < 0)
		pdpm->vbat_step = -SMALL_STEP;

	if (ibus_limit - pdpm->total_ibus > LARGE_IBUS_DIFF)
		pdpm->ibus_step = LARGE_STEP;
	else if (ibus_limit - pdpm->total_ibus > MEDIUM_IBUS_DIFF)
		pdpm->ibus_step = MEDIUM_STEP;
	else if (ibus_limit - pdpm->total_ibus > -ibus_limit_thr)
		pdpm->ibus_step = SMALL_STEP;
	else if (ibus_limit - pdpm->total_ibus <= -ibus_limit_thr)
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
	if (pdpm->step_chg_fcc != pdpm->dts_config.max_fcc || pdpm->sw_cv) {
		if ((pdpm->final_step == SMALL_STEP && final_step == SMALL_STEP) || (pdpm->final_step == -SMALL_STEP && final_step == -SMALL_STEP))
			final_step = 0;
	}
	pdpm->final_step = final_step;
	fcc_ibatt_diff = ((-pdpm->ibat) > pdpm->target_fcc) ? ((-pdpm->ibat) - pdpm->target_fcc) : (pdpm->target_fcc - (-pdpm->ibat));
	if(pdpm->ibat < 0 && pdpm->total_ibus > 0 && pd_tune_count < 8 && apdo_max_ibus_temp >= (pdpm->target_fcc / pdpm->div_rate)){
		if (fcc_ibatt_diff > 4000){
			pdpm->final_step = pdpm->final_step * QUICK_RAISE_VOLT_STEP_DELTA;
			pd_tune_count++;
		}
	}

	if (fcc_ibatt_diff < 3000 || abs(pdpm->final_step) < LARGE_STEP){
		pdpm->pdm_slow_tuning = true;
	}

	if (pdpm->final_step) {
		request_voltage = min(pdpm->request_voltage + pdpm->final_step * STEP_MV, vbus_limit);
		request_current = ibus_limit;
		ret = adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, request_voltage, request_current);
		if (!ret) {
			msleep(PDM_SM_DELAY_200MS);
			pdpm->request_voltage = request_voltage;
			pdpm->request_current = request_current;
		}
	}

	return ret;
}

static int pdm_check_condition(struct usbpd_pm *pdpm)
{
	int min_thermal_limit_fcc = 0;
	if(pdpm->switch1_1_enable || pdpm->switch1_1_single_enable)
		min_thermal_limit_fcc = MIN_THERMAL_LIMIT_FCC_BYPASS;
	else if(pdpm->switch2_1_enable)
		min_thermal_limit_fcc = MIN_THERMAL_LIMIT_FCC_21;
	else
		min_thermal_limit_fcc = MIN_THERMAL_LIMIT_FCC_41;
  	if (pdpm->pd_soft_reset) {
		return PDM_SM_HOLD;
	} else if (pdpm->state == PD_PM_STATE_TUNE && pdm_taper_charge(pdpm)) {
		return PDM_SM_EXIT;
	} else if (pdpm->state == PD_PM_STATE_TUNE && (!pdpm->master_cp_enable || (!pdpm->switch1_1_single_enable && !pdpm->slave_cp_enable))) {
		charger_dev_cp_dump_register(pdpm->master_dev);
		charger_dev_cp_dump_register(pdpm->slave_dev);
		return PDM_SM_HOLD;
	} else if (pdpm->hold_pdm_use_pmic)
	{
		return PDM_SM_HOLD;
	} else if (pdpm->switch_mode)
	{
		return PDM_SM_HOLD;
	} else if (pdpm->input_suspend || pdpm->typec_burn) {
		return PDM_SM_HOLD;
	} else if (!is_between(MIN_JEITA_CHG_INDEX, MAX_JEITA_CHG_INDEX, pdpm->jeita_chg_index)) {
		return PDM_SM_HOLD;
	} else if (pdpm->target_fcc < min_thermal_limit_fcc) {
		return PDM_SM_HOLD;
	} else if (pdpm->state == PD_PM_STATE_ENTRY && pdpm->soc > pdpm->dts_config.high_soc) {
		return PDM_SM_EXIT;
	}else if(pinfo->night_charge_enable && pdpm->soc >= 80)
	{
		return PDM_SM_HOLD;
	}else if(pdpm->smart_chg[SMART_CHG_NAVIGATION].active_status && pdpm->soc >= pdpm->smart_chg[SMART_CHG_NAVIGATION].func_val)
	{
		return PDM_SM_HOLD;
	}else if(pdpm->bms_i2c_error_count >= 10)
	{
		return PDM_SM_EXIT;
	}else if (pdpm->bms_slave_connect_error == 0) {
		return PDM_SM_EXIT;
    }else
		return PDM_SM_CONTINUE;
}

static void pdm_move_sm(struct usbpd_pm *pdpm, enum pdm_sm_state state)
{
	pdpm->state = state;
	pdpm->no_delay = true;
}

static bool pdm_handle_sm(struct usbpd_pm *pdpm)
{
	int ret = 0;
	int apdo_max_ibus_temp = pdpm->apdo_max_ibus;
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
			return true;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			if((pinfo->product_name == COROT && pdpm->switch_mode) ||
				(pinfo->product_name == ZIRCON && pdpm->switch_mode && !pdpm->hold_pdm_use_pmic))
			{
				pdm_evaluate_src_caps(pdpm);
			}
			break;
		} else {
				charger_dev_enable(pdpm->charger_dev, false);
				charger_dev_enable_powerpath(pdpm->charger_dev, false);
				msleep(200);
				charger_dev_is_powerpath_enabled(pdpm->charger_dev, &power_path_en);
				if (power_path_en) {
					charger_dev_enable(pdpm->charger_dev, false);
					charger_dev_enable_powerpath(pdpm->charger_dev, false);
					msleep(200);
				}
			if (pdpm->target_fcc > switch2_1_enter_fcc_set && pdpm->cp_4_1_mode && !pdpm->switch2_1_enable) {
				pdpm->cp_work_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
				charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_4_TO_1);
				charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_4_TO_1);
				charger_dev_cp_enable_adc(pdpm->master_dev, true);
				charger_dev_cp_enable_adc(pdpm->slave_dev, true);
				pdpm->div_rate = 4;
			}else if(pdpm->switch2_1_enable)
			{
				 pdpm->cp_work_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
				charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_2_TO_1);
				charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_2_TO_1);
				charger_dev_cp_enable_adc(pdpm->master_dev, true);
				charger_dev_cp_enable_adc(pdpm->slave_dev, true);
				pdpm->div_rate = 2;
			}else if(pdpm->switch1_1_enable)
			{
				pdpm->cp_work_mode = SC8561_FORWARD_1_1_CHARGER_MODE;
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_1_1_CHARGER_MODE);
				charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_1_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_1_TO_1);
				charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_1_TO_1);
				charger_dev_cp_enable_adc(pdpm->master_dev, true);
				charger_dev_cp_enable_adc(pdpm->slave_dev, true);
				pdpm->div_rate = 1;
			}else if(pdpm->switch1_1_single_enable)
			{
				pdpm->cp_work_mode = SC8561_FORWARD_1_1_CHARGER_MODE;
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_1_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_1_TO_1);
				charger_dev_cp_enable_adc(pdpm->master_dev, true);
				charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_1_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_1_TO_1);
				charger_dev_cp_enable_adc(pdpm->slave_dev, true);
				pdpm->div_rate = 1;
			}else{
				if(!pdpm->cp_4_1_mode)
					pdpm->switch2_1_enable = true;
			    pdpm->cp_work_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
				charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_2_TO_1);
				charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_2_TO_1);
				charger_dev_cp_enable_adc(pdpm->master_dev, true);
				charger_dev_cp_enable_adc(pdpm->slave_dev, true);
				pdpm->div_rate = 2;
			}
			pdm_move_sm(pdpm, PD_PM_STATE_INIT_VBUS);
		}
		break;
	case PD_PM_STATE_INIT_VBUS:
		pdpm->sm_status = pdm_check_condition(pdpm);
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}
		pdpm->tune_vbus_count++;
		if (pdpm->tune_vbus_count == 1) {
			if (pdpm->mt6375_vbus > 3600000 && gpio_is_valid(pdpm->vbus_control_gpio)) {
				gpio_direction_output(pdpm->vbus_control_gpio, 1);
				gpio_set_value(pdpm->vbus_control_gpio, 1);
			}
			pdpm->request_voltage = pdpm->entry_vbus;
			pdpm->request_current = pdpm->entry_ibus;
			if (pdpm->request_voltage < 3600)
				pdpm->request_voltage = 3600;
			if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
				adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO_START, pdpm->request_voltage, pdpm->request_current);
			break;
		}

		if (pdpm->tune_vbus_count >= MAX_VBUS_TUNE_COUNT) {
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		} else if (pdpm->adapter_adjust_count >= MAX_ADAPTER_ADJUST_COUNT) {
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}

		if (pdpm->master_cp_vbus <= pdpm->vbat * pdpm->div_rate + pdpm->vbus_low_gap) {
			pdpm->request_voltage += pdpm->div_rate * STEP_MV;
		} else if (pdpm->master_cp_vbus >= pdpm->vbat * pdpm->div_rate + pdpm->vbus_high_gap) {
			pdpm->request_voltage -= pdpm->div_rate * STEP_MV;
		} else {
			pdm_move_sm(pdpm, PD_PM_STATE_ENABLE_CP);
			break;
		}

		if(pdpm->switch1_1_enable || pdpm->switch1_1_single_enable)
			apdo_max_ibus_temp = pdpm->apdo_max_ibus*80/100;
		pdpm->request_current = min(min(pdpm->target_fcc / pdpm->div_rate, apdo_max_ibus_temp), pdpm->dts_config.max_ibus);
		if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
			ret = adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, pdpm->request_voltage, pdpm->request_current);
		if (ret == MTK_ADAPTER_REJECT) {
			pdpm->adapter_adjust_count++;
			break;
		}
		break;
	case PD_PM_STATE_ENABLE_CP:
		pdpm->sm_status = pdm_check_condition(pdpm);
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}
		pdpm->enable_cp_count++;
		if (pdpm->enable_cp_count >= MAX_ENABLE_CP_COUNT) {
			charger_dev_cp_dump_register(pdpm->master_dev);
			charger_dev_cp_dump_register(pdpm->slave_dev);
			pdpm->sm_status = PDM_SM_EXIT;
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}
		if (!pdpm->master_cp_enable && pdpm->pdm_active)
			charger_dev_enable(pdpm->master_dev, true);

		if (!pdpm->slave_cp_enable && !pdpm->switch1_1_single_enable && pdpm->pdm_active)
			charger_dev_enable(pdpm->slave_dev, true);

		if (pdpm->master_cp_mode != pdpm->cp_work_mode)
			charger_dev_cp_set_mode(pdpm->master_dev, pdpm->cp_work_mode);

		if (pdpm->slave_cp_mode != pdpm->cp_work_mode && !pdpm->switch1_1_single_enable)
			charger_dev_cp_set_mode(pdpm->slave_dev, pdpm->cp_work_mode);

		if (pdpm->master_cp_enable && (pdpm->slave_cp_enable^pdpm->switch1_1_single_enable) && pdpm->master_cp_mode == pdpm->cp_work_mode &&
									(pdpm->slave_cp_mode == pdpm->cp_work_mode || pdpm->switch1_1_single_enable)) {
			charger_dev_enable_termination(pdpm->charger_dev, false);
			pdm_move_sm(pdpm, PD_PM_STATE_TUNE);
		} else {
			break;
		}
		break;
	case PD_PM_STATE_TUNE:
		pdpm->sm_status = pdm_check_condition(pdpm);
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
		} else {
			ret = pdm_tune_pdo(pdpm);
			if (ret == MTK_ADAPTER_ERROR) {
				pdpm->sm_status = PDM_SM_HOLD;
				pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			}
		}
		break;
	case PD_PM_STATE_EXIT:
		pdpm->tune_vbus_count = 0;
		pdpm->adapter_adjust_count = 0;
		pdpm->enable_cp_count = 0;
		pdpm->taper_count = 0;
		pd_tune_count = 0;
		charger_dev_enable(pdpm->master_dev, false);
		charger_dev_enable(pdpm->slave_dev, false);
		if(pdpm->support_4_1)
		{
			charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
			charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
			charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_4_TO_1);
			charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_4_TO_1);
		}else{
			charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
			charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
			charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_2_TO_1);
			charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_2_TO_1);
		}
		if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
			adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, max(DEFAULT_PDO_VBUS_1S, pdpm->apdo_min_vbus), min(DEFAULT_PDO_IBUS_1S, pdpm->apdo_max_ibus));
		msleep(500);
		charger_dev_get_vbus(pdpm->charger_dev, &pdpm->mt6375_vbus);
		if (pdpm->mt6375_vbus < 10000000 && gpio_is_valid(pdpm->vbus_control_gpio))
			gpio_set_value(pdpm->vbus_control_gpio, 0);

		if (!pdpm->input_suspend){
			charger_dev_enable_powerpath(pdpm->charger_dev, true);
			charger_dev_set_input_current(pdpm->charger_dev, 3000000);
		}
		msleep(1000);
		charger_dev_enable_termination(pdpm->charger_dev, true);
		if (pdpm->charge_full) {
			msleep(1000);
			charger_dev_enable(pdpm->charger_dev, false);
		} else if (pinfo->night_charge_enable || pdpm->smart_chg[SMART_CHG_NAVIGATION].active_status)
			charger_dev_enable(pdpm->charger_dev, false);

		if (pdpm->sm_status == PDM_SM_EXIT) {
			return true;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			if (pdpm->pd_soft_reset) {
				set_soft_reset_status(false);
				msleep(500);
			}
			if(!pdpm->hold_pdm_use_pmic){
				pdm_evaluate_src_caps(pdpm);
			}
			pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
		}

		break;
	default:
		break;
	}

	return false;
}

static void pdm_main_sm(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, main_sm_work.work);
	int internal = PDM_SM_DELAY_300MS;
	pdpm->pdm_slow_tuning = false;

	pdm_update_status(pdpm);
	pdm_multi_mode_switch(pdpm);

	if (!pdm_handle_sm(pdpm) && pdpm->pdm_active) {
		if (pdpm->no_delay) {
			internal = 0;
			pdpm->no_delay = false;
		} else {
			switch (pdpm->state) {
			case PD_PM_STATE_ENTRY:
			case PD_PM_STATE_EXIT:
			case PD_PM_STATE_INIT_VBUS:
				internal = PDM_SM_DELAY_200MS;
				break;
			case PD_PM_STATE_ENABLE_CP:
				internal = PDM_SM_DELAY_200MS;
				break;
			case PD_PM_STATE_TUNE:
				if(pdpm->jeita_chg_index == 2)
					internal = PDM_SM_DELAY_800MS;
				else if (pdpm->pdm_slow_tuning && pinfo->product_name == ZIRCON)
					internal = PDM_SM_DELAY_1000MS;	//FG ibat register is updated every 1000MS
				else
					internal = PDM_SM_DELAY_300MS;
				break;
			default:
				break;
			}
		}
		schedule_delayed_work(&pdpm->main_sm_work, msecs_to_jiffies(internal));
	}
}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
	usb_set_property(USB_PROP_APDO_MAX, 0);
	cancel_delayed_work(&pdpm->main_sm_work);
	charger_dev_enable(pdpm->master_dev, false);
	charger_dev_enable(pdpm->slave_dev, false);
	if(pdpm->support_4_1)
	{
		charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
		charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
		charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_4_TO_1);
		charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_4_TO_1);
	}else{
		charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
		charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
		charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_2_TO_1);
		charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_2_TO_1);
	}
	charger_dev_cp_enable_adc(pdpm->master_dev, false);
	charger_dev_cp_enable_adc(pdpm->slave_dev, false);
	if (!pdpm->cp_4_1_mode && !pdpm->input_suspend) {
		charger_dev_enable_powerpath(pdpm->charger_dev, true);
		charger_dev_set_input_current(pdpm->charger_dev, 3000000);
	}

	set_soft_reset_status(false);

	pdpm->pd_type = MTK_PD_CONNECT_NONE;
	pdpm->pd_verify_done = false;
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
	pdpm->switch_mode = false;
	pdpm->switch2_1_enable = false;
	pdpm->switch1_1_enable = false;
	pdpm->switch2_1_single_enable = false;
	pdpm->switch1_1_single_enable = false;
	pdpm->div_rate = 0;
	pd_tune_count = 0;
	memset(&pdpm->cap, 0, sizeof(struct adapter_power_cap));

	if (gpio_is_valid(pdpm->vbus_control_gpio)) {
		gpio_set_value(pdpm->vbus_control_gpio, 0);
	}
	pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
}

static void pdm_psy_change(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, psy_change_work);
	int ret = 0, val = 0;
	ret = usb_get_property(USB_PROP_PD_TYPE, &val);
	if (ret) {
		goto out;
	} else {
		pdpm->pd_type = val;
	}

	ret = usb_get_property(USB_PROP_PD_VERIFY_DONE, &val);
	if (ret) {
		goto out;
	} else {
		pdpm->pd_verify_done = !!val;
	}

	ret = usb_get_property(USB_PROP_CP_CHARGE_RECOVERY, &val);
	if (ret) {
                goto out;
        } else {
                pdpm->suspend_recovery = !!val;
        }

	if ((!pdpm->pdm_active || pdpm->suspend_recovery) && (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK || pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 || pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) && pdpm->pd_verify_done) {
		if(pdpm->support_4_1)
		{
			charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
			charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
			charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_4_TO_1);
			charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_4_TO_1);
		}else{
			charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
			charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
			charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_2_TO_1);
			charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_2_TO_1);
		}

		if (pdm_evaluate_src_caps(pdpm)) {
			pdpm->pdm_active = true;
#ifdef CONFIG_FACTORY_BUILD
			// pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
			schedule_delayed_work(&pdpm->main_sm_work, 0);
#else
			pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
			schedule_delayed_work(&pdpm->main_sm_work, 500);
#endif
		}
	} else if (pdpm->pdm_active && pdpm->pd_type == MTK_PD_CONNECT_NONE) {
		pdpm->pdm_active = false;
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
	return ret;
}

static ssize_t pdm_store_log_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = sscanf(buf, "%d", &log_level);
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

	rc = of_property_read_u32(node, "vbus_low_gap", &pdpm->vbus_low_gap);
	rc = of_property_read_u32(node, "vbus_high_gap", &pdpm->vbus_high_gap);
	rc = of_property_read_u32(node, "ibus_gap", &pdpm->ibus_gap);
	rc = of_property_read_u32(node, "min_pdo_vbus", &pdpm->dts_config.min_pdo_vbus);
	rc = of_property_read_u32(node, "max_pdo_vbus", &pdpm->dts_config.max_pdo_vbus);

	pdpm->vbus_control_gpio = of_get_named_gpio(node, "mt6375_control_gpio", 0);
	pdpm->support_4_1 = of_property_read_bool(node, "support_4_1");
#ifdef CONFIG_FACTORY_BUILD
	pdpm-> vbus_low_gap += 200;
	pdpm-> vbus_high_gap += 200;
#endif
	return rc;
}

static const struct platform_device_id pdm_id[] = {
	{ "pd_cp_manager", 0},
	{},
};
MODULE_DEVICE_TABLE(platform, pdm_id);

static const struct of_device_id pdm_of_match[] = {
	{ .compatible = "pd_cp_manager", },
	{},
};
MODULE_DEVICE_TABLE(of, pdm_of_match);

static int pdm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct usbpd_pm *pdpm;
	const struct of_device_id *of_id;

	of_id = of_match_device(pdm_of_match, &pdev->dev);
	pdev->id_entry = of_id->data;

	pdpm = kzalloc(sizeof(struct usbpd_pm), GFP_KERNEL);
	if (!pdpm)
		return -ENOMEM;

	pdpm->dev = dev;
	pdpm->last_time.tv_sec = 0;
	pdpm->cycle_count = 0;
	spin_lock_init(&pdpm->psy_change_lock);
	platform_set_drvdata(pdev, pdpm);

	ret = pd_policy_parse_dt(pdpm);
	if (ret < 0) {
		return ret;
	}

	if (gpio_is_valid(pdpm->vbus_control_gpio))
		gpio_direction_output(pdpm->vbus_control_gpio, 0);

	if (!pdm_check_cp_dev(pdpm)) {
		return -ENODEV;
	}

	if (!pdm_check_psy(pdpm)) {
		return -ENODEV;
	}
	pdpm->batt_drvdata = power_supply_get_drvdata(pdpm->batt_psy);

	if (!pdm_check_votable(pdpm)) {
		return -ENODEV;
	}

	INIT_WORK(&pdpm->psy_change_work, pdm_psy_change);
	INIT_DELAYED_WORK(&pdpm->main_sm_work, pdm_main_sm);

	pdpm->nb.notifier_call = usbpdm_psy_notifier_cb;
	power_supply_reg_notifier(&pdpm->nb);
	pdpm->cp_work_mode = SC8561_FORWARD_2_1_CHARGER_MODE;

	ret = sysfs_create_group(&pdpm->dev->kobj, &pdm_attr_group);
	if (ret) {
		return ret;
	}
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
MODULE_AUTHOR("liujiquan");
MODULE_DESCRIPTION("charge pump manager for PD");
MODULE_LICENSE("GPL");
