// SPDX-License-Identifier: GPL-2.0
/*
 * mddp_ipc.c - MDDP IPC API between AP and MD.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched/signal.h>

#include "mddp_debug.h"
#include "mddp_ipc.h"
#include "mddp_sm.h"

#include "mtk_ccci_common.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------------------
static int32_t mddp_ipc_tty_port_s = -1;
static struct task_struct *rx_task;
static struct wfpm_smem_info_t smem_info_s[] = {
	{MDDP_MD_SMEM_USER_RX_REORDER_TO_MD, 0, WFPM_SM_E_ATTRI_WO,
		0,
		0},
	{MDDP_MD_SMEM_USER_RX_REORDER_FROM_MD, 0, WFPM_SM_E_ATTRI_RO,
		0,
		0},
	{MDDP_MD_SMEM_USER_WIFI_STATISTICS, 0, WFPM_SM_E_ATTRI_RO,
		0,
		sizeof(struct mddpw_net_stat_t)},
	{MDDP_MD_SMEM_USER_WIFI_STATISTICS_EXT, 0, WFPM_SM_E_ATTRI_RO,
		sizeof(struct mddpw_net_stat_t),
		sizeof(struct mddpw_net_stat_ext_t)},
	{MDDP_MD_SMEM_USER_SYS_STAT_SYNC, 0, WFPM_SM_E_ATTRI_RW,
		sizeof(struct mddpw_net_stat_t) + sizeof(struct mddpw_net_stat_ext_t),
		sizeof(struct mddpw_sys_stat_t)},
};

static struct mddp_ipc_rx_msg_entry_t mddp_rx_msg_table_s[] = {
	/* msg_id */
		/* rx_msg_len */
	{ IPC_MSG_ID_WFPM_ENABLE_MD_FAST_PATH_RSP,
		sizeof(struct wfpm_enable_md_func_rsp_t) },
	{ IPC_MSG_ID_WFPM_DEACTIVATE_MD_FAST_PATH_RSP,
		sizeof(struct wfpm_deactivate_md_func_rsp_t) },
	{ IPC_MSG_ID_WFPM_MD_NOTIFY,
		sizeof(struct mddpw_md_notify_info_t) },
	{ IPC_MSG_ID_MDFPM_SUSPEND_TAG_IND,
		0 },
	{ IPC_MSG_ID_MDFPM_RESUME_TAG_IND,
		0 },
};
static uint32_t mddp_rx_msg_table_cnt = ARRAY_SIZE(mddp_rx_msg_table_s);

//------------------------------------------------------------------------------
// Private variables.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Private helper macro.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------
static int32_t mddp_ipc_md_smem_layout_config(void)
{
	struct wfpm_smem_info_t    *entry;
	uint32_t                    i;
	uint32_t                    total_len = 0;
	uint32_t                    size;
	uint8_t                    *addr;
	uint8_t                     attr;

	// Adjust offset of wfpm share memory
	for (i = 0; i < MDDP_MD_SMEM_USER_NUM; i++) {
		entry = &smem_info_s[i];
		entry->offset = total_len;
		total_len += entry->size;

		size = 0;
		if (!mddp_ipc_get_md_smem_by_id(entry->user_id,
				(void **)&addr, &attr, &size) && size > 0) {
			memset(addr, 0, size);
		}
	}

	MDDP_C_LOG(MDDP_LL_INFO,
			"%s: smem total_len(%d)!\n", __func__, total_len);
	return 0;
}

static int32_t mddp_ipc_open_port(void)
{
	int32_t                     ret;

	mddp_ipc_tty_port_s = mtk_ccci_request_port(MDDP_IPC_TTY_NAME);

	if (mddp_ipc_tty_port_s < 0) {
		MDDP_C_LOG(MDDP_LL_WARN,
				"%s: Failed to request port(%s, %d)!\n",
				__func__,
				MDDP_IPC_TTY_NAME, mddp_ipc_tty_port_s);
		return -ENODEV;
	}

	ret = mtk_ccci_open_port(mddp_ipc_tty_port_s);
	if (ret < 0) {
		MDDP_C_LOG(MDDP_LL_WARN,
				"%s: Failed to open port(%d)!\n",
				__func__, mddp_ipc_tty_port_s);
		return -ENODEV;
	}

	return 0;
}

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
/*
 * Rx kthread used to receive ctrl_msg from MD.
 */
