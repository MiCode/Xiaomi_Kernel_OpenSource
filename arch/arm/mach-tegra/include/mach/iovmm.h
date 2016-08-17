/*
 * arch/arm/mach-tegra/include/mach/iovmm.h
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed i the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/rbtree.h>
#include <linux/rwsem.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#ifndef _MACH_TEGRA_IOVMM_H_
#define _MACH_TEGRA_IOVMM_H_

typedef u32 tegra_iovmm_addr_t;

struct tegra_iovmm_device_ops;

/*
 * each I/O virtual memory manager unit should register a device with
 * the iovmm system
 */
struct tegra_iovmm_device {
	struct tegra_iovmm_device_ops	*ops;
	const char			*name;
	struct list_head		list;
	int				pgsize_bits;
};

/*
 * tegra_iovmm_domain serves a purpose analagous to mm_struct as defined in
 * <linux/mm_types.h> - it defines a virtual address space within which
 * tegra_iovmm_areas can be created.
 */
struct tegra_iovmm_domain {
	atomic_t		clients;
	atomic_t		locks;
	spinlock_t		block_lock;  /* RB-tree for iovmm_area blocks */
	unsigned long		flags;
	wait_queue_head_t	delay_lock;  /* when lock_client fails */
	struct rw_semaphore	map_lock;
	struct rb_root		all_blocks;  /* ordered by address */
	struct rb_root		free_blocks; /* ordered by size */
	struct tegra_iovmm_device *dev;
};

/*
 * tegra_iovmm_client is analagous to an individual task in the task group
 * which owns an mm_struct.
 */

struct iovmm_share_group;

#if !defined(CONFIG_IOMMU_API)

struct tegra_iovmm_client {
	const char			*name;
	unsigned long			flags;
	struct iovmm_share_group	*group;
	struct tegra_iovmm_domain	*domain;
	struct miscdevice		*misc_dev;
	struct list_head		list;
};

/*
 * tegra_iovmm_area serves a purpose analagous to vm_area_struct as defined
 * in <linux/mm_types.h> - it defines a virtual memory area which can be
 * mapped to physical memory by a client-provided mapping function. */

struct tegra_iovmm_area {
	struct tegra_iovmm_domain	*domain;
	tegra_iovmm_addr_t		iovm_start;
	size_t				iovm_length;
	pgprot_t			pgprot;
	struct tegra_iovmm_area_ops	*ops;
};

#else	/* CONFIG_IOMMU_API */

/*
 * To replace IOVMM with IOMMU backend
 */

struct tegra_iovmm_client {
	struct device *dev;
};

struct tegra_iovmm_area {
	dma_addr_t		iovm_start;
	size_t			iovm_length;
	pgprot_t		pgprot;
	struct device		*dev;
};

#endif /* CONFIG_IOMMU_API */

struct tegra_iovmm_device_ops {
	/* maps a VMA using the page residency functions provided by the VMA */
	int (*map)(struct tegra_iovmm_domain *domain,
		struct tegra_iovmm_area *io_vma);
	/* marks all PTEs in a VMA as invalid; decommits the virtual addres
	 * space (potentially freeing PDEs when decommit is true.) */
	void (*unmap)(struct tegra_iovmm_domain *domain,
		struct tegra_iovmm_area *io_vma, bool decommit);
	void (*map_pfn)(struct tegra_iovmm_domain *domain,
		struct tegra_iovmm_area *io_vma,
		unsigned long offs, unsigned long pfn);
	/*
	 * ensures that a domain is resident in the hardware's mapping region
	 * so that it may be used by a client
	 */
	int (*lock_domain)(struct tegra_iovmm_domain *domain,
		struct tegra_iovmm_client *client);
	void (*unlock_domain)(struct tegra_iovmm_domain *domain,
		struct tegra_iovmm_client *client);
	/*
	 * allocates a vmm_domain for the specified client; may return the same
	 * domain for multiple clients
	 */
	struct tegra_iovmm_domain* (*alloc_domain)(
		struct tegra_iovmm_device *dev,
		struct tegra_iovmm_client *client);
	void (*free_domain)(struct tegra_iovmm_domain *domain,
		struct tegra_iovmm_client *client);
	int (*suspend)(struct tegra_iovmm_device *dev);
	void (*resume)(struct tegra_iovmm_device *dev);
};

struct tegra_iovmm_area_ops {
	/*
	 * ensures that the page of data starting at the specified offset
	 * from the start of the iovma is resident and pinned for use by
	 * DMA, returns the system pfn, or an invalid pfn if the
	 * operation fails.
	 */
	unsigned long (*lock_makeresident)(struct tegra_iovmm_area *area,
		tegra_iovmm_addr_t offs);
	/* called when the page is unmapped from the I/O VMA */
	void (*release)(struct tegra_iovmm_area *area, tegra_iovmm_addr_t offs);
};

