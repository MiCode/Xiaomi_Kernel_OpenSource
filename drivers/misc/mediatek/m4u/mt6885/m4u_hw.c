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

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/sched/clock.h>

#include "m4u_priv.h"
#include "m4u_platform.h"
#include "m4u_hw.h"

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <mt-plat/mtk_lpae.h>
#include <mt-plat/mtk_secure_api.h>

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

static struct m4u_domain_t gM4uDomain[TOTAL_M4U_NUM];

static unsigned long gM4UBaseAddr[TOTAL_M4U_NUM];
static unsigned long gLarbBaseAddr[SMI_LARB_NR];
static unsigned long gM4UtfAddr[TOTAL_M4U_NUM];
#if (TOTAL_M4U_NUM > 1)
static unsigned long gPericfgBaseAddr;
#endif
static unsigned int gM4UTagCount[TOTAL_M4U_NUM] = {64, 64};
static unsigned long gM4USecAddr[TOTAL_M4U_NUM];
static unsigned int M4USecIrq[TOTAL_M4U_NUM];
#ifdef M4U_MMU_SLAVE_SWITCH
static unsigned long gComBaseAddr;
#endif


/* static struct M4U_RANGE_DES_T gM4u0_seq[M4U0_SEQ_NR] = {{0}}; */

/* static struct M4U_RANGE_DES_T gM4u1_seq[M4U1_SEQ_NR] = {{0}}; */
/* static struct M4U_RANGE_DES_T *gM4USeq[] = { NULL, NULL }; */
/*static  struct M4U_MAU_STATUS_T gM4u0_mau[M4U0_MAU_NR] = { {0} };*/

/*static unsigned int gMAU_candidate_id = M4U0_MAU_NR - 1;*/

/* static DEFINE_MUTEX(gM4u_seq_mutex); */

static struct M4U_PROG_DIST_T gM4U0_prog_pfh[M4U0_PROG_PFH_NR] = { {0} };

/* static struct M4U_PROG_DIST_T gM4u1_prog_pfh[M4U0_PROG_PFH_NR] = {{0}}; */
static struct M4U_PROG_DIST_T *gM4UProgPfh[] = { gM4U0_prog_pfh, NULL };

static DEFINE_MUTEX(gM4u_prog_pfh_mutex);

#define TF_PROTECT_BUFFER_SIZE 256L

int gM4U_L2_enable = 1;
int gM4U_4G_DRAM_Mode;
unsigned int g_translation_fault_debug;

static spinlock_t gM4u_reg_lock[TOTAL_M4U_NUM];
int gM4u_port_num = M4U_PORT_UNKNOWN;

static DEFINE_MUTEX(m4u_larb0_mutex);

int m4u_invalid_tlb(int m4u_id, int L2_en,
		int isInvAll, unsigned int mva_start,
			unsigned int mva_end)
{
	unsigned int reg = 0;
	unsigned long m4u_base;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_id];
	if (mva_start >= mva_end)
		isInvAll = 1;

	if (!isInvAll) {
		mva_start = round_down(mva_start, SZ_4K);
		mva_end = round_up(mva_end, SZ_4K);
	}

	if (L2_en)
		reg = F_MMU_INV_EN_L2;

	reg |= F_MMU_INV_EN_L1;

	spin_lock(&gM4u_reg_lock[m4u_id]);
	M4U_WriteReg32(m4u_base, REG_INVLID_SEL, reg);

	if (isInvAll) {
		M4U_WriteReg32(m4u_base, REG_MMU_INVLD, F_MMU_INV_ALL);
	} else {
		M4U_WriteReg32(m4u_base, REG_MMU_INVLD_SA, mva_start);
		M4U_WriteReg32(m4u_base, REG_MMU_INVLD_EA, mva_end);
		M4U_WriteReg32(m4u_base, REG_MMU_INVLD, F_MMU_INV_RANGE);
	}

	if (!isInvAll) {
		while (!M4U_ReadReg32(m4u_base, REG_MMU_CPE_DONE))
			;
		M4U_WriteReg32(m4u_base, REG_MMU_CPE_DONE, 0);
	}

	spin_unlock(&gM4u_reg_lock[m4u_id]);

	return 0;
}

static void m4u_invalid_tlb_all(unsigned int m4u_id)
{
	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return;
	}

	m4u_invalid_tlb(m4u_id, gM4U_L2_enable, 1, 0, 0);
}

void m4u_invalid_tlb_by_range(struct m4u_domain_t *m4u_domain,
	unsigned int mva_start, unsigned int mva_end)
{
	/* to-do: should get m4u connected to domain here */
	m4u_invalid_tlb(m4u_domain->domain, gM4U_L2_enable,
		0, mva_start, mva_end);
}

static int __m4u_dump_rs_info(unsigned int va[],
	unsigned int pa[], unsigned int st[], unsigned int pte[])
{
	int i = 0;

	M4ULOG_MID("m4u dump RS information =====>\n");
	M4ULOG_MID(
		"id   mva   valid   larb/port   pa   pte   w/r  other-status\n");
	for (i = 0; i < MMU_TOTAL_RS_NR; i++) {
		M4ULOG_MID(
			"%d: 0x%8x %5d   %d/%d    0x%8x   0x%8x   %d  0x%3x\n",
				i,
				F_MMU_RSx_VA_GET(va[i]),
				F_MMU_RSx_VA_VALID(va[i]),
				F_MMU_RSx_ST_LARB(st[i]),
				F_MMU_RSx_ST_PORT(st[i]),
				pa[i], pte[i],
				F_MMU_RSx_ST_WRT(st[i]),
				F_MMU_RSx_ST_OTHER(st[i]));
	}
	M4ULOG_MID("m4u dump RS information done =====>\n");
	return 0;
}

int m4u_dump_rs_info(unsigned int m4u_index, int m4u_slave_id)
{
	unsigned long m4u_base = 0;
	int i = 0;
	unsigned int va[MMU_TOTAL_RS_NR];
	unsigned int pa[MMU_TOTAL_RS_NR];
	unsigned int st[MMU_TOTAL_RS_NR];
	unsigned int pte[MMU_TOTAL_RS_NR];

	if (unlikely(m4u_index >= TOTAL_M4U_NUM ||
	    m4u_slave_id >= M4U_SLAVE_NUM(m4u_index))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d\n",
		    __func__, __LINE__, m4u_index, m4u_slave_id);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_index];
	for (i = 0; i < MMU_TOTAL_RS_NR; i++) {
		va[i] = COM_ReadReg32((m4u_base +
			REG_MMU_RSx_VA(m4u_slave_id, i)));
		pa[i] = COM_ReadReg32((m4u_base +
			REG_MMU_RSx_PA(m4u_slave_id, i)));
		st[i] = COM_ReadReg32((m4u_base +
			REG_MMU_RSx_ST(m4u_slave_id, i)));
		pte[i] = COM_ReadReg32((m4u_base +
			REG_MMU_RSx_2ND_BASE(m4u_slave_id, i)));
	}

	M4ULOG_MID("m4u dump RS information index: %d=====>\n", m4u_slave_id);
	__m4u_dump_rs_info(va, pa, st, pte);
	M4ULOG_MID("m4u dump RS information done =====>\n");
	return 0;
}

static int m4u_dump_debug_reg_info(int m4u_index)
{
	unsigned long m4u_base = gM4UBaseAddr[m4u_index];
	int i;

	M4UMSG("m4u dump debug register\n");
	for (i = 0; i < DEBUG_REG_NR; i++) {
		int offset = 0x60 + 4 * i;

		M4UMSG("0x%x = 0x%x\n",
			offset, M4U_ReadReg32(m4u_base, offset));
	}

	return 0;
}

static int m4u_dump_rs_sta_info(int m4u_index, int mmu)
{
	unsigned long m4u_base = gM4UBaseAddr[m4u_index];
	int i;

	M4UMSG("m4u dump RS status register -- mmu%d\n", mmu);
	for (i = 0; i < MMU_TOTAL_RS_NR; i++) {
		int offset = 0x38c + 16 * i + (mmu)*MMU01_SQ_OFFSET;

		M4UMSG("0x%x = 0x%x\n",
			offset, M4U_ReadReg32(m4u_base, offset));
	}

	return 0;
}

static inline void m4u_clear_intr_sec(unsigned int m4u_base_sec)
{
	m4uHw_set_field_by_mask(m4u_base_sec,
		REG_MMU_INT_L2_CONTROL_SEC, F_INT_L2_CLR_BIT_SEC,
				F_INT_L2_CLR_BIT_SEC);
}

static inline void m4u_clear_intr(unsigned int m4u_id)
{
	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return;
	}

	m4uHw_set_field_by_mask(gM4UBaseAddr[m4u_id],
		REG_MMU_INT_L2_CONTROL, F_INT_L2_CLR_BIT,
				F_INT_L2_CLR_BIT);
}

static inline void m4u_enable_intr(unsigned int m4u_id)
{
	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return;
	}

	M4U_WriteReg32(gM4UBaseAddr[m4u_id],
		REG_MMU_INT_L2_CONTROL, 0x6f);
	M4U_WriteReg32(gM4UBaseAddr[m4u_id],
		REG_MMU_INT_MAIN_CONTROL, 0xffffffff);
}

static inline void m4u_disable_intr(unsigned int m4u_id)
{
	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return;
	}

	M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_INT_L2_CONTROL, 0);
	M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_INT_MAIN_CONTROL, 0);
}

static inline void m4u_intr_modify_all(unsigned long enable)
{
	int i = 0;

	for (i = 0; i < TOTAL_M4U_NUM; i++) {
		if (enable)
			m4u_enable_intr(i);
		else
			m4u_disable_intr(i);
	}
}

struct mau_config_info {
	int m4u_id;
	int m4u_slave_id;
	int mau_set;
	unsigned int start;
	unsigned int end;
	unsigned int port_mask;
	unsigned int larb_mask;
	unsigned int write_monitor;	/* :1; */
	unsigned int virt;	/* :1; */
	unsigned int io;	/* :1; */
	unsigned int start_bit32;	/* :1; */
	unsigned int end_bit32;	/* :1; */
};

int mau_start_monitor(unsigned int m4u_id,
	      int m4u_slave_id, unsigned int mau_set,
		      int wr, int vir, int io, int bit32,
		      unsigned int start, unsigned int end,
		      unsigned int port_mask, unsigned int larb_mask)
{
	unsigned long m4u_base;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM ||
	  mau_set >= MAU_NR_PER_M4U_SLAVE ||
	  m4u_slave_id >= M4U_SLAVE_NUM(m4u_id))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d, mau=%d\n",
		    __func__, __LINE__, m4u_id, m4u_slave_id, mau_set);
		return -1;
	}
	m4u_base = gM4UBaseAddr[m4u_id];

	M4ULOG_HIGH(
		"%s [%d], start=0x%x, end=0x%x, write: %d, port_mask=0x%x, larb_mask=0x%x\n",
		__func__,
		mau_set, start, end,
		wr, port_mask, larb_mask);

	M4U_WriteReg32(m4u_base,
		REG_MMU_MAU_START(m4u_slave_id, mau_set), start);
	M4U_WriteReg32(m4u_base,
		REG_MMU_MAU_START_BIT32(m4u_slave_id, mau_set), (bit32));
	M4U_WriteReg32(m4u_base,
		REG_MMU_MAU_END(m4u_slave_id, mau_set), end);
	M4U_WriteReg32(m4u_base,
		REG_MMU_MAU_END_BIT32(m4u_slave_id, mau_set),
		(bit32));

	M4U_WriteReg32(m4u_base,
		REG_MMU_MAU_PORT_EN(m4u_slave_id, mau_set),
		port_mask);

	m4uHw_set_field_by_mask(m4u_base, REG_MMU_MAU_LARB_EN(m4u_slave_id),
				F_MAU_LARB_MSK(mau_set),
				F_MAU_LARB_VAL(mau_set, larb_mask));

	m4uHw_set_field_by_mask(m4u_base, REG_MMU_MAU_IO(m4u_slave_id),
				F_MAU_BIT_VAL(1, mau_set),
				F_MAU_BIT_VAL(io, mau_set));

	m4uHw_set_field_by_mask(m4u_base, REG_MMU_MAU_RW(m4u_slave_id),
				F_MAU_BIT_VAL(1, mau_set),
				F_MAU_BIT_VAL(wr, mau_set));

	m4uHw_set_field_by_mask(m4u_base, REG_MMU_MAU_VA(m4u_slave_id),
				F_MAU_BIT_VAL(1, mau_set),
				F_MAU_BIT_VAL(vir, mau_set));

	return 0;
}
/* notes: must fill cfg->m4u_id/m4u_slave_id/mau_set before call this func. */
int mau_get_config_info(struct mau_config_info *cfg)
{
	unsigned int m4u_id = cfg->m4u_id;
	unsigned int m4u_slave_id = cfg->m4u_slave_id;
	unsigned int mau_set = cfg->mau_set;
	unsigned long m4u_base;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM ||
	  mau_set >= MAU_NR_PER_M4U_SLAVE ||
	  m4u_slave_id >= M4U_SLAVE_NUM(m4u_id))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d, mau=%d\n",
		    __func__, __LINE__, m4u_id, m4u_slave_id, mau_set);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_id];
	cfg->start = M4U_ReadReg32(m4u_base,
		REG_MMU_MAU_START(m4u_slave_id, mau_set));
	cfg->end = M4U_ReadReg32(m4u_base,
		REG_MMU_MAU_END(m4u_slave_id, mau_set));
	cfg->start_bit32 = M4U_ReadReg32(m4u_base,
		REG_MMU_MAU_START_BIT32(m4u_slave_id, mau_set));
	cfg->end_bit32 = M4U_ReadReg32(m4u_base,
		REG_MMU_MAU_START_BIT32(m4u_slave_id, mau_set));
	cfg->port_mask = M4U_ReadReg32(m4u_base,
		REG_MMU_MAU_PORT_EN(m4u_slave_id, mau_set));
	cfg->larb_mask =
		m4uHw_get_field_by_mask(m4u_base,
			REG_MMU_MAU_LARB_EN(m4u_slave_id),
			F_MAU_LARB_MSK(mau_set));

	cfg->io =
		!!(m4uHw_get_field_by_mask
		   (m4u_base, REG_MMU_MAU_IO(m4u_slave_id),
		   F_MAU_BIT_VAL(1, mau_set)));

	cfg->write_monitor =
		!!m4uHw_get_field_by_mask(m4u_base,
			REG_MMU_MAU_RW(m4u_slave_id),
			F_MAU_BIT_VAL(1, mau_set));

	cfg->virt =
		!!m4uHw_get_field_by_mask(m4u_base,
			REG_MMU_MAU_VA(m4u_slave_id),
				F_MAU_BIT_VAL(1, mau_set));

	return 0;
}

