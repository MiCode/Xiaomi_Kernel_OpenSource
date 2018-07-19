/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm-dp-mst]: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_dp_mst_helper.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "sde_connector.h"
#include "dp_drm.h"

#define DP_MST_DEBUG(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

#define MAX_DP_MST_STREAMS		2
#define MAX_DP_MST_DRM_ENCODERS		2
#define MAX_DP_MST_DRM_BRIDGES		2
#define DP_MST_SIM_MAX_PORTS    2

struct dp_drm_mst_fw_helper_ops {
	int (*calc_pbn_mode)(int clock, int bpp);
	int (*find_vcpi_slots)(struct drm_dp_mst_topology_mgr *mgr, int pbn);
	bool (*allocate_vcpi)(struct drm_dp_mst_topology_mgr *mgr,
			      struct drm_dp_mst_port *port,
			      int pbn, int slots);
	int (*update_payload_part1)(struct drm_dp_mst_topology_mgr *mgr);
	int (*check_act_status)(struct drm_dp_mst_topology_mgr *mgr);
	int (*update_payload_part2)(struct drm_dp_mst_topology_mgr *mgr);
	enum drm_connector_status (*detect_port)(
		struct drm_connector *connector,
		struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port);
	struct edid *(*get_edid)(struct drm_connector *connector,
		struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port);
	int (*topology_mgr_set_mst)(struct drm_dp_mst_topology_mgr *mgr,
		bool mst_state);
	int (*atomic_release_vcpi_slots)(struct drm_atomic_state *state,
				     struct drm_dp_mst_topology_mgr *mgr,
				     int slots);
	int (*get_ch_start_slot)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port, int channel_id);
	void (*reset_vcpi_slots)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port);
	void (*deallocate_vcpi)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port);
};

struct dp_mst_sim_port {
	struct drm_dp_mst_port port;
	int port_id;
	int slots;
	int start_slot;
	struct drm_connector *connector;
	struct edid *edid;
};

struct dp_mst_sim_mode {
	bool mst_state;
	struct dp_mst_sim_port sim_port[DP_MST_SIM_MAX_PORTS];
	struct edid *edid;
	struct work_struct probe_work;
	struct work_struct conn_destroy_work;
	const struct drm_dp_mst_topology_cbs *cbs;
};

struct dp_mst_bridge {
	struct drm_bridge base;
	u32 id;

	bool in_use;

	struct dp_display *display;
	struct drm_encoder *encoder;
	bool encoder_active_sts;

	struct drm_display_mode drm_mode;
	struct dp_display_mode dp_mode;
	struct drm_connector *connector;
	void *dp_panel;

	int pbn;
	int slots;
	int start_slot;
};

struct dp_mst_private {
	bool mst_initialized;
	struct dp_mst_caps caps;
	struct drm_dp_mst_topology_mgr mst_mgr;
	struct dp_mst_bridge mst_bridge[MAX_DP_MST_DRM_BRIDGES];
	struct dp_display *dp_display;
	const struct dp_drm_mst_fw_helper_ops *mst_fw_cbs;
	struct dp_mst_sim_mode simulator;
};

#define to_dp_mst_bridge(x)     container_of((x), struct dp_mst_bridge, base)

struct dp_mst_private dp_mst;

/* DRM DP MST Framework simulator OPs */

static void drm_dp_mst_sim_link_probe_work(struct work_struct *work)
{
	struct dp_mst_sim_mode *sim;
	struct dp_mst_private *mst;
	u8 cnt;
	char pathprop[] = "0.1";

	DP_MST_DEBUG("enter\n");

	sim = container_of(work, struct dp_mst_sim_mode, probe_work);
	mst = container_of(sim, struct dp_mst_private, simulator);

	for (cnt = 0; cnt < DP_MST_SIM_MAX_PORTS; cnt++) {
		sim->sim_port[cnt].port_id = cnt;

		sim->sim_port[cnt].connector = sim->cbs->add_connector(
				&mst->mst_mgr,
				&sim->sim_port[cnt].port,
				pathprop);

		sim->cbs->register_connector(sim->sim_port[cnt].connector);
	}
	DP_MST_DEBUG("completed\n");
}

