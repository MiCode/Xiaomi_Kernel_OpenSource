// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/component.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

#include "mtk_cam.h"
#include "mtk_cam-seninf-def.h"
#include "mtk_cam-seninf.h"
#include "mtk_cam-seninf-hw.h"
#include "mtk_cam-seninf-route.h"

#define V4L2_CID_MTK_SENINF_BASE	(V4L2_CID_USER_BASE | 0xf000)
#define V4L2_CID_MTK_TEST_STREAMON	(V4L2_CID_MTK_SENINF_BASE + 1)

#define sd_to_ctx(__sd) container_of(__sd, struct seninf_ctx, subdev)
#define notifier_to_ctx(__n) container_of(__n, struct seninf_ctx, notifier)
#define ctrl_hdl_to_ctx(__h) container_of(__h, struct seninf_ctx, ctrl_handler)

static const char * const csi_port_names[] = {
	SENINF_CSI_PORT_NAMES
};

static const char * const clk_names[] = {
	SENINF_CLK_NAMES
};

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	return mtk_cam_seninf_show_status(dev, attr, buf);
}

static DEVICE_ATTR_RO(status);

static int seninf_dfs_init(struct seninf_dfs *dfs, struct device *dev)
{
	int ret, i;
	struct dev_pm_opp *opp;
	unsigned long freq;

	dfs->dev = dev;

	ret = dev_pm_opp_of_add_table(dev);
	if (ret < 0) {
		dev_err(dev, "fail to init opp table: %d\n", ret);
		return ret;
	}

	dfs->reg = devm_regulator_get_optional(dev, "dvfsrc-vcore");
	if (IS_ERR(dfs->reg)) {
		dev_err(dev, "can't get dvfsrc-vcore\n");
		return PTR_ERR(dfs->reg);
	}

	dfs->cnt = dev_pm_opp_get_opp_count(dev);

	dfs->freqs = devm_kzalloc(dev,
				  sizeof(unsigned long) * dfs->cnt, GFP_KERNEL);
	dfs->volts = devm_kzalloc(dev,
				  sizeof(unsigned long) * dfs->cnt, GFP_KERNEL);
	if (!dfs->freqs || !dfs->volts)
		return -ENOMEM;

	i = 0;
	freq = 0;
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {
		dfs->freqs[i] = freq;
		dfs->volts[i] = dev_pm_opp_get_voltage(opp);
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}

	for (i = 0; i < dfs->cnt; i++) {
		dev_info(dev, "dfs[%d] freq %ld volt %ld\n",
			 i, dfs->freqs[i], dfs->volts[i]);
	}

	return 0;
}

static int seninf_dfs_exit(struct seninf_dfs *dfs)
{
	if (!dfs->cnt)
		return 0;

	dev_pm_opp_of_remove_table(dfs->dev);

	return 0;
}

static int __seninf_dfs_set(struct seninf_ctx *ctx, unsigned long freq)
{
	int i;
	struct seninf_ctx *tmp;
	struct seninf_core *core = ctx->core;
	struct seninf_dfs *dfs = &core->dfs;
	unsigned long require = 0, old;

	if (!dfs->cnt)
		return 0;

	old = ctx->isp_freq;
	ctx->isp_freq = freq;

	list_for_each_entry(tmp, &core->list, list) {
		if (tmp->isp_freq > require)
			require = tmp->isp_freq;
	}

	for (i = 0; i < dfs->cnt; i++) {
		if (dfs->freqs[i] >= require)
			break;
	}

	if (i == dfs->cnt) {
		ctx->isp_freq = old;
		return -EINVAL;
	}

	regulator_set_voltage(dfs->reg, dfs->volts[i],
			      dfs->volts[dfs->cnt - 1]);

	dev_info(ctx->dev, "freq %ld require %ld selected %ld\n",
		 freq, require, dfs->freqs[i]);

	return 0;
}

static int seninf_dfs_set(struct seninf_ctx *ctx, unsigned long freq)
{
	int ret;
	struct seninf_core *core = ctx->core;

	mutex_lock(&core->mutex);
	ret = __seninf_dfs_set(ctx, freq);
	mutex_unlock(&core->mutex);

	return ret;
}

