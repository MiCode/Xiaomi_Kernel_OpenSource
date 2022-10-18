// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/debugfs.h>

#include "sde_vbif.h"
#include "sde_hw_vbif.h"
#include "sde_trace.h"
#include "sde_rotator_vbif.h"

#define MAX_XIN_CLIENT	16

#define VBIF_CLK_CLIENT(x) sde_kms->vbif_clk_clients[x]
#define VBIF_CLK_CLIENT_NAME(x) sde_clk_ctrl_type_s[x]

int sde_vbif_clk_register(struct sde_kms *sde_kms, struct sde_vbif_clk_client *client)
{
	enum sde_clk_ctrl_type clk_ctrl;

	if (!sde_kms || !client)
		return -EINVAL;

	clk_ctrl = client->clk_ctrl;
	if (!SDE_CLK_CTRL_VALID(clk_ctrl)) {
		SDE_ERROR("Invalid clock ctrl type %d", clk_ctrl);
		return -EINVAL;
	}

	VBIF_CLK_CLIENT(clk_ctrl).hw = client->hw;
	VBIF_CLK_CLIENT(clk_ctrl).clk_ctrl = clk_ctrl;
	memcpy(&VBIF_CLK_CLIENT(clk_ctrl).ops, &client->ops, sizeof(struct sde_vbif_clk_ops));

	SDE_DEBUG("registering hw:%pK clk_ctrl:%s\n", client->hw, VBIF_CLK_CLIENT_NAME(clk_ctrl));

	return 0;
}

/**
 * _sde_vbif_setup_clk_supported - check if VBIF setup_clk_force_ctrl API is supported
 * @sde_kms:	Pointer to sde_kms object
 * @clk_ctrl:	clock to be controlled
 * @return:	true if client is supported, otherwise false
 */
static bool _sde_vbif_setup_clk_supported(struct sde_kms *sde_kms, enum sde_clk_ctrl_type clk_ctrl)
{
	bool supported = false;
	bool has_split_vbif = sde_kms->catalog->has_vbif_clk_split;

	if ((has_split_vbif && VBIF_CLK_CLIENT(clk_ctrl).ops.setup_clk_force_ctrl) ||
			(!has_split_vbif && sde_kms->hw_mdp->ops.setup_clk_force_ctrl))
		supported = true;

	SDE_DEBUG("split_vbif:%d type:%s supported:%d\n", has_split_vbif,
			VBIF_CLK_CLIENT_NAME(clk_ctrl), supported);

	return supported;
}

/**
 * _sde_vbif_get_clk_supported - check if VBIF get_clk_ctrl_status API is supported
 * @sde_kms:	Pointer to sde_kms object
 * @clk_ctrl:	clock to be controlled
 * @return:	true if client is supported, otherwise false
 */
static bool _sde_vbif_get_clk_supported(struct sde_kms *sde_kms, enum sde_clk_ctrl_type clk_ctrl)
{
	bool supported = false;
	bool has_split_vbif = sde_kms->catalog->has_vbif_clk_split;

	if ((has_split_vbif && VBIF_CLK_CLIENT(clk_ctrl).ops.get_clk_ctrl_status) ||
			(!has_split_vbif && sde_kms->hw_mdp->ops.get_clk_ctrl_status))
		supported = true;

	SDE_DEBUG("split_vbif:%d type:%s supported:%d\n", has_split_vbif,
			VBIF_CLK_CLIENT_NAME(clk_ctrl), supported);

	return supported;
}

/**
 * _sde_vbif_setup_clk_force_ctrl - set clock force control
 * @sde_kms:	Pointer to sde_kms object
 * @clk_ctrl:	clock to be controlled
 * @enable:	force on enable
 * @return:	if the clock is forced-on by this function
 */
static int _sde_vbif_setup_clk_force_ctrl(struct sde_kms *sde_kms, enum sde_clk_ctrl_type clk_ctrl,
		bool enable)
{
	int rc = 0;
	struct sde_hw_blk_reg_map *hw = VBIF_CLK_CLIENT(clk_ctrl).hw;
	bool has_split_vbif = sde_kms->catalog->has_vbif_clk_split;

	if (has_split_vbif)
		rc = VBIF_CLK_CLIENT(clk_ctrl).ops.setup_clk_force_ctrl(hw, clk_ctrl, enable);
	else
		rc = sde_kms->hw_mdp->ops.setup_clk_force_ctrl(sde_kms->hw_mdp, clk_ctrl, enable);

	SDE_DEBUG("split_vbif:%d type:%s en:%d rc:%d\n", has_split_vbif,
			VBIF_CLK_CLIENT_NAME(clk_ctrl), enable, rc);

	return rc;
}

