// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/alarmtimer.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "conap_scp.h"
#include "conap_scp_priv.h"
#include "conap_scp_test.h"


#define TEST_DATA_GUARD_PATN_1 0x5a5a5a5a
#define TEST_DATA_GUARD_PATN_2 0x5b5b5b5b

/*********************************************/
/* Golbal */
/*********************************************/


#define CONAP_SCP_TEST_INST_SZ 2
struct conap_scp_test_ctx {
	enum conap_scp_drv_type drv_type;
	char thread_name[64];
	struct task_struct *thread;
	struct conap_scp_drv_cb scp_test_cb;
};
//static struct conap_scp_test_ctx g_scp_test_ctx[CONAP_SCP_TEST_INST_SZ];
struct conap_scp_test_ctx *g_em_test_ctx = NULL;
struct conap_scp_test_ctx *g_gps_test_ctx = NULL;

struct test_data_1 {
	unsigned int param0;
	unsigned int param1;
	unsigned int param2;
	unsigned int param3;
};

struct test_data_2 {
	unsigned int param0;
	unsigned int param1;
	unsigned int param2;
	unsigned int param3;
	unsigned int param4;
};


/******************************************************/
/* WiFi */
/******************************************************/
enum conap_scp_msg_id {
	CHRETEST_MSG_ID_DEFAULT = 0,
	CHRETEST_MSG_ID_WIFI,
	CHRETEST_MSG_ID_BT,
	CHRETEST_MSG_ID_GPS,
	CHRETEST_MSG_ID_MAX
};


#define MAC_ADDR_LEN 			6
struct wlan_out_beacon_frame {
	uint16_t ucRcpiValue;
	uint8_t aucBSSID[MAC_ADDR_LEN];
	uint8_t ucChannel;
};

#define WF_SCAN_INFO_MAX_SZ  10
struct wlan_out_data {
	int result;
	int size;
	struct wlan_out_beacon_frame frame[WF_SCAN_INFO_MAX_SZ];
};


int conap_scp_test_send_msg(void);


static void error_handler(void)
{
	if (g_em_test_ctx)
		kthread_stop(g_em_test_ctx->thread);
	if (g_gps_test_ctx)
		kthread_stop(g_gps_test_ctx->thread);
}




/*********************************************/
/* callback functions */
/*********************************************/
void conap_scp_test_em_msg_notify_cb(unsigned int msg_id, unsigned int *buf, unsigned int size)
{
	struct wlan_out_data *data = NULL;
	int i;

	pr_info("[%s] msg_id=[%d]", __func__, msg_id);
	if (msg_id == CHRETEST_MSG_ID_WIFI) {
		data = (struct wlan_out_data*) buf;
		pr_info("[%s] result=[%d] size=[%d]", __func__, data->result, data->size);
		for (i = 0; i < data->size; i++) {
			pr_info("[%s] RCPI=[%d] [%x-%x-%x-%x-%x-%x]", __func__, data->frame[i].ucRcpiValue,
						data->frame[i].aucBSSID[0], data->frame[i].aucBSSID[1],
						data->frame[i].aucBSSID[2], data->frame[i].aucBSSID[3],
						data->frame[i].aucBSSID[4], data->frame[i].aucBSSID[5]);
		}
	}
#if 0
	struct test_data_1 *data;

	data = (struct test_data_1*)buf;
	pr_info("[%s] msg_id=[%d] [%x][%x][%x][%x]", __func__, msg_id,
				data->param0, data->param1, data->param2, data->param3);

	if (msg_id == 0) {
		if (data->param0 != TEST_DATA_GUARD_PATN_1 || data->param1 != 0x99776644 ||
			data->param2 != 0xff7766ff || data->param3 != TEST_DATA_GUARD_PATN_2) {
			pr_err("[%s] =ERROR= msgid=[%d] [%x][%x][%x][%x]", __func__, msg_id,
						data->param0, data->param1, data->param2, data->param3);
			error_handler();
		}
	} else {
	}
#endif
}

void conap_scp_test_em_state_change(int state)
{
	pr_info("[%s] reason=[%d]", __func__, state);
}



void conap_scp_test_gps_msg_notify_cb(uint32_t msg_id, uint32_t *buf, uint32_t size)
{
	pr_info("");
}

void conap_scp_test_gps_state_change(int state)
{
	if (state == 0) {
		error_handler();
	}
}


/*********************************************/
/* send msg functions */
/*********************************************/
int conap_scp_test_send_msg(void)
{
	int ret;
	struct test_data_1 c_test_data;

	c_test_data.param0 = 0xaabbccdd;
	c_test_data.param1 = 0x55667788;
	c_test_data.param2 = 0xaabbccdd;
	c_test_data.param3 = 0x55667788;

	ret = conap_scp_send_message(DRV_TYPE_EM, 0, (unsigned char*)&c_test_data, sizeof(struct test_data_1));
	pr_info("[%s] send msg=[%d]", __func__, ret);

	return ret;
}