static void drm_dp_mst_sim_conn_destroy_work(struct work_struct *work)
{
	struct dp_mst_sim_mode *sim;
	struct dp_mst_private *mst;
	u8 cnt;

	DP_MST_DEBUG("enter\n");

	sim = container_of(work, struct dp_mst_sim_mode, conn_destroy_work);
	mst = container_of(sim, struct dp_mst_private, simulator);

	for (cnt = 0; cnt < DP_MST_SIM_MAX_PORTS; cnt++) {
		sim->cbs->destroy_connector(&mst->mst_mgr,
				sim->sim_port[cnt].connector);
	}

	DP_MST_DEBUG("completed\n");
}

static int drm_dp_sim_find_vcpi_slots(struct drm_dp_mst_topology_mgr *mgr,
			   int pbn)
{
	return 7;
}

static bool drm_dp_sim_mst_allocate_vcpi(struct drm_dp_mst_topology_mgr *mgr,
			      struct drm_dp_mst_port *port, int pbn, int slots)
{
	return true;
}

static int drm_dp_sim_atomic_release_vcpi_slots(struct drm_atomic_state *state,
				     struct drm_dp_mst_topology_mgr *mgr,
				     int slots)
{
	return 0;
}

static int drm_dp_sim_update_payload_part1(struct drm_dp_mst_topology_mgr *mgr)
{
	return 0;
}

static int drm_dp_sim_check_act_status(struct drm_dp_mst_topology_mgr *mgr)
{
	return 0;
}

static int drm_dp_sim_update_payload_part2(struct drm_dp_mst_topology_mgr *mgr)
{
	return 0;
}

static void drm_dp_sim_reset_vcpi_slots(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port)
{
}

static void drm_dp_sim_deallocate_vcpi(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port)
{
}

static enum drm_connector_status drm_dp_sim_mst_detect_port(
		struct drm_connector *connector,
		struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port)
{
	return connector_status_connected;
}

static struct edid *drm_dp_sim_mst_get_edid(struct drm_connector *connector,
		struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port)
{
	struct dp_mst_private *mst = container_of(mgr,
			struct dp_mst_private, mst_mgr);
	struct dp_mst_sim_port *sim_port = container_of(port,
						struct dp_mst_sim_port, port);

	sim_port->edid = drm_edid_duplicate(mst->simulator.edid);

	return sim_port->edid;
}

static int drm_dp_sim_mst_topology_mgr_set_mst(
		struct drm_dp_mst_topology_mgr *mgr,
		bool mst_state)
{
	struct dp_mst_private *mst = container_of(mgr,
			struct dp_mst_private, mst_mgr);

	if (mst->simulator.mst_state == mst_state)
		return 0;

	if (mst_state)
		queue_work(system_long_wq, &mst->simulator.probe_work);
	else
		queue_work(system_long_wq, &mst->simulator.conn_destroy_work);

	mst->simulator.mst_state = mst_state;

	return 0;
}

static int _drm_dp_sim_mst_get_ch_start_slot(
		struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port, int channel_id)
{
	struct dp_mst_sim_port *sim_port = container_of(port,
			struct dp_mst_sim_port, port);

	if (sim_port->port_id == 0)
		return 0;

	if (sim_port->port_id == 1)
		return 7;

	return 0;
}

static int _drm_dp_mst_get_ch_start_slot(struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port, int channel_id)
{
	return mgr->payloads[channel_id-1].start_slot;
}

static const struct dp_drm_mst_fw_helper_ops drm_dp_mst_fw_helper_ops = {
	.calc_pbn_mode             = drm_dp_calc_pbn_mode,
	.find_vcpi_slots           = drm_dp_find_vcpi_slots,
	.allocate_vcpi             = drm_dp_mst_allocate_vcpi,
	.update_payload_part1      = drm_dp_update_payload_part1,
	.check_act_status          = drm_dp_check_act_status,
	.update_payload_part2      = drm_dp_update_payload_part2,
	.detect_port               = drm_dp_mst_detect_port,
	.get_edid                  = drm_dp_mst_get_edid,
	.topology_mgr_set_mst      = drm_dp_mst_topology_mgr_set_mst,
	.get_ch_start_slot         = _drm_dp_mst_get_ch_start_slot,
	.atomic_release_vcpi_slots = drm_dp_atomic_release_vcpi_slots,
	.reset_vcpi_slots          = drm_dp_mst_reset_vcpi_slots,
	.deallocate_vcpi           = drm_dp_mst_deallocate_vcpi,
};

