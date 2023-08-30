
#include <linux/battmngr/xm_charger_core.h>

static int xm_charger_fcc_vote_callback(struct votable *votable,
			void *data, int value, const char *client)
{
	int ret = 0;

	charger_err("%s: vote FCC = %d, sw_disable = %d\n", __func__,
		value, g_battmngr_noti->mainchg_msg.sw_disable);

	if (g_battmngr_noti->mainchg_msg.sw_disable)
		value = MAIN_MIN_FCC;

	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_CURRENT, value/1000);
	if (ret) {
		charger_err("%s: failed to set FCC\n", __func__);
		return ret;
	}

	return ret;
}

static int xm_charger_fv_vote_callback(struct votable *votable,
			void *data, int value, const char *client)
{
	int ret = 0;

	charger_err("%s: vote FV = %d\n", __func__, value);
	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_VOLTAGE_TERM, value/1000);
	if (ret) {
		charger_err("%s: failed to set FV\n", __func__);
		return ret;
	}

	return ret;
}

static int xm_charger_icl_vote_callback(struct votable *votable,
			void *data, int value, const char *client)
{
	int ret = 0;

	charger_err("%s: vote ICL = %d\n", __func__, value);
	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_INPUT_CURRENT_SETTLED, value/1000);
	if (ret) {
		charger_err("%s: failed to set ICL\n", __func__);
		return ret;
	}

	return ret;
}


static int xm_charger_awake_vote_callback(struct votable *votable,
			void *data, int awake, const char *client)
{
	struct xm_charger *charger = data;

	if (awake)
		pm_stay_awake(charger->dev);
	else
		pm_relax(charger->dev);

	return 0;
}

static int xm_charger_input_suspend_vote_callback(struct votable *votable,
			void *data, int value, const char *client)
{
	int ret = 0;

	charger_err("%s: vote INPUT_SUSPEND = %d\n", __func__, value);
	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_ENABLED, !value);
	if (ret) {
		charger_err("%s: failed to set INPUT_SUSPEND\n", __func__);
		return ret;
	}

	return ret;
}

static int xm_charger_smart_batt_vote_callback(struct votable *votable,
			void *data, int value, const char *client)
{
	int ret = 0;

	charger_err("%s: vote SMART_BATT = %d\n", __func__, value);

	return ret;
}

int xm_charger_create_votable(struct xm_charger *charger)
{
	int rc = 0;

	charger_err("%s: Start\n", __func__);

	charger->fcc_votable = create_votable("FCC", VOTE_MIN, xm_charger_fcc_vote_callback, charger);
	if (IS_ERR(charger->fcc_votable)) {
		rc = PTR_ERR(charger->fcc_votable);
		charger->fcc_votable = NULL;
		destroy_votable(charger->fcc_votable);
		charger_err("%s: failed to create voter FCC\n", __func__);
	}

	charger->fv_votable = create_votable("FV", VOTE_MIN, xm_charger_fv_vote_callback, charger);
	if (IS_ERR(charger->fv_votable)) {
		rc = PTR_ERR(charger->fv_votable);
		charger->fv_votable = NULL;
		destroy_votable(charger->fv_votable);
		charger_err("%s: failed to create voter FV\n", __func__);
	}

	charger->usb_icl_votable = create_votable("ICL", VOTE_MIN, xm_charger_icl_vote_callback, charger);
	if (IS_ERR(charger->usb_icl_votable)) {
		rc = PTR_ERR(charger->usb_icl_votable);
		charger->usb_icl_votable = NULL;
		destroy_votable(charger->usb_icl_votable);
		charger_err("%s: failed to create voter ICL\n", __func__);
	}

	charger->awake_votable = create_votable("AWAKE", VOTE_SET_ANY, xm_charger_awake_vote_callback, charger);
	if (IS_ERR(charger->awake_votable)) {
		rc = PTR_ERR(charger->awake_votable);
		charger->awake_votable = NULL;
		destroy_votable(charger->awake_votable);
		charger_err("%s: failed to create voter AWAKE\n", __func__);
	}

	charger->input_suspend_votable = create_votable("INPUT_SUSPEND", VOTE_SET_ANY, xm_charger_input_suspend_vote_callback, charger);
	if (IS_ERR(charger->input_suspend_votable)) {
		rc = PTR_ERR(charger->input_suspend_votable);
		charger->input_suspend_votable = NULL;
		destroy_votable(charger->input_suspend_votable);
		charger_err("%s: failed to create voter INPUT_SUSPEND\n", __func__);
	}

	charger->smart_batt_votable = create_votable("SMART_BATT", VOTE_MIN, xm_charger_smart_batt_vote_callback, charger);
	if (IS_ERR(charger->smart_batt_votable)) {
		rc = PTR_ERR(charger->smart_batt_votable);
		charger->smart_batt_votable = NULL;
		destroy_votable(charger->smart_batt_votable);
		charger_err("%s: failed to create voter SMART_BATT\n", __func__);
	}

	charger_err("%s: End\n", __func__);

	return rc;
}

