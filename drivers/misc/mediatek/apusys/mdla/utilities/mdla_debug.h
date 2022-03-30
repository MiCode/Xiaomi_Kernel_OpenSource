/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_DEBUG_H__
#define __MDLA_DEBUG_H__

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/proc_fs.h>

#include <common/mdla_device.h>

extern u8 cfg_apusys_trace;

/* MDLA reset reason */
enum REASON_MDLA_RETVAL_ENUM {
	REASON_OTHERS    = 0,
	REASON_DRVINIT   = 1,
	REASON_TIMEOUT   = 2,
	REASON_POWERON   = 3,
	REASON_PREEMPTED = 4,
	REASON_SIMULATOR = 5,
	REASON_MAX,
};

enum FUNC_FOOT_PRINT {
	/*
	 * F_ENQUEUE: for enqueue_ce, val = (F_ENQUEUE|resume)
	 *      0x10: first add in list
	 *      0x11: add in list because preemption occur
	 */
	F_ENQUEUE               = 0x10,
	F_DEQUEUE               = 0x20,
	/*
	 * F_ISSUE:
	 *    0x30: Success
	 *    0x31: Issue cdma1 fail
	 *    0x32: Issue cdma2 fail
	 *    0x33: Issue cmd fail because HW queue is not empty and in irq
	 *    0x34: Issue cmd fail because HW queue is not empty and not in irq
	 */
	F_ISSUE                 = 0x30,
	F_CMDDONE_CE_PASS       = 0x40,
	F_CMDDONE_GO_TO_ISSUE   = 0x41,
	F_CMDDONE_CE_FIN3ERROR  = 0x42,

	/*
	 * F_STOP: for stop interrupt, val = (F_STOP|priority)
	 *   0x50: record low priority ce
	 *   0x51: record high priority ce
	 */
	F_STOP                  = 0x50,

	/*
	 * F_TIMEOUT:
	 *      0x60: cmd timeout and enter irq
	 *      0x61: others timeout and enter irq
	 */
	F_TIMEOUT               = 0x60,
	F_INIRQ_PASS            = 0x70,
	F_INIRQ_ERROR           = 0x71,
	F_INIRQ_CDMA4ERROR      = 0x72,
	F_INIRQ_STOP            = 0x73,

	/*
	 * F_SET_STOP: for set stop bit function
	 *   0x81: record single ce in MDLA HW
	 *   0x82: record dual ce in MDLA HW
	 *   0x89: record new ce
	 */
	F_SET_STOP_SINGLE       = 0x81,
	F_SET_STOP_DUAL         = 0x82,
	F_SET_STOP_NEW_CE       = 0x89,
};

enum MDLA_DEBUG_FS_NODE_U64 {
	FS_CFG_PMU_PERIOD,

	NF_MDLA_DEBUG_FS_U64
};

enum MDLA_DEBUG_FS_NODE_U32 {
	FS_C1,
	FS_C2,
	FS_C3,
	FS_C4,
	FS_C5,
	FS_C6,
	FS_C7,
	FS_C8,
	FS_C9,
	FS_C10,
	FS_C11,
	FS_C12,
	FS_C13,
	FS_C14,
	FS_C15,
	FS_CFG_ENG0,
	FS_CFG_ENG1,
	FS_CFG_ENG2,
	FS_CFG_ENG11,
	FS_POLLING_CMD_DONE,
	FS_DUMP_CMDBUF,
	FS_DVFS_RAND,
	FS_PMU_EVT_BY_APU,
	FS_KLOG,
	FS_POWEROFF_TIME,
	FS_TIMEOUT,
	FS_TIMEOUT_DBG,
	FS_BATCH_NUM,
	FS_PREEMPTION_TIMES,
	FS_PREEMPTION_DBG,

	NF_MDLA_DEBUG_FS_U32
};

struct mdla_dbg_cb_func {
	void (*destroy_dump_cmdbuf)(struct mdla_dev *mdla_device);
	int  (*create_dump_cmdbuf)(struct mdla_dev *mdla_device,
			struct command_entry *ce);
	void (*dump_reg)(u32 core_id, struct seq_file *s);
	void (*memory_show)(struct seq_file *s);

