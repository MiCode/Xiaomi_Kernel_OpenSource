// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include <mtk-interconnect.h>
#include <soc/mediatek/mmqos.h>
#include "mtk_cam-dvfs_qos.h"

struct dvfs_stream_info {
	unsigned int freq_hz;
};

static int mtk_cam_build_freq_table(struct device *dev,
				    struct camsys_opp_table *tbl,
				    int size)
{
	int count = dev_pm_opp_get_opp_count(dev);
	struct dev_pm_opp *opp;
	int i, index = 0;
	unsigned long freq = 1;

	if (WARN(count > size,
		 "opp table is being truncated\n"))
		count = size;

	for (i = 0; i < count; i++) {
		opp = dev_pm_opp_find_freq_ceil(dev, &freq);
		if (IS_ERR(opp))
			break;

		tbl[index].freq_hz = freq;
		tbl[index].volt_uv = dev_pm_opp_get_voltage(opp);

		dev_pm_opp_put(opp);
		index++;
		freq++;
	}

	return index;
}

int mtk_cam_dvfs_probe(struct device *dev,
		       struct mtk_camsys_dvfs *dvfs, int max_stream_num)
{
	const char *mux_name, *clksrc_name;
	struct property *clksrc_prop;
	u32 num_clksrc = 0;
	int ret;
	int i;

	dvfs->dev = dev;

	if (max_stream_num <= 0) {
		dev_info(dev, "invalid stream num %d\n", max_stream_num);
		return -1;
	}

	dvfs->max_stream_num = max_stream_num;
	dvfs->stream_infos = devm_kcalloc(dev, max_stream_num,
					  sizeof(*dvfs->stream_infos),
					  GFP_KERNEL);
	if (!dvfs->stream_infos)
		return -ENOMEM;

	ret = dev_pm_opp_of_add_table(dev);
	if (ret < 0) {
		dev_info(dev, "fail to init opp table: %d\n", ret);
		goto opp_default_table;
	}

	dvfs->reg_vmm = devm_regulator_get_optional(dev, "dvfs-vmm");
	if (IS_ERR(dvfs->reg_vmm)) {
		dev_info(dev, "can't get dvfsrc-vcore\n");
		goto opp_default_table;
	}

	dvfs->opp_num = mtk_cam_build_freq_table(dev, dvfs->opp,
						 ARRAY_SIZE(dvfs->opp));

	for (i = 0; i < dvfs->opp_num; i++)
		dev_info(dev, "[%s] idx=%d, clk=%d volt=%d\n", __func__,
			 i, dvfs->opp[i].freq_hz, dvfs->opp[i].volt_uv);

	/* Get CLK handles */
	ret = of_property_read_string(dev->of_node, "mux_name", &mux_name);
	if (ret < 0) {
		dev_info(dev, "has no mux_name\n");
		return -EINVAL;
	}

	dev_info(dev, "mux name(%s)\n", mux_name);
	dvfs->mux = devm_clk_get(dev, mux_name);

	/* Get CLK source */
	of_property_for_each_string(
		dev->of_node, "clk_src", clksrc_prop, clksrc_name) {
		if (WARN_ON(num_clksrc >= ARRAY_SIZE(dvfs->clk_src))) {
			dev_info(dev, "clk_src array is being truncated\n");
			break;
		}

		dev_info(dev, "clksrc name(%s)\n", clksrc_name);
		dvfs->clk_src[num_clksrc] = devm_clk_get(dev, clksrc_name);
		num_clksrc++;
	}

	spin_lock_init(&dvfs->lock);

	return 0;

opp_default_table:
	dvfs->opp_num = 1;
	dvfs->opp[0] = (struct camsys_opp_table) {
		.freq_hz = 546000000,
		.volt_uv = 650000,
	};
	return 0;
}

int mtk_cam_dvfs_remove(struct mtk_camsys_dvfs *dvfs)
{
	dev_pm_opp_of_remove_table(dvfs->dev);
	return 0;
}

static void reset_runtime_info(struct mtk_camsys_dvfs *dvfs)
{
	dvfs->cur_opp_idx = -1;

	memset(dvfs->stream_infos, 0,
	       dvfs->max_stream_num * sizeof(*dvfs->stream_infos));
}