static const struct dp_drm_mst_fw_helper_ops drm_dp_sim_mst_fw_helper_ops = {
	.calc_pbn_mode             = drm_dp_calc_pbn_mode,
	.find_vcpi_slots           = drm_dp_sim_find_vcpi_slots,
	.allocate_vcpi             = drm_dp_sim_mst_allocate_vcpi,
	.update_payload_part1      = drm_dp_sim_update_payload_part1,
	.check_act_status          = drm_dp_sim_check_act_status,
	.update_payload_part2      = drm_dp_sim_update_payload_part2,
	.detect_port               = drm_dp_sim_mst_detect_port,
	.get_edid                  = drm_dp_sim_mst_get_edid,
	.topology_mgr_set_mst      = drm_dp_sim_mst_topology_mgr_set_mst,
	.get_ch_start_slot         = _drm_dp_sim_mst_get_ch_start_slot,
	.atomic_release_vcpi_slots = drm_dp_sim_atomic_release_vcpi_slots,
	.reset_vcpi_slots          = drm_dp_sim_reset_vcpi_slots,
	.deallocate_vcpi           = drm_dp_sim_deallocate_vcpi,
};

/* DP MST Bridge OPs */

static int dp_mst_bridge_attach(struct drm_bridge *dp_bridge)
{
	struct dp_mst_bridge *bridge;

	DP_MST_DEBUG("enter\n");

	if (!dp_bridge) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	bridge = to_dp_mst_bridge(dp_bridge);

	DP_MST_DEBUG("mst bridge [%d] attached\n", bridge->id);

	return 0;
}

static bool dp_mst_bridge_mode_fixup(struct drm_bridge *drm_bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	bool ret = true;
	struct dp_display_mode dp_mode;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;

	DP_MST_DEBUG("enter\n");

	if (!drm_bridge || !mode || !adjusted_mode) {
		pr_err("Invalid params\n");
		ret = false;
		goto end;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		ret = false;
		goto end;
	}

	if (!bridge->dp_panel) {
		pr_err("Invalid dp_panel\n");
		ret = false;
		goto end;
	}

	dp = bridge->display;

	convert_to_dp_mode(mode, &dp_mode, dp);
	convert_to_drm_mode(&dp_mode, adjusted_mode);

	DP_MST_DEBUG("mst bridge [%d] mode:%s fixup\n", bridge->id, mode->name);
end:
	return ret;
}

static bool _dp_mst_compute_config(struct dp_mst_bridge *dp_bridge)
{
	struct dp_display *dp_display = dp_bridge->display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	int bpp, slots = 0, mst_pbn;

	DP_MST_DEBUG("enter\n");

	bpp = dp_bridge->dp_mode.timing.bpp;

	mst_pbn = mst->mst_fw_cbs->calc_pbn_mode(
			dp_bridge->drm_mode.crtc_clock, bpp);
	dp_bridge->pbn = mst_pbn;

	slots = mst->mst_fw_cbs->find_vcpi_slots(&mst->mst_mgr, mst_pbn);
	if (slots < 0) {
		pr_err("mst: failed finding vcpi slots:%d\n", slots);
		return false;
	}

	dp_bridge->slots = slots;

	DP_MST_DEBUG("mst bridge [%d] pbn: %d slots: %d\n", dp_bridge->id,
			mst_pbn, slots);

	return true;
}

static void _dp_mst_bridge_pre_enable_part1(struct dp_mst_bridge *dp_bridge)
{
	struct dp_display *dp_display = dp_bridge->display;
	struct sde_connector *c_conn =
		to_sde_connector(dp_bridge->connector);
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct drm_dp_mst_port *port = c_conn->mst_port;
	u32 ch;
	bool ret;

	DP_MST_DEBUG("enter\n");

	ret = mst->mst_fw_cbs->allocate_vcpi(&mst->mst_mgr,
				       port,
				       dp_bridge->pbn,
				       dp_bridge->slots);
	if (ret == false) {
		pr_err("mst: failed to allocate vcpi. bridge:%d\n",
				dp_bridge->id);
		return;
	}

	ret = mst->mst_fw_cbs->update_payload_part1(&mst->mst_mgr);

	ch = port->vcpi.vcpi;
	dp_bridge->start_slot = mst->mst_fw_cbs->get_ch_start_slot(
					&mst->mst_mgr,
					port, ch);

	DP_MST_DEBUG("ch %d start-slot %d tot-slots %d, bridge:%d\n", ch,
		dp_bridge->start_slot,
		dp_bridge->slots,
		dp_bridge->id);

	DP_MST_DEBUG("mst bridge [%d] _pre enable part-1 complete\n",
			dp_bridge->id);
}

