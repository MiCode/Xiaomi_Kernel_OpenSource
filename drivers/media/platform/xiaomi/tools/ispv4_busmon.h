#ifndef __ISPV4_BUSMON_H
#define __ISPV4_BUSMON_H

#include <linux/bits.h>
#include <ispv4_regops.h>
#include "ispv4_busmon_reg.h"

extern void pcie_iatu_reset(uint32_t addr);
extern uint32_t pcie_read_reg(uint32_t addr);
extern uint32_t pcie_write_reg(uint32_t data, uint32_t addr);
enum busmon_inst {
	APB_BUSMON_INST_START,
	APB_BUSMON_INST0 = APB_BUSMON_INST_START,
	APB_BUSMON_INST1,
	APB_BUSMON_INST2,

	DDR_BUSMON_INST_START,
	DDR_BUSMON_INST0 = DDR_BUSMON_INST_START,
	DDR_BUSMON_INST1,
	DDR_BUSMON_INST2,
	DDR_BUSMON_INST3,
	DDR_BUSMON_INST4,

	ISP_BUSMON_INST_START,
	ISP_BUSMON_INST0 = ISP_BUSMON_INST_START,
	ISP_BUSMON_INST1,
	ISP_BUSMON_INST2,
	ISP_BUSMON_INST3,

	BUSMON_INST_CNT,
	APB_BUSMON_INST_CNT = DDR_BUSMON_INST_START - APB_BUSMON_INST_START,
	DDR_BUSMON_INST_CNT = ISP_BUSMON_INST_START - DDR_BUSMON_INST_START,
	ISP_BUSMON_INST_CNT = BUSMON_INST_CNT - ISP_BUSMON_INST_START,
};

/******************************************************************************
 *  Info for ISPV400
 *
 *-----------------------------------------------------------------------------
 *||             ||            AXI0            ||            AXI1            ||
 *||   INSTANCE  ||----------------------------||----------------------------||
 *||             ||   DEVICE    | FUNC | WIDTH ||   DEVICE    | FUNC | WIDTH ||
 *||-------------||-------------|------|-------||-------------|------|-------||
 *|| APB_BUSMON0 || PCIe        |  M   | 64    || MIPI        |  M   | 128   ||
 *||-------------||-------------|------|-------||-------------|------|-------||
 *|| APB_BUSMON1 || CPU NIC S0  |  M   | 64    || DMA         |  M   | 64    ||
 *||-------------||-------------|------|-------||-------------|------|-------||
 *|| APB_BUSMON2 || OCM NIC S1  |  P   | 128   || NPU NIC S1  |  P   | 128   ||
 *||-------------||-------------|------|-------||----------------------------||
 *|| DDR_BUSMON0 || ISP NIC S0  |  P   | 128   ||             NA             ||
 *||-------------||-------------|------|-------||----------------------------||
 *|| DDR_BUSMON1 || ISP NIC S1  |  P   | 128   ||             NA             ||
 *||-------------||-------------|------|-------||----------------------------||
 *|| DDR_BUSMON2 || OCM NIC S0  |  P   | 128   ||             NA             ||
 *||-------------||-------------|------|-------||----------------------------||
 *|| DDR_BUSMON3 || CPU NIC S1  |  P   | 128   ||             NA             ||
 *||-------------||-------------|------|-------||----------------------------||
 *|| DDR_BUSMON4 || Main NIC S1 |  P   | 64    ||             NA             ||
 *||-------------||-------------|------|-------||----------------------------||
 *|| ISP_BUSMON0 || ISP NIC M0  |  P   | 128   || ISP NIC M1  |  P   | 128   ||
 *||-------------||-------------|------|-------||-------------|------|-------||
 *|| ISP_BUSMON1 || ISP NIC M2  |  P   | 128   || ISP NIC M3  |  P   | 128   ||
 *||-------------||-------------|------|-------||-------------|------|-------||
 *|| ISP_BUSMON2 || ISP NIC M4  |  P   | 128   || ISP NIC M5  |  P   | 128   ||
 *||-------------||-------------|------|-------||-------------|------|-------||
 *|| ISP_BUSMON3 || ISP NIC S2  |  M   | 128   || CMD_DMA     |  M   | 64    ||
 *-----------------------------------------------------------------------------
 *
 *
 ******************************************************************************/

