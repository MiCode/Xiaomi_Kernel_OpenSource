/* Copyright (c) 2009-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/list_sort.h>

#include "sde_dbg.h"
#include "sde/sde_hw_catalog.h"

#define SDE_DBG_BASE_MAX		10

#define DEFAULT_PANIC		1
#define DEFAULT_REGDUMP		SDE_DBG_DUMP_IN_MEM
#define DEFAULT_DBGBUS_SDE	SDE_DBG_DUMP_IN_MEM
#define DEFAULT_DBGBUS_VBIFRT	SDE_DBG_DUMP_IN_MEM
#define DEFAULT_BASE_REG_CNT	0x100
#define GROUP_BYTES		4
#define ROW_BYTES		16
#define RANGE_NAME_LEN		40
#define REG_BASE_NAME_LEN	80

#define DBGBUS_FLAGS_DSPP	BIT(0)
#define DBGBUS_DSPP_STATUS	0x34C

#define DBGBUS_NAME_SDE		"sde"
#define DBGBUS_NAME_VBIF_RT	"vbif_rt"

/* offsets from sde top address for the debug buses */
#define DBGBUS_SSPP0	0x188
#define DBGBUS_SSPP1	0x298
#define DBGBUS_DSPP	0x348
#define DBGBUS_PERIPH	0x418

#define TEST_MASK(id, tp)	((id << 4) | (tp << 1) | BIT(0))

/* following offsets are with respect to MDP VBIF base for DBG BUS access */
#define MMSS_VBIF_CLKON			0x4
#define MMSS_VBIF_TEST_BUS_OUT_CTRL	0x210
#define MMSS_VBIF_TEST_BUS_OUT		0x230

/* print debug ranges in groups of 4 u32s */
#define REG_DUMP_ALIGN		16
#define DBG_CTRL_STOP_FTRACE        BIT(0)
#define DBG_CTRL_PANIC_UNDERRUN     BIT(1)
#define DBG_CTRL_MAX                BIT(2)

/**
 * struct sde_dbg_reg_offset - tracking for start and end of region
 * @start: start offset
 * @start: end offset
 */
struct sde_dbg_reg_offset {
	u32 start;
	u32 end;
};

/**
 * struct sde_dbg_reg_range - register dumping named sub-range
 * @head: head of this node
 * @reg_dump: address for the mem dump
 * @range_name: name of this range
 * @offset: offsets for range to dump
 * @xin_id: client xin id
 */
struct sde_dbg_reg_range {
	struct list_head head;
	u32 *reg_dump;
	char range_name[RANGE_NAME_LEN];
	struct sde_dbg_reg_offset offset;
	uint32_t xin_id;
};

/**
 * struct sde_dbg_reg_base - register region base.
 *	may sub-ranges: sub-ranges are used for dumping
 *	or may not have sub-ranges: dumping is base -> max_offset
 * @reg_base_head: head of this node
 * @sub_range_list: head to the list with dump ranges
 * @name: register base name
 * @base: base pointer
 * @off: cached offset of region for manual register dumping
 * @cnt: cached range of region for manual register dumping
 * @max_offset: length of region
 * @buf: buffer used for manual register dumping
 * @buf_len:  buffer length used for manual register dumping
 * @reg_dump: address for the mem dump if no ranges used
 */
struct sde_dbg_reg_base {
	struct list_head reg_base_head;
	struct list_head sub_range_list;
	char name[REG_BASE_NAME_LEN];
	void __iomem *base;
	size_t off;
	size_t cnt;
	size_t max_offset;
	char *buf;
	size_t buf_len;
	u32 *reg_dump;
};

struct sde_debug_bus_entry {
	u32 wr_addr;
	u32 block_id;
	u32 test_id;
};

struct vbif_debug_bus_entry {
	u32 disable_bus_addr;
	u32 block_bus_addr;
	u32 bit_offset;
	u32 block_cnt;
	u32 test_pnt_start;
	u32 test_pnt_cnt;
};

struct sde_dbg_debug_bus_common {
	char *name;
	u32 enable_mask;
	bool include_in_deferred_work;
	u32 flags;
	u32 entries_size;
	u32 *dumped_content;
};

struct sde_dbg_sde_debug_bus {
	struct sde_dbg_debug_bus_common cmn;
	struct sde_debug_bus_entry *entries;
	u32 top_blk_off;
};

struct sde_dbg_vbif_debug_bus {
	struct sde_dbg_debug_bus_common cmn;
	struct vbif_debug_bus_entry *entries;
};

/**
 * struct sde_dbg_base - global sde debug base structure
 * @evtlog: event log instance
 * @reg_base_list: list of register dumping regions
 * @root: base debugfs root
 * @dev: device pointer
 * @mutex: mutex to serialize access to serialze dumps, debugfs access
 * @power_ctrl: callback structure for enabling power for reading hw registers
 * @req_dump_blks: list of blocks requested for dumping
 * @panic_on_err: whether to kernel panic after triggering dump via debugfs
 * @dump_work: work struct for deferring register dump work to separate thread
 * @work_panic: panic after dump if internal user passed "panic" special region
 * @enable_reg_dump: whether to dump registers into memory, kernel log, or both
 * @dbgbus_sde: debug bus structure for the sde
 * @dbgbus_vbif_rt: debug bus structure for the realtime vbif
 * @dump_all: dump all entries in register dump
 */
static struct sde_dbg_base {
	struct sde_dbg_evtlog *evtlog;
	struct list_head reg_base_list;
	struct dentry *root;
	struct device *dev;
	struct mutex mutex;
	struct sde_dbg_power_ctrl power_ctrl;

	struct sde_dbg_reg_base *req_dump_blks[SDE_DBG_BASE_MAX];

	u32 panic_on_err;
	struct work_struct dump_work;
	bool work_panic;
	u32 enable_reg_dump;

	struct sde_dbg_sde_debug_bus dbgbus_sde;
	struct sde_dbg_vbif_debug_bus dbgbus_vbif_rt;
	bool dump_all;
	u32 debugfs_ctrl;
} sde_dbg_base;

/* sde_dbg_base_evtlog - global pointer to main sde event log for macro use */
struct sde_dbg_evtlog *sde_dbg_base_evtlog;

static struct sde_debug_bus_entry dbg_bus_sde_8998[] = {

	/* Unpack 0 sspp 0*/
	{ DBGBUS_SSPP0, 50, 2 },
	{ DBGBUS_SSPP0, 60, 2 },
	{ DBGBUS_SSPP0, 70, 2 },
	{ DBGBUS_SSPP0, 85, 2 },

	/* Upack 0 sspp 1*/
	{ DBGBUS_SSPP1, 50, 2 },
	{ DBGBUS_SSPP1, 60, 2 },
	{ DBGBUS_SSPP1, 70, 2 },
	{ DBGBUS_SSPP1, 85, 2 },

	/* scheduler */
	{ DBGBUS_DSPP, 130, 0 },
	{ DBGBUS_DSPP, 130, 1 },
	{ DBGBUS_DSPP, 130, 2 },
	{ DBGBUS_DSPP, 130, 3 },
	{ DBGBUS_DSPP, 130, 4 },
	{ DBGBUS_DSPP, 130, 5 },

	/* qseed */
	{ DBGBUS_SSPP0, 6, 0},
	{ DBGBUS_SSPP0, 6, 1},
	{ DBGBUS_SSPP0, 26, 0},
	{ DBGBUS_SSPP0, 26, 1},
	{ DBGBUS_SSPP1, 6, 0},
	{ DBGBUS_SSPP1, 6, 1},
	{ DBGBUS_SSPP1, 26, 0},
	{ DBGBUS_SSPP1, 26, 1},

	/* scale */
	{ DBGBUS_SSPP0, 16, 0},
	{ DBGBUS_SSPP0, 16, 1},
	{ DBGBUS_SSPP0, 36, 0},
	{ DBGBUS_SSPP0, 36, 1},
	{ DBGBUS_SSPP1, 16, 0},
	{ DBGBUS_SSPP1, 16, 1},
	{ DBGBUS_SSPP1, 36, 0},
	{ DBGBUS_SSPP1, 36, 1},

	/* fetch sspp0 */

	/* vig 0 */
	{ DBGBUS_SSPP0, 0, 0 },
	{ DBGBUS_SSPP0, 0, 1 },
	{ DBGBUS_SSPP0, 0, 2 },
	{ DBGBUS_SSPP0, 0, 3 },
	{ DBGBUS_SSPP0, 0, 4 },
	{ DBGBUS_SSPP0, 0, 5 },
	{ DBGBUS_SSPP0, 0, 6 },
	{ DBGBUS_SSPP0, 0, 7 },

	{ DBGBUS_SSPP0, 1, 0 },
	{ DBGBUS_SSPP0, 1, 1 },
	{ DBGBUS_SSPP0, 1, 2 },
	{ DBGBUS_SSPP0, 1, 3 },
	{ DBGBUS_SSPP0, 1, 4 },
	{ DBGBUS_SSPP0, 1, 5 },
	{ DBGBUS_SSPP0, 1, 6 },
	{ DBGBUS_SSPP0, 1, 7 },

	{ DBGBUS_SSPP0, 2, 0 },
	{ DBGBUS_SSPP0, 2, 1 },
	{ DBGBUS_SSPP0, 2, 2 },
	{ DBGBUS_SSPP0, 2, 3 },
	{ DBGBUS_SSPP0, 2, 4 },
	{ DBGBUS_SSPP0, 2, 5 },
	{ DBGBUS_SSPP0, 2, 6 },
	{ DBGBUS_SSPP0, 2, 7 },

	{ DBGBUS_SSPP0, 4, 0 },
	{ DBGBUS_SSPP0, 4, 1 },
	{ DBGBUS_SSPP0, 4, 2 },
	{ DBGBUS_SSPP0, 4, 3 },
	{ DBGBUS_SSPP0, 4, 4 },
	{ DBGBUS_SSPP0, 4, 5 },
	{ DBGBUS_SSPP0, 4, 6 },
	{ DBGBUS_SSPP0, 4, 7 },

	{ DBGBUS_SSPP0, 5, 0 },
	{ DBGBUS_SSPP0, 5, 1 },
	{ DBGBUS_SSPP0, 5, 2 },
	{ DBGBUS_SSPP0, 5, 3 },
	{ DBGBUS_SSPP0, 5, 4 },
	{ DBGBUS_SSPP0, 5, 5 },
	{ DBGBUS_SSPP0, 5, 6 },
	{ DBGBUS_SSPP0, 5, 7 },

	/* vig 2 */
	{ DBGBUS_SSPP0, 20, 0 },
	{ DBGBUS_SSPP0, 20, 1 },
	{ DBGBUS_SSPP0, 20, 2 },
	{ DBGBUS_SSPP0, 20, 3 },
	{ DBGBUS_SSPP0, 20, 4 },
	{ DBGBUS_SSPP0, 20, 5 },
	{ DBGBUS_SSPP0, 20, 6 },
	{ DBGBUS_SSPP0, 20, 7 },

