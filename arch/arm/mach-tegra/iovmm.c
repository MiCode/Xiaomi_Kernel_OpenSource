/*
 * arch/arm/mach-tegra/iovmm.c
 *
 * Tegra I/O VM manager
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>

#include <mach/iovmm.h>

#define CREATE_TRACE_POINTS
#include <trace/events/nvmap_iovmm.h>

/*
 * after the best-fit block is located, the remaining pages not needed
 * for the allocation will be split into a new free block if the
 * number of remaining pages is >= MIN_SPLIT_PAGE.
 */
#define MIN_SPLIT_PAGE		4
#define MIN_SPLIT_BYTES(_d)	(MIN_SPLIT_PAGE << (_d)->dev->pgsize_bits)
#define NO_SPLIT(m)		((m) < MIN_SPLIT_BYTES(domain))
#define DO_SPLIT(m)		((m) >= MIN_SPLIT_BYTES(domain))

#define iovmm_start(_b)		((_b)->vm_area.iovm_start)
#define iovmm_length(_b)	((_b)->vm_area.iovm_length)
#define iovmm_end(_b)		(iovmm_start(_b) + iovmm_length(_b))

/* flags for the block */
#define BK_FREE		0 /* indicates free mappings */
#define BK_MAP_DIRTY	1 /* used by demand-loaded mappings */

/* flags for the client */
#define CL_LOCKED	0

/* flags for the domain */
#define DM_MAP_DIRTY	0

struct tegra_iovmm_block {
	struct tegra_iovmm_area vm_area;
	tegra_iovmm_addr_t	start;
	size_t			length;
	atomic_t		ref;
	unsigned long		flags;
	unsigned long		poison;
	struct rb_node		free_node;
	struct rb_node		all_node;
};

struct iovmm_share_group {
	const char			*name;
	struct tegra_iovmm_domain	*domain;
	struct list_head		client_list;
	struct list_head		group_list;
	spinlock_t			lock; /* for client_list */
};

static LIST_HEAD(iovmm_devices);
static LIST_HEAD(iovmm_groups);
static DEFINE_MUTEX(iovmm_group_list_lock);
static struct kmem_cache *iovmm_cache;

#define SIMALIGN(b, a)	(((b)->start % (a)) ? ((a) - ((b)->start % (a))) : 0)

size_t tegra_iovmm_get_max_free(struct tegra_iovmm_client *client)
{
	struct rb_node *n;
	struct tegra_iovmm_block *b;
	struct tegra_iovmm_domain *domain = client->domain;
	tegra_iovmm_addr_t max_free = 0;

	spin_lock(&domain->block_lock);
	n = rb_first(&domain->all_blocks);
	while (n) {
		b = rb_entry(n, struct tegra_iovmm_block, all_node);
		n = rb_next(n);
		if (test_bit(BK_FREE, &b->flags)) {
			max_free = max_t(tegra_iovmm_addr_t,
				max_free, iovmm_length(b));
		}
	}
	spin_unlock(&domain->block_lock);
	return max_free;
}


static void tegra_iovmm_block_stats(struct tegra_iovmm_domain *domain,
	unsigned int *num_blocks, unsigned int *num_free,
	tegra_iovmm_addr_t *total, size_t *total_free, size_t *max_free)
{
	struct rb_node *n;
	struct tegra_iovmm_block *b;

	*num_blocks = 0;
	*num_free = 0;
	*total = 0;
	*total_free = 0;
	*max_free = 0;

	spin_lock(&domain->block_lock);
	n = rb_first(&domain->all_blocks);
	while (n) {
		b = rb_entry(n, struct tegra_iovmm_block, all_node);
		n = rb_next(n);
		(*num_blocks)++;
		*total += b->length;
		if (test_bit(BK_FREE, &b->flags)) {
			(*num_free)++;
			*total_free += b->length;
			*max_free = max_t(size_t, *max_free, b->length);
		}
	}
	spin_unlock(&domain->block_lock);
}

