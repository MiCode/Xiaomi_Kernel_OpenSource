/* Copyright (c) 2009-2019, The Linux Foundation. All rights reserved.
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
#define DBGBUS_AXI_INTF	0x194
#define DBGBUS_SSPP1	0x298
#define DBGBUS_DSPP	0x348
#define DBGBUS_PERIPH	0x418

#define TEST_MASK(id, tp)	((id << 4) | (tp << 1) | BIT(0))

/* following offsets are with respect to MDP VBIF base for DBG BUS access */
#define MMSS_VBIF_CLKON			0x4
#define MMSS_VBIF_TEST_BUS_OUT_CTRL	0x210
#define MMSS_VBIF_TEST_BUS_OUT		0x230

/* Vbif error info */
#define MMSS_VBIF_PND_ERR		0x190
#define MMSS_VBIF_SRC_ERR		0x194
#define MMSS_VBIF_XIN_HALT_CTRL1	0x204
#define MMSS_VBIF_ERR_INFO		0X1a0
#define MMSS_VBIF_ERR_INFO_1		0x1a4
#define MMSS_VBIF_CLIENT_NUM		14

/* print debug ranges in groups of 4 u32s */
#define REG_DUMP_ALIGN		16

#define RSC_DEBUG_MUX_SEL_SDM845 9

#define DBG_CTRL_STOP_FTRACE	BIT(0)
#define DBG_CTRL_PANIC_UNDERRUN	BIT(1)
#define DBG_CTRL_RESET_HW_PANIC	BIT(2)
#define DBG_CTRL_MAX			BIT(3)

#define DUMP_BUF_SIZE			(4096 * 512)
#define DUMP_CLMN_COUNT			4
#define DUMP_LINE_SIZE			256
#define DUMP_MAX_LINES_PER_BLK		512

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
 * @cb: callback for external dump function, null if not defined
 * @cb_ptr: private pointer to callback function
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
	void (*cb)(void *ptr);
	void *cb_ptr;
};

struct sde_debug_bus_entry {
	u32 wr_addr;
	u32 block_id;
	u32 test_id;
	void (*analyzer)(void __iomem *mem_base,
				struct sde_debug_bus_entry *entry, u32 val);
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

struct sde_dbg_dsi_debug_bus {
	u32 *entries;
	u32 size;
};

/**
 * struct sde_dbg_regbuf - wraps buffer and tracking params for register dumps
 * @buf: pointer to allocated memory for storing register dumps in hw recovery
 * @buf_size: size of the memory allocated
 * @len: size of the dump data valid in the buffer
 * @rpos: cursor points to the buffer position read by client
 * @dump_done: to indicate if dumping to user memory is complete
 * @cur_blk: points to the current sde_dbg_reg_base block
 */
struct sde_dbg_regbuf {
	char *buf;
	int buf_size;
	int len;
	int rpos;
	int dump_done;
	struct sde_dbg_reg_base *cur_blk;
};

/**
 * struct sde_dbg_base - global sde debug base structure
 * @evtlog: event log instance
 * @reg_base_list: list of register dumping regions
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
 * @dsi_dbg_bus: dump dsi debug bus register
 * @regbuf: buffer data to track the register dumping in hw recovery
 * @cur_evt_index: index used for tracking event logs dump in hw recovery
 * @dbgbus_dump_idx: index used for tracking dbg-bus dump in hw recovery
 * @vbif_dbgbus_dump_idx: index for tracking vbif dumps in hw recovery
 */
static struct sde_dbg_base {
	struct sde_dbg_evtlog *evtlog;
	struct list_head reg_base_list;
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
	struct sde_dbg_dsi_debug_bus dbgbus_dsi;
	bool dump_all;
	bool dump_secure;
	bool dsi_dbg_bus;
	u32 debugfs_ctrl;

	struct sde_dbg_regbuf regbuf;
	u32 cur_evt_index;
	u32 dbgbus_dump_idx;
	u32 vbif_dbgbus_dump_idx;
	enum sde_dbg_dump_context dump_mode;
} sde_dbg_base;

/* sde_dbg_base_evtlog - global pointer to main sde event log for macro use */
struct sde_dbg_evtlog *sde_dbg_base_evtlog;

static void _sde_debug_bus_xbar_dump(void __iomem *mem_base,
		struct sde_debug_bus_entry *entry, u32 val)
{
	dev_err(sde_dbg_base.dev, "xbar 0x%x %d %d 0x%x\n",
			entry->wr_addr, entry->block_id, entry->test_id, val);
}

static void _sde_debug_bus_lm_dump(void __iomem *mem_base,
		struct sde_debug_bus_entry *entry, u32 val)
{
	if (!(val & 0xFFF000))
		return;

	dev_err(sde_dbg_base.dev, "lm 0x%x %d %d 0x%x\n",
			entry->wr_addr, entry->block_id, entry->test_id, val);
}

static void _sde_debug_bus_ppb0_dump(void __iomem *mem_base,
		struct sde_debug_bus_entry *entry, u32 val)
{
	if (!(val & BIT(15)))
		return;

	dev_err(sde_dbg_base.dev, "ppb0 0x%x %d %d 0x%x\n",
			entry->wr_addr, entry->block_id, entry->test_id, val);
}

static void _sde_debug_bus_ppb1_dump(void __iomem *mem_base,
		struct sde_debug_bus_entry *entry, u32 val)
{
	if (!(val & BIT(15)))
		return;

	dev_err(sde_dbg_base.dev, "ppb1 0x%x %d %d 0x%x\n",
			entry->wr_addr, entry->block_id, entry->test_id, val);
}

static void _sde_debug_bus_axi_dump_sdm845(void __iomem *mem_base,
		struct sde_debug_bus_entry *entry, u32 val)
{
	u32 status, i;

	if (!mem_base || !entry)
		return;

	for (i = 0; i <= RSC_DEBUG_MUX_SEL_SDM845; i++) {
		sde_rsc_debug_dump(i);

		/* make sure that mux_sel updated */
		wmb();

		/* read status again after rsc routes the debug bus */
		status = readl_relaxed(mem_base + DBGBUS_DSPP_STATUS);

		dev_err(sde_dbg_base.dev, "rsc mux_sel:%d 0x%x %d %d 0x%x\n",
			i, entry->wr_addr, entry->block_id,
			entry->test_id, status);
	}
}

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
	{ DBGBUS_DSPP, 31, 0, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, 33, 0, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, 35, 0, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, 42, 0, _sde_debug_bus_ppb0_dump },

	/* ppb_1 */
	{ DBGBUS_DSPP, 32, 0, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, 34, 0, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, 36, 0, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, 43, 0, _sde_debug_bus_ppb1_dump },

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
	{ DBGBUS_DSPP, 0, 0, _sde_debug_bus_xbar_dump },

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
	{ DBGBUS_DSPP, 63, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 64, 0},
	{ DBGBUS_DSPP, 64, 1},
	{ DBGBUS_DSPP, 64, 2},
	{ DBGBUS_DSPP, 64, 3},
	{ DBGBUS_DSPP, 64, 4},
	{ DBGBUS_DSPP, 64, 5},
	{ DBGBUS_DSPP, 64, 6},
	{ DBGBUS_DSPP, 64, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 65, 0},
	{ DBGBUS_DSPP, 65, 1},
	{ DBGBUS_DSPP, 65, 2},
	{ DBGBUS_DSPP, 65, 3},
	{ DBGBUS_DSPP, 65, 4},
	{ DBGBUS_DSPP, 65, 5},
	{ DBGBUS_DSPP, 65, 6},
	{ DBGBUS_DSPP, 65, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 66, 0},
	{ DBGBUS_DSPP, 66, 1},
	{ DBGBUS_DSPP, 66, 2},
	{ DBGBUS_DSPP, 66, 3},
	{ DBGBUS_DSPP, 66, 4},
	{ DBGBUS_DSPP, 66, 5},
	{ DBGBUS_DSPP, 66, 6},
	{ DBGBUS_DSPP, 66, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 67, 0},
	{ DBGBUS_DSPP, 67, 1},
	{ DBGBUS_DSPP, 67, 2},
	{ DBGBUS_DSPP, 67, 3},
	{ DBGBUS_DSPP, 67, 4},
	{ DBGBUS_DSPP, 67, 5},
	{ DBGBUS_DSPP, 67, 6},
	{ DBGBUS_DSPP, 67, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 68, 0},
	{ DBGBUS_DSPP, 68, 1},
	{ DBGBUS_DSPP, 68, 2},
	{ DBGBUS_DSPP, 68, 3},
	{ DBGBUS_DSPP, 68, 4},
	{ DBGBUS_DSPP, 68, 5},
	{ DBGBUS_DSPP, 68, 6},
	{ DBGBUS_DSPP, 68, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 69, 0},
	{ DBGBUS_DSPP, 69, 1},
	{ DBGBUS_DSPP, 69, 2},
	{ DBGBUS_DSPP, 69, 3},
	{ DBGBUS_DSPP, 69, 4},
	{ DBGBUS_DSPP, 69, 5},
	{ DBGBUS_DSPP, 69, 6},
	{ DBGBUS_DSPP, 69, 7, _sde_debug_bus_lm_dump },

	/* LM1 */
	{ DBGBUS_DSPP, 70, 0},
	{ DBGBUS_DSPP, 70, 1},
	{ DBGBUS_DSPP, 70, 2},
	{ DBGBUS_DSPP, 70, 3},
	{ DBGBUS_DSPP, 70, 4},
	{ DBGBUS_DSPP, 70, 5},
	{ DBGBUS_DSPP, 70, 6},
	{ DBGBUS_DSPP, 70, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 71, 0},
	{ DBGBUS_DSPP, 71, 1},
	{ DBGBUS_DSPP, 71, 2},
	{ DBGBUS_DSPP, 71, 3},
	{ DBGBUS_DSPP, 71, 4},
	{ DBGBUS_DSPP, 71, 5},
	{ DBGBUS_DSPP, 71, 6},
	{ DBGBUS_DSPP, 71, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 72, 0},
	{ DBGBUS_DSPP, 72, 1},
	{ DBGBUS_DSPP, 72, 2},
	{ DBGBUS_DSPP, 72, 3},
	{ DBGBUS_DSPP, 72, 4},
	{ DBGBUS_DSPP, 72, 5},
	{ DBGBUS_DSPP, 72, 6},
	{ DBGBUS_DSPP, 72, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 73, 0},
	{ DBGBUS_DSPP, 73, 1},
	{ DBGBUS_DSPP, 73, 2},
	{ DBGBUS_DSPP, 73, 3},
	{ DBGBUS_DSPP, 73, 4},
	{ DBGBUS_DSPP, 73, 5},
	{ DBGBUS_DSPP, 73, 6},
	{ DBGBUS_DSPP, 73, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 74, 0},
	{ DBGBUS_DSPP, 74, 1},
	{ DBGBUS_DSPP, 74, 2},
	{ DBGBUS_DSPP, 74, 3},
	{ DBGBUS_DSPP, 74, 4},
	{ DBGBUS_DSPP, 74, 5},
	{ DBGBUS_DSPP, 74, 6},
	{ DBGBUS_DSPP, 74, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 75, 0},
	{ DBGBUS_DSPP, 75, 1},
	{ DBGBUS_DSPP, 75, 2},
	{ DBGBUS_DSPP, 75, 3},
	{ DBGBUS_DSPP, 75, 4},
	{ DBGBUS_DSPP, 75, 5},
	{ DBGBUS_DSPP, 75, 6},
	{ DBGBUS_DSPP, 75, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 76, 0},
	{ DBGBUS_DSPP, 76, 1},
	{ DBGBUS_DSPP, 76, 2},
	{ DBGBUS_DSPP, 76, 3},
	{ DBGBUS_DSPP, 76, 4},
	{ DBGBUS_DSPP, 76, 5},
	{ DBGBUS_DSPP, 76, 6},
	{ DBGBUS_DSPP, 76, 7, _sde_debug_bus_lm_dump },

	/* LM2 */
	{ DBGBUS_DSPP, 77, 0},
	{ DBGBUS_DSPP, 77, 1},
	{ DBGBUS_DSPP, 77, 2},
	{ DBGBUS_DSPP, 77, 3},
	{ DBGBUS_DSPP, 77, 4},
	{ DBGBUS_DSPP, 77, 5},
	{ DBGBUS_DSPP, 77, 6},
	{ DBGBUS_DSPP, 77, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 78, 0},
	{ DBGBUS_DSPP, 78, 1},
	{ DBGBUS_DSPP, 78, 2},
	{ DBGBUS_DSPP, 78, 3},
	{ DBGBUS_DSPP, 78, 4},
	{ DBGBUS_DSPP, 78, 5},
	{ DBGBUS_DSPP, 78, 6},
	{ DBGBUS_DSPP, 78, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 79, 0},
	{ DBGBUS_DSPP, 79, 1},
	{ DBGBUS_DSPP, 79, 2},
	{ DBGBUS_DSPP, 79, 3},
	{ DBGBUS_DSPP, 79, 4},
	{ DBGBUS_DSPP, 79, 5},
	{ DBGBUS_DSPP, 79, 6},
	{ DBGBUS_DSPP, 79, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 80, 0},
	{ DBGBUS_DSPP, 80, 1},
	{ DBGBUS_DSPP, 80, 2},
	{ DBGBUS_DSPP, 80, 3},
	{ DBGBUS_DSPP, 80, 4},
	{ DBGBUS_DSPP, 80, 5},
	{ DBGBUS_DSPP, 80, 6},
	{ DBGBUS_DSPP, 80, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 81, 0},
	{ DBGBUS_DSPP, 81, 1},
	{ DBGBUS_DSPP, 81, 2},
	{ DBGBUS_DSPP, 81, 3},
	{ DBGBUS_DSPP, 81, 4},
	{ DBGBUS_DSPP, 81, 5},
	{ DBGBUS_DSPP, 81, 6},
	{ DBGBUS_DSPP, 81, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 82, 0},
	{ DBGBUS_DSPP, 82, 1},
	{ DBGBUS_DSPP, 82, 2},
	{ DBGBUS_DSPP, 82, 3},
	{ DBGBUS_DSPP, 82, 4},
	{ DBGBUS_DSPP, 82, 5},
	{ DBGBUS_DSPP, 82, 6},
	{ DBGBUS_DSPP, 82, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 83, 0},
	{ DBGBUS_DSPP, 83, 1},
	{ DBGBUS_DSPP, 83, 2},
	{ DBGBUS_DSPP, 83, 3},
	{ DBGBUS_DSPP, 83, 4},
	{ DBGBUS_DSPP, 83, 5},
	{ DBGBUS_DSPP, 83, 6},
	{ DBGBUS_DSPP, 83, 7, _sde_debug_bus_lm_dump },

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

static struct sde_debug_bus_entry dbg_bus_sde_sdm845[] = {

