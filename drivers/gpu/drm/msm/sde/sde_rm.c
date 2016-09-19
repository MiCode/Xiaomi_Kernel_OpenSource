/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include "sde_hw_pingpong.h"
#include "sde_hw_intf.h"
#include "sde_hw_wb.h"
#include "sde_hw_sspp.h"

#define RESERVED_BY_OTHER(h, r) \
	((h)->rsvp && ((h)->rsvp->enc_id != (r)->enc_id))

#define RM_RQ_LOCK(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_RESERVE_LOCK))
#define RM_RQ_CLEAR(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_RESERVE_CLEAR))
#define RM_RQ_DSPP(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_DSPP))
#define RM_RQ_PPSPLIT(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_PPSPLIT))
#define RM_RQ_FORCE_TILING(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_FORCE_TILING))

/**
 * struct sde_rm_requirements - Reservation requirements parameter bundle
 * @top_name:	DRM<->HW topology use case user is trying to enable
 * @dspp:	Whether the user requires a DSPP
 * @num_lm:	Number of layer mixers needed in the use case
 * @hw_res:	Hardware resources required as reported by the encoders
 */
struct sde_rm_requirements {
	enum sde_rm_topology_name top_name;
	uint64_t top_ctrl;
	int num_lm;
	int num_ctl;
	bool needs_split_display;
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
	const char *type_name;
	uint32_t id;
	void *catalog;
	void *hw;
};

