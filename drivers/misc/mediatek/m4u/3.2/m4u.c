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

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched/clock.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/fb.h>
/* #include <linux/earlysuspend.h> */
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <mt-plat/sync_write.h>
#include <asm/cacheflush.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/dma-direction.h>
#include <asm/page.h>
#include <linux/proc_fs.h>
#ifndef CONFIG_ARM64
#include "mm/dma.h"
#endif

#include "m4u_priv.h"
#include "m4u.h"
#include "m4u_hw.h"
#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>


#ifdef M4U_TEE_SERVICE_ENABLE
#include "tz_m4u.h"

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) ||\
	defined(CONFIG_MICROTRUST_TEE_SUPPORT)
#include "tee_client_api.h"
#endif

#ifdef __M4U_SECURE_SYSTRACE_ENABLE__
#include <linux/sectrace.h>
#endif


#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
#include "isee_kernel_api.h"
#define DRM_M4U_DRV_DRIVER_ID   (0x977aa)
#endif

int m4u_tee_en;

#endif

#ifdef M4U_GZ_SERVICE_ENABLE
#include "m4u_sec_gz.h"
#endif

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/uaccess.h>
#include <linux/compat.h>
#endif

static struct m4u_buf_info_t gMvaNode_unknown = {
	.va = 0,
	.mva = 0,
	.size = 0,
	.port = M4U_PORT_UNKNOWN,
};




/* ----------Global variables---- */
#ifdef M4U_PROFILE
mmp_event M4U_MMP_Events[M4U_MMP_MAX];
#endif

#define M4U_DEV_NAME "m4u"
struct m4u_device *gM4uDev;

static int m4u_buf_show(void *priv,
	unsigned int mva_start, unsigned int mva_end, void *data)
{
	struct m4u_buf_info_t *pMvaInfo = priv;

	M4U_PRINT_SEQ(data,
		"0x%-8x, 0x%-8x, 0x%lx, 0x%-8x, 0x%x, %s, 0x%x, 0x%x, 0x%x, %llu ns\n",
		pMvaInfo->mva, pMvaInfo->mva+pMvaInfo->size-1, pMvaInfo->va,
		pMvaInfo->size, pMvaInfo->prot,
		m4u_get_port_name(pMvaInfo->port),
		pMvaInfo->flags, mva_start, mva_end,
		pMvaInfo->current_ts);

	return 0;
}


int m4u_dump_buf_info(struct seq_file *seq, unsigned int domain_idx)
{

	M4U_PRINT_SEQ(seq, "\ndump mva allocated info ========>\n");
	M4U_PRINT_SEQ(seq,
	"mva_start   mva_end          va       size     prot   module   flags   debug1  debug2  ts\n");

	mva_foreach_priv((void *) m4u_buf_show, seq, domain_idx);

	M4U_PRINT_SEQ(seq, " dump mva allocated info done ========>\n");
	return 0;
}

#ifdef M4U_PROFILE
static void m4u_profile_init(void)
{
	mmp_event M4U_Event;

	mmprofile_enable(1);
	M4U_Event = mmprofile_register_event(MMP_ROOT_EVENT, "M4U");
	/* register events */
	M4U_MMP_Events[M4U_MMP_ALLOC_MVA] =
			mmprofile_register_event(M4U_Event, "Alloc MVA");
	M4U_MMP_Events[M4U_MMP_DEALLOC_MVA] =
			mmprofile_register_event(M4U_Event, "DeAlloc MVA");
	M4U_MMP_Events[M4U_MMP_CONFIG_PORT] =
			mmprofile_register_event(M4U_Event, "Config Port");
	M4U_MMP_Events[M4U_MMP_M4U_ERROR] =
			mmprofile_register_event(M4U_Event, "M4U ERROR");
	M4U_MMP_Events[M4U_MMP_CACHE_SYNC] =
			mmprofile_register_event(M4U_Event, "M4U_CACHE_SYNC");
	M4U_MMP_Events[M4U_MMP_TOGGLE_CG] =
			mmprofile_register_event(M4U_Event, "M4U_Toggle_CG");

	/* enable events by default */
	mmprofile_enable_event(M4U_MMP_Events[M4U_MMP_ALLOC_MVA], 1);
	mmprofile_enable_event(M4U_MMP_Events[M4U_MMP_DEALLOC_MVA], 1);
	mmprofile_enable_event(M4U_MMP_Events[M4U_MMP_CONFIG_PORT], 1);
	mmprofile_enable_event(M4U_MMP_Events[M4U_MMP_M4U_ERROR], 1);
	mmprofile_enable_event(M4U_MMP_Events[M4U_MMP_CACHE_SYNC], 1);
	/* mmprofile_enable_event(M4U_MMP_Events[M4U_MMP_TOGGLE_CG], 0); */
	mmprofile_start(1);
}
#endif

/* get ref count on all pages in sgtable */
int m4u_get_sgtable_pages(struct sg_table *table)
{
	return 0;
}

/* put ref count on all pages in sgtable */
int m4u_put_sgtable_pages(struct sg_table *table)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);

		if (page)
			put_page(page);
	}
	return 0;
}

static struct m4u_buf_info_t *m4u_alloc_buf_info(void)
{
	struct m4u_buf_info_t *pList = NULL;

	pList = kzalloc(sizeof(struct m4u_buf_info_t), GFP_KERNEL);
	if (pList == NULL) {
		M4UMSG("m4u_client_add_buf(), pList=0x%p\n", pList);
		return NULL;
	}

	INIT_LIST_HEAD(&(pList->link));
	return pList;
}

static int m4u_free_buf_info(struct m4u_buf_info_t *pList)
{
	kfree(pList);
	return 0;
}

static int m4u_client_add_buf(struct m4u_client_t *client,
	struct m4u_buf_info_t *pList)
{
	mutex_lock(&(client->dataMutex));
	list_add(&(pList->link), &(client->mvaList));
	mutex_unlock(&(client->dataMutex));

	return 0;
}


/*static int m4u_client_del_buf(struct m4u_client_t *client,
 *struct m4u_buf_info_t *pList)
 */
/*{*/
/*    mutex_lock(&(client->dataMutex));*/
/*    list_del(&(pList->link));*/
/*    mutex_unlock(&(client->dataMutex));*/

/*    return 0;*/
/*}*/


/***********************************************************/
/** find or delete a buffer from client list
 * @param   client   -- client to be searched
 * @param   mva      -- mva to be searched
 * @param   del      -- should we del this buffer from client?
 *
 * @return buffer_info if found, NULL on fail
 * @remark
 * @see
 * @to-do    we need to add multi domain support here.
 * @author K Zhang      @date 2013/11/14
 */
/************************************************************/
static struct m4u_buf_info_t *m4u_client_find_buf(
		struct m4u_client_t *client,
		unsigned int mva, int del,
		unsigned int domain)
{
	struct list_head *pListHead;
	struct m4u_buf_info_t *pList = NULL;
	struct m4u_buf_info_t *ret = NULL;

	if (client == NULL) {
		M4UERR(
			"m4u_delete_from_garbage_list(), client is NULL! domain=%u\n",
				domain);
		m4u_dump_buf_info(NULL, domain);
		return NULL;
	}

	mutex_lock(&(client->dataMutex));
	list_for_each(pListHead, &(client->mvaList)) {
		pList = container_of(pListHead,
				struct m4u_buf_info_t, link);
		if (pList->mva == mva && pList->domain_idx == domain)
			break;
	}
	if (pListHead == &(client->mvaList)) {
		ret = NULL;
	} else {
		if (del)
			list_del(pListHead);
		ret = pList;
	}

	mutex_unlock(&(client->dataMutex));

	return ret;
}


/*dump buf info in client*/
/*static void m4u_client_dump_buf(
 *struct m4u_client_t *client, const char *pMsg)
 */
/*{*/
/*    struct m4u_buf_info_t *pList;*/
/*    struct list_head *pListHead;*/

/*    M4UMSG("print mva list [%s] ===============>\n", pMsg);*/
/*    mutex_lock(&(client->dataMutex));*/
/*    list_for_each(pListHead, &(client->mvaList))*/
/*    {*/
/*	pList = container_of(pListHead, struct m4u_buf_info_t, link);*/
/*	M4UMSG("port=%s, va=0x%x, size=0x%x, mva=0x%x, prot=%d\n",*/
/*	m4u_get_port_name
 *	(pList->port), pList->va, pList->size, pList->mva, pList->prot);
 */
