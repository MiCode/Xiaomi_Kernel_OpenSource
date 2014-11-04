/*
 * Support for Medfield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * Copyright (c) 2010 Silicon Hive www.siliconhive.com.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef _hive_isp_css_defs_h
#define _hive_isp_css_defs_h

#define HIVE_ISP_CTRL_DATA_WIDTH     32
#define HIVE_ISP_CTRL_ADDRESS_WIDTH  32
#define HIVE_ISP_CTRL_MAX_BURST_SIZE  1
#define HIVE_ISP_NUM_GPIO_PINS       10

/* This list of vector num_elems/elem_bits pairs is valid both
   in C as initializer and in the DMA parameter list */
#define HIVE_ISP_DDR_DMA_SPECS \
    { {32,  8}, {16, 16}, {18, 14}, {25, 10}, {21, 12} }
#define HIVE_ISP_DDR_WORD_BITS      256
#define HIVE_ISP_DDR_WORD_BYTES     (HIVE_ISP_DDR_WORD_BITS/8)
#define HIVE_ISP_DDR_BYTES          (256 * 1024 * 1024)
#define HIVE_ISP_DDR_BYTES_RTL      ( 16 * 1024 * 1024)

#define HIVE_ISP_PAGE_SHIFT         12
#define HIVE_ISP_PAGE_SIZE          (1<<HIVE_ISP_PAGE_SHIFT)

/* If HIVE_ISP_DDR_BASE_OFFSET is set to a non-zero value,
 * the wide bus just before the DDRAM gets an extra dummy port where
 * address range 0 .. HIVE_ISP_DDR_BASE_OFFSET-1 maps onto.
 * This effectively creates an offset for the DDRAM from system perspective.
 */
#define HIVE_ISP_DDR_BASE_OFFSET 0 /* 0x200000 */

#define HIVE_DMA_ISP_BUS_CONN 0
#define HIVE_DMA_ISP_DDR_CONN 1
#define HIVE_DMA_BUS_DDR_CONN 2
#define HIVE_DMA_ISP_MASTER master_port0
#define HIVE_DMA_BUS_MASTER master_port1
#define HIVE_DMA_DDR_MASTER master_port2
#define HIVE_DMA_NUM_CHANNELS       8

#define HIVE_IF_PIXEL_WIDTH 12

#define HIVE_MMU_TLB_SETS           16
#define HIVE_MMU_TLB_SET_BLOCKS     8
#define HIVE_MMU_TLB_BLOCK_ELEMENTS 8
#define HIVE_MMU_PAGE_TABLE_LEVELS  2
#define HIVE_MMU_PAGE_BYTES         HIVE_ISP_PAGE_SIZE

#define HIVE_ISP_CH_ID_BITS    2
#define HIVE_ISP_FMT_TYPE_BITS 5
#define HIVE_ISP_ISEL_SEL_BITS 2

/* gp_register register id's   */
#define HIVE_GP_REGS_SWITCH_IF_IDX                              0
#define HIVE_GP_REGS_SWITCH_DMA_IDX                             1
#define HIVE_GP_REGS_SWITCH_GDC_IDX                             2

#define HIVE_GP_REGS_SYNCGEN_ENABLE_IDX                         3
#define HIVE_GP_REGS_SYNCGEN_NR_PIX_IDX                         4
#define HIVE_GP_REGS_SYNCGEN_NR_LINES_IDX                       5
#define HIVE_GP_REGS_SYNCGEN_HBLANK_CYCLES_IDX                  6
#define HIVE_GP_REGS_SYNCGEN_VBLANK_CYCLES_IDX                  7

#define HIVE_GP_REGS_ISEL_SOF_IDX                               8
#define HIVE_GP_REGS_ISEL_EOF_IDX                               9
#define HIVE_GP_REGS_ISEL_SOL_IDX                              10
#define HIVE_GP_REGS_ISEL_EOL_IDX                              11

#define HIVE_GP_REGS_PRBS_ENABLE                               12
#define HIVE_GP_REGS_PRBS_ENABLE_PORT_B                        13
#define HIVE_GP_REGS_PRBS_LFSR_RESET_VALUE                     14

#define HIVE_GP_REGS_TPG_ENABLE                                15
#define HIVE_GP_REGS_TPG_ENABLE_PORT_B                         16
#define HIVE_GP_REGS_TPG_HOR_CNT_MASK_IDX                      17
#define HIVE_GP_REGS_TPG_VER_CNT_MASK_IDX                      18
#define HIVE_GP_REGS_TPG_XY_CNT_MASK_IDX                       19
#define HIVE_GP_REGS_TPG_HOR_CNT_DELTA_IDX                     20
#define HIVE_GP_REGS_TPG_VER_CNT_DELTA_IDX                     21

