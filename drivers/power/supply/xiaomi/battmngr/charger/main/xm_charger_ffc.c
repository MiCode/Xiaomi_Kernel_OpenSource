
#include <linux/battmngr/xm_charger_core.h>

static int xm_charger_config_iterm(struct xm_charger *charger, int mode)
{
	int rc = 0, val = 0;
	int batt_temp;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_TEMP, &val);
	if (rc < 0) {
		charger_err("%s: Couldn't get battery authentic:%d\n",
					__func__, rc);
		return rc;
	}
	batt_temp = val;

	if (mode) {
		if (batt_temp >= BATT_NORMAL_H_THRESHOLD)
			val = charger->dt.ffc_ieoc_h;
		else
			val = charger->dt.ffc_ieoc_l;
	} else {
		val = charger->dt.non_ffc_ieoc;
	}

	rc = xm_battmngr_write_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_THERM_CURR, val);
	if (rc < 0) {
		charger_err("%s: Couldn't get battery authentic:%d\n",
					__func__, rc);
		return rc;
	}

	return 0;
}

int xm_charger_get_fastcharge_mode(struct xm_charger *charger, int *mode)
{
	int rc = 0;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_FASTCHARGE_MODE, mode);
	if (rc < 0) {
		charger_err("%s: Couldn't write fastcharge mode:%d\n",
					__func__, rc);
		return rc;
	}

	return 0;
}

int xm_charger_set_fastcharge_mode(struct xm_charger *charger, int mode)
{
	int rc = 0, val = 0;
	int batt_temp;

/*
	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_BATTERY_AUTH, &val);
	charger_err("%s: get battery authentic:%d\n",__func__, val);
	if (rc < 0) {
		charger_err("%s: Couldn't get battery authentic:%d\n",
					__func__, rc);
		return rc;
	}
	if (!val)
		mode = false;
*/

	/*if soc > 95 do not set fastcharge flag*/
	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_CAPACITY, &val);
	charger_err("%s: get fg capacity:%d\n",__func__, val);
	if (rc < 0) {
		charger_err("%s: Couldn't get fg capacity:%d\n",
					__func__, rc);
		return rc;
	}
	if (mode && val >= SOC_HIGH_THRESHOLD) {
		charger_err("%s: soc:%d is more than 95, do not setfastcharge mode\n",
					__func__, val);
		mode = false;
	}

	/*if temp > 480 or temp < 150 do not set fastcharge flag*/
	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_TEMP, &val);
	charger_err("%s: get fg temp:%d\n",__func__, val);
	if (rc < 0) {
		charger_err("%s: Couldn't get fg temp:%d\n",
					__func__, rc);
		return rc;
	}
	batt_temp = val;
	if (mode && (val >= BATT_WARM_THRESHOLD  || val <= BATT_COOL_THRESHOLD)) {
		charger_err("%s: temp:%d is abort, do not setfastcharge mode\n",
					__func__, val);
		mode = false;
	}

	rc = xm_battmngr_write_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_FASTCHARGE_MODE, mode);
	if (rc < 0) {
		charger_err("%s: Couldn't write fastcharge mode:%d\n",
					__func__, rc);
		return rc;
	}

	xm_charger_config_iterm(charger, mode);

	vote(charger->fcc_votable, FFC_MODE_VOTER, !mode, charger->dt.non_ffc_cc);
	vote(charger->fv_votable, FFC_MODE_VOTER, !mode, charger->dt.non_ffc_cv);

	charger_err("%s: fastcharge mode:%d\n", __func__, mode);

	return 0;
}

