/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/memblock.h>
#include <linux/completion.h>
#include <soc/qcom/ramdump.h>

#include "main.h"
#include "bus.h"
#include "debug.h"
#include "pci.h"

#define PCI_LINK_UP			1
#define PCI_LINK_DOWN			0

#define SAVE_PCI_CONFIG_SPACE		1
#define RESTORE_PCI_CONFIG_SPACE	0

#define PM_OPTIONS_DEFAULT		0
#define PM_OPTIONS_LINK_DOWN \
	(MSM_PCIE_CONFIG_NO_CFG_RESTORE | MSM_PCIE_CONFIG_LINKDOWN)

#define PCI_BAR_NUM			0

#define PCI_DMA_MASK_32_BIT		32
#define PCI_DMA_MASK_64_BIT		64

#define MHI_NODE_NAME			"qcom,mhi"
#define MHI_MSI_NAME			"MHI"

#define DEFAULT_M3_FILE_NAME		"m3.bin"
#define DEFAULT_FW_FILE_NAME		"amss.bin"
#define FW_V2_FILE_NAME			"amss20.bin"
#define FW_V2_NUMBER			2

#define WAKE_MSI_NAME			"WAKE"

#define FW_ASSERT_TIMEOUT		5000
#define DEV_RDDM_TIMEOUT		5000

#ifdef CONFIG_CNSS_EMULATION
#define EMULATION_HW			1
#else
#define EMULATION_HW			0
#endif

static DEFINE_SPINLOCK(pci_link_down_lock);
static DEFINE_SPINLOCK(pci_reg_window_lock);

#define MHI_TIMEOUT_OVERWRITE_MS	(plat_priv->ctrl_params.mhi_timeout)

#define QCA6390_PCIE_REMAP_BAR_CTRL_OFFSET	0x310C

#define QCA6390_CE_SRC_RING_REG_BASE		0xA00000
#define QCA6390_CE_DST_RING_REG_BASE		0xA01000
#define QCA6390_CE_COMMON_REG_BASE		0xA18000

#define QCA6390_CE_SRC_RING_BASE_LSB_OFFSET	0x0
#define QCA6390_CE_SRC_RING_BASE_MSB_OFFSET	0x4
#define QCA6390_CE_SRC_RING_ID_OFFSET		0x8
#define QCA6390_CE_SRC_RING_MISC_OFFSET		0x10
#define QCA6390_CE_SRC_CTRL_OFFSET		0x58
#define QCA6390_CE_SRC_R0_CE_CH_SRC_IS_OFFSET	0x5C
#define QCA6390_CE_SRC_RING_HP_OFFSET		0x400
#define QCA6390_CE_SRC_RING_TP_OFFSET		0x404

#define QCA6390_CE_DEST_RING_BASE_LSB_OFFSET	0x0
#define QCA6390_CE_DEST_RING_BASE_MSB_OFFSET	0x4
#define QCA6390_CE_DEST_RING_ID_OFFSET		0x8
#define QCA6390_CE_DEST_RING_MISC_OFFSET	0x10
#define QCA6390_CE_DEST_CTRL_OFFSET		0xB0
#define QCA6390_CE_CH_DST_IS_OFFSET		0xB4
#define QCA6390_CE_CH_DEST_CTRL2_OFFSET		0xB8
#define QCA6390_CE_DEST_RING_HP_OFFSET		0x400
#define QCA6390_CE_DEST_RING_TP_OFFSET		0x404

#define QCA6390_CE_STATUS_RING_BASE_LSB_OFFSET	0x58
#define QCA6390_CE_STATUS_RING_BASE_MSB_OFFSET	0x5C
#define QCA6390_CE_STATUS_RING_ID_OFFSET	0x60
#define QCA6390_CE_STATUS_RING_MISC_OFFSET	0x68
#define QCA6390_CE_STATUS_RING_HP_OFFSET	0x408
#define QCA6390_CE_STATUS_RING_TP_OFFSET	0x40C

#define QCA6390_CE_COMMON_GXI_ERR_INTS		0x14
#define QCA6390_CE_COMMON_GXI_ERR_STATS		0x18
#define QCA6390_CE_COMMON_GXI_WDOG_STATUS	0x2C
#define QCA6390_CE_COMMON_TARGET_IE_0		0x48
#define QCA6390_CE_COMMON_TARGET_IE_1		0x4C

#define QCA6390_CE_REG_INTERVAL			0x2000

#define SHADOW_REG_COUNT			36
#define QCA6390_PCIE_SHADOW_REG_VALUE_0		0x1E03024
#define QCA6390_PCIE_SHADOW_REG_VALUE_35	0x1E030B0

#define SHADOW_REG_INTER_COUNT			43
#define QCA6390_PCIE_SHADOW_REG_INTER_0		0x1E05000
#define QCA6390_PCIE_SHADOW_REG_HUNG		0x1E050A8

#define QDSS_APB_DEC_CSR_BASE			0x1C01000

#define QDSS_APB_DEC_CSR_ETRIRQCTRL_OFFSET	0x6C
#define QDSS_APB_DEC_CSR_PRESERVEETF_OFFSET	0x70
#define QDSS_APB_DEC_CSR_PRESERVEETR0_OFFSET	0x74
#define QDSS_APB_DEC_CSR_PRESERVEETR1_OFFSET	0x78

#define MAX_UNWINDOWED_ADDRESS			0x80000
#define WINDOW_ENABLE_BIT			0x40000000
#define WINDOW_SHIFT				19
#define WINDOW_VALUE_MASK			0x3F
#define WINDOW_START				MAX_UNWINDOWED_ADDRESS
#define WINDOW_RANGE_MASK			0x7FFFF

static struct cnss_pci_reg ce_src[] = {
	{ "SRC_RING_BASE_LSB", QCA6390_CE_SRC_RING_BASE_LSB_OFFSET },
	{ "SRC_RING_BASE_MSB", QCA6390_CE_SRC_RING_BASE_MSB_OFFSET },
	{ "SRC_RING_ID", QCA6390_CE_SRC_RING_ID_OFFSET },
	{ "SRC_RING_MISC", QCA6390_CE_SRC_RING_MISC_OFFSET },
	{ "SRC_CTRL", QCA6390_CE_SRC_CTRL_OFFSET },
	{ "SRC_R0_CE_CH_SRC_IS", QCA6390_CE_SRC_R0_CE_CH_SRC_IS_OFFSET },
	{ "SRC_RING_HP", QCA6390_CE_SRC_RING_HP_OFFSET },
	{ "SRC_RING_TP", QCA6390_CE_SRC_RING_TP_OFFSET },
	{ NULL },
};

static struct cnss_pci_reg ce_dst[] = {
	{ "DEST_RING_BASE_LSB", QCA6390_CE_DEST_RING_BASE_LSB_OFFSET },
	{ "DEST_RING_BASE_MSB", QCA6390_CE_DEST_RING_BASE_MSB_OFFSET },
	{ "DEST_RING_ID", QCA6390_CE_DEST_RING_ID_OFFSET },
	{ "DEST_RING_MISC", QCA6390_CE_DEST_RING_MISC_OFFSET },
	{ "DEST_CTRL", QCA6390_CE_DEST_CTRL_OFFSET },
	{ "CE_CH_DST_IS", QCA6390_CE_CH_DST_IS_OFFSET },
	{ "CE_CH_DEST_CTRL2", QCA6390_CE_CH_DEST_CTRL2_OFFSET },
	{ "DEST_RING_HP", QCA6390_CE_DEST_RING_HP_OFFSET },
	{ "DEST_RING_TP", QCA6390_CE_DEST_RING_TP_OFFSET },
	{ "STATUS_RING_BASE_LSB", QCA6390_CE_STATUS_RING_BASE_LSB_OFFSET },
	{ "STATUS_RING_BASE_MSB", QCA6390_CE_STATUS_RING_BASE_MSB_OFFSET },
	{ "STATUS_RING_ID", QCA6390_CE_STATUS_RING_ID_OFFSET },
	{ "STATUS_RING_MISC", QCA6390_CE_STATUS_RING_MISC_OFFSET },
	{ "STATUS_RING_HP", QCA6390_CE_STATUS_RING_HP_OFFSET },
	{ "STATUS_RING_TP", QCA6390_CE_STATUS_RING_TP_OFFSET },
	{ NULL },
};

static struct cnss_pci_reg ce_cmn[] = {
	{ "GXI_ERR_INTS", QCA6390_CE_COMMON_GXI_ERR_INTS },
	{ "GXI_ERR_STATS", QCA6390_CE_COMMON_GXI_ERR_STATS },
	{ "GXI_WDOG_STATUS", QCA6390_CE_COMMON_GXI_WDOG_STATUS },
	{ "TARGET_IE_0", QCA6390_CE_COMMON_TARGET_IE_0 },
	{ "TARGET_IE_1", QCA6390_CE_COMMON_TARGET_IE_1 },
	{ NULL },
};

static struct cnss_pci_reg qdss_csr[] = {
	{ "QDSSCSR_ETRIRQCTRL", QDSS_APB_DEC_CSR_ETRIRQCTRL_OFFSET },
	{ "QDSSCSR_PRESERVEETF", QDSS_APB_DEC_CSR_PRESERVEETF_OFFSET },
	{ "QDSSCSR_PRESERVEETR0", QDSS_APB_DEC_CSR_PRESERVEETR0_OFFSET },
	{ "QDSSCSR_PRESERVEETR1", QDSS_APB_DEC_CSR_PRESERVEETR1_OFFSET },
	{ NULL },
};

static int cnss_pci_check_link_status(struct cnss_pci_data *pci_priv)
{
	u16 device_id;

	if (pci_priv->pci_link_state == PCI_LINK_DOWN) {
		cnss_pr_dbg("PCIe link is suspended\n");
		return -EIO;
	}

	if (pci_priv->pci_link_down_ind) {
		cnss_pr_err("PCIe link is down\n");
		return -EIO;
	}

	pci_read_config_word(pci_priv->pci_dev, PCI_DEVICE_ID, &device_id);
	if (device_id != pci_priv->device_id)  {
		cnss_fatal_err("PCI device ID mismatch, link possibly down, current read ID: 0x%x, record ID: 0x%x\n",
			       device_id, pci_priv->device_id);
		return -EIO;
	}

	return 0;
}

static void cnss_pci_select_window(struct cnss_pci_data *pci_priv, u32 offset)
{
	u32 window = (offset >> WINDOW_SHIFT) & WINDOW_VALUE_MASK;

	if (window != pci_priv->remap_window) {
		writel_relaxed(WINDOW_ENABLE_BIT | window,
			       QCA6390_PCIE_REMAP_BAR_CTRL_OFFSET +
			       pci_priv->bar);
		pci_priv->remap_window = window;
		cnss_pr_dbg("Config PCIe remap window register to 0x%x\n",
			    WINDOW_ENABLE_BIT | window);
	}
}

static int cnss_pci_reg_read(struct cnss_pci_data *pci_priv,
			     u32 offset, u32 *val)
{
	int ret;

	ret = cnss_pci_check_link_status(pci_priv);
	if (ret)
		return ret;

	if (pci_priv->pci_dev->device == QCA6174_DEVICE_ID ||
	    offset < MAX_UNWINDOWED_ADDRESS) {
		*val = readl_relaxed(pci_priv->bar + offset);
		return 0;
	}

	spin_lock_bh(&pci_reg_window_lock);
	cnss_pci_select_window(pci_priv, offset);

	*val = readl_relaxed(pci_priv->bar + WINDOW_START +
			     (offset & WINDOW_RANGE_MASK));
	spin_unlock_bh(&pci_reg_window_lock);

	return 0;
}

static void cnss_pci_disable_l1(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct pci_dev *pdev = pci_priv->pci_dev;
	bool disable_l1;
	u32 lnkctl_offset;
	u32 val;

	disable_l1 = of_property_read_bool(plat_priv->dev_node,
					   "pcie-disable-l1");
	cnss_pr_dbg("disable_l1 %d\n", disable_l1);

	if (!disable_l1)
		return;

	lnkctl_offset = pdev->pcie_cap + PCI_EXP_LNKCTL;
	pci_read_config_dword(pdev, lnkctl_offset, &val);
	cnss_pr_dbg("lnkctl 0x%x\n", val);

	val &= ~PCI_EXP_LNKCTL_ASPM_L1;
	pci_write_config_dword(pdev, lnkctl_offset, val);
}