#define HIVE_GP_REGS_ISEL_CH_ID_IDX                            22
#define HIVE_GP_REGS_ISEL_FMT_TYPE_IDX                         23
#define HIVE_GP_REGS_ISEL_DATA_SEL_IDX                         24
#define HIVE_GP_REGS_ISEL_SBAND_SEL_IDX                        25
#define HIVE_GP_REGS_ISEL_SYNC_SEL_IDX                         26

/* HIVE_GP_REGS_INPUT_SWITCH_LUT_REG_? have to be sequential ! */
#define HIVE_GP_REGS_INPUT_SWITCH_LUT_REG_0                    27
#define HIVE_GP_REGS_INPUT_SWITCH_LUT_REG_1                    28
#define HIVE_GP_REGS_INPUT_SWITCH_LUT_REG_2                    29
#define HIVE_GP_REGS_INPUT_SWITCH_LUT_REG_3                    30
#define HIVE_GP_REGS_INPUT_SWITCH_LUT_REG_4                    31
#define HIVE_GP_REGS_INPUT_SWITCH_LUT_REG_5                    32
#define HIVE_GP_REGS_INPUT_SWITCH_LUT_REG_6                    33
#define HIVE_GP_REGS_INPUT_SWITCH_LUT_REG_7                    34

#define HIVE_GP_REGS_INPUT_SWITCH_LUT_REG_BASE \
	HIVE_GP_REGS_INPUT_SWITCH_LUT_REG_0
#define HIVE_GP_REGS_INPUT_SWITCH_FSYNC_LUT_REG                35

#define HIVE_GP_REGS_SDRAM_WAKEUP_IDX                          36
#define HIVE_GP_REGS_IDLE_IDX                                  37
#define HIVE_GP_REGS_IRQ_IDX                                   38

#define HIVE_GP_REGS_MIPI_FIFO_FULL                            39
#define HIVE_GP_REGS_MIPI_USED_DWORD                           40

#define HIVE_GP_REGS_SP_STREAM_STAT                            41
#define HIVE_GP_REGS_MOD_STREAM_STAT                           42
#define HIVE_GP_REGS_ISP_STREAM_STAT                           43

#define HIVE_GP_REGS_CH_ID_FMT_TYPE_IDX                        44

/* order of the input bits for the irq controller */
#define HIVE_TESTBENCH_IRQ_SOURCE_GPIO_PIN_0_BIT_ID             0
#define HIVE_TESTBENCH_IRQ_SOURCE_GPIO_PIN_1_BIT_ID             1
#define HIVE_TESTBENCH_IRQ_SOURCE_GPIO_PIN_2_BIT_ID             2
#define HIVE_TESTBENCH_IRQ_SOURCE_GPIO_PIN_3_BIT_ID             3
#define HIVE_TESTBENCH_IRQ_SOURCE_GPIO_PIN_4_BIT_ID             4
#define HIVE_TESTBENCH_IRQ_SOURCE_GPIO_PIN_5_BIT_ID             5
#define HIVE_TESTBENCH_IRQ_SOURCE_GPIO_PIN_6_BIT_ID             6
#define HIVE_TESTBENCH_IRQ_SOURCE_GPIO_PIN_7_BIT_ID             7
#define HIVE_TESTBENCH_IRQ_SOURCE_GPIO_PIN_8_BIT_ID             8
#define HIVE_TESTBENCH_IRQ_SOURCE_GPIO_PIN_9_BIT_ID             9
#define HIVE_TESTBENCH_IRQ_SOURCE_SP_BIT_ID                    10
#define HIVE_TESTBENCH_IRQ_SOURCE_ISP_BIT_ID                   11
#define HIVE_TESTBENCH_IRQ_SOURCE_MIPI_BIT_ID                  12
#define HIVE_TESTBENCH_IRQ_SOURCE_PRIM_IF_BIT_ID               13
#define HIVE_TESTBENCH_IRQ_SOURCE_PRIM_B_IF_BIT_ID             14
#define HIVE_TESTBENCH_IRQ_SOURCE_SEC_IF_BIT_ID                15
#define HIVE_TESTBENCH_IRQ_SOURCE_MEM_COPY_BIT_ID              16
#define HIVE_TESTBENCH_IRQ_SOURCE_MIPI_FIFO_FULL_BIT_ID        17
#define HIVE_TESTBENCH_IRQ_SOURCE_MIPI_SOF_BIT_ID              18
#define HIVE_TESTBENCH_IRQ_SOURCE_MIPI_EOF_BIT_ID              19
#define HIVE_TESTBENCH_IRQ_SOURCE_MIPI_SOL_BIT_ID              20
#define HIVE_TESTBENCH_IRQ_SOURCE_MIPI_EOL_BIT_ID              21
#define HIVE_TESTBENCH_IRQ_SOURCE_SYNC_GEN_SOF_BIT_ID          22
#define HIVE_TESTBENCH_IRQ_SOURCE_SYNC_GEN_EOF_BIT_ID          23
#define HIVE_TESTBENCH_IRQ_SOURCE_SYNC_GEN_SOL_BIT_ID          24
#define HIVE_TESTBENCH_IRQ_SOURCE_SYNC_GEN_EOL_BIT_ID          25
#define HIVE_TESTBENCH_IRQ_SOURCE_CSS_GEN_SHORT_VALID_BIT_ID   26
#define HIVE_TESTBENCH_IRQ_SOURCE_CSS_GEN_SHORT_ACCEPT_BIT_ID  27
#define HIVE_TESTBENCH_IRQ_SOURCE_SIDEBAND_CHANGED_BIT_ID      28

