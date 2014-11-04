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

#ifndef _css_receiver_ahb_defs_h_
#define _css_receiver_ahb_defs_h_

#define CSS_RECEIVER_DATA_WIDTH                8
#define CSS_RECEIVER_RX_TRIG                   4
#define CSS_RECEIVER_RF_WORD                  32
#define CSS_RECEIVER_IMG_PROC_RF_ADDR         10
#define CSS_RECEIVER_CSI_RF_ADDR               4
#define CSS_RECEIVER_DATA_OUT                 12
#define CSS_RECEIVER_CHN_NO                    2
#define CSS_RECEIVER_DWORD_CNT                11
#define CSS_RECEIVER_FORMAT_TYP                5
#define CSS_RECEIVER_HRESPONSE                 2
#define CSS_RECEIVER_STATE_WIDTH               3
#define CSS_RECEIVER_FIFO_DAT                 32
#define CSS_RECEIVER_CNT_VAL                   2
#define CSS_RECEIVER_PRED10_VAL               10
#define CSS_RECEIVER_PRED12_VAL               12
#define CSS_RECEIVER_CNT_WIDTH                 8
#define CSS_RECEIVER_WORD_CNT                 16
#define CSS_RECEIVER_PIXEL_LEN                 6
#define CSS_RECEIVER_PIXEL_CNT                 5
#define CSS_RECEIVER_COMP_8_BIT                8
#define CSS_RECEIVER_COMP_7_BIT                7
#define CSS_RECEIVER_COMP_6_BIT                6
#define CSS_RECEIVER_GEN_SHORT_DATA_WIDTH     16
#define CSS_RECEIVER_GEN_SHORT_CH_ID_WIDTH     2
#define CSS_RECEIVER_GEN_SHORT_FMT_TYPE_WIDTH  3
#define CSS_RECEIVER_GEN_SHORT_STR_REAL_WIDTH (CSS_RECEIVER_GEN_SHORT_DATA_WIDTH + CSS_RECEIVER_GEN_SHORT_CH_ID_WIDTH + CSS_RECEIVER_GEN_SHORT_FMT_TYPE_WIDTH)
#define CSS_RECEIVER_GEN_SHORT_STR_WIDTH      32 /* use 32 to be compatibel with streaming monitor !, MSB's of interface are tied to '0' */ 

/* division of gen_short data, ch_id and fmt_type over streaming data interface */
#define CSS_RECEIVER_GEN_SHORT_STR_DATA_BIT_LSB     0
#define CSS_RECEIVER_GEN_SHORT_STR_FMT_TYPE_BIT_LSB (CSS_RECEIVER_GEN_SHORT_STR_DATA_BIT_LSB     + CSS_RECEIVER_GEN_SHORT_DATA_WIDTH)
#define CSS_RECEIVER_GEN_SHORT_STR_CH_ID_BIT_LSB    (CSS_RECEIVER_GEN_SHORT_STR_FMT_TYPE_BIT_LSB + CSS_RECEIVER_GEN_SHORT_FMT_TYPE_WIDTH)
#define CSS_RECEIVER_GEN_SHORT_STR_DATA_BIT_MSB     (CSS_RECEIVER_GEN_SHORT_STR_FMT_TYPE_BIT_LSB - 1)
#define CSS_RECEIVER_GEN_SHORT_STR_FMT_TYPE_BIT_MSB (CSS_RECEIVER_GEN_SHORT_STR_CH_ID_BIT_LSB    - 1)
#define CSS_RECEIVER_GEN_SHORT_STR_CH_ID_BIT_MSB    (CSS_RECEIVER_GEN_SHORT_STR_REAL_WIDTH       - 1)

#define _HRT_CSS_RECEIVER_AHB_REG_ALIGN 4

#define hrt_css_receiver_ahb_4_lane_port_offset 0x100
#define hrt_css_receiver_ahb_1_lane_port_offset 0x200

#define _HRT_CSS_RECEIVER_AHB_DEVICE_READY_REG_IDX      0
#define _HRT_CSS_RECEIVER_AHB_IRQ_STATUS_REG_IDX        1
#define _HRT_CSS_RECEIVER_AHB_IRQ_ENABLE_REG_IDX        2
#define _HRT_CSS_RECEIVER_AHB_CSI2_FUNC_PROG_REG_IDX    3
#define _HRT_CSS_RECEIVER_AHB_INIT_COUNT_REG_IDX        4
#define _HRT_CSS_RECEIVER_AHB_COMP_FORMAT_REG_IDX       5
#define _HRT_CSS_RECEIVER_AHB_COMP_PREDICT_REG_IDX      6
#define _HRT_CSS_RECEIVER_AHB_FS_TO_LS_DELAY_REG_IDX    7
#define _HRT_CSS_RECEIVER_AHB_LS_TO_DATA_DELAY_REG_IDX  8
#define _HRT_CSS_RECEIVER_AHB_DATA_TO_LE_DELAY_REG_IDX  9
#define _HRT_CSS_RECEIVER_AHB_LE_TO_FE_DELAY_REG_IDX   10
#define _HRT_CSS_RECEIVER_AHB_FE_TO_FS_DELAY_REG_IDX   11
#define _HRT_CSS_RECEIVER_AHB_LE_TO_LS_DELAY_REG_IDX   12
#define _HRT_CSS_RECEIVER_AHB_TWO_PIXEL_EN_REG_IDX     13

