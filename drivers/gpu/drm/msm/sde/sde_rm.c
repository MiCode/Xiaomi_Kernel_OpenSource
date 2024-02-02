/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm:%s] " fmt, __func__
#include "sde_kms.h"
#include "sde_hw_lm.h"
#include "sde_hw_ctl.h"
#include "sde_hw_cdm.h"
#include "sde_hw_dspp.h"
#include "sde_hw_ds.h"
#include "sde_hw_pingpong.h"
#include "sde_hw_intf.h"
#include "sde_hw_wb.h"
#include "sde_encoder.h"
#include "sde_connector.h"
#include "sde_hw_dsc.h"
#include "sde_hw_rot.h"
#include "sde_crtc.h"
#include "sde_hw_qdss.h"

#define RESERVED_BY_OTHER(h, r) \
	((h)->rsvp && ((h)->rsvp->enc_id != (r)->enc_id))

#define RM_RQ_LOCK(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_RESERVE_LOCK))
#define RM_RQ_CLEAR(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_RESERVE_CLEAR))
#define RM_RQ_DSPP(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_DSPP))
#define RM_RQ_DS(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_DS))
#define RM_RQ_CWB(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_CWB))
#define RM_IS_TOPOLOGY_MATCH(t, r) ((t).num_lm == (r).num_lm && \
				(t).num_comp_enc == (r).num_enc && \
				(t).num_intf == (r).num_intf)

/**
 * toplogy information to be used when ctl path version does not
 * support driving more than one interface per ctl_path
 */
static const struct sde_rm_topology_def g_top_table[] = {
	{   SDE_RM_TOPOLOGY_NONE,                 0, 0, 0, 0, false },
	{   SDE_RM_TOPOLOGY_SINGLEPIPE,           1, 0, 1, 1, false },
	{   SDE_RM_TOPOLOGY_SINGLEPIPE_DSC,       1, 1, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE,             2, 0, 2, 2, true  },
	{   SDE_RM_TOPOLOGY_DUALPIPE_DSC,         2, 2, 2, 2, true  },
	{   SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE,     2, 0, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE_DSC, 2, 1, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE_DSCMERGE,    2, 2, 1, 1, false },
	{   SDE_RM_TOPOLOGY_PPSPLIT,              1, 0, 2, 1, true  },
};

/**
 * topology information to be used when the ctl path version
 * is SDE_CTL_CFG_VERSION_1_0_0
 */
static const struct sde_rm_topology_def g_ctl_ver_1_top_table[] = {
	{   SDE_RM_TOPOLOGY_NONE,                 0, 0, 0, 0, false },
	{   SDE_RM_TOPOLOGY_SINGLEPIPE,           1, 0, 1, 1, false },
	{   SDE_RM_TOPOLOGY_SINGLEPIPE_DSC,       1, 1, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE,             2, 0, 2, 1, true  },
	{   SDE_RM_TOPOLOGY_DUALPIPE_DSC,         2, 2, 2, 1, true  },
	{   SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE,     2, 0, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE_DSC, 2, 1, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE_DSCMERGE,    2, 2, 1, 1, false },
	{   SDE_RM_TOPOLOGY_PPSPLIT,              1, 0, 2, 1, true  },
};


/**
 * struct sde_rm_requirements - Reservation requirements parameter bundle
 * @top_ctrl:  topology control preference from kernel client
 * @top:       selected topology for the display
 * @hw_res:	   Hardware resources required as reported by the encoders
 */
struct sde_rm_requirements {
	uint64_t top_ctrl;
	const struct sde_rm_topology_def *topology;
	struct sde_encoder_hw_resources hw_res;
};

/**
 * struct sde_rm_rsvp - Use Case Reservation tagging structure
 *	Used to tag HW blocks as reserved by a CRTC->Encoder->Connector chain
 *	By using as a tag, rather than lists of pointers to HW blocks used
 *	we can avoid some list management since we don't know how many blocks
 *	of each type a given use case may require.
 * @list:	List head for list of all reservations
 * @seq:	Global RSVP sequence number for debugging, especially for
 *		differentiating differenct allocations for same encoder.
 * @enc_id:	Reservations are tracked by Encoder DRM object ID.
 *		CRTCs may be connected to multiple Encoders.
 *		An encoder or connector id identifies the display path.
 * @topology	DRM<->HW topology use case
 */
struct sde_rm_rsvp {
	struct list_head list;
	uint32_t seq;
	uint32_t enc_id;
	enum sde_rm_topology_name topology;
};

/**
 * struct sde_rm_hw_blk - hardware block tracking list member
 * @list:	List head for list of all hardware blocks tracking items
 * @rsvp:	Pointer to use case reservation if reserved by a client
 * @rsvp_nxt:	Temporary pointer used during reservation to the incoming
 *		request. Will be swapped into rsvp if proposal is accepted
 * @type:	Type of hardware block this structure tracks
 * @id:		Hardware ID number, within it's own space, ie. LM_X
 * @catalog:	Pointer to the hardware catalog entry for this block
 * @hw:		Pointer to the hardware register access object for this block
 */
struct sde_rm_hw_blk {
	struct list_head list;
	struct sde_rm_rsvp *rsvp;
	struct sde_rm_rsvp *rsvp_nxt;
	enum sde_hw_blk_type type;
	uint32_t id;
	struct sde_hw_blk *hw;
};

/**
 * sde_rm_dbg_rsvp_stage - enum of steps in making reservation for event logging
 */
enum sde_rm_dbg_rsvp_stage {
	SDE_RM_STAGE_BEGIN,
	SDE_RM_STAGE_AFTER_CLEAR,
	SDE_RM_STAGE_AFTER_RSVPNEXT,
	SDE_RM_STAGE_FINAL
};

static void _sde_rm_print_rsvps(
		struct sde_rm *rm,
		enum sde_rm_dbg_rsvp_stage stage)
{
	struct sde_rm_rsvp *rsvp;
	struct sde_rm_hw_blk *blk;
	enum sde_hw_blk_type type;

	SDE_DEBUG("%d\n", stage);

	list_for_each_entry(rsvp, &rm->rsvps, list) {
		SDE_DEBUG("%d rsvp[s%ue%u] topology %d\n", stage, rsvp->seq,
				rsvp->enc_id, rsvp->topology);
		SDE_EVT32(stage, rsvp->seq, rsvp->enc_id, rsvp->topology);
	}

	for (type = 0; type < SDE_HW_BLK_MAX; type++) {
		list_for_each_entry(blk, &rm->hw_blks[type], list) {
			if (!blk->rsvp && !blk->rsvp_nxt)
				continue;

			SDE_DEBUG("%d rsvp[s%ue%u->s%ue%u] %d %d\n", stage,
				(blk->rsvp) ? blk->rsvp->seq : 0,
				(blk->rsvp) ? blk->rsvp->enc_id : 0,
				(blk->rsvp_nxt) ? blk->rsvp_nxt->seq : 0,
				(blk->rsvp_nxt) ? blk->rsvp_nxt->enc_id : 0,
				blk->type, blk->id);

			SDE_EVT32(stage,
				(blk->rsvp) ? blk->rsvp->seq : 0,
				(blk->rsvp) ? blk->rsvp->enc_id : 0,
				(blk->rsvp_nxt) ? blk->rsvp_nxt->seq : 0,
				(blk->rsvp_nxt) ? blk->rsvp_nxt->enc_id : 0,
				blk->type, blk->id);
		}
	}
}

struct sde_hw_mdp *sde_rm_get_mdp(struct sde_rm *rm)
{
	return rm->hw_mdp;
}

void sde_rm_init_hw_iter(
		struct sde_rm_hw_iter *iter,
		uint32_t enc_id,
		enum sde_hw_blk_type type)
{
	memset(iter, 0, sizeof(*iter));
	iter->enc_id = enc_id;
	iter->type = type;
}

enum sde_rm_topology_name sde_rm_get_topology_name(
	struct msm_display_topology topology)
{
	int i;

	for (i = 0; i < SDE_RM_TOPOLOGY_MAX; i++)
		if (RM_IS_TOPOLOGY_MATCH(g_top_table[i], topology))
			return g_top_table[i].top_name;

	return SDE_RM_TOPOLOGY_NONE;
}

