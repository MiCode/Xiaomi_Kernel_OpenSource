// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2009-2021, The Linux Foundation. All rights reserved.
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
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include "sde_dbg.h"
#include "sde/sde_hw_catalog.h"
#include "sde/sde_kms.h"

#define SDE_DBG_BASE_MAX		10

#define DEFAULT_PANIC		1
#define DEFAULT_BASE_REG_CNT	DEFAULT_MDSS_HW_BLOCK_SIZE
#define GROUP_BYTES		4
#define ROW_BYTES		16
#define RANGE_NAME_LEN		40
#define REG_BASE_NAME_LEN	80

#define DBGBUS_NAME_SDE		"sde"
#define DBGBUS_NAME_VBIF_RT	"vbif_rt"
#define DBGBUS_NAME_DSI		"dsi"
#define DBGBUS_NAME_RSC		"sde_rsc_wrapper"
#define DBGBUS_NAME_LUTDMA	"reg_dma"
#define DBGBUS_NAME_DP		"dp_ahb"

/* offsets from LUTDMA top address for the debug buses */
#define LUTDMA_0_DEBUG_BUS_CTRL		0x1e8
#define LUTDMA_0_DEBUG_BUS_STATUS	0x1ec
#define LUTDMA_1_DEBUG_BUS_CTRL		0x5e8
#define LUTDMA_1_DEBUG_BUS_STATUS	0x5ec

/* offsets from sde top address for the debug buses */
#define DBGBUS_SSPP0		0x188
#define DBGBUS_AXI_INTF		0x194
#define DBGBUS_SSPP1		0x298
#define DBGBUS_DSPP		0x348
#define DBGBUS_DSPP_STATUS	0x34C
#define DBGBUS_PERIPH		0x418

/* offsets from DSI CTRL base address for the DSI debug buses */
#define DSI_DEBUG_BUS_CTRL	0x0124
#define DSI_DEBUG_BUS		0x0128

#define TEST_MASK(id, tp)	((id << 4) | (tp << 1) | BIT(0))
#define TEST_EXT_MASK(id, tp)	(((tp >> 3) << 24) | (id << 4) \
		| ((tp & 0x7) << 1) | BIT(0))

/* following offsets are with respect to MDP VBIF base for DBG BUS access */
#define MMSS_VBIF_CLKON			0x4
#define MMSS_VBIF_TEST_BUS_OUT_CTRL	0x210
#define MMSS_VBIF_TEST_BUS1_CTRL0	0x214
#define MMSS_VBIF_TEST_BUS2_CTRL0	0x21c
#define MMSS_VBIF_TEST_BUS_OUT		0x230

/* Vbif error info */
#define MMSS_VBIF_PND_ERR		0x190
#define MMSS_VBIF_SRC_ERR		0x194
#define MMSS_VBIF_XIN_HALT_CTRL1	0x204
#define MMSS_VBIF_ERR_INFO		0X1a0
#define MMSS_VBIF_ERR_INFO_1		0x1a4
#define MMSS_VBIF_CLIENT_NUM		14

#define RSC_WRAPPER_DEBUG_BUS		0x010
#define RSC_WRAPPER_DEBUG_BUS_DATA	0x014

/* offsets from DP AHB base address for DP debug bus */
#define DPTX_DEBUG_BUS_CTRL		0x88
#define DPTX_DEBUG_BUS_OUTPUT		0x8c

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
#define EXT_TEST_GROUP_SEL_EN		0x7
#define DSPP_DEBUGBUS_CTRL_EN		0x7001

#define SDE_HW_REV_MAJOR(rev) ((rev) >> 28)

#define SDE_DBG_LOG_START "start"
#define SDE_DBG_LOG_END "end"

#define SDE_DBG_LOG_MARKER(name, marker, log) \
	if (log) \
		dev_info(sde_dbg_base.dev, "======== %s %s dump =========\n", marker, name)

#define SDE_DBG_LOG_ENTRY(off, x0, x4, x8, xc, log) \
	if (log) \
		dev_info(sde_dbg_base.dev, "0x%lx : %08x %08x %08x %08x\n", off, x0, x4, x8, xc)

#define SDE_DBG_LOG_DUMP_ADDR(name, addr, size, off, log) \
	if (log) \
		dev_info(sde_dbg_base.dev, "%s: start_addr:0x%pK len:0x%x offset=0x%lx\n", \
				name, addr, size, off)

#define SDE_DBG_LOG_DEBUGBUS(name, addr, block_id, test_id, val) \
	dev_err(sde_dbg_base.dev, "%s 0x%x %d %d 0x%x\n", name, addr, block_id, test_id, val)

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
 * @phys_addr: block physical address
 * @off: cached offset of region for manual register dumping
 * @cnt: cached range of region for manual register dumping
 * @max_offset: length of region
 * @buf: buffer used for manual register dumping
 * @buf_len:  buffer length used for manual register dumping
 * @reg_dump: address for the mem dump if no ranges used
 * @cb: callback for external dump function, null if not defined
 * @cb_ptr: private pointer to callback function
 * @blk_id: id indicate the HW block
 */
struct sde_dbg_reg_base {
	struct list_head reg_base_head;
	struct list_head sub_range_list;
	char name[REG_BASE_NAME_LEN];
	void __iomem *base;
	unsigned long phys_addr;
	size_t off;
	size_t cnt;
	size_t max_offset;
	char *buf;
	size_t buf_len;
	u32 *reg_dump;
	void (*cb)(void *ptr);
	void *cb_ptr;
	u64 blk_id;
};

struct sde_debug_bus_entry {
	u32 wr_addr;
	u32 rd_addr;
	u32 block_id;
	u32 block_id_max;
	u32 test_id;
	u32 test_id_max;
	void (*analyzer)(u32 wr_addr, u32 block_id, u32 test_id, u32 val);
};

struct sde_dbg_dsi_ctrl_list_entry {
	const char *name;
	void __iomem *base;
	struct list_head list;
};

struct sde_dbg_debug_bus_common {
	char *name;
	u32 entries_size;
	u32 limited_entries_size;
	u32 *dumped_content;
	u32 content_idx;
	u32 content_size;
	u64 blk_id;
};

struct sde_dbg_sde_debug_bus {
	struct sde_dbg_debug_bus_common cmn;
	struct sde_debug_bus_entry *entries;
	struct sde_debug_bus_entry *limited_entries;
	u32 top_blk_off;
	u32 (*read_tp)(void __iomem *mem_base, u32 wr_addr, u32 rd_addr, u32 block_id, u32 test_id);
	void (*clear_tp)(void __iomem *mem_base, u32 wr_addr);
	void (*disable_block)(void __iomem *mem_base, u32 wr_addr);
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
 * @reglog: reg log instance
 * @reg_base_list: list of register dumping regions
 * @reg_dump_base: base address of register dump region
 * @reg_dump_addr: register dump address for a block/range
 * @dev: device pointer
 * @mutex: mutex to serialize access to serialze dumps, debugfs access
 * @req_dump_blks: list of blocks requested for dumping
 * @panic_on_err: whether to kernel panic after triggering dump via debugfs
 * @dump_work: work struct for deferring register dump work to separate thread
 * @work_panic: panic after dump if internal user passed "panic" special region
 * @enable_reg_dump: whether to dump registers into memory, kernel log, or both
 * @enable_dbgbus_dump: whether to dump dbgbus into memory, kernel log, or both
 * @dbgbus_sde: debug bus structure for the sde
 * @dbgbus_vbif_rt: debug bus structure for the realtime vbif
 * @dbgbus_dsi: debug bus structure for the dsi
 * @dbgbus_rsc: debug bus structure for rscc
 * @dbgbus_lutdma: debug bus structure for the lutdma hw
 * @dbgbus_dp: debug bus structure for dp
 * @dump_blk_mask: mask of all the hw blk-ids that has to be dumped
 * @dump_secure: dump entries excluding few as it is in secure-session
 * @regbuf: buffer data to track the register dumping in hw recovery
 * @cur_evt_index: index used for tracking event logs dump in hw recovery
 * @cur_reglog_index: index used for tracking register logs dump in hw recovery
 * @dbgbus_dump_idx: index used for tracking dbg-bus dump in hw recovery
 * @vbif_dbgbus_dump_idx: index for tracking vbif dumps in hw recovery
 * @hw_ownership: indicates if the VM owns the HW resources
 */
struct sde_dbg_base {
	struct sde_dbg_evtlog *evtlog;
	struct sde_dbg_reglog *reglog;
	struct list_head reg_base_list;
	void *reg_dump_base;
	void *reg_dump_addr;
	struct device *dev;
	struct mutex mutex;

	struct sde_dbg_reg_base *req_dump_blks[SDE_DBG_BASE_MAX];

	u32 panic_on_err;
	struct work_struct dump_work;
	bool work_panic;
	u32 enable_reg_dump;
	u32 enable_dbgbus_dump;