static void _sde_rm_print_rsvps(struct sde_rm *rm, const char *msg)
{
	struct sde_rm_rsvp *rsvp;
	struct sde_rm_hw_blk *blk;
	enum sde_hw_blk_type type;

	SDE_DEBUG("%s\n", msg);

	list_for_each_entry(rsvp, &rm->rsvps, list)
		SDE_DEBUG("%s rsvp[s%ue%u] topology %d\n", msg, rsvp->seq,
				rsvp->enc_id, rsvp->topology);

	for (type = 0; type < SDE_HW_BLK_MAX; type++) {
		list_for_each_entry(blk, &rm->hw_blks[type], list) {
			if (!blk->rsvp && !blk->rsvp_nxt)
				continue;

			SDE_DEBUG("%s rsvp[s%ue%u->s%ue%u] %s %d\n", msg,
				(blk->rsvp) ? blk->rsvp->seq : 0,
				(blk->rsvp) ? blk->rsvp->enc_id : 0,
				(blk->rsvp_nxt) ? blk->rsvp_nxt->seq : 0,
				(blk->rsvp_nxt) ? blk->rsvp_nxt->enc_id : 0,
				blk->type_name, blk->id);
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

bool sde_rm_get_hw(struct sde_rm *rm, struct sde_rm_hw_iter *i)
{
	struct list_head *blk_list;

	if (!rm || !i || i->type >= SDE_HW_BLK_MAX) {
		SDE_ERROR("invalid rm\n");
		return false;
	}

	i->hw = NULL;
	blk_list = &rm->hw_blks[i->type];

	if (i->blk && (&i->blk->list == blk_list)) {
		SDE_ERROR("attempt resume iteration past last\n");
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
			SDE_DEBUG("found type %d %s id %d for enc %d\n",
					i->type, i->blk->type_name, i->blk->id,
					i->enc_id);
			return true;
		}
	}

	SDE_DEBUG("no match, type %d for enc %d\n", i->type, i->enc_id);

	return false;
}

void *sde_rm_get_hw_by_id(struct sde_rm *rm, enum sde_hw_blk_type type, int id)
{
	struct list_head *blk_list;
	struct sde_rm_hw_blk *blk;
	void *hw = NULL;

	if (!rm || type >= SDE_HW_BLK_MAX) {
		SDE_ERROR("invalid rm\n");
		return hw;
	}

	blk_list = &rm->hw_blks[type];

	list_for_each_entry(blk, blk_list, list) {
		if (blk->id == id) {
			hw = blk->hw;
			SDE_DEBUG("found type %d %s id %d\n",
					type, blk->type_name, blk->id);
			return hw;
		}
	}

	SDE_DEBUG("no match, type %d id=%d\n", type, id);

	return hw;
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
	case SDE_HW_BLK_SSPP:
		sde_hw_sspp_destroy(hw);
		break;
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

	return 0;
}

static int _sde_rm_hw_blk_create(
		struct sde_rm *rm,
		struct sde_mdss_cfg *cat,
		void *mmio,
		enum sde_hw_blk_type type,
		uint32_t id,
		void *hw_catalog_info)
{
	struct sde_rm_hw_blk *blk;
	struct sde_hw_mdp *hw_mdp;
	const char *name;
	void *hw;

	hw_mdp = rm->hw_mdp;

	switch (type) {
	case SDE_HW_BLK_LM:
		hw = sde_hw_lm_init(id, mmio, cat);
		name = "lm";
		break;
	case SDE_HW_BLK_DSPP:
		hw = sde_hw_dspp_init(id, mmio, cat);
		name = "dspp";
		break;
	case SDE_HW_BLK_CTL:
		hw = sde_hw_ctl_init(id, mmio, cat);
		name = "ctl";
		break;
	case SDE_HW_BLK_CDM:
		hw = sde_hw_cdm_init(id, mmio, cat, hw_mdp);
		name = "cdm";
		break;
	case SDE_HW_BLK_PINGPONG:
		hw = sde_hw_pingpong_init(id, mmio, cat);
		name = "pp";
		break;
	case SDE_HW_BLK_INTF:
		hw = sde_hw_intf_init(id, mmio, cat);
		name = "intf";
		break;
	case SDE_HW_BLK_WB:
		hw = sde_hw_wb_init(id, mmio, cat, hw_mdp);
		name = "wb";
		break;
	case SDE_HW_BLK_SSPP:
		hw = sde_hw_sspp_init(id, mmio, cat);
		name = "sspp";
		break;
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

	blk->type_name = name;
	blk->type = type;
	blk->id = id;
	blk->catalog = hw_catalog_info;
	blk->hw = hw;
	list_add_tail(&blk->list, &rm->hw_blks[type]);

	return 0;
}

int sde_rm_init(struct sde_rm *rm,
		struct sde_mdss_cfg *cat,
		void *mmio,
		struct drm_device *dev)
{
	int rc, i;
	enum sde_hw_blk_type type;

	if (!rm || !cat || !mmio || !dev) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}

	/* Clear, setup lists */
	memset(rm, 0, sizeof(*rm));
	INIT_LIST_HEAD(&rm->rsvps);
	for (type = 0; type < SDE_HW_BLK_MAX; type++)
		INIT_LIST_HEAD(&rm->hw_blks[type]);

	/* Some of the sub-blocks require an mdptop to be created */
	rm->hw_mdp = sde_hw_mdptop_init(MDP_TOP, mmio, cat);
	if (IS_ERR_OR_NULL(rm->hw_mdp)) {
		rc = PTR_ERR(rm->hw_mdp);
		rm->hw_mdp = NULL;
		goto fail;
	}

	for (i = 0; i < cat->sspp_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_SSPP,
				cat->sspp[i].id, &cat->sspp[i]);
		if (rc)
			goto fail;
	}

	/* Interrogate HW catalog and create tracking items for hw blocks */
	for (i = 0; i < cat->mixer_count; i++) {
		struct sde_lm_cfg *lm = &cat->mixer[i];

		if (lm->pingpong == PINGPONG_MAX) {
			SDE_DEBUG("skip mixer %d without pingpong\n", lm->id);
			continue;
		}

		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_LM,
				cat->mixer[i].id, &cat->mixer[i]);
		if (rc)
			goto fail;

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
		if (rc)
			goto fail;
	}

	for (i = 0; i < cat->pingpong_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_PINGPONG,
				cat->pingpong[i].id, &cat->pingpong[i]);
		if (rc)
			goto fail;
	}

	for (i = 0; i < cat->intf_count; i++) {
		if (cat->intf[i].type == INTF_NONE) {
			SDE_DEBUG("skip intf %d with type none\n", i);
			continue;
		}

		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_INTF,
				cat->intf[i].id, &cat->intf[i]);
		if (rc)
			goto fail;
	}

	for (i = 0; i < cat->wb_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_WB,
				cat->wb[i].id, &cat->wb[i]);
		if (rc)
			goto fail;
	}

	for (i = 0; i < cat->ctl_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_CTL,
				cat->ctl[i].id, &cat->ctl[i]);
		if (rc)
			goto fail;
	}

	for (i = 0; i < cat->cdm_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_CDM,
				cat->cdm[i].id, &cat->cdm[i]);
		if (rc)
			goto fail;
	}

	return 0;

