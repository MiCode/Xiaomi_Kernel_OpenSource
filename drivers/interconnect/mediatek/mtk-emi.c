// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/interconnect-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <dt-bindings/interconnect/mtk,mt8183-emi.h>
#include <dt-bindings/interconnect/mtk,mt6873-emi.h>

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#define CREATE_TRACE_POINTS
#include "../internal.h"
#include "mtk-dvfsrc-icc-trace.h"
#endif

enum mtk_icc_name {
	SLAVE_DDR_EMI,
	MASTER_MCUSYS,
	MASTER_GPUSYS,
	MASTER_MMSYS,
	MASTER_MM_VPU,
	MASTER_MM_DISP,
	MASTER_MM_VDEC,
	MASTER_MM_VENC,
	MASTER_MM_CAM,
	MASTER_MM_IMG,
	MASTER_MM_MDP,
	MASTER_VPUSYS,
	MASTER_VPU_PORT_0,
	MASTER_VPU_PORT_1,
	MASTER_MDLASYS,
	MASTER_MDLA_PORT_0,
	MASTER_UFS,
	MASTER_PCIE,
	MASTER_USB,
	MASTER_WIFI,
	MASTER_BT,
	MASTER_NETSYS,
	MASTER_DBGIF,

	SLAVE_HRT_DDR_EMI,
	MASTER_HRT_MMSYS,
	MASTER_HRT_MM_DISP,
	MASTER_HRT_MM_VDEC,
	MASTER_HRT_MM_VENC,
	MASTER_HRT_MM_CAM,
	MASTER_HRT_MM_IMG,
	MASTER_HRT_MM_MDP,
	MASTER_HRT_DBGIF,
};

#define MT8183_MAX_LINKS	1

/**
 * struct mtk_icc_node - Mediatek specific interconnect nodes
 * @name: the node name used in debugfs
 * @ep : the type of this endpoint
 * @id: a unique node identifier
 * @links: an array of nodes where we can go next while traversing
 * @num_links: the total number of @links
 * @buswidth: width of the interconnect between a node and the bus
 * @sum_avg: current sum aggregate value of all avg bw kBps requests
 * @max_peak: current max aggregate value of all peak bw kBps requests
 */
struct mtk_icc_node {
	unsigned char *name;
	int ep;
	u16 id;
	u16 links[MT8183_MAX_LINKS];
	u16 num_links;
	u64 sum_avg;
	u64 max_peak;
};

struct mtk_icc_desc {
	struct mtk_icc_node **nodes;
	size_t num_nodes;
};

#define DEFINE_MNODE(_name, _id, _ep, ...)	\
		static struct mtk_icc_node _name = {			\
		.name = #_name,						\
		.id = _id,						\
		.ep = _ep,						\
		.num_links = ARRAY_SIZE(((int[]){ __VA_ARGS__ })),	\
		.links = { __VA_ARGS__ },				\
}

