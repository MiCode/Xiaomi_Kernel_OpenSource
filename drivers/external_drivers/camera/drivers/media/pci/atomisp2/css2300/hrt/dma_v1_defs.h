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

#ifndef _dma_v1_defs_h
#define _dma_v1_defs_h

#define _DMA_V1_NUM_CHANNELS_ID               MaxNumChannels
#define _DMA_V1_CONNECTIONS_ID                Connections
#define _DMA_V1_DEV_ELEM_WIDTHS_ID            DevElemWidths
#define _DMA_V1_DEV_FIFO_DEPTH_ID             DevFifoDepth
#define _DMA_V1_DEV_FIFO_RD_LAT_ID            DevFifoRdLat
#define _DMA_V1_DEV_FIFO_LAT_BYPASS_ID        DevFifoRdLatBypass
#define _DMA_V1_DEV_2_CIO_ID                  DevConnectedToCIO
#define _DMA_V1_DEV_HAS_CRUN_ID               CRunMasters
#define _DMA_V1_CONN_GROUPS_ID                ConnectionGroups
#define _DMA_V1_CONN_GROUP_FIFO_DEPTH_ID      ConnectionGroupFifoDepth
#define _DMA_V1_CONN_GROUP_FIFO_RD_LAT_ID     ConnectionGroupFifoRdLat
#define _DMA_V1_CONN_GROUP_FIFO_LAT_BYPASS_ID ConnectionGroupFifoRdLatBypass

#define _DMA_V1_REG_ALIGN                4
#define _DMA_V1_REG_ADDR_BITS            2

/* Command word */
#define _DMA_CMD_IDX         0
#define _DMA_CMD_BITS        4
#define _DMA_CHANNEL_IDX     (_DMA_CMD_IDX + _DMA_CMD_BITS)
#define _DMA_CHANNEL_BITS    8
#define _DMA_PARAM_IDX       (_DMA_CHANNEL_IDX + _DMA_CHANNEL_BITS)
#define _DMA_PARAM_BITS      4
#define _DMA_CRUN_IDX        (_DMA_PARAM_IDX + _DMA_PARAM_BITS)
#define _DMA_CRUN_BITS       1

/* Packing setup word */
#define _DMA_CONNECTION_IDX  0
#define _DMA_CONNECTION_BITS 8
#define _DMA_EXTENSION_IDX   (_DMA_CONNECTION_IDX + _DMA_CONNECTION_BITS)
#define _DMA_EXTENSION_BITS  4
#define _DMA_ELEM_ORDER_IDX  (_DMA_EXTENSION_IDX + _DMA_EXTENSION_BITS)
#define _DMA_ELEM_ORDER_BITS 4

/* Elements packing word */
#define _DMA_ELEMENTS_IDX        0
#define _DMA_ELEMENTS_BITS      12
#define _DMA_LEFT_CROPPING_IDX  (_DMA_ELEMENTS_IDX + _DMA_ELEMENTS_BITS)
#define _DMA_LEFT_CROPPING_BITS 12

#define _DMA_WIDTH_IDX   0
#define _DMA_WIDTH_BITS 16

#define _DMA_HEIGHT_IDX   0
#define _DMA_HEIGHT_BITS 16

#define _DMA_STRIDE_IDX   0
#define _DMA_STRIDE_BITS 32

/* Command IDs */
#define _DMA_READ_COMMAND              0
#define _DMA_WRITE_COMMAND             1
#define _DMA_CONFIG_CHANNEL_COMMAND    2
#define _DMA_SET_CHANNEL_PARAM_COMMAND 3
#define _DMA_INIT_COMMAND              8
#define _DMA_RESET_COMMAND            15

/* Channel Parameter IDs */
#define _DMA_PACKING_SETUP_PARAM   0
#define _DMA_STRIDE_A_PARAM        1
#define _DMA_ELEM_CROPPING_A_PARAM 2
#define _DMA_WIDTH_A_PARAM         3
#define _DMA_STRIDE_B_PARAM        4
#define _DMA_ELEM_CROPPING_B_PARAM 5
#define _DMA_WIDTH_B_PARAM         6
#define _DMA_HEIGHT_PARAM          7

/* Parameter Constants */
#define _DMA_ZERO_EXTEND     0
#define _DMA_SIGN_EXTEND     1
#define _DMA_REVERSE_ELEMS   1
#define _DMA_KEEP_ELEM_ORDER 0

  /* SLAVE address map */
#define _DMA_SEL_FSM_CMD                           0
#define _DMA_SEL_CH_REG                            1
#define _DMA_SEL_CONN_GROUP                        2
#define _DMA_SEL_DEV_INTERF                        3
#define _DMA_SEL_RESET                             15

#define _DMA_RESET_TOKEN                  0xDEADCAFE

#define _DMA_SEL_CONN_CMD                          0
#define _DMA_SEL_CONN_ADDRESS_A                    1
#define _DMA_SEL_CONN_ADDRESS_B                    2
#define _DMA_SEL_FSM_CONN_CTRL                     3
#define _DMA_SEL_FSM_PACK                          4
#define _DMA_SEL_FSM_REQ                           5
#define _DMA_SEL_FSM_WR                            6
  
