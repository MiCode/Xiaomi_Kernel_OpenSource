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
#include <linux/pmic_voter.h>
#include <mt-plat/mtk_charger.h>
#include <mtk_intf.h>
#include <adapter_class.h>
#include "pd_cp_manager.h"

enum product_name {
	PISSARRO,
	PISSARROPRO,
};

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
	int	cv_vbat;
	int	cv_vbat_ffc;
	int	cv_ibat;
	int	min_pdo_vbus;
	int	max_pdo_vbus;
	int	max_bbc_vbus;
	int	min_bbc_vbus;
};

struct usbpd_pm {
	struct device *dev;
	struct charger_device *master_dev;
	struct charger_device *slave_dev;
	struct charger_device *bbc_dev;

	struct power_supply *master_psy;
	struct power_supply *slave_psy;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;

	struct votable *bbc_en_votable;
	struct votable *bbc_icl_votable;
	struct votable *bbc_fcc_votable;
	struct votable *bbc_fv_votable;
	struct votable *bbc_vinmin_votable;

	struct delayed_work main_sm_work;
	struct work_struct psy_change_work;
	struct notifier_block nb;

	spinlock_t psy_change_lock;

	struct pdm_dts_config dts_config;
	struct timespec last_time;

	enum pdm_sm_state state;
	enum pdm_sm_state last_state;
	enum pdm_sm_status sm_status;
	enum adapter_event pd_type;
	enum power_supply_type psy_type;
	bool	pdm_active;
	bool	psy_change_running;
	bool	master_cp_enable;
	bool	master_cp_bypass;
	bool	slave_cp_enable;
	bool	slave_cp_bypass;
	bool	bypass_enable;
	bool	cp_bypass_support;
	bool	pdo_bypass_support;
	bool	switch_mode;
	bool	disable_slave;
	bool	ffc_enable;
	bool	input_suspend;
	bool	typec_burn;
	bool	no_delay;
	bool	pd_verify_done;
	int	master_cp_ibus;
	int	slave_cp_ibus;
	int	total_ibus;
	int	jeita_chg_index;
	int	soc;
	int	ibat;
	int	vbat;
	int	target_fcc;
	int	thermal_limit_fcc;
	int	sic_limit_fcc;
	int	step_chg_fcc;
	int	bbc_vbus;
	int	bbc_ibus;
	int	sw_cv;
	int	vbus_step;
	int	ibus_step;
	int	ibat_step;
	int	vbat_step;
	int	final_step;
	int	request_voltage;
	int	request_current;
	int	retry_count;
	int	entry_vbus;
	int	entry_ibus;
	int	vbus_low_gap;
	int	vbus_high_gap;
	int	ibus_gap;
	int	tune_vbus_count;
	int	adapter_adjust_count;
	int	enable_cp_count;
	int	enable_cp_fail_count;
	int	taper_count;
	int	cv_wa_count;
	int	low_ibus_count;
	int	bms_i2c_error_count;

	int	apdo_max_vbus;
	int	apdo_min_vbus;
	int	apdo_max_ibus;
	int	apdo_max_watt;
	int	bypass_entry_fcc;
	int	bypass_exit_fcc;
	struct	pd_cap cap;
};

static const unsigned char *pm_str[] = {
	"PD_PM_STATE_ENTRY",
	"PD_PM_STATE_INIT_VBUS",
	"PD_PM_STATE_ENABLE_CP",
	"PD_PM_STATE_TUNE",
	"PD_PM_STATE_EXIT",
};

static int log_level = 1;
static int product_name = PISSARRO;

static int bypass_entry_fcc_pissarro = 4300;
module_param_named(bypass_entry_fcc_pissarro, bypass_entry_fcc_pissarro, int, 0600);

static int bypass_exit_fcc_pissarro = 6000;
module_param_named(bypass_exit_fcc_pissarro, bypass_exit_fcc_pissarro, int, 0600);

static int bypass_entry_fcc_pissarropro = 4000;
module_param_named(bypass_entry_fcc_pissarropro, bypass_entry_fcc_pissarropro, int, 0600);

static int bypass_exit_fcc_pissarropro = 6000;
module_param_named(bypass_exit_fcc_pissarropro, bypass_exit_fcc_pissarropro, int, 0600);

static int vbus_low_gap_div = 800;
module_param_named(vbus_low_gap_div, vbus_low_gap_div, int, 0600);

static int vbus_high_gap_div = 950;
module_param_named(vbus_high_gap_div, vbus_high_gap_div, int, 0600);

static int ibus_gap_div = 350;
module_param_named(ibus_gap_div, ibus_gap_div, int, 0600);

static int vbus_low_gap_bypass = 160;
module_param_named(vbus_low_gap_bypass, vbus_low_gap_bypass, int, 0600);

static int vbus_high_gap_bypass = 220;
module_param_named(vbus_high_gap_bypass, vbus_high_gap_bypass, int, 0600);

static int ibus_gap_bypass = 500;
module_param_named(ibus_gap_bypass, ibus_gap_bypass, int, 0600);

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

#define cut_cap(value, min, max)	((min > value) ? min : ((value > max) ? max : value))

#define is_between(left, right, value)				\
		(((left) >= (right) && (left) >= (value)	\
			&& (value) >= (right))			\
		|| ((left) <= (right) && (left) <= (value)	\
			&& (value) <= (right)))

static bool pdm_check_cp_dev(struct usbpd_pm *pdpm)
{
	if (!pdpm->master_dev)
		pdpm->master_dev = get_charger_by_name("cp_master");

	if (!pdpm->master_dev) {
		pdm_err("failed to get master_dev\n");
		return false;
	}

	if (!pdpm->slave_dev)
		pdpm->slave_dev = get_charger_by_name("cp_slave");

	if (!pdpm->slave_dev) {
		pdm_err("failed to get slave_dev\n");
		return false;
	}

	if (!pdpm->bbc_dev)
		pdpm->bbc_dev = get_charger_by_name("bbc");

	if (!pdpm->bbc_dev) {
		pdm_err("failed to get bbc_dev\n");
		return false;
	}

	return true;
}

