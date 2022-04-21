// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*******************************************************************************/
/*                     E X T E R N A L   R E F E R E N C E S                   */
/*******************************************************************************/
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/timer.h>
#include "msg_thread.h"
#include "conap_scp.h"
#include "aoltest_netlink.h"
#include "aoltest_core.h"

/*******************************************************************************/
/*                             D A T A   T Y P E S                             */
/*******************************************************************************/
enum em_commd_id {
	EM_MSG_DEFAULT = 0,
	EM_MSG_START_TEST = 1,
	EM_MSG_STOP_TEST = 2,
	EM_MSG_START_DATA_TRANS = 3,
	EM_MSG_STOP_DATA_TRANS = 4,
	EM_MSG_MAX_SIZE
};

enum aoltest_core_opid {
	AOLTEST_OPID_DEFAULT = 0,
	AOLTEST_OPID_SCP_REGISTER = 1,
	AOLTEST_OPID_SCP_UNREGISTER = 2,
	AOLTEST_OPID_SEND_MSG  = 3,
	AOLTEST_OPID_RECV_MSG  = 4,
	AOLTEST_OPID_MAX
};

enum aoltest_core_status {
	AOLTEST_INACTIVE,
	AOLTEST_ACTIVE,
};

enum aoltest_msg_id {
	AOLTEST_MSG_ID_DEFAULT = 0,
	AOLTEST_MSG_ID_WIFI = 1,
	AOLTEST_MSG_ID_BT = 2,
	AOLTEST_MSG_ID_GPS = 3,
	AOLTEST_MSG_ID_MAX
};

struct aoltest_core_ctx {
	enum conap_scp_drv_type drv_type;
	struct msg_thread_ctx msg_ctx;
	struct conap_scp_drv_cb scp_test_cb;
	int status;
};

/*******************************************************************************/
/*                             M A C R O S                                     */
/*******************************************************************************/
#define MAX_BUF_LEN     (3 * 1024)

struct aoltest_core_ctx g_aoltest_ctx;

struct test_info g_test_info;
char g_buf[MAX_BUF_LEN];
static unsigned char g_is_scp_ready;
static bool g_is_test_started;
static bool g_is_data_trans;

struct aol_data_buf {
	u8 buf[MAX_BUF_LEN];
	u32 size;
	struct list_head list;
};
struct aol_buf_list {
	struct mutex lock;
	struct list_head list;
};

struct aol_buf_list g_aol_data_buf_list;

struct mutex g_aoltest_msg_lock;

/*******************************************************************************/
/*                  F U N C T I O N   D E C L A R A T I O N S                  */
/*******************************************************************************/
static int opfunc_scp_register(struct msg_op_data *op);
static int opfunc_scp_unregister(struct msg_op_data *op);
static int opfunc_send_msg(struct msg_op_data *op);
static int opfunc_recv_msg(struct msg_op_data *op);

static void aoltest_core_state_change(int state);
static void aoltest_core_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size);

static const msg_opid_func aoltest_core_opfunc[] = {
	[AOLTEST_OPID_SCP_REGISTER] = opfunc_scp_register,
	[AOLTEST_OPID_SCP_UNREGISTER] = opfunc_scp_unregister,
	[AOLTEST_OPID_SEND_MSG] = opfunc_send_msg,
	[AOLTEST_OPID_RECV_MSG] = opfunc_recv_msg,
};

/*******************************************************************************/
/*                              F U N C T I O N S                              */
/*******************************************************************************/

static struct aol_data_buf *aoltest_data_buffer_alloc(void)
{
	struct aol_data_buf *data_buf = NULL;

	if (list_empty(&g_aol_data_buf_list.list)) {
		data_buf = kmalloc(sizeof(struct aol_data_buf), GFP_KERNEL);
		if (data_buf == NULL)
			return NULL;
		INIT_LIST_HEAD(&data_buf->list);
	} else {

		mutex_lock(&g_aol_data_buf_list.lock);
		data_buf = list_first_entry(&g_aol_data_buf_list.list, struct aol_data_buf, list);
		list_del(&data_buf->list);
		mutex_unlock(&g_aol_data_buf_list.lock);
	}
	return data_buf;
}

static void aoltest_data_buffer_free(struct aol_data_buf *data_buf)
{
	if (!data_buf)
		return;
	mutex_lock(&g_aol_data_buf_list.lock);
	list_add_tail(&data_buf->list, &g_aol_data_buf_list.list);
	mutex_unlock(&g_aol_data_buf_list.lock);
}

