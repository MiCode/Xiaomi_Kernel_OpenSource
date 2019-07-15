/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DVFSRC_MT6779_H
#define __DVFSRC_MT6779_H
struct dvfsrc_opp {
	u32 vcore_opp;
	u32 dram_opp;
	u32 vcore_uv;
	u32 dram_khz;
};

struct dvfsrc_opp_desc {
	int num_vcore_opp;
	int num_dram_opp;
	int num_opp;
	struct dvfsrc_opp *opps;
};

struct mtk_dvfsrc;
struct dvfsrc_debug_data {
	const int *regs;
	u32 num_opp_desc;
	struct dvfsrc_opp_desc *opps_desc;
	u32 ip_verion;
	void (*setup_opp_table)(struct mtk_dvfsrc *dvfsrc);
	void (*force_opp)(struct mtk_dvfsrc *dvfsrc, u32 opp);
	char *(*dump_info)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_reg)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_record)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	int (*get_current_level)(struct mtk_dvfsrc *dvfsrc);
};

struct mtk_dvfsrc {
	struct device *dev;
	struct icc_path *path;
	struct regulator *vcore_power;
	struct regulator *dvfsrc_vcore_power;
	struct regulator *dvfsrc_vscp_power;
	int dram_type;
	struct dvfsrc_opp_desc *opp_desc;
	int num_perf;
	int *perfs;
	u32 force_opp_idx;
	void __iomem *regs;
	const struct dvfsrc_debug_data *dvd;
};

extern int mt6779_dvfsrc_register_sysfs(struct device *dev);
extern void mt6779_dvfsrc_unregister_sysfs(struct device *dev);
#endif

