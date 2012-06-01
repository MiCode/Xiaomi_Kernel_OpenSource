/** MobiCore driver module.(interface to the secure world SWD)
 * @addtogroup MCD_MCDIMPL_KMOD_IMPL
 * @{
 * @file
 * MobiCore Driver Kernel Module.
 * This module is written as a Linux device driver.
 * This driver represents the command proxy on the lowest layer, from the
 * secure world to the non secure world, and vice versa.
 * This driver is located in the non secure world (Linux).
 * This driver offers IOCTL commands, for access to the secure world, and has
 * the interface from the secure world to the normal world.
 * The access to the driver is possible with a file descriptor,
 * which has to be created by the fd = open(/dev/mobicore) command.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "mc_drv_module.h"
#include "mc_drv_module_linux_api.h"
#include "mc_drv_module_android.h"
#include "mc_drv_module_fastcalls.h"
#include "public/mc_kernel_api.h"

/* Initial value for the daemon sempahore signaling */
#define DAEMON_SEM_VAL 0

/** MobiCore interrupt context data */
static struct mc_drv_kmod_ctx	mc_drv_kmod_ctx;

/** MobiCore MCI information */
static uint32_t mci_base;
/*
#############################################################################
##
## Convenience functions for Linux API functions
##
#############################################################################*/
static int goto_cpu0(void);
static int goto_all_cpu(void) __attribute__ ((unused));


/*----------------------------------------------------------------------------*/
static void init_and_add_to_list(
	struct list_head *item,
	struct list_head *list_head
)
{
	INIT_LIST_HEAD(item);

	list_add(item, list_head);
}

/*----------------------------------------------------------------------------*/
/** check if CPU supports the ARM TrustZone Security Extensions
 *	@return int TRUE or FALSE */
static int has_security_extensions(
	void
)
{
	u32 fea = 0;
	asm volatile(
		"mrc p15, 0, %[fea], cr0, cr1, 0" :
		[fea]"=r" (fea));

	MCDRV_DBG_VERBOSE("CPU Features: 0x%X", fea);

	/* If the CPU features ID has 0 for security features then the CPU
	 * doesn't support TrustZone at all!
	 */
	if ((fea & ARM_SECURITY_EXTENSION_MASK) == 0)
		return 0;

	return 1;
}

/*----------------------------------------------------------------------------*/
/** check if running in secure mode
 *	@return int TRUE or FALSE */
static int is_secure_mode(
	void
)
{
	u32 cpsr = 0, nsacr = 0;
	asm volatile(
		"mrc	p15, 0, %[nsacr], cr1, cr1, 2\n"
		"mrs %[cpsr], cpsr\n" :
		[nsacr]"=r" (nsacr),
		[cpsr]"=r"(cpsr));

	MCDRV_DBG_VERBOSE("CPRS.M = set to 0x%X\n", cpsr & ARM_CPSR_MASK);
	MCDRV_DBG_VERBOSE("SCR.NS = set to 0x%X\n", nsacr);

	/* If the NSACR contains the reset value(=0) then most likely we are
	 * running in Secure MODE.
	 * If the cpsr mode is set to monitor mode then we cannot load!
	 */
	if (nsacr == 0 || ((cpsr & ARM_CPSR_MASK) == ARM_MONITOR_MODE))
		return 1;

	return 0;
}

/*----------------------------------------------------------------------------*/
/** check if userland caller is privileged (aka has "root" access rights).
	@return int TRUE or FALSE */
static int is_userland_caller_privileged(
	void
) {
	/* For some platforms we cannot run the Daemon as root - for Android
	 * compliance tests it is not allowed, thus we assume the daemon is ran
	 * as the system user.
	 * In Android the system user for daemons has no particular capabilities
	 * other than a fixed UID: AID_SYSTEM 1000
	 * The actual number is guaranteed to be the same in all Android systems
	 * so we will take it for granted: see android_filesystem_config.h in
	 * the Android source tree for all UIDs and their meaning:
	 * http://android-dls.com/wiki/index.php?title=Android_UIDs_and_GIDs
	 */
#ifdef MC_ANDROID_UID_CHECK
	return current_euid() <= AID_SYSTEM;
#else
	/* capable should cover all possibilities, root or sudo, uid checking
	 * was not very reliable */
	return capable(CAP_SYS_ADMIN);
#endif
}



/*----------------------------------------------------------------------------*/
static void unlock_page_from_used_l2_table(
	struct page *page
){
	/* REV axh: check if we should do this. */
	SetPageDirty(page);

	/* release page, old api was page_cache_release() */
	ClearPageReserved(page);
	put_page(page);
}

/*----------------------------------------------------------------------------*/
/* convert L2 PTE to page pointer */
static struct page *l2_pte_to_page(
	pte_t pte
) {
	void *phys_page_addr	= (void *)((unsigned int)pte & PAGE_MASK);
	unsigned int pfn	= addr_to_pfn(phys_page_addr);
	struct page *page	= pfn_to_page(pfn);
	return page;
}

/*----------------------------------------------------------------------------*/
/* convert page pointer to L2 PTE */
static pte_t page_to_l2_pte(
	struct page *page
)
{
	unsigned int pfn	= page_to_pfn(page);
	void *phys_addr		= pfn_to_addr(pfn);
	pte_t pte		= (pte_t)((unsigned int)phys_addr & PAGE_MASK);
	return pte;
}


/*----------------------------------------------------------------------------*/
static inline int lock_user_pages(
	struct task_struct	*task,
	void			*virt_start_page_addr,
	int			nr_of_pages,
	struct page		**pages
)
{
	int		ret = 0;
	int		locked_pages = 0;
	unsigned int	i;

	do {

		/* lock user pages, must hold the mmap_sem to do this. */
		down_read(&(task->mm->mmap_sem));
		locked_pages = get_user_pages(
					  task,
					  task->mm,
					  (unsigned long)virt_start_page_addr,
					  nr_of_pages,
					  1, /* write access */
					  0, /* they say drivers should always
						pass 0 here..... */
					  pages,
					  NULL); /* we don't need the VMAs */
		up_read(&(task->mm->mmap_sem));

		/* could as lock all pages? */
		if (locked_pages != nr_of_pages) {
			MCDRV_DBG_ERROR(
				"get_user_pages() failed, "
				"locked_pages=%d\n",
				locked_pages);
			ret = -ENOMEM;
			/* check if an error has been returned. */
			if (locked_pages < 0) {
				ret = locked_pages;
				locked_pages = 0;
			}
			break;
		}

		/* do cache maintenance on locked pages. */
		for (i = 0; i < nr_of_pages; i++)
			flush_dcache_page(pages[i]);

	} while (FALSE);


	if (ret != 0) {
		/* release all locked pages. */
		MCDRV_ASSERT(locked_pages >= 0);
		for (i = 0; i < locked_pages; i++)
			put_page(pages[i]);
	}

	return ret;

}

/*
#############################################################################
##
## Driver implementation functions
##
#############################################################################*/
/*----------------------------------------------------------------------------*/
/* check if caller is MobiCore Daemon */
static unsigned int is_caller_mc_daemon(
	struct mc_instance *instance
)
{
	return ((instance != NULL)
		&& (mc_drv_kmod_ctx.daemon_inst == instance));
}


/*----------------------------------------------------------------------------*/
/* Get process context from file pointer */
static struct mc_instance *get_instance(
	struct file *file
) {
	MCDRV_ASSERT(file != NULL);

	return (struct mc_instance *)(file->private_data);
}


/*----------------------------------------------------------------------------*/
/* Get a unique ID */
static unsigned int get_mc_kmod_unique_id(
	void
)
{
	return (unsigned int)atomic_inc_return(
			   &(mc_drv_kmod_ctx.unique_counter));
}


/*----------------------------------------------------------------------------*/
/* Get kernel pointer to shared L2 table given a per-process reference */
static struct l2table *get_l2_table_kernel_virt(
	struct mc_used_l2_table	*used_l2table
)
{
	MCDRV_ASSERT(used_l2table != NULL);
	MCDRV_ASSERT(used_l2table->set != NULL);
	MCDRV_ASSERT(used_l2table->set->kernel_virt != NULL);
	return &(used_l2table->set->kernel_virt->table[used_l2table->idx]);
}

/*----------------------------------------------------------------------------*/
/* Get physical address of a shared L2 table given a per-process reference */
static struct l2table *get_l2_table_phys(
	struct mc_used_l2_table  *used_l2table
)
{
	MCDRV_ASSERT(used_l2table != NULL);
	MCDRV_ASSERT(used_l2table->set != NULL);
	MCDRV_ASSERT(used_l2table->set->phys != NULL);
	return &(used_l2table->set->phys->table[used_l2table->idx]);
}

/*----------------------------------------------------------------------------*/
static unsigned int is_in_use_used_l2_table(
	struct mc_used_l2_table	*used_l2table
)
{
	return ((used_l2table->flags &
			  (MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_APP
			   | MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_MC)) != 0);
}



/*----------------------------------------------------------------------------*/
static struct mc_used_l2_table *find_used_l2_table_by_handle(
	unsigned int	handle
) {
	struct mc_used_l2_table  *used_l2table;
	struct mc_used_l2_table  *used_l2table_with_handle = NULL;

	list_for_each_entry(
		used_l2table,
		&(mc_drv_kmod_ctx.mc_used_l2_tables),
		list
	) {
		if (handle == used_l2table->handle) {
			used_l2table_with_handle = used_l2table;
			break;
		}
	}

	return used_l2table_with_handle;
}

/*
#############################################################################
##
## L2 Table Pool
##
#############################################################################*/