int mtk_cam_dvfs_regulator_enable(struct mtk_camsys_dvfs *dvfs)
{
	if (!dvfs->reg_vmm) {
		dev_info(dvfs->dev, "reg_vmm is not ready\n");
		return 0;
	}

	if (regulator_enable(dvfs->reg_vmm)) {
		dev_info(dvfs->dev, "regulator_enable fail\n");
		return -1;
	}

	reset_runtime_info(dvfs);
	return 0;
}

int mtk_cam_dvfs_regulator_disable(struct mtk_camsys_dvfs *dvfs)
{
	if (dvfs->reg_vmm && regulator_is_enabled(dvfs->reg_vmm))
		regulator_disable(dvfs->reg_vmm);

	return 0;
}

static int mtk_cam_dvfs_get_clkidx(struct mtk_camsys_dvfs *dvfs,
				   unsigned int freq)
{
	int i;

	for (i = 0; i < dvfs->opp_num; i++)
		if (freq == dvfs->opp[i].freq_hz)
			break;

	if (i == dvfs->opp_num) {
		dev_info(dvfs->dev, "failed to find idx for freq %u\n", freq);
		return -1;
	}
	return i;
}

static int set_clk_src(struct mtk_camsys_dvfs *dvfs, int opp_idx)
{
	struct clk *mux, *clk_src;
	int ret;

	if (opp_idx >= ARRAY_SIZE(dvfs->opp) || opp_idx < 0) {
		dev_info(dvfs->dev, "opp level(%d) is out of bound\n", opp_idx);
		return -EINVAL;
	}

	mux = dvfs->mux;
	clk_src = dvfs->clk_src[opp_idx];

	ret = clk_prepare_enable(mux);
	if (ret) {
		dev_info(dvfs->dev, "prepare cam mux fail:%d opp_idx:%d\n",
			 ret, opp_idx);
		return ret;
	}

	ret = clk_set_parent(mux, clk_src);
	if (ret)
		dev_info(dvfs->dev, "set cam mux parent fail:%d opp_idx:%d\n",
			 ret, opp_idx);
	clk_disable_unprepare(mux);
	return ret;
}

static bool to_update_stream_opp_idx(struct mtk_camsys_dvfs *dvfs,
				     int stream_id, unsigned int freq,
				     unsigned int *max_freq)
{
	int i;
	unsigned int fmax = 0;
	bool change;

	spin_lock(&dvfs->lock);

	change = !(dvfs->stream_infos[stream_id].freq_hz == freq);

	dvfs->stream_infos[stream_id].freq_hz = freq;
	for (i = 0; i < dvfs->max_stream_num; i++)
		fmax = max(fmax, dvfs->stream_infos[i].freq_hz);

	spin_unlock(&dvfs->lock);

	if (max_freq)
		*max_freq = fmax;

	return change;
}

static int mtk_cam_dvfs_update_opp(struct mtk_camsys_dvfs *dvfs,
				   int opp_idx)
{
	int current_volt, target_volt;
	int ret;

	if (!dvfs->reg_vmm || !regulator_is_enabled(dvfs->reg_vmm)) {
		dev_info(dvfs->dev, "%s: regulator is not ready\n", __func__);
		return -1;
	}

	current_volt = regulator_get_voltage(dvfs->reg_vmm);
	target_volt = dvfs->opp[opp_idx].volt_uv;

	if (target_volt < current_volt) {
		ret = set_clk_src(dvfs, opp_idx);
		if (ret)
			return -1;

		ret = regulator_set_voltage(dvfs->reg_vmm, target_volt, INT_MAX);
		if (ret) {
			dev_info(dvfs->dev, "[%s] set voltage fail: %d->%d\n",
				 __func__, current_volt, target_volt);
		}

	} else {
		ret = regulator_set_voltage(dvfs->reg_vmm, target_volt, INT_MAX);
		if (ret) {
			dev_info(dvfs->dev, "[%s] set voltage fail: %d->%d\n",
				 __func__, current_volt, target_volt);
			return -1;
		}

		ret = set_clk_src(dvfs, opp_idx);
	}

	return 0;
}

/* TODO:
 *  1. temporal adjustment
 */

