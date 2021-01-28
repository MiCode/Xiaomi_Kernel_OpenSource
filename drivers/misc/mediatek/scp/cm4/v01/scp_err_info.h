/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef SCP_ERR_INFO_H
#define SCP_ERR_INFO_H


#include <linux/types.h>        /* for uint32_t */


/******************************************************************************
 ******************************************************************************/
#define ERR_MAX_CONTEXT_LEN     32


/******************************************************************************
 * SCP side uses the data types err_case_id_t and err_sensor_id_t which can be
 * regarded as uint32_t.
 ******************************************************************************/
struct error_info {
	uint32_t case_id;
	uint32_t sensor_id;
	char context[ERR_MAX_CONTEXT_LEN];
};


__attribute__((weak)) void report_hub_dmd(uint32_t case_id, uint32_t sensor_id,
						char *context);


#endif  // SCP_ERR_INFO_H

