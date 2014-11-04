/*
 * Support for Medifield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2012 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef ATOMISP_REGS_H
#define ATOMISP_REGS_H

/* common register definitions */
#define PUNIT_PORT		0x04

#define PCICMDSTS		0x01
#define INTR			0x0f
#define MSI_CAPID		0x24
#define MSI_ADDRESS		0x25
#define MSI_DATA		0x26
#define INTR_CTL		0x27

#define PCI_MSI_CAPID		0x90
#define PCI_MSI_ADDR		0x94
#define PCI_MSI_DATA		0x98
#define PCI_INTERRUPT_CTRL	0x9C
#define PCI_I_CONTROL		0xfc

/* MFLD specific register definitions */
#define MFLD_IUNITPHY_PORT	0x09

#define MFLD_CSI_RCOMP		0x00
#define MFLD_CSI_AFE		0x02
#define MFLD_CSI_CONTROL	0x03
#define MFLD_CG_DIS		0x36
#define MFLD_OR1		0x72

#define MFLD_PCI_PMCS		0xd4
#define MFLD_PCI_CG_DIS		0xd8

/*
 * Enables the combining of adjacent 32-byte read requests to the same
 * cache line. When cleared, each 32-byte read request is sent as a
 * separate request on the IB interface.
 */
#define MFLD_PCI_I_CONTROL_ENABLE_READ_COMBINING	BIT(16)

/*
 * Enables the combining of adjacent 32-byte write requests to the same
 * cache line. When cleared, each 32-byte write request is sent as a
 * separate request on the IB interface.
 */
#define MFLD_PCI_I_CONTROL_ENABLE_WRITE_COMBINING	BIT(17)

/* Ensure the correct bits are set for the clock gating disable regster */
#define MFLD_PCI_CG_DIS_DISABLED_ISPCLK		BIT(0)
#define MFLD_PCI_CG_DIS_DISABLED_PERF_MON	BIT(2)
#define MFLD_PCI_CG_DIS_DISABLED_NOA_MON	BIT(3)

/* The MIPI1 and MIPI4 interface and lanes configuration */
#define MFLD_CSI_CONTROL_DIS_MIPI1_IF	BIT(8)
#define MFLD_CSI_CONTROL_DIS_MIPI4_IF	BIT(9)
#define MFLD_CSI_CONTROL_EN_MIPI1_LANE	BIT(10)
#define MFLD_CSI_CONTROL_EN_MIPI4_LANE	(BIT(11) | BIT(12) | BIT(13) | BIT(14))

#define MFLD_MAX_ZOOM_FACTOR	64

/* MRFLD specific register definitions */
#define MRFLD_CSI_AFE		0x39
#define MRFLD_CSI_CONTROL	0x3a
#define MRFLD_CSI_RCOMP		0x3d

#define MRFLD_PCI_PMCS		0x84
#define MRFLD_PCI_CSI_ACCESS_CTRL_VIOL	0xd4
#define MRFLD_PCI_CSI_AFE_HS_CONTROL	0xdc
#define MRFLD_PCI_CSI_AFE_RCOMP_CONTROL	0xe0
#define MRFLD_PCI_CSI_CONTROL		0xe8
#define MRFLD_PCI_CSI_AFE_TRIM_CONTROL	0xe4
#define MRFLD_PCI_CSI_DEADLINE_CONTROL	0xec
#define MRFLD_PCI_CSI_RCOMP_CONTROL	0xf4

/* Select Arasan (legacy)/Intel input system */
#define MRFLD_PCI_CSI_CONTROL_PARPATHEN	BIT(24)

/*
 * Enables the combining of adjacent 32-byte read requests to the same
 * cache line. When cleared, each 32-byte read request is sent as a
 * separate request on the IB interface.
 */
#define MRFLD_PCI_I_CONTROL_ENABLE_READ_COMBINING	0x1

/*
 * Register: MRFLD_PCI_CSI_RCOMP_CONTROL
 * If cleared, the high speed clock going to the digital logic is gated when
 * RCOMP update is happening. The clock is gated for a minimum of 100 nsec.
 * If this bit is set, then the high speed clock is not gated during the
 * update cycle.
 */
