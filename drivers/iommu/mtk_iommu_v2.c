// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#include <linux/bootmem.h>
#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/dma-iommu.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/list.h>
#include <linux/of_address.h>
#include <linux/of_iommu.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched/clock.h>
#include <asm/barrier.h>
#include <soc/mediatek/smi.h>
#include <linux/dma-debug.h>
#ifndef CONFIG_ARM64
#include <asm/dma-iommu.h>
#endif
#include "mtk_lpae.h"
//smccc related include
//#include "mtk_secure_api.h" //old
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>

#include "io-pgtable.h"
#include "mtk_iommu.h"
#include "mach/mt_iommu.h"
#include "mach/mt_iommu_plat.h"
#include "mach/pseudo_m4u.h"
#include "mtk_iommu_ext.h"
#if defined(APU_IOMMU_INDEX) && \
	defined(IOMMU_POWER_CLK_SUPPORT) && \
	defined(CONFIG_MTK_APUSYS_SUPPORT)
#include "apusys_power.h"
#endif
#if defined(APU_IOMMU_INDEX) && \
	defined(MTK_APU_TFRP_SUPPORT)
#include "mnoc_api.h"
#endif

#define PREALLOC_DMA_DEBUG_ENTRIES 4096

#define MTK_IOMMU_DEBUG

/* IO virtual address start page frame number */
#define IOVA_START_PFN	  (1)
#define IOVA_PFN(addr)	  ((addr) >> PAGE_SHIFT)
#define DMA_32BIT_PFN	   IOVA_PFN(DMA_BIT_MASK(32))

#define MTK_PROTECT_PA_ALIGN	(256)

#ifdef MTK_IOMMU_BANK_IRQ_SUPPORT
static int mtk_irq_bank[MTK_IOMMU_M4U_COUNT][MTK_IOMMU_BANK_NODE_COUNT];
#endif


#ifdef IOMMU_DEBUG_ENABLED
static bool g_tf_test;
#endif
void mtk_iommu_switch_tf_test(bool enable,
	const char *msg)
{
#ifdef IOMMU_DEBUG_ENABLED
	g_tf_test = !!enable;
	pr_notice("<<<<<<<<<<< mtk iommu translation fault test is switched to %d by %s >>>>>>>>>>>",
		  g_tf_test, msg);
#endif
}

struct mtk_iommu_domain *to_mtk_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct mtk_iommu_domain, domain);
}

static struct iommu_ops mtk_iommu_ops;
static const struct of_device_id mtk_iommu_of_ids[];
static LIST_HEAD(m4ulist);
static unsigned int total_iommu_cnt;
static unsigned int init_data_id;

static struct mtk_iommu_data *mtk_iommu_get_m4u_data(int id)
{
	struct mtk_iommu_data *data;
	unsigned int i = 0;

	list_for_each_entry(data, &m4ulist, list) {
		if (data && data->m4uid == id &&
		    data->base && !IS_ERR(data->base))
			return data;
		if (++i >= total_iommu_cnt)
			return NULL;
	}

	pr_notice("%s, %d, failed to get data of %d\n", __func__, __LINE__, id);
	return NULL;
}

#if MTK_IOMMU_PAGE_TABLE_SHARE
static struct mtk_iommu_pgtable *m4u_pgtable;
#endif
static struct mtk_iommu_pgtable *mtk_iommu_get_pgtable(
			const struct mtk_iommu_data *data, unsigned int data_id)
{
#if !MTK_IOMMU_PAGE_TABLE_SHARE
	if (data)
		return data->pgtable;

	data = mtk_iommu_get_m4u_data(data_id);
	if (data)
		return data->pgtable;

	return NULL;
#else
	return m4u_pgtable;
#endif
}

int mtk_iommu_set_pgtable(
			const struct mtk_iommu_data *data,
			unsigned int data_id,
			struct mtk_iommu_pgtable *value)
{
#if !MTK_IOMMU_PAGE_TABLE_SHARE
	if (data) {
		data->pgtable = value;
	} else {
		data = mtk_iommu_get_m4u_data(data_id);
		if (data)
			data->pgtable = value;
		else
			return -1;
	}
#else
	m4u_pgtable = value;
#endif

	return 0;
}

static unsigned int __mtk_iommu_get_domain_id(
				unsigned int larbid, unsigned int portid)
{
	unsigned int domain_id = MTK_IOVA_DOMAIN_COUNT;
	int i;

	if (larbid >= MTK_IOMMU_LARB_NR) {
		pr_notice("%s, %d, cannot find domain of port(%d-%d)\n",
			  __func__, __LINE__,
			  larbid, portid);
		return MTK_IOVA_DOMAIN_COUNT;
	}

	for (i = 0; i < MTK_IOVA_DOMAIN_COUNT; i++) {
		if (mtk_domain_array[i].port_mask[larbid] &
		    (1 << portid)) {
			domain_id = i;
			break;
		}
	}

	if (domain_id == MTK_IOVA_DOMAIN_COUNT)
		pr_notice("%s, %d, cannot find domain of port(%d-%d)\n",
			  __func__, __LINE__,
			  larbid, portid);
	return domain_id;
}

static unsigned int mtk_iommu_get_domain_id(
					struct device *dev)
{
	struct iommu_fwspec *fwspec;
	unsigned int larbid, portid, domain_id = 0;

	if (!dev)
		return MTK_IOVA_DOMAIN_COUNT;

	fwspec = dev->iommu_fwspec;
	larbid = MTK_IOMMU_TO_LARB(fwspec->ids[0]);
	portid = MTK_IOMMU_TO_PORT(fwspec->ids[0]);

	domain_id = __mtk_iommu_get_domain_id(larbid, portid);
	if (domain_id >= MTK_IOVA_DOMAIN_COUNT)
		dev_notice(dev, "%s, %d, cannot find domain of port%d[%d-%d]\n",
			  __func__, __LINE__, fwspec->ids[0],
			  larbid, portid);
	return domain_id;
}

int mtk_iommu_get_port_id(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	if (!dev->iommu_fwspec ||
	    !dev->iommu_fwspec->iommu_priv)
		return M4U_PORT_GPU;

	return dev->iommu_fwspec->ids[0];
}
EXPORT_SYMBOL_GPL(mtk_iommu_get_port_id);

static struct iommu_domain *__mtk_iommu_get_domain(
				const struct mtk_iommu_data *data,
				unsigned int larbid, unsigned int portid)
{
	unsigned int domain_id;
	struct mtk_iommu_domain *dom;

	domain_id = __mtk_iommu_get_domain_id(
				larbid, portid);
	if (domain_id == MTK_IOVA_DOMAIN_COUNT)
		return NULL;

	if (!data->pgtable)
		return NULL;

	list_for_each_entry(dom, &data->pgtable->m4u_dom, list) {
		if (dom->id == domain_id)
			return &dom->domain;
	}
	return NULL;
}

static struct mtk_iommu_domain *__mtk_iommu_get_mtk_domain(
					struct device *dev)
{
	struct mtk_iommu_data *data;
	struct mtk_iommu_domain *dom;
	unsigned int domain_id;

	if (!dev)
		return NULL;

	data = dev->iommu_fwspec->iommu_priv;
	domain_id = mtk_iommu_get_domain_id(dev);
	if (domain_id == MTK_IOVA_DOMAIN_COUNT)
		return NULL;

	list_for_each_entry(dom, &data->pgtable->m4u_dom, list) {
		if (dom->id == domain_id)
			return dom;
	}
	return NULL;
}

static struct iommu_group *mtk_iommu_get_group(
					struct device *dev)
{
	struct mtk_iommu_domain *dom;

	dom = __mtk_iommu_get_mtk_domain(dev);
	if (dom)
		return dom->group;

	return NULL;
}

bool mtk_dev_is_size_alignment(struct device *dev)
{
#ifdef MTK_IOMMU_SIZE_NOT_ALIGNMENT
	return false;
#else
	struct iommu_fwspec *fwspec;
	unsigned int larbid, portid, port;
	int i, count;

	if (!dev)
		return true;

	fwspec = dev->iommu_fwspec;
	larbid = MTK_IOMMU_TO_LARB(fwspec->ids[0]);
	portid = MTK_IOMMU_TO_PORT(fwspec->ids[0]);
	port = MTK_M4U_ID(larbid, portid);

	count = ARRAY_SIZE(port_size_not_aligned);
	for (i = 0; i < count; i++)
		if (port == port_size_not_aligned[i])
			return false;

	return true;
#endif
}
EXPORT_SYMBOL_GPL(mtk_dev_is_size_alignment);

#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
static unsigned int __mtk_iommu_get_boundary_id(
				unsigned int larbid, unsigned int portid)
{
	unsigned int boundary = MTK_IOMMU_IOVA_BOUNDARY_COUNT;
	int i;

	if (larbid >= MTK_IOMMU_LARB_NR)
		return MTK_IOMMU_IOVA_BOUNDARY_COUNT;

	for (i = 0; i < MTK_IOVA_DOMAIN_COUNT; i++) {
		if (mtk_domain_array[i].port_mask[larbid] &
		    (1 << portid)) {
			boundary = mtk_domain_array[i].boundary;
#ifdef MTK_IOMMU_DEBUG
			pr_debug("%s, %d, larb%d, port%d bundary%d\n",
				  __func__, __LINE__,
				  larbid, portid, boundary);
#endif
			break;
		}
	}

	return boundary;
}

int mtk_iommu_get_boundary_id(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	unsigned int larbid, portid, boundary;

	larbid = MTK_IOMMU_TO_LARB(fwspec->ids[0]);
	portid = MTK_IOMMU_TO_PORT(fwspec->ids[0]);

	boundary =  __mtk_iommu_get_boundary_id(larbid, portid);
	if (boundary >= MTK_IOMMU_IOVA_BOUNDARY_COUNT)
		return -1;

	return boundary;
}
#endif

int __mtk_iommu_atf_call(unsigned int cmd, unsigned int m4u_id,
		unsigned int bank, size_t *tf_port,
		size_t *tf_iova, size_t *tf_int)
{
#ifdef IOMMU_DESIGN_OF_BANK
	unsigned int atf_cmd = 0;
	int ret = 0;
	struct arm_smccc_res res;

	if (cmd >= IOMMU_ATF_CMD_COUNT ||
	    m4u_id >= MTK_IOMMU_M4U_COUNT ||
	    bank > MTK_IOMMU_BANK_COUNT) {
		pr_notice("%s, %d, invalid m4u:%d, bank:%d, cmd:%d\n",
			  __func__, __LINE__, m4u_id, bank, cmd);
		return -1;
	}
	atf_cmd = IOMMU_ATF_SET_COMMAND(m4u_id, bank, cmd);
	/*pr_notice("%s, M4U CALL ATF CMD:0x%x\n", __func__, atf_cmd);*/
	arm_smccc_smc(MTK_M4U_DEBUG_DUMP, atf_cmd,
			      0, 0, 0, 0, 0, 0, &res);
	ret = res.a0;
	*tf_port = res.a1;
	*tf_iova = res.a2;
	*tf_int = res.a3;
	return ret;
#else
	return 0;
#endif
}

int mtk_iommu_atf_call(unsigned int cmd, unsigned int m4u_id,
		unsigned int bank)
{
	size_t tf_port = 0, tf_iova = 0, tf_int = 0;

	return __mtk_iommu_atf_call(cmd, m4u_id, bank, &tf_port,
				&tf_iova, &tf_int);
}

#ifndef SMI_LARB_SEC_CON_EN
int mtk_iommu_dump_sec_larb(int larb, int port)
{
	unsigned int atf_cmd = 0;
	int ret = 0;
	struct arm_smccc_res res;

	if (larb >= SMI_LARB_NR ||
	    port >= ONE_SMI_PORT_NR) {
		pr_notice("%s, %d, invalid larb:%d, port:%d\n",
			  __func__, __LINE__, larb, port);
		return -1;
	}

	atf_cmd = IOMMU_ATF_SET_COMMAND(0, 0, IOMMU_ATF_DUMP_SMI_SEC_LARB);
	arm_smccc_smc(MTK_M4U_DEBUG_DUMP, atf_cmd,
			      MTK_M4U_ID(larb, port), 0, 0, 0, 0, 0, &res);
	ret = res.a0;

	return ret;
}
#endif

static void mtk_iommu_atf_test_recovery(unsigned int m4u_id, unsigned int cmd)
{
	int ret = 0;

	if (cmd == IOMMU_ATF_SECURITY_DEBUG_DISABLE)
		ret = mtk_iommu_atf_call(
				IOMMU_ATF_SECURITY_DEBUG_ENABLE,
				m4u_id,
				MTK_IOMMU_BANK_COUNT);

	if (cmd == IOMMU_ATF_SECURITY_DEBUG_ENABLE)
		ret = mtk_iommu_atf_call(
				IOMMU_ATF_SECURITY_DEBUG_DISABLE,
				m4u_id,
				MTK_IOMMU_BANK_COUNT);
}

void mtk_iommu_atf_test(unsigned int m4u_id, unsigned int cmd)
{
	int ret = 0, i;
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);

	if (m4u_id >= MTK_IOMMU_M4U_COUNT || !data)
		return;

#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		pr_notice("%s: iommu:%d power off\n",
			  __func__, m4u_id);
		return;
	}
#endif

	if (cmd < IOMMU_ATF_CMD_COUNT) {
		pr_notice("======== IOMMU test ATF cmd %d: %s=========\n",
			  cmd, iommu_atf_cmd_name[cmd]);
		ret = mtk_iommu_atf_call(cmd, m4u_id,
				MTK_IOMMU_BANK_COUNT);
		pr_notice(">>> cmd:%d %s, ret:%d\n", cmd,
			  (ret ? "FAIL" : "PASS"), ret);
		mtk_iommu_atf_test_recovery(m4u_id, cmd);
		return;
	}

	for (i = 0; i < IOMMU_ATF_DUMP_SECURE_PORT_CONFIG; i++) {
		pr_notice("======== IOMMU test ATF cmd %d: %s=========\n",
			  i, iommu_atf_cmd_name[i]);
		ret = mtk_iommu_atf_call(i, m4u_id,
				MTK_IOMMU_BANK_COUNT);
		pr_notice(">>> cmd:%d %s, ret:%d\n", i,
			  (ret ? "FAIL" : "PASS"), ret);
		mtk_iommu_atf_test_recovery(m4u_id, cmd);
	}
}

int mtk_switch_secure_debug_func(unsigned int m4u_id, bool enable)
{
#ifdef IOMMU_SECURITY_DBG_SUPPORT
	int ret = 0;

	if (m4u_id >= MTK_IOMMU_M4U_COUNT)
		return -EINVAL;

	if (enable)
		ret = mtk_iommu_atf_call(IOMMU_ATF_SECURITY_DEBUG_ENABLE,
				m4u_id, MTK_IOMMU_BANK_COUNT);
	else
		ret = mtk_iommu_atf_call(IOMMU_ATF_SECURITY_DEBUG_DISABLE,
				m4u_id, MTK_IOMMU_BANK_COUNT);
	if (ret)
		return ret;
#endif
	return 0;
}

static int mtk_dump_reg(const struct mtk_iommu_data *data,
	unsigned int start, unsigned int length, struct seq_file *s)
{
	int i = 0;
	void __iomem *base;

	if (!data) {
		mmu_seq_print(s,
			      "%s, %d, invalid data\n",
			      __func__, __LINE__);
		return -1;
	}
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron)
		return 0;
#endif

	base = data->base;

	for (i = 0; i < length; i += 4) {
		if (length - i == 1)
			mmu_seq_print(s,
				      "0x%x=0x%x\n",
				      start + 4 * i,
				      readl_relaxed(base + start + 4 * i));
		else if (length - i == 2)
			mmu_seq_print(s,
				      "0x%x=0x%x, 0x%x=0x%x\n",
				      start + 4 * i,
				      readl_relaxed(base + start + 4 * i),
				      start + 4 * (i + 1),
				      readl_relaxed(base + start +
						    4 * (i + 1)));
		else if (length - i == 3)
			mmu_seq_print(s,
				      "0x%x=0x%x, 0x%x=0x%x, 0x%x=0x%x\n",
				      start + 4 * i,
				      readl_relaxed(base + start + 4 * i),
				      start + 4 * (i + 1),
				      readl_relaxed(base + start +
						    4 * (i + 1)),
				      start + 4 * (i + 2),
				      readl_relaxed(base + start +
						    4 * (i + 2)));
		else if (length - i >= 4)
			mmu_seq_print(s,
				      "0x%x=0x%x, 0x%x=0x%x, 0x%x=0x%x, 0x%x=0x%x\n",
				      start + 4 * i,
				      readl_relaxed(base + start + 4 * i),
				      start + 4 * (i + 1),
				      readl_relaxed(base + start +
						    4 * (i + 1)),
				      start + 4 * (i + 2),
				      readl_relaxed(base + start +
						    4 * (i + 2)),
				      start + 4 * (i + 3),
				      readl_relaxed(base + start +
						    4 * (i + 3)));
	}

	return 0;
}
static int mtk_dump_debug_reg_info(const struct mtk_iommu_data *data,
		struct seq_file *s)
{
	mmu_seq_print(s,
		      "------ iommu:%d debug register ------\n",
		      data->m4uid);
	return mtk_dump_reg(data, REG_MMU_DBG(0), MTK_IOMMU_DEBUG_REG_NR, s);
}

static int mtk_dump_rs_sta_info(const struct mtk_iommu_data *data, int mmu,
		struct seq_file *s)
{
	mmu_seq_print(s,
		      "------ iommu:%d mmu%d: RS status register ------\n",
		      data->m4uid, mmu);
	mmu_seq_print(s,
		      "--<0x0>iova/bank --<0x4>descriptor --<0x8>2nd-base --<0xc>status\n");
	return mtk_dump_reg(data,
			    REG_MMU_RS_VA(mmu, 0),
			    MTK_IOMMU_RS_COUNT * 4, s);
}

int __mtk_dump_reg_for_hang_issue(unsigned int m4u_id,
		struct seq_file *s)
{
	int cnt, ret, i;
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);
	void __iomem *base;
	unsigned long flags;

	mmu_seq_print(s,
		      "==== hang debug reg iommu%d ====\n",
		      m4u_id);

	if (!data || data->base == 0) {
		mmu_seq_print(s,
			      "%s, %d, base is NULL\n",
			      __func__, __LINE__);
		return 0;
	}
	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		mmu_seq_print(s,
			      "iommu:%d power off\n", m4u_id);
		return 0;
	}
