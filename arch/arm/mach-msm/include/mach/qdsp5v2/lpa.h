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
 *
 */
#ifndef __MACH_QDSP5_V2_LPA_H__
#define __MACH_QDSP5_V2_LPA_H__

#define LPA_OUTPUT_INTF_WB_CODEC 3
#define LPA_OUTPUT_INTF_SDAC     1
#define LPA_OUTPUT_INTF_MI2S     2

struct lpa_codec_config {
	uint32_t sample_rate;
	uint32_t sample_width;
	uint32_t output_interface;
	uint32_t num_channels;
};

struct lpa_drv;

struct lpa_drv *lpa_get(void);
void lpa_put(struct lpa_drv *lpa);
int lpa_cmd_codec_config(struct lpa_drv *lpa,
	struct lpa_codec_config *config_ptr);
int lpa_cmd_enable_codec(struct lpa_drv *lpa, bool enable);

#endif