static void _dp_mst_bridge_pre_enable_part2(struct dp_mst_bridge *dp_bridge)
{
	struct dp_display *dp_display = dp_bridge->display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;

	DP_MST_DEBUG("enter\n");

	mst->mst_fw_cbs->check_act_status(&mst->mst_mgr);

	mst->mst_fw_cbs->update_payload_part2(&mst->mst_mgr);

	DP_MST_DEBUG("mst bridge [%d] _pre enable part-2 complete\n",
			dp_bridge->id);
}

static void _dp_mst_bridge_pre_disable_part1(struct dp_mst_bridge *dp_bridge)
{
	struct dp_display *dp_display = dp_bridge->display;
	struct sde_connector *c_conn =
		to_sde_connector(dp_bridge->connector);
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct drm_dp_mst_port *port = c_conn->mst_port;

	DP_MST_DEBUG("enter\n");

	mst->mst_fw_cbs->reset_vcpi_slots(&mst->mst_mgr, port);

	mst->mst_fw_cbs->update_payload_part1(&mst->mst_mgr);

	DP_MST_DEBUG("mst bridge [%d] _pre disable part-1 complete\n",
			dp_bridge->id);
}

static void _dp_mst_bridge_pre_disable_part2(struct dp_mst_bridge *dp_bridge)
{
	struct dp_display *dp_display = dp_bridge->display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct sde_connector *c_conn =
		to_sde_connector(dp_bridge->connector);
	struct drm_dp_mst_port *port = c_conn->mst_port;

	DP_MST_DEBUG("enter\n");

	mst->mst_fw_cbs->check_act_status(&mst->mst_mgr);

	mst->mst_fw_cbs->update_payload_part2(&mst->mst_mgr);

	mst->mst_fw_cbs->deallocate_vcpi(&mst->mst_mgr, port);

	DP_MST_DEBUG("mst bridge [%d] _pre disable part-2 complete\n",
			dp_bridge->id);
}

