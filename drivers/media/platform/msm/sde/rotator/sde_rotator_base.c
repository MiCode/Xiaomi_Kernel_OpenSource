/* Copyright (c) 2012, 2015-2019, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/file.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/major.h>
#include <linux/debugfs.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/regulator/consumer.h>

#define CREATE_TRACE_POINTS
#include "sde_rotator_base.h"
#include "sde_rotator_util.h"
#include "sde_rotator_trace.h"
#include "sde_rotator_debug.h"
#include "sde_rotator_dev.h"
#include "sde_rotator_vbif.h"

static inline u64 fudge_factor(u64 val, u32 numer, u32 denom)
{
	u64 result = (val * (u64)numer);

	do_div(result, denom);
	return result;
}

static inline u64 apply_fudge_factor(u64 val,
	struct sde_mult_factor *factor)
{
	return fudge_factor(val, factor->numer, factor->denom);
}

static inline u64 apply_inverse_fudge_factor(u64 val,
	struct sde_mult_factor *factor)
{
	return fudge_factor(val, factor->denom, factor->numer);
}

static inline bool validate_comp_ratio(struct sde_mult_factor *factor)
{
	return factor->numer && factor->denom;
}

u32 sde_apply_comp_ratio_factor(u32 quota,
	struct sde_mdp_format_params *fmt,
	struct sde_mult_factor *factor)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();

	if (!mdata || !test_bit(SDE_QOS_OVERHEAD_FACTOR,
		      mdata->sde_qos_map))
		return quota;

	/* apply compression ratio, only for compressed formats */
	if (sde_mdp_is_ubwc_format(fmt) &&
	    validate_comp_ratio(factor))
		quota = apply_inverse_fudge_factor(quota, factor);

	return quota;
}

#define RES_1080p		(1088*1920)
#define RES_UHD		(3840*2160)
#define RES_WQXGA	(2560*1600)
#define XIN_HALT_TIMEOUT_US	0x4000

static int sde_mdp_wait_for_xin_halt(u32 xin_id)
{
	void __iomem *vbif_base;
	u32 status;
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	u32 idle_mask = BIT(xin_id);
	int rc;

	vbif_base = mdata->vbif_nrt_io.base;

	rc = readl_poll_timeout(vbif_base + MMSS_VBIF_XIN_HALT_CTRL1,
		status, (status & idle_mask),
		1000, XIN_HALT_TIMEOUT_US);
	if (rc == -ETIMEDOUT) {
		SDEROT_ERR("VBIF client %d not halting. TIMEDOUT.\n",
			xin_id);
	} else {
		SDEROT_DBG("VBIF client %d is halted\n", xin_id);
	}

	return rc;
}

/**
 * force_on_xin_clk() - enable/disable the force-on for the pipe clock
 * @bit_off: offset of the bit to enable/disable the force-on.
 * @reg_off: register offset for the clock control.
 * @enable: boolean to indicate if the force-on of the clock needs to be
 * enabled or disabled.
 *
 * This function returns:
 * true - if the clock is forced-on by this function
 * false - if the clock was already forced on
 * It is the caller responsibility to check if this function is forcing
 * the clock on; if so, it will need to remove the force of the clock,
 * otherwise it should avoid to remove the force-on.
 * Clocks must be on when calling this function.
 */
static bool force_on_xin_clk(u32 bit_off, u32 clk_ctl_reg_off, bool enable)
{
	u32 val;
	u32 force_on_mask;
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	bool clk_forced_on = false;

	force_on_mask = BIT(bit_off);
	val = readl_relaxed(mdata->mdp_base + clk_ctl_reg_off);

	clk_forced_on = !(force_on_mask & val);

	if (true == enable)
		val |= force_on_mask;
	else
		val &= ~force_on_mask;

	writel_relaxed(val, mdata->mdp_base + clk_ctl_reg_off);

	return clk_forced_on;
}

void vbif_lock(struct platform_device *parent_pdev)
{
	if (!parent_pdev)
		return;

	mdp_vbif_lock(parent_pdev, true);
}

void vbif_unlock(struct platform_device *parent_pdev)
{
	if (!parent_pdev)
		return;

	mdp_vbif_lock(parent_pdev, false);
}

