// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Unisoc Communications Inc.

#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define SPRD_APB_PROT_CMD_AND_STATUS			0xff0
#define SPRD_APB_PROT_INT_EVT_EN			0xff4
#define SPRD_APB_PROT_INT_CLR				0xff8
#define SPRD_APB_PROT_DEBUG_SHAPSHOT_IF			0xffc

#define SPRD_APB_PROT_LOCK_ALL_MAGIC			0x898c
#define SPRD_APB_PROT_LOCK_DIRECT_ACC_MAGIC		0x6896
#define SPRD_APB_PROT_UNLOCK_ALL_MAGIC \
			(~SPRD_APB_PROT_LOCK_ALL_MAGIC)
#define SPRD_APB_PROT_UNLOCK_DIRECT_ACC_MAGIC \
			(~SPRD_APB_PROT_LOCK_DIRECT_ACC_MAGIC)

#define SPRD_APB_PROT_GLB_LOCK				BIT(15)

#define SPRD_APB_PROT_AP_LOCK_ID			(1)

#define SPRD_APB_PROT_LOCK_BUT_WRITE_EVT		BIT(0)
#define SPRD_APB_PROT_LOCK_BUT_READ_EVT			BIT(1)
#define SPRD_APB_PROT_CLEAR_LOCK_FAIL_EVT		BIT(3)
#define SPRD_APB_PROT_GRAB_LOCK_FAIL_EVT		BIT(4)
#define SPRD_APB_PROT_CROSS_SWITCH_FAIL_EVT		BIT(5)

#define SPRD_APB_PROT_RECORD_LAST_EVT			BIT(15)

#define SPRD_APB_PROT_LOCK_BUT_WRITE_INT		BIT(16)
#define SPRD_APB_PROT_LOCK_BUT_READ_INT			BIT(17)
#define SPRD_APB_PROT_CLEAR_LOCK_FAIL_INT		BIT(19)
#define SPRD_APB_PROT_GRAB_LOCK_FAIL_INT		BIT(20)
#define SPRD_APB_PROT_CROSS_SWITCH_FAIL_INT		BIT(21)

#define SPRD_APB_PROT_INT_MASK				(0x3f << 16)

#define SPRD_APB_PROT_MAGIC_VALID(magic) \
		(((magic) == SPRD_APB_PROT_LOCK_ALL_MAGIC) \
		|| ((magic) == SPRD_APB_PROT_UNLOCK_ALL_MAGIC) \
		|| ((magic) == SPRD_APB_PROT_LOCK_DIRECT_ACC_MAGIC) \
		|| ((magic) == SPRD_APB_PROT_UNLOCK_DIRECT_ACC_MAGIC))

#define SPRD_APB_PROT_GET_RECORD_ADDR(record)	((record) & 0xffff)
#define SPRD_APB_PROT_GET_RECORD_MASTER(record)	(((record) >> 16) & 0xffff)

struct apb_prot_info {
	struct regmap *addr;
	int irq;
};

struct apb_prot_cfg {
	u32 lock_magic;
	u32 lock_bmp;
	bool glb_lock;
	bool lock_but_write_int;
	bool record_last;
};

struct sprd_apb_prot {
	struct device *dev;
	struct miscdevice misc;
	struct apb_prot_info info;
	struct apb_prot_cfg cfg;
	bool panic;
};

static int sprd_apb_prot_get_lock(struct regmap *map)
{
	int ret;
	unsigned int lock_status, cfg = 0;

	ret = regmap_read(map, SPRD_APB_PROT_CMD_AND_STATUS, &lock_status);
	if (ret)
		return ret;

	if (lock_status & SPRD_APB_PROT_GLB_LOCK) {
		cfg = SPRD_APB_PROT_LOCK_DIRECT_ACC_MAGIC << 16 | SPRD_APB_PROT_AP_LOCK_ID;
		ret = regmap_write(map, SPRD_APB_PROT_CMD_AND_STATUS, cfg);
	}

	return ret;
}

