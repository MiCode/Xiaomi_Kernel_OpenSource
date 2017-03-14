/*
 * Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include <linux/debugfs.h>
#include <linux/seq_file.h>

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
#include "sde_color_processing.h"
#include "sde_trace.h"

#define SDE_DEBUG_ENC(e, fmt, ...) SDE_DEBUG("enc%d " fmt,\
		(e) ? (e)->base.base.id : -1, ##__VA_ARGS__)

#define SDE_ERROR_ENC(e, fmt, ...) SDE_ERROR("enc%d " fmt,\
		(e) ? (e)->base.base.id : -1, ##__VA_ARGS__)

/* timeout in frames waiting for frame done */
#define SDE_ENCODER_FRAME_DONE_TIMEOUT	60

/*
 * Two to anticipate panels that can do cmd/vid dynamic switching
 * plan is to create all possible physical encoder types, and switch between
 * them at runtime
 */
#define NUM_PHYS_ENCODER_TYPES 2

#define MAX_PHYS_ENCODERS_PER_VIRTUAL \
	(MAX_H_TILES_PER_DISPLAY * NUM_PHYS_ENCODER_TYPES)

#define MAX_CHANNELS_PER_ENC 2

/* rgb to yuv color space conversion matrix */
static struct sde_csc_cfg sde_csc_10bit_convert[SDE_MAX_CSC] = {
	[SDE_CSC_RGB2YUV_601L] = {
		{
			TO_S15D16(0x0083), TO_S15D16(0x0102), TO_S15D16(0x0032),
			TO_S15D16(0xffb4), TO_S15D16(0xff6b), TO_S15D16(0x00e1),
			TO_S15D16(0x00e1), TO_S15D16(0xff44), TO_S15D16(0xffdb),
		},
		{ 0x0, 0x0, 0x0,},
		{ 0x0040, 0x0200, 0x0200,},
		{ 0x0, 0x3ff, 0x0, 0x3ff, 0x0, 0x3ff,},
		{ 0x0040, 0x03ac, 0x0040, 0x03c0, 0x0040, 0x03c0,},
	},

	[SDE_CSC_RGB2YUV_601FR] = {
		{
			TO_S15D16(0x0099), TO_S15D16(0x012d), TO_S15D16(0x003a),
			TO_S15D16(0xffaa), TO_S15D16(0xff56), TO_S15D16(0x0100),
			TO_S15D16(0x0100), TO_S15D16(0xff2a), TO_S15D16(0xffd6),
		},
		{ 0x0, 0x0, 0x0,},
		{ 0x0000, 0x0200, 0x0200,},
		{ 0x0, 0x3ff, 0x0, 0x3ff, 0x0, 0x3ff,},
		{ 0x0, 0x3ff, 0x0, 0x3ff, 0x0, 0x3ff,},
	},

	[SDE_CSC_RGB2YUV_709L] = {
		{
			TO_S15D16(0x005d), TO_S15D16(0x013a), TO_S15D16(0x0020),
			TO_S15D16(0xffcc), TO_S15D16(0xff53), TO_S15D16(0x00e1),
			TO_S15D16(0x00e1), TO_S15D16(0xff34), TO_S15D16(0xffeb),
		},
		{ 0x0, 0x0, 0x0,},
		{ 0x0040, 0x0200, 0x0200,},
		{ 0x0, 0x3ff, 0x0, 0x3ff, 0x0, 0x3ff,},
		{ 0x0040, 0x03ac, 0x0040, 0x03c0, 0x0040, 0x03c0,},
	},

	[SDE_CSC_RGB2YUV_2020L] = {
		{
			TO_S15D16(0x0073), TO_S15D16(0x0129), TO_S15D16(0x001a),
			TO_S15D16(0xffc1), TO_S15D16(0xff5e), TO_S15D16(0x00e0),
			TO_S15D16(0x00e0), TO_S15D16(0xff32), TO_S15D16(0xffee),
		},
		{ 0x0, 0x0, 0x0,},
		{ 0x0040, 0x0200, 0x0200,},
		{ 0x0, 0x3ff, 0x0, 0x3ff, 0x0, 0x3ff,},
		{ 0x0040, 0x03ac, 0x0040, 0x03c0, 0x0040, 0x03c0,},
	},

	[SDE_CSC_RGB2YUV_2020FR] = {
		{
			TO_S15D16(0x0086), TO_S15D16(0x015b), TO_S15D16(0x001e),
			TO_S15D16(0xffb9), TO_S15D16(0xff47), TO_S15D16(0x0100),
			TO_S15D16(0x0100), TO_S15D16(0xff15), TO_S15D16(0xffeb),
		},
		{ 0x0, 0x0, 0x0,},
		{ 0x0, 0x0200, 0x0200,},
		{ 0x0, 0x3ff, 0x0, 0x3ff, 0x0, 0x3ff,},
		{ 0x0, 0x3ff, 0x0, 0x3ff, 0x0, 0x3ff,},
	},
};

/**
 * struct sde_encoder_virt - virtual encoder. Container of one or more physical
 *	encoders. Virtual encoder manages one "logical" display. Physical
 *	encoders manage one intf block, tied to a specific panel/sub-panel.
 *	Virtual encoder defers as much as possible to the physical encoders.
 *	Virtual encoder registers itself with the DRM Framework as the encoder.
 * @base:		drm_encoder base class for registration with DRM
 * @enc_spin_lock:	Virtual-Encoder-Wide Spin Lock for IRQ purposes
 * @bus_scaling_client:	Client handle to the bus scaling interface
 * @num_phys_encs:	Actual number of physical encoders contained.
 * @phys_encs:		Container of physical encoders managed.
 * @cur_master:		Pointer to the current master in this mode. Optimization
 *			Only valid after enable. Cleared as disable.
 * @hw_pp		Handle to the pingpong blocks used for the display. No.
 *                      pingpong blocks can be different than num_phys_encs.
 * @crtc_vblank_cb:	Callback into the upper layer / CRTC for
 *			notification of the VBLANK
 * @crtc_vblank_cb_data:	Data from upper layer for VBLANK notification
 * @crtc_kickoff_cb:		Callback into CRTC that will flush & start
 *				all CTL paths
 * @crtc_kickoff_cb_data:	Opaque user data given to crtc_kickoff_cb
 * @debugfs_root:		Debug file system root file node
 * @enc_lock:			Lock around physical encoder create/destroy and
				access.
 * @frame_busy_mask:		Bitmask tracking which phys_enc we are still
 *				busy processing current command.
 *				Bit0 = phys_encs[0] etc.
 * @crtc_frame_event_cb:	callback handler for frame event
 * @crtc_frame_event_cb_data:	callback handler private data
 * @crtc_request_flip_cb:	callback handler for requesting page-flip event
 * @crtc_request_flip_cb_data:	callback handler private data
 * @crtc_frame_event:		callback event
 * @frame_done_timeout:		frame done timeout in Hz
 * @frame_done_timer:		watchdog timer for frame done event
 */