/* enum for APP use */
enum busmon_chn {
	APB_BUSMON_CHN_START,
	APB_BUSMON_PCIE = APB_BUSMON_CHN_START,
	APB_BUSMON_MIPI,
	APB_BUSMON_CPU,
	APB_BUSMON_DMA,
	APB_BUSMON_OCM,
	APB_BUSMON_NPU,

	DDR_BUSMON_CHN_START,
	DDR_BUSMON_ISP_NIC0 = DDR_BUSMON_CHN_START,
	DDR_BUSMON_ISP_NIC1,
	DDR_BUSMON_OCM_NIC,
	DDR_BUSMON_CPU_NIC,
	DDR_BUSMON_MAIN_NIC,

	ISP_BUSMON_CHN_START,
	ISP_BUSMON_FE0 = ISP_BUSMON_CHN_START,
	ISP_BUSMON_FE1,
	ISP_BUSMON_CVP,
	ISP_BUSMON_ROUTER,
	ISP_BUSMON_BEF,
	ISP_BUSMON_BEB,
	ISP_BUSMON_NIC,
	ISP_BUSMON_CMDDMA,

	BUSMON_CHN_CNT,
	APB_BUSMON_CHN_CNT = DDR_BUSMON_CHN_START - APB_BUSMON_CHN_START,
	DDR_BUSMON_CHN_CNT = ISP_BUSMON_CHN_START - DDR_BUSMON_CHN_START,
	ISP_BUSMON_CHN_CNT = BUSMON_CHN_CNT - ISP_BUSMON_CHN_START,
};

enum busmon_perf_output_option {
	/*
	 * Write the result to the specified memory, and call the callback function at the call point
	 */
	BUSMON_PERF_OUTPUT_WR_MEM,
	/*
	 * Print to log when each interrupt, and not call the callback
	 */
	BUSMON_PERF_OUTPUT_PRINT,
	BUSMON_PERF_OUTPUT_OPTION_CNT,
};

/* Provide address range and id range for match and perf function */
struct busmon_trans_filter_s {
	/* Address range: [addr_min, addr_max] */
	uint64_t addr_min;
	uint64_t addr_max;

	/* ID range: [id_min, id_max] */
	uint32_t id_min;
	uint32_t id_max;

	bool addr_exc; /* Exclude the specified address range */
	bool id_exc; /* Exclude the specified id range */
};

/***************************************
 *
 * Definition for match function
 *
 ***************************************/

#define ISPV4_BUSMON_MATCH_CNT_PER_CHN (8)

struct busmon_match_param_s {
	int chn;
	struct busmon_trans_filter_s filter;
};

struct busmon_trans_info_s {
	uint64_t addr;
	uint32_t id;
	uint8_t cache;
	uint8_t len;
	uint8_t size;
	uint8_t burst;
};

struct busmon_match_info_s {
	uint8_t wcmd_valid_cnt; /* valid count for write command record */
	uint8_t rcmd_valid_cnt; /* valid count for read command record */
	struct busmon_trans_info_s wr_trans_info[ISPV4_BUSMON_MATCH_CNT_PER_CHN];
	struct busmon_trans_info_s rd_trans_info[ISPV4_BUSMON_MATCH_CNT_PER_CHN];
};

typedef int (*busmon_match_cb)(struct busmon_match_info_s *info, void *arg);

int busmon_match_get_result(enum busmon_chn chn,
			    struct busmon_match_info_s *info);
int busmon_match_start(struct busmon_match_param_s *param);
void busmon_match_stop(enum busmon_chn chn);

/***************************************
 *
 * Definition for performance function
 *
 ***************************************/

#define ISPV4_BUSMON_PERF_CNT_PER_CHN (8)