/**
 * _sde_vbif_get_clk_ctrl_status - get clock control status
 * @sde_kms:	Pointer to sde_kms object
 * @clk_ctrl:	clock to be controlled
 * @return:	clock status if success, otherwise return code
 */
static int _sde_vbif_get_clk_ctrl_status(struct sde_kms *sde_kms, enum sde_clk_ctrl_type clk_ctrl)
{
	int rc = 0;
	struct sde_hw_blk_reg_map *hw = VBIF_CLK_CLIENT(clk_ctrl).hw;
	bool has_split_vbif = sde_kms->catalog->has_vbif_clk_split;

	if (has_split_vbif)
		rc = VBIF_CLK_CLIENT(clk_ctrl).ops.get_clk_ctrl_status(hw, clk_ctrl);
	else
		rc = sde_kms->hw_mdp->ops.get_clk_ctrl_status(sde_kms->hw_mdp, clk_ctrl);

	SDE_DEBUG("split_vbif:%d type:%s status:%d rc:%d\n", has_split_vbif,
			VBIF_CLK_CLIENT_NAME(clk_ctrl), rc);

	return rc;
}

/**
 * _sde_vbif_wait_for_xin_halt - wait for the xin to halt
 * @vbif:	Pointer to hardware vbif driver
 * @xin_id:	Client interface identifier
 * @return:	0 if success; error code otherwise
 */
static int _sde_vbif_wait_for_xin_halt(struct sde_hw_vbif *vbif, u32 xin_id)
{
	ktime_t timeout;
	bool status;
	int rc;

	if (!vbif || !vbif->cap || !vbif->ops.get_xin_halt_status) {
		SDE_ERROR("invalid arguments vbif %d\n", !vbif);
		return -EINVAL;
	}

	timeout = ktime_add_us(ktime_get(), vbif->cap->xin_halt_timeout);
	for (;;) {
		status = vbif->ops.get_xin_halt_status(vbif, xin_id);
		if (status)
			break;
		if (ktime_compare_safe(ktime_get(), timeout) > 0) {
			status = vbif->ops.get_xin_halt_status(vbif, xin_id);
			break;
		}
		usleep_range(501, 1000);
	}

	if (!status) {
		rc = -ETIMEDOUT;
		SDE_ERROR("VBIF %d client %d not halting. TIMEDOUT.\n",
				vbif->idx - VBIF_0, xin_id);
	} else {
		rc = 0;
		SDE_DEBUG("VBIF %d client %d is halted\n",
				vbif->idx - VBIF_0, xin_id);
	}

	return rc;
}

static int _sde_vbif_wait_for_axi_halt(struct sde_hw_vbif *vbif)
{
	int rc;

	if (!vbif || !vbif->cap || !vbif->ops.get_axi_halt_status) {
		SDE_ERROR("invalid arguments vbif %d\n", !vbif);
		return -EINVAL;
	}

	rc = vbif->ops.get_axi_halt_status(vbif);
	if (rc)
		SDE_ERROR("VBIF %d AXI port(s) not halting. TIMEDOUT.\n",
				vbif->idx - VBIF_0);
	else
		SDE_DEBUG("VBIF %d AXI port(s) halted\n",
				vbif->idx - VBIF_0);

	return rc;
}