struct sde_encoder_virt {
	struct drm_encoder base;
	spinlock_t enc_spinlock;
	uint32_t bus_scaling_client;

	uint32_t display_num_of_h_tiles;

	unsigned int num_phys_encs;
	struct sde_encoder_phys *phys_encs[MAX_PHYS_ENCODERS_PER_VIRTUAL];
	struct sde_encoder_phys *cur_master;
	struct sde_hw_pingpong *hw_pp[MAX_CHANNELS_PER_ENC];

	void (*crtc_vblank_cb)(void *);
	void *crtc_vblank_cb_data;

	struct dentry *debugfs_root;
	struct mutex enc_lock;
	DECLARE_BITMAP(frame_busy_mask, MAX_PHYS_ENCODERS_PER_VIRTUAL);
	void (*crtc_frame_event_cb)(void *, u32 event);
	void *crtc_frame_event_cb_data;
	void (*crtc_request_flip_cb)(void *);
	void *crtc_request_flip_cb_data;
	u32 crtc_frame_event;
	atomic_t frame_done_timeout;
	struct timer_list frame_done_timer;
};

#define to_sde_encoder_virt(x) container_of(x, struct sde_encoder_virt, base)

void sde_encoder_get_hw_resources(struct drm_encoder *drm_enc,
		struct sde_encoder_hw_resources *hw_res,
		struct drm_connector_state *conn_state)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;

	if (!hw_res || !drm_enc || !conn_state) {
		SDE_ERROR("invalid argument(s), drm_enc %d, res %d, state %d\n",
				drm_enc != 0, hw_res != 0, conn_state != 0);
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	/* Query resources used by phys encs, expected to be without overlap */
	memset(hw_res, 0, sizeof(*hw_res));
	hw_res->display_num_of_h_tiles = sde_enc->display_num_of_h_tiles;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.get_hw_resources)
			phys->ops.get_hw_resources(phys, hw_res, conn_state);
	}
}

void sde_encoder_destroy(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	mutex_lock(&sde_enc->enc_lock);
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.destroy) {
			phys->ops.destroy(phys);
			--sde_enc->num_phys_encs;
			sde_enc->phys_encs[i] = NULL;
		}
	}

	if (sde_enc->num_phys_encs)
		SDE_ERROR_ENC(sde_enc, "expected 0 num_phys_encs not %d\n",
				sde_enc->num_phys_encs);
	sde_enc->num_phys_encs = 0;
	mutex_unlock(&sde_enc->enc_lock);

	drm_encoder_cleanup(drm_enc);
	debugfs_remove_recursive(sde_enc->debugfs_root);
	mutex_destroy(&sde_enc->enc_lock);

	kfree(sde_enc);
}

void sde_encoder_helper_split_config(
		struct sde_encoder_phys *phys_enc,
		enum sde_intf interface)
{
	struct sde_encoder_virt *sde_enc;
	struct split_pipe_cfg cfg = { 0 };
	struct sde_hw_mdp *hw_mdptop;
	enum sde_rm_topology_name topology;

	if (!phys_enc || !phys_enc->hw_mdptop || !phys_enc->parent) {
		SDE_ERROR("invalid arg(s), encoder %d\n", phys_enc != 0);
		return;
	}

	sde_enc = to_sde_encoder_virt(phys_enc->parent);
	hw_mdptop = phys_enc->hw_mdptop;
	cfg.en = phys_enc->split_role != ENC_ROLE_SOLO;
	cfg.mode = phys_enc->intf_mode;
	cfg.intf = interface;

	if (cfg.en && phys_enc->ops.needs_single_flush &&
			phys_enc->ops.needs_single_flush(phys_enc))
		cfg.split_flush_en = true;

	topology = sde_connector_get_topology_name(phys_enc->connector);
	if (topology == SDE_RM_TOPOLOGY_PPSPLIT)
		cfg.pp_split_slave = cfg.intf;
	else
		cfg.pp_split_slave = INTF_MAX;

	if (phys_enc->split_role != ENC_ROLE_SLAVE) {
		/* master/solo encoder */
		SDE_DEBUG_ENC(sde_enc, "enable %d\n", cfg.en);

		if (hw_mdptop->ops.setup_split_pipe)
			hw_mdptop->ops.setup_split_pipe(hw_mdptop, &cfg);
	} else {
		/*
		 * slave encoder
		 * - determine split index from master index,
		 *   assume master is first pp
		 */
		cfg.pp_split_index = sde_enc->hw_pp[0]->idx - PINGPONG_0;
		SDE_DEBUG_ENC(sde_enc, "master using pp%d\n",
				cfg.pp_split_index);

		if (hw_mdptop->ops.setup_pp_split)
			hw_mdptop->ops.setup_pp_split(hw_mdptop, &cfg);
	}
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