static bool _sde_rm_get_hw_locked(struct sde_rm *rm, struct sde_rm_hw_iter *i)
{
	struct list_head *blk_list;

	if (!rm || !i || i->type >= SDE_HW_BLK_MAX) {
		SDE_ERROR("invalid rm\n");
		return false;
	}

	i->hw = NULL;
	blk_list = &rm->hw_blks[i->type];

	if (i->blk && (&i->blk->list == blk_list)) {
		SDE_DEBUG("attempt resume iteration past last\n");
		return false;
	}

	i->blk = list_prepare_entry(i->blk, blk_list, list);

	list_for_each_entry_continue(i->blk, blk_list, list) {
		struct sde_rm_rsvp *rsvp = i->blk->rsvp;

		if (i->blk->type != i->type) {
			SDE_ERROR("found incorrect block type %d on %d list\n",
					i->blk->type, i->type);
			return false;
		}

		if ((i->enc_id == 0) || (rsvp && rsvp->enc_id == i->enc_id)) {
			i->hw = i->blk->hw;
			SDE_DEBUG("found type %d id %d for enc %d\n",
					i->type, i->blk->id, i->enc_id);
			return true;
		}
	}

	SDE_DEBUG("no match, type %d for enc %d\n", i->type, i->enc_id);

	return false;
}

static bool _sde_rm_request_hw_blk_locked(struct sde_rm *rm,
		struct sde_rm_hw_request *hw_blk_info)
{
	struct list_head *blk_list;
	struct sde_rm_hw_blk *blk = NULL;

	if (!rm || !hw_blk_info || hw_blk_info->type >= SDE_HW_BLK_MAX) {
		SDE_ERROR("invalid rm\n");
		return false;
	}

	hw_blk_info->hw = NULL;
	blk_list = &rm->hw_blks[hw_blk_info->type];

	blk = list_prepare_entry(blk, blk_list, list);

	list_for_each_entry_continue(blk, blk_list, list) {
		if (blk->type != hw_blk_info->type) {
			SDE_ERROR("found incorrect block type %d on %d list\n",
					blk->type, hw_blk_info->type);
			return false;
		}

		if (blk->hw->id == hw_blk_info->id) {
			hw_blk_info->hw = blk->hw;
			SDE_DEBUG("found type %d id %d\n",
					blk->type, blk->id);
			return true;
		}
	}

	SDE_DEBUG("no match, type %d id %d\n", hw_blk_info->type,
			hw_blk_info->id);

	return false;
}

bool sde_rm_get_hw(struct sde_rm *rm, struct sde_rm_hw_iter *i)
{
	bool ret;

	mutex_lock(&rm->rm_lock);
	ret = _sde_rm_get_hw_locked(rm, i);
	mutex_unlock(&rm->rm_lock);

	return ret;
}

bool sde_rm_request_hw_blk(struct sde_rm *rm, struct sde_rm_hw_request *hw)
{
	bool ret;

	mutex_lock(&rm->rm_lock);
	ret = _sde_rm_request_hw_blk_locked(rm, hw);
	mutex_unlock(&rm->rm_lock);

	return ret;
}

static void _sde_rm_hw_destroy(enum sde_hw_blk_type type, void *hw)
{
	switch (type) {
	case SDE_HW_BLK_LM:
		sde_hw_lm_destroy(hw);
		break;
	case SDE_HW_BLK_DSPP:
		sde_hw_dspp_destroy(hw);
		break;
	case SDE_HW_BLK_DS:
		sde_hw_ds_destroy(hw);
		break;
	case SDE_HW_BLK_CTL:
		sde_hw_ctl_destroy(hw);
		break;
	case SDE_HW_BLK_CDM:
		sde_hw_cdm_destroy(hw);
		break;
	case SDE_HW_BLK_PINGPONG:
		sde_hw_pingpong_destroy(hw);
		break;
	case SDE_HW_BLK_INTF:
		sde_hw_intf_destroy(hw);
		break;
	case SDE_HW_BLK_WB:
		sde_hw_wb_destroy(hw);
		break;
	case SDE_HW_BLK_DSC:
		sde_hw_dsc_destroy(hw);
		break;
	case SDE_HW_BLK_ROT:
		sde_hw_rot_destroy(hw);
		break;
	case SDE_HW_BLK_QDSS:
		sde_hw_qdss_destroy(hw);
		break;
	case SDE_HW_BLK_SSPP:
		/* SSPPs are not managed by the resource manager */
	case SDE_HW_BLK_TOP:
		/* Top is a singleton, not managed in hw_blks list */
	case SDE_HW_BLK_MAX:
	default:
		SDE_ERROR("unsupported block type %d\n", type);
		break;
	}
}

int sde_rm_destroy(struct sde_rm *rm)
{

	struct sde_rm_rsvp *rsvp_cur, *rsvp_nxt;
	struct sde_rm_hw_blk *hw_cur, *hw_nxt;
	enum sde_hw_blk_type type;

	if (!rm) {
		SDE_ERROR("invalid rm\n");
		return -EINVAL;
	}

	list_for_each_entry_safe(rsvp_cur, rsvp_nxt, &rm->rsvps, list) {
		list_del(&rsvp_cur->list);
		kfree(rsvp_cur);
	}


	for (type = 0; type < SDE_HW_BLK_MAX; type++) {
		list_for_each_entry_safe(hw_cur, hw_nxt, &rm->hw_blks[type],
				list) {
			list_del(&hw_cur->list);
			_sde_rm_hw_destroy(hw_cur->type, hw_cur->hw);
			kfree(hw_cur);
		}
	}

	sde_hw_mdp_destroy(rm->hw_mdp);
	rm->hw_mdp = NULL;

	mutex_destroy(&rm->rm_lock);

	return 0;
}

static int _sde_rm_hw_blk_create(
		struct sde_rm *rm,
		struct sde_mdss_cfg *cat,
		void __iomem *mmio,
		enum sde_hw_blk_type type,
		uint32_t id,
		void *hw_catalog_info)
{
	struct sde_rm_hw_blk *blk;
	struct sde_hw_mdp *hw_mdp;
	void *hw;

	hw_mdp = rm->hw_mdp;

	switch (type) {
	case SDE_HW_BLK_LM:
		hw = sde_hw_lm_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_DSPP:
		hw = sde_hw_dspp_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_DS:
		hw = sde_hw_ds_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_CTL:
		hw = sde_hw_ctl_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_CDM:
		hw = sde_hw_cdm_init(id, mmio, cat, hw_mdp);
		break;
	case SDE_HW_BLK_PINGPONG:
		hw = sde_hw_pingpong_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_INTF:
		hw = sde_hw_intf_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_WB:
		hw = sde_hw_wb_init(id, mmio, cat, hw_mdp);
		break;
	case SDE_HW_BLK_DSC:
		hw = sde_hw_dsc_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_ROT:
		hw = sde_hw_rot_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_QDSS:
		hw = sde_hw_qdss_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_SSPP:
		/* SSPPs are not managed by the resource manager */
	case SDE_HW_BLK_TOP:
		/* Top is a singleton, not managed in hw_blks list */
	case SDE_HW_BLK_MAX:
	default:
		SDE_ERROR("unsupported block type %d\n", type);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(hw)) {
		SDE_ERROR("failed hw object creation: type %d, err %ld\n",
				type, PTR_ERR(hw));
		return -EFAULT;
	}

	blk = kzalloc(sizeof(*blk), GFP_KERNEL);
	if (!blk) {
		_sde_rm_hw_destroy(type, hw);
		return -ENOMEM;
	}

	blk->type = type;
	blk->id = id;
	blk->hw = hw;
	list_add_tail(&blk->list, &rm->hw_blks[type]);

	return 0;
}

int sde_rm_init(struct sde_rm *rm,
		struct sde_mdss_cfg *cat,
		void __iomem *mmio,
		struct drm_device *dev)
{
	int i, rc = 0;
	enum sde_hw_blk_type type;