#define _DMA_ADDR_SEL_COMP_IDX                    12
#define _DMA_ADDR_SEL_COMP_BITS                    4
#define _DMA_ADDR_SEL_CH_REG_IDX                   2
#define _DMA_ADDR_SEL_CH_REG_BITS                  6
#define _DMA_ADDR_SEL_PARAM_IDX                    8
#define _DMA_ADDR_SEL_PARAM_BITS                   4

#define _DMA_ADDR_SEL_GROUP_IDX                    2
#define _DMA_ADDR_SEL_GROUP_BITS                   3
#define _DMA_ADDR_SEL_GROUP_COMP_IDX               5
#define _DMA_ADDR_SEL_GROUP_COMP_BITS              3
#define _DMA_ADDR_SEL_GROUP_COMP_INFO_IDX          8
#define _DMA_ADDR_SEL_GROUP_COMP_INFO_BITS         4

#define _DMA_ADDR_SEL_DEV_INTERF_IDX_IDX           2
#define _DMA_ADDR_SEL_DEV_INTERF_IDX_BITS          6
#define _DMA_ADDR_SEL_DEV_INTERF_INFO_IDX          8
#define _DMA_ADDR_SEL_DEV_INTERF_INFO_BITS         4

#define _DMA_FSM_GROUP_CMD_IDX                     0
#define _DMA_FSM_GROUP_ADDR_A_IDX                  1
#define _DMA_FSM_GROUP_ADDR_B_IDX                  2
#define _DMA_FSM_GROUP_FSM_CTRL_IDX                3
#define _DMA_FSM_GROUP_FSM_PACK_IDX                4
#define _DMA_FSM_GROUP_FSM_REQ_IDX                 5
#define _DMA_FSM_GROUP_FSM_WR_IDX                  6
  
#define _DMA_FSM_GROUP_FSM_CTRL_STATE_IDX          0
#define _DMA_FSM_GROUP_FSM_CTRL_REQ_DEV_IDX        1
#define _DMA_FSM_GROUP_FSM_CTRL_REQ_ADDR_IDX       2
#define _DMA_FSM_GROUP_FSM_CTRL_REQ_STRIDE_IDX     3
#define _DMA_FSM_GROUP_FSM_CTRL_REQ_XB_IDX         4
#define _DMA_FSM_GROUP_FSM_CTRL_REQ_YB_IDX         5
#define _DMA_FSM_GROUP_FSM_CTRL_PACK_REQ_DEV_IDX   6
#define _DMA_FSM_GROUP_FSM_CTRL_PACK_WR_DEV_IDX    7
#define _DMA_FSM_GROUP_FSM_CTRL_WR_ADDR_IDX        8
#define _DMA_FSM_GROUP_FSM_CTRL_WR_STRIDE_IDX      9
#define _DMA_FSM_GROUP_FSM_CTRL_PACK_REQ_XB_IDX   10
#define _DMA_FSM_GROUP_FSM_CTRL_PACK_WR_YB_IDX    11
#define _DMA_FSM_GROUP_FSM_CTRL_PACK_WR_XB_IDX    12
#define _DMA_FSM_GROUP_FSM_CTRL_PACK_ELEM_REQ_IDX 13
#define _DMA_FSM_GROUP_FSM_CTRL_PACK_ELEM_WR_IDX  14
#define _DMA_FSM_GROUP_FSM_CTRL_PACK_S_Z_REV_IDX  15

#define _DMA_FSM_GROUP_FSM_PACK_STATE_IDX          0
#define _DMA_FSM_GROUP_FSM_PACK_CNT_YB_IDX         1
#define _DMA_FSM_GROUP_FSM_PACK_CNT_XB_REQ_IDX     2
#define _DMA_FSM_GROUP_FSM_PACK_CNT_XB_WR_IDX      3

#define _DMA_FSM_GROUP_FSM_REQ_STATE_IDX           0
#define _DMA_FSM_GROUP_FSM_REQ_CNT_YB_IDX          1
#define _DMA_FSM_GROUP_FSM_REQ_CNT_XB_IDX          2

#define _DMA_FSM_GROUP_FSM_WR_STATE_IDX            0
#define _DMA_FSM_GROUP_FSM_WR_CNT_YB_IDX           1
#define _DMA_FSM_GROUP_FSM_WR_CNT_XB_IDX           2

#define _DMA_DEV_INTERF_REQ_SIDE_STATUS_IDX        0
#define _DMA_DEV_INTERF_SEND_SIDE_STATUS_IDX       1
#define _DMA_DEV_INTERF_FIFO_STATUS_IDX            2
#define _DMA_DEV_INTERF_MAX_BURST_IDX              3
#define _DMA_DEV_INTERF_CHK_ADDR_ALIGN             4

#endif /* _dma_v1_defs_h */
