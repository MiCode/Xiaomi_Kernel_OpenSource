// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/circ_buf.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/slab.h>

#include "../bus_tracer_interface.h"
#include "bus_tracer_v1.h"

static int start(struct bus_tracer_plt *plt)
{
	struct device_node *node;
	int ret, num_tracer, i;
	/* u32 args[3]; */

	if (!plt) {
		pr_notice("%s:%d: plt == NULL\n", __func__, __LINE__);
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,bus_tracer-v1");
	if (!node) {
		pr_notice("can't find compatible node for bus_tracer\n");
		return -1;
	}

	if (of_property_read_u32(node, "mediatek,err_flag",
				&plt->err_flag) != 0)
		plt->err_flag = 0xFFF8FFFF;

	if (of_property_read_u32(node, "mediatek,num_tracer",
				&num_tracer) != 0) {
		pr_notice("can't find property \"mediatek,num_tracer\"\n");
		return -1;
	}

	if (num_tracer <= 0) {
		pr_notice("[bus tracer] fatal error: num_tracer <= 0\n");
		return -1;
	}

	plt->dem_base = of_iomap(node, 0);
	plt->dbgao_base = of_iomap(node, 1);
	plt->funnel_base = of_iomap(node, 2);
	plt->etb_base = of_iomap(node, 3);

	plt->num_tracer = num_tracer;
	plt->tracer = kcalloc(num_tracer, sizeof(struct tracer), GFP_KERNEL);

	if (!plt->tracer)
		return -ENOMEM;

	for (i = 0; i <= num_tracer-1; ++i) {
		if (of_property_read_u32_index(node, "mediatek,enabled_tracer",
					i, &ret) == 0)
			plt->tracer[i].enabled = ret & 0x1;
		else
			plt->tracer[i].enabled = 0;

		if (of_property_read_u32_index(node, "mediatek,at_id", i,
					&ret) == 0)
			plt->tracer[i].at_id = ret;

		plt->tracer[i].base = of_iomap(node, 4+i);
		plt->tracer[i].recording = 0;
	}

	return 0;
}

static void enable_etb(void __iomem *base)
{
	unsigned int depth, i;

	if (!base)
		return;

	CS_UNLOCK(base);
	depth = readl(base + ETB_DEPTH);
	writel(0x0, base + ETB_WRITEADDR);

	for (i = 0; i < depth; ++i)
		writel(0x0, base + ETB_RWD);

	writel(0x0, base + ETB_WRITEADDR);
	writel(0x0, base + ETB_READADDR);

	writel(0x1, base + ETB_CTRL);
	CS_LOCK(base);
	dsb(sy);
}

static int dump(struct bus_tracer_plt *plt, char *buf, int len)
{
	return 0;
}

/* force_enable=0 to enable all the tracers with enabled=1 */
static int enable(struct bus_tracer_plt *plt, unsigned char force_enable,
		unsigned int tracer_id)
{
	int i, j;
	unsigned long ret;

	if (!plt->tracer) {
		pr_notice("%s:%d: plt->tracer == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (!plt->dem_base) {
		pr_notice("%s:%d: dem_base == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (!plt->dbgao_base) {
		pr_notice("%s:%d: dbgao_base == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (!plt->funnel_base) {
		pr_notice("%s:%d: funnel_base == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (force_enable) {
		for (j = 0; j <= plt->num_tracer - 1; ++j)
			plt->tracer[j].enabled = 1;
	} else {
		j = 0;
		for (i = 0; i <= plt->num_tracer - 1; ++i)
			j += plt->tracer[i].enabled;
		if (j == 0) {
			pr_debug("%s:%d: all the tracers are disabled\n",
				__func__, __LINE__);
			return -2;
		}
	}

	/* enable ATB clk */
	writel(0x1, plt->dbgao_base + DEM_ATB_CLK);
	dsb(sy);

	writel(0x1, plt->dem_base + DEM_ATB_CLK);
	dsb(sy);

	/* set dem_ao unlock */
	CS_UNLOCK(plt->dem_base);
	dsb(sy);

	/* set DBGRST_ALL to 0 */
	writel(0x0, plt->dem_base + DEM_DBGRST_ALL);
	dsb(sy);

	CS_LOCK(plt->dem_base);
	dsb(sy);

	/* timestamp per 10us; 1: 76.923ns; 13: 1us */
	writel(0x8201, plt->dem_base + INSERT_TS0);
	//writel(0x0D01, plt->dem_base + INSERT_TS0);

	writel(plt->err_flag, plt->dbgao_base + DBG_ERR_FLAG_IRQ_POLARITY);
	dsb(sy);

	writel(0x0, plt->dbgao_base + DBG_ERR_FLAG_CON);
	dsb(sy);
	writel(0x3, plt->dbgao_base + DBG_ERR_FLAG_CON);
	dsb(sy);

	/* replicator 1 setup */
	CS_UNLOCK(plt->funnel_base + REPLICATOR1_BASE);

	writel(0x00, plt->funnel_base + REPLICATOR1_BASE +
			REPLICATOR_IDFILTER0);
	writel(0x00, plt->funnel_base + REPLICATOR1_BASE +
			REPLICATOR_IDFILTER1);

	CS_LOCK(plt->funnel_base + REPLICATOR1_BASE);
	dsb(sy);

	/* funnel setup */
	CS_UNLOCK(plt->funnel_base);
	writel(0xff, plt->funnel_base + FUNNEL_CTRL_REG);
	CS_LOCK(plt->funnel_base);
	dsb(sy);

	for (i = 0; i <= plt->num_tracer - 1; ++i) {
		/* enable ETB */
		enable_etb(plt->etb_base);

		/* set ID */
		writel(plt->tracer[i].at_id, plt->tracer[i].base +
				BUS_TRACE_ATID);

		writel(0x1, plt->tracer[i].base + BUS_TRACE_CON_SYNC_SET);

		ret = readl(plt->tracer[i].base + BUS_TRACE_CON_SYNC_STATUS);
		while (0 == (ret & 0x1))
			ret = readl(plt->tracer[i].base + BUS_TRACE_CON_SYNC_STATUS);

		/* enable tracer */
		if (plt->tracer[i].enabled) {
			ret = readl(plt->tracer[i].base + BUS_TRACE_CON);
			writel(ret|BUS_TRACE_EN|WDT_RST_EN|SW_RST_B,
				plt->tracer[i].base + BUS_TRACE_CON);
			plt->tracer[i].recording = 1;
		}
		dsb(sy);
	}

	return 0;
}

static int set_recording(struct bus_tracer_plt *plt, unsigned char pause)
{
	int i;
	unsigned long ret;

	if (!plt->tracer) {
		pr_notice("%s:%d: plt->tracer == NULL\n", __func__, __LINE__);
		return -1;
	}

	for (i = 0; i <= plt->num_tracer - 1; ++i) {
		/* only pause/resume tracers that are enabled*/
		if (!plt->tracer[i].enabled)
			continue;

		if (pause) {
			/* disable tracer */
			ret = readl(plt->tracer[i].base);
			writel(ret & ~(BUS_TRACE_EN), plt->tracer[i].base +
					BUS_TRACE_CON);
			dsb(sy);

			/* disable etb */
			/* disable_etb(plt->etb_base); */

			plt->tracer[i].recording = 0;
		} else {
			/* enable ETB */
			enable_etb(plt->etb_base);

			/* enable tracer */
			ret = readl(plt->tracer[i].base);
			writel(ret|BUS_TRACE_EN, plt->tracer[i].base +
					BUS_TRACE_CON);
			dsb(sy);

			plt->tracer[i].recording = 1;
		}
	}

	return 0;
}

static int dump_setting(struct bus_tracer_plt *plt, char *buf, int len)
{
	int i;

	if (!plt->tracer) {
		pr_notice("%s:%d: plt->tracer == NULL\n", __func__, __LINE__);
		return -1;
	}

	for (i = 0; i <= plt->num_tracer-1; ++i) {
		buf += sprintf(buf, "== dump setting of tracer %d ==\n", i);
		buf += sprintf(buf, "enabled = %x\ntrace recording = %x\n",
			plt->tracer[i].enabled, plt->tracer[i].recording);
		buf += sprintf(buf, "==== register dump ====\n");
		buf += sprintf(buf, "0x0 = 0x%lx\n0x10 = 0x%lx\n0x14 = 0x%lx\n"
			"0x18 = 0x%lx\n",
			(unsigned long) readl(plt->tracer[i].base),
			(unsigned long) readl(plt->tracer[i].base + 0x10),
			(unsigned long) readl(plt->tracer[i].base + 0x14),
			(unsigned long) readl(plt->tracer[i].base + 0x18));
	}

	buf += sprintf(buf, "== DBG_ERR_FLAG register dump  ==\n");
	buf += sprintf(buf, "0x80 = 0x%lx\n0x84 = 0x%lx\n0x88 = 0x%lx\n"
		"0x8c = 0x%lx\n",
		(unsigned long) readl(plt->dbgao_base + 0x80),
		(unsigned long) readl(plt->dbgao_base + 0x84),
		(unsigned long) readl(plt->dbgao_base + 0x88),
		(unsigned long) readl(plt->dbgao_base + 0x8c));

	buf += sprintf(buf, "0x90 = 0x%lx\n0x94 = 0x%lx\n0x98 = 0x%lx\n"
		"0x9c = 0x%lx\n",
		(unsigned long) readl(plt->dbgao_base + 0x90),
		(unsigned long) readl(plt->dbgao_base + 0x94),
		(unsigned long) readl(plt->dbgao_base + 0x98),
		(unsigned long) readl(plt->dbgao_base + 0x9c));

	return 0;
}

static int resume(struct bus_tracer_plt *plt, struct platform_device *pdev)
{
	return 0;
}

static struct bus_tracer_plt_operations bus_tracer_ops = {
	.dump = dump,
	.start = start,
	.enable = enable,
	.set_recording = set_recording,
	/* .set_watchpoint_filter = set_watchpoint_filter, */
	/* .set_bypass_filter = set_bypass_filter, */
	/* .set_id_filter = set_id_filter, */
	/* .set_rw_filter = set_rw_filter, */
	.dump_setting = dump_setting,
	.resume = resume,
};

struct bus_tracer_plt *plt;
EXPORT_SYMBOL(plt);

static int __init bus_tracer_init(void)
{
	plt = kzalloc(sizeof(struct bus_tracer_plt), GFP_KERNEL);
	if (!plt)
		return -ENOMEM;

	plt->ops = &bus_tracer_ops;
	plt->min_buf_len = 8192; /* 8K */

	//set_sram_flag_timestamp();

	return 0;
}

static void __exit bus_tracer_exit(void)
{
	kfree(plt);
}

module_init(bus_tracer_init);
module_exit(bus_tracer_exit);

MODULE_LICENSE("GPL v2");