static void cnss_pci_disable_l1ss(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct pci_dev *pdev = pci_priv->pci_dev;
	bool disable_l1ss;
	u32 l1ss_cap_id_offset;
	u32 l1ss_ctl1_offset;
	u32 val;

	disable_l1ss = of_property_read_bool(plat_priv->dev_node,
					     "pcie-disable-l1ss");
	cnss_pr_dbg("disable_l1ss %d\n", disable_l1ss);

	if (!disable_l1ss)
		return;

	l1ss_cap_id_offset = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
	if (!l1ss_cap_id_offset) {
		cnss_pr_dbg("could not find L1ss capability register\n");
		return;
	}

	l1ss_ctl1_offset = l1ss_cap_id_offset + PCI_L1SS_CTL1;

	pci_read_config_dword(pdev, l1ss_ctl1_offset, &val);
	cnss_pr_dbg("l1ss_ctl1 0x%x\n", val);

	val &= ~(PCI_L1SS_CTL1_PCIPM_L1_1 | PCI_L1SS_CTL1_PCIPM_L1_2 |
		 PCI_L1SS_CTL1_ASPM_L1_1 | PCI_L1SS_CTL1_ASPM_L1_2);
	pci_write_config_dword(pdev, l1ss_ctl1_offset, val);
}

static int cnss_set_pci_config_space(struct cnss_pci_data *pci_priv, bool save)
{
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	bool link_down_or_recovery;

	if (!plat_priv)
		return -ENODEV;

	link_down_or_recovery = pci_priv->pci_link_down_ind ||
		(test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state));

	if (save) {
		if (link_down_or_recovery) {
			pci_priv->saved_state = NULL;
		} else {
			pci_save_state(pci_dev);
			pci_priv->saved_state = pci_store_saved_state(pci_dev);
		}
	} else {
		if (link_down_or_recovery) {
			pci_load_saved_state(pci_dev, pci_priv->default_state);
			pci_restore_state(pci_dev);
		} else if (pci_priv->saved_state) {
			pci_load_and_free_saved_state(pci_dev,
						      &pci_priv->saved_state);
			pci_restore_state(pci_dev);
		}

		cnss_pci_disable_l1ss(pci_priv);
	}

	return 0;
}

static int cnss_set_pci_link(struct cnss_pci_data *pci_priv, bool link_up)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;

	ret = msm_pcie_pm_control(link_up ? MSM_PCIE_RESUME :
				  MSM_PCIE_SUSPEND,
				  pci_dev->bus->number,
				  pci_dev, NULL,
				  PM_OPTIONS_DEFAULT);
	if (ret) {
		cnss_pr_err("Failed to %s PCI link with default option, err = %d\n",
			    link_up ? "resume" : "suspend", ret);
		return ret;
	}

	return 0;
}

int cnss_suspend_pci_link(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv)
		return -ENODEV;

	cnss_pr_dbg("Suspending PCI link\n");
	if (!pci_priv->pci_link_state) {
		cnss_pr_info("PCI link is already suspended!\n");
		goto out;
	}

	pci_clear_master(pci_priv->pci_dev);

	ret = cnss_set_pci_config_space(pci_priv, SAVE_PCI_CONFIG_SPACE);
	if (ret)
		goto out;

	pci_disable_device(pci_priv->pci_dev);

	if (pci_priv->pci_dev->device != QCA6174_DEVICE_ID) {
		if (pci_set_power_state(pci_priv->pci_dev, PCI_D3hot))
			cnss_pr_err("Failed to set D3Hot, err =  %d\n", ret);
	}

	ret = cnss_set_pci_link(pci_priv, PCI_LINK_DOWN);
	if (ret)
		goto out;

	pci_priv->pci_link_state = PCI_LINK_DOWN;

	return 0;
out:
	return ret;
}

int cnss_resume_pci_link(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv)
		return -ENODEV;

	cnss_pr_dbg("Resuming PCI link\n");
	if (pci_priv->pci_link_state) {
		cnss_pr_info("PCI link is already resumed!\n");
		goto out;
	}

	ret = cnss_set_pci_link(pci_priv, PCI_LINK_UP);
	if (ret)
		goto out;

	pci_priv->pci_link_state = PCI_LINK_UP;

	if (pci_priv->pci_dev->device != QCA6174_DEVICE_ID) {
		ret = pci_set_power_state(pci_priv->pci_dev, PCI_D0);
		if (ret) {
			cnss_pr_err("Failed to set D0, err = %d\n", ret);
			goto out;
		}
	}

	ret = cnss_set_pci_config_space(pci_priv, RESTORE_PCI_CONFIG_SPACE);
	if (ret)
		goto out;

	ret = pci_enable_device(pci_priv->pci_dev);
	if (ret) {
		cnss_pr_err("Failed to enable PCI device, err = %d\n", ret);
		goto out;
	}

	pci_set_master(pci_priv->pci_dev);

	if (pci_priv->pci_link_down_ind)
		pci_priv->pci_link_down_ind = false;

	return 0;
out:
	return ret;
}

int cnss_pci_link_down(struct device *dev)
{
	unsigned long flags;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL!\n");
		return -EINVAL;
	}

	plat_priv = pci_priv->plat_priv;
	if (test_bit(ENABLE_PCI_LINK_DOWN_PANIC,
		     &plat_priv->ctrl_params.quirks))
		panic("cnss: PCI link is down!\n");

	spin_lock_irqsave(&pci_link_down_lock, flags);
	if (pci_priv->pci_link_down_ind) {
		cnss_pr_dbg("PCI link down recovery is in progress, ignore!\n");
		spin_unlock_irqrestore(&pci_link_down_lock, flags);
		return -EINVAL;
	}
	pci_priv->pci_link_down_ind = true;
	spin_unlock_irqrestore(&pci_link_down_lock, flags);

	cnss_pr_err("PCI link down is detected by host driver, schedule recovery!\n");

	cnss_schedule_recovery(dev, CNSS_REASON_LINK_DOWN);

	return 0;
}
EXPORT_SYMBOL(cnss_pci_link_down);

int cnss_pci_is_device_down(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_pci_data *pci_priv;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	pci_priv = plat_priv->bus_priv;
	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	return test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state) |
		pci_priv->pci_link_down_ind;
}
EXPORT_SYMBOL(cnss_pci_is_device_down);

void cnss_pci_lock_reg_window(struct device *dev, unsigned long *flags)
{
	spin_lock_bh(&pci_reg_window_lock);
}
EXPORT_SYMBOL(cnss_pci_lock_reg_window);

void cnss_pci_unlock_reg_window(struct device *dev, unsigned long *flags)
{
	spin_unlock_bh(&pci_reg_window_lock);
}
EXPORT_SYMBOL(cnss_pci_unlock_reg_window);

int cnss_pci_call_driver_probe(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;

	if (test_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state)) {
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		cnss_pr_dbg("Skip driver probe\n");
		goto out;
	}

	if (!pci_priv->driver_ops) {
		cnss_pr_err("driver_ops is NULL\n");
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state)) {
		ret = pci_priv->driver_ops->reinit(pci_priv->pci_dev,
						   pci_priv->pci_device_id);
		if (ret) {
			cnss_pr_err("Failed to reinit host driver, err = %d\n",
				    ret);
			goto out;
		}
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		complete(&plat_priv->recovery_complete);
	} else if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state)) {
		ret = pci_priv->driver_ops->probe(pci_priv->pci_dev,
						  pci_priv->pci_device_id);
		if (ret) {
			cnss_pr_err("Failed to probe host driver, err = %d\n",
				    ret);
			goto out;
		}
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
		set_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
	} else if (test_bit(CNSS_DRIVER_IDLE_RESTART,
			    &plat_priv->driver_state)) {
		ret = pci_priv->driver_ops->idle_restart(pci_priv->pci_dev,
			pci_priv->pci_device_id);
		if (ret) {
			cnss_pr_err("Failed to idle restart host driver, err = %d\n",
				    ret);
			goto out;
		}
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_IDLE_RESTART, &plat_priv->driver_state);
		complete(&plat_priv->power_up_complete);
	} else {
		complete(&plat_priv->power_up_complete);
	}

	return 0;

out:
	return ret;
}

int cnss_pci_call_driver_remove(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;

	if (test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state) ||
	    test_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state)) {
		cnss_pr_dbg("Skip driver remove\n");
		return 0;
	}

	if (!pci_priv->driver_ops) {
		cnss_pr_err("driver_ops is NULL\n");
		return -EINVAL;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state)) {
		pci_priv->driver_ops->shutdown(pci_priv->pci_dev);
	} else if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state) &&
		   test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state)) {
		pci_priv->driver_ops->remove(pci_priv->pci_dev);
		clear_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
		clear_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);
	} else if (test_bit(CNSS_DRIVER_IDLE_SHUTDOWN,
			    &plat_priv->driver_state)) {
		pci_priv->driver_ops->idle_shutdown(pci_priv->pci_dev);
		clear_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);
	}

	return 0;
}

int cnss_pci_call_driver_modem_status(struct cnss_pci_data *pci_priv,
				      int modem_current_status)
{
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		return -ENODEV;

	driver_ops = pci_priv->driver_ops;
	if (!driver_ops || !driver_ops->modem_status)
		return -EINVAL;

	driver_ops->modem_status(pci_priv->pci_dev, modem_current_status);

	return 0;
}

int cnss_pci_update_status(struct cnss_pci_data *pci_priv,
			   enum cnss_driver_status status)
{
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		return -ENODEV;

	driver_ops = pci_priv->driver_ops;
	if (!driver_ops || !driver_ops->update_status)
		return -EINVAL;

	cnss_pr_dbg("Update driver status: %d\n", status);

	driver_ops->update_status(pci_priv->pci_dev, status);

	return 0;
}

static void cnss_pci_dump_shadow_reg(struct cnss_pci_data *pci_priv)
{
	int i, j = 0, array_size = SHADOW_REG_COUNT + SHADOW_REG_INTER_COUNT;
	gfp_t gfp = GFP_KERNEL;
	u32 reg_offset;

	if (cnss_pci_check_link_status(pci_priv))
		return;

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	if (!pci_priv->debug_reg) {
		pci_priv->debug_reg = devm_kzalloc(&pci_priv->pci_dev->dev,
						   sizeof(*pci_priv->debug_reg)
						   * array_size, gfp);
		if (!pci_priv->debug_reg)
			return;
	}

	cnss_pr_dbg("Start to dump shadow registers\n");

	for (i = 0; i < SHADOW_REG_COUNT; i++, j++) {
		reg_offset = QCA6390_PCIE_SHADOW_REG_VALUE_0 + i * 4;
		pci_priv->debug_reg[j].offset = reg_offset;
		if (cnss_pci_reg_read(pci_priv, reg_offset,
				      &pci_priv->debug_reg[j].val))
			return;
	}

	for (i = 0; i < SHADOW_REG_INTER_COUNT; i++, j++) {
		reg_offset = QCA6390_PCIE_SHADOW_REG_INTER_0 + i * 4;
		pci_priv->debug_reg[j].offset = reg_offset;
		if (cnss_pci_reg_read(pci_priv, reg_offset,
				      &pci_priv->debug_reg[j].val))
			return;
	}
}

#ifdef CONFIG_CNSS2_DEBUG
static void cnss_pci_collect_dump(struct cnss_pci_data *pci_priv)
{
	cnss_pci_collect_dump_info(pci_priv, false);
	CNSS_ASSERT(0);
}
#else
static void cnss_pci_collect_dump(struct cnss_pci_data *pci_priv)
{
}
#endif

static int cnss_qca6174_powerup(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	ret = cnss_power_on_device(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to power on device, err = %d\n", ret);
		goto out;
	}

	ret = cnss_resume_pci_link(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to resume PCI link, err = %d\n", ret);
		goto power_off;
	}

	ret = cnss_pci_call_driver_probe(pci_priv);
	if (ret)
		goto suspend_link;

	return 0;
suspend_link:
	cnss_suspend_pci_link(pci_priv);
power_off:
	cnss_power_off_device(plat_priv);
out:
	return ret;
}

static int cnss_qca6174_shutdown(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	cnss_pm_request_resume(pci_priv);

	cnss_pci_call_driver_remove(pci_priv);

	cnss_request_bus_bandwidth(&plat_priv->plat_dev->dev,
				   CNSS_BUS_WIDTH_NONE);
	cnss_pci_set_monitor_wake_intr(pci_priv, false);
	cnss_pci_set_auto_suspended(pci_priv, 0);

	ret = cnss_suspend_pci_link(pci_priv);
	if (ret)
		cnss_pr_err("Failed to suspend PCI link, err = %d\n", ret);

	cnss_power_off_device(plat_priv);

	clear_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);
	clear_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state);

	return ret;
}

