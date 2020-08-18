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
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/cma.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/pagemap.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <linux/clk.h>
#include <linux/irqdomain.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/completion.h>
#include <mt-plat/mtk_cmo.h>

#include "trustzone/kree/tz_mod.h"
#include "trustzone/kree/mem.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/tz_pm.h"
#include "trustzone/kree/tz_irq.h"
#include "trustzone/tz_cross/ta_mem.h"
#include "kree_int.h"
#include "tz_counter.h"
#include "tz_cross/ta_pm.h"
#include "tz_ndbg.h"

#include "tz_playready.h"

#include "tz_secure_clock.h"
#define MTEE_MOD_TAG "MTEE_MOD"

#define MAX_TAG_SIZE   32

#define TZ_DEVNAME "mtk_tz"

/**Used for mapping user space address to physical memory
 */
struct MTIOMMU_PIN_RANGE_T {
	void *start;
	void *pageArray;
	uint32_t size;
	uint32_t nrPages;
	uint32_t isPage;
};

#ifdef CONFIG_OF
/*Used for clk management*/
#define CLK_NAME_LEN 16
struct mtee_clk {
	struct list_head list;
	char clk_name[CLK_NAME_LEN];
	struct clk *clk;
};
static LIST_HEAD(mtee_clk_list);

static void mtee_clks_init(struct platform_device *pdev)
{
	int clks_num;
	int idx;

	clks_num = of_property_count_strings(pdev->dev.of_node, "clock-names");
	for (idx = 0; idx < clks_num; idx++) {
		const char *clk_name;
		struct clk *clk;
		struct mtee_clk *mtee_clk;

		if (of_property_read_string_index(pdev->dev.of_node,
					"clock-names", idx, &clk_name)) {
			pr_warn("[%s] get clk_name failed, index:%d\n",
				MODULE_NAME,
				idx);
			continue;
		}
		if (strlen(clk_name) > CLK_NAME_LEN-1) {
			pr_warn("[%s] clk_name %s is longer than %d, trims to %d\n",
				MODULE_NAME,
				clk_name, CLK_NAME_LEN-1, CLK_NAME_LEN-1);
		}
		clk = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(clk)) {
			pr_warn("[%s] get devm_clk_get failed, clk_name:%s\n",
				MODULE_NAME,
				clk_name);
			continue;
		}

		mtee_clk = kzalloc(sizeof(struct mtee_clk), GFP_KERNEL);
		strncpy(mtee_clk->clk_name, clk_name, CLK_NAME_LEN-1);
		mtee_clk->clk = clk;

		list_add(&mtee_clk->list, &mtee_clk_list);
	}
}

struct clk *mtee_clk_get(const char *clk_name)
{
	struct mtee_clk *cur;
	struct mtee_clk *tmp;

	list_for_each_entry_safe(cur, tmp, &mtee_clk_list, list) {
		if (strncmp(cur->clk_name, clk_name,
			strlen(cur->clk_name)) == 0)
			return cur->clk;
	}
	return NULL;
}

/*Used for power management*/
#define PM_NAME_LEN 16
struct mtee_pm {
	struct list_head list;
	char pm_name[PM_NAME_LEN];
	struct device *dev;
};
static LIST_HEAD(mtee_pm_list);

static void mtee_pms_init(struct platform_device *pdev)
{
	int pms_num;
	int idx;

	pms_num = of_property_count_strings(pdev->dev.of_node, "pm-names");
	for (idx = 0; idx < pms_num; idx++) {
		const char *pm_name;
		struct platform_device *platdev;
		struct mtee_pm *mtee_pm;
		struct device_node *node;

		if (of_property_read_string_index(pdev->dev.of_node,
					"pm-names", idx, &pm_name)) {
			pr_warn("[%s] get pm_name failed, index:%d\n",
				MODULE_NAME,
				idx);
			continue;
		}
		if (strlen(pm_name) > PM_NAME_LEN-1) {
			pr_warn("[%s] pm_name %s is longer than %d, trims to %d\n",
				MODULE_NAME,
				pm_name, PM_NAME_LEN-1, PM_NAME_LEN-1);
		}
		node = of_parse_phandle(pdev->dev.of_node, "pm-devs", idx);
		if (!node)
			continue;

		platdev = of_find_device_by_node(node);
		of_node_put(node);
		if (!platdev)
			continue;

		mtee_pm = kzalloc(sizeof(struct mtee_pm), GFP_KERNEL);
		strncpy(mtee_pm->pm_name, pm_name, PM_NAME_LEN-1);
		mtee_pm->dev = &platdev->dev;

		list_add(&mtee_pm->list, &mtee_pm_list);
	}
}

struct device *mtee_pmdev_get(const char *pm_name)
{
	struct mtee_pm *cur;
	struct mtee_pm *tmp;

	list_for_each_entry_safe(cur, tmp, &mtee_pm_list, list) {
		if (strncmp(cur->pm_name, pm_name,
			strlen(cur->pm_name)) == 0)
			return cur->dev;
	}
	return NULL;
}

#endif

/*****************************************************************************
 * FUNCTION DEFINITION
 *****************************************************************************/
static struct cdev tz_client_cdev;
static dev_t tz_client_dev;

static int tz_client_open(struct inode *inode, struct file *filp);
static int tz_client_release(struct inode *inode, struct file *filp);
static long tz_client_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg);
#ifdef CONFIG_COMPAT
static long tz_client_ioctl_compat(struct file *file, unsigned int cmd,
					unsigned long arg);
#endif

static int tz_client_init_client_info(struct file *file);
static void tz_client_free_client_info(struct file *file);

#define TZ_CLIENT_INIT_HANDLE_SPACE  32
#define TZ_CLIENT_INIT_SHAREDMEM_SPACE  32


struct tz_sharedmem_info {
	KREE_SHAREDMEM_HANDLE mem_handle;
	KREE_SESSION_HANDLE session_handle;
	uint32_t *resource;
};

struct tz_client_info {
	int handle_num;
	struct mutex mux;
	KREE_SESSION_HANDLE *handles;
	struct tz_sharedmem_info *shm_info;
	int shm_info_num;
};


static const struct file_operations tz_client_fops = {
	.open = tz_client_open,
	.release = tz_client_release,
	.unlocked_ioctl = tz_client_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_client_ioctl_compat,
#endif

	.owner = THIS_MODULE,
};

static int tz_client_open(struct inode *inode, struct file *filp)
{
	return tz_client_init_client_info(filp);
}

static int tz_client_release(struct inode *inode, struct file *filp)
{
	tz_client_free_client_info(filp);

	return 0;
}

