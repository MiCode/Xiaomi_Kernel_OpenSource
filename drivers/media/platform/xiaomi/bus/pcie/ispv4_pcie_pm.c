#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/msm_pcie.h>
#include "ispv4_pcie_pm.h"
#include <media/ispv4_defs.h>
#include "ispv4_boot.h"

#define ISPV4_PCI_LINK_DOWN		0
#define ISPV4_PCI_LINK_UP		1
void pci_reset_secondary_bus(struct pci_dev *dev)
{
	u16 ctrl;

	pci_read_config_word(dev, PCI_BRIDGE_CONTROL, &ctrl);
	ctrl |= PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_word(dev, PCI_BRIDGE_CONTROL, ctrl);

	/*
	 * PCI spec v3.0 7.6.4.2 requires minimum Trst of 1ms.  Double
	 * this to 2ms to ensure that we meet the minimum requirement.
	 */
	msleep(2);

	ctrl &= ~PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_word(dev, PCI_BRIDGE_CONTROL, ctrl);

	/*
	 * Trhfa for conventional PCI is 2^25 clock cycles.
	 * Assuming a minimum 33MHz clock this results in a 1s
	 * delay before we can consider subordinate devices to
	 * be re-initialized.  PCIe has some ways to shorten this,
	 * but we don't make use of them yet.
	 */
	ssleep(1);
}

static int ispv4_set_pci_config_space(struct ispv4_data *data, bool save)
{
	int ret = 0;

	if (save) {
		ret = pci_save_state(data->pci);
		data->saved_state = pci_store_saved_state(data->pci);
	} else {
		if (data->saved_state)
			pci_load_and_free_saved_state(data->pci, &data->saved_state);
		else
			pci_load_saved_state(data->pci, data->default_state);

		pci_restore_state(data->pci);
	}

	return ret;
}

static int ispv4_set_pci_link(struct ispv4_data *data, bool link_up)
{
	enum msm_pcie_pm_opt pm_ops;
	int retry_time = 0;
	int ret = 0;

	if (link_up)
		pm_ops = MSM_PCIE_RESUME;
	else
		pm_ops = MSM_PCIE_SUSPEND;

retry:
	ret = msm_pcie_pm_control(pm_ops, data->pci->bus->number, data->pci,
				  NULL, PM_OPTIONS_DEFAULT);
	if (ret) {
		dev_err(data->dev, "Failed to %s PCI link with default option, err = %d\n",
			link_up ? "resume" : "suspend", ret);
		if (link_up && retry_time++ < LINK_TRAINING_RETRY_MAX_TIMES) {
			dev_dbg(data->dev, "Retry PCI link training #%d\n",
				retry_time);
			goto retry;
		}
	}

	return ret;
}

extern void _pci_enable_dbi_by_cs(struct pci_dev *pdev, bool s);

int ispv4_resume_pci_link(struct ispv4_data *data)
{
	int ret = 0;

	if (!data->pci)
		return -ENODEV;

	if (data->pci_link_state == ISPV4_PCI_LINK_UP) {
		dev_warn(data->dev, "pci has been in linkup");
		return 0;
	}

	_pci_reset();
	ret = ispv4_set_pci_link(data, ISPV4_PCI_LINK_UP);
	if (ret) {
		dev_err(data->dev, "Failed to set link up status, err =  %d\n",
			ret);
		return ret;
	}

	_pci_enable_dbi_by_cs(data->pci, true);
	ispv4_resume_config_pci();
	_pci_enable_dbi_by_cs(data->pci, false);

	ret = pci_enable_device(data->pci);
	if (ret) {
		dev_err(data->dev, "Failed to enable PCI device, err = %d\n",
			ret);
		return ret;
	}

	ret = ispv4_set_pci_config_space(data, RESTORE_PCI_CONFIG_SPACE);
	if (ret) {
		dev_err(data->dev, "Failed to restore config space, err = %d\n",
			ret);
		return ret;
	}

	pci_set_master(data->pci);
	_pci_config_iatu_fast(data);

	data->pci_link_state = ISPV4_PCI_LINK_UP;

	return 0;
}
EXPORT_SYMBOL_GPL(ispv4_resume_pci_link);