	struct sde_dbg_sde_debug_bus dbgbus_sde;
	struct sde_dbg_sde_debug_bus dbgbus_vbif_rt;
	struct sde_dbg_sde_debug_bus dbgbus_dsi;
	struct sde_dbg_sde_debug_bus dbgbus_rsc;
	struct sde_dbg_sde_debug_bus dbgbus_lutdma;
	struct sde_dbg_sde_debug_bus dbgbus_dp;
	u64 dump_blk_mask;
	bool dump_secure;
	u32 debugfs_ctrl;

	struct sde_dbg_regbuf regbuf;
	u32 cur_evt_index;
	u32 cur_reglog_index;
	enum sde_dbg_dump_context dump_mode;
	bool hw_ownership;
} sde_dbg_base;

static LIST_HEAD(sde_dbg_dsi_list);
static DEFINE_MUTEX(sde_dbg_dsi_mutex);

/* sde_dbg_base_evtlog - global pointer to main sde event log for macro use */
struct sde_dbg_evtlog *sde_dbg_base_evtlog;

/* sde_dbg_base_reglog - global pointer to main sde reg log for macro use */
struct sde_dbg_reglog *sde_dbg_base_reglog;

static void _sde_debug_bus_xbar_dump(u32 wr_addr, u32 block_id, u32 test_id, u32 val)
{
	SDE_DBG_LOG_DEBUGBUS("xbar", wr_addr, block_id, test_id, val);
}

static void _sde_debug_bus_lm_dump(u32 wr_addr, u32 block_id, u32 test_id, u32 val)
{
	if (!(val & 0xFFF000))
		return;

	SDE_DBG_LOG_DEBUGBUS("lm", wr_addr, block_id, test_id, val);
}

static void _sde_debug_bus_ppb0_dump(u32 wr_addr, u32 block_id, u32 test_id, u32 val)
{
	if (!(val & BIT(15)))
		return;

	SDE_DBG_LOG_DEBUGBUS("pp0", wr_addr, block_id, test_id, val);
}

static void _sde_debug_bus_ppb1_dump(u32 wr_addr, u32 block_id, u32 test_id, u32 val)
{
	if (!(val & BIT(15)))
		return;

	SDE_DBG_LOG_DEBUGBUS("pp1", wr_addr, block_id, test_id, val);
}

static struct sde_debug_bus_entry dbg_bus_sde_limited[] = {
	{ DBGBUS_SSPP0, DBGBUS_DSPP_STATUS, 0, 9, 0, 8 },
	{ DBGBUS_SSPP0, DBGBUS_DSPP_STATUS, 20, 34, 0, 8 },
	{ DBGBUS_SSPP0, DBGBUS_DSPP_STATUS, 60, 4, 0, 8 },
	{ DBGBUS_SSPP0, DBGBUS_DSPP_STATUS, 70, 4, 0, 8 },

	{ DBGBUS_SSPP1, DBGBUS_DSPP_STATUS, 0, 9, 0, 8 },
	{ DBGBUS_SSPP1, DBGBUS_DSPP_STATUS, 20, 34, 0, 8 },
	{ DBGBUS_SSPP1, DBGBUS_DSPP_STATUS, 60, 4, 0, 8 },
	{ DBGBUS_SSPP1, DBGBUS_DSPP_STATUS, 70, 4, 0, 8 },

	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 0, 1, 0, 8 },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 9, 1, 0, 8 },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 13, 2, 0, 8 },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 19, 2, 0, 8 },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 24, 2, 0, 8 },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 31, 8, 0, 8 },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 42, 12, 0, 8 },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 54, 2, 0, 32 },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 56, 2, 0, 8 },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 63, 73, 0, 8 },

	{ DBGBUS_PERIPH, DBGBUS_DSPP_STATUS, 0, 1, 0, 8 },
	{ DBGBUS_PERIPH, DBGBUS_DSPP_STATUS, 47, 7, 0, 8 },
	{ DBGBUS_PERIPH, DBGBUS_DSPP_STATUS, 60, 14, 0, 8 },
	{ DBGBUS_PERIPH, DBGBUS_DSPP_STATUS, 80, 3, 0, 8 },
};

static struct sde_debug_bus_entry dbg_bus_sde[] = {
	{ DBGBUS_SSPP0, DBGBUS_DSPP_STATUS, 0, 74, 0, 32 },
	{ DBGBUS_SSPP1, DBGBUS_DSPP_STATUS, 0, 74, 0, 32 },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 0, 137, 0, 32 },
	{ DBGBUS_PERIPH, DBGBUS_DSPP_STATUS, 0, 78, 0, 32 },
	{ DBGBUS_AXI_INTF, DBGBUS_DSPP_STATUS, 0, 63, 0, 32 },

	/* ppb_0 */
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 31, 1, 0, 1, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 33, 1, 0, 1, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 35, 1, 0, 1, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 42, 1, 0, 1, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 47, 1, 0, 1, _sde_debug_bus_ppb0_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 49, 1, 0, 1, _sde_debug_bus_ppb0_dump },

	/* ppb_1 */
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 32, 1, 0, 1, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 34, 1, 0, 1, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 36, 1, 0, 1, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 43, 1, 0, 1, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 48, 1, 0, 1, _sde_debug_bus_ppb1_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 50, 1, 0, 1, _sde_debug_bus_ppb1_dump },

	/* crossbar */
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 0, 1, 0, 1, _sde_debug_bus_xbar_dump },

	/* blend */
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 63, 1, 7, 1, _sde_debug_bus_lm_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 70, 1, 7, 1, _sde_debug_bus_lm_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 77, 1, 7, 1, _sde_debug_bus_lm_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 110, 1, 7, 1, _sde_debug_bus_lm_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 96, 1, 7, 1, _sde_debug_bus_lm_dump },
	{ DBGBUS_DSPP, DBGBUS_DSPP_STATUS, 124, 1, 7, 1, _sde_debug_bus_lm_dump }
};

static struct sde_debug_bus_entry vbif_dbg_bus_limited[] = {
	{ MMSS_VBIF_TEST_BUS1_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 0, 2, 0, 12},
	{ MMSS_VBIF_TEST_BUS1_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 4, 6, 0, 12},
	{ MMSS_VBIF_TEST_BUS1_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 12, 2, 0, 12},

	{ MMSS_VBIF_TEST_BUS2_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 0, 2, 0, 16},
	{ MMSS_VBIF_TEST_BUS2_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 0, 2, 128, 208},
	{ MMSS_VBIF_TEST_BUS2_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 4, 6, 0, 16},
	{ MMSS_VBIF_TEST_BUS2_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 4, 6, 128, 208},
	{ MMSS_VBIF_TEST_BUS2_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 12, 2, 0, 16},
	{ MMSS_VBIF_TEST_BUS2_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 12, 2, 128, 208},
	{ MMSS_VBIF_TEST_BUS2_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 16, 2, 0, 16},
	{ MMSS_VBIF_TEST_BUS2_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 16, 2, 128, 208},
};

static struct sde_debug_bus_entry vbif_dbg_bus[] = {
	{ MMSS_VBIF_TEST_BUS1_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 0, 15, 0, 512},
	{ MMSS_VBIF_TEST_BUS2_CTRL0, MMSS_VBIF_TEST_BUS_OUT, 0, 18, 0, 512},
};

static struct sde_debug_bus_entry dsi_dbg_bus[] = {
	{DSI_DEBUG_BUS_CTRL, DSI_DEBUG_BUS, 0, 4, 0, 64},
};

static struct sde_debug_bus_entry rsc_dbg_bus[] = {
	{RSC_WRAPPER_DEBUG_BUS, RSC_WRAPPER_DEBUG_BUS_DATA, 0, 1, 0, 16},
};

static struct sde_debug_bus_entry dbg_bus_lutdma[] = {
	{ LUTDMA_0_DEBUG_BUS_CTRL, LUTDMA_0_DEBUG_BUS_STATUS, 0, 1, 0, 12 },
	{ LUTDMA_0_DEBUG_BUS_CTRL, LUTDMA_0_DEBUG_BUS_STATUS, 0, 1, 256, 1 },
	{ LUTDMA_0_DEBUG_BUS_CTRL, LUTDMA_0_DEBUG_BUS_STATUS, 0, 1, 512, 4 },
	{ LUTDMA_0_DEBUG_BUS_CTRL, LUTDMA_0_DEBUG_BUS_STATUS, 0, 1, 768, 1 },
	{ LUTDMA_0_DEBUG_BUS_CTRL, LUTDMA_0_DEBUG_BUS_STATUS, 0, 1, 8192, 2 },
	{ LUTDMA_0_DEBUG_BUS_CTRL, LUTDMA_0_DEBUG_BUS_STATUS, 0, 1, 8448, 1 },
	{ LUTDMA_0_DEBUG_BUS_CTRL, LUTDMA_0_DEBUG_BUS_STATUS, 0, 1, 8704, 1 },
	{ LUTDMA_0_DEBUG_BUS_CTRL, LUTDMA_0_DEBUG_BUS_STATUS, 0, 1, 8960, 1 },