int sde_vbif_halt_plane_xin(struct sde_kms *sde_kms, u32 xin_id, u32 clk_ctrl)
{
	struct sde_hw_vbif *vbif = NULL;
	struct sde_hw_mdp *mdp;
	bool forced_on = false;
	bool status;
	int rc = 0;

	if (!sde_kms) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}

	if (!sde_kms_is_vbif_operation_allowed(sde_kms)) {
		SDE_DEBUG("vbif operations not permitted\n");
		return 0;
	}

	vbif = sde_kms->hw_vbif[VBIF_RT];
	mdp = sde_kms->hw_mdp;
	if (!vbif || !mdp || !vbif->ops.get_xin_halt_status ||
		       !vbif->ops.set_xin_halt ||
		       !_sde_vbif_setup_clk_supported(sde_kms, clk_ctrl)) {
		SDE_ERROR("invalid vbif or mdp arguments\n");
		return -EINVAL;
	}

	mutex_lock(&vbif->mutex);

	SDE_EVT32_VERBOSE(vbif->idx, xin_id);

	/*
	 * If status is 0, then make sure client clock is not gated
	 * while halting by forcing it ON only if it was not previously
	 * forced on. If status is 1 then its already halted.
	 */
	status = vbif->ops.get_xin_halt_status(vbif, xin_id);
	if (status) {
		mutex_unlock(&vbif->mutex);
		return 0;
	}

	forced_on = _sde_vbif_setup_clk_force_ctrl(sde_kms, clk_ctrl, true);

	/* send halt request for unused plane's xin client */
	vbif->ops.set_xin_halt(vbif, xin_id, true);

	rc = _sde_vbif_wait_for_xin_halt(vbif, xin_id);
	if (rc) {
		SDE_ERROR(
		"wait failed for pipe halt:xin_id %u, clk_ctrl %u, rc %u\n",
			xin_id, clk_ctrl, rc);
		SDE_EVT32(xin_id, clk_ctrl, rc, SDE_EVTLOG_ERROR);
	}

	/* open xin client to enable transactions */
	vbif->ops.set_xin_halt(vbif, xin_id, false);
	if (forced_on)
		_sde_vbif_setup_clk_force_ctrl(sde_kms, clk_ctrl, false);

	mutex_unlock(&vbif->mutex);

	return rc;
}

/**
 * _sde_vbif_apply_dynamic_ot_limit - determine OT based on usecase parameters
 * @vbif:	Pointer to hardware vbif driver
 * @ot_lim:	Pointer to OT limit to be modified
 * @params:	Pointer to usecase parameters
 */
static void _sde_vbif_apply_dynamic_ot_limit(struct sde_hw_vbif *vbif,
		u32 *ot_lim, struct sde_vbif_set_ot_params *params)
{
	u64 pps;
	const struct sde_vbif_dynamic_ot_tbl *tbl;
	u32 i;

	if (!vbif || !(vbif->cap->features & BIT(SDE_VBIF_QOS_OTLIM)))
		return;

	/* Dynamic OT setting done only for WFD */
	if (!params->is_wfd)
		return;

	pps = params->frame_rate;
	pps *= params->width;
	pps *= params->height;

	tbl = params->rd ? &vbif->cap->dynamic_ot_rd_tbl :
			&vbif->cap->dynamic_ot_wr_tbl;

	for (i = 0; i < tbl->count; i++) {
		if (pps <= tbl->cfg[i].pps) {
			*ot_lim = tbl->cfg[i].ot_limit;
			break;
		}
	}

	SDE_DEBUG("vbif:%d xin:%d w:%d h:%d fps:%d pps:%llu ot:%u\n",
			vbif->idx - VBIF_0, params->xin_id,
			params->width, params->height, params->frame_rate,
			pps, *ot_lim);
}

/**
 * _sde_vbif_get_ot_limit - get OT based on usecase & configuration parameters
 * @vbif:	Pointer to hardware vbif driver
 * @params:	Pointer to usecase parameters
 * @return:	OT limit
 */
static u32 _sde_vbif_get_ot_limit(struct sde_hw_vbif *vbif,
	struct sde_vbif_set_ot_params *params)
{
	u32 ot_lim = 0;
	u32 val;

	if (!vbif || !vbif->cap) {
		SDE_ERROR("invalid arguments vbif %d\n", !vbif);
		return -EINVAL;
	}

	if (vbif->cap->default_ot_wr_limit && !params->rd)
		ot_lim = vbif->cap->default_ot_wr_limit;
	else if (vbif->cap->default_ot_rd_limit && params->rd)
		ot_lim = vbif->cap->default_ot_rd_limit;

	/*
	 * If default ot is not set from dt/catalog,
	 * then do not configure it.
	 */
	if (ot_lim == 0)
		goto exit;

	/* Modify the limits if the target and the use case requires it */
	_sde_vbif_apply_dynamic_ot_limit(vbif, &ot_lim, params);

	if (vbif && vbif->ops.get_limit_conf) {
		val = vbif->ops.get_limit_conf(vbif,
				params->xin_id, params->rd);
		if (val == ot_lim)
			ot_lim = 0;
	}

exit:
	SDE_DEBUG("vbif:%d xin:%d ot_lim:%d\n",
			vbif->idx - VBIF_0, params->xin_id, ot_lim);
	return ot_lim;
}

