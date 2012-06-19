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
 *
 */

#ifndef __MSM_CAM_IRQROUTER_H__
#define __MSM_CAM_IRQROUTER_H__

#include <linux/bitops.h>

/* Camera SS common registers defines - Start */
/* These registers are not directly related to
 * IRQ Router, but are common to the Camera SS.
 * IRQ Router registers dont have a unique base address
 * in the memory mapped address space. It is offset from
 * the Camera SS base address. So keep the common Camera
 * SS registers also in the IRQ Router subdev for now. */

/* READ ONLY: Camera Subsystem HW version */
#define CAMSS_HW_VERSION			0x00000000

/* Bits 4:0 of this register can be used to select a desired
 * camera core test bus to drive the Camera_SS test bus output */
#define CAMSS_TESTBUS_SEL			0x00000004

/* Bits 4:0 of this register is used to allow either Microcontroller
 * or the CCI drive the corresponding GPIO output.
 * For eg: Setting bit 0 of this register allows Microcontroller to
 * drive GPIO #0. Clearing the bit allows CCI to drive GPIO #0. */
#define CAMSS_GPIO_MUX_SEL			0x00000008

/* Bit 0 of this register is used to set the default AHB master
 * for the AHB Arbiter. 0 - AHB Master 0, 1 - AHB Master 1*/
#define CAMSS_AHB_ARB_CTL			0x0000000C

/* READ ONLY */
#define CAMSS_XPU2_STATUS			0x00000010

/* Select the appropriate CSI clock for CSID Pixel pipes */
#define CAMSS_CSI_PIX_CLK_MUX_SEL		0x00000020
#define CAMSS_CSI_PIX_CLK_CGC_EN		0x00000024

/* Select the appropriate CSI clock for CSID RDI pipes */
#define CAMSS_CSI_RDI_CLK_MUX_SEL		0x00000028
#define CAMSS_CSI_RDI_CLK_CGC_EN		0x0000002C

/* Select the appropriate CSI clock for CSI Phy0 */
#define CAMSS_CSI_PHY_0_CLK_MUX_SEL		0x00000030
#define CAMSS_CSI_PHY_0_CLK_CGC_EN		0x00000034

/* Select the appropriate CSI clock for CSI Phy1 */
#define CAMSS_CSI_PHY_1_CLK_MUX_SEL		0x00000038
#define CAMSS_CSI_PHY_1_CLK_CGC_EN		0x0000003C

/* Select the appropriate CSI clock for CSI Phy2 */
#define CAMSS_CSI_PHY_2_CLK_MUX_SEL		0x00000040
#define CAMSS_CSI_PHY_2_CLK_CGC_EN		0x00000044
/* Camera SS common registers defines - End */

/* IRQ Router registers defines - Start */
/* This register is used to reset the composite
 * IRQ outputs of the Camera_SS IRQ Router */
#define CAMSS_IRQ_COMPOSITE_RESET_CTRL		0x00000060

/* By default, this 'allows' the interrupts from
 * Micro to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_0		0x00000064

/* By default, this 'allows' the interrupts from
 * CCI to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_1		0x00000068

/* By default, this 'allows' the interrupts from
 * CSI_0 to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_2		0x0000006C

/* By default, this 'allows' the interrupts from
 * CSI_1 to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_3		0x00000070

/* By default, this 'allows' the interrupts from
 * CSI_2 to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_4		0x00000074

/* By default, this 'allows' the interrupts from
 * CSI_3 to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_5		0x00000078

/* By default, this 'allows' the interrupts from
 * ISPIF to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_6		0x0000007C

/* By default, this 'allows' the interrupts from
 * CPP to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_7		0x00000080

/* By default, this 'allows' the interrupts from
 * VFE_0 to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_8		0x00000084

/* By default, this 'allows' the interrupts from
 * VFE_1 to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_9		0x00000088

/* By default, this 'allows' the interrupts from
 * JPEG_0 to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_10		0x0000008C

/* By default, this 'allows' the interrupts from
 * JPEG_1 to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_11		0x00000090

/* By default, this 'allows' the interrupts from
 * JPEG_2 to pass through, unless configured in
 * composite mode. */
#define CAMSS_IRQ_COMPOSITE_MASK_12		0x00000094

/* The following IRQ_COMPOSITE_MICRO_MASK registers
 * allow the interrupts from the individual hw
 * cores to be composited into an IRQ for Micro. */
#define CAMSS_IRQ_COMPOSITE_MICRO_MASK_0	0x000000A4
#define CAMSS_IRQ_COMPOSITE_MICRO_MASK_1	0x000000A8
#define CAMSS_IRQ_COMPOSITE_MICRO_MASK_2	0x000000AC
#define CAMSS_IRQ_COMPOSITE_MICRO_MASK_3	0x000000B0
#define CAMSS_IRQ_COMPOSITE_MICRO_MASK_4	0x000000B4
#define CAMSS_IRQ_COMPOSITE_MICRO_MASK_5	0x000000B8
/* IRQ Router register defines - End */

/* Writing this mask will reset all the composite
 * IRQs of the Camera_SS IRQ Router */
#define CAMSS_IRQ_COMPOSITE_RESET_MASK		0x003F1FFF

/* Use this to enable Micro IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_MICRO_IRQ_IN_COMPOSITE		BIT(0)
/* Use this to enable CCI IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_CCI_IRQ_IN_COMPOSITE		BIT(1)
/* Use this to enable CSI0 IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_CSI0_IRQ_IN_COMPOSITE		BIT(2)
/* Use this to enable CSI1 IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_CSI1_IRQ_IN_COMPOSITE		BIT(3)
/* Use this to enable CSI2 IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_CSI2_IRQ_IN_COMPOSITE		BIT(4)
/* Use this to enable CSI3 IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_CSI3_IRQ_IN_COMPOSITE		BIT(5)
/* Use this to enable ISPIF IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_ISPIF_IRQ_IN_COMPOSITE		BIT(6)
/* Use this to enable CPP IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_CPP_IRQ_IN_COMPOSITE		BIT(7)
/* Use this to enable VFE0 IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_VFE0_IRQ_IN_COMPOSITE		BIT(8)
/* Use this to enable VFE1 IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_VFE1_IRQ_IN_COMPOSITE		BIT(9)
/* Use this to enable JPEG0 IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_JPEG0_IRQ_IN_COMPOSITE		BIT(10)
/* Use this to enable JPEG1 IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_JPEG1_IRQ_IN_COMPOSITE		BIT(11)
/* Use this to enable JPEG2 IRQ from IRQ Router
 * composite interrupt */
#define ENABLE_JPEG2_IRQ_IN_COMPOSITE		BIT(12)

struct irqrouter_ctrl_type {
	/* v4l2 subdev */
	struct v4l2_subdev subdev;
	struct platform_device *pdev;

	void __iomem *irqr_dev_base;

	struct resource	*irqr_dev_mem;
	struct resource *irqr_dev_io;
	atomic_t active;
	struct msm_cam_server_irqmap_entry def_hw_irqmap[CAMERA_SS_IRQ_MAX];
};

#endif /* __MSM_CAM_IRQROUTER_H__ */