static int32_t mddp_md_msg_hdlr(void *arg)
{
	struct mdfpm_ctrl_msg_t     ctrl_msg;
	int32_t                     rx_count;

	allow_signal(SIGTERM);

	while (!kthread_should_stop()) {
		if (mddp_ipc_tty_port_s < 0) {
			MDDP_C_LOG(MDDP_LL_WARN,
					"%s: ipc_tty_port is invalid(%d)!\n",
					__func__, mddp_ipc_tty_port_s);
			msleep(5000);
			continue;
		}

		rx_count = mtk_ccci_read_data(mddp_ipc_tty_port_s,
				(char *)&(ctrl_msg), sizeof(ctrl_msg));

		if (signal_pending(current))
			break;

		if (rx_count > 0 && rx_count >= MDFPM_CTRL_MSG_HEADER_SZ) {
			// OK. Forward to dest_user.
			mddp_sm_msg_hdlr(ctrl_msg.dest_user_id, ctrl_msg.msg_id,
					&(ctrl_msg.buf), ctrl_msg.buf_len);
		} else {
			// NG. Error to read TTY port!
			MDDP_C_LOG(MDDP_LL_DEBUG,
					"%s: Failed to read TTY (%d), count(%d)!\n",
					__func__,
					mddp_ipc_tty_port_s, rx_count);
			msleep(1000);
			continue;
		}
	}

	return 0;
}

/*
 * Tx API used to send ctrl_msg to MD.
 */
int32_t mddp_ipc_send_md(
	void *in_app,
	struct mddp_md_msg_t *msg,
	enum mdfpm_user_id_e dest_user)
{
	struct mddp_app_t      *app;
	int32_t                 ret;
	struct mdfpm_ctrl_msg_t ctrl_msg;

	if (!in_app)
		app = mddp_get_default_app_inst();
	else
		app = (struct mddp_app_t *) in_app;

	if (app->state == MDDP_STATE_UNINIT) {
		kfree(msg);
		return -ENODEV;
	}

	ctrl_msg.dest_user_id = (dest_user == MDFPM_USER_ID_NULL)
		? (app->md_cfg.ipc_md_user_id) : (dest_user);

	ctrl_msg.msg_id = msg->msg_id;
	ctrl_msg.buf_len = msg->data_len;

	if (msg->data_len > 0)
		memcpy(ctrl_msg.buf, msg->data, msg->data_len);

	ret = mtk_ccci_send_data(mddp_ipc_tty_port_s, (char *)&ctrl_msg,
			sizeof(struct mddp_md_msg_t) + msg->data_len);

	kfree(msg);

	if (unlikely(ret < 0)) {
		MDDP_C_LOG(MDDP_LL_WARN,
				"%s: mtk_ccci_send_data error(%d)!\n",
				__func__, ret);
		app->abnormal_flags |= MDDP_ABNORMAL_CCCI_SEND_FAILED;
		return -EAGAIN;
	}

	return 0;
}

int32_t wfpm_ipc_get_smem_list(void **smem_info_base, uint32_t *smem_num)
{
	*smem_info_base = &smem_info_s;
	*smem_num = MDDP_MD_SMEM_USER_NUM;

	return 0;
}

int32_t mddp_ipc_get_md_smem_by_id(enum mddp_md_smem_user_id_e app_id,
		void **smem_addr, uint8_t *smem_attr, uint32_t *smem_size)
{
	struct wfpm_smem_info_t    *smem_entry;
	uint32_t                    smem_total_len;

	smem_entry = &smem_info_s[app_id];
	*smem_attr = smem_entry->attribute;
	*smem_size = smem_entry->size;

	*smem_addr = (uint8_t *)get_smem_start_addr(MD_SYS1,
		SMEM_USER_RAW_USB, &smem_total_len);

	if (!(*smem_addr))
		return -EINVAL;

	*smem_addr += smem_entry->offset;

	return 0;
}

int32_t mddp_ipc_init(void)
{
	int32_t ret = 0;

	mddp_ipc_md_smem_layout_config();

	ret = mddp_ipc_open_port();

	rx_task = kthread_run(mddp_md_msg_hdlr, NULL, "mddp_rx");

	if (IS_ERR(rx_task)) {
		MDDP_C_LOG(MDDP_LL_ERR,
				"%s: kthread_run fail(%li)!\n",
				__func__, PTR_ERR(rx_task));

		rx_task = NULL;
		ret = -ECHILD;
		mtk_ccci_release_port(mddp_ipc_tty_port_s);
	}

	return ret;
}

void mddp_ipc_uninit(void)
{
	if (rx_task) {
		send_sig(SIGTERM, rx_task, 1);
		kthread_stop(rx_task);
		rx_task = NULL;
	}

	mtk_ccci_release_port(mddp_ipc_tty_port_s);
}

bool mddp_ipc_rx_msg_validation(enum MDDP_MDFPM_MSG_ID_CODE msg_id,
		uint32_t msg_len)
{
	uint32_t                i;

	for (i = 0; i < mddp_rx_msg_table_cnt; i++)
		if (mddp_rx_msg_table_s[i].msg_id == msg_id)
			return (mddp_rx_msg_table_s[i].rx_msg_len <= msg_len)
				? (true) : (false);

	return true;
}

