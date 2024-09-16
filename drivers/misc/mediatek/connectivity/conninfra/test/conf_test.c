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

#include "conninfra_conf.h"
#include "conf_test.h"

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


int conninfra_conf_test(void)
{
	int ret;
	const struct conninfra_conf *conf;

#if 0
	ret = conninfra_conf_set_cfg_file("WMT_SOC.cfg");
	if (ret) {
		pr_err("set cfg file fail [%d]", ret);
		return -1;
	}
#endif

	ret = conninfra_conf_init();
	if (ret) {
		pr_err("int conf fail [%d]", ret);
		return -1;
	}

	conf = conninfra_conf_get_cfg();
	if (NULL == conf) {
		pr_err("int conf fail [%d]", ret);
		return -1;
	}
	if (conf->tcxo_gpio != 0) {
		pr_err("test tcxo gpio fail [%d]. For most case, it should be 0.",
			conf->tcxo_gpio);
		return -1;
	}

	pr_info("[%s] test PASS\n", __func__);
	return 0;
}


