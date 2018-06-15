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

#include <linux/debugfs.h>
#include "ipa_pm.h"
#include "ipa_i.h"

static const char *client_state_to_str[IPA_PM_STATE_MAX] = {
	__stringify(IPA_PM_DEACTIVATED),
	__stringify(IPA_PM_DEACTIVATE_IN_PROGRESS),
	__stringify(IPA_PM_ACTIVATE_IN_PROGRESS),
	__stringify(IPA_PM_ACTIVATED),
	__stringify(IPA_PM_ACTIVATED_PENDING_DEACTIVATION),
	__stringify(IPA_PM_ACTIVATED_TIMER_SET),
	__stringify(IPA_PM_ACTIVATED_PENDING_RESCHEDULE),
};

static const char *ipa_pm_group_to_str[IPA_PM_GROUP_MAX] = {
	__stringify(IPA_PM_GROUP_DEFAULT),
	__stringify(IPA_PM_GROUP_APPS),
	__stringify(IPA_PM_GROUP_MODEM),
};


#define IPA_PM_DRV_NAME "ipa_pm"

#define IPA_PM_DBG(fmt, args...) \
	do { \
		pr_debug(IPA_PM_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPA_PM_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_PM_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)
#define IPA_PM_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IPA_PM_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_PM_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)
#define IPA_PM_ERR(fmt, args...) \
	do { \
		pr_err(IPA_PM_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPA_PM_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_PM_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)
#define IPA_PM_DBG_STATE(hdl, name, state) \
	IPA_PM_DBG_LOW("Client[%d] %s: %s\n", hdl, name, \
		client_state_to_str[state])


#if IPA_PM_MAX_CLIENTS > 32
#error max client greater than 32 all bitmask types should be changed
#endif

/*
 * struct ipa_pm_exception_list - holds information about an exception
 * @pending: number of clients in exception that have not yet been adctivated
 * @bitmask: bitmask of the clients in the exception based on handle
 * @threshold: the threshold values for the exception
 */
struct ipa_pm_exception_list {
	char clients[IPA_PM_MAX_EX_CL];
	int pending;
	u32 bitmask;
	int threshold[IPA_PM_THRESHOLD_MAX];
};

/*
 * struct clk_scaling_db - holds information about threshholds and exceptions
 * @lock: lock the bitmasks and thresholds
 * @exception_list: pointer to the list of exceptions
 * @work: work for clock scaling algorithm
 * @active_client_bitmask: the bits represent handles in the clients array that
 * contain non-null client
 * @threshold_size: size of the throughput threshold
 * @exception_size: size of the exception list
 * @cur_vote: idx of the threshold
 * @default_threshold: the thresholds used if no exception passes
 * @current_threshold: the current threshold of the clock plan
 */
struct clk_scaling_db {
	spinlock_t lock;
	struct ipa_pm_exception_list exception_list[IPA_PM_EXCEPTION_MAX];
	struct work_struct work;
	u32 active_client_bitmask;
	int threshold_size;
	int exception_size;
	int cur_vote;
	int default_threshold[IPA_PM_THRESHOLD_MAX];
	int *current_threshold;
};

/*
 * ipa_pm state names
 *
 * Timer free states:
 * @IPA_PM_DEACTIVATED: client starting state when registered
 * @IPA_PM_DEACTIVATE_IN_PROGRESS: deactivate was called in progress of a client
 *				   activating
 * @IPA_PM_ACTIVATE_IN_PROGRESS: client is being activated by work_queue
 * @IPA_PM_ACTIVATED: client is activated without any timers
 *
 * Timer set states:
 * @IPA_PM_ACTIVATED_PENDING_DEACTIVATION: moves to deactivate once timer pass
 * @IPA_PM_ACTIVATED_TIMER_SET: client was activated while timer was set, so
 *			 when the timer pass, client will still be activated
 *@IPA_PM_ACTIVATED_PENDING_RESCHEDULE: state signifying extended timer when
 *             a client is deferred_deactivated when a time ris still active
 */
enum ipa_pm_state {
	IPA_PM_DEACTIVATED,
	IPA_PM_DEACTIVATE_IN_PROGRESS,
	IPA_PM_ACTIVATE_IN_PROGRESS,
	IPA_PM_ACTIVATED,
	IPA_PM_ACTIVATED_PENDING_DEACTIVATION,
	IPA_PM_ACTIVATED_TIMER_SET,
	IPA_PM_ACTIVATED_PENDING_RESCHEDULE,
};

#define IPA_PM_STATE_ACTIVE(state) \
	(state == IPA_PM_ACTIVATED ||\
		state == IPA_PM_ACTIVATED_PENDING_DEACTIVATION ||\
		state  == IPA_PM_ACTIVATED_TIMER_SET ||\
		state == IPA_PM_ACTIVATED_PENDING_RESCHEDULE)

#define IPA_PM_STATE_IN_PROGRESS(state) \
	(state == IPA_PM_ACTIVATE_IN_PROGRESS \
		|| state == IPA_PM_DEACTIVATE_IN_PROGRESS)

/*
 * struct ipa_pm_client - holds information about a specific IPA client
 * @name: string name of the client
 * @callback: pointer to the client's callback function
 * @callback_params: pointer to the client's callback parameters
 * @state: Activation state of the client
 * @skip_clk_vote: 0 if client votes for clock when activated, 1 if no vote
 * @group: the ipa_pm_group the client belongs to
 * @hdl: handle of the client
 * @throughput: the throughput of the client for clock scaling
 * @state_lock: spinlock to lock the pm_states
 * @activate_work: work for activate (blocking case)
 * @deactivate work: delayed work for deferred_deactivate function
 * @complete: generic wait-for-completion handler
 * @wlock: wake source to prevent AP suspend
 */
struct ipa_pm_client {
	char name[IPA_PM_MAX_EX_CL];
	void (*callback)(void*, enum ipa_pm_cb_event);
	void *callback_params;
	enum ipa_pm_state state;
	bool skip_clk_vote;
	int group;
	int hdl;
	int throughput;
	spinlock_t state_lock;
	struct work_struct activate_work;
	struct delayed_work deactivate_work;
	struct completion complete;
	struct wakeup_source wlock;
};

/*
 * struct ipa_pm_ctx - global ctx that will hold the client arrays and tput info
 * @clients: array to the clients with the handle as its index
 * @clients_by_pipe: array to the clients with endpoint as the index
 * @wq: work queue for deferred deactivate, activate, and clk_scaling work
 8 @clk_scaling: pointer to clock scaling database
 * @client_mutex: global mutex to  lock the client arrays
 * @aggragated_tput: aggragated tput value of all valid activated clients
 * @group_tput: combined throughput for the groups
 */
struct ipa_pm_ctx {
	struct ipa_pm_client *clients[IPA_PM_MAX_CLIENTS];
	struct ipa_pm_client *clients_by_pipe[IPA3_MAX_NUM_PIPES];
	struct workqueue_struct *wq;
	struct clk_scaling_db clk_scaling;
	struct mutex client_mutex;
	int aggregated_tput;
	int group_tput[IPA_PM_GROUP_MAX];
};

static struct ipa_pm_ctx *ipa_pm_ctx;

/**
 * pop_max_from_array() -pop the max and move the last element to where the
 * max was popped
 * @arr: array to be searched for max
 * @n: size of the array
 *
 * Returns: max value of the array
 */
static int pop_max_from_array(int *arr, int *n)
{
	int i;
	int max, max_idx;

	max_idx = *n - 1;
	max = 0;

	if (*n == 0)
		return 0;

	for (i = 0; i < *n; i++) {
		if (arr[i] > max) {
			max = arr[i];
			max_idx = i;
		}
	}
	(*n)--;
	arr[max_idx] = arr[*n];

	return max;
}

/**
 * calculate_throughput() - calculate the aggregated throughput
 * based on active clients
 *
 * Returns: aggregated tput value
 */
static int calculate_throughput(void)
{
	int client_tput[IPA_PM_MAX_CLIENTS] = { 0 };
	bool group_voted[IPA_PM_GROUP_MAX] = { false };
	int i, n;
	int max, second_max, aggregated_tput;
	struct ipa_pm_client *client;

	/* Create a basic array to hold throughputs*/
	for (i = 1, n = 0; i < IPA_PM_MAX_CLIENTS; i++) {
		client = ipa_pm_ctx->clients[i];
		if (client != NULL && IPA_PM_STATE_ACTIVE(client->state)) {
			/* default case */
			if (client->group == IPA_PM_GROUP_DEFAULT) {
				client_tput[n++] = client->throughput;
			} else if (group_voted[client->group] == false) {
				client_tput[n++] = ipa_pm_ctx->group_tput
					[client->group];
				group_voted[client->group] = true;
			}
		}
	}
	/*the array will only use n+1 spots. n will be the last index used*/

	aggregated_tput = 0;

	/**
	 * throughput algorithm:
	 * 1) pop the max and second_max
	 * 2) add the 2nd max to aggregated tput
	 * 3) insert the value of max - 2nd max
	 * 4) repeat until array is of size 1
	 */
	while (n > 1) {
		max = pop_max_from_array(client_tput, &n);
		second_max = pop_max_from_array(client_tput, &n);
		client_tput[n++] = max - second_max;
		aggregated_tput += second_max;
	}

	IPA_PM_DBG_LOW("Aggregated throughput: %d\n", aggregated_tput);

	return aggregated_tput;
}

/**
 * deactivate_client() - turn off the bit in the active client bitmask based on
 * the handle passed in
 * @hdl: The index of the client to be deactivated
 */
static void deactivate_client(u32 hdl)
{
	unsigned long flags;

	spin_lock_irqsave(&ipa_pm_ctx->clk_scaling.lock, flags);
	ipa_pm_ctx->clk_scaling.active_client_bitmask &= ~(1 << hdl);
	spin_unlock_irqrestore(&ipa_pm_ctx->clk_scaling.lock, flags);
	IPA_PM_DBG_LOW("active bitmask: %x\n",
		ipa_pm_ctx->clk_scaling.active_client_bitmask);
}

/**
 * activate_client() - turn on the bit in the active client bitmask based on
 * the handle passed in
 * @hdl: The index of the client to be activated
 */
static void activate_client(u32 hdl)
{
	unsigned long flags;

	spin_lock_irqsave(&ipa_pm_ctx->clk_scaling.lock, flags);
	ipa_pm_ctx->clk_scaling.active_client_bitmask |= (1 << hdl);
	spin_unlock_irqrestore(&ipa_pm_ctx->clk_scaling.lock, flags);
	IPA_PM_DBG_LOW("active bitmask: %x\n",
		ipa_pm_ctx->clk_scaling.active_client_bitmask);
}

/**
 * deactivate_client() - get threshold
 *
 * Returns: threshold of the exception that passes or default if none pass
 */
static void set_current_threshold(void)
{
	int i;
	struct clk_scaling_db *clk;
	struct ipa_pm_exception_list *exception;
	unsigned long flags;

	clk = &ipa_pm_ctx->clk_scaling;

	spin_lock_irqsave(&ipa_pm_ctx->clk_scaling.lock, flags);
	for (i = 0; i < clk->exception_size; i++) {
		exception = &clk->exception_list[i];
		if (exception->pending == 0 && (exception->bitmask
			& ~clk->active_client_bitmask) == 0) {
			spin_unlock_irqrestore(&ipa_pm_ctx->clk_scaling.lock,
				 flags);
			clk->current_threshold = exception->threshold;
			IPA_PM_DBG("Exception %d set\n", i);
			return;
		}
	}
	clk->current_threshold = clk->default_threshold;
	spin_unlock_irqrestore(&ipa_pm_ctx->clk_scaling.lock, flags);
}

/**
 * do_clk_scaling() - set the clock based on the activated clients
 *
 * Returns: 0 if success, negative otherwise
 */
static int do_clk_scaling(void)
{
	int i, tput;
	int new_th_idx = 1;
	struct clk_scaling_db *clk_scaling;

	clk_scaling = &ipa_pm_ctx->clk_scaling;

	mutex_lock(&ipa_pm_ctx->client_mutex);
	IPA_PM_DBG_LOW("clock scaling started\n");
	tput = calculate_throughput();
	ipa_pm_ctx->aggregated_tput = tput;
	set_current_threshold();

	mutex_unlock(&ipa_pm_ctx->client_mutex);

	for (i = 0; i < clk_scaling->threshold_size; i++) {
		if (tput > clk_scaling->current_threshold[i])
			new_th_idx++;
	}

	IPA_PM_DBG_LOW("old idx was at %d\n", ipa_pm_ctx->clk_scaling.cur_vote);


	if (ipa_pm_ctx->clk_scaling.cur_vote != new_th_idx) {
		ipa_pm_ctx->clk_scaling.cur_vote = new_th_idx;
		ipa3_set_clock_plan_from_pm(ipa_pm_ctx->clk_scaling.cur_vote);
	}

	IPA_PM_DBG_LOW("new idx is at %d\n", ipa_pm_ctx->clk_scaling.cur_vote);

	return 0;
}

/**
 * clock_scaling_func() - set the clock on a work queue
 */
static void clock_scaling_func(struct work_struct *work)
{
	do_clk_scaling();
}

/**
 * activate_work_func - activate a client and vote for clock on a work queue
 */
static void activate_work_func(struct work_struct *work)
{
	struct ipa_pm_client *client;
	bool dec_clk = false;
	unsigned long flags;

	client = container_of(work, struct ipa_pm_client, activate_work);
	if (!client->skip_clk_vote) {
		IPA_ACTIVE_CLIENTS_INC_SPECIAL(client->name);
		if (client->group == IPA_PM_GROUP_APPS)
			__pm_stay_awake(&client->wlock);
	}

	spin_lock_irqsave(&client->state_lock, flags);
	IPA_PM_DBG_STATE(client->hdl, client->name, client->state);
	if (client->state == IPA_PM_ACTIVATE_IN_PROGRESS) {
		client->state = IPA_PM_ACTIVATED;
	} else if (client->state == IPA_PM_DEACTIVATE_IN_PROGRESS) {
		client->state = IPA_PM_DEACTIVATED;
		dec_clk = true;
	} else {
		IPA_PM_ERR("unexpected state %d\n", client->state);
		WARN_ON(1);
	}
	spin_unlock_irqrestore(&client->state_lock, flags);

	complete_all(&client->complete);

	if (dec_clk) {
		if (!client->skip_clk_vote) {
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL(client->name);
			if (client->group == IPA_PM_GROUP_APPS)
				__pm_relax(&client->wlock);
		}

		IPA_PM_DBG_STATE(client->hdl, client->name, client->state);
		return;
	}

	activate_client(client->hdl);

	mutex_lock(&ipa_pm_ctx->client_mutex);
	if (client->callback) {
		client->callback(client->callback_params,
			IPA_PM_CLIENT_ACTIVATED);
	} else {
		IPA_PM_ERR("client has no callback");
		WARN_ON(1);
	}
	mutex_unlock(&ipa_pm_ctx->client_mutex);

	IPA_PM_DBG_STATE(client->hdl, client->name, client->state);
	do_clk_scaling();
}

/**
 * delayed_deferred_deactivate_work_func - deferred deactivate on a work queue
 */
static void delayed_deferred_deactivate_work_func(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct ipa_pm_client *client;
	unsigned long flags;

	dwork = container_of(work, struct delayed_work, work);
	client = container_of(dwork, struct ipa_pm_client, deactivate_work);

	spin_lock_irqsave(&client->state_lock, flags);
	IPA_PM_DBG_STATE(client->hdl, client->name, client->state);
	switch (client->state) {
	case IPA_PM_ACTIVATED_TIMER_SET:
		client->state = IPA_PM_ACTIVATED;
		goto bail;
	case IPA_PM_ACTIVATED_PENDING_RESCHEDULE:
		queue_delayed_work(ipa_pm_ctx->wq, &client->deactivate_work,
			msecs_to_jiffies(IPA_PM_DEFERRED_TIMEOUT));
		client->state = IPA_PM_ACTIVATED_PENDING_DEACTIVATION;
		goto bail;
	case IPA_PM_ACTIVATED_PENDING_DEACTIVATION:
		client->state = IPA_PM_DEACTIVATED;
		IPA_PM_DBG_STATE(client->hdl, client->name, client->state);
		spin_unlock_irqrestore(&client->state_lock, flags);
		if (!client->skip_clk_vote) {
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL(client->name);
			if (client->group == IPA_PM_GROUP_APPS)
				__pm_relax(&client->wlock);
		}

		deactivate_client(client->hdl);
		do_clk_scaling();
		return;
	default:
		IPA_PM_ERR("unexpected state %d\n", client->state);
		WARN_ON(1);
		goto bail;
	}

bail:
	IPA_PM_DBG_STATE(client->hdl, client->name, client->state);
	spin_unlock_irqrestore(&client->state_lock, flags);
}

static int find_next_open_array_element(const char *name)
{
	int i, n;

	n = -ENOBUFS;

	/* 0 is not a valid handle */
	for (i = IPA_PM_MAX_CLIENTS - 1; i >= 1; i--) {
		if (ipa_pm_ctx->clients[i] == NULL) {
			n = i;
			continue;
		}

		if (strlen(name) == strlen(ipa_pm_ctx->clients[i]->name))
			if (!strcmp(name, ipa_pm_ctx->clients[i]->name))
				return -EEXIST;
	}
	return n;
}

/**
 * add_client_to_exception_list() - add client to the exception list and
 * update pending if necessary
 * @hdl: index of the IPA client
 *
 * Returns: 0 if success, negative otherwise
 */
static int add_client_to_exception_list(u32 hdl)
{
	int i;
	struct ipa_pm_exception_list *exception;

	mutex_lock(&ipa_pm_ctx->client_mutex);
	for (i = 0; i < ipa_pm_ctx->clk_scaling.exception_size; i++) {
		exception = &ipa_pm_ctx->clk_scaling.exception_list[i];
		if (strnstr(exception->clients, ipa_pm_ctx->clients[hdl]->name,
			strlen(exception->clients))) {
			exception->pending--;

			if (exception->pending < 0) {
				WARN_ON(1);
				exception->pending = 0;
				mutex_unlock(&ipa_pm_ctx->client_mutex);
				return -EPERM;
			}
			exception->bitmask |= (1 << hdl);
		}
	}
	IPA_PM_DBG("%s added to exception list\n",
		ipa_pm_ctx->clients[hdl]->name);
	mutex_unlock(&ipa_pm_ctx->client_mutex);

	return 0;
}

/**
 * remove_client_to_exception_list() - remove client from the exception list and
 * update pending if necessary
 * @hdl: index of the IPA client
 *
 * Returns: 0 if success, negative otherwise
 */
static int remove_client_from_exception_list(u32 hdl)
{
	int i;
	struct ipa_pm_exception_list *exception;

	for (i = 0; i < ipa_pm_ctx->clk_scaling.exception_size; i++) {
		exception = &ipa_pm_ctx->clk_scaling.exception_list[i];
		if (exception->bitmask & (1 << hdl)) {
			exception->pending++;
			exception->bitmask &= ~(1 << hdl);
		}
	}
	IPA_PM_DBG("Client %d removed from exception list\n", hdl);

	return 0;
}

/**
 * ipa_pm_init() - initialize  IPA PM Components
 * @ipa_pm_init_params: parameters needed to fill exceptions and thresholds
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_pm_init(struct ipa_pm_init_params *params)
{
	int i, j;
	struct clk_scaling_db *clk_scaling;

	if (params == NULL) {
		IPA_PM_ERR("Invalid Params\n");
		return -EINVAL;
	}

	if (params->threshold_size <= 0
		|| params->threshold_size > IPA_PM_THRESHOLD_MAX) {
		IPA_PM_ERR("Invalid threshold size\n");
		return -EINVAL;
	}

	if (params->exception_size < 0
		|| params->exception_size > IPA_PM_EXCEPTION_MAX) {
		IPA_PM_ERR("Invalid exception size\n");
		return -EINVAL;
	}

	IPA_PM_DBG("IPA PM initialization started\n");

	if (ipa_pm_ctx != NULL) {
		IPA_PM_ERR("Already initialized\n");
		return -EPERM;
	}


	ipa_pm_ctx = kzalloc(sizeof(*ipa_pm_ctx), GFP_KERNEL);
	if (!ipa_pm_ctx) {
		IPA_PM_ERR(":kzalloc err.\n");
		return -ENOMEM;
	}

	ipa_pm_ctx->wq = create_singlethread_workqueue("ipa_pm_activate");
	if (!ipa_pm_ctx->wq) {
		IPA_PM_ERR("create workqueue failed\n");
		kfree(ipa_pm_ctx);
		return -ENOMEM;
	}

	mutex_init(&ipa_pm_ctx->client_mutex);

	/* Populate and init locks in clk_scaling_db */
	clk_scaling = &ipa_pm_ctx->clk_scaling;
	spin_lock_init(&clk_scaling->lock);
	clk_scaling->threshold_size = params->threshold_size;
	clk_scaling->exception_size = params->exception_size;
	INIT_WORK(&clk_scaling->work, clock_scaling_func);

	for (i = 0; i < params->threshold_size; i++)
		clk_scaling->default_threshold[i] =
			params->default_threshold[i];

	/* Populate exception list*/
	for (i = 0; i < params->exception_size; i++) {
		strlcpy(clk_scaling->exception_list[i].clients,
			params->exceptions[i].usecase, IPA_PM_MAX_EX_CL);
		IPA_PM_DBG("Usecase: %s\n", params->exceptions[i].usecase);

		/* Parse the commas to count the size of the clients */
		for (j = 0; j < IPA_PM_MAX_EX_CL &&
			clk_scaling->exception_list[i].clients[j]; j++) {
			if (clk_scaling->exception_list[i].clients[j] == ',')
				clk_scaling->exception_list[i].pending++;
		}

		clk_scaling->exception_list[i].pending++;
		IPA_PM_DBG("Pending: %d\n",
			clk_scaling->exception_list[i].pending);

		/* populate the threshold */
		for (j = 0; j < params->threshold_size; j++) {
			clk_scaling->exception_list[i].threshold[j]
			= params->exceptions[i].threshold[j];
		}

	}
	IPA_PM_DBG("initialization success");

	return 0;
}

int ipa_pm_destroy(void)
{
	IPA_PM_DBG("IPA PM destroy started\n");

	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("Already destroyed\n");
		return -EPERM;
	}

	destroy_workqueue(ipa_pm_ctx->wq);

	kfree(ipa_pm_ctx);
	ipa_pm_ctx = NULL;

	return 0;
}

