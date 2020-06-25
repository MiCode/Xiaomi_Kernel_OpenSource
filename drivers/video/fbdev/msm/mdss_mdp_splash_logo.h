/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2014, 2018, 2020, The Linux Foundation. All rights reserved. */
#ifndef MDSS_MDP_SPLASH_LOGO
#define MDSS_MDP_SPLASH_LOGO

#include <linux/types.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/completion.h>

struct msm_fb_splash_info {
	struct task_struct	*splash_thread;
	bool			splash_logo_enabled;
	bool			iommu_dynamic_attached;
	struct notifier_block	notifier;
	uint32_t		frame_done_count;
	struct completion	frame_done;

	struct dma_buf		*dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table		*table;
	dma_addr_t		iova;
	void			*splash_buffer;
	int			pipe_ndx[2];
	bool			splash_pipe_allocated;
	uint32_t		size;
};

struct msm_fb_data_type;

void mdss_mdp_release_splash_pipe(struct msm_fb_data_type *mfd);
int mdss_mdp_splash_cleanup(struct msm_fb_data_type *mfd,
				 bool use_borderfill);
int mdss_mdp_splash_init(struct msm_fb_data_type *mfd);

#endif