static int seninf_core_probe(struct platform_device *pdev)
{
	int i, ret;
	struct resource *res;
	struct seninf_core *core;
	struct device *dev = &pdev->dev;

	core = devm_kzalloc(&pdev->dev, sizeof(*core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	dev_set_drvdata(dev, core);
	core->dev = dev;
	mutex_init(&core->mutex);
	INIT_LIST_HEAD(&core->list);
	mtk_cam_seninf_init_res(core);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	core->reg_if = devm_ioremap_resource(dev, res);
	if (IS_ERR(core->reg_if))
		return PTR_ERR(core->reg_if);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ana-rx");
	core->reg_ana = devm_ioremap_resource(dev, res);
	if (IS_ERR(core->reg_ana))
		return PTR_ERR(core->reg_ana);

	for (i = 0; i < CLK_MAXCNT; i++) {
		core->clk[i] = devm_clk_get(dev, clk_names[i]);
		if (IS_ERR(core->clk[i])) {
			dev_err(dev, "failed to get %s\n", clk_names[i]);
			return -EINVAL;
		}
	}

	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "%s: failed to createt sub devices\n", __func__);
		return ret;
	}

	ret = seninf_dfs_init(&core->dfs, dev);
	if (ret) {
		dev_err(dev, "%s: failed to init dfs\n", __func__);
		//return ret;
	}

	ret = device_create_file(dev, &dev_attr_status);
	if (ret)
		dev_warn(dev, "failed to create sysfs status\n");

	pm_runtime_enable(dev);

	dev_info(dev, "%s\n", __func__);

	return 0;
}

static int seninf_core_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct seninf_core *core = dev_get_drvdata(dev);

	seninf_dfs_exit(&core->dfs);
	of_platform_depopulate(dev);
	pm_runtime_disable(dev);
	device_remove_file(dev, &dev_attr_status);

	dev_info(dev, "%s\n", __func__);

	return 0;
}

static const struct of_device_id seninf_core_of_match[] = {
	{.compatible = "mediatek,seninf-core"},
	{},
};
MODULE_DEVICE_TABLE(of, seninf_core_of_match);

struct platform_driver seninf_core_pdrv = {
	.probe	= seninf_core_probe,
	.remove	= seninf_core_remove,
	.driver	= {
		.name = "seninf-core",
		.of_match_table = seninf_core_of_match,
	},
};

static int get_csi_port(struct device *dev, int *port)
{
	int i, ret;
	const char *name;

	ret = of_property_read_string(dev->of_node, "csi-port", &name);
	if (ret)
		return ret;

	for (i = 0; i < CSI_PORT_MAX_NUM; i++) {
		if (!strcasecmp(name, csi_port_names[i])) {
			*port = i;
			return 0;
		}
	}

	return -1;
}

static int seninf_subscribe_event(struct v4l2_subdev *sd,
				  struct v4l2_fh *fh,
				  struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subdev_subscribe_event(sd, fh, sub);
	default:
		return -EINVAL;
	}
}

static void init_fmt(struct seninf_ctx *ctx)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(ctx->fmt); i++) {
		ctx->fmt[i].format.code = MEDIA_BUS_FMT_SBGGR10_1X10;
		ctx->fmt[i].format.width = DEFAULT_WIDTH;
		ctx->fmt[i].format.height = DEFAULT_HEIGHT;
		ctx->fmt[i].format.field = V4L2_FIELD_NONE;
		ctx->fmt[i].format.colorspace = V4L2_COLORSPACE_SRGB;
		ctx->fmt[i].format.xfer_func = V4L2_XFER_FUNC_DEFAULT;
		ctx->fmt[i].format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
		ctx->fmt[i].format.quantization = V4L2_QUANTIZATION_DEFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(ctx->vcinfo.vc); i++)
		ctx->vcinfo.vc[i].pixel_mode = SENINF_DEF_PIXEL_MODE;
}

static const struct v4l2_mbus_framefmt fmt_default = {
	.code = MEDIA_BUS_FMT_SBGGR10_1X10,
	.width = DEFAULT_WIDTH,
	.height = DEFAULT_HEIGHT,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_SRGB,
	.xfer_func = V4L2_XFER_FUNC_DEFAULT,
	.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
	.quantization = V4L2_QUANTIZATION_DEFAULT,
};

static int mtk_cam_seninf_init_cfg(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg)
{
	struct v4l2_mbus_framefmt *mf;
	unsigned int i;

	for (i = 0; i < sd->entity.num_pads; i++) {
		mf = v4l2_subdev_get_try_format(sd, cfg, i);
		*mf = fmt_default;
	}

	return 0;
}

