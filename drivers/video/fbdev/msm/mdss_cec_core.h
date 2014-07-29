/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_CEC_CORE_H__
#define __MDSS_CEC_CORE_H__

#define MAX_OPERAND_SIZE	14

/* total size:  HEADER block (1) + opcode block (1) + operands (14) */
#define MAX_CEC_FRAME_SIZE      (MAX_OPERAND_SIZE + 2)

/* CEC message set */
#define CEC_MSG_SET_OSD_STRING		0x64
#define CEC_MSG_GIVE_PHYS_ADDR		0x83
#define CEC_MSG_ABORT			0xFF
#define CEC_MSG_GIVE_OSD_NAME		0x46
#define CEC_MSG_GIVE_POWER_STATUS	0x8F
#define CEC_MSG_ROUTE_CHANGE_CMD	0x80
#define CEC_MSG_SET_STREAM_PATH		0x86
#define CEC_MSG_USER_CTRL_PRESS		0x44
#define CEC_MSG_USER_CTRL_RELEASE	0x45

/**
 * struct cec_msg - CEC message related data
 * @sender_id: CEC message initiator's id
 * @recvr_id: CEC message destination's id
 * @opcode: CEC message opcode
 * @operand: CEC message operands corresponding to opcode
 * @frame_size: total CEC frame size
 * @retransmit: number of re-tries to transmit message
 *
 * Basic CEC message structure used by both client and driver.
 */
struct cec_msg {
	u8 sender_id;
	u8 recvr_id;
	u8 opcode;
	u8 operand[MAX_OPERAND_SIZE];
	u8 frame_size;
	u8 retransmit;
};

/**
 * struct cec_ops - CEC operations function pointers
 * @enable: function pointer to enable CEC
 * @send_msg: function pointer to send CEC message
 * @wt_logical_addr: function pointer to write logical address
 * @wakeup_en: function pointer to enable wakeup feature
 * @is_wakeup_en: function pointer to query wakeup feature state
 * @device_suspend: function pointer to update device suspend state
 * @data: pointer to the data needed to send with operation functions
 *
 * Defines all the operations that abstract module can call
 * to programe the CEC driver.
 */
struct cec_ops {
	int (*enable)(void *data, bool enable);
	int (*send_msg)(void *data,
		struct cec_msg *msg);
	void (*wt_logical_addr)(void *data, u8 addr);
	void (*wakeup_en)(void *data, bool en);
	bool (*is_wakeup_en)(void *data);
	void (*device_suspend)(void *data, bool suspend);
	void *data;
};

/**
 * struct cec_cbs - CEC callback function pointers
 * @msg_recv_notify: function pointer called CEC driver to notify incoming msg
 * @data: pointer to data needed to be send with the callback function
 *
 * Defines callback functions which CEC driver can callback to notify any
 * change in the hardware.
 */
struct cec_cbs {
	int (*msg_recv_notify)(void *data, struct cec_msg *msg);
	void *data;
};

/**
 * struct cec_abstract_init_data - initalization data for abstract module
 * @ops: pointer to struct containing all operation function pointers
 * @cbs: pointer to struct containing all callack function pointers
 * @kobj: pointer to kobject instance associated with CEC driver.
 *
 * Defines initialization data needed by init API to initialize the module.
 */
struct cec_abstract_init_data {
	struct cec_ops *ops;
	struct cec_cbs *cbs;
	struct kobject *kobj;
};

void *cec_abstract_init(struct cec_abstract_init_data *init_data);
int cec_abstract_deinit(void *input);
#endif /* __MDSS_CEC_CORE_H_*/