struct busmon_alarm_param_s {
	uint32_t latency_th;
	uint32_t timeout_th;
};

enum busmon_perf_count_mode {
	BUSMON_PERF_CNT_ALL,
	BUSMON_PERF_CNT_ADDR,
	BUSMON_PERF_CNT_ID,
	BUSMON_PERF_CNT_ID_ADDR,
};

struct busmon_perf_output_cfg_s {
	enum busmon_perf_output_option option;
	size_t mem_size;
	//uintptr_t mem_addr; /* 4B aligned */
	/*
	 * memory size constraint: multiple of 64KB
	 */
};

struct busmon_perf_count_param_s {
	bool enable;
	enum busmon_perf_count_mode mode;
	struct busmon_trans_filter_s filter;
};

/* use 1 group for total statistics */
#define ISPV4_BUSMON_PERF_PARAM_CNT_PER_CHN (ISPV4_BUSMON_PERF_CNT_PER_CHN - 1)

struct busmon_perf_param_s {
	int chn;
	uint32_t win_len_us; /* unit: us */
	struct busmon_perf_output_cfg_s output_cfg;
	struct busmon_perf_count_param_s
		count_param[ISPV4_BUSMON_PERF_CNT_PER_CHN];
};

#define PERF_BW_CALC_UNIT (1024) /* KB */
#define PERF_BW_OVERFLOW_WR_SHIFT (0)
#define PERF_BW_OVERFLOW_RD_SHIFT (16)

#define PERF_RESULT_MAGIC 0x4255534d /* 'BUSM' */

/*128B*/
struct __attribute__((__packed__)) busmon_perf_cnt_result_s {
	/*
	 * total info:
	 *	4B * 10 = 40B
	 */
	uint32_t magic;
	uint64_t time_us;
	union {
		struct {
			uint8_t latcy_wr_of : 1;
			uint8_t latcy_wr_uf : 1;
			uint8_t latcy_rd_of : 1;
			uint8_t latcy_rd_uf : 1;
			uint8_t cmd_wr_of : 1;
			uint8_t cmd_rd_of : 1;
		} bits;
		uint32_t raw;
	} flags;
	int32_t latcy_total_wr;
	int32_t latcy_total_rd;
	int32_t latcy_ave_wr;
	int32_t latcy_ave_rd;
	uint32_t cmd_total_wr;
	uint32_t cmd_total_rd;

	/*
	 * bw info:
	 *	4B + 4B + 4B*8*2 = 72B
	 */
	uint32_t bw_enable;
	/* [ISPV4_BUSMON_PERF_CNT_PER_CHN-1 : 0] for write, [ISPV4_BUSMON_PERF_CNT_PER_CHN+15 : 16] for read */
	uint32_t bw_overflow;
	uint32_t bw_wr[ISPV4_BUSMON_PERF_CNT_PER_CHN]; /* KB/s */
	uint32_t bw_rd[ISPV4_BUSMON_PERF_CNT_PER_CHN]; /* KB/s */

	/* totally: 40B + 72B = 112B */
	uint8_t reserved[16];
};

int busmon_perf_start(struct busmon_perf_param_s *param);
void busmon_perf_stop(enum busmon_chn chn);

int busmon_latcy_alarm_start(enum busmon_chn chn,
			     struct busmon_alarm_param_s *param);
void busmon_latcy_alarm_stop(enum busmon_chn chn);