/*----------------------------------------------------------------------------*/
static struct mc_used_l2_table *allocate_used_l2_table(
	struct mc_instance	*instance
) {
	int				ret = 0;
	struct mc_l2_table_store	*l2table_store = NULL;
	struct mc_l2_tables_set		*l2table_set = NULL;
	struct mc_used_l2_table		*used_l2table = NULL;
	struct page			*page;
	unsigned int			i = 0;

	do {
		/* allocate a WSM L2 descriptor */
		used_l2table  = kmalloc(sizeof(*used_l2table), GFP_KERNEL);
		if (used_l2table == NULL) {
			ret = -ENOMEM;
			MCDRV_DBG_ERROR("out of memory\n");
			break;
		}
		/* clean */
		memset(used_l2table, 0, sizeof(*used_l2table));
		used_l2table->handle = get_mc_kmod_unique_id();
		used_l2table->owner = instance;

		/* add to global list. */
		init_and_add_to_list(
			&(used_l2table->list),
			&(mc_drv_kmod_ctx.mc_used_l2_tables));

		/* walk though list to find free set. */
		list_for_each_entry(
			l2table_set,
			&(mc_drv_kmod_ctx.mc_l2_tables_sets),
			list
		) {
			for (i = 0; i < MC_DRV_KMOD_L2_TABLE_PER_PAGES; i++) {
				if ((l2table_set->usage_bitmap & (1U << i))
					== 0) {
					/* found a set,
						l2table_set and i are set. */
					l2table_store =
						l2table_set->kernel_virt;
					break;
				}
			}
			if (l2table_store != NULL)
				break;
		} /* end while */

		if (l2table_store == NULL) {
			l2table_store = (struct mc_l2_table_store *)
						get_zeroed_page(GFP_KERNEL);
			if (l2table_store == NULL) {
				ret = -ENOMEM;
				break;
			}

			/* Actually, locking is not necessary, because kernel
				memory is not supposed to get swapped out. But
				we play safe.... */
			page = virt_to_page(l2table_store);
			SetPageReserved(page);

			/* allocate a descriptor */
			l2table_set = kmalloc(sizeof(*l2table_set), GFP_KERNEL);
			if (l2table_set == NULL) {
				kfree(l2table_store);
				ret = -ENOMEM;
				break;
			}
			/* initialize */
			memset(l2table_set, 0, sizeof(*l2table_set));

			l2table_set->kernel_virt = l2table_store;
			l2table_set->page = page;
			l2table_set->phys = (void *)virt_to_phys(l2table_store);

			/* init add to list. */
			init_and_add_to_list(
				&(l2table_set->list),
				&(mc_drv_kmod_ctx.mc_l2_tables_sets));

			/* use first table */
			i = 0;
		}

		/* set set usage */
		l2table_set->usage_bitmap |= (1U << i);

		/* set set reference */
		used_l2table->set = l2table_set;
		used_l2table->idx = i;

		MCDRV_DBG_VERBOSE(
			"chunkPhys=%p,idx=%d\n",
			l2table_set->phys, i);

	} while (FALSE);

	if (ret != 0) {
		if (used_l2table != NULL) {
			/* remove from list */
			list_del(&(l2table_set->list));
			/* free memory */
			kfree(used_l2table);
			used_l2table = NULL;
		}
	}

	return used_l2table;
}

/*----------------------------------------------------------------------------*/
static void free_used_l2_table(
	struct mc_used_l2_table *used_l2table
)
{
	struct mc_l2_tables_set	*l2table_set;
	unsigned int		idx;

	MCDRV_ASSERT(used_l2table != NULL);

	l2table_set = used_l2table->set;
	MCDRV_ASSERT(l2table_set != NULL);

	/* clean usage flag */
	idx = used_l2table->idx;
	MCDRV_ASSERT(idx < MC_DRV_KMOD_L2_TABLE_PER_PAGES);
	l2table_set->usage_bitmap &= ~(1U << idx);

	/* if nobody uses this set, we can release it. */
	if (l2table_set->usage_bitmap == 0) {
		MCDRV_ASSERT(l2table_set->page != NULL);
		ClearPageReserved(l2table_set->page);

		MCDRV_ASSERT(l2table_set->kernel_virt != NULL);
		free_page((unsigned long)l2table_set->kernel_virt);

		/* remove from list */
		list_del(&(l2table_set->list));

		/* free memory */
		kfree(l2table_set);
	}

	return;
}



/*----------------------------------------------------------------------------*/
/**
 * Create a L2 table in a WSM container that has been allocates previously.
 *
 * @param task		pointer to task owning WSM
 * @param wsm_buffer	user space WSM start
 * @param wsm_len	WSM length
 * @param used_l2table	Pointer to L2 table details
 */
static int map_buffer_into_used_l2_table(
	struct task_struct	*task,
	void			*wsm_buffer,
	unsigned int		wsm_len,
	struct mc_used_l2_table	*used_l2table
)
{
	int		ret = 0;
	unsigned int	i, nr_of_pages;
	void		*virt_addr_page;
	struct page	*page;
	struct l2table	*l2table;
	struct page	**l2table_as_array_of_pointers_to_page;

	/* task can be null when called from kernel space */
	MCDRV_ASSERT(wsm_buffer != NULL);
	MCDRV_ASSERT(wsm_len != 0);
	MCDRV_ASSERT(used_l2table != NULL);

	MCDRV_DBG_VERBOSE("WSM addr=0x%p, len=0x%08x\n", wsm_buffer, wsm_len);

	/* Check if called from kernel space wsm_buffer is actually
	 * vmalloced or not */
	if (task == NULL && !is_vmalloc_addr(wsm_buffer)) {
		MCDRV_DBG_ERROR("WSM addr is not a vmalloc address");
		return -EINVAL;
	}

	l2table = get_l2_table_kernel_virt(used_l2table);
	/* We use the memory for the L2 table to hold the pointer
	and convert them later. This works, as everything comes
	down to a 32 bit value. */
	l2table_as_array_of_pointers_to_page = (struct page **)l2table;

	do {

		/* no size > 1Mib supported */
		if (wsm_len > SZ_1M) {
			MCDRV_DBG_ERROR("size > 1 MiB\n");
			ret = -EINVAL;
			break;
		}

		/* calculate page usage */
		virt_addr_page = get_page_start(wsm_buffer);
		nr_of_pages  = get_nr_of_pages_for_buffer(wsm_buffer, wsm_len);


		MCDRV_DBG_VERBOSE("virt addr pageStart=0x%p,pages=%d\n",
				  virt_addr_page,
				  nr_of_pages);

		/* L2 table can hold max 1MiB in 256 pages. */
		if ((nr_of_pages*PAGE_SIZE) > SZ_1M) {
			MCDRV_DBG_ERROR("WSM paged exceed 1 MiB\n");
			ret = -EINVAL;
			break;
		}

		/* Request comes from user space */
		if (task != NULL) {
			/* lock user page in memory, so they do not get swapped
			* out.
			* REV axh:
			* Kernel 2.6.27 added a new get_user_pages_fast()
			* function, maybe it is called fast_gup() in some
			* versions.
			* handle user process doing a fork().
			* Child should not get things.
			* http://osdir.com/ml/linux-media/2009-07/msg00813.html
			* http://lwn.net/Articles/275808/ */

			ret = lock_user_pages(
					  task,
					  virt_addr_page,
					  nr_of_pages,
					  l2table_as_array_of_pointers_to_page);
			if (ret != 0) {
				MCDRV_DBG_ERROR("lock_user_pages() failed\n");
				break;
			}
		}
		/* Request comes from kernel space(vmalloc buffer) */
		else {
			void *uaddr = wsm_buffer;
			for (i = 0; i < nr_of_pages; i++) {
				page = vmalloc_to_page(uaddr);
				if (!page) {
					MCDRV_DBG_ERROR(
						"vmalloc_to_Page()"
						" failed to map address\n");
					ret = -EINVAL;
					break;
				}
				get_page(page);
				/* Lock the page in memory, it can't be swapped
				 * out */
				SetPageReserved(page);
				l2table_as_array_of_pointers_to_page[i] = page;
				uaddr += PAGE_SIZE;
			}
		}

		used_l2table->nr_of_pages = nr_of_pages;
		used_l2table->flags |= MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_APP;

		/* create L2 Table entries. used_l2table->table contains a list
		of page pointers here. For a proper cleanup we have to ensure
		that the following code either works and used_l2table contains
		a valid L2 table - or fails and used_l2table->table contains the
		list of page pointers. Any mixed contents will make cleanup
		difficult.*/

		for (i = 0; i < nr_of_pages; i++) {
			pte_t pte;
			page = l2table_as_array_of_pointers_to_page[i];

			/* create L2 table entry, see ARM MMU docu for details
			about flags stored in the lowest 12 bits. As a side
			reference, the Article "ARM's multiply-mapped memory
			mess" found in the collection at at
			http://lwn.net/Articles/409032/ is also worth reading.*/
			pte = page_to_l2_pte(page)
					| L2_FLAG_AP1 | L2_FLAG_AP0
					| L2_FLAG_C | L2_FLAG_B
					| L2_FLAG_SMALL | L2_FLAG_SMALL_XN
			/* Linux uses different mappings for SMP systems(the
			 * sharing flag is set for the pte. In order not to
			 * confuse things too much in Mobicore make sure the
			 * shared buffers have the same flags.
			 * This should also be done in SWD side
			 */
#ifdef CONFIG_SMP
					| L2_FLAG_S | L2_FLAG_SMALL_TEX0
#endif
				  ;

			l2table->table_entries[i] = pte;
			MCDRV_DBG_VERBOSE("L2 entry %d:  0x%08x\n", i,
					  (unsigned int)(pte));
		}

		/* ensure rest of table is empty */
		while (i < 255)
			l2table->table_entries[i++] = (pte_t)0;

	} while (FALSE);

	return ret;
}


/*----------------------------------------------------------------------------*/
/**
 * Remove a L2 table in a WSM container. Afterwards the container may be
 * released.
 *
 * @param used_l2table	Pointer to L2 table details
 */

