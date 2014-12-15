/*
 * Support for Medifield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * Copyright (c) 2010 Silicon Hive www.siliconhive.com.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef	__HMM_BO_H__
#define	__HMM_BO_H__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include "hmm_common.h"
#include "hmm/hmm_vm.h"

#define	check_bo_status_yes_goto(bo, _status, label) \
	var_not_equal_goto((bo->status & (_status)), (_status), \
			label, \
			"HMM buffer status not contain %s.\n", \
			#_status)

#define	check_bo_status_no_goto(bo, _status, label) \
	var_equal_goto((bo->status & (_status)), (_status), \
			label, \
			"HMM buffer status contains %s.\n", \
			#_status)
#define	list_to_hmm_bo(list_ptr)	\
	list_entry((list_ptr), struct hmm_buffer_object, list)

#define	kref_to_hmm_bo(kref_ptr)	\
	list_entry((kref_ptr), struct hmm_buffer_object, kref)

#define	check_bo_null_return(bo, exp)	\
	check_null_return(bo, exp, "NULL hmm buffer object.\n")

#define	check_bo_null_return_void(bo)	\
	check_null_return_void(bo, "NULL hmm buffer object.\n")

#define	HMM_MAX_ORDER		3
#define	HMM_MIN_ORDER		0

struct hmm_bo_device;

/*
 * buffer object type.
 *
 *	HMM_BO_PRIVATE:
 *	pages are allocated by driver itself.
 *	HMM_BO_SHARE:
 *	pages are allocated by other component. currently: video driver.
 *	HMM_BO_USER:
 *	pages are allocated in user space process.
 *	HMM_BO_ION:
 *	pages are allocated through ION.
 *
 */
enum hmm_bo_type {
	HMM_BO_PRIVATE,
	HMM_BO_SHARE,
	HMM_BO_USER,
#ifdef CONFIG_ION
	HMM_BO_ION,
#endif
	HMM_BO_LAST,
};

enum hmm_page_type {
	HMM_PAGE_TYPE_RESERVED,
	HMM_PAGE_TYPE_DYNAMIC,
	HMM_PAGE_TYPE_GENERAL,
};

#define	HMM_BO_VM_ALLOCED	0x1
#define	HMM_BO_PAGE_ALLOCED	0x2
#define	HMM_BO_BINDED		0x4
#define	HMM_BO_MMAPED		0x8
#define	HMM_BO_VMAPED		0x10
#define	HMM_BO_VMAPED_CACHED	0x20
#define	HMM_BO_ACTIVE		0x1000
#define	HMM_BO_MEM_TYPE_USER     0x1
#define	HMM_BO_MEM_TYPE_PFN      0x2

struct hmm_page_object {
	struct page		*page;
	enum hmm_page_type	type;
};

struct hmm_buffer_object {
	struct hmm_bo_device	*bdev;
	struct list_head	list;
	struct kref		kref;

	/* mutex protecting this BO */
	struct mutex		mutex;
	enum hmm_bo_type	type;
	struct hmm_page_object	*page_obj;	/* physical pages */
	unsigned int		pgnr;	/* page number */
	int			from_highmem;
	int			mmap_count;
	struct hmm_vm_node	*vm_node;
#ifdef CONFIG_ION
	struct ion_handle	*ihandle;
#endif
	int			status;
	int         mem_type;
	void		*vmap_addr; /* kernel virtual address by vmap */
	/*
	 * release callback for releasing buffer object.
	 *
	 * usually set to the release function to release the
	 * upper level buffer object which has hmm_buffer_object
	 * embedded in. if the hmm_buffer_object is dynamically
	 * created by hmm_bo_create, release will set to kfree.
	 *
	 */
	void (*release)(struct hmm_buffer_object *bo);
};

/*
 * use this function to initialize pre-allocated hmm_buffer_object.
 *
 * the hmm_buffer_object use reference count to manage its life cycle.
 *
 * bo->kref is inited to 1.
 *
 * use hmm_bo_ref/hmm_bo_unref increase/decrease the reference count,
 * and hmm_bo_unref will free resource of buffer object (but not the
 * buffer object itself as it can be both pre-allocated or dynamically
 * allocated) when reference reaches 0.
 *
 * see detailed description of hmm_bo_ref/hmm_bo_unref below.
 *
 * as hmm_buffer_object may be used as an embedded object in an upper
 * level object, a release callback must be provided. if it is
 * embedded in upper level object, set release call back to release
 * function of that object. if no upper level object, set release
 * callback to NULL.
 *
 * ex:
 *	struct hmm_buffer_object bo;
 *	hmm_bo_init(bdev, &bo, pgnr, NULL);
 *
 * or
 *	struct my_buffer_object {
 *		struct hmm_buffer_object bo;
 *		...
 *	};
 *
 *	void my_buffer_release(struct hmm_buffer_object *bo)
 *	{
 *		struct my_buffer_object *my_bo =
 *			container_of(bo, struct my_buffer_object, bo);
 *
 *		...	// release resource in my_buffer_object
 *
 *		kfree(my_bo);
 *	}
 *
 *	struct my_buffer_object *my_bo =
 *		kmalloc(sizeof(*my_bo), GFP_KERNEL);
 *
 *	hmm_bo_init(bdev, &my_bo->bo, pgnr, my_buffer_release);
 *	...
 *
 *	hmm_bo_unref(&my_bo->bo);
 */