	if (!drm_enc || !crtc_state || !conn_state) {
		SDE_ERROR("invalid arg(s), drm_enc %d, crtc/conn state %d/%d\n",
				drm_enc != 0, crtc_state != 0, conn_state != 0);
		return -EINVAL;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	mode = &crtc_state->mode;
	adj_mode = &crtc_state->adjusted_mode;
	SDE_EVT32(DRMID(drm_enc));

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
			SDE_ERROR_ENC(sde_enc,
					"mode unsupported, phys idx %d\n", i);
			break;
		}
	}

	/* Reserve dynamic resources now. Indicating AtomicTest phase */
	if (!ret)
		ret = sde_rm_reserve(&sde_kms->rm, drm_enc, crtc_state,
				conn_state, true);

	if (!ret)
		drm_mode_set_crtcinfo(adj_mode, 0);

	SDE_EVT32(DRMID(drm_enc), adj_mode->flags, adj_mode->private_flags);

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
	struct sde_rm_hw_iter pp_iter;
	int i = 0, ret;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);
	connector_list = &sde_kms->dev->mode_config.connector_list;

	SDE_EVT32(DRMID(drm_enc));

	list_for_each_entry(conn_iter, connector_list, head)
		if (conn_iter->encoder == drm_enc)
			conn = conn_iter;

	if (!conn) {
		SDE_ERROR_ENC(sde_enc, "failed to find attached connector\n");
		return;
	} else if (!conn->state) {
		SDE_ERROR_ENC(sde_enc, "invalid connector state\n");
		return;
	}

	/* Reserve dynamic resources now. Indicating non-AtomicTest phase */
	ret = sde_rm_reserve(&sde_kms->rm, drm_enc, drm_enc->crtc->state,
			conn->state, false);
	if (ret) {
		SDE_ERROR_ENC(sde_enc,
				"failed to reserve hw resources, %d\n", ret);
		return;
	}

	sde_rm_init_hw_iter(&pp_iter, drm_enc->base.id, SDE_HW_BLK_PINGPONG);
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		sde_enc->hw_pp[i] = NULL;
		if (!sde_rm_get_hw(&sde_kms->rm, &pp_iter))
			break;
		sde_enc->hw_pp[i] = (struct sde_hw_pingpong *) pp_iter.hw;
	}

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys) {
			if (!sde_enc->hw_pp[i]) {
				SDE_ERROR_ENC(sde_enc,
				    "invalid pingpong block for the encoder\n");
				return;
			}
			phys->hw_pp = sde_enc->hw_pp[i];
			phys->connector = conn->state->connector;
			if (phys->ops.mode_set)
				phys->ops.mode_set(phys, mode, adj_mode);
		}
	}
}

static void sde_encoder_virt_enable(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	int i = 0;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	} else if (!drm_enc->dev) {
		SDE_ERROR("invalid dev\n");
		return;
	} else if (!drm_enc->dev->dev_private) {
		SDE_ERROR("invalid dev_private\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	SDE_DEBUG_ENC(sde_enc, "\n");
	SDE_EVT32(DRMID(drm_enc));

	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, true);

	sde_enc->cur_master = NULL;
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys) {
			atomic_set(&phys->vsync_cnt, 0);
			atomic_set(&phys->underrun_cnt, 0);

			if (phys->ops.is_master && phys->ops.is_master(phys)) {
				SDE_DEBUG_ENC(sde_enc,
						"master is now idx %d\n", i);
				sde_enc->cur_master = phys;
			} else if (phys->ops.enable) {
				phys->ops.enable(phys);
			}
		}
	}

	if (!sde_enc->cur_master)
		SDE_ERROR("virt encoder has no master! num_phys %d\n", i);
	else if (sde_enc->cur_master->ops.enable)
		sde_enc->cur_master->ops.enable(sde_enc->cur_master);
}

static void sde_encoder_virt_disable(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	int i = 0;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	} else if (!drm_enc->dev) {
		SDE_ERROR("invalid dev\n");
		return;
	} else if (!drm_enc->dev->dev_private) {
		SDE_ERROR("invalid dev_private\n");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	priv = drm_enc->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	SDE_EVT32(DRMID(drm_enc));

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys) {
			if (phys->ops.disable && !phys->ops.is_master(phys))
				phys->ops.disable(phys);
			phys->connector = NULL;
			atomic_set(&phys->vsync_cnt, 0);
			atomic_set(&phys->underrun_cnt, 0);
		}
	}

	/* after phys waits for frame-done, should be no more frames pending */
	if (atomic_xchg(&sde_enc->frame_done_timeout, 0)) {
		SDE_ERROR("enc%d timeout pending\n", drm_enc->base.id);
		del_timer_sync(&sde_enc->frame_done_timer);
	}

	if (sde_enc->cur_master && sde_enc->cur_master->ops.disable)
		sde_enc->cur_master->ops.disable(sde_enc->cur_master);

	sde_enc->cur_master = NULL;
	SDE_DEBUG_ENC(sde_enc, "cleared master\n");

	sde_rm_release(&sde_kms->rm, drm_enc);

	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);
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

static void sde_encoder_vblank_callback(struct drm_encoder *drm_enc,
		struct sde_encoder_phys *phy_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	unsigned long lock_flags;

	if (!drm_enc || !phy_enc)
		return;

	SDE_ATRACE_BEGIN("encoder_vblank_callback");
	sde_enc = to_sde_encoder_virt(drm_enc);

	spin_lock_irqsave(&sde_enc->enc_spinlock, lock_flags);
	if (sde_enc->crtc_vblank_cb)
		sde_enc->crtc_vblank_cb(sde_enc->crtc_vblank_cb_data);
	spin_unlock_irqrestore(&sde_enc->enc_spinlock, lock_flags);

	atomic_inc(&phy_enc->vsync_cnt);
	SDE_ATRACE_END("encoder_vblank_callback");
}

static void sde_encoder_underrun_callback(struct drm_encoder *drm_enc,
		struct sde_encoder_phys *phy_enc)
{
	if (!phy_enc)
		return;

	SDE_ATRACE_BEGIN("encoder_underrun_callback");
	atomic_inc(&phy_enc->underrun_cnt);
	SDE_EVT32(DRMID(drm_enc), atomic_read(&phy_enc->underrun_cnt));
	SDE_ATRACE_END("encoder_underrun_callback");
}

