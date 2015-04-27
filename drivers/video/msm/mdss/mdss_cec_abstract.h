/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MDSS_CEC_ABSTRACT_H__
#define __MDSS_CEC_ABSTRACT_H__

#define MAX_OPERAND_SIZE	15

struct cec_msg {
	u8 sender_id;
	u8 recvr_id;
	u8 opcode;
	u8 operand[MAX_OPERAND_SIZE];
	u8 frame_size;
	u8 retransmit;
};

struct cec_ops {
	int (*enable)(void *data);
	int (*disable)(void *data);
	int (*send_msg)(void *data,
		struct cec_msg *msg);
	void (*wt_logical_addr)(void *data, u8 addr);
	int (*wakeup_en)(void *data, bool en);
	void *data;
};

struct cec_cbs {
	int (*msg_recv_notify)(void *data, struct cec_msg *msg);
	void *data;
};

struct cec_data {
	u32 id;
	struct cec_ops *ops;
	struct cec_cbs *cbs;
	struct kobject *sysfs_kobj;
};

int cec_abstract_init(struct cec_data *init_data);
int cec_abstract_deinit(void *input);
#endif /* __MDSS_CEC_ABSTRACT_H_*/