/* map user space pages */
/* control -> 0 = write, 1 = read only memory */
static long _map_user_pages(struct MTIOMMU_PIN_RANGE_T *pinRange,
				unsigned long uaddr, uint32_t size,
				uint32_t control)
{
	int nr_pages;
	unsigned long first, last, gup_flags;
	struct page **pages;
	struct vm_area_struct *vma;
	int res, j;

	if ((uaddr == 0) || (size == 0))
		return -EFAULT;

	pinRange->start = (void *)uaddr;
	pinRange->size = size;

	first = (uaddr & PAGE_MASK) >> PAGE_SHIFT;
	last = ((uaddr + size + PAGE_SIZE - 1) & PAGE_MASK) >> PAGE_SHIFT;
	nr_pages = last - first;
	pages = kcalloc(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (pages == NULL)
		return -ENOMEM;

	pinRange->pageArray = (void *) pages;
	gup_flags = (control == 0) ? FOLL_WRITE  : 0;

	/* Try to fault in all of the necessary pages */
	down_read(&current->mm->mmap_sem);
	vma = find_vma_intersection(current->mm, uaddr, uaddr+size);
	if (!vma) {
		res = -EFAULT;
		goto out;
	}
	if (!(vma->vm_flags & (VM_IO | VM_PFNMAP))) {
		pinRange->isPage = 1;
#if defined(CONFIG_MTEE_CMA_SECURE_MEMORY)
		res = get_user_pages_durable(uaddr, nr_pages, gup_flags,
					pages, NULL);
#else
		res = get_user_pages(uaddr, nr_pages, gup_flags,
					pages, NULL);
#endif
	} else {
		/* pfn mapped memory, don't touch page struct.
		 * the buffer manager (possibly ion) should make sure
		 * it won't be used for anything else
		 */
		pinRange->isPage = 0;
		res = 0;
		do {
			unsigned long *pfns = (void *)pages;

			while (res < nr_pages &&
				uaddr + PAGE_SIZE <= vma->vm_end) {
				j = follow_pfn(vma, uaddr, &pfns[res]);
				if (j) { /* error */
					res = j;
					goto out;
				}
				uaddr += PAGE_SIZE;
				res++;
			}
			if (res >= nr_pages || uaddr < vma->vm_end)
				break;
			vma = find_vma_intersection(current->mm, uaddr,
							uaddr+1);
		} while (vma && vma->vm_flags & (VM_IO | VM_PFNMAP));
	}
 out:
	up_read(&current->mm->mmap_sem);
	if (res < 0) {
		pr_warn("%s error = %d\n", __func__, res);
		goto out_free;
	}

	pinRange->nrPages = res;
	/* Errors and no page mapped should return here */
	if (res < nr_pages)
		goto out_unmap;

	return 0;

 out_unmap:
	pr_warn("%s fail\n", __func__);
	if (pinRange->isPage) {
		for (j = 0; j < res; j++)
			put_page(pages[j]);
	}
	res = -EFAULT;

 out_free:
	kfree(pages);
	return res;
}

static void _unmap_user_pages(struct MTIOMMU_PIN_RANGE_T *pinRange)
{
	int res;
	int j;
	struct page **pages;

	pages = (struct page **)pinRange->pageArray;

	if (pinRange->isPage) {
		res = pinRange->nrPages;

		if (res > 0) {
			for (j = 0; j < res; j++)
				put_page(pages[j]);
			res = 0;
		}
	}

	kfree(pages);
}

static struct tz_sharedmem_info *tz_get_sharedmem(struct tz_client_info *info,
						  KREE_SHAREDMEM_HANDLE handle)
{
	struct tz_sharedmem_info *shm_info;
	int i;

	/* search handle */
	shm_info = NULL;
	for (i = 0; i < info->shm_info_num; i++) {
		if (info->shm_info[i].mem_handle == handle) {
			shm_info = &info->shm_info[i];
			break;
		}
	}

	return shm_info;
}



/**************************************************************************
 *  DEV tz_client_info handling
 **************************************************************************/
static int tz_client_register_session(struct file *file,
					KREE_SESSION_HANDLE handle)
{
	struct tz_client_info *info;
	int i, num, nspace, ret = -1;
	void *ptr;

	info = (struct tz_client_info *)file->private_data;

	/* lock */
	mutex_lock(&info->mux);

	/* find empty space. */
	num = info->handle_num;
	for (i = 0; i < num; i++) {
		if (info->handles[i] == 0) {
			ret = i;
			break;
		}
	}

	if (ret == -1) {
		/* Try grow the space */
		nspace = num * 2;
		ptr = krealloc(info->handles,
				nspace * sizeof(KREE_SESSION_HANDLE),
				GFP_KERNEL);
		if (ptr == 0) {
			mutex_unlock(&info->mux);
			return -ENOMEM;
		}

		ret = num;
		info->handle_num = nspace;
		info->handles = (KREE_SESSION_HANDLE *) ptr;
		memset(&info->handles[num], 0,
			(nspace - num) * sizeof(KREE_SESSION_HANDLE));
	}

	if (ret >= 0)
		info->handles[ret] = handle;

	/* unlock */
	mutex_unlock(&info->mux);
	return ret + 1;
}

static KREE_SESSION_HANDLE tz_client_get_session(struct file *file, int handle)
{
	struct tz_client_info *info;
	KREE_SESSION_HANDLE rhandle;

	info = (struct tz_client_info *)file->private_data;

	/* lock */
	mutex_lock(&info->mux);

	if (handle <= 0 || handle > info->handle_num)
		rhandle = (KREE_SESSION_HANDLE) 0;
	else
		rhandle = info->handles[handle - 1];

	/* unlock */
	mutex_unlock(&info->mux);
	return rhandle;
}

static int tz_client_unregister_session(struct file *file, int handle)
{
	struct tz_client_info *info;
	int ret = 0;

	info = (struct tz_client_info *)file->private_data;

	/* lock */
	mutex_lock(&info->mux);

	if (handle <= 0 || handle > info->handle_num ||
			info->handles[handle - 1] == 0)
		ret = -EINVAL;
	else
		info->handles[handle - 1] = (KREE_SESSION_HANDLE) 0;

	/* unlock */
	mutex_unlock(&info->mux);
	return ret;
}

static int
tz_client_register_sharedmem(struct file *file, KREE_SESSION_HANDLE handle,
				KREE_SHAREDMEM_HANDLE mem_handle,
				uint32_t *resource)
{
	struct tz_client_info *info;
	int i, num, nspace, ret = -1;
	void *ptr;

	info = (struct tz_client_info *)file->private_data;

	/* lock */
	mutex_lock(&info->mux);

	/* find empty space. */
	num = info->shm_info_num;
	for (i = 0; i < num; i++) {
		if ((int)info->shm_info[i].mem_handle == 0) {
			ret = i;
			break;
		}
	}

	if (ret == -1) {
		/* Try grow the space */
		nspace = num * 2;
		ptr = krealloc(info->shm_info,
				nspace * sizeof(struct tz_sharedmem_info),
			       GFP_KERNEL);
		if (ptr == 0) {
			mutex_unlock(&info->mux);
			return -ENOMEM;
		}

		ret = num;
		info->shm_info_num = nspace;
		info->shm_info = (struct tz_sharedmem_info *)ptr;
		memset(&info->shm_info[num], 0,
			(nspace - num) * sizeof(struct tz_sharedmem_info));
	}

	if (ret >= 0) {
		info->shm_info[ret].mem_handle = mem_handle;
		info->shm_info[ret].session_handle = handle;
		info->shm_info[ret].resource = resource;
	}
	/* unlock */
	mutex_unlock(&info->mux);

	return ret;
}

static int tz_client_unregister_sharedmem(struct file *file,
						KREE_SHAREDMEM_HANDLE handle)
{
	struct tz_client_info *info;
	struct tz_sharedmem_info *shm_info;
	struct MTIOMMU_PIN_RANGE_T *pin;
	int ret = 0;

	info = (struct tz_client_info *)file->private_data;

	/* lock */
	mutex_lock(&info->mux);

	shm_info = tz_get_sharedmem(info, handle);

	if ((shm_info == NULL) || (shm_info->mem_handle == 0))
		ret = -EINVAL;
	else {
		pin = (struct MTIOMMU_PIN_RANGE_T *) shm_info->resource;
		_unmap_user_pages(pin);
		kfree(pin);
		memset(shm_info, 0, sizeof(struct tz_sharedmem_info));
	}

	/* unlock */
	mutex_unlock(&info->mux);

	return ret;
}


static int tz_client_init_client_info(struct file *file)
{
	struct tz_client_info *info;

	info = (struct tz_client_info *)
	    kmalloc(sizeof(struct tz_client_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->handles = kcalloc(TZ_CLIENT_INIT_HANDLE_SPACE,
				sizeof(KREE_SESSION_HANDLE), GFP_KERNEL);
	if (!info->handles) {
		kfree(info);
		return -ENOMEM;
	}
	info->handle_num = TZ_CLIENT_INIT_HANDLE_SPACE;

	/* init shared memory */
	info->shm_info = kcalloc(TZ_CLIENT_INIT_SHAREDMEM_SPACE,
				 sizeof(struct tz_sharedmem_info), GFP_KERNEL);
	if (!info->shm_info) {
		kfree(info->handles);
		kfree(info);
		return -ENOMEM;
	}
	info->shm_info_num = TZ_CLIENT_INIT_SHAREDMEM_SPACE;

	mutex_init(&info->mux);
	file->private_data = info;
	return 0;
}

static void tz_client_free_client_info(struct file *file)
{
	struct tz_client_info *info;
	struct tz_sharedmem_info *shm_info;
	struct MTIOMMU_PIN_RANGE_T *pin;
	int i, num;

	info = (struct tz_client_info *)file->private_data;

	/* lock */
	mutex_lock(&info->mux);

	num = info->handle_num;
	for (i = 0; i < num; i++) {
		if (info->handles[i] == 0)
			continue;

		KREE_CloseSession(info->handles[i]);
		info->handles[i] = (KREE_SESSION_HANDLE) 0;
	}

	/* unregister shared memory */
	num = info->shm_info_num;
	for (i = 0; i < num; i++) {
		if (info->shm_info[i].mem_handle == 0)
			continue;

		shm_info = tz_get_sharedmem(info, info->shm_info[i].mem_handle);
		if (shm_info == NULL)
			continue;

		pin = (struct MTIOMMU_PIN_RANGE_T *) shm_info->resource;

		_unmap_user_pages(pin);
		kfree(pin);
	}

	/* unlock */
	file->private_data = 0;
	mutex_unlock(&info->mux);
	kfree(info->handles);
	kfree(info->shm_info);
	kfree(info);
}

/**************************************************************************
 *  DEV DRIVER IOCTL
 **************************************************************************/
static KREE_SESSION_HANDLE tz_client_open_session_impl(int with_tag,
				unsigned long arg,
				struct kree_session_tag_cmd_param *pparam)
{
	size_t param_size;
	int ret;
	unsigned long cret;
	char uuid[40];
	long len;
	KREE_SESSION_HANDLE handle;
	char tag[48] = { 0 };

	if (with_tag == 0)
		param_size = sizeof(struct kree_session_cmd_param);
	else
		param_size = sizeof(struct kree_session_tag_cmd_param);

	cret = copy_from_user(pparam, (void *)arg, param_size);
	if (cret)
		return -EFAULT;

	/* Check if can we access UUID string. 10 for min uuid len. */
	if (!access_ok(VERIFY_READ, pparam->data, 10))
		return -EFAULT;

	if (with_tag != 0 && pparam->tag != 0) {
		/* Check if can we access tag string. */
		if (!access_ok(VERIFY_READ, pparam->tag, pparam->tag_size))
			return -EFAULT;

		len = strncpy_from_user(tag,
					(void *)(unsigned long)pparam->tag,
					sizeof(tag));
		if (len <= 0)
			return -EFAULT;

		tag[sizeof(tag) - 1] = 0;
	}

	len = strncpy_from_user(uuid,
				(void *)(unsigned long)pparam->data,
				sizeof(uuid));
	if (len <= 0)
		return -EFAULT;

	uuid[sizeof(uuid) - 1] = 0;
	if (with_tag != 0 && pparam->tag != 0)
		ret = KREE_CreateSessionWithTag(uuid, &handle, tag);
	else
		ret = KREE_CreateSession(uuid, &handle);
	pparam->ret = ret;

	if (ret != TZ_RESULT_SUCCESS)
		return KREE_SESSION_HANDLE_FAIL;

	return handle;
}

static long tz_client_open_session(struct file *file, unsigned long arg)
{
	unsigned long cret;
	KREE_SESSION_HANDLE handle;
	struct kree_session_tag_cmd_param param;

	handle = tz_client_open_session_impl(0, arg, &param);

	if (handle != KREE_SESSION_HANDLE_FAIL &&
		handle != KREE_SESSION_HANDLE_NULL) {
		param.handle = tz_client_register_session(file, handle);
		if (param.handle < 0)
			goto error_register;
	}

	cret = copy_to_user((void *)arg, &param,
				sizeof(struct kree_session_cmd_param));
	if (cret)
		goto error_copy;

	return 0;

 error_copy:
	tz_client_unregister_session(file, param.handle);
 error_register:
	KREE_CloseSession(handle);
	return -EFAULT;
}

static long tz_client_open_session_with_tag(struct file *file,
						unsigned long arg)
{
	unsigned long cret;
	KREE_SESSION_HANDLE handle;
	struct kree_session_tag_cmd_param param;

	handle = tz_client_open_session_impl(1, arg, &param);

	if (handle != KREE_SESSION_HANDLE_FAIL &&
		handle != KREE_SESSION_HANDLE_NULL) {
		param.handle = tz_client_register_session(file, handle);
		if (param.handle < 0)
			goto error_register;
	}

	cret = copy_to_user((void *)arg, &param, sizeof(param));
	if (cret)
		goto error_copy;

	return 0;

 error_copy:
	tz_client_unregister_session(file, param.handle);
 error_register:
	KREE_CloseSession(handle);
	return -EFAULT;
}

static long tz_client_close_session(struct file *file, unsigned long arg)
{
	struct kree_session_cmd_param param;
	unsigned long cret;
	int ret;
	KREE_SESSION_HANDLE handle;

	cret = copy_from_user(&param, (void *)arg, sizeof(param));
	if (cret)
		return -EFAULT;

	handle = tz_client_get_session(file, param.handle);
	if (handle == 0)
		return -EINVAL;

	tz_client_unregister_session(file, param.handle);
	ret = KREE_CloseSession(handle);
	param.ret = ret;

	cret = copy_to_user((void *)arg, &param, sizeof(param));
	if (cret)
		return -EFAULT;

	return 0;
}

static long tz_client_tee_service(struct file *file, unsigned long arg,
					unsigned int compat)
{
	struct kree_tee_service_cmd_param cparam;
	unsigned long cret;
	uint32_t tmpTypes;
	union MTEEC_PARAM param[4], oparam[4];
	int i;
	int ret;
	KREE_SESSION_HANDLE handle;
	void __user *ubuf;
	uint32_t ubuf_sz;

	cret = copy_from_user(&cparam, (void *)arg, sizeof(cparam));
	if (cret)
		return -EFAULT;

	if (cparam.paramTypes != TZPT_NONE || cparam.param) {
		/* Check if can we access param */
		if (!access_ok(VERIFY_READ, cparam.param, sizeof(oparam)))
			return -EFAULT;

		cret = copy_from_user(oparam,
					(void *)(unsigned long)cparam.param,
					sizeof(oparam));
		if (cret)
			return -EFAULT;
	}
	/* Check handle */
	handle = tz_client_get_session(file, cparam.handle);
	if (handle <= 0)
		return -EINVAL;

	/* Parameter processing. */
	memset(param, 0, sizeof(param));
	tmpTypes = cparam.paramTypes;
	for (i = 0; tmpTypes; i++) {
		enum TZ_PARAM_TYPES type = tmpTypes & 0xff;

		tmpTypes >>= 8;
		switch (type) {
		case TZPT_VALUE_INPUT:
		case TZPT_VALUE_INOUT:
			param[i] = oparam[i];
			break;

		case TZPT_VALUE_OUTPUT:
		case TZPT_NONE:
			/* Nothing to do */
			break;

		case TZPT_MEM_INPUT:
		case TZPT_MEM_OUTPUT:
		case TZPT_MEM_INOUT:
#ifdef CONFIG_COMPAT
			if (compat) {
				ubuf = compat_ptr(oparam[i].mem32.buffer);
				ubuf_sz = oparam[i].mem32.size;
			} else
#endif
			{
				ubuf = oparam[i].mem.buffer;
				ubuf_sz = oparam[i].mem.size;
			}

			/* Mem Access check */
			if (type != TZPT_MEM_OUTPUT) {
				if (!access_ok(VERIFY_READ, ubuf,
					       ubuf_sz)) {
					cret = -EFAULT;
					goto error;
				}
			}
			if (type != TZPT_MEM_INPUT) {
				if (!access_ok(VERIFY_WRITE, ubuf,
					       ubuf_sz)) {
					cret = -EFAULT;
					goto error;
				}
			}
			/* Allocate kernel space memory. Fail if > 4kb */
			if (ubuf_sz > TEE_PARAM_MEM_LIMIT) {
				cret = -ENOMEM;
				goto error;
			}

			param[i].mem.size = ubuf_sz;
			param[i].mem.buffer = kmalloc(param[i].mem.size,
							GFP_KERNEL);
			if (!param[i].mem.buffer) {
				cret = -ENOMEM;
				goto error;
			}

			if (type != TZPT_MEM_OUTPUT) {
				cret = copy_from_user(param[i].mem.buffer,
						      ubuf,
						      param[i].mem.size);
				if (cret) {
					cret = -EFAULT;
					goto error;
				}
			}
			break;

		case TZPT_MEMREF_INPUT:
		case TZPT_MEMREF_OUTPUT:
		case TZPT_MEMREF_INOUT:
			/* Check if share memory is valid. */
			/* Not done yet. */
			param[i] = oparam[i];
			break;

		default:
			/* Bad format, return. */
			ret = TZ_RESULT_ERROR_BAD_FORMAT;
			goto error;
		}
	}

	/* Execute. */
	ret = KREE_TeeServiceCallNoCheck(handle, cparam.command,
						cparam.paramTypes, param);

	/* Copy memory back. */
	cparam.ret = ret;
	tmpTypes = cparam.paramTypes;
	for (i = 0; tmpTypes; i++) {
		enum TZ_PARAM_TYPES type = tmpTypes & 0xff;

		tmpTypes >>= 8;
		switch (type) {
		case TZPT_VALUE_OUTPUT:
		case TZPT_VALUE_INOUT:
			oparam[i] = param[i];
			break;

		default:
			/* This should never happen */
		case TZPT_MEMREF_INPUT:
		case TZPT_MEMREF_OUTPUT:
		case TZPT_MEMREF_INOUT:
		case TZPT_VALUE_INPUT:
		case TZPT_NONE:
			/* Nothing to do */
			break;

		case TZPT_MEM_INPUT:
		case TZPT_MEM_OUTPUT:
		case TZPT_MEM_INOUT:
#ifdef CONFIG_COMPAT
			if (compat)
				ubuf = compat_ptr(oparam[i].mem32.buffer);
			else
#endif
				ubuf = oparam[i].mem.buffer;

			if (type != TZPT_MEM_INPUT) {
				cret = copy_to_user(ubuf,
						param[i].mem.buffer,
						param[i].mem.size);
				if (cret) {
					cret = -EFAULT;
					goto error;
				}
			}

			kfree(param[i].mem.buffer);
			break;
		}
	}

	/* Copy data back. */
	if (cparam.paramTypes != TZPT_NONE) {
		cret = copy_to_user((void *)(unsigned long)cparam.param,
					oparam,
					sizeof(oparam));
		if (cret)
			return -EFAULT;
	}

	cret = copy_to_user((void *)arg, &cparam, sizeof(cparam));
	if (cret)
		return -EFAULT;
	return 0;

 error:
	tmpTypes = cparam.paramTypes;
	for (i = 0; tmpTypes; i++) {
		enum TZ_PARAM_TYPES type = tmpTypes & 0xff;

		tmpTypes >>= 8;
		switch (type) {
		case TZPT_MEM_INPUT:
		case TZPT_MEM_OUTPUT:
		case TZPT_MEM_INOUT:
			kfree(param[i].mem.buffer);
			break;

		default:
			/* Don't care. */
			break;
		}
	}

	return cret;
}

static long __tz_reg_sharedmem(struct file *file, unsigned long arg,
				struct kree_sharedmemory_tag_cmd_param *cparam)
{
	unsigned long cret;
	KREE_SESSION_HANDLE session;
	uint32_t mem_handle;
	struct MTIOMMU_PIN_RANGE_T *pin;
	uint64_t *map_p;
	int ret;
	struct page **page;
	int i;
	long errcode;
	unsigned long *pfns;
	char *tag = NULL;

	/* handle tag for debugging purpose */
	if (cparam->tag != 0 && cparam->tag_size != 0) {
		tag = kmalloc(cparam->tag_size+1, GFP_KERNEL);
		if (tag == NULL)
			return -ENOMEM;
		cret = copy_from_user(tag,
					(void *)(unsigned long)cparam->tag,
					cparam->tag_size);
		if (cret) {
			kfree(tag);
			return -EFAULT;
		}
		tag[cparam->tag_size] = '\0';
	}

	/* session handle */
	session = tz_client_get_session(file, cparam->session);
	if (session <= 0) {
		kfree(tag);
		return -EINVAL;
	}

	/* map pages
	 */
	/* 1. get user pages */
	/* note: 'pin' resource need to keep for unregister.
	 * It will be freed after unregisted.
	 */
	pin = kzalloc(sizeof(struct MTIOMMU_PIN_RANGE_T), GFP_KERNEL);
	if (pin == NULL) {
		errcode = -ENOMEM;
		goto client_regshm_mapfail;
	}
	cret = _map_user_pages(pin, (unsigned long)cparam->buffer,
				cparam->size, cparam->control);
	if (cret != 0) {
		pr_warn("tz_client_reg_sharedmem fail: map user pages = 0x%x\n",
			(uint32_t) cret);
		errcode = -EFAULT;
		goto client_regshm_mapfail_1;
	}
	/* 2. build PA table */
	map_p = kzalloc(sizeof(uint64_t) * (pin->nrPages + 1), GFP_KERNEL);
	if (map_p == NULL) {
		errcode = -ENOMEM;
		goto client_regshm_mapfail_2;
	}
	map_p[0] = pin->nrPages;
	if (pin->isPage) {
		page = (struct page **)pin->pageArray;
		for (i = 0; i < pin->nrPages; i++) {
			map_p[1 + i] = PFN_PHYS(page_to_pfn(page[i])); /* PA */
			/* pr_debug("tz_client_reg_sharedmem ---> 0x%x\n",
			 * map_p[1+i]);
			 */
		}
	} else { /* pfn */
		pfns = (unsigned long *)pin->pageArray;
		for (i = 0; i < pin->nrPages; i++) {
			map_p[1 + i] = PFN_PHYS(pfns[i]);	/* get PA */
			/* pr_debug("tz_client_reg_sharedmem ---> 0x%x\n",
			 * map_p[1+i]);
			 */
		}
	}

	/* register it ...
	 */
	ret = kree_register_sharedmem(session, &mem_handle, pin->start,
					pin->size, (void *)map_p, tag);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("tz_client_reg_sharedmem fail: register shm 0x%x\n",
			ret);
		kfree(map_p);
		kfree(tag);
		errcode = -EFAULT;
		goto client_regshm_free;
	}
	kfree(map_p);
	kfree(tag);

	/* register to fd
	 */
	cret = tz_client_register_sharedmem(file, session,
					(KREE_SHAREDMEM_HANDLE) mem_handle,
					(uint32_t *) pin);
	if (cret < 0) {
		pr_warn("tz_client_reg_sharedmem fail: register fd 0x%x\n",
			(uint32_t) cret);
		errcode = -EFAULT;
		goto client_regshm_free_1;
	}

	cparam->mem_handle = mem_handle;
	cparam->ret = ret;	/* TEE service call return */
	cret = copy_to_user((void *)arg, cparam,
				sizeof(struct kree_sharedmemory_cmd_param));
	if (cret)
		return -EFAULT;

	return TZ_RESULT_SUCCESS;

client_regshm_free_1:
	kree_unregister_sharedmem(session, mem_handle);
client_regshm_free:
	_unmap_user_pages(pin);
	kfree(pin);
	cparam->ret = ret;	/* TEE service call return */
	cret = copy_to_user((void *)arg, cparam,
				sizeof(struct kree_sharedmemory_cmd_param));
	pr_warn("tz_client_reg_sharedmem fail: shm reg\n");
	return errcode;

client_regshm_mapfail_2:
	_unmap_user_pages(pin);
client_regshm_mapfail_1:
	kfree(pin);
client_regshm_mapfail:
	kfree(tag);
	pr_warn("tz_client_reg_sharedmem fail: map memory\n");
	return errcode;
}

static long tz_client_reg_sharedmem_with_tag(struct file *file,
						unsigned long arg)
{
	unsigned long cret;
	struct kree_sharedmemory_tag_cmd_param cparam;

	cret = copy_from_user(&cparam, (void *)arg, sizeof(cparam));
	if (cret)
		return -EFAULT;

	if (cparam.tag_size > MAX_TAG_SIZE)
		cparam.tag_size = MAX_TAG_SIZE;

	return __tz_reg_sharedmem(file, arg, &cparam);
}

static long tz_client_reg_sharedmem(struct file *file, unsigned long arg)
{
	unsigned long cret;
	struct kree_sharedmemory_tag_cmd_param cparam;

	cret = copy_from_user(&cparam, (void *)arg,
			sizeof(struct kree_sharedmemory_cmd_param));
	if (cret)
		return -EFAULT;

	cparam.tag = 0;
	cparam.tag_size = 0;

	return __tz_reg_sharedmem(file, arg, &cparam);
}

static long tz_client_unreg_sharedmem(struct file *file, unsigned long arg)
{
	unsigned long cret;
	struct kree_sharedmemory_cmd_param cparam;
	KREE_SESSION_HANDLE session;
	int ret;

	cret = copy_from_user(&cparam, (void *)arg, sizeof(cparam));
	if (cret)
		return -EFAULT;

	/* session handle */
	session = tz_client_get_session(file, cparam.session);
	if (session <= 0)
		return -EINVAL;

	/* Unregister
	 */
	ret = kree_unregister_sharedmem(session, (uint32_t) cparam.mem_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("%s: 0x%x\n", __func__, ret);
		cparam.ret = ret;
		cret = copy_to_user((void *)arg, &cparam, sizeof(cparam));
		return -EFAULT;
	}

	/* unmap user pages and unregister to fd
	 */
	ret = tz_client_unregister_sharedmem(file, cparam.mem_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("%s: unregister shm = 0x%x\n", __func__, ret);
		return -EFAULT;
	}

	cparam.ret = ret;
	cret = copy_to_user((void *)arg, &cparam, sizeof(cparam));
	if (cret)
		return -EFAULT;

	return TZ_RESULT_SUCCESS;
}



static long do_tz_client_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg, unsigned int compat)
{
	int err = 0;

	/* ---------------------------------- */
	/* IOCTL                              */
	/* ---------------------------------- */
	if (_IOC_TYPE(cmd) != MTEE_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > DEV_IOC_MAXNR)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err |= !access_ok(VERIFY_WRITE, (void __user *)arg,
					_IOC_SIZE(cmd));
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		err |= !access_ok(VERIFY_READ, (void __user *)arg,
					_IOC_SIZE(cmd));

	if (err)
		return -EFAULT;

	switch (cmd) {
	case MTEE_CMD_OPEN_SESSION:
		return tz_client_open_session(file, arg);

	case MTEE_CMD_OPEN_SESSION_WITH_TAG:
		return tz_client_open_session_with_tag(file, arg);

	case MTEE_CMD_CLOSE_SESSION:
		return tz_client_close_session(file, arg);

	case MTEE_CMD_TEE_SERVICE:
		return tz_client_tee_service(file, arg, compat);

	case MTEE_CMD_SHM_REG:
		return tz_client_reg_sharedmem(file, arg);

	case MTEE_CMD_SHM_REG_WITH_TAG:
		return tz_client_reg_sharedmem_with_tag(file, arg);

	case MTEE_CMD_SHM_UNREG:
		return tz_client_unreg_sharedmem(file, arg);

	default:
		return -ENOTTY;
	}

	return 0;
}


static long tz_client_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	return do_tz_client_ioctl(file, cmd, arg, 0);
}

