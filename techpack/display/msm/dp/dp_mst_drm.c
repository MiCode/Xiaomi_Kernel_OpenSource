// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

/*
 * Copyright © 2014 Red Hat.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_dp_mst_helper.h>
#include <drm/drm_fixed.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "sde_connector.h"
#include "dp_drm.h"
#include "dp_debug.h"

#define DP_MST_DEBUG(fmt, ...) DP_DEBUG(fmt, ##__VA_ARGS__)
#define DP_MST_INFO_LOG(fmt, ...) DP_DEBUG(fmt, ##__VA_ARGS__)

#define MAX_DP_MST_DRM_ENCODERS		2
#define MAX_DP_MST_DRM_BRIDGES		2
#define HPD_STRING_SIZE			30

struct dp_drm_mst_fw_helper_ops {
	int (*calc_pbn_mode)(struct dp_display_mode *dp_mode);
	int (*find_vcpi_slots)(struct drm_dp_mst_topology_mgr *mgr, int pbn);
	int (*atomic_find_vcpi_slots)(struct drm_atomic_state *state,
				  struct drm_dp_mst_topology_mgr *mgr,
				  struct drm_dp_mst_port *port, int pbn);
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
	void (*get_vcpi_info)(struct drm_dp_mst_topology_mgr *mgr,
		int vcpi, int *start_slot, int *num_slots);
	void (*reset_vcpi_slots)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port);
	void (*deallocate_vcpi)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port);
};

struct dp_mst_sim_port_data {
	bool input_port;
	u8 peer_device_type;
	u8 port_number;
	bool mcs;
	bool ddps;
	bool legacy_device_plug_status;
	u8 dpcd_revision;
	u8 peer_guid[16];
	u8 num_sdp_streams;
	u8 num_sdp_stream_sinks;
};

struct dp_mst_sim_port_edid {
	u8 port_number;
	u8 edid[SZ_256];
	bool valid;
};

struct dp_mst_sim_mode {
	bool mst_state;
	struct edid *edid;
	struct dp_mst_sim_port_edid port_edids[DP_MST_SIM_MAX_PORTS];
	struct work_struct probe_work;
	const struct drm_dp_mst_topology_cbs *cbs;
	u32 port_cnt;
};

struct dp_mst_bridge {
	struct drm_bridge base;
	struct drm_private_obj obj;
	u32 id;

	bool in_use;

	struct dp_display *display;
	struct drm_encoder *encoder;

	struct drm_display_mode drm_mode;
	struct dp_display_mode dp_mode;
	struct drm_connector *connector;
	void *dp_panel;

	int vcpi;
	int pbn;
	int num_slots;
	int start_slot;

	u32 fixed_port_num;
	bool fixed_port_added;
	struct drm_connector *fixed_connector;
};

struct dp_mst_bridge_state {
	struct drm_private_state base;
	struct drm_connector *connector;
	void *dp_panel;
	int num_slots;
};

struct dp_mst_private {
	bool mst_initialized;
	struct dp_mst_caps caps;
	struct drm_dp_mst_topology_mgr mst_mgr;
	struct dp_mst_bridge mst_bridge[MAX_DP_MST_DRM_BRIDGES];
	struct dp_display *dp_display;
	const struct dp_drm_mst_fw_helper_ops *mst_fw_cbs;
	struct dp_mst_sim_mode simulator;
	struct mutex mst_lock;
	enum dp_drv_state state;
	bool mst_session_state;
};

struct dp_mst_encoder_info_cache {
	u8 cnt;
	struct drm_encoder *mst_enc[MAX_DP_MST_DRM_BRIDGES];
};

#define to_dp_mst_bridge(x)     container_of((x), struct dp_mst_bridge, base)
#define to_dp_mst_bridge_priv(x) \
		container_of((x), struct dp_mst_bridge, obj)
#define to_dp_mst_bridge_priv_state(x) \
		container_of((x), struct dp_mst_bridge_state, base)
#define to_dp_mst_bridge_state(x) \
		to_dp_mst_bridge_priv_state((x)->obj.state)

struct dp_mst_private dp_mst;
struct dp_mst_encoder_info_cache dp_mst_enc_cache;

static struct drm_private_state *dp_mst_duplicate_bridge_state(
		struct drm_private_obj *obj)
{
	struct dp_mst_bridge_state *state;

	state = kmemdup(obj->state, sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base);

	return &state->base;
}

static void dp_mst_destroy_bridge_state(struct drm_private_obj *obj,
		struct drm_private_state *state)
{
	struct dp_mst_bridge_state *priv_state =
		to_dp_mst_bridge_priv_state(state);

	kfree(priv_state);
}

static const struct drm_private_state_funcs dp_mst_bridge_state_funcs = {
	.atomic_duplicate_state = dp_mst_duplicate_bridge_state,
	.atomic_destroy_state = dp_mst_destroy_bridge_state,
};

static struct dp_mst_bridge_state *dp_mst_get_bridge_atomic_state(
		struct drm_atomic_state *state, struct dp_mst_bridge *bridge)
{
	struct drm_device *dev = bridge->base.dev;

	WARN_ON(!drm_modeset_is_locked(&dev->mode_config.connection_mutex));

	return to_dp_mst_bridge_priv_state(
			drm_atomic_get_private_obj_state(state, &bridge->obj));
}

static void dp_mst_sim_destroy_port(struct kref *ref)
{
	struct drm_dp_mst_port *port = container_of(ref,
			struct drm_dp_mst_port, kref);
	struct drm_dp_mst_topology_mgr *mgr = port->mgr;

	if (port->cached_edid)
		kfree(port->cached_edid);

	if (port->connector) {
		mutex_lock(&mgr->destroy_connector_lock);
		kref_get(&port->parent->kref);
		list_add(&port->next, &mgr->destroy_connector_list);
		mutex_unlock(&mgr->destroy_connector_lock);
		schedule_work(&mgr->destroy_connector_work);
		return;
	} else {
		kfree(port);
	}
}

/* DRM DP MST Framework simulator OPs */
static void dp_mst_sim_add_port(struct dp_mst_private *mst,
			struct dp_mst_sim_port_data *port_msg)
{
	struct drm_dp_mst_branch *mstb;
	struct drm_dp_mst_port *port;

	mstb = mst->mst_mgr.mst_primary;

	port = kzalloc(sizeof(*port), GFP_KERNEL);
	if (!port)
		return;
	kref_init(&port->kref);
	port->parent = mstb;
	port->port_num = port_msg->port_number;
	port->mgr = mstb->mgr;
	port->aux.name = dp_mst.caps.drm_aux->name;
	port->aux.dev = mst->dp_display->drm_dev->dev;

	port->pdt = port_msg->peer_device_type;
	port->input = port_msg->input_port;
	port->mcs = port_msg->mcs;
	port->ddps = port_msg->ddps;
	port->ldps = port_msg->legacy_device_plug_status;
	port->dpcd_rev = port_msg->dpcd_revision;
	port->num_sdp_streams = port_msg->num_sdp_streams;
	port->num_sdp_stream_sinks = port_msg->num_sdp_stream_sinks;

	mutex_lock(&mstb->mgr->lock);
	kref_get(&port->kref);
	list_add(&port->next, &mstb->ports);
	mutex_unlock(&mstb->mgr->lock);

	/* use fixed pbn for simulator ports */
	port->available_pbn = 2520;

	if (!port->input) {
		port->connector = (*mstb->mgr->cbs->add_connector)
				(mstb->mgr, port, NULL);
		if (!port->connector) {
			/* remove it from the port list */
			mutex_lock(&mstb->mgr->lock);
			list_del(&port->next);
			mutex_unlock(&mstb->mgr->lock);
			kref_put(&port->kref, dp_mst_sim_destroy_port);
			goto put_port;
		}
		(*mstb->mgr->cbs->register_connector)(port->connector);
	}

put_port:
	kref_put(&port->kref, dp_mst_sim_destroy_port);
}

