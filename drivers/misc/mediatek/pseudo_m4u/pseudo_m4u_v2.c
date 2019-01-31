/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/iommu.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/dma-iommu.h>
#include <soc/mediatek/smi.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/memblock.h>
#include <asm/cacheflush.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <mach/pseudo_m4u.h>
#include <linux/pagemap.h>
#include <linux/compat.h>

#include <sync_write.h>
#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif
#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu_ext.h"
#include "mach/mt_iommu.h"
#endif

#define LOG_LEVEL_HIGH	3
#define LOG_LEVEL_MID	2
#define LOG_LEVEL_LOW	1

int m4u_log_level = 2;
int m4u_log_to_uart = 2;
static unsigned int temp_st = 0xffffffff;
static unsigned int temp_en;

#define _M4ULOG(level, string, args...) \
do { \
	if (level > m4u_log_level) { \
		if (level > m4u_log_to_uart) \
			pr_info("[PSEUDO][%s #%d]: "string,		\
				__func__, __LINE__, ##args); \
		else\
			pr_debug("[PSEUDO][%s #%d]: "string,		\
				__func__, __LINE__, ##args); \
	}  \
} while (0)

#define M4U_ERR(string, args...)	_M4ULOG(LOG_LEVEL_HIGH, string, ##args)
#define M4U_MSG(string, args...)	_M4ULOG(LOG_LEVEL_HIGH, string, ##args)
#define M4U_DBG(string, args...)	_M4ULOG(LOG_LEVEL_LOW, string, ##args)

#define M4UTRACE() \
do { \
	if (!m4u_log_to_uart) \
		pr_info("[PSEUDO] %s, %d\n", __func__, __LINE__); \
} while (0)

LIST_HEAD(pseudo_sglist);
/* this is the mutex lock to protect mva_sglist->list*/
static DEFINE_MUTEX(pseudo_list_mutex);

static const struct of_device_id mtk_pseudo_of_ids[] = {
	{ .compatible = "mediatek,mt-pseudo_m4u",},
	{}
};

static const struct of_device_id mtk_pseudo_port_of_ids[] = {
	{ .compatible = "mediatek,mt-pseudo_m4u-port",},
	{}
};

int M4U_L2_ENABLE = 1;

/* garbage collect related */
#define MVA_REGION_FLAG_NONE 0x0
#define MVA_REGION_HAS_TLB_RANGE 0x1
#define MVA_REGION_REGISTER    0x2

#define F_VAL(val, msb, lsb) (((val)&((1<<(msb-lsb+1))-1))<<lsb)
#define F_VAL_L(val, msb, lsb) (((val)&((1L<<(msb-lsb+1))-1))<<lsb)
#define F_MSK(msb, lsb)     F_VAL(0xffffffff, msb, lsb)
#define F_MSK_L(msb, lsb)     F_VAL_L(0xffffffffffffffff, msb, lsb)
#define F_BIT_SET(bit)	  (1<<(bit))
#define F_BIT_VAL(val, bit)  ((!!(val))<<(bit))
#define F_MSK_SHIFT(regval, msb, lsb) (((regval)&F_MSK(msb, lsb))>>lsb)

#define SMI_LARB_NON_SEC_CONx(larb_port)	(0x380 + ((larb_port)<<2))
	#define F_SMI_NON_SEC_MMU_EN(en)	F_BIT_VAL(en, 0)
	#define F_SMI_MMU_EN          F_BIT_SET(0)


#define SMI_LARB_SEC_CONx(larb_port)	(0xf80 + ((larb_port)<<2))
	#define F_SMI_SEC_MMU_EN(en)	F_BIT_VAL(en, 0)
	#define F_SMI_SEC_EN(sec)	F_BIT_VAL(sec, 1)
	#define F_SMI_DOMN(domain)	F_VAL(domain, 8, 4)

static unsigned long pseudo_mmubase[TOTAL_M4U_NUM];
static unsigned long pseudo_larbbase[SMI_LARB_NR];
static const char *const pseudo_larbname[] = {
	"mediatek,smi_larb0", "mediatek,smi_larb1", "mediatek,smi_larb2",
	"mediatek,smi_larb3", "mediatek,smi_larb4", "mediatek,smi_larb5",
	"mediatek,smi_larb6", "mediatek,smi_larb7"
};
char *pseudo_larb_clk_name[] = {
	"m4u_smi_larb0", "m4u_smi_larb1", "m4u_smi_larb2", "m4u_smi_larb3",
	"m4u_smi_larb5", "m4u_smi_larb6", "m4u_smi_larb7", "m4u_smi_larb8"
};

struct m4u_device *pseudo_mmu_dev;

static inline unsigned int pseudo_readreg32(
						unsigned long mmuase,
						unsigned int offset)
{
	unsigned int val;

	val = ioread32((void *)(mmuase + offset));
	return val;
}

static inline void pseudo_writereg32(unsigned long mmuase,
					 unsigned int offset,
					 unsigned int val)
{
	mt_reg_sync_writel(val, (void *)(mmuase + offset));
}

static inline void pseudo_set_reg_by_mask(
					   unsigned long M4UBase,
					   unsigned int reg,
					   unsigned long mask,
					   unsigned int val)
{
	unsigned int regval;

	regval = pseudo_readreg32(M4UBase, reg);
	regval = (regval & (~mask)) | val;
	pseudo_writereg32(M4UBase, reg, regval);
}

static inline unsigned int pseudo_get_reg_by_mask(
						   unsigned long Base,
						   unsigned int reg,
						   unsigned int mask)
{
	return pseudo_readreg32(Base, reg) & mask;
}

static inline int m4u_user2kernel_port(int userport)
{
	return userport;
}

static inline int m4u_kernel2user_port(int kernelport)
{
	return kernelport;
}

static inline int m4u_get_larbid(int kernel_port)
{
	return MTK_M4U_TO_LARB(kernel_port);
}

static int m4u_port_2_larb_port(int kernel_port)
{
	return MTK_M4U_TO_PORT(kernel_port);
}

static char *m4u_get_port_name(int portID)
{
	int larbid = m4u_get_larbid(portID);

	switch (larbid) {

	case 0:
		return "M4U_LARB_0";
	case 1:
		return "M4U_LARB_1";
	case 2:
		return "M4U_LARB_2";
	case 3:
		return "M4U_LARB_3";
	case 4:
		return "M4U_LARB_4";
	case 5:
		return "M4U_LARB_5";
	case 6:
		return "M4U_LARB_6";
	case 7:
		return "M4U_LARB_7";
	default:
		M4U_MSG("invalid module id=%d.\n", portID);
		return "UNKNOWN";

	}
}


static char *m4u_get_module_name(int portID)
{
	return m4u_get_port_name(portID);
}

static struct pseudo_device larbdev[MTK_IOMMU_LARB_NR];
struct device *pseudo_get_larbdev(int portid)
{
	int larbid;
	struct pseudo_device *pseudo;

	larbid = m4u_get_larbid(portid);
	pseudo = &larbdev[0];

	if (pseudo && pseudo->dev)
		return pseudo->dev;

	M4U_MSG("could not get larbdev\n");
	return NULL;
}


static int larb_clock_on(int larb, bool config_mtcmos)
{
#ifdef CONFIG_MTK_SMI_EXT
	int ret = -1;

	if (larb < ARRAY_SIZE(pseudo_larb_clk_name))
		ret = smi_bus_prepare_enable(larb,
					     pseudo_larb_clk_name[larb],
					     true);
	if (ret != 0)
		M4U_MSG("larb_clock_on error: larb %d\n", larb);
#endif

	return 0;
}


static int larb_clock_off(int larb, bool config_mtcmos)
{
#ifdef CONFIG_MTK_SMI_EXT
	int ret = -1;

	if (larb < ARRAY_SIZE(pseudo_larb_clk_name))
		ret = smi_bus_disable_unprepare(larb,
						pseudo_larb_clk_name[larb],
						true);
	if (ret != 0)
		M4U_MSG("larb_clock_on error: larb %d\n", larb);
#endif

	return 0;
}

#ifdef M4U_TEE_SERVICE_ENABLE

#include "tz_cross/trustzone.h"
#include "trustzone/kree/system.h"
#include "tz_cross/ta_m4u.h"

KREE_SESSION_HANDLE m4u_session;
bool m4u_tee_en;

static DEFINE_MUTEX(gM4u_port_tee);
static int pseudo_session_init(void)
{
	TZ_RESULT ret;

	ret = KREE_CreateSession(TZ_TA_M4U_UUID, &m4u_session);
	if (ret != TZ_RESULT_SUCCESS) {
		M4U_MSG("m4u CreateSession error %d\n", ret);
		return -1;
	}
	M4U_MSG("create session : 0x%x\n", (unsigned int)m4u_session);
	m4u_tee_en = true;
	return 0;
}

int m4u_larb_restore_sec(unsigned int larb_idx)
{
	MTEEC_PARAM param[4];
	uint32_t paramTypes;
	TZ_RESULT ret;

	if (!m4u_tee_en)  /*tee may not init*/
		return -2;

	if (larb_idx == 0 || larb_idx == 4) { /*only support disp*/
		param[0].value.a = larb_idx;
		paramTypes = TZ_ParamTypes1(TZPT_VALUE_INPUT);

		ret = KREE_TeeServiceCall(m4u_session,
			M4U_TZCMD_LARB_REG_RESTORE,
			paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS) {
			M4U_MSG("m4u reg backup ServiceCall error %d\n", ret);
			return -1;
		}
	}
	return 0;
}

int m4u_larb_backup_sec(unsigned int larb_idx)
{
	MTEEC_PARAM param[4];
	uint32_t paramTypes;
	TZ_RESULT ret;

	if (!m4u_tee_en)  /*tee may not init */
		return -2;

	if (larb_idx == 0 || larb_idx == 4) { /*only support disp*/
		param[0].value.a = larb_idx;
		paramTypes = TZ_ParamTypes1(TZPT_VALUE_INPUT);

		ret = KREE_TeeServiceCall(m4u_session,
					M4U_TZCMD_LARB_REG_BACKUP,
					paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS) {
			M4U_MSG("m4u reg backup ServiceCall error %d\n", ret);
			return -1;
		}
	}
	return 0;
}

int smi_reg_backup_sec(void)
{
	uint32_t paramTypes;
	TZ_RESULT ret;

	paramTypes = TZ_ParamTypes1(TZPT_NONE);
	ret = KREE_TeeServiceCall(m4u_session, M4U_TZCMD_REG_BACKUP,
				paramTypes, NULL);
	if (ret != TZ_RESULT_SUCCESS) {
		M4U_MSG("m4u reg backup ServiceCall error %d\n", ret);
		return -1;
	}
	return 0;
}

int smi_reg_restore_sec(void)
{
	uint32_t paramTypes;
	TZ_RESULT ret;

	paramTypes = TZ_ParamTypes1(TZPT_NONE);
	ret = KREE_TeeServiceCall(m4u_session, M4U_TZCMD_REG_RESTORE,
				paramTypes, NULL);
	if (ret != TZ_RESULT_SUCCESS) {
		M4U_MSG("m4u reg backup ServiceCall error %d\n", ret);
		return -1;
	}

	return 0;
}

int pseudo_do_config_port(struct M4U_PORT_STRUCT *pM4uPort)
{
	MTEEC_PARAM param[4];
	uint32_t paramTypes;
	TZ_RESULT ret;

	/* do not config port if session has not been inited. */
	if (!m4u_session)
		return 0;

	param[0].value.a = pM4uPort->ePortID;
	param[0].value.b = pM4uPort->Virtuality;
	param[1].value.a = pM4uPort->Distance;
	param[1].value.b = pM4uPort->Direction;

	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);

	mutex_lock(&gM4u_port_tee);
	ret = KREE_TeeServiceCall(m4u_session,
				  M4U_TZCMD_CONFIG_PORT,
				  paramTypes, param);
	mutex_unlock(&gM4u_port_tee);

	if (ret != TZ_RESULT_SUCCESS)
		M4U_MSG("ServiceCall error 0x%x\n", ret);

	return 0;
}

static int pseudo_sec_init(unsigned int u4NonSecPa,
			      unsigned int L2_enable,
			      unsigned int *security_mem_size)
{
	MTEEC_PARAM param[4];
	uint32_t paramTypes;
	TZ_RESULT ret;

	param[0].value.a = u4NonSecPa;
	param[0].value.b = L2_enable;
	param[1].value.a = 1;
	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT,
					TZPT_VALUE_OUTPUT);
	ret = KREE_TeeServiceCall(m4u_session, M4U_TZCMD_SEC_INIT,
			paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		M4U_MSG("m4u sec init error 0x%x\n", ret);
		return -1;
	}

	*security_mem_size = param[1].value.a;
	return 0;
}

