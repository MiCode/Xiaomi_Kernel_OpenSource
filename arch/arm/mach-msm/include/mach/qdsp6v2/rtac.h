/* Copyright (c) 2011, 2013, The Linux Foundation. All rights reserved.
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

#ifndef __RTAC_H__
#define __RTAC_H__

/* Voice Modes */
#define RTAC_CVP		0
#define RTAC_CVS		1
#define RTAC_VOICE_MODES	2

enum {
	ADM_RTAC_CAL,
	ASM_RTAC_CAL,
	VOICE_RTAC_CAL,
	MAX_RTAC_BLOCKS
};

struct rtac_cal_mem_map_data {
	uint32_t		map_size;
	uint32_t		map_handle;
	struct ion_client	*ion_client;
	struct ion_handle	*ion_handle;
};

struct rtac_cal_data {
	uint32_t		size;
	uint32_t		kvaddr;
	uint32_t		paddr;
};

struct rtac_cal_block_data {
	struct rtac_cal_mem_map_data	map_data;
	struct rtac_cal_data		cal_data;
};

void rtac_add_adm_device(u32 port_id, u32 copp_id, u32 path_id, u32 popp_id);
void rtac_remove_adm_device(u32 port_id, u32 copp_id);
void rtac_remove_popp_from_adm_devices(u32 popp_id);
void rtac_add_voice(u32 cvs_handle, u32 cvp_handle, u32 rx_afe_port,
	u32 tx_afe_port, u32 session_id);
void rtac_remove_voice(u32 cvs_handle);
void rtac_set_adm_handle(void *handle);
bool rtac_make_adm_callback(uint32_t *payload, u32 payload_size);
void rtac_copy_adm_payload_to_user(void *payload, u32 payload_size);
void rtac_set_asm_handle(u32 session_id, void *handle);
bool rtac_make_asm_callback(u32 session_id, uint32_t *payload,
	u32 payload_size);
void rtac_copy_asm_payload_to_user(void *payload, u32 payload_size);
void rtac_set_voice_handle(u32 mode, void *handle);
bool rtac_make_voice_callback(u32 mode, uint32_t *payload, u32 payload_size);
void rtac_copy_voice_payload_to_user(void *payload, u32 payload_size);
int rtac_clear_mapping(uint32_t cal_type);

#endif
