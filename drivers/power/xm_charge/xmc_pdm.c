/*******************************************************************************************
* Description:	XIAOMI-BSP-CHARGE
* 		This xmc_pdm.c is the fast charge manager for PPS.
* ------------------------------ Revision History: --------------------------------
* <version>	<date>		<author>			<desc>
* 1.0		2022-02-22	chenyichun@xiaomi.com		Created for new architecture
********************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
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

#include "xmc_core.h"
#include "xmc_pdm.h"

static const unsigned char *state_name[] = {
	"PDM_STATE_ENTRY",
	"PDM_STATE_INIT_VBUS",
	"PDM_STATE_ENABLE_CP",
	"PDM_STATE_TUNE",
	"PDM_STATE_EXIT",
};

static struct charge_chip *g_chip = NULL;

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

static bool pdm_evaluate_src_caps(struct pdm_chip *pdm)
{
	union power_supply_propval val = {0,};
	bool legal_pdo = false;
	int i = 0;

	pdm->apdo_max_vbus = 0;
	pdm->apdo_min_vbus = 0;
	pdm->apdo_max_ibus = 0;
	pdm->apdo_max_watt = 0;

	xmc_ops_get_cap(g_chip->adapter_dev, XMC_PDO_PD, &pdm->cap);

	for (i = 0; i < pdm->cap.nr; i++) {
		xmc_info("[XMC_PDM] PDO: max_mv = %d, min_mv = %d, ma = %d, max_watt = %d, type = %d\n",
			pdm->cap.max_mv[i], pdm->cap.min_mv[i], pdm->cap.ma[i], pdm->cap.maxwatt[i], pdm->cap.type[i]);
		if (pdm->cap.type[i] != XMC_PDO_APDO || pdm->cap.max_mv[i] < pdm->dts_config.min_pdo_vbus || pdm->cap.max_mv[i] > pdm->dts_config.max_pdo_vbus)
			continue;

		if (pdm->cap.maxwatt[i] > pdm->apdo_max_watt) {
			pdm->apdo_max_vbus = pdm->cap.max_mv[i];
			pdm->apdo_min_vbus = pdm->cap.min_mv[i];
			pdm->apdo_max_ibus = pdm->cap.ma[i];
			pdm->apdo_max_watt = pdm->cap.maxwatt[i];
		}

		legal_pdo = true;
	}

	power_supply_get_property(g_chip->battery_psy, POWER_SUPPLY_PROP_CAPACITY, &val);

	if (legal_pdo) {
		if (g_chip->usb_typec.pd_type == XMC_PD_TYPE_PPS) {
			pdm->pdo_bypass_support = false;
			xmc_info("[XMC_PDM] MAX_PDO = [%d %d %d]\n", pdm->apdo_max_vbus, pdm->apdo_max_ibus, pdm->apdo_max_watt / 1000000);
		} else {
			legal_pdo = false;
		}
	}

	return legal_pdo;
}

static bool pdm_taper_charge(struct pdm_chip *pdm)
{
	int cv_vbat = 0;

	cv_vbat = pdm->ffc_enable ? pdm->dts_config.cv_vbat_ffc : pdm->dts_config.cv_vbat;
	if (pdm->vbat > cv_vbat && (-pdm->ibat) < pdm->dts_config.cv_ibat)
		pdm->taper_count++;
	else
		pdm->taper_count = 0;

	if (pdm->taper_count > MAX_TAPER_COUNT)
		return true;
	else
		return false;
}

static void pdm_update_status(struct pdm_chip *pdm)
{
	union power_supply_propval val = {0,};

	xmc_ops_get_vbus(g_chip->master_cp_dev, &pdm->master_cp_vbus);
	xmc_ops_get_ibus(g_chip->master_cp_dev, &pdm->master_cp_ibus);
	xmc_ops_get_ibus(g_chip->slave_cp_dev, &pdm->slave_cp_ibus);
	xmc_ops_get_charge_enable(g_chip->master_cp_dev, &pdm->master_cp_enable);
	xmc_ops_get_charge_enable(g_chip->slave_cp_dev, &pdm->slave_cp_enable);
	pdm->master_cp_bypass = false;
	pdm->slave_cp_bypass = false;

	pdm->total_ibus = pdm->master_cp_ibus + pdm->slave_cp_ibus;

//	power_supply_get_property(g_chip->master_psy, POWER_SUPPLY_PROP_BYPASS_SUPPORT, &val);
	pdm->cp_bypass_support = false;

	power_supply_get_property(g_chip->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	pdm->ibat = val.intval / 1000;

	power_supply_get_property(g_chip->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	pdm->vbat = val.intval / 1000;

	power_supply_get_property(g_chip->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	pdm->soc = val.intval;
	pdm->bms_i2c_error_count = 0;
	pdm->jeita_chg_index = 4;
	pdm->ffc_enable = true;
	pdm->sw_cv = 0;
	pdm->input_suspend = false;
	pdm->typec_burn = false;

	pdm->step_chg_fcc = get_client_vote(g_chip->bbc_fcc_votable, STEP_CHARGE_VOTER);
	pdm->thermal_limit_fcc = min(get_client_vote(g_chip->bbc_fcc_votable, THERMAL_VOTER), pdm->dts_config.max_fcc);
	pdm->target_fcc = get_effective_result(g_chip->bbc_fcc_votable);

	xmc_info("[XMC_PDM] BUS = [%d %d %d %d], CP = [%d %d %d %d], BAT = [%d %d %d], STEP = [%d %d %d %d %d], CC_CV = [%d %d]\n",
		pdm->master_cp_vbus, pdm->master_cp_ibus, pdm->slave_cp_ibus, pdm->total_ibus,
		pdm->master_cp_enable, pdm->slave_cp_enable, pdm->master_cp_bypass, pdm->slave_cp_bypass,
		pdm->vbat, pdm->ibat, pdm->soc,
		pdm->vbat_step, pdm->ibat_step, pdm->vbus_step, pdm->ibus_step, pdm->final_step,
		pdm->target_fcc, pdm->thermal_limit_fcc);
}

static void pdm_bypass_check(struct pdm_chip *pdm)
{
	ktime_t ktime_now;
	struct timespec64 time_now;

	pdm->bypass_entry_fcc = bypass_entry_fcc_pissarro;
	pdm->bypass_exit_fcc  = bypass_exit_fcc_pissarro;

	if (!pdm->cp_bypass_support || !pdm->pdo_bypass_support || pdm->apdo_max_watt <= MAX_WATT_33W || true) {
		pdm->bypass_enable = false;
		pdm->switch_mode = false;
	} else {
		if (pdm->soc < 92 && (pdm->state == PDM_STATE_TUNE || pdm->state == PDM_STATE_ENTRY)) {
			if (pdm->bypass_enable) {
				if (pdm->thermal_limit_fcc >= pdm->bypass_exit_fcc)
					pdm->switch_mode = true;
				else
					pdm->switch_mode = false;
			} else {
				if (pdm->thermal_limit_fcc <= pdm->bypass_entry_fcc)
					pdm->switch_mode = true;
				else
					pdm->switch_mode = false;
			}
		} else {
			pdm->switch_mode = false;
		}
	}

	if (pdm->switch_mode) {
		ktime_now = ktime_get_boottime();
		time_now = ktime_to_timespec64(ktime_now);
		if (pdm->last_switch_time.tv_sec == 0 || time_now.tv_sec - pdm->last_switch_time.tv_sec >= 15) {
			pdm->bypass_enable = !pdm->bypass_enable;
			pdm->last_switch_time.tv_sec = time_now.tv_sec;
		} else {
			pdm->switch_mode = false;
		}
	}

	pdm->vbus_low_gap = pdm->bypass_enable ? vbus_low_gap_bypass : vbus_low_gap_div;
	pdm->vbus_high_gap = pdm->bypass_enable ? vbus_high_gap_bypass : vbus_high_gap_div;
	pdm->ibus_gap = pdm->bypass_enable ? ibus_gap_bypass : ibus_gap_div;

	pdm->entry_vbus = min(min(((pdm->vbat * (pdm->bypass_enable ? 1 : 4)) + pdm->vbus_low_gap), pdm->dts_config.max_vbus), pdm->apdo_max_vbus);
	pdm->entry_ibus = min(min(((pdm->target_fcc / (pdm->bypass_enable ? 1 : 4)) + pdm->ibus_gap), pdm->dts_config.max_ibus), pdm->apdo_max_ibus);
	if (!pdm->bypass_enable && pdm->soc > 85)
		pdm->entry_vbus = min(min(pdm->entry_vbus - 360, pdm->dts_config.max_vbus), pdm->apdo_max_vbus);
}

static void pdm_disable_slave_check(struct pdm_chip *pdm)
{
	return;
	if (pdm->bypass_enable) {
		pdm->low_ibus_count = 0;
		pdm->disable_slave = false;
	} else if (pdm->apdo_max_watt <= MAX_WATT_33W) {
		pdm->low_ibus_count = 0;
		pdm->disable_slave = true;
	} else if (pdm->thermal_limit_fcc < LOW_THERMAL_LIMIT_FCC) {
		pdm->low_ibus_count = 0;
		pdm->disable_slave = true;
	} else if (pdm->state == PDM_STATE_TUNE) {
		if (pdm->slave_cp_ibus <= LOW_CP_IBUS) {
			if (pdm->low_ibus_count < MAX_LOW_CP_IBUS_COUNT)
				pdm->low_ibus_count++;
			if (pdm->low_ibus_count >= MAX_LOW_CP_IBUS_COUNT)
				pdm->disable_slave = true;
			else
				pdm->disable_slave = false;
		} else {
			pdm->low_ibus_count = 0;
			pdm->disable_slave = false;
		}
	}
}

static int pdm_tune_pdo(struct pdm_chip *pdm)
{
	int fv = 0, ibus_limit = 0, vbus_limit = 0, request_voltage = 0, request_current = 0, final_step = 0, ret = 0;

	if (pdm->sw_cv)
		fv = pdm->sw_cv;
	else
		fv = pdm->ffc_enable ? pdm->dts_config.fv_ffc : pdm->dts_config.fv;

//	if (pdm->request_voltage > pdm->master_cp_vbus + pdm->total_ibus * MAX_CABLE_RESISTANCE / 1000)
//		pdm->request_voltage = pdm->master_cp_vbus + pdm->total_ibus * MAX_CABLE_RESISTANCE / 1000;

	pdm->ibat_step = pdm->vbat_step = pdm->ibus_step = pdm->vbus_step = 0;
	ibus_limit = min(min(((pdm->target_fcc / (pdm->bypass_enable ? 1 : 2)) + pdm->ibus_gap), pdm->dts_config.max_ibus), pdm->apdo_max_ibus);
	if (pdm->apdo_max_ibus <= 3000)
		ibus_limit = min(ibus_limit, pdm->apdo_max_ibus - 200);
	vbus_limit = min(pdm->dts_config.max_vbus, pdm->apdo_max_vbus);

	if ((-pdm->ibat) < (pdm->target_fcc - pdm->dts_config.fcc_low_hyst)) {
		if (((pdm->target_fcc - pdm->dts_config.fcc_low_hyst) - (-pdm->ibat)) > LARGE_IBAT_DIFF)
			pdm->ibat_step = LARGE_STEP;
		else if (((pdm->target_fcc - pdm->dts_config.fcc_low_hyst) - (-pdm->ibat)) > MEDIUM_IBAT_DIFF)
			pdm->ibat_step = MEDIUM_STEP;
		else
			pdm->ibat_step = SMALL_STEP;
	} else if ((-pdm->ibat) > (pdm->target_fcc + pdm->dts_config.fcc_high_hyst)) {
		if (((-pdm->ibat) - (pdm->target_fcc + pdm->dts_config.fcc_high_hyst)) > LARGE_IBAT_DIFF)
			pdm->ibat_step = -LARGE_STEP;
		else if (((-pdm->ibat) - (pdm->target_fcc + pdm->dts_config.fcc_high_hyst)) > MEDIUM_IBAT_DIFF)
			pdm->ibat_step = -MEDIUM_STEP;
		else
			pdm->ibat_step = -SMALL_STEP;
	} else {
		pdm->ibat_step = 0;
	}

	if (fv - pdm->vbat > LARGE_VBAT_DIFF)
		pdm->vbat_step = LARGE_STEP;
	else if (fv - pdm->vbat > MEDIUM_VBAT_DIFF)
		pdm->vbat_step = MEDIUM_STEP;
	else if (fv - pdm->vbat > 5)
		pdm->vbat_step = SMALL_STEP;
	else if (fv - pdm->vbat < -2)
		pdm->vbat_step = -MEDIUM_STEP;
	else if (fv - pdm->vbat < 0)
		pdm->vbat_step = -SMALL_STEP;

	if (ibus_limit - pdm->total_ibus > LARGE_IBUS_DIFF)
		pdm->ibus_step = LARGE_STEP;
	else if (ibus_limit - pdm->total_ibus > MEDIUM_IBUS_DIFF)
		pdm->ibus_step = MEDIUM_STEP;
	else if (ibus_limit - pdm->total_ibus > -MEDIUM_IBUS_DIFF)
		pdm->ibus_step = SMALL_STEP;
	else if (ibus_limit - pdm->total_ibus < -(MEDIUM_IBUS_DIFF + 50))
		pdm->ibus_step = -SMALL_STEP;

	if (vbus_limit - pdm->master_cp_vbus > LARGE_VBUS_DIFF)
		pdm->vbus_step = LARGE_STEP;
	else if (vbus_limit - pdm->master_cp_vbus > MEDIUM_VBUS_DIFF)
		pdm->vbus_step = MEDIUM_STEP;
	else if (vbus_limit - pdm->master_cp_vbus > 0)
		pdm->vbus_step = SMALL_STEP;
	else
		pdm->vbus_step = -SMALL_STEP;

	final_step = min(min(pdm->ibat_step, pdm->vbat_step), min(pdm->ibus_step, pdm->vbus_step));
	if (pdm->step_chg_fcc != pdm->dts_config.max_fcc || pdm->sw_cv) {
		if ((pdm->final_step == SMALL_STEP && final_step == SMALL_STEP) || (pdm->final_step == -SMALL_STEP && final_step == -SMALL_STEP))
			final_step = 0;
	}

	pdm->final_step = final_step;
	if (pdm->bypass_enable || pdm->soc > 85)
		pdm->final_step = cut_cap(pdm->final_step, -3, 3);

	if (pdm->final_step) {
		request_voltage = min(pdm->request_voltage + pdm->final_step * STEP_MV, vbus_limit);
		request_current = ibus_limit;
		ret = xmc_ops_set_cap(g_chip->adapter_dev, XMC_PDO_APDO_START, request_voltage, request_current);
		if (ret == TCPM_SUCCESS) {
			msleep(PDM_SM_DELAY_200MS);
			pdm->request_voltage = request_voltage;
			pdm->request_current = request_current;
		} else {
			xmc_err("[XMC_PDM] failed to tune PDO\n");
		}
	}

	return ret;
}

static int pdm_check_condition(struct pdm_chip *pdm)
{
	if (pdm->state == PDM_STATE_TUNE && pdm_taper_charge(pdm))
		return PDM_STATUS_EXIT;
	else if (pdm->state == PDM_STATE_TUNE && (!pdm->master_cp_enable || (!pdm->disable_slave && !pdm->slave_cp_enable)))
		return PDM_STATUS_HOLD;
	else if (pdm->state == PDM_STATE_TUNE && pdm->switch_mode)
		return PDM_STATUS_HOLD;
	else if (pdm->input_suspend || pdm->typec_burn)
		return PDM_STATUS_HOLD;
	else if (pdm->bms_i2c_error_count >= 10)
		return PDM_STATUS_EXIT;
	else if (!is_between(MIN_JEITA_CHG_INDEX, MAX_JEITA_CHG_INDEX, pdm->jeita_chg_index))
		return PDM_STATUS_HOLD;
	else if (pdm->thermal_limit_fcc > 0 && pdm->thermal_limit_fcc < MIN_THERMAL_LIMIT_FCC)
		return PDM_STATUS_HOLD;
	else if (pdm->state == PDM_STATE_ENTRY && pdm->soc > pdm->dts_config.high_soc)
		return PDM_STATUS_EXIT;
	else
		return PDM_STATUS_CONTINUE;
}

static void pdm_move_sm(struct pdm_chip *pdm, enum pdm_sm_state state)
{
	xmc_info("[XMC_PDM] state change:%s -> %s\n", state_name[pdm->state], state_name[state]);
	pdm->last_state = pdm->state;
	pdm->state = state;
	pdm->no_delay = true;
}

static bool pdm_handle_sm(struct pdm_chip *pdm)
{
	static bool last_disable_slave = false;
	int ret = 0;

	switch (pdm->state) {
	case PDM_STATE_ENTRY:
		pdm->tune_vbus_count = 0;
		pdm->adapter_adjust_count = 0;
		pdm->enable_cp_count = 0;
		pdm->taper_count = 0;
		pdm->final_step = 0;
		pdm->step_chg_fcc = 0;

		pdm->sm_status = pdm_check_condition(pdm);
		if (pdm->sm_status == PDM_STATUS_EXIT) {
			xmc_info("[XMC_PDM] PDM_STATUS_EXIT, don't start sm\n");
			return true;
		} else if (pdm->sm_status == PDM_STATUS_HOLD) {
			break;
		} else {
			vote(g_chip->bbc_icl_votable, PDM_VOTER, true, PDM_BBC_ICL);
			pdm_move_sm(pdm, PDM_STATE_INIT_VBUS);
			xmc_ops_set_div_mode(g_chip->master_cp_dev, XMC_CP_4T1);
			xmc_ops_set_div_mode(g_chip->slave_cp_dev, XMC_CP_4T1);
			xmc_ops_device_init(g_chip->master_cp_dev, XMC_CP_4T1);
			xmc_ops_device_init(g_chip->slave_cp_dev, XMC_CP_4T1);
			xmc_ops_powerpath_enable(g_chip->bbc_dev, false);
			xmc_ops_adc_enable(g_chip->master_cp_dev, true);
			xmc_ops_adc_enable(g_chip->slave_cp_dev, true);
		}
		break;
	case PDM_STATE_INIT_VBUS:
		pdm->tune_vbus_count++;
		if (pdm->tune_vbus_count == 1 || pdm->switch_mode) {
			pdm->request_voltage = pdm->entry_vbus;
			pdm->request_current = pdm->entry_ibus;
			xmc_ops_set_cap(g_chip->adapter_dev, XMC_PDO_APDO_START, pdm->request_voltage, pdm->request_current);
			xmc_info("[XMC_PDM] request first PDO = [%d %d]\n", pdm->request_voltage, pdm->request_current);
			break;
		}

		if (pdm->tune_vbus_count >= MAX_VBUS_TUNE_COUNT) {
			xmc_err("[XMC_PDM] failed to tune VBUS to target window, exit PDM\n");
			pdm->sm_status = PDM_STATUS_EXIT;
			pdm_move_sm(pdm, PDM_STATE_EXIT);
			break;
		} else if (pdm->adapter_adjust_count >= MAX_ADAPTER_ADJUST_COUNT) {
			xmc_err("[XMC_PDM] failed to request PDO, exit PDM\n");
			pdm->sm_status = PDM_STATUS_EXIT;
			pdm_move_sm(pdm, PDM_STATE_EXIT);
			break;
		}

		if (pdm->master_cp_vbus <= (pdm->vbat * (pdm->bypass_enable ? 1 : 4) + pdm->vbus_low_gap - ((pdm->soc > 85) ? 360 : 0))) {
			pdm->request_voltage += (pdm->bypass_enable ? 1 : 4) * STEP_MV;
		} else if (pdm->master_cp_vbus >= pdm->vbat * (pdm->bypass_enable ? 1 : 4) + pdm->vbus_high_gap) {
			pdm->request_voltage -= (pdm->bypass_enable ? 1 : 3) * STEP_MV;
		} else {
			xmc_info("[XMC_PDM] success to tune VBUS to target window\n");
			pdm_move_sm(pdm, PDM_STATE_ENABLE_CP);
			break;
		}

		ret = xmc_ops_set_cap(g_chip->adapter_dev, XMC_PDO_APDO_START, pdm->request_voltage, pdm->request_current);
		if (ret != TCPM_SUCCESS) {
			pdm->adapter_adjust_count++;
			xmc_err("[XMC_PDM] failed to request PDO, try again\n");
			break;
		}
		break;
	case PDM_STATE_ENABLE_CP:
		pdm->enable_cp_count++;
		if (pdm->enable_cp_count >= MAX_ENABLE_CP_COUNT) {
			xmc_err("[XMC_PDM] failed to enable charge pump, exit PDM\n");
			pdm->enable_cp_fail_count++;
			if (pdm->enable_cp_fail_count < 2)
				pdm->sm_status = PDM_STATUS_HOLD;
			else
				pdm->sm_status = PDM_STATUS_EXIT;
			pdm_move_sm(pdm, PDM_STATE_EXIT);
			break;
		}

		if (!pdm->master_cp_enable)
			xmc_ops_charge_enable(g_chip->master_cp_dev, true);

		if (!pdm->disable_slave) {
			if (!pdm->slave_cp_enable)
				xmc_ops_charge_enable(g_chip->slave_cp_dev, true);
		} else {
			if (pdm->slave_cp_enable)
				xmc_ops_charge_enable(g_chip->slave_cp_dev, false);
		}

		if (pdm->master_cp_enable && ((!pdm->disable_slave && pdm->slave_cp_enable) || (pdm->disable_slave && !pdm->slave_cp_enable)) &&
			((!pdm->bypass_enable && !pdm->master_cp_bypass && !pdm->slave_cp_bypass) || (pdm->bypass_enable && pdm->master_cp_bypass && pdm->slave_cp_bypass))) {
			xmc_info("success to enable charge pump\n");
			xmc_ops_powerpath_enable(g_chip->bbc_dev, false);
			pdm_move_sm(pdm, PDM_STATE_TUNE);
		} else {
			if (!pdm->bypass_enable && pdm->enable_cp_count > 1) {
				pdm->request_voltage += STEP_MV;
				xmc_ops_set_cap(g_chip->adapter_dev, XMC_PDO_APDO, pdm->request_voltage, pdm->request_current);
				msleep(100);
			}
			xmc_err("failed to enable charge pump, try again\n");
			break;
		}

		break;
	case PDM_STATE_TUNE:
		pdm->sm_status = pdm_check_condition(pdm);
		if (pdm->sm_status == PDM_STATUS_EXIT) {
			xmc_info("[XMC_PDM] taper charge done\n");
			vote(g_chip->bbc_fcc_votable, PDM_VOTER, true, pdm->dts_config.cv_ibat);
			pdm_move_sm(pdm, PDM_STATE_EXIT);
		} else if (pdm->sm_status == PDM_STATUS_HOLD) {
			pdm_move_sm(pdm, PDM_STATE_EXIT);
		} else {
			ret = pdm_tune_pdo(pdm);
			if (ret != TCPM_SUCCESS) {
				pdm->sm_status = PDM_STATUS_HOLD;
				pdm_move_sm(pdm, PDM_STATE_EXIT);
			}
		}

		if (last_disable_slave != pdm->disable_slave) {
			last_disable_slave = pdm->disable_slave;
			xmc_ops_charge_enable(g_chip->slave_cp_dev, !pdm->disable_slave);
		}
		break;
	case PDM_STATE_EXIT:
		pdm->tune_vbus_count = 0;
		pdm->adapter_adjust_count = 0;
		pdm->enable_cp_count = 0;
		pdm->taper_count = 0;
		pdm->disable_slave = false;

		xmc_ops_powerpath_enable(g_chip->bbc_dev, true);
		xmc_ops_charge_enable(g_chip->master_cp_dev, false);
		xmc_ops_charge_enable(g_chip->slave_cp_dev, false);

		if (pdm->sm_status == PDM_STATUS_EXIT)
			msleep(500);
		else
			msleep(50);

		vote(g_chip->bbc_icl_votable, PDM_VOTER, false, 0);

		if (pdm->sm_status == PDM_STATUS_EXIT) {
			return true;
		} else if (pdm->sm_status == PDM_STATUS_HOLD) {
			pdm_evaluate_src_caps(pdm);
			pdm_move_sm(pdm, PDM_STATE_ENTRY);
		}

		break;
	default:
		xmc_err("[XMC_PDM] not supportted pdm_sm_state\n");
		break;
	}

	return false;
}

static void pdm_main_sm(struct work_struct *work)
{
	struct pdm_chip *pdm = container_of(work, struct pdm_chip, main_sm_work.work);
	int internal = PDM_SM_DELAY_300MS;

	pdm_update_status(pdm);
	pdm_bypass_check(pdm);
	pdm_disable_slave_check(pdm);

	if (!pdm_handle_sm(pdm) && pdm->pdm_active) {
		if (pdm->no_delay) {
			internal = 0;
			pdm->no_delay = false;
		} else {
			switch (pdm->state) {
			case PDM_STATE_ENTRY:
			case PDM_STATE_EXIT:
			case PDM_STATE_INIT_VBUS:
				internal = PDM_SM_DELAY_200MS;
				break;
			case PDM_STATE_ENABLE_CP:
				internal = PDM_SM_DELAY_200MS;
				break;
			case PDM_STATE_TUNE:
				internal = PDM_SM_DELAY_300MS;
				break;
			default:
				xmc_err("not supportted pdm_sm_state\n");
				break;
			}
		}
		schedule_delayed_work(&pdm->main_sm_work, msecs_to_jiffies(internal));
	}
}

static void usbpd_pm_disconnect(struct pdm_chip *pdm)
{
	union power_supply_propval pval = {0, };

	cancel_delayed_work_sync(&pdm->main_sm_work);
	xmc_ops_charge_enable(g_chip->master_cp_dev, false);
	xmc_ops_charge_enable(g_chip->slave_cp_dev, false);
	xmc_ops_adc_enable(g_chip->master_cp_dev, false);
	xmc_ops_adc_enable(g_chip->slave_cp_dev, false);
	vote(g_chip->bbc_icl_votable, PDM_VOTER, false, 0);
	vote(g_chip->bbc_vinmin_votable, PDM_VOTER, false, 0);
	xmc_ops_powerpath_enable(g_chip->bbc_dev, true);

	pdm->psy_type = POWER_SUPPLY_TYPE_UNKNOWN;
	pdm->bypass_enable = false;
	pdm->pdo_bypass_support = false;
	pdm->disable_slave = false;
	pdm->low_ibus_count = 0;
	pdm->tune_vbus_count = 0;
	pdm->adapter_adjust_count = 0;
	pdm->enable_cp_count = 0;
	pdm->enable_cp_fail_count = 0;
	pdm->bms_i2c_error_count = 0;
	pdm->apdo_max_vbus = 0;
	pdm->apdo_min_vbus = 0;
	pdm->apdo_max_ibus = 0;
	pdm->apdo_max_watt = 0;
	pdm->final_step = 0;
	pdm->step_chg_fcc = 0;
	pdm->thermal_limit_fcc = 0;
	pdm->last_switch_time.tv_sec = 0;
	memset(&pdm->cap, 0, sizeof(struct xmc_pd_cap));

	pval.intval = 0;
//	power_supply_set_property(g_chip->usb_psy, POWER_SUPPLY_PROP_APDO_MAX, &pval);

	pdm_move_sm(pdm, PDM_STATE_ENTRY);
	pdm->last_state = PDM_STATE_ENTRY;
}

static void pdm_psy_change(struct work_struct *work)
{
	struct pdm_chip *pdm = container_of(work, struct pdm_chip, psy_change_work);

	xmc_info("[XMC_PDM] [pd_type pd_authen pdm_active] = [%d %d %d]\n", g_chip->usb_typec.pd_type, g_chip->adapter.authenticate_success, pdm->pdm_active);

	if (!pdm->pdm_active && g_chip->usb_typec.pd_type == XMC_PD_TYPE_PPS) {
		if (pdm_evaluate_src_caps(pdm)) {
			pdm->pdm_active = true;
			pdm_move_sm(pdm, PDM_STATE_ENTRY);
			schedule_delayed_work(&pdm->main_sm_work, 0);
		}
	} else if (pdm->pdm_active && g_chip->usb_typec.pd_type != XMC_PD_TYPE_PPS) {
		pdm->pdm_active = false;
		xmc_info("[XMC_PDM] cancel state machine\n");
		usbpd_pm_disconnect(pdm);
	}

	pdm->psy_change_running = false;
}

static int usbpdm_psy_notifier_cb(struct notifier_block *nb, unsigned long event, void *data)
{
	struct pdm_chip *pdm = container_of(nb, struct pdm_chip, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	spin_lock_irqsave(&pdm->psy_change_lock, flags);
	if (strcmp(psy->desc->name, "usb") == 0 && !pdm->psy_change_running) {
		pdm->psy_change_running = true;
		schedule_work(&pdm->psy_change_work);
	}
	spin_unlock_irqrestore(&pdm->psy_change_lock, flags);

	return NOTIFY_OK;
}

static bool pdm_parse_dt(struct pdm_chip *pdm)
{
	struct charge_chip *chip = container_of(pdm, struct charge_chip, pdm);
	struct device_node *node = chip->dev->of_node;
	struct device_node *pdm_node = NULL;
	bool ret = false;

	if (node)
		pdm_node = of_find_node_by_name(node, "pdm");

	if (!node || !pdm_node) {
		xmc_err("[XMC_PROBE] device tree node missing\n");
		return false;
	}

	ret |= of_property_read_u32(pdm_node, "fv_ffc", &pdm->dts_config.fv_ffc);
	ret |= of_property_read_u32(pdm_node, "fv", &pdm->dts_config.fv);
	ret |= of_property_read_u32(pdm_node, "max_fcc", &pdm->dts_config.max_fcc);
	ret |= of_property_read_u32(pdm_node, "max_vbus", &pdm->dts_config.max_vbus);
	ret |= of_property_read_u32(pdm_node, "max_ibus", &pdm->dts_config.max_ibus);
	ret |= of_property_read_u32(pdm_node, "fcc_low_hyst", &pdm->dts_config.fcc_low_hyst);
	ret |= of_property_read_u32(pdm_node, "fcc_high_hyst", &pdm->dts_config.fcc_high_hyst);
	ret |= of_property_read_u32(pdm_node, "low_tbat", &pdm->dts_config.low_tbat);
	ret |= of_property_read_u32(pdm_node, "high_tbat", &pdm->dts_config.high_tbat);
	ret |= of_property_read_u32(pdm_node, "high_vbat", &pdm->dts_config.high_vbat);
	ret |= of_property_read_u32(pdm_node, "high_soc", &pdm->dts_config.high_soc);
	ret |= of_property_read_u32(pdm_node, "cv_vbat", &pdm->dts_config.cv_vbat);
	ret |= of_property_read_u32(pdm_node, "cv_vbat_ffc", &pdm->dts_config.cv_vbat_ffc);
	ret |= of_property_read_u32(pdm_node, "cv_ibat", &pdm->dts_config.cv_ibat);

	ret |= of_property_read_u32(pdm_node, "vbus_low_gap_div", &vbus_low_gap_div);
	ret |= of_property_read_u32(pdm_node, "vbus_high_gap_div", &vbus_high_gap_div);
	ret |= of_property_read_u32(pdm_node, "min_pdo_vbus", &pdm->dts_config.min_pdo_vbus);
	ret |= of_property_read_u32(pdm_node, "max_pdo_vbus", &pdm->dts_config.max_pdo_vbus);
	ret |= of_property_read_u32(pdm_node, "max_bbc_vbus", &pdm->dts_config.max_bbc_vbus);
	ret |= of_property_read_u32(pdm_node, "min_bbc_vbus", &pdm->dts_config.min_bbc_vbus);

	xmc_info("[XMC_PROBE] parse config, FV = %d, FV_FFC = %d, FCC = [%d %d %d], MAX_VBUS = %d, MAX_IBUS = %d, CV = [%d %d %d], ENTRY = [%d %d %d %d], PDO_GAP = [%d %d %d %d %d %d]\n",
			pdm->dts_config.fv, pdm->dts_config.fv_ffc, pdm->dts_config.max_fcc, pdm->dts_config.fcc_low_hyst, pdm->dts_config.fcc_high_hyst,
			pdm->dts_config.max_vbus, pdm->dts_config.max_ibus, pdm->dts_config.cv_vbat, pdm->dts_config.cv_vbat_ffc, pdm->dts_config.cv_ibat,
			pdm->dts_config.low_tbat, pdm->dts_config.high_tbat, pdm->dts_config.high_vbat, pdm->dts_config.high_soc,
			vbus_low_gap_div, vbus_high_gap_div, pdm->dts_config.min_pdo_vbus, pdm->dts_config.max_pdo_vbus, pdm->dts_config.max_bbc_vbus, pdm->dts_config.min_bbc_vbus);

	pdm->vbus_control_gpio = of_get_named_gpio(pdm_node, "vbus_control_gpio", 0);
	if (gpio_is_valid(pdm->vbus_control_gpio))
		gpio_direction_output(pdm->vbus_control_gpio, 0);
	else
		xmc_info("[XMC_PROBE] invalid VBUS_control GPIO");

	return !ret;
}

bool xmc_pdm_init(struct charge_chip *chip)
{
	struct pdm_chip *pdm = &chip->pdm;
	g_chip = chip;

	if (!pdm_parse_dt(pdm)) {
		xmc_err("[XMC_PROBE] failed to parse DTSI\n");
		return false;
	}

	pdm->last_switch_time.tv_sec = 0;

	spin_lock_init(&pdm->psy_change_lock);
	INIT_WORK(&pdm->psy_change_work, pdm_psy_change);
	INIT_DELAYED_WORK(&pdm->main_sm_work, pdm_main_sm);
	pdm->nb.notifier_call = usbpdm_psy_notifier_cb;
	power_supply_reg_notifier(&pdm->nb);

	xmc_info("[XMC_PROBE] PDM init success");
	return true;
}
