/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __H_MTK_DISP_MGR__
#define __H_MTK_DISP_MGR__

#include "disp_session.h"
#include <linux/fs.h>

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/mutex.h>

enum PREPARE_FENCE_TYPE {
	PREPARE_INPUT_FENCE,
	PREPARE_OUTPUT_FENCE,
	PREPARE_PRESENT_FENCE
};

struct repaint_job_t {
	struct list_head link;
	unsigned int type;
};

long mtk_disp_mgr_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int disp_create_session(struct disp_session_config *config);
int disp_destroy_session(struct disp_session_config *config);
int set_session_mode(struct disp_session_config *config_info, int force);
void trigger_repaint(int type);
char *disp_session_type_str(unsigned int session_id);
void dump_input_cfg_info(struct disp_input_config *input_cfg,
			 unsigned int session_id, int is_err);
int disp_input_free_dirty_roi(struct disp_frame_cfg_t *cfg);
int disp_validate_ioctl_params(struct disp_frame_cfg_t *cfg);
int disp_mgr_has_mem_session(void);
int get_HWC_gpid(void);
#endif /* __H_MTK_DISP_MGR__ */