static bool pdm_check_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->master_psy)
		pdpm->master_psy = power_supply_get_by_name("cp_master");

	if (!pdpm->master_psy) {
		pdm_err("failed to get master_psy\n");
		return false;
	}

	if (!pdpm->slave_psy)
		pdpm->slave_psy = power_supply_get_by_name("cp_slave");

	if (!pdpm->slave_psy) {
		pdm_err("failed to get slave_psy\n");
		return false;
	}

	if (!pdpm->usb_psy)
		pdpm->usb_psy = power_supply_get_by_name("usb");

	if (!pdpm->usb_psy) {
		pdm_err("failed to get usb_psy\n");
		return false;
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
	if (!pdpm->bbc_en_votable)
		pdpm->bbc_en_votable = find_votable("BBC_ENABLE");

	if (!pdpm->bbc_en_votable) {
		pdm_err("failed to get bbc_en_votable\n");
		return false;
	}

	if (!pdpm->bbc_icl_votable)
		pdpm->bbc_icl_votable = find_votable("BBC_ICL");

	if (!pdpm->bbc_icl_votable) {
		pdm_err("failed to get bbc_icl_votable\n");
		return false;
	}

	if (!pdpm->bbc_fcc_votable)
		pdpm->bbc_fcc_votable = find_votable("BBC_FCC");

	if (!pdpm->bbc_fcc_votable) {
		pdm_err("failed to get bbc_fcc_votable\n");
		return false;
	}

	if (!pdpm->bbc_fv_votable)
		pdpm->bbc_fv_votable = find_votable("BBC_FV");

	if (!pdpm->bbc_fv_votable) {
		pdm_err("failed to get bbc_fv_votable\n");
		return false;
	}

	if (!pdpm->bbc_vinmin_votable)
		pdpm->bbc_vinmin_votable = find_votable("BBC_VINMIN");

	if (!pdpm->bbc_vinmin_votable) {
		pdm_err("failed to get bbc_vinmin_votable\n");
		return false;
	}

	return true;
}

static void pdm_cv_wa_func(struct usbpd_pm *pdpm)
{
	struct pd_cap cap;
	int vbus = 0, i = 0, ret = 0;

	if (product_name != PISSARROPRO || (pdpm->state != PD_PM_STATE_ENTRY && pdpm->state != PD_PM_STATE_EXIT))
		return;

	charger_dev_get_vbus(pdpm->bbc_dev, &vbus);
	if (vbus <= CV_VBUS_MP2762_WA)
		return;

	vote(pdpm->bbc_fcc_votable, CV_WA_VOTER, true, 500);
	vote(pdpm->bbc_icl_votable, CV_WA_VOTER, true, 500);
	charger_dev_enable_powerpath(pdpm->bbc_dev, false);
	msleep(400);
	adapter_get_cap(&cap);
	for (i = 0; i < pdpm->cap.nr; i++) {
		if (cap.max_mv[i] >= 5000 && cap.max_mv[i] <= 6000) {
			ret = adapter_set_cap(cap.max_mv[i], cap.ma[i]);
			if (ret != ADAPTER_OK) {
				msleep(100);
				adapter_set_cap(cap.max_mv[i], cap.ma[i]);
			}
			break;
		}
	}

	vote(pdpm->bbc_fcc_votable, CV_WA_VOTER, false, 0);
	vote(pdpm->bbc_icl_votable, CV_WA_VOTER, false, 0);
	charger_dev_enable_powerpath(pdpm->bbc_dev, true);
	return;
}

static bool pdm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
	union power_supply_propval val = {0,};
	bool legal_pdo = false, low_power_pd2 = true;
	int ret = 0, i = 0;

	pdpm->apdo_max_vbus = 0;
	pdpm->apdo_min_vbus = 0;
	pdpm->apdo_max_ibus = 0;
	pdpm->apdo_max_watt = 0;

	adapter_get_cap(&pdpm->cap);

	for (i = 0; i < pdpm->cap.nr; i++) {
		if (pdpm->cap.type[i] != MTK_PD_APDO || pdpm->cap.max_mv[i] < pdpm->dts_config.min_pdo_vbus || pdpm->cap.max_mv[i] > pdpm->dts_config.max_pdo_vbus)
			continue;

		if (pdpm->cap.maxwatt[i] > pdpm->apdo_max_watt) {
			pdpm->apdo_max_vbus = pdpm->cap.max_mv[i];
			pdpm->apdo_min_vbus = pdpm->cap.min_mv[i];
			pdpm->apdo_max_ibus = pdpm->cap.ma[i];
			pdpm->apdo_max_watt = pdpm->cap.maxwatt[i];
		}

		legal_pdo = true;
	}

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);

	if (legal_pdo) {
		if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
			//if (product_name == PISSARRO && pdpm->apdo_max_ibus == SECOND_IBUS_67W)
				//pdpm->apdo_max_ibus = MAX_IBUS_67W;

			if ((product_name == PISSARRO && pdpm->apdo_min_vbus <= BYPASS_MIN_VBUS_1S) ||
				(product_name == PISSARROPRO && pdpm->apdo_min_vbus <= BYPASS_MIN_VBUS_2S))
				pdpm->pdo_bypass_support = true;
			else
				pdpm->pdo_bypass_support = false;

			pdm_info("MAX_PDO = [%d %d %d]\n", pdpm->apdo_max_vbus, pdpm->apdo_max_ibus, pdpm->apdo_max_watt / 1000000);
		} else {
			legal_pdo = false;
		}
	} else {
		for (i = 0; i <= pdpm->cap.nr - 1; i++) {
			if (pdpm->cap.type[i] == MTK_PD_APDO || !is_between(pdpm->dts_config.min_bbc_vbus, pdpm->dts_config.max_bbc_vbus, pdpm->cap.max_mv[i]))
				continue;

			if (pdpm->cv_wa_count < CV_MP2762_WA_COUNT && val.intval <= 98) {
				vote(pdpm->bbc_icl_votable, PDM_VOTER, true, pdpm->cap.ma[i] - 100);
				msleep(100);
				ret = adapter_set_cap(pdpm->cap.max_mv[i], pdpm->cap.ma[i]);
				if (ret == ADAPTER_OK) {
					low_power_pd2 = false;
					break;
				}
			}
		}

		if (low_power_pd2) {
			vote(pdpm->bbc_icl_votable, PDM_VOTER, true, min(2000, pdpm->cap.ma[0] - 100));
			vote(pdpm->bbc_vinmin_votable, PDM_VOTER, true, LOW_POWER_PD2_VINMIN);
		}
	}

	return legal_pdo;
}

