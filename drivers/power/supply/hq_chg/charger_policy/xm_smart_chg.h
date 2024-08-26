#ifndef __XM_SMART_CHG_H__
#define __XM_SMART_CHG_H__

#include "hq_charger_manager.h"
#include "../common/hq_voter.h"

void set_error(struct charger_manager *manager);
void set_success(struct charger_manager *manager);
int smart_chg_is_error(struct charger_manager *manager);
void handle_smart_chg_functype(struct charger_manager *manager,
	const int func_type, const int en_ret, const int func_val);
int handle_smart_chg_functype_status(struct charger_manager *manager);
void monitor_smart_chg(struct charger_manager *manager);
void monitor_smart_batt(struct charger_manager *manager);
void monitor_cycle_count(struct charger_manager *manager);
void monitor_night_charging(struct charger_manager *manager);
void monitor_low_fast_strategy(struct charger_manager *manager);
void xm_charge_work(struct work_struct *work);
#endif
