/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020, The Linux Foundation. All rights reserved. */

#ifndef IP_MULTIMEDIA_SUBSYSTEM_PRIVATE_SERVICE_V01_H
#define IP_MULTIMEDIA_SUBSYSTEM_PRIVATE_SERVICE_V01_H

#include <linux/soc/qcom/qmi.h>

#define IMSPRIVATE_SERVICE_ID_V01 0x4D
#define IMSPRIVATE_SERVICE_VERS_V01 0x01

#define QMI_IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_RSP_V01 0x003E
#define QMI_IMS_PRIVATE_SERVICE_WFC_CALL_STATUS_IND_V01 0x0040
#define QMI_IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_REQ_V01 0x003E
#define QMI_IMS_PRIVATE_SERVICE_MT_INVITE_IND_V01 0x003F
#define QMI_IMS_PRIVATE_SERVICE_WFC_CALL_TWT_CONFIG_RSP_V01 0x0041
#define QMI_IMS_PRIVATE_SERVICE_WFC_CALL_TWT_CONFIG_REQ_V01 0x0041

#define IMS_PRIVATE_SERVICE_MAX_MT_INVITE_HEADERS_V01 15
#define IMS_PRIVATE_SERVICE_HEADER_STR_LEN_V01 1024
#define IMS_PRIVATE_SERVICE_MAX_ICCID_LEN_V01 21

enum ims_common_resp_enum_v01 {
	IMS_COMMON_RESP_ENUM_MIN_VAL_V01 = INT_MIN,
	IMS_COMMON_MSG_NO_ERR_V01 = 0,
	IMS_COMMON_MSG_IMS_NOT_READY_V01 = 1,
	IMS_COMMON_MSG_FILE_NOT_AVAILABLE_V01 = 2,
	IMS_COMMON_MSG_READ_FAILED_V01 = 3,
	IMS_COMMON_MSG_WRITE_FAILED_V01 = 4,
	IMS_COMMON_MSG_OTHER_INTERNAL_ERR_V01 = 5,
	IMS_COMMON_RESP_ENUM_MAX_VAL_V01 = INT_MAX,
};

enum ims_subscription_type_enum_v01 {
	IMS_SUBSCRIPTION_TYPE_ENUM_MIN_VAL_V01 = INT_MIN,
	IMS_SUBSCRIPTION_TYPE_NONE_V01 = -1,
	IMS_SUBSCRIPTION_TYPE_PRIMARY_V01 = 0,
	IMS_SUBSCRIPTION_TYPE_SECONDARY_V01 = 1,
	IMS_SUBSCRIPTION_TYPE_TERTIARY_V01 = 2,
	IMS_SUBSCRIPTION_TYPE_ENUM_MAX_VAL_V01 = INT_MAX,
};

enum wfc_media_quality_v01 {
	WFC_MEDIA_QUALITY_MIN_VAL_V01 = INT_MIN,
	WFC_MEDIA_QUAL_NOT_AVAILABLE_V01 = 0,
	WFC_MEDIA_QUAL_BAD_V01 = 1,
	WFC_MEDIA_QUAL_GOOD_V01 = 2,
	WFC_MEDIA_QUAL_EXCELLENT_V01 = 3,
	WFC_MEDIA_QUALITY_MAX_VAL_V01 = INT_MAX,
};

struct ims_private_service_header_value_v01 {
	char header[IMS_PRIVATE_SERVICE_HEADER_STR_LEN_V01 + 1];
	char value[IMS_PRIVATE_SERVICE_HEADER_STR_LEN_V01 + 1];
};

struct ims_private_service_subscribe_for_indications_req_msg_v01 {
	u8 mt_invite_valid;
	u8 mt_invite;
	u8 wfc_call_status_valid;
	u8 wfc_call_status;
};

#define IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_REQ_MSG_V01_MAX_MSG_LEN 8
extern struct qmi_elem_info
ims_private_service_subscribe_for_indications_req_msg_v01_ei[];

struct ims_private_service_subscribe_for_indications_rsp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_RSP_MSG_V01_MAX_MSG_LEN 7
extern struct qmi_elem_info
ims_private_service_subscribe_for_indications_rsp_msg_v01_ei[];

struct ims_private_service_mt_invite_ind_msg_v01 {
	enum ims_subscription_type_enum_v01 subscription_type;
	u8 iccid_valid;
	char iccid[IMS_PRIVATE_SERVICE_MAX_ICCID_LEN_V01 + 1];
	u8 header_value_list_valid;
	u32 header_value_list_len;
	struct ims_private_service_header_value_v01
	header_value_list[IMS_PRIVATE_SERVICE_MAX_MT_INVITE_HEADERS_V01];
};

#define IMS_PRIVATE_SERVICE_MT_INVITE_IND_MSG_V01_MAX_MSG_LEN 30815
extern struct qmi_elem_info ims_private_service_mt_invite_ind_msg_v01_ei[];

struct ims_private_service_wfc_call_status_ind_msg_v01 {
	u8 wfc_call_active;
	u8 all_wfc_calls_held_valid;
	u8 all_wfc_calls_held;
	u8 is_wfc_emergency_valid;
	u8 is_wfc_emergency;
	u8 twt_ims_start_valid;
	u64 twt_ims_start;
	u8 twt_ims_int_valid;
	u16 twt_ims_int;
	u8 media_quality_valid;
	enum wfc_media_quality_v01 media_quality;
};

#define IMS_PRIVATE_SERVICE_WFC_CALL_STATUS_IND_MSG_V01_MAX_MSG_LEN 35
extern struct qmi_elem_info
ims_private_service_wfc_call_status_ind_msg_v01_ei[];

struct ims_private_service_wfc_call_twt_config_req_msg_v01 {
	u8 twt_sta_start_valid;
	u64 twt_sta_start;
	u8 twt_sta_int_valid;
	u16 twt_sta_int;
	u8 twt_sta_upo_valid;
	u16 twt_sta_upo;
	u8 twt_sta_sp_valid;
	u16 twt_sta_sp;
	u8 twt_sta_dl_valid;
	u16 twt_sta_dl;
	u8 twt_sta_config_changed_valid;
	u8 twt_sta_config_changed;
};

#define IMS_PRIVATE_SERVICE_WFC_CALL_TWT_CONFIG_REQ_MSG_V01_MAX_MSG_LEN 35
extern struct qmi_elem_info
ims_private_service_wfc_call_twt_config_req_msg_v01_ei[];

struct ims_private_service_wfc_call_twt_config_rsp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define IMS_PRIVATE_SERVICE_WFC_CALL_TWT_CONFIG_RSP_MSG_V01_MAX_MSG_LEN 7
extern struct qmi_elem_info
ims_private_service_wfc_call_twt_config_rsp_msg_v01_ei[];

#endif
