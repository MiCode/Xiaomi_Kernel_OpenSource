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

static int pse_get_next_cc_cv(struct batt_props bat_prop,
	struct ps_batt_chg_prof  bprof, unsigned long *cc, unsigned long *cv)
{
	int tzone;
	struct ps_pse_mod_prof *pse_mod_bprof =
			(struct ps_pse_mod_prof *) bprof.batt_prof;
	if ((bprof.chrg_prof_type != PSE_MOD_CHRG_PROF) || (!pse_mod_bprof))
		return 1;

	*cc = *cv = 0;

	tzone = get_tempzone(pse_mod_bprof, bat_prop.temperature);

	if (tzone < 0)
		return 1;

	*cv = pse_mod_bprof->temp_mon_range[tzone].full_chrg_vol;
	*cc = pse_mod_bprof->temp_mon_range[tzone].full_chrg_cur;

	return 0;
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
