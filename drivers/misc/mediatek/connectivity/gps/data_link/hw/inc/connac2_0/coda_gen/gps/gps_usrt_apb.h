/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __GPS_USRT_APB_REGS_H__
#define __GPS_USRT_APB_REGS_H__

#define GPS_USRT_APB_BASE                                      0x80073000

#define GPS_USRT_APB_GPS_APB_DATA_ADDR                         (GPS_USRT_APB_BASE + 0x0000)
#define GPS_USRT_APB_APB_CTRL_ADDR                             (GPS_USRT_APB_BASE + 0x0004)
#define GPS_USRT_APB_APB_INTEN_ADDR                            (GPS_USRT_APB_BASE + 0x0008)
#define GPS_USRT_APB_APB_STA_ADDR                              (GPS_USRT_APB_BASE + 0x000C)
#define GPS_USRT_APB_MONF_ADDR                                 (GPS_USRT_APB_BASE + 0x0010)
#define GPS_USRT_APB_MCUB_A2DF_ADDR                            (GPS_USRT_APB_BASE + 0x0020)
#define GPS_USRT_APB_MCUB_D2AF_ADDR                            (GPS_USRT_APB_BASE + 0x0024)
#define GPS_USRT_APB_MCU_A2D0_ADDR                             (GPS_USRT_APB_BASE + 0x0030)
#define GPS_USRT_APB_MCU_A2D1_ADDR                             (GPS_USRT_APB_BASE + 0x0034)
#define GPS_USRT_APB_MCU_D2A0_ADDR                             (GPS_USRT_APB_BASE + 0x0050)
#define GPS_USRT_APB_MCU_D2A1_ADDR                             (GPS_USRT_APB_BASE + 0x0054)


#define GPS_USRT_APB_APB_CTRL_BYTEN_ADDR                       GPS_USRT_APB_APB_CTRL_ADDR
#define GPS_USRT_APB_APB_CTRL_BYTEN_MASK                       0x00000008
#define GPS_USRT_APB_APB_CTRL_BYTEN_SHFT                       3
#define GPS_USRT_APB_APB_CTRL_TX_EN_ADDR                       GPS_USRT_APB_APB_CTRL_ADDR
#define GPS_USRT_APB_APB_CTRL_TX_EN_MASK                       0x00000002
#define GPS_USRT_APB_APB_CTRL_TX_EN_SHFT                       1
#define GPS_USRT_APB_APB_CTRL_RX_EN_ADDR                       GPS_USRT_APB_APB_CTRL_ADDR
#define GPS_USRT_APB_APB_CTRL_RX_EN_MASK                       0x00000001
#define GPS_USRT_APB_APB_CTRL_RX_EN_SHFT                       0

#define GPS_USRT_APB_APB_INTEN_NODAIEN_ADDR                    GPS_USRT_APB_APB_INTEN_ADDR
#define GPS_USRT_APB_APB_INTEN_NODAIEN_MASK                    0x00000002
#define GPS_USRT_APB_APB_INTEN_NODAIEN_SHFT                    1
#define GPS_USRT_APB_APB_INTEN_TXIEN_ADDR                      GPS_USRT_APB_APB_INTEN_ADDR
#define GPS_USRT_APB_APB_INTEN_TXIEN_MASK                      0x00000001
#define GPS_USRT_APB_APB_INTEN_TXIEN_SHFT                      0

