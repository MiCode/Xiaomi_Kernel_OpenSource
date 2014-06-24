/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MOBICORE_DRIVER_CMD_H_
#define _MOBICORE_DRIVER_CMD_H_

#include "mcuuid.h"

enum mc_drv_cmd_t {
	MC_DRV_CMD_PING			= 0,
	MC_DRV_CMD_GET_INFO		= 1,
	MC_DRV_CMD_OPEN_DEVICE		= 2,
	MC_DRV_CMD_CLOSE_DEVICE		= 3,
	MC_DRV_CMD_NQ_CONNECT		= 4,
	MC_DRV_CMD_OPEN_SESSION		= 5,
	MC_DRV_CMD_CLOSE_SESSION	= 6,
	MC_DRV_CMD_NOTIFY		= 7,
	MC_DRV_CMD_MAP_BULK_BUF		= 8,
	MC_DRV_CMD_UNMAP_BULK_BUF	= 9
};


enum mc_drv_rsp_t {
	MC_DRV_RSP_OK				= 0,
	MC_DRV_RSP_FAILED			= 1,
	MC_DRV_RSP_DEVICE_NOT_OPENED		= 2,
	MC_DRV_RSP_DEVICE_ALREADY_OPENED	= 3,
	MC_DRV_RSP_COMMAND_NOT_ALLOWED		= 4,
	MC_DRV_INVALID_DEVICE_NAME		= 5,
	MC_DRV_RSP_MAP_BULK_ERRO		= 6,
	MC_DRV_RSP_TRUSTLET_NOT_FOUND		= 7,
	MC_DRV_RSP_PAYLOAD_LENGTH_ERROR		= 8,
};


struct mc_drv_command_header_t {
	uint32_t command_id;
};

struct mc_drv_response_header_t {
	uint32_t response_id;
};

#define MC_DEVICE_ID_DEFAULT	0		/* The default device ID */

struct mc_drv_cmd_open_device_payload_t {
	uint32_t device_id;
};

struct mc_drv_cmd_open_device_t {
	struct mc_drv_command_header_t header;
	struct mc_drv_cmd_open_device_payload_t payload;
};


struct mc_drv_rsp_open_device_payload_t {
	/* empty */
};

struct mc_drv_rsp_open_device_t {
	struct mc_drv_response_header_t header;
	struct mc_drv_rsp_open_device_payload_t payload;
};

struct mc_drv_cmd_close_device_t {
	struct mc_drv_command_header_t header;
	/*
	 * no payload here because close has none.
	 * If we use an empty struct, C++ will count it as 4 bytes.
	 * This will write too much into the socket at write(cmd,sizeof(cmd))
	 */
};


struct mc_drv_rsp_close_device_payload_t {
	/* empty */
};

struct mc_drv_rsp_close_device_t {
	struct mc_drv_response_header_t header;
	struct mc_drv_rsp_close_device_payload_t payload;
};

struct mc_drv_cmd_open_session_payload_t {
	uint32_t device_id;
	struct mc_uuid_t uuid;
	uint32_t tci;
	uint32_t handle;
	uint32_t len;
};

struct mc_drv_cmd_open_session_t {
	struct mc_drv_command_header_t header;
	struct mc_drv_cmd_open_session_payload_t payload;
};


struct mc_drv_rsp_open_session_payload_t {
	uint32_t session_id;
	uint32_t device_session_id;
	uint32_t session_magic;
};

struct mc_drv_rsp_open_session_t {
	struct mc_drv_response_header_t header;
	struct mc_drv_rsp_open_session_payload_t  payload;
};

struct mc_drv_cmd_close_session_payload_t {
	uint32_t  session_id;
};

struct mc_drv_cmd_close_session_t {
	struct mc_drv_command_header_t header;
	struct mc_drv_cmd_close_session_payload_t payload;
};


struct mc_drv_rsp_close_session_payload_t {
	/* empty */
};

struct mc_drv_rsp_close_session_t {
	struct mc_drv_response_header_t header;
	struct mc_drv_rsp_close_session_payload_t payload;
};

struct mc_drv_cmd_notify_payload_t {
	uint32_t session_id;
};

