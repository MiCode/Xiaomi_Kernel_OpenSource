/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#ifndef WCD_SPI_CTL_V01_H
#define WCD_SPI_CTL_V01_H

#define WCD_SPI_CTL_SERVICE_ID_V01 0x421
#define WCD_SPI_CTL_SERVICE_VERS_V01 0x01

#define WCD_SPI_BUFF_RESP_V01 0x0022
#define WCD_SPI_REL_ACCESS_RESP_V01 0x0021
#define WCD_SPI_REQ_ACCESS_MSG_V01 0x0020
#define WCD_SPI_BUFF_MSG_V01 0x0022
#define WCD_SPI_REL_ACCESS_MSG_V01 0x0021
#define WCD_SPI_REQ_ACCESS_RESP_V01 0x0020

#define WCD_SPI_BUFF_CHANNELS_MAX_V01 0x08

#define WCD_SPI_REQ_DATA_TRANSFER_V01 ((u64)0x01ULL)
#define WCD_SPI_REQ_CONCURRENCY_V01 ((u64)0x02ULL)
#define WCD_SPI_REQ_REMOTE_DOWN_V01 ((u64)0x04ULL)

struct wcd_spi_req_access_msg_v01 {
	u8 reason_valid;
	u64 reason;
};
#define WCD_SPI_REQ_ACCESS_MSG_V01_MAX_MSG_LEN 11
extern struct elem_info wcd_spi_req_access_msg_v01_ei[];

struct wcd_spi_req_access_resp_v01 {
	struct qmi_response_type_v01 resp;
};
#define WCD_SPI_REQ_ACCESS_RESP_V01_MAX_MSG_LEN 7
extern struct elem_info wcd_spi_req_access_resp_v01_ei[];

struct wcd_spi_rel_access_msg_v01 {
	char placeholder;
};
#define WCD_SPI_REL_ACCESS_MSG_V01_MAX_MSG_LEN 0
extern struct elem_info wcd_spi_rel_access_msg_v01_ei[];

struct wcd_spi_rel_access_resp_v01 {
	struct qmi_response_type_v01 resp;
};
#define WCD_SPI_REL_ACCESS_RESP_V01_MAX_MSG_LEN 7
extern struct elem_info wcd_spi_rel_access_resp_v01_ei[];

struct wcd_spi_buff_msg_v01 {
	u32 buff_addr_1[WCD_SPI_BUFF_CHANNELS_MAX_V01];
	u8 buff_addr_2_valid;
	u32 buff_addr_2[WCD_SPI_BUFF_CHANNELS_MAX_V01];
};
#define WCD_SPI_BUFF_MSG_V01_MAX_MSG_LEN 70
extern struct elem_info wcd_spi_buff_msg_v01_ei[];

struct wcd_spi_buff_resp_v01 {
	struct qmi_response_type_v01 resp;
};
#define WCD_SPI_BUFF_RESP_V01_MAX_MSG_LEN 7
extern struct elem_info wcd_spi_buff_resp_v01_ei[];

#endif
