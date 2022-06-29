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
#include "mddp_dev.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
typedef int32_t (*mddp_sm_init_func_t)(struct mddp_app_t *app);

struct feature_set {
	enum mddp_vc_mf_id_e type;
	uint32_t bm; // bit map
};

struct check_feature_req {
	uint16_t        major_version;
	uint16_t        minor_version;
	uint32_t        wifi_feature;
};

struct check_feature_rsp {
	uint16_t        major_version;
	uint16_t        minor_version;
	uint32_t        num;
	struct          feature_set fs[0]; // Unfixed size
};

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
	struct mddp_md_msg_t *md_msg;
	struct mddp_app_t *app;
	struct check_feature_req ap_info = {
		.major_version = 13,
		.minor_version = 0,
		.wifi_feature = 0
	};

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) + sizeof(struct check_feature_req),
			GFP_ATOMIC);
	if (unlikely(!md_msg)) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
			"%s: failed to alloc md_msg bug!\n", __func__);
		return;
	}

	md_msg->msg_id = IPC_MSG_ID_MDFPM_CHECK_FEATURE_REQ;
	md_msg->data_len = sizeof(struct check_feature_req);
	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);
	app->abnormal_flags |= MDDP_ABNORMAL_CHECK_FEATURE_ABSENT;
	memcpy(md_msg->data, &ap_info, md_msg->data_len);
	mddp_ipc_send_md(app, md_msg, MDFPM_USER_ID_MDFPM);
}

static void mddp_handshake_done(void *buf, uint32_t buf_len)
{
	struct mddp_app_t *app;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);
	app->abnormal_flags &= ~MDDP_ABNORMAL_CHECK_FEATURE_ABSENT;

	if (buf_len == 4) {
		app->feature = *(uint32_t *)buf;
	} else {
		struct check_feature_rsp *rsp;
		int i;

		rsp = (struct check_feature_rsp *)buf;
		if (buf_len == (8 + (rsp->num * 8))) {
			app->feature |= MDDP_FEATURE_NEW_INFO;
			app->mddp_feat.major_version = rsp->major_version;
			app->mddp_feat.minor_version = rsp->minor_version;
			for (i = 0; i < rsp->num; i++) {
				switch (rsp->fs[i].type) {
				case MF_ID_COMMON:
					app->mddp_feat.common = rsp->fs[i].bm;
					app->feature |= MDDP_FEATURE_MCIF_WIFI;
					break;
				case MF_ID_WFC:
					app->mddp_feat.wfc = rsp->fs[i].bm;
					app->feature |= MDDP_FEATURE_MCIF_WIFI;
					break;
				case MF_ID_MDDP_WH:
					app->mddp_feat.wh = rsp->fs[i].bm;
					app->feature |= MDDP_FEATURE_MDDP_WH;
					break;
				default:
					break;
				}
			}
		} else
			MDDP_S_LOG(MDDP_LL_ERR, "MD response size(%d) error, num(%d)",
					buf_len, rsp->num);
	}
}

bool mddp_check_subfeature(int type, int feat)
{
	struct mddp_app_t *app;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);
	switch (type) {
	case MF_ID_COMMON:
		return app->mddp_feat.common & (1 << feat);
	case MF_ID_WFC:
		return app->mddp_feat.wfc & (1 << feat);
	case MF_ID_MDDP_WH:
		return app->mddp_feat.wh & (1 << feat);
	default:
		return 0;
	}
}

char operate[4][14] = {
	"Add dl filter",
	"Del dl filter",
	"Add ul filter",
	"Del ul filter"
};

