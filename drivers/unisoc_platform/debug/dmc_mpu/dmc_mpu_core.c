/*
 *copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/vmalloc.h>
#include "dmc_mpu.h"

#define SPRD_MPU_CHN_PROP_SIZE		3
#define SPRD_MPU_ID_PROP_SIZE		3
#define SPRD_MPU_RANGE_ADDRS_SIZE	4
#define SPRD_MPU_PORT_SIZE		1
#define SPRD_MPU_CHN_CFG_CELL_SIZE	4
#define SPRD_MPU_DUMP_SIZE		(128 << 4)
#define SPRD_MPU_VIO_MAX_WORD		16
#define SPRD_WORD_SWAPPED(u)		((u) << 32 | (u) >> 32)


static ssize_t sprd_dmc_mpu_core_cfg_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sprd_dmpu_core *core = dev_get_drvdata(dev->parent);
	struct sprd_dmpu_chn_cfg *cfg = core->cfg;
	int i, cnt = 0;

	for (i = 0; i < core->mpu_num; i++) {
		if (!cfg[i].en) {
			cnt += sprintf(buf + cnt, "chn%d: closed\n", i);
			continue;
		}

		cnt += sprintf(buf + cnt, "chn%d:0x%08llX ~ 0x%08llX", i,
			       cfg[i].addr_start, cfg[i].addr_end);
		if (cfg[i].include)
			cnt += sprintf(buf + cnt, "  include");
		else
			cnt += sprintf(buf + cnt, "  exchule");

		if (cfg[i].mode == SPRD_MPU_W_MODE)
			cnt += sprintf(buf + cnt, "  W");
		else if (cfg[i].mode == SPRD_MPU_R_MODE)
			cnt += sprintf(buf + cnt, "  R");
		else
			cnt += sprintf(buf + cnt, "  RW");
		cnt += sprintf(buf + cnt,
			       "  %s userid: 0x%x mask: 0x%x\n",
			       cfg[i].id_type ? "userid" : "mpuid",
			       cfg[i].userid, cfg[i].id_mask);
	}

	cnt += sprintf(buf + cnt, "panic is %s\n",
		       core->panic ? "open" : "close");

	return cnt;
}

static ssize_t sprd_dmc_mpu_core_cfg_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct sprd_dmpu_core *core = dev_get_drvdata(dev->parent);
	struct sprd_dmpu_chn_cfg *cfg = core->cfg;
	struct sprd_dmpu_chn_cfg temp_cfg;
	struct sprd_dmpu_ops *ops = core->ops;
	u32 pub, chn, ret, panic;
	u32 shared_chn = 0;

	ret = sscanf(buf, "%d %d %llx %llx %x %x %x %x %x %x %d",
		&pub, &chn, &temp_cfg.addr_start, &temp_cfg.addr_end,
		&temp_cfg.include, &temp_cfg.mode, &temp_cfg.id_type,
		&temp_cfg.userid, &temp_cfg.id_mask, &panic,
		&shared_chn);

	if (ret != 11) {
		dev_err(dev->parent,
			"enter wrong parameter numeber\n");
		return -EINVAL;
	}

	if (pub > core->interleaved) {
		dev_err(dev->parent,
			"enter wrong pub number parameter\n");
		return -EINVAL;
	}

	if (chn >= core->chn_num) {
		dev_err(dev->parent,
			"enter wrong channel number parameter\n");
		return -EINVAL;
	}

	if (temp_cfg.addr_start > temp_cfg.addr_end) {
		dev_err(dev->parent,
			"enter wrong address parameter\n");
		return -EINVAL;
	}

	if (temp_cfg.include > 1) {
		dev_err(dev->parent,
			"enter wrong include parameter\n");
		return -EINVAL;
	}

	if (!temp_cfg.mode || temp_cfg.mode >
	    (SPRD_MPU_W_MODE|SPRD_MPU_R_MODE)) {
		dev_err(dev->parent,
			"enter wrong mode parameter\n");
		return -EINVAL;
	}

	if (temp_cfg.id_type > 1) {
		dev_err(dev->parent,
			"enter wrong id_type parameter\n");
		return -EINVAL;
	}

	if (panic > 1) {
		dev_err(dev->parent,
			"enter wrong panic parameter\n");
		return -EINVAL;
	}

	if (shared_chn > 1) {
		dev_err(dev->parent,
			"enter wrong shared_chn parameter\n");
		return -EINVAL;
	}

	core->panic = panic;
	temp_cfg.port = chn;
	temp_cfg.en = 1;

	if (shared_chn)
		chn = core->chn_num;

	cfg = cfg + chn;
	memcpy(cfg, &temp_cfg, sizeof(struct sprd_dmpu_chn_cfg));
	ops->enable(core, pub, false);
	ops->config(core, pub, chn);
	ops->enable(core, pub, true);

	return strnlen(buf, count);
}

static ssize_t sprd_dmc_mpu_core_chn_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sprd_dmpu_core *core = dev_get_drvdata(dev->parent);
	int chn, cnt = 0;

	for (chn = 0; chn < core->chn_num; chn++)
		cnt += sprintf(buf + cnt, "%d: %s\n", chn, core->channel[chn]);

	return cnt;
}

static ssize_t sprd_dmc_mpu_core_dump_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sprd_dmpu_core *core = dev_get_drvdata(dev->parent);
	struct sprd_dmpu_info *mpu_info = core->mpu_info;
	struct sprd_dmpu_violate *vio = &mpu_info->vio;
	u32 *dump_vaddr = (u32 *)mpu_info->dump_vaddr;
	int cnt = 0, i;

	if (!core->irq_count)
		return sprintf(buf,
			       "dmc mpu do not detect violated transaction\n");

	cnt += sprintf(buf + cnt,
		       "warning! dmc mpu detected violated transaction!!!\n");
	cnt += sprintf(buf + cnt, "pub%d: chn%d: %s\n", mpu_info->pub_id,
		       vio->port, core->channel[vio->port]);
	cnt += sprintf(buf + cnt, "%s: 0x%llX -mpuid: 0x%08X userid:0x%08X\n",
		       vio->wr ? "waddr" : "raddr", vio->addr, vio->id,
		       vio->userid);

	cnt += sprintf(buf + cnt, "data:\n");
	for (i = 0; i < SPRD_MPU_VIO_MAX_WORD; i++)
		cnt += sprintf(buf + cnt, "0x%08X\n", dump_vaddr[i]);

	return cnt;
}

static ssize_t sprd_dmc_mpu_core_panic_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct sprd_dmpu_core *core = dev_get_drvdata(dev->parent);

	return sprintf(buf, "panic mode %s!!!\n",
		       core->panic ? "open" : "closed");
}

static DEVICE_ATTR(config, 0644, sprd_dmc_mpu_core_cfg_show,
		   sprd_dmc_mpu_core_cfg_store);
static DEVICE_ATTR(channel, 0440, sprd_dmc_mpu_core_chn_show, NULL);
static DEVICE_ATTR(dump_scene, 0440, sprd_dmc_mpu_core_dump_show, NULL);
static DEVICE_ATTR(panic, 0440, sprd_dmc_mpu_core_panic_show, NULL);

static struct attribute *dmc_mpu_attrs[] = {
	&dev_attr_config.attr,
	&dev_attr_channel.attr,
	&dev_attr_dump_scene.attr,
	&dev_attr_panic.attr,
	NULL,
};

static struct attribute_group dmc_mpu_group = {
	.attrs = dmc_mpu_attrs,
};

struct miscdevice dmc_mpu_misc = {
	.name = "dmc_mpu",
	.parent = NULL,
	.minor = MISC_DYNAMIC_MINOR,
	.fops = NULL,
};

static void *sprd_dmc_mpu_mem_ram_vmap(struct platform_device *pdev,
				phys_addr_t start, size_t size,
				int nocached, u32 *count)
{
	struct page **pages;
	pgprot_t prot;
	phys_addr_t page_start;
	phys_addr_t addr;
	void *vaddr;
	u32 i, page_count;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);
	*count = page_count;
	if (nocached)
		prot = pgprot_noncached(PAGE_KERNEL);
	else
		prot = PAGE_KERNEL;

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		dev_err(&pdev->dev, "sprd dmc mpu malloc error\n");
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}

	vaddr = vm_map_ram(pages, page_count, -1);
	if (!vaddr)
		dev_err(&pdev->dev, "sprd dmc mpu vmap error\n");
	else
		vaddr += offset_in_page(start);

	kfree(pages);
	return vaddr;
}

static void *sprd_dmc_mpu_mem_ram_vmap_nocache(struct platform_device *pdev,
				phys_addr_t start, size_t size,
				u32 *count)
{
	return sprd_dmc_mpu_mem_ram_vmap(pdev, start, size, 1, count);
}

static irqreturn_t sprd_dmc_mpu_core_irq(int irq_num, void *dev)
{
	struct sprd_dmpu_info *mpu_info = (struct sprd_dmpu_info *) dev;
	struct sprd_dmpu_violate *vio = &mpu_info->vio;
	u32 *dump_vaddr = (u32 *)mpu_info->dump_vaddr;
	struct sprd_dmpu_core *core = mpu_info->core;
	struct sprd_dmpu_ops *ops;
	int i;

	if (!core)
		return IRQ_RETVAL(-EINVAL);

	ops = core->ops;
	ops->enable(core, mpu_info->pub_id, false);
	ops->vio_cmd(core, mpu_info->pub_id);
	dev_emerg(core->dev,
		  "warning! dmc mpu detected violated transaction!!!\n");
	dev_emerg(core->dev, "pub%d: chn%d: %s\n", mpu_info->pub_id,
		  vio->port, core->channel[vio->port]);
	dev_emerg(core->dev, "%s: 0x%08llX - mpuid: 0x%08X userid:0x%08X\n",
		  vio->wr ? "waddr" : "raddr", vio->addr, vio->id, vio->userid);
	dev_emerg(core->dev, "data:\n");
	for (i = 0; i < SPRD_MPU_VIO_MAX_WORD; i++)
		dev_emerg(core->dev, "0x%08X\n", dump_vaddr[i]);

	if (core->panic)
		BUG();

	/* mpu clear interrupt info */
	ops->clr_irq(core, mpu_info->pub_id);
	ops->enable(core, mpu_info->pub_id, true);
	core->irq_count++;

	return IRQ_HANDLED;
}

