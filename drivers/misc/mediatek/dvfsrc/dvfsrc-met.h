/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DVFSRC_MET_H
#define __DVFSRC_MET_H
#include <dvfsrc-exp.h>

struct mtk_dvfsrc_met;

struct dvfsrc_met_config {
	u32 ip_verion;
	int (*dvfsrc_get_src_req_num)(void);
	char **(*dvfsrc_get_src_req_name)(void);
	unsigned int *(*dvfsrc_get_src_req)(struct mtk_dvfsrc_met *dvfs);
	int (*dvfsrc_get_ddr_ratio)(struct mtk_dvfsrc_met *dvfs);
	u32 (*get_current_level)(struct mtk_dvfsrc_met *dvfsrc);
};

struct dvfsrc_met_data {
	const struct dvfsrc_met_config *met;
	u32 version;
};

struct mtk_dvfsrc_met {
	struct device *dev;
	void __iomem *regs;
	const struct dvfsrc_met_data *dvd;
	struct regulator *dvfsrc_vcore_power;
	struct icc_path *bw_path;
	struct icc_path *hrt_path;
};

extern const struct dvfsrc_met_config mt6873_met_config;
extern const struct dvfsrc_met_config mt6983_met_config;
extern const struct dvfsrc_met_config mt6768_met_config;
extern const struct dvfsrc_met_config mt6765_met_config;

#endif

