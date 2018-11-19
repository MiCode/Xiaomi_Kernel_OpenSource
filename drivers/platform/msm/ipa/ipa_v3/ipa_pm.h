/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _IPA_PM_H_
#define _IPA_PM_H_

#include <linux/msm_ipa.h>

/* internal to ipa */
#define IPA_PM_MAX_CLIENTS 32 /* actual max is value -1 since we start from 1*/
#define IPA_PM_MAX_EX_CL 64
#define IPA_PM_THRESHOLD_MAX 5
#define IPA_PM_EXCEPTION_MAX 2
#define IPA_PM_DEFERRED_TIMEOUT 10

/*
 * ipa_pm group names
 *
 * Default stands for individual clients while other groups share one throughput
 * Some groups also have special flags like modem which do not vote for clock
 * but is accounted for in clock scaling while activated
 */
enum ipa_pm_group {
	IPA_PM_GROUP_DEFAULT,
	IPA_PM_GROUP_APPS,
	IPA_PM_GROUP_MODEM,
	IPA_PM_GROUP_MAX,
};

/*
 * ipa_pm_cb_event
 *
 * specifies what kind of callback is being called.
 * IPA_PM_CLIENT_ACTIVATED: the client has completed asynchronous activation
 * IPA_PM_REQUEST_WAKEUP: wake up the client after it has been suspended
 */
enum ipa_pm_cb_event {
	IPA_PM_CLIENT_ACTIVATED,
	IPA_PM_REQUEST_WAKEUP,
	IPA_PM_CB_EVENT_MAX,
};

/*
 * struct ipa_pm_exception - clients included in exception and its threshold
 * @usecase: comma separated client names
 * @threshold: the threshold values for the exception
 */
struct ipa_pm_exception {
	const char *usecase;
	int threshold[IPA_PM_THRESHOLD_MAX];
};

/*
 * struct ipa_pm_init_params - parameters needed for initializng the pm
 * @default_threshold: the thresholds used if no exception passes
 * @threshold_size: size of the threshold
 * @exceptions: list of exceptions  for the pm
 * @exception_size: size of the exception_list
 */
struct ipa_pm_init_params {
	int default_threshold[IPA_PM_THRESHOLD_MAX];
	int threshold_size;
	struct ipa_pm_exception exceptions[IPA_PM_EXCEPTION_MAX];
	int exception_size;
};

/*
 * struct ipa_pm_register_params - parameters needed to register a client
 * @name: name of the client
 * @callback: pointer to the client's callback function
 * @user_data: pointer to the client's callback parameters
 * @group: group number of the client
 * @skip_clk_vote: 0 if client votes for clock when activated, 1 if no vote
 */
struct ipa_pm_register_params {
	const char *name;
	void (*callback)(void*, enum ipa_pm_cb_event);
	void *user_data;
	enum ipa_pm_group group;
	bool skip_clk_vote;
};

#ifdef CONFIG_IPA3

int ipa_pm_register(struct ipa_pm_register_params *params, u32 *hdl);
int ipa_pm_associate_ipa_cons_to_client(u32 hdl, enum ipa_client_type consumer);
int ipa_pm_activate(u32 hdl);
int ipa_pm_activate_sync(u32 hdl);
int ipa_pm_deferred_deactivate(u32 hdl);
int ipa_pm_deactivate_sync(u32 hdl);
int ipa_pm_set_throughput(u32 hdl, int throughput);
int ipa_pm_deregister(u32 hdl);

/* IPA Internal Functions */
int ipa_pm_init(struct ipa_pm_init_params *params);
int ipa_pm_destroy(void);
int ipa_pm_handle_suspend(u32 pipe_bitmask);
int ipa_pm_deactivate_all_deferred(void);
int ipa_pm_stat(char *buf, int size);
int ipa_pm_exceptions_stat(char *buf, int size);

#else

static inline int ipa_pm_register(
	struct ipa_pm_register_params *params, u32 *hdl)
{
	return -EPERM;
}

static inline int ipa_pm_associate_ipa_cons_to_client(
	u32 hdl, enum ipa_client_type consumer)
{
	return -EPERM;
}

static inline int ipa_pm_activate(u32 hdl)
{
	return -EPERM;
}

static inline int ipa_pm_activate_sync(u32 hdl)
{
	return -EPERM;
}

static inline int ipa_pm_deferred_deactivate(u32 hdl)
{
	return -EPERM;
}

static inline int ipa_pm_deactivate_sync(u32 hdl)
{
	return -EPERM;
}

static inline int ipa_pm_set_throughput(u32 hdl, int throughput)
{
	return -EPERM;
}

static inline int ipa_pm_deregister(u32 hdl)
{
	return -EPERM;
}

/* IPA Internal Functions */
static inline int ipa_pm_init(struct ipa_pm_init_params *params)
{
	return -EPERM;
}

static inline int ipa_pm_destroy(void)
{
	return -EPERM;
}

static inline int ipa_pm_handle_suspend(u32 pipe_bitmask)
{
	return -EPERM;
}

static inline int ipa_pm_deactivate_all_deferred(void)
{
	return -EPERM;
}

static inline int ipa_pm_stat(char *buf, int size)
{
	return -EPERM;
}

static inline int ipa_pm_exceptions_stat(char *buf, int size)
{
	return -EPERM;
}
#endif

#endif /* _IPA_PM_H_ */