static int sprd_dmc_mpu_core_base_init(struct platform_device *pdev,
		       struct sprd_dmpu_core *core, u32 pub)
{
	const char *pub_name[2] = {"pub0_dmc_mpu", "pub1_dmc_mpu"};
	struct sprd_dmpu_info *mpu_info = core->mpu_info;
	int ret;
	u32 cnt;

	mpu_info[pub].pub_id = pub;
	mpu_info[pub].pub_irq = platform_get_irq(pdev, pub);
	if (mpu_info[pub].pub_irq < 0) {
		dev_err(&pdev->dev,
			"can't get the pub%d mpu irq number\n", pub);
		return -ENXIO;
	}

	mpu_info[pub].dump_paddr = core->ddr_addr_offset;


	mpu_info[pub].dump_vaddr = sprd_dmc_mpu_mem_ram_vmap_nocache(pdev,
				mpu_info[pub].dump_paddr, SPRD_MPU_DUMP_SIZE, &cnt);

	if (!mpu_info[pub].dump_vaddr) {
		dev_err(&pdev->dev,
			"pub%d can't dma_alloc_coherent\n", pub);
		return -ENOMEM;
	}

	ret = devm_request_threaded_irq(&pdev->dev,
					mpu_info[pub].pub_irq,
					sprd_dmc_mpu_core_irq, NULL,
					IRQF_TRIGGER_NONE, pub_name[pub],
					&mpu_info[pub]);
	if (ret) {
		dev_err(&pdev->dev,
			"can't request %s irq\n", pub_name[pub]);
		return ret;
	}