#ifdef CONFIG_COMPAT
static long tz_client_ioctl_compat(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	return do_tz_client_ioctl(file, cmd, (unsigned long)compat_ptr(arg), 1);
}
#endif


/* pm op funcstions */
#ifdef TZ_PLAYREADY_SECURETIME_SUPPORT
#ifdef CONFIG_HAS_EARLYSUSPEND
static int securetime_savefile(void)
{
	int ret = 0;

	KREE_SESSION_HANDLE securetime_session = 0;

	ret = KREE_CreateSession(TZ_TA_PLAYREADY_UUID, &securetime_session);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("[securetime]CreateSession error %d\n", ret);

	TEE_update_pr_time_infile(securetime_session);

	ret = KREE_CloseSession(securetime_session);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("CloseSession error %d\n", ret);


	return ret;
}
#endif /* EARLYSUSPEND */

static void st_shutdown(struct platform_device *pdev)
{
	pr_info("[securetime]%s: kickoff\n", __func__);
}
#endif

#ifdef CONFIG_PM_SLEEP
static int tz_suspend(struct device *pdev)
{
	int tzret;

	tzret = kree_pm_device_ops(MTEE_SUSPEND);
	return (tzret != TZ_RESULT_SUCCESS) ? (-EBUSY) : (0);
}

