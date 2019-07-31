/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2019, The Linux Foundation. All rights reserved. */

#ifndef COEXISTENCE_SERVICE_V01_H
#define COEXISTENCE_SERVICE_V01_H

#define COEX_SERVICE_ID_V01 0x22
#define COEX_SERVICE_VERS_V01 0x01

#define COEX_SERVICE_MAX_MSG_LEN 8204

#define QMI_COEX_SWITCH_ANTENNA_TO_WLAN_RESP_V01 0x0042
#define QMI_COEX_SWITCH_ANTENNA_TO_WLAN_REQ_V01 0x0042
#define QMI_COEX_SWITCH_ANTENNA_TO_MDM_RESP_V01 0x0042
#define QMI_COEX_SWITCH_ANTENNA_TO_MDM_REQ_V01 0x0042

#define COEX_ANTENNA_BAND_2GHZ_CHAIN0_V01 ((u64)0x0000000000000001ULL)
#define COEX_ANTENNA_BAND_2GHZ_CHAIN1_V01 ((u64)0x0000000000000002ULL)
#define COEX_ANTENNA_BAND_5GHZ_CHAIN0_V01 ((u64)0x0000000000000004ULL)
#define COEX_ANTENNA_BAND_5GHZ_CHAIN1_V01 ((u64)0x0000000000000008ULL)

struct coex_antenna_switch_to_wlan_req_msg_v01 {
	u64 antenna;
};

#define COEX_ANTENNA_SWITCH_TO_WLAN_REQ_MSG_V01_MAX_MSG_LEN 11
extern struct qmi_elem_info coex_antenna_switch_to_wlan_req_msg_v01_ei[];

struct coex_antenna_switch_to_wlan_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u8 grant_valid;
	u64 grant;
};

#define COEX_ANTENNA_SWITCH_TO_WLAN_RESP_MSG_V01_MAX_MSG_LEN 18
extern struct qmi_elem_info coex_antenna_switch_to_wlan_resp_msg_v01_ei[];

struct coex_antenna_switch_to_mdm_req_msg_v01 {
	u64 antenna;
};

#define COEX_ANTENNA_SWITCH_TO_MDM_REQ_MSG_V01_MAX_MSG_LEN 11
extern struct qmi_elem_info coex_antenna_switch_to_mdm_req_msg_v01_ei[];

struct coex_antenna_switch_to_mdm_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define COEX_ANTENNA_SWITCH_TO_MDM_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct qmi_elem_info coex_antenna_switch_to_mdm_resp_msg_v01_ei[];

#endif