	if (!rm || !cat || !mmio || !dev) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}

	/* Clear, setup lists */
	memset(rm, 0, sizeof(*rm));

	mutex_init(&rm->rm_lock);

	INIT_LIST_HEAD(&rm->rsvps);
	for (type = 0; type < SDE_HW_BLK_MAX; type++)
		INIT_LIST_HEAD(&rm->hw_blks[type]);

	rm->dev = dev;

	if (IS_SDE_CTL_REV_100(cat->ctl_rev))
		rm->topology_tbl = g_ctl_ver_1_top_table;
	else
		rm->topology_tbl = g_top_table;

	/* Some of the sub-blocks require an mdptop to be created */
	rm->hw_mdp = sde_hw_mdptop_init(MDP_TOP, mmio, cat);
	if (IS_ERR_OR_NULL(rm->hw_mdp)) {
		rc = PTR_ERR(rm->hw_mdp);
		rm->hw_mdp = NULL;
		SDE_ERROR("failed: mdp hw not available\n");
		goto fail;
	}

	/* Interrogate HW catalog and create tracking items for hw blocks */
	for (i = 0; i < cat->mixer_count; i++) {
		struct sde_lm_cfg *lm = &cat->mixer[i];

		if (lm->pingpong == PINGPONG_MAX) {
			SDE_ERROR("mixer %d without pingpong\n", lm->id);
			goto fail;
		}

		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_LM,
				cat->mixer[i].id, &cat->mixer[i]);
		if (rc) {
			SDE_ERROR("failed: lm hw not available\n");
			goto fail;
		}

		if (!rm->lm_max_width) {
			rm->lm_max_width = lm->sblk->maxwidth;
		} else if (rm->lm_max_width != lm->sblk->maxwidth) {
			/*
			 * Don't expect to have hw where lm max widths differ.
			 * If found, take the min.
			 */
			SDE_ERROR("unsupported: lm maxwidth differs\n");
			if (rm->lm_max_width > lm->sblk->maxwidth)
				rm->lm_max_width = lm->sblk->maxwidth;
		}
	}

	for (i = 0; i < cat->dspp_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_DSPP,
				cat->dspp[i].id, &cat->dspp[i]);
		if (rc) {
			SDE_ERROR("failed: dspp hw not available\n");
			goto fail;
		}
	}

	if (cat->mdp[0].has_dest_scaler) {
		for (i = 0; i < cat->ds_count; i++) {
			rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_DS,
					cat->ds[i].id, &cat->ds[i]);
			if (rc) {
				SDE_ERROR("failed: ds hw not available\n");
				goto fail;
			}
		}
	}

	for (i = 0; i < cat->pingpong_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_PINGPONG,
				cat->pingpong[i].id, &cat->pingpong[i]);
		if (rc) {
			SDE_ERROR("failed: pp hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->dsc_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_DSC,
			cat->dsc[i].id, &cat->dsc[i]);
		if (rc) {
			SDE_ERROR("failed: dsc hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->intf_count; i++) {
		if (cat->intf[i].type == INTF_NONE) {
			SDE_DEBUG("skip intf %d with type none\n", i);
			continue;
		}

		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_INTF,
				cat->intf[i].id, &cat->intf[i]);
		if (rc) {
			SDE_ERROR("failed: intf hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->wb_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_WB,
				cat->wb[i].id, &cat->wb[i]);
		if (rc) {
			SDE_ERROR("failed: wb hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->rot_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_ROT,
				cat->rot[i].id, &cat->rot[i]);
		if (rc) {
			SDE_ERROR("failed: rot hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->ctl_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_CTL,
				cat->ctl[i].id, &cat->ctl[i]);
		if (rc) {
			SDE_ERROR("failed: ctl hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->cdm_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_CDM,
				cat->cdm[i].id, &cat->cdm[i]);
		if (rc) {
			SDE_ERROR("failed: cdm hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->qdss_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_QDSS,
				cat->qdss[i].id, &cat->qdss[i]);
		if (rc) {
			SDE_ERROR("failed: qdss hw not available\n");
			goto fail;
		}
	}

	return 0;

fail:
	sde_rm_destroy(rm);

	return rc;
}

/**
 * _sde_rm_check_lm_and_get_connected_blks - check if proposed layer mixer meets
 *	proposed use case requirements, incl. hardwired dependent blocks like
 *	pingpong, and dspp.
 * @rm: sde resource manager handle
 * @rsvp: reservation currently being created
 * @reqs: proposed use case requirements
 * @lm: proposed layer mixer, function checks if lm, and all other hardwired
 *      blocks connected to the lm (pp, dspp) are available and appropriate
 * @dspp: output parameter, dspp block attached to the layer mixer.
 *        NULL if dspp was not available, or not matching requirements.
 * @pp: output parameter, pingpong block attached to the layer mixer.
 *      NULL if dspp was not available, or not matching requirements.
 * @primary_lm: if non-null, this function check if lm is compatible primary_lm
 *              as well as satisfying all other requirements
 * @Return: true if lm matches all requirements, false otherwise
 */
static bool _sde_rm_check_lm_and_get_connected_blks(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		struct sde_rm_requirements *reqs,
		struct sde_rm_hw_blk *lm,
		struct sde_rm_hw_blk **dspp,
		struct sde_rm_hw_blk **ds,
		struct sde_rm_hw_blk **pp,
		struct sde_rm_hw_blk *primary_lm)
{
	const struct sde_lm_cfg *lm_cfg = to_sde_hw_mixer(lm->hw)->cap;
	const struct sde_pingpong_cfg *pp_cfg;
	struct sde_rm_hw_iter iter;
	bool is_valid_dspp, is_valid_ds, ret;
	u32 display_pref, cwb_pref;

	*dspp = NULL;
	*ds = NULL;
	*pp = NULL;
	display_pref = lm_cfg->features & BIT(SDE_DISP_PRIMARY_PREF);
	cwb_pref = lm_cfg->features & BIT(SDE_DISP_CWB_PREF);

	SDE_DEBUG("check lm %d: dspp %d ds %d pp %d disp_pref: %d cwb_pref%d\n",
		lm_cfg->id, lm_cfg->dspp, lm_cfg->ds,
		lm_cfg->pingpong, display_pref, cwb_pref);

	/* Check if this layer mixer is a peer of the proposed primary LM */
	if (primary_lm) {
		const struct sde_lm_cfg *prim_lm_cfg =
				to_sde_hw_mixer(primary_lm->hw)->cap;

		if (!test_bit(lm_cfg->id, &prim_lm_cfg->lm_pair_mask)) {
			SDE_DEBUG("lm %d not peer of lm %d\n", lm_cfg->id,
					prim_lm_cfg->id);
			return false;
		}
	}

	/* bypass rest of the checks if LM for primary display is found */
	if (!display_pref) {
		is_valid_dspp = (lm_cfg->dspp != DSPP_MAX) ? true : false;
		is_valid_ds = (lm_cfg->ds != DS_MAX) ? true : false;

		/**
		 * RM_RQ_X: specification of which LMs to choose
		 * is_valid_X: indicates whether LM is tied with block X
		 * ret: true if given LM matches the user requirement,
		 *      false otherwise
		 */
		if (RM_RQ_DSPP(reqs) && RM_RQ_DS(reqs))
			ret = (is_valid_dspp && is_valid_ds);
		else if (RM_RQ_DSPP(reqs))
			ret = is_valid_dspp;
		else if (RM_RQ_DS(reqs))
			ret = is_valid_ds;
		else
			ret = !(is_valid_dspp || is_valid_ds);

		if (!ret) {
			SDE_DEBUG(
				"fail:lm(%d)req_dspp(%d)dspp(%d)req_ds(%d)ds(%d)\n",
				lm_cfg->id, (bool)(RM_RQ_DSPP(reqs)),
				lm_cfg->dspp, (bool)(RM_RQ_DS(reqs)),
				lm_cfg->ds);
			return ret;
		}

		/**
		 * If CWB is enabled and LM is not CWB supported
		 * then return false.
		 */
		if (RM_RQ_CWB(reqs) && !cwb_pref) {
			SDE_DEBUG("fail: cwb supported lm not allocated\n");
			return false;
		}

	} else if (!(reqs->hw_res.is_primary && display_pref)) {
		SDE_DEBUG(
			"display preference is not met. is_primary: %d display_pref: %d\n",
			(int)reqs->hw_res.is_primary, (int)display_pref);
		return false;
	}

	/* Already reserved? */
	if (RESERVED_BY_OTHER(lm, rsvp)) {
		SDE_DEBUG("lm %d already reserved\n", lm_cfg->id);
		return false;
	}

	if (lm_cfg->dspp != DSPP_MAX) {
		sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_DSPP);
		while (_sde_rm_get_hw_locked(rm, &iter)) {
			if (iter.blk->id == lm_cfg->dspp) {
				*dspp = iter.blk;
				break;
			}
		}

		if (!*dspp) {
			SDE_DEBUG("lm %d failed to retrieve dspp %d\n", lm->id,
					lm_cfg->dspp);
			return false;
		}

		if (RESERVED_BY_OTHER(*dspp, rsvp)) {
			SDE_DEBUG("lm %d dspp %d already reserved\n",
					lm->id, (*dspp)->id);
			return false;
		}
	}

	if (lm_cfg->ds != DS_MAX) {
		sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_DS);
		while (_sde_rm_get_hw_locked(rm, &iter)) {
			if (iter.blk->id == lm_cfg->ds) {
				*ds = iter.blk;
				break;
			}
		}

		if (!*ds) {
			SDE_DEBUG("lm %d failed to retrieve ds %d\n", lm->id,
					lm_cfg->ds);
			return false;
		}

		if (RESERVED_BY_OTHER(*ds, rsvp)) {
			SDE_DEBUG("lm %d ds %d already reserved\n",
					lm->id, (*ds)->id);
			return false;
		}
	}

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_PINGPONG);
	while (_sde_rm_get_hw_locked(rm, &iter)) {
		if (iter.blk->id == lm_cfg->pingpong) {
			*pp = iter.blk;
			break;
		}
	}

	if (!*pp) {
		SDE_ERROR("failed to get pp on lm %d\n", lm_cfg->pingpong);
		return false;
	}

	if (RESERVED_BY_OTHER(*pp, rsvp)) {
		SDE_DEBUG("lm %d pp %d already reserved\n", lm->id,
				(*pp)->id);
		*dspp = NULL;
		*ds = NULL;
		return false;
	}

	pp_cfg = to_sde_hw_pingpong((*pp)->hw)->caps;
	if ((reqs->topology->top_name == SDE_RM_TOPOLOGY_PPSPLIT) &&
			!(test_bit(SDE_PINGPONG_SPLIT, &pp_cfg->features))) {
		SDE_DEBUG("pp %d doesn't support ppsplit\n", pp_cfg->id);
		*dspp = NULL;
		*ds = NULL;
		return false;
	}

	return true;
}

static int _sde_rm_reserve_lms(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		struct sde_rm_requirements *reqs,
		u8 *_lm_ids)

{
	struct sde_rm_hw_blk *lm[MAX_BLOCKS];
	struct sde_rm_hw_blk *dspp[MAX_BLOCKS];
	struct sde_rm_hw_blk *ds[MAX_BLOCKS];
	struct sde_rm_hw_blk *pp[MAX_BLOCKS];
	struct sde_rm_hw_iter iter_i, iter_j;
	int lm_count = 0;
	int i, rc = 0;

	if (!reqs->topology->num_lm) {
		SDE_DEBUG("invalid number of lm: %d\n", reqs->topology->num_lm);
		return 0;
	}

	/* Find a primary mixer */
	sde_rm_init_hw_iter(&iter_i, 0, SDE_HW_BLK_LM);
	while (lm_count != reqs->topology->num_lm &&
			_sde_rm_get_hw_locked(rm, &iter_i)) {
		memset(&lm, 0, sizeof(lm));
		memset(&dspp, 0, sizeof(dspp));
		memset(&ds, 0, sizeof(ds));
		memset(&pp, 0, sizeof(pp));

		lm_count = 0;
		lm[lm_count] = iter_i.blk;

		SDE_DEBUG("blk id = %d, _lm_ids[%d] = %d\n",
			iter_i.blk->id,
			lm_count,
			_lm_ids ? _lm_ids[lm_count] : -1);

		if (_lm_ids && (lm[lm_count])->id != _lm_ids[lm_count])
			continue;

		if (!_sde_rm_check_lm_and_get_connected_blks(
				rm, rsvp, reqs, lm[lm_count],
				&dspp[lm_count], &ds[lm_count],
				&pp[lm_count], NULL))
			continue;

		++lm_count;

		/* Valid primary mixer found, find matching peers */
		sde_rm_init_hw_iter(&iter_j, 0, SDE_HW_BLK_LM);

		while (lm_count != reqs->topology->num_lm &&
				_sde_rm_get_hw_locked(rm, &iter_j)) {
			if (iter_i.blk == iter_j.blk)
				continue;

			if (!_sde_rm_check_lm_and_get_connected_blks(
					rm, rsvp, reqs, iter_j.blk,
					&dspp[lm_count], &ds[lm_count],
					&pp[lm_count], iter_i.blk))
				continue;

			lm[lm_count] = iter_j.blk;
			SDE_DEBUG("blk id = %d, _lm_ids[%d] = %d\n",
				iter_i.blk->id,
				lm_count,
				_lm_ids ? _lm_ids[lm_count] : -1);

			if (_lm_ids && (lm[lm_count])->id != _lm_ids[lm_count])
				continue;

			++lm_count;
		}
	}

	if (lm_count != reqs->topology->num_lm) {
		SDE_DEBUG("unable to find appropriate mixers\n");
		return -ENAVAIL;
	}

	for (i = 0; i < ARRAY_SIZE(lm); i++) {
		if (!lm[i])
			break;

		lm[i]->rsvp_nxt = rsvp;
		pp[i]->rsvp_nxt = rsvp;
		if (dspp[i])
			dspp[i]->rsvp_nxt = rsvp;

		if (ds[i])
			ds[i]->rsvp_nxt = rsvp;

		SDE_EVT32(lm[i]->type, rsvp->enc_id, lm[i]->id, pp[i]->id,
				dspp[i] ? dspp[i]->id : 0,
				ds[i] ? ds[i]->id : 0);
	}

	if (reqs->topology->top_name == SDE_RM_TOPOLOGY_PPSPLIT) {
		/* reserve a free PINGPONG_SLAVE block */
		rc = -ENAVAIL;
		sde_rm_init_hw_iter(&iter_i, 0, SDE_HW_BLK_PINGPONG);
		while (_sde_rm_get_hw_locked(rm, &iter_i)) {
			const struct sde_hw_pingpong *pp =
					to_sde_hw_pingpong(iter_i.blk->hw);
			const struct sde_pingpong_cfg *pp_cfg = pp->caps;

			if (!(test_bit(SDE_PINGPONG_SLAVE, &pp_cfg->features)))
				continue;
			if (RESERVED_BY_OTHER(iter_i.blk, rsvp))
				continue;

			iter_i.blk->rsvp_nxt = rsvp;
			rc = 0;
			break;
		}
	}

	return rc;
}

static int _sde_rm_reserve_ctls(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		struct sde_rm_requirements *reqs,
		const struct sde_rm_topology_def *top,
		u8 *_ctl_ids)
{
	struct sde_rm_hw_blk *ctls[MAX_BLOCKS];
	struct sde_rm_hw_iter iter;
	int i = 0;

	if (!top->num_ctl) {
		SDE_DEBUG("invalid number of ctl: %d\n", top->num_ctl);
		return 0;
	}

	memset(&ctls, 0, sizeof(ctls));

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_CTL);
	while (_sde_rm_get_hw_locked(rm, &iter)) {
		const struct sde_hw_ctl *ctl = to_sde_hw_ctl(iter.blk->hw);
		unsigned long features = ctl->caps->features;
		bool has_split_display, has_ppsplit, primary_pref;

		if (RESERVED_BY_OTHER(iter.blk, rsvp))
			continue;

		has_split_display = BIT(SDE_CTL_SPLIT_DISPLAY) & features;
		has_ppsplit = BIT(SDE_CTL_PINGPONG_SPLIT) & features;
		primary_pref = BIT(SDE_CTL_PRIMARY_PREF) & features;

		SDE_DEBUG("ctl %d caps 0x%lX\n", iter.blk->id, features);

		/*
		 * bypass rest feature checks on finding CTL preferred
		 * for primary displays.
		 */
		if (!primary_pref && !_ctl_ids) {
			if (top->needs_split_display != has_split_display)
				continue;

			if (top->top_name == SDE_RM_TOPOLOGY_PPSPLIT &&
					!has_ppsplit)
				continue;
		} else if (!(reqs->hw_res.is_primary && primary_pref) &&
				!_ctl_ids) {
			SDE_DEBUG(
				"display pref not met. is_primary: %d primary_pref: %d\n",
				reqs->hw_res.is_primary, primary_pref);
			continue;
		}

		ctls[i] = iter.blk;

		SDE_DEBUG("blk id = %d, _ctl_ids[%d] = %d\n",
			iter.blk->id, i,
			_ctl_ids ? _ctl_ids[i] : -1);

		if (_ctl_ids && (ctls[i]->id != _ctl_ids[i]))
			continue;

		SDE_DEBUG("ctl %d match\n", iter.blk->id);

		if (++i == top->num_ctl)
			break;
	}

	if (i != top->num_ctl)
		return -ENAVAIL;

	for (i = 0; i < ARRAY_SIZE(ctls) && i < top->num_ctl; i++) {
		ctls[i]->rsvp_nxt = rsvp;
		SDE_EVT32(ctls[i]->type, rsvp->enc_id, ctls[i]->id);
	}

	return 0;
}

static int _sde_rm_reserve_dsc(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		const struct sde_rm_topology_def *top,
		u8 *_dsc_ids)
{
	struct sde_rm_hw_iter iter;
	int alloc_count = 0;
	int num_dsc_enc = top->num_lm;

	if (!top->num_comp_enc)
		return 0;

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_DSC);

	while (_sde_rm_get_hw_locked(rm, &iter)) {
		if (RESERVED_BY_OTHER(iter.blk, rsvp))
			continue;

		SDE_DEBUG("blk id = %d, _dsc_ids[%d] = %d\n",
			iter.blk->id,
			alloc_count,
			_dsc_ids ? _dsc_ids[alloc_count] : -1);

		if (_dsc_ids && (iter.blk->id != _dsc_ids[alloc_count]))
			continue;

		iter.blk->rsvp_nxt = rsvp;
		SDE_EVT32(iter.blk->type, rsvp->enc_id, iter.blk->id);

		if (++alloc_count == num_dsc_enc)
			return 0;
	}

	SDE_ERROR("couldn't reserve %d dsc blocks for enc id %d\n",
		num_dsc_enc, rsvp->enc_id);

	return -ENAVAIL;
}

