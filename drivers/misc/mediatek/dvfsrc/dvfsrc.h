/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DVFSRC_H
#define __DVFSRC_H

#define DVFSRC_MD_RISING_DDR_REQ	1
#define DVFSRC_MD_HRT_BW			2
#define DVFSRC_HIFI_VCORE_REQ		3
#define DVFSRC_HIFI_DDR_REQ			4
#define DVFSRC_HIFI_RISING_DDR_REQ	5
#define DVFSRC_HRT_BW_DDR_REQ		6

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

struct dvfsrc_qos_config {
	const int *ipi_pin;
	int (*qos_dvfsrc_init)(struct mtk_dvfsrc *dvfs);
};

struct dvfsrc_met_config {
	u32 ip_verion;
	int (*dvfsrc_get_src_req_num)(void);
	char **(*dvfsrc_get_src_req_name)(void);
	unsigned int *(*dvfsrc_get_src_req)(struct mtk_dvfsrc *dvfs);
	int (*dvfsrc_get_ddr_ratio)(struct mtk_dvfsrc *dvfs);
};

struct dvfsrc_config {
	u32 ip_verion;
	const int *regs;
	void (*force_opp)(struct mtk_dvfsrc *dvfsrc, u32 opp);
	char *(*dump_info)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_reg)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_record)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	int (*get_current_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_current_rglevel)(struct mtk_dvfsrc *dvfsrc);
	int (*query_request)(struct mtk_dvfsrc *dvfsrc, u32 id);
};


struct dvfsrc_debug_data {
	u32 num_opp_desc;
	struct dvfsrc_opp_desc *opps_desc;
	const struct dvfsrc_met_config *met;
	const struct dvfsrc_qos_config *qos;
	const struct dvfsrc_config *config;
	void (*setup_opp_table)(struct mtk_dvfsrc *dvfsrc);
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

extern int dvfsrc_register_sysfs(struct device *dev);
extern void dvfsrc_unregister_sysfs(struct device *dev);
extern int vcorefs_get_num_opp(void);
extern int vcorefs_get_opp_info_num(void);
extern int vcorefs_get_src_req_num(void);
extern char **vcorefs_get_opp_info_name(void);
extern unsigned int *vcorefs_get_opp_info(void);
extern char **vcorefs_get_src_req_name(void);
extern unsigned int *vcorefs_get_src_req(void);

extern const struct dvfsrc_met_config mt6779_met_config;
extern const struct dvfsrc_qos_config mt6779_qos_config;
extern const struct dvfsrc_config mt6779_dvfsrc_config;
extern const struct dvfsrc_met_config mt6761_met_config;
extern const struct dvfsrc_qos_config mt6761_qos_config;
extern const struct dvfsrc_config mt6761_dvfsrc_config;
#endif

