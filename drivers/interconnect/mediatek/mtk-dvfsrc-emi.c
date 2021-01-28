// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <dt-bindings/interconnect/mtk,mt6779-emi.h>
#include <dt-bindings/interconnect/mtk,mt6761-emi.h>
#include <linux/device.h>
#include <linux/interconnect-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>

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
	MASTER_DEBUGSYS,
};

#define MAX_LINKS	6

/**
 * struct mtk_icc_node - Mediatek specific interconnect nodes
 * @name: the node name used in debugfs
 * @links: an array of nodes where we can go next while traversing
 * @id: a unique node identifier
 * @num_links: the total number of @links
 * @buswidth: width of the interconnect between a node and the bus
 * @sum_avg: current sum aggregate value of all avg bw [kBps] requests
 * @max_peak: current max aggregate value of all peak bw [kBps] requests
 */
struct mtk_icc_node {
	unsigned char *name;
	bool ep;
	u16 id;
	u16 links[MAX_LINKS];
	u16 num_links;
	u16 buswidth;
	u64 sum_avg;
	u64 sum_peak;
};

struct mtk_icc_desc {
	struct mtk_icc_node **nodes;
	size_t num_nodes;
};

#define DEFINE_MNODE(_name, _id, _buswidth, _ep, _numlinks, ...)	\
		static struct mtk_icc_node _name = {			\
		.name = #_name,						\
		.id = _id,						\
		.buswidth = _buswidth,					\
		.ep = _ep,						\
		.num_links = ARRAY_SIZE(((int[]){ __VA_ARGS__ })),	\
		.links = { __VA_ARGS__ },				\
}

DEFINE_MNODE(ddr_emi, SLAVE_DDR_EMI, 1024, 1, 0, 0);
DEFINE_MNODE(mcusys, MASTER_MCUSYS, 256, 0, 1, SLAVE_DDR_EMI);
DEFINE_MNODE(gpu, MASTER_GPUSYS, 256, 0, 1, SLAVE_DDR_EMI);
DEFINE_MNODE(mmsys, MASTER_MMSYS, 256, 0, 1, SLAVE_DDR_EMI);
DEFINE_MNODE(mm_vpu, MASTER_MM_VPU, 128, 0, 1, MASTER_MMSYS);
DEFINE_MNODE(mm_disp, MASTER_MM_DISP, 128, 0, 1, MASTER_MMSYS);
DEFINE_MNODE(mm_vdec, MASTER_MM_VDEC, 128, 0, 1, MASTER_MMSYS);
DEFINE_MNODE(mm_venc, MASTER_MM_VENC, 128, 0, 1, MASTER_MMSYS);
DEFINE_MNODE(mm_cam, MASTER_MM_CAM, 128, 0, 1, MASTER_MMSYS);
DEFINE_MNODE(mm_img, MASTER_MM_IMG, 128, 0, 1, MASTER_MMSYS);
DEFINE_MNODE(mm_mdp, MASTER_MM_MDP, 128, 0, 1, MASTER_MMSYS);
DEFINE_MNODE(vpusys, MASTER_VPUSYS, 256, 0, 1, SLAVE_DDR_EMI);
DEFINE_MNODE(vpu_port_0, MASTER_VPU_PORT_0, 128, 0, 1, MASTER_VPUSYS);
DEFINE_MNODE(vpu_port_1, MASTER_VPU_PORT_1, 128, 0, 1, MASTER_VPUSYS);
DEFINE_MNODE(mdlasys, MASTER_MDLASYS, 256, 0, 1, SLAVE_DDR_EMI);
DEFINE_MNODE(mdla_port_0, MASTER_MDLA_PORT_0, 256, 0, 1, MASTER_MDLASYS);
DEFINE_MNODE(debugsys, MASTER_DEBUGSYS, 256, 0, 1, SLAVE_DDR_EMI);

static struct mtk_icc_node *mt6779_icc_nodes[] = {
	[MT6779_SLAVE_DDR_EMI] = &ddr_emi,
	[MT6779_MASTER_MCUSYS] = &mcusys,
	[MT6779_MASTER_GPUSYS]	 = &gpu,
	[MT6779_MASTER_MMSYS]	 = &mmsys,
	[MT6779_MASTER_MM_VPU] = &mm_vpu,
	[MT6779_MASTER_MM_DISP] = &mm_disp,
	[MT6779_MASTER_MM_VDEC] = &mm_vdec,
	[MT6779_MASTER_MM_VENC] = &mm_venc,
	[MT6779_MASTER_MM_CAM] = &mm_cam,
	[MT6779_MASTER_MM_IMG] = &mm_img,
	[MT6779_MASTER_MM_MDP] = &mm_mdp,
	[MT6779_MASTER_VPUSYS] = &vpusys,
	[MT6779_MASTER_VPU_0]	= &vpu_port_0,
	[MT6779_MASTER_VPU_1]	= &vpu_port_1,
	[MT6779_MASTER_MDLASYS] = &mdlasys,
	[MT6779_MASTER_MDLA_0] = &mdla_port_0,
	[MT6779_MASTER_DEBUGSYS] = &debugsys,
};