int __mau_dump_status(unsigned int m4u_id,
	int m4u_slave_id, unsigned int mau)
{
	unsigned long m4u_base;
	unsigned int status;
	unsigned int assert_id, assert_addr, assert_b32;
	int larb, port;
	struct mau_config_info mau_cfg;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM ||
	  mau >= MAU_NR_PER_M4U_SLAVE ||
	  m4u_slave_id >= M4U_SLAVE_NUM(m4u_id))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d, mau=%d\n",
		    __func__, __LINE__, m4u_id, m4u_slave_id, mau);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_id];
	status = M4U_ReadReg32(m4u_base, REG_MMU_MAU_ASSERT_ST(m4u_slave_id));

	if (status & (1 << mau)) {
		M4ULOG_HIGH("mau_assert in set %d\n", mau);
		assert_id = M4U_ReadReg32(m4u_base,
			REG_MMU_MAU_ASSERT_ID(m4u_slave_id, mau));
		assert_addr = M4U_ReadReg32(m4u_base,
			REG_MMU_MAU_ADDR(m4u_slave_id, mau));
		assert_b32 = M4U_ReadReg32(m4u_base,
			REG_MMU_MAU_ADDR_BIT32(m4u_slave_id, mau));
		larb = F_MMU_MAU_ASSERT_ID_LARB(assert_id);
		port = F_MMU_MAU_ASSERT_ID_PORT(assert_id);
		M4ULOG_HIGH("id=0x%x(%s),addr=0x%x,b32=0x%x\n", assert_id,
				m4u_get_port_name(
				larb_port_2_m4u_port(larb, port)),
				assert_addr, assert_b32);

		M4U_WriteReg32(m4u_base,
			REG_MMU_MAU_CLR(m4u_slave_id), (1 << mau));
		M4U_WriteReg32(m4u_base, REG_MMU_MAU_CLR(m4u_slave_id), 0);

		mau_cfg.m4u_id = m4u_id;
		mau_cfg.m4u_slave_id = m4u_slave_id;
		mau_cfg.mau_set = mau;
		mau_get_config_info(&mau_cfg);
		M4ULOG_HIGH(
			"mau_cfg: start=0x%x,end=0x%x,virt(%d),io(%d),wr(%d),s_b32(%d),e_b32(%d)\n",
		 mau_cfg.start, mau_cfg.end,
		 mau_cfg.virt, mau_cfg.io,
		 mau_cfg.write_monitor,
		 mau_cfg.start_bit32, mau_cfg.end_bit32);

	} else
		M4ULOG_MID("mau no assert in set %d\n", mau);

	return 0;
}

int mau_dump_status(unsigned int m4u_id, unsigned int m4u_slave_id)
{
	int i = 0;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM ||
	  m4u_slave_id >= M4U_SLAVE_NUM(m4u_id))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d\n",
		    __func__, __LINE__, m4u_id, m4u_slave_id);
		return -1;
	}

	for (i = 0; i < MAU_NR_PER_M4U_SLAVE; i++)
		__mau_dump_status(m4u_id, m4u_slave_id, i);

	return 0;
}

int m4u_dump_reg(unsigned int m4u_index, unsigned int start, unsigned int end)
{
	int i = 0;
	unsigned long m4u_base;

	if (unlikely(m4u_index >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_index);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_index];
	for (i = start; i < end; i += 16) {
		M4UINFO("0x%x=0x%x, 0x%x=0x%x, 0x%x=0x%x, 0x%x=0x%x\n",
			(i + 4 * 0), M4U_ReadReg32(m4u_base, i + 4 * 0),
			(i + 4 * 1), M4U_ReadReg32(m4u_base, i + 4 * 1),
			(i + 4 * 2), M4U_ReadReg32(m4u_base, i + 4 * 2),
			(i + 4 * 3), M4U_ReadReg32(m4u_base, i + 4 * 3));
	}

	return 0;
}

unsigned int m4u_get_main_descriptor(unsigned int m4u_id,
	unsigned int m4u_slave_id, int idx)
{
	unsigned int regValue = 0;
	unsigned long m4u_base = gM4UBaseAddr[m4u_id];

	if (unlikely(m4u_id >= TOTAL_M4U_NUM ||
	  m4u_slave_id >= M4U_SLAVE_NUM(m4u_id))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d\n",
		    __func__, __LINE__, m4u_id, m4u_slave_id);
		return 0;
	}

	regValue = F_READ_ENTRY_EN
		   | F_READ_ENTRY_MMx_MAIN(m4u_slave_id)
		   | F_READ_ENTRY_MAIN_IDX(idx);

	M4U_WriteReg32(m4u_base, REG_MMU_READ_ENTRY, regValue);
	while (M4U_ReadReg32(m4u_base, REG_MMU_READ_ENTRY) & F_READ_ENTRY_EN)
		;
	return M4U_ReadReg32(m4u_base, REG_MMU_DES_RDATA);
}

unsigned int m4u_get_main_tag(unsigned int m4u_id,
	unsigned int m4u_slave_id, int idx)
{
	unsigned long m4u_base;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM ||
	  m4u_slave_id >= M4U_SLAVE_NUM(m4u_id))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d\n",
		    __func__, __LINE__, m4u_id, m4u_slave_id);
		return 0;
	}

	m4u_base = gM4UBaseAddr[m4u_id];

	return M4U_ReadReg32(m4u_base, REG_MMU_MAIN_TAG(m4u_slave_id, idx));
}

void m4u_get_main_tlb(unsigned int m4u_id,
	unsigned int m4u_slave_id, int idx, struct mmu_tlb_t *pTlb)
{
	if (unlikely(m4u_id >= TOTAL_M4U_NUM ||
	  m4u_slave_id >= M4U_SLAVE_NUM(m4u_id))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d\n",
		    __func__, __LINE__, m4u_id, m4u_slave_id);
		return;
	}

	pTlb->tag = m4u_get_main_tag(m4u_id, m4u_slave_id, idx);
	pTlb->desc = m4u_get_main_descriptor(m4u_id, m4u_slave_id, idx);
}

unsigned int m4u_get_pfh_tlb(unsigned int m4u_id,
	int set, int page, int way, struct mmu_tlb_t *pTlb)
{
	unsigned int regValue = 0;
	unsigned long m4u_base;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return 0;
	}

	m4u_base = gM4UBaseAddr[m4u_id];
	regValue = F_READ_ENTRY_EN
		   | F_READ_ENTRY_PFH
		   | F_READ_ENTRY_PFH_IDX(set)
		   | F_READ_ENTRY_PFH_PAGE_IDX(page)
		   | F_READ_ENTRY_PFH_WAY(way);

	M4U_WriteReg32(m4u_base, REG_MMU_READ_ENTRY, regValue);
	while (M4U_ReadReg32(m4u_base, REG_MMU_READ_ENTRY) & F_READ_ENTRY_EN)
		;
	pTlb->desc = M4U_ReadReg32(m4u_base, REG_MMU_DES_RDATA);
	pTlb->tag = M4U_ReadReg32(m4u_base, REG_MMU_PFH_TAG_RDATA);

	return 0;
}

unsigned int m4u_get_pfh_tag(unsigned int m4u_id, int set, int page, int way)
{
	struct mmu_tlb_t tlb;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return 0;
	}

	m4u_get_pfh_tlb(m4u_id, set, page, way, &tlb);
	return tlb.tag;
}

unsigned int m4u_get_pfh_descriptor(unsigned int m4u_id,
	int set, int page, int way)
{
	struct mmu_tlb_t tlb;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return 0;
	}

	m4u_get_pfh_tlb(m4u_id, set, page, way, &tlb);
	return tlb.desc;
}

int m4u_dump_main_tlb(unsigned int m4u_id, int m4u_slave_id)
{
	/* M4U related */
	unsigned int i = 0;
	struct mmu_tlb_t tlb;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM ||
	  m4u_slave_id >= M4U_SLAVE_NUM(m4u_id))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d\n",
		    __func__, __LINE__, m4u_id, m4u_slave_id);
		return -1;
	}

	M4ULOG_HIGH("dump main tlb: m4u %d  ====>\n", m4u_id);
	for (i = 0; i < gM4UTagCount[m4u_id]; i++) {
		m4u_get_main_tlb(m4u_id, m4u_slave_id, i, &tlb);
		M4ULOG_HIGH("%d:0x%x:0x%x  ", i, tlb.tag, tlb.desc);
		if ((i + 1) % 8 == 0)
			M4ULOG_HIGH("===\n");
	}

	return 0;
}

int m4u_dump_valid_main0_tlb(unsigned int m4u_id, int m4u_slave_id)
{
	unsigned int i = 0;
	struct mmu_tlb_t tlb;

	M4UMSG("%s +\n", __func__);
	for (i = 0; i < gM4UTagCount[m4u_id]; i++) {
		m4u_get_main_tlb(m4u_id, m4u_slave_id, i, &tlb);
		if ((tlb.tag & F_MAIN_TLB_VALID_BIT) == F_MAIN_TLB_VALID_BIT)
			M4ULOG_HIGH("%d:0x%x:0x%x\n", i, tlb.tag, tlb.desc);

	}
	M4ULOG_HIGH("%s -\n", __func__);

	return 0;
}

#if 1
unsigned int m4u_get_main1_descriptor(int m4u_id, int m4u_slave_id, int idx)
{
	unsigned int regValue = 0;
	unsigned long m4u_base = gM4UBaseAddr[m4u_id];

	regValue = F_READ_ENTRY_EN
		   | F_READ_ENTRY_MMx_MAIN(m4u_slave_id)
		   | F_READ_ENTRY_MMU1_IDX(idx);/* mmu1 */

	M4U_WriteReg32(m4u_base, REG_MMU_READ_ENTRY, regValue);
	while (M4U_ReadReg32(m4u_base, REG_MMU_READ_ENTRY) & F_READ_ENTRY_EN)
		;
	return M4U_ReadReg32(m4u_base, REG_MMU_DES_RDATA);
}

void m4u_get_main1_tlb(int m4u_id, int m4u_slave_id, int idx,
		struct mmu_tlb_t *pTlb)
{
	pTlb->tag = m4u_get_main_tag(m4u_id, m4u_slave_id, idx);
	pTlb->desc = m4u_get_main1_descriptor(m4u_id, m4u_slave_id, idx);
}

int m4u_dump_valid_main1_tlb(int m4u_id, int m4u_2nd_id)
{
	unsigned int i = 0;
	struct mmu_tlb_t tlb;

	M4UMSG("dump main tlb start %d -- %d\n", m4u_id, m4u_2nd_id);
	for (i = 0; i < gM4UTagCount[m4u_id]; i++) {
		m4u_get_main1_tlb(m4u_id, m4u_2nd_id, i, &tlb);
		if ((tlb.tag & F_MAIN_TLB_VALID_BIT) == F_MAIN_TLB_VALID_BIT)
			M4ULOG_HIGH("%d:0x%x:0x%x\n", i, tlb.tag, tlb.desc);

	}
	M4UMSG("dump mmu1 main tlb end\n");

	return 0;
}

int dump_fault_mva_pfh_tlb(int m4u_id, unsigned int mva)
{
	int set;
	int way, page, valid;
	struct mmu_tlb_t tlb;
	unsigned int regval;

	set = (mva >> 15) & 0x7f;
	for (way = 0; way < MMU_WAY_NR; way++) {
		for (page = 0; page < MMU_PAGE_PER_LINE; page++) {
			regval = M4U_ReadReg32(gM4UBaseAddr[m4u_id],
				REG_MMU_PFH_VLD(m4u_id, set, way));
			valid = !!(regval & F_MMU_PFH_VLD_BIT(set, way));

			m4u_get_pfh_tlb(m4u_id, set, page, way, &tlb);
			M4UMSG(
				"fault_mva:0x%x, way:%d, set:%d, page:%d, valid:%d--0x%x, tag:0x%x, des:0x%x\n",
				mva, way, set, page, valid,
				regval, tlb.tag, tlb.desc);
		}
	}

	return 0;
}
#endif

static unsigned int imu_pfh_tag_to_va(int mmu,
		int set, int way, unsigned int tag)
{
	unsigned int tmp;

	if (tag & F_PFH_TAG_LAYER_BIT)
		return F_PFH_TAG_VA_GET(mmu, tag) | ((set) << 15);

	tmp = F_PFH_TAG_VA_GET(mmu, tag);
	tmp &= F_MMU_PFH_TAG_VA_LAYER0_MSK(mmu);
	tmp |= (set) << 23;
	return tmp;

}

int m4u_dump_pfh_tlb(unsigned int m4u_id)
{
	unsigned int regval;
	unsigned long m4u_base;
	int result = 0;
	int set_nr, way_nr, set, way;
	int valid;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_id];
	set_nr = MMU_SET_NR(m4u_id);
	way_nr = MMU_WAY_NR;

	M4ULOG_HIGH("dump pfh_tlb: m4u %d  ====>\n", m4u_id);

	for (way = 0; way < way_nr; way++) {
		for (set = 0; set < set_nr; set++) {
			int page;
			struct mmu_tlb_t tlb;

			regval = M4U_ReadReg32(m4u_base,
				REG_MMU_PFH_VLD(m4u_id, set, way));
			valid = !!(regval & F_MMU_PFH_VLD_BIT(set, way));
			m4u_get_pfh_tlb(m4u_id, set, 0, way, &tlb);
			M4ULOG_HIGH(
				"va(0x%x) lay(%d) 16x(%d) sec(%d) pfh(%d) v(%d),set(%d),way(%d), 0x%x:",
			 imu_pfh_tag_to_va(m4u_id, set, way, tlb.tag),
			 !!(tlb.tag & F_PFH_TAG_LAYER_BIT),
			 !!(tlb.tag & F_PFH_TAG_16X_BIT),
			 !!(tlb.tag & F_PFH_TAG_SEC_BIT),
			 !!(tlb.tag & F_PFH_TAG_AUTO_PFH),
			 valid, set, way, tlb.desc);

			for (page = 1; page < MMU_PAGE_PER_LINE; page++) {
				m4u_get_pfh_tlb(m4u_id, set, page, way, &tlb);
				M4ULOG_HIGH("0x%x:", tlb.desc);
			}
			M4ULOG_HIGH("\n");
		}
	}

	return result;
}

int m4u_get_pfh_tlb_all(unsigned int m4u_id,
		struct mmu_pfh_tlb_t *pfh_buf)
{
	unsigned int regval;
	unsigned long m4u_base;
	int set_nr, way_nr, set, way;
	int valid;
	int pfh_id = 0;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_id];
	set_nr = MMU_SET_NR(m4u_id);
	way_nr = MMU_WAY_NR;

	for (way = 0; way < way_nr; way++) {
		for (set = 0; set < set_nr; set++) {
			int page;
			struct mmu_tlb_t tlb;

			regval = M4U_ReadReg32(m4u_base,
				REG_MMU_PFH_VLD(m4u_id, set, way));
			valid = !!(regval & F_MMU_PFH_VLD_BIT(set, way));
			m4u_get_pfh_tlb(m4u_id, set, 0, way, &tlb);

			pfh_buf[pfh_id].tag = tlb.tag;
			pfh_buf[pfh_id].va =
				imu_pfh_tag_to_va(m4u_id,
					set, way, tlb.tag);
			pfh_buf[pfh_id].layer =
				!!(tlb.tag & F_PFH_TAG_LAYER_BIT);
			pfh_buf[pfh_id].x16 = !!(tlb.tag & F_PFH_TAG_16X_BIT);
			pfh_buf[pfh_id].sec = !!(tlb.tag & F_PFH_TAG_SEC_BIT);
			pfh_buf[pfh_id].pfh = !!(tlb.tag & F_PFH_TAG_AUTO_PFH);
			pfh_buf[pfh_id].set = set;
			pfh_buf[pfh_id].way = way;
			pfh_buf[pfh_id].valid = valid;
			pfh_buf[pfh_id].desc[0] = tlb.desc;
			pfh_buf[pfh_id].page_size =
				pfh_buf[pfh_id].layer ?
					MMU_SMALL_PAGE_SIZE : MMU_SECTION_SIZE;

			for (page = 1; page < MMU_PAGE_PER_LINE; page++) {
				m4u_get_pfh_tlb(m4u_id, set, page, way, &tlb);
				pfh_buf[pfh_id].desc[page] = tlb.desc;
			}
			pfh_id++;
		}
	}

	return 0;
}

