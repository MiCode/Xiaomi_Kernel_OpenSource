/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/firmware.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <asm/dma-iommu.h>
#include <linux/msm_dma_iommu_mapping.h>

#define upd720x_firmware  "K2026090.mem"

#define UPD_PCI_F4  0xF4

#define UPD_PCI_F4_FWDOWNLOADENABLE  (0x0001)
#define UPD_PCI_F4_FWDOWNLOADLOCK    (0x0002)
#define UPD_PCI_F4_SETDATA0          (0x0100)
#define UPD_PCI_F4_SETDATA1          (0x0200)
#define UPD_PCI_F4_RESULT            (0x0070)

#define UPD_PCI_F8  0xF8

#define UPD_PCI_FC  0xFC

enum SET_DATA {
	SET_DATA_PAGE0,
	SET_DATA_PAGE1
};

#define SMMU_BASE 0x10000000
#define SMMU_SIZE 0x40000000

struct firmware *fw_pointer;
static struct dma_iommu_mapping *upd720x_smmu_init(struct device *dev)
{
	int rc = 0;
	int atomic_ctx = 1;
	int bypass_enable = 1;
	struct dma_iommu_mapping *mapping = NULL;

	mapping = arm_iommu_create_mapping(&platform_bus_type,
		SMMU_BASE, SMMU_SIZE);
	if (IS_ERR(mapping)) {
		rc = PTR_ERR(mapping);
		dev_err(dev, "create mapping failed, err = %d\n", rc);
		return NULL;
	}

	rc = iommu_domain_set_attr(mapping->domain,
		DOMAIN_ATTR_ATOMIC, &atomic_ctx);
	if (rc) {
		dev_err(dev, "Set atomic attribute to SMMU failed (%d)\n", rc);
		arm_iommu_release_mapping(mapping);
		return NULL;
	}

	rc = iommu_domain_set_attr(mapping->domain,
		DOMAIN_ATTR_S1_BYPASS, &bypass_enable);
	if (rc) {
		dev_err(dev, "Set bypass attribute to SMMU failed (%d)\n", rc);
		arm_iommu_release_mapping(mapping);
		return NULL;
	}

	rc = arm_iommu_attach_device(dev, mapping);
	if (rc) {
		dev_err(dev, "arm_iommu_attach_device failed (%d)\n", rc);
		arm_iommu_release_mapping(mapping);
		return NULL;
	}

	pr_info("attached to IOMMU\n");
	return mapping;
}

static int upd720x_download_enable(struct pci_dev *pDev)
{
	unsigned int read_data;
	int result;

	result = pci_read_config_dword(pDev, UPD_PCI_F4, &read_data);
	pr_info("Set FW Download enable\n");
	result = pci_write_config_dword(pDev, UPD_PCI_F4, read_data |
		UPD_PCI_F4_FWDOWNLOADENABLE);
	return result;
}

static int upd720x_download_lock(struct pci_dev *pDev)
{
	unsigned int read_data;
	int result;

	result = pci_read_config_dword(pDev, UPD_PCI_F4, &read_data);
	pr_info("Set FW Download lock\n");
	result = pci_write_config_dword(pDev, UPD_PCI_F4, read_data |
		UPD_PCI_F4_FWDOWNLOADLOCK);
	return result;
}

static int upd720x_set_data0(struct pci_dev *pDev)
{
	unsigned int read_data;
	int result;

	result = pci_read_config_dword(pDev, UPD_PCI_F4, &read_data);
	result = pci_write_config_dword(pDev, UPD_PCI_F4,
		(read_data & ~UPD_PCI_F4_SETDATA1) |
		UPD_PCI_F4_SETDATA0);

	return result;
}

static int upd720x_set_data1(struct pci_dev *pDev)
{
	unsigned int read_data;
	int result;

	result = pci_read_config_dword(pDev, UPD_PCI_F4, &read_data);
	result = pci_write_config_dword(pDev, UPD_PCI_F4,
		(read_data & ~UPD_PCI_F4_SETDATA0) |
		UPD_PCI_F4_SETDATA1);

	return result;
}

static int upd720x_download_clearcontrol(struct pci_dev *pDev)
{
	int read_buf;
	int rc;

	rc = pci_read_config_dword(pDev, UPD_PCI_F4, &read_buf);
	if (rc == 0) {
		rc = pci_write_config_dword(pDev, UPD_PCI_F4, read_buf &
		~UPD_PCI_F4_FWDOWNLOADENABLE);
	}
	return rc;
}
int upd720x_firmware_download(struct pci_dev  *pDev,
	unsigned char *pFWImage, unsigned int firmware_size)
{
	enum SET_DATA page = SET_DATA_PAGE0;
	int offset;
	unsigned int *image = (unsigned int *)pFWImage;
	unsigned int fw_dwordsize   = firmware_size /
		(sizeof(unsigned int) / sizeof(unsigned char));

	if ((firmware_size %
	(sizeof(unsigned int) / sizeof(unsigned char))) != 0)
		fw_dwordsize++;

	if (upd720x_download_enable(pDev) == -EFAULT) {
		pr_info("Set FW Download Enable is timeout");
		return -EFAULT;
	}

	for (offset = 0; offset < fw_dwordsize; offset++) {
		switch (page) {
		case SET_DATA_PAGE0:
			pci_write_config_dword(pDev, UPD_PCI_F8, image[offset]);

			if (upd720x_set_data0(pDev) == -EFAULT)
				return -EFAULT;
			page = SET_DATA_PAGE1;
			break;

		case SET_DATA_PAGE1:
			pci_write_config_dword(pDev, UPD_PCI_FC, image[offset]);
			if (upd720x_set_data1(pDev) == -EFAULT)
				return -EFAULT;
			page = SET_DATA_PAGE0;
			break;
		default:
			break;
		}
	}

	if (upd720x_download_clearcontrol(pDev) == -EFAULT)
		return -EFAULT;

	if (upd720x_download_lock(pDev) == -EFAULT)
		return -EFAULT;

	return 0;
}

int upd720x_finish_download(struct pci_dev *pDev)
{
	int result = -EFAULT;
	char *firmwarename = upd720x_firmware;
	int lc;
	int ret;

	if (!upd720x_smmu_init(&pDev->dev))
		return -EFAULT;

	for (lc = 0; (lc < 2) && (fw_pointer == NULL); lc++) {
		ret = request_firmware((const struct firmware **)&fw_pointer,
			firmwarename, &pDev->bus->dev);
		if (ret == 0) {
			result = upd720x_firmware_download(pDev,
				(unsigned char *)fw_pointer->data,
				fw_pointer->size);
			break;
		}
		result = ret;
	}
	return result;
}



