/* SPDX-License-Identifier: GPL-2.0 */
//
// Unisoc ddr dvfs driver
//
// Copyright (C) 2022 Unisoc, Inc.
// Author: Mingmin Ling <mingmin.ling@unisoc.com

#ifndef __SPRD_DVFS_DRV_H__
#define __SPRD_DVFS_DRV_H__

#include <linux/platform_device.h>
#include "../governor.h"

struct dvfs_hw_callback {
	int (*hw_dvfs_vote)(const char *name);
	int (*hw_dvfs_unvote)(const char *name);
	int (*hw_dvfs_set_point)(const char *name, unsigned int freq);
	int (*hw_dvfs_get_point_info)(char **name, unsigned int *freq,
				      unsigned int *flag, int index);
	int (*dvfs_freq_request)(unsigned int freq);
};

enum DDR_DFS_STATE_STEP {
	SCENARIO_DFS_ENTER = 1,
	EXIT_SCENE,
	AUTO_DFS_ON_OFF,
	SCALING_FORCE_DDR_FREQ,
	SCENE_BOOST_ENTER,
	SET_BACKDOOR,
	DFS_ON_OFF,
	CHANGE_POINT,
	SCENE_FREQ_SET,
	GET_OVERFLOW_T,
	SET_OVERFLOW_T,
	GET_UNDERFLOW_T,
	SET_UNDERFLOW_T,
	GET_DVFS_STATUS_T,
	GET_DVFS_AUTO_STATUS_T,
	GET_DVFS_FORCE_FREQ_T,
	GET_CUR_FREQ_T,
	GET_FREQ_TABLE_T,
	SEND_FREQ_REQUEST_T,
};

#define PARSE_FLOW_ERR 0x5a5a5a5a
#define DDR_DVFS_INIT_DONE 1
#define DDR_DB_NODE_NUM 32
#define INFO_LEN_MAX 128
#define DDR_DUMP_BUFFER (DDR_DB_NODE_NUM * INFO_LEN_MAX)
#define SCENE_MAX 25
#define COMM_MAX 25
#define DEFAULT_VOL 750
struct DDR_DFS_STEP_T {
	enum DDR_DFS_STATE_STEP step;
	int status;
	u32 buff;
	char scene[SCENE_MAX];
	char comm[COMM_MAX];
	int pid;
	ktime_t time;
};

struct ddr_dfs_step_list_t {
	struct ddr_dfs_step_list_t *next;
	struct DDR_DFS_STEP_T data;
};

struct governor_callback {
	int (*governor_vote)(const char *name);
	int (*governor_unvote)(const char *name);
	int (*governor_change_point)(const char *name, unsigned int freq);
	int (*get_point_info)(char **name, unsigned int *freq, unsigned int *flag, int index);
	int (*get_freq_num)(unsigned int *data);
	int (*get_overflow)(unsigned int *data, unsigned int sel);
	int (*set_overflow)(unsigned int value, unsigned int sel);
	int (*get_underflow)(unsigned int *data, unsigned int sel);
	int (*set_underflow)(unsigned int value, unsigned int sel);
	int (*get_dvfs_status)(unsigned int *data);
	int (*dvfs_enable)(void);
	int (*dvfs_disable)(void);
	int (*get_dvfs_auto_status)(unsigned int *data);
	int (*dvfs_auto_enable)(void);
	int (*dvfs_auto_disable)(void);
	int (*dvfs_perf_mode_enable)(bool en);
	int (*get_cur_freq)(unsigned int *data);
	int (*get_freq_table)(unsigned long *data, unsigned int sel);
	int (*ddrinfo_dfs_step_parse)(char **arg, char **step_status, char **scene, u32 *buff,
				      int *pid, char **comm, ktime_t *time, u32 i);
	void (*ddr_dfs_step_add)(enum DDR_DFS_STATE_STEP cur_step, int status,
				 char *scene, u32 buff, int pid, char *comm, ktime_t time);
	int (*get_request_freq)(unsigned int *data);
	int (*send_freq_request)(unsigned int freq);
	int (*get_force_freq)(unsigned int *data);
};

/*functions supportd by dvfs core to specific drivers*/
int dvfs_core_init(struct platform_device *pdev);
int dvfs_core_clear(struct platform_device *pdev);
void dvfs_core_hw_callback_register(struct dvfs_hw_callback *hw_callback);
void dvfs_core_hw_callback_clear(struct dvfs_hw_callback *hw_callback);
unsigned long get_min_freq(void);
unsigned long get_max_freq(void);
int send_vote_request(unsigned int freq);

/*EXPORT_SYMBOLs supoorted by governor for other kernel drivers*/
int scene_dfs_request(char *scenario);
int scene_exit(char *scenario);
int change_scene_freq(char *scenario, unsigned int freq);

#if IS_ENABLED(CONFIG_DEVFREQ_GOV_SPRD_VOTE)
extern struct devfreq_governor sprd_vote;
#endif

static inline int sprd_dvfs_add_governor(void)
{
#if IS_ENABLED(CONFIG_DEVFREQ_GOV_SPRD_VOTE)
	return devfreq_add_governor(&sprd_vote);
#endif
}

static inline void sprd_dvfs_del_governor(void)
{
#if IS_ENABLED(CONFIG_DEVFREQ_GOV_SPRD_VOTE)
	devfreq_remove_governor(&sprd_vote);
#endif
}
#endif