/*    }*/
/*   mutex_unlock(&(client->dataMutex));*/

/*    M4UMSG("print mva list done ==========================>\n");*/
/*}*/


struct m4u_client_t *m4u_create_client(void)
{
	struct m4u_client_t *client;

	client = kmalloc(sizeof(struct m4u_client_t),
			GFP_ATOMIC);
	if (!client)
		return NULL;

	mutex_init(&(client->dataMutex));
	mutex_lock(&(client->dataMutex));
	client->open_pid = current->pid;
	client->open_tgid = current->tgid;
	INIT_LIST_HEAD(&(client->mvaList));
	mutex_unlock(&(client->dataMutex));

	return client;
}

int m4u_destroy_client(struct m4u_client_t *client)
{
	struct m4u_buf_info_t *pMvaInfo;
	unsigned int mva, size;
	int port;

	while (1) {
		mutex_lock(&(client->dataMutex));
		if (list_empty(&client->mvaList)) {
			mutex_unlock(&(client->dataMutex));
			break;
		}
		pMvaInfo = container_of(client->mvaList.next,
				struct m4u_buf_info_t, link);
		M4UMSG("warnning: clean garbage at m4u close\n");
		M4UMSG("module=%s,va=0x%lx,mva=0x%x,size=%d\n",
			m4u_get_port_name(pMvaInfo->port),
			pMvaInfo->va, pMvaInfo->mva,
			pMvaInfo->size);

		port = pMvaInfo->port;
		mva = pMvaInfo->mva;
		size = pMvaInfo->size;

		mutex_unlock(&(client->dataMutex));

		m4u_reclaim_notify(port, mva, size);

		/* m4u_dealloc_mva will lock client->dataMutex again */
		m4u_dealloc_mva(client, port, mva);
	}

	kfree(client);

	return 0;
}

static int m4u_dump_mmaps(unsigned long addr)
{
	struct vm_area_struct *vma;

	M4ULOG_MID("addr=0x%lx, name=%s, pid=0x%x,",
			addr, current->comm, current->pid);

	vma = find_vma(current->mm, addr);

	if (vma && (addr >= vma->vm_start)) {
		M4ULOG_MID("find vma: 0x%16lx-0x%16lx, flags=0x%lx\n",
			   (vma->vm_start), (vma->vm_end), vma->vm_flags);
		return 0;
	}

	M4UMSG("cannot find vma for addr 0x%lx\n", addr);
	return -1;
}

/* to-do: need modification to support 4G DRAM */
static phys_addr_t m4u_user_v2p(unsigned long va)
{
	unsigned long pageOffset = (va & (PAGE_SIZE - 1));
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	phys_addr_t pa;

	if (current == NULL) {
		M4UMSG("warning: %s, current is NULL!\n", __func__);
		return 0;
	}
	if (current->mm == NULL) {
		M4UMSG("warning: %s, current->mm is NULL!\n", __func__);
		M4UMSG("tgid=0x%x, name=%s\n",
		       current->tgid, current->comm);
		return 0;
	}

	pgd = pgd_offset(current->mm, va);	/* what is tsk->mm */
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		M4UMSG("%s(), va=0x%lx, pgd invalid!\n", __func__, va);
		return 0;
	}

	pud = pud_offset(pgd, va);
	if (pud_none(*pud) || pud_bad(*pud)) {
		M4UMSG("%s(), va=0x%lx, pud invalid!\n", __func__, va);
		return 0;
	}

	pmd = pmd_offset(pud, va);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		M4UMSG("%s(), va=0x%lx, pmd invalid!\n", __func__, va);
		return 0;
	}

	pte = pte_offset_map(pmd, va);
	if (pte_present(*pte)) {
		/* pa=(pte_val(*pte) & (PAGE_MASK)) | pageOffset; */
		pa = (pte_val(*pte) &
			(PHYS_MASK) & (~((phys_addr_t) 0xfff))) |
				pageOffset;
		pte_unmap(pte);
		return pa;
	}

	pte_unmap(pte);

	M4UMSG("%s(), va=0x%lx, pte invalid!\n", __func__, va);
	return 0;
}


static int m4u_fill_sgtable_user(
				struct vm_area_struct *vma,
				unsigned long va, int page_num,
				 struct scatterlist **pSg, int has_page)
{
	unsigned long va_align;
	phys_addr_t pa = 0;
	int i;
	long ret = 0;
	struct scatterlist *sg = *pSg;
	struct page *pages = NULL;
	int gup_flags;

	va_align = round_down(va, PAGE_SIZE);
	gup_flags = FOLL_TOUCH | FOLL_POPULATE | FOLL_MLOCK;
	if (vma->vm_flags & VM_LOCKONFAULT)
		gup_flags &= ~FOLL_POPULATE;
	/*
	 * We want to touch writable mappings with a write fault in order
	 * to break COW, except for shared mappings because these don't COW
	 * and we would not want to dirty them for nothing.
	 */
	if ((vma->vm_flags & (VM_WRITE | VM_SHARED)) == VM_WRITE)
		gup_flags |= FOLL_WRITE;

	/*
	 * We want mlock to succeed for regions that have any permissions
	 * other than PROT_NONE.
	 */
	if (vma->vm_flags & (VM_READ | VM_WRITE | VM_EXEC))
		gup_flags |= FOLL_FORCE;


	for (i = 0; i < page_num; i++) {
		int fault_cnt;
		unsigned long va_tmp = va_align+i*PAGE_SIZE;

		pa = 0;

		for (fault_cnt = 0; fault_cnt < 3000; fault_cnt++) {
			if (has_page) {
				ret = get_user_pages(va_tmp, 1,
						     gup_flags,
						     &pages, NULL);

				if (ret == 1)
					pa = page_to_phys(pages) |
						(va_tmp & ~PAGE_MASK);
			} else {
				pa = m4u_user_v2p(va_tmp);
				if (!pa) {
					handle_mm_fault(vma, va_tmp,
							(vma->vm_flags &
							 VM_WRITE) ?
							FAULT_FLAG_WRITE : 0);
				}
			}

			if (pa) {
				/* Add one line comment for
				 * avoid kernel coding style, WARNING:BRACES:
				 */
				break;
			}
			cond_resched();
		}

		if (!pa || !sg) {
			struct vm_area_struct *vma_temp;

			M4UMSG("%s: fail(0x%lx) va=0x%lx,page_num=0x%x\n",
				__func__, ret, va, page_num);
			M4UMSG("%s: fail_va=0x%lx,pa=0x%lx,sg=0x%p,i=%d\n",
				__func__, va_tmp, (unsigned long)pa, sg, i);
			vma_temp = find_vma(current->mm, va_tmp);
			if (vma_temp != NULL) {
				M4UMSG
					("vm_s=0x%lx,vm_end=%lx,vm_flag=%lx\n",
				vma->vm_start, vma->vm_end, vma->vm_flags);
				M4UMSG("vma_temp_s=0x%lx, vma_temp_e=%lx\n",
				vma_temp->vm_start, vma_temp->vm_end);
				M4UMSG("vm_temp_flag= %lx\n",
					vma_temp->vm_flags);
			}

			show_pte(current->mm, va_tmp);
			m4u_dump_mmaps(va);
			m4u_dump_mmaps(va_tmp);
			return -1;
		}

		if (fault_cnt > 2) {
			M4UINFO
				("warning: handle_mm_fault for %d times\n",
					fault_cnt);
			show_pte(current->mm, va_tmp);
			m4u_dump_mmaps(va_tmp);
		}
		/* debug check... */
		if ((pa & (PAGE_SIZE - 1)) != 0) {
			M4ULOG_MID("pa error, pa: 0x%lx\n",
				   (unsigned long)pa);
			M4ULOG_MID("va: 0x%lx, align: 0x%lx\n",
				   va_tmp, va_align);
		}

		if (has_page) {
			struct page *page;

			page = phys_to_page(pa);
			/* M4UMSG
			 *("page=0x%x, pfn=%d\n", page, __phys_to_pfn(pa));
			 */
			sg_set_page(sg, page, PAGE_SIZE, 0);
			#ifdef CONFIG_NEED_SG_DMA_LENGTH
				sg->dma_length = sg->length;
			#endif
		} else {
			sg_dma_address(sg) = pa;
			sg_dma_len(sg) = PAGE_SIZE;
		}
		sg = sg_next(sg);
	}
	*pSg = sg;
	return 0;
}

