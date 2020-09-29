// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sort.h>
#include <linux/clk.h>
#include <linux/bitmap.h>

#include "sde_rotator_r1_hwio.h"
#include "sde_rotator_util.h"
#include "sde_rotator_r1_internal.h"
#include "sde_rotator_core.h"

struct sde_mdp_ctl *sde_mdp_ctl_alloc(struct sde_rot_data_type *mdata,
					       u32 off)
{
	struct sde_mdp_ctl *ctl = NULL;
	static struct sde_mdp_ctl sde_ctl[5];
	static const u32 offset[] = {0x00002000, 0x00002200, 0x00002400,
			     0x00002600, 0x00002800};

	if (off >= ARRAY_SIZE(offset)) {
		SDEROT_ERR("invalid parameters\n");
		return ERR_PTR(-EINVAL);
	}

	ctl = &sde_ctl[off];
	ctl->mdata = mdata;
	ctl->num = off;
	ctl->offset = offset[ctl->num];
	ctl->base = mdata->sde_io.base + ctl->offset;
	return ctl;
}

int sde_mdp_ctl_free(struct sde_mdp_ctl *ctl)
{
	if (!ctl)
		return -ENODEV;

	if (ctl->wb)
		sde_mdp_wb_free(ctl->wb);

	ctl->is_secure = false;
	ctl->mixer_left = NULL;
	ctl->mixer_right = NULL;
	ctl->wb = NULL;
	memset(&ctl->ops, 0, sizeof(ctl->ops));

	return 0;
}

struct sde_mdp_mixer *sde_mdp_mixer_assign(u32 id, bool wb)
{
	struct sde_mdp_mixer *mixer = NULL;
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	static struct sde_mdp_mixer sde_mixer[16];
	static const u32 offset[] = {0x00048000, 0x00049000};

	if (id >= ARRAY_SIZE(offset)) {
		SDEROT_ERR("invalid parameters\n");
		return ERR_PTR(-EINVAL);
	}

	mixer = &sde_mixer[id];
	mixer->num = id;
	mixer->offset = offset[mixer->num];
	mixer->base = mdata->sde_io.base + mixer->offset;
	return mixer;
}

static void sde_mdp_mixer_setup(struct sde_mdp_ctl *master_ctl,
	int mixer_mux)
{
	int i;
	struct sde_mdp_ctl *ctl = NULL;
	struct sde_mdp_mixer *mixer = sde_mdp_mixer_get(master_ctl,
		mixer_mux);

	if (!mixer)
		return;

	ctl = mixer->ctl;
	if (!ctl)
		return;

	/* check if mixer setup for rotator is needed */
	if (mixer->rotator_mode) {
		int nmixers = 5;

		for (i = 0; i < nmixers; i++)
			sde_mdp_ctl_write(ctl, SDE_MDP_REG_CTL_LAYER(i), 0);
		return;
	}
}

struct sde_mdp_mixer *sde_mdp_mixer_get(struct sde_mdp_ctl *ctl, int mux)
{
	struct sde_mdp_mixer *mixer = NULL;

	if (!ctl) {
		SDEROT_ERR("ctl not initialized\n");
		return NULL;
	}

	switch (mux) {
	case SDE_MDP_MIXER_MUX_DEFAULT:
	case SDE_MDP_MIXER_MUX_LEFT:
		mixer = ctl->mixer_left;
		break;
	case SDE_MDP_MIXER_MUX_RIGHT:
		mixer = ctl->mixer_right;
		break;
	}

	return mixer;
}

int sde_mdp_get_pipe_flush_bits(struct sde_mdp_pipe *pipe)
{
	u32 flush_bits = 0;

	if (pipe->type == SDE_MDP_PIPE_TYPE_DMA)
		flush_bits |= BIT(pipe->num) << 5;
	else if (pipe->num == SDE_MDP_SSPP_VIG3 ||
			pipe->num == SDE_MDP_SSPP_RGB3)
		flush_bits |= BIT(pipe->num) << 10;
	else if (pipe->type == SDE_MDP_PIPE_TYPE_CURSOR)
		flush_bits |= BIT(22 + pipe->num - SDE_MDP_SSPP_CURSOR0);
	else /* RGB/VIG 0-2 pipes */
		flush_bits |= BIT(pipe->num);

	return flush_bits;
}

