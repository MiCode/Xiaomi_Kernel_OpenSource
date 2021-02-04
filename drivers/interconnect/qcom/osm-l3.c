// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/interconnect-provider.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include <dt-bindings/interconnect/qcom,osm-l3.h>

#define LUT_MAX_ENTRIES			40U
#define LUT_SRC				GENMASK(31, 30)
#define LUT_L_VAL			GENMASK(7, 0)
#define LUT_ROW_SIZE			32
#define CLK_HW_DIV			2

/* Register offsets */
#define REG_ENABLE			0x0
#define REG_FREQ_LUT			0x110
#define REG_PERF_STATE			0x920

#define OSM_L3_MAX_LINKS		6

#define to_qcom_provider(_provider) \
	container_of(_provider, struct qcom_osm_l3_icc_provider, provider)

enum {
	OSM_MASTER_L3_APPS = 5000,
	OSM_SLAVE_L3,
	OSM_SLAVE_L3_CLUSTER0,
	OSM_SLAVE_L3_CLUSTER1,
	OSM_SLAVE_L3_CLUSTER2,
	OSM_SLAVE_L3_GPU,
	OSM_SLAVE_L3_MISC,
};

struct qcom_osm_l3_icc_provider {
	void __iomem *base;
	unsigned int max_state;
	unsigned long lut_tables[LUT_MAX_ENTRIES];
	struct icc_provider provider;
};

/**
 * struct qcom_icc_node - QTI specific interconnect nodes
 * @name: the node name used in debugfs
 * @links: an array of nodes where we can go next while traversing
 * @id: a unique node identifier
 * @num_links: the total number of @links
 * @buswidth: width of the interconnect between a node and the bus
 */
struct qcom_icc_node {
	const char *name;
	u16 links[OSM_L3_MAX_LINKS];
	u16 id;
	u16 num_links;
	u16 buswidth;
};

struct qcom_icc_desc {
	struct qcom_icc_node **nodes;
	size_t num_nodes;
};

#define DEFINE_QNODE(_name, _id, _buswidth, ...)			\
		static struct qcom_icc_node _name = {			\
		.name = #_name,						\
		.id = _id,						\
		.buswidth = _buswidth,					\
		.num_links = ARRAY_SIZE(((int[]){ __VA_ARGS__ })),	\
		.links = { __VA_ARGS__ },				\
	}

DEFINE_QNODE(sdm845_osm_apps_l3, OSM_MASTER_L3_APPS, 16, OSM_SLAVE_L3);
DEFINE_QNODE(sdm845_osm_l3, OSM_SLAVE_L3, 16);

static struct qcom_icc_node *sdm845_osm_l3_nodes[] = {
	[MASTER_OSM_L3_APPS] = &sdm845_osm_apps_l3,
	[SLAVE_OSM_L3] = &sdm845_osm_l3,
};

const static struct qcom_icc_desc sdm845_icc_osm_l3 = {
	.nodes = sdm845_osm_l3_nodes,
	.num_nodes = ARRAY_SIZE(sdm845_osm_l3_nodes),
};

DEFINE_QNODE(mas_osm_l3_apps, OSM_MASTER_L3_APPS, 1,
		OSM_SLAVE_L3_CLUSTER0, OSM_SLAVE_L3_CLUSTER1,
		OSM_SLAVE_L3_CLUSTER2, OSM_SLAVE_L3_MISC, OSM_SLAVE_L3_GPU);
DEFINE_QNODE(slv_osm_l3_cluster0, OSM_SLAVE_L3_CLUSTER0, 1);
DEFINE_QNODE(slv_osm_l3_cluster1, OSM_SLAVE_L3_CLUSTER1, 1);
DEFINE_QNODE(slv_osm_l3_cluster2, OSM_SLAVE_L3_CLUSTER2, 1);
DEFINE_QNODE(slv_osm_l3_misc, OSM_SLAVE_L3_MISC, 1);
DEFINE_QNODE(slv_osm_l3_gpu, OSM_SLAVE_L3_GPU, 1);