static void dp_mst_sim_link_probe_work(struct work_struct *work)
{
	struct dp_mst_sim_mode *sim;
	struct dp_mst_private *mst;
	struct dp_mst_sim_port_data port_data;
	u8 cnt, i;

	DP_MST_DEBUG("enter\n");
	sim = container_of(work, struct dp_mst_sim_mode, probe_work);
	mst = container_of(sim, struct dp_mst_private, simulator);

	port_data.input_port = false;
	port_data.peer_device_type = DP_PEER_DEVICE_SST_SINK;
	port_data.mcs = false;
	port_data.ddps = true;
	port_data.legacy_device_plug_status = false;
	port_data.dpcd_revision = 0;
	port_data.num_sdp_streams = 0;
	port_data.num_sdp_stream_sinks = 0;

	for (i = 0; i < DP_MST_SIM_MAX_PORTS; i++)
		sim->port_edids[i].valid = false;

	for (cnt = 0; cnt < sim->port_cnt; cnt++) {
		port_data.port_number = cnt;

		for (i = 0; i < DP_MST_SIM_MAX_PORTS; i++) {
			if (sim->port_edids[i].valid) continue;

			sim->port_edids[i].port_number = port_data.port_number;
			memcpy(sim->port_edids[i].edid, sim->edid, SZ_256);
			sim->port_edids[i].valid = true;
			break;
		}

		dp_mst_sim_add_port(mst, &port_data);
	}

	mst->mst_mgr.cbs->hotplug(&mst->mst_mgr);
	DP_MST_DEBUG("completed\n");
}

static int dp_mst_sim_no_action(struct drm_dp_mst_topology_mgr *mgr)
{
	return 0;
}

static int dp_mst_sim_update_payload_part1(struct drm_dp_mst_topology_mgr *mgr)
{
	int i, j;
	int cur_slots = 1;
	struct drm_dp_payload req_payload;
	struct drm_dp_mst_port *port;

	mutex_lock(&mgr->payload_lock);
	for (i = 0; i < mgr->max_payloads; i++) {
		req_payload.start_slot = cur_slots;
		if (mgr->proposed_vcpis[i]) {
			port = container_of(mgr->proposed_vcpis[i],
					struct drm_dp_mst_port, vcpi);
			req_payload.num_slots =
					mgr->proposed_vcpis[i]->num_slots;
			req_payload.vcpi = mgr->proposed_vcpis[i]->vcpi;
		} else {
			port = NULL;
			req_payload.num_slots = 0;
		}

		if (mgr->payloads[i].start_slot != req_payload.start_slot)
			mgr->payloads[i].start_slot = req_payload.start_slot;

		if (mgr->payloads[i].num_slots != req_payload.num_slots) {
			if (req_payload.num_slots) {
				req_payload.payload_state = DP_PAYLOAD_LOCAL;
				mgr->payloads[i].num_slots =
						req_payload.num_slots;
				mgr->payloads[i].vcpi = req_payload.vcpi;
			} else if (mgr->payloads[i].num_slots) {
				mgr->payloads[i].num_slots = 0;
				mgr->payloads[i].payload_state =
						DP_PAYLOAD_DELETE_LOCAL;
				req_payload.payload_state =
						mgr->payloads[i].payload_state;
				mgr->payloads[i].start_slot = 0;
			} else
				req_payload.payload_state =
					mgr->payloads[i].payload_state;

			mgr->payloads[i].payload_state =
				req_payload.payload_state;
		}
		cur_slots += req_payload.num_slots;
	}

	for (i = 0; i < mgr->max_payloads; i++) {
		if (mgr->payloads[i].payload_state == DP_PAYLOAD_DELETE_LOCAL) {
			DP_DEBUG("removing payload %d\n", i);
			for (j = i; j < mgr->max_payloads - 1; j++) {
				memcpy(&mgr->payloads[j],
					&mgr->payloads[j + 1],
					sizeof(struct drm_dp_payload));
				mgr->proposed_vcpis[j] =
					mgr->proposed_vcpis[j + 1];
				if (mgr->proposed_vcpis[j] &&
					mgr->proposed_vcpis[j]->num_slots) {
					set_bit(j + 1, &mgr->payload_mask);
				} else {
					clear_bit(j + 1, &mgr->payload_mask);
				}
			}
			memset(&mgr->payloads[mgr->max_payloads - 1], 0,
					sizeof(struct drm_dp_payload));
			mgr->proposed_vcpis[mgr->max_payloads - 1] = NULL;
			clear_bit(mgr->max_payloads, &mgr->payload_mask);
		}
	}
	mutex_unlock(&mgr->payload_lock);
	return 0;
}

static int dp_mst_sim_update_payload_part2(struct drm_dp_mst_topology_mgr *mgr)
{
	struct drm_dp_mst_port *port;
	int i;

	mutex_lock(&mgr->payload_lock);
	for (i = 0; i < mgr->max_payloads; i++) {

		if (!mgr->proposed_vcpis[i])
			continue;

		port = container_of(mgr->proposed_vcpis[i],
				struct drm_dp_mst_port, vcpi);

		DP_DEBUG("payload %d %d\n", i, mgr->payloads[i].payload_state);
		if (mgr->payloads[i].payload_state == DP_PAYLOAD_LOCAL)
			mgr->payloads[i].payload_state = DP_PAYLOAD_REMOTE;
		else if (mgr->payloads[i].payload_state ==
				DP_PAYLOAD_DELETE_LOCAL)
			mgr->payloads[i].payload_state = 0;
	}
	mutex_unlock(&mgr->payload_lock);
	return 0;
}

static struct edid *dp_mst_sim_get_edid(struct drm_connector *connector,
		struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port)
{
	struct dp_mst_private *mst = container_of(mgr,
			struct dp_mst_private, mst_mgr);
	int i;

	for (i = 0; i < DP_MST_SIM_MAX_PORTS; i++) {
		if (mst->simulator.port_edids[i].valid &&
				mst->simulator.port_edids[i].port_number ==
				port->port_num) {
			return drm_edid_duplicate((struct edid *)
					(mst->simulator.port_edids[i].edid));
		}
	}

	DRM_ERROR("edid not found for connector %d\n", connector->base.id);
	return NULL;
}

static int dp_mst_sim_topology_mgr_set_mst(
		struct drm_dp_mst_topology_mgr *mgr,
		bool mst_state)
{
	int rc;
	struct dp_mst_private *mst = container_of(mgr,
			struct dp_mst_private, mst_mgr);

	rc = drm_dp_mst_topology_mgr_set_mst(mgr, mst_state);
	if (rc < 0) {
		DRM_ERROR("unable to set mst topology mgr, rc: %d\n", rc);
		return rc;
	}

	if (mst_state)
		queue_work(system_long_wq, &mst->simulator.probe_work);

	mst->simulator.mst_state = mst_state;
	return 0;
}

static void dp_mst_sim_handle_hpd_irq(void *dp_display,
		struct dp_mst_hpd_info *info)
{
	struct dp_display *dp;
	struct dp_mst_private *mst;
	struct drm_dp_mst_port *port;
	struct dp_mst_sim_port_data port_data;
	struct drm_dp_mst_branch *mstb;
	int i;
	bool in_list, port_available;

	dp = dp_display;
	mst = dp->dp_mst_prv_info;

