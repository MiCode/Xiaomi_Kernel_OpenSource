/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef SDE_DBG_H_
#define SDE_DBG_H_

#include <stdarg.h>
#include <linux/debugfs.h>
#include <linux/list.h>

/* select an uncommon hex value for the limiter */
#define SDE_EVTLOG_DATA_LIMITER	(0xC0DEBEEF)
#define SDE_EVTLOG_FUNC_ENTRY	0x1111
#define SDE_EVTLOG_FUNC_EXIT	0x2222
#define SDE_EVTLOG_FUNC_CASE1	0x3333
#define SDE_EVTLOG_FUNC_CASE2	0x4444
#define SDE_EVTLOG_FUNC_CASE3	0x5555
#define SDE_EVTLOG_FUNC_CASE4	0x6666
#define SDE_EVTLOG_FUNC_CASE5	0x7777
#define SDE_EVTLOG_FUNC_CASE6	0x8888
#define SDE_EVTLOG_FUNC_CASE7	0x9999
#define SDE_EVTLOG_FUNC_CASE8	0xaaaa
#define SDE_EVTLOG_FUNC_CASE9	0xbbbb
#define SDE_EVTLOG_FUNC_CASE10	0xcccc
#define SDE_EVTLOG_PANIC	0xdead
#define SDE_EVTLOG_FATAL	0xbad
#define SDE_EVTLOG_ERROR	0xebad

#define SDE_DBG_DUMP_DATA_LIMITER (NULL)

enum sde_dbg_evtlog_flag {
	SDE_EVTLOG_CRITICAL = BIT(0),
	SDE_EVTLOG_IRQ = BIT(1),
	SDE_EVTLOG_VERBOSE = BIT(2),
	SDE_EVTLOG_ALWAYS = -1
};

enum sde_dbg_dump_flag {
	SDE_DBG_DUMP_IN_LOG = BIT(0),
	SDE_DBG_DUMP_IN_MEM = BIT(1),
};

enum sde_dbg_dump_context {
	SDE_DBG_DUMP_PROC_CTX,
	SDE_DBG_DUMP_IRQ_CTX,
	SDE_DBG_DUMP_CLK_ENABLED_CTX,
};

#ifdef CONFIG_DRM_SDE_EVTLOG_DEBUG
#define SDE_EVTLOG_DEFAULT_ENABLE (SDE_EVTLOG_CRITICAL | SDE_EVTLOG_IRQ)
#else
#define SDE_EVTLOG_DEFAULT_ENABLE 0
#endif

/*
 * evtlog will print this number of entries when it is called through
 * sysfs node or panic. This prevents kernel log from evtlog message
 * flood.
 */
#define SDE_EVTLOG_PRINT_ENTRY	256

/*
 * evtlog keeps this number of entries in memory for debug purpose. This
 * number must be greater than print entry to prevent out of bound evtlog
 * entry array access.
 */
#define SDE_EVTLOG_ENTRY	(SDE_EVTLOG_PRINT_ENTRY * 8)
#define SDE_EVTLOG_MAX_DATA 15
#define SDE_EVTLOG_BUF_MAX 512
#define SDE_EVTLOG_BUF_ALIGN 32

struct sde_dbg_power_ctrl {
	void *handle;
	void *client;
	int (*enable_fn)(void *handle, void *client, bool enable);
};

struct sde_dbg_evtlog_log {
	s64 time;
	const char *name;
	int line;
	u32 data[SDE_EVTLOG_MAX_DATA];
	u32 data_cnt;
	int pid;
};

/**
 * @last_dump: Index of last entry to be output during evtlog dumps
 * @filter_list: Linked list of currently active filter strings
 */
struct sde_dbg_evtlog {
	struct sde_dbg_evtlog_log logs[SDE_EVTLOG_ENTRY];
	u32 first;
	u32 last;
	u32 last_dump;
	u32 curr;
	u32 next;
	u32 enable;
	spinlock_t spin_lock;
	struct list_head filter_list;
};

extern struct sde_dbg_evtlog *sde_dbg_base_evtlog;

/**
 * SDE_EVT32 - Write a list of 32bit values to the event log, default area
 * ... - variable arguments
 */
