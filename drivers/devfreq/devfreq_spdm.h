/*
*Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
*
*This program is free software; you can redistribute it and/or modify
*it under the terms of the GNU General Public License version 2 and
*only version 2 as published by the Free Software Foundation.
*
*This program is distributed in the hope that it will be useful,
*but WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*GNU General Public License for more details.
*/

#ifndef DEVFREQ_SPDM_H
#define DEVFREQ_SPDM_H

#include <linux/list.h>
#include <soc/qcom/hvc.h>
#include <soc/qcom/scm.h>

enum pl_levels { SPDM_PL1, SPDM_PL2, SPDM_PL3, SPDM_PL_COUNT };
enum actions { SPDM_UP, SPDM_DOWN };
enum spdm_client { SPDM_CLIENT_CPU, SPDM_CLIENT_GPU, SPDM_CLIENT_COUNT };

struct spdm_config_data {
	/* in MB/s */
	u32 upstep;
	u32 downstep;
	u32 up_step_multp;

	u32 num_ports;
	u32 *ports;
	u32 aup;
	u32 adown;
	u32 bucket_size;

	/*
	 * If We define n PL levels we need n-1 frequencies to tell
	 * where to change from one pl to another
	 */
	/* hz */
	u32 pl_freqs[SPDM_PL_COUNT - 1];
	/*
	 * We have a low threshold and a high threhold for each pl to support
	 * the two port solution so we need twice as many entries as
	 * performance levels
	 */
	/* in 100th's of a percent */
	u32 reject_rate[SPDM_PL_COUNT * 2];
	u32 response_time_us[SPDM_PL_COUNT * 2];
	u32 cci_response_time_us[SPDM_PL_COUNT * 2];
	/* hz */
	u32 max_cci_freq;
	/* in MB/s */
	u32 max_vote;

};

struct spdm_data {
	/* bus scaling data */
	int cur_idx;
	struct msm_bus_scale_pdata *pdata;
	u32 bus_scale_client_id;
	/* in mb/s */
	u32 new_bw;

	/* devfreq data */
	struct devfreq *devfreq;
	struct devfreq_dev_profile *profile;
	unsigned long action;
	int window;
	struct clk *cci_clk;

	/* spdm hw/gov data */
	struct spdm_config_data config_data;

	enum spdm_client spdm_client;
	/* list used by governor to keep track of spdm devices */
	struct list_head list;

	struct dentry *debugfs_dir;

	bool enabled;
};

extern void spdm_init_debugfs(struct device *dev);
extern void spdm_remove_debugfs(struct spdm_data *data);

#define SPDM_HYP_FNID 5
#define SPDM_SCM_SVC_ID 0x9
#define SPDM_SCM_CMD_ID 0x4
/* SPDM CMD ID's for hypervisor/SCM */
#define SPDM_CMD_GET_BW_ALL 1
#define SPDM_CMD_GET_BW_SPECIFIC 2
#define SPDM_CMD_ENABLE 3
#define SPDM_CMD_DISABLE 4
#define SPDM_CMD_CFG_PORTS 5
#define SPDM_CMD_CFG_FLTR 6
#define SPDM_CMD_CFG_PL 7
#define SPDM_CMD_CFG_REJRATE_LOW 8
#define SPDM_CMD_CFG_REJRATE_MED 9
#define SPDM_CMD_CFG_REJRATE_HIGH 10
#define SPDM_CMD_CFG_RESPTIME_LOW 11
#define SPDM_CMD_CFG_RESPTIME_MED 12
#define SPDM_CMD_CFG_RESPTIME_HIGH 13
#define SPDM_CMD_CFG_CCIRESPTIME_LOW 14
#define SPDM_CMD_CFG_CCIRESPTIME_MED 15
#define SPDM_CMD_CFG_CCIRESPTIME_HIGH 16
#define SPDM_CMD_CFG_MAXCCI 17
#define SPDM_CMD_CFG_VOTES 18

#define SPDM_MAX_ARGS 6
#define SPDM_MAX_RETS 3

struct spdm_args {
	u64 arg[SPDM_MAX_ARGS];
	u64 ret[SPDM_MAX_RETS];
};

extern int __spdm_hyp_call(struct spdm_args *args, int num_args);
extern int __spdm_scm_call(struct spdm_args *args, int num_args);

#ifdef CONFIG_SPDM_SCM
#define spdm_ext_call __spdm_scm_call
#else
#define spdm_ext_call __spdm_hyp_call
#endif
#endif
