/*
 * Copyright (C) 2016 MediaTek Inc.
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
/*! \file
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/

#define pr_fmt(fmt) "msg_evt_test@(%s:%d) " fmt, __func__, __LINE__

#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include "msg_thread.h"
#include "msg_evt_test.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

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


/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

struct work_struct rst_worker;
struct msg_thread_ctx g_ctx;

typedef enum {
	INFRA_TEST_OPID_1 = 0,
	INFRA_TEST_OPID_2 = 1,
	INFRA_TEST_OPID_MAX
} test_opid;


static int opfunc_test_1(struct msg_op_data *op);
static int opfunc_test_2(struct msg_op_data *op);

static const msg_opid_func test_op_func[] = {
	[INFRA_TEST_OPID_1] = opfunc_test_1,
	[INFRA_TEST_OPID_2] = opfunc_test_2,
};

int opfunc_test_1(struct msg_op_data *op)
{
	pr_info("[%s]", __func__);
	return 0;
}

int opfunc_test_2(struct msg_op_data *op)
{
	unsigned int drv_type = op->op_data[0];

	pr_info("[%s] param=[%d]", __func__, drv_type);
	return 0;
}


static void msg_thrd_handler(struct work_struct *work)
{
	msg_thread_send_1(&g_ctx, INFRA_TEST_OPID_2, 2011);
	osal_sleep_ms(5);
	msg_thread_send_wait_1(&g_ctx, INFRA_TEST_OPID_2, 0, 2022);
	msg_thread_send_wait_1(&g_ctx, INFRA_TEST_OPID_2, 0, 2033);
}

int msg_evt_test(void)
{
	int ret;

	INIT_WORK(&rst_worker, msg_thrd_handler);

	ret = msg_thread_init(&g_ctx, "TestThread",
					test_op_func, INFRA_TEST_OPID_MAX);
	if (ret) {
		pr_err("inti msg_thread fail ret=[%d]\n", ret);
		return -2;
	}

	schedule_work(&rst_worker);

	msg_thread_send_wait_1(&g_ctx, INFRA_TEST_OPID_2, 0, 1011);
	//osal_sleep_ms(10);
	msg_thread_send_1(&g_ctx, INFRA_TEST_OPID_2, 1022);
	osal_sleep_ms(10);
	msg_thread_send_wait_1(&g_ctx, INFRA_TEST_OPID_2, 0, 1033);

	osal_sleep_ms(1000);

	pr_info("<<<<<>>>>>>> freeOpq=[%u][%u] ActiveQ=[%u][%u]",
			g_ctx.free_op_q.write, g_ctx.free_op_q.read,
			g_ctx.active_op_q.write, g_ctx.active_op_q.read);
	osal_sleep_ms(500);

	ret = msg_thread_deinit(&g_ctx);
	pr_info("[%s] msg_thread_deinit\n", __func__);

	pr_info("[%s] test PASS\n", __func__);
	return 0;
}