#define GPS_USRT_APB_APB_STA_RX_UNDR_ADDR                      GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_RX_UNDR_MASK                      0x80000000
#define GPS_USRT_APB_APB_STA_RX_UNDR_SHFT                      31
#define GPS_USRT_APB_APB_STA_RX_OVF_ADDR                       GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_RX_OVF_MASK                       0x40000000
#define GPS_USRT_APB_APB_STA_RX_OVF_SHFT                       30
#define GPS_USRT_APB_APB_STA_RX_EMP_ADDR                       GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_RX_EMP_MASK                       0x20000000
#define GPS_USRT_APB_APB_STA_RX_EMP_SHFT                       29
#define GPS_USRT_APB_APB_STA_RX_FULL_ADDR                      GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_RX_FULL_MASK                      0x10000000
#define GPS_USRT_APB_APB_STA_RX_FULL_SHFT                      28
#define GPS_USRT_APB_APB_STA_TX_UNDR_ADDR                      GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_TX_UNDR_MASK                      0x08000000
#define GPS_USRT_APB_APB_STA_TX_UNDR_SHFT                      27
#define GPS_USRT_APB_APB_STA_TX_OVF_ADDR                       GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_TX_OVF_MASK                       0x04000000
#define GPS_USRT_APB_APB_STA_TX_OVF_SHFT                       26
#define GPS_USRT_APB_APB_STA_TX_EMP_ADDR                       GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_TX_EMP_MASK                       0x02000000
#define GPS_USRT_APB_APB_STA_TX_EMP_SHFT                       25
#define GPS_USRT_APB_APB_STA_TX_FULL_ADDR                      GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_TX_FULL_MASK                      0x01000000
#define GPS_USRT_APB_APB_STA_TX_FULL_SHFT                      24
#define GPS_USRT_APB_APB_STA_TX_ST_ADDR                        GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_TX_ST_MASK                        0x00700000
#define GPS_USRT_APB_APB_STA_TX_ST_SHFT                        20
#define GPS_USRT_APB_APB_STA_RX_ST_ADDR                        GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_RX_ST_MASK                        0x00070000
#define GPS_USRT_APB_APB_STA_RX_ST_SHFT                        16
#define GPS_USRT_APB_APB_STA_REGE_ADDR                         GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_REGE_MASK                         0x00000020
#define GPS_USRT_APB_APB_STA_REGE_SHFT                         5
#define GPS_USRT_APB_APB_STA_URAME_ADDR                        GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_URAME_MASK                        0x00000010
#define GPS_USRT_APB_APB_STA_URAME_SHFT                        4
#define GPS_USRT_APB_APB_STA_TX_IND_ADDR                       GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_TX_IND_MASK                       0x00000008
#define GPS_USRT_APB_APB_STA_TX_IND_SHFT                       3
#define GPS_USRT_APB_APB_STA_NODAINTB_ADDR                     GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_NODAINTB_MASK                     0x00000002
#define GPS_USRT_APB_APB_STA_NODAINTB_SHFT                     1
#define GPS_USRT_APB_APB_STA_TXINTB_ADDR                       GPS_USRT_APB_APB_STA_ADDR
#define GPS_USRT_APB_APB_STA_TXINTB_MASK                       0x00000001
#define GPS_USRT_APB_APB_STA_TXINTB_SHFT                       0

#define GPS_USRT_APB_MCUB_A2DF_A2DF3_ADDR                      GPS_USRT_APB_MCUB_A2DF_ADDR
#define GPS_USRT_APB_MCUB_A2DF_A2DF3_MASK                      0x00000008
#define GPS_USRT_APB_MCUB_A2DF_A2DF3_SHFT                      3

#define GPS_USRT_APB_MCUB_D2AF_D2AF3_ADDR                      GPS_USRT_APB_MCUB_D2AF_ADDR
#define GPS_USRT_APB_MCUB_D2AF_D2AF3_MASK                      0x00000008
#define GPS_USRT_APB_MCUB_D2AF_D2AF3_SHFT                      3

#define GPS_USRT_APB_MCU_A2D1_A2D_1_ADDR                       GPS_USRT_APB_MCU_A2D1_ADDR
#define GPS_USRT_APB_MCU_A2D1_A2D_1_MASK                       0x0000FFFF
#define GPS_USRT_APB_MCU_A2D1_A2D_1_SHFT                       0

#endif /* __GPS_USRT_APB_REGS_H__ */