#define SDE_EVT32(...) sde_evtlog_log(sde_dbg_base_evtlog, __func__, \
		__LINE__, SDE_EVTLOG_ALWAYS, ##__VA_ARGS__, \
		SDE_EVTLOG_DATA_LIMITER)

/**
 * SDE_EVT32_VERBOSE - Write a list of 32bit values for verbose event logging
 * ... - variable arguments
 */
#define SDE_EVT32_VERBOSE(...) sde_evtlog_log(sde_dbg_base_evtlog, __func__, \
		__LINE__, SDE_EVTLOG_VERBOSE, ##__VA_ARGS__, \
		SDE_EVTLOG_DATA_LIMITER)

/**
 * SDE_EVT32_IRQ - Write a list of 32bit values to the event log, IRQ area
 * ... - variable arguments
 */
#define SDE_EVT32_IRQ(...) sde_evtlog_log(sde_dbg_base_evtlog, __func__, \
		__LINE__, SDE_EVTLOG_IRQ, ##__VA_ARGS__, \
		SDE_EVTLOG_DATA_LIMITER)

/**
 * SDE_DBG_DUMP - trigger dumping of all sde_dbg facilities
 * @va_args:	list of named register dump ranges and regions to dump, as
 *		registered previously through sde_dbg_reg_register_base and
 *		sde_dbg_reg_register_dump_range.
 *		Including the special name "panic" will trigger a panic after
 *		the dumping work has completed.
 */
#define SDE_DBG_DUMP(...) sde_dbg_dump(SDE_DBG_DUMP_PROC_CTX, __func__, \
		##__VA_ARGS__, SDE_DBG_DUMP_DATA_LIMITER)

/**
 * SDE_DBG_DUMP_WQ - trigger dumping of all sde_dbg facilities, queuing the work
 * @va_args:	list of named register dump ranges and regions to dump, as
 *		registered previously through sde_dbg_reg_register_base and
 *		sde_dbg_reg_register_dump_range.
 *		Including the special name "panic" will trigger a panic after
 *		the dumping work has completed.
 */
#define SDE_DBG_DUMP_WQ(...) sde_dbg_dump(SDE_DBG_DUMP_IRQ_CTX, __func__, \
		##__VA_ARGS__, SDE_DBG_DUMP_DATA_LIMITER)

/**
 * SDE_DBG_DUMP_CLK_EN - trigger dumping of all sde_dbg facilities, without clk
 * @va_args:	list of named register dump ranges and regions to dump, as
 *		registered previously through sde_dbg_reg_register_base and
 *		sde_dbg_reg_register_dump_range.
 *		Including the special name "panic" will trigger a panic after
 *		the dumping work has completed.
 */
#define SDE_DBG_DUMP_CLK_EN(...) sde_dbg_dump(SDE_DBG_DUMP_CLK_ENABLED_CTX, \
		__func__, ##__VA_ARGS__, SDE_DBG_DUMP_DATA_LIMITER)

/**
 * SDE_DBG_EVT_CTRL - trigger a different driver events
 *  event: event that trigger different behavior in the driver
 */
#define SDE_DBG_CTRL(...) sde_dbg_ctrl(__func__, ##__VA_ARGS__, \
		SDE_DBG_DUMP_DATA_LIMITER)

#if defined(CONFIG_DEBUG_FS)

/**
 * sde_evtlog_init - allocate a new event log object
 * Returns:	evtlog or -ERROR
 */
struct sde_dbg_evtlog *sde_evtlog_init(void);

/**
 * sde_evtlog_destroy - destroy previously allocated event log
 * @evtlog:	pointer to evtlog
 * Returns:	none
 */
void sde_evtlog_destroy(struct sde_dbg_evtlog *evtlog);

/**
 * sde_evtlog_log - log an entry into the event log.
 *	log collection may be enabled/disabled entirely via debugfs
 *	log area collection may be filtered by user provided flags via debugfs.
 * @evtlog:	pointer to evtlog
 * @name:	function name of call site
 * @line:	line number of call site
 * @flag:	log area filter flag checked against user's debugfs request
 * Returns:	none
 */
void sde_evtlog_log(struct sde_dbg_evtlog *evtlog, const char *name, int line,
		int flag, ...);

/**
 * sde_evtlog_dump_all - print all entries in event log to kernel log
 * @evtlog:	pointer to evtlog
 * Returns:	none
 */