/**
 * sde_vbif_set_ot_limit - set OT based on usecase & configuration parameters
 * @vbif:	Pointer to hardware vbif driver
 * @params:	Pointer to usecase parameters
 *
 * Note this function would block waiting for bus halt.
 */
void sde_vbif_set_ot_limit(struct sde_kms *sde_kms,
		struct sde_vbif_set_ot_params *params)
{
	struct sde_hw_vbif *vbif = NULL;
	struct sde_hw_mdp *mdp;
	bool forced_on = false;
	u32 ot_lim;
	int ret, i;

	if (!sde_kms || !params || params->clk_ctrl >= SDE_CLK_CTRL_MAX) {
		SDE_ERROR("invalid arguments\n");
		return;
	}

	if (!sde_kms_is_vbif_operation_allowed(sde_kms)) {
		SDE_DEBUG("vbif operations not permitted\n");
		return;
	}

	mdp = sde_kms->hw_mdp;

	for (i = 0; i < ARRAY_SIZE(sde_kms->hw_vbif); i++) {
		if (sde_kms->hw_vbif[i] &&
				sde_kms->hw_vbif[i]->idx == params->vbif_idx) {
			vbif = sde_kms->hw_vbif[i];
			break;
		}
	}

	if (!vbif || !mdp) {
		SDE_DEBUG("invalid arguments vbif %d mdp %d\n",
				vbif != NULL, mdp != NULL);
		return;
	}

	if (!_sde_vbif_setup_clk_supported(sde_kms, params->clk_ctrl) ||
			!vbif->ops.set_limit_conf ||
			!vbif->ops.set_xin_halt)
		return;

	mutex_lock(&vbif->mutex);

	SDE_EVT32_VERBOSE(vbif->idx, params->xin_id);

	/* set write_gather_en for all write clients */
	if (vbif->ops.set_write_gather_en && !params->rd)
		vbif->ops.set_write_gather_en(vbif, params->xin_id);

	ot_lim = _sde_vbif_get_ot_limit(vbif, params) & 0xFF;

	if (ot_lim == 0)
		goto exit;

	trace_sde_perf_set_ot(params->num, params->xin_id, ot_lim,
		params->vbif_idx);

	forced_on = _sde_vbif_setup_clk_force_ctrl(sde_kms, params->clk_ctrl, true);

	vbif->ops.set_limit_conf(vbif, params->xin_id, params->rd, ot_lim);

	vbif->ops.set_xin_halt(vbif, params->xin_id, true);

	ret = _sde_vbif_wait_for_xin_halt(vbif, params->xin_id);
	if (ret)
		SDE_EVT32(vbif->idx, params->xin_id);

	vbif->ops.set_xin_halt(vbif, params->xin_id, false);

	if (forced_on)
		_sde_vbif_setup_clk_force_ctrl(sde_kms, params->clk_ctrl, false);

exit:
	mutex_unlock(&vbif->mutex);
}

void mdp_vbif_lock(struct platform_device *parent_pdev, bool enable)
{
	struct drm_device *ddev;
	struct sde_kms *sde_kms;
	struct sde_hw_vbif *vbif = NULL;
	int i;

	ddev = platform_get_drvdata(parent_pdev);
	if (!ddev || !ddev_to_msm_kms(ddev)) {
		SDE_ERROR("invalid drm device\n");
		return;
	}

	sde_kms = to_sde_kms(ddev_to_msm_kms(ddev));

	for (i = 0; i < ARRAY_SIZE(sde_kms->hw_vbif); i++) {
		if (sde_kms->hw_vbif[i] &&
				sde_kms->hw_vbif[i]->idx == VBIF_RT) {
			vbif = sde_kms->hw_vbif[i];
			break;
		}
	}

	if (!vbif) {
		SDE_DEBUG("invalid vbif structure\n");
		return;
	}

	if (enable)
		mutex_lock(&vbif->mutex);
	else
		mutex_unlock(&vbif->mutex);

}

bool sde_vbif_set_xin_halt(struct sde_kms *sde_kms,
		struct sde_vbif_set_xin_halt_params *params)
{
	struct sde_hw_vbif *vbif = NULL;
	struct sde_hw_mdp *mdp;
	bool forced_on = false;
	int ret, i;