static void dp_mst_bridge_pre_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;

	DP_MST_DEBUG("enter\n");

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	dp = bridge->display;

	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		return;
	}

	/* By this point mode should have been validated through mode_fixup */
	rc = dp->set_mode(dp, bridge->dp_panel, &bridge->dp_mode);
	if (rc) {
		pr_err("[%d] failed to perform a mode set, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	rc = dp->prepare(dp, bridge->dp_panel);
	if (rc) {
		pr_err("[%d] DP display prepare failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	_dp_mst_compute_config(bridge);
	_dp_mst_bridge_pre_enable_part1(bridge);

	dp->set_stream_info(dp, bridge->dp_panel, bridge->id,
			bridge->start_slot, bridge->slots, bridge->pbn);

	rc = dp->enable(dp, bridge->dp_panel);
	if (rc) {
		pr_err("[%d] DP display enable failed, rc=%d\n",
		       bridge->id, rc);
		dp->unprepare(dp, bridge->dp_panel);
	} else {
		_dp_mst_bridge_pre_enable_part2(bridge);
	}

	DP_MST_DEBUG("mst bridge [%d] pre enable complete\n", bridge->id);
}

static void dp_mst_bridge_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;

	DP_MST_DEBUG("enter\n");

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		return;
	}

	dp = bridge->display;

	rc = dp->post_enable(dp, bridge->dp_panel);
	if (rc) {
		pr_err("mst bridge [%d] post enable failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	DP_MST_DEBUG("mst bridge [%d] post enable complete\n", bridge->id);
}

static void dp_mst_bridge_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;

	DP_MST_DEBUG("enter\n");

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		return;
	}

	dp = bridge->display;

	sde_connector_helper_bridge_disable(bridge->connector);

	_dp_mst_bridge_pre_disable_part1(bridge);

	rc = dp->pre_disable(dp, bridge->dp_panel);
	if (rc) {
		pr_err("[%d] DP display pre disable failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	_dp_mst_bridge_pre_disable_part2(bridge);

	DP_MST_DEBUG("mst bridge [%d] disable complete\n", bridge->id);
}

static void dp_mst_bridge_post_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;
	struct dp_mst_private *mst;

	DP_MST_DEBUG("enter\n");

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		return;
	}

	dp = bridge->display;
	mst = dp->dp_mst_prv_info;

	rc = dp->disable(dp, bridge->dp_panel);
	if (rc) {
		pr_err("[%d] DP display disable failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	rc = dp->unprepare(dp, bridge->dp_panel);
	if (rc) {
		pr_err("[%d] DP display unprepare failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	/* Disconnect the connector and panel info from bridge */
	mst->mst_bridge[bridge->id].connector = NULL;
	mst->mst_bridge[bridge->id].dp_panel = NULL;
	mst->mst_bridge[bridge->id].encoder_active_sts = false;

	DP_MST_DEBUG("mst bridge [%d] post disable complete\n", bridge->id);
}

static void dp_mst_bridge_mode_set(struct drm_bridge *drm_bridge,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;

	DP_MST_DEBUG("enter\n");

	if (!drm_bridge || !mode || !adjusted_mode) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		return;
	}

	if (!bridge->dp_panel) {
		pr_err("Invalid dp_panel\n");
		return;
	}

	dp = bridge->display;

	memset(&bridge->dp_mode, 0x0, sizeof(struct dp_display_mode));
	memcpy(&bridge->drm_mode, adjusted_mode, sizeof(bridge->drm_mode));
	convert_to_dp_mode(adjusted_mode, &bridge->dp_mode, dp);

	DP_MST_DEBUG("mst bridge [%d] mode set complete\n", bridge->id);
}

/* DP MST Bridge APIs */

static const struct drm_bridge_funcs dp_mst_bridge_ops = {
	.attach       = dp_mst_bridge_attach,
	.mode_fixup   = dp_mst_bridge_mode_fixup,
	.pre_enable   = dp_mst_bridge_pre_enable,
	.enable       = dp_mst_bridge_enable,
	.disable      = dp_mst_bridge_disable,
	.post_disable = dp_mst_bridge_post_disable,
	.mode_set     = dp_mst_bridge_mode_set,
};

int dp_mst_drm_bridge_init(void *data, struct drm_encoder *encoder)
{
	int rc = 0;
	struct dp_mst_bridge *bridge = NULL;
	struct drm_device *dev;
	struct dp_display *display = data;
	struct msm_drm_private *priv = NULL;
	struct dp_mst_private *mst = display->dp_mst_prv_info;
	int i;

	if (!mst || !mst->mst_initialized) {
		pr_err("mst not initialized\n");
		return -ENODEV;
	}

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (!mst->mst_bridge[i].in_use) {
			bridge = &mst->mst_bridge[i];
			bridge->encoder = encoder;
			bridge->in_use = true;
			bridge->id = i;
			break;
		}
	}

	if (i == MAX_DP_MST_DRM_BRIDGES) {
		pr_err("mst supports only %d bridges\n", i);
		rc = -EACCES;
		goto end;
	}

	dev = display->drm_dev;
	bridge->display = display;
	bridge->base.funcs = &dp_mst_bridge_ops;
	bridge->base.encoder = encoder;

	priv = dev->dev_private;

	rc = drm_bridge_attach(encoder, &bridge->base, NULL);
	if (rc) {
		pr_err("failed to attach bridge, rc=%d\n", rc);
		goto end;
	}

	encoder->bridge = &bridge->base;
	priv->bridges[priv->num_bridges++] = &bridge->base;

	DP_MST_DEBUG("mst drm bridge init. bridge id:%d\n", i);

	return 0;

end:
	return rc;
}

void dp_mst_drm_bridge_deinit(void *display)
{
	DP_MST_DEBUG("mst bridge deinit\n");
}

/* DP MST Connector OPs */

static enum drm_connector_status
dp_mst_connector_detect(struct drm_connector *connector, bool force,
		void *display)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display *dp_display = c_conn->display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	enum drm_connector_status status;

	status = mst->mst_fw_cbs->detect_port(connector,
			&mst->mst_mgr,
			c_conn->mst_port);

	DP_MST_DEBUG("mst connector:%d detect, status:%d\n",
			connector->base.id, status);

	return status;
}

static int dp_mst_connector_get_modes(struct drm_connector *connector,
		void *display)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct edid *edid;
	int rc = 0;

	edid = mst->mst_fw_cbs->get_edid(connector, &mst->mst_mgr,
			c_conn->mst_port);

	if (edid)
		rc = dp_display->mst_connector_update_edid(dp_display,
				connector, edid);

	DP_MST_DEBUG("mst connector get modes. id: %d\n", connector->base.id);

	return rc;
}

enum drm_mode_status dp_mst_connector_mode_valid(
		struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display)
{
	enum drm_mode_status status;

	status = dp_connector_mode_valid(connector, mode, display);

	DP_MST_DEBUG("mst connector:%d mode:%s valid status:%d\n",
			connector->base.id, mode->name, status);

	return status;
}

int dp_mst_connector_get_info(struct drm_connector *connector,
		struct msm_display_info *info,
		void *display)
{
	int rc;
	enum drm_connector_status status = connector_status_unknown;

	rc = dp_connector_get_info(connector, info, display);

	if (!rc) {
		status = dp_mst_connector_detect(connector, false, display);

		if (status == connector_status_connected)
			info->is_connected = true;
		else
			info->is_connected = false;
	}

	DP_MST_DEBUG("mst connector:%d get info:%d, rc:%d\n",
			connector->base.id, status, rc);

	return rc;
}

int dp_mst_connector_get_mode_info(struct drm_connector *connector,
		const struct drm_display_mode *drm_mode,
		struct msm_mode_info *mode_info,
		u32 max_mixer_width, void *display)
{
	int rc;

	rc = dp_connector_get_mode_info(connector, drm_mode, mode_info,
			max_mixer_width, display);

	DP_MST_DEBUG("mst connector:%d get mode info. rc:%d\n",
			connector->base.id, rc);

	return rc;
}

static struct drm_encoder *
dp_mst_atomic_best_encoder(struct drm_connector *connector,
			void *display, struct drm_connector_state *state)
{
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct sde_connector *conn = to_sde_connector(connector);
	struct drm_encoder *enc = NULL;
	u32 i;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (mst->mst_bridge[i].connector == connector) {
			enc = mst->mst_bridge[i].encoder;
			goto end;
		}
	}

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (!mst->mst_bridge[i].encoder_active_sts) {
			mst->mst_bridge[i].encoder_active_sts = true;
			mst->mst_bridge[i].connector = connector;
			mst->mst_bridge[i].dp_panel = conn->drv_panel;
			enc = mst->mst_bridge[i].encoder;
			break;
		}
	}

