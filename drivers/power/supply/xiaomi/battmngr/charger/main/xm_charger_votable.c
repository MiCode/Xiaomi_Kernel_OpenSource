
#include <linux/battmngr/xm_charger_core.h>

static int xm_charger_fcc_vote_callback(struct votable *votable,
			void *data, int value, const char *client)
{
	//struct xm_charger *charger = data;
	int ret = 0;

	charger_err("%s: vote FCC = %d\n", __func__, value);
	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_CURRENT, value);
	if (ret) {
		charger_err("%s: failed to set FCC\n", __func__);
		return ret;
	}

	return ret;
}

static int xm_charger_fv_vote_callback(struct votable *votable,
			void *data, int value, const char *client)
{
	//struct xm_charger *charger = data;
	int ret = 0;

	charger_err("%s: vote FV = %d\n", __func__, value);
	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_VOLTAGE_TERM, value);
	if (ret) {
		charger_err("%s: failed to set FV\n", __func__);
		return ret;
	}

	return ret;
}

static int xm_charger_icl_vote_callback(struct votable *votable,
			void *data, int value, const char *client)
{
	//struct xm_charger *charger = data;
	int ret = 0;

	charger_err("%s: vote ICL = %d\n", __func__, value);
	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_INPUT_CURRENT_SETTLED, value);
	if (ret) {
		charger_err("%s: failed to set ICL\n", __func__);
		return ret;
	}

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

	charger->icl_votable = create_votable("ICL", VOTE_MIN, xm_charger_icl_vote_callback, charger);
	if (IS_ERR(charger->icl_votable)) {
		rc = PTR_ERR(charger->icl_votable);
		charger->icl_votable = NULL;
		destroy_votable(charger->icl_votable);
		charger_err("%s: failed to create voter ICL\n", __func__);
	}

	charger_err("%s: End\n", __func__);

	return rc;
}

