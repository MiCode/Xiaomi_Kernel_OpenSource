/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef MSM_CSID_HWREG_H
#define MSM_CSID_HWREG_H

/* MIPI	CSID registers */
#define CSID_HW_VERSION_ADDR                        0x0
#define CSID_CORE_CTRL_0_ADDR                       0x4
#define CSID_CORE_CTRL_1_ADDR                       0x4
#define CSID_RST_CMD_ADDR                           0x8
#define CSID_CID_LUT_VC_0_ADDR                      0xc
#define CSID_CID_LUT_VC_1_ADDR                      0x10
#define CSID_CID_LUT_VC_2_ADDR                      0x14
#define CSID_CID_LUT_VC_3_ADDR                      0x18
#define CSID_CID_n_CFG_ADDR                         0x1C
#define CSID_IRQ_CLEAR_CMD_ADDR                     0x5c
#define CSID_IRQ_MASK_ADDR                          0x60
#define CSID_IRQ_STATUS_ADDR                        0x64
#define CSID_CAPTURED_UNMAPPED_LONG_PKT_HDR_ADDR    0x68
#define CSID_CAPTURED_MMAPPED_LONG_PKT_HDR_ADDR     0x6c
#define CSID_CAPTURED_SHORT_PKT_ADDR                0x70
#define CSID_CAPTURED_LONG_PKT_HDR_ADDR             0x74
#define CSID_CAPTURED_LONG_PKT_FTR_ADDR             0x78
#define CSID_PIF_MISR_DL0_ADDR                      0x7C
#define CSID_PIF_MISR_DL1_ADDR                      0x80
#define CSID_PIF_MISR_DL2_ADDR                      0x84
#define CSID_PIF_MISR_DL3_ADDR                      0x88
#define CSID_STATS_TOTAL_PKTS_RCVD_ADDR             0x8C
#define CSID_STATS_ECC_ADDR                         0x90
#define CSID_STATS_CRC_ADDR                         0x94
#define CSID_TG_CTRL_ADDR                           0x9C
#define CSID_TG_VC_CFG_ADDR                         0xA0
#define CSID_TG_DT_n_CFG_0_ADDR                     0xA8
#define CSID_TG_DT_n_CFG_1_ADDR                     0xAC
#define CSID_TG_DT_n_CFG_2_ADDR                     0xB0
#define CSID_RST_DONE_IRQ_BITSHIFT                  11
#define CSID_RST_STB_ALL                            0x7FFF
#define CSID_DL_INPUT_SEL_SHIFT                     0x2
#define CSID_PHY_SEL_SHIFT                          17
#define CSID_VERSION                                0x02000011

#endif
