/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

struct dvfsrc_opp {
	u32 vcore_opp;
	u32 dram_opp;
	u32 vcore_uv;
	u32 dram_khz;
};

struct mtk_dvfsrc;
struct dvfsrc_debug_data {
	const int *regs;
	u32 num_opp;
	const struct dvfsrc_opp **opps;
	u32 ip_verion;
	char *(*dump_info)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_reg)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_record)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
};

struct mtk_dvfsrc {
	struct device *dev;
	struct icc_path *path;
	struct regulator *vcore_power;
	struct regulator *dvfsrc_vcore_power;
	struct regulator *dvfsrc_vscp_power;
	int dram_type;
	int num_perf;
	int *perfs;
	void __iomem *regs;
	const struct dvfsrc_debug_data *dvd;
};

extern int mt6779_dvfsrc_register_sysfs(struct device *dev);
extern void mt6779_dvfsrc_unregister_sysfs(struct device *dev);

