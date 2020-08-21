/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */


#ifndef __CCCI_MSG_ID_H__
#define __CCCI_MSG_ID_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>



enum ccci_msg_id {
	CCCI_USB_MSG_ID = 0,
	CCCI_PORT_MSG_ID,
	CCCI_FSM_MSG_ID,

	CCCI_MAX_MSG_ID,
};

enum ccci_usb_sub_id {
	CCCI_USB_UPSTREAM_BUF = 0,
	CCCI_USB_BUFFER_PUSH,
	CCCI_USB_INTERCEPT,

};


enum ccci_error_id {
	CCCI_ERR_INVALID_MSG_ID = 0X70000000,
	CCCI_ERR_NO_USER_RECV,
	CCCI_ERR_NO_MEMORY,
	CCCI_ERR_NO_CALLBACK_FUN,
	CCCI_ERR_CMD_HEAD_NULL,

};


/* define none sub id */
#define CCCI_NONE_SUB_ID (-1)




#endif	/* __CCCI_MSG_ID_H__ */