static int _sde_rm_reserve_qdss(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		const struct sde_rm_topology_def *top,
		u8 *_qdss_ids)
{
	struct sde_rm_hw_iter iter;
	struct msm_drm_private *priv = rm->dev->dev_private;
	struct sde_kms *sde_kms;

	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}
	sde_kms = to_sde_kms(priv->kms);

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_QDSS);

	while (_sde_rm_get_hw_locked(rm, &iter)) {
		if (RESERVED_BY_OTHER(iter.blk, rsvp))
			continue;

		SDE_DEBUG("blk id = %d\n", iter.blk->id);

		iter.blk->rsvp_nxt = rsvp;
		SDE_EVT32(iter.blk->type, rsvp->enc_id, iter.blk->id);
		return 0;
	}

	if (!iter.hw && sde_kms->catalog->qdss_count) {
		SDE_DEBUG("couldn't reserve qdss for type %d id %d\n",
						SDE_HW_BLK_QDSS, iter.blk->id);
		return -ENAVAIL;
	}

	return 0;
}

static int _sde_rm_reserve_cdm(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		uint32_t id,
		enum sde_hw_blk_type type)
{
	struct sde_rm_hw_iter iter;

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_CDM);
	while (_sde_rm_get_hw_locked(rm, &iter)) {
		const struct sde_hw_cdm *cdm = to_sde_hw_cdm(iter.blk->hw);
		const struct sde_cdm_cfg *caps = cdm->caps;
		bool match = false;

		if (RESERVED_BY_OTHER(iter.blk, rsvp))
			continue;

		if (type == SDE_HW_BLK_INTF && id != INTF_MAX)
			match = test_bit(id, &caps->intf_connect);
		else if (type == SDE_HW_BLK_WB && id != WB_MAX)
			match = test_bit(id, &caps->wb_connect);

		SDE_DEBUG("type %d id %d, cdm intfs %lu wbs %lu match %d\n",
				type, id, caps->intf_connect, caps->wb_connect,
				match);

		if (!match)
			continue;

		iter.blk->rsvp_nxt = rsvp;
		SDE_EVT32(iter.blk->type, rsvp->enc_id, iter.blk->id);
		break;
	}

	if (!iter.hw) {
		SDE_ERROR("couldn't reserve cdm for type %d id %d\n", type, id);
		return -ENAVAIL;
	}

	return 0;
}

