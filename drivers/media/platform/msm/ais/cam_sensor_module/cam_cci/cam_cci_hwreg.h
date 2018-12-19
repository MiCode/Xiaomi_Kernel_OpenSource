/* Copyright (c) 2012-2015, 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_CCI_HWREG_
#define _CAM_CCI_HWREG_

#define CCI_HW_VERSION_ADDR                                         0x00000000
#define CCI_RESET_CMD_ADDR                                          0x00000004
#define CCI_RESET_CMD_RMSK                                          0x0f73f3f7
#define CCI_M0_RESET_RMSK                                                0x3F1
#define CCI_M1_RESET_RMSK                                              0x3F001
#define CCI_QUEUE_START_ADDR                                        0x00000008
#define CCI_SET_CID_SYNC_TIMER_ADDR                                 0x00000010
#define CCI_SET_CID_SYNC_TIMER_OFFSET                               0x00000004
#define CCI_I2C_M0_SCL_CTL_ADDR                                     0x00000100
#define CCI_I2C_M0_SDA_CTL_0_ADDR                                   0x00000104
#define CCI_I2C_M0_SDA_CTL_1_ADDR                                   0x00000108
#define CCI_I2C_M0_SDA_CTL_2_ADDR                                   0x0000010c
#define CCI_I2C_M0_READ_DATA_ADDR                                   0x00000118
#define CCI_I2C_M0_MISC_CTL_ADDR                                    0x00000110
#define CCI_I2C_M0_READ_BUF_LEVEL_ADDR                              0x0000011C
#define CCI_HALT_REQ_ADDR                                           0x00000034
#define CCI_M0_HALT_REQ_RMSK                                               0x1
#define CCI_M1_HALT_REQ_RMSK                                               0x2
#define CCI_I2C_M1_SCL_CTL_ADDR                                     0x00000200
#define CCI_I2C_M1_SDA_CTL_0_ADDR                                   0x00000204
#define CCI_I2C_M1_SDA_CTL_1_ADDR                                   0x00000208
#define CCI_I2C_M1_SDA_CTL_2_ADDR                                   0x0000020c
#define CCI_I2C_M1_MISC_CTL_ADDR                                    0x00000210
#define CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR                             0x00000304
#define CCI_I2C_M0_Q0_CUR_CMD_ADDR                                  0x00000308
#define CCI_I2C_M0_Q0_REPORT_STATUS_ADDR                            0x0000030c
#define CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR                            0x00000300
#define CCI_I2C_M0_Q0_LOAD_DATA_ADDR                                0x00000310
#define CCI_IRQ_MASK_0_ADDR                                         0x00000c04
#define CCI_IRQ_MASK_0_RMSK                                         0x7fff7ff7
#define CCI_IRQ_MASK_1_ADDR                                         0x00000c10
#define CCI_IRQ_MASK_1_RMSK                                         0x00110000
#define CCI_IRQ_CLEAR_0_ADDR                                        0x00000c08
#define CCI_IRQ_CLEAR_1_ADDR                                        0x00000c14
#define CCI_IRQ_STATUS_0_ADDR                                       0x00000c0c
#define CCI_IRQ_STATUS_1_ADDR                                       0x00000c18
#define CCI_IRQ_STATUS_0_I2C_M1_Q0Q1_HALT_ACK_BMSK                   0x4000000
#define CCI_IRQ_STATUS_0_I2C_M0_Q0Q1_HALT_ACK_BMSK                   0x2000000
#define CCI_IRQ_STATUS_0_RST_DONE_ACK_BMSK                           0x1000000
#define CCI_IRQ_STATUS_0_I2C_M1_Q1_REPORT_BMSK                        0x100000
#define CCI_IRQ_STATUS_0_I2C_M1_Q0_REPORT_BMSK                         0x10000
#define CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK                            0x1000
#define CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD                          0x100000
#define CCI_IRQ_STATUS_1_I2C_M1_RD_PAUSE                              0x200000
#define CCI_IRQ_STATUS_0_I2C_M0_Q1_REPORT_BMSK                           0x100
#define CCI_IRQ_STATUS_0_I2C_M0_Q0_REPORT_BMSK                            0x10
#define CCI_IRQ_STATUS_0_I2C_M0_ERROR_BMSK                          0x18000EE6
#define CCI_IRQ_STATUS_0_I2C_M1_ERROR_BMSK                          0x60EE6000
#define CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK                               0x1
#define CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD                           0x10000
#define CCI_IRQ_STATUS_1_I2C_M0_RD_PAUSE                               0x20000
#define CCI_I2C_M0_RD_THRESHOLD_ADDR                                0x00000120
#define CCI_I2C_M1_RD_THRESHOLD_ADDR                                0x00000220
#define CCI_I2C_RD_THRESHOLD_VALUE                                        0x30
#define CCI_IRQ_GLOBAL_CLEAR_CMD_ADDR                               0x00000c00

#define DEBUG_TOP_REG_START                                                0x0
#define DEBUG_TOP_REG_COUNT                                                 14
#define DEBUG_MASTER_REG_START                                           0x100
#define DEBUG_MASTER_REG_COUNT                                               9
#define DEBUG_MASTER_QUEUE_REG_START                                     0x300
#define DEBUG_MASTER_QUEUE_REG_COUNT                                         7
#define DEBUG_INTR_REG_START                                             0xC00
#define DEBUG_INTR_REG_COUNT                                                 7
#endif /* _CAM_CCI_HWREG_ */