#endif

	base = data->base;

	/* control register */
	mmu_seq_print(s,
		      "REG_MMU_PT_BASE_ADDR(0x0)	   = 0x%x\n",
		      readl_relaxed(base + REG_MMU_PT_BASE_ADDR));
	mmu_seq_print(s,
		      "REG_MMU_TFRP_PADDR(0x114)	   = 0x%x\n",
		      readl_relaxed(base + REG_MMU_TFRP_PADDR));
	mmu_seq_print(s,
		      "REG_MMU_DUMMY(0x44)	   = 0x%x\n",
		      readl_relaxed(base + REG_MMU_DUMMY));
	mmu_seq_print(s,
		      "REG_MMU_MISC_CTRL(0x48)   = 0x%x\n",
		      readl_relaxed(base + REG_MMU_MISC_CTRL));
	mmu_seq_print(s,
		      "REG_MMU_DCM_DIS(0x50)	 = 0x%x\n",
		      readl_relaxed(base + REG_MMU_DCM_DIS));
	mmu_seq_print(s,
		      "REG_MMU_WR_LEN_CTRL(0x54) = 0x%x\n",
		      readl_relaxed(base + REG_MMU_WR_LEN_CTRL));
	mmu_seq_print(s,
		      "REG_MMU_TBW_ID(0xA0)	  = 0x%x\n",
		      readl_relaxed(base + REG_MMU_TBW_ID));
	mmu_seq_print(s,
		      "REG_MMU_CTRL_REG(0x110)   = 0x%x\n",
		      readl_relaxed(base + REG_MMU_CTRL_REG));

	/* dump five times*/
	ret = mtk_switch_secure_debug_func(m4u_id, 1);
	if (ret) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		mmu_seq_print(s,
			      "%s, %d, failed to enable secure debug signal\n",
			      __func__, __LINE__);
		return 0;
	}

	for (cnt = 0; cnt < 3; cnt++) {
		mmu_seq_print(s,
			      "====== the %d time: REG_MMU_STA(0x08) = 0x%x ======\n",
			      cnt, readl_relaxed(base + REG_MMU_STA));
		mtk_dump_debug_reg_info(data, s);
		for (i = 0; i < MTK_IOMMU_MMU_COUNT; i++)
			mtk_dump_rs_sta_info(data, i, s);
	}

	mmu_seq_print(s,
		      "========== dump hang reg end ========\n");

	ret = mtk_switch_secure_debug_func(m4u_id, 0);
	if (ret)
		mmu_seq_print(s,
			      "%s, %d, failed to disable secure debug signal\n",
			      __func__, __LINE__);

	spin_unlock_irqrestore(&data->reg_lock, flags);

	return 0;
}

void mtk_dump_reg_for_hang_issue(unsigned int type)
{
	int i, start = -1, end = -1;

#ifdef APU_IOMMU_INDEX
	switch (type) {
	case 0: //smi power on before dump
		start = 0;
		end = APU_IOMMU_INDEX - 1;
		break;
	case 1: //apu power on before dump
		start = APU_IOMMU_INDEX;
		end = MTK_IOMMU_M4U_COUNT - 1;
		break;
	default:
		start = -1;
		end = -1;
		break;
	}
#else
	start = 0;
	end = MTK_IOMMU_M4U_COUNT - 1;
#endif

	if (start < 0 || end < 0)
		return;

	for (i = start; i <= end; i++)
		__mtk_dump_reg_for_hang_issue(i, NULL);
}
EXPORT_SYMBOL_GPL(mtk_dump_reg_for_hang_issue);

int mtk_iommu_dump_reg(int m4u_id, unsigned int start,
	unsigned int end, char *user)
{
	int ret = 0;
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);
	unsigned long flags;

	if (!data || !user)
		return -1;

	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return 0;
	}
#endif

	pr_notice("====== [%s] dump reg of iommu:%d from 0x%x to 0x%x =======\n",
		  user, m4u_id, start, end);

	ret = mtk_switch_secure_debug_func(m4u_id, 1);
	if (ret) {
		pr_notice("%s, %d, failed to enable secure debug signal\n",
			  __func__, __LINE__);
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return 0;
	}

	mtk_dump_reg(data, start, (end - start + 4) / 4, NULL);
	pr_notice("============= dump end ===============\n");

	ret = mtk_switch_secure_debug_func(m4u_id, 0);
	if (ret)
		pr_notice("%s, %d, failed to disable secure debug signal\n",
			  __func__, __LINE__);

	spin_unlock_irqrestore(&data->reg_lock, flags);
	return 0;
}

static unsigned int g_iommu_power_support;
unsigned int mtk_iommu_power_support(void)
{
	return g_iommu_power_support;
}

#ifdef IOMMU_POWER_CLK_SUPPORT
static int mtk_iommu_hw_clock_power_switch(const struct mtk_iommu_data *data,
			bool enable, char *master, bool is_clk)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int err_id = -1, ret = 0;
	unsigned int i, clk_nr;
	struct mtk_iommu_clks *m4u_clks;
	struct clk *clk_node;

	if (!data) {
		pr_notice("%s, %d, invalid data\n", __func__, __LINE__);
		return -1;
	}

	m4u_clks = data->m4u_clks;
	if (!m4u_clks) {
		pr_notice("%s, %d, invalid m4u_clks\n", __func__, __LINE__);
		return -1;
	}

	if (is_clk)
		clk_nr = m4u_clks->nr_clks;
	else
		clk_nr = m4u_clks->nr_powers;

	for (i = 0; i < clk_nr; i++) {
		if (is_clk)
			clk_node = m4u_clks->clks[i];
		else
			clk_node = m4u_clks->powers[i];

		if (enable)
			ret = clk_prepare_enable(clk_node);
		else
			clk_disable_unprepare(clk_node);

		if (ret) {
			err_id = i;
			if (enable) {
				for (i = 0; i < err_id; i++) {
					clk_disable_unprepare(
						m4u_clks->clks[i]);
				}
			}
			break;
		}
	}

	if (ret)
		pr_notice("%s failed to %s %s[%d] of iommu%d, id%d for %s, ret:%d\n",
			  __func__, (enable ? "enable" : "disable"),
			  (is_clk ? "clock" : "power"), i,
			  data->m4uid, err_id, master, ret);
	else
		pr_debug("%s: %s %s[%d] of iommu%d, id%d for %s, ret:%d\n",
			 __func__, (enable ? "enable" : "disable"),
			 (is_clk ? "clock" : "power"), i,
			 data->m4uid, err_id, master, ret);

	return ret;
#else
	return 0;
#endif
}

int mtk_iommu_larb_clock_switch(unsigned int larb, bool enable)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int ret = 0;
	unsigned int iommu_id;
	struct mtk_iommu_data *data;

	if (larb >= ARRAY_SIZE(smi_clk_name)) {
		pr_notice("%s, invalid larb %d\n",
			  __func__, larb);
		return -1;
	}

	iommu_id = mtk_get_iommu_index(larb);
	data = mtk_iommu_get_m4u_data(iommu_id);
	ret = mtk_iommu_hw_clock_power_switch(data,
				enable, smi_clk_name[larb], true);

	if (ret)
		pr_notice("switch larb clock err:%d, larb:%d, on:%d\n",
			  ret, larb, enable);

	return ret;
#else
	return 0;
#endif
}
#endif

int mtk_iommu_port_clock_switch(unsigned int port, bool enable)
{
#ifdef IOMMU_POWER_CLK_SUPPORT
	unsigned int larb;
	int ret = 0;

	larb = MTK_IOMMU_TO_LARB(port);
	ret = mtk_iommu_larb_clock_switch(larb, enable);
	return ret;
#else
	return 0;
#endif
}

static int mtk_iommu_power_switch(struct mtk_iommu_data *data,
			bool enable, char *master)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef IOMMU_POWER_CLK_SUPPORT
	int ret = 0;

#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->m4u_clks->nr_powers)
		return 0;
#endif
	ret = mtk_iommu_hw_clock_power_switch(data, enable, master, !enable);
	pr_notice("%s: %d: %s %s %s of iommu%d for %s, ret=%d\n",
		  __func__, __LINE__,
		  enable ? "enable" : "disable",
		  enable ? "power" : "clock",
		  ret ? "error" : "pass",
		  data->m4uid,
		  master ? master : "NULL",
		  ret);
	if (ret)
		return ret;

	ret = mtk_iommu_hw_clock_power_switch(data, enable, master, enable);
	pr_notice("%s, %d, %s %s %s at iommu%d, for %s, ret=%d\n",
		  __func__, __LINE__,
		  enable ? "enable" : "disable",
		  enable ? "clock" : "power",
		  ret ? "error" : "pass",
		  data->m4uid,
		  master ? master : "NULL",
		  ret);

	return ret;
#endif
#endif
	return 0;
}

int mtk_iommu_power_switch_by_id(unsigned int m4uid,
			bool enable, char *master)
{
	struct mtk_iommu_data *data;

	if (m4uid >= MTK_IOMMU_M4U_COUNT) {
		pr_notice("%s, invalid m4uid:%d,%s\n",
			  __func__, m4uid,
			  master ? master : "NULL");
		return -1;
	}

	data = mtk_iommu_get_m4u_data(m4uid);
	if (!data) {
		pr_notice("%s, err data of m4uid:%d,%s\n",
			  __func__, m4uid,
			  master ? master : "NULL");
		return -2;
	}
	return mtk_iommu_power_switch(data, enable, master);
}

static void __mtk_iommu_tlb_flush_all(const struct mtk_iommu_data *data)
{
	if (!data->base || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base\n",
			  __func__, __LINE__);
		return;
	}

#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron)
		return;
#endif
	writel_relaxed(F_MMU_INV_EN_L2 | F_MMU_INV_EN_L1,
		   data->base + REG_INVLID_SEL);
	writel_relaxed(F_MMU_INVLDT_ALL,
		   data->base + REG_MMU_INVLDT);
	wmb(); /* Make sure the tlb flush all done */
}

static int __mtk_iommu_tlb_sync(struct mtk_iommu_data *data)
{
	return 0;
}

static void __mtk_iommu_tlb_add_flush_nosync(
					   struct mtk_iommu_data *data,
					   unsigned long iova_start,
					   unsigned long iova_end)
{
	unsigned int regval;
	int ret = 0;
	u32 tmp;
	unsigned long start, end, flags;

	if (!data->base  || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base addr\n",
			  __func__, __LINE__);
		return;
	}

	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return;
	}
#endif
	start = round_down(iova_start, SZ_4K);
	end = round_down(iova_end, SZ_4K);

	//0x38 for V1, 0x2c for V2
	writel_relaxed(F_MMU_INV_EN_L2 | F_MMU_INV_EN_L1,
		   data->base + REG_INVLID_SEL);

	regval = (unsigned int)(start &
				F_MMU_INVLD_BIT31_12);
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	regval |= (start >> 32) & F_MMU_INVLD_BIT32;
#endif
	writel_relaxed(regval, //0x24
		   data->base + REG_MMU_INVLD_START_A);
	regval = (unsigned int)(end &
				F_MMU_INVLD_BIT31_12);
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	regval |= (end >> 32) & F_MMU_INVLD_BIT32;
#endif
	writel_relaxed(regval, //0x28
		   data->base + REG_MMU_INVLD_END_A);
	writel(F_MMU_INVLDT_RNG, //0x20
		   data->base + REG_MMU_INVLDT);
	wmb(); /*make sure the TLB sync has been triggered*/

	ret = readl_poll_timeout_atomic(data->base +
					REG_MMU_CPE_DONE, //0x12c
					tmp, tmp != 0,
					10, 5000);

	if (ret) {
		dev_notice(data->dev,
			 "Partial TLB flush time out, iommu:%d,start=0x%lx(0x%lx),end=0x%lx(0x%lx)\n",
			 data->m4uid, iova_start, start,
			 iova_end, end);
		mtk_dump_reg(data, REG_MMU_PT_BASE_ADDR, 14, NULL);
		__mtk_iommu_tlb_flush_all(data);
	}
	/* Clear the CPE status */
	writel_relaxed(0, data->base + REG_MMU_INVLD_START_A);
	writel_relaxed(0, data->base + REG_MMU_INVLD_END_A);
	writel_relaxed(0, data->base + REG_MMU_CPE_DONE);
	wmb(); /*make sure the TLB status has been cleared*/

	spin_unlock_irqrestore(&data->reg_lock, flags);
	return;
}

#if MTK_IOMMU_PAGE_TABLE_SHARE
void mtk_iommu_dump_iova_space(unsigned long target)
{
	struct mtk_iommu_domain *dom;
	int i = 0;
	struct mtk_iommu_pgtable *pgtable = mtk_iommu_get_pgtable(NULL, 0);

	if (!pgtable) {
		pr_notice("%s, invalid pgtable\n", __func__);
		return;
	}

	pr_notice("========= %s++ total %d domain ============\n",
		  __func__, pgtable->domain_count);
	list_for_each_entry(dom, &pgtable->m4u_dom, list) {
		pr_notice("===== domain %d =====\n", dom->id);
		iommu_dma_dump_iovad(&dom->domain, target);
		if (++i >= pgtable->domain_count)
			break;
	}
	pr_notice("========= %s-- ============\n", __func__);
}

static void mtk_iommu_tlb_flush_all_lock(void *cookie, bool lock)
{
	struct mtk_iommu_data *data, *temp;
	int i = 0;
	unsigned long flags;

	list_for_each_entry_safe(data, temp, &m4ulist, list) {
		if (lock)
			spin_lock_irqsave(&data->reg_lock, flags);
		__mtk_iommu_tlb_flush_all(data);
		if (lock)
			spin_unlock_irqrestore(&data->reg_lock, flags);
		if (++i >= total_iommu_cnt)
			return;  //do not while loop if m4ulist is destroyed
	}
}

static void mtk_iommu_tlb_add_flush_nosync(unsigned long iova,
					   size_t size,
					   size_t granule, bool leaf,
					   void *cookie)
{
	struct mtk_iommu_data *data, *temp;
	unsigned int i = 0;
	unsigned long iova_start = iova;
	unsigned long iova_end = iova + size - 1;

	list_for_each_entry_safe(data, temp, &m4ulist, list) {
		__mtk_iommu_tlb_add_flush_nosync(data, iova_start, iova_end);
		if (++i >= total_iommu_cnt)
			return;  //do not while loop if m4ulist is destroyed
	}
}

static void mtk_iommu_tlb_sync(void *cookie)
{
	struct mtk_iommu_data *data, *temp;
	int i = 0, ret;

	list_for_each_entry_safe(data, temp, &m4ulist, list) {
		ret = __mtk_iommu_tlb_sync(data);
		if (ret)
			pr_notice("%s, failed at iommu:%d, of the %d time\n",
				  __func__, data->m4uid, i);
		if (++i >= total_iommu_cnt)
			return;  //do not while loop if m4ulist is destroyed
	}
}

#else
void mtk_iommu_dump_iova_space(unsigned long iova)
{
	struct mtk_iommu_domain *dom;
	struct mtk_iommu_pgtable *pgtable;
	int i = 0;

	for (i = 0; i < total_iommu_cnt; i++)
		pr_notice("<<<<<<<< iommu %d >>>>>>>>\n", i);
		pgtable = mtk_iommu_get_pgtable(NULL, i);
		if (!pgtable)
			continue;
		list_for_each_entry(dom, &pgtable->m4u_dom, list) {
			pr_notice("===== domain %d =====\n", dom->id);
			iommu_dma_dump_iovad(dom->domain, iova);
			pr_notice("=====================\n");
		}
		pr_notice("<<<<<<<<<<<<>>>>>>>>>>>>\n");
	}
}

static void mtk_iommu_tlb_flush_all_lock(void *cookie, bool lock)
{
	struct mtk_iommu_data *data = cookie->data;
	unsigned long flags;

	if (lock)
		spin_lock_irqsave(&data->reg_lock, flags);
	__mtk_iommu_tlb_flush_all(data);
	if (lock)
		spin_unlock_irqrestore(&data->reg_lock, flags);
}

static void mtk_iommu_tlb_add_flush_nosync(unsigned long iova,
					   size_t size,
					   size_t granule, bool leaf,
					   void *cookie)
{
	const struct mtk_iommu_data *data = cookie->data;

	__mtk_iommu_tlb_add_flush_nosync(data, iova_start, iova_end);
}

static void mtk_iommu_tlb_sync(void *cookie)
{
	const struct mtk_iommu_data *data = cookie->data;

	ret = __mtk_iommu_tlb_sync(data);
	if (ret)
		pr_notice("%s, failed at iommu:%d\n",
			  __func__, data->m4uid);
}
#endif

#ifdef MTK_IOMMU_PERFORMANCE_IMPROVEMENT
static void mtk_iommu_tlb_add_flush_nosync_dummy(unsigned long iova,
					   size_t size,
					   size_t granule, bool leaf,
					   void *cookie)
{
	/* do nothing for each sg table pa node sync
	 * but do one time tlb sync at then end of page table ops
	 */
}

void mtk_iommu_tlb_flush_all_dummy(void *cookie)
{
	/* do nothing for each sg table pa node sync
	 * but do one time tlb sync at then end of page table ops
	 */
}

static void mtk_iommu_tlb_sync_dummy(void *cookie)
{
	/* do nothing for each sg table pa node sync
	 * but do one time tlb sync at then end of page table ops
	 */
}

#endif
void mtk_iommu_tlb_flush_all(void *cookie)
{
	mtk_iommu_tlb_flush_all_lock(cookie, true);
}

static void mtk_iommu_iotlb_flush_all(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	mtk_iommu_tlb_flush_all(dom);
}

static void mtk_iommu_iotlb_range_add(struct iommu_domain *domain,
					unsigned long iova, size_t size)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	mtk_iommu_tlb_add_flush_nosync(iova, size, 0, 0, dom);
}
static void mtk_iommu_iotlb_sync(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	mtk_iommu_tlb_sync(dom);
}

static const struct iommu_gather_ops mtk_iommu_gather_ops = {
#ifdef MTK_IOMMU_PERFORMANCE_IMPROVEMENT
	.tlb_add_flush = mtk_iommu_tlb_add_flush_nosync_dummy,
	.tlb_flush_all = mtk_iommu_tlb_flush_all_dummy,
	.tlb_sync = mtk_iommu_tlb_sync_dummy,
#else
	.tlb_flush_all = mtk_iommu_tlb_flush_all,
	.tlb_add_flush = mtk_iommu_tlb_add_flush_nosync,
	.tlb_sync = mtk_iommu_tlb_sync,
#endif
};

static inline void mtk_iommu_intr_modify_all(unsigned long enable)
{
	struct mtk_iommu_data *data, *temp;
	unsigned int i = 0;
	unsigned long flags;

	list_for_each_entry_safe(data, temp, &m4ulist, list) {
		if (!data->base  || IS_ERR(data->base)) {
			pr_notice("%s, %d, invalid base addr\n",
				  __func__, __LINE__);
			continue;
		}

		spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
		if (!data->poweron) {
			spin_unlock_irqrestore(&data->reg_lock, flags);
			continue;
		}
#endif

		if (enable) {
			writel_relaxed(0x6f,
				   data->base +
				   REG_MMU_INT_CONTROL0);
			writel_relaxed(0xffffffff,
				   data->base +
				   REG_MMU_INT_MAIN_CONTROL);
		} else {
			writel_relaxed(0,
				   data->base +
				   REG_MMU_INT_CONTROL0);
			writel_relaxed(0,
				   data->base +
				   REG_MMU_INT_MAIN_CONTROL);
		}
		spin_unlock_irqrestore(&data->reg_lock, flags);

		if (++i >= total_iommu_cnt)
			return;  //do not while loop if m4ulist is destroyed
	}
}

static void mtk_iommu_isr_restart(struct timer_list *t)
{
	mtk_iommu_intr_modify_all(1);
	mtk_iommu_debug_reset();
}

static int mtk_iommu_isr_pause_timer_init(struct mtk_iommu_data *data)
{
	timer_setup(&data->iommu_isr_pause_timer, mtk_iommu_isr_restart, 0);
	return 0;
}

static int mtk_iommu_isr_pause(int delay, struct mtk_iommu_data *data)
{
	mtk_iommu_intr_modify_all(0); /* disable all intr */
	/* delay seconds */
	data->iommu_isr_pause_timer.expires = jiffies + delay * HZ;
	if (!timer_pending(&data->iommu_isr_pause_timer))
		add_timer(&data->iommu_isr_pause_timer);
	return 0;
}

static void mtk_iommu_isr_record(struct mtk_iommu_data *data)
{
	static int isr_cnt;
	static unsigned long first_jiffies;

	/* we allow one irq in 1s, or we will disable them after 5s. */
	if (!isr_cnt || time_after(jiffies, first_jiffies + isr_cnt * HZ)) {
		isr_cnt = 1;
		first_jiffies = jiffies;
	} else {
		isr_cnt++;
		if (isr_cnt >= 5) {
			/* 5 irqs come in 5s, too many ! */
			/* disable irq for a while, to avoid HWT timeout */
			mtk_iommu_isr_pause(10, data);
			isr_cnt = 0;
		}
	}
}
static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova);

