/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef VCP_ERR_INFO_H
#define VCP_ERR_INFO_H


#include <linux/types.h>        /* for uint32_t */


/******************************************************************************
 ******************************************************************************/
#define ERR_MAX_CONTEXT_LEN     32


/******************************************************************************
 * VCP side uses the data types err_case_id_t and err_sensor_id_t which can be
 * regarded as uint32_t.
 ******************************************************************************/
struct error_info {
	uint32_t case_id;
	uint32_t sensor_id;
	char context[ERR_MAX_CONTEXT_LEN];
};

#endif  // VCP_ERR_INFO_H