static bool pdm_taper_charge(struct usbpd_pm *pdpm)
{
	int cv_vbat = 0;

	cv_vbat = pdpm->ffc_enable ? pdpm->dts_config.cv_vbat_ffc : pdpm->dts_config.cv_vbat;
	if (pdpm->vbat > cv_vbat && (-pdpm->ibat) < pdpm->dts_config.cv_ibat)
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

	charger_dev_get_vbus(pdpm->bbc_dev, &pdpm->bbc_vbus);
	charger_dev_get_ibus(pdpm->bbc_dev, &pdpm->bbc_ibus);
	charger_dev_get_ibus(pdpm->master_dev, &pdpm->master_cp_ibus);
	charger_dev_get_ibus(pdpm->slave_dev, &pdpm->slave_cp_ibus);
	charger_dev_is_enabled(pdpm->master_dev, &pdpm->master_cp_enable);
	charger_dev_is_enabled(pdpm->slave_dev, &pdpm->slave_cp_enable);
	charger_dev_is_bypass_enabled(pdpm->master_dev, &pdpm->master_cp_bypass);
	charger_dev_is_bypass_enabled(pdpm->slave_dev, &pdpm->slave_cp_bypass);

	pdpm->total_ibus = pdpm->bbc_ibus + pdpm->master_cp_ibus + pdpm->slave_cp_ibus;

	power_supply_get_property(pdpm->master_psy, POWER_SUPPLY_PROP_BYPASS_SUPPORT, &val);
	pdpm->cp_bypass_support = val.intval;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	pdpm->ibat = val.intval / 1000;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	pdpm->vbat = val.intval / 1000;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	pdpm->soc = val.intval;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_I2C_ERROR_COUNT, &val);
	pdpm->bms_i2c_error_count = val.intval;

	power_supply_get_property(pdpm->usb_psy, POWER_SUPPLY_PROP_JEITA_CHG_INDEX, &val);
	pdpm->jeita_chg_index = val.intval;

	power_supply_get_property(pdpm->usb_psy, POWER_SUPPLY_PROP_FFC_ENABLE, &val);
	pdpm->ffc_enable = val.intval;

	power_supply_get_property(pdpm->usb_psy, POWER_SUPPLY_PROP_SW_CV, &val);
	pdpm->sw_cv = val.intval;

	power_supply_get_property(pdpm->usb_psy, POWER_SUPPLY_PROP_INPUT_SUSPEND, &val);
	pdpm->input_suspend = val.intval;

	power_supply_get_property(pdpm->usb_psy, POWER_SUPPLY_PROP_TYPEC_BURN, &val);
	pdpm->typec_burn = val.intval;

	power_supply_get_property(pdpm->usb_psy, POWER_SUPPLY_PROP_CV_WA_COUNT, &val);
	pdpm->cv_wa_count = val.intval;

	pdpm->step_chg_fcc = get_client_vote(pdpm->bbc_fcc_votable, STEP_CHARGE_VOTER);
	pdpm->target_fcc = get_effective_result(pdpm->bbc_fcc_votable);
	pdpm->thermal_limit_fcc = get_client_vote(pdpm->bbc_fcc_votable, THERMAL_VOTER);
	pdpm->sic_limit_fcc = get_client_vote(pdpm->bbc_fcc_votable, SIC_VOTER);
	if (pdpm->sic_limit_fcc > 0)
		pdpm->thermal_limit_fcc = min(pdpm->thermal_limit_fcc, pdpm->sic_limit_fcc);

	pdm_info("BUS = [%d %d %d %d %d], CP = [%d %d %d %d %d %d %d %d], BAT = [%d %d %d %d %d], STEP = [%d %d %d %d %d], CC_CV = [%d %d %d], CMD = [%d %d %d]\n",
		pdpm->bbc_vbus, pdpm->bbc_ibus, pdpm->master_cp_ibus, pdpm->slave_cp_ibus, pdpm->total_ibus,
		pdpm->master_cp_enable, pdpm->slave_cp_enable, pdpm->master_cp_bypass, pdpm->slave_cp_bypass,
		pdpm->disable_slave, pdpm->bypass_enable, pdpm->cp_bypass_support, pdpm->pdo_bypass_support,
		pdpm->soc, pdpm->vbat, pdpm->ibat, pdpm->jeita_chg_index, pdpm->bms_i2c_error_count,
		pdpm->vbat_step, pdpm->ibat_step, pdpm->vbus_step, pdpm->ibus_step, pdpm->final_step,
		pdpm->target_fcc, pdpm->thermal_limit_fcc, pdpm->sw_cv, pdpm->input_suspend, pdpm->typec_burn, pdpm->cv_wa_count);
}