	{ DBGBUS_SSPP0, 21, 0 },
	{ DBGBUS_SSPP0, 21, 1 },
	{ DBGBUS_SSPP0, 21, 2 },
	{ DBGBUS_SSPP0, 21, 3 },
	{ DBGBUS_SSPP0, 21, 4 },
	{ DBGBUS_SSPP0, 21, 5 },
	{ DBGBUS_SSPP0, 21, 6 },
	{ DBGBUS_SSPP0, 21, 7 },

	{ DBGBUS_SSPP0, 22, 0 },
	{ DBGBUS_SSPP0, 22, 1 },
	{ DBGBUS_SSPP0, 22, 2 },
	{ DBGBUS_SSPP0, 22, 3 },
	{ DBGBUS_SSPP0, 22, 4 },
	{ DBGBUS_SSPP0, 22, 5 },
	{ DBGBUS_SSPP0, 22, 6 },
	{ DBGBUS_SSPP0, 22, 7 },

	{ DBGBUS_SSPP0, 24, 0 },
	{ DBGBUS_SSPP0, 24, 1 },
	{ DBGBUS_SSPP0, 24, 2 },
	{ DBGBUS_SSPP0, 24, 3 },
	{ DBGBUS_SSPP0, 24, 4 },
	{ DBGBUS_SSPP0, 24, 5 },
	{ DBGBUS_SSPP0, 24, 6 },
	{ DBGBUS_SSPP0, 24, 7 },

	{ DBGBUS_SSPP0, 25, 0 },
	{ DBGBUS_SSPP0, 25, 1 },
	{ DBGBUS_SSPP0, 25, 2 },
	{ DBGBUS_SSPP0, 25, 3 },
	{ DBGBUS_SSPP0, 25, 4 },
	{ DBGBUS_SSPP0, 25, 5 },
	{ DBGBUS_SSPP0, 25, 6 },
	{ DBGBUS_SSPP0, 25, 7 },

	/* dma 2 */
	{ DBGBUS_SSPP0, 30, 0 },
	{ DBGBUS_SSPP0, 30, 1 },
	{ DBGBUS_SSPP0, 30, 2 },
	{ DBGBUS_SSPP0, 30, 3 },
	{ DBGBUS_SSPP0, 30, 4 },
	{ DBGBUS_SSPP0, 30, 5 },
	{ DBGBUS_SSPP0, 30, 6 },
	{ DBGBUS_SSPP0, 30, 7 },

	{ DBGBUS_SSPP0, 31, 0 },
	{ DBGBUS_SSPP0, 31, 1 },
	{ DBGBUS_SSPP0, 31, 2 },
	{ DBGBUS_SSPP0, 31, 3 },
	{ DBGBUS_SSPP0, 31, 4 },
	{ DBGBUS_SSPP0, 31, 5 },
	{ DBGBUS_SSPP0, 31, 6 },
	{ DBGBUS_SSPP0, 31, 7 },

	{ DBGBUS_SSPP0, 32, 0 },
	{ DBGBUS_SSPP0, 32, 1 },
	{ DBGBUS_SSPP0, 32, 2 },
	{ DBGBUS_SSPP0, 32, 3 },
	{ DBGBUS_SSPP0, 32, 4 },
	{ DBGBUS_SSPP0, 32, 5 },
	{ DBGBUS_SSPP0, 32, 6 },
	{ DBGBUS_SSPP0, 32, 7 },

	{ DBGBUS_SSPP0, 33, 0 },
	{ DBGBUS_SSPP0, 33, 1 },
	{ DBGBUS_SSPP0, 33, 2 },
	{ DBGBUS_SSPP0, 33, 3 },
	{ DBGBUS_SSPP0, 33, 4 },
	{ DBGBUS_SSPP0, 33, 5 },
	{ DBGBUS_SSPP0, 33, 6 },
	{ DBGBUS_SSPP0, 33, 7 },

	{ DBGBUS_SSPP0, 34, 0 },
	{ DBGBUS_SSPP0, 34, 1 },
	{ DBGBUS_SSPP0, 34, 2 },
	{ DBGBUS_SSPP0, 34, 3 },
	{ DBGBUS_SSPP0, 34, 4 },
	{ DBGBUS_SSPP0, 34, 5 },
	{ DBGBUS_SSPP0, 34, 6 },
	{ DBGBUS_SSPP0, 34, 7 },

	{ DBGBUS_SSPP0, 35, 0 },
	{ DBGBUS_SSPP0, 35, 1 },
	{ DBGBUS_SSPP0, 35, 2 },
	{ DBGBUS_SSPP0, 35, 3 },

	/* dma 0 */
	{ DBGBUS_SSPP0, 40, 0 },
	{ DBGBUS_SSPP0, 40, 1 },
	{ DBGBUS_SSPP0, 40, 2 },
	{ DBGBUS_SSPP0, 40, 3 },
	{ DBGBUS_SSPP0, 40, 4 },
	{ DBGBUS_SSPP0, 40, 5 },
	{ DBGBUS_SSPP0, 40, 6 },
	{ DBGBUS_SSPP0, 40, 7 },

	{ DBGBUS_SSPP0, 41, 0 },
	{ DBGBUS_SSPP0, 41, 1 },
	{ DBGBUS_SSPP0, 41, 2 },
	{ DBGBUS_SSPP0, 41, 3 },
	{ DBGBUS_SSPP0, 41, 4 },
	{ DBGBUS_SSPP0, 41, 5 },
	{ DBGBUS_SSPP0, 41, 6 },
	{ DBGBUS_SSPP0, 41, 7 },

	{ DBGBUS_SSPP0, 42, 0 },
	{ DBGBUS_SSPP0, 42, 1 },
	{ DBGBUS_SSPP0, 42, 2 },
	{ DBGBUS_SSPP0, 42, 3 },
	{ DBGBUS_SSPP0, 42, 4 },
	{ DBGBUS_SSPP0, 42, 5 },
	{ DBGBUS_SSPP0, 42, 6 },
	{ DBGBUS_SSPP0, 42, 7 },

	{ DBGBUS_SSPP0, 44, 0 },
	{ DBGBUS_SSPP0, 44, 1 },
	{ DBGBUS_SSPP0, 44, 2 },
	{ DBGBUS_SSPP0, 44, 3 },
	{ DBGBUS_SSPP0, 44, 4 },
	{ DBGBUS_SSPP0, 44, 5 },
	{ DBGBUS_SSPP0, 44, 6 },
	{ DBGBUS_SSPP0, 44, 7 },

	{ DBGBUS_SSPP0, 45, 0 },
	{ DBGBUS_SSPP0, 45, 1 },
	{ DBGBUS_SSPP0, 45, 2 },
	{ DBGBUS_SSPP0, 45, 3 },
	{ DBGBUS_SSPP0, 45, 4 },
	{ DBGBUS_SSPP0, 45, 5 },
	{ DBGBUS_SSPP0, 45, 6 },
	{ DBGBUS_SSPP0, 45, 7 },

	/* fetch sspp1 */
	/* vig 1 */
	{ DBGBUS_SSPP1, 0, 0 },
	{ DBGBUS_SSPP1, 0, 1 },
	{ DBGBUS_SSPP1, 0, 2 },
	{ DBGBUS_SSPP1, 0, 3 },
	{ DBGBUS_SSPP1, 0, 4 },
	{ DBGBUS_SSPP1, 0, 5 },
	{ DBGBUS_SSPP1, 0, 6 },
	{ DBGBUS_SSPP1, 0, 7 },

	{ DBGBUS_SSPP1, 1, 0 },
	{ DBGBUS_SSPP1, 1, 1 },
	{ DBGBUS_SSPP1, 1, 2 },
	{ DBGBUS_SSPP1, 1, 3 },
	{ DBGBUS_SSPP1, 1, 4 },
	{ DBGBUS_SSPP1, 1, 5 },
	{ DBGBUS_SSPP1, 1, 6 },
	{ DBGBUS_SSPP1, 1, 7 },

	{ DBGBUS_SSPP1, 2, 0 },
	{ DBGBUS_SSPP1, 2, 1 },
	{ DBGBUS_SSPP1, 2, 2 },
	{ DBGBUS_SSPP1, 2, 3 },
	{ DBGBUS_SSPP1, 2, 4 },
	{ DBGBUS_SSPP1, 2, 5 },
	{ DBGBUS_SSPP1, 2, 6 },
	{ DBGBUS_SSPP1, 2, 7 },

	{ DBGBUS_SSPP1, 4, 0 },
	{ DBGBUS_SSPP1, 4, 1 },
	{ DBGBUS_SSPP1, 4, 2 },
	{ DBGBUS_SSPP1, 4, 3 },
	{ DBGBUS_SSPP1, 4, 4 },
	{ DBGBUS_SSPP1, 4, 5 },
	{ DBGBUS_SSPP1, 4, 6 },
	{ DBGBUS_SSPP1, 4, 7 },

	{ DBGBUS_SSPP1, 5, 0 },
	{ DBGBUS_SSPP1, 5, 1 },
	{ DBGBUS_SSPP1, 5, 2 },
	{ DBGBUS_SSPP1, 5, 3 },
	{ DBGBUS_SSPP1, 5, 4 },
	{ DBGBUS_SSPP1, 5, 5 },
	{ DBGBUS_SSPP1, 5, 6 },
	{ DBGBUS_SSPP1, 5, 7 },

	/* vig 3 */
	{ DBGBUS_SSPP1, 20, 0 },
	{ DBGBUS_SSPP1, 20, 1 },
	{ DBGBUS_SSPP1, 20, 2 },
	{ DBGBUS_SSPP1, 20, 3 },
	{ DBGBUS_SSPP1, 20, 4 },
	{ DBGBUS_SSPP1, 20, 5 },
	{ DBGBUS_SSPP1, 20, 6 },
	{ DBGBUS_SSPP1, 20, 7 },

	{ DBGBUS_SSPP1, 21, 0 },
	{ DBGBUS_SSPP1, 21, 1 },
	{ DBGBUS_SSPP1, 21, 2 },
	{ DBGBUS_SSPP1, 21, 3 },
	{ DBGBUS_SSPP1, 21, 4 },
	{ DBGBUS_SSPP1, 21, 5 },
	{ DBGBUS_SSPP1, 21, 6 },
	{ DBGBUS_SSPP1, 21, 7 },

	{ DBGBUS_SSPP1, 22, 0 },
	{ DBGBUS_SSPP1, 22, 1 },
	{ DBGBUS_SSPP1, 22, 2 },
	{ DBGBUS_SSPP1, 22, 3 },
	{ DBGBUS_SSPP1, 22, 4 },
	{ DBGBUS_SSPP1, 22, 5 },
	{ DBGBUS_SSPP1, 22, 6 },
	{ DBGBUS_SSPP1, 22, 7 },

