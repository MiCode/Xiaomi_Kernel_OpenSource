// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#define SPRD_APB_BM_CFG			0
#define SPRD_APB_BM_ADDR_MON		0x0004
#define SPRD_APB_BM_ADDR_MSK		0x0008
#define SPRD_APB_BM_DATA_MON		0x000c
#define SPRD_APB_BM_DATA_MSK		0x0010
#define SPRD_APB_BM_INT_EN		0x0014
#define SPRD_APB_BM_INT_CLR		0x0018
#define SPRD_APB_BM_INT_RAW		0x001C
#define SPRD_APB_BM_INT_STS		0x0020
#define SPRD_APB_BM_ADDR_HANG		0x0024
#define SPRD_APB_BM_DATA_HANG		0x0028
#define SPRD_APB_BM_BUS_INTF_HANG	0x002c
#define SPRD_APB_BM_ADDR_MATCH		0x0030
#define SPRD_APB_BM_DATA_MATCH		0x0034
#define SPRD_APB_BM_BUS_INTF_MATCH	0x0038
#define SPRD_APB_BM_LATEST_ADDR		0x003C
#define SPRD_APB_BM_LATEST_DATA		0x0040
#define SPRD_APB_BM_LATEST_BUS_INTF	0x0044
#define SPRD_APB_BM_VERSION		0x0048

#define SPRD_APB_BM_TIMEOUT(v)		(((v) << 16) & GENMASK(31, 16))
#define SPRD_APB_BM_TIMEOUT_MASK	GENMASK(31, 16)
#define SPRD_APB_BM_GET_TIMEOUT(v)	((v) >> 16)
#define SPRD_APB_BM_HANG_REC_LAST	BIT(11)
#define SPRD_APB_BM_MATCH_REC_LAST	BIT(10)
#define SPRD_APB_BM_DIR_SEL(m)		(((m) << 8) & GENMASK(9, 8))
#define SPRD_APB_BM_DIR_MASK		GENMASK(9, 8)
#define SPRD_APB_BM_MATCH_WRITE		BIT(8)
#define SPRD_APB_BM_MATCH_READ		BIT(9)
#define SPRD_APB_BM_MATCH_RW		0x3
#define SPRD_APB_BM_TRACE_DATA_MATCH	BIT(7)
#define SPRD_APB_BM_TRACE_ADDR_MATCH	BIT(6)
#define SPRD_APB_BM_MATCH_TYPE(t)	(((t) << 4) & GENMASK(5, 4))
#define SPRD_APB_BM_MATCH_TYPE_MASK	GENMASK(5, 4)
#define SPRD_APB_BM_MATCH_TYPE_MAX	0x3
#define SPRD_APB_BM_MATCH_ADDR		BIT(5)
#define SPRD_APB_BM_MATCH_DATA		BIT(4)
#define SPRD_APB_BM_LATEST_DET		BIT(3)
#define SPRD_APB_BM_HANG_DET		BIT(2)
#define SPRD_APB_BM_MATCH_DET		BIT(1)
#define SPRD_APB_BM_EN			BIT(0)

#define SPRD_APB_BM_BUS_HANG_INT_EN	BIT(1)
#define SPRD_APB_BM_BUS_MATCH_INT_EN	BIT(0)

#define SPRD_APB_BM_BUS_HANG_INT_CLR	BIT(1)
#define SPRD_APB_BM_BUS_MATCH_INT_CLR	BIT(0)

#define SPRD_APB_BM_BUS_HANG_INT_STS	BIT(1)
#define SPRD_APB_BM_BUS_MATCH_INT_STS	BIT(0)

struct apb_bm_info {
	u32 addr;
	u32 data;
	u32 bus_info;
	u32 count;
};

struct apb_bm_configs {
	u32 addr;
	u32 addr_mask;
	u32 data;
	u32 data_mask;
	u32 busmon_cfg;
	bool hang_det;
	bool match_det;
};

struct sprd_apb_busmonitor {
	struct device *dev;
	struct miscdevice misc;
	void __iomem *base;
	struct apb_bm_configs cfg;
	struct apb_bm_info match;
	struct apb_bm_info hang;
	int irq;
	bool panic;
};

