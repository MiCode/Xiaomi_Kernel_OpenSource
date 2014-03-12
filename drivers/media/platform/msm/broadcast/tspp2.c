/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/platform_device.h>
#include <linux/msm_ion.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/iommu.h>
#include <linux/qcom_iommu.h>
#include <linux/msm_iommu_domains.h>
#include <linux/msm-bus.h>
#include <mach/msm_tspp2.h>
#include <linux/clk/msm-clk.h>

#define TSPP2_MODULUS_OP(val, mod)	((val) & ((mod) - 1))

/* General definitions. Note we're reserving one batch. */
#define TSPP2_NUM_ALL_INPUTS	(TSPP2_NUM_TSIF_INPUTS + TSPP2_NUM_MEM_INPUTS)
#define TSPP2_NUM_CONTEXTS		128
#define TSPP2_NUM_AVAIL_CONTEXTS	127
#define TSPP2_NUM_HW_FILTERS		128
#define TSPP2_NUM_BATCHES		15
#define TSPP2_FILTERS_PER_BATCH		8
#define TSPP2_NUM_AVAIL_FILTERS	(TSPP2_NUM_HW_FILTERS - TSPP2_FILTERS_PER_BATCH)
#define TSPP2_NUM_KEYTABLES		32
#define TSPP2_TSIF_DEF_TIME_LIMIT   15000 /* Number of tsif-ref-clock ticks */

#define TSPP2_NUM_EVENT_WORK_ELEMENTS	256

/*
 * Based on the hardware programming guide, HW requires we wait for up to 2ms
 * before closing the pipes used by the filter.
 * This is required to avoid unexpected pipe reset interrupts.
 */
#define TSPP2_HW_DELAY_USEC		2000

/*
 * Default source configuration:
 * Sync byte 0x47, check sync byte,
 * Do not monitor scrambling bits,
 * Discard packets with invalid AF,
 * Do not assume duplicates,
 * Do not ignore discontinuity indicator,
 * Check continuity of TS packets.
 */
#define TSPP2_DEFAULT_SRC_CONFIG	0x47801E49

/*
 * Default memory source configuration:
 * Use 16 batches,
 * Attach last batch to each memory source.
 */
#define TSPP2_DEFAULT_MEM_SRC_CONFIG	0x80000010

/* Bypass VBIF/IOMMU for debug and bring-up purposes */
static int tspp2_iommu_bypass;
module_param(tspp2_iommu_bypass, int, S_IRUGO);

/* Enable Invalid Adaptation Field control bits event */
static int tspp2_en_invalid_af_ctrl;
module_param(tspp2_en_invalid_af_ctrl, int, S_IRUGO | S_IWUSR);

/* Enable Invalid Adaptation Field length event */
static int tspp2_en_invalid_af_length;
module_param(tspp2_en_invalid_af_length, int, S_IRUGO | S_IWUSR);

/* Enable PES No Sync event */
static int tspp2_en_pes_no_sync;
module_param(tspp2_en_pes_no_sync, int, S_IRUGO | S_IWUSR);

/**
 * enum tspp2_operation_opcode - TSPP2 Operation opcode for TSPP2_OPCODE
 */
enum tspp2_operation_opcode {
	TSPP2_OPCODE_PES_ANALYSIS = 0x03,
	TSPP2_OPCODE_RAW_TRANSMIT = 0x07,
	TSPP2_OPCODE_PES_TRANSMIT = 0x00,
	TSPP2_OPCODE_PCR_EXTRACTION = 0x05,
	TSPP2_OPCODE_CIPHER = 0x01,
	TSPP2_OPCODE_INDEXING = 0x09,
	TSPP2_OPCODE_COPY_PACKET = 0x0B,
	TSPP2_OPCODE_EXIT = 0x0F
};

/* TSIF Register definitions: */
#define TSPP2_TSIF_STS_CTL		(0x0000)
#define TSPP2_TSIF_TIME_LIMIT		(0x0004)
#define TSPP2_TSIF_CLK_REF		(0x0008)
#define TSPP2_TSIF_LPBK_FLAGS		(0x000C)
#define TSPP2_TSIF_LPBK_DATA		(0x0010)
#define TSPP2_TSIF_DATA_PORT		(0x0100)

/* Bits for TSPP2_TSIF_STS_CTL register */
#define TSIF_STS_CTL_PKT_WRITE_ERR	BIT(30)
#define TSIF_STS_CTL_PKT_READ_ERR	BIT(29)
#define TSIF_STS_CTL_EN_IRQ		BIT(28)
#define TSIF_STS_CTL_PACK_AVAIL		BIT(27)
#define TSIF_STS_CTL_1ST_PACKET		BIT(26)
#define TSIF_STS_CTL_OVERFLOW		BIT(25)
#define TSIF_STS_CTL_LOST_SYNC		BIT(24)
#define TSIF_STS_CTL_TIMEOUT		BIT(23)
#define TSIF_STS_CTL_INV_SYNC		BIT(21)
#define TSIF_STS_CTL_INV_NULL		BIT(20)
#define TSIF_STS_CTL_INV_ERROR		BIT(19)
#define TSIF_STS_CTL_INV_ENABLE		BIT(18)
#define TSIF_STS_CTL_INV_DATA		BIT(17)
#define TSIF_STS_CTL_INV_CLOCK		BIT(16)
#define TSIF_STS_CTL_PARALLEL		BIT(14)
#define TSIF_STS_CTL_EN_NULL		BIT(11)
#define TSIF_STS_CTL_EN_ERROR		BIT(10)
#define TSIF_STS_CTL_LAST_BIT		BIT(9)
#define TSIF_STS_CTL_EN_TIME_LIM	BIT(8)
#define TSIF_STS_CTL_EN_TCR		BIT(7)
#define TSIF_STS_CTL_TEST_MODE		BIT(6)
#define TSIF_STS_CTL_MODE_2		BIT(5)
#define TSIF_STS_CTL_EN_DM		BIT(4)
#define TSIF_STS_CTL_STOP		BIT(3)
#define TSIF_STS_CTL_START		BIT(0)

/* Indexing Table Register definitions: id = 0..3, n = 0..25 */
#define TSPP2_INDEX_TABLE_PREFIX(id)		(0x6000 + ((id) << 2))
#define TSPP2_INDEX_TABLE_PREFIX_MASK(id)	(0x6010 + ((id) << 2))
#define TSPP2_INDEX_TABLE_PATTEREN(id, n)	(0x3C00 + ((id) << 8) + \
							((n) << 3))
#define TSPP2_INDEX_TABLE_MASK(id, n)		(0x3C04 + ((id) << 8) + \
							((n) << 3))
#define TSPP2_INDEX_TABLE_PARAMS(id)		(0x6020 + ((id) << 2))

/* Bits for TSPP2_INDEX_TABLE_PARAMS register */
#define INDEX_TABLE_PARAMS_PREFIX_SIZE_OFFS	8
#define INDEX_TABLE_PARAMS_NUM_PATTERNS_OFFS	0

/* Source with memory input register definitions: n = 0..7 */
#define TSPP2_MEM_INPUT_SRC_CONFIG(n)		(0x6040 + ((n) << 2))

/* Bits for TSPP2_MEM_INPUT_SRC_CONFIG register */
#define MEM_INPUT_SRC_CONFIG_BATCHES_OFFS	16
#define MEM_INPUT_SRC_CONFIG_INPUT_PIPE_OFFS	8
#define MEM_INPUT_SRC_CONFIG_16_BATCHES_OFFS	4
#define MEM_INPUT_SRC_CONFIG_STAMP_SUFFIX_OFFS	2
#define MEM_INPUT_SRC_CONFIG_STAMP_EN_OFFS	1
#define MEM_INPUT_SRC_CONFIG_INPUT_EN_OFFS	0

/* Source with TSIF input register definitions: n = 0..1 */
#define TSPP2_TSIF_INPUT_SRC_CONFIG(n)		(0x6060 + ((n) << 2))
#define TSIF_INPUT_SRC_CONFIG_16_BATCHES_OFFS	4

/* Bits for TSPP2_TSIF_INPUT_SRC_CONFIG register */
#define TSIF_INPUT_SRC_CONFIG_BATCHES_OFFS	16
#define TSIF_INPUT_SRC_CONFIG_INPUT_EN_OFFS	0

/* Source with any input register definitions: n = 0..9 */
#define TSPP2_SRC_DEST_PIPES(n)			(0x6070 + ((n) << 2))
#define TSPP2_SRC_CONFIG(n)			(0x6120 + ((n) << 2))
#define TSPP2_SRC_TOTAL_TSP(n)			(0x6600 + ((n) << 2))
#define TSPP2_SRC_FILTERED_OUT_TSP(n)		(0x6630 + ((n) << 2))

/* Bits for TSPP2_SRC_CONFIG register */
#define SRC_CONFIG_SYNC_BYTE_OFFS		24
#define SRC_CONFIG_CHECK_SYNC_OFFS		23
#define SRC_CONFIG_SCRAMBLING_MONITOR_OFFS	13
#define SRC_CONFIG_VERIFY_PES_START_OFFS	12
#define SRC_CONFIG_SCRAMBLING3_OFFS		10
#define SRC_CONFIG_SCRAMBLING2_OFFS		8
#define SRC_CONFIG_SCRAMBLING1_OFFS		6
#define SRC_CONFIG_SCRAMBLING0_OFFS		4
#define SRC_CONFIG_DISCARD_INVALID_AF_OFFS	3
#define SRC_CONFIG_ASSUME_DUPLICATES_OFFS	2
#define SRC_CONFIG_IGNORE_DISCONT_OFFS		1
#define SRC_CONFIG_CHECK_CONT_OFFS		0

/* Context register definitions: n = 0..127 */
#define TSPP2_PES_CONTEXT0(n)		(0x0000 + ((n) << 4))
#define TSPP2_PES_CONTEXT1(n)		(0x0004 + ((n) << 4))
#define TSPP2_PES_CONTEXT2(n)		(0x0008 + ((n) << 4))
#define TSPP2_PES_CONTEXT3(n)		(0x000C + ((n) << 4))
#define TSPP2_INDEXING_CONTEXT0(n)	(0x0800 + ((n) << 3))
#define TSPP2_INDEXING_CONTEXT1(n)	(0x0804 + ((n) << 3))
#define TSPP2_TSP_CONTEXT(n)		(0x5600 + ((n) << 2))

/* Bits for TSPP2_TSP_CONTEXT register */
#define TSP_CONTEXT_TS_HEADER_SC_OFFS	6
#define TSP_CONTEXT_PES_HEADER_SC_OFFS	8

/* Operations register definitions: f_idx = 0..127, n = 0..15 */
#define TSPP2_OPCODE(f_idx, n)	(0x1000 + \
				((f_idx) * (TSPP2_MAX_OPS_PER_FILTER << 2)) + \
				((n) << 2))

/* Filter register definitions: n = 0..127 */
#define TSPP2_FILTER_ENTRY0(n)		(0x5800 + ((n) << 3))
#define TSPP2_FILTER_ENTRY1(n)		(0x5804 + ((n) << 3))

/* Bits for TSPP2_FILTER_ENTRY0 register */
#define FILTER_ENTRY0_PID_OFFS		0
#define FILTER_ENTRY0_MASK_OFFS		13
#define FILTER_ENTRY0_EN_OFFS		26
#define FILTER_ENTRY0_CODEC_OFFS	27

/* Bits for TSPP2_FILTER_ENTRY1 register */
#define FILTER_ENTRY1_CONTEXT_OFFS	0

/* Filter context-based counter register definitions: n = 0..127 */
#define TSPP2_FILTER_TSP_SYNC_ERROR(n)		(0x4000 + ((n) << 2))
#define TSPP2_FILTER_ERRED_TSP(n)		(0x4200 + ((n) << 2))
#define TSPP2_FILTER_DISCONTINUITIES(n)		(0x4400 + ((n) << 2))
#define TSPP2_FILTER_SCRAMBLING_BITS_DISCARD(n)	(0x4600 + ((n) << 2))
#define TSPP2_FILTER_TSP_TOTAL_NUM(n)		(0x4800 + ((n) << 2))
#define TSPP2_FILTER_DISCONT_INDICATOR(n)	(0x4A00 + ((n) << 2))
#define TSPP2_FILTER_TSP_NO_PAYLOAD(n)		(0x4C00 + ((n) << 2))
#define TSPP2_FILTER_TSP_DUPLICATE(n)		(0x4E00 + ((n) << 2))
#define TSPP2_FILTER_KEY_FETCH_FAILURE(n)	(0x5000 + ((n) << 2))
#define TSPP2_FILTER_DROPPED_PCR(n)		(0x5200 + ((n) << 2))
#define TSPP2_FILTER_PES_ERRORS(n)		(0x5400 + ((n) << 2))

/* Pipe register definitions: n = 0..30 */
#define TSPP2_PIPE_THRESH_CONFIG(n)		(0x60A0 + ((n) << 2))
#define TSPP2_PIPE_LAST_ADDRESS(n)		(0x6190 + ((n) << 2))
#define TSPP2_PIPE_SECURITY			0x6150
#define TSPP2_DATA_NOT_SENT_ON_PIPE(n)		(0x6660 + ((n) << 2))

/* Global register definitions: */
#define TSPP2_PCR_GLOBAL_CONFIG			0x6160
#define TSPP2_CLK_TO_PCR_TIME_UNIT		0x6170
#define TSPP2_DESC_WAIT_TIMEOUT			0x6180
#define TSPP2_GLOBAL_IRQ_STATUS			0x6300
#define TSPP2_GLOBAL_IRQ_CLEAR			0x6304
#define TSPP2_GLOBAL_IRQ_ENABLE			0x6308
#define TSPP2_KEY_NOT_READY_IRQ_STATUS		0x6310
#define TSPP2_KEY_NOT_READY_IRQ_CLEAR		0x6314
#define TSPP2_KEY_NOT_READY_IRQ_ENABLE		0x6318
#define TSPP2_UNEXPECTED_RST_IRQ_STATUS		0x6320
#define TSPP2_UNEXPECTED_RST_IRQ_CLEAR		0x6324
#define TSPP2_UNEXPECTED_RST_IRQ_ENABLE		0x6328
#define TSPP2_WRONG_PIPE_DIR_IRQ_STATUS		0x6330
#define TSPP2_WRONG_PIPE_DIR_IRQ_CLEAR		0x6334
#define TSPP2_WRONG_PIPE_DIR_IRQ_ENABLE		0x6338
#define TSPP2_QSB_RESPONSE_ERROR_IRQ_STATUS	0x6340
#define TSPP2_QSB_RESPONSE_ERROR_IRQ_CLEAR	0x6344
#define TSPP2_QSB_RESPONSE_ERROR_IRQ_ENABLE	0x6348
#define TSPP2_SRC_TOTAL_TSP_RESET		0x6710
#define TSPP2_SRC_FILTERED_OUT_TSP_RESET	0x6714
#define TSPP2_DATA_NOT_SENT_ON_PIPE_RESET	0x6718
#define TSPP2_VERSION				0x6FFC

/* Bits for TSPP2_GLOBAL_IRQ_CLEAR register */
#define GLOBAL_IRQ_CLEAR_RESERVED_OFFS         4

/* Bits for TSPP2_VERSION register */
#define VERSION_MAJOR_OFFS			28
#define VERSION_MINOR_OFFS			16
#define VERSION_STEP_OFFS			0

/* Bits for TSPP2_GLOBAL_IRQ_XXX registers */
#define GLOBAL_IRQ_TSP_INVALID_AF_OFFS		0
#define GLOBAL_IRQ_TSP_INVALID_LEN_OFFS		1
#define GLOBAL_IRQ_PES_NO_SYNC_OFFS		2
#define GLOBAL_IRQ_ENCRYPT_LEVEL_ERR_OFFS	3
#define GLOBAL_IRQ_KEY_NOT_READY_OFFS		4
#define GLOBAL_IRQ_UNEXPECTED_RESET_OFFS	5
#define GLOBAL_IRQ_QSB_RESP_ERR_OFFS		6
#define GLOBAL_IRQ_WRONG_PIPE_DIR_OFFS		7
#define GLOBAL_IRQ_SC_GO_HIGH_OFFS		8
#define GLOBAL_IRQ_SC_GO_LOW_OFFS		9
#define GLOBAL_IRQ_READ_FAIL_OFFS		16
#define GLOBAL_IRQ_FC_STALL_OFFS		24

/* Bits for TSPP2_PCR_GLOBAL_CONFIG register */
#define PCR_GLOBAL_CONFIG_PCR_ON_DISCONT_OFFS	10
#define PCR_GLOBAL_CONFIG_STC_OFFSET_OFFS	8
#define PCR_GLOBAL_CONFIG_PCR_INTERVAL_OFFS	0
#define PCR_GLOBAL_CONFIG_PCR_ON_DISCONT	BIT(10)
#define PCR_GLOBAL_CONFIG_STC_OFFSET		(BIT(8)|BIT(9))
#define PCR_GLOBAL_CONFIG_PCR_INTERVAL		0xFF

/* n = 0..3, each register handles 32 filters */
#define TSPP2_SC_GO_HIGH_STATUS(n)		(0x6350 + ((n) << 2))
#define TSPP2_SC_GO_HIGH_CLEAR(n)		(0x6360 + ((n) << 2))
#define TSPP2_SC_GO_HIGH_ENABLE(n)		(0x6370 + ((n) << 2))
#define TSPP2_SC_GO_LOW_STATUS(n)		(0x6390 + ((n) << 2))
#define TSPP2_SC_GO_LOW_CLEAR(n)		(0x63A0 + ((n) << 2))
#define TSPP2_SC_GO_LOW_ENABLE(n)		(0x63B0 + ((n) << 2))

/* n = 0..3, each register handles 32 contexts */
#define TSPP2_TSP_CONTEXT_RESET(n)		(0x6500 + ((n) << 2))
#define TSPP2_PES_CONTEXT_RESET(n)		(0x6510 + ((n) << 2))
#define TSPP2_INDEXING_CONTEXT_RESET(n)		(0x6520 + ((n) << 2))

/* debugfs entries */

#define TSPP2_S_RW	(S_IRUGO | S_IWUSR)

struct debugfs_entry {
	const char *name;
	mode_t mode;
	int offset;
};

static const struct debugfs_entry tsif_regs[] = {
	{"sts_ctl",	TSPP2_S_RW,	TSPP2_TSIF_STS_CTL},
	{"time_limit",	TSPP2_S_RW,	TSPP2_TSIF_TIME_LIMIT},
	{"clk_ref",	TSPP2_S_RW,	TSPP2_TSIF_CLK_REF},
	{"lpbk_flags",	TSPP2_S_RW,	TSPP2_TSIF_LPBK_FLAGS},
	{"lpbk_data",	TSPP2_S_RW,	TSPP2_TSIF_LPBK_DATA},
	{"data_port",	S_IRUGO,	TSPP2_TSIF_DATA_PORT},
};

static const struct debugfs_entry tspp2_regs[] = {
	/* Memory input source configuration registers */
	{"mem_input_src_config_0", TSPP2_S_RW, TSPP2_MEM_INPUT_SRC_CONFIG(0)},
	{"mem_input_src_config_1", TSPP2_S_RW, TSPP2_MEM_INPUT_SRC_CONFIG(1)},
	{"mem_input_src_config_2", TSPP2_S_RW, TSPP2_MEM_INPUT_SRC_CONFIG(2)},
	{"mem_input_src_config_3", TSPP2_S_RW, TSPP2_MEM_INPUT_SRC_CONFIG(3)},
	{"mem_input_src_config_4", TSPP2_S_RW, TSPP2_MEM_INPUT_SRC_CONFIG(4)},
	{"mem_input_src_config_5", TSPP2_S_RW, TSPP2_MEM_INPUT_SRC_CONFIG(5)},
	{"mem_input_src_config_6", TSPP2_S_RW, TSPP2_MEM_INPUT_SRC_CONFIG(6)},
	{"mem_input_src_config_7", TSPP2_S_RW, TSPP2_MEM_INPUT_SRC_CONFIG(7)},
	/* TSIF input source configuration registers */
	{"tsif_input_src_config_0", TSPP2_S_RW, TSPP2_TSIF_INPUT_SRC_CONFIG(0)},
	{"tsif_input_src_config_1", TSPP2_S_RW, TSPP2_TSIF_INPUT_SRC_CONFIG(1)},
	/* Source destination pipes association registers */
	{"src_dest_pipes_0", TSPP2_S_RW, TSPP2_SRC_DEST_PIPES(0)},
	{"src_dest_pipes_1", TSPP2_S_RW, TSPP2_SRC_DEST_PIPES(1)},
	{"src_dest_pipes_2", TSPP2_S_RW, TSPP2_SRC_DEST_PIPES(2)},
	{"src_dest_pipes_3", TSPP2_S_RW, TSPP2_SRC_DEST_PIPES(3)},
	{"src_dest_pipes_4", TSPP2_S_RW, TSPP2_SRC_DEST_PIPES(4)},
	{"src_dest_pipes_5", TSPP2_S_RW, TSPP2_SRC_DEST_PIPES(5)},
	{"src_dest_pipes_6", TSPP2_S_RW, TSPP2_SRC_DEST_PIPES(6)},
	{"src_dest_pipes_7", TSPP2_S_RW, TSPP2_SRC_DEST_PIPES(7)},
	{"src_dest_pipes_8", TSPP2_S_RW, TSPP2_SRC_DEST_PIPES(8)},
	{"src_dest_pipes_9", TSPP2_S_RW, TSPP2_SRC_DEST_PIPES(9)},
	/* Source configuration registers */
	{"src_config_0", TSPP2_S_RW, TSPP2_SRC_CONFIG(0)},
	{"src_config_1", TSPP2_S_RW, TSPP2_SRC_CONFIG(1)},
	{"src_config_2", TSPP2_S_RW, TSPP2_SRC_CONFIG(2)},
	{"src_config_3", TSPP2_S_RW, TSPP2_SRC_CONFIG(3)},
	{"src_config_4", TSPP2_S_RW, TSPP2_SRC_CONFIG(4)},
	{"src_config_5", TSPP2_S_RW, TSPP2_SRC_CONFIG(5)},
	{"src_config_6", TSPP2_S_RW, TSPP2_SRC_CONFIG(6)},
	{"src_config_7", TSPP2_S_RW, TSPP2_SRC_CONFIG(7)},
	{"src_config_8", TSPP2_S_RW, TSPP2_SRC_CONFIG(8)},
	{"src_config_9", TSPP2_S_RW, TSPP2_SRC_CONFIG(9)},
	/* Source total TS packets counter registers */
	{"src_total_tsp_0", S_IRUGO, TSPP2_SRC_TOTAL_TSP(0)},
	{"src_total_tsp_1", S_IRUGO, TSPP2_SRC_TOTAL_TSP(1)},
	{"src_total_tsp_2", S_IRUGO, TSPP2_SRC_TOTAL_TSP(2)},
	{"src_total_tsp_3", S_IRUGO, TSPP2_SRC_TOTAL_TSP(3)},
	{"src_total_tsp_4", S_IRUGO, TSPP2_SRC_TOTAL_TSP(4)},
	{"src_total_tsp_5", S_IRUGO, TSPP2_SRC_TOTAL_TSP(5)},
	{"src_total_tsp_6", S_IRUGO, TSPP2_SRC_TOTAL_TSP(6)},
	{"src_total_tsp_7", S_IRUGO, TSPP2_SRC_TOTAL_TSP(7)},
	{"src_total_tsp_8", S_IRUGO, TSPP2_SRC_TOTAL_TSP(8)},
	{"src_total_tsp_9", S_IRUGO, TSPP2_SRC_TOTAL_TSP(9)},
	/* Source total filtered out TS packets counter registers */
	{"src_filtered_out_tsp_0", S_IRUGO, TSPP2_SRC_FILTERED_OUT_TSP(0)},
	{"src_filtered_out_tsp_1", S_IRUGO, TSPP2_SRC_FILTERED_OUT_TSP(1)},
	{"src_filtered_out_tsp_2", S_IRUGO, TSPP2_SRC_FILTERED_OUT_TSP(2)},
	{"src_filtered_out_tsp_3", S_IRUGO, TSPP2_SRC_FILTERED_OUT_TSP(3)},
	{"src_filtered_out_tsp_4", S_IRUGO, TSPP2_SRC_FILTERED_OUT_TSP(4)},
	{"src_filtered_out_tsp_5", S_IRUGO, TSPP2_SRC_FILTERED_OUT_TSP(5)},
	{"src_filtered_out_tsp_6", S_IRUGO, TSPP2_SRC_FILTERED_OUT_TSP(6)},
	{"src_filtered_out_tsp_7", S_IRUGO, TSPP2_SRC_FILTERED_OUT_TSP(7)},
	{"src_filtered_out_tsp_8", S_IRUGO, TSPP2_SRC_FILTERED_OUT_TSP(8)},
	{"src_filtered_out_tsp_9", S_IRUGO, TSPP2_SRC_FILTERED_OUT_TSP(9)},
	/* Global registers */
	{"pipe_security", TSPP2_S_RW, TSPP2_PIPE_SECURITY},
	{"pcr_global_config", TSPP2_S_RW, TSPP2_PCR_GLOBAL_CONFIG},
	{"clk_to_pcr_time_unit", TSPP2_S_RW, TSPP2_CLK_TO_PCR_TIME_UNIT},
	{"desc_wait_timeout", TSPP2_S_RW, TSPP2_DESC_WAIT_TIMEOUT},
	{"global_irq_status", S_IRUGO, TSPP2_GLOBAL_IRQ_STATUS},
	{"global_irq_clear", S_IWUSR, TSPP2_GLOBAL_IRQ_CLEAR},
	{"global_irq_en", TSPP2_S_RW, TSPP2_GLOBAL_IRQ_ENABLE},
	{"key_not_ready_irq_status", S_IRUGO, TSPP2_KEY_NOT_READY_IRQ_STATUS},
	{"key_not_ready_irq_clear", S_IWUSR, TSPP2_KEY_NOT_READY_IRQ_CLEAR},
	{"key_not_ready_irq_en", TSPP2_S_RW, TSPP2_KEY_NOT_READY_IRQ_ENABLE},
	{"unexpected_rst_irq_status", S_IRUGO, TSPP2_UNEXPECTED_RST_IRQ_STATUS},
	{"unexpected_rst_irq_clear", S_IWUSR, TSPP2_UNEXPECTED_RST_IRQ_CLEAR},
	{"unexpected_rst_irq_en", TSPP2_S_RW, TSPP2_UNEXPECTED_RST_IRQ_ENABLE},
	{"wrong_pipe_dir_irq_status", S_IRUGO, TSPP2_WRONG_PIPE_DIR_IRQ_STATUS},
	{"wrong_pipe_dir_irq_clear", S_IWUSR, TSPP2_WRONG_PIPE_DIR_IRQ_CLEAR},
	{"wrong_pipe_dir_irq_en", TSPP2_S_RW, TSPP2_WRONG_PIPE_DIR_IRQ_ENABLE},
	{"qsb_response_error_irq_status", S_IRUGO,
					TSPP2_QSB_RESPONSE_ERROR_IRQ_STATUS},
	{"qsb_response_error_irq_clear", S_IWUSR,
					TSPP2_QSB_RESPONSE_ERROR_IRQ_CLEAR},
	{"qsb_response_error_irq_en", TSPP2_S_RW,
					TSPP2_QSB_RESPONSE_ERROR_IRQ_ENABLE},
	{"src_total_tsp_reset", S_IWUSR, TSPP2_SRC_TOTAL_TSP_RESET},
	{"src_filtered_out_tsp_reset", S_IWUSR,
					TSPP2_SRC_FILTERED_OUT_TSP_RESET},
	{"data_not_sent_on_pipe_reset", S_IWUSR,
					TSPP2_DATA_NOT_SENT_ON_PIPE_RESET},
	{"version", S_IRUGO, TSPP2_VERSION},
	/* Scrambling bits monitoring interrupt registers */
	{"sc_go_high_status_0", S_IRUGO, TSPP2_SC_GO_HIGH_STATUS(0)},
	{"sc_go_high_status_1", S_IRUGO, TSPP2_SC_GO_HIGH_STATUS(1)},
	{"sc_go_high_status_2", S_IRUGO, TSPP2_SC_GO_HIGH_STATUS(2)},
	{"sc_go_high_status_3", S_IRUGO, TSPP2_SC_GO_HIGH_STATUS(3)},
	{"sc_go_high_clear_0", S_IWUSR, TSPP2_SC_GO_HIGH_CLEAR(0)},
	{"sc_go_high_clear_1", S_IWUSR, TSPP2_SC_GO_HIGH_CLEAR(1)},
	{"sc_go_high_clear_2", S_IWUSR, TSPP2_SC_GO_HIGH_CLEAR(2)},
	{"sc_go_high_clear_3", S_IWUSR, TSPP2_SC_GO_HIGH_CLEAR(3)},
	{"sc_go_high_en_0", TSPP2_S_RW, TSPP2_SC_GO_HIGH_ENABLE(0)},
	{"sc_go_high_en_1", TSPP2_S_RW, TSPP2_SC_GO_HIGH_ENABLE(1)},
	{"sc_go_high_en_2", TSPP2_S_RW, TSPP2_SC_GO_HIGH_ENABLE(2)},
	{"sc_go_high_en_3", TSPP2_S_RW, TSPP2_SC_GO_HIGH_ENABLE(3)},
	{"sc_go_low_status_0", S_IRUGO, TSPP2_SC_GO_LOW_STATUS(0)},
	{"sc_go_low_status_1", S_IRUGO, TSPP2_SC_GO_LOW_STATUS(1)},
	{"sc_go_low_status_2", S_IRUGO, TSPP2_SC_GO_LOW_STATUS(2)},
	{"sc_go_low_status_3", S_IRUGO, TSPP2_SC_GO_LOW_STATUS(3)},
	{"sc_go_low_clear_0", S_IWUSR, TSPP2_SC_GO_LOW_CLEAR(0)},
	{"sc_go_low_clear_1", S_IWUSR, TSPP2_SC_GO_LOW_CLEAR(1)},
	{"sc_go_low_clear_2", S_IWUSR, TSPP2_SC_GO_LOW_CLEAR(2)},
	{"sc_go_low_clear_3", S_IWUSR, TSPP2_SC_GO_LOW_CLEAR(3)},
	{"sc_go_low_en_0", TSPP2_S_RW, TSPP2_SC_GO_LOW_ENABLE(0)},
	{"sc_go_low_en_1", TSPP2_S_RW, TSPP2_SC_GO_LOW_ENABLE(1)},
	{"sc_go_low_en_2", TSPP2_S_RW, TSPP2_SC_GO_LOW_ENABLE(2)},
	{"sc_go_low_en_3", TSPP2_S_RW, TSPP2_SC_GO_LOW_ENABLE(3)},
};

/* Data structures */

/**
 * struct tspp2_tsif_device - TSIF device
 *
 * @base:		TSIF device memory base address.
 * @hw_index:		TSIF device HW index (0 .. (TSPP2_NUM_TSIF_INPUTS - 1)).
 * @dev:		Back pointer to the TSPP2 device.
 * @time_limit:		TSIF device time limit
 *			(maximum time allowed between each TS packet).
 * @ref_count:		TSIF device reference count.
 * @tsif_irq:		TSIF device IRQ number.
 * @mode:		TSIF mode of operation.
 * @clock_inverse:	Invert input clock signal.
 * @data_inverse:	Invert input data signal.
 * @sync_inverse:	Invert input sync signal.
 * @enable_inverse:	Invert input enable signal.
 * @debugfs_entrys:	TSIF device debugfs entry.
 * @stat_pkt_write_err:	TSIF device packet write error statistics.
 * @stat__pkt_read_err: TSIF device packet read error statistics.
 * @stat_overflow:	TSIF device overflow statistics.
 * @stat_lost_sync:	TSIF device lost sync statistics.
 * @stat_timeout:	TSIF device timeout statistics.
 */
struct tspp2_tsif_device {
	void __iomem *base;
	u32 hw_index;
	struct tspp2_device *dev;
	u32 time_limit;
	u32 ref_count;
	u32 tsif_irq;
	enum tspp2_tsif_mode mode;
	int clock_inverse;
	int data_inverse;
	int sync_inverse;
	int enable_inverse;
	struct dentry *debugfs_entry;
	u32 stat_pkt_write_err;
	u32 stat_pkt_read_err;
	u32 stat_overflow;
	u32 stat_lost_sync;
	u32 stat_timeout;
};

/**
 * struct tspp2_indexing_table - Indexing table
 *
 * @prefix_value:	4-byte common prefix value.
 * @prefix_mask:	4-byte prefix mask.
 * @entry_value:	An array of 4-byte pattern values.
 * @entry_mask:		An array of corresponding 4-byte pattern masks.
 * @num_valid_entries:	Number of valid entries in the arrays.
 */
struct tspp2_indexing_table {
	u32 prefix_value;
	u32 prefix_mask;
	u32 entry_value[TSPP2_NUM_INDEXING_PATTERNS];
	u32 entry_mask[TSPP2_NUM_INDEXING_PATTERNS];
	u16 num_valid_entries;
};

/**
 * struct tspp2_event_work - Event work information
 *
 * @device:		TSPP2 device back-pointer.
 * @callback:		Callback to invoke.
 * @cookie:		Cookie to pass to the callback.
 * @event_bitmask:	A bit mask of events to pass to the callback.
 * @work:		The work structure to queue.
 * @link:		A list element.
 */
struct tspp2_event_work {
	struct tspp2_device *device;
	void (*callback)(void *cookie, u32 event_bitmask);
	void *cookie;
	u32 event_bitmask;
	struct work_struct work;
	struct list_head link;
};

/**
 * struct tspp2_filter - Filter object
 *
 * @opened:			A flag to indicate whether the filter is open.
 * @device:			Back-pointer to the TSPP2 device the filter
 *				belongs to.
 * @batch:			The filter batch this filter belongs to.
 * @src:			Back-pointer to the source the filter is
 *				associated with.
 * @hw_index:			The filter's HW index.
 * @pid_value:			The filter's 13-bit PID value.
 * @mask:			The corresponding 13-bit bitmask.
 * @context:			The filter's context ID.
 * @indexing_table_id:		The ID of the indexing table this filter uses
 *				in case an indexing operation is set.
 * @operations:			An array of user-defined operations.
 * @num_user_operations:	The number of user-defined operations.
 * @indexing_op_set:		A flag to indicate an indexing operation
 *				has been set.
 * @raw_op_with_indexing:	A flag to indicate a Raw Transmit operation
 *				with support_indexing parameter has been set.
 * @pes_analysis_op_set:	A flag to indicate a PES Analysis operation
 *				has been set.
 * @raw_op_set:			A flag to indicate a Raw Transmit operation
 *				has been set.
 * @pes_tx_op_set:		A flag to indicate a PES Transmit operation
 *				has been set.
 * @event_callback:		A user callback to invoke when a filter event
 *				occurs.
 * @event_cookie:		A user cookie to provide to the callback.
 * @event_bitmask:		A bit mask of filter events
 *				TSPP2_FILTER_EVENT_XXX.
 * @enabled:			A flag to indicate whether the filter
 *				is enabled.
 * @link:			A list element. When the filter is associated
 *				with a source, it is added to the source's
 *				list of filters.
 */
