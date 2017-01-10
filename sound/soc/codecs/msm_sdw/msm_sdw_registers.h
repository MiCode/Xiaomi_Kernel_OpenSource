/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#ifndef MSM_SDW_REGISTERS_H
#define MSM_SDW_REGISTERS_H

#define MSM_SDW_PAGE_REGISTER                     0x0000

/* Page-A Registers */
#define MSM_SDW_TX9_SPKR_PROT_PATH_CTL               0x0308
#define MSM_SDW_TX9_SPKR_PROT_PATH_CFG0              0x030c
#define MSM_SDW_TX10_SPKR_PROT_PATH_CTL              0x0318
#define MSM_SDW_TX10_SPKR_PROT_PATH_CFG0             0x031c
#define MSM_SDW_TX11_SPKR_PROT_PATH_CTL              0x0328
#define MSM_SDW_TX11_SPKR_PROT_PATH_CFG0             0x032c
#define MSM_SDW_TX12_SPKR_PROT_PATH_CTL              0x0338
#define MSM_SDW_TX12_SPKR_PROT_PATH_CFG0             0x033c

/* Page-B Registers */
#define MSM_SDW_COMPANDER7_CTL0                      0x0024
#define MSM_SDW_COMPANDER7_CTL1                      0x0028
#define MSM_SDW_COMPANDER7_CTL2                      0x002c
#define MSM_SDW_COMPANDER7_CTL3                      0x0030
#define MSM_SDW_COMPANDER7_CTL4                      0x0034
#define MSM_SDW_COMPANDER7_CTL5                      0x0038
#define MSM_SDW_COMPANDER7_CTL6                      0x003c
#define MSM_SDW_COMPANDER7_CTL7                      0x0040
#define MSM_SDW_COMPANDER8_CTL0                      0x0044
#define MSM_SDW_COMPANDER8_CTL1                      0x0048
#define MSM_SDW_COMPANDER8_CTL2                      0x004c
#define MSM_SDW_COMPANDER8_CTL3                      0x0050
#define MSM_SDW_COMPANDER8_CTL4                      0x0054
#define MSM_SDW_COMPANDER8_CTL5                      0x0058
#define MSM_SDW_COMPANDER8_CTL6                      0x005c
#define MSM_SDW_COMPANDER8_CTL7                      0x0060
#define MSM_SDW_RX7_RX_PATH_CTL                      0x01a4
#define MSM_SDW_RX7_RX_PATH_CFG0                     0x01a8
#define MSM_SDW_RX7_RX_PATH_CFG1                     0x01ac
#define MSM_SDW_RX7_RX_PATH_CFG2                     0x01b0
#define MSM_SDW_RX7_RX_VOL_CTL                       0x01b4
#define MSM_SDW_RX7_RX_PATH_MIX_CTL                  0x01b8
#define MSM_SDW_RX7_RX_PATH_MIX_CFG                  0x01bc
#define MSM_SDW_RX7_RX_VOL_MIX_CTL                   0x01c0
#define MSM_SDW_RX7_RX_PATH_SEC0                     0x01c4
#define MSM_SDW_RX7_RX_PATH_SEC1                     0x01c8
#define MSM_SDW_RX7_RX_PATH_SEC2                     0x01cc
#define MSM_SDW_RX7_RX_PATH_SEC3                     0x01d0
#define MSM_SDW_RX7_RX_PATH_SEC5                     0x01d8
#define MSM_SDW_RX7_RX_PATH_SEC6                     0x01dc
#define MSM_SDW_RX7_RX_PATH_SEC7                     0x01e0
#define MSM_SDW_RX7_RX_PATH_MIX_SEC0                 0x01e4
#define MSM_SDW_RX7_RX_PATH_MIX_SEC1                 0x01e8
#define MSM_SDW_RX8_RX_PATH_CTL                      0x0384
#define MSM_SDW_RX8_RX_PATH_CFG0                     0x0388
#define MSM_SDW_RX8_RX_PATH_CFG1                     0x038c
#define MSM_SDW_RX8_RX_PATH_CFG2                     0x0390
#define MSM_SDW_RX8_RX_VOL_CTL                       0x0394
#define MSM_SDW_RX8_RX_PATH_MIX_CTL                  0x0398
#define MSM_SDW_RX8_RX_PATH_MIX_CFG                  0x039c
#define MSM_SDW_RX8_RX_VOL_MIX_CTL                   0x03a0
#define MSM_SDW_RX8_RX_PATH_SEC0                     0x03a4
#define MSM_SDW_RX8_RX_PATH_SEC1                     0x03a8
#define MSM_SDW_RX8_RX_PATH_SEC2                     0x03ac
#define MSM_SDW_RX8_RX_PATH_SEC3                     0x03b0
#define MSM_SDW_RX8_RX_PATH_SEC5                     0x03b8
#define MSM_SDW_RX8_RX_PATH_SEC6                     0x03bc
#define MSM_SDW_RX8_RX_PATH_SEC7                     0x03c0
#define MSM_SDW_RX8_RX_PATH_MIX_SEC0                 0x03c4
#define MSM_SDW_RX8_RX_PATH_MIX_SEC1                 0x03c8

