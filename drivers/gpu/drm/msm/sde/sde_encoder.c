/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "msm_drv.h"
#include "sde_kms.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_intf.h"
#include "sde_hw_ctl.h"
#include "sde_formats.h"
#include "sde_encoder_phys.h"
#include "display_manager.h"

/*
 * Two to anticipate panels that can do cmd/vid dynamic switching
 * plan is to create all possible physical encoder types, and switch between
 * them at runtime
 */
#define NUM_PHYS_ENCODER_TYPES 2

#define MAX_PHYS_ENCODERS_PER_VIRTUAL \
	(MAX_H_TILES_PER_DISPLAY * NUM_PHYS_ENCODER_TYPES)

#define WAIT_TIMEOUT_MSEC 100

/**
 * struct sde_encoder_virt - virtual encoder. Container of one or more physical
 *	encoders. Virtual encoder manages one "logical" display. Physical
 *	encoders manage one intf block, tied to a specific panel/sub-panel.
 *	Virtual encoder defers as much as possible to the physical encoders.
 *	Virtual encoder registers itself with the DRM Framework as the encoder.
 * @base:		drm_encoder base class for registration with DRM
 * @spin_lock:		Lock for IRQ purposes
 * @bus_scaling_client:	Client handle to the bus scaling interface
 * @num_phys_encs:	Actual number of physical encoders contained.
 * @phys_encs:		Container of physical encoders managed.
 * @cur_master:		Pointer to the current master in this mode. Optimization
 *			Only valid after enable. Cleared as disable.
 * @crtc_vblank_cb:	Callback into the upper layer / CRTC for
 *			notification of the VBLANK
 * @crtc_vblank_cb_data:	Data from upper layer for VBLANK notification
 * @pending_kickoff_mask:	Bitmask used to track which physical encoders
 *				still have pending transmissions before we can
 *				trigger the next kickoff. Bitmask tracks the
 *				index of the phys_enc table. Protect since
 *				shared between irq and commit thread
 * @crtc_kickoff_cb:		Callback into CRTC that will flush & start
 *				all CTL paths
 * @crtc_kickoff_cb_data:	Opaque user data given to crtc_kickoff_cb
 * @pending_kickoff_mask:	Bitmask tracking which phys_enc we are still
 *				waiting on before we can trigger the next
 *				kickoff. Bit0 = phys_encs[0] etc.
 * @pending_kickoff_wq:		Wait queue commit thread to wait on phys_encs
 *				become ready for kickoff in IRQ contexts
 */
struct sde_encoder_virt {
	struct drm_encoder base;
	spinlock_t spin_lock;
	uint32_t bus_scaling_client;

	uint32_t display_num_of_h_tiles;

	unsigned int num_phys_encs;
	struct sde_encoder_phys *phys_encs[MAX_PHYS_ENCODERS_PER_VIRTUAL];
	struct sde_encoder_phys *cur_master;

	void (*crtc_vblank_cb)(void *);
	void *crtc_vblank_cb_data;

	unsigned int pending_kickoff_mask;
	wait_queue_head_t pending_kickoff_wq;
};

#define to_sde_encoder_virt(x) container_of(x, struct sde_encoder_virt, base)

#ifdef CONFIG_MSM_BUS_SCALING
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#define MDP_BUS_VECTOR_ENTRY(ab_val, ib_val)		\
	{						\
		.src = MSM_BUS_MASTER_MDP_PORT0,	\
		.dst = MSM_BUS_SLAVE_EBI_CH0,		\
		.ab = (ab_val),				\
		.ib = (ib_val),				\
	}

static struct msm_bus_vectors mdp_bus_vectors[] = {
	MDP_BUS_VECTOR_ENTRY(0, 0),
	MDP_BUS_VECTOR_ENTRY(8000000000, 8000000000),
};

static struct msm_bus_paths mdp_bus_usecases[] = { {
						    .num_paths = 1,
						    .vectors =
						    &mdp_bus_vectors[0],
						    }, {
							.num_paths = 1,
							.vectors =
							&mdp_bus_vectors[1],
							}
};

static struct msm_bus_scale_pdata mdp_bus_scale_table = {
	.usecase = mdp_bus_usecases,
	.num_usecases = ARRAY_SIZE(mdp_bus_usecases),
	.name = "mdss_mdp",
};

