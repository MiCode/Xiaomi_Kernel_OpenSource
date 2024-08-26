/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __PMIC_VOTER_H
#define __PMIC_VOTER_H

#include <linux/mutex.h>

#define CHARGER_TYPE_VOTER             "CHARGER_TYPE_VOTER"
#define FACTORY_KIT_VOTER              "FACTORY_KIT_VOTER"
#define JEITA_VOTER                    "JEITA_VOTER"
#define CALL_THERMAL_DAEMON_VOTER      "CALL_THERMAL_DAEMON_VOTER"
#define TEMP_THERMAL_DAEMON_VOTER      "TEMP_THERMAL_DAEMON_VOTER"
#define QC_POLICY_VOTER                "QC_POLICY_VOTER"
#define ITER_VOTER                     "ITER_VOTER"
#define MAIN_FCC_MAX_VOTER             "MAIN_FCC_MAX_VOTER"
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
#define FG_I2C_ERR                     "FG_I2C_ERR"
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#define SMART_BATT_VOTER               "SMART_BATT_VOTER"
#define CYCLE_COUNT_VOTER              "CYCLE_COUNT_VOTER"
#define NIGHT_CHARGING_VOTER           "NIGHT_CHARGING_VOTER"
#define SMOOTH_NEW_VOTER               "SMOOTH_NEW_VOTER"
#define ENDURANCE_VOTER                "ENDURANCE_VOTER"
#endif

#define false 0
#define true  1

struct votable;

enum votable_type {
	VOTE_MIN,
	VOTE_MAX,
	VOTE_SET_ANY,
	NUM_VOTABLE_TYPES,
};

bool is_client_vote_enabled(struct votable *votable, const char *client_str);
bool is_client_vote_enabled_locked(struct votable *votable,
							const char *client_str);
bool is_override_vote_enabled(struct votable *votable);
bool is_override_vote_enabled_locked(struct votable *votable);
int get_client_vote(struct votable *votable, const char *client_str);
int get_client_vote_locked(struct votable *votable, const char *client_str);
int get_effective_result(struct votable *votable);
int get_effective_result_locked(struct votable *votable);
const char *get_effective_client(struct votable *votable);
const char *get_effective_client_locked(struct votable *votable);
int vote(struct votable *votable, const char *client_str, bool state, int val);
int vote_override(struct votable *votable, const char *override_client,
		  bool state, int val);
int rerun_election(struct votable *votable);
struct votable *find_votable(const char *name);
struct votable *create_votable(const char *name,
				int votable_type,
				int (*callback)(struct votable *votable,
						void *data,
						int effective_result,
						const char *effective_client),
				void *data);
void destroy_votable(struct votable *votable);
void lock_votable(struct votable *votable);
void unlock_votable(struct votable *votable);

#endif /* __PMIC_VOTER_H */