/**
 * ipa_pm_register() - register an IPA PM client with the PM
 * @register_params: params for a client like throughput, callback, etc.
 * @hdl: int pointer that will be used as an index to access the client
 *
 * Returns: 0 on success, negative on failure
 *
 * Side effects: *hdl is replaced with the client index or -EEXIST if
 * client is already registered
 */
int ipa_pm_register(struct ipa_pm_register_params *params, u32 *hdl)
{
	struct ipa_pm_client *client;
	struct wakeup_source *wlock;

	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("PM_ctx is null\n");
		return -EINVAL;
	}

	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("PM_ctx is null\n");
		return -EINVAL;
	}

	if (params == NULL || hdl == NULL || params->name == NULL) {
		IPA_PM_ERR("Invalid Params\n");
		return -EINVAL;
	}

	IPA_PM_DBG("IPA PM registering client\n");

	mutex_lock(&ipa_pm_ctx->client_mutex);

	*hdl = find_next_open_array_element(params->name);

	if (*hdl > IPA_CLIENT_MAX) {
		mutex_unlock(&ipa_pm_ctx->client_mutex);
		IPA_PM_ERR("client is already registered or array is full\n");
		return *hdl;
	}

	ipa_pm_ctx->clients[*hdl] = kzalloc(sizeof
		(struct ipa_pm_client), GFP_KERNEL);
	if (!ipa_pm_ctx->clients[*hdl]) {
		mutex_unlock(&ipa_pm_ctx->client_mutex);
		IPA_PM_ERR(":kzalloc err.\n");
		return -ENOMEM;
	}
	mutex_unlock(&ipa_pm_ctx->client_mutex);

	client = ipa_pm_ctx->clients[*hdl];

	spin_lock_init(&client->state_lock);

	INIT_DELAYED_WORK(&client->deactivate_work,
		delayed_deferred_deactivate_work_func);

	INIT_WORK(&client->activate_work, activate_work_func);

	/* populate fields */
	strlcpy(client->name, params->name, IPA_PM_MAX_EX_CL);
	client->callback = params->callback;
	client->callback_params = params->user_data;
	client->group = params->group;
	client->hdl = *hdl;
	client->skip_clk_vote = params->skip_clk_vote;
	wlock = &client->wlock;
	wakeup_source_init(wlock, client->name);

	/* add client to exception list */
	if (add_client_to_exception_list(*hdl)) {
		ipa_pm_deregister(*hdl);
		IPA_PM_ERR("Fail to add client to exception_list\n");
		return -EPERM;
	}

	IPA_PM_DBG("IPA PM client registered with handle %d\n", *hdl);
	return 0;
}

