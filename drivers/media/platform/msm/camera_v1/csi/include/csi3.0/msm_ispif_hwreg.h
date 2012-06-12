/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef MSM_ISPIF_HWREG_H
#define MSM_ISPIF_HWREG_H


/* ISPIF registers */

#define ISPIF_RST_CMD_ADDR                        0x08
#define ISPIF_RST_CMD_1_ADDR                      0x0C
#define ISPIF_INTF_CMD_ADDR                      0x248
#define ISPIF_INTF_CMD_1_ADDR                    0x24C
#define ISPIF_CTRL_ADDR                           0x08
#define ISPIF_INPUT_SEL_ADDR                     0x244
#define ISPIF_PIX_0_INTF_CID_MASK_ADDR           0x254
#define ISPIF_RDI_0_INTF_CID_MASK_ADDR           0x264
#define ISPIF_PIX_1_INTF_CID_MASK_ADDR           0x258
#define ISPIF_RDI_1_INTF_CID_MASK_ADDR           0x268
#define ISPIF_RDI_2_INTF_CID_MASK_ADDR           0x26C
#define ISPIF_PIX_0_STATUS_ADDR                  0x2C0
#define ISPIF_RDI_0_STATUS_ADDR                  0x2D0
#define ISPIF_PIX_1_STATUS_ADDR                  0x2C4
#define ISPIF_RDI_1_STATUS_ADDR                  0x2D4
#define ISPIF_RDI_2_STATUS_ADDR                  0x2D8
#define ISPIF_IRQ_MASK_ADDR                      0x208
#define ISPIF_IRQ_CLEAR_ADDR                     0x230
#define ISPIF_IRQ_STATUS_ADDR                    0x21C
#define ISPIF_IRQ_MASK_1_ADDR                    0x20C
#define ISPIF_IRQ_CLEAR_1_ADDR                   0x234
#define ISPIF_IRQ_STATUS_1_ADDR                  0x220
#define ISPIF_IRQ_MASK_2_ADDR                    0x210
#define ISPIF_IRQ_CLEAR_2_ADDR                   0x238
#define ISPIF_IRQ_STATUS_2_ADDR                  0x224
#define ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR           0x1C

/* new */
#define ISPIF_VFE_m_CTRL_0_ADDR                  0x200
#define ISPIF_VFE_m_IRQ_MASK_0                   0x208
#define ISPIF_VFE_m_IRQ_MASK_1                   0x20C
#define ISPIF_VFE_m_IRQ_MASK_2                   0x210
#define ISPIF_VFE_m_IRQ_STATUS_0                 0x21C
#define ISPIF_VFE_m_IRQ_STATUS_1                 0x220
#define ISPIF_VFE_m_IRQ_STATUS_2                 0x224
#define ISPIF_VFE_m_IRQ_CLEAR_0                  0x230
#define ISPIF_VFE_m_IRQ_CLEAR_1                  0x234
#define ISPIF_VFE_m_IRQ_CLEAR_2                  0x238

/*ISPIF RESET BITS*/

#define VFE_CLK_DOMAIN_RST           31
#define RDI_CLK_DOMAIN_RST           26
#define RDI_1_CLK_DOMAIN_RST         27
#define RDI_2_CLK_DOMAIN_RST         28
#define PIX_CLK_DOMAIN_RST           29
#define PIX_1_CLK_DOMAIN_RST         30
#define AHB_CLK_DOMAIN_RST           25
#define RDI_2_VFE_RST_STB            12
#define RDI_2_CSID_RST_STB           11
#define RDI_1_VFE_RST_STB            10
#define RDI_1_CSID_RST_STB           9
#define RDI_0_VFE_RST_STB            8
#define RDI_0_CSID_RST_STB           7
#define PIX_1_VFE_RST_STB            6
#define PIX_1_CSID_RST_STB           5
#define PIX_0_VFE_RST_STB            4
#define PIX_0_CSID_RST_STB           3
#define SW_REG_RST_STB               2
#define MISC_LOGIC_RST_STB           1
#define STROBED_RST_EN               0

#define PIX_INTF_0_OVERFLOW_IRQ      12
#define RAW_INTF_0_OVERFLOW_IRQ      25
#define RAW_INTF_1_OVERFLOW_IRQ      25
#define RESET_DONE_IRQ               27

#define ISPIF_IRQ_STATUS_MASK          0xA493249
#define ISPIF_IRQ_1_STATUS_MASK        0xA493249
#define ISPIF_IRQ_STATUS_RDI_SOF_MASK  0x492000
#define ISPIF_IRQ_STATUS_PIX_SOF_MASK  0x249
#define ISPIF_IRQ_STATUS_SOF_MASK      0x492249
#define ISPIF_IRQ_GLOBAL_CLEAR_CMD     0x1


#endif
