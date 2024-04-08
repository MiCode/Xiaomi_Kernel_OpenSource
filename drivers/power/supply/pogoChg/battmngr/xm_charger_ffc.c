
#include <linux/battmngr/xm_charger_core.h>
#include <../extSOC/inc/virtual_fg.h>

static int xm_charger_config_iterm(struct xm_charger *charger, int mode)
{
	int rc = 0, val = 0;
	int batt_temp;
	union power_supply_propval pval = {
		0,
	};
	//rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			//BATT_FG_TEMP, &val);
	rc = power_supply_get_property(charger->batt_psy,
			POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0) {
		charger_err("%s: Couldn't get battery temp:%d\n",
					__func__, rc);
		return rc;
	}
	batt_temp = pval.intval;
	charger_err("%s: batt_temp:%d\n", __func__, batt_temp);
	if (mode) {
		if (batt_temp >= BATT_NORMAL_H_THRESHOLD)
			val = charger->dt.ffc_ieoc_h;
		else
			val = charger->dt.ffc_ieoc_l;
	} else {
		val = charger->dt.non_ffc_ieoc;
	}
	val /= 1000;

	charger_err("%s: charger_config_iterm:%d\n", __func__, val);
	if (check_qti_ops(&charger->battmg_dev))
		rc = battmngr_qtiops_set_term_cur(charger->battmg_dev, val);
	if (rc < 0) {
		charger_err("%s: Couldn't get charge term:%d\n",
					__func__, rc);
		return rc;
	}

	return 0;
}

int xm_charger_get_fastcharge_mode(struct xm_charger *charger, int *mode)
{
	int rc = 0;
	int fastcharge_mode = 0;
	//rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			//BATT_FG_FASTCHARGE_MODE, mode);
	if (check_qti_ops(&charger->battmg_dev))
		fastcharge_mode = battmngr_qtiops_get_fg1_fastcharge(charger->battmg_dev);

	if (rc < 0) {
		charger_err("%s: Couldn't write fastcharge mode:%d\n",
					__func__, rc);
		return rc;
	}
	*mode = fastcharge_mode;
	charger_err("%s:  fastcharge mode:%d\n",
					__func__, fastcharge_mode);
	return 0;
}

int xm_charger_set_fastcharge_mode(struct xm_charger *charger, int mode)
{
	int rc = 0, val = 0, rc1=0;
	int batt_temp;
	union power_supply_propval pval = {0, };
	int chip_ok = 0;

	if (check_qti_ops(&charger->battmg_dev))
		val = battmngr_qtiops_get_batt_auth(charger->battmg_dev);

	if (val < 0) {
		charger_err("%s: Couldn't get battery authentic:%d\n",
					__func__, val);
		return val;
	}
	charger_err("%s: get battery authentic:%d\n",__func__, val);
	if (!val)
		mode = false;

	/*if soc > 95 do not set fastcharge flag*/
	if (!charger->batt_psy)
		charger->batt_psy = power_supply_get_by_name("battery");

	rc = power_supply_get_property(charger->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &pval);
	charger_err("%s: get fg capacity:%d\n",__func__, pval.intval);
	if (rc < 0) {
		charger_err("%s: Couldn't get fg capacity:%d\n",
					__func__, rc);
		return rc;
	}
	if (mode && pval.intval >= SOC_HIGH_THRESHOLD) {
		charger_err("%s: soc:%d is more than 95, do not setfastcharge mode\n",
					__func__, pval.intval);
		mode = false;
	}

	/*if temp > 480 or temp < 150 do not set fastcharge flag*/
	rc = power_supply_get_property(charger->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	batt_temp = pval.intval;
	charger_err("%s: get fg temp:%d\n",__func__, batt_temp);
	if (rc < 0) {
		charger_err("%s: Couldn't get fg temp:%d\n",
					__func__, rc);
		return rc;
	}
	if (mode && (batt_temp >= BATT_WARM_THRESHOLD  || batt_temp <= BATT_COOL_THRESHOLD)) {
		charger_err("%s: temp:%d is abort, do not setfastcharge mode\n",
					__func__, batt_temp);
		mode = false;
	}

	if (check_qti_ops(&charger->battmg_dev))
		chip_ok = battmngr_qtiops_get_chip_ok(charger->battmg_dev);

	charger_err("%s: get fg chip_ok:%d\n",__func__, chip_ok);
	if (chip_ok < 0) {
		charger_err("%s: Couldn't get fg chip_ok:%d\n",
					__func__, chip_ok);
		return chip_ok;
	}
	if (mode && !chip_ok) {
		charger_err("%s: chip_ok is :%d, do not setfastcharge mode\n",
					__func__, chip_ok);
		mode = false;
	}

	if (check_qti_ops(&charger->battmg_dev)) {
		rc = battmngr_qtiops_set_fg1_fastcharge(charger->battmg_dev, mode);
		rc = battmngr_qtiops_set_fg2_fastcharge(charger->battmg_dev, mode);

	}
	if (rc < 0 || rc1 < 0) {
		charger_err("%s: Couldn't write fastcharge mode  FG1:%d\nFG2:%d\n",
					__func__, rc, rc1);
		return rc;
	}

	xm_charger_config_iterm(charger, mode);

	//vote(charger->fcc_votable, FFC_MODE_VOTER, !mode, charger->dt.non_ffc_cc);
	//vote(charger->fv_votable, FFC_MODE_VOTER, !mode, charger->dt.non_ffc_cv);

	charger_err("%s: fastcharge mode:%d\n", __func__, mode);

	return 0;
}