	return 0;
}

static int sprd_dmc_mpu_core_get_cfg(struct sprd_dmpu_core *core, u32 index)
{
	int i, sz, cells_count[SPRD_MPU_CHN_CFG_CELL_SIZE] = {
		SPRD_MPU_CHN_PROP_SIZE,
		SPRD_MPU_ID_PROP_SIZE,
		SPRD_MPU_RANGE_ADDRS_SIZE,
		SPRD_MPU_PORT_SIZE};
	const char *channel_attrs[SPRD_MPU_CHN_CFG_CELL_SIZE] = {
		"sprd,chn-config", "sprd,id-config",
		"sprd,ranges", "sprd,port-map"};
	struct sprd_dmpu_chn_cfg *cfg = core->cfg;
	u32 *val_p = (u32 *)(&cfg[index]);
	struct property *prop;
	const __be32 *val;

	for (i = 0; i < SPRD_MPU_CHN_CFG_CELL_SIZE; i++) {
		prop = of_find_property(core->dev->of_node,
					channel_attrs[i], NULL);
		if (!prop) {
			dev_err(core->dev, "skip %s property\n",
				channel_attrs[i]);
			continue;
		}
		val = (const __be32 *)prop->value + index * cells_count[i];
		sz = cells_count[i];
		while (sz--)
			*val_p++ = be32_to_cpup(val++);
	}

	cfg[index].addr_start = SPRD_WORD_SWAPPED(cfg[index].addr_start);
	cfg[index].addr_end = SPRD_WORD_SWAPPED(cfg[index].addr_end);

	return 0;
}

static int sprd_dmc_mpu_core_init_cfg(struct sprd_dmpu_core *core)
{
	int ret, i;
	u32 size;

	size = sizeof(struct sprd_dmpu_chn_cfg) * core->mpu_num;
	core->cfg = devm_kzalloc(core->dev, size, GFP_KERNEL);
	if (!core->cfg)
		return -ENOMEM;

	for (i = 0; i < core->mpu_num; i++) {
		ret = sprd_dmc_mpu_core_get_cfg(core, i);
		if (ret)
			return ret;
	}

	return 0;
}

