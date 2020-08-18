/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include <mt-plat/mtk_boot.h>
#include "mtk_charger_intf.h"
#include "mtk_dual_switch_charging.h"

static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

static bool is_in_pe40_state(struct charger_manager *info)
{
	struct dual_switch_charging_alg_data *swchgalg = info->algorithm_data;

	if (swchgalg->state == CHR_PE40_CC || swchgalg->state == CHR_PE40_TUNING
	    || swchgalg->state == CHR_PE40_POSTCC
	    || swchgalg->state == CHR_PE40_INIT)
		return true;
	return false;
}

static void _disable_all_charging(struct charger_manager *info)
{
	bool chg2_chip_enabled = false;

	charger_dev_is_chip_enabled(info->chg2_dev, &chg2_chip_enabled);
	charger_dev_enable(info->chg1_dev, false);
	if (chg2_chip_enabled) {
		charger_dev_enable(info->chg2_dev, false);
		charger_dev_enable_chip(info->chg2_dev, false);
	}

	if (mtk_pe20_get_is_enable(info)) {
		mtk_pe20_set_is_enable(info, false);
		if (mtk_pe20_get_is_connect(info))
			mtk_pe20_reset_ta_vchr(info);
	}

	if (mtk_pe_get_is_enable(info)) {
		mtk_pe_set_is_enable(info, false);
		if (mtk_pe_get_is_connect(info))
			mtk_pe_reset_ta_vchr(info);
	}

	if (mtk_pe40_get_is_enable(info)) {
		if (mtk_pe40_get_is_connect(info))
			mtk_pe40_end(info, 3, true);
	}

	if (mtk_pdc_check_charger(info))
		mtk_pdc_reset(info);
}

static bool dual_swchg_check_pd_leave(struct charger_manager *info)
{
	struct mtk_pdc *pd = &info->pdc;
	int ichg = 0;

	if (info->disable_pd_dual)
		return true;

	if (pd->pd_cap_max_watt < 10000000)
		return true;

	if (info->enable_hv_charging == false)
		return true;

	ichg = battery_get_bat_current() * 100;
	if (battery_get_soc() >= info->data.pd_stop_battery_soc ||
		battery_get_uisoc() == -1)
		return true;

	return false;
}