DEFINE_MNODE(ddr_emi, SLAVE_DDR_EMI, 1);
DEFINE_MNODE(mcusys, MASTER_MCUSYS, 0, SLAVE_DDR_EMI);
DEFINE_MNODE(gpu, MASTER_GPUSYS, 0, SLAVE_DDR_EMI);
DEFINE_MNODE(mmsys, MASTER_MMSYS, 0, SLAVE_DDR_EMI);
DEFINE_MNODE(mm_vpu, MASTER_MM_VPU, 0, MASTER_MMSYS);
DEFINE_MNODE(mm_disp, MASTER_MM_DISP, 0, MASTER_MMSYS);
DEFINE_MNODE(mm_vdec, MASTER_MM_VDEC, 0, MASTER_MMSYS);
DEFINE_MNODE(mm_venc, MASTER_MM_VENC, 0, MASTER_MMSYS);
DEFINE_MNODE(mm_cam, MASTER_MM_CAM, 0, MASTER_MMSYS);
DEFINE_MNODE(mm_img, MASTER_MM_IMG, 0, MASTER_MMSYS);
DEFINE_MNODE(mm_mdp, MASTER_MM_MDP, 0, MASTER_MMSYS);
DEFINE_MNODE(vpusys, MASTER_VPUSYS, 0, SLAVE_DDR_EMI);
DEFINE_MNODE(vpu_port_0, MASTER_VPU_PORT_0, 0, MASTER_VPUSYS);
DEFINE_MNODE(vpu_port_1, MASTER_VPU_PORT_1, 0, MASTER_VPUSYS);
DEFINE_MNODE(mdlasys, MASTER_MDLASYS, 0, SLAVE_DDR_EMI);
DEFINE_MNODE(mdla_port_0, MASTER_MDLA_PORT_0, 0, MASTER_MDLASYS);
DEFINE_MNODE(ufs, MASTER_UFS, 0, SLAVE_DDR_EMI);
DEFINE_MNODE(pcie, MASTER_PCIE, 0, SLAVE_DDR_EMI);
DEFINE_MNODE(usb, MASTER_USB, 0, SLAVE_DDR_EMI);
DEFINE_MNODE(wifi, MASTER_WIFI, 0, SLAVE_DDR_EMI);
DEFINE_MNODE(bt, MASTER_BT, 0, SLAVE_DDR_EMI);
DEFINE_MNODE(netsys, MASTER_NETSYS, 0, SLAVE_DDR_EMI);
DEFINE_MNODE(dbgif, MASTER_DBGIF, 0, SLAVE_DDR_EMI);

DEFINE_MNODE(hrt_ddr_emi, SLAVE_HRT_DDR_EMI, 2);
DEFINE_MNODE(hrt_mmsys, MASTER_HRT_MMSYS, 0, SLAVE_HRT_DDR_EMI);
DEFINE_MNODE(hrt_mm_disp, MASTER_HRT_MM_DISP, 0, MASTER_HRT_MMSYS);
DEFINE_MNODE(hrt_mm_vdec, MASTER_HRT_MM_VDEC, 0, MASTER_HRT_MMSYS);
DEFINE_MNODE(hrt_mm_venc, MASTER_HRT_MM_VENC, 0, MASTER_HRT_MMSYS);
DEFINE_MNODE(hrt_mm_cam, MASTER_HRT_MM_CAM, 0, MASTER_HRT_MMSYS);
DEFINE_MNODE(hrt_mm_img, MASTER_HRT_MM_IMG, 0, MASTER_HRT_MMSYS);
DEFINE_MNODE(hrt_mm_mdp, MASTER_HRT_MM_MDP, 0, MASTER_HRT_MMSYS);
DEFINE_MNODE(hrt_dbgif, MASTER_HRT_DBGIF, 0, SLAVE_HRT_DDR_EMI);

static struct mtk_icc_node *mt8183_icc_nodes[] = {
	[MT8183_SLAVE_DDR_EMI] = &ddr_emi,
	[MT8183_MASTER_MCUSYS] = &mcusys,
	[MT8183_MASTER_GPU] = &gpu,
	[MT8183_MASTER_MMSYS] = &mmsys,
	[MT8183_MASTER_MM_VPU] = &mm_vpu,
	[MT8183_MASTER_MM_DISP] = &mm_disp,
	[MT8183_MASTER_MM_VDEC] = &mm_vdec,
	[MT8183_MASTER_MM_VENC] = &mm_venc,
	[MT8183_MASTER_MM_CAM] = &mm_cam,
	[MT8183_MASTER_MM_IMG] = &mm_img,
	[MT8183_MASTER_MM_MDP] = &mm_mdp,
};

static struct mtk_icc_desc mt8183_icc = {
	.nodes = mt8183_icc_nodes,
	.num_nodes = ARRAY_SIZE(mt8183_icc_nodes),
};

