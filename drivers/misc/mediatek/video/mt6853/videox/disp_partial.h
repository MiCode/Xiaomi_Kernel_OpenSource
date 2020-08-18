/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _DISP_PARTIAL_H_
#define _DISP_PARTIAL_H_

int disp_partial_is_support(void);

void disp_patial_lcm_validate_roi(struct disp_lcm_handle *plcm,
	struct disp_rect *roi);

int disp_partial_update_roi_to_lcm(disp_path_handle dp_handle,
		struct disp_rect partial, void *cmdq_handle);

int disp_partial_compute_ovl_roi(struct disp_frame_cfg_t *cfg,
		struct disp_ddp_path_config *old_cfg, struct disp_rect *result);

void assign_full_lcm_roi(struct disp_rect *roi);

int is_equal_full_lcm(const struct disp_rect *roi);

#endif