	if (info->mst_sim_add_con) {
		port_available = false;
		for (i = 0; i < DP_MST_SIM_MAX_PORTS; i++) {
			if (mst->simulator.port_edids[i].valid) continue;

			port_data.port_number = i;
			mst->simulator.port_edids[i].port_number = i;
			memcpy(mst->simulator.port_edids[i].edid, info->edid,
					SZ_256);
			mst->simulator.port_edids[i].valid = true;
			port_available = true;
			break;
		}

		if (!port_available) {
			DRM_ERROR("add port failed, limit (%d) reached\n",
					DP_MST_SIM_MAX_PORTS);
			return;
		}

		port_data.input_port = false;
		port_data.peer_device_type = DP_PEER_DEVICE_SST_SINK;
		port_data.mcs = false;
		port_data.ddps = true;
		port_data.legacy_device_plug_status = false;
		port_data.dpcd_revision = 0;
		port_data.num_sdp_streams = 0;
		port_data.num_sdp_stream_sinks = 0;

		dp_mst_sim_add_port(mst, &port_data);
	} else if (info->mst_sim_remove_con) {
		mstb = mst->mst_mgr.mst_primary;
		in_list = false;

		mutex_lock(&mst->mst_mgr.lock);
		list_for_each_entry(port,
				&mstb->ports, next) {
			if (port->connector && port->connector->base.id ==
					info->mst_sim_remove_con_id) {
				in_list = true;
				list_del(&port->next);
				break;
			}
		}
		mutex_unlock(&mst->mst_mgr.lock);

		if (!in_list) {
			DRM_ERROR("invalid connector id %d\n",
					info->mst_sim_remove_con_id);
			return;
		}

		for (i = 0; i < DP_MST_SIM_MAX_PORTS; i++) {
			if (mst->simulator.port_edids[i].port_number ==
					port->port_num) {
				mst->simulator.port_edids[i].valid = false;
			}
		}

		kref_put(&port->kref, dp_mst_sim_destroy_port);
	}
}

static void _dp_mst_get_vcpi_info(
		struct drm_dp_mst_topology_mgr *mgr,
		int vcpi, int *start_slot, int *num_slots)
{
	int i;

	*start_slot = 0;
	*num_slots = 0;

	mutex_lock(&mgr->payload_lock);
	for (i = 0; i < mgr->max_payloads; i++) {
		if (mgr->payloads[i].vcpi == vcpi) {
			*start_slot = mgr->payloads[i].start_slot;
			*num_slots = mgr->payloads[i].num_slots;
			break;
		}
	}
	mutex_unlock(&mgr->payload_lock);

	DP_INFO("vcpi_info. vcpi:%d, start_slot:%d, num_slots:%d\n",
			vcpi, *start_slot, *num_slots);
}

static int dp_mst_calc_pbn_mode(struct dp_display_mode *dp_mode)
{
	int pbn, bpp;
	bool dsc_en;
	s64 pbn_fp;

	dsc_en = dp_mode->timing.comp_info.comp_ratio ? true : false;
	bpp = dsc_en ? dp_mode->timing.comp_info.dsc_info.bpp :
		dp_mode->timing.bpp;

	pbn = drm_dp_calc_pbn_mode(dp_mode->timing.pixel_clk_khz, bpp);
	pbn_fp = drm_fixp_from_fraction(pbn, 1);

	DP_DEBUG("before overhead pbn:%d, bpp:%d\n", pbn, bpp);

	if (dsc_en)
		pbn_fp = drm_fixp_mul(pbn_fp, dp_mode->dsc_overhead_fp);

	if (dp_mode->fec_overhead_fp)
		pbn_fp = drm_fixp_mul(pbn_fp, dp_mode->fec_overhead_fp);

	pbn = drm_fixp2int(pbn_fp);

	DP_DEBUG("after overhead pbn:%d, bpp:%d\n", pbn, bpp);
	return pbn;
}

static const struct dp_drm_mst_fw_helper_ops drm_dp_mst_fw_helper_ops = {
	.calc_pbn_mode             = dp_mst_calc_pbn_mode,
	.find_vcpi_slots           = drm_dp_find_vcpi_slots,
	.atomic_find_vcpi_slots    = drm_dp_atomic_find_vcpi_slots,
	.allocate_vcpi             = drm_dp_mst_allocate_vcpi,
	.update_payload_part1      = drm_dp_update_payload_part1,
	.check_act_status          = drm_dp_check_act_status,
	.update_payload_part2      = drm_dp_update_payload_part2,
	.detect_port               = drm_dp_mst_detect_port,
	.get_edid                  = drm_dp_mst_get_edid,
	.topology_mgr_set_mst      = drm_dp_mst_topology_mgr_set_mst,
	.get_vcpi_info             = _dp_mst_get_vcpi_info,
	.atomic_release_vcpi_slots = drm_dp_atomic_release_vcpi_slots,
	.reset_vcpi_slots          = drm_dp_mst_reset_vcpi_slots,
	.deallocate_vcpi           = drm_dp_mst_deallocate_vcpi,
};

static const struct dp_drm_mst_fw_helper_ops drm_dp_sim_mst_fw_helper_ops = {
	.calc_pbn_mode             = dp_mst_calc_pbn_mode,
	.find_vcpi_slots           = drm_dp_find_vcpi_slots,
	.atomic_find_vcpi_slots    = drm_dp_atomic_find_vcpi_slots,
	.allocate_vcpi             = drm_dp_mst_allocate_vcpi,
	.update_payload_part1      = dp_mst_sim_update_payload_part1,
	.check_act_status          = dp_mst_sim_no_action,
	.update_payload_part2      = dp_mst_sim_update_payload_part2,
	.detect_port               = drm_dp_mst_detect_port,
	.get_edid                  = dp_mst_sim_get_edid,
	.topology_mgr_set_mst      = dp_mst_sim_topology_mgr_set_mst,
	.get_vcpi_info             = _dp_mst_get_vcpi_info,
	.atomic_release_vcpi_slots = drm_dp_atomic_release_vcpi_slots,
	.reset_vcpi_slots          = drm_dp_mst_reset_vcpi_slots,
	.deallocate_vcpi           = drm_dp_mst_deallocate_vcpi,
};

/* DP MST Bridge OPs */

static int dp_mst_bridge_attach(struct drm_bridge *dp_bridge)
{
	struct dp_mst_bridge *bridge;

	DP_MST_DEBUG("enter\n");

	if (!dp_bridge) {
		DP_ERR("Invalid params\n");
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
	struct drm_crtc_state *crtc_state;
	struct dp_mst_bridge_state *bridge_state;

	DP_MST_DEBUG("enter\n");

	if (!drm_bridge || !mode || !adjusted_mode) {
		DP_ERR("Invalid params\n");
		ret = false;
		goto end;
	}

	bridge = to_dp_mst_bridge(drm_bridge);

	crtc_state = container_of(mode, struct drm_crtc_state, mode);
	bridge_state = dp_mst_get_bridge_atomic_state(crtc_state->state,
			bridge);
	if (IS_ERR(bridge_state)) {
		DP_ERR("invalid bridge state\n");
		ret = false;
		goto end;
	}

	if (!bridge_state->dp_panel) {
		DP_ERR("Invalid dp_panel\n");
		ret = false;
		goto end;
	}

	dp = bridge->display;

	dp->convert_to_dp_mode(dp, bridge_state->dp_panel, mode, &dp_mode);
	convert_to_drm_mode(&dp_mode, adjusted_mode);

	DP_MST_DEBUG("mst bridge [%d] mode:%s fixup\n", bridge->id, mode->name);
end:
	return ret;
}

static int _dp_mst_compute_config(struct drm_atomic_state *state,
		struct dp_mst_private *mst, struct drm_connector *connector,
		struct dp_display_mode *mode)
{
	int slots = 0, pbn;
	struct sde_connector *c_conn = to_sde_connector(connector);

	DP_MST_DEBUG("enter\n");

	pbn = mst->mst_fw_cbs->calc_pbn_mode(mode);

	slots = mst->mst_fw_cbs->atomic_find_vcpi_slots(state,
			&mst->mst_mgr, c_conn->mst_port, pbn);
	if (slots < 0) {
		DP_ERR("mst: failed to find vcpi slots. pbn:%d, slots:%d\n",
				pbn, slots);
		return slots;
	}

	DP_MST_DEBUG("exit\n");

	return slots;
}

static void _dp_mst_update_timeslots(struct dp_mst_private *mst,
		struct dp_mst_bridge *mst_bridge)
{
	int i;
	struct dp_mst_bridge *dp_bridge;
	int pbn, start_slot, num_slots;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		dp_bridge = &mst->mst_bridge[i];

		pbn = 0;
		start_slot = 0;
		num_slots = 0;

		if (dp_bridge->vcpi) {
			mst->mst_fw_cbs->get_vcpi_info(&mst->mst_mgr,
					dp_bridge->vcpi,
					&start_slot, &num_slots);
			pbn = dp_bridge->pbn;
		}

		if (mst_bridge == dp_bridge)
			dp_bridge->num_slots = num_slots;

		mst->dp_display->set_stream_info(mst->dp_display,
				dp_bridge->dp_panel,
				dp_bridge->id, start_slot, num_slots, pbn,
				dp_bridge->vcpi);

		DP_INFO("bridge:%d vcpi:%d start_slot:%d num_slots:%d, pbn:%d\n",
			dp_bridge->id, dp_bridge->vcpi,
			start_slot, num_slots, pbn);
	}
}