/**
 * ipa_pm_deregister() - deregister IPA client from the PM
 * @hdl: index of the client in the array
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_pm_deregister(u32 hdl)
{
	struct ipa_pm_client *client;
	int i;
	unsigned long flags;

	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("PM_ctx is null\n");
		return -EINVAL;
	}

	if (hdl >= IPA_PM_MAX_CLIENTS) {
		IPA_PM_ERR("Invalid Param\n");
		return -EINVAL;
	}

	if (ipa_pm_ctx->clients[hdl] == NULL) {
		IPA_PM_ERR("Client is Null\n");
		return -EINVAL;
	}

	IPA_PM_DBG("IPA PM deregistering client\n");

	client = ipa_pm_ctx->clients[hdl];
	spin_lock_irqsave(&client->state_lock, flags);
	if (IPA_PM_STATE_IN_PROGRESS(client->state)) {
		spin_unlock_irqrestore(&client->state_lock, flags);
		wait_for_completion(&client->complete);
		spin_lock_irqsave(&client->state_lock, flags);
	}

	if (IPA_PM_STATE_ACTIVE(client->state)) {
		IPA_PM_DBG("Activated clients cannot be deregistered");
		spin_unlock_irqrestore(&client->state_lock, flags);
		return -EPERM;
	}
	spin_unlock_irqrestore(&client->state_lock, flags);

	mutex_lock(&ipa_pm_ctx->client_mutex);

	/* nullify pointers in pipe array */
	for (i = 0; i < IPA3_MAX_NUM_PIPES; i++) {
		if (ipa_pm_ctx->clients_by_pipe[i] == ipa_pm_ctx->clients[hdl])
			ipa_pm_ctx->clients_by_pipe[i] = NULL;
	}
	wakeup_source_trash(&client->wlock);
	kfree(client);
	ipa_pm_ctx->clients[hdl] = NULL;

	remove_client_from_exception_list(hdl);
	IPA_PM_DBG("IPA PM client %d deregistered\n", hdl);
	mutex_unlock(&ipa_pm_ctx->client_mutex);

	return 0;
}

