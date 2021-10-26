/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * This file help debug multimedia HW smi-larb hang and monitor smi-larb.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <soc/mediatek/smi.h>

/* m4u/smi-common use this struction. */
struct smi_test_dev {
	void __iomem *base;
	struct device *dev;
};

struct smi_larb_dev {
	void __iomem *base;
	struct device *dev;

	int         larbid;

	struct smi_test_dev *m4u;
	struct smi_test_dev *common;
};

#ifndef MTK_LARB_NR_MAX
#define MTK_LARB_NR_MAX		16
#endif
struct mtk_smi_dbg {
	struct smi_larb_dev larb[MTK_LARB_NR_MAX];

	struct smi_test_dev m4u[2];
	struct smi_test_dev smi_common[2];

	bool res_init;
	/*
	 *Sometime don't touch the larb power and clock to avoid affect their
	 *status.
	 *-false: defaultly, enable the power/clk before dump.
	 *-true: Don't enable the power/clk before dump.
	 */
	bool larb_power_disable;

	int  mon_larb_mode; /* 0. all, 1: read, 2: write.*/
	int  mon_common_read;
	unsigned long long mon_timeout;
};

static const char smi_m4u_str[] = "mediatek,mt8168-m4u";

static int mtk_smi_debug_res_init(struct mtk_smi_dbg *dbgmng)
{
	struct device_node *m4u_dev_node = NULL;
	struct platform_device *pdev;
	struct resource *res;
	struct smi_larb_dev *larb;
	struct smi_test_dev *m4u;
	struct smi_test_dev *common;
	int m4uidx = 0;
	int ret, larb_nr, i;

	/* Find M4U device. */
	do {
		m4u_dev_node = of_find_compatible_node(m4u_dev_node, NULL,
						       smi_m4u_str);
		if (!m4u_dev_node)
			break;
		pdev = of_find_device_by_node(m4u_dev_node);
		of_node_put(m4u_dev_node);
		if (!pdev)
			return -ENODEV;
		m4u = &dbgmng->m4u[m4uidx];

		m4u->dev = &pdev->dev;
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			return -ENODEV;
		m4u->base = devm_ioremap_nocache(m4u->dev, res->start,
						 0x1000);
		if (IS_ERR(m4u->base))
			return PTR_ERR(m4u->base);

		/* Parse the larbs */
		larb_nr = of_count_phandle_with_args(m4u_dev_node,
						     "mediatek,larbs", NULL);
		if (larb_nr < 0)
			return larb_nr;
		for (i = 0; i < larb_nr; i++) {
			struct device_node *larbnode, *smi_node;
			struct platform_device *plarbdev, *pcommondev;
			unsigned int idx;

			larbnode = of_parse_phandle(m4u_dev_node,
						    "mediatek,larbs", i);
			if (!larbnode)
				return -EINVAL;

			if (!of_device_is_available(larbnode))
				continue;

			ret = of_property_read_u32(larbnode, "mediatek,larb-id",
						   &idx);
			if (ret)
				idx = i;
			larb = &dbgmng->larb[idx];
			WARN_ON(!!larb->dev);
			larb->larbid = idx;

			plarbdev = of_find_device_by_node(larbnode);
			of_node_put(larbnode);

			larb->dev = &plarbdev->dev;
			res = platform_get_resource(plarbdev, IORESOURCE_MEM,
						    0);
			larb->base = devm_ioremap_nocache(larb->dev,
							  res->start, 0x1000);
			if (IS_ERR(larb->base))
				return PTR_ERR(larb->base);
			larb->m4u = m4u;

			/* Parse the smi-common in smi-larb */
			smi_node = of_parse_phandle(larbnode, "mediatek,smi",
						    0);
			if (!smi_node)
				return -EINVAL;
			pcommondev = of_find_device_by_node(smi_node);
			of_node_put(smi_node);

			common = &dbgmng->smi_common[0];
			if (!common->dev)
				common->dev = &pcommondev->dev;

			if (common->dev != &pcommondev->dev) {
				/* Try smi common1 */
				common = &dbgmng->smi_common[1];
				if (!common->dev)
					common->dev = &pcommondev->dev;
			}

			if (!common->base) {
				res = platform_get_resource(pcommondev,
							    IORESOURCE_MEM, 0);
				common->base = devm_ioremap_nocache(common->dev,
								    res->start,
								    0x1000);
				if (IS_ERR(common->base))
					return PTR_ERR(common->base);
			}
			larb->common = common;
			dev_info(larb->dev, "id %d, common %s.\n",
				 larb->larbid, dev_name(common->dev));
		}

		m4uidx++;
	} while (m4u_dev_node);

	dbgmng->mon_common_read = 1; /* common default dump read. */
	dbgmng->mon_timeout = 167000000ULL; /* 167ms */
	dbgmng->res_init = true;

	return 0;
}

