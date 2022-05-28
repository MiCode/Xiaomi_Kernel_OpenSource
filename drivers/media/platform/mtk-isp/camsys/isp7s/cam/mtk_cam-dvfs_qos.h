/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_DVFS_QOS_H
#define __MTK_CAM_DVFS_QOS_H

#include <linux/clk.h>
#include <linux/interconnect.h>
#include <linux/spinlock_types.h>

struct device;
struct regulator;

#define MAX_OPP_NUM 8
struct dvfs_stream_info;
struct mtk_camsys_dvfs {
	struct device *dev;
	struct regulator *reg_vmm;

	unsigned int opp_num;
	struct camsys_opp_table {
		unsigned int freq_hz;
		unsigned int volt_uv;
	} opp[8];

	struct clk *mux;
	struct clk *clk_src[8];

	spinlock_t lock;
	int max_stream_num;
	struct dvfs_stream_info *stream_infos;
	int cur_opp_idx;
};

int mtk_cam_dvfs_probe(struct device *dev,
		       struct mtk_camsys_dvfs *dvfs, int max_stream_num);
int mtk_cam_dvfs_remove(struct mtk_camsys_dvfs *dvfs);

int mtk_cam_dvfs_regulator_enable(struct mtk_camsys_dvfs *dvfs);
int mtk_cam_dvfs_regulator_disable(struct mtk_camsys_dvfs *dvfs);

int mtk_cam_dvfs_update(struct mtk_camsys_dvfs *dvfs,
			int stream_id, unsigned int target_freq_hz);

static inline
int mtk_cam_dvfs_get_opp_table(struct mtk_camsys_dvfs *dvfs,
			       const struct camsys_opp_table **tbl)
{
	*tbl = dvfs->opp;
	return dvfs->opp_num;
}

struct mtk_camsys_qos_path;
struct mtk_camsys_qos {
	int n_path;
	struct mtk_camsys_qos_path *cam_path;
};

int mtk_cam_qos_probe(struct device *dev,
		      struct mtk_camsys_qos *qos,
		      int *ids, int n_id);
int mtk_cam_qos_remove(struct mtk_camsys_qos *qos);

static inline u32 to_qos_icc(unsigned long Bps)
{
	return kBps_to_icc(Bps / 1024);
}

int mtk_cam_qos_update(struct mtk_camsys_qos *qos,
		       int path_id, u32 avg_bw, u32 peak_bw);
int mtk_cam_qos_reset_all(struct mtk_camsys_qos *qos);

/* note: may sleep */
/* TODO: only wait if bw changes? */
int mtk_cam_qos_wait_throttle_done(void);

#endif /* __MTK_CAM_DVFS_QOS_H */