	/* Unpack 0 sspp 0*/
	{ DBGBUS_SSPP0, 50, 2 },
	{ DBGBUS_SSPP0, 60, 2 },
	{ DBGBUS_SSPP0, 70, 2 },

	/* Upack 0 sspp 1*/
	{ DBGBUS_SSPP1, 50, 2 },
	{ DBGBUS_SSPP1, 60, 2 },
	{ DBGBUS_SSPP1, 70, 2 },

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
	{ DBGBUS_DSPP, 31, 0, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, 33, 0, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, 35, 0, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, 42, 0, _sde_debug_bus_ppb0_dump },

	/* ppb_1 */
	{ DBGBUS_DSPP, 32, 0, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, 34, 0, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, 36, 0, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, 43, 0, _sde_debug_bus_ppb1_dump },

	/* lm_lut */
	{ DBGBUS_DSPP, 109, 0 },
	{ DBGBUS_DSPP, 105, 0 },
	{ DBGBUS_DSPP, 103, 0 },

	/* crossbar */
	{ DBGBUS_DSPP, 0, 0, _sde_debug_bus_xbar_dump },

	/* rotator */
	{ DBGBUS_DSPP, 9, 0},

	/* blend */
	/* LM0 */
	{ DBGBUS_DSPP, 63, 1},
	{ DBGBUS_DSPP, 63, 2},
	{ DBGBUS_DSPP, 63, 3},
	{ DBGBUS_DSPP, 63, 4},
	{ DBGBUS_DSPP, 63, 5},
	{ DBGBUS_DSPP, 63, 6},
	{ DBGBUS_DSPP, 63, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 64, 1},
	{ DBGBUS_DSPP, 64, 2},
	{ DBGBUS_DSPP, 64, 3},
	{ DBGBUS_DSPP, 64, 4},
	{ DBGBUS_DSPP, 64, 5},
	{ DBGBUS_DSPP, 64, 6},
	{ DBGBUS_DSPP, 64, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 65, 1},
	{ DBGBUS_DSPP, 65, 2},
	{ DBGBUS_DSPP, 65, 3},
	{ DBGBUS_DSPP, 65, 4},
	{ DBGBUS_DSPP, 65, 5},
	{ DBGBUS_DSPP, 65, 6},
	{ DBGBUS_DSPP, 65, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 66, 1},
	{ DBGBUS_DSPP, 66, 2},
	{ DBGBUS_DSPP, 66, 3},
	{ DBGBUS_DSPP, 66, 4},
	{ DBGBUS_DSPP, 66, 5},
	{ DBGBUS_DSPP, 66, 6},
	{ DBGBUS_DSPP, 66, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 67, 1},
	{ DBGBUS_DSPP, 67, 2},
	{ DBGBUS_DSPP, 67, 3},
	{ DBGBUS_DSPP, 67, 4},
	{ DBGBUS_DSPP, 67, 5},
	{ DBGBUS_DSPP, 67, 6},
	{ DBGBUS_DSPP, 67, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 68, 1},
	{ DBGBUS_DSPP, 68, 2},
	{ DBGBUS_DSPP, 68, 3},
	{ DBGBUS_DSPP, 68, 4},
	{ DBGBUS_DSPP, 68, 5},
	{ DBGBUS_DSPP, 68, 6},
	{ DBGBUS_DSPP, 68, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 69, 1},
	{ DBGBUS_DSPP, 69, 2},
	{ DBGBUS_DSPP, 69, 3},
	{ DBGBUS_DSPP, 69, 4},
	{ DBGBUS_DSPP, 69, 5},
	{ DBGBUS_DSPP, 69, 6},
	{ DBGBUS_DSPP, 69, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 84, 1},
	{ DBGBUS_DSPP, 84, 2},
	{ DBGBUS_DSPP, 84, 3},
	{ DBGBUS_DSPP, 84, 4},
	{ DBGBUS_DSPP, 84, 5},
	{ DBGBUS_DSPP, 84, 6},
	{ DBGBUS_DSPP, 84, 7, _sde_debug_bus_lm_dump },


	{ DBGBUS_DSPP, 85, 1},
	{ DBGBUS_DSPP, 85, 2},
	{ DBGBUS_DSPP, 85, 3},
	{ DBGBUS_DSPP, 85, 4},
	{ DBGBUS_DSPP, 85, 5},
	{ DBGBUS_DSPP, 85, 6},
	{ DBGBUS_DSPP, 85, 7, _sde_debug_bus_lm_dump },


	{ DBGBUS_DSPP, 86, 1},
	{ DBGBUS_DSPP, 86, 2},
	{ DBGBUS_DSPP, 86, 3},
	{ DBGBUS_DSPP, 86, 4},
	{ DBGBUS_DSPP, 86, 5},
	{ DBGBUS_DSPP, 86, 6},
	{ DBGBUS_DSPP, 86, 7, _sde_debug_bus_lm_dump },


	{ DBGBUS_DSPP, 87, 1},
	{ DBGBUS_DSPP, 87, 2},
	{ DBGBUS_DSPP, 87, 3},
	{ DBGBUS_DSPP, 87, 4},
	{ DBGBUS_DSPP, 87, 5},
	{ DBGBUS_DSPP, 87, 6},
	{ DBGBUS_DSPP, 87, 7, _sde_debug_bus_lm_dump },

	/* LM1 */
	{ DBGBUS_DSPP, 70, 1},
	{ DBGBUS_DSPP, 70, 2},
	{ DBGBUS_DSPP, 70, 3},
	{ DBGBUS_DSPP, 70, 4},
	{ DBGBUS_DSPP, 70, 5},
	{ DBGBUS_DSPP, 70, 6},
	{ DBGBUS_DSPP, 70, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 71, 1},
	{ DBGBUS_DSPP, 71, 2},
	{ DBGBUS_DSPP, 71, 3},
	{ DBGBUS_DSPP, 71, 4},
	{ DBGBUS_DSPP, 71, 5},
	{ DBGBUS_DSPP, 71, 6},
	{ DBGBUS_DSPP, 71, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 72, 1},
	{ DBGBUS_DSPP, 72, 2},
	{ DBGBUS_DSPP, 72, 3},
	{ DBGBUS_DSPP, 72, 4},
	{ DBGBUS_DSPP, 72, 5},
	{ DBGBUS_DSPP, 72, 6},
	{ DBGBUS_DSPP, 72, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 73, 1},
	{ DBGBUS_DSPP, 73, 2},
	{ DBGBUS_DSPP, 73, 3},
	{ DBGBUS_DSPP, 73, 4},
	{ DBGBUS_DSPP, 73, 5},
	{ DBGBUS_DSPP, 73, 6},
	{ DBGBUS_DSPP, 73, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 74, 1},
	{ DBGBUS_DSPP, 74, 2},
	{ DBGBUS_DSPP, 74, 3},
	{ DBGBUS_DSPP, 74, 4},
	{ DBGBUS_DSPP, 74, 5},
	{ DBGBUS_DSPP, 74, 6},
	{ DBGBUS_DSPP, 74, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 75, 1},
	{ DBGBUS_DSPP, 75, 2},
	{ DBGBUS_DSPP, 75, 3},
	{ DBGBUS_DSPP, 75, 4},
	{ DBGBUS_DSPP, 75, 5},
	{ DBGBUS_DSPP, 75, 6},
	{ DBGBUS_DSPP, 75, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 76, 1},
	{ DBGBUS_DSPP, 76, 2},
	{ DBGBUS_DSPP, 76, 3},
	{ DBGBUS_DSPP, 76, 4},
	{ DBGBUS_DSPP, 76, 5},
	{ DBGBUS_DSPP, 76, 6},
	{ DBGBUS_DSPP, 76, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 88, 1},
	{ DBGBUS_DSPP, 88, 2},
	{ DBGBUS_DSPP, 88, 3},
	{ DBGBUS_DSPP, 88, 4},
	{ DBGBUS_DSPP, 88, 5},
	{ DBGBUS_DSPP, 88, 6},
	{ DBGBUS_DSPP, 88, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 89, 1},
	{ DBGBUS_DSPP, 89, 2},
	{ DBGBUS_DSPP, 89, 3},
	{ DBGBUS_DSPP, 89, 4},
	{ DBGBUS_DSPP, 89, 5},
	{ DBGBUS_DSPP, 89, 6},
	{ DBGBUS_DSPP, 89, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 90, 1},
	{ DBGBUS_DSPP, 90, 2},
	{ DBGBUS_DSPP, 90, 3},
	{ DBGBUS_DSPP, 90, 4},
	{ DBGBUS_DSPP, 90, 5},
	{ DBGBUS_DSPP, 90, 6},
	{ DBGBUS_DSPP, 90, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 91, 1},
	{ DBGBUS_DSPP, 91, 2},
	{ DBGBUS_DSPP, 91, 3},
	{ DBGBUS_DSPP, 91, 4},
	{ DBGBUS_DSPP, 91, 5},
	{ DBGBUS_DSPP, 91, 6},
	{ DBGBUS_DSPP, 91, 7, _sde_debug_bus_lm_dump },

	/* LM2 */
	{ DBGBUS_DSPP, 77, 0},
	{ DBGBUS_DSPP, 77, 1},
	{ DBGBUS_DSPP, 77, 2},
	{ DBGBUS_DSPP, 77, 3},
	{ DBGBUS_DSPP, 77, 4},
	{ DBGBUS_DSPP, 77, 5},
	{ DBGBUS_DSPP, 77, 6},
	{ DBGBUS_DSPP, 77, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 78, 0},
	{ DBGBUS_DSPP, 78, 1},
	{ DBGBUS_DSPP, 78, 2},
	{ DBGBUS_DSPP, 78, 3},
	{ DBGBUS_DSPP, 78, 4},
	{ DBGBUS_DSPP, 78, 5},
	{ DBGBUS_DSPP, 78, 6},
	{ DBGBUS_DSPP, 78, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 79, 0},
	{ DBGBUS_DSPP, 79, 1},
	{ DBGBUS_DSPP, 79, 2},
	{ DBGBUS_DSPP, 79, 3},
	{ DBGBUS_DSPP, 79, 4},
	{ DBGBUS_DSPP, 79, 5},
	{ DBGBUS_DSPP, 79, 6},
	{ DBGBUS_DSPP, 79, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 80, 0},
	{ DBGBUS_DSPP, 80, 1},
	{ DBGBUS_DSPP, 80, 2},
	{ DBGBUS_DSPP, 80, 3},
	{ DBGBUS_DSPP, 80, 4},
	{ DBGBUS_DSPP, 80, 5},
	{ DBGBUS_DSPP, 80, 6},
	{ DBGBUS_DSPP, 80, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 81, 0},
	{ DBGBUS_DSPP, 81, 1},
	{ DBGBUS_DSPP, 81, 2},
	{ DBGBUS_DSPP, 81, 3},
	{ DBGBUS_DSPP, 81, 4},
	{ DBGBUS_DSPP, 81, 5},
	{ DBGBUS_DSPP, 81, 6},
	{ DBGBUS_DSPP, 81, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 82, 0},
	{ DBGBUS_DSPP, 82, 1},
	{ DBGBUS_DSPP, 82, 2},
	{ DBGBUS_DSPP, 82, 3},
	{ DBGBUS_DSPP, 82, 4},
	{ DBGBUS_DSPP, 82, 5},
	{ DBGBUS_DSPP, 82, 6},
	{ DBGBUS_DSPP, 82, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 83, 0},
	{ DBGBUS_DSPP, 83, 1},
	{ DBGBUS_DSPP, 83, 2},
	{ DBGBUS_DSPP, 83, 3},
	{ DBGBUS_DSPP, 83, 4},
	{ DBGBUS_DSPP, 83, 5},
	{ DBGBUS_DSPP, 83, 6},
	{ DBGBUS_DSPP, 83, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 92, 1},
	{ DBGBUS_DSPP, 92, 2},
	{ DBGBUS_DSPP, 92, 3},
	{ DBGBUS_DSPP, 92, 4},
	{ DBGBUS_DSPP, 92, 5},
	{ DBGBUS_DSPP, 92, 6},
	{ DBGBUS_DSPP, 92, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 93, 1},
	{ DBGBUS_DSPP, 93, 2},
	{ DBGBUS_DSPP, 93, 3},
	{ DBGBUS_DSPP, 93, 4},
	{ DBGBUS_DSPP, 93, 5},
	{ DBGBUS_DSPP, 93, 6},
	{ DBGBUS_DSPP, 93, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 94, 1},
	{ DBGBUS_DSPP, 94, 2},
	{ DBGBUS_DSPP, 94, 3},
	{ DBGBUS_DSPP, 94, 4},
	{ DBGBUS_DSPP, 94, 5},
	{ DBGBUS_DSPP, 94, 6},
	{ DBGBUS_DSPP, 94, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 95, 1},
	{ DBGBUS_DSPP, 95, 2},
	{ DBGBUS_DSPP, 95, 3},
	{ DBGBUS_DSPP, 95, 4},
	{ DBGBUS_DSPP, 95, 5},
	{ DBGBUS_DSPP, 95, 6},
	{ DBGBUS_DSPP, 95, 7, _sde_debug_bus_lm_dump },