static void sprd_apb_busmon_irq_enable(struct sprd_apb_busmonitor *apb_bm)
{
	struct apb_bm_configs *cfg = &apb_bm->cfg;
	u32 val = 0;

	if (cfg->hang_det)
		val |= SPRD_APB_BM_BUS_HANG_INT_EN;
	if (cfg->match_det)
		val |= SPRD_APB_BM_BUS_MATCH_INT_EN;

	writel_relaxed(val, apb_bm->base + SPRD_APB_BM_INT_EN);
}

static void sprd_apb_busmon_save_hang_sence(struct sprd_apb_busmonitor *apb_bm)
{
	apb_bm->hang.addr = readl_relaxed(apb_bm->base + SPRD_APB_BM_ADDR_HANG);
	apb_bm->hang.data = readl_relaxed(apb_bm->base + SPRD_APB_BM_DATA_HANG);
	apb_bm->hang.bus_info = readl_relaxed(apb_bm->base +
					      SPRD_APB_BM_BUS_INTF_HANG);
}

static void sprd_apb_busmon_save_match_sence(struct sprd_apb_busmonitor *apb_bm)
{
	apb_bm->match.addr = readl_relaxed(apb_bm->base +
					   SPRD_APB_BM_ADDR_MATCH);
	apb_bm->match.data = readl_relaxed(apb_bm->base +
					   SPRD_APB_BM_DATA_MATCH);
	apb_bm->match.bus_info = readl_relaxed(apb_bm->base +
					       SPRD_APB_BM_BUS_INTF_MATCH);
}

static void sprd_apb_busmon_dump_info(struct device *dev,
				      struct apb_bm_info *info)
{
	dev_emerg(dev, "addr:0x%08x\n", info->addr);
	dev_emerg(dev, "data:0x%08x\n", info->data);
	dev_emerg(dev, "bus_info:0x%08x\n", info->bus_info);
	dev_emerg(dev, "count:%d\n", info->count);
}

static void sprd_apb_busmon_dump_scene(struct sprd_apb_busmonitor *apb_bm)
{
	if (apb_bm->hang.count)
		sprd_apb_busmon_dump_info(apb_bm->dev, &apb_bm->hang);

	if (apb_bm->match.count)
		sprd_apb_busmon_dump_info(apb_bm->dev, &apb_bm->match);
}

static irqreturn_t sprd_apb_busmon_irq(int irq_num, void *dev)
{
	struct sprd_apb_busmonitor *apb_bm = (struct sprd_apb_busmonitor *) dev;
	u32 val = readl_relaxed(apb_bm->base + SPRD_APB_BM_INT_STS);

	if (val & SPRD_APB_BM_BUS_HANG_INT_STS) {
		sprd_apb_busmon_save_hang_sence(apb_bm);
		dev_emerg(apb_bm->dev, "hang:\n");
		apb_bm->hang.count++;
	}

	if (val & SPRD_APB_BM_BUS_MATCH_INT_STS) {
		sprd_apb_busmon_save_match_sence(apb_bm);
		dev_emerg(apb_bm->dev, "match:\n");
		apb_bm->match.count++;
	}

	sprd_apb_busmon_dump_scene(apb_bm);

	if (val & SPRD_APB_BM_BUS_HANG_INT_STS)
		writel_relaxed(SPRD_APB_BM_BUS_HANG_INT_CLR,
			       apb_bm->base + SPRD_APB_BM_INT_CLR);
	if (val & SPRD_APB_BM_BUS_MATCH_INT_STS)
		writel_relaxed(SPRD_APB_BM_BUS_MATCH_INT_CLR,
			       apb_bm->base + SPRD_APB_BM_INT_CLR);

	if (apb_bm->panic)
		BUG();


	return IRQ_HANDLED;
}