uint32_t printV4Msg(struct mdfpm_log *mdfpm_log_buf, int action_id_t, char *str_dstate)
{
	uint32_t size;
	uint16_t port[2];
	char SrcAddr[16];
	char DstAddr[16];

	snprintf(SrcAddr, 16, "%3u.%3u.%3u.%3u",
			mdfpm_log_buf->buf[0], mdfpm_log_buf->buf[1],
			mdfpm_log_buf->buf[2], mdfpm_log_buf->buf[3]);
	snprintf(DstAddr, 16, "%3u.%3u.%3u.%3u",
			mdfpm_log_buf->buf[4], mdfpm_log_buf->buf[5],
			mdfpm_log_buf->buf[6], mdfpm_log_buf->buf[7]);
	port[0] = mdfpm_log_buf->buf[9] << 8 | mdfpm_log_buf->buf[8];
	port[1] = mdfpm_log_buf->buf[11] << 8 | mdfpm_log_buf->buf[10];
	size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ,
			"[MDDP_WH] %s, src_addr(%s), dst_addr(%s), src_port(%5u), dst_port(%5u)",
			operate[action_id_t%4], SrcAddr, DstAddr, port[0], port[1]);
	MDDP_S_LOG(MDDP_LL_DEBUG, "%s", str_dstate);
	return size;
}

uint32_t printV6Msg(struct mdfpm_log *mdfpm_log_buf, int action_id_t, char *str_dstate)
{
	uint32_t idx, size;
	uint16_t d16Addr[16], v6Port[2];
	char v6SrcAddr[40];
	char v6DstAddr[40];

	for (idx = 0; idx < 16; idx++)
		d16Addr[idx] = mdfpm_log_buf->buf[2*idx+1] << 8 | mdfpm_log_buf->buf[2*idx];
	snprintf(v6SrcAddr, 40,	"%4x:%4x:%4x:%4x:%4x:%4x:%4x:%4x",
			d16Addr[0], d16Addr[1], d16Addr[2], d16Addr[3],
			d16Addr[4], d16Addr[5], d16Addr[6], d16Addr[7]);
	snprintf(v6DstAddr, 40, "%4x:%4x:%4x:%4x:%4x:%4x:%4x:%4x",
			d16Addr[8], d16Addr[9], d16Addr[10], d16Addr[11],
			d16Addr[12], d16Addr[13], d16Addr[14], d16Addr[15]);
	v6Port[0] = mdfpm_log_buf->buf[33] << 8 | mdfpm_log_buf->buf[32];
	v6Port[1] = mdfpm_log_buf->buf[35] << 8 | mdfpm_log_buf->buf[34];
	size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ,
			"[MDDP_WH] %s, src_addr(%s), dst_addr(%s), src_port(%5u), dst_port(%5u)",
			operate[action_id_t%4], v6SrcAddr, v6DstAddr, v6Port[0], v6Port[1]);
	MDDP_S_LOG(MDDP_LL_DEBUG, "%s", str_dstate);
	return size;
}

uint32_t print_unexpected_id(struct mdfpm_log *mdfpm_log_buf, uint32_t buf_len, char *str_dstate)
{
	uint32_t i, size, idx = 0;
	char buffer[MDFPM_SEND_LOG_BUF_SZ];

	for (i = 0; i < buf_len; i++)
		idx += snprintf(&buffer[idx], buf_len-idx, "%u", mdfpm_log_buf->buf[i]);
	size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ,
			"[MDDP_WH] Unexpected id, buffer:%s", buffer);
	MDDP_S_LOG(MDDP_LL_DEBUG, "%s\n", str_dstate);
	return size;
}