static void pdm_bypass_check(struct usbpd_pm *pdpm)
{
	struct timespec time;

	if(product_name == PISSARRO) {
		pdpm->bypass_entry_fcc = bypass_entry_fcc_pissarro;
		pdpm->bypass_exit_fcc  = bypass_exit_fcc_pissarro;
	}else if(product_name == PISSARROPRO) {
		pdpm->bypass_entry_fcc = bypass_entry_fcc_pissarropro;
		pdpm->bypass_exit_fcc  = bypass_exit_fcc_pissarropro;
	}

	if (!pdpm->cp_bypass_support || !pdpm->pdo_bypass_support || (product_name == PISSARRO && pdpm->apdo_max_watt <= MAX_WATT_33W)) {
		pdpm->bypass_enable = false;
		pdpm->switch_mode = false;
	} else {
		if (((product_name == PISSARRO && pdpm->soc < 92) || (product_name == PISSARROPRO && pdpm->soc < 94)) &&
			(pdpm->state == PD_PM_STATE_TUNE || pdpm->state == PD_PM_STATE_ENTRY)) {
			if (pdpm->bypass_enable) {
				if (pdpm->thermal_limit_fcc >= pdpm->bypass_exit_fcc)
					pdpm->switch_mode = true;
				else
					pdpm->switch_mode = false;
			} else {
				if (pdpm->thermal_limit_fcc <= pdpm->bypass_entry_fcc)
					pdpm->switch_mode = true;
				else
					pdpm->switch_mode = false;
			}
		} else {
			pdpm->switch_mode = false;
		}
	}

	if (pdpm->switch_mode) {
		get_monotonic_boottime(&time);
		if (pdpm->last_time.tv_sec == 0 || time.tv_sec - pdpm->last_time.tv_sec >= 15) {
			pdpm->bypass_enable = !pdpm->bypass_enable;
			pdpm->last_time.tv_sec = time.tv_sec;
		} else {
			pdpm->switch_mode = false;
		}
	}

	pdpm->vbus_low_gap = pdpm->bypass_enable ? vbus_low_gap_bypass : vbus_low_gap_div;
	pdpm->vbus_high_gap = pdpm->bypass_enable ? vbus_high_gap_bypass : vbus_high_gap_div;
	pdpm->ibus_gap = pdpm->bypass_enable ? ibus_gap_bypass : ibus_gap_div;

	pdpm->entry_vbus = min(min(((pdpm->vbat * (pdpm->bypass_enable ? 1 : 2)) + pdpm->vbus_low_gap), pdpm->dts_config.max_vbus), pdpm->apdo_max_vbus);
	pdpm->entry_ibus = min(min(((pdpm->target_fcc / (pdpm->bypass_enable ? 1 : 2)) + pdpm->ibus_gap), pdpm->dts_config.max_ibus), pdpm->apdo_max_ibus);
	if (!pdpm->bypass_enable && pdpm->soc > 85)
		pdpm->entry_vbus = min(min(pdpm->entry_vbus - 360, pdpm->dts_config.max_vbus), pdpm->apdo_max_vbus);
}

static void pdm_disable_slave_check(struct usbpd_pm *pdpm)
{
	if (pdpm->bypass_enable) {
		pdpm->low_ibus_count = 0;
		pdpm->disable_slave = false;
	} else if (pdpm->apdo_max_watt <= MAX_WATT_33W && product_name == PISSARROPRO) {
		pdpm->low_ibus_count = 0;
		pdpm->disable_slave = true;
	} else if (pdpm->thermal_limit_fcc < LOW_THERMAL_LIMIT_FCC) {
		pdpm->low_ibus_count = 0;
		pdpm->disable_slave = true;
	} else if (pdpm->state == PD_PM_STATE_TUNE) {
		if (pdpm->slave_cp_ibus <= LOW_CP_IBUS) {
			if (pdpm->low_ibus_count < MAX_LOW_CP_IBUS_COUNT)
				pdpm->low_ibus_count++;
			if (pdpm->low_ibus_count >= MAX_LOW_CP_IBUS_COUNT)
				pdpm->disable_slave = true;
			else
				pdpm->disable_slave = false;
		} else {
			pdpm->low_ibus_count = 0;
			pdpm->disable_slave = false;
		}
	}
}