static void
dual_swchg_select_charging_current_limit(struct charger_manager *info)
{
	struct charger_data *pdata, *pdata2;
	struct dual_switch_charging_alg_data *swchgalg = info->algorithm_data;
	u32 ichg1_min = 0, ichg2_min = 0, aicr1_min = 0, aicr2_min = 0;
	int ret = 0;
	bool chg2_chip_enabled = false;
	bool chg2_enabled = false;

	charger_dev_is_chip_enabled(info->chg2_dev, &chg2_chip_enabled);
	charger_dev_is_enabled(info->chg2_dev, &chg2_enabled);

	pdata = &info->chg1_data;
	pdata2 = &info->chg2_data;

	mutex_lock(&swchgalg->ichg_aicr_access_mutex);

	/* AICL */
	if (!mtk_pe20_get_is_connect(info) && !mtk_pe_get_is_connect(info) &&
	    !mtk_is_TA_support_pd_pps(info) && !mtk_pdc_check_charger(info)) {
		charger_dev_run_aicl(info->chg1_dev,
				&pdata->input_current_limit_by_aicl);
		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
				info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
					info->data.max_dmivr_charger_current;
		}
	}

	if (pdata->force_charging_current > 0) {

		pdata->charging_current_limit = pdata->force_charging_current;
		if (pdata->force_charging_current <= 450000)
			pdata->input_current_limit = 500000;
		else
			pdata->input_current_limit =
					info->data.ac_charger_input_current;

		goto done;
	}

	if (info->usb_unlimited) {
		pdata->input_current_limit =
					info->data.ac_charger_input_current;
		pdata->charging_current_limit = info->data.ac_charger_current;
		goto done;
	}

	if (info->water_detected) {
		pdata->input_current_limit = info->data.usb_charger_current;
		pdata->charging_current_limit = info->data.usb_charger_current;
		goto done;
	}

	if ((get_boot_mode() == META_BOOT) ||
	    (get_boot_mode() == ADVMETA_BOOT)) {
		pdata->input_current_limit = 200000; /* 200mA */
		goto done;
	}

	if (info->atm_enabled == true && (info->chr_type == STANDARD_HOST ||
	    info->chr_type == CHARGING_HOST)) {
		pdata->input_current_limit = 100000; /* 100mA */
		goto done;
	}

	if (mtk_pe40_get_is_connect(info)) {
		if (is_dual_charger_supported(info)) {
			/* Slave charger may not have input current control */
			pdata->input_current_limit =
				info->data.pe40_dual_charger_input_current;
			pdata2->input_current_limit =
				info->data.pe40_dual_charger_input_current;

			switch (swchgalg->state) {
			case CHR_PE40_INIT:
			case CHR_PE40_CC:
				pdata->charging_current_limit =
				info->data.pe40_dual_charger_chg1_current;
				pdata2->charging_current_limit
				= info->data.pe40_dual_charger_chg2_current;
				break;
			case CHR_PE40_TUNING:
				pdata->charging_current_limit
				= info->data.pe40_dual_charger_chg1_current;
				break;
			default:
				break;
			}

		} else {
			pdata->input_current_limit =
				info->data.pe40_single_charger_input_current;
			pdata->charging_current_limit =
				info->data.pe40_single_charger_current;
		}
	} else if (is_typec_adapter(info)) {
		if (adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL)
			== 3000) {
			pdata->input_current_limit = 3000000;
			pdata->charging_current_limit = 3000000;
		} else if (adapter_dev_get_property(info->pd_adapter,
			TYPEC_RP_LEVEL) == 1500) {
			pdata->input_current_limit = 1500000;
			pdata->charging_current_limit = 2000000;
		} else {
			chr_err("type-C: inquire rp error\n");
			pdata->input_current_limit = 500000;
			pdata->charging_current_limit = 500000;
		}

		chr_err("type-C:%d current:%d\n",
			info->pd_type,
			adapter_dev_get_property(info->pd_adapter,
				TYPEC_RP_LEVEL));
	} else if (mtk_pdc_check_charger(info)) {
		int vbus = 0, cur = 0, idx = 0;

		ret = mtk_pdc_get_setting(info, &vbus, &cur, &idx);
		if (ret != -1 && idx != -1) {
			pdata->input_current_limit = cur * 1000;
			pdata->charging_current_limit =
				info->data.pd_charger_current;
			mtk_pdc_setup(info, idx);
		} else {
			pdata->input_current_limit =
				info->data.usb_charger_current_configured;
			pdata->charging_current_limit =
				info->data.usb_charger_current_configured;
		}

		if (!dual_swchg_check_pd_leave(info)) {
			/* Slave charger may not have input current control */
			pdata2->input_current_limit = cur * 1000;

			switch (swchgalg->state) {
			case CHR_CC:
				pdata->charging_current_limit
					= info->data.chg1_ta_ac_charger_current;
				pdata2->charging_current_limit
					= info->data.chg2_ta_ac_charger_current;
				break;
			case CHR_TUNING:
				pdata->charging_current_limit
					= info->data.chg1_ta_ac_charger_current;
				break;
			default:
				break;
			}
		}

		chr_info("[%s]vbus:%d input_cur:%d idx:%d current:%d\n",
			__func__, vbus, cur, idx,
			info->data.pd_charger_current);

	} else if (info->chr_type == STANDARD_HOST) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)) {
			if (info->usb_state == USB_SUSPEND)
				pdata->input_current_limit =
					info->data.usb_charger_current_suspend;
			else if (info->usb_state == USB_UNCONFIGURED)
				pdata->input_current_limit =
				info->data.usb_charger_current_unconfigured;
			else if (info->usb_state == USB_CONFIGURED)
				pdata->input_current_limit =
				info->data.usb_charger_current_configured;
			else
				pdata->input_current_limit =
				info->data.usb_charger_current_unconfigured;

			pdata->charging_current_limit =
						pdata->input_current_limit;
		} else {
			pdata->input_current_limit =
						info->data.usb_charger_current;
			/* it can be larger */
			pdata->charging_current_limit =
						info->data.usb_charger_current;
		}
	} else if (info->chr_type == NONSTANDARD_CHARGER) {
		pdata->input_current_limit =
					info->data.non_std_ac_charger_current;
		pdata->charging_current_limit =
					info->data.non_std_ac_charger_current;
	} else if (info->chr_type == STANDARD_CHARGER) {
		pdata->input_current_limit =
					info->data.ac_charger_input_current;
		pdata->charging_current_limit =
					info->data.ac_charger_current;
		mtk_pe20_set_charging_current(info,
					&pdata->charging_current_limit,
					&pdata->input_current_limit);
		mtk_pe_set_charging_current(info,
					&pdata->charging_current_limit,
					&pdata->input_current_limit);

		/* Only enable slave charger when PE+/PE+2.0 is connected */
		if ((mtk_pe20_get_is_enable(info) &&
		    mtk_pe20_get_is_connect(info))
		    || (mtk_pe_get_is_enable(info) &&
		    mtk_pe_get_is_connect(info))) {

			/* Slave charger may not have input current control */
			pdata2->input_current_limit
					= info->data.ac_charger_input_current;

			switch (swchgalg->state) {
			case CHR_CC:
				pdata->charging_current_limit
					= info->data.chg1_ta_ac_charger_current;
				pdata2->charging_current_limit
					= info->data.chg2_ta_ac_charger_current;
				break;
			case CHR_TUNING:
				pdata->charging_current_limit
					= info->data.chg1_ta_ac_charger_current;
				break;
			default:
				break;
			}
		}

	} else if (info->chr_type == CHARGING_HOST) {
		pdata->input_current_limit =
				info->data.charging_host_charger_current;
		pdata->charging_current_limit =
				info->data.charging_host_charger_current;
	} else if (info->chr_type == APPLE_1_0A_CHARGER) {
		pdata->input_current_limit =
				info->data.apple_1_0a_charger_current;
		pdata->charging_current_limit =
				info->data.apple_1_0a_charger_current;
	} else if (info->chr_type == APPLE_2_1A_CHARGER) {
		pdata->input_current_limit =
				info->data.apple_2_1a_charger_current;
		pdata->charging_current_limit =
				info->data.apple_2_1a_charger_current;
	}

	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
		    && info->chr_type == STANDARD_HOST)
			pr_debug("USBIF & STAND_HOST skip current check\n");
		else {
			if (info->sw_jeita.sm == TEMP_T0_TO_T1) {
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 350000;
			}
		}
	}

	/*
	 * If thermal current limit is less than charging IC's minimum
	 * current setting, disable the charger by setting its current
	 * setting to 0.
	 */

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
		    pdata->charging_current_limit)
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
		ret = charger_dev_get_min_charging_current(info->chg1_dev,
							&ichg1_min);
		if (ret != -ENOTSUPP &&
		    pdata->thermal_charging_current_limit < ichg1_min)
			pdata->charging_current_limit = 0;
	}

	if (pdata2->thermal_charging_current_limit != -1) {
		if (pdata2->thermal_charging_current_limit <
		    pdata2->charging_current_limit)
			pdata2->charging_current_limit =
				pdata2->thermal_charging_current_limit;

		ret = charger_dev_get_min_charging_current(info->chg2_dev,
							&ichg2_min);
		if (ret != -ENOTSUPP &&
		    pdata2->thermal_charging_current_limit < ichg2_min)
			pdata2->charging_current_limit = 0;
	}

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;

		ret = charger_dev_get_min_input_current(info->chg1_dev,
							&aicr1_min);
		if (ret != -ENOTSUPP &&
		    pdata->thermal_input_current_limit < aicr1_min)
			pdata->input_current_limit = 0;
	}

	if (pdata2->thermal_input_current_limit != -1) {
		if (pdata2->thermal_input_current_limit <
		    pdata2->input_current_limit)
			pdata2->input_current_limit =
					pdata2->thermal_input_current_limit;

		ret = charger_dev_get_min_input_current(info->chg2_dev,
							&aicr2_min);
		if (ret != -ENOTSUPP &&
		    pdata2->thermal_input_current_limit < aicr2_min)
			pdata2->input_current_limit = 0;
	}

	if (mtk_pe40_get_is_connect(info)) {
		if (info->pe4.pe4_input_current_limit != -1 &&
		    info->pe4.pe4_input_current_limit <
		    pdata->input_current_limit) {
			pdata->input_current_limit =
				info->pe4.pe4_input_current_limit;
			if (info->data.parallel_vbus)
				pdata2->input_current_limit =
				info->pe4.pe4_input_current_limit;
		}

		info->pe4.input_current_limit = pdata->input_current_limit;

		if (info->pe4.pe4_input_current_limit_setting != -1 &&
		    info->pe4.pe4_input_current_limit_setting <
		    pdata->input_current_limit) {
			pdata->input_current_limit =
				info->pe4.pe4_input_current_limit_setting;
			if (info->data.parallel_vbus)
				pdata2->input_current_limit =
				info->pe4.pe4_input_current_limit_setting;
		}
	}

	if (pdata->input_current_limit_by_aicl != -1 &&
	    !mtk_pe20_get_is_connect(info) && !mtk_pe_get_is_connect(info) &&
	    !mtk_is_TA_support_pd_pps(info)) {
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->input_current_limit_by_aicl;
	}

