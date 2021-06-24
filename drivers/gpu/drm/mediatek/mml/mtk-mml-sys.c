/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"

#define MML_MAX_SYS_COMPONENTS	10
#define MML_MAX_SYS_MUX_PINS	88
#define MML_MAX_SYS_DL_INS	4
#define MML_MAX_SYS_DBG_REGS	60

enum mml_comp_type {
	MML_CT_COMPONENT = 0,
	MML_CT_SYS,
	MML_CT_PATH,
	MML_CT_DL_IN,
	MML_CT_DL_OUT,

	MML_COMP_TYPE_TOTAL
};

struct mml_comp_sys;

struct mml_data {
	int (*comp_inits[MML_COMP_TYPE_TOTAL])(struct device *dev,
		struct mml_comp_sys *sys, struct mml_comp *comp);
};

enum mml_mux_type {
	MML_MUX_UNUSED = 0,
	MML_MUX_MOUT,
	MML_MUX_SOUT,
	MML_MUX_SELIN,
};

struct mml_mux_pin {
	u16 index;
	u16 from;
	u16 to;
	u16 type;
	u16 offset;
} __attribute__ ((__packed__));

struct mml_dbg_reg {
	char name[16];
	u32 offset;
};

struct mml_comp_sys {
	const struct mml_data *data;
	struct mml_comp comps[MML_MAX_SYS_COMPONENTS];
	u32 comp_cnt;
	u32 comp_bound;

	/* MML multiplexer pins.
	 * The entry 0 leaves empty for efficiency, do not use. */
	struct mml_mux_pin mux_pins[MML_MAX_SYS_MUX_PINS + 1];
	u32 mux_cnt;
	u16 dl_offsets[MML_MAX_SYS_DL_INS + 1];
	u32 dl_cnt;

	/* Table of component or adjacency data index.
	 *
	 * The element is an index to data arrays.
	 * The component data by type is indexed by adjacency[id][id];
	 * in the upper-right tri. are MOUTs and SOUTs by adjacency[from][to];
	 * in the bottom-left tri. are SELIN pins by adjacency[to][from].
	 * Direct-wires are not in this table.
	 *
	 * Ex.:
	 *	dl_offsets[adjacency[DLI0][DLI0]] is offset of comp DLI0.
	 *	mux_pins[adjacency[RDMA0][RSZ0]] is MOUT from RDMA0 to RSZ0.
	 *	mux_pins[adjacency[RSZ0][RDMA0]] is SELIN from RDMA0 to RSZ0.
	 *
	 * array data would be like:
	 *	[0] = { T  indices of },	T: indices of component data
	 *	[1] = { . T . MOUTs & },	   (by component type, the
	 *	[2] = { . . T . SOUTs },	    indices refer to different
	 *	[3] = { . . . T . . . },	    data arrays.)
	 *	[4] = { . . . . T . . },
	 *	[5] = { indices . T . },
	 *	[6] = { of SELINs . T },
	 */
	u8 adjacency[MML_MAX_COMPONENTS][MML_MAX_COMPONENTS];
	struct mml_dbg_reg dbg_regs[MML_MAX_SYS_DBG_REGS];
	u32 dbg_reg_cnt;
};

static inline struct mml_comp_sys *comp_to_sys(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_sys, comps[comp->sub_idx]);
}