void sde_mdp_halt_vbif_xin(struct sde_mdp_vbif_halt_params *params)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	u32 reg_val;
	bool forced_on;
	int rc = 0;

	if (!mdata || !params || !params->reg_off_mdp_clk_ctrl) {
		SDEROT_ERR("null input parameter\n");
		return;
	}

	if (!mdata->parent_pdev &&
		params->xin_id > MMSS_VBIF_NRT_VBIF_CLK_FORCE_CTRL0_XIN1) {
		SDEROT_ERR("xin_id:%d exceed max limit\n", params->xin_id);
		return;
	}

	forced_on = force_on_xin_clk(params->bit_off_mdp_clk_ctrl,
		params->reg_off_mdp_clk_ctrl, true);

	vbif_lock(mdata->parent_pdev);

	SDEROT_EVTLOG(forced_on, params->xin_id);

	reg_val = SDE_VBIF_READ(mdata, MMSS_VBIF_XIN_HALT_CTRL0);
	SDE_VBIF_WRITE(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
		reg_val | BIT(params->xin_id));

	/* this is a polling operation */
	rc = sde_mdp_wait_for_xin_halt(params->xin_id);
	if (rc == -ETIMEDOUT)
		params->xin_timeout = BIT(params->xin_id);

	reg_val = SDE_VBIF_READ(mdata, MMSS_VBIF_XIN_HALT_CTRL0);
	SDE_VBIF_WRITE(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
		reg_val & ~BIT(params->xin_id));

	vbif_unlock(mdata->parent_pdev);

	if (forced_on)
		force_on_xin_clk(params->bit_off_mdp_clk_ctrl,
			params->reg_off_mdp_clk_ctrl, false);
}

u32 sde_mdp_get_ot_limit(u32 width, u32 height, u32 pixfmt, u32 fps, u32 is_rd)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_mdp_format_params *fmt;
	u32 ot_lim;
	u32 is_yuv;
	u64 res;

	ot_lim = (is_rd) ? mdata->default_ot_rd_limit :
				mdata->default_ot_wr_limit;

	/*
	 * If default ot is not set from dt,
	 * then do not configure it.
	 */
	if (ot_lim == 0)
		goto exit;

	/* Modify the limits if the target and the use case requires it */
	if (false == test_bit(SDE_QOS_OTLIM, mdata->sde_qos_map))
		goto exit;

	width = min_t(u32, width, SDE_ROT_MAX_IMG_WIDTH);
	height = min_t(u32, height, SDE_ROT_MAX_IMG_HEIGHT);

	res = width * height;
	res = res * fps;

	fmt = sde_get_format_params(pixfmt);

	if (!fmt) {
		SDEROT_WARN("invalid format %8.8x\n", pixfmt);
		goto exit;
	}

	is_yuv = sde_mdp_is_yuv_format(fmt);

	SDEROT_DBG("w:%d h:%d fps:%d pixfmt:%8.8x yuv:%d res:%llu rd:%d\n",
		width, height, fps, pixfmt, is_yuv, res, is_rd);

	/*
	 * If (total_source_pixels <= 62208000  && YUV) -> RD/WROT=2 //1080p30
	 * If (total_source_pixels <= 124416000 && YUV) -> RD/WROT=4 //1080p60
	 * If (total_source_pixels <= 2160p && YUV && FPS <= 30) -> RD/WROT = 32
	 */
	switch (mdata->mdss_version) {
	case SDE_MDP_HW_REV_540:
		if (is_yuv) {
			if (res <= (RES_1080p * 30))
				ot_lim = 2;
			else if (res <= (RES_1080p * 60))
				ot_lim = 4;
			else if (res <= (RES_WQXGA * 60))
				ot_lim = 4;
			else if (res <= (RES_UHD * 30))
				ot_lim = 8;
		} else if (fmt->bpp == 4 && res <= (RES_WQXGA * 60)) {
			ot_lim = 16;
		}

		break;
	default:
		if (is_yuv) {
			if (res <= (RES_1080p * 30))
				ot_lim = 2;
			else if (res <= (RES_1080p * 60))
				ot_lim = 4;
		}
		break;
	}

exit:
	SDEROT_DBG("ot_lim=%d\n", ot_lim);
	return ot_lim;
}