void sde_encoder_register_vblank_callback(struct drm_encoder *drm_enc,
		void (*vbl_cb)(void *), void *vbl_data)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	unsigned long lock_flags;
	bool enable;
	int i;

	enable = vbl_cb ? true : false;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	SDE_DEBUG_ENC(sde_enc, "\n");
	SDE_EVT32(DRMID(drm_enc), enable);

	spin_lock_irqsave(&sde_enc->enc_spinlock, lock_flags);
	sde_enc->crtc_vblank_cb = vbl_cb;
	sde_enc->crtc_vblank_cb_data = vbl_data;
	spin_unlock_irqrestore(&sde_enc->enc_spinlock, lock_flags);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.control_vblank_irq)
			phys->ops.control_vblank_irq(phys, enable);
	}
}

void sde_encoder_register_frame_event_callback(struct drm_encoder *drm_enc,
		void (*frame_event_cb)(void *, u32 event),
		void *frame_event_cb_data)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	unsigned long lock_flags;
	bool enable;

	enable = frame_event_cb ? true : false;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	SDE_DEBUG_ENC(sde_enc, "\n");
	SDE_EVT32(DRMID(drm_enc), enable, 0);

	spin_lock_irqsave(&sde_enc->enc_spinlock, lock_flags);
	sde_enc->crtc_frame_event_cb = frame_event_cb;
	sde_enc->crtc_frame_event_cb_data = frame_event_cb_data;
	spin_unlock_irqrestore(&sde_enc->enc_spinlock, lock_flags);
}

void sde_encoder_register_request_flip_callback(struct drm_encoder *drm_enc,
		void (*request_flip_cb)(void *),
		void *request_flip_cb_data)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	unsigned long lock_flags;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	spin_lock_irqsave(&sde_enc->enc_spinlock, lock_flags);
	sde_enc->crtc_request_flip_cb = request_flip_cb;
	sde_enc->crtc_request_flip_cb_data = request_flip_cb_data;
	spin_unlock_irqrestore(&sde_enc->enc_spinlock, lock_flags);
}

static void sde_encoder_frame_done_callback(
		struct drm_encoder *drm_enc,
		struct sde_encoder_phys *ready_phys, u32 event)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	unsigned int i;

	/* One of the physical encoders has become idle */
	for (i = 0; i < sde_enc->num_phys_encs; i++)
		if (sde_enc->phys_encs[i] == ready_phys) {
			clear_bit(i, sde_enc->frame_busy_mask);
			sde_enc->crtc_frame_event |= event;
			SDE_EVT32(DRMID(drm_enc), i,
					sde_enc->frame_busy_mask[0]);
		}

	if (!sde_enc->frame_busy_mask[0]) {
		atomic_set(&sde_enc->frame_done_timeout, 0);
		del_timer(&sde_enc->frame_done_timer);

		if (sde_enc->crtc_frame_event_cb)
			sde_enc->crtc_frame_event_cb(
					sde_enc->crtc_frame_event_cb_data,
					sde_enc->crtc_frame_event);
	}
}

/**
 * _sde_encoder_trigger_flush - trigger flush for a physical encoder
 * drm_enc: Pointer to drm encoder structure
 * phys: Pointer to physical encoder structure
 * extra_flush_bits: Additional bit mask to include in flush trigger
 */
static inline void _sde_encoder_trigger_flush(struct drm_encoder *drm_enc,
		struct sde_encoder_phys *phys, uint32_t extra_flush_bits)
{
	struct sde_hw_ctl *ctl;
	int pending_kickoff_cnt;

	if (!drm_enc || !phys) {
		SDE_ERROR("invalid argument(s), drm_enc %d, phys_enc %d\n",
				drm_enc != 0, phys != 0);
		return;
	}

	ctl = phys->hw_ctl;
	if (!ctl || !ctl->ops.trigger_flush) {
		SDE_ERROR("missing trigger cb\n");
		return;
	}

	pending_kickoff_cnt = sde_encoder_phys_inc_pending(phys);
	SDE_EVT32(DRMID(&to_sde_encoder_virt(drm_enc)->base),
			phys->intf_idx, pending_kickoff_cnt);

	if (extra_flush_bits && ctl->ops.update_pending_flush)
		ctl->ops.update_pending_flush(ctl, extra_flush_bits);

	ctl->ops.trigger_flush(ctl);
	SDE_EVT32(DRMID(drm_enc), ctl->idx);
}

/**
 * _sde_encoder_trigger_start - trigger start for a physical encoder
 * phys: Pointer to physical encoder structure
 */
static inline void _sde_encoder_trigger_start(struct sde_encoder_phys *phys)
{
	if (!phys) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	if (phys->ops.trigger_start && phys->enable_state != SDE_ENC_DISABLED)
		phys->ops.trigger_start(phys);
}

void sde_encoder_helper_trigger_start(struct sde_encoder_phys *phys_enc)
{
	struct sde_hw_ctl *ctl;
	int ctl_idx = -1;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	ctl = phys_enc->hw_ctl;
	if (ctl && ctl->ops.trigger_start) {
		ctl->ops.trigger_start(ctl);
		ctl_idx = ctl->idx;
	}

	if (phys_enc && phys_enc->parent)
		SDE_EVT32(DRMID(phys_enc->parent), ctl_idx);
}

int sde_encoder_helper_wait_event_timeout(
		int32_t drm_id,
		int32_t hw_id,
		wait_queue_head_t *wq,
		atomic_t *cnt,
		s64 timeout_ms)
{
	int rc = 0;
	s64 expected_time = ktime_to_ms(ktime_get()) + timeout_ms;
	s64 jiffies = msecs_to_jiffies(timeout_ms);
	s64 time;

	do {
		rc = wait_event_timeout(*wq, atomic_read(cnt) == 0, jiffies);
		time = ktime_to_ms(ktime_get());

		SDE_EVT32(drm_id, hw_id, rc, time, expected_time,
				atomic_read(cnt));
	/* If we timed out, counter is valid and time is less, wait again */
	} while (atomic_read(cnt) && (rc == 0) && (time < expected_time));

	return rc;
}