end:
	if (enc)
		DP_MST_DEBUG("mst connector:%d atomic best encoder:%d\n",
			connector->base.id, i);
	else
		DP_MST_DEBUG("mst connector:%d atomic best encoder failed\n",
				connector->base.id);

	return enc;
}

static struct dp_mst_bridge *_dp_mst_get_bridge_from_encoder(
		struct dp_display *dp_display,
		struct drm_encoder *encoder)
{
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	int i;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (mst->mst_bridge[i].encoder == encoder)
			return &mst->mst_bridge[i];
	}

	DP_MST_DEBUG("mst bridge detect for encoder failed\n");

	return NULL;
}

static int dp_mst_connector_atomic_check(struct drm_connector *connector,
		void *display, struct drm_connector_state *new_conn_state)
{
	int ret = 0;
	struct drm_atomic_state *state = new_conn_state->state;
	struct drm_connector_state *old_conn_state;
	struct drm_crtc *old_crtc;
	struct drm_crtc_state *crtc_state;
	int slots;
	struct dp_mst_bridge *bridge = NULL;
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;

	old_conn_state = drm_atomic_get_old_connector_state(state, connector);

	if (!old_conn_state)
		return ret;

	old_crtc = old_conn_state->crtc;
	if (!old_crtc)
		return ret;

	crtc_state = drm_atomic_get_new_crtc_state(state, old_crtc);

	bridge = _dp_mst_get_bridge_from_encoder(dp_display,
			old_conn_state->best_encoder);

	if (!bridge)
		return 0;

	slots = bridge->slots;
	if (drm_atomic_crtc_needs_modeset(crtc_state) && slots > 0) {
		ret = mst->mst_fw_cbs->atomic_release_vcpi_slots(state,
				&mst->mst_mgr, slots);
		if (ret)
			pr_err("failed releasing %d vcpi slots:%d\n",
					slots, ret);
	}

	DP_MST_DEBUG("mst connector:%d atomic check\n", connector->base.id);

	return ret;
}

static int dp_mst_connector_config_hdr(struct drm_connector *connector,
		void *display, struct sde_connector_state *c_state)
{
	int rc;

	rc = dp_connector_config_hdr(connector, display, c_state);

	DP_MST_DEBUG("mst connector:%d cfg hdr. rc:%d\n",
			connector->base.id, rc);

	return rc;
}

static void dp_mst_connector_pre_destroy(struct drm_connector *connector,
		void *display)
{
	struct dp_display *dp_display = display;

	DP_MST_DEBUG("enter:\n");
	dp_display->mst_connector_uninstall(dp_display, connector);
	DP_MST_DEBUG("exit:\n");
}