static u32 get_ot_limit(u32 reg_off, u32 bit_off,
	struct sde_mdp_set_ot_params *params)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	u32 ot_lim;
	u32 val;

	ot_lim = sde_mdp_get_ot_limit(
			params->width, params->height,
			params->fmt, params->fps,
			params->reg_off_vbif_lim_conf == MMSS_VBIF_RD_LIM_CONF);

	/*
	 * If default ot is not set from dt,
	 * then do not configure it.
	 */
	if (ot_lim == 0)
		goto exit;

	val = SDE_VBIF_READ(mdata, reg_off);
	val &= (0xFF << bit_off);
	val = val >> bit_off;

	SDEROT_EVTLOG(val, ot_lim);

	if (val == ot_lim)
		ot_lim = 0;

exit:
	SDEROT_DBG("ot_lim=%d\n", ot_lim);
	SDEROT_EVTLOG(params->width, params->height, params->fmt, params->fps,
			ot_lim);
	return ot_lim;
}

void sde_mdp_set_ot_limit(struct sde_mdp_set_ot_params *params)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	u32 ot_lim;
	u32 reg_off_vbif_lim_conf = ((params->xin_id / mdata->npriority_lvl)
					* mdata->npriority_lvl)
					+ params->reg_off_vbif_lim_conf;
	u32 bit_off_vbif_lim_conf = (params->xin_id % mdata->npriority_lvl) * 8;
	u32 reg_val;
	u32 sts;
	bool forced_on;

	vbif_lock(mdata->parent_pdev);

	ot_lim = get_ot_limit(
		reg_off_vbif_lim_conf,
		bit_off_vbif_lim_conf,
		params) & 0xFF;

	if (ot_lim == 0)
		goto exit;

	if (params->rotsts_base && params->rotsts_busy_mask) {
		sts = readl_relaxed(params->rotsts_base);
		if (sts & params->rotsts_busy_mask) {
			SDEROT_ERR(
				"Rotator still busy, should not modify VBIF\n");
			SDEROT_EVTLOG_TOUT_HANDLER(
				"rot", "vbif_dbg_bus", "panic");
		}
	}

	trace_rot_perf_set_ot(params->num, params->xin_id, ot_lim);

	forced_on = force_on_xin_clk(params->bit_off_mdp_clk_ctrl,
		params->reg_off_mdp_clk_ctrl, true);

	reg_val = SDE_VBIF_READ(mdata, reg_off_vbif_lim_conf);
	reg_val &= ~(0xFF << bit_off_vbif_lim_conf);
	reg_val |= (ot_lim) << bit_off_vbif_lim_conf;
	SDE_VBIF_WRITE(mdata, reg_off_vbif_lim_conf, reg_val);

	reg_val = SDE_VBIF_READ(mdata, MMSS_VBIF_XIN_HALT_CTRL0);
	SDE_VBIF_WRITE(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
		reg_val | BIT(params->xin_id));

	/* this is a polling operation */
	sde_mdp_wait_for_xin_halt(params->xin_id);

	reg_val = SDE_VBIF_READ(mdata, MMSS_VBIF_XIN_HALT_CTRL0);
	SDE_VBIF_WRITE(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
		reg_val & ~BIT(params->xin_id));

	if (forced_on)
		force_on_xin_clk(params->bit_off_mdp_clk_ctrl,
			params->reg_off_mdp_clk_ctrl, false);

	SDEROT_EVTLOG(params->num, params->xin_id, ot_lim);
exit:
	vbif_unlock(mdata->parent_pdev);
	return;
}

/*
 * sde_mdp_set_vbif_memtype - set memtype output for the given xin port
 * @mdata: pointer to global rotator data
 * @xin_id: xin identifier
 * @memtype: memtype output configuration
 * return: none
 */
static void sde_mdp_set_vbif_memtype(struct sde_rot_data_type *mdata,
		u32 xin_id, u32 memtype)
{
	u32 reg_off;
	u32 bit_off;
	u32 reg_val;

	/*
	 * Assume 4 bits per bit field, 8 fields per 32-bit register.
	 */
	if (xin_id >= 8)
		return;

	reg_off = MMSS_VBIF_NRT_VBIF_OUT_AXI_AMEMTYPE_CONF0;

	bit_off = (xin_id & 0x7) * 4;
	reg_val = SDE_VBIF_READ(mdata, reg_off);
	reg_val &= ~(0x7 << bit_off);
	reg_val |= (memtype & 0x7) << bit_off;
	SDE_VBIF_WRITE(mdata, reg_off, reg_val);
}

