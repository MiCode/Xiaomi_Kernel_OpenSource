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

#include <linux/pci.h>

#include "pcie_pm.h"
#include "wcn_dbg.h"

struct aspm_register_info {
	u32 support:2;
	u32 enabled:2;
	u32 latency_encoding_l0s;
	u32 latency_encoding_l1;

	/* L1 substates */
	u32 l1ss_cap_ptr;
	u32 l1ss_cap;
	u32 l1ss_ctl1;
	u32 l1ss_ctl2;
};

static void pcie_get_aspm_reg(struct pci_dev *pdev,
			      struct aspm_register_info *info)
{
	u16 reg16;
	u32 reg32;

	pcie_capability_read_dword(pdev, PCI_EXP_LNKCAP, &reg32);
	info->support = (reg32 & PCI_EXP_LNKCAP_ASPMS) >> 10;
	info->latency_encoding_l0s = (reg32 & PCI_EXP_LNKCAP_L0SEL) >> 12;
	info->latency_encoding_l1  = (reg32 & PCI_EXP_LNKCAP_L1EL) >> 15;
	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &reg16);
	info->enabled = reg16 & PCI_EXP_LNKCTL_ASPMC;

	/* Read L1 PM substate capabilities */
	info->l1ss_cap = info->l1ss_ctl1 = info->l1ss_ctl2 = 0;
	info->l1ss_cap_ptr = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
	if (!info->l1ss_cap_ptr)
		return;
	pci_read_config_dword(pdev, info->l1ss_cap_ptr + PCI_L1SS_CAP,
			      &info->l1ss_cap);
	if (!(info->l1ss_cap & PCI_L1SS_CAP_L1_PM_SS)) {
		info->l1ss_cap = 0;
		return;
	}
	pci_read_config_dword(pdev, info->l1ss_cap_ptr + PCI_L1SS_CTL1,
			      &info->l1ss_ctl1);
	pci_read_config_dword(pdev, info->l1ss_cap_ptr + PCI_L1SS_CTL2,
			      &info->l1ss_ctl2);
}

/*
 * 1. 0x7c set [11:10] = 2'b11
 * 2. 0x80 set [0] = 1'b1
 */
void wcn_aspm_l0s(struct pci_dev *pdev)
{
	u32 val;

	pci_read_config_dword(pdev,
			      pdev->pcie_cap + VF_LINK_CAPABILITIES_REG,
			      &val);
	val |= PCIE_CAP_ACTIVE_STATE_LINK_PM_SUPPORT;
	pci_write_config_dword(pdev,
			       pdev->pcie_cap + VF_LINK_CAPABILITIES_REG,
			       val);
	WCN_INFO("EP: %x=0x%x\n",
		 pdev->pcie_cap + VF_LINK_CAPABILITIES_REG,
		 val);

	pci_read_config_dword(pdev,
			      pdev->pcie_cap + VF_LINK_CONTROL_LINK_STATUS_REG,
			      &val);
	val |= PCIE_CAP_ACTIVE_STATE_LINK_PM_CONTROL;
	pci_write_config_dword(pdev,
			       pdev->pcie_cap + VF_LINK_CONTROL_LINK_STATUS_REG,
			       val);
	WCN_INFO("EP: %x=0x%x\n",
		 pdev->pcie_cap + VF_LINK_CONTROL_LINK_STATUS_REG,
		 val);

}

/*
 * 1. 0x7c set [11:10] = 2'b11 Read only, no need to set
 * 2. 0x80 set [1] = 1'b1
 *
 * lookat -s 0x0030ac12 0x20c0007c
 * lookat -s 0x30110002 0x20c00080
 */