static void  sprd_apb_busmon_hw_cfg(struct sprd_apb_busmonitor *apb_bm)
{
	struct apb_bm_configs *cfg = &apb_bm->cfg;

	writel_relaxed(cfg->busmon_cfg, apb_bm->base + SPRD_APB_BM_CFG);
	writel_relaxed(cfg->addr, apb_bm->base + SPRD_APB_BM_ADDR_MON);
	writel_relaxed(cfg->addr_mask, apb_bm->base + SPRD_APB_BM_ADDR_MSK);
	writel_relaxed(cfg->data, apb_bm->base + SPRD_APB_BM_DATA_MON);
	writel_relaxed(cfg->data_mask, apb_bm->base + SPRD_APB_BM_DATA_MSK);

	sprd_apb_busmon_irq_enable(apb_bm);
}

static void sprd_apb_busmon_get_hw_cfg(struct sprd_apb_busmonitor *apb_bm)
{
	struct device_node *np = apb_bm->dev->of_node;
	struct apb_bm_configs *cfg = &apb_bm->cfg;
	u32 timeout;

	cfg->busmon_cfg = SPRD_APB_BM_EN;

	if (of_property_read_bool(np, "sprd,match-addr-trigger"))
		cfg->busmon_cfg |= SPRD_APB_BM_TRACE_ADDR_MATCH;

	if (of_property_read_bool(np, "sprd,match-data-trigger"))
		cfg->busmon_cfg |= SPRD_APB_BM_TRACE_DATA_MATCH;

	if (of_property_read_bool(np, "sprd,match-addr"))
		cfg->busmon_cfg |= SPRD_APB_BM_MATCH_ADDR |
			SPRD_APB_BM_MATCH_DET;

	if (of_property_read_bool(np, "sprd,match-data"))
		cfg->busmon_cfg |= SPRD_APB_BM_MATCH_DATA |
			SPRD_APB_BM_MATCH_DET;

	if (of_property_read_u32(np, "sprd,hang-timeout", &timeout))
		cfg->busmon_cfg |= SPRD_APB_BM_TIMEOUT(timeout) |
			SPRD_APB_BM_HANG_DET;

	if (of_property_read_bool(np, "sprd,record-last-hang"))
		cfg->busmon_cfg |= SPRD_APB_BM_HANG_REC_LAST;

	if (of_property_read_bool(np, "sprd,record-last-match"))
		cfg->busmon_cfg |= SPRD_APB_BM_MATCH_REC_LAST;

	if (of_property_read_bool(np, "sprd,latest-detection"))
		cfg->busmon_cfg |= SPRD_APB_BM_LATEST_DET;

	if (of_property_read_bool(np, "sprd,write-detection"))
		cfg->busmon_cfg |= SPRD_APB_BM_MATCH_WRITE;

	if (of_property_read_bool(np, "sprd,read-detection"))
		cfg->busmon_cfg |= SPRD_APB_BM_MATCH_READ;
}

static ssize_t sprd_apb_busmon_bus_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_apb_busmonitor *apb_bm = dev_get_drvdata(dev->parent);
	int cnt = 0;

	cnt += sprintf(buf + cnt, "Addr:0x%08x\n",
		       readl_relaxed(apb_bm->base + SPRD_APB_BM_LATEST_ADDR));
	cnt += sprintf(buf + cnt, "DATA:0x%08x\n",
		       readl_relaxed(apb_bm->base + SPRD_APB_BM_LATEST_DATA));
	cnt += sprintf(buf + cnt, "INTF:0x%08x\n",
		       readl_relaxed(apb_bm->base +
				     SPRD_APB_BM_LATEST_BUS_INTF));

	return cnt;
}

