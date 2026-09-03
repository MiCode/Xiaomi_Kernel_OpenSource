/* Copyright (C) 2020 Spreadtrum Communications Inc.
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

#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include "../djtag/djtag.h"
#include "busmonitor.h"

#define INT_MSK_STATUS		BIT(31)
#define INT_CLR			BIT(29)
#define INT_EN			BIT(28)
#define CHN_EN			BIT(0)

#define ID_EN			BIT(3)
#define WRITE_EN		BIT(0)
#define WRITE_CFG		BIT(1)

#define UAER_EN			BIT(31)
#define MATCH_WRITE		BIT(29)
#define ADDR_EXC_EN		BIT(28)

#define USER_EN			BIT(31)

#define BM_CONFIG_SIZE		3
#define BM_ID_SIZE		3
#define RANGES_SIZE		2
#define BM_RW_MASK		GENMASK(1, 0)
#define BM_MAX_ADDR		GENMASK(31, 0)
#define BM_OFFSET(type, base)	((type) ? (AXI_##base) : (AHB_##base))
#define DBWRITE(val, base)	master->ops->write(master, (base), 0x20, (val))
#define DBREAD(base)		master->ops->read(master, (base), 0x20)
#define SPRD_BM_W_MODE		0x1
#define SPRD_BM_R_MODE		0x2
#define BUSMONITER_IRQ_MAX  32

struct bm_match {
	u32 id;
	u32 addr;
	u32 addr_h;
	u32 cmd;
	u32 data_l;
	u32 data_h;
	u32 ext_data_l;
	u32 ext_data_h;
	u32 userid;
	const char *name;
};

struct bm_base {
	const char *name;
	u32 type;
	u32 dap;
};

struct bm_configs {
	u32 enable;
	u32 mode;
	u32 include;
	u32 id_enable;
	u32 id;
	u32 id_type;
	u32 start;
	u32 end;
};

struct sprd_busmonitor {
	struct device *dev;
	struct djtag_device *ddev;
	struct bm_base *desc;
	struct bm_configs *cf;
	struct bm_match match;
	struct clk *clk;
	u32 num;
	int count;
	bool retention;
	bool panic;
};

static void sprd_busmon_enable(struct sprd_busmonitor *bm, bool eb, u32 n)
{
	struct djtag_master *master = bm->ddev->master;
	u32 val;

	val = DBREAD(BM_OFFSET(bm->desc[n].type, CHN_INT));
	if (eb)
		DBWRITE(val | CHN_EN, BM_OFFSET(bm->desc[n].type, CHN_INT));
	else
		DBWRITE(val & ~CHN_EN, BM_OFFSET(bm->desc[n].type, CHN_INT));
}

static void sprd_busmon_irq_enable(struct sprd_busmonitor *bm, u32 n)
{
	struct djtag_master *master = bm->ddev->master;
	u32 val;

	val = DBREAD(BM_OFFSET(bm->desc[n].type, CHN_INT));
	DBWRITE(val | INT_EN, BM_OFFSET(bm->desc[n].type, CHN_INT));
}

static void sprd_busmon_irq_clr(struct sprd_busmonitor *bm, u32 n)
{
	struct djtag_master *master = bm->ddev->master;
	u32 val;

	val = DBREAD(BM_OFFSET(bm->desc[n].type, CHN_INT));
	DBWRITE(val & ~INT_CLR, BM_OFFSET(bm->desc[n].type, CHN_INT));
	DBWRITE(val | INT_CLR, BM_OFFSET(bm->desc[n].type, CHN_INT));
}

static void sprd_busmon_config(struct sprd_busmonitor *bm, u32 n)
{
	struct djtag_master *master = bm->ddev->master;
	u32 wr_m[4] = {0, 3, 1, 0};
	u32 val, id_cfg = 0;

	if (!bm->cf[n].enable) {
		dev_err(bm->dev, "%s busmonitor is disable\n",
			bm->desc[n].name);
		return;
	}

	master->ops->mux_sel(master, bm->ddev->sys, bm->desc[n].dap);
	val = DBREAD(BM_OFFSET(bm->desc[n].type, CHN_CFG));
	val = (val & ~BM_RW_MASK) | (wr_m[bm->cf[n].mode] & BM_RW_MASK);
	if (bm->desc[n].type && bm->cf[n].id_enable) {
		if (bm->cf[n].id_type) {
			DBWRITE(bm->cf[n].id | USER_EN, AXI_USER_CFG);
		} else {
			val |= ID_EN;
			id_cfg = id_cfg | bm->cf[n].id;
		}
	}

	if (bm->desc[n].type) {
		id_cfg = bm->cf[n].include ? id_cfg : (id_cfg | ADDR_EXC_EN);
		DBWRITE(id_cfg, AXI_ID_CFG);
	}

	DBWRITE(val, BM_OFFSET(bm->desc[n].type, CHN_CFG));
	DBWRITE(bm->cf[n].start, BM_OFFSET(bm->desc[n].type, ADDR_MIN));
	DBWRITE(bm->cf[n].end, BM_OFFSET(bm->desc[n].type, ADDR_MAX));
	DBWRITE(BM_MAX_ADDR, BM_OFFSET(bm->desc[n].type, DATA_MIN_L32));
	if (bm->desc[n].type) {
		DBWRITE(0, BM_OFFSET(bm->desc[n].type, ADDR_MIN_H32));
		DBWRITE(0, BM_OFFSET(bm->desc[n].type, ADDR_MAX_H32));
	}

	sprd_busmon_enable(bm, true, n);
	sprd_busmon_irq_enable(bm, n);
}

static void sprd_busmon_config_all(struct sprd_busmonitor *bm)
{
	struct djtag_master *master = bm->ddev->master;
	int i, ret;

	ret = master->ops->lock(master);
	if (ret)
		return;
	for (i = 0; i < bm->num; i++)
		sprd_busmon_config(bm, i);
	master->ops->unlock(master);
}

static int sprd_busmon_config_init(struct sprd_busmonitor *bm, u32 n)
{
	u32 size[] = { BM_CONFIG_SIZE, BM_ID_SIZE, RANGES_SIZE };
	static const char * const propname[] = {
		"sprd,bm-config", "sprd,bm-id", "sprd,bm-ranges"
	};
	struct device_node *np = bm->dev->of_node;
	u32 *cf = (u32 *)(bm->cf + n);
	int ret, i, index, key;

	for (i = 0; i < sizeof(propname) / sizeof(char *); i++) {
		for (key = 0; key < size[i]; key++) {
			index = n *  size[i] + key;
			ret = of_property_read_u32_index(np, propname[i],
							 index, cf++);
			if (ret) {
				dev_err(bm->dev,
					"bm %d get %s property fail\n",
					n, propname[i]);
				return ret;
			}
		}
	}

	return 0;
}

static int sprd_busmon_base_init(struct sprd_busmonitor *bm, u32 n)
{
	struct device_node *np = bm->dev->of_node;
	struct bm_base *desc = bm->desc;
	int ret;

	ret = of_property_read_string_index(np, "sprd,bm-name", n,
					    &(desc[n].name));
	if (ret) {
		dev_err(bm->dev, "bm %d get sprd,bm-name property fail\n", n);
		return ret;
	}

	ret = of_property_read_u32_index(np, "sprd,bm-type", n,
					 &(desc[n].type));
	if (ret) {
		dev_err(bm->dev, "bm %d get sprd,bm-type property fail\n", n);
		return ret;
	}

	ret = of_property_read_u32_index(np, "sprd,bm-dap", n, &(desc[n].dap));
	if (ret) {
		dev_err(bm->dev, "bm %d get sprd,bm-dap property fail\n", n);
		return ret;
	}

	return 0;
}

static void sprd_busmon_axi_dump_scene(struct sprd_busmonitor *bm)
{
	bool rw = bm->match.cmd & MATCH_WRITE;

	dev_emerg(bm->dev,
		  "warning! busmonitor detected violated transaction!!!\n");
	dev_emerg(bm->dev, "info: sys:%s BM name:%s %s overlap\n",
			bm->dev->of_node->name,
			bm->match.name, rw ? "write" : "read");
	dev_emerg(bm->dev, "Overlap Addr:0x%08X %08X\n",
			bm->match.addr_h,
			bm->match.addr);
	dev_emerg(bm->dev, "Overlap Data:0x%08X %08X %08X %08X\n",
			bm->match.ext_data_h, bm->match.ext_data_l,
			bm->match.data_h, bm->match.data_l);
	dev_emerg(bm->dev, "Overlap CMD:0x%x\n", bm->match.cmd);
	dev_emerg(bm->dev, "Match ID:0x%x Match USERID:0x%x\n",
			bm->match.id, bm->match.userid);
}

static void sprd_busmon_ahb_dump_scene(struct sprd_busmonitor *bm)
{
	dev_emerg(bm->dev,
		  "warning! busmonitor detected violated transaction!!!\n");
	dev_emerg(bm->dev, "info: sys:%s BM name:%s\n",
			bm->dev->of_node->name,
			bm->match.name);
	dev_emerg(bm->dev, "Overlap Addr:0x%08X\n",
			bm->match.addr);
	dev_emerg(bm->dev, "Overlap Data:0x%08X %08X\n",
			bm->match.data_h, bm->match.data_l);
	dev_emerg(bm->dev, "Overlap CMD:0x%x\n", bm->match.cmd);
}

static void sprd_busmon_axi_match(struct sprd_busmonitor *bm)
{
	struct djtag_master *master = bm->ddev->master;
	u32 *match = (u32 *)&bm->match;
	u32 offset;

	for (offset = AXI_MATCH_ID; offset < AXI_BUS_STATUS; offset++)
		*match++ = DBREAD(offset);
	*match =  DBREAD(AXI_MATCH_USERID);
}

static void sprd_busmon_ahb_match(struct sprd_busmonitor *bm)
{
	struct djtag_master *master = bm->ddev->master;

	bm->match.addr = DBREAD(AHB_MATCH_ADDR);
	bm->match.cmd = DBREAD(AHB_MATCH_CMD);
	bm->match.data_l = DBREAD(AHB_MATCH_DATA_L32);
	bm->match.data_h = DBREAD(AHB_MATCH_DATA_H32);
}

static irqreturn_t sprd_busmon_irq(int irq_num, void *dev)
{
	struct sprd_busmonitor *bm = (struct sprd_busmonitor *) dev;
	struct djtag_master *master = bm->ddev->master;
	struct bm_base *desc = bm->desc;
	u32 i, val;
	int ret;

	ret = master->ops->lock(master);
	if (ret)
		return ret;
	for (i = 0; i < bm->num; i++) {
		master->ops->mux_sel(master, bm->ddev->sys, desc[i].dap);
		val = DBREAD(BM_OFFSET(bm->desc[i].type, CHN_INT));
		if (val & INT_MSK_STATUS)
			break;
	}

	if (i >= bm->num) {
		master->ops->unlock(master);
		return IRQ_NONE;
	}

	bm->match.name = desc[i].name;

	if (bm->desc[i].type)
		sprd_busmon_axi_match(bm);
	else
		sprd_busmon_ahb_match(bm);
	sprd_busmon_irq_clr(bm, i);
	master->ops->unlock(master);

	if (bm->desc[i].type)
		sprd_busmon_axi_dump_scene(bm);
	else
		sprd_busmon_ahb_dump_scene(bm);
	if (bm->panic) {
		dev_emerg(bm->dev, "DJTAG Bus Monitor enter panic!\n");
		BUG();
	}
	bm->count++;
	dev_emerg(bm->dev, "match count = %d\n", bm->count);

	return IRQ_HANDLED;
}

static int sprd_busmon_init(struct sprd_busmonitor *bm)
{
	int i, ret;

	for (i = 0; i < bm->num; i++) {
		ret = sprd_busmon_base_init(bm, i);
		if (ret)
			return ret;
		ret = sprd_busmon_config_init(bm, i);
		if (ret)
			return ret;
	}

	sprd_busmon_config_all(bm);

	return 0;
}

static ssize_t sprd_busmon_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_busmonitor *bm = dev_get_drvdata(dev);
	int i, cnt = 0;

	for (i = 0; i < bm->num; i++)
		cnt += sprintf(buf + cnt, "%d: %s\n", i, bm->desc[i].name);

	return cnt;
}

static ssize_t sprd_busmon_dump_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_busmonitor *bm = dev_get_drvdata(dev);
	bool rw = bm->match.cmd & MATCH_WRITE;
	int cnt = 0;

	if (!bm->count)
		return sprintf(buf,
			"busmonitor do not detect violated transaction\n");

	cnt += sprintf(buf + cnt,
		"warning! busmonitor detected violated transaction!!!\n");
	cnt += sprintf(buf + cnt, "info: sys:%s BM name:%s %s overlap\n",
		       bm->dev->of_node->name,
		       bm->match.name, rw ? "write" : "read");
	cnt += sprintf(buf + cnt, "Overlap Addr:0x%08X%08X\n",
		       bm->match.addr_h, bm->match.addr);
	cnt += sprintf(buf + cnt, "Overlap Data:0x%08X%08X%08X%08X\n",
		       bm->match.ext_data_h, bm->match.ext_data_l,
		       bm->match.data_h, bm->match.data_l);
	cnt += sprintf(buf + cnt, "Overlap CMD:0x%x\n", bm->match.cmd);
	cnt += sprintf(buf + cnt, "Match ID:0x%x Match USERID:0x%x\n",
		       bm->match.id, bm->match.userid);

	return cnt;
}

static ssize_t sprd_busmon_panic_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_busmonitor *bm = dev_get_drvdata(dev);

	return sprintf(buf, "panic mode %s!!!\n",
		       bm->panic ? "open" : "closed");
}

static ssize_t sprd_busmon_active_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct sprd_busmonitor *bm = dev_get_drvdata(dev);
	struct djtag_master *master = bm->ddev->master;
	u32 num, eb;
	int ret;

	ret = sscanf(buf, "%d %d", &num, &eb);
	if (ret != 2) {
		dev_err(dev->parent,
			"enter wrong parameter number\n");
		return -EINVAL;
	}

	if (num >= bm->num) {
		dev_err(dev->parent,
			"enter num wrong parameter\n");
		return -EINVAL;
	}

	ret = master->ops->lock(master);
	if (ret)
		return -ENODEV;
	master->ops->mux_sel(master, bm->ddev->sys, bm->desc[num].dap);
	sprd_busmon_enable(bm, eb ? true : false, num);
	master->ops->unlock(master);

	return strnlen(buf, count);
}

static ssize_t sprd_busmon_cfg_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct sprd_busmonitor *bm = dev_get_drvdata(dev);
	struct djtag_master *master = bm->ddev->master;
	struct bm_configs cfg;
	u32 panic, num;
	int ret;

	ret = sscanf(buf, "%x %x %x %x %x %x %x %x %x",
		&num, &cfg.start, &cfg.end, &cfg.include,
		&cfg.id_enable, &cfg.id_type, &cfg.id, &cfg.mode, &panic);
	if (ret != 9) {
		dev_err(dev->parent,
			"enter wrong parameter number\n");
		return -EINVAL;
	}

	if (num >= bm->num) {
		dev_err(dev->parent,
			"enter num wrong parameter\n");
		return -EINVAL;
	}

	if (cfg.start > cfg.end) {
		dev_err(dev->parent,
			"enter wrong address parameter\n");
		return -EINVAL;
	}

	if (cfg.include > 1) {
		dev_err(dev->parent, "enter wrong include parameter\n");
		return -EINVAL;
	}

	if (cfg.mode > 4) {
		dev_err(dev->parent, "enter wrong mode parameter\n");
		return -EINVAL;
	}

	if (cfg.id_type > 1) {
		dev_err(dev->parent, "enter wrong id_type parameter\n");
		return -EINVAL;
	}

	bm->panic = panic ? true : false;
	cfg.enable = 1;
	memcpy(&bm->cf[num], &cfg, sizeof(struct bm_configs));

	ret = master->ops->lock(master);
	if (ret)
		return -ENODEV;
	sprd_busmon_config(bm, num);
	master->ops->unlock(master);

	return strnlen(buf, count);
}

static ssize_t sprd_busmon_cfg_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_busmonitor *bm = dev_get_drvdata(dev);
	int i, cnt = 0;

	for (i = 0; i < bm->num; i++) {
		if (!bm->cf[i].enable) {
			cnt += sprintf(buf + cnt, "%s: closed\n",
				       bm->desc[i].name);
			continue;
		}

		cnt += sprintf(buf + cnt, "%s: 0x%08X ~ 0x%08X",
			       bm->desc[i].name, bm->cf[i].start,
			       bm->cf[i].end);
		cnt += sprintf(buf + cnt, "%s", bm->cf[i].include ?
			       "  include " : "  exchule ");
		if (bm->cf[i].mode & SPRD_BM_W_MODE)
			cnt += sprintf(buf + cnt, "%c", 'W');
		if (bm->cf[i].mode & SPRD_BM_R_MODE)
			cnt += sprintf(buf + cnt, "%c", 'R');
		cnt += sprintf(buf + cnt, "  id match: %s  ",
			       bm->cf[i].id_enable ? "enable" : "disable");
		cnt += sprintf(buf + cnt, " %s",
			       bm->cf[i].id_type ? "userid" : "axiid");
		cnt += sprintf(buf + cnt, " id: 0x%x\n", bm->cf[i].id);
	}

	cnt += sprintf(buf + cnt, "panic is %s\n",
		       bm->panic ? "open" : "close");

	return cnt;
}

static DEVICE_ATTR(busmonitor, 0440, sprd_busmon_show, NULL);
static DEVICE_ATTR(dump_scene, 0440, sprd_busmon_dump_show, NULL);
static DEVICE_ATTR(panic, 0440, sprd_busmon_panic_show, NULL);
static DEVICE_ATTR(active, 0644, NULL, sprd_busmon_active_store);
static DEVICE_ATTR(config, 0644, sprd_busmon_cfg_show,
		   sprd_busmon_cfg_store);

static struct attribute *busmon_attrs[] = {
	&dev_attr_busmonitor.attr,
	&dev_attr_dump_scene.attr,
	&dev_attr_panic.attr,
	&dev_attr_active.attr,
	&dev_attr_config.attr,
	NULL,
};
static struct attribute_group busmon_group = {
	.attrs = busmon_attrs,
};

static int sprd_busmon_probe(struct djtag_device *ddev)
{
	struct device_node *np = ddev->dev.of_node;
	struct sprd_busmonitor *bm;
	int ret, irq, i;

	bm = devm_kzalloc(&ddev->dev, sizeof(*bm), GFP_KERNEL);
	if (!bm)
		return -ENOMEM;

	bm->dev = &ddev->dev;
	bm->ddev = ddev;
	bm->panic = of_property_read_bool(np, "sprd,panic");
	bm->retention = of_property_read_bool(np, "sprd,retention");
	if (of_property_read_u32(np, "sprd,bm-num", &bm->num)) {
		dev_err(bm->dev, "dt can not find sprd,bm-num\n");
		return -ENODEV;
	}

	bm->desc = devm_kzalloc(&ddev->dev, sizeof(*bm->desc) * bm->num,
				GFP_KERNEL);
	if (!bm->desc)
		return -ENOMEM;

	bm->cf = devm_kzalloc(&ddev->dev, sizeof(*bm->cf) * bm->num,
			      GFP_KERNEL);
	if (!bm->cf)
		return -ENOMEM;

	ret = sprd_busmon_init(bm);
	if (ret)
		return ret;

	dev_set_drvdata(bm->dev, bm);

	for (i = 0; i < BUSMONITER_IRQ_MAX; i++) {
		irq = of_irq_get(ddev->dev.of_node, i);
		if (irq == -ENOENT || irq < 0)
			break;
		ret = devm_request_threaded_irq(&ddev->dev, irq,
						sprd_busmon_irq,
						NULL, IRQF_TRIGGER_NONE | IRQF_SHARED,
						np->name, bm);
		if (ret) {
			dev_err(&ddev->dev, "%s m[%d] request irq fail\n",
				np->name, i);
			return ret;
		}
	}

	ret = sysfs_create_group(&bm->dev->kobj, &busmon_group);
	if (ret) {
		dev_err(bm->dev, "unable to create sysfs\n");
		return ret;
	}

	return 0;
}

static int sprd_busmon_remove(struct djtag_device *ddev)
{
	struct sprd_busmonitor *bm = dev_get_drvdata(&ddev->dev);

	sysfs_remove_group(&bm->dev->kobj, &busmon_group);
	return 0;
}

static int sprd_busmon_resume(struct device *dev)
{
	struct sprd_busmonitor *bm = dev_get_drvdata(dev);

	if (!bm->retention)
		return 0;
	sprd_busmon_config_all(bm);

	return 0;
}

static SIMPLE_DEV_PM_OPS(sprd_bm_pm_ops, NULL,
			 sprd_busmon_resume);

static const struct of_device_id sprd_bm_of_match[] = {
	{ .compatible = "sprd,busmonitor", },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, sprd_bm_of_match);

static struct djtag_driver sprd_bm_driver = {
	.probe		= sprd_busmon_probe,
	.remove		= sprd_busmon_remove,
	.driver = {
		.name	= "sprd-busmonitor",
		.of_match_table = sprd_bm_of_match,
		.pm = &sprd_bm_pm_ops,
	},
};

module_djtag_driver(sprd_bm_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lanqing Liu<lanqing.liu@spreadtrum.com>");
MODULE_DESCRIPTION("spreadtrum platform busmonitor driver");