static void bs_init(struct sde_encoder_virt *sde_enc)
{
	sde_enc->bus_scaling_client =
	    msm_bus_scale_register_client(&mdp_bus_scale_table);
	DBG("bus scale client: %08x", sde_enc->bus_scaling_client);
}

static void bs_fini(struct sde_encoder_virt *sde_enc)
{
	if (sde_enc->bus_scaling_client) {
		msm_bus_scale_unregister_client(sde_enc->bus_scaling_client);
		sde_enc->bus_scaling_client = 0;
	}
}

static void bs_set(struct sde_encoder_virt *sde_enc, int idx)
{
	if (sde_enc->bus_scaling_client) {
		DBG("set bus scaling: %d", idx);
		idx = 1;
		msm_bus_scale_client_update_request(sde_enc->bus_scaling_client,
						    idx);
	}
}
#else
static void bs_init(struct sde_encoder_virt *sde_enc)
{
}

static void bs_fini(struct sde_encoder_virt *sde_enc)
{
}

static void bs_set(struct sde_encoder_virt *sde_enc, int idx)
{
}
#endif

void sde_encoder_get_hw_resources(struct drm_encoder *drm_enc,
		struct sde_encoder_hw_resources *hw_res,
		struct drm_connector_state *conn_state)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;

	DBG("");

	if (!hw_res || !drm_enc || !conn_state) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	/* Query resources used by phys encs, expected to be without overlap */
	memset(hw_res, 0, sizeof(*hw_res));
	hw_res->display_num_of_h_tiles = sde_enc->display_num_of_h_tiles;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.get_hw_resources)
			phys->ops.get_hw_resources(phys, hw_res, conn_state);
	}
}

bool sde_encoder_needs_ctl_start(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	struct sde_encoder_phys *phys;

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return false;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	phys = sde_enc->cur_master;

	if (phys && phys->ops.needs_ctl_start)
		return phys->ops.needs_ctl_start(phys);

	return false;
}

static void sde_encoder_destroy(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;

	DBG("");

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	for (i = 0; i < ARRAY_SIZE(sde_enc->phys_encs); i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.destroy) {
			phys->ops.destroy(phys);
			--sde_enc->num_phys_encs;
			sde_enc->phys_encs[i] = NULL;
		}
	}

	if (sde_enc->num_phys_encs) {
		DRM_ERROR("Expected num_phys_encs to be 0 not %d\n",
				sde_enc->num_phys_encs);
	}

	drm_encoder_cleanup(drm_enc);
	bs_fini(sde_enc);
	kfree(sde_enc);
}

static int sde_encoder_virt_atomic_check(
		struct drm_encoder *drm_enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	const struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;
	int i = 0;
	int ret = 0;

	DBG("");

	if (!drm_enc || !crtc_state || !conn_state) {
		DRM_ERROR("Invalid pointer");
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	mode = &crtc_state->mode;
	adj_mode = &crtc_state->adjusted_mode;
	MSM_EVT(drm_enc->dev, 0, 0);

	/* perform atomic check on the first physical encoder (master) */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.atomic_check)
			ret = phys->ops.atomic_check(phys, crtc_state,
					conn_state);
		else if (phys && phys->ops.mode_fixup)
			if (!phys->ops.mode_fixup(phys, mode, adj_mode))
				ret = -EINVAL;

		if (ret) {
			SDE_ERROR("enc %d mode unsupported, phys %d\n",
					drm_enc->base.id, i);
			break;
		}
	}

	/* Reserve dynamic resources now. Indicating AtomicTest phase */
	if (!ret)
		ret = sde_rm_reserve(&sde_kms->rm, drm_enc, crtc_state,
				conn_state, true);

	/* Call to populate mode->crtc* information required by framework */
	drm_mode_set_crtcinfo(adj_mode, 0);

	MSM_EVT(drm_enc->dev, adj_mode->flags, adj_mode->private_flags);

	return ret;
}