static int m4u_create_sgtable_user(
		unsigned long va_align, struct sg_table *table)
{
	int ret = 0;
	struct vm_area_struct *vma;
	struct scatterlist *sg = table->sgl;
	unsigned int left_page_num = table->nents;
	unsigned long va = va_align;

	down_read(&current->mm->mmap_sem);

	while (left_page_num) {
		unsigned int vma_page_num;

		vma = find_vma(current->mm, va);
		if (vma == NULL || vma->vm_start > va) {
			M4UMSG("cannot find vma: va=0x%lx, vma=0x%p\n",
					va, vma);
			if (vma != NULL) {
				M4UMSG("vm_start=0x%lx, vm_end=0x%lx\n",
				vma->vm_start, vma->vm_end);
				M4UMSG("vm_flag= 0x%lx\n",
					vma->vm_flags);
			}
			m4u_dump_mmaps(va);
			ret = -1;
			goto out;
		} else {
			/* M4ULOG_MID("%s va: 0x%lx, vma->vm_start=0x%lx\n",*/
			/*__func__, va, vma->vm_start); */
			M4ULOG_MID("vma->vm_end=0x%lx\n",
				vma->vm_end);
		}

		vma_page_num = (vma->vm_end - va) / PAGE_SIZE;
		vma_page_num = min(vma_page_num, left_page_num);

		if ((vma->vm_flags) & VM_PFNMAP) {
			/* ion va or ioremap vma has this flag */
			/* VM_PFNMAP: Page-ranges
			 * managed without "struct page", just pure PFN
			 */
			ret = m4u_fill_sgtable_user(vma,
				va, vma_page_num, &sg, 0);
			M4ULOG_MID("alloc_mva VM_PFNMAP");
			M4ULOG_MID("va=0x%lx, page_num=0x%x\n",
				va, vma_page_num);
		} else {
			/* Add one line comment for
			 * avoid kernel coding style, WARNING:BRACES:
			 */
			ret = m4u_fill_sgtable_user(vma,
				va, vma_page_num, &sg, 1);
			if (-1 == ret) {
				struct vm_area_struct *vma_temp;

				vma_temp = find_vma(current->mm, va_align);
				if (!vma_temp) {
					M4UMSG("%s not find vma\n", __func__);
					return -1;
				}
				M4UMSG
					("%s:vm_s=0x%lx\n",
				__func__,
				vma_temp->vm_start);
				M4UMSG("vm_end=0x%lx, vm_flag= 0x%lx\n",
				vma_temp->vm_end, vma_temp->vm_flags);
			}
		}
		if (ret) {
			/* Add one line comment for
			 * avoid kernel coding style, WARNING:BRACES:
			 */
			goto out;
		}

		left_page_num -= vma_page_num;
		va += vma_page_num * PAGE_SIZE;
	}

out:
	up_read(&current->mm->mmap_sem);
	return ret;
}

/* make a sgtable for virtual buffer */
struct sg_table *m4u_create_sgtable(unsigned long va, unsigned int size)
{
	struct sg_table *table;
	int ret, i, page_num;
	unsigned long va_align;
	phys_addr_t pa;
	struct scatterlist *sg;
	struct page *page;

	page_num = M4U_GET_PAGE_NUM(va, size);
	va_align = round_down(va, PAGE_SIZE);

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		M4UMSG(
			"%s table kmalloc fail: va=0x%lx, size=0x%x, page_num=%d\n",
				__func__, va, size, page_num);
		return ERR_PTR(-ENOMEM);
	}

	ret = sg_alloc_table(table, page_num, GFP_KERNEL);
	if (ret) {
		kfree(table);
		M4UMSG(
			"%s alloc_sgtable fail: va=0x%lx, size=0x%x, page_num=%d\n",
				__func__, va, size, page_num);
		return ERR_PTR(-ENOMEM);
	}

	M4ULOG_LOW(
		"%s va=0x%lx, PAGE_OFFSET=0x%lx, VMALLOC_START=0x%lx, VMALLOC_END=0x%lx\n",
		   __func__, va, PAGE_OFFSET,
		   VMALLOC_START, VMALLOC_END);

	if (va < PAGE_OFFSET) {	/* from user space */
		if (va >= VMALLOC_START && va <= VMALLOC_END) {	/* vmalloc */
			M4ULOG_MID("from user space vmalloc, va = 0x%lx", va);
			for_each_sg(table->sgl, sg, table->nents, i) {
				page = vmalloc_to_page((void *)
						(va_align + i * PAGE_SIZE));
				if (!page) {
					M4UMSG(
						"vmalloc_to_page fail from user space! va=0x%lx\n",
						va_align + i * PAGE_SIZE);
					goto err;
				}
				sg_set_page(sg, page, PAGE_SIZE, 0);
			}
		} else {
			ret = m4u_create_sgtable_user(va_align, table);
			if (ret) {
				M4UMSG("%s error va=0x%lx, size=%d\n",
						__func__, va, size);
				goto err;
			}
		}
	} else {		/* from kernel space */
		if (va >= VMALLOC_START && va <= VMALLOC_END) {	/* vmalloc */
			M4ULOG_MID(
				"from kernel space vmalloc va = 0x%lx\n",
				va);
			for_each_sg(table->sgl, sg, table->nents, i) {
				page = vmalloc_to_page((void *)
						(va_align + i * PAGE_SIZE));
				if (!page) {
					M4UMSG(
						"vmalloc_to_page fail from kernel space! va=0x%lx\n",
						va_align + i * PAGE_SIZE);
					goto err;
				}
				sg_set_page(sg, page, PAGE_SIZE, 0);
			}
		} else {	/* kmalloc to-do: use one entry sgtable. */
			for_each_sg(table->sgl, sg, table->nents, i) {
				pa = virt_to_phys((void *)
						(va_align + i * PAGE_SIZE));
				page = phys_to_page(pa);
				sg_set_page(sg, page, PAGE_SIZE, 0);
			}
		}
	}

	return table;

err:
	sg_free_table(table);
	kfree(table);
	return ERR_PTR(-EFAULT);
}

int m4u_destroy_sgtable(struct sg_table *table)
{
	if (!IS_ERR_OR_NULL(table)) {
		sg_free_table(table);
		kfree(table);
	}
	return 0;
}

/* #define __M4U_MAP_MVA_TO_KERNEL_FOR_DEBUG__ */

int m4u_alloc_mva(struct m4u_client_t *client,
			unsigned int port,
			unsigned long va, struct sg_table *sg_table,
			unsigned int size, unsigned int prot,
			unsigned int flags, unsigned int *pMva)
{
	int ret;
	struct m4u_buf_info_t *pMvaInfo;
	unsigned int mva = 0, mva_align, size_align;
	unsigned int domain_idx = 0;

#ifdef M4U_PROFILE
	mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_ALLOC_MVA],
		MMPROFILE_FLAG_START, va, size);
#endif
#if 0
	if (va && sg_table) {
		M4UMSG(
			"%s, va or sg_table are both valid: va=0x%lx, sg=0x%p\n",
			__func__, va, sg_table);
	}