static int smi_larb_power_get(struct device *dev, unsigned int id,
			      bool larb_power_disable)
{
	if (larb_power_disable)
		return 0;

	return pm_runtime_get_sync(dev);
}

static void smi_larb_power_put(struct device *dev, unsigned int id,
			      bool larb_power_disable)
{
	if (larb_power_disable)
		return;

	pm_runtime_put_sync(dev);
}

static void smi_m4u_dump(struct device *dev, void __iomem *base)
{
	if (!base)
		return;
	dev_info(dev, "0x48:0x%x(0xa->common 0x2). 0x54:0x%x.\n",
		 readl_relaxed(base + 0x48), readl_relaxed(base + 0x54));
}

static void smi_larb_dump(struct device *dev, void __iomem *base)
{
	int i, j, cnt = 5;

	if (!base) {
		dev_info(dev, "base is null\n");
		return;
	}

	dev_info(dev, "0xb0(fifo status) 0x%x-0x%x-0x%x. 0x24 0x%x\n",
		 readl_relaxed(base + 0xb0), readl_relaxed(base + 0xb4),
		 readl_relaxed(base + 0xb8), readl_relaxed(base + 0x24));
	dev_info(dev, "0xa0(violation) 0x%x-0x%x-0x%x-0x%x(0 is expected)\n",
		 readl_relaxed(base + 0xa0), readl_relaxed(base + 0xa4),
		 readl_relaxed(base + 0xa8), readl_relaxed(base + 0xac));
	dev_info(dev, "0x0c(slp_en) 0x%x, 0xc8(ext_ongoing) 0x%x\n",
		readl_relaxed(base + 0x00c), readl_relaxed(base + 0x0c8));
	dev_info(dev, "0x60(outstanding) 0x%x, below is 0x200+x\n",
		 readl_relaxed(base + 0x60));

	for (i = 0; i < 32; i += 8) /* 32 */
		dev_info(dev, "0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x\n",
			 readl_relaxed(base + 0x200 + i * 4),
			 readl_relaxed(base + 0x200 + (i + 1) * 4),
			 readl_relaxed(base + 0x200 + (i + 2) * 4),
			 readl_relaxed(base + 0x200 + (i + 3) * 4),
			 readl_relaxed(base + 0x200 + (i + 4) * 4),
			 readl_relaxed(base + 0x200 + (i + 5) * 4),
			 readl_relaxed(base + 0x200 + (i + 6) * 4),
			 readl_relaxed(base + 0x200 + (i + 7) * 4));
	for (i = 0; i < 32; i += 8) /* 32 */
		dev_info(dev, "mmu:0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x\n",
			 readl_relaxed(base + 0x380 + i * 4),
			 readl_relaxed(base + 0x380 + (i + 1) * 4),
			 readl_relaxed(base + 0x380 + (i + 2) * 4),
			 readl_relaxed(base + 0x380 + (i + 3) * 4),
			 readl_relaxed(base + 0x380 + (i + 4) * 4),
			 readl_relaxed(base + 0x380 + (i + 5) * 4),
			 readl_relaxed(base + 0x380 + (i + 6) * 4),
			 readl_relaxed(base + 0x380 + (i + 7) * 4));

	for (j = 0; j < cnt; j++) {
		u32 status = readl_relaxed(base + 0x0);

		dev_info(dev,
			 "dump(%d/%d):0x0(%s) 0x%x. 0xbc 0x%x Outstand:\n",
			 j, cnt, status ? "busy" : "idle", status,
			 readl_relaxed(base + 0xbc));
		for (i = 0; i < 32; i += 8) /* 32 */
			dev_info(dev,
				 "0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x\n",
				 readl_relaxed(base + 0x280 + i * 4),
				 readl_relaxed(base + 0x280 + (i + 1) * 4),
				 readl_relaxed(base + 0x280 + (i + 2) * 4),
				 readl_relaxed(base + 0x280 + (i + 3) * 4),
				 readl_relaxed(base + 0x280 + (i + 4) * 4),
				 readl_relaxed(base + 0x280 + (i + 5) * 4),
				 readl_relaxed(base + 0x280 + (i + 6) * 4),
				 readl_relaxed(base + 0x280 + (i + 7) * 4));
	}

	dev_info(dev, "larb dump done\n");
}

