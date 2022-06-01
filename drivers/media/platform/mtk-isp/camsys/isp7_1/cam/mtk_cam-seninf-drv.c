// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_domain.h>
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
#include "imgsensor-user.h"
#include "mtk_cam-seninf-ca.h"

#define ESD_RESET_SUPPORT 1
#define V4L2_CID_MTK_SENINF_BASE	(V4L2_CID_USER_BASE | 0xf000)
#define V4L2_CID_MTK_TEST_STREAMON	(V4L2_CID_MTK_SENINF_BASE + 1)

#define sd_to_ctx(__sd) container_of(__sd, struct seninf_ctx, subdev)
#define notifier_to_ctx(__n) container_of(__n, struct seninf_ctx, notifier)
#define ctrl_hdl_to_ctx(__h) container_of(__h, struct seninf_ctx, ctrl_handler)
#define sizeof_u32(__struct_name__) (sizeof(__struct_name__) / sizeof(u32))
#define sizeof_u16(__struct_name__) (sizeof(__struct_name__) / sizeof(u16))

struct mtk_cam_seninf_ops *g_seninf_ops;


#ifdef CSI_EFUSE_SET
#include <linux/nvmem-consumer.h>
#endif
static const char * const csi_phy_versions[] = {
	MTK_CSI_PHY_VERSIONS
};


static const char * const csi_port_names[] = {
	SENINF_CSI_PORT_NAMES
};

static const char * const clk_names[] = {
	SENINF_CLK_NAMES
};

static const char * const set_reg_names[] = {
	SET_REG_KEYS_NAMES
};

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	return g_seninf_ops->_show_status(dev, attr, buf);
}

static DEVICE_ATTR_RO(status);

static ssize_t err_status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	return g_seninf_ops->_show_err_status(dev, attr, buf);
}

static DEVICE_ATTR_RO(err_status);

static ssize_t debug_ops_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "This is debug ops message\n");

	return len;
}

enum REG_OPS_CMD {
	REG_OPS_CMD_ID,
	REG_OPS_CMD_CSI,
	REG_OPS_CMD_RG,
	REG_OPS_CMD_VAL,
	REG_OPS_CMD_MAX_NUM,
};

static ssize_t debug_ops_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	char delim[] = " ";
	char csi_names[20];
	char *token = NULL;
	char *sbuf = kzalloc(sizeof(char) * (count + 1), GFP_KERNEL);
	char *s = sbuf;
	int ret, i;
	unsigned int num_para = 0;
	char *arg[REG_OPS_CMD_MAX_NUM];
	struct seninf_core *core = dev_get_drvdata(dev);
	struct seninf_ctx *ctx;
	int csi_port = -1;
	int rg_idx = -1;
	u32 val;

	if (!sbuf)
		goto ERR_DEBUG_OPS_STORE;

	memcpy(sbuf, buf, count);

	token = strsep(&s, delim);
	while (token != NULL && num_para < REG_OPS_CMD_MAX_NUM) {
		if (strlen(token)) {
			arg[num_para] = token;
			num_para++;
		}

		token = strsep(&s, delim);
	}

	if (num_para != REG_OPS_CMD_MAX_NUM) {
		dev_info(dev, "Wrong command parameter number\n");
		goto ERR_DEBUG_OPS_STORE;
	}

	if (strncmp("SET_REG", arg[REG_OPS_CMD_ID], sizeof("SET_REG")) == 0) {
		for (i = 0; i < REG_KEY_MAX_NUM; i++) {
			if (!strcasecmp(arg[REG_OPS_CMD_RG], set_reg_names[i]))
				rg_idx = i;
		}
		if (rg_idx < 0)
			goto ERR_DEBUG_OPS_STORE;

		ret = kstrtouint(arg[REG_OPS_CMD_VAL], 0, &val);
		if (ret)
			goto ERR_DEBUG_OPS_STORE;

		for (i = 0; i < CSI_PORT_MAX_NUM; i++) {
			memset(csi_names, 0, ARRAY_SIZE(csi_names));
			ret = snprintf(csi_names, 10, "csi-%s", csi_port_names[i]);
			if (ret < 0) {
				dev_info(dev, "fail to snprintf\n");
				goto ERR_DEBUG_OPS_STORE;
			}
			if (!strcasecmp(arg[REG_OPS_CMD_CSI], csi_names))
				csi_port = i;
		}

		if (csi_port < 0)
			goto ERR_DEBUG_OPS_STORE;

		// reg call
		mutex_lock(&core->mutex);

		list_for_each_entry(ctx, &core->list, list) {
			if (csi_port == ctx->port)
				g_seninf_ops->_set_reg(ctx, rg_idx, val);
		}

		mutex_unlock(&core->mutex);
	}

ERR_DEBUG_OPS_STORE:

	kfree(sbuf);

	return count;
}

static DEVICE_ATTR_RW(debug_ops);

