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

#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include "conninfra.h"
#include "osal.h"

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

int pre_chip_rst_handler(enum consys_drv_type drv, char* reason)
{
	pr_info("[%s] ===========", __func__);
	osal_sleep_ms(100);
	return 0;
}

int post_chip_rst_handler(void)
{
	pr_info("[%s] ===========", __func__);
	return 0;
}

int pre_chip_rst_timeout_handler(enum consys_drv_type drv, char* reason)
{
	pr_info("[%s] ++++++++++++", __func__);
	osal_sleep_ms(800);
	pr_info("[%s] ------------", __func__);
	return 0;
}

struct sub_drv_ops_cb g_drv_ops_cb;
struct sub_drv_ops_cb g_drv_timeout_ops_cb;

int chip_rst_test(void)
{
	int ret;

	memset(&g_drv_ops_cb, 0, sizeof(struct sub_drv_ops_cb));
	memset(&g_drv_timeout_ops_cb, 0, sizeof(struct sub_drv_ops_cb));

	g_drv_ops_cb.rst_cb.pre_whole_chip_rst = pre_chip_rst_handler;
	g_drv_ops_cb.rst_cb.post_whole_chip_rst = post_chip_rst_handler;

	g_drv_timeout_ops_cb.rst_cb.pre_whole_chip_rst = pre_chip_rst_timeout_handler;
	g_drv_timeout_ops_cb.rst_cb.post_whole_chip_rst = post_chip_rst_handler;

	pr_info("[%s] cb init [%p][%p]", __func__,
				g_drv_ops_cb.rst_cb.pre_whole_chip_rst,
				g_drv_ops_cb.rst_cb.post_whole_chip_rst);

	conninfra_sub_drv_ops_register(CONNDRV_TYPE_BT, &g_drv_ops_cb);
	conninfra_sub_drv_ops_register(CONNDRV_TYPE_WIFI, &g_drv_ops_cb);
	conninfra_sub_drv_ops_register(CONNDRV_TYPE_FM, &g_drv_timeout_ops_cb);

	pr_info("[%s] ++++++++++++++++++++++", __func__);

	ret = conninfra_trigger_whole_chip_rst(CONNDRV_TYPE_BT, "test reset");
	if (ret)
		pr_warn("[%s] fail [%d]", __func__, ret);
	else
		pr_info("Trigger chip reset success. Test pass.");
	osal_sleep_ms(10);

	pr_info("Try to trigger whole chip reset when reset is ongoing. It should be fail.");
	ret = conninfra_trigger_whole_chip_rst(CONNDRV_TYPE_BT, "test reset");
	pr_info("Test %s. ret = %d.", ret == 1? "pass": "fail", ret);

	pr_info("Try to funcion on when reset is ongoing. It should be fail.");
	ret = conninfra_pwr_on(CONNDRV_TYPE_WIFI);
	pr_info("Test %s. ret = %d.", ret == CONNINFRA_ERR_RST_ONGOING ? "pass": "fail", ret);

	osal_sleep_ms(3000);

	conninfra_sub_drv_ops_unregister(CONNDRV_TYPE_BT);
	conninfra_sub_drv_ops_unregister(CONNDRV_TYPE_WIFI);
	pr_info("chip_rst_test finish");
	return 0;
}