static void cnss_qca6174_crash_shutdown(struct cnss_pci_data *pci_priv)
{
	if (pci_priv->driver_ops && pci_priv->driver_ops->crash_shutdown)
		pci_priv->driver_ops->crash_shutdown(pci_priv->pci_dev);
}

static int cnss_qca6174_ramdump(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_ramdump_info *ramdump_info;
	struct ramdump_segment segment;

	ramdump_info = &plat_priv->ramdump_info;
	if (!ramdump_info->ramdump_size)
		return -EINVAL;

	memset(&segment, 0, sizeof(segment));
	segment.v_address = ramdump_info->ramdump_va;
	segment.size = ramdump_info->ramdump_size;
	ret = do_ramdump(ramdump_info->ramdump_dev, &segment, 1);

	return ret;
}

static int cnss_qca6290_powerup(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	unsigned int timeout;

	if (plat_priv->ramdump_info_v2.dump_data_valid ||
	    test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_DEINIT);
		cnss_pci_clear_dump_info(pci_priv);
	}

	ret = cnss_power_on_device(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to power on device, err = %d\n", ret);
		goto out;
	}

	ret = cnss_resume_pci_link(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to resume PCI link, err = %d\n", ret);
		goto power_off;
	}

	timeout = cnss_get_boot_timeout(&pci_priv->pci_dev->dev);

	ret = cnss_pci_start_mhi(pci_priv);
	if (ret) {
		cnss_fatal_err("Failed to start MHI, err = %d\n", ret);
		CNSS_ASSERT(0);
		if (!test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state) &&
		    !pci_priv->pci_link_down_ind && timeout)
			mod_timer(&plat_priv->fw_boot_timer,
				  jiffies + msecs_to_jiffies(timeout >> 1));
		return 0;
	}

	if (test_bit(USE_CORE_ONLY_FW, &plat_priv->ctrl_params.quirks)) {
		clear_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		return 0;
	}

	cnss_set_pin_connect_status(plat_priv);

	if (test_bit(QMI_BYPASS, &plat_priv->ctrl_params.quirks)) {
		ret = cnss_pci_call_driver_probe(pci_priv);
		if (ret)
			goto stop_mhi;
	} else if (timeout) {
		mod_timer(&plat_priv->fw_boot_timer,
			  jiffies + msecs_to_jiffies(timeout << 1));
	}

	return 0;

stop_mhi:
	cnss_pci_stop_mhi(pci_priv);
	cnss_suspend_pci_link(pci_priv);
power_off:
	cnss_power_off_device(plat_priv);
out:
	return ret;
}

static int cnss_qca6290_shutdown(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	cnss_pm_request_resume(pci_priv);

	cnss_pci_call_driver_remove(pci_priv);

	cnss_request_bus_bandwidth(&plat_priv->plat_dev->dev,
				   CNSS_BUS_WIDTH_NONE);
	cnss_pci_set_monitor_wake_intr(pci_priv, false);
	cnss_pci_set_auto_suspended(pci_priv, 0);

	if ((test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
	     test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state) ||
	     test_bit(CNSS_DRIVER_IDLE_RESTART, &plat_priv->driver_state) ||
	     test_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state)) &&
	    test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state)) {
		del_timer(&pci_priv->dev_rddm_timer);
		cnss_pci_collect_dump(pci_priv);
	}

	cnss_pci_stop_mhi(pci_priv);

	ret = cnss_suspend_pci_link(pci_priv);
	if (ret)
		cnss_pr_err("Failed to suspend PCI link, err = %d\n", ret);

	cnss_power_off_device(plat_priv);

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_dbg("recovery sleep start\n");
		msleep(200);
		cnss_pr_dbg("recovery sleep 200ms done\n");
	}

	pci_priv->remap_window = 0;

	clear_bit(CNSS_FW_READY, &plat_priv->driver_state);
	clear_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state);
	clear_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);
	clear_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state);

	return ret;
}

static void cnss_qca6290_crash_shutdown(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	cnss_pr_dbg("Crash shutdown with driver_state 0x%lx\n",
		    plat_priv->driver_state);

	cnss_pci_collect_dump_info(pci_priv, true);
}

static int cnss_qca6290_ramdump(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_ramdump_info_v2 *info_v2 = &plat_priv->ramdump_info_v2;
	struct cnss_dump_data *dump_data = &info_v2->dump_data;
	struct cnss_dump_seg *dump_seg = info_v2->dump_data_vaddr;
	struct ramdump_segment *ramdump_segs, *s;
	int i, ret = 0;

	if (!info_v2->dump_data_valid ||
	    dump_data->nentries == 0)
		return 0;

	ramdump_segs = kcalloc(dump_data->nentries,
			       sizeof(*ramdump_segs),
			       GFP_KERNEL);
	if (!ramdump_segs)
		return -ENOMEM;

	s = ramdump_segs;
	for (i = 0; i < dump_data->nentries; i++) {
		s->address = dump_seg->address;
		s->v_address = dump_seg->v_address;
		s->size = dump_seg->size;
		s++;
		dump_seg++;
	}

	ret = do_elf_ramdump(info_v2->ramdump_dev, ramdump_segs,
			     dump_data->nentries);
	kfree(ramdump_segs);

	cnss_pci_set_mhi_state(plat_priv->bus_priv, CNSS_MHI_DEINIT);
	cnss_pci_clear_dump_info(plat_priv->bus_priv);

	return ret;
}