static int tz_resume(struct device *pdev)
{
	int tzret;

	tzret = kree_pm_device_ops(MTEE_RESUME);
	return (tzret != TZ_RESULT_SUCCESS) ? (-EBUSY) : (0);
}
#endif

static int tz_suspend_late(struct device *pdev)
{
	int tzret;

	tzret = kree_pm_device_ops(MTEE_SUSPEND_LATE);
	return (tzret != TZ_RESULT_SUCCESS) ? (-EBUSY) : (0);
}

static int tz_resume_early(struct device *pdev)
{
	int tzret;

	tzret = kree_pm_device_ops(MTEE_RESUME_EARLY);
	return (tzret != TZ_RESULT_SUCCESS) ? (-EBUSY) : (0);
}

static const struct dev_pm_ops tz_pm_ops = {
	.suspend_late = tz_suspend_late,
	.freeze_late = tz_suspend_late,
	.resume_early = tz_resume_early,
	.thaw_early = tz_resume_early,
	SET_SYSTEM_SLEEP_PM_OPS(tz_suspend, tz_resume)
};

static struct class *pTzClass;
static struct device *pTzDevice;

#ifdef CONFIG_CMA
static struct cma *tz_cma;
#endif  /* CONFIG_CMA */

#ifdef CONFIG_MTEE_CMA_SECURE_MEMORY
static struct page *secure_pages;
static size_t secure_size;