static int _sde_rm_reserve_intf_or_wb(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		uint32_t id,
		enum sde_hw_blk_type type,
		bool needs_cdm)
{
	struct sde_rm_hw_iter iter;
	int ret = 0;

	/* Find the block entry in the rm, and note the reservation */
	sde_rm_init_hw_iter(&iter, 0, type);
	while (_sde_rm_get_hw_locked(rm, &iter)) {
		if (iter.blk->id != id)
			continue;

		if (RESERVED_BY_OTHER(iter.blk, rsvp)) {
			SDE_ERROR("type %d id %d already reserved\n", type, id);
			return -ENAVAIL;
		}

		iter.blk->rsvp_nxt = rsvp;
		SDE_EVT32(iter.blk->type, rsvp->enc_id, iter.blk->id);
		break;
	}

	/* Shouldn't happen since wbs / intfs are fixed at probe */
	if (!iter.hw) {
		SDE_ERROR("couldn't find type %d id %d\n", type, id);
		return -EINVAL;
	}

	/* Expected only one intf or wb will request cdm */
	if (needs_cdm)
		ret = _sde_rm_reserve_cdm(rm, rsvp, id, type);

	return ret;
}

static int _sde_rm_reserve_intf_related_hw(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		struct sde_encoder_hw_resources *hw_res)
{
	int i, ret = 0;
	u32 id;

	for (i = 0; i < ARRAY_SIZE(hw_res->intfs); i++) {
		if (hw_res->intfs[i] == INTF_MODE_NONE)
			continue;
		id = i + INTF_0;
		ret = _sde_rm_reserve_intf_or_wb(rm, rsvp, id,
				SDE_HW_BLK_INTF, hw_res->needs_cdm);
		if (ret)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(hw_res->wbs); i++) {
		if (hw_res->wbs[i] == INTF_MODE_NONE)
			continue;
		id = i + WB_0;
		ret = _sde_rm_reserve_intf_or_wb(rm, rsvp, id,
				SDE_HW_BLK_WB, hw_res->needs_cdm);
		if (ret)
			return ret;
	}

	return ret;
}

static int _sde_rm_make_next_rsvp(
		struct sde_rm *rm,
		struct drm_encoder *enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state,
		struct sde_rm_rsvp *rsvp,
		struct sde_rm_requirements *reqs)
{
	int ret;
	struct sde_rm_topology_def topology;

	/* Create reservation info, tag reserved blocks with it as we go */
	rsvp->seq = ++rm->rsvp_next_seq;
	rsvp->enc_id = enc->base.id;
	rsvp->topology = reqs->topology->top_name;
	list_add_tail(&rsvp->list, &rm->rsvps);

	/*
	 * Assign LMs and blocks whose usage is tied to them: DSPP & Pingpong.
	 * Do assignment preferring to give away low-resource mixers first:
	 * - Check mixers without DSPPs
	 * - Only then allow to grab from mixers with DSPP capability
	 */
	ret = _sde_rm_reserve_lms(rm, rsvp, reqs, NULL);
	if (ret && !RM_RQ_DSPP(reqs)) {
		reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_DSPP);
		ret = _sde_rm_reserve_lms(rm, rsvp, reqs, NULL);
	}

	if (ret) {
		SDE_ERROR("unable to find appropriate mixers\n");
		return ret;
	}

	/*
	 * Do assignment preferring to give away low-resource CTLs first:
	 * - Check mixers without Split Display
	 * - Only then allow to grab from CTLs with split display capability
	 */
	_sde_rm_reserve_ctls(rm, rsvp, reqs, reqs->topology, NULL);
	if (ret && !reqs->topology->needs_split_display &&
			reqs->topology->num_ctl > SINGLE_CTL) {
		memcpy(&topology, reqs->topology, sizeof(topology));
		topology.needs_split_display = true;
		_sde_rm_reserve_ctls(rm, rsvp, reqs, &topology, NULL);
	}
	if (ret) {
		SDE_ERROR("unable to find appropriate CTL\n");
		return ret;
	}

	/* Assign INTFs, WBs, and blks whose usage is tied to them: CTL & CDM */
	ret = _sde_rm_reserve_intf_related_hw(rm, rsvp, &reqs->hw_res);
	if (ret)
		return ret;

	ret = _sde_rm_reserve_dsc(rm, rsvp, reqs->topology, NULL);
	if (ret)
		return ret;

	ret = _sde_rm_reserve_qdss(rm, rsvp, reqs->topology, NULL);
	if (ret)
		return ret;

	return ret;
}