static int mtk_cam_seninf_set_fmt(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	struct v4l2_mbus_framefmt *format;
	char bSinkFormatChanged = 0;

	if (fmt->pad < PAD_SINK || fmt->pad >= PAD_MAXCNT)
		return -EINVAL;

	format = &ctx->fmt[fmt->pad].format;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
	} else {
		/* NOTE: update vcinfo once the SINK format changed */
		if (fmt->pad == PAD_SINK &&
		    format->code != fmt->format.code &&
			format->width != fmt->format.width &&
			format->height != fmt->format.height)
			bSinkFormatChanged = 1;

		format->code = fmt->format.code;
		format->width = fmt->format.width;
		format->height = fmt->format.height;

		if (bSinkFormatChanged)
			mtk_cam_seninf_get_vcinfo(ctx);
	}

	dev_info(ctx->dev, "s_fmt pad %d code/res 0x%x/%dx%d => 0x%x/%dx%d\n",
		 fmt->pad,
		fmt->format.code,
		fmt->format.width,
		fmt->format.height,
		format->code,
		format->width,
		format->height);

	return 0;
}

static int mtk_cam_seninf_get_fmt(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	struct v4l2_mbus_framefmt *format;

	if (fmt->pad < PAD_SINK || fmt->pad >= PAD_MAXCNT)
		return -EINVAL;

	format = &ctx->fmt[fmt->pad].format;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
	} else {
		fmt->format.code = format->code;
		fmt->format.width = format->width;
		fmt->format.height = format->height;
		fmt->format.field = format->field;
		fmt->format.colorspace = format->colorspace;
		fmt->format.xfer_func = format->xfer_func;
		fmt->format.ycbcr_enc = format->ycbcr_enc;
		fmt->format.quantization = format->quantization;
	}

	dev_info(ctx->dev, "g_fmt pad %d code/res 0x%x/%dx%d\n",
		 fmt->pad,
		format->code,
		format->width,
		format->height);

	return 0;
}

static int set_test_model(struct seninf_ctx *ctx, char enable)
{
	struct seninf_vc *vc;
	struct seninf_mux *mux;
	struct seninf_dfs *dfs = &ctx->core->dfs;
	int pref_idx[] = {0, 5}; //FIXME

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
	if (!vc) {
		dev_err(ctx->dev, "failed to get vc\n");
		return -1;
	}

	if (enable) {
		mux = mtk_cam_seninf_mux_get_pref(ctx,
						  pref_idx,
						  ARRAY_SIZE(pref_idx));
		if (!mux)
			return -EBUSY;
		vc->mux = mux->idx;
		vc->cam = ctx->pad2cam[vc->out_pad];
		vc->enable = 1;
		pm_runtime_get_sync(ctx->dev);
		clk_prepare_enable(ctx->core->clk[CLK_TOP_CAMTM]);
		if (dfs->cnt)
			seninf_dfs_set(ctx, dfs->freqs[dfs->cnt - 1]);
		mtk_cam_seninf_set_test_model(ctx,
					      vc->mux, vc->cam, vc->pixel_mode);
	} else {
		mtk_cam_seninf_set_idle(ctx);
		mtk_cam_seninf_release_mux(ctx);
		if (dfs->cnt)
			seninf_dfs_set(ctx, 0);
		clk_disable_unprepare(ctx->core->clk[CLK_TOP_CAMTM]);
		pm_runtime_put(ctx->dev);
	}

	ctx->streaming = enable;

	return 0;
}

static int config_hw(struct seninf_ctx *ctx)
{
	int i, intf, skip_mux_ctrl;
	int hsPol, vsPol, vc_sel, dt_sel, dt_en;
	struct seninf_vcinfo *vcinfo;
	struct seninf_vc *vc;
	struct seninf_mux *mux, *mux_by_grp[4] = {0};

	intf = ctx->seninfIdx;
	vcinfo = &ctx->vcinfo;

	mtk_cam_seninf_reset(ctx, intf);

	mtk_cam_seninf_set_vc(ctx, intf, vcinfo);
	mtk_cam_seninf_set_csi_mipi(ctx);

	// TODO
	hsPol = 0;
	vsPol = 0;

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];

		vc->enable = mtk_cam_seninf_is_vc_enabled(ctx, vc);
		if (!vc->enable) {
			dev_info(ctx->dev, "vc[%d] pad %d. skip\n",
				 i, vc->feature, vc->out_pad);
			continue;
		}

		/* alloc mux by group */
		if (mux_by_grp[vc->group]) {
			mux = mux_by_grp[vc->group];
			skip_mux_ctrl = 1;
		} else {
			mux = mux_by_grp[vc->group] =
				mtk_cam_seninf_mux_get(ctx);
			skip_mux_ctrl = 0;
		}

		if (!mux) {
			mtk_cam_seninf_release_mux(ctx);
			return -EBUSY;
		}

		vc->mux = mux->idx;
		vc->cam = ctx->pad2cam[vc->out_pad];

		if (!skip_mux_ctrl) {
			mtk_cam_seninf_mux(ctx, vc->mux);
			mtk_cam_seninf_set_mux_ctrl(ctx, vc->mux,
						    hsPol, vsPol,
				MIPI_SENSOR + vc->group,
				vc->pixel_mode);

			mtk_cam_seninf_set_top_mux_ctrl(ctx, vc->mux, intf);

			//TODO
			//mtk_cam_seninf_set_mux_crop(ctx, vc->mux, 0, 2327, 0);
		}

		vc_sel = vc->vc;
		dt_sel = vc->dt;
		dt_en = !!dt_sel;

		/* CMD_SENINF_FINALIZE_CAM_MUX */
		mtk_cam_seninf_set_cammux_vc(ctx, vc->cam,
					     vc_sel, dt_sel, dt_en, dt_en);
		mtk_cam_seninf_set_cammux_src(ctx, vc->mux, vc->cam,
					      vc->exp_hsize, vc->exp_vsize);
		mtk_cam_seninf_set_cammux_chk_pixel_mode(ctx,
							 vc->cam,
							 vc->pixel_mode);
		mtk_cam_seninf_cammux(ctx, vc->cam);

		dev_info(ctx->dev, "vc[%d] pad %d intf %d mux %d cam %d\n",
			 i, vc->out_pad, intf, vc->mux, vc->cam,
			vc_sel, dt_sel);
	}

	return 0;
}