/* the caller should enable smi clock, it should be only called by mtk_smi.c*/
int pseudo_config_port_tee(int kernelport)
{
	struct M4U_PORT_STRUCT pM4uPort;

	pM4uPort.ePortID = m4u_kernel2user_port(kernelport);
	pM4uPort.Virtuality = 1;
	pM4uPort.Distance = 1;
	pM4uPort.Direction = 1;

#ifdef M4U_TEE_SERVICE_ENABLE
	return pseudo_do_config_port(&pM4uPort);
#else
	return 0;
#endif
}

#endif


/* make sure the va size is page aligned to get the continues iova. */
int m4u_va_align(unsigned long *addr, unsigned int *size)
{
	int offset, remain;

	/* we need to align the bufaddr to make sure the iova is continues */
	offset = *addr & (M4U_PAGE_SIZE - 1);
	if (offset) {
		*addr &= ~(M4U_PAGE_SIZE - 1);
		*size += offset;
	}

	/* make sure we alloc one page size iova at least */
	remain = *size % M4U_PAGE_SIZE;
	if (remain)
		*size += M4U_PAGE_SIZE - remain;
	/* dma32 would skip the last page, we added it here */
	/* *size += PAGE_SIZE; */
	return offset;
}

static inline int pseudo_config_port(struct M4U_PORT_STRUCT *pM4uPort)
{
	/* all the port will be attached by dma and configed by iommu driver */
	unsigned long larb_base;
	unsigned int larb, larb_port;
	int mmu_en = 0, mmu_old_en;

	larb = m4u_get_larbid(pM4uPort->ePortID);
	larb_port = m4u_port_2_larb_port(pM4uPort->ePortID);
	larb_base = pseudo_larbbase[larb];
	larb_clock_on(larb, 1);
	mmu_old_en = pseudo_readreg32(larb_base,
					  SMI_LARB_NON_SEC_CONx(larb_port));
	pseudo_set_reg_by_mask(larb_base,
				       SMI_LARB_NON_SEC_CONx(larb_port),
				       F_SMI_MMU_EN,
				       !!(pM4uPort->Virtuality));

	/* debug use */
	mmu_en = pseudo_readreg32(larb_base,
				      SMI_LARB_NON_SEC_CONx(larb_port));
	if (!!(mmu_en) != pM4uPort->Virtuality) {
		M4U_ERR("error,port=%d(%d-%d),%d, 0x%x-0x%x (%x,%x)\n",
			pM4uPort->ePortID, larb, larb_port,
			pM4uPort->Virtuality, mmu_old_en, mmu_en,
			pseudo_readreg32(larb_base,
					     SMI_LARB_NON_SEC_CONx(larb_port)),
					     F_SMI_MMU_EN);
	}

	larb_clock_off(larb, 1);
	return 0;
}

/*
 * reserve the iova for direct mapping.
 * without this, the direct mapping iova maybe allocated to other users,
 * and the armv7s iopgtable may assert warning and return error.
 * We reserve those iova to avoid this iova been allocated by other users.
 */
static dma_addr_t pseudo_alloc_fixed_iova(int port,
					  unsigned int fix_iova,
					  unsigned int size)
{
	struct device *dev = pseudo_get_larbdev(0);
	struct iommu_domain *domain;
	struct iova_domain *iovad;
	struct iova *iova;
	unsigned int shift = 0;
	dma_addr_t dma_addr = DMA_ERROR_CODE;

retry:
	/* in case pseudo larb has not finished probe. */
	if (!dev) {
		M4U_ERR("device is NULL and the larb have not been probed\n");
		return -EINVAL;
	}

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		M4U_MSG("get iommu_domain fail\n");
		domain = iommu_get_domain_for_dev(dev);
		cond_resched();
		goto retry;
	}

	iovad = domain->iova_cookie;
	shift = iova_shift(iovad);

	/* iovad->start_pfn is 0x1, so limit_pfn need to add 1,*/
	/* do not align the size */
	iova = alloc_iova(iovad, size >> shift,
		((fix_iova + size - 1) >> shift), false);

	if (!iova) {
		M4U_ERR("fix_iova error:port %d,req iova:0x%x-%d,shift:0x%x\n",
			port, fix_iova, size, shift);

		return dma_addr;
	}

	dma_addr = iova_dma_addr(iovad, iova);
	M4U_DBG("fix_iova %d done, 0x%x-0x%x get iova 0x%lx-0x%lx,shift %d\n",
		port, fix_iova, size, iova->pfn_lo << PAGE_SHIFT,
		iova->pfn_hi << PAGE_SHIFT, shift);

	return dma_addr;
}

static void pseudo_free_iova(int port, dma_addr_t dma_addr)
{
	struct device *dev = pseudo_get_larbdev(0);
	struct iommu_domain *domain;
	struct iova_domain *iovad;
	unsigned long shift;
	unsigned long pfn;
	struct iova *iova;
	size_t size;

	if (!dev) {
		M4U_ERR("device is NULL\n");
		return;
	}

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		M4U_MSG("get iommu_domain failed\n");
		return;
	}

	iovad = domain->iova_cookie;
	shift = iova_shift(iovad);
	pfn = dma_addr >> shift;
	iova = find_iova(iovad, pfn);

	if (WARN_ON(!iova))
		return;
	size = iova_size(iova) << shift;

	__free_iova(iovad, iova);

	M4U_MSG("pseudo_free_iova dma_addr iova 0x%llx, size %zu, done\n",
		dma_addr, size);
}



static int m4u_put_unlock_page(struct page *page)
{
	if (!page)
		return 0;

	if (!PageReserved(page))
		SetPageDirty(page);
	put_page(page);

	return 0;

}

/* to-do: need modification to support 4G DRAM */
static phys_addr_t m4u_user_v2p(unsigned long va)
{
	unsigned long pageOffset = (va & (M4U_PAGE_SIZE - 1));
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	phys_addr_t pa;

	if (!current) {
		M4U_MSG("warning: m4u_user_v2p, current is NULL, va 0x%lx!\n",
		       va);
		return 0;
	}
	if (!current->mm) {
		M4U_MSG("warning: tgid=0x%x, name=%s, va 0x%lx\n",
		       current->tgid, current->comm, va);
		return 0;
	}

	pgd = pgd_offset(current->mm, va);	/* what is tsk->mm */
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		M4U_DBG("m4u_user_v2p(), va=0x%lx, pgd invalid!\n", va);
		return 0;
	}

	pud = pud_offset(pgd, va);
	if (pud_none(*pud) || pud_bad(*pud)) {
		M4U_DBG("m4u_user_v2p(), va=0x%lx, pud invalid!\n", va);
		return 0;
	}

	pmd = pmd_offset(pud, va);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		M4U_MSG("m4u_user_v2p(), va=0x%lx, pmd invalid!\n", va);
		return 0;
	}

	pte = pte_offset_map(pmd, va);
	if (pte_present(*pte)) {
		pa = (pte_val(*pte) & (PHYS_MASK) & (PAGE_MASK)) | pageOffset;
		pte_unmap(pte);
		return pa;
	}

	pte_unmap(pte);

	M4U_DBG("m4u_user_v2p(), va=0x%lx, pte invalid!\n", va);
	return 0;
}

