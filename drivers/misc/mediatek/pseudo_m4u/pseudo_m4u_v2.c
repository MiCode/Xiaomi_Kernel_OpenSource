/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include <linux/fb.h>
#include <mt-plat/aee.h>
#include <mach/pseudo_m4u.h>
#include <mach/pseudo_m4u_plat.h>
#include <linux/pagemap.h>
#include <linux/compat.h>
#include <linux/sched/signal.h>
#include <linux/sched/clock.h>
#include <asm/dma-iommu.h>
#include <sync_write.h>
#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif
#ifdef CONFIG_MTK_IOMMU_V2
#include "mach/mt_iommu.h"
#endif
#ifdef PSEUDO_M4U_TEE_SERVICE_ENABLE
#include "pseudo_m4u_sec.h"
#endif
#include "pseudo_m4u_log.h"
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && \
	!defined(CONFIG_MTK_TEE_GP_SUPPORT)
#include "mobicore_driver_api.h"
static const struct mc_uuid_t m4u_drv_uuid = M4U_DRV_UUID;
static struct mc_session_handle m4u_dci_session;
static struct m4u_msg *m4u_dci_msg;
#endif

#define IOMMU_DEBUG_ENABLED
static struct m4u_client_t *ion_m4u_client;
int m4u_log_level = 2;
int m4u_log_to_uart = 2;

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
#define MVA_REGION_REGISTER	0x2

static unsigned long pseudo_mmubase[TOTAL_M4U_NUM];
static unsigned long pseudo_larbbase[SMI_LARB_NR];
struct m4u_device *pseudo_mmu_dev;

static inline unsigned int pseudo_readreg32(
						unsigned long base,
						unsigned int offset)
{
	unsigned int val;

	val = ioread32((void *)(base + offset));
	return val;
}

static inline void pseudo_writereg32(unsigned long base,
					 unsigned int offset,
					 unsigned int val)
{
	mt_reg_sync_writel(val, (void *)(base + offset));
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

static inline int m4u_kernel2user_port(int kernelport)
{
	return kernelport;
}

static inline int m4u_get_larbid(int kernel_port)
{
	return MTK_IOMMU_TO_LARB(kernel_port);
}

static int m4u_port_2_larb_port(int kernel_port)
{
	return MTK_IOMMU_TO_PORT(kernel_port);
}

static char *m4u_get_module_name(int portID)
{
	return iommu_get_port_name(portID);
}

static int get_pseudo_larb(unsigned int port)
{
	int i, j, fake_nr;

	fake_nr = ARRAY_SIZE(pseudo_dev_larb_fake);
	for (i = 0; i < fake_nr; i++) {
		for (j = 0; j < 32; j++) {
			if (pseudo_dev_larb_fake[i].port[j] == -1)
				break;
			if (pseudo_dev_larb_fake[i].port[j] == port) {
				return i;
			}
		}
	}
	return -1;
}

struct device *pseudo_get_larbdev(int portid)
{
	struct pseudo_device *pseudo = NULL;
	unsigned int larbid, larbport, fake_nr;
	int index = -1;

	fake_nr = ARRAY_SIZE(pseudo_dev_larb_fake);
	larbid = m4u_get_larbid(portid);
	larbport = m4u_port_2_larb_port(portid);

	if (larbid >= (SMI_LARB_NR + fake_nr) ||
	    larbport >= 32)
		goto out;

	if (larbid >= 0 &&
	    larbid < SMI_LARB_NR) {
		pseudo = &pseudo_dev_larb[larbid];
	} else if (larbid < (SMI_LARB_NR + fake_nr)) {
		index = get_pseudo_larb(portid);
		if (index >= 0 &&
		    index < fake_nr)
			pseudo = &pseudo_dev_larb_fake[index];
	}

out:
	if (!pseudo) {
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT == 32)
		/*
		 * for 34bit IOVA space, boundary is mandatory
		 * we cannot use a default device for iova mapping
		 */
#ifndef CONFIG_FPGA_EARLY_PORTING
		index = get_pseudo_larb(M4U_PORT_OVL_DEBUG);
		if (index >= 0 &&
		    index < fake_nr)
			pseudo = &pseudo_dev_larb_fake[index];
#else
		pseudo = &pseudo_dev_larb_fake[2];
#endif
#endif
	}

	if (pseudo && pseudo->dev)
		return pseudo->dev;

	M4U_ERR("err, p:%d(%d-%d) index=%d fake_nr=%d smi_nr=%d\n",
		portid, larbid, larbport, index, fake_nr, SMI_LARB_NR);
	return NULL;
}


int larb_clock_on(int larb, bool config_mtcmos)
{
	int ret = 0;

#ifdef CONFIG_MTK_SMI_EXT
	if (larb < ARRAY_SIZE(pseudo_larb_clk_name))
		ret = smi_bus_prepare_enable(larb,
						 pseudo_larb_clk_name[larb]);
	if (ret) {
		M4U_ERR("err larb %d\n", larb);
		ret = -1;
	}
#endif

	return ret;
}

void larb_clock_off(int larb, bool config_mtcmos)
{
#ifdef CONFIG_MTK_SMI_EXT
	int ret = 0;

	if (larb < ARRAY_SIZE(pseudo_larb_clk_name))
		ret = smi_bus_disable_unprepare(larb,
						pseudo_larb_clk_name[larb]);
	if (ret)
		M4U_MSG("err: larb %d\n", larb);
#endif
}

#ifdef M4U_MTEE_SERVICE_ENABLE

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
#if 0
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
#endif
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
#if 0
/* the caller should enable smi clock, it should be only called by mtk_smi.c*/
int pseudo_config_port_tee(int kernelport)
{
	struct M4U_PORT_STRUCT pM4uPort;

	pM4uPort.ePortID = m4u_kernel2user_port(kernelport);
	pM4uPort.Virtuality = 1;
	pM4uPort.Distance = 1;
	pM4uPort.Direction = 1;

#ifdef M4U_MTEE_SERVICE_ENABLE
	return pseudo_do_config_port(&pM4uPort);
#else
	return 0;
#endif
}
#endif
#endif


/* make sure the va size is page aligned to get the continues iova. */
int m4u_va_align(unsigned long *addr, unsigned long *size)
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

int pseudo_get_reg_of_path(unsigned int port, bool is_va,
			unsigned int *reg, unsigned int *mask,
			unsigned int *value)
{
	unsigned long larb_base;
	unsigned int larb, larb_port;

	larb = m4u_get_larbid(port);
	larb_port = m4u_port_2_larb_port(port);
	larb_base = pseudo_larbbase[larb];
	if (!larb_base) {
		M4U_MSG("larb not existed, no need of config\n", larb);
		return -1;
	}

	*reg = larb_base + SMI_LARB_NON_SEC_CONx(larb_port);
	*mask = F_SMI_MMU_EN;
	if (is_va)
		*value = 1;
	else
		*value = 0;

	return 0;
}

int m4u_get_boundary(int port)
{
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	struct device *dev = pseudo_get_larbdev(port);

	if (!dev)
		return -1;

	return mtk_iommu_get_boundary_id(dev);
#else
	return 0;
#endif
}