static int get_buffered_pixel_rate(struct seninf_ctx *ctx,
				   struct v4l2_subdev *sd, int sd_pad_idx,
				   s64 *result)
{
	int ret;
	struct v4l2_ctrl *ctrl;
	struct v4l2_subdev_format fmt;
	struct v4l2_subdev_frame_interval fi;
	s64 width, height, hblank, vblank, pclk, buffered_pixel_rate;

	fmt.pad = sd_pad_idx;
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
	if (ret) {
		dev_err(ctx->dev, "no get_fmt in %s\n", sd->name);
		return ret;
	}

	width = fmt.format.width;
	height = fmt.format.height;

	memset(&fi, 0, sizeof(fi));
	fi.pad = sd_pad_idx;
	ret = v4l2_subdev_call(sd, video, g_frame_interval, &fi);
	if (ret) {
		dev_err(ctx->dev, "no g_frame_interval in %s\n", sd->name);
		return ret;
	}

	ctrl = v4l2_ctrl_find(sd->ctrl_handler, V4L2_CID_HBLANK);
	if (!ctrl) {
		dev_err(ctx->dev, "no hblank in %s\n", sd->name);
		return -EINVAL;
	}

	hblank = v4l2_ctrl_g_ctrl(ctrl);

	ctrl = v4l2_ctrl_find(sd->ctrl_handler, V4L2_CID_VBLANK);
	if (!ctrl) {
		dev_err(ctx->dev, "no vblank in %s\n", sd->name);
		return -EINVAL;
	}

	vblank = v4l2_ctrl_g_ctrl(ctrl);

	/* update fps */
	ctx->fps_n = fi.interval.denominator;
	ctx->fps_d = fi.interval.numerator;

	/* calculate pclk */
	pclk = (width + hblank) * (height + vblank) * ctx->fps_n;
	do_div(pclk, ctx->fps_d);

	/* calculate buffered pixel_rate */
	buffered_pixel_rate = pclk * width;
	do_div(buffered_pixel_rate, (width + hblank - HW_BUF_EFFECT));
	*result = buffered_pixel_rate;

	dev_info(ctx->dev, "w %d h %d hb %d vb %d fps %d/%d pclk %lld->%lld\n",
		 width, height, hblank, vblank,
			ctx->fps_n, ctx->fps_d,
			pclk, buffered_pixel_rate);

	return 0;
}

static int get_pixel_rate(struct seninf_ctx *ctx, struct v4l2_subdev *sd,
			  s64 *result)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sd->ctrl_handler, V4L2_CID_PIXEL_RATE);
	if (!ctrl) {
		dev_err(ctx->dev, "no pixel rate in subdev %s\n", sd->name);
		return -EINVAL;
	}

	*result = v4l2_ctrl_g_ctrl_int64(ctrl);

	return 0;
}

static int get_mbus_config(struct seninf_ctx *ctx, struct v4l2_subdev *sd)
{
	struct v4l2_mbus_config cfg;
	int ret;

	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if (ret) {
		dev_warn(ctx->dev, "no g_mbus_config in %s\n",
			 sd->entity.name);
		return -1;
	}

	ctx->is_cphy = cfg.type == V4L2_MBUS_CSI2_CPHY;

	if (cfg.flags & V4L2_MBUS_CSI2_1_LANE)
		ctx->num_data_lanes = 1;
	else if (cfg.flags & V4L2_MBUS_CSI2_2_LANE)
		ctx->num_data_lanes = 2;
	else if (cfg.flags & V4L2_MBUS_CSI2_3_LANE)
		ctx->num_data_lanes = 3;
	else if (cfg.flags & V4L2_MBUS_CSI2_4_LANE)
		ctx->num_data_lanes = 4;

	return 0;
}