done:
	if (info->data.parallel_vbus) {
		pdata->input_current_limit = pdata->input_current_limit / 2;
		pdata2->input_current_limit = pdata2->input_current_limit / 2;
	}

	pr_notice("force:%d %d thermal:(%d %d,%d %d)(%d %d %d)setting:(%d %d)(%d %d)",
		_uA_to_mA(pdata->force_charging_current),
		_uA_to_mA(pdata2->force_charging_current),
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata2->thermal_input_current_limit),
		_uA_to_mA(pdata2->thermal_charging_current_limit),
		_uA_to_mA(info->pe4.pe4_input_current_limit),
		_uA_to_mA(info->pe4.pe4_input_current_limit_setting),
		_uA_to_mA(info->pe4.input_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		_uA_to_mA(pdata2->input_current_limit),
		_uA_to_mA(pdata2->charging_current_limit));

	pr_notice("type:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d parallel:%d\n",
		info->chr_type, info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		_uA_to_mA(pdata->input_current_limit_by_aicl),
		info->atm_enabled, info->data.parallel_vbus);

	charger_dev_set_input_current(info->chg1_dev,
					pdata->input_current_limit);
	charger_dev_set_charging_current(info->chg1_dev,
					pdata->charging_current_limit);

	if ((mtk_pe20_get_is_enable(info) && mtk_pe20_get_is_connect(info))
	    || (mtk_pe_get_is_enable(info) && mtk_pe_get_is_connect(info))
	    || mtk_pe40_get_is_connect(info)
	    || (mtk_pdc_check_charger(info) &&
		!dual_swchg_check_pd_leave(info))) {
		if (chg2_chip_enabled) {
			charger_dev_set_input_current(info->chg2_dev,
				pdata2->input_current_limit);
			charger_dev_set_charging_current(info->chg2_dev,
				pdata2->charging_current_limit);
		}
	}

	charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);

	/*
	 * If thermal current limit is larger than charging IC's minimum
	 * current setting, enable the charger immediately
	 */
	if (pdata->input_current_limit > aicr1_min
	    && pdata->charging_current_limit > ichg1_min
	    && info->can_charging)
		charger_dev_enable(info->chg1_dev, true);

	if (pdata->thermal_input_current_limit == -1 &&
	    pdata->thermal_charging_current_limit == -1 &&
	    pdata2->thermal_input_current_limit == -1 &&
	    pdata2->thermal_charging_current_limit == -1) {
		if (!mtk_pe20_get_is_enable(info) && info->can_charging) {
			swchgalg->state = CHR_CC;
			mtk_pe20_set_is_enable(info, true);
			mtk_pe20_set_to_check_chr_type(info, true);
		}

		if (!mtk_pe_get_is_enable(info) && info->can_charging) {
			swchgalg->state = CHR_CC;
			mtk_pe_set_is_enable(info, true);
			mtk_pe_set_to_check_chr_type(info, true);
		}
	}

	mutex_unlock(&swchgalg->ichg_aicr_access_mutex);
}

