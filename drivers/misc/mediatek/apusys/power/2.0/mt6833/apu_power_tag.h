// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUPWR_TAG_H__
#define __APUPWR_TAG_H__

#include "apusys_power_cust.h"

/* Tags: Count of Tags */
#define APUPWR_TAGS_CNT (2000)

#define LOG_STR_LEN (64)


/* The tag entry of APUPWR */
struct apupwr_tag {
	int type;

	union apupwr_tag_data {
		struct apupwr_tag_pwr {
			unsigned int vvpu;
			unsigned int vmdla;
			unsigned int vcore;
			unsigned int vsram;
			unsigned int dsp_freq;	// dsp conn
			unsigned int dsp1_freq;	// vpu core0
			unsigned int dsp2_freq;	// vpu core1
			unsigned int dsp5_freq;	// mdla core0
			unsigned int ipuif_freq;	// ipu interface
			unsigned long long id;
		} pwr;
		struct apupwr_tag_rpc {
			unsigned int spm_wakeup;
			unsigned int rpc_intf_rdy;
			unsigned int vcore_cg_stat;
			unsigned int conn_cg_stat;
			unsigned int vpu0_cg_stat;
			unsigned int vpu1_cg_stat;
			unsigned int mdla0_cg_stat;
		} rpc;
		struct apupwr_tag_dvfs {
			char log_str[LOG_STR_LEN];
		} dvfs;
	} d;
};

#ifdef APUPWR_TAG_TP
int apupwr_init_drv_tags(void);
void apupwr_exit_drv_tags(void);
void apupwr_tags_show(struct seq_file *s);
#else
static inline int apupwr_init_drv_tags(void)
{
	return 0;
}

static inline void apupwr_exit_drv_tags(void)
{
}

static inline void apupwr_tags_show(struct seq_file *s)
{
}
#endif

#endif