struct tspp2_filter {
	int opened;
	struct tspp2_device *device;
	struct tspp2_filter_batch *batch;
	struct tspp2_src *src;
	u16 hw_index;
	u16 pid_value;
	u16 mask;
	u16 context;
	u8 indexing_table_id;
	struct tspp2_operation operations[TSPP2_MAX_OPS_PER_FILTER];
	u8 num_user_operations;
	int indexing_op_set;
	int raw_op_with_indexing;
	int pes_analysis_op_set;
	int raw_op_set;
	int pes_tx_op_set;
	void (*event_callback)(void *cookie, u32 event_bitmask);
	void *event_cookie;
	u32 event_bitmask;
	int enabled;
	struct list_head link;
};

/**
 * struct tspp2_pipe - Pipe object
 *
 * @opened:		A flag to indicate whether the pipe is open.
 * @device:		Back-pointer to the TSPP2 device the pipe belongs to.
 * @cfg:		Pipe configuration parameters.
 * @sps_pipe:		The BAM SPS pipe.
 * @sps_connect_cfg:	SPS pipe connection configuration.
 * @sps_event:		SPS pipe event registration parameters.
 * @desc_ion_handle:	ION handle for the SPS pipe descriptors.
 * @iova:		TSPP2 IOMMU-mapped virtual address of the
 *			data buffer provided by the user.
 * @hw_index:		The pipe's HW index (for register access).
 * @threshold:		Pipe threshold.
 * @ref_cnt:		Pipe reference count. Incremented when pipe
 *			is attached to a source, decremented when it
 *			is detached from a source.
 */
struct tspp2_pipe {
	int opened;
	struct tspp2_device *device;
	struct tspp2_pipe_config_params cfg;
	struct sps_pipe *sps_pipe;
	struct sps_connect sps_connect_cfg;
	struct sps_register_event sps_event;
	struct ion_handle *desc_ion_handle;
	ion_phys_addr_t iova;
	u32 hw_index;
	u16 threshold;
	u32 ref_cnt;
};

/**
 * struct tspp2_output_pipe - Output pipe element to add to a source's list
 *
 * @pipe:	A pointer to an output pipe object.
 * @link:	A list element. When an output pipe is attached to a source,
 *		it is added to the source's output pipe list. Note the same pipe
 *		can be attached to multiple sources, so we allocate an output
 *		pipe element to add to the list - we don't add the actual pipe.
 */
struct tspp2_output_pipe {
	struct tspp2_pipe *pipe;
	struct list_head link;
};

/**
 * struct tspp2_filter_batch - Filter batch object
 *
 * @batch_id:	Filter batch ID.
 * @hw_filters:	An array of HW filters that belong to this batch. When set, this
 *		indicates the filter is used. The actual HW index of a filter is
 *		calculated according to the index in this array along with the
 *		batch ID.
 * @src:	Back-pointer to the source the batch is associated with. This is
 *		also used to indicate this batch is "taken".
 * @link:	A list element. When the batch is associated with a source, it
 *		is added to the source's list of filter batches.
 */
struct tspp2_filter_batch {
	u8 batch_id;
	int hw_filters[TSPP2_FILTERS_PER_BATCH];
	struct tspp2_src *src;
	struct list_head link;
};

/**
 * struct tspp2_src - Source object
 *
 * @opened:			A flag to indicate whether the source is open.
 * @device:			Back-pointer to the TSPP2 device the source
 *				belongs to.
 * @hw_index:			The source's HW index. This is used when writing
 *				to HW registers relevant for this source.
 *				There are registers specific to TSIF or memory
 *				sources, and there are registers common to all
 *				sources.
 * @input:			Source input type (TSIF / memory).
 * @pkt_format:			Input packet size and format for this source.
 * @scrambling_bits_monitoring:	Scrambling bits monitoring mode.
 * @batches_list:		A list of associated filter batches.
 * @filters_list:		A list of associated filters.
 * @input_pipe:			A pointer to the source's input pipe, if exists.
 * @output_pipe_list:		A list of output pipes attached to the source.
 *				For each pipe we also save whether it is
 *				stalling for this source.
 * @num_associated_batches:	Number of associated filter batches.
 * @num_associated_pipes:	Number of associated pipes.
 * @num_associated_filters:	Number of associated filters.
 * @reserved_filter_hw_index:	A HW filter index reserved for updating an
 *				active filter's operations.
 * @event_callback:		A user callback to invoke when a source event
 *				occurs.
 * @event_cookie:		A user cookie to provide to the callback.
 * @event_bitmask:		A bit mask of source events
 *				TSPP2_SRC_EVENT_XXX.
 * @enabled:			A flag to indicate whether the source
 *				is enabled.
 */
struct tspp2_src {
	int opened;
	struct tspp2_device *device;
	u8 hw_index;
	enum tspp2_src_input input;
	enum tspp2_packet_format pkt_format;
	enum tspp2_src_scrambling_monitoring scrambling_bits_monitoring;
	struct list_head batches_list;
	struct list_head filters_list;
	struct tspp2_pipe *input_pipe;
	struct list_head output_pipe_list;
	u8 num_associated_batches;
	u8 num_associated_pipes;
	u32 num_associated_filters;
	u16 reserved_filter_hw_index;
	void (*event_callback)(void *cookie, u32 event_bitmask);
	void *event_cookie;
	u32 event_bitmask;
	int enabled;
};

/**
 * struct tspp2_global_irq_stats - Global interrupt statistics counters
 *
 * @tsp_invalid_af_control:	Invalid adaptation field control bit.
 * @tsp_invalid_length:		Invalid adaptation field length.
 * @pes_no_sync:		PES sync sequence not found.
 * @encrypt_level_err:		Cipher operation configuration error.
 */
struct tspp2_global_irq_stats {
	u32 tsp_invalid_af_control;
	u32 tsp_invalid_length;
	u32 pes_no_sync;
	u32 encrypt_level_err;
};

/**
 * struct tspp2_src_irq_stats - Memory source interrupt statistics counters
 *
 * @read_failure:	Failure to read from memory input.
 * @flow_control_stall:	Input is stalled due to flow control.
 */
struct tspp2_src_irq_stats {
	u32 read_failure;
	u32 flow_control_stall;
};

/**
 * struct tspp2_keytable_irq_stats - Key table interrupt statistics counters
 *
 * @key_not_ready:	Ciphering keys are not ready in the key table.
 */
struct tspp2_keytable_irq_stats {
	u32 key_not_ready;
};

/**
 * struct tspp2_pipe_irq_stats - Pipe interrupt statistics counters
 *
 * @unexpected_reset:		SW reset the pipe before all operations on this
 *				pipe ended.
 * @qsb_response_error:		TX operation ends with QSB error.
 * @wrong_pipe_direction:	Trying to use a pipe in the wrong direction.
 */
struct tspp2_pipe_irq_stats {
	u32 unexpected_reset;
	u32 qsb_response_error;
	u32 wrong_pipe_direction;
};

/**
 * struct tspp2_filter_context_irq_stats - Filter interrupt statistics counters
 *
 * @sc_go_high:	Scrambling bits change from clear to encrypted.
 * @sc_go_low:	Scrambling bits change from encrypted to clear.
 */
struct tspp2_filter_context_irq_stats {
	u32 sc_go_high;
	u32 sc_go_low;
};

/**
 * struct tspp2_irq_stats - Interrupt statistics counters
 *
 * @global:	Global interrupt statistics counters
 * @src:	Memory source interrupt statistics counters
 * @kt:		Key table interrupt statistics counters
 * @pipe:	Pipe interrupt statistics counters
 * @ctx:	Filter context interrupt statistics counters
 */
struct tspp2_irq_stats {
	struct tspp2_global_irq_stats global;
	struct tspp2_src_irq_stats src[TSPP2_NUM_MEM_INPUTS];
	struct tspp2_keytable_irq_stats kt[TSPP2_NUM_KEYTABLES];
	struct tspp2_pipe_irq_stats pipe[TSPP2_NUM_PIPES];
	struct tspp2_filter_context_irq_stats ctx[TSPP2_NUM_CONTEXTS];
};

/**
 * struct tspp2_iommu_info - TSPP2 IOMMU information
 *
 * @hlos_group:		TSPP2 IOMMU HLOS (Non-Secure) group.
 * @cpz_group:		TSPP2 IOMMU HLOS (Secure) group.
 * @hlos_domain:	TSPP2 IOMMU HLOS (Non-Secure) domain.
 * @cpz_domain:		TSPP2 IOMMU CPZ (Secure) domain.
 * @hlos_domain_num:	TSPP2 IOMMU HLOS (Non-Secure) domain number.
 * @cpz_domain_num:	TSPP2 IOMMU CPZ (Secure) domain number.
 * @hlos_partition:	TSPP2 IOMMU HLOS partition number.
 * @cpz_partition:	TSPP2 IOMMU CPZ partition number.
 */
struct tspp2_iommu_info {
	struct iommu_group *hlos_group;
	struct iommu_group *cpz_group;
	struct iommu_domain *hlos_domain;
	struct iommu_domain *cpz_domain;
	int hlos_domain_num;
	int cpz_domain_num;
	int hlos_partition;
	int cpz_partition;
};

/**
 * struct tspp2_device - TSPP2 device
 *
 * @dev_id:			TSPP2 device ID.
 * @opened:			A flag to indicate whether the device is open.
 * @pdev:			Platform device.
 * @dev:			Device structure, used for driver prints.
 * @base:			TSPP2 Device memory base address.
 * @tspp2_irq:			TSPP2 Device IRQ number.
 * @bam_handle:			BAM handle.
 * @bam_irq:			BAM IRQ number.
 * @bam_props:			BAM properties.
 * @iommu_info:			IOMMU information.
 * @wakeup_src:			A wakeup source to keep CPU awake when needed.
 * @spinlock:			A spinlock to protect accesses to
 *				data structures that happen from APIs and ISRs.
 * @mutex:			A mutex for mutual exclusion between API calls.
 * @tsif_devices:		An array of TSIF devices.
 * @gdsc:			GDSC power regulator.
 * @bus_client:			Client for bus bandwidth voting.
 * @tspp2_ahb_clk:		TSPP2 AHB clock.
 * @tspp2_core_clk:		TSPP2 core clock.
 * @tspp2_vbif_clk:		TSPP2 VBIF clock.
 * @vbif_ahb_clk:               VBIF AHB clock.
 * @vbif_axi_clk:               VBIF AXI clock.
 * @tspp2_klm_ahb_clk:		TSPP2 KLM AHB clock.
 * @tsif_ref_clk:		TSIF reference clock.
 * @batches:			An array of filter batch objects.
 * @contexts:			An array of context indexes. The index in this
 *				array represents the context's HW index, while
 *				the value represents whether it is used by a
 *				filter or free.
 * @indexing_tables:		An array of indexing tables.
 * @tsif_sources:		An array of source objects for TSIF input.
 * @mem_sources:		An array of source objects for memory input.
 * @filters:			An array of filter objects.
 * @pipes:			An array of pipe objects.
 * @num_secured_opened_pipes:	Number of secured opened pipes.
 * @num_non_secured_opened_pipes:	Number of non-secured opened pipes.
 * @num_enabled_sources:	Number of enabled sources.
 * @work_queue:			A work queue for invoking user callbacks.
 * @event_callback:		A user callback to invoke when a global event
 *				occurs.
 * @event_cookie:		A user cookie to provide to the callback.
 * @event_bitmask:		A bit mask of global events
 *				TSPP2_GLOBAL_EVENT_XXX.
 * @debugfs_entry:		TSPP2 device debugfs entry.
 * @irq_stats:			TSPP2 IRQ statistics.
 * @free_work_list:		A list of available work elements.
 * @work_pool:			A pool of work elements.
 */
struct tspp2_device {
	u32 dev_id;
	int opened;
	struct platform_device *pdev;
	struct device *dev;
	void __iomem *base;
	u32 tspp2_irq;
	unsigned long bam_handle;
	u32 bam_irq;
	struct sps_bam_props bam_props;
	struct tspp2_iommu_info iommu_info;
	struct wakeup_source wakeup_src;
	spinlock_t spinlock;
	struct mutex mutex;
	struct tspp2_tsif_device tsif_devices[TSPP2_NUM_TSIF_INPUTS];
	struct regulator *gdsc;
	uint32_t bus_client;
	struct clk *tspp2_ahb_clk;
	struct clk *tspp2_core_clk;
	struct clk *tspp2_vbif_clk;
	struct clk *vbif_ahb_clk;
	struct clk *vbif_axi_clk;
	struct clk *tspp2_klm_ahb_clk;
	struct clk *tsif_ref_clk;
	struct tspp2_filter_batch batches[TSPP2_NUM_BATCHES];
	int contexts[TSPP2_NUM_AVAIL_CONTEXTS];
	struct tspp2_indexing_table indexing_tables[TSPP2_NUM_INDEXING_TABLES];
	struct tspp2_src tsif_sources[TSPP2_NUM_TSIF_INPUTS];
	struct tspp2_src mem_sources[TSPP2_NUM_MEM_INPUTS];
	struct tspp2_filter filters[TSPP2_NUM_AVAIL_FILTERS];
	struct tspp2_pipe pipes[TSPP2_NUM_PIPES];
	u8 num_secured_opened_pipes;
	u8 num_non_secured_opened_pipes;
	u8 num_enabled_sources;
	struct workqueue_struct *work_queue;
	void (*event_callback)(void *cookie, u32 event_bitmask);
	void *event_cookie;
	u32 event_bitmask;
	struct dentry *debugfs_entry;
	struct tspp2_irq_stats irq_stats;
	struct list_head free_work_list;
	struct tspp2_event_work work_pool[TSPP2_NUM_EVENT_WORK_ELEMENTS];
};

/* Global TSPP2 devices database */
static struct tspp2_device *tspp2_devices[TSPP2_NUM_DEVICES];

/* debugfs support */

static int debugfs_iomem_x32_set(void *data, u64 val)
{
	int ret;
	struct tspp2_device *device = tspp2_devices[0]; /* Assuming device 0 */

	if (!device->opened)
		return -ENODEV;

	ret = pm_runtime_get_sync(device->dev);
	if (ret < 0)
		return ret;

	writel_relaxed(val, data);
	wmb();

	pm_runtime_mark_last_busy(device->dev);
	pm_runtime_put_autosuspend(device->dev);

	return 0;
}

static int debugfs_iomem_x32_get(void *data, u64 *val)
{
	int ret;
	struct tspp2_device *device = tspp2_devices[0]; /* Assuming device 0 */

	if (!device->opened)
		return -ENODEV;

	ret = pm_runtime_get_sync(device->dev);
	if (ret < 0)
		return ret;

	*val = readl_relaxed(data);

	pm_runtime_mark_last_busy(device->dev);
	pm_runtime_put_autosuspend(device->dev);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_iomem_x32, debugfs_iomem_x32_get,
			debugfs_iomem_x32_set, "0x%08llX");

static int debugfs_dev_open_set(void *data, u64 val)
{
	int ret = 0;

	/* Assuming device 0 */
	if (val == 1)
		ret = tspp2_device_open(0);
	else
		ret = tspp2_device_close(0);

	return ret;
}

static int debugfs_dev_open_get(void *data, u64 *val)
{
	struct tspp2_device *device = tspp2_devices[0]; /* Assuming device 0 */

	*val = device->opened;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_device_open, debugfs_dev_open_get,
			debugfs_dev_open_set, "0x%08llX");

/**
 * tspp2_tsif_debugfs_init() - TSIF device debugfs initialization.
 *
 * @tsif_device:	TSIF device.
 */
static void tspp2_tsif_debugfs_init(struct tspp2_tsif_device *tsif_device)
{
	int i;
	char name[10];
	struct dentry *dentry;
	void __iomem *base = tsif_device->base;

	snprintf(name, 10, "tsif%i", tsif_device->hw_index);
	tsif_device->debugfs_entry = debugfs_create_dir(name, NULL);

	if (!tsif_device->debugfs_entry)
		return;

	dentry = tsif_device->debugfs_entry;
	if (dentry) {
		for (i = 0; i < ARRAY_SIZE(tsif_regs); i++) {
			debugfs_create_file(
				tsif_regs[i].name,
				tsif_regs[i].mode,
				dentry,
				base + tsif_regs[i].offset,
				&fops_iomem_x32);
		}
	}

	dentry = debugfs_create_dir("statistics", tsif_device->debugfs_entry);
	if (dentry) {
		debugfs_create_u32(
			"stat_pkt_write_err",
			S_IRUGO | S_IWUSR | S_IWGRP,
			dentry,
			&tsif_device->stat_pkt_write_err);

		debugfs_create_u32(
			"stat_pkt_read_err",
			S_IRUGO | S_IWUSR | S_IWGRP,
			dentry,
			&tsif_device->stat_pkt_read_err);

		debugfs_create_u32(
			"stat_overflow",
			S_IRUGO | S_IWUSR | S_IWGRP,
			dentry,
			&tsif_device->stat_overflow);

		debugfs_create_u32(
			"stat_lost_sync",
			S_IRUGO | S_IWUSR | S_IWGRP,
			dentry,
			&tsif_device->stat_lost_sync);

		debugfs_create_u32(
			"stat_timeout",
			S_IRUGO | S_IWUSR | S_IWGRP,
			dentry,
			&tsif_device->stat_timeout);
	}
}

static char *op_to_string(enum tspp2_operation_type op)
{
	switch (op) {
	case TSPP2_OP_PES_ANALYSIS:
		return "TSPP2_OP_PES_ANALYSIS";
	case TSPP2_OP_RAW_TRANSMIT:
		return "TSPP2_OP_RAW_TRANSMIT";
	case TSPP2_OP_PES_TRANSMIT:
		return "TSPP2_OP_PES_TRANSMIT";
	case TSPP2_OP_PCR_EXTRACTION:
		return "TSPP2_OP_PCR_EXTRACTION";
	case TSPP2_OP_CIPHER:
		return "TSPP2_OP_CIPHER";
	case TSPP2_OP_INDEXING:
		return "TSPP2_OP_INDEXING";
	case TSPP2_OP_COPY_PACKET:
		return "TSPP2_OP_COPY_PACKET";
	default:
		return "Invalid Operation";
	}
}

static char *src_input_to_string(enum tspp2_src_input src_input)
{
	switch (src_input) {
	case TSPP2_INPUT_TSIF0:
		return "TSPP2_INPUT_TSIF0";
	case TSPP2_INPUT_TSIF1:
		return "TSPP2_INPUT_TSIF1";
	case TSPP2_INPUT_MEMORY:
		return "TSPP2_INPUT_MEMORY";
	default:
		return "Unknown source input type";
	}
}

static char *pkt_format_to_string(enum tspp2_packet_format pkt_format)
{
	switch (pkt_format) {
	case TSPP2_PACKET_FORMAT_188_RAW:
		return "TSPP2_PACKET_FORMAT_188_RAW";
	case TSPP2_PACKET_FORMAT_192_HEAD:
		return "TSPP2_PACKET_FORMAT_192_HEAD";
	case TSPP2_PACKET_FORMAT_192_TAIL:
		return "TSPP2_PACKET_FORMAT_192_TAIL";
	default:
		return "Unknown packet format";
	}
}

/**
 * debugfs service to print device information.
 */
static int tspp2_device_debugfs_print(struct seq_file *s, void *p)
{
	int count;
	int exist_flag = 0;
	struct tspp2_device *device = (struct tspp2_device *)s->private;

	seq_printf(s, "dev_id: %d\n", device->dev_id);
	seq_puts(s, "Enabled filters:");
	for (count = 0; count < TSPP2_NUM_AVAIL_FILTERS; count++)
		if (device->filters[count].enabled) {
			seq_printf(s, "\n\tfilter%3d", count);
			exist_flag = 1;
		}
	if (!exist_flag)
		seq_puts(s, " none\n");
	else
		seq_puts(s, "\n");

	exist_flag = 0;
	seq_puts(s, "Opened filters:");
	for (count = 0; count < TSPP2_NUM_AVAIL_FILTERS; count++)
		if (device->filters[count].opened) {
			seq_printf(s, "\n\tfilter%3d", count);
			exist_flag = 1;
		}
	if (!exist_flag)
		seq_puts(s, " none\n");
	else
		seq_puts(s, "\n");

	exist_flag = 0;
	seq_puts(s, "Opened pipes:\n");
	for (count = 0; count < TSPP2_NUM_PIPES; count++)
		if (device->pipes[count].opened) {
			seq_printf(s, "\tpipe%2d\n", count);
			exist_flag = 1;
		}
	if (!exist_flag)
		seq_puts(s, " none\n");
	else
		seq_puts(s, "\n");

	return 0;
}

/**
 * debugfs service to print source information.
 */
static int tspp2_src_debugfs_print(struct seq_file *s, void *p)
{
	struct tspp2_filter_batch *batch;
	struct tspp2_filter *filter;
	struct tspp2_output_pipe *output_pipe;
	struct tspp2_src *src = (struct tspp2_src *)s->private;

	if (!src) {
		seq_puts(s, "error\n");
		return 1;
	}
	seq_printf(s, "Status: %s\n", src->enabled ? "enabled" : "disabled");
	seq_printf(s, "hw_index: %d\n", src->hw_index);
	seq_printf(s, "event_bitmask: 0x%08X\n", src->event_bitmask);
	if (src->input_pipe)
		seq_printf(s, "input_pipe hw_index: %d\n",
				src->input_pipe->hw_index);
	seq_printf(s, "tspp2_src_input: %s\n", src_input_to_string(src->input));
	seq_printf(s, "pkt_format: %s\n",
			pkt_format_to_string(src->pkt_format));
	seq_printf(s, "num_associated_batches: %d\n",
			src->num_associated_batches);

	if (src->num_associated_batches) {
		seq_puts(s, "batch_ids: ");
		list_for_each_entry(batch, &src->batches_list, link)
			seq_printf(s, "%d  ", batch->batch_id);
		seq_puts(s, "\n");
	}

	seq_printf(s, "num_associated_pipes: %d\n", src->num_associated_pipes);
	if (src->num_associated_pipes) {
		seq_puts(s, "pipes_hw_idxs: ");
		list_for_each_entry(output_pipe, &src->output_pipe_list, link) {
			seq_printf(s, "%d  ", output_pipe->pipe->hw_index);
		}
		seq_puts(s, "\n");
	}

	seq_printf(s, "reserved_filter_hw_index: %d\n",
			src->reserved_filter_hw_index);

	seq_printf(s, "num_associated_filters: %d\n",
			src->num_associated_filters);
	if (src->num_associated_filters) {
		int i;
		seq_puts(s, "Open filters:\n");
		list_for_each_entry(filter, &src->filters_list, link) {
			if (!filter->opened)
				continue;
			seq_printf(s, "\thw_index: %d\n",
					filter->hw_index);
			seq_printf(s, "\tStatus: %s\n",
					filter->enabled ? "enabled"
							: "disabled");
			seq_printf(s, "\tpid_value: 0x%08X\n",
					filter->pid_value);
			seq_printf(s, "\tmask: 0x%08X\n", filter->mask);
			seq_printf(s, "\tnum_user_operations: %d\n",
					filter->num_user_operations);
			if (filter->num_user_operations) {
				seq_puts(
					s, "\tTypes of operations:\n");
				for (i = 0;
					i < filter->num_user_operations; i++) {
					seq_printf(s, "\t\t%s\n", op_to_string(
						filter->operations[i].type));
				}
			}
		}

	} else {
		seq_puts(s, "no filters\n");
	}

	return 0;
}

/**
 * debugfs service to print filter information.
 */
static int filter_debugfs_print(struct seq_file *s, void *p)
{
	int i;
	struct tspp2_filter *filter = (struct tspp2_filter *)s->private;

	seq_printf(s, "Status: %s\n", filter->opened ? "opened" : "closed");
	if (filter->batch)
		seq_printf(s, "Located in batch %d\n", filter->batch->batch_id);
	if (filter->src)
		seq_printf(s, "Associated with src %d\n",
				filter->src->hw_index);
	seq_printf(s, "hw_index: %d\n", filter->hw_index);
	seq_printf(s, "pid_value: 0x%08X\n", filter->pid_value);
	seq_printf(s, "mask: 0x%08X\n", filter->mask);
	seq_printf(s, "context: %d\n", filter->context);
	seq_printf(s, "indexing_table_id: %d\n", filter->indexing_table_id);
	seq_printf(s, "num_user_operations: %d\n", filter->num_user_operations);
	seq_puts(s, "Types of operations:\n");
	for (i = 0; i < filter->num_user_operations; i++)
		seq_printf(s, "\t%s\n", op_to_string(
				filter->operations[i].type));
	seq_printf(s, "indexing_op_set: %d\n", filter->indexing_op_set);
	seq_printf(s, "raw_op_with_indexing: %d\n",
			filter->raw_op_with_indexing);
	seq_printf(s, "pes_analysis_op_set: %d\n", filter->pes_analysis_op_set);
	seq_printf(s, "raw_op_set: %d\n", filter->raw_op_set);
	seq_printf(s, "pes_tx_op_set: %d\n", filter->pes_tx_op_set);
	seq_printf(s, "Status: %s\n", filter->enabled ? "enabled" : "disabled");

	if (filter->enabled) {
		seq_printf(s, "Filter context-based counters, context %d\n",
				filter->context);
		seq_printf(s, "filter_tsp_sync_err = 0x%08X\n",
			readl_relaxed(filter->device->base +
			TSPP2_FILTER_TSP_SYNC_ERROR(filter->context)));
		seq_printf(s, "filter_erred_tsp = 0x%08X\n",
			readl_relaxed(filter->device->base +
			TSPP2_FILTER_ERRED_TSP(filter->context)));
		seq_printf(s, "filter_discontinuities = 0x%08X\n",
			readl_relaxed(filter->device->base +
			TSPP2_FILTER_DISCONTINUITIES(filter->context)));
		seq_printf(s, "filter_sc_bits_discard = 0x%08X\n",
			readl_relaxed(filter->device->base +
			TSPP2_FILTER_SCRAMBLING_BITS_DISCARD(filter->context)));
		seq_printf(s, "filter_tsp_total_num = 0x%08X\n",
			readl_relaxed(filter->device->base +
			TSPP2_FILTER_TSP_TOTAL_NUM(filter->context)));
		seq_printf(s, "filter_discont_indicator = 0x%08X\n",
			readl_relaxed(filter->device->base +
			TSPP2_FILTER_DISCONT_INDICATOR(filter->context)));
		seq_printf(s, "filter_tsp_no_payload = 0x%08X\n",
			readl_relaxed(filter->device->base +
			TSPP2_FILTER_TSP_NO_PAYLOAD(filter->context)));
		seq_printf(s, "filter_tsp_duplicate = 0x%08X\n",
			readl_relaxed(filter->device->base +
			TSPP2_FILTER_TSP_DUPLICATE(filter->context)));
		seq_printf(s, "filter_key_fetch_fail = 0x%08X\n",
			readl_relaxed(filter->device->base +
			TSPP2_FILTER_KEY_FETCH_FAILURE(filter->context)));
		seq_printf(s, "filter_dropped_pcr = 0x%08X\n",
			readl_relaxed(filter->device->base +
			TSPP2_FILTER_DROPPED_PCR(filter->context)));
		seq_printf(s, "filter_pes_errors = 0x%08X\n",
			readl_relaxed(filter->device->base +
			TSPP2_FILTER_PES_ERRORS(filter->context)));
	}

	return 0;
}

/**
 * debugfs service to print pipe information.
 */
static int pipe_debugfs_print(struct seq_file *s, void *p)
{
	struct tspp2_pipe *pipe = (struct tspp2_pipe *)s->private;
	seq_printf(s, "hw_index: %d\n", pipe->hw_index);
	seq_printf(s, "iova: 0x%08X\n", pipe->iova);
	seq_printf(s, "threshold: %d\n", pipe->threshold);
	seq_printf(s, "Status: %s\n", pipe->opened ? "opened" : "closed");
	seq_printf(s, "ref_cnt: %d\n", pipe->ref_cnt);
	return 0;
}

static int tspp2_dev_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, tspp2_device_debugfs_print,
			inode->i_private);
}

static int tspp2_filter_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, filter_debugfs_print, inode->i_private);
}

static int tspp2_pipe_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, pipe_debugfs_print, inode->i_private);
}

static int tspp2_src_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, tspp2_src_debugfs_print, inode->i_private);
}