	{ LUTDMA_1_DEBUG_BUS_CTRL, LUTDMA_1_DEBUG_BUS_STATUS, 0, 1, 0, 12 },
	{ LUTDMA_1_DEBUG_BUS_CTRL, LUTDMA_1_DEBUG_BUS_STATUS, 0, 1, 256, 1 },
	{ LUTDMA_1_DEBUG_BUS_CTRL, LUTDMA_1_DEBUG_BUS_STATUS, 0, 1, 512, 4 },
	{ LUTDMA_1_DEBUG_BUS_CTRL, LUTDMA_1_DEBUG_BUS_STATUS, 0, 1, 768, 1 },
	{ LUTDMA_1_DEBUG_BUS_CTRL, LUTDMA_1_DEBUG_BUS_STATUS, 0, 1, 8192, 2 },
	{ LUTDMA_1_DEBUG_BUS_CTRL, LUTDMA_1_DEBUG_BUS_STATUS, 0, 1, 8448, 1 },
	{ LUTDMA_1_DEBUG_BUS_CTRL, LUTDMA_1_DEBUG_BUS_STATUS, 0, 1, 8704, 1 },
	{ LUTDMA_1_DEBUG_BUS_CTRL, LUTDMA_1_DEBUG_BUS_STATUS, 0, 1, 8960, 1 },
};

static struct sde_debug_bus_entry dp_dbg_bus[] = {
	{DPTX_DEBUG_BUS_CTRL, DPTX_DEBUG_BUS_OUTPUT, 0, 1, 0, 145},
};

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
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	u32 *dump_addr = NULL;
	char *end_addr;
	int i;

	if (!len_bytes || !dump_mem)
		return;

	in_log = (reg_dump_flag & (SDE_DBG_DUMP_IN_LOG | SDE_DBG_DUMP_IN_LOG_LIMITED));
	in_mem = (reg_dump_flag & SDE_DBG_DUMP_IN_MEM);

	pr_debug("%s: reg_dump_flag=%d in_log=%d in_mem=%d\n",
		dump_name, reg_dump_flag, in_log, in_mem);

	if (!in_log && !in_mem)
		return;

	len_align = (len_bytes + REG_DUMP_ALIGN - 1) / REG_DUMP_ALIGN;
	len_padded = len_align * REG_DUMP_ALIGN;
	end_addr = addr + len_bytes;

	*dump_mem = dbg_base->reg_dump_addr;
	dbg_base->reg_dump_addr += len_padded;

	dump_addr = *dump_mem;
	SDE_DBG_LOG_DUMP_ADDR(dump_name, dump_addr, len_padded,
					(unsigned long)(addr - base_addr), in_log);

	for (i = 0; i < len_align; i++) {
		u32 x0, x4, x8, xc;

		x0 = (addr < end_addr) ? readl_relaxed(addr + 0x0) : 0;
		x4 = (addr + 0x4 < end_addr) ? readl_relaxed(addr + 0x4) : 0;
		x8 = (addr + 0x8 < end_addr) ? readl_relaxed(addr + 0x8) : 0;
		xc = (addr + 0xc < end_addr) ? readl_relaxed(addr + 0xc) : 0;

		SDE_DBG_LOG_ENTRY((unsigned long)(addr - base_addr), x0, x4, x8, xc, in_log);

		if (dump_addr) {
			dump_addr[i * 4] = x0;
			dump_addr[i * 4 + 1] = x4;
			dump_addr[i * 4 + 2] = x8;
			dump_addr[i * 4 + 3] = xc;
		}

		addr += REG_DUMP_ALIGN;
	}
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

static u32 _sde_dbg_get_reg_blk_size(struct sde_dbg_reg_base *dbg)
{
	u32 len, len_align, len_padded;
	u32 size = 0;
	struct sde_dbg_reg_range *range_node;

	if (!dbg || !dbg->base) {
		pr_err("dbg base is null!\n");
		return 0;
	}

	if (!list_empty(&dbg->sub_range_list)) {
		list_for_each_entry(range_node, &dbg->sub_range_list, head) {
			len = _sde_dbg_get_dump_range(&range_node->offset,
					dbg->max_offset);
			len_align = (len + REG_DUMP_ALIGN - 1) / REG_DUMP_ALIGN;
			len_padded = len_align * REG_DUMP_ALIGN;
			size += REG_BASE_NAME_LEN + RANGE_NAME_LEN + len_padded;
		}
	} else {
		len = dbg->max_offset;
		len_align = (len + REG_DUMP_ALIGN - 1) / REG_DUMP_ALIGN;
		len_padded = len_align * REG_DUMP_ALIGN;
		size += REG_BASE_NAME_LEN + RANGE_NAME_LEN + len_padded;
	}

	return size;
}

static u32 _sde_dbg_get_reg_dump_size(void)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;
	u32 size = 0;

	list_for_each_entry(blk_base, &dbg_base->reg_base_list, reg_base_head)
		size += _sde_dbg_get_reg_blk_size(blk_base);

	return size;
}

#if IS_ENABLED(CONFIG_QCOM_VA_MINIDUMP)
static void sde_dbg_add_dbg_buses_to_minidump_va(void)
{
	static struct sde_dbg_base *dbg = &sde_dbg_base;

	sde_mini_dump_add_va_region("sde_dbgbus", dbg->dbgbus_sde.cmn.content_size*sizeof(u32),
			dbg->dbgbus_sde.cmn.dumped_content);

	sde_mini_dump_add_va_region("vbif_dbgbus", dbg->dbgbus_vbif_rt.cmn.content_size*sizeof(u32),
			dbg->dbgbus_vbif_rt.cmn.dumped_content);

	sde_mini_dump_add_va_region("dsi_dbgbus", dbg->dbgbus_dsi.cmn.content_size*sizeof(u32),
			dbg->dbgbus_dsi.cmn.dumped_content);

	sde_mini_dump_add_va_region("lutdma_dbgbus",
			dbg->dbgbus_lutdma.cmn.content_size*sizeof(u32),
			dbg->dbgbus_lutdma.cmn.dumped_content);

	sde_mini_dump_add_va_region("rsc_dbgbus", dbg->dbgbus_rsc.cmn.content_size*sizeof(u32),
			dbg->dbgbus_rsc.cmn.dumped_content);

	sde_mini_dump_add_va_region("dp_dbgbus", dbg->dbgbus_dp.cmn.content_size*sizeof(u32),
			dbg->dbgbus_dp.cmn.dumped_content);
}

static int sde_md_notify_handler(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct device *dev = sde_dbg_base.dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *ddev = platform_get_drvdata(pdev);
	struct msm_drm_private *priv = ddev->dev_private;
	struct sde_kms *sde_kms = to_sde_kms(priv->kms);
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	u32 reg_dump_size = _sde_dbg_get_reg_dump_size();

	sde_mini_dump_add_va_region("msm_drm_priv", sizeof(*priv), priv);
	sde_mini_dump_add_va_region("sde_evtlog",
			sizeof(*sde_dbg_base_evtlog), sde_dbg_base_evtlog);
	sde_mini_dump_add_va_region("sde_reglog",
			sizeof(*sde_dbg_base_reglog), sde_dbg_base_reglog);

	sde_mini_dump_add_va_region("sde_reg_dump", reg_dump_size, dbg_base->reg_dump_base);

	sde_dbg_add_dbg_buses_to_minidump_va();
	sde_kms_add_data_to_minidump_va(sde_kms);

	return 0;
}

static struct notifier_block sde_md_notify_blk = {
	.notifier_call = sde_md_notify_handler,
	.priority = INT_MAX,
};

static int sde_register_md_panic_notifer()
{
	qcom_va_md_register("display", &sde_md_notify_blk);
	return 0;
}

void sde_mini_dump_add_va_region(const char *name, u32 size, void *virt_addr)
{
	struct va_md_entry md_entry;
	int ret;

	strlcpy(md_entry.owner, name, sizeof(md_entry.owner));
	md_entry.vaddr = (uintptr_t)virt_addr;
	md_entry.size = size;

	ret = qcom_va_md_add_region(&md_entry);
	if (ret < 0)
		SDE_ERROR("va minudmp add entry failed for %s, returned %d\n",
			name, ret);

	return;
}
#else
static int sde_register_md_panic_notifer()
{
	return 0;
}

void sde_mini_dump_add_va_region(const char *name, u32 size, void *virt_addr)
{
	return;
}
#endif

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

static int _sde_dump_blk_phys_addr_cmp(void *priv, struct list_head *a,
		struct list_head *b)
{
	struct sde_dbg_reg_base *ar, *br;

	if (!a || !b)
		return 0;

	ar = container_of(a, struct sde_dbg_reg_base, reg_base_head);
	br = container_of(b, struct sde_dbg_reg_base, reg_base_head);