static int pdm_tune_pdo(struct usbpd_pm *pdpm)
{
	int fv = 0, ibus_limit = 0, vbus_limit = 0, request_voltage = 0, request_current = 0, final_step = 0, ret = 0;

	if (pdpm->sw_cv)
		fv = pdpm->sw_cv;
	else
		fv = pdpm->ffc_enable ? pdpm->dts_config.fv_ffc : pdpm->dts_config.fv;

	if (pdpm->request_voltage > pdpm->bbc_vbus + pdpm->total_ibus * MAX_CABLE_RESISTANCE / 1000)
		pdpm->request_voltage = pdpm->bbc_vbus + pdpm->total_ibus * MAX_CABLE_RESISTANCE / 1000;

	pdpm->ibat_step = pdpm->vbat_step = pdpm->ibus_step = pdpm->vbus_step = 0;
	ibus_limit = min(min(((pdpm->target_fcc / (pdpm->bypass_enable ? 1 : 2)) + pdpm->ibus_gap), pdpm->dts_config.max_ibus), pdpm->apdo_max_ibus);
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
	else if (fv - pdpm->vbat < -2)
		pdpm->vbat_step = -MEDIUM_STEP;
	else if (fv - pdpm->vbat < 0)
		pdpm->vbat_step = -SMALL_STEP;

	if (ibus_limit - pdpm->total_ibus > LARGE_IBUS_DIFF)
		pdpm->ibus_step = LARGE_STEP;
	else if (ibus_limit - pdpm->total_ibus > MEDIUM_IBUS_DIFF)
		pdpm->ibus_step = MEDIUM_STEP;
	else if (ibus_limit - pdpm->total_ibus > -MEDIUM_IBUS_DIFF)
		pdpm->ibus_step = SMALL_STEP;
	else if (ibus_limit - pdpm->total_ibus < -(MEDIUM_IBUS_DIFF + 50))
		pdpm->ibus_step = -SMALL_STEP;

	if (vbus_limit - pdpm->bbc_vbus > LARGE_VBUS_DIFF)
		pdpm->vbus_step = LARGE_STEP;
	else if (vbus_limit - pdpm->bbc_vbus > MEDIUM_VBUS_DIFF)
		pdpm->vbus_step = MEDIUM_STEP;
	else if (vbus_limit - pdpm->bbc_vbus > 0)
		pdpm->vbus_step = SMALL_STEP;
	else
		pdpm->vbus_step = -SMALL_STEP;

	final_step = min(min(pdpm->ibat_step, pdpm->vbat_step), min(pdpm->ibus_step, pdpm->vbus_step));
	if (pdpm->step_chg_fcc != pdpm->dts_config.max_fcc || pdpm->sw_cv) {
		if ((pdpm->final_step == SMALL_STEP && final_step == SMALL_STEP) || (pdpm->final_step == -SMALL_STEP && final_step == -SMALL_STEP))
			final_step = 0;
	}

	pdpm->final_step = final_step;
	if (pdpm->bypass_enable || pdpm->soc > 85)
		pdpm->final_step = cut_cap(pdpm->final_step, -3, 3);

	if (pdpm->final_step) {
		request_voltage = min(pdpm->request_voltage + pdpm->final_step * STEP_MV, vbus_limit);
		request_current = ibus_limit;
		ret = adapter_set_cap_xm(request_voltage, request_current);
		if (ret == ADAPTER_OK) {
			msleep(PDM_SM_DELAY_200MS);
			pdpm->request_voltage = request_voltage;
			pdpm->request_current = request_current;
		} else if (ret == ADAPTER_ERROR) {
			pdm_err("cann't find match pdo and continue\n");
			ret = adapter_set_cap_start_xm(request_voltage, request_current);
			if (ret == ADAPTER_OK) {
				msleep(PDM_SM_DELAY_200MS);
				pdpm->request_voltage = request_voltage;
				pdpm->request_current = request_current;
				pdpm->retry_count = 0;
			}else {
				pdpm->retry_count++;
				ret = ADAPTER_OK;
				pdm_err("set cap start failed and retry!\n");
			}
			if (pdpm->retry_count > 3) {
				pdpm->retry_count = 0;
				pdm_err("retry count over 3 and exit!\n");
			}
		}else {
			pdm_err("failed to tune PDO\n");
		}
	}

	return ret;
}

static int pdm_check_condition(struct usbpd_pm *pdpm)
{
	int min_vbus = 0, max_vbus = 0;

	if (product_name == PISSARRO) {
		min_vbus = (pdpm->bypass_enable ? (BYPASS_MIN_VBUS_1S - 100) : DIV2_MIN_VBUS_1S);
		max_vbus = (pdpm->bypass_enable ? BYPASS_MAX_VBUS_1S : DIV2_MAX_VBUS_1S);
	} else if (product_name == PISSARROPRO) {
		min_vbus = (pdpm->bypass_enable ? (BYPASS_MIN_VBUS_2S - 100) : DIV2_MIN_VBUS_2S);
		max_vbus = (pdpm->bypass_enable ? BYPASS_MAX_VBUS_2S : DIV2_MAX_VBUS_2S);
	}

	if (pdpm->state == PD_PM_STATE_TUNE && pdm_taper_charge(pdpm))
		return PDM_SM_EXIT;
	else if (pdpm->state == PD_PM_STATE_TUNE && (!pdpm->master_cp_enable || (!pdpm->disable_slave && !pdpm->slave_cp_enable)))
		return PDM_SM_HOLD;
	else if (pdpm->state == PD_PM_STATE_TUNE && pdpm->switch_mode)
		return PDM_SM_HOLD;
	else if (pdpm->input_suspend || pdpm->typec_burn || (pdpm->state != PD_PM_STATE_TUNE && pdpm->cv_wa_count >= CV_MP2762_WA_COUNT))
		return PDM_SM_HOLD;
	else if (pdpm->bms_i2c_error_count >= 10)
		return PDM_SM_EXIT;
	else if (pdpm->state == PD_PM_STATE_TUNE && !is_between(min_vbus, max_vbus, pdpm->bbc_vbus))
		return PDM_SM_HOLD;
	else if (!is_between(MIN_JEITA_CHG_INDEX, MAX_JEITA_CHG_INDEX, pdpm->jeita_chg_index))
		return PDM_SM_HOLD;
	else if (pdpm->thermal_limit_fcc < MIN_THERMAL_LIMIT_FCC)
		return PDM_SM_HOLD;
	else if (pdpm->state == PD_PM_STATE_ENTRY && pdpm->soc > pdpm->dts_config.high_soc)
		return PDM_SM_EXIT;
	else
		return PDM_SM_CONTINUE;
}

static void pdm_move_sm(struct usbpd_pm *pdpm, enum pdm_sm_state state)
{
	pdm_info("state change:%s -> %s\n", pm_str[pdpm->state], pm_str[state]);
	pdpm->last_state = pdpm->state;
	pdpm->state = state;
	pdpm->no_delay = true;
}