int ispv4_suspend_pci_link(struct ispv4_data *data)
{
	int ret = 0;

	if (!data->pci)
		return -ENODEV;

	if (data->pci_link_state == ISPV4_PCI_LINK_DOWN) {
		dev_warn(data->dev, "pci has been in linkdown");
		return 0;
	}

	pci_clear_master(data->pci);

	ret = ispv4_set_pci_config_space(data, SAVE_PCI_CONFIG_SPACE);
	if (ret) {
		dev_err(data->dev, "Failed to save config space, err =  %d\n",
			ret);
		return ret;
	}

	pci_disable_device(data->pci);

	ret = pci_set_power_state(data->pci, PCI_D3hot);
	if (ret) {
		dev_err(data->dev, "Failed to set D3Hot, err =  %d\n", ret);
		return ret;
	}

	ret = ispv4_set_pci_link(data, ISPV4_PCI_LINK_DOWN);
	if (ret) {
		dev_err(data->dev, "Failed to set link down status, err =  %d\n",
			ret);
		return ret;
	}

	data->pci_link_state = ISPV4_PCI_LINK_DOWN;

	return 0;
}
EXPORT_SYMBOL_GPL(ispv4_suspend_pci_link);

int ispv4_suspend_pci_force(struct ispv4_data *data)
{

	enum msm_pcie_pm_opt pm_ops;
	int retry_time = 0;
	int ret = 0;
	pm_ops = MSM_PCIE_SUSPEND;

	if (data->saved_state) {
		kfree(data->saved_state);
		data->saved_state = NULL;
	}

retry:
	ret = msm_pcie_pm_control(pm_ops, data->pci->bus->number, data->pci,
				  NULL, MSM_PCIE_CONFIG_FORCE_SUSP);
	if (ret) {
		dev_err(data->dev, "Failed to suspend PCI link with default option, err = %d\n",
			ret);
		if ( retry_time++ < LINK_TRAINING_RETRY_MAX_TIMES) {
			dev_dbg(data->dev, "Retry PCI link training #%d\n",
				retry_time);
			goto retry;
		}
	}
	data->pci_link_state = ISPV4_PCI_LINK_DOWN;
	return ret;
}

EXPORT_SYMBOL_GPL(ispv4_suspend_pci_force);

void pci_config_l0s(struct pci_dev *pdev, int enable)
{
	if (enable)
		pcie_capability_clear_and_set_word(pdev, PCI_EXP_LNKCTL, 0, PCI_EXP_LNKCTL_ASPM_L0S);
	else
		pcie_capability_clear_and_set_word(pdev, PCI_EXP_LNKCTL, PCI_EXP_LNKCTL_ASPM_L0S, 0);

}

void pci_config_l1(struct pci_dev *pdev, int enable)
{
	if (enable)
		pcie_capability_clear_and_set_word(pdev, PCI_EXP_LNKCTL,
						   0, PCI_EXP_LNKCTL_ASPM_L1);
	else
		pcie_capability_clear_and_set_word(pdev, PCI_EXP_LNKCTL,
						   PCI_EXP_LNKCTL_ASPM_L1, 0);

}

void pci_config_l1_1(struct pci_dev *pdev, int enable)
{
	u32 l1ss_cap_id_offset, l1ss_ctl1_offset;
	u32 l1ss_ctl1_data;

	l1ss_cap_id_offset = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
	if (!l1ss_cap_id_offset) {
		dev_err(&pdev->dev, "Can not find ext cap\n");
		return;
	}

	l1ss_ctl1_offset = l1ss_cap_id_offset + PCI_L1SS_CTL1;

	if (enable) {
		pci_read_config_dword(pdev, l1ss_ctl1_offset, &l1ss_ctl1_data);
		l1ss_ctl1_data |= PCI_L1SS_CTL1_ASPM_L1_1;
		pci_write_config_dword(pdev, l1ss_ctl1_offset, l1ss_ctl1_data);
	}
	else {
		pci_read_config_dword(pdev, l1ss_ctl1_offset, &l1ss_ctl1_data);
		l1ss_ctl1_data &= ~PCI_L1SS_CTL1_ASPM_L1_1;
		pci_write_config_dword(pdev, l1ss_ctl1_offset, l1ss_ctl1_data);
	}
}

void pci_config_l1_2(struct pci_dev *pdev, int enable)
{
	u32 l1ss_cap_id_offset, l1ss_ctl1_offset;
	u32 l1ss_ctl1_data;

	l1ss_cap_id_offset = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
	if (!l1ss_cap_id_offset) {
		dev_err(&pdev->dev, "Can not find ext cap\n");
		return;
	}

	l1ss_ctl1_offset = l1ss_cap_id_offset + PCI_L1SS_CTL1;

	if (enable) {
		pci_read_config_dword(pdev, l1ss_ctl1_offset, &l1ss_ctl1_data);
		l1ss_ctl1_data |= PCI_L1SS_CTL1_ASPM_L1_2;
		pci_write_config_dword(pdev, l1ss_ctl1_offset, l1ss_ctl1_data);
	}
	else {
		pci_read_config_dword(pdev, l1ss_ctl1_offset, &l1ss_ctl1_data);
		l1ss_ctl1_data &= ~PCI_L1SS_CTL1_ASPM_L1_2;
		pci_write_config_dword(pdev, l1ss_ctl1_offset, l1ss_ctl1_data);
	}

}

