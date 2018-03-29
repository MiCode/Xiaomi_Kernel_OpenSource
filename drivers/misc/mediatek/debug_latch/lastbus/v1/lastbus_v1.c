/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <mt-plat/mt_io.h>
#include <asm/io.h>

#include "../lastbus.h"
#include "lastbus_v1.h"

struct lastbus_imp {
	struct lastbus_plt plt;
	void __iomem *toprgu_reg;
};

#define to_lastbus_imp(p)	container_of((p), struct lastbus_imp, plt)

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
		for (i = 0; i <= NUM_MASTER_PORT-1; ++i) {
			debug_raw = readl(IOMEM(mcu_base + BUS_MCU_M0 + 4 * i));
			meter = readl(IOMEM(mcu_base + BUS_MCU_M0_M + 4 * i));
			w_counter = meter & 0x3f;
			r_counter = (meter >> 8) & 0x3f;

			if ((w_counter != 0) || (r_counter != 0)) {
				ptr += sprintf(ptr, "[LAST BUS] Master %d: ", i);
				ptr += sprintf(ptr, "aw_pending_counter = 0x%02lx, ar_pending_counter = 0x%02lx\n",
						w_counter, r_counter);
				ptr += sprintf(ptr, "STATUS = %03lx\n", debug_raw & 0x3ff);
			}
		}

		for (i = 1; i <= NUM_SLAVE_PORT; ++i) {
			debug_raw = readl(IOMEM(mcu_base + BUS_MCU_S1 + 4 * (i-1)));
			meter = readl(IOMEM(mcu_base + BUS_MCU_S1_M + 4 * (i-1)));

			w_counter = meter & 0x3f;
			r_counter = (meter >> 8) & 0x3f;
			c_counter = (meter >> 16) & 0x3f;

			if ((w_counter != 0) || (r_counter != 0) || (c_counter != 0)) {
				ptr += sprintf(ptr, "[LAST BUS] Slave %d: ", i);

				ptr += sprintf(ptr,
						"aw_pending_counter = 0x%02lx, ar_pending_counter = 0x%02lx, ac_pending_counter = 0x%02lx\n",
						w_counter, r_counter, c_counter);
				if (i <= 2)
					ptr += sprintf(ptr, "STATUS = %04lx\n", debug_raw & 0x3fff);
				else
					ptr += sprintf(ptr, "STATUS = %04lx\n", debug_raw & 0xffff);
			}
		}

	}

	if (peri_base) {
		if (readl(IOMEM(peri_base+BUS_PERI_R1)) & 0x1) {
			ptr += sprintf(ptr, "[LAST BUS] PERISYS TIMEOUT:\n");
			for (i = 0; i <= NUM_MON-1; ++i)
				ptr += sprintf(ptr, "PERI MON%d = %04x\n", i, readl(IOMEM(peri_base+BUS_PERI_MON+4*i)));
		}
	}

	return 0;
}

static int enable(struct lastbus_plt *plt)
{
	void __iomem *peri_base = plt->common->peri_base;

	if (!peri_base)
		return -1;

	/* timeout set to around 130 ms */
	writel(0x3fff, IOMEM(peri_base+BUS_PERI_R0));
	/* enable the perisys debugging funcationality */
	writel(0xc, IOMEM(peri_base+BUS_PERI_R1));

	return 0;
}

static int test_show(char *buf)
{
	return snprintf(buf, PAGE_SIZE, "==LAST BUS TEST==\n1. test case 1\n2. test case 2\n3. test case 3\n");
}

static int soc_hang_test(unsigned long addr_to_set, unsigned long addr_to_access, unsigned int flag)
{
	void __iomem *p1 = ioremap(addr_to_set, 0x4);
	void __iomem *p2 = ioremap(addr_to_access, 0x4);

	/* *iNFRA_TOPAXI_PROTECTEN (0x1000_1220) |= 0x40 //isolate MFG  */
	writel(readl(p1) | (flag), p1);

	/* Access MM area */
	readl(p2);

	/* clear the setting */
	writel(readl(p1) & ~(flag), p1);
	pr_err("[LAST BUS] SOC hang test failed\n");
	return 1;
}

static int mcusys_hang_test(unsigned long addr_to_set, unsigned int flag)
{
	void __iomem *p1 = ioremap(addr_to_set, 0x4);
	int data_on_dram;
	/* XXX: Turn on 8 cores before test */

	/* *MP1_AXI_CONFIG (0x1020_022C) |= 0x10;   //disable cluster1 snoop channel */
	writel(readl(p1) | (flag), p1);

	/* Access any DRAM address (snooping) or invalidate TLB (DVM) */
	data_on_dram = 1;
	mb();

	pr_err("[LAST BUS] MCUSYS hang test failed\n");
	return 1;
}


static int perisys_hang_test(unsigned long addr_to_set, unsigned long addr_to_access, unsigned int flag)
{
	void __iomem *p1 = ioremap(addr_to_set, 0x4);
	void __iomem *p2 = ioremap(addr_to_access, 0x4);

	writel(readl(p1) | (flag), p1);

	readl(p2);
	return 1;
}

static int test(struct lastbus_plt *plt, int test_case)
{
	switch (test_case) {
	case 1:
		soc_hang_test(0x10001220, 0x14000000, 0x40);
		break;
	case 2:
		mcusys_hang_test(0x1020022C, 0x10);
		break;
	case 3:
		perisys_hang_test(0x10001088, 0x11230000, 1<<2);
		break;
	default:
		break;
	}
	return 0;
}

static struct lastbus_plt_operations lastbus_ops = {
	.dump = dump,
	.test = test,
	.test_show = test_show,
	.enable = enable,
};

static int __init lastbus_init(void)
{
	struct lastbus_imp *drv = NULL;
	int ret = 0;

	drv = kzalloc(sizeof(struct lastbus_imp), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->plt.ops = &lastbus_ops;
	drv->plt.min_buf_len = 2048;	/* TODO: can calculate the len by how many levels of bt we want */

	ret = lastbus_register(&drv->plt);
	if (ret) {
		pr_err("%s:%d: lastbus_register failed\n", __func__, __LINE__);
		goto register_lastbus_err;
	}

	return 0;

register_lastbus_err:
	kfree(drv);
	return ret;
}

core_initcall(lastbus_init);
