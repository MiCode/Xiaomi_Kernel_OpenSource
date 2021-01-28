/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __CCU_MAILBOX_H__
#define __CCU_MAILBOX_H__

#include "ccu_ext_interface/ccu_mailbox_extif.h"

enum mb_result {
		MAILBOX_OK = 0,
		MAILBOX_QUEUE_FULL,
		MAILBOX_QUEUE_EMPTY,
		MAILBOX_UNINIT
};

enum mb_result mailbox_init(struct ccu_mailbox_t *apmcu_mb_addr,
	struct ccu_mailbox_t *ccu_mb_addr);

enum mb_result mailbox_send_cmd(struct ccu_msg_t *task);

enum mb_result mailbox_receive_cmd(struct ccu_msg_t *task);

#endif
