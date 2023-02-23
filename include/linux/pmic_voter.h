/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __PMIC_VOTER_H
#define __PMIC_VOTER_H

#include <linux/mutex.h>

#define DEBUG_FORCE_CLIENT	"DEBUG_FORCE_CLIENT"
#define BBC_ENABLE_VOTER	"BBC_ENABLE_VOTER"
#define USB_SUSPEND_VOTER	"USB_SUSPEND_VOTER"
#define BAT_SUSPEND_VOTER	"BAT_SUSPEND_VOTER"
#define FULL_ENABLE_VOTER	"FULL_ENABLE_VOTER"
#define CHARGER_TYPE_VOTER	"CHARGER_TYPE_VOTER"
#define CHARGER_MANAGER_VOTER	"CHARGER_MANAGER_VOTER"
#define HW_LIMIT_VOTER		"HW_LIMIT_VOTER"
#define SIC_VOTER		"SIC_VOTER"
#define PDM_VOTER		"PDM_VOTER"
#define QCM_VOTER		"QCM_VOTER"
#define JEITA_CHARGE_VOTER	"JEITA_CHARGE_VOTER"
#define THERMAL_VOTER		"THERMAL_VOTER"
#define STEP_CHARGE_VOTER	"STEP_CHARGE_VOTER"
#define FFC_VOTER		"FFC_VOTER"
#define ITERM_WA_VOTER		"ITERM_WA_VOTER"
#define TYPEC_BURN_VOTER	"TYPEC_BURN_VOTER"
#define CV_WA_VOTER		"CV_WA_VOTER"
#define BAT_VERIFY_VOTER	"BAT_VERIFY_VOTER"
#define NIGHT_CHARGING_VOTER	"NIGHT_CHARGING_VOTER"
#define MAX_POWER_VOTER		"MAX_POWER_VOTER"
#define SOC_VOTER		"SOC_VOTER"
#define DYNAMIC_FV_VOTER	"DYNAMIC_FV_VOTER"
#define BATTERY_CONNECTOR_VOTER	"BATTERY_CONNECTOR_VOTER"

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