static void smi_common_dump(struct device *dev, void __iomem *base)
{
	int i, cnt = 5;

	dev_info(dev, "0x220 0x%x 0x100 0x%x 0x300(dcm) 0x%x\n",
		 readl_relaxed(base + 0x220), readl_relaxed(base + 0x100),
		 readl_relaxed(base + 0x300));

	if (readl_relaxed(base + 0x234) != 0x06080608)
		dev_info(dev, "0x234 fail 0x%x\n", readl_relaxed(base + 0x234));
	if (readl_relaxed(base + 0x238) != 0x05070507)
		dev_info(dev, "0x238 fail 0x%x\n", readl_relaxed(base + 0x238));
	if (readl_relaxed(base + 0x23c) != 0x05070507)
		dev_info(dev, "0x23c fail 0x%x\n", readl_relaxed(base + 0x23c));

	dev_info(dev, "clamp:0x3c0 0x%x(should 0), 0x3c8:0x%x.\n",
		 readl_relaxed(base + 0x3c0), readl_relaxed(base + 0x3c8));

	for (i = 0; i < cnt; i++) {
		u32 status = readl_relaxed(base + 0x440);

		dev_info(
		       dev,
		       "axi input(%d/%d):0x400 0x%x-0x%x-0x%x-0x%x,0x%x-0x%x-0x%x-0x%x\n",
		       i, cnt,
		       readl_relaxed(base + 0x400), readl_relaxed(base + 0x404),
		       readl_relaxed(base + 0x408), readl_relaxed(base + 0x40c),
		       readl_relaxed(base + 0x410), readl_relaxed(base + 0x414),
		       readl_relaxed(base + 0x418), readl_relaxed(base + 0x41c)
		       );
		dev_info(
		       dev, "axi output:0x430 0x%x-0x%x, 0x440 0x%x(%s)\n",
		       readl_relaxed(base + 0x430), readl_relaxed(base + 0x434),
		       status, (status & 0x1) ? "idle" : "busy");
	}
}

static void smi_larb_monitor_start(struct device *dev, void __iomem *base,
				   unsigned int portid, int mode)
{
	u32 reg;

	writel(1, base + 0x404); /* clear */
	writel_relaxed(portid, base + 0x408);

	reg = 0x1 << 9;
	reg |= (mode & 0x3) << 2; /* read/write.*/
	writel_relaxed(reg, base + 0x40c);
	writel(0x1, base + 0x400); /* start */
}

static void smi_larb_monitor_stop(struct device *dev, void __iomem *base)
{
	writel(0x0, base + 0x400); /* stop */

	dev_info(dev, "stop larb monitor and dump:\n");
	dev_info(dev, "0x410 0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x.\n",
		 readl_relaxed(base + 0x410), readl_relaxed(base + 0x414),
		 readl_relaxed(base + 0x418), readl_relaxed(base + 0x41c),
		 readl_relaxed(base + 0x420), readl_relaxed(base + 0x424),
		 readl_relaxed(base + 0x428));
}

static void smi_common_monitor_start(struct device *dev, void __iomem *base,
				     bool read)
{
	/*0x1b0[7:4] axi output id, 0 or 1. default always is 0. */
	writel_relaxed(0x1, base + 0x1b0);

	writel_relaxed(!read, base + 0x1ac);

	writel_relaxed(0x1, base + 0x1a4); /* clear */
	writel(0x1, base + 0x1a0); /* start */
}

static void smi_common_monitor_stop(struct device *dev, void __iomem *base)
{
	writel(0x0, base + 0x1a0); /* stop */

	dev_info(dev, "stop common monitor and dump:\n");
	dev_info(dev, "0x1c0 0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x.\n",
		 readl_relaxed(base + 0x1c0), readl_relaxed(base + 0x1c4),
		 readl_relaxed(base + 0x1c8), readl_relaxed(base + 0x1cc),
		 readl_relaxed(base + 0x1d0), readl_relaxed(base + 0x1d4),
		 readl_relaxed(base + 0x1d8));
}

/*
 * param: 0xabcd
 *     a: test case
 *        0: dump hang reg;    3: set monitor type(read/write)  4: mon timeout
 *        1: monitor start;
 *     b: larb id              commmon mode(0:read; 1:write)     timeout ms
 *     cd: port id             larbmod(0:both, 1:read, 2:write)  timeout ms
 */
