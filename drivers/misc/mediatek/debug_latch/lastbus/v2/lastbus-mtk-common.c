#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <mt-plat/mt_io.h>

#include "../lastbus.h"
#include "lastbus-mtk-common.h"
#include "plat_cfg.h"

static int dump(struct lastbus_plt *plt, char *buf, int len)
{
	void __iomem *mcu_base = plt->common->mcu_base;
	void __iomem *peri_base = plt->common->peri_base;
	unsigned long meter;
	unsigned long debug_raw;
	unsigned long w_counter, r_counter, c_counter;
	char *ptr = buf;
	int i;

	if (!mcu_base && !peri_base)
		return -1;

	if (mcu_base) {
		for (i = 0; i <= plt_cfg.num_master_port-1; ++i) {
			debug_raw = readl(IOMEM(mcu_base + plt_cfg.mcusys_offsets.bus_mcu_m0 + 4 * i));
			meter = readl(IOMEM(mcu_base + plt_cfg.mcusys_offsets.bus_mcu_m0_m + 4 * i));
			w_counter = meter & 0x3f;
			r_counter = (meter >> 8) & 0x3f;

			if ((w_counter != 0) || (r_counter != 0)) {
				ptr += sprintf(ptr, "[LAST BUS] Master %d: ", i);
				ptr += sprintf(ptr,
					"aw_pending_counter = 0x%02lx, ar_pending_counter = 0x%02lx\n",
					w_counter, r_counter);
				ptr += sprintf(ptr, "STATUS = %03lx\n", debug_raw & 0x3ff);
			}
		}

		for (i = 1; i <= plt_cfg.num_slave_port-1; ++i) {
			debug_raw = readl(IOMEM(mcu_base + plt_cfg.mcusys_offsets.bus_mcu_s1 + 4 * (i-1)));
			meter = readl(IOMEM(mcu_base + plt_cfg.mcusys_offsets.bus_mcu_s1_m + 4 * (i-1)));

			w_counter = meter & 0x3f;
			r_counter = (meter >> 8) & 0x3f;
			c_counter = (meter >> 16) & 0x3f;

			if ((w_counter != 0) || (r_counter != 0) || (c_counter != 0)) {
				ptr += sprintf(ptr, "[LAST BUS] Slave %d: ", i);

				ptr += sprintf(ptr,
					"aw_pending_counter = 0x%02lx, ar_pending_counter = 0x%02lx,",
					w_counter, r_counter);
				ptr += sprintf(ptr, " ac_pending_counter = 0x%02lx\n", c_counter);
				if (i <= 2)
					ptr += sprintf(ptr, "STATUS = %04lx\n", debug_raw & 0x3fff);
				else
					ptr += sprintf(ptr, "STATUS = %04lx\n", debug_raw & 0xffff);
			}
		}

	}

	if (peri_base) {
		if (readl(IOMEM(peri_base+plt_cfg.perisys_offsets.bus_peri_r0)) & 0x1) {
			ptr += sprintf(ptr, "[LAST BUS] PERISYS TIMEOUT:\n");
			for (i = 0; i <= plt_cfg.num_perisys_mon-1; ++i)
				ptr += sprintf(ptr, "PERI MON%d = %04x\n",
					i, readl(IOMEM(peri_base+plt_cfg.perisys_offsets.bus_peri_mon+4*i)));
		}
	}

	return 0;
}

static int enable(struct lastbus_plt *plt)
{
	void __iomem *peri_base = plt->common->peri_base;

	if (!peri_base)
		return -1;

	writel(plt_cfg.peri_timeout_setting, IOMEM(peri_base+plt_cfg.perisys_offsets.bus_peri_r0));
	/* enable the perisys debugging funcationality */
	writel(plt_cfg.peri_enable_setting, IOMEM(peri_base+plt_cfg.perisys_offsets.bus_peri_r1));

	return 0;
}

static struct lastbus_plt_operations lastbus_ops = {
	.dump = dump,
	.enable = enable,
};

static int __init lastbus_init(void)
{
	struct lastbus_plt *plt = NULL;
	int ret = 0;

	plt = kzalloc(sizeof(struct lastbus_plt), GFP_KERNEL);
	if (!plt)
		return -ENOMEM;

	plt->ops = &lastbus_ops;
	plt->min_buf_len = 2048;

	ret = lastbus_register(plt);
	if (ret) {
		pr_err("%s:%d: lastbus_register failed\n", __func__, __LINE__);
		goto register_lastbus_err;
	}

	return 0;

register_lastbus_err:
	kfree(plt);
	return ret;
}

core_initcall(lastbus_init);
