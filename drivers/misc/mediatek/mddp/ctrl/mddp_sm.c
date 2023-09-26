// SPDX-License-Identifier: GPL-2.0
/*
 * mddp_sm.c - MDDP state machine.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/mutex.h>

#include "mddp_ctrl.h"
#include "mddp_debug.h"
#include "mddp_filter.h"
#include "mddp_sm.h"
#include "mddp_usage.h"

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
	#define MDDP_MODULE_ID(_id)
	#define MDDP_MODULE_PREFIX(_prefix) _prefix ## _sm_init,
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
static struct mutex mddp_state_handler_mtx;

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

void mddp_check_feature(void)
{
	struct mddp_md_msg_t           *md_msg;
	struct mddp_app_t *app;

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t), GFP_ATOMIC);
	if (unlikely(!md_msg)) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
			"%s: failed to alloc md_msg bug!\n", __func__);
		return;
	}

	md_msg->msg_id = IPC_MSG_ID_MDFPM_CHECK_FEATURE_REQ;
	md_msg->data_len = 0;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);
	app->abnormal_flags |= MDDP_ABNORMAL_CHECK_FEATURE_ABSENT;

	mddp_ipc_send_md(app, md_msg, MDFPM_USER_ID_MDFPM);
}

static void mddp_handshake_done(uint32_t feature)
{
	struct mddp_app_t *app;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);
	atomic_set(&app->feature, feature);
	app->abnormal_flags &= ~MDDP_ABNORMAL_CHECK_FEATURE_ABSENT;
}

static int32_t mddp_sm_ctrl_msg_hdlr(
		uint32_t msg_id,
		void *buf,
		uint32_t buf_len)
{
	int32_t                 ret = 0;

	switch (msg_id) {
	case IPC_MSG_ID_MDFPM_CHECK_FEATURE_RSP:
		mddp_handshake_done(*(uint32_t *)buf);
		break;
	default:
		MDDP_S_LOG(MDDP_LL_ERR,
				"%s: Unaccepted msg_id(%d)!\n",
				__func__, msg_id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
int32_t mddp_sm_init(void)
{
	struct mddp_app_t              *app;
	uint32_t                        type;
	uint32_t                        idx;

	memset(&mddp_app_inst_s, 0, sizeof(mddp_app_inst_s));

	for (idx = 0; idx < MDDP_MOD_CNT; idx++) {
		type = mddp_sm_module_list_s[idx];
		app = mddp_get_app_inst(type);

		app->type = type;
		MDDP_SM_LOCK_INIT(app->locker);
		_mddp_set_state(app, MDDP_STATE_UNINIT);

		mddp_sm_init_func_list_s[idx](app);
		atomic_set(&app->feature, 0);
	}
	mutex_init(&mddp_state_handler_mtx);

	return 0;
}

void mddp_sm_uninit(void)
{
	struct mddp_app_t              *app;
	uint32_t                        type;
	uint32_t                        idx;

	for (idx = 0; idx < MDDP_MOD_CNT; idx++) {
		type = mddp_sm_module_list_s[idx];
		app = mddp_get_app_inst(type);

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

static enum mddp_state_e mddp_get_state(struct mddp_app_t *app)
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

	complete(&app->md_resp_comp);
	curr_state = mddp_get_state(app);
	event = (md_rsp_result) ? MDDP_EVT_MD_RSP_OK : MDDP_EVT_MD_RSP_FAIL;

	if (curr_state == prev_state) {
		/* OK.
		 * There is no interrupt event from upper module
		 * when MD handles this request.
		 */
		new_state = mddp_sm_on_event(app, event);

		MDDP_S_LOG(MDDP_LL_NOTICE,
				"%s: OK. event(%d), prev_state(%d) -> new_state(%d).\n",
				__func__, event, prev_state, new_state);

		return new_state;
	}

	/* DC (Don't Care).
	 * There are interrupt events from upper module
	 * when MD handles this request.
	 */
	MDDP_S_LOG(MDDP_LL_WARN,
			"%s: DC. event(%d), prev_state(%d) -> new_state(%d).\n",
			__func__, event, prev_state, new_state);

	return MDDP_STATE_DUMMY;
}