/* TEE chunk memory allocate by REE service
 */
int KREE_ServGetChunkmemPool(u32 op,
			u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
	struct ree_service_chunk_mem *chunkmem;

	if (!tz_cma)
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;

	/* get parameters */
	chunkmem = (struct ree_service_chunk_mem *)uparam;

	if (secure_pages != NULL) {
		pr_warn("%s() already allocated!\n", __func__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	secure_size = cma_get_size(tz_cma);
	secure_pages = cma_alloc_large(tz_cma, secure_size/SZ_4K,
					get_order(SZ_1M));
	if (secure_pages == NULL) {
		pr_warn("%s() cma_alloc() failed!\n", __func__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}
	chunkmem->size = secure_size;
	chunkmem->chunkmem_pa = (uint64_t)page_to_phys(secure_pages);

	pr_info("%s() get @%llx [0x%zx]\n", __func__,
			chunkmem->chunkmem_pa, secure_size);

	/* flush cache to avoid writing secure memory after allocation. */
	smp_inner_dcache_flush_all();

	return TZ_RESULT_SUCCESS;
}

int KREE_ServReleaseChunkmemPool(u32 op,
			u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
	if (secure_pages != NULL) {
		phys_addr_t addr = page_to_phys(secure_pages);

		cma_release(tz_cma, secure_pages,
				cma_get_size(tz_cma)>>PAGE_SHIFT);
		pr_info("%s() release @%pax [0x%zx]\n", __func__,
				&addr, secure_size);
		secure_pages = NULL;
		secure_size = 0;
		return TZ_RESULT_SUCCESS;
	}
	return TZ_RESULT_ERROR_GENERIC;
}

#ifndef NO_CMA_RELEASE_THROUGH_SHRINKER_FOR_EARLY_STAGE

int KREE_TeeReleseChunkmemPool(void)
{
	int ret;
	KREE_SESSION_HANDLE mem_session;

	ret = KREE_CreateSession(TZ_TA_MEM_UUID, &mem_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("Create memory session error %d\n", ret);
		return ret;
	}

	ret = KREE_TeeServiceCall(mem_session, TZCMD_MEM_RELEASE_SECURECM,
					TZPT_NONE, NULL);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("Release Secure CM failed, ret = %d\n", ret);

	KREE_CloseSession(mem_session);

	return ret;
}

DECLARE_COMPLETION(tz_cm_shrinker_work);
DECLARE_COMPLETION(tz_cm_shrinker_finish_work);

int tz_cm_shrinker_thread(void *data)
{
	do {
		if (wait_for_completion_interruptible(
						&tz_cm_shrinker_work) != 0)
			continue;

		/* TeeReleaseChunkmemPool will call     */
		/* ree-service call to do cma_release() */
		if (KREE_TeeReleseChunkmemPool() != TZ_RESULT_SUCCESS)
			pr_warn("Can't free tz chunk memory\n");

		complete(&tz_cm_shrinker_finish_work);
	} while (1);
}

static int KREE_IsTeeChunkmemPoolReleasable(int *releasable)
{
	int ret;
	KREE_SESSION_HANDLE mem_session;
	union MTEEC_PARAM param[4];

	if (releasable == NULL)
		return TZ_RESULT_ERROR_BAD_FORMAT;

	ret = KREE_CreateSession(TZ_TA_MEM_UUID, &mem_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("Create memory session error %d\n", ret);
		return ret;
	}

	ret = KREE_TeeServiceCall(mem_session, TZCMD_MEM_USAGE_SECURECM,
					TZPT_VALUE_OUTPUT, param);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("Is Secure CM releasable failed, ret = %d\n", ret);

	*releasable = param[0].value.a;

	KREE_CloseSession(mem_session);

	return ret;
}

static unsigned long tz_cm_count(struct shrinker *s,
						struct shrink_control *sc)
{
	int releasable;

	if (secure_pages == NULL)
		return 0;

	if (KREE_IsTeeChunkmemPoolReleasable(&releasable) != TZ_RESULT_SUCCESS)
		return 0;

	if (releasable == 0)
		return 0;

	return secure_size / SZ_4K;
}
static unsigned long tz_cm_scan(struct shrinker *s,
						struct shrink_control *sc)
{
	unsigned long release_objects_count = secure_size;

	if (secure_pages == NULL)
		return SHRINK_STOP;

	complete(&tz_cm_shrinker_work);
	wait_for_completion(&tz_cm_shrinker_finish_work);

	if (secure_pages != NULL)
		return SHRINK_STOP;

	return release_objects_count;
}

static struct shrinker tz_cm_shrinker = {
	.scan_objects = tz_cm_scan,
	.count_objects = tz_cm_count,
	.seeks = DEFAULT_SEEKS
};

#else

static int KREE_TeeTriggerChunkmemAllocation(void)
{
	int ret;
	KREE_SESSION_HANDLE mem_session;
	KREE_SECUREMEM_HANDLE cm_handle;

	ret = KREE_CreateSession(TZ_TA_MEM_UUID, &mem_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("Create memory session error %d\n", ret);
		return ret;
	}

	ret = KREE_AllocSecurechunkmem(mem_session, &cm_handle, 1, 4096);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("Allocate securechunkmem error %d\n", ret);
		return ret;
	}

	ret = KREE_UnreferenceSecurechunkmem(mem_session, cm_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("Unreference securechunkmem error %d\n", ret);
		return ret;
	}

	KREE_CloseSession(mem_session);

	return ret;
}
#endif  /* NO_CMA_RELEASE_THROUGH_SHRINKER_FOR_EARLY_STAGE */

#endif

static int mtee_probe(struct platform_device *pdev)
{
	int ret;
	int tzret;
#ifdef ENABLE_INC_ONLY_COUNTER
	struct task_struct *thread;
#endif
#ifdef TZ_SECURETIME_SUPPORT
	struct task_struct *thread_securetime_gb;
#endif
#ifdef TZ_PLAYREADY_SECURETIME_SUPPORT
	struct task_struct *thread_securetime;
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&securetime_early_suspend);
#endif
#endif

#if defined(CONFIG_MTEE_CMA_SECURE_MEMORY) && \
	!defined(NO_CMA_RELEASE_THROUGH_SHRINKER_FOR_EARLY_STAGE)
	struct task_struct *thread_tz_cm_shrinker;
#endif

#ifdef CONFIG_OF
	struct device_node *parent_node;

	if (pdev->dev.of_node) {
		parent_node = of_irq_find_parent(pdev->dev.of_node);
		if (parent_node)
			kree_set_sysirq_node(parent_node);
		else
			pr_warn("can't find interrupt-parent device node from mtee\n");
	} else
		pr_warn("No mtee device node\n");

	mtee_clks_init(pdev);
	mtee_pms_init(pdev);
#endif /* CONFIG_OF */

	tz_client_dev = MKDEV(MAJOR_DEV_NUM, 0);

	pr_debug(" init\n");

	ret = register_chrdev_region(tz_client_dev, 1, TZ_DEV_NAME);
	if (ret) {
		pr_warn("[%s] register device failed, ret:%d\n",
			MODULE_NAME, ret);
		return ret;
	}

	/* initialize the device structure and register the device  */
	cdev_init(&tz_client_cdev, &tz_client_fops);
	tz_client_cdev.owner = THIS_MODULE;

	ret = cdev_add(&tz_client_cdev, tz_client_dev, 1);
	if (ret < 0) {
		pr_warn("[%s] could not allocate chrdev for the device, ret:%d\n",
			MODULE_NAME,
			ret);
		return ret;
	}

	tzret = KREE_InitTZ();
	if (tzret != TZ_RESULT_SUCCESS) {
		pr_warn("tz_client_init: TZ Failed %d\n", (int)tzret);
		WARN_ON(tzret != TZ_RESULT_SUCCESS);
	}

	kree_irq_init();
	kree_pm_init();

#ifdef ENABLE_INC_ONLY_COUNTER
	thread = kthread_run(update_counter_thread, NULL, "update_tz_counter");
#endif

	pr_debug("tz_client_init: successfully\n");

	/* tz_test(); */

	/* create /dev/trustzone automatically */
	pTzClass = class_create(THIS_MODULE, TZ_DEV_NAME);
	if (IS_ERR(pTzClass)) {
		int ret = PTR_ERR(pTzClass);

		pr_warn("[%s] could not create class for the device, ret:%d\n",
			MODULE_NAME,
			ret);
		return ret;
	}
	pTzDevice = device_create(pTzClass, NULL, tz_client_dev, NULL,
					TZ_DEV_NAME);
#ifdef TZ_PLAYREADY_SECURETIME_SUPPORT
	thread_securetime = kthread_run(update_securetime_thread, NULL,
						"update_securetime");
#endif

#ifdef TZ_SECURETIME_SUPPORT
	thread_securetime_gb = kthread_run(update_securetime_thread_gb, NULL,
						"update_securetime_gb");
#endif

#ifdef CONFIG_MTEE_CMA_SECURE_MEMORY
#ifndef NO_CMA_RELEASE_THROUGH_SHRINKER_FOR_EARLY_STAGE
	thread_tz_cm_shrinker = kthread_run(tz_cm_shrinker_thread, NULL,
						"tz_cm_shrinker");
	register_shrinker(&tz_cm_shrinker);
#endif  /* NO_CMA_RELEASE_THROUGH_SHRINKER_FOR_EARLY_STAGE */
#endif  /* CONFIG_MTEE_CMA_SECURE_MEMORY */

	return 0;
}

static const struct of_device_id mtee_of_match[] = {
	{ .compatible = "mediatek,mtee", },
	{}
};
MODULE_DEVICE_TABLE(of, mtee_of_match);

/* add tz virtual driver for suspend/resume support */
static struct platform_driver tz_driver = {
	.probe = mtee_probe,
	.driver = {
		.name = TZ_DEVNAME,
		.pm = &tz_pm_ops,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mtee_of_match,
#endif
	},
#ifdef TZ_PLAYREADY_SECURETIME_SUPPORT
	.shutdown   = st_shutdown,
#endif
};

/* CMA */
#ifdef CONFIG_CMA
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_fdt.h>

static int __init rmem_tz_sec_setup(struct reserved_mem *rmem)
{
	unsigned long node = rmem->fdt_node;
	struct cma *cma;
	int err;

	if (!of_get_flat_dt_prop(node, "reusable", NULL) ||
	    of_get_flat_dt_prop(node, "no-map", NULL))
		return -EINVAL;

	if (!rmem->size) {
		pr_err("%s: size == 0\n", __func__);
		return -EINVAL;
	}

	err = cma_init_reserved_mem(rmem->base, rmem->size, 0, "tz_cma", &cma);
	if (err) {
		pr_err("Reserved memory: unable to setup CMA region\n");
		return err;
	}

	tz_cma = cma;
	pr_info("Reserved memory: created CMA memory pool at %pa, size %ld MiB\n",
		&rmem->base, (unsigned long)rmem->size / SZ_1M);

	return 0;
}
RESERVEDMEM_OF_DECLARE(tz_sec, "shared-secure-pool", rmem_tz_sec_setup);
#endif  /* CONFIG_CMA */

/******************************************************************************
 * register_tz_driver
 *
 * DESCRIPTION:
 *   register the device driver !
 *
 * PARAMETERS:
 *   None
 *
 * RETURNS:
 *   0 for success
 *
 * NOTES:
 *   None
 *
 ******************************************************************************/
static int __init register_tz_driver(void)
{
	int ret = 0;

#ifndef CONFIG_OF
	if (platform_device_register(&tz_device)) {
		ret = -ENODEV;
		pr_warn("[%s] could not register device for the device, ret:%d\n",
			MODULE_NAME,
			ret);
		return ret;
	}
#endif

	if (platform_driver_register(&tz_driver)) {
		ret = -ENODEV;
		pr_warn("[%s] could not register device for the device, ret:%d\n",
			MODULE_NAME,
			ret);
#ifndef CONFIG_OF
		platform_device_unregister(&tz_device);
#endif
		return ret;
	}

	return ret;
}
#ifdef TZ_PLAYREADY_SECURETIME_SUPPORT

#ifdef CONFIG_HAS_EARLYSUSPEND

static void st_early_suspend(struct early_suspend *h)
{
	pr_debug("%s: start\n", __func__);
	securetime_savefile();
}

static void st_late_resume(struct early_suspend *h)
{
	int ret = 0;
	KREE_SESSION_HANDLE securetime_session = 0;

	ret = KREE_CreateSession(TZ_TA_PLAYREADY_UUID, &securetime_session);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("[securetime]CreateSession error %d\n", ret);

	TEE_update_pr_time_intee(securetime_session);
	ret = KREE_CloseSession(securetime_session);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("[securetime]CloseSession error %d\n", ret);
}

static struct early_suspend securetime_early_suspend = {
	.level  = 258,
	.suspend = st_early_suspend,
	.resume  = st_late_resume,
};
#endif
#endif

/******************************************************************************
 * tz_client_init
 *
 * DESCRIPTION:
 *   Init the device driver !
 *
 * PARAMETERS:
 *   None
 *
 * RETURNS:
 *   0 for success
 *
 * NOTES:
 *   None
 *
 ******************************************************************************/
static int __init tz_client_init(void)
{
	int ret = 0;

	ret = register_tz_driver();
	if (ret) {
		pr_warn("[%s] register device/driver failed, ret:%d\n",
			MODULE_NAME,
			ret);
		return ret;
	}

	return 0;
}
#ifdef CONFIG_TRUSTY
device_initcall(tz_client_init);
#else
arch_initcall(tz_client_init);
#endif

#ifdef NO_CMA_RELEASE_THROUGH_SHRINKER_FOR_EARLY_STAGE
static int __init tz_client_cma_alloc_init(void)
{
	/* trigger allocation chunk memory in initialization flow */
	KREE_TeeTriggerChunkmemAllocation();
	return 0;
}
/* use late_initcall to be triggered after M4U init is done. */
late_initcall(tz_client_cma_alloc_init);
#endif
