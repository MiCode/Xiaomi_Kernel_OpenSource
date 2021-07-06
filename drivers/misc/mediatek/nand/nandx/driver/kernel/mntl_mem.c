/*
 * Copyright (C) 2019 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#include <linux/of_fdt.h>
#include <linux/module.h>

static u64 mntl_base;
static u64 mntl_size;

static int __init __fdt_scan_reserved_mem(unsigned long node, const char *uname,
					  int depth, void *data)
{
	static int found;
	const __be32 *reg, *endp;
	int l;

	if (!found && depth == 1 && strcmp(uname, "reserved-memory") == 0) {
		found = 1;
		/* scan next node */
		return 0;
	} else if (!found) {
		/* scan next node */
		return 0;
	} else if (found && depth < 2) {
		/* scanning of /reserved-memory has been finished */
		return 1;
	}

	if (!strstr(uname, "KOBuffer"))
		return 0;

	reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));
	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		mntl_base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		mntl_size = dt_mem_next_cell(dt_root_size_cells, &reg);
	}

	return 0;
}

static int __init init_fdt_mntl_buf(void)
{
	of_scan_flat_dt(__fdt_scan_reserved_mem, NULL);

	return 0;
}
early_initcall(init_fdt_mntl_buf);

int get_mntl_buf(u64 *base, u64 *size)
{
	*base = mntl_base;
	*size = mntl_size;

	return 0;
}