	/* LM5 */
	{ DBGBUS_DSPP, 110, 1},
	{ DBGBUS_DSPP, 110, 2},
	{ DBGBUS_DSPP, 110, 3},
	{ DBGBUS_DSPP, 110, 4},
	{ DBGBUS_DSPP, 110, 5},
	{ DBGBUS_DSPP, 110, 6},
	{ DBGBUS_DSPP, 110, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 111, 1},
	{ DBGBUS_DSPP, 111, 2},
	{ DBGBUS_DSPP, 111, 3},
	{ DBGBUS_DSPP, 111, 4},
	{ DBGBUS_DSPP, 111, 5},
	{ DBGBUS_DSPP, 111, 6},
	{ DBGBUS_DSPP, 111, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 112, 1},
	{ DBGBUS_DSPP, 112, 2},
	{ DBGBUS_DSPP, 112, 3},
	{ DBGBUS_DSPP, 112, 4},
	{ DBGBUS_DSPP, 112, 5},
	{ DBGBUS_DSPP, 112, 6},
	{ DBGBUS_DSPP, 112, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 113, 1},
	{ DBGBUS_DSPP, 113, 2},
	{ DBGBUS_DSPP, 113, 3},
	{ DBGBUS_DSPP, 113, 4},
	{ DBGBUS_DSPP, 113, 5},
	{ DBGBUS_DSPP, 113, 6},
	{ DBGBUS_DSPP, 113, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 114, 1},
	{ DBGBUS_DSPP, 114, 2},
	{ DBGBUS_DSPP, 114, 3},
	{ DBGBUS_DSPP, 114, 4},
	{ DBGBUS_DSPP, 114, 5},
	{ DBGBUS_DSPP, 114, 6},
	{ DBGBUS_DSPP, 114, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 115, 1},
	{ DBGBUS_DSPP, 115, 2},
	{ DBGBUS_DSPP, 115, 3},
	{ DBGBUS_DSPP, 115, 4},
	{ DBGBUS_DSPP, 115, 5},
	{ DBGBUS_DSPP, 115, 6},
	{ DBGBUS_DSPP, 115, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 116, 1},
	{ DBGBUS_DSPP, 116, 2},
	{ DBGBUS_DSPP, 116, 3},
	{ DBGBUS_DSPP, 116, 4},
	{ DBGBUS_DSPP, 116, 5},
	{ DBGBUS_DSPP, 116, 6},
	{ DBGBUS_DSPP, 116, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 117, 1},
	{ DBGBUS_DSPP, 117, 2},
	{ DBGBUS_DSPP, 117, 3},
	{ DBGBUS_DSPP, 117, 4},
	{ DBGBUS_DSPP, 117, 5},
	{ DBGBUS_DSPP, 117, 6},
	{ DBGBUS_DSPP, 117, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 118, 1},
	{ DBGBUS_DSPP, 118, 2},
	{ DBGBUS_DSPP, 118, 3},
	{ DBGBUS_DSPP, 118, 4},
	{ DBGBUS_DSPP, 118, 5},
	{ DBGBUS_DSPP, 118, 6},
	{ DBGBUS_DSPP, 118, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 119, 1},
	{ DBGBUS_DSPP, 119, 2},
	{ DBGBUS_DSPP, 119, 3},
	{ DBGBUS_DSPP, 119, 4},
	{ DBGBUS_DSPP, 119, 5},
	{ DBGBUS_DSPP, 119, 6},
	{ DBGBUS_DSPP, 119, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 120, 1},
	{ DBGBUS_DSPP, 120, 2},
	{ DBGBUS_DSPP, 120, 3},
	{ DBGBUS_DSPP, 120, 4},
	{ DBGBUS_DSPP, 120, 5},
	{ DBGBUS_DSPP, 120, 6},
	{ DBGBUS_DSPP, 120, 7, _sde_debug_bus_lm_dump },

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
	{ DBGBUS_SSPP0, 17, 0},
	{ DBGBUS_SSPP0, 17, 1},
	{ DBGBUS_SSPP0, 17, 3},
	{ DBGBUS_SSPP0, 37, 0},
	{ DBGBUS_SSPP0, 37, 1},
	{ DBGBUS_SSPP0, 37, 3},
	{ DBGBUS_SSPP0, 46, 0},
	{ DBGBUS_SSPP0, 46, 1},
	{ DBGBUS_SSPP0, 46, 3},

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

	/* intf0-3 */
	{ DBGBUS_PERIPH, 0, 0},
	{ DBGBUS_PERIPH, 1, 0},
	{ DBGBUS_PERIPH, 2, 0},
	{ DBGBUS_PERIPH, 3, 0},

	/* te counter wrapper */
	{ DBGBUS_PERIPH, 60, 0},

	/* dsc0 */
	{ DBGBUS_PERIPH, 47, 0},
	{ DBGBUS_PERIPH, 47, 1},
	{ DBGBUS_PERIPH, 47, 2},
	{ DBGBUS_PERIPH, 47, 3},
	{ DBGBUS_PERIPH, 47, 4},
	{ DBGBUS_PERIPH, 47, 5},
	{ DBGBUS_PERIPH, 47, 6},
	{ DBGBUS_PERIPH, 47, 7},

	/* dsc1 */
	{ DBGBUS_PERIPH, 48, 0},
	{ DBGBUS_PERIPH, 48, 1},
	{ DBGBUS_PERIPH, 48, 2},
	{ DBGBUS_PERIPH, 48, 3},
	{ DBGBUS_PERIPH, 48, 4},
	{ DBGBUS_PERIPH, 48, 5},
	{ DBGBUS_PERIPH, 48, 6},
	{ DBGBUS_PERIPH, 48, 7},

	/* dsc2 */
	{ DBGBUS_PERIPH, 51, 0},
	{ DBGBUS_PERIPH, 51, 1},
	{ DBGBUS_PERIPH, 51, 2},
	{ DBGBUS_PERIPH, 51, 3},
	{ DBGBUS_PERIPH, 51, 4},
	{ DBGBUS_PERIPH, 51, 5},
	{ DBGBUS_PERIPH, 51, 6},
	{ DBGBUS_PERIPH, 51, 7},

	/* dsc3 */
	{ DBGBUS_PERIPH, 52, 0},
	{ DBGBUS_PERIPH, 52, 1},
	{ DBGBUS_PERIPH, 52, 2},
	{ DBGBUS_PERIPH, 52, 3},
	{ DBGBUS_PERIPH, 52, 4},
	{ DBGBUS_PERIPH, 52, 5},
	{ DBGBUS_PERIPH, 52, 6},
	{ DBGBUS_PERIPH, 52, 7},

	/* tear-check */
	{ DBGBUS_PERIPH, 63, 0 },
	{ DBGBUS_PERIPH, 64, 0 },
	{ DBGBUS_PERIPH, 65, 0 },
	{ DBGBUS_PERIPH, 73, 0 },
	{ DBGBUS_PERIPH, 74, 0 },

	/* cdwn */
	{ DBGBUS_PERIPH, 80, 0},
	{ DBGBUS_PERIPH, 80, 1},
	{ DBGBUS_PERIPH, 80, 2},

	{ DBGBUS_PERIPH, 81, 0},
	{ DBGBUS_PERIPH, 81, 1},
	{ DBGBUS_PERIPH, 81, 2},

	{ DBGBUS_PERIPH, 82, 0},
	{ DBGBUS_PERIPH, 82, 1},
	{ DBGBUS_PERIPH, 82, 2},
	{ DBGBUS_PERIPH, 82, 3},
	{ DBGBUS_PERIPH, 82, 4},
	{ DBGBUS_PERIPH, 82, 5},
	{ DBGBUS_PERIPH, 82, 6},
	{ DBGBUS_PERIPH, 82, 7},

	/* hdmi */
	{ DBGBUS_PERIPH, 68, 0},
	{ DBGBUS_PERIPH, 68, 1},
	{ DBGBUS_PERIPH, 68, 2},
	{ DBGBUS_PERIPH, 68, 3},
	{ DBGBUS_PERIPH, 68, 4},
	{ DBGBUS_PERIPH, 68, 5},

	/* edp */
	{ DBGBUS_PERIPH, 69, 0},
	{ DBGBUS_PERIPH, 69, 1},
	{ DBGBUS_PERIPH, 69, 2},
	{ DBGBUS_PERIPH, 69, 3},
	{ DBGBUS_PERIPH, 69, 4},
	{ DBGBUS_PERIPH, 69, 5},

	/* dsi0 */
	{ DBGBUS_PERIPH, 70, 0},
	{ DBGBUS_PERIPH, 70, 1},
	{ DBGBUS_PERIPH, 70, 2},
	{ DBGBUS_PERIPH, 70, 3},
	{ DBGBUS_PERIPH, 70, 4},
	{ DBGBUS_PERIPH, 70, 5},

	/* dsi1 */
	{ DBGBUS_PERIPH, 71, 0},
	{ DBGBUS_PERIPH, 71, 1},
	{ DBGBUS_PERIPH, 71, 2},
	{ DBGBUS_PERIPH, 71, 3},
	{ DBGBUS_PERIPH, 71, 4},
	{ DBGBUS_PERIPH, 71, 5},

	/* axi - should be last entry */
	{ DBGBUS_AXI_INTF, 62, 0, _sde_debug_bus_axi_dump_sdm845},
};

static struct sde_debug_bus_entry dbg_bus_sde_sm8150[] = {

	/* Unpack 0 sspp 0*/
	{ DBGBUS_SSPP0, 35, 2 },
	{ DBGBUS_SSPP0, 50, 2 },
	{ DBGBUS_SSPP0, 60, 2 },
	{ DBGBUS_SSPP0, 70, 2 },

	/* Unpack 1 sspp 0*/
	{ DBGBUS_SSPP0, 36, 2 },
	{ DBGBUS_SSPP0, 51, 2 },
	{ DBGBUS_SSPP0, 61, 2 },
	{ DBGBUS_SSPP0, 71, 2 },

	/* Unpack 2 sspp 0*/
	{ DBGBUS_SSPP0, 37, 2 },
	{ DBGBUS_SSPP0, 52, 2 },
	{ DBGBUS_SSPP0, 62, 2 },
	{ DBGBUS_SSPP0, 72, 2 },


	/* Unpack 3 sspp 0*/
	{ DBGBUS_SSPP0, 38, 2 },
	{ DBGBUS_SSPP0, 53, 2 },
	{ DBGBUS_SSPP0, 63, 2 },
	{ DBGBUS_SSPP0, 73, 2 },

	/* Unpack 0 sspp 1*/
	{ DBGBUS_SSPP1, 35, 2 },
	{ DBGBUS_SSPP1, 50, 2 },
	{ DBGBUS_SSPP1, 60, 2 },
	{ DBGBUS_SSPP1, 70, 2 },

	/* Unpack 1 sspp 1*/
	{ DBGBUS_SSPP1, 36, 2 },
	{ DBGBUS_SSPP1, 51, 2 },
	{ DBGBUS_SSPP1, 61, 2 },
	{ DBGBUS_SSPP1, 71, 2 },

	/* Unpack 2 sspp 1*/
	{ DBGBUS_SSPP1, 37, 2 },
	{ DBGBUS_SSPP1, 52, 2 },
	{ DBGBUS_SSPP1, 62, 2 },
	{ DBGBUS_SSPP1, 72, 2 },


	/* Unpack 3 sspp 1*/
	{ DBGBUS_SSPP1, 38, 2 },
	{ DBGBUS_SSPP1, 53, 2 },
	{ DBGBUS_SSPP1, 63, 2 },
	{ DBGBUS_SSPP1, 73, 2 },

	/* scheduler */
	{ DBGBUS_DSPP, 130, 0 },
	{ DBGBUS_DSPP, 130, 1 },
	{ DBGBUS_DSPP, 130, 2 },
	{ DBGBUS_DSPP, 130, 3 },
	{ DBGBUS_DSPP, 130, 4 },
	{ DBGBUS_DSPP, 130, 5 },


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

	/* ppb_0 */
	{ DBGBUS_DSPP, 31, 0, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, 33, 0, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, 35, 0, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, 42, 0, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, 47, 0, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, 49, 0, _sde_debug_bus_ppb0_dump },

	/* ppb_1 */
	{ DBGBUS_DSPP, 32, 0, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, 34, 0, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, 36, 0, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, 43, 0, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, 48, 0, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, 50, 0, _sde_debug_bus_ppb1_dump },

	/* crossbar */
	{ DBGBUS_DSPP, 0, 0, _sde_debug_bus_xbar_dump },

	/* rotator */
	{ DBGBUS_DSPP, 9, 0},

	/* blend */
	/* LM0 */
	{ DBGBUS_DSPP, 63, 1},
	{ DBGBUS_DSPP, 63, 2},
	{ DBGBUS_DSPP, 63, 3},
	{ DBGBUS_DSPP, 63, 4},
	{ DBGBUS_DSPP, 63, 5},
	{ DBGBUS_DSPP, 63, 6},
	{ DBGBUS_DSPP, 63, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 64, 1},
	{ DBGBUS_DSPP, 64, 2},
	{ DBGBUS_DSPP, 64, 3},
	{ DBGBUS_DSPP, 64, 4},
	{ DBGBUS_DSPP, 64, 5},
	{ DBGBUS_DSPP, 64, 6},
	{ DBGBUS_DSPP, 64, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 65, 1},
	{ DBGBUS_DSPP, 65, 2},
	{ DBGBUS_DSPP, 65, 3},
	{ DBGBUS_DSPP, 65, 4},
	{ DBGBUS_DSPP, 65, 5},
	{ DBGBUS_DSPP, 65, 6},
	{ DBGBUS_DSPP, 65, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 66, 1},
	{ DBGBUS_DSPP, 66, 2},
	{ DBGBUS_DSPP, 66, 3},
	{ DBGBUS_DSPP, 66, 4},
	{ DBGBUS_DSPP, 66, 5},
	{ DBGBUS_DSPP, 66, 6},
	{ DBGBUS_DSPP, 66, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 67, 1},
	{ DBGBUS_DSPP, 67, 2},
	{ DBGBUS_DSPP, 67, 3},
	{ DBGBUS_DSPP, 67, 4},
	{ DBGBUS_DSPP, 67, 5},
	{ DBGBUS_DSPP, 67, 6},
	{ DBGBUS_DSPP, 67, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 68, 1},
	{ DBGBUS_DSPP, 68, 2},
	{ DBGBUS_DSPP, 68, 3},
	{ DBGBUS_DSPP, 68, 4},
	{ DBGBUS_DSPP, 68, 5},
	{ DBGBUS_DSPP, 68, 6},
	{ DBGBUS_DSPP, 68, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 69, 1},
	{ DBGBUS_DSPP, 69, 2},
	{ DBGBUS_DSPP, 69, 3},
	{ DBGBUS_DSPP, 69, 4},
	{ DBGBUS_DSPP, 69, 5},
	{ DBGBUS_DSPP, 69, 6},
	{ DBGBUS_DSPP, 69, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 84, 1},
	{ DBGBUS_DSPP, 84, 2},
	{ DBGBUS_DSPP, 84, 3},
	{ DBGBUS_DSPP, 84, 4},
	{ DBGBUS_DSPP, 84, 5},
	{ DBGBUS_DSPP, 84, 6},
	{ DBGBUS_DSPP, 84, 7, _sde_debug_bus_lm_dump },


	{ DBGBUS_DSPP, 85, 1},
	{ DBGBUS_DSPP, 85, 2},
	{ DBGBUS_DSPP, 85, 3},
	{ DBGBUS_DSPP, 85, 4},
	{ DBGBUS_DSPP, 85, 5},
	{ DBGBUS_DSPP, 85, 6},
	{ DBGBUS_DSPP, 85, 7, _sde_debug_bus_lm_dump },


	{ DBGBUS_DSPP, 86, 1},
	{ DBGBUS_DSPP, 86, 2},
	{ DBGBUS_DSPP, 86, 3},
	{ DBGBUS_DSPP, 86, 4},
	{ DBGBUS_DSPP, 86, 5},
	{ DBGBUS_DSPP, 86, 6},
	{ DBGBUS_DSPP, 86, 7, _sde_debug_bus_lm_dump },


	{ DBGBUS_DSPP, 87, 1},
	{ DBGBUS_DSPP, 87, 2},
	{ DBGBUS_DSPP, 87, 3},
	{ DBGBUS_DSPP, 87, 4},
	{ DBGBUS_DSPP, 87, 5},
	{ DBGBUS_DSPP, 87, 6},
	{ DBGBUS_DSPP, 87, 7, _sde_debug_bus_lm_dump },