static struct mtk_icc_node *mt6873_icc_nodes[] = {
	[MT6873_SLAVE_DDR_EMI] = &ddr_emi,
	[MT6873_MASTER_MCUSYS] = &mcusys,
	[MT6873_MASTER_GPUSYS] = &gpu,
	[MT6873_MASTER_MMSYS] = &mmsys,
	[MT6873_MASTER_MM_VPU] = &mm_vpu,
	[MT6873_MASTER_MM_DISP] = &mm_disp,
	[MT6873_MASTER_MM_VDEC] = &mm_vdec,
	[MT6873_MASTER_MM_VENC] = &mm_venc,
	[MT6873_MASTER_MM_CAM] = &mm_cam,
	[MT6873_MASTER_MM_IMG] = &mm_img,
	[MT6873_MASTER_MM_MDP] = &mm_mdp,
	[MT6873_MASTER_VPUSYS] = &vpusys,
	[MT6873_MASTER_VPU_0] = &vpu_port_0,
	[MT6873_MASTER_VPU_1] = &vpu_port_1,
	[MT6873_MASTER_MDLASYS] = &mdlasys,
	[MT6873_MASTER_MDLA_0] = &mdla_port_0,
	[MT6873_MASTER_UFS] = &ufs,
	[MT6873_MASTER_PCIE] = &pcie,
	[MT6873_MASTER_USB] = &usb,
	[MT6873_MASTER_WIFI] = &wifi,
	[MT6873_MASTER_BT] = &bt,
	[MT6873_MASTER_NETSYS] = &netsys,
	[MT6873_MASTER_DBGIF] = &dbgif,

	[MT6873_SLAVE_HRT_DDR_EMI] = &hrt_ddr_emi,
	[MT6873_MASTER_HRT_MMSYS] = &hrt_mmsys,
	[MT6873_MASTER_HRT_MM_DISP] = &hrt_mm_disp,
	[MT6873_MASTER_HRT_MM_VDEC] = &hrt_mm_vdec,
	[MT6873_MASTER_HRT_MM_VENC] = &hrt_mm_venc,
	[MT6873_MASTER_HRT_MM_CAM] = &hrt_mm_cam,
	[MT6873_MASTER_HRT_MM_IMG] = &hrt_mm_img,
	[MT6873_MASTER_HRT_MM_MDP] = &hrt_mm_mdp,
	[MT6873_MASTER_HRT_DBGIF] = &hrt_dbgif,
};

static struct mtk_icc_desc mt6873_icc = {
	.nodes = mt6873_icc_nodes,
	.num_nodes = ARRAY_SIZE(mt6873_icc_nodes),
};

static const struct of_device_id emi_icc_of_match[] = {
	{ .compatible = "mediatek,mt8183-dvfsrc", .data = &mt8183_icc },
	{ .compatible = "mediatek,mt6873-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6853-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6789-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6885-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6893-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6833-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6877-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6983-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6895-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6879-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6855-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6768-dvfsrc", .data = &mt6873_icc },
	{ .compatible = "mediatek,mt6765-dvfsrc", .data = &mt6873_icc },
	{ },
};
MODULE_DEVICE_TABLE(of, emi_icc_of_match);

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
static void emi_icc_trace_bw_consumers(struct icc_node *n, bool is_hrt)
{
	struct icc_req *r;

	hlist_for_each_entry(r, &n->req_list, req_node) {
		if (!r->dev)
			continue;
		if (is_hrt)
			trace_mtk_pm_qos_update_request(150, r->avg_bw / 1000, dev_name(r->dev));
		else {
			trace_mtk_pm_qos_update_request(130, r->avg_bw / 1000, dev_name(r->dev));
			trace_mtk_pm_qos_update_request(140, r->peak_bw / 1000, dev_name(r->dev));
		}
	}
}
#endif

static int emi_icc_aggregate(struct icc_node *node, u32 tag, u32 avg_bw,
			     u32 peak_bw, u32 *agg_avg, u32 *agg_peak)
{
	struct mtk_icc_node *in;

	in = node->data;

	*agg_avg += avg_bw;
	*agg_peak = max_t(u32, *agg_peak, peak_bw);

	in->sum_avg = *agg_avg;
	in->max_peak = *agg_peak;

	return 0;
}

