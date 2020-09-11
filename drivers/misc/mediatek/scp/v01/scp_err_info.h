/*
 * Copyright (C) 2018 MediaTek Inc.
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