fail:
	SDE_ERROR("failed to init sde kms resources\n");
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
		struct sde_rm_hw_blk **pp,
		struct sde_rm_hw_blk *primary_lm)
{
	struct sde_lm_cfg *lm_cfg = (struct sde_lm_cfg *)lm->catalog;
	struct sde_pingpong_cfg *pp_cfg;
	struct sde_rm_hw_iter iter;

	*dspp = NULL;
	*pp = NULL;

	SDE_DEBUG("check lm %d: dspp %d pp %d\n", lm_cfg->id, lm_cfg->dspp,
			lm_cfg->pingpong);

	/* Check if this layer mixer is a peer of the proposed primary LM */
	if (primary_lm) {
		struct sde_lm_cfg *prim_lm_cfg =
				(struct sde_lm_cfg *)primary_lm->catalog;

		if (!test_bit(lm_cfg->id, &prim_lm_cfg->lm_pair_mask)) {
			SDE_DEBUG("lm %d not peer of lm %d\n", lm_cfg->id,
					prim_lm_cfg->id);
			return false;
		}
	}

	/* Matches user requirements? */
	if ((RM_RQ_DSPP(reqs) && lm_cfg->dspp == DSPP_MAX) ||
			(!RM_RQ_DSPP(reqs) && lm_cfg->dspp != DSPP_MAX)) {
		SDE_DEBUG("dspp req mismatch lm %d reqdspp %d, lm->dspp %d\n",
				lm_cfg->id, (bool)(RM_RQ_DSPP(reqs)),
				lm_cfg->dspp);
		return false;
	}

	/* Already reserved? */
	if (RESERVED_BY_OTHER(lm, rsvp)) {
		SDE_DEBUG("lm %d already reserved\n", lm_cfg->id);
		return false;
	}

	if (lm_cfg->dspp != DSPP_MAX) {
		sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_DSPP);
		while (sde_rm_get_hw(rm, &iter)) {
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

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_PINGPONG);
	while (sde_rm_get_hw(rm, &iter)) {
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
		return false;
	}

	pp_cfg = (struct sde_pingpong_cfg *)((*pp)->catalog);
	if ((reqs->top_name == SDE_RM_TOPOLOGY_PPSPLIT) &&
			!(test_bit(SDE_PINGPONG_SPLIT, &pp_cfg->features))) {
		SDE_DEBUG("pp %d doesn't support ppsplit\n", pp_cfg->id);
		*dspp = NULL;
		return false;
	}

	return true;
}

static int _sde_rm_reserve_lms(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		struct sde_rm_requirements *reqs)