static int smi_debug_set(void *data, u64 val)
{
	unsigned int testcase = (val & 0xf000) >> 12;
	unsigned int larbid = (val & 0xf00) >> 8;
	unsigned int portid = (val & 0xff);
	struct mtk_smi_dbg *dbgmng = data;
	unsigned long long start, end;
	struct smi_larb_dev *larb;
	struct smi_test_dev *smi_common, *m4u;
	struct device *larbdev;
	int ret = 0;

	pr_info("%s:val=%llx, case%d larb %d, port %d\n",
		__func__, val, testcase, larbid, portid);

	if (larbid >= MTK_LARB_NR_MAX)
		return -EINVAL;

	if (!dbgmng->res_init) {
		ret = mtk_smi_debug_res_init(dbgmng);
		if (ret) {
			pr_info("smi debug res init fail\n");
			return ret;
		}
	}

	larb = &dbgmng->larb[larbid];
	larbdev = larb->dev;
	smi_common = larb->common;
	m4u = larb->m4u;

	if (!smi_common || !larbdev) {
		pr_info("smi dev is null\n");
		return -EINVAL;
	}

	switch (testcase) {
	case 0: {/* hang dump. */
		ret = smi_larb_power_get(larbdev, larbid,
					 dbgmng->larb_power_disable);
		if (ret < 0)
			dev_info(larbdev, "power/clk enable fail\n");
		smi_common_dump(smi_common->dev, smi_common->base);
		smi_m4u_dump(m4u->dev, m4u->base);
		smi_larb_dump(larbdev, larb->base);
		smi_larb_power_put(larbdev, larbid, dbgmng->larb_power_disable);
	}
	break;

	case 1: {/* larb monitor start.*/
		ret = smi_larb_power_get(larbdev, larbid,
					 dbgmng->larb_power_disable);
		if (ret < 0)
			dev_info(larbdev, "power/clk enable fail\n");
		dev_info(larbdev, "begin to monitor larb %d port %d\n",
			 larbid, portid);

		start = sched_clock();
		/* larb default dump both(read and write). */
		smi_larb_monitor_start(larbdev, larb->base,
				       portid, dbgmng->mon_larb_mode);
		/* common default dump read. */
		smi_common_monitor_start(smi_common->dev, smi_common->base,
					 !!dbgmng->mon_common_read);

		/* Suppose this is 4k video case.
		 * 60fps, then 1s/60 = 16.7ms per frame.
		 * Only monitor 10 frame here. that is 10*16.7ms = 167ms
		 */
		do {
			end = sched_clock(); /* unit is ns */
			usleep_range(1000, 3000);
		} while (end - start < dbgmng->mon_timeout);

		smi_larb_monitor_stop(larbdev, larb->base);
		smi_common_monitor_stop(smi_common->dev, smi_common->base);
		smi_larb_power_put(larbdev, larbid, dbgmng->larb_power_disable);

		dev_info(larbdev, "monitor larb %d, port %d done\n",
			 larbid, portid);
	}
	break;

	case 3: {/* monitor type */
		int larbmod = portid & 0x3; /*larb: 1: read, 2 write. 0: both */
		bool commonread = !larbid; /*common: 0: read, 1: write */

		if (larbmod != 1 && larbmod != 2)
			larbmod = 0;
		dbgmng->mon_larb_mode = larbmod;
		dbgmng->mon_common_read = commonread;
		dev_info(larbdev, "larbmod %d. common read %d\n",
			 larbmod, commonread);
	}
	break;
	case 4: {
		unsigned int time_ms = val & 0xfff;

		dbgmng->mon_timeout = time_ms * 1000000ULL;
		pr_info("monitor timeout is %d ms\n", time_ms);
	}
	break;

	case 5:
		dbgmng->larb_power_disable = !dbgmng->larb_power_disable;
		pr_info("larb_power_disable %d.\n", dbgmng->larb_power_disable);
		break;

	default:
		pr_err("smi_dbg_set error,val=%llu\n", val);
	}

	return ret;
}

static int smi_debug_get(void *data, u64 *val)
{
	pr_info("help smi larb debug.\n");
	*val = 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(smi_debug_fops, smi_debug_get,
			smi_debug_set, "%llu\n");

static int __init mtk_smi_debug_init(void)
{
	struct mtk_smi_dbg *dbg_mng;
	struct dentry *dbg;

	dbg_mng = kzalloc(sizeof(*dbg_mng), GFP_KERNEL);
	if (!dbg_mng)
		return -ENOMEM;

	dbg = debugfs_create_file("smi", 0644, NULL, dbg_mng, &smi_debug_fops);
	if (IS_ERR(dbg)) {
		kfree(dbg_mng);
		return PTR_ERR(dbg);
	}

	return 0;
}
late_initcall(mtk_smi_debug_init);

MODULE_DESCRIPTION("MEDIATEK SMI debug driver");
MODULE_AUTHOR("Yong Wu <yong.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