	{ DBGBUS_SSPP1, 24, 0 },
	{ DBGBUS_SSPP1, 24, 1 },
	{ DBGBUS_SSPP1, 24, 2 },
	{ DBGBUS_SSPP1, 24, 3 },
	{ DBGBUS_SSPP1, 24, 4 },
	{ DBGBUS_SSPP1, 24, 5 },
	{ DBGBUS_SSPP1, 24, 6 },
	{ DBGBUS_SSPP1, 24, 7 },

	{ DBGBUS_SSPP1, 25, 0 },
	{ DBGBUS_SSPP1, 25, 1 },
	{ DBGBUS_SSPP1, 25, 2 },
	{ DBGBUS_SSPP1, 25, 3 },
	{ DBGBUS_SSPP1, 25, 4 },
	{ DBGBUS_SSPP1, 25, 5 },
	{ DBGBUS_SSPP1, 25, 6 },
	{ DBGBUS_SSPP1, 25, 7 },

	/* dma 3 */
	{ DBGBUS_SSPP1, 30, 0 },
	{ DBGBUS_SSPP1, 30, 1 },
	{ DBGBUS_SSPP1, 30, 2 },
	{ DBGBUS_SSPP1, 30, 3 },
	{ DBGBUS_SSPP1, 30, 4 },
	{ DBGBUS_SSPP1, 30, 5 },
	{ DBGBUS_SSPP1, 30, 6 },
	{ DBGBUS_SSPP1, 30, 7 },

	{ DBGBUS_SSPP1, 31, 0 },
	{ DBGBUS_SSPP1, 31, 1 },
	{ DBGBUS_SSPP1, 31, 2 },
	{ DBGBUS_SSPP1, 31, 3 },
	{ DBGBUS_SSPP1, 31, 4 },
	{ DBGBUS_SSPP1, 31, 5 },
	{ DBGBUS_SSPP1, 31, 6 },
	{ DBGBUS_SSPP1, 31, 7 },

	{ DBGBUS_SSPP1, 32, 0 },
	{ DBGBUS_SSPP1, 32, 1 },
	{ DBGBUS_SSPP1, 32, 2 },
	{ DBGBUS_SSPP1, 32, 3 },
	{ DBGBUS_SSPP1, 32, 4 },
	{ DBGBUS_SSPP1, 32, 5 },
	{ DBGBUS_SSPP1, 32, 6 },
	{ DBGBUS_SSPP1, 32, 7 },

	{ DBGBUS_SSPP1, 33, 0 },
	{ DBGBUS_SSPP1, 33, 1 },
	{ DBGBUS_SSPP1, 33, 2 },
	{ DBGBUS_SSPP1, 33, 3 },
	{ DBGBUS_SSPP1, 33, 4 },
	{ DBGBUS_SSPP1, 33, 5 },
	{ DBGBUS_SSPP1, 33, 6 },
	{ DBGBUS_SSPP1, 33, 7 },

	{ DBGBUS_SSPP1, 34, 0 },
	{ DBGBUS_SSPP1, 34, 1 },
	{ DBGBUS_SSPP1, 34, 2 },
	{ DBGBUS_SSPP1, 34, 3 },
	{ DBGBUS_SSPP1, 34, 4 },
	{ DBGBUS_SSPP1, 34, 5 },
	{ DBGBUS_SSPP1, 34, 6 },
	{ DBGBUS_SSPP1, 34, 7 },

	{ DBGBUS_SSPP1, 35, 0 },
	{ DBGBUS_SSPP1, 35, 1 },
	{ DBGBUS_SSPP1, 35, 2 },

	/* dma 1 */
	{ DBGBUS_SSPP1, 40, 0 },
	{ DBGBUS_SSPP1, 40, 1 },
	{ DBGBUS_SSPP1, 40, 2 },
	{ DBGBUS_SSPP1, 40, 3 },
	{ DBGBUS_SSPP1, 40, 4 },
	{ DBGBUS_SSPP1, 40, 5 },
	{ DBGBUS_SSPP1, 40, 6 },
	{ DBGBUS_SSPP1, 40, 7 },

	{ DBGBUS_SSPP1, 41, 0 },
	{ DBGBUS_SSPP1, 41, 1 },
	{ DBGBUS_SSPP1, 41, 2 },
	{ DBGBUS_SSPP1, 41, 3 },
	{ DBGBUS_SSPP1, 41, 4 },
	{ DBGBUS_SSPP1, 41, 5 },
	{ DBGBUS_SSPP1, 41, 6 },
	{ DBGBUS_SSPP1, 41, 7 },

	{ DBGBUS_SSPP1, 42, 0 },
	{ DBGBUS_SSPP1, 42, 1 },
	{ DBGBUS_SSPP1, 42, 2 },
	{ DBGBUS_SSPP1, 42, 3 },
	{ DBGBUS_SSPP1, 42, 4 },
	{ DBGBUS_SSPP1, 42, 5 },
	{ DBGBUS_SSPP1, 42, 6 },
	{ DBGBUS_SSPP1, 42, 7 },

	{ DBGBUS_SSPP1, 44, 0 },
	{ DBGBUS_SSPP1, 44, 1 },
	{ DBGBUS_SSPP1, 44, 2 },
	{ DBGBUS_SSPP1, 44, 3 },
	{ DBGBUS_SSPP1, 44, 4 },
	{ DBGBUS_SSPP1, 44, 5 },
	{ DBGBUS_SSPP1, 44, 6 },
	{ DBGBUS_SSPP1, 44, 7 },

	{ DBGBUS_SSPP1, 45, 0 },
	{ DBGBUS_SSPP1, 45, 1 },
	{ DBGBUS_SSPP1, 45, 2 },
	{ DBGBUS_SSPP1, 45, 3 },
	{ DBGBUS_SSPP1, 45, 4 },
	{ DBGBUS_SSPP1, 45, 5 },
	{ DBGBUS_SSPP1, 45, 6 },
	{ DBGBUS_SSPP1, 45, 7 },

	/* cursor 1 */
	{ DBGBUS_SSPP1, 80, 0 },
	{ DBGBUS_SSPP1, 80, 1 },
	{ DBGBUS_SSPP1, 80, 2 },
	{ DBGBUS_SSPP1, 80, 3 },
	{ DBGBUS_SSPP1, 80, 4 },
	{ DBGBUS_SSPP1, 80, 5 },
	{ DBGBUS_SSPP1, 80, 6 },
	{ DBGBUS_SSPP1, 80, 7 },

	{ DBGBUS_SSPP1, 81, 0 },
	{ DBGBUS_SSPP1, 81, 1 },
	{ DBGBUS_SSPP1, 81, 2 },
	{ DBGBUS_SSPP1, 81, 3 },
	{ DBGBUS_SSPP1, 81, 4 },
	{ DBGBUS_SSPP1, 81, 5 },
	{ DBGBUS_SSPP1, 81, 6 },
	{ DBGBUS_SSPP1, 81, 7 },

	{ DBGBUS_SSPP1, 82, 0 },
	{ DBGBUS_SSPP1, 82, 1 },
	{ DBGBUS_SSPP1, 82, 2 },
	{ DBGBUS_SSPP1, 82, 3 },
	{ DBGBUS_SSPP1, 82, 4 },
	{ DBGBUS_SSPP1, 82, 5 },
	{ DBGBUS_SSPP1, 82, 6 },
	{ DBGBUS_SSPP1, 82, 7 },

	{ DBGBUS_SSPP1, 83, 0 },
	{ DBGBUS_SSPP1, 83, 1 },
	{ DBGBUS_SSPP1, 83, 2 },
	{ DBGBUS_SSPP1, 83, 3 },
	{ DBGBUS_SSPP1, 83, 4 },
	{ DBGBUS_SSPP1, 83, 5 },
	{ DBGBUS_SSPP1, 83, 6 },
	{ DBGBUS_SSPP1, 83, 7 },

	{ DBGBUS_SSPP1, 84, 0 },
	{ DBGBUS_SSPP1, 84, 1 },
	{ DBGBUS_SSPP1, 84, 2 },
	{ DBGBUS_SSPP1, 84, 3 },
	{ DBGBUS_SSPP1, 84, 4 },
	{ DBGBUS_SSPP1, 84, 5 },
	{ DBGBUS_SSPP1, 84, 6 },
	{ DBGBUS_SSPP1, 84, 7 },

	/* dspp */
	{ DBGBUS_DSPP, 13, 0 },
	{ DBGBUS_DSPP, 19, 0 },
	{ DBGBUS_DSPP, 14, 0 },
	{ DBGBUS_DSPP, 14, 1 },
	{ DBGBUS_DSPP, 14, 3 },
	{ DBGBUS_DSPP, 20, 0 },
	{ DBGBUS_DSPP, 20, 1 },
	{ DBGBUS_DSPP, 20, 3 },

	/* ppb_0 */
	{ DBGBUS_DSPP, 31, 0 },
	{ DBGBUS_DSPP, 33, 0 },
	{ DBGBUS_DSPP, 35, 0 },
	{ DBGBUS_DSPP, 42, 0 },

	/* ppb_1 */
	{ DBGBUS_DSPP, 32, 0 },
	{ DBGBUS_DSPP, 34, 0 },
	{ DBGBUS_DSPP, 36, 0 },
	{ DBGBUS_DSPP, 43, 0 },

	/* lm_lut */
	{ DBGBUS_DSPP, 109, 0 },
	{ DBGBUS_DSPP, 105, 0 },
	{ DBGBUS_DSPP, 103, 0 },

	/* tear-check */
	{ DBGBUS_PERIPH, 63, 0 },
	{ DBGBUS_PERIPH, 64, 0 },
	{ DBGBUS_PERIPH, 65, 0 },
	{ DBGBUS_PERIPH, 73, 0 },
	{ DBGBUS_PERIPH, 74, 0 },

	/* crossbar */
	{ DBGBUS_DSPP, 0, 0},

	/* rotator */
	{ DBGBUS_DSPP, 9, 0},

	/* blend */
	/* LM0 */
	{ DBGBUS_DSPP, 63, 0},
	{ DBGBUS_DSPP, 63, 1},
	{ DBGBUS_DSPP, 63, 2},
	{ DBGBUS_DSPP, 63, 3},
	{ DBGBUS_DSPP, 63, 4},
	{ DBGBUS_DSPP, 63, 5},
	{ DBGBUS_DSPP, 63, 6},
	{ DBGBUS_DSPP, 63, 7},

	{ DBGBUS_DSPP, 64, 0},
	{ DBGBUS_DSPP, 64, 1},
	{ DBGBUS_DSPP, 64, 2},
	{ DBGBUS_DSPP, 64, 3},
	{ DBGBUS_DSPP, 64, 4},
	{ DBGBUS_DSPP, 64, 5},
	{ DBGBUS_DSPP, 64, 6},
	{ DBGBUS_DSPP, 64, 7},