static bool pdm_handle_sm(struct usbpd_pm *pdpm)
{
	static bool last_disable_slave = false;
	int ret = 0;

	switch (pdpm->state) {
	case PD_PM_STATE_ENTRY:
		pdpm->tune_vbus_count = 0;
		pdpm->adapter_adjust_count = 0;
		pdpm->enable_cp_count = 0;
		pdpm->taper_count = 0;
		pdpm->final_step = 0;
		pdpm->step_chg_fcc = 0;
		pdpm->retry_count = 0;

		pdpm->sm_status = pdm_check_condition(pdpm);
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdm_info("PDM_SM_EXIT, don't start sm\n");
			return true;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			break;
		} else {
			vote(pdpm->bbc_icl_votable, PDM_VOTER, true, PDM_BBC_ICL);
			pdm_move_sm(pdpm, PD_PM_STATE_INIT_VBUS);
		}
		break;
	case PD_PM_STATE_INIT_VBUS:
		pdpm->tune_vbus_count++;
		if (pdpm->tune_vbus_count == 1 || pdpm->switch_mode) {
			pdpm->request_voltage = pdpm->entry_vbus;
			pdpm->request_current = pdpm->entry_ibus;
			if (product_name == PISSARROPRO) {
				charger_dev_enable_powerpath(pdpm->bbc_dev, false);
				msleep(250);
			}
			adapter_set_cap_start_xm(pdpm->request_voltage, pdpm->request_current);
			pdm_info("request first PDO = [%d %d]\n", pdpm->request_voltage, pdpm->request_current);
			break;
		}

		if (pdpm->tune_vbus_count >= MAX_VBUS_TUNE_COUNT) {
			pdm_err("failed to tune VBUS to target window, exit PDM\n");
			pdpm->sm_status = PDM_SM_EXIT;
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		} else if (pdpm->adapter_adjust_count >= MAX_ADAPTER_ADJUST_COUNT) {
			pdm_err("failed to request PDO, exit PDM\n");
			pdpm->sm_status = PDM_SM_EXIT;
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}

		if (pdpm->bbc_vbus <= (pdpm->vbat * (pdpm->bypass_enable ? 1 : 2) + pdpm->vbus_low_gap - ((pdpm->soc > 85) ? 360 : 0))) {
			pdpm->request_voltage += (pdpm->bypass_enable ? 1 : 2) * STEP_MV;
		} else if (pdpm->bbc_vbus >= pdpm->vbat * (pdpm->bypass_enable ? 1 : 2) + pdpm->vbus_high_gap) {
			pdpm->request_voltage -= (pdpm->bypass_enable ? 1 : 3) * STEP_MV;
		} else {
			pdm_info("success to tune VBUS to target window\n");
			pdm_move_sm(pdpm, PD_PM_STATE_ENABLE_CP);
			break;
		}

		ret = adapter_set_cap_xm(pdpm->request_voltage, pdpm->request_current);
		if (ret == ADAPTER_ADJUST) {
			pdpm->adapter_adjust_count++;
			pdm_err("failed to request PDO, try again\n");
			break;
		}
		break;
	case PD_PM_STATE_ENABLE_CP:
		pdpm->enable_cp_count++;
		if (pdpm->enable_cp_count >= MAX_ENABLE_CP_COUNT) {
			pdm_err("failed to enable charge pump, exit PDM\n");
			pdpm->enable_cp_fail_count++;
			if (pdpm->enable_cp_fail_count < 2)
				pdpm->sm_status = PDM_SM_HOLD;
			else
				pdpm->sm_status = PDM_SM_EXIT;
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}

		if (!pdpm->master_cp_enable)
			charger_dev_enable(pdpm->master_dev, true);

		if (!pdpm->disable_slave) {
			if (!pdpm->slave_cp_enable)
				charger_dev_enable(pdpm->slave_dev, true);
		} else {
			if (pdpm->slave_cp_enable)
				charger_dev_enable(pdpm->slave_dev, false);
		}

		if (pdpm->master_cp_bypass != pdpm->bypass_enable)
			charger_dev_enable_bypass(pdpm->master_dev, pdpm->bypass_enable);
		if (pdpm->slave_cp_bypass != pdpm->bypass_enable)
			charger_dev_enable_bypass(pdpm->slave_dev, pdpm->bypass_enable);

		if (pdpm->master_cp_enable && ((!pdpm->disable_slave && pdpm->slave_cp_enable) || (pdpm->disable_slave && !pdpm->slave_cp_enable)) &&
			((!pdpm->bypass_enable && !pdpm->master_cp_bypass && !pdpm->slave_cp_bypass) || (pdpm->bypass_enable && pdpm->master_cp_bypass && pdpm->slave_cp_bypass))) {
			pdm_info("success to enable charge pump\n");
			charger_dev_enable_powerpath(pdpm->bbc_dev, false);
			charger_dev_enable_termination(pdpm->bbc_dev, false);
			pdm_move_sm(pdpm, PD_PM_STATE_TUNE);
		} else {
			if (!pdpm->bypass_enable && pdpm->enable_cp_count > 1) {
				pdpm->request_voltage += STEP_MV;
				adapter_set_cap_xm(pdpm->request_voltage, pdpm->request_current);
				msleep(100);
			}
			pdm_err("failed to enable charge pump, try again\n");
			break;
		}
		break;
	case PD_PM_STATE_TUNE:
		pdpm->sm_status = pdm_check_condition(pdpm);
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdm_info("taper charge done\n");
			vote(pdpm->bbc_fcc_votable, PDM_VOTER, true, pdpm->dts_config.cv_ibat);
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
		} else {
			ret = pdm_tune_pdo(pdpm);
			if (ret == ADAPTER_ADJUST) {
				pdm_err("Adapter deny for invalid request, need to re-evaluate src caps!\n");
				pdm_evaluate_src_caps(pdpm);
				pdm_move_sm(pdpm, PD_PM_STATE_INIT_VBUS);
			} else if (ret == ADAPTER_ERROR) {
				pdpm->sm_status = PDM_SM_HOLD;
				pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			}
		}

		if (last_disable_slave != pdpm->disable_slave) {
			last_disable_slave = pdpm->disable_slave;
			charger_dev_enable(pdpm->slave_dev, !pdpm->disable_slave);
		}
		break;
	case PD_PM_STATE_EXIT:
		pdpm->tune_vbus_count = 0;
		pdpm->adapter_adjust_count = 0;
		pdpm->enable_cp_count = 0;
		pdpm->taper_count = 0;
		pdpm->disable_slave = false;

		charger_dev_enable_powerpath(pdpm->bbc_dev, false);
		charger_dev_enable(pdpm->master_dev, false);
		charger_dev_enable(pdpm->slave_dev, false);
		msleep(50);
		charger_dev_enable_bypass(pdpm->master_dev, false);
		charger_dev_enable_bypass(pdpm->slave_dev, false);
		msleep(400);

		if (pdpm->typec_burn || product_name == PISSARRO || pdpm->sm_status == PDM_SM_EXIT)
			ret = adapter_set_cap_xm(DEFAULT_PDO_VBUS_1S, DEFAULT_PDO_IBUS_1S);
		else
			ret = adapter_set_cap_xm(DEFAULT_PDO_VBUS_2S, DEFAULT_PDO_IBUS_2S);
		if (ret != ADAPTER_OK)
			ret = adapter_set_cap(5500, 2000);

		if (pdpm->sm_status == PDM_SM_EXIT)
			msleep(500);
		else
			msleep(50);

		vote(pdpm->bbc_icl_votable, PDM_VOTER, false, 0);
		charger_dev_enable_termination(pdpm->bbc_dev, true);
		charger_dev_enable_powerpath(pdpm->bbc_dev, true);

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
	int internal = PDM_SM_DELAY_300MS;

	pdm_update_status(pdpm);
	pdm_bypass_check(pdpm);
	pdm_disable_slave_check(pdpm);

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
				internal = PDM_SM_DELAY_300MS;
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
	union power_supply_propval pval = {0, };

	cancel_delayed_work_sync(&pdpm->main_sm_work);
	charger_dev_enable(pdpm->master_dev, false);
	charger_dev_enable(pdpm->slave_dev, false);
	charger_dev_enable_bypass(pdpm->master_dev, false);
	charger_dev_enable_bypass(pdpm->slave_dev, false);
	vote(pdpm->bbc_icl_votable, PDM_VOTER, false, 0);
	vote(pdpm->bbc_vinmin_votable, PDM_VOTER, false, 0);
	charger_dev_enable_powerpath(pdpm->bbc_dev, true);
	charger_dev_enable_termination(pdpm->bbc_dev, true);

	pdpm->pd_type = MTK_PD_CONNECT_NONE;
	pdpm->psy_type = POWER_SUPPLY_TYPE_UNKNOWN;
	pdpm->bypass_enable = false;
	pdpm->pd_verify_done = false;
	pdpm->pdo_bypass_support = false;
	pdpm->disable_slave = false;
	pdpm->low_ibus_count = 0;
	pdpm->tune_vbus_count = 0;
	pdpm->retry_count = 0;
	pdpm->adapter_adjust_count = 0;
	pdpm->enable_cp_count = 0;
	pdpm->enable_cp_fail_count = 0;
	pdpm->bms_i2c_error_count = 0;
	pdpm->apdo_max_vbus = 0;
	pdpm->apdo_min_vbus = 0;
	pdpm->apdo_max_ibus = 0;
	pdpm->apdo_max_watt = 0;
	pdpm->final_step = 0;
	pdpm->step_chg_fcc = 0;
	pdpm->thermal_limit_fcc = 0;
	pdpm->last_time.tv_sec = 0;
	memset(&pdpm->cap, 0, sizeof(struct pd_cap));

	pval.intval = 0;
	power_supply_set_property(pdpm->usb_psy, POWER_SUPPLY_PROP_APDO_MAX, &pval);

	pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
	pdpm->last_state = PD_PM_STATE_ENTRY;
}

