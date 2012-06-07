/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#include "vidc_hwio_reg.h"
#include "vidc_hwio.h"
#include "vidc_pix_cache.h"


#define VIDC_1080P_MAX_DEC_DPB 19
#define VIDC_TILE_MULTIPLY_FACTOR 8192

void vidc_pix_cache_sw_reset(void)
{
	u32 sw_reset_value = 0;

	VIDC_HWIO_IN(REG_169013, &sw_reset_value);
	sw_reset_value |= HWIO_REG_169013_PIX_CACHE_SW_RESET_BMSK;
	VIDC_HWIO_OUT(REG_169013, sw_reset_value);
	VIDC_HWIO_IN(REG_169013, &sw_reset_value);
	sw_reset_value &= (~HWIO_REG_169013_PIX_CACHE_SW_RESET_BMSK);
	VIDC_HWIO_OUT(REG_169013, sw_reset_value);
}

void vidc_pix_cache_init_luma_chroma_base_addr(u32 dpb,
	u32 *pn_dpb_luma_offset, u32 *pn_dpb_chroma_offset)
{
	u32 count, num_dpb_used = dpb;
	u32 dpb_reset_value = VIDC_1080P_DEC_DPB_RESET_VALUE;

	if (num_dpb_used > VIDC_1080P_MAX_DEC_DPB)
		num_dpb_used = VIDC_1080P_MAX_DEC_DPB;
	for (count = 0; count < VIDC_1080P_MAX_DEC_DPB; count++) {
		if (count < num_dpb_used) {
			if (pn_dpb_luma_offset) {
				VIDC_HWIO_OUTI(
					REG_804925,
					count, pn_dpb_luma_offset[count]);
			} else {
				VIDC_HWIO_OUTI(
					REG_804925,
					count, dpb_reset_value);
			}
			if (pn_dpb_chroma_offset) {
				VIDC_HWIO_OUTI(
					REG_41909,
					count, pn_dpb_chroma_offset[count]);
			} else {
				VIDC_HWIO_OUTI(
					REG_41909,
					count, dpb_reset_value);
			}
		} else {
			VIDC_HWIO_OUTI(REG_804925,
				count, dpb_reset_value);
			VIDC_HWIO_OUTI(REG_41909,
				count, dpb_reset_value);
		}
	}
}

void vidc_pix_cache_set_frame_range(u32 luma_size, u32 chroma_size)
{
	u32 frame_range;

	frame_range =
		(((luma_size / VIDC_TILE_MULTIPLY_FACTOR) & 0xFF) << 8)|
		((chroma_size / VIDC_TILE_MULTIPLY_FACTOR) & 0xFF);
	VIDC_HWIO_OUT(REG_905239, frame_range);
}
void vidc_pix_cache_set_frame_size(u32 frame_width, u32 frame_height)
{
   u32 frame_size;
   frame_size =  (((u32) (frame_height << HWIO_REG_951731_FRAME_HEIGHT_SHFT) &
		HWIO_REG_951731_FRAME_HEIGHT_BMSK) |
		((u32) (frame_width << HWIO_REG_951731_FRAME_WIDTH_SHFT) &
		 HWIO_REG_951731_FRAME_WIDTH_BMSK));
   VIDC_HWIO_OUT(REG_951731, frame_size);
}

void vidc_pix_cache_init_config(
	struct vidc_1080P_pix_cache_config *config)
{
	u32 cfg_reg = 0;

	if (config->cache_enable)
		cfg_reg |= HWIO_REG_22756_CACHE_EN_BMSK;
	else
		cfg_reg &= (~HWIO_REG_22756_CACHE_EN_BMSK);
	if (config->port_select == VIDC_1080P_PIX_CACHE_PORT_A)
		cfg_reg &=
			(~HWIO_REG_22756_CACHE_PORT_SELECT_BMSK);
	else
		cfg_reg |= HWIO_REG_22756_CACHE_PORT_SELECT_BMSK;
	if (!config->statistics_off)
		cfg_reg |= HWIO_REG_22756_STATISTICS_OFF_BMSK;
	else
		cfg_reg &= (~HWIO_REG_22756_STATISTICS_OFF_BMSK);
	if (config->prefetch_en)
		cfg_reg |= HWIO_REG_22756_PREFETCH_EN_BMSK;
	else
		cfg_reg &= (~HWIO_REG_22756_PREFETCH_EN_BMSK);
	cfg_reg &= (~HWIO_REG_22756_PAGE_SIZE_BMSK);
	cfg_reg |= VIDC_SETFIELD(config->page_size,
			HWIO_REG_22756_PAGE_SIZE_SHFT,
			HWIO_REG_22756_PAGE_SIZE_BMSK);
	VIDC_HWIO_OUT(REG_22756, cfg_reg);
}