int cnss_pci_dev_powerup(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	switch (pci_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_powerup(pci_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCN7605_DEVICE_ID:
		ret = cnss_qca6290_powerup(pci_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%x\n",
			    pci_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

int cnss_pci_dev_shutdown(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	switch (pci_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_shutdown(pci_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCN7605_DEVICE_ID:
		ret = cnss_qca6290_shutdown(pci_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%x\n",
			    pci_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

int cnss_pci_dev_crash_shutdown(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	switch (pci_priv->device_id) {
	case QCA6174_DEVICE_ID:
		cnss_qca6174_crash_shutdown(pci_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
		cnss_qca6290_crash_shutdown(pci_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%x\n",
			    pci_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

int cnss_pci_dev_ramdump(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	switch (pci_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_ramdump(pci_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCN7605_DEVICE_ID:
		ret = cnss_qca6290_ramdump(pci_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%x\n",
			    pci_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

int cnss_pci_is_drv_connected(struct device *dev)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));

	if (!pci_priv)
		return -ENODEV;

	return pci_priv->drv_connected_last;
}
EXPORT_SYMBOL(cnss_pci_is_drv_connected);

int cnss_wlan_register_driver(struct cnss_wlan_driver *driver_ops)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct cnss_pci_data *pci_priv;
	unsigned int timeout;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	pci_priv = plat_priv->bus_priv;
	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	if (pci_priv->driver_ops) {
		cnss_pr_err("Driver has already registered\n");
		return -EEXIST;
	}

	if (!test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state))
		goto register_driver;

	cnss_pr_dbg("Start to wait for calibration to complete\n");

	timeout = cnss_get_boot_timeout(&pci_priv->pci_dev->dev);
	ret = wait_for_completion_timeout(&plat_priv->cal_complete,
					  msecs_to_jiffies(timeout) << 2);
	if (!ret) {
		cnss_pr_err("Timeout waiting for calibration to complete\n");
		ret = -EAGAIN;
		goto out;
	}

register_driver:
	ret = cnss_driver_event_post(plat_priv,
				     CNSS_DRIVER_EVENT_REGISTER_DRIVER,
				     CNSS_EVENT_SYNC_UNINTERRUPTIBLE,
				     driver_ops);

out:
	return ret;
}
EXPORT_SYMBOL(cnss_wlan_register_driver);

void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver_ops)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return;
	}

	if (!test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    !test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		goto skip_wait;

	reinit_completion(&plat_priv->recovery_complete);
	ret = wait_for_completion_timeout(&plat_priv->recovery_complete,
					  RECOVERY_TIMEOUT);
	if (!ret) {
		cnss_pr_err("Timeout waiting for recovery to complete\n");
		CNSS_ASSERT(0);
	}

skip_wait:
	cnss_driver_event_post(plat_priv,
			       CNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
			       CNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
}
EXPORT_SYMBOL(cnss_wlan_unregister_driver);

int cnss_pci_register_driver_hdlr(struct cnss_pci_data *pci_priv,
				  void *data)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	set_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
	pci_priv->driver_ops = data;

	ret = cnss_pci_dev_powerup(pci_priv);
	if (ret) {
		clear_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
		pci_priv->driver_ops = NULL;
	}

	return ret;
}

int cnss_pci_unregister_driver_hdlr(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	set_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);
	cnss_pci_dev_shutdown(pci_priv);
	pci_priv->driver_ops = NULL;

	return 0;
}

static int cnss_pci_smmu_fault_handler(struct iommu_domain *domain,
				       struct device *dev, unsigned long iova,
				       int flags, void *handler_token)
{
	struct cnss_pci_data *pci_priv = handler_token;

	cnss_pr_err("SMMU fault happened with IOVA 0x%lx\n", iova);

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	cnss_force_fw_assert(&pci_priv->pci_dev->dev);

	/* IOMMU driver requires non-zero return value to print debug info. */
	return -EINVAL;
}

static int cnss_pci_init_smmu(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct device *dev;
	struct dma_iommu_mapping *mapping;
	int atomic_ctx = 1, s1_bypass = 1, fast = 1, cb_stall_disable = 1,
		no_cfre = 1, non_fatal_faults = 1;

	cnss_pr_dbg("Initializing SMMU\n");

	dev = &pci_priv->pci_dev->dev;

	mapping = arm_iommu_create_mapping(dev->bus,
					   pci_priv->smmu_iova_start,
					   pci_priv->smmu_iova_len);
	if (IS_ERR(mapping)) {
		ret = PTR_ERR(mapping);
		cnss_pr_err("Failed to create SMMU mapping, err = %d\n", ret);
		goto out;
	}

	if (pci_priv->smmu_s1_enable) {
		cnss_pr_dbg("Enabling SMMU S1 stage\n");

		ret = iommu_domain_set_attr(mapping->domain,
					    DOMAIN_ATTR_ATOMIC,
					    &atomic_ctx);
		if (ret) {
			cnss_pr_err("Failed to set SMMU atomic_ctx attribute, err = %d\n",
				    ret);
			goto release_mapping;
		}

		ret = iommu_domain_set_attr(mapping->domain,
					    DOMAIN_ATTR_FAST,
					    &fast);
		if (ret) {
			cnss_pr_err("Failed to set SMMU fast attribute, err = %d\n",
				    ret);
			goto release_mapping;
		}

		ret = iommu_domain_set_attr(mapping->domain,
					    DOMAIN_ATTR_CB_STALL_DISABLE,
					    &cb_stall_disable);
		if (ret) {
			cnss_pr_err("Failed to set SMMU cb_stall_disable attribute, err = %d\n",
				    ret);
			goto release_mapping;
		}

		ret = iommu_domain_set_attr(mapping->domain,
					    DOMAIN_ATTR_NO_CFRE,
					    &no_cfre);
		if (ret) {
			cnss_pr_err("Failed to set SMMU no_cfre attribute, err = %d\n",
				    ret);
			goto release_mapping;
		}

		ret = iommu_domain_set_attr(mapping->domain,
					    DOMAIN_ATTR_NON_FATAL_FAULTS,
					    &non_fatal_faults);
		if (ret) {
			cnss_pr_err("Failed to set SMMU non_fatal_faults attribute, err = %d\n",
				    ret);
			goto release_mapping;
		}

		iommu_set_fault_handler(mapping->domain,
					cnss_pci_smmu_fault_handler, pci_priv);
	} else {
		ret = iommu_domain_set_attr(mapping->domain,
					    DOMAIN_ATTR_S1_BYPASS,
					    &s1_bypass);
		if (ret) {
			cnss_pr_err("Failed to set SMMU s1_bypass attribute, err = %d\n",
				    ret);
			goto release_mapping;
		}
	}

	ret = arm_iommu_attach_device(dev, mapping);
	if (ret) {
		cnss_pr_err("Failed to attach SMMU device, err = %d\n", ret);
		goto release_mapping;
	}

	pci_priv->smmu_mapping = mapping;

	return ret;
release_mapping:
	arm_iommu_release_mapping(mapping);
out:
	return ret;
}

static void cnss_pci_deinit_smmu(struct cnss_pci_data *pci_priv)
{
	arm_iommu_detach_device(&pci_priv->pci_dev->dev);
	arm_iommu_release_mapping(pci_priv->smmu_mapping);

	pci_priv->smmu_mapping = NULL;
}

static void cnss_pci_event_cb(struct msm_pcie_notify *notify)
{
	unsigned long flags;
	struct pci_dev *pci_dev;
	struct cnss_pci_data *pci_priv;
	struct cnss_plat_data *plat_priv;

	if (!notify)
		return;

	pci_dev = notify->user;
	if (!pci_dev)
		return;

	pci_priv = cnss_get_pci_priv(pci_dev);
	if (!pci_priv)
		return;

	plat_priv = pci_priv->plat_priv;
	switch (notify->event) {
	case MSM_PCIE_EVENT_LINKDOWN:
		if (test_bit(ENABLE_PCI_LINK_DOWN_PANIC,
			     &plat_priv->ctrl_params.quirks))
			panic("cnss: PCI link is down!\n");

		spin_lock_irqsave(&pci_link_down_lock, flags);
		if (pci_priv->pci_link_down_ind) {
			cnss_pr_dbg("PCI link down recovery is in progress, ignore!\n");
			spin_unlock_irqrestore(&pci_link_down_lock, flags);
			return;
		}
		pci_priv->pci_link_down_ind = true;
		spin_unlock_irqrestore(&pci_link_down_lock, flags);

		cnss_fatal_err("PCI link down, schedule recovery!\n");
		if (pci_dev->device == QCA6174_DEVICE_ID)
			disable_irq(pci_dev->irq);
		cnss_schedule_recovery(&pci_dev->dev, CNSS_REASON_LINK_DOWN);
		break;
	case MSM_PCIE_EVENT_WAKEUP:
		if (cnss_pci_get_monitor_wake_intr(pci_priv) &&
		    cnss_pci_get_auto_suspended(pci_priv)) {
			cnss_pci_set_monitor_wake_intr(pci_priv, false);
			pm_request_resume(&pci_dev->dev);
		}
		break;
	default:
		cnss_pr_err("Received invalid PCI event: %d\n", notify->event);
	}
}

static int cnss_reg_pci_event(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct msm_pcie_register_event *pci_event;

	pci_event = &pci_priv->msm_pci_event;
	pci_event->events = MSM_PCIE_EVENT_LINKDOWN |
		MSM_PCIE_EVENT_WAKEUP;
	pci_event->user = pci_priv->pci_dev;
	pci_event->mode = MSM_PCIE_TRIGGER_CALLBACK;
	pci_event->callback = cnss_pci_event_cb;
	pci_event->options = MSM_PCIE_CONFIG_NO_RECOVERY;

	ret = msm_pcie_register_event(pci_event);
	if (ret)
		cnss_pr_err("Failed to register MSM PCI event, err = %d\n",
			    ret);

	return ret;
}

static void cnss_dereg_pci_event(struct cnss_pci_data *pci_priv)
{
	msm_pcie_deregister_event(&pci_priv->msm_pci_event);
}

static int cnss_pci_suspend(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_wlan_driver *driver_ops;

	pm_message_t state = { .event = PM_EVENT_SUSPEND };

	if (!pci_priv)
		goto out;

	driver_ops = pci_priv->driver_ops;
	if (driver_ops && driver_ops->suspend) {
		ret = driver_ops->suspend(pci_dev, state);
		if (ret) {
			cnss_pr_err("Failed to suspend host driver, err = %d\n",
				    ret);
			ret = -EAGAIN;
			goto out;
		}
	}

	if (pci_priv->pci_link_state == PCI_LINK_UP && !pci_priv->disable_pc) {
		ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_SUSPEND);
		if (ret) {
			if (driver_ops && driver_ops->resume)
				driver_ops->resume(pci_dev);
			ret = -EAGAIN;
			goto out;
		}

		pci_clear_master(pci_dev);
		cnss_set_pci_config_space(pci_priv,
					  SAVE_PCI_CONFIG_SPACE);
		pci_disable_device(pci_dev);

		ret = pci_set_power_state(pci_dev, PCI_D3hot);
		if (ret)
			cnss_pr_err("Failed to set D3Hot, err = %d\n",
				    ret);
	}

	cnss_pci_set_monitor_wake_intr(pci_priv, false);

	return 0;

out:
	return ret;
}

static int cnss_pci_resume(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		goto out;

	if (pci_priv->pci_link_down_ind)
		goto out;

	if (pci_priv->pci_link_state == PCI_LINK_UP && !pci_priv->disable_pc) {
		ret = pci_enable_device(pci_dev);
		if (ret)
			cnss_pr_err("Failed to enable PCI device, err = %d\n",
				    ret);

		if (pci_priv->saved_state)
			cnss_set_pci_config_space(pci_priv,
						  RESTORE_PCI_CONFIG_SPACE);

		pci_set_master(pci_dev);
		cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RESUME);
	}

	driver_ops = pci_priv->driver_ops;
	if (driver_ops && driver_ops->resume) {
		ret = driver_ops->resume(pci_dev);
		if (ret)
			cnss_pr_err("Failed to resume host driver, err = %d\n",
				    ret);
	}

	return 0;

out:
	return ret;
}

static int cnss_pci_suspend_noirq(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		goto out;

	driver_ops = pci_priv->driver_ops;
	if (driver_ops && driver_ops->suspend_noirq)
		ret = driver_ops->suspend_noirq(pci_dev);

	if (pci_priv->disable_pc && !pci_dev->state_saved)
		pci_save_state(pci_dev);

out:
	return ret;
}

static int cnss_pci_resume_noirq(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		goto out;

	driver_ops = pci_priv->driver_ops;
	if (driver_ops && driver_ops->resume_noirq &&
	    !pci_priv->pci_link_down_ind)
		ret = driver_ops->resume_noirq(pci_dev);

out:
	return ret;
}

static int cnss_pci_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		return -EAGAIN;

	if (pci_priv->pci_link_down_ind) {
		cnss_pr_dbg("PCI link down recovery is in progress!\n");
		return -EAGAIN;
	}

	cnss_pr_dbg("Runtime suspend start\n");

	driver_ops = pci_priv->driver_ops;
	if (driver_ops && driver_ops->runtime_ops &&
	    driver_ops->runtime_ops->runtime_suspend)
		ret = driver_ops->runtime_ops->runtime_suspend(pci_dev);
	else
		ret = cnss_auto_suspend(dev);

	cnss_pr_info("Runtime suspend status: %d\n", ret);

	return ret;
}

static int cnss_pci_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		return -EAGAIN;

	if (pci_priv->pci_link_down_ind) {
		cnss_pr_dbg("PCI link down recovery is in progress!\n");
		return -EAGAIN;
	}

	cnss_pr_dbg("Runtime resume start\n");

	driver_ops = pci_priv->driver_ops;
	if (driver_ops && driver_ops->runtime_ops &&
	    driver_ops->runtime_ops->runtime_resume)
		ret = driver_ops->runtime_ops->runtime_resume(pci_dev);
	else
		ret = cnss_auto_resume(dev);

	cnss_pr_info("Runtime resume status: %d\n", ret);

	return ret;
}

static int cnss_pci_runtime_idle(struct device *dev)
{
	cnss_pr_dbg("Runtime idle\n");

	pm_request_autosuspend(dev);

	return -EBUSY;
}

int cnss_wlan_pm_control(struct device *dev, bool vote)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	int ret = 0;

	if (!pci_priv)
		return -ENODEV;

	ret = msm_pcie_pm_control(vote ? MSM_PCIE_DISABLE_PC :
				  MSM_PCIE_ENABLE_PC,
				  pci_dev->bus->number, pci_dev,
				  NULL, PM_OPTIONS_DEFAULT);
	if (ret)
		return ret;

	pci_priv->disable_pc = vote;
	cnss_pr_dbg("%s PCIe power collapse\n", vote ? "disable" : "enable");

	return 0;
}
EXPORT_SYMBOL(cnss_wlan_pm_control);

void cnss_pci_pm_runtime_show_usage_count(struct cnss_pci_data *pci_priv)
{
	struct device *dev;

	if (!pci_priv)
		return;

	dev = &pci_priv->pci_dev->dev;

	cnss_pr_dbg("Runtime PM usage count: %d\n",
		    atomic_read(&dev->power.usage_count));
}

int cnss_pci_pm_runtime_get(struct cnss_pci_data *pci_priv)
{
	if (!pci_priv)
		return -ENODEV;

	return pm_runtime_get(&pci_priv->pci_dev->dev);
}

void cnss_pci_pm_runtime_get_noresume(struct cnss_pci_data *pci_priv)
{
	if (!pci_priv)
		return;

	return pm_runtime_get_noresume(&pci_priv->pci_dev->dev);
}

int cnss_pci_pm_runtime_put_autosuspend(struct cnss_pci_data *pci_priv)
{
	if (!pci_priv)
		return -ENODEV;

	return pm_runtime_put_autosuspend(&pci_priv->pci_dev->dev);
}

void cnss_pci_pm_runtime_put_noidle(struct cnss_pci_data *pci_priv)
{
	if (!pci_priv)
		return;

	pm_runtime_put_noidle(&pci_priv->pci_dev->dev);
}

void cnss_pci_pm_runtime_mark_last_busy(struct cnss_pci_data *pci_priv)
{
	if (!pci_priv)
		return;

	pm_runtime_mark_last_busy(&pci_priv->pci_dev->dev);
}

int cnss_auto_suspend(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct cnss_bus_bw_info *bus_bw_info;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -ENODEV;

	if (pci_priv->pci_link_state) {
		if (cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_SUSPEND)) {
			ret = -EAGAIN;
			goto out;
		}

		pci_clear_master(pci_dev);
		cnss_set_pci_config_space(pci_priv, SAVE_PCI_CONFIG_SPACE);
		pci_disable_device(pci_dev);

		ret = pci_set_power_state(pci_dev, PCI_D3hot);
		if (ret)
			cnss_pr_err("Failed to set D3Hot, err =  %d\n", ret);

		cnss_pr_dbg("Suspending PCI link\n");
		if (cnss_set_pci_link(pci_priv, PCI_LINK_DOWN)) {
			cnss_pr_err("Failed to suspend PCI link!\n");
			ret = -EAGAIN;
			goto resume_mhi;
		}

		pci_priv->pci_link_state = PCI_LINK_DOWN;
	}

	cnss_pci_set_auto_suspended(pci_priv, 1);
	cnss_pci_set_monitor_wake_intr(pci_priv, true);

	bus_bw_info = &plat_priv->bus_bw_info;
	msm_bus_scale_client_update_request(bus_bw_info->bus_client,
					    CNSS_BUS_WIDTH_NONE);

	return 0;

resume_mhi:
	if (pci_enable_device(pci_dev))
		cnss_pr_err("Failed to enable PCI device!\n");
	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RESUME);
out:
	return ret;
}
EXPORT_SYMBOL(cnss_auto_suspend);

int cnss_auto_resume(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct cnss_bus_bw_info *bus_bw_info;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -ENODEV;

	if (!pci_priv->pci_link_state) {
		cnss_pr_dbg("Resuming PCI link\n");
		if (cnss_set_pci_link(pci_priv, PCI_LINK_UP)) {
			cnss_pr_err("Failed to resume PCI link!\n");
			ret = -EAGAIN;
			goto out;
		}
		pci_priv->pci_link_state = PCI_LINK_UP;

		ret = pci_enable_device(pci_dev);
		if (ret)
			cnss_pr_err("Failed to enable PCI device, err = %d\n",
				    ret);

		cnss_set_pci_config_space(pci_priv, RESTORE_PCI_CONFIG_SPACE);
		pci_set_master(pci_dev);
		cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RESUME);
	}

	cnss_pci_set_auto_suspended(pci_priv, 0);

	bus_bw_info = &plat_priv->bus_bw_info;
	msm_bus_scale_client_update_request(bus_bw_info->bus_client,
					    bus_bw_info->current_bw_vote);
out:
	return ret;
}
EXPORT_SYMBOL(cnss_auto_resume);

int cnss_pm_request_resume(struct cnss_pci_data *pci_priv)
{
	struct pci_dev *pci_dev;

	if (!pci_priv)
		return -ENODEV;

	pci_dev = pci_priv->pci_dev;
	if (!pci_dev)
		return -ENODEV;

	return pm_request_resume(&pci_dev->dev);
}