int __m4u_get_user_pages(int eModuleID, struct task_struct *tsk,
			 struct mm_struct *mm,
			 unsigned long start, int nr_pages,
			 unsigned int gup_flags,
			 struct page **pages, struct vm_area_struct *vmas)
{
	int i, ret;
	unsigned long vm_flags;

	if (nr_pages <= 0)
		return 0;

	/* VM_BUG_ON(!!pages != !!(gup_flags & FOLL_GET)); */
	if (!!pages != !!(gup_flags & FOLL_GET)) {
		M4U_MSG("error: !!pages != !!(gup_flags & FOLL_GET),");
		M4U_MSG("gup_flags & FOLL_GET=0x%x\n", gup_flags & FOLL_GET);
	}

	/*
	 * Require read or write permissions.
	 * If FOLL_FORCE is set, we only require the "MAY" flags.
	 */
	vm_flags = (gup_flags & FOLL_WRITE) ?
		(VM_WRITE | VM_MAYWRITE) : (VM_READ | VM_MAYREAD);
	vm_flags &= (gup_flags & FOLL_FORCE) ?
		(VM_MAYREAD | VM_MAYWRITE) : (VM_READ | VM_WRITE);
	i = 0;

	M4U_DBG("Try to get_user_pages from start vaddr 0x%lx with %d pages\n",
		start, nr_pages);

	do {
		struct vm_area_struct *vma;

		M4U_DBG("For a new vma area from 0x%lx\n", start);
		if (vmas)
			vma = vmas;
		else
			vma = find_extend_vma(mm, start);

		if (!vma) {
			M4U_MSG("error: vma not find, start=0x%x, module=%d\n",
			       (unsigned int)start, eModuleID);
			return i ? i : -EFAULT;
		}
		if (((~vma->vm_flags) &
		     (VM_IO | VM_PFNMAP | VM_SHARED | VM_WRITE)) == 0) {
			M4U_MSG("error: m4u_get_pages: bypass garbage pages!");
			M4U_MSG("vma->vm_flags=0x%x, start=0x%lx, module=%d\n",
				(unsigned int)(vma->vm_flags),
				start, eModuleID);
			return i ? i : -EFAULT;
		}
		if (vma->vm_flags & VM_IO)
			M4U_DBG("warning: vma is marked as VM_IO\n");

		if (vma->vm_flags & VM_PFNMAP) {
			M4U_MSG
			    ("error:vma permission not right,0x%x,0x%lx,%d\n",
			     (unsigned int)(vma->vm_flags), start, eModuleID);
			M4U_MSG
			    ("maybe mem is remap with un-permit vm_flags!\n");
			/* m4u_dump_maps(start); */
			return i ? i : -EFAULT;
		}
		if (!(vm_flags & vma->vm_flags)) {
			M4U_MSG
			    ("error: vm_flags invalid, 0x%x,0x%x,0x%lx,%d\n",
			     (unsigned int)vm_flags,
			     (unsigned int)(vma->vm_flags),
			     start, eModuleID);
			/* m4u_dump_maps(start); */
			return i ? : -EFAULT;
		}

		do {
			struct page *page;
			unsigned int foll_flags = gup_flags;
			/*
			 * If we have a pending SIGKILL, don't keep faulting
			 * pages and potentially allocating memory.
			 */
			if (unlikely(fatal_signal_pending(current)))
				return i ? i : -ERESTARTSYS;

			ret = get_user_pages(start, 1,
					     (vma->vm_flags & VM_WRITE),
					     &page, NULL);

			if (ret == 1)
				pages[i] = page;

			while (!page) {
				int ret;

				ret =
				    handle_mm_fault(vma, start,
						    (foll_flags & FOLL_WRITE) ?
						     FAULT_FLAG_WRITE : 0);

				if (ret & VM_FAULT_ERROR) {
					if (ret & VM_FAULT_OOM) {
						M4U_MSG("error: no memory,");
						M4U_MSG("addr:0x%lx (%d %d)\n",
							start, i, eModuleID);
						/* m4u_dump_maps(start); */
						return i ? i : -ENOMEM;
					}
					if (ret &
					    (VM_FAULT_HWPOISON |
					     VM_FAULT_SIGBUS)) {
						M4U_MSG("error: invalid va,");
						M4U_MSG("addr:0x%lx (%d %d)\n",
							start, i, eModuleID);
						/* m4u_dump_maps(start); */
						return i ? i : -EFAULT;
					}
				}
				if (ret & VM_FAULT_MAJOR)
					tsk->maj_flt++;
				else
					tsk->min_flt++;

				/*
				 * The VM_FAULT_WRITE bit tells us that
				 * do_wp_page has broken COW when necessary,
				 * even if maybe_mkwrite decided not to set
				 * pte_write. We can thus safely do subsequent
				 * page lookups as if they were reads. But only
				 * do so when looping for pte_write is futile:
				 * in some cases userspace may also be wanting
				 * to write to the gotten user page, which a
				 * read fault here might prevent (a readonly
				 * page might get reCOWed by userspace write).
				 */
				if ((ret & VM_FAULT_WRITE) &&
				    !(vma->vm_flags & VM_WRITE))
					foll_flags &= ~FOLL_WRITE;

				ret = get_user_pages(start, 1,
						     (vma->vm_flags & VM_WRITE),
						     &page, NULL);
				if (ret == 1)
					pages[i] = page;
			}
			if (IS_ERR(page)) {
				M4U_MSG("error: faulty page is returned,");
				M4U_MSG("addr:0x%lx (%d %d)\n",
					start, i, eModuleID);
				/* m4u_dump_maps(start); */
				return i ? i : PTR_ERR(page);
			}

			i++;
			start += M4U_PAGE_SIZE;
			nr_pages--;
		} while (nr_pages && start < vma->vm_end);
	} while (nr_pages);

	return i;
}

/* refer to mm/memory.c:get_user_pages() */
int m4u_get_user_pages(int eModuleID, struct task_struct *tsk,
		       struct mm_struct *mm, unsigned long start,
		       int nr_pages, int write, int force,
		       struct page **pages, struct vm_area_struct *vmas)
{
	int flags = FOLL_TOUCH;

	if (pages)
		flags |= FOLL_GET;
	if (write)
		flags |= FOLL_WRITE;
	if (force)
		flags |= FOLL_FORCE;

	return __m4u_get_user_pages(eModuleID, tsk, mm, start,
				    nr_pages, flags, pages, vmas);
}