/*
 * sde_mdp_init_vbif - initialize static vbif configuration
 * return: 0 if success; error code otherwise
 */
int sde_mdp_init_vbif(void)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	int i;

	if (!mdata)
		return -EINVAL;

	if (mdata->vbif_memtype_count && mdata->vbif_memtype) {
		for (i = 0; i < mdata->vbif_memtype_count; i++)
			sde_mdp_set_vbif_memtype(mdata, i,
					mdata->vbif_memtype[i]);

		SDEROT_DBG("amemtype=0x%x\n", SDE_VBIF_READ(mdata,
				MMSS_VBIF_NRT_VBIF_OUT_AXI_AMEMTYPE_CONF0));
	}

	return 0;
}

struct reg_bus_client *sde_reg_bus_vote_client_create(char *client_name)
{
	struct reg_bus_client *client;
	struct sde_rot_data_type *sde_res = sde_rot_get_mdata();
	static u32 id;

	if (client_name == NULL) {
		SDEROT_ERR("client name is null\n");
		return ERR_PTR(-EINVAL);
	}

	client = kzalloc(sizeof(struct reg_bus_client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&sde_res->reg_bus_lock);
	strlcpy(client->name, client_name, MAX_CLIENT_NAME_LEN);
	client->usecase_ndx = VOTE_INDEX_DISABLE;
	client->id = id;
	SDEROT_DBG("bus vote client %s created:%p id :%d\n", client_name,
		client, id);
	id++;
	list_add(&client->list, &sde_res->reg_bus_clist);
	mutex_unlock(&sde_res->reg_bus_lock);

	return client;
}

void sde_reg_bus_vote_client_destroy(struct reg_bus_client *client)
{
	struct sde_rot_data_type *sde_res = sde_rot_get_mdata();

	if (!client) {
		SDEROT_ERR("reg bus vote: invalid client handle\n");
	} else {
		SDEROT_DBG("bus vote client %s destroyed:%p id:%u\n",
			client->name, client, client->id);
		mutex_lock(&sde_res->reg_bus_lock);
		list_del_init(&client->list);
		mutex_unlock(&sde_res->reg_bus_lock);
		kfree(client);
	}
}

int sde_update_reg_bus_vote(struct reg_bus_client *bus_client, u32 usecase_ndx)
{
	int ret = 0;
	bool changed = false;
	u32 max_usecase_ndx = VOTE_INDEX_DISABLE;
	struct reg_bus_client *client, *temp_client;
	struct sde_rot_data_type *sde_res = sde_rot_get_mdata();

	if (!sde_res || !sde_res->reg_bus_hdl || !bus_client)
		return 0;

	mutex_lock(&sde_res->reg_bus_lock);
	bus_client->usecase_ndx = usecase_ndx;
	list_for_each_entry_safe(client, temp_client, &sde_res->reg_bus_clist,
		list) {

		if (client->usecase_ndx < VOTE_INDEX_MAX &&
		    client->usecase_ndx > max_usecase_ndx)
			max_usecase_ndx = client->usecase_ndx;
	}

	if (sde_res->reg_bus_usecase_ndx != max_usecase_ndx) {
		changed = true;
		sde_res->reg_bus_usecase_ndx = max_usecase_ndx;
	}

	SDEROT_DBG(
		"%pS: changed=%d current idx=%d request client %s id:%u idx:%d\n",
		__builtin_return_address(0), changed, max_usecase_ndx,
		bus_client->name, bus_client->id, usecase_ndx);
	if (changed)
		ret = msm_bus_scale_client_update_request(sde_res->reg_bus_hdl,
			max_usecase_ndx);

	mutex_unlock(&sde_res->reg_bus_lock);
	return ret;
}

static int sde_mdp_parse_dt_handler(struct platform_device *pdev,
		char *prop_name, u32 *offsets, int len)
{
	int rc;

	rc = of_property_read_u32_array(pdev->dev.of_node, prop_name,
					offsets, len);
	if (rc) {
		SDEROT_ERR("Error from prop %s : u32 array read\n", prop_name);
		return -EINVAL;
	}

	return 0;
}

static int sde_mdp_parse_dt_prop_len(struct platform_device *pdev,
				      char *prop_name)
{
	int len = 0;

	of_find_property(pdev->dev.of_node, prop_name, &len);

	if (len < 1) {
		SDEROT_INFO("prop %s : doesn't exist in device tree\n",
			prop_name);
		return 0;
	}

	len = len/sizeof(u32);

	return len;
}

static void sde_mdp_parse_vbif_memtype(struct platform_device *pdev,
		struct sde_rot_data_type *mdata)
{
	int rc;

