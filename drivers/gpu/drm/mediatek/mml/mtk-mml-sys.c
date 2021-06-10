/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"

#define MML_MAX_SYS_COMPONENTS	10
#define MML_MAX_SYS_MUX_PINS	50

struct mml_data {
};

static const struct mml_data mt6893_mml_data = {
};

enum mml_mux_type {
	UNUSED,
	MOUT,
	SOUT,
	SELIN,
};

struct mml_mux_pin {
	u16 index;
	u16 from;
	u16 to;
	u16 type;
	u16 offset;
} __attribute__ ((__packed__));

struct mml_sys {
	const struct mml_data *data;
	struct mml_comp comps[MML_MAX_SYS_COMPONENTS];
	u32 comp_cnt;
	u32 comp_bound;

	/* MML multiplexer pins.
	 * The entry 0 leaves empty for efficiency, do not use. */
	struct mml_mux_pin mux_pins[MML_MAX_SYS_MUX_PINS + 1];
	u32 mux_cnt;
	/* Table of component adjacency with mux pins.
	 *
	 * The element value is an index to mux pin array.
	 * In the upper-right tri. are MOUTs or SOUTs by adjacency[from][to];
	 * in the bottom-left tri. are SELIN pins by adjacency[to][from].
	 * Direct-wire is not in this table.
	 *
	 * array data would be like:
	 *	[0] = { 0  indices of },
	 *	[1] = { . 0 . MOUTs & },
	 *	[2] = { . . 0 . SOUTs },
	 *	[3] = { . . . 0 . . . },
	 *	[4] = { . . . . 0 . . },
	 *	[5] = { indices . 0 . },
	 *	[6] = { of SELINs . 0 },
	 */
	u8 adjacency[MML_MAX_SYS_COMPONENTS][MML_MAX_SYS_COMPONENTS];
};

static void config_mux(struct mml_sys *sys, struct cmdq_pkt *pkt,
		       const phys_addr_t base_pa, u8 mux_idx,
		       u16 *offset, u32 *mout)
{
	struct mml_mux_pin *mux;

	if (!mux_idx)
		return;
	mux = &sys->mux_pins[mux_idx];

	switch (mux->type) {
	case MOUT:
		*offset = mux->offset;
		*mout |= 1 << mux->index;
		break;
	case SOUT:
	case SELIN:
		cmdq_pkt_write(pkt, NULL, base_pa + mux->offset,
			       mux->index, U32_MAX);
		break;
	default:
		break;
	}
}

static s32 sys_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u8 idx)
{
	struct mml_sys *sys = container_of(comp, struct mml_sys, comps[0]);
	const struct mml_topology_path *path = task->config->path[ccfg->pipe];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	u32 i, j;

	for (i = 0; i < path->node_cnt; i++) {
		const struct mml_path_node *node = &path->nodes[i];
		u16 offset = 0;
		u32 mout = 0;
		u8 from = node->id, to, mux_idx;

		/* TODO: continue if node disabled */
		for (j = 0; j < ARRAY_SIZE(node->next); j++) {
			if (node->next[j]) {	/* && next enabled */
				to = node->next[j]->id;
				mux_idx = sys->adjacency[from][to];
				config_mux(sys, pkt, base_pa, mux_idx,
					   &offset, &mout);
				mux_idx = sys->adjacency[to][from];
				config_mux(sys, pkt, base_pa, mux_idx,
					   &offset, &mout);
			}
		}
		if (mout)
			cmdq_pkt_write(pkt, NULL, base_pa + offset,
				       mout, U32_MAX);
	}
	return 0;
}

static const struct mml_comp_config_ops sys_config_ops = {
	.tile = sys_config_tile
};

