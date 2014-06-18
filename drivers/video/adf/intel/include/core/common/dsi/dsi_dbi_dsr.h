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
#ifndef DSI_DBI_DSR_H_
#define DSI_DBI_DSR_H_

#include "dsi_pipe.h"

enum {
	DSR_INIT = 0,
	DSR_EXITED,
	DSR_ENTERED_LEVEL0,
	DSR_ENTERED_LEVEL1,
};

/*protected by context_lock in dsi config*/
struct dsi_dsr {
	/*dsr reference count*/
	int ref_count;

	int free_count;

	/*dsr enabled*/
	int dsr_enabled;
	/*dsr state*/
	int dsr_state;
	/*dsi config*/
	void *config;

	struct work_struct te_work;
};

int dsi_dsr_report_te(struct dsi_pipe *pipe);

int dsi_dsr_forbid(struct dsi_pipe *pipe);
int dsi_dsr_allow(struct dsi_pipe *pipe);
int dsi_dsr_forbid_locked(struct dsi_pipe *pipe);
int dsi_dsr_allow_locked(struct dsi_pipe *pipe);

int dsi_dsr_init(struct dsi_pipe *pipe);
void dsi_dsr_destroy(struct dsi_config *config);

void dsi_dsr_enable(struct dsi_pipe *pipe);

#endif /* DSI_DBI_DSR_H_ */