void wcn_aspm_l1(struct pci_dev *pdev)
{
	u32 val;

	pci_read_config_dword(pdev,
			      pdev->pcie_cap + VF_LINK_CAPABILITIES_REG,
			      &val);
	val |= PCIE_CAP_ACTIVE_STATE_LINK_PM_SUPPORT;
	pci_write_config_dword(pdev,
			       pdev->pcie_cap + VF_LINK_CAPABILITIES_REG,
			       val);
	WCN_INFO("EP: %x=0x%x\n",
		 pdev->pcie_cap + VF_LINK_CAPABILITIES_REG,
		 val);

	pci_read_config_dword(pdev,
			      pdev->pcie_cap + VF_LINK_CONTROL_LINK_STATUS_REG,
			      &val);
	val |= (PCIE_CAP_ACTIVE_STATE_LINK_PM_CONTROL << 1);
	pci_write_config_dword(pdev,
			       pdev->pcie_cap + VF_LINK_CONTROL_LINK_STATUS_REG,
			       val);
	WCN_INFO("EP: %x=0x%x\n",
		 pdev->pcie_cap + VF_LINK_CONTROL_LINK_STATUS_REG,
		 val);
}

/*
 * 1. 0x158 set [3] = 1'b1
 * 2. 0x7c set [11:10] = 2'b11
 * 3. 0x80 set [1] = 1'b1
 *
 * lookat -s 0x00000a08 0x20c00158
 * lookat -s 0x0030ac12 0x20c0007c
 * lookat -s 0x30110002 0x20c00080
 *
 * wcn_check -s 0x8 0x40180158
 * wcn_check -s 0x44cc11 0x4018007c
 * wcn_check -s 0x10110002 0x40180080
 */
void wcn_aspm_l1_1(struct pci_dev *pdev)
{
	struct aspm_register_info dwreg;

	pcie_get_aspm_reg(pdev, &dwreg);
	WCN_INFO("EP: addr:%x=0x%x\n",
		 dwreg.l1ss_cap_ptr + PCI_L1SS_CTL1,
		 dwreg.l1ss_ctl1);
	dwreg.l1ss_ctl1 |= PCI_L1SS_CTL1_ASPM_L1_1;
	pci_write_config_dword(pdev,
			       dwreg.l1ss_cap_ptr + PCI_L1SS_CTL1,
			       dwreg.l1ss_ctl1);
	WCN_INFO("EP: addr:%x=0x%x\n",
		 dwreg.l1ss_cap_ptr + PCI_L1SS_CTL1,
		 dwreg.l1ss_ctl1);

	wcn_aspm_l1(pdev);
}

/*
 * 1. 0x15c set [3] = 1'b1, [1:0] =2'b00
 *
 * 2. 0x158 set REG(0x158) | (0x1 << 2) | (0x2 << 8) | (0x80 << 16)
 * 3. 0xB30 (PL_LTR_LATENCY_OFF) set REG(0xB30) | 0xffff
 * 4. 0x98 (PL_LTR_LATENCY_OFF) set REG(0xB30) | (0x1 << 10)
 *
 * 5. gen2_aspm_l1(pp);
 *
 * lookat -s 0x00000028 0x20c0015c
 * lookat -s 0x800a04 0x20c00158
 * lookat -s 0xffff 0x20c00B30
 * lookat -s 0x400 0x20c00098
 * lookat -s 0x0030ac12 0x20c0007c
 * lookat -s 0x30110002 0x20c00080
 *
 * wcn_check -s 0x28 0x4018015c
 * wcn_check -s 0x800204 0x40180158
 * wcn_check -s 0xffff 0x40180B30
 * wcn_check -s 0x400 0x40180098
 * wcn_check -s 0x44cc11 0x4018007c
 * wcn_check -s 0x10110002 0x40180080
 */