static int sprd_dmc_mpu_core_get_config(struct sprd_dmpu_core *core)
{
	struct device_node *np = core->dev->of_node;
	u32 size;
	int ret;

	ret = of_property_read_u32(np, "sprd,channel-num",
				   &core->chn_num);
	if (ret) {
		dev_err(core->dev, "get sprd,channel-names count fail\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sprd,mpu-num",
				   &core->mpu_num);
	if (ret) {
		dev_err(core->dev, "get sprd,mpu-num fail\n");
		return ret;
	}

	size = sizeof(*(core->channel)) * core->mpu_num;
	core->channel = (const char **)devm_kzalloc(core->dev, size,
						    GFP_KERNEL);
	if (!core->channel)
		return -ENOMEM;

	ret = of_property_read_string_array(np,
					    "sprd,channel-names",
					    core->channel,
					    core->mpu_num);
	if (ret != core->mpu_num) {
		dev_err(core->dev, "get channel-names from dt failed\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sprd,ddr-offset",
				   &core->ddr_addr_offset);
	if (ret) {
		dev_err(core->dev,
			"get sprd,ddr_offset value from dt failed\n");
		return ret;
	}

	return 0;
}

static int sprd_dmc_mpu_core_monitor_cfg(struct sprd_dmpu_core *core, int pub)
{
	struct sprd_dmpu_ops *ops = core->ops;
	int i;

	for (i = 0; i < core->mpu_num; i++)
		ops->config(core, pub, i);

	ops->enable(core, pub, true);

	return 0;
}

static int sprd_dmc_mpu_core_init(struct platform_device *pdev,
				  struct sprd_dmpu_core *core)
{
	struct sprd_dmpu_info *mpu_info;
	bool interleaved;
	int i, ret;

	interleaved = of_property_read_bool(pdev->dev.of_node,
					    "sprd,ddr-interleaved");
	core->dev = &pdev->dev;
	core->interleaved = interleaved;
	core->panic = of_property_read_bool(pdev->dev.of_node,
					    "sprd,panic");
	ret = sprd_dmc_mpu_core_get_config(core);
	if (ret) {
		dev_err(&pdev->dev,
			"dmc mpu init failed ret = %d\n", ret);
		return ret;
	}

	ret = sprd_dmc_mpu_core_init_cfg(core);
	if (ret) {
		dev_err(&pdev->dev,
			"dmc mpu init channel_cfg failed ret = %d\n", ret);
		return ret;
	}

	core->mpu_info =
		devm_kzalloc(&pdev->dev, sizeof(*mpu_info) << interleaved,
			     GFP_KERNEL);
	if (!core->mpu_info)
		return -ENOMEM;
	mpu_info = core->mpu_info;
	mpu_info->core = core;

	for (i = 0; i <= interleaved; i++) {
		ret = sprd_dmc_mpu_core_base_init(pdev, core, i);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"dmc mpu init core failed ret = %d\n", ret);
			return ret;
		}
	}

	return 0;
}

int sprd_dmc_mpu_register(struct platform_device *pdev,
			  struct sprd_dmpu_core *core,
			  struct sprd_dmpu_ops *ops)
{
	int i, ret;

	ret = sprd_dmc_mpu_core_init(pdev, core);
	if (ret)
		return ret;

	core->ops = ops;
	dmc_mpu_misc.parent = core->dev;
	ret = misc_register(&dmc_mpu_misc);
	if (ret)
		return ret;

	ret = sysfs_create_group(&dmc_mpu_misc.this_device->kobj,
				 &dmc_mpu_group);
	if (ret) {
		misc_deregister(&dmc_mpu_misc);
		return ret;
	}

	for (i = 0; i <= core->interleaved; i++) {
		ops->dump_cfg(core, i);
		ret = sprd_dmc_mpu_core_monitor_cfg(core, i);
		ops->irq_enable(core, i);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_dmc_mpu_register);

void sprd_dmc_mpu_unregister(struct sprd_dmpu_core *core)
{
	struct sprd_dmpu_ops *ops = core->ops;
	int i;

	for (i = 0; i <= core->interleaved; i++) {
		ops->irq_disable(core, i);
		ops->enable(core, i, false);
	}

	sysfs_remove_group(&dmc_mpu_misc.this_device->kobj,
		&dmc_mpu_group);
	misc_deregister(&dmc_mpu_misc);
}
EXPORT_SYMBOL_GPL(sprd_dmc_mpu_unregister);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bobs Wang <bobs.wang@unisoc.com>");
MODULE_DESCRIPTION("Unisoc platform dmc mpu driver");
