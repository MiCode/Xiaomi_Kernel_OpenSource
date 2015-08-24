/*
 * Generic page table allocator for IOMMUs.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2014 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#define pr_fmt(fmt)	"io-pgtable: " fmt

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/iommu.h>
#include <linux/debugfs.h>
#include <linux/atomic.h>

#include "io-pgtable.h"

extern struct io_pgtable_init_fns io_pgtable_arm_32_lpae_s1_init_fns;
extern struct io_pgtable_init_fns io_pgtable_arm_32_lpae_s2_init_fns;
extern struct io_pgtable_init_fns io_pgtable_arm_64_lpae_s1_init_fns;
extern struct io_pgtable_init_fns io_pgtable_arm_64_lpae_s2_init_fns;

static const struct io_pgtable_init_fns *
io_pgtable_init_table[IO_PGTABLE_NUM_FMTS] =
{
#ifdef CONFIG_IOMMU_IO_PGTABLE_LPAE
	[ARM_32_LPAE_S1] = &io_pgtable_arm_32_lpae_s1_init_fns,
	[ARM_32_LPAE_S2] = &io_pgtable_arm_32_lpae_s2_init_fns,
	[ARM_64_LPAE_S1] = &io_pgtable_arm_64_lpae_s1_init_fns,
	[ARM_64_LPAE_S2] = &io_pgtable_arm_64_lpae_s2_init_fns,
#endif
};

static struct dentry *io_pgtable_top;

struct io_pgtable_ops *alloc_io_pgtable_ops(enum io_pgtable_fmt fmt,
					    struct io_pgtable_cfg *cfg,
					    void *cookie)
{
	struct io_pgtable *iop;
	const struct io_pgtable_init_fns *fns;

	if (fmt >= IO_PGTABLE_NUM_FMTS)
		return NULL;

	fns = io_pgtable_init_table[fmt];
	if (!fns)
		return NULL;

	iop = fns->alloc(cfg, cookie);
	if (!iop)
		return NULL;

	iop->fmt	= fmt;
	iop->cookie	= cookie;
	iop->cfg	= *cfg;

	return &iop->ops;
}

/*
 * It is the IOMMU driver's responsibility to ensure that the page table
 * is no longer accessible to the walker by this point.
 */
void free_io_pgtable_ops(struct io_pgtable_ops *ops)
{
	struct io_pgtable *iop;

	if (!ops)
		return;

	iop = container_of(ops, struct io_pgtable, ops);
	io_pgtable_init_table[iop->fmt]->free(iop);
}

static atomic_t pages_allocated;

void *io_pgtable_alloc_pages_exact(struct io_pgtable_cfg *cfg, void *cookie,
				   size_t size, gfp_t gfp_mask)
{
	void *ret;

	if (cfg->tlb->alloc_pages_exact)
		ret = cfg->tlb->alloc_pages_exact(cookie, size, gfp_mask);
	else
		ret = alloc_pages_exact(size, gfp_mask);

	if (likely(ret))
		atomic_add(1 << get_order(size), &pages_allocated);

	return ret;
}

void io_pgtable_free_pages_exact(struct io_pgtable_cfg *cfg, void *cookie,
				 void *virt, size_t size)
{
	if (cfg->tlb->free_pages_exact)
		cfg->tlb->free_pages_exact(cookie, virt, size);
	else
		free_pages_exact(virt, size);

	atomic_sub(1 << get_order(size), &pages_allocated);
}

static int io_pgtable_init(void)
{
	io_pgtable_top = debugfs_create_dir("io-pgtable", iommu_debugfs_top);

	if (!io_pgtable_top)
		return -ENODEV;

	if (!debugfs_create_atomic_t("pages", 0600,
				     io_pgtable_top, &pages_allocated)) {
		debugfs_remove_recursive(io_pgtable_top);
		return -ENODEV;
	}

	return 0;
}

static void io_pgtable_exit(void)
{
	debugfs_remove_recursive(io_pgtable_top);
}

module_init(io_pgtable_init);
module_exit(io_pgtable_exit);