/**
 * _sde_rm_get_hw_blk_for_cont_splash - retrieve the LM blocks on given CTL
 * and populate the connected HW blk ids in sde_splash_display
 * @rm:	Pointer to resource manager structure
 * @ctl: Pointer to CTL hardware block
 * @splash_display: Pointer to struct sde_splash_display
 * return: number of active LM blocks for this CTL block
 */
static int _sde_rm_get_hw_blk_for_cont_splash(struct sde_rm *rm,
		struct sde_hw_ctl *ctl,
		struct sde_splash_display *splash_display)
{
	u32 lm_reg;
	struct sde_rm_hw_iter iter_lm, iter_pp;
	struct sde_hw_pingpong *pp;

	if (!rm || !ctl || !splash_display) {
		SDE_ERROR("invalid input parameters\n");
		return 0;
	}

	sde_rm_init_hw_iter(&iter_lm, 0, SDE_HW_BLK_LM);
	sde_rm_init_hw_iter(&iter_pp, 0, SDE_HW_BLK_PINGPONG);
	while (_sde_rm_get_hw_locked(rm, &iter_lm)) {
		_sde_rm_get_hw_locked(rm, &iter_pp);

		if (splash_display->lm_cnt >= MAX_DATA_PATH_PER_DSIPLAY)
			break;

		lm_reg = ctl->ops.read_ctl_layers(ctl, iter_lm.blk->id);
		if (!lm_reg)
			continue;

		splash_display->lm_ids[splash_display->lm_cnt++] =
			iter_lm.blk->id;
		SDE_DEBUG("lm_cnt=%d lm_reg[%d]=0x%x\n", splash_display->lm_cnt,
				iter_lm.blk->id - LM_0, lm_reg);

		if (ctl->ops.get_staged_sspp &&
				ctl->ops.get_staged_sspp(ctl, iter_lm.blk->id,
					&splash_display->pipes[
					splash_display->pipe_cnt], 1)) {
			splash_display->pipe_cnt++;
		} else {
			SDE_ERROR("no pipe detected on LM-%d\n",
					iter_lm.blk->id - LM_0);
			return 0;
		}

		pp = to_sde_hw_pingpong(iter_pp.blk->hw);
		if (pp && pp->ops.get_dsc_status &&
				pp->ops.get_dsc_status(pp)) {
			splash_display->dsc_ids[splash_display->dsc_cnt++] =
				iter_pp.blk->id;
			SDE_DEBUG("lm/pp[%d] path, using dsc[%d]\n",
					iter_lm.blk->id - LM_0,
					iter_pp.blk->id - DSC_0);
		}
	}

	return splash_display->lm_cnt;
}

int sde_rm_cont_splash_res_init(struct msm_drm_private *priv,
				struct sde_rm *rm,
				struct sde_splash_data *splash_data,
				struct sde_mdss_cfg *cat)
{
	struct sde_rm_hw_iter iter_c;
	int index = 0, ctl_top_cnt;
	struct sde_kms *sde_kms = NULL;
	struct sde_hw_mdp *hw_mdp;
	struct sde_splash_display *splash_display;
	u8 intf_sel;

	if (!priv || !rm || !cat || !splash_data) {
		SDE_ERROR("invalid input parameters\n");
		return -EINVAL;
	}

	SDE_DEBUG("mixer_count=%d, ctl_count=%d, dsc_count=%d\n",
			cat->mixer_count,
			cat->ctl_count,
			cat->dsc_count);

	ctl_top_cnt = cat->ctl_count;

	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}
	sde_kms = to_sde_kms(priv->kms);

	hw_mdp = sde_rm_get_mdp(rm);

	sde_rm_init_hw_iter(&iter_c, 0, SDE_HW_BLK_CTL);
	while (_sde_rm_get_hw_locked(rm, &iter_c)) {
		struct sde_hw_ctl *ctl = to_sde_hw_ctl(iter_c.blk->hw);

		if (!ctl->ops.get_ctl_intf) {
			SDE_ERROR("get_ctl_intf not initialized\n");
			return -EINVAL;
		}

		intf_sel = ctl->ops.get_ctl_intf(ctl);
		if (intf_sel) {
			splash_display =  &splash_data->splash_display[index];
			SDE_DEBUG("finding resources for display=%d ctl=%d\n",
					index, iter_c.blk->id - CTL_0);

			_sde_rm_get_hw_blk_for_cont_splash(rm,
					ctl, splash_display);
			splash_display->cont_splash_enabled = true;
			splash_display->ctl_ids[splash_display->ctl_cnt++] =
				iter_c.blk->id;

			if (hw_mdp && hw_mdp->ops.get_split_flush_status) {
				splash_display->single_flush_en =
					hw_mdp->ops.get_split_flush_status(
							hw_mdp);
			}

			if (!splash_display->single_flush_en ||
					(iter_c.blk->id != CTL_0))
				index++;

			if (index >= ARRAY_SIZE(splash_data->splash_display))
				break;
		}
	}

	if (index != splash_data->num_splash_displays) {
		SDE_DEBUG("mismatch active displays vs actually enabled :%d/%d",
				splash_data->num_splash_displays, index);
		return -EINVAL;
	}

	return 0;
}

static int _sde_rm_make_next_rsvp_for_cont_splash(
		struct sde_rm *rm,
		struct drm_encoder *enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state,
		struct sde_rm_rsvp *rsvp,
		struct sde_rm_requirements *reqs)
{
	int ret;
	struct sde_rm_topology_def topology;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct sde_splash_display *splash_display = NULL;
	int i;

	if (!enc->dev || !enc->dev->dev_private) {
		SDE_ERROR("drm device invalid\n");
		return -EINVAL;
	}
	priv = enc->dev->dev_private;
	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}
	sde_kms = to_sde_kms(priv->kms);

	for (i = 0; i < ARRAY_SIZE(sde_kms->splash_data.splash_display); i++) {
		if (enc == sde_kms->splash_data.splash_display[i].encoder)
			splash_display =
				&sde_kms->splash_data.splash_display[i];
	}

	if (!splash_display) {
		SDE_ERROR("invalid splash data for enc:%d\n", enc->base.id);
		return -EINVAL;
	}

	for (i = 0; i < splash_display->lm_cnt; i++)
		SDE_DEBUG("splash_data.lm_ids[%d] = %d\n",
			i, splash_display->lm_ids[i]);

	if (splash_display->lm_cnt !=
			reqs->topology->num_lm)
		SDE_DEBUG("Configured splash screen LMs != needed LM cnt\n");

	/* Create reservation info, tag reserved blocks with it as we go */
	rsvp->seq = ++rm->rsvp_next_seq;
	rsvp->enc_id = enc->base.id;
	rsvp->topology = reqs->topology->top_name;
	list_add_tail(&rsvp->list, &rm->rsvps);

	/*
	 * Assign LMs and blocks whose usage is tied to them: DSPP & Pingpong.
	 * Do assignment preferring to give away low-resource mixers first:
	 * - Check mixers without DSPPs
	 * - Only then allow to grab from mixers with DSPP capability
	 */
	ret = _sde_rm_reserve_lms(rm, rsvp, reqs,
				splash_display->lm_ids);
	if (ret && !RM_RQ_DSPP(reqs)) {
		reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_DSPP);
		ret = _sde_rm_reserve_lms(rm, rsvp, reqs,
					splash_display->lm_ids);
	}

	if (ret) {
		SDE_ERROR("unable to find appropriate mixers\n");
		return ret;
	}

	/*
	 * Do assignment preferring to give away low-resource CTLs first:
	 * - Check mixers without Split Display
	 * - Only then allow to grab from CTLs with split display capability
	 */
	for (i = 0; i < splash_display->ctl_cnt; i++)
		SDE_DEBUG("splash_data.ctl_ids[%d] = %d\n",
			i, splash_display->ctl_ids[i]);

	_sde_rm_reserve_ctls(rm, rsvp, reqs, reqs->topology,
			splash_display->ctl_ids);
	if (ret && !reqs->topology->needs_split_display) {
		memcpy(&topology, reqs->topology, sizeof(topology));
		topology.needs_split_display = true;
		_sde_rm_reserve_ctls(rm, rsvp, reqs, &topology,
				splash_display->ctl_ids);
	}
	if (ret) {
		SDE_ERROR("unable to find appropriate CTL\n");
		return ret;
	}

	/* Assign INTFs, WBs, and blks whose usage is tied to them: CTL & CDM */
	ret = _sde_rm_reserve_intf_related_hw(rm, rsvp, &reqs->hw_res);
	if (ret)
		return ret;

	for (i = 0; i < splash_display->dsc_cnt; i++)
		SDE_DEBUG("splash_data.dsc_ids[%d] = %d\n",
			i, splash_display->dsc_ids[i]);

	ret = _sde_rm_reserve_dsc(rm, rsvp, reqs->topology,
				splash_display->dsc_ids);
	if (ret)
		return ret;

	ret = _sde_rm_reserve_qdss(rm, rsvp, reqs->topology, NULL);
	if (ret)
		return ret;

	return ret;
}

