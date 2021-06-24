/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"

#define MUTEX_MAX_MOD_REGS	((MML_MAX_COMPONENTS + 31) >> 5)

/* MUTEX register offset */
#define MUTEX_EN(id)		(0x20 + (id) * 0x20)
#define MUTEX_MOD(id, offset)	((offset) + (id) * 0x20)
#define MUTEX_SOF(id)		(0x2c + (id) * 0x20)

struct mutex_data {
	/* Count of display mutex HWs */
	u32 mutex_cnt;
	/* Offsets and count of MUTEX_MOD registers per mutex */
	u32 mod_offsets[MUTEX_MAX_MOD_REGS];
	u32 mod_cnt;
};

static const struct mutex_data mt6893_mutex_data = {
	.mutex_cnt = 16,
	.mod_offsets = {0x30, 0x34},
	.mod_cnt = 2,
};

struct mutex_module {
	u32 mutex_id;
	u32 index;
	u32 field;
	bool select:1;
	bool trigger:1;
};

struct mml_mutex {
	struct mml_comp comp;
	const struct mutex_data *data;

	struct mutex_module modules[MML_MAX_COMPONENTS];
};

static s32 mutex_trigger(struct mml_comp *comp, struct mml_task *task,
			 struct mml_comp_config *ccfg)
{
	struct mml_mutex *mutex = container_of(comp, struct mml_mutex, comp);
	const struct mml_topology_path *path = task->config->path[ccfg->pipe];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	u32 mutex_id = 0;
	u32 mutex_mod[MUTEX_MAX_MOD_REGS] = {0};
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mutex_module *mod = &mutex->modules[path->nodes[i].id];

		if (mod->select)
			mutex_id = mod->mutex_id;
		if (mod->trigger)
			mutex_mod[mod->index] |= 1 << mod->field;
	}

	for (i = 0; i < mutex->data->mod_cnt; i++) {
		u32 offset = mutex->data->mod_offsets[i];

		cmdq_pkt_write(pkt, NULL, base_pa + MUTEX_MOD(mutex_id, offset),
			       mutex_mod[i], U32_MAX);
	}
	cmdq_pkt_write(pkt, NULL, base_pa + MUTEX_SOF(mutex_id), 0x0, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + MUTEX_EN(mutex_id), 0x1, U32_MAX);
	return 0;
}

static const struct mml_comp_config_ops mutex_config_ops = {
	.mutex = mutex_trigger
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_mutex *mutex = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &mutex->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_mutex *mutex = dev_get_drvdata(dev);

	mml_unregister_comp(master, &mutex->comp);
}


static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_mutex *dbg_probed_components[2];
static int dbg_probed_count;

static void mutex_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	struct mml_mutex *mutex = container_of(comp, struct mml_mutex, comp);
	u8 i, j;

	mml_err("mutex component %u dump:", comp->id);

	for (i = 0; i < mutex->data->mutex_cnt; i++)
		for (j = 0; j < mutex->data->mod_cnt; j++) {
			u32 offset = mutex->data->mod_offsets[j];
			u32 value;

			value = readl(base + MUTEX_MOD(i, offset));
			mml_err("MDP_MUTEX%d_MOD%d %#010x",
			i, j, value);
		}
}

static const struct mml_comp_debug_ops mutex_debug_ops = {
	.dump = &mutex_debug_dump,
};

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_mutex *priv;
	struct device_node *node = dev->of_node;
	struct property *prop;
	const char *name;
	u32 mod[3], comp_id, mutex_id;
	s32 id_count, i, ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (const struct mutex_data *)of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	of_property_for_each_string(node, "mutex-comps", prop, name) {
		ret = of_property_read_u32_array(node, name, mod, 3);
		if (ret) {
			dev_err(dev, "no property %s in dts node %s: %d\n",
				name, dev->of_node->full_name, ret);
			return ret;
		}
		if (mod[0] >= MML_MAX_COMPONENTS) {
			dev_err(dev, "%s id %u is larger than max:%d\n",
				name, mod[0], MML_MAX_COMPONENTS);
			return -EINVAL;
		}
		if (mod[1] >= priv->data->mod_cnt) {
			dev_err(dev,
				"%s mod index %u is larger than count:%d\n",
				name, mod[1], priv->data->mod_cnt);
			return -EINVAL;
		}
		if (mod[2] >= 32) {
			dev_err(dev,
				"%s mod field %u is larger than bits:32\n",
				name, mod[2]);
			return -EINVAL;
		}
		priv->modules[mod[0]].index = mod[1];
		priv->modules[mod[0]].field = mod[2];
		priv->modules[mod[0]].trigger = true;
	}

	id_count = of_property_count_u32_elems(node, "mutex-ids");
	for (i = 0; i + 1 < id_count; i += 2) {
		of_property_read_u32_index(node, "mutex-ids", i, &comp_id);
		of_property_read_u32_index(node, "mutex-ids", i + 1, &mutex_id);
		if (comp_id >= MML_MAX_COMPONENTS) {
			dev_err(dev, "component id %u is larger than max:%d\n",
				comp_id, MML_MAX_COMPONENTS);
			return -EINVAL;
		}
		if (mutex_id >= priv->data->mutex_cnt) {
			dev_err(dev, "mutex id %u is larger than count:%d\n",
				mutex_id, priv->data->mutex_cnt);
			return -EINVAL;
		}
		priv->modules[comp_id].mutex_id = mutex_id;
		priv->modules[comp_id].select = true;
	}

	priv->comp.config_ops = &mutex_config_ops;
	priv->comp.debug_ops= &mutex_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_mutex_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6893-mml_mutex",
		.data = &mt6893_mutex_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_mutex_driver_dt_match);

struct platform_driver mml_mutex_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
			.name = "mediatek-mml-mutex",
			.owner = THIS_MODULE,
			.of_match_table = mml_mutex_driver_dt_match,
		},
};

//module_platform_driver(mml_mutex_driver);

static s32 ut_case;
static s32 ut_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = sscanf(val, "%d", &ut_case);
	if (result != 1) {
		mml_err("invalid input: %s, result(%d)", val, result);
		return -EINVAL;
	}
	mml_log("%s: case_id=%d", __func__, ut_case);

	switch (ut_case) {
	case 0:
		mml_log("use read to dump current pwm setting");
		break;
	default:
		mml_err("invalid case_id: %d", ut_case);
		break;
	}

	mml_log("%s END", __func__);
	return 0;
}

static s32 ut_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (ut_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", ut_case, dbg_probed_count);
		for(i = 0; i < dbg_probed_count; i++) {
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] comp.id: %d\n", i,
				dbg_probed_components[i]->comp.id);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -      mml.bind: %d\n",
				dbg_probed_components[i]->comp.bound);
		}
	default:
		mml_err("not support read for case_id: %d", ut_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static struct kernel_param_ops up_param_ops = {
	.set = ut_set,
	.get = ut_get,
};
module_param_cb(mutex_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(mutex_ut_case, "mml mutex UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML Mutex driver");
MODULE_LICENSE("GPL v2");