static struct mtk_icc_desc mt6779_icc = {
	.nodes = mt6779_icc_nodes,
	.num_nodes = ARRAY_SIZE(mt6779_icc_nodes),
};

static struct mtk_icc_node *mt6761_icc_nodes[] = {
	[MT6761_SLAVE_DDR_EMI] = &ddr_emi,
	[MT6761_MASTER_MCUSYS] = &mcusys,
	[MT6761_MASTER_GPUSYS]	 = &gpu,
	[MT6761_MASTER_MMSYS]	 = &mmsys,
	[MT6761_MASTER_MM_VPU] = &mm_vpu,
	[MT6761_MASTER_MM_DISP] = &mm_disp,
	[MT6761_MASTER_MM_VDEC] = &mm_vdec,
	[MT6761_MASTER_MM_VENC] = &mm_venc,
	[MT6761_MASTER_MM_CAM] = &mm_cam,
	[MT6761_MASTER_MM_IMG] = &mm_img,
	[MT6761_MASTER_MM_MDP] = &mm_mdp,
	[MT6761_MASTER_DEBUGSYS] = &debugsys,
};

static struct mtk_icc_desc mt6761_icc = {
	.nodes = mt6761_icc_nodes,
	.num_nodes = ARRAY_SIZE(mt6761_icc_nodes),
};

static int emi_icc_aggregate(struct icc_node *node, u32 avg_bw,
			      u32 peak_bw, u32 *agg_avg, u32 *agg_peak)
{
	struct mtk_icc_node *in;

	in = node->data;

	*agg_avg  += avg_bw;
	*agg_peak += peak_bw;

	in->sum_avg = *agg_avg;
	in->sum_peak = *agg_peak;

	return 0;
}

static int emi_icc_set(struct icc_node *src, struct icc_node *dst)
{
	int ret = 0;
	struct mtk_icc_node *node;

	node = dst->data;
	if (node->ep) {
		pr_debug("sum_avg (%llu), max_peak (%llu)\n",
			node->sum_avg, node->sum_peak);
		mtk_dvfsrc_send_request(src->provider->dev->parent,
					MTK_DVFSRC_CMD_BW_REQUEST,
					node->sum_avg);
		mtk_dvfsrc_send_request(src->provider->dev->parent,
					MTK_DVFSRC_CMD_HRTBW_REQUEST,
					node->sum_peak);
	}

	return ret;
}

static int emi_icc_probe(struct platform_device *pdev)
{
	int ret;
	const struct mtk_icc_desc *desc;
	struct icc_node *node;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct mtk_icc_node **mnodes;
	size_t num_nodes, i, j;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	mnodes = desc->nodes;
	num_nodes = desc->num_nodes;

	provider = devm_kzalloc(&pdev->dev, sizeof(*provider), GFP_KERNEL);
	if (!provider)
		return -ENOMEM;

	data = devm_kcalloc(&pdev->dev, num_nodes, sizeof(*node), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	provider->dev = &pdev->dev;
	provider->set = emi_icc_set;
	provider->aggregate = emi_icc_aggregate;
	provider->xlate = of_icc_xlate_onecell;
	INIT_LIST_HEAD(&provider->nodes);
	provider->data = data;

	ret = icc_provider_add(provider);
	if (ret) {
		dev_err(&pdev->dev, "error adding interconnect provider\n");
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

		dev_dbg(&pdev->dev, "registered node %s, num link: %d\n",
			mnodes[i]->name, mnodes[i]->num_links);

		/* populate links */
		for (j = 0; j < mnodes[i]->num_links; j++)
			icc_link_create(node, mnodes[i]->links[j]);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;

	platform_set_drvdata(pdev, provider);

	return 0;
err:
	list_for_each_entry(node, &provider->nodes, node_list) {
		icc_node_del(node);
		icc_node_destroy(node->id);
	}

	icc_provider_del(provider);
	return ret;
}

static int emi_icc_remove(struct platform_device *pdev)
{
	struct icc_provider *provider = platform_get_drvdata(pdev);
	struct icc_node *n;

	list_for_each_entry(n, &provider->nodes, node_list) {
		icc_node_del(n);
		icc_node_destroy(n->id);
	}

	return icc_provider_del(provider);
}

static const struct of_device_id emi_icc_of_match[] = {
	{ .compatible = "mediatek,dvfsrc-mt6779-emi", .data = &mt6779_icc },
	{ .compatible = "mediatek,dvfsrc-mt6761-emi", .data = &mt6761_icc },
	{ },
};
MODULE_DEVICE_TABLE(of, emi_icc_of_match);

static struct platform_driver emi_icc_driver = {
	.probe = emi_icc_probe,
	.remove = emi_icc_remove,
	.driver = {
		.name = "mediatek-dvfsrc-emi",
		.of_match_table = emi_icc_of_match,
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