static int sprd_apb_prot_release_lock(struct regmap *map)
{
	int ret;
	unsigned int lock_status, cfg;

	ret = regmap_read(map, SPRD_APB_PROT_CMD_AND_STATUS, &lock_status);
	if (ret)
		return ret;

	if (lock_status & SPRD_APB_PROT_GLB_LOCK) {
		cfg = SPRD_APB_PROT_UNLOCK_DIRECT_ACC_MAGIC << 16 | SPRD_APB_PROT_AP_LOCK_ID;
		ret = regmap_write(map, SPRD_APB_PROT_CMD_AND_STATUS, cfg);
	}

	return ret;
}

int sprd_apb_prot_write(struct regmap *map, unsigned int reg, unsigned int val)
{
	int ret;

	sprd_apb_prot_get_lock(map);

	ret = regmap_write(map, reg, val);

	sprd_apb_prot_release_lock(map);

	return ret;
}
EXPORT_SYMBOL_GPL(sprd_apb_prot_write);

static int sprd_apb_prot_irq_handle(struct apb_prot_info *info,
			struct sprd_apb_prot *apb_prot)
{
	int ret;
	u32 status, record;

	ret = regmap_read(info->addr, SPRD_APB_PROT_CMD_AND_STATUS, &status);
	if (ret)
		return ret;
	dev_emerg(apb_prot->dev, " cmd_and_status: 0x%x\n", status);

	ret = regmap_read(info->addr, SPRD_APB_PROT_DEBUG_SHAPSHOT_IF, &record);
	if (ret)
		return ret;
	dev_emerg(apb_prot->dev, " illegal master: 0x%x\n",
		  SPRD_APB_PROT_GET_RECORD_MASTER(record));
	dev_emerg(apb_prot->dev, " illegal addr_offset: 0x%x\n",
		  SPRD_APB_PROT_GET_RECORD_ADDR(record));

	ret = regmap_write(info->addr, SPRD_APB_PROT_INT_CLR,
			   SPRD_APB_PROT_INT_MASK);
	if (ret)
		return ret;

	if (apb_prot->panic)
		BUG();

	return 0;
}

static irqreturn_t sprd_apb_prot_irq(int irq_num, void *dev)
{
	int ret;
	struct sprd_apb_prot *apb_prot = (struct sprd_apb_prot *)dev;

	dev_emerg(apb_prot->dev, "APB PROT illegal access info:\n");
	ret = sprd_apb_prot_irq_handle(&apb_prot->info, apb_prot);
	if (ret)
		return IRQ_NONE;

	return IRQ_HANDLED;
}

static int sprd_apb_prot_set_int_evt(struct regmap *addr,
				     struct apb_prot_cfg *prot_cfg)
{
	u32 cfg = 0;

	if (prot_cfg->lock_but_write_int)
		cfg = SPRD_APB_PROT_LOCK_BUT_WRITE_INT | SPRD_APB_PROT_LOCK_BUT_WRITE_EVT;

	if (prot_cfg->record_last)
		cfg |= SPRD_APB_PROT_RECORD_LAST_EVT;

	return regmap_write(addr, SPRD_APB_PROT_INT_EVT_EN, cfg);
}

static int sprd_apb_prot_set_lock(struct regmap *addr,
				  struct apb_prot_cfg *prot_cfg)
{
	u32 cfg;

	cfg = prot_cfg->lock_magic << 16;
	if (prot_cfg->glb_lock)
		cfg |= SPRD_APB_PROT_GLB_LOCK;
	cfg |= prot_cfg->lock_bmp;

	return regmap_write(addr, SPRD_APB_PROT_CMD_AND_STATUS, cfg);
}

static void sprd_apb_prot_set_hw(struct sprd_apb_prot *apb_prot)
{
	if (SPRD_APB_PROT_MAGIC_VALID(apb_prot->cfg.lock_magic)) {
		sprd_apb_prot_set_int_evt(apb_prot->info.addr, &apb_prot->cfg);
		sprd_apb_prot_set_lock(apb_prot->info.addr, &apb_prot->cfg);
	}
}