/* /> m4u driver internal use function */
/* /> should not be called outside m4u kernel driver */
static int m4u_get_pages(int eModuleID, unsigned long BufAddr,
			 unsigned long BufSize, unsigned long *pPhys)
{
	int ret, i;
	int page_num;
	unsigned long start_pa;
	unsigned int write_mode = 0;
	struct vm_area_struct *vma = NULL;

	M4U_MSG("m4u_get_pages: module=%s,BufAddr=0x%lx,BufSize=%ld,0x%lx\n",
	       m4u_get_module_name(eModuleID), BufAddr, BufSize, PAGE_OFFSET);

	/* caculate page number */
	page_num = (BufSize + (BufAddr & 0xfff)) / M4U_PAGE_SIZE;
	if ((BufAddr + BufSize) & 0xfff)
		page_num++;

	if (BufSize > 200*1024*1024) {
		M4U_MSG("alloc size=0x%lx, bigger than limit=0x%x\n",
		     BufSize, 200*1024*1024);
		return -EFAULT;
	}

	if (BufAddr < PAGE_OFFSET) {	/* from user space */
		start_pa = m4u_user_v2p(BufAddr);
		if (!start_pa) {
			M4U_MSG("m4u_user_v2p=0 in m4u_get_pages()\n");
			return -EFAULT;
		}

		down_read(&current->mm->mmap_sem);

		vma = find_vma(current->mm, BufAddr);
		if (vma == NULL) {
			M4U_MSG("cannot find vma:module=%s,va=0x%lx-0x%lx\n",
			       m4u_get_module_name(eModuleID),
			       BufAddr, BufSize);
			up_read(&current->mm->mmap_sem);
			return -1;
		}
		write_mode = (vma->vm_flags & VM_WRITE) ? 1 : 0;
		if ((vma->vm_flags) & VM_PFNMAP) {
			unsigned long bufEnd = BufAddr + BufSize - 1;

			if (bufEnd > vma->vm_end + M4U_PAGE_SIZE) {
				M4U_MSG
				    ("error:p_num=%d,%s,va=0x%lx-0x%lx,0x%x\n",
				     page_num, m4u_get_module_name(eModuleID),
				     BufAddr, BufSize,
				     (unsigned int)vma->vm_flags);
				M4U_MSG("but vma is: start=0x%lx,end=0x%lx\n",
				       (unsigned long)vma->vm_start,
				       (unsigned long)vma->vm_end);
				up_read(&current->mm->mmap_sem);
				return -1;
			}
			up_read(&current->mm->mmap_sem);

			for (i = 0; i < page_num; i++) {
				unsigned long va_align = BufAddr &
							(~M4U_PAGE_MASK);
				unsigned long va_next;
				int err_cnt;
				unsigned int flags;

				for (err_cnt = 0; err_cnt < 30; err_cnt++) {
					va_next = va_align + 0x1000 * i;
					flags = (vma->vm_flags & VM_WRITE) ?
						FAULT_FLAG_WRITE : 0;
					*(pPhys + i) = m4u_user_v2p(va_next);
					if (!*(pPhys + i) &&
					    (va_next >= vma->vm_start) &&
					    (va_next <= vma->vm_end)) {
						handle_mm_fault(vma,
								va_next,
								flags);
						cond_resched();
					} else
						break;
				}

				if (err_cnt > 20) {
					M4U_MSG("fault_cnt %d,0x%lx,%d,0x%x\n",
					err_cnt, BufAddr, i, page_num);
					M4U_MSG("%s, va=0x%lx-0x%lx, 0x%x\n",
						m4u_get_module_name(eModuleID),
						BufAddr, BufSize,
						(unsigned int)vma->vm_flags);
					up_read(&current->mm->mmap_sem);
					return -1;
				}
			}

			M4U_MSG("%s, va=0x%lx, size=0x%lx, vm_flag=0x%x\n",
				m4u_get_module_name(eModuleID),
				BufAddr, BufSize,
				(unsigned int)vma->vm_flags);
		} else {
			ret = m4u_get_user_pages(eModuleID, current,
						 current->mm,
						 BufAddr, page_num, write_mode,
						 0, (struct page **)pPhys,
						 vma);

			up_read(&current->mm->mmap_sem);

			if (ret < page_num) {
				/* release pages first */
				for (i = 0; i < ret; i++)
					m4u_put_unlock_page((struct page *)
							(*(pPhys + i)));

				if (unlikely(fatal_signal_pending(current))) {
					M4U_MSG("error: receive sigkill when");
					M4U_MSG("get_user_pages,%d %d,%s,%s\n",
						page_num, ret,
						m4u_get_module_name(eModuleID),
						current->comm);
				}
				/*
				 * return value bigger than 0 but smaller
				 * than expected, trigger red screen
				 */
				if (ret > 0) {
					M4U_MSG("error:page_num=%d, return=%d",
						page_num, ret);
					M4U_MSG("module=%s, current_proc:%s\n",
						m4u_get_module_name(eModuleID),
						current->comm);
					M4U_MSG("maybe the allocated VA size");
					M4U_MSG("is smaller than the size");
					M4U_MSG("config to m4u_alloc_mva()!");
				} else {
					M4U_MSG("error: page_num=%d,return=%d",
						page_num, ret);
					M4U_MSG("module=%s, current_proc:%s\n",
						m4u_get_module_name(eModuleID),
						current->comm);
					M4U_MSG("maybe the VA is deallocated");
					M4U_MSG("before call m4u_alloc_mva(),");
					M4U_MSG("or no VA has beallocated!");
				}

				return -EFAULT;
			}

			for (i = 0; i < page_num; i++) {
				*(pPhys + i) =
				    page_to_phys((struct page *)
						(*(pPhys + i)));
			}
		}
	} else {		/* from kernel space */
#ifndef CONFIG_ARM64
		if (BufAddr >= VMALLOC_START && BufAddr <= VMALLOC_END) {
			/* vmalloc */
			struct page *ppage;

			for (i = 0; i < page_num; i++) {
				ppage =
				    vmalloc_to_page((unsigned int *)(BufAddr +
						    i * M4U_PAGE_SIZE));
				*(pPhys + i) = page_to_phys(ppage) &
							0xfffff000;
			}
		} else {	/* kmalloc */
#endif
			for (i = 0; i < page_num; i++)
				*(pPhys + i) = virt_to_phys((void *)((BufAddr &
							0xfffff000) +
							i * M4U_PAGE_SIZE));
#ifndef CONFIG_ARM64
		}
#endif
	}
/*get_page_exit:*/

	return page_num;
}


/* make a sgtable for virtual buffer */
#define M4U_GET_PAGE_NUM(va, size) \
	(((((unsigned long)va) & (M4U_PAGE_SIZE-1)) +\
	(size) + (M4U_PAGE_SIZE-1)) >> 12)
/*
 * the upper caller should make sure the va is page aligned
 * get the pa from va, and calc the size of pa, fill the pa into the sgtable.
 * if the va does not have pages, fill the sg_dma_address with pa.
 * We need to modify the arm_iommu_map_sg inter face.
 */
struct sg_table *pseudo_get_sg(int portid, unsigned long va, int size)
{
	int i, page_num, ret, have_page, get_pages;
	struct sg_table *table;
	struct scatterlist *sg;
	struct page *page;
	struct vm_area_struct *vma = NULL;
	unsigned long *pPhys;

	page_num = M4U_GET_PAGE_NUM(va, size);
	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table) {
		M4U_MSG("kzalloc failed table is null.\n");
		return NULL;
	}

	ret = sg_alloc_table(table, page_num, GFP_KERNEL);
	if (ret) {
		M4U_MSG("sg alloc table failed %d. page_num is %d\n",
			ret, page_num);
		kfree(table);
		return NULL;
	}
	sg = table->sgl;

	pPhys = vmalloc(page_num * sizeof(unsigned long *));
	if (pPhys == NULL) {
		M4U_MSG("m4u_fill_pagetable : error to vmalloc %d*4 size\n",
		       page_num);
		goto err_free;
	}
	get_pages = m4u_get_pages(portid, va, size, pPhys);
	if (get_pages <= 0) {
		M4U_DBG("Error : m4u_get_pages failed\n");
		goto err_free;
	}

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, va);
	if (vma && vma->vm_flags & VM_PFNMAP)
		have_page = 0;
	else
		have_page = 1;
	up_read(&current->mm->mmap_sem);
	for (i = 0; i < page_num; i++) {
		va += i * M4U_PAGE_SIZE;

		if (((pPhys[i] & (M4U_PAGE_SIZE - 1)) !=
		     (va & (M4U_PAGE_SIZE - 1)) || !pPhys[i])
			&& (i != page_num - 1)) {
			M4U_MSG("m4u user v2p failed, pa is 0x%lx\n",
				pPhys[i]);
		}


		if (!pPhys[i] && i < page_num - 1) {
			M4U_MSG("get pa failed, pa is 0. 0x%lx, %d, %d, %s\n",
				va, page_num, i,
				m4u_get_port_name(portid));
			goto err_free;
		}

		if (have_page) {
			page = phys_to_page(pPhys[i]);
			sg_set_page(sg, page, M4U_PAGE_SIZE, 0);
		} else {
			/*
			 * the pa must not be set to zero or DMA would omit
			 * this page and then the mva allocation would be
			 * failed. So just make the last pages's pa as it's
			 * previous page plus page size. It's ok to do so since
			 * the hw would not access this very last page. DMA
			 * would like to ovmit the very last sg if the pa is 0
			 */
			if ((i == page_num - 1) && (pPhys[i] == 0)) {
				/* i == 0 should be take care of specially. */
				if (i)
					pPhys[i] = pPhys[i - 1] +
							M4U_PAGE_SIZE;
				else
					pPhys[i] = M4U_PAGE_SIZE;
			}

			sg_dma_address(sg) = pPhys[i];
			sg_dma_len(sg) = M4U_PAGE_SIZE;
		}
		sg = sg_next(sg);
	}

	vfree(pPhys);

	return table;

err_free:
	sg_free_table(table);
	kfree(table);
	if (pPhys)
		vfree(pPhys);

	return NULL;
}

struct sg_table *pseudo_find_sgtable(unsigned int mva)
{
	struct mva_sglist *entry;

	mutex_lock(&pseudo_list_mutex);

	list_for_each_entry(entry, &pseudo_sglist, list) {
		if (entry->mva == mva) {
			mutex_unlock(&pseudo_list_mutex);
			return entry->table;
		}
	}

	mutex_unlock(&pseudo_list_mutex);
	return NULL;
}

struct sg_table *pseudo_add_sgtable(struct mva_sglist *mva_sg)
{
	struct sg_table *table;

	table = pseudo_find_sgtable(mva_sg->mva);
	if (table)
		return table;

	table = mva_sg->table;
	mutex_lock(&pseudo_list_mutex);
	list_add(&mva_sg->list, &pseudo_sglist);
	mutex_unlock(&pseudo_list_mutex);

	M4U_DBG("adding pseudo_sglist, mva = 0x%x\n", mva_sg->mva);
	return table;
}

