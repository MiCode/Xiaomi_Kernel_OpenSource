/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _ISP_DVFS_H
#define _ISP_DVFS_H
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/remoteproc.h>
#include <linux/workqueue.h>

#define MAX_MUX_NUM (10)
#define MAX_OPP_STEP 7
#define DEFAULT_VOLTAGE 650000
#define FINE_GRAIN_LOWER_BOUND 650000
#define DEFAULT_VOLTAGE_LEVEL 3
#define WAIT_POWER_ON_OFF_TIMEOUT_MS 2000

enum dvfs_apmcu_task_id {
	DVFS_CCU_NO_NEED_CB = -1,
	DVFS_CCU_INIT = 0,
	DVFS_VOLTAGE_UPDATE = 1,
	DVFS_CCU_DVFS_RESET = 2,
	DVFS_CCU_QUERY_VB = 3,
};

enum dvfs_dbg_id {
	DVFS_DEBUG_DEFAULT = 0,
	DVFS_DEBUG_LOG = 1,
	DVFS_DEBUG_MICROP = 2,
};

struct dvfs_clk_data {
	const char *mux_name;
	struct clk *mux;
	struct clk *clk_src[MAX_OPP_STEP];
};

struct dvfs_table {
	unsigned long frequency[MAX_OPP_STEP];
	int voltage[MAX_OPP_STEP];
	int opp_num;
};

struct dvfs_info {
	struct mutex voltage_mutex;
	int voltage_target;
	u32 opp_level;
};

struct ccu_handle_info {
	phandle handle;
	struct platform_device *ccu_pdev;
};

struct dvfs_driver_data {
	struct device *dev;
	u32 num_muxes;
	bool mux_is_enable;
	struct dvfs_clk_data muxes[MAX_MUX_NUM];
	struct dvfs_table opp_table;
	struct dvfs_info current_dvfs;
	u32 simulate_aging;
	u32 en_vb;
	u32 disable_dvfs;
	struct ccu_handle_info ccu_handle;
	atomic_t ccu_power_on;
};

struct dvfs_ipc_init {
	u32 needVoltageBin;
	u32 needSimAging;
	u32 needCbFromMicroP;
};

struct dvfs_ipc_info {
	u32 maxOppIdx;
	u32 minOppIdx;
	u32 curTickCnt;
};

struct dvfs_ipc_vb {
	u32 efuseValue;
	int voltage[MAX_OPP_STEP];
};

#endif