void wcn_aspm_l1_2(struct pci_dev *pdev)
{
	u32 val;
	struct aspm_register_info dwreg;

	pcie_get_aspm_reg(pdev, &dwreg);
	WCN_INFO("EP: addr:%x=0x%x\n",
		 dwreg.l1ss_cap_ptr + PCI_L1SS_CTL1,
		 dwreg.l1ss_ctl1);
	WCN_INFO("EP: addr:%x=0x%x\n",
		 dwreg.l1ss_cap_ptr + PCI_L1SS_CTL2,
		 dwreg.l1ss_ctl2);

	/* l1ss_ctl2: 15C */
	dwreg.l1ss_ctl2 = (dwreg.l1ss_ctl2 | T_POWER_ON_VALUE) &
			   (~T_POWER_ON_SCALE);
	pci_write_config_dword(pdev,
			       dwreg.l1ss_cap_ptr + PCI_L1SS_CTL2,
			       dwreg.l1ss_ctl2);
	WCN_INFO("EP: addr:%x=0x%x\n",
		 dwreg.l1ss_cap_ptr + PCI_L1SS_CTL2,
		 dwreg.l1ss_ctl2);

	/* l1ss_ctl1: 158 */
	dwreg.l1ss_ctl1 |= PCI_L1SS_CTL1_ASPM_L1_2 | T_COMMON_MODE |
			    L1_2_TH_VAL;
	pci_write_config_dword(pdev,
			       dwreg.l1ss_cap_ptr + PCI_L1SS_CTL1,
			       dwreg.l1ss_ctl1);
	WCN_INFO("EP: addr:%x=0x%x\n",
		 dwreg.l1ss_cap_ptr + PCI_L1SS_CTL1,
		 dwreg.l1ss_ctl1);


	pci_read_config_dword(pdev,
			      pdev->pcie_cap +
			      DEVICE_CONTROL2_DEVICE_STATUS2_REG,
			      &val);
	val |= PCIE_CAP_LTR_EN;
	pci_write_config_dword(pdev,
			       pdev->pcie_cap +
			       DEVICE_CONTROL2_DEVICE_STATUS2_REG,
			       val);
	WCN_INFO("EP: %x=0x%x\n",
		 pdev->pcie_cap + DEVICE_CONTROL2_DEVICE_STATUS2_REG,
		 val);

	wcn_aspm_l1(pdev);
}
/* TODO: RC chip bug, need work around */
#if 0
/*
 * 1.0x158 set [0] = 1'b1
 * 2.0x080 [8] = 1'b1
 * 3.gen2_aspm_l1(pdev);
 */
void wcn_aspm_l1clk(struct pci_dev *pdev)
{
	u32 val;
	struct aspm_register_info dwreg;

	pcie_get_aspm_reg(pdev, &dwreg);
	WCN_INFO("EP: addr:%x=0x%x\n",
		 dwreg.l1ss_cap_ptr + PCI_L1SS_CTL1,
		 dwreg.l1ss_ctl1);

	/* l1ss_ctl1: 158 */
	dwreg.l1ss_ctl1 |= L1_2_PCIPM_EN;
	pci_write_config_dword(pdev,
			       dwreg.l1ss_cap_ptr + PCI_L1SS_CTL1,
			       dwreg.l1ss_ctl1);
	WCN_INFO("EP: addr:%x=0x%x\n",
		 dwreg.l1ss_cap_ptr + PCI_L1SS_CTL1,
		 dwreg.l1ss_ctl1);

	pci_read_config_dword(pdev,
			      pdev->pcie_cap + VF_LINK_CONTROL_LINK_STATUS_REG,
			      &val);
	val |= PCIE_CAP_EN_CLK_POWER_MAN;
	pci_write_config_dword(pdev,
			       pdev->pcie_cap + VF_LINK_CONTROL_LINK_STATUS_REG,
			       val);
	WCN_INFO("EP: %x=0x%x\n",
		 pdev->pcie_cap + VF_LINK_CONTROL_LINK_STATUS_REG,
		 val);

	wcn_aspm_l1(pdev);
}
#endif
void wcn_aspm_enable(struct pci_dev *pdev)
{
	wcn_aspm_l0s(pdev);
	wcn_aspm_l1_1(pdev);
	wcn_aspm_l1_2(pdev);
	wcn_aspm_l1(pdev);
}