struct mc_drv_cmd_notify_t {
	struct mc_drv_command_header_t header;
	struct mc_drv_cmd_notify_payload_t payload;
};


struct mc_drv_rsp_notify_payload_t {
	/* empty */
};

struct mc_drv_rsp_notify_t {
	struct mc_drv_response_header_t header;
	struct mc_drv_rsp_notify_payload_t  payload;
};

struct mc_drv_cmd_map_bulk_mem_payload_t {
	uint32_t session_id;
	uint32_t handle;
	uint32_t rfu;
	uint32_t offset_payload;
	uint32_t len_bulk_mem;
};

struct mc_drv_cmd_map_bulk_mem_t {
	struct mc_drv_command_header_t header;
	struct mc_drv_cmd_map_bulk_mem_payload_t  payload;
};


struct mc_drv_rsp_map_bulk_mem_payload_t {
	uint32_t session_id;
	uint32_t secure_virtual_adr;
};

struct mc_drv_rsp_map_bulk_mem_t {
	struct mc_drv_response_header_t header;
	struct mc_drv_rsp_map_bulk_mem_payload_t  payload;
};

struct mc_drv_cmd_unmap_bulk_mem_payload_t {
	uint32_t session_id;
	uint32_t handle;
	uint32_t secure_virtual_adr;
	uint32_t len_bulk_mem;
};

struct mc_drv_cmd_unmap_bulk_mem_t {
	struct mc_drv_command_header_t header;
	struct mc_drv_cmd_unmap_bulk_mem_payload_t payload;
};


struct mc_drv_rsp_unmap_bulk_mem_payload_t {
	uint32_t response_id;
	uint32_t session_id;
};

struct mc_drv_rsp_unmap_bulk_mem_t {
	struct mc_drv_response_header_t header;
	struct mc_drv_rsp_unmap_bulk_mem_payload_t payload;
};

struct mc_drv_cmd_nqconnect_payload_t {
	uint32_t device_id;
	uint32_t session_id;
	uint32_t device_session_id;
	uint32_t session_magic; /* Random data */
};

struct mc_drv_cmd_nqconnect_t {
	struct mc_drv_command_header_t header;
	struct mc_drv_cmd_nqconnect_payload_t  payload;
};


struct mc_drv_rsp_nqconnect_payload_t {
	/* empty; */
};

struct mc_drv_rsp_nqconnect_t {
	struct mc_drv_response_header_t header;
	struct mc_drv_rsp_nqconnect_payload_t payload;
};

union mc_drv_command_t {
	struct mc_drv_command_header_t		header;
	struct mc_drv_cmd_open_device_t		mc_drv_cmd_open_device;
	struct mc_drv_cmd_close_device_t	mc_drv_cmd_close_device;
	struct mc_drv_cmd_open_session_t	mc_drv_cmd_open_session;
	struct mc_drv_cmd_close_session_t	mc_drv_cmd_close_session;
	struct mc_drv_cmd_nqconnect_t		mc_drv_cmd_nqconnect;
	struct mc_drv_cmd_notify_t		mc_drv_cmd_notify;
	struct mc_drv_cmd_map_bulk_mem_t	mc_drv_cmd_map_bulk_mem;
	struct mc_drv_cmd_unmap_bulk_mem_t	mc_drv_cmd_unmap_bulk_mem;
};

union mc_drv_response_t {
	struct mc_drv_response_header_t		header;
	struct mc_drv_rsp_open_device_t		mc_drv_rsp_open_device;
	struct mc_drv_rsp_close_device_t	mc_drv_rsp_close_device;
	struct mc_drv_rsp_open_session_t	mc_drv_rsp_open_session;
	struct mc_drv_rsp_close_session_t	mc_drv_rsp_close_session;
	struct mc_drv_rsp_nqconnect_t		mc_drv_rsp_nqconnect;
	struct mc_drv_rsp_notify_t		mc_drv_rsp_notify;
	struct mc_drv_rsp_map_bulk_mem_t	mc_drv_rsp_map_bulk_mem;
	struct mc_drv_rsp_unmap_bulk_mem_t	mc_drv_rsp_unmap_bulk_mem;
};

#endif /* _MOBICORE_DRIVER_CMD_H_ */
