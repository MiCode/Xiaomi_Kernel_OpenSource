/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

#ifndef __SND_ADIE_SVC_H_
#define __SND_ADIE_SVC_H_

#define ADIE_SVC_PROG	0x30000002
#define ADIE_SVC_VERS	0x00020003

#define ADIE_SVC_CLIENT_STATUS_FUNC_PTR_TYPE_PROC 0xFFFFFF01
#define SND_ADIE_SVC_CLIENT_REGISTER_PROC 	34
#define SND_ADIE_SVC_CONFIG_ADIE_BLOCK_PROC 	35
#define SND_ADIE_SVC_CLIENT_DEREGISTER_PROC 	36

#define ADIE_SVC_MAX_CLIENTS 5

enum adie_svc_client_operation{
	ADIE_SVC_REGISTER_CLIENT,
	ADIE_SVC_DEREGISTER_CLIENT,
	ADIE_SVC_CONFIG_ADIE_BLOCK,
};

enum adie_svc_status_type{
	ADIE_SVC_STATUS_SUCCESS,
	ADIE_SVC_STATUS_FAILURE,
	ADIE_SVC_STATUS_INUSE
};

enum adie_block_enum_type{
	MIC_BIAS,
	HSSD,
	HPH_PA
};

enum adie_config_enum_type{
	DISABLE,
	ENABLE
};

struct adie_svc_client{
	int client_id;
	int cb_id;
	enum adie_svc_status_type status;
	bool adie_svc_cb_done;
	struct mutex lock;
	wait_queue_head_t wq;
	struct msm_rpc_client *rpc_client;
};

struct adie_svc_client_register_cb_cb_args {
	int cb_id;
	uint32_t size;
	int client_id;
	enum adie_block_enum_type adie_block;
	enum adie_svc_status_type status;
	enum adie_svc_client_operation client_operation;
};

struct adie_svc_client_register_cb_args {
	int cb_id;
};

struct adie_svc_client_deregister_cb_args {
	int client_id;
};

struct adie_svc_config_adie_block_cb_args {
	int client_id;
	enum adie_block_enum_type adie_block;
	enum adie_config_enum_type config;
};

int adie_svc_get(void);
int adie_svc_put(int id);
int adie_svc_config_adie_block(int id,
	enum adie_block_enum_type adie_block_type, bool enable);
#endif