static struct qcom_icc_node *sm8150_osm_l3_nodes[] = {
	[MASTER_OSM_L3_APPS] = &mas_osm_l3_apps,
	[SLAVE_OSM_L3_CLUSTER0] = &slv_osm_l3_cluster0,
	[SLAVE_OSM_L3_CLUSTER1] = &slv_osm_l3_cluster1,
	[SLAVE_OSM_L3_CLUSTER2] = &slv_osm_l3_cluster2,
	[SLAVE_OSM_L3_MISC] = &slv_osm_l3_misc,
	[SLAVE_OSM_L3_GPU] = &slv_osm_l3_gpu,
};

static const struct qcom_icc_desc sm8150_icc_osm_l3 = {
	.nodes = sm8150_osm_l3_nodes,
	.num_nodes = ARRAY_SIZE(sm8150_osm_l3_nodes),
};

DEFINE_QNODE(mas_osm_l3_apps_scshrike, OSM_MASTER_L3_APPS, 1,
		OSM_SLAVE_L3_CLUSTER0, OSM_SLAVE_L3_CLUSTER1,
		OSM_SLAVE_L3_MISC, OSM_SLAVE_L3_GPU);

static struct qcom_icc_node *scshrike_osm_l3_nodes[] = {
	[MASTER_OSM_L3_APPS] = &mas_osm_l3_apps_scshrike,
	[SLAVE_OSM_L3_CLUSTER0] = &slv_osm_l3_cluster0,
	[SLAVE_OSM_L3_CLUSTER1] = &slv_osm_l3_cluster1,
	[SLAVE_OSM_L3_MISC] = &slv_osm_l3_misc,
	[SLAVE_OSM_L3_GPU] = &slv_osm_l3_gpu,
};

static const struct qcom_icc_desc scshrike_icc_osm_l3 = {
	.nodes = scshrike_osm_l3_nodes,
	.num_nodes = ARRAY_SIZE(scshrike_osm_l3_nodes),
};

DEFINE_QNODE(mas_osm_l3_apps_sm6150, OSM_MASTER_L3_APPS, 1,
		OSM_SLAVE_L3_CLUSTER0, OSM_SLAVE_L3_CLUSTER1,
		OSM_SLAVE_L3_MISC, OSM_SLAVE_L3_GPU);

static struct qcom_icc_node *sm6150_osm_l3_nodes[] = {
	[MASTER_OSM_L3_APPS] = &mas_osm_l3_apps_sm6150,
	[SLAVE_OSM_L3_CLUSTER0] = &slv_osm_l3_cluster0,
	[SLAVE_OSM_L3_CLUSTER1] = &slv_osm_l3_cluster1,
	[SLAVE_OSM_L3_MISC] = &slv_osm_l3_misc,
	[SLAVE_OSM_L3_GPU] = &slv_osm_l3_gpu,
};

static const struct qcom_icc_desc sm6150_icc_osm_l3 = {
	.nodes = sm6150_osm_l3_nodes,
	.num_nodes = ARRAY_SIZE(sm6150_osm_l3_nodes),
};

static int qcom_icc_aggregate(struct icc_node *node, u32 tag, u32 avg_bw,
		u32 peak_bw, u32 *agg_avg, u32 *agg_peak)
{
	*agg_avg += avg_bw;
	*agg_peak = max(*agg_peak, peak_bw);

	return 0;
}

static int qcom_icc_set(struct icc_node *src, struct icc_node *dst)
{
	struct qcom_osm_l3_icc_provider *qp;
	struct icc_provider *provider;
	struct qcom_icc_node *qn;
	unsigned int index;
	u64 rate;

	qn = src->data;
	provider = src->provider;
	qp = to_qcom_provider(provider);

	rate = dst->peak_bw;
	rate = icc_units_to_bps(rate);
	do_div(rate, qn->buswidth);

	for (index = 0; index < qp->max_state - 1; index++) {
		if (qp->lut_tables[index] >= rate)
			break;
	}

	writel_relaxed(index, qp->base + REG_PERF_STATE);

	return 0;
}

static int qcom_osm_l3_remove(struct platform_device *pdev)
{
	struct qcom_osm_l3_icc_provider *qp = platform_get_drvdata(pdev);
	struct icc_provider *provider = &qp->provider;
	struct icc_node *n;

	list_for_each_entry(n, &provider->nodes, node_list) {
		icc_node_del(n);
		icc_node_destroy(n->id);
	}

	return icc_provider_del(&qp->provider);
}

