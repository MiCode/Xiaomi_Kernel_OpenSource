// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
