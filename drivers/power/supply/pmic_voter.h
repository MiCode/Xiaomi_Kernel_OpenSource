/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __PMIC_VOTER_H
#define __PMIC_VOTER_H

#include <linux/mutex.h>

#define CHARGER_PLUG_VOTER	"CHARGER_PLUG_VOTER"
#define CHARGER_TYPE_VOTER	"CHARGER_TYPE_VOTER"
#define HW_LIMIT_VOTER		"HW_LIMIT_VOTER"
#define PDM_VOTER		"PDM_VOTER"
#define JEITA_CHARGE_VOTER	"JEITA_CHARGE_VOTER"
#define THERMAL_VOTER		"THERMAL_VOTER"
#define STEP_CHARGE_VOTER	"STEP_CHARGE_VOTER"
#define FFC_VOTER		"FFC_VOTER"
#define TYPEC_BURN_VOTER	"TYPEC_BURN_VOTER"
#define SIC_VOTER		"SIC_VOTER"
#define FG_I2C_VOTER	        "FG_I2C_VOTER"
#define MAIN_CON_ERR_VOTER	     "MAIN_CON_ERR_VOTER"
#define ICL_VOTER		"ICL_VOTER"
#define FCC_VOTER		 "FCC_VOTER"
#define NIGHT_CHARGING_VOTER	   "NIGHT_CHARGING_VOTER"
#define MIVR_VOTER		"MIVR_VOTER"
#define BAT_OVP_VOTER		"BAT_OVP_VOTER"

#define BAT_OVP_VOLTAGE_HIGH		4450
#define BAT_OVP_VOLTAGE_LOW		4100

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
extern int get_client_vote(struct votable *votable, const char *client_str);
int get_client_vote_locked(struct votable *votable, const char *client_str);
extern int get_effective_result(struct votable *votable);
int get_effective_result_locked(struct votable *votable);
const char *get_effective_client(struct votable *votable);
const char *get_effective_client_locked(struct votable *votable);
int vote(struct votable *votable, const char *client_str, bool state, int val);
int vote_override(struct votable *votable, const char *override_client,
		  bool state, int val);
int rerun_election(struct votable *votable);
extern struct votable *find_votable(const char *name);
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
