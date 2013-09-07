/* Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_FB_H
#define MDSS_FB_H

#include <linux/msm_ion.h>
#include <linux/list.h>
#include <linux/msm_mdp.h>
#include <linux/types.h>

#include "mdss_panel.h"

#define MDSS_LPAE_CHECK(phys)	\
	((sizeof(phys) > sizeof(unsigned long)) ? ((phys >> 32) & 0xFF) : (0))

#define MSM_FB_DEFAULT_PAGE_SIZE 2
#define MFD_KEY  0x11161126
#define MSM_FB_MAX_DEV_LIST 32

#define MSM_FB_ENABLE_DBGFS
#define WAIT_FENCE_FIRST_TIMEOUT (3 * MSEC_PER_SEC)
#define WAIT_FENCE_FINAL_TIMEOUT (10 * MSEC_PER_SEC)
/* Display op timeout should be greater than total timeout */
#define WAIT_DISP_OP_TIMEOUT ((WAIT_FENCE_FIRST_TIMEOUT + \
		WAIT_FENCE_FINAL_TIMEOUT) * MDP_MAX_FENCE_FD)

#ifndef MAX
#define  MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef MIN
#define  MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

struct disp_info_type_suspend {
	int op_enable;
	int panel_power_on;
};

struct disp_info_notify {
	int type;
	struct timer_list timer;
	struct completion comp;
	struct mutex lock;
	int value;
};

struct msm_sync_pt_data {
	char *fence_name;
	u32 acq_fen_cnt;
	struct sync_fence *acq_fen[MDP_MAX_FENCE_FD];
	int cur_rel_fen_fd;
	struct sync_pt *cur_rel_sync_pt;
	struct sync_fence *cur_rel_fence;
	struct sw_sync_timeline *timeline;
	int timeline_value;
	u32 threshold;
	struct mutex sync_mutex;
};

struct msm_fb_data_type;

struct msm_mdp_interface {
	int (*fb_mem_alloc_fnc)(struct msm_fb_data_type *mfd);
	int (*fb_mem_get_iommu_domain)(void);
	int (*init_fnc)(struct msm_fb_data_type *mfd);
	int (*on_fnc)(struct msm_fb_data_type *mfd);
	int (*off_fnc)(struct msm_fb_data_type *mfd);
	/* called to release resources associated to the process */
	int (*release_fnc)(struct msm_fb_data_type *mfd);
	int (*kickoff_fnc)(struct msm_fb_data_type *mfd,
					struct mdp_display_commit *data);
	int (*ioctl_handler)(struct msm_fb_data_type *mfd, u32 cmd, void *arg);
	void (*dma_fnc)(struct msm_fb_data_type *mfd);
	int (*cursor_update)(struct msm_fb_data_type *mfd,
				struct fb_cursor *cursor);
	int (*lut_update)(struct msm_fb_data_type *mfd, struct fb_cmap *cmap);
	int (*do_histogram)(struct msm_fb_data_type *mfd,
				struct mdp_histogram *hist);
	int (*update_ad_input)(struct msm_fb_data_type *mfd);
	int (*panel_register_done)(struct mdss_panel_data *pdata);
	u32 (*fb_stride)(u32 fb_index, u32 xres, int bpp);
	struct msm_sync_pt_data *(*get_sync_fnc)(struct msm_fb_data_type *mfd,
				const struct mdp_buf_sync *buf_sync);
	void *private1;
};

#define IS_CALIB_MODE_BL(mfd) (((mfd)->calib_mode) & MDSS_CALIB_MODE_BL)
#define MDSS_BRIGHT_TO_BL(out, v, bl_max, max_bright) do {\
					out = (2 * (v) * (bl_max) + max_bright)\
					/ (2 * max_bright);\
					} while (0)

struct mdss_fb_proc_info {
	int pid;
	u32 ref_cnt;
	struct list_head list;
};

struct msm_fb_data_type {
	u32 key;
	u32 index;
	u32 ref_cnt;
	u32 fb_page;

	struct panel_id panel;
	struct mdss_panel_info *panel_info;
	int split_display;
	int split_fb_left;
	int split_fb_right;

	u32 dest;
	struct fb_info *fbi;

	int op_enable;
	u32 fb_imgType;
	int panel_reconfig;

	u32 dst_format;
	int panel_power_on;
	struct disp_info_type_suspend suspend;

	struct ion_handle *ihdl;
	dma_addr_t iova;
	void *cursor_buf;
	phys_addr_t cursor_buf_phys;
	dma_addr_t cursor_buf_iova;

	int ext_ad_ctrl;
	u32 ext_bl_ctrl;
	u32 calib_mode;
	u32 bl_level;
	u32 bl_scale;
	u32 bl_min_lvl;
	u32 unset_bl_level;
	u32 bl_updated;
	u32 bl_level_old;
	struct mutex bl_lock;
	struct mutex lock;

	struct platform_device *pdev;

	u32 mdp_fb_page_protection;

	struct disp_info_notify update;
	struct disp_info_notify no_update;
	struct completion power_off_comp;

	struct msm_mdp_interface mdp;

	struct msm_sync_pt_data mdp_sync_pt_data;

	/* for non-blocking */
	struct completion commit_comp;
	u32 is_committing;
	struct work_struct commit_work;
	void *msm_fb_backup;
	struct completion power_set_comp;
	u32 is_power_setting;

	u32 dcm_state;
	struct list_head proc_list;
};

struct msm_fb_backup_type {
	struct fb_info info;
	struct mdp_display_commit disp_commit;
};

static inline void mdss_fb_update_notify_update(struct msm_fb_data_type *mfd)
{
	int needs_complete = 0;
	mutex_lock(&mfd->update.lock);
	mfd->update.value = mfd->update.type;
	needs_complete = mfd->update.value == NOTIFY_TYPE_UPDATE;
	mutex_unlock(&mfd->update.lock);
	if (needs_complete) {
		complete(&mfd->update.comp);
		mutex_lock(&mfd->no_update.lock);
		if (mfd->no_update.timer.function)
			del_timer(&(mfd->no_update.timer));

		mfd->no_update.timer.expires = jiffies + (2 * HZ);
		add_timer(&mfd->no_update.timer);
		mutex_unlock(&mfd->no_update.lock);
	}
}

int mdss_fb_get_phys_info(dma_addr_t *start, unsigned long *len, int fb_num);
void mdss_fb_set_backlight(struct msm_fb_data_type *mfd, u32 bkl_lvl);
void mdss_fb_update_backlight(struct msm_fb_data_type *mfd);
void mdss_fb_wait_for_fence(struct msm_sync_pt_data *sync_pt_data);
void mdss_fb_signal_timeline(struct msm_sync_pt_data *sync_pt_data);
int mdss_fb_register_mdp_instance(struct msm_mdp_interface *mdp);
int mdss_fb_dcm(struct msm_fb_data_type *mfd, int req_state);
#endif /* MDSS_FB_H */