int mtk_cam_dvfs_update(struct mtk_camsys_dvfs *dvfs,
			int stream_id, unsigned int target_freq_hz)
{
	int opp_idx = -1;
	int max_freq = 0;
	int ret;

	if (WARN_ON(stream_id < 0 || stream_id >= dvfs->max_stream_num))
		return -1;

	/* check if freq is changed */
	if (!to_update_stream_opp_idx(dvfs,
				     stream_id, target_freq_hz,
				     &max_freq))
		return 0;

	opp_idx = mtk_cam_dvfs_get_clkidx(dvfs, max_freq);
	if (opp_idx < 0)
		return -1;

	if (opp_idx == dvfs->cur_opp_idx)
		return 0;

	ret = mtk_cam_dvfs_update_opp(dvfs, opp_idx);
	if (ret)
		return ret;

	dev_info(dvfs->dev,
		 "dvfs_update: stream %d freq %u/max %u, opp_idx: %d->%d\n",
		 stream_id, target_freq_hz, max_freq,
		 dvfs->cur_opp_idx, opp_idx);

	dvfs->cur_opp_idx = opp_idx;

	return 0;
}

struct mtk_camsys_qos_path {
	int id;
	struct icc_path *path;
};

int mtk_cam_qos_probe(struct device *dev,
		      struct mtk_camsys_qos *qos,
		      int *ids, int n_id)
{
	const char *names[32];
	struct mtk_camsys_qos_path *cam_path;
	int i;
	int n;
	int ret = 0;

	n = of_property_count_strings(dev->of_node, "interconnect-names");
	if (n <= 0) {
		dev_info(dev, "skip without interconnect-names\n");
		return 0;
	}

	qos->n_path = n;
	qos->cam_path = devm_kcalloc(dev, qos->n_path,
				     sizeof(*qos->cam_path), GFP_KERNEL);
	if (!qos->cam_path)
		return -ENOMEM;

	//dev_info(dev, "icc_path num %d\n", qos->n_path);
	if (qos->n_path > ARRAY_SIZE(names)) {
		dev_info(dev, "%s: array size of names is not enough.\n");
		return -EINVAL;
	}

	if (qos->n_path != n_id) {
		dev_info(dev, "icc num(%d) mismatch with ids num(%d)\n",
			 qos->n_path, n_id);
		return -EINVAL;
	}

	of_property_read_string_array(dev->of_node, "interconnect-names",
				      names, qos->n_path);

	for (i = 0, cam_path = qos->cam_path; i < qos->n_path; i++, cam_path++) {
		//dev_info(dev, "interconnect: idx %d [%s id = %d]\n",
		//	 i, names[i], ids[i]);

		cam_path->id = ids[i];
		cam_path->path = of_mtk_icc_get(dev, names[i]);
		if (IS_ERR_OR_NULL(cam_path->path)) {
			dev_info(dev, "failed to get icc of %s\n",
				 names[i]);
			ret = -EINVAL;
		}
	}

	return ret;
}

int mtk_cam_qos_remove(struct mtk_camsys_qos *qos)
{
	int i;

	if (!qos->cam_path)
		return 0;

	for (i = 0; i < qos->n_path; i++)
		icc_put(qos->cam_path[i].path);

	return 0;
}

static struct mtk_camsys_qos_path *find_qos_path(struct mtk_camsys_qos *qos,
						 int id)
{
	struct mtk_camsys_qos_path *cam_path;
	int i;

	for (i = 0, cam_path = qos->cam_path; i < qos->n_path; i++, cam_path++)
		if (cam_path->id == id)
			return cam_path;
	return NULL;
}

int mtk_cam_qos_update(struct mtk_camsys_qos *qos,
		       int path_id, u32 avg_bw, u32 peak_bw)
{
	struct mtk_camsys_qos_path *cam_path;

	cam_path = find_qos_path(qos, path_id);
	if (!cam_path)
		return -1;

	/* TODO
	 * 1. detect change
	 * 2. temporal adjustment
	 */
	return mtk_icc_set_bw(cam_path->path, avg_bw, peak_bw);
}

int mtk_cam_qos_reset_all(struct mtk_camsys_qos *qos)
{
	struct mtk_camsys_qos_path *cam_path;
	int i;

	for (i = 0, cam_path = qos->cam_path; i < qos->n_path; i++, cam_path++)
		mtk_icc_set_bw(cam_path->path, 0, 0);
	return 0;
}

int mtk_cam_qos_wait_throttle_done(void)
{
	mtk_mmqos_wait_throttle_done();
	return 0;
}