static const struct file_operations dbgfs_tspp2_device_fops = {
	.open = tspp2_dev_dbgfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations dbgfs_filter_fops = {
	.open = tspp2_filter_dbgfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations dbgfs_pipe_fops = {
	.open = tspp2_pipe_dbgfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations dbgfs_src_fops = {
	.open = tspp2_src_dbgfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

/**
 * tspp2_tsif_debugfs_exit() - TSIF device debugfs teardown.
 *
 * @tsif_device:	TSIF device.
 */
static void tspp2_tsif_debugfs_exit(struct tspp2_tsif_device *tsif_device)
{
	debugfs_remove_recursive(tsif_device->debugfs_entry);
	tsif_device->debugfs_entry = NULL;
}

/**
 * tspp2_debugfs_init() - TSPP2 device debugfs initialization.
 *
 * @device:	TSPP2 device.
 */
static void tspp2_debugfs_init(struct tspp2_device *device)
{
	int i, j;
	char name[80];
	struct dentry *dentry;
	struct dentry *dir;
	void __iomem *base = device->base;

	snprintf(name, 80, "tspp2_%i", device->dev_id);
	device->debugfs_entry = debugfs_create_dir(name, NULL);

	if (!device->debugfs_entry)
		return;

	/* Support device open/close */
	debugfs_create_file("open", TSPP2_S_RW, device->debugfs_entry,
				NULL, &fops_device_open);

	dentry = debugfs_create_dir("regs", device->debugfs_entry);
	if (dentry) {
		for (i = 0; i < ARRAY_SIZE(tspp2_regs); i++) {
			debugfs_create_file(
				tspp2_regs[i].name,
				tspp2_regs[i].mode,
				dentry,
				base + tspp2_regs[i].offset,
				&fops_iomem_x32);
		}
	}

	dentry = debugfs_create_dir("statistics", device->debugfs_entry);
	if (dentry) {
		debugfs_create_u32(
			"stat_tsp_invalid_af_control",
			S_IRUGO | S_IWUSR | S_IWGRP,
			dentry,
			&device->irq_stats.global.tsp_invalid_af_control);

		debugfs_create_u32(
			"stat_tsp_invalid_length",
			S_IRUGO | S_IWUSR | S_IWGRP,
			dentry,
			&device->irq_stats.global.tsp_invalid_length);

		debugfs_create_u32(
			"stat_pes_no_sync",
			S_IRUGO | S_IWUSR | S_IWGRP,
			dentry,
			&device->irq_stats.global.pes_no_sync);

		debugfs_create_u32(
			"stat_encrypt_level_err",
			S_IRUGO | S_IWUSR | S_IWGRP,
			dentry,
			&device->irq_stats.global.encrypt_level_err);
	}

	dir = debugfs_create_dir("counters", device->debugfs_entry);
	for (i = 0; i < TSPP2_NUM_CONTEXTS; i++) {
		snprintf(name, 80, "context%03i", i);
		dentry = debugfs_create_dir(name, dir);
		if (dentry) {
			debugfs_create_file("filter_tsp_sync_err",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_TSP_SYNC_ERROR(i),
				&fops_iomem_x32);

			debugfs_create_file("filter_erred_tsp",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_ERRED_TSP(i),
				&fops_iomem_x32);

			debugfs_create_file("filter_discontinuities",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_DISCONTINUITIES(i),
				&fops_iomem_x32);

			debugfs_create_file("filter_sc_bits_discard",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_SCRAMBLING_BITS_DISCARD(i),
				&fops_iomem_x32);

			debugfs_create_file("filter_tsp_total_num",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_TSP_TOTAL_NUM(i),
				&fops_iomem_x32);

			debugfs_create_file("filter_discont_indicator",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_DISCONT_INDICATOR(i),
				&fops_iomem_x32);

			debugfs_create_file("filter_tsp_no_payload",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_TSP_NO_PAYLOAD(i),
				&fops_iomem_x32);

			debugfs_create_file("filter_tsp_duplicate",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_TSP_DUPLICATE(i),
				&fops_iomem_x32);

			debugfs_create_file("filter_key_fetch_fail",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_KEY_FETCH_FAILURE(i),
				&fops_iomem_x32);

			debugfs_create_file("filter_dropped_pcr",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_DROPPED_PCR(i),
				&fops_iomem_x32);

			debugfs_create_file("filter_pes_errors",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_PES_ERRORS(i),
				&fops_iomem_x32);

			debugfs_create_u32(
				"stat_sc_go_high",
				S_IRUGO | S_IWUSR | S_IWGRP,
				dentry,
				&device->irq_stats.ctx[i].sc_go_high);

			debugfs_create_u32(
				"stat_sc_go_low",
				S_IRUGO | S_IWUSR | S_IWGRP,
				dentry,
				&device->irq_stats.ctx[i].sc_go_low);
		}
	}

	dir = debugfs_create_dir("filters", device->debugfs_entry);
	for (i = 0; i < TSPP2_NUM_HW_FILTERS; i++) {
		snprintf(name, 80, "filter%03i", i);
		dentry = debugfs_create_dir(name, dir);
		if (dentry) {
			debugfs_create_file("filter_entry0",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_ENTRY0(i),
				&fops_iomem_x32);

			debugfs_create_file("filter_entry1",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_FILTER_ENTRY1(i),
				&fops_iomem_x32);

			for (j = 0; j < TSPP2_MAX_OPS_PER_FILTER; j++) {
				snprintf(name, 80, "opcode%02i", j);
				debugfs_create_file(name,
					TSPP2_S_RW,
					dentry,
					base + TSPP2_OPCODE(i, j),
					&fops_iomem_x32);
			}
		}
	}

	dir = debugfs_create_dir("mem_sources", device->debugfs_entry);
	for (i = 0; i < TSPP2_NUM_MEM_INPUTS; i++) {
		snprintf(name, 80, "mem_src%i", i);
		dentry = debugfs_create_dir(name, dir);
		if (dentry) {
			debugfs_create_u32(
				"stat_read_failure",
				S_IRUGO | S_IWUSR | S_IWGRP,
				dentry,
				&device->irq_stats.src[i].read_failure);

			debugfs_create_u32(
				"stat_flow_control_stall",
				S_IRUGO | S_IWUSR | S_IWGRP,
				dentry,
				&device->irq_stats.src[i].flow_control_stall);
		}
	}

	dir = debugfs_create_dir("key_tables", device->debugfs_entry);
	for (i = 0; i < TSPP2_NUM_KEYTABLES; i++) {
		snprintf(name, 80, "key_table%02i", i);
		dentry = debugfs_create_dir(name, dir);
		if (dentry) {
			debugfs_create_u32(
				"stat_key_not_ready",
				S_IRUGO | S_IWUSR | S_IWGRP,
				dentry,
				&device->irq_stats.kt[i].key_not_ready);
		}
	}

	dir = debugfs_create_dir("pipes", device->debugfs_entry);
	for (i = 0; i < TSPP2_NUM_PIPES; i++) {
		snprintf(name, 80, "pipe%02i", i);
		dentry = debugfs_create_dir(name, dir);
		if (dentry) {
			debugfs_create_file("threshold",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_PIPE_THRESH_CONFIG(i),
				&fops_iomem_x32);

			debugfs_create_file("last_address",
				S_IRUGO,
				dentry,
				base + TSPP2_PIPE_LAST_ADDRESS(i),
				&fops_iomem_x32);

			debugfs_create_file("data_not_sent",
				S_IRUGO,
				dentry,
				base + TSPP2_DATA_NOT_SENT_ON_PIPE(i),
				&fops_iomem_x32);

			debugfs_create_u32(
				"stat_unexpected_reset",
				S_IRUGO | S_IWUSR | S_IWGRP,
				dentry,
				&device->irq_stats.pipe[i].unexpected_reset);

			debugfs_create_u32(
				"stat_qsb_response_error",
				S_IRUGO | S_IWUSR | S_IWGRP,
				dentry,
				&device->irq_stats.pipe[i].qsb_response_error);

			debugfs_create_u32(
				"stat_wrong_pipe_direction",
				S_IRUGO | S_IWUSR | S_IWGRP,
				dentry,
				&device->irq_stats.pipe[i].
							wrong_pipe_direction);
		}
	}

	dir = debugfs_create_dir("indexing_tables", device->debugfs_entry);
	for (i = 0; i < TSPP2_NUM_INDEXING_TABLES; i++) {
		snprintf(name, 80, "indexing_table%i", i);
		dentry = debugfs_create_dir(name, dir);
		if (dentry) {
			debugfs_create_file("prefix",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_INDEX_TABLE_PREFIX(i),
				&fops_iomem_x32);

			debugfs_create_file("mask",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_INDEX_TABLE_PREFIX_MASK(i),
				&fops_iomem_x32);

			debugfs_create_file("parameters",
				TSPP2_S_RW,
				dentry,
				base + TSPP2_INDEX_TABLE_PARAMS(i),
				&fops_iomem_x32);

			for (j = 0; j < TSPP2_NUM_INDEXING_PATTERNS; j++) {
				snprintf(name, 80, "pattern_%02i", j);
				debugfs_create_file(name,
					TSPP2_S_RW,
					dentry,
					base + TSPP2_INDEX_TABLE_PATTEREN(i, j),
					&fops_iomem_x32);

				snprintf(name, 80, "mask_%02i", j);
				debugfs_create_file(name,
					TSPP2_S_RW,
					dentry,
					base + TSPP2_INDEX_TABLE_MASK(i, j),
					&fops_iomem_x32);
			}
		}
	}
	dir = debugfs_create_dir("software", device->debugfs_entry);
	debugfs_create_file("device", S_IRUGO, dir, device,
					&dbgfs_tspp2_device_fops);

	dentry = debugfs_create_dir("filters", dir);
	if (dentry) {
		for (i = 0; i < TSPP2_NUM_AVAIL_FILTERS; i++) {
			snprintf(name, 20, "filter%03i", i);
			debugfs_create_file(name, S_IRUGO, dentry,
				&(device->filters[i]), &dbgfs_filter_fops);
		}
	}

	dentry = debugfs_create_dir("pipes", dir);
	if (dentry) {
		for (i = 0; i < TSPP2_NUM_PIPES; i++) {
			snprintf(name, 20, "pipe%02i", i);
			debugfs_create_file(name, S_IRUGO, dentry,
					&(device->pipes[i]), &dbgfs_pipe_fops);
		}
	}

	dentry = debugfs_create_dir("sources", dir);
	if (dentry) {
		for (i = 0; i < TSPP2_NUM_TSIF_INPUTS; i++) {
			snprintf(name, 20, "tsif%d", i);
			debugfs_create_file(name, S_IRUGO, dentry,
				&(device->tsif_sources[i]), &dbgfs_src_fops);
		}
		for (i = 0; i < TSPP2_NUM_MEM_INPUTS; i++) {
			snprintf(name, 20, "mem%d", i);
			debugfs_create_file(name, S_IRUGO, dentry,
				&(device->mem_sources[i]), &dbgfs_src_fops);
		}
	}
}

/**
 * tspp2_debugfs_exit() - TSPP2 device debugfs teardown.
 *
 * @device:	TSPP2 device.
 */
static void tspp2_debugfs_exit(struct tspp2_device *device)
{
	debugfs_remove_recursive(device->debugfs_entry);
	device->debugfs_entry = NULL;
}

/**
 *  tspp2_tsif_start() - Start TSIF device HW.
 *
 * @tsif_device:	TSIF device.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_tsif_start(struct tspp2_tsif_device *tsif_device)
{
	u32 ctl;

	if (tsif_device->ref_count > 0)
		return 0;

	ctl = (TSIF_STS_CTL_EN_IRQ | TSIF_STS_CTL_EN_DM |
		TSIF_STS_CTL_PACK_AVAIL | TSIF_STS_CTL_OVERFLOW |
		TSIF_STS_CTL_LOST_SYNC | TSIF_STS_CTL_TIMEOUT |
		TSIF_STS_CTL_PARALLEL);

	if (tsif_device->clock_inverse)
		ctl |= TSIF_STS_CTL_INV_CLOCK;

	if (tsif_device->data_inverse)
		ctl |= TSIF_STS_CTL_INV_DATA;

	if (tsif_device->sync_inverse)
		ctl |= TSIF_STS_CTL_INV_SYNC;

	if (tsif_device->enable_inverse)
		ctl |= TSIF_STS_CTL_INV_ENABLE;

	switch (tsif_device->mode) {
	case TSPP2_TSIF_MODE_LOOPBACK:
		ctl |= TSIF_STS_CTL_EN_NULL		|
				TSIF_STS_CTL_EN_ERROR	|
				TSIF_STS_CTL_TEST_MODE;
		break;
	case TSPP2_TSIF_MODE_1:
		ctl |= TSIF_STS_CTL_EN_TIME_LIM | TSIF_STS_CTL_EN_TCR;
		break;
	case TSPP2_TSIF_MODE_2:
		ctl |= TSIF_STS_CTL_EN_TIME_LIM		|
				TSIF_STS_CTL_EN_TCR	|
				TSIF_STS_CTL_MODE_2;
		break;
	default:
		pr_warn("%s: Unknown TSIF mode %d, setting to TSPP2_TSIF_MODE_2\n",
			__func__, tsif_device->mode);
		ctl |= TSIF_STS_CTL_EN_TIME_LIM		|
				TSIF_STS_CTL_EN_TCR	|
				TSIF_STS_CTL_MODE_2;
		break;
	}

	writel_relaxed(ctl, tsif_device->base + TSPP2_TSIF_STS_CTL);
	writel_relaxed(tsif_device->time_limit,
		  tsif_device->base + TSPP2_TSIF_TIME_LIMIT);
	wmb();
	writel_relaxed(ctl | TSIF_STS_CTL_START,
		  tsif_device->base + TSPP2_TSIF_STS_CTL);
	wmb();

	ctl = readl_relaxed(tsif_device->base + TSPP2_TSIF_STS_CTL);
	if (ctl & TSIF_STS_CTL_START)
		tsif_device->ref_count++;

	return (ctl & TSIF_STS_CTL_START) ? 0 : -EBUSY;
}


static int tspp2_vbif_clock_start(struct tspp2_device *device)
{
	int ret;

	if (device->tspp2_vbif_clk) {
		ret = clk_prepare_enable(device->tspp2_vbif_clk);
		if (ret) {
			pr_err("%s: Can't start tspp2_vbif_clk\n", __func__);
			return ret;
		}
	}

	if (device->vbif_ahb_clk) {
		ret = clk_prepare_enable(device->vbif_ahb_clk);
		if (ret) {
			pr_err("%s: Can't start vbif_ahb_clk\n", __func__);
			goto disable_vbif_tspp2;
		}
	}
	if (device->vbif_axi_clk) {
		ret = clk_prepare_enable(device->vbif_axi_clk);
		if (ret) {
			pr_err("%s: Can't start vbif_ahb_clk\n", __func__);
			goto disable_vbif_ahb;
		}
	}

	return 0;

disable_vbif_ahb:
	if (device->vbif_ahb_clk)
		clk_disable_unprepare(device->vbif_ahb_clk);
disable_vbif_tspp2:
	if (device->tspp2_vbif_clk)
		clk_disable_unprepare(device->tspp2_vbif_clk);

	return ret;
}

static void tspp2_vbif_clock_stop(struct tspp2_device *device)
{
	if (device->tspp2_vbif_clk)
		clk_disable_unprepare(device->tspp2_vbif_clk);

	if (device->vbif_ahb_clk)
		clk_disable_unprepare(device->vbif_ahb_clk);

	if (device->vbif_axi_clk)
		clk_disable_unprepare(device->vbif_axi_clk);
}

/**
 * tspp2_tsif_stop() - Stop TSIF device HW.
 *
 * @tsif_device:	TSIF device.
 */
static void tspp2_tsif_stop(struct tspp2_tsif_device *tsif_device)
{
	if (tsif_device->ref_count == 0)
		return;

	tsif_device->ref_count--;

	if (tsif_device->ref_count == 0) {
		writel_relaxed(TSIF_STS_CTL_STOP,
			tsif_device->base + TSPP2_TSIF_STS_CTL);
		/*
		 * The driver assumes that after this point the TSIF is stopped,
		 * so a memory barrier is required to allow
		 * further register writes.
		 */
		wmb();
	}
}

/* Clock functions */

static int tspp2_reg_clock_start(struct tspp2_device *device)
{
	int rc;

	if (device->tspp2_ahb_clk &&
		clk_prepare_enable(device->tspp2_ahb_clk) != 0) {
		pr_err("%s: Can't start tspp2_ahb_clk\n", __func__);
		return -EBUSY;
	}

	if (device->tspp2_core_clk &&
		clk_prepare_enable(device->tspp2_core_clk) != 0) {
		pr_err("%s: Can't start tspp2_core_clk\n", __func__);
		if (device->tspp2_ahb_clk)
			clk_disable_unprepare(device->tspp2_ahb_clk);
		return -EBUSY;
	}

	/* Request minimal bandwidth on the bus, required for register access */
	if (device->bus_client) {
		rc = msm_bus_scale_client_update_request(device->bus_client, 1);
		if (rc) {
			pr_err("%s: Can't enable bus\n", __func__);
			if (device->tspp2_core_clk)
				clk_disable_unprepare(device->tspp2_core_clk);
			if (device->tspp2_ahb_clk)
				clk_disable_unprepare(device->tspp2_ahb_clk);
			return -EBUSY;
		}
	}

	return 0;
}

static int tspp2_reg_clock_stop(struct tspp2_device *device)
{
	/* Minimize bandwidth bus voting */
	if (device->bus_client)
		msm_bus_scale_client_update_request(device->bus_client, 0);

	if (device->tspp2_core_clk)
		clk_disable_unprepare(device->tspp2_core_clk);

	if (device->tspp2_ahb_clk)
		clk_disable_unprepare(device->tspp2_ahb_clk);

	return 0;
}

/**
 * tspp2_clock_start() - Enable the required TSPP2 clocks
 *
 * @device:	The TSPP2 device.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_clock_start(struct tspp2_device *device)
{
	int tspp2_ahb_clk = 0;
	int tspp2_core_clk = 0;
	int tspp2_vbif_clk = 0;
	int tspp2_klm_ahb_clk = 0;
	int tsif_ref_clk = 0;

	if (device == NULL) {
		pr_err("%s: Can't start clocks, invalid device\n", __func__);
		return -EINVAL;
	}

	if (device->tspp2_ahb_clk) {
		if (clk_prepare_enable(device->tspp2_ahb_clk) != 0) {
			pr_err("%s: Can't start tspp2_ahb_clk\n", __func__);
			goto err_clocks;
		}
		tspp2_ahb_clk = 1;
	}

	if (device->tspp2_core_clk) {
		if (clk_prepare_enable(device->tspp2_core_clk) != 0) {
			pr_err("%s: Can't start tspp2_core_clk\n", __func__);
			goto err_clocks;
		}
		tspp2_core_clk = 1;
	}

	if (device->tspp2_klm_ahb_clk) {
		if (clk_prepare_enable(device->tspp2_klm_ahb_clk) != 0) {
			pr_err("%s: Can't start tspp2_klm_ahb_clk\n", __func__);
			goto err_clocks;
		}
		tspp2_klm_ahb_clk = 1;
	}

	if (device->tsif_ref_clk) {
		if (clk_prepare_enable(device->tsif_ref_clk) != 0) {
			pr_err("%s: Can't start tsif_ref_clk\n", __func__);
			goto err_clocks;
		}
		tsif_ref_clk = 1;
	}

	/* Request Max bandwidth on the bus, required for full operation */
	if (device->bus_client &&
		msm_bus_scale_client_update_request(device->bus_client, 2)) {
			pr_err("%s: Can't enable bus\n", __func__);
			goto err_clocks;
	}

	return 0;

err_clocks:
	if (tspp2_ahb_clk)
		clk_disable_unprepare(device->tspp2_ahb_clk);

	if (tspp2_core_clk)
		clk_disable_unprepare(device->tspp2_core_clk);

	if (tspp2_vbif_clk)
		clk_disable_unprepare(device->tspp2_vbif_clk);

	if (tspp2_klm_ahb_clk)
		clk_disable_unprepare(device->tspp2_klm_ahb_clk);

	if (tsif_ref_clk)
		clk_disable_unprepare(device->tsif_ref_clk);

	return -EBUSY;
}

/**
 * tspp2_clock_stop() - Disable TSPP2 clocks
 *
 * @device:	The TSPP2 device.
 */
static void tspp2_clock_stop(struct tspp2_device *device)
{
	if (device == NULL) {
		pr_err("%s: Can't stop clocks, invalid device\n", __func__);
		return;
	}

	/* Minimize bandwidth bus voting */
	if (device->bus_client)
		msm_bus_scale_client_update_request(device->bus_client, 0);

	if (device->tsif_ref_clk)
		clk_disable_unprepare(device->tsif_ref_clk);

	if (device->tspp2_klm_ahb_clk)
		clk_disable_unprepare(device->tspp2_klm_ahb_clk);

	if (device->tspp2_core_clk)
		clk_disable_unprepare(device->tspp2_core_clk);

	if (device->tspp2_ahb_clk)
		clk_disable_unprepare(device->tspp2_ahb_clk);
}

/**
 * tspp2_filter_counters_reset() - Reset a filter's HW counters.
 *
 * @device:	TSPP2 device.
 * @index:	Filter context index. Note counters are based on the context
 *		index and not on the filter HW index.
 */
static void tspp2_filter_counters_reset(struct tspp2_device *device, u32 index)
{
	/* Reset filter counters */
	writel_relaxed(0, device->base + TSPP2_FILTER_TSP_SYNC_ERROR(index));
	writel_relaxed(0, device->base + TSPP2_FILTER_ERRED_TSP(index));
	writel_relaxed(0, device->base + TSPP2_FILTER_DISCONTINUITIES(index));
	writel_relaxed(0,
		device->base + TSPP2_FILTER_SCRAMBLING_BITS_DISCARD(index));
	writel_relaxed(0, device->base + TSPP2_FILTER_TSP_TOTAL_NUM(index));
	writel_relaxed(0, device->base + TSPP2_FILTER_DISCONT_INDICATOR(index));
	writel_relaxed(0, device->base + TSPP2_FILTER_TSP_NO_PAYLOAD(index));
	writel_relaxed(0, device->base + TSPP2_FILTER_TSP_DUPLICATE(index));
	writel_relaxed(0, device->base + TSPP2_FILTER_KEY_FETCH_FAILURE(index));
	writel_relaxed(0, device->base + TSPP2_FILTER_DROPPED_PCR(index));
	writel_relaxed(0, device->base + TSPP2_FILTER_PES_ERRORS(index));
}

/**
 * tspp2_global_hw_reset() - Reset TSPP2 device registers to a default state.
 *
 * @device:		TSPP2 device.
 * @enable_intr:	Enable specific interrupts or disable them.
 *
 * A helper function called from probe() and remove(), this function resets both
 * TSIF devices' SW structures and verifies the TSIF HW is stopped. It resets
 * TSPP2 registers to appropriate default values and makes sure to disable
 * all sources, filters etc. Finally, it clears all interrupts and unmasks
 * the "important" interrupts.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_global_hw_reset(struct tspp2_device *device,
				int enable_intr)
{
	int i, n;
	unsigned long rate_in_hz = 0;
	u32 global_irq_en = 0;

	if (!device) {
		pr_err("%s: NULL device\n", __func__);
		return -ENODEV;
	}

	/* Stop TSIF devices */
	for (i = 0; i < TSPP2_NUM_TSIF_INPUTS; i++) {
		device->tsif_devices[i].hw_index = i;
		device->tsif_devices[i].dev = device;
		device->tsif_devices[i].mode = TSPP2_TSIF_MODE_2;
		device->tsif_devices[i].clock_inverse = 0;
		device->tsif_devices[i].data_inverse = 0;
		device->tsif_devices[i].sync_inverse = 0;
		device->tsif_devices[i].enable_inverse = 0;
		device->tsif_devices[i].stat_pkt_write_err = 0;
		device->tsif_devices[i].stat_pkt_read_err = 0;
		device->tsif_devices[i].stat_overflow = 0;
		device->tsif_devices[i].stat_lost_sync = 0;
		device->tsif_devices[i].stat_timeout = 0;
		device->tsif_devices[i].time_limit = TSPP2_TSIF_DEF_TIME_LIMIT;
		/* Set ref_count to 1 to allow stopping HW */
		device->tsif_devices[i].ref_count = 1;
		/* This will reset ref_count to 0 */
		tspp2_tsif_stop(&device->tsif_devices[i]);
	}

	/* Reset indexing table registers */
	for (i = 0; i < TSPP2_NUM_INDEXING_TABLES; i++) {
		writel_relaxed(0, device->base + TSPP2_INDEX_TABLE_PREFIX(i));
		writel_relaxed(0,
			device->base + TSPP2_INDEX_TABLE_PREFIX_MASK(i));
		for (n = 0; n < TSPP2_NUM_INDEXING_PATTERNS; n++) {
			writel_relaxed(0, device->base +
					TSPP2_INDEX_TABLE_PATTEREN(i, n));
			writel_relaxed(0,
				device->base + TSPP2_INDEX_TABLE_MASK(i, n));
		}
		/* Set number of patterns to 0, prefix size to 4 by default */
		writel_relaxed(0x00000400,
			device->base + TSPP2_INDEX_TABLE_PARAMS(i));
	}

	/* Disable TSIF inputs. Set mode of operation to 16 batches */
	for (i = 0; i < TSPP2_NUM_TSIF_INPUTS; i++)
		writel_relaxed((0x1 << TSIF_INPUT_SRC_CONFIG_16_BATCHES_OFFS),
			device->base + TSPP2_TSIF_INPUT_SRC_CONFIG(i));

	/* Reset source related registers and performance counters */
	for (i = 0; i < TSPP2_NUM_ALL_INPUTS; i++) {
		writel_relaxed(0, device->base + TSPP2_SRC_DEST_PIPES(i));

		/* Set source configuration to default values */
		writel_relaxed(TSPP2_DEFAULT_SRC_CONFIG,
			device->base + TSPP2_SRC_CONFIG(i));
	}
	writel_relaxed(0x000003FF, device->base + TSPP2_SRC_TOTAL_TSP_RESET);
	writel_relaxed(0x000003FF,
		device->base + TSPP2_SRC_FILTERED_OUT_TSP_RESET);

	/* Reset all contexts, each register handles 32 contexts */
	for (i = 0; i < 4; i++) {
		writel_relaxed(0xFFFFFFFF,
			device->base + TSPP2_TSP_CONTEXT_RESET(i));
		writel_relaxed(0xFFFFFFFF,
			device->base + TSPP2_PES_CONTEXT_RESET(i));
		writel_relaxed(0xFFFFFFFF,
			device->base + TSPP2_INDEXING_CONTEXT_RESET(i));
	}

	for (i = 0; i < TSPP2_NUM_HW_FILTERS; i++) {
		/*
		 * Reset operations: put exit operation in all filter operations
		 */
		for (n = 0; n < TSPP2_MAX_OPS_PER_FILTER; n++) {
			writel_relaxed(TSPP2_OPCODE_EXIT,
				device->base + TSPP2_OPCODE(i, n));
		}
		/* Disable all HW filters */
		writel_relaxed(0, device->base + TSPP2_FILTER_ENTRY0(i));
		writel_relaxed(0, device->base + TSPP2_FILTER_ENTRY1(i));
	}

	for (i = 0; i < TSPP2_NUM_CONTEXTS; i++) {
		/* Reset filter context-based counters */
		tspp2_filter_counters_reset(device, i);
	}

	/*
	 * Disable memory inputs. Set mode of operation to 16 batches.
	 * Configure last batch to be associated with all memory input sources,
	 * and add a filter to match all PIDs and drop the TS packets in the
	 * last HW filter entry. Use the last context for this filter.
	 */
	for (i = 0; i < TSPP2_NUM_MEM_INPUTS; i++)
		writel_relaxed(TSPP2_DEFAULT_MEM_SRC_CONFIG,
			device->base + TSPP2_MEM_INPUT_SRC_CONFIG(i));

	writel_relaxed(((TSPP2_NUM_CONTEXTS - 1) << FILTER_ENTRY1_CONTEXT_OFFS),
		device->base + TSPP2_FILTER_ENTRY1((TSPP2_NUM_HW_FILTERS - 1)));
	writel_relaxed((0x1 << FILTER_ENTRY0_EN_OFFS),
		device->base + TSPP2_FILTER_ENTRY0((TSPP2_NUM_HW_FILTERS - 1)));

	/* Reset pipe registers */
	for (i = 0; i < TSPP2_NUM_PIPES; i++)
		writel_relaxed(0xFFFF,
			device->base + TSPP2_PIPE_THRESH_CONFIG(i));

	writel_relaxed(0, device->base + TSPP2_PIPE_SECURITY);
	writel_relaxed(0x7FFFFFFF,
		device->base + TSPP2_DATA_NOT_SENT_ON_PIPE_RESET);

	/* Set global configuration to default values */

	/*
	 * Default: minimum time between PCRs = 50msec, STC offset is 0,
	 * transmit PCR on discontinuity.
	 */
	writel_relaxed(0x00000432, device->base + TSPP2_PCR_GLOBAL_CONFIG);

	/* Set correct value according to TSPP2 clock: */
	if (device->tspp2_core_clk) {
		rate_in_hz = clk_get_rate(device->tspp2_core_clk);
		writel_relaxed((rate_in_hz / MSEC_PER_SEC),
			device->base + TSPP2_CLK_TO_PCR_TIME_UNIT);
	} else {
		writel_relaxed(0x00000000,
			device->base + TSPP2_CLK_TO_PCR_TIME_UNIT);
	}

	writel_relaxed(0x00000000, device->base + TSPP2_DESC_WAIT_TIMEOUT);

	/* Clear all global interrupts */
	writel_relaxed(0xFFFF000F, device->base + TSPP2_GLOBAL_IRQ_CLEAR);
	writel_relaxed(0x7FFFFFFF,
			device->base + TSPP2_UNEXPECTED_RST_IRQ_CLEAR);
	writel_relaxed(0x7FFFFFFF,
			device->base + TSPP2_WRONG_PIPE_DIR_IRQ_CLEAR);
	writel_relaxed(0x7FFFFFFF,
			device->base + TSPP2_QSB_RESPONSE_ERROR_IRQ_CLEAR);
	writel_relaxed(0xFFFFFFFF,
			device->base + TSPP2_KEY_NOT_READY_IRQ_CLEAR);

	/*
	 * Global interrupts configuration:
	 * Flow Control (per memory source):	Disabled
	 * Read Failure (per memory source):	Enabled
	 * SC_GO_LOW (aggregate):		Enabled
	 * SC_GO_HIGH (aggregate):		Enabled
	 * Wrong Pipe Direction (aggregate):	Enabled
	 * QSB Response Error (aggregate):	Enabled
	 * Unexpected Reset (aggregate):	Enabled
	 * Key Not Ready (aggregate):		Disabled
	 * Op Encrypt Level Error:		Enabled
	 * PES No Sync:				Disabled (module parameter)
	 * TSP Invalid Length:			Disabled (module parameter)
	 * TSP Invalid AF Control:		Disabled (module parameter)
	 */
	global_irq_en = 0x00FF03E8;
	if (tspp2_en_invalid_af_ctrl)
		global_irq_en |=
			(0x1 << GLOBAL_IRQ_TSP_INVALID_AF_OFFS);
	if (tspp2_en_invalid_af_length)
		global_irq_en |= (0x1 << GLOBAL_IRQ_TSP_INVALID_LEN_OFFS);
	if (tspp2_en_pes_no_sync)
		global_irq_en |= (0x1 << GLOBAL_IRQ_PES_NO_SYNC_OFFS);

	if (enable_intr)
		writel_relaxed(global_irq_en,
			device->base + TSPP2_GLOBAL_IRQ_ENABLE);
	else
		writel_relaxed(0, device->base + TSPP2_GLOBAL_IRQ_ENABLE);

	if (enable_intr) {
		/* Enable all pipe related interrupts */
		writel_relaxed(0x7FFFFFFF,
			device->base + TSPP2_UNEXPECTED_RST_IRQ_ENABLE);
		writel_relaxed(0x7FFFFFFF,
			device->base + TSPP2_WRONG_PIPE_DIR_IRQ_ENABLE);
		writel_relaxed(0x7FFFFFFF,
			device->base + TSPP2_QSB_RESPONSE_ERROR_IRQ_ENABLE);
	} else {
		/* Disable all pipe related interrupts */
		writel_relaxed(0,
			device->base + TSPP2_UNEXPECTED_RST_IRQ_ENABLE);
		writel_relaxed(0,
			device->base + TSPP2_WRONG_PIPE_DIR_IRQ_ENABLE);
		writel_relaxed(0,
			device->base + TSPP2_QSB_RESPONSE_ERROR_IRQ_ENABLE);
	}

	/* Disable Key Ladder interrupts */
	writel_relaxed(0, device->base + TSPP2_KEY_NOT_READY_IRQ_ENABLE);

	/*
	 * Clear and disable scrambling control interrupts.
	 * Each register handles 32 filters.
	 */
	for (i = 0; i < 4; i++) {
		writel_relaxed(0xFFFFFFFF,
			device->base + TSPP2_SC_GO_HIGH_CLEAR(i));
		writel_relaxed(0, device->base + TSPP2_SC_GO_HIGH_ENABLE(i));
		writel_relaxed(0xFFFFFFFF,
			device->base + TSPP2_SC_GO_LOW_CLEAR(i));
		writel_relaxed(0, device->base + TSPP2_SC_GO_LOW_ENABLE(i));
	}

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}

/**
 * tspp2_event_work_handler - Handle the work - invoke the user callback.
 *
 * @work:	The work information.
 */
static void tspp2_event_work_handler(struct work_struct *work)
{
	struct tspp2_event_work *event_work =
		container_of(work, struct tspp2_event_work, work);
	struct tspp2_event_work cb_info = *event_work;

	if (mutex_lock_interruptible(&event_work->device->mutex))
		return;

	list_add_tail(&event_work->link, &event_work->device->free_work_list);

	mutex_unlock(&event_work->device->mutex);

	/*
	 * Must run callback with tspp2 device mutex unlocked,
	 * as callback might call tspp2 driver API and cause a deadlock.
	 */
	if (cb_info.callback)
		cb_info.callback(cb_info.cookie, cb_info.event_bitmask);
}

/**
 * tspp2_device_initialize() - Initialize TSPP2 device SW structures.
 *
 * @device:	TSPP2 device
 *
 * Initialize the required SW structures and fields in the TSPP2 device,
 * including ION client creation, BAM registration, debugfs initialization etc.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_device_initialize(struct tspp2_device *device)
{
	int i, ret;

	if (!device) {
		pr_err("%s: NULL device\n", __func__);
		return -ENODEV;
	}

	/* Register BAM */
	device->bam_props.summing_threshold = 0x10;
	device->bam_props.irq = device->bam_irq;
	device->bam_props.manage = SPS_BAM_MGR_LOCAL;

	ret = sps_register_bam_device(&device->bam_props, &device->bam_handle);
	if (ret) {
		pr_err("%s: failed to register BAM\n", __func__);
		return ret;
	}
	ret = sps_device_reset(device->bam_handle);
	if (ret) {
		sps_deregister_bam_device(device->bam_handle);
		pr_err("%s: error resetting BAM\n", __func__);
		return ret;
	}

	spin_lock_init(&device->spinlock);
	wakeup_source_init(&device->wakeup_src, dev_name(&device->pdev->dev));

	for (i = 0; i < TSPP2_NUM_TSIF_INPUTS; i++)
		tspp2_tsif_debugfs_init(&device->tsif_devices[i]);

	/*
	 * The device structure was allocated using devm_kzalloc() so
	 * the memory was initialized to zero. We don't need to specifically set
	 * fields to zero, then. We only set the fields we need to, such as
	 * batch_id.
	 */

	for (i = 0; i < TSPP2_NUM_BATCHES; i++) {
		device->batches[i].batch_id = i;
		device->batches[i].src = NULL;
		INIT_LIST_HEAD(&device->batches[i].link);
	}

	/*
	 * We set the device back-pointer in the sources, filters and pipes
	 * databases here, so that back-pointer is always valid (instead of
	 * setting it when opening a source, filter or pipe).
	 */
	for (i = 0; i < TSPP2_NUM_TSIF_INPUTS; i++)
		device->tsif_sources[i].device = device;

	for (i = 0; i < TSPP2_NUM_MEM_INPUTS; i++)
		device->mem_sources[i].device = device;

	for (i = 0; i < TSPP2_NUM_AVAIL_FILTERS; i++)
		device->filters[i].device = device;

	for (i = 0; i < TSPP2_NUM_PIPES; i++)
		device->pipes[i].device = device;

	/*
	 * Note: tsif_devices are initialized as part of tspp2_global_hw_reset()
	 */

	device->work_queue =
			create_singlethread_workqueue(dev_name(device->dev));
	INIT_LIST_HEAD(&device->free_work_list);
	for (i = 0; i < TSPP2_NUM_EVENT_WORK_ELEMENTS; i++) {
		device->work_pool[i].device = device;
		device->work_pool[i].callback = 0;
		device->work_pool[i].cookie = 0;
		device->work_pool[i].event_bitmask = 0;
		INIT_LIST_HEAD(&device->work_pool[i].link);
		INIT_WORK(&device->work_pool[i].work,
			tspp2_event_work_handler);

		list_add_tail(&device->work_pool[i].link,
			&device->free_work_list);
	}

	device->event_callback = NULL;
	device->event_cookie = NULL;

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}

/**
 * tspp2_device_uninitialize() - TSPP2 device teardown and cleanup.
 *
 * @device:	TSPP2 device
 *
 * TSPP2 device teardown: debugfs removal, BAM de-registration etc.
 * Return 0 on success, error value otherwise.
 */
static int tspp2_device_uninitialize(struct tspp2_device *device)
{
	int i;

	if (!device) {
		pr_err("%s: NULL device\n", __func__);
		return -ENODEV;
	}

	destroy_workqueue(device->work_queue);

	for (i = 0; i < TSPP2_NUM_TSIF_INPUTS; i++)
		tspp2_tsif_debugfs_exit(&device->tsif_devices[i]);

	/* Need to start clocks for BAM de-registration */
	if (pm_runtime_get_sync(device->dev) >= 0) {
		sps_deregister_bam_device(device->bam_handle);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
	}

	wakeup_source_trash(&device->wakeup_src);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}

/**
 * tspp2_src_disable_internal() - Helper function to disable a source.
 *
 * @src: Source to disable.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_src_disable_internal(struct tspp2_src *src)
{
	u32 reg;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		return -EINVAL;
	}

	if (!src->enabled) {
		pr_warn("%s: Source already disabled\n", __func__);
		return 0;
	}

	if ((src->input == TSPP2_INPUT_TSIF0) ||
		(src->input == TSPP2_INPUT_TSIF1)) {
		reg = readl_relaxed(src->device->base +
			TSPP2_TSIF_INPUT_SRC_CONFIG(src->input));
		reg &= ~(0x1 << TSIF_INPUT_SRC_CONFIG_INPUT_EN_OFFS);
		writel_relaxed(reg, src->device->base +
			TSPP2_TSIF_INPUT_SRC_CONFIG(src->input));

		tspp2_tsif_stop(&src->device->tsif_devices[src->input]);
	} else {
		reg = readl_relaxed(src->device->base +
			TSPP2_MEM_INPUT_SRC_CONFIG(src->hw_index));
		reg &= ~(0x1 << MEM_INPUT_SRC_CONFIG_INPUT_EN_OFFS);
		writel_relaxed(reg, src->device->base +
			TSPP2_MEM_INPUT_SRC_CONFIG(src->hw_index));
	}

	/*
	 * HW requires we wait for up to 2ms here before closing the pipes
	 * attached to (and used by) this source
	 */
	udelay(TSPP2_HW_DELAY_USEC);

	src->enabled = 0;
	src->device->num_enabled_sources--;

	if (src->device->num_enabled_sources == 0) {
		__pm_relax(&src->device->wakeup_src);
		tspp2_clock_stop(src->device);
	}

	return 0;
}

