// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ming-Fan Chen <ming-fan.chen@mediatek.com>
 */
#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <linux/clk.h>
#include <linux/interconnect-provider.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/mediatek/mtk_mmdvfs.h>
#include <soc/mediatek/smi.h>
#include "mtk_iommu.h"
#include "mmqos-mtk.h"
#define SHIFT_ROUND(a, b)	((((a) - 1) >> (b)) + 1)
#define icc_to_MBps(x)	((x) / 1000)
#define MASK_16(a) (a & 0xff)

struct common_port_node {
	struct mmqos_base_node *base;
	struct common_node *common;
	struct device *larb_dev;
	struct mutex bw_lock;
	u32 latest_mix_bw;
	u32 latest_peak_bw;
	u32 latest_avg_bw;
	struct list_head list;
	u8 channel;
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
	struct workqueue_struct *wq;
	u32 max_ratio;
	bool qos_bound; /* Todo: Set qos_bound to true if necessary */
};

static void mmqos_update_comm_bw(struct device *dev,
	u32 comm_port, u32 freq, u64 mix_bw, u64 bw_peak, bool qos_bound)
{
	u32 comm_bw = 0;
	u32 value;

	if (!freq || !dev)
		return;
	if (mix_bw)
		comm_bw = (mix_bw << 8) / freq;
	if (comm_bw)
		value = ((comm_bw > 0xfff) ? 0xfff : comm_bw) |
			((bw_peak > 0 || !qos_bound) ? 0x1000 : 0x3000);
	else
		value = 0x1200;
	mtk_smi_common_bw_set(dev, comm_port, value);
	dev_dbg(dev, "comm port=%d bw=%d freq=%d qos_bound=%d value=%#x\n",
		comm_port, comm_bw, freq, qos_bound, value);
}

static int update_mm_clk(struct notifier_block *nb,
		unsigned long value, void *v)
{
	struct mtk_mmqos *mmqos =
		container_of(nb, struct mtk_mmqos, nb);
	struct common_node *comm_node;
	struct common_port_node *comm_port;

	list_for_each_entry(comm_node, &mmqos->comm_list, list) {
		comm_node->freq = clk_get_rate(comm_node->clk)/1000000;
		list_for_each_entry(comm_port,
				    &comm_node->comm_port_list, list) {
			mutex_lock(&comm_port->bw_lock);
			if (comm_port->latest_mix_bw
				|| comm_port->latest_peak_bw) {
				mmqos_update_comm_bw(comm_port->larb_dev,
					MASK_16(comm_port->base->icc_node->id),
					comm_port->common->freq,
					icc_to_MBps(comm_port->latest_mix_bw),
					icc_to_MBps(comm_port->latest_peak_bw),
					mmqos->qos_bound);
			}
			mutex_unlock(&comm_port->bw_lock);
		}
	}
	return 0;
}

static void set_chn_bw(u32 *bw_array, u8 chn_id, u32 bw)
{
	u32 i;

	for (i = 0; i < MMQOS_COMM_CHANNEL_NUM; i++)
		if (chn_id & (1 << i))
			bw_array[i] += bw;
}

