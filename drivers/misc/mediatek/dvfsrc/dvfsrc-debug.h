/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DVFSRC_DEBUG_H
#define __DVFSRC_DEBUG_H

#include <dvfsrc-exp.h>

static struct mtk_dvfsrc *dvfsrc_drv;

struct dvfsrc_config {
	u32 ip_verion;
	const int *regs;
	const int *spm_regs;
	void (*force_opp)(struct mtk_dvfsrc *dvfsrc, u32 opp);
	char *(*dump_info)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_reg)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_record)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_spm_info)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	int (*query_request)(struct mtk_dvfsrc *dvfsrc, u32 id);
};

struct dvfsrc_debug_data {
	bool pmqos_enable;
	const struct dvfsrc_config *config;
};

struct mtk_dvfsrc {
	struct device *dev;
	struct icc_path *path;
	struct regulator *vcore_power;
	struct regulator *dvfsrc_vcore_power;
	struct regulator *dvfsrc_vscp_power;
	int num_perf;
	int *perfs;
	u32 force_opp_idx;
	void __iomem *regs;
	void __iomem *spm_regs;
	const struct dvfsrc_debug_data *dvd;
	u32 num_vopp;
	u32 *vopp_uv_tlb;
	struct notifier_block dvfsrc_vchk_notifier;
};

extern int dvfsrc_register_sysfs(struct device *dev);
extern void dvfsrc_unregister_sysfs(struct device *dev);

extern const struct dvfsrc_config mt6779_dvfsrc_config;
extern const struct dvfsrc_config mt6761_dvfsrc_config;
#endif

