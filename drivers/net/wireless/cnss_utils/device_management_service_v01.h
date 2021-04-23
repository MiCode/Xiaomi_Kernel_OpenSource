/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef DEVICE_MANAGEMENT_SERVICE_V01_H
#define DEVICE_MANAGEMENT_SERVICE_V01_H

#define DMS_SERVICE_ID_V01 0x02
#define DMS_SERVICE_VERS_V01 0x01

#define QMI_DMS_GET_MAC_ADDRESS_RESP_V01 0x005C
#define QMI_DMS_GET_MAC_ADDRESS_REQ_V01 0x005C
#define QMI_DMS_MAC_ADDR_MAX_V01 8

enum dms_device_mac_enum_v01 {
	DMS_DEVICE_MAC_ENUM_MIN_VAL_V01 = INT_MIN,
	DMS_DEVICE_MAC_WLAN_V01 = 0,
	DMS_DEVICE_MAC_BT_V01 = 1,
	DMS_DEVICE_MAC_ENUM_MAX_VAL_V01 = INT_MAX,
};

struct dms_get_mac_address_req_msg_v01 {
	enum dms_device_mac_enum_v01 device;
};

#define DMS_GET_MAC_ADDRESS_REQ_MSG_V01_MAX_MSG_LEN 7
extern struct qmi_elem_info dms_get_mac_address_req_msg_v01_ei[];

struct dms_get_mac_address_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u8 mac_address_valid;
	u32 mac_address_len;
	u8 mac_address[QMI_DMS_MAC_ADDR_MAX_V01];
};

#define DMS_GET_MAC_ADDRESS_RESP_MSG_V01_MAX_MSG_LEN 19
extern struct qmi_elem_info dms_get_mac_address_resp_msg_v01_ei[];

#endif