int sde_mdp_mixer_pipe_update(struct sde_mdp_pipe *pipe,
			 struct sde_mdp_mixer *mixer, int params_changed)
{
	struct sde_mdp_ctl *ctl;

	if (!pipe)
		return -EINVAL;
	if (!mixer)
		return -EINVAL;
	ctl = mixer->ctl;
	if (!ctl)
		return -EINVAL;

	ctl->flush_bits |= sde_mdp_get_pipe_flush_bits(pipe);
	return 0;
}

int sde_mdp_display_wait4comp(struct sde_mdp_ctl *ctl)
{
	int ret = 0;

	if (!ctl) {
		SDEROT_ERR("invalid ctl\n");
		return -ENODEV;
	}

	if (ctl->ops.wait_fnc)
		ret = ctl->ops.wait_fnc(ctl, NULL);

	return ret;
}

int sde_mdp_display_commit(struct sde_mdp_ctl *ctl, void *arg,
	struct sde_mdp_commit_cb *commit_cb)
{
	int ret = 0;
	u32 ctl_flush_bits = 0;

	if (!ctl) {
		SDEROT_ERR("display function not set\n");
		return -ENODEV;
	}

	if (ctl->ops.prepare_fnc)
		ret = ctl->ops.prepare_fnc(ctl, arg);

	if (ret) {
		SDEROT_ERR("error preparing display\n");
		goto done;
	}

	sde_mdp_mixer_setup(ctl, SDE_MDP_MIXER_MUX_LEFT);
	sde_mdp_mixer_setup(ctl, SDE_MDP_MIXER_MUX_RIGHT);

	sde_mdp_ctl_write(ctl, SDE_MDP_REG_CTL_TOP, ctl->opmode);
	ctl->flush_bits |= BIT(17);	/* CTL */

	ctl_flush_bits = ctl->flush_bits;

	sde_mdp_ctl_write(ctl, SDE_MDP_REG_CTL_FLUSH, ctl_flush_bits);
	/* ensure the flush command is issued after the barrier */
	wmb();
	ctl->flush_reg_data = ctl_flush_bits;
	ctl->flush_bits = 0;
	if (ctl->ops.display_fnc)
		ret = ctl->ops.display_fnc(ctl, arg); /* DSI0 kickoff */
	if (ret)
		SDEROT_WARN("ctl %d error displaying frame\n", ctl->num);

done:
	return ret;
}

/**
 * @sde_mdp_ctl_mixer_switch() - return ctl mixer of @return_type
 * @ctl: Pointer to ctl structure to be switched.
 * @return_type: wb_type of the ctl to be switched to.
 *
 * Virtual mixer switch should be performed only when there is no
 * dedicated wfd block and writeback block is shared.
 */
struct sde_mdp_ctl *sde_mdp_ctl_mixer_switch(struct sde_mdp_ctl *ctl,
					       u32 return_type)
{
	if (ctl->wb_type == return_type)
		return ctl;

	SDEROT_ERR("unable to switch mixer to type=%d\n", return_type);
	return NULL;
}

struct sde_mdp_writeback *sde_mdp_wb_assign(u32 num, u32 reg_index)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_mdp_writeback *wb = NULL;
	static struct sde_mdp_writeback sde_wb[16];
	static const u32 offset[] = {0x00065000, 0x00065800, 0x00066000};

	if (num >= ARRAY_SIZE(offset)) {
		SDEROT_ERR("invalid parameters\n");
		return ERR_PTR(-EINVAL);
	}

	wb = &sde_wb[num];
	wb->num = num;
	wb->offset = offset[wb->num];
	if (!wb)
		return NULL;

	wb->base = mdata->sde_io.base;
	wb->base += wb->offset;
	return wb;
}

void sde_mdp_wb_free(struct sde_mdp_writeback *wb)
{
}