static int __mau_dump_status(int m4u_id, int slave, int mau);

static irqreturn_t mtk_iommu_isr(int irq, void *dev_id)
{
	struct mtk_iommu_data *data = NULL;
	struct iommu_domain *domain;
	u32 int_state, int_state_l2, regval, int_id;
	unsigned long fault_iova, fault_pa;
	unsigned int fault_larb, fault_port;
	bool layer, write, is_vpu;
	int slave_id = 0, i, j, port_id;
	unsigned int m4uid, bankid = MTK_IOMMU_BANK_NODE_COUNT;
	phys_addr_t pa;
	unsigned long flags;
	int ret = 0, ret1 = 0;
	void __iomem *base = NULL;

#ifdef MTK_IOMMU_BANK_IRQ_SUPPORT
	pr_notice("%s, irq=%d\n", __func__, irq);
	for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++) {
		for (j = 0; j < MTK_IOMMU_BANK_NODE_COUNT; j++) {
			if (irq == mtk_irq_bank[i][j]) {
				m4uid = i;
				bankid = j;
				data = mtk_iommu_get_m4u_data(m4uid);
				if (!data) {
					pr_notice("%s, m4u:%u, bank:%u Invalid bank node\n",
						__func__, m4uid, bankid);
					return 0;
				}
				base = data->base_bank[bankid];
				break;
			}
		}
	}

	if (!data) {
		data = dev_id;
		if (!data) {
			pr_notice("%s, Invalid normal irq %d\n",
					__func__, irq);
			return 0;
		}
		m4uid = data->m4uid;
		bankid = MTK_IOMMU_BANK_NODE_COUNT;
		base = data->base;
	}

	if (!base || IS_ERR(base)) {
		pr_notice("%s, %d, invalid base addr of iommu:%u, bank:%u\n",
			  __func__, __LINE__, m4uid, bankid);
		return 0;
	}
#else
	data = dev_id;
	if (!data) {
		pr_notice("%s, Invalid normal irq %d\n",
				__func__, irq);
		return 0;
	}

	m4uid = data->m4uid;
	if (!data->base || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base addr\n",
			  __func__, __LINE__);
		return 0;
	}
	base = data->base;
#endif

#ifdef IOMMU_POWER_CLK_SUPPORT
	spin_lock_irqsave(&data->reg_lock, flags);
	if (!data->poweron) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return 0;
	}
	data->isr_ref++;
	spin_unlock_irqrestore(&data->reg_lock, flags);
#endif

	ret1 = mtk_switch_secure_debug_func(data->m4uid, 1);
	if (ret1)
		pr_notice("%s, %d, m4u:%u, failed to enable secure debug signal\n",
			  __func__, __LINE__, data->m4uid);

	/* Read error info from registers */
	int_state_l2 = readl_relaxed(base + REG_MMU_L2_FAULT_ST);
	int_state = readl_relaxed(base + REG_MMU_FAULT_ST1);

	if (!int_state_l2 && !int_state) {
		ret = 0;
		goto out;
	}

	pr_notice("iommu:%u, bank:%u, L2 int sta(0x130)=0x%x, main sta(0x134)=0x%x\n",
		  m4uid,
		  bankid == MTK_IOMMU_BANK_NODE_COUNT ? 0 : bankid + 1,
		  int_state_l2, int_state);
	if (int_state_l2 & F_INT_L2_MULTI_HIT_FAULT)
		MMU_INT_REPORT(m4uid, 0, F_INT_L2_MULTI_HIT_FAULT);

	if (int_state_l2 & F_INT_L2_TABLE_WALK_FAULT) {
		unsigned int layer;

		MMU_INT_REPORT(m4uid, 0, F_INT_L2_TABLE_WALK_FAULT);
		regval = readl_relaxed(base +
				REG_MMU_TBWALK_FAULT_VA);
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		fault_iova = ((unsigned long)regval & F_MMU_FAULT_VA_BIT31_12) |
				(((unsigned long)regval &
					F_MMU_FAULT_VA_BIT32) << 23);
#else
		fault_iova = (unsigned long)regval;
#endif
		layer = regval & 1;
		mmu_aee_print(
				"L2 table walk fault: iova=0x%lx, layer=%d\n",
				fault_iova, layer);
	}

	if (int_state_l2 & F_INT_L2_PFH_DMA_FIFO_OVERFLOW)
		MMU_INT_REPORT(m4uid, 0,
			F_INT_L2_PFH_DMA_FIFO_OVERFLOW);

	if (int_state_l2 & F_INT_L2_MISS_DMA_FIFO_OVERFLOW)
		MMU_INT_REPORT(m4uid, 0,
			F_INT_L2_MISS_DMA_FIFO_OVERFLOW);

	if (int_state_l2 & F_INT_L2_INVALID_DONE)
		MMU_INT_REPORT(m4uid, 0, F_INT_L2_INVALID_DONE);

	if (int_state_l2 & F_INT_L2_PFH_OUT_FIFO_ERROR)
		MMU_INT_REPORT(m4uid, 0,
			F_INT_L2_PFH_OUT_FIFO_ERROR);

	if (int_state_l2 & F_INT_L2_PFH_IN_FIFO_ERROR)
		MMU_INT_REPORT(m4uid, 0,
			F_INT_L2_PFH_IN_FIFO_ERROR);

	if (int_state_l2 & F_INT_L2_MISS_OUT_FIFO_ERROR)
		MMU_INT_REPORT(m4uid, 0,
			F_INT_L2_MISS_OUT_FIFO_ERROR);

	if (int_state_l2 & F_INT_L2_MISS_IN_FIFO_ERR)
		MMU_INT_REPORT(m4uid, 0, F_INT_L2_MISS_IN_FIFO_ERR);

	for (i = 0; i < MTK_MMU_NUM_OF_IOMMU(m4uid); i++) {
		if (int_state & (F_INT_MMU_MAIN_MSK(i) |
		    F_INT_MAIN_MAU_INT_EN(i))) {
			slave_id = i;
			break;
		}
	}
	if (i == MTK_IOMMU_MMU_COUNT) {
		pr_info("m4u interrupt error: status = 0x%x\n", int_state);
		iommu_set_field_by_mask(base, REG_MMU_INT_CONTROL0,
					F_INT_CTL0_INT_CLR,
					F_INT_CTL0_INT_CLR);
		ret = 0;
		goto out;
	}

	if (int_state & F_INT_TRANSLATION_FAULT(slave_id)) {
		int_id = readl_relaxed(base + REG_MMU_INT_ID(slave_id));
		port_id = mtk_iommu_get_larb_port(
				F_MMU_INT_TF_VAL(int_id),
				m4uid, &fault_larb,
				&fault_port);
		pr_notice("iommu:%d, slave:%d, port_id=%d(%d-%d), tf_id:0x%x\n",
			  m4uid, slave_id, port_id,
			  fault_larb, fault_port, int_id);

		if (port_id < 0) {
			WARN_ON(1);
			ret = 0;
			goto out;
		}
		/*pseudo_dump_port(port_id, true);*/

		regval = readl_relaxed(base +
					REG_MMU_FAULT_STATUS(slave_id));
		layer = regval & F_MMU_FAULT_VA_LAYER_BIT;
		write = regval & F_MMU_FAULT_VA_WRITE_BIT;
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		fault_iova = ((unsigned long)regval & F_MMU_FAULT_VA_BIT31_12) |
				(((unsigned long)regval &
					F_MMU_FAULT_VA_BIT32) << 23);
#else
		fault_iova = (unsigned long)(regval);
#endif
		pr_notice("%s, %d, fault_iova=%lx, regval=0x%x\n",
			  __func__, __LINE__, fault_iova, regval);

		domain = __mtk_iommu_get_domain(data,
					fault_larb, fault_port);
		if (!domain) {
			WARN_ON(1);
			ret = 0;
			goto out;
		}
		pa = mtk_iommu_iova_to_phys(domain, fault_iova & PAGE_MASK);

		fault_pa = readl_relaxed(base +
					REG_MMU_INVLD_PA(slave_id));
		fault_pa |= (unsigned long)(regval &
					F_MMU_FAULT_PA_BIT32) << 26;
		pr_notice("fault_pa=0x%lx, get pa=%x, tfrp=0x%x, ptbase=0x%x\n",
			  fault_pa, (unsigned int)pa,
			  readl_relaxed(base + REG_MMU_TFRP_PADDR),
			  readl_relaxed(base + REG_MMU_PT_BASE_ADDR));
#ifdef APU_IOMMU_INDEX
		if (m4uid >= APU_IOMMU_INDEX) {
			is_vpu = true;
		} else {
			is_vpu = false;
		}
#endif
		if (enable_custom_tf_report())
			report_custom_iommu_fault(m4uid,
						  base,
						  fault_iova,
						  fault_pa,
						  F_MMU_INT_TF_VAL(int_id),
						  is_vpu, false);

		if (report_iommu_fault(domain, data->dev, fault_iova,
					  write ? IOMMU_FAULT_WRITE :
					  IOMMU_FAULT_READ)) {
			dev_err_ratelimited(
				data->dev,
				"iommu fault type=0x%x iova=0x%lx pa=0x%lx larb=%d port=%d is_vpu=%d layer=%d %s\n",
				int_state, fault_iova, fault_pa,
				fault_larb, fault_port, is_vpu,
				layer, write ? "write" : "read");
		}
#ifdef MTK_IOMMU_BANK_IRQ_SUPPORT
		if (bankid < MTK_IOMMU_BANK_NODE_COUNT)
			mtk_iommu_atf_call(IOMMU_ATF_BANK_DUMP_INFO,
					m4uid, bankid + 1);
#endif
		m4u_dump_pgtable(1, fault_iova);
	}

	if (int_state &
	    F_INT_MAIN_MULTI_HIT_FAULT(slave_id)) {
		MMU_INT_REPORT(m4uid, slave_id,
			F_INT_MAIN_MULTI_HIT_FAULT(slave_id));
	}
	if (int_state &
	    F_INT_INVALID_PHYSICAL_ADDRESS_FAULT(slave_id)) {
		if (!(int_state &
		    F_INT_TRANSLATION_FAULT(slave_id))) {
			MMU_INT_REPORT(m4uid, slave_id,
				F_INT_INVALID_PHYSICAL_ADDRESS_FAULT(slave_id));

		}
	}
	if (int_state & F_INT_ENTRY_REPLACEMENT_FAULT(slave_id)) {
		MMU_INT_REPORT(m4uid, slave_id,
			F_INT_ENTRY_REPLACEMENT_FAULT(slave_id));
	}
	if (int_state & F_INT_TLB_MISS_FAULT(slave_id))
		MMU_INT_REPORT(m4uid, slave_id,
				F_INT_TLB_MISS_FAULT(slave_id));

	if (int_state & F_INT_MISS_FIFO_ERR(slave_id))
		MMU_INT_REPORT(m4uid, slave_id,
				F_INT_MISS_FIFO_ERR(slave_id));

	if (int_state & F_INT_PFH_FIFO_ERR(slave_id))
		MMU_INT_REPORT(m4uid, slave_id,
				F_INT_PFH_FIFO_ERR(slave_id));

	if (int_state & F_INT_MAIN_MAU_INT_EN(slave_id)) {
		MMU_INT_REPORT(m4uid, slave_id,
			F_INT_MAIN_MAU_INT_EN(slave_id));
		__mau_dump_status(m4uid, slave_id, 0);
	}

	/* Interrupt clear */
	regval = readl_relaxed(base + REG_MMU_INT_CONTROL0);
	regval |= F_INT_CTL0_INT_CLR;
	writel_relaxed(regval, base + REG_MMU_INT_CONTROL0);

	mtk_iommu_tlb_flush_all_lock(data, false);
	mtk_iommu_isr_record(data);

	ret = IRQ_HANDLED;

out:
	spin_lock_irqsave(&data->reg_lock, flags);
	data->isr_ref--;
	spin_unlock_irqrestore(&data->reg_lock, flags);

	ret1 = mtk_switch_secure_debug_func(data->m4uid, 0);
	if (ret1)
		pr_notice("%s, %d, m4u:%u, failed to disable secure debug signal\n",
			  __func__, __LINE__, data->m4uid);

	return ret;
}

#ifdef MTK_M4U_SECURE_IRQ_SUPPORT
static int mtk_irq_sec[MTK_IOMMU_M4U_COUNT];
irqreturn_t MTK_M4U_isr_sec(int irq, void *dev_id)
{
	struct mtk_iommu_data *data = NULL;
	size_t tf_port = 0, tf_iova = 0, tf_int = 0;
	unsigned int m4u_id = 0;
	int i, ret = 0;
	unsigned long flags, fault_iova;

	for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++) {
		if (irq == mtk_irq_sec[i]) {
			m4u_id = i;
			data = mtk_iommu_get_m4u_data(m4u_id);
			break;
		}
	}

	if (!data) {
		pr_notice("%s, Invalid secure irq %d\n",
				__func__, irq);
		return 0;
	}

	if (!data->base_sec || IS_ERR(data->base_sec) ||
	    !data->base || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base addr of iommu:%d\n",
			  __func__, __LINE__, m4u_id);
		return 0;
	}

#ifdef IOMMU_POWER_CLK_SUPPORT
	spin_lock_irqsave(&data->reg_lock, flags);
	if (!data->poweron) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return 0;
	}
	data->isr_ref++;
	spin_unlock_irqrestore(&data->reg_lock, flags);
#endif

	ret = __mtk_iommu_atf_call(IOMMU_ATF_DUMP_SECURE_REG,
			m4u_id, 4, &tf_port, &tf_iova, &tf_int);
	pr_notice("iommu:%d secure bank fault_id:0x%zx port:0x%zx in normal world!\n",
		  m4u_id, tf_int, tf_port);
	if (!ret && tf_port < M4U_PORT_UNKNOWN) {
		bool is_vpu;

		if (m4u_id >= APU_IOMMU_INDEX)
			is_vpu = true;
		else
			is_vpu = false;

#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		fault_iova = ((unsigned long)tf_iova &
				F_MMU_FAULT_VA_BIT31_12) |
				(((unsigned long)tf_iova &
				F_MMU_FAULT_VA_BIT32) << 23);
#else
		fault_iova = (unsigned long)(tf_iova);
#endif
		if (enable_custom_tf_report())
			report_custom_iommu_fault(m4u_id,
						  data->base,
						  fault_iova,
						  0,
						  F_MMU_INT_TF_VAL(tf_int),
						  is_vpu,
						  true);
	}

	ret = IRQ_HANDLED;

	spin_lock_irqsave(&data->reg_lock, flags);
	data->isr_ref--;
	spin_unlock_irqrestore(&data->reg_lock, flags);

	return ret;
}
#endif
static void mtk_iommu_config(struct mtk_iommu_data *data,
				 struct device *dev, bool enable)
{
#ifdef CONFIG_MTK_SMI_EXT
	struct mtk_smi_larb_iommu	*larb_mmu;
	unsigned int				 larbid, portid;
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	int i;

	for (i = 0; i < fwspec->num_ids; ++i) {
		larbid = MTK_IOMMU_TO_LARB(fwspec->ids[i]);
		portid = MTK_IOMMU_TO_PORT(fwspec->ids[i]);

		if (larbid >= MTK_LARB_NR_MAX) {
			WARN_ON(1);
			dev_notice(dev, "%d(%d) exceed the max larb ID\n",
				   larbid, fwspec->ids[i]);
			break;
		}
		larb_mmu = &data->smi_imu.larb_imu[larbid];

		dev_dbg(dev, "%s iommu port: %d\n",
			enable ? "enable" : "disable", portid);

		if (enable)
			larb_mmu->mmu |= MTK_SMI_MMU_EN(portid);
		else
			larb_mmu->mmu &= ~MTK_SMI_MMU_EN(portid);
	}
#endif
}

int __mtk_iommu_get_pgtable_base_addr(
		struct mtk_iommu_pgtable *pgtable,
		unsigned int *pgd_pa)
{
	if (!pgtable)
		pgtable = mtk_iommu_get_pgtable(NULL, 0);

	if (!pgtable) {
		pr_notice("%s, %d, cannot find pgtable\n",
			  __func__, __LINE__);
		return -1;
	}
	*pgd_pa = pgtable->cfg.arm_v7s_cfg.ttbr[0] & F_MMU_PT_BASE_ADDR_MSK;
	if (pgtable->cfg.arm_v7s_cfg.ttbr[1] <
	    (1 << (CONFIG_MTK_IOMMU_PGTABLE_EXT - 32))) {
		*pgd_pa |= pgtable->cfg.arm_v7s_cfg.ttbr[1] &
			  F_MMU_PT_BASE_ADDR_BIT32;
	} else {
		pr_notice("%s, %d, invalid pgtable base addr, 0x%x_%x\n",
			  __func__, __LINE__,
			  pgtable->cfg.arm_v7s_cfg.ttbr[1],
			  pgtable->cfg.arm_v7s_cfg.ttbr[0]);
		return -2;
	}

	return 0;
}

int mtk_iommu_get_pgtable_base_addr(unsigned long *pgd_pa)
{
	unsigned int pgd_reg_val = 0;
	int ret = 0;

	ret = __mtk_iommu_get_pgtable_base_addr(NULL, &pgd_reg_val);
	if (ret)
		return ret;

	*pgd_pa = ((unsigned long)(pgd_reg_val &
		   F_MMU_PT_BASE_ADDR_BIT32) << 32) |
		   (unsigned long)(pgd_reg_val &
		   F_MMU_PT_BASE_ADDR_MSK);

	return 0;
}

static int mtk_iommu_create_pgtable(struct mtk_iommu_data *data)
{
	struct mtk_iommu_pgtable *pgtable =
				mtk_iommu_get_pgtable(data, init_data_id);

	if (pgtable)
		return 0;

	pgtable = kzalloc(sizeof(*pgtable), GFP_KERNEL);
	if (!pgtable)
		return -ENOMEM;

	spin_lock_init(&pgtable->pgtlock);
	spin_lock_init(&pgtable->domain_lock);
	pgtable->domain_count = 0;
	INIT_LIST_HEAD(&pgtable->m4u_dom);

	pgtable->cfg = (struct io_pgtable_cfg) {
		.quirks = IO_PGTABLE_QUIRK_ARM_NS |
			IO_PGTABLE_QUIRK_NO_PERMS |
			IO_PGTABLE_QUIRK_TLBI_ON_MAP,
		.pgsize_bitmap = mtk_iommu_ops.pgsize_bitmap,
#if defined(MTK_IOVA_ADDR_BITS) && defined(MTK_PHYS_ADDR_BITS)
		.ias = MTK_IOVA_ADDR_BITS,
		.oas = MTK_PHYS_ADDR_BITS,
#else
		.ias = 32,
		.oas = 32,
#endif
		.tlb = &mtk_iommu_gather_ops,
		.iommu_dev = data->dev,
	};

	if (data->enable_4GB)
		pgtable->cfg.quirks |= IO_PGTABLE_QUIRK_ARM_MTK_4GB;

	pgtable->iop = alloc_io_pgtable_ops(ARM_V7S, &pgtable->cfg, data);
	if (!pgtable->iop) {
		dev_err(data->dev, "Failed to alloc io pgtable\n");
		return -EINVAL;
	}

	if (mtk_iommu_set_pgtable(data, init_data_id, pgtable)) {
		pr_notice("%s, failed to set pgtable\n", __func__);
		return -EFAULT;
	}

	pr_notice("%s, %d, create pgtable done\n",
		  __func__, __LINE__);
	return 0;
}

