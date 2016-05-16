/* Copyright (c) 2008-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/notifier.h>

#include "mdss_panel.h"
#include "mdss_mdp_splash_logo.h"

#define MDSS_LPAE_CHECK(phys)	\
	((sizeof(phys) > sizeof(unsigned long)) ? ((phys >> 32) & 0xFF) : (0))

#define MSM_FB_DEFAULT_PAGE_SIZE 2
#define MFD_KEY  0x11161126
#define MSM_FB_MAX_DEV_LIST 32

#define MSM_FB_ENABLE_DBGFS
#define WAIT_FENCE_FIRST_TIMEOUT (3 * MSEC_PER_SEC)
#define WAIT_FENCE_FINAL_TIMEOUT (7 * MSEC_PER_SEC)
/* Display op timeout should be greater than the total timeout but not
 * unreasonably large. Set to 1s more than first wait + final wait which
 * are already quite long and proceed without any further waits. */
#define WAIT_DISP_OP_TIMEOUT (WAIT_FENCE_FIRST_TIMEOUT + \
		WAIT_FENCE_FINAL_TIMEOUT + 1)

#ifndef MAX
#define  MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef MIN
#define  MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#define MDP_PP_AD_BL_LINEAR	0x0
#define MDP_PP_AD_BL_LINEAR_INV	0x1

/**
 * enum mdp_notify_event - Different frame events to indicate frame update state
 *
 * @MDP_NOTIFY_FRAME_BEGIN:	Frame update has started, the frame is about to
 *				be programmed into hardware.
 * @MDP_NOTIFY_FRAME_CFG_DONE:	Frame configuration is done.
 * @MDP_NOTIFY_FRAME_CTX_DONE:	Frame has finished accessing sw context.
 *				Next frame can start preparing.
 * @MDP_NOTIFY_FRAME_READY:	Frame ready to be kicked off, this can be used
 *				as the last point in time to synchronize with
 *				source buffers before kickoff.
 * @MDP_NOTIFY_FRAME_FLUSHED:	Configuration of frame has been flushed and
 *				DMA transfer has started.
 * @MDP_NOTIFY_FRAME_DONE:	Frame DMA transfer has completed.
 *				- For video mode panels this will indicate that
 *				  previous frame has been replaced by new one.
 *				- For command mode/writeback frame done happens
 *				  as soon as the DMA of the frame is done.
 * @MDP_NOTIFY_FRAME_TIMEOUT:	Frame DMA transfer has failed to complete within
 *				a fair amount of time.
 */
enum mdp_notify_event {
	MDP_NOTIFY_FRAME_BEGIN = 1,
	MDP_NOTIFY_FRAME_CFG_DONE,
	MDP_NOTIFY_FRAME_CTX_DONE,
	MDP_NOTIFY_FRAME_READY,
	MDP_NOTIFY_FRAME_FLUSHED,
	MDP_NOTIFY_FRAME_DONE,
	MDP_NOTIFY_FRAME_TIMEOUT,
};

/**
 * enum mdp_split_mode - Lists the possible split modes in the device
 *
 * @MDP_SPLIT_MODE_NONE: Not a Dual display, no panel split.
 * @MDP_SPLIT_MODE_LM:   Dual Display is true, Split across layer mixers
 * @MDP_SPLIT_MODE_DST:  Dual Display is true, Split is in the Destination
 *                      i.e ping pong split.
 */
enum mdp_split_mode {
	MDP_SPLIT_MODE_NONE,
	MDP_SPLIT_MODE_LM,
	MDP_SPLIT_MODE_DST,
};

/**
 * enum mdp_mmap_type - Lists the possible mmap type in the device
 *
 * @MDP_FB_MMAP_NONE: Unknown type.
 * @MDP_FB_MMAP_ION_ALLOC:   Use ION allocate a buffer for mmap
 * @MDP_FB_MMAP_PHYSICAL_ALLOC:  Use physical buffer for mmap
 */
enum mdp_mmap_type {
	MDP_FB_MMAP_NONE,
	MDP_FB_MMAP_ION_ALLOC,
	MDP_FB_MMAP_PHYSICAL_ALLOC,
};

struct disp_info_type_suspend {
	int op_enable;
	int panel_power_state;
};

struct disp_info_notify {
	int type;
	struct timer_list timer;
	struct completion comp;
	struct mutex lock;
	int value;
	int is_suspend;
	int ref_count;
	bool init_done;
};

