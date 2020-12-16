/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUPWR_TAG_H__
#define __APUPWR_TAG_H__

#include "apusys_core.h"
/* Tags: Count of Tags */
#define APUPWR_TAGS_CNT (2000)

#define LOG_STR_LEN (64)

struct apupwr_tag_pwr {
	unsigned int vvpu;
	unsigned int vmdla;
	unsigned int vcore;
	unsigned int vsram;
	unsigned int dsp_freq;	// dsp conn
	unsigned int dsp1_freq; // vpu core0
	unsigned int dsp2_freq; // vpu core1
	unsigned int dsp3_freq; // vpu core2
	unsigned int dsp5_freq; // mdla 0 for 68x3
	unsigned int dsp6_freq; // mdla core0 & core1 for 688x
	unsigned int dsp7_freq; // iommu
	unsigned int ipuif_freq;	// ipu interface
};

struct apupwr_tag_rpc {
	unsigned int spm_wakeup;
	unsigned int rpc_intf_rdy;
	unsigned int vcore_cg_stat;
	unsigned int conn_cg_stat;
	unsigned int vpu0_cg_stat;
	unsigned int vpu1_cg_stat;
	unsigned int vpu2_cg_stat;
	unsigned int mdla0_cg_stat;
	unsigned int mdla1_cg_stat;
};

struct apupwr_tag_dvfs {
	char *gov_name;
	const char *p_name;
	const char *c_name;
	u32 opp;
	ulong freq;
};

/* The tag entry of APUPWR */
struct apupwr_tag {
	int type;

	struct apupwr_tag_pwr pwr;
	struct apupwr_tag_rpc rpc;
	struct apupwr_tag_dvfs dvfs;
};

int apupwr_init_drv_tags(struct apusys_core_info *info);
void apupwr_exit_drv_tags(void);
void apupwr_tags_show(struct seq_file *s);

#endif