static void swchg_select_cv(struct charger_manager *info)
{
	u32 constant_voltage;
	bool chg2_chip_enabled = false;

	charger_dev_is_chip_enabled(info->chg2_dev, &chg2_chip_enabled);

	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0) {
			charger_dev_set_constant_voltage(info->chg1_dev,
							info->sw_jeita.cv);
			return;
		}

	/* dynamic cv*/
	constant_voltage = info->data.battery_cv;
	mtk_get_dynamic_cv(info, &constant_voltage);

	charger_dev_set_constant_voltage(info->chg1_dev, constant_voltage);
	/* Set slave charger's CV to 200mV higher than master's */
	if (chg2_chip_enabled)
		charger_dev_set_constant_voltage(info->chg2_dev,
			constant_voltage + 200000);
}

static void dual_swchg_turn_on_charging(struct charger_manager *info)
{
	struct dual_switch_charging_alg_data *swchgalg = info->algorithm_data;
	bool chg1_enable = true;
	bool chg2_enable = true;
	bool chg2_chip_enabled = false;

	charger_dev_is_chip_enabled(info->chg2_dev, &chg2_chip_enabled);

	if (is_dual_charger_supported(info) == false)
		chg2_enable = false;

	if (swchgalg->state == CHR_ERROR) {
		chg1_enable = false;
		chg2_enable = false;
		pr_notice("Charging Error, disable charging!\n");
	} else if ((get_boot_mode() == META_BOOT) ||
		   (get_boot_mode() == ADVMETA_BOOT)) {
		chg1_enable = false;
		chg2_enable = false;
		pr_notice("In meta mode, disable charging\n");
	} else {
		mtk_pe20_start_algorithm(info);
		if (mtk_pe20_get_is_connect(info) == false)
			mtk_pe_start_algorithm(info);

		dual_swchg_select_charging_current_limit(info);
		if (info->chg1_data.input_current_limit == 0
		    || info->chg1_data.charging_current_limit == 0) {
			chg1_enable = false;
			chg2_enable = false;
			pr_notice("chg1's aicr is set to 0mA, turn off\n");
		}

		if ((mtk_pe20_get_is_enable(info) &&
		    mtk_pe20_get_is_connect(info))
		    || (mtk_pe_get_is_enable(info) &&
		    mtk_pe_get_is_connect(info))
		    || mtk_pe40_get_is_connect(info)
		    || (mtk_pdc_check_charger(info) &&
		    !dual_swchg_check_pd_leave(info))) {
			if (info->chg2_data.input_current_limit == 0 ||
			    info->chg2_data.charging_current_limit == 0) {
				chg2_enable = false;
				pr_notice("chg2's aicr is 0mA, turn off\n");
			}
		}
		if (chg1_enable)
			swchg_select_cv(info);
	}

	charger_dev_enable(info->chg1_dev, chg1_enable);

	if (chg2_enable == true) {
		if ((mtk_pe20_get_is_enable(info) &&
		    mtk_pe20_get_is_connect(info))
		    || (mtk_pe_get_is_enable(info) &&
		    mtk_pe_get_is_connect(info))
		    || mtk_pe40_get_is_connect(info)
		    || (mtk_pdc_check_charger(info) &&
		    !dual_swchg_check_pd_leave(info))) {
			if (!chg2_chip_enabled)
				charger_dev_enable_chip(info->chg2_dev, true);
			if (swchgalg->state != CHR_POSTCC &&
			    swchgalg->state != CHR_PE40_POSTCC) {
				charger_dev_enable(info->chg2_dev, true);
				charger_dev_set_eoc_current(info->chg1_dev,
						info->data.dual_polling_ieoc);
				charger_dev_enable_termination(info->chg1_dev,
								false);
			} else {
				charger_dev_set_eoc_current(info->chg1_dev,
								150000);
				if (mtk_pe40_get_is_connect(info) == false)
					charger_dev_enable_termination(
							info->chg1_dev, true);
			}
		} else {
			if (chg2_chip_enabled) {
				charger_dev_enable(info->chg2_dev, false);
				charger_dev_enable_chip(info->chg2_dev, false);
			}
			charger_dev_set_eoc_current(info->chg1_dev, 150000);
			charger_dev_enable_termination(info->chg1_dev, true);
		}
	} else {
		if (chg2_chip_enabled) {
			charger_dev_enable(info->chg2_dev, false);
			charger_dev_enable_chip(info->chg2_dev, false);
		}
	}

	/* If chg1 or chg2 is disabled, leave PE+/PE+20 charging */
	if (chg1_enable == false || chg2_enable == false) {
		if (mtk_pe20_get_is_enable(info)) {
			mtk_pe20_set_is_enable(info, false);
			if (mtk_pe20_get_is_connect(info))
				mtk_pe20_reset_ta_vchr(info);
		}

		if (mtk_pe_get_is_enable(info)) {
			mtk_pe_set_is_enable(info, false);
			if (mtk_pe_get_is_connect(info))
				mtk_pe_reset_ta_vchr(info);
		}
	}

	charger_dev_is_enabled(info->chg2_dev, &chg2_enable);
	charger_dev_is_chip_enabled(info->chg2_dev, &chg2_chip_enabled);

	if (info->data.parallel_vbus) {
		if (!chg2_enable) {
			charger_dev_set_input_current(info->chg1_dev,
				info->chg1_data.input_current_limit * 2);
		}
	}

	chr_err("chg1:%d chg2:%d chg2_chip_en:%d\n", chg1_enable, chg2_enable,
		chg2_chip_enabled);
}