static void unmap_buffers_from_used_l2_table(
	struct mc_used_l2_table	*used_l2table
)
{
	unsigned int	i;
	struct l2table	*l2table;

	MCDRV_ASSERT(used_l2table != NULL);
	/* this should not happen, as we have no empty tables. */
	MCDRV_ASSERT(!is_in_use_used_l2_table(used_l2table));

	/* found the table, now release the resources. */
	MCDRV_DBG_VERBOSE("clear L2 table, phys_base=%p, nr_of_pages=%d\n",
			  get_l2_table_phys(used_l2table),
			  used_l2table->nr_of_pages);

	l2table = get_l2_table_kernel_virt(used_l2table);

	/* release all locked user space pages */
	for (i = 0; i < used_l2table->nr_of_pages; i++) {
		/* convert physical entries from L2 table to page pointers */
		pte_t pte = get_l2_table_kernel_virt(used_l2table)->
							table_entries[i];
		struct page *page = l2_pte_to_page(pte);
		unlock_page_from_used_l2_table(page);
	}

	/* remember that all pages have been freed */
	used_l2table->nr_of_pages = 0;

	return;
}


/*
#############################################################################
##
## Helper functions
##
#############################################################################*/
/*----------------------------------------------------------------------------*/
#define FREE_FROM_SWD	TRUE
#define FREE_FROM_NWD	FALSE
/** Delete a used l2 table. */
static void delete_used_l2_table(
	struct mc_used_l2_table	*used_l2table,
	unsigned int		is_swd
)
{
	if (is_swd) {
		used_l2table->flags &=
			~MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_MC;
	} else {
		used_l2table->flags &=
			~MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_APP;
		used_l2table->owner = NULL;
	}

	/* release if Nwd and Swd/MC do no longer use it. */
	if (is_in_use_used_l2_table(used_l2table)) {
		MCDRV_DBG_WARN(
			"WSM L2 table still in use: physBase=%p, "
			"nr_of_pages=%d\n",
			get_l2_table_phys(used_l2table),
			used_l2table->nr_of_pages);
	} else {
		unmap_buffers_from_used_l2_table(used_l2table);
		free_used_l2_table(used_l2table);

		list_del(&(used_l2table->list));

		kfree(used_l2table);
	}
	return;
}

/*----------------------------------------------------------------------------*/
/** Allocate L2 table and map buffer into it. That is, create respective table
	entries. Must hold Semaphore mc_drv_kmod_ctx.wsm_l2_sem */
static struct mc_used_l2_table *new_used_l2_table(
	struct mc_instance	*instance,
	struct task_struct	*task,
	void			*wsm_buffer,
	unsigned int		wsm_len
) {
	int			ret = 0;
	struct mc_used_l2_table	*used_l2table;

	do {
		used_l2table = allocate_used_l2_table(instance);
		if (used_l2table == NULL) {
			MCDRV_DBG_ERROR(
				"allocate_used_l2_table() failed\n");
			break;
		}

		/* create the L2 page for the WSM */
		ret = map_buffer_into_used_l2_table(
				  task,
				  wsm_buffer,
				  wsm_len,
				  used_l2table);
		if (ret != 0) {
			MCDRV_DBG_ERROR(
				"map_buffer_into_used_l2_table() failed\n");
			delete_used_l2_table(used_l2table, FREE_FROM_NWD);
			used_l2table = NULL;
			break;
		}

	} while (FALSE);


	return used_l2table;
}

/*
#############################################################################
##
## IoCtl handler
##
#############################################################################*/

/**
 * Map a virtual memory buffer structure to Mobicore
 * @param instance
 * @param addr		address of the buffer(NB it must be kernel virtual!)
 * @param len		buffer length
 * @param handle	pointer to handle
 * @param phys_wsm_l2_table	pointer to physical L2 table(?)
 *
 * @return 0 if no error
 *
 */
/*----------------------------------------------------------------------------*/
int mobicore_map_vmem(
	struct mc_instance	*instance,
	void			*addr,
	uint32_t		len,
	uint32_t		*handle,
	void			**phys_wsm_l2_table
)
{
	int ret = 0;
	struct mc_used_l2_table *used_l2table = NULL;
	MCDRV_ASSERT(instance != NULL);

	MCDRV_DBG_VERBOSE("enter\n");

	do {
		if (len == 0) {
			MCDRV_DBG_ERROR("len=0 is not supported!\n");
			ret = -EINVAL;
			break;
		}

		/* try to get the semaphore */
		ret = down_interruptible(&(mc_drv_kmod_ctx.wsm_l2_sem));
		if (ret != 0) {
			MCDRV_DBG_ERROR("down_interruptible() failed with %d\n",
					ret);
			ret = -ERESTARTSYS;
			break;
		}

		do {
			used_l2table = new_used_l2_table(
						   instance,
						   NULL,
						   addr,
						   len);

			if (used_l2table == NULL) {
				MCDRV_DBG_ERROR("new_used_l2_table() failed\n");
				ret = -EINVAL;
				break;
			}

			/* set response */
			*handle = used_l2table->handle;
			*phys_wsm_l2_table =
				(void *)get_l2_table_phys(used_l2table);
			MCDRV_DBG_VERBOSE("handle: %d, phys=%p\n",
					  *handle,
					  (void *)(*phys_wsm_l2_table));

		} while (FALSE);

		/* release semaphore */
		up(&(mc_drv_kmod_ctx.wsm_l2_sem));

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}
EXPORT_SYMBOL(mobicore_map_vmem);
/*----------------------------------------------------------------------------*/
/**
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int handle_ioctl_app_register_wsm_l2(
	struct mc_instance			*instance,
	union mc_ioctl_app_reg_wsm_l2_params	*user_params
)
{
	int					ret = 0;
	union mc_ioctl_app_reg_wsm_l2_params	params;
	struct mc_used_l2_table			*used_l2table = NULL;
	struct pid				*pid_struct = NULL;
	struct task_struct			*task = current;

	MCDRV_ASSERT(instance != NULL);

	MCDRV_DBG_VERBOSE("enter\n");

	do {
		/* get use parameters */
		ret = copy_from_user(
				  &(params.in),
				  &(user_params->in),
				  sizeof(params.in));
		if (ret != 0) {
			MCDRV_DBG_ERROR("copy_from_user() failed\n");
			break;
		}

		/* daemon can do this for another task. */
		if (params.in.pid != 0) {
			MCDRV_DBG_ERROR("pid != 0 unsupported\n");
			ret = -EINVAL;
			break;
		}
		if (params.in.len == 0) {
			MCDRV_DBG_ERROR("len=0 is not supported!\n");
			ret = -EINVAL;
			break;
		}

		/* try to get the semaphore */
		ret = down_interruptible(&(mc_drv_kmod_ctx.wsm_l2_sem));
		if (ret != 0) {
			MCDRV_DBG_ERROR("down_interruptible() failed with %d\n",
					ret);
			ret = -ERESTARTSYS;
			break;
		}

		do {
			used_l2table = new_used_l2_table(
						   instance,
						   task,
						   (void *)(params.in.buffer),
						   params.in.len);

			if (used_l2table == NULL) {
				MCDRV_DBG_ERROR("new_used_l2_table() failed\n");
				ret = -EINVAL;
				break;
			}

			/* if the daemon does this, we set the MC lock */
			if (is_caller_mc_daemon(instance))
				used_l2table->flags |=
					MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_MC;

			/* set response */
			memset(&params.out, 0, sizeof(params.out));
			params.out.handle = used_l2table->handle;
			/* TODO: return the physical address for daemon only,
				otherwise set NULL */
			params.out.phys_wsm_l2_table =
				(uint32_t)get_l2_table_phys(used_l2table);

			MCDRV_DBG_VERBOSE("handle: %d, phys=%p\n",
					params.out.handle,
					(void *)(params.out.phys_wsm_l2_table));


			/* copy L2Table to user space */
			ret = copy_to_user(
					  &(user_params->out),
					  &(params.out),
					  sizeof(params.out));
			if (ret != 0) {
				MCDRV_DBG_ERROR("copy_to_user() failed\n");

				/* free the table again, as app does not know
					about anything. */
				if (is_caller_mc_daemon(instance)) {
					used_l2table->flags &=
					~MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_MC;
				}
				delete_used_l2_table(used_l2table,
						FREE_FROM_NWD);
				used_l2table = NULL;
				break;
			}

		} while (FALSE);

		/* release semaphore */
		up(&(mc_drv_kmod_ctx.wsm_l2_sem));

	} while (FALSE);



	/* release PID struct reference */
	if (pid_struct != NULL)
		put_pid(pid_struct);


	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}


/*----------------------------------------------------------------------------*/
/**
 * Unmap a virtual memory buffer from mobicore
 * @param instance
 * @param handle
 *
 * @return 0 if no error
 *
 */