#endif
	if (!va && !sg_table)	{
		M4UMSG(
			"%s, va or sg_table are both invalid: va=0x%lx, sg=0x%p\n",
			__func__, va, sg_table);
		ret = -EFAULT;
		goto err;
	}

	if (va && ((flags & M4U_FLAGS_SG_READY) == 0)) {
		sg_table = m4u_create_sgtable(va, size);
		if (IS_ERR_OR_NULL(sg_table)) {
			M4UMSG(
				"%s, cannot create sg: larb=%d,module=%s,va=0x%lx,sg=0x%p,size=%d,prot=0x%x,flags=0x%x\n"
				, __func__, m4u_port_2_larb_id(port),
				m4u_get_port_name(port),
				va, sg_table, size, prot, flags);
			ret = -EFAULT;
			goto err;
		}
	}

	/* here we get correct sg_table for this buffer */

	pMvaInfo = m4u_alloc_buf_info();
	if (!pMvaInfo) {
		ret = -ENOMEM;
		goto err;
	}

	pMvaInfo->va = va;
	pMvaInfo->port = port;
	pMvaInfo->size = size;
	pMvaInfo->prot = prot;
	pMvaInfo->flags = flags;
	pMvaInfo->sg_table = sg_table;

	if (port >= M4U_PORT_VPU)
		domain_idx = 1;

	if (flags & M4U_FLAGS_FIX_MVA)
		mva = m4u_do_mva_alloc_fix(domain_idx, va,
					*pMva, size, pMvaInfo);
	else if (flags & M4U_FLAGS_START_FROM)
		mva = m4u_do_mva_alloc_start_from(domain_idx, va,
					*pMva, size, pMvaInfo);
	else
		mva = m4u_do_mva_alloc(domain_idx, va, size, pMvaInfo);

	if (mva == 0) {
		m4u_aee_print(
			"alloc mva fail: larb=%d,module=%s,size=%d,domain=%u\n",
				m4u_port_2_larb_id(port),
				m4u_get_port_name(port), size,
				domain_idx);
		m4u_dump_buf_info(NULL, domain_idx);
		ret = -EINVAL;
		goto err1;
	} else
		M4ULOG_LOW("%s,mva = 0x%x, domain=%u\n",
			__func__, mva, domain_idx);

	m4u_get_sgtable_pages(sg_table);

	mva_align = round_down(mva, PAGE_SIZE);
	size_align = PAGE_ALIGN(mva + size - mva_align);

	ret = m4u_map_sgtable(m4u_get_domain_by_port(port), mva_align, sg_table,
			size_align, pMvaInfo->prot);
	if (ret < 0) {
		M4UMSG("error to map sgtable\n");
		goto err2;
	}

	pMvaInfo->mva = mva;
	pMvaInfo->mva_align = mva_align;
	pMvaInfo->size_align = size_align;
	pMvaInfo->domain_idx = domain_idx;
	pMvaInfo->current_ts = sched_clock();
	*pMva = mva;

	if (flags & M4U_FLAGS_SEQ_ACCESS)
		pMvaInfo->seq_id = m4u_insert_seq_range(port,
				mva, mva + size - 1);

	m4u_client_add_buf(client, pMvaInfo);

	M4ULOG_MID(
		"%s: pMvaInfo=0x%p, larb=%d,module=%s,va=0x%lx,sg=0x%p,size=%d,prot=0x%x,flags=0x%x,mva=0x%x,domain=%u\n",
		__func__, pMvaInfo, m4u_port_2_larb_id(port),
		m4u_get_port_name(port), va, sg_table,
		size, prot, flags, mva, domain_idx);

#ifdef M4U_PROFILE
	mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_ALLOC_MVA],
		MMPROFILE_FLAG_END, port, mva);
#endif

#ifdef __M4U_MAP_MVA_TO_KERNEL_FOR_DEBUG__
	/* map this mva to kernel va just for debug */
	{
		unsigned long kernel_va;
		unsigned int kernel_size;
		int ret;

		ret = m4u_mva_map_kernel(mva, size, &kernel_va, &kernel_size);
		if (ret) {
			M4UMSG("error to map kernel va\n");
			M4UMSG("mva=0x%x, size=%d\n", mva, size);
		} else {
			pMvaInfo->mapped_kernel_va_for_debug = kernel_va;
			M4ULOG_MID("[kernel_va_debug] map va\n");
			M4ULOG_MID("mva=0x%x, kernel_va=0x%lx,size=0x%x\n",
				mva, kernel_va, size);
		}
	}
#endif

	return 0;

err2:
	m4u_do_mva_free(domain_idx, mva, size);

err1:
	m4u_free_buf_info(pMvaInfo);

err:
	if (va)
		m4u_destroy_sgtable(sg_table);

	*pMva = 0;

	M4UMSG(
		"error: larb=%d,module=%s,va=0x%lx,size=%d,prot=0x%x,flags=0x%x, mva=0x%x, domain=%u\n",
		m4u_port_2_larb_id(port), m4u_get_port_name(port),
		va, size, prot, flags, mva, domain_idx);

#ifdef M4U_PROFILE
	mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_ALLOC_MVA],
		MMPROFILE_FLAG_END, port, 0);
#endif

	return ret;
}

/* interface for ion */
static struct m4u_client_t *ion_m4u_client;
int m4u_alloc_mva_sg(struct port_mva_info_t *port_info,
				struct sg_table *sg_table)
{
	int prot;
	int ret;
	unsigned int flags = 0;

	if (!ion_m4u_client) {
		ion_m4u_client = m4u_create_client();
		if (IS_ERR_OR_NULL(ion_m4u_client)) {
			ion_m4u_client = NULL;
			return -1;
		}
	}

	prot = M4U_PROT_READ | M4U_PROT_WRITE
	    | (port_info->cache_coherent ?
			(M4U_PROT_SHARE | M4U_PROT_CACHE) : 0)
	    | (port_info->security ? M4U_PROT_SEC : 0);

	if (port_info->flags & M4U_FLAGS_FIX_MVA) {
		if (port_info->iova_end >
			port_info->iova_start + port_info->buf_size) {
			port_info->mva = port_info->iova_start;
			flags = M4U_FLAGS_START_FROM;
		} else
			flags = M4U_FLAGS_FIX_MVA;
	}
	if (port_info->flags & M4U_FLAGS_SG_READY)
		flags |= M4U_FLAGS_SG_READY;
	else
		port_info->va = 0;
	ret = m4u_alloc_mva(ion_m4u_client,
		port_info->emoduleid, port_info->va, sg_table,
		port_info->buf_size, prot, flags, &port_info->mva);
	return ret;
}

#ifdef M4U_TEE_SERVICE_ENABLE
static int m4u_unmap_nonsec_buffer(unsigned int mva, unsigned int size);

int m4u_register_mva_share(int eModuleID, unsigned int mva)
{
	struct m4u_buf_info_t *pMvaInfo;
	unsigned int domain_idx = 0;

	if (eModuleID >= M4U_PORT_VPU)
		domain_idx = 1;

	pMvaInfo = mva_get_priv(mva, domain_idx);
	if (!pMvaInfo) {
		M4UMSG("%s cannot find mva: module=%s, mva=0x%x, domain=%u\n",
			__func__, m4u_get_port_name(eModuleID),
			mva, domain_idx);
		return -1;
	}
	pMvaInfo->flags |= M4U_FLAGS_SEC_SHAREABLE;

	return 0;
}
#endif


int m4u_dealloc_mva_sg(int eModuleID, struct sg_table *sg_table,
			const unsigned int buf_size, const unsigned int MVA)
{
	if (!ion_m4u_client) {
		m4u_aee_print("ion_m4u_client==NULL !! oops oops~~~~\n");
		return -1;
	}

	return m4u_dealloc_mva(ion_m4u_client, eModuleID, MVA);
}

/* should not hold client->dataMutex here. */
int m4u_dealloc_mva(struct m4u_client_t *client,
		int port, unsigned int mva)
{
	struct m4u_buf_info_t *pMvaInfo;
	int ret, is_err = 0;
	unsigned int size;
	unsigned int domain_idx = 0;

	if (port >= M4U_PORT_VPU)
		domain_idx = 1;

#ifdef M4U_PROFILE
	mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_DEALLOC_MVA],
		MMPROFILE_FLAG_START, port, mva);
#endif

	pMvaInfo = m4u_client_find_buf(client, mva, 1, domain_idx);
	if (!pMvaInfo) {
		ret = -ENOMEM;
		return ret;
	}
	if (unlikely(!pMvaInfo)) {
		M4UMSG(
			"error: %s no mva found in client! module=%s, mva=0x%x, domian=%u\n",
				__func__,
				m4u_get_port_name(port), mva,
				domain_idx);
		m4u_dump_buf_info(NULL, domain_idx);
#ifdef M4U_PROFILE
		mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_DEALLOC_MVA],
			MMPROFILE_FLAG_START, 0x5a5a5a5a, mva);
#endif
		return -EINVAL;
	}

	pMvaInfo->flags |= M4U_FLAGS_MVA_IN_FREE;

	M4ULOG_MID(
		"%s: larb=%d,module=%s,mva=0x%x, size=%d, domian=%u\n",
			__func__,
			m4u_port_2_larb_id(port),
			m4u_get_port_name(port), mva, pMvaInfo->size,
			domain_idx);

