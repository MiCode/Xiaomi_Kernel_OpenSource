/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_ISPIF_HWREG_V3_H__
#define __MSM_ISPIF_HWREG_V3_H__

/* common registers */
#define ISPIF_RST_CMD_ADDR                       0x008
#define ISPIF_RST_CMD_1_ADDR                     0x00C
#define ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR          0x01C
#define PIX0_LINE_BUF_EN_BIT                     6

#define ISPIF_VFE(m)                             ((m) * 0x200)

#define ISPIF_VFE_m_CTRL_0(m)                    (0x200 + ISPIF_VFE(m))
#define ISPIF_VFE_m_CTRL_1(m)                    (0x204 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_MASK_0(m)                (0x208 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_MASK_1(m)                (0x20C + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_MASK_2(m)                (0x210 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_STATUS_0(m)              (0x21C + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_STATUS_1(m)              (0x220 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_STATUS_2(m)              (0x224 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_CLEAR_0(m)               (0x230 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_CLEAR_1(m)               (0x234 + ISPIF_VFE(m))
#define ISPIF_VFE_m_IRQ_CLEAR_2(m)               (0x238 + ISPIF_VFE(m))
#define ISPIF_VFE_m_INPUT_SEL(m)                 (0x244 + ISPIF_VFE(m))
#define ISPIF_VFE_m_INTF_CMD_0(m)                (0x248 + ISPIF_VFE(m))
#define ISPIF_VFE_m_INTF_CMD_1(m)                (0x24C + ISPIF_VFE(m))
#define ISPIF_VFE_m_PIX_INTF_n_CID_MASK(m, n)    (0x254 + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_RDI_INTF_n_CID_MASK(m, n)    (0x264 + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_RDI_INTF_n_PACK_0(m, n)      (0x270 + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_RDI_INTF_n_PACK_1(m, n)      (0x27C + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_PIX_INTF_n_CROP(m, n)        (0x288 + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_3D_THRESHOLD(m)              (0x290 + ISPIF_VFE(m))
#define ISPIF_VFE_m_OUTPUT_SEL(m)                (0x294 + ISPIF_VFE(m))
#define ISPIF_VFE_m_PIX_OUTPUT_n_MISR(m, n)      (0x298 + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_RDI_OUTPUT_n_MISR_0(m, n)    (0x29C + ISPIF_VFE(m) + 8*(n))
#define ISPIF_VFE_m_RDI_OUTPUT_n_MISR_1(m, n)    (0x2A0 + ISPIF_VFE(m) + 8*(n))
#define ISPIF_VFE_m_PIX_INTF_n_STATUS(m, n)      (0x2C0 + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_RDI_INTF_n_STATUS(m, n)      (0x2D0 + ISPIF_VFE(m) + 4*(n))
#define ISPIF_VFE_m_3D_DESKEW_SIZE(m)            (0x2E4 + ISPIF_VFE(m))

/* CSID CLK MUX SEL REGISTERS */
#define ISPIF_RDI_CLK_MUX_SEL_ADDR               0x8

/* ISPIF RESET BITS */
#define VFE_CLK_DOMAIN_RST                       BIT(31)
#define PIX_1_CLK_DOMAIN_RST                     BIT(30)
#define PIX_CLK_DOMAIN_RST                       BIT(29)
#define RDI_2_CLK_DOMAIN_RST                     BIT(28)
#define RDI_1_CLK_DOMAIN_RST                     BIT(27)
#define RDI_CLK_DOMAIN_RST                       BIT(26)
#define AHB_CLK_DOMAIN_RST                       BIT(25)
#define RDI_2_VFE_RST_STB                        BIT(12)
#define RDI_2_CSID_RST_STB                       BIT(11)
#define RDI_1_VFE_RST_STB                        BIT(10)
#define RDI_1_CSID_RST_STB                       BIT(9)
#define RDI_0_VFE_RST_STB                        BIT(8)
#define RDI_0_CSID_RST_STB                       BIT(7)
#define PIX_1_VFE_RST_STB                        BIT(6)
#define PIX_1_CSID_RST_STB                       BIT(5)
#define PIX_0_VFE_RST_STB                        BIT(4)
#define PIX_0_CSID_RST_STB                       BIT(3)
#define SW_REG_RST_STB                           BIT(2)
#define MISC_LOGIC_RST_STB                       BIT(1)
#define STROBED_RST_EN                           BIT(0)

#define ISPIF_RST_CMD_MASK                       0xFE7F1FFF
#define ISPIF_RST_CMD_1_MASK                     0xFC7F1FF9

#define ISPIF_RST_CMD_MASK_RESTART               0x7F1FF9
#define ISPIF_RST_CMD_1_MASK_RESTART             0x7F1FF9

#define PIX_INTF_0_OVERFLOW_IRQ                  BIT(12)
#define RAW_INTF_0_OVERFLOW_IRQ                  BIT(25)
#define RAW_INTF_1_OVERFLOW_IRQ                  BIT(25)
#define RAW_INTF_2_OVERFLOW_IRQ                  BIT(12)
#define RESET_DONE_IRQ                           BIT(27)

#define ISPIF_IRQ_STATUS_MASK                    0x0A493249
#define ISPIF_IRQ_STATUS_1_MASK                  0x02493249
#define ISPIF_IRQ_STATUS_2_MASK                  0x00001249

#define ISPIF_IRQ_STATUS_PIX_SOF_MASK            0x249
#define ISPIF_IRQ_STATUS_RDI0_SOF_MASK           0x492000
#define ISPIF_IRQ_STATUS_RDI1_SOF_MASK           0x492000
#define ISPIF_IRQ_STATUS_RDI2_SOF_MASK           0x249

#define ISPIF_IRQ_GLOBAL_CLEAR_CMD               0x1

#define ISPIF_STOP_INTF_IMMEDIATELY              0xAAAAAAAA

/* ISPIF RDI pack mode support */
static inline void msm_ispif_cfg_pack_mode(struct ispif_device *ispif,
	uint8_t intftype, uint8_t vfe_intf, uint32_t *pack_cfg_mask)
{
	uint32_t pack_addr[2];

	BUG_ON(!ispif);

	switch (intftype) {
	case RDI0:
		pack_addr[0] = ISPIF_VFE_m_RDI_INTF_n_PACK_0(vfe_intf, 0);
		pack_addr[1] = ISPIF_VFE_m_RDI_INTF_n_PACK_1(vfe_intf, 0);
		break;
	case RDI1:
		pack_addr[0] = ISPIF_VFE_m_RDI_INTF_n_PACK_0(vfe_intf, 1);
		pack_addr[1] = ISPIF_VFE_m_RDI_INTF_n_PACK_1(vfe_intf, 1);
		break;
	case RDI2:
		pack_addr[0] = ISPIF_VFE_m_RDI_INTF_n_PACK_0(vfe_intf, 2);
		pack_addr[1] = ISPIF_VFE_m_RDI_INTF_n_PACK_1(vfe_intf, 2);
		break;
	default:
		pr_debug("%s: pack_mode not supported on intftype=%d\n",
			__func__, intftype);
		return;
	}
	pr_debug("%s: intftype %d pack_mask %x: 0x%x, %x:0x%x\n",
		__func__, intftype, pack_addr[0],
		pack_cfg_mask[0], pack_addr[1],
		pack_cfg_mask[1]);
	msm_camera_io_w_mb(pack_cfg_mask[0], ispif->base + pack_addr[0]);
	msm_camera_io_w_mb(pack_cfg_mask[1], ispif->base + pack_addr[1]);
}
#endif /* __MSM_ISPIF_HWREG_V3_H__ */