{
	struct sde_rm_hw_blk *lm[MAX_BLOCKS];
	struct sde_rm_hw_blk *dspp[MAX_BLOCKS];
	struct sde_rm_hw_blk *pp[MAX_BLOCKS];
	struct sde_rm_hw_iter iter_i, iter_j;
	int lm_count = 0;
	int i;

	if (!reqs->num_lm) {
		SDE_ERROR("invalid number of lm: %d\n", reqs->num_lm);
		return -EINVAL;
	}

	/* Find a primary mixer */
	sde_rm_init_hw_iter(&iter_i, 0, SDE_HW_BLK_LM);
	while (lm_count != reqs->num_lm && sde_rm_get_hw(rm, &iter_i)) {
		memset(&lm, 0, sizeof(lm));
		memset(&dspp, 0, sizeof(dspp));
		memset(&pp, 0, sizeof(pp));

		lm_count = 0;
		lm[lm_count] = iter_i.blk;

		if (!_sde_rm_check_lm_and_get_connected_blks(rm, rsvp, reqs,
				lm[lm_count], &dspp[lm_count], &pp[lm_count],
				NULL))
			continue;

		++lm_count;

		/* Valid primary mixer found, find matching peers */
		sde_rm_init_hw_iter(&iter_j, 0, SDE_HW_BLK_LM);

		while (lm_count != reqs->num_lm && sde_rm_get_hw(rm, &iter_j)) {
			if (iter_i.blk == iter_j.blk)
				continue;

			if (!_sde_rm_check_lm_and_get_connected_blks(rm, rsvp,
					reqs, iter_j.blk, &dspp[lm_count],
					&pp[lm_count], iter_i.blk))
				continue;

			lm[lm_count] = iter_j.blk;
			++lm_count;
		}
	}

	if (lm_count != reqs->num_lm) {
		SDE_DEBUG("unable to find appropriate mixers\n");
		return -ENAVAIL;
	}

	for (i = 0; i < ARRAY_SIZE(lm); i++) {
		if (!lm[i])
			break;

		lm[i]->rsvp_nxt = rsvp;
		pp[i]->rsvp_nxt = rsvp;
		MSM_EVTMSG(rm->dev, lm[i]->type_name, rsvp->enc_id, lm[i]->id);
		MSM_EVTMSG(rm->dev, pp[i]->type_name, rsvp->enc_id, pp[i]->id);
		if (dspp[i]) {
			dspp[i]->rsvp_nxt = rsvp;
			MSM_EVTMSG(rm->dev, dspp[i]->type_name, rsvp->enc_id,
					dspp[i]->id);
		}
	}

	return 0;
}

