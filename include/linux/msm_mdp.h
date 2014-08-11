/* include/linux/msm_mdp.h
 *
 * Copyright (C) 2007 Google Incorporated
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _MSM_MDP_H_
#define _MSM_MDP_H_

#include <uapi/linux/msm_mdp.h>

int msm_fb_get_iommu_domain(struct fb_info *info, int domain);
/* get the framebuffer physical address information */
int get_fb_phys_info(unsigned long *start, unsigned long *len, int fb_num,
	int subsys_id);
struct fb_info *msm_fb_get_writeback_fb(void);
int msm_fb_writeback_init(struct fb_info *info);
int msm_fb_writeback_start(struct fb_info *info);
int msm_fb_writeback_queue_buffer(struct fb_info *info,
		struct msmfb_data *data);
int msm_fb_writeback_dequeue_buffer(struct fb_info *info,
		struct msmfb_data *data);
int msm_fb_writeback_stop(struct fb_info *info);
int msm_fb_writeback_terminate(struct fb_info *info);
int msm_fb_writeback_set_secure(struct fb_info *info, int enable);
int msm_fb_writeback_iommu_ref(struct fb_info *info, int enable);

#endif /*_MSM_MDP_H_*/