	return ar->phys_addr - br->phys_addr;
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
static void _sde_dump_reg_by_ranges(struct sde_dbg_reg_base *dbg, u32 reg_dump_flag)
{
	char *addr;
	size_t len;
	struct sde_dbg_reg_range *range_node;
	bool in_log;
	struct sde_dbg_base *dbg_base = &sde_dbg_base;

	if (!dbg || !(dbg->base || dbg->cb)) {
		pr_err("dbg base is null!\n");
		return;
	}

	in_log = (reg_dump_flag & (SDE_DBG_DUMP_IN_LOG | SDE_DBG_DUMP_IN_LOG_LIMITED));
	SDE_DBG_LOG_MARKER(dbg->name, SDE_DBG_LOG_START, in_log);

	if (dbg->cb) {
		dbg->cb(dbg->cb_ptr);
	/* If there is a list to dump the registers by ranges, use the ranges */
	} else if (!list_empty(&dbg->sub_range_list)) {
		/* sort the list by start address first */
		list_sort(NULL, &dbg->sub_range_list, _sde_dump_reg_range_cmp);
		list_for_each_entry(range_node, &dbg->sub_range_list, head) {
			len = _sde_dbg_get_dump_range(&range_node->offset, dbg->max_offset);
			addr = dbg->base + range_node->offset.start;

			pr_debug("%s: range_base=0x%pK start=0x%x end=0x%x\n",
				range_node->range_name, addr, range_node->offset.start,
				range_node->offset.end);

			scnprintf(dbg_base->reg_dump_addr, REG_BASE_NAME_LEN, dbg->name);
			dbg_base->reg_dump_addr += REG_BASE_NAME_LEN;
			scnprintf(dbg_base->reg_dump_addr, RANGE_NAME_LEN,
					range_node->range_name);
			dbg_base->reg_dump_addr += RANGE_NAME_LEN;
			_sde_dump_reg(range_node->range_name, reg_dump_flag,
					dbg->base, addr, len, &range_node->reg_dump);
		}
	} else {
		/* If there is no list to dump ranges, dump all registers */
		SDE_DBG_LOG_DUMP_ADDR("base", dbg->base, dbg->max_offset, 0, in_log);
		addr = dbg->base;
		len = dbg->max_offset;
		scnprintf(dbg_base->reg_dump_addr, REG_BASE_NAME_LEN, dbg->name);
		dbg_base->reg_dump_addr += REG_BASE_NAME_LEN;
		dbg_base->reg_dump_addr += RANGE_NAME_LEN;
		_sde_dump_reg(dbg->name, reg_dump_flag, dbg->base, addr, len, &dbg->reg_dump);
	}
}

/**
 * _sde_dump_reg_mask - dump register regions based on mask
 * @dump_blk_mask: mask of all the hw blk-ids that has to be dumped
 * @dump_secure: flag to indicate dumping in secure-session
 */
static void _sde_dump_reg_mask(u64 dump_blk_mask, bool dump_secure)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;

	if (!dump_blk_mask)
		return;

	list_sort(NULL, &dbg_base->reg_base_list, _sde_dump_blk_phys_addr_cmp);

