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
	int ddr_type;
};

static struct DDR_INFO *ddr_info = NULL;
static bool DDR_INFIO_FROM_FDT = true;

static void of_get_ddr_info(struct DDR_INFO *ddr_info)
{
	u32 ddr_type = 0;
	struct device_node *mem_node;

	mem_node = of_find_node_by_path("/memory");
	if (!mem_node)
		return;

	of_property_read_u32(mem_node, "ddr_device_type", &ddr_type);
	of_node_put(mem_node);

	ddr_info->ddr_type = ddr_type;
}

static void init_ddr_info(void)
{
	ddr_info = kzalloc(sizeof(struct DDR_INFO), GFP_KERNEL);

	if (DDR_INFIO_FROM_FDT)
		of_get_ddr_info(ddr_info);
}

int get_ddr_type(void)
{
	if(!ddr_info)
		init_ddr_info();

	return ddr_info->ddr_type;
}

static ssize_t ddr_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%0x\n", get_ddr_type());
}

static DEVICE_ATTR_RO(ddr_type);

static struct attribute *dram_sysfs[] = {
	&dev_attr_ddr_type.attr,
	NULL,
};

const struct attribute_group dram_sysfs_group = {
	.name = "ddr",
	.attrs = dram_sysfs,
};