static void sde_encoder_virt_mode_set(struct drm_encoder *drm_enc,
				      struct drm_display_mode *mode,
				      struct drm_display_mode *adj_mode)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct list_head *connector_list;
	struct drm_connector *conn = NULL, *conn_iter;
	int i = 0, ret;

	DBG("");

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	connector_list = &sde_kms->dev->mode_config.connector_list;

	MSM_EVT(drm_enc->dev, 0, 0);

	list_for_each_entry(conn_iter, connector_list, head)
		if (conn_iter->encoder == drm_enc)
			conn = conn_iter;

	if (!conn) {
		SDE_ERROR("enc %d failed to find attached connector\n",
				drm_enc->base.id);
		return;
	}

	/* Reserve dynamic resources now. Indicating non-AtomicTest phase */
	ret = sde_rm_reserve(&sde_kms->rm, drm_enc, drm_enc->crtc->state,
			conn->state, false);
	if (ret) {
		SDE_ERROR("enc %d failed to reserve hw resources, ret %d\n",
				drm_enc->base.id, ret);
		return;
	}

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.mode_set)
			phys->ops.mode_set(phys, mode, adj_mode);
	}
}

static void sde_encoder_virt_enable(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;

	DBG("");

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	MSM_EVT(drm_enc->dev, 0, 0);

	bs_set(sde_enc, 1);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys) {
			if (phys->ops.enable)
				phys->ops.enable(phys);

			/*
			 * Master can switch at enable time.
			 * It is based on the current mode (CMD/VID) and
			 * the encoder role found at panel probe time
			 */
			if (phys->ops.is_master && phys->ops.is_master(phys)) {
				DBG("phys enc master is now idx %d", i);
				sde_enc->cur_master = phys;
			}
		}
	}
}

static void sde_encoder_virt_disable(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	int i = 0;

	DBG("");

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	MSM_EVT(drm_enc->dev, 0, 0);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.disable)
			phys->ops.disable(phys);
	}

	sde_enc->cur_master = NULL;
	DBG("clear phys enc master");

	bs_set(sde_enc, 0);

	sde_rm_release(&sde_kms->rm, drm_enc);
}

static const struct drm_encoder_helper_funcs sde_encoder_helper_funcs = {
	.mode_set = sde_encoder_virt_mode_set,
	.disable = sde_encoder_virt_disable,
	.enable = sde_encoder_virt_enable,
	.atomic_check = sde_encoder_virt_atomic_check,
};

static const struct drm_encoder_funcs sde_encoder_funcs = {
		.destroy = sde_encoder_destroy,
};

static enum sde_intf sde_encoder_get_intf(struct sde_mdss_cfg *catalog,
		enum sde_intf_type type, u32 controller_id)
{
	int i = 0;

	DBG("");

	for (i = 0; i < catalog->intf_count; i++) {
		if (catalog->intf[i].type == type
		    && catalog->intf[i].controller_id == controller_id) {
			return catalog->intf[i].id;
		}
	}

	return INTF_MAX;
}

static enum sde_wb sde_encoder_get_wb(struct sde_mdss_cfg *catalog,
		enum sde_intf_type type, u32 controller_id)
{
	if (controller_id < catalog->wb_count)
		return catalog->wb[controller_id].id;

	return WB_MAX;
}

static void sde_encoder_vblank_callback(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	unsigned long lock_flags;

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	spin_lock_irqsave(&sde_enc->spin_lock, lock_flags);
	if (sde_enc->crtc_vblank_cb)
		sde_enc->crtc_vblank_cb(sde_enc->crtc_vblank_cb_data);
	spin_unlock_irqrestore(&sde_enc->spin_lock, lock_flags);
}

void sde_encoder_register_vblank_callback(struct drm_encoder *drm_enc,
		void (*vbl_cb)(void *), void *vbl_data)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	unsigned long lock_flags;
	bool enable;
	int i;

	enable = vbl_cb ? true : false;

	MSM_EVT(drm_enc->dev, enable, 0);

	spin_lock_irqsave(&sde_enc->spin_lock, lock_flags);
	sde_enc->crtc_vblank_cb = vbl_cb;
	sde_enc->crtc_vblank_cb_data = vbl_data;
	spin_unlock_irqrestore(&sde_enc->spin_lock, lock_flags);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.control_vblank_irq)
			phys->ops.control_vblank_irq(phys, enable);
	}
}

static void sde_encoder_handle_phys_enc_ready_for_kickoff(
		struct drm_encoder *drm_enc,
		struct sde_encoder_phys *ready_phys)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	unsigned long lock_flags;
	unsigned int i, mask;

	/* One of the physical encoders has become ready for kickoff */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		if (sde_enc->phys_encs[i] == ready_phys) {
			spin_lock_irqsave(&sde_enc->spin_lock, lock_flags);
			sde_enc->pending_kickoff_mask &= ~(1 << i);
			mask = sde_enc->pending_kickoff_mask;
			spin_unlock_irqrestore(&sde_enc->spin_lock, lock_flags);
			MSM_EVT(drm_enc->dev, i, mask);
		}
	}

	/* Wake the commit thread to check if they all ready for kickoff */
	wake_up_all(&sde_enc->pending_kickoff_wq);
}