void sde_evtlog_dump_all(struct sde_dbg_evtlog *evtlog);

/**
 * sde_evtlog_is_enabled - check whether log collection is enabled for given
 *	event log and log area flag
 * @evtlog:	pointer to evtlog
 * @flag:	log area filter flag
 * Returns:	none
 */
bool sde_evtlog_is_enabled(struct sde_dbg_evtlog *evtlog, u32 flag);

/**
 * sde_evtlog_dump_to_buffer - print content of event log to the given buffer
 * @evtlog:		pointer to evtlog
 * @evtlog_buf:		target buffer to print into
 * @evtlog_buf_size:	size of target buffer
 * @update_last_entry:	whether or not to stop at most recent entry
 * @full_dump:          whether to dump full or to limit print entries
 * Returns:		number of bytes written to buffer
 */
ssize_t sde_evtlog_dump_to_buffer(struct sde_dbg_evtlog *evtlog,
		char *evtlog_buf, ssize_t evtlog_buf_size,
		bool update_last_entry, bool full_dump);

/**
 * sde_dbg_init_dbg_buses - initialize debug bus dumping support for the chipset
 * @hwversion:		Chipset revision
 */
void sde_dbg_init_dbg_buses(u32 hwversion);

/**
 * sde_dbg_init - initialize global sde debug facilities: evtlog, regdump
 * @dev:		device handle
 * @power_ctrl:		power control callback structure for enabling clocks
 *			during register dumping
 * Returns:		0 or -ERROR
 */
int sde_dbg_init(struct device *dev, struct sde_dbg_power_ctrl *power_ctrl);

/**
 * sde_dbg_debugfs_register - register entries at the given debugfs dir
 * @debugfs_root:	debugfs root in which to create sde debug entries
 * Returns:	0 or -ERROR
 */
int sde_dbg_debugfs_register(struct dentry *debugfs_root);

/**
 * sde_dbg_destroy - destroy the global sde debug facilities
 * Returns:	none
 */
void sde_dbg_destroy(void);

/**
 * sde_dbg_dump - trigger dumping of all sde_dbg facilities
 * @queue_work:	whether to queue the dumping work to the work_struct
 * @name:	string indicating origin of dump
 * @va_args:	list of named register dump ranges and regions to dump, as
 *		registered previously through sde_dbg_reg_register_base and
 *		sde_dbg_reg_register_dump_range.
 *		Including the special name "panic" will trigger a panic after
 *		the dumping work has completed.
 * Returns:	none
 */
void sde_dbg_dump(enum sde_dbg_dump_context mode, const char *name, ...);

/**
 * sde_dbg_ctrl - trigger specific actions for the driver with debugging
 *		purposes. Those actions need to be enabled by the debugfs entry
 *		so the driver executes those actions in the corresponding calls.
 * @va_args:	list of actions to trigger
 * Returns:	none
 */
void sde_dbg_ctrl(const char *name, ...);

/**
 * sde_dbg_reg_register_base - register a hw register address section for later
 *	dumping. call this before calling sde_dbg_reg_register_dump_range
 *	to be able to specify sub-ranges within the base hw range.
 * @name:	name of base region
 * @base:	base pointer of region
 * @max_offset:	length of region
 * Returns:	0 or -ERROR
 */
int sde_dbg_reg_register_base(const char *name, void __iomem *base,
		size_t max_offset);

/**
 * sde_dbg_reg_register_cb - register a hw register callback for later
 *	dumping.
 * @name:	name of base region
 * @cb:		callback of external region
 * @cb_ptr:	private pointer of external region
 * Returns:	0 or -ERROR
 */
int sde_dbg_reg_register_cb(const char *name, void (*cb)(void *), void *ptr);

/**
 * sde_dbg_reg_unregister_cb - register a hw unregister callback for later
 *	dumping.
 * @name:	name of base region
 * @cb:		callback of external region
 * @cb_ptr:	private pointer of external region
 * Returns:	None
 */
void sde_dbg_reg_unregister_cb(const char *name, void (*cb)(void *), void *ptr);

