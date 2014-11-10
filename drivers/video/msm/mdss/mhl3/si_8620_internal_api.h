/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

struct drv_hw_context;

void si_mhl_tx_drv_skip_to_next_edid_block(struct drv_hw_context *hw_context);
int si_mhl_tx_drv_get_edid_fifo_partial_block(struct drv_hw_context *hw_context,
	uint8_t start, uint8_t length, uint8_t *edid_buf);
int si_mhl_tx_drv_get_edid_fifo_next_block(struct drv_hw_context *hw_context,
	uint8_t *edid_buf);
int si_mhl_tx_drv_set_upstream_edid(struct drv_hw_context *hw_context,
	uint8_t *edid, uint16_t length);
void setup_sans_cbus1(struct mhl_dev_context *dev_context);
uint8_t si_get_peer_mhl_version(struct mhl_dev_context *drv_context);
int si_peer_supports_packed_pixel(void *drv_context);

uint16_t si_mhl_tx_drv_get_incoming_timing(struct drv_hw_context *hw_context,
	struct si_incoming_timing_t *p_timing);