static int mtk_dual_switch_charging_plug_in(struct charger_manager *info)
{
	struct dual_switch_charging_alg_data *swchgalg = info->algorithm_data;

	swchgalg->state = CHR_CC;
	info->polling_interval = CHARGING_INTERVAL;
	swchgalg->disable_charging = false;

	return 0;
}

static int mtk_dual_switch_charging_plug_out(struct charger_manager *info)
{
	mtk_pe20_set_is_cable_out_occur(info, true);
	mtk_pe_set_is_cable_out_occur(info, true);
	mtk_pdc_plugout(info);
	mtk_pe40_plugout_reset(info);
	/* charger_dev_enable(info->chg2_dev, false); */
	charger_dev_enable_chip(info->chg2_dev, false);

	return 0;
}

static int mtk_dual_switch_charging_do_charging(struct charger_manager *info,
						bool en)
{
	struct dual_switch_charging_alg_data *swchgalg = info->algorithm_data;

	pr_info("[%s] en:%d %s\n", __func__, en, info->algorithm_name);
	if (en) {
		swchgalg->disable_charging = false;
		swchgalg->state = CHR_CC;
		charger_manager_notifier(info, CHARGER_NOTIFY_NORMAL);
		mtk_pe40_set_is_enable(info, en);
	} else {
		/* disable charging might change state, so call it first */
		_disable_all_charging(info);
		swchgalg->disable_charging = true;
		swchgalg->state = CHR_ERROR;
		charger_manager_notifier(info, CHARGER_NOTIFY_ERROR);
	}

	return 0;
}

