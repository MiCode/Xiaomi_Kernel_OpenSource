/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __CCCI_MSG_CENTER_H__
#define __CCCI_MSG_CENTER_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "ccci_msg_id.h"
#include "ccci_msg_data.h"



extern
int	ccci_msg_register(
		int msg_id,
		unsigned int sub_id,
		void *my_data,
		int (*callback)(
			int msg_id,
			unsigned int sub_id,
			void *msg_data,
			void *my_data));

extern
int	ccci_msg_unregister(
		int msg_id,
		unsigned int sub_id,
		void *callback);

/* no compare sub_id, think the msg_id is one-to-first */
extern
int ccci_msg_send_to_first(
		int msg_id,
		unsigned int sub_id,
		void *msg_data);

/* no compare sub_id, think the msg_id is one-to-one */
extern
int ccci_msg_send_to_one(
		int msg_id,
		unsigned int sub_id,
		void *msg_data);

/* bit compare sub_id, think the msg_id is one-to-bit */
extern
int ccci_msg_send_to_bit(
		int msg_id,
		unsigned int sub_id,
		void *msg_data);

/* bit compare sub_id, think the msg_id is one-to-more */
extern
int ccci_msg_send(
		int msg_id,
		unsigned int sub_id,
		void *msg_data);




#endif	/* __CCCI_MSG_CENTER_H__ */