int conap_spc_test_stress(int enable)
{
	//int ret;

#if 0
	if (enable) {
		//alarm_timer_init();
		//alarm_timer_fired();

	} else {
		return alarm_cancel(&g_alarm_timer);
	}
#endif

	//return ret;
	return 0;
}


int conap_spc_test_is_driver_ready(void)
{
	int ret;
	ret = conap_scp_is_drv_ready(DRV_TYPE_EM);
	pr_info("[%s] EM ready =[%d]", __func__, ret);

	ret = conap_scp_is_drv_ready(DRV_TYPE_GPS);
	pr_info("[%s] GPS ready =[%d]", __func__, ret);

	return 0;
}

int (*test_case_func)(struct conap_scp_test_ctx *ctx);

int test_case_func_1(struct conap_scp_test_ctx *ctx)
{
	int ret = 0;
	struct test_data_1 c_test_data;
	int drv_type;

	drv_type = ctx->drv_type;
	pr_info("[%s][Test-1] ctx=[%p] drv=[%d][%d] thread=[%x] 111 ",
				ctx->thread_name, ctx, ctx->drv_type, drv_type, ctx->thread);
	ret = conap_scp_is_drv_ready(ctx->drv_type);

	pr_info("[%s][Test-1] ctx=[%p] drv=[%d][%d] thread=[%x] 222 ",
				ctx->thread_name, ctx, ctx->drv_type, drv_type, ctx->thread);

	c_test_data.param0 = TEST_DATA_GUARD_PATN_1;
	c_test_data.param1 = ctx->drv_type;
	c_test_data.param2 = 0xaabbccdd;
	c_test_data.param3 = TEST_DATA_GUARD_PATN_1;

	ret = conap_scp_send_message(ctx->drv_type, 0, (unsigned char*)&c_test_data, sizeof(struct test_data_1));
	pr_info("[%s] send msg [%d]", ctx->thread_name, ret);

	return 0;
}

int test_case_func_2(struct conap_scp_test_ctx *ctx)
{
	int ret;
	struct test_data_2 c_test_data;

	ret = conap_scp_is_drv_ready(ctx->drv_type);
	pr_info("[%s][Test-2] drv=[%d] EM ready =[%d]", __func__, ctx->drv_type, ret);

	c_test_data.param0 = TEST_DATA_GUARD_PATN_2;
	c_test_data.param1 = ctx->drv_type;
	c_test_data.param2 = 0xaabbccdd;
	c_test_data.param3 = 0x55667788;
	c_test_data.param4 = TEST_DATA_GUARD_PATN_2;

	ret = conap_scp_send_message(ctx->drv_type, 1, (unsigned char*)&c_test_data, sizeof(struct test_data_2));
	pr_info("[%s] send msg [%d]", ctx->thread_name, ret);

	return 0;
}

/***********************************************************/
/* */
/***********************************************************/
struct em_test_info {
	uint32_t wifi_enabled;
	uint32_t wifi_scan_intvl;
	uint32_t wifi_cb_intvl;
	uint32_t bt_enabled;
	uint32_t bt_scan_intvl;
	uint32_t bt_cb_intvl;
	uint32_t gps_enabled;
	uint32_t gps_scan_intvl;
	uint32_t gps_cb_intvl;
};

enum em_commd_id {
	EM_MSG_DEFAULT = 0,
	EM_MSG_START_TEST = 1,
	EM_MSG_STOP_TEST = 2,
	EM_MSG_START_DATA_TRANS = 3,
	EM_MSG_STOP_DATA_TRANS = 4,
	EM_MSG_MAX_SIZE
};


int test_case_func_3(struct conap_scp_test_ctx *ctx)
{
	struct em_test_info test_info;
	int ret;

	ret = conap_scp_is_drv_ready(ctx->drv_type);
	pr_info("[%s][Test-3] drv=[%d] EM ready =[%d]", __func__, ctx->drv_type, ret);

	memset(&test_info, 0, sizeof(struct em_test_info));
#if 1
	test_info.wifi_enabled = 1;
	test_info.wifi_scan_intvl = 10; // 30 sec
	test_info.wifi_cb_intvl = 10;
#else

	test_info.bt_enabled = 1;
	test_info.bt_scan_intvl = 10; // 30 sec
	test_info.bt_cb_intvl = 10;
#endif

	ret = conap_scp_send_message(ctx->drv_type, EM_MSG_START_TEST,
				(unsigned char*)&test_info, sizeof(struct em_test_info));
	pr_info("[%s] send msg [%d]", ctx->thread_name, ret);
	return 0;
}