static int mtk_dual_switch_chr_pe40_init(struct charger_manager *info)
{
	dual_swchg_turn_on_charging(info);
	return mtk_pe40_init_state(info);
}

static int mtk_dual_switch_chr_pe40_cc(struct charger_manager *info)
{
	struct dual_switch_charging_alg_data *swchgalg = info->algorithm_data;
	bool chg2_en = false;
	struct charger_data *pdata = &info->chg1_data;

	dual_swchg_turn_on_charging(info);
	charger_dev_is_enabled(info->chg2_dev, &chg2_en);

	/* Check whether eoc condition is met */
	if (swchgalg->state != CHR_POSTCC &&
		swchgalg->state != CHR_PE40_POSTCC
		&& chg2_en
	    && (pdata->thermal_charging_current_limit > 500000 ||
		pdata->thermal_charging_current_limit ==  -1)) {
		charger_dev_safety_check(info->chg1_dev,
					 info->data.dual_polling_ieoc);
	}

	return mtk_pe40_cc_state(info);
}

static int mtk_dual_switch_chr_cc(struct charger_manager *info)
{
	bool chg_done = false;
	bool chg2_en = false;
	struct dual_switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct charger_data *pdata = &info->chg1_data;

	/* check bif */
	if (IS_ENABLED(CONFIG_MTK_BIF_SUPPORT)) {
		if (pmic_is_bif_exist() != 1) {
			pr_notice("No BIF battery, stop charging\n");
			swchgalg->state = CHR_ERROR;
			charger_manager_notifier(info, CHARGER_NOTIFY_ERROR);
		}
	}

	if (mtk_pe40_is_ready(info)) {
		chr_err("enter PE4.0!\n");
		swchgalg->state = CHR_PE40_INIT;
		info->pe4.is_connect = true;
		return 1;
	}

	dual_swchg_turn_on_charging(info);

	charger_dev_is_enabled(info->chg2_dev, &chg2_en);

	chr_err("safety_check state:%d en:%d thermal:%d",
		swchgalg->state,
		chg2_en,
		pdata->thermal_charging_current_limit);
	/* Check whether eoc condition is met */
	if (swchgalg->state != CHR_POSTCC && swchgalg->state != CHR_PE40_POSTCC
	    && chg2_en
	    && (pdata->thermal_charging_current_limit > 500000 ||
		pdata->thermal_charging_current_limit ==  -1)) {
		charger_dev_safety_check(info->chg1_dev,
					 info->data.dual_polling_ieoc);
	}

	if (info->enable_sw_jeita) {
		if (info->sw_jeita.pre_sm != TEMP_T2_TO_T3
		    && info->sw_jeita.sm == TEMP_T2_TO_T3) {
			/* set to CC state to reset chg2's ichg */
			pr_info("back to normal temp, reset state\n");
			swchgalg->state = CHR_CC;
		}
	}

	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	if (chg_done) {
		swchgalg->state = CHR_BATFULL;
		charger_dev_do_event(info->chg1_dev, EVENT_EOC, 0);
		chr_err("battery full!\n");
	}

	/* If it is not disabled by throttling,
	 * enable PE+/PE+20, if it is disabled
	 */
	if (info->chg1_data.thermal_input_current_limit != -1 &&
		info->chg1_data.thermal_input_current_limit < 300000)
		return 0;

	return 0;
}

