/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __CCCI_CMD_CENTER_H__
#define __CCCI_CMD_CENTER_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "ccci_msg_id.h"
#include "ccci_msg_data.h"




extern
int	ccci_cmd_register(
		int msg_id,
		unsigned int cmd_id,
		void *my_data,
		int (*callback)(
			int msg_id,
			unsigned int cmd_id,
			void *cmd_data,
			void *my_data));

extern
int	ccci_cmd_unregister(
		int msg_id,
		unsigned int cmd_id,
		void *callback);

extern
int ccci_cmd_send(
		int msg_id,
		unsigned int cmd_id,
		void *msg_data);


#endif	/* __CCCI_CMD_CENTER_H__ */
