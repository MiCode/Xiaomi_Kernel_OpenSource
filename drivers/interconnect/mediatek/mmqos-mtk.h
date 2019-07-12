/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ming-Fan Chen <ming-fan.chen@mediatek.com>
 */
#ifndef MMQOS_MTK_H
#define MMQOS_MTK_H

#include <linux/interconnect-provider.h>
#include <linux/workqueue.h>

#define MMQOS_NO_LINK	(0xffffffff)
#define MMQOS_MAX_COMM_PORT_NUM	(15)

struct mmqos_base_node {
	struct icc_node *icc_node;
	u32	mix_bw;
};

struct common_node {
	struct mmqos_base_node *base;
	const char *clk_name;
	struct clk *clk;
	u64 freq;
	struct list_head list;
	struct icc_path *icc_path;
	struct work_struct work;
};

struct common_port_node {
	struct mmqos_base_node *base;
	struct common_node *common;
	struct device *larb_dev;
	struct mutex bw_lock;
	u32 latest_mix_bw;
	u32 latest_peak_bw;
	u32 latest_avg_bw;
	struct list_head list;
};

struct larb_node {
	struct mmqos_base_node *base;
	struct device *larb_dev;
};

struct larb_port_node {
	struct mmqos_base_node *base;
	u16 bw_ratio;
};

struct mtk_mmqos {
	struct device *dev;
	struct icc_provider prov;
	struct notifier_block nb;
	struct list_head comm_list;
	struct list_head comm_port_list;
	struct workqueue_struct *wq;
	u32 max_ratio;
};

struct mtk_node_desc {
	const char *name;
	u32 id;
	u32 link;
	u16 bw_ratio;
};

struct mtk_mmqos_desc {
	const struct mtk_node_desc *nodes;
	const size_t num_nodes;
	const char * const *comm_muxes;
	const char * const *comm_icc_path_names;
	const u32 max_ratio;
};

#define DEFINE_MNODE(_name, _id, _bw_ratio, _link) {	\
	.name = #_name,	\
	.id = _id,	\
	.bw_ratio = _bw_ratio,	\
	.link = _link,	\
	}

int mtk_mmqos_probe(struct platform_device *pdev);
int mtk_mmqos_remove(struct platform_device *pdev);

#endif /* MMQOS_MTK_H */