int cnss_pci_force_wake_request(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct mhi_controller *mhi_ctrl;

	if (!pci_priv)
		return -ENODEV;

	if (pci_priv->device_id != QCA6390_DEVICE_ID)
		return 0;

	mhi_ctrl = pci_priv->mhi_ctrl;
	if (!mhi_ctrl)
		return -EINVAL;

	read_lock_bh(&mhi_ctrl->pm_lock);
	mhi_ctrl->wake_get(mhi_ctrl, true);
	read_unlock_bh(&mhi_ctrl->pm_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_pci_force_wake_request);

int cnss_pci_is_device_awake(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct mhi_controller *mhi_ctrl;

	if (!pci_priv)
		return -ENODEV;

	if (pci_priv->device_id != QCA6390_DEVICE_ID)
		return true;

	mhi_ctrl = pci_priv->mhi_ctrl;
	if (!mhi_ctrl)
		return -EINVAL;

	return mhi_ctrl->dev_state == MHI_STATE_M0 ? true : false;
}
EXPORT_SYMBOL(cnss_pci_is_device_awake);

int cnss_pci_force_wake_release(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct mhi_controller *mhi_ctrl;

	if (!pci_priv)
		return -ENODEV;

	if (pci_priv->device_id != QCA6390_DEVICE_ID)
		return 0;

	mhi_ctrl = pci_priv->mhi_ctrl;
	if (!mhi_ctrl)
		return -EINVAL;

	read_lock_bh(&mhi_ctrl->pm_lock);
	mhi_ctrl->wake_put(mhi_ctrl, false);
	read_unlock_bh(&mhi_ctrl->pm_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_pci_force_wake_release);

int cnss_pci_alloc_fw_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	int i;

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (!fw_mem[i].va && fw_mem[i].size) {
			fw_mem[i].va =
				dma_alloc_coherent(&pci_priv->pci_dev->dev,
						   fw_mem[i].size,
						   &fw_mem[i].pa, GFP_KERNEL);
			if (!fw_mem[i].va) {
				cnss_pr_err("Failed to allocate memory for FW, size: 0x%zx, type: %u\n",
					    fw_mem[i].size, fw_mem[i].type);

				return -ENOMEM;
			}
		}
	}

	return 0;
}

int cnss_pci_alloc_qdss_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *qdss_mem = plat_priv->qdss_mem;
	int i, j;

	for (i = 0; i < plat_priv->qdss_mem_seg_len; i++) {
		if (!qdss_mem[i].va && qdss_mem[i].size) {
			qdss_mem[i].va =
				dma_alloc_coherent(&pci_priv->pci_dev->dev,
						   qdss_mem[i].size,
						   &qdss_mem[i].pa,
						   GFP_KERNEL);
			if (!qdss_mem[i].va) {
				cnss_pr_err("Failed to allocate QDSS memory for FW, size: 0x%zx, type: %u, chuck-ID: %d\n",
					    qdss_mem[i].size,
					    qdss_mem[i].type, i);
				break;
			}
		}
	}

	/* Best-effort allocation for QDSS trace */
	if (i < plat_priv->qdss_mem_seg_len) {
		for (j = i; j < plat_priv->qdss_mem_seg_len; j++) {
			qdss_mem[j].type = 0;
			qdss_mem[j].size = 0;
		}
		plat_priv->qdss_mem_seg_len = i;
	}

	return 0;
}

void cnss_pci_free_qdss_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *qdss_mem = plat_priv->qdss_mem;
	int i;

	for (i = 0; i < plat_priv->qdss_mem_seg_len; i++) {
		if (qdss_mem[i].va && qdss_mem[i].size) {
			cnss_pr_dbg("Freeing memory for QDSS: pa: %pa, size: 0x%zx, type: %u\n",
				    &qdss_mem[i].pa, qdss_mem[i].size,
				    qdss_mem[i].type);
			dma_free_coherent(&pci_priv->pci_dev->dev,
					  qdss_mem[i].size, qdss_mem[i].va,
					  qdss_mem[i].pa);
			qdss_mem[i].va = NULL;
			qdss_mem[i].pa = 0;
			qdss_mem[i].size = 0;
			qdss_mem[i].type = 0;
		}
	}
	plat_priv->qdss_mem_seg_len = 0;
}

static void cnss_pci_free_fw_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	int i;

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (fw_mem[i].va && fw_mem[i].size) {
			cnss_pr_dbg("Freeing memory for FW, va: 0x%pK, pa: %pa, size: 0x%zx, type: %u\n",
				    fw_mem[i].va, &fw_mem[i].pa,
				    fw_mem[i].size, fw_mem[i].type);
			dma_free_coherent(&pci_priv->pci_dev->dev,
					  fw_mem[i].size, fw_mem[i].va,
					  fw_mem[i].pa);
			fw_mem[i].va = NULL;
			fw_mem[i].pa = 0;
			fw_mem[i].size = 0;
			fw_mem[i].type = 0;
		}
	}

	plat_priv->fw_mem_seg_len = 0;
}

int cnss_pci_load_m3(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *m3_mem = &plat_priv->m3_mem;
	char filename[CNSS_FW_PATH_MAX_LEN];
	const struct firmware *fw_entry;
	int ret = 0;

	if (!m3_mem->va && !m3_mem->size) {
		snprintf(filename, sizeof(filename),
			 "%s" DEFAULT_M3_FILE_NAME,
			 cnss_get_fw_path(plat_priv));

		ret = request_firmware(&fw_entry, filename,
				       &pci_priv->pci_dev->dev);
		if (ret) {
			cnss_pr_err("Failed to load M3 image: %s\n", filename);
			return ret;
		}

		m3_mem->va = dma_alloc_coherent(&pci_priv->pci_dev->dev,
						fw_entry->size, &m3_mem->pa,
						GFP_KERNEL);
		if (!m3_mem->va) {
			cnss_pr_err("Failed to allocate memory for M3, size: 0x%zx\n",
				    fw_entry->size);
			release_firmware(fw_entry);
			return -ENOMEM;
		}

		memcpy(m3_mem->va, fw_entry->data, fw_entry->size);
		m3_mem->size = fw_entry->size;
		release_firmware(fw_entry);
	}

	return 0;
}

static void cnss_pci_free_m3_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *m3_mem = &plat_priv->m3_mem;

	if (m3_mem->va && m3_mem->size) {
		cnss_pr_dbg("Freeing memory for M3, va: 0x%pK, pa: %pa, size: 0x%zx\n",
			    m3_mem->va, &m3_mem->pa, m3_mem->size);
		dma_free_coherent(&pci_priv->pci_dev->dev, m3_mem->size,
				  m3_mem->va, m3_mem->pa);
	}

	m3_mem->va = NULL;
	m3_mem->pa = 0;
	m3_mem->size = 0;
}

void cnss_pci_fw_boot_timeout_hdlr(struct cnss_pci_data *pci_priv)
{
	if (!pci_priv)
		return;

	cnss_fatal_err("Timeout waiting for FW ready indication\n");

	cnss_schedule_recovery(&pci_priv->pci_dev->dev,
			       CNSS_REASON_TIMEOUT);
}

struct dma_iommu_mapping *cnss_smmu_get_mapping(struct device *dev)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));

	if (!pci_priv)
		return NULL;

	return pci_priv->smmu_mapping;
}
EXPORT_SYMBOL(cnss_smmu_get_mapping);

int cnss_smmu_map(struct device *dev,
		  phys_addr_t paddr, uint32_t *iova_addr, size_t size)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));
	unsigned long iova;
	size_t len;
	int ret = 0;

	if (!pci_priv)
		return -ENODEV;

	if (!iova_addr) {
		cnss_pr_err("iova_addr is NULL, paddr %pa, size %zu\n",
			    &paddr, size);
		return -EINVAL;
	}

	len = roundup(size + paddr - rounddown(paddr, PAGE_SIZE), PAGE_SIZE);
	iova = roundup(pci_priv->smmu_iova_ipa_start, PAGE_SIZE);

	if (iova >=
	    (pci_priv->smmu_iova_ipa_start + pci_priv->smmu_iova_ipa_len)) {
		cnss_pr_err("No IOVA space to map, iova %lx, smmu_iova_ipa_start %pad, smmu_iova_ipa_len %zu\n",
			    iova,
			    &pci_priv->smmu_iova_ipa_start,
			    pci_priv->smmu_iova_ipa_len);
		return -ENOMEM;
	}

	ret = iommu_map(pci_priv->smmu_mapping->domain, iova,
			rounddown(paddr, PAGE_SIZE), len,
			IOMMU_READ | IOMMU_WRITE);
	if (ret) {
		cnss_pr_err("PA to IOVA mapping failed, ret %d\n", ret);
		return ret;
	}

	pci_priv->smmu_iova_ipa_start = iova + len;
	*iova_addr = (uint32_t)(iova + paddr - rounddown(paddr, PAGE_SIZE));

	return 0;
}
EXPORT_SYMBOL(cnss_smmu_map);

int cnss_get_soc_info(struct device *dev, struct cnss_soc_info *info)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));
	struct cnss_plat_data *plat_priv;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -ENODEV;

	info->va = pci_priv->bar;
	info->pa = pci_resource_start(pci_priv->pci_dev, PCI_BAR_NUM);

	memcpy(&info->device_version, &plat_priv->device_version,
	       sizeof(plat_priv->device_version));

	return 0;
}
EXPORT_SYMBOL(cnss_get_soc_info);

static struct cnss_msi_config msi_config = {
	.total_vectors = 32,
	.total_users = 4,
	.users = (struct cnss_msi_user[]) {
		{ .name = "MHI", .num_vectors = 3, .base_vector = 0 },
		{ .name = "CE", .num_vectors = 10, .base_vector = 3 },
		{ .name = "WAKE", .num_vectors = 1, .base_vector = 13 },
		{ .name = "DP", .num_vectors = 18, .base_vector = 14 },
	},
};

static int cnss_pci_get_msi_assignment(struct cnss_pci_data *pci_priv)
{
	pci_priv->msi_config = &msi_config;

	return 0;
}

static int cnss_pci_enable_msi(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	int num_vectors;
	struct cnss_msi_config *msi_config;
	struct msi_desc *msi_desc;

	ret = cnss_pci_get_msi_assignment(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to get MSI assignment, err = %d\n", ret);
		goto out;
	}

	msi_config = pci_priv->msi_config;
	if (!msi_config) {
		cnss_pr_err("msi_config is NULL!\n");
		ret = -EINVAL;
		goto out;
	}

	num_vectors = pci_alloc_irq_vectors(pci_dev,
					    msi_config->total_vectors,
					    msi_config->total_vectors,
					    PCI_IRQ_MSI);
	if (num_vectors != msi_config->total_vectors) {
		cnss_pr_err("Failed to get enough MSI vectors (%d), available vectors = %d",
			    msi_config->total_vectors, num_vectors);
		if (num_vectors >= 0)
			ret = -EINVAL;
		goto reset_msi_config;
	}

	msi_desc = irq_get_msi_desc(pci_dev->irq);
	if (!msi_desc) {
		cnss_pr_err("msi_desc is NULL!\n");
		ret = -EINVAL;
		goto free_msi_vector;
	}

	pci_priv->msi_ep_base_data = msi_desc->msg.data;
	cnss_pr_dbg("MSI base data is %d\n", pci_priv->msi_ep_base_data);

	return 0;

free_msi_vector:
	pci_free_irq_vectors(pci_priv->pci_dev);
reset_msi_config:
	pci_priv->msi_config = NULL;
out:
	return ret;
}

static void cnss_pci_disable_msi(struct cnss_pci_data *pci_priv)
{
	pci_free_irq_vectors(pci_priv->pci_dev);
}

int cnss_get_user_msi_assignment(struct device *dev, char *user_name,
				 int *num_vectors, u32 *user_base_data,
				 u32 *base_vector)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));
	struct cnss_msi_config *msi_config;
	int idx;

	if (!pci_priv)
		return -ENODEV;

	msi_config = pci_priv->msi_config;
	if (!msi_config) {
		cnss_pr_err("MSI is not supported.\n");
		return -EINVAL;
	}

	for (idx = 0; idx < msi_config->total_users; idx++) {
		if (strcmp(user_name, msi_config->users[idx].name) == 0) {
			*num_vectors = msi_config->users[idx].num_vectors;
			*user_base_data = msi_config->users[idx].base_vector
				+ pci_priv->msi_ep_base_data;
			*base_vector = msi_config->users[idx].base_vector;

			cnss_pr_dbg("Assign MSI to user: %s, num_vectors: %d, user_base_data: %u, base_vector: %u\n",
				    user_name, *num_vectors, *user_base_data,
				    *base_vector);

			return 0;
		}
	}

	cnss_pr_err("Failed to find MSI assignment for %s!\n", user_name);

	return -EINVAL;
}
EXPORT_SYMBOL(cnss_get_user_msi_assignment);

