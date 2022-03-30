/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
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

extern int send_sig(int sig, struct task_struct *p, int priv);

#endif