static void _dp_mst_update_single_timeslot(struct dp_mst_private *mst,
		struct dp_mst_bridge *mst_bridge)
{
	int pbn = 0, start_slot = 0, num_slots = 0;

	if (mst->state == PM_SUSPEND) {
		if (mst_bridge->vcpi) {
			mst->mst_fw_cbs->get_vcpi_info(&mst->mst_mgr,
					mst_bridge->vcpi,
					&start_slot, &num_slots);
			pbn = mst_bridge->pbn;
		}

		mst_bridge->num_slots = num_slots;

		mst->dp_display->set_stream_info(mst->dp_display,
				mst_bridge->dp_panel,
				mst_bridge->id, start_slot, num_slots, pbn,
				mst_bridge->vcpi);
	}
}

static void _dp_mst_bridge_pre_enable_part1(struct dp_mst_bridge *dp_bridge)
{
	struct dp_display *dp_display = dp_bridge->display;
	struct sde_connector *c_conn =
		to_sde_connector(dp_bridge->connector);
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct drm_dp_mst_port *port = c_conn->mst_port;
	bool ret;
	int pbn, slots;

	/* skip mst specific disable operations during suspend */
	if (mst->state == PM_SUSPEND) {
		dp_display->wakeup_phy_layer(dp_display, true);
		drm_dp_send_power_updown_phy(&mst->mst_mgr, port, true);
		dp_display->wakeup_phy_layer(dp_display, false);
		_dp_mst_update_single_timeslot(mst, dp_bridge);
		return;
	}

	pbn = mst->mst_fw_cbs->calc_pbn_mode(&dp_bridge->dp_mode);

	slots = mst->mst_fw_cbs->find_vcpi_slots(&mst->mst_mgr, pbn);

	DP_INFO("bridge:%d, pbn:%d, slots:%d\n", dp_bridge->id,
			dp_bridge->pbn, dp_bridge->num_slots);

	ret = mst->mst_fw_cbs->allocate_vcpi(&mst->mst_mgr,
				       port, pbn, slots);
	if (!ret) {
		DP_ERR("mst: failed to allocate vcpi. bridge:%d\n",
				dp_bridge->id);
		return;
	}

	dp_bridge->vcpi = port->vcpi.vcpi;
	dp_bridge->pbn = pbn;

	ret = mst->mst_fw_cbs->update_payload_part1(&mst->mst_mgr);

	_dp_mst_update_timeslots(mst, dp_bridge);
}

static void _dp_mst_bridge_pre_enable_part2(struct dp_mst_bridge *dp_bridge)
{
	struct dp_display *dp_display = dp_bridge->display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;

	DP_MST_DEBUG("enter\n");

	/* skip mst specific disable operations during suspend */
	if (mst->state == PM_SUSPEND)
		return;

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

	/* skip mst specific disable operations during suspend */
	if (mst->state == PM_SUSPEND) {
		_dp_mst_update_single_timeslot(mst, dp_bridge);
		return;
	}

	mst->mst_fw_cbs->reset_vcpi_slots(&mst->mst_mgr, port);

	mst->mst_fw_cbs->update_payload_part1(&mst->mst_mgr);

	_dp_mst_update_timeslots(mst, dp_bridge);

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

	/* skip mst specific disable operations during suspend */
	if (mst->state == PM_SUSPEND) {
		dp_display->wakeup_phy_layer(dp_display, true);
		drm_dp_send_power_updown_phy(&mst->mst_mgr, port, false);
		dp_display->wakeup_phy_layer(dp_display, false);
		return;
	}

	mst->mst_fw_cbs->check_act_status(&mst->mst_mgr);

	mst->mst_fw_cbs->update_payload_part2(&mst->mst_mgr);

	mst->mst_fw_cbs->deallocate_vcpi(&mst->mst_mgr, port);

	dp_bridge->vcpi = 0;
	dp_bridge->pbn = 0;

	DP_MST_DEBUG("mst bridge [%d] _pre disable part-2 complete\n",
			dp_bridge->id);
}

static void dp_mst_bridge_pre_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;
	struct dp_mst_private *mst;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	dp = bridge->display;

	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	mst = dp->dp_mst_prv_info;

	mutex_lock(&mst->mst_lock);

	/* By this point mode should have been validated through mode_fixup */
	rc = dp->set_mode(dp, bridge->dp_panel, &bridge->dp_mode);
	if (rc) {
		DP_ERR("[%d] failed to perform a mode set, rc=%d\n",
		       bridge->id, rc);
		goto end;
	}

	rc = dp->prepare(dp, bridge->dp_panel);
	if (rc) {
		DP_ERR("[%d] DP display prepare failed, rc=%d\n",
		       bridge->id, rc);
		goto end;
	}

	_dp_mst_bridge_pre_enable_part1(bridge);

	rc = dp->enable(dp, bridge->dp_panel);
	if (rc) {
		DP_ERR("[%d] DP display enable failed, rc=%d\n",
		       bridge->id, rc);
		dp->unprepare(dp, bridge->dp_panel);
		goto end;
	} else {
		_dp_mst_bridge_pre_enable_part2(bridge);
	}

	DP_MST_INFO_LOG("mode: id(%d) mode(%s), refresh(%d)\n",
			bridge->id, bridge->drm_mode.name,
			bridge->drm_mode.vrefresh);
	DP_MST_INFO_LOG("dsc: id(%d) dsc(%d)\n", bridge->id,
			bridge->dp_mode.timing.comp_info.comp_ratio);
	DP_MST_INFO_LOG("channel: id(%d) vcpi(%d) start(%d) tot(%d)\n",
			bridge->id, bridge->vcpi, bridge->start_slot,
			bridge->num_slots);
end:
	mutex_unlock(&mst->mst_lock);
}

static void dp_mst_bridge_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	dp = bridge->display;

	rc = dp->post_enable(dp, bridge->dp_panel);
	if (rc) {
		DP_ERR("mst bridge [%d] post enable failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	DP_MST_INFO_LOG("mst bridge [%d] post enable complete\n",
			bridge->id);
}

