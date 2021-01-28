/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DVFSRC_H
#define __DVFSRC_H

struct mtk_dvfsrc_up;
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

struct dvfsrc_qos_config {
	const int *ipi_pin;
	int (*qos_dvfsrc_init)(struct mtk_dvfsrc_up *dvfs);
};


struct dvfsrc_up_data {
	bool fb_act_enable;
	u32 num_opp_desc;
	struct dvfsrc_opp_desc *opps_desc;
	const struct dvfsrc_qos_config *qos;
	void (*setup_opp_table)(struct mtk_dvfsrc_up *dvfsrc);
};

struct mtk_dvfsrc_up {
	struct device *dev;
	int opp_type;
	int fw_type;
	struct dvfsrc_opp_desc *opp_desc;
	const struct dvfsrc_up_data *dvd;
};

extern const struct dvfsrc_qos_config mt6761_qos_config;
extern const struct dvfsrc_qos_config mt6779_qos_config;

#endif