#ifdef __MDDP_DEBUG__
void mddp_dump_sm_table(struct mddp_app_t *app)
{
	uint32_t                        i, j;
	struct mddp_sm_entry_t         *state_machine;
	struct mddp_sm_entry_t         *entry;

	MDDP_S_LOG(MDDP_LL_DEBUG,
			"\n\n\t%s:\n==============================\n",
			__func__);

	for (i = 0; i < MDDP_STATE_CNT; i++) {
		state_machine = app->state_machines[i];
		MDDP_S_LOG(MDDP_LL_DEBUG, "\n=====(%d)=====\n", i);

		for (j = 0; j < MDDP_EVT_CNT; j++) {
			entry = state_machine + j;

			if (entry->event == MDDP_EVT_DUMMY)
				break;

			MDDP_S_LOG(MDDP_LL_DEBUG,
					"\tevt(%d), new_state(%d)\n",
					entry->event, entry->new_state);
		}
	}

	MDDP_S_LOG(MDDP_LL_DEBUG,
			"\n ==============================\n\n");
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

	mutex_lock(&mddp_state_handler_mtx);

	new_state = old_state = mddp_get_state(app);
	state_machine = app->state_machines[old_state];

	for (idx = 0; idx < MDDP_EVT_CNT; idx++) {
		entry = state_machine + idx;
		if (event == entry->event) {
			/*
			 * OK. Valid event for this state.
			 */
			new_state = entry->new_state;

			if (new_state != MDDP_STATE_DUMMY)
				_mddp_set_state(app, new_state);

			mddp_dump_sm_table(app);
			MDDP_S_LOG(MDDP_LL_WARN,
					"%s: event(%d), old_state(%d) -> new_state(%d).\n",
					__func__, event, old_state, new_state);

			if (entry->action)
				entry->action(app);

			break;
		} else if (entry->event == MDDP_EVT_DUMMY) {
			/*
			 * NG. Unexpected event for this state!
			 */
			MDDP_S_LOG(MDDP_LL_WARN,
					"%s: Invalid event(%d) for current state(%d)!\n",
					__func__, event, old_state);

			break;
		}
	}

	mutex_unlock(&mddp_state_handler_mtx);
	return new_state;
}

void mddp_sm_wait_pre(struct mddp_app_t *app)
{
	init_completion(&app->md_resp_comp);
}

void mddp_sm_wait(struct mddp_app_t *app, enum mddp_event_e event)
{
	enum mddp_state_e state;

	state = mddp_get_state(app);
	if ((state == MDDP_STATE_DISABLED) && (event != MDDP_EVT_FUNC_ENABLE))
		return;
	if ((state ==  MDDP_STATE_DEACTIVATED) && (event == MDDP_EVT_FUNC_DEACT))
		return;

	if (wait_for_completion_timeout(&app->md_resp_comp, msecs_to_jiffies(150)) == 0)
		mddp_sm_on_event(app, MDDP_EVT_MD_RSP_TIMEOUT);
}

int32_t mddp_sm_msg_hdlr(
		uint32_t user_id,
		uint32_t msg_id,
		void *buf,
		uint32_t buf_len)
{
	struct mddp_app_t      *app = NULL;
	int32_t                 ret = -ENODEV;

	switch (user_id) {
	case MDFPM_USER_ID_MDFPM:
		ret = mddp_sm_ctrl_msg_hdlr(msg_id, buf, buf_len);
		goto _done;

	case MDFPM_USER_ID_WFPM:
		app = mddp_get_app_inst(MDDP_APP_TYPE_WH);
		break;

	case MDFPM_USER_ID_DPFM:
		ret = mddp_f_msg_hdlr(msg_id, buf, buf_len);
		if (ret)
			ret = mddp_u_msg_hdlr(msg_id, buf, buf_len);

		goto _done;

	default:
		/*
		 * NG. Receive invalid ctrl_msg!
		 */
		MDDP_S_LOG(MDDP_LL_WARN,
				"%s: Unaccepted user_id(%d)!\n",
				__func__, user_id);
		ret = -EINVAL;
		goto _done;
	}

	/*
	 * OK. This app_type is configured.
	 */
	if (app && app->is_config) {
		ret = app->md_recv_msg_hdlr(msg_id, buf, buf_len);
		goto _done;
	}

	/*
	 * NG. This app_type is not configured!
	 */
	MDDP_S_LOG(MDDP_LL_ERR,
			"%s: app is not configured, app(%p), user_id(%d), msg_id(%d)!\n",
			__func__, app, user_id, msg_id);

_done:
	return ret;
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
		app->drv_reg = 1;
		app->abnormal_flags &= ~MDDP_ABNORMAL_WIFI_DRV_GET_FEATURE_BEFORE_MD_READY;
		return 0;
	}

	/*
	 * NG. MDDP is not ready!
	 */
	MDDP_S_LOG(MDDP_LL_ERR,
			"%s: Failed to reg callback, type(%d), config(%d)!\n",
			__func__, conf->app_type, app->is_config);
	return -EPERM;
}

void mddp_sm_dereg_callback(
	struct mddp_drv_conf_t *conf,
	struct mddp_drv_handle_t *handle)
{
	struct mddp_app_t      *app;

	app = mddp_get_app_inst(conf->app_type);

	/*
	 * OK. This app_type is configured.
	 */
	if (app->is_config && app->dereg_drv_callback) {
		app->dereg_drv_callback(handle);
		memset(&app->drv_hdlr,
			0, sizeof(struct mddp_drv_handle_t));
		app->drv_reg = 0;
		return;
	}

	/*
	 * NG. MDDP is not ready!
	 */
	MDDP_S_LOG(MDDP_LL_ERR,
			"%s: Failed to dereg callback, type(%d), config(%d)!\n",
			__func__, conf->app_type, app->is_config);
	return;

}