static int __pseudo_alloc_mva(int port, unsigned long va, unsigned int size,
			   struct sg_table *sg_table, unsigned int flags,
			   unsigned int *retmva)
{
	struct mva_sglist *mva_sg;
	struct sg_table *table = NULL;
	int ret, kernelport = m4u_user2kernel_port(port);
	struct device *dev = pseudo_get_larbdev(kernelport);
	struct iommu_domain *domain;
	struct iova_domain *iovad;
	dma_addr_t dma_addr = DMA_ERROR_CODE;

	if (va && sg_table) {
		M4U_DBG("va/sg 0x%x are valid:0x%lx, 0x%p, 0x%x, 0x%x-0x%x\n",
		       port, va, sg_table, flags, *retmva, size);
	}
	if (dev == NULL) {
		M4U_MSG("dev NULL!\n");
		return -1;
	}
	domain = iommu_get_domain_for_dev(dev);

	if (!va && !sg_table) {
		M4U_MSG("va and sg_table are all NULL\n");
		return -EINVAL;
	}

	/* this is for ion mm heap and ion fb heap usage. */
	if (sg_table) {
		if ((flags & M4U_FLAGS_SG_READY) == 0) {
			struct scatterlist *s = sg_table->sgl, *ng;
			phys_addr_t phys;
			int i;

			table = kzalloc(sizeof(*table), GFP_KERNEL);
			ret = sg_alloc_table(table,
					     sg_table->nents,
					     GFP_KERNEL);
			if (ret) {
				kfree(table);
				*retmva = 0;
				return ret;
			}

			ng = table->sgl;
			size = 0;

			for (i = 0; i < sg_table->nents; i++) {
				phys = sg_phys(s);
				size += s->length;
				sg_set_page(ng, sg_page(s), s->length, 0);
				s = sg_next(s);
				ng = sg_next(ng);
			}
		} else
			table = sg_table;

	}

	if (!table) {
		table = pseudo_get_sg(port, va, size);
		if (!table) {
			M4U_MSG("pseudo_get_sg failed\n");
			goto ERR2_EXIT;
		}
	}

	if ((flags & (M4U_FLAGS_FIX_MVA | M4U_FLAGS_START_FROM)) != 0) {
		dma_addr = pseudo_alloc_fixed_iova(port, *retmva, size);
		if (iommu_map_sg(domain, dma_addr, table->sgl, sg_table ?
				 table->nents : table->orig_nents,
				 IOMMU_READ | IOMMU_WRITE) < size) {
			goto ERR1_EXIT;
		}
		sg_dma_address(table->sgl) = dma_addr;
	} else {
		iommu_dma_map_sg(dev, table->sgl,
				 sg_table ? table->nents : table->orig_nents,
				 IOMMU_READ | IOMMU_WRITE);
		dma_addr = sg_dma_address(table->sgl);
	}

	if (dma_addr == DMA_ERROR_CODE) {
		M4U_ERR("alloc mva failed, port is %s, dma:0x%lx s:0x%x\n",
			m4u_get_port_name(port),
			(unsigned long)dma_addr, size);
		M4U_ERR("SUSPECT that iova have been all exhaust\n");
		goto ERR1_EXIT;
	}

	*retmva = dma_addr;

	mva_sg = kzalloc(sizeof(*mva_sg), GFP_KERNEL);
	mva_sg->table = table;
	mva_sg->mva = *retmva;

	pseudo_add_sgtable(mva_sg);

	if (!domain) {
		M4U_MSG("iovad para pfn2, get iommu_domain fail\n");
	} else {
		iovad = domain->iova_cookie;
		M4U_DBG("pfn2 0x%x, 0x%lx, %p, 0x%lx, 0x%lx,0x%lx-0x%x-0x%x\n",
			port, iovad->dma_32bit_pfn, iovad->cached32_node,
			iovad->granule, iovad->start_pfn,
			dma_get_seg_boundary(dev), *retmva, flags);
	}

	M4U_DBG("mva is 0x%x, dma_address is 0x%lx, size is 0x%x\n",
		mva_sg->mva, (unsigned long)dma_addr, size);

	mtk_iommu_trace_log(IOMMU_ALLOC, dma_addr, size, flags | (port << 16));
	return 0;

ERR1_EXIT:

	M4U_MSG("iommu_map_sg failed\n");
	if (dma_addr != DMA_ERROR_CODE)
		pseudo_free_iova(port, dma_addr);

ERR2_EXIT:
	if (table) {
		sg_free_table(table);
		kfree(table);
	}
	*retmva = 0;
	return -EINVAL;
}

static struct m4u_buf_info_t *pseudo_alloc_buf_info(void)
{
	struct m4u_buf_info_t *pList = NULL;

	pList = kzalloc(sizeof(struct m4u_buf_info_t), GFP_KERNEL);
	if (pList == NULL) {
		M4U_MSG("pList=0x%p\n", pList);
		return NULL;
	}
	M4U_DBG("pList size %d, ptr %p\n", (int)sizeof(struct m4u_buf_info_t),
		pList);
	INIT_LIST_HEAD(&(pList->link));
	return pList;
}

static int pseudo_free_buf_info(struct m4u_buf_info_t *pList)
{
	kfree(pList);
	return 0;
}

static int pseudo_client_add_buf(struct m4u_client_t *client,
			      struct m4u_buf_info_t *pList)
{
	mutex_lock(&(client->dataMutex));
	list_add(&(pList->link), &(client->mvaList));
	mutex_unlock(&(client->dataMutex));

	return 0;
}

/*
 * find or delete a buffer from client list
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
static struct m4u_buf_info_t *pseudo_client_find_buf(
						  struct m4u_client_t *client,
						  unsigned int mva,
						  int del)
{
	struct list_head *pListHead;
	struct m4u_buf_info_t *pList = NULL;
	struct m4u_buf_info_t *ret = NULL;

	if (client == NULL) {
		M4U_ERR("m4u_delete_from_garbage_list(), client is NULL!\n");
		return NULL;
	}

	mutex_lock(&(client->dataMutex));
	list_for_each(pListHead, &(client->mvaList)) {
		pList = container_of(pListHead, struct m4u_buf_info_t, link);
		if (pList->mva == mva)
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

/* interface for ion */
static struct m4u_client_t *ion_m4u_client;
struct m4u_client_t *pseudo_create_client(void)
{
	struct m4u_client_t *client;

	client = kmalloc(sizeof(struct m4u_client_t), GFP_ATOMIC);
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
EXPORT_SYMBOL(pseudo_create_client);

int pseudo_alloc_mva_sg(struct port_mva_info_t *port_info,
				struct sg_table *sg_table)
{
	unsigned int flags = 0;
	int ret, offset;
	struct m4u_buf_info_t *pbuf_info;
	unsigned int mva = 0;
	unsigned long va_align;
	unsigned int *pMva;
	unsigned int mva_align;
	unsigned int size_align = port_info->bufsize;

	if (!ion_m4u_client) {
		ion_m4u_client = pseudo_create_client();
		if (IS_ERR_OR_NULL(ion_m4u_client)) {
			ion_m4u_client = NULL;
			return -1;
		}
	}

	if (port_info->flags & M4U_FLAGS_FIX_MVA) {
		if (port_info->iova_end >
		    (port_info->iova_start + port_info->bufsize)) {
			port_info->mva = (port_info->iova_end - size_align);
			flags = M4U_FLAGS_START_FROM;
		} else
			flags = M4U_FLAGS_FIX_MVA;
	}
	if (port_info->flags & M4U_FLAGS_SG_READY)
		flags |= M4U_FLAGS_SG_READY;
	else
		port_info->va = 0;

	va_align = port_info->va;
	pMva = &port_info->mva;
	mva_align = *pMva;

	/* align the va to allocate continues iova. */
	offset = m4u_va_align(&va_align, &size_align);
	/* pbuf_info for userspace compatible */
	pbuf_info = pseudo_alloc_buf_info();

	pbuf_info->va = port_info->va;
	pbuf_info->port = port_info->eModuleID;
	pbuf_info->size = port_info->bufsize;
	pbuf_info->flags = flags;
	pbuf_info->sg_table = sg_table;

	ret = __pseudo_alloc_mva(port_info->eModuleID, va_align, size_align,
			      sg_table, flags, &mva_align);
	if (ret) {
		M4U_MSG("error alloc mva: port %d, 0x%x, 0x%lx, 0x%x, 0x%x\n",
			port_info->eModuleID, flags, port_info->va,
			mva, port_info->bufsize);
		mva = 0;
		goto err;
	}

	mva = mva_align + offset;
	pbuf_info->mva = mva;
	pbuf_info->mva_align = mva_align;
	pbuf_info->size_align = size_align;
	*pMva = mva;

	pseudo_client_add_buf(ion_m4u_client, pbuf_info);
	M4U_DBG("port 0x%x, flags 0x%x, va 0x%lx, mva = 0x%x, size 0x%x\n",
		port_info->eModuleID, flags,
		port_info->va, mva, port_info->bufsize);

	return 0;
err:
	pseudo_free_buf_info(pbuf_info);
	return ret;
}

struct sg_table *pseudo_del_sgtable(unsigned int mva)
{
	struct mva_sglist *entry, *tmp;
	struct sg_table *table = NULL;

	M4U_DBG("mva = 0x%x\n", mva);
	mutex_lock(&pseudo_list_mutex);
	list_for_each_entry_safe(entry, tmp, &pseudo_sglist, list) {
		M4U_DBG("entry->mva = 0x%x\n",
			entry->mva);
		if (entry->mva == mva) {
			list_del(&entry->list);
			mutex_unlock(&pseudo_list_mutex);
			table = entry->table;
			M4U_DBG("mva is 0x%x, entry->mva is 0x%x\n",
				mva, entry->mva);
			kfree(entry);
			return table;
		}
	}
	mutex_unlock(&pseudo_list_mutex);

	return NULL;
}

/* put ref count on all pages in sgtable */
int pseudo_put_sgtable_pages(struct sg_table *table, int nents)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(table->sgl, sg, nents, i) {
		struct page *page = sg_page(sg);

		if (IS_ERR(page))
			return 0;
		if (page) {
			if (!PageReserved(page))
				SetPageDirty(page);
			put_page(page);
		}
	}
	return 0;
}

/* the caller should make sure the mva offset have been eliminated. */
int __pseudo_dealloc_mva(int port,
		      const unsigned long BufAddr,
		      const unsigned int size,
		      const unsigned int MVA,
		      struct sg_table *sg_table)
{
	struct sg_table *table = NULL;
	int kernelport = m4u_user2kernel_port(port);
	struct device *dev = pseudo_get_larbdev(0);
	struct iommu_domain *domain;
	unsigned long addr_align = MVA;
	unsigned int size_align = size;
	int offset;

