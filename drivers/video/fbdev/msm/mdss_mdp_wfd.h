/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2016, 2018, 2020, The Linux Foundation. All rights reserved. */

#ifndef __MDSS_MDP_WFD_H__
#define __MDSS_MDP_WFD_H__

#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/msm_mdp_ext.h>

#include "mdss_mdp.h"

struct mdss_mdp_wfd {
	struct mutex lock;
	struct list_head data_queue;
	struct mdss_mdp_ctl *ctl;
	struct device *device;
	struct completion comp;
};

struct mdss_mdp_wfd *mdss_mdp_wfd_init(struct device *device,
	struct mdss_mdp_ctl *ctl);

void mdss_mdp_wfd_deinit(struct mdss_mdp_wfd *wfd);

int mdss_mdp_wfd_setup(struct mdss_mdp_wfd *wfd,
	struct mdp_output_layer *layer);

void mdss_mdp_wfd_destroy(struct mdss_mdp_wfd *wfd);

struct mdss_mdp_wb_data *mdss_mdp_wfd_add_data(
	struct mdss_mdp_wfd *wfd,
	struct mdp_output_layer *layer);

void mdss_mdp_wfd_remove_data(struct mdss_mdp_wfd *wfd,
	struct mdss_mdp_wb_data *data);

int mdss_mdp_wfd_validate(struct mdss_mdp_wfd *wfd,
	struct mdp_output_layer *layer);

int mdss_mdp_wfd_kickoff(struct mdss_mdp_wfd *wfd,
	struct mdss_mdp_commit_cb *commit_cb);

int mdss_mdp_wfd_commit_done(struct mdss_mdp_wfd *wfd);

#endif