int mobicore_unmap_vmem(
	struct mc_instance	*instance,
	uint32_t		handle
)
{
	int ret = 0;
	struct mc_used_l2_table *used_l2table = NULL;

	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		/* try to get the semaphore */
		ret = down_interruptible(&(mc_drv_kmod_ctx.wsm_l2_sem));
		if (ret != 0) {
			MCDRV_DBG_ERROR("processOpenSession() failed with %d\n",
					ret);
			ret = -ERESTARTSYS;
			break;
		}

		do {
			used_l2table = find_used_l2_table_by_handle(handle);
			if (used_l2table == NULL) {
				ret = -EINVAL;
				MCDRV_DBG_ERROR("entry not found\n");
				break;
			}

			if (instance != used_l2table->owner) {
				ret = -EINVAL;
				MCDRV_DBG_ERROR("instance does no own it\n");
				break;
			}

			/* free table (if no further locks exist) */
			delete_used_l2_table(used_l2table, FREE_FROM_NWD);
			used_l2table = NULL;
			/* there are no out parameters */
		} while (FALSE);
		/* release semaphore */
		up(&(mc_drv_kmod_ctx.wsm_l2_sem));

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}
EXPORT_SYMBOL(mobicore_unmap_vmem);
/*----------------------------------------------------------------------------*/
/**
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int handle_ioctl_app_unregister_wsm_l2(
	struct mc_instance			*instance,
	struct mc_ioctl_app_unreg_wsm_l2_params	*user_params
)
{
	int					ret = 0;
	struct mc_ioctl_app_unreg_wsm_l2_params	params;
	struct mc_used_l2_table			*used_l2table = NULL;

	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		ret = copy_from_user(
				  &(params.in),
				  &(user_params->in),
				  sizeof(params.in));

		if (ret != 0) {
			MCDRV_DBG_ERROR("copy_from_user\n");
			break;
		}

		/* try to get the semaphore */
		ret = down_interruptible(&(mc_drv_kmod_ctx.wsm_l2_sem));
		if (ret != 0) {
			MCDRV_DBG_ERROR("down_interruptible() failed with %d\n",
					ret);
			ret = -ERESTARTSYS;
			break;
		}

		do {
			/* daemon can do this for another task. */
			if (params.in.pid != 0) {
				MCDRV_DBG_ERROR("pid != 0 unsupported\n");
				ret = -EINVAL;
				break;
			}

			used_l2table =
				find_used_l2_table_by_handle(params.in.handle);
			if (used_l2table == NULL) {
				ret = -EINVAL;
				MCDRV_DBG_ERROR("entry not found\n");
				break;
			}

			if (is_caller_mc_daemon(instance)) {
				/* if daemon does this, we have to release the
					MobiCore lock. */
				used_l2table->flags &=
					~MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_MC;
			} else if (instance != used_l2table->owner) {
				ret = -EINVAL;
				MCDRV_DBG_ERROR("instance does no own it\n");
				break;
			}

			/* free table (if no further locks exist) */
			delete_used_l2_table(used_l2table, FREE_FROM_NWD);
			used_l2table = NULL;

			/* there are no out parameters */

		} while (FALSE);

		/* release semaphore */
		up(&(mc_drv_kmod_ctx.wsm_l2_sem));

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}


/*----------------------------------------------------------------------------*/
static int handle_ioctl_daemon_lock_wsm_l2(
	struct mc_instance				*instance,
	struct mc_ioctl_daemon_lock_wsm_l2_params	*user_params
)
{
	int						ret = 0;
	struct mc_ioctl_daemon_lock_wsm_l2_params	params;
	struct mc_used_l2_table				*used_l2table = NULL;

	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		if (!is_caller_mc_daemon(instance)) {
			MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
			ret = -EFAULT;
			break;
		}

		ret = copy_from_user(
				  &(params.in),
				  &(user_params->in),
				  sizeof(params.in));

		if (ret != 0) {
			MCDRV_DBG_ERROR("copy_from_user\n");
			break;
		}
		/* try to get the semaphore */
		ret = down_interruptible(&(mc_drv_kmod_ctx.wsm_l2_sem));
		if (ret != 0) {
			MCDRV_DBG_ERROR("down_interruptible() failed with %d\n",
					ret);
			ret = -ERESTARTSYS;
			break;
		}

		do {
			used_l2table =
				find_used_l2_table_by_handle(params.in.handle);
			if (used_l2table == NULL) {
				ret = -EINVAL;
				MCDRV_DBG_ERROR("entry not found\n");
				break;
			}
			if (instance != used_l2table->owner) {
				ret = -EINVAL;
				MCDRV_DBG_ERROR("instance does no own it\n");
				break;
			}

			/* lock entry */
			if ((used_l2table->flags &
				  MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_MC) != 0) {
				MCDRV_DBG_WARN("entry already locked\n");
			}
			used_l2table->flags |=
				MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_MC;

			/* prepare response */
			memset(&(params.out), 0, sizeof(params.out));
			params.out.phys_wsm_l2_table =
				(uint32_t)get_l2_table_phys(used_l2table);

			/* copy to user space */
			ret = copy_to_user(
					  &(user_params->out),
					  &(params.out),
					  sizeof(params.out));
			if (ret != 0) {
				MCDRV_DBG_ERROR("copy_to_user() failed\n");

				/* undo, as userspace did not get it. */
				used_l2table->flags |=
					MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_MC;
				break;
			}

		} while (FALSE);

		/* release semaphore */
		up(&(mc_drv_kmod_ctx.wsm_l2_sem));

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}


/*----------------------------------------------------------------------------*/
static int handle_ioctl_daemon_unlock_wsm_l2(
	struct mc_instance				*instance,
	struct mc_ioctl_daemon_unlock_wsm_l2_params	*user_params
)
{
	int						ret = 0;
	struct mc_ioctl_daemon_unlock_wsm_l2_params	params;
	struct mc_used_l2_table				*used_l2table = NULL;

	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		if (!is_caller_mc_daemon(instance)) {
			MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
			ret = -EFAULT;
			break;
		}

		ret = copy_from_user(
				  &(params.in),
				  &(user_params->in),
				  sizeof(params.in));

		if (ret != 0) {
			MCDRV_DBG_ERROR("copy_from_user\n");
			break;
		}
		/* try to get the semaphore */
		ret = down_interruptible(&(mc_drv_kmod_ctx.wsm_l2_sem));
		if (ret != 0) {
			MCDRV_DBG_ERROR("down_interruptible() failed with %d\n",
						ret);
			ret = -ERESTARTSYS;
			break;
		}

		do {
			used_l2table =
				find_used_l2_table_by_handle(params.in.handle);
			if (used_l2table == NULL) {
				ret = -EINVAL;
				MCDRV_DBG_ERROR("entry not found\n");
				break;
			}
			if (instance != used_l2table->owner) {
				ret = -EINVAL;
				MCDRV_DBG_ERROR("instance does no own it\n");
				break;
			}

			/* lock entry */
			if ((used_l2table->flags &
				  MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_MC) == 0) {
				MCDRV_DBG_WARN("entry is not locked locked\n");
			}

			/* free table (if no further locks exist) */
			delete_used_l2_table(used_l2table, FREE_FROM_SWD);
			used_l2table = NULL;

			/* there are no out parameters */

		} while (FALSE);

	} while (FALSE);


	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}

/*----------------------------------------------------------------------------*/
/** Clears the reserved bit of each page and frees the pages */
static inline void free_continguous_pages(
	void		*addr,
	unsigned int	size
)
{
	struct page *page = virt_to_page(addr);
	int i;
	for (i = 0; i < size; i++) {
		MCDRV_DBG_VERBOSE("free page at 0x%p\n", page);
		ClearPageReserved(page);
		page++;
	}
	/* REV luh: see man kmalloc */
	free_pages((unsigned long)addr, size_to_order(size));
}

/*----------------------------------------------------------------------------*/
/**
 * Free a WSM buffer allocated with mobicore_allocate_wsm
 * @param instance
 * @param handle		handle of the buffer
 *
 * @return 0 if no error
 *
 */
int mobicore_free(
	struct mc_instance	*instance,
	uint32_t		handle
)
{
	int ret = 0;
	unsigned int i;
	struct mc_contg_buffer	*contg_buffer;

	do {
		/* search for the given address in the contg_buffers list */
		for (i = 0; i < MC_DRV_KMOD_CONTG_BUFFER_MAX; i++) {
			contg_buffer = &(instance->contg_buffers[i]);
			if (contg_buffer->handle == handle)
				break;
		}
		if (i == MC_DRV_KMOD_CONTG_BUFFER_MAX) {
			MCDRV_DBG_ERROR("contigous buffer not found\n");
			ret = -EFAULT;
			break;
		}

		MCDRV_DBG_VERBOSE("phys_addr=0x%p, virt_addr=0x%p\n",
				contg_buffer->phys_addr,
				contg_buffer->virt_kernel_addr);

		free_continguous_pages(contg_buffer->virt_kernel_addr,
					contg_buffer->num_pages);

		memset(contg_buffer, 0, sizeof(*contg_buffer));

		/* there are no out parameters */

	} while (FALSE);


	return ret;
}
EXPORT_SYMBOL(mobicore_free);
/*----------------------------------------------------------------------------*/

/**
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int handle_ioctl_free(
	struct mc_instance		*instance,
	union mc_ioctl_free_params	*user_params
)
{
	int				ret = 0;
	union mc_ioctl_free_params	params;


	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		ret = copy_from_user(
				  &(params.in),
				  &(user_params->in),
				  sizeof(params.in));

		if (ret != 0) {
			MCDRV_DBG_ERROR("copy_from_user\n");
			break;
		}

		/* daemon can do this for another task. */
		if (params.in.pid != 0) {
			MCDRV_DBG_ERROR("pid != 0 unsupported\n");
			ret = -EINVAL;
			break;
		}

		ret = mobicore_free(instance, params.in.handle);

		/* there are no out parameters */

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;

}


/*----------------------------------------------------------------------------*/
/**
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int handle_ioctl_info(
	struct mc_instance	*instance,
	union mc_ioctl_info_params	*user_params
)
{
	int			ret = 0;
	union mc_ioctl_info_params	params;
	union mc_fc_info		fc_info;


	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		/* only the MobiCore Daemon is allowed to call this function */
		if (!is_caller_mc_daemon(instance)) {
			MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
			ret = -EFAULT;
			break;
		}

		ret = copy_from_user(
				  &(params.in),
				  &(user_params->in),
				  sizeof(params.in));

		if (ret != 0) {
			MCDRV_DBG_ERROR("copy_from_user\n");
			break;
		}


		memset(&fc_info, 0, sizeof(fc_info));
		fc_info.as_in.cmd	   = MC_FC_INFO;
		fc_info.as_in.ext_info_id = params.in.ext_info_id;

		MCDRV_DBG(
			"fc_info in cmd=0x%08x, ext_info_id=0x%08x "
			"rfu=(0x%08x, 0x%08x)\n",
			fc_info.as_in.cmd,
			fc_info.as_in.ext_info_id,
			fc_info.as_in.rfu[0],
			fc_info.as_in.rfu[1]);

		mc_fastcall(&(fc_info.as_generic));

		MCDRV_DBG(
			"fc_info out resp=0x%08x, ret=0x%08x "
			"state=0x%08x, ext_info=0x%08x\n",
			fc_info.as_out.resp,
			fc_info.as_out.ret,
			fc_info.as_out.state,
			fc_info.as_out.ext_info);

		ret = convert_fc_ret(fc_info.as_out.ret);
		if (ret != 0)
			break;

		memset(&(params.out), 0, sizeof(params.out));
		params.out.state  = fc_info.as_out.state;
		params.out.ext_info = fc_info.as_out.ext_info;

		ret = copy_to_user(
				  &(user_params->out),
				  &(params.out),
				  sizeof(params.out));

		if (ret != 0) {
			MCDRV_DBG_ERROR("copy_to_user\n");
			break;
		}
	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}