	if (!dev) {
		M4U_MSG("dev is NULL\n");
		return -EINVAL;
	}

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		M4U_MSG("domain is NULL\n");
		return -EINVAL;
	}

	M4U_DBG("module=0x%x, addr=0x%lx, size=0x%x, MVA=0x%x, mva_end=0x%x\n",
		port, BufAddr, size, MVA, MVA + size - 1);

	/* for ion sg alloc, we did not align the mva in allocation. */
	/* if (!sg_table) */
	offset = m4u_va_align(&addr_align, &size_align);

	if (sg_table) {
		struct m4u_buf_info_t *m4u_buf_info;

		m4u_buf_info = pseudo_client_find_buf(ion_m4u_client,
						      addr_align, 1);
		if (m4u_buf_info && m4u_buf_info->mva != addr_align)
			M4U_MSG("warning: mva address are not same\n");
		table = pseudo_del_sgtable(addr_align);
		if (!table) {
			M4U_ERR("can't found the table from mva 0x%x-0x%lx\n",
				MVA, addr_align);
			M4U_ERR("%s, addr=0x%lx,size=0x%x,MVA=0x%x-0x%x\n",
				m4u_get_module_name(kernelport),
				BufAddr, size, MVA, MVA + size - 1);
			dump_stack();
			return -EINVAL;
		}

		if (sg_page(table->sgl) != sg_page(sg_table->sgl)) {
			M4U_MSG("error, sg have not been added\n");
			return -EINVAL;
		}

		pseudo_free_buf_info(m4u_buf_info);
	}

	if (!table)
		table = pseudo_del_sgtable(addr_align);

	mtk_iommu_trace_log(IOMMU_DEALLOC, MVA, size, (port << 16));

	if (table)
		iommu_dma_unmap_sg(dev, table->sgl, table->orig_nents, 0, 0);
	else {
		M4U_ERR("can't find the sgtable and would return error\n");
		return -EINVAL;
	}


	if (BufAddr) {
		/* from user space */
		if (BufAddr < PAGE_OFFSET) {
			struct vm_area_struct *vma = NULL;

			M4UTRACE();
			if (current->mm) {
				down_read(&current->mm->mmap_sem);
				vma = find_vma(current->mm, BufAddr);
			} else if (current->active_mm) {
				down_read(&current->active_mm->mmap_sem);
				vma = NULL;
			}
			M4UTRACE();
			if (vma == NULL) {
				M4U_MSG("can't find vma:%s,va:0x%lx-0x%x\n",
				       m4u_get_module_name(port),
				       BufAddr, size);
				if (current->mm)
					up_read(&current->mm->mmap_sem);
				else if (current->active_mm)
					up_read(&current->active_mm->mmap_sem);
				goto out;
			}
			if ((vma->vm_flags) & VM_PFNMAP) {
				if (current->mm)
					up_read(&current->mm->mmap_sem);
				else if (current->active_mm)
					up_read(&current->active_mm->mmap_sem);
				goto out;
			}
			if (current->mm)
				up_read(&current->mm->mmap_sem);
			else if (current->active_mm)
				up_read(&current->active_mm->mmap_sem);
			if (!((BufAddr >= VMALLOC_START) &&
				(BufAddr <= VMALLOC_END)))
				if (!sg_table) {
					if (BufAddr +  size < vma->vm_end)
						pseudo_put_sgtable_pages(
							table,
							table->nents);
					else
						pseudo_put_sgtable_pages(
							table,
							table->nents - 1);
				}
		}
	}
out:
	if (table) {
		sg_free_table(table);
		kfree(table);
	}

	M4UTRACE();
	return 0;

}

int pseudo_dealloc_mva(struct m4u_client_t *client, int port, unsigned int mva)
{
	struct m4u_buf_info_t *pMvaInfo;
	int offset, ret;

	pMvaInfo = pseudo_client_find_buf(client, mva, 1);

	offset = m4u_va_align(&pMvaInfo->va, &pMvaInfo->size);
	pMvaInfo->mva -= offset;

	ret = __pseudo_dealloc_mva(port, pMvaInfo->va, pMvaInfo->size,
				   mva, NULL);

	M4U_DBG("port %d, flags 0x%x, va 0x%lx, mva = 0x%x, size 0x%x\n",
		port, pMvaInfo->flags,
		pMvaInfo->va, mva, pMvaInfo->size);
	if (ret)
		return ret;

	pseudo_free_buf_info(pMvaInfo);
	return ret;

}

int pseudo_dealloc_mva_sg(int eModuleID,
		       struct sg_table *sg_table,
		       const unsigned int BufSize, const unsigned int MVA)
{
	if (!sg_table) {
		M4U_MSG("sg_table is NULL\n");
		return -EINVAL;
	}

	return __pseudo_dealloc_mva(eModuleID, 0, BufSize, MVA, sg_table);
}

int pseudo_destroy_client(struct m4u_client_t *client)
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
					struct m4u_buf_info_t,
					link);
		M4U_MSG
		    ("warn: clean garbage: %s,va=0x%lx,mva=0x%x,size=%d\n",
		     m4u_get_port_name(pMvaInfo->port), pMvaInfo->va,
		     pMvaInfo->mva,
		     pMvaInfo->size);

		port = pMvaInfo->port;
		mva = pMvaInfo->mva;
		size = pMvaInfo->size;

		mutex_unlock(&(client->dataMutex));

		/* m4u_dealloc_mva will lock client->dataMutex again */
		pseudo_dealloc_mva(client, port, mva);
	}

	kfree(client);

	return 0;
}
EXPORT_SYMBOL(pseudo_destroy_client);

static int m4u_fill_sgtable_user(struct vm_area_struct *vma,
				 unsigned long va,
				 int page_num,
				 struct scatterlist **pSg, int has_page)
{
	unsigned long va_align;
	phys_addr_t pa = 0;
	int i;
	long ret = 0;
	struct scatterlist *sg = *pSg;
	struct page *pages;
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
							(vma->
							 vm_flags & VM_WRITE) ?
							FAULT_FLAG_WRITE : 0);
				}
			}

			if (pa) {
				/* Add one line comment for coding style */
				break;
			}
			cond_resched();
		}

		if (!pa || !sg) {
			struct vm_area_struct *vma_temp;

			M4U_MSG("%s: fail(0x%lx) va=0x%lx,page_num=0x%x\n",
				__func__, ret, va, page_num);
			M4U_MSG("%s: fail_va=0x%lx,pa=0x%lx,sg=0x%p,i=%d\n",
				__func__, va_tmp, (unsigned long)pa, sg, i);
			vma_temp = find_vma(current->mm, va_tmp);
			if (vma_temp != NULL) {
				M4U_MSG("vma start=0x%lx,end=%lx,flag=%lx\n",
					vma->vm_start,
					vma->vm_end,
					vma->vm_flags);
				M4U_MSG("temp start=0x%lx,end=%lx,flag=%lx\n",
					vma_temp->vm_start,
					vma_temp->vm_end,
					vma_temp->vm_flags);
			}

			return -1;
		}

		if (fault_cnt > 2)
			M4U_MSG("warning: handle_mm_fault for %d times\n",
			       fault_cnt);

		/* debug check... */
		if ((pa & (PAGE_SIZE - 1)) != 0) {
			M4U_MSG("pa error, pa:0x%lx, va:0x%lx, align:0x%lx\n",
				   (unsigned long)pa, va_tmp, va_align);
		}

		if (has_page) {
			struct page *page;

			page = phys_to_page(pa);
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

static int m4u_create_sgtable_user(unsigned long va_align,
				   struct sg_table *table)
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
			M4U_MSG("cannot find vma: va=0x%lx, vma=0x%p\n",
			       va, vma);
			if (vma != NULL) {
				M4U_MSG("start=0x%lx,end=0x%lx,flag=0x%lx\n",
					vma->vm_start,
					vma->vm_end,
					vma->vm_flags);
			}
			///m4u_dump_mmaps(va);
			ret = -1;
			goto out;
		} else {
			/* M4ULOG_MID("%s va: 0x%lx, vma->vm_start=0x%lx, */
			/* vma->vm_end=0x%lx\n",*/
			/*__func__, va, vma->vm_start, vma->vm_end); */
		}

		vma_page_num = (vma->vm_end - va) / PAGE_SIZE;
		vma_page_num = min(vma_page_num, left_page_num);

		if ((vma->vm_flags) & VM_PFNMAP) {
			/* ion va or ioremap vma has this flag */
			/* VM_PFNMAP: Page-ranges managed without */
			/* "struct page", just pure PFN */
			ret = m4u_fill_sgtable_user(vma, va,
						    vma_page_num, &sg, 0);
			M4U_MSG("VM_PFNMAP va=0x%lx, page_num=0x%x\n",
				va,
				vma_page_num);
		} else {
			/* Add one line comment for avoid kernel coding style*/
			/* WARNING:BRACES: */
			ret = m4u_fill_sgtable_user(vma, va,
						    vma_page_num, &sg, 1);
			if (-1 == ret) {
				struct vm_area_struct *vma_temp;

				vma_temp = find_vma(current->mm, va_align);
				if (!vma_temp) {
					M4U_MSG("vma NUll for va 0x%lx\n",
						va_align);
					goto out;
				}
				M4U_MSG("start=0x%lx,end=0x%lx,flag=0x%lx\n",
					vma_temp->vm_start,
					vma_temp->vm_end,
					vma_temp->vm_flags);
			}
		}
		if (ret) {
			/* Add one line comment for avoid kernel coding */
			/* style, WARNING:BRACES: */
			goto out;
		}

		left_page_num -= vma_page_num;
		va += vma_page_num * PAGE_SIZE;
	}

out:
	up_read(&current->mm->mmap_sem);
	return ret;
}