static inline int pseudo_config_port(struct M4U_PORT_STRUCT *pM4uPort,
	bool is_user)
{
	/* all the port will be attached by dma and configed by iommu driver */
	unsigned long larb_base;
	unsigned int larb, larb_port, bit32 = 0;
	unsigned int old_value = 0, value;
	int ret = 0;
	char *name;

	larb = m4u_get_larbid(pM4uPort->ePortID);
	larb_port = m4u_port_2_larb_port(pM4uPort->ePortID);
	name = iommu_get_port_name(pM4uPort->ePortID);
	if (is_user && strcmp(name, pM4uPort->name)) {
		M4U_MSG("port name(%s) not matched(%s)\n",
			pM4uPort->name, name);
		aee_kernel_warning_api(__FILE__, __LINE__,
				       DB_OPT_DEFAULT |
				       DB_OPT_NATIVE_BACKTRACE,
				       "port name not matched",
				       "dump user backtrace");
		return -1;
	}

	if (pM4uPort->Virtuality) {
		bit32 = m4u_get_boundary(pM4uPort->ePortID);
		if (bit32 < 0 ||
		    bit32 >= (1 <<
		    (CONFIG_MTK_IOMMU_PGTABLE_EXT - 32))) {
			M4U_MSG("enable larb%d fail\n", larb);
			return -2;
		}
	}

	larb_base = pseudo_larbbase[larb];
	if (!larb_base) {
		M4U_MSG("larb %d not existed, no need of config\n", larb);
		return -3;
	}

	ret = larb_clock_on(larb, 1);
	if (ret < 0) {
		M4U_MSG("enable larb%d fail\n", larb);
		return ret;
	}

	old_value = pseudo_readreg32(larb_base,
					  SMI_LARB_NON_SEC_CONx(larb_port));
	if (pM4uPort->Virtuality) {
		value = (old_value & ~F_SMI_ADDR_BIT32) |
			(bit32 << 8) | (bit32 << 10) |
			(bit32 << 12) | (bit32 << 14) |
			F_SMI_MMU_EN;
	} else {
		value = old_value & ~F_SMI_ADDR_BIT32 &
			~F_SMI_MMU_EN;
	}

	if (value == old_value)
		goto out;

	pseudo_writereg32(larb_base,
			   SMI_LARB_NON_SEC_CONx(larb_port),
			   value);

	/* debug use */
	if (value != pseudo_readreg32(larb_base,
					  SMI_LARB_NON_SEC_CONx(larb_port))) {
		M4U_ERR("%d(%d-%d),vir=%d, bound=%d, old=0x%x, expect=0x%x\n",
			pM4uPort->ePortID, larb, larb_port,
			pM4uPort->Virtuality,
			bit32, old_value, value);
		ret = -4;
	}

	M4U_MSG("%s, l%d-p%d, switch fr %d to %d, bd:%d\n",
		m4u_get_module_name(pM4uPort->ePortID),
		larb, larb_port, old_value, value, bit32);

out:
	larb_clock_off(larb, 1);

	return ret;
}

int pseudo_dump_port(int port)
{
	/* all the port will be attached by dma and configed by iommu driver */
	unsigned long larb_base;
	unsigned int larb, larb_port;
	unsigned int regval = 0;
	int ret = 0;

	larb = m4u_get_larbid(port);
	larb_port = m4u_port_2_larb_port(port);
	if (larb >= SMI_LARB_NR || larb_port >= 32) {
		M4U_MSG("port:%d, larb:%d is fake, or port:%d invalid\n",
			port, larb, larb_port);
		return 0;
	}

	larb_base = pseudo_larbbase[larb];
	if (!larb_base) {
		M4U_MSG("larb:%d not existed, no need of config\n", larb);
		return 0;
	}

	ret = larb_clock_on(larb, 1);
	if (ret < 0) {
		M4U_MSG("enable larb%d fail\n", larb);
		return ret;
	}

	regval = pseudo_readreg32(larb_base,
					  SMI_LARB_NON_SEC_CONx(larb_port));
	M4U_MSG(
		"port %d(%d-%d):	%s	-- config:0x%x,	mmu:0x%x,	bit32:0x%x\n",
		port, larb, larb_port,
		iommu_get_port_name(MTK_M4U_ID(larb, larb_port)),
		regval, regval & F_SMI_MMU_EN,
		F_SMI_ADDR_BIT32_VAL(regval));

	larb_clock_off(larb, 1);

	return ret;
}

