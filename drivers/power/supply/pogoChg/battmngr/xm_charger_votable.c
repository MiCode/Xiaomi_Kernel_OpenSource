
//#include <linux/battmngr/xm_charger_core.h>
#include <linux/battmngr/qti_use_pogo.h>


static int xm_charger_fcc_vote_callback(struct votable *votable,
			void *data, int value, const char *client)
{

	int rc = 0;

	rc = write_voter_prop_id(Nanosic_FCC, value);
	if (rc < 0) {
		charger_err("%s, %s Failed to set FCC (%d uA) rc=%d\n", __func__, client, value, rc);
	} else {
		charger_err("%s: %s vote FCC = %d\n", __func__, client, value);
	}

	return rc;
}

static int xm_charger_fv_vote_callback(struct votable *votable,
			void *data, int value, const char *client)
{
	int rc = 0;

	rc = write_voter_prop_id(Nanosic_FV, value);
	if (rc < 0) {
		charger_err("%s, %s Failed to set FV (%d uA) rc=%d\n", __func__, client, value, rc);
	} else {
		charger_err("%s: %s vote FV = %d\n", __func__, client, value);

	}

	return rc;
}

static int xm_charger_icl_vote_callback(struct votable *votable,
			void *data, int value, const char *client)
{


	int rc = 0;

	rc = write_voter_prop_id(Nanosic_ICL, value);

	if (rc < 0) {
		charger_err("%s, %s Failed to set ICL (%d uA) rc=%d\n", __func__, client, value, rc);
		return -ENODEV;
	} else {
		charger_err("%s: %s vote ICL = %d\n", __func__,  client, value);
	}

	return 0;
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
	static u8 flag = 0;
	charger_err("%s: Start\n", __func__);

	if(check_g_bcdev_ops()) {
		if (flag) {
			charger_err("%s: Already create vote.\n", __func__);
			return 1;
		}

		charger->fcc_votable = create_votable("FCC", VOTE_MIN, xm_charger_fcc_vote_callback, charger);
		if (IS_ERR(charger->fcc_votable)) {
			rc = PTR_ERR(charger->fcc_votable);
			charger->fcc_votable = NULL;
			destroy_votable(charger->fcc_votable);
			charger_err("%s: failed to create voter FCC\n", __func__);
			return rc;
		}

		charger->fv_votable = create_votable("FV", VOTE_MIN, xm_charger_fv_vote_callback, charger);
		if (IS_ERR(charger->fv_votable)) {
			rc = PTR_ERR(charger->fv_votable);
			charger->fv_votable = NULL;
			destroy_votable(charger->fv_votable);
			charger_err("%s: failed to create voter FV\n", __func__);
			return rc;
		}

		charger->usb_icl_votable = create_votable("ICL", VOTE_MIN, xm_charger_icl_vote_callback, charger);
		if (IS_ERR(charger->usb_icl_votable)) {
			rc = PTR_ERR(charger->usb_icl_votable);
			charger->usb_icl_votable = NULL;
			destroy_votable(charger->usb_icl_votable);
			charger_err("%s: failed to create voter ICL\n", __func__);
			return rc;
		}

		charger->awake_votable = create_votable("AWAKE", VOTE_SET_ANY, xm_charger_awake_vote_callback, charger);
		if (IS_ERR(charger->awake_votable)) {
			rc = PTR_ERR(charger->awake_votable);
			charger->awake_votable = NULL;
			destroy_votable(charger->awake_votable);
			charger_err("%s: failed to create voter AWAKE\n", __func__);
			return rc;
		}

		charger->input_suspend_votable = create_votable("INPUT_SUSPEND", VOTE_SET_ANY, xm_charger_input_suspend_vote_callback, charger);
		if (IS_ERR(charger->input_suspend_votable)) {
			rc = PTR_ERR(charger->input_suspend_votable);
			charger->input_suspend_votable = NULL;
			destroy_votable(charger->input_suspend_votable);
			charger_err("%s: failed to create voter INPUT_SUSPEND\n", __func__);
			return rc;
		}

		charger->smart_batt_votable = create_votable("SMART_BATT", VOTE_MIN, xm_charger_smart_batt_vote_callback, charger);
		if (IS_ERR(charger->smart_batt_votable)) {
			rc = PTR_ERR(charger->smart_batt_votable);
			charger->smart_batt_votable = NULL;
			destroy_votable(charger->smart_batt_votable);
			charger_err("%s: failed to create voter SMART_BATT\n", __func__);
			return rc;
		}
		flag = 1;
		charger_err("%s: callback successful \n", __func__);
	} else  {
		charger_err("%s: callback failed \n", __func__);
		return -1;
	}

	charger_err("%s: End\n", __func__);

	return rc;
}

