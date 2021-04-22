// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 *
 */

#include <dt-bindings/interconnect/qcom,epss-l3.h>
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/interconnect-provider.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#define LUT_MAX_ENTRIES			40U
#define LUT_SRC				GENMASK(30, 30)
#define LUT_L_VAL			GENMASK(7, 0)
#define LUT_ROW_SIZE			4
#define CLK_HW_DIV			2

/* Register offsets */
#define REG_L3_VOTE			0x90
#define REG_FREQ_LUT			0x100
#define REG_PERF_STATE			0x320
#define EPSS_DOMAIN_OFFSET		0x1000
#define EPSS_CORE_OFFSET		0x4
#define EPSS_L3_VOTE_REG(base, domain, cpu)\
			((base + REG_L3_VOTE) +\
			(domain * EPSS_DOMAIN_OFFSET) +\
			(cpu * EPSS_CORE_OFFSET))

#define EPSS_L3_MAX_LINKS		9

#define to_qcom_provider(_provider) \
	container_of(_provider, struct qcom_epss_l3_icc_provider, provider)

enum {
	LAHAINA_MASTER_EPSS_L3_APPS = 5000,
	LAHAINA_SLAVE_EPSS_L3_CPU0,
	LAHAINA_SLAVE_EPSS_L3_CPU1,
	LAHAINA_SLAVE_EPSS_L3_CPU2,
	LAHAINA_SLAVE_EPSS_L3_CPU3,
	LAHAINA_SLAVE_EPSS_L3_CPU4,
	LAHAINA_SLAVE_EPSS_L3_CPU5,
	LAHAINA_SLAVE_EPSS_L3_CPU6,
	LAHAINA_SLAVE_EPSS_L3_CPU7,
	LAHAINA_SLAVE_EPSS_L3_SHARED,
};

struct qcom_epss_l3_icc_provider {
	void __iomem *base;
	unsigned int max_state;
	unsigned long lut_freqs[LUT_MAX_ENTRIES];
	struct icc_provider provider;
};

/**
 * struct qcom_icc_node - QTI specific interconnect nodes
 * @name: the node name used in debugfs
 * @links: an array of nodes where we can go next while traversing
 * @id: a unique node identifier
 * @num_links: the total number of @links
 * @buswidth: width of the interconnect between a node and the bus
 * @domain: clock domain of the cpu node
 * @cpu: cpu instance within its clock domain
 */
struct qcom_icc_node {
	const char *name;
	u16 links[EPSS_L3_MAX_LINKS];
	u16 id;
	u16 num_links;
	u16 buswidth;
	u16 domain;
	u16 cpu;
};

struct qcom_icc_desc {
	struct qcom_icc_node **nodes;
	size_t num_nodes;
};

#define DEFINE_QNODE(_name, _id, _buswidth, _domain, _cpu, ...)		\
		static struct qcom_icc_node _name = {			\
		.name = #_name,						\
		.id = _id,						\
		.buswidth = _buswidth,					\
		.domain = _domain,					\
		.cpu = _cpu,						\
		.num_links = ARRAY_SIZE(((int[]){ __VA_ARGS__ })),	\
		.links = { __VA_ARGS__ },				\
	}

DEFINE_QNODE(mas_epss_l3_apps, LAHAINA_MASTER_EPSS_L3_APPS, 1, 0, 0,
		LAHAINA_SLAVE_EPSS_L3_CPU0, LAHAINA_SLAVE_EPSS_L3_CPU1,
		LAHAINA_SLAVE_EPSS_L3_CPU2, LAHAINA_SLAVE_EPSS_L3_CPU3,
		LAHAINA_SLAVE_EPSS_L3_CPU4, LAHAINA_SLAVE_EPSS_L3_CPU5,
		LAHAINA_SLAVE_EPSS_L3_CPU6, LAHAINA_SLAVE_EPSS_L3_CPU7,
		LAHAINA_SLAVE_EPSS_L3_SHARED);