/* TSPP2 device open / close API */

/**
 * tspp2_device_open() - Open a TSPP2 device for use.
 *
 * @dev_id:	TSPP2 device ID.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_device_open(u32 dev_id)
{
	int rc;
	u32 reg = 0;
	struct tspp2_device *device;

	if (dev_id >= TSPP2_NUM_DEVICES) {
		pr_err("%s: Invalid device ID %d\n", __func__, dev_id);
		return -ENODEV;
	}

	device = tspp2_devices[dev_id];
	if (!device) {
		pr_err("%s: Invalid device\n", __func__);
		return -ENODEV;
	}

	if (mutex_lock_interruptible(&device->mutex))
		return -ERESTARTSYS;

	if (device->opened) {
		pr_err("%s: Device already opened\n", __func__);
		mutex_unlock(&device->mutex);
		return -EPERM;
	}

	/* Enable power regulator */
	rc = regulator_enable(device->gdsc);
	if (rc)
		goto err_mutex_unlock;

	/* Reset TSPP2 core */
	clk_reset(device->tspp2_core_clk, CLK_RESET_ASSERT);
	udelay(10);
	clk_reset(device->tspp2_core_clk, CLK_RESET_DEASSERT);

	/* Start HW clocks before accessing registers */
	rc = tspp2_reg_clock_start(device);
	if (rc)
		goto err_regulator_disable;

	rc = tspp2_global_hw_reset(device, 1);
	if (rc)
		goto err_stop_clocks;

	rc = tspp2_device_initialize(device);
	if (rc)
		goto err_stop_clocks;

	reg = readl_relaxed(device->base + TSPP2_VERSION);
	pr_info("TSPP2 HW Version: Major = %d, Minor = %d, Step = %d\n",
		((reg & 0xF0000000) >> VERSION_MAJOR_OFFS),
		((reg & 0x0FFF0000) >> VERSION_MINOR_OFFS),
		((reg & 0x0000FFFF) >> VERSION_STEP_OFFS));

	/* Stop HW clocks to save power */
	tspp2_reg_clock_stop(device);

	/* Enable runtime power management */
	pm_runtime_set_autosuspend_delay(device->dev, MSEC_PER_SEC);
	pm_runtime_use_autosuspend(device->dev);
	pm_runtime_enable(device->dev);

	device->opened = 1;

	mutex_unlock(&device->mutex);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;

err_stop_clocks:
	tspp2_reg_clock_stop(device);
err_regulator_disable:
	regulator_disable(device->gdsc);
err_mutex_unlock:
	mutex_unlock(&device->mutex);

	return rc;
}
EXPORT_SYMBOL(tspp2_device_open);

/**
 * tspp2_device_close() - Close a TSPP2 device.
 *
 * @dev_id:	TSPP2 device ID.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_device_close(u32 dev_id)
{
	int i;
	int ret = 0;
	struct tspp2_device *device;

	if (dev_id >= TSPP2_NUM_DEVICES) {
		pr_err("%s: Invalid device ID %d\n", __func__, dev_id);
		return -ENODEV;
	}

	device = tspp2_devices[dev_id];
	if (!device) {
		pr_err("%s: Invalid device\n", __func__);
		return -ENODEV;
	}

	ret = pm_runtime_get_sync(device->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&device->mutex);

	if (!device->opened) {
		pr_err("%s: Device already closed\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -EPERM;
	}
	device->opened = 0;

	/*
	 * In case the user has not disabled all the enabled sources, we need
	 * to disable them here, specifically in order to call tspp2_clock_stop,
	 * because the calls to enable and disable the clocks should be
	 * symmetrical (otherwise we cannot put the clocks).
	 */
	for (i = 0; i < TSPP2_NUM_TSIF_INPUTS; i++) {
		if (device->tsif_sources[i].enabled)
			tspp2_src_disable_internal(&device->tsif_sources[i]);
	}
	for (i = 0; i < TSPP2_NUM_MEM_INPUTS; i++) {
		if (device->mem_sources[i].enabled)
			tspp2_src_disable_internal(&device->mem_sources[i]);
	}

	/* bring HW registers back to a known state */
	tspp2_global_hw_reset(device, 0);

	tspp2_device_uninitialize(device);

	pm_runtime_mark_last_busy(device->dev);
	pm_runtime_put_autosuspend(device->dev);

	/* Disable runtime power management */
	pm_runtime_disable(device->dev);
	pm_runtime_set_suspended(device->dev);

	if (regulator_disable(device->gdsc))
		pr_err("%s: Error disabling power regulator\n", __func__);

	mutex_unlock(&device->mutex);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_device_close);

/* Global configuration API */

/**
 * tspp2_config_set() - Set device global configuration.
 *
 * @dev_id:	TSPP2 device ID.
 * @cfg:	TSPP2 global configuration parameters to set.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_config_set(u32 dev_id, const struct tspp2_config *cfg)
{
	int ret;
	u32 reg = 0;
	struct tspp2_device *device;

	if (dev_id >= TSPP2_NUM_DEVICES) {
		pr_err("%s: Invalid device ID %d\n", __func__, dev_id);
		return -ENODEV;
	}
	if (!cfg) {
		pr_err("%s: NULL configuration\n", __func__);
		return -EINVAL;
	}
	if (cfg->stc_byte_offset > 3) {
		pr_err("%s: Invalid stc_byte_offset %d, valid values are 0 - 3\n",
			__func__, cfg->stc_byte_offset);
		return -EINVAL;
	}

	device = tspp2_devices[dev_id];
	if (!device) {
		pr_err("%s: Invalid device\n", __func__);
		return -ENODEV;
	}

	ret = pm_runtime_get_sync(device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&device->mutex)) {
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -ERESTARTSYS;
	}

	if (!device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -EPERM;
	}

	if (cfg->pcr_on_discontinuity)
		reg |= (0x1 << PCR_GLOBAL_CONFIG_PCR_ON_DISCONT_OFFS);

	reg |= (cfg->stc_byte_offset << PCR_GLOBAL_CONFIG_STC_OFFSET_OFFS);
	reg |= (cfg->min_pcr_interval << PCR_GLOBAL_CONFIG_PCR_INTERVAL_OFFS);

	writel_relaxed(reg, device->base + TSPP2_PCR_GLOBAL_CONFIG);

	mutex_unlock(&device->mutex);

	pm_runtime_mark_last_busy(device->dev);
	pm_runtime_put_autosuspend(device->dev);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_config_set);

/**
 * tspp2_config_get() - Get current global configuration.
 *
 * @dev_id:	TSPP2 device ID.
 * @cfg:	TSPP2 global configuration parameters.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_config_get(u32 dev_id, struct tspp2_config *cfg)
{
	int ret;
	u32 reg = 0;
	struct tspp2_device *device;

	if (dev_id >= TSPP2_NUM_DEVICES) {
		pr_err("%s: Invalid device ID %d\n", __func__, dev_id);
		return -ENODEV;
	}
	if (!cfg) {
		pr_err("%s: NULL configuration\n", __func__);
		return -EINVAL;
	}

	device = tspp2_devices[dev_id];
	if (!device) {
		pr_err("%s: Invalid device\n", __func__);
		return -ENODEV;
	}

	ret = pm_runtime_get_sync(device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&device->mutex)) {
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -ERESTARTSYS;
	}

	if (!device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -EPERM;
	}

	reg = readl_relaxed(device->base + TSPP2_PCR_GLOBAL_CONFIG);

	cfg->pcr_on_discontinuity = ((reg & PCR_GLOBAL_CONFIG_PCR_ON_DISCONT) >>
					PCR_GLOBAL_CONFIG_PCR_ON_DISCONT_OFFS);
	cfg->stc_byte_offset = ((reg & PCR_GLOBAL_CONFIG_STC_OFFSET) >>
					PCR_GLOBAL_CONFIG_STC_OFFSET_OFFS);
	cfg->min_pcr_interval = ((reg & PCR_GLOBAL_CONFIG_PCR_INTERVAL) >>
					PCR_GLOBAL_CONFIG_PCR_INTERVAL_OFFS);

	mutex_unlock(&device->mutex);

	pm_runtime_mark_last_busy(device->dev);
	pm_runtime_put_autosuspend(device->dev);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_config_get);

/* Indexing tables API functions */

/**
 * tspp2_indexing_prefix_set() - Set prefix value and mask of an indexing table.
 *
 * @dev_id:	TSPP2 device ID.
 * @table_id:	Indexing table ID.
 * @value:	Prefix 4-byte value.
 * @mask:	Prefix 4-byte mask.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_indexing_prefix_set(u32 dev_id,
				u8 table_id,
				u32 value,
				u32 mask)
{
	int ret;
	u32 reg;
	u8 size = 0;
	int i;
	struct tspp2_device *device;
	struct tspp2_indexing_table *table;

	if (dev_id >= TSPP2_NUM_DEVICES) {
		pr_err("%s: Invalid device ID %d\n", __func__, dev_id);
		return -ENODEV;
	}
	if (table_id >= TSPP2_NUM_INDEXING_TABLES) {
		pr_err("%s: Invalid table ID %d\n", __func__, table_id);
		return -EINVAL;
	}

	device = tspp2_devices[dev_id];
	if (!device) {
		pr_err("%s: Invalid device\n", __func__);
		return -ENODEV;
	}

	ret = pm_runtime_get_sync(device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&device->mutex)) {
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -ERESTARTSYS;
	}

	if (!device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -EPERM;
	}

	table = &device->indexing_tables[table_id];
	table->prefix_value = value;
	table->prefix_mask = mask;

	/* HW expects values/masks to be written in Big Endian format */
	writel_relaxed(cpu_to_be32(value),
		device->base + TSPP2_INDEX_TABLE_PREFIX(table_id));
	writel_relaxed(cpu_to_be32(mask),
		device->base + TSPP2_INDEX_TABLE_PREFIX_MASK(table_id));

	/* Find the actual size of the prefix and set to HW */
	reg = readl_relaxed(device->base + TSPP2_INDEX_TABLE_PARAMS(table_id));
	for (i = 0; i < 32; i += 8) {
		if (mask & (0x000000FF << i))
			size++;
	}
	reg &= ~(0x7 << INDEX_TABLE_PARAMS_PREFIX_SIZE_OFFS);
	reg |= (size << INDEX_TABLE_PARAMS_PREFIX_SIZE_OFFS);
	writel_relaxed(reg, device->base + TSPP2_INDEX_TABLE_PARAMS(table_id));

	mutex_unlock(&device->mutex);

	pm_runtime_mark_last_busy(device->dev);
	pm_runtime_put_autosuspend(device->dev);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_indexing_prefix_set);

/**
 * tspp2_indexing_patterns_add() - Add patterns to an indexing table.
 *
 * @dev_id:		TSPP2 device ID.
 * @table_id:		Indexing table ID.
 * @values:		An array of 4-byte pattern values.
 * @masks:		An array of corresponding 4-byte masks.
 * @patterns_num:	Number of patterns in the values / masks arrays.
 *			Up to TSPP2_NUM_INDEXING_PATTERNS.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_indexing_patterns_add(u32 dev_id,
				u8 table_id,
				const u32 *values,
				const u32 *masks,
				u8 patterns_num)
{
	int ret;
	int i;
	u16 offs = 0;
	u32 reg;
	struct tspp2_device *device;
	struct tspp2_indexing_table *table;

	if (dev_id >= TSPP2_NUM_DEVICES) {
		pr_err("%s: Invalid device ID %d\n", __func__, dev_id);
		return -ENODEV;
	}
	if (table_id >= TSPP2_NUM_INDEXING_TABLES) {
		pr_err("%s: Invalid table ID %d\n", __func__, table_id);
		return -EINVAL;
	}
	if (!values || !masks) {
		pr_err("%s: NULL values or masks array\n", __func__);
		return -EINVAL;
	}

	device = tspp2_devices[dev_id];
	if (!device) {
		pr_err("%s: Invalid device\n", __func__);
		return -ENODEV;
	}

	ret = pm_runtime_get_sync(device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&device->mutex)) {
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -ERESTARTSYS;
	}

	if (!device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -EPERM;
	}

	table = &device->indexing_tables[table_id];

	if ((table->num_valid_entries + patterns_num) >
			TSPP2_NUM_INDEXING_PATTERNS) {
		pr_err("%s: Trying to add too many patterns: current number %d, trying to add %d, maximum allowed %d\n",
			__func__, table->num_valid_entries, patterns_num,
			TSPP2_NUM_INDEXING_PATTERNS);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -EINVAL;
	}

	/* There's enough room to add all the requested patterns */
	offs = table->num_valid_entries;
	for (i = 0; i < patterns_num; i++) {
		table->entry_value[offs + i] = values[i];
		table->entry_mask[offs + i] = masks[i];
		writel_relaxed(cpu_to_be32(values[i]),
			device->base +
				TSPP2_INDEX_TABLE_PATTEREN(table_id, offs + i));
		writel_relaxed(cpu_to_be32(masks[i]), device->base +
				TSPP2_INDEX_TABLE_MASK(table_id, offs + i));
	}
	table->num_valid_entries += patterns_num;
	reg = readl_relaxed(device->base + TSPP2_INDEX_TABLE_PARAMS(table_id));
	reg &= ~(0x1F << INDEX_TABLE_PARAMS_NUM_PATTERNS_OFFS);
	reg |= (table->num_valid_entries <<
			INDEX_TABLE_PARAMS_NUM_PATTERNS_OFFS);
	writel_relaxed(reg, device->base + TSPP2_INDEX_TABLE_PARAMS(table_id));

	mutex_unlock(&device->mutex);

	pm_runtime_mark_last_busy(device->dev);
	pm_runtime_put_autosuspend(device->dev);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_indexing_patterns_add);

/**
 * tspp2_indexing_patterns_clear() - Clear all patterns of an indexing table.
 *
 * @dev_id:	TSPP2 device ID.
 * @table_id:	Indexing table ID.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_indexing_patterns_clear(u32 dev_id,
				u8 table_id)
{
	int ret;
	int i;
	u32 reg;
	struct tspp2_device *device;
	struct tspp2_indexing_table *table;

	if (dev_id >= TSPP2_NUM_DEVICES) {
		pr_err("%s: Invalid device ID %d\n", __func__, dev_id);
		return -ENODEV;
	}
	if (table_id >= TSPP2_NUM_INDEXING_TABLES) {
		pr_err("%s: Invalid table ID %d\n", __func__, table_id);
		return -EINVAL;
	}

	device = tspp2_devices[dev_id];
	if (!device) {
		pr_err("%s: Invalid device\n", __func__);
		return -ENODEV;
	}

	ret = pm_runtime_get_sync(device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&device->mutex)) {
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -ERESTARTSYS;
	}

	if (!device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -EPERM;
	}

	table = &device->indexing_tables[table_id];

	for (i = 0; i < table->num_valid_entries; i++) {
		table->entry_value[i] = 0;
		table->entry_mask[i] = 0;
		writel_relaxed(0, device->base +
				TSPP2_INDEX_TABLE_PATTEREN(table_id, i));
		writel_relaxed(0, device->base +
				TSPP2_INDEX_TABLE_MASK(table_id, i));

	}
	table->num_valid_entries = 0;
	reg = readl_relaxed(device->base + TSPP2_INDEX_TABLE_PARAMS(table_id));
	reg &= ~(0x1F << INDEX_TABLE_PARAMS_NUM_PATTERNS_OFFS);
	writel_relaxed(reg, device->base + TSPP2_INDEX_TABLE_PARAMS(table_id));

	mutex_unlock(&device->mutex);

	pm_runtime_mark_last_busy(device->dev);
	pm_runtime_put_autosuspend(device->dev);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_indexing_patterns_clear);

/* Pipe API functions */

/**
 * tspp2_pipe_memory_init() - Initialize pipe memory helper function.
 *
 * @pipe:	The pipe to work on.
 *
 * The user is responsible for allocating the pipe's memory buffer via ION.
 * This helper function maps the given buffer to TSPP2 IOMMU memory space,
 * and sets the pipe's secure bit.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_pipe_memory_init(struct tspp2_pipe *pipe)
{
	int ret = 0;
	u32 reg;
	size_t align;
	unsigned long dummy_size = 0;
	size_t len = 0;
	int domain = 0;
	int partition = 0;
	int hlos_group_attached = 0;
	int cpz_group_attached = 0;
	int vbif_clk_started = 0;

	if (pipe->cfg.is_secure) {
		domain = pipe->device->iommu_info.cpz_domain_num;
		partition = pipe->device->iommu_info.cpz_partition;
		align = SZ_1M;
	} else {
		domain = pipe->device->iommu_info.hlos_domain_num;
		partition = pipe->device->iommu_info.hlos_partition;
		align = SZ_4K;
	}

	if (tspp2_iommu_bypass) {
		ret = ion_phys(pipe->cfg.ion_client,
				pipe->cfg.buffer_handle, &pipe->iova, &len);

		dummy_size = 0;

		if (ret) {
			pr_err("%s: Failed to get buffer physical address, ret = %d\n",
				__func__, ret);
			return ret;
		}

		if ((pipe->device->num_secured_opened_pipes +
			pipe->device->num_non_secured_opened_pipes) == 0) {
			ret = tspp2_vbif_clock_start(pipe->device);
			if (ret) {
				pr_err(
					"%s: tspp2_vbif_clock_start failed, ret=%d\n",
					__func__, ret);
				return ret;
			}
			vbif_clk_started = 1;
		}
	} else {
		/*
		 * We need to attach the group to enable the IOMMU and support
		 * the required memory mapping. This needs to be done before
		 * the first mapping is performed, so the number of opened pipes
		 * (of each type: secure or non-secure) is used as a
		 * reference count. Note that since the pipe descriptors are
		 * always allocated from HLOS domain, the HLOS group must be
		 * attached regardless of the pipe's security configuration.
		 * The mutex is taken at this point so there is no problem with
		 * synchronization.
		 */
		if ((pipe->device->num_secured_opened_pipes +
			pipe->device->num_non_secured_opened_pipes) == 0) {
			ret = tspp2_vbif_clock_start(pipe->device);
			if (ret) {
				pr_err("%s: tspp2_vbif_clock_start failed, ret=%d\n",
					__func__, ret);
				goto err_out;
			}
			vbif_clk_started = 1;

			pr_debug("%s: attaching HLOS group\n", __func__);
			ret = iommu_attach_group(
				pipe->device->iommu_info.hlos_domain,
				pipe->device->iommu_info.hlos_group);

			if (ret) {
				pr_err("%s: Failed attaching IOMMU HLOS group, %d\n",
					__func__, ret);
				goto err_out;
			}
			hlos_group_attached = 1;
		}

		if (pipe->cfg.is_secure &&
			(pipe->device->num_secured_opened_pipes == 0)) {
			pr_debug("%s: attaching CPZ group\n", __func__);
			ret = iommu_attach_group(
				pipe->device->iommu_info.cpz_domain,
				pipe->device->iommu_info.cpz_group);

			if (ret) {
				pr_err("%s: Failed attaching IOMMU CPZ group, %d\n",
					__func__, ret);
				goto err_out;
			}
			cpz_group_attached = 1;
		}

		/* Map to TSPP2 IOMMU */
		ret = ion_map_iommu(pipe->cfg.ion_client,
				pipe->cfg.buffer_handle,
				domain,
				partition,
				align, 0, &pipe->iova,
				&dummy_size, 0, 0); /* Uncached mapping */

		if (ret) {
			pr_err("%s: Failed mapping buffer to TSPP2, %d\n",
				__func__, ret);
			goto err_out;
		}
	}

	if (pipe->cfg.is_secure) {
		reg = readl_relaxed(pipe->device->base + TSPP2_PIPE_SECURITY);
		reg |= (0x1 << pipe->hw_index);
		writel_relaxed(reg, pipe->device->base + TSPP2_PIPE_SECURITY);
	}

	dev_dbg(pipe->device->dev, "%s: successful\n", __func__);

	return 0;

err_out:
	if (hlos_group_attached) {
		iommu_detach_group(pipe->device->iommu_info.hlos_domain,
				pipe->device->iommu_info.hlos_group);
	}

	if (cpz_group_attached) {
		iommu_detach_group(pipe->device->iommu_info.cpz_domain,
				pipe->device->iommu_info.cpz_group);
	}

	if (vbif_clk_started)
		tspp2_vbif_clock_stop(pipe->device);

	return ret;
}

/**
 * tspp2_pipe_memory_terminate() - Unmap pipe memory.
 *
 * @pipe:	The pipe to work on.
 *
 * Unmap the pipe's memory and clear the pipe's secure bit.
 */
static void tspp2_pipe_memory_terminate(struct tspp2_pipe *pipe)
{
	u32 reg;
	int domain = 0;
	int partition = 0;

	if (pipe->cfg.is_secure) {
		domain = pipe->device->iommu_info.cpz_domain_num;
		partition = pipe->device->iommu_info.cpz_partition;
	} else {
		domain = pipe->device->iommu_info.hlos_domain_num;
		partition = pipe->device->iommu_info.hlos_partition;
	}

	if (!tspp2_iommu_bypass) {
		ion_unmap_iommu(pipe->cfg.ion_client,
				pipe->cfg.buffer_handle,
				domain,
				partition);

		/*
		 * Opposite to what is done in tspp2_pipe_memory_init(),
		 * here we detach the IOMMU group when it is no longer in use.
		 */
		if (pipe->cfg.is_secure &&
			(pipe->device->num_secured_opened_pipes == 0)) {
			pr_debug("%s: detaching CPZ group\n", __func__);
			iommu_detach_group(
				pipe->device->iommu_info.cpz_domain,
				pipe->device->iommu_info.cpz_group);
		}

		if ((pipe->device->num_secured_opened_pipes +
			pipe->device->num_non_secured_opened_pipes) == 0) {
			pr_debug("%s: detaching HLOS group\n", __func__);
			iommu_detach_group(
				pipe->device->iommu_info.hlos_domain,
				pipe->device->iommu_info.hlos_group);
			tspp2_vbif_clock_stop(pipe->device);
		}
	} else if ((pipe->device->num_secured_opened_pipes +
		pipe->device->num_non_secured_opened_pipes) == 0) {
		tspp2_vbif_clock_stop(pipe->device);
	}

	pipe->iova = 0;

	if (pipe->cfg.is_secure) {
		reg = readl_relaxed(pipe->device->base + TSPP2_PIPE_SECURITY);
		reg &= ~(0x1 << pipe->hw_index);
		writel_relaxed(reg, pipe->device->base + TSPP2_PIPE_SECURITY);
	}

	dev_dbg(pipe->device->dev, "%s: successful\n", __func__);
}

/**
 * tspp2_sps_pipe_init() - BAM SPS pipe configuration and initialization
 *
 * @pipe:	The pipe to work on.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_sps_pipe_init(struct tspp2_pipe *pipe)
{
	u32 descriptors_num;
	unsigned long dummy_size = 0;
	int ret = 0;
	int iommu_mapped = 0;

	if (pipe->cfg.buffer_size % pipe->cfg.sps_cfg.descriptor_size) {
		pr_err(
			"%s: Buffer size %d is not aligned to descriptor size %d\n",
			__func__, pipe->cfg.buffer_size,
			pipe->cfg.sps_cfg.descriptor_size);
		return -EINVAL;
	}

	pipe->sps_pipe = sps_alloc_endpoint();
	if (!pipe->sps_pipe) {
		pr_err("%s: Failed to allocate BAM pipe\n", __func__);
		return -ENOMEM;
	}

	/* get default configuration */
	sps_get_config(pipe->sps_pipe, &pipe->sps_connect_cfg);
	if (pipe->cfg.pipe_mode == TSPP2_SRC_PIPE_INPUT) {
		pipe->sps_connect_cfg.mode = SPS_MODE_DEST;
		pipe->sps_connect_cfg.source = SPS_DEV_HANDLE_MEM;
		pipe->sps_connect_cfg.destination = pipe->device->bam_handle;
		pipe->sps_connect_cfg.dest_pipe_index = pipe->hw_index;
	} else {
		pipe->sps_connect_cfg.mode = SPS_MODE_SRC;
		pipe->sps_connect_cfg.source = pipe->device->bam_handle;
		pipe->sps_connect_cfg.destination = SPS_DEV_HANDLE_MEM;
		pipe->sps_connect_cfg.src_pipe_index = pipe->hw_index;
	}
	pipe->sps_connect_cfg.desc.base = NULL;
	pipe->sps_connect_cfg.options = pipe->cfg.sps_cfg.setting;
	descriptors_num = (pipe->cfg.buffer_size /
				pipe->cfg.sps_cfg.descriptor_size);

	/*
	 * If size of descriptors FIFO can hold N descriptors, we can submit
	 * (N-1) descriptors only, therefore we allocate extra descriptor
	 */
	descriptors_num++;
	pipe->sps_connect_cfg.desc.size = (descriptors_num *
					sizeof(struct sps_iovec));

	if (tspp2_iommu_bypass) {
		pipe->sps_connect_cfg.desc.base = dma_alloc_coherent(NULL,
					pipe->sps_connect_cfg.desc.size,
					&pipe->sps_connect_cfg.desc.phys_base,
					GFP_KERNEL);

		if (!pipe->sps_connect_cfg.desc.base) {
			pr_err("%s: Failed to allocate descriptor FIFO\n",
				__func__);
			ret = -ENOMEM;
			goto init_sps_failed_free_endpoint;
		}
	} else {
		pipe->desc_ion_handle = ion_alloc(pipe->cfg.ion_client,
					pipe->sps_connect_cfg.desc.size,
					SZ_4K, ION_HEAP(ION_IOMMU_HEAP_ID), 0);

		if (!pipe->desc_ion_handle) {
			pr_err("%s: Failed to allocate descriptors via ION\n",
				__func__);
			ret = -ENOMEM;
			goto init_sps_failed_free_endpoint;
		}

		ret = ion_map_iommu(pipe->cfg.ion_client,
			pipe->desc_ion_handle,
			pipe->device->iommu_info.hlos_domain_num,
			pipe->device->iommu_info.hlos_partition,
			SZ_4K, 0,
			&pipe->sps_connect_cfg.desc.phys_base,
			&dummy_size, 0, 0); /* Uncached mapping */

		if (ret) {
			pr_err("%s: Failed mapping descriptors to IOMMU\n",
				__func__);
			goto init_sps_failed_free_mem;
		}

		iommu_mapped = 1;

		pipe->sps_connect_cfg.desc.base =
			ion_map_kernel(pipe->cfg.ion_client,
			pipe->desc_ion_handle);

		if (!pipe->sps_connect_cfg.desc.base) {
			pr_err("%s: Failed mapping descriptors to kernel\n",
				__func__);
			ret = -ENOMEM;
			goto init_sps_failed_free_mem;
		}
	}

	ret = sps_connect(pipe->sps_pipe, &pipe->sps_connect_cfg);
	if (ret) {
		pr_err("%s: Failed to connect BAM, %d\n", __func__, ret);
		goto init_sps_failed_free_mem;
	}

	pipe->sps_event.options = pipe->cfg.sps_cfg.wakeup_events;
	if (pipe->sps_event.options) {
		pipe->sps_event.mode = SPS_TRIGGER_CALLBACK;
		pipe->sps_event.callback = pipe->cfg.sps_cfg.callback;
		pipe->sps_event.xfer_done = NULL;
		pipe->sps_event.user = pipe->cfg.sps_cfg.user_info;

		ret = sps_register_event(pipe->sps_pipe, &pipe->sps_event);
		if (ret) {
			pr_err("%s: Failed to register pipe event, %d\n",
					__func__, ret);
			goto init_sps_failed_free_connection;
		}
	}

	dev_dbg(pipe->device->dev, "%s: successful\n", __func__);

	return 0;

init_sps_failed_free_connection:
	sps_disconnect(pipe->sps_pipe);
init_sps_failed_free_mem:
	if (tspp2_iommu_bypass) {
		dma_free_coherent(NULL, pipe->sps_connect_cfg.desc.size,
					pipe->sps_connect_cfg.desc.base,
					pipe->sps_connect_cfg.desc.phys_base);
	} else {
		if (pipe->sps_connect_cfg.desc.base)
			ion_unmap_kernel(pipe->cfg.ion_client,
				pipe->desc_ion_handle);

		if (iommu_mapped) {
			ion_unmap_iommu(pipe->cfg.ion_client,
				pipe->desc_ion_handle,
				pipe->device->iommu_info.hlos_domain_num,
				pipe->device->iommu_info.hlos_partition);
		}

		ion_free(pipe->cfg.ion_client, pipe->desc_ion_handle);
	}
init_sps_failed_free_endpoint:
	sps_free_endpoint(pipe->sps_pipe);

	return ret;
}