/**
 * ipa_pm_associate_ipa_cons_to_client() - add mapping to pipe with ipa cllent
 * @hdl: index of the client to be mapped
 * @consumer: the pipe/consumer name to be pipped to the client
 *
 * Returns: 0 on success, negative on failure
 *
 * Side effects: multiple pipes are allowed to be mapped to a single client
 */
int ipa_pm_associate_ipa_cons_to_client(u32 hdl, enum ipa_client_type consumer)
{
	int idx;

	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("PM_ctx is null\n");
		return -EINVAL;
	}

	if (hdl >= IPA_PM_MAX_CLIENTS || consumer < 0 ||
		consumer >= IPA_CLIENT_MAX) {
		IPA_PM_ERR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_pm_ctx->client_mutex);
	idx = ipa_get_ep_mapping(consumer);

	IPA_PM_DBG("Mapping pipe %d to client %d\n", idx, hdl);

	if (idx < 0) {
		mutex_unlock(&ipa_pm_ctx->client_mutex);
		IPA_PM_DBG("Pipe is not used\n");
		return 0;
	}

	if (ipa_pm_ctx->clients[hdl] == NULL) {
		mutex_unlock(&ipa_pm_ctx->client_mutex);
		IPA_PM_ERR("Client is NULL\n");
		return -EPERM;
	}

	if (ipa_pm_ctx->clients_by_pipe[idx] != NULL) {
		mutex_unlock(&ipa_pm_ctx->client_mutex);
		IPA_PM_ERR("Pipe is already mapped\n");
		return -EPERM;
	}
	ipa_pm_ctx->clients_by_pipe[idx] = ipa_pm_ctx->clients[hdl];
	mutex_unlock(&ipa_pm_ctx->client_mutex);

	IPA_PM_DBG("Pipe %d is mapped to client %d\n", idx, hdl);

	return 0;
}

