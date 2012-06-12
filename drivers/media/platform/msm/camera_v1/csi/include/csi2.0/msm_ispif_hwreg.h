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

#define ISPIF_RST_CMD_ADDR                        0x00
#define ISPIF_RST_CMD_1_ADDR                      0x00
#define ISPIF_INTF_CMD_ADDR                       0x04
#define ISPIF_INTF_CMD_1_ADDR                     0x30
#define ISPIF_CTRL_ADDR                           0x08
#define ISPIF_INPUT_SEL_ADDR                      0x0C
#define ISPIF_PIX_0_INTF_CID_MASK_ADDR            0x10
#define ISPIF_RDI_0_INTF_CID_MASK_ADDR            0x14
#define ISPIF_PIX_1_INTF_CID_MASK_ADDR            0x38
#define ISPIF_RDI_1_INTF_CID_MASK_ADDR            0x3C
#define ISPIF_RDI_2_INTF_CID_MASK_ADDR            0x44
#define ISPIF_PIX_0_STATUS_ADDR                   0x24
#define ISPIF_RDI_0_STATUS_ADDR                   0x28
#define ISPIF_PIX_1_STATUS_ADDR                   0x60
#define ISPIF_RDI_1_STATUS_ADDR                   0x64
#define ISPIF_RDI_2_STATUS_ADDR                   0x6C
#define ISPIF_IRQ_MASK_ADDR                     0x0100
#define ISPIF_IRQ_CLEAR_ADDR                    0x0104
#define ISPIF_IRQ_STATUS_ADDR                   0x0108
#define ISPIF_IRQ_MASK_1_ADDR                   0x010C
#define ISPIF_IRQ_CLEAR_1_ADDR                  0x0110
#define ISPIF_IRQ_STATUS_1_ADDR                 0x0114
#define ISPIF_IRQ_MASK_2_ADDR                   0x0118
#define ISPIF_IRQ_CLEAR_2_ADDR                  0x011C
#define ISPIF_IRQ_STATUS_2_ADDR                 0x0120
#define ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR         0x0124

/*ISPIF RESET BITS*/

#define VFE_CLK_DOMAIN_RST           31
#define RDI_CLK_DOMAIN_RST           30
#define PIX_CLK_DOMAIN_RST           29
#define AHB_CLK_DOMAIN_RST           28
#define RDI_1_CLK_DOMAIN_RST         27
#define RDI_2_VFE_RST_STB            19
#define RDI_2_CSID_RST_STB           18
#define RDI_1_VFE_RST_STB            13
#define RDI_1_CSID_RST_STB           12
#define RDI_0_VFE_RST_STB            7
#define RDI_0_CSID_RST_STB           6
#define PIX_1_VFE_RST_STB            10
#define PIX_1_CSID_RST_STB           9
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
