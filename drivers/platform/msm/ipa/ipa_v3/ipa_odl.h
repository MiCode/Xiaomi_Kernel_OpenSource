/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA3_ODL_H_
#define _IPA3_ODL_H_

#define IPA_ODL_AGGR_BYTE_LIMIT (15 * 1024)
#define IPA_ODL_RX_RING_SIZE 192
#define MAX_QUEUE_TO_ODL 1024
#define CONFIG_SUCCESS 1
#define ODL_EP_TYPE_HSUSB 2
#define ODL_EP_PERIPHERAL_IFACE_ID 3

struct ipa3_odlstats {
	u32 odl_rx_pkt;
	u32 odl_tx_diag_pkt;
	u32 odl_drop_pkt;
	atomic_t numer_in_queue;
};

struct odl_state_bit_mask {
	u32 odl_init:1;
	u32 odl_open:1;
	u32 adpl_open:1;
	u32 aggr_byte_limit_sent:1;
	u32 odl_ep_setup:1;
	u32 odl_setup_done_sent:1;
	u32 odl_ep_info_sent:1;
	u32 odl_connected:1;
	u32 odl_disconnected:1;
	u32:0;
};

/**
 * struct ipa3_odl_char_device_context - IPA ODL character device
 * @class: pointer to the struct class
 * @dev_num: device number
 * @dev: the dev_t of the device
 * @cdev: cdev of the device
 */
struct ipa3_odl_char_device_context {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
};

struct ipa_odl_context {
	struct ipa3_odl_char_device_context odl_cdev[2];
	struct list_head adpl_msg_list;
	struct mutex adpl_msg_lock;
	struct ipa_sys_connect_params odl_sys_param;
	u32 odl_client_hdl;
	struct odl_state_bit_mask odl_state;
	bool odl_ctl_msg_wq_flag;
	struct ipa3_odlstats stats;
	u32 odl_pm_hdl;
};

struct ipa3_push_msg_odl {
	void *buff;
	int len;
	struct list_head link;
};

extern struct ipa_odl_context *ipa3_odl_ctx;

int ipa_odl_init(void);
void ipa3_odl_pipe_cleanup(bool is_ssr);
int ipa3_odl_pipe_open(void);

#endif /* _IPA3_ODL_H_ */
