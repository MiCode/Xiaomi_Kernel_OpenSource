#include <asm/page.h>
#include <asm/setup.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <mach/mtk_memcfg.h>

#ifdef CONFIG_OF
/* return the actual physical DRAM size */
static u64 kernel_mem_sz;
static u64 phone_dram_sz;	/* original phone DRAM size */
static int dt_scan_memory(unsigned long node, const char *uname, int depth, void *data)
{
	char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	int i;
	__be32 *reg, *endp;
	unsigned long l;
	dram_info_t *dram_info;

	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0) {
		return 0;
	}

	/*
	 * Use kernel_mem_sz if phone_dram_sz is not available (workaround)
	 * Projects use device tree should have orig_dram_info entry in their
	 * device tree.
	 * After the porting is done, kernel_mem_sz will be removed.
	 */
	reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));
	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		if (size == 0)
			continue;

		kernel_mem_sz += size;
	}

	/* orig_dram_info */
	dram_info = (dram_info_t *)of_get_flat_dt_prop(node,
			"orig_dram_info", NULL);
	if (dram_info) {
		for (i = 0; i < dram_info->rank_num; i++)
			phone_dram_sz += dram_info->rank_info[i].size;
	}

	return node;
}

static int __init init_get_max_DRAM_size(void)
{
	if (!phone_dram_sz && !kernel_mem_sz) {
		if (of_scan_flat_dt(dt_scan_memory, NULL)) {
			pr_alert("init_get_max_DRAM_size done. phone_dram_sz: 0x%llx, kernel_mem_sz: 0x%llx\n",
				 (unsigned long long)phone_dram_sz,
				 (unsigned long long)kernel_mem_sz);
		} else {
			pr_err("init_get_max_DRAM_size fail\n");
			BUG();
		}
	}
	return 0;
}

phys_addr_t get_max_DRAM_size(void)
{
	if (!phone_dram_sz && !kernel_mem_sz)
		init_get_max_DRAM_size();
	return phone_dram_sz ? (phys_addr_t)phone_dram_sz : (phys_addr_t)kernel_mem_sz;
}
early_initcall(init_get_max_DRAM_size);
#else
extern phys_addr_t mtk_get_max_DRAM_size(void);
phys_addr_t get_max_DRAM_size(void)
{
	return mtk_get_max_DRAM_size();
}
#endif /* end of CONFIG_OF */
EXPORT_SYMBOL(get_max_DRAM_size);

/*
 * Return the DRAM size used by Linux kernel.
 * In current stage, use phone DRAM size directly
 */
phys_addr_t get_memory_size(void)
{
	return get_max_DRAM_size();
}
EXPORT_SYMBOL(get_memory_size);

phys_addr_t get_phys_offset(void)
{
	return PHYS_OFFSET;
}
EXPORT_SYMBOL(get_phys_offset);