/* irq.h*/
#define ISPV4_IRQ_EXTINT 32 /* Vector number of the first ext int */
#define ISPV4_IRQ_BUSMON_CPU_DMA_REQ_MATCH (ISPV4_IRQ_EXTINT + 64)
#define ISPV4_IRQ_BUSMON_CPU_DMA_REQ_PERF_CNT (ISPV4_IRQ_EXTINT + 65)
#define ISPV4_IRQ_BUSMON_CPU_DMA_REQ_LAT_BAD (ISPV4_IRQ_EXTINT + 66)
#define ISPV4_IRQ_BUSMON_CPU_DMA_REQ_BUS_HANG (ISPV4_IRQ_EXTINT + 67)
#define ISPV4_IRQ_BUSMON_NPU_OCM_REQ_MATCH (ISPV4_IRQ_EXTINT + 68)
#define ISPV4_IRQ_BUSMON_NPU_OCM_REQ_PERF_CNT (ISPV4_IRQ_EXTINT + 69)
#define ISPV4_IRQ_BUSMON_NPU_OCM_REQ_LAT_BAD (ISPV4_IRQ_EXTINT + 70)
#define ISPV4_IRQ_BUSMON_NPU_OCM_REQ_BUS_HANG (ISPV4_IRQ_EXTINT + 71)
#define ISPV4_IRQ_BUSMON_PCIE_MIPI_REQ_MATCH (ISPV4_IRQ_EXTINT + 72)
#define ISPV4_IRQ_BUSMON_PCIE_MIPI_REQ_PERF_CNT (ISPV4_IRQ_EXTINT + 73)
#define ISPV4_IRQ_BUSMON_PCIE_MIPI_REQ_LAT_BAD (ISPV4_IRQ_EXTINT + 74)
#define ISPV4_IRQ_BUSMON_PCIE_MIPI_REQ_BUS_HANG (ISPV4_IRQ_EXTINT + 75)
#define ISPV4_IRQ_DDR_BUSMON (ISPV4_IRQ_EXTINT + 32) /* DDR Sys: bus monitor */
#define ISPV4_IRQ_ISP_BUSMON (ISPV4_IRQ_EXTINT + 91)

/***************************************
 *
 * registers read and write function
 *
 ***************************************/
static inline void _write_field(uintptr_t addr, uint32_t shift, uint32_t width,
				uint32_t data)
{
	uint32_t mask = ((1 << width) - 1) << shift;
	uint32_t addrin = addr;
	uint32_t tmp = getreg32(addrin) & (~mask);
	tmp |= ((data << shift) & mask);
	putreg32(tmp, addrin);
}
#define _read_field(addr, shift, width)                                        \
	((getreg32(addr) & (((1 << (width)) - 1) << (shift))) >> (shift))

