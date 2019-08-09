/*
 * mddp_sm.c - MDDP state machine.
 *
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/types.h>

#include "mddp_sm.h"
#include "mddp_ipc.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
typedef int32_t (*mddp_sm_init_func_t)(struct mddp_app_t *app);

//------------------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------------------
static const mddp_sm_init_func_t mddp_sm_init_func_list_s[] = {
	#undef MDDP_MODULE_ID
	#undef MDDP_MODULE_PREFIX
//	#undef MDDP_MODULE_PREFIX_SEMICOLON
	#define MDDP_MODULE_ID(_id)
	#define MDDP_MODULE_PREFIX(_prefix) _prefix ## _sm_init,
//	#define MDDP_MODULE_PREFIX_SEMICOLON(_prefix)
	#include "mddp_app_config.h"
};

//------------------------------------------------------------------------------
// Private helper macro.
//------------------------------------------------------------------------------
#define MDDP_SM_LOCK_FLAG unsigned long flags
#define MDDP_SM_LOCK_INIT(_locker) spin_lock_init(&(_locker))
#define MDDP_SM_LOCK(_locker) spin_lock_irqsave(&(_locker), flags)
#define MDDP_SM_UNLOCK(_locker) spin_unlock_irqrestore(&(_locker), flags)

//------------------------------------------------------------------------------
// Private variables.
//------------------------------------------------------------------------------
static struct mddp_app_t mddp_app_inst_s[MDDP_APP_TYPE_CNT];

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------
static void _mddp_set_state(struct mddp_app_t *app, enum mddp_state_e new_state)
{
	MDDP_SM_LOCK_FLAG;

	MDDP_SM_LOCK(app->locker);
	app->state = new_state;
	MDDP_SM_UNLOCK(app->locker);
}

void _mddp_send_msg_to_md_wq(struct work_struct *in_work)
{
	MDDP_SM_LOCK_FLAG;
	struct mddp_app_t              *app;
	struct mddp_md_queue_t         *md_queue;
	struct mddp_md_msg_t           *md_msg;

	md_queue = container_of(in_work, struct mddp_md_queue_t, work);
	app = container_of(md_queue, struct mddp_app_t, md_send_queue);

	while (!list_empty(&md_queue->list)) {
		MDDP_SM_LOCK(md_queue->locker);
		md_msg = list_first_entry(&md_queue->list,
				struct mddp_md_msg_t, list);
		list_del(&md_msg->list);
		MDDP_SM_UNLOCK(md_queue->locker);

		mddp_ipc_send_md(app, md_msg, -1);

		kfree(md_msg);
	}
}

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
int32_t mddp_sm_init(void)
{
	struct mddp_app_t              *app;
	struct mddp_md_queue_t         *md_queue;
	uint32_t                        type;
	uint32_t                        idx;

	memset(&mddp_app_inst_s, 0, sizeof(mddp_app_inst_s));

	for (idx = 0; idx < MDDP_MOD_CNT; idx++) {
		type = mddp_sm_module_list_s[idx];
		app = mddp_get_app_inst(type);

		app->type = type;
		MDDP_SM_LOCK_INIT(app->locker);
		_mddp_set_state(app, MDDP_STATE_UNINIT);

		// Init md_send_queue.
		md_queue = &app->md_send_queue;
		INIT_WORK(&md_queue->work, _mddp_send_msg_to_md_wq);
		INIT_LIST_HEAD(&md_queue->list);
		MDDP_SM_LOCK_INIT(md_queue->locker);

		mddp_sm_init_func_list_s[idx](app);
	}

	return 0;
}

void mddp_sm_uninit(void)
{
	MDDP_SM_LOCK_FLAG;
	struct mddp_app_t              *app;
	struct mddp_md_queue_t         *md_queue;
	struct mddp_md_msg_t           *msg;
	uint32_t                        type;
	uint32_t                        idx;

	for (idx = 0; idx < MDDP_MOD_CNT; idx++) {
		type = mddp_sm_module_list_s[idx];
		app = mddp_get_app_inst(type);

		// Uninit md_send_queue.
		md_queue = &app->md_send_queue;
		cancel_work_sync(&md_queue->work);
		while (!list_empty(&md_queue->list)) {
			MDDP_SM_LOCK(md_queue->locker);
			msg = list_first_entry(&md_queue->list,
					struct mddp_md_msg_t, list);
			list_del(&msg->list);
			MDDP_SM_UNLOCK(md_queue->locker);
			kfree(msg);
		}

		// <TJ_TODO> uninit needs to send FUNC_DISABLE at every state
		mddp_sm_on_event(app, MDDP_EVT_FUNC_DISABLE);
	}

	memset(mddp_app_inst_s, 0, sizeof(mddp_app_inst_s));
}

struct mddp_app_t *mddp_get_default_app_inst(void)
{
	return &mddp_app_inst_s[0];
}

struct mddp_app_t *mddp_get_app_inst(enum mddp_app_type_e type)
{
	struct mddp_app_t      *app;

	app = &mddp_app_inst_s[type];
	return app;
}

enum mddp_state_e mddp_get_state(struct mddp_app_t *app)
{
	return app->state;
}

enum mddp_state_e mddp_sm_set_state_by_md_rsp(struct mddp_app_t *app,
	enum mddp_state_e prev_state,
	bool md_rsp_result)
{
	enum mddp_state_e       curr_state;
	enum mddp_state_e       new_state = MDDP_STATE_DUMMY;
	enum mddp_event_e       event;

	curr_state = mddp_get_state(app);
	event = (md_rsp_result) ? MDDP_EVT_MD_RSP_OK : MDDP_EVT_MD_RSP_FAIL;

	if (curr_state == prev_state) {
		/* OK.
		 * There is no interrupt event from upper module
		 * when MD handles this request.
		 */
		new_state = mddp_sm_on_event(app, event);

		pr_notice("%s: OK. event(%d), prev_state(%d) -> new_state(%d).\n",
				__func__, event, prev_state, new_state);

		return new_state;
	}

	/* DC (Don't Care).
	 * There are interrupt events from upper module
	 * when MD handles this request.
	 */
	pr_notice("%s: DC. event(%d), prev_state(%d) -> new_state(%d).\n",
			__func__, event, prev_state, new_state);

	return MDDP_STATE_DUMMY;
}