	/* LM1 */
	{ DBGBUS_DSPP, 70, 1},
	{ DBGBUS_DSPP, 70, 2},
	{ DBGBUS_DSPP, 70, 3},
	{ DBGBUS_DSPP, 70, 4},
	{ DBGBUS_DSPP, 70, 5},
	{ DBGBUS_DSPP, 70, 6},
	{ DBGBUS_DSPP, 70, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 71, 1},
	{ DBGBUS_DSPP, 71, 2},
	{ DBGBUS_DSPP, 71, 3},
	{ DBGBUS_DSPP, 71, 4},
	{ DBGBUS_DSPP, 71, 5},
	{ DBGBUS_DSPP, 71, 6},
	{ DBGBUS_DSPP, 71, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 72, 1},
	{ DBGBUS_DSPP, 72, 2},
	{ DBGBUS_DSPP, 72, 3},
	{ DBGBUS_DSPP, 72, 4},
	{ DBGBUS_DSPP, 72, 5},
	{ DBGBUS_DSPP, 72, 6},
	{ DBGBUS_DSPP, 72, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 73, 1},
	{ DBGBUS_DSPP, 73, 2},
	{ DBGBUS_DSPP, 73, 3},
	{ DBGBUS_DSPP, 73, 4},
	{ DBGBUS_DSPP, 73, 5},
	{ DBGBUS_DSPP, 73, 6},
	{ DBGBUS_DSPP, 73, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 74, 1},
	{ DBGBUS_DSPP, 74, 2},
	{ DBGBUS_DSPP, 74, 3},
	{ DBGBUS_DSPP, 74, 4},
	{ DBGBUS_DSPP, 74, 5},
	{ DBGBUS_DSPP, 74, 6},
	{ DBGBUS_DSPP, 74, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 75, 1},
	{ DBGBUS_DSPP, 75, 2},
	{ DBGBUS_DSPP, 75, 3},
	{ DBGBUS_DSPP, 75, 4},
	{ DBGBUS_DSPP, 75, 5},
	{ DBGBUS_DSPP, 75, 6},
	{ DBGBUS_DSPP, 75, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 76, 1},
	{ DBGBUS_DSPP, 76, 2},
	{ DBGBUS_DSPP, 76, 3},
	{ DBGBUS_DSPP, 76, 4},
	{ DBGBUS_DSPP, 76, 5},
	{ DBGBUS_DSPP, 76, 6},
	{ DBGBUS_DSPP, 76, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 88, 1},
	{ DBGBUS_DSPP, 88, 2},
	{ DBGBUS_DSPP, 88, 3},
	{ DBGBUS_DSPP, 88, 4},
	{ DBGBUS_DSPP, 88, 5},
	{ DBGBUS_DSPP, 88, 6},
	{ DBGBUS_DSPP, 88, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 89, 1},
	{ DBGBUS_DSPP, 89, 2},
	{ DBGBUS_DSPP, 89, 3},
	{ DBGBUS_DSPP, 89, 4},
	{ DBGBUS_DSPP, 89, 5},
	{ DBGBUS_DSPP, 89, 6},
	{ DBGBUS_DSPP, 89, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 90, 1},
	{ DBGBUS_DSPP, 90, 2},
	{ DBGBUS_DSPP, 90, 3},
	{ DBGBUS_DSPP, 90, 4},
	{ DBGBUS_DSPP, 90, 5},
	{ DBGBUS_DSPP, 90, 6},
	{ DBGBUS_DSPP, 90, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 91, 1},
	{ DBGBUS_DSPP, 91, 2},
	{ DBGBUS_DSPP, 91, 3},
	{ DBGBUS_DSPP, 91, 4},
	{ DBGBUS_DSPP, 91, 5},
	{ DBGBUS_DSPP, 91, 6},
	{ DBGBUS_DSPP, 91, 7, _sde_debug_bus_lm_dump },

	/* LM2 */
	{ DBGBUS_DSPP, 77, 0},
	{ DBGBUS_DSPP, 77, 1},
	{ DBGBUS_DSPP, 77, 2},
	{ DBGBUS_DSPP, 77, 3},
	{ DBGBUS_DSPP, 77, 4},
	{ DBGBUS_DSPP, 77, 5},
	{ DBGBUS_DSPP, 77, 6},
	{ DBGBUS_DSPP, 77, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 78, 0},
	{ DBGBUS_DSPP, 78, 1},
	{ DBGBUS_DSPP, 78, 2},
	{ DBGBUS_DSPP, 78, 3},
	{ DBGBUS_DSPP, 78, 4},
	{ DBGBUS_DSPP, 78, 5},
	{ DBGBUS_DSPP, 78, 6},
	{ DBGBUS_DSPP, 78, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 79, 0},
	{ DBGBUS_DSPP, 79, 1},
	{ DBGBUS_DSPP, 79, 2},
	{ DBGBUS_DSPP, 79, 3},
	{ DBGBUS_DSPP, 79, 4},
	{ DBGBUS_DSPP, 79, 5},
	{ DBGBUS_DSPP, 79, 6},
	{ DBGBUS_DSPP, 79, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 80, 0},
	{ DBGBUS_DSPP, 80, 1},
	{ DBGBUS_DSPP, 80, 2},
	{ DBGBUS_DSPP, 80, 3},
	{ DBGBUS_DSPP, 80, 4},
	{ DBGBUS_DSPP, 80, 5},
	{ DBGBUS_DSPP, 80, 6},
	{ DBGBUS_DSPP, 80, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 81, 0},
	{ DBGBUS_DSPP, 81, 1},
	{ DBGBUS_DSPP, 81, 2},
	{ DBGBUS_DSPP, 81, 3},
	{ DBGBUS_DSPP, 81, 4},
	{ DBGBUS_DSPP, 81, 5},
	{ DBGBUS_DSPP, 81, 6},
	{ DBGBUS_DSPP, 81, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 82, 0},
	{ DBGBUS_DSPP, 82, 1},
	{ DBGBUS_DSPP, 82, 2},
	{ DBGBUS_DSPP, 82, 3},
	{ DBGBUS_DSPP, 82, 4},
	{ DBGBUS_DSPP, 82, 5},
	{ DBGBUS_DSPP, 82, 6},
	{ DBGBUS_DSPP, 82, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 83, 0},
	{ DBGBUS_DSPP, 83, 1},
	{ DBGBUS_DSPP, 83, 2},
	{ DBGBUS_DSPP, 83, 3},
	{ DBGBUS_DSPP, 83, 4},
	{ DBGBUS_DSPP, 83, 5},
	{ DBGBUS_DSPP, 83, 6},
	{ DBGBUS_DSPP, 83, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 92, 1},
	{ DBGBUS_DSPP, 92, 2},
	{ DBGBUS_DSPP, 92, 3},
	{ DBGBUS_DSPP, 92, 4},
	{ DBGBUS_DSPP, 92, 5},
	{ DBGBUS_DSPP, 92, 6},
	{ DBGBUS_DSPP, 92, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 93, 1},
	{ DBGBUS_DSPP, 93, 2},
	{ DBGBUS_DSPP, 93, 3},
	{ DBGBUS_DSPP, 93, 4},
	{ DBGBUS_DSPP, 93, 5},
	{ DBGBUS_DSPP, 93, 6},
	{ DBGBUS_DSPP, 93, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 94, 1},
	{ DBGBUS_DSPP, 94, 2},
	{ DBGBUS_DSPP, 94, 3},
	{ DBGBUS_DSPP, 94, 4},
	{ DBGBUS_DSPP, 94, 5},
	{ DBGBUS_DSPP, 94, 6},
	{ DBGBUS_DSPP, 94, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 95, 1},
	{ DBGBUS_DSPP, 95, 2},
	{ DBGBUS_DSPP, 95, 3},
	{ DBGBUS_DSPP, 95, 4},
	{ DBGBUS_DSPP, 95, 5},
	{ DBGBUS_DSPP, 95, 6},
	{ DBGBUS_DSPP, 95, 7, _sde_debug_bus_lm_dump },


	/* LM3 */
	{ DBGBUS_DSPP, 110, 1},
	{ DBGBUS_DSPP, 110, 2},
	{ DBGBUS_DSPP, 110, 3},
	{ DBGBUS_DSPP, 110, 4},
	{ DBGBUS_DSPP, 110, 5},
	{ DBGBUS_DSPP, 110, 6},
	{ DBGBUS_DSPP, 110, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 111, 1},
	{ DBGBUS_DSPP, 111, 2},
	{ DBGBUS_DSPP, 111, 3},
	{ DBGBUS_DSPP, 111, 4},
	{ DBGBUS_DSPP, 111, 5},
	{ DBGBUS_DSPP, 111, 6},
	{ DBGBUS_DSPP, 111, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 112, 1},
	{ DBGBUS_DSPP, 112, 2},
	{ DBGBUS_DSPP, 112, 3},
	{ DBGBUS_DSPP, 112, 4},
	{ DBGBUS_DSPP, 112, 5},
	{ DBGBUS_DSPP, 112, 6},
	{ DBGBUS_DSPP, 112, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 113, 1},
	{ DBGBUS_DSPP, 113, 2},
	{ DBGBUS_DSPP, 113, 3},
	{ DBGBUS_DSPP, 113, 4},
	{ DBGBUS_DSPP, 113, 5},
	{ DBGBUS_DSPP, 113, 6},
	{ DBGBUS_DSPP, 113, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 114, 1},
	{ DBGBUS_DSPP, 114, 2},
	{ DBGBUS_DSPP, 114, 3},
	{ DBGBUS_DSPP, 114, 4},
	{ DBGBUS_DSPP, 114, 5},
	{ DBGBUS_DSPP, 114, 6},
	{ DBGBUS_DSPP, 114, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 115, 1},
	{ DBGBUS_DSPP, 115, 2},
	{ DBGBUS_DSPP, 115, 3},
	{ DBGBUS_DSPP, 115, 4},
	{ DBGBUS_DSPP, 115, 5},
	{ DBGBUS_DSPP, 115, 6},
	{ DBGBUS_DSPP, 115, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 116, 1},
	{ DBGBUS_DSPP, 116, 2},
	{ DBGBUS_DSPP, 116, 3},
	{ DBGBUS_DSPP, 116, 4},
	{ DBGBUS_DSPP, 116, 5},
	{ DBGBUS_DSPP, 116, 6},
	{ DBGBUS_DSPP, 116, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 117, 1},
	{ DBGBUS_DSPP, 117, 2},
	{ DBGBUS_DSPP, 117, 3},
	{ DBGBUS_DSPP, 117, 4},
	{ DBGBUS_DSPP, 117, 5},
	{ DBGBUS_DSPP, 117, 6},
	{ DBGBUS_DSPP, 117, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 118, 1},
	{ DBGBUS_DSPP, 118, 2},
	{ DBGBUS_DSPP, 118, 3},
	{ DBGBUS_DSPP, 118, 4},
	{ DBGBUS_DSPP, 118, 5},
	{ DBGBUS_DSPP, 118, 6},
	{ DBGBUS_DSPP, 118, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 119, 1},
	{ DBGBUS_DSPP, 119, 2},
	{ DBGBUS_DSPP, 119, 3},
	{ DBGBUS_DSPP, 119, 4},
	{ DBGBUS_DSPP, 119, 5},
	{ DBGBUS_DSPP, 119, 6},
	{ DBGBUS_DSPP, 119, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 120, 1},
	{ DBGBUS_DSPP, 120, 2},
	{ DBGBUS_DSPP, 120, 3},
	{ DBGBUS_DSPP, 120, 4},
	{ DBGBUS_DSPP, 120, 5},
	{ DBGBUS_DSPP, 120, 6},
	{ DBGBUS_DSPP, 120, 7, _sde_debug_bus_lm_dump },

	/* LM4 */
	{ DBGBUS_DSPP, 96, 1},
	{ DBGBUS_DSPP, 96, 2},
	{ DBGBUS_DSPP, 96, 3},
	{ DBGBUS_DSPP, 96, 4},
	{ DBGBUS_DSPP, 96, 5},
	{ DBGBUS_DSPP, 96, 6},
	{ DBGBUS_DSPP, 96, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 97, 1},
	{ DBGBUS_DSPP, 97, 2},
	{ DBGBUS_DSPP, 97, 3},
	{ DBGBUS_DSPP, 97, 4},
	{ DBGBUS_DSPP, 97, 5},
	{ DBGBUS_DSPP, 97, 6},
	{ DBGBUS_DSPP, 97, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 98, 1},
	{ DBGBUS_DSPP, 98, 2},
	{ DBGBUS_DSPP, 98, 3},
	{ DBGBUS_DSPP, 98, 4},
	{ DBGBUS_DSPP, 98, 5},
	{ DBGBUS_DSPP, 98, 6},
	{ DBGBUS_DSPP, 98, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 99, 1},
	{ DBGBUS_DSPP, 99, 2},
	{ DBGBUS_DSPP, 99, 3},
	{ DBGBUS_DSPP, 99, 4},
	{ DBGBUS_DSPP, 99, 5},
	{ DBGBUS_DSPP, 99, 6},
	{ DBGBUS_DSPP, 99, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 100, 1},
	{ DBGBUS_DSPP, 100, 2},
	{ DBGBUS_DSPP, 100, 3},
	{ DBGBUS_DSPP, 100, 4},
	{ DBGBUS_DSPP, 100, 5},
	{ DBGBUS_DSPP, 100, 6},
	{ DBGBUS_DSPP, 100, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 101, 1},
	{ DBGBUS_DSPP, 101, 2},
	{ DBGBUS_DSPP, 101, 3},
	{ DBGBUS_DSPP, 101, 4},
	{ DBGBUS_DSPP, 101, 5},
	{ DBGBUS_DSPP, 101, 6},
	{ DBGBUS_DSPP, 101, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 103, 1},
	{ DBGBUS_DSPP, 103, 2},
	{ DBGBUS_DSPP, 103, 3},
	{ DBGBUS_DSPP, 103, 4},
	{ DBGBUS_DSPP, 103, 5},
	{ DBGBUS_DSPP, 103, 6},
	{ DBGBUS_DSPP, 103, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 105, 1},
	{ DBGBUS_DSPP, 105, 2},
	{ DBGBUS_DSPP, 105, 3},
	{ DBGBUS_DSPP, 105, 4},
	{ DBGBUS_DSPP, 105, 5},
	{ DBGBUS_DSPP, 105, 6},
	{ DBGBUS_DSPP, 105, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 106, 1},
	{ DBGBUS_DSPP, 106, 2},
	{ DBGBUS_DSPP, 106, 3},
	{ DBGBUS_DSPP, 106, 4},
	{ DBGBUS_DSPP, 106, 5},
	{ DBGBUS_DSPP, 106, 6},
	{ DBGBUS_DSPP, 106, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 109, 1},
	{ DBGBUS_DSPP, 109, 2},
	{ DBGBUS_DSPP, 109, 3},
	{ DBGBUS_DSPP, 109, 4},
	{ DBGBUS_DSPP, 109, 5},
	{ DBGBUS_DSPP, 109, 6},
	{ DBGBUS_DSPP, 109, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 122, 1},
	{ DBGBUS_DSPP, 122, 2},
	{ DBGBUS_DSPP, 122, 3},
	{ DBGBUS_DSPP, 122, 4},
	{ DBGBUS_DSPP, 122, 5},
	{ DBGBUS_DSPP, 122, 6},
	{ DBGBUS_DSPP, 122, 7, _sde_debug_bus_lm_dump },