static int ipa_pm_activate_helper(struct ipa_pm_client *client, bool sync)
{
	struct ipa_active_client_logging_info log_info;
	int result = 0;
	unsigned long flags;

	spin_lock_irqsave(&client->state_lock, flags);
	IPA_PM_DBG_STATE(client->hdl, client->name, client->state);

	if (IPA_PM_STATE_IN_PROGRESS(client->state)) {
		if (sync) {
			spin_unlock_irqrestore(&client->state_lock, flags);
			wait_for_completion(&client->complete);
			spin_lock_irqsave(&client->state_lock, flags);
		} else {
			client->state = IPA_PM_ACTIVATE_IN_PROGRESS;
			spin_unlock_irqrestore(&client->state_lock, flags);
			return -EINPROGRESS;
		}
	}

	switch (client->state) {
	case IPA_PM_ACTIVATED_PENDING_RESCHEDULE:
	case IPA_PM_ACTIVATED_PENDING_DEACTIVATION:
		client->state = IPA_PM_ACTIVATED_TIMER_SET;
	case IPA_PM_ACTIVATED:
	case IPA_PM_ACTIVATED_TIMER_SET:
		spin_unlock_irqrestore(&client->state_lock, flags);
		return 0;
	case IPA_PM_DEACTIVATED:
		break;
	default:
		IPA_PM_ERR("Invalid State\n");
		spin_unlock_irqrestore(&client->state_lock, flags);
		return -EPERM;
	}
	IPA_PM_DBG_STATE(client->hdl, client->name, client->state);

	IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, client->name);
	if (!client->skip_clk_vote) {
		if (sync) {
			client->state = IPA_PM_ACTIVATE_IN_PROGRESS;
			spin_unlock_irqrestore(&client->state_lock, flags);
			IPA_ACTIVE_CLIENTS_INC_SPECIAL(client->name);
			spin_lock_irqsave(&client->state_lock, flags);
		} else
			result = ipa3_inc_client_enable_clks_no_block
				 (&log_info);
	}

	/* we got the clocks */
	if (result == 0) {
		client->state = IPA_PM_ACTIVATED;
		if (client->group == IPA_PM_GROUP_APPS)
			__pm_stay_awake(&client->wlock);
		spin_unlock_irqrestore(&client->state_lock, flags);
		activate_client(client->hdl);
		if (sync)
			do_clk_scaling();
		else
			queue_work(ipa_pm_ctx->wq,
				   &ipa_pm_ctx->clk_scaling.work);
		IPA_PM_DBG_STATE(client->hdl, client->name, client->state);
		return 0;
	}

	client->state = IPA_PM_ACTIVATE_IN_PROGRESS;
	init_completion(&client->complete);
	queue_work(ipa_pm_ctx->wq, &client->activate_work);
	spin_unlock_irqrestore(&client->state_lock, flags);
	IPA_PM_DBG_STATE(client->hdl, client->name, client->state);
	return -EINPROGRESS;
}