static void sprd_apb_prot_get_cfg(struct sprd_apb_prot *apb_prot)
{
	struct device_node *np = apb_prot->dev->of_node;
	u32 lock_magic;
	u32 lock_id;

	if (!of_property_read_u32(np, "sprd,lock_magic", &lock_magic))
		apb_prot->cfg.lock_magic = lock_magic;
	else
		apb_prot->cfg.lock_magic = 0;

	if (of_property_read_bool(np, "sprd,glb_lock"))
		apb_prot->cfg.glb_lock = true;

	if (!of_property_read_u32(np, "sprd,lock_id", &lock_id))
		apb_prot->cfg.lock_bmp = BIT(lock_id);
	else
		apb_prot->cfg.lock_bmp = 0;

	if (of_property_read_bool(np, "sprd,lock_but_write_int"))
		apb_prot->cfg.lock_but_write_int = true;

	if (of_property_read_bool(np, "sprd,record_last"))
		apb_prot->cfg.record_last = true;

}

static ssize_t sprd_apb_prot_show_hw(struct regmap *addr, char *buf)
{
	u32 reg;
	int ret, cnt = 0;

	ret = regmap_read(addr, SPRD_APB_PROT_CMD_AND_STATUS, &reg);
	if (ret)
		return cnt;
	cnt += sprintf(buf + cnt, "  cmd_and_status: 0x%x\n", reg);

	ret = regmap_read(addr, SPRD_APB_PROT_INT_EVT_EN, &reg);
	if (ret)
		return cnt;
	cnt += sprintf(buf + cnt, "  int_evt_en: 0x%x\n", reg);

	ret = regmap_read(addr, SPRD_APB_PROT_DEBUG_SHAPSHOT_IF, &reg);
	if (ret)
		return cnt;
	cnt += sprintf(buf + cnt, "  debug_shapshot: 0x%x\n", reg);
	cnt += sprintf(buf + cnt, "    master: 0x%x\n",
		       SPRD_APB_PROT_GET_RECORD_MASTER(reg));
	cnt += sprintf(buf + cnt, "    addr_offset: 0x%x\n",
		       SPRD_APB_PROT_GET_RECORD_ADDR(reg));

	return cnt;
}

static ssize_t sprd_apb_prot_reg_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct sprd_apb_prot *apb_prot = dev_get_drvdata(dev);
	int cnt = 0;

	cnt += sprintf(buf + cnt, " apb prot register info\n");
	cnt += sprd_apb_prot_show_hw(apb_prot->info.addr, buf);

	return cnt;
}

static ssize_t sprd_apb_prot_reg_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct sprd_apb_prot *apb_prot = dev_get_drvdata(dev);
	u32 reg_offset, val;
	int ret;

	ret = sscanf(buf, "%x %x", &reg_offset, &val);
	if (ret != 2) {
		dev_err(dev, "enter wrong parameter number\n");
		return -EINVAL;
	}

	regmap_write(apb_prot->info.addr, reg_offset, val);

	return strnlen(buf, count);
}

static ssize_t sprd_apb_prot_panic_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sprd_apb_prot *apb_prot = dev_get_drvdata(dev);

	return sprintf(buf, "panic mode %s!!!\n",
		       apb_prot->panic ? "open" : "closed");
}

static ssize_t sprd_apb_prot_panic_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct sprd_apb_prot *apb_prot = dev_get_drvdata(dev);
	u32 val;
	int ret;

	ret = sscanf(buf, "%x", &val);
	if (ret != 1) {
		dev_err(dev, "enter wrong parameter number\n");
		return -EINVAL;
	}

	apb_prot->panic = val;

	return strnlen(buf, count);
}

static ssize_t sprd_apb_prot_cfg_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct sprd_apb_prot *apb_prot = dev_get_drvdata(dev);
	int cnt = 0;

	cnt += sprintf(buf + cnt, "apb prot cfg info:\n");

	cnt += sprintf(buf + cnt, "  lock magic num: 0x%x\n",
		       apb_prot->cfg.lock_magic);
	cnt += sprintf(buf + cnt, "  glb lock: %s\n",
		       apb_prot->cfg.glb_lock ? "enable" : "disable");
	cnt += sprintf(buf + cnt, "  lock bmp: 0x%x\n",
		       apb_prot->cfg.lock_bmp);
	cnt += sprintf(buf + cnt, "  lock but write int: %s\n",
		       apb_prot->cfg.lock_but_write_int ? "enable" : "disable");
	cnt += sprintf(buf + cnt, "  record mode: %s event\n",
		       apb_prot->cfg.record_last ? "last" : "first");

	return cnt;
}

