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
#include "conninfra_core.h"
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

int pre_cal_power_on_handler(void)
{
	pr_info("[%s] ===========", __func__);
	osal_sleep_ms(100);
	return 0;
}

int pre_cal_do_cal_handler(void)
{
	pr_info("[%s] ===========", __func__);
	return 0;
}


struct sub_drv_ops_cb g_cal_drv_ops_cb;

int calibration_test(void)
{
	int ret;

	memset(&g_cal_drv_ops_cb, 0, sizeof(struct sub_drv_ops_cb));

	g_cal_drv_ops_cb.pre_cal_cb.pwr_on_cb = pre_cal_power_on_handler;
	g_cal_drv_ops_cb.pre_cal_cb.do_cal_cb = pre_cal_do_cal_handler;


	pr_info("[%s] cb init [%p][%p]", __func__,
				g_cal_drv_ops_cb.pre_cal_cb.pwr_on_cb,
				g_cal_drv_ops_cb.pre_cal_cb.do_cal_cb);

	conninfra_sub_drv_ops_register(CONNDRV_TYPE_BT, &g_cal_drv_ops_cb);
	conninfra_sub_drv_ops_register(CONNDRV_TYPE_WIFI, &g_cal_drv_ops_cb);

	ret = conninfra_core_pre_cal_start();
	if (ret)
		pr_warn("[%s] fail [%d]", __func__, ret);

	osal_sleep_ms(1000);

	conninfra_core_screen_on();

	conninfra_sub_drv_ops_unregister(CONNDRV_TYPE_BT);
	conninfra_sub_drv_ops_unregister(CONNDRV_TYPE_WIFI);
	pr_info("[%s] finish.", __func__);
	return 0;
}