int test_case_func_4(struct conap_scp_test_ctx * ctx)
{
	int ret;

	ret = conap_scp_is_drv_ready(ctx->drv_type);
	pr_info("[%s][Test-4] drv=[%d] EM ready =[%d]", __func__, ctx->drv_type, ret);

	ret = conap_scp_send_message(ctx->drv_type, EM_MSG_STOP_TEST,
				NULL, 0);
	pr_info("[%s] send msg [%d]", ctx->thread_name, ret);
	return 0;
}

static int conap_scp_test_thread(void *pvData)
{
	int ret, loop = 0;
	struct conap_scp_test_ctx *ctx = (struct conap_scp_test_ctx*)pvData;

	BUG_ON(ctx == NULL);

	pr_info("[%s] init drv=[%d]", ctx->thread_name, ctx->drv_type);
	ret = conap_scp_register_drv(ctx->drv_type, &ctx->scp_test_cb);
	pr_info("[%s] scp register [%d] [%d]", ctx->thread_name, ret, ctx->drv_type);

	while (1) {
		if (kthread_should_stop())
			break;
		pr_info("[test_thread] loop =%d=", loop);
		msleep(3000);
		test_case_func_3(ctx);
		msleep(15000);
		//test_case_func_4(ctx);

		if (true) break;
#if 0
		msleep(3000);
		test_case_func_1(ctx);
		msleep(2000);
		test_case_func_2(ctx);
		msleep(5000);
		test_case_func_1(ctx);
		//break;
#endif
		loop++;
	}

	pr_info("[%s] thread stop!!", ctx->thread_name);
	return 0;
}


int conap_scp_test_init(void)
{
	//int i;
	struct conap_scp_test_ctx *test_ctx;
	//struct conap_scp_drv_cb scp_test_cb;

	/* ======= EM ====== */
	g_em_test_ctx = kmalloc(sizeof(struct conap_scp_test_ctx), GFP_KERNEL);
	if (g_em_test_ctx == NULL) {
		pr_err("[%s] malloc fail", __func__);
		return -1;
	}
	test_ctx = g_em_test_ctx;

	//for (int i = 0; i < CONAP_SCP_TEST_INST_SZ; i++) {
	//for (i = 0; i < 1; i++) {
		memset(test_ctx, 0, sizeof(struct conap_scp_test_ctx));
		test_ctx->drv_type = DRV_TYPE_EM;
		//memcpy(&test_ctx->scp_test_cb, &g_scp_test_cb, sizeof(struct conap_scp_drv_cb));
		test_ctx->scp_test_cb.conap_scp_msg_notify_cb = conap_scp_test_em_msg_notify_cb;
		test_ctx->scp_test_cb.conap_scp_state_notify_cb = conap_scp_test_em_state_change;

		snprintf(test_ctx->thread_name, 64, "conap_test_em");

		test_ctx->thread = kthread_create(conap_scp_test_thread, test_ctx,
									test_ctx->thread_name);

		if (IS_ERR(test_ctx->thread)) {
			kfree(g_em_test_ctx);
			return -1;
		}
		wake_up_process(test_ctx->thread);
	//}

#if 0
	/* ======= GPS ====== */
	g_gps_test_ctx = kmalloc(sizeof(struct conap_scp_test_ctx), GFP_KERNEL);
	if (g_gps_test_ctx == NULL) {
		pr_err("[%s] malloc fail", __func__);
		return -1;
	}
	test_ctx = g_gps_test_ctx;
	memset(test_ctx, 0, sizeof(struct conap_scp_test_ctx));
	test_ctx->drv_type = DRV_TYPE_GPS;
	test_ctx->scp_test_cb.conap_scp_msg_notify_cb = conap_scp_test_gps_msg_notify_cb;
	test_ctx->scp_test_cb.conap_scp_state_notify_cb = conap_scp_test_gps_state_change;
	snprintf(test_ctx->thread_name, 64, "conap_test_gps");
	test_ctx->thread = kthread_create(conap_scp_test_thread, test_ctx,
									test_ctx->thread_name);

	if (IS_ERR(test_ctx->thread)) {
		kfree(g_gps_test_ctx);
		return -1;
	}
	wake_up_process(test_ctx->thread);
#endif
	return 0;
}

int conap_scp_test_deinit(void)
{
	conap_scp_deinit();

	if (g_em_test_ctx)
		kfree(g_em_test_ctx);

	if (g_gps_test_ctx)
		kfree(g_gps_test_ctx);
	return 0;
}