int hmm_bo_init(struct hmm_bo_device *bdev,
		struct hmm_buffer_object *bo,
		unsigned int pgnr,
		void (*release)(struct hmm_buffer_object *));

/*
 * use these functions to dynamically alloc hmm_buffer_object.
 *
 * hmm_bo_init will called for that allocated buffer object, and
 * the release callback is set to kfree.
 *
 * ex:
 *	hmm_buffer_object *bo = hmm_bo_create(bdev, pgnr);
 *	...
 *	hmm_bo_unref(bo);
 */
struct hmm_buffer_object *hmm_bo_create(struct hmm_bo_device *bdev,
		int pgnr);

/*
 * increse buffer object reference.
 */
void hmm_bo_ref(struct hmm_buffer_object *bo);

/*
 * decrese buffer object reference. if reference reaches 0,
 * release function of the buffer object will be called.
 *
 * this call is also used to release hmm_buffer_object or its
 * upper level object with it embedded in. you need to call
 * this function when it is no longer used.
 *
 * Note:
 *
 * user dont need to care about internal resource release of
 * the buffer object in the release callback, it will be
 * handled internally.
 *
 * this call will only release internal resource of the buffer
 * object but will not free the buffer object itself, as the
 * buffer object can be both pre-allocated statically or
 * dynamically allocated. so user need to deal with the release
 * of the buffer object itself manually. below example shows
 * the normal case of using the buffer object.
 *
 *	struct hmm_buffer_object *bo = hmm_bo_create(bdev, pgnr);
 *	......
 *	hmm_bo_unref(bo);
 *
 * or:
 *
 *	struct hmm_buffer_object bo;
 *
 *	hmm_bo_init(bdev, &bo, pgnr, NULL);
 *	...
 *	hmm_bo_unref(&bo);
 */
void hmm_bo_unref(struct hmm_buffer_object *bo);


/*
 * put buffer object to unactivated status, meaning put it into
 * bo->bdev->free_bo_list, but not destroy it.
 *
 * this can be used to instead of hmm_bo_destroy if there are
 * lots of petential hmm_bo_init/hmm_bo_destroy operations with
 * the same buffer object size. using this with hmm_bo_device_get_bo
 * can improve performace as lots of memory allocation/free are
 * avoided..
 */
void hmm_bo_unactivate(struct hmm_buffer_object *bo);
int hmm_bo_activated(struct hmm_buffer_object *bo);

/*
 * allocate/free virtual address space for the bo.
 */
int hmm_bo_alloc_vm(struct hmm_buffer_object *bo);
void hmm_bo_free_vm(struct hmm_buffer_object *bo);
int hmm_bo_vm_allocated(struct hmm_buffer_object *bo);

/*
 * allocate/free physical pages for the bo. will try to alloc mem
 * from highmem if from_highmem is set, and type indicate that the
 * pages will be allocated by using video driver (for share buffer)
 * or by ISP driver itself.
 */
int hmm_bo_alloc_pages(struct hmm_buffer_object *bo,
		enum hmm_bo_type type, int from_highmem,
		void *userptr, bool cached);
void hmm_bo_free_pages(struct hmm_buffer_object *bo);
int hmm_bo_page_allocated(struct hmm_buffer_object *bo);

/*
 * get physical page info of the bo.
 */
int hmm_bo_get_page_info(struct hmm_buffer_object *bo,
		struct hmm_page_object **page_obj, int *pgnr);

/*
 * bind/unbind the physical pages to a virtual address space.
 */
int hmm_bo_bind(struct hmm_buffer_object *bo);
void hmm_bo_unbind(struct hmm_buffer_object *bo);
int hmm_bo_binded(struct hmm_buffer_object *bo);

/*
 * vmap buffer object's pages to contiguous kernel virtual address.
 * if the buffer has been vmaped, return the virtual address directly.
 */
void *hmm_bo_vmap(struct hmm_buffer_object *bo, bool cached);

/*
 * flush the cache for the vmapped buffer object's pages,
 * if the buffer has not been vmapped, return directly.
 */
void hmm_bo_flush_vmap(struct hmm_buffer_object *bo);

/*
 * vunmap buffer object's kernel virtual address.
 */
void hmm_bo_vunmap(struct hmm_buffer_object *bo);

/*
 * mmap the bo's physical pages to specific vma.
 *
 * vma's address space size must be the same as bo's size,
 * otherwise it will return -EINVAL.
 *
 * vma->vm_flags will be set to (VM_RESERVED | VM_IO).
 */
int hmm_bo_mmap(struct vm_area_struct *vma,
		struct hmm_buffer_object *bo);

extern struct hmm_pool	dynamic_pool;
extern struct hmm_pool	reserved_pool;

#endif