int cnss_get_msi_irq(struct device *dev, unsigned int vector)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	int irq_num;

	irq_num = pci_irq_vector(pci_dev, vector);
	cnss_pr_dbg("Get IRQ number %d for vector index %d\n", irq_num, vector);

	return irq_num;
}
EXPORT_SYMBOL(cnss_get_msi_irq);

void cnss_get_msi_address(struct device *dev, u32 *msi_addr_low,
			  u32 *msi_addr_high)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);

	pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_LO,
			      msi_addr_low);

	pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_HI,
			      msi_addr_high);
}
EXPORT_SYMBOL(cnss_get_msi_address);

u32 cnss_pci_get_wake_msi(struct cnss_pci_data *pci_priv)
{
	int ret, num_vectors;
	u32 user_base_data, base_vector;

	if (!pci_priv)
		return -ENODEV;

	ret = cnss_get_user_msi_assignment(&pci_priv->pci_dev->dev,
					   WAKE_MSI_NAME, &num_vectors,
					   &user_base_data, &base_vector);
	if (ret) {
		cnss_pr_err("WAKE MSI is not valid\n");
		return 0;
	}

	return user_base_data;
}

static int cnss_pci_enable_bus(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	u16 device_id;
	u32 pci_dma_mask = PCI_DMA_MASK_64_BIT;

	pci_read_config_word(pci_dev, PCI_DEVICE_ID, &device_id);
	if (device_id != pci_priv->pci_device_id->device)  {
		cnss_pr_err("PCI device ID mismatch, config ID: 0x%x, probe ID: 0x%x\n",
			    device_id, pci_priv->pci_device_id->device);
		ret = -EIO;
		goto out;
	}

	ret = pci_assign_resource(pci_dev, PCI_BAR_NUM);
	if (ret) {
		pr_err("Failed to assign PCI resource, err = %d\n", ret);
		goto out;
	}

	ret = pci_enable_device(pci_dev);
	if (ret) {
		cnss_pr_err("Failed to enable PCI device, err = %d\n", ret);
		goto out;
	}

	ret = pci_request_region(pci_dev, PCI_BAR_NUM, "cnss");
	if (ret) {
		cnss_pr_err("Failed to request PCI region, err = %d\n", ret);
		goto disable_device;
	}

	if (device_id == QCA6174_DEVICE_ID)
		pci_dma_mask = PCI_DMA_MASK_32_BIT;

	ret = pci_set_dma_mask(pci_dev, DMA_BIT_MASK(pci_dma_mask));
	if (ret) {
		cnss_pr_err("Failed to set PCI DMA mask (%d), err = %d\n",
			    ret, pci_dma_mask);
		goto release_region;
	}

	ret = pci_set_consistent_dma_mask(pci_dev, DMA_BIT_MASK(pci_dma_mask));
	if (ret) {
		cnss_pr_err("Failed to set PCI consistent DMA mask (%d), err = %d\n",
			    ret, pci_dma_mask);
		goto release_region;
	}

	pci_set_master(pci_dev);

	pci_priv->bar = pci_iomap(pci_dev, PCI_BAR_NUM, 0);
	if (!pci_priv->bar) {
		cnss_pr_err("Failed to do PCI IO map!\n");
		ret = -EIO;
		goto clear_master;
	}
	return 0;

clear_master:
	pci_clear_master(pci_dev);
release_region:
	pci_release_region(pci_dev, PCI_BAR_NUM);
disable_device:
	pci_disable_device(pci_dev);
out:
	return ret;
}

static void cnss_pci_disable_bus(struct cnss_pci_data *pci_priv)
{
	struct pci_dev *pci_dev = pci_priv->pci_dev;

	if (pci_priv->bar) {
		pci_iounmap(pci_dev, pci_priv->bar);
		pci_priv->bar = NULL;
	}

	pci_clear_master(pci_dev);
	pci_release_region(pci_dev, PCI_BAR_NUM);
	if (pci_is_enabled(pci_dev))
		pci_disable_device(pci_dev);
}

static int cnss_mhi_pm_runtime_get(struct mhi_controller *mhi_ctrl, void *priv)
{
	struct cnss_pci_data *pci_priv = priv;

	return pm_runtime_get(&pci_priv->pci_dev->dev);
}

static void cnss_mhi_pm_runtime_put_noidle(struct mhi_controller *mhi_ctrl,
					   void *priv)
{
	struct cnss_pci_data *pci_priv = priv;

	pm_runtime_put_noidle(&pci_priv->pci_dev->dev);
}

static char *cnss_mhi_state_to_str(enum cnss_mhi_state mhi_state)
{
	switch (mhi_state) {
	case CNSS_MHI_INIT:
		return "INIT";
	case CNSS_MHI_DEINIT:
		return "DEINIT";
	case CNSS_MHI_POWER_ON:
		return "POWER_ON";
	case CNSS_MHI_POWER_OFF:
		return "POWER_OFF";
	case CNSS_MHI_FORCE_POWER_OFF:
		return "FORCE_POWER_OFF";
	case CNSS_MHI_SUSPEND:
		return "SUSPEND";
	case CNSS_MHI_RESUME:
		return "RESUME";
	case CNSS_MHI_TRIGGER_RDDM:
		return "TRIGGER_RDDM";
	case CNSS_MHI_RDDM_DONE:
		return "RDDM_DONE";
	default:
		return "UNKNOWN";
	}
};

static void cnss_pci_dump_qdss_reg(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	int i, array_size = ARRAY_SIZE(qdss_csr) - 1;
	gfp_t gfp = GFP_KERNEL;
	u32 reg_offset;

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	if (!plat_priv->qdss_reg) {
		plat_priv->qdss_reg = devm_kzalloc(&pci_priv->pci_dev->dev,
						   sizeof(*plat_priv->qdss_reg)
						   * array_size, gfp);
		if (!plat_priv->qdss_reg)
			return;
	}

	for (i = 0; qdss_csr[i].name; i++) {
		reg_offset = QDSS_APB_DEC_CSR_BASE + qdss_csr[i].offset;
		if (cnss_pci_reg_read(pci_priv, reg_offset,
				      &plat_priv->qdss_reg[i]))
			return;
		cnss_pr_dbg("%s[0x%x] = 0x%x\n", qdss_csr[i].name, reg_offset,
			    plat_priv->qdss_reg[i]);
	}
}

static void cnss_pci_dump_ce_reg(struct cnss_pci_data *pci_priv,
				 enum cnss_ce_index ce)
{
	int i;
	u32 ce_base = ce * QCA6390_CE_REG_INTERVAL;
	u32 reg_offset, val;

	switch (ce) {
	case CNSS_CE_09:
	case CNSS_CE_10:
		for (i = 0; ce_src[i].name; i++) {
			reg_offset = QCA6390_CE_SRC_RING_REG_BASE +
				ce_base + ce_src[i].offset;
			if (cnss_pci_reg_read(pci_priv, reg_offset, &val))
				return;
			cnss_pr_dbg("CE_%02d_%s[0x%x] = 0x%x\n",
				    ce, ce_src[i].name, reg_offset, val);
		}

		for (i = 0; ce_dst[i].name; i++) {
			reg_offset = QCA6390_CE_DST_RING_REG_BASE +
				ce_base + ce_dst[i].offset;
			if (cnss_pci_reg_read(pci_priv, reg_offset, &val))
				return;
			cnss_pr_dbg("CE_%02d_%s[0x%x] = 0x%x\n",
				    ce, ce_dst[i].name, reg_offset, val);
		}
		break;
	case CNSS_CE_COMMON:
		for (i = 0; ce_cmn[i].name; i++) {
			reg_offset = QCA6390_CE_COMMON_REG_BASE +
				ce_cmn[i].offset;
			if (cnss_pci_reg_read(pci_priv, reg_offset, &val))
				return;
			cnss_pr_dbg("CE_COMMON_%s[0x%x] = 0x%x\n",
				    ce_cmn[i].name, reg_offset, val);
		}
		break;
	default:
		cnss_pr_err("Unsupported CE[%d] registers dump\n", ce);
	}
}

static void cnss_pci_dump_registers(struct cnss_pci_data *pci_priv)
{
	cnss_pr_dbg("Start to dump debug registers\n");

	if (cnss_pci_check_link_status(pci_priv))
		return;

	mhi_debug_reg_dump(pci_priv->mhi_ctrl);
	cnss_pci_dump_ce_reg(pci_priv, CNSS_CE_COMMON);
	cnss_pci_dump_ce_reg(pci_priv, CNSS_CE_09);
	cnss_pci_dump_ce_reg(pci_priv, CNSS_CE_10);
}

int cnss_pci_force_fw_assert_hdlr(struct cnss_pci_data *pci_priv)
{
	int ret;
	struct cnss_plat_data *plat_priv;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -ENODEV;

	cnss_pci_dump_shadow_reg(pci_priv);

	ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_TRIGGER_RDDM);
	if (ret) {
		cnss_fatal_err("Failed to trigger RDDM, err = %d\n", ret);
		cnss_pci_dump_registers(pci_priv);
		cnss_schedule_recovery(&pci_priv->pci_dev->dev,
				       CNSS_REASON_DEFAULT);
		return ret;
	}

	if (!test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state)) {
		mod_timer(&plat_priv->fw_boot_timer,
			  jiffies + msecs_to_jiffies(FW_ASSERT_TIMEOUT));
	}

	return 0;
}

void cnss_pci_collect_dump_info(struct cnss_pci_data *pci_priv, bool in_panic)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_dump_data *dump_data =
		&plat_priv->ramdump_info_v2.dump_data;
	struct cnss_dump_seg *dump_seg =
		plat_priv->ramdump_info_v2.dump_data_vaddr;
	struct image_info *fw_image, *rddm_image;
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	int ret, i;

	if (test_bit(CNSS_MHI_RDDM_DONE, &pci_priv->mhi_state)) {
		cnss_pr_dbg("RAM dump is already collected, skip\n");
		return;
	}

	if (cnss_pci_check_link_status(pci_priv))
		return;

	cnss_pci_dump_qdss_reg(pci_priv);

	ret = mhi_download_rddm_img(pci_priv->mhi_ctrl, in_panic);
	if (ret) {
		cnss_fatal_err("Failed to download RDDM image, err = %d\n",
			       ret);
		cnss_pci_dump_registers(pci_priv);
		return;
	}

	fw_image = pci_priv->mhi_ctrl->fbc_image;
	rddm_image = pci_priv->mhi_ctrl->rddm_image;
	dump_data->nentries = 0;

	cnss_pr_dbg("Collect FW image dump segment, nentries %d\n",
		    fw_image->entries);

	for (i = 0; i < fw_image->entries; i++) {
		dump_seg->address = fw_image->mhi_buf[i].dma_addr;
		dump_seg->v_address = fw_image->mhi_buf[i].buf;
		dump_seg->size = fw_image->mhi_buf[i].len;
		dump_seg->type = CNSS_FW_IMAGE;
		cnss_pr_dbg("seg-%d: address 0x%lx, v_address %pK, size 0x%lx\n",
			    i, dump_seg->address,
			    dump_seg->v_address, dump_seg->size);
		dump_seg++;
	}

	dump_data->nentries += fw_image->entries;

	cnss_pr_dbg("Collect RDDM image dump segment, nentries %d\n",
		    rddm_image->entries);

	for (i = 0; i < rddm_image->entries; i++) {
		dump_seg->address = rddm_image->mhi_buf[i].dma_addr;
		dump_seg->v_address = rddm_image->mhi_buf[i].buf;
		dump_seg->size = rddm_image->mhi_buf[i].len;
		dump_seg->type = CNSS_FW_RDDM;
		cnss_pr_dbg("seg-%d: address 0x%lx, v_address %pK, size 0x%lx\n",
			    i, dump_seg->address,
			    dump_seg->v_address, dump_seg->size);
		dump_seg++;
	}

	dump_data->nentries += rddm_image->entries;

	cnss_pr_dbg("Collect remote heap dump segment\n");

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (fw_mem[i].type == CNSS_MEM_TYPE_DDR) {
			dump_seg->address = fw_mem[i].pa;
			dump_seg->v_address = fw_mem[i].va;
			dump_seg->size = fw_mem[i].size;
			dump_seg->type = CNSS_FW_REMOTE_HEAP;
			cnss_pr_dbg("seg-%d: address 0x%lx, v_address %pK, size 0x%lx\n",
				    i, dump_seg->address, dump_seg->v_address,
				    dump_seg->size);
			dump_seg++;
			dump_data->nentries++;
		}
	}

	if (dump_data->nentries > 0)
		plat_priv->ramdump_info_v2.dump_data_valid = true;

	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RDDM_DONE);
	complete(&plat_priv->rddm_complete);
}