static int mtk_iommu_attach_pgtable(struct mtk_iommu_data *data,
			struct device *dev)
{
	struct mtk_iommu_pgtable *pgtable =
				mtk_iommu_get_pgtable(data, init_data_id);
	unsigned int regval = 0, ret;
	unsigned int pgd_pa_reg = 0;
	unsigned long flags;
#ifdef IOMMU_POWER_CLK_SUPPORT
	struct mtk_iommu_suspend_reg *reg = &data->reg;
#endif

	// create pgtable
	if (!pgtable) {
		ret = mtk_iommu_create_pgtable(data);
		if (ret) {
			pr_notice("%s, %d, failed to create pgtable, err %d\n",
				  __func__, __LINE__, ret);
			return ret;
		}
		pgtable = mtk_iommu_get_pgtable(data, init_data_id);
	}

	// binding to pgtable
	data->pgtable = pgtable;

	// update HW settings
	if (__mtk_iommu_get_pgtable_base_addr(pgtable, &pgd_pa_reg))
		return -EFAULT;

	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (data->poweron) {
		writel(pgd_pa_reg, data->base + REG_MMU_PT_BASE_ADDR);
		regval = readl_relaxed(data->base + REG_MMU_PT_BASE_ADDR);
		pr_notice("%s, %d, iommu:%d config pgtable base addr=0x%x, quiks=0x%lx\n",
			  __func__, __LINE__, data->m4uid,
			  regval, pgtable->cfg.quirks);
		spin_unlock_irqrestore(&data->reg_lock, flags);
	} else {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		reg->pt_base = pgd_pa_reg;
		pr_notice("%s, %d, iommu:%d backup pgtable base addr=0x%x, quiks=0x%lx\n",
			  __func__, __LINE__, data->m4uid,
			  reg->pt_base, pgtable->cfg.quirks);
	}
#else
	writel(pgd_pa_reg, data->base + REG_MMU_PT_BASE_ADDR);
	regval = readl_relaxed(data->base + REG_MMU_PT_BASE_ADDR);
	pr_notice("%s, %d, iommu:%d config pgtable base addr=0x%x, quiks=0x%lx\n",
		  __func__, __LINE__, data->m4uid,
		  regval, pgtable->cfg.quirks);
	spin_unlock_irqrestore(&data->reg_lock, flags);
#endif

	return 0;
}

#ifndef CONFIG_ARM64
static int mtk_extend_iommu_mapping(struct dma_iommu_mapping *mapping)
{
	int next_bitmap;

	if (mapping->nr_bitmaps >= mapping->extensions) {
		pr_notice("%s, %d, err nr:0x%x > externsions:0x%x\n",
			  __func__, __LINE__,
			  mapping->nr_bitmaps, mapping->extensions);
		return -EINVAL;
	}

	next_bitmap = mapping->nr_bitmaps;
	mapping->bitmaps[next_bitmap] = kzalloc(mapping->bitmap_size,
						GFP_ATOMIC);
	if (!mapping->bitmaps[next_bitmap])
		return -ENOMEM;

	mapping->nr_bitmaps++;

	return 0;
}

static inline int mtk_do_reserve_iova(
				struct dma_iommu_mapping *mapping,
				dma_addr_t iova,
				size_t size, unsigned int pg_off)
{
	unsigned long count, start;
	unsigned long flags;
	int i, sbitmap, ebitmap;

	if (iova < mapping->base) {
		pr_notice("%s, %d, err iova:0x%x < base:0x%x\n",
			  __func__, __LINE__, iova, mapping->base);
		return -EINVAL;
	}

	start = (iova - mapping->base) >> pg_off;
	count = PAGE_ALIGN(size) >> pg_off;

	sbitmap = start / mapping->bits;
	ebitmap = (start + count) / mapping->bits;
	start = start % mapping->bits;

	if (ebitmap > mapping->extensions) {
		pr_notice("%s, %d, err end:0x%x > extensions:0x%x\n",
			  __func__, __LINE__, ebitmap,
			  mapping->extensions);
		return -EINVAL;
	}

	spin_lock_irqsave(&mapping->lock, flags);

	for (i = mapping->nr_bitmaps; i <= ebitmap; i++) {
		if (mtk_extend_iommu_mapping(mapping)) {
			pr_notice("%s, %d, err extend\n",
				  __func__, __LINE__);
			spin_unlock_irqrestore(&mapping->lock, flags);
			return -ENOMEM;
		}
	}

	for (i = sbitmap; count && i < mapping->nr_bitmaps; i++) {
		int bits = count;

		if (bits + start > mapping->bits)
			bits = mapping->bits - start;

		bitmap_set(mapping->bitmaps[i], start, bits);
		start = 0;
		count -= bits;
	}

	spin_unlock_irqrestore(&mapping->lock, flags);

	return 0;
}

static int mtk_iova_reserve_iommu_regions(struct mtk_iommu_domain *dom,
			struct device *dev)
{
	struct iommu_resv_region *region;
	LIST_HEAD(resv_regions);
	unsigned int pg_size, pg_off;
	struct iommu_domain *domain = &dom->domain;
	struct dma_iommu_mapping *mapping = dom->mapping;
	int ret = 0;

	if (dom->resv_status)
		return 0;

	if (!domain->ops->pgsize_bitmap) {
		WARN_ON(1);
		pg_off = PAGE_SHIFT;
	} else {
		pg_off = __ffs(domain->ops->pgsize_bitmap);
	}
	pg_size = 1UL << pg_off;
	iommu_get_resv_regions(dev, &resv_regions);

	/* We need to consider overlapping regions for different devices */
	list_for_each_entry(region, &resv_regions, list) {
		dma_addr_t start, end, addr;

		start = ALIGN(region->start, pg_size);
		end   = ALIGN(region->start + region->length, pg_size);

		for (addr = start; addr < end; addr += pg_size) {
			phys_addr_t phys_addr;

			phys_addr = iommu_iova_to_phys(domain, addr);
			if (phys_addr)
				continue;

			ret = iommu_map(domain, addr, addr,
					pg_size, region->prot);
			if (ret)
				goto out;
		}

		ret = mtk_do_reserve_iova(mapping, start, end - start, pg_off);
		if (ret != 0) {
			pr_notice("%s, %d, err reserve (0x%llx+0x%lx) in the mapping of group %d, pg_off=0x%x\n",
				  __func__, __LINE__, region->start,
				  region->length, dom->id, pg_off);
			goto out;
		} else {
			dom->resv_status = 1;
			pr_notice("%s, %d, finish reserve (0x%llx+0x%lx) in the mapping of group %d, pg_off=0x%x\n",
				  __func__, __LINE__, region->start,
				  region->length, dom->id, pg_off);
		}
	}

out:
	iommu_put_resv_regions(dev, &resv_regions);

	return ret;
}

				   // struct of_phandle_args *args)
static int mtk_iommu_create_mapping(struct device *dev)
{
	struct dma_iommu_mapping *mapping;
	unsigned long start, end, size;
	int ret = 0;
	struct mtk_iommu_domain *dom;

	dom = __mtk_iommu_get_mtk_domain(dev);
	if (!dom) {
		pr_notice("%s, %d, err domain\n",
			  __func__, __LINE__);
		return -ENODEV;
	}

	mapping = dom->mapping;
	if (!mapping) {
		start = max_t(unsigned long, SZ_4K,
			mtk_domain_array[dom->id].min_iova);
		end = min_t(unsigned long,
			DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT),
			mtk_domain_array[dom->id].max_iova);
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		if (start >> 32 != end >> 32 ||
		    start >> 32 != mtk_domain_array[dom->id].boundary) {
			pr_notice("%s, %d, err start:0x%lx, end:0x%lx, boundary:%d\n",
				  __func__, __LINE__, start, end,
				  mtk_domain_array[dom->id].boundary);
			return -EINVAL;
		}
#endif
		size = end - start + 1;
		if (size < 0 || size > DMA_BIT_MASK(
		    CONFIG_MTK_IOMMU_PGTABLE_EXT)) {
			pr_notice("%s, %d, err domain size 0x%x\n",
				  __func__, __LINE__, size);
			return -EINVAL;
		}
		mapping = arm_iommu_create_mapping(&platform_bus_type,
						start, size);
		if (IS_ERR(mapping)) {
			pr_notice("%s, %d, err mapping\n",
				  __func__, __LINE__);
			return -ENOMEM;
		}

		dom->mapping = mapping;
		dev_notice(dev, "%s, %d, create mapping for group %d, start:0x%x, size:0x%x\n",
				__func__, __LINE__, dom->id, start, size);
	}

	ret = arm_iommu_attach_device(dev, mapping);
	if (ret) {
		dev_notice(dev, "%s, %d, failed to attach to mapping of group %d\n",
			__func__, __LINE__, dom->id);
		goto err_release_mapping;
	}

	return 0;

err_release_mapping:
	arm_iommu_release_mapping(mapping);

	return ret;
}
#endif

static struct iommu_domain *mtk_iommu_domain_alloc(unsigned int type)
{
	struct mtk_iommu_domain *dom;
	struct mtk_iommu_pgtable *pgtable =
				mtk_iommu_get_pgtable(NULL, init_data_id);
	unsigned int id;
#ifdef CONFIG_ARM64
	// allocated at device_group for IOVA  space management by iovad
	unsigned int domain_type = IOMMU_DOMAIN_DMA;
#else
	// allocated at create mapping for IOVA space management by mapping
	unsigned int domain_type = IOMMU_DOMAIN_UNMANAGED;
#endif

	if (!pgtable) {
		pr_notice("%s, %d, err pgtabe of iommu%d\n",
			  __func__, __LINE__, init_data_id);
		return NULL;
	}

	if (type != domain_type) {
		pr_notice("%s, %d, err type%d\n",
			  __func__, __LINE__, type);
		return NULL;
	}

	id = pgtable->init_domain_id;
	list_for_each_entry(dom, &pgtable->m4u_dom, list) {
		if (dom->id == id)
			return &dom->domain;
	}

	return NULL;
}

static void mtk_iommu_domain_free(struct iommu_domain *domain)
{
	unsigned long flags;
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;

	pr_notice("%s, %d, domain_count=%d, free the %d domain\n",
		  __func__, __LINE__, pgtable->domain_count, dom->id);

#ifdef CONFIG_ARM64
	iommu_put_dma_cookie(domain);
#else
	arm_iommu_release_mapping(dom->mapping);
#endif
	kfree(dom);

	spin_lock_irqsave(&pgtable->domain_lock, flags);
	pgtable->domain_count--;
	if (pgtable->domain_count > 0) {
		spin_unlock_irqrestore(&pgtable->domain_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&pgtable->domain_lock, flags);
	free_io_pgtable_ops(pgtable->iop);
	kfree(pgtable);
}

static int mtk_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;
#if 0
	ifndef CONFIG_ARM64 case but not required for now.
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
#endif

	if (!data)
		return -ENODEV;

	mtk_iommu_config(data, dev, true);
#if 0
	ifndef CONFIG_ARM64 case but not require for now.
	/* reserve IOVA region after pgTable ready */
	mtk_iova_reserve_iommu_regions(dom, dev);
#endif
	return 0;
}

static void mtk_iommu_detach_device(struct iommu_domain *domain,
				struct device *dev)
{
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;

	if (!data)
		return;

	mtk_iommu_config(data, dev, false);
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	unsigned long flags;
	int ret = 0;

#ifdef IOMMU_DEBUG_ENABLED
	if (g_tf_test)
		return 0;
#endif

	spin_lock_irqsave(&pgtable->pgtlock, flags);
	ret = pgtable->iop->map(pgtable->iop, iova, paddr, size, prot);
	spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	return ret;
}

static size_t mtk_iommu_unmap(struct iommu_domain *domain,
			unsigned long iova, size_t size)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	unsigned long flags;
	size_t unmapsz;

#ifdef IOMMU_DEBUG_ENABLED
	if (g_tf_test)
		return size;
#endif

	spin_lock_irqsave(&pgtable->pgtlock, flags);
	unmapsz = pgtable->iop->unmap(pgtable->iop, iova, size);
	spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	return unmapsz;
}

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	unsigned long flags;
	phys_addr_t pa;

	spin_lock_irqsave(&pgtable->pgtlock, flags);
	pa = pgtable->iop->iova_to_phys(pgtable->iop, iova);
	spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	return pa;
}

int mtk_iommu_switch_acp(struct device *dev,
			  unsigned long iova, size_t size, bool is_acp)
{
	struct iommu_domain *domain = iommu_get_domain_for_dev(dev);
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&pgtable->pgtlock, flags);
	ret = pgtable->iop->switch_acp(pgtable->iop, iova, size, is_acp);
#ifdef MTK_IOMMU_PERFORMANCE_IMPROVEMENT
	mtk_iommu_tlb_add_flush_nosync(iova, size, 0, 0, dom);
	mtk_iommu_tlb_sync(dom);
#endif
	spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	if (ret)
		dev_notice(dev, "%s, %d, failed to switch acp, iova:0x%lx, size:0x%lx, acp:%d\n",
			  __func__, __LINE__, iova, size, is_acp);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_iommu_switch_acp);

static struct iommu_group *mtk_iommu_create_iova_space(
			const struct mtk_iommu_data *data, struct device *dev)
{
	struct mtk_iommu_pgtable *pgtable =
				mtk_iommu_get_pgtable(data, init_data_id);
	struct mtk_iommu_domain *dom;
	struct iommu_group *group;
	unsigned long flags, start, end;
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	unsigned int boundary;
#endif

	if (!pgtable) {
		pr_notice("%s, %d, err pgtable of iommu%d\n",
			  __func__, __LINE__, init_data_id);
		return NULL;
	}
	group = mtk_iommu_get_group(dev);

	if (group) {
		iommu_group_ref_get(group);
		return group;
	}

	// init mtk_iommu_domain
	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom)
		return NULL;

	// init iommu_group
	group = iommu_group_alloc();
	if (IS_ERR(group)) {
		dev_notice(dev, "Failed to allocate M4U IOMMU group\n");
		goto free_dom;
	}
	dom->group = group;

	dom->id = mtk_iommu_get_domain_id(dev);
	if (dom->id >= MTK_IOVA_DOMAIN_COUNT) {
		dev_notice(dev, "%s, %d, invalid iommu device, dom id = %d\n",
			  __func__, __LINE__, dom->id);
		goto free_group;
	}

	spin_lock_irqsave(&pgtable->domain_lock, flags);
	if (pgtable->domain_count >= MTK_IOVA_DOMAIN_COUNT) {
		spin_unlock_irqrestore(&pgtable->domain_lock, flags);
		pr_notice("%s, %d, too many domain, count=%d\n",
			  __func__, __LINE__, pgtable->domain_count);
		goto free_group;
	}
	pgtable->init_domain_id = dom->id;
	pgtable->domain_count++;
	spin_unlock_irqrestore(&pgtable->domain_lock, flags);

	dom->domain.pgsize_bitmap = pgtable->cfg.pgsize_bitmap;
	dom->pgtable = pgtable;
	list_add_tail(&dom->list, &pgtable->m4u_dom);
#if !MTK_IOMMU_PAGE_TABLE_SHARE
	dom->data = data;
#endif

#ifdef CONFIG_ARM64
	// init mtk_iommu_domain
	if (iommu_get_dma_cookie(&dom->domain))
		goto free_group;

	start = mtk_domain_array[dom->id].min_iova;
	end = mtk_domain_array[dom->id].max_iova;
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	boundary = mtk_domain_array[dom->id].boundary;
	if (start >> 32 != end >> 32 ||
	    start >> 32 != boundary) {
		pr_notice("%s, %d, err start:0x%lx, end:0x%lx, boundary:%d\n",
			  __func__, __LINE__, start, end, boundary);
		goto free_group;
	}
#endif
	dom->domain.geometry.aperture_start = start;
	dom->domain.geometry.aperture_end = end;
	dom->domain.geometry.force_aperture = true;
#else
	dom->resv_status = 0;
#endif
	dom->owner = mtk_domain_array[dom->id].owner;
#ifdef IOMMU_DEBUG_ENABLED
	pr_notice("%s, %d, dev:%s allocated IOVA group%d:%p, domain%d:%p owner:%d start:0x%llx end:0x%llx ext=%d\n",
		  __func__, __LINE__, dev_name(dev),
		  iommu_group_id(group),
		  group, dom->id, &dom->domain,
		  dom->owner,
		  dom->domain.geometry.aperture_start,
		  dom->domain.geometry.aperture_end,
		  CONFIG_MTK_IOMMU_PGTABLE_EXT);
#endif
	return group;

free_group:
	kfree(group);

free_dom:
	kfree(dom);
	return NULL;
}

static struct iommu_group *mtk_iommu_device_group(struct device *dev)
{
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;
	struct mtk_iommu_pgtable *pgtable;
	int ret = 0;

	if (!data)
		return NULL;

	init_data_id = data->m4uid;
	pgtable = data->pgtable;
	if (!pgtable) {
		ret = mtk_iommu_attach_pgtable(data, dev);
		if (ret) {
			data->pgtable = NULL;
			return NULL;
		}
	}

	return mtk_iommu_create_iova_space(data, dev);
}