static int _sde_rm_reserve_ctls(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		struct sde_rm_requirements *reqs)
{
	struct sde_rm_hw_blk *ctls[MAX_BLOCKS];
	struct sde_rm_hw_iter iter;
	struct sde_hw_ctl *ctl;
	int i = 0;

	memset(&ctls, 0, sizeof(ctls));

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_CTL);
	while (sde_rm_get_hw(rm, &iter)) {
		unsigned long caps;
		bool has_split_display, has_ppsplit;

		if (RESERVED_BY_OTHER(iter.blk, rsvp))
			continue;

		caps = ((struct sde_ctl_cfg *)iter.blk->catalog)->features;
		has_split_display = BIT(SDE_CTL_SPLIT_DISPLAY) & caps;
		has_ppsplit = BIT(SDE_CTL_PINGPONG_SPLIT) & caps;

		SDE_DEBUG("ctl %d caps 0x%lX\n", iter.blk->id, caps);

		if (reqs->needs_split_display != has_split_display)
			continue;

		if (reqs->top_name == SDE_RM_TOPOLOGY_PPSPLIT && !has_ppsplit)
			continue;

		ctls[i] = iter.blk;
		ctl = (struct sde_hw_ctl *)iter.blk->hw;
		if (ctl) {
			if (reqs->top_name == SDE_RM_TOPOLOGY_DUALPIPEMERGE)
				ctl->mode_3d = BLEND_3D_H_ROW_INT;
			else
				ctl->mode_3d = BLEND_3D_NONE;
		}
		SDE_DEBUG("ctl %d match\n", iter.blk->id);

		if (++i == reqs->num_ctl)
			break;
	}

	if (i != reqs->num_ctl)
		return -ENAVAIL;

	for (i = 0; i < ARRAY_SIZE(ctls) && i < reqs->num_ctl; i++) {
		ctls[i]->rsvp_nxt = rsvp;
		MSM_EVTMSG(rm->dev, ctls[i]->type_name, rsvp->enc_id,
				ctls[i]->id);
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
	struct sde_cdm_cfg *cdm;

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_CDM);
	while (sde_rm_get_hw(rm, &iter)) {
		bool match = false;

		if (RESERVED_BY_OTHER(iter.blk, rsvp))
			continue;

		cdm = (struct sde_cdm_cfg *)(iter.blk->catalog);

		if (type == SDE_HW_BLK_INTF && id != INTF_MAX)
			match = test_bit(id, &cdm->intf_connect);
		else if (type == SDE_HW_BLK_WB && id != WB_MAX)
			match = test_bit(id, &cdm->wb_connect);

		SDE_DEBUG("type %d id %d, cdm intfs %lu wbs %lu match %d\n",
				type, id, cdm->intf_connect, cdm->wb_connect,
				match);

		if (!match)
			continue;

		iter.blk->rsvp_nxt = rsvp;
		MSM_EVTMSG(rm->dev, iter.blk->type_name, rsvp->enc_id,
				iter.blk->id);
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
	while (sde_rm_get_hw(rm, &iter)) {
		if (iter.blk->id != id)
			continue;

		if (RESERVED_BY_OTHER(iter.blk, rsvp)) {
			SDE_ERROR("type %d id %d already reserved\n", type, id);
			return -ENAVAIL;
		}

		iter.blk->rsvp_nxt = rsvp;
		MSM_EVTMSG(rm->dev, iter.blk->type_name,
				rsvp->enc_id, iter.blk->id);
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

	/* Create reservation info, tag reserved blocks with it as we go */
	rsvp->seq = ++rm->rsvp_next_seq;
	rsvp->enc_id = enc->base.id;
	rsvp->topology = reqs->top_name;
	list_add_tail(&rsvp->list, &rm->rsvps);

	/*
	 * Assign LMs and blocks whose usage is tied to them: DSPP & Pingpong.
	 * Do assignment preferring to give away low-resource mixers first:
	 * - Check mixers without DSPPs
	 * - Only then allow to grab from mixers with DSPP capability
	 */
	ret = _sde_rm_reserve_lms(rm, rsvp, reqs);
	if (ret && !RM_RQ_DSPP(reqs)) {
		reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_DSPP);
		ret = _sde_rm_reserve_lms(rm, rsvp, reqs);
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
	_sde_rm_reserve_ctls(rm, rsvp, reqs);
	if (ret && !reqs->needs_split_display) {
		reqs->needs_split_display = true;
		_sde_rm_reserve_ctls(rm, rsvp, reqs);
	}
	if (ret) {
		SDE_ERROR("unable to find appropriate CTL\n");
		return ret;
	}

	/* Assign INTFs, WBs, and blks whose usage is tied to them: CTL & CDM */
	ret = _sde_rm_reserve_intf_related_hw(rm, rsvp, &reqs->hw_res);
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

	/**
	 * DRM<->HW Topologies
	 *
	 * Name: SINGLEPIPE
	 * Description: 1 LM, 1 PP, 1 INTF
	 * Condition: 1 DRM Encoder w/ 1 Display Tiles (Default)
	 *
	 * Name: DUALPIPE
	 * Description: 2 LM, 2 PP, 2 INTF
	 * Condition: 1 DRM Encoder w/ 2 Display Tiles
	 *
	 * Name: PPSPLIT
	 * Description: 1 LM, 1 PP + 1 Slave PP, 2 INTF
	 * Condition:
	 *	1 DRM Encoder w/ 2 Display Tiles
	 *	topology_control & SDE_TOPREQ_PPSPLIT
	 *
	 * Name: DUALPIPEMERGE
	 * Description: 2 LM, 2 PP, 3DMux, 1 INTF
	 * Condition:
	 *	1 DRM Encoder w/ 1 Display Tiles
	 *	display_info.max_width >= layer_mixer.max_width
	 *
	 * Name: DUALPIPEMERGE
	 * Description: 2 LM, 2 PP, 3DMux, 1 INTF
	 * Condition:
	 *	1 DRM Encoder w/ 1 Display Tiles
	 *	display_info.max_width <= layer_mixer.max_width
	 *	topology_control & SDE_TOPREQ_FORCE_TILING
	 */

	memset(reqs, 0, sizeof(*reqs));

	reqs->top_ctrl = sde_connector_get_property(conn_state,
			CONNECTOR_PROP_TOPOLOGY_CONTROL);
	sde_encoder_get_hw_resources(enc, &reqs->hw_res, conn_state);

	/* Base assumption is LMs = h_tiles, conditions below may override */
	reqs->num_lm = reqs->hw_res.display_num_of_h_tiles;

	if (reqs->num_lm == 2) {
		if (RM_RQ_PPSPLIT(reqs)) {
			/* user requests serving dual display with 1 lm */
			reqs->top_name = SDE_RM_TOPOLOGY_PPSPLIT;
			reqs->num_lm = 1;
			reqs->num_ctl = 1;
			reqs->needs_split_display = true;
		} else {
			/* dual display, serve with 2 lms */
			reqs->top_name = SDE_RM_TOPOLOGY_DUALPIPE;
			reqs->num_ctl = 2;
			reqs->needs_split_display = true;
		}

	} else if (reqs->num_lm == 1) {
		if (mode->hdisplay > rm->lm_max_width) {
			/* wide display, must split across 2 lm and merge */
			reqs->top_name = SDE_RM_TOPOLOGY_DUALPIPEMERGE;
			reqs->num_lm = 2;
			reqs->num_ctl = 1;
			reqs->needs_split_display = false;
		} else if (RM_RQ_FORCE_TILING(reqs)) {
			/* thin display, but user requests 2 lm and merge */
			reqs->top_name = SDE_RM_TOPOLOGY_DUALPIPEMERGE;
			reqs->num_lm = 2;
			reqs->num_ctl = 1;
			reqs->needs_split_display = false;
		} else {
			/* thin display, serve with only 1 lm */
			reqs->top_name = SDE_RM_TOPOLOGY_SINGLEPIPE;
			reqs->num_ctl = 1;
			reqs->needs_split_display = false;
		}

	} else {
		/* Currently no configurations with # LM > 2 */
		SDE_ERROR("unsupported # of mixers %d\n", reqs->num_lm);
		return -EINVAL;
	}

	SDE_DEBUG("top_ctrl 0x%llX num_h_tiles %d\n", reqs->top_ctrl,
			reqs->hw_res.display_num_of_h_tiles);
	SDE_DEBUG("display_max_width %d rm->lm_max_width %d\n",
			mode->hdisplay, rm->lm_max_width);
	SDE_DEBUG("num_lm %d num_ctl %d topology_name %d\n", reqs->num_lm,
			reqs->num_ctl, reqs->top_name);
	MSM_EVT(rm->dev, mode->hdisplay, rm->lm_max_width);
	MSM_EVT(rm->dev, reqs->num_lm, reqs->top_ctrl);
	MSM_EVT(rm->dev, reqs->top_name, 0);

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

/**
 * _sde_rm_release_rsvp - release resources and release a reservation
 * @rm:	KMS handle
 * @rsvp:	RSVP pointer to release and release resources for
 */
void _sde_rm_release_rsvp(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp)
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
				SDE_DEBUG("rel rsvp %d enc %d %s %d\n",
						rsvp->seq, rsvp->enc_id,
						blk->type_name, blk->id);
			}
			if (blk->rsvp_nxt == rsvp) {
				blk->rsvp_nxt = NULL;
				SDE_DEBUG("rel rsvp_nxt %d enc %d %s %d\n",
						rsvp->seq, rsvp->enc_id,
						blk->type_name, blk->id);
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

	rsvp = _sde_rm_get_rsvp(rm, enc);
	if (!rsvp) {
		SDE_ERROR("failed to find rsvp for enc %d\n", enc->base.id);
		return;
	}

	conn = _sde_rm_get_connector(enc);
	if (!conn) {
		SDE_ERROR("failed to get connector for enc %d\n", enc->base.id);
		return;
	}

	top_ctrl = sde_connector_get_property(conn->state,
			CONNECTOR_PROP_TOPOLOGY_CONTROL);

	if (top_ctrl & BIT(SDE_RM_TOPCTL_RESERVE_LOCK)) {
		SDE_DEBUG("rsvp[s%de%d] not releasing locked resources\n",
				rsvp->seq, rsvp->enc_id);
	} else {
		SDE_DEBUG("release rsvp[s%de%d]\n", rsvp->seq,
				rsvp->enc_id);
		_sde_rm_release_rsvp(rm, rsvp);
		(void) msm_property_set_property(
				sde_connector_get_propinfo(conn),
				sde_connector_get_property_values(conn->state),
				CONNECTOR_PROP_TOPOLOGY_NAME,
				SDE_RM_TOPOLOGY_UNKNOWN);
	}
}

static int _sde_rm_commit_rsvp(
		struct sde_rm *rm,
		struct sde_rm_rsvp *rsvp,
		struct drm_connector_state *conn_state)
{
	struct sde_rm_hw_blk *blk;
	enum sde_hw_blk_type type;
	int ret = 0;

	ret = msm_property_set_property(
			sde_connector_get_propinfo(conn_state->connector),
			sde_connector_get_property_values(conn_state),
			CONNECTOR_PROP_TOPOLOGY_NAME,
			rsvp->topology);
	if (ret)
		_sde_rm_release_rsvp(rm, rsvp);

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
		MSM_EVT(rm->dev, rsvp->enc_id, rsvp->topology);
	}

	return ret;
}

