/*

SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             

*/

struct drv_hw_context;

int si_mhl_tx_drv_get_edid_fifo_next_block(struct drv_hw_context *hw_context,
										   uint8_t *edid_buf);
int si_mhl_tx_drv_set_upstream_edid(struct drv_hw_context *hw_context,
									uint8_t *edid, uint16_t length);
uint8_t	si_get_peer_mhl_version(void *drv_context);
int si_peer_supports_packed_pixel(void *drv_context);
bool si_mhl_tx_set_int(void *drv_context, uint8_t regToWrite,
					   uint8_t  mask, uint8_t priorityLevel);

uint16_t si_mhl_tx_drv_get_incoming_horizontal_total(
										struct drv_hw_context *hw_context);
uint16_t si_mhl_tx_drv_get_incoming_vertical_total(
										struct drv_hw_context *hw_context);

void si_mhl_tx_process_info_frame_change(void * driver_context
									, vendor_specific_info_frame_t *vs_info_frame
									, avi_info_frame_t *avi_info_frame);
#ifdef ENABLE_COLOR_SPACE_DEBUG_PRINT //(

void print_color_settings_impl( void *drv_context,char *pszId,int iLine);
#define print_color_settings(drv_context,id,line) print_color_settings_impl(drv_context,id,line);

#else //)(

#define print_color_settings(drv_context,id,line)

#endif //)
#if 0
void si_mhl_tx_tmds_enable( void *drv_context);
#endif // 0