struct msm_sync_pt_data {
	char *fence_name;
	u32 acq_fen_cnt;
	struct sync_fence *acq_fen[MDP_MAX_FENCE_FD];
	u32 temp_fen_cnt;
	struct sync_fence *temp_fen[MDP_MAX_FENCE_FD];

	struct sw_sync_timeline *timeline;
	int timeline_value;
	u32 threshold;
	u32 retire_threshold;
	atomic_t commit_cnt;
	bool flushed;
	bool async_wait_fences;

	struct mutex sync_mutex;
	struct notifier_block notifier;

	struct sync_fence *(*get_retire_fence)
		(struct msm_sync_pt_data *sync_pt_data);
};

struct msm_fb_data_type;

struct msm_mdp_interface {
	int (*fb_mem_alloc_fnc)(struct msm_fb_data_type *mfd);
	int (*fb_mem_get_iommu_domain)(void);
	int (*init_fnc)(struct msm_fb_data_type *mfd);
	int (*on_fnc)(struct msm_fb_data_type *mfd);
	int (*off_fnc)(struct msm_fb_data_type *mfd);
	/* called to release resources associated to the process */
	int (*release_fnc)(struct msm_fb_data_type *mfd, bool release_all,
				uint32_t pid);
	int (*kickoff_fnc)(struct msm_fb_data_type *mfd,
					struct mdp_display_commit *data);
	int (*ioctl_handler)(struct msm_fb_data_type *mfd, u32 cmd, void *arg);
	void (*dma_fnc)(struct msm_fb_data_type *mfd);
	int (*cursor_update)(struct msm_fb_data_type *mfd,
				struct fb_cursor *cursor);
	int (*lut_update)(struct msm_fb_data_type *mfd, struct fb_cmap *cmap);
	int (*do_histogram)(struct msm_fb_data_type *mfd,
				struct mdp_histogram *hist);
	int (*ad_calc_bl)(struct msm_fb_data_type *mfd, int bl_in,
		int *bl_out, bool *bl_out_notify);
	int (*ad_work_setup)(struct msm_fb_data_type *mfd);
	int (*ad_shutdown_cleanup)(struct msm_fb_data_type *mfd);
	int (*panel_register_done)(struct mdss_panel_data *pdata);
	u32 (*fb_stride)(u32 fb_index, u32 xres, int bpp);
	int (*splash_init_fnc)(struct msm_fb_data_type *mfd);
	struct msm_sync_pt_data *(*get_sync_fnc)(struct msm_fb_data_type *mfd,
				const struct mdp_buf_sync *buf_sync);
	void (*check_dsi_status)(struct work_struct *work, uint32_t interval);
	int (*configure_panel)(struct msm_fb_data_type *mfd, int mode);
	void *private1;
};

#define IS_CALIB_MODE_BL(mfd) (((mfd)->calib_mode) & MDSS_CALIB_MODE_BL)
#define MDSS_BRIGHT_TO_BL(out, v, bl_max, max_bright) do {\
					out = (2 * (v) * (bl_max) + max_bright)\
					/ (2 * max_bright);\
					} while (0)

struct mdss_fb_file_info {
	struct file *file;
	struct list_head list;
};

struct mdss_fb_proc_info {
	int pid;
	u32 ref_cnt;
	struct list_head file_list;
	struct list_head list;
};

struct msm_fb_backup_type {
	struct fb_info info;
	struct mdp_display_commit disp_commit;
};

struct msm_fb_data_type {
	u32 key;
	u32 index;
	u32 ref_cnt;
	u32 fb_page;

	struct panel_id panel;
	struct mdss_panel_info *panel_info;
	int split_mode;
	int split_fb_left;
	int split_fb_right;

	u32 dest;
	struct fb_info *fbi;

	int dispparam_level;
	int idle_time;
	struct delayed_work idle_notify_work;

	int op_enable;
	u32 fb_imgType;
	int panel_reconfig;
	u32 panel_orientation;

	u32 dst_format;
	int panel_power_state;
	struct disp_info_type_suspend suspend;

	struct ion_handle *ihdl;
	dma_addr_t iova;
	void *cursor_buf;
	phys_addr_t cursor_buf_phys;
	dma_addr_t cursor_buf_iova;