	bool (*dbgfs_u64_enable)(int node);
	bool (*dbgfs_u32_enable)(int node);
	void (*dbgfs_plat_init)(struct device *dev, struct dentry *parent);
};

struct mdla_dbg_cb_func *mdla_dbg_plat_cb(void);
void mdla_dbg_set_version(u32 ver);

u64 mdla_dbg_read_u64(int node);
u32 mdla_dbg_read_u32(int node);

void mdla_dbg_write_u64(int node, u64 val);
void mdla_dbg_write_u32(int node, u32 val);

void mdla_dbg_sub_u64(int node, u64 val);
void mdla_dbg_sub_u32(int node, u32 val);

void mdla_dbg_add_u64(int node, u64 val);
void mdla_dbg_add_u32(int node, u32 val);

enum MDLA_DEBUG_MASK {
	MDLA_DBG_DRV         = (1U << 0),
	MDLA_DBG_MEM         = (1U << 1),
	MDLA_DBG_CMD         = (1U << 2),
	MDLA_DBG_PMU         = (1U << 3),
	MDLA_DBG_PERF        = (1U << 4),
	MDLA_DBG_PWR         = (1U << 5),
	MDLA_DBG_TIMEOUT     = (1U << 6),
	MDLA_DBG_RSV         = (1U << 7),

	MDLA_DBG_ALL         = (1U << 8) - 1,
};

void mdla_dbg_show_klog_info(struct seq_file *s, char *prefix);

#define ce_func_trace(ce, val) (ce)->footprint = ((ce)->footprint << 8) | (val & 0xFF)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#define mdla_aee_warn(key, format, args...) \
	do { \
		pr_info(format, ##args); \
		aee_kernel_warning("MDLA", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)
#else
#define mdla_aee_warn(key, format, args...)
#endif


#define redirect_output(...) pr_info(__VA_ARGS__)

/* log level : error */
#define mdla_err(...) redirect_output(__VA_ARGS__)

/* log level : debug */
#define mdla_debug(mask, ...)				\
do {							\
	if ((mask & mdla_dbg_read_u32(FS_KLOG)))	\
		redirect_output(__VA_ARGS__);		\
} while (0)

#define mdla_drv_debug(...) mdla_debug(MDLA_DBG_DRV, __VA_ARGS__)
#define mdla_mem_debug(...) mdla_debug(MDLA_DBG_MEM, __VA_ARGS__)
#define mdla_cmd_debug(...) mdla_debug(MDLA_DBG_CMD, __VA_ARGS__)
#define mdla_pmu_debug(...) mdla_debug(MDLA_DBG_PMU, __VA_ARGS__)
#define mdla_perf_debug(...) mdla_debug(MDLA_DBG_PERF, __VA_ARGS__)
#define mdla_pwr_debug(...) mdla_debug(MDLA_DBG_PWR, __VA_ARGS__)
#define mdla_timeout_debug(...) mdla_debug(MDLA_DBG_TIMEOUT, __VA_ARGS__)

/* log level : verbose */
#define mdla_verbose(...)				\
do {							\
	if (mdla_dbg_read_u32(FS_KLOG) == MDLA_DBG_ALL)	\
		redirect_output(__VA_ARGS__);		\
} while (0)

const char *mdla_dbg_get_reason_str(int res);

/* debugfs node name : used to r/w 32/64-bit value */
const char *mdla_dbg_get_u64_node_str(int node);
const char *mdla_dbg_get_u32_node_str(int node);

/* debugfs node name : used to show information */
#define DBGFS_HW_REG_NAME   "register"
#define DBGFS_CMDBUF_NAME   "mdla_memory"
#define PROCFS_CMDBUF_NAME  DBGFS_CMDBUF_NAME

void mdla_dbg_dump(struct mdla_dev *mdla_info, struct command_entry *ce);
void mdla_dbg_ce_info(u32 core_id, struct command_entry *ce);

struct dentry *mdla_dbg_get_fs_root(void);
struct proc_dir_entry *mdla_dbg_get_procfs_dir(void);

void mdla_dbg_fs_setup(struct device *dev);
void mdla_dbg_fs_init(struct dentry *apusys_dbg_root);
void mdla_dbg_fs_exit(void);

#endif /* __MDLA_DEBUG_H__ */
