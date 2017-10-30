/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
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

#ifndef __BG_INTF_H_
#define __BG_INTF_H_

#define MAX_APP_NAME_SIZE 100
#define RESULT_SUCCESS 0
#define RESULT_FAILURE -1

/* tzapp command list.*/
enum bg_tz_commands {
	BGPIL_RAMDUMP,
	BGPIL_IMAGE_LOAD,
	BGPIL_AUTH_MDT,
	BGPIL_DLOAD_CONT,
};

/* tzapp bg request.*/
__packed struct tzapp_bg_req {
	uint8_t tzapp_bg_cmd;
	phys_addr_t address_fw;
	size_t size_fw;
};

/* tzapp bg response.*/
__packed struct tzapp_bg_rsp {
	uint32_t tzapp_bg_cmd;
	uint32_t bg_info_len;
	uint32_t status;
	uint32_t bg_info[100];
};

#endif
