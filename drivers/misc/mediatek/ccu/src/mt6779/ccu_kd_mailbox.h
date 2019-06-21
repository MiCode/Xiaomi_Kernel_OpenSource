/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __CCU_MAILBOX_H__
#define __CCU_MAILBOX_H__

#include "ccu_ext_interface/ccu_mailbox_extif.h"

enum mb_result {
		MAILBOX_OK = 0,
		MAILBOX_QUEUE_FULL,
		MAILBOX_QUEUE_EMPTY
};

enum mb_result mailbox_init(struct ccu_mailbox_t *apmcu_mb_addr,
	struct ccu_mailbox_t *ccu_mb_addr);

enum mb_result mailbox_send_cmd(struct ccu_msg *task);

enum mb_result mailbox_receive_cmd(struct ccu_msg *task);

#endif