static int update_isp_clk(struct seninf_ctx *ctx)
{
	int i, ret, pixelmode;
	struct seninf_dfs *dfs = &ctx->core->dfs;
	s64 pixel_rate = -1, dfs_freq;
	struct seninf_vc *vc;

	if (!dfs->cnt)
		return 0;

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
	if (!vc) {
		dev_err(ctx->dev, "failed to get vc\n");
		return -1;
	}

	if (ctx->buffered_pixel_rate)
		pixel_rate = ctx->buffered_pixel_rate;
	else if (ctx->mipi_pixel_rate)
		pixel_rate = ctx->mipi_pixel_rate;
	else {
		dev_warn(ctx->dev, "failed to get pixel_rate\n");
		return -EINVAL;
	}

	pixelmode = vc->pixel_mode;
	for (i = 0; i < dfs->cnt; i++) {
		dfs_freq = dfs->freqs[i];
		dfs_freq = dfs_freq * (100 - SENINF_CLK_MARGIN_IN_PERCENT);
		do_div(dfs_freq, 100);
		if ((dfs_freq << pixelmode) >= pixel_rate)
			break;
	}

	if (i == dfs->cnt) {
		dev_err(ctx->dev, "mux is overrun. please adjust pixelmode\n");
		return -EINVAL;
	}

	ret = seninf_dfs_set(ctx, dfs->freqs[i]);
	if (ret) {
		dev_err(ctx->dev, "failed to set freq\n");
		return ret;
	}

	return 0;
}

#ifdef SENINF_VC_ROUTING
static int update_sensor_frame_desc(struct seninf_ctx *ctx)
{
	int i, ret;
	struct v4l2_mbus_frame_desc fd;

	ret = v4l2_subdev_call(ctx->sensor_sd, pad, get_frame_desc,
			       ctx->sensor_pad_idx, &fd);

	if (ret || fd.type != V4L2_MBUS_FRAME_DESC_TYPE_CSI2)
		return -1;

	for (i = 0; i < fd.num_entries; i++) {
		fd.entry[i].bus.csi2.enable =
			mtk_cam_seninf_is_di_enabled
				(ctx, fd.entry[i].bus.csi2.channel,
				 fd.entry[i].bus.csi2.data_type);
	}

	v4l2_subdev_call(ctx->sensor_sd, pad, set_frame_desc,
			 ctx->sensor_pad_idx, &fd);

	return 0;
}
#endif

static int seninf_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct seninf_ctx *ctx = sd_to_ctx(sd);

	if (ctx->streaming == enable)
		return 0;

	if (ctx->is_test_model)
		return set_test_model(ctx, enable);

	if (!ctx->sensor_sd) {
		dev_err(ctx->dev, "no sensor\n");
		return -EFAULT;
	}

	get_mbus_config(ctx, ctx->sensor_sd);
	get_buffered_pixel_rate(ctx, ctx->sensor_sd,
				ctx->sensor_pad_idx, &ctx->buffered_pixel_rate);
	get_pixel_rate(ctx, ctx->sensor_sd, &ctx->mipi_pixel_rate);

	if (enable) {
		ret = pm_runtime_get_sync(ctx->dev);
		if (ret < 0) {
			dev_err(ctx->dev, "pm_runtime_get_sync ret %d\n", ret);
			pm_runtime_put_noidle(ctx->dev);
			return ret;
		}

		update_isp_clk(ctx);

		ret = config_hw(ctx);
		if (ret) {
			dev_err(ctx->dev, "config_seninf_hw ret %d\n", ret);
			return ret;
		}

#ifdef SENINF_VC_ROUTING
		update_sensor_frame_desc(ctx);
#endif

		ret = v4l2_subdev_call(ctx->sensor_sd, video, s_stream, 1);
		if (ret) {
			dev_err(ctx->dev, "sensor stream-on ret %d\n", ret);
			return  ret;
		}

	} else {
		ret = v4l2_subdev_call(ctx->sensor_sd, video, s_stream, 0);
		if (ret) {
			dev_err(ctx->dev, "sensor stream-off ret %d\n", ret);
			return ret;
		}
		mtk_cam_seninf_set_idle(ctx);
		mtk_cam_seninf_release_mux(ctx);
		seninf_dfs_set(ctx, 0);
		pm_runtime_put(ctx->dev);
	}

	ctx->streaming = enable;

	return 0;
}