	{ DBGBUS_DSPP, 65, 0},
	{ DBGBUS_DSPP, 65, 1},
	{ DBGBUS_DSPP, 65, 2},
	{ DBGBUS_DSPP, 65, 3},
	{ DBGBUS_DSPP, 65, 4},
	{ DBGBUS_DSPP, 65, 5},
	{ DBGBUS_DSPP, 65, 6},
	{ DBGBUS_DSPP, 65, 7},

	{ DBGBUS_DSPP, 66, 0},
	{ DBGBUS_DSPP, 66, 1},
	{ DBGBUS_DSPP, 66, 2},
	{ DBGBUS_DSPP, 66, 3},
	{ DBGBUS_DSPP, 66, 4},
	{ DBGBUS_DSPP, 66, 5},
	{ DBGBUS_DSPP, 66, 6},
	{ DBGBUS_DSPP, 66, 7},

	{ DBGBUS_DSPP, 67, 0},
	{ DBGBUS_DSPP, 67, 1},
	{ DBGBUS_DSPP, 67, 2},
	{ DBGBUS_DSPP, 67, 3},
	{ DBGBUS_DSPP, 67, 4},
	{ DBGBUS_DSPP, 67, 5},
	{ DBGBUS_DSPP, 67, 6},
	{ DBGBUS_DSPP, 67, 7},

	{ DBGBUS_DSPP, 68, 0},
	{ DBGBUS_DSPP, 68, 1},
	{ DBGBUS_DSPP, 68, 2},
	{ DBGBUS_DSPP, 68, 3},
	{ DBGBUS_DSPP, 68, 4},
	{ DBGBUS_DSPP, 68, 5},
	{ DBGBUS_DSPP, 68, 6},
	{ DBGBUS_DSPP, 68, 7},

	{ DBGBUS_DSPP, 69, 0},
	{ DBGBUS_DSPP, 69, 1},
	{ DBGBUS_DSPP, 69, 2},
	{ DBGBUS_DSPP, 69, 3},
	{ DBGBUS_DSPP, 69, 4},
	{ DBGBUS_DSPP, 69, 5},
	{ DBGBUS_DSPP, 69, 6},
	{ DBGBUS_DSPP, 69, 7},

	/* LM1 */
	{ DBGBUS_DSPP, 70, 0},
	{ DBGBUS_DSPP, 70, 1},
	{ DBGBUS_DSPP, 70, 2},
	{ DBGBUS_DSPP, 70, 3},
	{ DBGBUS_DSPP, 70, 4},
	{ DBGBUS_DSPP, 70, 5},
	{ DBGBUS_DSPP, 70, 6},
	{ DBGBUS_DSPP, 70, 7},

	{ DBGBUS_DSPP, 71, 0},
	{ DBGBUS_DSPP, 71, 1},
	{ DBGBUS_DSPP, 71, 2},
	{ DBGBUS_DSPP, 71, 3},
	{ DBGBUS_DSPP, 71, 4},
	{ DBGBUS_DSPP, 71, 5},
	{ DBGBUS_DSPP, 71, 6},
	{ DBGBUS_DSPP, 71, 7},

	{ DBGBUS_DSPP, 72, 0},
	{ DBGBUS_DSPP, 72, 1},
	{ DBGBUS_DSPP, 72, 2},
	{ DBGBUS_DSPP, 72, 3},
	{ DBGBUS_DSPP, 72, 4},
	{ DBGBUS_DSPP, 72, 5},
	{ DBGBUS_DSPP, 72, 6},
	{ DBGBUS_DSPP, 72, 7},

	{ DBGBUS_DSPP, 73, 0},
	{ DBGBUS_DSPP, 73, 1},
	{ DBGBUS_DSPP, 73, 2},
	{ DBGBUS_DSPP, 73, 3},
	{ DBGBUS_DSPP, 73, 4},
	{ DBGBUS_DSPP, 73, 5},
	{ DBGBUS_DSPP, 73, 6},
	{ DBGBUS_DSPP, 73, 7},

	{ DBGBUS_DSPP, 74, 0},
	{ DBGBUS_DSPP, 74, 1},
	{ DBGBUS_DSPP, 74, 2},
	{ DBGBUS_DSPP, 74, 3},
	{ DBGBUS_DSPP, 74, 4},
	{ DBGBUS_DSPP, 74, 5},
	{ DBGBUS_DSPP, 74, 6},
	{ DBGBUS_DSPP, 74, 7},

	{ DBGBUS_DSPP, 75, 0},
	{ DBGBUS_DSPP, 75, 1},
	{ DBGBUS_DSPP, 75, 2},
	{ DBGBUS_DSPP, 75, 3},
	{ DBGBUS_DSPP, 75, 4},
	{ DBGBUS_DSPP, 75, 5},
	{ DBGBUS_DSPP, 75, 6},
	{ DBGBUS_DSPP, 75, 7},

	{ DBGBUS_DSPP, 76, 0},
	{ DBGBUS_DSPP, 76, 1},
	{ DBGBUS_DSPP, 76, 2},
	{ DBGBUS_DSPP, 76, 3},
	{ DBGBUS_DSPP, 76, 4},
	{ DBGBUS_DSPP, 76, 5},
	{ DBGBUS_DSPP, 76, 6},
	{ DBGBUS_DSPP, 76, 7},

	/* LM2 */
	{ DBGBUS_DSPP, 77, 0},
	{ DBGBUS_DSPP, 77, 1},
	{ DBGBUS_DSPP, 77, 2},
	{ DBGBUS_DSPP, 77, 3},
	{ DBGBUS_DSPP, 77, 4},
	{ DBGBUS_DSPP, 77, 5},
	{ DBGBUS_DSPP, 77, 6},
	{ DBGBUS_DSPP, 77, 7},

	{ DBGBUS_DSPP, 78, 0},
	{ DBGBUS_DSPP, 78, 1},
	{ DBGBUS_DSPP, 78, 2},
	{ DBGBUS_DSPP, 78, 3},
	{ DBGBUS_DSPP, 78, 4},
	{ DBGBUS_DSPP, 78, 5},
	{ DBGBUS_DSPP, 78, 6},
	{ DBGBUS_DSPP, 78, 7},

	{ DBGBUS_DSPP, 79, 0},
	{ DBGBUS_DSPP, 79, 1},
	{ DBGBUS_DSPP, 79, 2},
	{ DBGBUS_DSPP, 79, 3},
	{ DBGBUS_DSPP, 79, 4},
	{ DBGBUS_DSPP, 79, 5},
	{ DBGBUS_DSPP, 79, 6},
	{ DBGBUS_DSPP, 79, 7},

	{ DBGBUS_DSPP, 80, 0},
	{ DBGBUS_DSPP, 80, 1},
	{ DBGBUS_DSPP, 80, 2},
	{ DBGBUS_DSPP, 80, 3},
	{ DBGBUS_DSPP, 80, 4},
	{ DBGBUS_DSPP, 80, 5},
	{ DBGBUS_DSPP, 80, 6},
	{ DBGBUS_DSPP, 80, 7},

	{ DBGBUS_DSPP, 81, 0},
	{ DBGBUS_DSPP, 81, 1},
	{ DBGBUS_DSPP, 81, 2},
	{ DBGBUS_DSPP, 81, 3},
	{ DBGBUS_DSPP, 81, 4},
	{ DBGBUS_DSPP, 81, 5},
	{ DBGBUS_DSPP, 81, 6},
	{ DBGBUS_DSPP, 81, 7},

	{ DBGBUS_DSPP, 82, 0},
	{ DBGBUS_DSPP, 82, 1},
	{ DBGBUS_DSPP, 82, 2},
	{ DBGBUS_DSPP, 82, 3},
	{ DBGBUS_DSPP, 82, 4},
	{ DBGBUS_DSPP, 82, 5},
	{ DBGBUS_DSPP, 82, 6},
	{ DBGBUS_DSPP, 82, 7},

	{ DBGBUS_DSPP, 83, 0},
	{ DBGBUS_DSPP, 83, 1},
	{ DBGBUS_DSPP, 83, 2},
	{ DBGBUS_DSPP, 83, 3},
	{ DBGBUS_DSPP, 83, 4},
	{ DBGBUS_DSPP, 83, 5},
	{ DBGBUS_DSPP, 83, 6},
	{ DBGBUS_DSPP, 83, 7},

	/* csc */
	{ DBGBUS_SSPP0, 7, 0},
	{ DBGBUS_SSPP0, 7, 1},
	{ DBGBUS_SSPP0, 27, 0},
	{ DBGBUS_SSPP0, 27, 1},
	{ DBGBUS_SSPP1, 7, 0},
	{ DBGBUS_SSPP1, 7, 1},
	{ DBGBUS_SSPP1, 27, 0},
	{ DBGBUS_SSPP1, 27, 1},

	/* pcc */
	{ DBGBUS_SSPP0, 3,  3},
	{ DBGBUS_SSPP0, 23, 3},
	{ DBGBUS_SSPP0, 33, 3},
	{ DBGBUS_SSPP0, 43, 3},
	{ DBGBUS_SSPP1, 3,  3},
	{ DBGBUS_SSPP1, 23, 3},
	{ DBGBUS_SSPP1, 33, 3},
	{ DBGBUS_SSPP1, 43, 3},

	/* spa */
	{ DBGBUS_SSPP0, 8,  0},
	{ DBGBUS_SSPP0, 28, 0},
	{ DBGBUS_SSPP1, 8,  0},
	{ DBGBUS_SSPP1, 28, 0},
	{ DBGBUS_DSPP, 13, 0},
	{ DBGBUS_DSPP, 19, 0},

	/* igc */
	{ DBGBUS_SSPP0, 9,  0},
	{ DBGBUS_SSPP0, 9,  1},
	{ DBGBUS_SSPP0, 9,  3},
	{ DBGBUS_SSPP0, 29, 0},
	{ DBGBUS_SSPP0, 29, 1},
	{ DBGBUS_SSPP0, 29, 3},
	{ DBGBUS_SSPP0, 17, 0},
	{ DBGBUS_SSPP0, 17, 1},
	{ DBGBUS_SSPP0, 17, 3},
	{ DBGBUS_SSPP0, 37, 0},
	{ DBGBUS_SSPP0, 37, 1},
	{ DBGBUS_SSPP0, 37, 3},
	{ DBGBUS_SSPP0, 46, 0},
	{ DBGBUS_SSPP0, 46, 1},
	{ DBGBUS_SSPP0, 46, 3},

	{ DBGBUS_SSPP1, 9,  0},
	{ DBGBUS_SSPP1, 9,  1},
	{ DBGBUS_SSPP1, 9,  3},
	{ DBGBUS_SSPP1, 29, 0},
	{ DBGBUS_SSPP1, 29, 1},
	{ DBGBUS_SSPP1, 29, 3},
	{ DBGBUS_SSPP1, 17, 0},
	{ DBGBUS_SSPP1, 17, 1},
	{ DBGBUS_SSPP1, 17, 3},
	{ DBGBUS_SSPP1, 37, 0},
	{ DBGBUS_SSPP1, 37, 1},
	{ DBGBUS_SSPP1, 37, 3},
	{ DBGBUS_SSPP1, 46, 0},
	{ DBGBUS_SSPP1, 46, 1},
	{ DBGBUS_SSPP1, 46, 3},