DEFINE_QNODE(slv_epss_l3_cpu0, LAHAINA_SLAVE_EPSS_L3_CPU0, 1, 1, 0);
DEFINE_QNODE(slv_epss_l3_cpu1, LAHAINA_SLAVE_EPSS_L3_CPU1, 1, 1, 1);
DEFINE_QNODE(slv_epss_l3_cpu2, LAHAINA_SLAVE_EPSS_L3_CPU2, 1, 1, 2);
DEFINE_QNODE(slv_epss_l3_cpu3, LAHAINA_SLAVE_EPSS_L3_CPU3, 1, 1, 3);
DEFINE_QNODE(slv_epss_l3_cpu4, LAHAINA_SLAVE_EPSS_L3_CPU4, 1, 2, 0);
DEFINE_QNODE(slv_epss_l3_cpu5, LAHAINA_SLAVE_EPSS_L3_CPU5, 1, 2, 1);
DEFINE_QNODE(slv_epss_l3_cpu6, LAHAINA_SLAVE_EPSS_L3_CPU6, 1, 2, 2);
DEFINE_QNODE(slv_epss_l3_cpu7, LAHAINA_SLAVE_EPSS_L3_CPU7, 1, 3, 0);
DEFINE_QNODE(slv_epss_l3_shared, LAHAINA_SLAVE_EPSS_L3_SHARED, 1, 0, 0);

static struct qcom_icc_node *lahaina_epss_l3_nodes[] = {
	[MASTER_EPSS_L3_APPS] = &mas_epss_l3_apps,
	[SLAVE_EPSS_L3_CPU0] = &slv_epss_l3_cpu0,
	[SLAVE_EPSS_L3_CPU1] = &slv_epss_l3_cpu1,
	[SLAVE_EPSS_L3_CPU2] = &slv_epss_l3_cpu2,
	[SLAVE_EPSS_L3_CPU3] = &slv_epss_l3_cpu3,
	[SLAVE_EPSS_L3_CPU4] = &slv_epss_l3_cpu4,
	[SLAVE_EPSS_L3_CPU5] = &slv_epss_l3_cpu5,
	[SLAVE_EPSS_L3_CPU6] = &slv_epss_l3_cpu6,
	[SLAVE_EPSS_L3_CPU7] = &slv_epss_l3_cpu7,
	[SLAVE_EPSS_L3_SHARED] = &slv_epss_l3_shared,
};

static struct qcom_icc_desc lahaina_epss_l3 = {
	.nodes = lahaina_epss_l3_nodes,
	.num_nodes = ARRAY_SIZE(lahaina_epss_l3_nodes),
};

DEFINE_QNODE(slv_epss_l3_cpu7_direwolf, LAHAINA_SLAVE_EPSS_L3_CPU7, 1, 2, 3);

static struct qcom_icc_node *direwolf_epss_l3_nodes[] = {
	[MASTER_EPSS_L3_APPS] = &mas_epss_l3_apps,
	[SLAVE_EPSS_L3_CPU0] = &slv_epss_l3_cpu0,
	[SLAVE_EPSS_L3_CPU1] = &slv_epss_l3_cpu1,
	[SLAVE_EPSS_L3_CPU2] = &slv_epss_l3_cpu2,
	[SLAVE_EPSS_L3_CPU3] = &slv_epss_l3_cpu3,
	[SLAVE_EPSS_L3_CPU4] = &slv_epss_l3_cpu4,
	[SLAVE_EPSS_L3_CPU5] = &slv_epss_l3_cpu5,
	[SLAVE_EPSS_L3_CPU6] = &slv_epss_l3_cpu6,
	[SLAVE_EPSS_L3_CPU7] = &slv_epss_l3_cpu7_direwolf,
	[SLAVE_EPSS_L3_SHARED] = &slv_epss_l3_shared,
};

static struct qcom_icc_desc direwolf_epss_l3 = {
	.nodes = direwolf_epss_l3_nodes,
	.num_nodes = ARRAY_SIZE(direwolf_epss_l3_nodes),
};

static int qcom_icc_aggregate(struct icc_node *node, u32 tag, u32 avg_bw,
			      u32 peak_bw, u32 *agg_avg, u32 *agg_peak)
{
	*agg_avg += avg_bw;
	*agg_peak = max_t(u32, *agg_peak, peak_bw);

	return 0;
}