static const struct v4l2_subdev_pad_ops seninf_subdev_pad_ops = {
	.init_cfg = mtk_cam_seninf_init_cfg,
	.set_fmt = mtk_cam_seninf_set_fmt,
	.get_fmt = mtk_cam_seninf_get_fmt,
};

static const struct v4l2_subdev_video_ops seninf_subdev_video_ops = {
	.s_stream = seninf_s_stream,
};

static struct v4l2_subdev_core_ops seninf_subdev_core_ops = {
	.subscribe_event	= seninf_subscribe_event,
	.unsubscribe_event	= v4l2_event_subdev_unsubscribe,
};

static struct v4l2_subdev_ops seninf_subdev_ops = {
	.core	= &seninf_subdev_core_ops,
	.video	= &seninf_subdev_video_ops,
	.pad	= &seninf_subdev_pad_ops,
};

static int seninf_link_setup(struct media_entity *entity,
			     const struct media_pad *local,
				 const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd;
	struct seninf_ctx *ctx;

	sd = media_entity_to_v4l2_subdev(entity);
	ctx = v4l2_get_subdevdata(sd);

	if (local->flags & MEDIA_PAD_FL_SOURCE) {
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (!mtk_cam_seninf_get_vc_by_pad(ctx, local->index))
				return -EIO;
		}
	} else {
		/* NOTE: update vcinfo once the link becomes enabled */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			ctx->sensor_sd =
				media_entity_to_v4l2_subdev(remote->entity);
			ctx->sensor_pad_idx = remote->index;
			mtk_cam_seninf_get_vcinfo(ctx);
		}
	}

	return 0;
}