#ifdef CONFIG_ARM64
static int mtk_iommu_add_device(struct device *dev)
{
	struct mtk_iommu_data *data;
	struct iommu_group *group;

	if (!dev->iommu_fwspec ||
	    dev->iommu_fwspec->ops != &mtk_iommu_ops) {
		return -ENODEV;
	}

	data = dev->iommu_fwspec->iommu_priv;
	iommu_device_link(&data->iommu, dev);

	group = iommu_group_get_for_dev(dev);
	if (IS_ERR(group)) {
		dev_notice(dev, "%s, %d, invalid group\n", __func__, __LINE__);
		return PTR_ERR(group);
	}

	iommu_group_put(group);
	return 0;
}
#else
static int mtk_iommu_add_device(struct device *dev)
{
	struct of_phandle_args iommu_spec;
	struct mtk_iommu_data *data;
	struct iommu_group *group;
	int idx = 0, ret = 0;

	if (!dev->iommu_fwspec ||
	    dev->iommu_fwspec->ops != &mtk_iommu_ops) {
		return -ENODEV; /* Not a iommu client device */
	}

	// create group, domain
	group = mtk_iommu_device_group(dev);
	if (IS_ERR(group)) {
		dev_notice(dev, "%s, %d, Failed to allocate M4U IOMMU group\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	// attach the device to domain before access it when create mapping
	ret = iommu_group_add_device(group, dev);
	iommu_group_put(group);
	pr_notice("%s, %d\n", __func__, __LINE__);

	if (ret < 0) {
		dev_notice(dev, "%s, %d, Failed to add device to IPMMU group\n",
			__func__, __LINE__);
		iommu_group_remove_device(dev);
		group = NULL;
		return ret;
	}

	// create mappings
	while (!of_parse_phandle_with_args(dev->of_node, "iommus",
					   "#iommu-cells", idx,
					   &iommu_spec)) {
		mtk_iommu_create_mapping(dev);
		of_node_put(iommu_spec.np);
		idx++;
	}

	if (!idx) {
		pr_notice("%s, %d invalid idx:%d\n",
			  __func__, __LINE__, idx);
		return -ENODEV;
	}

	data = dev->iommu_fwspec->iommu_priv;
	iommu_device_link(&data->iommu, dev);

	return 0;
}

#endif
static void mtk_iommu_remove_device(struct device *dev)
{
	struct mtk_iommu_data *data;

	if (!dev->iommu_fwspec || dev->iommu_fwspec->ops != &mtk_iommu_ops)
		return;

	data = dev->iommu_fwspec->iommu_priv;
	iommu_device_unlink(&data->iommu, dev);

	iommu_group_remove_device(dev);
	iommu_fwspec_free(dev);
}

static int mtk_iommu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	struct platform_device *m4updev;

	if (args->args_count != 1) {
		dev_err(dev, "invalid #iommu-cells(%d) property for IOMMU\n",
			args->args_count);
		return -EINVAL;
	}

	if (!dev->iommu_fwspec->iommu_priv) {
		/* Get the m4u device */
		m4updev = of_find_device_by_node(args->np);
		of_node_put(args->np);
		if (!m4updev) {
			WARN_ON(1);
			return -EINVAL;
		}

		dev->iommu_fwspec->iommu_priv = platform_get_drvdata(m4updev);
	}
	return iommu_fwspec_add_ids(dev, args->args, 1);
}

static void mtk_iommu_get_resv_region(
					struct device *dev,
					struct list_head *list)
{
	struct iommu_resv_region *region;
	const struct mtk_iova_domain_data *dom_data;
	struct mtk_iommu_domain *dom;
	unsigned int i;

	dom = __mtk_iommu_get_mtk_domain(dev);
	if (!dom) {
		WARN_ON(1);
		return;
	}

	dom_data = &mtk_domain_array[dom->id];
	switch (dom_data->resv_type) {
	case IOVA_REGION_REMOVE:
		for (i = 0; i < MTK_IOVA_REMOVE_CNT; i++) {
			if (!dom_data->resv_size[i])
				continue;

			region = iommu_alloc_resv_region(
					 dom_data->resv_start[i],
					 dom_data->resv_size[i],
					 0, IOMMU_RESV_RESERVED);
			if (!region) {
				pr_notice("Out of memory allocating dm-regions for %s\n",
					  dev_name(dev));
				return;
			}
			list_add_tail(&region->list, list);
		}
		break;
	case IOVA_REGION_STAY:
		for (i = 0; i < MTK_IOVA_REMOVE_CNT; i++) {
			if (!dom_data->resv_size[i])
				continue;

			if (dom_data->resv_start[i] != 0) {
				region = iommu_alloc_resv_region(0x0,
						 dom_data->resv_start[i],
						 0, IOMMU_RESV_RESERVED);
				if (!region) {
					pr_notice("Out of memory allocating dm-regions for %s\n",
						  dev_name(dev));
					return;
				}
				list_add_tail(&region->list, list);
			}
			region = iommu_alloc_resv_region(
					 dom_data->resv_start[i] +
					 dom_data->resv_size[i],
					 DMA_BIT_MASK(32) -
					 dom_data->resv_start[i] -
					 dom_data->resv_size[i] + 1,
					 0, IOMMU_RESV_RESERVED);
			if (!region) {
				pr_notice("Out of memory allocating dm-regions for %s\n",
					  dev_name(dev));
				return;
			}
			list_add_tail(&region->list, list);
		}
		break;
	default:
		break;
	}
}

static void mtk_iommu_put_resv_region(
					struct device *dev,
					struct list_head *list)
{
	struct  iommu_resv_region *region, *tmp;

	list_for_each_entry_safe(region, tmp, list, list)
		kfree(region);
}

/*
 * func: get the IOVA space of target device
 * dev: user input the target device
 * base: return the start addr of IOVA space
 * max: return the end addr of IOVA space
 * list: return the reserved region of IOVA space
 *       check the usage of struct iommu_resv_region
 */
int mtk_iommu_get_iova_space(struct device *dev,
		unsigned long *base, unsigned long *max,
		int *owner, struct list_head *list)
{
	struct mtk_iommu_domain *dom;
	struct mtk_iommu_pgtable *pgtable = mtk_iommu_get_pgtable(NULL, 0);
	unsigned long flags = 0;

	if (!pgtable)
		pr_notice("%s, invalid pgtable\n", __func__);

	dom = __mtk_iommu_get_mtk_domain(dev);
	if (dom)
		*owner = dom->owner;
	else
		*owner = -1;

	if (pgtable)
		spin_lock_irqsave(&pgtable->pgtlock, flags);
	iommu_dma_get_iovad_info(dev, base, max);
	if (pgtable)
		spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	if (list)
		iommu_get_resv_regions(dev, list);

	return mtk_iommu_get_domain_id(dev);
}

/*
 * func: free the memory allocated by mtk_iommu_get_iova_space()
 * list: user input the reserved region list returned by
 *       mtk_iommu_get_iova_space()
 */
void mtk_iommu_put_iova_space(struct device *dev,
		struct list_head *list)
{
	struct  iommu_resv_region *region, *tmp;

	list_for_each_entry_safe(region, tmp, list, list)
		kfree(region);
}

static struct iommu_ops mtk_iommu_ops = {
	.domain_alloc = mtk_iommu_domain_alloc,
	.domain_free = mtk_iommu_domain_free,
	.attach_dev = mtk_iommu_attach_device,
	.detach_dev = mtk_iommu_detach_device,
	.map = mtk_iommu_map,
	.unmap = mtk_iommu_unmap,
	.map_sg = default_iommu_map_sg,
	.flush_iotlb_all = mtk_iommu_iotlb_flush_all,
	.iotlb_range_add = mtk_iommu_iotlb_range_add,
	.iotlb_sync = mtk_iommu_iotlb_sync,
	.iova_to_phys = mtk_iommu_iova_to_phys,
	.add_device = mtk_iommu_add_device,
	.remove_device = mtk_iommu_remove_device,
	.device_group = mtk_iommu_device_group,
	.of_xlate = mtk_iommu_of_xlate,
	.pgsize_bitmap = SZ_4K | SZ_64K | SZ_1M | SZ_16M,
	.get_resv_regions = mtk_iommu_get_resv_region,
	.put_resv_regions = mtk_iommu_put_resv_region,
};

unsigned int mtk_get_main_descriptor(const struct mtk_iommu_data *data,
		int m4u_slave_id, int idx)
{
	unsigned int regValue = 0;
	void __iomem *base = data->base;
	u32 tmp = 0;
	int ret = 0;

	regValue = F_READ_ENTRY_EN
		   | F_READ_ENTRY_MMx_MAIN(m4u_slave_id)
		   | F_READ_ENTRY_MAIN_IDX(m4u_slave_id, idx);

	writel_relaxed(regValue,
		   base + REG_MMU_READ_ENTRY);
	ret = readl_poll_timeout_atomic(base +
					REG_MMU_READ_ENTRY, tmp,
					(tmp & F_READ_ENTRY_EN) == 0,
					10, 1000);
	if (ret) {
		dev_notice(data->dev, "iommu:%d polling timeout\n",
			   data->m4uid);
		return 0;
	}

	return readl_relaxed(base + REG_MMU_DES_RDATA);
}

unsigned int mtk_get_main_tag(const struct mtk_iommu_data *data,
		int m4u_slave_id, int idx)
{
	void __iomem *base = data->base;

	return readl_relaxed(base + REG_MMU_MAIN_TAG(m4u_slave_id, idx));
}

static unsigned long imu_main_tag_to_va(unsigned int tag)
{
	unsigned long tmp;

	tmp = ((unsigned long)tag & F_MAIN_TLB_VA_MSK) |
	      (((unsigned long)tag & F_MAIN_TLB_VA_BIT32) << 32);

	return tmp;
}

void mtk_get_main_tlb(const struct mtk_iommu_data *data,
		int m4u_slave_id, int idx,
		struct mmu_tlb_t *pTlb)
{
	pTlb->tag = mtk_get_main_tag(data, m4u_slave_id, idx);
	pTlb->desc = mtk_get_main_descriptor(data, m4u_slave_id, idx);
}

unsigned int mtk_get_pfh_tlb(const struct mtk_iommu_data *data,
		int set, int page, int way, struct mmu_tlb_t *pTlb)
{
	unsigned int regValue = 0;
	void __iomem *base = data->base;
	u32 tmp = 0;
	int ret = 0;

	regValue = F_READ_ENTRY_EN
		   | F_READ_ENTRY_PFH
		   | F_READ_ENTRY_PFH_IDX(set)
		   | F_READ_ENTRY_PFH_PAGE_IDX(page)
		   | F_READ_ENTRY_PFH_WAY(way);

	writel_relaxed(regValue,
		   base + REG_MMU_READ_ENTRY);
	ret = readl_poll_timeout_atomic(base +
					REG_MMU_READ_ENTRY, tmp,
					(tmp & F_READ_ENTRY_EN) == 0,
					10, 1000);
	if (ret) {
		dev_notice(data->dev, "iommu:%d polling timeout\n",
			    data->m4uid);
		pTlb->desc = 0;
		pTlb->tag = 0;
		return 0;
	}
	pTlb->desc = readl_relaxed(base + REG_MMU_DES_RDATA);
	pTlb->tag = readl_relaxed(base + REG_MMU_PFH_TAG_RDATA);

	return 0;
}

unsigned int mtk_get_pfh_tag(
		const struct mtk_iommu_data *data,
		int set, int page, int way)
{
	struct mmu_tlb_t tlb;

	mtk_get_pfh_tlb(data, set, page, way, &tlb);
	return tlb.tag;
}

unsigned int mtk_get_pfh_descriptor(
		const struct mtk_iommu_data *data,
		int set, int page, int way)
{
	struct mmu_tlb_t tlb;

	mtk_get_pfh_tlb(data, set, page, way, &tlb);
	return tlb.desc;
}

int mtk_dump_main_tlb(int m4u_id, int m4u_slave_id,
		struct seq_file *s)
{
	/* M4U related */
	unsigned int i = 0;
	struct mmu_tlb_t tlb;
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);
	unsigned long flags;
	int ret;

	if (!data)
		return 0;

	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		mmu_seq_print(s,
			       "iommu:%d power off\n", m4u_id);
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return 0;
	}
#endif

	ret = mtk_switch_secure_debug_func(m4u_id, 1);
	if (ret) {
		mmu_seq_print(s,
				"%s, failed to enable secure debug signal\n",
			  __func__);
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return 0;
	}

	mmu_seq_print(s,
		       "==== main tlb iommu%d mmu%d ====\n",
		       m4u_id, m4u_slave_id);
	for (i = 0; i < g_tag_count[m4u_id]; i++) {
		mtk_get_main_tlb(data, m4u_slave_id, i, &tlb);
		mmu_seq_print(s,
			       "%d v:%d va:0x%lx bank%d layer%d sec:%d <0x%x-0x%x>\n",
				   i, !!(tlb.tag & F_MAIN_TLB_VALID_BIT),
			       imu_main_tag_to_va(tlb.tag),
			       F_MAIN_TLB_TABLE_ID_BIT(tlb.tag),
				   !!(tlb.tag & F_MAIN_TLB_LAYER_BIT),
				   !!(tlb.tag & F_MAIN_TLB_SEC_BIT),
			       tlb.tag, tlb.desc);
	}

	ret = mtk_switch_secure_debug_func(m4u_id, 0);
	if (ret)
		mmu_seq_print(s,
				"%s, failed to disable secure debug signal\n",
			  __func__);


	spin_unlock_irqrestore(&data->reg_lock, flags);

	return 0;
}

#if 0
int mtk_dump_valid_main0_tlb(
	const struct mtk_iommu_data *data, int m4u_slave_id)
{
	unsigned int i = 0;
	struct mmu_tlb_t tlb;

	pr_notice("dump main tlb start %d -- %d\n", data->m4uid, m4u_slave_id);
	for (i = 0; i < g_tag_count[data->m4uid]; i++) {
		mtk_get_main_tlb(data, m4u_slave_id, i, &tlb);
		if ((tlb.tag & F_MAIN_TLB_VALID_BIT) == F_MAIN_TLB_VALID_BIT)
			pr_info("%d:0x%x:0x%x\n", i, tlb.tag, tlb.desc);

	}
	pr_notice("dump inv main tlb end\n");

	return 0;
}
#endif

int dump_fault_mva_pfh_tlb(const struct mtk_iommu_data *data, unsigned int mva)
{
	int set;
	int way, page, valid;
	struct mmu_tlb_t tlb;
	unsigned int regval;
	void __iomem *base = data->base;

	set = (mva >> 15) & 0x7f;
	for (way = 0; way < MTK_IOMMU_WAY_NR; way++) {
		for (page = 0; page < MMU_PAGE_PER_LINE; page++) {
			regval = readl_relaxed(base +
				REG_MMU_PFH_VLD(set, way));
			valid = !!(regval & F_MMU_PFH_VLD_BIT(set, way));

			mtk_get_pfh_tlb(data, set, page, way, &tlb);
			pr_notice(
				  "fault_mva:0x%x, way:%d, set:%d, page:%d, valid:%d--0x%x, tag:0x%x, des:0x%x\n",
				  mva, way, set, page, valid,
				  regval, tlb.tag, tlb.desc);
		}
	}

	return 0;
}

static unsigned long imu_pfh_tag_to_va(int mmu,
		int set, int way, unsigned int tag)
{
	unsigned long tmp;

	tmp = F_PFH_TAG_VA_GET(mmu, tag);
	if (tag & F_PFH_TAG_LAYER_BIT)
		tmp |= ((set) << 15);
	else {
		//tmp &= F_MMU_PFH_TAG_VA_LAYER0_MSK(mmu);
		tmp |= (set) << 23;
	}

	return tmp;
}

int mtk_dump_pfh_tlb(int m4u_id,
		struct seq_file *s)
{
	unsigned int regval;
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);
	void __iomem *base;
	int result = 0;
	int set_nr, way_nr, set, way;
	int valid;
	unsigned long flags;
	int ret;

	if (!data)
		return 0;

	base = data->base;
	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		mmu_seq_print(s,
			       "iommu:%d power off\n", m4u_id);
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return 0;
	}
#endif

	ret = mtk_switch_secure_debug_func(m4u_id, 1);
	if (ret) {
		mmu_seq_print(s,
				"%s, failed to enable secure debug signal\n",
			  __func__);
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return 0;
	}

	set_nr = MTK_IOMMU_SET_NR(m4u_id);
	way_nr = MTK_IOMMU_WAY_NR;

	mmu_seq_print(s,
		       "==== prefetch tlb iommu%d  ====\n", m4u_id);

	for (way = 0; way < way_nr; way++) {
		for (set = 0; set < set_nr; set++) {
			int page;
			struct mmu_tlb_t tlb;

			regval = readl_relaxed(base +
				REG_MMU_PFH_VLD(set, way));
			valid = !!(regval & F_MMU_PFH_VLD_BIT(set, way));
			mtk_get_pfh_tlb(data, set, 0, way, &tlb);
			mmu_seq_print(s,
				       "%d-%d v:%d va:0x%lx layer%d bank%d sec:%d pfh:%d tag:0x%x <0x%x ",
			 way, set, valid,
			 imu_pfh_tag_to_va(m4u_id, set, way, tlb.tag),
			 !!(tlb.tag & F_PFH_TAG_LAYER_BIT),
			 (tlb.tag & F_PFH_PT_BANK_BIT),
			 !!(tlb.tag & F_PFH_TAG_SEC_BIT),
			 !!(tlb.tag & F_PFH_TAG_AUTO_PFH),
			 tlb.tag, tlb.desc);

			for (page = 1; page < MMU_PAGE_PER_LINE; page++) {
				mtk_get_pfh_tlb(data, set, page, way, &tlb);
				mmu_seq_print(s, "0x%x ", tlb.desc);
			}
			mmu_seq_print(s, ">\n");
		}
	}

	ret = mtk_switch_secure_debug_func(m4u_id, 0);
	if (ret)
		mmu_seq_print(s,
				"%s, failed to disable secure debug signal\n",
			  __func__);

	spin_unlock_irqrestore(&data->reg_lock, flags);

	return result;
}

#if 0
int mtk_get_pfh_tlb_all(const struct mtk_iommu_data *data,
		struct mmu_pfh_tlb_t *pfh_buf)
{
	unsigned int regval, m4u_id = data->m4uid;
	void __iomem *base = data->base;
	int set_nr, way_nr, set, way;
	int valid;
	int pfh_id = 0;

	set_nr = MTK_IOMMU_SET_NR(m4u_id);
	way_nr = MTK_IOMMU_WAY_NR;

	for (way = 0; way < way_nr; way++) {
		for (set = 0; set < set_nr; set++) {
			int page;
			struct mmu_tlb_t tlb;

			regval = readl_relaxed(base +
				REG_MMU_PFH_VLD(set, way));
			valid = !!(regval & F_MMU_PFH_VLD_BIT(set, way));
			mtk_get_pfh_tlb(data, set, 0, way, &tlb);

			pfh_buf[pfh_id].tag = tlb.tag;
			pfh_buf[pfh_id].va =
				imu_pfh_tag_to_va(m4u_id,
					set, way, tlb.tag);
			pfh_buf[pfh_id].layer =
				!!(tlb.tag & F_PFH_TAG_LAYER_BIT);
			pfh_buf[pfh_id].bank = !!(tlb.tag & F_PFH_PT_BANK_BIT);
			pfh_buf[pfh_id].sec = !!(tlb.tag & F_PFH_TAG_SEC_BIT);
			pfh_buf[pfh_id].pfh = !!(tlb.tag & F_PFH_TAG_AUTO_PFH);
			pfh_buf[pfh_id].set = set;
			pfh_buf[pfh_id].way = way;
			pfh_buf[pfh_id].valid = valid;
			pfh_buf[pfh_id].desc[0] = tlb.desc;
			pfh_buf[pfh_id].page_size =
				pfh_buf[pfh_id].layer ?
					SZ_4K : SZ_1M;

			for (page = 1; page < MMU_PAGE_PER_LINE; page++) {
				mtk_get_pfh_tlb(data, set, page, way, &tlb);
				pfh_buf[pfh_id].desc[page] = tlb.desc;
			}
			pfh_id++;
		}
	}

	return 0;
}
#endif

unsigned int mtk_get_victim_tlb(const struct mtk_iommu_data *data, int page,
			int entry, struct mmu_tlb_t *pTlb)
{
	unsigned int regValue = 0;
	void __iomem *base = data->base;
	u32 tmp = 0;
	int ret = 0;

	regValue = F_READ_ENTRY_EN
		   | F_READ_ENTRY_VICT_TLB_SEL
#if (MMU_ENTRY_PER_VICTIM == 16)
		   | F_READ_ENTRY_PFH_IDX((entry & 0xc) >> 2)
#endif
		   | F_READ_ENTRY_PFH_PAGE_IDX(page)
		   | F_READ_ENTRY_PFH_WAY(entry & 0x3);

	writel_relaxed(regValue,
		   base + REG_MMU_READ_ENTRY);
	ret = readl_poll_timeout_atomic(base +
					REG_MMU_READ_ENTRY, tmp,
					(tmp & F_READ_ENTRY_EN) == 0,
					10, 1000);
	if (ret) {
		dev_notice(data->dev, "iommu:%d polling timeout\n",
			   data->m4uid);
		pTlb->desc = 0;
		pTlb->tag = 0;
		return 0;
	}
	pTlb->desc = readl_relaxed(base + REG_MMU_DES_RDATA);
	pTlb->tag = readl_relaxed(base + REG_MMU_PFH_TAG_RDATA);

	return 0;
}

static unsigned long imu_victim_tag_to_va(int mmu, unsigned int tag)
{
	unsigned long tmp;

	tmp = F_PFH_TAG_VA_GET(mmu, tag);
	if (tag & F_PFH_TAG_LAYER_BIT)
		tmp |= F_VIC_TAG_VA_GET_L1(mmu, tag);

	return tmp;

}

int mtk_dump_victim_tlb(int m4u_id,
		struct seq_file *s)
{
	unsigned int regval;
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);
	void __iomem *base;
	int result = 0;
	int entry, entry_nr;
	int valid;
	unsigned long flags;
	int ret;

	if (!data)
		return 0;
	base = data->base;

	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		mmu_seq_print(s,
			       "iommu:%d power off\n", m4u_id);
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return 0;
	}