int mtk_dual_switch_chr_err(struct charger_manager *info)
{
	struct dual_switch_charging_alg_data *swchgalg = info->algorithm_data;

	if (info->can_charging) {
		if (info->enable_sw_jeita) {
			if ((info->sw_jeita.sm == TEMP_BELOW_T0)
			    || (info->sw_jeita.sm == TEMP_ABOVE_T4))
				info->sw_jeita.error_recovery_flag = false;

			if ((info->sw_jeita.error_recovery_flag == false)
			    && (info->sw_jeita.sm != TEMP_BELOW_T0)
			    && (info->sw_jeita.sm != TEMP_ABOVE_T4)) {
				info->sw_jeita.error_recovery_flag = true;
				swchgalg->state = CHR_CC;
			}
		} else {
			if (info->thermal.sm == BAT_TEMP_NORMAL)
				swchgalg->state = CHR_CC;
		}
	}

	_disable_all_charging(info);
	return 0;
}

int mtk_dual_switch_chr_full(struct charger_manager *info)
{
	bool chg_done = false;
	struct dual_switch_charging_alg_data *swchgalg = info->algorithm_data;

	/* turn off LED */

	/*
	 * If CV is set to lower value by JEITA,
	 * Reset CV to normal value if temperture is in normal zone
	 */
	swchg_select_cv(info);
	info->polling_interval = CHARGING_FULL_INTERVAL;
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	if (!chg_done) {
		swchgalg->state = CHR_CC;
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
		mtk_pe20_set_to_check_chr_type(info, true);
		mtk_pe_set_to_check_chr_type(info, true);
		mtk_pe40_set_is_enable(info, true);
		info->enable_dynamic_cv = true;
		chr_err("battery recharging!\n");
		info->polling_interval = CHARGING_INTERVAL;
	}

	return 0;
}


static int mtk_dual_switch_charge_current(struct charger_manager *info)
{
	dual_swchg_select_charging_current_limit(info);
	return 0;
}

static int mtk_dual_switch_charging_run(struct charger_manager *info)
{
	struct dual_switch_charging_alg_data *swchgalg = info->algorithm_data;
	int ret = 10;
	bool chg2_en = false;

	pr_info("%s [%d]\n", __func__, swchgalg->state);

	if (mtk_pdc_check_charger(info) == false &&
	    mtk_is_TA_support_pd_pps(info) == false) {
		mtk_pe20_check_charger(info);
		if (mtk_pe20_get_is_connect(info) == false)
			mtk_pe_check_charger(info);
	}

	switch (swchgalg->state) {
	case CHR_CC:
	case CHR_TUNING:
	case CHR_POSTCC:
		ret = mtk_dual_switch_chr_cc(info);
		break;

	case CHR_PE40_INIT:
		ret = mtk_dual_switch_chr_pe40_init(info);
		break;

	case CHR_PE40_CC:
	case CHR_PE40_TUNING:
	case CHR_PE40_POSTCC:
		ret = mtk_dual_switch_chr_pe40_cc(info);
		break;

	case CHR_BATFULL:
		ret = mtk_dual_switch_chr_full(info);
		break;

	case CHR_ERROR:
		ret = mtk_dual_switch_chr_err(info);
		break;
	}

	charger_dev_dump_registers(info->chg1_dev);
	charger_dev_is_enabled(info->chg2_dev, &chg2_en);
	pr_debug_ratelimited("chg2_en: %d\n", chg2_en);
	if (chg2_en)
		charger_dev_dump_registers(info->chg2_dev);
	return 0;
}

