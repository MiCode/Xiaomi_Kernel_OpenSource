// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define pr_fmt(fmt) "dump_test@(%s:%d) " fmt, __func__, __LINE__

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include <linux/delay.h>
#include <linux/printk.h>

#include "conninfra.h"
#include "connsys_debug_utility.h"

#include "coredump_test.h"
#include "coredump/conndump_netlink.h"

#include "msg_thread.h"


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/


static int coredump_test_init(void);
static int coredump_test_start(void);
static int coredump_test_deinit(void);

static int dump_test_reg_readable(void);
static void dump_test_poll_cpu_pcr(unsigned int times, unsigned int sleep);

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static void* g_dumpCtx = 0;
static int nl_init = 0;
struct msg_thread_ctx g_msg_ctx;

static int init = 0;

typedef enum {
	DUMP_TEST_OPID_1 = 0,
	DUMP_TEST_OPID_2 = 1,
	DUMP_TEST_OPID_MAX
} test_opid;


static int opfunc_test_1(struct msg_op_data *op);
static int opfunc_test_2(struct msg_op_data *op);

static const msg_opid_func test_op_func[] = {
	[DUMP_TEST_OPID_1] = opfunc_test_1,
	[DUMP_TEST_OPID_2] = opfunc_test_2,
};

struct coredump_event_cb g_cb = {
	.reg_readable = dump_test_reg_readable,
	.poll_cpupcr = dump_test_poll_cpu_pcr,
};

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

static int dump_test_reg_readable(void)
{
	return 1;
}

static void dump_test_poll_cpu_pcr(unsigned  int times, unsigned int sleep)
{
	pr_info("[%s]\n", __func__);
}

#define TEST_STRING	"this is coredump test"

static int coredump_test_start(void)
{

	if (!g_dumpCtx) {
		pr_err("coredump contxt is not init\n");
		return 1;
	}
#if 0
	pr_info("[%s] tc[0] test case\n", __func__);
	/* Trigger interrupt */
	connsys_coredump_setup_tc(g_dumpCtx, 0);

	/* Start to dump */
	connsys_coredump_start(g_dumpCtx, 0);

	pr_info("[%s] sleep\n", __func__);
	msleep(5000);

	pr_info("[%s] tc[1] test case\n", __func__);
	/* Trigger interrupt */
	connsys_coredump_setup_tc(g_dumpCtx, 1);
	/* Start to dump */
	connsys_coredump_start(g_dumpCtx, 0);

	pr_info("[%s] tc[2] test case\n", __func__);
	/* Trigger interrupt */
	connsys_coredump_setup_tc(g_dumpCtx, 2);
	/* Start to dump */
	connsys_coredump_start(g_dumpCtx, 0);
#endif

	pr_info("[%s] end\n", __func__);
	return 0;

}

static int coredump_nl_start(void)
{
	int ret;


	ret = conndump_netlink_send_to_native(CONN_DEBUG_TYPE_WIFI, "[M]", TEST_STRING, osal_strlen(TEST_STRING));
	pr_info("send ret=%d", ret);
	ret = conndump_netlink_send_to_native(CONN_DEBUG_TYPE_WIFI, "[EMI]", NULL, 0);
	pr_info("send emi dump ret=%d", ret);
	return 0;
}

static int coredump_test_init(void)
{
	unsigned int emi_addr = 0;
	unsigned int emi_size = 0;

	if (g_dumpCtx) {
		pr_err("log handler has been init. Need to deinit.\n");
		return 1;
	}

	conninfra_get_phy_addr(&emi_addr, &emi_size);
	if (!emi_addr || !emi_size) {
		pr_err("EMI init fail.\n");
		return 2;
	}

	pr_info("emi_addr=0x%08x, size=%d", emi_addr, emi_size);

	g_dumpCtx = connsys_coredump_init(
		CONN_DEBUG_TYPE_WIFI, &g_cb);

	if (!g_dumpCtx) {
		pr_err("Coredump init fail.\n");
		return 3;
	}

	return 0;

}

static void coredump_end_cb(void* ctx) {
	pr_info("Get dump end");
}

static int coredump_nl_init(void)
{
	int ret;
	struct netlink_event_cb nl_cb;

	if (!nl_init) {
		nl_cb.coredump_end = coredump_end_cb;
		ret = conndump_netlink_init(CONN_DEBUG_TYPE_WIFI, NULL, &nl_cb);
		pr_info("init get %d", ret);
		nl_init = 1;
	}
	return 0;
}

static int coredump_test_deinit(void)
{
	if (!g_dumpCtx) {
		pr_err("coredump contxt is not init\n");
		return 1;
	}

	connsys_coredump_deinit(g_dumpCtx);
	return 0;
}

static int coredump_nl_deinit(void)
{
	nl_init = 0;
	return 0;
}

int opfunc_test_1(struct msg_op_data *op)
{
	int tc = op->op_data[0];
	int ret = 0;

	pr_info("[%s] param=[%d]", __func__, tc);
	switch (tc) {
		case 0:
			ret = coredump_test_init();
			break;
		case 1:
			ret = coredump_test_start();
			break;
		case 2:
			ret = coredump_test_deinit();
			break;
	}
	pr_info("ret = %d", ret);
	return 0;
}

int opfunc_test_2(struct msg_op_data *op)
{
	int tc = op->op_data[0];
	int ret = 0;

	pr_info("[%s] param=[%d]", __func__, tc);

	switch (tc) {
		case 0:
			ret = coredump_nl_init();
			break;
		case 1:
			ret = coredump_nl_start();
			break;
		case 2:
			ret = coredump_nl_deinit();
			break;
	}
	pr_info("ret = %d", ret);
	return 0;
}

int coredump_test(int par1, int par2, int par3)
{
	pr_info("Disable coredump test in SQC\n");
	init = 1;
	return 0;
#if 0
	int ret = 0;

	if (init == 0) {
		ret = msg_thread_init(
			&g_msg_ctx, "DumptestThread", test_op_func, DUMP_TEST_OPID_MAX);
		init = 1;
	}
	if (par2 == 0xff && par3 == 0xff && init) {
		pr_info("End test");
		msg_thread_deinit(&g_msg_ctx);
		init = 0;
	}

	msg_thread_send_1(&g_msg_ctx, par2, par3);

	return 0;
#endif
}