struct sg_table *m4u_create_sgtable(unsigned long va, unsigned int size)
{
	struct sg_table *table;
	int ret, i, page_num;
	unsigned long va_align;
	phys_addr_t pa;
	struct scatterlist *sg;
	struct page *page;
	unsigned int psize = PAGE_SIZE;

	page_num = M4U_GET_PAGE_NUM(va, size);
	va_align = round_down(va, PAGE_SIZE);

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		M4U_MSG("table kmalloc fail:va=0x%lx,size=0x%x,page_num=%d\n",
			va, size, page_num);
		return ERR_PTR(-ENOMEM);
	}

	ret = sg_alloc_table(table, page_num, GFP_KERNEL);
	if (ret) {
		kfree(table);
		M4U_MSG("alloc_sgtable fail: va=0x%lx,size=0x%x,page_num=%d\n",
			va, size, page_num);
		return ERR_PTR(-ENOMEM);
	}

	if (va < PAGE_OFFSET) {	/* from user space */
		if (va >= VMALLOC_START && va <= VMALLOC_END) {	/* vmalloc */
			M4U_MSG(" from user space vmalloc, va = 0x%lx\n", va);
			for_each_sg(table->sgl, sg, table->nents, i) {
				page = vmalloc_to_page((void *)(va_align +
								i * psize));
				if (!page) {
					M4U_MSG("va_to_page fail, va=0x%lx\n",
					       va_align + i * psize);
					goto err;
				}
				sg_set_page(sg, page, psize, 0);
			}
		} else {
			ret = m4u_create_sgtable_user(va_align, table);
			if (ret) {
				M4U_MSG("error va=0x%lx, size=%d\n",
					va, size);
				goto err;
			}
		}
	} else {		/* from kernel sp */
		if (va >= VMALLOC_START && va <= VMALLOC_END) {	/* vmalloc */
			M4U_MSG(" from kernel space vmalloc, va = 0x%lx", va);
			for_each_sg(table->sgl, sg, table->nents, i) {
				page = vmalloc_to_page((void *)(va_align +
								i * psize));
				if (!page) {
					M4U_MSG("va_to_page fail, va=0x%lx\n",
					       va_align + i * psize);
					goto err;
				}
				sg_set_page(sg, page, psize, 0);
			}
		} else {	/* kmalloc to-do: use one entry sgtable. */
			for_each_sg(table->sgl, sg, table->nents, i) {
				pa = virt_to_phys((void *)(va_align +
						 i * psize));
				page = phys_to_page(pa);
				sg_set_page(sg, page, psize, 0);
			}
		}
	}
	return table;

err:
	sg_free_table(table);
	kfree(table);
	return ERR_PTR(-EFAULT);
}

int m4u_dealloc_mva_sg(int eModuleID,
		 struct sg_table *sg_table,
			 const unsigned int BufSize, const unsigned int MVA)
{
	return pseudo_dealloc_mva_sg(eModuleID, sg_table, BufSize, MVA);
}

int m4u_alloc_mva_sg(struct port_mva_info_t *port_info,
		  struct sg_table *sg_table)
{
	return pseudo_alloc_mva_sg(port_info, sg_table);
}

static int pseudo_open(struct inode *inode, struct file *file)
{
	struct m4u_client_t *client;

	M4U_DBG("enter pseudo_open() process : %s\n", current->comm);
	client = pseudo_create_client();
	if (IS_ERR_OR_NULL(client)) {
		M4U_MSG("createclientfail\n");
		return -ENOMEM;
	}

	file->private_data = client;

	return 0;
}


int m4u_config_port(struct M4U_PORT_STRUCT *pM4uPort)
{
	int ret;

#ifdef M4U_TEE_SERVICE_ENABLE
	ret = pseudo_do_config_port(pM4uPort);
#else
	ret = pseudo_config_port(pM4uPort);
#endif

	return ret;
}

static int pseudo_release(struct inode *inode, struct file *file)
{
	struct m4u_client_t *client = file->private_data;

	M4U_DBG("enter pseudo_release() process : %s\n", current->comm);
	pseudo_destroy_client(client);
	return 0;
}

static int pseudo_flush(struct file *a_pstFile, fl_owner_t a_id)
{
	return 0;
}

/*
 * inherent this from original m4u driver, we use this to make sure
 * we could still support
 * userspace ioctl commands.
 */