/* DRM MST callbacks */

static struct drm_connector *
dp_mst_add_connector(struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port, const char *pathprop)
{
	static const struct sde_connector_ops dp_mst_connector_ops = {
		.post_init  = NULL,
		.detect     = dp_mst_connector_detect,
		.get_modes  = dp_mst_connector_get_modes,
		.mode_valid = dp_mst_connector_mode_valid,
		.get_info   = dp_mst_connector_get_info,
		.get_mode_info  = dp_mst_connector_get_mode_info,
		.atomic_best_encoder = dp_mst_atomic_best_encoder,
		.atomic_check = dp_mst_connector_atomic_check,
		.config_hdr = dp_mst_connector_config_hdr,
		.pre_destroy = dp_mst_connector_pre_destroy,
	};
	struct dp_mst_private *dp_mst;
	struct drm_device *dev;
	struct dp_display *dp_display;
	struct drm_connector *connector;
	struct sde_connector *c_conn;
	int rc, i;

	DP_MST_DEBUG("enter\n");

	dp_mst = container_of(mgr, struct dp_mst_private, mst_mgr);

	dp_display = dp_mst->dp_display;
	dev = dp_display->drm_dev;

	connector = sde_connector_init(dev,
				dp_mst->mst_bridge[0].encoder,
				NULL,
				dp_display,
				&dp_mst_connector_ops,
				DRM_CONNECTOR_POLL_HPD,
				DRM_MODE_CONNECTOR_DisplayPort);

	if (!connector) {
		pr_err("mst sde_connector_init failed\n");
		return connector;
	}

	rc = dp_display->mst_connector_install(dp_display, connector);
	if (rc) {
		pr_err("mst connector install failed\n");
		sde_connector_destroy(connector);
		return NULL;
	}

	c_conn = to_sde_connector(connector);
	c_conn->mst_port = port;

	if (connector->funcs->reset)
		connector->funcs->reset(connector);

	for (i = 1; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		drm_mode_connector_attach_encoder(connector,
				dp_mst->mst_bridge[i].encoder);
	}

	drm_object_attach_property(&connector->base,
			dev->mode_config.path_property, 0);
	drm_object_attach_property(&connector->base,
			dev->mode_config.tile_property, 0);

	DP_MST_DEBUG("add mst connector:%d\n", connector->base.id);

	return connector;
}

static void dp_mst_register_connector(struct drm_connector *connector)
{
	DP_MST_DEBUG("enter\n");

	connector->status = connector->funcs->detect(connector, false);

	DP_MST_DEBUG("register mst connector:%d\n", connector->base.id);
	drm_connector_register(connector);
}

static void dp_mst_destroy_connector(struct drm_dp_mst_topology_mgr *mgr,
					   struct drm_connector *connector)
{
	DP_MST_DEBUG("enter\n");

	DP_MST_DEBUG("destroy mst connector:%d\n", connector->base.id);

	drm_connector_unregister(connector);
	drm_connector_unreference(connector);
}

static void dp_mst_hotplug(struct drm_dp_mst_topology_mgr *mgr)
{
	struct dp_mst_private *mst = container_of(mgr, struct dp_mst_private,
							mst_mgr);
	struct drm_device *dev = mst->dp_display->drm_dev;
	char event_string[] = "MST_HOTPLUG=1";
	char *envp[2];

	envp[0] = event_string;
	envp[1] = NULL;

	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE, envp);

	DP_MST_DEBUG("mst hot plug event\n");
}

/* DP Driver Callback OPs */

static void dp_mst_display_hpd(void *dp_display, bool hpd_status,
		struct dp_mst_hdp_info *info)
{
	int rc;
	struct dp_display *dp = dp_display;
	struct dp_mst_private *mst = dp->dp_mst_prv_info;

	if (!hpd_status)
		rc = mst->mst_fw_cbs->topology_mgr_set_mst(&mst->mst_mgr,
				hpd_status);

	if (info && !info->mst_protocol) {
		mst->simulator.edid = (struct edid *)info->edid;
		mst->mst_fw_cbs = &drm_dp_sim_mst_fw_helper_ops;
	} else {
		mst->mst_fw_cbs = &drm_dp_mst_fw_helper_ops;
	}

	if (hpd_status)
		rc = mst->mst_fw_cbs->topology_mgr_set_mst(&mst->mst_mgr,
				hpd_status);