int sde_rm_check_property_topctl(uint64_t val)
{
	if ((BIT(SDE_RM_TOPCTL_FORCE_TILING) & val) &&
			(BIT(SDE_RM_TOPCTL_PPSPLIT) & val)) {
		SDE_ERROR("ppsplit & force_tiling are incompatible\n");
		return -EINVAL;
	}

	return 0;
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
	int ret;

	if (!rm || !enc || !crtc_state || !conn_state) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	/* Check if this is just a page-flip */
	if (!drm_atomic_crtc_needs_modeset(crtc_state))
		return 0;

	SDE_DEBUG("reserving hw for conn %d enc %d crtc %d test_only %d\n",
			conn_state->connector->base.id, enc->base.id,
			crtc_state->crtc->base.id, test_only);
	MSM_EVT(rm->dev, enc->base.id, conn_state->connector->base.id);

	_sde_rm_print_rsvps(rm, "begin_reserve");

	ret = _sde_rm_populate_requirements(rm, enc, crtc_state,
			conn_state, &reqs);
	if (ret) {
		SDE_ERROR("failed to populate hw requirements\n");
		return ret;
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
	if (!rsvp_nxt)
		return -ENOMEM;

	rsvp_cur = _sde_rm_get_rsvp(rm, enc);

	/*
	 * User can request that we clear out any reservation during the
	 * atomic_check phase by using this CLEAR bit
	 */
	if (rsvp_cur && test_only && RM_RQ_CLEAR(&reqs)) {
		SDE_DEBUG("test_only & CLEAR: clear rsvp[s%de%d]\n",
				rsvp_cur->seq, rsvp_cur->enc_id);
		_sde_rm_release_rsvp(rm, rsvp_cur);
		rsvp_cur = NULL;
		_sde_rm_print_rsvps(rm, "post_clear");
	}

	/* Check the proposed reservation, store it in hw's "next" field */
	ret = _sde_rm_make_next_rsvp(rm, enc, crtc_state, conn_state,
			rsvp_nxt, &reqs);

	_sde_rm_print_rsvps(rm, "new_rsvp_next");

	if (ret) {
		SDE_ERROR("failed to reserve hw resources: %d\n", ret);
		_sde_rm_release_rsvp(rm, rsvp_nxt);
	} else if (test_only && !RM_RQ_LOCK(&reqs)) {
		/*
		 * Normally, if test_only, test the reservation and then undo
		 * However, if the user requests LOCK, then keep the reservation
		 * made during the atomic_check phase.
		 */
		SDE_DEBUG("test_only: discard test rsvp[s%de%d]\n",
				rsvp_nxt->seq, rsvp_nxt->enc_id);
		_sde_rm_release_rsvp(rm, rsvp_nxt);
	} else {
		if (test_only && RM_RQ_LOCK(&reqs))
			SDE_DEBUG("test_only & LOCK: lock rsvp[s%de%d]\n",
					rsvp_nxt->seq, rsvp_nxt->enc_id);

		_sde_rm_release_rsvp(rm, rsvp_cur);

		ret = _sde_rm_commit_rsvp(rm, rsvp_nxt, conn_state);
	}

	_sde_rm_print_rsvps(rm, "final");

	return ret;
}