unsigned int m4u_get_victim_tlb(unsigned int m4u_id,
	int page, int way, struct mmu_tlb_t *pTlb)
{
	unsigned int regValue = 0;
	unsigned long m4u_base;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_id];
	regValue = F_READ_ENTRY_EN
		   | F_READ_ENTRY_VICT_TLB_SEL
		   | F_READ_ENTRY_PFH_PAGE_IDX(page)
		   | F_READ_ENTRY_PFH_WAY(way);

	M4U_WriteReg32(m4u_base, REG_MMU_READ_ENTRY, regValue);
	while (M4U_ReadReg32(m4u_base, REG_MMU_READ_ENTRY) & F_READ_ENTRY_EN)
		;
	pTlb->desc = M4U_ReadReg32(m4u_base, REG_MMU_DES_RDATA);
	pTlb->tag = M4U_ReadReg32(m4u_base, REG_MMU_PFH_TAG_RDATA);

	return 0;
}

static unsigned int imu_victim_tag_to_va(int mmu, int way, unsigned int tag)
{
	unsigned int tmp;

	if (tag & F_PFH_TAG_LAYER_BIT)
		return F_PFH_TAG_VA_GET(mmu, tag);

	tmp = F_PFH_TAG_VA_GET(mmu, tag);
	tmp &= F_MMU_PFH_TAG_VA_LAYER0_MSK(mmu);
	return tmp;

}

int m4u_dump_victim_tlb(unsigned int m4u_id)
{
	unsigned int regval;
	int result = 0;
	int way_nr, way;
	int valid;
	unsigned long m4u_base;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_id];
	way_nr = MMU_WAY_NR;

	M4ULOG_HIGH("dump victim_tlb: m4u %d  ====>\n", m4u_id);

	for (way = 0; way < way_nr; way++) {
		int page;
		struct mmu_tlb_t tlb;

		regval = M4U_ReadReg32(m4u_base, REG_MMU_VICT_VLD);
		valid = !!(regval & F_MMU_VICT_VLD_BIT(way));
		m4u_get_victim_tlb(m4u_id, 0, way, &tlb);
		M4ULOG_HIGH
		("way(%d), v(%d),:", way, valid);

		for (page = 0; page < MMU_PAGE_PER_LINE; page++) {
			m4u_get_victim_tlb(m4u_id, page, way, &tlb);
			M4ULOG_HIGH(
				"va(0x%x) lay(%d) Bit32(%d) sec(%d) pfh(%d), 0x%x: 0x%x",
			imu_victim_tag_to_va(m4u_id, way, tlb.tag),
			!!(tlb.tag & F_PFH_TAG_LAYER_BIT),
			!!(tlb.tag & F_PFH_TAG_16X_BIT),
			!!(tlb.tag & F_PFH_TAG_SEC_BIT),
			!!(tlb.tag & F_PFH_TAG_AUTO_PFH),
			tlb.tag, tlb.desc);

		}
	}

	return result;
}

int m4u_confirm_main_range_invalidated(
		int m4u_index,
		int m4u_slave_id, unsigned int MVAStart,
				       unsigned int MVAEnd)
{
	unsigned int i;
	unsigned int regval;
	if (unlikely(m4u_index >= TOTAL_M4U_NUM ||
	 m4u_slave_id >= M4U_SLAVE_NUM(m4u_index))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d\n",
		    __func__, __LINE__, m4u_index, m4u_slave_id);
		return -1;
	}

	/* /> check Main TLB part */
	for (i = 0; i < gM4UTagCount[m4u_index]; i++) {
		regval = m4u_get_main_tag(m4u_index, m4u_slave_id, i);

		if (regval & (F_MAIN_TLB_VALID_BIT)) {
			unsigned int tag_s, tag_e, sa, ea;
			int layer = regval & F_MAIN_TLB_LAYER_BIT;
			int large = regval & F_MAIN_TLB_16X_BIT;

			tag_s = regval & F_MAIN_TLB_VA_MSK;
			sa = MVAStart & (~(PAGE_SIZE - 1));
			ea = MVAEnd | (PAGE_SIZE - 1);

			if (layer) {	/* pte */
				if (large)
					tag_e = tag_s + MMU_LARGE_PAGE_SIZE - 1;
				else
					tag_e = tag_s + PAGE_SIZE - 1;

				if (!((tag_e < sa) || (tag_s > ea))) {
					M4UERR(
						"main: i=%d, idx=0x%x, MVAStart=0x%x, MVAEnd=0x%x, RegValue=0x%x\n",
					 i, m4u_index, MVAStart,
					 MVAEnd, regval);
					return -1;
				}

			} else {
				if (large)
					tag_e =
						tag_s +
							MMU_SUPERSECTION_SIZE -
								1;
				else
					tag_e =
						tag_s +
							MMU_SECTION_SIZE -
								1;

				if ((tag_s >= sa) && (tag_e <= ea)) {
					M4UERR(
						"main: i=%d, idx=0x%x, MVAStart=0x%x, MVAEnd=0x%x, RegValue=0x%x\n",
					 i, m4u_index,
					 MVAStart, MVAEnd, regval);
					return -1;
				}
			}
		}
	}
	return 0;
}

int m4u_confirm_range_invalidated(unsigned int m4u_index,
		unsigned int MVAStart, unsigned int MVAEnd)
{
	unsigned int i = 0;
	unsigned int regval;
	int result = 0;
	int set_nr, way_nr, set, way;
	unsigned long m4u_base;

	if (unlikely(m4u_index >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_index);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_index];
	/* /> check Main TLB part */
	result =
		m4u_confirm_main_range_invalidated(
		m4u_index, 0, MVAStart, MVAEnd);
	if (result < 0)
		return -1;

	if (m4u_index == 0) {
		result =
			m4u_confirm_main_range_invalidated(
				m4u_index, 1, MVAStart, MVAEnd);
		if (result < 0)
			return -1;
	}

	set_nr = MMU_SET_NR(m4u_index);
	way_nr = MMU_WAY_NR;

	for (way = 0; way < way_nr; way++) {
		for (set = 0; set < set_nr; set++) {
			regval =
				M4U_ReadReg32(m4u_base,
					REG_MMU_PFH_VLD(m4u_index, set, way));
			if (regval & F_MMU_PFH_VLD_BIT(set, way)) {
				unsigned int tag =
					m4u_get_pfh_tag(m4u_index,
						set, 0, way);
				unsigned int tag_s, tag_e, sa, ea;
				int layer = tag & F_PFH_TAG_LAYER_BIT;
				int large = tag & F_PFH_TAG_16X_BIT;

				tag_s = imu_pfh_tag_to_va(m4u_index,
						set, way, tag);

				sa = MVAStart & (~(PAGE_SIZE - 1));
				ea = MVAEnd | (PAGE_SIZE - 1);

				if (layer) {	/* pte */
					if (large)
						tag_e =
							tag_s +
							MMU_LARGE_PAGE_SIZE * 8
									- 1;
					else
						tag_e =
							tag_s +
							PAGE_SIZE * 8 - 1;

					if (!((tag_e < sa) || (tag_s > ea))) {
						M4UERR(
							"main: i=%d, idx=0x%x, MVAStart=0x%x, MVAEnd=0x%x, RegValue=0x%x\n",
						 i, m4u_index, MVAStart,
						 MVAEnd, regval);
						return -1;
					}

				} else {
					if (large)
						tag_e =
						tag_s +
						MMU_SUPERSECTION_SIZE * 8
								- 1;
					else
						tag_e =
						tag_s +
						MMU_SECTION_SIZE * 8 -
						1;

					/* if((tag_s>=sa)&&(tag_e<=ea)) */
					if (!((tag_e < sa) || (tag_s > ea))) {
						M4UERR(
							"main: i=%d, idx=0x%x, MVAStart=0x%x, MVAEnd=0x%x, RegValue=0x%x\n",
						 i, m4u_index, MVAStart,
						 MVAEnd, regval);
						return -1;
					}
				}
			}
		}
	}

	return result;
}

int m4u_confirm_main_all_invalid(unsigned int m4u_index, int m4u_slave_id)
{
	unsigned int i = 0;
	unsigned int regval;

	if (unlikely(m4u_index >= TOTAL_M4U_NUM ||
	 m4u_slave_id >= M4U_SLAVE_NUM(m4u_index))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d\n",
		    __func__, __LINE__, m4u_index, m4u_slave_id);
		return -1;
	}

	for (i = 0; i < gM4UTagCount[m4u_index]; i++) {
		regval = m4u_get_main_tag(m4u_index, m4u_slave_id, i);

		if (regval & (F_MAIN_TLB_VALID_BIT)) {
			M4UERR(
				"main: i=%d, idx=0x%x, RegValue=0x%x\n",
				i, m4u_index, regval);
			return -1;
		}
	}
	return 0;
}

int m4u_confirm_pfh_all_invalid(unsigned int m4u_index)
{
	unsigned int regval;
	int set_nr, way_nr, set, way;
	unsigned long m4u_base;

	if (unlikely(m4u_index >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_index);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_index];
	set_nr = MMU_SET_NR(m4u_index);
	way_nr = MMU_WAY_NR;

	for (way = 0; way < way_nr; way++) {
		for (set = 0; set < set_nr; set++) {
			regval = M4U_ReadReg32(m4u_base,
				REG_MMU_PFH_VLD(m4u_index, set, way));
			if (regval & F_MMU_PFH_VLD_BIT(set, way))
				return -1;

		}
	}
	return 0;
}

int m4u_confirm_all_invalidated(unsigned int m4u_index)
{
	if (unlikely(m4u_index >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_index);
		return -1;
	}

	if (m4u_confirm_main_all_invalid(m4u_index, 0))
		return -1;

	if (m4u_index == 0) {
		if (m4u_confirm_main_all_invalid(m4u_index, 1))
			return -1;
	}

	if (m4u_confirm_pfh_all_invalid(m4u_index))
		return -1;

	return 0;
}

int m4u_power_on(int m4u_index)
{
	return 0;
}

int m4u_power_off(int m4u_index)
{
	return 0;
}

static int m4u_clock_on(void)
{

	return 0;
}

char *smi_clk_name[SMI_LARB_NR] = {
	"m4u_smi_larb0", "m4u_smi_larb1", "m4u_smi_larb2",
	"m4u_smi_larb3", "m4u_smi_larb4", "m4u_smi_larb5",
	"m4u_smi_larb6", "m4u_smi_larb7", "m4u_smi_larb8",
	"m4u_smi_larb9", "m4u_smi_larb10"
};

int larb_clock_on(int larb, bool config_mtcmos)
{
#ifdef CONFIG_MTK_SMI_EXT
	int ret = -1;

	if (larb < SMI_LARB_NR)
		ret =
		    smi_bus_prepare_enable(larb, smi_clk_name[larb]);
	if (ret != 0)
		M4UMSG("%s error: larb %d\n", __func__, larb);
#endif

	return 0;
}


int larb_clock_off(int larb, bool config_mtcmos)
{
#ifdef CONFIG_MTK_SMI_EXT
	int ret = -1;

	if (larb < SMI_LARB_NR)
		ret =
		    smi_bus_disable_unprepare(larb, smi_clk_name[larb]);
	if (ret != 0)
		M4UMSG("%s error: larb %d\n", __func__, larb);
#endif

	return 0;
}

int m4u_enable_prog_dist_by_id(unsigned int port, int id)
{
	unsigned long m4u_base;
	unsigned int m4u_id = m4u_port_2_m4u_id(port);
	unsigned int m4u_slave_id = m4u_port_2_m4u_slave_id(port);

	if (unlikely(m4u_id >= TOTAL_M4U_NUM ||
	    m4u_slave_id >= M4U_MAX_SLAVE)) {
		M4UMSG("%s, invalid port=%d, slave=%d, m4u_id=%d\n",
			__func__, port, m4u_slave_id, m4u_id);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_id];
	spin_lock(&gM4u_reg_lock[m4u_id]);
	m4uHw_set_field_by_mask(m4u_base,
	     REG_MMU_PROG_DIST(id, m4u_slave_id), F_PF_EN(1), 1);
	spin_unlock(&gM4u_reg_lock[m4u_id]);

	return 0;
}

int m4u_disable_prog_dist_by_id(unsigned int port, int id)
{
	unsigned long m4u_base;
	unsigned int m4u_id = m4u_port_2_m4u_id(port);
	unsigned int m4u_slave_id = m4u_port_2_m4u_slave_id(port);

	if (unlikely(m4u_id >= TOTAL_M4U_NUM ||
	    m4u_slave_id >= M4U_MAX_SLAVE)) {
		M4UMSG("%s, invalid port=%d, slave=%d, m4u_id=%d\n",
			__func__, port, m4u_slave_id, m4u_id);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_id];

	spin_lock(&gM4u_reg_lock[m4u_id]);
	m4uHw_set_field_by_mask(m4u_base,
	   REG_MMU_PROG_DIST(id, m4u_slave_id), F_PF_EN(1), 0);
	spin_unlock(&gM4u_reg_lock[m4u_id]);

	return 0;
}