static int tegra_iovmm_read_proc(char *page, char **start, off_t off,
	int count, int *eof, void *data)
{
	struct iovmm_share_group *grp;
	size_t max_free, total_free, total;
	unsigned int num, num_free;

	int len = 0;

	mutex_lock(&iovmm_group_list_lock);
	len += snprintf(page + len, count - len, "\ngroups\n");
	if (list_empty(&iovmm_groups))
		len += snprintf(page + len, count - len, "\t<empty>\n");
	else {
		list_for_each_entry(grp, &iovmm_groups, group_list) {
			len += snprintf(page + len, count - len,
				"\t%s (device: %s)\n",
				grp->name ? grp->name : "<unnamed>",
				grp->domain->dev->name);
			tegra_iovmm_block_stats(grp->domain, &num,
				&num_free, &total, &total_free, &max_free);
			total >>= 10;
			total_free >>= 10;
			max_free >>= 10;
			len += snprintf(page + len, count - len,
				"\t\tsize: %uKiB free: %uKiB "
				"largest: %uKiB (%u free / %u total blocks)\n",
				total, total_free, max_free, num_free, num);
		}
	}
	mutex_unlock(&iovmm_group_list_lock);

	*eof = 1;
	return len;
}

static void iovmm_block_put(struct tegra_iovmm_block *b)
{
	BUG_ON(b->poison);
	BUG_ON(atomic_read(&b->ref) == 0);
	if (!atomic_dec_return(&b->ref)) {
		b->poison = 0xa5a5a5a5;
		kmem_cache_free(iovmm_cache, b);
	}
}