/**
 * ipa_pm_activate(): activate ipa client to vote for clock(). Can be called
 * from atomic context and returns -EINPROGRESS if cannot be done synchronously
 * @hdl: index of the client in the array
 *
 * Returns: 0 on success, -EINPROGRESS if operation cannot be done synchronously
 * and other negatives on failure
 */
int ipa_pm_activate(u32 hdl)
{
	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("PM_ctx is null\n");
		return -EINVAL;
	}

	if (hdl >= IPA_PM_MAX_CLIENTS || ipa_pm_ctx->clients[hdl] == NULL) {
		IPA_PM_ERR("Invalid Param\n");
		return -EINVAL;
	}

	return ipa_pm_activate_helper(ipa_pm_ctx->clients[hdl], false);
}

/**
 * ipa_pm_activate(): activate ipa client to vote for clock synchronously.
 * Cannot be called from an atomic contex.
 * @hdl: index of the client in the array
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_pm_activate_sync(u32 hdl)
{
	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("PM_ctx is null\n");
		return -EINVAL;
	}

	if (hdl >= IPA_PM_MAX_CLIENTS || ipa_pm_ctx->clients[hdl] == NULL) {
		IPA_PM_ERR("Invalid Param\n");
		return -EINVAL;
	}

	return ipa_pm_activate_helper(ipa_pm_ctx->clients[hdl], true);
}

/**
 * ipa_pm_deferred_deactivate(): schedule a timer to deactivate client and
 * devote clock. Can be called from atomic context (asynchronously)
 * @hdl: index of the client in the array
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_pm_deferred_deactivate(u32 hdl)
{
	struct ipa_pm_client *client;
	unsigned long flags;

	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("PM_ctx is null\n");
		return -EINVAL;
	}

	if (hdl >= IPA_PM_MAX_CLIENTS || ipa_pm_ctx->clients[hdl] == NULL) {
		IPA_PM_ERR("Invalid Param\n");
		return -EINVAL;
	}

	client = ipa_pm_ctx->clients[hdl];
	IPA_PM_DBG_STATE(hdl, client->name, client->state);

	spin_lock_irqsave(&client->state_lock, flags);
	switch (client->state) {
	case IPA_PM_ACTIVATE_IN_PROGRESS:
		client->state = IPA_PM_DEACTIVATE_IN_PROGRESS;
	case IPA_PM_DEACTIVATED:
		IPA_PM_DBG_STATE(hdl, client->name, client->state);
		spin_unlock_irqrestore(&client->state_lock, flags);
		return 0;
	case IPA_PM_ACTIVATED:
		client->state = IPA_PM_ACTIVATED_PENDING_DEACTIVATION;
		queue_delayed_work(ipa_pm_ctx->wq, &client->deactivate_work,
			msecs_to_jiffies(IPA_PM_DEFERRED_TIMEOUT));
		break;
	case IPA_PM_ACTIVATED_TIMER_SET:
	case IPA_PM_ACTIVATED_PENDING_DEACTIVATION:
		client->state = IPA_PM_ACTIVATED_PENDING_RESCHEDULE;
	case IPA_PM_DEACTIVATE_IN_PROGRESS:
	case IPA_PM_ACTIVATED_PENDING_RESCHEDULE:
		break;
	}
	IPA_PM_DBG_STATE(hdl, client->name, client->state);
	spin_unlock_irqrestore(&client->state_lock, flags);

	return 0;
}

/**
 * ipa_pm_deactivate_all_deferred(): Cancel the deferred deactivation timer and
 * immediately devotes for IPA clocks
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_pm_deactivate_all_deferred(void)
{
	int i;
	bool run_algorithm = false;
	struct ipa_pm_client *client;
	unsigned long flags;

	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("PM_ctx is null\n");
		return -EINVAL;
	}

	for (i = 1; i < IPA_PM_MAX_CLIENTS; i++) {
		client = ipa_pm_ctx->clients[i];

		if (client == NULL)
			continue;

		cancel_delayed_work_sync(&client->deactivate_work);

		if (IPA_PM_STATE_IN_PROGRESS(client->state)) {
			wait_for_completion(&client->complete);
			continue;
		}

		spin_lock_irqsave(&client->state_lock, flags);
		IPA_PM_DBG_STATE(client->hdl, client->name, client->state);

		if (client->state == IPA_PM_ACTIVATED_TIMER_SET) {
			client->state = IPA_PM_ACTIVATED;
			IPA_PM_DBG_STATE(client->hdl, client->name,
				client->state);
			spin_unlock_irqrestore(&client->state_lock, flags);
		} else if (client->state ==
				IPA_PM_ACTIVATED_PENDING_DEACTIVATION ||
			client->state ==
				IPA_PM_ACTIVATED_PENDING_RESCHEDULE) {
			run_algorithm = true;
			client->state = IPA_PM_DEACTIVATED;
			IPA_PM_DBG_STATE(client->hdl, client->name,
				client->state);
			spin_unlock_irqrestore(&client->state_lock, flags);
			if (!client->skip_clk_vote) {
				IPA_ACTIVE_CLIENTS_DEC_SPECIAL(client->name);
				if (client->group == IPA_PM_GROUP_APPS)
					__pm_relax(&client->wlock);
			}
			deactivate_client(client->hdl);
		} else /* if activated or deactivated, we do nothing */
			spin_unlock_irqrestore(&client->state_lock, flags);
	}

	if (run_algorithm)
		do_clk_scaling();

	return 0;
}