	{ DBGBUS_DSPP, 14, 0},
	{ DBGBUS_DSPP, 14, 1},
	{ DBGBUS_DSPP, 14, 3},
	{ DBGBUS_DSPP, 20, 0},
	{ DBGBUS_DSPP, 20, 1},
	{ DBGBUS_DSPP, 20, 3},

	{ DBGBUS_PERIPH, 60, 0},
};

static struct vbif_debug_bus_entry vbif_dbg_bus_msm8998[] = {
	{0x214, 0x21c, 16, 2, 0x0, 0xd},     /* arb clients */
	{0x214, 0x21c, 16, 2, 0x80, 0xc0},   /* arb clients */
	{0x214, 0x21c, 16, 2, 0x100, 0x140}, /* arb clients */
	{0x214, 0x21c, 0, 16, 0x0, 0xf},     /* xin blocks - axi side */
	{0x214, 0x21c, 0, 16, 0x80, 0xa4},   /* xin blocks - axi side */
	{0x214, 0x21c, 0, 15, 0x100, 0x124}, /* xin blocks - axi side */
	{0x21c, 0x214, 0, 14, 0, 0xc}, /* xin blocks - clock side */
};

/**
 * _sde_dbg_enable_power - use callback to turn power on for hw register access
 * @enable: whether to turn power on or off
 */
static inline void _sde_dbg_enable_power(int enable)
{
	if (!sde_dbg_base.power_ctrl.enable_fn)
		return;
	sde_dbg_base.power_ctrl.enable_fn(
			sde_dbg_base.power_ctrl.handle,
			sde_dbg_base.power_ctrl.client,
			enable);
}

/**
 * _sde_dump_reg - helper function for dumping rotator register set content
 * @dump_name: register set name
 * @reg_dump_flag: dumping flag controlling in-log/memory dump location
 * @base_addr: starting address of io region for calculating offsets to print
 * @addr: starting address offset for dumping
 * @len_bytes: range of the register set
 * @dump_mem: output buffer for memory dump location option
 * @from_isr: whether being called from isr context
 */
static void _sde_dump_reg(const char *dump_name, u32 reg_dump_flag,
		char __iomem *base_addr, char __iomem *addr, size_t len_bytes,
		u32 **dump_mem, bool from_isr)
{
	u32 in_log, in_mem, len_align, len_padded;
	u32 *dump_addr = NULL;
	char __iomem *end_addr;
	int i;

	if (!len_bytes)
		return;

	in_log = (reg_dump_flag & SDE_DBG_DUMP_IN_LOG);
	in_mem = (reg_dump_flag & SDE_DBG_DUMP_IN_MEM);

	pr_debug("%s: reg_dump_flag=%d in_log=%d in_mem=%d\n",
		dump_name, reg_dump_flag, in_log, in_mem);

	if (!in_log && !in_mem)
		return;

	if (in_log)
		dev_info(sde_dbg_base.dev, "%s: start_offset 0x%lx len 0x%zx\n",
				dump_name, addr - base_addr, len_bytes);

	len_align = (len_bytes + REG_DUMP_ALIGN - 1) / REG_DUMP_ALIGN;
	len_padded = len_align * REG_DUMP_ALIGN;
	end_addr = addr + len_bytes;

	if (in_mem) {
		if (dump_mem && !(*dump_mem)) {
			phys_addr_t phys = 0;
			*dump_mem = dma_alloc_coherent(sde_dbg_base.dev,
					len_padded, &phys, GFP_KERNEL);
		}

		if (dump_mem && *dump_mem) {
			dump_addr = *dump_mem;
			dev_info(sde_dbg_base.dev,
				"%s: start_addr:0x%pK len:0x%x reg_offset=0x%lx\n",
				dump_name, dump_addr, len_padded,
				addr - base_addr);
		} else {
			in_mem = 0;
			pr_err("dump_mem: kzalloc fails!\n");
		}
	}

	if (!from_isr)
		_sde_dbg_enable_power(true);

	for (i = 0; i < len_align; i++) {
		u32 x0, x4, x8, xc;

		x0 = (addr < end_addr) ? readl_relaxed(addr + 0x0) : 0;
		x4 = (addr + 0x4 < end_addr) ? readl_relaxed(addr + 0x4) : 0;
		x8 = (addr + 0x8 < end_addr) ? readl_relaxed(addr + 0x8) : 0;
		xc = (addr + 0xc < end_addr) ? readl_relaxed(addr + 0xc) : 0;

		if (in_log)
			dev_info(sde_dbg_base.dev,
					"0x%lx : %08x %08x %08x %08x\n",
					addr - base_addr, x0, x4, x8, xc);

		if (dump_addr) {
			dump_addr[i * 4] = x0;
			dump_addr[i * 4 + 1] = x4;
			dump_addr[i * 4 + 2] = x8;
			dump_addr[i * 4 + 3] = xc;
		}

		addr += REG_DUMP_ALIGN;
	}

	if (!from_isr)
		_sde_dbg_enable_power(false);
}

/**
 * _sde_dbg_get_dump_range - helper to retrieve dump length for a range node
 * @range_node: range node to dump
 * @max_offset: max offset of the register base
 * @Return: length
 */
static u32 _sde_dbg_get_dump_range(struct sde_dbg_reg_offset *range_node,
		size_t max_offset)
{
	u32 length = 0;

	if ((range_node->start > range_node->end) ||
		(range_node->end > max_offset) || (range_node->start == 0
		&& range_node->end == 0)) {
		length = max_offset;
	} else {
		length = range_node->end - range_node->start;
	}

	return length;
}

static int _sde_dump_reg_range_cmp(void *priv, struct list_head *a,
		struct list_head *b)
{
	struct sde_dbg_reg_range *ar, *br;

	if (!a || !b)
		return 0;

	ar = container_of(a, struct sde_dbg_reg_range, head);
	br = container_of(b, struct sde_dbg_reg_range, head);

	return ar->offset.start - br->offset.start;
}

/**
 * _sde_dump_reg_by_ranges - dump ranges or full range of the register blk base
 * @dbg: register blk base structure
 * @reg_dump_flag: dump target, memory, kernel log, or both
 */
static void _sde_dump_reg_by_ranges(struct sde_dbg_reg_base *dbg,
	u32 reg_dump_flag)
{
	char __iomem *addr;
	size_t len;
	struct sde_dbg_reg_range *range_node;

	if (!dbg || !dbg->base) {
		pr_err("dbg base is null!\n");
		return;
	}

	dev_info(sde_dbg_base.dev, "%s:=========%s DUMP=========\n", __func__,
			dbg->name);

	/* If there is a list to dump the registers by ranges, use the ranges */
	if (!list_empty(&dbg->sub_range_list)) {
		/* sort the list by start address first */
		list_sort(NULL, &dbg->sub_range_list, _sde_dump_reg_range_cmp);
		list_for_each_entry(range_node, &dbg->sub_range_list, head) {
			len = _sde_dbg_get_dump_range(&range_node->offset,
				dbg->max_offset);
			addr = dbg->base + range_node->offset.start;
			pr_debug("%s: range_base=0x%pK start=0x%x end=0x%x\n",
				range_node->range_name,
				addr, range_node->offset.start,
				range_node->offset.end);

			_sde_dump_reg(range_node->range_name, reg_dump_flag,
					dbg->base, addr, len,
					&range_node->reg_dump, false);
		}
	} else {
		/* If there is no list to dump ranges, dump all registers */
		dev_info(sde_dbg_base.dev,
				"Ranges not found, will dump full registers\n");
		dev_info(sde_dbg_base.dev, "base:0x%pK len:0x%zx\n", dbg->base,
				dbg->max_offset);
		addr = dbg->base;
		len = dbg->max_offset;
		_sde_dump_reg(dbg->name, reg_dump_flag, dbg->base, addr, len,
				&dbg->reg_dump, false);
	}
}

/**
 * _sde_dump_reg_by_blk - dump a named register base region
 * @blk_name: register blk name
 */
static void _sde_dump_reg_by_blk(const char *blk_name)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;

	if (!dbg_base)
		return;

	list_for_each_entry(blk_base, &dbg_base->reg_base_list, reg_base_head) {
		if (strlen(blk_base->name) &&
			!strcmp(blk_base->name, blk_name)) {
			_sde_dump_reg_by_ranges(blk_base,
				dbg_base->enable_reg_dump);
			break;
		}
	}
}

/**
 * _sde_dump_reg_all - dump all register regions
 */
static void _sde_dump_reg_all(void)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;

	if (!dbg_base)
		return;

	list_for_each_entry(blk_base, &dbg_base->reg_base_list, reg_base_head)
		if (strlen(blk_base->name))
			_sde_dump_reg_by_blk(blk_base->name);
}

/**
 * _sde_dump_get_blk_addr - retrieve register block address by name
 * @blk_name: register blk name
 * @Return: register blk base, or NULL
 */
static struct sde_dbg_reg_base *_sde_dump_get_blk_addr(const char *blk_name)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;

	list_for_each_entry(blk_base, &dbg_base->reg_base_list, reg_base_head)
		if (strlen(blk_base->name) && !strcmp(blk_base->name, blk_name))
			return blk_base;

	return NULL;
}