	mdata->vbif_memtype_count = sde_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-rot-vbif-memtype");
	mdata->vbif_memtype = kcalloc(mdata->vbif_memtype_count,
			sizeof(u32), GFP_KERNEL);
	if (!mdata->vbif_memtype || !mdata->vbif_memtype_count) {
		mdata->vbif_memtype_count = 0;
		return;
	}

	rc = sde_mdp_parse_dt_handler(pdev,
		"qcom,mdss-rot-vbif-memtype", mdata->vbif_memtype,
			mdata->vbif_memtype_count);
	if (rc) {
		SDEROT_DBG("vbif memtype not found\n");
		kfree(mdata->vbif_memtype);
		mdata->vbif_memtype = NULL;
		mdata->vbif_memtype_count = 0;
		return;
	}
}

static void sde_mdp_parse_vbif_qos(struct platform_device *pdev,
		struct sde_rot_data_type *mdata)
{
	int rc;

	mdata->vbif_rt_qos = NULL;

	mdata->npriority_lvl = sde_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-rot-vbif-qos-setting");
	mdata->vbif_nrt_qos = kcalloc(mdata->npriority_lvl,
			sizeof(u32), GFP_KERNEL);
	if (!mdata->vbif_nrt_qos || !mdata->npriority_lvl) {
		mdata->npriority_lvl = 0;
		return;
	}

	rc = sde_mdp_parse_dt_handler(pdev,
		"qcom,mdss-rot-vbif-qos-setting", mdata->vbif_nrt_qos,
			mdata->npriority_lvl);
	if (rc) {
		SDEROT_DBG("vbif setting not found\n");
		kfree(mdata->vbif_nrt_qos);
		mdata->vbif_nrt_qos = NULL;
		mdata->npriority_lvl = 0;
		return;
	}
}

static void sde_mdp_parse_vbif_xin_id(struct platform_device *pdev,
		struct sde_rot_data_type *mdata)
{
	mdata->vbif_xin_id[XIN_SSPP] = XIN_SSPP;
	mdata->vbif_xin_id[XIN_WRITEBACK] = XIN_WRITEBACK;

	sde_mdp_parse_dt_handler(pdev, "qcom,mdss-rot-xin-id",
					mdata->vbif_xin_id, MAX_XIN);
}

static void sde_mdp_parse_cdp_setting(struct platform_device *pdev,
		struct sde_rot_data_type *mdata)
{
	int rc;
	u32 len, data[SDE_ROT_OP_MAX] = {0};

	len = sde_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-rot-cdp-setting");
	if (len == SDE_ROT_OP_MAX) {
		rc = sde_mdp_parse_dt_handler(pdev,
			"qcom,mdss-rot-cdp-setting", data, len);
		if (rc) {
			SDEROT_ERR("invalid CDP setting\n");
			goto end;
		}

		set_bit(SDE_QOS_CDP, mdata->sde_qos_map);
		mdata->enable_cdp[SDE_ROT_RD] = data[SDE_ROT_RD];
		mdata->enable_cdp[SDE_ROT_WR] = data[SDE_ROT_WR];
		return;
	}
end:
	clear_bit(SDE_QOS_CDP, mdata->sde_qos_map);
}

static void sde_mdp_parse_rot_lut_setting(struct platform_device *pdev,
		struct sde_rot_data_type *mdata)
{
	int rc;
	u32 len, data[4];

	len = sde_mdp_parse_dt_prop_len(pdev, "qcom,mdss-rot-qos-lut");
	if (len == 4) {
		rc = sde_mdp_parse_dt_handler(pdev,
			"qcom,mdss-rot-qos-lut", data, len);
		if (!rc) {
			mdata->lut_cfg[SDE_ROT_RD].creq_lut_0 = data[0];
			mdata->lut_cfg[SDE_ROT_RD].creq_lut_1 = data[1];
			mdata->lut_cfg[SDE_ROT_WR].creq_lut_0 = data[2];
			mdata->lut_cfg[SDE_ROT_WR].creq_lut_1 = data[3];
			set_bit(SDE_QOS_LUT, mdata->sde_qos_map);
		} else {
			SDEROT_DBG("qos lut setting not found\n");
		}
	}