static void set_comm_icc_bw_handler(struct work_struct *work)
{
	struct common_node *comm_node = container_of(
				work, struct common_node, work);
	struct common_port_node *comm_port_node;
	u32 avg_bw = 0, peak_bw = 0, max_bw = 0;
	u32 normalize_peak_bw, i;
	u32 chn_hrt_bw[MMQOS_COMM_CHANNEL_NUM] = {0};
	u32 chn_srt_bw[MMQOS_COMM_CHANNEL_NUM] = {0};
	struct dev_pm_opp *opp;
	unsigned long smi_clk = 0;
	u32 volt;

	list_for_each_entry(comm_port_node, &comm_node->comm_port_list, list) {
		mutex_lock(&comm_port_node->bw_lock);
		avg_bw += comm_port_node->latest_avg_bw;
		normalize_peak_bw = (comm_port_node->latest_peak_bw
				& ~(MTK_MMQOS_MAX_BW));
		peak_bw += normalize_peak_bw;
		set_chn_bw(chn_hrt_bw, comm_port_node->channel,
			       normalize_peak_bw);
		set_chn_bw(chn_srt_bw, comm_port_node->channel,
			       comm_port_node->latest_avg_bw);
		mutex_unlock(&comm_port_node->bw_lock);
	}
	for (i = 0; i < MMQOS_COMM_CHANNEL_NUM; i++) {
		max_bw = max_t(u32, max_bw, chn_hrt_bw[i]);
		max_bw = max_t(u32, max_bw, chn_srt_bw[i]);
	}
	if (max_bw)
		smi_clk = SHIFT_ROUND(max_bw, 4) * 1000;
	if (comm_node->comm_dev) {
		opp = dev_pm_opp_find_freq_ceil(comm_node->comm_dev, &smi_clk);
		if (opp == ERR_PTR(-ERANGE))
			opp = dev_pm_opp_find_freq_floor(comm_node->comm_dev,
							 &smi_clk);
		if (!IS_ERR_OR_NULL(opp)) {
			volt = dev_pm_opp_get_voltage(opp);
			dev_pm_opp_put(opp);
			regulator_set_voltage(comm_node->comm_reg, volt,
						    comm_node->high_volt);
		}
	}
	icc_set_bw(comm_node->icc_path, avg_bw, 0);
	icc_set_bw(comm_node->icc_hrt_path, peak_bw, 0);
}

static int mtk_mmqos_set(struct icc_node *src, struct icc_node *dst)
{
	struct larb_node *larb_node;
	struct larb_port_node *larb_port_node;
	struct common_port_node *comm_port_node;
	struct common_node *comm_node;
	struct mtk_mmqos *mmqos = container_of(dst->provider,
					struct mtk_mmqos, prov);
	u32 value = 1;

	switch (dst->id >> 16) {
	case MTK_MMQOS_NODE_COMMON:
		comm_node = (struct common_node *)dst->data;
		queue_work(mmqos->wq, &comm_node->work);
		break;
	case MTK_MMQOS_NODE_COMMON_PORT:
		comm_port_node = (struct common_port_node *)dst->data;
		mutex_lock(&comm_port_node->bw_lock);
		comm_port_node->latest_mix_bw = comm_port_node->base->mix_bw;
		comm_port_node->latest_peak_bw = dst->peak_bw;
		comm_port_node->latest_avg_bw = dst->avg_bw;
		mmqos_update_comm_bw(comm_port_node->larb_dev,
			MASK_16(dst->id), comm_port_node->common->freq,
			icc_to_MBps(comm_port_node->latest_mix_bw),
			icc_to_MBps(comm_port_node->latest_peak_bw),
			mmqos->qos_bound);
		mutex_unlock(&comm_port_node->bw_lock);
		break;
	case MTK_MMQOS_NODE_LARB:
		larb_port_node = (struct larb_port_node *)src->data;
		larb_node = (struct larb_node *)dst->data;
		if (larb_port_node->base->mix_bw)
			value = SHIFT_ROUND(
				icc_to_MBps(larb_port_node->base->mix_bw),
				larb_port_node->bw_ratio);
		if (value > mmqos->max_ratio)
			value = mmqos->max_ratio;
		mtk_smi_larb_bw_set(
			larb_node->larb_dev,
			MASK_16(src->id), value);
		break;
	default:
		break;
	}
	return 0;
}

