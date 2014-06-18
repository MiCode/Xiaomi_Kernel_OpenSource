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

#ifndef DSI_DPI_SDO_H_
#define DSI_DPI_SDO_H_

#include "dsi_pipe.h"

enum {
	SDO_INIT = 0,
	SDO_EXITED,
	SDO_ENTERED_LEVEL0,
};

struct dsi_sdo {
	/* SDO reference count */
	int ref_count;

	/* Check how many times of repeated frame to enter SDO */
	int free_count;

	int sdo_enabled;
	int sdo_state;

	/*dsi config*/
	void *config;

	struct work_struct repeated_frm_work;
};

int dsi_sdo_report_repeated_frame(struct dsi_pipe *pipe);

int dsi_sdo_forbid(struct dsi_pipe *pipe);
int dsi_sdo_allow(struct dsi_pipe *pipe);

int dsi_sdo_init(struct dsi_pipe *pipe);
void dsi_sdo_destroy(struct dsi_config *config);

void dsi_sdo_enable(struct dsi_pipe *pipe);
void dsi_sdo_disable(struct dsi_pipe *pipe);

#endif /* DSI_DPI_SDO_H_ */
