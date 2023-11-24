#include <linux/device.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <asm/page.h>
#include "memory_debugfs.h"
#include "dram_info.h"
#include "ufs_info.h"


struct dram_info_t *g_dram_info = NULL;
struct dram_info_t dram_info;

struct dram_info_t *init_dram_info(void)
{
	int ret = 0;
	unsigned int i, size, rank;
	struct device_node *dramc_node;

	g_dram_info = &dram_info;

	dramc_node = of_find_node_by_path("/dramc@10230000");
	if (!dramc_node) {
		dramc_node = of_find_node_by_path("/soc/dramc@10230000");
		if (!dramc_node) {
			pr_err("%s: find dram node fail\n", __func__);
			goto err;
		}
	}

	ret = of_property_read_u32(dramc_node,
		"dram_type", &(g_dram_info->ddr_type));
	if (ret) {
		pr_err("%s: get ddr_type fail\n", __func__);
		goto err;
	}

	ret = of_property_read_u32(dramc_node,
		"support_ch_cnt", &(g_dram_info->support_ch_cnt));
	if (ret) {
		pr_err("%s: get support_ch_cnt fail\n", __func__);
		goto err;
	}

	ret = of_property_read_u32(dramc_node,
		"ch_cnt", &(g_dram_info->ch_cnt));
	if (ret) {
		pr_err("%s: get ch_cnt fail\n", __func__);
		goto err;
	}

	ret = of_property_read_u32(dramc_node,
		"rk_cnt", &(g_dram_info->rk_cnt));
	if (ret) {
		pr_err("%s: get rk_cnt fail\n", __func__);
		goto err;
	}

	size = sizeof(unsigned int) * g_dram_info->rk_cnt;
	g_dram_info->rk_size = kmalloc(size, GFP_KERNEL);
	if (!(g_dram_info->rk_size))
		goto err;
	ret = of_property_read_u32_array(dramc_node,
		"rk_size", g_dram_info->rk_size, g_dram_info->rk_cnt);
	if (ret) {
		pr_info("%s: get rk_size fail\n", __func__);
		goto err;
	}

	for (rank = 0; rank < g_dram_info->rk_cnt ; rank++) {
		g_dram_info->ddr_size += (g_dram_info->rk_size[rank]*128)/1024;
		pr_err("%s: g_dram_info->rk_size[%d]=%d\n", __func__,rank, g_dram_info->rk_size[rank]);
	}

	ret = of_property_read_u32(dramc_node,
		"mr_cnt", &(g_dram_info->mr_cnt));
	if (ret) {
		pr_err("%s: get mr_cnt fail\n", __func__);
		goto err;
	}

	size = sizeof(struct mr_info_t) * g_dram_info->mr_cnt;
	g_dram_info->mr_info_ptr = kmalloc(size, GFP_KERNEL);
	ret = of_property_read_u32_array(dramc_node, "mr",
		(unsigned int *)g_dram_info->mr_info_ptr, size >> 2);
	if (ret) {
		pr_err("%s: get mr_info fail\n", __func__);
		goto err;
	}
	for (i = 0; i < g_dram_info->mr_cnt; i++)
		pr_err("%s: mr%d(%x)\n", __func__,
			g_dram_info->mr_info_ptr[i].mr_index,
			g_dram_info->mr_info_ptr[i].mr_value);

	ret = of_property_read_u32(dramc_node,
		"freq_cnt", &(g_dram_info->freq_cnt));
	if (ret) {
		pr_err("%s: get freq_cnt fail\n", __func__);
		goto err;
	}
	size = sizeof(unsigned int) * g_dram_info->freq_cnt;
	g_dram_info->freq_step = kmalloc(size, GFP_KERNEL);
	if (!(g_dram_info->freq_step))
		return NULL;
	ret = of_property_read_u32_array(dramc_node, "freq_step",
		g_dram_info->freq_step, g_dram_info->freq_cnt);
	if (ret) {
		pr_err("%s: get freq_step fail\n", __func__);
		goto err;
	}

	g_dram_info->ddr_id = (g_dram_info->mr_info_ptr[0].mr_value) & 0xff;

	switch(g_dram_info->ddr_id) {
		case HWINFO_DDRID_SAMSUNG:
			memcpy(g_dram_info->ddr_vendor, "SAMSUNG", sizeof("SAMSUNG"));
			break;
		case HWINFO_DDRID_SKHYNIX:
			memcpy(g_dram_info->ddr_vendor, "SKHYNIX", sizeof("SKHYNIX"));
			break;
		case HWINFO_DDRID_ELPIDA:
			memcpy(g_dram_info->ddr_vendor, "ELPIDA", sizeof("ELPIDA"));
			break;
		case HWINFO_DDRID_MICRON:
			memcpy(g_dram_info->ddr_vendor, "MICRON", sizeof("MICRON"));
			break;
		case HWINFO_DDRID_NANYA:
			memcpy(g_dram_info->ddr_vendor, "NANYA", sizeof("NANYA"));
			break;
		case HWINFO_DDRID_INTEL:
			memcpy(g_dram_info->ddr_vendor, "INTEL", sizeof("INTEL"));
			break;
		case HWINFO_DDRID_CXMT:
			memcpy(g_dram_info->ddr_vendor, "CXMT", sizeof("CXMT"));
			break;
		default:
			memcpy(g_dram_info->ddr_vendor, "UNKNOWN", sizeof("UNKNOWN"));
			break;
	}

	printk("%s: %s(%s),%s(%d),%s(%d),%s(%d),%s(%d),%s(%d),%s(%d),%s(%d),%s(%d)\n",
		__func__,
		"ddr_vendor", g_dram_info->ddr_vendor,
		"ddr_type", g_dram_info->ddr_type,
		"ddr_id", g_dram_info->ddr_id,
		"ddr_size", g_dram_info->ddr_size,
		"support_ch_cnt", g_dram_info->support_ch_cnt,
		"ch_cnt", g_dram_info->ch_cnt,
		"rk_cnt", g_dram_info->rk_cnt,
		"mr_cnt", g_dram_info->mr_cnt,
		"freq_cnt", g_dram_info->freq_cnt);


	return g_dram_info;
err:
	g_dram_info = NULL;
	return NULL;
}
EXPORT_SYMBOL(init_dram_info);



static ssize_t ddr_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(!g_dram_info)
		return snprintf(buf, PAGE_SIZE, "dram_info is NULL\n");
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", g_dram_info->ddr_size);
}

static DEVICE_ATTR_RO(ddr_size);

static ssize_t ddr_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(!g_dram_info)
		return snprintf(buf, PAGE_SIZE, "dram_info is NULL\n");
	else
		return snprintf(buf, PAGE_SIZE, "0x%02x\n", g_dram_info->ddr_id);
}

static DEVICE_ATTR_RO(ddr_id);

static ssize_t ddr_vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(!g_dram_info)
		return snprintf(buf, PAGE_SIZE, "dram_info is NULL\n");
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", g_dram_info->ddr_vendor);
}

static DEVICE_ATTR_RO(ddr_vendor);

static ssize_t ddr_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(!g_dram_info)
		return snprintf(buf, PAGE_SIZE, "dram_info is NULL\n");
	else
		return snprintf(buf, PAGE_SIZE, "0x%0x\n", g_dram_info->ddr_type);
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