static int mml_sys_init(struct platform_device *pdev, struct mml_sys *sys,
	const struct component_ops *comp_ops)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	int mux_cnt, comp_cnt, i;
	int ret;

	sys->data = (const struct mml_data *)of_device_get_match_data(dev);

	/* Initialize mux-pins */
	mux_cnt = of_property_count_elems_of_size(node, "mux-pins",
						  sizeof(struct mml_mux_pin));
	if (mux_cnt < 0 || mux_cnt > MML_MAX_SYS_MUX_PINS) {
		dev_err(dev, "no mux-pins or out of size in component %s: %d\n",
			node->full_name, mux_cnt);
		return -EINVAL;
	}

	of_property_read_u16_array(node, "mux-pins", (u16 *)&sys->mux_pins[1],
		mux_cnt * (sizeof(struct mml_mux_pin) / sizeof(u16)));
	for (i = 1; i <= mux_cnt; i++) {
		struct mml_mux_pin *mux = &sys->mux_pins[i];

		if (mux->type == SELIN)
			sys->adjacency[mux->to][mux->from] = i;
		else
			sys->adjacency[mux->from][mux->to] = i;
	}

	/* Initialize component and subcomponents */
	comp_cnt = of_mml_count_comps(node);
	if (comp_cnt <= 0) {
		dev_err(dev, "no comp-ids in component %s: %d\n",
			node->full_name, comp_cnt);
		return -EINVAL;
	}

	for (i = 0; i < comp_cnt; i++) {
		ret = mml_subcomp_init(pdev, i, &sys->comps[i]);
		if (ret) {
			dev_err(dev, "failed to init mmlsys comp-%d: %d\n",
				i, ret);
			return ret;
		}
	}

	/* TODO: distinguish component and subcomponents */
	sys->comps[0].config_ops = &sys_config_ops;

	ret = component_add(dev, comp_ops);
	if (ret) {
		dev_err(dev, "failed to add mmlsys comp-%d: %d\n", 0, ret);
		return ret;
	}
	for (i = 1; i < comp_cnt; i++) {
		ret = component_add_typed(dev, comp_ops, i);
		if (ret) {
			dev_err(dev, "failed to add mmlsys comp-%d: %d\n",
				i, ret);
			goto err_comp_add;
		}
	}
	sys->comp_cnt = comp_cnt;
	return 0;

err_comp_add:
	for (; i > 0; i--)
		component_del(dev, comp_ops);
	return ret;
}

struct mml_sys *mml_sys_create(struct platform_device *pdev,
	const struct component_ops *comp_ops)
{
	struct device *dev = &pdev->dev;
	struct mml_sys *sys;
	int ret;

	sys = devm_kzalloc(dev, sizeof(*sys), GFP_KERNEL);
	if (!sys)
		return ERR_PTR(-ENOMEM);

	ret = mml_sys_init(pdev, sys, comp_ops);
	if (ret) {
		dev_err(dev, "failed to init mml sys: %d\n", ret);
		devm_kfree(dev, sys);
		return ERR_PTR(ret);
	}
	return sys;
}

void mml_sys_destroy(struct platform_device *pdev, struct mml_sys *sys,
	const struct component_ops *comp_ops)
{
	int i;

	for (i = 0; i < sys->comp_cnt; i++)
		component_del(&pdev->dev, comp_ops);
	devm_kfree(&pdev->dev, sys);
}

int mml_sys_bind(struct device *dev, struct device *master,
	struct mml_sys *sys)
{
	s32 ret;

	if (WARN_ON(sys->comp_bound >= sys->comp_cnt))
		return -ERANGE;
	ret = mml_register_comp(master, &sys->comps[sys->comp_bound++]);
	if (ret) {
		dev_err(dev, "failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
		sys->comp_bound--;
	}
	return ret;
}

void mml_sys_unbind(struct device *dev, struct device *master,
	struct mml_sys *sys)
{
	if (WARN_ON(sys->comp_bound <= 0))
		return;
	mml_unregister_comp(master, &sys->comps[--sys->comp_bound]);
}

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	return mml_sys_bind(dev, master, dev_get_drvdata(dev));
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	mml_sys_unbind(dev, master, dev_get_drvdata(dev));
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static int probe(struct platform_device *pdev)
{
	struct mml_sys *priv;

	priv = mml_sys_create(pdev, &mml_comp_ops);
	if (IS_ERR(priv)) {
		dev_err(&pdev->dev, "failed to init mml sys: %d\n",
			PTR_ERR(priv));
		return PTR_ERR(priv);
	}
	platform_set_drvdata(pdev, priv);
	return 0;
}

static int remove(struct platform_device *pdev)
{
	mml_sys_destroy(pdev, platform_get_drvdata(pdev), &mml_comp_ops);
	return 0;
}

const struct of_device_id mtk_mml_of_ids[] = {
	{
		.compatible = "mediatek,mt6893-mml",
		.data = &mt6893_mml_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mml_of_ids);

static const struct of_device_id mml_sys_of_ids[] = {
	{
		.compatible = "mediatek,mt6893-mml_sys",
		.data = &mt6893_mml_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_sys_of_ids);

static struct platform_driver mml_sys_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mmlsys",
		.owner = THIS_MODULE,
		.of_match_table = mml_sys_of_ids,
	},
};
module_platform_driver(mml_sys_driver);

MODULE_DESCRIPTION("MediaTek SoC display MMLSYS driver");
MODULE_AUTHOR("Ping-Hsun Wu <ping-hsun.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
