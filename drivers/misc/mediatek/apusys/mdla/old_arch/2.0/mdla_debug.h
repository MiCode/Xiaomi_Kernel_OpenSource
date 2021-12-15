// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDLA_DEBUG_H__
#define __MDLA_DEBUG_H__

#define DEBUG 1

#include "mdla.h"
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/seq_file.h>
#include <aee.h>

#ifdef CONFIG_MTK_AEE_FEATURE
#define mdla_aee_warn(key, format, args...) \
	do { \
		pr_info(format, ##args); \
		aee_kernel_warning("MDLA", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)
#else
#define mdla_aee_warn(key, format, args...)
#endif

extern int g_vpu_log_level;
extern unsigned int g_mdla_func_mask;
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
 * mdla_dump_mesg - dump the log buffer, which is wroted by MDLA
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

/**
 * mdla_dump_memory - dump the mdla code code buffer
 * @s:		the pointer to seq_file.
 */
int mdla_dump_mdla_memory(struct seq_file *s);

int mdla_dump_dbg(struct mdla_dev *mdla_info, struct command_entry *ce);

void mdla_dump_cmd_buf_free(unsigned int core_id);

int mdla_create_dmp_cmd_buf(struct command_entry *ce,
	struct mdla_dev *mdla_info);

enum MDLA_DEBUG_MASK {
	MDLA_DBG_DRV = 0x01,
	MDLA_DBG_MEM = 0x02,
	MDLA_DBG_CMD = 0x04,
	MDLA_DBG_PMU = 0x08,
	MDLA_DBG_PERF = 0x10,
	MDLA_DBG_QOS = 0x20,
	MDLA_DBG_TIMEOUT = 0x40,
	MDLA_DBG_DVFS = 0x80,
	MDLA_DBG_TIMEOUT_ALL = 0x100,
};

#if 1
extern u32 mdla_klog;
#ifdef __APUSYS_MDLA_UT__
#define mdla_debug(mask, ...) do { if (mdla_klog & mask) \
		pr_info(__VA_ARGS__); \
	} while (0)
#else
#define mdla_debug(mask, ...) do { if (mdla_klog & mask) \
		pr_debug(__VA_ARGS__); \
	} while (0)
#endif
void mdla_dump_ce(struct command_entry *ce);
void mdla_dump_buf(int mask, void *kva, int group, u32 size);
void mdla_debugfs_init(void);
void mdla_debugfs_exit(void);
#else
#define mdla_debug(mask, ...)
static inline void mdla_dump_reg(int core_id)
{
}
static inline
void mdla_dump_ce(struct command_entry *ce)
{
}
static inline
void mdla_dump_buf(int mask, void *kva, int group, u32 size)
{
}
static inline void mdla_debugfs_init(void)
{
}
static inline void mdla_debugfs_exit(void)
{
}
#endif

#define mdla_drv_debug(...) mdla_debug(MDLA_DBG_DRV, __VA_ARGS__)
#define mdla_mem_debug(...) mdla_debug(MDLA_DBG_MEM, __VA_ARGS__)
#define mdla_cmd_debug(...) mdla_debug(MDLA_DBG_CMD, __VA_ARGS__)
#define mdla_pmu_debug(...) mdla_debug(MDLA_DBG_PMU, __VA_ARGS__)
#define mdla_perf_debug(...) mdla_debug(MDLA_DBG_PERF, __VA_ARGS__)
#define mdla_qos_debug(...) mdla_debug(MDLA_DBG_QOS, __VA_ARGS__)
#define mdla_timeout_debug(...) mdla_debug(MDLA_DBG_TIMEOUT, __VA_ARGS__)
#define mdla_dvfs_debug(...) mdla_debug(MDLA_DBG_DVFS, __VA_ARGS__)
#define mdla_timeout_all_debug(...) \
	mdla_debug(MDLA_DBG_TIMEOUT_ALL, __VA_ARGS__)
#define dump_reg_top(core_id, name) \
	mdla_timeout_debug("%s: %d: %.8x\n", #name,\
	core_id, mdla_reg_read_with_mdlaid(core_id, name))

#define dump_reg_cfg(core_id, name) \
	mdla_timeout_debug("%s: %d: %.8x\n", #name,\
	core_id, mdla_cfg_read_with_mdlaid(core_id, name))

#endif