/**
 * _sde_encoder_kickoff_phys - handle physical encoder kickoff
 *	Iterate through the physical encoders and perform consolidated flush
 *	and/or control start triggering as needed. This is done in the virtual
 *	encoder rather than the individual physical ones in order to handle
 *	use cases that require visibility into multiple physical encoders at
 *	a time.
 * sde_enc: Pointer to virtual encoder structure
 */
static void _sde_encoder_kickoff_phys(struct sde_encoder_virt *sde_enc)
{
	struct sde_hw_ctl *ctl;
	uint32_t i, pending_flush;
	unsigned long lock_flags;

	if (!sde_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	pending_flush = 0x0;
	sde_enc->crtc_frame_event = 0;

	/* update pending counts and trigger kickoff ctl flush atomically */
	spin_lock_irqsave(&sde_enc->enc_spinlock, lock_flags);

	/* don't perform flush/start operations for slave encoders */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys || phys->enable_state == SDE_ENC_DISABLED)
			continue;

		ctl = phys->hw_ctl;
		if (!ctl)
			continue;

		set_bit(i, sde_enc->frame_busy_mask);

		if (!phys->ops.needs_single_flush ||
				!phys->ops.needs_single_flush(phys))
			_sde_encoder_trigger_flush(&sde_enc->base, phys, 0x0);
		else if (ctl->ops.get_pending_flush)
			pending_flush |= ctl->ops.get_pending_flush(ctl);
	}

	/* for split flush, combine pending flush masks and send to master */
	if (pending_flush && sde_enc->cur_master) {
		_sde_encoder_trigger_flush(
				&sde_enc->base,
				sde_enc->cur_master,
				pending_flush);
	}

	/* HW flush has happened, request a flip complete event now */
	if (sde_enc->crtc_request_flip_cb)
		sde_enc->crtc_request_flip_cb(
		sde_enc->crtc_request_flip_cb_data);

	_sde_encoder_trigger_start(sde_enc->cur_master);

	spin_unlock_irqrestore(&sde_enc->enc_spinlock, lock_flags);
}

void sde_encoder_prepare_for_kickoff(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	struct drm_connector *conn_mas = NULL;
	unsigned int i;
	enum sde_csc_type conn_csc;
	struct drm_display_mode *mode;
	struct sde_hw_cdm *hw_cdm;
	int mode_is_yuv = 0;
	int rc;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);

	SDE_DEBUG_ENC(sde_enc, "\n");
	SDE_EVT32(DRMID(drm_enc));

	/* prepare for next kickoff, may include waiting on previous kickoff */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		if (phys && phys->ops.prepare_for_kickoff)
			phys->ops.prepare_for_kickoff(phys);
	}

	if (sde_enc->cur_master && sde_enc->cur_master->connector) {
		conn_mas = sde_enc->cur_master->connector;
		rc = sde_connector_pre_kickoff(conn_mas);
		if (rc)
			SDE_ERROR_ENC(sde_enc,
				"kickoff conn%d failed rc %d\n",
				conn_mas->base.id,
				rc);

		for (i = 0; i < sde_enc->num_phys_encs; i++) {
			phys = sde_enc->phys_encs[i];
			if (phys) {
				mode = &phys->cached_mode;
				mode_is_yuv = (mode->private_flags &
					MSM_MODE_FLAG_COLOR_FORMAT_YCBCR420);
			}
			/**
			 * Check the CSC matrix type to which the
			 * CDM CSC matrix should be updated to based
			 * on the connector HDR state
			 */
			conn_csc = sde_connector_get_csc_type(conn_mas);
			if (phys && mode_is_yuv) {
				if (phys->enc_cdm_csc != conn_csc) {
					hw_cdm = phys->hw_cdm;
					rc = hw_cdm->ops.setup_csc_data(hw_cdm,
					&sde_csc_10bit_convert[conn_csc]);

					if (rc)
						SDE_ERROR_ENC(sde_enc,
							"CSC setup failed rc %d\n",
							rc);
					SDE_DEBUG_ENC(sde_enc,
						"updating CSC %d to %d\n",
						phys->enc_cdm_csc,
						conn_csc);
					phys->enc_cdm_csc = conn_csc;

				}
			}
		}
	}
}

void sde_encoder_kickoff(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;
	unsigned int i;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	SDE_ATRACE_BEGIN("encoder_kickoff");
	sde_enc = to_sde_encoder_virt(drm_enc);

	SDE_DEBUG_ENC(sde_enc, "\n");

	atomic_set(&sde_enc->frame_done_timeout,
			SDE_ENCODER_FRAME_DONE_TIMEOUT * 1000 /
			drm_enc->crtc->state->adjusted_mode.vrefresh);
	mod_timer(&sde_enc->frame_done_timer, jiffies +
		((atomic_read(&sde_enc->frame_done_timeout) * HZ) / 1000));

	/* All phys encs are ready to go, trigger the kickoff */
	_sde_encoder_kickoff_phys(sde_enc);

	/* allow phys encs to handle any post-kickoff business */
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		phys = sde_enc->phys_encs[i];
		if (phys && phys->ops.handle_post_kickoff)
			phys->ops.handle_post_kickoff(phys);
	}
	SDE_ATRACE_END("encoder_kickoff");
}

static int _sde_encoder_status_show(struct seq_file *s, void *data)
{
	struct sde_encoder_virt *sde_enc;
	int i;

	if (!s || !s->private)
		return -EINVAL;

	sde_enc = s->private;

	mutex_lock(&sde_enc->enc_lock);
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys)
			continue;

		seq_printf(s, "intf:%d    vsync:%8d     underrun:%8d    ",
				phys->intf_idx - INTF_0,
				atomic_read(&phys->vsync_cnt),
				atomic_read(&phys->underrun_cnt));

		switch (phys->intf_mode) {
		case INTF_MODE_VIDEO:
			seq_puts(s, "mode: video\n");
			break;
		case INTF_MODE_CMD:
			seq_puts(s, "mode: command\n");
			break;
		case INTF_MODE_WB_BLOCK:
			seq_puts(s, "mode: wb block\n");
			break;
		case INTF_MODE_WB_LINE:
			seq_puts(s, "mode: wb line\n");
			break;
		default:
			seq_puts(s, "mode: ???\n");
			break;
		}
	}
	mutex_unlock(&sde_enc->enc_lock);

	return 0;
}