static void pdm_psy_change(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, psy_change_work);
	union power_supply_propval val = {0,};
	int ret = 0;

	ret = power_supply_get_property(pdpm->usb_psy, POWER_SUPPLY_PROP_PD_TYPE, &val);
	if (ret) {
		pdm_err("Failed to read pd type!\n");
		goto out;
	} else {
		pdpm->pd_type = val.intval;
	}

	ret = power_supply_get_property(pdpm->usb_psy, POWER_SUPPLY_PROP_PD_VERIFY_DONE, &val);
	if (ret) {
		pdm_err("Failed to read pd_verify_done!\n");
		goto out;
	} else {
		pdpm->pd_verify_done = val.intval;
	}

	ret = power_supply_get_property(pdpm->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &val);
	if (ret) {
		pdm_err("Failed to read real_type\n");
		goto out;
	} else {
		pdpm->psy_type = val.intval;
	}

	ret = power_supply_get_property(pdpm->usb_psy, POWER_SUPPLY_PROP_CV_WA_COUNT, &val);
	if (ret) {
		pdm_err("Failed to read cv_wa_count\n");
		goto out;
	} else {
		pdpm->cv_wa_count = val.intval;
	}

	pdm_info("[pd_type pd_verify_done psy_type pdm_active cv_wa_count] = [%d %d %d %d %d]\n", pdpm->pd_type, pdpm->pd_verify_done, pdpm->psy_type, pdpm->pdm_active, pdpm->cv_wa_count);

	if (pdpm->cv_wa_count >= CV_MP2762_WA_COUNT && pdpm->psy_type == POWER_SUPPLY_TYPE_USB_PD)
		pdm_cv_wa_func(pdpm);

	if (!pdpm->pdm_active && pdpm->psy_type == POWER_SUPPLY_TYPE_USB_PD && pdpm->pd_verify_done) {
		if (pdm_evaluate_src_caps(pdpm)) {
			pdpm->pdm_active = true;
			schedule_delayed_work(&pdpm->main_sm_work, 0);
		}
	} else if (pdpm->pdm_active && pdpm->pd_type != MTK_PD_CONNECT_PE_READY_SNK_APDO) {
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
	rc = of_property_read_u32(node, "cv_vbat", &pdpm->dts_config.cv_vbat);
	rc = of_property_read_u32(node, "cv_vbat_ffc", &pdpm->dts_config.cv_vbat_ffc);
	rc = of_property_read_u32(node, "cv_ibat", &pdpm->dts_config.cv_ibat);

	rc = of_property_read_u32(node, "vbus_low_gap_div", &vbus_low_gap_div);
	rc = of_property_read_u32(node, "vbus_high_gap_div", &vbus_high_gap_div);
	rc = of_property_read_u32(node, "min_pdo_vbus", &pdpm->dts_config.min_pdo_vbus);
	rc = of_property_read_u32(node, "max_pdo_vbus", &pdpm->dts_config.max_pdo_vbus);
	rc = of_property_read_u32(node, "max_bbc_vbus", &pdpm->dts_config.max_bbc_vbus);
	rc = of_property_read_u32(node, "min_bbc_vbus", &pdpm->dts_config.min_bbc_vbus);

#ifdef CONFIG_FACTORY_BUILD
	if (product_name == PISSARRO) {
		vbus_low_gap_div += 200;
		vbus_high_gap_div += 200;
	} else if (product_name == PISSARROPRO) {
		vbus_low_gap_div += 200;
		vbus_high_gap_div += 200;
	}
#endif

	pdm_info("parse config, FV = %d, FV_FFC = %d, FCC = [%d %d %d], MAX_VBUS = %d, MAX_IBUS = %d, CV = [%d %d %d], ENTRY = [%d %d %d %d], PDO_GAP = [%d %d %d %d %d %d]\n",
			pdpm->dts_config.fv, pdpm->dts_config.fv_ffc, pdpm->dts_config.max_fcc, pdpm->dts_config.fcc_low_hyst, pdpm->dts_config.fcc_high_hyst,
			pdpm->dts_config.max_vbus, pdpm->dts_config.max_ibus, pdpm->dts_config.cv_vbat, pdpm->dts_config.cv_vbat_ffc, pdpm->dts_config.cv_ibat,
			pdpm->dts_config.low_tbat, pdpm->dts_config.high_tbat, pdpm->dts_config.high_vbat, pdpm->dts_config.high_soc,
			vbus_low_gap_div, vbus_high_gap_div, pdpm->dts_config.min_pdo_vbus, pdpm->dts_config.max_pdo_vbus, pdpm->dts_config.max_bbc_vbus, pdpm->dts_config.min_bbc_vbus);

	return rc;
}