void sde_encoder_schedule_kickoff(struct drm_encoder *drm_enc,
		void (*kickoff_cb)(void *), void *kickoff_data)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	unsigned long lock_flags;
	bool need_to_wait;
	unsigned int i;
	int ret;

	if (!drm_enc) {
		DRM_ERROR("invalid arguments");
		return;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);

	MSM_EVT(drm_enc->dev, 0, 0);

	spin_lock_irqsave(&sde_enc->spin_lock, lock_flags);
	sde_enc->pending_kickoff_mask = 0;
	spin_unlock_irqrestore(&sde_enc->spin_lock, lock_flags);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		need_to_wait = false;
		phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.prepare_for_kickoff)
			phys->ops.prepare_for_kickoff(phys, &need_to_wait);

		if (need_to_wait) {
			spin_lock_irqsave(&sde_enc->spin_lock, lock_flags);
			sde_enc->pending_kickoff_mask |= 1 << i;
			spin_unlock_irqrestore(&sde_enc->spin_lock, lock_flags);
		}
	}

	spin_lock_irqsave(&sde_enc->spin_lock, lock_flags);
	MSM_EVT(drm_enc->dev, sde_enc->pending_kickoff_mask, 0);
	spin_unlock_irqrestore(&sde_enc->spin_lock, lock_flags);

	/* Wait for the busy phys encs to be ready */
	ret = -ERESTARTSYS;
	while (ret == -ERESTARTSYS) {
		spin_lock_irqsave(&sde_enc->spin_lock, lock_flags);
		ret = wait_event_interruptible_lock_irq_timeout(
				sde_enc->pending_kickoff_wq,
				sde_enc->pending_kickoff_mask == 0,
				sde_enc->spin_lock,
				msecs_to_jiffies(WAIT_TIMEOUT_MSEC));
		spin_unlock_irqrestore(&sde_enc->spin_lock, lock_flags);
		if (!ret)
			DBG("wait %u msec timed out", WAIT_TIMEOUT_MSEC);
	}

	/* All phys encs are ready to go, trigger the kickoff */
	if (kickoff_cb)
		kickoff_cb(kickoff_data);

	/* Allow phys encs to handle any post-kickoff business */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.handle_post_kickoff)
			phys->ops.handle_post_kickoff(phys);
	}
}

static int sde_encoder_virt_add_phys_encs(
		u32 display_caps,
		struct sde_encoder_virt *sde_enc,
		struct sde_enc_phys_init_params *params)
{
	struct sde_encoder_phys *enc = NULL;

	DBG("");

	/*
	 * We may create up to NUM_PHYS_ENCODER_TYPES physical encoder types
	 * in this function, check up-front.
	 */
	if (sde_enc->num_phys_encs + NUM_PHYS_ENCODER_TYPES >=
			ARRAY_SIZE(sde_enc->phys_encs)) {
		DRM_ERROR("Too many physical encoders %d, unable to add\n",
			  sde_enc->num_phys_encs);
		return -EINVAL;
	}

	if (display_caps & MSM_DISPLAY_CAP_VID_MODE) {
		enc = sde_encoder_phys_vid_init(params);

		if (IS_ERR_OR_NULL(enc)) {
			DRM_ERROR("Failed to initialize phys vid enc: %ld\n",
				PTR_ERR(enc));
			return enc == 0 ? -EINVAL : PTR_ERR(enc);
		}

		sde_enc->phys_encs[sde_enc->num_phys_encs] = enc;
		++sde_enc->num_phys_encs;
	}

	if (display_caps & MSM_DISPLAY_CAP_CMD_MODE) {
		enc = sde_encoder_phys_cmd_init(params);

		if (IS_ERR_OR_NULL(enc)) {
			DRM_ERROR("Failed to initialize phys cmd enc: %ld\n",
				PTR_ERR(enc));
			return enc == 0 ? -EINVAL : PTR_ERR(enc);
		}

		sde_enc->phys_encs[sde_enc->num_phys_encs] = enc;
		++sde_enc->num_phys_encs;
	}