#if defined __MDDP_DEBUG__
void mddp_dump_sm_table(struct mddp_app_t *app)
{
	uint32_t                        i, j;
	struct mddp_sm_entry_t         *state_machine;
	struct mddp_sm_entry_t         *entry;

	pr_notice("\n\n\t%s:\n==============================\n", __func__);

	for (i = 0; i < MDDP_STATE_CNT; i++) {
		state_machine = app->state_machines[i];
		pr_notice("\n=====(%d)=====\n", i);

		for (j = 0; j < MDDP_EVT_CNT; j++) {
			entry = state_machine + j;

			if (entry->event == MDDP_EVT_DUMMY)
				break;

			pr_notice("\tevt(%d), new_state(%d)\n",
					entry->event, entry->new_state);
		}
	}

	pr_notice("\n ==============================\n\n");
}
#endif

enum mddp_state_e mddp_sm_on_event(struct mddp_app_t *app,
		enum mddp_event_e event)
{
	uint32_t                        idx;
	enum mddp_state_e               old_state;
	enum mddp_state_e               new_state;
	struct mddp_sm_entry_t         *state_machine;
	struct mddp_sm_entry_t         *entry;

	new_state = old_state = mddp_get_state(app);
	state_machine = app->state_machines[old_state];

	for (idx = 0; idx < MDDP_EVT_CNT; idx++) {
		entry = state_machine + idx;
		if (event == entry->event) {
			/*
			 * OK. Valid event for this state.
			 */
			new_state = entry->new_state;
			_mddp_set_state(app, new_state);

			mddp_dump_sm_table(app);
			pr_notice("%s: event(%d), old_state(%d) -> new_state(%d).\n",
					__func__, event, old_state, new_state);

			if (entry->action)
				entry->action(app);

			break;
		} else if (entry->event == MDDP_EVT_DUMMY) {
			/*
			 * NG. Unexpected event for this state!
			 */
			pr_notice("%s: Invalid event(%d) for current state(%d)!\n",
					__func__, event, old_state);

			break;
		}
	}

	return new_state;
}

int32_t mddp_sm_msg_hdlr(enum mddp_app_type_e type, struct ipc_ilm *ilm)
{
	struct mddp_app_t      *app;
	int32_t                 ret;

	app = mddp_get_app_inst(type);

	/*
	 * OK. This app_type is configured.
	 */
	if (app->is_config) {
		ret = app->md_recv_msg_hdlr(ilm);
		return ret;
	}

	/*
	 * NG. This app_type is not configured!
	 */
	pr_notice("%s: Unaccepted msg(%d) on app_type(%d)!\n",
			__func__, ilm->msg_id, type);
	return -ENODEV;
}

int32_t mddp_sm_reg_callback(
	struct mddp_drv_conf_t *conf,
	struct mddp_drv_handle_t *handle)
{
	struct mddp_app_t      *app;

	app = mddp_get_app_inst(conf->app_type);

	/*
	 * OK. This app_type is configured.
	 */
	if (app->is_config && app->reg_drv_callback) {
		app->reg_drv_callback(handle);
		memcpy(&app->drv_hdlr,
			handle, sizeof(struct mddp_drv_handle_t));
		return 0;
	}

	/*
	 * NG. MDDP is not ready!
	 */
	pr_notice("%s: Failed to reg callback, type(%d), config(%d)!\n",
			__func__, conf->app_type, app->is_config);
	return -EPERM;
}