static long pseudo_ioctl(struct file *filp,
			  unsigned int cmd,
			  unsigned long arg)
{
	int ret = 0;
#ifdef M4U_TEE_SERVICE_ENABLE
	struct M4U_PORT_STRUCT m4u_port;
#endif

	switch (cmd) {
#ifdef M4U_TEE_SERVICE_ENABLE
	case MTK_M4U_T_CONFIG_PORT:
		{
			ret = copy_from_user(&m4u_port, (void *)arg,
					     sizeof(struct M4U_PORT_STRUCT));
			if (ret) {
				M4U_MSG("copy_from_user failed:%d\n", ret);
				return -EFAULT;
			}

			ret = m4u_config_port(&m4u_port);
		}
		break;
	case MTK_M4U_T_CONFIG_PORT_ARRAY:
		{
			struct m4u_port_array port_array;

			ret = copy_from_user(&port_array,
					     (void *)arg,
					     sizeof(struct m4u_port_array));
			if (ret) {
				M4U_MSG("copy_from_user failed:%d\n", ret);
				return -EFAULT;
			}

			/*ret = pseudo_config_port_array(&port_array);*/
		}
		break;
#endif
	default:
		M4U_MSG("MTK M4U ioctl:No such command(0x%x)!!\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)

static int compat_get_module_struct(
		struct COMPAT_M4U_MOUDLE_STRUCT __user *data32,
		struct M4U_MOUDLE_STRUCT __user *data)
{
	compat_uint_t u;
	compat_ulong_t l;
	int err;

	err = get_user(u, &(data32->port));
	err |= put_user(u, &(data->port));
	err |= get_user(l, &(data32->BufAddr));
	err |= put_user(l, &(data->BufAddr));
	err |= get_user(u, &(data32->BufSize));
	err |= put_user(u, &(data->BufSize));
	err |= get_user(u, &(data32->prot));
	err |= put_user(u, &(data->prot));
	err |= get_user(u, &(data32->MVAStart));
	err |= put_user(u, &(data->MVAStart));
	err |= get_user(u, &(data32->MVAEnd));
	err |= put_user(u, &(data->MVAEnd));
	err |= get_user(u, &(data32->flags));
	err |= put_user(u, &(data->flags));

	return err;
}

static int compat_put_module_struct(
		struct COMPAT_M4U_MOUDLE_STRUCT __user *data32,
		struct M4U_MOUDLE_STRUCT __user *data)
{
	compat_uint_t u;
	compat_ulong_t l;
	int err;


	err = get_user(u, &(data->port));
	err |= put_user(u, &(data32->port));
	err |= get_user(l, &(data->BufAddr));
	err |= put_user(l, &(data32->BufAddr));
	err |= get_user(u, &(data->BufSize));
	err |= put_user(u, &(data32->BufSize));
	err |= get_user(u, &(data->prot));
	err |= put_user(u, &(data32->prot));
	err |= get_user(u, &(data->MVAStart));
	err |= put_user(u, &(data32->MVAStart));
	err |= get_user(u, &(data->MVAEnd));
	err |= put_user(u, &(data32->MVAEnd));
	err |= get_user(u, &(data->flags));
	err |= put_user(u, &(data32->flags));

	return err;
}


long pseudo_compat_ioctl(struct file *filp,
			     unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_MTK_M4U_T_ALLOC_MVA:
		{
			struct COMPAT_M4U_MOUDLE_STRUCT __user *data32;
			struct M4U_MOUDLE_STRUCT __user *data;
			int err;
			int module_size = sizeof(struct M4U_MOUDLE_STRUCT);

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(module_size);
			if (data == NULL)
				return -EFAULT;

			err = compat_get_module_struct(data32, data);
			if (err)
				return err;

			ret = filp->f_op->unlocked_ioctl(filp,
							 MTK_M4U_T_ALLOC_MVA,
							 (unsigned long)data);

			err = compat_put_module_struct(data32, data);

			if (err)
				return err;
		}
		break;
	case COMPAT_MTK_M4U_T_DEALLOC_MVA:
		{
			struct COMPAT_M4U_MOUDLE_STRUCT __user *data32;
			struct M4U_MOUDLE_STRUCT __user *data;
			int err;
			int module_size = sizeof(struct M4U_MOUDLE_STRUCT);

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(module_size);
			if (data == NULL)
				return -EFAULT;

			err = compat_get_module_struct(data32, data);
			if (err)
				return err;

			ret = filp->f_op->unlocked_ioctl(filp,
							  MTK_M4U_T_DEALLOC_MVA,
							  (unsigned long)data);
		}
		break;
	default:
		M4U_MSG("compat ioctl:No such command(0x%x)!!\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#else

#define pseudo_compat_ioctl  NULL

#endif

#ifdef M4U_TEE_SERVICE_ENABLE
/* reserve iova address range for security world for 1GB memory */
static int __reserve_iova_sec(struct device *device,
				dma_addr_t dma_addr, size_t size)
{
	struct device *dev = pseudo_get_larbdev(0);
	struct iommu_domain *domain;
	struct iova_domain *iovad;
	struct iova *iova;
	unsigned long sec_mem_size = size >> PAGE_SHIFT;

retry:
	/* in case pseudo larb has not finished probe. */
	if (!dev && !device) {
		M4U_ERR("device is NULL and the larb have not been probed\n");
		return -EINVAL;
	} else if (!dev && device) {
		dev = device;
	}

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		M4U_MSG("get iommu_domain failed\n");
		domain = iommu_get_domain_for_dev(dev);
		cond_resched();
		goto retry;
	}

	iovad = domain->iova_cookie;

	/* iovad->start_pfn is 0x1, so limit_pfn need to add 1, */
	/* do not align the size */
	iova = alloc_iova(iovad, sec_mem_size,
		sec_mem_size + 1, false);

	if (!iova) {
		M4U_ERR("%s pseudo alloc_iova failed\n",
			dev_name(data->dev));
		return -1;
	}

	M4U_DBG("reserve iova for sec world success, get iova 0x%lx-0x%lx\n",
		iova->pfn_lo << PAGE_SHIFT, iova->pfn_hi << PAGE_SHIFT);

	return 0;
}
#endif

/*
 * reserve the iova for direct mapping.
 * without this, the direct mapping iova maybe allocated to other users,
 * and the armv7s iopgtable may assert warning and return error.
 * We reserve those iova to avoid this iova been allocated by other users.
 */
static int pseudo_reserve_dm(void)
{
	struct iommu_dm_region *entry;
	struct list_head mappings;
	struct device *dev = pseudo_get_larbdev(0);
	struct iommu_domain *domain;
	struct iova_domain *iovad;
	struct iova *iova;
	unsigned long pg_size = SZ_4K, limit, shift;

	if (!dev)
		return -1;

	INIT_LIST_HEAD(&mappings);
	iommu_get_dm_regions(dev, &mappings);

	/* We need to consider overlapping regions for different devices */
	list_for_each_entry(entry, &mappings, list) {
		dma_addr_t start;

		start = ALIGN(entry->start, pg_size);
retry:
		domain = iommu_get_domain_for_dev(dev);
		if (!domain) {
			M4U_MSG("get iommu_domain failed\n");
			domain = iommu_get_domain_for_dev(dev);
			cond_resched();
			goto retry;
		}

		iovad = domain->iova_cookie;

		M4U_MSG("pfn 0x%lx, node %p, granule 0x%lx, start_pfn 0x%lx\n",
			iovad->dma_32bit_pfn, iovad->cached32_node,
			iovad->granule, iovad->start_pfn);

		shift = iova_shift(iovad);
		limit = (start + entry->length) >> shift;
		/* add plus one page for the size of allocation */
		/* or there maybe overlap */
		iova = alloc_iova(iovad, (entry->length >> shift) + 1,
				  limit, false);
		if (!iova) {
			M4U_ERR("%s pseudo alloc_iova failed dm: 0x%lx-0x%lx\n",
				dev_name(dev),
				(unsigned long)entry->start, entry->length);
			return -1;
		}
		M4U_MSG("reserve iova done,dm 0x%lx-0x%lx,iova 0x%lx-0x%lx\n",
			(unsigned long)entry->start, entry->length,
			iova->pfn_lo << shift, iova->pfn_hi << shift);
		M4U_MSG("ipfn 0x%lx, node %p, granule 0x%lx,start_pfn 0x%lx\n",
			iovad->dma_32bit_pfn, iovad->cached32_node,
			iovad->granule, iovad->start_pfn);
		if (iova->pfn_hi > temp_en)
			temp_en = iova->pfn_hi;
		if (iova->pfn_lo < temp_st)
			temp_st = iova->pfn_lo;

	}

	return 0;
}

static const struct file_operations pseudo_fops = {
	.owner = THIS_MODULE,
	.open = pseudo_open,
	.release = pseudo_release,
	.flush = pseudo_flush,
	.unlocked_ioctl = pseudo_ioctl,
	.compat_ioctl = pseudo_compat_ioctl,
};


static int pseudo_probe(struct platform_device *pdev)
{
	int i, j;
	struct device_node *node = NULL;

	M4U_MSG("pseudo_probe +0\n");
	for (i = 0; i < 3; i++) {
		/* wait for larb probe done. */
		/* if (mtk_smi_larb_ready(i) == 0) {
		 *	M4U_MSG("pseudo_probe - smi not ready\n");
		 *	return -EPROBE_DEFER;
		 * }
		 */
		/* wait for pseudo larb probe done. */
		if (!larbdev[i].dev) {
			M4U_MSG("pseudo_probe - dev not ready\n");
			return -EPROBE_DEFER;
		}
	}

#ifdef M4U_TEE_SERVICE_ENABLE
	{
		/* init the sec_mem_size to 400M to avoid build error. */
		unsigned int sec_mem_size = 400 * 0x100000;
		/*reserve mva range for sec */
		struct device *dev = &pdev->dev;

		pseudo_session_init();

		pseudo_sec_init(0, M4U_L2_ENABLE, &sec_mem_size);
		/* reserve mva range for security world */
		__reserve_iova_sec(dev, 0, sec_mem_size);
	}
#endif
	pseudo_reserve_dm();
	pseudo_mmu_dev = kzalloc(sizeof(struct m4u_device), GFP_KERNEL);
	if (!pseudo_mmu_dev) {
		M4U_MSG("kmalloc for m4u_device fail\n");
		return -ENOMEM;
	}
	pseudo_mmu_dev->m4u_dev_proc_entry = proc_create("m4u", 0000, NULL,
							 &pseudo_fops);
	if (!pseudo_mmu_dev->m4u_dev_proc_entry) {
		M4U_ERR("proc m4u create error\n");
		return -ENODEV;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,iommu_v0");
	if (node == NULL)
		M4U_ERR("init iommu_v0 error\n");

	pseudo_mmubase[0] = (unsigned long)of_iomap(node, 0);

	for (i = 0; i < SMI_LARB_NR; i++) {
		node = of_find_compatible_node(NULL, NULL, pseudo_larbname[i]);
		if (node == NULL)
			M4U_ERR("init larb %d error\n", i);

		pseudo_larbbase[i] = (unsigned long)of_iomap(node, 0);
		/* set mm engine domain to 0x4 (default value) */
		larb_clock_on(i, 1);
#ifndef CONFIG_FPGA_EARLY_PORTING
		M4U_MSG("m4u write all port domain to 4\n");
		for (j = 0; j < 32; j++)
			pseudo_set_reg_by_mask(pseudo_larbbase[i],
						       SMI_LARB_SEC_CONx(j),
						       F_SMI_DOMN(0x7),
						       F_SMI_DOMN(0x4));
#else
		j = 0;
#endif
		larb_clock_off(i, 1);

		M4U_MSG("init larb %d, 0x%lx\n", i, pseudo_larbbase[i]);
	}
	M4U_MSG("pseudo_probe -\n");
	return 0;
}

static int pseudo_port_probe(struct platform_device *pdev)
{
	int larbid;
	int ret;
	struct device *dev;
	struct device_dma_parameters *dma_param;
	struct device_node *np;

	M4U_MSG("pseudo_port_probe +\n");
	/* dma will split the iova into max size to 65535 byte by default */
	/* if we do not set this.*/
	dma_param = kzalloc(sizeof(*dma_param), GFP_KERNEL);
	if (!dma_param)
		return -ENOMEM;

	/* set the iova to 256MB for one time map, should be suffice for ION */
	dma_param->max_segment_size = 0x10000000;
	dev = &pdev->dev;
	dev->dma_parms = dma_param;
	ret = of_property_read_u32(dev->of_node, "mediatek,larbid", &larbid);
	if (ret)
		return ret;

	larbdev[larbid].larbid = larbid;
	larbdev[larbid].dev = &pdev->dev;

	if (larbdev[larbid].mmuen)
		return 0;

	dev = larbdev[larbid].dev;

	np = of_parse_phandle(dev->of_node, "iommus", 0);
	if (!np)
		return 0;

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (WARN_ON(!pdev))
		return -EINVAL;
	M4U_MSG("pseudo_port_probe -\n");
	return 0;
}

static int pseudo_remove(struct platform_device *pdev)
{
	return 0;
}

static int pseudo_port_remove(struct platform_device *pdev)
{
	return 0;
}

static int pseudo_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int pseudo_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver pseudo_driver = {
	.probe = pseudo_probe,
	.remove = pseudo_remove,
	.suspend = pseudo_suspend,
	.resume = pseudo_resume,
	.driver = {
		.name = M4U_DEVNAME,
		.of_match_table = mtk_pseudo_of_ids,
		.owner = THIS_MODULE,
	}
};

static struct platform_driver pseudo_port_driver = {
	.probe = pseudo_port_probe,
	.remove = pseudo_port_remove,
	.suspend = pseudo_suspend,
	.resume = pseudo_resume,
	.driver = {
		.name = "pseudo_port_device",
		.of_match_table = mtk_pseudo_port_of_ids,
		.owner = THIS_MODULE,
	}
};


static int __init mtk_pseudo_init(void)
{
	M4U_MSG("mtk_pseudo_init +\n");

	if (platform_driver_register(&pseudo_port_driver)) {
		M4U_MSG("failed to register pseudo port driver");
		platform_driver_unregister(&pseudo_driver);
		return -ENODEV;
	}
	M4U_MSG("mtk_pseudo_init -\n");

	if (platform_driver_register(&pseudo_driver)) {
		M4U_MSG("failed to register pseudo driver");
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_pseudo_exit(void)
{
	M4U_MSG("mtk_pseudo_exit +\n");
	platform_driver_unregister(&pseudo_driver);
	platform_driver_unregister(&pseudo_port_driver);
}

module_init(mtk_pseudo_init);
module_exit(mtk_pseudo_exit);

MODULE_DESCRIPTION("MTK pseudo m4u driver based on iommu");
MODULE_AUTHOR("Honghui Zhang <honghui.zhang@mediatek.com>");
MODULE_LICENSE("GPL");