int ispv4_pci_linksta_ctl(struct pci_dev *pdev, enum ispv4_link_sta st)
{
	int ret = 0;
	const char* msg[] = {
        "ISPV4_ENABLE_L0s",
        "ISPV4_DISABLE_L0s",
        "ISPV4_ENABLE_L1",
        "ISPV4_DISABLE_L1",
        "ISPV4_ENABLE_L1_1",
        "ISPV4_DISABLE_L1_1",
        "ISPV4_ENABLE_L1_2",
        "ISPV4_DISABLE_L1_2",
	};
	switch(st) {
		case ISPV4_ENABLE_L0s:
			pci_config_l0s(pdev, 1);
			break;
		case ISPV4_DISABLE_L0s:
			pci_config_l0s(pdev, 0);
			break;
		case ISPV4_ENABLE_L1:
			pci_config_l1(pdev, 1);
			break;
		case ISPV4_DISABLE_L1:
			pci_config_l1(pdev, 0);
			break;
		case ISPV4_ENABLE_L1_1:
			pci_config_l1_1(pdev, 1);
			break;
		case ISPV4_DISABLE_L1_1:
			pci_config_l1_1(pdev, 0);
			break;
		case ISPV4_ENABLE_L1_2:
			pci_config_l1_2(pdev, 1);
			break;
		case ISPV4_DISABLE_L1_2:
			pci_config_l1_2(pdev, 0);
			break;
		default:
			dev_err(&pdev->dev, "err para\n");
			return -1;
	}

	dev_info(&pdev->dev, "set link state %s\n", msg[st]);

	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_pci_linksta_ctl);

int ispv4_set_linkspeed(struct pci_dev *pdev, int link_speed)
{
	uint16_t link_exp_ctrl2;
	uint16_t link_exp_linksta;
	uint16_t link_exp_linkctl;
	uint32_t timeout = 100;

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL2, &link_exp_ctrl2);
	link_exp_ctrl2 &= ~(PCI_EXP_LNKCTL2_TLS_2_5GT | PCI_EXP_LNKCTL2_TLS_5_0GT
			    | PCI_EXP_LNKCTL2_TLS_8_0GT | PCI_EXP_LNKCTL2_TLS_16_0GT
			    | PCI_EXP_LNKCTL2_TLS_32_0GT);
	switch (link_speed) {
		case GEN1_SPEED:
			link_exp_ctrl2 |= PCI_EXP_LNKCTL2_TLS_2_5GT;
			break;
		case GEN2_SPEED:
			link_exp_ctrl2 |= PCI_EXP_LNKCTL2_TLS_5_0GT;
			break;
		case GEN3_SPEED:
			link_exp_ctrl2 |= PCI_EXP_LNKCTL2_TLS_8_0GT;
			break;
		case GEN4_SPEED:
			link_exp_ctrl2 |= PCI_EXP_LNKCTL2_TLS_16_0GT;
			break;
		default:
			dev_err(&pdev->dev, "err para\n");
			return -1;
	}

	pcie_capability_write_word(pdev, PCI_EXP_LNKCTL2, link_exp_ctrl2);

	while(timeout--) {
		pcie_capability_read_word(pdev, PCI_EXP_LNKSTA, &link_exp_linksta);
		if (!(link_exp_ctrl2 & PCI_EXP_LNKSTA_LT))
			break;
	}
	if (timeout == 0)
		return -1;

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &link_exp_linkctl);
	link_exp_linkctl |= PCI_EXP_LNKCTL_RL;
	pcie_capability_write_word(pdev, PCI_EXP_LNKCTL, link_exp_linkctl);

	msleep(1);
	return 0;
}
EXPORT_SYMBOL_GPL(ispv4_set_linkspeed);

uint16_t ispv4_get_linkspeed(struct pci_dev *pdev)
{
	uint16_t link_status;
	pcie_capability_read_word(pdev, PCI_EXP_LNKSTA, &link_status);

	return link_status & 0x7;
}
EXPORT_SYMBOL_GPL(ispv4_get_linkspeed);
