/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#ifndef _UAPI_QBT1000_H_
#define _UAPI_QBT1000_H_

#define MAX_NAME_SIZE					 32

/*
* enum qbt1000_commands -
*      enumeration of command options
* @QBT1000_LOAD_APP - cmd loads TZ app
* @QBT1000_UNLOAD_APP - cmd unloads TZ app
* @QBT1000_SEND_TZCMD - sends cmd TZ app
*/
enum qbt1000_commands {
	QBT1000_LOAD_APP = 100,
	QBT1000_UNLOAD_APP = 101,
	QBT1000_SEND_TZCMD = 102
};

/*
* struct qbt1000_app -
*      used to load and unload apps in TZ
* @app_handle - qseecom handle for clients
* @name - Name of secure app to load
* @size - Size of requested buffer of secure app
* @high_band_width - 1 - for high bandwidth usage
*                    0 - for normal bandwidth usage
*/
struct qbt1000_app {
	struct qseecom_handle **app_handle;
	char name[MAX_NAME_SIZE];
	uint32_t size;
	uint8_t high_band_width;
};

/*
* struct qbt1000_send_tz_cmd -
*      used to cmds to TZ App
* @app_handle - qseecom handle for clients
* @req_buf - Buffer containing request for secure app
* @req_buf_len - Length of request buffer
* @rsp_buf - Buffer containing response from secure app
* @rsp_buf_len - Length of response buffer
*/
struct qbt1000_send_tz_cmd {
	struct qseecom_handle *app_handle;
	uint8_t *req_buf;
	uint32_t req_buf_len;
	uint8_t *rsp_buf;
	uint32_t rsp_buf_len;
};

#endif /* _UAPI_QBT1000_H_ */