	len = sde_mdp_parse_dt_prop_len(pdev, "qcom,mdss-rot-danger-lut");
	if (len == SDE_ROT_OP_MAX) {
		rc = sde_mdp_parse_dt_handler(pdev,
			"qcom,mdss-rot-danger-lut", data, len);
		if (!rc) {
			mdata->lut_cfg[SDE_ROT_RD].danger_lut
							= data[SDE_ROT_RD];
			mdata->lut_cfg[SDE_ROT_WR].danger_lut
							= data[SDE_ROT_WR];
			set_bit(SDE_QOS_DANGER_LUT, mdata->sde_qos_map);
		} else {
			SDEROT_DBG("danger lut setting not found\n");
		}
	}

	len = sde_mdp_parse_dt_prop_len(pdev, "qcom,mdss-rot-safe-lut");
	if (len == SDE_ROT_OP_MAX) {
		rc = sde_mdp_parse_dt_handler(pdev,
			"qcom,mdss-rot-safe-lut", data, len);
		if (!rc) {
			mdata->lut_cfg[SDE_ROT_RD].safe_lut = data[SDE_ROT_RD];
			mdata->lut_cfg[SDE_ROT_WR].safe_lut = data[SDE_ROT_WR];
			set_bit(SDE_QOS_SAFE_LUT, mdata->sde_qos_map);
		} else {
			SDEROT_DBG("safe lut setting not found\n");
		}
	}
}

static void sde_mdp_parse_inline_rot_lut_setting(struct platform_device *pdev,
		struct sde_rot_data_type *mdata)
{
	int rc;
	u32 len, data[4];

	len = sde_mdp_parse_dt_prop_len(pdev, "qcom,mdss-inline-rot-qos-lut");
	if (len == 4) {
		rc = sde_mdp_parse_dt_handler(pdev,
			"qcom,mdss-inline-rot-qos-lut", data, len);
		if (!rc) {
			mdata->inline_lut_cfg[SDE_ROT_RD].creq_lut_0 = data[0];
			mdata->inline_lut_cfg[SDE_ROT_RD].creq_lut_1 = data[1];
			mdata->inline_lut_cfg[SDE_ROT_WR].creq_lut_0 = data[2];
			mdata->inline_lut_cfg[SDE_ROT_WR].creq_lut_1 = data[3];
			set_bit(SDE_INLINE_QOS_LUT, mdata->sde_inline_qos_map);
		} else {
			SDEROT_DBG("inline qos lut setting not found\n");
		}
	}

	len = sde_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-inline-rot-danger-lut");
	if (len == SDE_ROT_OP_MAX) {
		rc = sde_mdp_parse_dt_handler(pdev,
			"qcom,mdss-inline-rot-danger-lut", data, len);
		if (!rc) {
			mdata->inline_lut_cfg[SDE_ROT_RD].danger_lut
							= data[SDE_ROT_RD];
			mdata->inline_lut_cfg[SDE_ROT_WR].danger_lut
							= data[SDE_ROT_WR];
			set_bit(SDE_INLINE_QOS_DANGER_LUT,
					mdata->sde_inline_qos_map);
		} else {
			SDEROT_DBG("inline danger lut setting not found\n");
		}
	}

	len = sde_mdp_parse_dt_prop_len(pdev, "qcom,mdss-inline-rot-safe-lut");
	if (len == SDE_ROT_OP_MAX) {
		rc = sde_mdp_parse_dt_handler(pdev,
			"qcom,mdss-inline-rot-safe-lut", data, len);
		if (!rc) {
			mdata->inline_lut_cfg[SDE_ROT_RD].safe_lut
							= data[SDE_ROT_RD];
			mdata->inline_lut_cfg[SDE_ROT_WR].safe_lut
							= data[SDE_ROT_WR];
			set_bit(SDE_INLINE_QOS_SAFE_LUT,
					mdata->sde_inline_qos_map);
		} else {
			SDEROT_DBG("inline safe lut setting not found\n");
		}
	}
}

static void sde_mdp_parse_rt_rotator(struct device_node *np)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct platform_device *pdev;
	struct of_phandle_args phargs;
	int rc = 0;