#ifdef M4U_TEE_SERVICE_ENABLE
	if (pMvaInfo->flags & M4U_FLAGS_SEC_SHAREABLE)
		m4u_unmap_nonsec_buffer(mva, pMvaInfo->size);
#endif

	ret = m4u_unmap(m4u_get_domain_by_port(port),
		pMvaInfo->mva_align, pMvaInfo->size_align);
	if (ret) {
		is_err = 1;
		M4UMSG("m4u_unmap fail, domian=%u\n", domain_idx);
	}

	if (pMvaInfo->va != 0) {
		/* non ion buffer*/
		if (pMvaInfo->va < PAGE_OFFSET) {  /* from user space */
			if (!(pMvaInfo->va >= VMALLOC_START &&
				pMvaInfo->va <= VMALLOC_END)) {/*non vmalloc */
				m4u_put_sgtable_pages(pMvaInfo->sg_table);
			}
		}
	}

	ret = m4u_do_mva_free(domain_idx, mva, pMvaInfo->size);
	if (ret) {
		is_err = 1;
		M4UMSG("do_mva_free fail\n");
	}

	if (pMvaInfo->va) {	/* buffer is allocated by va */
		m4u_destroy_sgtable(pMvaInfo->sg_table);
	}

	if (pMvaInfo->flags & M4U_FLAGS_SEQ_ACCESS) {
		if (pMvaInfo->seq_id > 0)
			m4u_invalid_seq_range_by_id(port, pMvaInfo->seq_id);
	}

	if (is_err) {
		m4u_aee_print(
			"%s fail: port=%s, mva=0x%x, size=0x%x, va=0x%lx, domian=%u\n",
			__func__, m4u_get_port_name(port), mva,
			pMvaInfo->size, pMvaInfo->va,
			domain_idx);
		ret = -EINVAL;
	} else
		ret = 0;

	size = pMvaInfo->size;

#ifdef __M4U_MAP_MVA_TO_KERNEL_FOR_DEBUG__
	/* unmap kernel va for debug */
	{
		if (pMvaInfo->mapped_kernel_va_for_debug) {
			M4ULOG_MID("[kernel_va_debug] unmap va\n");
			M4ULOG_MID("mva=0x%x, kernel_va=0x%lx, size=0x%x\n",
				pMvaInfo->mva,
				pMvaInfo->mapped_kernel_va_for_debug,
				pMvaInfo->size);
				m4u_mva_unmap_kernel(pMvaInfo->mva,
				pMvaInfo->size,
				pMvaInfo->mapped_kernel_va_for_debug);
		}
	}
#endif

	m4u_free_buf_info(pMvaInfo);

#ifdef M4U_PROFILE
	mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_DEALLOC_MVA],
		MMPROFILE_FLAG_END, size, mva);
#endif

	return ret;
}

int m4u_dump_info(int m4u_index)
{
	return 0;
}

void m4u_get_pgd(struct m4u_client_t *client,
	unsigned int port, void **pgd_va, void **pgd_pa,
	unsigned int *size)
{
	struct m4u_domain_t *pDomain;

	pDomain = m4u_get_domain_by_port(port);
	*pgd_va = pDomain->pgd;
	*pgd_pa = (void *)(uintptr_t)pDomain->pgd_pa;
	*size = M4U_PGD_SIZE;
}

unsigned long m4u_mva_to_pa(struct m4u_client_t *client,
		unsigned int port, unsigned int mva)
{
	unsigned long pa;
	struct m4u_domain_t *pDomain;

	pDomain = m4u_get_domain_by_port(port);

	pa = m4u_get_pte(pDomain, mva);

	return pa;
}

int m4u_mva_check(int port, unsigned int mva)
{
	int valid = 0;
	struct m4u_domain_t *pDomain;

	pDomain = m4u_get_domain_by_port(port);

	valid = _m4u_get_pte(pDomain, mva);

	return valid;
}

int __m4u_query_mva_info(unsigned int domain_idx,
	unsigned int mva, unsigned int size,
	unsigned int *real_mva, unsigned int *real_size)
{
	struct m4u_buf_info_t *pMvaInfo;

	if ((!real_mva) || (!real_size))
		return -1;

	pMvaInfo = mva_get_priv_ext(domain_idx, mva);
	if (!pMvaInfo) {
		M4UMSG(
			"%s cannot find mva: mva=0x%x, size=0x%x, domain=%u\n",
			__func__, mva, size, domain_idx);
		*real_mva = 0;
		*real_size = 0;

		return -2;
	}
	*real_mva = pMvaInfo->mva;
	*real_size = pMvaInfo->size;

	return 0;
}
EXPORT_SYMBOL(__m4u_query_mva_info);

int m4u_query_mva_info(unsigned int mva, unsigned int size,
	unsigned int *real_mva, unsigned int *real_size)
{
	return __m4u_query_mva_info(0, mva,
			size, real_mva, real_size);
}
EXPORT_SYMBOL(m4u_query_mva_info);

/***********************************************************/
/* map mva buffer to kernel va buffer
 *   this function should ONLY used for DEBUG
 */
/************************************************************/
int m4u_mva_map_kernel(unsigned int mva,
		unsigned int size, unsigned long *map_va,
		       unsigned int *map_size)
{
	struct m4u_buf_info_t *pMvaInfo;
	struct sg_table *table;
	struct scatterlist *sg;
	int i, j, k, ret = 0;
	struct page **pages;
	unsigned int page_num;
	void *kernel_va;
	unsigned int kernel_size;
	unsigned int domain_idx = 0;

	if (unlikely(g_translation_fault_debug))
		return 0;

	pMvaInfo = mva_get_priv(mva, domain_idx);

	if (!pMvaInfo || pMvaInfo->size < size) {
		M4UMSG
			("%s cannot find mva: mva=0x%x, size=0x%x\n",
				__func__, mva, size);
		if (pMvaInfo)
			M4UMSG
			("pMvaInfo: mva=0x%x, size=0x%x\n",
				pMvaInfo->mva, pMvaInfo->size);
		return -1;
	}

	table = pMvaInfo->sg_table;

	page_num = M4U_GET_PAGE_NUM(mva, size);
	pages = vmalloc(sizeof(struct page *) * page_num);
	if (pages == NULL) {
		M4UMSG("mva_map_kernel:error to vmalloc for %d\n",
		       (unsigned int)sizeof(struct page *) * page_num);
		return -1;
	}

	k = 0;
	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page_start;
		int pages_in_this_sg = PAGE_ALIGN(sg_dma_len(sg)) / PAGE_SIZE;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
		if (sg_dma_address(sg) == 0)
			pages_in_this_sg = PAGE_ALIGN(sg->length) / PAGE_SIZE;
#endif
		page_start = sg_page(sg);
		for (j = 0; j < pages_in_this_sg; j++) {
			pages[k++] = page_start++;
			if (k >= page_num)
				goto get_pages_done;
		}
	}

get_pages_done:
	if (k < page_num) {
		/* this should not happen, because
		 * we have checked the size before.
		 */
		M4UMSG("mva_map_kernel:only get %d pages: mva=0x%x\n",
			k, mva);
		M4UMSG("mva_map_kernel:size=0x%x, pg_num=%d\n",
			size, page_num);
		ret = -1;
		goto error_out;
	}

	kernel_va = 0;
	kernel_size = 0;
	kernel_va = vmap(pages, page_num, VM_MAP, PAGE_KERNEL);
	if (kernel_va == 0 || (unsigned long)kernel_va & M4U_PAGE_MASK) {
		M4UMSG("mva_map_kernel:vmap fail\n");
		M4UMSG("page_num=%d, kernel_va=0x%p\n",
			page_num, kernel_va);
		ret = -2;
		goto error_out;
	}

	kernel_va += ((unsigned long)mva & (M4U_PAGE_MASK));

	*map_va = (unsigned long)kernel_va;
	*map_size = size;

error_out:
	vfree(pages);
	M4ULOG_LOW("mva_map_kernel:mva=0x%x,size=0x%x\n",
		   mva, size);
	M4ULOG_LOW("mva_map_kernel:map_va=0x%lx,map_size=0x%x\n",
		   *map_va, *map_size);

	return ret;
}
EXPORT_SYMBOL(m4u_mva_map_kernel);

