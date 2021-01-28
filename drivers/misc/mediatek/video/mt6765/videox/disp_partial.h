/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