	list_for_each_entry(blk_base, &dbg_base->reg_base_list, reg_base_head) {

		if ((!(blk_base->blk_id & dump_blk_mask)) || (!strlen(blk_base->name)))
			continue;

		if (dump_secure && is_block_exclude((char **)exclude_modules, blk_base->name))
			continue;

		_sde_dump_reg_by_ranges(blk_base, dbg_base->enable_reg_dump);
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

static u32 _sde_dbg_cmn_read_test_point(void __iomem *mem_base, u32 wr_addr, u32 rd_addr, u32 val)
{
	writel_relaxed(val, mem_base + wr_addr);
	wmb(); /* make sure debug-bus test point is enabled */
	return readl_relaxed(mem_base + rd_addr);
}

static void _sde_dbg_cmn_clear_test_point(void __iomem *mem_base, u32 wr_addr)
{
	writel_relaxed(0, mem_base + wr_addr);
}

static u32 _sde_dbg_dp_read_test_point(void __iomem *mem_base, u32 wr_addr, u32 rd_addr,
			u32 block_id, u32 test_id)
{
	u32 val = (test_id << 4) | BIT(0);

	return _sde_dbg_cmn_read_test_point(mem_base, wr_addr, rd_addr, val);
}

static u32 _sde_dbg_rsc_read_test_point(void __iomem *mem_base, u32 wr_addr, u32 rd_addr,
			u32 block_id, u32 test_id)
{
	u32 val = ((test_id & 0xf) << 1) | BIT(0);

	return _sde_dbg_cmn_read_test_point(mem_base, wr_addr, rd_addr, val);
}

static u32 _sde_dbg_lutdma_read_test_point(void __iomem *mem_base, u32 wr_addr, u32 rd_addr,
			u32 block_id, u32 test_id)
{
	u32 val = (BIT(0) | (test_id << 1)) & 0xFFFF;

	return _sde_dbg_cmn_read_test_point(mem_base, wr_addr, rd_addr, val);
}

static u32 _sde_dbg_dsi_read_test_point(void __iomem *mem_base, u32 wr_addr, u32 rd_addr,
			u32 block_id, u32 test_id)
{
	u32 val = (((block_id & 0x3) << 12) | ((test_id & 0x3f) << 4) | BIT(0));

	return _sde_dbg_cmn_read_test_point(mem_base, wr_addr, rd_addr, val);
}

static void _sde_dbg_vbif_disable_block(void __iomem *mem_base, u32 wr_addr)
{
	u32 disable_addr;

	/* make sure that other bus is off */
	disable_addr = (wr_addr == MMSS_VBIF_TEST_BUS1_CTRL0) ?
			MMSS_VBIF_TEST_BUS2_CTRL0 : MMSS_VBIF_TEST_BUS1_CTRL0;
	writel_relaxed(0, mem_base + disable_addr);
	writel_relaxed(BIT(0), mem_base + MMSS_VBIF_TEST_BUS_OUT_CTRL);
	wmb(); /* update test bus */
}

static u32 _sde_dbg_vbif_read_test_point(void __iomem *mem_base, u32 wr_addr, u32 rd_addr,
			u32 block_id, u32 test_id)
{
	writel_relaxed((1 << block_id), mem_base + wr_addr);
	writel_relaxed(test_id, mem_base + wr_addr + 0x4);
	wmb(); /* make sure debug-bus test point is enabled */
	return readl_relaxed(mem_base + rd_addr);
}

static void _sde_dbg_vbif_clear_test_point(void __iomem *mem_base, u32 wr_addr)
{
	writel_relaxed(0, mem_base + wr_addr);
	writel_relaxed(0, mem_base + wr_addr + 0x4);
	wmb(); /* update test point clear */
}

static u32 _sde_dbg_sde_read_test_point(void __iomem *mem_base, u32 wr_addr, u32 rd_addr,
			u32 block_id, u32 test_id)
{
	if (block_id > EXT_TEST_GROUP_SEL_EN)
		writel_relaxed(TEST_EXT_MASK(block_id, test_id), mem_base + wr_addr);
	else
		writel_relaxed(TEST_MASK(block_id, test_id), mem_base + wr_addr);

	/* keep DSPP test point enabled */
	if (wr_addr != DBGBUS_DSPP)
		writel_relaxed(DSPP_DEBUGBUS_CTRL_EN, mem_base + DBGBUS_DSPP);
	wmb(); /* make sure test bits were written */

	return readl_relaxed(mem_base + rd_addr);
}

static void _sde_dbg_sde_clear_test_point(void __iomem *mem_base, u32 wr_addr)
{
	writel_relaxed(0x0, mem_base + wr_addr);
	if (wr_addr != DBGBUS_DSPP)
		writel_relaxed(0x0, mem_base + DBGBUS_DSPP);
}

static void _sde_dbg_dump_vbif_err_info(void __iomem *mem_base)
{
	u32 value, d0, d1;
	unsigned long reg, reg1, reg2;
	int i;

	value = readl_relaxed(mem_base + MMSS_VBIF_CLKON);
	writel_relaxed(value | BIT(1), mem_base + MMSS_VBIF_CLKON);
	wmb(); /* make sure that vbif core is on */

	/*
	 * Extract VBIF error info based on XIN halt and error status.
	 * If the XIN client is not in HALT state, or an error is detected,
	 * then retrieve the VBIF error info for it.
	 */
	reg = readl_relaxed(mem_base + MMSS_VBIF_XIN_HALT_CTRL1);
	reg1 = readl_relaxed(mem_base + MMSS_VBIF_PND_ERR);
	reg2 = readl_relaxed(mem_base + MMSS_VBIF_SRC_ERR);
	dev_err(sde_dbg_base.dev, "xin halt:0x%lx, pnd err:0x%lx, src err:0x%lx\n",
				reg, reg1, reg2);
	reg >>= 16;
	reg &= ~(reg1 | reg2);
	for (i = 0; i < MMSS_VBIF_CLIENT_NUM; i++) {
		if (!test_bit(0, &reg)) {
			writel_relaxed(i, mem_base + MMSS_VBIF_ERR_INFO);
			wmb(); /* make sure reg write goes through */

			d0 = readl_relaxed(mem_base + MMSS_VBIF_ERR_INFO);
			d1 = readl_relaxed(mem_base + MMSS_VBIF_ERR_INFO_1);
			dev_err(sde_dbg_base.dev, "Client:%d, errinfo=0x%x, errinfo1=0x%x\n",
						i, d0, d1);
		}
		reg >>= 1;
	}
}

static bool _is_dbg_bus_limited_valid(struct sde_dbg_sde_debug_bus *bus,
				u32 wr_addr, u32 block_id, u32 test_id)
{
	struct sde_debug_bus_entry *entry;
	u32 block_id_max, test_id_max;
	int i;

	if (!bus->limited_entries || !bus->cmn.limited_entries_size)
		return true;

	for (i = 0; i < bus->cmn.limited_entries_size; i++) {
		entry = bus->limited_entries + i;
		block_id_max = entry->block_id + entry->block_id_max;
		test_id_max = entry->test_id + entry->test_id_max;

		if ((wr_addr == entry->wr_addr)
		    && ((block_id >= entry->block_id) && (block_id < block_id_max))
		    && ((test_id >= entry->test_id) && (test_id < test_id_max)))
			return true;
	}

	return false;
}

static void _sde_dbg_dump_bus_entry(struct sde_dbg_sde_debug_bus *bus,
		struct sde_debug_bus_entry *entries, u32 bus_size,
		void __iomem *mem_base, u32 *dump_addr, u32 enable_mask)
{
	u32 status = 0;
	int i, j, k;
	bool in_mem, in_log, in_log_limited;
	struct sde_debug_bus_entry *entry;

	if (!bus->read_tp || !bus->clear_tp)
		return;

	in_mem = (enable_mask & SDE_DBG_DUMP_IN_MEM);
	in_log = (enable_mask & SDE_DBG_DUMP_IN_LOG);
	in_log_limited = (enable_mask & SDE_DBG_DUMP_IN_LOG_LIMITED);

	for (k = 0; k < bus_size; k++) {
		entry = entries + k;
		if (bus->disable_block)
			bus->disable_block(mem_base, entry->wr_addr);

		for (i = entry->block_id; i < (entry->block_id + entry->block_id_max); i++) {
			for (j = entry->test_id; j < (entry->test_id + entry->test_id_max); j++) {

				status = bus->read_tp(mem_base, entry->wr_addr,
							entry->rd_addr, i, j);

				if (!entry->analyzer && (in_log || (in_log_limited &&
					    _is_dbg_bus_limited_valid(bus, entry->wr_addr, i, j))))
					SDE_DBG_LOG_ENTRY(0, entry->wr_addr, i, j, status, true);

				if (dump_addr && in_mem) {
					*dump_addr++ = entry->wr_addr;
					*dump_addr++ = i;
					*dump_addr++ = j;
					*dump_addr++ = status;
				}

				if (entry->analyzer)
					entry->analyzer(entry->wr_addr, i, j, status);
			}
		}
		/* Disable debug bus once we are done */
		bus->clear_tp(mem_base, entry->wr_addr);
	}
}

static void _sde_dbg_dump_sde_dbg_bus(struct sde_dbg_sde_debug_bus *bus, u32 enable_mask)
{
	bool in_mem, in_log;
	u32 **dump_mem = NULL;
	u32 *dump_addr = NULL;
	int i, list_size = 0;
	void __iomem *mem_base = NULL;
	struct sde_dbg_reg_base *reg_base;
	struct sde_debug_bus_entry *entries;
	u32 bus_size;
	char name[20];

	reg_base = _sde_dump_get_blk_addr(bus->cmn.name);
	if (!reg_base || !reg_base->base) {
		pr_err("unable to find mem_base for %s\n", bus->cmn.name);
		return;
	}

	in_mem = (enable_mask & SDE_DBG_DUMP_IN_MEM);
	in_log = (enable_mask & (SDE_DBG_DUMP_IN_LOG | SDE_DBG_DUMP_IN_LOG_LIMITED));

	mem_base = reg_base->base;
	if (!strcmp(bus->cmn.name, DBGBUS_NAME_SDE))
		mem_base += bus->top_blk_off;

	if (!strcmp(bus->cmn.name, DBGBUS_NAME_VBIF_RT))
		_sde_dbg_dump_vbif_err_info(mem_base);

	entries = bus->entries;
	bus_size = bus->cmn.entries_size;
	dump_mem = &bus->cmn.dumped_content;

	if (!dump_mem || !entries || !bus_size)
		return;

	/* allocate memory for each test id */
	for (i = 0; i < bus_size; i++)
		list_size += (entries[i].block_id_max * entries[i].test_id_max);
	list_size *= sizeof(u32) * DUMP_CLMN_COUNT;

	snprintf(name, sizeof(name), "%s-debugbus", bus->cmn.name);
	SDE_DBG_LOG_MARKER(name, SDE_DBG_LOG_START, in_log);

	if (in_mem && (!(*dump_mem))) {
		*dump_mem = vzalloc(list_size);
		bus->cmn.content_size = list_size / sizeof(u32);
	}
	dump_addr = *dump_mem;
	SDE_DBG_LOG_DUMP_ADDR(bus->cmn.name, dump_addr, list_size, 0, in_log);

	_sde_dbg_dump_bus_entry(bus, entries, bus_size, mem_base, dump_addr, enable_mask);
}

static void _sde_dbg_dump_dsi_dbg_bus(struct sde_dbg_sde_debug_bus *bus, u32 enable_mask)
{
	struct sde_dbg_dsi_ctrl_list_entry *ctl_entry;
	struct list_head *list;
	int list_size = 0;
	bool in_mem, in_log;
	u32 i, dsi_count = 0;
	u32 **dump_mem = NULL;
	u32 *dump_addr = NULL;
	struct sde_debug_bus_entry *entries;
	u32 bus_size;
	char name[20];

	entries = bus->entries;
	bus_size = bus->cmn.entries_size;
	dump_mem = &bus->cmn.dumped_content;

	if (!dump_mem || !entries || !bus_size || list_empty(&sde_dbg_dsi_list))
		return;

	in_mem = (enable_mask & SDE_DBG_DUMP_IN_MEM);
	in_log = (enable_mask & (SDE_DBG_DUMP_IN_LOG | SDE_DBG_DUMP_IN_LOG_LIMITED));

	list_for_each(list, &sde_dbg_dsi_list)
		dsi_count++;

	for (i = 0; i < bus_size; i++)
		list_size += (entries[i].block_id_max * entries[i].test_id_max);
	list_size *= sizeof(u32) * DUMP_CLMN_COUNT * dsi_count;

	snprintf(name, sizeof(name), "%s-debugbus", bus->cmn.name);
	SDE_DBG_LOG_MARKER(name, SDE_DBG_LOG_START, in_log);

	mutex_lock(&sde_dbg_dsi_mutex);
	if (in_mem && (!(*dump_mem))) {
		*dump_mem = vzalloc(list_size);
		bus->cmn.content_size = list_size / sizeof(u32);
	}
	dump_addr = *dump_mem;

	list_for_each_entry(ctl_entry, &sde_dbg_dsi_list, list) {
		SDE_DBG_LOG_DUMP_ADDR(ctl_entry->name, dump_addr, list_size / dsi_count, 0, in_log);

		_sde_dbg_dump_bus_entry(bus, entries, bus_size, ctl_entry->base,
					dump_addr, enable_mask);
		if (dump_addr)
			dump_addr += list_size / (sizeof(u32) * dsi_count);
	}
	mutex_unlock(&sde_dbg_dsi_mutex);
}

/**
 * _sde_dump_array - dump array of register bases
 * @do_panic: whether to trigger a panic after dumping
 * @name: string indicating origin of dump
 * @dump_secure: flag to indicate dumping in secure-session
 * @dump_blk_mask: mask of all the hw blk-ids that has to be dumped
 */
static void _sde_dump_array(bool do_panic, const char *name, bool dump_secure, u64 dump_blk_mask)
{
	int rc;
	ktime_t start, end;
	u32 reg_dump_size;
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	bool skip_power;

	mutex_lock(&dbg_base->mutex);

	reg_dump_size =  _sde_dbg_get_reg_dump_size();
	if (!dbg_base->reg_dump_base)
		dbg_base->reg_dump_base = vzalloc(reg_dump_size);

	dbg_base->reg_dump_addr =  dbg_base->reg_dump_base;

	/*
	 * sde power resources are expected to be enabled in this context and might
	 * result in deadlock if its called again.
	 */
	skip_power = (dbg_base->dump_mode == SDE_DBG_DUMP_CLK_ENABLED_CTX);

	if (sde_evtlog_is_enabled(dbg_base->evtlog, SDE_EVTLOG_ALWAYS))
		sde_evtlog_dump_all(dbg_base->evtlog);

	if (!skip_power) {
		rc = pm_runtime_get_sync(dbg_base->dev);
		if (rc < 0) {
			pr_err("failed to enable power %d\n", rc);
			return;
		}
	}

	start = ktime_get();
	_sde_dump_reg_mask(dump_blk_mask, dump_secure);
	end = ktime_get();
	dev_info(dbg_base->dev,
		"ctx:%d, reg-dump logging time start_us:%llu, end_us:%llu , duration_us:%llu\n",
		dbg_base->dump_mode, ktime_to_us(start), ktime_to_us(end),
		ktime_us_delta(end, start));

	start = ktime_get();
	if (dump_blk_mask & SDE_DBG_SDE_DBGBUS)
		_sde_dbg_dump_sde_dbg_bus(&dbg_base->dbgbus_sde, dbg_base->enable_dbgbus_dump);

	if (dump_blk_mask & SDE_DBG_LUTDMA_DBGBUS)
		_sde_dbg_dump_sde_dbg_bus(&dbg_base->dbgbus_lutdma, dbg_base->enable_dbgbus_dump);

	if (dump_blk_mask & SDE_DBG_RSC_DBGBUS)
		_sde_dbg_dump_sde_dbg_bus(&dbg_base->dbgbus_rsc, dbg_base->enable_dbgbus_dump);

	if (dump_blk_mask & SDE_DBG_VBIF_RT_DBGBUS)
		_sde_dbg_dump_sde_dbg_bus(&dbg_base->dbgbus_vbif_rt, dbg_base->enable_dbgbus_dump);

	if (dump_blk_mask & SDE_DBG_DSI_DBGBUS)
		_sde_dbg_dump_dsi_dbg_bus(&dbg_base->dbgbus_dsi, dbg_base->enable_dbgbus_dump);

	if (dump_blk_mask & SDE_DBG_DP_DBGBUS)
		_sde_dbg_dump_sde_dbg_bus(&dbg_base->dbgbus_dp, dbg_base->enable_dbgbus_dump);

	end = ktime_get();
	dev_info(dbg_base->dev,
			"debug-bus logging time start_us:%llu, end_us:%llu , duration_us:%llu\n",
			ktime_to_us(start), ktime_to_us(end), ktime_us_delta(end, start));

	if (!skip_power)
		pm_runtime_put_sync(dbg_base->dev);

	if (do_panic && dbg_base->panic_on_err)
		panic(name);

	mutex_unlock(&dbg_base->mutex);
}

/**
 * _sde_dump_work - deferred dump work function
 * @work: work structure
 */
static void _sde_dump_work(struct work_struct *work)
{
	_sde_dump_array(sde_dbg_base.work_panic, "evtlog_workitem",
			sde_dbg_base.dump_secure, sde_dbg_base.dump_blk_mask);
}

void sde_dbg_dump(enum sde_dbg_dump_context dump_mode, const char *name, u64 dump_blk_mask, ...)
{
	int i = 0;
	bool do_panic = false;
	bool dump_secure = false;
	va_list args;
	char *str = NULL;

	if (!sde_evtlog_is_enabled(sde_dbg_base.evtlog, SDE_EVTLOG_ALWAYS))
		return;

	if ((dump_mode == SDE_DBG_DUMP_IRQ_CTX) && work_pending(&sde_dbg_base.dump_work))
		return;

	sde_dbg_base.dump_mode = dump_mode;

	va_start(args, dump_blk_mask);
	while ((str = va_arg(args, char*))) {
		if (i++ >= SDE_EVTLOG_MAX_DATA) {
			pr_err("could not parse all dump arguments\n");
			break;
		}

		if (!strcmp(str, "panic"))
			do_panic = true;
		else if (!strcmp(str, "secure"))
			dump_secure = true;
	}
	va_end(args);

	if (dump_mode == SDE_DBG_DUMP_IRQ_CTX) {
		/* schedule work to dump later */
		sde_dbg_base.work_panic = do_panic;
		sde_dbg_base.dump_blk_mask = dump_blk_mask;
		schedule_work(&sde_dbg_base.dump_work);
	} else {
		_sde_dump_array(do_panic, name, dump_secure, dump_blk_mask);
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
				sde_dbg_base.debugfs_ctrl & DBG_CTRL_STOP_FTRACE) {
			pr_debug("tracing off\n");
			tracing_off();
		}

		if (!strcmp(blk_name, "panic_underrun") &&
				sde_dbg_base.debugfs_ctrl & DBG_CTRL_PANIC_UNDERRUN) {
			pr_err("panic underrun\n");
			SDE_DBG_DUMP_WQ(SDE_DBG_BUILT_IN_ALL, "panic");
		}

		if (!strcmp(blk_name, "reset_hw_panic") &&
				sde_dbg_base.debugfs_ctrl & DBG_CTRL_RESET_HW_PANIC) {
			pr_debug("reset hw panic\n");
			panic("reset_hw");
		}
	}

	va_end(args);
}

#ifdef CONFIG_DEBUG_FS
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
	sde_dbg_base.evtlog->first = (u32)atomic_add_return(0, &sde_dbg_base.evtlog->curr) + 1;
	sde_dbg_base.evtlog->last =
		sde_dbg_base.evtlog->first + SDE_EVTLOG_ENTRY;
	mutex_unlock(&sde_dbg_base.mutex);
	return 0;
}