	return 0;
}

static int sde_encoder_virt_add_phys_enc_wb(struct sde_encoder_virt *sde_enc,
		struct sde_enc_phys_init_params *params)
{
	struct sde_encoder_phys *enc = NULL;

	DBG("");

	if (sde_enc->num_phys_encs + 1 >= ARRAY_SIZE(sde_enc->phys_encs)) {
		DRM_ERROR("Too many physical encoders %d, unable to add\n",
			  sde_enc->num_phys_encs);
		return -EINVAL;
	}

	enc = sde_encoder_phys_wb_init(params);

	if (IS_ERR_OR_NULL(enc)) {
		DRM_ERROR("Failed to initialize phys wb enc: %ld\n",
			PTR_ERR(enc));
		return enc == 0 ? -EINVAL : PTR_ERR(enc);
	}

	sde_enc->phys_encs[sde_enc->num_phys_encs] = enc;
	++sde_enc->num_phys_encs;

	return 0;
}

static int sde_encoder_setup_display(struct sde_encoder_virt *sde_enc,
				 struct sde_kms *sde_kms,
				 struct msm_display_info *disp_info,
				 int *drm_enc_mode)
{
	int ret = 0;
	int i = 0;
	enum sde_intf_type intf_type;
	struct sde_encoder_virt_ops parent_ops = {
		sde_encoder_vblank_callback,
		sde_encoder_handle_phys_enc_ready_for_kickoff
	};
	struct sde_enc_phys_init_params phys_params;

	memset(&phys_params, 0, sizeof(phys_params));
	phys_params.sde_kms = sde_kms;
	phys_params.parent = &sde_enc->base;
	phys_params.parent_ops = parent_ops;

	DBG("");

	if (disp_info->intf_type == DRM_MODE_CONNECTOR_DSI) {
		*drm_enc_mode = DRM_MODE_ENCODER_DSI;
		intf_type = INTF_DSI;
	} else if (disp_info->intf_type == DRM_MODE_CONNECTOR_HDMIA) {
		*drm_enc_mode = DRM_MODE_ENCODER_TMDS;
		intf_type = INTF_HDMI;
	} else if (disp_info->intf_type == DRM_MODE_CONNECTOR_VIRTUAL) {
		*drm_enc_mode = DRM_MODE_ENCODER_VIRTUAL;
		intf_type = INTF_WB;
	} else {
		DRM_ERROR("Unsupported display interface type");
		return -EINVAL;
	}

	WARN_ON(disp_info->num_of_h_tiles < 1);

	sde_enc->display_num_of_h_tiles = disp_info->num_of_h_tiles;

	DBG("disp_info->num_of_h_tiles %d", disp_info->num_of_h_tiles);

	for (i = 0; i < disp_info->num_of_h_tiles && !ret; i++) {
		/*
		 * Left-most tile is at index 0, content is controller id
		 * h_tile_instance_ids[2] = {0, 1}; DSI0 = left, DSI1 = right
		 * h_tile_instance_ids[2] = {1, 0}; DSI1 = left, DSI0 = right
		 */
		u32 controller_id = disp_info->h_tile_instance[i];

		if (disp_info->num_of_h_tiles > 1) {
			if (i == 0)
				phys_params.split_role = ENC_ROLE_MASTER;
			else
				phys_params.split_role = ENC_ROLE_SLAVE;
		} else {
			phys_params.split_role = ENC_ROLE_SOLO;
		}

		DBG("h_tile_instance %d = %d, split_role %d",
				i, controller_id, phys_params.split_role);

		if (intf_type == INTF_WB) {
			phys_params.intf_idx = INTF_MAX;
			phys_params.wb_idx = sde_encoder_get_wb(
					sde_kms->catalog,
					intf_type, controller_id);
			if (phys_params.wb_idx == WB_MAX) {
				DRM_ERROR(
					"Error: could not get writeback: type %d, id %d\n",
					intf_type, controller_id);
				ret = -EINVAL;
			}
		} else {
			phys_params.wb_idx = WB_MAX;
			phys_params.intf_idx = sde_encoder_get_intf(
					sde_kms->catalog, intf_type,
					controller_id);
			if (phys_params.intf_idx == INTF_MAX) {
				DRM_ERROR(
					"Error: could not get physical: type %d, id %d\n",
					intf_type, controller_id);
				ret = -EINVAL;
			}
		}

		if (!ret) {
			if (intf_type == INTF_WB)
				ret = sde_encoder_virt_add_phys_enc_wb(sde_enc,
						&phys_params);
			else
				ret = sde_encoder_virt_add_phys_encs(
						disp_info->capabilities,
						sde_enc,
						&phys_params);
			if (ret)
				DRM_ERROR("Failed to add phys encs\n");
		}
	}


	return ret;
}

