// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/pmic-voter.h>
#include <linux/power_supply.h>
#include <linux/iio/consumer.h>
#include "smblite-lib.h"
#include "smb5-iio.h"
#include "smblite-reg.h"
#include "schgm-flashlite.h"

int smblite_iio_get_prop(struct smb_charger *chg, int channel, int *val)
{
	union power_supply_propval pval = {0, };
	int rc = 0;

	pval.intval = 0;
	*val = 0;

	switch (channel) {
	/* USB */
	case PSY_IIO_USB_REAL_TYPE:
		*val = chg->real_charger_type;
		break;
	case PSY_IIO_TYPEC_MODE:
		rc = smblite_lib_get_usb_prop_typec_mode(chg, val);
		break;
	case PSY_IIO_TYPEC_POWER_ROLE:
		rc = smblite_lib_get_prop_typec_power_role(chg, val);
		break;
	case PSY_IIO_TYPEC_CC_ORIENTATION:
		rc = smblite_lib_get_prop_typec_cc_orientation(chg, val);
		break;
	case PSY_IIO_USB_INPUT_CURRENT_SETTLED:
		rc = smblite_lib_get_prop_input_current_settled(chg, val);
		break;
	case PSY_IIO_CONNECTOR_TYPE:
		*val = chg->connector_type;
		break;
	case PSY_IIO_HW_CURRENT_MAX:
		rc = smblite_lib_get_hw_current_max(chg, val);
		break;
	/* MAIN */
	case PSY_IIO_MAIN_INPUT_CURRENT_SETTLED:
		rc = smblite_lib_get_prop_input_current_settled(chg, val);
		if (!rc)
			*val = pval.intval;
		break;
	case PSY_IIO_MAIN_INPUT_VOLTAGE_SETTLED:
		rc = smblite_lib_get_prop_input_voltage_settled(chg, val);
		break;
	case PSY_IIO_FCC_DELTA:
		val = 0;
		break;
	case PSY_IIO_FLASH_ACTIVE:
		*val = chg->flash_active;
		break;
	case PSY_IIO_FLASH_TRIGGER:
		*val = 0;
		rc = schgm_flashlite_get_vreg_ok(chg, val);
		break;
	case PSY_IIO_CURRENT_MAX:
		rc = smblite_lib_get_icl_current(chg, val);
		break;
	case PSY_IIO_VOLTAGE_MAX:
		rc = smblite_lib_get_charge_param(chg, &chg->param.fv, val);
		break;
	case PSY_IIO_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smblite_lib_get_charge_param(chg, &chg->param.fcc, val);
		break;
	case PSY_IIO_MAIN_FCC_MAX:
		*val = chg->main_fcc_max;
		break;
	/* BATTERY */
	case PSY_IIO_CHARGER_TEMP:
		rc = smblite_lib_get_prop_charger_temp(chg, val);
		break;
	case PSY_IIO_SW_JEITA_ENABLED:
		*val = 1;
		break;
	case PSY_IIO_PARALLEL_DISABLE:
		*val = get_client_vote(chg->pl_disable_votable,
					      USER_VOTER);
		break;
	case PSY_IIO_CHARGE_DONE:
		rc = smblite_lib_get_prop_batt_charge_done(chg, val);
		break;
	case PSY_IIO_SET_SHIP_MODE:
		/* Not in ship mode as long as device is active */
		*val = 0;
		break;
	case PSY_IIO_INPUT_CURRENT_LIMITED:
		rc = smblite_lib_get_prop_input_current_limited(chg, val);
		break;
	case PSY_IIO_RECHARGE_SOC:
		*val = chg->auto_recharge_soc;
		break;
	case PSY_IIO_FORCE_RECHARGE:
		*val = 0;
		break;
	case PSY_IIO_FCC_STEPPER_ENABLE:
		*val = chg->fcc_stepper_enable;
		break;
	case PSY_IIO_DIE_HEALTH:
		rc = smblite_lib_get_die_health(chg, val);
		break;
	default:
		pr_err("get prop %x is not supported\n", channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err("Couldn't get prop %x rc = %d\n", channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

int smblite_iio_set_prop(struct smb_charger *chg, int channel, int val)
{
	union power_supply_propval pval = {0, };
	int icl_ua, rc = 0;

	switch (channel) {
	/* USB */
	case PSY_IIO_USB_REAL_TYPE:
		rc = smblite_lib_set_prop_usb_type(chg, val);
		break;
	case PSY_IIO_TYPEC_POWER_ROLE:
		rc = smblite_lib_set_prop_typec_power_role(chg, val);
		break;
	/* MAIN */
	case PSY_IIO_FLASH_ACTIVE:
		if (chg->flash_active != val) {
			chg->flash_active = val;

			rc = smblite_lib_get_prop_usb_present(chg, &pval);
			if (rc < 0)
				pr_err("Failed to get USB present status rc=%d\n",
						rc);
			if (!pval.intval) {
				/* vote 100ma when usb is not present*/
				vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER,
							true, USBIN_100UA);
			} else if (chg->flash_active) {
				icl_ua = get_effective_result_locked(
						chg->usb_icl_votable);
				if (icl_ua >= USBIN_400UA) {
					vote(chg->usb_icl_votable,
						FLASH_ACTIVE_VOTER,
						true, icl_ua - USBIN_300UA);
				}
			} else {
				vote(chg->usb_icl_votable, FLASH_ACTIVE_VOTER,
							false, 0);
			}
			pr_debug("flash_active=%d usb_present=%d icl=%d\n",
				chg->flash_active, pval.intval,
				get_effective_result_locked(
				chg->usb_icl_votable));
		}
		break;
	case PSY_IIO_VOLTAGE_MAX:
		rc = smblite_lib_set_charge_param(chg, &chg->param.fv, val);
		break;
	case PSY_IIO_CURRENT_MAX:
		rc = smblite_lib_set_icl_current(chg, val);
		break;
	case PSY_IIO_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smblite_lib_set_charge_param(chg, &chg->param.fcc, val);
		break;
	case PSY_IIO_MAIN_FCC_MAX:
		chg->main_fcc_max = val;
		rerun_election(chg->fcc_votable);
		break;
	/* BATTERY */
	case PSY_IIO_SET_SHIP_MODE:
		/* Not in ship mode as long as the device is active */
		if (!val)
			break;
		if (chg->iio_chan_list_smb_parallel)
			rc = iio_write_channel_raw(
				chg->iio_chan_list_smb_parallel[SMB_SET_SHIP_MODE],
				val);
		rc = smblite_lib_set_prop_ship_mode(chg, val);
		break;
	case PSY_IIO_RECHARGE_SOC:
		rc = smblite_lib_set_prop_rechg_soc_thresh(chg, val);
		break;
	case PSY_IIO_FORCE_RECHARGE:
		/* toggle charging to force recharge */
		vote(chg->chg_disable_votable, FORCE_RECHARGE_VOTER,
				true, 0);
		/* charge disable delay */
		msleep(50);
		vote(chg->chg_disable_votable, FORCE_RECHARGE_VOTER,
				false, 0);
		break;
	case PSY_IIO_FCC_STEPPER_ENABLE:
		chg->fcc_stepper_enable = val;
		break;
	case PSY_IIO_DIE_HEALTH:
		power_supply_changed(chg->batt_psy);
		break;
	default:
		pr_err("get prop %d is not supported\n", channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err("Couldn't set prop %d rc = %d\n", channel, rc);
		return rc;
	}

	return 0;
}

struct iio_channel **get_ext_channels(struct device *dev,
		 const char *const *channel_map, int size)
{
	int i, rc = 0;
	struct iio_channel **iio_ch_ext;

	iio_ch_ext = devm_kcalloc(dev, size, sizeof(*iio_ch_ext), GFP_KERNEL);
	if (!iio_ch_ext)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < size; i++) {
		iio_ch_ext[i] = devm_iio_channel_get(dev, channel_map[i]);

		if (IS_ERR(iio_ch_ext[i])) {
			rc = PTR_ERR(iio_ch_ext[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "%s channel unavailable, %d\n",
						channel_map[i], rc);
			return ERR_PTR(rc);
		}
	}

	return iio_ch_ext;
}