int m4u_mva_unmap_kernel(unsigned int mva,
	unsigned int size, unsigned long map_va)
{
	M4ULOG_LOW("mva_unmap_kernel:mva=0x%x,size=0x%x,va=0x%lx\n",
		mva, size, map_va);
	vunmap((void *)(map_va & (~M4U_PAGE_MASK)));
	return 0;
}
EXPORT_SYMBOL(m4u_mva_unmap_kernel);

static int MTK_M4U_open(struct inode *inode, struct file *file)
{
	struct m4u_client_t *client;

	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client)) {
		M4UMSG("createclientfail\n");
		return -ENOMEM;
	}

	file->private_data = client;

	return 0;
}

static int MTK_M4U_release(struct inode *inode, struct file *file)
{
	struct m4u_client_t *client = file->private_data;

	m4u_destroy_client(client);
	return 0;
}

static int MTK_M4U_flush(struct file *filp, fl_owner_t a_id)
{
	return 0;
}

#ifdef M4U_TEE_SERVICE_ENABLE
#include "m4u_sec_gp.h"
static DEFINE_MUTEX(gM4u_sec_init);

static int __m4u_sec_init(void)
{
	int ret, i;
	void *pgd_va;
	unsigned long pt_pa_nonsec;
	unsigned int size;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4UTL_INIT);
	if (!ctx)
		return -EFAULT;

	m4u_get_pgd(NULL, 0, &pgd_va, (void *)&pt_pa_nonsec, &size);
	for (i = 0; i < SMI_LARB_NR; i++)
		larb_clock_on(i, 1);

	ctx->m4u_msg->cmd = CMD_M4UTL_INIT;
	ctx->m4u_msg->init_param.nonsec_pt_pa = pt_pa_nonsec;
	ctx->m4u_msg->init_param.l2_en = gM4U_L2_enable;
	/* m4u_alloc_sec_pt_for_debug(); */
	ctx->m4u_msg->init_param.sec_pt_pa = 0;

	M4ULOG_HIGH(
		"%s call m4u_exec_cmd CMD_M4UTL_INIT, nonsec_pt_pa: 0x%lx\n",
		__func__, pt_pa_nonsec);
	ret = m4u_exec_cmd(ctx);
	if (ret < 0) {
		M4UERR("m4u exec command fail\n");
		goto out;
	}

	ret = ctx->m4u_msg->rsp;
out:
	for (i = 0; i < SMI_LARB_NR; i++)
		larb_clock_off(i, 1);
	m4u_sec_ctx_put(ctx);
	return ret;
}

/* ------------------------------------------------------------- */
int m4u_sec_init(void)
{
	int ret;

	M4UINFO("%s in normal m4u driver\n", __func__);

	if (m4u_tee_en) {
		M4UMSG("warning: m4u secure has been inited, %d\n", m4u_tee_en);
		goto m4u_sec_reinit;
	}

	m4u_sec_set_context();

	if (!m4u_tee_en) {
		ret = m4u_sec_context_init();
		if (ret)
			return ret;

		m4u_tee_en = 1;
	} else {
		M4UMSG("warning: m4u secure has been inited, %d\n", m4u_tee_en);
	}
m4u_sec_reinit:
	ret = __m4u_sec_init();
	if (ret < 0) {
		m4u_tee_en = 0;
		m4u_sec_context_deinit();
		M4UMSG("%s:init fail,ret=0x%x\n", __func__, ret);
		return ret;
	}

	/* don't deinit ta because of multiple init operation */
	M4UINFO("%s:normal init done\n", __func__);
	return 0;
}

int m4u_config_port_tee(struct M4U_PORT_STRUCT *pM4uPort)	/* native */
{
	int ret;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_CFG_PORT);
	if (!ctx)
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4U_CFG_PORT;
	ctx->m4u_msg->port_param.port = pM4uPort->ePortID;
	ctx->m4u_msg->port_param.virt = pM4uPort->Virtuality;
	ctx->m4u_msg->port_param.direction = pM4uPort->Direction;
	ctx->m4u_msg->port_param.distance = pM4uPort->Distance;
	ctx->m4u_msg->port_param.sec = 0;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		M4UMSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}
#if 0
int m4u_config_port_array_tee(unsigned char *port_array)	/* native */
{
	int ret;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_CFG_PORT_ARRAY);
	if (!ctx)
		return -EFAULT;

	memset(ctx->m4u_msg, 0, sizeof(*ctx->m4u_msg));
	memcpy(ctx->m4u_msg->port_array_param.m4u_port_array, port_array,
		   sizeof(ctx->m4u_msg->port_array_param.m4u_port_array));

	ctx->m4u_msg->cmd = CMD_M4U_CFG_PORT_ARRAY;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		M4UMSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}
#endif

/*#ifdef TO_BE_IMPL*/
int m4u_larb_backup_sec(unsigned int larb_idx)
{
	int ret;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_LARB_BACKUP);
	if (!ctx)
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4U_LARB_BACKUP;
	ctx->m4u_msg->larb_param.larb_idx = larb_idx;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		M4UMSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}

int m4u_larb_restore_sec(unsigned int larb_idx)
{
	int ret;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_LARB_RESTORE);
	if (!ctx)
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4U_LARB_RESTORE;
	ctx->m4u_msg->larb_param.larb_idx = larb_idx;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		M4UMSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}



static int m4u_reg_backup_sec(void)
{
	int ret;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_REG_BACKUP);
	if (!ctx)
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4U_REG_BACKUP;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		M4UMSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}

static int m4u_reg_restore_sec(void)
{
	int ret;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_REG_RESTORE);
	if (!ctx)
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4U_REG_RESTORE;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		M4UMSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}

	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}

static void m4u_early_suspend(void)
{
	int i = 0;

	M4UMSG("%s +, %d\n", __func__, m4u_tee_en);

	//smi_debug_bus_hang_detect(false, M4U_DEV_NAME);
	if (m4u_tee_en) {
		for (i = 0; i < SMI_LARB_NR; i++)
			larb_clock_on(i, 1);

		m4u_reg_backup_sec();

		for (i = 0; i < SMI_LARB_NR; i++)
			larb_clock_off(i, 1);
	}
	M4UMSG("%s -\n", __func__);
}

static void m4u_late_resume(void)
{
	int i = 0;

	M4UMSG("%s +, %d\n", __func__, m4u_tee_en);

	//smi_debug_bus_hang_detect(false, M4U_DEV_NAME);
	if (m4u_tee_en) {
		for (i = 0; i < SMI_LARB_NR; i++)
			larb_clock_on(i, 1);

		m4u_reg_restore_sec();

		for (i = 0; i < SMI_LARB_NR; i++)
			larb_clock_off(i, 1);
	}

	M4UMSG("%s -\n", __func__);
}

static struct notifier_block m4u_fb_notifier;
static int m4u_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	M4UMSG("%s %ld, %d\n", __func__, event, FB_EVENT_BLANK);

	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
		m4u_late_resume();
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
		break;
	case FB_BLANK_POWERDOWN:
		m4u_early_suspend();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int m4u_map_nonsec_buf(int port, unsigned int mva, unsigned int size)
{
	int ret;

	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_MAP_NONSEC_BUFFER);
	if (!ctx)
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4U_MAP_NONSEC_BUFFER;
	ctx->m4u_msg->buf_param.mva = mva;
	ctx->m4u_msg->buf_param.size = size;
	ctx->m4u_msg->buf_param.port = port;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		M4UMSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}


static int m4u_unmap_nonsec_buffer(unsigned int mva, unsigned int size)
{
	int ret;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_UNMAP_NONSEC_BUFFER);
	if (!ctx)
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4U_UNMAP_NONSEC_BUFFER;
	ctx->m4u_msg->buf_param.mva = mva;
	ctx->m4u_msg->buf_param.size = size;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		M4UMSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}


#endif

static long MTK_M4U_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;
#ifdef M4U_FPGAPORTING
	struct M4U_PORT_STRUCT m4u_port;
