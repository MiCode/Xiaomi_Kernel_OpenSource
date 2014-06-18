/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/
#ifndef DSI_DPI_H_
#define DSI_DPI_H_

#include "core/common/dsi/dsi_pipe.h"
#include "core/common/dsi/dsi_dpi_sdo.h"

struct dpi_timing {
	u16 hsync_count;
	u16 hbp_count;
	u16 hfp_count;
	u16 hactive_count;
	u16 vsync_count;
	u16 vbp_count;
	u16 vfp_count;
};

extern int dpi_pipe_power_on(struct dsi_pipe *pipe);
extern int dpi_pipe_power_off(struct dsi_pipe *pipe);
extern int dpi_pipe_mode_set(struct dsi_pipe *pipe,
		struct drm_mode_modeinfo *mode);
extern void dpi_pipe_pre_post(struct dsi_pipe *pipe);
extern int dpi_pipe_set_event(struct dsi_pipe *pipe, u8 event, bool enabled);
extern void dpi_pipe_get_events(struct dsi_pipe *pipe, u32 *events);
extern void dpi_pipe_handle_events(struct dsi_pipe *pipe, u32 events);

#endif /* DSI_DPI_H_ */