	if (!sde_kms || !params || params->clk_ctrl >= SDE_CLK_CTRL_MAX) {
		SDE_ERROR("invalid arguments\n");
		return false;
	}

	if (!sde_kms_is_vbif_operation_allowed(sde_kms)) {
		SDE_DEBUG("vbif operations not permitted\n");
		return true;
	}

	mdp = sde_kms->hw_mdp;

	for (i = 0; i < ARRAY_SIZE(sde_kms->hw_vbif); i++) {
		if (sde_kms->hw_vbif[i] &&
				sde_kms->hw_vbif[i]->idx == params->vbif_idx) {
			vbif = sde_kms->hw_vbif[i];
			break;
		}
	}

	if (!vbif || !mdp) {
		SDE_DEBUG("invalid arguments vbif %d mdp %d\n",
				vbif != NULL, mdp != NULL);
		return false;
	}

	if (!_sde_vbif_setup_clk_supported(sde_kms, params->clk_ctrl) ||
			!vbif->ops.set_xin_halt)
		return false;

	mutex_lock(&vbif->mutex);

	SDE_EVT32_VERBOSE(vbif->idx, params->xin_id);

	if (params->enable) {
		forced_on = _sde_vbif_setup_clk_force_ctrl(sde_kms, params->clk_ctrl, true);

		vbif->ops.set_xin_halt(vbif, params->xin_id, true);

		ret = _sde_vbif_wait_for_xin_halt(vbif, params->xin_id);
		if (ret)
			SDE_EVT32(vbif->idx, params->xin_id, SDE_EVTLOG_ERROR);
	} else {
		vbif->ops.set_xin_halt(vbif, params->xin_id, false);

		if (params->forced_on)
			_sde_vbif_setup_clk_force_ctrl(sde_kms, params->clk_ctrl, false);
	}

	mutex_unlock(&vbif->mutex);

	return forced_on;
}

bool sde_vbif_get_xin_status(struct sde_kms *sde_kms,
		struct sde_vbif_get_xin_status_params *params)
{
	struct sde_hw_vbif *vbif = NULL;
	struct sde_hw_mdp *mdp;
	int i, rc;
	bool status;

	if (!sde_kms || !params) {
		SDE_ERROR("invalid arguments\n");
		return false;
	}

	if (!sde_kms_is_vbif_operation_allowed(sde_kms)) {
		SDE_DEBUG("vbif operations not permitted\n");
		return true;
	}

	mdp = sde_kms->hw_mdp;

	for (i = 0; i < ARRAY_SIZE(sde_kms->hw_vbif); i++) {
		if (sde_kms->hw_vbif[i] &&
				sde_kms->hw_vbif[i]->idx == params->vbif_idx) {
			vbif = sde_kms->hw_vbif[i];
			break;
		}
	}

	if (!vbif || !mdp) {
		SDE_DEBUG("invalid arguments vbif:%d mdp:%d vbif idx:%d\n",
				vbif != NULL, mdp != NULL, params->vbif_idx);
		return false;
	}

	if (!_sde_vbif_get_clk_supported(sde_kms, params->clk_ctrl) ||
			!vbif->ops.get_xin_halt_status)
		return false;

	mutex_lock(&vbif->mutex);
	SDE_EVT32_VERBOSE(vbif->idx, params->xin_id);
	status = vbif->ops.get_xin_halt_status(vbif, params->xin_id);
	if (status) {
		rc = _sde_vbif_get_clk_ctrl_status(sde_kms, params->clk_ctrl);
		status = (rc < 0) ? false : !rc;
	}
	mutex_unlock(&vbif->mutex);

	return status;
}

void sde_vbif_set_qos_remap(struct sde_kms *sde_kms,
		struct sde_vbif_set_qos_params *params)
{
	struct sde_hw_vbif *vbif = NULL;
	struct sde_hw_mdp *mdp;
	bool forced_on = false;
	const struct sde_vbif_qos_tbl *qos_tbl;
	int i;

	if (!sde_kms || !params || !sde_kms->hw_mdp || params->clk_ctrl >= SDE_CLK_CTRL_MAX) {
		SDE_ERROR("invalid arguments\n");
		return;
	}

	if (!sde_kms_is_vbif_operation_allowed(sde_kms)) {
		SDE_DEBUG("vbif operations not permitted\n");
		return;
	}

	mdp = sde_kms->hw_mdp;