#endif
	unsigned int ModuleID;
	//struct m4u_client_t *client = filp->private_data;

	switch (cmd) {
	case MTK_M4U_T_POWER_ON:
		ret = copy_from_user(&ModuleID,
			(void *)arg, sizeof(unsigned int));
		if (ret) {
			m4u_info("M4U_POWER_ON,copy_from_user failed,%d\n",
				 ret);
			return -EFAULT;
		}
		if (ModuleID < 0 || ModuleID >= M4U_PORT_UNKNOWN) {
			m4u_info("MTK_M4U_T_POWER_ON, moduleid%d is invalid\n",
				 ModuleID);
			return -EFAULT;
		}
		ret = m4u_power_on(ModuleID);
		break;

	case MTK_M4U_T_POWER_OFF:
		ret = copy_from_user(&ModuleID,
			(void *)arg, sizeof(unsigned int));
		if (ret) {
			m4u_info("M4U_POWER_OFF,copy_from_user failed,%d\n",
				 ret);
			return -EFAULT;
		}
		if (ModuleID < 0 || ModuleID >= M4U_PORT_UNKNOWN) {
			m4u_info("MTK_M4U_T_POWER_Off, moduleid%d is invalid\n",
				 ModuleID);
			return -EFAULT;
		}
		ret = m4u_power_off(ModuleID);
		break;
	case MTK_M4U_T_DUMP_INFO:
		ret = copy_from_user(&ModuleID,
			(void *)arg, sizeof(unsigned int));
		if (ret) {
			m4u_info("M4U_Invalid_TLB_Range,copy_user failed,%d\n",
				 ret);

			return -EFAULT;
		}
		if (ModuleID < 0 || ModuleID >= M4U_PORT_UNKNOWN) {
			m4u_info("M4U_Invalid_TLB_Range, port%d is invalid\n",
				 ModuleID);

			return -EFAULT;
		}
		break;

#ifdef M4U_FPGAPORTING
	case MTK_M4U_T_CONFIG_PORT:
		ret = copy_from_user(&m4u_port,
			(void *)arg, sizeof(struct M4U_PORT_STRUCT));
		if (ret) {
			m4u_info("M4U_CONFIG_PORT,copy_from_user failed:%d\n",
					ret);
			return -EFAULT;
		}
		if (m4u_port.ePortID < 0 ||
			m4u_port.ePortID >= M4U_PORT_UNKNOWN) {
			m4u_info("MTK_M4U_T_CONFIG_PORT, port%d is invalid\n",
					m4u_port.ePortID);
			return -EFAULT;
		}
#ifdef M4U_TEE_SERVICE_ENABLE
		mutex_lock(&gM4u_sec_init);
#endif
		ret = m4u_config_port(&m4u_port);
#ifdef M4U_TEE_SERVICE_ENABLE
		mutex_unlock(&gM4u_sec_init);
#endif
		break;
#endif

	case MTK_M4U_T_CONFIG_PORT_ARRAY:
#if 0
		{
			struct m4u_port_array port_array;

			ret = copy_from_user(&port_array,
				(void *)arg, sizeof(struct m4u_port_array));
			if (ret) {
				M4UMSG
					("M4U_CFG_PORT,copy_user failed:%d\n",
						ret);
				return -EFAULT;
			}
#ifdef M4U_TEE_SERVICE_ENABLE
			mutex_lock(&gM4u_sec_init);
#endif
			ret = m4u_config_port_array(&port_array);
			M4UMSG("config port by ioctl is not support\n");
#ifdef M4U_TEE_SERVICE_ENABLE
			mutex_unlock(&gM4u_sec_init);
#endif
		}
#endif
		break;

	case MTK_M4U_T_CONFIG_TF:
		{
			struct M4U_TF_STRUCT rM4UTF;

			ret = copy_from_user(&rM4UTF,
				(void *)arg, sizeof(struct M4U_TF_STRUCT));
			if (ret) {
				m4u_info("M4U_FG_TF,copy_user failed:%d\n",
					 ret);
				return -EFAULT;
			}
			if (rM4UTF.port < 0 ||
					rM4UTF.port >= M4U_PORT_UNKNOWN) {
				m4u_info("M4U_CFG_TF, port%d is invalid\n",
					 rM4UTF.port);
				return -EFAULT;
			}

			ret = m4u_enable_tf(rM4UTF.port, rM4UTF.fgEnable);
		}
		break;
#ifdef M4U_TEE_SERVICE_ENABLE
	case MTK_M4U_T_SEC_INIT:
		{
			m4u_info("M4U ioctl : M4U_EC_INIT command!! 0x%x\n",
				 cmd);
			mutex_lock(&gM4u_sec_init);
			ret = m4u_sec_init();
			mutex_unlock(&gM4u_sec_init);
		}
		break;
#endif

#ifdef M4U_GZ_SERVICE_ENABLE
	case MTK_M4U_T_GZ_SEC_INIT:
	{
		int mtk_iommu_sec_id = 0;

		m4u_info("MTK M4U ioctl : MTK_M4U_T_GZ_SEC_INIT command!! 0x%x, arg:%d\n",
			 cmd, arg);
		mtk_iommu_sec_id = arg;
		if (mtk_iommu_sec_id < 0 ||
			mtk_iommu_sec_id > SEC_ID_COUNT)
			return -EFAULT;
		ret = m4u_gz_sec_init(mtk_iommu_sec_id);
	}
	break;
#endif

	default:
		/* M4UMSG("MTK M4U ioctl:No such command!!\n"); */
		ret = -EINVAL;
		break;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)

struct COMPAT_M4U_MOUDLE_STRUCT {
	compat_uint_t port;
	compat_ulong_t BufAddr;
	compat_uint_t buf_size;
	compat_uint_t prot;
	compat_uint_t MVAStart;
	compat_uint_t MVAEnd;
	compat_uint_t flags;
};

long MTK_M4U_COMPAT_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case MTK_M4U_T_POWER_ON:
	case MTK_M4U_T_POWER_OFF:
	case MTK_M4U_T_DUMP_INFO:
	case MTK_M4U_T_CONFIG_PORT:
	case MTK_M4U_T_MONITOR_START:
	case MTK_M4U_T_MONITOR_STOP:
	case MTK_M4U_T_CONFIG_PORT_ARRAY:
	case MTK_M4U_T_SEC_INIT:
	case MTK_M4U_T_GZ_SEC_INIT:
		return filp->f_op->unlocked_ioctl(filp,
			cmd, (unsigned long)compat_ptr(arg));
	default:
		return -ENOIOCTLCMD;
	}
}

#else

#define MTK_M4U_COMPAT_ioctl  NULL

#endif

static const struct file_operations m4u_fops = {
	.owner = THIS_MODULE,
	.open = MTK_M4U_open,
	.release = MTK_M4U_release,
	.flush = MTK_M4U_flush,
	.unlocked_ioctl = MTK_M4U_ioctl,
	.compat_ioctl = MTK_M4U_COMPAT_ioctl,
	/* .mmap = NULL; */
};

static int m4u_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;

	m4u_debug("%s 0\n", __func__);
#if defined(CONFIG_MTK_SMI_EXT)
	m4u_debug("%s: smi_mm ret: %d\n", __func__, smi_mm_first_get());
	if (!smi_mm_first_get()) {
		M4UMSG("SMI not start probe\n");
		return -EPROBE_DEFER;
	}
#endif
	if (pdev->dev.of_node) {
		int err;

		err = of_property_read_u32(node, "cell-index", &pdev->id);
		if (err) {
			M4UMSG("[DTS] m4u probe failed of platform_device %d\n",
			  err);
			return -EPROBE_DEFER;
		}
	}
	if (pdev->id < 0 || pdev->id >= TOTAL_M4U_NUM) {
		M4UMSG("%s id(%d) is error...\n",
				__func__, pdev->id);
		return 0;
	}

	m4u_debug("%s 1, pdev id = %d name = %s\n",
		  __func__,
		  pdev->id, pdev->name);

	gM4uDev->pDev[pdev->id] = &pdev->dev;
	gM4uDev->m4u_base[pdev->id] = (unsigned long)of_iomap(node, 0);
	gM4uDev->irq_num[pdev->id] = irq_of_parse_and_map(node, 0);

	M4UMSG("%s 2, of_iomap: 0x%lx, irq_num: %d, pDev: %p\n",
		__func__,
		gM4uDev->m4u_base[pdev->id],
		gM4uDev->irq_num[pdev->id], gM4uDev->pDev[pdev->id]);

	m4u_domain_init(gM4uDev, &gMvaNode_unknown, pdev->id);