/*----------------------------------------------------------------------------*/
/**
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int handle_ioctl_yield(
	struct mc_instance	*instance
)
{
	int			ret = 0;
	union mc_fc_s_yield	fc_s_yield;

	MCDRV_ASSERT(instance != NULL);

	/* avoid putting debug output here, as we do this very often */
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		/* only the MobiCore Daemon is allowed to call this function */
		if (!is_caller_mc_daemon(instance)) {
			MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
			ret = -EFAULT;
			break;
		}

		memset(&fc_s_yield, 0, sizeof(fc_s_yield));
		fc_s_yield.as_in.cmd = MC_SMC_N_YIELD;
		mc_fastcall(&(fc_s_yield.as_generic));
		ret = convert_fc_ret(fc_s_yield.as_out.ret);
		if (ret != 0)
			break;

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}

/*----------------------------------------------------------------------------*/
/**
 * handle ioctl and call common notify
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int handle_ioctl_nsiq(
	struct mc_instance	*instance,
	unsigned long		arg
)
{
	int		ret = 0;

	MCDRV_ASSERT(instance != NULL);

	/* avoid putting debug output here, as we do this very often */
	MCDRV_DBG_VERBOSE("enter\n");
	/* only the MobiCore Daemon is allowed to call this function */
	if (!is_caller_mc_daemon(instance)) {
		MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
		return -EFAULT;
	}

	do {
		union mc_fc_nsiq fc_nsiq;
		memset(&fc_nsiq, 0, sizeof(fc_nsiq));
		fc_nsiq.as_in.cmd = MC_SMC_N_SIQ;
		mc_fastcall(&(fc_nsiq.as_generic));
		ret = convert_fc_ret(fc_nsiq.as_out.ret);
		if (ret != 0)
			break;
	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}

/*----------------------------------------------------------------------------*/
/**
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int handle_ioctl_dump_status(
	struct mc_instance	*instance,
	unsigned long		arg
)
{
	int		ret = 0;
	int		i = 0;
	union mc_fc_info	fc_info;

	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		/* anybody with root access can do this. */
		if (!is_userland_caller_privileged()) {
			MCDRV_DBG_ERROR("caller must have root privileges\n");
			ret = -EFAULT;
			break;
		}

		/* loop ext_info */
		while (TRUE) {
			memset(&fc_info, 0, sizeof(fc_info));
			fc_info.as_in.cmd	   = MC_FC_INFO;
			fc_info.as_in.ext_info_id = i;

			MCDRV_DBG(
				"fc_info in cmd=0x%08x, ext_info_id=0x%08x "
				"rfu=(0x%08x, 0x%08x)\n",
				fc_info.as_in.cmd,
				fc_info.as_in.ext_info_id,
				fc_info.as_in.rfu[0],
				fc_info.as_in.rfu[1]);

			mc_fastcall(&(fc_info.as_generic));

			MCDRV_DBG(
				"fc_info out resp=0x%08x, ret=0x%08x "
				"state=0x%08x, ext_info=0x%08x\n",
				fc_info.as_out.resp,
				fc_info.as_out.ret,
				fc_info.as_out.state,
				fc_info.as_out.ext_info);

			ret = convert_fc_ret(fc_info.as_out.ret);
			if (ret != 0)
				break;

			MCDRV_DBG("state=%08X, idx=%02d: ext_info=%08X\n",
				fc_info.as_out.state,
				i,
				fc_info.as_out.ext_info);
			i++;
		};

		if (ret != 0)
			break;


	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}

/*----------------------------------------------------------------------------*/
/**
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int handle_ioctl_init(
	struct mc_instance	*instance,
	union mc_ioctl_init_params	*user_params
)
{
	int			ret = 0;
	union mc_ioctl_init_params	params;
	union mc_fc_init		fc_init;

	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		/* only the MobiCore Daemon is allowed to call this function */
		if (!is_caller_mc_daemon(instance)) {
			MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
			ret = -EFAULT;
			break;
		}

		ret = copy_from_user(
				  &(params.in),
				  &(user_params->in),
				  sizeof(params.in));
		if (ret != 0) {
			MCDRV_DBG_ERROR("copy_from_user failed\n");
			break;
		}

		memset(&fc_init, 0, sizeof(fc_init));

		fc_init.as_in.cmd	= MC_FC_INIT;
		/* base address of mci buffer 4KB aligned */
		fc_init.as_in.base   = (uint32_t)params.in.base;
		/* notification buffer start/length [16:16] [start, length] */
		fc_init.as_in.nq_info  = (params.in.nq_offset << 16)
					  | (params.in.nq_length & 0xFFFF);
		/* mcp buffer start/length [16:16] [start, length] */
		fc_init.as_in.mcp_info = (params.in.mcp_offset << 16)
					  | (params.in.mcp_length & 0xFFFF);

		/* Set KMOD notification queue to start of MCI
			mciInfo was already set up in mmap */
		if (!mci_base) {
			MCDRV_DBG_ERROR("No MCI set yet.\n");
			return -EFAULT;
		}
		MCDRV_DBG("in cmd=0x%08x, base=0x%08x, "
			  "nq_info=0x%08x, mcp_info=0x%08x\n",
			  fc_init.as_in.cmd,
			  fc_init.as_in.base,
			  fc_init.as_in.nq_info,
			  fc_init.as_in.mcp_info);

		mc_fastcall(&(fc_init.as_generic));

		MCDRV_DBG("out cmd=0x%08x, ret=0x%08x rfu=(0x%08x, 0x%08x)\n",
			  fc_init.as_out.resp,
			  fc_init.as_out.ret,
			  fc_init.as_out.rfu[0],
			  fc_init.as_out.rfu[1]);

		ret = convert_fc_ret(fc_init.as_out.ret);
		if (ret != 0)
			break;

		/* no ioctl response parameters */

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}

/*----------------------------------------------------------------------------*/
/**
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int handle_ioctl_fc_execute(
	struct mc_instance		*instance,
	union mc_ioctl_fc_execute_params	*user_params
)
{
	int				ret = 0;
	union mc_ioctl_fc_execute_params	params;
	union fc_generic			fc_params;

	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		/* only the MobiCore Daemon is allowed to call this function */
		if (!is_caller_mc_daemon(instance)) {
			MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
			ret = -EFAULT;
			break;
		}

		ret = copy_from_user(
				  &(params.in),
				  &(user_params->in),
				  sizeof(params.in));
		if (ret != 0) {
			MCDRV_DBG_ERROR("copy_from_user failed\n");
			break;
		}

		fc_params.as_in.cmd = -4;/*FC_EXECUTE */
		fc_params.as_in.param[0] = params.in.phys_start_addr;
		fc_params.as_in.param[1] = params.in.length;
		fc_params.as_in.param[2] = 0;

		MCDRV_DBG("in cmd=0x%08x, startAddr=0x%08x, length=0x%08x\n",
			  fc_params.as_in.cmd,
			  fc_params.as_in.param[0],
			  fc_params.as_in.param[1]);

		mc_fastcall(&fc_params);

		MCDRV_DBG("out cmd=0x%08x, ret=0x%08x rfu=(0x%08x, 0x%08x)\n",
			  fc_params.as_out.resp,
			  fc_params.as_out.ret,
			  fc_params.as_out.param[0],
			  fc_params.as_out.param[1]);

		ret = convert_fc_ret(fc_params.as_out.ret);
		if (ret != 0)
			break;

		/* no ioctl response parameters */

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}

/*----------------------------------------------------------------------------*/
#define MC_MAKE_VERSION(major, minor) \
		(((major & 0x0000ffff) << 16) | (minor & 0x0000ffff))
/**
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int handle_ioctl_get_version(
	struct mc_instance			*instance,
	struct mc_ioctl_get_version_params	*user_params
)
{
	int ret = 0;
	struct mc_ioctl_get_version_params params = {
		{
			MC_MAKE_VERSION(MCDRVMODULEAPI_VERSION_MAJOR,
					MCDRVMODULEAPI_VERSION_MINOR)
		}
	};

	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		MCDRV_DBG("mcDrvModuleApi version is %i.%i\n",
				MCDRVMODULEAPI_VERSION_MAJOR,
				MCDRVMODULEAPI_VERSION_MINOR);

		/* no ioctl response parameters */
		ret = copy_to_user(
					&(user_params->out),
					&(params.out),
					sizeof(params.out));
		if (ret != 0)
			MCDRV_DBG_ERROR("copy_to_user() failed\n");

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}

/*----------------------------------------------------------------------------*/
/**
 * This function will be called from user space as ioctl(...).
 * @param file	pointer to file
 * @param cmd	command
 * @param arg	arguments
 *
 * @return int 0 for OK and an errno in case of error
 */