static void dp_mst_bridge_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;
	struct dp_mst_private *mst;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	dp = bridge->display;

	mst = dp->dp_mst_prv_info;

	sde_connector_helper_bridge_disable(bridge->connector);

	mutex_lock(&mst->mst_lock);

	_dp_mst_bridge_pre_disable_part1(bridge);

	rc = dp->pre_disable(dp, bridge->dp_panel);
	if (rc)
		DP_ERR("[%d] DP display pre disable failed, rc=%d\n",
		       bridge->id, rc);

	_dp_mst_bridge_pre_disable_part2(bridge);

	DP_MST_INFO_LOG("mst bridge [%d] disable complete\n", bridge->id);

	mutex_unlock(&mst->mst_lock);
}

static void dp_mst_bridge_post_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;
	struct dp_mst_private *mst;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	dp = bridge->display;
	mst = dp->dp_mst_prv_info;

	rc = dp->disable(dp, bridge->dp_panel);
	if (rc)
		DP_INFO("[%d] DP display disable failed, rc=%d\n",
		       bridge->id, rc);

	rc = dp->unprepare(dp, bridge->dp_panel);
	if (rc)
		DP_INFO("[%d] DP display unprepare failed, rc=%d\n",
		       bridge->id, rc);

	bridge->connector = NULL;
	bridge->dp_panel =  NULL;

	DP_MST_INFO_LOG("mst bridge [%d] post disable complete\n",
			bridge->id);
}

static void dp_mst_bridge_mode_set(struct drm_bridge *drm_bridge,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct dp_mst_bridge *bridge;
	struct dp_mst_bridge_state *dp_bridge_state;
	struct dp_display *dp;

	DP_MST_DEBUG("enter\n");

	if (!drm_bridge || !mode || !adjusted_mode) {
		DP_ERR("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);

	dp_bridge_state = to_dp_mst_bridge_state(bridge);
	bridge->connector = dp_bridge_state->connector;
	bridge->dp_panel = dp_bridge_state->dp_panel;

	dp = bridge->display;

	memset(&bridge->dp_mode, 0x0, sizeof(struct dp_display_mode));
	memcpy(&bridge->drm_mode, adjusted_mode, sizeof(bridge->drm_mode));
	dp->convert_to_dp_mode(dp, bridge->dp_panel, adjusted_mode,
			&bridge->dp_mode);

	DP_MST_DEBUG("mst bridge [%d] mode set complete\n", bridge->id);
}

/* DP MST Bridge APIs */

static struct drm_connector *
dp_mst_drm_fixed_connector_init(struct dp_display *dp_display,
				struct drm_encoder *encoder);

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
	struct dp_mst_bridge_state *state;
	struct drm_device *dev;
	struct dp_display *display = data;
	struct msm_drm_private *priv = NULL;
	struct dp_mst_private *mst = display->dp_mst_prv_info;
	int i;

	if (!mst || !mst->mst_initialized) {
		if (dp_mst_enc_cache.cnt >= MAX_DP_MST_DRM_BRIDGES) {
			DP_INFO("exceeding max bridge cnt %d\n",
					dp_mst_enc_cache.cnt);
			return 0;
		}

		dp_mst_enc_cache.mst_enc[dp_mst_enc_cache.cnt] = encoder;
		dp_mst_enc_cache.cnt++;
		DP_INFO("mst not initialized. cache encoder information\n");
		return 0;
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
		DP_ERR("mst supports only %d bridges\n", i);
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
		DP_ERR("failed to attach bridge, rc=%d\n", rc);
		goto end;
	}

	encoder->bridge = &bridge->base;
	priv->bridges[priv->num_bridges++] = &bridge->base;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state == NULL) {
		rc = -ENOMEM;
		goto end;
	}

	drm_atomic_private_obj_init(&bridge->obj,
				    &state->base,
				    &dp_mst_bridge_state_funcs);

	DP_MST_DEBUG("mst drm bridge init. bridge id:%d\n", i);

	/*
	 * If fixed topology port is defined, connector will be created
	 * immediately.
	 */
	rc = display->mst_get_fixed_topology_port(display, bridge->id,
			&bridge->fixed_port_num);
	if (!rc) {
		bridge->fixed_connector =
			dp_mst_drm_fixed_connector_init(display,
				bridge->encoder);
		if (bridge->fixed_connector == NULL) {
			DP_ERR("failed to create fixed connector\n");
			kfree(state);
			rc = -ENOMEM;
			goto end;
		}
	}

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
	struct dp_mst_connector mst_conn;

	DP_MST_DEBUG("enter:\n");

	status = mst->mst_fw_cbs->detect_port(connector,
			&mst->mst_mgr,
			c_conn->mst_port);

	memset(&mst_conn, 0, sizeof(mst_conn));
	dp_display->mst_get_connector_info(dp_display, connector, &mst_conn);
	if (mst_conn.conn == connector &&
			mst_conn.state != connector_status_unknown) {
		status = mst_conn.state;
	}

	DP_MST_DEBUG("mst connector:%d detect, status:%d\n",
			connector->base.id, status);

	DP_MST_DEBUG("exit:\n");

	return status;
}

static int dp_mst_connector_get_modes(struct drm_connector *connector,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct edid *edid;
	int rc = 0;

	DP_MST_DEBUG("enter:\n");

	edid = mst->mst_fw_cbs->get_edid(connector, &mst->mst_mgr,
			c_conn->mst_port);

	if (edid)
		rc = dp_display->mst_connector_update_edid(dp_display,
				connector, edid);

	DP_MST_DEBUG("mst connector get modes. id: %d\n", connector->base.id);

	DP_MST_DEBUG("exit:\n");

	return rc;
}

enum drm_mode_status dp_mst_connector_mode_valid(
		struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst;
	struct sde_connector *c_conn;
	struct drm_dp_mst_port *mst_port;
	struct dp_display_mode dp_mode;
	uint16_t available_pbn, required_pbn;
	int available_slots, required_slots;
	struct dp_mst_bridge_state *dp_bridge_state;
	int i, slots_in_use = 0, active_enc_cnt = 0;
	const u32 tot_slots = 63;

	if (!connector || !mode || !display) {
		DP_ERR("invalid input\n");
		return 0;
	}

	mst = dp_display->dp_mst_prv_info;
	c_conn = to_sde_connector(connector);
	mst_port = c_conn->mst_port;

	/* dp bridge state is protected by drm_mode_config.connection_mutex */
	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		dp_bridge_state = to_dp_mst_bridge_state(&mst->mst_bridge[i]);
		if (dp_bridge_state->connector &&
				dp_bridge_state->connector != connector) {
			active_enc_cnt++;
			slots_in_use += dp_bridge_state->num_slots;
		}
	}

	if (active_enc_cnt < DP_STREAM_MAX) {
		available_pbn = mst_port->available_pbn;
		available_slots = tot_slots - slots_in_use;
	} else {
		pr_debug("all mst streams are active\n");
		return MODE_BAD;
	}

	dp_display->convert_to_dp_mode(dp_display, c_conn->drv_panel,
			mode, &dp_mode);

	required_pbn = mst->mst_fw_cbs->calc_pbn_mode(&dp_mode);
	required_slots = mst->mst_fw_cbs->find_vcpi_slots(
			&mst->mst_mgr, required_pbn);

	if (required_pbn > available_pbn || required_slots > available_slots) {
		DP_DEBUG("mode:%s not supported\n", mode->name);
		return MODE_BAD;
	}

	return dp_connector_mode_valid(connector, mode, display, avail_res);
}

int dp_mst_connector_get_info(struct drm_connector *connector,
		struct msm_display_info *info,
		void *display)
{
	int rc;
	enum drm_connector_status status = connector_status_unknown;

	DP_MST_DEBUG("enter:\n");

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

	DP_MST_DEBUG("exit:\n");

	return rc;
}