/**
 * sde_dbg_reg_register_dump_range - register a hw register sub-region for
 *	later register dumping associated with base specified by
 *	sde_dbg_reg_register_base
 * @base_name:		name of base region
 * @range_name:		name of sub-range within base region
 * @offset_start:	sub-range's start offset from base's base pointer
 * @offset_end:		sub-range's end offset from base's base pointer
 * @xin_id:		xin id
 * Returns:		none
 */
void sde_dbg_reg_register_dump_range(const char *base_name,
		const char *range_name, u32 offset_start, u32 offset_end,
		uint32_t xin_id);

/**
 * sde_dbg_set_sde_top_offset - set the target specific offset from mdss base
 *	address of the top registers. Used for accessing debug bus controls.
 * @blk_off: offset from mdss base of the top block
 */
void sde_dbg_set_sde_top_offset(u32 blk_off);

/**
 * sde_evtlog_set_filter - update evtlog filtering
 * @evtlog:	pointer to evtlog
 * @filter:     pointer to optional function name filter, set to NULL to disable
 */
void sde_evtlog_set_filter(struct sde_dbg_evtlog *evtlog, char *filter);

/**
 * sde_evtlog_get_filter - query configured evtlog filters
 * @evtlog:	pointer to evtlog
 * @index:	filter index to retrieve
 * @buf:	pointer to output filter buffer
 * @bufsz:	size of output filter buffer
 * Returns:	zero if a filter string was returned
 */
int sde_evtlog_get_filter(struct sde_dbg_evtlog *evtlog, int index,
		char *buf, size_t bufsz);

/**
 * sde_rsc_debug_dump - sde rsc debug dump status
 * @mux_sel:	select mux on rsc debug bus
 */
void sde_rsc_debug_dump(u32 mux_sel);

/**
 * dsi_ctrl_debug_dump - dump dsi debug dump status
 * @entries:	array of debug bus control values
 * @size:	size of the debug bus control array
 */
void dsi_ctrl_debug_dump(u32 *entries, u32 size);

#else
static inline struct sde_dbg_evtlog *sde_evtlog_init(void)
{
	return NULL;
}

static inline void sde_evtlog_destroy(struct sde_dbg_evtlog *evtlog)
{
}

static inline void sde_evtlog_log(struct sde_dbg_evtlog *evtlog,
		const char *name, int line, int flag, ...)
{
}

static inline void sde_evtlog_dump_all(struct sde_dbg_evtlog *evtlog)
{
}

static inline bool sde_evtlog_is_enabled(struct sde_dbg_evtlog *evtlog,
		u32 flag)
{
	return false;
}

static inline ssize_t sde_evtlog_dump_to_buffer(struct sde_dbg_evtlog *evtlog,
		char *evtlog_buf, ssize_t evtlog_buf_size,
		bool update_last_entry)
{
	return 0;
}

static inline void sde_dbg_init_dbg_buses(u32 hwversion)
{
}

static inline int sde_dbg_init(struct device *dev,
		struct sde_dbg_power_ctrl *power_ctrl)
{
	return 0;
}

static inline int sde_dbg_debugfs_register(struct dentry *debugfs_root)
{
	return 0;
}

static inline void sde_dbg_destroy(void)
{
}

static inline void sde_dbg_dump(enum sde_dbg_dump_context,
	const char *name, ...)
{
}

static inline void sde_dbg_ctrl(const char *name, ...)
{
}

static inline int sde_dbg_reg_register_base(const char *name,
		void __iomem *base, size_t max_offset)
{
	return 0;
}

static inline void sde_dbg_reg_register_dump_range(const char *base_name,
		const char *range_name, u32 offset_start, u32 offset_end,
		uint32_t xin_id)
{
}

void sde_dbg_set_sde_top_offset(u32 blk_off)
{
}

static inline void sde_evtlog_set_filter(
		struct sde_dbg_evtlog *evtlog, char *filter)
{
}

static inline int sde_evtlog_get_filter(struct sde_dbg_evtlog *evtlog,
		int index, char *buf, size_t bufsz)
{
	return -EINVAL;
}

static inline void sde_rsc_debug_dump(u32 mux_sel)
{
}

static inline void dsi_ctrl_debug_dump(u32 entries, u32 size)
{
}

#endif /* defined(CONFIG_DEBUG_FS) */


#endif /* SDE_DBG_H_ */