/**
 * ipa_pm_deactivate_sync(): deactivate ipa client and devote clock. Cannot be
 * called from atomic context.
 * @hdl: index of the client in the array
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_pm_deactivate_sync(u32 hdl)
{
	struct ipa_pm_client *client;
	unsigned long flags;

	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("PM_ctx is null\n");
		return -EINVAL;
	}

	if (hdl >= IPA_PM_MAX_CLIENTS || ipa_pm_ctx->clients[hdl] == NULL) {
		IPA_PM_ERR("Invalid Param\n");
		return -EINVAL;
	}
	client = ipa_pm_ctx->clients[hdl];

	cancel_delayed_work_sync(&client->deactivate_work);

	if (IPA_PM_STATE_IN_PROGRESS(client->state))
		wait_for_completion(&client->complete);

	spin_lock_irqsave(&client->state_lock, flags);
	IPA_PM_DBG_STATE(hdl, client->name, client->state);

	if (client->state == IPA_PM_DEACTIVATED) {
		spin_unlock_irqrestore(&client->state_lock, flags);
		return 0;
	}

	spin_unlock_irqrestore(&client->state_lock, flags);

	/* else case (Deactivates all Activated cases)*/
	if (!client->skip_clk_vote) {
		IPA_ACTIVE_CLIENTS_DEC_SPECIAL(client->name);
		if (client->group == IPA_PM_GROUP_APPS)
			__pm_relax(&client->wlock);
	}

	spin_lock_irqsave(&client->state_lock, flags);
	client->state = IPA_PM_DEACTIVATED;
	IPA_PM_DBG_STATE(hdl, client->name, client->state);
	spin_unlock_irqrestore(&client->state_lock, flags);
	deactivate_client(hdl);
	do_clk_scaling();

	return 0;
}