void vidc_pix_cache_set_prefetch_page_limit(u32 page_size_limit)
{
	u32 cfg_reg = 0;

	VIDC_HWIO_IN(REG_22756, &cfg_reg);
	cfg_reg &= (~HWIO_REG_22756_PAGE_SIZE_BMSK);
	cfg_reg |= VIDC_SETFIELD(page_size_limit,
			HWIO_REG_22756_PAGE_SIZE_SHFT,
			HWIO_REG_22756_PAGE_SIZE_BMSK);
	VIDC_HWIO_OUT(REG_22756, cfg_reg);
}

void vidc_pix_cache_enable_prefetch(u32 prefetch_enable)
{
	u32 cfg_reg = 0;

	VIDC_HWIO_IN(REG_22756, &cfg_reg);
	if (prefetch_enable)
		cfg_reg |= HWIO_REG_22756_PREFETCH_EN_BMSK;
	else
		cfg_reg &= (~HWIO_REG_22756_PREFETCH_EN_BMSK);
	VIDC_HWIO_OUT(REG_22756, cfg_reg);
}

void vidc_pix_cache_disable_statistics(u32 statistics_off)
{
	u32 cfg_reg = 0;

	VIDC_HWIO_IN(REG_22756, &cfg_reg);
	if (!statistics_off)
		cfg_reg |= HWIO_REG_22756_STATISTICS_OFF_BMSK;
	else
		cfg_reg &= (~HWIO_REG_22756_STATISTICS_OFF_BMSK);
	VIDC_HWIO_OUT(REG_22756, cfg_reg);
}

void vidc_pix_cache_set_port(
	enum vidc_1080P_pix_cache_port_sel port_select)
{
	u32 cfg_reg = 0;

	VIDC_HWIO_IN(REG_22756, &cfg_reg);
	if (port_select == VIDC_1080P_PIX_CACHE_PORT_A)
		cfg_reg &=
			(~HWIO_REG_22756_CACHE_PORT_SELECT_BMSK);
	else
		cfg_reg |= HWIO_REG_22756_CACHE_PORT_SELECT_BMSK;
	VIDC_HWIO_OUT(REG_22756, cfg_reg);
}

void vidc_pix_cache_enable_cache(u32 cache_enable)
{
	u32 cfg_reg = 0;

	VIDC_HWIO_IN(REG_22756, &cfg_reg);
	if (cache_enable)
		cfg_reg |= HWIO_REG_22756_CACHE_EN_BMSK;
	else
		cfg_reg &= (~HWIO_REG_22756_CACHE_EN_BMSK);
	VIDC_HWIO_OUT(REG_22756, cfg_reg);
}

void vidc_pix_cache_clear_cache_tags(void)
{
	u32 cfg_reg = 0;

	VIDC_HWIO_IN(REG_22756, &cfg_reg);
	cfg_reg |= HWIO_REG_22756_CACHE_TAG_CLEAR_BMSK;
	VIDC_HWIO_OUT(REG_22756, cfg_reg);
	VIDC_HWIO_IN(REG_22756, &cfg_reg);
	cfg_reg &= (~HWIO_REG_22756_CACHE_TAG_CLEAR_BMSK);
	VIDC_HWIO_OUT(REG_22756, cfg_reg);
}

void vidc_pix_cache_set_halt(u32 halt_enable)
{
	u32 cfg_reg = 0;

	VIDC_HWIO_IN(REG_22756, &cfg_reg);
	if (halt_enable)
		cfg_reg |= HWIO_REG_22756_CACHE_HALT_BMSK;
	else
		cfg_reg &= (~HWIO_REG_22756_CACHE_HALT_BMSK);
	VIDC_HWIO_OUT(REG_22756, cfg_reg);
}

void vidc_pix_cache_get_status_idle(u32 *idle_status)
{
	VIDC_HWIO_IN(REG_919904, idle_status);
}

void vidc_pix_cache_set_ram(u32 ram_select)
{
	u32 dmi_cfg_reg = 0;

	VIDC_HWIO_IN(REG_261029, &dmi_cfg_reg);
	dmi_cfg_reg &= (~HWIO_REG_261029_DMI_RAM_SEL_BMSK);
	dmi_cfg_reg |= VIDC_SETFIELD(ram_select,
			HWIO_REG_261029_AUTO_INC_EN_SHFT,
			HWIO_REG_261029_DMI_RAM_SEL_BMSK);
	VIDC_HWIO_OUT(REG_261029, dmi_cfg_reg);
}

void vidc_pix_cache_set_auto_inc_ram_addr(u32 auto_inc_enable)
{
	u32 dmi_cfg_reg = 0;

	VIDC_HWIO_IN(REG_261029, &dmi_cfg_reg);
	if (auto_inc_enable)
		dmi_cfg_reg |= HWIO_REG_261029_AUTO_INC_EN_BMSK;
	else
		dmi_cfg_reg &= (~HWIO_REG_261029_AUTO_INC_EN_BMSK);
	VIDC_HWIO_OUT(REG_261029, dmi_cfg_reg);
}


