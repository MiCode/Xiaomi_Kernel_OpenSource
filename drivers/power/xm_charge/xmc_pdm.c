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

#ifdef CONFIG_FACTORY_BUILD
static int pdm_vbus_low_gap = 950;
module_param_named(pdm_vbus_low_gap, pdm_vbus_low_gap, int, 0600);

static int pdm_vbus_high_gap = 1250;
module_param_named(pdm_vbus_high_gap, pdm_vbus_high_gap, int, 0600);
#else
static int pdm_vbus_low_gap = 600;
module_param_named(pdm_vbus_low_gap, pdm_vbus_low_gap, int, 0600);

static int pdm_vbus_high_gap = 850;
module_param_named(pdm_vbus_high_gap, pdm_vbus_high_gap, int, 0600);
#endif

static int pdm_ibus_gap = 450;
module_param_named(pdm_ibus_gap, pdm_ibus_gap, int, 0600);

static bool pdm_evaluate_src_caps(struct pdm_chip *pdm)
{
	int power_2t1 = 0, power_4t1 = 0, index_2t1 = 0, index_4t1 = 0;
	int i = 0;

	if (g_chip->adapter.cap.nr == 0) {
		xmc_info("[XMC_PDM] source PDO is invalid\n");
		return false;
	}

	for (i = 0; i < g_chip->adapter.cap.nr; i++) {
		if (g_chip->adapter.cap.type[i] != TCPM_POWER_CAP_VAL_TYPE_AUGMENT)
			continue;

		if (g_chip->adapter.cap.max_mv[i] >= 9000 && g_chip->adapter.cap.max_mv[i] < 18000) {
			if (g_chip->adapter.cap.maxwatt[i] > power_2t1) {
				power_2t1 = g_chip->adapter.cap.maxwatt[i];
				index_2t1 = i;
			}
		} else if (g_chip->adapter.cap.max_mv[i] >= 18000 && g_chip->adapter.cap.max_mv[i] <= 22000) {
			if (g_chip->adapter.cap.maxwatt[i] > power_4t1) {
				power_4t1 = g_chip->adapter.cap.maxwatt[i];
				index_4t1 = i;
			}
		}
	}

	if (power_2t1 == 0 && power_4t1 == 0) {
		pdm->select_index = 0;
		pdm->apdo_max_vbus = 0;
		pdm->apdo_min_vbus = 0;
		pdm->apdo_max_ibus = 0;
		pdm->apdo_max_watt = 0;
		pdm->div_rate = 0;
		return false;
	} else {
		if (power_4t1 >= power_2t1) {
			pdm->select_index = index_4t1;
			pdm->div_mode = XMC_CP_4T1;
			pdm->div_rate = 4;
		} else {
			pdm->select_index = index_2t1;
			pdm->div_mode = XMC_CP_2T1;
			pdm->div_rate = 2;
		}

		pdm->apdo_max_vbus = g_chip->adapter.cap.max_mv[pdm->select_index];
		pdm->apdo_min_vbus = g_chip->adapter.cap.min_mv[pdm->select_index];
		pdm->apdo_max_ibus = g_chip->adapter.cap.ma[pdm->select_index];
		pdm->apdo_max_watt = g_chip->adapter.cap.maxwatt[pdm->select_index];
		xmc_info("[XMC_PDM] select index = %d, max_vbus = %d, min_vbus = %d, max_ibus = %d, power = %d\n",
			pdm->select_index, pdm->apdo_max_vbus, pdm->apdo_min_vbus, pdm->apdo_max_ibus, pdm->apdo_max_watt);
		return true;
	}
}

static bool pdm_taper_charge(struct pdm_chip *pdm)
{
	int cv_vbat = 0;

	if (g_chip->charge_full)
		return true;

	cv_vbat = g_chip->ffc_enable ? pdm->dts_config.cv_vbat_ffc : pdm->dts_config.cv_vbat;
	if (pdm->vbat > cv_vbat && (-pdm->ibat) < pdm->dts_config.cv_ibat)
		pdm->taper_count++;
	else
		pdm->taper_count = 0;

	if (pdm->taper_count > MAX_TAPER_COUNT)
		return true;
	else
		return false;
}