/**
 * tspp2_sps_queue_descriptors() - Queue BAM SPS descriptors
 *
 * @pipe:	The pipe to work on.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_sps_queue_descriptors(struct tspp2_pipe *pipe)
{
	int ret = 0;
	u32 data_offset = 0;
	u32 desc_length = pipe->cfg.sps_cfg.descriptor_size;
	u32 desc_flags = pipe->cfg.sps_cfg.descriptor_flags;
	u32 data_length = pipe->cfg.buffer_size;

	while (data_length > 0) {
		ret = sps_transfer_one(pipe->sps_pipe,
				pipe->iova + data_offset,
				desc_length,
				pipe->cfg.sps_cfg.user_info,
				desc_flags);

		if (ret) {
			pr_err("%s: sps_transfer_one failed, %d\n",
				__func__, ret);
			return ret;
		}

		data_offset += desc_length;
		data_length -= desc_length;
	}

	return 0;
}

/**
 * tspp2_sps_pipe_terminate() - Disconnect and terminate SPS BAM pipe
 *
 * @pipe:	The pipe to work on.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_sps_pipe_terminate(struct tspp2_pipe *pipe)
{
	int ret;

	ret = sps_disconnect(pipe->sps_pipe);
	if (ret) {
		pr_err("%s: failed to disconnect BAM pipe, %d\n",
			__func__, ret);
		return ret;
	}
	if (tspp2_iommu_bypass) {
		dma_free_coherent(NULL, pipe->sps_connect_cfg.desc.size,
					pipe->sps_connect_cfg.desc.base,
					pipe->sps_connect_cfg.desc.phys_base);
	} else {
		ion_unmap_kernel(pipe->cfg.ion_client,
				pipe->desc_ion_handle);

		ion_unmap_iommu(pipe->cfg.ion_client,
				pipe->desc_ion_handle,
				pipe->device->iommu_info.hlos_domain_num,
				pipe->device->iommu_info.hlos_partition);

		ion_free(pipe->cfg.ion_client, pipe->desc_ion_handle);
	}
	pipe->sps_connect_cfg.desc.base = NULL;

	ret = sps_free_endpoint(pipe->sps_pipe);
	if (ret) {
		pr_err("%s: failed to release BAM end-point, %d\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

/**
 * tspp2_pipe_open() - Open a pipe for use.
 *
 * @dev_id:		TSPP2 device ID.
 * @cfg:		Pipe configuration parameters.
 * @iova:		TSPP2 IOMMU virtual address of the pipe's buffer.
 * @pipe_handle:	Opened pipe handle.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_pipe_open(u32 dev_id,
			const struct tspp2_pipe_config_params *cfg,
			ion_phys_addr_t *iova,
			u32 *pipe_handle)
{
	struct tspp2_device *device;
	struct tspp2_pipe *pipe;
	int i;
	int ret = 0;

	if (dev_id >= TSPP2_NUM_DEVICES) {
		pr_err("%s: Invalid device ID %d\n", __func__, dev_id);
		return -ENODEV;
	}

	if (!cfg || !iova || !pipe_handle) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	/* Some minimal sanity tests on the pipe configuration: */
	if (!cfg->ion_client || !cfg->buffer_handle) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	device = tspp2_devices[dev_id];
	if (!device) {
		pr_err("%s: Invalid device\n", __func__);
		return -ENODEV;
	}

	ret = pm_runtime_get_sync(device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&device->mutex)) {
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -ERESTARTSYS;
	}

	if (!device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -EPERM;
	}

	/* Find a free pipe */
	for (i = 0; i < TSPP2_NUM_PIPES; i++) {
		pipe = &device->pipes[i];
		if (!pipe->opened)
			break;
	}
	if (i == TSPP2_NUM_PIPES) {
		pr_err("%s: No available pipes\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -ENOMEM;
	}

	pipe->hw_index = i;
	/* Actual pipe threshold is set when the pipe is attached to a source */
	pipe->threshold = 0;
	pipe->cfg = *cfg;
	pipe->ref_cnt = 0;
	/* device back-pointer is already initialized, always remains valid */

	ret = tspp2_pipe_memory_init(pipe);
	if (ret) {
		pr_err("%s: Error initializing pipe memory\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return ret;
	}
	ret = tspp2_sps_pipe_init(pipe);
	if (ret) {
		pr_err("%s: Error initializing BAM pipe\n", __func__);
		tspp2_pipe_memory_terminate(pipe);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return ret;
	}

	/* For output pipes, we queue BAM descriptors here so they are ready */
	if (pipe->cfg.pipe_mode == TSPP2_SRC_PIPE_OUTPUT) {
		ret = tspp2_sps_queue_descriptors(pipe);
		if (ret) {
			pr_err("%s: Error queuing BAM pipe descriptors\n",
				__func__);
			tspp2_sps_pipe_terminate(pipe);
			tspp2_pipe_memory_terminate(pipe);
			mutex_unlock(&device->mutex);
			pm_runtime_mark_last_busy(device->dev);
			pm_runtime_put_autosuspend(device->dev);
			return ret;
		}
	}

	/* Reset counter */
	writel_relaxed((0x1 << pipe->hw_index),
		device->base + TSPP2_DATA_NOT_SENT_ON_PIPE_RESET);

	/* Return handle to the caller */
	*pipe_handle = (u32)pipe;
	*iova = pipe->iova;

	pipe->opened = 1;
	if (pipe->cfg.is_secure)
		device->num_secured_opened_pipes++;
	else
		device->num_non_secured_opened_pipes++;

	mutex_unlock(&device->mutex);

	pm_runtime_mark_last_busy(device->dev);
	pm_runtime_put_autosuspend(device->dev);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_pipe_open);

/**
 * tspp2_pipe_close() - Close an opened pipe.
 *
 * @pipe_handle:	Pipe to be closed.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_pipe_close(u32 pipe_handle)
{
	int ret;
	struct tspp2_pipe *pipe = (struct tspp2_pipe *)pipe_handle;

	if (!pipe) {
		pr_err("%s: Invalid pipe handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(pipe->device->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&pipe->device->mutex);

	if (!pipe->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&pipe->device->mutex);
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -EPERM;
	}

	if (!pipe->opened) {
		pr_err("%s: Pipe already closed\n", __func__);
		mutex_unlock(&pipe->device->mutex);
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -EINVAL;
	}

	if (pipe->ref_cnt > 0) {
		pr_err("%s: Pipe %u is still attached to a source\n",
			__func__, pipe_handle);
		mutex_unlock(&pipe->device->mutex);
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -EPERM;
	}

	/*
	 * Note: need to decrement the pipe reference count here, before
	 * calling tspp2_pipe_memory_terminate().
	 */
	if (pipe->cfg.is_secure)
		pipe->device->num_secured_opened_pipes--;
	else
		pipe->device->num_non_secured_opened_pipes--;

	tspp2_sps_pipe_terminate(pipe);
	tspp2_pipe_memory_terminate(pipe);

	pipe->iova = 0;
	pipe->opened = 0;

	mutex_unlock(&pipe->device->mutex);

	pm_runtime_mark_last_busy(pipe->device->dev);
	pm_runtime_put_autosuspend(pipe->device->dev);

	dev_dbg(pipe->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_pipe_close);

/* Source API functions */

/**
 * tspp2_src_open() - Open a new source for use.
 *
 * @dev_id:	TSPP2 device ID.
 * @cfg:	Source configuration parameters.
 * @src_handle:	Opened source handle.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_open(u32 dev_id,
			struct tspp2_src_cfg *cfg,
			u32 *src_handle)
{
	int ret;
	int i;
	struct tspp2_device *device;
	struct tspp2_src *src;
	enum tspp2_src_input input;

	if (dev_id >= TSPP2_NUM_DEVICES) {
		pr_err("%s: Invalid device ID %d\n", __func__, dev_id);
		return -ENODEV;
	}
	if (!src_handle) {
		pr_err("%s: Invalid source handle pointer\n", __func__);
		return -EINVAL;
	}
	if (!cfg) {
		pr_err("%s: Invalid configuration parameters\n", __func__);
		return -EINVAL;
	}

	device = tspp2_devices[dev_id];
	if (!device) {
		pr_err("%s: Invalid device\n", __func__);
		return -ENODEV;
	}

	ret = pm_runtime_get_sync(device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&device->mutex)) {
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -ERESTARTSYS;
	}

	if (!device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -EPERM;
	}

	input = cfg->input;
	if ((input == TSPP2_INPUT_TSIF0) || (input == TSPP2_INPUT_TSIF1)) {
		/* Input from TSIF */
		if (device->tsif_sources[input].opened) {
			pr_err("%s: TSIF input %d already opened\n",
				__func__, input);
			mutex_unlock(&device->mutex);
			pm_runtime_mark_last_busy(device->dev);
			pm_runtime_put_autosuspend(device->dev);
			return -EINVAL;
		}
		src = &device->tsif_sources[input];

		/*
		 * When writing to HW registers that are relevant to sources
		 * of both TSIF and memory input types, the register offsets
		 * for the TSIF-related registers come after the memory-related
		 * registers. For example: for TSPP2_SRC_CONFIG(n), n=[0..9],
		 * indexes 0..7 are for memory inputs, and indexes 8, 9 are
		 * for TSIF inputs.
		 */
		src->hw_index = TSPP2_NUM_MEM_INPUTS + input;

		/* Save TSIF source parameters in TSIF device */
		device->tsif_devices[input].mode =
			cfg->params.tsif_params.tsif_mode;
		device->tsif_devices[input].clock_inverse =
			cfg->params.tsif_params.clock_inverse;
		device->tsif_devices[input].data_inverse =
			cfg->params.tsif_params.data_inverse;
		device->tsif_devices[input].sync_inverse =
			cfg->params.tsif_params.sync_inverse;
		device->tsif_devices[input].enable_inverse =
			cfg->params.tsif_params.enable_inverse;
	} else {
		/* Input from memory */
		for (i = 0; i < TSPP2_NUM_MEM_INPUTS; i++) {
			if (!device->mem_sources[i].opened)
				break;
		}
		if (i == TSPP2_NUM_MEM_INPUTS) {
			pr_err("%s: No memory inputs available\n", __func__);
			mutex_unlock(&device->mutex);
			pm_runtime_mark_last_busy(device->dev);
			pm_runtime_put_autosuspend(device->dev);
			return -ENOMEM;
		}

		src = &device->mem_sources[i];
		src->hw_index = i;
	}

	src->opened = 1;
	src->input = input;
	src->pkt_format = TSPP2_PACKET_FORMAT_188_RAW; /* default value */
	src->scrambling_bits_monitoring = TSPP2_SRC_SCRAMBLING_MONITOR_NONE;
	INIT_LIST_HEAD(&src->batches_list);
	INIT_LIST_HEAD(&src->filters_list);
	src->input_pipe = NULL;
	INIT_LIST_HEAD(&src->output_pipe_list);
	src->num_associated_batches = 0;
	src->num_associated_pipes = 0;
	src->num_associated_filters = 0;
	src->reserved_filter_hw_index = 0;
	src->event_callback = NULL;
	src->event_cookie = NULL;
	src->event_bitmask = 0;
	src->enabled = 0;
	/* device back-pointer is already initialized, always remains valid */

	/* Reset source-related registers */
	if ((input == TSPP2_INPUT_TSIF0) || (input == TSPP2_INPUT_TSIF1)) {
		writel_relaxed((0x1 << TSIF_INPUT_SRC_CONFIG_16_BATCHES_OFFS),
			device->base +
			TSPP2_TSIF_INPUT_SRC_CONFIG(src->input));
	} else {
		/*
		 * Disable memory inputs. Set mode of operation to 16 batches.
		 * Configure last batch to be associated with this source.
		 */
		writel_relaxed(TSPP2_DEFAULT_MEM_SRC_CONFIG,
			device->base +
			TSPP2_MEM_INPUT_SRC_CONFIG(src->hw_index));
	}
	writel_relaxed(0, device->base +
				TSPP2_SRC_DEST_PIPES(src->hw_index));
	writel_relaxed(TSPP2_DEFAULT_SRC_CONFIG, device->base +
				TSPP2_SRC_CONFIG(src->hw_index));
	writel_relaxed((0x1 << src->hw_index),
		device->base + TSPP2_SRC_TOTAL_TSP_RESET);
	writel_relaxed((0x1 << src->hw_index),
		device->base + TSPP2_SRC_FILTERED_OUT_TSP_RESET);

	/* Return handle to the caller */
	*src_handle = (u32)src;

	mutex_unlock(&device->mutex);

	pm_runtime_mark_last_busy(device->dev);
	pm_runtime_put_autosuspend(device->dev);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_src_open);

/**
 * tspp2_src_close() - Close an opened source.
 *
 * @src_handle:	Source to be closed.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_close(u32 src_handle)
{
	unsigned long flags;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&src->device->mutex);

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source already closed\n", __func__);
		mutex_unlock(&src->device->mutex);
		return -EINVAL;
	}

	if (src->enabled) {
		pr_err("%s: Source needs to be disabled before it can be closed\n",
			__func__);
		mutex_unlock(&src->device->mutex);
		return -EPERM;
	}

	/* Verify resources have been released by the caller */
	if ((src->num_associated_batches > 0) ||
		(src->num_associated_pipes > 0) ||
		(src->num_associated_filters > 0)) {
		pr_err("%s: Source's resources need to be removed before it can be closed\n",
			__func__);
		mutex_unlock(&src->device->mutex);
		return -EPERM;
	}

	/*
	 * Most fields are reset to default values when opening a source, so
	 * there is no need to reset them all here. We only need to mark the
	 * source as closed.
	 */
	src->opened = 0;
	spin_lock_irqsave(&src->device->spinlock, flags);
	src->event_callback = NULL;
	src->event_cookie = NULL;
	src->event_bitmask = 0;
	spin_unlock_irqrestore(&src->device->spinlock, flags);
	src->enabled = 0;

	/*
	 * Source-related HW registers are reset when opening a source, so
	 * we don't reser them here. Note that a source is disabled before
	 * it is closed, so no need to disable it here either.
	 */

	mutex_unlock(&src->device->mutex);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_src_close);

/**
 * tspp2_src_parsing_option_set() - Set source parsing configuration option.
 *
 * @src_handle:	Source to configure.
 * @option:	Parsing configuration option to enable / disable.
 * @enable:	Enable / disable option.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_parsing_option_set(u32 src_handle,
			enum tspp2_src_parsing_option option,
			int enable)
{
	int ret;
	u32 reg = 0;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	reg = readl_relaxed(src->device->base +
			TSPP2_SRC_CONFIG(src->hw_index));

	switch (option) {
	case TSPP2_SRC_PARSING_OPT_CHECK_CONTINUITY:
		if (enable)
			reg |= (0x1 << SRC_CONFIG_CHECK_CONT_OFFS);
		else
			reg &= ~(0x1 << SRC_CONFIG_CHECK_CONT_OFFS);
		break;
	case TSPP2_SRC_PARSING_OPT_IGNORE_DISCONTINUITY:
		if (enable)
			reg |= (0x1 << SRC_CONFIG_IGNORE_DISCONT_OFFS);
		else
			reg &= ~(0x1 << SRC_CONFIG_IGNORE_DISCONT_OFFS);
		break;
	case TSPP2_SRC_PARSING_OPT_ASSUME_DUPLICATE_PACKETS:
		if (enable)
			reg |= (0x1 << SRC_CONFIG_ASSUME_DUPLICATES_OFFS);
		else
			reg &= ~(0x1 << SRC_CONFIG_ASSUME_DUPLICATES_OFFS);
		break;
	case TSPP2_SRC_PARSING_OPT_DISCARD_INVALID_AF_PACKETS:
		if (enable)
			reg |= (0x1 << SRC_CONFIG_DISCARD_INVALID_AF_OFFS);
		else
			reg &= ~(0x1 << SRC_CONFIG_DISCARD_INVALID_AF_OFFS);
		break;
	case TSPP2_SRC_PARSING_OPT_VERIFY_PES_START:
		if (enable)
			reg |= (0x1 << SRC_CONFIG_VERIFY_PES_START_OFFS);
		else
			reg &= ~(0x1 << SRC_CONFIG_VERIFY_PES_START_OFFS);
		break;
	default:
		pr_err("%s: Invalid option %d\n", __func__, option);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	writel_relaxed(reg, src->device->base +
			TSPP2_SRC_CONFIG(src->hw_index));

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_src_parsing_option_set);

/**
 * tspp2_src_parsing_option_get() - Get source parsing configuration option.
 *
 * @src_handle:	Source handle.
 * @option:	Parsing configuration option to get.
 * @enable:	Option's enable / disable indication.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_parsing_option_get(u32 src_handle,
			enum tspp2_src_parsing_option option,
			int *enable)
{
	int ret;
	u32 reg = 0;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}
	if (!enable) {
		pr_err("%s: NULL pointer\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	reg = readl_relaxed(src->device->base +
			TSPP2_SRC_CONFIG(src->hw_index));

	switch (option) {
	case TSPP2_SRC_PARSING_OPT_CHECK_CONTINUITY:
		*enable = ((reg >> SRC_CONFIG_CHECK_CONT_OFFS) & 0x1);
		break;
	case TSPP2_SRC_PARSING_OPT_IGNORE_DISCONTINUITY:
		*enable = ((reg >> SRC_CONFIG_IGNORE_DISCONT_OFFS) & 0x1);
		break;
	case TSPP2_SRC_PARSING_OPT_ASSUME_DUPLICATE_PACKETS:
		*enable = ((reg >> SRC_CONFIG_ASSUME_DUPLICATES_OFFS) & 0x1);
		break;
	case TSPP2_SRC_PARSING_OPT_DISCARD_INVALID_AF_PACKETS:
		*enable = ((reg >> SRC_CONFIG_DISCARD_INVALID_AF_OFFS) & 0x1);
		break;
	case TSPP2_SRC_PARSING_OPT_VERIFY_PES_START:
		*enable = ((reg >> SRC_CONFIG_VERIFY_PES_START_OFFS) & 0x1);
		break;
	default:
		pr_err("%s: Invalid option %d\n", __func__, option);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_src_parsing_option_get);

/**
 * tspp2_src_sync_byte_config_set() - Set source sync byte configuration.
 *
 * @src_handle:		Source to configure.
 * @check_sync_byte:	Check TS packet sync byte.
 * @sync_byte_value:	Sync byte value to check (e.g., 0x47).
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_sync_byte_config_set(u32 src_handle,
			int check_sync_byte,
			u8 sync_byte_value)
{
	int ret;
	u32 reg = 0;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	reg = readl_relaxed(src->device->base +
			TSPP2_SRC_CONFIG(src->hw_index));

	if (check_sync_byte)
		reg |= (0x1 << SRC_CONFIG_CHECK_SYNC_OFFS);
	else
		reg &= ~(0x1 << SRC_CONFIG_CHECK_SYNC_OFFS);

	reg &= ~(0xFF << SRC_CONFIG_SYNC_BYTE_OFFS);
	reg |= (sync_byte_value << SRC_CONFIG_SYNC_BYTE_OFFS);

	writel_relaxed(reg, src->device->base +
			TSPP2_SRC_CONFIG(src->hw_index));

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_src_sync_byte_config_set);

/**
 * tspp2_src_sync_byte_config_get() - Get source sync byte configuration.
 *
 * @src_handle:		Source handle.
 * @check_sync_byte:	Check TS packet sync byte indication.
 * @sync_byte_value:	Sync byte value.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_sync_byte_config_get(u32 src_handle,
			int *check_sync_byte,
			u8 *sync_byte_value)
{
	int ret;
	u32 reg = 0;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}
	if (!check_sync_byte || !sync_byte_value) {
		pr_err("%s: NULL pointer\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	reg = readl_relaxed(src->device->base +
			TSPP2_SRC_CONFIG(src->hw_index));

	*check_sync_byte = (reg >> SRC_CONFIG_CHECK_SYNC_OFFS) & 0x1;
	*sync_byte_value = (reg >> SRC_CONFIG_SYNC_BYTE_OFFS) & 0xFF;

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_src_sync_byte_config_get);

/**
 * tspp2_src_scrambling_config_set() - Set source scrambling configuration.
 *
 * @src_handle:	Source to configure.
 * @cfg:	Scrambling configuration to set.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_scrambling_config_set(u32 src_handle,
			const struct tspp2_src_scrambling_config *cfg)
{
	int ret;
	u32 reg = 0;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}
	if (!cfg) {
		pr_err("%s: NULL pointer\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	reg = readl_relaxed(src->device->base +
			TSPP2_SRC_CONFIG(src->hw_index));

	/* Clear all scrambling configuration bits before setting them */
	reg &= ~(0x3 << SRC_CONFIG_SCRAMBLING0_OFFS);
	reg &= ~(0x3 << SRC_CONFIG_SCRAMBLING1_OFFS);
	reg &= ~(0x3 << SRC_CONFIG_SCRAMBLING2_OFFS);
	reg &= ~(0x3 << SRC_CONFIG_SCRAMBLING3_OFFS);
	reg &= ~(0x3 << SRC_CONFIG_SCRAMBLING_MONITOR_OFFS);

	reg |= (cfg->scrambling_0_ctrl << SRC_CONFIG_SCRAMBLING0_OFFS);
	reg |= (cfg->scrambling_1_ctrl << SRC_CONFIG_SCRAMBLING1_OFFS);
	reg |= (cfg->scrambling_2_ctrl << SRC_CONFIG_SCRAMBLING2_OFFS);
	reg |= (cfg->scrambling_3_ctrl << SRC_CONFIG_SCRAMBLING3_OFFS);
	reg |= (cfg->scrambling_bits_monitoring <<
					SRC_CONFIG_SCRAMBLING_MONITOR_OFFS);

	writel_relaxed(reg, src->device->base +
			TSPP2_SRC_CONFIG(src->hw_index));

	src->scrambling_bits_monitoring = cfg->scrambling_bits_monitoring;

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_src_scrambling_config_set);

/**
 * tspp2_src_scrambling_config_get() - Get source scrambling configuration.
 *
 * @src_handle:	Source handle.
 * @cfg:	Scrambling configuration.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_scrambling_config_get(u32 src_handle,
			struct tspp2_src_scrambling_config *cfg)
{
	int ret;
	u32 reg = 0;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}
	if (!cfg) {
		pr_err("%s: NULL pointer\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	reg = readl_relaxed(src->device->base +
			TSPP2_SRC_CONFIG(src->hw_index));

	cfg->scrambling_0_ctrl = ((reg >> SRC_CONFIG_SCRAMBLING0_OFFS) & 0x3);
	cfg->scrambling_1_ctrl = ((reg >> SRC_CONFIG_SCRAMBLING1_OFFS) & 0x3);
	cfg->scrambling_2_ctrl = ((reg >> SRC_CONFIG_SCRAMBLING2_OFFS) & 0x3);
	cfg->scrambling_3_ctrl = ((reg >> SRC_CONFIG_SCRAMBLING3_OFFS) & 0x3);
	cfg->scrambling_bits_monitoring =
			((reg >> SRC_CONFIG_SCRAMBLING_MONITOR_OFFS) & 0x3);

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_src_scrambling_config_get);

/**
 * tspp2_src_packet_format_set() - Set source packet size and format.
 *
 * @src_handle:	Source to configure.
 * @format:	Packet format.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_packet_format_set(u32 src_handle,
				enum tspp2_packet_format format)
{
	int ret;
	u32 reg = 0;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	if (src->input == TSPP2_INPUT_MEMORY) {
		reg = readl_relaxed(src->device->base +
			TSPP2_MEM_INPUT_SRC_CONFIG(src->hw_index));

		reg &= ~(0x1 << MEM_INPUT_SRC_CONFIG_STAMP_SUFFIX_OFFS);
		reg &= ~(0x1 << MEM_INPUT_SRC_CONFIG_STAMP_EN_OFFS);

		switch (format) {
		case TSPP2_PACKET_FORMAT_188_RAW:
			/* We do not need to set any bit */
			break;
		case TSPP2_PACKET_FORMAT_192_HEAD:
			reg |= (0x1 << MEM_INPUT_SRC_CONFIG_STAMP_EN_OFFS);
			break;
		case TSPP2_PACKET_FORMAT_192_TAIL:
			reg |= (0x1 << MEM_INPUT_SRC_CONFIG_STAMP_EN_OFFS);
			reg |= (0x1 << MEM_INPUT_SRC_CONFIG_STAMP_SUFFIX_OFFS);
			break;
		default:
			pr_err("%s: Unknown packet format\n", __func__);
			mutex_unlock(&src->device->mutex);
			pm_runtime_mark_last_busy(src->device->dev);
			pm_runtime_put_autosuspend(src->device->dev);
			return -EINVAL;
		}
		writel_relaxed(reg, src->device->base +
			TSPP2_MEM_INPUT_SRC_CONFIG(src->hw_index));
	}
	src->pkt_format = format;

	/* Update source's input pipe threshold if needed */
	if (src->input_pipe) {
		if (src->pkt_format == TSPP2_PACKET_FORMAT_188_RAW)
			src->input_pipe->threshold = 188;
		else
			src->input_pipe->threshold = 192;

		writel_relaxed(src->input_pipe->threshold,
			src->input_pipe->device->base +
			TSPP2_PIPE_THRESH_CONFIG(src->input_pipe->hw_index));
	}

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_src_packet_format_set);

/**
 * tspp2_src_pipe_attach() - Attach a pipe to a source.
 *
 * @src_handle:		Source to attach the pipe to.
 * @pipe_handle:	Pipe to attach to the source.
 * @cfg:		For output pipes - the pipe's pull mode parameters.
 *			It is not allowed to pass NULL for output pipes.
 *			For input pipes this is irrelevant and the caller can
 *			pass NULL.
 *
 * This function attaches a given pipe to a given source.
 * The pipe's mode (input or output) was set when the pipe was opened.
 * An input pipe can be attached to a single source (with memory input).
 * A source can have multiple output pipes attached, and an output pipe can
 * be attached to multiple sources.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_pipe_attach(u32 src_handle,
				u32 pipe_handle,
				const struct tspp2_pipe_pull_mode_params *cfg)
{
	int ret;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;
	struct tspp2_pipe *pipe = (struct tspp2_pipe *)pipe_handle;
	struct tspp2_output_pipe *output_pipe = NULL;
	u32 reg;

	if (!src || !pipe) {
		pr_err("%s: Invalid source or pipe handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		goto err_inval;
	}

	if (!pipe->opened) {
		pr_err("%s: Pipe not opened\n", __func__);
		goto err_inval;
	}
	if ((pipe->cfg.pipe_mode == TSPP2_SRC_PIPE_OUTPUT) && (cfg == NULL)) {
		pr_err("%s: Invalid pull mode parameters\n", __func__);
		goto err_inval;
	}

	if (pipe->cfg.pipe_mode == TSPP2_SRC_PIPE_INPUT) {
		if (src->input_pipe != NULL) {
			pr_err("%s: Source already has an input pipe attached\n",
				__func__);
			goto err_inval;
		}
		if (pipe->ref_cnt > 0) {
			pr_err(
				"%s: Pipe %u is already attached to a source. An input pipe can only be attached once\n",
				__func__, pipe_handle);
			goto err_inval;
		}
		/*
		 * Input pipe threshold is determined according to the
		 * source's packet size.
		 */
		if (src->pkt_format == TSPP2_PACKET_FORMAT_188_RAW)
			pipe->threshold = 188;
		else
			pipe->threshold = 192;

		src->input_pipe = pipe;

		reg = readl_relaxed(src->device->base +
			TSPP2_MEM_INPUT_SRC_CONFIG(src->hw_index));
		reg &= ~(0x1F << MEM_INPUT_SRC_CONFIG_INPUT_PIPE_OFFS);
		reg |= (pipe->hw_index << MEM_INPUT_SRC_CONFIG_INPUT_PIPE_OFFS);
		writel_relaxed(reg, src->device->base +
			TSPP2_MEM_INPUT_SRC_CONFIG(src->hw_index));
	} else {
		list_for_each_entry(output_pipe,
				&src->output_pipe_list, link) {
			if (output_pipe->pipe == pipe) {
				pr_err(
				"%s: Output pipe %u is already attached to source %u\n",
					__func__, pipe_handle, src_handle);
				goto err_inval;
			}
		}
		output_pipe = kmalloc(sizeof(struct tspp2_output_pipe),
				GFP_KERNEL);
		if (!output_pipe) {
			pr_err("%s: No memory to save output pipe\n", __func__);
			mutex_unlock(&src->device->mutex);
			pm_runtime_mark_last_busy(src->device->dev);
			pm_runtime_put_autosuspend(src->device->dev);
			return -ENOMEM;
		}
		output_pipe->pipe = pipe;
		pipe->threshold = (cfg->threshold & 0xFFFF);
		list_add_tail(&output_pipe->link, &src->output_pipe_list);

		reg = readl_relaxed(src->device->base +
			TSPP2_SRC_DEST_PIPES(src->hw_index));
		if (cfg->is_stalling)
			reg |= (0x1 << pipe->hw_index);
		else
			reg &= ~(0x1 << pipe->hw_index);
		writel_relaxed(reg, src->device->base +
			TSPP2_SRC_DEST_PIPES(src->hw_index));
	}

	reg = readl_relaxed(pipe->device->base +
			TSPP2_PIPE_THRESH_CONFIG(pipe->hw_index));
	if ((pipe->cfg.pipe_mode == TSPP2_SRC_PIPE_OUTPUT) &&
		(pipe->ref_cnt > 0) && (pipe->threshold != (reg & 0xFFFF))) {
		pr_warn("%s: overwriting output pipe threshold\n", __func__);
	}

	writel_relaxed(pipe->threshold, pipe->device->base +
			TSPP2_PIPE_THRESH_CONFIG(pipe->hw_index));

	pipe->ref_cnt++;
	src->num_associated_pipes++;

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;

err_inval:
	mutex_unlock(&src->device->mutex);
	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	return -EINVAL;
}
EXPORT_SYMBOL(tspp2_src_pipe_attach);

/**
 * tspp2_src_pipe_detach() - Detach a pipe from a source.
 *
 * @src_handle:		Source to detach the pipe from.
 * @pipe_handle:	Pipe to detach from the source.
 *
 * Detaches a pipe from a source. The given pipe should have been previously
 * attached to this source as either an input pipe or an output pipe.
 * Note: there is no checking if this pipe is currently defined as the output
 * pipe of any operation!
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_pipe_detach(u32 src_handle, u32 pipe_handle)
{
	int ret;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;
	struct tspp2_pipe *pipe = (struct tspp2_pipe *)pipe_handle;
	struct tspp2_output_pipe *output_pipe = NULL;
	int found = 0;
	u32 reg;

	if (!src || !pipe) {
		pr_err("%s: Invalid source or pipe handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&src->device->mutex);

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		goto err_inval;
	}

	if (!pipe->opened) {
		pr_err("%s: Pipe not opened\n", __func__);
		goto err_inval;
	}

	if (pipe->cfg.pipe_mode == TSPP2_SRC_PIPE_INPUT) {
		if (src->input_pipe != pipe) {
			pr_err(
				"%s: Input pipe %u is not attached to source %u\n",
				__func__, pipe_handle, src_handle);
			goto err_inval;
		}

		writel_relaxed(0xFFFF,	src->input_pipe->device->base +
			TSPP2_PIPE_THRESH_CONFIG(src->input_pipe->hw_index));

		if (src->enabled) {
			pr_warn("%s: Detaching input pipe from an active memory source\n",
				__func__);
		}
		/*
		 * Note: not updating TSPP2_MEM_INPUT_SRC_CONFIG to reflect
		 * this pipe is detached, since there is no invalid value we
		 * can write instead. tspp2_src_pipe_attach() already takes
		 * care of zeroing the relevant bit-field before writing the
		 * new pipe nummber.
		 */

		src->input_pipe = NULL;
	} else {
		list_for_each_entry(output_pipe,
				&src->output_pipe_list, link) {
			if (output_pipe->pipe == pipe) {
				found = 1;
				break;
			}
		}
		if (found) {
			list_del(&output_pipe->link);
			kfree(output_pipe);
			reg = readl_relaxed(src->device->base +
				TSPP2_SRC_DEST_PIPES(src->hw_index));
			reg &= ~(0x1 << pipe->hw_index);
			writel_relaxed(reg, src->device->base +
				TSPP2_SRC_DEST_PIPES(src->hw_index));
			if (pipe->ref_cnt == 1) {
				writel_relaxed(0xFFFF,	pipe->device->base +
					TSPP2_PIPE_THRESH_CONFIG(
							pipe->hw_index));
			}
		} else {
			pr_err("%s: Output pipe %u is not attached to source %u\n",
				__func__, pipe_handle, src_handle);
			goto err_inval;
		}
	}
	pipe->ref_cnt--;
	src->num_associated_pipes--;

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;

err_inval:
	mutex_unlock(&src->device->mutex);
	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	return -EINVAL;
}
EXPORT_SYMBOL(tspp2_src_pipe_detach);

/**
 * tspp2_src_enable() - Enable source.
 *
 * @src_handle:	Source to enable.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_enable(u32 src_handle)
{
	struct tspp2_src *src = (struct tspp2_src *)src_handle;
	u32 reg;
	int ret;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	if (src->enabled) {
		pr_warn("%s: Source already enabled\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return 0;
	}

	/*
	 * Memory sources require their input pipe to be configured
	 * before enabling the source.
	 */
	if ((src->input == TSPP2_INPUT_MEMORY) && (src->input_pipe == NULL)) {
		pr_err("%s: A memory source must have an input pipe attached before enabling the source",
			__func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	if (src->device->num_enabled_sources == 0) {
		ret = tspp2_clock_start(src->device);
		if (ret) {
			mutex_unlock(&src->device->mutex);
			pm_runtime_mark_last_busy(src->device->dev);
			pm_runtime_put_autosuspend(src->device->dev);
			return ret;
		}
		__pm_stay_awake(&src->device->wakeup_src);
	}

	if ((src->input == TSPP2_INPUT_TSIF0) ||
		(src->input == TSPP2_INPUT_TSIF1)) {
		tspp2_tsif_start(&src->device->tsif_devices[src->input]);

		reg = readl_relaxed(src->device->base +
			TSPP2_TSIF_INPUT_SRC_CONFIG(src->input));
		reg |= (0x1 << TSIF_INPUT_SRC_CONFIG_INPUT_EN_OFFS);
		writel_relaxed(reg, src->device->base +
			TSPP2_TSIF_INPUT_SRC_CONFIG(src->input));
	} else {
		reg = readl_relaxed(src->device->base +
			TSPP2_MEM_INPUT_SRC_CONFIG(src->hw_index));
		reg |= (0x1 << MEM_INPUT_SRC_CONFIG_INPUT_EN_OFFS);
		writel_relaxed(reg, src->device->base +
			TSPP2_MEM_INPUT_SRC_CONFIG(src->hw_index));
	}

	src->enabled = 1;
	src->device->num_enabled_sources++;

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_src_enable);

/**
 * tspp2_src_disable() - Disable source.
 *
 * @src_handle:	Source to disable.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_disable(u32 src_handle)
{
	struct tspp2_src *src = (struct tspp2_src *)src_handle;
	int ret;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&src->device->mutex);

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	ret = tspp2_src_disable_internal(src);

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	if (!ret)
		dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return ret;
}
EXPORT_SYMBOL(tspp2_src_disable);

/**
 *  tspp2_filter_ops_clear() - Clear filter operations database and HW
 *
 * @filter:	The filter to work on.
 */
static void tspp2_filter_ops_clear(struct tspp2_filter *filter)
{
	int i;

	/* Set all filter operations in HW to Exit operation */
	for (i = 0; i < TSPP2_MAX_OPS_PER_FILTER; i++) {
		writel_relaxed(TSPP2_OPCODE_EXIT, filter->device->base +
					TSPP2_OPCODE(filter->hw_index, i));
	}
	memset(filter->operations, 0,
		(sizeof(struct tspp2_operation) * TSPP2_MAX_OPS_PER_FILTER));
	filter->num_user_operations = 0;
	filter->indexing_op_set = 0;
	filter->raw_op_with_indexing = 0;
	filter->pes_analysis_op_set = 0;
	filter->raw_op_set = 0;
	filter->pes_tx_op_set = 0;
}

/**
 * tspp2_filter_context_reset() - Reset filter context and release it.
 *
 * @filter:	The filter to work on.
 */
static void tspp2_filter_context_reset(struct tspp2_filter *filter)
{
	/* Reset this filter's context. Each register handles 32 contexts */
	writel_relaxed((0x1 << TSPP2_MODULUS_OP(filter->context, 32)),
		filter->device->base +
			TSPP2_TSP_CONTEXT_RESET(filter->context >> 5));
	writel_relaxed((0x1 << TSPP2_MODULUS_OP(filter->context, 32)),
		filter->device->base +
			TSPP2_PES_CONTEXT_RESET(filter->context >> 5));
	writel_relaxed((0x1 << TSPP2_MODULUS_OP(filter->context, 32)),
		filter->device->base +
			TSPP2_INDEXING_CONTEXT_RESET(filter->context >> 5));

	writel_relaxed(0, filter->device->base +
		TSPP2_FILTER_ENTRY1(filter->hw_index));

	/* Release context */
	filter->device->contexts[filter->context] = 0;
}

/**
 * tspp2_filter_sw_reset() - Reset filter SW fields helper function.
 *
 * @filter:	The filter to work on.
 */
static void tspp2_filter_sw_reset(struct tspp2_filter *filter)
{
	unsigned long flags;
	/*
	 * All fields are cleared when opening a filter. Still it is important
	 * to reset some of the fields here, specifically to set opened to 0 and
	 * also to set the callback to NULL.
	 */
	filter->opened = 0;
	filter->src = NULL;
	filter->batch = NULL;
	filter->context = 0;
	filter->hw_index = 0;
	filter->pid_value = 0;
	filter->mask = 0;
	spin_lock_irqsave(&filter->device->spinlock, flags);
	filter->event_callback = NULL;
	filter->event_cookie = NULL;
	filter->event_bitmask = 0;
	spin_unlock_irqrestore(&filter->device->spinlock, flags);
	filter->enabled = 0;
}

/**
 * tspp2_src_batch_set() - Set/clear a filter batch to/from a source.
 *
 * @src:	The source to work on.
 * @batch_id:	The batch to set/clear.
 * @set:	Set/clear flag.
 */
static void tspp2_src_batch_set(struct tspp2_src *src, u8 batch_id, int set)
{
	u32 reg = 0;

	if (src->input == TSPP2_INPUT_MEMORY) {
		reg = readl_relaxed(src->device->base +
			TSPP2_MEM_INPUT_SRC_CONFIG(src->hw_index));
		if (set)
			reg |= ((1 << batch_id) <<
				MEM_INPUT_SRC_CONFIG_BATCHES_OFFS);
		else
			reg &= ~((1 << batch_id) <<
				MEM_INPUT_SRC_CONFIG_BATCHES_OFFS);
		writel_relaxed(reg, src->device->base +
			TSPP2_MEM_INPUT_SRC_CONFIG(src->hw_index));
	} else {
		reg = readl_relaxed(src->device->base +
			TSPP2_TSIF_INPUT_SRC_CONFIG(src->input));
		if (set)
			reg |= ((1 << batch_id) <<
				TSIF_INPUT_SRC_CONFIG_BATCHES_OFFS);
		else
			reg &= ~((1 << batch_id) <<
				TSIF_INPUT_SRC_CONFIG_BATCHES_OFFS);
		writel_relaxed(reg, src->device->base +
			TSPP2_TSIF_INPUT_SRC_CONFIG(src->input));
	}
}

/**
 * tspp2_src_filters_clear() - Clear all filters from a source.
 *
 * @src_handle:	Source to clear all filters from.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_filters_clear(u32 src_handle)
{
	int ret;
	int i;
	struct tspp2_filter *filter = NULL;
	struct tspp2_filter *tmp_filter;
	struct tspp2_filter_batch *batch = NULL;
	struct tspp2_filter_batch *tmp_batch;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&src->device->mutex);

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	/* Go over filters in source, disable them, clear their operations,
	 * "close" them (similar to tspp2_filter_close function but simpler).
	 * No need to worry about cases of reserved filter, so just clear
	 * filters HW- and SW-wise. Then update source's filters and batches
	 * lists and numbers. Simple :)
	 */
	list_for_each_entry_safe(filter, tmp_filter, &src->filters_list, link) {
		/* Disable filter */
		writel_relaxed(0, filter->device->base +
			TSPP2_FILTER_ENTRY0(filter->hw_index));
		/* Clear filter operations in HW as well as related SW fields */
		tspp2_filter_ops_clear(filter);
		/* Reset filter context-based counters */
		tspp2_filter_counters_reset(filter->device, filter->context);
		/* Reset filter context and release it back to the device */
		tspp2_filter_context_reset(filter);
		/* Reset filter SW fields */
		tspp2_filter_sw_reset(filter);

		list_del(&filter->link);
	}

	list_for_each_entry_safe(batch, tmp_batch, &src->batches_list, link) {
		tspp2_src_batch_set(src, batch->batch_id, 0);
		for (i = 0; i < TSPP2_FILTERS_PER_BATCH; i++)
			batch->hw_filters[i] = 0;
		batch->src = NULL;
		list_del(&batch->link);
	}

	src->num_associated_batches = 0;
	src->num_associated_filters = 0;
	src->reserved_filter_hw_index = 0;

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_src_filters_clear);

