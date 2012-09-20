/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#ifndef __Q6CORE_H__
#define __Q6CORE_H__
#include <mach/qdsp6v2/apr.h>


#define ADSP_CMD_REMOTE_BUS_BW_REQUEST		0x0001115D
#define AUDIO_IF_BUS_ID				1

struct adsp_cmd_remote_bus_bw_request {
	struct apr_hdr hdr;
	u16 bus_identifier;
	u16 reserved;
	u32 ab_bps;
	u32 ib_bps;
} __packed;

#define ADSP_GET_VERSION     0x00011152
#define ADSP_GET_VERSION_RSP 0x00011153

struct adsp_get_version {
	uint32_t build_id;
	uint32_t svc_cnt;
};

struct adsp_service_info {
	uint32_t svc_id;
	uint32_t svc_ver;
};

#define ADSP_CMD_SET_POWER_COLLAPSE_STATE 0x0001115C
struct adsp_power_collapse {
	struct apr_hdr hdr;
	uint32_t power_collapse;
};

#define ADSP_CMD_SET_DTS_MODEL_ID 0x00012917

struct adsp_dts_modelid {
	struct apr_hdr hdr;
	uint32_t  model_ID_size;
	uint8_t   model_ID[128];
};

int core_req_bus_bandwith(u16 bus_id, u32 ab_bps, u32 ib_bps);

uint32_t core_get_adsp_version(void);

uint32_t core_set_dts_model_id(uint32_t id_size, uint8_t *id);

#endif /* __Q6CORE_H__ */