int dp_mst_connector_get_mode_info(struct drm_connector *connector,
		const struct drm_display_mode *drm_mode,
		struct msm_mode_info *mode_info,
		void *display,
		const struct msm_resource_caps_info *avail_res)
{
	int rc;

	DP_MST_DEBUG("enter:\n");

	rc = dp_connector_get_mode_info(connector, drm_mode, mode_info,
			display, avail_res);

	DP_MST_DEBUG("mst connector:%d get mode info. rc:%d\n",
			connector->base.id, rc);

	DP_MST_DEBUG("exit:\n");

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
	struct dp_mst_bridge_state *bridge_state;
	u32 i;

	if (state->best_encoder)
		return state->best_encoder;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		bridge_state = dp_mst_get_bridge_atomic_state(
				state->state, &mst->mst_bridge[i]);
		if (IS_ERR(bridge_state))
			goto end;

		if (bridge_state->connector == connector) {
			enc = mst->mst_bridge[i].encoder;
			goto end;
		}
	}

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (mst->mst_bridge[i].fixed_connector)
			continue;

		bridge_state = dp_mst_get_bridge_atomic_state(
				state->state, &mst->mst_bridge[i]);

		if (!bridge_state->connector) {
			bridge_state->connector = connector;
			bridge_state->dp_panel = conn->drv_panel;
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

static int dp_mst_connector_atomic_check(struct drm_connector *connector,
		void *display, struct drm_connector_state *new_conn_state)
{
	int rc = 0, slots, i;
	struct drm_atomic_state *state;
	struct drm_connector_state *old_conn_state;
	struct drm_crtc *old_crtc;
	struct drm_crtc_state *crtc_state;
	struct dp_mst_bridge *bridge;
	struct dp_mst_bridge_state *bridge_state;
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct sde_connector *c_conn;
	struct dp_display_mode dp_mode;

	DP_MST_DEBUG("enter:\n");

	if (!new_conn_state)
		return rc;

	state = new_conn_state->state;

	old_conn_state = drm_atomic_get_old_connector_state(state, connector);
	if (!old_conn_state)
		goto mode_set;

	old_crtc = old_conn_state->crtc;
	if (!old_crtc)
		goto mode_set;

	crtc_state = drm_atomic_get_new_crtc_state(state, old_crtc);

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		bridge = &mst->mst_bridge[i];
		DP_MST_DEBUG("bridge id:%d, vcpi:%d, pbn:%d, slots:%d\n",
				bridge->id, bridge->vcpi, bridge->pbn,
				bridge->num_slots);
	}

	if (drm_atomic_crtc_needs_modeset(crtc_state)) {
		if (WARN_ON(!old_conn_state->best_encoder)) {
			rc = -EINVAL;
			goto end;
		}

		bridge = to_dp_mst_bridge(
				old_conn_state->best_encoder->bridge);

		bridge_state = dp_mst_get_bridge_atomic_state(state, bridge);
		if (IS_ERR(bridge_state)) {
			rc = PTR_ERR(bridge_state);
			goto end;
		}

		if (WARN_ON(bridge_state->connector != connector)) {
			rc = -EINVAL;
			goto end;
		}

		slots = bridge_state->num_slots;
		if (slots > 0) {
			rc = mst->mst_fw_cbs->atomic_release_vcpi_slots(state,
					&mst->mst_mgr, slots);
			if (rc) {
				pr_err("failed releasing %d vcpi slots %d\n",
						slots, rc);
				goto end;
			}
		}

		bridge_state->num_slots = 0;

		if (!new_conn_state->crtc && mst->state != PM_SUSPEND) {
			bridge_state->connector = NULL;
			bridge_state->dp_panel = NULL;

			DP_MST_DEBUG("clear best encoder: %d\n", bridge->id);
		}
	}

mode_set:
	if (!new_conn_state->crtc)
		goto end;

	crtc_state = drm_atomic_get_new_crtc_state(state, new_conn_state->crtc);

	if (drm_atomic_crtc_needs_modeset(crtc_state) && crtc_state->active) {
		c_conn = to_sde_connector(connector);

		if (WARN_ON(!new_conn_state->best_encoder)) {
			rc = -EINVAL;
			goto end;
		}

		bridge = to_dp_mst_bridge(
				new_conn_state->best_encoder->bridge);

		bridge_state = dp_mst_get_bridge_atomic_state(state, bridge);
		if (IS_ERR(bridge_state)) {
			rc = PTR_ERR(bridge_state);
			goto end;
		}

		if (WARN_ON(bridge_state->connector != connector)) {
			rc = -EINVAL;
			goto end;
		}

		if (WARN_ON(bridge_state->num_slots)) {
			rc = -EINVAL;
			goto end;
		}

		dp_display->convert_to_dp_mode(dp_display, c_conn->drv_panel,
				&crtc_state->mode, &dp_mode);

		slots = _dp_mst_compute_config(state, mst, connector, &dp_mode);
		if (slots < 0) {
			rc = slots;
			goto end;
		}

		bridge_state->num_slots = slots;
	}

end:
	DP_MST_DEBUG("mst connector:%d atomic check ret %d\n",
			connector->base.id, rc);
	return rc;
}

static int dp_mst_connector_config_hdr(struct drm_connector *connector,
		void *display, struct sde_connector_state *c_state)
{
	int rc;

	DP_MST_DEBUG("enter:\n");

	rc = dp_connector_config_hdr(connector, display, c_state);

	DP_MST_DEBUG("mst connector:%d cfg hdr. rc:%d\n",
			connector->base.id, rc);

	DP_MST_DEBUG("exit:\n");

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
		.update_pps = dp_connector_update_pps,
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

	/* make sure connector is not accessed before reset */
	drm_modeset_lock_all(dev);

	connector = sde_connector_init(dev,
				dp_mst->mst_bridge[0].encoder,
				NULL,
				dp_display,
				&dp_mst_connector_ops,
				DRM_CONNECTOR_POLL_HPD,
				DRM_MODE_CONNECTOR_DisplayPort);

	if (!connector) {
		DP_ERR("mst sde_connector_init failed\n");
		drm_modeset_unlock_all(dev);
		return connector;
	}

	rc = dp_display->mst_connector_install(dp_display, connector);
	if (rc) {
		DP_ERR("mst connector install failed\n");
		sde_connector_destroy(connector);
		drm_modeset_unlock_all(dev);
		return NULL;
	}

	c_conn = to_sde_connector(connector);
	c_conn->mst_port = port;

	if (connector->funcs->reset)
		connector->funcs->reset(connector);

	for (i = 1; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		drm_connector_attach_encoder(connector,
				dp_mst->mst_bridge[i].encoder);
	}

	drm_object_attach_property(&connector->base,
			dev->mode_config.path_property, 0);
	drm_object_attach_property(&connector->base,
			dev->mode_config.tile_property, 0);

	/* unlock connector and make it accessible */
	drm_modeset_unlock_all(dev);

	DP_MST_INFO_LOG("add mst connector id:%d\n", connector->base.id);

	return connector;
}

static void dp_mst_register_connector(struct drm_connector *connector)
{
	DP_MST_DEBUG("enter\n");

	connector->status = connector->funcs->detect(connector, false);

	DP_MST_INFO_LOG("register mst connector id:%d\n",
			connector->base.id);
	drm_connector_register(connector);
}

static void dp_mst_destroy_connector(struct drm_dp_mst_topology_mgr *mgr,
					   struct drm_connector *connector)
{
	DP_MST_DEBUG("enter\n");

	DP_MST_INFO_LOG("destroy mst connector id:%d\n", connector->base.id);

	drm_connector_unregister(connector);
	drm_connector_put(connector);
}

static enum drm_connector_status
dp_mst_fixed_connector_detect(struct drm_connector *connector, bool force,
			void *display)
{
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	int i;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (mst->mst_bridge[i].fixed_connector != connector)
			continue;

		if (!mst->mst_bridge[i].fixed_port_added)
			break;

		return dp_mst_connector_detect(connector, force, display);
	}

	return connector_status_disconnected;
}