static void pdm_reset_status(struct pdm_chip *pdm)
{
	pdm->div_rate = 0;
	pdm->master_cp_enable = false;
	pdm->master_cp_ibus = 0;
	pdm->master_cp_vbus = 0;
	pdm->slave_cp_enable = false;
	pdm->slave_cp_ibus = 0;
	pdm->slave_cp_vbus = 0;
	pdm->fv = 0;
	pdm->total_ibus = 0;
	pdm->low_ibus_count = 0;
	pdm->tune_vbus_count = 0;
	pdm->enable_cp_count = 0;
	pdm->enable_cp_fail_count = 0;
	pdm->apdo_max_vbus = 0;
	pdm->apdo_min_vbus = 0;
	pdm->apdo_max_ibus = 0;
	pdm->apdo_max_watt = 0;
	pdm->ibus_step = 0;
	pdm->vbus_step = 0;
	pdm->ibat_step = 0;
	pdm->vbat_step = 0;
	pdm->final_step = 0;
	pdm->request_current = 0;
	pdm->request_voltage = 0;
	pdm->step_chg_fcc = 0;
	pdm->thermal_limit_fcc = 0;
}

static void pdm_update_status(struct pdm_chip *pdm)
{
	union power_supply_propval val = {0,};

	xmc_ops_get_vbus(g_chip->master_cp_dev, &pdm->master_cp_vbus);
	xmc_ops_get_ibus(g_chip->master_cp_dev, &pdm->master_cp_ibus);
	xmc_ops_get_ibus(g_chip->slave_cp_dev, &pdm->slave_cp_ibus);
	xmc_ops_get_charge_enable(g_chip->master_cp_dev, &pdm->master_cp_enable);
	xmc_ops_get_charge_enable(g_chip->slave_cp_dev, &pdm->slave_cp_enable);
	xmc_ops_get_div_mode(g_chip->master_cp_dev, &pdm->master_cp_mode);
	xmc_ops_get_div_mode(g_chip->slave_cp_dev, &pdm->slave_cp_mode);

	pdm->total_ibus = pdm->master_cp_ibus + pdm->slave_cp_ibus;

	power_supply_get_property(g_chip->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	pdm->ibat = val.intval / 1000;

	power_supply_get_property(g_chip->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	pdm->vbat = val.intval / 1000;

	pdm->soc = g_chip->battery.uisoc;
	pdm->step_chg_fcc = get_client_vote(g_chip->bbc_fcc_votable, STEP_CHARGE_VOTER);
	pdm->thermal_limit_fcc = min(get_client_vote(g_chip->bbc_fcc_votable, THERMAL_VOTER), get_client_vote(g_chip->bbc_fcc_votable, JEITA_CHARGE_VOTER));
	pdm->target_fcc = get_effective_result(g_chip->bbc_fcc_votable);

	if (g_chip->step_cv)
		pdm->fv = g_chip->step_chg_cfg[g_chip->step_index[0]].low_threshold + g_chip->step_forward_hyst;
	else
		pdm->fv = (g_chip->ffc_enable ? pdm->dts_config.fv_ffc : pdm->dts_config.fv) - (g_chip->smart_fv_shift <= 100 ? g_chip->smart_fv_shift : 0) - 7;

	pdm->entry_vbus = min(min(((pdm->vbat * pdm->div_rate) + pdm_vbus_low_gap), pdm->dts_config.max_vbus), pdm->apdo_max_vbus);
	pdm->entry_ibus = min(min(((pdm->target_fcc / pdm->div_rate) + pdm_ibus_gap), pdm->dts_config.max_ibus), pdm->apdo_max_ibus);
	if (pdm->soc > 85)
		pdm->entry_vbus = min(min(pdm->entry_vbus - 360, pdm->dts_config.max_vbus), pdm->apdo_max_vbus);

	xmc_info("[XMC_PDM] BUS = [%d %d %d %d], CP = [%d %d %d %d %d], BAT = [%d %d %d], STEP = [%d %d %d %d %d], CC_CV = [%d %d %d %d]\n",
		pdm->master_cp_vbus, pdm->master_cp_ibus, pdm->slave_cp_ibus, pdm->total_ibus,
		pdm->master_cp_enable, pdm->slave_cp_enable, pdm->master_cp_mode, pdm->slave_cp_mode, pdm->div_mode,
		pdm->vbat, pdm->ibat, pdm->soc,
		pdm->vbat_step, pdm->ibat_step, pdm->vbus_step, pdm->ibus_step, pdm->final_step,
		pdm->target_fcc, pdm->thermal_limit_fcc, pdm->step_chg_fcc, pdm->fv);
}

static int pdm_tune_pdo(struct pdm_chip *pdm)
{
	int ibus_limit = 0, vbus_limit = 0, request_voltage = 0, request_current = 0, final_step = 0, ret = 0;

	pdm->ibat_step = pdm->vbat_step = pdm->ibus_step = pdm->vbus_step = 0;
	ibus_limit = min(min(((pdm->target_fcc / pdm->div_rate) + pdm_ibus_gap), pdm->dts_config.max_ibus), pdm->apdo_max_ibus);
	if (pdm->apdo_max_ibus <= 3000)
		ibus_limit = min(ibus_limit, pdm->apdo_max_ibus - 200);
	else if (pdm->apdo_max_ibus <= 6200)
		ibus_limit = min(ibus_limit, pdm->apdo_max_ibus - 300);

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

	if (pdm->fv - pdm->vbat > LARGE_VBAT_DIFF)
		pdm->vbat_step = LARGE_STEP;
	else if (pdm->fv - pdm->vbat > MEDIUM_VBAT_DIFF)
		pdm->vbat_step = MEDIUM_STEP;
	else if (pdm->fv - pdm->vbat > 5)
		pdm->vbat_step = SMALL_STEP;
	else if (pdm->fv - pdm->vbat < -2)
		pdm->vbat_step = -MEDIUM_STEP;
	else if (pdm->fv - pdm->vbat < 0)
		pdm->vbat_step = -SMALL_STEP;

	if (ibus_limit - pdm->total_ibus > LARGE_IBUS_DIFF)
		pdm->ibus_step = LARGE_STEP;
	else if (ibus_limit - pdm->total_ibus > MEDIUM_IBUS_DIFF)
		pdm->ibus_step = MEDIUM_STEP;
	else if (ibus_limit - pdm->total_ibus > 300)
		pdm->ibus_step = SMALL_STEP;
	else if (ibus_limit - pdm->total_ibus > 0)
		pdm->ibus_step = 0;
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
	if (pdm->step_chg_fcc != pdm->dts_config.max_fcc || g_chip->step_cv) {
		if ((pdm->final_step == SMALL_STEP && final_step == SMALL_STEP) || (pdm->final_step == -SMALL_STEP && final_step == -SMALL_STEP))
			final_step = 0;
	}

	pdm->final_step = final_step;
	if (pdm->soc > 85)
		pdm->final_step = cut_cap(pdm->final_step, -3, 3);

	if (pdm->final_step) {
		request_voltage = min(pdm->request_voltage + pdm->final_step * STEP_MV, vbus_limit);
		request_current = ibus_limit;
		ret = xmc_ops_set_cap(g_chip->adapter_dev, XMC_PDO_APDO, request_voltage, request_current);
		if (ret == TCPM_SUCCESS) {
			if (g_chip->step_index[0] == STEP_JEITA_TUPLE_NUM - 1)
				msleep(PDM_SM_DELAY_500MS);
			else
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
	else if (pdm->state == PDM_STATE_TUNE && (!pdm->master_cp_enable || !pdm->slave_cp_enable)) {
		xmc_ops_dump_register(g_chip->master_cp_dev);
		xmc_ops_dump_register(g_chip->slave_cp_dev);
		return PDM_STATUS_HOLD;
	} else if (g_chip->usb_typec.cmd_input_suspend || g_chip->usb_typec.burn_detect || g_chip->battery.connector_remove || (g_chip->night_charging && g_chip->battery.uisoc >= 80))
		return PDM_STATUS_HOLD;
	else if ((pdm->thermal_limit_fcc > 0 && pdm->thermal_limit_fcc < MIN_PDM_FCC) || g_chip->jeita_index[0] == 0 || g_chip->jeita_index[0] == 6)
		return PDM_STATUS_HOLD;
	else if (pdm->state == PDM_STATE_ENTRY && pdm->soc > pdm->dts_config.high_soc)
		return PDM_STATUS_EXIT;
	else
		return PDM_STATUS_CONTINUE;
}

static void pdm_move_sm(struct pdm_chip *pdm, enum pdm_sm_state state)
{
	xmc_info("[XMC_PDM] state change:%s -> %s\n", state_name[pdm->state], state_name[state]);
	pdm->state = state;
	pdm->no_delay = true;
}

static bool pdm_handle_sm(struct pdm_chip *pdm)
{
	int ret = 0;

	switch (pdm->state) {
	case PDM_STATE_ENTRY:
		pdm->sm_status = pdm_check_condition(pdm);
		if (pdm->sm_status == PDM_STATUS_EXIT) {
			xmc_info("[XMC_PDM] PDM_STATUS_EXIT, don't start sm\n");
			return true;
		} else if (pdm->sm_status == PDM_STATUS_HOLD) {
			break;
		} else {
			pdm_move_sm(pdm, PDM_STATE_INIT_VBUS);
			if (pdm->target_fcc > 6000 && pdm->div_mode == XMC_CP_4T1) {
				xmc_ops_powerpath_enable(g_chip->bbc_dev, false);
				xmc_ops_adc_enable(g_chip->master_cp_dev, true);
				xmc_ops_adc_enable(g_chip->slave_cp_dev, true);
				xmc_ops_device_init(g_chip->master_cp_dev, pdm->div_mode);
				xmc_ops_device_init(g_chip->slave_cp_dev, pdm->div_mode);
				xmc_ops_set_div_mode(g_chip->master_cp_dev, pdm->div_mode);
				xmc_ops_set_div_mode(g_chip->slave_cp_dev, pdm->div_mode);
				pdm->div_rate = 4;
			} else {
			    pdm->div_mode = XMC_CP_2T1;
				pdm->div_rate = 2;
				xmc_ops_powerpath_enable(g_chip->bbc_dev, false);
				xmc_ops_adc_enable(g_chip->master_cp_dev, true);
				xmc_ops_adc_enable(g_chip->slave_cp_dev, true);
				xmc_ops_device_init(g_chip->master_cp_dev, pdm->div_mode);
				xmc_ops_device_init(g_chip->slave_cp_dev, pdm->div_mode);
				xmc_ops_set_div_mode(g_chip->master_cp_dev, pdm->div_mode);
				xmc_ops_set_div_mode(g_chip->slave_cp_dev, pdm->div_mode);
			}
			xmc_info("[XMC_PDM] div mode = %d\n", pdm->div_mode);
		}
		break;
	case PDM_STATE_INIT_VBUS:
		pdm->tune_vbus_count++;
		if (pdm->tune_vbus_count == 1) {
			if (gpio_is_valid(pdm->vbus_control_gpio)) {
				gpio_set_value(pdm->vbus_control_gpio, 1);
				g_chip->bbc.vbus_disable = true;
			}
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
		}

		if (pdm->master_cp_vbus <= (pdm->vbat * pdm->div_rate + pdm_vbus_low_gap - 70)) {
			pdm->request_voltage += 2 * STEP_MV;
		} else if (pdm->master_cp_vbus >= pdm->vbat * pdm->div_rate + pdm_vbus_high_gap + 70) {
			pdm->request_voltage -= 2 * STEP_MV;
		} else {
			xmc_info("[XMC_PDM] success to tune VBUS to target window\n");
			pdm_move_sm(pdm, PDM_STATE_ENABLE_CP);
			break;
		}

		xmc_ops_set_cap(g_chip->adapter_dev, XMC_PDO_APDO, pdm->request_voltage, pdm->request_current);
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
		if (!pdm->slave_cp_enable)
			xmc_ops_charge_enable(g_chip->slave_cp_dev, true);
		if (pdm->master_cp_mode != pdm->div_mode) {
			xmc_ops_device_init(g_chip->master_cp_dev, pdm->div_mode);
			xmc_ops_set_div_mode(g_chip->master_cp_dev, pdm->div_mode);
		}
		if (pdm->slave_cp_mode != pdm->div_mode) {
			xmc_ops_device_init(g_chip->slave_cp_dev, pdm->div_mode);
			xmc_ops_set_div_mode(g_chip->slave_cp_dev, pdm->div_mode);
		}

		if (pdm->master_cp_enable && pdm->slave_cp_enable && pdm->master_cp_mode == pdm->div_mode && pdm->slave_cp_mode == pdm->div_mode) {
			xmc_info("success to enable charge pump\n");
			pdm_move_sm(pdm, PDM_STATE_TUNE);
		} else {
			if (pdm->enable_cp_count > 1) {
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
		break;
	case PDM_STATE_EXIT:
		pdm_reset_status(pdm);

		xmc_ops_charge_enable(g_chip->master_cp_dev, false);
		xmc_ops_charge_enable(g_chip->slave_cp_dev, false);
		xmc_ops_adc_enable(g_chip->master_cp_dev, false);
		xmc_ops_adc_enable(g_chip->slave_cp_dev, false);
		xmc_ops_set_cap(g_chip->adapter_dev, XMC_PDO_APDO, 5000, 3000);
		if (!g_chip->charge_full) {
			xmc_ops_powerpath_enable(g_chip->bbc_dev, true);
		} else {
			msleep(200);
			xmc_ops_charge_enable(g_chip->bbc_dev, false);
		}

		if (gpio_is_valid(pdm->vbus_control_gpio)) {
			gpio_set_value(pdm->vbus_control_gpio, 0);
			g_chip->bbc.vbus_disable = false;
		}

		msleep(100);

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

	if (!pdm_handle_sm(pdm) && pdm->pdm_active) {
		if (pdm->no_delay) {
			internal = 0;
			pdm->no_delay = false;
		} else {
			switch (pdm->state) {
			case PDM_STATE_ENTRY:
				internal = PDM_SM_DELAY_500MS;
				break;
			case PDM_STATE_EXIT:
			case PDM_STATE_INIT_VBUS:
				internal = PDM_SM_DELAY_200MS;
				break;
			case PDM_STATE_ENABLE_CP:
				internal = PDM_SM_DELAY_200MS;
				break;
			case PDM_STATE_TUNE:
				internal = PDM_SM_DELAY_500MS;
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
	pdm_reset_status(pdm);

	cancel_delayed_work_sync(&pdm->main_sm_work);
	xmc_ops_charge_enable(g_chip->master_cp_dev, false);
	xmc_ops_charge_enable(g_chip->slave_cp_dev, false);
	xmc_ops_adc_enable(g_chip->master_cp_dev, false);
	xmc_ops_adc_enable(g_chip->slave_cp_dev, false);

	if (gpio_is_valid(pdm->vbus_control_gpio)) {
		gpio_set_value(pdm->vbus_control_gpio, 0);
		g_chip->bbc.vbus_disable = false;
	}

	if (!g_chip->charge_full)
		xmc_ops_powerpath_enable(g_chip->bbc_dev, true);

	pdm_move_sm(pdm, PDM_STATE_ENTRY);
}

static void pdm_psy_change(struct work_struct *work)
{
	struct pdm_chip *pdm = container_of(work, struct pdm_chip, psy_change_work);

	xmc_info("[XMC_PDM] [pd_type auth_done pdm_active] = [%d %d %d]\n", g_chip->usb_typec.pd_type, g_chip->adapter.authenticate_done, pdm->pdm_active);

	if (!pdm->pdm_active && g_chip->usb_typec.pd_type == XMC_PD_TYPE_PPS && g_chip->adapter.authenticate_done) {
		if (pdm_evaluate_src_caps(pdm)) {
			pdm->pdm_active = true;
			pdm_move_sm(pdm, PDM_STATE_ENTRY);
			schedule_delayed_work(&pdm->main_sm_work, PDM_SM_DELAY_200MS);
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

	xmc_info("[XMC_PROBE] parse config, FV = %d, FV_FFC = %d, FCC = [%d %d %d], MAX_VBUS = %d, MAX_IBUS = %d, CV = [%d %d %d], ENTRY = [%d %d %d %d]\n",
			pdm->dts_config.fv, pdm->dts_config.fv_ffc, pdm->dts_config.max_fcc, pdm->dts_config.fcc_low_hyst, pdm->dts_config.fcc_high_hyst,
			pdm->dts_config.max_vbus, pdm->dts_config.max_ibus, pdm->dts_config.cv_vbat, pdm->dts_config.cv_vbat_ffc, pdm->dts_config.cv_ibat,
			pdm->dts_config.low_tbat, pdm->dts_config.high_tbat, pdm->dts_config.high_vbat, pdm->dts_config.high_soc);

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

	spin_lock_init(&pdm->psy_change_lock);
	INIT_WORK(&pdm->psy_change_work, pdm_psy_change);
	INIT_DELAYED_WORK(&pdm->main_sm_work, pdm_main_sm);
	pdm->nb.notifier_call = usbpdm_psy_notifier_cb;
	power_supply_reg_notifier(&pdm->nb);

	xmc_info("[XMC_PROBE] PDM init success");
	return true;
}