	/* LM5 */
	{ DBGBUS_DSPP, 124, 1},
	{ DBGBUS_DSPP, 124, 2},
	{ DBGBUS_DSPP, 124, 3},
	{ DBGBUS_DSPP, 124, 4},
	{ DBGBUS_DSPP, 124, 5},
	{ DBGBUS_DSPP, 124, 6},
	{ DBGBUS_DSPP, 124, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 125, 1},
	{ DBGBUS_DSPP, 125, 2},
	{ DBGBUS_DSPP, 125, 3},
	{ DBGBUS_DSPP, 125, 4},
	{ DBGBUS_DSPP, 125, 5},
	{ DBGBUS_DSPP, 125, 6},
	{ DBGBUS_DSPP, 125, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 126, 1},
	{ DBGBUS_DSPP, 126, 2},
	{ DBGBUS_DSPP, 126, 3},
	{ DBGBUS_DSPP, 126, 4},
	{ DBGBUS_DSPP, 126, 5},
	{ DBGBUS_DSPP, 126, 6},
	{ DBGBUS_DSPP, 126, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 127, 1},
	{ DBGBUS_DSPP, 127, 2},
	{ DBGBUS_DSPP, 127, 3},
	{ DBGBUS_DSPP, 127, 4},
	{ DBGBUS_DSPP, 127, 5},
	{ DBGBUS_DSPP, 127, 6},
	{ DBGBUS_DSPP, 127, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 128, 1},
	{ DBGBUS_DSPP, 128, 2},
	{ DBGBUS_DSPP, 128, 3},
	{ DBGBUS_DSPP, 128, 4},
	{ DBGBUS_DSPP, 128, 5},
	{ DBGBUS_DSPP, 128, 6},
	{ DBGBUS_DSPP, 128, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 129, 1},
	{ DBGBUS_DSPP, 129, 2},
	{ DBGBUS_DSPP, 129, 3},
	{ DBGBUS_DSPP, 129, 4},
	{ DBGBUS_DSPP, 129, 5},
	{ DBGBUS_DSPP, 129, 6},
	{ DBGBUS_DSPP, 129, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 131, 1},
	{ DBGBUS_DSPP, 131, 2},
	{ DBGBUS_DSPP, 131, 3},
	{ DBGBUS_DSPP, 131, 4},
	{ DBGBUS_DSPP, 131, 5},
	{ DBGBUS_DSPP, 131, 6},
	{ DBGBUS_DSPP, 131, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 132, 1},
	{ DBGBUS_DSPP, 132, 2},
	{ DBGBUS_DSPP, 132, 3},
	{ DBGBUS_DSPP, 132, 4},
	{ DBGBUS_DSPP, 132, 5},
	{ DBGBUS_DSPP, 132, 6},
	{ DBGBUS_DSPP, 132, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 133, 1},
	{ DBGBUS_DSPP, 133, 2},
	{ DBGBUS_DSPP, 133, 3},
	{ DBGBUS_DSPP, 133, 4},
	{ DBGBUS_DSPP, 133, 5},
	{ DBGBUS_DSPP, 133, 6},
	{ DBGBUS_DSPP, 133, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 134, 1},
	{ DBGBUS_DSPP, 134, 2},
	{ DBGBUS_DSPP, 134, 3},
	{ DBGBUS_DSPP, 134, 4},
	{ DBGBUS_DSPP, 134, 5},
	{ DBGBUS_DSPP, 134, 6},
	{ DBGBUS_DSPP, 134, 7, _sde_debug_bus_lm_dump },

	{ DBGBUS_DSPP, 135, 1},
	{ DBGBUS_DSPP, 135, 2},
	{ DBGBUS_DSPP, 135, 3},
	{ DBGBUS_DSPP, 135, 4},
	{ DBGBUS_DSPP, 135, 5},
	{ DBGBUS_DSPP, 135, 6},
	{ DBGBUS_DSPP, 135, 7, _sde_debug_bus_lm_dump },

	/* csc */
	{ DBGBUS_SSPP0, 7, 0},
	{ DBGBUS_SSPP0, 7, 1},
	{ DBGBUS_SSPP0, 7, 2},
	{ DBGBUS_SSPP0, 27, 0},
	{ DBGBUS_SSPP0, 27, 1},
	{ DBGBUS_SSPP0, 27, 2},
	{ DBGBUS_SSPP1, 7, 0},
	{ DBGBUS_SSPP1, 7, 1},
	{ DBGBUS_SSPP1, 7, 2},
	{ DBGBUS_SSPP1, 27, 0},
	{ DBGBUS_SSPP1, 27, 1},
	{ DBGBUS_SSPP1, 27, 2},

	/* pcc */
	{ DBGBUS_SSPP0, 43, 3},
	{ DBGBUS_SSPP0, 47, 3},
	{ DBGBUS_SSPP1, 43, 3},
	{ DBGBUS_SSPP1, 47, 3},

	/* spa */
	{ DBGBUS_SSPP0, 8,  0},
	{ DBGBUS_SSPP0, 28, 0},
	{ DBGBUS_SSPP1, 8,  0},
	{ DBGBUS_SSPP1, 28, 0},

	/* dspp pa */
	{ DBGBUS_DSPP, 13, 0},
	{ DBGBUS_DSPP, 19, 0},
	{ DBGBUS_DSPP, 24, 0},
	{ DBGBUS_DSPP, 37, 0},

	/* igc */
	{ DBGBUS_SSPP0, 39, 0},
	{ DBGBUS_SSPP0, 39, 1},
	{ DBGBUS_SSPP0, 39, 2},

	{ DBGBUS_SSPP1, 39, 0},
	{ DBGBUS_SSPP1, 39, 1},
	{ DBGBUS_SSPP1, 39, 2},

	{ DBGBUS_SSPP0, 46, 0},
	{ DBGBUS_SSPP0, 46, 1},
	{ DBGBUS_SSPP0, 46, 2},

	{ DBGBUS_SSPP1, 46, 0},
	{ DBGBUS_SSPP1, 46, 1},
	{ DBGBUS_SSPP1, 46, 2},

	{ DBGBUS_DSPP, 14, 0},
	{ DBGBUS_DSPP, 14, 1},
	{ DBGBUS_DSPP, 14, 2},
	{ DBGBUS_DSPP, 20, 0},
	{ DBGBUS_DSPP, 20, 1},
	{ DBGBUS_DSPP, 20, 2},
	{ DBGBUS_DSPP, 25, 0},
	{ DBGBUS_DSPP, 25, 1},
	{ DBGBUS_DSPP, 25, 2},
	{ DBGBUS_DSPP, 38, 0},
	{ DBGBUS_DSPP, 38, 1},
	{ DBGBUS_DSPP, 38, 2},

	/* intf0-3 */
	{ DBGBUS_PERIPH, 0, 0},
	{ DBGBUS_PERIPH, 1, 0},
	{ DBGBUS_PERIPH, 2, 0},
	{ DBGBUS_PERIPH, 3, 0},
	{ DBGBUS_PERIPH, 4, 0},
	{ DBGBUS_PERIPH, 5, 0},

	/* te counter wrapper */
	{ DBGBUS_PERIPH, 60, 0},
	{ DBGBUS_PERIPH, 60, 1},
	{ DBGBUS_PERIPH, 60, 2},
	{ DBGBUS_PERIPH, 60, 3},
	{ DBGBUS_PERIPH, 60, 4},
	{ DBGBUS_PERIPH, 60, 5},

	/* dsc0 */
	{ DBGBUS_PERIPH, 47, 0},
	{ DBGBUS_PERIPH, 47, 1},
	{ DBGBUS_PERIPH, 47, 2},
	{ DBGBUS_PERIPH, 47, 3},
	{ DBGBUS_PERIPH, 47, 4},
	{ DBGBUS_PERIPH, 47, 5},
	{ DBGBUS_PERIPH, 47, 6},
	{ DBGBUS_PERIPH, 47, 7},

	/* dsc1 */
	{ DBGBUS_PERIPH, 48, 0},
	{ DBGBUS_PERIPH, 48, 1},
	{ DBGBUS_PERIPH, 48, 2},
	{ DBGBUS_PERIPH, 48, 3},
	{ DBGBUS_PERIPH, 48, 4},
	{ DBGBUS_PERIPH, 48, 5},
	{ DBGBUS_PERIPH, 48, 6},
	{ DBGBUS_PERIPH, 48, 7},

	/* dsc2 */
	{ DBGBUS_PERIPH, 50, 0},
	{ DBGBUS_PERIPH, 50, 1},
	{ DBGBUS_PERIPH, 50, 2},
	{ DBGBUS_PERIPH, 50, 3},
	{ DBGBUS_PERIPH, 50, 4},
	{ DBGBUS_PERIPH, 50, 5},
	{ DBGBUS_PERIPH, 50, 6},
	{ DBGBUS_PERIPH, 50, 7},

	/* dsc3 */
	{ DBGBUS_PERIPH, 51, 0},
	{ DBGBUS_PERIPH, 51, 1},
	{ DBGBUS_PERIPH, 51, 2},
	{ DBGBUS_PERIPH, 51, 3},
	{ DBGBUS_PERIPH, 51, 4},
	{ DBGBUS_PERIPH, 51, 5},
	{ DBGBUS_PERIPH, 51, 6},
	{ DBGBUS_PERIPH, 51, 7},

	/* dsc4 */
	{ DBGBUS_PERIPH, 52, 0},
	{ DBGBUS_PERIPH, 52, 1},
	{ DBGBUS_PERIPH, 52, 2},
	{ DBGBUS_PERIPH, 52, 3},
	{ DBGBUS_PERIPH, 52, 4},
	{ DBGBUS_PERIPH, 52, 5},
	{ DBGBUS_PERIPH, 52, 6},
	{ DBGBUS_PERIPH, 52, 7},

	/* dsc5 */
	{ DBGBUS_PERIPH, 53, 0},
	{ DBGBUS_PERIPH, 53, 1},
	{ DBGBUS_PERIPH, 53, 2},
	{ DBGBUS_PERIPH, 53, 3},
	{ DBGBUS_PERIPH, 53, 4},
	{ DBGBUS_PERIPH, 53, 5},
	{ DBGBUS_PERIPH, 53, 6},
	{ DBGBUS_PERIPH, 53, 7},

	/* tear-check */
	/* INTF_0 */
	{ DBGBUS_PERIPH, 63, 0 },
	{ DBGBUS_PERIPH, 63, 1 },
	{ DBGBUS_PERIPH, 63, 2 },
	{ DBGBUS_PERIPH, 63, 3 },
	{ DBGBUS_PERIPH, 63, 4 },
	{ DBGBUS_PERIPH, 63, 5 },
	{ DBGBUS_PERIPH, 63, 6 },
	{ DBGBUS_PERIPH, 63, 7 },

	/* INTF_1 */
	{ DBGBUS_PERIPH, 64, 0 },
	{ DBGBUS_PERIPH, 64, 1 },
	{ DBGBUS_PERIPH, 64, 2 },
	{ DBGBUS_PERIPH, 64, 3 },
	{ DBGBUS_PERIPH, 64, 4 },
	{ DBGBUS_PERIPH, 64, 5 },
	{ DBGBUS_PERIPH, 64, 6 },
	{ DBGBUS_PERIPH, 64, 7 },

	/* INTF_2 */
	{ DBGBUS_PERIPH, 65, 0 },
	{ DBGBUS_PERIPH, 65, 1 },
	{ DBGBUS_PERIPH, 65, 2 },
	{ DBGBUS_PERIPH, 65, 3 },
	{ DBGBUS_PERIPH, 65, 4 },
	{ DBGBUS_PERIPH, 65, 5 },
	{ DBGBUS_PERIPH, 65, 6 },
	{ DBGBUS_PERIPH, 65, 7 },

	/* INTF_4 */
	{ DBGBUS_PERIPH, 66, 0 },
	{ DBGBUS_PERIPH, 66, 1 },
	{ DBGBUS_PERIPH, 66, 2 },
	{ DBGBUS_PERIPH, 66, 3 },
	{ DBGBUS_PERIPH, 66, 4 },
	{ DBGBUS_PERIPH, 66, 5 },
	{ DBGBUS_PERIPH, 66, 6 },
	{ DBGBUS_PERIPH, 66, 7 },

	/* INTF_5 */
	{ DBGBUS_PERIPH, 67, 0 },
	{ DBGBUS_PERIPH, 67, 1 },
	{ DBGBUS_PERIPH, 67, 2 },
	{ DBGBUS_PERIPH, 67, 3 },
	{ DBGBUS_PERIPH, 67, 4 },
	{ DBGBUS_PERIPH, 67, 5 },
	{ DBGBUS_PERIPH, 67, 6 },
	{ DBGBUS_PERIPH, 67, 7 },

	/* INTF_3 */
	{ DBGBUS_PERIPH, 73, 0 },
	{ DBGBUS_PERIPH, 73, 1 },
	{ DBGBUS_PERIPH, 73, 2 },
	{ DBGBUS_PERIPH, 73, 3 },
	{ DBGBUS_PERIPH, 73, 4 },
	{ DBGBUS_PERIPH, 73, 5 },
	{ DBGBUS_PERIPH, 73, 6 },
	{ DBGBUS_PERIPH, 73, 7 },

	/* cdwn */
	{ DBGBUS_PERIPH, 80, 0},
	{ DBGBUS_PERIPH, 80, 1},
	{ DBGBUS_PERIPH, 80, 2},

	{ DBGBUS_PERIPH, 81, 0},
	{ DBGBUS_PERIPH, 81, 1},
	{ DBGBUS_PERIPH, 81, 2},

	{ DBGBUS_PERIPH, 82, 0},
	{ DBGBUS_PERIPH, 82, 1},
	{ DBGBUS_PERIPH, 82, 2},
	{ DBGBUS_PERIPH, 82, 3},
	{ DBGBUS_PERIPH, 82, 4},
	{ DBGBUS_PERIPH, 82, 5},
	{ DBGBUS_PERIPH, 82, 6},
	{ DBGBUS_PERIPH, 82, 7},