void vidc_pix_cache_read_ram_data(u32 src_ram_address,
	u32 ram_size, u32 *dest_address)
{
	u32 count, dmi_cfg_reg = 0;

	VIDC_HWIO_IN(REG_261029, &dmi_cfg_reg);
	VIDC_HWIO_OUT(REG_576200, src_ram_address);
	vidc_pix_cache_set_auto_inc_ram_addr(1);
	for (count = 0; count < ram_size; count++) {
		VIDC_HWIO_IN(REG_556274, dest_address);
		dest_address++;
		VIDC_HWIO_IN(REG_917583, dest_address);
		dest_address++;
	}
	VIDC_HWIO_OUT(REG_261029, dmi_cfg_reg);
}

void vidc_pix_cache_write_ram_data(u32 *src_address,
	u32 ram_size, u32 dest_ram_address)
{
	u32 count, dmi_cfg_reg = 0;

	VIDC_HWIO_IN(REG_261029, &dmi_cfg_reg);
	VIDC_HWIO_OUT(REG_576200, dest_ram_address);
	vidc_pix_cache_set_auto_inc_ram_addr(1);
	for (count = 0; count < ram_size; count++) {
		VIDC_HWIO_OUT(REG_917583, *src_address);
		src_address++;
		VIDC_HWIO_OUT(REG_556274, *src_address);
		src_address++;
	}
	VIDC_HWIO_OUT(REG_261029, dmi_cfg_reg);
}

void vidc_pix_cache_get_statistics(
	struct vidc_1080P_pix_cache_statistics *statistics)
{
	VIDC_HWIO_IN(REG_278310,
		&statistics->access_miss);
	VIDC_HWIO_IN(REG_421222,
		&statistics->access_hit);
	VIDC_HWIO_IN(REG_609607,
		&statistics->axi_req);
	VIDC_HWIO_IN(REG_395232,
		&statistics->core_req);
	VIDC_HWIO_IN(REG_450146,
		&statistics->axi_bus);
	VIDC_HWIO_IN(REG_610651,
		&statistics->core_bus);
}

void vidc_pix_cache_enable_misr(u32 misr_enable)
{
   u32 misr_cfg_reg = 0;

	VIDC_HWIO_IN(REG_883784, &misr_cfg_reg);
	if (misr_enable)
		misr_cfg_reg |= HWIO_REG_883784_MISR_EN_BMSK;
	else
		misr_cfg_reg &=
			(~HWIO_REG_883784_MISR_EN_BMSK);
	VIDC_HWIO_OUT(REG_261029, misr_cfg_reg);
}

void vidc_pix_cache_set_misr_interface(u32 input_select)
{
	u32 misr_cfg_reg = 0;

	VIDC_HWIO_IN(REG_883784, &misr_cfg_reg);
	misr_cfg_reg &= (~HWIO_REG_883784_INPUT_SEL_BMSK);
	misr_cfg_reg |= VIDC_SETFIELD(input_select,
			HWIO_REG_883784_INPUT_SEL_SHFT,
			HWIO_REG_883784_INPUT_SEL_BMSK);
	VIDC_HWIO_OUT(REG_261029, misr_cfg_reg);
}

void vidc_pix_cache_set_misr_id_filtering(
	struct vidc_1080P_pix_cache_misr_id_filtering *filter_id)
{
	u32 misr_cfg_reg = 0;

	VIDC_HWIO_IN(REG_883784, &misr_cfg_reg);
	if (filter_id->ignore_id)
		misr_cfg_reg |=
			HWIO_REG_883784_IGNORE_ID_BMSK;
	else
		misr_cfg_reg &=
			(~HWIO_REG_883784_IGNORE_ID_BMSK);
	misr_cfg_reg &= (~HWIO_REG_883784_ID_BMSK);
	misr_cfg_reg |= VIDC_SETFIELD(filter_id->id,
			HWIO_REG_883784_ID_SHFT,
			HWIO_REG_883784_ID_BMSK);
	VIDC_HWIO_OUT(REG_261029, misr_cfg_reg);
}

void vidc_pix_cache_set_misr_filter_trans(u32 no_of_trans)
{
	u32 misr_cfg_reg = 0;

	VIDC_HWIO_IN(REG_883784, &misr_cfg_reg);
	misr_cfg_reg &= (~HWIO_REG_883784_COUNTER_BMSK);
	misr_cfg_reg |= VIDC_SETFIELD(no_of_trans,
			HWIO_REG_883784_COUNTER_SHFT,
			HWIO_REG_883784_COUNTER_BMSK);
	VIDC_HWIO_OUT(REG_261029, misr_cfg_reg);
}

void vidc_pix_cache_get_misr_signatures(
	struct vidc_1080P_pix_cache_misr_signature *signatures)
{
	VIDC_HWIO_INI(REG_651391, 0,
		&signatures->signature0);
	VIDC_HWIO_INI(REG_651391, 1,
		&signatures->signature1);
}