void cnss_pci_clear_dump_info(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	plat_priv->ramdump_info_v2.dump_data.nentries = 0;
	plat_priv->ramdump_info_v2.dump_data_valid = false;
}

static char *cnss_mhi_notify_status_to_str(enum MHI_CB status)
{
	switch (status) {
	case MHI_CB_IDLE:
		return "IDLE";
	case MHI_CB_EE_RDDM:
		return "RDDM";
	case MHI_CB_SYS_ERROR:
		return "SYS_ERROR";
	case MHI_CB_FATAL_ERROR:
		return "FATAL_ERROR";
	default:
		return "UNKNOWN";
	}
};

static void cnss_dev_rddm_timeout_hdlr(unsigned long data)
{
	struct cnss_pci_data *pci_priv = (struct cnss_pci_data *)data;

	if (!pci_priv)
		return;

	cnss_fatal_err("Timeout waiting for RDDM notification\n");

	cnss_schedule_recovery(&pci_priv->pci_dev->dev, CNSS_REASON_TIMEOUT);
}

static int cnss_mhi_link_status(struct mhi_controller *mhi_ctrl, void *priv)
{
	struct cnss_pci_data *pci_priv = priv;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -EINVAL;
	}

	return cnss_pci_check_link_status(pci_priv);
}

static void cnss_mhi_notify_status(struct mhi_controller *mhi_ctrl, void *priv,
				   enum MHI_CB reason)
{
	struct cnss_pci_data *pci_priv = priv;
	struct cnss_plat_data *plat_priv;
	enum cnss_recovery_reason cnss_reason;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL");
		return;
	}

	plat_priv = pci_priv->plat_priv;

	cnss_pr_dbg("MHI status cb is called with reason %s(%d)\n",
		    cnss_mhi_notify_status_to_str(reason), reason);

	switch (reason) {
	case MHI_CB_IDLE:
		return;
	case MHI_CB_FATAL_ERROR:
		set_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);
		del_timer(&plat_priv->fw_boot_timer);
		cnss_pci_update_status(pci_priv, CNSS_FW_DOWN);
		cnss_reason = CNSS_REASON_DEFAULT;
		break;
	case MHI_CB_SYS_ERROR:
		set_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);
		del_timer(&plat_priv->fw_boot_timer);
		mod_timer(&pci_priv->dev_rddm_timer,
			  jiffies + msecs_to_jiffies(DEV_RDDM_TIMEOUT));
		cnss_pci_update_status(pci_priv, CNSS_FW_DOWN);
		return;
	case MHI_CB_EE_RDDM:
		del_timer(&pci_priv->dev_rddm_timer);
		cnss_reason = CNSS_REASON_RDDM;
		break;
	default:
		cnss_pr_err("Unsupported MHI status cb reason: %d\n", reason);
		return;
	}

	cnss_schedule_recovery(&pci_priv->pci_dev->dev, cnss_reason);
}

static int cnss_pci_get_mhi_msi(struct cnss_pci_data *pci_priv)
{
	int ret, num_vectors, i;
	u32 user_base_data, base_vector;
	int *irq;

	ret = cnss_get_user_msi_assignment(&pci_priv->pci_dev->dev,
					   MHI_MSI_NAME, &num_vectors,
					   &user_base_data, &base_vector);
	if (ret)
		return ret;

	cnss_pr_dbg("Number of assigned MSI for MHI is %d, base vector is %d\n",
		    num_vectors, base_vector);

	irq = kcalloc(num_vectors, sizeof(int), GFP_KERNEL);
	if (!irq)
		return -ENOMEM;

	for (i = 0; i < num_vectors; i++)
		irq[i] = cnss_get_msi_irq(&pci_priv->pci_dev->dev,
					  base_vector + i);

	pci_priv->mhi_ctrl->irq = irq;
	pci_priv->mhi_ctrl->msi_allocated = num_vectors;

	return 0;
}

static void cnss_pci_update_fw_name(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct mhi_controller *mhi_ctrl = pci_priv->mhi_ctrl;

	plat_priv->device_version.family_number = mhi_ctrl->family_number;
	plat_priv->device_version.device_number = mhi_ctrl->device_number;
	plat_priv->device_version.major_version = mhi_ctrl->major_version;
	plat_priv->device_version.minor_version = mhi_ctrl->minor_version;

	cnss_pr_dbg("Get device version info, family number: 0x%x, device number: 0x%x, major version: 0x%x, minor version: 0x%x\n",
		    plat_priv->device_version.family_number,
		    plat_priv->device_version.device_number,
		    plat_priv->device_version.major_version,
		    plat_priv->device_version.minor_version);

	if (pci_priv->device_id == QCA6390_DEVICE_ID &&
	    plat_priv->device_version.major_version >= FW_V2_NUMBER) {
		snprintf(plat_priv->firmware_name,
			 sizeof(plat_priv->firmware_name),
			 "%s" FW_V2_FILE_NAME, cnss_get_fw_path(plat_priv));
		mhi_ctrl->fw_image = plat_priv->firmware_name;
	}

	cnss_pr_dbg("Firmware name is %s\n", mhi_ctrl->fw_image);
}

static int cnss_pci_register_mhi(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	struct mhi_controller *mhi_ctrl;

	mhi_ctrl = mhi_alloc_controller(0);
	if (!mhi_ctrl) {
		cnss_pr_err("Invalid MHI controller context\n");
		return -EINVAL;
	}

	pci_priv->mhi_ctrl = mhi_ctrl;

	mhi_ctrl->priv_data = pci_priv;
	mhi_ctrl->dev = &pci_dev->dev;
	mhi_ctrl->of_node = plat_priv->dev_node;
	mhi_ctrl->dev_id = pci_priv->device_id;
	mhi_ctrl->domain = pci_domain_nr(pci_dev->bus);
	mhi_ctrl->bus = pci_dev->bus->number;
	mhi_ctrl->slot = PCI_SLOT(pci_dev->devfn);

	mhi_ctrl->fw_image = plat_priv->firmware_name;

	mhi_ctrl->regs = pci_priv->bar;
	cnss_pr_dbg("BAR starts at %pa\n",
		    &pci_resource_start(pci_priv->pci_dev, PCI_BAR_NUM));

	ret = cnss_pci_get_mhi_msi(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to get MSI for MHI\n");
		return ret;
	}

	if (pci_priv->smmu_s1_enable) {
		mhi_ctrl->iova_start = pci_priv->smmu_iova_start;
		mhi_ctrl->iova_stop = pci_priv->smmu_iova_start +
					pci_priv->smmu_iova_len;
	} else {
		/* assume all addresses are valid */
		mhi_ctrl->iova_start = 0;
		mhi_ctrl->iova_stop = (dma_addr_t)U64_MAX;
	}

	mhi_ctrl->link_status = cnss_mhi_link_status;
	mhi_ctrl->status_cb = cnss_mhi_notify_status;
	mhi_ctrl->runtime_get = cnss_mhi_pm_runtime_get;
	mhi_ctrl->runtime_put = cnss_mhi_pm_runtime_put_noidle;

	mhi_ctrl->rddm_size = pci_priv->plat_priv->ramdump_info_v2.ramdump_size;
	if (pci_priv->device_id == QCN7605_DEVICE_ID)
		mhi_ctrl->sbl_size = SZ_256K;
	else
		mhi_ctrl->sbl_size = SZ_512K;

	mhi_ctrl->seg_len = SZ_512K;
	mhi_ctrl->fbc_download = true;

	mhi_ctrl->log_buf = ipc_log_context_create(CNSS_IPC_LOG_PAGES,
						   "cnss-mhi", 0);
	if (!mhi_ctrl->log_buf)
		cnss_pr_err("Unable to create CNSS MHI IPC log context\n");

	ret = of_register_mhi_controller(mhi_ctrl);
	if (ret) {
		cnss_pr_err("Failed to register to MHI bus, err = %d\n", ret);
		return ret;
	}

	return 0;
}

static void cnss_pci_unregister_mhi(struct cnss_pci_data *pci_priv)
{
	struct mhi_controller *mhi_ctrl = pci_priv->mhi_ctrl;

	mhi_unregister_mhi_controller(mhi_ctrl);
	ipc_log_context_destroy(mhi_ctrl->log_buf);
	kfree(mhi_ctrl->irq);
}