	/* DPTX1 */
	{ DBGBUS_PERIPH, 68, 0},
	{ DBGBUS_PERIPH, 68, 1},
	{ DBGBUS_PERIPH, 68, 2},
	{ DBGBUS_PERIPH, 68, 3},
	{ DBGBUS_PERIPH, 68, 4},
	{ DBGBUS_PERIPH, 68, 5},
	{ DBGBUS_PERIPH, 68, 6},
	{ DBGBUS_PERIPH, 68, 7},

	/* DP */
	{ DBGBUS_PERIPH, 69, 0},
	{ DBGBUS_PERIPH, 69, 1},
	{ DBGBUS_PERIPH, 69, 2},
	{ DBGBUS_PERIPH, 69, 3},
	{ DBGBUS_PERIPH, 69, 4},
	{ DBGBUS_PERIPH, 69, 5},

	/* dsi0 */
	{ DBGBUS_PERIPH, 70, 0},
	{ DBGBUS_PERIPH, 70, 1},
	{ DBGBUS_PERIPH, 70, 2},
	{ DBGBUS_PERIPH, 70, 3},
	{ DBGBUS_PERIPH, 70, 4},
	{ DBGBUS_PERIPH, 70, 5},

	/* dsi1 */
	{ DBGBUS_PERIPH, 71, 0},
	{ DBGBUS_PERIPH, 71, 1},
	{ DBGBUS_PERIPH, 71, 2},
	{ DBGBUS_PERIPH, 71, 3},
	{ DBGBUS_PERIPH, 71, 4},
	{ DBGBUS_PERIPH, 71, 5},

	/* eDP */
	{ DBGBUS_PERIPH, 72, 0},
	{ DBGBUS_PERIPH, 72, 1},
	{ DBGBUS_PERIPH, 72, 2},
	{ DBGBUS_PERIPH, 72, 3},
	{ DBGBUS_PERIPH, 72, 4},
	{ DBGBUS_PERIPH, 72, 5},

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

static u32 dsi_dbg_bus_sdm845[] = {
	0x0001, 0x1001, 0x0001, 0x0011,
	0x1021, 0x0021, 0x0031, 0x0041,
	0x0051, 0x0061, 0x3061, 0x0061,
	0x2061, 0x2061, 0x1061, 0x1061,
	0x1061, 0x0071, 0x0071, 0x0071,
	0x0081, 0x0081, 0x00A1, 0x00A1,
	0x10A1, 0x20A1, 0x30A1, 0x10A1,
	0x10A1, 0x30A1, 0x20A1, 0x00B1,
	0x00C1, 0x00C1, 0x10C1, 0x20C1,
	0x30C1, 0x00D1, 0x00D1, 0x20D1,
	0x30D1, 0x00E1, 0x00E1, 0x00E1,
	0x00F1, 0x00F1, 0x0101, 0x0101,
	0x1101, 0x2101, 0x3101, 0x0111,
	0x0141, 0x1141, 0x0141, 0x1141,
	0x1141, 0x0151, 0x0151, 0x1151,
	0x2151, 0x3151, 0x0161, 0x0161,
	0x1161, 0x0171, 0x0171, 0x0181,
	0x0181, 0x0191, 0x0191, 0x01A1,
	0x01A1, 0x01B1, 0x01B1, 0x11B1,
	0x21B1, 0x01C1, 0x01C1, 0x11C1,
	0x21C1, 0x31C1, 0x01D1, 0x01D1,
	0x01D1, 0x01D1, 0x11D1, 0x21D1,
	0x21D1, 0x01E1, 0x01E1, 0x01F1,
	0x01F1, 0x0201, 0x0201, 0x0211,
	0x0221, 0x0231, 0x0241, 0x0251,
	0x0281, 0x0291, 0x0281, 0x0291,
	0x02A1, 0x02B1, 0x02C1, 0x0321,
	0x0321, 0x1321, 0x2321, 0x3321,
	0x0331, 0x0331, 0x1331, 0x0341,
	0x0341, 0x1341, 0x2341, 0x3341,
	0x0351, 0x0361, 0x0361, 0x1361,
	0x2361, 0x0371, 0x0381, 0x0391,
	0x03C1, 0x03D1, 0x03E1, 0x03F1,
};

/**
 * _sde_dbg_enable_power - use callback to turn power on for hw register access
 * @enable: whether to turn power on or off
 * Return: zero if success; error code otherwise
 */
static inline int _sde_dbg_enable_power(int enable)
{
	if (!sde_dbg_base.power_ctrl.enable_fn)
		return -EINVAL;
	return sde_dbg_base.power_ctrl.enable_fn(
			sde_dbg_base.power_ctrl.handle,
			sde_dbg_base.power_ctrl.client,
			enable);
}

/**
 * _sde_power_check - check if power needs to enabled
 * @dump_mode: to check if power need to be enabled
 * Return: true if success; false otherwise
 */
static inline bool _sde_power_check(enum sde_dbg_dump_context dump_mode)
{
	return (dump_mode == SDE_DBG_DUMP_CLK_ENABLED_CTX ||
		dump_mode == SDE_DBG_DUMP_IRQ_CTX) ? false : true;
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
		char *base_addr, char *addr, size_t len_bytes, u32 **dump_mem)
{
	u32 in_log, in_mem, len_align, len_padded;
	u32 *dump_addr = NULL;
	char *end_addr;
	int i;
	int rc;

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
				dump_name, (unsigned long)(addr - base_addr),
					len_bytes);

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
				(unsigned long)(addr - base_addr));
		} else {
			in_mem = 0;
			pr_err("dump_mem: kzalloc fails!\n");
		}
	}

	if (_sde_power_check(sde_dbg_base.dump_mode)) {
		rc = _sde_dbg_enable_power(true);
		if (rc) {
			pr_err("failed to enable power %d\n", rc);
			return;
		}
	}

	for (i = 0; i < len_align; i++) {
		u32 x0, x4, x8, xc;

		x0 = (addr < end_addr) ? readl_relaxed(addr + 0x0) : 0;
		x4 = (addr + 0x4 < end_addr) ? readl_relaxed(addr + 0x4) : 0;
		x8 = (addr + 0x8 < end_addr) ? readl_relaxed(addr + 0x8) : 0;
		xc = (addr + 0xc < end_addr) ? readl_relaxed(addr + 0xc) : 0;

		if (in_log)
			dev_info(sde_dbg_base.dev,
					"0x%lx : %08x %08x %08x %08x\n",
					(unsigned long)(addr - base_addr),
					x0, x4, x8, xc);

		if (dump_addr) {
			dump_addr[i * 4] = x0;
			dump_addr[i * 4 + 1] = x4;
			dump_addr[i * 4 + 2] = x8;
			dump_addr[i * 4 + 3] = xc;
		}

		addr += REG_DUMP_ALIGN;
	}

	if (_sde_power_check(sde_dbg_base.dump_mode))
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

	if (range_node->start == 0 && range_node->end == 0) {
		length = max_offset;
	} else if (range_node->start < max_offset) {
		if (range_node->end > max_offset)
			length = max_offset - range_node->start;
		else if (range_node->start < range_node->end)
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

static const char *const exclude_modules[] = {
	"vbif_rt",
	"vbif_nrt",
	"wb_2",
	NULL
};

static bool is_block_exclude(char **modules, char *name)
{
	char **ptr = modules;

	while (*ptr != NULL) {
		if (!strcmp(name, *ptr))
			return true;
		++ptr;
	}
	return false;
}

/**
 * _sde_dump_reg_by_ranges - dump ranges or full range of the register blk base
 * @dbg: register blk base structure
 * @reg_dump_flag: dump target, memory, kernel log, or both
 */
static void _sde_dump_reg_by_ranges(struct sde_dbg_reg_base *dbg,
	u32 reg_dump_flag, bool dump_secure)
{
	char *addr;
	size_t len;
	struct sde_dbg_reg_range *range_node;

	if (!dbg || !(dbg->base || dbg->cb)) {
		pr_err("dbg base is null!\n");
		return;
	}

	dev_info(sde_dbg_base.dev, "%s:=========%s DUMP=========\n", __func__,
			dbg->name);
	if (dbg->cb) {
		dbg->cb(dbg->cb_ptr);
	/* If there is a list to dump the registers by ranges, use the ranges */
	} else if (!list_empty(&dbg->sub_range_list)) {
		/* sort the list by start address first */
		list_sort(NULL, &dbg->sub_range_list, _sde_dump_reg_range_cmp);
		list_for_each_entry(range_node, &dbg->sub_range_list, head) {
			len = _sde_dbg_get_dump_range(&range_node->offset,
				dbg->max_offset);
			addr = dbg->base + range_node->offset.start;

			if (dump_secure &&
				is_block_exclude((char **)exclude_modules,
				range_node->range_name))
				continue;

			pr_debug("%s: range_base=0x%pK start=0x%x end=0x%x\n",
				range_node->range_name,
				addr, range_node->offset.start,
				range_node->offset.end);

			_sde_dump_reg(range_node->range_name, reg_dump_flag,
					dbg->base, addr, len,
					&range_node->reg_dump);
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
				&dbg->reg_dump);
	}
}

/**
 * _sde_dump_reg_by_blk - dump a named register base region
 * @blk_name: register blk name
 */
static void _sde_dump_reg_by_blk(const char *blk_name, bool dump_secure)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;

	if (!dbg_base)
		return;

	list_for_each_entry(blk_base, &dbg_base->reg_base_list, reg_base_head) {
		if (strlen(blk_base->name) &&
			!strcmp(blk_base->name, blk_name)) {
			_sde_dump_reg_by_ranges(blk_base,
				dbg_base->enable_reg_dump, dump_secure);
			break;
		}
	}
}

/**
 * _sde_dump_reg_all - dump all register regions
 */
static void _sde_dump_reg_all(bool dump_secure)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;

	if (!dbg_base)
		return;

	list_for_each_entry(blk_base, &dbg_base->reg_base_list, reg_base_head) {

		if (!strlen(blk_base->name))
			continue;

		if (dump_secure &&
			is_block_exclude((char **)exclude_modules,
			blk_base->name))
			continue;

		_sde_dump_reg_by_blk(blk_base->name, dump_secure);
	}
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
	int rc;

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

	if (_sde_power_check(sde_dbg_base.dump_mode)) {
		rc = _sde_dbg_enable_power(true);
		if (rc) {
			pr_err("failed to enable power %d\n", rc);
			return;
		}
	}

	for (i = 0; i < bus->cmn.entries_size; i++) {
		head = bus->entries + i;
		writel_relaxed(TEST_MASK(head->block_id, head->test_id),
				mem_base + head->wr_addr);
		wmb(); /* make sure test bits were written */

		if (bus->cmn.flags & DBGBUS_FLAGS_DSPP) {
			offset = DBGBUS_DSPP_STATUS;
			/* keep DSPP test point enabled */
			if (head->wr_addr != DBGBUS_DSPP)
				writel_relaxed(0x7001, mem_base + DBGBUS_DSPP);
		} else {
			offset = head->wr_addr + 0x4;
		}

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

		if (head->analyzer)
			head->analyzer(mem_base, head, status);

		/* Disable debug bus once we are done */
		writel_relaxed(0, mem_base + head->wr_addr);
		if (bus->cmn.flags & DBGBUS_FLAGS_DSPP &&
						head->wr_addr != DBGBUS_DSPP)
			writel_relaxed(0x0, mem_base + DBGBUS_DSPP);
	}

	if (_sde_power_check(sde_dbg_base.dump_mode))
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
	u32 value, d0, d1;
	unsigned long reg, reg1, reg2;
	struct vbif_debug_bus_entry *head;
	phys_addr_t phys = 0;
	int i, list_size = 0;
	void __iomem *mem_base = NULL;
	struct vbif_debug_bus_entry *dbg_bus;
	u32 bus_size;
	struct sde_dbg_reg_base *reg_base;
	int rc;

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


	if (_sde_power_check(sde_dbg_base.dump_mode)) {
		rc = _sde_dbg_enable_power(true);
		if (rc) {
			pr_err("failed to enable power %d\n", rc);
			return;
		}
	}

	value = readl_relaxed(mem_base + MMSS_VBIF_CLKON);
	writel_relaxed(value | BIT(1), mem_base + MMSS_VBIF_CLKON);

	/* make sure that vbif core is on */
	wmb();

	/**
	 * Extract VBIF error info based on XIN halt and error status.
	 * If the XIN client is not in HALT state, or an error is detected,
	 * then retrieve the VBIF error info for it.
	 */
	reg = readl_relaxed(mem_base + MMSS_VBIF_XIN_HALT_CTRL1);
	reg1 = readl_relaxed(mem_base + MMSS_VBIF_PND_ERR);
	reg2 = readl_relaxed(mem_base + MMSS_VBIF_SRC_ERR);
	dev_err(sde_dbg_base.dev,
			"XIN HALT:0x%lX, PND ERR:0x%lX, SRC ERR:0x%lX\n",
			reg, reg1, reg2);
	reg >>= 16;
	reg &= ~(reg1 | reg2);
	for (i = 0; i < MMSS_VBIF_CLIENT_NUM; i++) {
		if (!test_bit(0, &reg)) {
			writel_relaxed(i, mem_base + MMSS_VBIF_ERR_INFO);
			/* make sure reg write goes through */
			wmb();

			d0 = readl_relaxed(mem_base + MMSS_VBIF_ERR_INFO);
			d1 = readl_relaxed(mem_base + MMSS_VBIF_ERR_INFO_1);

			dev_err(sde_dbg_base.dev,
					"Client:%d, errinfo=0x%X, errinfo1=0x%X\n",
					i, d0, d1);
		}
		reg >>= 1;
	}

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

	if (_sde_power_check(sde_dbg_base.dump_mode))
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
	bool dump_dbgbus_vbif_rt, bool dump_all, bool dump_secure)
{
	int i;

	mutex_lock(&sde_dbg_base.mutex);

	if (dump_all)
		sde_evtlog_dump_all(sde_dbg_base.evtlog);

	if (dump_all || !blk_arr || !len) {
		_sde_dump_reg_all(dump_secure);
	} else {
		for (i = 0; i < len; i++) {
			if (blk_arr[i] != NULL)
				_sde_dump_reg_by_ranges(blk_arr[i],
					sde_dbg_base.enable_reg_dump,
					dump_secure);
		}
	}

	if (dump_dbgbus_sde)
		_sde_dbg_dump_sde_dbg_bus(&sde_dbg_base.dbgbus_sde);

	if (dump_dbgbus_vbif_rt)
		_sde_dbg_dump_vbif_dbg_bus(&sde_dbg_base.dbgbus_vbif_rt);

	if (sde_dbg_base.dsi_dbg_bus || dump_all)
		dsi_ctrl_debug_dump(sde_dbg_base.dbgbus_dsi.entries,
				    sde_dbg_base.dbgbus_dsi.size);

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
		sde_dbg_base.dump_all,
		sde_dbg_base.dump_secure);
}

