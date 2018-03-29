/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __GED_ERROR_H__
#define __GED_ERROR_H__

typedef enum GED_ERROR_TAG
{
	GED_OK,
	GED_ERROR_FAIL,
	GED_ERROR_OOM,
	GED_ERROR_OUT_OF_FD,
	GED_ERROR_FAIL_WITH_LIMIT,
	GED_ERROR_TIMEOUT,
	GED_ERROR_CMD_NOT_PROCESSED,
	GED_ERROR_INVALID_PARAMS,
	GED_INTENTIONAL_BLOCK
} GED_ERROR;


#endif