static int cnss_pci_check_mhi_state_bit(struct cnss_pci_data *pci_priv,
					enum cnss_mhi_state mhi_state)
{
	switch (mhi_state) {
	case CNSS_MHI_INIT:
		if (!test_bit(CNSS_MHI_INIT, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_DEINIT:
	case CNSS_MHI_POWER_ON:
		if (test_bit(CNSS_MHI_INIT, &pci_priv->mhi_state) &&
		    !test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_FORCE_POWER_OFF:
		if (test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_POWER_OFF:
	case CNSS_MHI_SUSPEND:
		if (test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state) &&
		    !test_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_RESUME:
		if (test_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_TRIGGER_RDDM:
		if (test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state) &&
		    !test_bit(CNSS_MHI_TRIGGER_RDDM, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_RDDM_DONE:
		return 0;
	default:
		cnss_pr_err("Unhandled MHI state: %s(%d)\n",
			    cnss_mhi_state_to_str(mhi_state), mhi_state);
	}

	cnss_pr_err("Cannot set MHI state %s(%d) in current MHI state (0x%lx)\n",
		    cnss_mhi_state_to_str(mhi_state), mhi_state,
		    pci_priv->mhi_state);

	return -EINVAL;
}

static void cnss_pci_set_mhi_state_bit(struct cnss_pci_data *pci_priv,
				       enum cnss_mhi_state mhi_state)
{
	switch (mhi_state) {
	case CNSS_MHI_INIT:
		set_bit(CNSS_MHI_INIT, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_DEINIT:
		clear_bit(CNSS_MHI_INIT, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_POWER_ON:
		set_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_POWER_OFF:
	case CNSS_MHI_FORCE_POWER_OFF:
		clear_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state);
		clear_bit(CNSS_MHI_TRIGGER_RDDM, &pci_priv->mhi_state);
		clear_bit(CNSS_MHI_RDDM_DONE, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_SUSPEND:
		set_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_RESUME:
		clear_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_TRIGGER_RDDM:
		set_bit(CNSS_MHI_TRIGGER_RDDM, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_RDDM_DONE:
		set_bit(CNSS_MHI_RDDM_DONE, &pci_priv->mhi_state);
		break;
	default:
		cnss_pr_err("Unhandled MHI state (%d)\n", mhi_state);
	}
}

int cnss_pci_set_mhi_state(struct cnss_pci_data *pci_priv,
			   enum cnss_mhi_state mhi_state)
{
	int ret = 0;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL!\n");
		return -ENODEV;
	}

	if (pci_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (mhi_state < 0) {
		cnss_pr_err("Invalid MHI state (%d)\n", mhi_state);
		return -EINVAL;
	}

	ret = cnss_pci_check_mhi_state_bit(pci_priv, mhi_state);
	if (ret)
		goto out;

	cnss_pr_dbg("Setting MHI state: %s(%d)\n",
		    cnss_mhi_state_to_str(mhi_state), mhi_state);

	switch (mhi_state) {
	case CNSS_MHI_INIT:
		ret = mhi_prepare_for_power_up(pci_priv->mhi_ctrl);
		break;
	case CNSS_MHI_DEINIT:
		mhi_unprepare_after_power_down(pci_priv->mhi_ctrl);
		ret = 0;
		break;
	case CNSS_MHI_POWER_ON:
		ret = mhi_sync_power_up(pci_priv->mhi_ctrl);
		break;
	case CNSS_MHI_POWER_OFF:
		mhi_power_down(pci_priv->mhi_ctrl, true);
		ret = 0;
		break;
	case CNSS_MHI_FORCE_POWER_OFF:
		mhi_power_down(pci_priv->mhi_ctrl, false);
		ret = 0;
		break;
	case CNSS_MHI_SUSPEND:
		ret = mhi_pm_suspend(pci_priv->mhi_ctrl);
		break;
	case CNSS_MHI_RESUME:
		ret = mhi_pm_resume(pci_priv->mhi_ctrl);
		break;
	case CNSS_MHI_TRIGGER_RDDM:
		ret = mhi_force_rddm_mode(pci_priv->mhi_ctrl);
		break;
	case CNSS_MHI_RDDM_DONE:
		break;
	default:
		cnss_pr_err("Unhandled MHI state (%d)\n", mhi_state);
		ret = -EINVAL;
	}

	if (ret)
		goto out;

	cnss_pci_set_mhi_state_bit(pci_priv, mhi_state);

	return 0;

out:
	cnss_pr_err("Failed to set MHI state: %s(%d)\n",
		    cnss_mhi_state_to_str(mhi_state), mhi_state);
	return ret;
}

int cnss_pci_start_mhi(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL!\n");
		return -ENODEV;
	}

	plat_priv = pci_priv->plat_priv;
	if (test_bit(FBC_BYPASS, &plat_priv->ctrl_params.quirks))
		return 0;

	if (MHI_TIMEOUT_OVERWRITE_MS)
		pci_priv->mhi_ctrl->timeout_ms = MHI_TIMEOUT_OVERWRITE_MS;

	ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_INIT);
	if (ret)
		goto out;

	ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_POWER_ON);
	if (ret)
		goto out;

	return 0;

out:
	return ret;
}

void cnss_pci_stop_mhi(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL!\n");
		return;
	}

	plat_priv = pci_priv->plat_priv;
	if (test_bit(FBC_BYPASS, &plat_priv->ctrl_params.quirks))
		return;

	cnss_pci_set_mhi_state_bit(pci_priv, CNSS_MHI_RESUME);
	if (!pci_priv->pci_link_down_ind)
		cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_POWER_OFF);
	else
		cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_FORCE_POWER_OFF);

	if (plat_priv->ramdump_info_v2.dump_data_valid ||
	    test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state))
		return;

	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_DEINIT);
}

static int cnss_pci_get_dev_cfg_node(struct cnss_plat_data *plat_priv)
{
	struct device_node *child;
	u32 id, i;
	int id_n, ret;

	if (!plat_priv->is_converged_dt) {
		plat_priv->dev_node = plat_priv->plat_dev->dev.of_node;
		return 0;
	}

	if (!plat_priv->device_id) {
		cnss_pr_err("Invalid device id\n");
		return -EINVAL;
	}

	for_each_available_child_of_node(plat_priv->plat_dev->dev.of_node,
					 child) {
		if (strcmp(child->name, "chip_cfg"))
			continue;

		id_n = of_property_count_u32_elems(child, "supported-ids");
		if (id_n <= 0) {
			cnss_pr_err("Device id is NOT set\n");
			return -EINVAL;
		}

		for (i = 0; i < id_n; i++) {
			ret = of_property_read_u32_index(child,
							 "supported-ids",
							 i, &id);
			if (ret) {
				cnss_pr_err("Failed to read supported ids\n");
				return -EINVAL;
			}

			if (id == plat_priv->device_id) {
				plat_priv->dev_node = child;
				cnss_pr_dbg("got node[%s@%d] for device[0x%x]\n",
					    child->name, i, id);
				return 0;
			}
		}
	}

	return -EINVAL;
}

/* For converged dt, property 'reg' is declared in sub node,
 * won't be parsed during probe.
 */
static int cnss_pci_get_smmu_cfg(struct cnss_plat_data *plat_priv)
{
	struct resource res_tmp;
	struct cnss_pci_data *pci_priv;
	struct resource *res;
	int index;
	int ret;
	struct device_node *dev_node;

	dev_node = (plat_priv->dev_node ?
		    plat_priv->dev_node : plat_priv->plat_dev->dev.of_node);

	if (plat_priv->is_converged_dt) {
		index = of_property_match_string(dev_node, "reg-names",
						 "smmu_iova_base");
		if (index < 0) {
			ret = -ENODATA;
			goto out;
		}
		ret = of_address_to_resource(dev_node, index, &res_tmp);
		if (ret)
			goto out;

		res = &res_tmp;
	} else {
		res = platform_get_resource_byname(plat_priv->plat_dev,
						   IORESOURCE_MEM,
						   "smmu_iova_base");
		if (!res) {
			ret = -ENODATA;
			goto out;
		}
	}

	pci_priv = plat_priv->bus_priv;
	if (of_property_read_bool(dev_node, "qcom,smmu-s1-enable"))
		pci_priv->smmu_s1_enable = true;

	pci_priv->smmu_iova_start = res->start;
	pci_priv->smmu_iova_len = resource_size(res);
	cnss_pr_dbg("smmu_iova_start: %pa, smmu_iova_len: %zu\n",
		    &pci_priv->smmu_iova_start,
		    pci_priv->smmu_iova_len);

	if (plat_priv->is_converged_dt) {
		index = of_property_match_string(dev_node, "reg-names",
						 "smmu_iova_ipa");
		if (index < 0) {
			ret = -ENODATA;
			goto out;
		}

		ret = of_address_to_resource(dev_node, index, &res_tmp);
		if (ret)
			goto out;

		res = &res_tmp;
	} else {
		res = platform_get_resource_byname(plat_priv->plat_dev,
						   IORESOURCE_MEM,
						   "smmu_iova_ipa");
		if (!res) {
			ret = -ENODATA;
			goto out;
		}
	}

	pci_priv->smmu_iova_ipa_start = res->start;
	pci_priv->smmu_iova_ipa_len = resource_size(res);
	cnss_pr_dbg("%s - smmu_iova_ipa_start: %pa, smmu_iova_ipa_len: %zu\n",
		    (plat_priv->is_converged_dt ?
		    "converged dt" : "single dt"),
		    &pci_priv->smmu_iova_ipa_start,
		    pci_priv->smmu_iova_ipa_len);
	return 0;

out:
	return ret;
}

static int cnss_pci_probe(struct pci_dev *pci_dev,
			  const struct pci_device_id *id)
{
	int ret = 0;
	struct cnss_pci_data *pci_priv;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);

	cnss_pr_dbg("PCI is probing, vendor ID: 0x%x, device ID: 0x%x\n",
		    id->vendor, pci_dev->device);

	pci_priv = devm_kzalloc(&pci_dev->dev, sizeof(*pci_priv),
				GFP_KERNEL);
	if (!pci_priv) {
		ret = -ENOMEM;
		goto out;
	}

	pci_priv->pci_link_state = PCI_LINK_UP;
	pci_priv->plat_priv = plat_priv;
	pci_priv->pci_dev = pci_dev;
	pci_priv->pci_device_id = id;
	pci_priv->device_id = pci_dev->device;
	cnss_set_pci_priv(pci_dev, pci_priv);
	plat_priv->device_id = pci_dev->device;
	plat_priv->bus_priv = pci_priv;

	ret = cnss_pci_get_dev_cfg_node(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to get device cfg node, err = %d\n", ret);
		goto reset_ctx;
	}

	ret = cnss_dev_specific_power_on(plat_priv);
	if (ret)
		goto reset_ctx;

	ret = cnss_register_subsys(plat_priv);
	if (ret)
		goto reset_ctx;

	ret = cnss_register_ramdump(plat_priv);
	if (ret)
		goto unregister_subsys;

	ret = cnss_pci_get_smmu_cfg(plat_priv);
	if (!ret) {
		ret = cnss_pci_init_smmu(pci_priv);
		if (ret) {
			cnss_pr_err("Failed to init SMMU, err = %d\n", ret);
			goto unregister_ramdump;
		}
	}

	ret = cnss_reg_pci_event(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to register PCI event, err = %d\n", ret);
		goto deinit_smmu;
	}

	ret = cnss_pci_enable_bus(pci_priv);
	if (ret)
		goto dereg_pci_event;

	cnss_pci_disable_l1(pci_priv);

	pci_save_state(pci_dev);
	pci_priv->default_state = pci_store_saved_state(pci_dev);

	switch (pci_dev->device) {
	case QCA6174_DEVICE_ID:
		pci_read_config_word(pci_dev, QCA6174_REV_ID_OFFSET,
				     &pci_priv->revision_id);
		ret = cnss_suspend_pci_link(pci_priv);
		if (ret)
			cnss_pr_err("Failed to suspend PCI link, err = %d\n",
				    ret);
		cnss_power_off_device(plat_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCN7605_DEVICE_ID:
		setup_timer(&pci_priv->dev_rddm_timer,
			    cnss_dev_rddm_timeout_hdlr,
			    (unsigned long)pci_priv);

		ret = cnss_pci_enable_msi(pci_priv);
		if (ret)
			goto disable_bus;

		snprintf(plat_priv->firmware_name,
			 sizeof(plat_priv->firmware_name),
			 "%s" DEFAULT_FW_FILE_NAME,
			 cnss_get_fw_path(plat_priv));

		ret = cnss_pci_register_mhi(pci_priv);
		if (ret) {
			cnss_pci_disable_msi(pci_priv);
			goto disable_bus;
		}
		/* Update fw name according to different chip subtype */
		cnss_pci_update_fw_name(pci_priv);

		if (EMULATION_HW)
			break;
		ret = cnss_suspend_pci_link(pci_priv);
		if (ret)
			cnss_pr_err("Failed to suspend PCI link, err = %d\n",
				    ret);
		cnss_power_off_device(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown PCI device found: 0x%x\n",
			    pci_dev->device);
		ret = -ENODEV;
		goto disable_bus;
	}

	return 0;

disable_bus:
	cnss_pci_disable_bus(pci_priv);
dereg_pci_event:
	cnss_dereg_pci_event(pci_priv);
deinit_smmu:
	if (pci_priv->smmu_mapping)
		cnss_pci_deinit_smmu(pci_priv);
unregister_ramdump:
	cnss_unregister_ramdump(plat_priv);
unregister_subsys:
	cnss_unregister_subsys(plat_priv);
reset_ctx:
	plat_priv->bus_priv = NULL;
out:
	return ret;
}

static void cnss_pci_remove(struct pci_dev *pci_dev)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv =
		cnss_bus_dev_to_plat_priv(&pci_dev->dev);

	cnss_pci_free_m3_mem(pci_priv);
	cnss_pci_free_fw_mem(pci_priv);
	cnss_pci_free_qdss_mem(pci_priv);

	switch (pci_dev->device) {
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
		cnss_pci_unregister_mhi(pci_priv);
		cnss_pci_disable_msi(pci_priv);
		del_timer(&pci_priv->dev_rddm_timer);
		break;
	default:
		break;
	}

	pci_load_and_free_saved_state(pci_dev, &pci_priv->saved_state);

	cnss_pci_disable_bus(pci_priv);
	cnss_dereg_pci_event(pci_priv);
	if (pci_priv->smmu_mapping)
		cnss_pci_deinit_smmu(pci_priv);
	cnss_unregister_ramdump(plat_priv);
	cnss_unregister_subsys(plat_priv);
	plat_priv->bus_priv = NULL;
}

static const struct pci_device_id cnss_pci_id_table[] = {
	{ QCA6174_VENDOR_ID, QCA6174_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ QCA6290_VENDOR_ID, QCA6290_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ QCA6390_VENDOR_ID, QCA6390_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ QCN7605_VENDOR_ID, QCN7605_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID},
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, cnss_pci_id_table);

static const struct dev_pm_ops cnss_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cnss_pci_suspend, cnss_pci_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(cnss_pci_suspend_noirq,
				      cnss_pci_resume_noirq)
	SET_RUNTIME_PM_OPS(cnss_pci_runtime_suspend, cnss_pci_runtime_resume,
			   cnss_pci_runtime_idle)
};

struct pci_driver cnss_pci_driver = {
	.name     = "cnss_pci",
	.id_table = cnss_pci_id_table,
	.probe    = cnss_pci_probe,
	.remove   = cnss_pci_remove,
	.driver = {
		.pm = &cnss_pm_ops,
	},
};

int cnss_pci_init(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev = &plat_priv->plat_dev->dev;
	u32 rc_num;

	ret = of_property_read_u32(dev->of_node, "qcom,wlan-rc-num", &rc_num);
	if (ret) {
		cnss_pr_err("Failed to find PCIe RC number, err = %d\n", ret);
		goto out;
	}

	ret = msm_pcie_enumerate(rc_num);
	if (ret) {
		cnss_pr_err("Failed to enable PCIe RC%x, err = %d\n",
			    rc_num, ret);
		goto out;
	}

	ret = pci_register_driver(&cnss_pci_driver);
	if (ret) {
		cnss_pr_err("Failed to register to PCI framework, err = %d\n",
			    ret);
		goto out;
	}

	if (!plat_priv->bus_priv) {
		cnss_pr_err("Failed to probe pci driver\n");
		ret = -ENODEV;
		goto deinit;
	}

	return 0;

deinit:
	pci_unregister_driver(&cnss_pci_driver);
out:
	return ret;
}

void cnss_pci_deinit(struct cnss_plat_data *plat_priv)
{
	pci_unregister_driver(&cnss_pci_driver);
}