	int ext_ad_ctrl;
	u32 ext_bl_ctrl;
	u32 calib_mode;
	u32 ad_bl_level;
	u32 bl_level;
	u32 bl_scale;
	u32 bl_min_lvl;
	u32 unset_bl_level;
	u32 bl_updated;
	u32 bl_level_old;
	struct mutex bl_lock;

	struct platform_device *pdev;

	u32 mdp_fb_page_protection;

	struct disp_info_notify update;
	struct disp_info_notify no_update;
	struct completion power_off_comp;

	struct msm_mdp_interface mdp;

	struct msm_sync_pt_data mdp_sync_pt_data;

	/* for non-blocking */
	struct task_struct *disp_thread;
	atomic_t commits_pending;
	atomic_t kickoff_pending;
	wait_queue_head_t commit_wait_q;
	wait_queue_head_t idle_wait_q;
	wait_queue_head_t kickoff_wait_q;
	bool shutdown_pending;

	struct msm_fb_splash_info splash_info;

	wait_queue_head_t ioctl_q;
	atomic_t ioctl_ref_cnt;

	struct msm_fb_backup_type msm_fb_backup;
	struct completion power_set_comp;
	u32 is_power_setting;

	u32 dcm_state;
	struct list_head proc_list;
	struct ion_client *fb_ion_client;
	struct ion_handle *fb_ion_handle;
	struct dma_buf *fbmem_buf;

	bool mdss_fb_split_stored;

	u32 wait_for_kickoff;
	u32 thermal_level;
	int doze_mode;

	int fb_mmap_type;
	struct led_trigger *boot_notification_led;
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

		mfd->no_update.timer.expires = jiffies + ((1 * HZ) / 10);
		add_timer(&mfd->no_update.timer);
		mutex_unlock(&mfd->no_update.lock);
	}
}

/* Function returns true for either Layer Mixer split or Ping pong split */
static inline bool is_panel_split(struct msm_fb_data_type *mfd)
{
	return (mfd && (!(mfd->split_mode == MDP_SPLIT_MODE_NONE)));
}
/* Function returns true, if Layer Mixer split is Set*/
static inline bool is_split_lm(struct msm_fb_data_type *mfd)
{
	return (mfd && (mfd->split_mode == MDP_SPLIT_MODE_LM));
}
/* Function returns true, if Ping pong split is Set*/
static inline bool is_split_dst(struct msm_fb_data_type *mfd)
{
	return (mfd && (mfd->split_mode == MDP_SPLIT_MODE_DST));
}

static inline bool mdss_fb_is_power_off(struct msm_fb_data_type *mfd)
{
	return mdss_panel_is_power_off(mfd->panel_power_state);
}

static inline bool mdss_fb_is_power_on_interactive(
	struct msm_fb_data_type *mfd)
{
	return mdss_panel_is_power_on_interactive(mfd->panel_power_state);
}

static inline bool mdss_fb_is_power_on(struct msm_fb_data_type *mfd)
{
	return mdss_panel_is_power_on(mfd->panel_power_state);
}

static inline bool mdss_fb_is_power_on_lp(struct msm_fb_data_type *mfd)
{
	return mdss_panel_is_power_on_lp(mfd->panel_power_state);
}

static inline bool mdss_fb_is_hdmi_primary(struct msm_fb_data_type *mfd)
{
	return (mfd && (mfd->index == 0) &&
		(mfd->panel_info->type == DTV_PANEL));
}

int mdss_fb_get_phys_info(dma_addr_t *start, unsigned long *len, int fb_num);
void mdss_fb_set_backlight(struct msm_fb_data_type *mfd, u32 bkl_lvl);
void mdss_fb_update_backlight(struct msm_fb_data_type *mfd);
int mdss_fb_wait_for_fence(struct msm_sync_pt_data *sync_pt_data);
void mdss_fb_signal_timeline(struct msm_sync_pt_data *sync_pt_data);
struct sync_fence *mdss_fb_sync_get_fence(struct sw_sync_timeline *timeline,
				const char *fence_name, int val);
int mdss_fb_register_mdp_instance(struct msm_mdp_interface *mdp);
int mdss_fb_dcm(struct msm_fb_data_type *mfd, int req_state);
int mdss_fb_suspres_panel(struct device *dev, void *data);
int mdss_fb_do_ioctl(struct fb_info *info, unsigned int cmd,
		     unsigned long arg);
int mdss_fb_compat_ioctl(struct fb_info *info, unsigned int cmd,
			 unsigned long arg);
#endif /* MDSS_FB_H */