static const struct media_entity_operations seninf_media_ops = {
	.link_setup = seninf_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static int seninf_notifier_bound(struct v4l2_async_notifier *notifier,
				 struct v4l2_subdev *sd,
					 struct v4l2_async_subdev *asd)
{
	struct seninf_ctx *ctx = notifier_to_ctx(notifier);
	int ret;

	dev_info(ctx->dev, "%s bounded\n", sd->entity.name);

	ret = media_create_pad_link(&sd->entity, 0,
				    &ctx->subdev.entity, 0, 0);
	if (ret) {
		dev_err(ctx->dev,
			"failed to create link for %s\n",
			sd->entity.name);
		return ret;
	}

	ret = v4l2_device_register_subdev_nodes(ctx->subdev.v4l2_dev);
	if (ret) {
		dev_err(ctx->dev, "failed to create subdev nodes\n");
		return ret;
	}

	return 0;
}

static void seninf_notifier_unbind(struct v4l2_async_notifier *notifier,
				   struct v4l2_subdev *sd,
					   struct v4l2_async_subdev *asd)
{
	struct seninf_ctx *ctx = notifier_to_ctx(notifier);

	dev_info(ctx->dev, "%s is unbounded\n", sd->entity.name);
}

static const struct v4l2_async_notifier_operations seninf_async_ops = {
	.bound = seninf_notifier_bound,
	.unbind = seninf_notifier_unbind,
};

/* NOTE: update vcinfo once test_model switches */
static int seninf_test_pattern(struct seninf_ctx *ctx, u32 pattern)
{
	switch (pattern) {
	case 0:
		if (ctx->streaming)
			return -EBUSY;
		ctx->is_test_model = 0;
		mtk_cam_seninf_get_vcinfo(ctx);
		dev_info(ctx->dev, "test pattern off\n");
		break;
	case 1:
		if (ctx->streaming)
			return -EBUSY;
		ctx->is_test_model = 1;
		mtk_cam_seninf_get_vcinfo_test(ctx);
		dev_info(ctx->dev, "test pattern on\n");
		break;
	default:
		break;
	}

	return 0;
}

#ifdef SENINF_DEBUG
static int seninf_test_streamon(struct seninf_ctx *ctx, u32 en)
{
	if (en) {
		ctx->is_test_streamon = 1;
		mtk_cam_seninf_alloc_cam_mux(ctx);
		seninf_s_stream(&ctx->subdev, 1);
	} else {
		seninf_s_stream(&ctx->subdev, 0);
		mtk_cam_seninf_release_cam_mux(ctx);
		ctx->is_test_streamon = 0;
	}

	return 0;
}
#endif

static int mtk_cam_seninf_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct seninf_ctx *ctx = ctrl_hdl_to_ctx(ctrl->handler);
	int ret;

	switch (ctrl->id) {
	case V4L2_CID_TEST_PATTERN:
		ret = seninf_test_pattern(ctx, ctrl->val);
		break;
#ifdef SENINF_DEBUG
	case V4L2_CID_MTK_TEST_STREAMON:
		ret = seninf_test_streamon(ctx, ctrl->val);
		break;
#endif
	default:
		ret = 0;
		dev_warn(ctx->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops seninf_ctrl_ops = {
	.s_ctrl = mtk_cam_seninf_set_ctrl,
};

static const char * const seninf_test_pattern_menu[] = {
	"Disabled",
	"generate_test_pattern",
};

#ifdef SENINF_DEBUG
static const struct v4l2_ctrl_config cfg_test_streamon = {
	.ops = &seninf_ctrl_ops,
	.id = V4L2_CID_MTK_TEST_STREAMON,
	.name = "test_streamon",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.max = 1,
	.step = 1,
};
#endif

static int seninf_initialize_controls(struct seninf_ctx *ctx)
{
	struct v4l2_ctrl_handler *handler;
	struct v4l2_ctrl *test_pattern;
	int ret;

	handler = &ctx->ctrl_handler;
	ret = v4l2_ctrl_handler_init(handler, 2);
	if (ret)
		return ret;
	test_pattern =
	v4l2_ctrl_new_std_menu_items(handler, &seninf_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
		ARRAY_SIZE(seninf_test_pattern_menu) - 1,
		0, 0, seninf_test_pattern_menu);

#ifdef SENINF_DEBUG
	v4l2_ctrl_new_custom(handler, &cfg_test_streamon, NULL);
#endif

	if (handler->error) {
		ret = handler->error;
		dev_err(ctx->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	ctx->subdev.ctrl_handler = handler;
	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int register_subdev(struct seninf_ctx *ctx, struct v4l2_device *v4l2_dev)
{
	int i, ret;
	struct v4l2_subdev *sd = &ctx->subdev;
	struct device *dev = ctx->dev;
	struct media_pad *pads = ctx->pads;
	struct v4l2_async_notifier *notifier = &ctx->notifier;

	v4l2_subdev_init(sd, &seninf_subdev_ops);

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	sd->dev = dev;

	snprintf(sd->name, sizeof(sd->name), "%s-%s",
		 dev_driver_string(dev), csi_port_names[ctx->port]);

	v4l2_set_subdevdata(sd, ctx);

	sd->entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	sd->entity.ops = &seninf_media_ops;

	pads[PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	for (i = PAD_SRC_RAW0; i < PAD_MAXCNT; i++)
		pads[i].flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&sd->entity, PAD_MAXCNT, pads);
	if (ret < 0) {
		dev_err(dev, "failed to init pads\n");
		return ret;
	}

	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret < 0) {
		dev_err(dev, "failed to register subdev\n");
		return ret;
	}

	v4l2_async_notifier_init(notifier);
	ret = v4l2_async_notifier_parse_fwnode_endpoints
		(dev, notifier, sizeof(struct v4l2_async_subdev), NULL);
	if (ret < 0)
		dev_warn(dev, "no endpoint\n");

	notifier->ops = &seninf_async_ops;
	ret = v4l2_async_notifier_register(v4l2_dev, notifier);
	if (ret < 0) {
		dev_err(dev, "failed to register notifier\n");
		goto err_unregister_subdev;
	}

	return 0;

err_unregister_subdev:
	v4l2_device_unregister_subdev(sd);
	v4l2_async_notifier_cleanup(notifier);

	return ret;
}

static void unregister_subdev(struct seninf_ctx *ctx)
{
	struct v4l2_subdev *sd = &ctx->subdev;

	v4l2_async_notifier_unregister(&ctx->notifier);
	v4l2_async_notifier_cleanup(&ctx->notifier);
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
}

static int seninf_comp_bind(struct device *dev, struct device *master,
			    void *data)
{
	struct mtk_cam_device *cam_dev = data;
	struct v4l2_device *v4l2_dev = &cam_dev->v4l2_dev;
	struct seninf_ctx *ctx = dev_get_drvdata(dev);

	return register_subdev(ctx, v4l2_dev);
}

static void seninf_comp_unbind(struct device *dev, struct device *master,
			       void *data)
{
	struct seninf_ctx *ctx = dev_get_drvdata(dev);

	unregister_subdev(ctx);
}

static const struct component_ops seninf_comp_ops = {
	.bind = seninf_comp_bind,
	.unbind = seninf_comp_unbind,
};

static int seninf_probe(struct platform_device *pdev)
{
	int ret, port;
	struct seninf_ctx *ctx;
	struct device *dev = &pdev->dev;
	struct seninf_core *core;

	if (!dev->parent)
		return -EPROBE_DEFER;

	core = dev_get_drvdata(dev->parent);
	if (!core)
		return -EPROBE_DEFER;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	dev_set_drvdata(dev, ctx);
	ctx->dev = dev;
	ctx->core = core;
	list_add(&ctx->list, &core->list);
	INIT_LIST_HEAD(&ctx->list_mux);
	INIT_LIST_HEAD(&ctx->list_cam_mux);

	ret = get_csi_port(dev, &port);
	if (ret) {
		dev_err(dev, "get_csi_port ret %d\n", ret);
		return ret;
	}

	mtk_cam_seninf_init_iomem(ctx, core->reg_if, core->reg_ana);
	mtk_cam_seninf_init_port(ctx, port);
	init_fmt(ctx);

	ret = seninf_initialize_controls(ctx);
	if (ret) {
		dev_err(dev, "Failed to initialize controls\n");
		return ret;
	}

	ret = component_add(dev, &seninf_comp_ops);
	if (ret < 0) {
		dev_err(dev, "component_add failed\n");
		goto err_free_handler;
	}

	pm_runtime_enable(dev);

	dev_info(dev, "%s: port=%d\n", __func__, ctx->port);

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);

	return ret;
}

static int runtime_suspend(struct device *dev)
{
	int i;
	struct seninf_ctx *ctx = dev_get_drvdata(dev);
	struct seninf_core *core = ctx->core;

	mutex_lock(&core->mutex);

	core->refcnt--;

	if (core->refcnt == 0) {
		i = CLK_TOP_SENINF_END;
		do {
			i--;
			clk_disable_unprepare(ctx->core->clk[i]);
		} while (i);
		pm_runtime_put(ctx->core->dev);
	}

	mutex_unlock(&core->mutex);

	return 0;
}

static int runtime_resume(struct device *dev)
{
	int i;
	struct seninf_ctx *ctx = dev_get_drvdata(dev);
	struct seninf_core *core = ctx->core;

	mutex_lock(&core->mutex);

	core->refcnt++;

	if (core->refcnt == 1) {
		pm_runtime_get_sync(core->dev);
		for (i = 0; i < CLK_TOP_SENINF_END; i++)
			clk_prepare_enable(core->clk[i]);
		mtk_cam_seninf_disable_all_mux(ctx);
		mtk_cam_seninf_disable_all_cammux(ctx);
	}

	mutex_unlock(&core->mutex);

	return 0;
}

static const struct dev_pm_ops pm_ops = {
	SET_RUNTIME_PM_OPS(runtime_suspend, runtime_resume, NULL)
};

static int seninf_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct seninf_ctx *ctx = dev_get_drvdata(dev);

	if (ctx->streaming) {
		mtk_cam_seninf_set_idle(ctx);
		mtk_cam_seninf_release_mux(ctx);
	}

	pm_runtime_disable(ctx->dev);

	component_del(dev, &seninf_comp_ops);

	v4l2_ctrl_handler_free(&ctx->ctrl_handler);

	dev_info(dev, "%s\n", __func__);

	return 0;
}

static const struct of_device_id seninf_of_match[] = {
	{.compatible = "mediatek,seninf"},
	{},
};
MODULE_DEVICE_TABLE(of, seninf_of_match);

static int seninf_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int seninf_resume(struct platform_device *pdev)
{
	return 0;
}

struct platform_driver seninf_pdrv = {
	.probe	= seninf_probe,
	.remove	= seninf_remove,
	.suspend = seninf_suspend,
	.resume = seninf_resume,
	.driver	= {
		.name = "seninf",
		.of_match_table = seninf_of_match,
		.pm  = &pm_ops,
	},
};

int mtk_cam_seninf_get_pixelrate(struct v4l2_subdev *sd, s64 *p_pixel_rate)
{
	int ret;
	s64 pixel_rate = -1;
	struct seninf_ctx *ctx = sd_to_ctx(sd);

	if (!ctx->sensor_sd) {
		dev_err(ctx->dev, "no sensor\n");
		return -EFAULT;
	}

	ret = get_buffered_pixel_rate(ctx,
				      ctx->sensor_sd, ctx->sensor_pad_idx,
				      &pixel_rate);
	if (ret)
		get_pixel_rate(ctx, ctx->sensor_sd, &pixel_rate);

	if (pixel_rate <= 0) {
		dev_warn(ctx->dev, "failed to get pixel_rate\n");
		return -EINVAL;
	}

	*p_pixel_rate = pixel_rate;

	return 0;
}

