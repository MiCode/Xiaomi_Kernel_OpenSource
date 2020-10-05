/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_ROTATOR_R1_INTERNAL_H__
#define __SDE_ROTATOR_R1_INTERNAL_H__

#include <linux/types.h>
#include <linux/file.h>
#include <linux/kref.h>
#include <linux/kernel.h>

#include "sde_rotator_util.h"

/**
 * enum sde_commit_stage_type - Indicate different commit stages
 */
enum sde_commit_stage_type {
	SDE_COMMIT_STAGE_SETUP_DONE,
	SDE_COMMIT_STAGE_READY_FOR_KICKOFF,
};

enum sde_mdp_wb_ctl_type {
	SDE_MDP_WB_CTL_TYPE_BLOCK = 1,
	SDE_MDP_WB_CTL_TYPE_LINE
};

enum sde_mdp_mixer_mux {
	SDE_MDP_MIXER_MUX_DEFAULT,
	SDE_MDP_MIXER_MUX_LEFT,
	SDE_MDP_MIXER_MUX_RIGHT,
};

enum sde_mdp_pipe_type {
	SDE_MDP_PIPE_TYPE_UNUSED,
	SDE_MDP_PIPE_TYPE_VIG,
	SDE_MDP_PIPE_TYPE_RGB,
	SDE_MDP_PIPE_TYPE_DMA,
	SDE_MDP_PIPE_TYPE_CURSOR,
};

struct sde_mdp_data;
struct sde_mdp_ctl;
struct sde_mdp_pipe;
struct sde_mdp_mixer;
struct sde_mdp_wb;

struct sde_mdp_writeback {
	u32 num;
	char __iomem *base;
	u32 offset;
};

struct sde_mdp_ctl_intfs_ops {
	int (*start_fnc)(struct sde_mdp_ctl *ctl);
	int (*stop_fnc)(struct sde_mdp_ctl *ctl, int panel_power_state);
	int (*prepare_fnc)(struct sde_mdp_ctl *ctl, void *arg);
	int (*display_fnc)(struct sde_mdp_ctl *ctl, void *arg);
	int (*wait_fnc)(struct sde_mdp_ctl *ctl, void *arg);
};

struct sde_mdp_ctl {
	u32 num;
	char __iomem *base;
	u32 opmode;
	u32 flush_bits;
	u32 flush_reg_data;
	bool is_secure;
	struct sde_rot_data_type *mdata;
	struct sde_mdp_mixer *mixer_left;
	struct sde_mdp_mixer *mixer_right;
	void *priv_data;
	u32 wb_type;
	struct sde_mdp_writeback *wb;
	struct sde_mdp_ctl_intfs_ops ops;
	u32 offset;
	int irq_num;
};

struct sde_mdp_mixer {
	u32 num;
	char __iomem *base;
	u8 rotator_mode;
	struct sde_mdp_ctl *ctl;
	u32 offset;
};

struct sde_mdp_shared_reg_ctrl {
	u32 reg_off;
	u32 bit_off;
};

struct sde_mdp_pipe {
	u32 num;
	u32 type;
	u32 ndx;
	char __iomem *base;
	u32 xin_id;
	u32 flags;
	u32 bwc_mode;
	u16 img_width;
	u16 img_height;
	u8 horz_deci;
	u8 vert_deci;
	struct sde_rect src;
	struct sde_rect dst;
	struct sde_mdp_format_params *src_fmt;
	struct sde_mdp_plane_sizes src_planes;
	struct sde_mdp_mixer *mixer_left;
	struct sde_mdp_mixer *mixer_right;
	struct sde_mdp_shared_reg_ctrl clk_ctrl;
	u32 params_changed;
	u32 offset;
};

struct sde_mdp_writeback_arg {
	struct sde_mdp_data *data;
	void *priv_data;
};

struct sde_mdp_commit_cb {
	void *data;
	int (*commit_cb_fnc)(enum sde_commit_stage_type commit_state,
		void *data);
};

static inline void sde_mdp_ctl_write(struct sde_mdp_ctl *ctl,
				      u32 reg, u32 val)
{
	SDEROT_DBG("ctl%d:%6.6x:%8.8x\n", ctl->num, ctl->offset + reg, val);
	writel_relaxed(val, ctl->base + reg);
}

static inline bool sde_mdp_is_nrt_vbif_client(struct sde_rot_data_type *mdata,
					struct sde_mdp_pipe *pipe)
{
	return mdata->vbif_nrt_io.base && pipe->mixer_left &&
			pipe->mixer_left->rotator_mode;
}
int sde_mdp_set_intr_callback(u32 intr_type, u32 intf_num,
				void (*fnc_ptr)(void *), void *arg);
int sde_mdp_display_wait4comp(struct sde_mdp_ctl *ctl);
int sde_mdp_writeback_display_commit(struct sde_mdp_ctl *ctl, void *arg);
int sde_mdp_pipe_queue_data(struct sde_mdp_pipe *pipe,
			     struct sde_mdp_data *src_data);
struct sde_mdp_ctl *sde_mdp_ctl_alloc(struct sde_rot_data_type *mdata,
					       u32 off);
struct sde_mdp_writeback *sde_mdp_wb_assign(u32 num, u32 reg_index);
void sde_mdp_wb_free(struct sde_mdp_writeback *wb);
struct sde_mdp_mixer *sde_mdp_mixer_assign(u32 id, bool wb);
int sde_mdp_writeback_start(struct sde_mdp_ctl *ctl);
struct sde_mdp_pipe *sde_mdp_pipe_assign(struct sde_rot_data_type *mdata,
	struct sde_mdp_mixer *mixer, u32 ndx);
int sde_mdp_pipe_destroy(struct sde_mdp_pipe *pipe);
int sde_mdp_ctl_free(struct sde_mdp_ctl *ctl);
int sde_mdp_display_commit(struct sde_mdp_ctl *ctl, void *arg,
	struct sde_mdp_commit_cb *commit_cb);
int sde_mdp_mixer_pipe_update(struct sde_mdp_pipe *pipe,
			 struct sde_mdp_mixer *mixer, int params_changed);
int sde_mdp_get_pipe_flush_bits(struct sde_mdp_pipe *pipe);
struct sde_mdp_ctl *sde_mdp_ctl_mixer_switch(struct sde_mdp_ctl *ctl,
					       u32 return_type);
struct sde_mdp_mixer *sde_mdp_mixer_get(struct sde_mdp_ctl *ctl, int mux);
#endif /* __SDE_ROTATOR_R1_INTERNAL_H__ */