static void pdm_parse_cmdline(void)
{
	char *pissarro = NULL, *pissarropro = NULL, *pissarroinpro = NULL;

	pissarro = strnstr(saved_command_line, "pissarro", strlen(saved_command_line));
	pissarropro = strnstr(saved_command_line, "pissarropro", strlen(saved_command_line));
	pissarroinpro = strnstr(saved_command_line, "pissarroinpro", strlen(saved_command_line));

	pdm_info("pissarro = %d, pissarropro = %d, pissarroinpro = %d\n", pissarro ? 1 : 0, pissarropro ? 1 : 0, pissarroinpro ? 1 : 0);

	if (pissarropro || pissarroinpro)
		product_name = PISSARROPRO;
	else if (pissarro)
		product_name = PISSARRO;
}

static const struct platform_device_id pdm_id[] = {
	{ "pissarro_pd_cp_manager", PISSARRO },
	{ "pissarropro_pd_cp_manager", PISSARROPRO },
	{},
};
MODULE_DEVICE_TABLE(platform, pdm_id);

static const struct of_device_id pdm_of_match[] = {
	{ .compatible = "pissarro_pd_cp_manager", .data = &pdm_id[0], },
	{ .compatible = "pissarropro_pd_cp_manager", .data = &pdm_id[1], },
	{},
};
MODULE_DEVICE_TABLE(of, pdm_of_match);

static int pdm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct usbpd_pm *pdpm;
	const struct of_device_id *of_id;

	pdm_parse_cmdline();

	of_id = of_match_device(pdm_of_match, &pdev->dev);
	pdev->id_entry = of_id->data;

	if (pdev->id_entry->driver_data == product_name) {
		pdm_info("PDM probe start\n");
	} else {
		pdm_info("driver_data and product_name not match, don't probe, %d\n", pdev->id_entry->driver_data);
		return -ENODEV;
	}

	pdpm = kzalloc(sizeof(struct usbpd_pm), GFP_KERNEL);
	if (!pdpm)
		return -ENOMEM;

	pdpm->dev = dev;
	pdpm->bypass_enable = false;
	pdpm->last_time.tv_sec = 0;
	spin_lock_init(&pdpm->psy_change_lock);
	platform_set_drvdata(pdev, pdpm);

	ret = pd_policy_parse_dt(pdpm);
	if (ret < 0) {
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
	}

	INIT_WORK(&pdpm->psy_change_work, pdm_psy_change);
	INIT_DELAYED_WORK(&pdpm->main_sm_work, pdm_main_sm);

	pdpm->nb.notifier_call = usbpdm_psy_notifier_cb;
	power_supply_reg_notifier(&pdpm->nb);

	ret = sysfs_create_group(&pdpm->dev->kobj, &pdm_attr_group);
	if (ret) {
		pdm_err("failed to register sysfs\n");
		return ret;
	}

	pdm_info("PDM probe success\n");
	return ret;
}

static int pdm_remove(struct platform_device *pdev)
{
	struct usbpd_pm *pdpm = platform_get_drvdata(pdev);

	power_supply_unreg_notifier(&pdpm->nb);
	cancel_delayed_work(&pdpm->main_sm_work);
	cancel_work(&pdpm->psy_change_work);

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
MODULE_AUTHOR("Chenyichun");
MODULE_DESCRIPTION("charge pump manager for PD");
MODULE_LICENSE("GPL");
