/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

#ifndef _VIDEO_CORE_PIXCACHE_
#define _VIDEO_CORE_PIXCACHE_


#include "vidc.h"

#define VIDC_1080P_DEC_DPB_RESET_VALUE 0xFFFFFFF8

enum vidc_1080P_pix_cache_port_sel{
	VIDC_1080P_PIX_CACHE_PORT_A = 0,
	VIDC_1080P_PIX_CACHE_PORT_B = 1,
	VIDC_1080P_PIX_CACHE_PORT_32BIT = 0x7FFFFFFF
};
enum vidc_1080P_pix_cache_page_size{
	VIDC_1080P_PIX_CACHE_PAGE_SIZE_1K = 0,
	VIDC_1080P_PIX_CACHE_PAGE_SIZE_2K = 1,
	VIDC_1080P_PIX_CACHE_PAGE_SIZE_4K = 2
};
struct vidc_1080P_pix_cache_config{
	u32 cache_enable;
	u32 prefetch_en;
	enum vidc_1080P_pix_cache_port_sel port_select;
	u32 statistics_off;
	enum vidc_1080P_pix_cache_page_size page_size;
};
struct vidc_1080P_pix_cache_statistics{
	u32 access_miss;
	u32 access_hit;
	u32 axi_req;
	u32 core_req;
	u32 axi_bus;
	u32 core_bus;
};
struct vidc_1080P_pix_cache_misr_id_filtering{
	u32 ignore_id;
	u32 id;
};
struct vidc_1080P_pix_cache_misr_signature{
	u32 signature0;
	u32 signature1;
};

void vidc_pix_cache_sw_reset(void);
void vidc_pix_cache_init_luma_chroma_base_addr(u32 dpb,
	u32 *pn_dpb_luma_offset, u32 *pn_dpb_chroma_offset);
void vidc_pix_cache_set_frame_range(u32 luma_size, u32 chroma_size);
void vidc_pix_cache_set_frame_size(u32 frame_width, u32 frame_height);
void vidc_pix_cache_init_config(
	struct vidc_1080P_pix_cache_config *config);
void vidc_pix_cache_set_prefetch_page_limit(u32 page_size_limit);
void vidc_pix_cache_enable_prefetch(u32 prefetch_enable);
void vidc_pix_cache_disable_statistics(u32 statistics_off);
void vidc_pix_cache_set_port(
	enum vidc_1080P_pix_cache_port_sel port_select);
void vidc_pix_cache_enable_cache(u32 cache_enable);
void vidc_pix_cache_clear_cache_tags(void);
void vidc_pix_cache_set_halt(u32 halt_enable);
void vidc_pix_cache_get_status_idle(u32 *idle_status);
void vidc_pix_cache_set_ram(u32 ram_select);
void vidc_pix_cache_set_auto_inc_ram_addr(u32 auto_inc_enable);
void vidc_pix_cache_read_ram_data(u32 src_ram_address, u32 ram_size,
	u32 *dest_address);
void vidc_pix_cache_write_ram_data(u32 *src_address, u32 ram_size,
	u32 dest_ram_address);
void vidc_pix_cache_get_statistics(
	struct vidc_1080P_pix_cache_statistics *statistics);
void vidc_pix_cache_enable_misr(u32 misr_enable);
void vidc_pix_cache_set_misr_interface(u32 input_select);
void vidc_pix_cache_set_misr_id_filtering(
	struct vidc_1080P_pix_cache_misr_id_filtering *filter_id);
void vidc_pix_cache_set_misr_filter_trans(u32 no_of_trans);
void vidc_pix_cache_get_misr_signatures(
	struct vidc_1080P_pix_cache_misr_signature *signatures);
#endif