static int is_scp_ready(void)
{
	int ret = 0;
	unsigned int retry = 10;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	while (--retry > 0) {
		ret = conap_scp_is_drv_ready(ctx->drv_type);

		if (ret == 1) {
			g_is_scp_ready = 1;
			break;
		}

		msleep(20);
	}

	if (retry == 0) {
		g_is_scp_ready = 0;
		pr_info("SCP is not yet ready\n");
		return -1;
	}

	return ret;
}

/*******************************************************************************/
/*      O P          F U N C T I O N S                                         */
/*******************************************************************************/
static int opfunc_scp_register(struct msg_op_data *op)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	ret = conap_scp_register_drv(ctx->drv_type, &ctx->scp_test_cb);
	pr_info("SCP register drv_type=[%d], ret=[%d]", ctx->drv_type, ret);

	return ret;
}

static int opfunc_scp_unregister(struct msg_op_data *op)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	ret = conap_scp_unregister_drv(ctx->drv_type);
	pr_info("SCP unregister drv_type=[%d], ret=[%d]", ctx->drv_type, ret);

	return ret;
}

static int opfunc_send_msg(struct msg_op_data *op)
{
	int ret = 0;
	u32 msg_id;
	u32 size;
	struct aol_data_buf *data_buf;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	if (!g_is_scp_ready) {
		ret = is_scp_ready();
		pr_info("[%s] is ready=[%d] ret=[%d]", __func__, g_is_scp_ready, ret);

		if (ret <= 0)
			return -1;
	}

	msg_id = (u32)op->op_data[0];
	data_buf = (struct aol_data_buf *)op->op_data[1];
	size = (u32)op->op_data[2];

	ret = conap_scp_send_message(ctx->drv_type, msg_id, data_buf->buf, size);
	pr_info("[%s] cmd is AOLTEST_CMD_START_TEST and call scp, ret=%d",
					__func__, ret);

	if (data_buf)
		aoltest_data_buffer_free(data_buf);

	pr_info("Send drv_type=[%d], cmd=[%d], ret=[%d]\n", ctx->drv_type, msg_id, ret);
	return ret;
}

static int opfunc_recv_msg(struct msg_op_data *op)
{
	int ret = 0;
	u32 msg_id = (unsigned int)op->op_data[0];
	struct aol_data_buf *data_buf = (struct aol_data_buf *)op->op_data[1];
	u32 size = (u32)op->op_data[2];

	pr_info("Send to netlink client, sz=[%d]\n", size);
	aoltest_netlink_send_to_native("[AOLTEST]", msg_id, data_buf->buf, size);
	aoltest_data_buffer_free(data_buf);

	return ret;
}

/*******************************************************************************/
/*      C H R E T E S T     F U N C T I O N S                                  */
/*******************************************************************************/
void aoltest_core_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;
	struct aol_data_buf *data_buf = NULL;

	pr_info("[%s] msg_id=[%d] size=[%d]\n", __func__, msg_id, size);
	if (ctx->status == AOLTEST_INACTIVE) {
		pr_info("EM test ctx is inactive\n");
		return;
	}

	if (size > MAX_BUF_LEN) {
		pr_info("size [%d] exceed expected [%d]\n", size, MAX_BUF_LEN);
		return;
	}

	if (size > 0) {
		data_buf = aoltest_data_buffer_alloc();
		if (!data_buf) {
			pr_notice("[%s] data buf is empty", __func__);
			return;
		}
		memcpy(&(data_buf->buf[0]), buf, size);
	}

	ret = msg_thread_send_3(&ctx->msg_ctx, AOLTEST_OPID_RECV_MSG,
							msg_id, (size_t)data_buf, size);

	if (ret)
		pr_info("[%s] Notify recv msg fail, ret=[%d]\n", __func__, ret);
}