static void _sde_dbg_dump_sde_dbg_bus(struct sde_dbg_sde_debug_bus *bus)
{
	bool in_log, in_mem;
	u32 **dump_mem = NULL;
	u32 *dump_addr = NULL;
	u32 status = 0;
	struct sde_debug_bus_entry *head;
	phys_addr_t phys = 0;
	int list_size;
	int i;
	u32 offset;
	void __iomem *mem_base = NULL;
	struct sde_dbg_reg_base *reg_base;

	if (!bus || !bus->cmn.entries_size)
		return;

	list_for_each_entry(reg_base, &sde_dbg_base.reg_base_list,
			reg_base_head)
		if (strlen(reg_base->name) &&
			!strcmp(reg_base->name, bus->cmn.name))
			mem_base = reg_base->base + bus->top_blk_off;

	if (!mem_base) {
		pr_err("unable to find mem_base for %s\n", bus->cmn.name);
		return;
	}

	dump_mem = &bus->cmn.dumped_content;

	/* will keep in memory 4 entries of 4 bytes each */
	list_size = (bus->cmn.entries_size * 4 * 4);

	in_log = (bus->cmn.enable_mask & SDE_DBG_DUMP_IN_LOG);
	in_mem = (bus->cmn.enable_mask & SDE_DBG_DUMP_IN_MEM);

	if (!in_log && !in_mem)
		return;

	dev_info(sde_dbg_base.dev, "======== start %s dump =========\n",
			bus->cmn.name);

	if (in_mem) {
		if (!(*dump_mem))
			*dump_mem = dma_alloc_coherent(sde_dbg_base.dev,
				list_size, &phys, GFP_KERNEL);

		if (*dump_mem) {
			dump_addr = *dump_mem;
			dev_info(sde_dbg_base.dev,
				"%s: start_addr:0x%pK len:0x%x\n",
				__func__, dump_addr, list_size);
		} else {
			in_mem = false;
			pr_err("dump_mem: allocation fails\n");
		}
	}

	_sde_dbg_enable_power(true);
	for (i = 0; i < bus->cmn.entries_size; i++) {
		head = bus->entries + i;
		writel_relaxed(TEST_MASK(head->block_id, head->test_id),
				mem_base + head->wr_addr);
		wmb(); /* make sure test bits were written */

		if (bus->cmn.flags & DBGBUS_FLAGS_DSPP)
			offset = DBGBUS_DSPP_STATUS;
		else
			offset = head->wr_addr + 0x4;

		status = readl_relaxed(mem_base + offset);

		if (in_log)
			dev_info(sde_dbg_base.dev,
					"waddr=0x%x blk=%d tst=%d val=0x%x\n",
					head->wr_addr, head->block_id,
					head->test_id, status);

		if (dump_addr && in_mem) {
			dump_addr[i*4]     = head->wr_addr;
			dump_addr[i*4 + 1] = head->block_id;
			dump_addr[i*4 + 2] = head->test_id;
			dump_addr[i*4 + 3] = status;
		}

		/* Disable debug bus once we are done */
		writel_relaxed(0, mem_base + head->wr_addr);

	}
	_sde_dbg_enable_power(false);

	dev_info(sde_dbg_base.dev, "======== end %s dump =========\n",
			bus->cmn.name);
}

static void _sde_dbg_dump_vbif_debug_bus_entry(
		struct vbif_debug_bus_entry *head, void __iomem *mem_base,
		u32 *dump_addr, bool in_log)
{
	int i, j;
	u32 val;

	if (!dump_addr && !in_log)
		return;

	for (i = 0; i < head->block_cnt; i++) {
		writel_relaxed(1 << (i + head->bit_offset),
				mem_base + head->block_bus_addr);
		/* make sure that current bus blcok enable */
		wmb();
		for (j = head->test_pnt_start; j < head->test_pnt_cnt; j++) {
			writel_relaxed(j, mem_base + head->block_bus_addr + 4);
			/* make sure that test point is enabled */
			wmb();
			val = readl_relaxed(mem_base + MMSS_VBIF_TEST_BUS_OUT);
			if (dump_addr) {
				*dump_addr++ = head->block_bus_addr;
				*dump_addr++ = i;
				*dump_addr++ = j;
				*dump_addr++ = val;
			}
			if (in_log)
				dev_info(sde_dbg_base.dev,
					"testpoint:%x arb/xin id=%d index=%d val=0x%x\n",
					head->block_bus_addr, i, j, val);
		}
	}
}

static void _sde_dbg_dump_vbif_dbg_bus(struct sde_dbg_vbif_debug_bus *bus)
{
	bool in_log, in_mem;
	u32 **dump_mem = NULL;
	u32 *dump_addr = NULL;
	u32 value;
	struct vbif_debug_bus_entry *head;
	phys_addr_t phys = 0;
	int i, list_size = 0;
	void __iomem *mem_base = NULL;
	struct vbif_debug_bus_entry *dbg_bus;
	u32 bus_size;
	struct sde_dbg_reg_base *reg_base;

	if (!bus || !bus->cmn.entries_size)
		return;

	list_for_each_entry(reg_base, &sde_dbg_base.reg_base_list,
			reg_base_head)
		if (strlen(reg_base->name) &&
			!strcmp(reg_base->name, bus->cmn.name))
			mem_base = reg_base->base;

	if (!mem_base) {
		pr_err("unable to find mem_base for %s\n", bus->cmn.name);
		return;
	}

	dbg_bus = bus->entries;
	bus_size = bus->cmn.entries_size;
	list_size = bus->cmn.entries_size;
	dump_mem = &bus->cmn.dumped_content;

	dev_info(sde_dbg_base.dev, "======== start %s dump =========\n",
			bus->cmn.name);

	if (!dump_mem || !dbg_bus || !bus_size || !list_size)
		return;

	/* allocate memory for each test point */
	for (i = 0; i < bus_size; i++) {
		head = dbg_bus + i;
		list_size += (head->block_cnt * head->test_pnt_cnt);
	}

	/* 4 bytes * 4 entries for each test point*/
	list_size *= 16;

	in_log = (bus->cmn.enable_mask & SDE_DBG_DUMP_IN_LOG);
	in_mem = (bus->cmn.enable_mask & SDE_DBG_DUMP_IN_MEM);

	if (!in_log && !in_mem)
		return;

	if (in_mem) {
		if (!(*dump_mem))
			*dump_mem = dma_alloc_coherent(sde_dbg_base.dev,
				list_size, &phys, GFP_KERNEL);

		if (*dump_mem) {
			dump_addr = *dump_mem;
			dev_info(sde_dbg_base.dev,
				"%s: start_addr:0x%pK len:0x%x\n",
				__func__, dump_addr, list_size);
		} else {
			in_mem = false;
			pr_err("dump_mem: allocation fails\n");
		}
	}

	_sde_dbg_enable_power(true);

	value = readl_relaxed(mem_base + MMSS_VBIF_CLKON);
	writel_relaxed(value | BIT(1), mem_base + MMSS_VBIF_CLKON);

	/* make sure that vbif core is on */
	wmb();

	for (i = 0; i < bus_size; i++) {
		head = dbg_bus + i;

		writel_relaxed(0, mem_base + head->disable_bus_addr);
		writel_relaxed(BIT(0), mem_base + MMSS_VBIF_TEST_BUS_OUT_CTRL);
		/* make sure that other bus is off */
		wmb();

		_sde_dbg_dump_vbif_debug_bus_entry(head, mem_base, dump_addr,
				in_log);
		if (dump_addr)
			dump_addr += (head->block_cnt * head->test_pnt_cnt * 4);
	}

	_sde_dbg_enable_power(false);

	dev_info(sde_dbg_base.dev, "======== end %s dump =========\n",
			bus->cmn.name);
}

/**
 * _sde_dump_array - dump array of register bases
 * @blk_arr: array of register base pointers
 * @len: length of blk_arr
 * @do_panic: whether to trigger a panic after dumping
 * @name: string indicating origin of dump
 * @dump_dbgbus_sde: whether to dump the sde debug bus
 * @dump_dbgbus_vbif_rt: whether to dump the vbif rt debug bus
 */
static void _sde_dump_array(struct sde_dbg_reg_base *blk_arr[],
	u32 len, bool do_panic, const char *name, bool dump_dbgbus_sde,
	bool dump_dbgbus_vbif_rt, bool dump_all)
{
	int i;

	mutex_lock(&sde_dbg_base.mutex);

	for (i = 0; i < len; i++) {
		if (blk_arr[i] != NULL)
			_sde_dump_reg_by_ranges(blk_arr[i],
				sde_dbg_base.enable_reg_dump);
	}

	if (dump_all)
		sde_evtlog_dump_all(sde_dbg_base.evtlog);

	if (dump_dbgbus_sde)
		_sde_dbg_dump_sde_dbg_bus(&sde_dbg_base.dbgbus_sde);

	if (dump_dbgbus_vbif_rt)
		_sde_dbg_dump_vbif_dbg_bus(&sde_dbg_base.dbgbus_vbif_rt);

	if (do_panic && sde_dbg_base.panic_on_err)
		panic(name);

	mutex_unlock(&sde_dbg_base.mutex);
}

/**
 * _sde_dump_work - deferred dump work function
 * @work: work structure
 */
static void _sde_dump_work(struct work_struct *work)
{
	_sde_dump_array(sde_dbg_base.req_dump_blks,
		ARRAY_SIZE(sde_dbg_base.req_dump_blks),
		sde_dbg_base.work_panic, "evtlog_workitem",
		sde_dbg_base.dbgbus_sde.cmn.include_in_deferred_work,
		sde_dbg_base.dbgbus_vbif_rt.cmn.include_in_deferred_work,
		sde_dbg_base.dump_all);
}

void sde_dbg_dump(bool queue_work, const char *name, ...)
{
	int i, index = 0;
	bool do_panic = false;
	bool dump_dbgbus_sde = false;
	bool dump_dbgbus_vbif_rt = false;
	bool dump_all = false;
	va_list args;
	char *blk_name = NULL;
	struct sde_dbg_reg_base *blk_base = NULL;
	struct sde_dbg_reg_base **blk_arr;
	u32 blk_len;

	if (!sde_evtlog_is_enabled(sde_dbg_base.evtlog, SDE_EVTLOG_DEFAULT))
		return;

	if (queue_work && work_pending(&sde_dbg_base.dump_work))
		return;

	blk_arr = &sde_dbg_base.req_dump_blks[0];
	blk_len = ARRAY_SIZE(sde_dbg_base.req_dump_blks);

	memset(sde_dbg_base.req_dump_blks, 0,
			sizeof(sde_dbg_base.req_dump_blks));
	sde_dbg_base.dump_all = false;

	va_start(args, name);
	i = 0;
	while ((blk_name = va_arg(args, char*))) {
		if (i++ >= SDE_EVTLOG_MAX_DATA) {
			pr_err("could not parse all dump arguments\n");
			break;
		}
		if (IS_ERR_OR_NULL(blk_name))
			break;

		blk_base = _sde_dump_get_blk_addr(blk_name);
		if (blk_base) {
			if (index < blk_len) {
				blk_arr[index] = blk_base;
				index++;
			} else {
				pr_err("insufficient space to to dump %s\n",
						blk_name);
			}
		}
		if (!strcmp(blk_name, "all"))
			dump_all = true;

		if (!strcmp(blk_name, "dbg_bus"))
			dump_dbgbus_sde = true;

		if (!strcmp(blk_name, "vbif_dbg_bus"))
			dump_dbgbus_vbif_rt = true;

		if (!strcmp(blk_name, "panic"))
			do_panic = true;
	}
	va_end(args);

	if (queue_work) {
		/* schedule work to dump later */
		sde_dbg_base.work_panic = do_panic;
		sde_dbg_base.dbgbus_sde.cmn.include_in_deferred_work =
				dump_dbgbus_sde;
		sde_dbg_base.dbgbus_vbif_rt.cmn.include_in_deferred_work =
				dump_dbgbus_vbif_rt;
		sde_dbg_base.dump_all = dump_all;
		schedule_work(&sde_dbg_base.dump_work);
	} else {
		_sde_dump_array(blk_arr, blk_len, do_panic, name,
				dump_dbgbus_sde, dump_dbgbus_vbif_rt, dump_all);
	}
}