static int qcom_osm_l3_probe(struct platform_device *pdev)
{
	u32 info, src, lval, i, prev_freq = 0, freq;
	static unsigned long hw_rate, xo_rate;
	struct qcom_osm_l3_icc_provider *qp;
	const struct qcom_icc_desc *desc;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct qcom_icc_node **qnodes;
	struct icc_node *node;
	size_t num_nodes;
	struct clk *clk;
	int ret;

	clk = clk_get(&pdev->dev, "xo");
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	xo_rate = clk_get_rate(clk);
	clk_put(clk);

	clk = clk_get(&pdev->dev, "alternate");
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	hw_rate = clk_get_rate(clk) / CLK_HW_DIV;
	clk_put(clk);

	qp = devm_kzalloc(&pdev->dev, sizeof(*qp), GFP_KERNEL);
	if (!qp)
		return -ENOMEM;

	qp->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(qp->base))
		return PTR_ERR(qp->base);

	/* HW should be in enabled state to proceed */
	if (!(readl_relaxed(qp->base + REG_ENABLE) & 0x1)) {
		dev_err(&pdev->dev, "error hardware not enabled\n");
		return -ENODEV;
	}

	for (i = 0; i < LUT_MAX_ENTRIES; i++) {
		info = readl_relaxed(qp->base + REG_FREQ_LUT +
				     i * LUT_ROW_SIZE);
		src = FIELD_GET(LUT_SRC, info);
		lval = FIELD_GET(LUT_L_VAL, info);
		if (src)
			freq = xo_rate * lval;
		else
			freq = hw_rate;

		/* Two of the same frequencies signify end of table */
		if (i > 0 && prev_freq == freq)
			break;

		dev_dbg(&pdev->dev, "index=%d freq=%d\n", i, freq);

		qp->lut_tables[i] = freq;
		prev_freq = freq;
	}
	qp->max_state = i;

	desc = device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	qnodes = desc->nodes;
	num_nodes = desc->num_nodes;

	data = devm_kcalloc(&pdev->dev, num_nodes, sizeof(*node), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	provider = &qp->provider;
	provider->dev = &pdev->dev;
	provider->set = qcom_icc_set;
	provider->aggregate = qcom_icc_aggregate;
	provider->xlate = of_icc_xlate_onecell;
	INIT_LIST_HEAD(&provider->nodes);
	provider->data = data;

	ret = icc_provider_add(provider);
	if (ret) {
		dev_err(&pdev->dev, "error adding interconnect provider\n");
		return ret;
	}

	for (i = 0; i < num_nodes; i++) {
		size_t j;

		if (!qnodes[i])
			continue;

		node = icc_node_create(qnodes[i]->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}

		node->name = qnodes[i]->name;
		node->data = qnodes[i];
		icc_node_add(node, provider);

		for (j = 0; j < qnodes[i]->num_links; j++)
			icc_link_create(node, qnodes[i]->links[j]);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;

	platform_set_drvdata(pdev, qp);

	return 0;
err:
	qcom_osm_l3_remove(pdev);

	return ret;
}

static const struct of_device_id osm_l3_of_match[] = {
	{ .compatible = "qcom,sdm845-osm-l3", .data = &sdm845_icc_osm_l3 },
	{ .compatible = "qcom,sm8150-osm-l3", .data = &sm8150_icc_osm_l3 },
	{ .compatible = "qcom,sm6150-osm-l3", .data = &sm6150_icc_osm_l3 },
	{ .compatible = "qcom,scshrike-osm-l3", .data = &scshrike_icc_osm_l3 },
	{ }
};
MODULE_DEVICE_TABLE(of, osm_l3_of_match);

static struct platform_driver osm_l3_driver = {
	.probe = qcom_osm_l3_probe,
	.remove = qcom_osm_l3_remove,
	.driver = {
		.name = "osm-l3",
		.of_match_table = osm_l3_of_match,
	},
};
module_platform_driver(osm_l3_driver);

MODULE_DESCRIPTION("QTI OSM L3 interconnect driver");
MODULE_LICENSE("GPL v2");