static ssize_t sprd_apb_busmon_dump_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_apb_busmonitor *apb_bm = dev_get_drvdata(dev->parent);
	struct apb_bm_info *info;
	int cnt = 0;

	if (!apb_bm->hang.count && !apb_bm->match.count)
		return sprintf(buf,
			       "apb busmonitor do not detect violated transaction\n");

	if (apb_bm->hang.count) {
		info = &apb_bm->hang;
		cnt += sprintf(buf + cnt, "addr:0x%08x\n", info->addr);
		cnt += sprintf(buf + cnt, "data:0x%08x\n", info->data);
		cnt += sprintf(buf + cnt, "bus_info:0x%08x\n", info->bus_info);
		cnt += sprintf(buf + cnt, "count:0x%08x\n", info->count);
	}

	if (apb_bm->match.count) {
		info = &apb_bm->match;
		cnt += sprintf(buf + cnt, "addr:0x%08x\n", info->addr);
		cnt += sprintf(buf + cnt, "data:0x%08x\n", info->data);
		cnt += sprintf(buf + cnt, "bus_info:0x%08x\n", info->bus_info);
		cnt += sprintf(buf + cnt, "count:0x%08x\n", info->count);
	}

	return cnt;
}

static ssize_t sprd_apb_busmon_panic_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_apb_busmonitor *apb_bm = dev_get_drvdata(dev->parent);

	return sprintf(buf, "panic mode %s!!!\n",
		       apb_bm->panic ? "open" : "closed");
}

static ssize_t sprd_apb_busmon_match_cfg_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct sprd_apb_busmonitor *apb_bm = dev_get_drvdata(dev->parent);
	u32 panic, rw, record_last, trace_addr_match, trace_data_match, match;
	struct apb_bm_configs *cfg = &apb_bm->cfg;
	int ret;

	ret = sscanf(buf, "%x %x %x %x %x %x %x %x %x %x", &match, &cfg->addr,
		     &cfg->addr_mask, &cfg->data, &cfg->data_mask,
		     &trace_addr_match, &trace_data_match, &rw, &record_last,
		     &panic);
	if (ret != 10) {
		dev_err(dev->parent,
			"enter wrong parameter number\n");
		return -EINVAL;
	}

	if (match > SPRD_APB_BM_MATCH_TYPE_MAX) {
		dev_err(dev->parent,
			"enter wrong match parameter\n");
		return -EINVAL;
	}

	if (trace_addr_match > 1) {
		dev_err(dev->parent,
			"enter wrong addr_match parameter\n");
		return -EINVAL;
	}

	if (trace_data_match > 1) {
		dev_err(dev->parent,
			"enter wrong data_match parameter\n");
		return -EINVAL;
	}

	if (rw > SPRD_APB_BM_MATCH_RW) {
		dev_err(dev->parent,
			"enter wrong mode parameter\n");
		return -EINVAL;
	}

	if (record_last > 1) {
		dev_err(dev->parent,
			"enter wrong record parameter\n");
		return -EINVAL;
	}

	cfg->match_det = true;
	apb_bm->panic = panic ? true : false;
	cfg->busmon_cfg &= ~SPRD_APB_BM_MATCH_TYPE_MASK;
	cfg->busmon_cfg |= SPRD_APB_BM_MATCH_TYPE(match);

	if (trace_addr_match)
		cfg->busmon_cfg |= SPRD_APB_BM_TRACE_ADDR_MATCH;
	else
		cfg->busmon_cfg &= ~SPRD_APB_BM_TRACE_ADDR_MATCH;

	if (trace_data_match)
		cfg->busmon_cfg |= SPRD_APB_BM_TRACE_DATA_MATCH;
	else
		cfg->busmon_cfg &= ~SPRD_APB_BM_TRACE_DATA_MATCH;

	if (record_last)
		cfg->busmon_cfg |= SPRD_APB_BM_MATCH_REC_LAST;
	else
		cfg->busmon_cfg &= ~SPRD_APB_BM_MATCH_REC_LAST;

	cfg->busmon_cfg &= ~SPRD_APB_BM_DIR_MASK;
	cfg->busmon_cfg |= SPRD_APB_BM_DIR_SEL(rw);
	cfg->busmon_cfg |= SPRD_APB_BM_MATCH_DET;
	sprd_apb_busmon_hw_cfg(apb_bm);

	return strnlen(buf, count);
}

