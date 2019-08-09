/*
 * Copyright (c) 2015-2016 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _TZ_DCIH_TEST_H_
#define _TZ_DCIH_TEST_H_

#define MAX_BUF_SIZE 512

enum {
	GREETING,
	BIT_OP_NOT,
};

struct dci_message {
	uint32_t cmd;
	uint8_t req_data[MAX_BUF_SIZE];
	uint8_t res_data[MAX_BUF_SIZE];
};

void start_dcih_notify_test(uint32_t driver_id);
int get_dcih_notify_test_result(void);
void start_dcih_wait_notify_test(uint32_t driver_id);
int get_dcih_wait_notify_test_result(void);

#endif