static int _sde_rm_populate_requirements(
		struct sde_rm *rm,
		struct drm_encoder *enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state,
		struct sde_rm_requirements *reqs)
{
	const struct drm_display_mode *mode = &crtc_state->mode;
	int i, num_lm;

	memset(reqs, 0, sizeof(*reqs));

	reqs->top_ctrl = sde_connector_get_property(conn_state,
			CONNECTOR_PROP_TOPOLOGY_CONTROL);
	sde_encoder_get_hw_resources(enc, &reqs->hw_res, conn_state);

	for (i = 0; i < SDE_RM_TOPOLOGY_MAX; i++) {
		if (RM_IS_TOPOLOGY_MATCH(rm->topology_tbl[i],
					reqs->hw_res.topology)) {
			reqs->topology = &rm->topology_tbl[i];
			break;
		}
	}

	if (!reqs->topology) {
		SDE_ERROR("invalid topology for the display\n");
		return -EINVAL;
	}

	/*
	 * select dspp HW block for all dsi displays and ds for only
	 * primary dsi display.
	 */
	if (conn_state->connector->connector_type == DRM_MODE_CONNECTOR_DSI) {
		if (!RM_RQ_DSPP(reqs))
			reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_DSPP);

		if (!RM_RQ_DS(reqs) && rm->hw_mdp->caps->has_dest_scaler &&
		    sde_encoder_is_primary_display(enc))
			reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_DS);
	}

	/**
	 * Set the requirement for LM which has CWB support if CWB is
	 * found enabled.
	 */
	if (!RM_RQ_CWB(reqs) && sde_encoder_in_clone_mode(enc)) {
		reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_CWB);

		/*
		 * topology selection based on conn mode is not valid for CWB
		 * as WB conn populates modes based on max_mixer_width check
		 * but primary can be using dual LMs. This topology override for
		 * CWB is to check number of datapath active in primary and
		 * allocate same number of LM/PP blocks reserved for CWB
		 */
		reqs->topology =
			&rm->topology_tbl[SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE];

		num_lm = sde_crtc_get_num_datapath(crtc_state->crtc,
				conn_state->connector);

		if (num_lm == 1)
			reqs->topology =
				&rm->topology_tbl[SDE_RM_TOPOLOGY_SINGLEPIPE];
		else if (num_lm == 0)
			SDE_ERROR("Primary layer mixer is not set\n");

		SDE_EVT32(num_lm, reqs->topology->num_lm,
			reqs->topology->top_name, reqs->topology->num_ctl);
	}

	SDE_DEBUG("top_ctrl: 0x%llX num_h_tiles: %d\n", reqs->top_ctrl,
			reqs->hw_res.display_num_of_h_tiles);
	SDE_DEBUG("num_lm: %d num_ctl: %d topology: %d split_display: %d\n",
			reqs->topology->num_lm, reqs->topology->num_ctl,
			reqs->topology->top_name,
			reqs->topology->needs_split_display);
	SDE_EVT32(mode->hdisplay, rm->lm_max_width, reqs->topology->num_lm,
			reqs->top_ctrl, reqs->topology->top_name,
			reqs->topology->num_ctl);

	return 0;
}

static struct sde_rm_rsvp *_sde_rm_get_rsvp(
		struct sde_rm *rm,
		struct drm_encoder *enc)
{
	struct sde_rm_rsvp *i;

	if (!rm || !enc) {
		SDE_ERROR("invalid params\n");
		return NULL;
	}

	if (list_empty(&rm->rsvps))
		return NULL;

	list_for_each_entry(i, &rm->rsvps, list)
		if (i->enc_id == enc->base.id)
			return i;

	return NULL;
}

static struct drm_connector *_sde_rm_get_connector(
		struct drm_encoder *enc)
{
	struct drm_connector *conn = NULL;
	struct list_head *connector_list =
			&enc->dev->mode_config.connector_list;

	list_for_each_entry(conn, connector_list, head)
		if (conn->encoder == enc)
			return conn;

	return NULL;
}

int sde_rm_update_topology(struct drm_connector_state *conn_state,
	struct msm_display_topology *topology)
{
	int i, ret = 0;
	struct msm_display_topology top;
	enum sde_rm_topology_name top_name = SDE_RM_TOPOLOGY_NONE;

	if (!conn_state)
		return -EINVAL;

	if (topology) {
		top = *topology;
		for (i = 0; i < SDE_RM_TOPOLOGY_MAX; i++)
			if (RM_IS_TOPOLOGY_MATCH(g_top_table[i], top)) {
				top_name = g_top_table[i].top_name;
				break;
			}
	}

	ret = msm_property_set_property(
			sde_connector_get_propinfo(conn_state->connector),
			sde_connector_get_property_state(conn_state),
			CONNECTOR_PROP_TOPOLOGY_NAME, top_name);

	return ret;
}

/**
 * _sde_rm_release_rsvp - release resources and release a reservation
 * @rm:	KMS handle
 * @rsvp:	RSVP pointer to release and release resources for
 */
static void _sde_rm_release_rsvp(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		struct drm_connector *conn)
{
	struct sde_rm_rsvp *rsvp_c, *rsvp_n;
	struct sde_rm_hw_blk *blk;
	enum sde_hw_blk_type type;

	if (!rsvp)
		return;

	SDE_DEBUG("rel rsvp %d enc %d\n", rsvp->seq, rsvp->enc_id);

	list_for_each_entry_safe(rsvp_c, rsvp_n, &rm->rsvps, list) {
		if (rsvp == rsvp_c) {
			list_del(&rsvp_c->list);
			break;
		}
	}

	for (type = 0; type < SDE_HW_BLK_MAX; type++) {
		list_for_each_entry(blk, &rm->hw_blks[type], list) {
			if (blk->rsvp == rsvp) {
				blk->rsvp = NULL;
				SDE_DEBUG("rel rsvp %d enc %d %d %d\n",
						rsvp->seq, rsvp->enc_id,
						blk->type, blk->id);
			}
			if (blk->rsvp_nxt == rsvp) {
				blk->rsvp_nxt = NULL;
				SDE_DEBUG("rel rsvp_nxt %d enc %d %d %d\n",
						rsvp->seq, rsvp->enc_id,
						blk->type, blk->id);
			}
		}
	}

	kfree(rsvp);
}

void sde_rm_release(struct sde_rm *rm, struct drm_encoder *enc)
{
	struct sde_rm_rsvp *rsvp;
	struct drm_connector *conn;
	uint64_t top_ctrl;

	if (!rm || !enc) {
		SDE_ERROR("invalid params\n");
		return;
	}

	mutex_lock(&rm->rm_lock);

	rsvp = _sde_rm_get_rsvp(rm, enc);
	if (!rsvp)
		goto end;

	conn = _sde_rm_get_connector(enc);
	if (!conn) {
		SDE_ERROR("failed to get connector for enc %d\n", enc->base.id);
		goto end;
	}

	top_ctrl = sde_connector_get_property(conn->state,
			CONNECTOR_PROP_TOPOLOGY_CONTROL);

	if (top_ctrl & BIT(SDE_RM_TOPCTL_RESERVE_LOCK)) {
		SDE_DEBUG("rsvp[s%de%d] not releasing locked resources\n",
				rsvp->seq, rsvp->enc_id);
	} else {
		SDE_DEBUG("release rsvp[s%de%d]\n", rsvp->seq,
				rsvp->enc_id);
		_sde_rm_release_rsvp(rm, rsvp, conn);
	}

end:
	mutex_unlock(&rm->rm_lock);
}