static ssize_t sprd_apb_busmon_match_cfg_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_apb_busmonitor *apb_bm = dev_get_drvdata(dev->parent);
	struct apb_bm_configs *cfg = &apb_bm->cfg;
	int cnt = 0;

	if (!apb_bm->cfg.match_det) {
		cnt += sprintf(buf + cnt, "match disable\n");
		return cnt;
	}

	cnt += sprintf(buf + cnt, "MATCH FUNCTION:\n");
	if (cfg->busmon_cfg & SPRD_APB_BM_MATCH_ADDR)
		cnt += sprintf(buf + cnt, "addr: %s trigger\n",
			       cfg->busmon_cfg | SPRD_APB_BM_TRACE_ADDR_MATCH ?
			       "match" : "unmatch");
	else
		cnt += sprintf(buf + cnt, "addr match disable\n");

	if (cfg->busmon_cfg & SPRD_APB_BM_MATCH_DATA)
		cnt += sprintf(buf + cnt, "data: %s trigger\n",
			       cfg->busmon_cfg | SPRD_APB_BM_TRACE_DATA_MATCH ?
			       "match" : "unmatch");
	else
		cnt += sprintf(buf + cnt, "data match disable\n");

	if (cfg->busmon_cfg & SPRD_APB_BM_MATCH_WRITE)
		cnt += sprintf(buf + cnt, "direction: write\n");

	if (cfg->busmon_cfg & SPRD_APB_BM_MATCH_READ)
		cnt += sprintf(buf + cnt, "direction: read\n");

	cnt += sprintf(buf + cnt, "addr: 0x%08x, mask: 0x%08x\n",
		       cfg->addr, cfg->addr_mask);
	cnt += sprintf(buf + cnt, "data: 0x%08x, mask: 0x%08x\n",
		       cfg->data, cfg->data_mask);

	if (cfg->busmon_cfg & SPRD_APB_BM_MATCH_REC_LAST)
		cnt += sprintf(buf + cnt, "record last event\n");
	else
		cnt += sprintf(buf + cnt, "record first event\n");

	return cnt;
}

static ssize_t sprd_apb_busmon_hang_cfg_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t count)
{
	struct sprd_apb_busmonitor *apb_bm = dev_get_drvdata(dev->parent);
	struct apb_bm_configs *cfg = &apb_bm->cfg;
	u32 panic, timeout, record;
	int ret;

	ret = sscanf(buf, "%x %x %x", &record, &timeout, &panic);
	if (ret != 3) {
		dev_err(dev->parent,
			"enter wrong parameter number\n");
		return -EINVAL;
	}

	if (record > 1) {
		dev_err(dev->parent,
			"enter wrong record parameter\n");
		return -EINVAL;
	}

	cfg->hang_det = true;
	if (record)
		cfg->busmon_cfg |=  SPRD_APB_BM_HANG_REC_LAST;
	else
		cfg->busmon_cfg &=  ~SPRD_APB_BM_HANG_REC_LAST;

	cfg->busmon_cfg &= ~SPRD_APB_BM_TIMEOUT_MASK;
	cfg->busmon_cfg |= SPRD_APB_BM_TIMEOUT(timeout);
	cfg->busmon_cfg |= SPRD_APB_BM_HANG_DET;
	sprd_apb_busmon_hw_cfg(apb_bm);

	return strnlen(buf, count);
}

static ssize_t sprd_apb_busmon_hang_cfg_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_apb_busmonitor *apb_bm = dev_get_drvdata(dev->parent);
	struct apb_bm_configs *cfg = &apb_bm->cfg;
	int cnt = 0;

	if (!apb_bm->cfg.hang_det) {
		cnt += sprintf(buf + cnt, "hang disable\n");
		return cnt;
	}

	cnt += sprintf(buf + cnt, "HANG FUNCTION:\n");
	cnt += sprintf(buf + cnt, "timeout: %x\n",
		       SPRD_APB_BM_GET_TIMEOUT(cfg->busmon_cfg));

	if (cfg->busmon_cfg & SPRD_APB_BM_HANG_REC_LAST)
		cnt += sprintf(buf + cnt, "hang last event\n");
	else
		cnt += sprintf(buf + cnt, "hang first event\n");

	return cnt;
}