#define write_field(base, reg, field, data)                                    \
	_write_field(base + reg##_OFFSET, field##_SHIFT, field##_WIDTH, data)

#define read_field(base, reg, field)                                           \
	_read_field(base + reg##_OFFSET, field##_SHIFT, field##_WIDTH)

#define write_all(base, reg, data) putreg32(data, base + reg##_OFFSET)

#define read_all(base, reg) (getreg32(base + reg##_OFFSET))

/* operations for grouped registers */
#define write_all_g(base, idx, reg, data)                                      \
	putreg32(data, base + reg##_OFFSET + (idx)*reg##_GRP_SIZE)

#define write_field_g(base, idx, reg, field, data)                             \
	write_field(base + (idx)*reg##_GRP_SIZE, reg, field, data)

#define read_all_g(base, idx, reg)                                             \
	(getreg32(base + reg##_OFFSET + (idx)*reg##_GRP_SIZE))

#define read_field_g(base, idx, reg, field)                                    \
	_read_field(base + reg##_OFFSET + (idx)*reg##_GRP_SIZE, field##_SHIFT, \
		    field##_WIDTH)

/* operations for bits grouped registers */
#define write_field_bg(base, reg, idx, field, data)                            \
	_write_field(base + reg##_OFFSET,                                      \
		     field##_SHIFT + (idx)*field##_GRP_BITS, field##_WIDTH,    \
		     data)
#define read_field_bg(base, reg, idx, field)                                                       \
	_read_field(base + reg##_OFFSET, field##_SHIFT + (idx)*field##_GRP_BITS, field##_WIDTH)

/**** pcie read and write reg**********************************************************************************************/

static inline void _write_field_pcie(uintptr_t addr, uint32_t shift, uint32_t width,
				uint32_t data)
{
	uint32_t mask = ((1 << width) - 1) << shift;
	uint32_t addrin = addr;
	uint32_t tmp = pcie_read_reg(addrin) & (~mask);
	tmp |= ((data << shift) & mask);
	pcie_write_reg(tmp, addrin);
}

#define read_all_pcie(base, reg)	(pcie_read_reg(base + reg##_OFFSET))
#define read_all_g_pcie(base, idx, reg) (pcie_read_reg(base + reg##_OFFSET + (idx)*reg##_GRP_SIZE))

#define _read_field_pcie(addr, shift, width)                                                       \
	((pcie_read_reg(addr) & (((1 << (width)) - 1) << (shift))) >> (shift))
#define read_field_pcie(base, reg, field)                                                          \
	_read_field_pcie(base + reg##_OFFSET, field##_SHIFT, field##_WIDTH)
#define read_field_g_pcie(base, idx, reg, field)                                                   \
	_read_field_pcie(base + reg##_OFFSET + (idx)*reg##_GRP_SIZE, field##_SHIFT, field##_WIDTH)
#define read_field_bg_pcie(base, reg, idx, field)                                                  \
	_read_field_pcie(base + reg##_OFFSET, field##_SHIFT + (idx)*field##_GRP_BITS, field##_WIDTH)

#define write_all_pcie(base, reg, data) pcie_write_reg(data, base + reg##_OFFSET)
#define write_all_g_pcie(base, idx, reg, data)                                                     \
	pcie_write_reg(data, base + reg##_OFFSET + (idx)*reg##_GRP_SIZE)

#define write_field_pcie(base, reg, field, data)                                                   \
	_write_field_pcie(base + reg##_OFFSET, field##_SHIFT, field##_WIDTH, data)

#define write_field_g_pcie(base, idx, reg, field, data)                                            \
	write_field_pcie(base + (idx)*reg##_GRP_SIZE, reg, field, data)

#define write_field_bg_pcie(base, reg, idx, field, data)                                           \
	_write_field_pcie(base + reg##_OFFSET, field##_SHIFT + (idx)*field##_GRP_BITS,             \
			  field##_WIDTH, data)
/***************************************
 *
 * ioctl setting
 *
 ***************************************/
/*AP INTC addr and bit set*/
#define AP_INTC_G0R0_INT_MASK_REG_ADDR (0xD42C000)
#define AP_INTC_G1R0_INT_MASK_REG_ADDR (0xD42C020)
#define AP_INTC_G2R0_INT_MASK_REG_ADDR (0xD42C040)
#define AP_INTC_G2R1_INT_MASK_REG_ADDR (0xD42C048)

#define AP_PMU_INT_BIT		       (1 << 14)

#define AP_DDR_BUSMON_BIT (1 << 0)
#define AP_ISP_BUSMON_BIT (1 << 27)
#define AP_REQ_MATCH_BIT (1 << 0)
#define AP_REQ_MATCHT_BIT (1 << 4)
#define AP_REQ_MATCHTT_BIT (1 << 8)
#define AP_REQ_PERFM_CNT_BIT (1 << 1)
#define AP_REQ_PERFM_CNTT_BIT (1 << 5)
#define AP_REQ_PERFM_CNTTT_BIT (1 << 9)
#define AP_REQ_MATCH		       (1 << 0 | 1 << 4 | 1 << 8)
#define AP_REQ_PERFM_CNT	       (1 << 1 | 1 << 5 | 1 << 9)

#define INTC_G2R1_INT_STATUS 0x4c
#define INTC_G2R2_INT_STATUS 0x54
#define AP_INTC_G2R1_INT_MASK 0x48
#define AP_INTC_G2R2_INT_MASK 0x50

/***************************************
 *
 * confirm the AXI bus clock frequency
 *
 ***************************************/
#if !(IS_ENABLED(CONFIG_MIISP_CHIP))
/* 40MHz for FPGA */
#define BUSMON_AXI_CLK_PCIE (40000000)
#define BUSMON_AXI_CLK_MIPI (40000000)
#define BUSMON_AXI_CLK_CPU (40000000)
#define BUSMON_AXI_CLK_DMA (40000000)
#define BUSMON_AXI_CLK_OCM (40000000)
#define BUSMON_AXI_CLK_NPU (40000000)
#define BUSMON_AXI_CLK_ISP_NIC0 (40000000)
#define BUSMON_AXI_CLK_ISP_NIC1 (40000000)
#define BUSMON_AXI_CLK_OCM_NIC (40000000)
#define BUSMON_AXI_CLK_CPU_NIC (40000000)
#define BUSMON_AXI_CLK_MAIN_NIC (40000000)
#define BUSMON_AXI_CLK_ISP_FE0 (40000000)
#define BUSMON_AXI_CLK_ISP_FE1 (40000000)
#define BUSMON_AXI_CLK_ISP_CVP (40000000)
#define BUSMON_AXI_CLK_ISP_ROUTER (40000000)
#define BUSMON_AXI_CLK_ISP_BEF (40000000)
#define BUSMON_AXI_CLK_ISP_BEB (40000000)
#define BUSMON_AXI_CLK_ISP_NIC (40000000)
#define BUSMON_AXI_CLK_ISP_CMDDMA (40000000)
#else /* EMU & ASIC */
#define BUSMON_AXI_CLK_PCIE (533000000)
#define BUSMON_AXI_CLK_MIPI (533000000)
#define BUSMON_AXI_CLK_CPU (533000000)
#define BUSMON_AXI_CLK_DMA (533000000)
#define BUSMON_AXI_CLK_OCM (533000000)
#define BUSMON_AXI_CLK_NPU (533000000)
#define BUSMON_AXI_CLK_ISP_NIC0 (533000000)
#define BUSMON_AXI_CLK_ISP_NIC1 (533000000)
#define BUSMON_AXI_CLK_OCM_NIC (533000000)
#define BUSMON_AXI_CLK_CPU_NIC (533000000)
#define BUSMON_AXI_CLK_MAIN_NIC (533000000)
#define BUSMON_AXI_CLK_ISP_FE0 (533000000)
#define BUSMON_AXI_CLK_ISP_FE1 (533000000)
#define BUSMON_AXI_CLK_ISP_CVP (533000000)
#define BUSMON_AXI_CLK_ISP_ROUTER (533000000)
#define BUSMON_AXI_CLK_ISP_BEF (533000000)
#define BUSMON_AXI_CLK_ISP_BEB (533000000)
#define BUSMON_AXI_CLK_ISP_NIC (533000000)
#define BUSMON_AXI_CLK_ISP_CMDDMA (533000000)
#endif

#define BUSMON_AXI_WIDTH_PCIE (64)
#define BUSMON_AXI_WIDTH_MIPI (128)
#define BUSMON_AXI_WIDTH_CPU (64)
#define BUSMON_AXI_WIDTH_DMA (64)
#define BUSMON_AXI_WIDTH_OCM (128)
#define BUSMON_AXI_WIDTH_NPU (128)
#define BUSMON_AXI_WIDTH_ISP_NIC0 (128)
#define BUSMON_AXI_WIDTH_ISP_NIC1 (128)
#define BUSMON_AXI_WIDTH_OCM_NIC (128)
#define BUSMON_AXI_WIDTH_CPU_NIC (128)
#define BUSMON_AXI_WIDTH_MAIN_NIC (64)
#define BUSMON_AXI_WIDTH_ISP_FE0 (128)
#define BUSMON_AXI_WIDTH_ISP_FE1 (128)
#define BUSMON_AXI_WIDTH_ISP_CVP (128)
#define BUSMON_AXI_WIDTH_ISP_ROUTER (128)
#define BUSMON_AXI_WIDTH_ISP_BEF (128)
#define BUSMON_AXI_WIDTH_ISP_BEB (128)
#define BUSMON_AXI_WIDTH_ISP_NIC (128)
#define BUSMON_AXI_WIDTH_ISP_CMDDMA (64)

#define BUSMON_PERF_MEM_SIZE_MULTIPLE (0x10000) /* 64KB */

#define ISP_BUSMON_MATCH_MODE 0
#define ISP_BUSMON_PERFM_MODE 1
#define DDR_BUSMON_MATCH_MODE 1
#define DDR_BUSMON_PERFM_MODE 0

#define DDR_BUSMON_ARR_LEN 10
#define ISP_BUSMON_ARR_LEN 8

#define WR_CMD0_MATCH_GRP_SIZE (WR_CMD1_MATCH_OFFSET - WR_CMD0_MATCH_OFFSET)
#define WR_ADDR0_H_MATCH_GRP_SIZE                                              \
	(WR_ADDR1_H_MATCH_OFFSET - WR_ADDR0_H_MATCH_OFFSET)
#define WR_ADDR0_L_MATCH_GRP_SIZE                                              \
	(WR_ADDR1_L_MATCH_OFFSET - WR_ADDR0_L_MATCH_OFFSET)
#define WR_ID0_MATCH_GRP_SIZE (WR_ID1_MATCH_OFFSET - WR_ID0_MATCH_OFFSET)
#define RD_CMD0_MATCH_GRP_SIZE (RD_CMD1_MATCH_OFFSET - RD_CMD0_MATCH_OFFSET)
#define RD_ADDR0_H_MATCH_GRP_SIZE                                              \
	(RD_ADDR1_H_MATCH_OFFSET - RD_ADDR0_H_MATCH_OFFSET)
#define RD_ADDR0_L_MATCH_GRP_SIZE                                              \
	(RD_ADDR1_L_MATCH_OFFSET - RD_ADDR0_L_MATCH_OFFSET)
#define RD_ID0_MATCH_GRP_SIZE (RD_ID1_MATCH_OFFSET - RD_ID0_MATCH_OFFSET)

#define US_TO_CLK_CYC(us, freq) ((uint64_t)(us) * (freq) / 1000000)
#define CLK_CYC_TO_US(clk, freq) ((uint64_t)(clk)*1000000 / (freq))

#define PERFM_MIN_ADDR0_H_GRP_SIZE                                             \
	(PERFM_MIN_ADDR1_H_OFFSET - PERFM_MIN_ADDR0_H_OFFSET)
#define PERFM_MIN_ADDR0_L_GRP_SIZE                                             \
	(PERFM_MIN_ADDR1_L_OFFSET - PERFM_MIN_ADDR0_L_OFFSET)
#define PERFM_MAX_ADDR0_H_GRP_SIZE                                             \
	(PERFM_MAX_ADDR1_H_OFFSET - PERFM_MAX_ADDR0_H_OFFSET)
#define PERFM_MAX_ADDR0_L_GRP_SIZE                                             \
	(PERFM_MAX_ADDR1_L_OFFSET - PERFM_MAX_ADDR0_L_OFFSET)
#define PERFM_MIN_ID0_GRP_SIZE (PERFM_MIN_ID1_OFFSET - PERFM_MIN_ID0_OFFSET)
#define PERFM_MAX_ID0_GRP_SIZE (PERFM_MAX_ID1_OFFSET - PERFM_MAX_ID0_OFFSET)
#define PERFM_ADDR_EXC_EN0_GRP_BITS                                            \
	(PERFM_ADDR_EXC_EN1_SHIFT - PERFM_ADDR_EXC_EN0_SHIFT)
#define PERFM_ID_EXC_EN0_GRP_BITS                                              \
	(PERFM_ID_EXC_EN1_SHIFT - PERFM_ID_EXC_EN0_SHIFT)
#define PERFM_CNT0_MODE_GRP_BITS (PERFM_CNT1_MODE_SHIFT - PERFM_CNT0_MODE_SHIFT)

#define PERFM_CNT_GRP0_EN_SHIFT (0)
#define PERFM_CNT_GRP0_EN_WIDTH (1)
#define PERFM_CNT_GRP0_EN_GRP_BITS (1)

#define CNT0_WR_BW_OVFW_PERFM_GRP_BITS                                         \
	(CNT1_WR_BW_OVFW_PERFM_SHIFT - CNT0_WR_BW_OVFW_PERFM_SHIFT)
#define CNT0_RD_BW_OVFW_PERFM_GRP_BITS                                         \
	(CNT1_RD_BW_OVFW_PERFM_SHIFT - CNT0_RD_BW_OVFW_PERFM_SHIFT)
#define CNT0_WR_BW_PERFM_GRP_SIZE                                              \
	(CNT1_WR_BW_PERFM_OFFSET - CNT0_WR_BW_PERFM_OFFSET)
#define CNT0_RD_BW_PERFM_GRP_SIZE                                              \
	(CNT1_RD_BW_PERFM_OFFSET - CNT0_RD_BW_PERFM_OFFSET)
#define WR_ADDR0_H_GRP0_PERFM_GRP_SIZE                                         \
	(WR_ADDR0_H_GRP1_PERFM_OFFSET - WR_ADDR0_H_GRP0_PERFM_OFFSET)
#define WR_ADDR0_L_GRP0_PERFM_GRP_SIZE                                         \
	(WR_ADDR0_L_GRP1_PERFM_OFFSET - WR_ADDR0_L_GRP0_PERFM_OFFSET)
#define RD_ADDR0_H_GRP0_PERFM_GRP_SIZE                                         \
	(RD_ADDR0_H_GRP1_PERFM_OFFSET - RD_ADDR0_H_GRP0_PERFM_OFFSET)
#define RD_ADDR0_L_GRP0_PERFM_GRP_SIZE                                         \
	(RD_ADDR0_L_GRP1_PERFM_OFFSET - RD_ADDR0_L_GRP0_PERFM_OFFSET)
#define WR_ID0_GRP0_PERFM_GRP_SIZE                                             \
	(WR_ID0_GRP1_PERFM_OFFSET - WR_ID0_GRP0_PERFM_OFFSET)
#define RD_ID0_GRP0_PERFM_GRP_SIZE                                             \
	(RD_ID0_GRP1_PERFM_OFFSET - RD_ID0_GRP0_PERFM_OFFSET)

#define CNT0_WR_CMD_OVFW_PERFM_GRP_BITS                                        \
	(CNT1_WR_CMD_OVFW_PERFM_SHIFT - CNT0_WR_CMD_OVFW_PERFM_SHIFT)
#define CNT0_RD_CMD_OVFW_PERFM_GRP_BITS                                        \
	(CNT1_RD_CMD_OVFW_PERFM_SHIFT - CNT0_RD_CMD_OVFW_PERFM_SHIFT)
#define CNT0_WR_CMD_PERFM_GRP_SIZE                                             \
	(CNT1_WR_CMD_PERFM_OFFSET - CNT0_WR_CMD_PERFM_OFFSET)
#define CNT0_RD_CMD_PERFM_GRP_SIZE                                             \
	(CNT1_RD_CMD_PERFM_OFFSET - CNT0_RD_CMD_PERFM_OFFSET)

struct debugfs_com {
	u32 debug_min_addr;
	u32 debug_max_addr;
	u32 debug_min_id;
	u32 debug_max_id;
	u32 debug_chn;
	u32 addr_exc;
	u32 id_exc;
	/*perfm*/
	u32 len_us;
	u32 output_option;
	u32 perf_param_cnt;
	u32 perf_mem_size;
	u32 perf_chn;

	u32 perf_min_addr;
	u32 perf_max_addr;
	u32 perf_min_id;
	u32 perf_max_id;
	u32 perf_count_mode;
	u32 perf_enable;
	u32 perf_addr_exc;
	u32 perf_id_exc;

	struct dentry *busmon_debugfs;
	struct dentry *perfm_debugfs_menu;
	struct dentry *match_debugfs_menu;
};

const int ddr_busmon_bit[DDR_BUSMON_ARR_LEN] = { 1, 2,	4,  5,	7,
						 8, 10, 11, 13, 14 };

const int isp_busmon_bit[ISP_BUSMON_ARR_LEN] = { 0, 1, 3, 4, 6, 7, 9, 10 };

#endif
