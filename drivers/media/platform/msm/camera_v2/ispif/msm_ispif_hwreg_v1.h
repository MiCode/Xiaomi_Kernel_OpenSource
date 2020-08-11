/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2013-2016, 2018, 2020 The Linux Foundation. All rights reserved.
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
#define ISPIF_RST_CMD_ADDR                       0x0000
#define ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR          0x0124
#define PIX0_LINE_BUF_EN_BIT                     0

#define ISPIF_VFE(m)                             (0x0)

#define ISPIF_VFE_m_CTRL_0(m)                    (0x0008 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_MASK_0(m)                (0x0100 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_MASK_1(m)                (0x010C + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_MASK_2(m)                (0x0118 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_STATUS_0(m)              (0x0108 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_STATUS_1(m)              (0x0114 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_STATUS_2(m)              (0x0120 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_CLEAR_0(m)               (0x0104 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_CLEAR_1(m)               (0x0110 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_CLEAR_2(m)               (0x011C + ISPIF_VFE(m))
#define ISPIF_VFE_m_INPUT_SEL(m)                 (0x000C + ISPIF_VFE(m))
#define ISPIF_VFE_m_INTF_CMD_0(m)                (0x0004 + ISPIF_VFE(m))
#define ISPIF_VFE_m_INTF_CMD_1(m)                (0x0030 + ISPIF_VFE(m))
#define ISPIF_VFE_m_PIX_INTF_n_CID_MASK(m, n)    (0x0010 + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_RDI_INTF_n_CID_MASK(m, n)    (0x0014 + ISPIF_VFE(m) + \
							((n > 0) ? (0x20) : 0) \
							+ 8*(n))
#define ISPIF_VFE_m_PIX_OUTPUT_n_MISR(m, n)      (0x0290 + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_RDI_OUTPUT_n_MISR_0(m, n)    (0x001C + ISPIF_VFE(m) + \
							((n > 0) ? (0x24) : 0) \
							+ 0xc*(n))
#define ISPIF_VFE_m_RDI_OUTPUT_n_MISR_1(m, n)    (0x0020 + ISPIF_VFE(m) + \
							((n > 0) ? (0x24) : 0) \
							+ 0xc*(n))
#define ISPIF_VFE_m_PIX_INTF_n_STATUS(m, n)      (0x0024 + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_RDI_INTF_n_STATUS(m, n)      (0x0028 + ISPIF_VFE(m) + \
							((n > 0) ? (0x34) : 0) \
							+ 8*(n))

/* Defines for compatibility with newer ISPIF versions */
#define ISPIF_RST_CMD_1_ADDR                     (0x0000)
#define ISPIF_VFE_m_PIX_INTF_n_CROP(m, n)        (0x0000 + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_3D_THRESHOLD(m)              (0x0000 + ISPIF_VFE(m))
#define ISPIF_VFE_m_OUTPUT_SEL(m)                (0x0000 + ISPIF_VFE(m))
#define ISPIF_VFE_m_3D_DESKEW_SIZE(m)            (0x0000 + ISPIF_VFE(m))



/* CSID CLK MUX SEL REGISTERS */
#define ISPIF_RDI_CLK_MUX_SEL_ADDR              0x8

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

#define VFE_PIX_INTF_SEL_3D                      0x3
#define PIX_OUTPUT_0_MISR_RST_STB                BIT(16)
#define L_R_SOF_MISMATCH_ERR_IRQ                 BIT(16)
#define L_R_EOF_MISMATCH_ERR_IRQ                 BIT(17)
#define L_R_SOL_MISMATCH_ERR_IRQ                 BIT(18)

#define ISPIF_RST_CMD_MASK              0xFE1C77FF
#define ISPIF_RST_CMD_1_MASK            0xFFFFFFFF /* undefined */

#define ISPIF_RST_CMD_MASK_RESTART      0x00001FF9
#define ISPIF_RST_CMD_1_MASK_RESTART    0x00001FF9 /* undefined */

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

#define ISPIF_STOP_INTF_IMMEDIATELY              0xAAAAAAAA

/* ISPIF RDI pack mode not supported */
static inline void msm_ispif_cfg_pack_mode(struct ispif_device *ispif,
	uint8_t intftype, uint8_t vfe_intf, uint32_t *pack_cfg_mask)
{
}
#endif /* __MSM_ISPIF_HWREG_V1_H__ */