void sde_dbg_dump(enum sde_dbg_dump_context dump_mode, const char *name, ...)
{
	int i, index = 0;
	bool do_panic = false;
	bool dump_dbgbus_sde = false;
	bool dump_dbgbus_vbif_rt = false;
	bool dump_all = false;
	bool dump_secure = false;
	va_list args;
	char *blk_name = NULL;
	struct sde_dbg_reg_base *blk_base = NULL;
	struct sde_dbg_reg_base **blk_arr;
	u32 blk_len;

	if (!sde_evtlog_is_enabled(sde_dbg_base.evtlog, SDE_EVTLOG_ALWAYS))
		return;

	if ((dump_mode == SDE_DBG_DUMP_IRQ_CTX) &&
		work_pending(&sde_dbg_base.dump_work))
		return;

	blk_arr = &sde_dbg_base.req_dump_blks[0];
	blk_len = ARRAY_SIZE(sde_dbg_base.req_dump_blks);

	memset(sde_dbg_base.req_dump_blks, 0,
			sizeof(sde_dbg_base.req_dump_blks));
	sde_dbg_base.dump_all = false;
	sde_dbg_base.dump_mode = dump_mode;

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

		if (!strcmp(blk_name, "dsi_dbg_bus"))
			sde_dbg_base.dsi_dbg_bus = true;

		if (!strcmp(blk_name, "panic"))
			do_panic = true;

		if (!strcmp(blk_name, "secure"))
			dump_secure = true;
	}
	va_end(args);

	if (dump_mode == SDE_DBG_DUMP_IRQ_CTX) {
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
				dump_dbgbus_sde, dump_dbgbus_vbif_rt, dump_all,
				dump_secure);
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
			pr_err("panic underrun\n");
			SDE_DBG_DUMP_WQ("all", "dbg_bus", "vbif_dbg_bus",
					"panic");
		}

		if (!strcmp(blk_name, "reset_hw_panic") &&
				sde_dbg_base.debugfs_ctrl &
				DBG_CTRL_RESET_HW_PANIC) {
			pr_debug("reset hw panic\n");
			panic("reset_hw");
		}
	}

	va_end(args);
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
	mutex_lock(&sde_dbg_base.mutex);
	sde_dbg_base.cur_evt_index = 0;
	sde_dbg_base.evtlog->first = sde_dbg_base.evtlog->curr + 1;
	sde_dbg_base.evtlog->last =
		sde_dbg_base.evtlog->first + SDE_EVTLOG_ENTRY;
	mutex_unlock(&sde_dbg_base.mutex);
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

	mutex_lock(&sde_dbg_base.mutex);
	len = sde_evtlog_dump_to_buffer(sde_dbg_base.evtlog,
			evtlog_buf, SDE_EVTLOG_BUF_MAX,
			!sde_dbg_base.cur_evt_index, true);
	sde_dbg_base.cur_evt_index++;
	mutex_unlock(&sde_dbg_base.mutex);

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
	_sde_dump_array(NULL, 0, sde_dbg_base.panic_on_err, "dump_debugfs",
		true, true, true, false);

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

/*
 * sde_evtlog_filter_show - read callback for evtlog filter
 * @s: pointer to seq_file object
 * @data: pointer to private data
 */
static int sde_evtlog_filter_show(struct seq_file *s, void *data)
{
	struct sde_dbg_evtlog *evtlog;
	char buffer[64];
	int i;

	if (!s || !s->private)
		return -EINVAL;

	evtlog = s->private;

	for (i = 0; !sde_evtlog_get_filter(
				evtlog, i, buffer, ARRAY_SIZE(buffer)); ++i)
		seq_printf(s, "*%s*\n", buffer);
	return 0;
}

/*
 * sde_evtlog_filter_open - debugfs open handler for evtlog filter
 * @inode: debugfs inode
 * @file: file handle
 * Returns: zero on success
 */
static int sde_evtlog_filter_open(struct inode *inode, struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	return single_open(file, sde_evtlog_filter_show, inode->i_private);
}

/*
 * sde_evtlog_filter_write - write callback for evtlog filter
 * @file: pointer to file structure
 * @user_buf: pointer to incoming user data
 * @count: size of incoming user buffer
 * @ppos: pointer to file offset
 */
static ssize_t sde_evtlog_filter_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	char *tmp_filter = NULL;
	ssize_t rc = 0;

	if (!user_buf)
		return -EINVAL;

	if (count > 0) {
		/* copy user provided string and null terminate it */
		tmp_filter = kzalloc(count + 1, GFP_KERNEL);
		if (!tmp_filter)
			rc = -ENOMEM;
		else if (copy_from_user(tmp_filter, user_buf, count))
			rc = -EFAULT;
	}

	/* update actual filter configuration on success */
	if (!rc) {
		sde_evtlog_set_filter(sde_dbg_base.evtlog, tmp_filter);
		rc = count;
	}
	kfree(tmp_filter);

	return rc;
}

static const struct file_operations sde_evtlog_filter_fops = {
	.open =		sde_evtlog_filter_open,
	.write =	sde_evtlog_filter_write,
	.read =		seq_read,
	.llseek =	seq_lseek,
	.release =	seq_release
};

static int sde_recovery_regdump_open(struct inode *inode, struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;

	/* initialize to start position */
	sde_dbg_base.regbuf.rpos = 0;
	sde_dbg_base.regbuf.cur_blk = NULL;
	sde_dbg_base.regbuf.dump_done = false;

	return 0;
}

static ssize_t _sde_dbg_dump_reg_rows(u32 reg_start,
		void *start, int count, char *buf, int buflen)
{
	int i;
	int len = 0;
	u32 *addr;
	u32  reg_offset = 0;
	int rows = min(count / DUMP_CLMN_COUNT, DUMP_MAX_LINES_PER_BLK);

	if (!start || !buf) {
		pr_err("invalid address for dump\n");
		return len;
	}

	if (buflen < PAGE_SIZE) {
		pr_err("buffer too small for dump\n");
		return len;
	}

	for (i = 0; i < rows; i++) {
		addr = start + (i * DUMP_CLMN_COUNT * sizeof(u32));
		reg_offset = reg_start + (i * DUMP_CLMN_COUNT * sizeof(u32));
		if (buflen < (len + DUMP_LINE_SIZE))
			break;

		len += snprintf(buf + len, DUMP_LINE_SIZE,
				"0x%.8X | %.8X %.8X %.8X %.8X\n",
				reg_offset, addr[0], addr[1], addr[2], addr[3]);
	}

	return len;
}

static int  _sde_dbg_recovery_dump_sub_blk(struct sde_dbg_reg_range *sub_blk,
		char  *buf, int buflen)
{
	int count = 0;
	int len = 0;

	if (!sub_blk || (buflen < PAGE_SIZE)) {
		pr_err("invalid params buflen:%d subblk valid:%d\n",
				buflen, sub_blk != NULL);
		return len;
	}

	count = (sub_blk->offset.end - sub_blk->offset.start) / (sizeof(u32));
	if (count < DUMP_CLMN_COUNT) {
		pr_err("invalid count for register dumps :%d\n", count);
		return len;
	}

	len += snprintf(buf + len, DUMP_LINE_SIZE,
			"------------------------------------------\n");
	len += snprintf(buf + len, DUMP_LINE_SIZE,
			"**** sub block [%s] - size:%d ****\n",
			sub_blk->range_name, count);
	len += _sde_dbg_dump_reg_rows(sub_blk->offset.start, sub_blk->reg_dump,
			count, buf + len, buflen - len);

	return len;
}

static int  _sde_dbg_recovery_dump_reg_blk(struct sde_dbg_reg_base *blk,
		char *buf, int buf_size, int *out_len)
{
	int ret = 0;
	int len = 0;
	struct sde_dbg_reg_range *sub_blk;

	if (buf_size < PAGE_SIZE) {
		pr_err("buffer too small for dump\n");
		return len;
	}

	if (!blk || !strlen(blk->name)) {
		len += snprintf(buf + len, DUMP_LINE_SIZE,
			"Found one invalid block - skip dump\n");
		*out_len = len;
		return len;
	}

	len += snprintf(buf + len, DUMP_LINE_SIZE,
			"******************************************\n");
	len += snprintf(buf + len, DUMP_LINE_SIZE,
			"==========================================\n");
	len += snprintf(buf + len, DUMP_LINE_SIZE,
			"*********** DUMP of %s block *************\n",
			blk->name);
	len += snprintf(buf + len, DUMP_LINE_SIZE,
			"count:%ld max-off:0x%lx has_sub_blk:%d\n",
			blk->cnt, blk->max_offset,
			!list_empty(&blk->sub_range_list));

	if (list_empty(&blk->sub_range_list)) {
		len += _sde_dbg_dump_reg_rows(0, blk->reg_dump,
				blk->max_offset / sizeof(u32), buf + len,
				buf_size - len);
	} else {
		list_for_each_entry(sub_blk, &blk->sub_range_list, head)
			len += _sde_dbg_recovery_dump_sub_blk(sub_blk,
					buf + len, buf_size - len);
	}
	*out_len = len;

	return ret;
}

static ssize_t sde_recovery_regdump_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	int usize = 0;
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_regbuf *rbuf = &dbg_base->regbuf;

	mutex_lock(&sde_dbg_base.mutex);
	if (!rbuf->dump_done && !rbuf->cur_blk) {
		if (!rbuf->buf)
			rbuf->buf = kzalloc(DUMP_BUF_SIZE, GFP_KERNEL);
		if (!rbuf->buf) {
			len =  -ENOMEM;
			goto err;
		}
		rbuf->rpos = 0;
		rbuf->len = 0;
		rbuf->buf_size = DUMP_BUF_SIZE;

		rbuf->cur_blk = list_first_entry(&dbg_base->reg_base_list,
				struct sde_dbg_reg_base, reg_base_head);
		if (rbuf->cur_blk)
			_sde_dbg_recovery_dump_reg_blk(rbuf->cur_blk,
					rbuf->buf,
					rbuf->buf_size,
					&rbuf->len);
		pr_debug("dumping done for blk:%s len:%d\n", rbuf->cur_blk ?
				rbuf->cur_blk->name : "unknown", rbuf->len);
	} else if (rbuf->len == rbuf->rpos && rbuf->cur_blk) {
		rbuf->rpos = 0;
		rbuf->len = 0;
		rbuf->buf_size = DUMP_BUF_SIZE;

		if (rbuf->cur_blk == list_last_entry(&dbg_base->reg_base_list,
					struct sde_dbg_reg_base, reg_base_head))
			rbuf->cur_blk = NULL;
		else
			rbuf->cur_blk = list_next_entry(rbuf->cur_blk,
					reg_base_head);

		if (rbuf->cur_blk)
			_sde_dbg_recovery_dump_reg_blk(rbuf->cur_blk,
					rbuf->buf,
					rbuf->buf_size,
					&rbuf->len);
		pr_debug("dumping done for blk:%s len:%d\n", rbuf->cur_blk ?
				rbuf->cur_blk->name : "unknown", rbuf->len);
	}

	if ((rbuf->len - rbuf->rpos) > 0) {
		usize = ((rbuf->len - rbuf->rpos) > count) ?
			count  : rbuf->len - rbuf->rpos;
		if (copy_to_user(ubuf, rbuf->buf + rbuf->rpos, usize)) {
			len =  -EFAULT;
			goto err;
		}

		len = usize;
		rbuf->rpos += usize;
		*ppos += usize;
	}

	if (!len && rbuf->buf)
		rbuf->dump_done = true;
err:
	mutex_unlock(&sde_dbg_base.mutex);

	return len;
}

static const struct file_operations sde_recovery_reg_fops = {
	.open = sde_recovery_regdump_open,
	.read = sde_recovery_regdump_read,
};

static int sde_recovery_dbgbus_dump_open(struct inode *inode, struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;

	mutex_lock(&sde_dbg_base.mutex);
	sde_dbg_base.dbgbus_dump_idx = 0;
	mutex_unlock(&sde_dbg_base.mutex);

	return 0;
}

static ssize_t sde_recovery_dbgbus_dump_read(struct file *file,
		char __user *buff,
		size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char evtlog_buf[SDE_EVTLOG_BUF_MAX];
	u32 *data;
	struct sde_dbg_sde_debug_bus *bus;

	mutex_lock(&sde_dbg_base.mutex);
	bus = &sde_dbg_base.dbgbus_sde;
	if (!bus->cmn.dumped_content || !bus->cmn.entries_size)
		goto dump_done;

	if (sde_dbg_base.dbgbus_dump_idx <=
			((bus->cmn.entries_size - 1) * DUMP_CLMN_COUNT)) {
		data = &bus->cmn.dumped_content[
			sde_dbg_base.dbgbus_dump_idx];
		len = snprintf(evtlog_buf, SDE_EVTLOG_BUF_MAX,
				"0x%.8X | %.8X %.8X %.8X %.8X\n",
				sde_dbg_base.dbgbus_dump_idx,
				data[0], data[1], data[2], data[3]);
		sde_dbg_base.dbgbus_dump_idx += DUMP_CLMN_COUNT;
		if ((count < len) || copy_to_user(buff, evtlog_buf, len)) {
			len = -EFAULT;
			goto dump_done;
		}
		*ppos += len;
	}
dump_done:
	mutex_unlock(&sde_dbg_base.mutex);

	return len;
}

static const struct file_operations sde_recovery_dbgbus_fops = {
	.open = sde_recovery_dbgbus_dump_open,
	.read = sde_recovery_dbgbus_dump_read,
};

static int sde_recovery_vbif_dbgbus_dump_open(struct inode *inode,
		struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;

	mutex_lock(&sde_dbg_base.mutex);
	sde_dbg_base.vbif_dbgbus_dump_idx = 0;
	mutex_unlock(&sde_dbg_base.mutex);

	return 0;
}

static ssize_t sde_recovery_vbif_dbgbus_dump_read(struct file *file,
		char __user *buff,
		size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char evtlog_buf[SDE_EVTLOG_BUF_MAX];
	int i;
	u32 *data;
	u32 list_size = 0;
	struct vbif_debug_bus_entry *head;
	struct sde_dbg_vbif_debug_bus *bus;

	mutex_lock(&sde_dbg_base.mutex);
	bus = &sde_dbg_base.dbgbus_vbif_rt;
	if (!bus->cmn.dumped_content || !bus->cmn.entries_size)
		goto dump_done;

	/* calculate total number of test point */
	for (i = 0; i < bus->cmn.entries_size; i++) {
		head = bus->entries + i;
		list_size += (head->block_cnt * head->test_pnt_cnt);
	}

	/* 4 entries for each test point*/
	list_size *= DUMP_CLMN_COUNT;
	if (sde_dbg_base.vbif_dbgbus_dump_idx < list_size) {
		data = &bus->cmn.dumped_content[
			sde_dbg_base.vbif_dbgbus_dump_idx];
		len = snprintf(evtlog_buf, SDE_EVTLOG_BUF_MAX,
				"0x%.8X | %.8X %.8X %.8X %.8X\n",
				sde_dbg_base.vbif_dbgbus_dump_idx,
				data[0], data[1], data[2], data[3]);
		sde_dbg_base.vbif_dbgbus_dump_idx += DUMP_CLMN_COUNT;
		if ((count < len) || copy_to_user(buff, evtlog_buf, len)) {
			len = -EFAULT;
			goto dump_done;
		}
		*ppos += len;
	}
dump_done:
	mutex_unlock(&sde_dbg_base.mutex);

	return len;
}