int m4u_config_prog_dist(unsigned int port,
	int dir, int dist, int en, unsigned int mm_id, int sel)
{
	int i, free_id = -1;
	unsigned int m4u_index = m4u_port_2_m4u_id(port);
	unsigned int m4u_slave_id = m4u_port_2_m4u_slave_id(port);
	unsigned int larb = m4u_port_2_larb_id(port);
	unsigned int larb_port = m4u_port_2_larb_port(port);
	unsigned long m4u_base;
	unsigned long larb_base;
	struct M4U_PROG_DIST_T *pProgPfh;

	if (unlikely(m4u_index >= TOTAL_M4U_NUM ||
	    m4u_slave_id >= M4U_MAX_SLAVE ||
	    larb >= SMI_LARB_NR)) {
		M4UMSG("%s, invalid port=%d, larb=%d, m4u_id=%d\n",
			__func__, port, larb, m4u_index);
		return -1;
	}

	larb_base = gLarbBaseAddr[larb];
	m4u_base = gM4UBaseAddr[m4u_index];
	pProgPfh = gM4UProgPfh[m4u_index] +
		M4U_PROG_PFH_NUM(m4u_index) * m4u_slave_id;

	mutex_lock(&gM4u_prog_pfh_mutex);

	for (i = 0; i < M4U_PROG_PFH_NUM(m4u_index); i++) {
		if (pProgPfh[i].Enabled == 1) {
			if (port == pProgPfh[i].port &&
					(sel == 0 || pProgPfh[i].sel == 0)) {
				M4UMSG(
					"m4u warning: cannot set two direction or difference distance in the same port.\n");
				M4UMSG(
					"original value: module = %s, mm_id = %d, dir = %d, dist = %d, sel = %d.\n",
				 m4u_get_port_name(port), mm_id,
				 dir, dist, sel);
				M4UMSG(
					"new value: module = %s, mm_id = %d, dir = %d, dist = %d, sel = %d.\n",
				 m4u_get_port_name(pProgPfh[i].port),
				 pProgPfh[i].mm_id,
				 pProgPfh[i].dir,
				 pProgPfh[i].dist, pProgPfh[i].sel);
				free_id = i;
				break;
			}
		} else {
			free_id = i;
			break;
		}
	}

	if (free_id == -1) {
		M4ULOG_MID("warning: can not find available prog pfh reg.\n");
		mutex_unlock(&gM4u_prog_pfh_mutex);
		return -1;
	}

	pProgPfh[free_id].Enabled = 1;
	pProgPfh[free_id].port = port;
	pProgPfh[free_id].mm_id = mm_id;
	pProgPfh[free_id].dir = dir;
	pProgPfh[free_id].dist = dist;
	pProgPfh[free_id].en = en;
	pProgPfh[free_id].sel = sel;
	mutex_unlock(&gM4u_prog_pfh_mutex);

	spin_lock(&gM4u_reg_lock[m4u_index]);

	m4uHw_set_field_by_mask(m4u_base,
		REG_MMU_PROG_DIST(free_id, m4u_slave_id), F_PF_ID_COMP_SEL(1),
				F_PF_ID_COMP_SEL(!!(sel)));

	m4uHw_set_field_by_mask(m4u_base,
		REG_MMU_PROG_DIST(free_id, m4u_slave_id), F_PF_DIR(1),
				F_PF_DIR(!!(dir)));

	m4uHw_set_field(m4u_base, REG_MMU_PROG_DIST(free_id, m4u_slave_id),
		F_PF_DIST_MSB - F_PF_DIST_LSB + 1, F_PF_DIST_LSB, dist);

	m4uHw_set_field(m4u_base, REG_MMU_PROG_DIST(free_id, m4u_slave_id),
		F_PF_ID_MSB - F_PF_ID_LSB + 1,
		F_PF_ID_LSB, F_PF_ID(larb, larb_port, mm_id));

	m4uHw_set_field_by_mask(m4u_base,
		REG_MMU_PROG_DIST(free_id, m4u_slave_id),
			F_PF_EN(1), F_PF_EN(!!(en)));

	spin_unlock(&gM4u_reg_lock[m4u_index]);

	return free_id;
}

int m4u_invalid_prog_dist_by_id(int port)
{
	int i = 0;
	unsigned int m4u_index = m4u_port_2_m4u_id(port);
	unsigned int m4u_slave_id = m4u_port_2_m4u_slave_id(port);
	unsigned long m4u_base;
	struct M4U_PROG_DIST_T *pProgPfh;

	if (unlikely(m4u_index == 1 ||
	     m4u_index >= TOTAL_M4U_NUM ||
	     m4u_slave_id >= M4U_MAX_SLAVE)) {
		M4UMSG("%s, invalid port=%d, m4u=%d\n",
			__func__, port, m4u_index);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_index];
	pProgPfh = gM4UProgPfh[m4u_index] +
		M4U_PROG_PFH_NUM(m4u_index) * m4u_slave_id;
	mutex_lock(&gM4u_prog_pfh_mutex);
	for (i = 0; i < M4U_PROG_PFH_NUM(m4u_index); i++) {
		if (pProgPfh[i].Enabled == 1) {
			if (port == pProgPfh[i].port && pProgPfh[i].sel == 0) {
				pProgPfh[i].Enabled = 0;
				break;
			}
		}
	}
	mutex_unlock(&gM4u_prog_pfh_mutex);

	if (i == M4U_PROG_PFH_NUM(m4u_index)) {
		/* M4UMSG (
		 * "m4u info: m4u_invalid_prog_dist_by_id cannot
		 * found proj dist set for port %d.\n", port);
		 */
		return -1;
	}

	spin_lock(&gM4u_reg_lock[m4u_index]);
	/* set to default value */
	M4U_WriteReg32(m4u_base, REG_MMU_PROG_DIST(i, m4u_slave_id), 0x10000);
	spin_unlock(&gM4u_reg_lock[m4u_index]);

	return 0;
}

int m4u_insert_seq_range(unsigned int port,
		unsigned int MVAStart, unsigned int MVAEnd)
{
#if 0
	int i, free_id = -1;
	unsigned int m4u_index = m4u_port_2_m4u_id(port);
	unsigned int m4u_slave_id =
		m4u_port_2_m4u_slave_id(port);
	struct M4U_RANGE_DES_T *pSeq;

	if (unlikely(m4u_index >= TOTAL_M4U_NUM ||
	    m4u_slave_id >= M4U_MAX_SLAVE)) {
		M4UMSG("%s, invalid port=%d, m4u=%d\n",
			__func__, port, m4u_index);
		return -1;
	}

	pSeq = gM4USeq[m4u_index] +
		M4U_SEQ_NUM(m4u_index) * m4u_slave_id;
	M4ULOG_MID(
		"%s , module:%s, MVAStart:0x%x, MVAEnd:0x%x\n",
			__func__,
			m4u_get_port_name(port), MVAStart, MVAEnd);

	if (MVAEnd - MVAStart < PAGE_SIZE) {
		M4ULOG_MID(
			"too small size, skip to insert! module:%s, MVAStart:0x%x, size:%d\n",
			   m4u_get_port_name(port),
			   MVAStart, MVAEnd - MVAStart + 1);
		return free_id;
	}
	/* =============================================== */
	/* every seq range has to align to 1M Bytes */
	MVAStart &= ~M4U_SEQ_ALIGN_MSK;
	MVAEnd |= M4U_SEQ_ALIGN_MSK;

	mutex_lock(&gM4u_seq_mutex);

	/* ================================================================== */
	/* check if the range is overlap with previous ones */

	for (i = 0; i < M4U_SEQ_NUM(m4u_index); i++) {
		if (pSeq[i].Enabled == 1) {
			if (MVAEnd < pSeq[i].MVAStart ||
				MVAStart > pSeq[i].MVAEnd)     /* no overlap */
				continue;
			else {
				M4ULOG_HIGH(
					"insert range overlap!: larb=%d,module=%s\n",
					    m4u_port_2_larb_id(port),
					    m4u_get_port_name(port));
				M4ULOG_HIGH(
					"warning: tlb range overlapped with previous ranges, current process=%s,!\n",
				 current->comm);
				M4ULOG_HIGH(
					"module=%s, mva_start=0x%x, mva_end=0x%x\n",
					    m4u_get_port_name(port),
					    MVAStart, MVAEnd);
				M4ULOG_HIGH(
					"overlapped range id=%d, module=%s, mva_start=0x%x, mva_end=0x%x\n",
				 i, m4u_get_port_name(pSeq[i].port),
				 pSeq[i].MVAStart,
				 pSeq[i].MVAEnd);
				mutex_unlock(&gM4u_seq_mutex);
				return -1;
			}
		} else
			free_id = i;
	}

	if (free_id == -1) {
		M4ULOG_MID("warning: can not find available range\n");
		mutex_unlock(&gM4u_seq_mutex);
		return -1;
	}
	/* record range information in array */
	pSeq[free_id].Enabled = 1;
	pSeq[free_id].port = port;
	pSeq[free_id].MVAStart = MVAStart;
	pSeq[free_id].MVAEnd = MVAEnd;

	mutex_unlock(&gM4u_seq_mutex);

	/* set the range register */

	MVAStart &= F_SQ_VA_MASK;
	MVAStart |= F_SQ_EN_BIT;
	/* align mva end to 1M */
	MVAEnd |= ~F_SQ_VA_MASK;

	spin_lock(&gM4u_reg_lock[m4u_index]);
	{
		M4U_WriteReg32(gM4UBaseAddr[m4u_index],
				REG_MMU_SQ_START(m4u_slave_id, free_id),
				MVAStart);
		M4U_WriteReg32(gM4UBaseAddr[m4u_index],
				REG_MMU_SQ_END(m4u_slave_id, free_id),
				MVAEnd);
	}
	spin_unlock(&gM4u_reg_lock[m4u_index]);

	return free_id;
#else
	return 0;
#endif
}

int m4u_invalid_seq_range_by_id(unsigned int port, int seq_id)
{

#if 0
	unsigned int m4u_index = m4u_port_2_m4u_id(port);
	unsigned int m4u_slave_id = m4u_port_2_m4u_slave_id(port);
	unsigned long m4u_base;
	struct M4U_RANGE_DES_T *pSeq;
	int ret = 0;

	if (unlikely(m4u_index >= TOTAL_M4U_NUM ||
	    m4u_slave_id >= M4U_MAX_SLAVE)) {
		M4UMSG("%s, invalid port=%d, m4u=%d\n",
			__func__, port, m4u_index);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_index];
	pSeq = gM4USeq[m4u_index] +
			M4U_SEQ_NUM(m4u_index) * m4u_slave_id;
	mutex_lock(&gM4u_seq_mutex);
	{
		pSeq[seq_id].Enabled = 0;
	}
	mutex_unlock(&gM4u_seq_mutex);

	spin_lock(&gM4u_reg_lock[m4u_index]);
	M4U_WriteReg32(m4u_base, REG_MMU_SQ_START(m4u_slave_id, seq_id), 0);
	M4U_WriteReg32(m4u_base, REG_MMU_SQ_END(m4u_slave_id, seq_id), 0);
	spin_unlock(&gM4u_reg_lock[m4u_index]);

	return ret;
#else
	return 0;
#endif
}

static int _m4u_config_port(unsigned int port,
	int virt, int sec, int dis, int dir)
{
	unsigned int m4u_index = m4u_port_2_m4u_id(port);
	/*  unsigned long m4u_base = gM4UBaseAddr[m4u_index]; */
	unsigned long larb_base;
	unsigned int larb, larb_port;
	int ret = 0;

	if (unlikely(m4u_index >= TOTAL_M4U_NUM)) {
		M4UMSG("%s, invalid port=%d, m4u=%d\n",
			__func__, port, m4u_index);
		return -1;
	}

#ifndef CONFIG_FPGA_EARLY_PORTING
	if (virt == 0 || sec == 1)
#endif
		M4ULOG_HIGH("config_port:%s,v%d,s%d\n",
			m4u_get_port_name(port), virt, sec);

	mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_CONFIG_PORT],
		MMPROFILE_FLAG_START, port, virt);

	/* Prefetch Distance & Direction, one bit for each port, 1:-, 0:+ */

	if (dir != 0 || dis != 1)
		m4u_config_prog_dist(port, dir, dis, 1, 0, 0);
	else
		m4u_invalid_prog_dist_by_id(port);

	if (virt == 0)
		m4u_invalid_prog_dist_by_id(port);

	spin_lock(&gM4u_reg_lock[m4u_index]);

	if (m4u_index == 0) {
		int mmu_en = 0;

		larb = m4u_port_2_larb_id(port);
		larb_port = m4u_port_2_larb_port(port);
		if (unlikely(larb >= SMI_LARB_NR ||
			larb_port >= M4U_MAX_LARB_PORT)) {
			M4UMSG("%s, invalid port=%d, larb=%d\n",
				__func__, port, larb);
			spin_unlock(&gM4u_reg_lock[m4u_index]);
			return -1;
		}
		larb_base = gLarbBaseAddr[larb];
		m4uHw_set_field_by_mask(larb_base,
			SMI_LARB_NON_SEC_CONx(larb_port),
			F_SMI_MMU_EN, !!(virt));

		/* debug use */
		mmu_en = m4uHw_get_field_by_mask(larb_base,
			SMI_LARB_NON_SEC_CONx(larb_port), 0x1);
#ifndef CONFIG_FPGA_EARLY_PORTING
		if (!!(mmu_en) != virt) {
#endif
			M4ULOG_HIGH(
				"m4u_config_port error, port=%s, Virtuality=%d, mmu_en=%x (%x, %x)\n",
				m4u_get_port_name(port), virt, mmu_en,
				M4U_ReadReg32(larb_base,
					SMI_LARB_NON_SEC_CONx(larb_port)),
					F_SMI_MMU_EN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		}
#endif
	} else if (m4u_index == 1) {
		M4ULOG_HIGH("This is VPU_IOMMU domian, no need config port\n");
	} else {
		larb_port = m4u_port_2_larb_port(port);

		m4uHw_set_field_by_mask(gPericfgBaseAddr, REG_PERIAXI_BUS_CTL3,
			F_PERI_MMU_EN(larb_port, 1),
			F_PERI_MMU_EN(larb_port,
				!!(virt)));
	}

	spin_unlock(&gM4u_reg_lock[m4u_index]);

	mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_CONFIG_PORT],
		MMPROFILE_FLAG_END, dis, dir);

	return ret;
}

static inline void _m4u_port_clock_toggle(int m4u_index, int larb, int on)
{
	unsigned long long start, end;
	if (unlikely(m4u_index >= TOTAL_M4U_NUM || larb >= SMI_LARB_NR)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, larb=%d\n",
		    __func__, __LINE__, m4u_index, larb);
		return;
	}

	mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_TOGGLE_CG],
			MMPROFILE_FLAG_START, larb, on);
	if (m4u_index == 0) {
		start = sched_clock();
		if (on)
			larb_clock_on(larb, 1);
		else
			larb_clock_off(larb, 1);

		end = sched_clock();

		if (end - start > 50000000ULL) {
			/* unit is ns */
			M4ULOG_HIGH(
				"warn: larb%d clock %d time: %lld ns\n",
				larb, on, end - start);
		}
	}
	mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_TOGGLE_CG],
			MMPROFILE_FLAG_END, 0, 0);
}

int m4u_config_port(
	struct M4U_PORT_STRUCT *pM4uPort)	/* native */
{
	unsigned int PortID = (pM4uPort->ePortID);
	unsigned int m4u_index = m4u_port_2_m4u_id(PortID);
	unsigned int larb = m4u_port_2_larb_id(PortID);

	int ret;
#ifdef M4U_TEE_SERVICE_ENABLE
	unsigned int larb_port, mmu_en = 0, sec_en = 0;
#endif

	if (unlikely(m4u_index >= TOTAL_M4U_NUM ||
	    larb >= SMI_LARB_NR)) {
		M4ULOG_HIGH("%s, %d, invalid port=%d,id=%d\n",
		    __func__, __LINE__, PortID, m4u_index);
		return -1;
	}