/*
 * sde_dbg_reg_base_open - debugfs open handler for reg base
 * @inode: debugfs inode
 * @file: file handle
 */
static int sde_dbg_reg_base_open(struct inode *inode, struct file *file)
{
	char base_name[64] = {0};
	struct sde_dbg_reg_base *reg_base = NULL;

	if (!inode || !file)
		return -EINVAL;

	snprintf(base_name, sizeof(base_name), "%s",
		file->f_path.dentry->d_iname);

	base_name[strlen(file->f_path.dentry->d_iname) - 4] = '\0';
	reg_base = _sde_dump_get_blk_addr(base_name);
	if (!reg_base) {
		pr_err("error: unable to locate base %s\n",
				base_name);
		return -EINVAL;
	}

	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = reg_base;
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
		pr_err("len is more than user buffer size\n");
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
	_sde_dump_array(sde_dbg_base.panic_on_err, "dump_debugfs", false,
			sde_dbg_base.dump_blk_mask);

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

	if (len < 0 || len >= sizeof(buf))
		return 0;

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
			"****** DUMP of %s block (0x%08x) ******\n",
			blk->name, blk->phys_addr);
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
	if (!sde_dbg_base.hw_ownership) {
		pr_debug("op not supported due to HW unavailablity\n");
		len = -EOPNOTSUPP;
		goto err;
	}

	if (!rbuf->dump_done && !rbuf->cur_blk) {
		if (!rbuf->buf)
			rbuf->buf = vzalloc(DUMP_BUF_SIZE);
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

static ssize_t sde_recovery_dbgbus_dump_read(struct file *file,
		char __user *buff,
		size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char log_buf[SDE_EVTLOG_BUF_MAX];
	u32 *data;
	struct sde_dbg_debug_bus_common *cmn = file->private_data;
	u32 entry_size = DUMP_CLMN_COUNT;
	u32 max_size = min_t(size_t, count, SDE_EVTLOG_BUF_MAX);

	memset(log_buf,  0, sizeof(log_buf));
	mutex_lock(&sde_dbg_base.mutex);
	if (!sde_dbg_base.hw_ownership) {
		pr_debug("op not supported due to HW unavailablity\n");
		len = -EOPNOTSUPP;
		goto dump_done;
	}

	if (!cmn->dumped_content || !cmn->entries_size)
		goto dump_done;

	if (cmn->content_idx < cmn->content_size) {
		data = &cmn->dumped_content[cmn->content_idx];
		len = scnprintf(log_buf, max_size,
				"0x%.8lX | %.8X %.8X %.8X %.8X\n",
				cmn->content_idx * sizeof(*data),
				data[0], data[1], data[2], data[3]);

		cmn->content_idx += entry_size;
		if (copy_to_user(buff, log_buf, len)) {
			len = -EFAULT;
			goto dump_done;
		}
		*ppos += len;
	}
dump_done:
	mutex_unlock(&sde_dbg_base.mutex);

	return len;
}

static int sde_recovery_dbgbus_dump_open(struct inode *inode, struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = (void *)&sde_dbg_base.dbgbus_sde.cmn;

	mutex_lock(&sde_dbg_base.mutex);
	sde_dbg_base.dbgbus_sde.cmn.content_idx = 0;
	mutex_unlock(&sde_dbg_base.mutex);

	return 0;
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
	file->private_data = (void *)&sde_dbg_base.dbgbus_vbif_rt.cmn;

	mutex_lock(&sde_dbg_base.mutex);
	sde_dbg_base.dbgbus_vbif_rt.cmn.content_idx = 0;
	mutex_unlock(&sde_dbg_base.mutex);

	return 0;
}

static const struct file_operations sde_recovery_vbif_dbgbus_fops = {
	.open = sde_recovery_vbif_dbgbus_dump_open,
	.read = sde_recovery_dbgbus_dump_read,
};

static int sde_recovery_dsi_dbgbus_dump_open(struct inode *inode,
		struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data =  (void *)&sde_dbg_base.dbgbus_dsi.cmn;

	mutex_lock(&sde_dbg_base.mutex);
	sde_dbg_base.dbgbus_dsi.cmn.content_idx = 0;
	mutex_unlock(&sde_dbg_base.mutex);

	return 0;
}

static const struct file_operations sde_recovery_dsi_dbgbus_fops = {
	.open = sde_recovery_dsi_dbgbus_dump_open,
	.read = sde_recovery_dbgbus_dump_read,
};

static int sde_recovery_rsc_dbgbus_dump_open(struct inode *inode,
		struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data =  (void *)&sde_dbg_base.dbgbus_rsc.cmn;

	mutex_lock(&sde_dbg_base.mutex);
	sde_dbg_base.dbgbus_rsc.cmn.content_idx = 0;
	mutex_unlock(&sde_dbg_base.mutex);

	return 0;
}

static const struct file_operations sde_recovery_rsc_dbgbus_fops = {
	.open = sde_recovery_rsc_dbgbus_dump_open,
	.read = sde_recovery_dbgbus_dump_read,
};

static int sde_recovery_lutdma_dbgbus_dump_open(struct inode *inode,
		struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data =  (void *)&sde_dbg_base.dbgbus_lutdma.cmn;

	mutex_lock(&sde_dbg_base.mutex);
	sde_dbg_base.dbgbus_lutdma.cmn.content_idx = 0;
	mutex_unlock(&sde_dbg_base.mutex);

	return 0;
}

static const struct file_operations sde_recovery_lutdma_dbgbus_fops = {
	.open = sde_recovery_lutdma_dbgbus_dump_open,
	.read = sde_recovery_dbgbus_dump_read,
};

static int sde_recovery_dp_dbgbus_dump_open(struct inode *inode,
		struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data =  (void *)&sde_dbg_base.dbgbus_dp.cmn;

	mutex_lock(&sde_dbg_base.mutex);
	sde_dbg_base.dbgbus_dp.cmn.content_idx = 0;
	mutex_unlock(&sde_dbg_base.mutex);

	return 0;
}

static const struct file_operations sde_recovery_dp_dbgbus_fops = {
	.open = sde_recovery_dp_dbgbus_dump_open,
	.read = sde_recovery_dbgbus_dump_read,
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
static bool sde_dbg_reg_base_is_valid_range(
	struct sde_dbg_reg_base *base,
	u32 off, u32 cnt)
{
	struct sde_dbg_reg_range *node;

	pr_debug("check offset=0x%x cnt=0x%x\n", off, cnt);

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

	if (!list_empty(&dbg->sub_range_list)) {
		rc = sde_dbg_reg_base_is_valid_range(dbg, off, cnt);
		if (!rc)
			return -EINVAL;
	}

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

#ifdef CONFIG_DYNAMIC_DEBUG
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
	if (!sde_dbg_base.hw_ownership) {
		pr_debug("op not supported due to hw unavailablity\n");
		count = -EOPNOTSUPP;
		goto end;
	}

	if (off >= dbg->max_offset) {
		count = -EFAULT;
		goto end;
	}

	if (!list_empty(&dbg->sub_range_list)) {
		rc = sde_dbg_reg_base_is_valid_range(dbg, off, cnt);
		if (!rc) {
			count = -EINVAL;
			goto end;
		}
	}

	rc = pm_runtime_get_sync(sde_dbg_base.dev);
	if (rc < 0) {
		pr_err("failed to enable power %d\n", rc);
		count = rc;
		goto end;
	}

	writel_relaxed(data, dbg->base + off);

	pm_runtime_put_sync(sde_dbg_base.dev);

	pr_debug("addr=%zx data=%x\n", off, data);

end:
	mutex_unlock(&sde_dbg_base.mutex);

	return count;
}
#endif

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
	if (!sde_dbg_base.hw_ownership) {
		pr_debug("op not supported due to hw unavailablity\n");
		len = -EOPNOTSUPP;
		goto end;
	}

	if (!dbg->buf) {
		char dump_buf[64];
		char *ptr;
		int cnt, tot;

		dbg->buf_len = sizeof(dump_buf) *
			DIV_ROUND_UP(dbg->cnt, ROW_BYTES);
		dbg->buf = kzalloc(dbg->buf_len, GFP_KERNEL);

		if (!dbg->buf) {
			len = -ENOMEM;
			goto end;
		}

		if (dbg->off % sizeof(u32)) {
			len = -EFAULT;
			goto end;
		}

		ptr = dbg->base + dbg->off;
		tot = 0;

		rc = pm_runtime_get_sync(sde_dbg_base.dev);
		if (rc < 0) {
			pr_err("failed to enable power %d\n", rc);
			len = rc;
			goto end;
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

		pm_runtime_put_sync(sde_dbg_base.dev);

		dbg->buf_len = tot;
	}

	if (*ppos >= dbg->buf_len) {
		len = 0; /* done reading */
		goto end;
	}

	len = min(count, dbg->buf_len - (size_t) *ppos);
	if (copy_to_user(user_buf, dbg->buf + *ppos, len)) {
		pr_err("failed to copy to user\n");
		len = -EFAULT;
		goto end;
	}

	*ppos += len; /* increase offset */

end:
	mutex_unlock(&sde_dbg_base.mutex);

	return len;
}

static const struct file_operations sde_off_fops = {
	.open = sde_dbg_reg_base_open,
	.release = sde_dbg_reg_base_release,
	.read = sde_dbg_reg_base_offset_read,
	.write = sde_dbg_reg_base_offset_write,
};

static const struct file_operations sde_reg_fops = {
	.open = sde_dbg_reg_base_open,
	.release = sde_dbg_reg_base_release,
	.read = sde_dbg_reg_base_reg_read,
#ifdef  CONFIG_DYNAMIC_DEBUG
	.write = sde_dbg_reg_base_reg_write,
#endif
};

int sde_dbg_debugfs_register(struct device *dev)
{
	static struct sde_dbg_base *dbg = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;
	char debug_name[80] = "";
	struct dentry *debugfs_root = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *ddev = platform_get_drvdata(pdev);
	struct msm_drm_private *priv = NULL;

	if (!ddev || !ddev->dev_private) {
		pr_err("Invalid drm device node\n");
		return -EINVAL;
	}
	priv = ddev->dev_private;

	debugfs_root = debugfs_create_dir("debug", ddev->primary->debugfs_root);
	if (IS_ERR_OR_NULL(debugfs_root)) {
		pr_err("debugfs_root create_dir fail, error %ld\n", PTR_ERR(debugfs_root));
		priv->debug_root = NULL;
		return -EINVAL;
	}

	priv->debug_root = debugfs_root;

	debugfs_create_file("dbg_ctrl", 0600, debugfs_root, NULL, &sde_dbg_ctrl_fops);
	debugfs_create_file("dump", 0600, debugfs_root, NULL, &sde_evtlog_fops);
	debugfs_create_file("recovery_reg", 0400, debugfs_root, NULL, &sde_recovery_reg_fops);

	debugfs_create_u32("enable", 0600, debugfs_root, &(sde_dbg_base.evtlog->enable));
	debugfs_create_u32("evtlog_dump", 0600, debugfs_root, &(sde_dbg_base.evtlog->dump_mode));
	debugfs_create_u32("panic", 0600, debugfs_root, &sde_dbg_base.panic_on_err);
	debugfs_create_u32("reg_dump", 0600, debugfs_root, &sde_dbg_base.enable_reg_dump);
	debugfs_create_u32("dbgbus_dump", 0600, debugfs_root, &sde_dbg_base.enable_dbgbus_dump);
	debugfs_create_u64("reg_dump_blk_mask", 0600, debugfs_root, &sde_dbg_base.dump_blk_mask);

	if (dbg->dbgbus_sde.entries)
		debugfs_create_file("recovery_dbgbus", 0400, debugfs_root, NULL,
				&sde_recovery_dbgbus_fops);

	if (dbg->dbgbus_vbif_rt.entries)
		debugfs_create_file("recovery_vbif_dbgbus", 0400, debugfs_root,
				NULL, &sde_recovery_vbif_dbgbus_fops);

	if (dbg->dbgbus_dsi.entries)
		debugfs_create_file("recovery_dsi_dbgbus", 0400, debugfs_root,
				NULL, &sde_recovery_dsi_dbgbus_fops);

	if (dbg->dbgbus_rsc.entries)
		debugfs_create_file("recovery_rsc_dbgbus", 0400, debugfs_root,
				NULL, &sde_recovery_rsc_dbgbus_fops);

	if (dbg->dbgbus_lutdma.entries)
		debugfs_create_file("recovery_lutdma_dbgbus", 0400, debugfs_root,
				NULL, &sde_recovery_lutdma_dbgbus_fops);

	if (dbg->dbgbus_dp.entries)
		debugfs_create_file("recovery_dp_dbgbus", 0400, debugfs_root,
				NULL, &sde_recovery_dp_dbgbus_fops);

	list_for_each_entry(blk_base, &dbg->reg_base_list, reg_base_head) {
		snprintf(debug_name, sizeof(debug_name), "%s_off", blk_base->name);
		debugfs_create_file(debug_name, 0600, debugfs_root, blk_base, &sde_off_fops);

		snprintf(debug_name, sizeof(debug_name), "%s_reg", blk_base->name);
		debugfs_create_file(debug_name, 0400, debugfs_root, blk_base, &sde_reg_fops);
	}

	return 0;
}

#else

int sde_dbg_debugfs_register(struct device *dev)
{
	return 0;
}

#endif

static void _sde_dbg_debugfs_destroy(void)
{
}

void sde_dbg_init_dbg_buses(u32 hwversion)
{
	static struct sde_dbg_base *dbg = &sde_dbg_base;

	memset(&dbg->dbgbus_sde, 0, sizeof(dbg->dbgbus_sde));
	memset(&dbg->dbgbus_vbif_rt, 0, sizeof(dbg->dbgbus_vbif_rt));
	memset(&dbg->dbgbus_dsi, 0, sizeof(dbg->dbgbus_dsi));
	memset(&dbg->dbgbus_rsc, 0, sizeof(dbg->dbgbus_rsc));
	memset(&dbg->dbgbus_dp, 0, sizeof(dbg->dbgbus_dp));

	dbg->dbgbus_sde.entries = dbg_bus_sde;
	dbg->dbgbus_sde.cmn.entries_size = ARRAY_SIZE(dbg_bus_sde);
	dbg->dbgbus_sde.limited_entries = dbg_bus_sde_limited;
	dbg->dbgbus_sde.cmn.limited_entries_size = ARRAY_SIZE(dbg_bus_sde_limited);
	dbg->dbgbus_sde.cmn.name = DBGBUS_NAME_SDE;
	dbg->dbgbus_sde.cmn.blk_id = SDE_DBG_SDE_DBGBUS;
	dbg->dbgbus_sde.read_tp = _sde_dbg_sde_read_test_point;
	dbg->dbgbus_sde.clear_tp = _sde_dbg_sde_clear_test_point;

	dbg->dbgbus_vbif_rt.entries = vbif_dbg_bus;
	dbg->dbgbus_vbif_rt.cmn.entries_size = ARRAY_SIZE(vbif_dbg_bus);
	dbg->dbgbus_vbif_rt.limited_entries = vbif_dbg_bus_limited;
	dbg->dbgbus_vbif_rt.cmn.limited_entries_size = ARRAY_SIZE(vbif_dbg_bus_limited);
	dbg->dbgbus_vbif_rt.cmn.name = DBGBUS_NAME_VBIF_RT;
	dbg->dbgbus_vbif_rt.cmn.blk_id = SDE_DBG_VBIF_RT_DBGBUS;
	dbg->dbgbus_vbif_rt.read_tp = _sde_dbg_vbif_read_test_point;
	dbg->dbgbus_vbif_rt.clear_tp = _sde_dbg_vbif_clear_test_point;
	dbg->dbgbus_vbif_rt.disable_block = _sde_dbg_vbif_disable_block;

	dbg->dbgbus_dsi.entries = dsi_dbg_bus;
	dbg->dbgbus_dsi.cmn.entries_size = ARRAY_SIZE(dsi_dbg_bus);
	dbg->dbgbus_dsi.cmn.name = DBGBUS_NAME_DSI;
	dbg->dbgbus_dsi.cmn.blk_id = SDE_DBG_DSI_DBGBUS;
	dbg->dbgbus_dsi.read_tp = _sde_dbg_dsi_read_test_point;
	dbg->dbgbus_dsi.clear_tp = _sde_dbg_cmn_clear_test_point;

	dbg->dbgbus_rsc.entries = rsc_dbg_bus;
	dbg->dbgbus_rsc.cmn.entries_size = ARRAY_SIZE(rsc_dbg_bus);
	dbg->dbgbus_rsc.cmn.name = DBGBUS_NAME_RSC;
	dbg->dbgbus_rsc.cmn.blk_id = SDE_DBG_RSC_DBGBUS;
	dbg->dbgbus_rsc.read_tp = _sde_dbg_rsc_read_test_point;
	dbg->dbgbus_rsc.clear_tp = _sde_dbg_cmn_clear_test_point;

	dbg->dbgbus_dp.entries = dp_dbg_bus;
	dbg->dbgbus_dp.cmn.entries_size = ARRAY_SIZE(dp_dbg_bus);
	dbg->dbgbus_dp.cmn.name = DBGBUS_NAME_DP;
	dbg->dbgbus_dp.cmn.blk_id = SDE_DBG_DP_DBGBUS;
	dbg->dbgbus_dp.read_tp = _sde_dbg_dp_read_test_point;
	dbg->dbgbus_dp.clear_tp = _sde_dbg_cmn_clear_test_point;

	dbg->dbgbus_lutdma.entries = dbg_bus_lutdma;
	dbg->dbgbus_lutdma.cmn.name = DBGBUS_NAME_LUTDMA;
	dbg->dbgbus_lutdma.cmn.blk_id = SDE_DBG_LUTDMA_DBGBUS;
	dbg->dbgbus_lutdma.cmn.entries_size = ARRAY_SIZE(dbg_bus_lutdma);
	dbg->dbgbus_lutdma.read_tp = _sde_dbg_lutdma_read_test_point;
	dbg->dbgbus_lutdma.clear_tp = _sde_dbg_cmn_clear_test_point;
}

int sde_dbg_init(struct device *dev)
{
	if (!dev) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mutex_init(&sde_dbg_base.mutex);
	INIT_LIST_HEAD(&sde_dbg_base.reg_base_list);
	sde_dbg_base.dev = dev;

	sde_dbg_base.evtlog = sde_evtlog_init();
	if (IS_ERR_OR_NULL(sde_dbg_base.evtlog))
		return PTR_ERR(sde_dbg_base.evtlog);

	sde_dbg_base_evtlog = sde_dbg_base.evtlog;

	sde_dbg_base.reglog = sde_reglog_init();
	if (IS_ERR_OR_NULL(sde_dbg_base.reglog))
		return PTR_ERR(sde_dbg_base.reglog);

	sde_dbg_base_reglog = sde_dbg_base.reglog;

	INIT_WORK(&sde_dbg_base.dump_work, _sde_dump_work);
	sde_dbg_base.work_panic = false;
	sde_dbg_base.panic_on_err = DEFAULT_PANIC;
	sde_dbg_base.enable_reg_dump = SDE_DBG_DEFAULT_DUMP_MODE;
	sde_dbg_base.enable_dbgbus_dump = SDE_DBG_DEFAULT_DUMP_MODE;
	sde_dbg_base.dump_blk_mask = SDE_DBG_BUILT_IN_ALL;
	memset(&sde_dbg_base.regbuf, 0, sizeof(sde_dbg_base.regbuf));
	sde_register_md_panic_notifer();

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
	vfree(dbg_base->reg_dump_base);
}

static void sde_dbg_dsi_ctrl_destroy(void)
{
	struct sde_dbg_dsi_ctrl_list_entry *entry, *tmp;

	mutex_lock(&sde_dbg_dsi_mutex);
	list_for_each_entry_safe(entry, tmp, &sde_dbg_dsi_list, list) {
		list_del(&entry->list);
		kfree(entry);
	}
	mutex_unlock(&sde_dbg_dsi_mutex);
}

static void sde_dbg_buses_destroy(void)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;

	vfree(dbg_base->dbgbus_sde.cmn.dumped_content);
	vfree(dbg_base->dbgbus_vbif_rt.cmn.dumped_content);
	vfree(dbg_base->dbgbus_dsi.cmn.dumped_content);
	vfree(dbg_base->dbgbus_lutdma.cmn.dumped_content);
	vfree(dbg_base->dbgbus_rsc.cmn.dumped_content);
	vfree(dbg_base->dbgbus_dp.cmn.dumped_content);
}

/**
 * sde_dbg_destroy - destroy sde debug facilities
 */
void sde_dbg_destroy(void)
{
	vfree(sde_dbg_base.regbuf.buf);
	memset(&sde_dbg_base.regbuf, 0, sizeof(sde_dbg_base.regbuf));
	_sde_dbg_debugfs_destroy();
	sde_dbg_base_evtlog = NULL;
	sde_evtlog_destroy(sde_dbg_base.evtlog);
	sde_dbg_base.evtlog = NULL;
	sde_reglog_destroy(sde_dbg_base.reglog);
	sde_dbg_base.reglog = NULL;
	sde_dbg_reg_base_destroy();
	sde_dbg_dsi_ctrl_destroy();
	sde_dbg_buses_destroy();
	mutex_destroy(&sde_dbg_base.mutex);
}

int sde_dbg_dsi_ctrl_register(void __iomem *base, const char *name)
{
	struct sde_dbg_dsi_ctrl_list_entry *entry;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->name = name;
	entry->base = base;
	mutex_lock(&sde_dbg_dsi_mutex);
	list_add_tail(&entry->list, &sde_dbg_dsi_list);
	mutex_unlock(&sde_dbg_dsi_mutex);

	pr_debug("registered DSI CTRL %s for debugbus support\n", entry->name);
	return 0;
}

int sde_dbg_reg_register_base(const char *name, void __iomem *base, size_t max_offset,
		unsigned long phys_addr, u64 blk_id)
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
	reg_base->phys_addr = phys_addr;
	reg_base->max_offset = max_offset;
	reg_base->off = 0;
	reg_base->cnt = DEFAULT_BASE_REG_CNT;
	reg_base->reg_dump = NULL;
	reg_base->blk_id = blk_id;

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

void sde_dbg_set_hw_ownership_status(bool enable)
{
	mutex_lock(&sde_dbg_base.mutex);
	sde_dbg_base.hw_ownership = enable;
	mutex_unlock(&sde_dbg_base.mutex);
}

void sde_dbg_set_sde_top_offset(u32 blk_off)
{
	sde_dbg_base.dbgbus_sde.top_blk_off = blk_off;
}
