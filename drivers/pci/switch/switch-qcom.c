// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020-2021, The Linux Foundation. All rights reserved. */

/*
 * MSM PCIe switch controller
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_pci.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/of.h>

#define DIODE_VENDOR_ID	0x12d8
#define DIODE_DEVICE_ID	0xb304

/*
 * @DIODE_ERRATA_0: Apply errata specific to the upstream port (USP).
 * @DIODE_ERRATA_1: Apply errata configuration to downstream port1 (DSP1).
 * @DIODE_ERRATA_2: Common errata applied to (DSP2) port in the switch.
 */
enum {
	DIODE_ERRATA_0,
	DIODE_ERRATA_1,
	DIODE_ERRATA_2,
	DIODE_ERRATA_MAX,
};

struct pci_qcom_switch_errata {
	int (*config_errata)(struct device *dev, void *data);
};

/*
 * Supports configuring Port Arbitration for VC0 only. Also,
 * current support is only dword updates for parb_phase_value.
 */
static int config_port_arbitration(struct device *dev, int parb_select,
				   u32 parb_phase_value)
{
	struct pci_dev *pcidev = to_pci_dev(dev);
	int i, pos, size;
	int timeout = 100;
	u32 parb_offset, parb_phase = 0, parb_size;
	u32 val;
	u16 status;

	pos = pci_find_ext_capability(pcidev, PCI_EXT_CAP_ID_VC);
	if (!pos) {
		pr_err("port %s: could not find VC capability register\n");
		return -EIO;
	}

	pci_read_config_dword(pcidev, pos + PCI_VC_RES_CAP, &val);
	parb_offset = ((val & PCI_VC_RES_CAP_ARB_OFF) >> 24) * 16;

	if (val & PCI_VC_RES_CAP_256_PHASE)
		parb_phase = 256;
	else if (val & (PCI_VC_RES_CAP_128_PHASE |
			PCI_VC_RES_CAP_128_PHASE_TB))
		parb_phase = 128;
	else if (val & PCI_VC_RES_CAP_64_PHASE)
		parb_phase = 64;
	else if (val & PCI_VC_RES_CAP_32_PHASE)
		parb_phase = 32;

	pci_read_config_dword(pcidev, pos + PCI_VC_PORT_CAP1, &val);
	/* Port Arbitration Table Entry Size (bits) */
	parb_size = 1 << ((val & PCI_VC_CAP1_ARB_SIZE) >> 10);

	/* parb size (b) * number of phases / bits to byte / byte per dword */
	size = (parb_size * parb_phase) / 8 / 4;
	if (!size)
		return -EIO;

	/* update Port Arbitration Table */
	for (i = 0; i < size; i++)
		pci_write_config_dword(pcidev, pos + parb_offset + i * 4,
				       parb_phase_value);

	/* set Load Port Arbitration Table bit to update the port arbitration */
	pci_read_config_dword(pcidev, pos + PCI_VC_RES_CTRL, &val);
	val |= PCI_VC_RES_CTRL_LOAD_TABLE;
	pci_write_config_dword(pcidev, pos + PCI_VC_RES_CTRL, val);

	/* poll Port Arbitration Status until H/W completes with the update */
	do {
		usleep_range(1000, 1005);
		pci_read_config_word(pcidev, pos + PCI_VC_RES_STATUS, &status);
	} while ((status & PCI_VC_RES_STATUS_TABLE) && --timeout);

	if (!timeout) {
		pr_err("port %s failed to load Port Arbitration Table\n",
		       dev_name(&pcidev->dev));
		return -EIO;
	}

	/* update Port Arbitration Select field to select scheme */
	pci_read_config_dword(pcidev, pos + PCI_VC_RES_CTRL, &val);
	val &= ~PCI_VC_RES_CTRL_ARB_SELECT;
	val |= parb_select << 17;
	pci_write_config_dword(pcidev, pos + PCI_VC_RES_CTRL, val);

	return 0;
}

static int config_common_port_diode(struct device *dev, void *data)
{
	struct pci_dev *pcidev = to_pci_dev(dev);
	int ret = 0;
	u32 addr, val;

	if (!pcidev) {
		pr_err("%s: port not found\n", __func__);
		return -ENODEV;
	}

	addr = 0x3c;
	ret = pci_read_config_dword(pcidev, addr, &val);
	if (ret) {
		pr_err("read fail: port %s read cfg addr:%x val:%x ret:%d\n",
		    dev_name(&pcidev->dev), addr, val, ret);
		return ret;
	}

	val |= 1 << 17;
	ret = pci_write_config_dword(pcidev, addr, val);
	if (ret) {
		pr_err("write fail: port %s read cfg addr:%x val:%x ret:%d\n",
		    dev_name(&pcidev->dev), addr, val, ret);
		return ret;
	}

	return 0;
}

