/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MDLA_DEBUG_H__
#define __MDLA_DEBUG_H__

#define DEBUG 1

#include "mdla.h"
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/seq_file.h>
extern int g_vpu_log_level;
extern unsigned int g_mdla_func_mask;
extern void *apu_mdla_cmde_mreg_top;
extern void *apu_mdla_config_top;
extern void *apu_conn_top;
/* LOG & AEE */
#define MDLA_TAG "[mdla]"
/*#define VPU_DEBUG*/
#ifdef VPU_DEBUG
#define LOG_DBG(format, args...)    pr_debug(MDLA_TAG " " format, ##args)
#else
#define LOG_DBG(format, args...)
#endif
#define LOG_INF(format, args...)    pr_info(MDLA_TAG " " format, ##args)
#define LOG_WRN(format, args...)    pr_info(MDLA_TAG "[warn] " format, ##args)
#define LOG_ERR(format, args...)    pr_info(MDLA_TAG "[error] " format, ##args)

enum MdlaFuncMask {
	VFM_NEED_WAIT_VCORE		= 0x1,
	VFM_ROUTINE_PRT_SYSLOG = 0x2
};

enum MdlaLogThre {
	/* >1, performance break down check */
	MdlaLogThre_PERFORMANCE    = 1,

	/* >2, algo info, opp info check */
	Log_ALGO_OPP_INFO  = 2,

	/* >3, state machine check, while wait vcore/do running */
	Log_STATE_MACHINE  = 3,

	/* >4, dump buffer mva */
	MdlaLogThre_DUMP_BUF_MVA   = 4
};
enum MDLA_power_param {
	MDLA_POWER_PARAM_FIX_OPP,
	MDLA_POWER_PARAM_DVFS_DEBUG,
	MDLA_POWER_PARAM_JTAG,
	MDLA_POWER_PARAM_LOCK,
	MDLA_POWER_PARAM_VOLT_STEP,
	MDLA_POWER_HAL_CTL,
	MDLA_EARA_CTL,
};
#define mdla_print_seq(seq_file, format, args...) \
	{ \
		if (seq_file) \
			seq_printf(seq_file, format, ##args); \
		else \
			LOG_ERR(format, ##args); \
	}

/**
 * mdla_dump_register - dump the register table, and show the content
 *                     of all fields.
 * @s:		the pointer to seq_file.
 */
int mdla_dump_register(struct seq_file *s);

/**
 * mdla_dump_image_file - dump the binary information stored in flash storage.
 * @s:          the pointer to seq_file.
 */
int mdla_dump_image_file(struct seq_file *s);

/**
 * mdla_dump_mesg - dump the log buffer, which is wroted by VPU
 * @s:          the pointer to seq_file.
 */
int mdla_dump_mesg(struct seq_file *s);

/**
 * mdla_dump_mdla - dump the mdla status
 * @s:          the pointer to seq_file.
 */
int mdla_dump_mdla(struct seq_file *s);

/**
 * mdla_dump_device_dbg - dump the remaining user information to debug fd leak
 * @s:          the pointer to seq_file.
 */
int mdla_dump_device_dbg(struct seq_file *s);


enum MDLA_DEBUG_MASK {
	MDLA_DBG_DRV = 0x01,
	MDLA_DBG_MEM = 0x02,
	MDLA_DBG_CMD = 0x04,
	MDLA_DBG_PMU = 0x08,
	MDLA_DBG_PERF = 0x10,
	MDLA_DBG_QOS = 0x20,
	MDLA_DBG_TIMEOUT = 0x40,
	MDLA_DBG_DVFS = 0x80,
};

extern u32 mdla_klog;
#define mdla_debug(mask, ...) do { if (mdla_klog & mask) \
		pr_debug(__VA_ARGS__); \
	} while (0)
void mdla_dump_reg(void);
void mdla_dump_ce(struct command_entry *ce);
void mdla_dump_buf(int mask, void *kva, int group, u32 size);
void mdla_debugfs_init(void);
void mdla_debugfs_exit(void);

#define mdla_drv_debug(...) mdla_debug(MDLA_DBG_DRV, __VA_ARGS__)
#define mdla_mem_debug(...) mdla_debug(MDLA_DBG_MEM, __VA_ARGS__)
#define mdla_cmd_debug(...) mdla_debug(MDLA_DBG_CMD, __VA_ARGS__)
#define mdla_pmu_debug(...) mdla_debug(MDLA_DBG_PMU, __VA_ARGS__)
#define mdla_perf_debug(...) mdla_debug(MDLA_DBG_PERF, __VA_ARGS__)
#define mdla_qos_debug(...) mdla_debug(MDLA_DBG_QOS, __VA_ARGS__)
#define mdla_timeout_debug(...) mdla_debug(MDLA_DBG_TIMEOUT, __VA_ARGS__)
#define mdla_dvfs_debug(...) mdla_debug(MDLA_DBG_DVFS, __VA_ARGS__)

#endif