/* Filters and Operations API functions */

/**
 * tspp2_filter_open() - Open a new filter and add it to a source.
 *
 * @src_handle:		Source to add the new filter to.
 * @pid:		Filter's 13-bit PID value.
 * @mask:		Filter's 13-bit mask. Note it is highly recommended
 *			to use a full bit mask of 0x1FFF, so the filter
 *			operates on a unique PID.
 * @filter_handle:	Opened filter handle.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_filter_open(u32 src_handle, u16 pid, u16 mask, u32 *filter_handle)
{
	struct tspp2_src *src = (struct tspp2_src *)src_handle;
	struct tspp2_filter_batch *batch;
	struct tspp2_filter *filter = NULL;
	u16 hw_idx;
	int i;
	u32 reg = 0;
	int found = 0;
	int ret;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}
	if (!filter_handle) {
		pr_err("%s: Invalid filter handle pointer\n", __func__);
		return -EINVAL;
	}

	if ((pid & ~0x1FFF) || (mask & ~0x1FFF)) {
		pr_err("%s: Invalid PID or mask values (13 bits available)\n",
			__func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EINVAL;
	}

	/* Find an available filter object in the device's filters database */
	for (i = 0; i < TSPP2_NUM_AVAIL_FILTERS; i++)
		if (!src->device->filters[i].opened)
			break;
	if (i == TSPP2_NUM_AVAIL_FILTERS) {
		pr_err("%s: No available filters\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ENOMEM;
	}
	filter = &src->device->filters[i];

	/* Find an available context. Each new filter needs a unique context */
	for (i = 0; i < TSPP2_NUM_AVAIL_CONTEXTS; i++)
		if (!src->device->contexts[i])
			break;
	if (i == TSPP2_NUM_AVAIL_CONTEXTS) {
		pr_err("%s: No available filters\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ENOMEM;
	}
	src->device->contexts[i] = 1;
	filter->context = i;

	if (src->num_associated_batches) {
		/*
		 * Look for an available HW filter among the batches
		 * already associated with this source.
		 */
		list_for_each_entry(batch, &src->batches_list, link) {
			for (i = 0; i < TSPP2_FILTERS_PER_BATCH; i++) {
				hw_idx = (batch->batch_id *
						TSPP2_FILTERS_PER_BATCH) + i;
				if ((hw_idx != src->reserved_filter_hw_index) &&
					(batch->hw_filters[i] == 0))
					break;
			}
			if (i < TSPP2_FILTERS_PER_BATCH) {
				/* Found an available HW filter */
				batch->hw_filters[i] = 1;
				found = 1;
				break;
			}
		}
	}

	if (!found) {
		/* Either the source did not have any associated batches,
		 * or we could not find an available HW filter in any of
		 * the source's batches. In any case, we need to find a new
		 * batch. Then we use the first filter in this batch.
		 */
		for (i = 0; i < TSPP2_NUM_BATCHES; i++) {
			if (!src->device->batches[i].src) {
				src->device->batches[i].src = src;
				batch = &src->device->batches[i];
				batch->hw_filters[0] = 1;
				hw_idx = (batch->batch_id *
						TSPP2_FILTERS_PER_BATCH);
				break;
			}
		}
		if (i == TSPP2_NUM_BATCHES) {
			pr_err("%s: No available filters\n", __func__);
			src->device->contexts[filter->context] = 0;
			filter->context = 0;
			mutex_unlock(&src->device->mutex);
			pm_runtime_mark_last_busy(src->device->dev);
			pm_runtime_put_autosuspend(src->device->dev);
			return -ENOMEM;
		}

		tspp2_src_batch_set(src, batch->batch_id, 1);

		list_add_tail(&batch->link, &src->batches_list);

		/* Update reserved filter index only when needed */
		if (src->num_associated_batches == 0) {
			src->reserved_filter_hw_index =
				(batch->batch_id * TSPP2_FILTERS_PER_BATCH) +
					TSPP2_FILTERS_PER_BATCH - 1;
		}
		src->num_associated_batches++;
	}

	filter->opened = 1;
	filter->src = src;
	filter->batch = batch;
	filter->hw_index = hw_idx;
	filter->pid_value = pid;
	filter->mask = mask;
	filter->indexing_table_id = 0;
	tspp2_filter_ops_clear(filter);
	filter->event_callback = NULL;
	filter->event_cookie = NULL;
	filter->event_bitmask = 0;
	filter->enabled = 0;
	/* device back-pointer is already initialized, always remains valid */

	list_add_tail(&filter->link, &src->filters_list);
	src->num_associated_filters++;

	/* Reset filter context-based counters */
	tspp2_filter_counters_reset(filter->device, filter->context);

	/* Reset this filter's context */
	writel_relaxed((0x1 << TSPP2_MODULUS_OP(filter->context, 32)),
		filter->device->base +
		TSPP2_TSP_CONTEXT_RESET(filter->context >> 5));
	writel_relaxed((0x1 << TSPP2_MODULUS_OP(filter->context, 32)),
		filter->device->base +
		TSPP2_PES_CONTEXT_RESET(filter->context >> 5));
	writel_relaxed((0x1 << TSPP2_MODULUS_OP(filter->context, 32)),
		filter->device->base +
		TSPP2_INDEXING_CONTEXT_RESET(filter->context >> 5));

	/* Write PID and mask */
	reg = ((pid << FILTER_ENTRY0_PID_OFFS) |
		(mask << FILTER_ENTRY0_MASK_OFFS));
	writel_relaxed(reg, filter->device->base +
		TSPP2_FILTER_ENTRY0(filter->hw_index));

	writel_relaxed((filter->context << FILTER_ENTRY1_CONTEXT_OFFS),
		filter->device->base + TSPP2_FILTER_ENTRY1(filter->hw_index));

	*filter_handle = (u32)filter;

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_filter_open);

/**
 * tspp2_hw_filters_in_batch() - Check for used HW filters in a batch.
 *
 * @batch:	The filter batch to check.
 *
 * Helper function to check if there are any HW filters used on this batch.
 *
 * Return 1 if found a used filter in this batch, 0 otherwise.
 */
static inline int tspp2_hw_filters_in_batch(struct tspp2_filter_batch *batch)
{
	int i;

	for (i = 0; i < TSPP2_FILTERS_PER_BATCH; i++)
		if (batch->hw_filters[i] == 1)
			return 1;

	return 0;
}

/**
 * tspp2_filter_close() - Close a filter.
 *
 * @filter_handle:	Filter to close.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_filter_close(u32 filter_handle)
{
	int i;
	int ret;
	struct tspp2_device *device;
	struct tspp2_src *src = NULL;
	struct tspp2_filter_batch *batch = NULL;
	struct tspp2_filter_batch *tmp_batch;
	struct tspp2_filter *filter = (struct tspp2_filter *)filter_handle;

	if (!filter) {
		pr_err("%s: Invalid filter handle\n", __func__);
		return -EINVAL;
	}

	device = filter->device;

	ret = pm_runtime_get_sync(device->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&device->mutex);

	if (!device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -EPERM;
	}

	if (!filter->opened) {
		pr_err("%s: Filter already closed\n", __func__);
		mutex_unlock(&device->mutex);
		pm_runtime_mark_last_busy(device->dev);
		pm_runtime_put_autosuspend(device->dev);
		return -EINVAL;
	}

	if (filter->num_user_operations)
		pr_warn("%s: Closing filters that has %d operations\n",
			__func__, filter->num_user_operations);

	/* Disable filter */
	writel_relaxed(0, device->base +
		TSPP2_FILTER_ENTRY0(filter->hw_index));

	/* Clear filter operations in HW as well as related SW fields */
	tspp2_filter_ops_clear(filter);

	/* Reset filter context-based counters */
	tspp2_filter_counters_reset(device, filter->context);

	/* Reset filter context and release it back to the device */
	tspp2_filter_context_reset(filter);

	/* Mark filter as unused in batch */
	filter->batch->hw_filters[(filter->hw_index -
		(filter->batch->batch_id * TSPP2_FILTERS_PER_BATCH))] = 0;

	/* Remove filter from source */
	list_del(&filter->link);
	filter->src->num_associated_filters--;

	/* We may need to update the reserved filter for this source.
	 * Cases to handle:
	 * 1. This is the last filter on this source.
	 * 2. This is the last filter on this batch + reserved filter is not on
	 * this batch.
	 * 3. This is the last filter on this batch + reserved filter is on this
	 * batch. Can possibly move reserved filter to another batch if space is
	 * available.
	 * 4. This is not the last filter on this batch. The reserved filter may
	 * be the only one taking another batch and may be moved to this batch
	 * to save space.
	 */

	src = filter->src;
	/*
	 * Case #1: this could be the last filter associated with this source.
	 * If this is the case, we can release the batch too. We don't care
	 * about the reserved HW filter index, since there are no more filters.
	 */
	if (src->num_associated_filters == 0) {
		filter->batch->src = NULL;
		list_del(&filter->batch->link);
		src->num_associated_batches--;
		tspp2_src_batch_set(src, filter->batch->batch_id, 0);
		src->reserved_filter_hw_index = 0;
		goto filter_clear;
	}

	/*
	 * If this is the last filter that was used in this batch, we may be
	 * able to release this entire batch. However, we have to make sure the
	 * reserved filter is not in this batch. If it is, we may find a place
	 * for it in another batch in this source.
	 */
	if (!tspp2_hw_filters_in_batch(filter->batch)) {
		/* There are no more used filters on this batch */
		if ((src->reserved_filter_hw_index <
			(filter->batch->batch_id * TSPP2_FILTERS_PER_BATCH)) ||
			(src->reserved_filter_hw_index >=
			((filter->batch->batch_id * TSPP2_FILTERS_PER_BATCH) +
				TSPP2_FILTERS_PER_BATCH))) {
			/* Case #2: the reserved filter is not on this batch */
			filter->batch->src = NULL;
			list_del(&filter->batch->link);
			src->num_associated_batches--;
			tspp2_src_batch_set(src, filter->batch->batch_id, 0);
		} else {
			/*
			 * Case #3: see if we can "move" the reserved filter to
			 * a different batch.
			 */
			list_for_each_entry_safe(batch, tmp_batch,
					&src->batches_list, link) {
				if (batch == filter->batch)
					continue;

				for (i = 0; i < TSPP2_FILTERS_PER_BATCH; i++) {
					if (batch->hw_filters[i] == 0) {
						src->reserved_filter_hw_index =
							(batch->batch_id *
							TSPP2_FILTERS_PER_BATCH)
							+ i;

						filter->batch->src = NULL;
						list_del(&filter->batch->link);
						src->num_associated_batches--;
						tspp2_src_batch_set(src,
							filter->batch->batch_id,
							0);
						goto filter_clear;
					}
				}
			}
		}
	} else {
		/* Case #4: whenever we remove a filter, there is always a
		 * chance that the reserved filter was the only filter used on a
		 * different batch. So now this is a good opportunity to check
		 * if we can release that batch and use the index of the filter
		 * we're freeing instead.
		 */
		list_for_each_entry_safe(batch, tmp_batch,
				&src->batches_list, link) {
			if (((src->reserved_filter_hw_index >=
				(batch->batch_id * TSPP2_FILTERS_PER_BATCH)) &&
				(src->reserved_filter_hw_index <
				(batch->batch_id * TSPP2_FILTERS_PER_BATCH +
				TSPP2_FILTERS_PER_BATCH))) &&
				!tspp2_hw_filters_in_batch(batch)) {
				src->reserved_filter_hw_index =
					filter->hw_index;
				batch->src = NULL;
				list_del(&batch->link);
				src->num_associated_batches--;
				tspp2_src_batch_set(src, batch->batch_id, 0);
				break;
			}
		}
	}

filter_clear:
	tspp2_filter_sw_reset(filter);

	mutex_unlock(&device->mutex);

	pm_runtime_mark_last_busy(device->dev);
	pm_runtime_put_autosuspend(device->dev);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_filter_close);

/**
 * tspp2_filter_enable() - Enable a filter.
 *
 * @filter_handle:	Filter to enable.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_filter_enable(u32 filter_handle)
{
	struct tspp2_filter *filter = (struct tspp2_filter *)filter_handle;
	u32 reg;
	int ret;

	if (!filter) {
		pr_err("%s: Invalid filter handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(filter->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&filter->device->mutex)) {
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -ERESTARTSYS;
	}

	if (!filter->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EPERM;
	}

	if (!filter->opened) {
		pr_err("%s: Filter not opened\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EINVAL;
	}

	if (filter->enabled) {
		pr_warn("%s: Filter already enabled\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return 0;
	}

	reg = readl_relaxed(filter->device->base +
		TSPP2_FILTER_ENTRY0(filter->hw_index));
	reg |= (0x1 << FILTER_ENTRY0_EN_OFFS);
	writel_relaxed(reg, filter->device->base +
		TSPP2_FILTER_ENTRY0(filter->hw_index));

	filter->enabled = 1;

	mutex_unlock(&filter->device->mutex);

	pm_runtime_mark_last_busy(filter->device->dev);
	pm_runtime_put_autosuspend(filter->device->dev);

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_filter_enable);

/**
 * tspp2_filter_disable() - Disable a filter.
 *
 * @filter_handle:	Filter to disable.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_filter_disable(u32 filter_handle)
{
	struct tspp2_filter *filter = (struct tspp2_filter *)filter_handle;
	u32 reg;
	int ret;

	if (!filter) {
		pr_err("%s: Invalid filter handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(filter->device->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&filter->device->mutex);

	if (!filter->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EPERM;
	}

	if (!filter->opened) {
		pr_err("%s: Filter not opened\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EINVAL;
	}

	if (!filter->enabled) {
		pr_warn("%s: Filter already disabled\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return 0;
	}

	reg = readl_relaxed(filter->device->base +
		TSPP2_FILTER_ENTRY0(filter->hw_index));
	reg &= ~(0x1 << FILTER_ENTRY0_EN_OFFS);
	writel_relaxed(reg, filter->device->base +
		TSPP2_FILTER_ENTRY0(filter->hw_index));

	/*
	 * HW requires we wait for up to 2ms here before closing the pipes
	 * used by this filter
	 */
	udelay(TSPP2_HW_DELAY_USEC);

	filter->enabled = 0;

	mutex_unlock(&filter->device->mutex);

	pm_runtime_mark_last_busy(filter->device->dev);
	pm_runtime_put_autosuspend(filter->device->dev);

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_filter_disable);

/**
 * tspp2_pes_analysis_op_write() - Write a PES Analysis operation.
 *
 * @filter:	The filter to set the operation to.
 * @op:		The operation.
 * @op_index:	The operation's index in this filter.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_pes_analysis_op_write(struct tspp2_filter *filter,
				const struct tspp2_operation *op,
				u8 op_index)
{
	u32 reg = 0;

	if (filter->mask != TSPP2_UNIQUE_PID_MASK) {
		pr_err(
			"%s: A filter with a PES Analysis operation must handle a unique PID\n",
			__func__);
		return -EINVAL;
	}

	/*
	 * Bits[19:6] = 0, Bit[5] = Source,
	 * Bit[4] = Skip, Bits[3:0] = Opcode
	 */
	reg |= TSPP2_OPCODE_PES_ANALYSIS;
	if (op->params.pes_analysis.skip_ts_errs)
		reg |= (0x1 << 4);

	if (op->params.pes_analysis.input == TSPP2_OP_BUFFER_B)
		reg |= (0x1 << 5);

	filter->pes_analysis_op_set = 1;

	writel_relaxed(reg, filter->device->base +
				TSPP2_OPCODE(filter->hw_index, op_index));

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}

/**
 * tspp2_raw_tx_op_write() - Write a RAW Transmit operation.
 *
 * @filter:	The filter to set the operation to.
 * @op:		The operation.
 * @op_index:	The operation's index in this filter.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_raw_tx_op_write(struct tspp2_filter *filter,
				const struct tspp2_operation *op,
				u8 op_index)
{
	u32 reg = 0;
	int timestamp = 0;
	struct tspp2_pipe *pipe = (struct tspp2_pipe *)
			op->params.raw_transmit.output_pipe_handle;

	if (!pipe || !pipe->opened) {
		pr_err("%s: Invalid pipe handle\n", __func__);
		return -EINVAL;
	}

	/*
	 * Bits[19:16] = 0, Bit[15] = Support Indexing,
	 * Bit[14] = Timestamp position,
	 * Bits[13:12] = Timestamp mode,
	 * Bits[11:6] = Output pipe, Bit[5] = Source,
	 * Bit[4] = Skip, Bits[3:0] = Opcode
	 */
	reg |= TSPP2_OPCODE_RAW_TRANSMIT;
	if (op->params.raw_transmit.skip_ts_errs)
		reg |= (0x1 << 4);

	if (op->params.raw_transmit.input == TSPP2_OP_BUFFER_B)
		reg |= (0x1 << 5);

	reg |= ((pipe->hw_index & 0x3F) << 6);

	switch (op->params.raw_transmit.timestamp_mode) {
	case TSPP2_OP_TIMESTAMP_NONE:
		/* nothing to do, keep bits value as 0 */
		break;
	case TSPP2_OP_TIMESTAMP_ZERO:
		reg |= (0x1 << 12);
		timestamp = 1;
		break;
	case TSPP2_OP_TIMESTAMP_STC:
		reg |= (0x2 << 12);
		timestamp = 1;
		break;
	default:
		pr_err("%s: Invalid timestamp mode\n", __func__);
		return -EINVAL;
	}

	if (timestamp && op->params.raw_transmit.timestamp_position ==
					TSPP2_PACKET_FORMAT_188_RAW) {
		pr_err("%s: Invalid timestamp position\n", __func__);
		return -EINVAL;
	}

	if (op->params.raw_transmit.timestamp_position ==
				TSPP2_PACKET_FORMAT_192_TAIL)
		reg |= (0x1 << 14);

	if (op->params.raw_transmit.support_indexing) {
		if (filter->raw_op_with_indexing) {
			pr_err(
				"%s: Only one Raw Transmit operation per filter can support HW indexing\n",
				__func__);
			return -EINVAL;
		}
		filter->raw_op_with_indexing = 1;
		reg |= (0x1 << 15);
	}

	filter->raw_op_set = 1;

	writel_relaxed(reg, filter->device->base +
				TSPP2_OPCODE(filter->hw_index, op_index));

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}

/**
 * tspp2_pes_tx_op_write() - Write a PES Transmit operation.
 *
 * @filter:	The filter to set the operation to.
 * @op:		The operation.
 * @op_index:	The operation's index in this filter.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_pes_tx_op_write(struct tspp2_filter *filter,
				const struct tspp2_operation *op,
				u8 op_index)
{
	u32 reg = 0;
	struct tspp2_pipe *payload_pipe = (struct tspp2_pipe *)
		op->params.pes_transmit.output_pipe_handle;
	struct tspp2_pipe *header_pipe;

	if (!payload_pipe || !payload_pipe->opened) {
		pr_err("%s: Invalid payload pipe handle\n", __func__);
		return -EINVAL;
	}

	if (!filter->pes_analysis_op_set) {
		pr_err(
			"%s: PES Analysys operation must precede any PES Transmit operation\n",
			__func__);
		return -EINVAL;
	}

	/*
	 * Bits[19:18] = 0, Bits[17:12] = PES Header output pipe,
	 * Bits[11:6] = Output pipe, Bit[5] = Source,
	 * Bit[4] = Attach STC and flags,
	 * Bit[3] = Disable TX on PES discontinuity,
	 * Bit[2] = Enable SW indexing, Bit[1] = Mode, Bit[0] = 0
	 */

	if (op->params.pes_transmit.mode == TSPP2_OP_PES_TRANSMIT_FULL) {
		reg |= (0x1 << 1);
	} else {
		/* Separated PES mode requires another pipe */
		header_pipe = (struct tspp2_pipe *)
			op->params.pes_transmit.header_output_pipe_handle;

		if (!header_pipe || !header_pipe->opened) {
			pr_err("%s: Invalid header pipe handle\n", __func__);
			return -EINVAL;
		}

		reg |= ((header_pipe->hw_index & 0x3F) << 12);
	}

	if (op->params.pes_transmit.enable_sw_indexing) {
		if (!filter->raw_op_set) {
			pr_err(
				"%s: PES Transmit operation with SW indexing must be preceded by a Raw Transmit operation\n",
				__func__);
			return -EINVAL;
		}
		reg |= (0x1 << 2);
	}

	if (op->params.pes_transmit.disable_tx_on_pes_discontinuity)
		reg |= (0x1 << 3);

	if (op->params.pes_transmit.attach_stc_flags)
		reg |= (0x1 << 4);

	if (op->params.pes_transmit.input == TSPP2_OP_BUFFER_B)
		reg |= (0x1 << 5);

	reg |= ((payload_pipe->hw_index & 0x3F) << 6);

	filter->pes_tx_op_set = 1;

	writel_relaxed(reg, filter->device->base +
				TSPP2_OPCODE(filter->hw_index, op_index));

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}

/**
 * tspp2_pcr_op_write() - Write a PCR Extraction operation.
 *
 * @filter:	The filter to set the operation to.
 * @op:		The operation.
 * @op_index:	The operation's index in this filter.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_pcr_op_write(struct tspp2_filter *filter,
				const struct tspp2_operation *op,
				u8 op_index)
{
	u32 reg = 0;
	struct tspp2_pipe *pipe = (struct tspp2_pipe *)
			op->params.pcr_extraction.output_pipe_handle;

	if (!pipe || !pipe->opened) {
		pr_err("%s: Invalid pipe handle\n", __func__);
		return -EINVAL;
	}

	if (!op->params.pcr_extraction.extract_pcr &&
		!op->params.pcr_extraction.extract_opcr &&
		!op->params.pcr_extraction.extract_splicing_point &&
		!op->params.pcr_extraction.extract_transport_private_data &&
		!op->params.pcr_extraction.extract_af_extension &&
		!op->params.pcr_extraction.extract_all_af) {
		pr_err("%s: Invalid extraction parameters\n", __func__);
		return -EINVAL;
	}

	/*
	 * Bits[19:18] = 0, Bit[17] = All AF, Bit[16] = AF Extension,
	 * Bit[15] = Transport Priave Data, Bit[14] = Splicing Point,
	 * Bit[13] = OPCR, Bit[12] = PCR, Bits[11:6] = Output pipe,
	 * Bit[5] = Source, Bit[4] = Skip, Bits[3:0] = Opcode
	 */
	reg |= TSPP2_OPCODE_PCR_EXTRACTION;
	if (op->params.pcr_extraction.skip_ts_errs)
		reg |= (0x1 << 4);

	if (op->params.pcr_extraction.input == TSPP2_OP_BUFFER_B)
		reg |= (0x1 << 5);

	reg |= ((pipe->hw_index & 0x3F) << 6);

	if (op->params.pcr_extraction.extract_pcr)
		reg |= (0x1 << 12);

	if (op->params.pcr_extraction.extract_opcr)
		reg |= (0x1 << 13);

	if (op->params.pcr_extraction.extract_splicing_point)
		reg |= (0x1 << 14);

	if (op->params.pcr_extraction.extract_transport_private_data)
		reg |= (0x1 << 15);

	if (op->params.pcr_extraction.extract_af_extension)
		reg |= (0x1 << 16);

	if (op->params.pcr_extraction.extract_all_af)
		reg |= (0x1 << 17);

	writel_relaxed(reg, filter->device->base +
				TSPP2_OPCODE(filter->hw_index, op_index));

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}

/**
 * tspp2_cipher_op_write() - Write a Cipher operation.
 *
 * @filter:	The filter to set the operation to.
 * @op:		The operation.
 * @op_index:	The operation's index in this filter.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_cipher_op_write(struct tspp2_filter *filter,
				const struct tspp2_operation *op,
				u8 op_index)
{
	u32 reg = 0;

	/*
	 * Bits[19:18] = 0, Bits[17:15] = Scrambling related,
	 * Bit[14] = Mode, Bit[13] = Decrypt PES header,
	 * Bits[12:7] = Key ladder index, Bit[6] = Destination,
	 * Bit[5] = Source, Bit[4] = Skip, Bits[3:0] = Opcode
	 */

	reg |= TSPP2_OPCODE_CIPHER;
	if (op->params.cipher.skip_ts_errs)
		reg |= (0x1 << 4);

	if (op->params.cipher.input == TSPP2_OP_BUFFER_B)
		reg |= (0x1 << 5);

	if (op->params.cipher.output == TSPP2_OP_BUFFER_B)
		reg |= (0x1 << 6);

	reg |= ((op->params.cipher.key_ladder_index & 0x3F) << 7);

	if (op->params.cipher.mode == TSPP2_OP_CIPHER_ENCRYPT &&
		op->params.cipher.decrypt_pes_header) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (op->params.cipher.decrypt_pes_header)
		reg |= (0x1 << 13);

	if (op->params.cipher.mode == TSPP2_OP_CIPHER_ENCRYPT)
		reg |= (0x1 << 14);

	switch (op->params.cipher.scrambling_mode) {
	case TSPP2_OP_CIPHER_AS_IS:
		reg |= (0x1 << 15);
		break;
	case TSPP2_OP_CIPHER_SET_SCRAMBLING_0:
		/* nothing to do, keep bits[17:16] as 0 */
		break;
	case TSPP2_OP_CIPHER_SET_SCRAMBLING_1:
		reg |= (0x1 << 16);
		break;
	case TSPP2_OP_CIPHER_SET_SCRAMBLING_2:
		reg |= (0x2 << 16);
		break;
	case TSPP2_OP_CIPHER_SET_SCRAMBLING_3:
		reg |= (0x3 << 16);
		break;
	default:
		pr_err("%s: Invalid scrambling mode\n", __func__);
		return -EINVAL;
	}

	writel_relaxed(reg, filter->device->base +
				TSPP2_OPCODE(filter->hw_index, op_index));

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}

/**
 * tspp2_index_op_write() - Write an Indexing operation.
 *
 * @filter:	The filter to set the operation to.
 * @op:		The operation.
 * @op_index:	The operation's index in this filter.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_index_op_write(struct tspp2_filter *filter,
				const struct tspp2_operation *op,
				u8 op_index)
{
	u32 reg = 0;
	u32 filter_reg = 0;
	struct tspp2_pipe *pipe = (struct tspp2_pipe *)
			op->params.indexing.output_pipe_handle;

	if (!pipe || !pipe->opened) {
		pr_err("%s: Invalid pipe handle\n", __func__);
		return -EINVAL;
	}

	/* Enforce Indexing related HW restrictions */
	if (filter->indexing_op_set) {
		pr_err(
			"%s: Only one indexing operation supported per filter\n",
			__func__);
		return -EINVAL;
	}
	if (!filter->raw_op_with_indexing) {
		pr_err(
			"%s: Raw Transmit operation with indexing support must be configured before the Indexing operation\n",
			__func__);
		return -EINVAL;
	}

	if (!filter->pes_analysis_op_set) {
		pr_err(
			"%s: PES Analysis operation must precede Indexing operation\n",
			__func__);
		return -EINVAL;
	}

	/*
	 * Bits [19:15] = 0, Bit[14] = Index by RAI,
	 * Bits[13:12] = 0,
	 * Bits[11:6] = Output pipe, Bit[5] = Source,
	 * Bit[4] = Skip, Bits[3:0] = Opcode
	 */

	reg |= TSPP2_OPCODE_INDEXING;
	if (op->params.indexing.skip_ts_errs)
		reg |= (0x1 << 4);

	if (op->params.indexing.input == TSPP2_OP_BUFFER_B)
		reg |= (0x1 << 5);

	reg |= ((pipe->hw_index & 0x3F) << 6);

	if (op->params.indexing.random_access_indicator_indexing)
		reg |= (0x1 << 14);

	/* Indexing table ID is set in the filter and not in the operation */
	filter->indexing_table_id = op->params.indexing.indexing_table_id;
	filter_reg = readl_relaxed(filter->device->base +
				TSPP2_FILTER_ENTRY0(filter->hw_index));
	filter_reg &= ~(0x3 << FILTER_ENTRY0_CODEC_OFFS);
	filter_reg |= (filter->indexing_table_id << FILTER_ENTRY0_CODEC_OFFS);
	writel_relaxed(filter_reg, filter->device->base +
			TSPP2_FILTER_ENTRY0(filter->hw_index));

	filter->indexing_op_set = 1;

	writel_relaxed(reg, filter->device->base +
				TSPP2_OPCODE(filter->hw_index, op_index));

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}

/**
 * tspp2_copy_op_write() - Write an Copy operation.
 *
 * @filter:	The filter to set the operation to.
 * @op:		The operation.
 * @op_index:	The operation's index in this filter.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_copy_op_write(struct tspp2_filter *filter,
				const struct tspp2_operation *op,
				u8 op_index)
{
	u32 reg = 0;

	/* Bits[19:6] = 0, Bit[5] = Source, Bit[4] = 0, Bits[3:0] = Opcode */
	reg |= TSPP2_OPCODE_COPY_PACKET;
	if (op->params.copy_packet.input == TSPP2_OP_BUFFER_B)
		reg |= (0x1 << 5);

	writel_relaxed(reg, filter->device->base +
				TSPP2_OPCODE(filter->hw_index, op_index));

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}

/**
 * tspp2_op_write() - Write an operation of any type.
 *
 * @filter:	The filter to set the operation to.
 * @op:		The operation.
 * @op_index:	The operation's index in this filter.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_op_write(struct tspp2_filter *filter,
				const struct tspp2_operation *op,
				u8 op_index)
{
	switch (op->type) {
	case TSPP2_OP_PES_ANALYSIS:
		return tspp2_pes_analysis_op_write(filter, op, op_index);
	case TSPP2_OP_RAW_TRANSMIT:
		return tspp2_raw_tx_op_write(filter, op, op_index);
	case TSPP2_OP_PES_TRANSMIT:
		return tspp2_pes_tx_op_write(filter, op, op_index);
	case TSPP2_OP_PCR_EXTRACTION:
		return tspp2_pcr_op_write(filter, op, op_index);
	case TSPP2_OP_CIPHER:
		return tspp2_cipher_op_write(filter, op, op_index);
	case TSPP2_OP_INDEXING:
		return tspp2_index_op_write(filter, op, op_index);
	case TSPP2_OP_COPY_PACKET:
		return tspp2_copy_op_write(filter, op, op_index);
	default:
		pr_warn("%s: Unknown operation type\n", __func__);
		return -EINVAL;
	}
}

/**
 * tspp2_filter_ops_add() - Set the operations of a disabled filter.
 *
 * @filter:	The filter to work on.
 * @op:		The new operations array.
 * @op_index:	The number of operations in the array.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_filter_ops_add(struct tspp2_filter *filter,
				const struct tspp2_operation *ops,
				u8 operations_num)
{
	int i;
	int ret = 0;

	/* User parameter validity checks were already performed */

	/*
	 * We want to start with a clean slate here. The user may call us to
	 * set operations several times, so need to make sure only the last call
	 * counts.
	 */
	tspp2_filter_ops_clear(filter);

	/* Save user operations in filter's database */
	for (i = 0; i < operations_num; i++)
		filter->operations[i] = ops[i];

	/* Write user operations to HW */
	for (i = 0; i < operations_num; i++) {
		ret = tspp2_op_write(filter, &ops[i], i);
		if (ret)
			goto ops_cleanup;
	}

	/*
	 * Here we want to add the Exit operation implicitly if required, that
	 * is, if the user provided less than TSPP2_MAX_OPS_PER_FILTER
	 * operations. However, we already called tspp2_filter_ops_clear()
	 * which set all the operations in HW to Exit, before writing the
	 * actual user operations. So, no need to do it again here.
	 * Also, if someone calls this function with operations_num == 0,
	 * it is similar to calling tspp2_filter_operations_clear().
	 */

	filter->num_user_operations = operations_num;

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;

ops_cleanup:
	pr_err("%s: Failed to set operations to filter, clearing all\n",
		__func__);

	tspp2_filter_ops_clear(filter);

	return ret;
}

/**
 * tspp2_filter_ops_update() - Update the operations of an enabled filter.
 *
 * This function updates the operations of an enabled filter. In fact, it is
 * not possible to update an existing filter without disabling it, clearing
 * the existing operations and setting new ones. However, if we do that,
 * we'll miss TS packets and not handle the stream properly, so a smooth
 * transition is required.
 * The algorithm is as follows:
 * 1. Find a free temporary filter object.
 * 2. Set the new filter's HW index to the reserved HW index.
 * 3. Set the operations to the new filter. This sets the operations to
 * the correct HW registers, based on the new HW index, and also updates
 * the relevant information in the temporary filter object. Later we copy this
 * to the actual filter object.
 * 4. Use the same context as the old filter (to maintain HW state).
 * 5. Reset parts of the context if needed.
 * 6. Enable the new HW filter, then disable the old filter.
 * 7. Update the source's reserved filter HW index.
 * 8. Update the filter's batch, HW index and operations-related information.
 *
 * @filter:	The filter to work on.
 * @op:		The new operations array.
 * @op_index:	The number of operations in the array.
 *
 * Return 0 on success, error value otherwise.
 */