static int qcom_icc_l3_cpu_set(struct icc_node *src, struct icc_node *dst)
{
	struct qcom_epss_l3_icc_provider *qp;
	struct icc_provider *provider;
	struct qcom_icc_node *qn;
	unsigned int index;
	u64 rate;

	qn = dst->data;
	provider = src->provider;
	qp = to_qcom_provider(provider);

	rate = dst->peak_bw;

	for (index = 0; index < qp->max_state; index++) {
		if (qp->lut_freqs[index] >= rate)
			break;
	}

	writel_relaxed(index, EPSS_L3_VOTE_REG(qp->base, qn->domain, qn->cpu));
	return 0;
}

static int qcom_epss_l3_remove(struct platform_device *pdev)
{
	struct qcom_epss_l3_icc_provider *qp = platform_get_drvdata(pdev);
	struct icc_provider *provider = &qp->provider;
	struct icc_node *n;

	list_for_each_entry(n, &provider->nodes, node_list) {
		icc_node_del(n);
		icc_node_destroy(n->id);
	}

	return icc_provider_del(provider);
}

static int qcom_epss_l3_probe(struct platform_device *pdev)
{
	u32 info, src, lval, i, prev_freq = 0, freq;
	static unsigned long hw_rate, xo_rate;
	struct qcom_epss_l3_icc_provider *qp;
	const struct qcom_icc_desc *desc;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct qcom_icc_node **qnodes;
	const char *compat = NULL;
	struct icc_node *node;
	struct resource *res;
	size_t num_nodes;
	struct clk *clk;
	int compatlen;
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

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOMEM;

	qp->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(qp->base))
		return PTR_ERR(qp->base);

	for (i = 0; i < LUT_MAX_ENTRIES; i++) {
		info = readl_relaxed(qp->base + REG_FREQ_LUT +
				     i * LUT_ROW_SIZE);
		src = FIELD_GET(LUT_SRC, info);
		lval = FIELD_GET(LUT_L_VAL, info);
		if (src)
			freq = xo_rate * lval;
		else
			freq = hw_rate;

		/*
		 * Two of the same frequencies means end of table.
		 */
		if (i > 0 && prev_freq == freq)
			break;

		qp->lut_freqs[i] = freq;
		prev_freq = freq;
	}
	qp->max_state = i;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	qnodes = desc->nodes;
	num_nodes = desc->num_nodes;

	data = devm_kcalloc(&pdev->dev, num_nodes, sizeof(*node), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	provider = &qp->provider;
	provider->dev = &pdev->dev;
	provider->set = qcom_icc_l3_cpu_set;
	provider->aggregate = qcom_icc_aggregate;
	provider->xlate = of_icc_xlate_onecell;
	INIT_LIST_HEAD(&provider->nodes);
	provider->data = data;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;

	ret = icc_provider_add(provider);
	if (ret) {
		dev_err(&pdev->dev, "error adding interconnect provider\n");
		return ret;
	}

	for (i = 0; i < num_nodes; i++) {
		size_t j;

		node = icc_node_create(qnodes[i]->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}

		node->name = qnodes[i]->name;
		node->data = qnodes[i];
		icc_node_add(node, provider);

		dev_dbg(&pdev->dev, "registered node %s %d\n",
			qnodes[i]->name, node->id);

		/* populate links */
		for (j = 0; j < qnodes[i]->num_links; j++)
			icc_link_create(node, qnodes[i]->links[j]);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;

	platform_set_drvdata(pdev, qp);

	return ret;
err:
	qcom_epss_l3_remove(pdev);
	return ret;
}

static const struct of_device_id epss_l3_of_match[] = {
	{ .compatible = "qcom,lahaina-epss-l3-cpu", .data = &lahaina_epss_l3 },
	{ .compatible = "qcom,direwolf-epss-l3-cpu", .data = &direwolf_epss_l3 },
	{ },
};
MODULE_DEVICE_TABLE(of, epss_l3_of_match);

static struct platform_driver epss_l3_driver = {
	.probe = qcom_epss_l3_probe,
	.remove = qcom_epss_l3_remove,
	.driver = {
		.name = "epss-l3",
		.of_match_table = epss_l3_of_match,
	},
};
module_platform_driver(epss_l3_driver);

MODULE_DESCRIPTION("QTI EPSS L3 interconnect driver");
MODULE_LICENSE("GPL v2");