static void mddp_print_mdfpm_log(struct mdfpm_log *buf, uint32_t buf_len)
{
	uint32_t size = 0;
	struct mdfpm_log *mdfpm_log_buf;
	char str_dstate[MDFPM_SEND_LOG_BUF_SZ];

	if (buf_len >= MDFPM_SEND_LOG_HEADER) {
		mdfpm_log_buf = (struct mdfpm_log *)buf;
		switch (mdfpm_log_buf->action_id) {
		case MDFPM_LOG_NONE:
			size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ, "[MDDP_WH] None");
			MDDP_S_LOG(MDDP_LL_DEBUG, "%s", str_dstate);
			break;
		case MDFPM_LOG_MDDP_WH_RUN:
			size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ, "[MDDP_WH] Running");
			MDDP_S_LOG(MDDP_LL_DEBUG, "%s", str_dstate);
			break;
		case MDFPM_LOG_MDDP_WH_STOP:
			size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ, "[MDDP_WH] Stopped");
			MDDP_S_LOG(MDDP_LL_DEBUG, "%s", str_dstate);
			break;
		case MDFPM_LOG_MDDP_EM_TEST:
			size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ,
					"[MDDP_WH] em_test_cmd_id(%u)",
					mdfpm_log_buf->buf[0]);
			MDDP_S_LOG(MDDP_LL_DEBUG, "%s", str_dstate);
			break;
		case MDFPM_LOG_MDDP_WH_LOCK_MD:
			size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ,
					"[MDDP_WH] Lock MD path(%u)",
					mdfpm_log_buf->buf[0]);
			MDDP_S_LOG(MDDP_LL_DEBUG, "%s", str_dstate);
			break;
		case MDFPM_LOG_MDDP_WH_RM_BY_REQ:
			size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ,
					"[MDDP_WH] Del filter by req, task_id(%u), level(%u)",
					mdfpm_log_buf->buf[0], mdfpm_log_buf->buf[1]);
			MDDP_S_LOG(MDDP_LL_DEBUG, "%s", str_dstate);
			break;
		case MDFPM_LOG_MDDP_WH_RM_BY_ASSIGN:
			size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ,
					"[MDDP_WH] Del filter by assign, level(%u)",
					mdfpm_log_buf->buf[0]);
			MDDP_S_LOG(MDDP_LL_DEBUG, "%s", str_dstate);
			break;
		case MDFPM_LOG_MDDP_WH_RM_BY_SCORE:
			size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ,
					"[MDDP_WH] Del filter by score, level(%u)",
					mdfpm_log_buf->buf[0]);
			MDDP_S_LOG(MDDP_LL_DEBUG, "%s", str_dstate);
			break;
		case MDFPM_LOG_MD_ADD_FILTER_V4:
		case MDFPM_LOG_MD_DEL_FILTER_V4:
		case MDFPM_LOG_CS_ADD_FILTER_V4:
		case MDFPM_LOG_CS_DEL_FILTER_V4:
			size = printV4Msg(mdfpm_log_buf, mdfpm_log_buf->action_id, &str_dstate[0]);
			break;
		case MDFPM_LOG_MD_ADD_FILTER_V6:
		case MDFPM_LOG_MD_DEL_FILTER_V6:
		case MDFPM_LOG_CS_ADD_FILTER_V6:
		case MDFPM_LOG_CS_DEL_FILTER_V6:
			size = printV6Msg(mdfpm_log_buf, mdfpm_log_buf->action_id, &str_dstate[0]);
			break;
		default:
			break;
		}
		if (mdfpm_log_buf->action_id >= MDFPM_LOG_NUM)
			size = print_unexpected_id(mdfpm_log_buf, buf_len, &str_dstate[0]);
	} else {
		size = snprintf(str_dstate, MDFPM_SEND_LOG_BUF_SZ,
				"[MDDP_WH] error, buf_len(%u) should large than %u",
				buf_len, MDFPM_SEND_LOG_HEADER);
		MDDP_S_LOG(MDDP_LL_INFO, "%s", str_dstate);
	}
	mddp_enqueue_md_log(MDDP_MD_LOG_ID_GET_LOG, size, str_dstate);
}

static int32_t mddp_sm_ctrl_msg_hdlr(
		uint32_t msg_id,
		void *buf,
		uint32_t buf_len)
{
	int32_t                 ret = 0;

	switch (msg_id) {
	case IPC_MSG_ID_MDFPM_CHECK_FEATURE_RSP:
		mddp_handshake_done(buf, buf_len);
		break;
	case IPC_MSG_ID_MDFPM_LOG:
		mddp_print_mdfpm_log(buf, buf_len);
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
