/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2019, The Linux Foundation. All rights reserved. */

#ifndef IP_MULTIMEDIA_SUBSYSTEM_PRIVATE_SERVICE_V01_H
#define IP_MULTIMEDIA_SUBSYSTEM_PRIVATE_SERVICE_V01_H

#define IMSPRIVATE_SERVICE_ID_V01 0x4D
#define IMSPRIVATE_SERVICE_VERS_V01 0x01

#define IMSPRIVATE_SERVICE_MAX_MSG_LEN 8

#define QMI_IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_RSP_V01 0x003E
#define QMI_IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_REQ_V01 0x003E
#define QMI_IMS_PRIVATE_SERVICE_WFC_CALL_STATUS_IND_V01 0x0040

struct ims_private_service_subscribe_for_indications_req_msg_v01 {
	u8 mt_invite_valid;
	u8 mt_invite;
	u8 wfc_call_status_valid;
	u8 wfc_call_status;
};

#define IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_REQ_MSG_V01_MAX_MSG_LEN 8
extern struct
qmi_elem_info ims_private_service_subscribe_for_indications_req_msg_v01_ei[];

struct ims_private_service_subscribe_for_indications_rsp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_RSP_MSG_V01_MAX_MSG_LEN 7
extern struct
qmi_elem_info ims_private_service_subscribe_for_indications_rsp_msg_v01_ei[];

struct ims_private_service_wfc_call_status_ind_msg_v01 {
	u8 wfc_call_active;
};

#define IMS_PRIVATE_SERVICE_WFC_CALL_STATUS_IND_MSG_V01_MAX_MSG_LEN 4
extern struct
qmi_elem_info ims_private_service_wfc_call_status_ind_msg_v01_ei[];

#endif