#define MRFLD_PCI_CSI_HS_OVR_CLK_GATE_ON_UPDATE		0x800000

/*
 * Enables the combining of adjacent 32-byte write requests to the same
 * cache line. When cleared, each 32-byte write request is sent as a
 * separate request on the IB interface.
 */
#define MRFLD_PCI_I_CONTROL_ENABLE_WRITE_COMBINING	0x2

#define MRFLD_PCI_CSI1_HSRXCLKTRIM		0x2
#define MRFLD_PCI_CSI1_HSRXCLKTRIM_SHIFT	16
#define MRFLD_PCI_CSI2_HSRXCLKTRIM		0x3
#define MRFLD_PCI_CSI2_HSRXCLKTRIM_SHIFT	24
#define MRFLD_PCI_CSI3_HSRXCLKTRIM		0x2
#define MRFLD_PCI_CSI3_HSRXCLKTRIM_SHIFT	28
#define MRFLD_PCI_CSI_HSRXCLKTRIM_MASK		0xf

/*
 * This register is IUINT MMIO register, it is used to select the CSI
 * receiver backend.
 * 1: SH CSI backend
 * 0: Arasan CSI backend
 */
#define MRFLD_CSI_RECEIVER_SELECTION_REG       0x8081c

#define MRFLD_INTR_CLEAR_REG		       0x50c
#define MRFLD_INTR_STATUS_REG		       0x508
#define MRFLD_INTR_ENABLE_REG		       0x510

#define MRFLD_MAX_ZOOM_FACTOR	1024

/* MRFLD ISP POWER related */
#define MRFLD_ISPSSPM0         0x39
#define MRFLD_ISPSSPM0_ISPSSC_OFFSET   0
#define MRFLD_ISPSSPM0_ISPSSS_OFFSET   24
#define MRFLD_ISPSSPM0_ISPSSC_MASK     0x3
#define MRFLD_ISPSSPM0_IUNIT_POWER_ON  0
#define MRFLD_ISPSSPM0_IUNIT_POWER_OFF 0x3

/* MRFLD CSI lane configuration related */
#define MRFLD_PORT_CONFIG_NUM  8
#define MRFLD_PORT_NUM         3
#define MRFLD_PORT1_ENABLE_SHIFT       0
#define MRFLD_PORT2_ENABLE_SHIFT       1
#define MRFLD_PORT3_ENABLE_SHIFT       2
#define MRFLD_PORT1_LANES_SHIFT        3
#define MRFLD_PORT2_LANES_SHIFT        7
#define MRFLD_PORT3_LANES_SHIFT        8
#define MRFLD_PORT_CONFIG_MASK 0x000f03ff
#define MRFLD_PORT_CONFIGCODE_SHIFT    16
#define MRFLD_ALL_CSI_PORTS_OFF_MASK   0x7

#define CHV_PORT3_LANES_SHIFT		9
#define CHV_PORT_CONFIG_MASK		0x1f07ff

#define ISPSSPM1				0x3a
#define ISP_FREQ_STAT_MASK			(0x1f << ISP_FREQ_STAT_OFFSET)
#define ISP_REQ_FREQ_MASK			0x1f
#define ISP_FREQ_VALID_MASK			(0x1 << ISP_FREQ_VALID_OFFSET)
#define ISP_FREQ_STAT_OFFSET			0x18
#define ISP_REQ_GUAR_FREQ_OFFSET		0x8
#define ISP_REQ_FREQ_OFFSET			0x0
#define ISP_FREQ_VALID_OFFSET			0x7
#define ISP_FREQ_RULE_ANY			0x0

#define ISP_FREQ_457MHZ				0x1C9
#define ISP_FREQ_400MHZ				0x190
#define ISP_FREQ_320MHZ				0x140
#define ISP_FREQ_266MHZ				0x10a
#define ISP_FREQ_200MHZ				0xc8
#define HPLL_FREQ				0x640

#if defined(ISP2401)
#define ISP_FREQ_MAX	ISP_FREQ_320MHZ
#else
#define ISP_FREQ_MAX	ISP_FREQ_400MHZ
#endif
#endif /* ATOMISP_REGS_H */