static int _sde_encoder_debugfs_status_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, _sde_encoder_status_show, inode->i_private);
}

static void _sde_set_misr_params(struct sde_encoder_phys *phys, u32 enable,
					u32 frame_count)
{
	int j;

	if (!phys->misr_map)
		return;

	phys->misr_map->enable = enable;

	if (frame_count <= SDE_CRC_BATCH_SIZE)
		phys->misr_map->frame_count = frame_count;
	else if (frame_count <= 0)
		phys->misr_map->frame_count = 0;
	else
		phys->misr_map->frame_count = SDE_CRC_BATCH_SIZE;

	if (!enable) {
		phys->misr_map->last_idx = 0;
		phys->misr_map->frame_count = 0;
		for (j = 0; j < SDE_CRC_BATCH_SIZE; j++)
			phys->misr_map->crc_value[j] = 0;
	}
}

static ssize_t _sde_encoder_misr_set(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_encoder_virt *sde_enc;
	struct drm_encoder *drm_enc;
	int i = 0;
	char buf[10];
	u32 enable, frame_count;

	drm_enc = file->private_data;
	sde_enc = to_sde_encoder_virt(drm_enc);

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0; /* end of string */

	if (sscanf(buf, "%u %u", &enable, &frame_count) != 2)
		return -EFAULT;

	mutex_lock(&sde_enc->enc_lock);
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (!phys || !phys->misr_map || !phys->ops.setup_misr)
			continue;

		_sde_set_misr_params(phys, enable, frame_count);
		phys->ops.setup_misr(phys, phys->misr_map);
	}
	mutex_unlock(&sde_enc->enc_lock);
	return count;
}

static ssize_t _sde_encoder_misr_read(
		struct file *file,
		char __user *buff, size_t count, loff_t *ppos)
{
	struct sde_encoder_virt *sde_enc;
	struct drm_encoder *drm_enc;
	int i = 0, j = 0, len = 0;
	char buf[512] = {'\0'};

	if (*ppos)
		return 0;

	drm_enc = file->private_data;
	sde_enc = to_sde_encoder_virt(drm_enc);

	mutex_lock(&sde_enc->enc_lock);
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];
		struct sde_misr_params *misr_map;

		if (!phys || !phys->misr_map)
			continue;

		misr_map = phys->misr_map;

		len += snprintf(buf+len, sizeof(buf), "INTF%d\n", i);
		for (j = 0; j < SDE_CRC_BATCH_SIZE; j++)
			len += snprintf(buf+len, sizeof(buf), "%x\n",
						misr_map->crc_value[j]);
	}

	if (len < 0 || len >= sizeof(buf))
		return 0;

	if ((count < sizeof(buf)) || copy_to_user(buff, buf, len))
		return -EFAULT;

	*ppos += len;   /* increase offset */
	mutex_unlock(&sde_enc->enc_lock);

	return len;
}

static void _sde_encoder_init_debugfs(struct drm_encoder *drm_enc,
	struct sde_encoder_virt *sde_enc, struct sde_kms *sde_kms)
{
	static const struct file_operations debugfs_status_fops = {
		.open =		_sde_encoder_debugfs_status_open,
		.read =		seq_read,
		.llseek =	seq_lseek,
		.release =	single_release,
	};

	static const struct file_operations debugfs_misr_fops = {
		.open = simple_open,
		.read = _sde_encoder_misr_read,
		.write = _sde_encoder_misr_set,
	};

	char name[SDE_NAME_SIZE];

	if (!drm_enc || !sde_enc || !sde_kms) {
		SDE_ERROR("invalid encoder or kms\n");
		return;
	}

	snprintf(name, SDE_NAME_SIZE, "encoder%u", drm_enc->base.id);

	/* create overall sub-directory for the encoder */
	sde_enc->debugfs_root = debugfs_create_dir(name,
					sde_debugfs_get_root(sde_kms));
	if (sde_enc->debugfs_root) {
		/* don't error check these */
		debugfs_create_file("status", S_IRUGO | S_IWUSR,
			sde_enc->debugfs_root, sde_enc, &debugfs_status_fops);

		debugfs_create_file("misr_data", S_IRUGO | S_IWUSR,
			sde_enc->debugfs_root, drm_enc, &debugfs_misr_fops);

	}
}

static int sde_encoder_virt_add_phys_encs(
		u32 display_caps,
		struct sde_encoder_virt *sde_enc,
		struct sde_enc_phys_init_params *params)
{
	struct sde_encoder_phys *enc = NULL;

	SDE_DEBUG_ENC(sde_enc, "\n");

	/*
	 * We may create up to NUM_PHYS_ENCODER_TYPES physical encoder types
	 * in this function, check up-front.
	 */
	if (sde_enc->num_phys_encs + NUM_PHYS_ENCODER_TYPES >=
			ARRAY_SIZE(sde_enc->phys_encs)) {
		SDE_ERROR_ENC(sde_enc, "too many physical encoders %d\n",
			  sde_enc->num_phys_encs);
		return -EINVAL;
	}

	if (display_caps & MSM_DISPLAY_CAP_VID_MODE) {
		enc = sde_encoder_phys_vid_init(params);

		if (IS_ERR_OR_NULL(enc)) {
			SDE_ERROR_ENC(sde_enc, "failed to init vid enc: %ld\n",
				PTR_ERR(enc));
			return enc == 0 ? -EINVAL : PTR_ERR(enc);
		}

		sde_enc->phys_encs[sde_enc->num_phys_encs] = enc;
		++sde_enc->num_phys_encs;
	}