static long mc_kernel_module_ioctl(
	struct file	*file,
	unsigned int	cmd,
	unsigned long	arg
)
{
	int ret;
	struct mc_instance *instance = get_instance(file);

	MCDRV_ASSERT(instance != NULL);

	switch (cmd) {
	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_DUMP_STATUS:
		ret = handle_ioctl_dump_status(
				instance,
				arg);
		break;

	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_FC_INIT:
		ret = handle_ioctl_init(
				instance,
				(union mc_ioctl_init_params *)arg);
		break;
	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_FC_INFO:
		ret = handle_ioctl_info(
				instance,
				(union mc_ioctl_info_params *)arg);
		break;

	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_FC_YIELD:
		ret = handle_ioctl_yield(
				instance);
		break;

	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_FC_NSIQ:
		ret = handle_ioctl_nsiq(
				instance,
				arg);
		break;

	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_DAEMON_LOCK_WSM_L2:
		ret = handle_ioctl_daemon_lock_wsm_l2(
			instance,
			(struct mc_ioctl_daemon_lock_wsm_l2_params *)arg);
		break;

	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_DAEMON_UNLOCK_WSM_L2:
		ret = handle_ioctl_daemon_unlock_wsm_l2(
			instance,
			(struct mc_ioctl_daemon_unlock_wsm_l2_params *)arg);
		break;

	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_FREE:
		/* called by ClientLib */
		ret = handle_ioctl_free(
				instance,
				(union mc_ioctl_free_params *)arg);
		break;

	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_APP_REGISTER_WSM_L2:
		/* called by ClientLib */
		ret = handle_ioctl_app_register_wsm_l2(
				instance,
				(union mc_ioctl_app_reg_wsm_l2_params *)arg);
		break;

	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_APP_UNREGISTER_WSM_L2:
		/* called by ClientLib */
		ret = handle_ioctl_app_unregister_wsm_l2(
				instance,
				(struct mc_ioctl_app_unreg_wsm_l2_params *)arg);
		break;

	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_FC_EXECUTE:
		ret = handle_ioctl_fc_execute(
				instance,
				(union mc_ioctl_fc_execute_params *)arg);
		break;

	/*--------------------------------------------------------------------*/
	case MC_DRV_KMOD_IOCTL_GET_VERSION:
		ret = handle_ioctl_get_version(
				instance,
				(struct mc_ioctl_get_version_params *)arg);
		break;

	/*--------------------------------------------------------------------*/
	default:
		MCDRV_DBG_ERROR("unsupported cmd=%d\n", cmd);
		ret = -EFAULT;
		break;

	} /* end switch(cmd) */

#ifdef MC_MEM_TRACES
	mobicore_log_read();
#endif

	return (int)ret;
}


/*----------------------------------------------------------------------------*/
/**
 * This function will be called from user space as read(...).
 * The read function is blocking until a interrupt occurs. In that case the
 * event counter is copied into user space and the function is finished.
 * @param *file
 * @param *buffer  buffer where to copy to(userspace)
 * @param buffer_len	 number of requested data
 * @param *pos	 not used
 * @return ssize_t  ok case: number of copied data
 *				error case: return errno
 */
static ssize_t mc_kernel_module_read(
	struct file	*file,
	char		*buffer,
	size_t		buffer_len,
	loff_t		*pos
)
{
	int ret = 0, ssiq_counter;
	size_t retLen = 0;
	struct mc_instance *instance = get_instance(file);

	MCDRV_ASSERT(instance != NULL);

	/* avoid debug output on non-error, because this is call quite often */
	MCDRV_DBG_VERBOSE("enter\n");

	do {
		/* only the MobiCore Daemon is allowed to call this function */
		if (!is_caller_mc_daemon(instance)) {
			MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
			ret = -EFAULT;
			break;
		}

		if (buffer_len < sizeof(unsigned int)) {
			MCDRV_DBG_ERROR("invalid length\n");
			ret = (ssize_t)(-EINVAL);
			break;
		}

		for (;;) {
			if (down_interruptible(
					&mc_drv_kmod_ctx.daemon_ctx.sem)) {
				MCDRV_DBG_VERBOSE("read interrupted\n");
				ret = (ssize_t)-ERESTARTSYS;
				break;
			}

			ssiq_counter = atomic_read(
					&(mc_drv_kmod_ctx.ssiq_ctx.counter));
			MCDRV_DBG_VERBOSE("ssiq_counter=%i, ctx.counter=%i\n",
				ssiq_counter,
				mc_drv_kmod_ctx.daemon_ctx.ssiq_counter);

			if (ssiq_counter !=
				mc_drv_kmod_ctx.daemon_ctx.ssiq_counter) {
				/* read data and exit loop without
					error */
				mc_drv_kmod_ctx.daemon_ctx.ssiq_counter =
					ssiq_counter;
				ret = 0;
				break;
			}

			/* end loop if non-blocking */
			if ((file->f_flags & O_NONBLOCK) != 0) {
				MCDRV_DBG_ERROR("non-blocking read\n");
				ret = (ssize_t)(-EAGAIN);
				break;
			}

			if (signal_pending(current) != 0) {
				MCDRV_DBG_VERBOSE("received signal.\n");
				ret = (ssize_t)(-ERESTARTSYS);
				break;
			}

		}

		/* we are here if an event occurred or we had an
			error.*/
		if (ret != 0)
			break;

		/* read data and exit loop */
		ret = copy_to_user(
				  buffer,
				  &(mc_drv_kmod_ctx.daemon_ctx.ssiq_counter),
				  sizeof(unsigned int));


		if (ret != 0) {
			MCDRV_DBG_ERROR("copy_to_user failed\n");
			ret = (ssize_t)(-EFAULT);
			break;
		}

		retLen = sizeof(s32);

	} while (FALSE);

	/* avoid debug on non-error. */
	if (ret == 0)
		ret = (size_t)retLen;
	else
		MCDRV_DBG("exit with %d/0x%08X\n", ret, ret);

	return (ssize_t)ret;
}

/*----------------------------------------------------------------------------*/
/**
 * Allocate WSM for given instance
 *
 * @param instance		instance
 * @param requested_size		size of the WSM
 * @param handle		pointer where the handle will be saved
 * @param virt_kernel_addr	pointer for the kernel virtual address
 * @param phys_addr		pointer for the physical address
 *
 * @return error code or 0 for success
 */