#define SENINF_DVFS_READY
#ifdef SENINF_DVFS_READY
static int seninf_dfs_init(struct seninf_dfs *dfs, struct device *dev)
{
	int ret, i;
	struct dev_pm_opp *opp;
	unsigned long freq;

	dfs->dev = dev;

	ret = dev_pm_opp_of_add_table(dev);
	if (ret < 0) {
		dev_info(dev, "fail to init opp table: %d\n", ret);
		return ret;
	}

	dfs->reg = devm_regulator_get_optional(dev, "dvfsrc-vmm");
	if (IS_ERR(dfs->reg)) {
		dev_info(dev, "can't get dvfsrc-vmm\n");
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
#endif

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

static int seninf_core_pm_runtime_enable(struct seninf_core *core)
{
	int i;

	core->pm_domain_cnt = of_count_phandle_with_args(core->dev->of_node,
					"power-domains",
					"#power-domain-cells");
	if (core->pm_domain_cnt == 1)
		pm_runtime_enable(core->dev);
	else if (core->pm_domain_cnt > 1) {
		core->pm_domain_devs = devm_kcalloc(core->dev, core->pm_domain_cnt,
					sizeof(*core->pm_domain_devs), GFP_KERNEL);
		if (!core->pm_domain_devs)
			return -ENOMEM;

		for (i = 0; i < core->pm_domain_cnt; i++) {
			core->pm_domain_devs[i] =
				dev_pm_domain_attach_by_id(core->dev, i);

			if (IS_ERR_OR_NULL(core->pm_domain_devs[i])) {
				dev_info(core->dev, "%s: fail to probe pm id %d\n",
					__func__, i);
				core->pm_domain_devs[i] = NULL;
			}
		}
	} else
		dev_info(core->dev, "core->pm_domain_cnt < 0\n");

	return 0;
}

static int seninf_core_pm_runtime_disable(struct seninf_core *core)
{
	int i;

	if (core->pm_domain_cnt == 1)
		pm_runtime_disable(core->dev);
	else if (core->pm_domain_cnt > 1) {
		if (!core->pm_domain_devs)
			return -ENOMEM;

		for (i = 0; i < core->pm_domain_cnt; i++) {
			if (core->pm_domain_devs[i] != NULL)
				dev_pm_domain_detach(core->pm_domain_devs[i], 1);
		}
	} else
		dev_info(core->dev, "core->pm_domain_cnt < 0\n");

	return 0;
}

static int seninf_core_pm_runtime_get_sync(struct seninf_core *core)
{
	int i;
	int ret = 0;

	if (core->pm_domain_cnt == 1) {
		ret = pm_runtime_get_sync(core->dev);
		if (ret < 0)
			dev_info(core->dev, "pm_runtime_get_sync fail\n");
	} else if (core->pm_domain_cnt > 1) {
		if (!core->pm_domain_devs)
			return -ENOMEM;

		for (i = 0; i < core->pm_domain_cnt; i++) {
			if (core->pm_domain_devs[i] != NULL) {
				ret = pm_runtime_get_sync(core->pm_domain_devs[i]);
				if (ret < 0)
					dev_info(core->dev, "pm_runtime_get_sync fail\n");
			}
		}
	} else
		dev_info(core->dev, "core->pm_domain_cnt < 0\n");

	return 0;
}

static int seninf_core_pm_runtime_put(struct seninf_core *core)
{
	int i;
	int ret = 0;

	if (core->pm_domain_cnt == 1) {
		ret = pm_runtime_put_sync(core->dev);
		if (ret < 0)
			dev_info(core->dev, "pm_runtime_put_sync fail\n");
	} else if (core->pm_domain_cnt > 1) {
		if (!core->pm_domain_devs)
			return -ENOMEM;

		for (i = core->pm_domain_cnt - 1; i >= 0; i--) {
			if (core->pm_domain_devs[i] != NULL) {
				ret = pm_runtime_put_sync(core->pm_domain_devs[i]);
				if (ret < 0)
					dev_info(core->dev, "pm_runtime_put_sync fail\n");
			}
		}
	} else
		dev_info(core->dev, "core->pm_domain_cnt < 0\n");

	return 0;
}

static irqreturn_t mtk_irq_seninf(int irq, void *data)
{
	g_seninf_ops->_irq_handler(irq, data);
	return IRQ_HANDLED;
}

static int get_seninf_ops(struct device *dev, struct seninf_core *core)
{
	int i, ret;
	const char *ver;

	ret = of_property_read_string(dev->of_node, "mtk_csi_phy_ver", &ver);
	if (ret) {
		g_seninf_ops = &mtk_csi_phy_3_0;
		of_property_read_u32(dev->of_node, "seninf_num",
			&g_seninf_ops->seninf_num);
		of_property_read_u32(dev->of_node, "mux_num",
			&g_seninf_ops->mux_num);
		of_property_read_u32(dev->of_node, "cam_mux_num",
			&g_seninf_ops->cam_mux_num);
		of_property_read_u32(dev->of_node, "pref_mux_num",
			&g_seninf_ops->pref_mux_num);

		dev_info(dev, "%s: seninf_num = %d, mux_num = %d, cam_mux_num = %d, pref_mux_num =%d\n",
			__func__,
			g_seninf_ops->seninf_num,
			g_seninf_ops->mux_num,
			g_seninf_ops->cam_mux_num,
			g_seninf_ops->pref_mux_num);

		return 0;
	}
	for (i = 0; i < SENINF_PHY_VER_NUM; i++) {
		if (!strcasecmp(ver, csi_phy_versions[i])) {
			if (i == SENINF_PHY_2_0)
				g_seninf_ops = &mtk_csi_phy_2_0;
			else
				g_seninf_ops = &mtk_csi_phy_3_0;

			dev_info(dev, "%s: mtk_csi_phy_2_0 = 0x%x mtk_csi_phy_3_0 = 0x%x\n",
			__func__,
			&mtk_csi_phy_2_0, &mtk_csi_phy_3_0);

			dev_info(dev, "%s: mtk_csi_phy_ver = %s i = %d 0x%x ret = %d\n",
			__func__,
			csi_phy_versions[i], i, g_seninf_ops, ret);

			of_property_read_u32(dev->of_node, "seninf_num",
				&g_seninf_ops->seninf_num);
			of_property_read_u32(dev->of_node, "mux_num",
				&g_seninf_ops->mux_num);
			of_property_read_u32(dev->of_node, "cam_mux_num",
				&g_seninf_ops->cam_mux_num);
			of_property_read_u32(dev->of_node, "pref_mux_num",
				&g_seninf_ops->pref_mux_num);


			dev_info(dev, "%s: seninf_num = %d, mux_num = %d, cam_mux_num = %d, pref_mux_num =%d\n",
				__func__,
				g_seninf_ops->seninf_num,
				g_seninf_ops->mux_num,
				g_seninf_ops->cam_mux_num,
				g_seninf_ops->pref_mux_num);
			return 0;
		}
	}

	return -1;
}

static int seninf_core_probe(struct platform_device *pdev)
{
	int i, ret, irq;
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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	core->reg_if = devm_ioremap_resource(dev, res);
	if (IS_ERR(core->reg_if))
		return PTR_ERR(core->reg_if);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ana-rx");
	core->reg_ana = devm_ioremap_resource(dev, res);
	if (IS_ERR(core->reg_ana))
		return PTR_ERR(core->reg_ana);

	ret = get_seninf_ops(dev, core);
	if (ret) {
		dev_dbg(dev, "failed to get seninf ops\n");
		return ret;
	}
	mtk_cam_seninf_init_res(core);

	spin_lock_init(&core->spinlock_irq);
	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_dbg(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(dev, irq, mtk_irq_seninf, 0,
				dev_name(dev), core);
	if (ret) {
		dev_dbg(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", irq);

	/* default platform properties */
	core->cphy_settle_delay_dt = SENINF_CPHY_SETTLE_DELAY_DT;
	core->dphy_settle_delay_dt = SENINF_DPHY_SETTLE_DELAY_DT;
	core->settle_delay_ck = SENINF_SETTLE_DELAY_CK;
	core->hs_trail_parameter = SENINF_HS_TRAIL_PARAMETER;

	/* read platform properties from device tree */
	of_property_read_u32(dev->of_node, "cphy_settle_delay_dt",
		&core->cphy_settle_delay_dt);
	of_property_read_u32(dev->of_node, "dphy_settle_delay_dt",
		&core->dphy_settle_delay_dt);
	of_property_read_u32(dev->of_node, "settle_delay_ck",
		&core->settle_delay_ck);
	of_property_read_u32(dev->of_node, "hs_trail_parameter",
		&core->hs_trail_parameter);

	for (i = 0; i < CLK_MAXCNT; i++) {
		core->clk[i] = devm_clk_get(dev, clk_names[i]);
		if (IS_ERR(core->clk[i])) {
			dev_info(dev, "failed to get %s\n", clk_names[i]);
			core->clk[i] = NULL;
			//return -EINVAL;
		}
	}

	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret) {
		dev_info(dev, "%s: failed to create sub devices\n", __func__);
		return ret;
	}

#ifdef SENINF_DVFS_READY
	ret = seninf_dfs_init(&core->dfs, dev);
	if (ret) {
		dev_info(dev, "%s: failed to init dfs\n", __func__);
		//return ret;
	}
#endif

	ret = device_create_file(dev, &dev_attr_status);
	if (ret)
		dev_info(dev, "failed to create sysfs status\n");

	ret = device_create_file(dev, &dev_attr_debug_ops);
	if (ret)
		dev_info(dev, "failed to create sysfs debug ops\n");

	ret = device_create_file(dev, &dev_attr_err_status);
	if (ret)
		dev_info(dev, "failed to create sysfs status\n");

	seninf_core_pm_runtime_enable(core);

	kthread_init_worker(&core->seninf_worker);
	core->seninf_kworker_task = kthread_run(kthread_worker_fn,
				&core->seninf_worker, "seninf_worker");
	if (IS_ERR(core->seninf_kworker_task)) {
		dev_info(dev, "%s: failed to start seninf kthread worker\n",
			__func__);
		core->seninf_kworker_task = NULL;
	}

	dev_info(dev, "%s\n", __func__);

	return 0;
}

static int seninf_core_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct seninf_core *core = dev_get_drvdata(dev);

	seninf_dfs_exit(&core->dfs);
	of_platform_depopulate(dev);
	seninf_core_pm_runtime_disable(core);
	device_remove_file(dev, &dev_attr_status);
	device_remove_file(dev, &dev_attr_debug_ops);
	device_remove_file(dev, &dev_attr_err_status);

	if (core->seninf_kworker_task)
		kthread_stop(core->seninf_kworker_task);

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
#ifdef CSI_EFUSE_SET
static int dev_read_csi_efuse(struct seninf_ctx *ctx)
{
	struct nvmem_cell *cell;
	size_t len = 0;
	u32 *buf;

	ctx->m_csi_efuse = 0x00000000;

	cell = nvmem_cell_get(ctx->dev, "rg_csi");
	dev_info(ctx->dev, "ctx->port = %d\n", ctx->port);
	if (IS_ERR(cell)) {
		if (PTR_ERR(cell) == -EPROBE_DEFER) {
			dev_info(ctx->dev, "read csi efuse returned with error cell %d\n",
				-EPROBE_DEFER-EPROBE_DEFER);
			return PTR_ERR(cell);
		}
		dev_info(ctx->dev, "read csi efuse returned with error cell %d\n", -1);
		return -1;
	}
	buf = (u32 *)nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);
	if (IS_ERR(buf)) {
		dev_info(ctx->dev, "read csi efuse returned with error buf\n");
		return PTR_ERR(buf);
	}
	ctx->m_csi_efuse = *buf;
	kfree(buf);
	dev_info(ctx->dev, "Efuse Data: 0x%08x\n", ctx->m_csi_efuse);

	return 0;
}
#endif
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

	if (fmt->pad >= PAD_MAXCNT)
		return -EINVAL;

	format = &ctx->fmt[fmt->pad].format;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
		dev_dbg(ctx->dev, "s_fmt pad %d code/res 0x%x/%dx%d which %d=> 0x%x/%dx%d\n",
			fmt->pad,
			fmt->format.code,
			fmt->format.width,
			fmt->format.height,
			fmt->which,
			format->code,
			format->width,
			format->height);
	} else {
		/* NOTE: update vcinfo once the SINK format changed */
		if (fmt->pad == PAD_SINK)
			bSinkFormatChanged = 1;

		format->code = fmt->format.code;
		format->width = fmt->format.width;
		format->height = fmt->format.height;

		if (bSinkFormatChanged && !ctx->is_test_model)
			mtk_cam_seninf_get_vcinfo(ctx);

		dev_info(ctx->dev, "s_fmt pad %d code/res 0x%x/%dx%d which %d=> 0x%x/%dx%d\n",
			fmt->pad,
			fmt->format.code,
			fmt->format.width,
			fmt->format.height,
			fmt->which,
			format->code,
			format->width,
			format->height);
	}

	return 0;
}

static int mtk_cam_seninf_get_fmt(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	struct v4l2_mbus_framefmt *format;

	if (fmt->pad >= PAD_MAXCNT)
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

	return 0;
}

static int set_test_model(struct seninf_ctx *ctx, char enable)
{
	struct seninf_vc *vc[] = {NULL, NULL, NULL, NULL, NULL};
	int i = 0, vc_used = 0;
	struct seninf_mux *mux;
	struct seninf_dfs *dfs = &ctx->core->dfs;
	int pref_idx[] = {0, 1, 2, 3, 4}; //FIXME
	int ret = 0;

	if (ctx->is_test_model == 1) {
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
	} else if (ctx->is_test_model == 2) {
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW1);
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW2);
	} else if (ctx->is_test_model == 3) {
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_PDAF0);
	} else if (ctx->is_test_model == 4) {
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW1);
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW2);
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_PDAF0);
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_PDAF1);
	} else if (ctx->is_test_model == 5) {
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
		vc[vc_used++] = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW_W0);
	} else {
		dev_info(ctx->dev, "testmodel %d invalid\n", ctx->is_test_model);
		return -1;
	}

	for (; i < vc_used; ++i) {
		if (!vc[i]) {
			dev_info(ctx->dev, "vc not found\n");
			return -1;
		}
	}

	if (enable) {
		pm_runtime_get_sync(ctx->dev);
		if (ctx->core->clk[CLK_TOP_CAMTM]) {
			ret = clk_prepare_enable(ctx->core->clk[CLK_TOP_CAMTM]);
			if (ret < 0)
				dev_info(ctx->dev, "clk_prepare_enable fail\n");
		}

		if (dfs->cnt)
			seninf_dfs_set(ctx, dfs->freqs[dfs->cnt - 1]);

		for (i = 0; i < vc_used; ++i) {
			mux = mtk_cam_seninf_mux_get_pref(ctx,
							  pref_idx,
							  ARRAY_SIZE(pref_idx));
			if (!mux)
				return -EBUSY;
			vc[i]->mux = mux->idx;
			vc[i]->cam = ctx->pad2cam[vc[i]->out_pad];
			vc[i]->enable = 1;

			dev_info(ctx->dev, "test mode mux %d, cam %d, pixel mode %d\n",
					vc[i]->mux, vc[i]->cam, vc[i]->pixel_mode);

			g_seninf_ops->_set_test_model(ctx,
					vc[i]->mux, vc[i]->cam, vc[i]->pixel_mode);

			if (vc[i]->out_pad == PAD_SRC_PDAF0) {
				mdelay(40);
			} else {
				udelay(40);
			}
		}
	} else {
		g_seninf_ops->_set_idle(ctx);
		mtk_cam_seninf_release_mux(ctx);
		if (dfs->cnt)
			seninf_dfs_set(ctx, 0);

		if (ctx->core->clk[CLK_TOP_CAMTM])
			clk_disable_unprepare(ctx->core->clk[CLK_TOP_CAMTM]);
		pm_runtime_put_sync(ctx->dev);
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
	struct seninf_mux *mux, *mux_by_grp[SENINF_VC_MAXCNT] = {0};

	intf = ctx->seninfIdx;
	vcinfo = &ctx->vcinfo;

	mtk_cam_seninf_get_csi_param(ctx);

	g_seninf_ops->_reset(ctx, intf);

	g_seninf_ops->_set_vc(ctx, intf, vcinfo);

	g_seninf_ops->_set_csi_mipi(ctx);

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
			int pref_idx[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
					10, 11, 12, 13, 14, 15,	16, 17, 18, 19, 20, 21 };
			mux = mux_by_grp[vc->group] =
				//mtk_cam_seninf_mux_get(ctx);
				mtk_cam_seninf_mux_get_pref(ctx,
						pref_idx,
						g_seninf_ops->pref_mux_num);
			skip_mux_ctrl = 0;
		}

		if (!mux) {
			mtk_cam_seninf_release_mux(ctx);
			return -EBUSY;
		}

		vc->mux = mux->idx;
		vc->cam = ctx->pad2cam[vc->out_pad];

		if (!skip_mux_ctrl) {
			g_seninf_ops->_mux(ctx, vc->mux);
			g_seninf_ops->_set_mux_ctrl(ctx, vc->mux,
						    hsPol, vsPol,
				MIPI_SENSOR + vc->group,
				vc->pixel_mode);

			g_seninf_ops->_set_top_mux_ctrl(ctx, vc->mux, intf);

			//TODO
			//mtk_cam_seninf_set_mux_crop(ctx, vc->mux, 0, 2327, 0);
		}
		dev_info(ctx->dev, "ctx->pad2cam[%d] %d vc->out_pad %d vc->cam %d, i %d",
			vc->out_pad, ctx->pad2cam[vc->out_pad], vc->out_pad, vc->cam, i);

		if (vc->cam != 0xff) {
			vc_sel = vc->vc;
			dt_sel = vc->dt;
			dt_en = !!dt_sel;

			/* CMD_SENINF_FINALIZE_CAM_MUX */
			g_seninf_ops->_set_cammux_vc(ctx, vc->cam,
						     vc_sel, dt_sel, dt_en, dt_en);
			g_seninf_ops->_set_cammux_src(ctx, vc->mux, vc->cam,
						      vc->exp_hsize, vc->exp_vsize, vc->dt);
			g_seninf_ops->_set_cammux_chk_pixel_mode(ctx,
								 vc->cam,
								 vc->pixel_mode);
			g_seninf_ops->_cammux(ctx, vc->cam);

			dev_info(ctx->dev, "vc[%d] pad %d intf %d mux %d cam %d\n",
				 i, vc->out_pad, intf, vc->mux, vc->cam,
				vc_sel, dt_sel);

#ifdef SENSOR_SECURE_MTEE_SUPPORT
			if (ctx->is_secure == 1) {
				dev_info(ctx->dev, "Sensor kernel init seninf_ca");
				if (!seninf_ca_open_session())
					dev_info(ctx->dev, "seninf_ca_open_session fail");

				dev_info(ctx->dev, "Sensor kernel ca_checkpipe");
				seninf_ca_checkpipe(ctx->SecInfo_addr);
			}
#endif

		} else
			dev_info(ctx->dev, "not set camtg yet, vc[%d] pad %d intf %d mux %d cam %d\n",
					 i, vc->out_pad, intf, vc->mux, vc->cam);
	}
	return 0;
}