/* Page-C Registers */
#define MSM_SDW_BOOST0_BOOST_PATH_CTL                0x0064
#define MSM_SDW_BOOST0_BOOST_CTL                     0x0068
#define MSM_SDW_BOOST0_BOOST_CFG1                    0x006c
#define MSM_SDW_BOOST0_BOOST_CFG2                    0x0070
#define MSM_SDW_BOOST1_BOOST_PATH_CTL                0x0084
#define MSM_SDW_BOOST1_BOOST_CTL                     0x0088
#define MSM_SDW_BOOST1_BOOST_CFG1                    0x008c
#define MSM_SDW_BOOST1_BOOST_CFG2                    0x0090
#define MSM_SDW_AHB_BRIDGE_WR_DATA_0                 0x00a4
#define MSM_SDW_AHB_BRIDGE_WR_DATA_1                 0x00a8
#define MSM_SDW_AHB_BRIDGE_WR_DATA_2                 0x00ac
#define MSM_SDW_AHB_BRIDGE_WR_DATA_3                 0x00b0
#define MSM_SDW_AHB_BRIDGE_WR_ADDR_0                 0x00b4
#define MSM_SDW_AHB_BRIDGE_WR_ADDR_1                 0x00b8
#define MSM_SDW_AHB_BRIDGE_WR_ADDR_2                 0x00bc
#define MSM_SDW_AHB_BRIDGE_WR_ADDR_3                 0x00c0
#define MSM_SDW_AHB_BRIDGE_RD_ADDR_0                 0x00c4
#define MSM_SDW_AHB_BRIDGE_RD_ADDR_1                 0x00c8
#define MSM_SDW_AHB_BRIDGE_RD_ADDR_2                 0x00cc
#define MSM_SDW_AHB_BRIDGE_RD_ADDR_3                 0x00d0
#define MSM_SDW_AHB_BRIDGE_RD_DATA_0                 0x00d4
#define MSM_SDW_AHB_BRIDGE_RD_DATA_1                 0x00d8
#define MSM_SDW_AHB_BRIDGE_RD_DATA_2                 0x00dc
#define MSM_SDW_AHB_BRIDGE_RD_DATA_3                 0x00e0
#define MSM_SDW_AHB_BRIDGE_ACCESS_CFG                0x00e4
#define MSM_SDW_AHB_BRIDGE_ACCESS_STATUS             0x00e8

/* Page-D Registers */
#define MSM_SDW_CLK_RST_CTRL_MCLK_CONTROL            0x0104
#define MSM_SDW_CLK_RST_CTRL_FS_CNT_CONTROL          0x0108
#define MSM_SDW_CLK_RST_CTRL_SWR_CONTROL             0x010c
#define MSM_SDW_TOP_TOP_CFG0                         0x0204
#define MSM_SDW_TOP_TOP_CFG1                         0x0208
#define MSM_SDW_TOP_RX_I2S_CTL                       0x020c
#define MSM_SDW_TOP_TX_I2S_CTL                       0x0210
#define MSM_SDW_TOP_I2S_CLK                          0x0214
#define MSM_SDW_TOP_RX7_PATH_INPUT0_MUX              0x0218
#define MSM_SDW_TOP_RX7_PATH_INPUT1_MUX              0x021c
#define MSM_SDW_TOP_RX8_PATH_INPUT0_MUX              0x0220
#define MSM_SDW_TOP_RX8_PATH_INPUT1_MUX              0x0224
#define MSM_SDW_TOP_FREQ_MCLK                        0x0228
#define MSM_SDW_TOP_DEBUG_BUS_SEL                    0x022c
#define MSM_SDW_TOP_DEBUG_EN                         0x0230
#define MSM_SDW_TOP_I2S_RESET                        0x0234
#define MSM_SDW_TOP_BLOCKS_RESET                     0x0238

#endif