#ifdef CONFIG_TEGRA_IOVMM
/*
 * called by clients to allocate an I/O VMM client mapping context which
 * will be shared by all clients in the same share_group
 */
struct tegra_iovmm_client *__tegra_iovmm_alloc_client(const char *name,
	const char *share_group, struct miscdevice *misc_dev);

static inline struct tegra_iovmm_client *tegra_iovmm_alloc_client(
	struct device *dev, const char *share_group,
	struct miscdevice *misc_dev)
{
	return __tegra_iovmm_alloc_client(dev_name(dev), share_group, misc_dev);
}

size_t tegra_iovmm_get_vm_size(struct tegra_iovmm_client *client);

void tegra_iovmm_free_client(struct tegra_iovmm_client *client);

/*
 * called by clients to ensure that their mapping context is resident
 * before performing any DMA operations addressing I/O VMM regions.
 * client_lock may return -EINTR.
 */
int tegra_iovmm_client_lock(struct tegra_iovmm_client *client);
int tegra_iovmm_client_trylock(struct tegra_iovmm_client *client);

/* called by clients after DMA operations are complete */
void tegra_iovmm_client_unlock(struct tegra_iovmm_client *client);

/*
 * called by clients to allocate a new iovmm_area and reserve I/O virtual
 * address space for it. if ops is NULL, clients should subsequently call
 * tegra_iovmm_vm_map_pages and/or tegra_iovmm_vm_insert_pfn to explicitly
 * map the I/O virtual address to an OS-allocated page or physical address,
 * respectively. VM operations may be called before this call returns
 */
struct tegra_iovmm_area *tegra_iovmm_create_vm(
	struct tegra_iovmm_client *client, struct tegra_iovmm_area_ops *ops,
	size_t size, size_t align, pgprot_t pgprot, unsigned long iovm_start);

/*
 * called by clients to "zap" an iovmm_area, and replace all mappings
 * in it with invalid ones, without freeing the virtual address range
 */
void tegra_iovmm_zap_vm(struct tegra_iovmm_area *vm);

/*
 * after zapping a demand-loaded iovmm_area, the client should unzap it
 * to allow the VMM device to remap the page range.
 */
void tegra_iovmm_unzap_vm(struct tegra_iovmm_area *vm);

/* called by clients to return an iovmm_area to the free pool for the domain */
void tegra_iovmm_free_vm(struct tegra_iovmm_area *vm);

/* returns size of largest free iovm block */
size_t tegra_iovmm_get_max_free(struct tegra_iovmm_client *client);

/*
 * called by client software to map the page-aligned I/O address vaddr to
 * a specific physical address pfn. I/O VMA should have been created with
 * a NULL tegra_iovmm_area_ops structure.
 */
int tegra_iovmm_vm_insert_pfn(struct tegra_iovmm_area *area,
	tegra_iovmm_addr_t vaddr, unsigned long pfn);

static inline int tegra_iovmm_vm_insert_pages(struct tegra_iovmm_area *area,
					      dma_addr_t va,
					      struct page **pages, size_t count)
{
	return -ENODEV;
}

/*
 * called by clients to return the iovmm_area containing addr, or NULL if
 * addr has not been allocated. caller should call tegra_iovmm_area_put when
 * finished using the returned pointer
 */
struct tegra_iovmm_area *tegra_iovmm_find_area_get(
	struct tegra_iovmm_client *client, tegra_iovmm_addr_t addr);

struct tegra_iovmm_area *tegra_iovmm_area_get(struct tegra_iovmm_area *vm);
void tegra_iovmm_area_put(struct tegra_iovmm_area *vm);

/* called by drivers to initialize a tegra_iovmm_domain structure */
int tegra_iovmm_domain_init(struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_device *dev, tegra_iovmm_addr_t start,
	tegra_iovmm_addr_t end);

/* called by drivers to register an I/O VMM device with the system */
int tegra_iovmm_register(struct tegra_iovmm_device *dev);

/* called by drivers to remove an I/O VMM device from the system */
int tegra_iovmm_unregister(struct tegra_iovmm_device *dev);

#else /* CONFIG_TEGRA_IOVMM */

static inline struct tegra_iovmm_client *tegra_iovmm_alloc_client(
	struct device *dev, const char *share_group,
	struct miscdevice *misc_dev)
{
	return NULL;
}

static inline size_t tegra_iovmm_get_vm_size(struct tegra_iovmm_client *client)
{
	return 0;
}

static inline void tegra_iovmm_free_client(struct tegra_iovmm_client *client)
{
}

static inline int tegra_iovmm_client_lock(struct tegra_iovmm_client *client)
{
	return 0;
}

