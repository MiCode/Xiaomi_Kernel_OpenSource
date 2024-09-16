/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _CONNSYS_DEBUG_UTILITY_H_
#define _CONNSYS_DEBUG_UTILITY_H_

#include <linux/types.h>
#include <linux/compiler.h>

#include "coredump/connsys_coredump.h"
#include "connsyslog/connsyslog.h"

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

enum CONN_DEBUG_TYPE {
	CONN_DEBUG_TYPE_WIFI = 0,
	CONN_DEBUG_TYPE_BT = 1,
	CONN_DEBUG_TYPE_END,
};

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/


#endif /*_CONNSYS_DEBUG_UTILITY_H_*/