int mobicore_allocate_wsm(
	struct mc_instance	*instance,
	unsigned long		requested_size,
	uint32_t		*handle,
	void			**virt_kernel_addr,
	void			**phys_addr
)
{
	unsigned int	i;
	unsigned int	order;
	unsigned long	allocated_size;
	int		ret = 0;
	struct mc_contg_buffer	*contg_buffer = 0;
	void		*virt_kernel_addr_stack;
	void		*phys_addr_stack;

	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG("%s (size=%ld)\n", __func__, requested_size);

	order = size_to_order(requested_size);
	if (order == INVALID_ORDER) {
		MCDRV_DBG_ERROR(
			"size to order converting failed for size %ld\n",
			requested_size);
		return INVALID_ORDER;
	}

	allocated_size = (1<<order)*PAGE_SIZE;

	MCDRV_DBG("size %ld -> order %d --> %ld (2^n pages)\n",
		  requested_size, order, allocated_size);

	do {
		/* Usual Wsm request, allocate contigous buffer. */
		/* search for a free entry in the wsm buffer list
		 * REV axh: serialize this over multiple instances. */
		for (i = 0; i < MC_DRV_KMOD_CONTG_BUFFER_MAX; i++) {
			contg_buffer = &(instance->contg_buffers[i]);
			if (contg_buffer->handle == 0) {
				contg_buffer->handle = get_mc_kmod_unique_id();
				break;
			}
		}
		if (i == MC_DRV_KMOD_CONTG_BUFFER_MAX) {
			MCDRV_DBG_ERROR("no free contigous buffer\n");
			ret = -EFAULT;
			break;
		}

		/* Common code for all allocation paths */
		virt_kernel_addr_stack = (void *)__get_free_pages(
							GFP_USER | __GFP_COMP,
							order);
		if (virt_kernel_addr_stack == NULL) {
			MCDRV_DBG_ERROR("get_free_pages failed\n");
			ret = -ENOMEM;
			break;
		}

		/* Get physical address to instance data */
		phys_addr_stack = (void *)virt_to_phys(virt_kernel_addr_stack);
		/* TODO: check for INVALID_ADDRESS? */

		MCDRV_DBG(
			"allocated phys=0x%p - 0x%p, "
			"size=%ld, kernel_virt=0x%p, handle=%d\n",
			phys_addr_stack,
			(void *)((unsigned int)phys_addr_stack+allocated_size),
			allocated_size,
			virt_kernel_addr_stack,
			contg_buffer->handle);

		/* Usual Wsm request, allocate contg_buffer.
		 *		Also, we never free a persistent Tci */
		contg_buffer->phys_addr	 = phys_addr_stack;
		contg_buffer->virt_kernel_addr = virt_kernel_addr_stack;
		contg_buffer->virt_user_addr   = virt_kernel_addr_stack;
		contg_buffer->num_pages	 = (1U << order);
		*handle = contg_buffer->handle;
		*virt_kernel_addr = virt_kernel_addr_stack;
		*phys_addr = phys_addr_stack;

	} while (FALSE);

	MCDRV_DBG_VERBOSE("%s: exit with 0x%08X\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(mobicore_allocate_wsm);


/*----------------------------------------------------------------------------*/
/**
 * This function will be called from user space as address = mmap(...).
 *
 * @param file
 * @param vmarea
 * vmarea.pg_offset != 0 is mapping of MCI is requested
 *
 * @return 0 if OK or -ENOMEM in case of error.
 */
static int mc_kernel_module_mmap(
	struct file		*file,
	struct vm_area_struct	*vmarea
)
{
	unsigned int		i;
	unsigned int		order;
	void			*virt_kernel_addr_stack = 0;
	void			*phys_addr = 0;
	unsigned long		requested_size =
					vmarea->vm_end - vmarea->vm_start;
	unsigned long		allocated_size;
	int			ret = 0;
	struct mc_contg_buffer	*contg_buffer = 0;
	unsigned int		handle = 0;
	struct mc_instance	*instance = get_instance(file);
	unsigned int		request = vmarea->vm_pgoff * 4096;
#if defined(DEBUG)
	bool release = false;
#else
	bool release = true;
#endif

	MCDRV_ASSERT(instance != NULL);
	MCDRV_DBG("enter (vmaStart=0x%p, size=%ld, request=0x%x, mci=0x%x)\n",
		  (void *)vmarea->vm_start,
		  requested_size,
		  request,
		  mci_base);

	order = size_to_order(requested_size);
	if (order == INVALID_ORDER) {
		MCDRV_DBG_ERROR(
			"size to order converting failed for size %ld\n",
			requested_size);
		return -ENOMEM;
	}

	allocated_size = (1<<order)*PAGE_SIZE;

	MCDRV_DBG("size %ld -> order %d --> %ld (2^n pages)\n",
		  requested_size, order, allocated_size);

	do {
		/* Daemon tries to get an existing MCI */
		if ((request == MC_DRV_KMOD_MMAP_MCI) && (mci_base != 0)) {
			MCDRV_DBG("Request MCI, it is at (%x)\n", mci_base);

			if (!is_caller_mc_daemon(instance)) {
				ret = -EPERM;
				break;
			}
			virt_kernel_addr_stack = (void *)mci_base;
			phys_addr =
				(void *)virt_to_phys(virt_kernel_addr_stack);
		} else {
			/* Usual Wsm request, allocate buffer. */
			if (request == MC_DRV_KMOD_MMAP_WSM) {
				/* search for a free entry in the buffer list
				REV axh: serialize this over multiple instances.
				*/
				for (i = 0; i < MC_DRV_KMOD_CONTG_BUFFER_MAX;
					i++) {
					contg_buffer =
						&(instance->contg_buffers[i]);
					if (contg_buffer->handle == 0) {
						contg_buffer->handle =
							get_mc_kmod_unique_id();
						break;
					}
				}
				if (i == MC_DRV_KMOD_CONTG_BUFFER_MAX) {
					MCDRV_DBG_ERROR(
						"no free contigous buffer\n");
					ret = -EFAULT;
					break;
				}
			} else {
				if (request <= MC_DRV_KMOD_MMAP_PERSISTENTWSM
					|| release) {
					/* Special Wsm request
						--> only Daemon is allowed */
					if (!is_caller_mc_daemon(instance)) {
						ret = -EPERM;
						break;
					}
				}
			}
			if (request <= MC_DRV_KMOD_MMAP_PERSISTENTWSM) {
				/* Common code for all allocation paths
					*  get physical address, */
				virt_kernel_addr_stack =
					(void *)__get_free_pages(
							GFP_USER | __GFP_COMP,
							order);
				if (virt_kernel_addr_stack == NULL) {
					MCDRV_DBG_ERROR(
						"get_free_pages failed\n");
					ret = -ENOMEM;
					break;
				}
				if (request == MC_DRV_KMOD_MMAP_WSM)
					handle = contg_buffer->handle;
				/* Get physical address to instance data */
				/* TODO: check for INVALID_ADDRESS? */
				phys_addr = (void *)virt_to_phys(
							virt_kernel_addr_stack);
			} else {
#if defined(DEBUG)
				phys_addr = (void *)request;
				virt_kernel_addr_stack = phys_to_virt(request);
#endif
			}
		}
		/* Common code for all mmap calls:
		 * map page to user
		 * store data in page */

		MCDRV_DBG("allocated phys=0x%p - 0x%p, "
			"size=%ld, kernel_virt=0x%p, handle=%d\n",
			phys_addr,
			(void *)((unsigned int)phys_addr+allocated_size),
			allocated_size, virt_kernel_addr_stack, handle);

		vmarea->vm_flags |= VM_RESERVED;
		/* convert Kernel address to User Address. Kernel address begins
			at PAGE_OFFSET, user Address range is below PAGE_OFFSET.
			Remapping the area is always done, so multiple mappings
			of one region are possible. Now remap kernel address
			space into user space */
		ret = (int)remap_pfn_range(
				vmarea,
				(vmarea->vm_start),
				addr_to_pfn(phys_addr),
				requested_size,
				vmarea->vm_page_prot);
		if (ret != 0) {
			MCDRV_DBG_ERROR("remapPfnRange failed\n");

			/* free allocated pages when mmap fails, however, do not
				do it, when daemon tried to get an MCI that
				existed */
			if (!((request == MC_DRV_KMOD_MMAP_MCI) &&
				  (mci_base != 0)))
				free_continguous_pages(virt_kernel_addr_stack,
							(1U << order));
			break;
		}

		/* Usual Wsm request, allocate contg_buffer.
			When requesting Mci, we do not associate the page with
			the process.
			Note: we also never free the Mci
			Also, we never free a persistent Tci */
		if (request == MC_DRV_KMOD_MMAP_WSM) {
			contg_buffer->phys_addr = phys_addr;
			contg_buffer->virt_kernel_addr = virt_kernel_addr_stack;
			contg_buffer->virt_user_addr =
						(void *)(vmarea->vm_start);
			contg_buffer->num_pages = (1U << order);
		}

		/* set response in allocated buffer */
		{
			struct mc_mmap_resp *mmapResp =
				(struct mc_mmap_resp *)virt_kernel_addr_stack;
			/* TODO: do this for daemon only, otherwise set NULL */
			mmapResp->phys_addr = (uint32_t)phys_addr;
			mmapResp->handle = handle;
			if ((request == MC_DRV_KMOD_MMAP_MCI) &&
				(mci_base != 0)) {
				mmapResp->is_reused = 1;
			} else
				mmapResp->is_reused = 0;
		}

		/* store MCI pointer */
		if ((request == MC_DRV_KMOD_MMAP_MCI) && (mci_base == 0)) {
			mci_base = (uint32_t)virt_kernel_addr_stack;
			MCDRV_DBG("MCI base set to 0x%x\n", mci_base);
		}
	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return (int)ret;
}

#ifdef CONFIG_SMP
/*----------------------------------------------------------------------------*/
/**
 * Force migration of current task to CPU0(where the monitor resides)
 *
 * @return Error code or 0 for success
 */
static int goto_cpu0(
	void
)
{
	int		ret = 0;
	struct cpumask	mask =  CPU_MASK_CPU0;

	MCDRV_DBG_VERBOSE("System has %d CPU's, we are on CPU #%d\n"
		  "\tBinding this process to CPU #0.\n"
		  "\tactive mask is %lx, setting it to mask=%lx\n",
		  nr_cpu_ids,
		  raw_smp_processor_id(),
		  cpu_active_mask->bits[0],
		  mask.bits[0]);
	ret = set_cpus_allowed_ptr(current, &mask);
	if (ret != 0)
		MCDRV_DBG_ERROR("set_cpus_allowed_ptr=%d.\n", ret);
	MCDRV_DBG_VERBOSE("And now we are on CPU #%d\n",
				raw_smp_processor_id());

	return ret;
}

/*----------------------------------------------------------------------------*/
/**
 * Restore CPU mask for current to ALL Cpus(reverse of goto_cpu0)
 *
 * @return Error code or 0 for success
 */
static int goto_all_cpu(
	void
)
{
	int		ret = 0;

	struct cpumask	mask =  CPU_MASK_ALL;

	MCDRV_DBG_VERBOSE("System has %d CPU's, we are on CPU #%d\n"
		  "\tBinding this process to CPU #0.\n"
		  "\tactive mask is %lx, setting it to mask=%lx\n",
		  nr_cpu_ids,
		  raw_smp_processor_id(),
		  cpu_active_mask->bits[0],
		  mask.bits[0]);
	ret = set_cpus_allowed_ptr(current, &mask);
	if (ret != 0)
		MCDRV_DBG_ERROR("set_cpus_allowed_ptr=%d.\n", ret);
	MCDRV_DBG_VERBOSE("And now we are on CPU #%d\n",
				raw_smp_processor_id());

	return ret;
}

#else
static int goto_cpu0(void)
{
	return 0;
}

static int goto_all_cpu(void)
{
	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
/**
 * Initialize a new mobicore API instance object
 *
 * @return Instance or NULL if no allocation was possible.
 */
struct mc_instance *mobicore_open(
	void
) {
	struct mc_instance	*instance;
	pid_t			pid_vnr;

	instance = kzalloc(sizeof(*instance), GFP_KERNEL);
	if (instance == NULL)
		return NULL;

	/* get a unique ID for this instance (PIDs are not unique) */
	instance->handle = get_mc_kmod_unique_id();

	/* get the PID of the calling process. We avoid using
	 *	current->pid directly, as 2.6.24 introduced PID
	 *	namespaces. See also http://lwn.net/Articles/259217 */
	pid_vnr = task_pid_vnr(current);
	instance->pid_vnr = pid_vnr;

	return instance;
}
EXPORT_SYMBOL(mobicore_open);

/*----------------------------------------------------------------------------*/
/**
 * This function will be called from user space as fd = open(...).
 * A set of internal instance data are created and initialized.
 *
 * @param inode
 * @param file
 * @return 0 if OK or -ENOMEM if no allocation was possible.
 */
static int mc_kernel_module_open(
	struct inode	*inode,
	struct file	*file
)
{
	struct mc_instance	*instance;
	int			ret = 0;

	MCDRV_DBG_VERBOSE("enter\n");

	do {
		instance = mobicore_open();
		if (instance == NULL)
			return -ENOMEM;

		/* check if Daemon. We simply assume that the first to open us
			with root privileges must be the daemon. */
		if ((is_userland_caller_privileged())
			&& (mc_drv_kmod_ctx.daemon_inst == NULL)) {
			MCDRV_DBG("accept this as MobiCore Daemon\n");

			/* Set the caller's CPU mask to CPU0*/
			ret = goto_cpu0();
			if (ret != 0) {
				mobicore_release(instance);
				file->private_data = NULL;
				MCDRV_DBG("changing core failed!\n");
				break;
			}

			mc_drv_kmod_ctx.daemon_inst = instance;
			sema_init(&mc_drv_kmod_ctx.daemon_ctx.sem,
					DAEMON_SEM_VAL);
			/* init ssiq event counter */
			mc_drv_kmod_ctx.daemon_ctx.ssiq_counter =
				atomic_read(
					&(mc_drv_kmod_ctx.ssiq_ctx.counter));

#ifdef MC_MEM_TRACES
			/* The traces have to be setup on CPU-0 since we must
			 * do a fastcall to MobiCore. */
			if (!mci_base)
				/* Do the work only if MCI base is not
				 * initialized properly */
				work_on_cpu(0, mobicore_log_setup, NULL);
#endif
		}

		/* store instance data reference */
		file->private_data = instance;

		/* TODO axh: link all instances to allow clean up? */

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return (int)ret;

}

/*----------------------------------------------------------------------------*/
/**
 * Release a mobicore instance object and all objects related to it
 * @param instance instance
 * @return 0 if Ok or -E ERROR
 */
int mobicore_release(
	struct mc_instance	*instance
)
{
	int ret = 0;
	int i;
	struct mc_used_l2_table	*used_l2table, *used_l2table_temp;

	do {
		/* try to get the semaphore */
		ret = down_interruptible(&(mc_drv_kmod_ctx.wsm_l2_sem));
		if (ret != 0) {
			MCDRV_DBG_ERROR(
				"down_interruptible() failed with %d\n", ret);
			/* TODO: can be block here? */
			ret = -ERESTARTSYS;
		} else {
			/* Check if some WSM is still in use. */
			list_for_each_entry_safe(
				used_l2table,
				used_l2table_temp,
				&(mc_drv_kmod_ctx.mc_used_l2_tables),
				list
			) {
				if (used_l2table->owner == instance) {
					MCDRV_DBG_WARN(
						"trying to release WSM L2: "
						"physBase=%p ,nr_of_pages=%d\n",
						get_l2_table_phys(used_l2table),
						used_l2table->nr_of_pages);

					/* unlock app usage and free if MobiCore
					does not use it */
					delete_used_l2_table(used_l2table,
							FREE_FROM_NWD);
				}
			} /* end while */

			/* release semaphore */
			up(&(mc_drv_kmod_ctx.wsm_l2_sem));
		}


		/* release all mapped data */
		for (i = 0; i < MC_DRV_KMOD_CONTG_BUFFER_MAX; i++) {
			struct mc_contg_buffer *contg_buffer =
					&(instance->contg_buffers[i]);

			if (contg_buffer->virt_user_addr != 0) {
				free_continguous_pages(
					contg_buffer->virt_kernel_addr,
					contg_buffer->num_pages);
			}
		}

		/* release instance context */
		kfree(instance);
	} while (FALSE);

	return ret;
}
EXPORT_SYMBOL(mobicore_release);

/*----------------------------------------------------------------------------*/
/**
 * This function will be called from user space as close(...).
 * The instance data are freed and the associated memory pages are unreserved.
 *
 * @param inode
 * @param file
 *
 * @return 0
 */
static int mc_kernel_module_release(
	struct inode	*inode,
	struct file	*file
)
{
	int			ret = 0;
	struct mc_instance	*instance = get_instance(file);

	MCDRV_DBG_VERBOSE("enter\n");

	do {
		/* check if daemon closes us. */
		if (is_caller_mc_daemon(instance)) {
			/* TODO: cleanup?
				* mc_drv_kmod_ctx.mc_used_l2_tables remains */
			MCDRV_DBG_WARN("WARNING: MobiCore Daemon died\n");
			mc_drv_kmod_ctx.daemon_inst = NULL;
		}

		ret = mobicore_release(instance);

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return (int)ret;
}


/*----------------------------------------------------------------------------*/
/**
 * This function represents the interrupt function of the mcDrvModule.
 * It signals by incrementing of an event counter and the start of the read
 * waiting queue, the read function a interrupt has occurred.
 *
 * @param   intr
 * @param   *context  pointer to registered device data
 *
 * @return  IRQ_HANDLED
 */
static irqreturn_t mc_kernel_module_intr_ssiq(
	int	intr,
	void	*context
)
{
	irqreturn_t	ret = IRQ_NONE;

	/* we know the context. */
	MCDRV_ASSERT(&mc_drv_kmod_ctx == context);

	do {
		if (intr != MC_INTR_SSIQ) {
			/* this should not happen, as we did no register for any
				other interrupt. For debugging, we print a
				message, but continue */
			MCDRV_DBG_WARN(
				"unknown interrupt %d, expecting only %d\n",
				intr, MC_INTR_SSIQ);
		}
		MCDRV_DBG_VERBOSE("received interrupt %d\n",
				  intr);

		/* increment interrupt event counter */
		atomic_inc(&(mc_drv_kmod_ctx.ssiq_ctx.counter));

		/* signal the daemon */
		up(&mc_drv_kmod_ctx.daemon_ctx.sem);


		ret = IRQ_HANDLED;

	} while (FALSE);

	return ret;
}

/*----------------------------------------------------------------------------*/
/** function table structure of this device driver. */
static const struct file_operations mc_kernel_module_file_operations = {
	.owner		= THIS_MODULE, /**< driver owner */
	.open		= mc_kernel_module_open, /**< driver open function */
	.release	= mc_kernel_module_release, /**< driver release function*/
	.unlocked_ioctl	= mc_kernel_module_ioctl, /**< driver ioctl function */
	.mmap		= mc_kernel_module_mmap, /**< driver mmap function */
	.read		= mc_kernel_module_read, /**< driver read function */
};

/*----------------------------------------------------------------------------*/
/** registration structure as miscdevice. */
static struct miscdevice mc_kernel_module_device = {
	.name	= MC_DRV_MOD_DEVNODE, /**< device name */
	.minor	= MISC_DYNAMIC_MINOR, /**< device minor number */
	/** device interface function structure */
	.fops	= &mc_kernel_module_file_operations,
};


/*----------------------------------------------------------------------------*/
/**
 * This function is called the kernel during startup or by a insmod command.
 * This device is installed and registered as miscdevice, then interrupt and
 * queue handling is set up
 *
 * @return 0 for no error or -EIO if registration fails
 */
static int __init mc_kernel_module_init(
	void
)
{
	int ret = 0;

	MCDRV_DBG("enter (Build " __TIMESTAMP__ ")\n");
	MCDRV_DBG("mcDrvModuleApi version is %i.%i\n",
			MCDRVMODULEAPI_VERSION_MAJOR,
			MCDRVMODULEAPI_VERSION_MINOR);
#ifdef MOBICORE_COMPONENT_BUILD_TAG
	MCDRV_DBG("%s\n", MOBICORE_COMPONENT_BUILD_TAG);
#endif
	do {
		/* Hardware does not support ARM TrustZone
			-> Cannot continue! */
		if (!has_security_extensions()) {
			MCDRV_DBG_ERROR(
				"Hardware does't support ARM TrustZone!\n");
			ret = -ENODEV;
			break;
		}

		/* Running in secure mode -> Cannot load the driver! */
		if (is_secure_mode()) {
			MCDRV_DBG_ERROR("Running in secure MODE!\n");
			ret = -ENODEV;
			break;
		}

		sema_init(&mc_drv_kmod_ctx.daemon_ctx.sem, DAEMON_SEM_VAL);
		/* set up S-SIQ interrupt handler */
		ret = request_irq(
				  MC_INTR_SSIQ,
				  mc_kernel_module_intr_ssiq,
				  IRQF_TRIGGER_RISING,
				  MC_DRV_MOD_DEVNODE,
				  &mc_drv_kmod_ctx);
		if (ret != 0) {
			MCDRV_DBG_ERROR("interrupt request failed\n");
			break;
		}

		ret = misc_register(&mc_kernel_module_device);
		if (ret != 0) {
			MCDRV_DBG_ERROR("device register failed\n");
			break;
		}

		/* initialize event counter for signaling of an IRQ to zero */
		atomic_set(&(mc_drv_kmod_ctx.ssiq_ctx.counter), 0);

		/* init list for WSM L2 chunks. */
		INIT_LIST_HEAD(&(mc_drv_kmod_ctx.mc_l2_tables_sets));

		/* L2 table descriptor list. */
		INIT_LIST_HEAD(&(mc_drv_kmod_ctx.mc_used_l2_tables));

		sema_init(&(mc_drv_kmod_ctx.wsm_l2_sem), 1);

		/* initialize unique number counter which we can use for
			handles. It is limited to 2^32, but this should be
			enough to be roll-over safe for us. We start with 1
			instead of 0. */
		atomic_set(&(mc_drv_kmod_ctx.unique_counter), 1);

		mci_base = 0;
		MCDRV_DBG("initialized\n");

		ret = 0;

	} while (FALSE);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return (int)ret;
}



/*----------------------------------------------------------------------------*/
/**
 * This function removes this device driver from the Linux device manager .
 */
static void __exit mc_kernel_module_exit(
	void
)
{
	struct mc_used_l2_table	*used_l2table;

	MCDRV_DBG_VERBOSE("enter\n");

	mobicore_log_free();

	/* Check if some WSM is still in use. */
	list_for_each_entry(
		used_l2table,
		&(mc_drv_kmod_ctx.mc_used_l2_tables),
		list
	) {
		MCDRV_DBG_WARN(
			"WSM L2 still in use: physBase=%p ,nr_of_pages=%d\n",
			get_l2_table_phys(used_l2table),
			used_l2table->nr_of_pages);
	} /* end while */

	free_irq(MC_INTR_SSIQ, &mc_drv_kmod_ctx);

	misc_deregister(&mc_kernel_module_device);
	MCDRV_DBG_VERBOSE("exit");
}


/*----------------------------------------------------------------------------*/
/* Linux Driver Module Macros */
module_init(mc_kernel_module_init);
module_exit(mc_kernel_module_exit);
MODULE_AUTHOR("Giesecke & Devrient GmbH");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MobiCore driver");

/** @} */