static int tspp2_filter_ops_update(struct tspp2_filter *filter,
				const struct tspp2_operation *ops,
				u8 operations_num)
{
	int i;
	int ret = 0;
	int found = 0;
	u32 reg = 0;
	u16 hw_idx;
	struct tspp2_filter_batch *batch;
	struct tspp2_filter *tmp_filter = NULL;
	struct tspp2_src *src = filter->src;

	/*
	 * Find an available temporary filter object in the device's
	 * filters database.
	 */
	for (i = 0; i < TSPP2_NUM_AVAIL_FILTERS; i++)
		if (!src->device->filters[i].opened)
			break;
	if (i == TSPP2_NUM_AVAIL_FILTERS) {
		/* Should never happen */
		pr_err("%s: No available filters\n", __func__);
		return -ENOMEM;
	}
	tmp_filter = &src->device->filters[i];

	/*
	 * Set new filter operations. We do this relatively early
	 * in the function to avoid cleanup operations if this fails.
	 * Since this also writes to HW, we have to set the correct HW index.
	 */
	tmp_filter->hw_index = src->reserved_filter_hw_index;
	/*
	 * Need to set the mask properly to indicate if the filter handles
	 * a unique PID.
	 */
	tmp_filter->mask = filter->mask;
	ret = tspp2_filter_ops_add(tmp_filter, ops, operations_num);
	if (ret) {
		tmp_filter->hw_index = 0;
		tmp_filter->mask = 0;
		return ret;
	}

	/*
	 * Mark new filter (in fact, the new filter HW index) as used in the
	 * appropriate batch. The batch has to be one of the batches already
	 * associated with the source.
	 */
	list_for_each_entry(batch, &src->batches_list, link) {
		for (i = 0; i < TSPP2_FILTERS_PER_BATCH; i++) {
			hw_idx = (batch->batch_id *
					TSPP2_FILTERS_PER_BATCH) + i;
			if (hw_idx == tmp_filter->hw_index) {
				batch->hw_filters[i] = 1;
				found = 1;
				break;
			}
		}
		if (found)
			break;
	}

	if (!found) {
		pr_err("%s: Could not find matching batch\n", __func__);
		tspp2_filter_ops_clear(tmp_filter);
		tmp_filter->hw_index = 0;
		return -EINVAL;
	}

	/* Set the same context of the old filter to the new HW filter */
	writel_relaxed((filter->context << FILTER_ENTRY1_CONTEXT_OFFS),
		filter->device->base +
		TSPP2_FILTER_ENTRY1(tmp_filter->hw_index));

	/*
	 * Reset partial context, if necessary. We want to reset a partial
	 * context before we start using it, so if there's a new operation
	 * that uses a context where before there was no operation that used it,
	 * we reset that context. We need to do this before we start using the
	 * new operation, so before we enable the new filter.
	 * Note: there is no need to reset most of the filter's context-based
	 * counters, because the filter keeps using the same context. The
	 * exception is the PES error counters that we may want to reset when
	 * resetting the entire PES context.
	 */
	if (!filter->pes_tx_op_set && tmp_filter->pes_tx_op_set) {
		/* PES Tx operation added */
		writel_relaxed(
			(0x1 << TSPP2_MODULUS_OP(filter->context, 32)),
			filter->device->base +
			TSPP2_PES_CONTEXT_RESET(filter->context >> 5));
		writel_relaxed(0, filter->device->base +
			TSPP2_FILTER_PES_ERRORS(filter->context));
	}

	if (!filter->indexing_op_set && tmp_filter->indexing_op_set) {
		/* Indexing operation added */
		writel_relaxed(
			(0x1 << TSPP2_MODULUS_OP(filter->context, 32)),
			filter->device->base +
			TSPP2_INDEXING_CONTEXT_RESET(filter->context >> 5));
	}

	/*
	 * Write PID and mask to new filter HW registers and enable it.
	 * Preserve filter indexing table ID.
	 */
	reg |= (0x1 << FILTER_ENTRY0_EN_OFFS);
	reg |= ((filter->pid_value << FILTER_ENTRY0_PID_OFFS) |
		(filter->mask << FILTER_ENTRY0_MASK_OFFS));
	reg |= (tmp_filter->indexing_table_id << FILTER_ENTRY0_CODEC_OFFS);
	writel_relaxed(reg, filter->device->base +
		TSPP2_FILTER_ENTRY0(tmp_filter->hw_index));

	/* Disable old HW filter */
	writel_relaxed(0, filter->device->base +
		TSPP2_FILTER_ENTRY0(filter->hw_index));

	/*
	 * HW requires we wait for up to 2ms here before removing the
	 * operations used by this filter.
	 */
	udelay(TSPP2_HW_DELAY_USEC);

	tspp2_filter_ops_clear(filter);

	writel_relaxed(0, filter->device->base +
		TSPP2_FILTER_ENTRY1(filter->hw_index));

	/* Mark HW filter as unused in old batch */
	filter->batch->hw_filters[(filter->hw_index -
		(filter->batch->batch_id * TSPP2_FILTERS_PER_BATCH))] = 0;

	/* The new HW filter may be in a new batch, so we need to update */
	filter->batch = batch;

	/*
	 * Update source's reserved filter HW index, and also update the
	 * new HW index in the filter object.
	 */
	src->reserved_filter_hw_index = filter->hw_index;
	filter->hw_index = tmp_filter->hw_index;

	/*
	 * We've already set the new operations to HW, but we want to
	 * update the filter object, too. tmp_filter contains all the
	 * operations' related information we need (operations and flags).
	 * Also, we make sure to update indexing_table_id based on the new
	 * indexing operations.
	 */
	memcpy(filter->operations, tmp_filter->operations,
		(sizeof(struct tspp2_operation) * TSPP2_MAX_OPS_PER_FILTER));
	filter->num_user_operations = tmp_filter->num_user_operations;
	filter->indexing_op_set = tmp_filter->indexing_op_set;
	filter->raw_op_with_indexing = tmp_filter->raw_op_with_indexing;
	filter->pes_analysis_op_set = tmp_filter->pes_analysis_op_set;
	filter->raw_op_set = tmp_filter->raw_op_set;
	filter->pes_tx_op_set = tmp_filter->pes_tx_op_set;
	filter->indexing_table_id = tmp_filter->indexing_table_id;

	/*
	 * Now we can clean tmp_filter. This is really just to keep the filter
	 * object clean. However, we don't want to use tspp2_filter_ops_clear()
	 * because it clears the operations from HW too.
	 */
	memset(tmp_filter->operations, 0,
		(sizeof(struct tspp2_operation) * TSPP2_MAX_OPS_PER_FILTER));
	tmp_filter->num_user_operations = 0;
	tmp_filter->indexing_op_set = 0;
	tmp_filter->raw_op_with_indexing = 0;
	tmp_filter->pes_analysis_op_set = 0;
	tmp_filter->raw_op_set = 0;
	tmp_filter->pes_tx_op_set = 0;
	tmp_filter->indexing_table_id = 0;
	tmp_filter->hw_index = 0;

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}

/**
 * tspp2_filter_operations_set() - Set operations to a filter.
 *
 * @filter_handle:	Filter to set operations to.
 * @ops:		An array of up to TSPP2_MAX_OPS_PER_FILTER
 *			operations.
 * @operations_num:	Number of operations in the ops array.
 *
 * This function sets the required operations to a given filter. The filter
 * can either be disabled (in which case it may or may not already have some
 * operations set), or enabled (in which case it certainly has some oprations
 * set). In any case, the filter's previous operations are cleared, and the new
 * operations provided are set.
 *
 * In addition to some trivial parameter validity checks, the following
 * restrictions are enforced:
 * 1. A filter with a PES Analysis operation must handle a unique PID (i.e.,
 *    should have a mask that equals TSPP2_UNIQUE_PID_MASK).
 * 2. Only a single Raw Transmit operation per filter can support HW indexing
 *    (i.e., can have its support_indexing configuration parameter set).
 * 3. A PES Analysys operation must precede any PES Transmit operation.
 * 4. A PES Transmit operation with SW indexing (i.e., with its
 *    enable_sw_indexing parameter set) must be preceded by a Raw Transmit
 *    operation.
 * 5. Only a single indexing operation is supported per filter.
 * 6. A Raw Transmit operation with indexing support must be configured before
 *    the Indexing operation.
 * 7. A PES Analysis operation must precede the Indexing operation.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_filter_operations_set(u32 filter_handle,
			const struct tspp2_operation *ops,
			u8 operations_num)
{
	struct tspp2_filter *filter = (struct tspp2_filter *)filter_handle;
	int ret = 0;

	if (!filter) {
		pr_err("%s: Invalid filter handle\n", __func__);
		return -EINVAL;
	}
	if (!ops || operations_num > TSPP2_MAX_OPS_PER_FILTER ||
			operations_num == 0) {
		pr_err("%s: Invalid ops parameter\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(filter->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&filter->device->mutex)) {
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -ERESTARTSYS;
	}

	if (!filter->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EPERM;
	}

	if (!filter->opened) {
		pr_err("%s: Filter not opened\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EINVAL;
	}

	if (filter->enabled)
		ret = tspp2_filter_ops_update(filter, ops, operations_num);
	else
		ret = tspp2_filter_ops_add(filter, ops, operations_num);

	mutex_unlock(&filter->device->mutex);

	pm_runtime_mark_last_busy(filter->device->dev);
	pm_runtime_put_autosuspend(filter->device->dev);

	return ret;
}
EXPORT_SYMBOL(tspp2_filter_operations_set);

/**
 * tspp2_filter_operations_clear() - Clear all operations from a filter.
 *
 * @filter_handle:	Filter to clear all operations from.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_filter_operations_clear(u32 filter_handle)
{
	int ret;
	struct tspp2_filter *filter = (struct tspp2_filter *)filter_handle;

	if (!filter) {
		pr_err("%s: Invalid filter handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(filter->device->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&filter->device->mutex);

	if (!filter->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EPERM;
	}

	if (!filter->opened) {
		pr_err("%s: Filter not opened\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EINVAL;
	}

	if (filter->num_user_operations == 0) {
		pr_warn("%s: No operations to clear from filter\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return 0;
	}

	tspp2_filter_ops_clear(filter);

	mutex_unlock(&filter->device->mutex);

	pm_runtime_mark_last_busy(filter->device->dev);
	pm_runtime_put_autosuspend(filter->device->dev);

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_filter_operations_clear);

/**
 * tspp2_filter_current_scrambling_bits_get() - Get the current scrambling bits.
 *
 * @filter_handle:		Filter to get the scrambling bits from.
 * @scrambling_bits_value:	The current value of the scrambling bits.
 *				This could be the value from the TS packet
 *				header, the value from the PES header, or a
 *				logical OR operation of both values, depending
 *				on the scrambling_bits_monitoring configuration
 *				of the source this filter belongs to.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_filter_current_scrambling_bits_get(u32 filter_handle,
			u8 *scrambling_bits_value)
{
	struct tspp2_filter *filter = (struct tspp2_filter *)filter_handle;
	u32 reg;
	u32 ts_bits;
	u32 pes_bits;
	int ret;

	if (!filter) {
		pr_err("%s: Invalid filter handle\n", __func__);
		return -EINVAL;
	}
	if (scrambling_bits_value == NULL) {
		pr_err("%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(filter->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&filter->device->mutex)) {
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -ERESTARTSYS;
	}

	if (!filter->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EPERM;
	}

	if (!filter->opened) {
		pr_err("%s: Filter not opened\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EINVAL;
	}

	reg = readl_relaxed(filter->device->base +
			TSPP2_TSP_CONTEXT(filter->context));

	ts_bits = ((reg >> TSP_CONTEXT_TS_HEADER_SC_OFFS) & 0x3);
	pes_bits = ((reg >> TSP_CONTEXT_PES_HEADER_SC_OFFS) & 0x3);

	switch (filter->src->scrambling_bits_monitoring) {
	case TSPP2_SRC_SCRAMBLING_MONITOR_PES_ONLY:
		*scrambling_bits_value = pes_bits;
		break;
	case TSPP2_SRC_SCRAMBLING_MONITOR_TS_ONLY:
		*scrambling_bits_value = ts_bits;
		break;
	case TSPP2_SRC_SCRAMBLING_MONITOR_PES_AND_TS:
		*scrambling_bits_value = (pes_bits | ts_bits);
		break;
	case TSPP2_SRC_SCRAMBLING_MONITOR_NONE:
		/* fall through to default case */
	default:
		pr_err("%s: Invalid scrambling bits mode\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EINVAL;
	}

	mutex_unlock(&filter->device->mutex);

	pm_runtime_mark_last_busy(filter->device->dev);
	pm_runtime_put_autosuspend(filter->device->dev);

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_filter_current_scrambling_bits_get);

/* Data-path API functions */

/**
 * tspp2_pipe_descriptor_get() - Get a data descriptor from a pipe.
 *
 * @pipe_handle:	Pipe to get the descriptor from.
 * @desc:		Received pipe data descriptor.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_pipe_descriptor_get(u32 pipe_handle, struct sps_iovec *desc)
{
	int ret;
	struct tspp2_pipe *pipe = (struct tspp2_pipe *)pipe_handle;

	if (!pipe) {
		pr_err("%s: Invalid pipe handle\n", __func__);
		return -EINVAL;
	}
	if (!desc) {
		pr_err("%s: Invalid descriptor pointer\n", __func__);
		return -EINVAL;
	}

	/* Descriptor pointer validity is checked inside the SPS driver. */

	ret = pm_runtime_get_sync(pipe->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&pipe->device->mutex)) {
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -ERESTARTSYS;
	}

	if (!pipe->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&pipe->device->mutex);
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -EPERM;
	}

	if (!pipe->opened) {
		pr_err("%s: Pipe not opened\n", __func__);
		mutex_unlock(&pipe->device->mutex);
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -EINVAL;
	}

	ret = sps_get_iovec(pipe->sps_pipe, desc);

	mutex_unlock(&pipe->device->mutex);

	pm_runtime_mark_last_busy(pipe->device->dev);
	pm_runtime_put_autosuspend(pipe->device->dev);

	dev_dbg(pipe->device->dev, "%s: successful\n", __func__);

	return ret;

}
EXPORT_SYMBOL(tspp2_pipe_descriptor_get);

/**
 * tspp2_pipe_descriptor_put() - Release a descriptor for reuse by the pipe.
 *
 * @pipe_handle:	Pipe to release the descriptor to.
 * @addr:		Address to release for reuse.
 * @size:		Size to release.
 * @flags:		Descriptor flags.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_pipe_descriptor_put(u32 pipe_handle, u32 addr, u32 size, u32 flags)
{
	int ret;
	struct tspp2_pipe *pipe = (struct tspp2_pipe *)pipe_handle;

	if (!pipe) {
		pr_err("%s: Invalid pipe handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(pipe->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&pipe->device->mutex)) {
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -ERESTARTSYS;
	}

	if (!pipe->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&pipe->device->mutex);
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -EPERM;
	}

	if (!pipe->opened) {
		pr_err("%s: Pipe not opened\n", __func__);
		mutex_unlock(&pipe->device->mutex);
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -EINVAL;
	}

	ret = sps_transfer_one(pipe->sps_pipe, addr, size, NULL, flags);

	mutex_unlock(&pipe->device->mutex);

	pm_runtime_mark_last_busy(pipe->device->dev);
	pm_runtime_put_autosuspend(pipe->device->dev);

	dev_dbg(pipe->device->dev, "%s: successful\n", __func__);

	return ret;
}
EXPORT_SYMBOL(tspp2_pipe_descriptor_put);

/**
 * tspp2_pipe_last_address_used_get() - Get the last address the TSPP2 used.
 *
 * @pipe_handle:	Pipe to get the address from.
 * @address:		The last (virtual) address TSPP2 wrote data to.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_pipe_last_address_used_get(u32 pipe_handle, u32 *address)
{
	int ret;
	struct tspp2_pipe *pipe = (struct tspp2_pipe *)pipe_handle;

	if (!pipe) {
		pr_err("%s: Invalid pipe handle\n", __func__);
		return -EINVAL;
	}
	if (!address) {
		pr_err("%s: Invalid address pointer\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(pipe->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&pipe->device->mutex)) {
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -ERESTARTSYS;
	}

	if (!pipe->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&pipe->device->mutex);
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -EPERM;
	}

	if (!pipe->opened) {
		pr_err("%s: Pipe not opened\n", __func__);
		mutex_unlock(&pipe->device->mutex);
		pm_runtime_mark_last_busy(pipe->device->dev);
		pm_runtime_put_autosuspend(pipe->device->dev);
		return -EINVAL;
	}

	*address = readl_relaxed(pipe->device->base +
		TSPP2_PIPE_LAST_ADDRESS(pipe->hw_index));

	mutex_unlock(&pipe->device->mutex);

	pm_runtime_mark_last_busy(pipe->device->dev);
	pm_runtime_put_autosuspend(pipe->device->dev);

	*address = be32_to_cpu(*address);

	dev_dbg(pipe->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_pipe_last_address_used_get);

/**
 * tspp2_data_write() - Write (feed) data to a source.
 *
 * @src_handle:	Source to feed data to.
 * @offset:	Offset in the source's input pipe buffer.
 * @size:	Size of data to write, in bytes.
 *
 * Schedule BAM transfers to feed data from the source's input pipe
 * to TSPP2 for processing. Note that the user is responsible for opening
 * an input pipe with the appropriate configuration parameters, and attaching
 * this pipe as an input pipe to the source. Pipe configuration validity is not
 * verified by this function.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_data_write(u32 src_handle, u32 offset, u32 size)
{
	int ret;
	u32 desc_length;
	u32 desc_flags;
	u32 data_length = size;
	u32 data_offset = offset;
	struct tspp2_pipe *pipe;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		goto err_inval;
	}

	if (!src->enabled) {
		pr_err("%s: Source not enabled\n", __func__);
		goto err_inval;
	}

	if ((src->input != TSPP2_INPUT_MEMORY) || !src->input_pipe) {
		pr_err("%s: Invalid source input or no input pipe\n", __func__);
		goto err_inval;
	}

	pipe = src->input_pipe;

	if (offset + size > pipe->cfg.buffer_size) {
		pr_err("%s: offset + size > buffer size\n", __func__);
		goto err_inval;
	}

	while (data_length) {
		if (data_length > pipe->cfg.sps_cfg.descriptor_size) {
			desc_length = pipe->cfg.sps_cfg.descriptor_size;
			desc_flags = 0;
		} else {
			/* last descriptor */
			desc_length = data_length;
			desc_flags = SPS_IOVEC_FLAG_EOT;
		}

		ret = sps_transfer_one(pipe->sps_pipe,
				pipe->iova + data_offset,
				desc_length,
				pipe->cfg.sps_cfg.user_info,
				desc_flags);

		if (ret) {
			pr_err("%s: sps_transfer_one failed, %d\n",
				__func__, ret);
			mutex_unlock(&src->device->mutex);
			pm_runtime_mark_last_busy(src->device->dev);
			pm_runtime_put_autosuspend(src->device->dev);
			return ret;
		}

		data_offset += desc_length;
		data_length -= desc_length;
	}

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;

err_inval:
	mutex_unlock(&src->device->mutex);
	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	return -EINVAL;
}
EXPORT_SYMBOL(tspp2_data_write);

/**
 * tspp2_tsif_data_write() - Write (feed) data to a TSIF source via Loopback.
 *
 * @src_handle:	Source to feed data to.
 * @data:	data buffer containing one TS packet of size 188 Bytes.
 *
 * Write one TS packet of size 188 bytes to the TSIF loopback interface.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_tsif_data_write(u32 src_handle, u32 *data)
{
	int i;
	int ret;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;
	struct tspp2_tsif_device *tsif_device;
	const unsigned int loopback_flags[3] = {0x01000000, 0, 0x02000000};

	if (data == NULL) {
		pr_err("%s: NULL data\n", __func__);
		return -EINVAL;
	}

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		goto err_inval;
	}

	if (!src->enabled) {
		pr_err("%s: Source not enabled\n", __func__);
		goto err_inval;
	}

	if ((src->input != TSPP2_INPUT_TSIF0)
		&& (src->input != TSPP2_INPUT_TSIF1)) {
		pr_err("%s: Invalid source input\n", __func__);
		goto err_inval;
	}

	tsif_device = &src->device->tsif_devices[src->input];

	/* lpbk_flags : start && !last */
	writel_relaxed(loopback_flags[0],
		tsif_device->base + TSPP2_TSIF_LPBK_FLAGS);

	/* 1-st dword of data */
	writel_relaxed(data[0],
		tsif_device->base + TSPP2_TSIF_LPBK_DATA);

	/* Clear start bit */
	writel_relaxed(loopback_flags[1],
		tsif_device->base + TSPP2_TSIF_LPBK_FLAGS);

	/* 45 more dwords */
	for (i = 1; i < 46; i++)
		writel_relaxed(data[i],
			tsif_device->base + TSPP2_TSIF_LPBK_DATA);

	/* Set last bit */
	writel_relaxed(loopback_flags[2],
		tsif_device->base + TSPP2_TSIF_LPBK_FLAGS);

	/* Last data dword */
	writel_relaxed(data[46], tsif_device->base + TSPP2_TSIF_LPBK_DATA);

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;

err_inval:
	mutex_unlock(&src->device->mutex);
	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	return -EINVAL;
}
EXPORT_SYMBOL(tspp2_tsif_data_write);

/* Event notification API functions */

/**
 * tspp2_global_event_notification_register() - Get notified on a global event.
 *
 * @dev_id:			TSPP2 device ID.
 * @global_event_bitmask:	A bitmask of global events,
 *				TSPP2_GLOBAL_EVENT_XXX.
 * @callback:			User callback function.
 * @cookie:			User information passed to the callback.
 *
 * Register a user callback which will be invoked when certain global
 * events occur. Note the values (mask, callback and cookie) are overwritten
 * when calling this function multiple times. Therefore it is possible to
 * "unregister" a callback by calling this function with the bitmask set to 0
 * and with NULL callback and cookie.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_global_event_notification_register(u32 dev_id,
			u32 global_event_bitmask,
			void (*callback)(void *cookie, u32 event_bitmask),
			void *cookie)
{
	struct tspp2_device *device;
	unsigned long flags;
	u32 reg = 0;

	if (dev_id >= TSPP2_NUM_DEVICES) {
		pr_err("%s: Invalid device ID %d\n", __func__, dev_id);
		return -ENODEV;
	}

	device = tspp2_devices[dev_id];
	if (!device) {
		pr_err("%s: Invalid device\n", __func__);
		return -ENODEV;
	}

	if (mutex_lock_interruptible(&device->mutex))
		return -ERESTARTSYS;

	if (!device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&device->mutex);
		return -EPERM;
	}

	/*
	 * Some of the interrupts that are generated when these events occur
	 * may be disabled due to module parameters. So we make sure to enable
	 * them here, depending on which event was requested. If some events
	 * were requested before and now this function is called again with
	 * other events, though, we want to restore the interrupt configuration
	 * to the default state according to the module parameters.
	 */
	reg = readl_relaxed(device->base + TSPP2_GLOBAL_IRQ_ENABLE);
	if (global_event_bitmask & TSPP2_GLOBAL_EVENT_INVALID_AF_CTRL) {
		reg |= (0x1 << GLOBAL_IRQ_TSP_INVALID_AF_OFFS);
	} else {
		if (tspp2_en_invalid_af_ctrl)
			reg |= (0x1 << GLOBAL_IRQ_TSP_INVALID_AF_OFFS);
		else
			reg &= ~(0x1 << GLOBAL_IRQ_TSP_INVALID_AF_OFFS);
	}

	if (global_event_bitmask & TSPP2_GLOBAL_EVENT_INVALID_AF_LENGTH) {
		reg |= (0x1 << GLOBAL_IRQ_TSP_INVALID_LEN_OFFS);
	} else {
		if (tspp2_en_invalid_af_length)
			reg |= (0x1 << GLOBAL_IRQ_TSP_INVALID_LEN_OFFS);
		else
			reg &= ~(0x1 << GLOBAL_IRQ_TSP_INVALID_LEN_OFFS);
	}

	if (global_event_bitmask & TSPP2_GLOBAL_EVENT_PES_NO_SYNC) {
		reg |= (0x1 << GLOBAL_IRQ_PES_NO_SYNC_OFFS);
	} else {
		if (tspp2_en_pes_no_sync)
			reg |= (0x1 << GLOBAL_IRQ_PES_NO_SYNC_OFFS);
		else
			reg &= ~(0x1 << GLOBAL_IRQ_PES_NO_SYNC_OFFS);
	}

	writel_relaxed(reg, device->base + TSPP2_GLOBAL_IRQ_ENABLE);

	spin_lock_irqsave(&device->spinlock, flags);
	device->event_callback = callback;
	device->event_cookie = cookie;
	device->event_bitmask = global_event_bitmask;
	spin_unlock_irqrestore(&device->spinlock, flags);

	mutex_unlock(&device->mutex);

	dev_dbg(device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_global_event_notification_register);

/**
 * tspp2_src_event_notification_register() - Get notified on a source event.
 *
 * @src_handle:		Source handle.
 * @src_event_bitmask:	A bitmask of source events,
 *			TSPP2_SRC_EVENT_XXX.
 * @callback:		User callback function.
 * @cookie:		User information passed to the callback.
 *
 * Register a user callback which will be invoked when certain source
 * events occur. Note the values (mask, callback and cookie) are overwritten
 * when calling this function multiple times. Therefore it is possible to
 * "unregister" a callback by calling this function with the bitmask set to 0
 * and with NULL callback and cookie.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_src_event_notification_register(u32 src_handle,
			u32 src_event_bitmask,
			void (*callback)(void *cookie, u32 event_bitmask),
			void *cookie)
{
	int ret;
	u32 reg;
	unsigned long flags;
	struct tspp2_src *src = (struct tspp2_src *)src_handle;

	if (!src) {
		pr_err("%s: Invalid source handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(src->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&src->device->mutex)) {
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -ERESTARTSYS;
	}

	if (!src->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&src->device->mutex);
		pm_runtime_mark_last_busy(src->device->dev);
		pm_runtime_put_autosuspend(src->device->dev);
		return -EPERM;
	}

	if (!src->opened) {
		pr_err("%s: Source not opened\n", __func__);
		goto err_inval;
	}

	if (((src->input == TSPP2_INPUT_TSIF0) ||
		(src->input == TSPP2_INPUT_TSIF1)) &&
		((src_event_bitmask & TSPP2_SRC_EVENT_MEMORY_READ_ERROR) ||
		(src_event_bitmask & TSPP2_SRC_EVENT_FLOW_CTRL_STALL))) {
		pr_err("%s: Invalid event bitmask for a source with TSIF input\n",
				__func__);
		goto err_inval;
	}

	if ((src->input == TSPP2_INPUT_MEMORY) &&
		((src_event_bitmask & TSPP2_SRC_EVENT_TSIF_LOST_SYNC) ||
		(src_event_bitmask & TSPP2_SRC_EVENT_TSIF_TIMEOUT) ||
		(src_event_bitmask & TSPP2_SRC_EVENT_TSIF_OVERFLOW) ||
		(src_event_bitmask & TSPP2_SRC_EVENT_TSIF_PKT_READ_ERROR) ||
		(src_event_bitmask & TSPP2_SRC_EVENT_TSIF_PKT_WRITE_ERROR))) {
		pr_err("%s: Invalid event bitmask for a source with memory input\n",
			__func__);
		goto err_inval;
	}

	spin_lock_irqsave(&src->device->spinlock, flags);
	src->event_callback = callback;
	src->event_cookie = cookie;
	src->event_bitmask = src_event_bitmask;
	spin_unlock_irqrestore(&src->device->spinlock, flags);

	/* Enable/disable flow control stall interrupt on the source */
	reg = readl_relaxed(src->device->base + TSPP2_GLOBAL_IRQ_ENABLE);
	if (callback && (src_event_bitmask & TSPP2_SRC_EVENT_FLOW_CTRL_STALL)) {
		reg |= ((0x1 << src->hw_index) <<
			GLOBAL_IRQ_FC_STALL_OFFS);
	} else {
		reg &= ~((0x1 << src->hw_index) <<
			GLOBAL_IRQ_FC_STALL_OFFS);
	}
	writel_relaxed(reg, src->device->base + TSPP2_GLOBAL_IRQ_ENABLE);

	mutex_unlock(&src->device->mutex);

	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	dev_dbg(src->device->dev, "%s: successful\n", __func__);

	return 0;

err_inval:
	mutex_unlock(&src->device->mutex);
	pm_runtime_mark_last_busy(src->device->dev);
	pm_runtime_put_autosuspend(src->device->dev);

	return -EINVAL;
}
EXPORT_SYMBOL(tspp2_src_event_notification_register);

/**
 * tspp2_filter_event_notification_register() - Get notified on a filter event.
 *
 * @filter_handle:		Filter handle.
 * @filter_event_bitmask:	A bitmask of filter events,
 *				TSPP2_FILTER_EVENT_XXX.
 * @callback:			User callback function.
 * @cookie:			User information passed to the callback.
 *
 * Register a user callback which will be invoked when certain filter
 * events occur. Note the values (mask, callback and cookie) are overwritten
 * when calling this function multiple times. Therefore it is possible to
 * "unregister" a callback by calling this function with the bitmask set to 0
 * and with NULL callback and cookie.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_filter_event_notification_register(u32 filter_handle,
			u32 filter_event_bitmask,
			void (*callback)(void *cookie, u32 event_bitmask),
			void *cookie)
{
	int ret;
	int idx;
	u32 reg;
	unsigned long flags;
	struct tspp2_filter *filter = (struct tspp2_filter *)filter_handle;

	if (!filter) {
		pr_err("%s: Invalid filter handle\n", __func__);
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(filter->device->dev);
	if (ret < 0)
		return ret;

	if (mutex_lock_interruptible(&filter->device->mutex)) {
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -ERESTARTSYS;
	}

	if (!filter->device->opened) {
		pr_err("%s: Device must be opened first\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EPERM;
	}

	if (!filter->opened) {
		pr_err("%s: Filter not opened\n", __func__);
		mutex_unlock(&filter->device->mutex);
		pm_runtime_mark_last_busy(filter->device->dev);
		pm_runtime_put_autosuspend(filter->device->dev);
		return -EINVAL;
	}

	spin_lock_irqsave(&filter->device->spinlock, flags);
	filter->event_callback = callback;
	filter->event_cookie = cookie;
	filter->event_bitmask = filter_event_bitmask;
	spin_unlock_irqrestore(&filter->device->spinlock, flags);

	/* Enable/disable SC high/low interrupts per filter as requested */
	idx = (filter->context >> 5);
	reg = readl_relaxed(filter->device->base +
		TSPP2_SC_GO_HIGH_ENABLE(idx));
	if (callback &&
		(filter_event_bitmask & TSPP2_FILTER_EVENT_SCRAMBLING_HIGH)) {
		reg |= (0x1 << TSPP2_MODULUS_OP(filter->context, 32));
	} else {
		reg &= ~(0x1 << TSPP2_MODULUS_OP(filter->context, 32));
	}
	writel_relaxed(reg, filter->device->base +
		TSPP2_SC_GO_HIGH_ENABLE(idx));

	reg = readl_relaxed(filter->device->base +
		TSPP2_SC_GO_LOW_ENABLE(idx));
	if (callback &&
		(filter_event_bitmask & TSPP2_FILTER_EVENT_SCRAMBLING_LOW)) {
		reg |= (0x1 << TSPP2_MODULUS_OP(filter->context, 32));
	} else {
		reg &= ~(0x1 << TSPP2_MODULUS_OP(filter->context, 32));
	}
	writel_relaxed(reg, filter->device->base +
		TSPP2_SC_GO_LOW_ENABLE(idx));

	mutex_unlock(&filter->device->mutex);

	pm_runtime_mark_last_busy(filter->device->dev);
	pm_runtime_put_autosuspend(filter->device->dev);

	dev_dbg(filter->device->dev, "%s: successful\n", __func__);

	return 0;
}
EXPORT_SYMBOL(tspp2_filter_event_notification_register);

/**
 * tspp2_get_filter_hw_index() - Get a filter's hardware index.
 *
 * @filter_handle:		Filter handle.
 *
 * This is an helper function to support tspp2 auto-testing.
 *
 * Return the filter's hardware index on success, error value otherwise.
 */
int tspp2_get_filter_hw_index(u32 filter_handle)
{
	struct tspp2_filter *filter = (struct tspp2_filter *)filter_handle;
	if (!filter_handle)
		return -EINVAL;
	return filter->hw_index;
}
EXPORT_SYMBOL(tspp2_get_filter_hw_index);

/**
 * tspp2_get_reserved_hw_index() - Get a source's reserved hardware index.
 *
 * @src_handle:		Source handle.
 *
 * This is an helper function to support tspp2 auto-testing.
 *
 * Return the source's reserved hardware index on success,
 * error value otherwise.
 */
int tspp2_get_reserved_hw_index(u32 src_handle)
{
	struct tspp2_src *src = (struct tspp2_src *)src_handle;
	if (!src_handle)
		return -EINVAL;
	return src->reserved_filter_hw_index;
}
EXPORT_SYMBOL(tspp2_get_reserved_hw_index);

/**
 * tspp2_get_ops_array() - Get filter's operations.
 *
 * @filter_handle:		Filter handle.
 * @ops_array:			The filter's operations.
 * @num_of_ops:			The filter's number of operations.
 *
 * This is an helper function to support tspp2 auto-testing.
 *
 * Return 0 on success, error value otherwise.
 */
int tspp2_get_ops_array(u32 filter_handle,
		struct tspp2_operation ops_array[TSPP2_MAX_OPS_PER_FILTER],
		u8 *num_of_ops)
{
	int i;
	struct tspp2_filter *filter = (struct tspp2_filter *)filter_handle;
	if (!filter_handle || !num_of_ops)
		return -EINVAL;
	*num_of_ops = filter->num_user_operations;
	for (i = 0; i < *num_of_ops; i++)
		ops_array[i] = filter->operations[i];
	return 0;
}
EXPORT_SYMBOL(tspp2_get_ops_array);

/* Platform driver related functions: */

/**
 * msm_tspp2_dt_to_pdata() - Copy device-tree data to platfrom data structure.
 *
 * @pdev:	Platform device.
 *
 * Return pointer to allocated platform data on success, NULL on failure.
 */
static struct msm_tspp2_platform_data *
msm_tspp2_dt_to_pdata(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct msm_tspp2_platform_data *data;
	int rc;

	/* Note: memory allocated by devm_kzalloc is freed automatically */
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("%s: Unable to allocate platform data\n", __func__);
		return NULL;
	}

	/* Get power regulator */
	if (!of_get_property(node, "vdd-supply", NULL)) {
		pr_err("%s: Could not find vdd-supply property\n", __func__);
		return NULL;
	}

	/* Get IOMMU information */
	rc = of_property_read_string(node, "qcom,iommu-hlos-group",
					&data->hlos_group);
	if (rc) {
		pr_err("%s: Could not find iommu-hlos-group property, err = %d\n",
			__func__, rc);
		return NULL;
	}
	rc = of_property_read_string(node, "qcom,iommu-cpz-group",
					&data->cpz_group);
	if (rc) {
		pr_err("%s: Could not find iommu-cpz-group property, err = %d\n",
			__func__, rc);
		return NULL;
	}
	rc = of_property_read_u32(node, "qcom,iommu-hlos-partition",
					&data->hlos_partition);
	if (rc) {
		pr_err("%s: Could not find iommu-hlos-partition property, err = %d\n",
			__func__, rc);
		return NULL;
	}
	rc = of_property_read_u32(node, "qcom,iommu-cpz-partition",
					&data->cpz_partition);
	if (rc) {
		pr_err("%s: Could not find iommu-cpz-partition property, err = %d\n",
			__func__, rc);
		return NULL;
	}

	return data;
}