void sde_dbg_ctrl(const char *name, ...)
{
	int i = 0;
	va_list args;
	char *blk_name = NULL;


	/* no debugfs controlled events are enabled, just return */
	if (!sde_dbg_base.debugfs_ctrl)
		return;

	va_start(args, name);

	while ((blk_name = va_arg(args, char*))) {
		if (i++ >= SDE_EVTLOG_MAX_DATA) {
			pr_err("could not parse all dbg arguments\n");
			break;
		}

		if (IS_ERR_OR_NULL(blk_name))
			break;

		if (!strcmp(blk_name, "stop_ftrace") &&
				sde_dbg_base.debugfs_ctrl &
				DBG_CTRL_STOP_FTRACE) {
			pr_debug("tracing off\n");
			tracing_off();
		}

		if (!strcmp(blk_name, "panic_underrun") &&
				sde_dbg_base.debugfs_ctrl &
				DBG_CTRL_PANIC_UNDERRUN) {
			pr_debug("panic underrun\n");
			panic("underrun");
		}
	}

}

/*
 * sde_dbg_debugfs_open - debugfs open handler for evtlog dump
 * @inode: debugfs inode
 * @file: file handle
 */
static int sde_dbg_debugfs_open(struct inode *inode, struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

/**
 * sde_evtlog_dump_read - debugfs read handler for evtlog dump
 * @file: file handler
 * @buff: user buffer content for debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_evtlog_dump_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char evtlog_buf[SDE_EVTLOG_BUF_MAX];

	if (!buff || !ppos)
		return -EINVAL;

	len = sde_evtlog_dump_to_buffer(sde_dbg_base.evtlog, evtlog_buf,
			SDE_EVTLOG_BUF_MAX, true);
	if (len < 0 || len > count) {
		pr_err("len is more than user buffer size");
		return 0;
	}

	if (copy_to_user(buff, evtlog_buf, len))
		return -EFAULT;
	*ppos += len;

	return len;
}

/**
 * sde_evtlog_dump_write - debugfs write handler for evtlog dump
 * @file: file handler
 * @user_buf: user buffer content from debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_evtlog_dump_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	_sde_dump_reg_all();

	sde_evtlog_dump_all(sde_dbg_base.evtlog);

	_sde_dbg_dump_sde_dbg_bus(&sde_dbg_base.dbgbus_sde);
	_sde_dbg_dump_vbif_dbg_bus(&sde_dbg_base.dbgbus_vbif_rt);

	if (sde_dbg_base.panic_on_err)
		panic("sde");

	return count;
}

static const struct file_operations sde_evtlog_fops = {
	.open = sde_dbg_debugfs_open,
	.read = sde_evtlog_dump_read,
	.write = sde_evtlog_dump_write,
};

/**
 * sde_dbg_ctrl_read - debugfs read handler for debug ctrl read
 * @file: file handler
 * @buff: user buffer content for debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_dbg_ctrl_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char buf[24] = {'\0'};

	if (!buff || !ppos)
		return -EINVAL;

	if (*ppos)
		return 0;	/* the end */

	len = snprintf(buf, sizeof(buf), "0x%x\n", sde_dbg_base.debugfs_ctrl);
	pr_debug("%s: ctrl:0x%x len:0x%zx\n",
		__func__, sde_dbg_base.debugfs_ctrl, len);

	if ((count < sizeof(buf)) || copy_to_user(buff, buf, len)) {
		pr_err("error copying the buffer! count:0x%zx\n", count);
		return -EFAULT;
	}

	*ppos += len;	/* increase offset */
	return len;
}

/**
 * sde_dbg_ctrl_write - debugfs read handler for debug ctrl write
 * @file: file handler
 * @user_buf: user buffer content from debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_dbg_ctrl_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	u32 dbg_ctrl = 0;
	char buf[24];

	if (!file) {
		pr_err("DbgDbg: %s: error no file --\n", __func__);
		return -EINVAL;
	}

	if (count >= sizeof(buf))
		return -EFAULT;


	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0; /* end of string */

	if (kstrtouint(buf, 0, &dbg_ctrl)) {
		pr_err("%s: error in the number of bytes\n", __func__);
		return -EFAULT;
	}

	pr_debug("dbg_ctrl_read:0x%x\n", dbg_ctrl);
	sde_dbg_base.debugfs_ctrl = dbg_ctrl;

	return count;
}

static const struct file_operations sde_dbg_ctrl_fops = {
	.open = sde_dbg_debugfs_open,
	.read = sde_dbg_ctrl_read,
	.write = sde_dbg_ctrl_write,
};

void sde_dbg_init_dbg_buses(u32 hwversion)
{
	static struct sde_dbg_base *dbg = &sde_dbg_base;
	char debug_name[80] = "";

	memset(&dbg->dbgbus_sde, 0, sizeof(dbg->dbgbus_sde));
	memset(&dbg->dbgbus_vbif_rt, 0, sizeof(dbg->dbgbus_vbif_rt));

	switch (hwversion) {
	case SDE_HW_VER_300:
	case SDE_HW_VER_301:
		dbg->dbgbus_sde.entries = dbg_bus_sde_8998;
		dbg->dbgbus_sde.cmn.entries_size = ARRAY_SIZE(dbg_bus_sde_8998);
		dbg->dbgbus_sde.cmn.flags = DBGBUS_FLAGS_DSPP;

		dbg->dbgbus_vbif_rt.entries = vbif_dbg_bus_msm8998;
		dbg->dbgbus_vbif_rt.cmn.entries_size =
				ARRAY_SIZE(vbif_dbg_bus_msm8998);
		break;
	default:
		pr_err("unsupported chipset id %u\n", hwversion);
		break;
	}

	if (dbg->dbgbus_sde.entries) {
		dbg->dbgbus_sde.cmn.name = DBGBUS_NAME_SDE;
		snprintf(debug_name, sizeof(debug_name), "%s_dbgbus",
				dbg->dbgbus_sde.cmn.name);
		dbg->dbgbus_sde.cmn.enable_mask = DEFAULT_DBGBUS_SDE;
		debugfs_create_u32(debug_name, 0600, dbg->root,
				&dbg->dbgbus_sde.cmn.enable_mask);
	}

	if (dbg->dbgbus_vbif_rt.entries) {
		dbg->dbgbus_vbif_rt.cmn.name = DBGBUS_NAME_VBIF_RT;
		snprintf(debug_name, sizeof(debug_name), "%s_dbgbus",
				dbg->dbgbus_vbif_rt.cmn.name);
		dbg->dbgbus_vbif_rt.cmn.enable_mask = DEFAULT_DBGBUS_VBIFRT;
		debugfs_create_u32(debug_name, 0600, dbg->root,
				&dbg->dbgbus_vbif_rt.cmn.enable_mask);
	}
}

int sde_dbg_init(struct dentry *debugfs_root, struct device *dev,
		struct sde_dbg_power_ctrl *power_ctrl)
{
	int i;

	mutex_init(&sde_dbg_base.mutex);
	INIT_LIST_HEAD(&sde_dbg_base.reg_base_list);
	sde_dbg_base.dev = dev;
	sde_dbg_base.power_ctrl = *power_ctrl;


	sde_dbg_base.evtlog = sde_evtlog_init();
	if (IS_ERR_OR_NULL(sde_dbg_base.evtlog))
		return PTR_ERR(sde_dbg_base.evtlog);

	sde_dbg_base_evtlog = sde_dbg_base.evtlog;

	sde_dbg_base.root = debugfs_create_dir("evt_dbg", debugfs_root);
	if (IS_ERR_OR_NULL(sde_dbg_base.root)) {
		pr_err("debugfs_create_dir fail, error %ld\n",
		       PTR_ERR(sde_dbg_base.root));
		sde_dbg_base.root = NULL;
		return -ENODEV;
	}

	INIT_WORK(&sde_dbg_base.dump_work, _sde_dump_work);
	sde_dbg_base.work_panic = false;

	for (i = 0; i < SDE_EVTLOG_ENTRY; i++)
		sde_dbg_base.evtlog->logs[i].counter = i;

	debugfs_create_file("dbg_ctrl", 0600, sde_dbg_base.root, NULL,
			&sde_dbg_ctrl_fops);
	debugfs_create_file("dump", 0600, sde_dbg_base.root, NULL,
						&sde_evtlog_fops);
	debugfs_create_u32("enable", 0600, sde_dbg_base.root,
			&(sde_dbg_base.evtlog->enable));
	debugfs_create_u32("panic", 0600, sde_dbg_base.root,
			&sde_dbg_base.panic_on_err);
	debugfs_create_u32("reg_dump", 0600, sde_dbg_base.root,
			&sde_dbg_base.enable_reg_dump);

	sde_dbg_base.panic_on_err = DEFAULT_PANIC;
	sde_dbg_base.enable_reg_dump = DEFAULT_REGDUMP;

	pr_info("evtlog_status: enable:%d, panic:%d, dump:%d\n",
		sde_dbg_base.evtlog->enable, sde_dbg_base.panic_on_err,
		sde_dbg_base.enable_reg_dump);

	return 0;
}

/**
 * sde_dbg_destroy - destroy sde debug facilities
 */
void sde_dbg_destroy(void)
{
	debugfs_remove_recursive(sde_dbg_base.root);
	sde_dbg_base.root = NULL;

	sde_dbg_base_evtlog = NULL;
	sde_evtlog_destroy(sde_dbg_base.evtlog);
	sde_dbg_base.evtlog = NULL;
	mutex_destroy(&sde_dbg_base.mutex);
}

/**
 * sde_dbg_reg_base_release - release allocated reg dump file private data
 * @inode: debugfs inode
 * @file: file handle
 * @Return: 0 on success
 */
static int sde_dbg_reg_base_release(struct inode *inode, struct file *file)
{
	struct sde_dbg_reg_base *dbg;

	if (!file)
		return -EINVAL;

	dbg = file->private_data;
	if (!dbg)
		return -ENODEV;

	mutex_lock(&sde_dbg_base.mutex);
	if (dbg && dbg->buf) {
		kfree(dbg->buf);
		dbg->buf_len = 0;
		dbg->buf = NULL;
	}
	mutex_unlock(&sde_dbg_base.mutex);

	return 0;
}

/**
 * sde_dbg_reg_base_is_valid_range - verify if requested memory range is valid
 * @off: address offset in bytes
 * @cnt: memory size in bytes
 * Return: true if valid; false otherwise
 */
static bool sde_dbg_reg_base_is_valid_range(u32 off, u32 cnt)
{
	static struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_range *node;
	struct sde_dbg_reg_base *base;

	pr_debug("check offset=0x%x cnt=0x%x\n", off, cnt);

	list_for_each_entry(base, &dbg_base->reg_base_list, reg_base_head) {
		list_for_each_entry(node, &base->sub_range_list, head) {
			pr_debug("%s: start=0x%x end=0x%x\n", node->range_name,
					node->offset.start, node->offset.end);

			if (node->offset.start <= off
					&& off <= node->offset.end
					&& off + cnt <= node->offset.end) {
				pr_debug("valid range requested\n");
				return true;
			}
		}
	}

	pr_err("invalid range requested\n");
	return false;
}

