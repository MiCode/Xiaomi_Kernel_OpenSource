/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_SAMPLE_DRV_H__
#define __APUSYS_SAMPLE_DRV_H__

#define SAMPLE_REQUEST_NAME_SIZE 32

struct sample_request {
	char name[SAMPLE_REQUEST_NAME_SIZE];
	uint32_t algo_id;
	uint32_t delay_ms;


	uint8_t driver_done;
};

#endif