static void msm_tspp2_iommu_info_free(struct tspp2_device *device)
{
	if (device->iommu_info.hlos_group) {
		iommu_group_put(device->iommu_info.hlos_group);
		device->iommu_info.hlos_group = NULL;
	}

	if (device->iommu_info.cpz_group) {
		iommu_group_put(device->iommu_info.cpz_group);
		device->iommu_info.cpz_group = NULL;
	}

	device->iommu_info.hlos_domain = NULL;
	device->iommu_info.cpz_domain = NULL;
	device->iommu_info.hlos_domain_num = -1;
	device->iommu_info.cpz_domain_num = -1;
	device->iommu_info.hlos_partition = -1;
	device->iommu_info.cpz_partition = -1;
}

/**
 * msm_tspp2_iommu_info_get() - Get IOMMU information.
 *
 * @pdev:	Platform device, containing platform information.
 * @device:	TSPP2 device.
 *
 * Return 0 on success, error value otherwise.
 */
static int msm_tspp2_iommu_info_get(struct platform_device *pdev,
						struct tspp2_device *device)
{
	int ret = 0;
	struct msm_tspp2_platform_data *data = pdev->dev.platform_data;

	device->iommu_info.hlos_group = NULL;
	device->iommu_info.cpz_group = NULL;
	device->iommu_info.hlos_domain = NULL;
	device->iommu_info.cpz_domain = NULL;
	device->iommu_info.hlos_domain_num = -1;
	device->iommu_info.cpz_domain_num = -1;
	device->iommu_info.hlos_partition = -1;
	device->iommu_info.cpz_partition = -1;

	device->iommu_info.hlos_group = iommu_group_find(data->hlos_group);
	if (!device->iommu_info.hlos_group) {
		dev_err(&pdev->dev, "%s: Cannot find IOMMU HLOS group",
			__func__);
		ret = -EINVAL;
		goto err_out;
	}
	device->iommu_info.cpz_group = iommu_group_find(data->cpz_group);
	if (!device->iommu_info.cpz_group) {
		dev_err(&pdev->dev, "%s: Cannot find IOMMU CPZ group",
			__func__);
		ret = -EINVAL;
		goto err_out;
	}

	device->iommu_info.hlos_domain =
		iommu_group_get_iommudata(device->iommu_info.hlos_group);
	if (IS_ERR_OR_NULL(device->iommu_info.hlos_domain)) {
		dev_err(&pdev->dev, "%s: iommu_group_get_iommudata failed",
			__func__);
		ret = -EINVAL;
		goto err_out;
	}

	device->iommu_info.cpz_domain =
		iommu_group_get_iommudata(device->iommu_info.cpz_group);
	if (IS_ERR_OR_NULL(device->iommu_info.cpz_domain)) {
		device->iommu_info.hlos_domain = NULL;
		dev_err(&pdev->dev, "%s: iommu_group_get_iommudata failed",
			__func__);
		ret = -EINVAL;
		goto err_out;
	}

	device->iommu_info.hlos_domain_num =
			msm_find_domain_no(device->iommu_info.hlos_domain);
	device->iommu_info.cpz_domain_num =
			msm_find_domain_no(device->iommu_info.cpz_domain);
	device->iommu_info.hlos_partition = data->hlos_partition;
	device->iommu_info.cpz_partition = data->cpz_partition;

	return 0;

err_out:
	msm_tspp2_iommu_info_free(device);

	return ret;
}

/**
 * tspp2_clocks_put() - Put clocks and disable regulator.
 *
 * @device:	TSPP2 device.
 */
static void tspp2_clocks_put(struct tspp2_device *device)
{
	if (device->tsif_ref_clk)
		clk_put(device->tsif_ref_clk);

	if (device->tspp2_klm_ahb_clk)
		clk_put(device->tspp2_klm_ahb_clk);

	if (device->tspp2_vbif_clk)
		clk_put(device->tspp2_vbif_clk);

	if (device->vbif_ahb_clk)
		clk_put(device->vbif_ahb_clk);

	if (device->vbif_axi_clk)
		clk_put(device->vbif_axi_clk);

	if (device->tspp2_core_clk)
		clk_put(device->tspp2_core_clk);

	if (device->tspp2_ahb_clk)
		clk_put(device->tspp2_ahb_clk);

	device->tspp2_ahb_clk = NULL;
	device->tspp2_core_clk = NULL;
	device->tspp2_vbif_clk = NULL;
	device->vbif_ahb_clk = NULL;
	device->vbif_axi_clk = NULL;
	device->tspp2_klm_ahb_clk = NULL;
	device->tsif_ref_clk = NULL;
}

/**
 * msm_tspp2_clocks_setup() - Get clocks and set their rate, enable regulator.
 *
 * @pdev:	Platform device, containing platform information.
 * @device:	TSPP2 device.
 *
 * Return 0 on success, error value otherwise.
 */
static int msm_tspp2_clocks_setup(struct platform_device *pdev,
						struct tspp2_device *device)
{
	int ret = 0;
	unsigned long rate_in_hz = 0;
	struct clk *tspp2_core_clk_src = NULL;

	/* Get power regulator (GDSC) */
	device->gdsc = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(device->gdsc)) {
		pr_err("%s: Failed to get vdd power regulator\n", __func__);
		ret = PTR_ERR(device->gdsc);
		device->gdsc = NULL;
		return ret;
	}

	device->tspp2_ahb_clk = NULL;
	device->tspp2_core_clk = NULL;
	device->tspp2_vbif_clk = NULL;
	device->vbif_ahb_clk = NULL;
	device->vbif_axi_clk = NULL;
	device->tspp2_klm_ahb_clk = NULL;
	device->tsif_ref_clk = NULL;

	device->tspp2_ahb_clk = clk_get(&pdev->dev, "bcc_tspp2_ahb_clk");
	if (IS_ERR(device->tspp2_ahb_clk)) {
		pr_err("%s: Failed to get %s", __func__, "bcc_tspp2_ahb_clk");
		ret = PTR_ERR(device->tspp2_ahb_clk);
		device->tspp2_ahb_clk = NULL;
		goto err_clocks;
	}

	device->tspp2_core_clk = clk_get(&pdev->dev, "bcc_tspp2_core_clk");
	if (IS_ERR(device->tspp2_core_clk)) {
		pr_err("%s: Failed to get %s", __func__, "bcc_tspp2_core_clk");
		ret = PTR_ERR(device->tspp2_core_clk);
		device->tspp2_core_clk = NULL;
		goto err_clocks;
	}

	device->tspp2_vbif_clk = clk_get(&pdev->dev, "bcc_vbif_tspp2_clk");
	if (IS_ERR(device->tspp2_vbif_clk)) {
		pr_err("%s: Failed to get %s", __func__, "bcc_vbif_tspp2_clk");
		ret = PTR_ERR(device->tspp2_vbif_clk);
		device->tspp2_vbif_clk = NULL;
		goto err_clocks;
	}

	device->vbif_ahb_clk = clk_get(&pdev->dev, "iface_vbif_clk");
	if (IS_ERR(device->vbif_ahb_clk)) {
		pr_err("%s: Failed to get %s", __func__, "iface_vbif_clk");
		ret = PTR_ERR(device->vbif_ahb_clk);
		device->vbif_ahb_clk = NULL;
		goto err_clocks;
	}

	device->vbif_axi_clk = clk_get(&pdev->dev, "vbif_core_clk");
	if (IS_ERR(device->vbif_axi_clk)) {
		pr_err("%s: Failed to get %s", __func__, "vbif_core_clk");
		ret = PTR_ERR(device->vbif_axi_clk);
		device->vbif_axi_clk = NULL;
		goto err_clocks;
	}

	device->tspp2_klm_ahb_clk = clk_get(&pdev->dev, "bcc_klm_ahb_clk");
	if (IS_ERR(device->tspp2_klm_ahb_clk)) {
		pr_err("%s: Failed to get %s", __func__, "bcc_klm_ahb_clk");
		ret = PTR_ERR(device->tspp2_klm_ahb_clk);
		device->tspp2_klm_ahb_clk = NULL;
		goto err_clocks;
	}

	device->tsif_ref_clk = clk_get(&pdev->dev, "gcc_tsif_ref_clk");
	if (IS_ERR(device->tsif_ref_clk)) {
		pr_err("%s: Failed to get %s", __func__, "gcc_tsif_ref_clk");
		ret = PTR_ERR(device->tsif_ref_clk);
		device->tsif_ref_clk = NULL;
		goto err_clocks;
	}

	/* Set relevant clock rates */
	rate_in_hz = clk_round_rate(device->tsif_ref_clk, 1);
	if (clk_set_rate(device->tsif_ref_clk, rate_in_hz)) {
		pr_err("%s: Failed to set rate %lu to %s\n", __func__,
			rate_in_hz, "gcc_tsif_ref_clk");
		goto err_clocks;
	}

	/* We need to set the rate of tspp2_core_clk_src */
	tspp2_core_clk_src = clk_get_parent(device->tspp2_core_clk);
	if (tspp2_core_clk_src) {
		rate_in_hz = clk_round_rate(tspp2_core_clk_src, 1);
		if (clk_set_rate(tspp2_core_clk_src, rate_in_hz)) {
			pr_err("%s: Failed to set rate %lu to tspp2_core_clk_src\n",
				__func__, rate_in_hz);
			goto err_clocks;
		}
	} else {
		pr_err("%s: Failed to get tspp2_core_clk parent\n", __func__);
		goto err_clocks;
	}

	return 0;

err_clocks:
	tspp2_clocks_put(device);

	return ret;
}

/**
 * msm_tspp2_map_io_memory() - Map memory resources to kernel space.
 *
 * @pdev:	Platform device, containing platform information.
 * @device:	TSPP2 device.
 *
 * Return 0 on success, error value otherwise.
 */
static int msm_tspp2_map_io_memory(struct platform_device *pdev,
						struct tspp2_device *device)
{
	struct resource *mem_tsif0;
	struct resource *mem_tsif1;
	struct resource *mem_tspp2;
	struct resource *mem_bam;

	/* Get memory resources */
	mem_tsif0 = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "MSM_TSIF0");
	if (!mem_tsif0) {
		dev_err(&pdev->dev, "%s: Missing TSIF0 MEM resource", __func__);
		return -ENXIO;
	}

	mem_tsif1 = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "MSM_TSIF1");
	if (!mem_tsif1) {
		dev_err(&pdev->dev, "%s: Missing TSIF1 MEM resource", __func__);
		return -ENXIO;
	}

	mem_tspp2 = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "MSM_TSPP2");
	if (!mem_tspp2) {
		dev_err(&pdev->dev, "%s: Missing TSPP2 MEM resource", __func__);
		return -ENXIO;
	}

	mem_bam = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "MSM_TSPP2_BAM");
	if (!mem_bam) {
		dev_err(&pdev->dev, "%s: Missing BAM MEM resource", __func__);
		return -ENXIO;
	}

	/* Map memory physical addresses to kernel space */
	device->tsif_devices[0].base = ioremap(mem_tsif0->start,
		resource_size(mem_tsif0));
	if (!device->tsif_devices[0].base) {
		dev_err(&pdev->dev, "%s: ioremap failed", __func__);
		goto err_map_tsif0;
	}

	device->tsif_devices[1].base = ioremap(mem_tsif1->start,
		resource_size(mem_tsif1));
	if (!device->tsif_devices[1].base) {
		dev_err(&pdev->dev, "%s: ioremap failed", __func__);
		goto err_map_tsif1;
	}

	device->base = ioremap(mem_tspp2->start, resource_size(mem_tspp2));
	if (!device->base) {
		dev_err(&pdev->dev, "%s: ioremap failed", __func__);
		goto err_map_dev;
	}

	memset(&device->bam_props, 0, sizeof(device->bam_props));
	device->bam_props.phys_addr = mem_bam->start;
	device->bam_props.virt_addr = ioremap(mem_bam->start,
		resource_size(mem_bam));
	if (!device->bam_props.virt_addr) {
		dev_err(&pdev->dev, "%s: ioremap failed", __func__);
		goto err_map_bam;
	}

	return 0;

err_map_bam:
	iounmap(device->base);

err_map_dev:
	iounmap(device->tsif_devices[1].base);

err_map_tsif1:
	iounmap(device->tsif_devices[0].base);

err_map_tsif0:
	return -ENXIO;
}

/**
 * tspp2_event_work_prepare() - Prepare and queue a work element.
 *
 * @device:		TSPP2 device.
 * @callback:		User callback to invoke.
 * @cookie:		User cookie.
 * @event_bitmask:	Event bitmask
 *
 * Get a free work element from the pool, prepare it and queue it
 * to the work queue. When scheduled, the work will invoke the user callback
 * for the event that the HW reported.
 */
static void tspp2_event_work_prepare(struct tspp2_device *device,
			void (*callback)(void *cookie, u32 event_bitmask),
			void *cookie,
			u32 event_bitmask)
{
	struct tspp2_event_work *work = NULL;

	if (!list_empty(&device->free_work_list)) {
		work = list_first_entry(&device->free_work_list,
			struct tspp2_event_work, link);
		list_del(&work->link);
		work->callback = callback;
		work->cookie = cookie;
		work->event_bitmask = event_bitmask;
		queue_work(device->work_queue, &work->work);
	} else {
		pr_warn("%s: No available work element\n", __func__);
	}
}

/**
 * tspp2_isr() - TSPP2 interrupt handler.
 *
 * @irq:	Interrupt number.
 * @dev:	TSPP2 device.
 *
 * Handle TSPP2 HW interrupt. Collect relevant statistics and invoke
 * user registered callbacks for global, source or filter events.
 *
 * Return IRQ_HANDLED.
 */
static irqreturn_t tspp2_isr(int irq, void *dev)
{
	struct tspp2_device *device = dev;
	struct tspp2_src *src = NULL;
	struct tspp2_filter *f = NULL;
	unsigned long ext_reg = 0;
	unsigned long val = 0;
	unsigned long flags;
	u32 i = 0, j = 0;
	u32 global_bitmask = 0;
	u32 src_bitmask[TSPP2_NUM_MEM_INPUTS] = {0};
	u32 filter_bitmask[TSPP2_NUM_CONTEXTS] = {0};
	u32 reg = 0;

	reg = readl_relaxed(device->base + TSPP2_GLOBAL_IRQ_STATUS);

	if (reg & (0x1 << GLOBAL_IRQ_TSP_INVALID_AF_OFFS)) {
		device->irq_stats.global.tsp_invalid_af_control++;
		global_bitmask |= TSPP2_GLOBAL_EVENT_INVALID_AF_CTRL;
	}

	if (reg & (0x1 << GLOBAL_IRQ_TSP_INVALID_LEN_OFFS)) {
		device->irq_stats.global.tsp_invalid_length++;
		global_bitmask |= TSPP2_GLOBAL_EVENT_INVALID_AF_LENGTH;
	}

	if (reg & (0x1 << GLOBAL_IRQ_PES_NO_SYNC_OFFS)) {
		device->irq_stats.global.pes_no_sync++;
		global_bitmask |= TSPP2_GLOBAL_EVENT_PES_NO_SYNC;
	}

	if (reg & (0x1 << GLOBAL_IRQ_ENCRYPT_LEVEL_ERR_OFFS))
		device->irq_stats.global.encrypt_level_err++;

	if (reg & (0x1 << GLOBAL_IRQ_KEY_NOT_READY_OFFS)) {
		ext_reg = readl_relaxed(device->base +
				TSPP2_KEY_NOT_READY_IRQ_STATUS);
		for_each_set_bit(i, &ext_reg, TSPP2_NUM_KEYTABLES)
			device->irq_stats.kt[i].key_not_ready++;
		writel_relaxed(ext_reg, device->base +
			TSPP2_KEY_NOT_READY_IRQ_CLEAR);
	}

	if (reg & (0x1 << GLOBAL_IRQ_UNEXPECTED_RESET_OFFS)) {
		ext_reg = readl_relaxed(device->base +
				TSPP2_UNEXPECTED_RST_IRQ_STATUS);
		for_each_set_bit(i, &ext_reg, TSPP2_NUM_PIPES)
			device->irq_stats.pipe[i].unexpected_reset++;
		writel_relaxed(ext_reg, device->base +
			TSPP2_UNEXPECTED_RST_IRQ_CLEAR);
	}

	if (reg & (0x1 << GLOBAL_IRQ_WRONG_PIPE_DIR_OFFS)) {
		ext_reg = readl_relaxed(device->base +
				TSPP2_WRONG_PIPE_DIR_IRQ_STATUS);
		for_each_set_bit(i, &ext_reg, TSPP2_NUM_PIPES)
			device->irq_stats.pipe[i].wrong_pipe_direction++;
		writel_relaxed(ext_reg, device->base +
			TSPP2_WRONG_PIPE_DIR_IRQ_CLEAR);
	}

	if (reg & (0x1 << GLOBAL_IRQ_QSB_RESP_ERR_OFFS)) {
		global_bitmask |= TSPP2_GLOBAL_EVENT_TX_FAIL;
		ext_reg = readl_relaxed(device->base +
				TSPP2_QSB_RESPONSE_ERROR_IRQ_STATUS);
		for_each_set_bit(i, &ext_reg, TSPP2_NUM_PIPES)
			device->irq_stats.pipe[i].qsb_response_error++;
		writel_relaxed(ext_reg, device->base +
			TSPP2_QSB_RESPONSE_ERROR_IRQ_CLEAR);
	}

	if (reg & (0x1 << GLOBAL_IRQ_SC_GO_HIGH_OFFS)) {
		for (j = 0; j < 3; j++) {
			ext_reg = readl_relaxed(device->base +
					TSPP2_SC_GO_HIGH_STATUS(j));
			for_each_set_bit(i, &ext_reg, 32) {
				filter_bitmask[j*32 + i] |=
					TSPP2_FILTER_EVENT_SCRAMBLING_HIGH;
				device->irq_stats.ctx[j*32 + i].sc_go_high++;
			}
			writel_relaxed(ext_reg, device->base +
					TSPP2_SC_GO_HIGH_CLEAR(j));
		}
	}

	if (reg & (0x1 << GLOBAL_IRQ_SC_GO_LOW_OFFS)) {
		for (j = 0; j < 3; j++) {
			ext_reg = readl_relaxed(device->base +
					TSPP2_SC_GO_LOW_STATUS(j));
			for_each_set_bit(i, &ext_reg, 32) {
				filter_bitmask[j*32 + i] |=
					TSPP2_FILTER_EVENT_SCRAMBLING_LOW;
				device->irq_stats.ctx[j*32 + i].sc_go_low++;
			}
			writel_relaxed(ext_reg, device->base +
					TSPP2_SC_GO_LOW_CLEAR(j));
		}
	}

	if (reg & (0xFF << GLOBAL_IRQ_READ_FAIL_OFFS)) {
		val = ((reg & (0xFF << GLOBAL_IRQ_READ_FAIL_OFFS)) >>
			GLOBAL_IRQ_READ_FAIL_OFFS);
		for_each_set_bit(i, &val, TSPP2_NUM_MEM_INPUTS) {
			src_bitmask[i] |= TSPP2_SRC_EVENT_MEMORY_READ_ERROR;
			device->irq_stats.src[i].read_failure++;
		}
	}

	if (reg & (0xFF << GLOBAL_IRQ_FC_STALL_OFFS)) {
		val = ((reg & (0xFF << GLOBAL_IRQ_FC_STALL_OFFS)) >>
			GLOBAL_IRQ_FC_STALL_OFFS);
		for_each_set_bit(i, &val, TSPP2_NUM_MEM_INPUTS) {
			src_bitmask[i] |= TSPP2_SRC_EVENT_FLOW_CTRL_STALL;
			device->irq_stats.src[i].flow_control_stall++;
		}
	}

	spin_lock_irqsave(&device->spinlock, flags);

	/* Invoke user callback for global events */
	if (device->event_callback && (global_bitmask & device->event_bitmask))
		tspp2_event_work_prepare(device, device->event_callback,
			device->event_cookie,
			(global_bitmask & device->event_bitmask));

	/* Invoke user callbacks on memory source events */
	for (i = 0; i < TSPP2_NUM_MEM_INPUTS; i++) {
		src = &device->mem_sources[i];
		if (src->event_callback &&
			(src_bitmask[src->hw_index] & src->event_bitmask))
			tspp2_event_work_prepare(device,
				src->event_callback,
				src->event_cookie,
				(src_bitmask[src->hw_index] &
					src->event_bitmask));
	}

	/* Invoke user callbacks on filter events */
	for (i = 0; i < TSPP2_NUM_AVAIL_FILTERS; i++) {
		f = &device->filters[i];
		if (f->event_callback &&
			(f->event_bitmask & filter_bitmask[f->context]))
			tspp2_event_work_prepare(device,
				f->event_callback,
				f->event_cookie,
				(f->event_bitmask &
					filter_bitmask[f->context]));
	}

	spin_unlock_irqrestore(&device->spinlock, flags);

	/*
	 * Clear global interrupts. Note bits [9:4] are an aggregation of
	 * other IRQs, and are reserved in the TSPP2_GLOBAL_IRQ_CLEAR register.
	 */
	reg &= ~(0x0FFF << GLOBAL_IRQ_CLEAR_RESERVED_OFFS);
	writel_relaxed(reg, device->base + TSPP2_GLOBAL_IRQ_CLEAR);
	/*
	 * Before returning IRQ_HANDLED to the generic interrupt handling
	 * framework, we need to make sure all operations, including clearing of
	 * interrupt status registers in the hardware, are performed.
	 * Thus a barrier after clearing the interrupt status register
	 * is required to guarantee that the interrupt status register has
	 * really been cleared by the time we return from this handler.
	 */
	wmb();

	return IRQ_HANDLED;
}

/**
 * tsif_isr() - TSIF interrupt handler.
 *
 * @irq:	Interrupt number.
 * @dev:	TSIF device that generated the interrupt.
 *
 * Handle TSIF HW interrupt. Collect HW statistics and, if the user registered
 * a relevant source callback, invoke it.
 *
 * Return IRQ_HANDLED on success, IRQ_NONE on irrelevant interrupts.
 */
static irqreturn_t tsif_isr(int irq, void *dev)
{
	u32 src_bitmask = 0;
	unsigned long flags;
	struct tspp2_src *src = NULL;
	struct tspp2_tsif_device *tsif_device = dev;
	u32 sts_ctl = 0;

	sts_ctl = readl_relaxed(tsif_device->base + TSPP2_TSIF_STS_CTL);

	if (!(sts_ctl & (TSIF_STS_CTL_PACK_AVAIL	|
			TSIF_STS_CTL_PKT_WRITE_ERR	|
			TSIF_STS_CTL_PKT_READ_ERR	|
			TSIF_STS_CTL_OVERFLOW		|
			TSIF_STS_CTL_LOST_SYNC		|
			TSIF_STS_CTL_TIMEOUT))) {
		return IRQ_NONE;
	}

	if (sts_ctl & TSIF_STS_CTL_PKT_WRITE_ERR) {
		src_bitmask |= TSPP2_SRC_EVENT_TSIF_PKT_WRITE_ERROR;
		tsif_device->stat_pkt_write_err++;
	}

	if (sts_ctl & TSIF_STS_CTL_PKT_READ_ERR) {
		src_bitmask |= TSPP2_SRC_EVENT_TSIF_PKT_READ_ERROR;
		tsif_device->stat_pkt_read_err++;
	}

	if (sts_ctl & TSIF_STS_CTL_OVERFLOW) {
		src_bitmask |= TSPP2_SRC_EVENT_TSIF_OVERFLOW;
		tsif_device->stat_overflow++;
	}

	if (sts_ctl & TSIF_STS_CTL_LOST_SYNC) {
		src_bitmask |= TSPP2_SRC_EVENT_TSIF_LOST_SYNC;
		tsif_device->stat_lost_sync++;
	}

	if (sts_ctl & TSIF_STS_CTL_TIMEOUT) {
		src_bitmask |= TSPP2_SRC_EVENT_TSIF_TIMEOUT;
		tsif_device->stat_timeout++;
	}

	/* Invoke user TSIF source callbacks if registered for these events */
	src = &tsif_device->dev->tsif_sources[tsif_device->hw_index];

	spin_lock_irqsave(&src->device->spinlock, flags);

	if (src->event_callback && (src->event_bitmask & src_bitmask))
		tspp2_event_work_prepare(tsif_device->dev, src->event_callback,
			src->event_cookie, (src->event_bitmask & src_bitmask));

	spin_unlock_irqrestore(&src->device->spinlock, flags);

	writel_relaxed(sts_ctl, tsif_device->base + TSPP2_TSIF_STS_CTL);
	/*
	 * Before returning IRQ_HANDLED to the generic interrupt handling
	 * framework, we need to make sure all operations, including clearing of
	 * interrupt status registers in the hardware, are performed.
	 * Thus a barrier after clearing the interrupt status register
	 * is required to guarantee that the interrupt status register has
	 * really been cleared by the time we return from this handler.
	 */
	wmb();

	return IRQ_HANDLED;
}

/**
 * msm_tspp2_map_irqs() - Get and request IRQs.
 *
 * @pdev:	Platform device, containing platform information.
 * @device:	TSPP2 device.
 *
 * Helper function to get IRQ numbers from the platform device and request
 * the IRQs (i.e., set interrupt handlers) for the TSPP2 and TSIF interrupts.
 *
 * Return 0 on success, error value otherwise.
 */
static int msm_tspp2_map_irqs(struct platform_device *pdev,
				struct tspp2_device *device)
{
	int rc;
	int i;

	/* get IRQ numbers from platform information */

	rc = platform_get_irq_byname(pdev, "TSPP2");
	if (rc > 0) {
		device->tspp2_irq = rc;
	} else {
		dev_err(&pdev->dev, "%s: Failed to get TSPP2 IRQ", __func__);
		return -EINVAL;
	}

	rc = platform_get_irq_byname(pdev, "TSIF0");
	if (rc > 0) {
		device->tsif_devices[0].tsif_irq = rc;
	} else {
		dev_err(&pdev->dev, "%s: Failed to get TSIF0 IRQ", __func__);
		return -EINVAL;
	}

	rc = platform_get_irq_byname(pdev, "TSIF1");
	if (rc > 0) {
		device->tsif_devices[1].tsif_irq = rc;
	} else {
		dev_err(&pdev->dev, "%s: Failed to get TSIF1 IRQ", __func__);
		return -EINVAL;
	}

	rc = platform_get_irq_byname(pdev, "TSPP2_BAM");
	if (rc > 0) {
		device->bam_irq = rc;
	} else {
		dev_err(&pdev->dev,
			"%s: Failed to get TSPP2 BAM IRQ", __func__);
		return -EINVAL;
	}

	rc = request_irq(device->tspp2_irq, tspp2_isr, IRQF_SHARED,
			 dev_name(&pdev->dev), device);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: Failed to request TSPP2 IRQ %d : %d",
			__func__, device->tspp2_irq, rc);
		goto request_irq_err;
	}

	for (i = 0; i < TSPP2_NUM_TSIF_INPUTS; i++) {
		rc = request_irq(device->tsif_devices[i].tsif_irq,
				tsif_isr, IRQF_SHARED,
				dev_name(&pdev->dev), &device->tsif_devices[i]);
		if (rc) {
			dev_warn(&pdev->dev,
				"%s: Failed to request TSIF%d IRQ: %d",
				__func__, i, rc);
			device->tsif_devices[i].tsif_irq = 0;
		}
	}

	return 0;

request_irq_err:
	device->tspp2_irq = 0;
	device->tsif_devices[0].tsif_irq = 0;
	device->tsif_devices[1].tsif_irq = 0;
	device->bam_irq = 0;

	return -EINVAL;
}

/* Device driver probe function */
static int msm_tspp2_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_tspp2_platform_data *data;
	struct tspp2_device *device;
	struct msm_bus_scale_pdata *tspp2_bus_pdata = NULL;

	if (pdev->dev.of_node) {
		/* Get information from device tree */
		data = msm_tspp2_dt_to_pdata(pdev);
		/* get device ID */
		rc = of_property_read_u32(pdev->dev.of_node,
					"cell-index", &pdev->id);
		if (rc)
			pdev->id = -1;

		tspp2_bus_pdata = msm_bus_cl_get_pdata(pdev);
		pdev->dev.platform_data = data;
	} else {
		/* Get information from platform data */
		data = pdev->dev.platform_data;
	}
	if (!data) {
		pr_err("%s: Platform data not available\n", __func__);
		return -EINVAL;
	}

	/* Verify device id is valid */
	if ((pdev->id < 0) || (pdev->id >= TSPP2_NUM_DEVICES)) {
		pr_err("%s: Invalid device ID %d\n", __func__, pdev->id);
		return -EINVAL;
	}

	device = devm_kzalloc(&pdev->dev,
				sizeof(struct tspp2_device),
				GFP_KERNEL);
	if (!device) {
		pr_err("%s: Failed to allocate memory for device\n", __func__);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, device);
	device->pdev = pdev;
	device->dev = &pdev->dev;
	device->dev_id = pdev->id;
	device->opened = 0;

	/* Register bus client */
	if (tspp2_bus_pdata) {
		device->bus_client =
			msm_bus_scale_register_client(tspp2_bus_pdata);
		if (!device->bus_client)
			pr_err("%s: Unable to register bus client\n", __func__);
	} else {
		pr_err("%s: Platform bus client data not available. Continue anyway...\n",
			__func__);
	}

	rc = msm_tspp2_iommu_info_get(pdev, device);
	if (rc) {
		pr_err("%s: Failed to get IOMMU information\n", __func__);
		goto err_bus_client;
	}

	rc = msm_tspp2_clocks_setup(pdev, device);
	if (rc)
		goto err_clocks_setup;

	rc = msm_tspp2_map_io_memory(pdev, device);
	if (rc)
		goto err_map_io_memory;

	rc = msm_tspp2_map_irqs(pdev, device);
	if (rc)
		goto err_map_irq;

	mutex_init(&device->mutex);

	tspp2_devices[pdev->id] = device;

	tspp2_debugfs_init(device);

	return rc;

err_map_irq:
	iounmap(device->base);
	iounmap(device->tsif_devices[0].base);
	iounmap(device->tsif_devices[1].base);
	iounmap(device->bam_props.virt_addr);

err_map_io_memory:
	tspp2_clocks_put(device);

err_clocks_setup:
	msm_tspp2_iommu_info_free(device);

err_bus_client:
	if (device->bus_client)
		msm_bus_scale_unregister_client(device->bus_client);

	return rc;
}

/* Device driver remove function */
static int msm_tspp2_remove(struct platform_device *pdev)
{
	int i;
	int rc = 0;
	struct tspp2_device *device = platform_get_drvdata(pdev);

	tspp2_debugfs_exit(device);

	if (device->tspp2_irq)
		free_irq(device->tspp2_irq, device);

	for (i = 0; i < TSPP2_NUM_TSIF_INPUTS; i++)
		if (device->tsif_devices[i].tsif_irq)
			free_irq(device->tsif_devices[i].tsif_irq,
				&device->tsif_devices[i]);

	/* Unmap memory */
	iounmap(device->base);
	iounmap(device->tsif_devices[0].base);
	iounmap(device->tsif_devices[1].base);
	iounmap(device->bam_props.virt_addr);

	msm_tspp2_iommu_info_free(device);

	if (device->bus_client)
		msm_bus_scale_unregister_client(device->bus_client);

	mutex_destroy(&device->mutex);

	tspp2_clocks_put(device);

	return rc;
}

/* Power Management */

static int tspp2_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct tspp2_device *device;
	struct platform_device *pdev;

	/*
	 * HW manages power collapse automatically.
	 * Disabling AHB and Core clocsk and "cancelling" bus bandwidth voting.
	 */

	pdev = container_of(dev, struct platform_device, dev);
	device = platform_get_drvdata(pdev);

	mutex_lock(&device->mutex);

	if (!device->opened)
		ret = -EPERM;
	else
		ret = tspp2_reg_clock_stop(device);

	mutex_unlock(&device->mutex);

	dev_dbg(dev, "%s\n", __func__);

	return ret;
}

static int tspp2_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct tspp2_device *device;
	struct platform_device *pdev;

	/*
	 * HW manages power collapse automatically.
	 * Enabling AHB and Core clocks to allow access to unit registers,
	 * and voting for the required bus bandwidth for register access.
	 */

	pdev = container_of(dev, struct platform_device, dev);
	device = platform_get_drvdata(pdev);

	mutex_lock(&device->mutex);

	if (!device->opened)
		ret = -EPERM;
	else
		ret = tspp2_reg_clock_start(device);

	mutex_unlock(&device->mutex);

	dev_dbg(dev, "%s\n", __func__);

	return ret;
}

static const struct dev_pm_ops tspp2_dev_pm_ops = {
	.runtime_suspend = tspp2_runtime_suspend,
	.runtime_resume = tspp2_runtime_resume,
};

/* Platform driver information */

static struct of_device_id msm_tspp2_match_table[] = {
	{.compatible = "qcom,msm_tspp2"},
	{}
};

static struct platform_driver msm_tspp2_driver = {
	.probe          = msm_tspp2_probe,
	.remove         = msm_tspp2_remove,
	.driver         = {
		.name   = "msm_tspp2",
		.pm     = &tspp2_dev_pm_ops,
		.of_match_table = msm_tspp2_match_table,
	},
};

/**
 * tspp2_module_init() - TSPP2 driver module init function.
 *
 * Return 0 on success, error value otherwise.
 */
static int __init tspp2_module_init(void)
{
	int rc;

	rc = platform_driver_register(&msm_tspp2_driver);
	if (rc)
		pr_err("%s: platform_driver_register failed: %d\n",
			__func__, rc);

	return rc;
}

/**
 * tspp2_module_exit() - TSPP2 driver module exit function.
 */
static void __exit tspp2_module_exit(void)
{
	platform_driver_unregister(&msm_tspp2_driver);
}

module_init(tspp2_module_init);
module_exit(tspp2_module_exit);

MODULE_DESCRIPTION("TSPP2 (Transport Stream Packet Processor v2) platform device driver");
MODULE_LICENSE("GPL v2");