/**
 * sde_dbg_reg_base_offset_write - set new offset and len to debugfs reg base
 * @file: file handler
 * @user_buf: user buffer content from debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_dbg_reg_base_offset_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_dbg_reg_base *dbg;
	u32 off = 0;
	u32 cnt = DEFAULT_BASE_REG_CNT;
	char buf[24];
	ssize_t rc = count;

	if (!file)
		return -EINVAL;

	dbg = file->private_data;
	if (!dbg)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	if (sscanf(buf, "%x %x", &off, &cnt) != 2)
		return -EFAULT;

	mutex_lock(&sde_dbg_base.mutex);
	if (off > dbg->max_offset) {
		rc = -EINVAL;
		goto exit;
	}

	if (off % sizeof(u32)) {
		rc = -EINVAL;
		goto exit;
	}

	if (cnt > (dbg->max_offset - off))
		cnt = dbg->max_offset - off;

	if (cnt % sizeof(u32)) {
		rc = -EINVAL;
		goto exit;
	}

	if (cnt == 0) {
		rc = -EINVAL;
		goto exit;
	}

	if (!sde_dbg_reg_base_is_valid_range(off, cnt)) {
		rc = -EINVAL;
		goto exit;
	}

	dbg->off = off;
	dbg->cnt = cnt;

exit:
	mutex_unlock(&sde_dbg_base.mutex);
	pr_debug("offset=%x cnt=%x\n", off, cnt);

	return rc;
}

/**
 * sde_dbg_reg_base_offset_read - read current offset and len of register base
 * @file: file handler
 * @user_buf: user buffer content from debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_dbg_reg_base_offset_read(struct file *file,
			char __user *buff, size_t count, loff_t *ppos)
{
	struct sde_dbg_reg_base *dbg;
	int len = 0;
	char buf[24] = {'\0'};

	if (!file)
		return -EINVAL;

	dbg = file->private_data;
	if (!dbg)
		return -ENODEV;

	if (!ppos)
		return -EINVAL;

	if (*ppos)
		return 0;	/* the end */

	mutex_lock(&sde_dbg_base.mutex);
	if (dbg->off % sizeof(u32)) {
		mutex_unlock(&sde_dbg_base.mutex);
		return -EFAULT;
	}

	len = snprintf(buf, sizeof(buf), "0x%08zx %zx\n", dbg->off, dbg->cnt);
	if (len < 0 || len >= sizeof(buf)) {
		mutex_unlock(&sde_dbg_base.mutex);
		return 0;
	}

	if ((count < sizeof(buf)) || copy_to_user(buff, buf, len)) {
		mutex_unlock(&sde_dbg_base.mutex);
		return -EFAULT;
	}

	*ppos += len;	/* increase offset */
	mutex_unlock(&sde_dbg_base.mutex);

	return len;
}

/**
 * sde_dbg_reg_base_reg_write - write to reg base hw at offset a given value
 * @file: file handler
 * @user_buf: user buffer content from debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_dbg_reg_base_reg_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_dbg_reg_base *dbg;
	size_t off;
	u32 data, cnt;
	char buf[24];

	if (!file)
		return -EINVAL;

	dbg = file->private_data;
	if (!dbg)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	cnt = sscanf(buf, "%zx %x", &off, &data);

	if (cnt < 2)
		return -EFAULT;

	mutex_lock(&sde_dbg_base.mutex);
	if (off >= dbg->max_offset) {
		mutex_unlock(&sde_dbg_base.mutex);
		return -EFAULT;
	}

	_sde_dbg_enable_power(true);

	writel_relaxed(data, dbg->base + off);

	_sde_dbg_enable_power(false);

	mutex_unlock(&sde_dbg_base.mutex);

	pr_debug("addr=%zx data=%x\n", off, data);

	return count;
}

/**
 * sde_dbg_reg_base_reg_read - read len from reg base hw at current offset
 * @file: file handler
 * @user_buf: user buffer content from debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_dbg_reg_base_reg_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_dbg_reg_base *dbg;
	size_t len;

	if (!file)
		return -EINVAL;

	dbg = file->private_data;
	if (!dbg) {
		pr_err("invalid handle\n");
		return -ENODEV;
	}

	if (!ppos)
		return -EINVAL;

	mutex_lock(&sde_dbg_base.mutex);
	if (!dbg->buf) {
		char *hwbuf;
		char dump_buf[64];
		char __iomem *ioptr;
		int cnt, tot;

		dbg->buf_len = sizeof(dump_buf) *
			DIV_ROUND_UP(dbg->cnt, ROW_BYTES);

		if (dbg->buf_len % sizeof(u32))
			return -EINVAL;

		dbg->buf = kzalloc(dbg->buf_len, GFP_KERNEL);

		if (!dbg->buf) {
			mutex_unlock(&sde_dbg_base.mutex);
			return -ENOMEM;
		}

		hwbuf = kzalloc(ROW_BYTES, GFP_KERNEL);
		if (!hwbuf) {
			kfree(dbg->buf);
			mutex_unlock(&sde_dbg_base.mutex);
			return -ENOMEM;
		}

		ioptr = dbg->base + dbg->off;
		tot = 0;
		_sde_dbg_enable_power(true);

		for (cnt = dbg->cnt; cnt > 0; cnt -= ROW_BYTES) {
			memcpy_fromio(hwbuf, ioptr, ROW_BYTES);
			hex_dump_to_buffer(hwbuf,
					   min(cnt, ROW_BYTES),
					   ROW_BYTES, GROUP_BYTES, dump_buf,
					   sizeof(dump_buf), false);
			len = scnprintf(dbg->buf + tot, dbg->buf_len - tot,
					"0x%08x: %s\n",
					((int) (unsigned long) ioptr) -
					((int) (unsigned long) dbg->base),
					dump_buf);

			ioptr += ROW_BYTES;
			tot += len;
			if (tot >= dbg->buf_len)
				break;
		}

		_sde_dbg_enable_power(false);

		dbg->buf_len = tot;
		kfree(hwbuf);
	}

	if (*ppos >= dbg->buf_len) {
		mutex_unlock(&sde_dbg_base.mutex);
		return 0; /* done reading */
	}

	len = min(count, dbg->buf_len - (size_t) *ppos);
	if (copy_to_user(user_buf, dbg->buf + *ppos, len)) {
		mutex_unlock(&sde_dbg_base.mutex);
		pr_err("failed to copy to user\n");
		return -EFAULT;
	}

	*ppos += len; /* increase offset */
	mutex_unlock(&sde_dbg_base.mutex);

	return len;
}

static const struct file_operations sde_off_fops = {
	.open = sde_dbg_debugfs_open,
	.release = sde_dbg_reg_base_release,
	.read = sde_dbg_reg_base_offset_read,
	.write = sde_dbg_reg_base_offset_write,
};

static const struct file_operations sde_reg_fops = {
	.open = sde_dbg_debugfs_open,
	.release = sde_dbg_reg_base_release,
	.read = sde_dbg_reg_base_reg_read,
	.write = sde_dbg_reg_base_reg_write,
};

int sde_dbg_reg_register_base(const char *name, void __iomem *base,
		size_t max_offset)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *reg_base;
	struct dentry *ent_off, *ent_reg;
	char dn[80] = "";
	int prefix_len = 0;

	reg_base = kzalloc(sizeof(*reg_base), GFP_KERNEL);
	if (!reg_base)
		return -ENOMEM;

	if (name)
		strlcpy(reg_base->name, name, sizeof(reg_base->name));
	reg_base->base = base;
	reg_base->max_offset = max_offset;
	reg_base->off = 0;
	reg_base->cnt = DEFAULT_BASE_REG_CNT;
	reg_base->reg_dump = NULL;

	if (name)
		prefix_len = snprintf(dn, sizeof(dn), "%s_", name);
	strlcpy(dn + prefix_len, "off", sizeof(dn) - prefix_len);
	ent_off = debugfs_create_file(dn, 0600, dbg_base->root, reg_base,
			&sde_off_fops);
	if (IS_ERR_OR_NULL(ent_off)) {
		pr_err("debugfs_create_file: offset fail\n");
		goto off_fail;
	}

	strlcpy(dn + prefix_len, "reg", sizeof(dn) - prefix_len);
	ent_reg = debugfs_create_file(dn, 0600, dbg_base->root, reg_base,
			&sde_reg_fops);
	if (IS_ERR_OR_NULL(ent_reg)) {
		pr_err("debugfs_create_file: reg fail\n");
		goto reg_fail;
	}

	/* Initialize list to make sure check for null list will be valid */
	INIT_LIST_HEAD(&reg_base->sub_range_list);

	pr_debug("%s base: %pK max_offset 0x%zX\n", reg_base->name,
			reg_base->base, reg_base->max_offset);

	list_add(&reg_base->reg_base_head, &dbg_base->reg_base_list);

	return 0;
reg_fail:
	debugfs_remove(ent_off);
off_fail:
	kfree(reg_base);
	return -ENODEV;
}

void sde_dbg_reg_register_dump_range(const char *base_name,
		const char *range_name, u32 offset_start, u32 offset_end,
		uint32_t xin_id)
{
	struct sde_dbg_reg_base *reg_base;
	struct sde_dbg_reg_range *range;

	reg_base = _sde_dump_get_blk_addr(base_name);
	if (!reg_base) {
		pr_err("error: for range %s unable to locate base %s\n",
				range_name, base_name);
		return;
	}

	if (!range_name || strlen(range_name) == 0) {
		pr_err("%pS: bad range name, base_name %s, offset_start 0x%X, end 0x%X\n",
				__builtin_return_address(0), base_name,
				offset_start, offset_end);
		return;
	}

	if (offset_end - offset_start < REG_DUMP_ALIGN ||
			offset_start > offset_end) {
		pr_err("%pS: bad range, base_name %s, range_name %s, offset_start 0x%X, end 0x%X\n",
				__builtin_return_address(0), base_name,
				range_name, offset_start, offset_end);
		return;
	}

	range = kzalloc(sizeof(*range), GFP_KERNEL);
	if (!range)
		return;

	strlcpy(range->range_name, range_name, sizeof(range->range_name));
	range->offset.start = offset_start;
	range->offset.end = offset_end;
	range->xin_id = xin_id;
	list_add_tail(&range->head, &reg_base->sub_range_list);

	pr_debug("base %s, range %s, start 0x%X, end 0x%X\n",
			base_name, range->range_name,
			range->offset.start, range->offset.end);
}

void sde_dbg_set_sde_top_offset(u32 blk_off)
{
	sde_dbg_base.dbgbus_sde.top_blk_off = blk_off;
}