static const struct file_operations sde_recovery_vbif_dbgbus_fops = {
	.open = sde_recovery_vbif_dbgbus_dump_open,
	.read = sde_recovery_vbif_dbgbus_dump_read,
};

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

	if (sscanf(buf, "%5x %x", &off, &cnt) != 2)
		return -EFAULT;

	if (off > dbg->max_offset)
		return -EINVAL;

	if (off % sizeof(u32))
		return -EINVAL;

	if (cnt > (dbg->max_offset - off))
		cnt = dbg->max_offset - off;

	if (cnt == 0)
		return -EINVAL;

	if (!sde_dbg_reg_base_is_valid_range(off, cnt))
		return -EINVAL;

	mutex_lock(&sde_dbg_base.mutex);
	dbg->off = off;
	dbg->cnt = cnt;
	mutex_unlock(&sde_dbg_base.mutex);

	pr_debug("offset=%x cnt=%x\n", off, cnt);

	return count;
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
	int rc;

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

	if (off % sizeof(u32))
		return -EFAULT;

	mutex_lock(&sde_dbg_base.mutex);
	if (off >= dbg->max_offset) {
		mutex_unlock(&sde_dbg_base.mutex);
		return -EFAULT;
	}

	rc = _sde_dbg_enable_power(true);
	if (rc) {
		mutex_unlock(&sde_dbg_base.mutex);
		pr_err("failed to enable power %d\n", rc);
		return rc;
	}

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
	int rc;

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
		char dump_buf[64];
		char *ptr;
		int cnt, tot;

		dbg->buf_len = sizeof(dump_buf) *
			DIV_ROUND_UP(dbg->cnt, ROW_BYTES);
		dbg->buf = kzalloc(dbg->buf_len, GFP_KERNEL);

		if (!dbg->buf) {
			mutex_unlock(&sde_dbg_base.mutex);
			return -ENOMEM;
		}

		if (dbg->off % sizeof(u32)) {
			mutex_unlock(&sde_dbg_base.mutex);
			return -EFAULT;
		}

		ptr = dbg->base + dbg->off;
		tot = 0;

		rc = _sde_dbg_enable_power(true);
		if (rc) {
			mutex_unlock(&sde_dbg_base.mutex);
			pr_err("failed to enable power %d\n", rc);
			return rc;
		}

		for (cnt = dbg->cnt; cnt > 0; cnt -= ROW_BYTES) {
			hex_dump_to_buffer(ptr, min(cnt, ROW_BYTES),
					   ROW_BYTES, GROUP_BYTES, dump_buf,
					   sizeof(dump_buf), false);
			len = scnprintf(dbg->buf + tot, dbg->buf_len - tot,
					"0x%08x: %s\n",
					((int) (unsigned long) ptr) -
					((int) (unsigned long) dbg->base),
					dump_buf);

			ptr += ROW_BYTES;
			tot += len;
			if (tot >= dbg->buf_len)
				break;
		}

		_sde_dbg_enable_power(false);

		dbg->buf_len = tot;
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

int sde_dbg_debugfs_register(struct dentry *debugfs_root)
{
	static struct sde_dbg_base *dbg = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;
	char debug_name[80] = "";

	if (!debugfs_root)
		return -EINVAL;

	debugfs_create_file("dbg_ctrl", 0600, debugfs_root, NULL,
			&sde_dbg_ctrl_fops);
	debugfs_create_file("dump", 0600, debugfs_root, NULL,
			&sde_evtlog_fops);
	debugfs_create_u32("enable", 0600, debugfs_root,
			&(sde_dbg_base.evtlog->enable));
	debugfs_create_file("filter", 0600, debugfs_root,
			sde_dbg_base.evtlog,
			&sde_evtlog_filter_fops);
	debugfs_create_u32("panic", 0600, debugfs_root,
			&sde_dbg_base.panic_on_err);
	debugfs_create_u32("reg_dump", 0600, debugfs_root,
			&sde_dbg_base.enable_reg_dump);
	debugfs_create_file("recovery_reg", 0400, debugfs_root, NULL,
			&sde_recovery_reg_fops);
	debugfs_create_file("recovery_dbgbus", 0400, debugfs_root, NULL,
			&sde_recovery_dbgbus_fops);
	debugfs_create_file("recovery_vbif_dbgbus", 0400, debugfs_root, NULL,
			&sde_recovery_vbif_dbgbus_fops);

	if (dbg->dbgbus_sde.entries) {
		dbg->dbgbus_sde.cmn.name = DBGBUS_NAME_SDE;
		snprintf(debug_name, sizeof(debug_name), "%s_dbgbus",
				dbg->dbgbus_sde.cmn.name);
		dbg->dbgbus_sde.cmn.enable_mask = DEFAULT_DBGBUS_SDE;
		debugfs_create_u32(debug_name, 0600, debugfs_root,
				&dbg->dbgbus_sde.cmn.enable_mask);
	}

	if (dbg->dbgbus_vbif_rt.entries) {
		dbg->dbgbus_vbif_rt.cmn.name = DBGBUS_NAME_VBIF_RT;
		snprintf(debug_name, sizeof(debug_name), "%s_dbgbus",
				dbg->dbgbus_vbif_rt.cmn.name);
		dbg->dbgbus_vbif_rt.cmn.enable_mask = DEFAULT_DBGBUS_VBIFRT;
		debugfs_create_u32(debug_name, 0600, debugfs_root,
				&dbg->dbgbus_vbif_rt.cmn.enable_mask);
	}

	list_for_each_entry(blk_base, &dbg->reg_base_list, reg_base_head) {
		snprintf(debug_name, sizeof(debug_name), "%s_off",
				blk_base->name);
		debugfs_create_file(debug_name, 0600, debugfs_root, blk_base,
				&sde_off_fops);

		snprintf(debug_name, sizeof(debug_name), "%s_reg",
				blk_base->name);
		debugfs_create_file(debug_name, 0600, debugfs_root, blk_base,
				&sde_reg_fops);
	}

	return 0;
}

static void _sde_dbg_debugfs_destroy(void)
{
}

void sde_dbg_init_dbg_buses(u32 hwversion)
{
	static struct sde_dbg_base *dbg = &sde_dbg_base;

	memset(&dbg->dbgbus_sde, 0, sizeof(dbg->dbgbus_sde));
	memset(&dbg->dbgbus_vbif_rt, 0, sizeof(dbg->dbgbus_vbif_rt));

	if (IS_MSM8998_TARGET(hwversion)) {
		dbg->dbgbus_sde.entries = dbg_bus_sde_8998;
		dbg->dbgbus_sde.cmn.entries_size = ARRAY_SIZE(dbg_bus_sde_8998);
		dbg->dbgbus_sde.cmn.flags = DBGBUS_FLAGS_DSPP;

		dbg->dbgbus_vbif_rt.entries = vbif_dbg_bus_msm8998;
		dbg->dbgbus_vbif_rt.cmn.entries_size =
				ARRAY_SIZE(vbif_dbg_bus_msm8998);
		dbg->dbgbus_dsi.entries = NULL;
		dbg->dbgbus_dsi.size = 0;
	} else if (IS_SDM845_TARGET(hwversion) || IS_SDM670_TARGET(hwversion)) {
		dbg->dbgbus_sde.entries = dbg_bus_sde_sdm845;
		dbg->dbgbus_sde.cmn.entries_size =
				ARRAY_SIZE(dbg_bus_sde_sdm845);
		dbg->dbgbus_sde.cmn.flags = DBGBUS_FLAGS_DSPP;

		/* vbif is unchanged vs 8998 */
		dbg->dbgbus_vbif_rt.entries = vbif_dbg_bus_msm8998;
		dbg->dbgbus_vbif_rt.cmn.entries_size =
				ARRAY_SIZE(vbif_dbg_bus_msm8998);
		dbg->dbgbus_dsi.entries = dsi_dbg_bus_sdm845;
		dbg->dbgbus_dsi.size = ARRAY_SIZE(dsi_dbg_bus_sdm845);
	} else if (IS_SM8150_TARGET(hwversion) || IS_SM6150_TARGET(hwversion) ||
				IS_SDMMAGPIE_TARGET(hwversion)) {
		dbg->dbgbus_sde.entries = dbg_bus_sde_sm8150;
		dbg->dbgbus_sde.cmn.entries_size =
				ARRAY_SIZE(dbg_bus_sde_sm8150);
		dbg->dbgbus_sde.cmn.flags = DBGBUS_FLAGS_DSPP;

		dbg->dbgbus_vbif_rt.entries = vbif_dbg_bus_msm8998;
		dbg->dbgbus_vbif_rt.cmn.entries_size =
				ARRAY_SIZE(vbif_dbg_bus_msm8998);
		dbg->dbgbus_dsi.entries = dsi_dbg_bus_sdm845;
		dbg->dbgbus_dsi.size = ARRAY_SIZE(dsi_dbg_bus_sdm845);
	} else {
		pr_err("unsupported chipset id %X\n", hwversion);
	}
}

int sde_dbg_init(struct device *dev, struct sde_dbg_power_ctrl *power_ctrl)
{
	if (!dev || !power_ctrl) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mutex_init(&sde_dbg_base.mutex);
	INIT_LIST_HEAD(&sde_dbg_base.reg_base_list);
	sde_dbg_base.dev = dev;
	sde_dbg_base.power_ctrl = *power_ctrl;

	sde_dbg_base.evtlog = sde_evtlog_init();
	if (IS_ERR_OR_NULL(sde_dbg_base.evtlog))
		return PTR_ERR(sde_dbg_base.evtlog);

	sde_dbg_base_evtlog = sde_dbg_base.evtlog;

	INIT_WORK(&sde_dbg_base.dump_work, _sde_dump_work);
	sde_dbg_base.work_panic = false;
	sde_dbg_base.panic_on_err = DEFAULT_PANIC;
	sde_dbg_base.enable_reg_dump = DEFAULT_REGDUMP;
	memset(&sde_dbg_base.regbuf, 0, sizeof(sde_dbg_base.regbuf));

	pr_info("evtlog_status: enable:%d, panic:%d, dump:%d\n",
		sde_dbg_base.evtlog->enable, sde_dbg_base.panic_on_err,
		sde_dbg_base.enable_reg_dump);

	return 0;
}

static void sde_dbg_reg_base_destroy(void)
{
	struct sde_dbg_reg_range *range_node, *range_tmp;
	struct sde_dbg_reg_base *blk_base, *blk_tmp;
	struct sde_dbg_base *dbg_base = &sde_dbg_base;

	if (!dbg_base)
		return;

	list_for_each_entry_safe(blk_base, blk_tmp, &dbg_base->reg_base_list,
							reg_base_head) {
		list_for_each_entry_safe(range_node, range_tmp,
				&blk_base->sub_range_list, head) {
			list_del(&range_node->head);
			kfree(range_node);
		}
		list_del(&blk_base->reg_base_head);
		kfree(blk_base);
	}
}
/**
 * sde_dbg_destroy - destroy sde debug facilities
 */
void sde_dbg_destroy(void)
{
	kfree(sde_dbg_base.regbuf.buf);
	memset(&sde_dbg_base.regbuf, 0, sizeof(sde_dbg_base.regbuf));
	_sde_dbg_debugfs_destroy();
	sde_dbg_base_evtlog = NULL;
	sde_evtlog_destroy(sde_dbg_base.evtlog);
	sde_dbg_base.evtlog = NULL;
	sde_dbg_reg_base_destroy();
	mutex_destroy(&sde_dbg_base.mutex);
}

int sde_dbg_reg_register_base(const char *name, void __iomem *base,
		size_t max_offset)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *reg_base;

	if (!name || !strlen(name)) {
		pr_err("no debug name provided\n");
		return -EINVAL;
	}

	reg_base = kzalloc(sizeof(*reg_base), GFP_KERNEL);
	if (!reg_base)
		return -ENOMEM;

	strlcpy(reg_base->name, name, sizeof(reg_base->name));
	reg_base->base = base;
	reg_base->max_offset = max_offset;
	reg_base->off = 0;
	reg_base->cnt = DEFAULT_BASE_REG_CNT;
	reg_base->reg_dump = NULL;

	/* Initialize list to make sure check for null list will be valid */
	INIT_LIST_HEAD(&reg_base->sub_range_list);

	pr_debug("%s base: %pK max_offset 0x%zX\n", reg_base->name,
			reg_base->base, reg_base->max_offset);

	list_add(&reg_base->reg_base_head, &dbg_base->reg_base_list);

	return 0;
}

int sde_dbg_reg_register_cb(const char *name, void (*cb)(void *), void *ptr)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *reg_base;

	if (!name || !strlen(name)) {
		pr_err("no debug name provided\n");
		return -EINVAL;
	}

	reg_base = kzalloc(sizeof(*reg_base), GFP_KERNEL);
	if (!reg_base)
		return -ENOMEM;

	strlcpy(reg_base->name, name, sizeof(reg_base->name));
	reg_base->base = NULL;
	reg_base->max_offset = 0;
	reg_base->off = 0;
	reg_base->cnt = DEFAULT_BASE_REG_CNT;
	reg_base->reg_dump = NULL;
	reg_base->cb = cb;
	reg_base->cb_ptr = ptr;

	/* Initialize list to make sure check for null list will be valid */
	INIT_LIST_HEAD(&reg_base->sub_range_list);

	pr_debug("%s cb: %pK cb_ptr: %pK\n", reg_base->name,
			reg_base->cb, reg_base->cb_ptr);

	list_add(&reg_base->reg_base_head, &dbg_base->reg_base_list);

	return 0;
}

void sde_dbg_reg_unregister_cb(const char *name, void (*cb)(void *), void *ptr)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *reg_base;

	if (!dbg_base)
		return;

	list_for_each_entry(reg_base, &dbg_base->reg_base_list, reg_base_head) {
		if (strlen(reg_base->name) &&
			!strcmp(reg_base->name, name)) {
			pr_debug("%s cb: %pK cb_ptr: %pK\n", reg_base->name,
					reg_base->cb, reg_base->cb_ptr);
			list_del(&reg_base->reg_base_head);
			kfree(reg_base);
			break;
		}
	}
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