int dual_charger_dev_event(struct notifier_block *nb, unsigned long event,
				void *v)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, chg1_nb);
	struct chgdev_notify *data = v;
	struct charger_data *pdata2 = &info->chg2_data;
	struct dual_switch_charging_alg_data *swchgalg = info->algorithm_data;
	u32 ichg2, ichg2_min;
	bool chg_en = false;
	bool chg2_chip_enabled = false;

	charger_dev_is_chip_enabled(info->chg2_dev, &chg2_chip_enabled);

	chr_info("charger_dev_event %ld\n", event);

	if (event == CHARGER_DEV_NOTIFY_EOC) {
		charger_dev_is_enabled(info->chg2_dev, &chg_en);

		if (!chg_en || !chg2_chip_enabled) {
			swchgalg->state = CHR_BATFULL;
			charger_manager_notifier(info, CHARGER_NOTIFY_EOC);
			if (info->chg1_dev->is_polling_mode == false)
				_wake_up_charger(info);
		} else {
			charger_dev_get_charging_current(info->chg2_dev,
							 &ichg2);
			charger_dev_get_min_charging_current(info->chg2_dev,
							 &ichg2_min);
			chr_info("ichg2:%d, ichg2_min:%d state:%d\n", ichg2,
				ichg2_min, swchgalg->state);
			if (ichg2 - 500000 < ichg2_min) {
				if (is_in_pe40_state(info))
					swchgalg->state = CHR_PE40_POSTCC;
				else
					swchgalg->state = CHR_POSTCC;

				charger_dev_enable(info->chg2_dev, false);
				charger_dev_set_eoc_current(info->chg1_dev,
								150000);
				if (mtk_pe40_get_is_connect(info) == false)
					charger_dev_enable_termination(
							info->chg1_dev, true);
			} else {
				if (is_in_pe40_state(info))
					swchgalg->state = CHR_PE40_TUNING;
				else
					swchgalg->state = CHR_TUNING;
				mutex_lock(&swchgalg->ichg_aicr_access_mutex);
				if (pdata2->charging_current_limit >= 500000)
					pdata2->charging_current_limit =
								ichg2 - 500000;
				else
					pdata2->charging_current_limit = 0;
				charger_dev_set_charging_current(info->chg2_dev,
						pdata2->charging_current_limit);
				mutex_unlock(&swchgalg->ichg_aicr_access_mutex);
			}
			charger_dev_reset_eoc_state(info->chg1_dev);
			_wake_up_charger(info);
		}

		return NOTIFY_DONE;
	}

	switch (event) {
	case CHARGER_DEV_NOTIFY_RECHG:
		charger_manager_notifier(info, CHARGER_NOTIFY_START_CHARGING);
		pr_info("%s: recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		info->safety_timeout = true;
		pr_info("%s: safety timer timeout\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		info->vbusov_stat = data->vbusov_stat;
		pr_info("%s: vbus ovp = %d\n", __func__, info->vbusov_stat);
		break;
	default:
		return NOTIFY_DONE;
	}

	if (info->chg1_dev->is_polling_mode == false)
		_wake_up_charger(info);

	return NOTIFY_DONE;
}

int mtk_dual_switch_charging_init(struct charger_manager *info)
{
	struct dual_switch_charging_alg_data *swch_alg;

	swch_alg = devm_kzalloc(&info->pdev->dev,
				sizeof(*swch_alg), GFP_KERNEL);
	if (!swch_alg)
		return -ENOMEM;

	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_info("Found primary charger [%s]\n",
			info->chg1_dev->props.alias_name);
	else
		chr_err("*** Error: can't find primary charger ***\n");

	info->chg2_dev = get_charger_by_name("secondary_chg");
	if (info->chg2_dev)
		chr_info("Found secondary charger [%s]\n",
			info->chg2_dev->props.alias_name);
	else
		chr_err("*** Error: can't find secondary charger\n");

	mutex_init(&swch_alg->ichg_aicr_access_mutex);

	info->algorithm_data = swch_alg;
	info->do_algorithm = mtk_dual_switch_charging_run;
	info->plug_in = mtk_dual_switch_charging_plug_in;
	info->plug_out = mtk_dual_switch_charging_plug_out;
	info->do_charging = mtk_dual_switch_charging_do_charging;
	info->do_event = dual_charger_dev_event;
	info->change_current_setting = mtk_dual_switch_charge_current;

	return 0;
}