static int calc_buffered_pixel_rate(struct device *dev,
				    s64 width, s64 height,
				    s64 hblank, s64 vblank,
				    int fps_n, int fps_d, s64 *result)
{
	s64 buffered_pixel_rate;
	s64 orig_pixel_rate = *result;
	s64 pclk;
	s64 k;

	/* calculate pclk */
	pclk = (width + hblank) * (height + vblank) * fps_n;
	do_div(pclk, fps_d);

	/* calculate buffered pixel_rate */
	buffered_pixel_rate = orig_pixel_rate * width;
	k = HW_BUF_EFFECT * orig_pixel_rate;
	do_div(k, ISP_CLK_LOW);
	do_div(buffered_pixel_rate, (width + hblank - k));
	*result = buffered_pixel_rate;

	dev_info(
		dev,
		"%s: w %d h %d hb %d vb %d fps %d/%d pclk %lld->%lld orig %lld k %lld hbe %d\n",
		__func__, width, height, hblank, vblank,
		fps_n, fps_d, pclk, buffered_pixel_rate, orig_pixel_rate, k, HW_BUF_EFFECT);

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
	s64 width, height, hblank, vblank;

	fmt.pad = sd_pad_idx;
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
	if (ret) {
		dev_info(ctx->dev, "no get_fmt in %s\n", sd->name);
		return ret;
	}

	width = fmt.format.width;
	height = fmt.format.height;

	memset(&fi, 0, sizeof(fi));
	fi.pad = sd_pad_idx;
	fi.reserved[0] = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(sd, video, g_frame_interval, &fi);
	if (ret) {
		dev_info(ctx->dev, "no g_frame_interval in %s\n", sd->name);
		return ret;
	}

	ctrl = v4l2_ctrl_find(sd->ctrl_handler, V4L2_CID_HBLANK);
	if (!ctrl) {
		dev_info(ctx->dev, "no hblank in %s\n", sd->name);
		return -EINVAL;
	}

	hblank = v4l2_ctrl_g_ctrl(ctrl);

	ctrl = v4l2_ctrl_find(sd->ctrl_handler, V4L2_CID_VBLANK);
	if (!ctrl) {
		dev_info(ctx->dev, "no vblank in %s\n", sd->name);
		return -EINVAL;
	}

	vblank = v4l2_ctrl_g_ctrl(ctrl);

	/* update fps */
	ctx->fps_n = fi.interval.denominator;
	ctx->fps_d = fi.interval.numerator;

	return calc_buffered_pixel_rate(ctx->dev, width, height, hblank, vblank,
					ctx->fps_n, ctx->fps_d, result);
}