	_m4u_port_clock_toggle(m4u_index, larb, 1);


#ifdef M4U_TEE_SERVICE_ENABLE
	larb_port = m4u_port_2_larb_port(PortID);
#if 0
	 mmu_en = !!(m4uHw_get_field_by_mask(gLarbBaseAddr[larb],
		SMI_LARB_MMU_EN, F_SMI_MMU_EN(larb_port, 1)));
	sec_en = !!(m4uHw_get_field_by_mask(gLarbBaseAddr[larb],
		SMI_LARB_SEC_EN, F_SMI_SEC_EN(larb_port, 1)));
#endif
	M4ULOG_HIGH(
		"%s: %s, m4u_tee_en:%d, mmu_en: %d -> %d, sec_en:%d -> %d\n",
			__func__,
		    m4u_get_port_name(PortID), m4u_tee_en,
		    mmu_en, pM4uPort->Virtuality, sec_en,
		    pM4uPort->Security);
	if (m4u_tee_en)
		m4u_config_port_tee(pM4uPort);
	else
#endif
	{
		ret = _m4u_config_port(PortID, pM4uPort->Virtuality,
				       pM4uPort->Security,
				       pM4uPort->Distance,
				       pM4uPort->Direction);
	}
	_m4u_port_clock_toggle(m4u_index, larb, 0);


	return 0;
}

int m4u_config_port_ext(struct M4U_PORT_STRUCT *pM4uPort)
{
	int ret = m4u_config_port(pM4uPort);

	return ret;
}

void m4u_port_array_init(struct m4u_port_array *port_array)
{
	memset(port_array, 0, sizeof(struct m4u_port_array));
}

int m4u_port_array_add(
	struct m4u_port_array *port_array,
	unsigned int port, int m4u_en, int secure)
{
	if (port >= M4U_PORT_NR) {
		M4UMSG(
			"error: port_array_add, port=%d, v(%d), s(%d)\n",
			port, m4u_en, secure);
		return -1;
	}
	port_array->ports[port] = M4U_PORT_ATTR_EN;
	if (m4u_en)
		port_array->ports[port] |= M4U_PORT_ATTR_VIRTUAL;
	if (secure)
		port_array->ports[port] |= M4U_PORT_ATTR_SEC;
	return 0;
}

#if 0
int m4u_config_port_array(struct m4u_port_array *port_array)
{
	unsigned int port, larb, larb_port;
	int ret = 0;
	unsigned int m4u_index;
	unsigned long larb_base;

	unsigned int config_larb[SMI_LARB_NR] = { 0 };
	unsigned int regNew[SMI_LARB_NR][32] = { {0} };
#ifdef M4U_TEE_SERVICE_ENABLE
	unsigned char m4u_port_array[(M4U_PORT_NR + 1) / 2] = { 0 };
#endif

	for (port = 0; port < M4U_PORT_NR; port++) {
		if (port_array->ports[port] && M4U_PORT_ATTR_EN != 0) {
			unsigned int value;

			larb = m4u_port_2_larb_id(port);
			larb_port = m4u_port_2_larb_port(port);
			if (larb >= SMI_LARB_NR ||
			    larb_port >= M4U_MAX_LARB_PORT)
				return -1;

			config_larb[larb] |= (1 << larb_port);
			regNew[larb][larb_port] = value =
				(!!(port_array->ports[port] &
					M4U_PORT_ATTR_VIRTUAL));

#ifdef M4U_TEE_SERVICE_ENABLE
			{
				unsigned char attr = ((!!value) << 1) | 0x1;

				if (port % 2)
					m4u_port_array[port / 2] |= (attr << 4);
				else
					m4u_port_array[port / 2] |= attr;
			}
#endif
		}
	}

	/* enable larb clock */
	for (larb = 0; larb < SMI_LARB_NR; larb++)
		if (config_larb[larb] != 0)
			_m4u_port_clock_toggle(0, larb, 1);

#ifdef M4U_TEE_SERVICE_ENABLE
	if (m4u_tee_en) {
		int i;
		int dom;

		for (i = 0; i < ((M4U_PORT_NR + 1) / 2); i++) {
			unsigned char attr = m4u_port_array[i];

			dom = (attr & 0xf0) >> 4;

			if (attr & 0x1)
				M4UMSG("%s idx:%d, dom:0x%x\n",
					__func__, i, dom);
		}
		M4UMSG("tee config port not support in normal world!\n");
		/*m4u_config_port_array_tee(m4u_port_array);*/
		for (larb = 0; larb < SMI_LARB_NR; larb++) {
			if (config_larb[larb] != 0)
				_m4u_port_clock_toggle(0, larb, 0);
		}
		return ret;
	}
#endif

	/* config port */
	for (port = 0; port < M4U_PORT_NR; port++) {
		if ((port_array->ports[port] && M4U_PORT_ATTR_EN) == 0)
			continue;

		m4u_index = m4u_port_2_m4u_id(port);
		if (m4u_index == 0) {
			unsigned int orig_value;

			larb = m4u_port_2_larb_id(port);
			larb_port = m4u_port_2_larb_port(port);
			if (larb >= SMI_LARB_NR ||
			   larb_port >= M4U_MAX_LARB_PORT) {
				M4UMSG("%s, larb %d is overflow\n",
					__func__, larb);
				ret = -1;
				goto out;
			}
			larb_base = gLarbBaseAddr[larb];
			orig_value =
				m4uHw_get_field_by_mask(
					larb_base,
					SMI_LARB_NON_SEC_CONx(larb_port),
					F_SMI_NON_SEC_MMU_EN(1));

				if (orig_value != regNew[larb][larb_port]) {
					spin_lock(&gM4u_reg_lock[m4u_index]);
				m4uHw_set_field_by_mask(
					larb_base,
					SMI_LARB_NON_SEC_CONx(larb_port),
					F_SMI_MMU_EN,
					F_SMI_NON_SEC_MMU_EN(
						!!(regNew[larb][larb_port])));
					spin_unlock(&gM4u_reg_lock[m4u_index]);
			}

		}
	}
out:
	/* disable larb clock */
	for (larb = 0; larb < SMI_LARB_NR; larb++)
		if (config_larb[larb] != 0)
			_m4u_port_clock_toggle(0, larb, 0);

	return ret;
}
#endif

void m4u_get_perf_counter(int m4u_index,
		int m4u_slave_id, struct M4U_PERF_COUNT *pM4U_perf_count)
{
	unsigned long m4u_base;
	int i = 0;

	if (unlikely(m4u_index >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_index);
		return;
	}

	m4u_base = gM4UBaseAddr[m4u_index];

	/* Main TLB performance count */
	for (i = 0; i < M4U_SLAVE_NUM(m4u_index); i++) {
		pM4U_perf_count->transaction_cnt[i] =
			M4U_ReadReg32(m4u_base,
			REG_MMU_ACC_CNT(i));
		pM4U_perf_count->main_tlb_miss_cnt[i] =
			M4U_ReadReg32(m4u_base,
			REG_MMU_MAIN_MSCNT(i));
		pM4U_perf_count->rs_perf_cnt[i] =
			M4U_ReadReg32(m4u_base,
			REG_MMU_RS_PERF_CNT(i));
	}
	/* Prefetch TLB performance count */
	pM4U_perf_count->pfh_tlb_miss_cnt =
	M4U_ReadReg32(m4u_base, REG_MMU_PF_MSCNT);
	pM4U_perf_count->pfh_cnt =
		M4U_ReadReg32(m4u_base, REG_MMU_PF_CNT);
}

int m4u_monitor_start(int m4u_id)
{
	unsigned long m4u_base;

	if (unlikely(m4u_id < 0 || m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_id];
	M4UINFO("====%s: %d======\n", __func__, m4u_id);
	/* clear GMC performance counter */
	m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG,
				F_MMU_CTRL_MONITOR_CLR(1),
				F_MMU_CTRL_MONITOR_CLR(1));
	m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG,
				F_MMU_CTRL_MONITOR_CLR(1),
				F_MMU_CTRL_MONITOR_CLR(0));

	/* enable GMC performance monitor */
	m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG,
				F_MMU_CTRL_MONITOR_EN(1),
				F_MMU_CTRL_MONITOR_EN(1));
	return 0;
}

int m4u_monitor_stop(int m4u_index)
{
	unsigned long m4u_base;

	if (unlikely(m4u_index < 0 || m4u_index >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_index);
		return -1;
	}

	m4u_base = gM4UBaseAddr[m4u_index];
	M4UINFO("====%s: %d======\n", __func__, m4u_index);
	/* disable GMC performance monitor */
	m4uHw_set_field_by_mask(m4u_base, REG_MMU_CTRL_REG,
				F_MMU_CTRL_MONITOR_EN(1),
				F_MMU_CTRL_MONITOR_EN(0));

	return 0;
}

void m4u_print_perf_counter(int m4u_index, int m4u_slave_id, const char *msg)
{
	struct M4U_PERF_COUNT cnt = {{0, 0}, {0, 0}, 0, 0, {0, 0} };
	unsigned int total_transaction_cnt = 0, total_cycle = 0;
	unsigned int total_hit_rate, mmu_hit_rate, bandwidth, rs_perf;
	int i = 0;

	if (unlikely(m4u_index >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_index);
		return;
	}

	M4UINFO(
		"====m4u performance count for %s m4u%d_%d======\n",
		msg, m4u_index, m4u_slave_id);
	m4u_get_perf_counter(m4u_index, m4u_slave_id, &cnt);
	/* read register get the count */
	for (i = 0; i < M4U_SLAVE_NUM(m4u_index); i++) {
		total_transaction_cnt += cnt.transaction_cnt[i];
		total_cycle += cnt.rs_perf_cnt[i];
		M4UINFO(
		"mmu%d perf: total:%u, miss:%u, rs:%u\n",
			   i,
			   cnt.transaction_cnt[i],
			   cnt.main_tlb_miss_cnt[i],
			   cnt.rs_perf_cnt[i]);
	}
	M4UINFO(
	"pfh perf: miss(walk):%u, auto pfh:%u\n",
		   cnt.pfh_tlb_miss_cnt, cnt.pfh_cnt);

	if (total_transaction_cnt > 0) {
		total_hit_rate = (total_transaction_cnt -
			 cnt.pfh_tlb_miss_cnt) *
			 100 / total_transaction_cnt;
		rs_perf = total_cycle / total_transaction_cnt;
	} else {
		total_hit_rate = 0;
		rs_perf = 0;
	}

	bandwidth = (cnt.pfh_tlb_miss_cnt + cnt.pfh_cnt) * 32 / 33;
	M4UINFO("total hit=%u, perf=%ucycle/tran, bw=%uB/ms\n",
	    total_hit_rate, rs_perf, bandwidth);
	for (i = 0; i < M4U_SLAVE_NUM(m4u_index); i++) {
		if (cnt.transaction_cnt[i] > 0)
			mmu_hit_rate = (cnt.transaction_cnt[i] -
				cnt.main_tlb_miss_cnt[i])
				* 100 / cnt.transaction_cnt[i];
		else
			mmu_hit_rate = 0;
		M4UINFO("mmu%d hit rate=%u\n", i, mmu_hit_rate);
	}

}


#define M4U_REG_BACKUP_SIZE     (100*sizeof(unsigned int))
static unsigned int *pM4URegBackUp;
static unsigned int gM4u_reg_backup_real_size;

#define __M4U_BACKUP(base, reg, back)    ((back) = M4U_ReadReg32(base, reg))
void __M4U_RESTORE(unsigned long base, unsigned int reg, unsigned int back)
{
	M4U_WriteReg32(base, reg, back);
}

int m4u_reg_backup(void)
{
	unsigned int *pReg = pM4URegBackUp;
	unsigned long m4u_base;
	int m4u_id, m4u_slave;
	int mau;
	unsigned int real_size;
	int dist;

	for (m4u_id = 0; m4u_id < TOTAL_M4U_NUM; m4u_id++) {
		m4u_base = gM4UBaseAddr[m4u_id];
		__M4U_BACKUP(m4u_base, REG_MMUg_PT_BASE, *(pReg++));
		__M4U_BACKUP(m4u_base, REG_MMU_PRIORITY, *(pReg++));
		__M4U_BACKUP(m4u_base, REG_MMU_DCM_DIS, *(pReg++));
		__M4U_BACKUP(m4u_base, REG_MMU_WR_LEN, *(pReg++));
		__M4U_BACKUP(m4u_base, REG_MMU_HW_DEBUG, *(pReg++));
		__M4U_BACKUP(m4u_base, REG_MMU_CTRL_REG, *(pReg++));
		__M4U_BACKUP(m4u_base, REG_MMU_IVRP_PADDR, *(pReg++));
		__M4U_BACKUP(m4u_base, REG_MMU_INT_L2_CONTROL, *(pReg++));
		__M4U_BACKUP(m4u_base, REG_MMU_INT_MAIN_CONTROL, *(pReg++));
		__M4U_BACKUP(m4u_base, REG_MMU_MMU_MISC_CTRL, *(pReg++));

		for (m4u_slave = 0; m4u_slave <
			M4U_SLAVE_NUM(m4u_id); m4u_slave++) {
			for (dist = 0; dist < MMU_TOTAL_PROG_DIST_NR; dist++)
				__M4U_BACKUP(m4u_base,
				REG_MMU_PROG_DIST(dist, m4u_slave), *(pReg++));
#if 0
			for (seq = 0; seq < M4U_SEQ_NUM(m4u_id); seq++) {
				__M4U_BACKUP(m4u_base,
					REG_MMU_SQ_START(m4u_slave, seq),
					*(pReg++));
				__M4U_BACKUP(m4u_base,
					REG_MMU_SQ_END(m4u_slave, seq),
					*(pReg++));
			}
#endif
			for (mau = 0; mau < MAU_NR_PER_M4U_SLAVE; mau++) {
				__M4U_BACKUP(m4u_base,
					REG_MMU_MAU_START(m4u_slave, mau),
					     *(pReg++));
				__M4U_BACKUP(m4u_base,
					REG_MMU_MAU_START_BIT32(m4u_slave, mau),
					     *(pReg++));
				__M4U_BACKUP(m4u_base,
					REG_MMU_MAU_END(m4u_slave, mau),
					*(pReg++));
				__M4U_BACKUP(m4u_base,
					REG_MMU_MAU_END_BIT32(m4u_slave, mau),
					     *(pReg++));
				__M4U_BACKUP(m4u_base,
					REG_MMU_MAU_PORT_EN(m4u_slave, mau),
					     *(pReg++));
			}
			__M4U_BACKUP(m4u_base,
				REG_MMU_MAU_LARB_EN(m4u_slave), *(pReg++));
			__M4U_BACKUP(m4u_base,
				REG_MMU_MAU_IO(m4u_slave), *(pReg++));
			__M4U_BACKUP(m4u_base,
				REG_MMU_MAU_RW(m4u_slave), *(pReg++));
			__M4U_BACKUP(m4u_base,
				REG_MMU_MAU_VA(m4u_slave), *(pReg++));
		}
	}

	/* check register size (to prevent overflow) */
	real_size = (pReg - pM4URegBackUp) * sizeof(unsigned int);
	if (real_size > M4U_REG_BACKUP_SIZE)
		m4u_aee_print("m4u_reg overflow! %d>%d\n",
			real_size, (int)M4U_REG_BACKUP_SIZE);

	gM4u_reg_backup_real_size = real_size;

	return 0;
}