static struct drm_encoder *
dp_mst_fixed_atomic_best_encoder(struct drm_connector *connector,
			void *display, struct drm_connector_state *state)
{
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct sde_connector *conn = to_sde_connector(connector);
	struct drm_encoder *enc = NULL;
	struct dp_mst_bridge_state *bridge_state;
	u32 i;

	if (state->best_encoder)
		return state->best_encoder;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (mst->mst_bridge[i].fixed_connector == connector) {
			bridge_state = dp_mst_get_bridge_atomic_state(
					state->state, &mst->mst_bridge[i]);
			if (IS_ERR(bridge_state))
				goto end;

			bridge_state->connector = connector;
			bridge_state->dp_panel = conn->drv_panel;
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

static u32 dp_mst_find_fixed_port_num(struct drm_dp_mst_branch *mstb,
		struct drm_dp_mst_port *target)
{
	struct drm_dp_mst_port *port;
	u32 port_num = 0;

	/*
	 * search through reversed order of adding sequence, so the port number
	 * will be unique once topology is fixed
	 */
	list_for_each_entry_reverse(port, &mstb->ports, next) {
		if (port->mstb)
			port_num += dp_mst_find_fixed_port_num(port->mstb,
						target);
		else if (!port->input) {
			++port_num;
			if (port == target)
				break;
		}
	}

	return port_num;
}

static struct drm_connector *
dp_mst_find_fixed_connector(struct dp_mst_private *dp_mst,
		struct drm_dp_mst_port *port)
{
	struct dp_display *dp_display = dp_mst->dp_display;
	struct drm_connector *connector = NULL;
	struct sde_connector *c_conn;
	u32 port_num;
	int i;

	mutex_lock(&port->mgr->lock);
	port_num = dp_mst_find_fixed_port_num(port->mgr->mst_primary, port);
	mutex_unlock(&port->mgr->lock);

	if (!port_num)
		return NULL;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (dp_mst->mst_bridge[i].fixed_port_num == port_num) {
			connector = dp_mst->mst_bridge[i].fixed_connector;
			c_conn = to_sde_connector(connector);
			c_conn->mst_port = port;
			dp_display->mst_connector_update_link_info(dp_display,
					connector);
			dp_mst->mst_bridge[i].fixed_port_added = true;
			DP_MST_DEBUG("found fixed connector %d\n",
					DRMID(connector));
			break;
		}
	}

	return connector;
}

static int
dp_mst_find_first_available_encoder_idx(struct dp_mst_private *dp_mst)
{
	int enc_idx = MAX_DP_MST_DRM_BRIDGES;
	int i;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (!dp_mst->mst_bridge[i].fixed_connector) {
			enc_idx = i;
			break;
		}
	}

	return enc_idx;
}

static struct drm_connector *
dp_mst_add_fixed_connector(struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port, const char *pathprop)
{
	struct dp_mst_private *dp_mst;
	struct drm_device *dev;
	struct dp_display *dp_display;
	struct drm_connector *connector;
	int i, enc_idx;

	DP_MST_DEBUG("enter\n");

	dp_mst = container_of(mgr, struct dp_mst_private, mst_mgr);

	dp_display = dp_mst->dp_display;
	dev = dp_display->drm_dev;

	if (port->input || port->mstb)
		enc_idx = MAX_DP_MST_DRM_BRIDGES;
	else {
		/* if port is already reserved, return immediately */
		connector = dp_mst_find_fixed_connector(dp_mst, port);
		if (connector != NULL)
			return connector;

		/* first available bridge index for non-reserved port */
		enc_idx = dp_mst_find_first_available_encoder_idx(dp_mst);
	}

	/* add normal connector */
	connector = dp_mst_add_connector(mgr, port, pathprop);
	if (!connector) {
		DP_MST_DEBUG("failed to add connector\n");
		return NULL;
	}

	drm_modeset_lock_all(dev);

	/* clear encoder list */
	for (i = 0; i < DRM_CONNECTOR_MAX_ENCODER; i++)
		connector->encoder_ids[i] = 0;

	/* re-attach encoders from first available encoders */
	for (i = enc_idx; i < MAX_DP_MST_DRM_BRIDGES; i++)
		drm_connector_attach_encoder(connector,
				dp_mst->mst_bridge[i].encoder);

	drm_modeset_unlock_all(dev);

	DP_MST_DEBUG("add mst connector:%d\n", connector->base.id);

	return connector;
}

static void dp_mst_register_fixed_connector(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display *dp_display = c_conn->display;
	struct dp_mst_private *dp_mst = dp_display->dp_mst_prv_info;
	int i;

	DP_MST_DEBUG("enter\n");

	/* skip connector registered for fixed topology ports */
	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (dp_mst->mst_bridge[i].fixed_connector == connector) {
			DP_MST_DEBUG("found fixed connector %d\n",
					DRMID(connector));
			return;
		}
	}

	dp_mst_register_connector(connector);
}

static void dp_mst_destroy_fixed_connector(struct drm_dp_mst_topology_mgr *mgr,
					   struct drm_connector *connector)
{
	struct dp_mst_private *dp_mst;
	int i;

	DP_MST_DEBUG("enter\n");

	dp_mst = container_of(mgr, struct dp_mst_private, mst_mgr);

	/* skip connector destroy for fixed topology ports */
	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (dp_mst->mst_bridge[i].fixed_connector == connector) {
			dp_mst->mst_bridge[i].fixed_port_added = false;
			DP_MST_DEBUG("destroy fixed connector %d\n",
					DRMID(connector));
			return;
		}
	}

	dp_mst_destroy_connector(mgr, connector);
}

static struct drm_connector *
dp_mst_drm_fixed_connector_init(struct dp_display *dp_display,
			struct drm_encoder *encoder)
{
	static const struct sde_connector_ops dp_mst_connector_ops = {
		.post_init  = NULL,
		.detect     = dp_mst_fixed_connector_detect,
		.get_modes  = dp_mst_connector_get_modes,
		.mode_valid = dp_mst_connector_mode_valid,
		.get_info   = dp_mst_connector_get_info,
		.get_mode_info  = dp_mst_connector_get_mode_info,
		.atomic_best_encoder = dp_mst_fixed_atomic_best_encoder,
		.atomic_check = dp_mst_connector_atomic_check,
		.config_hdr = dp_mst_connector_config_hdr,
		.pre_destroy = dp_mst_connector_pre_destroy,
	};
	struct drm_device *dev;
	struct drm_connector *connector;
	int rc;

	DP_MST_DEBUG("enter\n");

	dev = dp_display->drm_dev;

	connector = sde_connector_init(dev,
				encoder,
				NULL,
				dp_display,
				&dp_mst_connector_ops,
				DRM_CONNECTOR_POLL_HPD,
				DRM_MODE_CONNECTOR_DisplayPort);

	if (!connector) {
		DP_ERR("mst sde_connector_init failed\n");
		return NULL;
	}

	rc = dp_display->mst_connector_install(dp_display, connector);
	if (rc) {
		DP_ERR("mst connector install failed\n");
		sde_connector_destroy(connector);
		return NULL;
	}

	drm_object_attach_property(&connector->base,
			dev->mode_config.path_property, 0);
	drm_object_attach_property(&connector->base,
			dev->mode_config.tile_property, 0);

	DP_MST_DEBUG("add mst fixed connector:%d\n", connector->base.id);

	return connector;
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

	DP_MST_INFO_LOG("mst hot plug event\n");
}

