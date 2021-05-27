// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
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
#include "aoltest_message_builder.h"
#include "aoltest_ring_buffer.h"

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
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

struct aoltest_core_ctx {
	enum conap_scp_drv_type drv_type;
	struct msg_thread_ctx msg_ctx;
	struct conap_scp_drv_cb scp_test_cb;
	int status;
};

struct aoltest_core_ctx g_aoltest_ctx;
struct aoltest_core_rb g_rb;

TEST_INFO g_test_info;
static unsigned char g_is_scp_ready = 0;
static bool g_is_test_started = false;

/*******************************************************************************
*                             M A C R O S
********************************************************************************
*/
#define BUFF_SIZE (32 * 1024 * sizeof(char))

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static int opfunc_scp_register(struct msg_op_data *op);
static int opfunc_scp_unregister(struct msg_op_data *op);
static int opfunc_send_msg(struct msg_op_data *op);
static int opfunc_recv_msg(struct msg_op_data *op);

void aoltest_core_state_change(int state);
void aoltest_core_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size);

static const msg_opid_func aoltest_core_opfunc[] = {
	[AOLTEST_OPID_SCP_REGISTER] = opfunc_scp_register,
	[AOLTEST_OPID_SCP_UNREGISTER] = opfunc_scp_unregister,
	[AOLTEST_OPID_SEND_MSG] = opfunc_send_msg,
	[AOLTEST_OPID_RECV_MSG] = opfunc_recv_msg,
};

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
static int is_scp_ready()
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

/******************************************************
*      O P          F U N C T I O N S
*******************************************************
*/
static int opfunc_scp_register(struct msg_op_data *op)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	ret = conap_scp_register_drv(ctx->drv_type, &ctx->scp_test_cb);
	pr_info("SCP register drv_type=[%d], ret=[%d]", ctx->drv_type, ret);

	// Create ring buffer
	aoltest_core_rb_init(&g_rb);

	return ret;
}

static int opfunc_scp_unregister(struct msg_op_data *op)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	ret = conap_scp_unregister_drv(ctx->drv_type);
	pr_info("SCP unregister drv_type=[%d], ret=[%d]", ctx->drv_type, ret);

	// Destroy ring buffer
	aoltest_core_rb_deinit(&g_rb);

	return ret;
}

static int opfunc_send_msg(struct msg_op_data *op)
{
	int ret = 0;
	unsigned int cmd;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	if (!g_is_scp_ready) {
		 ret = is_scp_ready();
		 pr_info("[%s] is ready=[%d] ret=[%d]", __func__, g_is_scp_ready, ret);

		 if (ret <= 0)
			 return -1;
	 }

	cmd = (unsigned int)op->op_data[0];

	if (cmd == AOLTEST_CMD_START_TEST) {
		g_is_test_started = true;
		ret = conap_scp_send_message(ctx->drv_type, cmd,
								(unsigned char*)&g_test_info, sizeof(TEST_INFO));
	} else {
		if (cmd == AOLTEST_CMD_STOP_TEST) {
			g_is_test_started = false;
		}

		ret = conap_scp_send_message(ctx->drv_type, cmd, NULL, 0);
	}

	pr_info("Send drv_type=[%d], cmd=[%d], ret=[%d]\n", ctx->drv_type, cmd, ret);
	return ret;
}

static int opfunc_recv_msg(struct msg_op_data *op)
{
	int ret = 0;
	int type = -1;
	char buf[MAX_BUF_LEN];

	unsigned int msg_id = (unsigned int)op->op_data[0];
	memset(buf, '\0', sizeof(buf));

	type = aoltest_pop_message(&g_rb, buf);

	if (type <= 0) {
		pr_info("Pop message fail\n");
		return -1;
	}

	pr_info("Send to netlink client, buf=[%s]\n", buf);
	aoltest_netlink_send_to_native("[AOLTEST]", msg_id, buf, (strlen(buf) + 1));

	return ret;
}

/******************************************************
*      C H R E T E S T     F U N C T I O N S
*******************************************************
*/
void aoltest_core_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;
	unsigned int expect_size = 0;

	if (ctx->status == AOLTEST_INACTIVE) {
		pr_info("EM test ctx is inactive\n");
		return;
	}

	if (msg_id == AOLTEST_MSG_ID_WIFI) {
		expect_size = sizeof(struct aoltest_wifi_raw_data);
	} else if (msg_id == AOLTEST_MSG_ID_BT) {
		expect_size = sizeof(struct aoltest_bt_raw_data);
	} else if (msg_id == AOLTEST_MSG_ID_GPS) {
		expect_size = sizeof(struct aoltest_gps_raw_data);
	}

	if (expect_size != size) {
		pr_info("[%s] Buf size is unexpected, msg_id=[%u], expect size=[%u], recv size=[%u]\n",
			__func__, msg_id, expect_size, size);
		return;
	}

	aoltest_push_message(&g_rb, msg_id, buf);

	ret = msg_thread_send_1(&ctx->msg_ctx, AOLTEST_OPID_RECV_MSG, msg_id);

	if (ret) {
		pr_info("[%s] Notify recv msg fail, ret=[%d]\n", __func__, ret);
	}
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
			ret = msg_thread_send_1(&ctx->msg_ctx, AOLTEST_OPID_SEND_MSG, AOLTEST_CMD_START_TEST);
		}
	} else {
		g_is_scp_ready = 0;
	}
}

static int aoltest_core_handler(int cmd, void* data)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	if (ctx->status == AOLTEST_INACTIVE) {
		pr_info("EM test ctx is inactive\n");
		return -1;
	}

	if (cmd == AOLTEST_CMD_START_TEST) {
		g_test_info = *((TEST_INFO*)data);
	}

	ret = msg_thread_send_1(&ctx->msg_ctx, AOLTEST_OPID_SEND_MSG, cmd);

	if (ret) {
		pr_info("[%s] Send to msg thread fail, ret=[%d]\n", __func__, ret);
	}

	return ret;
}

static int aoltest_core_bind(void)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;

	memset(&g_aoltest_ctx, 0, sizeof(struct aoltest_core_ctx));

	// Create EM test thread
	ret = msg_thread_init(&ctx->msg_ctx, "em_test_thread", aoltest_core_opfunc, AOLTEST_OPID_MAX);

	if (ret) {
		pr_info("EM test thread init fail, ret=[%d]\n", ret);
		return -1;
	}

	ctx->drv_type = DRV_TYPE_EM;
	ctx->status = AOLTEST_ACTIVE;
	ctx->scp_test_cb.conap_scp_msg_notify_cb = aoltest_core_msg_notify;
	ctx->scp_test_cb.conap_scp_state_notify_cb = aoltest_core_state_change;

	ret = msg_thread_send(&ctx->msg_ctx, AOLTEST_OPID_SCP_REGISTER);

	if (ret) {
		pr_info("[%s] Send to msg thread fail, ret=[%d]\n", __func__, ret);
	}

	return ret;
}

static int aoltest_core_unbind(void)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltest_ctx;
	ctx->status = AOLTEST_INACTIVE;

	ret = msg_thread_send_wait(&ctx->msg_ctx, AOLTEST_OPID_SCP_UNREGISTER, 0);

	if (ret) {
		pr_info("SCP unregister fail\n");
		return -1;
	}

	// Delete EM test thread
	ret = msg_thread_deinit(&ctx->msg_ctx);

	if (ret) {
		pr_info("EM test thread deinit fail, ret=[%d]\n", ret);
		return -1;
	}

	return ret;
}

int aoltest_core_init(void)
{
	struct netlink_event_cb nl_cb;

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