static int get_customized_pixel_rate(struct seninf_ctx *ctx, struct v4l2_subdev *sd,
			  s64 *result)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sd->ctrl_handler, V4L2_CID_MTK_CUST_SENSOR_PIXEL_RATE);
	if (!ctrl) {
		dev_info(ctx->dev, "no cust pixel rate in subdev %s\n", sd->name);
		return -EINVAL;
	}

	*result = v4l2_ctrl_g_ctrl(ctrl);

	return 0;
}

static int get_pixel_rate(struct seninf_ctx *ctx, struct v4l2_subdev *sd,
			  s64 *result)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sd->ctrl_handler, V4L2_CID_PIXEL_RATE);
	if (!ctrl) {
		dev_info(ctx->dev, "no pixel rate in subdev %s\n", sd->name);
		return -EINVAL;
	}

	*result = v4l2_ctrl_g_ctrl_int64(ctrl);

	return 0;
}

static int get_mbus_config(struct seninf_ctx *ctx, struct v4l2_subdev *sd)
{
	struct v4l2_mbus_config cfg = {0};
	int ret;

	ret = v4l2_subdev_call(sd, pad, get_mbus_config, ctx->sensor_pad_idx, &cfg);
	if (ret) {
		dev_info(ctx->dev, "no g_mbus_config in %s\n",
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

//static int update_isp_clk(struct seninf_ctx *ctx)
int update_isp_clk(struct seninf_ctx *ctx)
{
	int i, ret, pixelmode;
	struct seninf_dfs *dfs = &ctx->core->dfs;
	s64 pixel_rate = -1, dfs_freq;
	struct seninf_vc *vc;


	if (!dfs->cnt)
		return 0;

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
	if (!vc) {
		dev_info(ctx->dev, "failed to get vc SRC_RAW0, try EXT0\n");

		vc = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW_EXT0);
		if (!vc) {
			dev_info(ctx->dev, "failed to get vc SRC_RAW_EXT0\n");

			return -1;
		}
	}
	dev_info(ctx->dev,
		"%s dfs->cnt %d pixel mode %d customized_pixel_rate %lld, buffered_pixel_rate %lld mipi_pixel_rate %lld\n",
		__func__,
		dfs->cnt,
		vc->pixel_mode,
		ctx->customized_pixel_rate,
		ctx->buffered_pixel_rate,
		ctx->mipi_pixel_rate);
	//Use SensorPixelrate
	if (ctx->customized_pixel_rate)
		pixel_rate = ctx->customized_pixel_rate;
	else if (ctx->buffered_pixel_rate)
		pixel_rate = ctx->buffered_pixel_rate;
	else if (ctx->mipi_pixel_rate)
		pixel_rate = ctx->mipi_pixel_rate;
	else {
		dev_info(ctx->dev, "failed to get pixel_rate\n");
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
		dev_info(ctx->dev, "mux is overrun. please adjust pixelmode\n");
		return -EINVAL;
	}

	ret = seninf_dfs_set(ctx, dfs->freqs[i]);
	if (ret) {
		dev_info(ctx->dev, "failed to set freq\n");
		return ret;
	}

	return 0;
}

static int debug_err_detect_initialize(struct seninf_ctx *ctx)
{
	struct seninf_core *core;
	struct seninf_ctx *ctx_;

	core = dev_get_drvdata(ctx->dev->parent);

	core->csi_irq_en_flag = 0;
	core->vsync_irq_en_flag = 0;

	list_for_each_entry(ctx_, &core->list, list) {
		ctx_->data_not_enough_flag = 0;
		ctx_->err_lane_resync_flag = 0;
		ctx_->crc_err_flag = 0;
		ctx_->ecc_err_double_flag = 0;
		ctx_->ecc_err_corrected_flag = 0;
		ctx_->fifo_overrun_flag = 0;
		ctx_->size_err_flag = 0;
		ctx_->data_not_enough_cnt = 0;
		ctx_->err_lane_resync_cnt = 0;
		ctx_->crc_err_cnt = 0;
		ctx_->ecc_err_double_cnt = 0;
		ctx_->ecc_err_corrected_cnt = 0;
		ctx_->fifo_overrun_cnt = 0;
		ctx_->size_err_cnt = 0;
	}

	return 0;
}

static int mtk_senif_get_ccu_phandle(struct seninf_core *core)
{
	struct device *dev = core->dev;
	struct device_node *node;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,camera_fsync_ccu");
	if (node == NULL) {
		dev_info(dev, "of_find mediatek,camera_fsync_ccu fail\n");
		ret = PTR_ERR(node);
		goto out;
	}

	ret = of_property_read_u32(node, "mediatek,ccu_rproc",
				   &core->rproc_ccu_phandle);
	if (ret) {
		dev_info(dev, "fail to get rproc_ccu_phandle:%d\n", ret);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

static int mtk_senif_power_ctrl_ccu(struct seninf_core *core, int on_off)
{
	int ret = 0;

	if (on_off) {
		ret = mtk_senif_get_ccu_phandle(core);
		if (ret)
			goto out;
		core->rproc_ccu_handle = rproc_get_by_phandle(core->rproc_ccu_phandle);
		if (core->rproc_ccu_handle == NULL) {
			dev_info(core->dev, "Get ccu handle fail\n");
			ret = PTR_ERR(core->rproc_ccu_handle);
			goto out;
		}

		ret = rproc_boot(core->rproc_ccu_handle);
		if (ret)
			dev_info(core->dev, "boot ccu rproc fail\n");

		if (core->dfs.reg) {
			ret = regulator_enable(core->dfs.reg);
			if (ret < 0)
				dev_info(core->dev, "regulator_enable dfs fail\n");
		}
	} else {
		if (core->dfs.reg && regulator_is_enabled(core->dfs.reg)) {
			ret = regulator_disable(core->dfs.reg);
			if (ret < 0)
				dev_info(core->dev, "regulator_disable dfs fail\n");
		}

		if (core->rproc_ccu_handle) {
			rproc_shutdown(core->rproc_ccu_handle);
			ret = 0;
		} else
			ret = -EINVAL;
	}
out:
	return ret;
}

static int seninf_s_stream(struct v4l2_subdev *sd, int enable)
{
#ifdef SENSOR_SECURE_MTEE_SUPPORT
	u32 ret_gz;
#endif
	int ret;
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	struct seninf_core *core = ctx->core;

	if (ctx->streaming == enable)
		return 0;

	if (ctx->is_test_model)
		return set_test_model(ctx, enable);

	if (!ctx->sensor_sd) {
		dev_info(ctx->dev, "no sensor\n");
		return -EFAULT;
	}

	if (enable) {
		debug_err_detect_initialize(ctx);
		get_mbus_config(ctx, ctx->sensor_sd);

		get_pixel_rate(ctx, ctx->sensor_sd, &ctx->mipi_pixel_rate);

		ctx->buffered_pixel_rate = ctx->mipi_pixel_rate;
		get_buffered_pixel_rate(ctx, ctx->sensor_sd,
					ctx->sensor_pad_idx, &ctx->buffered_pixel_rate);

		get_customized_pixel_rate(ctx, ctx->sensor_sd, &ctx->customized_pixel_rate);
		mtk_senif_power_ctrl_ccu(core, 1);
		ret = pm_runtime_get_sync(ctx->dev);
		if (ret < 0) {
			dev_info(ctx->dev, "%s pm_runtime_get_sync ret %d\n", __func__, ret);
			pm_runtime_put_noidle(ctx->dev);
			mtk_senif_power_ctrl_ccu(core, 0);
			return ret;
		}

		update_isp_clk(ctx);
		ret = config_hw(ctx);
		if (ret) {
			dev_info(ctx->dev, "config_seninf_hw ret %d\n", ret);
			return ret;
		}

		notify_fsync_cammux_usage(ctx);

#ifdef SENINF_VC_ROUTING
		//update_sensor_frame_desc(ctx);
#endif

		ret = v4l2_subdev_call(ctx->sensor_sd, video, s_stream, 1);
		if (ret) {
			dev_info(ctx->dev, "sensor stream-on ret %d\n", ret);
			return  ret;
		}
#ifdef SENINF_UT_DUMP
		g_seninf_ops->_debug(ctx);
#endif

	} else {
		ret = v4l2_subdev_call(ctx->sensor_sd, video, s_stream, 0);
		if (ret) {
			dev_info(ctx->dev, "sensor stream-off ret %d\n", ret);
			return ret;
		}
#ifdef SENSOR_SECURE_MTEE_SUPPORT
		if (ctx->is_secure == 1) {
			dev_info(ctx->dev, "sensor kernel ca_free");
			seninf_ca_free();

			dev_info(ctx->dev, "close seninf_ca");
			ret_gz = seninf_ca_close_session();
			ctx->is_secure = 0;
		}
#endif
		g_seninf_ops->_set_idle(ctx);
		mtk_cam_seninf_release_mux(ctx);
		seninf_dfs_set(ctx, 0);
		g_seninf_ops->_poweroff(ctx);
		ctx->dbg_last_dump_req = 0;
		pm_runtime_put_sync(ctx->dev);
		mtk_senif_power_ctrl_ccu(core, 0);
	}

	ctx->streaming = enable;
	return 0;
}

static const struct v4l2_subdev_pad_ops seninf_subdev_pad_ops = {
	.link_validate = mtk_cam_seninf_link_validate,
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
	if (sd != NULL) {
		ctx = v4l2_get_subdevdata(sd);

		if (local->flags & MEDIA_PAD_FL_SOURCE) {
			if (flags & MEDIA_LNK_FL_ENABLED) {
				if (!mtk_cam_seninf_get_vc_by_pad(ctx, local->index)) {
					dev_info(ctx->dev,
					"%s enable link w/o vc_info pad idex %d\n", local->index);
				}
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
				    &ctx->subdev.entity, 0,
				    MEDIA_LNK_FL_DYNAMIC);
	if (ret) {
		dev_info(ctx->dev,
			"failed to create link for %s\n",
			sd->entity.name);
		return ret;
	}

	ret = v4l2_device_register_subdev_nodes(ctx->subdev.v4l2_dev);
	if (ret) {
		dev_info(ctx->dev, "failed to create subdev nodes\n");
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
	case 1:// RAW only
	case 2:// Stagger: 3 expo
	case 3:// 1 RAW + 1 PD
	case 4:// 3 RAW + 2 PD
	case 5:// 1 RAW + 1 W channel
		if (ctx->streaming)
			return -EBUSY;
		ctx->is_test_model = pattern;
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
#ifdef SECURE_UT
	ctx->is_secure = 1;
	ctx->SecInfo_addr = 0x53;
#endif
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
	//struct seninf_vc *vc = NULL;

	switch (ctrl->id) {
	case V4L2_CID_TEST_PATTERN:
		ret = seninf_test_pattern(ctx, ctrl->val);
		break;
	case V4L2_CID_MTK_SENINF_S_STREAM:
		{
			ret = seninf_s_stream(&ctx->subdev, ctrl->val);
		}
		break;
#ifdef SENINF_DEBUG
	case V4L2_CID_MTK_TEST_STREAMON:
		ret = seninf_test_streamon(ctx, ctrl->val);
		break;
#endif
	default:
		ret = 0;
		dev_info(ctx->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops seninf_ctrl_ops = {
	.s_ctrl = mtk_cam_seninf_set_ctrl,
};

static int seninf_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	struct seninf_core *core;

	core = dev_get_drvdata(ctx->dev->parent);

	mutex_lock(&ctx->mutex);
	ctx->open_refcnt++;
	core->pid = find_get_pid(current->pid);

	if (ctx->open_refcnt == 1)
		dev_info(ctx->dev, "%s open_refcnt %d\n", __func__, ctx->open_refcnt);

	mutex_unlock(&ctx->mutex);

	return 0;
}

static int seninf_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);

	mutex_lock(&ctx->mutex);
	ctx->open_refcnt--;


	if (!ctx->open_refcnt) {
		dev_info(ctx->dev, "%s open_refcnt %d\n", __func__, ctx->open_refcnt);
#ifdef SENINF_DEBUG
		if (ctx->is_test_streamon)
			seninf_test_streamon(ctx, 0);
		else if (ctx->streaming)
#else
		if (ctx->streaming)
#endif
			seninf_s_stream(&ctx->subdev, 0);
	}

	mutex_unlock(&ctx->mutex);

	return 0;
}

static const struct v4l2_subdev_internal_ops seninf_internal_ops = {
	.open = seninf_open,
	.close = seninf_close,
};

static const char * const seninf_test_pattern_menu[] = {
	"Disabled",
	"generate_test_pattern",
	"generate_test_pattern_stagger",
	"generate_test_pattern_pd",
	"generate_test_pattern_5_src_pad",
	"generate_test_pattern_raw_and_w",
};

#ifdef SENINF_DEBUG
static const struct v4l2_ctrl_config cfg_test_streamon = {
	.ops = &seninf_ctrl_ops,
	.id = V4L2_CID_MTK_TEST_STREAMON,
	.name = "test_streamon",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 1,
	.step = 1,
};
#endif

static const struct v4l2_ctrl_config cfg_s_stream = {
	.ops = &seninf_ctrl_ops,
	.id = V4L2_CID_MTK_SENINF_S_STREAM,
	.name = "set_stream",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 1,
	.step = 1,
};

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
	v4l2_ctrl_new_custom(handler, &cfg_s_stream, NULL);


	if (handler->error) {
		ret = handler->error;
		dev_info(ctx->dev,
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

	if (strlen(dev->of_node->name) > 16) {
		ret = snprintf(sd->name, sizeof(sd->name), "%s-%s",
			dev_driver_string(dev), &dev->of_node->name[16]);
		if (ret < 0)
			dev_info(dev, "failed to snprintf\n");
	} else {
		ret = snprintf(sd->name, sizeof(sd->name), "%s-%s",
			dev_driver_string(dev), csi_port_names[(unsigned int)ctx->port]);
		if (ret < 0)
			dev_info(dev, "failed to snprintf\n");
	}

	v4l2_set_subdevdata(sd, ctx);

	sd->entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	sd->entity.ops = &seninf_media_ops;
	sd->internal_ops = &seninf_internal_ops;

	pads[PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	for (i = PAD_SRC_RAW0; i < PAD_MAXCNT; i++)
		pads[i].flags = MEDIA_PAD_FL_SOURCE;

	for (i = 0; i < PAD_MAXCNT; i++)
		ctx->pad2cam[i] = 0xff;

	ret = media_entity_pads_init(&sd->entity, PAD_MAXCNT, pads);
	if (ret < 0) {
		dev_info(dev, "failed to init pads\n");
		return ret;
	}

	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret < 0) {
		dev_info(dev, "failed to register subdev\n");
		return ret;
	}

	v4l2_async_notifier_init(notifier);
	ret = v4l2_async_notifier_parse_fwnode_endpoints
		(dev, notifier, sizeof(struct v4l2_async_subdev), NULL);
	if (ret < 0)
		dev_info(dev, "no endpoint\n");

	notifier->ops = &seninf_async_ops;
	ret = v4l2_async_notifier_register(v4l2_dev, notifier);
	if (ret < 0) {
		dev_info(dev, "failed to register notifier\n");
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

	ctx->open_refcnt = 0;
	mutex_init(&ctx->mutex);

	ret = get_csi_port(dev, &port);
	if (ret) {
		dev_info(dev, "get_csi_port ret %d\n", ret);
		return ret;
	}

	g_seninf_ops->_init_iomem(ctx, core->reg_if, core->reg_ana);
	g_seninf_ops->_init_port(ctx, port);
	init_fmt(ctx);

	/* default platform properties */
	//ctx->seninf_dphy_settle_delay_dt = 0;

	/* read platform properties from device tree */
	//of_property_read_u32(dev->of_node, "seninf_dphy_settle_delay_dt",
	//	&ctx->seninf_dphy_settle_delay_dt);
	//dev_info(dev, "seninf dphy settle delay dt = %u\n",
	//	 ctx->seninf_dphy_settle_delay_dt);

	ctx->cphy_settle_delay_dt = ctx->core->cphy_settle_delay_dt;
	ctx->dphy_settle_delay_dt = ctx->core->dphy_settle_delay_dt;
	ctx->settle_delay_ck = ctx->core->settle_delay_ck;
	ctx->hs_trail_parameter = ctx->core->hs_trail_parameter;

	of_property_read_u32(dev->of_node, "cphy_settle_delay_dt",
		&ctx->cphy_settle_delay_dt);
	of_property_read_u32(dev->of_node, "dphy_settle_delay_dt",
		&ctx->dphy_settle_delay_dt);
	of_property_read_u32(dev->of_node, "settle_delay_ck",
		&ctx->settle_delay_ck);
	of_property_read_u32(dev->of_node, "hs_trail_parameter",
		&ctx->hs_trail_parameter);

	dev_info(dev,
		"seninf d_settlte/d_settle_ck/d_trail/c_settle= %u/%u/%u/%u\n",
		ctx->seninf_dphy_settle_delay_dt,
		ctx->settle_delay_ck,
		ctx->hs_trail_parameter,
		ctx->cphy_settle_delay_dt);

#ifdef CSI_EFUSE_SET
	ret = dev_read_csi_efuse(ctx);
	if (ret < 0)
		dev_info(dev, "Failed to read efuse data\n");
#endif

	ret = seninf_initialize_controls(ctx);
	if (ret) {
		dev_info(dev, "Failed to initialize controls\n");
		return ret;
	}

	ret = component_add(dev, &seninf_comp_ops);
	if (ret < 0) {
		dev_info(dev, "component_add failed\n");
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
			if (ctx->core->clk[i])
				clk_disable_unprepare(ctx->core->clk[i]);
		} while (i);
		seninf_core_pm_runtime_put(core);
	}

	mutex_unlock(&core->mutex);

	return 0;
}

static int runtime_resume(struct device *dev)
{
	int i;
	struct seninf_ctx *ctx = dev_get_drvdata(dev);
	struct seninf_core *core = ctx->core;
	int ret = 0;

	mutex_lock(&core->mutex);

	core->refcnt++;

	if (core->refcnt == 1) {
		seninf_core_pm_runtime_get_sync(core);
		for (i = 0; i < CLK_TOP_SENINF_END; i++) {
			if (core->clk[i]) {
				ret = clk_prepare_enable(core->clk[i]);
				if (ret < 0)
					dev_info(dev, "clk_prepare_enable fail\n");
			}
		}
		g_seninf_ops->_disable_all_mux(ctx);
		g_seninf_ops->_disable_all_cammux(ctx);
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
		g_seninf_ops->_set_idle(ctx);
		mtk_cam_seninf_release_mux(ctx);
	}

	pm_runtime_disable(ctx->dev);

	component_del(dev, &seninf_comp_ops);

	v4l2_ctrl_handler_free(&ctx->ctrl_handler);

	mutex_destroy(&ctx->mutex);

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

int mtk_cam_seninf_calc_pixelrate(struct device *dev, s64 width, s64 height,
				  s64 hblank, s64 vblank,
				  int fps_n, int fps_d,
				  s64 sensor_pixel_rate)
{
	int ret;
	s64 p_pixel_rate = sensor_pixel_rate;

	ret = calc_buffered_pixel_rate(dev, width, height, hblank, vblank,
				       fps_n, fps_d, &p_pixel_rate);
	if (ret)
		return sensor_pixel_rate;

	return p_pixel_rate;
}

/* to be replaced by mtk_cam_seninf_calc_pixelrate() */
int mtk_cam_seninf_get_pixelrate(struct v4l2_subdev *sd, s64 *p_pixel_rate)
{
	int ret;
	s64 pixel_rate = -1;
	struct seninf_ctx *ctx = sd_to_ctx(sd);

	if (!ctx->sensor_sd) {
		dev_info(ctx->dev, "no sensor\n");
		return -EFAULT;
	}

	ret = get_buffered_pixel_rate(ctx,
				      ctx->sensor_sd, ctx->sensor_pad_idx,
				      &pixel_rate);
	if (ret)
		get_pixel_rate(ctx, ctx->sensor_sd, &pixel_rate);

	if (pixel_rate <= 0) {
		dev_info(ctx->dev, "failed to get pixel_rate\n");
		return -EINVAL;
	}

	*p_pixel_rate = pixel_rate;

	return 0;
}

#define SOF_TIMEOUT_RATIO 110
int mtk_cam_seninf_check_timeout(struct v4l2_subdev *sd, u64 time_after_sof)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	u64 frame_time = 400000;//400ms
	int val = 0;
	int ret = 0;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler, V4L2_CID_MTK_SOF_TIMEOUT_VALUE);
	if (!ctrl) {
		dev_info(ctx->dev, "no timeout value in subdev %s\n", sd->name);
		return -EINVAL;
	}

	val = v4l2_ctrl_g_ctrl(ctrl);

	if (val > 0)
		frame_time = val;
	time_after_sof /= 1000;// covert into us

	if ((time_after_sof) > ((frame_time * SOF_TIMEOUT_RATIO) / 100))
		ret = -1;

	dev_info(ctx->dev, "%s time_after_sof %llu frame_time %llu in us ret %d ratio %d val %d\n",
		__func__,
		time_after_sof,
		frame_time,
		ret,
		SOF_TIMEOUT_RATIO,
		val);
	return ret;
}

u64 mtk_cam_seninf_get_frame_time(struct v4l2_subdev *sd, u32 seq_id)
{
	u64 tmp = 33000;
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;
	int val = 0;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler, V4L2_CID_MTK_SOF_TIMEOUT_VALUE);
	if (ctrl) {
		val = v4l2_ctrl_g_ctrl(ctrl);
		if (val > 0)
			tmp = val;
	}
	return tmp * 1000;
}

int mtk_cam_seninf_dump(struct v4l2_subdev *sd, u32 seq_id, bool force_check)
{
	int ret = 0;
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;
	int val = 0;
	int reset_by_user = 0;

	if (!force_check && ctx->dbg_last_dump_req != 0 &&
		ctx->dbg_last_dump_req == seq_id) {
		dev_info(ctx->dev, "%s skip duplicate dump for req %u\n", __func__, seq_id);
		return 0;
	}

	ctx->dbg_last_dump_req = seq_id;
	ctx->dbg_timeout = 0; //in us

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler, V4L2_CID_MTK_SOF_TIMEOUT_VALUE);
	if (ctrl) {
		val = v4l2_ctrl_g_ctrl(ctrl);
		if (val > 0)
			ctx->dbg_timeout = val;
	}

	ret = pm_runtime_get_sync(ctx->dev);
	if (ret < 0) {
		dev_info(ctx->dev, "%s pm_runtime_get_sync ret %d\n", __func__, ret);
		pm_runtime_put_noidle(ctx->dev);
		return ret;
	}

	if (ctx->streaming) {
		ret = g_seninf_ops->_debug(sd_to_ctx(sd));
#if ESD_RESET_SUPPORT
		if (ret != 0) {
			reset_by_user = is_reset_by_user(sd_to_ctx(sd));
			if (!reset_by_user)
				reset_sensor(sd_to_ctx(sd));
		}
#endif
	} else
		dev_info(ctx->dev, "%s should not dump during stream off\n", __func__);

	pm_runtime_put_sync(ctx->dev);

	dev_info(ctx->dev, "%s ret(%d), req(%u), force(%d) reset_by_user(%d)\n",
		 __func__, ret, seq_id, force_check, reset_by_user);

	return (ret && reset_by_user);
}

void mtk_cam_seninf_set_secure(struct v4l2_subdev *sd, int enable, unsigned int SecInfo_addr)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);

	ctx->SecInfo_addr = SecInfo_addr;
	dev_info(ctx->dev, "[%s]: %x, enable: %d\n", __func__, SecInfo_addr, enable);
	ctx->is_secure = enable ? 1 : 0;
}