	rc = of_parse_phandle_with_args(np,
			"qcom,mdss-rot-parent", "#list-cells", 0, &phargs);

	if (rc)
		return;

	if (!phargs.np || !phargs.args_count) {
		SDEROT_ERR("invalid args\n");
		return;
	}

	pdev = of_find_device_by_node(phargs.np);
	if (pdev) {
		mdata->parent_pdev = pdev;
	} else {
		mdata->parent_pdev = NULL;
		SDEROT_ERR("Parent mdp node not available\n");
	}

	of_node_put(phargs.np);
}

static int sde_mdp_parse_dt_misc(struct platform_device *pdev,
		struct sde_rot_data_type *mdata)
{
	int rc;
	u32 data;

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,mdss-rot-block-size",
		&data);
	mdata->rot_block_size = (!rc ? data : 128);

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-default-ot-rd-limit", &data);
	mdata->default_ot_rd_limit = (!rc ? data : 0);

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-default-ot-wr-limit", &data);
	mdata->default_ot_wr_limit = (!rc ? data : 0);

	rc = of_property_read_u32(pdev->dev.of_node,
		 "qcom,mdss-highest-bank-bit", &(mdata->highest_bank_bit));
	if (rc)
		SDEROT_DBG(
			"Could not read optional property: highest bank bit\n");

	sde_mdp_parse_cdp_setting(pdev, mdata);

	sde_mdp_parse_vbif_qos(pdev, mdata);

	sde_mdp_parse_vbif_xin_id(pdev, mdata);

	sde_mdp_parse_vbif_memtype(pdev, mdata);

	sde_mdp_parse_rot_lut_setting(pdev, mdata);

	sde_mdp_parse_inline_rot_lut_setting(pdev, mdata);

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-rot-qos-cpu-mask", &data);
	mdata->rot_pm_qos_cpu_mask = (!rc ? data : 0);

	rc = of_property_read_u32(pdev->dev.of_node,
		 "qcom,mdss-rot-qos-cpu-dma-latency", &data);
	mdata->rot_pm_qos_cpu_dma_latency = (!rc ? data : 0);

	mdata->mdp_base = mdata->sde_io.base + SDE_MDP_OFFSET;

	return 0;
}

static void sde_mdp_destroy_dt_misc(struct platform_device *pdev,
		struct sde_rot_data_type *mdata)
{
	kfree(mdata->vbif_memtype);
	mdata->vbif_memtype = NULL;
	kfree(mdata->vbif_rt_qos);
	mdata->vbif_rt_qos = NULL;
	kfree(mdata->vbif_nrt_qos);
	mdata->vbif_nrt_qos = NULL;
}

#define MDP_REG_BUS_VECTOR_ENTRY(ab_val, ib_val)	\
	{						\
		.src = MSM_BUS_MASTER_AMPSS_M0,		\
		.dst = MSM_BUS_SLAVE_DISPLAY_CFG,	\
		.ab = (ab_val),				\
		.ib = (ib_val),				\
	}

#define BUS_VOTE_19_MHZ 153600000
#define BUS_VOTE_40_MHZ 320000000
#define BUS_VOTE_80_MHZ 640000000

static struct msm_bus_vectors mdp_reg_bus_vectors[] = {
	MDP_REG_BUS_VECTOR_ENTRY(0, 0),
	MDP_REG_BUS_VECTOR_ENTRY(0, BUS_VOTE_19_MHZ),
	MDP_REG_BUS_VECTOR_ENTRY(0, BUS_VOTE_40_MHZ),
	MDP_REG_BUS_VECTOR_ENTRY(0, BUS_VOTE_80_MHZ),
};
static struct msm_bus_paths mdp_reg_bus_usecases[ARRAY_SIZE(
		mdp_reg_bus_vectors)];
static struct msm_bus_scale_pdata mdp_reg_bus_scale_table = {
	.usecase = mdp_reg_bus_usecases,
	.num_usecases = ARRAY_SIZE(mdp_reg_bus_usecases),
	.name = "sde_reg",
	.active_only = true,
};