static int _sde_rm_commit_rsvp(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		struct drm_connector_state *conn_state)
{
	struct sde_rm_hw_blk *blk;
	enum sde_hw_blk_type type;
	int ret = 0;

	/* Swap next rsvp to be the active */
	for (type = 0; type < SDE_HW_BLK_MAX; type++) {
		list_for_each_entry(blk, &rm->hw_blks[type], list) {
			if (blk->rsvp_nxt) {
				blk->rsvp = blk->rsvp_nxt;
				blk->rsvp_nxt = NULL;
			}
		}
	}

	if (!ret) {
		SDE_DEBUG("rsrv enc %d topology %d\n", rsvp->enc_id,
				rsvp->topology);
		SDE_EVT32(rsvp->enc_id, rsvp->topology);
	}

	return ret;
}

static bool sde_rm_is_display_in_cont_splash(struct sde_kms *sde_kms,
		struct drm_encoder *enc)
{
	int i;
	struct sde_splash_display *splash_dpy;

	for (i = 0; i < MAX_DSI_DISPLAYS; i++) {
		splash_dpy = &sde_kms->splash_data.splash_display[i];
		if (splash_dpy->encoder ==  enc)
			return splash_dpy->cont_splash_enabled;
	}

	return false;
}

int sde_rm_reserve(
		struct sde_rm *rm,
		struct drm_encoder *enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state,
		bool test_only)
{
	struct sde_rm_rsvp *rsvp_cur, *rsvp_nxt;
	struct sde_rm_requirements reqs;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	int ret;

	if (!rm || !enc || !crtc_state || !conn_state) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if (!enc->dev || !enc->dev->dev_private) {
		SDE_ERROR("drm device invalid\n");
		return -EINVAL;
	}
	priv = enc->dev->dev_private;
	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}
	sde_kms = to_sde_kms(priv->kms);

	/* Check if this is just a page-flip */
	if (!sde_rm_is_display_in_cont_splash(sde_kms, enc) &&
			!drm_atomic_crtc_needs_modeset(crtc_state))
		return 0;

	SDE_DEBUG("reserving hw for conn %d enc %d crtc %d test_only %d\n",
			conn_state->connector->base.id, enc->base.id,
			crtc_state->crtc->base.id, test_only);
	SDE_EVT32(enc->base.id, conn_state->connector->base.id);

	mutex_lock(&rm->rm_lock);

	_sde_rm_print_rsvps(rm, SDE_RM_STAGE_BEGIN);

	ret = _sde_rm_populate_requirements(rm, enc, crtc_state,
			conn_state, &reqs);
	if (ret) {
		SDE_ERROR("failed to populate hw requirements\n");
		goto end;
	}

	/*
	 * We only support one active reservation per-hw-block. But to implement
	 * transactional semantics for test-only, and for allowing failure while
	 * modifying your existing reservation, over the course of this
	 * function we can have two reservations:
	 * Current: Existing reservation
	 * Next: Proposed reservation. The proposed reservation may fail, or may
	 *       be discarded if in test-only mode.
	 * If reservation is successful, and we're not in test-only, then we
	 * replace the current with the next.
	 */
	rsvp_nxt = kzalloc(sizeof(*rsvp_nxt), GFP_KERNEL);
	if (!rsvp_nxt) {
		ret = -ENOMEM;
		goto end;
	}

	rsvp_cur = _sde_rm_get_rsvp(rm, enc);

	/*
	 * User can request that we clear out any reservation during the
	 * atomic_check phase by using this CLEAR bit
	 */
	if (rsvp_cur && test_only && RM_RQ_CLEAR(&reqs)) {
		SDE_DEBUG("test_only & CLEAR: clear rsvp[s%de%d]\n",
				rsvp_cur->seq, rsvp_cur->enc_id);
		_sde_rm_release_rsvp(rm, rsvp_cur, conn_state->connector);
		rsvp_cur = NULL;
		_sde_rm_print_rsvps(rm, SDE_RM_STAGE_AFTER_CLEAR);
	}

	/* Check the proposed reservation, store it in hw's "next" field */
	if (sde_rm_is_display_in_cont_splash(sde_kms, enc)) {
		SDE_DEBUG("cont_splash enabled on enc-%d\n", enc->base.id);
		ret = _sde_rm_make_next_rsvp_for_cont_splash
			(rm, enc, crtc_state, conn_state, rsvp_nxt, &reqs);
	} else {
		ret = _sde_rm_make_next_rsvp(rm, enc, crtc_state, conn_state,
			rsvp_nxt, &reqs);
	}

	_sde_rm_print_rsvps(rm, SDE_RM_STAGE_AFTER_RSVPNEXT);

	if (ret) {
		SDE_ERROR("failed to reserve hw resources: %d\n", ret);
		_sde_rm_release_rsvp(rm, rsvp_nxt, conn_state->connector);
	} else if (test_only && !RM_RQ_LOCK(&reqs)) {
		/*
		 * Normally, if test_only, test the reservation and then undo
		 * However, if the user requests LOCK, then keep the reservation
		 * made during the atomic_check phase.
		 */
		SDE_DEBUG("test_only: discard test rsvp[s%de%d]\n",
				rsvp_nxt->seq, rsvp_nxt->enc_id);
		_sde_rm_release_rsvp(rm, rsvp_nxt, conn_state->connector);
	} else {
		if (test_only && RM_RQ_LOCK(&reqs))
			SDE_DEBUG("test_only & LOCK: lock rsvp[s%de%d]\n",
					rsvp_nxt->seq, rsvp_nxt->enc_id);

		_sde_rm_release_rsvp(rm, rsvp_cur, conn_state->connector);

		ret = _sde_rm_commit_rsvp(rm, rsvp_nxt, conn_state);
	}

	_sde_rm_print_rsvps(rm, SDE_RM_STAGE_FINAL);

end:
	mutex_unlock(&rm->rm_lock);

	return ret;
}

int sde_rm_ext_blk_create_reserve(struct sde_rm *rm,
		struct sde_hw_blk *hw, struct drm_encoder *enc)
{
	struct sde_rm_hw_blk *blk;
	struct sde_rm_rsvp *rsvp;
	int ret = 0;

	if (!rm || !hw || !enc) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	if (hw->type >= SDE_HW_BLK_MAX) {
		SDE_ERROR("invalid HW type\n");
		return -EINVAL;
	}

	mutex_lock(&rm->rm_lock);

	rsvp = _sde_rm_get_rsvp(rm, enc);
	if (!rsvp) {
		rsvp = kzalloc(sizeof(*rsvp), GFP_KERNEL);
		if (!rsvp) {
			ret = -ENOMEM;
			goto end;
		}

		rsvp->seq = ++rm->rsvp_next_seq;
		rsvp->enc_id = enc->base.id;
		list_add_tail(&rsvp->list, &rm->rsvps);

		SDE_DEBUG("create rsvp %d for enc %d\n",
					rsvp->seq, rsvp->enc_id);
	}

	blk = kzalloc(sizeof(*blk), GFP_KERNEL);
	if (!blk) {
		ret = -ENOMEM;
		goto end;
	}

	blk->type = hw->type;
	blk->id = hw->id;
	blk->hw = hw;
	blk->rsvp = rsvp;
	list_add_tail(&blk->list, &rm->hw_blks[hw->type]);

	SDE_DEBUG("create blk %d %d for rsvp %d enc %d\n", blk->type, blk->id,
					rsvp->seq, rsvp->enc_id);

end:
	mutex_unlock(&rm->rm_lock);
	return ret;
}

int sde_rm_ext_blk_destroy(struct sde_rm *rm,
		struct drm_encoder *enc)
{
	struct sde_rm_hw_blk *blk = NULL, *p;
	struct sde_rm_rsvp *rsvp;
	enum sde_hw_blk_type type;
	int ret = 0;

	if (!rm || !enc) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	mutex_lock(&rm->rm_lock);

	rsvp = _sde_rm_get_rsvp(rm, enc);
	if (!rsvp) {
		ret = -ENOENT;
		SDE_ERROR("failed to find rsvp for enc %d\n", enc->base.id);
		goto end;
	}

	for (type = 0; type < SDE_HW_BLK_MAX; type++) {
		list_for_each_entry_safe(blk, p, &rm->hw_blks[type], list) {
			if (blk->rsvp == rsvp) {
				list_del(&blk->list);
				SDE_DEBUG("del blk %d %d from rsvp %d enc %d\n",
						blk->type, blk->id,
						rsvp->seq, rsvp->enc_id);
				kfree(blk);
			}
		}
	}

	SDE_DEBUG("del rsvp %d\n", rsvp->seq);
	list_del(&rsvp->list);
	kfree(rsvp);
end:
	mutex_unlock(&rm->rm_lock);
	return ret;
}