#define HIVE_TESTBENCH_IRQ_SOURCE_SW_PIN_0_BIT_ID              29
#define HIVE_TESTBENCH_IRQ_SOURCE_SW_PIN_1_BIT_ID              30
#define HIVE_TESTBENCH_IRQ_SOURCE_SW_PIN_2_BIT_ID              31

#define HIVE_ISP_NUM_USED_IRQ_INPUTS \
	(HIVE_TESTBENCH_IRQ_SOURCE_SW_PIN_0_BIT_ID - \
	 HIVE_TESTBENCH_IRQ_SOURCE_SP_BIT_ID)

#define HIVE_GP_REGS_IRQ_REG_WIDTH \
	(HIVE_ISP_CTRL_DATA_WIDTH - \
	 HIVE_TESTBENCH_IRQ_SOURCE_SW_PIN_0_BIT_ID)

/* testbench signals:       */

/* GP adapter register ids  */
#define HIVE_TESTBENCH_GPIO_DATA_OUT_REG_IDX                    0
#define HIVE_TESTBENCH_GPIO_DIR_OUT_REG_IDX                     1
#define HIVE_TESTBENCH_IRQ_REG_IDX                              2
#define HIVE_TESTBENCH_SDRAM_WAKEUP_REG_IDX                     3
#define HIVE_TESTBENCH_IDLE_REG_IDX                             4
#define HIVE_TESTBENCH_GPIO_DATA_IN_REG_IDX                     5

/* Signal monitor input bit ids */
#define HIVE_TESTBENCH_SIG_MON_SOURCE_GPIO_PIN_O_BIT_ID         0
#define HIVE_TESTBENCH_SIG_MON_SOURCE_GPIO_PIN_1_BIT_ID         1
#define HIVE_TESTBENCH_SIG_MON_SOURCE_GPIO_PIN_2_BIT_ID         2
#define HIVE_TESTBENCH_SIG_MON_SOURCE_GPIO_PIN_3_BIT_ID         3
#define HIVE_TESTBENCH_SIG_MON_SOURCE_GPIO_PIN_4_BIT_ID         4
#define HIVE_TESTBENCH_SIG_MON_SOURCE_GPIO_PIN_5_BIT_ID         5
#define HIVE_TESTBENCH_SIG_MON_SOURCE_GPIO_PIN_6_BIT_ID         6
#define HIVE_TESTBENCH_SIG_MON_SOURCE_GPIO_PIN_7_BIT_ID         7
#define HIVE_TESTBENCH_SIG_MON_SOURCE_GPIO_PIN_8_BIT_ID         8
#define HIVE_TESTBENCH_SIG_MON_SOURCE_GPIO_PIN_9_BIT_ID         9
#define HIVE_TESTBENCH_SIG_MON_SOURCE_IRQ_PIN_BIT_ID           10
#define HIVE_TESTBENCH_SIG_MON_SOURCE_SDRAM_WAKEUP_PIN_BIT_ID  11
#define HIVE_TESTBENCH_SIG_MON_SOURCE_IDLE_PIN_BIT_ID          12

#endif /* _hive_isp_css_defs_h */