	for (i = 0; i < ARRAY_SIZE(sde_kms->hw_vbif); i++) {
		if (sde_kms->hw_vbif[i] &&
				sde_kms->hw_vbif[i]->idx == params->vbif_idx) {
			vbif = sde_kms->hw_vbif[i];
			break;
		}
	}

	if (!vbif || !vbif->cap) {
		SDE_ERROR("invalid vbif %d\n", params->vbif_idx);
		return;
	}

	if (!vbif->ops.set_qos_remap || !_sde_vbif_setup_clk_supported(sde_kms, params->clk_ctrl)) {
		SDE_DEBUG("qos remap not supported\n");
		return;
	}

	if (params->client_type > VBIF_MAX_CLIENT) {
		SDE_ERROR("invalid client type:%d\n", params->client_type);
		return;
	}

	qos_tbl = &vbif->cap->qos_tbl[params->client_type];
	if (!qos_tbl->npriority_lvl || !qos_tbl->priority_lvl) {
		SDE_DEBUG("qos tbl not defined\n");
		return;
	}

	mutex_lock(&vbif->mutex);

	forced_on = _sde_vbif_setup_clk_force_ctrl(sde_kms, params->clk_ctrl, true);

	for (i = 0; i < qos_tbl->npriority_lvl; i++) {
		SDE_DEBUG("vbif:%d xin:%d lvl:%d/%d\n",
				params->vbif_idx, params->xin_id, i,
				qos_tbl->priority_lvl[i]);
		vbif->ops.set_qos_remap(vbif, params->xin_id, i,
				qos_tbl->priority_lvl[i]);
	}

	if (forced_on)
		_sde_vbif_setup_clk_force_ctrl(sde_kms, params->clk_ctrl, false);

	mutex_unlock(&vbif->mutex);
}