static int emi_icc_set(struct icc_node *src, struct icc_node *dst)
{
	int ret = 0;
	struct mtk_icc_node *node;

	node = dst->data;

	if (node->ep == 1) {
		mtk_dvfsrc_send_request(src->provider->dev,
					MTK_DVFSRC_CMD_PEAK_BW_REQUEST,
					node->max_peak);
		mtk_dvfsrc_send_request(src->provider->dev,
					MTK_DVFSRC_CMD_BW_REQUEST,
					node->sum_avg);
#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
		trace_mtk_pm_qos_update_request(30, src->avg_bw / 1000, src->name);
		trace_mtk_pm_qos_update_request(40, src->peak_bw / 1000, src->name);
		if (strcmp(src->name, "dbgif") == 0)
			emi_icc_trace_bw_consumers(dst, false);
#endif
	} else if (node->ep == 2) {
		mtk_dvfsrc_send_request(src->provider->dev,
					MTK_DVFSRC_CMD_HRTBW_REQUEST,
					node->sum_avg);
#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
		trace_mtk_pm_qos_update_request(50, src->avg_bw / 1000, src->name);
		if (strcmp(src->name, "hrt_dbgif") == 0)
			emi_icc_trace_bw_consumers(dst, true);
#endif
	}

	return ret;
}

static int emi_icc_get_bw(struct icc_node *node, u32 *avg, u32 *peak)
{
	*avg = 0;
	*peak = 0;
	return 0;
}

static int emi_icc_remove(struct platform_device *pdev);
static int emi_icc_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct mtk_icc_desc *desc;
	struct device *dev = &pdev->dev;
	struct icc_node *node;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct mtk_icc_node **mnodes;
	struct icc_node *tmp;
	size_t num_nodes, i, j;
	int ret;

	match = of_match_node(emi_icc_of_match, dev->parent->of_node);

	if (!match) {
		dev_err(dev, "invalid compatible string\n");
		return -ENODEV;
	}

	desc = match->data;
	mnodes = desc->nodes;
	num_nodes = desc->num_nodes;

	provider = devm_kzalloc(dev, sizeof(*provider), GFP_KERNEL);
	if (!provider)
		return -ENOMEM;

	data = devm_kzalloc(dev, struct_size(data, nodes, num_nodes),
			GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	provider->dev = pdev->dev.parent;
	provider->set = emi_icc_set;
	provider->aggregate = emi_icc_aggregate;
	provider->xlate = of_icc_xlate_onecell;
	INIT_LIST_HEAD(&provider->nodes);
	provider->data = data;
	provider->get_bw = emi_icc_get_bw;

	ret = icc_provider_add(provider);
	if (ret) {
		dev_err(dev, "error adding interconnect provider\n");
		return ret;
	}

	for (i = 0; i < num_nodes; i++) {
		node = icc_node_create(mnodes[i]->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}

		node->name = mnodes[i]->name;
		node->data = mnodes[i];
		icc_node_add(node, provider);

		/* populate links */
		for (j = 0; j < mnodes[i]->num_links; j++)
			icc_link_create(node, mnodes[i]->links[j]);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;

	platform_set_drvdata(pdev, provider);

	return 0;
err:
	list_for_each_entry_safe(node, tmp, &provider->nodes, node_list) {
		icc_node_del(node);
		icc_node_destroy(node->id);
	}

	icc_provider_del(provider);
	return ret;
}

static int emi_icc_remove(struct platform_device *pdev)
{
	struct icc_provider *provider = platform_get_drvdata(pdev);
	struct icc_node *n, *tmp;

	list_for_each_entry_safe(n, tmp, &provider->nodes, node_list) {
		icc_node_del(n);
		icc_node_destroy(n->id);
	}

	return icc_provider_del(provider);
}

static struct platform_driver emi_icc_driver = {
	.probe = emi_icc_probe,
	.remove = emi_icc_remove,
	.driver = {
		.name = "mediatek-emi-icc",
		.sync_state = icc_sync_state,
	},
};

static int __init mtk_emi_icc_init(void)
{
	return platform_driver_register(&emi_icc_driver);
}
subsys_initcall(mtk_emi_icc_init);

static void __exit mtk_emi_icc_exit(void)
{
	platform_driver_unregister(&emi_icc_driver);
}
module_exit(mtk_emi_icc_exit);

MODULE_AUTHOR("Henry Chen <henryc.chen@mediatek.com>");
MODULE_LICENSE("GPL v2");