	DP_MST_DEBUG("mst display hpd:%d, rc:%d\n", hpd_status, rc);
}

static void dp_mst_display_hpd_irq(void *dp_display)
{
	int rc;
	struct dp_display *dp = dp_display;
	struct dp_mst_private *mst = dp->dp_mst_prv_info;
	u8 esi[14], idx;
	unsigned int esi_res = DP_SINK_COUNT_ESI + 1;
	bool handled;

	rc = drm_dp_dpcd_read(mst->caps.drm_aux, DP_SINK_COUNT_ESI,
		esi, 14);
	if (rc != 14) {
		pr_err("dpcd sync status read failed, rlen=%d\n", rc);
		return;
	}

	for (idx = 0; idx < 14; idx++)
		DP_MST_DEBUG("mst irq: esi[%d]: 0x%x\n", idx, esi[idx]);

	rc = drm_dp_mst_hpd_irq(&mst->mst_mgr, esi, &handled);

	/* ack the request */
	if (handled) {
		rc = drm_dp_dpcd_write(mst->caps.drm_aux, esi_res, &esi[1], 3);

		if (rc != 3)
			pr_err("dpcd esi_res failed. rlen=%d\n", rc);
	}

	DP_MST_DEBUG("mst display hpd_irq handled:%d rc:%d\n", handled, rc);
}

/* DP MST APIs */

static const struct dp_mst_drm_cbs dp_mst_display_cbs = {
	.hpd = dp_mst_display_hpd,
	.hpd_irq = dp_mst_display_hpd_irq,
};

static const struct drm_dp_mst_topology_cbs dp_mst_drm_cbs = {
	.add_connector = dp_mst_add_connector,
	.register_connector = dp_mst_register_connector,
	.destroy_connector = dp_mst_destroy_connector,
	.hotplug = dp_mst_hotplug,
};

static void drm_dp_sim_mst_init(struct dp_mst_private *mst)
{
	INIT_WORK(&mst->simulator.probe_work, drm_dp_mst_sim_link_probe_work);
	INIT_WORK(&mst->simulator.conn_destroy_work,
			drm_dp_mst_sim_conn_destroy_work);
	mst->simulator.cbs = &dp_mst_drm_cbs;
}

int dp_mst_init(struct dp_display *dp_display)
{
	struct drm_device *dev;
	int conn_base_id = 0;
	int ret;
	struct dp_mst_drm_install_info install_info;

	memset(&dp_mst, 0, sizeof(dp_mst));

	if (!dp_display) {
		pr_err("invalid params\n");
		return 0;
	}

	dev = dp_display->drm_dev;

	/* register with DP driver */
	install_info.dp_mst_prv_info = &dp_mst;
	install_info.cbs = &dp_mst_display_cbs;
	dp_display->mst_install(dp_display, &install_info);

	dp_display->get_mst_caps(dp_display, &dp_mst.caps);

	if (!dp_mst.caps.has_mst) {
		DP_MST_DEBUG("mst not supported\n");
		return 0;
	}

	dp_mst.mst_fw_cbs = &drm_dp_mst_fw_helper_ops;

	memset(&dp_mst.mst_mgr, 0, sizeof(dp_mst.mst_mgr));
	dp_mst.mst_mgr.cbs = &dp_mst_drm_cbs;
	conn_base_id = dp_display->base_connector->base.id;
	dp_mst.dp_display = dp_display;

	ret = drm_dp_mst_topology_mgr_init(&dp_mst.mst_mgr, dev,
					dp_mst.caps.drm_aux,
					dp_mst.caps.max_dpcd_transaction_bytes,
					dp_mst.caps.max_streams_supported,
					conn_base_id);
	if (ret) {
		pr_err("dp drm mst topology manager init failed\n");
		return ret;
	}

	drm_dp_sim_mst_init(&dp_mst);

	dp_mst.mst_initialized = true;

	DP_MST_DEBUG("dp drm mst topology manager init completed\n");

	return ret;
}

void dp_mst_deinit(struct dp_display *dp_display)
{
	struct dp_mst_private *mst;

	if (!dp_display) {
		pr_err("invalid params\n");
		return;
	}

	mst = dp_display->dp_mst_prv_info;

	if (!mst->mst_initialized)
		return;

	dp_display->mst_uninstall(dp_display);

	drm_dp_mst_topology_mgr_destroy(&mst->mst_mgr);

	dp_mst.mst_initialized = false;

	DP_MST_DEBUG("dp drm mst topology manager deinit completed\n");
}