	if (display_caps & MSM_DISPLAY_CAP_CMD_MODE) {
		enc = sde_encoder_phys_cmd_init(params);

		if (IS_ERR_OR_NULL(enc)) {
			SDE_ERROR_ENC(sde_enc, "failed to init cmd enc: %ld\n",
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

	if (!sde_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	SDE_DEBUG_ENC(sde_enc, "\n");

	if (sde_enc->num_phys_encs + 1 >= ARRAY_SIZE(sde_enc->phys_encs)) {
		SDE_ERROR_ENC(sde_enc, "too many physical encoders %d\n",
			  sde_enc->num_phys_encs);
		return -EINVAL;
	}

	enc = sde_encoder_phys_wb_init(params);

	if (IS_ERR_OR_NULL(enc)) {
		SDE_ERROR_ENC(sde_enc, "failed to init wb enc: %ld\n",
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
		sde_encoder_underrun_callback,
		sde_encoder_frame_done_callback,
	};
	struct sde_enc_phys_init_params phys_params;

	if (!sde_enc || !sde_kms) {
		SDE_ERROR("invalid arg(s), enc %d kms %d\n",
				sde_enc != 0, sde_kms != 0);
		return -EINVAL;
	}

	memset(&phys_params, 0, sizeof(phys_params));
	phys_params.sde_kms = sde_kms;
	phys_params.parent = &sde_enc->base;
	phys_params.parent_ops = parent_ops;
	phys_params.enc_spinlock = &sde_enc->enc_spinlock;

	SDE_DEBUG("\n");

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
		SDE_ERROR_ENC(sde_enc, "unsupported display interface type\n");
		return -EINVAL;
	}

	WARN_ON(disp_info->num_of_h_tiles < 1);

	sde_enc->display_num_of_h_tiles = disp_info->num_of_h_tiles;

	SDE_DEBUG("dsi_info->num_of_h_tiles %d\n", disp_info->num_of_h_tiles);

	mutex_lock(&sde_enc->enc_lock);
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

		SDE_DEBUG("h_tile_instance %d = %d, split_role %d\n",
				i, controller_id, phys_params.split_role);

		if (intf_type == INTF_WB) {
			phys_params.intf_idx = INTF_MAX;
			phys_params.wb_idx = sde_encoder_get_wb(
					sde_kms->catalog,
					intf_type, controller_id);
			if (phys_params.wb_idx == WB_MAX) {
				SDE_ERROR_ENC(sde_enc,
					"could not get wb: type %d, id %d\n",
					intf_type, controller_id);
				ret = -EINVAL;
			}
		} else {
			phys_params.wb_idx = WB_MAX;
			phys_params.intf_idx = sde_encoder_get_intf(
					sde_kms->catalog, intf_type,
					controller_id);
			if (phys_params.intf_idx == INTF_MAX) {
				SDE_ERROR_ENC(sde_enc,
					"could not get wb: type %d, id %d\n",
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
				SDE_ERROR_ENC(sde_enc,
						"failed to add phys encs\n");
		}
	}
	mutex_unlock(&sde_enc->enc_lock);


	return ret;
}

static void sde_encoder_frame_done_timeout(unsigned long data)
{
	struct drm_encoder *drm_enc = (struct drm_encoder *) data;
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	struct msm_drm_private *priv;

	if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private) {
		SDE_ERROR("invalid parameters\n");
		return;
	}
	priv = drm_enc->dev->dev_private;

	if (!sde_enc->frame_busy_mask[0] || !sde_enc->crtc_frame_event_cb) {
		SDE_DEBUG("enc%d invalid timeout\n", drm_enc->base.id);
		SDE_EVT32(DRMID(drm_enc),
				sde_enc->frame_busy_mask[0], 0);
		return;
	} else if (!atomic_xchg(&sde_enc->frame_done_timeout, 0)) {
		SDE_ERROR("enc%d invalid timeout\n", drm_enc->base.id);
		SDE_EVT32(DRMID(drm_enc), 0, 1);
		return;
	}

	SDE_EVT32(DRMID(drm_enc), 0, 2);
	sde_enc->crtc_frame_event_cb(sde_enc->crtc_frame_event_cb_data,
			SDE_ENCODER_FRAME_EVENT_ERROR);
}

struct drm_encoder *sde_encoder_init(
		struct drm_device *dev,
		struct msm_display_info *disp_info)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct sde_kms *sde_kms = to_sde_kms(priv->kms);
	struct drm_encoder *drm_enc = NULL;
	struct sde_encoder_virt *sde_enc = NULL;
	int drm_enc_mode = DRM_MODE_ENCODER_NONE;
	int ret = 0;

	sde_enc = kzalloc(sizeof(*sde_enc), GFP_KERNEL);
	if (!sde_enc) {
		ret = -ENOMEM;
		goto fail;
	}

	mutex_init(&sde_enc->enc_lock);
	ret = sde_encoder_setup_display(sde_enc, sde_kms, disp_info,
			&drm_enc_mode);
	if (ret)
		goto fail;

	sde_enc->cur_master = NULL;
	spin_lock_init(&sde_enc->enc_spinlock);
	drm_enc = &sde_enc->base;
	drm_encoder_init(dev, drm_enc, &sde_encoder_funcs, drm_enc_mode);
	drm_encoder_helper_add(drm_enc, &sde_encoder_helper_funcs);

	atomic_set(&sde_enc->frame_done_timeout, 0);
	setup_timer(&sde_enc->frame_done_timer, sde_encoder_frame_done_timeout,
			(unsigned long) sde_enc);

	_sde_encoder_init_debugfs(drm_enc, sde_enc, sde_kms);

	SDE_DEBUG_ENC(sde_enc, "created\n");

	return drm_enc;

fail:
	SDE_ERROR("failed to create encoder\n");
	if (drm_enc)
		sde_encoder_destroy(drm_enc);

	return ERR_PTR(ret);
}

int sde_encoder_wait_for_commit_done(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i, ret = 0;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}
	sde_enc = to_sde_encoder_virt(drm_enc);
	SDE_DEBUG_ENC(sde_enc, "\n");

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->ops.wait_for_commit_done) {
			ret = phys->ops.wait_for_commit_done(phys);
			if (ret)
				return ret;
		}

		if (phys && phys->ops.collect_misr)
			if (phys->misr_map && phys->misr_map->enable)
				phys->ops.collect_misr(phys, phys->misr_map);
	}