/* Interrupt bits for IRQ_STATUS and IRQ_ENABLE registers */
#define _HRT_CSS_RECEIVER_AHB_IRQ_OVERRUN_BIT                0
#define _HRT_CSS_RECEIVER_AHB_IRQ_RESERVED_BIT               1
#define _HRT_CSS_RECEIVER_AHB_IRQ_SLEEP_MODE_ENTRY_BIT       2
#define _HRT_CSS_RECEIVER_AHB_IRQ_SLEEP_MODE_EXIT_BIT        3
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_SOT_HS_BIT             4
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_SOT_SYNC_HS_BIT        5
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_CONTROL_BIT            6
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_ECC_DOUBLE_BIT         7
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_ECC_CORRECTED_BIT      8
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_ECC_NO_CORRECTION_BIT  9
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_CRC_BIT               10
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_ID_BIT                11
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_FRAME_SYNC_BIT        12
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_FRAME_DATA_BIT        13
#define _HRT_CSS_RECEIVER_AHB_IRQ_DATA_TIMEOUT_BIT          14
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_ESCAPE_BIT            15
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_LINE_SYNC_BIT         16

#define _HRT_CSS_RECEIVER_AHB_IRQ_OVERRUN_CAUSE_                  "Fifo Overrun"
#define _HRT_CSS_RECEIVER_AHB_IRQ_RESERVED_CAUSE_                 "Reserved"
#define _HRT_CSS_RECEIVER_AHB_IRQ_SLEEP_MODE_ENTRY_CAUSE_         "Sleep mode entry"
#define _HRT_CSS_RECEIVER_AHB_IRQ_SLEEP_MODE_EXIT_CAUSE_          "Sleep mode exit"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_SOT_HS_CAUSE_               "Error high speed SOT"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_SOT_SYNC_HS_CAUSE_          "Error high speed sync SOT"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_CONTROL_CAUSE_              "Error control"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_ECC_DOUBLE_CAUSE_           "Error correction double bit"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_ECC_CORRECTED_CAUSE_        "Error correction single bit"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_ECC_NO_CORRECTION_CAUSE_    "No error"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_CRC_CAUSE_                  "Error cyclic redundancy check"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_ID_CAUSE_                   "Error id"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_FRAME_SYNC_CAUSE_           "Error frame sync"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_FRAME_DATA_CAUSE_           "Error frame data"
#define _HRT_CSS_RECEIVER_AHB_IRQ_DATA_TIMEOUT_CAUSE_             "Data time-out"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_ESCAPE_CAUSE_               "Error escape"
#define _HRT_CSS_RECEIVER_AHB_IRQ_ERR_LINE_SYNC_CAUSE_            "Error line sync"
                                  
/* Bits for CSI2_FUNC_PROG register */
#define _HRT_CSS_RECEIVER_AHB_CSI2_NUM_DATA_LANES_IDX  0
#define _HRT_CSS_RECEIVER_AHB_CSI2_NUM_DATA_LANES_BITS 3
#define _HRT_CSS_RECEIVER_AHB_CSI2_DATA_TIMEOUT_IDX    (_HRT_CSS_RECEIVER_AHB_CSI2_NUM_DATA_LANES_IDX + _HRT_CSS_RECEIVER_AHB_CSI2_NUM_DATA_LANES_BITS)
#define _HRT_CSS_RECEIVER_AHB_CSI2_DATA_TIMEOUT_BITS   29

/* Bits for INIT_COUNT register */
#define _HRT_CSS_RECEIVER_AHB_INIT_TIMER_IDX  0
#define _HRT_CSS_RECEIVER_AHB_INIT_TIMER_BITS 16

/* Bits for COMP_FORMAT register, this selects the compression data format */
#define _HRT_CSS_RECEIVER_AHB_COMP_RAW_BITS_IDX  0
#define _HRT_CSS_RECEIVER_AHB_COMP_RAW_BITS_BITS 8
#define _HRT_CSS_RECEIVER_AHB_COMP_NUM_BITS_IDX  (_HRT_CSS_RECEIVER_AHB_COMP_RAW_BITS_IDX + _HRT_CSS_RECEIVER_AHB_COMP_RAW_BITS_BITS)
#define _HRT_CSS_RECEIVER_AHB_COMP_NUM_BITS_BITS 8

/* Bits for COMP_PREDICT register, this selects the predictor algorithm */
#define _HRT_CSS_RECEIVER_AHB_PREDICT_NO_COMP 0
#define _HRT_CSS_RECEIVER_AHB_PREDICT_1       1
#define _HRT_CSS_RECEIVER_AHB_PREDICT_2       2

/* Number of bits used for the delay registers */
#define _HRT_CSS_RECEIVER_AHB_DELAY_BITS 8

/* These hsync and vsync values are for HSS simulation only */
#define _HRT_CSS_RECEIVER_AHB_HSYNC_VAL (1<<16)
#define _HRT_CSS_RECEIVER_AHB_VSYNC_VAL (1<<17)

/* Definition of format_types */
/* !! Changes here should be copied to systems/isp/isp_css/bin/conv_transmitter_cmd.tcl !! */
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_RGB888           0 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_RGB555           1 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_RGB444           2 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_RGB565           3 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_RGB666           4 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_RAW8             5 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_RAW10            6 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_RAW6             7 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_RAW7             8 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_RAW12            9 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_RAW14           10 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_YUV420_8        11 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_YUV420_10       12 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_YUV422_8        13 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_YUV422_10       14 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_USR_DEF_1       15 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_YUV420_8L       16 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_Emb             17 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_USR_DEF_2       18 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_USR_DEF_3       19 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_USR_DEF_4       20 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_USR_DEF_5       21 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_USR_DEF_6       22 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_USR_DEF_7       23 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_USR_DEF_8       24 
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_YUV420_8_CSPS   25
#define _HRT_CSS_RECEIVER_AHB_FMT_TYPE_YUV420_10_CSPS  26


#endif /* _css_receiver_ahb_defs_h_ */
