#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <mt-plat/mt_io.h>

#include "../bus_tracer_interface.h"
#include "bus_tracer_v1.h"

static int start(struct bus_tracer_plt *plt)
{
	struct device_node *node;
	int ret, num_tracer, i, j;
	u32 args[3];

	if (!plt) {
		pr_err("%s:%d: plt == NULL\n", __func__, __LINE__);
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,bus_tracer-v1");
	if (!node) {
		pr_err("can't find compatible node for bus_tracer\n");
		return -1;
	}

	if (of_property_read_u32(node, "mediatek,num_tracer", &num_tracer) != 0) {
		pr_err("can't find property \"mediatek,num_tracer\" for bus_tracer\n");
		return -1;
	}

	if (num_tracer <= 0) {
		pr_err("[bus tracer] fatal error: num_tracer <= 0\n");
		return -1;
	}

	plt->dem_base = of_iomap(node, 0);
	plt->funnel_base = of_iomap(node, 1);

	plt->num_tracer = num_tracer;
	plt->tracer = kcalloc(num_tracer, sizeof(struct tracer), GFP_KERNEL);

	if (!plt->tracer)
		return -ENOMEM;

	for (i = 0; i <= num_tracer-1; ++i) {
		if (of_property_read_u32_index(node, "mediatek,enabled_tracer", i, &ret) == 0)
			plt->tracer[i].enabled = ret & 0x1;
		else
			plt->tracer[i].enabled = 0;

		if (of_property_read_u32_index(node, "mediatek,at_id", i, &ret) == 0)
			plt->tracer[i].at_id = ret;

		/* arguments for filters */
		if (of_property_read_u32_array(node, "mediatek,watchpoint_filter", args, 3) == 0) {
			plt->tracer[i].filter.watchpoint.addr_h = args[0];
			plt->tracer[i].filter.watchpoint.addr = args[1];
			plt->tracer[i].filter.watchpoint.mask = args[2];
			plt->tracer[i].filter.watchpoint.enabled = 1;
		} else
			plt->tracer[i].filter.watchpoint.enabled = 0;

		if (of_property_read_u32_array(node, "mediatek,bypass_filter", args, 2) == 0) {
			plt->tracer[i].filter.bypass.addr = args[0];
			plt->tracer[i].filter.bypass.mask = args[1];
			plt->tracer[i].filter.bypass.enabled = 1;
		} else
			plt->tracer[i].filter.bypass.enabled = 0;

		plt->tracer[i].filter.idf = (struct id_filter *)
			kzalloc(NUM_ID_FILTER * sizeof(struct id_filter), GFP_KERNEL);

		if (plt->tracer[i].filter.idf) {
			if (of_property_read_u32_array(node, "mediatek,id_filter",
						args, NUM_ID_FILTER) == 0) {
				for (j = 0; j <= NUM_ID_FILTER-1; ++j) {
					plt->tracer[i].filter.idf[j].id = args[j];
					plt->tracer[i].filter.idf[j].enabled = 1;
				}
			} else
				for (j = 0; j <= NUM_ID_FILTER-1; ++j)
					plt->tracer[i].filter.idf[j].enabled = 0;
		} else {
			pr_err("[bus tracer] failed to allocate id filter for tracer %d\n", i);
			return -ENOMEM;
		}

		if (of_property_read_u32_array(node, "mediatek,rw_filter", args, 2) == 0) {
			plt->tracer[i].filter.rwf.read = args[0];
			plt->tracer[i].filter.rwf.write = args[1];
		} else {
			plt->tracer[i].filter.rwf.read = 0;
			plt->tracer[i].filter.rwf.write = 0;
		}

		plt->tracer[i].base = of_iomap(node, 2+i*2);
		plt->tracer[i].etb_base = of_iomap(node, 3+i*2);
		plt->tracer[i].recording = 0;
	}

	return 0;
}

static int dump_etb(void __iomem *base, char **ptr)
{
	unsigned long length, rp, wp, ret;
	unsigned long long packet;
	int i;

	ret = readl(IOMEM(base + ETB_STATUS));
	rp = readl(IOMEM(base + ETB_READADDR));
	wp = readl(IOMEM(base + ETB_WRITEADDR));

	pr_err("[bus tracer] etb status = 0x%lx, rp = 0x%lx, wp = 0x%lx\n",
			ret, rp, wp);
	length = (ret & 0x1) ? readl(IOMEM(base + ETB_DEPTH)) : wp;

	pr_debug("[bus tracer] we get a trace buffer with length = 0x%lx\n", length);
	if (length == 0)
		return -1;

	(*ptr) += sprintf((*ptr), "[BUS TRACER] valid=1, num_entry = %ld\n", length/2);
	for (i = 0; i <= length - 1; i += 2) {
		packet = readl(IOMEM(base + ETB_READMEM));
		packet |= ((unsigned long long) readl(IOMEM(base + ETB_READMEM)) << 32);
		pr_err("%d %llx\n", i/2, packet);
		(*ptr) += sprintf((*ptr), "%16llx\n", packet);
	}

	writel(0x1, IOMEM(base + ETB_CTRL));
	writel(0x0, IOMEM(base + ETB_CTRL));

	writel(0x0, IOMEM(base + ETB_TRIGGERCOUNT));
	writel(0x0, IOMEM(base + ETB_READADDR));
	writel(0x0, IOMEM(base + ETB_WRITEADDR));

	dsb(sy);
	return 0;
}

static int dump(struct bus_tracer_plt *plt, char *buf, int len)
{
	int i, ret = 0;

	for (i = 0; i <= plt->num_tracer-1; ++i) {
		if (plt->tracer[i].recording) {
			pr_err("[bus tracer] tracer %d is running, you must pause it before first\n", i);
			buf += sprintf(buf,
				"[BUS TRACER] valid=0, error: tracer %d is running, you must pause it first\n", i);
			ret = -1;
			continue;
		}

		ret |= dump_etb(
			IOMEM(plt->tracer[i].etb_base)
			, &buf);
	}

	return ret;
}

/* force_enable=0 to enable all the tracers with enabled=1 */
static int enable(struct bus_tracer_plt *plt, unsigned char force_enable, unsigned int tracer_id)
{
	int i, j;
	unsigned long ret;

	if (!plt->tracer) {
		pr_err("%s:%d: plt->tracer == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (!plt->dem_base) {
		pr_err("%s:%d: dem_base == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (!plt->funnel_base) {
		pr_err("%s:%d: funnel_base == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (force_enable)
		plt->tracer[tracer_id].enabled = 1;
	else {
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
	writel(0x1, IOMEM(plt->dem_base + DEM_ATB_CLK));

	/* replicator 1 setup */
	writel(0xc5acce55, IOMEM(plt->funnel_base + REPLICATOR1_BASE + REPLICATOR_LAR));
	writel((~(1 << (plt->tracer[0].at_id >> 4)) & 0xff),
		IOMEM(plt->funnel_base + REPLICATOR1_BASE + REPLICATOR_IDFILTER0));

	if (plt->num_tracer >= 1)
		writel((~(1 << (plt->tracer[1].at_id >> 4)) & 0xff),
			IOMEM(plt->funnel_base + REPLICATOR1_BASE + REPLICATOR_IDFILTER1));

	/* funnel setup */
	writel(0xc5acce55, IOMEM(plt->funnel_base + FUNNEL_LOCKACCESS));
	writel(0xff, IOMEM(plt->funnel_base + FUNNEL_CTRL_REG));
	dsb(sy);

	for (i = 0; i <= plt->num_tracer - 1; ++i) {
		/* enable ETB */
		writel(0xc5acce55, IOMEM(plt->tracer[i].etb_base + ETB_LAR));
		writel(0x1, IOMEM(plt->tracer[i].etb_base + ETB_CTRL));
		dsb(sy);

		/* set ID */
		writel(plt->tracer[i].at_id, IOMEM(plt->tracer[i].base + BUS_TRACE_ATID));
		/* setup filters */
		if (plt->tracer[i].filter.watchpoint.enabled) {
			writel(plt->tracer[i].filter.watchpoint.addr_h,
				IOMEM(plt->tracer[i].base + BUS_TRACE_WATCHPOINT_H));
			writel(plt->tracer[i].filter.watchpoint.addr,
				IOMEM(plt->tracer[i].base + BUS_TRACE_WATCHPOINT));
			writel(plt->tracer[i].filter.watchpoint.mask,
				IOMEM(plt->tracer[i].base + BUS_TRACE_WATCHPOINT_MASK));
			writel(TRACE_WP_EN, IOMEM(plt->tracer[i].base + BUS_MON_CON));
		}

		if (plt->tracer[i].filter.bypass.enabled) {
			writel(plt->tracer[i].filter.bypass.addr >> BYPASS_FILTER_SHIFT,
					IOMEM(plt->tracer[i].base + BUS_TRACE_BYPASS_ADDR));
			writel(plt->tracer[i].filter.bypass.mask >> BYPASS_FILTER_SHIFT,
					IOMEM(plt->tracer[i].base + BUS_TRACE_BYPASS_MASK));
			ret = readl(IOMEM(plt->tracer[i].base + BUS_MON_CON));
			writel(TRACE_BYPASS_EN|ret, IOMEM(plt->tracer[i].base + BUS_MON_CON));
		}

		if (plt->tracer[i].filter.idf) {
			for (j = 0; j <= NUM_ID_FILTER-1; ++j) {
				if (plt->tracer[i].filter.idf[j].enabled) {
					writel(plt->tracer[i].filter.idf[j].id,
						IOMEM(plt->tracer[i].base + BUS_TRACE_IDF0 + (0x4 * j)));
					ret = readl(IOMEM(plt->tracer[i].base + BUS_MON_CON));
					writel(TRACE_IDF_EN|ret, IOMEM(plt->tracer[i].base + BUS_MON_CON));
				}
			}
		}

		writel(plt->tracer[i].filter.rwf.read | (plt->tracer[i].filter.rwf.write << 1)
			, IOMEM(plt->tracer[i].base + BUS_TRACE_RW_FILTER));

		/* enable tracer */
		if (plt->tracer[i].enabled) {
			ret = readl(IOMEM(plt->tracer[i].base + BUS_MON_CON));
			writel(BUS_MON_EN|BUS_TRACE_EN|WDT_RST_EN|ret, IOMEM(plt->tracer[i].base + BUS_MON_CON));
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
		pr_err("%s:%d: plt->tracer == NULL\n", __func__, __LINE__);
		return -1;
	}

	for (i = 0; i <= plt->num_tracer - 1; ++i) {
		/* only pause/resume tracers that are enabled*/
		if (!plt->tracer[i].enabled)
			continue;

		if (pause) {
			/* disable tracer */
			ret = readl(IOMEM(plt->tracer[i].base));
			writel(ret & ~(BUS_MON_EN|BUS_TRACE_EN), IOMEM(plt->tracer[i].base));
			dsb(sy);
			/* disable etb */
			writel(0x0, IOMEM(plt->tracer[i].etb_base + ETB_CTRL));
			dsb(sy);

			plt->tracer[i].recording = 0;
		} else {
			/* enable ETB */
			writel(0x1, IOMEM(plt->tracer[i].etb_base + ETB_CTRL));
			dsb(sy);

			/* enable tracer */
			ret = readl(IOMEM(plt->tracer[i].base));
			writel(ret|BUS_MON_EN|BUS_TRACE_EN, IOMEM(plt->tracer[i].base));
			dsb(sy);

			plt->tracer[i].recording = 1;
		}
	}

	return 0;
}

static int set_watchpoint_filter(struct bus_tracer_plt *plt, struct watchpoint_filter f, unsigned int tracer_id)
{
	unsigned long ret;

	if (!plt->tracer) {
		pr_err("%s:%d: plt->tracer == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (tracer_id > plt->num_tracer) {
		pr_err("%s:%d: tracer_id > plt->num_tracer\n", __func__, __LINE__);
		return -1;
	}

	plt->tracer[tracer_id].filter.watchpoint = f;

	writel(f.addr_h, IOMEM(plt->tracer[tracer_id].base + BUS_TRACE_WATCHPOINT_H));
	writel(f.addr, IOMEM(plt->tracer[tracer_id].base + BUS_TRACE_WATCHPOINT));
	writel(f.mask, IOMEM(plt->tracer[tracer_id].base + BUS_TRACE_WATCHPOINT_MASK));
	ret = readl(IOMEM(plt->tracer[tracer_id].base + BUS_MON_CON));
	if (f.enabled)
		writel(TRACE_WP_EN|ret, IOMEM(plt->tracer[tracer_id].base + BUS_MON_CON));
	else
		writel((~TRACE_WP_EN)&ret, IOMEM(plt->tracer[tracer_id].base + BUS_MON_CON));

	return 0;
}

static int set_bypass_filter(struct bus_tracer_plt *plt, struct bypass_filter f, unsigned int tracer_id)
{
	unsigned long ret;

	if (!plt->tracer) {
		pr_err("%s:%d: plt->tracer == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (tracer_id > plt->num_tracer) {
		pr_err("%s:%d: tracer_id > plt->num_tracer\n", __func__, __LINE__);
		return -1;
	}

	plt->tracer[tracer_id].filter.bypass = f;

	writel(f.addr, IOMEM(plt->tracer[tracer_id].base + BUS_TRACE_BYPASS_ADDR));
	writel(f.mask, IOMEM(plt->tracer[tracer_id].base + BUS_TRACE_BYPASS_MASK));
	ret = readl(IOMEM(plt->tracer[tracer_id].base + BUS_MON_CON));
	if (f.enabled)
		writel(TRACE_BYPASS_EN|ret, IOMEM(plt->tracer[tracer_id].base + BUS_MON_CON));
	else
		writel((~TRACE_BYPASS_EN)&ret, IOMEM(plt->tracer[tracer_id].base + BUS_MON_CON));

	return 0;
}

static int set_id_filter(struct bus_tracer_plt *plt,
		struct id_filter f, unsigned int tracer_id, unsigned int idf_id)
{
	unsigned long ret;

	if (!plt->tracer) {
		pr_err("%s:%d: plt->tracer == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (tracer_id > plt->num_tracer) {
		pr_err("%s:%d: tracer_id > plt->num_tracer\n", __func__, __LINE__);
		return -1;
	}

	if (!plt->tracer[tracer_id].filter.idf) {
		pr_err("%s:%d: plt->tracer[tracer_id].filter.idf == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (idf_id > NUM_ID_FILTER) {
		pr_err("%s:%d: idf_id >  NUM_ID_FILTER\n", __func__, __LINE__);
		return -1;
	}

	plt->tracer[tracer_id].filter.idf[idf_id] = f;

	writel(f.id, IOMEM(plt->tracer[tracer_id].base + BUS_TRACE_IDF0 + (0x4 * idf_id)));
	ret = readl(IOMEM(plt->tracer[tracer_id].base + BUS_MON_CON));
	if (f.enabled)
		writel(TRACE_IDF_EN|ret, IOMEM(plt->tracer[tracer_id].base + BUS_MON_CON));
	else
		writel((~TRACE_IDF_EN)&ret, IOMEM(plt->tracer[tracer_id].base + BUS_MON_CON));

	return 0;
}

static int set_rw_filter(struct bus_tracer_plt *plt, struct rw_filter f, unsigned int tracer_id)
{
	if (!plt->tracer) {
		pr_err("%s:%d: plt->tracer == NULL\n", __func__, __LINE__);
		return -1;
	}

	if (tracer_id > plt->num_tracer) {
		pr_err("%s:%d: tracer_id > plt->num_tracer\n", __func__, __LINE__);
		return -1;
	}

	plt->tracer[tracer_id].filter.rwf = f;

	writel(f.read|(f.write << 1), IOMEM(plt->tracer[tracer_id].base + BUS_TRACE_RW_FILTER));
	return 0;
}

static int dump_setting(struct bus_tracer_plt *plt, char *buf, int len)
{
	int i, j;

	if (!plt->tracer) {
		pr_err("%s:%d: plt->tracer == NULL\n", __func__, __LINE__);
		return -1;
	}

	for (i = 0; i <= plt->num_tracer-1; ++i) {
		buf += sprintf(buf, "== dump setting of tracer %d ==\n", i);
		buf += sprintf(buf, "enabled = %x\ntrace recording = %x\n",
				plt->tracer[i].enabled, plt->tracer[i].recording);

		if (plt->tracer[i].filter.watchpoint.enabled)
			buf += sprintf(buf, "watchpoint = 0x%x 0x%x 0x%x\n",
				plt->tracer[i].filter.watchpoint.addr_h,
				plt->tracer[i].filter.watchpoint.addr,
				plt->tracer[i].filter.watchpoint.mask);

		if (plt->tracer[i].filter.bypass.enabled)
			buf += sprintf(buf, "bypass = 0x%x 0x%x\n",
				plt->tracer[i].filter.bypass.addr,
				plt->tracer[i].filter.bypass.mask);

		for (j = 0; j <= NUM_ID_FILTER-1; ++j) {
			if (plt->tracer[i].filter.idf &&
				plt->tracer[i].filter.idf[j].enabled)
				buf += sprintf(buf, "idf %d = 0x%x\n",
					j, plt->tracer[i].filter.idf[j].id);
		}

		buf += sprintf(buf, "r = 0x%x, w = 0x%x\n",
			plt->tracer[i].filter.rwf.read,
			plt->tracer[i].filter.rwf.write);
	}

	return 0;
}

static struct bus_tracer_plt_operations bus_tracer_ops = {
	.dump = dump,
	.start = start,
	.enable = enable,
	.set_recording = set_recording,
	.set_watchpoint_filter = set_watchpoint_filter,
	.set_bypass_filter = set_bypass_filter,
	.set_id_filter = set_id_filter,
	.set_rw_filter = set_rw_filter,
	.dump_setting = dump_setting,
};

static int __init bus_tracer_init(void)
{
	struct bus_tracer_plt *plt = NULL;
	int ret = 0;

	plt = kzalloc(sizeof(struct bus_tracer_plt), GFP_KERNEL);
	if (!plt)
		return -ENOMEM;

	plt->ops = &bus_tracer_ops;
	plt->min_buf_len = 8192; /* 8K */

	ret = bus_tracer_register(plt);
	if (ret) {
		pr_err("%s:%d: bus_tracer_register failed\n", __func__, __LINE__);
		goto register_bus_tracer_err;
	}

	return 0;

register_bus_tracer_err:
	kfree(plt);
	return ret;
}

core_initcall(bus_tracer_init);
