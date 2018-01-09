/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef __PKTZR_H_
#define __PKTZR_H_

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <soc/qcom/glink.h>

#define VERSION_ID                   0x80
#define CLIENT_ID_AUDIO              0X01
#define DOMAIN_ID_APPS               0x01

#define PKTZR_BASIC_RESPONSE_RESULT 0x0002
#define PKTZR_CMD_OPEN              0x0021
#define PKTZR_CMD_SET_CONFIG        0x0022
#define PKTZR_CMD_START             0x0023
#define PKTZR_CMD_STOP              0x0024
#define PKTZR_CMD_CLOSE             0x0027
#define PKTZR_CMDRSP_GET_CONFIG     0x0007
#define PKTZR_CMD_LOAD_DATA         0x0008
#define PKTZR_CMDRSP_LOAD_DATA      0x0009
#define PKTZR_CMD_UNLOAD_DATA       0x000A
#define PKTZR_CMD_EVENT             0x000B
#define PKTZR_CMD_DATA              0x000C
#define PKTZR_CMDRSP_DATA           0x000D
#define PKTZR_CMD_INIT_PARAM        0x0029

typedef int (*pktzr_data_cmd_cb_fn)(void *buf, uint32_t len, void *priv_data,
				     bool *is_basic_rsp);

struct pktzr_cmd_rsp {
	/* Requested resp buffer */
	void *buf;
	/* Requested resp buffer size */
	uint32_t buf_size;
	/* Received resp buffer size */
	uint32_t rsp_size;
	/* Basic response or command response */
	bool is_basic_rsp;
};

extern int pktzr_init(void *pdev, struct bg_glink_ch_cfg *ch_info,
		      int num_channels, pktzr_data_cmd_cb_fn func);
extern void pktzr_deinit(void);
extern int pktzr_cmd_open(void *payload, uint32_t size,
			  struct pktzr_cmd_rsp *rsp);
extern int pktzr_cmd_close(void *payload, uint32_t size,
			   struct pktzr_cmd_rsp *rsp);
extern int pktzr_cmd_start(void *payload, uint32_t size,
			  struct pktzr_cmd_rsp *rsp);
extern int pktzr_cmd_stop(void *payload, uint32_t size,
			   struct pktzr_cmd_rsp *rsp);
extern int pktzr_cmd_data(void *payload, uint32_t size, void *priv_data);

extern int pktzr_cmd_set_params(void *payload, uint32_t size,
				struct pktzr_cmd_rsp *rsp);
extern int pktzr_cmd_init_params(void *payload, uint32_t size,
				 struct pktzr_cmd_rsp *rsp);
#endif
