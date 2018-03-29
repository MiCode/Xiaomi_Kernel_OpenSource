/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <mt-plat/mt_io.h>

#include "../lastpc.h"

struct lastpc_imp {
	struct lastpc_plt plt;
	void __iomem *toprgu_reg;
};

#define to_lastpc_imp(p)	container_of((p), struct lastpc_imp, plt)
#define LASTPC_BUFLEN		2048
#define LASTPC                                0X20
#define LASTSP                                0X24
#define LASTFP                                0X28
#define MUX_CONTOL_C0_REG             (base + 0x140)
#define MUX_READ_C0_REG                       (base + 0x144)
#define MUX_CONTOL_C1_REG		(plt->common->base + 0x21C)
#define MUX_READ_C1_REG			(plt->common->base + 0x258)

static int lastpc_plt_start(struct lastpc_plt *plt)
{
	return 0;
}

static int lastpc_plt_dump(struct lastpc_plt *plt, char *buf, int len)
{
	void __iomem *mcu_base = plt->common->base + 0x410;
	int ret = -1, cnt = num_possible_cpus() - 2;
	char *ptr = buf;
	char *ptr_end;
	unsigned long pc_value;
	unsigned long fp_value;
	unsigned long sp_value;
	unsigned long size = 0;
	unsigned long offset = 0;
	char str[KSYM_SYMBOL_LEN];
	int i, str_len;

	if (cnt < 0)
		return ret;

	ptr_end = buf + min_t(unsigned, len, LASTPC_BUFLEN);

#ifdef CONFIG_ARM64
	/* Get PC, FP, SP and save to buf */
	for (i = 0; i < cnt; i++) {
		pc_value = readq(IOMEM((mcu_base + 0x0 + (i << 5))));
		fp_value = readq(IOMEM((mcu_base + 0x10 + (i << 5))));
		sp_value = readq(IOMEM((mcu_base + 0x18 + (i << 5))));
		kallsyms_lookup(pc_value, &size, &offset, NULL, str);
		str_len = snprintf(ptr, ptr_end - ptr,
			"[LAST PC] CORE_%d PC = 0x%lx(%s + 0x%lx), FP = 0x%lx, SP = 0x%lx\n", i,
			pc_value, str, offset, fp_value, sp_value);
		ptr += str_len;
		if (ptr >= ptr_end)
			goto overflow;
	}
#else
	/* Get PC, FP, SP and save to buf */
	for (i = 0; i < cnt; i++) {
		cluster = i / 4;
		cpu_in_cluster = i % 4;
		pc_value =
		    readl(IOMEM((mcu_base + 0x0) + (cpu_in_cluster << 5) + (0x100 * cluster)));
		fp_value =
		    readl(IOMEM((mcu_base + 0x8) + (cpu_in_cluster << 5) + (0x100 * cluster)));
		sp_value =
		    readl(IOMEM((mcu_base + 0xc) + (cpu_in_cluster << 5) + (0x100 * cluster)));
		kallsyms_lookup((unsigned long)pc_value, &size, &offset, NULL, str);
		ptr +=
		    sprintf(ptr,
			    "[LAST PC] CORE_%d PC = 0x%lx(%s + 0x%lx), FP = 0x%lx, SP = 0x%lx\n", i,
			    pc_value, str, offset, fp_value, sp_value);
		pr_err("[LAST PC] CORE_%d PC = 0x%lx(%s + 0x%lx), FP = 0x%lx, SP = 0x%lx\n", i, pc_value,
			  str, offset, fp_value, sp_value);
	}
#endif

#if 1
	/* Get PC, FP, SP and save to buf */
	for (i = 0; i < 2; i++) {
		writel(1 + (i << 3), MUX_CONTOL_C1_REG);
		pc_value = readq(MUX_READ_C1_REG);
		kallsyms_lookup(pc_value, &size, &offset, NULL, str);
		str_len = snprintf(ptr, ptr_end - ptr,
			"[LAST PC] CORE_%d PC = 0x%lx (%s + 0x%lx)\n",
			i + num_possible_cpus() - 2, pc_value, str, offset);
		ptr += str_len;
		if (ptr >= ptr_end)
			goto overflow;
	}
#endif
	pr_err("%s", buf);

	return 0;

overflow:
	pr_err("[LAST PC] string buffer overflow\n");
	return -EINVAL;
}

static int reboot_test(struct lastpc_plt *plt)
{
	return 0;
}

static struct lastpc_plt_operations lastpc_ops = {
	.start = lastpc_plt_start,
	.dump = lastpc_plt_dump,
	.reboot_test = reboot_test,
};

static int __init lastpc_init(void)
{
	struct lastpc_imp *drv = NULL;
	int ret = 0;

	drv = kzalloc(sizeof(struct lastpc_imp), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->plt.ops = &lastpc_ops;
	drv->plt.chip_code = 0x8173;
	drv->plt.min_buf_len = LASTPC_BUFLEN;	/* TODO: can calculate the len by how many levels of bt we want */

	ret = lastpc_register(&drv->plt);
	if (ret) {
		pr_err("%s:%d: lastpc_register failed\n", __func__, __LINE__);
		goto register_lastpc_err;
	}

	return 0;

register_lastpc_err:
	kfree(drv);
	return ret;
}

arch_initcall(lastpc_init);
