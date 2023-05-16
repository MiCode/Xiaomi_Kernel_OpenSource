// SPDX-License-Identifier: GPL-2.0-onlyD/
/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/irq.h>
#include <linux/iio/consumer.h>
#include <linux/pmic-voter.h>
#include <linux/of_batterydata.h>
#include <linux/ktime.h>
#include "smb5-lib-munch.h"
#include "smb5-reg.h"
#include "schgm-flash.h"
#include "step-chg-jeita.h"
#include "storm-watch.h"
#include "schgm-flash.h"
#ifdef CONFIG_DUAL_FUEL_GAUGE_BQ27Z561
#include "dual_fuel_gauge_class.h"
#endif

#define smblib_err(chg, fmt, ...)		\
	pr_err("%s: %s: " fmt, chg->name,	\
		__func__, ##__VA_ARGS__)	\

#define smblib_dbg(chg, reason, fmt, ...)			\
	do {							\
		if (*chg->debug_mask & (reason))		\
			pr_err("%s: %s: " fmt, chg->name,	\
				__func__, ##__VA_ARGS__);	\
		else						\
			pr_debug("%s: %s: " fmt, chg->name,	\
				__func__, ##__VA_ARGS__);	\
	} while (0)

#define typec_rp_med_high(chg, typec_mode)			\
	((typec_mode == POWER_SUPPLY_TYPEC_SOURCE_MEDIUM	\
	|| typec_mode == POWER_SUPPLY_TYPEC_SOURCE_HIGH)	\
	&& (!chg->typec_legacy || chg->typec_legacy_use_rp_icl))

bool off_charge_flag;
static bool first_boot_flag;

static void update_sw_icl_max(struct smb_charger *chg, int pst);
static int smblib_get_prop_typec_mode(struct smb_charger *chg);
static int smblib_dc_therm_charging(struct smb_charger *chg,
					int temp_level);
static int smblib_get_batt_voltage_now(struct smb_charger *chg,
					union power_supply_propval *val);

int smblib_read(struct smb_charger *chg, u16 addr, u8 *val)
{
	unsigned int value;
	int rc = 0;

	rc = regmap_read(chg->regmap, addr, &value);
	if (rc >= 0)
		*val = (u8)value;

	return rc;
}

int smblib_batch_read(struct smb_charger *chg, u16 addr, u8 *val,
			int count)
{
	return regmap_bulk_read(chg->regmap, addr, val, count);
}

int smblib_write(struct smb_charger *chg, u16 addr, u8 val)
{
	return regmap_write(chg->regmap, addr, val);
}

int smblib_batch_write(struct smb_charger *chg, u16 addr, u8 *val,
			int count)
{
	return regmap_bulk_write(chg->regmap, addr, val, count);
}

int smblib_masked_write(struct smb_charger *chg, u16 addr, u8 mask, u8 val)
{
	return regmap_update_bits(chg->regmap, addr, mask, val);
}

int smblib_get_iio_channel(struct smb_charger *chg, const char *propname,
					struct iio_channel **chan)
{
	int rc = 0;

	rc = of_property_match_string(chg->dev->of_node,
					"io-channel-names", propname);
	if (rc < 0)
		return 0;

	*chan = iio_channel_get(chg->dev, propname);
	if (IS_ERR(*chan)) {
		rc = PTR_ERR(*chan);
		if (rc != -EPROBE_DEFER)
			smblib_err(chg, "%s channel unavailable, %d\n",
							propname, rc);
		*chan = NULL;
	}

	return rc;
}

#define DIV_FACTOR_MICRO_V_I	1
#define DIV_FACTOR_MILI_V_I	1000
#define DIV_FACTOR_DECIDEGC	100
int smblib_read_iio_channel(struct smb_charger *chg, struct iio_channel *chan,
							int div, int *data)
{
	int rc = 0;
	*data = -ENODATA;

	if (chan) {
		rc = iio_read_channel_processed(chan, data);
		if (rc < 0) {
			smblib_err(chg, "Error in reading IIO channel data, rc=%d\n",
					rc);
			return rc;
		}

		if (div != 0)
			*data /= div;
	}

	return rc;
}

int smblib_get_jeita_cc_delta(struct smb_charger *chg, int *cc_delta_ua)
{
	int rc, cc_minus_ua;
	u8 stat;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_7_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
			rc);
		return rc;
	}

	if (stat & BAT_TEMP_STATUS_HOT_SOFT_BIT) {
		rc = smblib_get_charge_param(chg, &chg->param.jeita_cc_comp_hot,
					&cc_minus_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get jeita cc minus rc=%d\n",
					rc);
			return rc;
		}
	} else if (stat & BAT_TEMP_STATUS_COLD_SOFT_BIT) {
		rc = smblib_get_charge_param(chg,
					&chg->param.jeita_cc_comp_cold,
					&cc_minus_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get jeita cc minus rc=%d\n",
					rc);
			return rc;
		}
	} else {
		cc_minus_ua = 0;
	}

	*cc_delta_ua = -cc_minus_ua;

	return 0;
}

int smblib_icl_override(struct smb_charger *chg, enum icl_override_mode  mode)
{
	int rc;
	u8 usb51_mode, icl_override, apsd_override;

	/*pr_info("smblib_icl_overridemode: %d\n", mode);*/
	switch (mode) {
	case SW_OVERRIDE_USB51_MODE:
		usb51_mode = 0;
		icl_override = ICL_OVERRIDE_BIT;
		apsd_override = 0;
		break;
	case SW_OVERRIDE_HC_MODE:
		usb51_mode = USBIN_MODE_CHG_BIT;
		icl_override = 0;
		apsd_override = ICL_OVERRIDE_AFTER_APSD_BIT;
		break;
	case SW_OVERRIDE_NO_CC_MODE:
		usb51_mode = USBIN_MODE_CHG_BIT;
		icl_override = 1;
		apsd_override = ICL_OVERRIDE_AFTER_APSD_BIT;
		break;
	case HW_AUTO_MODE:
	default:
		usb51_mode = USBIN_MODE_CHG_BIT;
		icl_override = 0;
		apsd_override = 0;
		break;
	}

	rc = smblib_masked_write(chg, USBIN_ICL_OPTIONS_REG,
				USBIN_MODE_CHG_BIT, usb51_mode);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set USBIN_ICL_OPTIONS rc=%d\n", rc);
		return rc;
	}

	rc = smblib_masked_write(chg, CMD_ICL_OVERRIDE_REG,
				ICL_OVERRIDE_BIT, icl_override);
	if (rc < 0) {
		smblib_err(chg, "Couldn't override ICL rc=%d\n", rc);
		return rc;
	}

	rc = smblib_masked_write(chg, USBIN_LOAD_CFG_REG,
				ICL_OVERRIDE_AFTER_APSD_BIT, apsd_override);
	if (rc < 0) {
		smblib_err(chg, "Couldn't override ICL_AFTER_APSD rc=%d\n", rc);
		return rc;
	}

	return rc;
}

/*
 * This function does smb_en pin access, which is lock protected.
 * It should be called with smb_lock held.
 */
static int smblib_select_sec_charger_locked(struct smb_charger *chg,
					int sec_chg)
{
	int rc = 0;

	switch (sec_chg) {
	case POWER_SUPPLY_CHARGER_SEC_CP:
		vote(chg->pl_disable_votable, PL_SMB_EN_VOTER, true, 0);

		/* select Charge Pump instead of slave charger */
		rc = smblib_masked_write(chg, MISC_SMB_CFG_REG,
					SMB_EN_SEL_BIT, SMB_EN_SEL_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't select SMB charger rc=%d\n",
				rc);
			return rc;
		}
		/* Enable Charge Pump, under HW control */
		rc = smblib_masked_write(chg, MISC_SMB_EN_CMD_REG,
					EN_CP_CMD_BIT, EN_CP_CMD_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable SMB charger rc=%d\n",
						rc);
			return rc;
		}
		vote(chg->smb_override_votable, PL_SMB_EN_VOTER, false, 0);
		break;
	case POWER_SUPPLY_CHARGER_SEC_PL:
		/* select slave charger instead of Charge Pump */
		rc = smblib_masked_write(chg, MISC_SMB_CFG_REG,
					SMB_EN_SEL_BIT, 0);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't select SMB charger rc=%d\n",
				rc);
			return rc;
		}
		/* Enable slave charger, under HW control */
		rc = smblib_masked_write(chg, MISC_SMB_EN_CMD_REG,
					EN_STAT_CMD_BIT, EN_STAT_CMD_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable SMB charger rc=%d\n",
						rc);
			return rc;
		}
		vote(chg->smb_override_votable, PL_SMB_EN_VOTER, false, 0);

		vote(chg->pl_disable_votable, PL_SMB_EN_VOTER, false, 0);

		break;
	case POWER_SUPPLY_CHARGER_SEC_NONE:
	default:
		vote(chg->pl_disable_votable, PL_SMB_EN_VOTER, true, 0);

		/* SW override, disabling secondary charger(s) */
		vote(chg->smb_override_votable, PL_SMB_EN_VOTER, true, 0);
		break;
	}

	return rc;
}

static int smblib_select_sec_charger(struct smb_charger *chg, int sec_chg,
					int reason, bool toggle)
{
	int rc;

	mutex_lock(&chg->smb_lock);

	if (toggle && sec_chg == POWER_SUPPLY_CHARGER_SEC_CP) {
		rc = smblib_select_sec_charger_locked(chg,
					POWER_SUPPLY_CHARGER_SEC_NONE);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't disable secondary charger rc=%d\n",
				rc);
			goto unlock_out;
		}

		/*
		 * A minimum of 20us delay is expected before switching on STAT
		 * pin.
		 */
		usleep_range(20, 30);
	}

	rc = smblib_select_sec_charger_locked(chg, sec_chg);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't switch secondary charger rc=%d\n",
			rc);
		goto unlock_out;
	}

	chg->sec_chg_selected = sec_chg;
	chg->cp_reason = reason;

unlock_out:
	mutex_unlock(&chg->smb_lock);

	return rc;
}

static void smblib_notify_extcon_props(struct smb_charger *chg, int id)
{
	union extcon_property_value val;
	union power_supply_propval prop_val;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC) {
		smblib_get_prop_typec_cc_orientation(chg, &prop_val);
		val.intval = ((prop_val.intval == 2) ? 1 : 0);
		extcon_set_property(chg->extcon, id,
				EXTCON_PROP_USB_TYPEC_POLARITY, val);
		val.intval = true;
		extcon_set_property(chg->extcon, id,
				EXTCON_PROP_USB_SS, val);
	} else if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		val.intval = false;
		extcon_set_property(chg->extcon, id,
				EXTCON_PROP_USB_SS, val);
	}
}

static void smblib_notify_device_mode(struct smb_charger *chg, bool enable)
{
	if (enable)
		smblib_notify_extcon_props(chg, EXTCON_USB);

	extcon_set_state_sync(chg->extcon, EXTCON_USB, enable);
}

static void smblib_notify_usb_host(struct smb_charger *chg, bool enable)
{
	int rc = 0;

	if (enable) {
		smblib_dbg(chg, PR_OTG, "enabling VBUS in OTG mode\n");
		rc = smblib_masked_write(chg, DCDC_CMD_OTG_REG,
					OTG_EN_BIT, OTG_EN_BIT);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't enable VBUS in OTG mode rc=%d\n", rc);
			return;
		}

		smblib_notify_extcon_props(chg, EXTCON_USB_HOST);
	} else {
		smblib_dbg(chg, PR_OTG, "disabling VBUS in OTG mode\n");
		rc = smblib_masked_write(chg, DCDC_CMD_OTG_REG,
					OTG_EN_BIT, 0);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't disable VBUS in OTG mode rc=%d\n",
				rc);
			return;
		}
	}

	extcon_set_state_sync(chg->extcon, EXTCON_USB_HOST, enable);
}

/********************
 * REGISTER GETTERS *
 ********************/

int smblib_get_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int *val_u)
{
	int rc = 0;
	u8 val_raw;

	rc = smblib_read(chg, param->reg, &val_raw);
	if (rc < 0) {
		smblib_err(chg, "%s: Couldn't read from 0x%04x rc=%d\n",
			param->name, param->reg, rc);
		return rc;
	}

	if (param->get_proc)
		*val_u = param->get_proc(param, val_raw);
	else
		*val_u = val_raw * param->step_u + param->min_u;
	smblib_dbg(chg, PR_REGISTER, "%s = %d (0x%02x)\n",
		   param->name, *val_u, val_raw);

	return rc;
}

int smblib_get_usb_suspend(struct smb_charger *chg, int *suspend)
{
	int rc = 0;
	u8 temp;

	rc = smblib_read(chg, USBIN_CMD_IL_REG, &temp);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USBIN_CMD_IL rc=%d\n", rc);
		return rc;
	}
	*suspend = temp & USBIN_SUSPEND_BIT;

	return rc;
}


static const s16 therm_lookup_table[] = {
	/* Index -30C~85C, ADC raw code */
	0x6C92, 0x6C43, 0x6BF0, 0x6B98, 0x6B3A, 0x6AD8, 0x6A70, 0x6A03,
	0x6990, 0x6916, 0x6897, 0x6811, 0x6785, 0x66F2, 0x6658, 0x65B7,
	0x650F, 0x6460, 0x63AA, 0x62EC, 0x6226, 0x6159, 0x6084, 0x5FA8,
	0x5EC3, 0x5DD8, 0x5CE4, 0x5BE9, 0x5AE7, 0x59DD, 0x58CD, 0x57B5,
	0x5696, 0x5571, 0x5446, 0x5314, 0x51DD, 0x50A0, 0x4F5E, 0x4E17,
	0x4CCC, 0x4B7D, 0x4A2A, 0x48D4, 0x477C, 0x4621, 0x44C4, 0x4365,
	0x4206, 0x40A6, 0x3F45, 0x3DE6, 0x3C86, 0x3B28, 0x39CC, 0x3872,
	0x3719, 0x35C4, 0x3471, 0x3322, 0x31D7, 0x308F, 0x2F4C, 0x2E0D,
	0x2CD3, 0x2B9E, 0x2A6E, 0x2943, 0x281D, 0x26FE, 0x25E3, 0x24CF,
	0x23C0, 0x22B8, 0x21B5, 0x20B8, 0x1FC2, 0x1ED1, 0x1DE6, 0x1D01,
	0x1C22, 0x1B49, 0x1A75, 0x19A8, 0x18E0, 0x181D, 0x1761, 0x16A9,
	0x15F7, 0x154A, 0x14A2, 0x13FF, 0x1361, 0x12C8, 0x1234, 0x11A4,
	0x1119, 0x1091, 0x100F, 0x0F90, 0x0F15, 0x0E9E, 0x0E2B, 0x0DBC,
	0x0D50, 0x0CE8, 0x0C83, 0x0C21, 0x0BC3, 0x0B67, 0x0B0F, 0x0AB9,
	0x0A66, 0x0A16, 0x09C9, 0x097E,
};

int smblib_get_thermal_threshold(struct smb_charger *chg, u16 addr, int *val)
{
	u8 buff[2];
	s16 temp;
	int rc = 0;
	int i, lower, upper;

	rc = smblib_batch_read(chg, addr, buff, 2);
	if (rc < 0) {
		pr_err("failed to write to 0x%04X, rc=%d\n", addr, rc);
		return rc;
	}

	temp = buff[1] | buff[0] << 8;

	lower = 0;
	upper = ARRAY_SIZE(therm_lookup_table) - 1;
	while (lower <= upper) {
		i = (upper + lower) / 2;
		if (therm_lookup_table[i] < temp)
			upper = i - 1;
		else if (therm_lookup_table[i] > temp)
			lower = i + 1;
		else
			break;
	}

	/* index 0 corresonds to -30C */
	*val = (i - 30) * 10;

	return rc;
}

struct apsd_result {
	const char * const name;
	const u8 bit;
	const enum power_supply_type pst;
};

enum {
	UNKNOWN,
	SDP,
	CDP,
	DCP,
	OCP,
	FLOAT,
	HVDCP2,
	HVDCP3,
	HVDCP3P5,
	MAX_TYPES
};

static const struct apsd_result smblib_apsd_results[] = {
	[UNKNOWN] = {
		.name	= "UNKNOWN",
		.bit	= 0,
		.pst	= POWER_SUPPLY_TYPE_UNKNOWN
	},
	[SDP] = {
		.name	= "SDP",
		.bit	= SDP_CHARGER_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB
	},
	[CDP] = {
		.name	= "CDP",
		.bit	= CDP_CHARGER_BIT,
#ifdef CONFIG_FACTORY_BUILD
		.pst	= POWER_SUPPLY_TYPE_USB
#else
		.pst	= POWER_SUPPLY_TYPE_USB_CDP
#endif
	},
	[DCP] = {
		.name	= "DCP",
		.bit	= DCP_CHARGER_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB_DCP
	},
	[OCP] = {
		.name	= "OCP",
		.bit	= OCP_CHARGER_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB_DCP
	},
	[FLOAT] = {
		.name	= "FLOAT",
		.bit	= FLOAT_CHARGER_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB_FLOAT
	},
	[HVDCP2] = {
		.name	= "HVDCP2",
		.bit	= DCP_CHARGER_BIT | QC_2P0_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB_HVDCP
	},
	[HVDCP3] = {
		.name	= "HVDCP3",
		.bit	= DCP_CHARGER_BIT | QC_3P0_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB_HVDCP_3,
	},
	[HVDCP3P5] = {
		.name	= "HVDCP3P5",
		.pst	= POWER_SUPPLY_TYPE_USB_HVDCP_3P5,
	},
};

static const struct apsd_result *smblib_get_apsd_result(struct smb_charger *chg)
{
	int rc, i;
	u8 apsd_stat, stat;
	const struct apsd_result *result = &smblib_apsd_results[UNKNOWN];

	rc = smblib_read(chg, APSD_STATUS_REG, &apsd_stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read APSD_STATUS rc=%d\n", rc);
		return result;
	}
	smblib_dbg(chg, PR_REGISTER, "APSD_STATUS = 0x%02x\n", apsd_stat);

	if (!(apsd_stat & APSD_DTC_STATUS_DONE_BIT))
		return result;

	rc = smblib_read(chg, APSD_RESULT_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read APSD_RESULT_STATUS rc=%d\n",
			rc);
		return result;
	}
	if (chg->apsd_stats != stat) {
		smblib_err(chg, "read APSD_RESULT_STATUS stat=0x%x\n", stat);
		chg->apsd_stats = stat;
	}
	stat &= APSD_RESULT_STATUS_MASK;

	for (i = 0; i < ARRAY_SIZE(smblib_apsd_results); i++) {
		if (smblib_apsd_results[i].bit == stat)
			result = &smblib_apsd_results[i];
	}

	if (apsd_stat & QC_CHARGER_BIT) {
		if (chg->qc3p5_supported) {
			if (result == &smblib_apsd_results[HVDCP3]
					&& chg->qc3p5_authenticated) {
				result = &smblib_apsd_results[HVDCP3P5];
			} else if ((result != &smblib_apsd_results[HVDCP3P5])
					&& (result != &smblib_apsd_results[HVDCP3])) {
				/* If not QC3 or QC3.5, return QC2 */
				result = &smblib_apsd_results[HVDCP2];
			}
		} else {
			/* since its a qc_charger, either return HVDCP3 or HVDCP2 */
			if (result != &smblib_apsd_results[HVDCP3])
				result = &smblib_apsd_results[HVDCP2];
		}
	}

	if (result == &smblib_apsd_results[HVDCP3] && chg->fake_hvdcp3) {
		pr_info("fake hvdcp3, set as hvdcp2");
		result = &smblib_apsd_results[HVDCP2];
		vote(chg->usb_icl_votable, MAIN_ICL_MIN_VOTER, false, 0);
		vote(chg->usb_icl_votable, HVDCP2_ICL_VOTER, true, 2000000);
		vote(chg->fcc_votable, HVDCP2_FCC_VOTER, true, 2000000);
	}

	return result;
}

#define INPUT_NOT_PRESENT	0
#define INPUT_PRESENT_USB	BIT(1)
#define INPUT_PRESENT_DC	BIT(2)
static int smblib_is_input_present(struct smb_charger *chg,
				   int *present)
{
	int rc;
	union power_supply_propval pval = {0, };

	*present = INPUT_NOT_PRESENT;

	rc = smblib_get_prop_usb_present(chg, &pval);
	if (rc < 0) {
		pr_err("Couldn't get usb presence status rc=%d\n", rc);
		return rc;
	}
	*present |= pval.intval ? INPUT_PRESENT_USB : INPUT_NOT_PRESENT;

	rc = smblib_get_prop_dc_present(chg, &pval);
	if (rc < 0) {
		pr_err("Couldn't get dc presence status rc=%d\n", rc);
		return rc;
	}
	*present |= pval.intval ? INPUT_PRESENT_DC : INPUT_NOT_PRESENT;

	return 0;
}

#define AICL_RANGE2_MIN_MV		5600
#define AICL_RANGE2_STEP_DELTA_MV	200
#define AICL_RANGE2_OFFSET		16
int smblib_get_aicl_cont_threshold(struct smb_chg_param *param, u8 val_raw)
{
	int base = param->min_u;
	u8 reg = val_raw;
	int step = param->step_u;


	if (val_raw >= AICL_RANGE2_OFFSET) {
		reg = val_raw - AICL_RANGE2_OFFSET;
		base = AICL_RANGE2_MIN_MV;
		step = AICL_RANGE2_STEP_DELTA_MV;
	}

	return base + (reg * step);
}

/********************
 * REGISTER SETTERS *
 ********************/
static const struct buck_boost_freq chg_freq_list[] = {
	[0] = {
		.freq_khz	= 2400,
		.val		= 7,
	},
	[1] = {
		.freq_khz	= 2100,
		.val		= 8,
	},
	[2] = {
		.freq_khz	= 1600,
		.val		= 11,
	},
	[3] = {
		.freq_khz	= 1200,
		.val		= 15,
	},
};

int smblib_set_chg_freq(struct smb_chg_param *param,
				int val_u, u8 *val_raw)
{
	u8 i;

	if (val_u > param->max_u || val_u < param->min_u)
		return -EINVAL;

	/* Charger FSW is the configured freqency / 2 */
	val_u *= 2;
	for (i = 0; i < ARRAY_SIZE(chg_freq_list); i++) {
		if (chg_freq_list[i].freq_khz == val_u)
			break;
	}
	if (i == ARRAY_SIZE(chg_freq_list)) {
		pr_err("Invalid frequency %d Hz\n", val_u / 2);
		return -EINVAL;
	}

	*val_raw = chg_freq_list[i].val;

	return 0;
}

int smblib_set_opt_switcher_freq(struct smb_charger *chg, int fsw_khz)
{
	union power_supply_propval pval = {0, };
	int rc = 0;

	rc = smblib_set_charge_param(chg, &chg->param.freq_switcher, fsw_khz);
	if (rc < 0)
		dev_err(chg->dev, "Error in setting freq_buck rc=%d\n", rc);

	if (chg->mode == PARALLEL_MASTER && chg->pl.psy) {
		pval.intval = fsw_khz;
		/*
		 * Some parallel charging implementations may not have
		 * PROP_BUCK_FREQ property - they could be running
		 * with a fixed frequency
		 */
		power_supply_set_property(chg->pl.psy,
				POWER_SUPPLY_PROP_BUCK_FREQ, &pval);
	}

	return rc;
}

int smblib_set_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int val_u)
{
	int rc = 0;
	u8 val_raw;

	if (param->set_proc) {
		rc = param->set_proc(param, val_u, &val_raw);
		if (rc < 0)
			return -EINVAL;
	} else {
		if (val_u > param->max_u || val_u < param->min_u)
			smblib_dbg(chg, PR_MISC,
				"%s: %d is out of range [%d, %d]\n",
				param->name, val_u, param->min_u, param->max_u);

		if (val_u > param->max_u)
			val_u = param->max_u;
		if (val_u < param->min_u)
			val_u = param->min_u;

		val_raw = (val_u - param->min_u) / param->step_u;
	}

	rc = smblib_write(chg, param->reg, val_raw);
	if (rc < 0) {
		smblib_err(chg, "%s: Couldn't write 0x%02x to 0x%04x rc=%d\n",
			param->name, val_raw, param->reg, rc);
		return rc;
	}

	smblib_dbg(chg, PR_REGISTER, "%s = %d (0x%02x)\n",
		   param->name, val_u, val_raw);

	return rc;
}

int smblib_set_usb_suspend(struct smb_charger *chg, bool suspend)
{
	int rc = 0;
	if (suspend)
		vote(chg->icl_irq_disable_votable, USB_SUSPEND_VOTER,
				true, 0);

	rc = smblib_masked_write(chg, USBIN_CMD_IL_REG, USBIN_SUSPEND_BIT,
				 suspend ? USBIN_SUSPEND_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't write %s to USBIN_SUSPEND_BIT rc=%d\n",
			suspend ? "suspend" : "resume", rc);

	if (!suspend)
		vote(chg->icl_irq_disable_votable, USB_SUSPEND_VOTER,
				false, 0);

	return rc;
}

int smblib_get_fastcharge_mode(struct smb_charger *chg)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	if (!chg->bms_psy)
		return 0;

	rc = power_supply_get_property(chg->bms_psy,
				POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't write fastcharge mode:%d\n", rc);
		return rc;
	}

	return pval.intval;
}

#ifdef CONFIG_QPNP_FG_GEN4
static int smb5_config_sys_iterm(struct smb_charger *chg, bool ffc_enable)
{
	union power_supply_propval pval = {0,};
	int rc;

	pval.intval = ffc_enable;

	rc = power_supply_set_property(chg->bms_psy,
			POWER_SUPPLY_PROP_SYS_TERMINATION_CURRENT, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set ffc termination current:%d\n", rc);
		return rc;
	}
	rc = power_supply_set_property(chg->bms_psy,
			POWER_SUPPLY_PROP_VBATT_FULL_VOL, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set ffc vabtt full:%d\n", rc);
		return rc;
	}

	rc = power_supply_set_property(chg->bms_psy,
			POWER_SUPPLY_PROP_KI_COEFF_CURRENT, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set ffc ki_coefet:%d\n", rc);
		return rc;
	}
	return rc;
}
#endif

int smb5_config_iterm(struct smb_charger *chg, int hi_thresh, int low_thresh)
{
	s16 raw_hi_thresh, raw_lo_thresh;
	u8 *buf;
	int rc = 0;

	rc = smblib_masked_write(chg, CHGR_ADC_TERM_CFG_REG,
			TERM_BASED_ON_SYNC_CONV_OR_SAMPLE_CNT,
			TERM_BASED_ON_SAMPLE_CNT);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure ADC_ITERM_CFG rc=%d\n",
				rc);
		return rc;
	}

	/*
	 * Conversion:
	 *	raw (A) = (scaled_mA * ADC_CHG_TERM_MASK) / (10 * 1000)
	 * Note: raw needs to be converted to big-endian format.
	 */
	dev_err(chg->dev, "configure ADC_ITERM_CFG hi_thresh=%d,low_thresh:%d\n",hi_thresh,low_thresh);
	if (hi_thresh) {
		raw_hi_thresh = ((hi_thresh * ADC_CHG_TERM_MASK) / 10000);
		raw_hi_thresh = sign_extend32(raw_hi_thresh, 15);
		buf = (u8 *)&raw_hi_thresh;
		rc = smblib_write(chg, CHGR_ADC_ITERM_UP_THD_MSB_REG,
							buf[1]);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't set term MSB rc=%d\n",
				rc);
			return rc;
		}
		rc = smblib_write(chg, CHGR_ADC_ITERM_UP_THD_LSB_REG,
							buf[0]);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't set term LSB rc=%d\n",
				rc);
			return rc;
		}
	}

	if (low_thresh) {
		raw_lo_thresh = ((low_thresh * ADC_CHG_TERM_MASK) / 10000);
		raw_lo_thresh = sign_extend32(raw_lo_thresh, 15);
		buf = (u8 *)&raw_lo_thresh;
		raw_lo_thresh = buf[1] | (buf[0] << 8);

		rc = smblib_batch_write(chg, CHGR_ADC_ITERM_LO_THD_MSB_REG,
				(u8 *)&raw_lo_thresh, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ITERM threshold LOW rc=%d\n",
					rc);
			return rc;
		}
	}

	return 0;
}

int smblib_set_fastcharge_mode(struct smb_charger *chg, bool enable)
{
	union power_supply_propval pval = {0,};
	int rc = 0;
	int termi = -220, batt_temp;

	if (!chg->bms_psy)
		return 0;

#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
	rc = power_supply_get_property(chg->bms_psy,
				POWER_SUPPLY_PROP_AUTHENTIC, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get battery authentic:%d\n", rc);
		return rc;
	}
	if (!pval.intval)
		enable = false;
#endif

	/*if soc > 95 do not set fastcharge flag*/
	rc = power_supply_get_property(chg->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get bms capacity:%d\n", rc);
		return rc;
	}
	if (enable && pval.intval >= 95) {
		smblib_dbg(chg, PR_MISC, "soc:%d is more than 95"
				"do not setfastcharge mode\n", pval.intval);
		enable = false;
	}

	/*if temp > 450 or temp < 100 do not set fastcharge flag*/
	rc = power_supply_get_property(chg->bms_psy,
				POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get bms capacity:%d\n", rc);
		goto set_term;
	}
	batt_temp = pval.intval;
	if (enable && (pval.intval >= chg->chg_warm_threshold  ||
			pval.intval <= chg->chg_cool_threshold)) {
		smblib_dbg(chg, PR_MISC, "temp:%d is abort"
				"do not setfastcharge mode\n", pval.intval);
		enable = false;
	}

	pval.intval = enable;
	rc = power_supply_set_property(chg->bms_psy,
				POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't write fastcharge mode:%d\n", rc);
		goto set_term;
	}
	if (enable) {
		rc = power_supply_get_property(chg->bms_psy,
				POWER_SUPPLY_PROP_FFC_TERMINATION_CURRENT, &pval);
		if (rc < 0) {
			smblib_err(chg, "Couldn't ffc termination current:%d\n", rc);
			goto set_term;
		}
		termi = pval.intval;
#ifdef CONFIG_DUAL_FUEL_GAUGE_BQ27Z561
		termi = pval.intval / 2;
#endif
	} else {
		termi = chg->chg_term_current_thresh_hi_from_dts;
	}
#ifdef CONFIG_QPNP_FG_GEN4
	smb5_config_sys_iterm(chg, enable);
#endif
set_term:
	smb5_config_iterm(chg, termi, 50);

	if (batt_temp < chg->chg_warm_threshold
			&& batt_temp > chg->chg_cool_threshold) {
		rc = vote(chg->fcc_votable, PD_VERIFED_VOTER,
			!enable, PD_UNVERIFED_CURRENT);
	}

	if (chg->support_ffc)
		rc = vote(chg->fv_votable, NON_FFC_VFLOAT_VOTER, !enable, chg->non_fcc_batt_profile_fv_uv);

	pr_info("fastcharge mode:%d termi:%d\n", enable, termi);

	return 0;
}

int smblib_set_dc_suspend(struct smb_charger *chg, bool suspend)
{
	int rc = 0;

	rc = smblib_masked_write(chg, DCIN_CMD_IL_REG, DCIN_SUSPEND_BIT,
				 suspend ? DCIN_SUSPEND_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't write %s to DCIN_SUSPEND_BIT rc=%d\n",
			suspend ? "suspend" : "resume", rc);

	return rc;
}

int smblib_set_prop_wireless_wakelock(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	if (val->intval) {
		/* hold a wakeup source when wireless tx is present */
		vote(chg->awake_votable, DC_AWAKE_VOTER, true, 0);
	} else {
		/* clear chg_awake wakeup source when wireless tx is absent */
		vote(chg->awake_votable, DC_AWAKE_VOTER, false, 0);
	}
	return 0;
}

static void smblib_wl_bq_enable(struct smb_charger *chg, bool enable)
{
	int rc = 0;

	if (enable) {
		rc = gpio_direction_output(chg->bq_en_gpio, 1);
		if (rc)
			pr_err("unable to set bq en to high rc = %d\n", rc);
	} else {
		rc = gpio_direction_output(chg->bq_en_gpio, 0);
		if (rc)
			pr_err("unable to set bq en to high rc = %d\n", rc);
	}

	rc = gpio_get_value(chg->bq_en_gpio);
	smblib_dbg(chg, PR_OEM, "bq_en_gpio: %d\n", rc);
}

static void smblib_dc_chg_q1_enable(struct smb_charger *chg, bool enable)
{
	int rc = 0;

	smblib_dbg(chg, PR_OEM, "enable: %d\n", enable);
	if (enable) {
		rc = gpio_direction_output(chg->dc_chg_gpio, 1);
		if (rc)
			pr_err("unable to set dc_chg_en to high rc = %d\n", rc);
	} else {
		rc = gpio_direction_output(chg->dc_chg_gpio, 0);
		if (rc)
			pr_err("unable to set dc_chg_en to high rc = %d\n", rc);
	}

	rc = gpio_get_value(chg->dc_chg_gpio);
	smblib_dbg(chg, PR_OEM, "dc_chg_gpio: %d\n", rc);
}
static void smblib_set_wireless_otg_state(struct smb_charger *chg, bool enable)
{
	union power_supply_propval val = {0, };
	if (!chg->wls_psy) {
		chg->wls_psy = power_supply_get_by_name("wireless");
		if (!chg->wls_psy)
			return;
	}
	val.intval = enable;
	if (chg->wls_psy)
		power_supply_set_property(chg->wls_psy , POWER_SUPPLY_PROP_OTG_STATE, &val);

}

static void smblib_ncp3902_enable(struct smb_charger *chg, bool enable)
{
	int rc = 0;

	printk("smblib_ncp3902_enable: %d\n", enable);
	if (!chg->ncp3902_en_gpio)
		return;

	if (enable) {
		gpio_set_value(chg->ncp3902_en_gpio, 1);
	} else {
		gpio_set_value(chg->ncp3902_en_gpio, 0);
	}
	rc = gpio_get_value(chg->ncp3902_en_gpio);
	smblib_dbg(chg, PR_OEM, "ncp3902_en_gpio: %d\n", rc);
}
static void ncp3902_pass_enable(struct smb_charger *chg, bool enable)
{
	int rc = 0;

	if (!chg->ncp3902_pass_en_gpio)
		return;

	if (enable) {
		gpio_set_value(chg->ncp3902_pass_en_gpio, 1);
	} else {
		gpio_set_value(chg->ncp3902_pass_en_gpio, 0);
	}
	rc = gpio_get_value(chg->ncp3902_pass_en_gpio);
	smblib_dbg(chg, PR_OEM, "ncp3902_pass_en_gpio: %d\n", rc);
}

static bool is_ncp3902_pull_low(struct smb_charger *chg)
{
	int rc = 0;

	if (!chg->ncp3902_en_gpio)
		return false;

	rc = gpio_get_value(chg->ncp3902_en_gpio);
	smblib_dbg(chg, PR_OEM, "ncp3902_en_gpio val is now: %d\n", rc);

	if (!rc)
		return true;
	else
		return false;
}

#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
static void smblib_check_batt_authentic(struct smb_charger *chg)
{
	int rc = 0;
	int authen_result = -1;
	union power_supply_propval pval = {0,};
	if (chg->batt_verify_psy && chg->bms_psy) {
		rc = power_supply_get_property(chg->bms_psy,
			POWER_SUPPLY_PROP_AUTHENTIC, &pval);
		if (!rc)
			authen_result = pval.intval;
		pr_err("authen_result: %d\n", authen_result);
		if (!authen_result) {
			pval.intval = 1;
			rc = power_supply_set_property(chg->batt_verify_psy,
					POWER_SUPPLY_PROP_AUTHENTIC, &pval);
			if (rc)
				pr_err("set batt_verify authentic prop failed: %d\n",
						rc);
		}
		/* notify smblib_notifier_call to reset BATT_VERIFY_VOTER fcc voter */
		power_supply_changed(chg->bms_psy);
	}
}
#endif

int smblib_set_prop_input_current_max(struct smb_charger *chg,
					const union power_supply_propval *val)
{
	int rc = 0;

	if (!chg->wireless_bq)
		return rc;

	rc = vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER,
					false, 0);
	vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);

	rc = vote(chg->usb_icl_votable, WIRELESS_BY_USB_IN_VOTER,
				true, val->intval);

	smblib_dc_therm_charging(chg, chg->dc_temp_level);

	return rc;
}

static int smblib_usb_pd_adapter_allowance_override(struct smb_charger *chg,
					u8 allowed_voltage)
{
	int rc = 0;

	if (chg->chg_param.smb_version == PMI632_SUBTYPE)
		return 0;

	rc = smblib_write(chg, USBIN_ADAPTER_ALLOW_OVERRIDE_REG,
						allowed_voltage);
	if (rc < 0)
		smblib_err(chg, "Couldn't write 0x%02x to USBIN_ADAPTER_ALLOW_OVERRIDE_REG rc=%d\n",
			allowed_voltage, rc);

	/*smblib_dbg(chg, PR_MISC, "set USBIN_ALLOW_OVERRIDE: %d\n",
			allowed_voltage);*/
	return rc;
}

#define MICRO_5V	5000000
#define MICRO_9V	9000000
#define MICRO_12V	12000000
static int smblib_set_usb_pd_fsw(struct smb_charger *chg, int voltage)
{
	int rc = 0;

	if (voltage == MICRO_5V)
		rc = smblib_set_opt_switcher_freq(chg, chg->chg_freq.freq_5V);
	else if (voltage > MICRO_5V && voltage < MICRO_9V)
		rc = smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_6V_8V);
	else if (voltage >= MICRO_9V && voltage < MICRO_12V)
		rc = smblib_set_opt_switcher_freq(chg, chg->chg_freq.freq_9V);
	else if (voltage == MICRO_12V)
		rc = smblib_set_opt_switcher_freq(chg, chg->chg_freq.freq_12V);
	else {
		smblib_err(chg, "Couldn't set Fsw: invalid voltage %d\n",
				voltage);
		return -EINVAL;
	}

	return rc;
}

#define CONT_AICL_HEADROOM_MV		1000
#define AICL_THRESHOLD_MV_IN_CC		5000
static int smblib_set_usb_pd_allowed_voltage(struct smb_charger *chg,
					int min_allowed_uv, int max_allowed_uv)
{
	int rc, aicl_threshold;
	u8 vbus_allowance;

	if (chg->chg_param.smb_version == PMI632_SUBTYPE)
		return 0;

	if (chg->pd_active == POWER_SUPPLY_PD_PPS_ACTIVE) {
		vbus_allowance = CONTINUOUS;
	} else if (min_allowed_uv == MICRO_5V && max_allowed_uv == MICRO_5V) {
		vbus_allowance = FORCE_5V;
	} else if (min_allowed_uv == MICRO_9V && max_allowed_uv == MICRO_9V) {
		vbus_allowance = FORCE_9V;
	} else if (min_allowed_uv == MICRO_12V && max_allowed_uv == MICRO_12V) {
		vbus_allowance = FORCE_12V;
	} else if (min_allowed_uv < MICRO_12V && max_allowed_uv <= MICRO_12V) {
		vbus_allowance = CONTINUOUS;
	} else {
		smblib_err(chg, "invalid allowed voltage [%d, %d]\n",
				min_allowed_uv, max_allowed_uv);
		return -EINVAL;
	}

	rc = smblib_usb_pd_adapter_allowance_override(chg, vbus_allowance);
	if (rc < 0) {
		smblib_err(chg, "set CONTINUOUS allowance failed, rc=%d\n",
				rc);
		return rc;
	}

	if (vbus_allowance != CONTINUOUS)
		return 0;

	aicl_threshold = min_allowed_uv / 1000 - CONT_AICL_HEADROOM_MV;
	if (chg->adapter_cc_mode)
		aicl_threshold = min(aicl_threshold, AICL_THRESHOLD_MV_IN_CC);

	rc = smblib_set_charge_param(chg, &chg->param.aicl_cont_threshold,
							aicl_threshold);
	if (rc < 0) {
		smblib_err(chg, "set CONT_AICL_THRESHOLD failed, rc=%d\n",
							rc);
		return rc;
	}

	return rc;
}

int smblib_set_aicl_cont_threshold(struct smb_chg_param *param,
				int val_u, u8 *val_raw)
{
	int base = param->min_u;
	int offset = 0;
	int step = param->step_u;

	if (val_u > param->max_u)
		val_u = param->max_u;
	if (val_u < param->min_u)
		val_u = param->min_u;

	if (val_u >= AICL_RANGE2_MIN_MV) {
		base = AICL_RANGE2_MIN_MV;
		step = AICL_RANGE2_STEP_DELTA_MV;
		offset = AICL_RANGE2_OFFSET;
	};

	*val_raw = ((val_u - base) / step) + offset;

	return 0;
}

/********************
 * HELPER FUNCTIONS *
 ********************/
static bool is_cp_available(struct smb_charger *chg)
{
	if (!chg->cp_psy)
		chg->cp_psy = power_supply_get_by_name("charge_pump_master");

	return !!chg->cp_psy;
}

static bool is_cp_topo_vbatt(struct smb_charger *chg)
{
	int rc;
	bool is_vbatt;
	union power_supply_propval pval;

	if (!is_cp_available(chg))
		return false;

	rc = power_supply_get_property(chg->cp_psy,
				POWER_SUPPLY_PROP_PARALLEL_OUTPUT_MODE, &pval);
	if (rc < 0)
		return false;

	is_vbatt = (pval.intval == POWER_SUPPLY_PL_OUTPUT_VBAT);

	smblib_dbg(chg, PR_WLS, "%s\n", is_vbatt ? "true" : "false");

	return is_vbatt;
}

#define CP_TO_MAIN_ICL_OFFSET_PC		10
int smblib_get_qc3_main_icl_offset(struct smb_charger *chg, int *offset_ua)
{
	union power_supply_propval pval = {0, };
	int rc;

	/*
	 * Apply ILIM offset to main charger's FCC if all of the following
	 * conditions are met:
	 * - HVDCP3 adapter with CP as parallel charger
	 * - Output connection topology is VBAT
	 */
	if (!is_cp_topo_vbatt(chg) || chg->hvdcp3_standalone_config
		|| ((chg->real_charger_type != POWER_SUPPLY_TYPE_USB_HVDCP_3)
		&& chg->real_charger_type != POWER_SUPPLY_TYPE_USB_HVDCP_3P5))
		return -EINVAL;

	rc = power_supply_get_property(chg->cp_psy, POWER_SUPPLY_PROP_CP_ENABLE,
					&pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get CP ENABLE rc=%d\n", rc);
		return rc;
	}

	if (!pval.intval)
		return -EINVAL;

	rc = power_supply_get_property(chg->cp_psy, POWER_SUPPLY_PROP_CP_ILIM,
					&pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get CP ILIM rc=%d\n", rc);
		return rc;
	}

	*offset_ua = (pval.intval * CP_TO_MAIN_ICL_OFFSET_PC * 2) / 100;

	return 0;
}

int smblib_get_prop_from_bbc(struct smb_charger *chg,
        enum power_supply_property psp,
        union power_supply_propval *val)
{
    int rc;

    if (!chg->bbc_psy)
        chg->bbc_psy = power_supply_get_by_name("bbc");

    if (!chg->bbc_psy || !chg->ext_bbc)
	    return -EINVAL;

    rc = power_supply_get_property(chg->bbc_psy, psp, val);

    return rc;
}

int smblib_get_prop_from_bms(struct smb_charger *chg,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int rc;

	if (!chg->bms_psy)
		return -EINVAL;

	rc = power_supply_get_property(chg->bms_psy, psp, val);

	return rc;
}

void smblib_apsd_enable(struct smb_charger *chg, bool enable)
{
	int rc;

	rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
				BC1P2_SRC_DETECT_BIT,
				enable ? BC1P2_SRC_DETECT_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "failed to write USBIN_OPTIONS_1_CFG rc=%d\n",
				rc);
}

void smblib_hvdcp_detect_enable(struct smb_charger *chg, bool enable)
{
	int rc;
	u8 mask;

	mask = HVDCP_AUTH_ALG_EN_CFG_BIT | HVDCP_EN_BIT;
	rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG, mask,
						enable ? mask : 0);
	if (rc < 0)
		smblib_err(chg, "failed to write USBIN_OPTIONS_1_CFG rc=%d\n",
				rc);
}

static void smblib_hvdcp_detect_try_enable(struct smb_charger *chg, bool enable)
{
	if (chg->hvdcp_disable || chg->pd_not_supported)
		return;
	smblib_hvdcp_detect_enable(chg, enable);
}

void smblib_hvdcp_hw_inov_enable(struct smb_charger *chg, bool enable)
{
	int rc;

	rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
				HVDCP_AUTONOMOUS_MODE_EN_CFG_BIT,
				enable ? HVDCP_AUTONOMOUS_MODE_EN_CFG_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "failed to write USBIN_OPTIONS_1_CFG rc=%d\n",
				rc);
}

void smblib_hvdcp_exit_config(struct smb_charger *chg)
{
	u8 stat;
	int rc;

	rc = smblib_read(chg, APSD_RESULT_STATUS_REG, &stat);
	if (rc < 0)
		return;

	if (stat & (QC_3P0_BIT | QC_2P0_BIT)) {
		/* force HVDCP to 5V */
		smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
				HVDCP_AUTONOMOUS_MODE_EN_CFG_BIT, 0);
		smblib_write(chg, CMD_HVDCP_2_REG, FORCE_5V_BIT);

		/* rerun APSD */
		smblib_masked_write(chg, CMD_APSD_REG, APSD_RERUN_BIT,
				APSD_RERUN_BIT);
	}
}

static int smblib_request_dpdm(struct smb_charger *chg, bool enable)
{
	int rc = 0;

	if (chg->pr_swap_in_progress)
		return 0;

	/* fetch the DPDM regulator */
	if (!chg->dpdm_reg && of_get_property(chg->dev->of_node,
				"dpdm-supply", NULL)) {
		chg->dpdm_reg = devm_regulator_get(chg->dev, "dpdm");
		if (IS_ERR(chg->dpdm_reg)) {
			rc = PTR_ERR(chg->dpdm_reg);
			smblib_err(chg, "Couldn't get dpdm regulator rc=%d\n",
					rc);
			chg->dpdm_reg = NULL;
			return rc;
		}
	}

	mutex_lock(&chg->dpdm_lock);
	if (enable) {
		if (chg->dpdm_reg && !chg->dpdm_enabled) {
			smblib_dbg(chg, PR_MISC, "enabling DPDM regulator\n");
			rc = regulator_enable(chg->dpdm_reg);
			if (rc < 0)
				smblib_err(chg,
					"Couldn't enable dpdm regulator rc=%d\n",
					rc);
			else
				chg->dpdm_enabled = true;
		}
	} else {
		if (chg->dpdm_reg && chg->dpdm_enabled) {
			smblib_dbg(chg, PR_MISC, "disabling DPDM regulator\n");
			rc = regulator_disable(chg->dpdm_reg);
			if (rc < 0)
				smblib_err(chg,
					"Couldn't disable dpdm regulator rc=%d\n",
					rc);
			else
				chg->dpdm_enabled = false;
		}
	}
	mutex_unlock(&chg->dpdm_lock);

	return rc;
}

void smblib_rerun_apsd(struct smb_charger *chg)
{
	int rc;

	if (chg->fake_hvdcp3 &&
			(chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP)) {
		smblib_dbg(chg, PR_MISC, "fake_hvdcp3: no rerun apsd.\n");
	} else {
		smblib_dbg(chg, PR_MISC, "re-running APSD\n");

		rc = smblib_masked_write(chg, CMD_APSD_REG,
					APSD_RERUN_BIT, APSD_RERUN_BIT);
		if (rc < 0)
			smblib_err(chg, "Couldn't re-run APSD rc=%d\n", rc);
	}
}

static const struct apsd_result *smblib_update_usb_type(struct smb_charger *chg)
{
	const struct apsd_result *apsd_result = smblib_get_apsd_result(chg);

	/* if PD is active, APSD is disabled so won't have a valid result */
	if (chg->pd_active) {
		chg->real_charger_type = POWER_SUPPLY_TYPE_USB_PD;
		chg->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_PD;
	} else if (chg->qc3p5_detected) {
		chg->real_charger_type = POWER_SUPPLY_TYPE_USB_HVDCP_3P5;
		chg->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_PD;
	} else {
		/*
		 * Update real charger type only if its not FLOAT
		 * detected as as SDP
		 */
		if (!(apsd_result->pst == POWER_SUPPLY_TYPE_USB_FLOAT &&
			chg->real_charger_type == POWER_SUPPLY_TYPE_USB) &&
				(!chg->qc3p5_supported || chg->qc3p5_auth_complete ||
				 apsd_result->pst != POWER_SUPPLY_TYPE_USB_HVDCP_3)) {
			chg->real_charger_type = apsd_result->pst;
			chg->usb_psy_desc.type = apsd_result->pst;
		}
	}

	if(chg->mtbf_current >= 1500){
		chg->real_charger_type = POWER_SUPPLY_TYPE_USB_CDP;
		chg->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		smblib_dbg(chg, PR_REGISTER, "mtbf current 1500 and force to CDP!\n");
	}
	smblib_dbg(chg, PR_REGISTER, "APSD=%s PD=%d QC3P5=%d\n",
			apsd_result->name, chg->pd_active, chg->qc3p5_detected);
	return apsd_result;
}

static int smblib_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct smb_charger *chg = container_of(nb, struct smb_charger, nb);
#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
	int rc;
	union power_supply_propval pval = {0,};
#endif

	if (!strcmp(psy->desc->name, "bms")) {
		if (!chg->bms_psy)
			chg->bms_psy = psy;
		if (ev == PSY_EVENT_PROP_CHANGED) {
#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
			rc = power_supply_get_property(chg->bms_psy,
					POWER_SUPPLY_PROP_AUTHENTIC, &pval);
			if (rc < 0) {
				pr_err("Couldn't get batt verify status rc=%d\n", rc);
			}
			chg->batt_verified = pval.intval;
			pr_err("batt_verified =%d\n", chg->batt_verified);
			schedule_work(&chg->batt_verify_update_work);
#endif
			schedule_work(&chg->bms_update_work);
		}
	}

	if (!strcmp(psy->desc->name, "battery")) {
		schedule_work(&chg->batt_update_work);
	}

	if (chg->jeita_configured == JEITA_CFG_NONE)
		schedule_work(&chg->jeita_update_work);

	if (chg->sec_pl_present && !chg->pl.psy
		&& !strcmp(psy->desc->name, "parallel")) {
		chg->pl.psy = psy;
		schedule_work(&chg->pl_update_work);
	}

	if (!strcmp(psy->desc->name, "charge_pump_master")) {
		pm_stay_awake(chg->dev);
		schedule_work(&chg->cp_status_change_work);
	}

	return NOTIFY_OK;
}

static int smblib_register_notifier(struct smb_charger *chg)
{
	int rc;

	chg->nb.notifier_call = smblib_notifier_call;
	rc = power_supply_reg_notifier(&chg->nb);
	if (rc < 0) {
		smblib_err(chg, "Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

int smblib_mapping_soc_from_field_value(struct smb_chg_param *param,
					     int val_u, u8 *val_raw)
{
	if (val_u > param->max_u || val_u < param->min_u)
		return -EINVAL;

	*val_raw = val_u << 1;

	return 0;
}

int smblib_mapping_cc_delta_to_field_value(struct smb_chg_param *param,
					   u8 val_raw)
{
	int val_u  = val_raw * param->step_u + param->min_u;

	if (val_u > param->max_u)
		val_u -= param->max_u * 2;

	return val_u;
}

int smblib_mapping_cc_delta_from_field_value(struct smb_chg_param *param,
					     int val_u, u8 *val_raw)
{
	if (val_u > param->max_u || val_u < param->min_u - param->max_u)
		return -EINVAL;

	val_u += param->max_u * 2 - param->min_u;
	val_u %= param->max_u * 2;
	*val_raw = val_u / param->step_u;

	return 0;
}

static void smblib_uusb_removal(struct smb_charger *chg)
{
	int rc;
	struct smb_irq_data *data;
	struct storm_watch *wdata;
	int sec_charger;

	sec_charger = chg->sec_pl_present ? POWER_SUPPLY_CHARGER_SEC_PL :
				POWER_SUPPLY_CHARGER_SEC_NONE;
	smblib_select_sec_charger(chg, sec_charger, POWER_SUPPLY_CP_NONE,
					false);

	cancel_delayed_work_sync(&chg->pl_enable_work);

	if (chg->wa_flags & BOOST_BACK_WA) {
		data = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data;
		if (data) {
			wdata = &data->storm_data;
			update_storm_count(wdata, WEAK_CHG_STORM_COUNT);
			vote(chg->usb_icl_votable, BOOST_BACK_VOTER, false, 0);
			vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
					false, 0);
		}
	}
	vote(chg->pl_disable_votable, PL_DELAY_VOTER, true, 0);
	vote(chg->awake_votable, PL_DELAY_VOTER, false, 0);

	/* reset both usbin current and voltage votes */
	vote(chg->pl_enable_votable_indirect, USBIN_I_VOTER, false, 0);
	vote(chg->pl_enable_votable_indirect, USBIN_V_VOTER, false, 0);
	vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
			is_flash_active(chg) ? SDP_CURRENT_UA : SDP_100_MA);
	vote(chg->usb_icl_votable, SW_QC3_VOTER, false, 0);
	vote(chg->usb_icl_votable, HVDCP2_ICL_VOTER, false, 0);
	vote(chg->usb_icl_votable, CHG_TERMINATION_VOTER, false, 0);
	vote(chg->usb_icl_votable, THERMAL_THROTTLE_VOTER, false, 0);
	vote(chg->limited_irq_disable_votable, CHARGER_TYPE_VOTER,
			true, 0);
	vote(chg->hdc_irq_disable_votable, CHARGER_TYPE_VOTER, true, 0);
	vote(chg->hdc_irq_disable_votable, HDC_IRQ_VOTER, false, 0);

	/* Remove SW thermal regulation WA votes */
	vote(chg->usb_icl_votable, SW_THERM_REGULATION_VOTER, false, 0);
	vote(chg->pl_disable_votable, SW_THERM_REGULATION_VOTER, false, 0);
	vote(chg->dc_suspend_votable, SW_THERM_REGULATION_VOTER, false, 0);
	if (chg->cp_disable_votable)
		vote(chg->cp_disable_votable, SW_THERM_REGULATION_VOTER,
								false, 0);

	/* reset USBOV votes and cancel work */
	cancel_delayed_work_sync(&chg->usbov_dbc_work);
	vote(chg->awake_votable, USBOV_DBC_VOTER, false, 0);
	chg->dbc_usbov = false;

	chg->voltage_min_uv = MICRO_5V;
	chg->voltage_max_uv = MICRO_5V;
	chg->usbin_forced_max_uv = 0;
	chg->usb_icl_delta_ua = 0;
	chg->pulse_cnt = 0;
	chg->uusb_apsd_rerun_done = false;
	chg->chg_param.forced_main_fcc = 0;

	del_timer_sync(&chg->apsd_timer);
	chg->apsd_ext_timeout = false;

	/* write back the default FLOAT charger configuration */
	rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				(u8)FLOAT_OPTIONS_MASK, chg->float_cfg);
	if (rc < 0)
		smblib_err(chg, "Couldn't write float charger options rc=%d\n",
			rc);

	/* clear USB ICL vote for USB_PSY_VOTER */
	rc = vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't un-vote for USB ICL rc=%d\n", rc);

	/* clear USB ICL vote for DCP_VOTER */
	rc = vote(chg->usb_icl_votable, DCP_VOTER, false, 0);
	if (rc < 0)
		smblib_err(chg,
			"Couldn't un-vote DCP from USB ICL rc=%d\n", rc);

	/*
	 * if non-compliant charger caused UV, restore original max pulses
	 * and turn SUSPEND_ON_COLLAPSE_USBIN_BIT back on.
	 */
	if (chg->qc2_unsupported_voltage) {
		rc = smblib_masked_write(chg, HVDCP_PULSE_COUNT_MAX_REG,
				HVDCP_PULSE_COUNT_MAX_QC2_MASK,
				chg->qc2_max_pulses);
		if (rc < 0)
			smblib_err(chg, "Couldn't restore max pulses rc=%d\n",
					rc);

		rc = smblib_masked_write(chg, USBIN_AICL_OPTIONS_CFG_REG,
				SUSPEND_ON_COLLAPSE_USBIN_BIT,
				SUSPEND_ON_COLLAPSE_USBIN_BIT);
		if (rc < 0)
			smblib_err(chg, "Couldn't turn on SUSPEND_ON_COLLAPSE_USBIN_BIT rc=%d\n",
					rc);

		chg->qc2_unsupported_voltage = QC2_COMPLIANT;
	}

	chg->qc3p5_detected = false;
	smblib_update_usb_type(chg);
}

void smblib_suspend_on_debug_battery(struct smb_charger *chg)
{
	int rc;
	union power_supply_propval val;

	rc = smblib_get_prop_from_bms(chg,
			POWER_SUPPLY_PROP_DEBUG_BATTERY, &val);
	if (rc < 0) {
		//smblib_err(chg, "Couldn't get debug battery prop rc=%d\n", rc);
		return;
	}
	if (chg->suspend_input_on_debug_batt) {
		vote(chg->usb_icl_votable, DEBUG_BOARD_VOTER, val.intval, 0);
		vote(chg->dc_suspend_votable, DEBUG_BOARD_VOTER, val.intval, 0);
		if (val.intval)
			pr_info("Input suspended: Fake battery\n");
	} else {
		vote(chg->chg_disable_votable, DEBUG_BOARD_VOTER,
					val.intval, 0);
	}
}

int smblib_rerun_apsd_if_required(struct smb_charger *chg)
{
	union power_supply_propval val;
	int rc;

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get usb present rc = %d\n", rc);
		return rc;
	}

	if (!val.intval)
		return 0;

	rc = smblib_request_dpdm(chg, true);
	if (rc < 0)
		smblib_err(chg, "Couldn't to enable DPDM rc=%d\n", rc);

	chg->uusb_apsd_rerun_done = true;
	smblib_rerun_apsd(chg);

	return 0;
}

static int smblib_get_pulse_cnt(struct smb_charger *chg, int *count)
{
	*count = chg->pulse_cnt;
	return 0;
}

#define USBIN_25MA	25000
#define USBIN_30MA	30000
#define USBIN_100MA	100000
#define USBIN_150MA	150000
#define USBIN_500MA	500000
#define USBIN_900MA	900000
#define USBIN_1000MA	1000000
static int set_sdp_current(struct smb_charger *chg, int icl_ua)
{
	int rc;
	u8 icl_options;
	const struct apsd_result *apsd_result = smblib_get_apsd_result(chg);

	/* power source is SDP */
	switch (icl_ua) {
	case USBIN_100MA:
		/* USB 2.0 100mA */
		icl_options = 0;
		break;
	case USBIN_150MA:
		/* USB 3.0 150mA */
		icl_options = CFG_USB3P0_SEL_BIT;
		break;
	case USBIN_500MA:
		/* USB 2.0 500mA */
		icl_options = USB51_MODE_BIT;
		break;
	case USBIN_900MA:
		/* USB 3.0 900mA */
		icl_options = CFG_USB3P0_SEL_BIT | USB51_MODE_BIT;
		break;
	default:
		return -EINVAL;
	}

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB &&
		apsd_result->pst == POWER_SUPPLY_TYPE_USB_FLOAT) {
		/*
		 * change the float charger configuration to SDP, if this
		 * is the case of SDP being detected as FLOAT
		 */
		rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
			FORCE_FLOAT_SDP_CFG_BIT, FORCE_FLOAT_SDP_CFG_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set float ICL options rc=%d\n",
						rc);
			return rc;
		}
	}

	rc = smblib_masked_write(chg, USBIN_ICL_OPTIONS_REG,
			CFG_USB3P0_SEL_BIT | USB51_MODE_BIT, icl_options);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set ICL options rc=%d\n", rc);
		return rc;
	}

	rc = smblib_icl_override(chg, SW_OVERRIDE_USB51_MODE);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set ICL override rc=%d\n", rc);
		return rc;
	}

	return rc;
}

#define CLEAN_CP_TO_SW_DELAY_MS 500
int smblib_set_icl_current(struct smb_charger *chg, int icl_ua)
{
	int rc = 0;
	union power_supply_propval pval = {0,};
	int dc_power_on = 0;
	enum icl_override_mode icl_override = HW_AUTO_MODE;
	static int pre_icl;

	/* suspend if 25mA or less is requested */
	bool suspend = (icl_ua <= USBIN_25MA);

	if (chg->chg_param.smb_version == PMI632_SUBTYPE)
		schgm_flash_torch_priority(chg, suspend ? TORCH_BOOST_MODE :
					TORCH_BUCK_MODE);

	/* Do not configure ICL from SW for DAM cables */
	if (smblib_get_prop_typec_mode(chg) ==
			    POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY)
		return 0;

	if (pre_icl == 0 && icl_ua >= 1500000) {
		chg->cp_to_sw_status = true;
		schedule_delayed_work(&chg->clean_cp_to_sw_work,
				msecs_to_jiffies(CLEAN_CP_TO_SW_DELAY_MS));
	}
	pre_icl = icl_ua;

	/*pr_err("set_icl_current_icl_ua : %d\n", icl_ua);*/

	if (suspend)
		return smblib_set_usb_suspend(chg, true);

	if (icl_ua == INT_MAX)
		goto set_mode;

	if (chg->wireless_bq) {
		smblib_get_prop_dc_present(chg, &pval);
		dc_power_on = pval.intval;
	}
	if (chg->wireless_bq && (dc_power_on == 1)) {
		if ((chg->typec_mode >= POWER_SUPPLY_TYPEC_NONE)
					&& (chg->real_charger_type >= POWER_SUPPLY_TYPE_USB
						|| chg->real_charger_type == POWER_SUPPLY_TYPE_UNKNOWN)) {
			rc = smblib_set_charge_param(chg, &chg->param.usb_icl, icl_ua);
			icl_override = SW_OVERRIDE_NO_CC_MODE;
			goto set_mode;
		}
	}

	if ((chg->typec_mode == POWER_SUPPLY_TYPEC_NONE)
				&& (chg->real_charger_type >= POWER_SUPPLY_TYPE_USB)) {
		rc = smblib_set_charge_param(chg, &chg->param.usb_icl, icl_ua);
		icl_override = SW_OVERRIDE_NO_CC_MODE;
		goto set_mode;
	}

	/* configure current */
	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB
		&& (chg->typec_legacy
		|| chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT
		|| chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)) {
		rc = set_sdp_current(chg, icl_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set SDP ICL rc=%d\n", rc);
			goto out;
		}
	} else {
		/*
		 * Try USB 2.0/3,0 option first on USB path when maximum input
		 * current limit is 500mA or below for better accuracy; in case
		 * of error, proceed to use USB high-current mode.
		 */
		if (icl_ua < USBIN_100MA) {
			rc = set_sdp_current(chg, icl_ua);
			if (rc >= 0)
				goto unsuspend;
		}

		rc = smblib_set_charge_param(chg, &chg->param.usb_icl, icl_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set HC ICL rc=%d\n", rc);
			goto out;
		}
		icl_override = SW_OVERRIDE_HC_MODE;
	}

set_mode:
	rc = smblib_icl_override(chg, icl_override);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set ICL override rc=%d\n", rc);
		goto out;
	}

unsuspend:
	/* unsuspend after configuring current and override */
	rc = smblib_set_usb_suspend(chg, false);
	if (rc < 0) {
		smblib_err(chg, "Couldn't resume input rc=%d\n", rc);
		goto out;
	}

	/* Re-run AICL */
	if (icl_override != SW_OVERRIDE_HC_MODE)
		rc = smblib_run_aicl(chg, RERUN_AICL);
out:
	return rc;
}

int smblib_get_icl_current(struct smb_charger *chg, int *icl_ua)
{
	int rc;

	rc = smblib_get_charge_param(chg, &chg->param.icl_max_stat, icl_ua);
	if (rc < 0)
		smblib_err(chg, "Couldn't get HC ICL rc=%d\n", rc);

	return rc;
}

int smblib_toggle_smb_en(struct smb_charger *chg, int toggle)
{
	int rc = 0;

	if (!toggle)
		return rc;

	rc = smblib_select_sec_charger(chg, chg->sec_chg_selected,
				chg->cp_reason, true);

	return rc;
}

int smblib_get_irq_status(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 reg;

	if (chg->wa_flags & SKIP_MISC_PBS_IRQ_WA) {
		val->intval = 0;
		return 0;
	}

	mutex_lock(&chg->irq_status_lock);
	/* Report and clear cached status */
	val->intval = chg->irq_status;
	chg->irq_status = 0;

	/* get real time status of pulse skip irq */
	rc = smblib_read(chg, MISC_PBS_RT_STS_REG, &reg);
	if (rc < 0)
		smblib_err(chg, "Couldn't read MISC_PBS_RT_STS_REG rc=%d\n",
				rc);
	else
		val->intval |= (reg & PULSE_SKIP_IRQ_BIT);
	mutex_unlock(&chg->irq_status_lock);

	return rc;
}

/****************************
 * uUSB Moisture Protection *
 ****************************/
#define MICRO_USB_DETECTION_ON_TIME_20_MS 0x08
#define MICRO_USB_DETECTION_PERIOD_X_100 0x03
#define U_USB_STATUS_WATER_PRESENT 0x00
static int smblib_set_moisture_protection(struct smb_charger *chg,
				bool enable)
{
	int rc = 0;

	if (chg->moisture_present == enable) {
		smblib_dbg(chg, PR_MISC, "No change in moisture protection status\n");
		return rc;
	}

	if (enable) {
		chg->moisture_present = true;

		/* Disable uUSB factory mode detection */
		rc = smblib_masked_write(chg, TYPEC_U_USB_CFG_REG,
					EN_MICRO_USB_FACTORY_MODE_BIT, 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't disable uUSB factory mode detection rc=%d\n",
				rc);
			return rc;
		}

		/* Disable moisture detection and uUSB state change interrupt */
		rc = smblib_masked_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG,
					TYPEC_WATER_DETECTION_INT_EN_BIT |
					MICRO_USB_STATE_CHANGE_INT_EN_BIT, 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't disable moisture detection interrupt rc=%d\n",
			rc);
			return rc;
		}

		/* Set 1% duty cycle on ID detection */
		rc = smblib_masked_write(chg,
				((chg->chg_param.smb_version == PMI632_SUBTYPE)
				? PMI632_TYPEC_U_USB_WATER_PROTECTION_CFG_REG :
				TYPEC_U_USB_WATER_PROTECTION_CFG_REG),
				EN_MICRO_USB_WATER_PROTECTION_BIT |
				MICRO_USB_DETECTION_ON_TIME_CFG_MASK |
				MICRO_USB_DETECTION_PERIOD_CFG_MASK,
				EN_MICRO_USB_WATER_PROTECTION_BIT |
				MICRO_USB_DETECTION_ON_TIME_20_MS |
				MICRO_USB_DETECTION_PERIOD_X_100);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set 1 percent CC_ID duty cycle rc=%d\n",
				rc);
			return rc;
		}

		vote(chg->usb_icl_votable, MOISTURE_VOTER, true, 0);
	} else {
		chg->moisture_present = false;
		vote(chg->usb_icl_votable, MOISTURE_VOTER, false, 0);

		/* Enable moisture detection and uUSB state change interrupt */
		rc = smblib_masked_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG,
					TYPEC_WATER_DETECTION_INT_EN_BIT |
					MICRO_USB_STATE_CHANGE_INT_EN_BIT,
					TYPEC_WATER_DETECTION_INT_EN_BIT |
					MICRO_USB_STATE_CHANGE_INT_EN_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't enable moisture detection and uUSB state change interrupt rc=%d\n",
				rc);
			return rc;
		}

		/* Disable periodic monitoring of CC_ID pin */
		rc = smblib_write(chg,
				((chg->chg_param.smb_version == PMI632_SUBTYPE)
				? PMI632_TYPEC_U_USB_WATER_PROTECTION_CFG_REG :
				TYPEC_U_USB_WATER_PROTECTION_CFG_REG), 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't disable 1 percent CC_ID duty cycle rc=%d\n",
				rc);
			return rc;
		}

		/* Enable uUSB factory mode detection */
		rc = smblib_masked_write(chg, TYPEC_U_USB_CFG_REG,
					EN_MICRO_USB_FACTORY_MODE_BIT,
					EN_MICRO_USB_FACTORY_MODE_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't disable uUSB factory mode detection rc=%d\n",
				rc);
			return rc;
		}
	}

	smblib_dbg(chg, PR_MISC, "Moisture protection %s\n",
			chg->moisture_present ? "enabled" : "disabled");
	return rc;
}

/*********************
 * VOTABLE CALLBACKS *
 *********************/
static int smblib_smb_disable_override_vote_callback(struct votable *votable,
			void *data, int disable_smb, const char *client)
{
	struct smb_charger *chg = data;
	int rc = 0;

	/* Enable/disable SMB_EN pin */
	rc = smblib_masked_write(chg, MISC_SMB_EN_CMD_REG,
			SMB_EN_OVERRIDE_BIT | SMB_EN_OVERRIDE_VALUE_BIT,
			disable_smb ? SMB_EN_OVERRIDE_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't configure SMB_EN, rc=%d\n", rc);

	return rc;
}

static int smblib_dc_suspend_vote_callback(struct votable *votable, void *data,
			int suspend, const char *client)
{
	struct smb_charger *chg = data;

	if (chg->chg_param.smb_version == PMI632_SUBTYPE)
		return 0;

	/* resume input if suspend is invalid */
	if (suspend < 0)
		suspend = 0;

	return smblib_set_dc_suspend(chg, (bool)suspend);
}

static int smblib_awake_vote_callback(struct votable *votable, void *data,
			int awake, const char *client)
{
	struct smb_charger *chg = data;

	if (awake)
		pm_stay_awake(chg->dev);
	else
		pm_relax(chg->dev);

	return 0;
}

static int smblib_chg_disable_vote_callback(struct votable *votable, void *data,
			int chg_disable, const char *client)
{
	struct smb_charger *chg = data;
	int rc;

	rc = smblib_masked_write(chg, CHARGING_ENABLE_CMD_REG,
				 CHARGING_ENABLE_CMD_BIT,
				 chg_disable ? 0 : CHARGING_ENABLE_CMD_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't %s charging rc=%d\n",
			chg_disable ? "disable" : "enable", rc);
		return rc;
	}

	return 0;
}

static int smblib_hdc_irq_disable_vote_callback(struct votable *votable,
				void *data, int disable, const char *client)
{
	struct smb_charger *chg = data;

	if (!chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq)
		return 0;

	if (chg->irq_info[HIGH_DUTY_CYCLE_IRQ].enabled) {
		if (disable)
			disable_irq_nosync(
				chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq);
	} else {
		if (!disable)
			enable_irq(chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq);
	}

	chg->irq_info[HIGH_DUTY_CYCLE_IRQ].enabled = !disable;

	return 0;
}

static int smblib_limited_irq_disable_vote_callback(struct votable *votable,
				void *data, int disable, const char *client)
{
	struct smb_charger *chg = data;

	if (!chg->irq_info[INPUT_CURRENT_LIMITING_IRQ].irq)
		return 0;

	if (chg->irq_info[INPUT_CURRENT_LIMITING_IRQ].enabled) {
		if (disable)
			disable_irq_nosync(
				chg->irq_info[INPUT_CURRENT_LIMITING_IRQ].irq);
	} else {
		if (!disable)
			enable_irq(
				chg->irq_info[INPUT_CURRENT_LIMITING_IRQ].irq);
	}

	chg->irq_info[INPUT_CURRENT_LIMITING_IRQ].enabled = !disable;

	return 0;
}

static int smblib_icl_irq_disable_vote_callback(struct votable *votable,
				void *data, int disable, const char *client)
{
	struct smb_charger *chg = data;

	if (!chg->irq_info[USBIN_ICL_CHANGE_IRQ].irq)
		return 0;

	if (chg->irq_info[USBIN_ICL_CHANGE_IRQ].enabled) {
		if (disable)
			disable_irq_nosync(
				chg->irq_info[USBIN_ICL_CHANGE_IRQ].irq);
	} else {
		if (!disable)
			enable_irq(chg->irq_info[USBIN_ICL_CHANGE_IRQ].irq);
	}

	chg->irq_info[USBIN_ICL_CHANGE_IRQ].enabled = !disable;

	return 0;
}

static int smblib_temp_change_irq_disable_vote_callback(struct votable *votable,
				void *data, int disable, const char *client)
{
	struct smb_charger *chg = data;

	if (!chg->irq_info[TEMP_CHANGE_IRQ].irq)
		return 0;

	if (chg->irq_info[TEMP_CHANGE_IRQ].enabled && disable) {
		if (chg->irq_info[TEMP_CHANGE_IRQ].wake)
			disable_irq_wake(chg->irq_info[TEMP_CHANGE_IRQ].irq);
		disable_irq_nosync(chg->irq_info[TEMP_CHANGE_IRQ].irq);
	} else if (!chg->irq_info[TEMP_CHANGE_IRQ].enabled && !disable) {
		enable_irq(chg->irq_info[TEMP_CHANGE_IRQ].irq);
		if (chg->irq_info[TEMP_CHANGE_IRQ].wake)
			enable_irq_wake(chg->irq_info[TEMP_CHANGE_IRQ].irq);
	}

	chg->irq_info[TEMP_CHANGE_IRQ].enabled = !disable;

	return 0;
}

/*******************
 * VCONN REGULATOR *
 * *****************/

int smblib_vconn_regulator_enable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;
	u8 stat, orientation;

	smblib_dbg(chg, PR_OTG, "enabling VCONN\n");

	rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n", rc);
		return rc;
	}

	/* VCONN orientation is opposite to that of CC */
	orientation =
		stat & TYPEC_CCOUT_VALUE_BIT ? 0 : VCONN_EN_ORIENTATION_BIT;
	rc = smblib_masked_write(chg, TYPE_C_VCONN_CONTROL_REG,
				VCONN_EN_VALUE_BIT | VCONN_EN_ORIENTATION_BIT,
				VCONN_EN_VALUE_BIT | orientation);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_CCOUT_CONTROL_REG rc=%d\n",
			rc);
		return rc;
	}

	return 0;
}

int smblib_vconn_regulator_disable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;

	smblib_dbg(chg, PR_OTG, "disabling VCONN\n");
	rc = smblib_masked_write(chg, TYPE_C_VCONN_CONTROL_REG,
				 VCONN_EN_VALUE_BIT, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't disable vconn regulator rc=%d\n", rc);

	return 0;
}

int smblib_vconn_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc;
	u8 cmd;

	rc = smblib_read(chg, TYPE_C_VCONN_CONTROL_REG, &cmd);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_INTRPT_ENB_SOFTWARE_CTRL rc=%d\n",
			rc);
		return rc;
	}

	return (cmd & VCONN_EN_VALUE_BIT) ? 1 : 0;
}

/*****************
 * OTG REGULATOR *
 *****************/

int smblib_vbus_regulator_enable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc;

	smblib_dbg(chg, PR_OTG, "enabling OTG\n");

	rc = smblib_masked_write(chg, DCDC_CMD_OTG_REG, OTG_EN_BIT, OTG_EN_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't enable OTG rc=%d\n", rc);
		return rc;
	}

	return 0;
}

int smblib_vbus_regulator_disable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc;

	smblib_dbg(chg, PR_OTG, "disabling OTG\n");

	rc = smblib_masked_write(chg, DCDC_CMD_OTG_REG, OTG_EN_BIT, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't disable OTG regulator rc=%d\n", rc);
		return rc;
	}

	return 0;
}

int smblib_vbus_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;
	u8 cmd;

	rc = smblib_read(chg, DCDC_CMD_OTG_REG, &cmd);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read CMD_OTG rc=%d", rc);
		return rc;
	}

	return (cmd & OTG_EN_BIT) ? 1 : 0;
}

/********************
 * BATT PSY GETTERS *
 ********************/

int smblib_get_prop_input_suspend(struct smb_charger *chg,
				  union power_supply_propval *val)
{
	val->intval
		= (get_client_vote(chg->usb_icl_votable, USER_VOTER) == 0)
		 || get_client_vote(chg->dc_suspend_votable, USER_VOTER);
	return 0;
}

int smblib_get_prop_battery_input_suspend(struct smb_charger *chg,
				  union power_supply_propval *val)
{
	val->intval
		= (get_client_vote(chg->fcc_votable, USER_VOTER) == 0);
	return 0;
}

int smblib_get_prop_batt_present(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, BATIF_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATIF_INT_RT_STS rc=%d\n", rc);
		return rc;
	}

	val->intval = !(stat & (BAT_THERM_OR_ID_MISSING_RT_STS_BIT
					| BAT_TERMINAL_MISSING_RT_STS_BIT));

	return rc;
}

static bool smblib_check_vbat_before_shutdown(struct smb_charger *chg)
{
	int rc;
	int vbat_uv = 0;
	union power_supply_propval pval = {0,};

	rc = smblib_get_batt_voltage_now(chg, &pval);
	if (rc < 0) {
		pr_err("Couldn't get vbat rc=%d\n", rc);
		return false;
	}
	vbat_uv = pval.intval;

	/*
	* if battery soc is 0% but vbat is above 3450mV
	* report 1% to improve user experience
	*/
	if (vbat_uv > (CUTOFF_VOL_THR + CUTOFF_VOL_HYS)
			&& !chg->vbat_critical_low_triggered) {
		return true;
	} else if (!chg->vbat_critical_low_triggered) {
		chg->vbat_critical_low_triggered = true;
		return false;
	} else {
		return false;
	}
}

static void smblib_check_input_status(struct smb_charger *chg)
{
	int rc;
	int input_present = 0, vbat_uv = 0;
	union power_supply_propval pval = {0,};

	rc = smblib_is_input_present(chg, &input_present);
	if (rc < 0)
		return;

	if (input_present == INPUT_NOT_PRESENT)
		return;

	rc = smblib_get_batt_voltage_now(chg, &pval);
	if (rc < 0) {
		pr_err("Couldn't get vbat rc=%d\n", rc);
		return;
	}
	vbat_uv = pval.intval;
	pr_err("vbat_uv: %d\n", vbat_uv);

	/*
	 * if battery soc is 0%, vbat is below 3400mV and input is present in
	 * normal mode (not power-off charging mode), set usb/usb_port/dc
	 * online to false to notify system to power off.
	 */
	if ((input_present & INPUT_PRESENT_DC
			|| input_present & INPUT_PRESENT_USB)
				&& !off_charge_flag
				&& (vbat_uv <= CUTOFF_VOL_THR)) {
		chg->report_input_absent = true;
		power_supply_changed(chg->batt_psy);
	}
}

int smblib_get_prop_batt_capacity(struct smb_charger *chg,
				  union power_supply_propval *val)
{
	int rc = -EINVAL;

	if (chg->fake_capacity >= 0 && chg->fake_capacity <= 100) {
		val->intval = chg->fake_capacity;
		return 0;
	}

	rc = smblib_get_prop_from_bms(chg, POWER_SUPPLY_PROP_CAPACITY, val);

	if (val->intval == 0) {
		smblib_check_input_status(chg);
		if (!chg->report_input_absent
				&& smblib_check_vbat_before_shutdown(chg))
			val->intval = 1;
	}

	return rc;
}
int smblib_get_prop_batt_capacity_level(struct smb_charger *chg,
                                  union power_supply_propval *val)
{
	int rc,cap;
	union power_supply_propval capacity;

	rc = smblib_get_prop_from_bms(chg, POWER_SUPPLY_PROP_CAPACITY, &capacity);

	cap=capacity.intval;
	if (cap == 0)
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	if (cap > 0 && cap <= 20)
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	if (cap > 20 && cap <= 80)
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	if (cap > 80 && cap <= 99)
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	if (cap == 100)
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	return rc;
}

static bool is_charging_paused(struct smb_charger *chg)
{
	int rc;
	u8 val;

	rc = smblib_read(chg, CHARGING_PAUSE_CMD_REG, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read CHARGING_PAUSE_CMD rc=%d\n", rc);
		return false;
	}

	return val & CHARGING_PAUSE_CMD_BIT;
}

int smblib_get_prop_battery_charging_enabled(struct smb_charger *chg,
					union power_supply_propval *val)
{
	int icl = 0;
	if (chg->is_qc_class_a && !chg->qc3_raise_done)
		icl = MAIN_ICL_MIN;

	if (chg->six_pin_step_charge_enable) {
		val->intval = !(get_client_vote(chg->usb_icl_votable, MAIN_ICL_MIN_VOTER)
				== MAIN_ICL_MIN);
	}
	else {
		if (chg->power_good_en)
			val->intval = !(get_client_vote(chg->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER)
					== MAIN_CHG_SUSPEND_ICL);
		else
			val->intval = !(get_client_vote(chg->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER)
					== icl);
	}
	return 0;
}


int smblib_get_prop_batt_status(struct smb_charger *chg,
				union power_supply_propval *val)
{
	union power_supply_propval pval = {0, };
	bool usb_online, dc_online;
	u8 stat;
	int rc, suspend = 0;
	int batt_health_status;
	int chg_en;
	int icl = 0;
	if (chg->is_qc_class_a && !chg->qc3_raise_done)
		icl = MAIN_ICL_MIN;
	if (chg->fake_chg_status_on_debug_batt) {
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_DEBUG_BATTERY, &pval);
		if (rc < 0) {
			pr_err_ratelimited("Couldn't get debug battery prop rc=%d\n",
					rc);
		} else if (pval.intval == 1) {
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
			return 0;
		}
	}
	if (chg->report_input_absent) {
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		return 0;
	}
	if (chg->fake_plug_out == true) {
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		return 0;
	}
	if (chg->ext_bbc) {
		rc = smblib_get_prop_from_bbc(chg,
				POWER_SUPPLY_PROP_STATUS, &pval);
		if (rc < 0)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		val->intval = pval.intval;
		return 0;
	}

	if (chg->use_bq_pump && !chg->six_pin_step_charge_enable
			&& (get_client_vote_locked(chg->usb_icl_votable,
				MAIN_CHG_SUSPEND_VOTER) == icl)) {
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		return 0;
	}

	if (chg->use_bq_pump && chg->six_pin_step_charge_enable
			&& (get_client_vote_locked(chg->usb_icl_votable,
				MAIN_ICL_MIN_VOTER) == MAIN_ICL_MIN)) {
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		return 0;
	}

	if (chg->use_bq_pump && !chg->six_pin_step_charge_enable
			&& is_client_vote_enabled_locked(chg->chg_disable_votable,
				AFTER_FFC_VOTER)) {
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		return 0;
	}
	if (chg->fake_chg_status_on_debug_batt) {
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_DEBUG_BATTERY, &pval);
		if (rc < 0) {
			pr_err_ratelimited("Couldn't get debug battery prop rc=%d\n",
					rc);
		} else if (pval.intval == 1) {
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
			return 0;
		}
	}

	rc = smblib_get_prop_batt_health(chg, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get batt health rc=%d\n", rc);
		return rc;
	}
	/*
	 * The charger status register shows charging even though the battery
	 * is discharging when the over voltage condition is hit. Report power
	 * supply state as NOT_CHARGING when the battery health reports
	 * over voltage.
	 */
	/*
	if (pval.intval == POWER_SUPPLY_HEALTH_OVERVOLTAGE) {
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		return 0;
	}*/
	if (get_client_vote_locked(chg->usb_icl_votable, JEITA_VOTER) == 0)
		/* show charging when JEITA_VOTER 0mA is vote to improve user experience */
		if(pval.intval != POWER_SUPPLY_HEALTH_OVERHEAT
			&& pval.intval != POWER_SUPPLY_HEALTH_COLD) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			return 0;
		}

	if (chg->dbc_usbov) {
		rc = smblib_get_prop_usb_present(chg, &pval);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't get usb present prop rc=%d\n", rc);
			return rc;
		}

		rc = smblib_get_usb_suspend(chg, &suspend);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't get usb suspend rc=%d\n", rc);
			return rc;
		}

		/*
		 * Report charging as long as USBOV is not debounced and
		 * charging path is un-suspended.
		 */
		if (pval.intval && !suspend) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			return 0;
		}
	}

	rc = smblib_get_prop_usb_online(chg, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get usb online property rc=%d\n",
			rc);
		return rc;
	}
	usb_online = (bool)pval.intval;

	rc = smblib_get_prop_dc_online(chg, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get dc online property rc=%d\n",
			rc);
		return rc;
	}
	dc_online = (bool)pval.intval;

	if (!usb_online && !dc_online) {
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		return rc;
	}

	rc = smblib_get_prop_batt_health(chg, &pval);
	if (rc < 0)
		smblib_err(chg, "Couldn't get batt health rc=%d\n", rc);

	batt_health_status = pval.intval;
	if (get_client_vote_locked(chg->usb_icl_votable, JEITA_VOTER) == 0) {
		/* show charging when JEITA_VOTER 0mA is vote to improve user experience */
		if (pval.intval != POWER_SUPPLY_HEALTH_OVERHEAT
					&& pval.intval != POWER_SUPPLY_HEALTH_COLD) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			return 0;
		}
	}

	chg_en = !(get_client_vote_locked(chg->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER)
			== MAIN_CHG_SUSPEND_ICL);

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return rc;
	}
	stat = stat & BATTERY_CHARGER_STATUS_MASK;
	switch (stat) {
	case TRICKLE_CHARGE:
	case PRE_CHARGE:
	case FULLON_CHARGE:
	case TAPER_CHARGE:
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case TERMINATE_CHARGE:
	case INHIBIT_CHARGE:
		if (batt_health_status == POWER_SUPPLY_HEALTH_WARM || chg->capacity <= 98) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else if (!chg_en && chg->power_good_en) {
			smblib_dbg(chg, PR_OEM, "report charging when open 25970\n");
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else {
			val->intval = POWER_SUPPLY_STATUS_FULL;
		}
		break;
	case DISABLE_CHARGE:
	case PAUSE_CHARGE:
		/*
		 * As from jeita status change, there is very short time not charging,
		 * to improve user experience, we report charging at this moment.
		 */
		if (batt_health_status == POWER_SUPPLY_HEALTH_WARM ||
			chg->cp_to_sw_status == true) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else {
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
		break;
	default:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	if (is_charging_paused(chg)) {
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		return 0;
	}

	/*
	 * If charge termination WA is active and has suspended charging, then
	 * continue reporting charging status as FULL.
	 */
	if (is_client_vote_enabled_locked(chg->usb_icl_votable,
						CHG_TERMINATION_VOTER)) {
		val->intval = POWER_SUPPLY_STATUS_FULL;
		return 0;
	}

	if (chg->wireless_bq && chg->fake_dc_on) {
		if ((POWER_SUPPLY_HEALTH_COLD == pval.intval)
			|| (POWER_SUPPLY_HEALTH_OVERHEAT == pval.intval))
			chg->fake_dc_on = 0;
		else if (val->intval != POWER_SUPPLY_STATUS_FULL)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		smblib_dbg(chg, PR_WLS, "fake dc on, battery staus: %d\n", val->intval);
		return 0;
	}

	if (val->intval != POWER_SUPPLY_STATUS_CHARGING)
		return 0;

	if (!usb_online && dc_online
		&& chg->fake_batt_status == POWER_SUPPLY_STATUS_FULL) {
		val->intval = POWER_SUPPLY_STATUS_FULL;
		return 0;
	}

	/*rc = smblib_read(chg, BATTERY_CHARGER_STATUS_5_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
				rc);
			return rc;
	}

	stat &= ENABLE_TRICKLE_BIT | ENABLE_PRE_CHARGING_BIT |
						ENABLE_FULLON_MODE_BIT;

	if (!stat)
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;*/

	return 0;
}

int smblib_get_prop_batt_charge_type(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;
	int icl = 0;
	if (chg->is_qc_class_a && !chg->qc3_raise_done)
		icl = MAIN_ICL_MIN;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return rc;
	}

	if (chg->use_bq_pump && !chg->six_pin_step_charge_enable
			&& (get_client_vote_locked(chg->usb_icl_votable,
				MAIN_CHG_SUSPEND_VOTER) == icl)) {
		val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		return rc;
	}

	switch (stat & BATTERY_CHARGER_STATUS_MASK) {
	case TRICKLE_CHARGE:
	case PRE_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case FULLON_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case TAPER_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_TAPER;
		break;
	default:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	return rc;
}

int smblib_get_prop_batt_health(struct smb_charger *chg,
				union power_supply_propval *val)
{
	union power_supply_propval pval;
	int rc;
	int effective_fv_uv;
	u8 stat;
	int over_voltage_thr_uv;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_2_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
			rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "BATTERY_CHARGER_STATUS_2 = 0x%02x\n",
		   stat);

	if (stat & CHARGER_ERROR_STATUS_BAT_OV_BIT) {
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (!rc) {
			/*
			 * If Vbatt is within 40 or 80mV above Vfloat, then don't
			 * treat it as overvoltage.
			 */
			effective_fv_uv = get_effective_result_locked(chg->fv_votable);
			/*
			 * as six pin battery vbat is much higher than cell voltage
			 * we should add more buffer to over voltage threshold
			 * to report over voltage health
			 */
			if (chg->six_pin_step_charge_enable) {
				if (effective_fv_uv == WARM_VFLOAT_UV)
					over_voltage_thr_uv = effective_fv_uv + 100000;
				else
					over_voltage_thr_uv = chg->batt_profile_fv_uv + 150000;
			} else {
				over_voltage_thr_uv = effective_fv_uv + 40000;
			}
			if (pval.intval >= over_voltage_thr_uv) {
				val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
				smblib_err(chg, "battery over-voltage vbat_fg = %duV, fv = %duV\n",
						pval.intval, effective_fv_uv);
				goto done;
			}
		}
	}

	if (chg->ext_fg || chg->chg_warm_threshold == 480) {
		//do not use default batt temp threshold
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_TEMP, &pval);
		if (rc < 0)
			val->intval = POWER_SUPPLY_HEALTH_GOOD;

		if (pval.intval < BAT_TEMP_COLD)
			val->intval = POWER_SUPPLY_HEALTH_COLD;
		else if (pval.intval < chg->chg_cool_threshold)
			val->intval = POWER_SUPPLY_HEALTH_COOL;
		else if (pval.intval > BAT_TEMP_TOO_HOT)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (pval.intval > chg->chg_warm_threshold)
			val->intval = POWER_SUPPLY_HEALTH_WARM;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
	}else {
		rc = smblib_read(chg, BATTERY_CHARGER_STATUS_7_REG, &stat);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
				rc);
			return rc;
		}
		if (stat & BAT_TEMP_STATUS_TOO_COLD_BIT)
			val->intval = POWER_SUPPLY_HEALTH_COLD;
		else if (stat & BAT_TEMP_STATUS_TOO_HOT_BIT)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (stat & BAT_TEMP_STATUS_COLD_SOFT_BIT)
			val->intval = POWER_SUPPLY_HEALTH_COOL;
		else if (stat & BAT_TEMP_STATUS_HOT_SOFT_BIT)
			val->intval = POWER_SUPPLY_HEALTH_WARM;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
	}

done:
	return rc;
}

int smblib_get_prop_dc_temp_level(struct smb_charger *chg,
				union power_supply_propval *val)
{
	val->intval = chg->dc_temp_level;
	return 0;
}

int smblib_get_prop_system_temp_level(struct smb_charger *chg,
				union power_supply_propval *val)
{
	val->intval = chg->system_temp_level;
	return 0;
}

int smblib_get_prop_system_temp_level_max(struct smb_charger *chg,
				union power_supply_propval *val)
{
	val->intval = chg->thermal_levels;
	return 0;
}

int smblib_get_prop_input_current_limited(struct smb_charger *chg,
				union power_supply_propval *val)
{
	u8 stat;
	int rc;

	if (chg->fake_input_current_limited >= 0) {
		val->intval = chg->fake_input_current_limited;
		return 0;
	}

	rc = smblib_read(chg, AICL_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read AICL_STATUS rc=%d\n", rc);
		return rc;
	}
	val->intval = (stat & SOFT_ILIMIT_BIT) || chg->is_hdc;
	return 0;
}

int smblib_get_prop_batt_iterm(struct smb_charger *chg,
		union power_supply_propval *val)
{
	int rc, temp;
	u8 stat, buf[2];

	/*
	 * Currently, only ADC comparator-based termination is supported,
	 * hence read only the threshold corresponding to ADC source.
	 * Proceed only if CHGR_ITERM_USE_ANALOG_BIT is 0.
	 */
	rc = smblib_read(chg, CHGR_ENG_CHARGING_CFG_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read CHGR_ENG_CHARGING_CFG_REG rc=%d\n",
				rc);
		return rc;
	}

	if (stat & CHGR_ITERM_USE_ANALOG_BIT) {
		val->intval = -EINVAL;
		return 0;
	}

	rc = smblib_batch_read(chg, CHGR_ADC_ITERM_UP_THD_MSB_REG, buf, 2);

	if (rc < 0) {
		smblib_err(chg, "Couldn't read CHGR_ADC_ITERM_UP_THD_MSB_REG rc=%d\n",
				rc);
		return rc;
	}

	temp = buf[1] | (buf[0] << 8);
	temp = sign_extend32(temp, 15);

	if (chg->chg_param.smb_version == PMI632_SUBTYPE)
		temp = DIV_ROUND_CLOSEST(temp * ITERM_LIMITS_PMI632_MA,
					ADC_CHG_ITERM_MASK);
	else
		temp = DIV_ROUND_CLOSEST(temp * ITERM_LIMITS_PM8150B_MA,
					ADC_CHG_ITERM_MASK);

	val->intval = temp;

	return rc;
}

static int smblib_set_wdog_bark_timer(struct smb_charger *chg,
					int wdog_timer)
{
	u8 val = 0;
	int rc;

	val = (ilog2(wdog_timer / 16) << BARK_WDOG_TIMEOUT_SHIFT)
			& BARK_WDOG_TIMEOUT_MASK;
	rc = smblib_masked_write(chg, SNARL_BARK_BITE_WD_CFG_REG,
			BARK_WDOG_TIMEOUT_MASK, val);
	if (rc < 0) {
		pr_err("Couldn't configue WD config rc=%d\n", rc);
		return rc;
	}
	return rc;
}

int smblib_get_prop_batt_charge_done(struct smb_charger *chg,
					union power_supply_propval *val)
{
	union power_supply_propval pval = {0, };
	int rc;
	u8 stat;
	int chg_en;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return rc;
	}

	stat = stat & BATTERY_CHARGER_STATUS_MASK;
	val->intval = (stat == TERMINATE_CHARGE);

	chg_en = !(get_client_vote_locked(chg->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER)
			== MAIN_CHG_SUSPEND_ICL);
	if (!chg_en && chg->power_good_en && val->intval) {
		smblib_dbg(chg, PR_OEM, "force recharge when charge done error\n");
		val->intval = 0;
		pval.intval = true;
		rc = power_supply_set_property(chg->batt_psy,
				POWER_SUPPLY_PROP_FORCE_RECHARGE, &pval);
		if (rc < 0) {
			smblib_dbg(chg, PR_OEM, "could not set force recharging!\n");
			return rc;
		}
		return rc;
	}
	/*  if charge is done, clear CHG_AWAKE_VOTER */
	if (val->intval == 1) {
#ifdef CONFIG_DUAL_FUEL_GAUGE_BQ27Z561
		Dual_Fg_Reset_Batt_Ctrl_gpio_default();
#endif
		rc = smblib_get_prop_batt_capacity(chg, &pval);
		if (rc < 0)
			smblib_err(chg, "Couldn't get batt capacity rc=%d\n", rc);
		if (pval.intval >= 98) {
			/* when charge done, set bark timer to 128s to decrease wakeups */
			smblib_set_wdog_bark_timer(chg, BARK_TIMER_LONG);
			vote(chg->awake_votable, CHG_AWAKE_VOTER, false, 0);
		}

		if (chg->power_good_en) {
			if ((smblib_get_fastcharge_mode(chg) == true)
				&& (pval.intval >= 98))
				smblib_set_fastcharge_mode(chg, false);
				return 0;
		}

		if (smblib_get_fastcharge_mode(chg) == true)
			smblib_set_fastcharge_mode(chg, false);
	}
	return 0;
}

int smblib_get_batt_current_now(struct smb_charger *chg,
					union power_supply_propval *val)
{
	int rc;

	rc = smblib_get_prop_from_bms(chg,
			POWER_SUPPLY_PROP_CURRENT_NOW, val);
	//if (!rc)
	//	val->intval *= (-1);

	return rc;
}

static int smblib_get_batt_voltage_now(struct smb_charger *chg,
					union power_supply_propval *val)
{
	int rc;

	rc = smblib_get_prop_from_bms(chg,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, val);

	return rc;
}

static void smblib_get_start_vbat_before_step_charge(struct smb_charger *chg)
{
	int rc;
	union power_supply_propval pval = {0, };

	rc = smblib_get_batt_voltage_now(chg, &pval);
	if (!rc)
		chg->start_step_vbat = pval.intval;
	else
		pr_err("could not get vbat vol from bms\n");

	pr_err("chg->start_step_vbat: %d\n", chg->start_step_vbat);
}

/***********************
 * BATTERY PSY SETTERS *
 ***********************/

int smblib_set_prop_input_suspend(struct smb_charger *chg,
				  const union power_supply_propval *val)
{
	int rc;

	/* vote 0mA when suspended */
	rc = vote(chg->usb_icl_votable, USER_VOTER, (bool)val->intval, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't vote to %s USB rc=%d\n",
			(bool)val->intval ? "suspend" : "resume", rc);
		return rc;
	}

	rc = vote(chg->dc_suspend_votable, USER_VOTER, (bool)val->intval, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't vote to %s DC rc=%d\n",
			(bool)val->intval ? "suspend" : "resume", rc);
		return rc;
	}

	power_supply_changed(chg->batt_psy);
	return rc;
}

int smblib_set_prop_battery_input_suspend(struct smb_charger *chg,
		const union power_supply_propval *val)
{
	int rc;

	/* vote 0mA when battery suspended */
	rc = vote(chg->fcc_votable, USER_VOTER, (bool)val->intval, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't vote to %s fcc rc=%d\n",
			(bool)val->intval ? "suspend" : "resume", rc);
		return rc;
	}

	power_supply_changed(chg->batt_psy);
	return rc;
}

int smblib_set_prop_batt_capacity(struct smb_charger *chg,
				  const union power_supply_propval *val)
{
	union power_supply_propval shutdown_delay_en = {0, };
	/* only enable write reasonable soc value */
	if (val->intval >= 0 && val->intval <= 100) {
		chg->fake_capacity = val->intval;
		power_supply_changed(chg->batt_psy);
	}

	power_supply_set_property(chg->bms_psy,
				POWER_SUPPLY_PROP_SHUTDOWN_DELAY_ENABLE,
				&shutdown_delay_en);

	smblib_err(chg, "Set capacity:%d, fake_capacity:%d.\n",
			val->intval, chg->fake_capacity);

	return 0;
}

int smblib_set_prop_batt_status(struct smb_charger *chg,
				  const union power_supply_propval *val)
{
	/* Faking battery full */
	if (val->intval == POWER_SUPPLY_STATUS_FULL)
		chg->fake_batt_status = val->intval;
	else
		chg->fake_batt_status = -EINVAL;

	power_supply_changed(chg->batt_psy);

	return 0;
}

static void smblib_wireless_set_rx_sleep_pin(struct smb_charger *chg, int enable)
{
	int rc = 0;
	union power_supply_propval val = {0, };

	chg->idtp_psy = power_supply_get_by_name("idt");
	if (chg->idtp_psy) {
		chg->wls_chip_psy = chg->idtp_psy;
	} else {
		chg->wip_psy = power_supply_get_by_name("rx1619");
		if (chg->wip_psy)
			chg->wls_chip_psy = chg->wip_psy;
		else
			return;
	}
	if (chg->wls_chip_psy) {
		printk("smblib_wireless_set_rx_sleep enable: %d\n", enable);
		val.intval = enable;
		rc = power_supply_set_property(chg->wls_chip_psy,
				POWER_SUPPLY_PROP_PIN_ENABLED, &val);
		if (rc < 0) {
			smblib_err(chg, "Could not set charger control limit =%d\n", rc);
			return;
		}
	}
}


static void smblib_set_wireless_present(struct smb_charger *chg, bool present)
{
	union power_supply_propval pval = {0, };

	chg->idtp_psy = power_supply_get_by_name("idt");
	if (chg->idtp_psy) {
		chg->wls_chip_psy = chg->idtp_psy;
	} else {
		chg->wip_psy = power_supply_get_by_name("rx1619");
		if (chg->wip_psy)
			chg->wls_chip_psy = chg->wip_psy;
		else
			return;
	}

	if (chg->wls_chip_psy) {
		pval.intval = (int)present;
			power_supply_set_property(chg->wls_chip_psy,
					POWER_SUPPLY_PROP_PRESENT, &pval);
	}
}

int smblib_get_prop_wireless_version(struct smb_charger *chg,
				     union power_supply_propval *val)
{
	int rc;

	chg->idtp_psy = power_supply_get_by_name("idt");
	if (chg->idtp_psy) {
		chg->wls_chip_psy = chg->idtp_psy;
	} else {
		chg->wip_psy = power_supply_get_by_name("rx1619");
		if (chg->wip_psy)
			chg->wls_chip_psy = chg->wip_psy;
		else
			return -EINVAL;
	}

	if (chg->wls_chip_psy)
		rc = power_supply_get_property(chg->wls_chip_psy,
				       POWER_SUPPLY_PROP_WIRELESS_VERSION, val);
	return rc;
}

int smblib_get_prop_wireless_fw_version(struct smb_charger *chg,
				     union power_supply_propval *val)
{
	int rc;

	chg->idtp_psy = power_supply_get_by_name("idt");
	if (chg->idtp_psy) {
		chg->wls_chip_psy = chg->idtp_psy;
	} else {
		chg->wip_psy = power_supply_get_by_name("rx1619");
		if (chg->wip_psy)
			chg->wls_chip_psy = chg->wip_psy;
		else
			return -EINVAL;
	}

	if (chg->wls_chip_psy)
		rc = power_supply_get_property(chg->wls_chip_psy,
				       POWER_SUPPLY_PROP_WIRELESS_FW_VERSION, val);
	return rc;
}

static int smblib_dc_therm_charging(struct smb_charger *chg,
					int temp_level)
{
	int thermal_icl_ua = 0;
	int thermal_fcc_ua = 0;
	int rc = 0;
	int is_epp = 0;
	union power_supply_propval pval = {0, };
	union power_supply_propval val = {0, };

	if (!chg->wireless_bq) {
		/*
		 * if use non wireless on bq and usbin, return driectly
		 * and to do later if use usb mid and dcin wireless charge
		 */
		return rc;
	}

	if (!chg->wls_psy) {
		chg->wls_psy = power_supply_get_by_name("wireless");
		if (!chg->wls_psy)
			return -ENODEV;
	}
	rc = power_supply_get_property(chg->wls_psy,
				POWER_SUPPLY_PROP_TX_ADAPTER,
				&pval);

	rc = power_supply_get_property(chg->wls_psy,
				POWER_SUPPLY_PROP_WIRELESS_VERSION,
				&val);

	is_epp = val.intval;

	switch (pval.intval) {
	case ADAPTER_XIAOMI_QC3:
	case ADAPTER_ZIMI_CAR_POWER:
	case ADAPTER_XIAOMI_PD:
		thermal_fcc_ua = chg->thermal_mitigation_dc_20W[temp_level];
		thermal_icl_ua = chg->thermal_mitigation_dc_20W[temp_level];
		break;

	case ADAPTER_XIAOMI_PD_40W:
	case ADAPTER_XIAOMI_PD_50W:
	case ADAPTER_XIAOMI_PD_60W:
	case ADAPTER_XIAOMI_PD_100W:
		thermal_fcc_ua = chg->thermal_mitigation_dc[temp_level];
		thermal_icl_ua = chg->thermal_mitigation_dc[temp_level];
		break;

	case ADAPTER_VOICE_BOX:
		thermal_fcc_ua = chg->thermal_mitigation_voice[temp_level];
		thermal_icl_ua = chg->thermal_mitigation_voice[temp_level];
		break;

	case ADAPTER_QC2:
		thermal_icl_ua = chg->thermal_mitigation_bpp_qc2[temp_level];
		break;

	case ADAPTER_QC3:
		if (is_epp == 1)/*is epp*/
			thermal_icl_ua = chg->thermal_mitigation_epp[temp_level];
		else
			thermal_icl_ua = chg->thermal_mitigation_bpp_qc3[temp_level];
		break;

	case ADAPTER_PD:
		if (is_epp == 1)/*is epp*/
			thermal_icl_ua = chg->thermal_mitigation_epp[temp_level];
		else
			thermal_icl_ua = chg->thermal_mitigation_bpp[temp_level];
		break;

	case ADAPTER_AUTH_FAILED:
		if (is_epp == 1)/*is epp*/
			thermal_icl_ua = chg->thermal_mitigation_epp[temp_level];
		else
			thermal_icl_ua = chg->thermal_mitigation_bpp[temp_level];
		break;

	case ADAPTER_CDP:
	case ADAPTER_DCP:
		thermal_icl_ua = chg->thermal_mitigation_bpp[temp_level];
		break;

	default:
		thermal_icl_ua = chg->thermal_mitigation_bpp[temp_level];
		break;
	}

	if (temp_level == 0) {
		/* if therm_lvl_sel is 0, clear thermal voter */
		rc = vote(chg->usb_icl_votable, THERMAL_DAEMON_VOTER, false, 0);
		rc = vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, false, 0);
	} else {
		if (thermal_icl_ua > 0)
			rc = vote(chg->usb_icl_votable, THERMAL_DAEMON_VOTER,
						true, thermal_icl_ua);

		if (thermal_fcc_ua > 0)
			rc = vote(chg->fcc_votable, THERMAL_DAEMON_VOTER,
						true, thermal_fcc_ua);
	}

	return rc;
}

int smblib_set_prop_dc_temp_level(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	union power_supply_propval dc_present;
	union power_supply_propval batt_temp;
	int rc;

	rc = smblib_get_prop_dc_present(chg, &dc_present);
	if (rc < 0) {
		pr_err("Couldn't get dc present rc=%d\n", rc);
		return -EINVAL;
	}

	rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_TEMP, &batt_temp);
	if (rc < 0) {
		pr_err("Couldn't get batt temp rc=%d\n", rc);
		return -EINVAL;
	}

	if (val->intval < 0)
		return -EINVAL;
	if (chg->dc_thermal_levels <= 0)
		return -EINVAL;
	if (val->intval > chg->dc_thermal_levels)
		return -EINVAL;
	chg->dc_temp_level = val->intval;

#if 0
	if (chg->dc_temp_level >= (chg->dc_thermal_levels - 1))
		return vote(chg->chg_disable_votable,
			THERMAL_DAEMON_VOTER, true, 0);

	vote(chg->chg_disable_votable, THERMAL_DAEMON_VOTER, false, 0);
#endif

	if (chg->power_good_en)
		smblib_dc_therm_charging(chg, val->intval);

	smblib_dbg(chg, PR_OEM,
			"thermal level:%d, batt temp:%d, thermal_levels:%d dc_present=%d\n",
			val->intval, batt_temp.intval, chg->dc_thermal_levels,dc_present.intval);

	return 0;
}

static int smblib_therm_charging(struct smb_charger *chg)
{
	int thermal_icl_ua = 0;
	int thermal_fcc_ua = 0;
	int rc;

	if (chg->system_temp_level >= MAX_TEMP_LEVEL)
		return 0;

	switch (chg->real_charger_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP:
		thermal_icl_ua = chg->thermal_mitigation_qc2[chg->system_temp_level];
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		if (get_client_vote_locked(chg->usb_icl_votable, THERMAL_DAEMON_VOTER) != 0) {
			rc = vote(chg->usb_icl_votable, THERMAL_DAEMON_VOTER, false, 0);
			if (rc < 0)
				pr_err("Couldn't disable USB thermal ICL vote rc=%d\n", rc);
		}

		if (chg->use_bq_pump) {
			if (chg->is_qc_class_a)
				thermal_fcc_ua =
					chg->thermal_fcc_qc3_cp[chg->system_temp_level];
			else
				thermal_fcc_ua =
					chg->thermal_fcc_qc3_classb_cp[chg->system_temp_level];
		} else {
			thermal_fcc_ua =
				chg->thermal_fcc_qc3_normal[chg->system_temp_level];
		}
		thermal_icl_ua = thermal_fcc_ua;
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3P5:
		thermal_fcc_ua =
			chg->thermal_fcc_qc3_cp[chg->system_temp_level];
		break;
	case POWER_SUPPLY_TYPE_USB_PD:
		if (chg->pd_active == POWER_SUPPLY_PD_PPS_ACTIVE) {
			if (chg->pps_thermal_level < 0)
				chg->pps_thermal_level = chg->system_temp_level;
			thermal_fcc_ua =
				chg->thermal_fcc_pps_cp[chg->pps_thermal_level];
		} else {
			thermal_icl_ua =
				chg->thermal_mitigation_icl[chg->system_temp_level];
			thermal_fcc_ua =
				chg->thermal_mitigation_pd_base[chg->system_temp_level];
		}
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
	default:
		thermal_icl_ua = chg->thermal_mitigation_dcp[chg->system_temp_level];
		thermal_fcc_ua = chg->thermal_mitigation[chg->system_temp_level];
		break;
	}

	pr_info("###thermal_icl_ua is %d, chg->system_temp_level: %d, thermal_fcc_ua is %d, charger type = %d\n",
				thermal_icl_ua, chg->system_temp_level, thermal_fcc_ua, chg->real_charger_type);
	if (chg->system_temp_level == 0) {
		/* if therm_lvl_sel is 0, clear thermal voter */
		rc = vote(chg->usb_icl_votable, THERMAL_DAEMON_VOTER, false, 0);
		if (rc < 0)
			pr_err("Couldn't disable USB thermal ICL vote rc=%d\n",
				rc);
		rc = vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, false, 0);
		if (rc < 0)
			pr_err("Couldn't disable USB thermal ICL vote rc=%d\n",
				rc);
	} else {
		pr_info("thermal_icl_ua is %d, chg->system_temp_level: %d, thermal_fcc_ua is %d, charger type = %d\n",
				thermal_icl_ua, chg->system_temp_level, thermal_fcc_ua, chg->real_charger_type);
		if (thermal_icl_ua > 0) {
			rc = vote(chg->usb_icl_votable, THERMAL_DAEMON_VOTER, true,
					thermal_icl_ua);
			if (rc < 0)
				pr_err("Couldn't enable USB thermal ICL vote rc=%d\n",
					rc);
		}
		if (thermal_fcc_ua > 0) {
			rc = vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, true,
					thermal_fcc_ua);
			if (rc < 0)
				pr_err("Couldn't enable USB thermal ICL vote rc=%d\n",
						rc);
		}
	}
	return rc;
}

static void smblib_thermal_setting_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
			thermal_setting_work.work);

	if (chg->pps_thermal_level > chg->system_temp_level) {
		if (chg->pps_thermal_level - chg->system_temp_level >= 2)
			chg->pps_thermal_level -= 2;
		else
			chg->pps_thermal_level -= 1;
	} else if (chg->pps_thermal_level < chg->system_temp_level) {
		if (chg->system_temp_level - chg->pps_thermal_level >= 2)
			chg->pps_thermal_level += 2;
		else
			chg->pps_thermal_level += 1;
	}

	smblib_therm_charging(chg);

	if (chg->pps_thermal_level != chg->system_temp_level)
		schedule_delayed_work(&chg->thermal_setting_work, 3 * HZ);
}

int smblib_set_prop_system_temp_level(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	union power_supply_propval pval = {0, };
	int rc = 0;

	if (val->intval < 0)
		return -EINVAL;

	if (chg->thermal_levels <= 0)
		return -EINVAL;

	if (val->intval > chg->thermal_levels)
		return -EINVAL;

	chg->system_temp_level = val->intval;

	smblib_dbg(chg, PR_OEM, "thermal level:%d, thermal_levels:%d "
			"chg->system_temp_level:%d, charger_type:%d\n",
			val->intval, chg->thermal_levels,
			chg->system_temp_level, chg->real_charger_type);

	/* Check whether USB is online or not */
	rc = power_supply_get_property(chg->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (rc < 0)
		pr_err("Couldn't get USB Online status, rc=%d\n", rc);

	if (!pval.intval) {
		smblib_dbg(chg, PR_OEM, "usb not online, don't set thermal\n");
		return 0;
	}

#if 0
	if (chg->system_temp_level >= (chg->thermal_levels - 1))
		return vote(chg->chg_disable_votable,
			THERMAL_DAEMON_VOTER, true, 0);

	vote(chg->chg_disable_votable, THERMAL_DAEMON_VOTER, false, 0);
#endif

	if (chg->pd_active == POWER_SUPPLY_PD_PPS_ACTIVE)
		schedule_delayed_work(&chg->thermal_setting_work, 3 * HZ);
	else
		smblib_therm_charging(chg);

	return 0;
}

int smblib_set_prop_battery_charging_enabled(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int icl = 0;
	if (chg->is_qc_class_a && !chg->qc3_raise_done)
		icl = MAIN_ICL_MIN;

	if (val->intval == 0) {
		if (chg->six_pin_step_charge_enable) {
			vote(chg->usb_icl_votable, MAIN_ICL_MIN_VOTER,
						true, MAIN_ICL_MIN);
		}
		else {
			if (chg->power_good_en)
				vote(chg->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER,
						true, MAIN_CHG_SUSPEND_ICL);
			else
				vote(chg->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER,
						true, icl);
		}
		
#if (!defined CONFIG_FUEL_GAUGE_BQ27Z561_MUNCH) && (!defined CONFIG_DUAL_FUEL_GAUGE_BQ27Z561)
		schedule_delayed_work(&chg->reduce_fcc_work,
			msecs_to_jiffies(ESR_WORK_TIME_97S));
#endif


	} else {
		if (chg->six_pin_step_charge_enable)
			vote(chg->usb_icl_votable, MAIN_ICL_MIN_VOTER,
						false, 0);
		else
			vote(chg->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER,
						false, 0);
						
#if (!defined CONFIG_FUEL_GAUGE_BQ27Z561_MUNCH) && (!defined CONFIG_DUAL_FUEL_GAUGE_BQ27Z561)
		if (is_client_vote_enabled(chg->fcc_votable, ESR_WORK_VOTER))
			vote(chg->fcc_votable, ESR_WORK_VOTER, false, 0);
		cancel_delayed_work_sync(&chg->reduce_fcc_work);
#endif
	}

	return 0;
}

int smblib_get_prop_type_recheck(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	int status = 0;

	if (chg->recheck_charger)
		status |= BIT(0) << 8;

	status |= chg->precheck_charger_type << 4;
	status |= chg->real_charger_type;

	val->intval = status;

	return 0;
}

int smblib_set_prop_type_recheck(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	if (val->intval == 0) {
		cancel_delayed_work_sync(&chg->charger_type_recheck);
		chg->recheck_charger = false;
	}
	return 0;
}

int smblib_night_charging_func(struct smb_charger *chg,
		union power_supply_propval *val)
{
	union power_supply_propval pval = {0, };
	static int pre_night_chg_flag = 0;
	int capacity, battery_input_suspend;
	int rc = 0;

	rc = smblib_get_prop_batt_capacity(chg, &pval);
	capacity = pval.intval;
	rc = smblib_get_prop_battery_input_suspend(chg, &pval);
	battery_input_suspend = pval.intval;
	if (chg->night_chg_flag) {
		pr_info("nchg_func:capacity:%d, battery_input_suspend:%d.\n",
				capacity, battery_input_suspend);
		pr_info("nchg_func:pre_nchg:%d, nchg:%d.\n",
				pre_night_chg_flag, chg->night_chg_flag);
	}
	if (pre_night_chg_flag != chg->night_chg_flag) {
		if (chg->night_chg_flag && capacity >= 80) {
			pval.intval = 1;
			rc = smblib_set_prop_battery_input_suspend(chg, &pval);
			pr_err("nchg_func:opne night charging.\n");
			pre_night_chg_flag = chg->night_chg_flag;
		}
	}

	if (battery_input_suspend &&
			(!chg->night_chg_flag || capacity <= 75)) {
		pval.intval = 0;
		smblib_set_prop_battery_input_suspend(chg, &pval);
		pr_err("nchg_func:close night charging.\n");
		pre_night_chg_flag = 0;
	}

	val->intval = chg->night_chg_flag;

	return 0;
}

#define PERIPHERAL_MASK		0xFF
static u16 peripheral_base;
static char log[256] = "";
static char version[8] = "smb:01:";
static inline void dump_reg(struct smb_charger *chg, u16 addr,
		const char *name)
{
	u8 reg;
	int rc;
	char reg_data[50] = "";

	if (NULL == name) {
		strlcat(log, "\n", sizeof(log));
		printk(log);
		return;
	}

	rc = smblib_read(chg, addr, &reg);
	if (rc < 0)
		smblib_err(chg, "Couldn't read OTG status rc=%d\n", rc);
	/* print one peripheral base registers in one line */
	if (peripheral_base != (addr & ~PERIPHERAL_MASK)) {
		peripheral_base = addr & ~PERIPHERAL_MASK;
		memset(log, 0, sizeof(log));
		snprintf(reg_data, sizeof(reg_data), "%s%04x ", version, peripheral_base);
		strlcat(log, reg_data, sizeof(log));
	}
	memset(reg_data, 0, sizeof(reg_data));
	snprintf(reg_data, sizeof(reg_data), "%02x ", reg);
	strlcat(log, reg_data, sizeof(log));

	smblib_dbg(chg, PR_REGISTER, "%s - %04X = %02X\n",
							name, addr, reg);
}

static void dump_regs(struct smb_charger *chg)
{
	u16 addr;

	/* charger peripheral */
	for (addr = 0x6; addr <= 0xE; addr++)
		dump_reg(chg, CHGR_BASE + addr, "CHGR Status");

	for (addr = 0x10; addr <= 0x1B; addr++)
		dump_reg(chg, CHGR_BASE + addr, "CHGR INT");

	for (addr = 0x50; addr <= 0x70; addr++)
		dump_reg(chg, CHGR_BASE + addr, "CHGR Config");

	dump_reg(chg, CHGR_BASE + addr, NULL);

	for (addr = 0x10; addr <= 0x1B; addr++)
		dump_reg(chg, BATIF_BASE + addr, "BATIF INT");

	for (addr = 0x50; addr <= 0x52; addr++)
		dump_reg(chg, BATIF_BASE + addr, "BATIF Config");

	for (addr = 0x60; addr <= 0x62; addr++)
		dump_reg(chg, BATIF_BASE + addr, "BATIF Config");

	for (addr = 0x70; addr <= 0x71; addr++)
		dump_reg(chg, BATIF_BASE + addr, "BATIF Config");

	dump_reg(chg, BATIF_BASE + addr, NULL);

	for (addr = 0x6; addr <= 0x10; addr++)
		dump_reg(chg, USBIN_BASE + addr, "USBIN Status");

	for (addr = 0x12; addr <= 0x19; addr++)
		dump_reg(chg, USBIN_BASE + addr, "USBIN INT ");

	for (addr = 0x40; addr <= 0x43; addr++)
		dump_reg(chg, USBIN_BASE + addr, "USBIN Cmd ");

	for (addr = 0x58; addr <= 0x70; addr++)
		dump_reg(chg, USBIN_BASE + addr, "USBIN Config ");

	for (addr = 0x80; addr <= 0x84; addr++)
		dump_reg(chg, USBIN_BASE + addr, "USBIN Config ");

	dump_reg(chg, USBIN_BASE + addr, NULL);

	for (addr = 0x06; addr <= 0x1B; addr++)
		dump_reg(chg, TYPEC_BASE + addr, "TYPEC Status");

	for (addr = 0x42; addr <= 0x72; addr++)
		dump_reg(chg, TYPEC_BASE + addr, "TYPEC Config");

	dump_reg(chg, TYPEC_BASE + 0x44, "TYPEC MODE CFG");
	dump_reg(chg, TYPEC_BASE + addr, NULL);

	for (addr = 0x6; addr <= 0x10; addr++)
		dump_reg(chg, MISC_BASE + addr, "MISC Status");

	for (addr = 0x15; addr <= 0x1B; addr++)
		dump_reg(chg, MISC_BASE + addr, "MISC INT");

	for (addr = 0x51; addr <= 0x62; addr++)
		dump_reg(chg, MISC_BASE + addr, "MISC Config");

	for (addr = 0x70; addr <= 0x76; addr++)
		dump_reg(chg, MISC_BASE + addr, "MISC Config");

	for (addr = 0x80; addr <= 0x84; addr++)
		dump_reg(chg, MISC_BASE + addr, "MISC Config");

	for (addr = 0x90; addr <= 0x94; addr++)
		dump_reg(chg, MISC_BASE + addr, "MISC Config");

	dump_reg(chg, MISC_BASE + addr, NULL);
}

#define CHARGING_PERIOD_S		20
#define NOT_CHARGING_PERIOD_S		60
static void smblib_reg_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							reg_work.work);
	int rc, usb_present;
	union power_supply_propval val;
	int icl_settle, usb_cur_in, usb_vol_in, icl_sts;
	int charger_type, typec_mode, typec_orientation, esr_uohms_nominal, esr_uohms_actual, resistance;
	power_supply_get_property(chg->usb_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW,
				&val);
	usb_vol_in = val.intval;
	usb_vol_in = usb_vol_in/1000;
	dump_regs(chg);
	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0||usb_vol_in < 1000) {
		pr_err("Couldn't get usb present rc=%d\n", rc);
		schedule_delayed_work(&chg->reg_work,
				NOT_CHARGING_PERIOD_S * HZ);
		return;
	}
	usb_present = val.intval;

	smblib_dbg(chg, PR_OEM, "AWAKE vote value is %d voted by %s\n",
					get_effective_result(chg->awake_votable),
					get_effective_client(chg->awake_votable));

	if (usb_present||usb_vol_in > 4000) {
		smblib_dbg(chg, PR_OEM, "ICL vote value is %d voted by %s\n",
					get_effective_result(chg->usb_icl_votable),
					get_effective_client(chg->usb_icl_votable));
		smblib_dbg(chg, PR_OEM, "FCC vote value is %d voted by %s\n",
					get_effective_result(chg->fcc_votable),
					get_effective_client(chg->fcc_votable));
		smblib_dbg(chg, PR_OEM, "FV vote value is %d voted by %s\n",
					get_effective_result(chg->fv_votable),
					get_effective_client(chg->fv_votable));

		smblib_dbg(chg, PR_OEM, "dc_suspend vote value is %d voted by %s\n",
					get_effective_result(chg->dc_suspend_votable),
					get_effective_client(chg->dc_suspend_votable));
		smblib_dbg(chg, PR_OEM, "fcc_main vote value is %d voted by %s\n",
					get_effective_result(chg->fcc_main_votable),
					get_effective_client(chg->fcc_main_votable));
		smblib_dbg(chg, PR_OEM, "awake vote value is %d voted by %s\n",
					get_effective_result(chg->awake_votable),
					get_effective_client(chg->awake_votable));

		smblib_dbg(chg, PR_OEM, "pl_disable vote value is %d voted by %s\n",
					get_effective_result(chg->pl_disable_votable),
					get_effective_client(chg->pl_disable_votable));
		smblib_dbg(chg, PR_OEM, "chg_disable vote value is %d voted by %s\n",
					get_effective_result(chg->chg_disable_votable),
					get_effective_client(chg->chg_disable_votable));
		smblib_dbg(chg, PR_OEM, "pl_enable_votable vote value is %d voted by %s\n",
					get_effective_result(chg->pl_enable_votable_indirect),
					get_effective_client(chg->pl_enable_votable_indirect));

		smblib_dbg(chg, PR_OEM, "cp_disable vote value is %d voted by %s\n",
					get_effective_result(chg->cp_disable_votable),
					get_effective_client(chg->cp_disable_votable));
		smblib_dbg(chg, PR_OEM, "cp_ilim vote value is %d voted by %s\n",
					get_effective_result(chg->cp_ilim_votable),
					get_effective_client(chg->cp_ilim_votable));
		smblib_dbg(chg, PR_OEM, "smb_override vote value is %d voted by %s\n",
					get_effective_result(chg->smb_override_votable),
					get_effective_client(chg->smb_override_votable));

		smblib_dbg(chg, PR_OEM, "icl_irq_disable vote value is %d voted by %s\n",
					get_effective_result(chg->icl_irq_disable_votable),
					get_effective_client(chg->icl_irq_disable_votable));
		smblib_dbg(chg, PR_OEM, "limited_irq_disable vote value is %d voted by %s\n",
					get_effective_result(chg->limited_irq_disable_votable),
					get_effective_client(chg->limited_irq_disable_votable));
		smblib_dbg(chg, PR_OEM, "hdc_irq_disable vote value is %d voted by %s\n",
					get_effective_result(chg->hdc_irq_disable_votable),
					get_effective_client(chg->hdc_irq_disable_votable));

		smblib_dbg(chg, PR_OEM, "temp_change_irq_disable vote value is %d voted by %s\n",
					get_effective_result(chg->temp_change_irq_disable_votable),
					get_effective_client(chg->temp_change_irq_disable_votable));
		smblib_dbg(chg, PR_OEM, "qnovo_disable vote value is %d voted by %s\n",
					get_effective_result(chg->qnovo_disable_votable),
					get_effective_client(chg->qnovo_disable_votable));
		power_supply_get_property(chg->usb_psy,
					POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
					&val);
		usb_cur_in = val.intval;
		power_supply_get_property(chg->usb_psy,
					POWER_SUPPLY_PROP_CURRENT_MAX,
					&val);
		icl_settle = val.intval;

		power_supply_get_property(chg->usb_psy,
					POWER_SUPPLY_PROP_REAL_TYPE,
					&val);
		charger_type = val.intval;

		power_supply_get_property(chg->usb_psy,
					POWER_SUPPLY_PROP_TYPEC_MODE,
					&val);
		typec_mode = val.intval;

		power_supply_get_property(chg->usb_psy,
					POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
					&val);
		typec_orientation = val.intval;

		power_supply_get_property(chg->bms_psy,
					POWER_SUPPLY_PROP_RESISTANCE,
					&val);
		resistance= val.intval;

		power_supply_get_property(chg->bms_psy,
					POWER_SUPPLY_PROP_ESR_NOMINAL,
					&val);
		esr_uohms_nominal= val.intval;

		power_supply_get_property(chg->bms_psy,
                                        POWER_SUPPLY_PROP_ESR_ACTUAL,
                                        &val);
                esr_uohms_actual= val.intval;

		smblib_dbg(chg, PR_OEM,	"ICL settle value[%d], usbin adc current[%d], vbusin adc vol[%d]\n, "
					"resistance [%d], esr_nominal [%d], esr_actual [%d]\n",icl_settle, usb_cur_in,
					 usb_vol_in, resistance, esr_uohms_nominal, esr_uohms_actual);
		if (!chg->usb_main_psy) {
			chg->usb_main_psy = power_supply_get_by_name("main");
		} else {
			power_supply_get_property(chg->usb_main_psy,
					POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
					&val);
			icl_sts = val.intval;
			smblib_dbg(chg, PR_OEM, "AICL_STS[%d]\n", icl_sts);
		}

		smblib_dbg(chg, PR_OEM,
					"Type-C orientation[%d], Type-C mode[%d], Real Charger Type[%d]\n",
					typec_orientation, typec_mode, charger_type);

		schedule_delayed_work(&chg->reg_work,
				CHARGING_PERIOD_S * HZ);
	} else {
		schedule_delayed_work(&chg->reg_work,
				NOT_CHARGING_PERIOD_S * HZ);
	}
}

int smblib_set_prop_input_current_limited(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	chg->fake_input_current_limited = val->intval;
	return 0;
}

int smblib_set_prop_rechg_soc_thresh(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc;
	u8 new_thr = DIV_ROUND_CLOSEST(val->intval * 255, 100);

	/*
	 * As DIV_ROUND_CLOSEST cal cause new_thr to 252, we add 1 more to
	 * improve recharging UI soc still to 100% to improve user experience.
	 */
	if (val->intval == RECHARGE_SOC_THR)
		new_thr += 1;

	rc = smblib_write(chg, CHARGE_RCHG_SOC_THRESHOLD_CFG_REG,
			new_thr);
	if (rc < 0) {
		smblib_err(chg, "Couldn't write to RCHG_SOC_THRESHOLD_CFG_REG rc=%d\n",
				rc);
		return rc;
	}

	chg->auto_recharge_soc = val->intval;

	return rc;
}

int smblib_run_aicl(struct smb_charger *chg, int type)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n",
								rc);
		return rc;
	}

	/* USB is suspended so skip re-running AICL */
	if (stat & USBIN_SUSPEND_STS_BIT)
		return rc;

	smblib_dbg(chg, PR_MISC, "re-running AICL\n");

	stat = (type == RERUN_AICL) ? RERUN_AICL_BIT : RESTART_AICL_BIT;
	rc = smblib_masked_write(chg, AICL_CMD_REG, stat, stat);
	if (rc < 0)
		smblib_err(chg, "Couldn't write to AICL_CMD_REG rc=%d\n",
				rc);
	return 0;
}

static int smblib_dp_pulse(struct smb_charger *chg)
{
	int rc;

	/* QC 3.0 increment */
	rc = smblib_masked_write(chg, CMD_HVDCP_2_REG, SINGLE_INCREMENT_BIT,
			SINGLE_INCREMENT_BIT);
	if (rc < 0)
		smblib_err(chg, "Couldn't write to CMD_HVDCP_2_REG rc=%d\n",
				rc);

	return rc;
}

static int smblib_dm_pulse(struct smb_charger *chg)
{
	int rc;

	/* QC 3.0 decrement */
	rc = smblib_masked_write(chg, CMD_HVDCP_2_REG, SINGLE_DECREMENT_BIT,
			SINGLE_DECREMENT_BIT);
	if (rc < 0)
		smblib_err(chg, "Couldn't write to CMD_HVDCP_2_REG rc=%d\n",
				rc);

	return rc;
}

int smblib_force_vbus_voltage(struct smb_charger *chg, u8 val)
{
	int rc;

	rc = smblib_masked_write(chg, CMD_HVDCP_2_REG, val, val);
	if (rc < 0)
		smblib_err(chg, "Couldn't write to CMD_HVDCP_2_REG rc=%d\n",
				rc);

	return rc;
}

static void smblib_hvdcp_set_fsw(struct smb_charger *chg, int bit)
{
	switch (bit) {
	case QC_5V_BIT:
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_5V);
		break;
	case QC_9V_BIT:
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_9V);
		break;
	case QC_12V_BIT:
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_12V);
		break;
	default:
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_removal);
		break;
	}
}

#define QC3_PULSES_FOR_6V	5
#define QC3_PULSES_FOR_9V	20
#define QC3_PULSES_FOR_12V	35
static int smblib_hvdcp3_set_fsw(struct smb_charger *chg)
{
	int pulse_count, rc;

	rc = smblib_get_pulse_cnt(chg, &pulse_count);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read QC_PULSE_COUNT rc=%d\n", rc);
		return rc;
	}

	if (pulse_count < QC3_PULSES_FOR_6V)
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_5V);
	else if (pulse_count < QC3_PULSES_FOR_9V)
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_6V_8V);
	else if (pulse_count < QC3_PULSES_FOR_12V)
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_9V);
	else
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_12V);

	return 0;
}

static void smblib_hvdcp_adaptive_voltage_change(struct smb_charger *chg)
{
	int rc;
	u8 stat;

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP) {
		if (chg->qc2_unsupported) {
			smblib_hvdcp_set_fsw(chg, QC_5V_BIT);
			power_supply_changed(chg->usb_main_psy);
			return;
		}
		rc = smblib_read(chg, QC_CHANGE_STATUS_REG, &stat);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't read QC_CHANGE_STATUS rc=%d\n", rc);
			return;
		}

		smblib_hvdcp_set_fsw(chg, stat & QC_2P0_STATUS_MASK);
		vote(chg->usb_icl_votable, HVDCP2_ICL_VOTER, false, 0);
	}

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP_3
		|| chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP_3P5) {
		/* we set hvdcp3 fsw in smblib_raise_qc3_vbus_work
		rc = smblib_hvdcp3_set_fsw(chg);
		if (rc < 0)
			smblib_err(chg, "Couldn't set QC3.0 Fsw rc=%d\n", rc);
		*/
	}

	power_supply_changed(chg->usb_main_psy);
}

int smblib_dp_dm(struct smb_charger *chg, int val)
{
	int target_icl_ua, rc = 0;
	union power_supply_propval pval;
	u8 stat;

	if (chg->use_bq_pump) {
		pr_info("dp_dm is controled by our self\n");
		return rc;
	}
	/* if raise_vbus work is running, ignore dp_dm pulses */
	if (chg->raise_vbus_to_detect)
		return rc;

	switch (val) {
	case POWER_SUPPLY_DP_DM_DP_PULSE:
		 /*
		 * if hvdcp_opti wrongly send more than 30 dp pulse(11V) to smb5,
		 * ignore them to allow maxium vbus as 11V, as charge pump do not
		 * need the vin more than 11V, and protect the device.
		 */
		if (chg->pulse_cnt > MAX_PLUSE_COUNT_ALLOWED)
			return rc;
		/*
		 * Pre-emptively increment pulse count to enable the setting
		 * of FSW prior to increasing voltage.
		 */
		chg->pulse_cnt++;

		rc = smblib_hvdcp3_set_fsw(chg);
		if (rc < 0)
			smblib_err(chg, "Couldn't set QC3.0 Fsw rc=%d\n", rc);

		rc = smblib_dp_pulse(chg);
		if (rc < 0) {
			smblib_err(chg, "Couldn't increase pulse count rc=%d\n",
				rc);
			/*
			 * Increment pulse count failed;
			 * reset to former value.
			 */
			chg->pulse_cnt--;
		}

		smblib_dbg(chg, PR_PARALLEL, "DP_DM_DP_PULSE rc=%d cnt=%d\n",
				rc, chg->pulse_cnt);
		/*
		 * if use class_a qc, and slave ic is charge pump, should limit
		 * maxium icl to 1.9A, as VBUS will raise to about 9V to 9.8V,
		 * so we set icl to 1.9A when vbus raise above 7.4V
		 */
		if (chg->is_qc_class_a && chg->sec_cp_present) {
			if (chg->pulse_cnt >= HIGH_NUM_PULSE_THR
					 && !chg->high_vbus_detected) {
				vote(chg->usb_icl_votable, QC_A_CP_ICL_MAX_VOTER, true,
					HVDCP_CLASS_A_FOR_CP_UA);
				chg->high_vbus_detected = true;
			}
		}
		break;
	case POWER_SUPPLY_DP_DM_DM_PULSE:
		rc = smblib_dm_pulse(chg);
		if (!rc && chg->pulse_cnt)
			chg->pulse_cnt--;
		smblib_dbg(chg, PR_PARALLEL, "DP_DM_DM_PULSE rc=%d cnt=%d\n",
				rc, chg->pulse_cnt);
		/*
		 * if use class_a qc, and slave ic is charge pump, should restore
		 * icl to 2.8A when charge pump is not working for class_a qc
		 */
		if (chg->is_qc_class_a && chg->sec_cp_present) {
			if (chg->pulse_cnt < HIGH_NUM_PULSE_THR
					 && chg->high_vbus_detected) {
				vote(chg->usb_icl_votable, QC_A_CP_ICL_MAX_VOTER, false,
					0);
				chg->high_vbus_detected = false;
			}
		}
		break;
	case POWER_SUPPLY_DP_DM_ICL_DOWN:
		target_icl_ua = get_effective_result(chg->usb_icl_votable);
		if (target_icl_ua < 0) {
			/* no client vote, get the ICL from charger */
			rc = power_supply_get_property(chg->usb_psy,
					POWER_SUPPLY_PROP_HW_CURRENT_MAX,
					&pval);
			if (rc < 0) {
				smblib_err(chg, "Couldn't get max curr rc=%d\n",
					rc);
				return rc;
			}
			target_icl_ua = pval.intval;
		}

		/*
		 * Check if any other voter voted on USB_ICL in case of
		 * voter other than SW_QC3_VOTER reset and restart reduction
		 * again.
		 */
		if (target_icl_ua != get_client_vote(chg->usb_icl_votable,
							SW_QC3_VOTER))
			chg->usb_icl_delta_ua = 0;

		chg->usb_icl_delta_ua += 100000;
		vote(chg->usb_icl_votable, SW_QC3_VOTER, true,
						target_icl_ua - 100000);
		smblib_dbg(chg, PR_PARALLEL, "ICL DOWN ICL=%d reduction=%d\n",
				target_icl_ua, chg->usb_icl_delta_ua);
		break;
	case POWER_SUPPLY_DP_DM_FORCE_5V:
		rc = smblib_force_vbus_voltage(chg, FORCE_5V_BIT);
		if (rc < 0)
			pr_err("Failed to force 5V\n");
		break;
	case POWER_SUPPLY_DP_DM_FORCE_9V:
		/* we use our own qc2 method to raise to 9V, so just return here */
		return 0;
		if (chg->qc2_unsupported_voltage == QC2_NON_COMPLIANT_9V) {
			smblib_err(chg, "Couldn't set 9V: unsupported\n");
			return -EINVAL;
		}

		/* If we are increasing voltage to get to 9V, set FSW first */
		rc = smblib_read(chg, QC_CHANGE_STATUS_REG, &stat);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read QC_CHANGE_STATUS_REG rc=%d\n",
					rc);
			break;
		}

		if (stat & QC_5V_BIT) {
			/* Force 1A ICL before requesting higher voltage */
			vote(chg->usb_icl_votable, HVDCP2_ICL_VOTER,
					true, 1000000);
			smblib_hvdcp_set_fsw(chg, QC_9V_BIT);
		}

		rc = smblib_force_vbus_voltage(chg, FORCE_9V_BIT);
		if (rc < 0)
			pr_err("Failed to force 9V\n");
		break;
	case POWER_SUPPLY_DP_DM_FORCE_12V:
		/* we use our own qc2 method to raise to 12V, so just return here */
		return 0;
		if (chg->qc2_unsupported_voltage == QC2_NON_COMPLIANT_12V) {
			smblib_err(chg, "Couldn't set 12V: unsupported\n");
			return -EINVAL;
		}

		/* If we are increasing voltage to get to 12V, set FSW first */
		rc = smblib_read(chg, QC_CHANGE_STATUS_REG, &stat);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read QC_CHANGE_STATUS_REG rc=%d\n",
					rc);
			break;
		}

		if ((stat & QC_9V_BIT) || (stat & QC_5V_BIT)) {
			/* Force 1A ICL before requesting higher voltage */
			vote(chg->usb_icl_votable, HVDCP2_ICL_VOTER,
					true, 1000000);
			smblib_hvdcp_set_fsw(chg, QC_12V_BIT);
		}

		rc = smblib_force_vbus_voltage(chg, FORCE_12V_BIT);
		if (rc < 0)
			pr_err("Failed to force 12V\n");
		break;
	case POWER_SUPPLY_DP_DM_CONFIRMED_HVDCP3P5:
		chg->qc3p5_detected = true;
		smblib_update_usb_type(chg);
		break;
	case POWER_SUPPLY_DP_DM_ICL_UP:
	default:
		break;
	}

	return rc;
}

int smblib_dp_dm_bq(struct smb_charger *chg, int val)
{
	int rc = 0, qc3p5_dp_cnt = 0;

	/* if raise_vbus work is running, ignore dp_dm pulses */
	if (chg->raise_vbus_to_detect)
		return rc;

	switch (val) {
	case POWER_SUPPLY_DP_DM_DP_PULSE:
		/*
		 * if hvdcp_opti wrongly send more than 30 dp pulse(11V) to smb5,
		 * ignore them to allow maxium vbus as 11V, as charge pump do not
		 * need the vin more than 11V, and protect the device.
		 */
		if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP_3
				&& chg->pulse_cnt > MAX_PLUSE_COUNT_ALLOWED) {
			return rc;
		} else if (chg->qc3p5_supported
				&& chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP_3P5) {
			if (chg->pulse_cnt > MAX_QC3P5_PLUSE_COUNT_ALLOWED)
				return rc;
			else if (chg->pulse_cnt >= QC3P5_DP_RAPIDLY_TUNE_ALLOWED)
				chg->qc3p5_dp_tune_rapidly = false;
			else
				chg->qc3p5_dp_tune_rapidly = true;
		}

		/*
		 * Pre-emptively increment pulse count to enable the setting
		 * of FSW prior to increasing voltage.
		 */
		if (chg->qc3p5_dp_tune_rapidly)
			chg->pulse_cnt += QC3P5_DP_RAPIDLY_TUNE_PULSE;
		else
			chg->pulse_cnt++;

		rc = smblib_hvdcp3_set_fsw(chg);
		if (rc < 0)
			smblib_err(chg, "Couldn't set QC3.0 Fsw rc=%d\n", rc);

		if (chg->qc3p5_dp_tune_rapidly) {
			for (qc3p5_dp_cnt = 1; qc3p5_dp_cnt <= QC3P5_DP_RAPIDLY_TUNE_PULSE; qc3p5_dp_cnt++) {
				rc = smblib_dp_pulse(chg);
				if (rc < 0) {
					smblib_err(chg, "Couldn't increase pulse count rc=%d\n", rc);
					chg->pulse_cnt--;
				}
				smblib_dbg(chg, PR_PARALLEL, "DP_DM_DP_PULSE rapidly rc=%d cnt=%d\n",
						rc, chg->pulse_cnt - QC3P5_DP_RAPIDLY_TUNE_PULSE + qc3p5_dp_cnt);
				usleep_range(8000, 8010);	// delay 8ms
			}
		} else {
			rc = smblib_dp_pulse(chg);
			if (rc < 0) {
				smblib_err(chg, "Couldn't increase pulse count rc=%d\n",
						rc);
				/*
				 * Increment pulse count failed;
				 * reset to former value.
				 */
				chg->pulse_cnt--;
			}

			smblib_dbg(chg, PR_PARALLEL, "DP_DM_DP_PULSE rc=%d cnt=%d\n",
					rc, chg->pulse_cnt);
		}
		/*
		 * if use class_a qc, and slave ic is charge pump, should limit
		 * maxium icl to 1.9A, as VBUS will raise to about 9V to 9.8V,
		 * so we set icl to 1.9A when vbus raise above 7.4V
		 */
		if (chg->is_qc_class_a && chg->use_bq_pump) {
			if (chg->pulse_cnt >= HIGH_NUM_PULSE_THR
					 && !chg->high_vbus_detected) {
				vote(chg->usb_icl_votable, QC_A_CP_ICL_MAX_VOTER, true,
					HVDCP_CLASS_A_FOR_CP_UA);
				chg->high_vbus_detected = true;
			}
		}
		break;

	case POWER_SUPPLY_DP_DM_DM_PULSE:
		rc = smblib_dm_pulse(chg);
		if (!rc && chg->pulse_cnt)
			chg->pulse_cnt--;
		smblib_dbg(chg, PR_PARALLEL, "DP_DM_DM_PULSE rc=%d cnt=%d\n",
				rc, chg->pulse_cnt);
		/*
		 * if use class_a qc, and slave ic is charge pump, should restore
		 * icl to 2.8A when charge pump is not working for class_a qc
		 */
		if (chg->is_qc_class_a && chg->use_bq_pump) {
			if (chg->pulse_cnt < HIGH_NUM_PULSE_THR
					 && chg->high_vbus_detected) {
				vote(chg->usb_icl_votable, QC_A_CP_ICL_MAX_VOTER, false,
					0);
				chg->high_vbus_detected = false;
			}
		}
		break;

	case POWER_SUPPLY_DP_DM_FORCE_5V:
		rc = smblib_force_vbus_voltage(chg, FORCE_5V_BIT);
		chg->pulse_cnt = 0;
		if (rc < 0)
			pr_err("Failed to force 5V\n");
		break;
	case POWER_SUPPLY_DP_DM_RAISE_VOLT_DONE:
		chg->qc3_raise_done = true;
		break;
	case POWER_SUPPLY_DP_DM_RAISE_VOLT_START:
		chg->qc3_raise_done = false;
		break;
	default:
		break;
	}

	return rc;
}

int smblib_disable_hw_jeita(struct smb_charger *chg, bool disable)
{
	int rc;
	u8 mask;

	/*
	 * Disable h/w base JEITA compensation if s/w JEITA is enabled
	 */
	 /*J1/K81 use ti gauge disable all hard jeita, J2 use qcom default jeita */
#if (defined CONFIG_FUEL_GAUGE_BQ27Z561_MUNCH) || (defined CONFIG_DUAL_FUEL_GAUGE_BQ27Z561)
	mask = 0xFF;
	pr_info("should disable hw jeita");
#else
	mask = JEITA_EN_COLD_SL_FCV_BIT
		| JEITA_EN_HOT_SL_FCV_BIT
		| JEITA_EN_HOT_SL_CCC_BIT
		| JEITA_EN_COLD_SL_CCC_BIT,
#endif
	rc = smblib_masked_write(chg, JEITA_EN_CFG_REG, mask,
			disable ? 0 : mask);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure s/w jeita rc=%d\n",
				rc);
		return rc;
	}

	return 0;
}

static int smblib_set_sw_thermal_regulation(struct smb_charger *chg,
						bool enable)
{
	int rc = 0;

	if (!(chg->wa_flags & SW_THERM_REGULATION_WA))
		return rc;

	if (enable) {
		/*
		 * Configure min time to quickly address thermal
		 * condition.
		 */
		rc = smblib_masked_write(chg, SNARL_BARK_BITE_WD_CFG_REG,
			SNARL_WDOG_TIMEOUT_MASK, SNARL_WDOG_TMOUT_62P5MS);
		if (rc < 0) {
			smblib_err(chg, "Couldn't configure snarl wdog tmout, rc=%d\n",
					rc);
			return rc;
		}

		/*
		 * Schedule SW_THERM_REGULATION_WORK directly if USB input
		 * is suspended due to SW thermal regulation WA since WDOG
		 * IRQ won't trigger with input suspended.
		 */
		if (is_client_vote_enabled(chg->usb_icl_votable,
						SW_THERM_REGULATION_VOTER)) {
			vote(chg->awake_votable, SW_THERM_REGULATION_VOTER,
								true, 0);
			schedule_delayed_work(&chg->thermal_regulation_work, 0);
		}
	} else {
		cancel_delayed_work_sync(&chg->thermal_regulation_work);
		vote(chg->awake_votable, SW_THERM_REGULATION_VOTER, false, 0);
	}

	smblib_dbg(chg, PR_MISC, "WDOG SNARL INT %s\n",
				enable ? "Enabled" : "Disabled");

	return rc;
}

static int smblib_update_thermal_readings(struct smb_charger *chg)
{
	union power_supply_propval pval = {0, };
	int rc = 0;

	if (!chg->pl.psy)
		chg->pl.psy = power_supply_get_by_name("parallel");

	rc = smblib_read_iio_channel(chg, chg->iio.die_temp_chan,
				DIV_FACTOR_DECIDEGC, &chg->die_temp);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read DIE TEMP channel, rc=%d\n", rc);
		return rc;
	}

	rc = smblib_read_iio_channel(chg, chg->iio.connector_temp_chan,
				DIV_FACTOR_DECIDEGC, &chg->connector_temp);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read CONN TEMP channel, rc=%d\n", rc);
		return rc;
	}

	rc = smblib_read_iio_channel(chg, chg->iio.skin_temp_chan,
				DIV_FACTOR_DECIDEGC, &chg->skin_temp);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read SKIN TEMP channel, rc=%d\n", rc);
		return rc;
	}

	if (chg->sec_chg_selected == POWER_SUPPLY_CHARGER_SEC_CP) {
		if (is_cp_available(chg)) {
			rc = power_supply_get_property(chg->cp_psy,
				POWER_SUPPLY_PROP_CP_DIE_TEMP, &pval);
			if (rc < 0) {
				smblib_err(chg, "Couldn't get smb1390 charger temp, rc=%d\n",
					rc);
				return rc;
			}
			chg->smb_temp = pval.intval;
		} else {
			smblib_dbg(chg, PR_MISC, "Coudln't find cp_psy\n");
			chg->smb_temp = -ENODATA;
		}
	} else if (chg->pl.psy && chg->sec_chg_selected ==
					POWER_SUPPLY_CHARGER_SEC_PL) {
		rc = power_supply_get_property(chg->pl.psy,
				POWER_SUPPLY_PROP_CHARGER_TEMP, &pval);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get smb1355 charger temp, rc=%d\n",
					rc);
			return rc;
		}
		chg->smb_temp = pval.intval;
	} else {
		chg->smb_temp = -ENODATA;
	}

	return rc;
}

/* SW thermal regulation thresholds in deciDegC */
#define DIE_TEMP_RST_THRESH		1000
#define DIE_TEMP_REG_H_THRESH		800
#define DIE_TEMP_REG_L_THRESH		600

#define CONNECTOR_TEMP_SHDN_THRESH	700
#define CONNECTOR_TEMP_RST_THRESH	600
#define CONNECTOR_TEMP_REG_H_THRESH	550
#define CONNECTOR_TEMP_REG_L_THRESH	500

#define SMB_TEMP_SHDN_THRESH		1400
#define SMB_TEMP_RST_THRESH		900
#define SMB_TEMP_REG_H_THRESH		800
#define SMB_TEMP_REG_L_THRESH		600

#define SKIN_TEMP_SHDN_THRESH		700
#define SKIN_TEMP_RST_THRESH		600
#define SKIN_TEMP_REG_H_THRESH		550
#define SKIN_TEMP_REG_L_THRESH		500

#define THERM_REG_RECHECK_DELAY_1S	1000	/* 1 sec */
#define THERM_REG_RECHECK_DELAY_8S	8000	/* 8 sec */
#define THERM_REG_RECHECK_DELAY_200MS	200	/* 200 msec */
#define THERM_REG_RECHECK_DELAY_5S	5000	/* 5 sec */
#define THERM_REG_RECHECK_DELAY_10S	10000	/* 10 sec */
#define CONNECTOR_THERM_ABOVE		200	/* 20 Dec */
#define CONNECTOR_THERM_HIG			500	/* 50 Dec */
#define CONNECTOR_THERM_TOO_HIG		700	/* 70 Dec */

int smblib_set_vbus_disable(struct smb_charger *chg,
					bool disable)
{
	int ret;

	smblib_err(chg, "set vbus disable:%d\n", disable);
	if (disable) {
		if (chg->vbus_disable_gpio) {
			gpio_set_value(chg->vbus_disable_gpio, 1);
		}
	} else {
		if (chg->vbus_disable_gpio) {
			gpio_set_value(chg->vbus_disable_gpio, 0);
		}
	}
	chg->vbus_disable = disable;

	return ret;
}

static int smblib_set_sw_conn_therm_regulation(struct smb_charger *chg,
						bool enable)
{
	int rc = 0;

	if (!chg->support_conn_therm)
		return rc;

	if (enable) {
		chg->entry_time = ktime_get();
		schedule_delayed_work(&chg->conn_therm_work,
				msecs_to_jiffies(THERM_REG_RECHECK_DELAY_1S));
	} else {
		if (chg->thermal_status != TEMP_ABOVE_RANGE)
			cancel_delayed_work(&chg->conn_therm_work);
	}

	return rc;
}

#define AFTER_RAISE_VBUS_CHECK_DELAY_US		300000 /* 300 msec */
#define AFTER_DETCH_CHECK_DELAY_US		650000 /* 65 msec */
#define FAKE_PLUG_OUT_CHECK_DELAY_MS		900 /* 900 Msec */
static void smblib_plugin_check_time_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
			plugin_check_time_work);
	u64 plugin_time;
	u64 delta_us;

	plugin_time = ktime_get();
	if (!chg->vbus_rising) {
		/*plug out */
		delta_us = ktime_us_delta(plugin_time, chg->after_raise_vbus_time);
		if (delta_us > AFTER_RAISE_VBUS_CHECK_DELAY_US || chg->vbus_disable) {
			chg->fake_plug_out = false;
		} else {
			chg->fake_plug_out = true;
			chg->plugin_detch_check_time = plugin_time;
			schedule_delayed_work(&chg->fake_plug_out_check_work,
				msecs_to_jiffies(FAKE_PLUG_OUT_CHECK_DELAY_MS));
			smblib_err(chg, "%s fake plug out delta_us:%d \n", __func__, delta_us);
		}

		if (chg->fake_plug_out == false)
			smblib_update_usb_type(chg);
	} else {
		/*plug in*/
		delta_us = ktime_us_delta(plugin_time, chg->plugin_detch_check_time);
		if (delta_us > AFTER_DETCH_CHECK_DELAY_US ) {
			chg->no_raise_vbus_status = false;
			return;
		} else {
			chg->plugin_attach_check_time = plugin_time;
			if (chg->fake_plug_out == true)
				chg->no_raise_vbus_status = true;
			smblib_dbg(chg, PR_OEM, "%s in delta_us:%d \n", __func__, delta_us);
		}
	}
}

static void smblib_fake_plug_out_check_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						fake_plug_out_check_work.work);

	chg->fake_plug_out = false;
	power_supply_changed(chg->usb_psy);
	smblib_dbg(chg, PR_OEM, "%s  clean fake_plug_out \n", __func__);
}

static void smblib_clean_cp_to_sw_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						clean_cp_to_sw_work.work);

	chg->cp_to_sw_status = false;
	smblib_dbg(chg, PR_OEM, "%s  clean cp_to_sw_status \n", __func__);
}

#define FFC_DISABLE_CHG_DELAY_US		120000000 /* 120 sec */
#define FFC_DISABLE_CHG_RECHECK_DELAY_1S	1000      /* 1 sec */
#define FFC_DISABLE_CHG_RECHECK_DELAY_10S	10000     /* 10 sec */
#define FFC_DISABLE_CHG_ENABLE_DELAY_120S	120000    /* 120 sec */
#define FFC_DISABLE_CHG_CHECK_TIME		5
#define BATTERY_REPORT_FULL_CURRENT		-400000

static void smblib_after_ffc_chg_dis_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
			after_ffc_chg_dis_work.work);
	union power_supply_propval pval = {0, };
	int rc = 0;
	u64 delta_us;
	static int count;

	if (!chg->last_ffc_remove_time)
		return;

	smblib_dbg(chg, PR_OEM, "delta_us :%d\n", delta_us / 1000000);

	delta_us = ktime_us_delta(ktime_get(), chg->last_ffc_remove_time);
	if (delta_us > FFC_DISABLE_CHG_DELAY_US)
		return;

	rc = smblib_get_prop_usb_present(chg, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get usb present rc = %d\n", rc);
		return;
	}
	if (!pval.intval) {
		vote(chg->chg_disable_votable, AFTER_FFC_VOTER, false, 0);
		count = 0;
		return;
	}

	rc = smblib_get_prop_from_bms(chg,
			POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read SOC value, rc=%d\n", rc);
		return;
	}
	if (pval.intval < 90)
		return;

	rc = smblib_get_prop_from_bms(chg,
		POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read SOC value, rc=%d\n", rc);
		return;
	}
	if (pval.intval < 0 && pval.intval > BATTERY_REPORT_FULL_CURRENT) {
		if (count < FFC_DISABLE_CHG_CHECK_TIME) {
			count++;
			schedule_delayed_work(&chg->after_ffc_chg_dis_work,
					msecs_to_jiffies(FFC_DISABLE_CHG_RECHECK_DELAY_1S));
		} else {
			smblib_dbg(chg, PR_OEM, "disable chg for :%ds when ffc charging\n",
					FFC_DISABLE_CHG_ENABLE_DELAY_120S - (delta_us / 1000));
			vote(chg->chg_disable_votable, AFTER_FFC_VOTER, true, 0);
			schedule_delayed_work(&chg->after_ffc_chg_en_work,
					msecs_to_jiffies(FFC_DISABLE_CHG_ENABLE_DELAY_120S - (delta_us / 1000)));
		}
	} else {
		count = 0;
	}

	return;
}

static void smblib_after_ffc_chg_en_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						after_ffc_chg_en_work.work);

	vote(chg->chg_disable_votable, AFTER_FFC_VOTER, false, 0);

	return;
}

#define TEMP_READ_RETRY	3
int smblib_get_prop_connector_temp(struct smb_charger *chg)
{
	int rc, retry = 0;

	rc = smblib_read_iio_channel(chg, chg->iio.connector_temp_chan,
				DIV_FACTOR_DECIDEGC, &chg->connector_temp);
	if (rc < 0) {
		while (retry < TEMP_READ_RETRY) {
			rc = smblib_read_iio_channel(chg, chg->iio.connector_temp_chan,
					DIV_FACTOR_DECIDEGC, &chg->connector_temp);
			if (rc >= 0) {
				retry = 0;
				break;
			} else {
				retry++;
			}
		}
		if (retry >= TEMP_READ_RETRY) {
			smblib_err(chg, "Couldn't read CONN TEMP channel, rc=%d\n", rc);
			return rc;
		}
	}

	return chg->connector_temp;
}

#define CHG_DETECT_CONN_THERM_US		120000000	/* 120sec */
static void smblib_conn_therm_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						conn_therm_work.work);
	int rc = 0, wdog_timeout = THERM_REG_RECHECK_DELAY_10S;
	static int thermal_status = TEMP_BELOW_RANGE;
	union power_supply_propval val = {0, };
	int usb_present;
	int retry_count = 30;
	u64 elapsed_us;
	bool cp_master_enabled = false;
	bool cp_slave_enabled = false;

	rc = smblib_get_prop_connector_temp(chg);
	if (rc < 0)
		smblib_err(chg, "Couldn't read CONN TEMP channel, rc=%d\n", rc);

	if (chg->fake_conn_temp != 0)
		chg->connector_temp = chg->fake_conn_temp;

	if (chg->connector_temp >=  CONNECTOR_THERM_TOO_HIG) {
		smblib_dbg(chg, PR_OEM, "chg->connector_temp:%d is too hig\n", chg->connector_temp);
		thermal_status = TEMP_ABOVE_RANGE;
		wdog_timeout = THERM_REG_RECHECK_DELAY_1S;
	} else if (chg->connector_temp >=  CONNECTOR_THERM_HIG) {
		if ((thermal_status == TEMP_ABOVE_RANGE)  &&
			(chg->connector_temp > CONNECTOR_THERM_TOO_HIG - 100)) {
			smblib_dbg(chg, PR_OEM, "chg->connector_temp:%d is warm\n", chg->connector_temp);
		} else {
			thermal_status = TEMP_ALERT_LEVEL;
		}
		wdog_timeout = THERM_REG_RECHECK_DELAY_1S;
	} else {
		thermal_status = TEMP_BELOW_RANGE;
		wdog_timeout = THERM_REG_RECHECK_DELAY_10S;
	}

	//smblib_dbg(chg, PR_OEM,"CONN TEMP thermal_status=%d, chip->thermal_status=%d, connect_temp= %d\n",
					//	thermal_status, chg->thermal_status, chg->connector_temp);

	if (thermal_status != chg->thermal_status) {
		chg->thermal_status = thermal_status;
		if (thermal_status == TEMP_ABOVE_RANGE) {
			smblib_err(chg, "connect temp is too hot, disable vbus, sink_src_mode[%d]\n",
						chg->sink_src_mode);
			smblib_err(chg, "vbus_temp[%d-%d-%d-%d]\n", chg->connector_temp, chg->skin_temp, chg->smb_temp, chg->die_temp);

			val.intval = 1;
			/*rc = smblib_set_prop_input_suspend(chg, &val);
			if (rc < 0)
				smblib_err(chg,"Failed to set suspend\n");*/

			power_supply_set_property(chg->batt_psy,POWER_SUPPLY_PROP_INPUT_SUSPEND,&val);

			while (retry_count) {
				if(chg->cp_psy && chg->cp_sec_psy){
					rc = power_supply_get_property(chg->cp_psy,POWER_SUPPLY_PROP_CHARGING_ENABLED,&val);
					if(!rc)
						cp_master_enabled = val.intval;

					rc = power_supply_get_property(chg->cp_sec_psy,POWER_SUPPLY_PROP_CHARGING_ENABLED,&val);
					if(!rc)
						cp_slave_enabled = val.intval;
	}
				smblib_err(chg, "connect temp is too hot, cp_enable:%d, cp_sec_enable：%d，retry_count:%d\n",
						cp_master_enabled,cp_slave_enabled,retry_count);

				if (!cp_master_enabled && !cp_slave_enabled)
					break;
				msleep(80);
				retry_count--;
			}

			msleep(500);

			if (chg->sink_src_mode == SRC_MODE)
				smblib_vbus_regulator_disable(chg->vbus_vreg->rdev);
			else
				smblib_set_vbus_disable(chg, true);
		} else {
			smblib_err(chg, "connect temp normal recovery vbus, sink_src_mode[%d]\n",
						chg->sink_src_mode);
			smblib_err(chg, "vbus_temp[%d-%d-%d-%d]\n", chg->connector_temp, chg->skin_temp, chg->smb_temp, chg->die_temp);

			val.intval = 0;
			power_supply_set_property(chg->batt_psy,POWER_SUPPLY_PROP_INPUT_SUSPEND,&val);
			/*rc = smblib_set_prop_input_suspend(chg, &val);
			if (rc < 0)
				smblib_err(chg,"Failed to set suspend\n");*/

			if (chg->sink_src_mode == SRC_MODE)
				smblib_vbus_regulator_enable(chg->vbus_vreg->rdev);
			else
				smblib_set_vbus_disable(chg, false);
		}
		power_supply_changed(chg->usb_psy);
	}

	elapsed_us = ktime_us_delta(ktime_get(), chg->entry_time);
	if (elapsed_us < CHG_DETECT_CONN_THERM_US)
		wdog_timeout = THERM_REG_RECHECK_DELAY_200MS;

	smblib_get_prop_usb_present(chg, &val);
	usb_present = val.intval;

	if (!usb_present && thermal_status == TEMP_BELOW_RANGE && chg->sink_src_mode != SRC_MODE) {
		smblib_dbg(chg, PR_OEM, "usb is disconnet cancel the connect them work\n");
		if (get_client_vote(chg->usb_icl_votable, USB_PSY_VOTER) == 500000)
			vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
		return;
	} else
		schedule_delayed_work(&chg->conn_therm_work,
				msecs_to_jiffies(wdog_timeout));

	return;
}

static void smblib_report_soc_decimal_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						report_soc_decimal_work.work);
	int quick_charge_type;

	quick_charge_type = smblib_get_quick_charge_type(chg);

	if (QUICK_CHARGE_TURBE == quick_charge_type)
		power_supply_changed(chg->bms_psy);
}

static void smblib_step_charge_notify_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						step_charge_notify_work.work);
	int input_present, capacity, rc;
	union power_supply_propval pval = {0, };

	rc = smblib_is_input_present(chg, &input_present);
	if (rc < 0)
		return;

	if (input_present == INPUT_NOT_PRESENT)
		return;

	rc = smblib_get_prop_batt_capacity(chg, &pval);
	if (rc < 0) {
		pr_err("Couldn't get batt charge type rc=%d\n", rc);
		return;
	}
	capacity = pval.intval;

	/* high capacity no need notify qcom step charge callback */
	if (capacity >= 90)
		return;

	if (input_present & INPUT_PRESENT_DC
			|| input_present & INPUT_PRESENT_USB) {
		/* notify qcom step charge callback to handle step charge every 2s */
		if (chg->batt_psy)
			power_supply_changed(chg->batt_psy);
		schedule_delayed_work(&chg->step_charge_notify_work,
				msecs_to_jiffies(NOTIFY_STEP_CALLBACK_MS));
	}
}

static int smblib_process_thermal_readings(struct smb_charger *chg)
{
	int rc = 0, wdog_timeout = SNARL_WDOG_TMOUT_8S;
	u32 thermal_status = TEMP_BELOW_RANGE;
	bool suspend_input = false, disable_smb = false;

	/*
	 * Following is the SW thermal regulation flow:
	 *
	 * TEMP_SHUT_DOWN_LEVEL: If either connector temp or skin temp
	 * exceeds their respective SHDN threshold. Need to suspend input
	 * and secondary charger.
	 *
	 * TEMP_SHUT_DOWN_SMB_LEVEL: If smb temp exceed its SHDN threshold
	 * but connector and skin temp are below it. Need to suspend SMB.
	 *
	 * TEMP_ALERT_LEVEL: If die, connector, smb or skin temp exceeds it's
	 * respective RST threshold. Stay put and monitor temperature closely.
	 *
	 * TEMP_ABOVE_RANGE or TEMP_WITHIN_RANGE or TEMP_BELOW_RANGE: If die,
	 * connector, smb or skin temp exceeds it's respective REG_H or REG_L
	 * threshold. Unsuspend input and SMB.
	 */
	if (chg->connector_temp > CONNECTOR_TEMP_SHDN_THRESH ||
		chg->skin_temp > SKIN_TEMP_SHDN_THRESH) {
		thermal_status = TEMP_SHUT_DOWN;
		wdog_timeout = SNARL_WDOG_TMOUT_1S;
		suspend_input = true;
		disable_smb = true;
		goto out;
	}

	if (chg->smb_temp > SMB_TEMP_SHDN_THRESH) {
		thermal_status = TEMP_SHUT_DOWN_SMB;
		wdog_timeout = SNARL_WDOG_TMOUT_1S;
		disable_smb = true;
		goto out;
	}

	if (chg->connector_temp > CONNECTOR_TEMP_RST_THRESH ||
			chg->skin_temp > SKIN_TEMP_RST_THRESH ||
			chg->smb_temp > SMB_TEMP_RST_THRESH ||
			chg->die_temp > DIE_TEMP_RST_THRESH) {
		thermal_status = TEMP_ALERT_LEVEL;
		wdog_timeout = SNARL_WDOG_TMOUT_1S;
		goto out;
	}

	if (chg->connector_temp > CONNECTOR_TEMP_REG_H_THRESH ||
			chg->skin_temp > SKIN_TEMP_REG_H_THRESH ||
			chg->smb_temp > SMB_TEMP_REG_H_THRESH ||
			chg->die_temp > DIE_TEMP_REG_H_THRESH) {
		thermal_status = TEMP_ABOVE_RANGE;
		wdog_timeout = SNARL_WDOG_TMOUT_1S;
		goto out;
	}

	if (chg->connector_temp > CONNECTOR_TEMP_REG_L_THRESH ||
			chg->skin_temp > SKIN_TEMP_REG_L_THRESH ||
			chg->smb_temp > SMB_TEMP_REG_L_THRESH ||
			chg->die_temp > DIE_TEMP_REG_L_THRESH) {
		thermal_status = TEMP_WITHIN_RANGE;
		wdog_timeout = SNARL_WDOG_TMOUT_8S;
	}
out:
	smblib_dbg(chg, PR_MISC, "Current temperatures: \tDIE_TEMP: %d,\tCONN_TEMP: %d,\tSMB_TEMP: %d,\tSKIN_TEMP: %d\nTHERMAL_STATUS: %d\n",
			chg->die_temp, chg->connector_temp, chg->smb_temp,
			chg->skin_temp, thermal_status);

	if (thermal_status != chg->thermal_status) {
		chg->thermal_status = thermal_status;
		/*
		 * If thermal level changes to TEMP ALERT LEVEL, don't
		 * enable/disable main/parallel charging.
		 */
		if (chg->thermal_status == TEMP_ALERT_LEVEL)
			goto exit;

		vote(chg->smb_override_votable, SW_THERM_REGULATION_VOTER,
				disable_smb, 0);

		/*
		 * Enable/disable secondary charger through votables to ensure
		 * that if SMB_EN pin get's toggled somehow, secondary charger
		 * remains enabled/disabled according to SW thermal regulation.
		 */
		if (!chg->cp_disable_votable)
			chg->cp_disable_votable = find_votable("CP_DISABLE");
		if (chg->cp_disable_votable)
			vote(chg->cp_disable_votable, SW_THERM_REGULATION_VOTER,
							disable_smb, 0);

		vote(chg->pl_disable_votable, SW_THERM_REGULATION_VOTER,
							disable_smb, 0);
		smblib_dbg(chg, PR_MISC, "Parallel %s as per SW thermal regulation\n",
				disable_smb ? "disabled" : "enabled");

		/*
		 * If thermal level changes to TEMP_SHUT_DOWN_SMB, don't
		 * enable/disable main charger.
		 */
		if (chg->thermal_status == TEMP_SHUT_DOWN_SMB)
			goto exit;

		/* Suspend input if SHDN threshold reached */
		vote(chg->dc_suspend_votable, SW_THERM_REGULATION_VOTER,
							suspend_input, 0);
		vote(chg->usb_icl_votable, SW_THERM_REGULATION_VOTER,
							suspend_input, 0);
		smblib_dbg(chg, PR_MISC, "USB/DC %s as per SW thermal regulation\n",
				suspend_input ? "suspended" : "unsuspended");
	}
exit:
	/*
	 * On USB suspend, WDOG IRQ stops triggering. To continue thermal
	 * monitoring and regulation until USB is plugged out, reschedule
	 * the SW thermal regulation work without releasing the wake lock.
	 */
	if (is_client_vote_enabled(chg->usb_icl_votable,
					SW_THERM_REGULATION_VOTER)) {
		schedule_delayed_work(&chg->thermal_regulation_work,
				msecs_to_jiffies(THERM_REG_RECHECK_DELAY_1S));
		return 0;
	}

	rc = smblib_masked_write(chg, SNARL_BARK_BITE_WD_CFG_REG,
			SNARL_WDOG_TIMEOUT_MASK, wdog_timeout);
	if (rc < 0)
		smblib_err(chg, "Couldn't set WD SNARL timer, rc=%d\n", rc);

	vote(chg->awake_votable, SW_THERM_REGULATION_VOTER, false, 0);
	return rc;
}

/*******************
 * DC PSY GETTERS *
 *******************/

int smblib_get_prop_voltage_wls_output(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	int rc;

	if (chg->wireless_bq)
		return rc;

	if (!chg->wls_psy) {
		chg->wls_psy = power_supply_get_by_name("wireless");
		if (!chg->wls_psy)
			return -ENODEV;
	}

	rc = power_supply_get_property(chg->wls_psy,
				POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
				val);
	if (rc < 0)
		dev_err(chg->dev, "Couldn't get POWER_SUPPLY_PROP_VOLTAGE_REGULATION, rc=%d\n",
				rc);

	return rc;
}

int smblib_get_prop_dc_present(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	if (chg->chg_param.smb_version == PMI632_SUBTYPE) {
		val->intval = 0;
		return 0;
	}

	rc = smblib_read(chg, DCIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read DCIN_RT_STS rc=%d\n", rc);
		return rc;
	}

	if (chg->wireless_bq) {
		if (chg->reverse_gpio_state == REVERSE_GPIO_STATE_START
		||chg->reverse_gpio_state == REVERSE_GPIO_STATE_END){
			val->intval = 0;
		} else {
			val->intval = (bool)(stat & DCIN_PON_RT_STS_BIT);
		}
	} else {
		val->intval = (bool)(stat & DCIN_PLUGIN_RT_STS_BIT);
	}
	return 0;
}

int smblib_get_prop_dc_online(struct smb_charger *chg,
			       union power_supply_propval *val)
{
	int rc = 0;
	u8 stat;

	if (chg->chg_param.smb_version == PMI632_SUBTYPE) {
		val->intval = 0;
		return 0;
	}

	if (get_client_vote(chg->dc_suspend_votable, USER_VOTER)) {
		val->intval = false;
		return rc;
	}

	if (chg->report_input_absent) {
		val->intval = 0;
		return rc;
	}

	if (chg->wireless_bq) {
		if (chg->fake_dc_on){
			val->intval = true;
			return rc;
		}
		rc = smblib_get_prop_dc_present(chg, val);
		return rc;
	}

	if (is_client_vote_enabled(chg->dc_suspend_votable,
						CHG_TERMINATION_VOTER)) {
		rc = smblib_get_prop_dc_present(chg, val);
		return rc;
	}

	rc = smblib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n",
			rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "POWER_PATH_STATUS = 0x%02x\n",
		   stat);

	val->intval = (stat & USE_DCIN_BIT) &&
		      (stat & VALID_INPUT_POWER_SOURCE_STS_BIT);

	return rc;
}

int smblib_get_prop_dc_current_max(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	return smblib_get_charge_param(chg, &chg->param.dc_icl, &val->intval);
}

int smblib_get_prop_dc_voltage_max(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	int rc;
	val->intval = MICRO_12V;

	if (!chg->wls_psy)
		chg->wls_psy = power_supply_get_by_name("wireless");

	if (chg->wls_psy) {
		rc = power_supply_get_property(chg->wls_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MAX,
				val);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't get VOLTAGE_MAX, rc=%d\n",
					rc);
			return rc;
		}
	}

	return 0;
}

int smblib_get_prop_dc_voltage_now(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	int rc;

	if (chg->wireless_bq)
		return rc;

	if (!chg->wls_psy) {
		chg->wls_psy = power_supply_get_by_name("wireless");
		if (!chg->wls_psy)
			return -ENODEV;
	}

	rc = power_supply_get_property(chg->wls_psy,
				POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
				val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't get POWER_SUPPLY_PROP_VOLTAGE_REGULATION, rc=%d\n",
				rc);
		return rc;
	}

	return rc;
}

/*******************
 * DC PSY SETTERS *
 *******************/

int smblib_set_prop_dc_current_max(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	chg->dcin_icl_user_set = true;

	if (chg->wireless_bq)
		return 0;
	return smblib_set_charge_param(chg, &chg->param.dc_icl, val->intval);
}

#define DCIN_AICL_RERUN_DELAY_MS	5000
int smblib_set_prop_voltage_wls_output(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc;

	if (chg->wireless_bq)
		return rc;

	if (!chg->wls_psy) {
		chg->wls_psy = power_supply_get_by_name("wireless");
		if (!chg->wls_psy)
			return -ENODEV;
	}

	rc = power_supply_set_property(chg->wls_psy,
				POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
				val);
	if (rc < 0)
		dev_err(chg->dev, "Couldn't set POWER_SUPPLY_PROP_VOLTAGE_REGULATION, rc=%d\n",
				rc);

	smblib_dbg(chg, PR_WLS, "%d\n", val->intval);

	/*
	 * When WLS VOUT goes down, the power-constrained adaptor may be able
	 * to supply more current, so allow it to do so - unless userspace has
	 * changed DCIN ICL value already due to thermal considerations.
	 */
	if (!chg->dcin_icl_user_set && (val->intval > 0) &&
			(val->intval < chg->last_wls_vout)) {
		alarm_start_relative(&chg->dcin_aicl_alarm,
				ms_to_ktime(DCIN_AICL_RERUN_DELAY_MS));
	}

	chg->last_wls_vout = val->intval;

	return rc;
}

int smblib_set_prop_dc_reset(struct smb_charger *chg)
{
	int rc;

	if (chg->wireless_bq)
		return rc;

	rc = vote(chg->dc_suspend_votable, VOUT_VOTER, true, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't suspend DC rc=%d\n", rc);
		return rc;
	}

	rc = smblib_masked_write(chg, DCIN_CMD_IL_REG, DCIN_EN_MASK,
				DCIN_EN_OVERRIDE_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set DCIN_EN_OVERRIDE_BIT rc=%d\n",
			rc);
		return rc;
	}

	rc = smblib_write(chg, DCIN_CMD_PON_REG, DCIN_PON_BIT | MID_CHG_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't write %d to DCIN_CMD_PON_REG rc=%d\n",
			DCIN_PON_BIT | MID_CHG_BIT, rc);
		return rc;
	}

	/* Wait for 10ms to allow the charge to get drained */
	usleep_range(10000, 10010);

	rc = smblib_write(chg, DCIN_CMD_PON_REG, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't clear DCIN_CMD_PON_REG rc=%d\n", rc);
		return rc;
	}

	rc = smblib_masked_write(chg, DCIN_CMD_IL_REG, DCIN_EN_MASK, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't clear DCIN_EN_OVERRIDE_BIT rc=%d\n",
			rc);
		return rc;
	}

	rc = vote(chg->dc_suspend_votable, VOUT_VOTER, false, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't unsuspend  DC rc=%d\n", rc);
		return rc;
	}

	smblib_dbg(chg, PR_MISC, "Wireless charger removal detection successful\n");
	return rc;
}

int smblib_set_prop_tx_mac(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc = 0;
	smblib_dbg(chg, PR_WLS, "mac raw %llx\n", val->int64val);
	chg->tx_bt_mac = val->int64val;
	if (!chg->wls_psy) {
		chg->wls_psy = power_supply_get_by_name("wireless");
		if (!chg->wls_psy)
			return -ENODEV;
	}
	power_supply_changed(chg->wls_psy);
	return rc;
}

void smblib_set_prop_pen_mac(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	smblib_dbg(chg, PR_WLS, "pen mac raw %llx\n", val->int64val);
	chg->pen_bt_mac = val->int64val;
	if (!chg->wls_psy) {
		chg->wls_psy = power_supply_get_by_name("wireless");
		if (!chg->wls_psy) {
			smblib_err(chg, "no wireless power supply, return\n");
			return;
		}
	}
	power_supply_changed(chg->wls_psy);
}

int smblib_set_prop_rx_cr(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc = 0;
	smblib_dbg(chg, PR_WLS, "rx_cr raw 0x%llx\n", val->int64val);
	if (!chg->wls_psy) {
		chg->wls_psy = power_supply_get_by_name("wireless");
		if (!chg->wls_psy) {
			return -ENODEV;
		}
	}
	if (!val->int64val)
		return rc;
	chg->oob_rpp_msg_cnt %= 9;
	chg->oob_rpp_msg_cnt++;
	chg->rpp = (val->int64val | chg->oob_rpp_msg_cnt << 48);
	power_supply_changed(chg->wls_psy);
	return rc;
}

int smblib_set_prop_rx_cep(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc = 0;
	smblib_dbg(chg, PR_WLS, "rx_cep raw 0x%llx\n", val->int64val);
	if (!chg->wls_psy) {
		chg->wls_psy = power_supply_get_by_name("wireless");
		if (!chg->wls_psy) {
			return -ENODEV;
		}
	}
	if (!val->int64val)
		return rc;
	chg->oob_cep_msg_cnt %= 9;
	chg->oob_cep_msg_cnt++;
	chg->cep = (val->int64val | chg->oob_cep_msg_cnt << 48);
	power_supply_changed(chg->wls_psy);
	return rc;
}
#define BLE_CONNECT	1
#define BLE_DISCONNECT	2
#define BLE_CONNECTING	3

extern int idtp_op_ble_flag(int en);
extern int rx_op_ble_flag(int en);
int smblib_set_prop_bt_state(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int bt_state = -2;

	smblib_dbg(chg, PR_WLS, "bt_state raw is 0x%x\n", val->intval);
	bt_state = val->intval;

#if defined(CONFIG_IDT_P9415) || defined(CONFIG_RX1619)
	switch (bt_state) {
	case BLE_CONNECT:
		if (chg->idtp_psy)
			idtp_op_ble_flag(1);
		else
			rx_op_ble_flag(1);
		chg->tx_bt_mac = 0;
		break;
	case BLE_DISCONNECT:
		if (chg->idtp_psy)
			idtp_op_ble_flag(0);
		else
			rx_op_ble_flag(0);
		chg->tx_bt_mac = 0;
		break;
	case BLE_CONNECTING:
		chg->tx_bt_mac = 0;
		break;
	default:
		break;
	}
#endif
	return 0;
}

/*******************
 * USB PSY GETTERS *
 *******************/

int smblib_get_prop_usb_present(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USBIN_RT_STS rc=%d\n", rc);
		return rc;
	}

	val->intval = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);
	return 0;
}

int smblib_get_prop_usb_online(struct smb_charger *chg,
			       union power_supply_propval *val)
{
	int usb_present, rc = 0;
	u8 stat;
	union power_supply_propval pval = {0, };
	bool dc_power_on;
	int icl = 0;
	if (chg->is_qc_class_a && !chg->qc3_raise_done)
		icl = MAIN_ICL_MIN;

	if (chg->ext_bbc) {
		rc = smblib_get_prop_from_bbc(chg,
				POWER_SUPPLY_PROP_PRESENT, &pval);
		if (rc < 0)
			val->intval = false;
		val->intval = pval.intval;
		return 0;
	}

	if (get_client_vote_locked(chg->usb_icl_votable, USER_VOTER) == 0) {
		val->intval = false;
		return rc;
	}

	if (chg->wireless_bq) {
		rc = smblib_get_prop_dc_present(chg, &pval);
		dc_power_on = pval.intval;
		if (dc_power_on == 1) {
			val->intval = false;
			return rc;
		}
	}

	rc = smblib_get_prop_usb_present(chg, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get usb present rc = %d\n", rc);
		return rc;
	}

	usb_present = pval.intval;
	if (usb_present &&
			get_client_vote_locked(chg->usb_icl_votable, JEITA_VOTER) == 0) {
		/* show online when JEITA_VOTER 0mA is vote to improve user experience */
		val->intval = true;
		return rc;
	}

	if (chg->fake_plug_out == true || chg->cp_to_sw_status == true) {
		val->intval = true;
		return rc;
	}

	if (chg->use_bq_pump && !chg->six_pin_step_charge_enable
			&& (get_client_vote_locked(chg->usb_icl_votable,
				MAIN_CHG_SUSPEND_VOTER) == icl)) {
		val->intval = true;
		return rc;
	}

	if (chg->use_bq_pump && chg->six_pin_step_charge_enable
			&& (get_client_vote_locked(chg->usb_icl_votable,
				MAIN_ICL_MIN_VOTER) == MAIN_ICL_MIN)) {
		val->intval = true;
		return rc;
	}

	if (chg->use_bq_pump && !chg->six_pin_step_charge_enable
		&& is_client_vote_enabled_locked(chg->chg_disable_votable,
			AFTER_FFC_VOTER)) {
		val->intval = true;
		return rc;
	}

	if (is_client_vote_enabled_locked(chg->usb_icl_votable,
					CHG_TERMINATION_VOTER)) {
		rc = smblib_get_prop_usb_present(chg, val);
		return rc;
	}

	rc = smblib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n",
			rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "USB ONLINE POWER_PATH_STATUS = 0x%02x\n",
		   stat);

	val->intval = (stat & USE_USBIN_BIT) &&
		      (stat & VALID_INPUT_POWER_SOURCE_STS_BIT);

	/* power good is on and sink pluged-in, do not report online */
	if (val->intval
		&& chg->power_good_en
		&& chg->typec_mode >= POWER_SUPPLY_TYPEC_SINK
		&& chg->typec_mode <= POWER_SUPPLY_TYPEC_POWERED_CABLE_ONLY) {
		val->intval = 0;
		return rc;
	}
	return rc;
}

int smblib_get_usb_online(struct smb_charger *chg,
			union power_supply_propval *val)
{
	int rc;

	if (chg->report_input_absent) {
		val->intval = 0;
		return rc;
	}

	rc = smblib_get_prop_usb_online(chg, val);
	if (!val->intval)
		goto exit;

	if (((chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT) ||
		(chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB))
		&& (chg->real_charger_type == POWER_SUPPLY_TYPE_USB))
		val->intval = 0;
	else
		val->intval = 1;

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_UNKNOWN)
		val->intval = 0;

exit:
	return rc;
}

int smblib_get_prop_usb_voltage_max_design(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	switch (chg->real_charger_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP:
		if (chg->qc2_unsupported_voltage == QC2_NON_COMPLIANT_9V) {
			val->intval = MICRO_5V;
			break;
		} else if (chg->qc2_unsupported_voltage ==
				QC2_NON_COMPLIANT_12V) {
			val->intval = MICRO_9V;
			break;
		}
		/* else, fallthrough */
	case POWER_SUPPLY_TYPE_USB_HVDCP_3P5:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
	case POWER_SUPPLY_TYPE_USB_PD:
		if (chg->chg_param.smb_version == PMI632_SUBTYPE)
			val->intval = MICRO_9V;
		else
			val->intval = MICRO_12V;
		break;
	default:
		val->intval = MICRO_5V;
		break;
	}

	return 0;
}

int smblib_get_prop_usb_voltage_max(struct smb_charger *chg,
					union power_supply_propval *val)
{
	switch (chg->real_charger_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP:
		if (chg->qc2_unsupported_voltage == QC2_NON_COMPLIANT_9V) {
			val->intval = MICRO_5V;
			break;
		} else if (chg->qc2_unsupported_voltage ==
				QC2_NON_COMPLIANT_12V) {
			val->intval = MICRO_9V;
			break;
		}
		/* else, fallthrough */
	case POWER_SUPPLY_TYPE_USB_HVDCP_3P5:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		if (chg->chg_param.smb_version == PMI632_SUBTYPE)
			val->intval = MICRO_9V;
		else
			val->intval = MICRO_12V;
		break;
	case POWER_SUPPLY_TYPE_USB_PD:
		val->intval = chg->voltage_max_uv;
		break;
	default:
		val->intval = MICRO_5V;
		break;
	}

	return 0;
}

#define HVDCP3_STEP_UV	200000
#define HVDCP3P5_STEP_UV	20000
static int smblib_estimate_adaptor_voltage(struct smb_charger *chg,
					  union power_supply_propval *val)
{
	int step_uv = HVDCP3_STEP_UV;

	switch (chg->real_charger_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP:
		val->intval = MICRO_12V;
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3P5:
		step_uv = HVDCP3P5_STEP_UV;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		val->intval = MICRO_5V + (step_uv * chg->pulse_cnt);
		break;
	case POWER_SUPPLY_TYPE_USB_PD:
		/* Take the average of min and max values */
		val->intval = chg->voltage_min_uv +
			((chg->voltage_max_uv - chg->voltage_min_uv) / 2);
		break;
	default:
		val->intval = MICRO_5V;
		break;
	}

	return 0;
}

static int smblib_read_mid_voltage_chan(struct smb_charger *chg,
					union power_supply_propval *val)
{
	int rc;

	if (!chg->iio.mid_chan)
		return -ENODATA;

	rc = iio_read_channel_processed(chg->iio.mid_chan, &val->intval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read MID channel rc=%d\n", rc);
		return rc;
	}

	/*
	 * If MID voltage < 1V, it is unreliable.
	 * Figure out voltage from registers and calculations.
	 */
	if (val->intval < 1000000)
		return smblib_estimate_adaptor_voltage(chg, val);

	return 0;
}

static int smblib_read_usbin_voltage_chan(struct smb_charger *chg,
				     union power_supply_propval *val)
{
	int rc;

	if (!chg->iio.usbin_v_chan)
		return -ENODATA;

	rc = iio_read_channel_processed(chg->iio.usbin_v_chan, &val->intval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USBIN channel rc=%d\n", rc);
		return rc;
	}

	return 0;
}

int smblib_get_prop_usb_voltage_now(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	union power_supply_propval pval = {0, };
	int rc, ret = 0;
	u8 reg, adc_ch_reg;

	mutex_lock(&chg->adc_lock);

	if (chg->wa_flags & USBIN_ADC_WA) {
		/* Store ADC channel config in order to restore later */
		rc = smblib_read(chg, BATIF_ADC_CHANNEL_EN_REG, &adc_ch_reg);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read ADC config rc=%d\n", rc);
			ret = rc;
			goto unlock;
		}

		/* Disable all ADC channels except IBAT channel */
		rc = smblib_write(chg, BATIF_ADC_CHANNEL_EN_REG,
						IBATT_CHANNEL_EN_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't disable ADC channels rc=%d\n",
						rc);
			ret = rc;
			goto unlock;
		}
	}

	rc = smblib_get_prop_usb_present(chg, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get usb presence status rc=%d\n", rc);
		ret = -ENODATA;
		goto restore_adc_config;
	}

	/*
	 * Skip reading voltage only if USB is not present and we are not in
	 * OTG mode.
	 */
	if (!pval.intval) {
		rc = smblib_read(chg, DCDC_CMD_OTG_REG, &reg);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read CMD_OTG rc=%d", rc);
			goto restore_adc_config;
		}

		if (!(reg & OTG_EN_BIT))
			goto restore_adc_config;
	}

	/*
	 * For PM8150B, use MID_CHG ADC channel because overvoltage is observed
	 * to occur randomly in the USBIN channel, particularly at high
	 * voltages.
	 */
	if (chg->chg_param.smb_version == PM8150B_SUBTYPE)
		rc = smblib_read_mid_voltage_chan(chg, val);
	else
		rc = smblib_read_usbin_voltage_chan(chg, val);
	if (rc < 0) {
		smblib_err(chg, "Failed to read USBIN over vadc, rc=%d\n", rc);
		ret = rc;
	}

restore_adc_config:
	 /* Restore ADC channel config */
	if (chg->wa_flags & USBIN_ADC_WA) {
		rc = smblib_write(chg, BATIF_ADC_CHANNEL_EN_REG, adc_ch_reg);
		if (rc < 0)
			smblib_err(chg, "Couldn't write ADC config rc=%d\n",
						rc);
	}

unlock:
	mutex_unlock(&chg->adc_lock);

	return ret;
}

static int smblib_get_usb_in_voltage_now(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	int rc, ret = 0;

	if (chg->iio.usbin_v_chan) {
		rc = iio_read_channel_processed(chg->iio.usbin_v_chan,
				&val->intval);
		if (rc < 0)
			ret = -ENODATA;
	} else {
		ret = -ENODATA;
	}

	return ret;
}

int smblib_get_prop_vph_voltage_now(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	int rc;

	if (!chg->iio.vph_v_chan)
		return -ENODATA;

	rc = iio_read_channel_processed(chg->iio.vph_v_chan, &val->intval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read vph channel rc=%d\n", rc);
		return rc;
	}

	return 0;
}

bool smblib_rsbux_low(struct smb_charger *chg, int r_thr)
{
	int r_sbu1, r_sbu2;
	bool ret = false;
	int rc;

	if (!chg->iio.sbux_chan)
		return false;

	/* disable crude sensors */
	rc = smblib_masked_write(chg, TYPE_C_CRUDE_SENSOR_CFG_REG,
			EN_SRC_CRUDE_SENSOR_BIT | EN_SNK_CRUDE_SENSOR_BIT,
			0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't disable crude sensor rc=%d\n", rc);
		return false;
	}

	/* select SBU1 as current source */
	rc = smblib_write(chg, TYPE_C_SBU_CFG_REG, SEL_SBU1_ISRC_VAL);
	if (rc < 0) {
		smblib_err(chg, "Couldn't select SBU1 rc=%d\n", rc);
		goto cleanup;
	}

	rc = iio_read_channel_processed(chg->iio.sbux_chan, &r_sbu1);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read SBU1 rc=%d\n", rc);
		goto cleanup;
	}

	if (r_sbu1 < r_thr) {
		ret = true;
		goto cleanup;
	}

	/* select SBU2 as current source */
	rc = smblib_write(chg, TYPE_C_SBU_CFG_REG, SEL_SBU2_ISRC_VAL);
	if (rc < 0) {
		smblib_err(chg, "Couldn't select SBU1 rc=%d\n", rc);
		goto cleanup;
	}

	rc = iio_read_channel_processed(chg->iio.sbux_chan, &r_sbu2);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read SBU1 rc=%d\n", rc);
		goto cleanup;
	}

	if (r_sbu2 < r_thr)
		ret = true;
cleanup:
	/* enable crude sensors */
	rc = smblib_masked_write(chg, TYPE_C_CRUDE_SENSOR_CFG_REG,
			EN_SRC_CRUDE_SENSOR_BIT | EN_SNK_CRUDE_SENSOR_BIT,
			EN_SRC_CRUDE_SENSOR_BIT | EN_SNK_CRUDE_SENSOR_BIT);
	if (rc < 0)
		smblib_err(chg, "Couldn't enable crude sensor rc=%d\n", rc);

	/* disable current source */
	rc = smblib_write(chg, TYPE_C_SBU_CFG_REG, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't select SBU1 rc=%d\n", rc);

	return ret;
}

int smblib_get_prop_charger_temp(struct smb_charger *chg,
				 union power_supply_propval *val)
{
	int temp, rc;
	int input_present;

	rc = smblib_is_input_present(chg, &input_present);
	if (rc < 0)
		return rc;

	if (input_present == INPUT_NOT_PRESENT)
		return -ENODATA;

	if (chg->iio.temp_chan) {
		rc = iio_read_channel_processed(chg->iio.temp_chan,
				&temp);
		if (rc < 0) {
			pr_err("Error in reading temp channel, rc=%d\n", rc);
			return rc;
		}
		val->intval = temp / 100;
	} else {
		return -ENODATA;
	}

	return rc;
}

int smblib_get_prop_typec_cc_orientation(struct smb_charger *chg,
					 union power_supply_propval *val)
{
	int rc = 0;
	u8 stat;

	val->intval = 0;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return 0;

	rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n", rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_STATUS_4 = 0x%02x\n", stat);

	if (stat & CC_ATTACHED_BIT)
		val->intval = (bool)(stat & CC_ORIENTATION_BIT) + 1;

	return rc;
}

static const char * const smblib_typec_mode_name[] = {
	[POWER_SUPPLY_TYPEC_NONE]		  = "NONE",
	[POWER_SUPPLY_TYPEC_SOURCE_DEFAULT]	  = "SOURCE_DEFAULT",
	[POWER_SUPPLY_TYPEC_SOURCE_MEDIUM]	  = "SOURCE_MEDIUM",
	[POWER_SUPPLY_TYPEC_SOURCE_HIGH]	  = "SOURCE_HIGH",
	[POWER_SUPPLY_TYPEC_NON_COMPLIANT]	  = "NON_COMPLIANT",
	[POWER_SUPPLY_TYPEC_SINK]		  = "SINK",
	[POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE]   = "SINK_POWERED_CABLE",
	[POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY] = "SINK_DEBUG_ACCESSORY",
	[POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER]   = "SINK_AUDIO_ADAPTER",
	[POWER_SUPPLY_TYPEC_POWERED_CABLE_ONLY]   = "POWERED_CABLE_ONLY",
};

static int smblib_get_prop_ufp_mode(struct smb_charger *chg)
{
	int rc;
	u8 stat;
	union power_supply_propval val = {0, };
	int usb_present = 0;

	rc = smblib_read(chg, TYPE_C_SNK_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_1 rc=%d\n", rc);
		return POWER_SUPPLY_TYPEC_NONE;
	}
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_STATUS_1 = 0x%02x\n", stat);

	switch (stat & DETECTED_SRC_TYPE_MASK) {
	case SNK_RP_STD_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	case SNK_RP_1P5_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
	case SNK_RP_3P0_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_HIGH;
	case SNK_RP_SHORT_BIT:
		return POWER_SUPPLY_TYPEC_NON_COMPLIANT;
	case SNK_DAM_500MA_BIT:
	case SNK_DAM_1500MA_BIT:
	case SNK_DAM_3000MA_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	default:
		break;
	}

	/* workaround for scp cable or similar A TO C cables */
	rc = smblib_read(chg, TYPE_C_SNK_DEBUG_ACC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_1 rc=%d\n", rc);
		return POWER_SUPPLY_TYPEC_NONE;
	}

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0)
		smblib_err(chg, "Couldn't get usb present rc = %d\n", rc);
	else
		usb_present = val.intval;

	if (chg->snk_debug_acc_detected && usb_present) {
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	}
	if (stat & SNK_DEBUG_ACC_RPSTD_PRSTD_BIT && usb_present) {
		chg->snk_debug_acc_detected = true;
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	}

	return POWER_SUPPLY_TYPEC_NONE;
}

static int smblib_get_prop_dfp_mode(struct smb_charger *chg)
{
	int rc;
	u8 stat;

	if (chg->lpd_stage == LPD_STAGE_COMMIT)
		return POWER_SUPPLY_TYPEC_NONE;

	rc = smblib_read(chg, TYPE_C_SRC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_SRC_STATUS_REG rc=%d\n",
				rc);
		return POWER_SUPPLY_TYPEC_NONE;
	}
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_SRC_STATUS_REG = 0x%02x\n", stat);

	switch (stat & DETECTED_SNK_TYPE_MASK) {
	case AUDIO_ACCESS_RA_RA_BIT:
		return POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER;
	case SRC_DEBUG_ACCESS_BIT:
		return POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY;
	case SRC_RD_RA_VCONN_BIT:
		return POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE;
	case SRC_RD_OPEN_BIT:
		return POWER_SUPPLY_TYPEC_SINK;
	default:
		break;
	}

	return POWER_SUPPLY_TYPEC_NONE;
}

static int smblib_get_prop_typec_mode(struct smb_charger *chg)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_MISC_STATUS_REG rc=%d\n",
				rc);
		return 0;
	}
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_MISC_STATUS_REG = 0x%02x\n", stat);

	if (stat & SNK_SRC_MODE_BIT)
		return smblib_get_prop_dfp_mode(chg);
	else
		return smblib_get_prop_ufp_mode(chg);
}

inline int smblib_get_usb_prop_typec_mode(struct smb_charger *chg,
				union power_supply_propval *val)
{
	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		val->intval = POWER_SUPPLY_TYPEC_NONE;
	else
		val->intval = chg->typec_mode;

	return 0;
}

int smblib_get_prop_typec_power_role(struct smb_charger *chg,
				     union power_supply_propval *val)
{
	int rc = 0;
	u8 ctrl;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		return 0;
	}

	rc = smblib_read(chg, TYPE_C_MODE_CFG_REG, &ctrl);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_MODE_CFG_REG rc=%d\n",
			rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_MODE_CFG_REG = 0x%02x\n",
		   ctrl);

	if (ctrl & TYPEC_DISABLE_CMD_BIT) {
		val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		return rc;
	}

	switch (ctrl & (EN_SRC_ONLY_BIT | EN_SNK_ONLY_BIT)) {
	case 0:
		val->intval = POWER_SUPPLY_TYPEC_PR_DUAL;
		break;
	case EN_SRC_ONLY_BIT:
		val->intval = POWER_SUPPLY_TYPEC_PR_SOURCE;
		break;
	case EN_SNK_ONLY_BIT:
		val->intval = POWER_SUPPLY_TYPEC_PR_SINK;
		break;
	default:
		val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		smblib_err(chg, "unsupported power role 0x%02lx\n",
			ctrl & (EN_SRC_ONLY_BIT | EN_SNK_ONLY_BIT));
		return -EINVAL;
	}

	chg->power_role = val->intval;
	return rc;
}

static inline bool typec_in_src_mode(struct smb_charger *chg)
{
	return (chg->typec_mode > POWER_SUPPLY_TYPEC_NONE &&
		chg->typec_mode < POWER_SUPPLY_TYPEC_SOURCE_DEFAULT);
}

int smblib_get_prop_typec_select_rp(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	int rc, rp;
	u8 stat;

	if (!typec_in_src_mode(chg))
		return -ENODATA;

	rc = smblib_read(chg, TYPE_C_CURRSRC_CFG_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_CURRSRC_CFG_REG rc=%d\n",
				rc);
		return rc;
	}

	switch (stat & TYPEC_SRC_RP_SEL_MASK) {
	case TYPEC_SRC_RP_STD:
		rp = POWER_SUPPLY_TYPEC_SRC_RP_STD;
		break;
	case TYPEC_SRC_RP_1P5A:
		rp = POWER_SUPPLY_TYPEC_SRC_RP_1P5A;
		break;
	case TYPEC_SRC_RP_3A:
	case TYPEC_SRC_RP_3A_DUPLICATE:
		rp = POWER_SUPPLY_TYPEC_SRC_RP_3A;
		break;
	default:
		return -EINVAL;
	}

	val->intval = rp;

	return 0;
}

int smblib_get_prop_usb_current_now(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	union power_supply_propval pval = {0, };
	int rc = 0, buck_scale = 1, boost_scale = 1;

	if (chg->iio.usbin_i_chan) {
		rc = iio_read_channel_processed(chg->iio.usbin_i_chan,
				&val->intval);
		if (rc < 0) {
			pr_err("Error in reading USBIN_I channel, rc=%d\n", rc);
			return rc;
		}

		/*
		 * For PM8150B, scaling factor = reciprocal of
		 * 0.2V/A in Buck mode, 0.4V/A in Boost mode.
		 * For PMI632, scaling factor = reciprocal of
		 * 0.4V/A in Buck mode, 0.8V/A in Boost mode.
		 */
		switch (chg->chg_param.smb_version) {
		case PMI632_SUBTYPE:
			buck_scale = 40;
			boost_scale = 80;
			break;
		default:
			buck_scale = 20;
			boost_scale = 40;
			break;
		}

		if (chg->otg_present || smblib_get_prop_dfp_mode(chg) !=
				POWER_SUPPLY_TYPEC_NONE) {
			val->intval = DIV_ROUND_CLOSEST(val->intval * 100,
								boost_scale);
			return rc;
		}

		rc = smblib_get_prop_usb_present(chg, &pval);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get usb present status,rc=%d\n",
				rc);
			return -ENODATA;
		}

		/* If USB is not present, return 0 */
		if (!pval.intval)
			val->intval = 0;
		else
			val->intval = DIV_ROUND_CLOSEST(val->intval * 100,
								buck_scale);
	} else {
		val->intval = 0;
		rc = -ENODATA;
	}

	return rc;
}

int smblib_get_prop_low_power(struct smb_charger *chg,
					  union power_supply_propval *val)
{
	int rc;
	u8 stat;

	if (chg->sink_src_mode != SRC_MODE)
		return -ENODATA;

	rc = smblib_read(chg, TYPE_C_SRC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_SRC_STATUS_REG rc=%d\n",
				rc);
		return rc;
	}

	val->intval = !(stat & SRC_HIGH_BATT_BIT);

	return 0;
}

int smblib_get_prop_input_current_settled(struct smb_charger *chg,
					  union power_supply_propval *val)
{
	return smblib_get_charge_param(chg, &chg->param.icl_stat, &val->intval);
}

int smblib_get_prop_input_current_max(struct smb_charger *chg,
					  union power_supply_propval *val)
{
	int icl_ua = 0, rc;

	rc = smblib_get_charge_param(chg, &chg->param.usb_icl, &icl_ua);
	if (rc < 0)
		return rc;

	if (is_override_vote_enabled_locked(chg->usb_icl_votable) &&
					icl_ua < USBIN_1000MA) {
		val->intval = USBIN_1000MA;
		return 0;
	}

	return smblib_get_charge_param(chg, &chg->param.icl_stat, &val->intval);
}

int smblib_get_prop_input_voltage_settled(struct smb_charger *chg,
						union power_supply_propval *val)
{
	int rc, pulses;
	int step_uv = HVDCP3_STEP_UV;

	switch (chg->real_charger_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP_3P5:
		step_uv = HVDCP3P5_STEP_UV;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		rc = smblib_get_pulse_cnt(chg, &pulses);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't read QC_PULSE_COUNT rc=%d\n", rc);
			return 0;
		}
		val->intval = MICRO_5V + step_uv * pulses;
		break;
	case POWER_SUPPLY_TYPE_USB_PD:
		val->intval = chg->voltage_min_uv;
		break;
	default:
		val->intval = MICRO_5V;
		break;
	}

	return 0;
}

int smblib_get_prop_pd_in_hard_reset(struct smb_charger *chg,
			       union power_supply_propval *val)
{
	val->intval = chg->pd_hard_reset;
	return 0;
}

int smblib_get_pe_start(struct smb_charger *chg,
			       union power_supply_propval *val)
{
	val->intval = chg->ok_to_pd;
	return 0;
}

int smblib_get_prop_smb_health(struct smb_charger *chg)
{
	int rc;
	int input_present;
	union power_supply_propval prop = {0, };

	rc = smblib_is_input_present(chg, &input_present);
	if (rc < 0)
		return rc;

	if ((input_present == INPUT_NOT_PRESENT) || (!is_cp_available(chg)))
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	rc = power_supply_get_property(chg->cp_psy,
				POWER_SUPPLY_PROP_CP_DIE_TEMP, &prop);
	if (rc < 0)
		return rc;

	if (prop.intval > SMB_TEMP_RST_THRESH)
		return POWER_SUPPLY_HEALTH_OVERHEAT;

	if (prop.intval > SMB_TEMP_REG_H_THRESH)
		return POWER_SUPPLY_HEALTH_HOT;

	if (prop.intval > SMB_TEMP_REG_L_THRESH)
		return POWER_SUPPLY_HEALTH_WARM;

	return POWER_SUPPLY_HEALTH_COOL;
}

int smblib_get_prop_die_health(struct smb_charger *chg)
{
	int rc;
	u8 stat;
	int input_present;

	rc = smblib_is_input_present(chg, &input_present);
	if (rc < 0)
		return rc;

	if (input_present == INPUT_NOT_PRESENT)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (chg->wa_flags & SW_THERM_REGULATION_WA) {
		if (chg->die_temp == -ENODATA)
			return POWER_SUPPLY_HEALTH_UNKNOWN;

		if (chg->die_temp > DIE_TEMP_RST_THRESH)
			return POWER_SUPPLY_HEALTH_OVERHEAT;

		if (chg->die_temp > DIE_TEMP_REG_H_THRESH)
			return POWER_SUPPLY_HEALTH_HOT;

		if (chg->die_temp > DIE_TEMP_REG_L_THRESH)
			return POWER_SUPPLY_HEALTH_WARM;

		return POWER_SUPPLY_HEALTH_COOL;
	}

	rc = smblib_read(chg, DIE_TEMP_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read DIE_TEMP_STATUS_REG, rc=%d\n",
				rc);
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	if (stat & DIE_TEMP_RST_BIT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;

	if (stat & DIE_TEMP_UB_BIT)
		return POWER_SUPPLY_HEALTH_HOT;

	if (stat & DIE_TEMP_LB_BIT)
		return POWER_SUPPLY_HEALTH_WARM;

	return POWER_SUPPLY_HEALTH_COOL;
}

int smblib_get_die_health(struct smb_charger *chg,
			union power_supply_propval *val)
{
	if (chg->die_health == -EINVAL)
		val->intval = smblib_get_prop_die_health(chg);
	else
		val->intval = chg->die_health;

	return 0;
}

int smblib_get_prop_scope(struct smb_charger *chg,
			union power_supply_propval *val)
{
	int rc;
	union power_supply_propval pval;

	val->intval = POWER_SUPPLY_SCOPE_UNKNOWN;
	rc = smblib_get_prop_usb_present(chg, &pval);
	if (rc < 0)
		return rc;

	val->intval = pval.intval ? POWER_SUPPLY_SCOPE_DEVICE
		: chg->otg_present ? POWER_SUPPLY_SCOPE_SYSTEM
		: POWER_SUPPLY_SCOPE_UNKNOWN;

	return 0;
}

static int smblib_get_typec_connector_temp_status(struct smb_charger *chg)
{
	int rc;
	u8 stat;

	if (chg->connector_health != -EINVAL)
		return chg->connector_health;

	if (chg->wa_flags & SW_THERM_REGULATION_WA) {
		if (chg->connector_temp == -ENODATA)
			return POWER_SUPPLY_HEALTH_UNKNOWN;

		if (chg->connector_temp > CONNECTOR_TEMP_RST_THRESH)
			return POWER_SUPPLY_HEALTH_OVERHEAT;

		if (chg->connector_temp > CONNECTOR_TEMP_REG_H_THRESH)
			return POWER_SUPPLY_HEALTH_HOT;

		if (chg->connector_temp > CONNECTOR_TEMP_REG_L_THRESH)
			return POWER_SUPPLY_HEALTH_WARM;

		return POWER_SUPPLY_HEALTH_COOL;
	}

	rc = smblib_read(chg, CONNECTOR_TEMP_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read CONNECTOR_TEMP_STATUS_REG, rc=%d\n",
				rc);
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	if (stat & CONNECTOR_TEMP_RST_BIT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;

	if (stat & CONNECTOR_TEMP_UB_BIT)
		return POWER_SUPPLY_HEALTH_HOT;

	if (stat & CONNECTOR_TEMP_LB_BIT)
		return POWER_SUPPLY_HEALTH_WARM;

	return POWER_SUPPLY_HEALTH_COOL;
}

int smblib_get_skin_temp_status(struct smb_charger *chg)
{
	int rc;
	u8 stat;

	if (!chg->en_skin_therm_mitigation)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	rc = smblib_read(chg, SKIN_TEMP_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read SKIN_TEMP_STATUS_REG, rc=%d\n",
				rc);
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	if (stat & SKIN_TEMP_RST_BIT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;

	if (stat & SKIN_TEMP_UB_BIT)
		return POWER_SUPPLY_HEALTH_HOT;

	if (stat & SKIN_TEMP_LB_BIT)
		return POWER_SUPPLY_HEALTH_WARM;

	return POWER_SUPPLY_HEALTH_COOL;
}

int smblib_get_prop_connector_health(struct smb_charger *chg)
{
	bool dc_present, usb_present;
	int input_present;
	int rc;

	rc = smblib_is_input_present(chg, &input_present);
	if (rc < 0)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	dc_present = input_present & INPUT_PRESENT_DC;
	usb_present = input_present & INPUT_PRESENT_USB;

	if (usb_present)
		return smblib_get_typec_connector_temp_status(chg);

	/*
	 * In PM8150B, SKIN channel measures Wireless charger receiver
	 * temp, used to regulate DC ICL.
	 */
	if (chg->chg_param.smb_version == PM8150B_SUBTYPE && dc_present)
		return smblib_get_skin_temp_status(chg);

	return POWER_SUPPLY_HEALTH_COOL;
}

#if 0
static int get_rp_based_dcp_current(struct smb_charger *chg, int typec_mode)
{
	int rp_ua;

	switch (typec_mode) {
	case POWER_SUPPLY_TYPEC_SOURCE_HIGH:
		rp_ua = TYPEC_HIGH_CURRENT_UA;
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_MEDIUM:
	case POWER_SUPPLY_TYPEC_SOURCE_DEFAULT:
	/* fall through */
	default:
		rp_ua = DCP_CURRENT_UA;
	}

	return rp_ua;
}
#endif

/*******************
 * USB PSY SETTERS *
 * *****************/

int smblib_set_prop_pd_current_max(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc, icl;

	if (chg->pd_active) {
		icl = get_client_vote(chg->usb_icl_votable, PD_VOTER);
		rc = vote(chg->usb_icl_votable, PD_VOTER, true, val->intval);
		if (val->intval != icl)
			power_supply_changed(chg->usb_psy);
	} else {
		rc = -EPERM;
	}

	return rc;
}

#define FLOAT_CHARGER_UA		1000000
#define SUSPEND_CURRENT_UA		2000
static int smblib_handle_usb_current(struct smb_charger *chg,
					int usb_current)
{
	int rc = 0, typec_mode;
	bool is_float = false;
	union power_supply_propval val = {0, };

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_FLOAT && (usb_current == SUSPEND_CURRENT_UA))
		is_float = true;

	if ((usb_current > 0 && usb_current < USBIN_500MA) || (usb_current == USBIN_900MA))
		usb_current = USBIN_500MA;
	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_FLOAT) {
		if (usb_current == -ETIMEDOUT || is_float) {
			/* we do not use USB500mA for float charger */
#if 0
			if ((chg->float_cfg & FLOAT_OPTIONS_MASK)
						== FORCE_FLOAT_SDP_CFG_BIT) {

				/*
				 * Confiugure USB500 mode if Float charger is
				 * configured for SDP mode.
				 */
				rc = vote(chg->usb_icl_votable,
					SW_ICL_MAX_VOTER, true, USBIN_500MA);
				if (rc < 0)
					smblib_err(chg,
						"Couldn't set SDP ICL rc=%d\n",
						rc);
				return rc;
			}
#endif
			if (chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC) {
				/*
				 * Valid FLOAT charger, report the current
				 * based of Rp.
				 */
				typec_mode = smblib_get_prop_typec_mode(chg);
				rc = vote(chg->usb_icl_votable,
						SW_ICL_MAX_VOTER, true, FLOAT_CHARGER_UA);
				if (rc < 0)
					return rc;
			} else {
				rc = vote(chg->usb_icl_votable,
					SW_ICL_MAX_VOTER, true, DCP_CURRENT_UA);
				if (rc < 0)
					return rc;
			}
		} else {
			/*
			 * FLOAT charger detected as SDP by USB driver,
			 * charge with the requested current and update the
			 * real_charger_type
			 */
			chg->real_charger_type = POWER_SUPPLY_TYPE_USB;
			rc = vote(chg->usb_icl_votable, USB_PSY_VOTER,
						true, usb_current);
			if (rc < 0)
				return rc;
			rc = vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER,
							false, 0);
			if (rc < 0)
				return rc;
		}
	} else {
		rc = smblib_get_prop_usb_present(chg, &val);
		if (!rc && !val.intval)
			return 0;

		/* if flash is active force 500mA */
		if ((usb_current < SDP_CURRENT_UA) && is_flash_active(chg))
			usb_current = SDP_CURRENT_UA;

		rc = vote(chg->usb_icl_votable, USB_PSY_VOTER, true,
							usb_current);
		if (rc < 0) {
			pr_err("Couldn't vote ICL USB_PSY_VOTER rc=%d\n", rc);
			return rc;
		}

		rc = vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
		if (rc < 0) {
			pr_err("Couldn't remove SW_ICL_MAX vote rc=%d\n", rc);
			return rc;
		}

	}

	return 0;
}

int smblib_set_prop_sdp_current_max(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	union power_supply_propval pval;
	int rc = 0;

	if (chg->power_good_en) {
		smblib_dbg(chg, PR_MISC, "power good on, don't set sdp icl\n");
		return rc;
	}

	if (!chg->pd_active) {
		rc = smblib_get_prop_usb_present(chg, &pval);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get usb present rc = %d\n",
						rc);
			return rc;
		}

		/* handle the request only when USB is present */
		if (pval.intval)
			rc = smblib_handle_usb_current(chg, val->intval);
	} else if (chg->system_suspend_supported) {
		if (val->intval <= USBIN_25MA)
			rc = vote(chg->usb_icl_votable,
				PD_SUSPEND_SUPPORTED_VOTER, true, val->intval);
		else
			rc = vote(chg->usb_icl_votable,
				PD_SUSPEND_SUPPORTED_VOTER, false, 0);
	}
	return rc;
}

int smblib_set_prop_boost_current(struct smb_charger *chg,
					const union power_supply_propval *val)
{
	int rc = 0;

	rc = smblib_set_charge_param(chg, &chg->param.freq_switcher,
				val->intval <= chg->boost_threshold_ua ?
				chg->chg_freq.freq_below_otg_threshold :
				chg->chg_freq.freq_above_otg_threshold);
	if (rc < 0) {
		dev_err(chg->dev, "Error in setting freq_boost rc=%d\n", rc);
		return rc;
	}

	chg->boost_current_ua = val->intval;
	return rc;
}

int smblib_set_prop_usb_voltage_max_limit(struct smb_charger *chg,
					const union power_supply_propval *val)
{
	union power_supply_propval pval = {0, };

	/* Exit if same value is re-configured */
	if (val->intval == chg->usbin_forced_max_uv)
		return 0;

	smblib_get_prop_usb_voltage_max_design(chg, &pval);

	if (val->intval >= MICRO_5V && val->intval <= pval.intval) {
		chg->usbin_forced_max_uv = val->intval;
		smblib_dbg(chg, PR_MISC, "Max VBUS limit changed to: %d\n",
				val->intval);
	} else if (chg->usbin_forced_max_uv) {
		chg->usbin_forced_max_uv = 0;
	} else {
		return 0;
	}

	power_supply_changed(chg->usb_psy);

	return 0;
}

void smblib_typec_irq_config(struct smb_charger *chg, bool en)
{
	if (en == chg->typec_irq_en)
		return;

	if (en) {
		enable_irq(
			chg->irq_info[TYPEC_ATTACH_DETACH_IRQ].irq);
		enable_irq(
			chg->irq_info[TYPEC_CC_STATE_CHANGE_IRQ].irq);
		enable_irq(
			chg->irq_info[TYPEC_OR_RID_DETECTION_CHANGE_IRQ].irq);
	} else {
		disable_irq_nosync(
			chg->irq_info[TYPEC_ATTACH_DETACH_IRQ].irq);
		disable_irq_nosync(
			chg->irq_info[TYPEC_CC_STATE_CHANGE_IRQ].irq);
		disable_irq_nosync(
			chg->irq_info[TYPEC_OR_RID_DETECTION_CHANGE_IRQ].irq);
	}

	chg->typec_irq_en = en;
}

#define PR_LOCK_TIMEOUT_MS	1000
int smblib_set_prop_typec_power_role(struct smb_charger *chg,
				     const union power_supply_propval *val)
{
	int rc = 0;
	u8 power_role;
	enum power_supply_typec_mode typec_mode;
	bool snk_attached = false, src_attached = false, is_pr_lock = false;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return 0;

	smblib_dbg(chg, PR_MISC, "power role change: %d --> %d!",
			chg->power_role, val->intval);

	if (chg->power_role == val->intval) {
		smblib_dbg(chg, PR_MISC, "power role already in %d, ignore!",
				chg->power_role);
		return 0;
	}

	typec_mode = smblib_get_prop_typec_mode(chg);
	if (typec_mode >= POWER_SUPPLY_TYPEC_SINK &&
			typec_mode <= POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER)
		snk_attached = true;
	else if (typec_mode >= POWER_SUPPLY_TYPEC_SOURCE_DEFAULT &&
			typec_mode <= POWER_SUPPLY_TYPEC_SOURCE_HIGH)
		src_attached = true;

	/*
	 * If current power role is in DRP, and type-c is already in the
	 * mode (source or sink) that's being requested, it means this is
	 * a power role locking request from USBPD driver. Disable type-c
	 * related interrupts for locking power role to avoid the redundant
	 * notifications.
	 */
	if ((chg->power_role == POWER_SUPPLY_TYPEC_PR_DUAL) &&
		((src_attached && val->intval == POWER_SUPPLY_TYPEC_PR_SINK) ||
		(snk_attached && val->intval == POWER_SUPPLY_TYPEC_PR_SOURCE)))
		is_pr_lock = true;

	smblib_dbg(chg, PR_MISC, "snk_attached = %d, src_attached = %d, is_pr_lock = %d\n",
			snk_attached, src_attached, is_pr_lock);
	cancel_delayed_work(&chg->pr_lock_clear_work);
	spin_lock(&chg->typec_pr_lock);
	if (!chg->pr_lock_in_progress && is_pr_lock) {
		smblib_dbg(chg, PR_MISC, "disable type-c interrupts for power role locking\n");
		smblib_typec_irq_config(chg, false);
		schedule_delayed_work(&chg->pr_lock_clear_work,
					msecs_to_jiffies(PR_LOCK_TIMEOUT_MS));
	} else if (chg->pr_lock_in_progress && !is_pr_lock) {
		smblib_dbg(chg, PR_MISC, "restore type-c interrupts after exit power role locking\n");
		smblib_typec_irq_config(chg, true);
	}

	chg->pr_lock_in_progress = is_pr_lock;
	spin_unlock(&chg->typec_pr_lock);

	switch (val->intval) {
	case POWER_SUPPLY_TYPEC_PR_NONE:
		power_role = TYPEC_DISABLE_CMD_BIT;
		break;
	case POWER_SUPPLY_TYPEC_PR_DUAL:
		power_role = chg->typec_try_mode;
		break;
	case POWER_SUPPLY_TYPEC_PR_SINK:
		power_role = EN_SNK_ONLY_BIT;
		break;
	case POWER_SUPPLY_TYPEC_PR_SOURCE:
		power_role = EN_SRC_ONLY_BIT;
		break;
	default:
		smblib_err(chg, "power role %d not supported\n", val->intval);
		return -EINVAL;
	}

	pr_err("set power_role to: %d\n", power_role);
	rc = smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
				TYPEC_POWER_ROLE_CMD_MASK | TYPEC_TRY_MODE_MASK,
				power_role);
	if (rc < 0) {
		smblib_err(chg, "Couldn't write 0x%02x to TYPE_C_INTRPT_ENB_SOFTWARE_CTRL rc=%d\n",
			power_role, rc);
		return rc;
	}

	chg->power_role = val->intval;
	return rc;
}

int smblib_set_prop_typec_boost_otg_disable(struct smb_charger *chg,
				     const union power_supply_propval *val)
{
	int rc = 0;

	if (!chg->wireless_bq)
		return 0;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return 0;

	if (val->intval == 1 && !chg->power_good_en) {
		pr_err("Type-C snk disconnect and no wireless charging, set bq_en q1 gpio to low now\n");
		smblib_dc_chg_q1_enable(chg, false);
	}

	return rc;
}


int smblib_set_prop_typec_select_rp(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc;

	if (!typec_in_src_mode(chg)) {
		smblib_err(chg, "Couldn't set curr src: not in SRC mode\n");
		return -EINVAL;
	}

	if (val->intval < TYPEC_SRC_RP_MAX_ELEMENTS) {
		/*Mi:modify otg src cap current to 0.5A*/
		rc = smblib_masked_write(chg, TYPE_C_CURRSRC_CFG_REG,
				TYPEC_SRC_RP_SEL_MASK,
				0);
		if (rc < 0)
			smblib_err(chg, "Couldn't write to TYPE_C_CURRSRC_CFG rc=%d\n",
					rc);
		return rc;
	}

	return -EINVAL;
}

int smblib_set_prop_pd_voltage_min(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc, min_uv;

	min_uv = min(val->intval, chg->voltage_max_uv);
	if (chg->voltage_min_uv == min_uv)
		return 0;

	rc = smblib_set_usb_pd_allowed_voltage(chg, min_uv,
					       chg->voltage_max_uv);
	if (rc < 0) {
		smblib_err(chg, "invalid min voltage %duV rc=%d\n",
			val->intval, rc);
		return rc;
	}

	chg->voltage_min_uv = min_uv;
	power_supply_changed(chg->usb_main_psy);

	return rc;
}

int smblib_set_prop_pd_voltage_max(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc, max_uv;

	max_uv = max(val->intval, chg->voltage_min_uv);
	if (chg->voltage_max_uv == max_uv)
		return 0;

	rc = smblib_set_usb_pd_fsw(chg, max_uv);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set FSW for voltage %duV rc=%d\n",
			val->intval, rc);
		return rc;
	}

	rc = smblib_set_usb_pd_allowed_voltage(chg, chg->voltage_min_uv,
					       max_uv);
	if (rc < 0) {
		smblib_err(chg, "invalid max voltage %duV rc=%d\n",
			val->intval, rc);
		return rc;
	}

	chg->voltage_max_uv = max_uv;
	power_supply_changed(chg->usb_main_psy);

	return rc;
}

int smblib_set_prop_pd_active(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	const struct apsd_result *apsd = smblib_get_apsd_result(chg);

	int rc = 0;
	int sec_charger, typec_mode;

	/*
	 * Ignore repetitive notification while PD is active, which
	 * is caused by hard reset.
	 */
	if (chg->pd_active && chg->pd_active == val->intval)
		return 0;

	chg->pd_active = val->intval;

	smblib_apsd_enable(chg, !chg->pd_active);

	if (!chg->pd && chg->use_bq_pump) {
		chg->pd = devm_usbpd_get_by_phandle(chg->dev,
				"qcom,usbpd-phandle");
		if (IS_ERR_OR_NULL(chg->pd))
			pr_err("Failed to get pd handle %ld\n",
					PTR_ERR(chg->pd));
	}
	update_sw_icl_max(chg, apsd->pst);

	if (chg->pd_active) {
		vote(chg->limited_irq_disable_votable, CHARGER_TYPE_VOTER,
				false, 0);
		vote(chg->hdc_irq_disable_votable, CHARGER_TYPE_VOTER,
				false, 0);

		/*
		 * Enforce 100mA for PD until the real vote comes in later.
		 * It is guaranteed that pd_active is set prior to
		 * pd_current_max
		 */
		vote(chg->usb_icl_votable, PD_VOTER, true, USBIN_100MA);
		vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
		/*set the fcc to PD_UNVERIFED_CURRENT when pd is not verifed*/
		if (!chg->pd_verifed) {
			rc = vote(chg->fcc_votable, PD_VERIFED_VOTER,
					true, PD_UNVERIFED_CURRENT);
			if (rc < 0)
				smblib_err(chg, "Couldn't unvote PD_VERIFED_VOTER, rc=%d\n", rc);
		} else {
			vote(chg->fcc_votable, PD_VERIFED_VOTER, false, 0);
		}
		/*
		 * For PPS, Charge Pump is preferred over parallel charger if
		 * present.
		 */
		if (chg->pd_active == POWER_SUPPLY_PD_PPS_ACTIVE
						&& chg->sec_cp_present) {
			rc = smblib_select_sec_charger(chg,
						POWER_SUPPLY_CHARGER_SEC_CP,
						POWER_SUPPLY_CP_PPS, false);
			if (rc < 0)
				dev_err(chg->dev, "Couldn't enable secondary charger rc=%d\n",
					rc);
		}

		if (chg->pd_active == POWER_SUPPLY_PD_PPS_ACTIVE
						&& chg->six_pin_step_charge_enable) {
			/* start six pin battery step charge monitor work */
			schedule_delayed_work(&chg->six_pin_batt_step_chg_work,
					msecs_to_jiffies(STEP_CHG_DELAYED_START_MS));
		}
		smblib_therm_charging(chg);
	} else {
		vote(chg->usb_icl_votable, PD_VOTER, false, 0);
		vote(chg->limited_irq_disable_votable, CHARGER_TYPE_VOTER,
				true, 0);
		vote(chg->hdc_irq_disable_votable, CHARGER_TYPE_VOTER,
				true, 0);

		sec_charger = chg->sec_pl_present ?
						POWER_SUPPLY_CHARGER_SEC_PL :
						POWER_SUPPLY_CHARGER_SEC_NONE;
		rc = smblib_select_sec_charger(chg, sec_charger,
						POWER_SUPPLY_CP_NONE, false);
		if (rc < 0)
			dev_err(chg->dev,
				"Couldn't enable secondary charger rc=%d\n",
					rc);

		/* PD hard resets failed, proceed to detect QC2/3 */
		if (chg->ok_to_pd) {
			chg->ok_to_pd = false;
			smblib_hvdcp_detect_try_enable(chg, true);
		}
	}

	if (!chg->fake_usb_insertion){
		smblib_usb_pd_adapter_allowance_override(chg,
			!!chg->pd_active ? FORCE_5V : FORCE_NULL);
	}

	smblib_update_usb_type(chg);

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB &&
			!chg->ok_to_pd) {
		typec_mode = smblib_get_prop_typec_mode(chg);
		if (typec_rp_med_high(chg, typec_mode))
			vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
	}

	power_supply_changed(chg->usb_psy);
	return rc;
}

int smblib_set_prop_ship_mode(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc;

	smblib_dbg(chg, PR_MISC, "Set ship mode: %d!!\n", !!val->intval);

	rc = smblib_masked_write(chg, SHIP_MODE_REG, SHIP_MODE_EN_BIT,
			!!val->intval ? SHIP_MODE_EN_BIT : 0);
	if (rc < 0)
		dev_err(chg->dev, "Couldn't %s ship mode, rc=%d\n",
				!!val->intval ? "enable" : "disable", rc);

	return rc;
}

int smblib_set_prop_pd_in_hard_reset(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc = 0;

	if (chg->pd_hard_reset == val->intval)
		return rc;

	chg->pd_hard_reset = val->intval;
	rc = smblib_masked_write(chg, TYPE_C_EXIT_STATE_CFG_REG,
			EXIT_SNK_BASED_ON_CC_BIT,
			(chg->pd_hard_reset) ? EXIT_SNK_BASED_ON_CC_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't set EXIT_SNK_BASED_ON_CC rc=%d\n",
				rc);

	return rc;
}

#define JEITA_SOFT			0
#define JEITA_HARD			1
static int smblib_update_jeita(struct smb_charger *chg, u32 *thresholds,
								int type)
{
	int rc;
	u16 temp, base;

	base = CHGR_JEITA_THRESHOLD_BASE_REG(type);

	temp = thresholds[1] & 0xFFFF;
	temp = ((temp & 0xFF00) >> 8) | ((temp & 0xFF) << 8);
	rc = smblib_batch_write(chg, base, (u8 *)&temp, 2);
	if (rc < 0) {
		smblib_err(chg,
			"Couldn't configure Jeita %s hot threshold rc=%d\n",
			(type == JEITA_SOFT) ? "Soft" : "Hard", rc);
		return rc;
	}

	temp = thresholds[0] & 0xFFFF;
	temp = ((temp & 0xFF00) >> 8) | ((temp & 0xFF) << 8);
	rc = smblib_batch_write(chg, base + 2, (u8 *)&temp, 2);
	if (rc < 0) {
		smblib_err(chg,
			"Couldn't configure Jeita %s cold threshold rc=%d\n",
			(type == JEITA_SOFT) ? "Soft" : "Hard", rc);
		return rc;
	}

	smblib_dbg(chg, PR_MISC, "%s Jeita threshold configured\n",
				(type == JEITA_SOFT) ? "Soft" : "Hard");

	return 0;
}

static int smblib_charge_inhibit_en(struct smb_charger *chg, bool enable)
{
	int rc;

	rc = smblib_masked_write(chg, CHGR_CFG2_REG,
					CHARGER_INHIBIT_BIT,
					enable ? CHARGER_INHIBIT_BIT : 0);
	return rc;
}

static int smblib_soft_jeita_arb_wa(struct smb_charger *chg)
{
	union power_supply_propval pval;
	int rc = 0;
	bool soft_jeita;

	rc = smblib_get_prop_batt_health(chg, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get battery health rc=%d\n", rc);
		return rc;
	}

	/* Do nothing on entering hard JEITA condition */
	if (pval.intval == POWER_SUPPLY_HEALTH_COLD ||
		pval.intval == POWER_SUPPLY_HEALTH_HOT)
		return 0;

	if (chg->jeita_soft_fcc[0] < 0 || chg->jeita_soft_fcc[1] < 0 ||
		chg->jeita_soft_fv[0] < 0 || chg->jeita_soft_fv[1] < 0)
		return 0;

	soft_jeita = (pval.intval == POWER_SUPPLY_HEALTH_COOL) ||
			(pval.intval == POWER_SUPPLY_HEALTH_WARM);

	/* Do nothing on entering soft JEITA from hard JEITA */
	if (chg->jeita_arb_flag && soft_jeita)
		return 0;

	/* Do nothing, initial to health condition */
	if (!chg->jeita_arb_flag && !soft_jeita)
		return 0;

	/* Entering soft JEITA from normal state */
	if (!chg->jeita_arb_flag && soft_jeita) {
		vote(chg->chg_disable_votable, JEITA_ARB_VOTER, true, 0);

		rc = smblib_charge_inhibit_en(chg, true);
		if (rc < 0)
			smblib_err(chg, "Couldn't enable charge inhibit rc=%d\n",
					rc);

		rc = smblib_update_jeita(chg, chg->jeita_soft_hys_thlds,
					JEITA_SOFT);
		if (rc < 0)
			smblib_err(chg,
				"Couldn't configure Jeita soft threshold rc=%d\n",
				rc);

		if (pval.intval == POWER_SUPPLY_HEALTH_COOL) {
			vote(chg->fcc_votable, JEITA_ARB_VOTER, true,
						chg->jeita_soft_fcc[0]);
			vote(chg->fv_votable, JEITA_ARB_VOTER, true,
						chg->jeita_soft_fv[0]);
		} else {
			vote(chg->fcc_votable, JEITA_ARB_VOTER, true,
						chg->jeita_soft_fcc[1]);
			vote(chg->fv_votable, JEITA_ARB_VOTER, true,
						chg->jeita_soft_fv[1]);
		}

		vote(chg->chg_disable_votable, JEITA_ARB_VOTER, false, 0);
		chg->jeita_arb_flag = true;
	} else if (chg->jeita_arb_flag && !soft_jeita) {
		/* Exit to health state from soft JEITA */

		vote(chg->chg_disable_votable, JEITA_ARB_VOTER, true, 0);

		rc = smblib_charge_inhibit_en(chg, false);
		if (rc < 0)
			smblib_err(chg, "Couldn't disable charge inhibit rc=%d\n",
					rc);

		rc = smblib_update_jeita(chg, chg->jeita_soft_thlds,
							JEITA_SOFT);
		if (rc < 0)
			smblib_err(chg, "Couldn't configure Jeita soft threshold rc=%d\n",
				rc);

		vote(chg->fcc_votable, JEITA_ARB_VOTER, false, 0);
		vote(chg->fv_votable, JEITA_ARB_VOTER, false, 0);
		vote(chg->chg_disable_votable, JEITA_ARB_VOTER, false, 0);
		chg->jeita_arb_flag = false;
	}

	smblib_dbg(chg, PR_MISC, "JEITA ARB status %d, soft JEITA status %d\n",
			chg->jeita_arb_flag, soft_jeita);
	return rc;
}

/************************
 * USB MAIN PSY GETTERS *
 ************************/
int smblib_get_prop_fcc_delta(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc, jeita_cc_delta_ua = 0;

	if (chg->sw_jeita_enabled) {
		val->intval = 0;
		return 0;
	}

	rc = smblib_get_jeita_cc_delta(chg, &jeita_cc_delta_ua);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get jeita cc delta rc=%d\n", rc);
		jeita_cc_delta_ua = 0;
	}

	val->intval = jeita_cc_delta_ua;
	return 0;
}

/************************
 * USB MAIN PSY SETTERS *
 ************************/
int smblib_get_charge_current(struct smb_charger *chg,
				int *total_current_ua)
{
	const struct apsd_result *apsd_result = smblib_get_apsd_result(chg);
	union power_supply_propval val = {0, };
	int rc = 0, typec_source_rd, current_ua;
	bool non_compliant;
	u8 stat;

	if (chg->pd_active) {
		*total_current_ua =
			get_client_vote_locked(chg->usb_icl_votable, PD_VOTER);
		return rc;
	}

	rc = smblib_read(chg, LEGACY_CABLE_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_5 rc=%d\n", rc);
		return rc;
	}
	non_compliant = stat & TYPEC_NONCOMP_LEGACY_CABLE_STATUS_BIT;

	/* get settled ICL */
	rc = smblib_get_prop_input_current_settled(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get settled ICL rc=%d\n", rc);
		return rc;
	}

	typec_source_rd = smblib_get_prop_ufp_mode(chg);

	/* QC 3.5 adapter */
	if (chg->qc3p5_supported &&
			(apsd_result->pst == POWER_SUPPLY_TYPE_USB_HVDCP_3P5) &&
			(chg->qc3p5_power_limit_w == 40)) {
		*total_current_ua = HVDCP3P5_40W_CURRENT_UA;
		return 0;
	}

	/* QC 2.0/3.0 adapter */
	if (apsd_result->bit & (QC_3P0_BIT | QC_2P0_BIT)) {
		*total_current_ua = HVDCP_CURRENT_UA;
		return 0;
	}

	if (non_compliant && !chg->typec_legacy_use_rp_icl) {
		switch (apsd_result->bit) {
		case CDP_CHARGER_BIT:
			current_ua = CDP_CURRENT_UA;
			break;
		case DCP_CHARGER_BIT:
		case OCP_CHARGER_BIT:
		case FLOAT_CHARGER_BIT:
			current_ua = DCP_CURRENT_UA;
			break;
		default:
			current_ua = 0;
			break;
		}

		*total_current_ua = max(current_ua, val.intval);
		return 0;
	}

	switch (typec_source_rd) {
	case POWER_SUPPLY_TYPEC_SOURCE_DEFAULT:
		switch (apsd_result->bit) {
		case CDP_CHARGER_BIT:
			current_ua = CDP_CURRENT_UA;
			break;
		case DCP_CHARGER_BIT:
		case OCP_CHARGER_BIT:
		case FLOAT_CHARGER_BIT:
			current_ua = chg->default_icl_ua;
			break;
		default:
			current_ua = 0;
			break;
		}
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_MEDIUM:
		current_ua = TYPEC_MEDIUM_CURRENT_UA;
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_HIGH:
		current_ua = TYPEC_HIGH_CURRENT_UA;
		break;
	case POWER_SUPPLY_TYPEC_NON_COMPLIANT:
	case POWER_SUPPLY_TYPEC_NONE:
	default:
		current_ua = 0;
		break;
	}

	*total_current_ua = max(current_ua, val.intval);
	return 0;
}

#define IADP_OVERHEAT_UA	500000
int smblib_set_prop_thermal_overheat(struct smb_charger *chg,
						int therm_overheat)
{
	int icl_ua = 0;

	if (chg->thermal_overheat == !!therm_overheat)
		return 0;

	/* Configure ICL to 500mA in case system health is Overheat */
	if (therm_overheat)
		icl_ua = IADP_OVERHEAT_UA;

	if (!chg->cp_disable_votable)
		chg->cp_disable_votable = find_votable("CP_DISABLE");

	if (chg->cp_disable_votable) {
		vote(chg->cp_disable_votable, OVERHEAT_LIMIT_VOTER,
							therm_overheat, 0);
		vote(chg->usb_icl_votable, OVERHEAT_LIMIT_VOTER,
							therm_overheat, icl_ua);
	}

	chg->thermal_overheat = !!therm_overheat;
	return 0;

}

/***************************************************
 * Enable OTG CHECK BY IRQ WHILE WIRELESS CHARGING *
 ***************************************************/
static void smblib_enable_otg_check_wl(struct smb_charger *chg, int enable){
	if(enable){
		smblib_masked_write(chg, TYPE_C_MODE_CFG_REG, EN_TRY_SNK_BIT, 0);
		smblib_masked_write(chg, TYPE_C_EXIT_STATE_CFG_REG, BYPASS_VSAFE0V_DURING_ROLE_SWAP_BIT, BYPASS_VSAFE0V_DURING_ROLE_SWAP_BIT);
	}else{
		smblib_masked_write(chg, TYPE_C_MODE_CFG_REG, EN_TRY_SNK_BIT, EN_TRY_SNK_BIT);
		smblib_masked_write(chg, TYPE_C_EXIT_STATE_CFG_REG, BYPASS_VSAFE0V_DURING_ROLE_SWAP_BIT, 0);
	}
}

/**********************
 * INTERRUPT HANDLERS *
 **********************/

irqreturn_t default_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	return IRQ_HANDLED;
}

#define DELAY_BEFORE_OPEN_NCP_MS 50
#define WIRELESS_INIT_ICL_UA 30000
irqreturn_t dc_power_on_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc;
	u8 stat;
	union power_supply_propval pval = {0, };
	bool dc_power_on;

	smblib_dbg(chg, PR_OEM, "IRQ: %s\n", irq_data->name);

	rc = smblib_read(chg, DCIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read DCIN_RT_STS rc=%d\n", rc);
		return IRQ_HANDLED;
	}
	printk("dcin int rt status stat val is 0x%x, reverse_gpio_state;%d\n", stat, chg->reverse_gpio_state);

	/* return while in reverse charging or fw updating state */
	if (chg->wireless_bq) {
		if (chg->reverse_gpio_state == REVERSE_GPIO_STATE_START
			||chg->reverse_gpio_state == REVERSE_GPIO_STATE_END) {

			/* if reverse gpio is end, set status to unset for normal operation */
			if (chg->reverse_gpio_state == REVERSE_GPIO_STATE_END) {
				chg->reverse_gpio_state = REVERSE_GPIO_STATE_UNSET;
			}

			/* ignore all the irq between REVERSE_GPIO_STATE_START and REVERSE_GPIO_STATE_END */
			return IRQ_HANDLED;
		}
	}
	rc = smblib_get_prop_dc_present(chg, &pval);
	if (rc < 0) {
		pr_err("Couldn't get dc present rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	dc_power_on = pval.intval;

	if (chg->wireless_bq) {
		if (dc_power_on) {
			/* when dc plug in, enable batt_temp irq wakeup */
			if (chg->irq_info[BAT_TEMP_IRQ].irq && !chg->batt_temp_irq_enabled) {
				enable_irq_wake(chg->irq_info[BAT_TEMP_IRQ].irq);
				chg->batt_temp_irq_enabled = true;
			}
			if (chg->power_good_en) {
				smblib_enable_otg_check_wl(chg, true);
				vote(chg->usb_icl_votable, WIRELESS_BY_USB_IN_VOTER,
					true, WIRELESS_INIT_ICL_UA);
				schedule_delayed_work(&chg->dc_power_work, msecs_to_jiffies(DELAY_BEFORE_OPEN_NCP_MS));
				smblib_set_wireless_present(chg, true);
				if (chg->six_pin_step_charge_enable)
					smblib_get_start_vbat_before_step_charge(chg);
#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
				smblib_check_batt_authentic(chg);
#endif
			} else {
				pr_err("dc_power_on without power good\n");
			}
			vote(chg->awake_votable, DC_AWAKE_VOTER, true, 0);
		} else {
			/* when dc plug out, disable batt_temp irq wakeup */
			if (chg->irq_info[BAT_TEMP_IRQ].irq && chg->batt_temp_irq_enabled) {
				disable_irq_wake(chg->irq_info[BAT_TEMP_IRQ].irq);
				chg->batt_temp_irq_enabled = false;
			}
			/* when dc plug out, set bark timer back to default 16s */
			smblib_set_wdog_bark_timer(chg, BARK_TIMER_NORMAL);
			if (chg->six_pin_step_charge_enable) {
				chg->init_start_vbat_checked = false;
				chg->trigger_taper_count = 0;
				chg->index_vfloat = 0;
				chg->health_not_good = false;
				vote(chg->fv_votable, SIX_PIN_VFLOAT_VOTER,
						false, 0);
				vote(chg->fcc_votable, SIX_PIN_VFLOAT_VOTER,
						false, 0);
				vote(chg->usb_icl_votable, MAIN_ICL_MIN_VOTER,
						false, 0);
			}
			chg->report_input_absent = false;
			smblib_enable_otg_check_wl(chg, false);
			smblib_set_wireless_present(chg, false);
			cancel_delayed_work(&chg->dc_power_work);
			if (is_ncp3902_pull_low(chg))
				smblib_ncp3902_enable(chg, true);
			/* when dc plug out, set bark timer back to default 16s */
			smblib_set_wdog_bark_timer(chg, BARK_TIMER_NORMAL);
			vote(chg->usb_icl_votable, WIRELESS_BY_USB_IN_VOTER,
					true, USBIN_30MA);
			vote(chg->usb_icl_votable, WIRELESS_BY_USB_IN_VOTER,
					false, 0);
			vote(chg->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER,
					false, 0);
			vote(chg->awake_votable, DC_AWAKE_VOTER, false, 0);
		}
	}

	if (chg->dc_psy)
		power_supply_changed(chg->dc_psy);
	if (chg->wls_psy)
		power_supply_changed(chg->wls_psy);

	return IRQ_HANDLED;
}

irqreturn_t smb_en_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc, input_present;

	if (!chg->cp_disable_votable) {
		chg->cp_disable_votable = find_votable("CP_DISABLE");
		if (!chg->cp_disable_votable)
			return IRQ_HANDLED;
	}

	if (chg->pd_hard_reset) {
		vote(chg->cp_disable_votable, BOOST_BACK_VOTER, true, 0);
		return IRQ_HANDLED;
	}

	rc = smblib_is_input_present(chg, &input_present);
	if (rc < 0) {
		pr_err("Couldn't get usb presence status rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	if (input_present) {
		/*
		 * Add some delay to enable SMB1390 switcher after SMB_EN
		 * pin goes high
		 */
		usleep_range(1000, 1100);
		vote(chg->cp_disable_votable, BOOST_BACK_VOTER, false, 0);
	}

	return IRQ_HANDLED;
}

irqreturn_t sdam_sts_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	mutex_lock(&chg->irq_status_lock);
	chg->irq_status |= PULSE_SKIP_IRQ_BIT;
	mutex_unlock(&chg->irq_status_lock);

	power_supply_changed(chg->usb_main_psy);

	return IRQ_HANDLED;
}

#define CHG_TERM_WA_ENTRY_DELAY_MS		300000		/* 5 min */
#define CHG_TERM_WA_EXIT_DELAY_MS		60000		/* 1 min */
static void smblib_eval_chg_termination(struct smb_charger *chg, u8 batt_status)
{
	union power_supply_propval pval = {0, };
	int rc = 0;

	if (chg->ext_fg)
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CAPACITY_RAW, &pval);
	else
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_REAL_CAPACITY, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read SOC value, rc=%d\n", rc);
		return;
	}

	if (chg->ext_fg)
		pval.intval = pval.intval / 100;
	/*
	 * Post charge termination, switch to BSM mode triggers the risk of
	 * over charging as BATFET opening may take some time post the necessity
	 * of staying in supplemental mode, leading to unintended charging of
	 * battery. Trigger the charge termination WA once charging is completed
	 * to prevent overcharing.
	 */
	if ((batt_status == TERMINATE_CHARGE) && (pval.intval == 100)) {
		smblib_err(chg, "term-current debug to enable alarm\n");
		chg->cc_soc_ref = 0;
		chg->last_cc_soc = 0;
		chg->term_vbat_uv = 0;
		alarm_start_relative(&chg->chg_termination_alarm,
				ms_to_ktime(CHG_TERM_WA_ENTRY_DELAY_MS));
	} else if (pval.intval < 100) {
		/*
		 * Reset CC_SOC reference value for charge termination WA once
		 * we exit the TERMINATE_CHARGE state and soc drops below 100%
		 */
		chg->cc_soc_ref = 0;
		chg->last_cc_soc = 0;
		chg->term_vbat_uv = 0;
	}
}

#define WIRELESS_DELAY_WAKE_MS 20000
irqreturn_t chg_state_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	u8 stat;
	int rc;
	int chg_en;

	smblib_dbg(chg, PR_OEM, "IRQ: %s\n", irq_data->name);

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
				rc);
		return IRQ_HANDLED;
	}

	stat = stat & BATTERY_CHARGER_STATUS_MASK;
	smblib_dbg(chg, PR_OEM, "term-current stat: %d\n", stat);
	if (chg->wa_flags & CHG_TERMINATION_WA)
		smblib_eval_chg_termination(chg, stat);

	chg_en = !(get_client_vote(chg->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER)
			== MAIN_CHG_SUSPEND_ICL);

	if (chg_en && (stat == TERMINATE_CHARGE) && (chg->power_good_en)) {
		smblib_dbg(chg, PR_OEM, "full delay clear dc wake lock\n");
		schedule_delayed_work(&chg->wireless_full_delay_work,
				msecs_to_jiffies(WIRELESS_DELAY_WAKE_MS));
	}
	chg->uusb_apsd_rerun_done = false;
	power_supply_changed(chg->batt_psy);
	return IRQ_HANDLED;
}

irqreturn_t batt_temp_changed_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	if (chg->jeita_configured != JEITA_CFG_COMPLETE)
		return IRQ_HANDLED;

	rc = smblib_soft_jeita_arb_wa(chg);
	if (rc < 0) {
		smblib_err(chg, "Couldn't fix soft jeita arb rc=%d\n",
				rc);
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

irqreturn_t batt_psy_changed_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	power_supply_changed(chg->batt_psy);
	return IRQ_HANDLED;
}

#define AICL_STEP_MV		200
#define MAX_AICL_THRESHOLD_MV	4800
irqreturn_t usbin_uv_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	struct storm_watch *wdata;
	const struct apsd_result *apsd = smblib_get_apsd_result(chg);
	int rc;
	u8 stat = 0, max_pulses = 0;

	smblib_dbg(chg, PR_OEM, "IRQ: %s\n", irq_data->name);

	if ((chg->wa_flags & WEAK_ADAPTER_WA)
			&& is_storming(&irq_data->storm_data)) {

		if (chg->aicl_max_reached) {
			smblib_dbg(chg, PR_MISC,
					"USBIN_UV storm at max AICL threshold\n");
			return IRQ_HANDLED;
		}

		smblib_dbg(chg, PR_MISC, "USBIN_UV storm at threshold %d\n",
				chg->aicl_5v_threshold_mv);

		/* suspend USBIN before updating AICL threshold */
		vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER, true, 0);

		/* delay for VASHDN deglitch */
		msleep(20);

		if (chg->aicl_5v_threshold_mv > MAX_AICL_THRESHOLD_MV) {
			/* reached max AICL threshold */
			chg->aicl_max_reached = true;
			goto unsuspend_input;
		}

		/* Increase AICL threshold by 200mV */
		rc = smblib_set_charge_param(chg, &chg->param.aicl_5v_threshold,
				chg->aicl_5v_threshold_mv + AICL_STEP_MV);
		if (rc < 0)
			dev_err(chg->dev,
				"Error in setting AICL threshold rc=%d\n", rc);
		else
			chg->aicl_5v_threshold_mv += AICL_STEP_MV;

		rc = smblib_set_charge_param(chg,
				&chg->param.aicl_cont_threshold,
				chg->aicl_cont_threshold_mv + AICL_STEP_MV);
		if (rc < 0)
			dev_err(chg->dev,
				"Error in setting AICL threshold rc=%d\n", rc);
		else
			chg->aicl_cont_threshold_mv += AICL_STEP_MV;

unsuspend_input:
		/* Force torch in boost mode to ensure it works with low ICL */
		if (chg->chg_param.smb_version == PMI632_SUBTYPE)
			schgm_flash_torch_priority(chg, TORCH_BOOST_MODE);

		if (chg->aicl_max_reached) {
			smblib_dbg(chg, PR_MISC,
				"Reached max AICL threshold resctricting ICL to 100mA\n");
			vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER,
					true, USBIN_100MA);
			smblib_run_aicl(chg, RESTART_AICL);
		} else {
			smblib_run_aicl(chg, RESTART_AICL);
			vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER,
					false, 0);
		}

		wdata = &chg->irq_info[USBIN_UV_IRQ].irq_data->storm_data;
		reset_storm_count(wdata);
	}

	if (!chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data)
		return IRQ_HANDLED;

	wdata = &chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data->storm_data;
	reset_storm_count(wdata);

	/* Workaround for non-QC2.0-compliant chargers follows */
	if (!chg->qc2_unsupported_voltage &&
			!chg->qc2_unsupported &&
			apsd->pst == POWER_SUPPLY_TYPE_USB_HVDCP) {
		rc = smblib_read(chg, QC_CHANGE_STATUS_REG, &stat);
		if (rc < 0)
			smblib_err(chg,
				"Couldn't read CHANGE_STATUS_REG rc=%d\n", rc);

		if (stat & QC_5V_BIT)
			return IRQ_HANDLED;

		rc = smblib_read(chg, HVDCP_PULSE_COUNT_MAX_REG, &max_pulses);
		if (rc < 0)
			smblib_err(chg,
				"Couldn't read QC2 max pulses rc=%d\n", rc);

		chg->qc2_max_pulses = (max_pulses &
				HVDCP_PULSE_COUNT_MAX_QC2_MASK);

		if (stat & QC_12V_BIT) {
			chg->qc2_unsupported_voltage = QC2_NON_COMPLIANT_12V;
			rc = smblib_masked_write(chg, HVDCP_PULSE_COUNT_MAX_REG,
					HVDCP_PULSE_COUNT_MAX_QC2_MASK,
					HVDCP_PULSE_COUNT_MAX_QC2_9V);
			if (rc < 0)
				smblib_err(chg, "Couldn't force max pulses to 9V rc=%d\n",
						rc);

		} else if (stat & QC_9V_BIT) {
			chg->qc2_unsupported_voltage = QC2_NON_COMPLIANT_9V;
			rc = smblib_masked_write(chg, HVDCP_PULSE_COUNT_MAX_REG,
					HVDCP_PULSE_COUNT_MAX_QC2_MASK,
					HVDCP_PULSE_COUNT_MAX_QC2_5V);
			if (rc < 0)
				smblib_err(chg, "Couldn't force max pulses to 5V rc=%d\n",
						rc);

		}

		rc = smblib_masked_write(chg, USBIN_AICL_OPTIONS_CFG_REG,
				SUSPEND_ON_COLLAPSE_USBIN_BIT,
				0);
		if (rc < 0)
			smblib_err(chg, "Couldn't turn off SUSPEND_ON_COLLAPSE_USBIN_BIT rc=%d\n",
					rc);

		smblib_rerun_apsd(chg);

		pr_info("qc2_unsupported charger detected\n");
		rc = smblib_force_vbus_voltage(chg, FORCE_5V_BIT);
		if (rc < 0)
			pr_err("Failed to force 5V\n");
		rc = smblib_usb_pd_adapter_allowance_override(chg, FORCE_5V);
		if (rc < 0)
			pr_err("Failed to set adapter allowance to 5V\n");
		rc = smblib_set_opt_switcher_freq(chg, chg->chg_freq.freq_5V);
		if (rc < 0)
			pr_err("Failed to set chg_freq.freq_5V\n");
		vote(chg->usb_icl_votable, QC2_UNSUPPORTED_VOTER, true,
				QC2_UNSUPPORTED_UA);
		chg->qc2_unsupported = true;
	}

	return IRQ_HANDLED;
}

#define USB_WEAK_INPUT_UA	1400000
#define ICL_CHANGE_DELAY_MS	1000
irqreturn_t icl_change_irq_handler(int irq, void *data)
{
	u8 stat;
	int rc, settled_ua, delay = ICL_CHANGE_DELAY_MS;
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	if (chg->mode == PARALLEL_MASTER) {
		/*
		 * Ignore if change in ICL is due to DIE temp mitigation.
		 * This is to prevent any further ICL split.
		 */
		if (chg->hw_die_temp_mitigation) {
			rc = smblib_read(chg, DIE_TEMP_STATUS_REG, &stat);
			if (rc < 0) {
				smblib_err(chg,
					"Couldn't read DIE_TEMP rc=%d\n", rc);
				return IRQ_HANDLED;
			}
			if (stat & (DIE_TEMP_UB_BIT | DIE_TEMP_LB_BIT)) {
				smblib_dbg(chg, PR_PARALLEL,
					"skip ICL change DIE_TEMP %x\n", stat);
				return IRQ_HANDLED;
			}
		}

		rc = smblib_read(chg, AICL_STATUS_REG, &stat);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read AICL_STATUS rc=%d\n",
					rc);
			return IRQ_HANDLED;
		}

		rc = smblib_get_charge_param(chg, &chg->param.icl_stat,
					&settled_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get ICL status rc=%d\n", rc);
			return IRQ_HANDLED;
		}

		/* If AICL settled then schedule work now */
		if (settled_ua == get_effective_result(chg->usb_icl_votable))
			delay = 0;

		cancel_delayed_work_sync(&chg->icl_change_work);
		schedule_delayed_work(&chg->icl_change_work,
						msecs_to_jiffies(delay));
	}

	return IRQ_HANDLED;
}

static int smblib_role_switch_failure(struct smb_charger *chg)
{
	int rc = 0;
	union power_supply_propval pval = {0, };

	if (!chg->use_extcon)
		return 0;

	rc = smblib_get_prop_usb_present(chg, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get usb presence status rc=%d\n",
				rc);
		return rc;
	}

	if (pval.intval) {
		smblib_dbg(chg, PR_MISC, " Role reversal failed, notifying device mode to usb driver.\n");
		smblib_notify_device_mode(chg, true);
	}

	return rc;
}

static int typec_partner_register(struct smb_charger *chg)
{
	int typec_mode, rc = 0;

	mutex_lock(&chg->typec_lock);

	if (!chg->typec_port || chg->pr_swap_in_progress)
		goto unlock;

	if (!chg->typec_partner) {
		if (chg->sink_src_mode == AUDIO_ACCESS_MODE)
			chg->typec_partner_desc.accessory =
					TYPEC_ACCESSORY_AUDIO;
		else
			chg->typec_partner_desc.accessory =
					TYPEC_ACCESSORY_NONE;

		chg->typec_partner = typec_register_partner(chg->typec_port,
				&chg->typec_partner_desc);
		if (IS_ERR(chg->typec_partner)) {
			rc = PTR_ERR(chg->typec_partner);
			pr_err("failed to register typec_partner rc=%d\n", rc);
			goto unlock;
		}
	}

	typec_mode = smblib_get_prop_typec_mode(chg);

	if (typec_mode >= POWER_SUPPLY_TYPEC_SOURCE_DEFAULT
			|| typec_mode == POWER_SUPPLY_TYPEC_NONE) {

		if (chg->typec_role_swap_failed) {
			rc = smblib_role_switch_failure(chg);
			if (rc < 0)
				smblib_err(chg, "Failed to role switch rc=%d\n",
					rc);
			chg->typec_role_swap_failed = false;
		}

		typec_set_data_role(chg->typec_port, TYPEC_DEVICE);
		typec_set_pwr_role(chg->typec_port, TYPEC_SINK);
	} else {
		typec_set_data_role(chg->typec_port, TYPEC_HOST);
		typec_set_pwr_role(chg->typec_port, TYPEC_SOURCE);
	}

unlock:
	mutex_unlock(&chg->typec_lock);
	return rc;
}

static void typec_partner_unregister(struct smb_charger *chg)
{
	mutex_lock(&chg->typec_lock);

	if (!chg->typec_port)
		goto unlock;

	if (chg->typec_partner && !chg->pr_swap_in_progress) {
		smblib_dbg(chg, PR_MISC, "Un-registering typeC partner\n");
		typec_unregister_partner(chg->typec_partner);
		chg->typec_partner = NULL;
	}

unlock:
	mutex_unlock(&chg->typec_lock);
}

#if (!defined CONFIG_FUEL_GAUGE_BQ27Z561_MUNCH) && (!defined CONFIG_DUAL_FUEL_GAUGE_BQ27Z561)
#define REDUCED_CURRENT		1000000
#define REDUCED_CURRENT_LOW		500000
#define FCC_LOW_FOR_ESR_THR		3000000
static int smblib_get_effective_fcc_val(struct smb_charger *chg)
{
	int effective_fcc_val = 0;

	effective_fcc_val = get_effective_result(chg->fcc_votable);
	return effective_fcc_val;
}

static int check_reduce_fcc_condition(struct smb_charger *chg)
{
	union power_supply_propval val = {0, };
	int rc, ibat = 0;
	int effective_fcc = 0;

	if (!chg->cp_psy) {
		chg->cp_psy = power_supply_get_by_name("bq2597x-standalone");
		if (!chg->cp_psy)
			pr_err("cp_psy not found\n");
			return 0;
	}

	rc = power_supply_get_property(chg->cp_psy,
				POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (rc < 0) {
		pr_err("Error in getting cp charge enable, rc=%d\n", rc);
		return 0;
	}
	chg->cp_charge_enabled = !!val.intval;

	rc = power_supply_get_property(chg->batt_psy, POWER_SUPPLY_PROP_STATUS,
						&val);
	if (rc < 0) {
		pr_err("Error in getting charging status, rc=%d\n", rc);
		return 0;
	}
	chg->charge_status = val.intval;

	rc = power_supply_get_property(chg->batt_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE, &val);
	if (rc < 0) {
		pr_err("Error in getting charge type, rc=%d\n", rc);
		return 0;
	}
	chg->charge_type = val.intval;

	rc = power_supply_get_property(chg->batt_psy, POWER_SUPPLY_PROP_HEALTH,
						&val);
	if (rc < 0) {
		pr_err("Error in getting charging status, rc=%d\n", rc);
		return 0;
	}
	chg->batt_health = val.intval;

	pr_info("cp_charge_enabled(%d), charge_status(%d), charge_type(%d), batt_health(%d)\n",
			chg->cp_charge_enabled, chg->charge_status, chg->charge_type, chg->batt_health);

	/* should add battery health later */
	if (!chg->cp_charge_enabled ||
		(chg->charge_status != POWER_SUPPLY_STATUS_CHARGING) ||
		(chg->charge_type != POWER_SUPPLY_CHARGE_TYPE_FAST) ||
		(chg->batt_health != POWER_SUPPLY_HEALTH_GOOD))
		return 0;

	rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (rc < 0)
		return 0;

	ibat = val.intval/1000;
	if (ibat > -450) {
		pr_info("Skip CHG ESR, Fails IBAT ibat(%d)\n", ibat);
		return 0;
	}

	effective_fcc = smblib_get_effective_fcc_val(chg);
	if (effective_fcc < 0)
		return 0;

	return effective_fcc;
}

/* In order to calculate battery esr */
static void reduce_fcc_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							reduce_fcc_work.work);
	int effective_fcc;
	int esr_work_time = ESR_WORK_TIME_97S;
	bool reduce_fcc = false;

	effective_fcc = check_reduce_fcc_condition(chg);
	if (effective_fcc > 0) {
		if (chg->esr_work_status == ESR_CHECK_FCC_NOLIMIT) {
			if (effective_fcc >= FCC_LOW_FOR_ESR_THR)
				effective_fcc -= REDUCED_CURRENT;
			else
				effective_fcc -= REDUCED_CURRENT_LOW;
			chg->esr_work_status = ESR_CHECK_FCC_LIMITED;
			reduce_fcc = true;
			esr_work_time = ESR_WORK_TIME_2S;
		} else {
			chg->esr_work_status = ESR_CHECK_FCC_NOLIMIT;
			reduce_fcc = false;
			esr_work_time = ESR_WORK_TIME_97S;
		}
	} else {
		chg->esr_work_status = ESR_CHECK_FCC_NOLIMIT;
		reduce_fcc = false;
		esr_work_time = ESR_WORK_TIME_97S;
		pr_info("calculate esr condition not satisfy.\n");
	}

	vote(chg->fcc_votable, ESR_WORK_VOTER, reduce_fcc, effective_fcc);
	schedule_delayed_work(&chg->reduce_fcc_work,
				msecs_to_jiffies(esr_work_time));
}
#endif

static void smblib_cc_un_compliant_charge_work(struct work_struct *work)
{
	union power_supply_propval val = {0, };
	int rc, usb_present = 0;

	struct smb_charger *chg = container_of(work, struct smb_charger,
					cc_un_compliant_charge_work.work);

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get usb present rc = %d\n", rc);
		return;
	}

	usb_present = val.intval;

	/*
	 * workaround for typec audio/charger connector charge issue
	 * when firstly insert earphone then insert charger, it will trigger usbin_plugin irq,
	 * but not trigger typec_attach_detach irq. so need force set charger type and ICL here.
	 */
	if (usb_present && chg->typec_mode == POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER) {
		chg->real_charger_type = POWER_SUPPLY_TYPE_USB_FLOAT;
		chg->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_FLOAT;
		if ((strcmp(get_effective_client(chg->usb_icl_votable), "OTG_VOTER") == 0) &&
					(get_effective_result(chg->usb_icl_votable) == 0))
			vote(chg->usb_icl_votable, OTG_VOTER, false, 0);

		if (get_client_vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER) != 500000)
			vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, 500000);
	}

	/*
	 * if CC pin of C to A cable is not connected to the receptacle
	 * or CC pin is bad or short to VBUS or C to C cable CC line is float,
	 * enable hvdcp detect since pm8150b CC protection voltage
	 * threshold is very high (22V), our wire charging maxium charging
	 * vbus is below 13.2V.
	 */
	if (usb_present
			&& (chg->typec_mode == POWER_SUPPLY_TYPEC_NONE ||
				chg->typec_mode == POWER_SUPPLY_TYPEC_NON_COMPLIANT ||
				chg->snk_debug_acc_detected == true)
			&& (chg->cc_un_compliant_detected == false)) {
		chg->cc_un_compliant_detected = true;
		smblib_hvdcp_detect_enable(chg, true);
		smblib_rerun_apsd_if_required(chg);
	}
}

static void smb_check_init_boot(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
					check_init_boot.work);
	first_boot_flag = true;
	if (chg->usb_psy)
		power_supply_changed(chg->usb_psy);
}
static void smblib_micro_usb_plugin(struct smb_charger *chg, bool vbus_rising)
{
	int rc = 0;

	if (!vbus_rising) {
		smblib_update_usb_type(chg);
		smblib_notify_device_mode(chg, false);
		smblib_uusb_removal(chg);
		typec_partner_unregister(chg);
	} else {
		rc = typec_partner_register(chg);
		if (rc < 0)
			smblib_err(chg, "Couldn't register partner rc =%d\n",
					rc);
	}
}

void smblib_usb_plugin_hard_reset_locked(struct smb_charger *chg)
{
	int rc;
	u8 stat;
	bool vbus_rising;
	struct smb_irq_data *data;
	struct storm_watch *wdata;
	union power_supply_propval val = {0,};

	rc = smblib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USB_INT_RT_STS rc=%d\n", rc);
		return;
	}

	vbus_rising = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);

	if (vbus_rising) {
		/* hold a wakeup source when charger is present */
		vote(chg->awake_votable, CHG_AWAKE_VOTER, true, 0);
		/* when vbus present, enable batt_temp irq wakeup */
		if (chg->irq_info[BAT_TEMP_IRQ].irq && !chg->batt_temp_irq_enabled) {
			enable_irq_wake(chg->irq_info[BAT_TEMP_IRQ].irq);
			chg->batt_temp_irq_enabled = true;
		}
		if (chg->wireless_bq && !chg->power_good_en)
			smblib_ncp3902_enable(chg, true);
		/* Remove FCC_STEPPER 1.5A init vote to allow FCC ramp up */
		if (chg->fcc_stepper_enable)
			vote(chg->fcc_votable, FCC_STEPPER_VOTER, false, 0);
		if (chg->six_pin_step_charge_enable)
			smblib_get_start_vbat_before_step_charge(chg);
	} else {
		if (chg->wa_flags & BOOST_BACK_WA) {
			data = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data;
			if (data) {
				wdata = &data->storm_data;
				update_storm_count(wdata,
						WEAK_CHG_STORM_COUNT);
				vote(chg->usb_icl_votable, BOOST_BACK_VOTER,
						false, 0);
				vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
						false, 0);
			}
		}

		/* when vbus absent, disable batt_temp irq wakeup */
		if (chg->irq_info[BAT_TEMP_IRQ].irq && chg->batt_temp_irq_enabled) {
			disable_irq_wake(chg->irq_info[BAT_TEMP_IRQ].irq);
			chg->batt_temp_irq_enabled = false;
		}

		cancel_delayed_work_sync(&chg->charger_type_recheck);
		chg->hvdcp_recheck_status = false;
		chg->recheck_charger = false;
		chg->precheck_charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
		if (chg->cc_un_compliant_detected) {
			smblib_hvdcp_detect_enable(chg, false);
			chg->cc_un_compliant_detected = false;
		}
		/* Force 1500mA FCC on USB removal if fcc stepper is enabled */
		if (chg->fcc_stepper_enable)
			vote(chg->fcc_votable, FCC_STEPPER_VOTER,
							true, 1500000);

		/* Check whether charge pump trigger reset after plug out */
		if (!chg->cp_psy)
			chg->cp_psy = power_supply_get_by_name("bq2597x-master");
		if (!chg->cp_sec_psy)
			chg->cp_sec_psy = power_supply_get_by_name("bq2597x-slave");
		if (chg->cp_psy)
			power_supply_get_property(chg->cp_psy,
					POWER_SUPPLY_PROP_TI_RESET_CHECK, &val);
		if (chg->cp_sec_psy)
			power_supply_get_property(chg->cp_sec_psy,
					POWER_SUPPLY_PROP_TI_RESET_CHECK, &val);

		/* clear chg_awake wakeup source when charger is absent */
		vote(chg->awake_votable, CHG_AWAKE_VOTER, false, 0);
	}
	power_supply_changed(chg->usb_psy);
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: usbin-plugin %s\n",
					vbus_rising ? "attached" : "detached");
}

#define PL_DELAY_MS	30000
void smblib_usb_plugin_locked(struct smb_charger *chg)
{
	int rc;
	u8 stat;
	struct smb_irq_data *data;
	struct storm_watch *wdata;
	union power_supply_propval val = {0,};

	rc = smblib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USB_INT_RT_STS rc=%d\n", rc);
		return;
	}

	chg->vbus_rising = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);
	smblib_set_opt_switcher_freq(chg, chg->vbus_rising ? chg->chg_freq.freq_5V :
						chg->chg_freq.freq_removal);
	schedule_work(&chg->plugin_check_time_work);
	if (chg->vbus_rising) {
		/* when vbus present, enable batt_temp irq wakeup */
		if (chg->irq_info[BAT_TEMP_IRQ].irq && !chg->batt_temp_irq_enabled) {
			enable_irq_wake(chg->irq_info[BAT_TEMP_IRQ].irq);
			chg->batt_temp_irq_enabled = true;
		}
		if (chg->wireless_bq) {
			/* if vbus present, but power good is low, set it high */
			if (!chg->power_good_en) {
				smblib_ncp3902_enable(chg, true);
			}
		}
		if (smblib_get_prop_dfp_mode(chg) != POWER_SUPPLY_TYPEC_NONE
				&& smblib_get_prop_dfp_mode(chg)
					!= POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER) {
			chg->fake_usb_insertion = true;
			return;
		}

		/* hold a wakeup source when charger is present */
		vote(chg->awake_votable, CHG_AWAKE_VOTER, true, 0);
		cancel_delayed_work_sync(&chg->pr_swap_detach_work);
		vote(chg->awake_votable, DETACH_DETECT_VOTER, false, 0);
		rc = smblib_request_dpdm(chg, true);
		if (rc < 0)
			smblib_err(chg, "Couldn't to enable DPDM rc=%d\n", rc);

#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
		smblib_check_batt_authentic(chg);
#endif

		/* Enable SW Thermal regulation */
		rc = smblib_set_sw_thermal_regulation(chg, true);
		if (rc < 0)
			smblib_err(chg, "Couldn't start SW thermal regulation WA, rc=%d\n",
				rc);
		/* Enable SW conn therm Regulation */
		 if (chg->support_conn_therm){
			rc = smblib_set_sw_conn_therm_regulation(chg, true);
			if (rc < 0)
				smblib_err(chg, "Couldn't start SW conn therm rc=%d\n", rc);
		 }
		/* Remove FCC_STEPPER 1.5A init vote to allow FCC ramp up */
		if (chg->fcc_stepper_enable)
			vote(chg->fcc_votable, FCC_STEPPER_VOTER, false, 0);

		if (chg->six_pin_step_charge_enable)
			smblib_get_start_vbat_before_step_charge(chg);

		/* Schedule work to enable parallel charger */
		vote(chg->awake_votable, PL_DELAY_VOTER, true, 0);
		if (!first_boot_flag)
			schedule_delayed_work(&chg->check_init_boot, msecs_to_jiffies(45000));
		schedule_delayed_work(&chg->pl_enable_work,
					msecs_to_jiffies(PL_DELAY_MS));
		schedule_delayed_work(&chg->charger_type_recheck,
					msecs_to_jiffies(CHARGER_RECHECK_DELAY_MS));
		schedule_delayed_work(&chg->cc_un_compliant_charge_work,
					msecs_to_jiffies(CC_UN_COMPLIANT_START_DELAY_MS));
		if (chg->ext_fg)
			schedule_delayed_work(&chg->after_ffc_chg_dis_work,
					msecs_to_jiffies(FFC_DISABLE_CHG_RECHECK_DELAY_10S));
	} else {
		/* when vbus absent, disable batt_temp irq wakeup */
		if (chg->irq_info[BAT_TEMP_IRQ].irq && chg->batt_temp_irq_enabled) {
			disable_irq_wake(chg->irq_info[BAT_TEMP_IRQ].irq);
			chg->batt_temp_irq_enabled = false;
		}
		if (chg->fake_usb_insertion) {
			chg->fake_usb_insertion = false;
			return;
		}
		chg->hvdcp_recheck_status = false;
		vote(chg->usb_icl_votable, HVDCP2_ICL_VOTER, false, 0);
		vote(chg->fcc_votable, HVDCP2_FCC_VOTER, false, 0);
		cancel_delayed_work_sync(&chg->charger_type_recheck);
		flush_delayed_work(&chg->after_ffc_chg_dis_work);
		cancel_delayed_work(&chg->after_ffc_chg_en_work);
		if (chg->cc_un_compliant_detected) {
			smblib_hvdcp_detect_enable(chg, false);
			chg->cc_un_compliant_detected = false;
		}
#if (!defined CONFIG_FUEL_GAUGE_BQ27Z561_MUNCH) && (!defined CONFIG_DUAL_FUEL_GAUGE_BQ27Z561)
		cancel_delayed_work_sync(&chg->reduce_fcc_work);
		vote(chg->fcc_votable, ESR_WORK_VOTER, false, 0);
#endif
		/* Disable SW Thermal Regulation */
		rc = smblib_set_sw_thermal_regulation(chg, false);
		if (rc < 0)
			smblib_err(chg, "Couldn't stop SW thermal regulation WA, rc=%d\n",
				rc);

		/* Disable SW conn therm Regulation */
		if (chg->support_conn_therm){
			rc = smblib_set_sw_conn_therm_regulation(chg, false);
			if (rc < 0)
				smblib_err(chg, "Couldn't start SW conn therm rc=%d\n", rc);
		}
		if (chg->wa_flags & BOOST_BACK_WA) {
			data = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data;
			if (data) {
				wdata = &data->storm_data;
				update_storm_count(wdata,
						WEAK_CHG_STORM_COUNT);
				vote(chg->usb_icl_votable, BOOST_BACK_VOTER,
						false, 0);
				vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
						false, 0);
			}
		}

		/* Force 1500mA FCC on removal if fcc stepper is enabled */
		if (chg->fcc_stepper_enable)
			vote(chg->fcc_votable, FCC_STEPPER_VOTER,
							true, 1500000);

		if (chg->wa_flags & WEAK_ADAPTER_WA) {
			chg->aicl_5v_threshold_mv =
					chg->default_aicl_5v_threshold_mv;
			chg->aicl_cont_threshold_mv =
					chg->default_aicl_cont_threshold_mv;

			smblib_set_charge_param(chg,
					&chg->param.aicl_5v_threshold,
					chg->aicl_5v_threshold_mv);
			smblib_set_charge_param(chg,
					&chg->param.aicl_cont_threshold,
					chg->aicl_cont_threshold_mv);
			chg->aicl_max_reached = false;

			if (chg->chg_param.smb_version == PMI632_SUBTYPE)
				schgm_flash_torch_priority(chg,
						TORCH_BUCK_MODE);

			data = chg->irq_info[USBIN_UV_IRQ].irq_data;
			if (data) {
				wdata = &data->storm_data;
				reset_storm_count(wdata);
			}
			vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER,
					false, 0);
		}

		rc = smblib_request_dpdm(chg, false);
		if (rc < 0)
			smblib_err(chg, "Couldn't disable DPDM rc=%d\n", rc);
		if (chg->qc2_unsupported) {
			chg->qc2_unsupported = false;
			rc = smblib_usb_pd_adapter_allowance_override(chg, FORCE_NULL);
		}
		chg->recheck_charger = false;
		chg->precheck_charger_type = POWER_SUPPLY_TYPE_UNKNOWN;

		/* Check whether charge pump trigger reset after plug out */
		if (!chg->cp_psy)
			chg->cp_psy = power_supply_get_by_name("bq2597x-master");
		if (!chg->cp_sec_psy)
			chg->cp_sec_psy = power_supply_get_by_name("bq2597x-slave");
		if (chg->cp_psy)
			power_supply_get_property(chg->cp_psy,
					POWER_SUPPLY_PROP_TI_RESET_CHECK, &val);
		if (chg->cp_sec_psy)
			power_supply_get_property(chg->cp_sec_psy,
					POWER_SUPPLY_PROP_TI_RESET_CHECK, &val);

		/* clear chg_awake wakeup source when charger is absent */
		vote(chg->awake_votable, CHG_AWAKE_VOTER, false, 0);
	}

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		smblib_micro_usb_plugin(chg, chg->vbus_rising);

	vote(chg->temp_change_irq_disable_votable, DEFAULT_VOTER,
						!chg->vbus_rising, 0);

	power_supply_changed(chg->usb_psy);
	smblib_dbg(chg, PR_OEM, "IRQ: usbin-plugin %s\n",
					chg->vbus_rising ? "attached" : "detached");
}

irqreturn_t usb_plugin_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	if (chg->pd_hard_reset)
		smblib_usb_plugin_hard_reset_locked(chg);
	else
		smblib_usb_plugin_locked(chg);

	return IRQ_HANDLED;
}

static void smblib_handle_slow_plugin_timeout(struct smb_charger *chg,
					      bool rising)
{
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: slow-plugin-timeout %s\n",
		   rising ? "rising" : "falling");
}

static void smblib_handle_sdp_enumeration_done(struct smb_charger *chg,
					       bool rising)
{
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: sdp-enumeration-done %s\n",
		   rising ? "rising" : "falling");
}

static bool qc3p5_vbus_timeout_check(struct smb_charger *chg,
		int timeout_ms, int vbus_lo_bound,
		int vbus_hi_bound, int *vbus_uv)
{
		union power_supply_propval pval;
		ktime_t start_kt, delta_kt;
		int rc = 0;

		rc = power_supply_get_property(chg->usb_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get VBUS voltage rc=%d\n", rc);
			return false;
		}

		start_kt = ktime_get_boottime();
		delta_kt = ktime_sub(ktime_get_boottime(), start_kt);
		while (((pval.intval <= vbus_lo_bound) || (pval.intval >= vbus_hi_bound))
				&& (ktime_to_ms(delta_kt) < timeout_ms)) {
			rc = power_supply_get_property(chg->usb_psy,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
			if (rc < 0) {
				smblib_err(chg, "Couldn't get VBUS voltage rc=%d\n", rc);
				return false;
			}
			delta_kt = ktime_sub(ktime_get_boottime(), start_kt);
		}

		if ((pval.intval <= vbus_lo_bound)
				|| (pval.intval >= vbus_hi_bound)
				|| (ktime_to_ms(delta_kt) > timeout_ms)) {
			smblib_err(chg, "QC3.5: VBUS failed to settle\n");
			return false;
		}

		*vbus_uv = (int)pval.intval;
		smblib_err(chg, "QC3.5: VBUS successfully settled\n");

		return true;
}

#define	QC3P5_T_TA_DETECTION_TIMEOUT_PMIC_MS	200
#define	QC3P5_T_TA_CAP_TIMEOUT_PMIC_MS			250
#define	VBUS_5P5_V_UV							5500000
#define	VBUS_6P4_V_UV							6400000
#define	VBUS_6P65_V_UV							6650000
#define	VBUS_7P35_V_UV							7350000
#define	VBUS_7P6_V_UV							7600000
#define	VBUS_8P4_V_UV							8400000
/*Use 8.55V and 9.8V to account for +10% and -5% spec on 9V for QC2/3 */
#define	VBUS_8P55_V_UV							8550000
#define	VBUS_9P8_V_UV							9800000

static int qc3p5_authenticate(struct smb_charger *chg)
{
	int i, vbus_uv, rc = 0;

	chg->qc3p5_authenticated = false;
	chg->qc3p5_authentication_started = true;
	chg->qc3p5_power_limit_w = 18;//Default to lowest power limit of 18W

	/* Set ICL to 500mA during QC3.5 Authentication */
	vote(chg->usb_icl_votable, QC3P5_VOTER, true, USBIN_500MA);

	/*
	 *	 * Fall back to QC3.0 Mode if VBUS doesn't settle between
	 *		 * 5.5V and 6.4V in 200ms
	 *			 */
	if (!qc3p5_vbus_timeout_check(chg, QC3P5_T_TA_DETECTION_TIMEOUT_PMIC_MS,
				VBUS_5P5_V_UV, VBUS_6P4_V_UV, &vbus_uv)) {
		smblib_err(chg, "VBUS doesn't reach 6V vbus=%d\n", vbus_uv);
		return -ENODEV;
	}

	smblib_err(chg, "QC3P5 AUTH: After QC3.0 Auth VBUS = %d\n", vbus_uv);

	/* Issue +-+-+- to request SRC CAP */
	for (i = 0; i < 3; i++) {
		rc = smblib_dp_pulse(chg);
		if (rc < 0) {
			smblib_err(chg, "Couldn't issue D+ pulse rc=%d\n", rc);
			return rc;
		}
		usleep_range(5000, 5010);

		rc = smblib_dm_pulse(chg);
		if (rc < 0) {
			smblib_err(chg, "Couldn't issue D- pulse rc=%d\n", rc);
			return rc;
		}
		usleep_range(5000, 5010);
	}

	/* Return to QC3.0 Mode if VBUS doesn't reach 7V in 200ms 50ms buffer */
	if (!qc3p5_vbus_timeout_check(chg, QC3P5_T_TA_CAP_TIMEOUT_PMIC_MS,
				VBUS_6P65_V_UV, VBUS_9P8_V_UV, &vbus_uv)) {
		smblib_err(chg, "VBUS doesn't reach 7V vbus=%d\n", vbus_uv);
		return -ENODEV;
	}
	smblib_err(chg, "QC3P5 AUTH: SRC_CAP receieved\n");

	/* Issue ++-- to confirm transition to QC3.5 */
	rc = smblib_dp_pulse(chg);
	if (rc < 0) {
		smblib_err(chg, "Couldn't issue D+ pulse rc=%d\n", rc);
		return rc;
	}
	usleep_range(5000, 5010);

	rc = smblib_dp_pulse(chg);
	if (rc < 0) {
		smblib_err(chg, "Couldn't issue D+ pulse rc=%d\n", rc);
		return rc;
	}
	usleep_range(5000, 5010);

	rc = smblib_dm_pulse(chg);
	if (rc < 0) {
		smblib_err(chg, "Couldn't issue D- pulse rc=%d\n", rc);
		return rc;
	}
	usleep_range(5000, 5010);

	rc = smblib_dm_pulse(chg);
	if (rc < 0) {
		smblib_err(chg, "Couldn't issue D- pulse rc=%d\n", rc);
		return rc;
	}

	/* SRC CAP 7V for 18W */
	if ((vbus_uv >= VBUS_6P65_V_UV) && (vbus_uv <= VBUS_7P35_V_UV)) {
		chg->qc3p5_power_limit_w = 18;
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, QC3P5_CHARGER_ICL);
	/* SRC CAP 8V for 27W */
	} else if ((vbus_uv >= VBUS_7P6_V_UV) && (vbus_uv <= VBUS_8P4_V_UV)) {
		chg->qc3p5_power_limit_w = 27;
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, QC3P5_CHARGER_ICL);
	/* SRC CAP 9V for 40W */
	} else if ((vbus_uv >= VBUS_8P55_V_UV) && (vbus_uv <= VBUS_9P8_V_UV)) {
		chg->qc3p5_power_limit_w = 40;
		// QC3.5 40W adapter's icl limited to 4A
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, HVDCP3P5_40W_CURRENT_UA);
	} else {
		smblib_err(chg, "not supported SRC CAP, vbus=%d\n", vbus_uv);
		return -ENODEV;
	}

	chg->qc3p5_authenticated = true;
	vote(chg->fcc_votable, FCC_MAX_QC3P5_VOTER, true, HVDCP3P5_40W_CURRENT_UA);

	if (chg->support_ffc && !smblib_get_fastcharge_mode(chg))
	smblib_set_fastcharge_mode(chg, true);

	smblib_err(chg, "QC3P5 AUTH: QC3.5 Authenticated\n");
	smblib_err(chg, "QC3P5 AUTH: Power Limit = %d\n",
			chg->qc3p5_power_limit_w);

	return rc;
}



#define APSD_EXTENDED_TIMEOUT_MS	400

static int smblib_hvdcp3_raise_fsw(struct smb_charger *chg, int pulse_count)
{
	if (pulse_count < QC3_PULSES_FOR_6V)
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_5V);
	else if (pulse_count < QC3_PULSES_FOR_9V)
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_6V_8V);
	else if (pulse_count < QC3_PULSES_FOR_12V)
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_9V);
	else
		smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_12V);
  return 0;
}

static void smblib_raise_qc3_vbus_work(struct work_struct *work)
{
	union power_supply_propval val = {0, };
	int i, usb_present = 0, vbus_now = 0;
	int vol_qc_ab_thr = 0;
	int rc;
	struct smb_charger *chg = container_of(work, struct smb_charger,
						raise_qc3_vbus_work.work);

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get usb present rc = %d\n", rc);
		return;
	}

	usb_present = val.intval;
	if (usb_present) {
		chg->raise_vbus_to_detect = true;
		if (chg->no_raise_vbus_status == true) {
			smblib_hvdcp3_raise_fsw(chg, MAX_PULSE);
			chg->is_qc_class_a = true;
			vote(chg->fcc_votable,
					CLASSA_QC_FCC_VOTER, true, QC_CLASS_A_CURRENT_UA);
			goto skip;
		}

		for (i = 0; i < MAX_PULSE; i++) {
			msleep(40);
			smblib_hvdcp3_raise_fsw(chg, i);
			rc = smblib_dp_pulse(chg);
		}
		chg->after_raise_vbus_time = ktime_get();

		msleep(200);
		rc = smblib_get_prop_usb_present(chg, &val);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get usb present rc = %d\n", rc);
			return;
		}

		usb_present = val.intval;
		pr_info("usb_present is %d\n", usb_present);
		if (!usb_present) {
			chg->raise_vbus_to_detect = false;
			rc = smblib_force_vbus_voltage(chg, FORCE_5V_BIT);
			if (rc < 0)
				pr_err("Failed to force 5V\n");
			return;
		}

		rc = smblib_get_usb_in_voltage_now(chg, &val);
		if (rc < 0)
			pr_err("Couldn't get usb voltage rc=%d\n", rc);
		vbus_now = val.intval;
		pr_info("vbus_now is %d\n", vbus_now);
		if (chg->snk_debug_acc_detected && usb_present)
			vol_qc_ab_thr = VOL_THR_FOR_QC_CLASS_AB
							+ COMP_FOR_LOW_RESISTANCE_CABLE;
		else
			vol_qc_ab_thr = VOL_THR_FOR_QC_CLASS_AB;
		if (vbus_now <= vol_qc_ab_thr) {
			pr_info("qc_class_a charger is detected\n");
			chg->is_qc_class_a = true;
			vote(chg->fcc_votable,
					CLASSA_QC_FCC_VOTER, true, QC_CLASS_A_CURRENT_UA);
		} else {
			pr_info("qc_class_b charger is detected\n");
			chg->is_qc_class_b = true;
			if (chg->usb_psy)
				power_supply_changed(chg->usb_psy);
		}
skip:
		rc = smblib_force_vbus_voltage(chg, FORCE_5V_BIT);
		if (rc < 0)
			pr_err("Failed to force 5V\n");
		if (chg->is_qc_class_a)
			vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					HVDCP_CLASS_A_MAX_UA);
		else {
			vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
				HVDCP_CURRENT_UA);

			if (!is_client_vote_enabled(chg->fcc_votable,
						PD_VERIFED_VOTER))
				vote(chg->fcc_votable,
						PD_VERIFED_VOTER, false, 0);

			if (chg->six_pin_step_charge_enable) {
				/* start six pin step charge monitor work */
				schedule_delayed_work(&chg->six_pin_batt_step_chg_work,
						msecs_to_jiffies(STEP_CHG_DELAYED_START_MS));
			}
		}
		/* select charge pump as second charger */
		if (chg->sec_cp_present){
			rc = smblib_select_sec_charger(chg, POWER_SUPPLY_CHARGER_SEC_CP,
							POWER_SUPPLY_CP_HVDCP3, false);
			if (rc < 0)
					dev_err(chg->dev,
						"Couldn't enable secondary chargers  rc=%d\n", rc);
		}

		smblib_therm_charging(chg);
		chg->raise_vbus_to_detect = false;
	}
}

struct quick_charge adapter_cap[11] = {
	{ POWER_SUPPLY_TYPE_USB,        QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_DCP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_CDP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_ACA,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_FLOAT,  QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_PD,       QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP,    QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3,  QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3P5,  QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_WIRELESS,     QUICK_CHARGE_FAST },
	{0, 0},
};

#define ADAPTER_PWR_NONE              0x0
#define ADAPTER_XIAOMI_QC3_PWR_20W    0x9
#define ADAPTER_XIAOMI_PD_PWR_20W     0xa
#define ADAPTER_XIAOMI_CAR_PWR_20W    0xb
#define ADAPTER_XIAOMI_PD_PWR_30W     0xc
#define ADAPTER_VOICE_BOX_PWR_30W     0xd
#define ADAPTER_XIAOMI_PD_PWR_50W     0xe
#define ADAPTER_XIAOMI_PD_PWR_60W     0xf
int smblib_get_quick_charge_type(struct smb_charger *chg)
{
	int i = 0, rc;
	int tx_adapter = 0, wls_online = 0;
	union power_supply_propval pval = {0, };

	if (!chg) {
		dev_err(chg->dev, "get quick charge type faied\n");
		return -EINVAL;
	}

	rc = smblib_get_prop_batt_health(chg, &pval);
	if (rc < 0)
		smblib_err(chg, "Couldn't get batt health rc=%d\n", rc);

	if ((pval.intval == POWER_SUPPLY_HEALTH_COLD) || (pval.intval == POWER_SUPPLY_HEALTH_OVERHEAT)) {
		pr_info("battery temp is under 0 or above 58\n");
		return 0;
	}

	if ((chg->real_charger_type == POWER_SUPPLY_TYPE_USB_PD) && chg->pd_verifed) {
		return QUICK_CHARGE_TURBE;
	}

	if (chg->is_qc_class_b || chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP_3P5)
		return QUICK_CHARGE_FLASH;

	if ((chg->real_charger_type == POWER_SUPPLY_TYPE_USB_DCP) &&
			(chg->hvdcp_recheck_status || chg->fake_plug_out == true))
		return QUICK_CHARGE_FAST;

	if (chg->wls_psy && chg->dc_psy) {
		rc = power_supply_get_property(chg->wls_psy, POWER_SUPPLY_PROP_TX_ADAPTER, &pval);
		tx_adapter = pval.intval;

		rc = power_supply_get_property(chg->dc_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
		wls_online = pval.intval;

		if (wls_online) {
			if (tx_adapter >= ADAPTER_XIAOMI_QC3_PWR_20W)
				return QUICK_CHARGE_TURBE;
			else if (tx_adapter > ADAPTER_PWR_NONE)
				return QUICK_CHARGE_NORMAL;
			else
				return 0;
		}
	}

	while (adapter_cap[i].adap_type != 0) {
		if (chg->real_charger_type == adapter_cap[i].adap_type) {
			return adapter_cap[i].adap_cap;
		}
		i++;
	}

	return 0;
}

#define WLS_POWER_20W	20
#define WLS_POWER_30W	30
#define WLS_POWER_50W	50
int smblib_get_adapter_power_max(struct smb_charger *chg)
{
	int rc;
	int wireless_power_good_en = 0;
	int tx_adapter = 0;
	int usb_present = 0;
	int apdo_max = 0;
	union power_supply_propval pval = {0, };

	if (chg->wls_psy) {
		rc = power_supply_get_property(chg->wls_psy,
				POWER_SUPPLY_PROP_WIRELESS_POWER_GOOD_EN, &pval);
		if (rc < 0) {
			dev_err(chg->dev, "get wireless_power_good_en failed, rc=%d\n", rc);
			return 0;
		}
		wireless_power_good_en = pval.intval;
		pr_info("wireless_power_good_en:%d\n", wireless_power_good_en);
	}

	if (wireless_power_good_en) {
		rc = power_supply_get_property(chg->wls_psy, POWER_SUPPLY_PROP_TX_ADAPTER, &pval);
		if (rc < 0) {
			dev_err(chg->dev, "get tx_adapter failed, rc=%d\n", rc);
			return 0;
		}
		tx_adapter = pval.intval;
		pr_info("tx_adapter:%d\n", tx_adapter);

		if (tx_adapter >= ADAPTER_XIAOMI_PD_PWR_30W)
			return WLS_POWER_30W;
		else if (tx_adapter >= ADAPTER_XIAOMI_QC3_PWR_20W)
			return WLS_POWER_20W;
		else
			return 0;

	} else {
		if (chg->usb_psy) {
			rc = power_supply_get_property(chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			if (rc < 0) {
				dev_err(chg->dev, "get usb online status failed, rc=%d\n", rc);
				return 0;
			}
		}
		usb_present = pval.intval;

		if (usb_present) {
			rc = power_supply_get_property(chg->usb_psy,
						POWER_SUPPLY_PROP_APDO_MAX, &pval);
			apdo_max = pval.intval;
			/*pr_info("apdo_max:%d\n", apdo_max);*/
			return apdo_max;
		}
	}

	return 0;
}

/* triggers when HVDCP 3.0 authentication has finished */
static void smblib_handle_hvdcp_3p0_auth_done(struct smb_charger *chg,
					      bool rising)
{
	const struct apsd_result *apsd_result;
	int rc;

	if (!rising || chg->qc2_unsupported)
		return;

	if (chg->qc3p5_supported) {
		/* Run QC3P5 Authentication */
		if (!chg->qc3p5_authentication_started) {
			rc = qc3p5_authenticate(chg);
			if (rc < 0) {
				chg->qc3p5_authenticated = false;
				dev_err(chg->dev, "QC3.5 Authentication Failed, rc=%d\n", rc);
			}
			chg->qc3p5_auth_complete = true;
			/* Release 500mA QC3.5 Authentication vote */
			vote(chg->usb_icl_votable, QC3P5_VOTER, false, 0);
		}
		smblib_update_usb_type(chg);
	}

	if (chg->mode == PARALLEL_MASTER)
		vote(chg->pl_enable_votable_indirect, USBIN_V_VOTER, true, 0);

	/* the APSD done handler will set the USB supply type */
	apsd_result = smblib_get_apsd_result(chg);

	/* for QC3, switch to CP if present */
	if ((apsd_result->bit & QC_3P0_BIT) && chg->sec_cp_present) {
		if (!chg->qc_class_ab) {
			rc = smblib_select_sec_charger(chg, POWER_SUPPLY_CHARGER_SEC_CP,
						POWER_SUPPLY_CP_HVDCP3, false);
			if (rc < 0)
				dev_err(chg->dev,
					"Couldn't enable secondary chargers  rc=%d\n", rc);
		} else {
			if (!chg->detect_low_power_qc3_charger &&
					(!chg->qc3p5_supported || !chg->qc3p5_authenticated)) {
				vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
						HVDCP_START_CURRENT_UA);
				schedule_delayed_work(&chg->raise_qc3_vbus_work, 0);
				chg->detect_low_power_qc3_charger = true;
			}
		}
	} else if ((apsd_result->bit & QC_3P0_BIT) && chg->use_bq_pump) {
		if (!chg->detect_low_power_qc3_charger) {
			vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					HVDCP_START_CURRENT_UA);
			schedule_delayed_work(&chg->raise_qc3_vbus_work, 0);
			chg->detect_low_power_qc3_charger = true;
		}
	} else if (apsd_result->bit & QC_2P0_BIT) {
			if (get_client_vote(chg->usb_icl_votable, USB_PSY_VOTER) == 500000)
				vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
			pr_info("force 9V for QC2 charger\n");
			rc = smblib_force_vbus_voltage(chg, FORCE_9V_BIT);
			if (rc < 0)
				pr_err("Failed to force 9V\n");
			vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					HVDCP2_CURRENT_UA);
	}
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: hvdcp-3p0-auth-done rising; %s detected\n",
		   apsd_result->name);
}

static void smblib_handle_hvdcp_check_timeout(struct smb_charger *chg,
					      bool rising, bool qc_charger)
{
	u32 hvdcp_ua = 0;

	if (rising) {

		if (qc_charger) {
			hvdcp_ua = (chg->real_charger_type ==
					POWER_SUPPLY_TYPE_USB_HVDCP) ?
					chg->chg_param.hvdcp2_max_icl_ua :
					HVDCP_CURRENT_UA;

			/* enable HDC and ICL irq for QC2/3 charger */
			vote(chg->limited_irq_disable_votable,
					CHARGER_TYPE_VOTER, false, 0);
			vote(chg->hdc_irq_disable_votable,
					CHARGER_TYPE_VOTER, false, 0);
			if (!chg->raise_vbus_to_detect
					&& chg->real_charger_type != POWER_SUPPLY_TYPE_USB_HVDCP_3P5) {
				if (chg->is_qc_class_a)
					vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
						HVDCP_CLASS_A_MAX_UA);
				else if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP)
					vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
						HVDCP2_CURRENT_UA);
				else
					vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
						HVDCP_CURRENT_UA);
			}
		} else {
			/* A plain DCP, enforce DCP ICL if specified */
			vote(chg->usb_icl_votable, DCP_VOTER,
				chg->dcp_icl_ua != -EINVAL, chg->dcp_icl_ua);
		}
	}

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s %s\n", __func__,
		   rising ? "rising" : "falling");
}

/* triggers when HVDCP is detected */
static void smblib_handle_hvdcp_detect_done(struct smb_charger *chg,
					    bool rising)
{
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: hvdcp-detect-done %s\n",
		   rising ? "rising" : "falling");
}

static bool smblib_handle_wireless_by_usbin_charge(struct smb_charger *chg)
{
	union power_supply_propval pval = {0, };
	union power_supply_propval val = {0, };
	int typec_mode;
	int dc_power_on = 0;

	if (!chg->wireless_bq)
		return false;

	smblib_get_prop_usb_present(chg, &val);
	smblib_get_prop_dc_present(chg, &pval);
	dc_power_on = pval.intval;

	typec_mode = smblib_get_prop_typec_mode(chg);
	if ((typec_mode >= POWER_SUPPLY_TYPEC_NONE)
			&& (val.intval == 1) && dc_power_on){
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
		return true;
	}
	return false;
}

static void update_sw_icl_max(struct smb_charger *chg, int pst)
{
	union power_supply_propval val = {0, };

	if (chg->power_good_en) {
		smblib_dbg(chg, PR_MISC, "power good on, no update icl\n");
		return;
	}

	/* while PD is active it should have complete ICL control */
	if (chg->pd_active)
		return;

	if (chg->typec_mode == POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER) {
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, 500000);
		return;
	}

	/*
	 * HVDCP 2/3/3.5, handled separately
	 */
	if (pst == POWER_SUPPLY_TYPE_USB_HVDCP
			|| pst == POWER_SUPPLY_TYPE_USB_HVDCP_3
			|| pst == POWER_SUPPLY_TYPE_USB_HVDCP_3P5)
		return;

	/* rp-std or legacy, USB BC 1.2 */
	switch (pst) {
	case POWER_SUPPLY_TYPE_USB:
		smblib_get_prop_usb_present(chg, &val);
		/*
		 * USB_PSY will vote to increase the current to 500/900mA once
		 * enumeration is done.
		 */
		if (!is_client_vote_enabled(chg->usb_icl_votable,
						USB_PSY_VOTER)) {
			vote(chg->usb_icl_votable, USB_PSY_VOTER, true, SDP_CURRENT_UA);
			vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
		} else if ((chg->typec_mode == POWER_SUPPLY_TYPEC_NONE)
				&& (val.intval == 1))
			vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, SDP_CURRENT_UA);
		else
			vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					CDP_CURRENT_UA);
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
                    DCP_CURRENT_UA);
		break;
	case POWER_SUPPLY_TYPE_USB_FLOAT:
		/*
		 * limit ICL to 100mA, the USB driver will enumerate to check
		 * if this is a SDP and appropriately set the current
		 */

		if (smblib_handle_wireless_by_usbin_charge(chg))
			break;

		if (!chg->recheck_charger)
			vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					SDP_100_MA);
		else
			vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					FLOAT_CHARGER_UA);

		break;
	case POWER_SUPPLY_TYPE_UNKNOWN:
	default:
		if (smblib_handle_wireless_by_usbin_charge(chg))
			break;

		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					SDP_100_MA);
		break;
	}
}

static void determine_thermal_current(struct smb_charger *chg)
{
	if (chg->system_temp_level > 0
			&& chg->system_temp_level < (chg->thermal_levels - 1)) {
		/*
		 * consider thermal limit only when it is active and not at
		 * the highest level
		 */
		smblib_therm_charging(chg);
	}
}

static void smblib_handle_apsd_done(struct smb_charger *chg, bool rising)
{
	const struct apsd_result *apsd_result;

	if (!rising)
		return;

	apsd_result = smblib_update_usb_type(chg);

	update_sw_icl_max(chg, apsd_result->pst);

	switch (apsd_result->bit) {
	case SDP_CHARGER_BIT:
	case CDP_CHARGER_BIT:
	case FLOAT_CHARGER_BIT:
		if (chg->use_extcon)
			smblib_notify_device_mode(chg, true);
		break;
	case OCP_CHARGER_BIT:
	case DCP_CHARGER_BIT:
		break;
	default:
		break;
	}

	determine_thermal_current(chg);
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: apsd-done rising; %s detected\n",
		   apsd_result->name);
}

irqreturn_t usb_source_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc = 0;
	u8 stat;
	u8 reg_value = 0;
	if (chg->fake_usb_insertion)
		return IRQ_HANDLED;

	/* PD session is ongoing, ignore BC1.2 and QC detection */
	if (chg->pd_active)
		return IRQ_HANDLED;

	rc = smblib_read(chg, APSD_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read APSD_STATUS rc=%d\n", rc);
		return IRQ_HANDLED;
	}
	smblib_dbg(chg, PR_INTERRUPT, "APSD_STATUS = 0x%02x\n", stat);

	if ((chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		&& (stat & APSD_DTC_STATUS_DONE_BIT)
		&& !chg->uusb_apsd_rerun_done) {
		/*
		 * Force re-run APSD to handle slow insertion related
		 * charger-mis-detection.
		 */
		chg->uusb_apsd_rerun_done = true;
		smblib_rerun_apsd_if_required(chg);
		return IRQ_HANDLED;
	}
	if ((stat & APSD_DTC_STATUS_DONE_BIT)
		&& !chg->uusb_apsd_rerun_done) {
		/*
		 * Force re-run APSD to handle slow insertion float charger.
		 */
		smblib_read(chg, APSD_RESULT_STATUS_REG, &reg_value);
		if (reg_value & (FLOAT_CHARGER_BIT)) {
			chg->uusb_apsd_rerun_done = true;
			smblib_err(chg, "float charger, rerun apsd\n");
			smblib_rerun_apsd(chg);
			return IRQ_HANDLED;
		}
	}
	smblib_handle_apsd_done(chg,
		(bool)(stat & APSD_DTC_STATUS_DONE_BIT));

	smblib_handle_hvdcp_detect_done(chg,
		(bool)(stat & QC_CHARGER_BIT));

	smblib_handle_hvdcp_check_timeout(chg,
		(bool)(stat & HVDCP_CHECK_TIMEOUT_BIT),
		(bool)(stat & QC_CHARGER_BIT));

	smblib_handle_hvdcp_3p0_auth_done(chg,
		(bool)(stat & QC_AUTH_DONE_STATUS_BIT));

	smblib_handle_sdp_enumeration_done(chg,
		(bool)(stat & ENUMERATION_DONE_BIT));

	smblib_handle_slow_plugin_timeout(chg,
		(bool)(stat & SLOW_PLUGIN_TIMEOUT_BIT));

	smblib_hvdcp_adaptive_voltage_change(chg);

	power_supply_changed(chg->usb_psy);

	rc = smblib_read(chg, APSD_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read APSD_STATUS rc=%d\n", rc);
		return IRQ_HANDLED;
	}
	smblib_err(chg, "APSD_STATUS = 0x%02x\n", stat);

	return IRQ_HANDLED;
}

enum alarmtimer_restart smblib_lpd_recheck_timer(struct alarm *alarm,
						ktime_t time)
{
	union power_supply_propval pval;
	struct smb_charger *chg = container_of(alarm, struct smb_charger,
							lpd_recheck_timer);
	int rc;

	if (chg->lpd_reason == LPD_MOISTURE_DETECTED) {
		pval.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
		rc = smblib_set_prop_typec_power_role(chg, &pval);
		if (rc < 0) {
			smblib_err(chg, "Couldn't write 0x%02x to TYPE_C_INTRPT_ENB_SOFTWARE_CTRL rc=%d\n",
				pval.intval, rc);
			return ALARMTIMER_NORESTART;
		}
		chg->moisture_present = false;
		power_supply_changed(chg->usb_psy);
	} else {
		rc = smblib_masked_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG,
					TYPEC_WATER_DETECTION_INT_EN_BIT,
					TYPEC_WATER_DETECTION_INT_EN_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set TYPE_C_INTERRUPT_EN_CFG_2_REG rc=%d\n",
					rc);
			return ALARMTIMER_NORESTART;
		}
	}

	chg->lpd_stage = LPD_STAGE_NONE;
	chg->lpd_reason = LPD_NONE;

	return ALARMTIMER_NORESTART;
}

#define RSBU_K_300K_UV	3000000
static bool smblib_src_lpd(struct smb_charger *chg)
{
	union power_supply_propval pval;
	bool lpd_flag = false;
	u8 stat;
	int rc;

	if (chg->lpd_disabled)
		return false;

	rc = smblib_read(chg, TYPE_C_SRC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_SRC_STATUS_REG rc=%d\n",
				rc);
		return false;
	}

	switch (stat & DETECTED_SNK_TYPE_MASK) {
	case SRC_DEBUG_ACCESS_BIT:
		if (smblib_rsbux_low(chg, RSBU_K_300K_UV))
			lpd_flag = true;
		break;
	case SRC_RD_RA_VCONN_BIT:
	case SRC_RD_OPEN_BIT:
	case AUDIO_ACCESS_RA_RA_BIT:
	default:
		break;
	}

	if (lpd_flag) {
		chg->lpd_stage = LPD_STAGE_COMMIT;
		pval.intval = POWER_SUPPLY_TYPEC_PR_SINK;
		rc = smblib_set_prop_typec_power_role(chg, &pval);
		if (rc < 0)
			smblib_err(chg, "Couldn't write 0x%02x to TYPE_C_INTRPT_ENB_SOFTWARE_CTRL rc=%d\n",
				pval.intval, rc);
		chg->lpd_reason = LPD_MOISTURE_DETECTED;
		chg->moisture_present =  true;
		vote(chg->usb_icl_votable, LPD_VOTER, true, 0);
		alarm_start_relative(&chg->lpd_recheck_timer,
						ms_to_ktime(60000));
		power_supply_changed(chg->usb_psy);
	} else {
		chg->lpd_reason = LPD_NONE;
		chg->typec_mode = smblib_get_prop_typec_mode(chg);
	}

	return lpd_flag;
}

static void typec_src_fault_condition_cfg(struct smb_charger *chg, bool src)
{
	int rc;
	u8 mask = USBIN_MID_COMP_FAULT_EN_BIT | USBIN_COLLAPSE_FAULT_EN_BIT;

	rc = smblib_masked_write(chg, OTG_FAULT_CONDITION_CFG_REG, mask,
					src ? 0 : mask);
	if (rc < 0)
		smblib_err(chg, "Couldn't write OTG_FAULT_CONDITION_CFG_REG rc=%d\n",
			rc);
}

static void typec_sink_insertion(struct smb_charger *chg)
{
	int rc;
	/* always close q1 to prevent reverse current */
	if (chg->wireless_bq) {
		smblib_dc_chg_q1_enable(chg, true);
		smblib_set_wireless_otg_state(chg, true);
	}
	/* do not vote while wireless attached to allow charging via usb */
	if (chg->wireless_bq && !chg->power_good_en)
		vote(chg->usb_icl_votable, OTG_VOTER, true, 0);

	typec_src_fault_condition_cfg(chg, true);
	rc = smblib_set_charge_param(chg, &chg->param.freq_switcher,
					chg->chg_freq.freq_above_otg_threshold);
	if (rc < 0)
		dev_err(chg->dev, "Error in setting freq_boost rc=%d\n", rc);

	if (chg->use_extcon) {
		smblib_notify_usb_host(chg, true);
		chg->otg_present = true;
	}

	if (!chg->pr_swap_in_progress)
		chg->ok_to_pd = (!(chg->pd_disabled) || chg->early_usb_attach)
					&& !chg->pd_not_supported;

	 if (chg->support_conn_therm) {
                rc = smblib_set_sw_conn_therm_regulation(chg, true);
                if (rc < 0)
                        smblib_err(chg, "Couldn't start SW conn therm rc=%d\n", rc);
        }
}

static void typec_src_insertion(struct smb_charger *chg)
{
	int rc = 0;
	u8 stat;

	if (chg->pr_swap_in_progress) {
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
		return;
	}

	rc = smblib_read(chg, LEGACY_CABLE_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATE_MACHINE_STATUS_REG rc=%d\n",
			rc);
		return;
	}

	chg->typec_legacy = stat & TYPEC_LEGACY_CABLE_STATUS_BIT;
	/*reset typec_legacy to detect PD when power_good_en online*/
	if (chg->power_good_en)
		chg->typec_legacy = false;
	chg->ok_to_pd = (!(chg->typec_legacy || chg->pd_disabled)
			|| chg->early_usb_attach) && !chg->pd_not_supported;

	/* allow apsd proceed to detect QC2/3 */
	if (!chg->ok_to_pd)
		smblib_hvdcp_detect_try_enable(chg, true);

	/* rerun apsd while insert usb in wireless charging */
	if (chg->power_good_en) {
		rc = smblib_request_dpdm(chg, true);
		if (rc < 0)
			smblib_err(chg, "Couldn't to enable DPDM rc=%d\n", rc);
		else
			smblib_rerun_apsd(chg);
	}

	if (chg->support_conn_therm) {
		rc = smblib_set_sw_conn_therm_regulation(chg, true);
		if (rc < 0)
			smblib_err(chg, "Couldn't start SW conn therm rc=%d\n", rc);
	}
}

static void typec_ra_ra_insertion(struct smb_charger *chg)
{
	vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, 500000);
	vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
	chg->ok_to_pd = false;
	smblib_hvdcp_detect_enable(chg, true);
}

static const char * const dr_mode_text[] = {
	"ufp", "dfp", "none"
};

static int smblib_force_dr_mode(struct smb_charger *chg, int mode)
{
	int rc = 0;

	switch (mode) {
	case TYPEC_PORT_SNK:
		rc = smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
			TYPEC_POWER_ROLE_CMD_MASK, EN_SNK_ONLY_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't enable snk, rc=%d\n", rc);
			return rc;
		}
		break;
	case TYPEC_PORT_SRC:
		rc = smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
			TYPEC_POWER_ROLE_CMD_MASK, EN_SRC_ONLY_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't enable src, rc=%d\n", rc);
			return rc;
		}
		break;
	case TYPEC_PORT_DRP:
		rc = smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
			TYPEC_POWER_ROLE_CMD_MASK, 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't enable DRP, rc=%d\n", rc);
			return rc;
		}
		break;
	default:
		smblib_err(chg, "Power role %d not supported\n", mode);
		return -EINVAL;
	}

	chg->dr_mode = mode;

	return rc;
}

int smblib_typec_port_type_set(const struct typec_capability *cap,
					enum typec_port_type type)
{
	struct smb_charger *chg = container_of(cap,
					struct smb_charger, typec_caps);
	int rc = 0;

	mutex_lock(&chg->typec_lock);

	if ((chg->pr_swap_in_progress) || (type == TYPEC_PORT_DRP)) {
		smblib_dbg(chg, PR_MISC, "Ignoring port type request type = %d swap_in_progress = %d\n",
				type, chg->pr_swap_in_progress);
		goto unlock;
	}

	chg->pr_swap_in_progress = true;

	rc = smblib_force_dr_mode(chg, type);
	if (rc < 0) {
		chg->pr_swap_in_progress = false;
		smblib_err(chg, "Failed to force mode, rc=%d\n", rc);
		goto unlock;
	}

	smblib_dbg(chg, PR_MISC, "Requested role %s\n",
				type ? "SINK" : "SOURCE");

	/*
	 * As per the hardware requirements,
	 * schedule the work with required delay.
	 */
	if (!(delayed_work_pending(&chg->role_reversal_check))) {
		cancel_delayed_work_sync(&chg->role_reversal_check);
		schedule_delayed_work(&chg->role_reversal_check,
			msecs_to_jiffies(ROLE_REVERSAL_DELAY_MS));
		vote(chg->awake_votable, TYPEC_SWAP_VOTER, true, 0);
	}

unlock:
	mutex_unlock(&chg->typec_lock);
	return rc;
}

static void smblib_typec_role_check_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
					role_reversal_check.work);
	int rc = 0;

	mutex_lock(&chg->typec_lock);

	switch (chg->dr_mode) {
	case TYPEC_PORT_SNK:
		if (chg->typec_mode < POWER_SUPPLY_TYPEC_SOURCE_DEFAULT) {
			smblib_dbg(chg, PR_MISC, "Role reversal not latched to UFP in %d msecs. Resetting to DRP mode\n",
						ROLE_REVERSAL_DELAY_MS);
			rc = smblib_force_dr_mode(chg, TYPEC_PORT_DRP);
			if (rc < 0)
				smblib_err(chg, "Failed to set DRP mode, rc=%d\n",
						rc);
		} else {
			chg->power_role = POWER_SUPPLY_TYPEC_PR_SINK;
			typec_set_pwr_role(chg->typec_port, TYPEC_SINK);
			typec_set_data_role(chg->typec_port, TYPEC_DEVICE);
			smblib_dbg(chg, PR_MISC, "Role changed successfully to SINK");
		}
		break;
	case TYPEC_PORT_SRC:
		if (chg->typec_mode >= POWER_SUPPLY_TYPEC_SOURCE_DEFAULT
			|| chg->typec_mode == POWER_SUPPLY_TYPEC_NONE) {
			smblib_dbg(chg, PR_MISC, "Role reversal not latched to DFP in %d msecs. Resetting to DRP mode\n",
						ROLE_REVERSAL_DELAY_MS);
			chg->pr_swap_in_progress = false;
			chg->typec_role_swap_failed = true;
			rc = smblib_force_dr_mode(chg, TYPEC_PORT_DRP);
			if (rc < 0)
				smblib_err(chg, "Failed to set DRP mode, rc=%d\n",
					rc);
		} else {
			chg->power_role = POWER_SUPPLY_TYPEC_PR_SOURCE;
			typec_set_pwr_role(chg->typec_port, TYPEC_SOURCE);
			typec_set_data_role(chg->typec_port, TYPEC_HOST);
			smblib_dbg(chg, PR_MISC, "Role changed successfully to SOURCE");
		}
		break;
	default:
		pr_debug("Already in DRP mode\n");
		break;
	}

	chg->pr_swap_in_progress = false;
	vote(chg->awake_votable, TYPEC_SWAP_VOTER, false, 0);
	mutex_unlock(&chg->typec_lock);
}

static void typec_sink_removal(struct smb_charger *chg)
{
	int rc;
	/* if wireless charging deatched, open q1 for wired charging */
	if (chg->wireless_bq && !chg->power_good_en) {
		smblib_dc_chg_q1_enable(chg, false);
		ncp3902_pass_enable(chg, false);
	}

	if (chg->wireless_bq )
		smblib_set_wireless_otg_state(chg, false);

	vote(chg->usb_icl_votable, OTG_VOTER, false, 0);
	typec_src_fault_condition_cfg(chg, false);
	rc = smblib_set_charge_param(chg, &chg->param.freq_switcher,
					chg->chg_freq.freq_removal);
	if (rc < 0)
		dev_err(chg->dev, "Error in setting freq_removal rc=%d\n", rc);

	if (chg->use_extcon) {
		if (chg->otg_present)
			smblib_notify_usb_host(chg, false);
		chg->otg_present = false;
	}
}

static void typec_src_removal(struct smb_charger *chg)
{
	int rc;
	struct smb_irq_data *data;
	struct storm_watch *wdata;
	int sec_charger;
	u8 val[2] = {0};

	sec_charger = chg->sec_pl_present ? POWER_SUPPLY_CHARGER_SEC_PL :
				POWER_SUPPLY_CHARGER_SEC_NONE;

	rc = smblib_select_sec_charger(chg, sec_charger, POWER_SUPPLY_CP_NONE,
					false);
	if (rc < 0)
		dev_err(chg->dev,
			"Couldn't disable secondary charger rc=%d\n", rc);

	/* Reset QC3.5 Flag and power limit*/
	chg->qc3p5_authenticated = false;
	chg->qc3p5_auth_complete = false;
	chg->qc3p5_authentication_started = false;
	chg->qc3p5_dp_tune_rapidly = false;
	chg->qc3p5_power_limit_w = 18;

	chg->qc3p5_detected = false;
	typec_src_fault_condition_cfg(chg, false);
	smblib_hvdcp_detect_try_enable(chg, false);
	if (chg->fake_plug_out == false)
		smblib_update_usb_type(chg);

	if (chg->wa_flags & BOOST_BACK_WA) {
		data = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data;
		if (data) {
			wdata = &data->storm_data;
			update_storm_count(wdata, WEAK_CHG_STORM_COUNT);
			vote(chg->usb_icl_votable, BOOST_BACK_VOTER, false, 0);
			vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
					false, 0);
		}
	}

	cancel_delayed_work_sync(&chg->pl_enable_work);
	cancel_delayed_work_sync(&chg->raise_qc3_vbus_work);
	cancel_delayed_work_sync(&chg->check_init_boot);

	/* reset input current limit voters */
	vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
			is_flash_active(chg) ? SDP_CURRENT_UA : SDP_100_MA);
	vote(chg->usb_icl_votable, PD_VOTER, false, 0);
	vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
	vote(chg->usb_icl_votable, DCP_VOTER, false, 0);
	vote(chg->usb_icl_votable, SW_QC3_VOTER, false, 0);
	vote(chg->usb_icl_votable, CTM_VOTER, false, 0);
	vote(chg->usb_icl_votable, HVDCP2_ICL_VOTER, false, 0);
	vote(chg->usb_icl_votable, CHG_TERMINATION_VOTER, false, 0);
	vote(chg->usb_icl_votable, THERMAL_THROTTLE_VOTER, false, 0);
	vote(chg->usb_icl_votable, OTG_VOTER, false, 0);
	vote(chg->usb_icl_votable, LPD_VOTER, false, 0);
	vote(chg->usb_icl_votable, QC2_UNSUPPORTED_VOTER, false, 0);
	vote(chg->usb_icl_votable, QC3P5_VOTER, false, 0);

	/* reset usb irq voters */
	vote(chg->limited_irq_disable_votable, CHARGER_TYPE_VOTER,
			true, 0);
	vote(chg->hdc_irq_disable_votable, CHARGER_TYPE_VOTER, true, 0);
	vote(chg->hdc_irq_disable_votable, HDC_IRQ_VOTER, false, 0);

	/* reset parallel voters */
	vote(chg->pl_disable_votable, PL_DELAY_VOTER, true, 0);
	vote(chg->pl_disable_votable, PL_FCC_LOW_VOTER, false, 0);
	vote(chg->pl_enable_votable_indirect, USBIN_I_VOTER, false, 0);
	vote(chg->pl_enable_votable_indirect, USBIN_V_VOTER, false, 0);
	vote(chg->awake_votable, PL_DELAY_VOTER, false, 0);

	/* Remove SW thermal regulation WA votes */
	vote(chg->usb_icl_votable, SW_THERM_REGULATION_VOTER, false, 0);
	vote(chg->pl_disable_votable, SW_THERM_REGULATION_VOTER, false, 0);
	vote(chg->dc_suspend_votable, SW_THERM_REGULATION_VOTER, false, 0);
	if (chg->cp_disable_votable)
		vote(chg->cp_disable_votable, SW_THERM_REGULATION_VOTER,
								false, 0);
	vote(chg->fcc_votable, CLASSA_QC_FCC_VOTER, false, 0);
	vote(chg->fcc_votable, PD_VERIFED_VOTER, false, 0);
	vote(chg->usb_icl_votable, QC_A_CP_ICL_MAX_VOTER, false, 0);
	vote(chg->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER, false, 0);
	vote(chg->usb_icl_votable, WIRELESS_BY_USB_IN_VOTER, false, 0);
	vote(chg->chg_disable_votable, AFTER_FFC_VOTER, false, 0);
	/* clear chg_awake wakeup source when typec removal */
	vote(chg->awake_votable, CHG_AWAKE_VOTER, false, 0);
	/* reset USBOV votes and cancel work */
	cancel_delayed_work_sync(&chg->usbov_dbc_work);
	vote(chg->awake_votable, USBOV_DBC_VOTER, false, 0);
	chg->dbc_usbov = false;

	chg->pulse_cnt = 0;
	chg->usb_icl_delta_ua = 0;
	chg->voltage_min_uv = MICRO_5V;
	chg->voltage_max_uv = MICRO_5V;
	chg->usbin_forced_max_uv = 0;
	chg->chg_param.forced_main_fcc = 0;

	if (chg->six_pin_step_charge_enable) {
		chg->init_start_vbat_checked = false;
		chg->trigger_taper_count = 0;
		chg->index_vfloat = 0;
		chg->health_not_good = false;
		chg->flag_second_ffc_term_current = false;
		vote(chg->fv_votable, SIX_PIN_VFLOAT_VOTER, false, 0);
		vote(chg->fcc_votable, SIX_PIN_VFLOAT_VOTER, false, 0);
		vote(chg->usb_icl_votable, MAIN_ICL_MIN_VOTER, false, 0);
	}

	/* Reset all CC mode votes */
	vote(chg->fcc_main_votable, MAIN_FCC_VOTER, false, 0);
	vote(chg->fcc_votable, HVDCP2_FCC_VOTER, false, 0);
	vote(chg->fcc_votable, FCC_MAX_QC3P5_VOTER, false, 0);
	chg->adapter_cc_mode = 0;
	chg->thermal_overheat = 0;
	vote_override(chg->fcc_votable, CC_MODE_VOTER, false, 0);
	vote_override(chg->usb_icl_votable, CC_MODE_VOTER, false, 0);
	vote(chg->cp_disable_votable, OVERHEAT_LIMIT_VOTER, false, 0);
	vote(chg->usb_icl_votable, OVERHEAT_LIMIT_VOTER, false, 0);
	vote_override(chg->fcc_main_votable, MAIN_FCC_VOTER, false, 0);

	vote(chg->fcc_votable, THERMAL_FCC_OVERRIDE_VOTER, false, 0);
	/* write back the default FLOAT charger configuration */
	rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				(u8)FLOAT_OPTIONS_MASK, chg->float_cfg);
	if (rc < 0)
		smblib_err(chg, "Couldn't write float charger options rc=%d\n",
			rc);

	if (chg->sdam_base) {
		rc = smblib_write(chg,
			chg->sdam_base + SDAM_QC_DET_STATUS_REG, 0);
		if (rc < 0)
			pr_err("Couldn't clear SDAM QC status rc=%d\n", rc);

		rc = smblib_batch_write(chg,
			chg->sdam_base + SDAM_QC_ADC_LSB_REG, val, 2);
		if (rc < 0)
			pr_err("Couldn't clear SDAM ADC status rc=%d\n", rc);
	}

	if (!chg->pr_swap_in_progress) {
		rc = smblib_usb_pd_adapter_allowance_override(chg, FORCE_NULL);
		if (rc < 0)
			smblib_err(chg, "Couldn't set FORCE_NULL rc=%d\n", rc);

		rc = smblib_set_charge_param(chg,
				&chg->param.aicl_cont_threshold,
				chg->default_aicl_cont_threshold_mv);
		if (rc < 0)
			smblib_err(chg, "Couldn't restore aicl_cont_threshold, rc=%d",
					rc);
	}
	/*
	 * if non-compliant charger caused UV, restore original max pulses
	 * and turn SUSPEND_ON_COLLAPSE_USBIN_BIT back on.
	 */
	if (chg->qc2_unsupported_voltage) {
		rc = smblib_masked_write(chg, HVDCP_PULSE_COUNT_MAX_REG,
				HVDCP_PULSE_COUNT_MAX_QC2_MASK,
				chg->qc2_max_pulses);
		if (rc < 0)
			smblib_err(chg, "Couldn't restore max pulses rc=%d\n",
					rc);

		/* always disable suspend when usbin collapse, so do not restore it here */
		/*rc = smblib_masked_write(chg, USBIN_AICL_OPTIONS_CFG_REG,
				SUSPEND_ON_COLLAPSE_USBIN_BIT,
				SUSPEND_ON_COLLAPSE_USBIN_BIT);
		if (rc < 0)
			smblib_err(chg, "Couldn't turn on SUSPEND_ON_COLLAPSE_USBIN_BIT rc=%d\n",
					rc);*/

		chg->qc2_unsupported_voltage = QC2_COMPLIANT;
	}

	if (chg->use_extcon)
		smblib_notify_device_mode(chg, false);

	typec_partner_unregister(chg);
	chg->typec_legacy = false;

	del_timer_sync(&chg->apsd_timer);
	chg->apsd_ext_timeout = false;
	/* when src removal, set bark timer back to default 16s */
	smblib_set_wdog_bark_timer(chg, BARK_TIMER_NORMAL);

	chg->typec_legacy = false;
	chg->detect_low_power_qc3_charger = false;
	chg->raise_vbus_to_detect = false;
	chg->is_qc_class_a = false;
	chg->is_qc_class_b = false;
	chg->high_vbus_detected = false;
	chg->recheck_charger = false;
	chg->snk_debug_acc_detected = false;
	chg->pps_thermal_level = -EINVAL;
	chg->qc2_unsupported = false;
	chg->cc_un_compliant_detected = false;
	chg->report_input_absent = false;
	chg->qc3_raise_done = false;

	if (chg->pd_verifed) {
		chg->pd_verifed = false;
		if (smblib_get_fastcharge_mode(chg) == true) {
			smblib_set_fastcharge_mode(chg, false);
			chg->last_ffc_remove_time = ktime_get();
		}
	}
}

static void typec_mode_unattached(struct smb_charger *chg)
{
	vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, USBIN_100MA);
}

static void smblib_handle_rp_change(struct smb_charger *chg, int typec_mode)
{
	const struct apsd_result *apsd = smblib_get_apsd_result(chg);

	/*
	 * We want the ICL vote @ 100mA for a FLOAT charger
	 * until the detection by the USB stack is complete.
	 * Ignore the Rp changes unless there is a
	 * pre-existing valid vote or FLOAT is configured for
	 * SDP current.
	 */
	if (apsd->pst == POWER_SUPPLY_TYPE_USB_FLOAT) {
		if (get_client_vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER)
					<= USBIN_100MA
			|| (chg->float_cfg & FLOAT_OPTIONS_MASK)
					== FORCE_FLOAT_SDP_CFG_BIT)
			return;
	}

	update_sw_icl_max(chg, apsd->pst);

	smblib_dbg(chg, PR_MISC, "CC change old_mode=%d new_mode=%d\n",
						chg->typec_mode, typec_mode);
}

static void smblib_lpd_launch_ra_open_work(struct smb_charger *chg)
{
	u8 stat;
	int rc;

	if (chg->lpd_disabled)
		return;

	rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_MISC_STATUS_REG rc=%d\n",
			rc);
		return;
	}

	if (!(stat & TYPEC_TCCDEBOUNCE_DONE_STATUS_BIT)
			&& chg->lpd_stage == LPD_STAGE_NONE) {
		chg->lpd_stage = LPD_STAGE_FLOAT;
		cancel_delayed_work_sync(&chg->lpd_ra_open_work);
		vote(chg->awake_votable, LPD_VOTER, true, 0);
		schedule_delayed_work(&chg->lpd_ra_open_work,
						msecs_to_jiffies(300));
	}
}

irqreturn_t typec_or_rid_detection_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		if (chg->uusb_moisture_protection_enabled) {
			/*
			 * Adding pm_stay_awake as because pm_relax is called
			 * on exit path from the work routine.
			 */
			pm_stay_awake(chg->dev);
			schedule_work(&chg->moisture_protection_work);
		}

		cancel_delayed_work_sync(&chg->uusb_otg_work);
		/*
		 * Skip OTG enablement if RID interrupt triggers with moisture
		 * protection still enabled.
		 */
		if (!chg->moisture_present) {
			vote(chg->awake_votable, OTG_DELAY_VOTER, true, 0);
			smblib_dbg(chg, PR_INTERRUPT, "Scheduling OTG work\n");
			schedule_delayed_work(&chg->uusb_otg_work,
				msecs_to_jiffies(chg->otg_delay_ms));
		}

		goto out;
	}

	if (chg->pr_swap_in_progress || chg->pd_hard_reset)
		goto out;

	smblib_lpd_launch_ra_open_work(chg);

	if (chg->usb_psy)
		power_supply_changed(chg->usb_psy);

out:
	return IRQ_HANDLED;
}

irqreturn_t typec_state_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int typec_mode;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		smblib_dbg(chg, PR_INTERRUPT,
				"Ignoring for micro USB\n");
		return IRQ_HANDLED;
	}

	typec_mode = smblib_get_prop_typec_mode(chg);
	if (chg->sink_src_mode != UNATTACHED_MODE
			&& (typec_mode != chg->typec_mode))
		smblib_handle_rp_change(chg, typec_mode);
	chg->typec_mode = typec_mode;

	smblib_dbg(chg, PR_MISC, "IRQ: cc-state-change; Type-C %s detected\n",
				smblib_typec_mode_name[chg->typec_mode]);

	power_supply_changed(chg->usb_psy);

	return IRQ_HANDLED;
}

static void smblib_lpd_clear_ra_open_work(struct smb_charger *chg)
{
	if (chg->lpd_disabled)
		return;

	cancel_delayed_work_sync(&chg->lpd_detach_work);
	chg->lpd_stage = LPD_STAGE_FLOAT_CANCEL;
	cancel_delayed_work_sync(&chg->lpd_ra_open_work);
	vote(chg->awake_votable, LPD_VOTER, false, 0);
}

#define TYPEC_DETACH_DETECT_DELAY_MS 2000
irqreturn_t typec_attach_detach_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	u8 stat;
	bool attached = false;
	int rc;

	smblib_dbg(chg, PR_OEM, "IRQ: %s\n", irq_data->name);

	/* IRQ not expected to be executed for uUSB, return */
	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return IRQ_HANDLED;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	rc = smblib_read(chg, TYPE_C_STATE_MACHINE_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATE_MACHINE_STATUS_REG rc=%d\n",
			rc);
		return IRQ_HANDLED;
	}

	attached = !!(stat & TYPEC_ATTACH_DETACH_STATE_BIT);

	if (attached) {
		smblib_lpd_clear_ra_open_work(chg);

		rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read TYPE_C_MISC_STATUS_REG rc=%d\n",
				rc);
			return IRQ_HANDLED;
		}

		if (smblib_get_prop_dfp_mode(chg) ==
				POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER) {
			chg->sink_src_mode = AUDIO_ACCESS_MODE;
			if (!chg->power_good_en)
				typec_ra_ra_insertion(chg);
		} else if (stat & SNK_SRC_MODE_BIT) {
			if (smblib_src_lpd(chg))
				return IRQ_HANDLED;
			chg->sink_src_mode = SRC_MODE;
			typec_sink_insertion(chg);
		} else {
			chg->sink_src_mode = SINK_MODE;
			typec_src_insertion(chg);
			if (chg->support_wireless)
				smblib_wireless_set_rx_sleep_pin(chg, false);
		}

	} else {
		switch (chg->sink_src_mode) {
		case SRC_MODE:
			typec_sink_removal(chg);
			break;
		case SINK_MODE:
			typec_src_removal(chg);
			if (chg->support_wireless) {
				smblib_wireless_set_rx_sleep_pin(chg, true);
			}
#ifdef CONFIG_DUAL_FUEL_GAUGE_BQ27Z561
		Dual_Fg_Reset_Batt_Ctrl_gpio_default();
#endif
			break;
		case AUDIO_ACCESS_MODE:
			if (!chg->power_good_en) {
				typec_src_removal(chg);
				if (chg->support_wireless) {
					smblib_wireless_set_rx_sleep_pin(chg, true);
				}
			}
			break;
		case UNATTACHED_MODE:
		default:
			typec_mode_unattached(chg);
			break;
		}

		if (!chg->pr_swap_in_progress) {
			chg->ok_to_pd = false;
			chg->sink_src_mode = UNATTACHED_MODE;
			chg->early_usb_attach = false;
			smblib_apsd_enable(chg, true);
		}

		if (chg->lpd_stage == LPD_STAGE_FLOAT_CANCEL)
			schedule_delayed_work(&chg->lpd_detach_work,
					msecs_to_jiffies(1000));
	}

	rc = smblib_masked_write(chg, USB_CMD_PULLDOWN_REG,
			EN_PULLDOWN_USB_IN_BIT,
			attached ?  0 : EN_PULLDOWN_USB_IN_BIT);
	if (rc < 0)
		smblib_err(chg, "Couldn't configure pulldown on USB_IN rc=%d\n",
				rc);

	power_supply_changed(chg->usb_psy);

	return IRQ_HANDLED;
}

/*add for wireless reverse charge to disable dc*/
int smblib_set_sw_disable_dc_en(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc = 0;

	if (val->intval) {
		smblib_dbg(chg, PR_OEM, "disable dc en by sw\n");
		/* step1: enter dc suspend */
		rc = vote(chg->dc_suspend_votable, SW_DISABLE_DC_VOTER,
			true, 0);

		/* step2: force dcin_en low */
		rc = smblib_masked_write(chg, DCIN_CMD_IL_REG,
						 DCIN_EN_OVERRIDE_BIT | DCIN_EN_BIT,
						 DCIN_EN_OVERRIDE_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure dc_en override rc=%d\n", rc);
			return rc;
		}

		/* step3: enable pull-down on dcin_pon and mid_chg */
		rc = smblib_masked_write(chg, DCIN_CMD_PULLDOWN_REG,
						 DCIN_PULLDOWN_EN_BIT | DCIN_MID_PULLDOWN_BIT,
						 DCIN_PULLDOWN_EN_BIT | DCIN_MID_PULLDOWN_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable dcin_pulldown rc=%d\n", rc);
			return rc;
		}
		/* wait 10ms to pull mid_chg lower than Vsys+Vrevi */
		msleep(10);

		/* step4: exit dc suspend */
		rc = vote(chg->dc_suspend_votable, SW_DISABLE_DC_VOTER,
			false, 0);
	} else {
		smblib_dbg(chg, PR_OEM, "enable dc en by sw\n");
		/* step1: enter dc suspend */
		rc = vote(chg->dc_suspend_votable, SW_DISABLE_DC_VOTER,
			true, 0);

		/* step2: force dcin_en low */
		rc = smblib_masked_write(chg, DCIN_CMD_IL_REG,
						 DCIN_EN_OVERRIDE_BIT, 0);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure dc_en override rc=%d\n", rc);
			return rc;
		}

		/* step3: enable pull-down on dcin_pon and mid_chg */
		rc = smblib_masked_write(chg, DCIN_CMD_PULLDOWN_REG,
						 DCIN_PULLDOWN_EN_BIT | DCIN_MID_PULLDOWN_BIT, 0);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't disable dcin_pulldown rc=%d\n", rc);
			return rc;
		}
		/* wait 10ms to pull mid_chg lower than Vsys+Vrevi */
		msleep(10);
		/* step4: exit dc suspend */
		rc = vote(chg->dc_suspend_votable, SW_DISABLE_DC_VOTER,
			false, 0);
	}

	return 0;
}

static int smblib_set_dc_pach_safe_when_power_good_enable(struct smb_charger *chg)
{

	int rc = 0;

	/* step1: force dcin_en low */
	rc = smblib_masked_write(chg, DCIN_CMD_IL_REG,
					DCIN_EN_OVERRIDE_BIT | DCIN_EN_BIT,
					DCIN_EN_OVERRIDE_BIT);
	if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure dc_en override rc=%d\n", rc);
			return rc;
	}

	/* step2: enable pull-down on dcin_pon and mid_chg */
	rc = smblib_masked_write(chg, DCIN_CMD_PULLDOWN_REG,
					DCIN_PULLDOWN_EN_BIT | DCIN_MID_PULLDOWN_BIT,
					DCIN_PULLDOWN_EN_BIT | DCIN_MID_PULLDOWN_BIT);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't enable dcin_pulldown rc=%d\n", rc);
		return rc;
	}
	/* wait 10ms to pull mid_chg lower than Vsys+Vrevi */
	msleep(10);

	/* step3: disable pull-down on dcin_pon and mid_chg */
	rc = smblib_masked_write(chg, DCIN_CMD_PULLDOWN_REG,
						 DCIN_PULLDOWN_EN_BIT | DCIN_MID_PULLDOWN_BIT,
						 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't disable dcin_pulldown rc=%d\n", rc);
		return rc;
	}

	/* step4: remove dcin_en low */
	rc = smblib_masked_write(chg, DCIN_CMD_IL_REG,
						 DCIN_EN_OVERRIDE_BIT | DCIN_EN_BIT,
						 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure dc_en override rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static void smblib_dc_plug_out_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							dc_plug_out_delay_work.work);

	chg->fake_dc_on = 0;  /*use for delay 1.8s*/
	power_supply_changed(chg->dc_psy);
	smblib_dbg(chg, PR_WLS, "Delay timeout and clear dc fake value\n");
	vote(chg->awake_votable, DC_PLUGOUT_WIRELESS_VOTER, false, 0);
}

static void smblib_wireless_delay_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							wireless_full_delay_work.work);
	int rc;
	u8 stat;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return;
	}

	stat = stat & BATTERY_CHARGER_STATUS_MASK;
	if (stat == TERMINATE_CHARGE) {
		smblib_dbg(chg, PR_OEM, "release wireless lock when full\n");
		/* when wpc charge done, set bark timer to 128s to decrease wakeups */
		smblib_set_wdog_bark_timer(chg, BARK_TIMER_LONG);
		vote(chg->awake_votable, DC_AWAKE_VOTER, false, 0);
	}
}

#define POWER_GOOD_OFF_DELAY_MS 1800
int smblib_set_wirless_power_good_enable(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc = 0;

	chg->power_good_en = val->intval;
	smblib_dbg(chg, PR_OEM, "power good: %d\n", chg->power_good_en);

	if (!chg->wireless_bq)
		return rc;

	if (chg->power_good_en) {
		chg->fake_dc_on = 1;
		smblib_dc_chg_q1_enable(chg, true);
		cancel_delayed_work(&chg->dc_plug_out_delay_work);

		if (chg->typec_mode == POWER_SUPPLY_TYPEC_SINK
			|| chg->typec_mode == POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE){
			smblib_dbg(chg, PR_OEM, "typec :%d, en 3902 pass\n", chg->typec_mode);
			ncp3902_pass_enable(chg, true);
		}

		/* disable OTG Voter for chargting via USB */
		vote(chg->usb_icl_votable, OTG_VOTER, false, 0);
		vote(chg->awake_votable, DC_PLUGOUT_WIRELESS_VOTER, false, 0);
		rc = smblib_usb_pd_adapter_allowance_override(chg, FORCE_NULL);
		if (rc < 0)
			pr_err("Failed to set adapter allowance to 5V or 9v to 12v\n");

		rc = smblib_masked_write(chg, USBIN_ADAPTER_ALLOW_CFG_REG,
				USBIN_ADAPTER_ALLOW_MASK, USBIN_ADAPTER_ALLOW_5V_TO_12V);
		if (rc < 0) {
			pr_err("Couldn't set USBIN_ADAPTER_ALLOW_CFG_REG rc=%d\n", rc);
		}

		rc = smblib_icl_override(chg, SW_OVERRIDE_NO_CC_MODE);
		if (rc < 0) {
			pr_err("Couldn't disable ICL override rc=%d\n", rc);
			return rc;
		}
	} else {
		/* delay 1.8s to show discharging */
		schedule_delayed_work(&chg->dc_plug_out_delay_work,
				msecs_to_jiffies(POWER_GOOD_OFF_DELAY_MS));
		vote(chg->awake_votable, DC_PLUGOUT_WIRELESS_VOTER, true, 0);
		/*
		* wireless detached and OTG plug-out, open q1 for wired charging.
		* wireless detached and OTG plug-in, vote usb-icl to 0 to prevent charging hint.
		*/

		/* close ncp pass mode first for OTG and wireless conflict process */
		ncp3902_pass_enable(chg, false);
		msleep(50);

		/* close ncp second for OTG and wireless conflict process */
		if (is_ncp3902_pull_low(chg)) {
			smblib_ncp3902_enable(chg, true);
			msleep(50);
		}

		/* open q1 last for OTG and wireless conflict process */
		if (chg->typec_mode != POWER_SUPPLY_TYPEC_SINK
			&& chg->typec_mode != POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE)
			smblib_dc_chg_q1_enable(chg, false);
		else
			vote(chg->usb_icl_votable, OTG_VOTER, true, 0);

		smblib_wl_bq_enable(chg, false);
		if (smblib_get_fastcharge_mode(chg) == true)
			smblib_set_fastcharge_mode(chg, false);

		smblib_set_dc_pach_safe_when_power_good_enable(chg);

		rc = smblib_usb_pd_adapter_allowance_override(chg, FORCE_NULL);
		if (rc < 0)
			pr_err("Failed to set adapter allowance to 5V or 9v to 12v\n");

		rc = smblib_masked_write(chg, USBIN_ADAPTER_ALLOW_CFG_REG,
				USBIN_ADAPTER_ALLOW_MASK, USBIN_ADAPTER_ALLOW_5V_OR_9V_TO_12V);
		if (rc < 0) {
			pr_err("Couldn't set USBIN_ADAPTER_ALLOW_CFG_REG rc=%d\n", rc);
		}

		chg->en_bq_flag = 0;
		rc = smblib_icl_override(chg, HW_AUTO_MODE);
		if (rc < 0) {
			pr_err("Couldn't disable ICL override rc=%d\n", rc);
			return rc;
		}
		/* close voter of termination when wireless plug out*/
		vote(chg->usb_icl_votable, CHG_TERMINATION_VOTER, false, 0);
	}

	if (chg->dc_psy)
		power_supply_changed(chg->dc_psy);
	if (chg->wireless_psy)
		power_supply_changed(chg->wireless_psy);
	if (chg->batt_psy)
		power_supply_changed(chg->batt_psy);

	return 0;
}


int smblib_set_wirless_cp_enable(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	/* if use usbmid and dcin wireless charge, to do later*/
	if (!chg->wireless_bq)
		return 0;

	if ((val->intval) && (chg->power_good_en)) {
		printk("enable bq for quick wireless charge\n");
		smblib_dbg(chg, PR_WLS, "enable bq for quick wireless charge\n");
		smblib_wl_bq_enable(chg, true);
		smblib_dc_therm_charging(chg, chg->dc_temp_level);
		chg->en_bq_flag = 1;
		/* enable FFC when cp enabled */
		if (smblib_get_fastcharge_mode(chg) == false)
			smblib_set_fastcharge_mode(chg, true);
		if (chg->six_pin_step_charge_enable) {
			/* start six pin step charge monitor work */
			schedule_delayed_work(&chg->six_pin_batt_step_chg_work,
					msecs_to_jiffies(STEP_CHG_DELAYED_START_MS));
		}
	} else {
		printk("disable bq quick charge\n");
		smblib_dbg(chg, PR_WLS, "disable bq quick charge\n");
		smblib_wl_bq_enable(chg, false);
		chg->en_bq_flag = 0;
	}

	if (chg->wireless_psy)
		power_supply_changed(chg->wireless_psy);
	return 0;
}

static void smblib_charger_type_recheck(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
			charger_type_recheck.work);
	int recheck_time = TYPE_RECHECK_TIME_5S;
	static int last_charger_type, check_count;
	int rc, dc_power_on;
	union power_supply_propval pval = {0,};

	smblib_get_prop_dc_present(chg, &pval);
	dc_power_on = pval.intval;
	smblib_update_usb_type(chg);
	smblib_dbg(chg, PR_OEM, "typec_mode:%d,last:%d: real charger type:%d\n",
			chg->typec_mode, last_charger_type, chg->real_charger_type);

	if (last_charger_type != chg->real_charger_type)
		check_count--;
	last_charger_type = chg->real_charger_type;

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP_3 ||
			chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP_3P5 ||
			chg->real_charger_type == POWER_SUPPLY_TYPE_USB_CDP ||
			chg->pd_active || (check_count >= TYPE_RECHECK_COUNT) ||
			((chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP) &&
			(chg->qc2_unsupported == true)) ||
			((chg->real_charger_type == POWER_SUPPLY_TYPE_USB_FLOAT) &&
			(chg->typec_mode == POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER)) ||
			((chg->real_charger_type == POWER_SUPPLY_TYPE_USB_FLOAT) &&
			  dc_power_on)) {
		smblib_dbg(chg, PR_OEM, "hvdcp detect or check_count = %d break\n",
				check_count);
		check_count = 0;
		return;
	}

	if (smblib_get_prop_dfp_mode(chg) != POWER_SUPPLY_TYPEC_NONE)
		goto check_next;

		if (chg->typec_port && !chg->pr_swap_in_progress) {

			/*
			 * Schedule the work to differentiate actual removal
			 * of cable and detach interrupt during role swap,
			 * unregister the partner only during actual cable
			 * removal.
			 */
			cancel_delayed_work(&chg->pr_swap_detach_work);
			vote(chg->awake_votable, DETACH_DETECT_VOTER, true, 0);
			schedule_delayed_work(&chg->pr_swap_detach_work,
				msecs_to_jiffies(TYPEC_DETACH_DETECT_DELAY_MS));
			smblib_force_dr_mode(chg, TYPEC_PORT_DRP);
			/*
			 * To handle cable removal during role
			 * swap failure.
			 */
			chg->typec_role_swap_failed = false;
		}

	if (!chg->recheck_charger)
		chg->precheck_charger_type = chg->real_charger_type;
	chg->recheck_charger = true;

	/* need request hsusb phy dpdm to false then true for float charger */
	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_FLOAT) {
		rc = smblib_request_dpdm(chg, false);
		if (rc < 0)
			smblib_err(chg, "Couldn't disable DPDM rc=%d\n", rc);

		msleep(500);
	}

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP)
		chg->hvdcp_recheck_status = true;

	smblib_rerun_apsd_if_required(chg);

check_next:
	check_count++;
	schedule_delayed_work(&chg->charger_type_recheck,
				msecs_to_jiffies(recheck_time));
}

static void dcin_aicl(struct smb_charger *chg)
{
	int rc, icl, icl_save;
	int input_present;
	bool aicl_done = true;

	/*
	 * Hold awake votable to prevent pm_relax being called prior to
	 * completion of this work.
	 */
	vote(chg->awake_votable, DCIN_AICL_VOTER, true, 0);

increment:
	mutex_lock(&chg->dcin_aicl_lock);

	rc = smblib_get_charge_param(chg, &chg->param.dc_icl, &icl);
	if (rc < 0)
		goto err;

	if (icl == chg->wls_icl_ua) {
		/* Upper limit reached; do nothing */
		smblib_dbg(chg, PR_WLS, "hit max ICL: stop\n");

		rc = smblib_is_input_present(chg, &input_present);
		if (rc < 0 || !(input_present & INPUT_PRESENT_DC))
			aicl_done = false;

		goto unlock;
	}

	icl = min(chg->wls_icl_ua, icl + DCIN_ICL_STEP_UA);
	icl_save = icl;

	rc = smblib_set_charge_param(chg, &chg->param.dc_icl, icl);
	if (rc < 0)
		goto err;

	mutex_unlock(&chg->dcin_aicl_lock);

	smblib_dbg(chg, PR_WLS, "icl: %d mA\n", (icl / 1000));

	/* Check to see if DC is still present before and after sleep */
	rc = smblib_is_input_present(chg, &input_present);
	if (rc < 0 || !(input_present & INPUT_PRESENT_DC)) {
		aicl_done = false;
		goto unvote;
	}

	/*
	 * Wait awhile to check for any DCIN_UVs (the UV handler reduces the
	 * ICL). If the adaptor collapses, the ICL read after waking up will be
	 * lesser, indicating that the AICL process is complete.
	 */
	msleep(500);

	rc = smblib_is_input_present(chg, &input_present);
	if (rc < 0 || !(input_present & INPUT_PRESENT_DC)) {
		aicl_done = false;
		goto unvote;
	}

	mutex_lock(&chg->dcin_aicl_lock);

	rc = smblib_get_charge_param(chg, &chg->param.dc_icl, &icl);
	if (rc < 0)
		goto err;

	if (icl < icl_save) {
		smblib_dbg(chg, PR_WLS, "done: icl: %d mA\n", (icl / 1000));
		goto unlock;
	}

	mutex_unlock(&chg->dcin_aicl_lock);

	goto increment;

err:
	aicl_done = false;
unlock:
	mutex_unlock(&chg->dcin_aicl_lock);
unvote:
	vote(chg->awake_votable, DCIN_AICL_VOTER, false, 0);
	chg->dcin_aicl_done = aicl_done;
}

static void dcin_aicl_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						dcin_aicl_work);
	dcin_aicl(chg);
}

static enum alarmtimer_restart dcin_aicl_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct smb_charger *chg = container_of(alarm, struct smb_charger,
					dcin_aicl_alarm);

	smblib_dbg(chg, PR_WLS, "rerunning DCIN AICL\n");

	pm_stay_awake(chg->dev);
	schedule_work(&chg->dcin_aicl_work);

	return ALARMTIMER_NORESTART;
}

static void dcin_icl_decrement(struct smb_charger *chg)
{
	int rc, icl;
	ktime_t now = ktime_get();

	rc = smblib_get_charge_param(chg, &chg->param.dc_icl, &icl);
	if (rc < 0) {
		smblib_err(chg, "reading DCIN ICL failed: %d\n", rc);
		return;
	}

	if (icl == DCIN_ICL_MIN_UA) {
		/* Cannot possibly decrease ICL any further - do nothing */
		smblib_dbg(chg, PR_WLS, "hit min ICL: stop\n");
		return;
	}

	/* Reduce ICL by 100 mA if 3 UVs happen in a row */
	if (ktime_us_delta(now, chg->dcin_uv_last_time) > (200 * 1000)) {
		chg->dcin_uv_count = 0;
	} else if (chg->dcin_uv_count >= 3) {
		icl -= DCIN_ICL_STEP_UA;

		smblib_dbg(chg, PR_WLS, "icl: %d mA\n", (icl / 1000));
		rc = smblib_set_charge_param(chg, &chg->param.dc_icl, icl);
		if (rc < 0) {
			smblib_err(chg, "setting DCIN ICL failed: %d\n", rc);
			return;
		}

		chg->dcin_uv_count = 0;
	}

	chg->dcin_uv_last_time = now;
}

irqreturn_t dcin_uv_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	/* directly return irq_handled if use bq wireless solution */
	if (chg->wireless_bq)
		return IRQ_HANDLED;

	mutex_lock(&chg->dcin_aicl_lock);

	chg->dcin_uv_count++;
	smblib_dbg(chg, (PR_WLS | PR_INTERRUPT), "DCIN UV count: %d\n",
			chg->dcin_uv_count);
	dcin_icl_decrement(chg);

	mutex_unlock(&chg->dcin_aicl_lock);

	return IRQ_HANDLED;
}

irqreturn_t dc_plugin_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	union power_supply_propval pval;
	int input_present;
	bool dcin_present, vbus_present;
	int rc, wireless_vout = 0, wls_set = 0;
	int sec_charger;

	/* directly return irq_handled if use bq wireless solution */
	if (chg->wireless_bq)
		return IRQ_HANDLED;

	rc = smblib_get_prop_vph_voltage_now(chg, &pval);
	if (rc < 0)
		return IRQ_HANDLED;

	/* 2*VPH, with a granularity of 100mV */
	wireless_vout = ((pval.intval * 2) / 100000) * 100000;

	rc = smblib_is_input_present(chg, &input_present);
	if (rc < 0)
		return IRQ_HANDLED;

	dcin_present = input_present & INPUT_PRESENT_DC;
	vbus_present = input_present & INPUT_PRESENT_USB;

	if (!chg->cp_ilim_votable)
		chg->cp_ilim_votable = find_votable("CP_ILIM");

	if (dcin_present && !vbus_present) {
		cancel_work_sync(&chg->dcin_aicl_work);

		/* Reset DCIN ICL to 100 mA */
		mutex_lock(&chg->dcin_aicl_lock);
		rc = smblib_set_charge_param(chg, &chg->param.dc_icl,
				DCIN_ICL_MIN_UA);
		mutex_unlock(&chg->dcin_aicl_lock);
		if (rc < 0)
			return IRQ_HANDLED;

		smblib_dbg(chg, (PR_WLS | PR_INTERRUPT), "reset: icl: 100 mA\n");

		/*
		 * Remove USB's CP ILIM vote - inapplicable for wireless
		 * parallel charging.
		 */
		if (chg->cp_ilim_votable)
			vote(chg->cp_ilim_votable, ICL_CHANGE_VOTER, false, 0);

		if (chg->sec_cp_present) {
			/*
			 * If CP output topology is VBATT, limit main charger's
			 * FCC share and let the CPs handle the rest.
			 */
			if (is_cp_topo_vbatt(chg))
				vote(chg->fcc_main_votable,
					WLS_PL_CHARGING_VOTER, true, 800000);

			rc = smblib_get_prop_batt_status(chg, &pval);
			if (rc < 0)
				smblib_err(chg, "Couldn't read batt status rc=%d\n",
						rc);

			wls_set = (pval.intval == POWER_SUPPLY_STATUS_FULL) ?
				MICRO_5V : wireless_vout;

			pval.intval = wls_set;
			rc = smblib_set_prop_voltage_wls_output(chg, &pval);
			if (rc < 0)
				dev_err(chg->dev, "Couldn't set dc voltage to 2*vph  rc=%d\n",
					rc);

			rc = smblib_select_sec_charger(chg,
					POWER_SUPPLY_CHARGER_SEC_CP,
					POWER_SUPPLY_CP_WIRELESS, false);
			if (rc < 0)
				dev_err(chg->dev, "Couldn't enable secondary chargers  rc=%d\n",
					rc);
		} else {
			/*
			 * If no secondary charger is present, commence
			 * wireless charging at 5 V by default.
			 */
			pval.intval = 5000000;
			rc = smblib_set_prop_voltage_wls_output(chg, &pval);
			if (rc < 0)
				dev_err(chg->dev, "Couldn't set dc voltage to 5 V rc=%d\n",
					rc);
		}

		schedule_work(&chg->dcin_aicl_work);
	} else {
		if (chg->cp_reason == POWER_SUPPLY_CP_WIRELESS) {
			sec_charger = chg->sec_pl_present ?
					POWER_SUPPLY_CHARGER_SEC_PL :
					POWER_SUPPLY_CHARGER_SEC_NONE;
			rc = smblib_select_sec_charger(chg, sec_charger,
					POWER_SUPPLY_CP_NONE, false);
			if (rc < 0)
				dev_err(chg->dev, "Couldn't disable secondary charger rc=%d\n",
					rc);
		}

		vote(chg->dc_suspend_votable, CHG_TERMINATION_VOTER, false, 0);
		vote(chg->fcc_main_votable, WLS_PL_CHARGING_VOTER, false, 0);

		chg->last_wls_vout = 0;
		chg->dcin_aicl_done = false;
		chg->dcin_icl_user_set = false;
	}

	/*
	 * Vote for 1500mA FCC upon WLS detach and remove vote upon attach if
	 * FCC stepper is enabled.
	 */
	if (chg->fcc_stepper_enable && !vbus_present)
		vote(chg->fcc_votable, FCC_STEPPER_VOTER, !dcin_present,
				dcin_present ? 0 : 1500000);

	if (chg->dc_psy)
		power_supply_changed(chg->dc_psy);

	smblib_dbg(chg, (PR_WLS | PR_INTERRUPT), "dcin_present= %d, usbin_present= %d, cp_reason = %d\n",
			dcin_present, vbus_present, chg->cp_reason);

	return IRQ_HANDLED;
}

irqreturn_t high_duty_cycle_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	chg->is_hdc = true;
	/*
	 * Disable usb IRQs after the flag set and re-enable IRQs after
	 * the flag cleared in the delayed work queue, to avoid any IRQ
	 * storming during the delays
	 */
	vote(chg->hdc_irq_disable_votable, HDC_IRQ_VOTER, true, 0);

	schedule_delayed_work(&chg->clear_hdc_work, msecs_to_jiffies(60));

	return IRQ_HANDLED;
}

static void smblib_bb_removal_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						bb_removal_work.work);

	vote(chg->usb_icl_votable, BOOST_BACK_VOTER, false, 0);
	vote(chg->awake_votable, BOOST_BACK_VOTER, false, 0);
}

#define BOOST_BACK_UNVOTE_DELAY_MS		750
#define BOOST_BACK_STORM_COUNT			3
#define WEAK_CHG_STORM_COUNT			8
irqreturn_t switcher_power_ok_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	struct storm_watch *wdata = &irq_data->storm_data;
	int rc, usb_icl;
	u8 stat;

	if (!(chg->wa_flags & BOOST_BACK_WA))
		return IRQ_HANDLED;

	rc = smblib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	/* skip suspending input if its already suspended by some other voter */
	usb_icl = get_effective_result(chg->usb_icl_votable);
	if ((stat & USE_USBIN_BIT) && usb_icl >= 0 && usb_icl <= USBIN_25MA)
		return IRQ_HANDLED;

	if (stat & USE_DCIN_BIT)
		return IRQ_HANDLED;

	if (is_storming(&irq_data->storm_data)) {
		/* This could be a weak charger reduce ICL */
		if (!is_client_vote_enabled(chg->usb_icl_votable,
						WEAK_CHARGER_VOTER)) {
			smblib_err(chg,
				"Weak charger detected: voting %dmA ICL\n",
				chg->weak_chg_icl_ua / 1000);
			vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
					true, chg->weak_chg_icl_ua);
			/*
			 * reset storm data and set the storm threshold
			 * to 3 for reverse boost detection.
			 */
			update_storm_count(wdata, BOOST_BACK_STORM_COUNT);
		} else {
			smblib_err(chg,
				"Reverse boost detected: voting 0mA to suspend input\n");
			vote(chg->usb_icl_votable, BOOST_BACK_VOTER, true, 0);
			vote(chg->awake_votable, BOOST_BACK_VOTER, true, 0);
			/*
			 * Remove the boost-back vote after a delay, to avoid
			 * permanently suspending the input if the boost-back
			 * condition is unintentionally hit.
			 */
			schedule_delayed_work(&chg->bb_removal_work,
				msecs_to_jiffies(BOOST_BACK_UNVOTE_DELAY_MS));
		}
	}

	return IRQ_HANDLED;
}

irqreturn_t wdog_snarl_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	if (chg->wa_flags & SW_THERM_REGULATION_WA) {
		cancel_delayed_work_sync(&chg->thermal_regulation_work);
		vote(chg->awake_votable, SW_THERM_REGULATION_VOTER, true, 0);
		schedule_delayed_work(&chg->thermal_regulation_work, 0);
	}

	power_supply_changed(chg->batt_psy);

	return IRQ_HANDLED;
}

irqreturn_t wdog_bark_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	rc = smblib_write(chg, BARK_BITE_WDOG_PET_REG, BARK_BITE_WDOG_PET_BIT);
	if (rc < 0)
		smblib_err(chg, "Couldn't pet the dog rc=%d\n", rc);

	if (chg->step_chg_enabled || chg->sw_jeita_enabled)
		power_supply_changed(chg->batt_psy);

	return IRQ_HANDLED;
}

static void smblib_die_rst_icl_regulate(struct smb_charger *chg)
{
	int rc;
	u8 temp;

	rc = smblib_read(chg, DIE_TEMP_STATUS_REG, &temp);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read DIE_TEMP_STATUS_REG rc=%d\n",
				rc);
		return;
	}

	/* Regulate ICL on die temp crossing DIE_RST threshold */
	vote(chg->usb_icl_votable, DIE_TEMP_VOTER,
				temp & DIE_TEMP_RST_BIT, 500000);
}

/*
 * triggered when DIE or SKIN or CONNECTOR temperature across
 * either of the _REG_L, _REG_H, _RST, or _SHDN thresholds
 */
irqreturn_t temp_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblib_die_rst_icl_regulate(chg);

	return IRQ_HANDLED;
}

static void smblib_usbov_dbc_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						usbov_dbc_work.work);

	smblib_dbg(chg, PR_MISC, "Resetting USBOV debounce\n");
	chg->dbc_usbov = false;
	vote(chg->awake_votable, USBOV_DBC_VOTER, false, 0);
}

static int smblib_get_step_vfloat_index(struct smb_charger *chg,
				int val)
{
	int i;

	/* select correct index by compare start_vbat and range vfloat threshold */
	for (i = 0; i <= ARRAY_SIZE(chg->six_pin_step_cfg) - 1; i++) {
		if (val < (chg->six_pin_step_cfg[i].vfloat_step_uv - VBAT_FOR_STEP_HYS_UV))
			break;
	}

	if (i >= ARRAY_SIZE(chg->six_pin_step_cfg) - 1)
		return ARRAY_SIZE(chg->six_pin_step_cfg) - 1;

	return i;
}

#define SECOND_FFC_TERM_TEMP 350
static void smblib_six_pin_batt_step_chg_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						six_pin_batt_step_chg_work.work);

	int rc = 0;
	int input_present;
	int main_charge_type;
	int interval_ms = STEP_CHG_DELAYED_MONITOR_MS;
	int fcc_ua = 0, fv_uv = 0, ibat_ua = 0, capacity = 0, health;
	int step_soc_th = 0;
	int ti_battery_voltage = 0, batt_temp = 0;
	union power_supply_propval pval = {0, };

	rc = smblib_is_input_present(chg, &input_present);
	if (rc < 0)
		return;

	if (chg->chg_enable_k11a) {
		if (!chg->cp_psy) {
			chg->cp_psy = power_supply_get_by_name("bq2597x-standalone");
			if (!chg->cp_psy)
				pr_err("cp_psy not found\n");
		}
		rc = power_supply_get_property(chg->cp_psy,
				POWER_SUPPLY_PROP_TI_BATTERY_VOLTAGE, &pval);
		if (rc < 0)
			pr_err("Error in getting TI_BATTERY_VOLTAGE, rc=%d\n", rc);
		ti_battery_voltage = pval.intval;

		rc = power_supply_get_property(chg->bms_psy,
					POWER_SUPPLY_PROP_TEMP, &pval);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get bms capacity:%d\n", rc);
		}
		batt_temp = pval.intval;
	}

	pr_info("input_present: %d\n", input_present);
	if (input_present == INPUT_NOT_PRESENT) {
		chg->init_start_vbat_checked = false;
		chg->trigger_taper_count = 0;
		chg->index_vfloat = 0;
		chg->flag_second_ffc_term_current = false;
		if (is_client_vote_enabled(chg->fv_votable,
						SIX_PIN_VFLOAT_VOTER))
			vote(chg->fv_votable, SIX_PIN_VFLOAT_VOTER, false, 0);
		if (is_client_vote_enabled(chg->fcc_votable,
						SIX_PIN_VFLOAT_VOTER))
			vote(chg->fcc_votable, SIX_PIN_VFLOAT_VOTER, false, 0);
		return;
	}

	if (chg->start_step_vbat >= VBAT_FOR_STEP_MIN_UV) {
		pr_err("start step vbat is too high, no need do step charge\n");
		return;
	}

	/* set init start vfloat according to chg->start_step_vbat */
	if (!chg->init_start_vbat_checked) {
		chg->index_vfloat =
				smblib_get_step_vfloat_index(chg, chg->start_step_vbat);
		vote(chg->fv_votable, SIX_PIN_VFLOAT_VOTER,
				true, chg->six_pin_step_cfg[chg->index_vfloat].vfloat_step_uv);
		vote(chg->fcc_votable, SIX_PIN_VFLOAT_VOTER,
				true, chg->six_pin_step_cfg[chg->index_vfloat].fcc_step_ua);
		chg->init_start_vbat_checked = true;
	}

	rc = smblib_get_prop_batt_charge_type(chg, &pval);
	if (rc < 0) {
		pr_err("Couldn't get batt charge type rc=%d\n", rc);
		return;
	}
	main_charge_type = pval.intval;
	pr_info("main_charge_type: %d\n", main_charge_type);

	/*
	 * Add capacity compare to optimize cool charge  switch to
	 * normal charge step too early issue, if capacity is below 40,
	 * do not switch to next step.
	 */
	rc = smblib_get_prop_batt_capacity(chg, &pval);
	if (rc < 0) {
		pr_err("Couldn't get batt charge type rc=%d\n", rc);
		return;
	}
	capacity = pval.intval;
	step_soc_th = chg->step_soc_threshold;
	pr_info("step_soc_threshold: %d\n", step_soc_th);

	rc = smblib_get_prop_batt_health(chg, &pval);
	if (rc < 0)
		smblib_err(chg, "Couldn't get batt health rc=%d\n", rc);

	health = pval.intval;
	if (health != POWER_SUPPLY_HEALTH_GOOD && !chg->health_not_good) {
		chg->health_not_good = true;
		vote(chg->fv_votable, SIX_PIN_VFLOAT_VOTER, false, 0);
	} else if (health == POWER_SUPPLY_HEALTH_GOOD
			&& chg->health_not_good) {
		chg->health_not_good = false;
		vote(chg->fv_votable, SIX_PIN_VFLOAT_VOTER,
					true, chg->six_pin_step_cfg[chg->index_vfloat].vfloat_step_uv);
	}

	if (chg->support_second_ffc_term_current) {
		rc = power_supply_get_property(chg->bms_psy,
				POWER_SUPPLY_PROP_TEMP, &pval);
		if (rc < 0)
			smblib_err(chg, "Couldn't get bms temp:%d\n", rc);

		if (main_charge_type == POWER_SUPPLY_CHARGE_TYPE_TAPER
				&& chg->flag_second_ffc_term_current == false
				&& pval.intval < chg->chg_warm_threshold
				&& pval.intval > SECOND_FFC_TERM_TEMP
				&& capacity == 100) {
			chg->flag_second_ffc_term_current = true;
			rc = power_supply_get_property(chg->bms_psy,
					POWER_SUPPLY_PROP_FFC_TERMINATION_CURRENT, &pval);
			if (!rc) {
				pval.intval -= chg->support_second_ffc_term_current_diff;
				smb5_config_iterm(chg, pval.intval, 50);
				smblib_dbg(chg, PR_MISC, "set second termi: %d\n",
						pval.intval);
			}
		}
	}

	if (main_charge_type == POWER_SUPPLY_CHARGE_TYPE_TAPER
			&& capacity > step_soc_th) {
		if (chg->chg_enable_k11a) {
			fcc_ua = get_effective_result(chg->fcc_votable)
								- TAPER_SLOWDECREASE_FCC_UA;
		} else {
			fcc_ua = get_effective_result(chg->fcc_votable)
								- TAPER_DECREASE_FCC_UA;
		}
		pr_err("taper from main charger, reducing FCC to %duA\n",
				fcc_ua);

		if (fcc_ua < MIN_TAPER_FCC_THR_UA)
			goto out;

		vote(chg->fcc_votable, SIX_PIN_VFLOAT_VOTER, true, fcc_ua);
	}

out:
	rc = smblib_get_batt_current_now(chg, &pval);
	if (rc < 0) {
		pr_err("Couldn't get ibat from bms rc=%d\n", rc);
		return;
	}

	ibat_ua = - pval.intval;
	pr_info("ibat_ua:%d, ti_battery_voltage:%d.\n",
			ibat_ua, ti_battery_voltage);

	if (main_charge_type == POWER_SUPPLY_CHARGE_TYPE_TAPER
			&& (capacity > step_soc_th)
			&& (chg->index_vfloat < (MAX_STEP_ENTRIES - 1))
			&& (ibat_ua <= (chg->six_pin_step_cfg[chg->index_vfloat + 1].fcc_step_ua
					+ TAPER_IBAT_TRH_HYS_UA)))
		chg->trigger_taper_count++;
	else
		chg->trigger_taper_count = 0;

	if (chg->trigger_taper_count >= MAX_COUNT_OF_IBAT_STEP) {
		chg->index_vfloat++;
		chg->trigger_taper_count = 0;
		if (chg->index_vfloat >= MAX_STEP_ENTRIES)
			chg->index_vfloat = MAX_STEP_ENTRIES - 1;
		if (chg->index_vfloat < MAX_STEP_ENTRIES) {
			vote(chg->fcc_votable, SIX_PIN_VFLOAT_VOTER,
					true, chg->six_pin_step_cfg[chg->index_vfloat].fcc_step_ua);
			vote(chg->fv_votable, SIX_PIN_VFLOAT_VOTER,
					true, chg->six_pin_step_cfg[chg->index_vfloat].vfloat_step_uv);
		}
	}

	fv_uv = get_effective_result(chg->fv_votable);
	fcc_ua = get_effective_result(chg->fcc_votable);

	if (chg->chg_enable_k11a) {
		pr_info("fv_uv:%d, fcc_ua:%d, index_vfloat:%d.\n", fv_uv, fcc_ua, chg->index_vfloat);
		if (batt_temp < chg->chg_cool_threshold
				&& ti_battery_voltage > (fv_uv / 1000)
				&& (fv_uv == 4450000)) {
			fv_uv -= 10000;
			vote(chg->fv_votable, NON_FFC_VFLOAT_VOTER, true, fv_uv);
			pr_err("temp:%d, ti_battery_voltage > NON_FFC_VFLOAT_VOTER, reduce uv:%d.\n", batt_temp, fv_uv);
		}

		if (chg->index_vfloat > 0) {
			if ((fv_uv == chg->six_pin_step_cfg[chg->index_vfloat].vfloat_step_uv)
					&& ti_battery_voltage > (fv_uv / 1000)) {
				fv_uv -= 10000;
				vote(chg->fv_votable, SIX_PIN_VFLOAT_VOTER, true, fv_uv);
				pr_err("ti_battery_voltage > vfloat_step_uv, reduce uv:%d.\n", fv_uv);
			}
			if ((fcc_ua == chg->six_pin_step_cfg[chg->index_vfloat].fcc_step_ua)
					&& ibat_ua > fcc_ua) {
				fcc_ua -= 150000;
				vote(chg->fcc_votable, SIX_PIN_VFLOAT_VOTER, true, fcc_ua);
				pr_err("ibat_ua > fcc_ua , reduce fcc:%d.\n", fcc_ua);
			}
		}
	}

	if (main_charge_type == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
		if (chg->chg_enable_k11a) {
			interval_ms = STEP_CHG_DELAYED_SLOWQUICK_MONITOR_MS;
		} else {
			interval_ms = STEP_CHG_DELAYED_QUICK_MONITOR_MS;
		}
	} else {
		interval_ms = STEP_CHG_DELAYED_MONITOR_MS;
	}

	schedule_delayed_work(&chg->six_pin_batt_step_chg_work,
				msecs_to_jiffies(interval_ms));
}

#define USB_OV_DBC_PERIOD_MS		1000
irqreturn_t usbin_ov_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	u8 stat;
	int rc;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	if (!(chg->wa_flags & USBIN_OV_WA))
		return IRQ_HANDLED;

	rc = smblib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USB_INT_RT_STS rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	/*
	 * On specific PMICs, OV IRQ triggers for very small duration in
	 * interim periods affecting charging status reflection. In order to
	 * differentiate between OV IRQ glitch and real OV_IRQ, add a debounce
	 * period for evaluation.
	 */
	if (stat & USBIN_OV_RT_STS_BIT) {
		chg->dbc_usbov = true;
		vote(chg->awake_votable, USBOV_DBC_VOTER, true, 0);
		schedule_delayed_work(&chg->usbov_dbc_work,
				msecs_to_jiffies(USB_OV_DBC_PERIOD_MS));
	} else {
		cancel_delayed_work_sync(&chg->usbov_dbc_work);
		chg->dbc_usbov = false;
		vote(chg->awake_votable, USBOV_DBC_VOTER, false, 0);
	}

	smblib_dbg(chg, PR_MISC, "USBOV debounce status %d\n",
				chg->dbc_usbov);
	return IRQ_HANDLED;
}

/**************
 * Additional USB PSY getters/setters
 * that call interrupt functions
 ***************/

int smblib_get_prop_pr_swap_in_progress(struct smb_charger *chg,
				union power_supply_propval *val)
{
	val->intval = chg->pr_swap_in_progress;
	return 0;
}

#define DETACH_DETECT_DELAY_MS 20
int smblib_set_prop_pr_swap_in_progress(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc;
	u8 stat = 0, orientation;
	int dc_power_on = 0;
	union power_supply_propval dc_val = {0,};

	smblib_dbg(chg, PR_MISC, "Requested PR_SWAP %d\n", val->intval);
	chg->pr_swap_in_progress = val->intval;
	if (chg->wireless_bq) {
		smblib_get_prop_dc_present(chg, &dc_val);
		dc_power_on = dc_val.intval;
	}

	/* check for cable removal during pr_swap */
	if (!chg->pr_swap_in_progress) {
		cancel_delayed_work_sync(&chg->pr_swap_detach_work);
		vote(chg->awake_votable, DETACH_DETECT_VOTER, true, 0);
		schedule_delayed_work(&chg->pr_swap_detach_work,
				msecs_to_jiffies(DETACH_DETECT_DELAY_MS));
	}

	rc = smblib_masked_write(chg, TYPE_C_DEBOUNCE_OPTION_REG,
			REDUCE_TCCDEBOUNCE_TO_2MS_BIT,
			val->intval ? REDUCE_TCCDEBOUNCE_TO_2MS_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't set tCC debounce rc=%d\n", rc);

	/* if wireless power on, do not set the bit. wireless will set later */
	if(!dc_power_on)
		rc = smblib_masked_write(chg, TYPE_C_EXIT_STATE_CFG_REG,
				BYPASS_VSAFE0V_DURING_ROLE_SWAP_BIT,
				val->intval ? BYPASS_VSAFE0V_DURING_ROLE_SWAP_BIT : 0);

	if (rc < 0)
		smblib_err(chg, "Couldn't set exit state cfg rc=%d\n", rc);

	if (chg->pr_swap_in_progress) {
		rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n",
				rc);
		}

		orientation =
			stat & CC_ORIENTATION_BIT ? TYPEC_CCOUT_VALUE_BIT : 0;
		rc = smblib_masked_write(chg, TYPE_C_CCOUT_CONTROL_REG,
			TYPEC_CCOUT_SRC_BIT | TYPEC_CCOUT_BUFFER_EN_BIT
					| TYPEC_CCOUT_VALUE_BIT,
			TYPEC_CCOUT_SRC_BIT | TYPEC_CCOUT_BUFFER_EN_BIT
					| orientation);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read TYPE_C_CCOUT_CONTROL_REG rc=%d\n",
				rc);
		}
	} else {
		rc = smblib_masked_write(chg, TYPE_C_CCOUT_CONTROL_REG,
			TYPEC_CCOUT_SRC_BIT, 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read TYPE_C_CCOUT_CONTROL_REG rc=%d\n",
				rc);
			return rc;
		}

		/* enable DRP */
		rc = smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
				 TYPEC_POWER_ROLE_CMD_MASK, 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't enable DRP rc=%d\n", rc);
			return rc;
		}
		chg->power_role = POWER_SUPPLY_TYPEC_PR_DUAL;
		smblib_dbg(chg, PR_MISC, "restore power role: %d\n",
				chg->power_role);
	}

	return 0;
}

/***************
 * Work Queues *
 ***************/
static void smblib_pr_lock_clear_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						pr_lock_clear_work.work);

	spin_lock(&chg->typec_pr_lock);
	if (chg->pr_lock_in_progress) {
		smblib_dbg(chg, PR_MISC, "restore type-c interrupts\n");
		smblib_typec_irq_config(chg, true);
		chg->pr_lock_in_progress = false;
	}
	spin_unlock(&chg->typec_pr_lock);
}

static void smblib_pr_swap_detach_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						pr_swap_detach_work.work);
	int rc;
	u8 stat;

	rc = smblib_read(chg, TYPE_C_STATE_MACHINE_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read STATE_MACHINE_STS rc=%d\n", rc);
		goto out;
	}
	smblib_dbg(chg, PR_REGISTER, "STATE_MACHINE_STS %x\n", stat);
	if (!(stat & TYPEC_ATTACH_DETACH_STATE_BIT)) {
		rc = smblib_request_dpdm(chg, false);
		if (rc < 0)
			smblib_err(chg, "Couldn't disable DPDM rc=%d\n", rc);

		if (chg->typec_port)
			typec_partner_unregister(chg);
	}
out:
	vote(chg->awake_votable, DETACH_DETECT_VOTER, false, 0);
}

static void smblib_uusb_otg_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						uusb_otg_work.work);
	int rc;
	u8 stat;
	bool otg;

	rc = smblib_read(chg, TYPEC_U_USB_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_3 rc=%d\n", rc);
		goto out;
	}
	otg = !!(stat & U_USB_GROUND_NOVBUS_BIT);
	if (chg->otg_present != otg)
		smblib_notify_usb_host(chg, otg);
	else
		goto out;

	chg->otg_present = otg;
	if (!otg)
		chg->boost_current_ua = 0;

	rc = smblib_set_charge_param(chg, &chg->param.freq_switcher,
				otg ? chg->chg_freq.freq_below_otg_threshold
					: chg->chg_freq.freq_removal);
	if (rc < 0)
		dev_err(chg->dev, "Error in setting freq_boost rc=%d\n", rc);

	smblib_dbg(chg, PR_REGISTER, "TYPE_C_U_USB_STATUS = 0x%02x OTG=%d\n",
			stat, otg);
	power_supply_changed(chg->usb_psy);

out:
	vote(chg->awake_votable, OTG_DELAY_VOTER, false, 0);
}

int smblib_set_prop_rechg_vbat_thresh(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc;
	int auto_recharge_vbat_mv = val->intval;

	rc = smblib_masked_write(chg, CHGR_CFG2_REG, RECHG_MASK,
				(auto_recharge_vbat_mv != -EINVAL) ?
				VBAT_BASED_RECHG_BIT : 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure VBAT-rechg CHG_CFG2_REG rc=%d\n",
			rc);
		return rc;
	}

	/* program the auto-recharge VBAT threshold */
	if (auto_recharge_vbat_mv != -EINVAL) {
		u32 temp = VBAT_TO_VRAW_ADC(auto_recharge_vbat_mv);

		temp = ((temp & 0xFF00) >> 8) | ((temp & 0xFF) << 8);
		rc = smblib_batch_write(chg,
			CHGR_ADC_RECHARGE_THRESHOLD_MSB_REG, (u8 *)&temp, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ADC_RECHARGE_THRESHOLD REG rc=%d\n",
				rc);
			return rc;
		}
		/* Program the sample count for VBAT based recharge to 3 */
		rc = smblib_masked_write(chg, CHGR_NO_SAMPLE_TERM_RCHG_CFG_REG,
					NO_OF_SAMPLE_FOR_RCHG,
					2 << NO_OF_SAMPLE_FOR_RCHG_SHIFT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure CHGR_NO_SAMPLE_FOR_TERM_RCHG_CFG rc=%d\n",
				rc);
			return rc;
		}
		chg->auto_recharge_vbat = auto_recharge_vbat_mv;
		smblib_dbg(chg, PR_OEM, "set chg->auto_recharge_vbat=%d\n", chg->auto_recharge_vbat);
	}

	return rc;
}

#define TEMP_COOL_RECHARGE_VBAT 4300
#define TEMP_WARM_RECHARGE_VBAT 3980
#define TEMP_NORM_RECHARGE_VBAT 4380

static int smblib_dynamic_recharge_vbat(struct smb_charger *chg)
{
	union power_supply_propval val;
	int rc, temp, recharge_vbat;
	static int last_recharge_vbat;

	if (chg->auto_recharge_vbat < 0)
		return 0;

	rc = smblib_get_prop_from_bms(chg,
			POWER_SUPPLY_PROP_TEMP, &val);
	if (rc < 0) {
		smblib_dbg(chg, PR_REGISTER, "Couldn't get debug battery prop rc=%d\n", rc);
		return -EINVAL;
	}
	temp = val.intval;

	if (temp <= BATT_COOL_THRESHOLD) {
		recharge_vbat = TEMP_COOL_RECHARGE_VBAT;
	}
	else if (temp >= BATT_WARM_THRESHOLD) {
		recharge_vbat = TEMP_WARM_RECHARGE_VBAT;
	} else {
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_RECHARGE_VBAT, &val);
		if (rc < 0) {
			smblib_dbg(chg, PR_REGISTER, "Couldn't get recharge vbat rc=%d\n", rc);
			recharge_vbat = TEMP_NORM_RECHARGE_VBAT;
		} else
			recharge_vbat = val.intval;
	}

	if (recharge_vbat != last_recharge_vbat) {
		val.intval = recharge_vbat;
		last_recharge_vbat = recharge_vbat;
	} else
		return 0;

	rc = power_supply_set_property(chg->batt_psy,
			POWER_SUPPLY_PROP_RECHARGE_VBAT,
			&val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set POWER_SUPPLY_PROP_CHARGER_TEMP_MAX rc=%d\n",
				rc);
		return -EINVAL;
	}

	return 0;
}


static void batt_update_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						batt_update_work);
	int rc;
	union power_supply_propval pval = {0,};

	if (!chg->batt_psy)
		return;

	rc = power_supply_get_property(chg->batt_psy,
			POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		pr_err("Couldn't get batt capacity status rc=%d\n", rc);
	}

	chg->capacity = pval.intval;
}

static void bms_update_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						bms_update_work);
	int bms_i2c_error_count;
	int ret;
	static int input_suspend = 0;
	union power_supply_propval val = {0,};


	smblib_suspend_on_debug_battery(chg);

	smblib_dynamic_recharge_vbat(chg);

	ret = power_supply_get_property(chg->bms_psy,
			POWER_SUPPLY_PROP_I2C_ERROR_COUNT, &val);
	if (!ret) {
			bms_i2c_error_count = val.intval;
	}

	if (bms_i2c_error_count && chg->batt_psy && !input_suspend) {
		input_suspend = 1;
		val.intval = 1;
		ret = power_supply_set_property(chg->batt_psy,POWER_SUPPLY_PROP_INPUT_SUSPEND,&val);
		if (ret < 0)
			smblib_err(chg,"Failed to set suspend\n");
	}else if(!bms_i2c_error_count && chg->batt_psy && input_suspend) {
		input_suspend = 0;
		val.intval = 0;
		ret = power_supply_set_property(chg->batt_psy,POWER_SUPPLY_PROP_INPUT_SUSPEND,&val);
		if (ret < 0)
			smblib_err(chg,"Failed to set suspend\n");
	}

	if (chg->batt_psy)
		power_supply_changed(chg->batt_psy);
}

static void pl_update_work(struct work_struct *work)
{
	union power_supply_propval prop_val;
	struct smb_charger *chg = container_of(work, struct smb_charger,
						pl_update_work);
	int rc;

	if (chg->smb_temp_max == -EINVAL) {
		rc = smblib_get_thermal_threshold(chg,
					SMB_REG_H_THRESHOLD_MSB_REG,
					&chg->smb_temp_max);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't get charger_temp_max rc=%d\n",
					rc);
			return;
		}
	}

	prop_val.intval = chg->smb_temp_max;
	rc = power_supply_set_property(chg->pl.psy,
				POWER_SUPPLY_PROP_CHARGER_TEMP_MAX,
				&prop_val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set POWER_SUPPLY_PROP_CHARGER_TEMP_MAX rc=%d\n",
				rc);
		return;
	}

	if (chg->sec_chg_selected == POWER_SUPPLY_CHARGER_SEC_CP)
		return;

	smblib_select_sec_charger(chg, POWER_SUPPLY_CHARGER_SEC_PL,
				POWER_SUPPLY_CP_NONE, false);
}

static void clear_hdc_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						clear_hdc_work.work);

	chg->is_hdc = false;
	vote(chg->hdc_irq_disable_votable, HDC_IRQ_VOTER, false, 0);
}

static void smblib_icl_change_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							icl_change_work.work);
	int rc, settled_ua;

	rc = smblib_get_charge_param(chg, &chg->param.icl_stat, &settled_ua);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get ICL status rc=%d\n", rc);
		return;
	}

	power_supply_changed(chg->usb_main_psy);

	smblib_dbg(chg, PR_INTERRUPT, "icl_settled=%d\n", settled_ua);
}

static void smblib_dc_power_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							dc_power_work.work);
	smblib_ncp3902_enable(chg, false);
}

static void smblib_pl_enable_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							pl_enable_work.work);

	smblib_dbg(chg, PR_PARALLEL, "timer expired, enabling parallel\n");
	vote(chg->pl_disable_votable, PL_DELAY_VOTER, false, 0);
	vote(chg->awake_votable, PL_DELAY_VOTER, false, 0);
}

static void smblib_thermal_regulation_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						thermal_regulation_work.work);
	int rc;

	rc = smblib_update_thermal_readings(chg);
	if (rc < 0)
		smblib_err(chg, "Couldn't read current thermal values %d\n",
					rc);

	rc = smblib_process_thermal_readings(chg);
	if (rc < 0)
		smblib_err(chg, "Couldn't run sw thermal regulation %d\n",
					rc);
}

#define MOISTURE_PROTECTION_CHECK_DELAY_MS 300000		/* 5 mins */
static void smblib_moisture_protection_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						moisture_protection_work);
	int rc;
	bool usb_plugged_in;
	u8 stat;

	/*
	 * Hold awake votable to prevent pm_relax being called prior to
	 * completion of this work.
	 */
	vote(chg->awake_votable, MOISTURE_VOTER, true, 0);

	/*
	 * Disable 1% duty cycle on CC_ID pin and enable uUSB factory mode
	 * detection to track any change on RID, as interrupts are disable.
	 */
	rc = smblib_write(chg, ((chg->chg_param.smb_version == PMI632_SUBTYPE) ?
			PMI632_TYPEC_U_USB_WATER_PROTECTION_CFG_REG :
			TYPEC_U_USB_WATER_PROTECTION_CFG_REG), 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't disable periodic monitoring of CC_ID rc=%d\n",
			rc);
		goto out;
	}

	rc = smblib_masked_write(chg, TYPEC_U_USB_CFG_REG,
					EN_MICRO_USB_FACTORY_MODE_BIT,
					EN_MICRO_USB_FACTORY_MODE_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't enable uUSB factory mode detection rc=%d\n",
			rc);
		goto out;
	}

	/*
	 * Add a delay of 100ms to allow change in rid to reflect on
	 * status registers.
	 */
	msleep(100);

	rc = smblib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USB_INT_RT_STS rc=%d\n", rc);
		goto out;
	}
	usb_plugged_in = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);

	/* Check uUSB status for moisture presence */
	rc = smblib_read(chg, TYPEC_U_USB_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_U_USB_STATUS_REG rc=%d\n",
				rc);
		goto out;
	}

	/*
	 * Factory mode detection happens in case of USB plugged-in by using
	 * a different current source of 2uA which can hamper moisture
	 * detection. Since factory mode is not supported in kernel, factory
	 * mode detection can be considered as equivalent to presence of
	 * moisture.
	 */
	if (stat == U_USB_STATUS_WATER_PRESENT || stat == U_USB_FMB1_BIT ||
			stat == U_USB_FMB2_BIT || (usb_plugged_in &&
			stat == U_USB_FLOAT1_BIT)) {
		smblib_set_moisture_protection(chg, true);
		alarm_start_relative(&chg->moisture_protection_alarm,
			ms_to_ktime(MOISTURE_PROTECTION_CHECK_DELAY_MS));
	} else {
		smblib_set_moisture_protection(chg, false);
		rc = alarm_cancel(&chg->moisture_protection_alarm);
		if (rc < 0)
			smblib_err(chg, "Couldn't cancel moisture protection alarm\n");
	}

out:
	vote(chg->awake_votable, MOISTURE_VOTER, false, 0);
}

static enum alarmtimer_restart moisture_protection_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct smb_charger *chg = container_of(alarm, struct smb_charger,
					moisture_protection_alarm);

	smblib_dbg(chg, PR_MISC, "moisture Protection Alarm Triggered %lld\n",
			ktime_to_ms(now));

	/* Atomic context, cannot use voter */
	pm_stay_awake(chg->dev);
	schedule_work(&chg->moisture_protection_work);

	return ALARMTIMER_NORESTART;
}

static void smblib_chg_termination_work(struct work_struct *work)
{
	union power_supply_propval pval;
	enum power_supply_property prop;
	struct smb_charger *chg = container_of(work, struct smb_charger,
						chg_termination_work);
	int rc, input_present, delay = CHG_TERM_WA_ENTRY_DELAY_MS;
	int vbat_now_uv, max_fv_uv, capacity_raw;

	/*
	 * Hold awake votable to prevent pm_relax being called prior to
	 * completion of this work.
	 */
	vote(chg->awake_votable, CHG_TERMINATION_VOTER, true, 0);

	rc = smblib_is_input_present(chg, &input_present);
	if ((rc < 0) || !input_present)
		goto out;

	if (chg->ext_fg) {
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CAPACITY_RAW, &pval);
		pval.intval = pval.intval / 100;
		capacity_raw = pval.intval;
	} else {
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_REAL_CAPACITY, &pval);
	}

	if ((rc < 0) || (pval.intval < 100)) {
		vote(chg->usb_icl_votable, CHG_TERMINATION_VOTER, false, 0);
		vote(chg->dc_suspend_votable, CHG_TERMINATION_VOTER, false, 0);
		goto out;
	}
	if (chg->ext_fg) {
		max_fv_uv = get_effective_result(chg->fv_votable);
	} else {
		/* Get the battery float voltage */
		rc = smblib_get_prop_from_bms(chg, POWER_SUPPLY_PROP_VOLTAGE_MAX,
					&pval);
		if (rc < 0)
			goto out;

		max_fv_uv = pval.intval;
	}

	/* Get the battery float voltage */
	rc = smblib_get_prop_from_bms(chg, POWER_SUPPLY_PROP_VOLTAGE_MAX,
				&pval);
	if (rc < 0)
		goto out;

	max_fv_uv = pval.intval;

	rc = smblib_get_prop_from_bms(chg, POWER_SUPPLY_PROP_CHARGE_FULL,
					&pval);
	if (rc < 0)
		goto out;

	if (chg->ext_fg)
		prop = POWER_SUPPLY_PROP_CAPACITY_RAW;
	else
		prop = POWER_SUPPLY_PROP_CC_SOC;
	/*
	 * On change in the value of learned capacity, re-initialize the
	 * reference cc_soc value due to change in cc_soc characteristic value
	 * at full capacity. Also, in case cc_soc_ref value is reset,
	 * re-initialize it.
	 */
	if (pval.intval != chg->charge_full_cc || !chg->cc_soc_ref) {
		chg->charge_full_cc = pval.intval;

		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0)
			goto out;

		/*
		 * Store the Vbat at the charge termination to compare with
		 * the current voltage to see if the Vbat is increasing after
		 * charge termination in BSM.
		 */
		chg->term_vbat_uv = pval.intval;
		vbat_now_uv = pval.intval;

		rc = smblib_get_prop_from_bms(chg, POWER_SUPPLY_PROP_CC_SOC,
					&pval);
		if (rc < 0)
			goto out;

		chg->cc_soc_ref = pval.intval;
	} else {
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0)
			goto out;

		vbat_now_uv = pval.intval;

		rc = smblib_get_prop_from_bms(chg, POWER_SUPPLY_PROP_CC_SOC,
					&pval);
		if (rc < 0)
			goto out;

		vbat_now_uv = pval.intval;
		rc = smblib_get_prop_from_bms(chg, prop, &pval);
		if (rc < 0)
			goto out;
	}
	/*
	 * In BSM a sudden jump in CC_SOC is not expected. If seen, its a
	 * good_ocv or updated capacity, reject it.
	 */
	if (chg->last_cc_soc && pval.intval > (chg->last_cc_soc + 100)) {
		/* CC_SOC has increased by 1% from last time */
		chg->cc_soc_ref = pval.intval;
		smblib_dbg(chg, PR_MISC, "cc_soc jumped(%d->%d), reset cc_soc_ref\n",
				chg->last_cc_soc, pval.intval);
	}
	chg->last_cc_soc = pval.intval;

	/*
	 * Suspend/Unsuspend USB input to keep cc_soc within the 0.5% to 0.75%
	 * overshoot range of the cc_soc value at termination and make sure that
	 * vbat is indeed rising above vfloat.
	 */
	if (chg->ext_fg) {
		if (capacity_raw < 100) {
			vote(chg->usb_icl_votable, CHG_TERMINATION_VOTER, false, 0);
			vote(chg->dc_suspend_votable, CHG_TERMINATION_VOTER, false, 0);
			delay = CHG_TERM_WA_ENTRY_DELAY_MS;
		} else if (((vbat_now_uv > chg->term_vbat_uv) && (vbat_now_uv > max_fv_uv))) {
			if (input_present & INPUT_PRESENT_USB)
				vote(chg->usb_icl_votable, CHG_TERMINATION_VOTER,
						true, 0);
			if (input_present & INPUT_PRESENT_DC)
				vote(chg->dc_suspend_votable, CHG_TERMINATION_VOTER,
						true, 0);
			delay = CHG_TERM_WA_EXIT_DELAY_MS;
		}
	} else {
		if (pval.intval < DIV_ROUND_CLOSEST(chg->cc_soc_ref * 10050, 10000)) {
			vote(chg->usb_icl_votable, CHG_TERMINATION_VOTER, false, 0);
			vote(chg->dc_suspend_votable, CHG_TERMINATION_VOTER, false, 0);
			delay = CHG_TERM_WA_ENTRY_DELAY_MS;
		} else if ((pval.intval > DIV_ROUND_CLOSEST(chg->cc_soc_ref * 10075, 10000))
			  && ((vbat_now_uv > chg->term_vbat_uv) && (vbat_now_uv > max_fv_uv))) {
			if (input_present & INPUT_PRESENT_USB)
				vote(chg->usb_icl_votable, CHG_TERMINATION_VOTER,
						true, 0);
			if (input_present & INPUT_PRESENT_DC)
				vote(chg->dc_suspend_votable, CHG_TERMINATION_VOTER,
						true, 0);
			delay = CHG_TERM_WA_EXIT_DELAY_MS;
		}
	}

	smblib_err(chg, "term-current Chg Term WA readings: cc_soc: %d, cc_soc_ref: %d, delay: %d vbat_now %d term_vbat %d\n",
			pval.intval, chg->cc_soc_ref, delay, vbat_now_uv, chg->term_vbat_uv);

	alarm_start_relative(&chg->chg_termination_alarm, ms_to_ktime(delay));
out:
	vote(chg->awake_votable, CHG_TERMINATION_VOTER, false, 0);
	vote(chg->usb_icl_votable, CHG_TERMINATION_VOTER, false, 0);
}

static enum alarmtimer_restart chg_termination_alarm_cb(struct alarm *alarm,
								ktime_t now)
{
	struct smb_charger *chg = container_of(alarm, struct smb_charger,
							chg_termination_alarm);

	smblib_err(chg, "term-current Charge termination WA alarm triggered %lld\n",
			ktime_to_ms(now));

	/* Atomic context, cannot use voter */
	pm_stay_awake(chg->dev);
	schedule_work(&chg->chg_termination_work);

	return ALARMTIMER_NORESTART;
}

static void apsd_timer_cb(struct timer_list *tm)
{
	struct smb_charger *chg = container_of(tm, struct smb_charger,
							apsd_timer);

	smblib_dbg(chg, PR_MISC, "APSD Extented timer timeout at %lld\n",
			jiffies_to_msecs(jiffies));

	chg->apsd_ext_timeout = true;
}

#define SOFT_JEITA_HYSTERESIS_OFFSET	0x200
static void jeita_update_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						jeita_update_work);
	struct device_node *node = chg->dev->of_node;
	struct device_node *batt_node, *pnode;
	union power_supply_propval val;
	int rc, tmp[2], max_fcc_ma, max_fv_uv;
	u32 jeita_hard_thresholds[2];
	u16 addr;
	u8 buff[2];

	batt_node = of_find_node_by_name(node, "qcom,battery-data");
	if (!batt_node) {
		smblib_err(chg, "Batterydata not available\n");
		goto out;
	}

	/* if BMS is not ready, defer the work */
	if (!chg->bms_psy)
		return;

	rc = smblib_get_prop_from_bms(chg,
			POWER_SUPPLY_PROP_RESISTANCE_ID, &val);
	if (rc < 0) {
		smblib_err(chg, "Failed to get batt-id rc=%d\n", rc);
		goto out;
	}

	/* if BMS hasn't read out the batt_id yet, defer the work */
	if (val.intval <= 0)
		return;

	pnode = of_batterydata_get_best_profile(batt_node,
					val.intval / 1000, NULL);
	if (IS_ERR(pnode)) {
		rc = PTR_ERR(pnode);
		smblib_err(chg, "Failed to detect valid battery profile %d\n",
				rc);
		goto out;
	}

	rc = of_property_read_u32_array(pnode, "qcom,jeita-hard-thresholds",
				jeita_hard_thresholds, 2);
	if (!rc) {
		rc = smblib_update_jeita(chg, jeita_hard_thresholds,
					JEITA_HARD);
		if (rc < 0) {
			smblib_err(chg, "Couldn't configure Hard Jeita rc=%d\n",
					rc);
			goto out;
		}
	}

	rc = of_property_read_u32_array(pnode, "qcom,jeita-soft-thresholds",
				chg->jeita_soft_thlds, 2);
	if (!rc) {
		rc = smblib_update_jeita(chg, chg->jeita_soft_thlds,
					JEITA_SOFT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't configure Soft Jeita rc=%d\n",
					rc);
			goto out;
		}

		rc = of_property_read_u32_array(pnode,
					"qcom,jeita-soft-hys-thresholds",
					chg->jeita_soft_hys_thlds, 2);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get Soft Jeita hysteresis thresholds rc=%d\n",
					rc);
			goto out;
		}
	} else {
		/* Populate the jeita-soft-thresholds */
		addr = CHGR_JEITA_THRESHOLD_BASE_REG(JEITA_SOFT);
		rc = smblib_batch_read(chg, addr, buff, 2);
		if (rc < 0) {
			pr_err("failed to read 0x%4X, rc=%d\n", addr, rc);
			goto out;
		}

		chg->jeita_soft_thlds[1] = buff[1] | buff[0] << 8;

		rc = smblib_batch_read(chg, addr + 2, buff, 2);
		if (rc < 0) {
			pr_err("failed to read 0x%4X, rc=%d\n", addr + 2, rc);
			goto out;
		}

		chg->jeita_soft_thlds[0] = buff[1] | buff[0] << 8;

		/*
		 * Update the soft jeita hysteresis 2 DegC less for warm and
		 * 2 DegC more for cool than the soft jeita thresholds to avoid
		 * overwriting the registers with invalid values.
		 */
		chg->jeita_soft_hys_thlds[0] =
			chg->jeita_soft_thlds[0] - SOFT_JEITA_HYSTERESIS_OFFSET;
		chg->jeita_soft_hys_thlds[1] =
			chg->jeita_soft_thlds[1] + SOFT_JEITA_HYSTERESIS_OFFSET;
	}

	chg->jeita_soft_fcc[0] = chg->jeita_soft_fcc[1] = -EINVAL;
	chg->jeita_soft_fv[0] = chg->jeita_soft_fv[1] = -EINVAL;
	max_fcc_ma = max_fv_uv = -EINVAL;

	of_property_read_u32(pnode, "qcom,fastchg-current-ma", &max_fcc_ma);
	of_property_read_u32(pnode, "qcom,max-voltage-uv", &max_fv_uv);

	if (max_fcc_ma <= 0 || max_fv_uv <= 0) {
		smblib_err(chg, "Incorrect fastchg-current-ma or max-voltage-uv\n");
		goto out;
	}

	rc = of_property_read_u32_array(pnode, "qcom,jeita-soft-fcc-ua",
					tmp, 2);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get fcc values for soft JEITA rc=%d\n",
				rc);
		goto out;
	}

	max_fcc_ma *= 1000;
	if (tmp[0] > max_fcc_ma || tmp[1] > max_fcc_ma) {
		smblib_err(chg, "Incorrect FCC value [%d %d] max: %d\n", tmp[0],
			tmp[1], max_fcc_ma);
		goto out;
	}
	chg->jeita_soft_fcc[0] = tmp[0];
	chg->jeita_soft_fcc[1] = tmp[1];

	rc = of_property_read_u32_array(pnode, "qcom,jeita-soft-fv-uv", tmp,
					2);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get fv values for soft JEITA rc=%d\n",
				rc);
		goto out;
	}

	if (tmp[0] > max_fv_uv || tmp[1] > max_fv_uv) {
		smblib_err(chg, "Incorrect FV value [%d %d] max: %d\n", tmp[0],
			tmp[1], max_fv_uv);
		goto out;
	}
	chg->jeita_soft_fv[0] = tmp[0];
	chg->jeita_soft_fv[1] = tmp[1];

	rc = smblib_soft_jeita_arb_wa(chg);
	if (rc < 0) {
		smblib_err(chg, "Couldn't fix soft jeita arb rc=%d\n",
				rc);
		goto out;
	}

	chg->jeita_configured = JEITA_CFG_COMPLETE;
	return;

out:
	chg->jeita_configured = JEITA_CFG_FAILURE;
}

static void smblib_lpd_ra_open_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							lpd_ra_open_work.work);
	union power_supply_propval pval;
	u8 stat;
	int rc;

	if (chg->pr_swap_in_progress || chg->pd_hard_reset) {
		chg->lpd_stage = LPD_STAGE_NONE;
		goto out;
	}

	if (chg->lpd_stage != LPD_STAGE_FLOAT)
		goto out;

	rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_MISC_STATUS_REG rc=%d\n",
			rc);
		goto out;
	}

	/* quit if moisture status is gone or in attached state */
	if (!(stat & TYPEC_WATER_DETECTION_STATUS_BIT)
			|| (stat & TYPEC_TCCDEBOUNCE_DONE_STATUS_BIT)) {
		chg->lpd_stage = LPD_STAGE_NONE;
		goto out;
	}

	chg->lpd_stage = LPD_STAGE_COMMIT;

	/* Enable source only mode */
	pval.intval = POWER_SUPPLY_TYPEC_PR_SOURCE;
	rc = smblib_set_prop_typec_power_role(chg, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set typec source only mode rc=%d\n",
					rc);
		goto out;
	}

	/* Wait 1.5ms to get SBUx ready */
	usleep_range(1500, 1510);

	if (smblib_rsbux_low(chg, RSBU_K_300K_UV)) {
		/* Moisture detected, enable sink only mode */
		pval.intval = POWER_SUPPLY_TYPEC_PR_SINK;
		rc = smblib_set_prop_typec_power_role(chg, &pval);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set typec sink only rc=%d\n",
				rc);
			goto out;
		}

		chg->lpd_reason = LPD_MOISTURE_DETECTED;
		chg->moisture_present =  true;
		vote(chg->usb_icl_votable, LPD_VOTER, true, 0);

	} else {
		/* Floating cable, disable water detection irq temporarily */
		rc = smblib_masked_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG,
					TYPEC_WATER_DETECTION_INT_EN_BIT, 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set TYPE_C_INTERRUPT_EN_CFG_2_REG rc=%d\n",
					rc);
			goto out;
		}

		/* restore DRP mode */
		pval.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
		rc = smblib_set_prop_typec_power_role(chg, &pval);
		if (rc < 0) {
			smblib_err(chg, "Couldn't write 0x%02x to TYPE_C_INTRPT_ENB_SOFTWARE_CTRL rc=%d\n",
				pval.intval, rc);
			goto out;
		}

		chg->lpd_reason = LPD_FLOATING_CABLE;
	}

	/* recheck in 60 seconds */
	alarm_start_relative(&chg->lpd_recheck_timer, ms_to_ktime(60000));
out:
	vote(chg->awake_votable, LPD_VOTER, false, 0);
}

static void smblib_lpd_detach_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							lpd_detach_work.work);

	if (chg->lpd_stage == LPD_STAGE_FLOAT_CANCEL)
		chg->lpd_stage = LPD_STAGE_NONE;
}

static void smblib_cp_status_change_work(struct work_struct *work)
{
	int rc;
	union power_supply_propval pval;
	struct smb_charger *chg = container_of(work, struct smb_charger,
			cp_status_change_work);

	if (!chg->cp_psy)
		chg->cp_psy = power_supply_get_by_name("charge_pump_master");

	if (!chg->cp_psy)
		goto relax;

	if (chg->cp_topo == -EINVAL) {
		rc = power_supply_get_property(chg->cp_psy,
				POWER_SUPPLY_PROP_PARALLEL_OUTPUT_MODE, &pval);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read cp topo rc=%d\n", rc);
			goto relax;
		}

		chg->cp_topo = pval.intval;

		if (chg->cp_topo == POWER_SUPPLY_PL_OUTPUT_VBAT &&
				chg->cp_reason == POWER_SUPPLY_CP_WIRELESS)
			vote(chg->fcc_main_votable, WLS_PL_CHARGING_VOTER, true,
					800000);
	}
relax:
	pm_relax(chg->dev);
}

static void smblib_batt_verify_update_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
			batt_verify_update_work);

	if (chg->batt_verified)
		vote(chg->fcc_votable, BATT_VERIFY_VOTER, false, 0);
}

static int smblib_create_votables(struct smb_charger *chg)
{
	int rc = 0;

	chg->fcc_votable = find_votable("FCC");
	if (chg->fcc_votable == NULL) {
		rc = -EINVAL;
		smblib_err(chg, "Couldn't find FCC votable rc=%d\n", rc);
		return rc;
	}

	chg->fcc_main_votable = find_votable("FCC_MAIN");
	if (chg->fcc_main_votable == NULL) {
		rc = -EINVAL;
		smblib_err(chg, "Couldn't find FCC Main votable rc=%d\n", rc);
		return rc;
	}

	chg->fv_votable = find_votable("FV");
	if (chg->fv_votable == NULL) {
		rc = -EINVAL;
		smblib_err(chg, "Couldn't find FV votable rc=%d\n", rc);
		return rc;
	}

	chg->usb_icl_votable = find_votable("USB_ICL");
	if (chg->usb_icl_votable == NULL) {
		rc = -EINVAL;
		smblib_err(chg, "Couldn't find USB_ICL votable rc=%d\n", rc);
		return rc;
	}

	chg->pl_disable_votable = find_votable("PL_DISABLE");
	if (chg->pl_disable_votable == NULL) {
		rc = -EINVAL;
		smblib_err(chg, "Couldn't find votable PL_DISABLE rc=%d\n", rc);
		return rc;
	}

	chg->pl_enable_votable_indirect = find_votable("PL_ENABLE_INDIRECT");
	if (chg->pl_enable_votable_indirect == NULL) {
		rc = -EINVAL;
		smblib_err(chg,
			"Couldn't find votable PL_ENABLE_INDIRECT rc=%d\n",
			rc);
		return rc;
	}

	vote(chg->pl_disable_votable, PL_DELAY_VOTER, true, 0);

	chg->smb_override_votable = create_votable("SMB_EN_OVERRIDE",
				VOTE_SET_ANY,
				smblib_smb_disable_override_vote_callback, chg);
	if (IS_ERR(chg->smb_override_votable)) {
		rc = PTR_ERR(chg->smb_override_votable);
		chg->smb_override_votable = NULL;
		return rc;
	}

	chg->dc_suspend_votable = create_votable("DC_SUSPEND", VOTE_SET_ANY,
					smblib_dc_suspend_vote_callback,
					chg);
	if (IS_ERR(chg->dc_suspend_votable)) {
		rc = PTR_ERR(chg->dc_suspend_votable);
		chg->dc_suspend_votable = NULL;
		return rc;
	}

	chg->awake_votable = create_votable("AWAKE", VOTE_SET_ANY,
					smblib_awake_vote_callback,
					chg);
	if (IS_ERR(chg->awake_votable)) {
		rc = PTR_ERR(chg->awake_votable);
		chg->awake_votable = NULL;
		return rc;
	}

	chg->chg_disable_votable = create_votable("CHG_DISABLE", VOTE_SET_ANY,
					smblib_chg_disable_vote_callback,
					chg);
	if (IS_ERR(chg->chg_disable_votable)) {
		rc = PTR_ERR(chg->chg_disable_votable);
		chg->chg_disable_votable = NULL;
		return rc;
	}

	chg->limited_irq_disable_votable = create_votable(
				"USB_LIMITED_IRQ_DISABLE",
				VOTE_SET_ANY,
				smblib_limited_irq_disable_vote_callback,
				chg);
	if (IS_ERR(chg->limited_irq_disable_votable)) {
		rc = PTR_ERR(chg->limited_irq_disable_votable);
		chg->limited_irq_disable_votable = NULL;
		return rc;
	}

	chg->hdc_irq_disable_votable = create_votable("USB_HDC_IRQ_DISABLE",
					VOTE_SET_ANY,
					smblib_hdc_irq_disable_vote_callback,
					chg);
	if (IS_ERR(chg->hdc_irq_disable_votable)) {
		rc = PTR_ERR(chg->hdc_irq_disable_votable);
		chg->hdc_irq_disable_votable = NULL;
		return rc;
	}

	chg->icl_irq_disable_votable = create_votable("USB_ICL_IRQ_DISABLE",
					VOTE_SET_ANY,
					smblib_icl_irq_disable_vote_callback,
					chg);
	if (IS_ERR(chg->icl_irq_disable_votable)) {
		rc = PTR_ERR(chg->icl_irq_disable_votable);
		chg->icl_irq_disable_votable = NULL;
		return rc;
	}

	chg->temp_change_irq_disable_votable = create_votable(
			"TEMP_CHANGE_IRQ_DISABLE", VOTE_SET_ANY,
			smblib_temp_change_irq_disable_vote_callback, chg);
	if (IS_ERR(chg->temp_change_irq_disable_votable)) {
		rc = PTR_ERR(chg->temp_change_irq_disable_votable);
		chg->temp_change_irq_disable_votable = NULL;
		return rc;
	}

	return rc;
}

static void smblib_destroy_votables(struct smb_charger *chg)
{
	if (chg->dc_suspend_votable)
		destroy_votable(chg->dc_suspend_votable);
	if (chg->usb_icl_votable)
		destroy_votable(chg->usb_icl_votable);
	if (chg->awake_votable)
		destroy_votable(chg->awake_votable);
	if (chg->chg_disable_votable)
		destroy_votable(chg->chg_disable_votable);
}

static void smblib_iio_deinit(struct smb_charger *chg)
{
	if (!IS_ERR_OR_NULL(chg->iio.usbin_v_chan))
		iio_channel_release(chg->iio.usbin_v_chan);
	if (!IS_ERR_OR_NULL(chg->iio.usbin_i_chan))
		iio_channel_release(chg->iio.usbin_i_chan);
	if (!IS_ERR_OR_NULL(chg->iio.temp_chan))
		iio_channel_release(chg->iio.temp_chan);
	if (!IS_ERR_OR_NULL(chg->iio.sbux_chan))
		iio_channel_release(chg->iio.sbux_chan);
	if (!IS_ERR_OR_NULL(chg->iio.vph_v_chan))
		iio_channel_release(chg->iio.vph_v_chan);
	if (!IS_ERR_OR_NULL(chg->iio.die_temp_chan))
		iio_channel_release(chg->iio.die_temp_chan);
	if (!IS_ERR_OR_NULL(chg->iio.connector_temp_chan))
		iio_channel_release(chg->iio.connector_temp_chan);
	if (!IS_ERR_OR_NULL(chg->iio.skin_temp_chan))
		iio_channel_release(chg->iio.skin_temp_chan);
	if (!IS_ERR_OR_NULL(chg->iio.smb_temp_chan))
		iio_channel_release(chg->iio.smb_temp_chan);
}

int smblib_init(struct smb_charger *chg)
{
	union power_supply_propval prop_val;
	int rc = 0;

	mutex_init(&chg->smb_lock);
	mutex_init(&chg->irq_status_lock);
	mutex_init(&chg->dcin_aicl_lock);
	mutex_init(&chg->dpdm_lock);
	spin_lock_init(&chg->typec_pr_lock);
	INIT_WORK(&chg->batt_update_work, batt_update_work);
	INIT_WORK(&chg->bms_update_work, bms_update_work);
	INIT_WORK(&chg->pl_update_work, pl_update_work);
	INIT_WORK(&chg->jeita_update_work, jeita_update_work);
	INIT_WORK(&chg->dcin_aicl_work, dcin_aicl_work);
	INIT_WORK(&chg->cp_status_change_work, smblib_cp_status_change_work);
	INIT_WORK(&chg->batt_verify_update_work, smblib_batt_verify_update_work);
	INIT_WORK(&chg->plugin_check_time_work, smblib_plugin_check_time_work);
	INIT_DELAYED_WORK(&chg->fake_plug_out_check_work, smblib_fake_plug_out_check_work);
	INIT_DELAYED_WORK(&chg->clear_hdc_work, clear_hdc_work);
	INIT_DELAYED_WORK(&chg->icl_change_work, smblib_icl_change_work);
	INIT_DELAYED_WORK(&chg->dc_power_work, smblib_dc_power_work);
	INIT_DELAYED_WORK(&chg->pl_enable_work, smblib_pl_enable_work);
	INIT_DELAYED_WORK(&chg->uusb_otg_work, smblib_uusb_otg_work);
	INIT_DELAYED_WORK(&chg->bb_removal_work, smblib_bb_removal_work);
	INIT_DELAYED_WORK(&chg->lpd_ra_open_work, smblib_lpd_ra_open_work);
	INIT_DELAYED_WORK(&chg->lpd_detach_work, smblib_lpd_detach_work);
	INIT_DELAYED_WORK(&chg->thermal_regulation_work,
					smblib_thermal_regulation_work);
	INIT_DELAYED_WORK(&chg->usbov_dbc_work, smblib_usbov_dbc_work);
	INIT_DELAYED_WORK(&chg->six_pin_batt_step_chg_work,
					smblib_six_pin_batt_step_chg_work);
	INIT_DELAYED_WORK(&chg->pr_swap_detach_work,
					smblib_pr_swap_detach_work);
	timer_setup(&chg->apsd_timer, apsd_timer_cb, 0);

	INIT_DELAYED_WORK(&chg->role_reversal_check,
					smblib_typec_role_check_work);
	INIT_DELAYED_WORK(&chg->reg_work, smblib_reg_work);
	INIT_DELAYED_WORK(&chg->thermal_setting_work, smblib_thermal_setting_work);
#if (!defined CONFIG_FUEL_GAUGE_BQ27Z561_MUNCH) && (!defined CONFIG_DUAL_FUEL_GAUGE_BQ27Z561)
	INIT_DELAYED_WORK(&chg->reduce_fcc_work, reduce_fcc_work);
#endif
	INIT_DELAYED_WORK(&chg->charger_type_recheck, smblib_charger_type_recheck);
	INIT_DELAYED_WORK(&chg->raise_qc3_vbus_work, smblib_raise_qc3_vbus_work);
	INIT_DELAYED_WORK(&chg->pr_lock_clear_work, smblib_pr_lock_clear_work);
	INIT_DELAYED_WORK(&chg->conn_therm_work, smblib_conn_therm_work);
	INIT_DELAYED_WORK(&chg->after_ffc_chg_dis_work, smblib_after_ffc_chg_dis_work);
	INIT_DELAYED_WORK(&chg->after_ffc_chg_en_work, smblib_after_ffc_chg_en_work);
	INIT_DELAYED_WORK(&chg->dc_plug_out_delay_work, smblib_dc_plug_out_work);
	INIT_DELAYED_WORK(&chg->wireless_full_delay_work, smblib_wireless_delay_work);
	INIT_DELAYED_WORK(&chg->report_soc_decimal_work, smblib_report_soc_decimal_work);
	INIT_DELAYED_WORK(&chg->step_charge_notify_work, smblib_step_charge_notify_work);
	INIT_DELAYED_WORK(&chg->cc_un_compliant_charge_work, smblib_cc_un_compliant_charge_work);
	INIT_DELAYED_WORK(&chg->clean_cp_to_sw_work, smblib_clean_cp_to_sw_work);
	INIT_DELAYED_WORK(&chg->check_init_boot, smb_check_init_boot);

	if (chg->wa_flags & CHG_TERMINATION_WA) {
		INIT_WORK(&chg->chg_termination_work,
					smblib_chg_termination_work);

		if (alarmtimer_get_rtcdev()) {
			alarm_init(&chg->chg_termination_alarm, ALARM_BOOTTIME,
						chg_termination_alarm_cb);
		} else {
			smblib_err(chg, "Couldn't get rtc device\n");
			return -ENODEV;
		}
	}

	if (chg->uusb_moisture_protection_enabled) {
		INIT_WORK(&chg->moisture_protection_work,
					smblib_moisture_protection_work);

		if (alarmtimer_get_rtcdev()) {
			alarm_init(&chg->moisture_protection_alarm,
				ALARM_BOOTTIME, moisture_protection_alarm_cb);
		} else {
			smblib_err(chg, "Failed to initialize moisture protection alarm\n");
			return -ENODEV;
		}
	}

	if (alarmtimer_get_rtcdev()) {
		alarm_init(&chg->dcin_aicl_alarm, ALARM_REALTIME,
				dcin_aicl_alarm_cb);
	} else {
		smblib_err(chg, "Failed to initialize dcin aicl alarm\n");
		return -ENODEV;
	}

	chg->fake_capacity = -EINVAL;
	chg->fake_input_current_limited = -EINVAL;
	chg->fake_batt_status = -EINVAL;
	chg->sink_src_mode = UNATTACHED_MODE;
	/*disable qcom default battery profile soft-jeita, use step chg jeita*/
	chg->jeita_configured = JEITA_CFG_FAILURE;
	chg->sec_chg_selected = POWER_SUPPLY_CHARGER_SEC_NONE;
	chg->cp_reason = POWER_SUPPLY_CP_NONE;
	chg->thermal_status = TEMP_BELOW_RANGE;
	chg->pps_thermal_level = -EINVAL;
#if (!defined CONFIG_FUEL_GAUGE_BQ27Z561_MUNCH) && (!defined CONFIG_DUAL_FUEL_GAUGE_BQ27Z561)
	chg->esr_work_status = ESR_CHECK_FCC_NOLIMIT;
#endif
	chg->typec_irq_en = true;
	chg->hvdcp_recheck_status = false;
	chg->batt_temp_irq_enabled = false;
	chg->no_raise_vbus_status = false;
	chg->fake_plug_out = false;
	chg->cp_to_sw_status = false;
	chg->cp_topo = -EINVAL;
	chg->dr_mode = TYPEC_PORT_DRP;
	chg->capacity = -EINVAL;

	switch (chg->mode) {
	case PARALLEL_MASTER:
		rc = qcom_batt_init(&chg->chg_param);
		if (rc < 0) {
			smblib_err(chg, "Couldn't init qcom_batt_init rc=%d\n",
				rc);
			return rc;
		}

		rc = qcom_step_chg_init(chg->dev, chg->step_chg_enabled,
						chg->sw_jeita_enabled, false);
		if (rc < 0) {
			smblib_err(chg, "Couldn't init qcom_step_chg_init rc=%d\n",
				rc);
			return rc;
		}

		rc = smblib_create_votables(chg);
		if (rc < 0) {
			smblib_err(chg, "Couldn't create votables rc=%d\n",
				rc);
			return rc;
		}

		chg->bms_psy = power_supply_get_by_name("bms");
		if (chg->ext_bbc) {
			chg->bbc_psy = power_supply_get_by_name("bbc");
		}

#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
		chg->batt_verify_psy = power_supply_get_by_name("batt_verify");
#endif

		if (chg->sec_pl_present) {
			chg->pl.psy = power_supply_get_by_name("parallel");
			if (chg->pl.psy) {
				if (chg->sec_chg_selected
					!= POWER_SUPPLY_CHARGER_SEC_CP) {
					rc = smblib_select_sec_charger(chg,
						POWER_SUPPLY_CHARGER_SEC_PL,
						POWER_SUPPLY_CP_NONE, false);
					if (rc < 0)
						smblib_err(chg, "Couldn't config pl charger rc=%d\n",
							rc);
				}

				if (chg->smb_temp_max == -EINVAL) {
					rc = smblib_get_thermal_threshold(chg,
						SMB_REG_H_THRESHOLD_MSB_REG,
						&chg->smb_temp_max);
					if (rc < 0) {
						dev_err(chg->dev, "Couldn't get charger_temp_max rc=%d\n",
								rc);
						return rc;
					}
				}

				prop_val.intval = chg->smb_temp_max;
				rc = power_supply_set_property(chg->pl.psy,
					POWER_SUPPLY_PROP_CHARGER_TEMP_MAX,
					&prop_val);
				if (rc < 0) {
					dev_err(chg->dev, "Couldn't set POWER_SUPPLY_PROP_CHARGER_TEMP_MAX rc=%d\n",
							rc);
					return rc;
				}
			}
		}

		rc = smblib_register_notifier(chg);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't register notifier rc=%d\n", rc);
			return rc;
		}
		break;
	case PARALLEL_SLAVE:
		break;
	default:
		smblib_err(chg, "Unsupported mode %d\n", chg->mode);
		return -EINVAL;
	}

	return rc;
}

int smblib_deinit(struct smb_charger *chg)
{
	switch (chg->mode) {
	case PARALLEL_MASTER:
		if (chg->uusb_moisture_protection_enabled) {
			alarm_cancel(&chg->moisture_protection_alarm);
			cancel_work_sync(&chg->moisture_protection_work);
		}
		if (chg->wa_flags & CHG_TERMINATION_WA) {
			alarm_cancel(&chg->chg_termination_alarm);
			cancel_work_sync(&chg->chg_termination_work);
		}
		del_timer_sync(&chg->apsd_timer);
		cancel_work_sync(&chg->batt_update_work);
		cancel_work_sync(&chg->bms_update_work);
		cancel_work_sync(&chg->jeita_update_work);
		cancel_work_sync(&chg->pl_update_work);
		cancel_work_sync(&chg->dcin_aicl_work);
		cancel_work_sync(&chg->cp_status_change_work);
		cancel_work_sync(&chg->batt_verify_update_work);
		cancel_work_sync(&chg->plugin_check_time_work);
		cancel_delayed_work_sync(&chg->fake_plug_out_check_work);
		cancel_delayed_work_sync(&chg->clear_hdc_work);
		cancel_delayed_work_sync(&chg->icl_change_work);
		cancel_delayed_work_sync(&chg->dc_power_work);
		cancel_delayed_work_sync(&chg->pl_enable_work);
		cancel_delayed_work_sync(&chg->uusb_otg_work);
		cancel_delayed_work_sync(&chg->bb_removal_work);
		cancel_delayed_work_sync(&chg->lpd_ra_open_work);
		cancel_delayed_work_sync(&chg->lpd_detach_work);
		cancel_delayed_work_sync(&chg->thermal_regulation_work);
		cancel_delayed_work_sync(&chg->usbov_dbc_work);
		cancel_delayed_work_sync(&chg->role_reversal_check);
		cancel_delayed_work_sync(&chg->six_pin_batt_step_chg_work);
		cancel_delayed_work_sync(&chg->pr_swap_detach_work);
		cancel_delayed_work_sync(&chg->reg_work);
#if (!defined CONFIG_FUEL_GAUGE_BQ27Z561_MUNCH) && (!defined CONFIG_DUAL_FUEL_GAUGE_BQ27Z561)
		cancel_delayed_work_sync(&chg->reduce_fcc_work);
#endif
		cancel_delayed_work_sync(&chg->charger_type_recheck);
		cancel_delayed_work_sync(&chg->raise_qc3_vbus_work);
		cancel_delayed_work_sync(&chg->conn_therm_work);
		cancel_delayed_work_sync(&chg->report_soc_decimal_work);
		cancel_delayed_work_sync(&chg->cc_un_compliant_charge_work);
		cancel_delayed_work_sync(&chg->step_charge_notify_work);
		cancel_delayed_work_sync(&chg->clean_cp_to_sw_work);

		power_supply_unreg_notifier(&chg->nb);
		smblib_destroy_votables(chg);
		qcom_step_chg_deinit();
		qcom_batt_deinit();
		break;
	case PARALLEL_SLAVE:
		break;
	default:
		smblib_err(chg, "Unsupported mode %d\n", chg->mode);
		return -EINVAL;
	}

	smblib_iio_deinit(chg);

	return 0;
}

static int __init early_parse_off_charge_flag(char *p)
{
	if (p) {
		if (!strcmp(p, "charger"))
			off_charge_flag = true;
	}

	return 0;
}
early_param("androidboot.mode", early_parse_off_charge_flag);