#endif

	ret = mtk_switch_secure_debug_func(m4u_id, 1);
	if (ret) {
		mmu_seq_print(s,
				"%s, failed to enable secure debug signal\n",
			  __func__);
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return 0;
	}

	entry_nr = MMU_ENTRY_PER_VICTIM;

	mmu_seq_print(s,
		       "==== victim tlb iommu%d  ====\n", m4u_id);

	for (entry = 0; entry < entry_nr; entry++) {
		int page;
		struct mmu_tlb_t tlb;

		regval = readl_relaxed(base + REG_MMU_VICT_VLD);
		valid = !!(regval & F_MMU_VICT_VLD_BIT(entry));
		mtk_get_victim_tlb(data, 0, entry, &tlb);
		mmu_seq_print(s,
			       "%d v:%d va:0x%lx layer%d bank%d sec:%d pfh:%d tag:0x%x <0x%x ",
			       entry, valid,
			       imu_victim_tag_to_va(m4u_id, tlb.tag),
			       !!(tlb.tag & F_PFH_TAG_LAYER_BIT),
			       (tlb.tag & F_PFH_PT_BANK_BIT),
			       !!(tlb.tag & F_PFH_TAG_SEC_BIT),
			       !!(tlb.tag & F_PFH_TAG_AUTO_PFH),
			       tlb.tag, tlb.desc);
		for (page = 1; page < MMU_PAGE_PER_LINE; page++) {
			mtk_get_victim_tlb(data, page, entry, &tlb);
			mmu_seq_print(s, "0x%x ", tlb.desc);
		}
		mmu_seq_print(s, ">\n");
	}

	ret = mtk_switch_secure_debug_func(m4u_id, 0);
	if (ret)
		mmu_seq_print(s,
				"%s, failed to disable secure debug signal\n",
			  __func__);

	spin_unlock_irqrestore(&data->reg_lock, flags);

	return result;
}

#if 0
int mtk_confirm_main_range_invalidated(
		const struct mtk_iommu_data *data,
		int m4u_slave_id, unsigned int iova_s,
		unsigned int iova_e)
{
	unsigned int i;
	unsigned int regval;

	/* /> check Main TLB part */
	for (i = 0; i < g_tag_count[data->m4uid]; i++) {
		regval = mtk_get_main_tag(data, m4u_slave_id, i);

		if (regval & (F_MAIN_TLB_VALID_BIT)) {
			unsigned int tag_s, tag_e, sa, ea;
			int layer = regval & F_MAIN_TLB_LAYER_BIT;
			int large = regval & F_MAIN_TLB_16X_BIT;

			tag_s = regval & F_MAIN_TLB_VA_MSK;
			sa = iova_s & (~(PAGE_SIZE - 1));
			ea = iova_e | (PAGE_SIZE - 1);

			if (layer) {	/* pte */
				if (large)
					tag_e = tag_s + SZ_64K - 1;
				else
					tag_e = tag_s + PAGE_SIZE - 1;

				if (!((tag_e < sa) || (tag_s > ea))) {
					pr_notice(
						  "main: i=%d, idx=0x%x, iova_s=0x%x, iova_e=0x%x, RegValue=0x%x\n",
						  i, data->m4uid, iova_s,
						  iova_e, regval);
					return -1;
				}

			} else {
				if (large)
					tag_e =
						tag_s +
							SZ_16M -
								1;
				else
					tag_e =
						tag_s +
							SZ_1M -
								1;

				if ((tag_s >= sa) && (tag_e <= ea)) {
					pr_notice(
						  "main: i=%d, idx=0x%x, iova_s=0x%x, iova_e=0x%x, RegValue=0x%x\n",
						  i, data->m4uid,
						  iova_s, iova_e, regval);
					return -1;
				}
			}
		}
	}
	return 0;
}

int mtk_confirm_range_invalidated(const struct mtk_iommu_data *data,
		unsigned int iova_s, unsigned int iova_e)
{
	unsigned int i = 0;
	unsigned int regval;
	void __iomem *base = data->base;
	int result = 0;
	int set_nr, way_nr, set, way;

	/* /> check Main TLB part */
	result =
		mtk_confirm_main_range_invalidated(
		data, 0, iova_s, iova_e);
	if (result < 0)
		return -1;

	if (data->m4uid == 0) {
		result =
			mtk_confirm_main_range_invalidated(
				data, 1, iova_s, iova_e);
		if (result < 0)
			return -1;
	}

	set_nr = MTK_IOMMU_SET_NR(data->m4uid);
	way_nr = MTK_IOMMU_WAY_NR;

	for (way = 0; way < way_nr; way++) {
		for (set = 0; set < set_nr; set++) {
			regval =
				readl_relaxed(base +
					REG_MMU_PFH_VLD(set, way));
			if (regval & F_MMU_PFH_VLD_BIT(set, way)) {
				unsigned int tag =
					mtk_get_pfh_tag(data,
						set, 0, way);
				unsigned int tag_s, tag_e, sa, ea;
				int layer = tag & F_PFH_TAG_LAYER_BIT;
				int large = tag & F_PFH_TAG_16X_BIT;

				tag_s = imu_pfh_tag_to_va(data->m4uid,
						set, way, tag);

				sa = iova_s & (~(PAGE_SIZE - 1));
				ea = iova_e | (PAGE_SIZE - 1);

				if (layer) {	/* pte */
					if (large)
						tag_e =
							tag_s +
							SZ_64K * 8
									- 1;
					else
						tag_e =
							tag_s +
							PAGE_SIZE * 8 - 1;

					if (!((tag_e < sa) || (tag_s > ea))) {
						pr_notice(
							  "main: i=%d, idx=0x%x, iova_s=0x%x, iova_e=0x%x, RegValue=0x%x\n",
							  i, data->m4uid,
							  iova_s,
							  iova_e, regval);
						return -1;
					}

				} else {
					if (large)
						tag_e =
						tag_s +
						SZ_16M * 8
								- 1;
					else
						tag_e =
						tag_s +
						SZ_1M * 8 -
						1;

					/* if((tag_s>=sa)&&(tag_e<=ea)) */
					if (!((tag_e < sa) || (tag_s > ea))) {
						pr_notice(
							  "main: i=%d, idx=0x%x, iova_s=0x%x, iova_e=0x%x, RegValue=0x%x\n",
							  i, data->m4uid,
							  iova_s,
							  iova_e, regval);
						return -1;
					}
				}
			}
		}
	}

	return result;
}

int mtk_confirm_main_all_invalid(
	const struct mtk_iommu_data *data, int m4u_slave_id)
{
	unsigned int i;
	unsigned int regval;

	for (i = 0; i < g_tag_count[data->m4uid]; i++) {
		regval = mtk_get_main_tag(data, m4u_slave_id, i);

		if (regval & (F_MAIN_TLB_VALID_BIT)) {
			pr_notice(
				  "main: i=%d, idx=0x%x, RegValue=0x%x\n",
				  i, data->m4uid, regval);
			return -1;
		}
	}
	return 0;
}

int mtk_confirm_pfh_all_invalid(const struct mtk_iommu_data *data)
{
	unsigned int regval;
	void __iomem *base = data->base;
	int set_nr, way_nr, set, way;

	set_nr = MTK_IOMMU_SET_NR(data->m4uid);
	way_nr = MTK_IOMMU_WAY_NR;

	for (way = 0; way < way_nr; way++) {
		for (set = 0; set < set_nr; set++) {
			regval = readl_relaxed(base +
				REG_MMU_PFH_VLD(set, way));
			if (regval & F_MMU_PFH_VLD_BIT(set, way))
				return -1;

		}
	}
	return 0;
}

int mtk_confirm_all_invalidated(int m4u_id)
{
	const struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);

	if (!data)
		return 0;
	if (mtk_confirm_main_all_invalid(data, 0))
		return -1;

	if (m4u_id == 0) {
		if (mtk_confirm_main_all_invalid(data, 1))
			return -1;
	}

	if (mtk_confirm_pfh_all_invalid(data))
		return -1;

	return 0;
}
#endif

int mau_start_monitor(unsigned int m4u_id, unsigned int slave,
			  unsigned int mau, struct mau_config_info *mau_info)
{
	void __iomem *base;
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);
	int ret = 0;
	unsigned long flags;

	if (!data || !mau_info ||
	    slave >= MTK_MMU_NUM_OF_IOMMU(m4u_id) ||
	    mau >= MTK_MAU_NUM_OF_MMU(slave)) {
		pr_notice("%s, %d, invalid m4u:%d, slave:%d, mau:%d\n",
			  __func__, __LINE__, m4u_id, slave, mau);
		return -1;
	}

	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		pr_notice("%s, iommu%u power off\n",
			  __func__, data->m4uid);
		return 0;
	}
#endif
	base = data->base;

	ret = mtk_switch_secure_debug_func(m4u_id, 1);
	if (ret) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		pr_notice("%s, %d, failed to enable secure debug signal\n",
			  __func__, __LINE__);
		return ret;
	}

	/*enable interrupt*/
	iommu_set_field_by_mask(base,
		   REG_MMU_INT_MAIN_CONTROL,
		   F_INT_MAIN_MAU_INT_EN(slave),
		   F_INT_MAIN_MAU_INT_EN(slave));

	/*config start addr*/
	writel_relaxed(mau_info->start, base +
		   REG_MMU_MAU_SA(slave, mau));
	writel_relaxed(mau_info->start_bit32, base +
		   REG_MMU_MAU_SA_EXT(slave, mau));

	/*config end addr*/
	writel_relaxed(mau_info->end, base +
		   REG_MMU_MAU_EA(slave, mau));
	writel_relaxed(mau_info->end_bit32, base +
		   REG_MMU_MAU_EA_EXT(slave, mau));

	/*config larb id*/
	writel_relaxed(mau_info->larb_mask, base +
		   REG_MMU_MAU_LARB_EN(slave));

	/*config port id*/
	writel_relaxed(mau_info->port_mask, base +
		   REG_MMU_MAU_PORT_EN(slave, mau));

	iommu_set_field_by_mask(base, REG_MMU_MAU_IO(slave),
				F_MAU_BIT_VAL(1, mau),
				F_MAU_BIT_VAL(mau_info->io, mau));

	iommu_set_field_by_mask(base, REG_MMU_MAU_RW(slave),
				F_MAU_BIT_VAL(1, mau),
				F_MAU_BIT_VAL(mau_info->wr, mau));

	iommu_set_field_by_mask(base, REG_MMU_MAU_VA(slave),
				F_MAU_BIT_VAL(1, mau),
				F_MAU_BIT_VAL(mau_info->virt, mau));
	wmb(); /*make sure the MAU ops has been triggered*/

	pr_notice("%s iommu:%d, slave:%d, mau:%d, start=0x%x(0x%x), end=0x%x(0x%x), vir:%d, wr:%d, io:0x%x, port:0x%x, larb:0x%x\n",
		  __func__, m4u_id, slave, mau,
		  readl_relaxed(base + REG_MMU_MAU_SA(slave, mau)),
		  readl_relaxed(base + REG_MMU_MAU_SA_EXT(slave, mau)),
		  readl_relaxed(base + REG_MMU_MAU_EA(slave, mau)),
		  readl_relaxed(base + REG_MMU_MAU_EA_EXT(slave, mau)),
		  readl_relaxed(base + REG_MMU_MAU_VA(slave)),
		  readl_relaxed(base + REG_MMU_MAU_RW(slave)),
		  readl_relaxed(base + REG_MMU_MAU_IO(slave)),
		  readl_relaxed(base + REG_MMU_MAU_PORT_EN(slave, mau)),
		  readl_relaxed(base + REG_MMU_MAU_LARB_EN(slave)));

	ret = mtk_switch_secure_debug_func(m4u_id, 0);
	if (ret)
		pr_notice("%s, %d, failed to disable secure debug signal\n",
			  __func__, __LINE__);

	spin_unlock_irqrestore(&data->reg_lock, flags);

	return 0;
}

void mau_stop_monitor(unsigned int m4u_id, unsigned int slave,
		unsigned int mau, bool force)
{
	unsigned int irq = 0;
	void __iomem *base;
	const struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);
	struct mau_config_info *mau_cfg = get_mau_info(m4u_id);

	if (force) {
		if (!data)
			return;
#ifdef IOMMU_POWER_CLK_SUPPORT
		if (!data->poweron) {
			pr_notice("%s, iommu%u power off\n",
				  __func__, data->m4uid);
			return;
		}
#endif
		base = data->base;
		irq = readl_relaxed(base +
			REG_MMU_INT_MAIN_CONTROL);
		irq = irq & ~F_INT_MAIN_MAU_INT_EN(slave);
		writel_relaxed(irq, base +
			REG_MMU_INT_MAIN_CONTROL);
		return;
	}
	mau_start_monitor(m4u_id, slave, mau, mau_cfg);
}

/* notes: must fill cfg->m4u_id/slave/mau before call this func. */
int mau_get_config_info(struct mau_config_info *cfg)
{
	int slave = cfg->slave;
	int mau = cfg->mau;
	void __iomem *base;
	const struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(cfg->m4u_id);

	if (!data ||
	    slave >= MTK_MMU_NUM_OF_IOMMU(cfg->m4u_id) ||
	    mau >= MTK_MAU_NUM_OF_MMU(slave)) {
		pr_notice("%s, %d, invalid m4u:%d, slave:%d, mau:%d\n",
			  __func__, __LINE__, cfg->m4u_id, slave, mau);
		return -1;
	}

#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron)
		return 0;
#endif
	base = data->base;

	cfg->start = readl_relaxed(base +
			REG_MMU_MAU_SA(slave, mau));
	cfg->end = readl_relaxed(base +
			REG_MMU_MAU_EA(slave, mau));
	cfg->start_bit32 = readl_relaxed(base +
			REG_MMU_MAU_SA_EXT(slave, mau));
	cfg->end_bit32 = readl_relaxed(base +
			REG_MMU_MAU_EA_EXT(slave, mau));
	cfg->port_mask = readl_relaxed(base +
			REG_MMU_MAU_PORT_EN(slave, mau));
	cfg->larb_mask = readl_relaxed(base +
			REG_MMU_MAU_LARB_EN(slave));

	cfg->io =
		!!(iommu_get_field_by_mask(base,
				 REG_MMU_MAU_IO(slave),
				 F_MAU_BIT_VAL(1, mau)));

	cfg->wr =
		!!iommu_get_field_by_mask(base,
				REG_MMU_MAU_RW(slave),
				F_MAU_BIT_VAL(1, mau));

	cfg->virt =
		!!iommu_get_field_by_mask(base,
				REG_MMU_MAU_VA(slave),
				F_MAU_BIT_VAL(1, mau));

	return 0;
}

int __mau_dump_status(int m4u_id, int slave, int mau)
{
	void __iomem *base;
	unsigned int status;
	unsigned long flags;
	unsigned int assert_id, assert_addr, assert_b32;
	char *name;
	struct mau_config_info mau_cfg;
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);

	if (!data ||
	    slave >= MTK_MMU_NUM_OF_IOMMU(m4u_id) ||
	    mau >= MTK_MAU_NUM_OF_MMU(slave)) {
		pr_notice("%s, %d, invalid m4u:%d, slave:%d, mau:%d\n",
			  __func__, __LINE__, m4u_id, slave, mau);
		return -1;
	}

	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		return 0;
	}
#endif
	base = data->base;
	status = readl_relaxed(base + REG_MMU_MAU_ASRT_STA(slave));

	if (status & (1 << mau)) {
		pr_notice("%s: mau_assert in set %d, status:0x%x\n",
			  __func__, mau, status);
		assert_id = readl_relaxed(base +
			REG_MMU_MAU_ASRT_ID(slave, mau));
		assert_addr = readl_relaxed(base +
			REG_MMU_MAU_ADDR(slave, mau));
		assert_b32 = readl_relaxed(base +
			REG_MMU_MAU_ADDR_BIT32(slave, mau));
		//larb = F_MMU_MAU_ASRT_ID_LARB(assert_id);
		//port = F_MMU_MAU_ASRT_ID_PORT(assert_id);
		name = mtk_iommu_get_port_name(m4u_id,
				(assert_id & F_MMU_MAU_ASRT_ID_VAL) << 2);
		pr_notice("%s: mau dump: id=0x%x(%s),addr=0x%x,b32=0x%x\n",
			  __func__, assert_id, name,
			  assert_addr, assert_b32);

		writel_relaxed((1 << mau), base +
			   REG_MMU_MAU_CLR(slave));
		writel_relaxed(0, base + REG_MMU_MAU_CLR(slave));
		wmb(); /*make sure the MAU data is cleared*/

		mau_cfg.m4u_id = m4u_id;
		mau_cfg.slave = slave;
		mau_cfg.mau = mau;
		mau_get_config_info(&mau_cfg);

		pr_notice(
			  "%s: mau_cfg: start=0x%x,end=0x%x,virt(%d),io(%d),wr(%d),s_b32(%d),e_b32(%d),larb(0x%x),port(0x%x)\n",
		 __func__,
		 mau_cfg.start, mau_cfg.end,
		 mau_cfg.virt, mau_cfg.io,
		 mau_cfg.wr,
		 mau_cfg.start_bit32, mau_cfg.end_bit32,
		 mau_cfg.larb_mask, mau_cfg.port_mask);

	} else
		pr_debug("%s: mau no assert in set %d\n",
			__func__, mau);

	spin_unlock_irqrestore(&data->reg_lock, flags);
	return 0;
}

int iommu_perf_get_counter(int m4u_id,
		int slave, struct IOMMU_PERF_COUNT *p_perf_count)
{
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);
	void __iomem *base;
	int ret = 0;
	unsigned long flags;

	if (!data ||
	    slave >= MTK_MMU_NUM_OF_IOMMU(m4u_id)) {
		pr_notice("%s, %d, invalid m4u:%d, slave:%d\n",
			  __func__, __LINE__, m4u_id, slave);
		return -1;
	}

	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		pr_notice("%s: iommu:%d power off\n",
			  __func__, m4u_id);
		return -2;
	}
#endif
	base = data->base;

	ret = mtk_switch_secure_debug_func(m4u_id, 1);
	if (ret) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		pr_notice("%s, %d, failed to enable secure debug signal\n",
			  __func__, __LINE__);
		return ret;
	}

	/* Transaction access count */
	p_perf_count->transaction_cnt =
		readl_relaxed(base + REG_MMU_ACC_CNT(slave));
	/* Main TLB miss count */
	p_perf_count->main_tlb_miss_cnt =
		readl_relaxed(base + REG_MMU_MAIN_L1_MSCNT(slave));
	p_perf_count->main_tlb_miss_cnt +=
		readl_relaxed(base + REG_MMU_MAIN_L2_MSCNT(slave));
	 /* /> Prefetch TLB miss count */
	p_perf_count->pfh_tlb_miss_cnt =
		readl_relaxed(base + REG_MMU_PF_L1_MSCNT);
	p_perf_count->pfh_tlb_miss_cnt +=
		readl_relaxed(base + REG_MMU_PF_L2_MSCNT);
	  /* /> Prefetch count */
	p_perf_count->pfh_cnt =
		readl_relaxed(base + REG_MMU_PF_L1_CNT);
	p_perf_count->pfh_cnt +=
		readl_relaxed(base + REG_MMU_PF_L2_CNT);
	p_perf_count->rs_perf_cnt =
		readl_relaxed(base + REG_MMU_RS_PERF_CNT(slave));

	spin_unlock_irqrestore(&data->reg_lock, flags);
	ret = mtk_switch_secure_debug_func(m4u_id, 0);
	if (ret)
		pr_notice("%s, %d, failed to disable secure debug signal\n",
			  __func__, __LINE__);

	return 0;
}