static int mtk_mmqos_aggregate(struct icc_node *node,
	u32 tag, u32 avg_bw, u32 peak_bw, u32 *agg_avg,
	u32 *agg_peak)
{
	struct mmqos_base_node *base_node = NULL;
	u32 mix_bw = peak_bw;

	switch (node->id >> 16) {
	case MTK_MMQOS_NODE_LARB_PORT:
		base_node = ((struct larb_node *)node->data)->base;
		if (peak_bw)
			mix_bw = SHIFT_ROUND(peak_bw * 3, 1);
		break;
	case MTK_MMQOS_NODE_COMMON_PORT:
		base_node = ((struct common_port_node *)node->data)->base;
		break;
	default:
		return 0;
	}
	if (base_node) {
		if (*agg_avg == 0 && *agg_peak == 0)
			base_node->mix_bw = 0;
		base_node->mix_bw += peak_bw ? mix_bw : avg_bw;
	}
	*agg_avg += avg_bw;
	if (peak_bw == MTK_MMQOS_MAX_BW)
		*agg_peak |= MTK_MMQOS_MAX_BW;
	else
		*agg_peak += peak_bw;
	return 0;
}

static struct icc_node *mtk_mmqos_xlate(
	struct of_phandle_args *spec, void *data)
{
	struct icc_onecell_data *icc_data;
	s32 i;

	if (!spec || !data)
		return ERR_PTR(-EPROBE_DEFER);
	icc_data = (struct icc_onecell_data *)data;
	for (i = 0; i < icc_data->num_nodes; i++)
		if (icc_data->nodes[i]->id == spec->args[0])
			return icc_data->nodes[i];
	pr_notice("%s: invalid index %u\n", __func__, spec->args[0]);
	return ERR_PTR(-EINVAL);
}