int m4u_reg_restore(void)
{
	unsigned int *pReg = pM4URegBackUp;
	unsigned long m4u_base;
	int m4u_id, m4u_slave;
	int mau;
	unsigned int real_size;
	int dist;

	m4u_call_atf_debug(M4U_ATF_SECURITY_DEBUG_EN);

	for (m4u_id = 0; m4u_id < TOTAL_M4U_NUM; m4u_id++) {
		m4u_base = gM4UBaseAddr[m4u_id];
		__M4U_RESTORE(m4u_base, REG_MMUg_PT_BASE, *(pReg++));
		__M4U_RESTORE(m4u_base, REG_MMU_PRIORITY, *(pReg++));
		__M4U_RESTORE(m4u_base, REG_MMU_DCM_DIS, *(pReg++));
		__M4U_RESTORE(m4u_base, REG_MMU_WR_LEN, *(pReg++));
		__M4U_RESTORE(m4u_base, REG_MMU_HW_DEBUG, *(pReg++));
		__M4U_RESTORE(m4u_base, REG_MMU_CTRL_REG, *(pReg++));
		__M4U_RESTORE(m4u_base, REG_MMU_IVRP_PADDR, *(pReg++));
		__M4U_RESTORE(m4u_base, REG_MMU_INT_L2_CONTROL, *(pReg++));
		__M4U_RESTORE(m4u_base, REG_MMU_INT_MAIN_CONTROL, *(pReg++));
		__M4U_RESTORE(m4u_base, REG_MMU_MMU_MISC_CTRL, *(pReg++));

		for (m4u_slave = 0; m4u_slave <
			M4U_SLAVE_NUM(m4u_id); m4u_slave++) {

			for (dist = 0; dist < MMU_TOTAL_PROG_DIST_NR; dist++)
				__M4U_RESTORE(m4u_base,
				REG_MMU_PROG_DIST(dist, m4u_slave), *(pReg++));
#if 0
			for (seq = 0; seq < M4U_SEQ_NUM(m4u_id); seq++) {
				__M4U_RESTORE(m4u_base,
					REG_MMU_SQ_START(m4u_slave, seq),
					      *(pReg++));
				__M4U_RESTORE(m4u_base,
					REG_MMU_SQ_END(m4u_slave, seq),
					*(pReg++));
			}
#endif
			for (mau = 0; mau < MAU_NR_PER_M4U_SLAVE; mau++) {
				__M4U_RESTORE(m4u_base,
					REG_MMU_MAU_START(m4u_slave, mau),
					      *(pReg++));
				__M4U_RESTORE(m4u_base,
					REG_MMU_MAU_START_BIT32(m4u_slave, mau),
					      *(pReg++));
				__M4U_RESTORE(m4u_base,
					REG_MMU_MAU_END(m4u_slave, mau),
					*(pReg++));
				__M4U_RESTORE(m4u_base,
					REG_MMU_MAU_END_BIT32(m4u_slave, mau),
					      *(pReg++));
				__M4U_RESTORE(m4u_base,
					REG_MMU_MAU_PORT_EN(m4u_slave, mau),
					      *(pReg++));
			}
			__M4U_RESTORE(m4u_base,
				REG_MMU_MAU_LARB_EN(m4u_slave), *(pReg++));
			__M4U_RESTORE(m4u_base,
				REG_MMU_MAU_IO(m4u_slave), *(pReg++));
			__M4U_RESTORE(m4u_base,
				REG_MMU_MAU_RW(m4u_slave), *(pReg++));
			__M4U_RESTORE(m4u_base,
				REG_MMU_MAU_VA(m4u_slave), *(pReg++));
		}

	}

	/* check register size (to prevent overflow) */
	real_size = (pReg - pM4URegBackUp) * sizeof(unsigned int);
	if (real_size != gM4u_reg_backup_real_size)
		m4u_aee_print("m4u_reg_retore %d!=%d\n",
			real_size, gM4u_reg_backup_real_size);

	return 0;
}

static unsigned int larb_reg_backup_buf[SMI_LARB_NR][64];

static void larb_backup(unsigned int larb_idx)
{
	unsigned long larb_base;
	unsigned int i = 0;

	if (unlikely(larb_idx >= SMI_LARB_NR)) {
		M4UMSG("error: %s larb_idx = %d\n", __func__, larb_idx);
		return;
	}

	larb_base = gLarbBaseAddr[larb_idx];
	if (unlikely(larb_base == 0)) {
		M4UMSG("%s, invalid larb=%d, addr=0x%lx\n",
			__func__, larb_idx, larb_base);
		return;
	}

#ifdef M4U_TEE_SERVICE_ENABLE
	if (m4u_tee_en)
		/* m4u_larb_backup_sec(larb_idx); */

#endif
	{
		for (i = 0; i < 32; i++)
			__M4U_BACKUP(larb_base, SMI_LARB_NON_SEC_CONx(i),
				larb_reg_backup_buf[larb_idx][i]);

		for (i = 0; i < 32; i++)
			__M4U_BACKUP(larb_base, SMI_LARB_SEC_CONx(i),
				larb_reg_backup_buf[larb_idx][i + 32]);
	}
}

static void larb_restore(unsigned int larb_idx)
{
	unsigned long larb_base;
	unsigned int i = 0;

	if (unlikely(larb_idx >= SMI_LARB_NR)) {
		M4UMSG("error: %s larb_idx = %d\n", __func__, larb_idx);
		return;
	}

	larb_base = gLarbBaseAddr[larb_idx];
	if (unlikely(larb_base == 0)) {
		M4UMSG("%s, invalid larb=%d, addr=0x%lx\n",
			__func__, larb_idx, larb_base);
		return;
	}
#ifdef M4U_TEE_SERVICE_ENABLE
	if (m4u_tee_en)
		m4u_larb_restore_sec(larb_idx);
	else
#endif
	{
		for (i = 0; i < 32; i++)
			__M4U_RESTORE(larb_base, SMI_LARB_NON_SEC_CONx(i),
				larb_reg_backup_buf[larb_idx][i]);

		for (i = 0; i < 32; i++)
			__M4U_RESTORE(larb_base, SMI_LARB_SEC_CONx(i),
				larb_reg_backup_buf[larb_idx][i + 32]);
	}
}

static unsigned int larb0_cnt;

void m4u_larb0_enable(char *name)
{
	M4UMSG("%s, refcnt: %d, %s\n", __func__, larb0_cnt, name);

	mutex_lock(&m4u_larb0_mutex);
	larb_clock_on(0, 1);
	if (larb0_cnt == 0)
		larb_restore(0);

	larb0_cnt++;
	mutex_unlock(&m4u_larb0_mutex);
}

void m4u_larb0_disable(char *name)
{
	M4UMSG("%s, refcnt: %d, %s\n", __func__, larb0_cnt, name);

	mutex_lock(&m4u_larb0_mutex);
	larb0_cnt--;
	if (larb0_cnt == 0)
		larb_backup(0);

	larb_clock_off(0, 1);
	mutex_unlock(&m4u_larb0_mutex);
}

void m4u_print_port_status(struct seq_file *seq, int only_print_active,
			    unsigned int target_larb)
{
	unsigned int port, mmu_en = 0;
	unsigned int m4u_index, larb, last_larb = M4U_PORT_NR, larb_port;
	unsigned long larb_base;

	M4U_PRINT_SEQ(seq, "%s of larb%d  ========>\n",
			__func__, target_larb);

	for (port = 0; port < M4U_PORT_NR; port++) {
		m4u_index = m4u_port_2_m4u_id(port);
		if (m4u_index == 0) {
			larb = m4u_port_2_larb_id(port);
			if (unlikely(larb >= SMI_LARB_NR))
				continue;
			/* don't dump the unnecessary larb, in irq handling
			 * there is lock issue when enable clock in irq
			 * and violation issue when dump without enable clock
			 */
			if (seq == NULL && larb != target_larb)
				continue;
			larb_base = gLarbBaseAddr[larb];
			if (unlikely(larb_base == 0)) {
				M4U_PRINT_SEQ(seq,
					"%s, invalid larb=%u, addr=0x%lx\n",
					__func__, larb, larb_base);
				continue;
			}
			if (seq && larb != last_larb) {
				larb_clock_on(larb, 1);
				if (last_larb < SMI_LARB_NR)
					larb_clock_off(last_larb, 1);
				last_larb = larb;
			}

			larb_port = m4u_port_2_larb_port(port);

			mmu_en =
				m4uHw_get_field_by_mask(
					larb_base,
					SMI_LARB_NON_SEC_CONx(larb_port),
					F_SMI_NON_SEC_MMU_EN(1));
#if (TOTAL_M4U_NUM > 1)
		} else if (m4u_index == 1) {
			larb_port = m4u_port_2_larb_port(port);

			mmu_en =
				m4uHw_get_field_by_mask(
				gPericfgBaseAddr, REG_PERIAXI_BUS_CTL3,
					F_PERI_MMU_EN(
					larb_port, 1));
#endif
		} else {
			M4U_PRINT_SEQ(seq,
					"\ninvalid port=%d, m4u_slave_id=%d\n",
					port, m4u_index);
			return;
		}

		if (only_print_active && !mmu_en)
			continue;

		M4U_PRINT_SEQ(seq,
			"%s(%d),", m4u_get_port_name(port),
			!!mmu_en);
	}

	if (seq && m4u_index == 0 &&
	    last_larb < SMI_LARB_NR)
		larb_clock_off(last_larb, 1);
	M4U_PRINT_SEQ(seq, "\n");
}


void m4u_print_port_status_ext(struct seq_file *seq, int tf_port)
{
	int mmu_en = 0, mmu_en_sec = 0, sec_en = 0;
	unsigned int m4u_index, larb, l_port;
	unsigned long larb_base;

	M4U_PRINT_SEQ(seq, "port_status_ext larb %d========>\n", tf_port);
	m4u_index = m4u_port_2_m4u_id(tf_port);
	larb = m4u_port_2_larb_id(tf_port);

	if (m4u_index >= TOTAL_M4U_NUM ||
	    larb >= SMI_LARB_NR) {
		M4U_PRINT_SEQ(seq, "port_status_ext errror larb %d\n", larb);
		return;
	}

	M4U_PRINT_SEQ(seq, "port_status_ext larb %d========>\n", larb);

	if (m4u_index == 0) {
		l_port = m4u_port_2_larb_port(tf_port);
		if (l_port >= 32) {
			M4U_PRINT_SEQ(seq,
				"%s, invalid tf_port=%d, l_port=%d\n",
				__func__, tf_port, l_port);
			return;
		}
		larb_base = gLarbBaseAddr[larb];
		if (unlikely(larb_base == 0)) {
			M4U_PRINT_SEQ(seq,
				"%s, invalid larb=%d, addr=0x%lx\n",
				__func__, larb, larb_base);
			return;
		}
		mmu_en = m4uHw_get_field_by_mask(larb_base,
						 SMI_LARB_NON_SEC_CONx(l_port),
						 F_SMI_NON_SEC_MMU_EN(1));
		mmu_en_sec = m4uHw_get_field_by_mask(larb_base,
						 SMI_LARB_SEC_CONx(l_port),
						 F_SMI_SEC_MMU_EN(1));
		sec_en = m4uHw_get_field_by_mask(larb_base,
						 SMI_LARB_SEC_CONx(l_port),
						 F_SMI_SEC_EN(1));
	}

	M4U_PRINT_SEQ(seq,
			"%s larb:%d, port:%s, mmu_en:%d mmu_en_sec:%d, sec:%d\n",
			__func__, larb, m4u_get_port_name(tf_port),
			!!mmu_en, !!mmu_en_sec, !!sec_en);
}