static DEVICE_ATTR(latest_bus_status, 0440, sprd_apb_busmon_bus_status_show,
		   NULL);
static DEVICE_ATTR(dump_scene, 0440, sprd_apb_busmon_dump_show, NULL);
static DEVICE_ATTR(panic, 0440, sprd_apb_busmon_panic_show, NULL);
static DEVICE_ATTR(match, 0644, sprd_apb_busmon_match_cfg_show,
		   sprd_apb_busmon_match_cfg_store);
static DEVICE_ATTR(hang, 0644, sprd_apb_busmon_hang_cfg_show,
		   sprd_apb_busmon_hang_cfg_store);

static struct attribute *apb_busmon_attrs[] = {
	&dev_attr_latest_bus_status.attr,
	&dev_attr_dump_scene.attr,
	&dev_attr_panic.attr,
	&dev_attr_match.attr,
	&dev_attr_hang.attr,
	NULL,
};

static struct attribute_group apb_busmon_group = {
	.attrs = apb_busmon_attrs,
};

struct miscdevice apb_busmon_misc = {
	.name = "apb_bm",
	.parent = NULL,
	.minor = MISC_DYNAMIC_MINOR,
	.fops = NULL,
};

static int sprd_apb_busmon_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sprd_apb_busmonitor *apb_bm;
	struct resource *res;
	u32 args[2];
	int ret;

	apb_bm = devm_kzalloc(&pdev->dev, sizeof(*apb_bm), GFP_KERNEL);
	if (!apb_bm)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	apb_bm->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(apb_bm->base))
		return PTR_ERR(apb_bm->base);

	apb_bm->dev = &pdev->dev;
	apb_bm->panic = of_property_read_bool(np, "sprd,panic");
	apb_bm->irq = platform_get_irq(pdev, 0);
	if (apb_bm->irq < 0)
		return apb_bm->irq;

	ret = of_property_read_u32_array(np, "sprd,target-addr", args, 2);
	if (ret)
		return ret;
	apb_bm->cfg.addr = args[0];
	apb_bm->cfg.addr_mask = args[1];

	ret = of_property_read_u32_array(np, "sprd,target-data", args, 2);
	if (ret)
		return ret;
	apb_bm->cfg.data = args[0];
	apb_bm->cfg.data_mask = args[1];

	sprd_apb_busmon_get_hw_cfg(apb_bm);
	dev_set_drvdata(apb_bm->dev, apb_bm);
	ret = devm_request_threaded_irq(apb_bm->dev, apb_bm->irq,
					sprd_apb_busmon_irq,
					NULL, IRQF_TRIGGER_NONE,
					np->name, apb_bm);
	if (ret) {
		dev_err(apb_bm->dev, "%s request irq fail\n", np->name);
		return ret;
	}

	sprd_apb_busmon_hw_cfg(apb_bm);

	apb_busmon_misc.parent = &pdev->dev;
	ret = misc_register(&apb_busmon_misc);
	if (ret)
		return ret;

	ret = sysfs_create_group(&apb_busmon_misc.this_device->kobj,
				 &apb_busmon_group);
	if (ret) {
		misc_deregister(&apb_busmon_misc);
		return ret;
	}

	return 0;
}

static int sprd_apb_busmon_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&apb_busmon_misc.this_device->kobj,
			   &apb_busmon_group);
	misc_deregister(&apb_busmon_misc);

	return 0;
}

static const struct of_device_id sprd_apb_bm_of_match[] = {
	{ .compatible = "sprd,apb-busmonitor", },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, sprd_apb_bm_of_match);

static struct platform_driver sprd_apb_bm_driver = {
	.probe		= sprd_apb_busmon_probe,
	.remove		= sprd_apb_busmon_remove,
	.driver = {
		.name	= "sprd-apb-busmonitor",
		.of_match_table = sprd_apb_bm_of_match,
	},
};

module_platform_driver(sprd_apb_bm_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lanqing Liu<lanqing.liu@spreadtrum.com>");
MODULE_DESCRIPTION("spreadtrum platform busmonitor driver");
