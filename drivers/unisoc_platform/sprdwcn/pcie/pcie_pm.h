/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PCIE_PM_H__
#define __PCIE_PM_H__

/* B = 0x70 (PCI_CAP_ID_EXP) ID = 0x10 */
#define VF_LINK_CAPABILITIES_REG		0x0C
#define PCIE_CAP_ACTIVE_STATE_LINK_PM_SUPPORT	((0x1 << 10) | (0x1 << 11))

#define VF_LINK_CONTROL_LINK_STATUS_REG		0x10
#define PCIE_CAP_ACTIVE_STATE_LINK_PM_CONTROL	(0x1 << 0)
#define PCIE_CAP_EN_CLK_POWER_MAN		(0x1 << 8)

#define DEVICE_CONTROL2_DEVICE_STATUS2_REG	0x28
#define PCIE_CAP_LTR_EN				(0x1 << 10)

#define PL_LTR_LATENCY_OFF			0xB30
#define SNOOP_LATENCY_VALUE			0xffff
#define SNOOP_LATENCY_SCALE			0xff0ff

/* B = 0x150(L1SS PM Substatus) ID=0x001E */
#define L1SUB_CONTROL1_REG			0x8
#define L1_1_ASPM_EN				(0x1 << 3)
#define L1_2_ASPM_EN				(0x1 << 2)
#define L1_2_PCIPM_EN				0x0
#define T_COMMON_MODE				(0x2 << 8)
#define L1_2_TH_VAL				(0x80 << 16)

#define L1SUB_CONTROL2_REG			0xC
#define T_POWER_ON_VALUE			(0x1 << 3)
#define T_POWER_ON_SCALE			((0x1 << 0) | (0x1 << 1))

void wcn_aspm_enable(struct pci_dev *pdev);

#endif