static struct drm_encoder *sde_encoder_virt_init(
		struct drm_device *dev, struct msm_display_info *disp_info)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct sde_kms *sde_kms = to_sde_kms(priv->kms);
	struct drm_encoder *drm_enc = NULL;
	struct sde_encoder_virt *sde_enc = NULL;
	int drm_enc_mode = DRM_MODE_ENCODER_NONE;
	int ret = 0;

	DBG("");

	sde_enc = kzalloc(sizeof(*sde_enc), GFP_KERNEL);
	if (!sde_enc) {
		ret = -ENOMEM;
		goto fail;
	}

	ret = sde_encoder_setup_display(sde_enc, sde_kms, disp_info,
			&drm_enc_mode);
	if (ret)
		goto fail;

	sde_enc->cur_master = NULL;
	spin_lock_init(&sde_enc->spin_lock);
	drm_enc = &sde_enc->base;
	drm_encoder_init(dev, drm_enc, &sde_encoder_funcs, drm_enc_mode);
	drm_encoder_helper_add(drm_enc, &sde_encoder_helper_funcs);
	bs_init(sde_enc);
	sde_enc->pending_kickoff_mask = 0;
	init_waitqueue_head(&sde_enc->pending_kickoff_wq);

	DBG("Created encoder");

	return drm_enc;

fail:
	DRM_ERROR("Failed to create encoder\n");
	if (drm_enc)
		sde_encoder_destroy(drm_enc);

	return ERR_PTR(ret);
}

int sde_encoder_wait_for_commit_done(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i, ret = 0;

	DBG("");

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return -EINVAL;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.wait_for_commit_done) {
			ret = phys->ops.wait_for_commit_done(phys);
			if (ret)
				return ret;
		}
	}

	return ret;
}

enum sde_intf_mode sde_encoder_get_intf_mode(struct drm_encoder *encoder)
{
	struct sde_encoder_virt *sde_enc = NULL;
	enum sde_intf_mode intf_mode = INTF_MODE_NONE;

	if (!encoder) {
		SDE_ERROR("invalid encoder\n");
		return INTF_MODE_NONE;
	}

	sde_enc = to_sde_encoder_virt(encoder);
	if (sde_enc->cur_master)
		intf_mode = sde_enc->cur_master->intf_mode;

	return intf_mode;
}

/* encoders init,
 * initialize encoder based on displays
 */
void sde_encoders_init(struct drm_device *dev)
{
	struct msm_drm_private *priv = NULL;
	struct display_manager *disp_man = NULL;
	u32 i = 0;
	u32 num_displays = 0;

	DBG("");

	if (!dev || !dev->dev_private) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	priv = dev->dev_private;
	priv->num_encoders = 0;
	if (!priv->kms || !priv->dm) {
		DRM_ERROR("Invalid pointer");
		return;
	}
	disp_man = priv->dm;

	num_displays = display_manager_get_count(disp_man);
	DBG("num_displays %d", num_displays);

	if (num_displays > ARRAY_SIZE(priv->encoders)) {
		num_displays = ARRAY_SIZE(priv->encoders);
		DRM_ERROR("Too many displays found, capping to %d",
				num_displays);
	}

	for (i = 0; i < num_displays; i++) {
		struct msm_display_info info = { 0 };
		struct drm_encoder *enc = NULL;
		u32 ret = 0;

		ret = display_manager_get_info_by_index(disp_man, i, &info);
		if (ret) {
			DRM_ERROR("Failed to get display info, %d", ret);
			return;
		}

		enc = sde_encoder_virt_init(dev, &info);
		if (IS_ERR_OR_NULL(enc)) {
			DRM_ERROR("Encoder initialization failed");
			return;
		}

		ret = display_manager_drm_init_by_index(disp_man, i, enc);
		if (ret) {
			DRM_ERROR("Display drm_init failed, %d", ret);
			return;
		}

		priv->encoders[priv->num_encoders++] = enc;
	}
}