static void iovmm_free_block(struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_block *block)
{
	struct tegra_iovmm_block *pred = NULL; /* address-order predecessor */
	struct tegra_iovmm_block *succ = NULL; /* address-order successor */
	struct rb_node **p;
	struct rb_node *parent = NULL, *temp;
	int pred_free = 0, succ_free = 0;

	iovmm_block_put(block);

	spin_lock(&domain->block_lock);
	temp = rb_prev(&block->all_node);
	if (temp)
		pred = rb_entry(temp, struct tegra_iovmm_block, all_node);
	temp = rb_next(&block->all_node);
	if (temp)
		succ = rb_entry(temp, struct tegra_iovmm_block, all_node);

	if (pred)
		pred_free = test_bit(BK_FREE, &pred->flags);
	if (succ)
		succ_free = test_bit(BK_FREE, &succ->flags);

	if (pred_free && succ_free) {
		pred->length += block->length;
		pred->length += succ->length;
		rb_erase(&block->all_node, &domain->all_blocks);
		rb_erase(&succ->all_node, &domain->all_blocks);
		rb_erase(&succ->free_node, &domain->free_blocks);
		rb_erase(&pred->free_node, &domain->free_blocks);
		iovmm_block_put(block);
		iovmm_block_put(succ);
		block = pred;
	} else if (pred_free) {
		pred->length += block->length;
		rb_erase(&block->all_node, &domain->all_blocks);
		rb_erase(&pred->free_node, &domain->free_blocks);
		iovmm_block_put(block);
		block = pred;
	} else if (succ_free) {
		block->length += succ->length;
		rb_erase(&succ->all_node, &domain->all_blocks);
		rb_erase(&succ->free_node, &domain->free_blocks);
		iovmm_block_put(succ);
	}

	p = &domain->free_blocks.rb_node;
	while (*p) {
		struct tegra_iovmm_block *b;
		parent = *p;
		b = rb_entry(parent, struct tegra_iovmm_block, free_node);
		if (block->length >= b->length)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	rb_link_node(&block->free_node, parent, p);
	rb_insert_color(&block->free_node, &domain->free_blocks);
	set_bit(BK_FREE, &block->flags);
	spin_unlock(&domain->block_lock);
}

/*
 * if the best-fit block is larger than the requested size, a remainder
 * block will be created and inserted into the free list in its place.
 * since all free blocks are stored in two trees the new block needs to be
 * linked into both.
 */
static struct tegra_iovmm_block *iovmm_split_free_block(
	struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_block *block, unsigned long size)
{
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct tegra_iovmm_block *rem;
	struct tegra_iovmm_block *b;

	spin_unlock(&domain->block_lock);
	rem = kmem_cache_zalloc(iovmm_cache, GFP_KERNEL);
	spin_lock(&domain->block_lock);

	if (!rem)
		return NULL;

	p = &domain->free_blocks.rb_node;

	rem->start  = block->start + size;
	rem->length = block->length - size;
	atomic_set(&rem->ref, 1);
	block->length = size;

	while (*p) {
		parent = *p;
		b = rb_entry(parent, struct tegra_iovmm_block, free_node);
		if (rem->length >= b->length)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	set_bit(BK_FREE, &rem->flags);
	rb_link_node(&rem->free_node, parent, p);
	rb_insert_color(&rem->free_node, &domain->free_blocks);

	p = &domain->all_blocks.rb_node;
	parent = NULL;
	while (*p) {
		parent = *p;
		b = rb_entry(parent, struct tegra_iovmm_block, all_node);
		if (rem->start >= b->start)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	rb_link_node(&rem->all_node, parent, p);
	rb_insert_color(&rem->all_node, &domain->all_blocks);

	return rem;
}

static int iovmm_block_splitting;
static struct tegra_iovmm_block *iovmm_alloc_block(
	struct tegra_iovmm_domain *domain, size_t size, size_t align)
{
	struct rb_node *n;
	struct tegra_iovmm_block *b, *best;
	size_t simalign;
	unsigned long page_size = 1 << domain->dev->pgsize_bits;

	BUG_ON(!size);

	size = round_up(size, page_size);
	align = round_up(align, page_size);
	for (;;) {
		spin_lock(&domain->block_lock);
		if (!iovmm_block_splitting)
			break;
		spin_unlock(&domain->block_lock);
		schedule();
	}
	n = domain->free_blocks.rb_node;
	best = NULL;
	while (n) {
		tegra_iovmm_addr_t aligned_start, block_ceil;

		b = rb_entry(n, struct tegra_iovmm_block, free_node);
		simalign = SIMALIGN(b, align);
		aligned_start = b->start + simalign;
		block_ceil = b->start + b->length;

		if (block_ceil >= aligned_start + size) {
			/* Block has enough size */
			best = b;
			if (NO_SPLIT(simalign) &&
				NO_SPLIT(block_ceil - (aligned_start + size)))
				break;
			n = n->rb_left;
		} else {
			n = n->rb_right;
		}
	}
	if (!best) {
		spin_unlock(&domain->block_lock);
		return NULL;
	}

	simalign = SIMALIGN(best, align);
	if (DO_SPLIT(simalign)) {
		iovmm_block_splitting = 1;

		/* Split off misalignment */
		b = best;
		best = iovmm_split_free_block(domain, b, simalign);
		if (best)
			simalign = 0;
		else
			best = b;
	}

	/* Unfree designed block */
	rb_erase(&best->free_node, &domain->free_blocks);
	clear_bit(BK_FREE, &best->flags);
	atomic_inc(&best->ref);

	iovmm_start(best) = best->start + simalign;
	iovmm_length(best) = size;

	if (DO_SPLIT((best->start + best->length) - iovmm_end(best))) {
		iovmm_block_splitting = 1;

		/* Split off excess */
		(void)iovmm_split_free_block(domain, best, size + simalign);
	}

	iovmm_block_splitting = 0;
	spin_unlock(&domain->block_lock);

	return best;
}

static struct tegra_iovmm_block *iovmm_allocate_vm(
	struct tegra_iovmm_domain *domain, size_t size,
	size_t align, unsigned long iovm_start)
{
	struct rb_node *n;
	struct tegra_iovmm_block *b, *best;
	unsigned long page_size = 1 << domain->dev->pgsize_bits;

	BUG_ON(iovm_start % align);
	BUG_ON(!size);

	size = round_up(size, page_size);
	for (;;) {
		spin_lock(&domain->block_lock);
		if (!iovmm_block_splitting)
			break;
		spin_unlock(&domain->block_lock);
		schedule();
	}

	n = rb_first(&domain->free_blocks);
	best = NULL;
	while (n) {
		b = rb_entry(n, struct tegra_iovmm_block, free_node);
		if ((b->start <= iovm_start) &&
		     (b->start + b->length) >= (iovm_start + size)) {
			best = b;
			break;
		}
		n = rb_next(n);
	}

	if (!best)
		goto fail;

	/* split the mem before iovm_start. */
	if (DO_SPLIT(iovm_start - best->start)) {
		iovmm_block_splitting = 1;
		best = iovmm_split_free_block(domain, best,
			(iovm_start - best->start));
	}
	if (!best)
		goto fail;

	/* remove the desired block from free list. */
	rb_erase(&best->free_node, &domain->free_blocks);
	clear_bit(BK_FREE, &best->flags);
	atomic_inc(&best->ref);

	iovmm_start(best) = iovm_start;
	iovmm_length(best) = size;

	BUG_ON(best->start > iovmm_start(best));
	BUG_ON((best->start + best->length) < iovmm_end(best));
	/* split the mem after iovm_start+size. */
	if (DO_SPLIT(best->start + best->length - iovmm_end(best))) {
		iovmm_block_splitting = 1;
		(void)iovmm_split_free_block(domain, best,
			(iovmm_start(best) - best->start + size));
	}
fail:
	iovmm_block_splitting = 0;
	spin_unlock(&domain->block_lock);
	return best;
}

int tegra_iovmm_domain_init(struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_device *dev, tegra_iovmm_addr_t start,
	tegra_iovmm_addr_t end)
{
	struct tegra_iovmm_block *b;
	unsigned long page_size = 1 << dev->pgsize_bits;

	b = kmem_cache_zalloc(iovmm_cache, GFP_KERNEL);
	if (!b)
		return -ENOMEM;

	domain->dev = dev;

	atomic_set(&domain->clients, 0);
	atomic_set(&domain->locks, 0);
	atomic_set(&b->ref, 1);
	spin_lock_init(&domain->block_lock);
	init_rwsem(&domain->map_lock);
	init_waitqueue_head(&domain->delay_lock);

	b->start  = round_up(start, page_size);
	b->length = round_down(end, page_size) - b->start;

	set_bit(BK_FREE, &b->flags);
	rb_link_node(&b->free_node, NULL, &domain->free_blocks.rb_node);
	rb_insert_color(&b->free_node, &domain->free_blocks);
	rb_link_node(&b->all_node, NULL, &domain->all_blocks.rb_node);
	rb_insert_color(&b->all_node, &domain->all_blocks);

	return 0;
}

/*
 * If iovm_start != 0, tries to allocate specified iova block if it is
 * free. if it is not free, it fails.
 */
struct tegra_iovmm_area *tegra_iovmm_create_vm(
	struct tegra_iovmm_client *client, struct tegra_iovmm_area_ops *ops,
	size_t size, size_t align, pgprot_t pgprot, unsigned long iovm_start)
{
	struct tegra_iovmm_block *b;
	struct tegra_iovmm_domain *domain;

	if (!client)
		return NULL;

	domain = client->domain;

	if (iovm_start)
		b = iovmm_allocate_vm(domain, size, align, iovm_start);
	else
		b = iovmm_alloc_block(domain, size, align);
	if (!b)
		return NULL;

	b->vm_area.domain = domain;
	b->vm_area.pgprot = pgprot;
	b->vm_area.ops = ops;

	down_read(&b->vm_area.domain->map_lock);
	if (ops && !test_bit(CL_LOCKED, &client->flags)) {
		set_bit(BK_MAP_DIRTY, &b->flags);
		set_bit(DM_MAP_DIRTY, &client->domain->flags);
	} else if (ops) {
		if (domain->dev->ops->map(domain, &b->vm_area))
			pr_err("%s failed to map locked domain\n", __func__);
	}
	up_read(&b->vm_area.domain->map_lock);

	trace_tegra_iovmm_create_vm(current ? current->comm:"No process ctx",
		iovmm_start(b), iovmm_end(b));

	return &b->vm_area;
}

int tegra_iovmm_vm_insert_pfn(struct tegra_iovmm_area *vm,
	tegra_iovmm_addr_t vaddr, unsigned long pfn)
{
	struct tegra_iovmm_domain *domain = vm->domain;

	BUG_ON(vaddr & ((1 << domain->dev->pgsize_bits) - 1));
	BUG_ON(vaddr >= vm->iovm_start + vm->iovm_length);
	BUG_ON(vaddr < vm->iovm_start);
	BUG_ON(vm->ops);

	domain->dev->ops->map_pfn(domain, vm, vaddr, pfn);
	return 0;
}

void tegra_iovmm_zap_vm(struct tegra_iovmm_area *vm)
{
	struct tegra_iovmm_block *b;
	struct tegra_iovmm_domain *domain;

	b = container_of(vm, struct tegra_iovmm_block, vm_area);
	domain = vm->domain;
	/*
	 * if the vm area mapping was deferred, don't unmap it since
	 * the memory for the page tables it uses may not be allocated
	 */
	down_read(&domain->map_lock);
	if (!test_and_clear_bit(BK_MAP_DIRTY, &b->flags))
		domain->dev->ops->unmap(domain, vm, false);
	up_read(&domain->map_lock);
}

void tegra_iovmm_unzap_vm(struct tegra_iovmm_area *vm)
{
	struct tegra_iovmm_block *b;
	struct tegra_iovmm_domain *domain;

	b = container_of(vm, struct tegra_iovmm_block, vm_area);
	domain = vm->domain;
	if (!vm->ops)
		return;

	down_read(&domain->map_lock);
	if (vm->ops) {
		if (atomic_read(&domain->locks))
			domain->dev->ops->map(domain, vm);
		else {
			set_bit(BK_MAP_DIRTY, &b->flags);
			set_bit(DM_MAP_DIRTY, &domain->flags);
		}
	}
	up_read(&domain->map_lock);
}

void tegra_iovmm_free_vm(struct tegra_iovmm_area *vm)
{
	struct tegra_iovmm_block *b;
	struct tegra_iovmm_domain *domain;

	if (!vm)
		return;

	b = container_of(vm, struct tegra_iovmm_block, vm_area);

	trace_tegra_iovmm_free_vm(current ? current->comm:"No process ctx",
		iovmm_start(b), iovmm_end(b));

	domain = vm->domain;
	down_read(&domain->map_lock);
	if (!test_and_clear_bit(BK_MAP_DIRTY, &b->flags))
		domain->dev->ops->unmap(domain, vm, true);
	iovmm_free_block(domain, b);
	up_read(&domain->map_lock);
}

struct tegra_iovmm_area *tegra_iovmm_area_get(struct tegra_iovmm_area *vm)
{
	struct tegra_iovmm_block *b;

	BUG_ON(!vm);
	b = container_of(vm, struct tegra_iovmm_block, vm_area);

	atomic_inc(&b->ref);
	return &b->vm_area;
}

void tegra_iovmm_area_put(struct tegra_iovmm_area *vm)
{
	struct tegra_iovmm_block *b;
	BUG_ON(!vm);
	b = container_of(vm, struct tegra_iovmm_block, vm_area);
	iovmm_block_put(b);
}

struct tegra_iovmm_area *tegra_iovmm_find_area_get(
	struct tegra_iovmm_client *client, tegra_iovmm_addr_t addr)
{
	struct rb_node *n;
	struct tegra_iovmm_block *b = NULL;

	if (!client)
		return NULL;

	spin_lock(&client->domain->block_lock);
	n = client->domain->all_blocks.rb_node;

	while (n) {
		b = rb_entry(n, struct tegra_iovmm_block, all_node);
		if (iovmm_start(b) <= addr && addr <= iovmm_end(b)) {
			if (test_bit(BK_FREE, &b->flags))
				b = NULL;
			break;
		}
		if (addr > iovmm_start(b))
			n = n->rb_right;
		else
			n = n->rb_left;
		b = NULL;
	}
	if (b)
		atomic_inc(&b->ref);
	spin_unlock(&client->domain->block_lock);
	if (!b)
		return NULL;
	return &b->vm_area;
}

static int _iovmm_client_lock(struct tegra_iovmm_client *client)
{
	struct tegra_iovmm_device *dev;
	struct tegra_iovmm_domain *domain;
	int v;

	if (unlikely(!client))
		return -ENODEV;

	if (unlikely(test_bit(CL_LOCKED, &client->flags))) {
		pr_err("attempting to relock client %s\n", client->name);
		return 0;
	}

	domain = client->domain;
	dev = domain->dev;
	down_write(&domain->map_lock);
	v = atomic_inc_return(&domain->locks);
	/*
	 * if the device doesn't export the lock_domain function, the
	 * device must guarantee that any valid domain will be locked.
	 */
	if (v == 1 && dev->ops->lock_domain) {
		if (dev->ops->lock_domain(domain, client)) {
			atomic_dec(&domain->locks);
			up_write(&domain->map_lock);
			return -EAGAIN;
		}
	}
	if (test_and_clear_bit(DM_MAP_DIRTY, &domain->flags)) {
		struct rb_node *n;
		struct tegra_iovmm_block *b;

		spin_lock(&domain->block_lock);
		n = rb_first(&domain->all_blocks);
		while (n) {
			b = rb_entry(n, struct tegra_iovmm_block, all_node);
			n = rb_next(n);
			if (test_bit(BK_FREE, &b->flags))
				continue;

			if (test_and_clear_bit(BK_MAP_DIRTY, &b->flags)) {
				if (!b->vm_area.ops) {
					pr_err("%s: "
					       "vm_area ops must exist for lazy maps\n",
					       __func__);
					continue;
				}
				dev->ops->map(domain, &b->vm_area);
			}
		}
	}
	set_bit(CL_LOCKED, &client->flags);
	up_write(&domain->map_lock);
	return 0;
}

int tegra_iovmm_client_trylock(struct tegra_iovmm_client *client)
{
	return _iovmm_client_lock(client);
}

int tegra_iovmm_client_lock(struct tegra_iovmm_client *client)
{
	int ret;

	if (!client)
		return -ENODEV;

	ret = wait_event_interruptible(client->domain->delay_lock,
		_iovmm_client_lock(client) != -EAGAIN);

	if (ret == -ERESTARTSYS)
		return -EINTR;

	return ret;
}

void tegra_iovmm_client_unlock(struct tegra_iovmm_client *client)
{
	struct tegra_iovmm_device *dev;
	struct tegra_iovmm_domain *domain;
	int do_wake = 0;

	if (!client)
		return;

	if (!test_and_clear_bit(CL_LOCKED, &client->flags)) {
		pr_err("unlocking unlocked client %s\n", client->name);
		return;
	}

	domain = client->domain;
	dev = domain->dev;
	down_write(&domain->map_lock);
	if (!atomic_dec_return(&domain->locks)) {
		if (dev->ops->unlock_domain)
			dev->ops->unlock_domain(domain, client);
		do_wake = 1;
	}
	up_write(&domain->map_lock);
	if (do_wake)
		wake_up(&domain->delay_lock);
}

size_t tegra_iovmm_get_vm_size(struct tegra_iovmm_client *client)
{
	struct tegra_iovmm_domain *domain;
	struct rb_node *n;
	struct tegra_iovmm_block *b;
	size_t size = 0;

	if (!client)
		return 0;

	domain = client->domain;

	spin_lock(&domain->block_lock);
	n = rb_first(&domain->all_blocks);
	while (n) {
		b = rb_entry(n, struct tegra_iovmm_block, all_node);
		n = rb_next(n);
		size += b->length;
	}
	spin_unlock(&domain->block_lock);

	return size;
}

void tegra_iovmm_free_client(struct tegra_iovmm_client *client)
{
	struct tegra_iovmm_device *dev;
	struct tegra_iovmm_domain *domain;

	if (!client)
		return;

	BUG_ON(!client->domain || !client->domain->dev);

	domain = client->domain;
	dev = domain->dev;

	if (test_and_clear_bit(CL_LOCKED, &client->flags)) {
		pr_err("freeing locked client %s\n", client->name);
		if (!atomic_dec_return(&domain->locks)) {
			down_write(&domain->map_lock);
			if (dev->ops->unlock_domain)
				dev->ops->unlock_domain(domain, client);
			up_write(&domain->map_lock);
			wake_up(&domain->delay_lock);
		}
	}
	mutex_lock(&iovmm_group_list_lock);
	if (!atomic_dec_return(&domain->clients))
		if (dev->ops->free_domain)
			dev->ops->free_domain(domain, client);
	list_del(&client->list);
	if (list_empty(&client->group->client_list)) {
		list_del(&client->group->group_list);
		kfree(client->group->name);
		kfree(client->group);
	}
	kfree(client->name);
	kfree(client);
	mutex_unlock(&iovmm_group_list_lock);
}

struct tegra_iovmm_client *__tegra_iovmm_alloc_client(const char *name,
	const char *share_group, struct miscdevice *misc_dev)
{
	struct tegra_iovmm_client *c = kzalloc(sizeof(*c), GFP_KERNEL);
	struct iovmm_share_group *grp = NULL;
	struct tegra_iovmm_device *dev;

	if (!c)
		return NULL;
	c->name = kstrdup(name, GFP_KERNEL);
	if (!c->name)
		goto fail;
	c->misc_dev = misc_dev;

	mutex_lock(&iovmm_group_list_lock);
	if (share_group) {
		list_for_each_entry(grp, &iovmm_groups, group_list) {
			if (grp->name && !strcmp(grp->name, share_group))
				break;
		}
	}
	if (!grp || strcmp(grp->name, share_group)) {
		grp = kzalloc(sizeof(*grp), GFP_KERNEL);
		if (!grp)
			goto fail_lock;
		grp->name =
			share_group ? kstrdup(share_group, GFP_KERNEL) : NULL;
		if (share_group && !grp->name) {
			kfree(grp);
			goto fail_lock;
		}
		list_for_each_entry(dev, &iovmm_devices, list) {
			grp->domain = dev->ops->alloc_domain(dev, c);
			if (grp->domain)
				break;
		}
		if (!grp->domain) {
			pr_err("%s: alloc_domain failed for %s\n",
				__func__, c->name);
			dump_stack();
			kfree(grp->name);
			kfree(grp);
			grp = NULL;
			goto fail_lock;
		}
		spin_lock_init(&grp->lock);
		INIT_LIST_HEAD(&grp->client_list);
		list_add_tail(&grp->group_list, &iovmm_groups);
	}

	atomic_inc(&grp->domain->clients);
	c->group = grp;
	c->domain = grp->domain;
	spin_lock(&grp->lock);
	list_add_tail(&c->list, &grp->client_list);
	spin_unlock(&grp->lock);
	mutex_unlock(&iovmm_group_list_lock);
	return c;

fail_lock:
	mutex_unlock(&iovmm_group_list_lock);
fail:
	if (c)
		kfree(c->name);
	kfree(c);
	return NULL;
}

int tegra_iovmm_register(struct tegra_iovmm_device *dev)
{
	BUG_ON(!dev);
	mutex_lock(&iovmm_group_list_lock);
	if (list_empty(&iovmm_devices)) {
		iovmm_cache = KMEM_CACHE(tegra_iovmm_block, 0);
		if (!iovmm_cache) {
			pr_err("%s: failed to make kmem cache\n", __func__);
			mutex_unlock(&iovmm_group_list_lock);
			return -ENOMEM;
		}
		create_proc_read_entry("iovmminfo", S_IRUGO, NULL,
			tegra_iovmm_read_proc, NULL);
	}
	list_add_tail(&dev->list, &iovmm_devices);
	mutex_unlock(&iovmm_group_list_lock);
	pr_info("%s: added %s\n", __func__, dev->name);
	return 0;
}

int tegra_iovmm_unregister(struct tegra_iovmm_device *dev)
{
	mutex_lock(&iovmm_group_list_lock);
	list_del(&dev->list);
	mutex_unlock(&iovmm_group_list_lock);
	return 0;
}

static int tegra_iovmm_suspend(void)
{
	int rc = 0;
	struct tegra_iovmm_device *dev;

	list_for_each_entry(dev, &iovmm_devices, list) {
		if (!dev->ops->suspend)
			continue;

		rc = dev->ops->suspend(dev);
		if (rc) {
			pr_err("%s: %s suspend returned %d\n",
			       __func__, dev->name, rc);
			return rc;
		}
	}
	return 0;
}

static void tegra_iovmm_resume(void)
{
	struct tegra_iovmm_device *dev;

	list_for_each_entry(dev, &iovmm_devices, list) {
		if (dev->ops->resume)
			dev->ops->resume(dev);
	}
}

static struct syscore_ops tegra_iovmm_syscore_ops = {
	.suspend = tegra_iovmm_suspend,
	.resume = tegra_iovmm_resume,
};

static __init int tegra_iovmm_syscore_init(void)
{
	register_syscore_ops(&tegra_iovmm_syscore_ops);
	return 0;
}
subsys_initcall(tegra_iovmm_syscore_init);