	return ret;
}

enum sde_intf_mode sde_encoder_get_intf_mode(struct drm_encoder *encoder)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i;

	if (!encoder) {
		SDE_ERROR("invalid encoder\n");
		return INTF_MODE_NONE;
	}
	sde_enc = to_sde_encoder_virt(encoder);

	if (sde_enc->cur_master)
		return sde_enc->cur_master->intf_mode;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys)
			return phys->intf_mode;
	}

	return INTF_MODE_NONE;
}

/**
 * sde_encoder_phys_setup_cdm - setup chroma down block
 * @phys_enc:	Pointer to physical encoder
 * @output_type: HDMI/WB
 * @format:	Output format
 * @roi: Output size
 */
void sde_encoder_phys_setup_cdm(struct sde_encoder_phys *phys_enc,
		const struct sde_format *format, u32 output_type,
		struct sde_rect *roi)
{
	struct drm_encoder *encoder = phys_enc->parent;
	struct sde_encoder_virt *sde_enc = NULL;
	struct sde_hw_cdm *hw_cdm = phys_enc->hw_cdm;
	struct sde_hw_cdm_cfg *cdm_cfg = &phys_enc->cdm_cfg;
	struct drm_connector *connector = phys_enc->connector;
	int ret;
	u32 csc_type = 0;

	if (!encoder) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	sde_enc = to_sde_encoder_virt(encoder);

	if (!SDE_FORMAT_IS_YUV(format)) {
		SDE_DEBUG_ENC(sde_enc, "[cdm_disable fmt:%x]\n",
				format->base.pixel_format);

		if (hw_cdm && hw_cdm->ops.disable)
			hw_cdm->ops.disable(hw_cdm);

		return;
	}

	memset(cdm_cfg, 0, sizeof(struct sde_hw_cdm_cfg));

	cdm_cfg->output_width = roi->w;
	cdm_cfg->output_height = roi->h;
	cdm_cfg->output_fmt = format;
	cdm_cfg->output_type = output_type;
	cdm_cfg->output_bit_depth = SDE_FORMAT_IS_DX(format) ?
		CDM_CDWN_OUTPUT_10BIT : CDM_CDWN_OUTPUT_8BIT;

	/* enable 10 bit logic */
	switch (cdm_cfg->output_fmt->chroma_sample) {
	case SDE_CHROMA_RGB:
		cdm_cfg->h_cdwn_type = CDM_CDWN_DISABLE;
		cdm_cfg->v_cdwn_type = CDM_CDWN_DISABLE;
		break;
	case SDE_CHROMA_H2V1:
		cdm_cfg->h_cdwn_type = CDM_CDWN_COSITE;
		cdm_cfg->v_cdwn_type = CDM_CDWN_DISABLE;
		break;
	case SDE_CHROMA_420:
		cdm_cfg->h_cdwn_type = CDM_CDWN_COSITE;
		cdm_cfg->v_cdwn_type = CDM_CDWN_OFFSITE;
		break;
	case SDE_CHROMA_H1V2:
	default:
		SDE_ERROR("unsupported chroma sampling type\n");
		cdm_cfg->h_cdwn_type = CDM_CDWN_DISABLE;
		cdm_cfg->v_cdwn_type = CDM_CDWN_DISABLE;
		break;
	}

	SDE_DEBUG_ENC(sde_enc, "[cdm_enable:%d,%d,%X,%d,%d,%d,%d]\n",
			cdm_cfg->output_width,
			cdm_cfg->output_height,
			cdm_cfg->output_fmt->base.pixel_format,
			cdm_cfg->output_type,
			cdm_cfg->output_bit_depth,
			cdm_cfg->h_cdwn_type,
			cdm_cfg->v_cdwn_type);

	/**
	 * Choose CSC matrix based on following rules:
	 * 1. If connector supports quantization select,
	 *	  pick Full-Range for better quality.
	 * 2. If non-CEA mode, then pick Full-Range as per CEA spec
	 * 3. Otherwise, pick Limited-Range as all other CEA modes
	 *    need a limited range
	 */

	if (output_type == CDM_CDWN_OUTPUT_HDMI) {
		if (connector && connector->yuv_qs)
			csc_type = SDE_CSC_RGB2YUV_601FR;
		else if (connector &&
			sde_connector_mode_needs_full_range(connector))
			csc_type = SDE_CSC_RGB2YUV_601FR;
		else
			csc_type = SDE_CSC_RGB2YUV_601L;
	} else if (output_type == CDM_CDWN_OUTPUT_WB) {
		csc_type = SDE_CSC_RGB2YUV_601L;
	}

	if (hw_cdm && hw_cdm->ops.setup_csc_data) {
		ret = hw_cdm->ops.setup_csc_data(hw_cdm,
				&sde_csc_10bit_convert[csc_type]);
		if (ret < 0) {
			SDE_ERROR("failed to setup CSC %d\n", ret);
			return;
		}
	}

	/* Cache the CSC default matrix type */
	phys_enc->enc_cdm_csc = csc_type;

	if (hw_cdm && hw_cdm->ops.setup_cdwn) {
		ret = hw_cdm->ops.setup_cdwn(hw_cdm, cdm_cfg);
		if (ret < 0) {
			SDE_ERROR("failed to setup CDM %d\n", ret);
			return;
		}
	}

	if (hw_cdm && hw_cdm->ops.enable) {
		ret = hw_cdm->ops.enable(hw_cdm, cdm_cfg);
		if (ret < 0) {
			SDE_ERROR("failed to enable CDM %d\n", ret);
			return;
		}
	}
}