static void config_mux(struct mml_comp_sys *sys, struct cmdq_pkt *pkt,
		       const phys_addr_t base_pa, u8 mux_idx,
		       u16 *offset, u32 *mout)
{
	struct mml_mux_pin *mux;

	if (!mux_idx)
		return;
	mux = &sys->mux_pins[mux_idx];

	switch (mux->type) {
	case MML_MUX_MOUT:
		*offset = mux->offset;
		*mout |= 1 << mux->index;
		break;
	case MML_MUX_SOUT:
	case MML_MUX_SELIN:
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
	struct mml_comp_sys *sys = comp_to_sys(comp);
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

static s32 dl_config_tile(struct mml_comp *comp, struct mml_task *task,
			  struct mml_comp_config *ccfg, u8 idx)
{
	struct mml_comp_sys *sys = comp_to_sys(comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u16 offset = sys->dl_offsets[sys->adjacency[comp->id][comp->id]];
	u32 dl_w = tile->in.xe - tile->in.xs + 1;
	u32 dl_h = tile->in.ye - tile->in.ys + 1;

	cmdq_pkt_write(pkt, NULL, base_pa + offset,
		       (dl_h << 16) + dl_w, U32_MAX);
	return 0;
}

static const struct mml_comp_config_ops dl_config_ops = {
	.tile = dl_config_tile
};

static void sys_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	struct mml_comp_sys *sys = comp_to_sys(comp);
	u32 value;
	u32 i;

	mml_err("mml component %u dump:", comp->id);

	for (i = 0; i < sys->dbg_reg_cnt; i++) {
		value = readl(base + sys->dbg_regs[i].offset);
		mml_err("%s %#010x", sys->dbg_regs[i].name, value);
	}
}

static const struct mml_comp_debug_ops sys_debug_ops = {
	.dump = &sys_debug_dump,
};

static int sys_comp_init(struct device *dev, struct mml_comp_sys *sys,
			 struct mml_comp *comp)
{
	struct device_node *node = dev->of_node;
	int mux_cnt, i;
	struct property *prop;
	const char *name;
	const __be32 *p;
	u32 value;

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

		if (mux->from >= MML_MAX_COMPONENTS ||
		    mux->to >= MML_MAX_COMPONENTS) {
			dev_err(dev, "comp idx %hu %hu out of boundary",
				mux->from, mux->to);
			continue;
		}
		if (mux->type == MML_MUX_SELIN)
			sys->adjacency[mux->to][mux->from] = i;
		else
			sys->adjacency[mux->from][mux->to] = i;
	}

	/* Initialize dbg-regs */
	i = 0;
	of_property_for_each_u32(node, "dbg-reg-offsets", prop, p, value) {
		if (i > MML_MAX_SYS_DBG_REGS) {
			dev_err(dev, "no dbg-reg-offsets or out of size in component %s: %d\n",
				node->full_name, i);
				return -EINVAL;
		}
		sys->dbg_regs[i].offset = value;
		i++;
	}
	sys->dbg_reg_cnt = i;

	i = 0;
	of_property_for_each_string(node, "dbg-reg-names", prop, name) {
		if (i > sys->dbg_reg_cnt) {
			dev_err(dev, "dbg-reg-names size over offsets size %s: %d\n",
				node->full_name, i);
				return -EINVAL;
		}
		memcpy(sys->dbg_regs[i].name, name, sizeof(sys->dbg_regs[i].name));
		i++;
	}

	comp->config_ops = &sys_config_ops;
	comp->debug_ops = &sys_debug_ops;
	return 0;
}

static int dl_comp_init(struct device *dev, struct mml_comp_sys *sys,
			struct mml_comp *comp)
{
	struct device_node *node = dev->of_node;
	char name[32] = "";
	u16 offset;
	int ret;

	if (sys->dl_cnt >= ARRAY_SIZE(sys->dl_offsets) - 1) {
		dev_err(dev, "out of dl-relay size in component %s: %d\n",
			node->full_name, sys->dl_cnt + 1);
		return -EINVAL;
	}
	if (!comp->name) {
		dev_err(dev, "no comp-name of mmlsys comp-%d (type dl-in)\n",
			comp->sub_idx);
		return -EINVAL;
	}

	ret = snprintf(name, sizeof(name), "%s-dl-relay", comp->name);
	if (ret >= sizeof(name)) {
		dev_err(dev, "len:%d over name size:%d", ret, sizeof(name));
		name[sizeof(name) - 1] = '\0';
	}
	ret = of_property_read_u16(node, name, &offset);
	if (ret) {
		dev_err(dev, "no %s property in node %s\n",
			name, node->full_name);
		return ret;
	}

	sys->dl_offsets[++sys->dl_cnt] = offset;
	sys->adjacency[comp->id][comp->id] = sys->dl_cnt;
	comp->config_ops = &dl_config_ops;
	/* TODO: pmqos_op */
	return 0;
}

static int subcomp_init(struct platform_device *pdev, struct mml_comp_sys *sys,
			int subcomponent)
{
	struct device *dev = &pdev->dev;
	struct mml_comp *comp = &sys->comps[subcomponent];
	u32 comp_type;
	int ret;

	ret = mml_subcomp_init(pdev, subcomponent, comp);
	if (ret)
		return ret;

	if (of_property_read_u32_index(dev->of_node, "comp-types",
				       subcomponent, &comp_type)) {
		dev_info(dev, "no comp-type of mmlsys comp-%d\n", subcomponent);
		return 0;
	}
	if (sys->data->comp_inits[comp_type])
		ret = sys->data->comp_inits[comp_type](dev, sys, comp);
	return ret;
}

static int mml_sys_init(struct platform_device *pdev, struct mml_comp_sys *sys,
			const struct component_ops *comp_ops)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	int comp_cnt, i;
	int ret;

	sys->data = (const struct mml_data *)of_device_get_match_data(dev);

	/* Initialize component and subcomponents */
	comp_cnt = of_mml_count_comps(node);
	if (comp_cnt <= 0) {
		dev_err(dev, "no comp-ids in component %s: %d\n",
			node->full_name, comp_cnt);
		return -EINVAL;
	}

	for (i = 0; i < comp_cnt; i++) {
		ret = subcomp_init(pdev, sys, i);
		if (ret) {
			dev_err(dev, "failed to init mmlsys comp-%d: %d\n",
				i, ret);
			return ret;
		}
	}

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

struct mml_comp_sys *mml_sys_create(struct platform_device *pdev,
				    const struct component_ops *comp_ops)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_sys *sys;
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

void mml_sys_destroy(struct platform_device *pdev, struct mml_comp_sys *sys,
		     const struct component_ops *comp_ops)
{
	int i;

	for (i = 0; i < sys->comp_cnt; i++)
		component_del(&pdev->dev, comp_ops);
	devm_kfree(&pdev->dev, sys);
}

int mml_sys_bind(struct device *dev, struct device *master,
		 struct mml_comp_sys *sys)
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
		    struct mml_comp_sys *sys)
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
	struct mml_comp_sys *priv;

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

static const struct mml_data mt6893_mml_data = {
	.comp_inits = {
		[MML_CT_SYS] = &sys_comp_init,
		[MML_CT_DL_IN] = &dl_comp_init,
	},
};

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

struct platform_driver mml_sys_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mmlsys",
		.owner = THIS_MODULE,
		.of_match_table = mml_sys_of_ids,
	},
};
//module_platform_driver(mml_sys_driver);

MODULE_DESCRIPTION("MediaTek SoC display MMLSYS driver");
MODULE_AUTHOR("Ping-Hsun Wu <ping-hsun.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