void sde_vbif_clear_errors(struct sde_kms *sde_kms)
{
	struct sde_hw_vbif *vbif;
	u32 i, pnd, src;

	if (!sde_kms) {
		SDE_ERROR("invalid argument\n");
		return;
	}

	if (!sde_kms_is_vbif_operation_allowed(sde_kms)) {
		SDE_DEBUG("vbif operations not permitted\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(sde_kms->hw_vbif); i++) {
		vbif = sde_kms->hw_vbif[i];
		if (vbif && vbif->ops.clear_errors) {
			mutex_lock(&vbif->mutex);
			vbif->ops.clear_errors(vbif, &pnd, &src);
			if (pnd || src) {
				SDE_EVT32(i, pnd, src);
				SDE_DEBUG("VBIF %d: pnd 0x%X, src 0x%X\n",
						vbif->idx - VBIF_0, pnd, src);
			}
			mutex_unlock(&vbif->mutex);
		}
	}
}

void sde_vbif_init_memtypes(struct sde_kms *sde_kms)
{
	struct sde_hw_vbif *vbif;
	int i, j;

	if (!sde_kms) {
		SDE_ERROR("invalid argument\n");
		return;
	}

	if (!sde_kms_is_vbif_operation_allowed(sde_kms)) {
		SDE_DEBUG("vbif operations not permitted\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(sde_kms->hw_vbif); i++) {
		vbif = sde_kms->hw_vbif[i];
		if (vbif && vbif->cap && vbif->ops.set_mem_type) {
			mutex_lock(&vbif->mutex);
			for (j = 0; j < vbif->cap->memtype_count; j++)
				vbif->ops.set_mem_type(
						vbif, j, vbif->cap->memtype[j]);
			mutex_unlock(&vbif->mutex);
		}
	}
}

void sde_vbif_axi_halt_request(struct sde_kms *sde_kms)
{
	struct sde_hw_vbif *vbif;
	int i;

	if (!sde_kms) {
		SDE_ERROR("invalid argument\n");
		return;
	}

	if (!sde_kms_is_vbif_operation_allowed(sde_kms)) {
		SDE_DEBUG("vbif operations not permitted\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(sde_kms->hw_vbif); i++) {
		vbif = sde_kms->hw_vbif[i];
		if (vbif && vbif->cap && vbif->ops.set_axi_halt) {
			mutex_lock(&vbif->mutex);
			vbif->ops.set_axi_halt(vbif);
			_sde_vbif_wait_for_axi_halt(vbif);
			mutex_unlock(&vbif->mutex);
		}
	}
}

int sde_vbif_halt_xin_mask(struct sde_kms *sde_kms, u32 xin_id_mask,
				bool halt)
{
	struct sde_hw_vbif *vbif;
	int i = 0, status, rc;

	if (!sde_kms) {
		SDE_ERROR("invalid argument\n");
		return -EINVAL;
	}

	vbif = sde_kms->hw_vbif[VBIF_RT];

	if (!vbif->ops.get_xin_halt_status || !vbif->ops.set_xin_halt)
		return 0;

	SDE_EVT32(xin_id_mask, halt);

	for (i = 0; i < MAX_XIN_CLIENT; i++) {
		if (xin_id_mask & BIT(i)) {
			/* unhalt the xin-clients */
			if (!halt) {
				vbif->ops.set_xin_halt(vbif, i, false);
				continue;
			}

			status = vbif->ops.get_xin_halt_status(vbif, i);
			if (status)
				continue;

			/* halt xin-clients and wait for ack */
			vbif->ops.set_xin_halt(vbif, i, true);

			rc = _sde_vbif_wait_for_xin_halt(vbif, i);
			if (rc) {
				SDE_ERROR("xin_halt failed for xin:%d, rc:%d\n",
					i, rc);
				SDE_EVT32(xin_id_mask, i, rc, SDE_EVTLOG_ERROR);
				return rc;
			}
		}
	}

	return 0;
}

#ifdef CONFIG_DEBUG_FS
void sde_debugfs_vbif_destroy(struct sde_kms *sde_kms)
{
	debugfs_remove_recursive(sde_kms->debugfs_vbif);
	sde_kms->debugfs_vbif = NULL;
}

int sde_debugfs_vbif_init(struct sde_kms *sde_kms, struct dentry *debugfs_root)
{
	char vbif_name[32];
	struct dentry *debugfs_vbif;
	int i, j;

	sde_kms->debugfs_vbif = debugfs_create_dir("vbif", debugfs_root);
	if (!sde_kms->debugfs_vbif) {
		SDE_ERROR("failed to create vbif debugfs\n");
		return -EINVAL;
	}

	for (i = 0; i < sde_kms->catalog->vbif_count; i++) {
		struct sde_vbif_cfg *vbif = &sde_kms->catalog->vbif[i];

		snprintf(vbif_name, sizeof(vbif_name), "%d", vbif->id);

		debugfs_vbif = debugfs_create_dir(vbif_name,
				sde_kms->debugfs_vbif);

		debugfs_create_u32("features", 0400, debugfs_vbif,
			(u32 *)&vbif->features);

		debugfs_create_u32("xin_halt_timeout", 0400, debugfs_vbif,
			(u32 *)&vbif->xin_halt_timeout);

		debugfs_create_u32("default_rd_ot_limit", 0400, debugfs_vbif,
			(u32 *)&vbif->default_ot_rd_limit);

		debugfs_create_u32("default_wr_ot_limit", 0400, debugfs_vbif,
			(u32 *)&vbif->default_ot_wr_limit);

		for (j = 0; j < vbif->dynamic_ot_rd_tbl.count; j++) {
			struct sde_vbif_dynamic_ot_cfg *cfg =
					&vbif->dynamic_ot_rd_tbl.cfg[j];

			snprintf(vbif_name, sizeof(vbif_name),
					"dynamic_ot_rd_%d_pps", j);
			debugfs_create_u64(vbif_name, 0400, debugfs_vbif,
					(u64 *)&cfg->pps);
			snprintf(vbif_name, sizeof(vbif_name),
					"dynamic_ot_rd_%d_ot_limit", j);
			debugfs_create_u32(vbif_name, 0400, debugfs_vbif,
					(u32 *)&cfg->ot_limit);
		}

		for (j = 0; j < vbif->dynamic_ot_wr_tbl.count; j++) {
			struct sde_vbif_dynamic_ot_cfg *cfg =
					&vbif->dynamic_ot_wr_tbl.cfg[j];

			snprintf(vbif_name, sizeof(vbif_name),
					"dynamic_ot_wr_%d_pps", j);
			debugfs_create_u64(vbif_name, 0400, debugfs_vbif,
					(u64 *)&cfg->pps);
			snprintf(vbif_name, sizeof(vbif_name),
					"dynamic_ot_wr_%d_ot_limit", j);
			debugfs_create_u32(vbif_name, 0400, debugfs_vbif,
					(u32 *)&cfg->ot_limit);
		}
	}

	return 0;
}
#endif