static int sde_mdp_bus_scale_register(struct sde_rot_data_type *mdata)
{
	struct msm_bus_scale_pdata *reg_bus_pdata;
	int i;

	if (!mdata->reg_bus_hdl) {
		reg_bus_pdata = &mdp_reg_bus_scale_table;
		for (i = 0; i < reg_bus_pdata->num_usecases; i++) {
			mdp_reg_bus_usecases[i].num_paths = 1;
			mdp_reg_bus_usecases[i].vectors =
				&mdp_reg_bus_vectors[i];
		}

		mdata->reg_bus_hdl =
			msm_bus_scale_register_client(reg_bus_pdata);
		if (!mdata->reg_bus_hdl) {
			/* Continue without reg_bus scaling */
			SDEROT_WARN("reg_bus_client register failed\n");
		} else
			SDEROT_DBG("register reg_bus_hdl=%x\n",
					mdata->reg_bus_hdl);
	}

	return 0;
}

static void sde_mdp_bus_scale_unregister(struct sde_rot_data_type *mdata)
{
	SDEROT_DBG("unregister reg_bus_hdl=%x\n", mdata->reg_bus_hdl);

	if (mdata->reg_bus_hdl) {
		msm_bus_scale_unregister_client(mdata->reg_bus_hdl);
		mdata->reg_bus_hdl = 0;
	}
}

static struct sde_rot_data_type *sde_rot_res;

struct sde_rot_data_type *sde_rot_get_mdata(void)
{
	return sde_rot_res;
}

/*
 * sde_rotator_base_init - initialize base rotator data/resource
 */
int sde_rotator_base_init(struct sde_rot_data_type **pmdata,
		struct platform_device *pdev,
		const void *drvdata)
{
	int rc;
	struct sde_rot_data_type *mdata;


	/* if probe deferral happened, return early*/
	if (sde_rot_res) {
		SDEROT_ERR("Rotator data already initialized, skip init\n");
		return 0;
	}

	mdata = devm_kzalloc(&pdev->dev, sizeof(*mdata), GFP_KERNEL);
	if (mdata == NULL)
		return -ENOMEM;

	mdata->pdev = pdev;
	sde_rot_res = mdata;
	mutex_init(&mdata->reg_bus_lock);
	INIT_LIST_HEAD(&mdata->reg_bus_clist);

	rc = sde_rot_ioremap_byname(pdev, &mdata->sde_io, "mdp_phys");
	if (rc) {
		SDEROT_ERR("unable to map SDE base\n");
		goto probe_done;
	}
	SDEROT_DBG("SDE ROT HW Base addr=0x%x len=0x%x\n",
		(int) (unsigned long) mdata->sde_io.base,
		mdata->sde_io.len);

	rc = sde_rot_ioremap_byname(pdev, &mdata->vbif_nrt_io, "rot_vbif_phys");
	if (rc) {
		SDEROT_ERR("unable to map SDE ROT VBIF base\n");
		goto probe_done;
	}
	SDEROT_DBG("SDE ROT VBIF HW Base addr=%pK len=0x%x\n",
			mdata->vbif_nrt_io.base, mdata->vbif_nrt_io.len);

	sde_mdp_parse_rt_rotator(pdev->dev.of_node);

	rc = sde_mdp_parse_dt_misc(pdev, mdata);
	if (rc) {
		SDEROT_ERR("Error in device tree : misc\n");
		goto probe_done;
	}

	rc = sde_mdp_bus_scale_register(mdata);
	if (rc) {
		SDEROT_ERR("unable to register bus scaling\n");
		goto probe_done;
	}

	rc = sde_smmu_init(&pdev->dev);
	if (rc) {
		SDEROT_ERR("sde smmu init failed %d\n", rc);
		goto probe_done;
	}

	*pmdata = mdata;

	return 0;
probe_done:
	return rc;
}

/*
 * sde_rotator_base_destroy - clean up base rotator data/resource
 */
void sde_rotator_base_destroy(struct sde_rot_data_type *mdata)
{
	struct platform_device *pdev;

	if (!mdata || !mdata->pdev)
		return;

	pdev = mdata->pdev;

	sde_rot_res = NULL;
	sde_mdp_bus_scale_unregister(mdata);
	sde_mdp_destroy_dt_misc(pdev, mdata);
	sde_rot_iounmap(&mdata->vbif_nrt_io);
	sde_rot_iounmap(&mdata->sde_io);
	devm_kfree(&pdev->dev, mdata);
}
