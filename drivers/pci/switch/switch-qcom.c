/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

/*
 * MSM PCIe switch controller
 */

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
	SWITCH_MAX,
};

struct pci_qcom_switch_errata {
	int (*config_errata)(struct device *dev, void *data);
};

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

struct pci_qcom_switch_errata errata[] = {
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

static int switch_qcom_pci_probe(struct pci_dev *pdev,
			const struct pci_device_id *id)
{
	int ret = 0, errata_num = 0;

	ret = of_property_read_u32((&pdev->dev)->of_node, "errata",
							&errata_num);
	if (ret) {
		pr_info("No erratas needed\n");
		return 0;
	}

	pr_info("Errata being requested: %d\n", errata_num);

	if (errata_num >= SWITCH_MAX) {
		pr_err("Invalid errata num:%d\n", errata_num);
		return -EINVAL;
	}

	ret = errata[errata_num].config_errata(&pdev->dev, NULL);
	if (ret) {
		pr_err("Error applying errata\n");
		return ret;
	}

	return 0;
}

static struct pci_driver switch_qcom_pci_driver = {
	.name		= "pcie-qcom-switch",
	.id_table	= switch_qcom_pci_tbl,
	.probe		= switch_qcom_pci_probe,
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