int mtk_mmqos_probe(struct platform_device *pdev)
{
	struct mtk_mmqos *mmqos;
	struct of_phandle_iterator it;
	struct icc_onecell_data *data;
	struct icc_node *node, *temp;
	struct mmqos_base_node *base_node;
	struct common_node *comm_node;
	struct common_port_node *comm_port_node;
	struct larb_node *larb_node;
	struct larb_port_node *larb_port_node;
	struct mtk_iommu_data smi_imu = {};
	int i, id, num_larbs = 0, num_volts, ret;
	const struct mtk_mmqos_desc *mmqos_desc;
	const struct mtk_node_desc *node_desc;
	struct device *larb_dev;
	struct mmqos_hrt *hrt;
	struct device_node *np;
	struct platform_device *comm_pdev;

	mmqos = devm_kzalloc(&pdev->dev, sizeof(*mmqos), GFP_KERNEL);
	if (!mmqos)
		return -ENOMEM;
	mmqos->dev = &pdev->dev;
	of_for_each_phandle(
		&it, ret, pdev->dev.of_node, "mediatek,larbs", NULL, 0) {
		struct device_node *np;
		struct platform_device *larb_pdev;

		np = of_node_get(it.node);
		if (!of_device_is_available(np))
			continue;
		larb_pdev = of_find_device_by_node(np);
		if (!larb_pdev) {
			larb_pdev = of_platform_device_create(
				np, NULL, platform_bus_type.dev_root);
			if (!larb_pdev || !larb_pdev->dev.driver) {
				of_node_put(np);
				return -EPROBE_DEFER;
			}
		}
		if (of_property_read_u32(np, "mediatek,larb-id", &id))
			id = num_larbs;
		smi_imu.larb_imu[id].dev = &larb_pdev->dev;
		num_larbs += 1;
	}
	INIT_LIST_HEAD(&mmqos->comm_list);
	INIT_LIST_HEAD(&mmqos->prov.nodes);
	mmqos->prov.set = mtk_mmqos_set;
	mmqos->prov.aggregate = mtk_mmqos_aggregate;
	mmqos->prov.xlate = mtk_mmqos_xlate;
	mmqos->prov.dev = &pdev->dev;
	ret = icc_provider_add(&mmqos->prov);
	if (ret) {
		dev_notice(&pdev->dev, "icc_provider_add failed:%d\n", ret);
		return ret;
	}
	mmqos_desc = (struct mtk_mmqos_desc *)
		of_device_get_match_data(&pdev->dev);
	if (!mmqos_desc) {
		ret = -EINVAL;
		goto err;
	}
	data = devm_kzalloc(&pdev->dev,
		sizeof(*data) + mmqos_desc->num_nodes * sizeof(node),
		GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	for (i = 0; i < mmqos_desc->num_nodes; i++) {
		node_desc = &mmqos_desc->nodes[i];
		node = icc_node_create(node_desc->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}
		icc_node_add(node, &mmqos->prov);
		if (node_desc->link != MMQOS_NO_LINK) {
			ret = icc_link_create(node, node_desc->link);
			if (ret)
				goto err;
		}
		node->name = node_desc->name;
		base_node = devm_kzalloc(
			&pdev->dev, sizeof(*base_node), GFP_KERNEL);
		if (!base_node) {
			ret = -ENOMEM;
			goto err;
		}
		base_node->icc_node = node;
		switch (node->id >> 16) {
		case MTK_MMQOS_NODE_COMMON:
			comm_node = devm_kzalloc(
				&pdev->dev, sizeof(*comm_node), GFP_KERNEL);
			if (!comm_node) {
				ret = -ENOMEM;
				goto err;
			}
			INIT_WORK(&comm_node->work, set_comm_icc_bw_handler);
			comm_node->clk = devm_clk_get(&pdev->dev,
				mmqos_desc->comm_muxes[MASK_16(node->id)]);
			if (IS_ERR(comm_node->clk)) {
				dev_notice(&pdev->dev, "get clk fail:%s\n",
					mmqos_desc->comm_muxes[
						MASK_16(node->id)]);
				ret = -EINVAL;
				goto err;
			}
			comm_node->freq = clk_get_rate(comm_node->clk)/1000000;
			INIT_LIST_HEAD(&comm_node->list);
			list_add_tail(&comm_node->list, &mmqos->comm_list);
			INIT_LIST_HEAD(&comm_node->comm_port_list);
			comm_node->icc_path = of_icc_get(&pdev->dev,
				mmqos_desc->comm_icc_path_names[
						MASK_16(node->id)]);
			if (IS_ERR_OR_NULL(comm_node->icc_path)) {
				dev_notice(&pdev->dev,
					"get icc_path fail:%s\n",
					mmqos_desc->comm_icc_path_names[
						MASK_16(node->id)]);
				ret = -EINVAL;
				goto err;
			}
			comm_node->icc_hrt_path = of_icc_get(&pdev->dev,
				mmqos_desc->comm_icc_hrt_path_names[
						MASK_16(node->id)]);
			if (IS_ERR_OR_NULL(comm_node->icc_hrt_path)) {
				dev_notice(&pdev->dev,
					"get icc_hrt_path fail:%s\n",
					mmqos_desc->comm_icc_hrt_path_names[
						MASK_16(node->id)]);
				ret = -EINVAL;
				goto err;
			}
			np = of_parse_phandle(pdev->dev.of_node,
					      "mediatek,commons",
					      MASK_16(node->id));
			if (!of_device_is_available(np)) {
				pr_notice("get common(%d) dev fail\n",
					  MASK_16(node->id));
				break;
			}
			comm_pdev = of_find_device_by_node(np);
			if (comm_pdev)
				comm_node->comm_dev = &comm_pdev->dev;
			else
				pr_notice("comm(%d) pdev is null\n",
					  MASK_16(node->id));
			comm_node->comm_reg =
				devm_regulator_get(comm_node->comm_dev,
						   "dvfsrc-vcore");
			if (!comm_node->comm_reg) {
				pr_notice("get common(%d) reg fail\n",
				  MASK_16(node->id));
				break;
			}
			num_volts =
				regulator_count_voltages(comm_node->comm_reg);
			if (num_volts <= 0) {
				pr_notice("get common(%d) num voltages fail\n",
					  MASK_16(node->id));
				break;
			}
			comm_node->high_volt =
				regulator_list_voltage(comm_node->comm_reg,
						       num_volts-1);
			if (comm_node->high_volt <= 0) {
				pr_notice("get common(%d) high volt fail\n",
					  MASK_16(node->id));
				break;
			}
			comm_node->base = base_node;
			node->data = (void *)comm_node;
			break;
		case MTK_MMQOS_NODE_COMMON_PORT:
			comm_port_node = devm_kzalloc(&pdev->dev,
				sizeof(*comm_port_node), GFP_KERNEL);
			if (!comm_port_node) {
				ret = -ENOMEM;
				goto err;
			}
			comm_port_node->channel =
				mmqos_desc->comm_port_channels[
				MASK_16((node->id >> 8))][MASK_16(node->id)];
			mutex_init(&comm_port_node->bw_lock);
			comm_port_node->common = node->links[0]->data;
			INIT_LIST_HEAD(&comm_port_node->list);
			list_add_tail(&comm_port_node->list,
				      &comm_port_node->common->comm_port_list);
			comm_port_node->base = base_node;
			node->data = (void *)comm_port_node;
			break;
		case MTK_MMQOS_NODE_LARB:
			larb_node = devm_kzalloc(
				&pdev->dev, sizeof(*larb_node), GFP_KERNEL);
			if (!larb_node) {
				ret = -ENOMEM;
				goto err;
			}
			comm_port_node = node->links[0]->data;
			larb_dev = smi_imu.larb_imu[node->id &
					(MTK_LARB_NR_MAX-1)].dev;
			if (larb_dev) {
				comm_port_node->larb_dev = larb_dev;
				larb_node->larb_dev = larb_dev;
			}
			larb_node->base = base_node;
			node->data = (void *)larb_node;
			break;
		case MTK_MMQOS_NODE_LARB_PORT:
			larb_port_node = devm_kzalloc(&pdev->dev,
				sizeof(*larb_port_node), GFP_KERNEL);
			if (!larb_port_node) {
				ret = -ENOMEM;
				goto err;
			}
			larb_port_node->bw_ratio = node_desc->bw_ratio;
			larb_port_node->base = base_node;
			node->data = (void *)larb_port_node;
			break;
		default:
			dev_notice(&pdev->dev,
				"invalid node id:%#x\n", node->id);
			ret = -EINVAL;
			goto err;
		}
		data->nodes[i] = node;
	}
	data->num_nodes = mmqos_desc->num_nodes;
	mmqos->prov.data = data;
	mmqos->max_ratio = mmqos_desc->max_ratio;
	mmqos->wq = create_singlethread_workqueue("mmqos_work_queue");
	if (!mmqos->wq) {
		dev_notice(&pdev->dev, "work queue create fail\n");
		ret = -ENOMEM;
		goto err;
	}
	hrt = devm_kzalloc(&pdev->dev, sizeof(*hrt), GFP_KERNEL);
	if (!hrt) {
		ret = -ENOMEM;
		goto err;
	}
	memcpy(hrt, &mmqos_desc->hrt, sizeof(mmqos_desc->hrt));
	mtk_mmqos_init_hrt(hrt);
	mmqos->nb.notifier_call = update_mm_clk;
	register_mmdvfs_notifier(&mmqos->nb);
	ret = mtk_mmqos_register_hrt_sysfs(&pdev->dev);
	if (ret)
		dev_notice(&pdev->dev, "sysfs create fail\n");
	platform_set_drvdata(pdev, mmqos);
	return 0;
err:
	list_for_each_entry_safe(node, temp, &mmqos->prov.nodes, node_list) {
		icc_node_del(node);
		icc_node_destroy(node->id);
	}
	icc_provider_del(&mmqos->prov);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_probe);

int mtk_mmqos_remove(struct platform_device *pdev)
{
	struct mtk_mmqos *mmqos = platform_get_drvdata(pdev);
	struct icc_node *node, *temp;

	list_for_each_entry_safe(node, temp, &mmqos->prov.nodes, node_list) {
		icc_node_del(node);
		icc_node_destroy(node->id);
	}
	icc_provider_del(&mmqos->prov);
	unregister_mmdvfs_notifier(&mmqos->nb);
	destroy_workqueue(mmqos->wq);
	mtk_mmqos_unregister_hrt_sysfs(&pdev->dev);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_remove);
MODULE_LICENSE("GPL v2");
