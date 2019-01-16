#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/aee.h>
#include <linux/timer.h>
#include <linux/cache.h>
#include <linux/printk.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#define MFV_LOGE(...) pr_err(__VA_ARGS__);

unsigned long pmem_user_v2p_video(unsigned long va)
{
	unsigned long pageOffset = (va & (PAGE_SIZE - 1));
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long pa;

	if (NULL == current) {
		MFV_LOGE("[ERROR] pmem_user_v2p_video, current is NULL!\n");
		return 0;
	}

	if (NULL == current->mm) {
		MFV_LOGE("[ERROR] pmem_user_v2p_video, current->mm is NULL! tgid=0x%x, name=%s\n",
			 current->tgid, current->comm);
		return 0;
	}

	pgd = pgd_offset(current->mm, va);	/* what is tsk->mm */
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		MFV_LOGE("[ERROR] pmem_user_v2p(), va=0x%lx, pgd invalid!\n", va);
		return 0;
	}

	pud = pud_offset(pgd, va);
	if (pud_none(*pud) || pud_bad(*pud)) {
		MFV_LOGE("[ERROR] pmem_user_v2p(), va=0x%lx, pud invalid!\n", va);
		return 0;
	}

	pmd = pmd_offset(pud, va);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		MFV_LOGE("[ERROR] pmem_user_v2p(), va=0x%lx, pmd invalid!\n", va);
		return 0;
	}

	pte = pte_offset_map(pmd, va);
	if (pte_present(*pte)) {
		pa = (pte_val(*pte) & (PAGE_MASK)) | pageOffset;
		pte_unmap(pte);
		return pa;
	}

	MFV_LOGE("[ERROR] pmem_user_v2p(), va=0x%lx, pte invalid!\n", va);
	return 0;
}
EXPORT_SYMBOL(pmem_user_v2p_video);