int pseudo_dump_all_port_status(struct seq_file *s)
{
	/* all the port will be attached by dma and configed by iommu driver */
	unsigned long larb_base;
	unsigned int larb, larb_port, count;
	int regval = 0;
	int ret = 0;

	for (larb = 0; larb < SMI_LARB_NR; larb++) {
		larb_base = pseudo_larbbase[larb];
		if (!larb_base) {
			M4U_MSG("larb not existed, no need of config\n", larb);
			continue;
		}

		ret = larb_clock_on(larb, 1);
		if (ret < 0) {
			M4U_ERR("err enable larb%d\n", larb);
			continue;
		}
		count = mtk_iommu_get_larb_port_count(larb);
		M4U_PRINT_SEQ(s,
				"====== larb:%d, total %d ports ======\n",
				larb, count);
		for (larb_port = 0; larb_port < count; larb_port++) {

			regval = pseudo_readreg32(larb_base,
					  SMI_LARB_NON_SEC_CONx(larb_port));
			M4U_PRINT_SEQ(s,
					"port%d:	%s	-- config:0x%x,	mmu:0x%x,	bit32:0x%x\n",
					larb_port,
					iommu_get_port_name(
						MTK_M4U_ID(larb, larb_port)),
					regval, regval & F_SMI_MMU_EN,
					F_SMI_ADDR_BIT32_VAL(regval));
		}
		larb_clock_off(larb, 1);
	}
	return ret;
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
		M4U_MSG("%s va 0x%lx, err current\n",
			   __func__, va);
		return 0;
	}
	if (!current->mm) {
		M4U_MSG("warning: tgid=0x%x, name=%s, va 0x%lx\n",
			   current->tgid, current->comm, va);
		return 0;
	}

	pgd = pgd_offset(current->mm, va);  /* what is tsk->mm */
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		M4U_DBG("%s va=0x%lx, err pgd\n", __func__, va);
		return 0;
	}

	pud = pud_offset(pgd, va);
	if (pud_none(*pud) || pud_bad(*pud)) {
		M4U_DBG("%s va=0x%lx, err pud\n", __func__, va);
		return 0;
	}

	pmd = pmd_offset(pud, va);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		M4U_MSG("%s va=0x%lx, err pmd\n", __func__, va);
		return 0;
	}

	pte = pte_offset_map(pmd, va);
	if (pte_present(*pte)) {
		pa = (pte_val(*pte) & (PHYS_MASK) & (PAGE_MASK)) | pageOffset;
		pte_unmap(pte);
		return pa;
	}

	pte_unmap(pte);

	M4U_DBG("%s, va=0x%lx err pte\n", __func__, va);
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
		M4U_ERR("error: !!pages != !!(gup_flags & FOLL_GET),");
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
			M4U_ERR("error: vma not find, start=0x%x, module=%d\n",
				   (unsigned int)start, eModuleID);
			return i ? i : -EFAULT;
		}
		if (((~vma->vm_flags) &
			 (VM_IO | VM_PFNMAP | VM_SHARED | VM_WRITE)) == 0) {
			M4U_ERR("error: m4u_get_pages: bypass garbage pages!");
			M4U_MSG("vma->vm_flags=0x%x, start=0x%lx, module=%d\n",
				(unsigned int)(vma->vm_flags),
				start, eModuleID);
			return i ? i : -EFAULT;
		}
		if (vma->vm_flags & VM_IO)
			M4U_DBG("warning: vma is marked as VM_IO\n");

		if (vma->vm_flags & VM_PFNMAP) {
			M4U_MSG
				("err vma permission,0x%x,0x%lx,%d\n",
				 (unsigned int)(vma->vm_flags),
				 start, eModuleID);
			M4U_MSG
				("maybe remap of un-permit vm_flags!\n");
			/* m4u_dump_maps(start); */
			return i ? i : -EFAULT;
		}
		if (!(vm_flags & vma->vm_flags)) {
			M4U_MSG
				("%s err flag, 0x%x,0x%x,0x%lx,%d\n",
				 __func__,
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
							(foll_flags &
							 FOLL_WRITE) ?
							 FAULT_FLAG_WRITE : 0);

				if (ret & VM_FAULT_ERROR) {
					if (ret & VM_FAULT_OOM) {
						M4U_ERR("error: no memory,");
						M4U_MSG("addr:0x%lx (%d %d)\n",
							start, i, eModuleID);
						/* m4u_dump_maps(start); */
						return i ? i : -ENOMEM;
					}
					if (ret &
						(VM_FAULT_HWPOISON |
						 VM_FAULT_SIGBUS)) {
						M4U_ERR("error: invalid va,");
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
							 (vma->vm_flags &
							  VM_WRITE),
							 &page, NULL);
				if (ret == 1)
					pages[i] = page;
			}
			if (IS_ERR(page)) {
				M4U_ERR("error: faulty page is returned,");
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

	M4U_MSG("%s: module=%s,BufAddr=0x%lx,BufSize=%ld,0x%lx\n",
		   __func__,
		   m4u_get_module_name(eModuleID),
		   BufAddr, BufSize, PAGE_OFFSET);

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
			M4U_ERR("err v2p\n");
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
				M4U_MSG("%s:n=%d,%s,v=0x%lx,s=0x%lx,f=0x%x\n",
					 __func__, page_num,
					 m4u_get_module_name(eModuleID),
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
					M4U_ERR("error: receive sigkill when");
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
					M4U_ERR("error:page_num=%d, return=%d",
						page_num, ret);
					M4U_MSG("module=%s, current_proc:%s\n",
						m4u_get_module_name(eModuleID),
						current->comm);
					M4U_MSG("maybe the allocated VA size");
					M4U_MSG("is smaller than the size");
					M4U_MSG("config to m4u_alloc_mva()!");
				} else {
					M4U_ERR("error: page_num=%d,return=%d",
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
					vmalloc_to_page((unsigned int *)
							(BufAddr +
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
				iommu_get_port_name(portid));
			goto err_free;
		}

		if (have_page) {
			page = phys_to_page(pPhys[i]);
			sg_set_page(sg, page, M4U_PAGE_SIZE, 0);
			sg_dma_len(sg) = M4U_PAGE_SIZE;
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

struct sg_table *pseudo_find_sgtable(unsigned long mva)
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

	//M4U_DBG("adding pseudo_sglist, mva = 0x%x\n", mva_sg->mva);
	return table;
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
 * @param   mva	  -- mva to be searched
 * @param   del	  -- should we del this buffer from client?
 *
 * @return buffer_info if found, NULL on fail
 * @remark
 * @see
 * @to-do	we need to add multi domain support here.
 * @author K Zhang	  @date 2013/11/14
 */
static struct m4u_buf_info_t *pseudo_client_find_buf(
						  struct m4u_client_t *client,
						  unsigned long mva,
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

static bool pseudo_is_acp_port(unsigned int port)
{
	unsigned int count = ARRAY_SIZE(pseudo_acp_port_array);
	unsigned int i;

	for (i = 0; i < count; i++) {
		if (pseudo_acp_port_array[i] == port)
			return true;
	}
	return false;
}

int m4u_switch_acp(unsigned int port,
		unsigned long iova, size_t size, bool is_acp)
{
	struct device *dev = pseudo_get_larbdev(port);
	struct m4u_buf_info_t *pMvaInfo;

	pMvaInfo = pseudo_client_find_buf(ion_m4u_client, iova, 0);
	if (!pseudo_is_acp_port(port) ||
	    port != pMvaInfo->port ||
	    size > pMvaInfo->size) {
		M4U_MSG("invalid p:%d, va:0x%lx, sz:0x%lx, ow:%d, sz:0x%lx\n",
			port, iova, size, pMvaInfo->port, pMvaInfo->size);
		return -EINVAL;
	}

	M4U_MSG("%s %d, switch acp, iova=0x%lx, size=0x%lx, acp=%d\n",
		__func__, __LINE__, iova, size, is_acp);
	return mtk_iommu_switch_acp(dev, iova, size, is_acp);
}
EXPORT_SYMBOL(m4u_switch_acp);

int __pseudo_alloc_mva(struct m4u_client_t *client,
			   int port, unsigned long va, unsigned long size,
			   struct sg_table *sg_table, unsigned int flags,
			   unsigned long *retmva)
{
	struct mva_sglist *mva_sg;
	struct sg_table *table = NULL;
	int ret;
	struct device *dev = pseudo_get_larbdev(port);
	dma_addr_t dma_addr = ARM_MAPPING_ERROR;
	unsigned int i;
	struct scatterlist *s;
	dma_addr_t orig_addr = ARM_MAPPING_ERROR;
	dma_addr_t offset = 0;
	struct m4u_buf_info_t *pbuf_info;
	unsigned long long current_ts = 0;
	struct task_struct *task;
	bool free_table = true;

	if (!dev) {
		M4U_MSG("dev NULL!\n");
		return -1;
	}

	if (va && sg_table) {
		M4U_MSG("va/sg 0x%x are valid:0x%lx, 0x%p, 0x%x, 0x%x-0x%x\n",
			   port, va, sg_table, flags, *retmva, size);
	} else if (!va && !sg_table) {
		M4U_ERR("err va, err sg\n");
		return -EINVAL;
	}

	/* this is for ion mm heap and ion fb heap usage. */
	if (sg_table) {
		s = sg_table->sgl;
		if ((flags & M4U_FLAGS_SG_READY) == 0) {
			struct scatterlist *ng;
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
				sg_dma_address(ng) = phys;
				sg_dma_len(ng) = s->length;
				s = sg_next(s);
				ng = sg_next(ng);
			}
			if (!size) {
				M4U_ERR("err sg, please set page\n");
				goto ERR_EXIT;
			}
		} else {
			table = sg_table;
			free_table = false;
		}
	}

	if (!table && va && size)
		table = pseudo_get_sg(port, va, size);

	if (!table) {
		M4U_ERR("err sg of va:0x%lx, size:0x%lx\n", va, size);
		goto ERR_EXIT;
	}

#if defined(CONFIG_MACH_MT6785)
	/*just a workaround, since m4u design didn't define VPU_DATA*/
	if (!(flags & (M4U_FLAGS_FIX_MVA | M4U_FLAGS_START_FROM))) {
		M4U_DBG("%s,%d, vpu data, flags=0x%x\n",
			__func__, __LINE__, flags);
		if (port == M4U_PORT_VPU)
			port = M4U_PORT_VPU_DATA;
		dev = pseudo_get_larbdev(port);
	} else {
		M4U_DBG("%s,%d, vpu code, flags=0x%x\n",
			__func__, __LINE__, flags);
	}
#endif
	dma_map_sg_attrs(dev, table->sgl,
			sg_table ? table->nents : table->orig_nents,
			DMA_BIDIRECTIONAL,
			DMA_ATTR_SKIP_CPU_SYNC);
	dma_addr = sg_dma_address(table->sgl);
	current_ts = sched_clock();

	if (!dma_addr || dma_addr == ARM_MAPPING_ERROR) {
		M4U_ERR("err map, %s, iova:0x%lx+0x%x, f:0x%x, n:%d-%d\n",
			iommu_get_port_name(port),
			(unsigned long)dma_addr, size,
			flags, table->nents, table->orig_nents);
		goto ERR_EXIT;
	}
	if (sg_table) {
		orig_addr = sg_dma_address(sg_table->sgl);
		if (orig_addr != dma_addr) {
			for_each_sg(sg_table->sgl, s, sg_table->nents, i) {
				sg_dma_address(s) = dma_addr + offset;
				offset += s->length;
			}
		}
	}
	*retmva = dma_addr;

	mva_sg = kzalloc(sizeof(*mva_sg), GFP_KERNEL);
	mva_sg->table = table;
	mva_sg->mva = *retmva;

	pseudo_add_sgtable(mva_sg);

#ifdef IOMMU_DEBUG_ENABLED
	M4U_MSG("%s, p:%d(%d-%d) pa=0x%lx iova=0x%lx s=0x%lx n=%d",
		iommu_get_port_name(port),
		port, MTK_IOMMU_TO_LARB(port),
		MTK_IOMMU_TO_PORT(port),
		sg_phys(table->sgl),
		*retmva, size, table->nents);
#endif

	/* pbuf_info for userspace compatible */
	pbuf_info = pseudo_alloc_buf_info();
	pbuf_info->va = va;
	pbuf_info->port = port;
	pbuf_info->size = size;
	pbuf_info->flags = flags;
	pbuf_info->sg_table = sg_table;
	pbuf_info->mva = *retmva;
	pbuf_info->mva_align = *retmva;
	pbuf_info->size_align = size;

	do_div(current_ts, 1000000);
	pbuf_info->timestamp = current_ts;

	task = current->group_leader;
	get_task_comm(pbuf_info->task_comm, task);
	pbuf_info->pid = task_pid_nr(task);

	pseudo_client_add_buf(client, pbuf_info);
	mtk_iommu_trace_log(IOMMU_ALLOC, dma_addr, size, flags | (port << 16));
	return 0;

ERR_EXIT:
	if (table && free_table) {
		sg_free_table(table);
		kfree(table);
	}

	*retmva = 0;
	return -EINVAL;
}

/* interface for ion */
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

struct m4u_client_t *pseudo_get_m4u_client(void)
{
	if (!ion_m4u_client) {
		ion_m4u_client = pseudo_create_client();
		if (IS_ERR_OR_NULL(ion_m4u_client)) {
			M4U_ERR("err client\n");
			ion_m4u_client = NULL;
			return NULL;
		}
	}

	return ion_m4u_client;
}
EXPORT_SYMBOL(pseudo_get_m4u_client);

int pseudo_alloc_mva_sg(struct port_mva_info_t *port_info,
				struct sg_table *sg_table)
{
	unsigned int flags = 0;
	int ret, offset;
	unsigned long mva = 0;
	unsigned long va_align;
	unsigned long *pMva;
	unsigned long mva_align;
	unsigned long size_align = port_info->buf_size;
	struct m4u_client_t *client;

	client = pseudo_get_m4u_client();
	if (!client) {
		M4U_ERR("failed to get ion_m4u_client\n");
		return -1;
	}

	if (port_info->flags & M4U_FLAGS_FIX_MVA)
		flags = M4U_FLAGS_FIX_MVA;

	if (port_info->flags & M4U_FLAGS_SG_READY)
		flags |= M4U_FLAGS_SG_READY;
	else
		port_info->va = 0;

	va_align = port_info->va;
	pMva = &port_info->mva;
	mva_align = *pMva;
	/* align the va to allocate continues iova. */
	offset = m4u_va_align(&va_align, &size_align);

	ret = __pseudo_alloc_mva(client, port_info->emoduleid,
				  va_align, size_align,
				  sg_table, flags, &mva_align);
	if (ret) {
		M4U_ERR("error alloc mva: port %d, 0x%x, 0x%lx, 0x%x, 0x%x\n",
			port_info->emoduleid, flags, port_info->va,
			mva, port_info->buf_size);
		mva = 0;
		return ret;
	}

	mva = mva_align + offset;
	*pMva = mva;

#if 0
	M4U_MSG("%s:port(%d), flags(%d), va(0x%lx), mva=0x%x, size 0x%x\n",
		__func__, port_info->emoduleid, flags,
		port_info->va, mva, port_info->buf_size);
#endif

	return 0;
}

struct sg_table *pseudo_del_sgtable(unsigned long mva)
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
int __pseudo_dealloc_mva(struct m4u_client_t *client,
			  int port,
			  unsigned long BufAddr,
			  const unsigned long size,
			  const unsigned long mva,
			  struct sg_table *sg_table)
{
	struct sg_table *table = NULL;
	struct device *dev = pseudo_get_larbdev(port);

	unsigned long addr_align = mva;
	unsigned long size_align = size;
	int offset;

	if (!dev) {
		M4U_MSG("dev is NULL\n");
		return -EINVAL;
	}

	M4U_MSG("larb%d, port%d, addr=0x%lx, size=0x%lx, iova=0x%x\n",
		MTK_IOMMU_TO_LARB(port),
		MTK_IOMMU_TO_PORT(port),
		BufAddr, size, mva);

	/* for ion sg alloc, we did not align the mva in allocation. */
	/* if (!sg_table) */
	offset = m4u_va_align(&addr_align, &size_align);

	if (sg_table) {
		struct m4u_buf_info_t *m4u_buf_info;

		m4u_buf_info = pseudo_client_find_buf(client,
							  mva, 1);
		table = pseudo_del_sgtable(addr_align);
		if (!table) {
#ifdef IOMMU_DEBUG_ENABLED
			M4U_ERR("err table of mva 0x%x-0x%lx\n",
				__func__, __LINE__, mva, addr_align);
			M4U_ERR("%s addr=0x%lx,size=0x%x\n",
				__func__, __LINE__, m4u_get_module_name(port),
				BufAddr, size);
#endif
			dump_stack();
			return -EINVAL;
		}

		if (sg_page(table->sgl) != sg_page(sg_table->sgl)) {
			M4U_ERR("error, sg\n");
			return -EINVAL;
		}

		if (m4u_buf_info) {
			BufAddr = m4u_buf_info->va;
			pseudo_free_buf_info(m4u_buf_info);
		}
	}

	if (!table)
		table = pseudo_del_sgtable(addr_align);

	mtk_iommu_trace_log(IOMMU_DEALLOC, mva, size, (port << 16));

	if (table) {
		dma_unmap_sg_attrs(dev, table->sgl,
				table->orig_nents,
				DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);
	} else {
		M4U_ERR("can't find the sgtable and would return error\n");
		return -EINVAL;
	}


	if (BufAddr) {
		/* from user space */
		if (BufAddr < PAGE_OFFSET) {
			if (!((BufAddr >= VMALLOC_START) &&
				(BufAddr <= VMALLOC_END))) {
				pseudo_put_sgtable_pages(
					table,
					table->nents);
			}
		}
	}
	if (table) {
		sg_free_table(table);
		kfree(table);
	}

	M4UTRACE();
	return 0;

}

int pseudo_dealloc_mva(struct m4u_client_t *client, int port, unsigned long mva)
{
	struct m4u_buf_info_t *pMvaInfo;
	int offset, ret;

	pMvaInfo = pseudo_client_find_buf(client, mva, 1);

	offset = m4u_va_align(&pMvaInfo->va, &pMvaInfo->size);
	pMvaInfo->mva -= offset;

	ret = __pseudo_dealloc_mva(client, port, pMvaInfo->va,
				   pMvaInfo->size, mva, NULL);

#ifdef IOMMU_DEBUG_ENABLED
	M4U_DBG("port %d, flags 0x%x, va 0x%lx, mva = 0x%x, size 0x%x\n",
		port, pMvaInfo->flags,
		pMvaInfo->va, mva, pMvaInfo->size);
#endif
	if (ret)
		return ret;

	pseudo_free_buf_info(pMvaInfo);
	return ret;

}

int pseudo_dealloc_mva_sg(int eModuleID,
			   struct sg_table *sg_table,
			   const unsigned int BufSize, const unsigned long mva)
{
	if (!sg_table) {
		M4U_MSG("sg_table is NULL\n");
		return -EINVAL;
	}

	return __pseudo_dealloc_mva(ion_m4u_client,
				eModuleID, 0,
				BufSize, mva, sg_table);
}

int pseudo_destroy_client(struct m4u_client_t *client)
{
	struct m4u_buf_info_t *pMvaInfo;
	unsigned long mva, size;
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
#ifdef IOMMU_DEBUG_ENABLED
		M4U_MSG
			("warn: clean garbage: %s,va=0x%lx,mva=0x%x,size=%d\n",
			 iommu_get_port_name(pMvaInfo->port), pMvaInfo->va,
			 pMvaInfo->mva,
			 pMvaInfo->size);
#endif
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
							(vma->vm_flags &
							 VM_WRITE) ?
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

#ifdef IOMMU_DEBUG_ENABLED
			M4U_MSG("%s: fail(0x%lx) va=0x%lx,page_num=0x%x\n",
				__func__, ret, va, page_num);
			M4U_MSG("%s: fail_va=0x%lx,pa=0x%lx,sg=0x%p,i=%d\n",
				__func__, va_tmp, (unsigned long)pa, sg, i);
#endif
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
			/* M4U_DBG("%s va: 0x%lx, vma->vm_start=0x%lx, */
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

struct sg_table *m4u_create_sgtable(unsigned long va, unsigned long size)
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

	if (va < PAGE_OFFSET) { /* from user space */
		if (va >= VMALLOC_START && va <= VMALLOC_END) { /* vmalloc */
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
				sg_dma_len(sg) = psize;
			}
		} else {
			ret = m4u_create_sgtable_user(va_align, table);
			if (ret) {
				M4U_ERR("error va=0x%lx, size=%d\n",
					va, size);
				goto err;
			}
		}
	} else {		/* from kernel sp */
		if (va >= VMALLOC_START && va <= VMALLOC_END) { /* vmalloc */
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
				sg_dma_len(sg) = psize;
			}
		} else {	/* kmalloc to-do: use one entry sgtable. */
			for_each_sg(table->sgl, sg, table->nents, i) {
				pa = virt_to_phys((void *)(va_align +
						 i * psize));
				page = phys_to_page(pa);
				sg_set_page(sg, page, psize, 0);
				sg_dma_len(sg) = psize;
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
		 const unsigned long BufSize, const unsigned long mva)
{
	return pseudo_dealloc_mva_sg(eModuleID, sg_table, BufSize, mva);
}

int m4u_alloc_mva_sg(struct port_mva_info_t *port_info,
		  struct sg_table *sg_table)
{
	return pseudo_alloc_mva_sg(port_info, sg_table);
}

static int pseudo_open(struct inode *inode, struct file *file)
{
	struct m4u_client_t *client;

	M4U_DBG("%s process : %s\n", __func__, current->comm);
	client = pseudo_get_m4u_client();
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

	ret = pseudo_config_port(pM4uPort, 0);

	return ret;
}

static int pseudo_release(struct inode *inode, struct file *file)
{
	struct m4u_client_t *client = file->private_data;

	M4U_DBG("%s process : %s\n", __func__, current->comm);
	pseudo_destroy_client(client);
	return 0;
}

static int pseudo_flush(struct file *a_pstFile, fl_owner_t a_id)
{
	return 0;
}

/***********************************************************/
/** map mva buffer to kernel va buffer
 *   this function should ONLY used for DEBUG
 ************************************************************/
int m4u_mva_map_kernel(unsigned long mva,
	unsigned long size, unsigned long *map_va,
	unsigned long *map_size)
{
	struct m4u_buf_info_t *pMvaInfo;
	struct sg_table *table;
	struct scatterlist *sg;
	int i, j, k, ret = 0;
	struct page **pages;
	unsigned int page_num;
	void *kernel_va;
	unsigned int kernel_size;

	pMvaInfo = pseudo_client_find_buf(ion_m4u_client, mva, 0);

	if (!pMvaInfo || pMvaInfo->size < size) {
		M4U_MSG(
			"%s cannot find mva: mva=0x%x, size=0x%x\n",
			__func__, mva, size);
		if (pMvaInfo)
			M4U_MSG(
			"pMvaInfo: mva=0x%x, size=0x%x\n",
			pMvaInfo->mva, pMvaInfo->size);
		return -1;
	}

	table = pMvaInfo->sg_table;

	page_num = M4U_GET_PAGE_NUM(mva, size);
	pages = vmalloc(sizeof(struct page *) * page_num);
	if (pages == NULL) {
		M4U_MSG("mva_map_kernel:error to vmalloc for %d\n",
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
		/* this should not happen, because we have
		 * checked the size before.
		 */
		M4U_MSG(
			"mva_map_kernel:only get %d pages: mva=0x%x, size=0x%x, pg_num=%d\n",
				k, mva, size, page_num);
		ret = -1;
		goto error_out;
	}

	kernel_va = 0;
	kernel_size = 0;
	kernel_va = vmap(pages, page_num, VM_MAP, PAGE_KERNEL);
	if (kernel_va == 0 || (unsigned long)kernel_va & M4U_PAGE_MASK) {
		M4U_MSG(
			"mva_map_kernel:vmap fail: page_num=%d, kernel_va=0x%p\n",
				page_num, kernel_va);
		ret = -2;
		goto error_out;
	}

	kernel_va += ((unsigned long)mva & (M4U_PAGE_MASK));

	*map_va = (unsigned long)kernel_va;
	*map_size = size;

error_out:
	vfree(pages);
	M4U_DBG(
		"mva_map_kernel:mva=0x%x,size=0x%x,map_va=0x%lx,map_size=0x%x\n",
		   mva, size, *map_va, *map_size);

	return ret;
}
EXPORT_SYMBOL(m4u_mva_map_kernel);

int m4u_mva_unmap_kernel(unsigned long mva,
		unsigned long size, unsigned long map_va)
{
	M4U_DBG(
		"mva_unmap_kernel:mva=0x%x,size=0x%x,va=0x%lx\n",
			mva, size, map_va);
	vunmap((void *)(map_va & (~M4U_PAGE_MASK)));
	return 0;
}
EXPORT_SYMBOL(m4u_mva_unmap_kernel);

#ifdef PSEUDO_M4U_TEE_SERVICE_ENABLE
static DEFINE_MUTEX(gM4u_sec_init);
bool m4u_tee_en;

static int __m4u_sec_init(void)
{
	int ret, i;
	unsigned long pt_pa_nonsec;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4UTL_INIT);
	if (!ctx)
		return -EFAULT;

	for (i = 0; i < SMI_LARB_NR; i++)
		larb_clock_on(i, 1);

	if (mtk_iommu_get_pgtable_base_addr((void *)&pt_pa_nonsec))
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4UTL_INIT;
	ctx->m4u_msg->init_param.nonsec_pt_pa = pt_pa_nonsec;
	ctx->m4u_msg->init_param.l2_en = M4U_L2_ENABLE;
	ctx->m4u_msg->init_param.sec_pt_pa = 0;
	/* m4u_alloc_sec_pt_for_debug(); */

	M4ULOG_HIGH("%s call CMD_M4UTL_INIT, nonsec_pt_pa: 0x%lx\n",
		__func__, pt_pa_nonsec);
	ret = m4u_exec_cmd(ctx);
	if (ret) {
		M4U_ERR("m4u exec command fail\n");
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
#ifdef __M4U_SECURE_SYSTRACE_ENABLE__
static int dr_map(unsigned long pa, size_t size)
{
	int ret;

	mutex_lock(&m4u_dci_mutex);
	if (!m4u_dci_msg) {
		M4U_ERR("error: m4u_dci_msg==null\n");
		ret = -1;
		goto out;
	}

	memset(m4u_dci_msg, 0, sizeof(struct m4u_msg));

	m4u_dci_msg->cmd = CMD_M4U_SYSTRACE_MAP;
	m4u_dci_msg->systrace_param.pa = pa;
	m4u_dci_msg->systrace_param.size = size;
	ret = m4u_exec_cmd(&m4u_dci_session, m4u_dci_msg);
	if (ret) {
		M4U_MSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = m4u_dci_msg->rsp;

out:
	mutex_unlock(&m4u_dci_mutex);
	return ret;
}

static int dr_unmap(unsigned long pa, size_t size)
{
	int ret;

	mutex_lock(&m4u_dci_mutex);
	if (!m4u_dci_msg) {
		M4U_ERR("error: m4u_dci_msg==null\n");
		ret = -1;
		goto out;
	}

	memset(m4u_dci_msg, 0, sizeof(struct m4u_msg));

	m4u_dci_msg->cmd = CMD_M4U_SYSTRACE_UNMAP;
	m4u_dci_msg->systrace_param.pa = pa;
	m4u_dci_msg->systrace_param.size = size;
	ret = m4u_exec_cmd(&m4u_dci_session, m4u_dci_msg);
	if (ret) {
		M4U_MSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = m4u_dci_msg->rsp;

out:
	mutex_unlock(&m4u_dci_mutex);
	return ret;
}

static int dr_transact(void)
{
	int ret;

	mutex_lock(&m4u_dci_mutex);
	if (!m4u_dci_msg) {
		M4U_ERR("error: m4u_dci_msg==null\n");
		ret = -1;
		goto out;
	}

	memset(m4u_dci_msg, 0, sizeof(struct m4u_msg));

	m4u_dci_msg->cmd = CMD_M4U_SYSTRACE_TRANSACT;
	m4u_dci_msg->systrace_param.pa = 0;
	m4u_dci_msg->systrace_param.size = 0;
	ret = m4u_exec_cmd(&m4u_dci_session, m4u_dci_msg);
	if (ret) {
		M4U_MSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = m4u_dci_msg->rsp;

out:
	mutex_unlock(&m4u_dci_mutex);
	return ret;
}
#endif

int m4u_sec_init(void)
{
	int ret;
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && \
	!defined(CONFIG_MTK_TEE_GP_SUPPORT)
	enum mc_result mcRet;
#endif

	M4U_MSG("%s: start\n", __func__);

	if (m4u_tee_en) {
		M4U_MSG("warning: re-initiation, %d\n", m4u_tee_en);
		goto m4u_sec_reinit;
	}

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && \
	!defined(CONFIG_MTK_TEE_GP_SUPPORT)
	/* Allocating WSM for DCI */
	mcRet = mc_malloc_wsm(MC_DEVICE_ID_DEFAULT, 0, sizeof(struct m4u_msg),
	(uint8_t **) &m4u_dci_msg, 0);
	if (mcRet != MC_DRV_OK) {
		M4U_MSG("tz_m4u: mc_malloc_wsm returned: %d\n", mcRet);
		return -1;
	}

	/* Open session the trustlet */
	m4u_dci_session.device_id = MC_DEVICE_ID_DEFAULT;
	mcRet = mc_open_session(&m4u_dci_session,
				&m4u_drv_uuid,
				(uint8_t *) m4u_dci_msg,
				(uint32_t) sizeof(struct m4u_msg));
	if (mcRet != MC_DRV_OK) {
		M4U_MSG("tz_m4u: mc_open_session returned: %d\n", mcRet);
		return -1;
	}

	M4U_DBG("tz_m4u: open DCI session returned: %d\n", mcRet);

	{
		mdelay(100);
		/* volatile int i, j;
		 * for (i = 0; i < 10000000; i++)
		 *	j++;
		 */
	}
#endif

	m4u_sec_set_context();

	if (!m4u_tee_en) {
		ret = m4u_sec_context_init();
		if (ret)
			return ret;

		m4u_tee_en = 1;
	} else {
		M4U_MSG("warning: reinit sec m4u en=%d\n", m4u_tee_en);
	}
m4u_sec_reinit:
	ret = __m4u_sec_init();
	if (ret < 0) {
		m4u_tee_en = 0;
		m4u_sec_context_deinit();
		M4U_MSG("%s:init fail,ret=0x%x\n", __func__, ret);
		return ret;
	}

	/* don't deinit ta because of multiple init operation */

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
		M4U_MSG("m4u exec command fail\n");
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
		M4U_MSG("m4u exec command fail\n");
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
		M4U_MSG("m4u exec command fail\n");
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
		M4U_MSG("m4u exec command fail\n");
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
		M4U_MSG("m4u exec command fail\n");
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
		M4U_MSG("m4u exec command fail\n");
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

	M4U_MSG("%s +, %d\n", __func__, m4u_tee_en);

	//smi_debug_bus_hang_detect(false, M4U_DEV_NAME);
	if (m4u_tee_en) {
		for (i = 0; i < SMI_LARB_NR; i++)
			larb_clock_on(i, 1);

		m4u_reg_backup_sec();

		for (i = 0; i < SMI_LARB_NR; i++)
			larb_clock_off(i, 1);
	}
	M4U_MSG("%s -\n", __func__);
}

static void m4u_late_resume(void)
{
	int i = 0;

	M4U_MSG("%s +, %d\n", __func__, m4u_tee_en);

	//smi_debug_bus_hang_detect(false, M4U_DEV_NAME);
	if (m4u_tee_en) {
		for (i = 0; i < SMI_LARB_NR; i++)
			larb_clock_on(i, 1);

		m4u_reg_restore_sec();

		for (i = 0; i < SMI_LARB_NR; i++)
			larb_clock_off(i, 1);
	}

	M4U_MSG("%s -\n", __func__);
}

static struct notifier_block m4u_fb_notifier;
static int m4u_fb_notifier_callback(
	struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	M4U_MSG("%s %ld, %d\n",
			__func__, event, FB_EVENT_BLANK);

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
#if 0
int m4u_map_nonsec_buf(int port, unsigned long mva, unsigned long size)
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
		M4U_MSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}


int m4u_unmap_nonsec_buffer(unsigned long mva, unsigned long size)
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
		M4U_MSG("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}
#endif
#endif

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
	struct M4U_PORT_STRUCT m4u_port;

	switch (cmd) {
	case MTK_M4U_T_CONFIG_PORT:
		{
			ret = copy_from_user(&m4u_port, (void *)arg,
						 sizeof(struct M4U_PORT_STRUCT
						 ));
			if (ret) {
				M4U_MSG("copy_from_user failed:%d\n", ret);
				return -EFAULT;
			}

			ret = pseudo_config_port(&m4u_port, 1);
		}
		break;
#ifdef PSEUDO_M4U_TEE_SERVICE_ENABLE
	case MTK_M4U_T_SEC_INIT:
		{
			M4U_MSG(
				"MTK M4U ioctl : MTK_M4U_T_SEC_INIT command!! 0x%x\n",
					cmd);
			mutex_lock(&gM4u_sec_init);
			ret = m4u_sec_init();
			mutex_unlock(&gM4u_sec_init);
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
#ifdef PSEUDO_M4U_TEE_SERVICE_ENABLE
	case COMPAT_MTK_M4U_T_SEC_INIT:
		{
			M4U_MSG(
				"MTK_M4U_T_SEC_INIT command!! 0x%x\n",
					cmd);
			mutex_lock(&gM4u_sec_init);
			ret = m4u_sec_init();
			mutex_unlock(&gM4u_sec_init);
		}
		break;
#endif
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
#ifndef CONFIG_FPGA_EARLY_PORTING
	unsigned int count = 0;
#endif
	int ret = 0;
	struct device_node *node = NULL;
#if defined(CONFIG_MTK_SMI_EXT)
	M4U_MSG("%s: %d\n", __func__, smi_mm_first_get());
	if (!smi_mm_first_get()) {
		M4U_MSG("SMI not start probe\n");
		return -EPROBE_DEFER;
	}
#endif
	for (i = 0; i < SMI_LARB_NR; i++) {
		/* wait for larb probe done. */
		/* if (mtk_smi_larb_ready(i) == 0) {
		 *  M4U_MSG("pseudo_probe - smi not ready\n");
		 *  return -EPROBE_DEFER;
		 * }
		 */
		/* wait for pseudo larb probe done. */
		if (!pseudo_dev_larb[i].dev &&
		    strcmp(pseudo_larbname[i], "m4u_none")) {
			M4U_MSG("%s: dev(%d) not ready\n", __func__, i);
#ifndef CONFIG_FPGA_EARLY_PORTING
			return -EPROBE_DEFER;
#endif
		}
	}

#ifdef M4U_MTEE_SERVICE_ENABLE
	{
		/* init the sec_mem_size to 400M to avoid build error. */
		unsigned int sec_mem_size = 400 * 0x100000;
		/*reserve mva range for sec */
		struct device *dev = &pdev->dev;

		pseudo_session_init();

		pseudo_sec_init(0, M4U_L2_ENABLE, &sec_mem_size);
	}
#endif
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
	pseudo_debug_init(pseudo_mmu_dev);

	node = of_find_compatible_node(NULL, NULL, "mediatek,iommu_v0");
	if (node == NULL)
		M4U_ERR("init iommu_v0 error\n");

	pseudo_mmubase[0] = (unsigned long)of_iomap(node, 0);

	for (i = 0; i < SMI_LARB_NR; i++) {
		node = of_find_compatible_node(NULL, NULL, pseudo_larbname[i]);
		if (node == NULL) {
			pseudo_larbbase[i] = 0x0;
			M4U_ERR("cannot find larb %d, skip it\n", i);
			continue;
		}

		pseudo_larbbase[i] = (unsigned long)of_iomap(node, 0);
		/* set mm engine domain to 0x4 (default value) */
		ret = larb_clock_on(i, 1);
		if (ret < 0) {
			M4U_MSG("larb%d clock on fail\n", i);
			continue;
		}
#ifndef CONFIG_FPGA_EARLY_PORTING
		M4U_MSG("m4u write all port domain to 4\n");
		count = mtk_iommu_get_larb_port_count(i);
		for (j = 0; j < count; j++) {
			pseudo_set_reg_by_mask(pseudo_larbbase[i],
							   SMI_LARB_SEC_CONx(j),
							   F_SMI_DOMN(0x7),
							   F_SMI_DOMN(0x4));
			// MDP path config
			if (m4u_port_id_of_mdp(i, j))
				pseudo_set_reg_by_mask(
						   pseudo_larbbase[i],
						   SMI_LARB_NON_SEC_CONx(j),
						   F_SMI_MMU_EN, 0x1);
		}
#else
		j = 0;
#endif
		larb_clock_off(i, 1);

		M4U_MSG("init larb%d=0x%lx\n", i, pseudo_larbbase[i]);
	}

#ifdef PSEUDO_M4U_TEE_SERVICE_ENABLE
	m4u_fb_notifier.notifier_call = m4u_fb_notifier_callback;
	ret = fb_register_client(&m4u_fb_notifier);
	if (ret)
		M4U_MSG("m4u register fb_notifier failed! ret(%d)\n", ret);
	else
		M4U_MSG("m4u register fb_notifier OK!\n");
#endif

	M4U_MSG("%s done\n", __func__);
	return 0;
}

static int pseudo_port_probe(struct platform_device *pdev)
{
	int larbid;
	unsigned int fake_nr, i;
	int ret;
	struct device *dev;
	struct device_dma_parameters *dma_param;
#if 0 //(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	unsigned long start, end;
	LIST_HEAD(list);
#endif

	M4U_MSG("start\n");
	/* dma will split the iova into max size to 65535 byte by default */
	/* if we do not set this.*/
	dma_param = kzalloc(sizeof(*dma_param), GFP_KERNEL);
	if (!dma_param)
		return -ENOMEM;

	/* set the iova to 256MB for one time map, should be suffice for ION */
	dma_param->max_segment_size = 0x10000000;
	dev = &pdev->dev;
	dev->dma_parms = dma_param;

#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	/* correct iova limit of pseudo device by update dma mask */
#if 0
	ret = mtk_iommu_get_iova_space(dev, &start, &end, &list);
	mtk_iommu_put_iova_space(dev, &resv_regions);
	if (ret) {
		dev_notice(dev, "%s, failed to get dma mask, ret:%d",
			  __func__, ret);
		goto out;
	}
	*dev->dma_mask = (u64)end;
	dev->coherent_dma_mask = (u64)end;
#else
	*dev->dma_mask = DMA_BIT_MASK(35);
	dev->coherent_dma_mask = DMA_BIT_MASK(35);
#endif
#endif

	ret = of_property_read_u32(dev->of_node, "mediatek,larbid", &larbid);
	if (ret)
		goto out;

	fake_nr = ARRAY_SIZE(pseudo_dev_larb_fake);
	if (larbid >= 0 && larbid < SMI_LARB_NR) {
		pseudo_dev_larb[larbid].larbid = larbid;
		pseudo_dev_larb[larbid].dev = dev;
		pseudo_dev_larb[larbid].name = pseudo_larbname[larbid];
		if (pseudo_dev_larb[larbid].mmuen)
			return 0;
	} else {
		for (i = 0; i < fake_nr; i++) {
			if (!pseudo_dev_larb_fake[i].dev &&
			    larbid == pseudo_dev_larb_fake[i].larbid) {
				pseudo_dev_larb_fake[i].dev = dev;
				break;
			}
		}
		if (i == fake_nr) {
			M4U_ERR("%s, pseudo not matched of dev larb%d\n",
				__func__, larbid);
			ret = -ENOMEM;
			goto out;
		}
	}

	M4U_MSG("%s done, larbid:%d, mask:0x%lx)\n",
		__func__, larbid, dev->coherent_dma_mask);
	return 0;

out:
	kfree(dma_param);
	return ret;
}

/*
 * func: get the IOVA space of target port
 * port: user input the target port id
 * base: return the start addr of IOVA space
 * max: return the end addr of IOVA space
 * list: return the reserved region list
 *       check the usage of struct iommu_resv_region
 */
int pseudo_get_iova_space(int port,
		unsigned long *base, unsigned long *max,
		struct list_head *list)
{
	struct device *dev = pseudo_get_larbdev(port);

	if (!dev)
		return -5;

	if (mtk_iommu_get_iova_space(dev, base, max, list) < 0)
		return -1;

	return 0;
}

/*
 * func: free the memory allocated by pseudo_get_iova_space()
 * list: user input the reserved region list returned by
 *       pseudo_get_iova_space()
 */
void pseudo_put_iova_space(int port,
		struct list_head *list)
{
	mtk_iommu_put_iova_space(NULL, list);
}

static int __pseudo_dump_iova_reserved_region(struct device *dev,
	struct seq_file *s)
{
	unsigned long base, max;
	int domain;
	struct iommu_resv_region *region;
	LIST_HEAD(resv_regions);

	domain = mtk_iommu_get_iova_space(dev,
				&base, &max, &resv_regions);
	if (domain < 0) {
		pr_notice("%s, %d, failed to get iova space\n",
			  __func__, __LINE__);
		return domain;
	}
	M4U_PRINT_SEQ(s,
		      "domain:%d, from:0x%lx, to:0x%lx\n",
		      domain, base, max);

	list_for_each_entry(region, &resv_regions, list)
		M4U_PRINT_SEQ(s,
			      ">> reserved: 0x%llx ~ 0x%llx\n",
			      region->start,
			      region->start + region->length - 1);

	mtk_iommu_put_iova_space(dev, &resv_regions);

	return 0;
}

/*
 * dump the reserved region of IOVA domain
 * this is the initialization result of mtk_domain_array[]
 * and the mapping relationship between
 * pseudo devices of mtk_domain_array[]
 */
int pseudo_dump_iova_reserved_region(struct seq_file *s)
{
	struct device *dev;
	unsigned int i, fake_nr;

	for (i = 0; i < SMI_LARB_NR; i++) {
		dev = pseudo_dev_larb[i].dev;
		if (!dev)
			continue;

		M4U_PRINT_SEQ(s,
			      "======== %s  ==========\n",
			  pseudo_dev_larb[i].name);
		__pseudo_dump_iova_reserved_region(dev, s);
	}

	fake_nr = ARRAY_SIZE(pseudo_dev_larb_fake);
	for (i = 0; i < fake_nr; i++) {
		M4U_PRINT_SEQ(s,
			      "======== %s  ==========\n",
			  pseudo_dev_larb_fake[i].name);
		dev = pseudo_dev_larb_fake[i].dev;
		if (!dev)
			continue;

		__pseudo_dump_iova_reserved_region(dev, s);
	}

	return 0;
}
EXPORT_SYMBOL(pseudo_dump_iova_reserved_region);

/*
 * dump the current status of pgtable
 * this is the runtime mapping result of IOVA and PA
 */
void __m4u_dump_pgtable(struct seq_file *s, unsigned int level,
	bool lock)
{
	struct m4u_client_t *client = ion_m4u_client;
	struct list_head *pListHead;
	struct m4u_buf_info_t *pList = NULL;
	unsigned long p_start, p_end, start, end;
	struct device *dev;

	if (!client)
		return;

	M4U_PRINT_SEQ(s,
		      "======== pseudo_m4u IOVA List ==========\n");
	if (s)
		pr_notice("======== pseudo_m4u IOVA List ==========\n");
	M4U_PRINT_SEQ(s,
		      " IOVA_start ~ IOVA_end	PA_start ~ PA_end	size(Byte)	port(larb-port)	name	time(ms)	process(pid)\n");
	if (s)
		pr_notice(" IOVA_start ~ IOVA_end	PA_start ~ PA_end	size(Byte)	port(larb-port)	name	time(ms)	process(pid)\n");
	if (lock)
		mutex_lock(&(client->dataMutex));
	list_for_each(pListHead, &(client->mvaList)) {
		pList = container_of(pListHead, struct m4u_buf_info_t, link);
		dev = pseudo_get_larbdev(pList->port);
		start = pList->mva,
		end = pList->mva + pList->size - 1,
		mtk_iommu_iova_to_pa(dev, start, &p_start);
		mtk_iommu_iova_to_pa(dev, end, &p_end);
		M4U_PRINT_SEQ(s,
			      " 0x%lx~0x%lx, 0x%lx~0x%lx, 0x%lx, 0x%x(%d-%d), %s, %llu,  %s(%d)\n",
			      start, end,
			      p_start, p_end,
			      pList->size, pList->port,
			      m4u_get_larbid(pList->port),
			      m4u_port_2_larb_port(pList->port),
			      iommu_get_port_name(pList->port),
			      pList->timestamp,
			      pList->task_comm, pList->pid);
		if (s)
			pr_notice(">>> 0x%lx~0x%lx, 0x%lx~0x%lx, 0x%lx, 0x%x(%d-%d), %s, %llu,  %s(%d)\n",
			      start, end,
				  p_start, p_end,
				  pList->size, pList->port,
				  m4u_get_larbid(pList->port),
				  m4u_port_2_larb_port(pList->port),
				  iommu_get_port_name(pList->port),
				  pList->timestamp,
				  pList->task_comm, pList->pid);
	}
	if (lock)
		mutex_unlock(&(client->dataMutex));

	if (level > 0)
		mtk_iommu_dump_iova_space();
}
EXPORT_SYMBOL(__m4u_dump_pgtable);

void m4u_dump_pgtable(unsigned int level)
{
	__m4u_dump_pgtable(NULL, level, false);
}
EXPORT_SYMBOL(m4u_dump_pgtable);

static int pseudo_remove(struct platform_device *pdev)
{
	if (pseudo_mmu_dev->m4u_dev_proc_entry)
		proc_remove(pseudo_mmu_dev->m4u_dev_proc_entry);
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
	if (platform_driver_register(&pseudo_port_driver)) {
		M4U_MSG("failed to register pseudo port driver");
		platform_driver_unregister(&pseudo_driver);
		return -ENODEV;
	}

	if (platform_driver_register(&pseudo_driver)) {
		M4U_MSG("failed to register pseudo driver");
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_pseudo_exit(void)
{
	platform_driver_unregister(&pseudo_driver);
	platform_driver_unregister(&pseudo_port_driver);
}

module_init(mtk_pseudo_init);
module_exit(mtk_pseudo_exit);

MODULE_DESCRIPTION("MTK pseudo m4u driver based on iommu");
MODULE_AUTHOR("Honghui Zhang <honghui.zhang@mediatek.com>");
MODULE_LICENSE("GPL");
