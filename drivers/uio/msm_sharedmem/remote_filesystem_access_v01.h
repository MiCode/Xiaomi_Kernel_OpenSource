 /* Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 and
  * only version 2 as published by the Free Software Foundation.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  */
#ifndef __REMOTE_FILESYSTEM_ACCESS_V01_H__
#define __REMOTE_FILESYSTEM_ACCESS_V01_H__

#define RFSA_SERVICE_ID_V01 0x1C
#define RFSA_SERVICE_VERS_V01 0x01

#define QMI_RFSA_GET_BUFF_ADDR_REQ_MSG_V01 0x0023
#define QMI_RFSA_GET_BUFF_ADDR_RESP_MSG_V01 0x0023

#define RFSA_GET_BUFF_ADDR_REQ_MSG_MAX_LEN_V01 14
#define RFSA_GET_BUFF_ADDR_RESP_MSG_MAX_LEN_V01 18

extern struct elem_info rfsa_get_buff_addr_req_msg_v01_ei[];
extern struct elem_info rfsa_get_buff_addr_resp_msg_v01_ei[];

struct rfsa_get_buff_addr_req_msg_v01 {
	uint32_t client_id;
	uint32_t size;
};

struct rfsa_get_buff_addr_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t address_valid;
	uint64_t address;
};

#endif /* __REMOTE_FILESYSTEM_ACCESS_V01_H__ */