#if defined(M4U_TEE_SERVICE_ENABLE) || defined(M4U_GZ_SERVICE_ENABLE)
{
	struct m4u_buf_info_t *pMvaInfo;
	unsigned int mva;

	pMvaInfo = m4u_alloc_buf_info();
	if (pMvaInfo != NULL && pdev->id < TOTAL_M4U_NUM) {
		pMvaInfo->port = M4U_PORT_UNKNOWN;
		pMvaInfo->size =
			M4U_NONSEC_MVA_START - 0x100000;

		mva = m4u_do_mva_alloc(pdev->id,
			0, M4U_NONSEC_MVA_START - 0x100000,
			pMvaInfo);
		m4u_info("reserve sec mva: 0x%x, domain:%d\n",
			 mva, pdev->id);
	} else {
		m4u_info("pMvaInfo is NULL,secure mva space reserve fail\n");
	}
}
#endif

	if (pdev->id == 1) {
		struct m4u_buf_info_t *pMvaInfo;

		pMvaInfo = m4u_alloc_buf_info();
		if (pMvaInfo != NULL) {
			pMvaInfo->port = M4U_PORT_VPU;
			pMvaInfo->size = VPU_IOMMU_MVA_SIZE;

			pMvaInfo->mva = m4u_do_mva_alloc_start_from(
				1, 0, VPU_IOMMU_MVA_START,
				VPU_IOMMU_MVA_SIZE, pMvaInfo);

			m4u_info("reserve vpu_iommu mva_start: 0x%x, mva_end:0x%x, domain:%d\n",
				 pMvaInfo->mva,
				 (pMvaInfo->mva + VPU_IOMMU_MVA_SIZE), pdev->id);
		} else {
			m4u_info("pMvaInfo is NULL,vpu_iommu mva space reserve fail\n");
		}
	}

	m4u_hw_init(gM4uDev, pdev->id);

	m4u_debug("%s 3 finish...\n", __func__);

	return 0;
}

static int m4u_remove(struct platform_device *pdev)
{
	m4u_hw_deinit(gM4uDev, pdev->id);

#ifndef __M4U_USE_PROC_NODE
	misc_deregister(&(gM4uDev->dev));
#else
	if (gM4uDev->m4u_dev_proc_entry)
		proc_remove(gM4uDev->m4u_dev_proc_entry);
#endif

	return 0;
}

static int m4u_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	m4u_reg_backup();
	M4UINFO("M4U backup in suspend\n");

	return 0;
}

static int m4u_resume(struct platform_device *pdev)
{
	m4u_reg_restore();
	M4UINFO("M4U restore in resume\n");
	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
static int m4u_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL)
		M4UERR("pdev is NULL!\n");

	return m4u_suspend(pdev, PMSG_SUSPEND);
}

static int m4u_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL)
		M4UERR("pdev is NULL!\n");

	return m4u_resume(pdev);
}

static int m4u_pm_restore_noirq(struct device *device)
{
	int i;

	for (i = 0; i < TOTAL_M4U_NUM; i++)
		irq_set_irq_type(gM4uDev->irq_num[i], IRQF_TRIGGER_LOW);


	return 0;
}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define m4u_pm_suspend NULL
#define m4u_pm_resume NULL
#define m4u_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
static const struct of_device_id mm_iommu_of_ids[] = {
	{.compatible = "mediatek,mm_m4u",},
	{.compatible = "mediatek,iommu_v0"},
	{.compatible = "mediatek,perisys_iommu",},
	{}
};

static const struct of_device_id vpu_iommu_of_ids[] = {
	{.compatible = "mediatek,vpu_m4u",},
	{.compatible = "mediatek,iommu_v0"},
	{.compatible = "mediatek,perisys_iommu",},
	{}
};


const struct dev_pm_ops m4u_pm_ops = {
	.suspend = m4u_pm_suspend,
	.resume = m4u_pm_resume,
	.freeze = m4u_pm_suspend,
	.thaw = m4u_pm_resume,
	.poweroff = m4u_pm_suspend,
	.restore = m4u_pm_resume,
	.restore_noirq = m4u_pm_restore_noirq,
};

static struct platform_driver mm_m4uDrv = {
	.probe = m4u_probe,
	.remove = m4u_remove,
	.suspend = m4u_suspend,
	.resume = m4u_resume,
	.driver = {
		   .name = "mm_m4u",
		   .of_match_table = mm_iommu_of_ids,
#ifdef CONFIG_PM
		   .pm = &m4u_pm_ops,
#endif
		   .owner = THIS_MODULE,
	}
};

static struct platform_driver vpu_m4uDrv = {
	.probe = m4u_probe,
	.remove = m4u_remove,
	.suspend = m4u_suspend,
	.resume = m4u_resume,
	.driver = {
		   .name = "vpu_m4u",
		   .of_match_table = vpu_iommu_of_ids,
#ifdef CONFIG_PM
		   .pm = &m4u_pm_ops,
#endif
		   .owner = THIS_MODULE,
	}
};


#if 0
static u64 m4u_dmamask = ~(u32) 0;

static struct platform_device mtk_m4u_dev = {
	.name = M4U_DEV_NAME,
	.id = 0,
	.dev = {
		.dma_mask = &m4u_dmamask,
		.coherent_dma_mask = 0xffffffffUL}
};
#endif

#define __M4U_USE_PROC_NODE

static int __init MTK_M4U_Init(void)
{
	int ret = 0;

	gM4uDev = kzalloc(sizeof(struct m4u_device), GFP_KERNEL);

	M4UINFO("%s kzalloc: %p\n", __func__, gM4uDev);

	if (!gM4uDev) {
		M4UMSG("kmalloc for m4u_device fail\n");
		return -ENOMEM;
	}
#ifndef __M4U_USE_PROC_NODE
	gM4uDev->dev.minor = MISC_DYNAMIC_MINOR;
	gM4uDev->dev.name = M4U_DEV_NAME;
	gM4uDev->dev.fops = &m4u_fops;
	gM4uDev->dev.parent = NULL;

	ret = misc_register(&(gM4uDev->dev));
	M4UINFO("misc_register, minor: %d\n", gM4uDev->dev.minor);
	if (ret) {
		M4UMSG("failed to register misc device.\n");
		return ret;
	}
#else
	gM4uDev->m4u_dev_proc_entry = proc_create("m4u", 0644, NULL, &m4u_fops);
	if (!(gM4uDev->m4u_dev_proc_entry)) {
		M4UMSG("m4u:failed to register m4u in proc/m4u_device.\n");
		return ret;
	}
#endif

	m4u_debug_init(gM4uDev);

	M4UINFO("M4U platform_driver_register start\n");

	if (platform_driver_register(&mm_m4uDrv)) {
		M4UMSG("failed to register M4U driver");
		return -ENODEV;
	}
	M4UINFO("mm m4u driver register done!\n");

	if (platform_driver_register(&vpu_m4uDrv)) {
		M4UMSG("failed to register M4U driver");
		return -ENODEV;
	}

	M4UINFO("vpu m4u driver register done!\n");

	M4UINFO("M4U platform_driver_register finsish\n");

#if 0
	retval = platform_device_register(&mtk_m4u_dev);
	if (retval != 0)
		return retval;
#endif

#ifdef M4U_PROFILE
	m4u_profile_init();
#endif

#ifdef M4U_TEE_SERVICE_ENABLE
	m4u_fb_notifier.notifier_call = m4u_fb_notifier_callback;
	ret = fb_register_client(&m4u_fb_notifier);
	if (ret)
		M4UMSG("m4u register fb_notifier failed! ret(%d)\n", ret);
	else
		M4UMSG("m4u register fb_notifier OK!\n");
#endif

	return 0;
}

static int __init mtk_m4u_late_init(void)
{
	return 0;
}

static void __exit MTK_M4U_Exit(void)
{
	platform_driver_unregister(&mm_m4uDrv);
	platform_driver_unregister(&vpu_m4uDrv);
}

subsys_initcall(MTK_M4U_Init);
late_initcall(mtk_m4u_late_init);
module_exit(MTK_M4U_Exit);

MODULE_DESCRIPTION("MTKM4Udriver");
MODULE_AUTHOR("MTK80347 <Xiang.Xu@mediatek.com>");
MODULE_LICENSE("GPL");