static inline int tegra_iovmm_client_trylock(struct tegra_iovmm_client *client)
{
	return 0;
}

static inline void tegra_iovmm_client_unlock(struct tegra_iovmm_client *client)
{
}

static inline struct tegra_iovmm_area *tegra_iovmm_create_vm(
	struct tegra_iovmm_client *client, struct tegra_iovmm_area_ops *ops,
	size_t size, size_t align, pgprot_t pgprot, unsigned long iovm_start)
{
	return NULL;
}

static inline void tegra_iovmm_zap_vm(struct tegra_iovmm_area *vm)
{
}

static inline void tegra_iovmm_unzap_vm(struct tegra_iovmm_area *vm)
{
}

static inline void tegra_iovmm_free_vm(struct tegra_iovmm_area *vm)
{
}

static inline size_t tegra_iovmm_get_max_free(struct tegra_iovmm_client *client)
{
	return 0;
}

static inline int tegra_iovmm_vm_insert_pfn(struct tegra_iovmm_area *area,
	tegra_iovmm_addr_t vaddr, unsigned long pfn)
{
	return 0;
}

static inline struct tegra_iovmm_area *tegra_iovmm_find_area_get(
	struct tegra_iovmm_client *client, tegra_iovmm_addr_t addr)
{
	return NULL;
}

static inline struct tegra_iovmm_area *tegra_iovmm_area_get(
	struct tegra_iovmm_area *vm)
{
	return NULL;
}

static inline void tegra_iovmm_area_put(struct tegra_iovmm_area *vm)
{
}

static inline int tegra_iovmm_domain_init(struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_device *dev, tegra_iovmm_addr_t start,
	tegra_iovmm_addr_t end)
{
	return 0;
}

static inline int tegra_iovmm_register(struct tegra_iovmm_device *dev)
{
	return 0;
}

static inline int tegra_iovmm_unregister(struct tegra_iovmm_device *dev)
{
	return 0;
}

static inline int tegra_iovmm_suspend(void)
{
	return 0;
}

static inline void tegra_iovmm_resume(void)
{
}

#ifdef CONFIG_IOMMU_API
/*
 * Replace tegra_iovmm_*() with tegra_iommu_*() helpers
 */
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>

#include <asm/dma-iommu.h>

#define tegra_iovmm_alloc_client(d, s, m)	tegra_iommu_alloc_client(d)
#define tegra_iovmm_free_client(c)		tegra_iommu_free_client(c)

#define tegra_iovmm_create_vm(c, o, s, a, p, i)		\
	tegra_iommu_create_vm((c)->dev, i, s, p)
#define tegra_iovmm_free_vm(v)	tegra_iommu_free_vm(v)

#define tegra_iovmm_zap_vm(v)	tegra_iommu_zap_vm(v)

#define tegra_iovmm_get_vm_size(c)	dma_iova_get_free_total((c)->dev)
#define tegra_iovmm_get_max_free(c)	dma_iova_get_free_max((c)->dev)

#define tegra_iovmm_vm_insert_pfn(area, handle, pfn)			\
	({								\
		dma_addr_t da;						\
		struct device *dev = area->dev;				\
		struct dma_map_ops *ops = get_dma_ops(dev);		\
		DEFINE_DMA_ATTRS(attrs);				\
		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);		\
		da = ops->map_page_at(dev, pfn_to_page(pfn), handle,	\
				 PAGE_SIZE, 0, 0, &attrs);		\
		dma_mapping_error(dev, da) ? -ENOMEM : 0;		\
	})

static inline int tegra_iovmm_vm_insert_pages(struct tegra_iovmm_area *area,
					      dma_addr_t va,
					      struct page **pages, size_t count)
{
	dma_addr_t da;
	struct device *dev = area->dev;
	struct dma_map_ops *ops = get_dma_ops(dev);
	DEFINE_DMA_ATTRS(attrs);

	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	da = ops->map_pages(dev, pages, va, count, 0, &attrs);
	return dma_mapping_error(dev, da) ? -ENOMEM : 0;
}

struct tegra_iovmm_area *tegra_iommu_create_vm(struct device *dev,
		       dma_addr_t req, size_t size, pgprot_t prot);

void tegra_iommu_free_vm(struct tegra_iovmm_area *area);

void tegra_iommu_zap_vm(struct tegra_iovmm_area *area);

struct tegra_iovmm_client *tegra_iommu_alloc_client(struct device *dev);

void tegra_iommu_free_client(struct tegra_iovmm_client *client);

#endif /* CONFIG_IOMMU_API */
#endif /* CONFIG_TEGRA_IOVMM */
#endif /* _MACH_TEGRA_IOVMM_H_*/