/**
 * ipa_pm_handle_suspend(): calls the callbacks of suspended clients to wake up
 * @pipe_bitmask: the bits represent the indexes of the clients to be woken up
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_pm_handle_suspend(u32 pipe_bitmask)
{
	int i;
	struct ipa_pm_client *client;
	bool client_notified[IPA_PM_MAX_CLIENTS] = { false };

	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("PM_ctx is null\n");
		return -EINVAL;
	}

	IPA_PM_DBG_LOW("bitmask: %d",  pipe_bitmask);

	if (pipe_bitmask == 0)
		return 0;

	mutex_lock(&ipa_pm_ctx->client_mutex);
	for (i = 0; i < IPA3_MAX_NUM_PIPES; i++) {
		if (pipe_bitmask & (1 << i)) {
			client = ipa_pm_ctx->clients_by_pipe[i];
			if (client && client_notified[client->hdl] == false) {
				if (client->callback) {
					client->callback(client->callback_params
						, IPA_PM_REQUEST_WAKEUP);
					client_notified[client->hdl] = true;
				} else {
					IPA_PM_ERR("client has no callback");
					WARN_ON(1);
				}
			}
		}
	}
	mutex_unlock(&ipa_pm_ctx->client_mutex);
	return 0;
}

/**
 * ipa_pm_set_perf_profile(): Adds/changes the throughput requirement to IPA PM
 * to be used for clock scaling
 * @hdl: index of the client in the array
 * @throughput: the new throughput value to be set for that client
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_pm_set_perf_profile(u32 hdl, int throughput)
{
	struct ipa_pm_client *client;
	unsigned long flags;

	if (ipa_pm_ctx == NULL) {
		IPA_PM_ERR("PM_ctx is null\n");
		return -EINVAL;
	}

	if (hdl >= IPA_PM_MAX_CLIENTS || ipa_pm_ctx->clients[hdl] == NULL
		|| throughput < 0) {
		IPA_PM_ERR("Invalid Params\n");
		return -EINVAL;
	}
	client = ipa_pm_ctx->clients[hdl];

	mutex_lock(&ipa_pm_ctx->client_mutex);
	if (client->group == IPA_PM_GROUP_DEFAULT)
		IPA_PM_DBG_LOW("Old throughput: %d\n",  client->throughput);
	else
		IPA_PM_DBG_LOW("old Group %d throughput: %d\n",
			client->group, ipa_pm_ctx->group_tput[client->group]);

	if (client->group == IPA_PM_GROUP_DEFAULT)
		client->throughput = throughput;
	else
		ipa_pm_ctx->group_tput[client->group] = throughput;

	if (client->group == IPA_PM_GROUP_DEFAULT)
		IPA_PM_DBG_LOW("New throughput: %d\n",  client->throughput);
	else
		IPA_PM_DBG_LOW("New Group %d throughput: %d\n",
			client->group, ipa_pm_ctx->group_tput[client->group]);
	mutex_unlock(&ipa_pm_ctx->client_mutex);

	spin_lock_irqsave(&client->state_lock, flags);
	if (IPA_PM_STATE_ACTIVE(client->state) || (client->group !=
			IPA_PM_GROUP_DEFAULT)) {
		spin_unlock_irqrestore(&client->state_lock, flags);
		do_clk_scaling();
		return 0;
	}
	spin_unlock_irqrestore(&client->state_lock, flags);

	return 0;
}

/**
 * ipa_pm_stat() - print PM stat
 * @buf: [in] The user buff used to print
 * @size: [in] The size of buf
 * Returns: number of bytes used on success, negative on failure
 *
 * This function is called by ipa_debugfs in order to receive
 * a picture of the clients in the PM and the throughput, threshold and cur vote
 */
int ipa_pm_stat(char *buf, int size)
{
	struct ipa_pm_client *client;
	struct clk_scaling_db *clk = &ipa_pm_ctx->clk_scaling;
	int i, j, tput, cnt = 0, result = 0;
	unsigned long flags;

	if (!buf || size < 0)
		return -EINVAL;

	mutex_lock(&ipa_pm_ctx->client_mutex);

	result = scnprintf(buf + cnt, size - cnt, "\n\nCurrent threshold: [");
	cnt += result;

	for (i = 0; i < clk->threshold_size; i++) {
		result = scnprintf(buf + cnt, size - cnt,
			"%d, ", clk->current_threshold[i]);
		cnt += result;
	}

	result = scnprintf(buf + cnt, size - cnt, "\b\b]\n");
	cnt += result;

	result = scnprintf(buf + cnt, size - cnt,
		"Aggregated tput: %d, Cur vote: %d",
		ipa_pm_ctx->aggregated_tput, clk->cur_vote);
	cnt += result;

	result = scnprintf(buf + cnt, size - cnt, "\n\nRegistered Clients:\n");
	cnt += result;


	for (i = 1; i < IPA_PM_MAX_CLIENTS; i++) {
		client = ipa_pm_ctx->clients[i];

		if (client == NULL)
			continue;

		spin_lock_irqsave(&client->state_lock, flags);
		if (client->group == IPA_PM_GROUP_DEFAULT)
			tput = client->throughput;
		else
			tput = ipa_pm_ctx->group_tput[client->group];

		result = scnprintf(buf + cnt, size - cnt,
		"Client[%d]: %s State:%s\nGroup: %s Throughput: %d Pipes: ",
			i, client->name, client_state_to_str[client->state],
			ipa_pm_group_to_str[client->group], tput);
		cnt += result;

		for (j = 0; j < IPA3_MAX_NUM_PIPES; j++) {
			if (ipa_pm_ctx->clients_by_pipe[j] == client) {
				result = scnprintf(buf + cnt, size - cnt,
					"%d, ", j);
				cnt += result;
			}
		}

		result = scnprintf(buf + cnt, size - cnt, "\b\b\n\n");
		cnt += result;
		spin_unlock_irqrestore(&client->state_lock, flags);
	}
	mutex_unlock(&ipa_pm_ctx->client_mutex);

	return cnt;
}

/**
 * ipa_pm_exceptions_stat() - print PM exceptions stat
 * @buf: [in] The user buff used to print
 * @size: [in] The size of buf
 * Returns: number of bytes used on success, negative on failure
 *
 * This function is called by ipa_debugfs in order to receive
 * a full picture of the exceptions in the PM
 */
int ipa_pm_exceptions_stat(char *buf, int size)
{
	int i, j, cnt = 0, result = 0;
	struct ipa_pm_exception_list *exception;

	if (!buf || size < 0)
		return -EINVAL;

	result = scnprintf(buf + cnt, size - cnt, "\n");
	cnt += result;

	mutex_lock(&ipa_pm_ctx->client_mutex);
	for (i = 0; i < ipa_pm_ctx->clk_scaling.exception_size; i++) {
		exception = &ipa_pm_ctx->clk_scaling.exception_list[i];
		if (exception == NULL) {
			result = scnprintf(buf + cnt, size - cnt,
			"Exception %d is NULL\n\n", i);
			cnt += result;
			continue;
		}

		result = scnprintf(buf + cnt, size - cnt,
			"Exception %d: %s\nPending: %d Bitmask: %d Threshold: ["
			, i, exception->clients, exception->pending,
			exception->bitmask);
		cnt += result;
		for (j = 0; j < ipa_pm_ctx->clk_scaling.threshold_size; j++) {
			result = scnprintf(buf + cnt, size - cnt,
				"%d, ", exception->threshold[j]);
			cnt += result;
		}
		result = scnprintf(buf + cnt, size - cnt, "\b\b]\n\n");
		cnt += result;
	}
	mutex_unlock(&ipa_pm_ctx->client_mutex);

	return cnt;
}
