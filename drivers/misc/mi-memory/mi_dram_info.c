
#include <linux/string.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/types.h>
#include <asm/page.h>
#include "mi_memory_sysfs.h"
#include <linux/of.h>

struct DDR_INFO
{
	u8 ddr_size;
	uint8_t ddr_id;
	int ddr_type;
};

static struct DDR_INFO *ddr_info = NULL;
static bool DDR_INFIO_FROM_FDT = true;

/*obtain ddr size*/
static u8 memblock_mem_size_in_gb(void)
{
	u8 ddr_size_in_GB = 0;
	u32 ddr_size_in_MB = 0;
	u32 tmp = 0;
	struct sysinfo i;

	si_meminfo(&i);	

	ddr_size_in_MB = (u32)((i.totalram << (PAGE_SHIFT - 10)) / 1024);
	pr_info("memblock_mem_size %d\n", ddr_size_in_MB);

	if (ddr_size_in_MB <= 4096)
		tmp = 1024;
	else if(ddr_size_in_MB > 4096)
		tmp = 2048;

	ddr_size_in_GB = ((ddr_size_in_MB + tmp - 1) & (~(tmp - 1))) / 1024;

	return ddr_size_in_GB;
}

static uint8_t get_ddr_id_from_ufs(void)
{
	uint8_t ret = 0;
	u16 ufsid = get_ufs_id();
	switch (ufsid) {
		case UFS_VENDOR_SAMSUNG:
			ret = HWINFO_DDRID_SAMSUNG;
			break;
		case UFS_VENDOR_SKHYNIX:
			ret = HWINFO_DDRID_SKHYNIX;
			break;
		case UFS_VENDOR_MICRON:
			ret = HWINFO_DDRID_MICRON;
			break;
		default:
			ret = 0;
			pr_err("unkmow ufsid for ddrid\n");
			break;
	}

	return ret;
}

static void of_get_ddr_info(struct DDR_INFO *ddr_info)
{
	u32 ddr_type = 0;
	u32 ddr_size = 0;
	u32 ddr_manufacturer_id = 0;
	struct device_node *mem_node;

	mem_node = of_find_node_by_path("/memory");
	if (!mem_node)
		return;

	of_property_read_u32(mem_node, "ddr_device_type", &ddr_type);
	of_property_read_u32(mem_node, "ddr_size", &ddr_size);
	of_property_read_u32(mem_node, "ddr_manufacturer_id", &ddr_manufacturer_id);
	of_node_put(mem_node);

	ddr_info->ddr_type = ddr_type;
	ddr_info->ddr_size = ddr_size;
	ddr_info->ddr_id = ddr_manufacturer_id;
}

static void init_ddr_info(void)
{
	ddr_info = kzalloc(sizeof(struct DDR_INFO), GFP_KERNEL);

	if (DDR_INFIO_FROM_FDT) {
		of_get_ddr_info(ddr_info);
	} else {
		ddr_info->ddr_id = get_ddr_id_from_ufs();
		ddr_info->ddr_size = memblock_mem_size_in_gb();	
	}
}

u8 get_ddr_size(void)
{
	if(!ddr_info)
		init_ddr_info();

	return ddr_info->ddr_size;
}

uint8_t get_ddr_id(void)
{
	if(!ddr_info)
		init_ddr_info();

	return ddr_info->ddr_id;
}

int get_ddr_type(void)
{
	if(!ddr_info)
		init_ddr_info();

	return ddr_info->ddr_type;
}

static void get_ddr_vendor(char* vendor)
{
	uint8_t ddr_id = 0;

	ddr_id = get_ddr_id();

	switch(ddr_id) {
		case HWINFO_DDRID_SAMSUNG:
			memcpy(vendor, "SAMSUNG", sizeof("SAMSUNG"));
			break;
		case HWINFO_DDRID_SKHYNIX:
			memcpy(vendor, "SKHYNIX", sizeof("SKHYNIX"));
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
		case HWINFO_DDRID_CXMT:
			memcpy(vendor, "CXMT", sizeof("CXMT"));
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

	get_ddr_vendor((char*)&vendor);

	return snprintf(buf, PAGE_SIZE, "%s\n", vendor);
}

static DEVICE_ATTR_RO(ddr_vendor);

static ssize_t ddr_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%0x\n", get_ddr_type());
}

static DEVICE_ATTR_RO(ddr_type);

static struct attribute *dram_sysfs[] = {
	&dev_attr_ddr_size.attr,
	&dev_attr_ddr_id.attr,
	&dev_attr_ddr_vendor.attr,
	&dev_attr_ddr_type.attr,
	NULL,
};

const struct attribute_group dram_sysfs_group = {
	.name = "ddr",
	.attrs = dram_sysfs,
};