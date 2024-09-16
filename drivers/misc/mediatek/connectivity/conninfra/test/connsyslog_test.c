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
#define pr_fmt(fmt) "connlog_test@(%s:%d) " fmt, __func__, __LINE__

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include <linux/printk.h>

#include "conninfra.h"
#include "connsys_debug_utility.h"

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
static void connlog_test_event_handler(void);

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

#define TEST_LOG_BUF_SIZE	1*2014

static char test_buf[TEST_LOG_BUF_SIZE];

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

static void connlog_test_event_handler(void)
{
	ssize_t read = 0;
	pr_info("connlog_test_event_handler\n");

	read = connsys_log_read(CONN_DEBUG_TYPE_WIFI, test_buf, TEST_LOG_BUF_SIZE);
	pr_info("Read %u:\n", read);
	connsys_log_dump_buf("log_test", test_buf, read);
	pr_info("============================================\n");
}

int connlog_test_init(void)
{
	unsigned int emi_addr = 0;
	unsigned int emi_size = 0;
	int ret;

	conninfra_get_phy_addr(&emi_addr, &emi_size);
	if (!emi_addr || !emi_size) {
		pr_err("EMI init fail.\n");
		return 1;
	}

	ret = connsys_dedicated_log_path_apsoc_init(emi_addr);
	if (ret) {
		pr_err("connsys_dedicated_log_path_apsoc_init should fail\n");
		return 2;
	} else {
		pr_info("connsys_dedicated_log_path_apsoc_init return fail as expection\n");
	}

	ret = connsys_log_init(CONN_DEBUG_TYPE_WIFI);
	if (ret) {
		pr_err("Init connsys log failed\n");
		return 3;
	}
	connsys_log_register_event_cb(CONN_DEBUG_TYPE_WIFI, connlog_test_event_handler);
	return 0;
}

int connlog_test_read(void)
{
	connsys_log_irq_handler(CONN_DEBUG_TYPE_WIFI);
	return 0;
}

int connlog_test_deinit(void)
{
	connsys_log_deinit(CONN_DEBUG_TYPE_WIFI);
	return 0;
}