void iommu_perf_print_counter(int m4u_id, int slave, const char *msg)
{
	struct IOMMU_PERF_COUNT cnt;
	int ret = 0;

	pr_info(
		"==== performance count for %s iommu:%d, slave:%d======\n",
		msg, m4u_id, slave);
	ret = iommu_perf_get_counter(m4u_id, slave, &cnt);
	if (!ret)
		pr_info(
			">>> total trans=%u, main_miss=%u, pfh_miss=%u, pfh_cnt=%u, rs_perf_cnt=%u\n",
			cnt.transaction_cnt, cnt.main_tlb_miss_cnt,
			cnt.pfh_tlb_miss_cnt, cnt.pfh_cnt,
			cnt.rs_perf_cnt);
	else
		pr_info("failed to get performance data, ret:%d\n", ret);
}

int iommu_perf_monitor_start(int m4u_id)
{
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);
	void __iomem *base;
	unsigned long flags;

	if (!data) {
		pr_notice("%s, %d, invalid m4u:%d\n",
			  __func__, __LINE__, m4u_id);
		return -1;
	}

	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		pr_notice("%s: iommu:%d power off\n",
			  __func__, m4u_id);
		return 0;
	}
#endif
	base = data->base;

	pr_info("====%s: %d======\n", __func__, m4u_id);
	/* clear GMC performance counter */
	iommu_set_field_by_mask(base, REG_MMU_CTRL_REG,
				F_MMU_CTRL_MONITOR_CLR(1),
				F_MMU_CTRL_MONITOR_CLR(1));
	iommu_set_field_by_mask(base, REG_MMU_CTRL_REG,
				F_MMU_CTRL_MONITOR_CLR(1),
				F_MMU_CTRL_MONITOR_CLR(0));

	/* enable GMC performance monitor */
	iommu_set_field_by_mask(base, REG_MMU_CTRL_REG,
				F_MMU_CTRL_MONITOR_EN(1),
				F_MMU_CTRL_MONITOR_EN(1));

	spin_unlock_irqrestore(&data->reg_lock, flags);
	return 0;
}

int iommu_perf_monitor_stop(int m4u_id)
{
	struct mtk_iommu_data *data = mtk_iommu_get_m4u_data(m4u_id);
	void __iomem *base;
	unsigned int i;
	unsigned long flags;

	if (!data) {
		pr_notice("%s, %d, invalid m4u:%d\n",
			  __func__, __LINE__, m4u_id);
		return -1;
	}

	spin_lock_irqsave(&data->reg_lock, flags);
#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		spin_unlock_irqrestore(&data->reg_lock, flags);
		pr_notice("%s: iommu:%d power off\n",
			  __func__, m4u_id);
		return 0;
	}
#endif
	base = data->base;

	pr_info("====%s: %d======\n", __func__, m4u_id);
	/* disable GMC performance monitor */
	iommu_set_field_by_mask(base, REG_MMU_CTRL_REG,
				F_MMU_CTRL_MONITOR_EN(1),
				F_MMU_CTRL_MONITOR_EN(0));

	spin_unlock_irqrestore(&data->reg_lock, flags);
	for (i = 0; i < MTK_IOMMU_MMU_COUNT; i++)
		iommu_perf_print_counter(m4u_id, i, __func__);

	return 0;
}

#define IOMMU_REG_BACKUP_SIZE	 (100 * sizeof(unsigned int))
static unsigned int *p_reg_backup[MTK_IOMMU_M4U_COUNT];
static unsigned int g_reg_backup_real_size[MTK_IOMMU_M4U_COUNT];

static int mau_reg_backup(const struct mtk_iommu_data *data)
{
	unsigned int *p_reg;
	void __iomem *base = data->base;
	int slave;
	int mau;
	unsigned int real_size;

#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron)
		return 0;
#endif

	if (!p_reg_backup[data->m4uid]) {
		pr_notice("%s, %d, iommu:%d no memory for backup\n",
			  __func__, __LINE__, data->m4uid);
		return -1;
	}
	p_reg = p_reg_backup[data->m4uid];

	for (slave = 0; slave < MTK_MMU_NUM_OF_IOMMU(data->m4uid); slave++) {
		for (mau = 0; mau < MTK_MAU_NUM_OF_MMU(slave); mau++) {
			*(p_reg++) = readl_relaxed(base +
				REG_MMU_MAU_SA(slave, mau));
			*(p_reg++) = readl_relaxed(base +
				REG_MMU_MAU_SA_EXT(slave, mau));
			*(p_reg++) = readl_relaxed(base +
				REG_MMU_MAU_EA(slave, mau));
			*(p_reg++) = readl_relaxed(base +
				REG_MMU_MAU_EA_EXT(slave, mau));
			*(p_reg++) = readl_relaxed(base +
				REG_MMU_MAU_PORT_EN(slave, mau));
		}
		*(p_reg++) = readl_relaxed(base +
			REG_MMU_MAU_LARB_EN(slave));
		*(p_reg++) = readl_relaxed(base +
			REG_MMU_MAU_IO(slave));
		*(p_reg++) = readl_relaxed(base +
			REG_MMU_MAU_RW(slave));
		*(p_reg++) = readl_relaxed(base +
			REG_MMU_MAU_VA(slave));
	}

	/* check register size (to prevent overflow) */
	real_size = (p_reg - p_reg_backup[data->m4uid]) * sizeof(unsigned int);
	if (real_size > IOMMU_REG_BACKUP_SIZE)
		mmu_aee_print("m4u_reg overflow! %d>%d\n",
			real_size, (int)IOMMU_REG_BACKUP_SIZE);

	g_reg_backup_real_size[data->m4uid] = real_size;

	return 0;
}

static int mau_reg_restore(const struct mtk_iommu_data *data)
{
	unsigned int *p_reg;
	void __iomem *base = data->base;
	int slave;
	int mau;
	unsigned int real_size;

#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron)
		return 0;
#endif

	if (!p_reg_backup[data->m4uid]) {
		pr_notice("%s, %d, iommu:%d no memory for restore\n",
			  __func__, __LINE__, data->m4uid);
		return -1;
	}
	p_reg = p_reg_backup[data->m4uid];

	for (slave = 0; slave < MTK_MMU_NUM_OF_IOMMU(data->m4uid); slave++) {
		for (mau = 0; mau < MTK_MAU_NUM_OF_MMU(slave); mau++) {
			writel_relaxed(*(p_reg++), base +
				REG_MMU_MAU_SA(slave, mau));
			writel_relaxed(*(p_reg++), base +
				REG_MMU_MAU_SA_EXT(slave, mau));
			writel_relaxed(*(p_reg++), base +
				REG_MMU_MAU_EA(slave, mau));
			writel_relaxed(*(p_reg++), base +
				REG_MMU_MAU_EA_EXT(slave, mau));
			writel_relaxed(*(p_reg++), base +
				REG_MMU_MAU_PORT_EN(slave, mau));
		}
		writel_relaxed(*(p_reg++), base +
			REG_MMU_MAU_LARB_EN(slave));
		writel_relaxed(*(p_reg++), base +
			REG_MMU_MAU_IO(slave));
		writel_relaxed(*(p_reg++), base +
			REG_MMU_MAU_RW(slave));
		writel_relaxed(*(p_reg++), base +
			REG_MMU_MAU_VA(slave));
	}
	wmb(); /*make sure the MVA data is restored*/

	/* check register size (to prevent overflow) */
	real_size = (p_reg - p_reg_backup[data->m4uid]) * sizeof(unsigned int);
	if (real_size != g_reg_backup_real_size[data->m4uid])
		mmu_aee_print("m4u_reg_retore %d!=%d\n",
			real_size,
			g_reg_backup_real_size[data->m4uid]);

	return 0;
}

static int mtk_iommu_reg_backup(struct mtk_iommu_data *data)
{
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;
	int ret = 0;

#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron)
		return 0;
#endif
	if (!base || IS_ERR((void *)(unsigned long)base)) {
		pr_notice("%s, %d, invalid base addr\n",
			  __func__, __LINE__);
		return -1;
	}

	ret = mtk_switch_secure_debug_func(data->m4uid, 1);
	if (ret)
		pr_notice("%s, %d, m4u:%u, failed to enable secure debug signal\n",
			  __func__, __LINE__, data->m4uid);

	reg->standard_axi_mode = readl_relaxed(base +
					   REG_MMU_MISC_CTRL);
	reg->dcm_dis = readl_relaxed(base +
					   REG_MMU_DCM_DIS);
	reg->ctrl_reg = readl_relaxed(base +
					   REG_MMU_CTRL_REG);
	reg->int_control0 = readl_relaxed(base +
					   REG_MMU_INT_CONTROL0);
	reg->int_main_control = readl_relaxed(base +
					   REG_MMU_INT_MAIN_CONTROL);
	reg->pt_base = readl_relaxed(base +
					   REG_MMU_PT_BASE_ADDR);
	reg->wr_ctrl = readl_relaxed(base +
					   REG_MMU_WR_LEN_CTRL);
	reg->ivrp_paddr = readl_relaxed(base +
					   REG_MMU_TFRP_PADDR);

#ifdef MTK_IOMMU_BANK_IRQ_SUPPORT
	ret = mtk_iommu_atf_call(IOMMU_ATF_SECURITY_BACKUP,
			   data->m4uid, MTK_IOMMU_BANK_COUNT);
#elif defined(MTK_M4U_SECURE_IRQ_SUPPORT)
	ret = mtk_iommu_atf_call(IOMMU_ATF_SECURITY_BACKUP,
			   data->m4uid, 4);
#endif

	mau_reg_backup(data);

	ret = mtk_switch_secure_debug_func(data->m4uid, 0);
	if (ret)
		pr_notice("%s, %d, m4u:%u, failed to disable secure debug signal\n",
			  __func__, __LINE__, data->m4uid);

	return 0;
}

static int mtk_iommu_reg_restore(struct mtk_iommu_data *data)
{
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;
	int ret = 0;

#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron)
		return 0;
#endif
	if (!base || IS_ERR(base)) {
		pr_notice("%s, %d, invalid base addr\n",
			  __func__, __LINE__);
		return -1;
	}

#ifdef MTK_IOMMU_BANK_IRQ_SUPPORT
	ret = mtk_iommu_atf_call(IOMMU_ATF_SECURITY_RESTORE,
			   data->m4uid, MTK_IOMMU_BANK_COUNT);
#elif defined(MTK_M4U_SECURE_IRQ_SUPPORT)
	ret = mtk_iommu_atf_call(IOMMU_ATF_SECURITY_RESTORE,
			   data->m4uid, 4);
#endif

	ret = mtk_switch_secure_debug_func(data->m4uid, 1);
	if (ret)
		pr_notice("%s, %d, m4u:%u, failed to enable secure debug signal\n",
			  __func__, __LINE__, data->m4uid);

	mau_reg_restore(data);

	writel_relaxed(reg->standard_axi_mode, base +
		   REG_MMU_MISC_CTRL);
	writel_relaxed(reg->dcm_dis, base +
		   REG_MMU_DCM_DIS);
	writel_relaxed(reg->ctrl_reg, base +
		   REG_MMU_CTRL_REG);
	writel_relaxed(reg->int_control0, base +
		   REG_MMU_INT_CONTROL0);
	writel_relaxed(reg->int_main_control, base +
		   REG_MMU_INT_MAIN_CONTROL);
	writel_relaxed(reg->pt_base, base +
		   REG_MMU_PT_BASE_ADDR);
	writel_relaxed(reg->wr_ctrl, base +
		   REG_MMU_WR_LEN_CTRL);
	writel_relaxed(reg->ivrp_paddr, base +
		   REG_MMU_TFRP_PADDR);
	wmb(); /*make sure the registers have been restored.*/

	ret = mtk_switch_secure_debug_func(data->m4uid, 0);
	if (ret)
		pr_notice("%s, %d, m4u:%u, failed to disable secure debug signal\n",
			  __func__, __LINE__, data->m4uid);

	return 0;
}

#ifdef MTK_IOMMU_LOW_POWER_SUPPORT
static void mtk_iommu_pg_after_on(enum subsys_id sys)
{
	struct mtk_iommu_data *data;
	int ret = 0, i;
	unsigned long flags;

	for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++) {
		if (iommu_mtcmos_subsys[i] != sys)
			continue;

		data = mtk_iommu_get_m4u_data(i);
		if (!data) {
			pr_notice("%s, %d iommu %d is null\n",
				  __func__, __LINE__, i);
			continue;
		}
		if (!data->m4u_clks->nr_powers) {
			pr_notice("%s, iommu%u power control is not support\n",
				  __func__, data->m4uid);
			continue;
		}

		spin_lock_irqsave(&data->reg_lock, flags);
		if (data->poweron) {
			pr_notice("%s, iommu%u already power on, skip restore\n",
				  __func__, data->m4uid);
			spin_unlock_irqrestore(&data->reg_lock, flags);
			continue;
		}
		data->poweron = true;

		ret = mtk_iommu_reg_restore(data);
		if (ret) {
			pr_notice("%s, %d, iommu:%d, sys:%d restore failed %d\n",
				  __func__, __LINE__, data->m4uid, sys, ret);
			data->poweron = false;
			spin_unlock_irqrestore(&data->reg_lock, flags);
			continue;
		}
		spin_unlock_irqrestore(&data->reg_lock, flags);
		/*pr_notice("%s,%d,iommu:%d,sys:%d restore after on\n",
		 *	  __func__, __LINE__, data->m4uid, sys);
		 */
	}
}

static void mtk_iommu_pg_before_off(enum subsys_id sys)
{
	struct mtk_iommu_data *data;
	int ret = 0, i;
	unsigned long flags;
	unsigned long long start = 0, end = 0;

	for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++) {
		if (iommu_mtcmos_subsys[i] != sys)
			continue;

		data = mtk_iommu_get_m4u_data(i);
		if (!data) {
			pr_notice("%s, %d iommu %d is null\n",
				  __func__, __LINE__, i);
			continue;
		}

		spin_lock_irqsave(&data->reg_lock, flags);
		if (!data->poweron) {
			pr_notice("%s, iommu%u already power off, skip backup\n",
				  __func__, data->m4uid);
			spin_unlock_irqrestore(&data->reg_lock, flags);
			continue;
		}
		if (data->isr_ref) {
			spin_unlock_irqrestore(&data->reg_lock, flags);
			start = sched_clock();
			/* waiting for irs handling done */
			while (data->isr_ref) {
				end = sched_clock();
				if (end - start > 1000000000ULL) { //10ms
					break;
				}
			}
			if (end)
				pr_notice("%s pg waiting isr:%lluns, ref:%d\n",
					  __func__, end - start, data->isr_ref);
			spin_lock_irqsave(&data->reg_lock, flags);
		}
		ret = mtk_iommu_reg_backup(data);
		if (ret) {
			pr_notice("%s, %d, iommu:%d, sys:%d backup failed %d\n",
				  __func__, __LINE__, data->m4uid, sys, ret);
			data->poweron = false;
			spin_unlock_irqrestore(&data->reg_lock, flags);
			continue;
		}
		data->poweron = false;
		spin_unlock_irqrestore(&data->reg_lock, flags);
		/*pr_notice("%s,%d,iommu:%d,sys:%d backup before off\n",
		 *	  __func__, __LINE__, data->m4uid, sys);
		 */
	}
}

static void mtk_iommu_pg_debug_dump(enum subsys_id sys)
{
	struct mtk_iommu_data *data;
	int i;

	for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++) {
		if (iommu_mtcmos_subsys[i] != sys)
			continue;

		data = mtk_iommu_get_m4u_data(i);
		if (!data) {
			pr_notice("%s, %d iommu:%d is null\n",
				  __func__, __LINE__, i);
			continue;
		}

		dev_notice(data->dev, "%s, iommu:%d,status:%d,user failed at power control, iommu:%d\n",
			   __func__, __LINE__, data->m4uid, data->poweron);
	}
}

static struct pg_callbacks mtk_iommu_pg_handle = {
	.after_on  = mtk_iommu_pg_after_on,
	.before_off = mtk_iommu_pg_before_off,
	.debug_dump = mtk_iommu_pg_debug_dump,
};
#endif

static int mtk_iommu_hw_init(struct mtk_iommu_data *data)
{
	u32 regval, i, wr_en;
	unsigned int m4u_id = data->m4uid;
#if defined(MTK_M4U_SECURE_IRQ_SUPPORT) || \
	defined(MTK_IOMMU_BANK_IRQ_SUPPORT)
	struct device_node *node = NULL;
#endif

	if (!data->base || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base addr\n",
			  __func__, __LINE__);
		return -1;
	}

#ifdef IOMMU_POWER_CLK_SUPPORT
	if (!data->poweron) {
		pr_notice("%s, iommu%u power off\n",
			  __func__, data->m4uid);
		return 0;
	}
#endif
	regval = readl_relaxed(data->base + REG_MMU_CTRL_REG);
	regval = regval | F_MMU_CTRL_PFH_DIS(0)
			 | F_MMU_CTRL_MONITOR_EN(1)
			 | F_MMU_CTRL_MONITOR_CLR(0)
			 | F_MMU_CTRL_INT_FREEZE_EN(0);

	writel_relaxed(regval, data->base + REG_MMU_CTRL_REG);

	for (i = 0; i < MTK_IOMMU_MMU_COUNT; i++) {
#ifdef APU_IOMMU_INDEX
		if (m4u_id < APU_IOMMU_INDEX)
			wr_en = 0;
		else
			wr_en = F_MMU_MISC_CTRL_IN_ORDER_WR_EN(i);
#else
		wr_en = 0;
#endif

		iommu_set_field_by_mask(data->base, REG_MMU_MISC_CTRL,
					F_MMU_MISC_CTRL_COHERENCE_EN(i),
					F_MMU_MISC_CTRL_COHERENCE_EN(i));
#ifdef CONFIG_MTK_SMI_EXT
		iommu_set_field_by_mask(data->base, REG_MMU_MISC_CTRL,
					F_MMU_MISC_CTRL_IN_ORDER_WR_EN(i),
					wr_en);
#endif
		iommu_set_field_by_mask(data->base, REG_MMU_WR_LEN_CTRL,
					F_MMU_WR_LEN_CTRL_THROT_DIS(i), 0);
	}

	writel_relaxed(0x6f, data->base + REG_MMU_INT_CONTROL0);
	writel_relaxed(0xffffffff, data->base + REG_MMU_INT_MAIN_CONTROL);

	writel_relaxed(F_MMU_TFRP_PA_SET(data->protect_base, data->enable_4GB),
		   data->base + REG_MMU_TFRP_PADDR);
	writel_relaxed(0x100, data->base + REG_MMU_DCM_DIS);

	//writel_relaxed(0, data->base + REG_MMU_STANDARD_AXI_MODE);

	if (devm_request_irq(data->dev, data->irq, mtk_iommu_isr, 0,
				 dev_name(data->dev), (void *)data)) {
		writel_relaxed(0, data->base + REG_MMU_PT_BASE_ADDR);
		dev_err(data->dev, "Failed @ IRQ-%d Request\n", data->irq);
		return -ENODEV;
	}

	wmb(); /*make sure the HW has been initialized*/

	p_reg_backup[m4u_id] = kmalloc(IOMMU_REG_BACKUP_SIZE,
		GFP_KERNEL | __GFP_ZERO);
	if (p_reg_backup[m4u_id] == NULL)
		return -ENOMEM;