static int _send_msg_to_scp(u32 msg_id, u32 msg_size, u8 *msg_data)
{
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;
	int ret;
	struct test_info *info = NULL;

	mutex_lock(&g_aoltest_msg_lock);

	if (msg_id == EM_MSG_START_TEST) {
		g_is_test_started = true;

		pr_info("[%s] START_TEST size=[%d][%u]", __func__, msg_size, sizeof(g_test_info));
		if (msg_size == sizeof(g_test_info)) {
			memcpy(&g_test_info, msg_data, sizeof(g_test_info));
			info = (struct test_info *)msg_data;
			pr_info("[%s] [%d][%d][%d] [%d][%d][%d]", __func__,
							info->wifi_enabled, info->wifi_scan_intvl,
							info->wifi_cb_intvl, info->bt_enabled,
							info->bt_scan_intvl, info->bt_cb_intvl);
		} else
			pr_notice("[%s] START_TEST but size not match [%d][%d]", __func__,
							sizeof(g_test_info), msg_size);
	} else if (msg_id == EM_MSG_STOP_TEST) {
		g_is_test_started = false;
	} else if (msg_id == EM_MSG_START_DATA_TRANS) {
		g_is_data_trans = true;
	} else if (msg_id == EM_MSG_STOP_DATA_TRANS) {
		g_is_data_trans = false;
	}

	ret = conap_scp_send_message(ctx->drv_type, msg_id, msg_data, msg_size);
	pr_info("[%s] send ret=%d msgid=[%d]",
					__func__, ret, msg_id);
	mutex_unlock(&g_aoltest_msg_lock);

	return 0;
}


void aoltest_core_state_change(int state)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	pr_info("[%s] reason=[%d]", __func__, state);

	if (ctx->status == AOLTEST_INACTIVE) {
		pr_info("EM test ctx is inactive\n");
		return;
	}

	// state = 1: scp ready
	// state = 0: scp stop
	if (state == 1) {
		g_is_scp_ready = 1;

		if (g_is_test_started) {

			// Re-send start test with test info
			mutex_lock(&g_aoltest_msg_lock);

			ret = conap_scp_send_message(ctx->drv_type, EM_MSG_START_TEST,
						(u8 *)&g_test_info, sizeof(g_test_info));
			if (ret)
				pr_notice("[%s] start test fail [%d]", __func__, ret);

			if (g_is_data_trans) {
				ret = conap_scp_send_message(ctx->drv_type, EM_MSG_START_DATA_TRANS,
											NULL, 0);
				if (ret)
					pr_notice("[%s] start data trans fail [%d]", __func__, ret);
			}
			mutex_unlock(&g_aoltest_msg_lock);
		}
	} else {
		g_is_scp_ready = 0;
	}
}

static int aoltest_core_handler(u32 msg_id, void *data, u32 sz)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	pr_info("[%s] Get msg_id: %d\n", __func__, msg_id);

	if (ctx->status == AOLTEST_INACTIVE) {
		pr_info("EM test ctx is inactive\n");
		return -1;
	}

	if (sz > MAX_BUF_LEN) {
		pr_notice("data size [%d] exceed expected [%d]", sz, MAX_BUF_LEN);
		return -2;
	}

	ret = _send_msg_to_scp(msg_id, sz, data);

	return ret;
}

static int aoltest_core_bind(void)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	memset(&g_aoltest_ctx, 0, sizeof(struct aoltest_core_ctx));

	// Create EM test thread
	ret = msg_thread_init(&ctx->msg_ctx, "em_test_thread",
					aoltest_core_opfunc, AOLTEST_OPID_MAX);

	if (ret) {
		pr_info("EM test thread init fail, ret=[%d]\n", ret);
		return -1;
	}

	ctx->drv_type = DRV_TYPE_EM;
	ctx->status = AOLTEST_ACTIVE;
	ctx->scp_test_cb.conap_scp_msg_notify_cb = aoltest_core_msg_notify;
	ctx->scp_test_cb.conap_scp_state_notify_cb = aoltest_core_state_change;

	ret = msg_thread_send(&ctx->msg_ctx, AOLTEST_OPID_SCP_REGISTER);

	if (ret)
		pr_info("[%s] Send to msg thread fail, ret=[%d]\n", __func__, ret);

	return ret;
}

static int aoltest_core_unbind(void)
{
	return 0;
}

int aoltest_core_init(void)
{
	struct netlink_event_cb nl_cb;

	mutex_init(&g_aoltest_msg_lock);

	mutex_init(&g_aol_data_buf_list.lock);
	INIT_LIST_HEAD(&g_aol_data_buf_list.list);

	// Init netlink
	nl_cb.aoltest_bind = aoltest_core_bind;
	nl_cb.aoltest_unbind = aoltest_core_unbind;
	nl_cb.aoltest_handler = aoltest_core_handler;

	return aoltest_netlink_init(&nl_cb);
}

void aoltest_core_deinit(void)
{
	pr_info("[%s]\n", __func__);
	aoltest_netlink_deinit();
}