int m4u_register_reclaim_callback(int port,
		m4u_reclaim_mva_callback_t *fn, void *data)
{
	if (port >= M4U_PORT_UNKNOWN) {
		M4UMSG("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	gM4uPort[port].reclaim_fn = fn;
	gM4uPort[port].reclaim_data = data;
	return 0;
}

int m4u_unregister_reclaim_callback(int port)
{
	if (port >= M4U_PORT_UNKNOWN) {
		M4UMSG("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	gM4uPort[port].reclaim_fn = NULL;
	gM4uPort[port].reclaim_data = NULL;
	return 0;
}

int m4u_reclaim_notify(unsigned int port, unsigned int mva, unsigned int size)
{
	int i = 0;

	for (i = 0; i < M4U_PORT_UNKNOWN; i++) {
		if (gM4uPort[i].reclaim_fn)
			gM4uPort[i].reclaim_fn(port, mva,
				size, gM4uPort[i].reclaim_data);
	}
	return 0;
}

int m4u_register_fault_callback(int port, m4u_fault_callback_t *fn, void *data)
{
	if (port >= M4U_PORT_UNKNOWN) {
		M4UMSG("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	gM4uPort[port].fault_fn = fn;
	gM4uPort[port].fault_data = data;
	return 0;
}

int m4u_unregister_fault_callback(int port)
{
	if (port >= M4U_PORT_UNKNOWN) {
		M4UMSG("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	gM4uPort[port].fault_fn = NULL;
	gM4uPort[port].fault_data = NULL;
	return 0;
}

int m4u_enable_tf(unsigned int port, bool fgenable)
{
	if (port < 0 || port >= M4U_PORT_UNKNOWN) {
		M4UMSG("%s fail,m port=%d\n", __func__, port);
		return -1;
	}
	gM4uPort[port].enable_tf = fgenable;
	return 0;
}

static struct timer_list m4u_isr_pause_timer;

static void m4u_isr_restart(unsigned long unused)
{
	M4UMSG("restart m4u irq\n");
	m4u_intr_modify_all(1);
}

static int m4u_isr_pause_timer_init(void)
{
	init_timer(&m4u_isr_pause_timer);
	m4u_isr_pause_timer.function = m4u_isr_restart;
	return 0;
}

static int m4u_isr_pause(int delay)
{
	m4u_intr_modify_all(0);
	m4u_isr_pause_timer.expires = jiffies + delay * HZ;
	add_timer(&m4u_isr_pause_timer);
	M4UMSG("warning: stop m4u irq for %ds\n", delay);
	return 0;
}

static void m4u_isr_record(void)
{
	static int m4u_isr_cnt;
	static unsigned long first_jiffies;

	/* we allow one irq in 1s, or we will disable them after 5s. */
	if (!m4u_isr_cnt ||
			time_after(jiffies,
				first_jiffies + m4u_isr_cnt * HZ)) {
		m4u_isr_cnt = 1;
		first_jiffies = jiffies;
	} else {
		m4u_isr_cnt++;
		if (m4u_isr_cnt >= 5) {
			/* 5 irqs come in 5s, too many ! */
			/* disable irq for a while, to avoid HWT timeout */
			m4u_isr_pause(10);
			m4u_isr_cnt = 0;
		}
	}
}

#define MMU_INT_REPORT(mmu, mmu_2nd_id, id) \
	M4UMSG("iommu%d_%d " #id "(0x%x) int happens!!\n", \
		mmu, mmu_2nd_id, id)
#define m4u_translation_log_format \
	"\nCRDISPATCH_KEY:M4U_%s\ntranslation fault:port=%s,mva=0x%x,pa=0x%x\n"

struct m4u_domain_t *m4u_get_domain_by_port(unsigned int port);

void dump_pgd_info(unsigned int mva)
{
	unsigned long pa;

	m4u_dump_pgtable(m4u_get_domain_by_port(0), NULL);

	pa = m4u_mva_to_pa(NULL, 0, mva);
	M4UMSG("(1) mva:0x%x pa:0x%lx\n", mva, pa);

}

irqreturn_t MTK_M4U_isr(int irq, void *dev_id)
{
	unsigned long m4u_base;
	unsigned int m4u_index;
	unsigned long larb_base;
	static int tf_cnt;

	if (irq == gM4uDev->irq_num[0]) {
		m4u_base = gM4UBaseAddr[0];
		m4u_index = 0;
		M4UMSG("This is MM_IOMMU domian\n");
	} else if (irq == gM4uDev->irq_num[1]) {
		m4u_base = gM4UBaseAddr[1];
		m4u_index = 1;
		M4UMSG("This is VPU_IOMMU domian\n");
	} else {
		M4UMSG("%s(), Invalid irq number %d\n", __func__, irq);
		return -1;
	}

	{
		/* L2 interrupt */
		unsigned int regval = M4U_ReadReg32(m4u_base,
				REG_MMU_L2_FAULT_ST);

		M4UMSG("m4u L2 interrupt sta=0x%x\n", regval);

		if (regval & F_INT_L2_MULTI_HIT_FAULT)
			MMU_INT_REPORT(m4u_index, 0, F_INT_L2_MULTI_HIT_FAULT);

		if (regval & F_INT_L2_TABLE_WALK_FAULT) {
			unsigned int fault_va, layer;

			MMU_INT_REPORT(m4u_index, 0, F_INT_L2_TABLE_WALK_FAULT);
			fault_va = M4U_ReadReg32(m4u_base,
					REG_MMU_TBWALK_FAULT_VA);
			layer = fault_va & 1;
			fault_va &= (~1);
			m4u_aee_print(
					"L2 table walk fault: mva=0x%x, layer=%d\n",
					fault_va, layer);
		}

		if (regval & F_INT_L2_PFH_DMA_FIFO_OVERFLOW)
			MMU_INT_REPORT(m4u_index, 0,
				F_INT_L2_PFH_DMA_FIFO_OVERFLOW);

		if (regval & F_INT_L2_MISS_DMA_FIFO_OVERFLOW)
			MMU_INT_REPORT(m4u_index, 0,
				F_INT_L2_MISS_DMA_FIFO_OVERFLOW);

		if (regval & F_INT_L2_INVALID_DONE)
			MMU_INT_REPORT(m4u_index, 0, F_INT_L2_INVALID_DONE);

		if (regval & F_INT_L2_PFH_OUT_FIFO_ERROR)
			MMU_INT_REPORT(m4u_index, 0,
				F_INT_L2_PFH_OUT_FIFO_ERROR);

		if (regval & F_INT_L2_PFH_IN_FIFO_ERROR)
			MMU_INT_REPORT(m4u_index, 0,
				F_INT_L2_PFH_IN_FIFO_ERROR);

		if (regval & F_INT_L2_MISS_OUT_FIFO_ERROR)
			MMU_INT_REPORT(m4u_index, 0,
				F_INT_L2_MISS_OUT_FIFO_ERROR);

		if (regval & F_INT_L2_MISS_IN_FIFO_ERR)
			MMU_INT_REPORT(m4u_index, 0, F_INT_L2_MISS_IN_FIFO_ERR);

	}

	{
		unsigned int IntrSrc = M4U_ReadReg32(m4u_base,
				REG_MMU_MAIN_FAULT_ST);
		unsigned int m4u_slave_id, larb_index = 0, larb_port = 0;
		unsigned int regval;
		int layer, write, m4u_port, i;
		unsigned int fault_mva, fault_pa, fault_pa_b33_32, fault_catch;
		unsigned int replace_pa;

		M4UMSG("m4u main interrupt happened: sta=0x%x\n", IntrSrc);

		if (m4u_index == 0)
			M4UMSG(" port normal 0x%x, sec 0x%x\n",
				M4U_ReadReg32(gLarbBaseAddr[0],
					SMI_LARB_NON_SEC_CONx(0)),
				M4U_ReadReg32(gLarbBaseAddr[0],
				SMI_LARB_SEC_CONx(0))
				);

		if (IntrSrc & (F_INT_MMU0_MAIN_MSK | F_INT_MMU0_MAU_MSK))
			m4u_slave_id = 0;
		else if (IntrSrc & (F_INT_MMU1_MAIN_MSK | F_INT_MMU1_MAU_MSK))
			m4u_slave_id = 1;
		else {
			M4UMSG("m4u interrupt error: status = 0x%x\n", IntrSrc);
			m4u_clear_intr(m4u_index);
			return 0;
		}

		/* read error info from registers */
		fault_mva = M4U_ReadReg32(m4u_base,
				REG_MMU_FAULT_VA(m4u_slave_id));
		layer = !!(fault_mva & F_MMU_FAULT_VA_LAYER_BIT);
		write = !!(fault_mva & F_MMU_FAULT_VA_WRITE_BIT);
		fault_pa_b33_32 = !!(fault_mva & F_MMU_FAULT_PA_B33_32);
		fault_catch = !!(fault_mva &
		   (F_MMU_FAULT_CATCH_INVPA | F_MMU_FAULT_CATCH_TF));
		fault_mva &= F_MMU_FAULT_VA_MSK;
		fault_pa = M4U_ReadReg32(m4u_base,
				REG_MMU_INVLD_PA(m4u_slave_id));
		regval = M4U_ReadReg32(m4u_base, REG_MMU_INT_ID(m4u_slave_id));
		m4u_port = m4u_get_port_by_tf_id(m4u_index, regval);
		replace_pa = M4U_ReadReg32(m4u_base, REG_MMU_IVRP_PADDR);
		larb_index = m4u_port_2_larb_id(m4u_port);
		larb_port = m4u_port_2_larb_port(m4u_port);
		if (unlikely(larb_index >= SMI_LARB_NR)) {
			M4UMSG("%s, invalid larb=%d, port=%d\n",
				__func__, larb_index, m4u_port);
		} else {
			larb_base = gLarbBaseAddr[larb_index];
			M4UMSG("larb%d, port%s, Nsec:0x%x, sec:0x%x\n",
				     larb_index, m4u_get_port_name(m4u_port),
				     M4U_ReadReg32(larb_base,
				     SMI_LARB_NON_SEC_CONx(larb_port)),
				     M4U_ReadReg32(larb_base,
				     SMI_LARB_SEC_CONx(larb_port))
				);
		}

		/* dump something quickly */
		///m4u_dump_rs_info(m4u_index, slave_id);
#if 0
		if (m4u_slave_id == 0)
			m4u_dump_valid_main0_tlb(m4u_index, m4u_slave_id);
		else if (m4u_slave_id == 1)
			m4u_dump_valid_main1_tlb(m4u_index, m4u_slave_id);
#endif
		/* m4u_dump_reg(m4u_index, 0x860); */
		/* m4u_dump_main_tlb(m4u_index, 0); */
		/* m4u_dump_pfh_tlb(m4u_index); */

		if (IntrSrc & F_INT_TRANSLATION_FAULT(m4u_slave_id)) {
			int bypass_DISP_TF = 0;

			MMU_INT_REPORT(m4u_index, m4u_slave_id,
				       F_INT_TRANSLATION_FAULT(m4u_slave_id));
			M4UMSG(
				"fault: port=%s, mva=0x%x, pa=0x%x, ,4g=0x%x,catch=0x%x\n",
			       m4u_get_port_name(m4u_port),
			       fault_mva, fault_pa,
			       fault_pa_b33_32, fault_catch);
			M4UMSG("layer=%d, wr=%d, fault_id=0x%x\n",
			       layer, write, regval);
			M4UMSG("replace_pa:0x%x, replace_pa1:0x%lx\n",
			       replace_pa,
			       gM4UtfAddr[m4u_index]);

			if (m4u_port == M4U_PORT_DISP_OVL0) {
				unsigned int valid_mva = 0;
				unsigned int valid_size = 0;
				unsigned int valid_mva_end = 0;

				__m4u_query_mva_info(m4u_index, fault_mva - 1,
					0, &valid_mva, &valid_size);
				if (valid_mva != 0 && valid_size != 0)
					valid_mva_end = valid_mva + valid_size;

				if (valid_mva_end != 0 &&
					fault_mva < valid_mva_end + SZ_4K) {
					M4UMSG(
						"bypass disp TF, valid mva=0x%x, size=0x%x\n",
					 valid_mva, valid_size);
					M4UMSG("bypass disp TF, mva_end=0x%x\n",
					 valid_mva_end);
					bypass_DISP_TF = 1;
				}
			}

			if (gM4uPort[m4u_port].enable_tf == 1 &&
					bypass_DISP_TF == 0) {
				int valid = 0;

				valid = m4u_dump_pte_nolock(
					m4u_get_domain_by_port(m4u_port),
							fault_mva);
				if (valid) {
					unsigned long long ts = sched_clock();

					dump_fault_mva_pfh_tlb(m4u_index,
						fault_mva);
					if (m4u_slave_id == 0)
						m4u_dump_valid_main0_tlb(
							m4u_index,
							m4u_slave_id);
					else if (m4u_slave_id == 1)
						m4u_dump_valid_main1_tlb(
							m4u_index,
							m4u_slave_id);

					/* workaround */
					if ((tf_cnt < 3) &&
						(m4u_port ==
						M4U_PORT_DISP_OVL0_2L)) {
						M4UMSG("tf cnt:%d ts:%llu ns\n",
							tf_cnt, ts);
						tf_cnt++;
						m4u_dump_buf_info(NULL,
							m4u_index);
						return IRQ_HANDLED;
					}
				}

				m4u_print_port_status_ext(NULL, m4u_port);

				if (m4u_index == 0) {
					M4UMSG("dump vpu domain page table\n");
					m4u_dump_pte_nolock(
						m4u_get_domain_by_port(
						M4U_PORT_VPU),
								fault_mva);
				} else if (m4u_index == 1) {
					M4UMSG("dump mm domain page table\n");
					m4u_dump_pte_nolock(
					m4u_get_domain_by_port(
					M4U_PORT_DISP_OVL0_HDR),
							fault_mva);
				}
				/*call user's callback to dump user registers*/
				if (m4u_port < M4U_PORT_UNKNOWN &&
					gM4uPort[m4u_port].fault_fn) {
					gM4uPort[m4u_port].fault_fn(m4u_port,
						fault_mva,
						gM4uPort[m4u_port].fault_data);
				}

				m4u_dump_buf_info(NULL, m4u_index);
				if (m4u_index == 0)
					m4u_aee_print(
						"\nCRDISPATCH_KEY:M4U_%s\ntranslation fault: port=%s, mva=0x%x, pa=0x%x, cnt:%d\n",
					 m4u_get_port_name(m4u_port),
					m4u_get_port_name(m4u_port),
					fault_mva, fault_pa,
					tf_cnt);
				else if (m4u_index == 1)
					m4u_aee_print(
						"\nCRDISPATCH_KEY:M4U_PORT_VPU(%s)\ntranslation fault: mva=0x%x, pa=0x%x, fault_id:0x%x\n",
					m4u_get_vpu_port_name(regval),
					fault_mva, fault_pa,
					regval);
			}
			mmprofile_log_ex(M4U_MMP_Events[M4U_MMP_M4U_ERROR],
					MMPROFILE_FLAG_PULSE,
				       m4u_port, fault_mva);
		}
		if (IntrSrc &
				F_INT_MAIN_MULTI_HIT_FAULT(m4u_slave_id)) {
			MMU_INT_REPORT(m4u_index, m4u_slave_id,
				F_INT_MAIN_MULTI_HIT_FAULT(m4u_slave_id));
		}
		if (IntrSrc &
				F_INT_INVALID_PA_FAULT(m4u_slave_id)) {
			if (!(IntrSrc &
				F_INT_TRANSLATION_FAULT(m4u_slave_id))) {
				MMU_INT_REPORT(m4u_index, m4u_slave_id,
					F_INT_INVALID_PA_FAULT(m4u_slave_id));

			}
		}
		if (IntrSrc & F_INT_ENTRY_REPLACEMENT_FAULT(m4u_slave_id)) {
			MMU_INT_REPORT(m4u_index, m4u_slave_id,
				F_INT_ENTRY_REPLACEMENT_FAULT(m4u_slave_id));
		}
		if (IntrSrc & F_INT_TLB_MISS_FAULT(m4u_slave_id))
			MMU_INT_REPORT(m4u_index, m4u_slave_id,
					F_INT_TLB_MISS_FAULT(m4u_slave_id));

		if (IntrSrc & F_INT_MISS_FIFO_ERR(m4u_slave_id))
			MMU_INT_REPORT(m4u_index, m4u_slave_id,
					F_INT_MISS_FIFO_ERR(m4u_slave_id));

		if (IntrSrc & F_INT_PFH_FIFO_ERR(m4u_slave_id))
			MMU_INT_REPORT(m4u_index, m4u_slave_id,
					F_INT_PFH_FIFO_ERR(m4u_slave_id));

		for (i = 0; i < MAU_NR_PER_M4U_SLAVE; i++) {
			if (IntrSrc & F_INT_MAU(m4u_slave_id, i)) {
				MMU_INT_REPORT(m4u_index, m4u_slave_id,
					F_INT_MAU(m4u_slave_id, i));
				__mau_dump_status(m4u_index, m4u_slave_id, i);
			}
		}
		m4u_clear_intr(m4u_index);
		m4u_isr_record();
	}

	return IRQ_HANDLED;
}

void m4u_call_atf_debug(int m4u_debug_id)
{
	size_t tf_port = 0;
	size_t tf_en = 0;

	M4UMSG("M4U CALL ATF ID:%d\n", m4u_debug_id);
	tf_en = mt_secure_call_ret2(MTK_M4U_DEBUG_DUMP,
				m4u_debug_id, 0, 0, 0, &tf_port);
}

irqreturn_t MTK_M4U_isr_sec(int irq, void *dev_id)
{
	size_t tf_en = 0;
	size_t tf_port = 0;
	size_t m4u_id = 0;

	if (irq == M4USecIrq[0]) {
		m4u_id = 0;
		M4UMSG("This is secure MM_IOMMU domian\n");
	} else if (irq == M4USecIrq[1]) {
		m4u_id = 1;
		M4UMSG("This is secure VPU_IOMMU domian\n");
	} else {
		M4UMSG("%s(), Invalid secure irq number %d\n", __func__, irq);
		return -1;
	}

	M4UMSG(
			"secure bank irq in normal world!\n");
		tf_en = mt_secure_call_ret2(MTK_M4U_DEBUG_DUMP,
				m4u_id, 0, 0, 0, &tf_port);
		M4UMSG(
			"secure bank go back form secure world! en:%zu\n",
				tf_en);
		if (tf_en) {
			if (m4u_id == 0)
				m4u_aee_print(
					"CRDISPATCH_KEY:M4U_%s translation fault(mm secure): port=%s\n",
						 m4u_get_port_name(tf_port),
						m4u_get_port_name(tf_port));
			else if (m4u_id == 1)
				m4u_aee_print(
					"CRDISPATCH_KEY:M4U_VPU_PORT translation fault(vpu secure)\n");
		}

	return IRQ_HANDLED;
}

struct m4u_domain_t *m4u_get_domain_by_port(unsigned int port)
{
	unsigned int domain_idx = 0;

	if (port >= M4U_PORT_VPU)
		domain_idx = 1;

	return &gM4uDomain[domain_idx];
}

struct m4u_domain_t *m4u_get_domain_by_id(int id)
{
	return &gM4uDomain[id];
}

int m4u_get_domain_nr(void)
{
	return 1;
}

#ifdef M4U_MMU_SLAVE_SWITCH
void m4u_switch_larb_slave(unsigned int larb_idx, unsigned int slave_idx)
{
	if (unlikely(larb_idx >= SMI_LARB_NR ||
	  slave_idx >= M4U_SLAVE_NUM(0))) {
		M4ULOG_HIGH("%s, %d, invalid id=%d, slave=%d\n",
		      __func__, __LINE__, larb_idx, slave_idx);
		return;
	}
	m4uHw_set_field_by_mask(gComBaseAddr,
		SMI_COMMON_LARB_BUS_SEL,
		F_SMI_LARB_BUS(0x3, larb_idx),
		F_SMI_LARB_BUS(slave_idx, larb_idx));
}
#endif


int m4u_reg_init(struct m4u_domain_t *m4u_domain,
	unsigned long ProtectPA, int m4u_id)
{
	unsigned int regval = 0;
	int i = 0;
	int j = 0;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return -1;
	}

	M4UINFO("====%s: %d======\n", __func__, m4u_id);

	M4UMSG("%s, ProtectPA = 0x%lx\n", __func__, ProtectPA);

	/* m4u clock is in infra domain, we never close this clock. */
	m4u_clock_on();


	/* ============================================= */
	/* SMI registers */
	/* ============================================= */
	/*bus selection: */
	/*  control which m4u_slave each larb routes to. */
	/*  this register is in smi_common domain */
	/*   Threre is only one AXI channel in K2, so don't need to set */

	/* ========================================= */
	/* larb init */
	/* ========================================= */
	if (m4u_id == 0) {
		struct device_node *node = NULL;
#ifdef M4U_MMU_SLAVE_SWITCH
		node =
			of_find_compatible_node(NULL,
				NULL, "mediatek,smi_common");
		if (node == NULL)
			M4UINFO("init common error\n");

		gComBaseAddr = (unsigned long)of_iomap(node, 0);
#endif
		for (i = 0; i < SMI_LARB_NR; i++) {
			node = of_find_compatible_node(NULL,
						NULL, gM4U_SMILARB[i]);
			if (node == NULL)
				M4UINFO("init larb %d error\n", i);

			gLarbBaseAddr[i] = (unsigned long)of_iomap(node, 0);
			/* set mm engine domain to 0x4 (default value) */
			larb_clock_on(i, 1);
#ifndef CONFIG_FPGA_EARLY_PORTING
			M4UMSG("m4u write all port domain to 4\n");
			for (j = 0; j < 32; j++) {
				if (gLarbBaseAddr[i] == 0)
					continue;

				m4uHw_set_field_by_mask(gLarbBaseAddr[i],
					SMI_LARB_SEC_CONx(j),
					F_SMI_DOMN(0x7), F_SMI_DOMN(0x4));
			}
#else
			j = 0;
#endif
			larb_clock_off(i, 1);

			M4UINFO("init larb %d, 0x%lx\n", i, gLarbBaseAddr[i]);
		}
	}
	/* ========================================= */
	/* perisys init */
	/* ========================================= */
	if (m4u_id == 2) {
		struct device_node *node = NULL;

		node = of_find_compatible_node(NULL, NULL, "mediatek,pericfg");
		gPericfgBaseAddr = (unsigned long)of_iomap(node, 0);

		M4UINFO("gPericfgBaseAddr: 0x%lx\n", gPericfgBaseAddr);
	}
	/* ============================================= */
	/* m4u registers */
	/* ============================================= */
	M4UMSG("m4u hw init id = %d,base address:0x%lx,pgd_pa:0x%lx\n",
		m4u_id, gM4UBaseAddr[m4u_id],
		(unsigned long)m4u_domain->pgd_pa);

	{
		unsigned int pgd_pa_reg = (unsigned int)m4u_domain->pgd_pa;

		if (m4u_domain->pgd_pa > 0xffffffffL) {
			if (!!(m4u_domain->pgd_pa & 0x100000000LL))
				pgd_pa_reg = (unsigned int)
					(m4u_domain->pgd_pa | F_PGD_REG_BIT32);
			if (!!(m4u_domain->pgd_pa & 0x200000000LL))
				pgd_pa_reg = (unsigned int)
					(m4u_domain->pgd_pa | F_PGD_REG_BIT33);
		}
		M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMUg_PT_BASE,
						   pgd_pa_reg);
		M4UMSG("m4u hw init id:%d, pg_base:0x%x\n",
			m4u_id, M4U_ReadReg32(gM4UBaseAddr[m4u_id],
			REG_MMUg_PT_BASE));

		regval = M4U_ReadReg32(gM4UBaseAddr[m4u_id], REG_MMU_CTRL_REG);

		regval = regval | F_MMU_CTRL_PFH_DIS(0)
			 | F_MMU_CTRL_MONITOR_EN(0)
			 | F_MMU_CTRL_MONITOR_CLR(0)
			 | F_MMU_CTRL_INT_HANG_EN(0);

		M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_CTRL_REG, regval);

		/* enable all interrupts */
		m4u_enable_intr(m4u_id);

		/* set translation fault proctection buffer address */
		M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_IVRP_PADDR,
			       (unsigned int)F_MMU_IVRP_PA_SET(ProtectPA));
		M4UMSG("m4u hw init id:%d, tf_reg:0x%x\n",
			m4u_id, M4U_ReadReg32(gM4UBaseAddr[m4u_id],
			REG_MMU_IVRP_PADDR));

		/* enable DCM */
		M4U_WriteReg32(gM4UBaseAddr[m4u_id], REG_MMU_DCM_DIS, 0);

		m4u_invalid_tlb_all(m4u_id);

		/* 4 write command throttling mode */
		m4uHw_set_field_by_mask(gM4UBaseAddr[m4u_id], REG_MMU_WR_LEN,
				F_MMU_MMU0_WR_THROT_DIS, 0);
		m4uHw_set_field_by_mask(gM4UBaseAddr[m4u_id], REG_MMU_WR_LEN,
				F_MMU_MMU1_WR_THROT_DIS, 0);
	}

	/* special settings for mmu0 (multimedia iommu) */
	if (m4u_id == 0) {
		unsigned long m4u_base = gM4UBaseAddr[0];
		/* 2 disable in-order-write */
#ifdef CONFIG_MTK_SMI_EXT
		m4uHw_set_field_by_mask(m4u_base, REG_MMU_MMU_MISC_CTRL,
			REG_MMU0_IN_ORDER_WR_EN, 0);
		m4uHw_set_field_by_mask(m4u_base, REG_MMU_MMU_MISC_CTRL,
			REG_MMU1_IN_ORDER_WR_EN, 0);
#endif

		/* 3 non-standard AXI mode */
		m4uHw_set_field_by_mask(m4u_base,
			REG_MMU_MMU_MISC_CTRL,
			F_REG_MMU0_STANDARD_AXI_MODE, 0);
		m4uHw_set_field_by_mask(m4u_base,
			REG_MMU_MMU_MISC_CTRL,
			F_REG_MMU1_STANDARD_AXI_MODE, 0);
	}
	M4UMSG("0x48 reg setting: 0x%x, dom:%d\n",
			M4U_ReadReg32(gM4UBaseAddr[m4u_id],
			REG_MMU_MMU_MISC_CTRL),
			m4u_id);
	return 0;
}

int m4u_domain_init(struct m4u_device *m4u_dev,
		void *priv_reserve, unsigned int m4u_id)
{
	M4UINFO("%s, domain=%d\n", __func__, m4u_id);

	memset(&gM4uDomain[m4u_id], 0, sizeof(gM4uDomain[m4u_id]));
		gM4uDomain[m4u_id].pgsize_bitmap = M4U_PGSIZES;
	mutex_init(&gM4uDomain[m4u_id].pgtable_mutex);
	gM4uDomain[m4u_id].domain = m4u_id;

	m4u_pgtable_init(m4u_dev, &gM4uDomain[m4u_id], m4u_id);

	m4u_mvaGraph_init(priv_reserve, m4u_id);

	return 0;
}

int m4u_reset(unsigned int m4u_id)
{
	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return -1;
	}

	m4u_invalid_tlb_all(m4u_id);
	m4u_clear_intr(m4u_id);

	return 0;
}

int m4u_hw_init(struct m4u_device *m4u_dev, unsigned int m4u_id)
{
	unsigned long pProtectVA;
	phys_addr_t ProtectPA;
	struct device_node *node = NULL;

	if (unlikely(m4u_id >= TOTAL_M4U_NUM)) {
		M4ULOG_HIGH("%s, %d, invalid id=%d\n",
		    __func__, __LINE__, m4u_id);
		return -1;
	}

#ifdef M4U_4GBDRAM
	gM4U_4G_DRAM_Mode = enable_4G();
#endif
	M4UMSG("4G DRAM Mode is: %d\n", gM4U_4G_DRAM_Mode);

	gM4UBaseAddr[m4u_id] = m4u_dev->m4u_base[m4u_id];

	pProtectVA = (unsigned long)kmalloc(TF_PROTECT_BUFFER_SIZE * 2,
				GFP_KERNEL | __GFP_ZERO);
	if ((void *)pProtectVA == NULL) {
		M4UMSG("Physical memory not available.\n");
		return -1;
	}
	pProtectVA = (pProtectVA + (TF_PROTECT_BUFFER_SIZE - 1)) &
		(~(TF_PROTECT_BUFFER_SIZE - 1));

	ProtectPA = virt_to_phys((void *)pProtectVA);
	if (ProtectPA & (TF_PROTECT_BUFFER_SIZE - 1)) {
		M4UMSG("protect buffer (0x%lx) not align.\n",
			(unsigned long)ProtectPA);
		return -1;
	}
	gM4UtfAddr[m4u_id] = ProtectPA;
	M4UMSG("protect memory va=0x%lx, pa=0x%lx, bc_tf=0x%lx\n",
		pProtectVA, (unsigned long)ProtectPA,
		gM4UtfAddr[m4u_id]);

	pM4URegBackUp = kmalloc(M4U_REG_BACKUP_SIZE, GFP_KERNEL | __GFP_ZERO);
	if (pM4URegBackUp == NULL) {
		M4UMSG("Physical memory not available size=%d.\n",
			(int)M4U_REG_BACKUP_SIZE);
		return -1;
	}

	spin_lock_init(&gM4u_reg_lock[m4u_id]);

	if (m4u_id == 0) {
		M4UMSG("m4u atf config\n");
		m4u_call_atf_debug(M4U_ATF_SECURITY_DEBUG_EN);
	}

	m4u_reg_init(&gM4uDomain[m4u_id], ProtectPA, m4u_id);

	/* register normal bank irq */
	if (request_irq(m4u_dev->irq_num[m4u_id], MTK_M4U_isr,
			IRQF_TRIGGER_NONE, "m4u", NULL)) {
		M4UERR("request M4U%d IRQ line failed\n", m4u_id);
		return -ENODEV;
	}

	M4UMSG("request_irq, irq_num=%d\n", m4u_dev->irq_num[m4u_id]);

	m4u_isr_pause_timer_init();

	m4u_monitor_start(m4u_id);

	/* register secure bank irq */
	if (m4u_id == 0) {
		node = of_find_compatible_node(NULL, NULL,
							   "mediatek,sec_mm_m4u");
	} else if (m4u_id == 1) {
		node = of_find_compatible_node(NULL, NULL,
							   "mediatek,sec_vpu_m4u");
	} else {
		M4UMSG("m4u_id is error, id:%d\n", m4u_id);
	}

	if (node != NULL) {
		gM4USecAddr[m4u_id] = (unsigned long)of_iomap(node, 0);
		M4USecIrq[m4u_id] = irq_of_parse_and_map(node, 0);

		M4UMSG("secure bank, of_iomap: 0x%lx, irq_num: %d, m4u_id:%d\n",
				gM4USecAddr[m4u_id], M4USecIrq[m4u_id], m4u_id);

		if (request_irq(M4USecIrq[m4u_id], MTK_M4U_isr_sec,
				IRQF_TRIGGER_NONE, "secure_m4u", NULL)) {
			M4UERR("request secure m4u%d IRQ line failed\n",
				m4u_id);
			return -ENODEV;
		}
	} else {
		M4UMSG(
			"ERR, unable to find node, m4u_id:%d\n", m4u_id);
	}


#if 0
	mau_start_monitor(0, 0, 0, 1, 1, 0, 0, 0x0,
			0xfffff, 0xffffffff, 0xffffffff);
	mau_start_monitor(0, 0, 1, 0, 1, 0, 0, 0x0,
			0xfffff, 0xffffffff, 0xffffffff);
#endif
	g_translation_fault_debug = 0;

	return 0;
}

int m4u_hw_deinit(struct m4u_device *m4u_dev, unsigned int m4u_id)
{
	free_irq(m4u_dev->irq_num[m4u_id], NULL);
	return 0;
}

int m4u_dump_reg_for_smi_hang_issue(void)
{
	/*NOTES: m4u_monitor_start() must be called before using m4u */
	/*please check m4u_hw_init() to ensure that */

	int cnt;

	M4UMSG("====== dump m4u reg start =======>\n");

	if (gM4UBaseAddr[0] == 0) {
		M4UMSG("gM4UBaseAddr[0] is NULL\n");
		return 0;
	}
	/* control register */
	M4UMSG("0x44 = 0x%x\n", M4U_ReadReg32(gM4UBaseAddr[0], 0x44));
	M4UMSG("0x48 = 0x%x\n", M4U_ReadReg32(gM4UBaseAddr[0], 0x48));
	M4UMSG("0x50 = 0x%x\n", M4U_ReadReg32(gM4UBaseAddr[0], 0x50));
	M4UMSG("0x54 = 0x%x\n", M4U_ReadReg32(gM4UBaseAddr[0], 0x54));
	M4UMSG("0xA0 = 0x%x\n", M4U_ReadReg32(gM4UBaseAddr[0], 0xa0));
	M4UMSG("0x110 = 0x%x\n", M4U_ReadReg32(gM4UBaseAddr[0], 0x110));

	/* dump five times*/
	for (cnt = 0; cnt < 5; cnt++) {
		M4UMSG("0x08 = 0x%x\n", M4U_ReadReg32(gM4UBaseAddr[0], 0x08));
		m4u_dump_debug_reg_info(0);
		m4u_dump_rs_sta_info(0, 0);
		m4u_dump_rs_sta_info(0, 1);
	}

	M4UMSG("====== dump m4u reg end =======>\n");

	return 0;
}