#ifdef MTK_M4U_SECURE_IRQ_SUPPORT
	/* register secure bank irq */
	node = of_find_compatible_node(NULL, NULL,
					 iommu_secure_compatible[m4u_id]);
	if (!node) {
		pr_notice(
			  "%s, WARN: didn't find secure node of iommu:%d\n",
			  __func__, m4u_id);
		return 0;
	}

	data->base_sec = of_iomap(node, 0);
	mtk_irq_sec[m4u_id] = irq_of_parse_and_map(node, 0);

	pr_notice("%s, secure bank, of_iomap: 0x%lx, irq_num: %d, m4u_id:%d\n",
			__func__, data->base_sec,
			mtk_irq_sec[m4u_id], m4u_id);

	if (request_irq(mtk_irq_sec[m4u_id], MTK_M4U_isr_sec,
			IRQF_TRIGGER_NONE, "secure_m4u", NULL)) {
		pr_notice("request secure m4u%d IRQ line failed\n",
			  m4u_id);
		return -ENODEV;
	}
#endif
#ifdef MTK_IOMMU_BANK_IRQ_SUPPORT
	/* register bank irq */
	for (i = 0; i < MTK_IOMMU_BANK_NODE_COUNT; i++) {
		node = of_find_compatible_node(NULL, NULL,
				 iommu_bank_compatible[m4u_id][i]);
		if (!node) {
			pr_notice(
				  "%s, WARN: didn't find bank node of iommu:%d\n",
				  __func__, m4u_id);
			continue;
		}

		data->base_bank[i] = of_iomap(node, 0);
		mtk_irq_bank[m4u_id][i] = irq_of_parse_and_map(node, 0);

		pr_notice("%s, bank:%d, of_iomap: 0x%lx, irq_num: %d, m4u_id:%d\n",
				__func__, i + 1, (uintptr_t)data->base_bank[i],
				mtk_irq_bank[m4u_id][i], m4u_id);

		if (request_irq(mtk_irq_bank[m4u_id][i], mtk_iommu_isr,
				IRQF_TRIGGER_NONE, "bank_m4u", NULL)) {
			pr_notice("request bank%d m4u%d IRQ line failed\n",
				  i + 1, m4u_id);
			continue;
		}
		writel_relaxed(0x6f, data->base_bank[i] +
			   REG_MMU_INT_CONTROL0);
		writel_relaxed(0xffffffff, data->base_bank[i] +
			   REG_MMU_INT_MAIN_CONTROL);
		writel_relaxed(F_MMU_TFRP_PA_SET(data->protect_base,
			   data->enable_4GB),
			   data->base_bank[i] + REG_MMU_TFRP_PADDR);
	}
#endif
	pr_notice("%s, done\n", __func__);

	return 0;
}

static const struct component_master_ops mtk_iommu_com_ops = {
	.bind	   = mtk_iommu_bind,
	.unbind	 = mtk_iommu_unbind,
};

static s32 mtk_iommu_clks_get(struct mtk_iommu_data *data)
{
#ifdef IOMMU_POWER_CLK_SUPPORT
	struct property *prop;
	struct device *dev;
	struct clk *clk;
	unsigned int nr = 0;
	struct mtk_iommu_clks *m4u_clks;
	const char *name, *clk_names = "clock-names";
	int i, ret = 0;

	if (!data || !data->dev) {
		pr_info("iommu No such device or address\n");
		return -ENXIO;
	} else if (data->m4u_clks) {
		pr_notice("%s, %d, clk reinit\n", __func__, __LINE__);
		return 0;
	}

	data->poweron = false;
	dev = data->dev;
	m4u_clks = kzalloc(sizeof(*m4u_clks), GFP_KERNEL);
	if (!m4u_clks)
		return -ENOMEM;

	nr = of_property_count_strings(dev->of_node, clk_names);
	if (nr > IOMMU_CLK_ID_COUNT * 2) {
		pr_info("iommu clk count %d exceed the max number of %d\n",
			nr, IOMMU_CLK_ID_COUNT);
		ret = -ENXIO;
		goto free_clks;
	}

	m4u_clks->nr_clks = 0;
	m4u_clks->nr_powers = 0;
	of_property_for_each_string(dev->of_node, clk_names, prop, name) {
		clk = devm_clk_get(dev, name);
		if (IS_ERR(clk)) {
			dev_info(dev, "clks of %s init failed\n",
				name);
			ret = PTR_ERR(clk);
			//kfree(clk);
			break;
		}
		if (strcmp(name, "power")) {
			m4u_clks->clks[m4u_clks->nr_clks] = clk;
			dev_info(dev, "iommu:%d clks%d of %s init done\n",
					data->m4uid, m4u_clks->nr_clks, name);
			m4u_clks->nr_clks++;
		} else {
			m4u_clks->powers[m4u_clks->nr_powers] = clk;
			dev_info(dev, "iommu:%d power%d of %s init done\n",
					data->m4uid, m4u_clks->nr_powers, name);
			m4u_clks->nr_powers++;
		}
	}

	if (ret)
		goto free_clk;

#if defined(APU_IOMMU_INDEX) && defined(CONFIG_MTK_APUSYS_SUPPORT)
	if (data->m4uid >= APU_IOMMU_INDEX &&
	    !apusys_power_check()) {
		m4u_clks->nr_powers = 0;
		m4u_clks->nr_clks = 0;
		pr_notice("%s, %d, apu power not support, power:%d, clk:%d\n",
			  __func__, __LINE__,
			  m4u_clks->nr_powers, m4u_clks->nr_clks);
	}
#endif
	data->m4u_clks = m4u_clks;
	g_iommu_power_support = 1;

	return 0;

free_clk:
	for (i = 0; i <  m4u_clks->nr_clks; i++)
		kfree(m4u_clks->clks[i]);

	for (i = 0; i <  m4u_clks->nr_powers; i++)
		kfree(m4u_clks->powers[i]);

free_clks:
	kfree(m4u_clks);
	return ret;

#else
	g_iommu_power_support = 0;
	return 0;
#endif
}

#ifdef CONFIG_MTK_SMI_EXT
/*
 * if CONFIG_MTK_SMI_EXT is enabled,
 * smi larb node will be init at arch_initcall_sync()
 * this will support the iommu probe at
 * the 1st time of kernel boot up.
 * if CONFIG_MTK_SMI_EXT is disabled,
 * smi larb node will be init at module_init()
 * this will delay the iommu probe,
 * and cause iommu devices init failed.
 */
static s32 mtk_iommu_larbs_get(struct mtk_iommu_data *data)
{
	int larb_nr, i;
	int ret = 0;
	struct device *dev;
	struct component_match  *match = NULL;

	if (!data || !data->dev) {
		pr_info("iommu No such device or address\n");
		return -ENXIO;
	}

	dev = data->dev;
	larb_nr = of_count_phandle_with_args(dev->of_node,
					 "mediatek,larbs", NULL);
	if (larb_nr < 0) {
		pr_notice("%s, %d, no larbs of iommu%d, larbnr=%d\n",
			  __func__, __LINE__, data->m4uid, larb_nr);
		data->smi_imu.larb_nr = 0;
		return 0;
	}
	data->smi_imu.larb_nr = larb_nr;

	for (i = 0; i < larb_nr; i++) {
		struct device_node *larbnode;
		struct platform_device *plarbdev;
		u32 id;

		larbnode = of_parse_phandle(dev->of_node, "mediatek,larbs", i);
		if (!larbnode)
			return -EINVAL;

		if (!of_device_is_available(larbnode))
			continue;

		ret = of_property_read_u32(larbnode, smi_larb_id, &id);
		if (ret) {
			/* The id is consecutive on legacy chip */
			id = i;
			pr_notice("%s, cannot find larbid, id=%d\n",
				  __func__, id);
		}

		if (id >= MTK_LARB_NR_MAX) {
			WARN_ON(1);
			dev_notice(dev, "%d exceed the max larb ID\n",
				   id);
			return -EINVAL;
		}

		plarbdev = of_find_device_by_node(larbnode);
		if (!plarbdev) {
			pr_notice("%s, invalid plarbdev, probe defer\n",
				  __func__);
			return -EPROBE_DEFER;
		}

		data->smi_imu.larb_imu[id].dev = &plarbdev->dev;

		component_match_add_release(dev, &match, release_of,
						compare_of, larbnode);
	}


	if (match)
		ret = component_master_add_with_match(dev,
					&mtk_iommu_com_ops,
					match);
	else
		ret = -ENOMEM;


	if (ret)
		pr_notice("%s, err add match, ret=%d, probe defer\n",
			  __func__, ret);

	return ret;
}
#endif

static int mtk_iommu_probe(struct platform_device *pdev)
{
	struct mtk_iommu_data *data;
	struct device *dev = &pdev->dev;
	struct resource *res;
	resource_size_t ioaddr;
	void *protect;
	unsigned long protect_pa;
	int ret = 0;
	unsigned int id = 0, slave = 0, mau = 0;

	pr_notice("%s+, %d\n",
		  __func__, __LINE__);

	ret = of_property_read_u32(dev->of_node, "cell-index", &id);
	if (ret)
		pr_notice("%s, failed to get cell index, ret=%d\n",
			  __func__, ret);

	if (total_iommu_cnt >= MTK_IOMMU_M4U_COUNT ||
	    id >= MTK_IOMMU_M4U_COUNT) {
		pr_notice("%s invalid iommu device: %d\n",
			  __func__, id);
		return 0;
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->m4uid = id; //total_iommu_cnt;
	data->dev = dev;
	data->plat_data = of_device_get_match_data(dev);

	/* Protect memory. HW will access here while translation fault.*/
#if defined(APU_IOMMU_INDEX) && \
	defined(MTK_APU_TFRP_SUPPORT)
	if (id >= APU_IOMMU_INDEX) {
		protect_pa = get_apu_iommu_tfrp(id - APU_IOMMU_INDEX);
		if (!protect_pa)
			return -ENOMEM;
		data->protect_base = protect_pa;
	} else {
		protect = devm_kzalloc(dev,
				       MTK_PROTECT_PA_ALIGN * 2, GFP_KERNEL);
		if (!protect)
			return -ENOMEM;
		protect_pa = virt_to_phys(protect);
		data->protect_base = ALIGN(protect_pa,
					   MTK_PROTECT_PA_ALIGN);
	}
#else
	protect = devm_kzalloc(dev, MTK_PROTECT_PA_ALIGN * 2, GFP_KERNEL);
	if (!protect)
		return -ENOMEM;
	protect_pa = virt_to_phys(protect);
	data->protect_base = ALIGN(protect_pa,
				   MTK_PROTECT_PA_ALIGN);
#endif

	/* Whether the current dram is over 4GB */
	data->enable_4GB = !!(max_pfn > (BIT_ULL(32) >> PAGE_SHIFT));
	spin_lock_init(&data->reg_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_info("%s, get resource is NULL\n", __func__);
		return -EINVAL;
	}
	data->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->base)) {
		pr_notice("mtk_iommu base is null\n");
		return PTR_ERR(data->base);
	}
	ioaddr = res->start;

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0) {
		pr_notice("mtk_iommu irq error\n");
		return data->irq;
	}
#ifdef CONFIG_MTK_SMI_EXT
	ret = mtk_iommu_larbs_get(data);
	if (ret) {
		pr_notice("%s, failed to get larbs\n", __func__);
		return ret;
	}
#endif
	platform_set_drvdata(pdev, data);

	ret = mtk_iommu_clks_get(data);
	if (ret) {
		pr_notice("%s, failed to get clk\n", __func__);
		return ret;
	}

	ret = mtk_iommu_power_switch(data, true, "iommu_probe");
	if (ret) {
		pr_notice("%s, failed to power switch on\n", __func__);
		return ret;
	}
#ifdef IOMMU_POWER_CLK_SUPPORT
	data->isr_ref = 0;
	if (data->m4u_clks->nr_powers)
		data->poweron = true;
	else
		pr_notice("%s, iommu%u power control is not support\n",
			  __func__, data->m4uid);
#endif

	ret = mtk_iommu_hw_init(data);
	if (ret) {
		pr_notice("%s, failed to hw init\n", __func__);
		return ret;
	}

	ret = iommu_device_sysfs_add(&data->iommu, dev, NULL,
				 "mtk-iommu.%pa", &ioaddr);
	if (ret) {
		pr_notice("%s, failed to sysfs add\n", __func__);
		return ret;
	}

	iommu_device_set_ops(&data->iommu, &mtk_iommu_ops);
	iommu_device_set_fwnode(&data->iommu, &pdev->dev.of_node->fwnode);

	ret = iommu_device_register(&data->iommu);
	if (ret) {
		pr_notice("%s, failed to device register\n", __func__);
		return ret;
	}


	list_add_tail(&data->list, &m4ulist);
	total_iommu_cnt++;
	pr_debug("%s, %d, add m4ulist, use %s pgtable\n",
		 __func__, __LINE__,
		 MTK_IOMMU_PAGE_TABLE_SHARE ? "share" : "private");
	/*
	 * trigger the bus to scan all the device to add them to iommu
	 * domain after all the iommu have finished probe.
	 */
	if (!iommu_present(&platform_bus_type) &&
	   total_iommu_cnt == MTK_IOMMU_M4U_COUNT)
		bus_set_iommu(&platform_bus_type, &mtk_iommu_ops);

	mtk_iommu_isr_pause_timer_init(data);

	for (slave = 0;
	     slave < MTK_MMU_NUM_OF_IOMMU(data->m4uid); slave++)
		for (mau = 0; mau < MTK_MAU_NUM_OF_MMU(slave); mau++) {
			struct mau_config_info *cfg = get_mau_info(data->m4uid);

			mau_start_monitor(data->m4uid, slave, mau, cfg);
		}
#ifdef MTK_IOMMU_LOW_POWER_SUPPORT
	if (total_iommu_cnt == 1)
		register_pg_callback(&mtk_iommu_pg_handle);

	ret = mtk_iommu_power_switch(data, false, "iommu_probe");
	if (ret)
		pr_notice("%s, failed to power switch off\n", __func__);

#endif

	pr_notice("%s-, %d,total=%d,m4u%d,base=0x%lx,protect=0x%pa\n",
		  __func__, __LINE__, total_iommu_cnt, data->m4uid,
		  (uintptr_t)data->base, &data->protect_base);
	return ret;
}

static int mtk_iommu_remove(struct platform_device *pdev)
{
	struct mtk_iommu_data *data = platform_get_drvdata(pdev);

	if (!data) {
		pr_notice("%s, data is NULL\n", __func__);
		return 0;
	}

	pr_notice("%s, %d, iommu%d\n",
		  __func__, __LINE__, data->m4uid);

	iommu_device_sysfs_remove(&data->iommu);
	iommu_device_unregister(&data->iommu);

	if (iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, NULL);

#ifdef IOMMU_POWER_CLK_SUPPORT
	if (data->m4u_clks->nr_powers)
		devm_free_irq(&pdev->dev, data->irq, data);
#endif
	component_master_del(&pdev->dev, &mtk_iommu_com_ops);
	return 0;
}

static void mtk_iommu_shutdown(struct platform_device *pdev)
{
	pr_notice("%s, %d\n",
		  __func__, __LINE__);
	mtk_iommu_remove(pdev);
}

static int mtk_iommu_suspend(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	int ret = 0;
#ifndef MTK_IOMMU_LOW_POWER_SUPPORT
	unsigned long flags;

	/*
	 * for IOMMU of DISP and MDP, do power off at suspend
	 * for IOMMU of APU, power off is controlled by APU
	 */
	if (!data) {
		pr_notice("%s, data is NULL\n", __func__);
		return 0;
	}

	spin_lock_irqsave(&data->reg_lock, flags);
	ret = mtk_iommu_reg_backup(data);
	if (ret)
		pr_notice("%s, %d, iommu:%d, backup failed %d\n",
			  __func__, __LINE__, data->m4uid, ret);
	spin_unlock_irqrestore(&data->reg_lock, flags);

	ret = mtk_iommu_power_switch(data, false, "iommu_suspend");
	if (ret)
		pr_notice("%s, failed to power switch off\n", __func__);
#else
	if (!data) {
		pr_notice("%s, data is NULL\n", __func__);
		return 0;
	}
	if (data->poweron)
		pr_notice("%s, iommu:%d user did not power off\n",
			  __func__, data->m4uid);
#endif
	return ret;
}

static int mtk_iommu_resume(struct device *dev)
{
	int ret = 0;
#ifndef MTK_IOMMU_LOW_POWER_SUPPORT
	unsigned long flags;
	struct mtk_iommu_data *data = dev_get_drvdata(dev);

	/*
	 * for IOMMU of DISP and MDP, do power on at suspend
	 * for IOMMU of APU, power on is controlled by APU
	 */
	if (!data) {
		pr_notice("%s, data is NULL\n", __func__);
		return 0;
	}

	ret = mtk_iommu_power_switch(data, true, "iommu_resume");
	if (ret)
		pr_notice("%s, failed to power switch on\n", __func__);

	spin_lock_irqsave(&data->reg_lock, flags);
	ret = mtk_iommu_reg_restore(data);
	if (ret)
		pr_notice("%s, %d, iommu:%d, restore failed %d\n",
			  __func__, __LINE__, data->m4uid, ret);
	spin_unlock_irqrestore(&data->reg_lock, flags);
#endif

	return ret;
}

static const struct dev_pm_ops mtk_iommu_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_iommu_suspend, mtk_iommu_resume)
};


const struct mtk_iommu_plat_data mt6xxx_v0_data = {
	.m4u_plat = iommu_mt6xxx_v0,
	.iommu_cnt = 1,
	.has_4gb_mode = true,
};

static const struct of_device_id mtk_iommu_of_ids[] = {
	{ .compatible = "mediatek,iommu_v0", .data = (void *)&mt6xxx_v0_data},
	{}
};

static struct platform_driver mtk_iommu_driver = {
	.probe  = mtk_iommu_probe,
	.remove = mtk_iommu_remove,
	.shutdown = mtk_iommu_shutdown,
	.driver = {
		.name = "mtk-iommu-v2",
		.of_match_table = of_match_ptr(mtk_iommu_of_ids),
		.pm = &mtk_iommu_pm_ops,
	}
};
#if 0
#ifdef CONFIG_ARM64
static int mtk_iommu_init_fn(struct device_node *np)
{
	static bool init_done;
	int ret = 0;
	struct platform_device *pdev;

	if (!np)
		pr_notice("%s, %d, error np\n", __func__, __LINE__);

	dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);
	if (!init_done) {
		pdev = of_platform_device_create(np, NULL,
						 platform_bus_type.dev_root);
		if (!pdev) {
			pr_notice("%s: Failed to create device\n", __func__);
			return -ENOMEM;
		}

		ret = platform_driver_register(&mtk_iommu_driver);
		if (ret) {
			pr_notice("%s: Failed to register driver\n", __func__);
			return ret;
		}
		init_done = true;
#ifdef IOMMU_DEBUG_ENABLED
	g_tf_test = false;
#endif

	}

	return 0;
}
#else
static int mtk_iommu_init_fn(struct device_node *np)
{
	int ret = 0;

	dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);
	ret = platform_driver_register(&mtk_iommu_driver);
	if (ret) {
		pr_notice("%s: Failed to register driver\n", __func__);
		return ret;
	}
#ifdef IOMMU_DEBUG_ENABLED
	g_tf_test = false;
#endif

	return 0;
}
#endif
IOMMU_OF_DECLARE(mtk_iommu, "mediatek,iommu_v0", mtk_iommu_init_fn);
#else

static int __init mtk_iommu_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_iommu_driver);
	if (ret != 0)
		pr_notice("Failed to register MTK IOMMU driver\n");
	else
		mtk_iommu_debug_init();

#ifdef IOMMU_DEBUG_ENABLED
	g_tf_test = false;
#endif

	return ret;
}

subsys_initcall(mtk_iommu_init);
#endif
