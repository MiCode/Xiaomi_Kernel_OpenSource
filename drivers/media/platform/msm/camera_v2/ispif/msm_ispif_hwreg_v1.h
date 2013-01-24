/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_ISPIF_HWREG_V1_H__
#define __MSM_ISPIF_HWREG_V1_H__

/* common registers */
#define ISPIF_RST_CMD_ADDR                  0x0000
#define ISPIF_RST_CMD_1_ADDR                0x0000 /* undefined */
#define ISPIF_INTF_CMD_ADDR                 0x0004
#define ISPIF_INTF_CMD_1_ADDR               0x0030
#define ISPIF_CTRL_ADDR                     0x0008
#define ISPIF_INPUT_SEL_ADDR                0x000C
#define ISPIF_PIX_0_INTF_CID_MASK_ADDR      0x0010
#define ISPIF_RDI_0_INTF_CID_MASK_ADDR      0x0014
#define ISPIF_PIX_1_INTF_CID_MASK_ADDR      0x0038
#define ISPIF_RDI_1_INTF_CID_MASK_ADDR      0x003C
#define ISPIF_RDI_2_INTF_CID_MASK_ADDR      0x0044
#define ISPIF_PIX_0_STATUS_ADDR             0x0024
#define ISPIF_RDI_0_STATUS_ADDR             0x0028
#define ISPIF_PIX_1_STATUS_ADDR             0x0060
#define ISPIF_RDI_1_STATUS_ADDR             0x0064
#define ISPIF_RDI_2_STATUS_ADDR             0x006C
#define ISPIF_IRQ_MASK_ADDR                 0x0100
#define ISPIF_IRQ_CLEAR_ADDR                0x0104
#define ISPIF_IRQ_STATUS_ADDR               0x0108
#define ISPIF_IRQ_MASK_1_ADDR               0x010C
#define ISPIF_IRQ_CLEAR_1_ADDR              0x0110
#define ISPIF_IRQ_STATUS_1_ADDR             0x0114
#define ISPIF_IRQ_MASK_2_ADDR               0x0118
#define ISPIF_IRQ_CLEAR_2_ADDR              0x011C
#define ISPIF_IRQ_STATUS_2_ADDR             0x0120
#define ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR     0x0124

/*ISPIF RESET BITS*/
#define VFE_CLK_DOMAIN_RST                 BIT(31)
#define RDI_CLK_DOMAIN_RST                 BIT(30)
#define PIX_CLK_DOMAIN_RST                 BIT(29)
#define AHB_CLK_DOMAIN_RST                 BIT(28)
#define RDI_1_CLK_DOMAIN_RST               BIT(27)
#define PIX_1_CLK_DOMAIN_RST               BIT(26)
#define RDI_2_CLK_DOMAIN_RST               BIT(25)
#define RDI_2_MISR_RST_STB                 BIT(20)
#define RDI_2_VFE_RST_STB                  BIT(19)
#define RDI_2_CSID_RST_STB                 BIT(18)
#define RDI_1_MISR_RST_STB                 BIT(14)
#define RDI_1_VFE_RST_STB                  BIT(13)
#define RDI_1_CSID_RST_STB                 BIT(12)
#define PIX_1_VFE_RST_STB                  BIT(10)
#define PIX_1_CSID_RST_STB                 BIT(9)
#define RDI_0_MISR_RST_STB                 BIT(8)
#define RDI_0_VFE_RST_STB                  BIT(7)
#define RDI_0_CSID_RST_STB                 BIT(6)
#define PIX_0_MISR_RST_STB                 BIT(5)
#define PIX_0_VFE_RST_STB                  BIT(4)
#define PIX_0_CSID_RST_STB                 BIT(3)
#define SW_REG_RST_STB                     BIT(2)
#define MISC_LOGIC_RST_STB                 BIT(1)
#define STROBED_RST_EN                     BIT(0)

#define ISPIF_RST_CMD_MASK              0xFE1C77FF
#define ISPIF_RST_CMD_1_MASK            0xFFFFFFFF /* undefined */

/* irq_mask_0 */
#define PIX_INTF_0_OVERFLOW_IRQ            BIT(12)
#define RAW_INTF_0_OVERFLOW_IRQ            BIT(25)
#define RESET_DONE_IRQ                     BIT(27)
/* irq_mask_1 */
#define PIX_INTF_1_OVERFLOW_IRQ            BIT(12)
#define RAW_INTF_1_OVERFLOW_IRQ            BIT(25)
/* irq_mask_2 */
#define RAW_INTF_2_OVERFLOW_IRQ            BIT(12)

#define ISPIF_IRQ_STATUS_MASK           0x0A493249
#define ISPIF_IRQ_STATUS_1_MASK         0x02493249
#define ISPIF_IRQ_STATUS_2_MASK         0x00001249

#define ISPIF_IRQ_STATUS_PIX_SOF_MASK     0x000249
#define ISPIF_IRQ_STATUS_RDI0_SOF_MASK    0x492000
#define ISPIF_IRQ_STATUS_RDI1_SOF_MASK    0x492000
#define ISPIF_IRQ_STATUS_RDI2_SOF_MASK    0x000249

#define ISPIF_IRQ_GLOBAL_CLEAR_CMD        0x000001

#endif /* __MSM_ISPIF_HWREG_V1_H__ */
