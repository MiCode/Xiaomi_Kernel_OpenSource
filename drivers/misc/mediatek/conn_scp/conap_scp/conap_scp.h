/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CONAP_SCP_H__
#define __CONAP_SCP_H__

enum conap_scp_drv_type {
	DRV_TYPE_CORE = 0,
	DRV_TYPE_GPS = 1,
	DRV_TYPE_EM = 2,
	CONAP_SCP_DRV_NUM
};

enum conap_scp_status {
	CONN_SUCCESS = 0,
	CONN_INVALID_HANDLE = -1,
	CONN_INVALID_ARGUMENT = -2,
	CONN_NOT_READY = -3,
	CONN_RESET = -4,
	CONN_TIMEOUT = -5,
	CONN_MSG_ONGOING = -6,
	CONN_MSG_QUEUE_FULL = -7,
	CONN_CONAP_NOT_SUPPORT = -8
};

struct conap_scp_drv_cb {
	/* notify message is arrived */
	void (*conap_scp_msg_notify_cb)(unsigned int msg_id, unsigned int *buf, unsigned int size);
	/* notify was resetted */
	void (*conap_scp_state_notify_cb)(int state);
};

/*
 * >= 0: success
 * <0 : fail
 */
int conap_scp_register_drv(enum conap_scp_drv_type type, struct conap_scp_drv_cb *cb);

int conap_scp_unregister_drv(enum conap_scp_drv_type type);

/*
 * return
 *   >0 : success, seq num,
 *			means msg was sent, but doesn't mean connsys received
 *	 ack_cb was called when
 *		ret=0, consys was received msg
 *		ret=CONN_TIMEOUT, timeout, connsys no response
 */
int conap_scp_send_message(enum conap_scp_drv_type type,
					unsigned int msg_id,
					unsigned char *buf, unsigned int size);

/*
 * return
 *   1 : scp driver ready
 *   0 : scp driver not ready
 *   <0 : fail, error no
 */
int conap_scp_is_drv_ready(enum conap_scp_drv_type type);


#endif
