#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>
#include <linux/power/battery_id.h>
#include "power_supply.h"
#include "power_supply_charger.h"

/* 98% of CV is considered as voltage to detect Full */
#define FULL_CV_MIN 98

/* Offset to exit from maintenance charging. In maintenance charging
*  if the volatge is less than the (maintenance_lower_threshold -
*  MAINT_EXIT_OFFSET) then system can switch to normal charging
*/
#define MAINT_EXIT_OFFSET 50  /* mV */

static int get_tempzone(struct ps_pse_mod_prof *pse_mod_bprof,
		int temp)
{

	int i = 0;
	int temp_range_cnt = min_t(u16, pse_mod_bprof->temp_mon_ranges,
					BATT_TEMP_NR_RNG);

	if ((temp < pse_mod_bprof->temp_low_lim) ||
		(temp > pse_mod_bprof->temp_mon_range[0].temp_up_lim))
		return -EINVAL;

	for (i = 0; i < temp_range_cnt; ++i)
		if (temp > pse_mod_bprof->temp_mon_range[i].temp_up_lim)
			break;
	return i-1;
}

static inline bool __is_battery_full
	(long volt, long cur, long iterm, unsigned long cv)
{
	pr_devel("%s:current=%d pse_mod_bprof->chrg_term_ma =%d voltage_now=%d full_cond=%d",
			__func__, cur, iterm, volt * 100, (FULL_CV_MIN * cv));

	return ((cur > 0) && (cur <= iterm) &&
	((volt * 100)  >= (FULL_CV_MIN * cv)));

}

static inline bool is_battery_full(struct batt_props bat_prop,
		struct ps_pse_mod_prof *pse_mod_bprof, unsigned long cv)
{

	int i;
	/* Software full detection. Check the battery charge current to detect
	*  battery Full. The voltage also verified to avoid false charge
	*  full detection.
	*/
	pr_devel("%s:current=%d pse_mod_bprof->chrg_term_ma =%d bat_prop.voltage_now=%d full_cond=%d",
		__func__, bat_prop.current_now, (pse_mod_bprof->chrg_term_ma),
		bat_prop.voltage_now * 100, (FULL_CV_MIN * cv));

	for (i = (MAX_CUR_VOLT_SAMPLES - 1); i >= 0; --i) {

		if (!(__is_battery_full(bat_prop.voltage_now_cache[i],
				bat_prop.current_now_cache[i],
				pse_mod_bprof->chrg_term_ma, cv)))
			return false;
	}

	return true;
}

static int  pse_get_bat_thresholds(struct ps_batt_chg_prof  bprof,
			struct psy_batt_thresholds *bat_thresh)
{
	struct ps_pse_mod_prof *pse_mod_bprof =
			(struct ps_pse_mod_prof *) bprof.batt_prof;

	if ((bprof.chrg_prof_type != PSE_MOD_CHRG_PROF) || (!pse_mod_bprof))
		return -EINVAL;

	bat_thresh->iterm = pse_mod_bprof->chrg_term_ma;
	bat_thresh->temp_min = pse_mod_bprof->temp_low_lim;
	bat_thresh->temp_max = pse_mod_bprof->temp_mon_range[0].temp_up_lim;

	return 0;
}

static enum psy_algo_stat pse_get_next_cc_cv(struct batt_props bat_prop,
	struct ps_batt_chg_prof  bprof, unsigned long *cc, unsigned long *cv)
{
	int tzone;
	struct ps_pse_mod_prof *pse_mod_bprof =
			(struct ps_pse_mod_prof *) bprof.batt_prof;
	enum psy_algo_stat algo_stat = bat_prop.algo_stat;
	int maint_exit_volt;

	*cc = *cv = 0;

	/* If STATUS is discharging, assume that charger is not connected.
	*  If charger is not connected, no need to take any action.
	*  If charge profile type is not PSE_MOD_CHRG_PROF or the charge profile
	*  is not present, no need to take any action.
	*/

	pr_devel("%s:battery status = %d algo_status=%d\n",
			__func__, bat_prop.status, algo_stat);

	if ((bprof.chrg_prof_type != PSE_MOD_CHRG_PROF) || (!pse_mod_bprof))
		return PSY_ALGO_STAT_NOT_CHARGE;

	tzone = get_tempzone(pse_mod_bprof, bat_prop.temperature);

	if (tzone < 0)
		return PSY_ALGO_STAT_NOT_CHARGE;

	/* Change the algo status to not charging, if battery is
	*  not really charging or less than maintenance exit threshold.
	*  This way algorithm can switch to normal
	*  charging if current status is full/maintenace
	*/
	maint_exit_volt = pse_mod_bprof->
			temp_mon_range[tzone].maint_chrg_vol_ll -
				MAINT_EXIT_OFFSET;

	if ((bat_prop.status == POWER_SUPPLY_STATUS_DISCHARGING) ||
		(bat_prop.status == POWER_SUPPLY_STATUS_NOT_CHARGING) ||
			bat_prop.voltage_now < maint_exit_volt) {
		algo_stat = PSY_ALGO_STAT_NOT_CHARGE;
	}

	/* read cc and cv based on temperature and algorithm status*/
	if (algo_stat == PSY_ALGO_STAT_FULL ||
			algo_stat == PSY_ALGO_STAT_MAINT) {

		/* if status is full and voltage is lower than maintenance lower
		*  threshold change status to maintenenance
		*/

		if (algo_stat == PSY_ALGO_STAT_FULL) {
			*cv = pse_mod_bprof->temp_mon_range
					[tzone].full_chrg_vol;
			*cc = pse_mod_bprof->temp_mon_range
					[tzone].full_chrg_cur;
		}

		if (algo_stat == PSY_ALGO_STAT_FULL && (bat_prop.voltage_now <=
			pse_mod_bprof->temp_mon_range[tzone].maint_chrg_vol_ll))
				algo_stat = PSY_ALGO_STAT_MAINT;

		/* Read maintenance CC and CV */
		if (algo_stat == PSY_ALGO_STAT_MAINT) {
			*cv = pse_mod_bprof->temp_mon_range
					[tzone].maint_chrg_vol_ul;
			*cc = pse_mod_bprof->temp_mon_range
					[tzone].maint_chrg_cur;
		}
	} else {
		*cv = pse_mod_bprof->temp_mon_range[tzone].full_chrg_vol;
		*cc = pse_mod_bprof->temp_mon_range[tzone].full_chrg_cur;
		algo_stat = PSY_ALGO_STAT_CHARGE;
	}

	if (bat_prop.voltage_now > *cv) {
		algo_stat = PSY_ALGO_STAT_NOT_CHARGE;
		return algo_stat;
	}

	if (algo_stat == PSY_ALGO_STAT_FULL)
		return algo_stat;

	if (is_battery_full(bat_prop, pse_mod_bprof, *cv))
		algo_stat = PSY_ALGO_STAT_FULL;

	return algo_stat;
}

static int __init pse_algo_init(void)
{
	struct charging_algo pse_algo;
	pse_algo.chrg_prof_type = PSE_MOD_CHRG_PROF;
	pse_algo.name = "pse_algo";
	pse_algo.get_next_cc_cv = pse_get_next_cc_cv;
	pse_algo.get_batt_thresholds = pse_get_bat_thresholds;
	power_supply_register_charging_algo(&pse_algo);
	return 0;
}

module_init(pse_algo_init);