static ssize_t sprd_apb_prot_cfg_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct sprd_apb_prot *apb_prot = dev_get_drvdata(dev);
	u32 magic, glb_lock, lock_bmp, int_en, record_last, panic;
	struct apb_prot_cfg *prot_cfg;
	int ret;

	ret = sscanf(buf, "%x %x %x %x %x %x", &magic, &glb_lock, &lock_bmp,
		     &int_en, &record_last, &panic);
	if (ret != 6) {
		dev_err(dev, "enter wrong parameter number\n");
		return -EINVAL;
	}

	prot_cfg = &apb_prot->cfg;

	prot_cfg->lock_magic = magic;
	prot_cfg->lock_bmp = lock_bmp;
	prot_cfg->glb_lock = glb_lock ? true : false;
	prot_cfg->lock_but_write_int = int_en ? true : false;
	prot_cfg->record_last = record_last ? true : false;

	apb_prot->panic = panic ? true : false;

	sprd_apb_prot_set_int_evt(apb_prot->info.addr, prot_cfg);
	sprd_apb_prot_set_lock(apb_prot->info.addr, prot_cfg);

	return strnlen(buf, count);
}

static DEVICE_ATTR(reg, 0644, sprd_apb_prot_reg_show, sprd_apb_prot_reg_store);
static DEVICE_ATTR(panic, 0644, sprd_apb_prot_panic_show,
		   sprd_apb_prot_panic_store);
static DEVICE_ATTR(config, 0644, sprd_apb_prot_cfg_show,
		   sprd_apb_prot_cfg_store);

static struct attribute *apb_prot_attrs[] = {
	&dev_attr_reg.attr,
	&dev_attr_panic.attr,
	&dev_attr_config.attr,
	NULL,
};

static struct attribute_group apb_prot_group = {
	.attrs = apb_prot_attrs,
};

static int sprd_apb_prot_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sprd_apb_prot *apb_prot;
	int ret;

	apb_prot = devm_kzalloc(&pdev->dev, sizeof(*apb_prot), GFP_KERNEL);
	if (!apb_prot)
		return -ENOMEM;

	apb_prot->dev = &pdev->dev;

	apb_prot->info.addr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
							      "sprd,apb-syscon");
	if (IS_ERR(apb_prot->info.addr)) {
		dev_err(&pdev->dev, "get the apb node fail\n");
		return PTR_ERR(apb_prot->info.addr);
	}

	apb_prot->panic = of_property_read_bool(np, "sprd,panic");

	apb_prot->info.irq = platform_get_irq(pdev, 0);
	if (apb_prot->info.irq < 0)
		return apb_prot->info.irq;

	dev_set_drvdata(apb_prot->dev, apb_prot);

	sprd_apb_prot_get_cfg(apb_prot);

	ret = devm_request_threaded_irq(apb_prot->dev, apb_prot->info.irq,
					sprd_apb_prot_irq,
					NULL, IRQF_TRIGGER_NONE,
					np->name, apb_prot);
	if (ret) {
		dev_err(apb_prot->dev, "%s request apb prot irq fail\n", np->name);
		return ret;
	}

	sprd_apb_prot_set_hw(apb_prot);

	ret = sysfs_create_group(&apb_prot->dev->kobj, &apb_prot_group);

	return ret;
}

static int sprd_apb_prot_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &apb_prot_group);

	return 0;
}

static const struct of_device_id sprd_apb_prot_of_match[] = {
	{ .compatible = "sprd,apb-prot", },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, sprd_apb_prot_of_match);

static struct platform_driver sprd_apb_prot_driver = {
	.probe = sprd_apb_prot_probe,
	.remove = sprd_apb_prot_remove,
	.driver = {
		.name = "sprd-apb-prot",
		.of_match_table = sprd_apb_prot_of_match,
	},
};

static int __init sprd_apb_prot_init(void)
{
	return platform_driver_register(&sprd_apb_prot_driver);
}
postcore_initcall(sprd_apb_prot_init);

static void __exit sprd_apb_prot_exit(void)
{
	platform_driver_unregister(&sprd_apb_prot_driver);
}
module_exit(sprd_apb_prot_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Xiaopeng Bai<xiaopeng.bai@unisoc.com>");
MODULE_DESCRIPTION("unisoc platform apb_prot driver");