static int config_upstream_port_diode(struct device *dev, void *data)
{
	struct pci_dev *pcidev = to_pci_dev(dev);
	u32 addr, val;
	int ret;

	if (!pcidev) {
		pr_err("%s: port not found\n", __func__);
		return -ENODEV;
	}

	/* write cfg offset 74h.bit[7] = 1 */
	addr = 0x74;
	ret = pci_read_config_dword(pcidev, addr, &val);
	if (ret) {
		pr_err("read fail: port %s read cfg addr:%x val:%x ret:%d\n",
			dev_name(&pcidev->dev), addr, val, ret);
		return ret;
	}

	val |= 1 << 7;
	ret = pci_write_config_dword(pcidev, addr, val);
	if (ret) {
		pr_err("write fail: port %s write cfg addr:%x val:%x ret:%d\n",
		    dev_name(&pcidev->dev), addr, val, ret);
		return ret;
	}

	ret = config_common_port_diode(dev, NULL);
	if (ret) {
		pr_err("%s: Applying common configuration failed\n", __func__);
		return ret;
	}

	/*
	 * Update Port Arbitration table
	 *
	 * Port arbitation scheme: 3: WRR 128 Phase
	 *
	 * 4-bits per phase
	 *
	 * Every 8th phase will be for Port2 while the rest is for Port1
	 * ex:
	 *	Phase[7]: Port2 Phase [6:0]: Port1
	 *	Phase[15]: Port2 Phase [14:8]: Port1
	 *	...
	 *	Phase[127]: Port2 Phase [126:120]: Port1
	 */
	ret = config_port_arbitration(dev, 3, 0x21111111);
	if (ret) {
		pr_err("%s: Failed to setup Port Arbitration\n", __func__);
		return ret;
	}

	return 0;
}

static int config_downstream_port_1_diode(struct device *dev, void *data)
{
	struct pci_dev *pcidev = to_pci_dev(dev);
	int ret;
	u32 addr, val;

	if (!pcidev) {
		pr_err("%s: port not found\n", __func__);
		return -ENODEV;
	}

	/* write cfg offset 6ch.bit[25] = 1 */
	addr = 0x6c;
	ret = pci_read_config_dword(pcidev, addr, &val);
	if (ret) {
		pr_err("read fail: port %s read cfg addr:%x val:%x ret:%d\n",
		    dev_name(&pcidev->dev), addr, val, ret);
		return ret;
	}

	val |= 1 << 25;
	ret = pci_write_config_dword(pcidev, addr, val);
	if (ret) {
		pr_err("write fail: port %s write cfg addr:%x val:%x ret:%d\n",
		    dev_name(&pcidev->dev), addr, val, ret);
		return ret;
	}

	/* write cfg offset 33ch.bit[2] = 1 */
	addr = 0x33c;
	ret = pci_read_config_dword(pcidev, addr, &val);
	if (ret) {
		pr_err("read fail: port %s read cfg addr:%x val:%x ret:%d\n",
		    dev_name(&pcidev->dev), addr, val, ret);
		return ret;
	}

	val |= 1 << 2;
	ret = pci_write_config_dword(pcidev, addr, val);
	if (ret) {
		pr_err("write fail: port %s write cfg addr:%x val:%x ret:%d\n",
		    dev_name(&pcidev->dev), addr, val, ret);
		return ret;
	}

	ret = config_common_port_diode(dev, NULL);
	if (ret) {
		pr_err("%s: Applying common configuration failed\n", __func__);
		return ret;
	}

	return 0;
}

struct pci_qcom_switch_errata diode_errata[] = {
	[DIODE_ERRATA_0] = {config_upstream_port_diode},
	[DIODE_ERRATA_1] = {config_downstream_port_1_diode},
	[DIODE_ERRATA_2] = {config_common_port_diode},
};

static struct pci_device_id switch_qcom_pci_tbl[] = {
	{
		PCI_DEVICE(DIODE_VENDOR_ID, DIODE_DEVICE_ID),
	},
	{0},
};
MODULE_DEVICE_TABLE(pci, switch_qcom_pci_tbl);

static int switch_qcom_config_errata(struct pci_dev *pdev)
{
	int errata_num = 0;
	int ret;

	ret = of_property_read_u32(pdev->dev.of_node, "errata",
				   &errata_num);
	if (ret)
		return 0;

	if (pdev->vendor == DIODE_VENDOR_ID) {
		dev_info(&pdev->dev, "Diode errata being requested: %d\n",
			 errata_num);

		if (errata_num >= DIODE_ERRATA_MAX) {
			dev_err(&pdev->dev, "Invalid errata num: %d\n",
				errata_num);
			return -EINVAL;
		}

		ret = diode_errata[errata_num].config_errata(&pdev->dev, NULL);
		if (ret)
			return ret;
	}

	return 0;
}

static void switch_qcom_pci_remove(struct pci_dev *pdev)
{
	pci_clear_master(pdev);
	pci_disable_device(pdev);
}

static int switch_qcom_pci_probe(struct pci_dev *pdev,
			const struct pci_device_id *id)
{
	int ret = 0;

	ret = switch_qcom_config_errata(pdev);
	if (ret)
		return ret;

	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable PCIe device\n");
		return ret;
	}

	pci_set_master(pdev);

	return 0;
}

static struct pci_driver switch_qcom_pci_driver = {
	.name		= "pcie-qcom-switch",
	.id_table	= switch_qcom_pci_tbl,
	.probe		= switch_qcom_pci_probe,
	.remove		= switch_qcom_pci_remove,
};

static int __init switch_qcom_pci_init(void)
{
	return pci_register_driver(&switch_qcom_pci_driver);
}
module_init(switch_qcom_pci_init);

static void __exit switch_qcom_pci_exit(void)
{
	return pci_unregister_driver(&switch_qcom_pci_driver);
}
module_exit(switch_qcom_pci_exit);