static void dp_mst_hpd_event_notify(struct dp_mst_private *mst, bool hpd_status)
{
	struct drm_device *dev = mst->dp_display->drm_dev;
	char event_string[] = "MST_HOTPLUG=1";
	char status[HPD_STRING_SIZE];
	char *envp[3];

	if (hpd_status)
		snprintf(status, HPD_STRING_SIZE, "status=connected");
	else
		snprintf(status, HPD_STRING_SIZE, "status=disconnected");

	envp[0] = event_string;
	envp[1] = status;
	envp[2] = NULL;

	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE, envp);

	DP_MST_INFO_LOG("%s finished\n", __func__);
}

/* DP Driver Callback OPs */

static void dp_mst_display_hpd(void *dp_display, bool hpd_status,
		struct dp_mst_hpd_info *info)
{
	int rc;
	struct dp_display *dp = dp_display;
	struct dp_mst_private *mst = dp->dp_mst_prv_info;

	mutex_lock(&mst->mst_lock);
	mst->mst_session_state = hpd_status;
	mutex_unlock(&mst->mst_lock);

	if (!hpd_status) {
		rc = mst->mst_fw_cbs->topology_mgr_set_mst(&mst->mst_mgr,
				hpd_status);
		if (rc < 0)
			goto fail;
	}

	if (info && !info->mst_protocol) {
		if (hpd_status) {
			mst->simulator.edid = (struct edid *)info->edid;
			mst->simulator.port_cnt = info->mst_port_cnt;
		}
		mst->mst_fw_cbs = &drm_dp_sim_mst_fw_helper_ops;
	} else {
		mst->mst_fw_cbs = &drm_dp_mst_fw_helper_ops;
	}

	if (hpd_status) {
		rc = mst->mst_fw_cbs->topology_mgr_set_mst(&mst->mst_mgr,
				hpd_status);
		if (rc < 0)
			goto fail;
	}

	dp_mst_hpd_event_notify(mst, hpd_status);

	DP_MST_INFO_LOG("mst display hpd success. hpd:%d, rc:%d\n", hpd_status,
			rc);
	return;
fail:
	DRM_ERROR("mst display hpd failed. hpd: %d, rc: %d\n",
			hpd_status, rc);
}

static void dp_mst_display_hpd_irq(void *dp_display,
			struct dp_mst_hpd_info *info)
{
	int rc;
	struct dp_display *dp = dp_display;
	struct dp_mst_private *mst = dp->dp_mst_prv_info;
	u8 esi[14];
	unsigned int esi_res = DP_SINK_COUNT_ESI + 1;
	bool handled;

	if (info->mst_hpd_sim) {
		if (info->mst_sim_add_con || info->mst_sim_remove_con) {
			dp_mst_sim_handle_hpd_irq(dp_display, info);

			/*
			 * When removing a connector, hpd_irq -> sim_destroy ->
			 * destroy_connector_work will be executed in a thread.
			 * This thread will perform the dp_mst_hotplug at the
			 * appropriate time. Do not perform hotplug here
			 * because it may be too early.
			 */
			if (info->mst_sim_remove_con)
				return;
		}

		dp_mst_hotplug(&mst->mst_mgr);
		return;
	}

	if (!mst->mst_session_state) {
		DP_ERR("mst_hpd_irq received before mst session start\n");
		return;
	}

	rc = drm_dp_dpcd_read(mst->caps.drm_aux, DP_SINK_COUNT_ESI,
		esi, 14);
	if (rc != 14) {
		DP_ERR("dpcd sink status read failed, rlen=%d\n", rc);
		return;
	}

	DP_MST_DEBUG("mst irq: esi1[0x%x] esi2[0x%x] esi3[%x]\n",
			esi[1], esi[2], esi[3]);

	rc = drm_dp_mst_hpd_irq(&mst->mst_mgr, esi, &handled);

	/* ack the request */
	if (handled) {
		rc = drm_dp_dpcd_write(mst->caps.drm_aux, esi_res, &esi[1], 3);

		if (rc != 3)
			DP_ERR("dpcd esi_res failed. rlen=%d\n", rc);
	}

	DP_MST_DEBUG("mst display hpd_irq handled:%d rc:%d\n", handled, rc);
}

static void dp_mst_set_state(void *dp_display, enum dp_drv_state mst_state)
{
	struct dp_display *dp = dp_display;
	struct dp_mst_private *mst = dp->dp_mst_prv_info;

	if (!mst) {
		DP_DEBUG("mst not initialized\n");
		return;
	}

	mst->state = mst_state;
	DP_MST_INFO_LOG("mst power state:%d\n", mst_state);
}

/* DP MST APIs */

static const struct dp_mst_drm_cbs dp_mst_display_cbs = {
	.hpd = dp_mst_display_hpd,
	.hpd_irq = dp_mst_display_hpd_irq,
	.set_drv_state = dp_mst_set_state,
};

static const struct drm_dp_mst_topology_cbs dp_mst_drm_cbs = {
	.add_connector = dp_mst_add_connector,
	.register_connector = dp_mst_register_connector,
	.destroy_connector = dp_mst_destroy_connector,
	.hotplug = dp_mst_hotplug,
};

static const struct drm_dp_mst_topology_cbs dp_mst_fixed_drm_cbs = {
	.add_connector = dp_mst_add_fixed_connector,
	.register_connector = dp_mst_register_fixed_connector,
	.destroy_connector = dp_mst_destroy_fixed_connector,
	.hotplug = dp_mst_hotplug,
};

static void dp_mst_sim_init(struct dp_mst_private *mst)
{
	INIT_WORK(&mst->simulator.probe_work, dp_mst_sim_link_probe_work);
	mst->simulator.cbs = &dp_mst_drm_cbs;
}

int dp_mst_init(struct dp_display *dp_display)
{
	struct drm_device *dev;
	int conn_base_id = 0;
	int ret, i;
	struct dp_mst_drm_install_info install_info;

	memset(&dp_mst, 0, sizeof(dp_mst));

	if (!dp_display) {
		DP_ERR("invalid params\n");
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

	mutex_init(&dp_mst.mst_lock);

	ret = drm_dp_mst_topology_mgr_init(&dp_mst.mst_mgr, dev,
					dp_mst.caps.drm_aux,
					dp_mst.caps.max_dpcd_transaction_bytes,
					dp_mst.caps.max_streams_supported,
					conn_base_id);
	if (ret) {
		DP_ERR("dp drm mst topology manager init failed\n");
		goto error;
	}

	dp_mst_sim_init(&dp_mst);

	dp_mst.mst_initialized = true;

	/* create drm_bridges for cached mst encoders and clear cache */
	for (i = 0; i < dp_mst_enc_cache.cnt; i++) {
		ret = dp_mst_drm_bridge_init(dp_display,
				dp_mst_enc_cache.mst_enc[i]);
	}
	memset(&dp_mst_enc_cache, 0, sizeof(dp_mst_enc_cache));

	/* choose fixed callback function if fixed topology is found */
	if (!dp_display->mst_get_fixed_topology_port(dp_display, 0, NULL))
		dp_mst.mst_mgr.cbs = &dp_mst_fixed_drm_cbs;

	DP_MST_INFO_LOG("dp drm mst topology manager init completed\n");

	return ret;

error:
	mutex_destroy(&dp_mst.mst_lock);
	return ret;
}

void dp_mst_deinit(struct dp_display *dp_display)
{
	struct dp_mst_private *mst;

	if (!dp_display) {
		DP_ERR("invalid params\n");
		return;
	}

	mst = dp_display->dp_mst_prv_info;

	if (!mst->mst_initialized)
		return;

	dp_display->mst_uninstall(dp_display);

	drm_dp_mst_topology_mgr_destroy(&mst->mst_mgr);

	dp_mst.mst_initialized = false;

	mutex_destroy(&mst->mst_lock);

	DP_MST_INFO_LOG("dp drm mst topology manager deinit completed\n");
}

