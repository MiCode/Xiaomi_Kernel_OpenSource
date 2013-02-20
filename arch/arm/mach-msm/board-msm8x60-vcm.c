/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/bootmem.h>

#include <linux/vcm.h>
#include <linux/vcm_alloc.h>

#define MSM_SMI_BASE           0x38000000
#define MSM_SMI_SIZE           0x04000000

#define SMI_16M	0
#define SMI_1M	1
#define SMI_64K	2
#define SMI_4K	3
#define EBI_16M 4
#define EBI_1M  5
#define EBI_64K 6
#define EBI_4K	7

static void free_ebi_pools(void);

static struct physmem_region memory[] = {
	{	/* SMI 16M */
		.addr = MSM_SMI_BASE,
		.size = SZ_16M,
		.chunk_size = SZ_16M
	},
	{	/* SMI 1M */
		.addr = MSM_SMI_BASE + SZ_16M,
		.size = SZ_8M,
		.chunk_size = SZ_1M
	},
	{	/* SMI 64K */
		.addr = MSM_SMI_BASE + SZ_16M + SZ_8M,
		.size = SZ_4M,
		.chunk_size = SZ_64K
	},
	{	/* SMI 4K */
		.addr = MSM_SMI_BASE + SZ_16M + SZ_8M + SZ_4M,
		.size = SZ_4M,
		.chunk_size = SZ_4K
	},

	{	/* EBI 16M */
		.addr = 0,
		.size = SZ_16M,
		.chunk_size = SZ_16M
	},
	{	/* EBI 1M */
		.addr = 0,
		.size = SZ_8M,
		.chunk_size = SZ_1M
	},
	{	/* EBI 64K */
		.addr = 0,
		.size = SZ_4M,
		.chunk_size = SZ_64K
	},
	{	/* EBI 4K */
		.addr = 0,
		.size = SZ_4M,
		.chunk_size = SZ_4K
	}
};


/* The pool priority MUST be in descending order of size */
static struct vcm_memtype_map mt_map[] __initdata = {
	{
		/* MEMTYPE_0 */
		.pool_id = {SMI_16M, SMI_1M, SMI_64K, SMI_4K},
		.num_pools = 4,
	},
	{
		/* MEMTYPE_1 */
		.pool_id = {SMI_16M, SMI_1M, SMI_64K, EBI_4K},
		.num_pools = 4,
	},
	{	/* MEMTYPE_2 */
		.pool_id = {EBI_16M, EBI_1M, EBI_64K, EBI_4K},
		.num_pools = 4,
	},
	{
		/* MEMTYPE_3 */
		.pool_id = {SMI_16M, SMI_1M, EBI_1M, SMI_64K, EBI_64K, EBI_4K},
		.num_pools = 6,
	}
};

static int __init msm8x60_vcm_init(void)
{
	int ret, i;
	void *ebi_chunk;


	for (i = 0; i < ARRAY_SIZE(memory); i++) {
		if (memory[i].addr == 0) {
			ebi_chunk = __alloc_bootmem(memory[i].size,
							    memory[i].size, 0);
			if (!ebi_chunk) {
				pr_err("Could not allocate VCM-managed physical"
				       " memory\n");
				ret = -ENOMEM;
				goto fail;
			}
			memory[i].addr = __pa(ebi_chunk);
		}
	}

	ret = vcm_sys_init(memory, ARRAY_SIZE(memory),
			   mt_map, ARRAY_SIZE(mt_map),
			   (void *)MSM_SMI_BASE + MSM_SMI_SIZE - SZ_8M, SZ_8M);

	if (ret != 0) {
		pr_err("vcm_sys_init() ret %i\n", ret);
		goto fail;
	}

	return 0;
fail:
	free_ebi_pools();
	return ret;
};

static void free_ebi_pools(void)
{
	int i;
	phys_addr_t r;
	for (i = 0; i < ARRAY_SIZE(memory); i++) {
		r = memory[i].addr;
		if (r > MSM_SMI_BASE + MSM_SMI_SIZE)
			free_bootmem((unsigned long)__va(r), memory[i].size);
	}
}


/* Useful for testing, and if VCM is ever unloaded */
static void __exit msm8x60_vcm_exit(void)
{
	int ret;

	ret = vcm_sys_destroy();
	if (ret != 0) {
		pr_err("vcm_sys_destroy() ret %i\n", ret);
		goto fail;
	}
	free_ebi_pools();
fail:
	return;
}


subsys_initcall(msm8x60_vcm_init);
module_exit(msm8x60_vcm_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Stepan Moskovchenko <stepanm@codeaurora.org>");
