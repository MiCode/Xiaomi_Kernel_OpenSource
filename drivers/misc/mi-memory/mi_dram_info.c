
#include <soc/qcom/socinfo.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/soc/qcom/smem.h>
//#include <linux/hwid.h>
#include "mi_memory_sysfs.h"
#include "mem_interface.h"

#if MEMORYTPYE_DIS
#define SMEM_ID_VENDOR1 	135
#define SMEM_ID_VENDOR2 	136
#endif

u8 get_ddr_size(void)
{
	double ddr_size_in_GB = 0;

	ddr_size_in_GB = memblock_mem_size_in_gb();
	pr_err("memblock_mem_size %f\n", ddr_size_in_GB);

	if (ddr_size_in_GB > 16 && ddr_size_in_GB <= 18) {
		ddr_size_in_GB = 18;
	} else if (ddr_size_in_GB > 12) {
		ddr_size_in_GB = 16;
	} else if (ddr_size_in_GB > 10) {
		ddr_size_in_GB = 12;
	} else if (ddr_size_in_GB > 8) {
		ddr_size_in_GB = 10;
	} else if (ddr_size_in_GB > 6) {
		ddr_size_in_GB = 8;
	} else if (ddr_size_in_GB > 4) {
		ddr_size_in_GB = 6;
	} else if (ddr_size_in_GB > 3) {
		ddr_size_in_GB = 4;
	} else if (ddr_size_in_GB > 2) {
		ddr_size_in_GB = 3;
	} else if (ddr_size_in_GB > 1) {
		ddr_size_in_GB = 2;
	} else {
		ddr_size_in_GB = 0;
	}

	return (u8)ddr_size_in_GB;
}

uint8_t get_ddr_id(void)
{
	uint8_t ret = 0;
#if MEMORYTPYE_DIS
	size_t size = 0;
	uint8_t *ddr_info = NULL;

	ddr_info = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_ID_VENDOR2, &size);
	pr_err("ddr id is 0x%lx\n", *ddr_info);
	if (IS_ERR(ddr_info)) {
		pr_err("Error fetching DDR manufacturer id from SMEM!\n");
		return ret;
	}
	ret = *ddr_info;
#else
	u16 ufsid = get_ufs_id();
	switch (ufsid) {
	case UFS_VENDOR_SAMSUNG:
		ret = HWINFO_DDRID_SAMSUNG;
		break;
	case UFS_VENDOR_HYNIX:
		ret = HWINFO_DDRID_HYNIX;
		break;
	case UFS_VENDOR_MICRON:
		ret = HWINFO_DDRID_MICRON;
		break;
	default:
		ret = 0;
		pr_err("unkmow ufsid for ddrid\n");
		break;
	}
#endif
	return ret;
}
void get_ddr_vendor(char *vendor)
{
	uint8_t ddr_id = 0;

	ddr_id = get_ddr_id();

	switch (ddr_id) {
	case HWINFO_DDRID_SAMSUNG:
		memcpy(vendor, "SAMSUNG", sizeof("SAMSUNG"));
		break;
	case HWINFO_DDRID_HYNIX:
		memcpy(vendor, "HYNIX", sizeof("HYNIX"));
		break;
	case HWINFO_DDRID_ELPIDA:
		memcpy(vendor, "ELPIDA", sizeof("ELPIDA"));
		break;
	case HWINFO_DDRID_MICRON:
		memcpy(vendor, "MICRON", sizeof("MICRON"));
		break;
	case HWINFO_DDRID_NANYA:
		memcpy(vendor, "NANYA", sizeof("NANYA"));
		break;
	case HWINFO_DDRID_INTEL:
		memcpy(vendor, "INTEL", sizeof("INTEL"));
		break;
	default:
		memcpy(vendor, "UNKNOWN", sizeof("UNKNOWN"));
		break;
	}
}

static ssize_t ddr_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", get_ddr_size());
}

static DEVICE_ATTR_RO(ddr_size);

static ssize_t ddr_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02x\n", get_ddr_id());
}

static DEVICE_ATTR_RO(ddr_id);

static ssize_t ddr_vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char vendor[8] = {0};

	get_ddr_vendor((char *)&vendor);

	return snprintf(buf, PAGE_SIZE, "%s\n", vendor);
}

static DEVICE_ATTR_RO(ddr_vendor);

static struct attribute *dram_sysfs[] = {
	&dev_attr_ddr_size.attr,
	&dev_attr_ddr_id.attr,
	&dev_attr_ddr_vendor.attr,
	NULL,
};

const struct attribute_group dram_sysfs_group = {
	.name = "ddr",
	.attrs = dram_sysfs,
};
