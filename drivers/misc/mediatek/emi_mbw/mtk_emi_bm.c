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

#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mt-plat/mtk_io.h>
#include "mach/mtk_emi_bm.h"

#define EMI_ENABLE_DEBUG

#ifdef EMI_ENABLE_DEBUG
#define EMI_DBG(fmt, args...) \
	pr_info("[%s@%d]"fmt, __func__, __LINE__, ##args)
#else
#define EMI_DBG(fmt, args...)
#endif

/* we just use pr_info for EMI_ERR, because generally,
 * pr_info log can output
 */
#define EMI_ERR(fmt, args...) \
	pr_info("[%s@%d]"fmt, __func__, __LINE__, ##args)

static unsigned char g_cBWL;
/* not initialise statics to 0 or NULL */
static void __iomem *EMI_BASE_ADDR;

void BM_Init(void)
{

	struct device_node *node;

	/* DTS version */
	node = of_find_compatible_node(NULL, NULL, "mediatek,EMI");
	if (node) {
		EMI_BASE_ADDR = of_iomap(node, 0);
		EMI_DBG("get EMI_BASE_ADDR @ %p\n", EMI_BASE_ADDR);
	} else {
		EMI_ERR("can't find compatible node\n");
		return;
	}

	g_cBWL = 0;

	/*
	 * make sure BW limiter counts consumed Soft-mode BW of each master
	 */
	if (readl(IOMEM(EMI_ARBA)) & 0x00008000) {
		g_cBWL |= 1 << 0;
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBA)) &
		~0x00008000, EMI_ARBA);
	}

	if (readl(IOMEM(EMI_ARBB)) & 0x00008000) {
		g_cBWL |= 1 << 1;
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBB)) &
		~0x00008000, EMI_ARBB);
	}

	if (readl(IOMEM(EMI_ARBC)) & 0x00008000) {
		g_cBWL |= 1 << 2;
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBC)) &
		~0x00008000, EMI_ARBC);
	}

	if (readl(IOMEM(EMI_ARBD)) & 0x00008000) {
		g_cBWL |= 1 << 3;
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBD)) &
		~0x00008000, EMI_ARBD);
	}

	if (readl(IOMEM(EMI_ARBE)) & 0x00008000) {
		g_cBWL |= 1 << 4;
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBE)) &
		~0x00008000, EMI_ARBE);
	}
	if (readl(IOMEM(EMI_ARBF)) & 0x00008000) {
		g_cBWL |= 1 << 5;
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBF)) &
		~0x00008000, EMI_ARBF);
	}

	if (readl(IOMEM(EMI_ARBG_2ND)) & 0x00008000) {
		g_cBWL |= 1 << 6;
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBG_2ND)) &
		~0x00008000, EMI_ARBG_2ND);
	}
#if defined(CONFIG_ARCH_MT6797)
	if (readl(IOMEM(EMI_ARBG)) & 0x00008000) {
		g_cBWL |= 1 << 6;
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBG)) &
		~0x00008000, EMI_ARBG);
	}
	if (readl(IOMEM(EMI_ARBH)) & 0x00008000) {
		g_cBWL |= 1 << 7;
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBH)) &
		~0x00008000, EMI_ARBH);
	}
#endif

}

void BM_DeInit(void)
{
	if (g_cBWL & (1 << 0)) {
		g_cBWL &= ~(1 << 0);
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBA)) |
		0x00008000, EMI_ARBA);
	}

	if (g_cBWL & (1 << 1)) {
		g_cBWL &= ~(1 << 1);
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBB)) |
		0x00008000, EMI_ARBB);
	}

	if (g_cBWL & (1 << 2)) {
		g_cBWL &= ~(1 << 2);
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBC)) |
		0x00008000, EMI_ARBC);
	}

	if (g_cBWL & (1 << 3)) {
		g_cBWL &= ~(1 << 3);
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBD)) |
		0x00008000, EMI_ARBD);
	}

	if (g_cBWL & (1 << 4)) {
		g_cBWL &= ~(1 << 4);
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBE)) |
		0x00008000, EMI_ARBE);
	}

	if (g_cBWL & (1 << 5)) {
		g_cBWL &= ~(1 << 5);
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBF)) |
		0x00008000, EMI_ARBF);
	}

	if (g_cBWL & (1 << 6)) {
		g_cBWL &= ~(1 << 6);
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBG_2ND)) |
		0x00008000, EMI_ARBG_2ND);
	}
#if defined(CONFIG_ARCH_MT6797)
	if (g_cBWL & (1 << 6)) {
		g_cBWL &= ~(1 << 6);
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBG)) |
		0x00008000, EMI_ARBG);
	}

	if (g_cBWL & (1 << 7)) {
		g_cBWL &= ~(1 << 7);
		mt_reg_sync_writel(readl(IOMEM(EMI_ARBH)) |
		0x00008000, EMI_ARBH);
	}
#endif

}

void BM_Enable(const unsigned int enable)
{
	const unsigned int value = readl(IOMEM(EMI_BMEN));

	mt_reg_sync_writel((value & ~(BUS_MON_PAUSE | BUS_MON_EN)) |
	(enable ? BUS_MON_EN : 0), EMI_BMEN);
}

/*
 *   void BM_Disable(void)
 *  {
 *   const unsigned int value = readl(EMI_BMEN);
 *
 *   mt_reg_sync_writel(value & (~BUS_MON_EN), EMI_BMEN);
 *   }
 */

void BM_Pause(void)
{
	const unsigned int value = readl(IOMEM(EMI_BMEN));

	mt_reg_sync_writel(value | BUS_MON_PAUSE, EMI_BMEN);
}

void BM_Continue(void)
{
	const unsigned int value = readl(IOMEM(EMI_BMEN));

	mt_reg_sync_writel(value & (~BUS_MON_PAUSE), EMI_BMEN);
}

unsigned int BM_IsOverrun(void)
{
	/*
	 * return 0 if EMI_BCNT(bus cycle counts)
	 * or EMI_WACT(total word counts) is overrun,
	 * otherwise return an !0 value
	 */
	const unsigned int value = readl(IOMEM(EMI_BMEN));

	return value & BC_OVERRUN;
}

void BM_SetReadWriteType(const unsigned int ReadWriteType)
{
	const unsigned int value = readl(IOMEM(EMI_BMEN));

	/*
	 * ReadWriteType: 00/11 --> both R/W
	 *                   01 --> only R
	 *                   10 --> only W
	 */
	mt_reg_sync_writel((value & 0xFFFFFFCF) |
	(ReadWriteType << 4), EMI_BMEN);
}

int BM_GetBusCycCount(void)
{
	return BM_IsOverrun() ? BM_ERR_OVERRUN : readl(IOMEM(EMI_BCNT));
}

unsigned int BM_GetTransAllCount(void)
{
	return readl(IOMEM(EMI_TACT));
}

int BM_GetTransCount(const unsigned int counter_num)
{
	unsigned int iCount;

	switch (counter_num) {
	case 1:
		iCount = readl(IOMEM(EMI_TSCT));
		break;

	case 2:
		iCount = readl(IOMEM(EMI_TSCT2));
		break;

	case 3:
		iCount = readl(IOMEM(EMI_TSCT3));
		break;

	default:
		return BM_ERR_WRONG_REQ;
	}

	return iCount;
}

long long BM_GetWordAllCount(void)
{
	unsigned int word_all_count;

	word_all_count = readl(IOMEM(EMI_WACT));

	if (BM_IsOverrun() && (word_all_count == 0xFFFFFFFF))
		return BM_ERR_OVERRUN;
	else
		return word_all_count;
}

int BM_GetWordCount(const unsigned int counter_num)
{
	unsigned int iCount;

	switch (counter_num) {
	case 1:
		iCount = readl(IOMEM(EMI_WSCT));
		break;

	case 2:
		iCount = readl(IOMEM(EMI_WSCT2));
		break;

	case 3:
		iCount = readl(IOMEM(EMI_WSCT3));
		break;

	case 4:
		iCount = readl(IOMEM(EMI_WSCT4));
		break;

	default:
		return BM_ERR_WRONG_REQ;
	}

	return iCount;
}

unsigned int BM_GetBandwidthWordCount(void)
{
	return readl(IOMEM(EMI_BACT));
}

unsigned int BM_GetOverheadWordCount(void)
{
	return readl(IOMEM(EMI_BSCT));
}

int BM_GetTransTypeCount(const unsigned int counter_num)
{
	return (counter_num < 1	|| counter_num > BM_COUNTER_MAX) ?
	BM_ERR_WRONG_REQ : readl(IOMEM(EMI_TTYPE1 + (counter_num - 1) * 8));
}

int BM_SetMonitorCounter(const unsigned int counter_num,
const unsigned int master, const unsigned int trans_type)
{
	unsigned int value;
	unsigned long addr;
	const unsigned int iMask = 0xFFFF;

	if (counter_num < 1 || counter_num > BM_COUNTER_MAX)
		return BM_ERR_WRONG_REQ;

	if (counter_num == 1) {
		addr = (unsigned long) EMI_BMEN;
		value = (readl(IOMEM(addr)) & ~(iMask << 16)) |
		((trans_type & 0xFF) << 24) | ((master & 0xFF) << 16);
	} else {
		addr = (counter_num <= 3) ? (unsigned long) EMI_MSEL :
			((unsigned long) EMI_MSEL2 + (counter_num / 2 - 2) * 8);

		/* clear master and transaction type fields */
		value =
		readl(IOMEM(addr)) & ~(iMask << ((counter_num % 2) * 16));

		/* set master and transaction type fields */
		value |= (((trans_type & 0xFF) << 8) |
		(master & 0xFF)) << ((counter_num % 2) * 16);
	}

	mt_reg_sync_writel(value, addr);

	return BM_REQ_OK;
}

int BM_SetMaster(const unsigned int counter_num, const unsigned int master)
{
	unsigned int value;
	unsigned long addr;
	const unsigned int iMask = 0xFF;

	if (counter_num < 1 || counter_num > BM_COUNTER_MAX)
		return BM_ERR_WRONG_REQ;

	if (counter_num == 1) {
		addr = (unsigned long) EMI_BMEN;
		value = (readl(IOMEM(addr)) & ~(iMask << 16)) |
		((master & iMask) << 16);
	} else {
		addr = (counter_num <= 3) ? (unsigned long) EMI_MSEL :
			((unsigned long) EMI_MSEL2 + (counter_num / 2 - 2) * 8);

		/* clear master and transaction type fields */
		value =
		readl(IOMEM(addr)) & ~(iMask << ((counter_num % 2) * 16));

		/* set master and transaction type fields */
		value |= ((master & iMask) << ((counter_num % 2) * 16));
	}

	mt_reg_sync_writel(value, addr);

	return BM_REQ_OK;
}

int BM_SetIDSelect(const unsigned int counter_num,
		const unsigned int id, const unsigned int enable)
{
	unsigned int value, shift_num;
	unsigned long addr;

	if ((counter_num < 1 || counter_num > BM_COUNTER_MAX)
	    || (id > 0x1FFF) || (enable > 1))
		return BM_ERR_WRONG_REQ;

	addr = (unsigned long) EMI_BMID0 + ((counter_num - 1) / 2) * 4;

	/* field's offset in the target EMI_BMIDx register */
	shift_num = ((counter_num - 1) % 2) * 16;

	/* clear SELx_ID field */
	value = readl(IOMEM(addr)) & ~(0x1FFF << shift_num);

	/* set SELx_ID field */
	value |= id << shift_num;

	mt_reg_sync_writel(value, addr);

	value = (readl(IOMEM(EMI_BMEN2)) & ~(1 << (counter_num - 1))) |
					(enable << (counter_num - 1));

	mt_reg_sync_writel(value, EMI_BMEN2);

	return BM_REQ_OK;
}

int BM_SetUltraHighFilter(const unsigned int counter_num,
		const unsigned int enable)
{
	unsigned int value;

	if ((counter_num < 1 || counter_num > BM_COUNTER_MAX)
	    || (enable > 1)) {
		return BM_ERR_WRONG_REQ;
	}

	value = (readl(IOMEM(EMI_BMEN1)) & ~(1 << (counter_num - 1))) |
					(enable << (counter_num - 1));

	mt_reg_sync_writel(value, EMI_BMEN1);

	return BM_REQ_OK;
}

int BM_SetLatencyCounter(void)
{
	unsigned int value;

	value = readl(IOMEM(EMI_BMEN2)) & ~(0x3 << 24);
	/*emi_ttype1 -- emi_ttype7 change as total latencies for m0 -- m6,
	 * and emi_ttype9 -- emi_ttype15 change
	 * as total transaction counts for m0 -- m6
	 */
	value |= (0x2 << 24);
	mt_reg_sync_writel(value, EMI_BMEN2);
	return BM_REQ_OK;
}

int BM_GetLatencyCycle(const unsigned int counter_num)
{
	unsigned int cycle_count;

	switch (counter_num) {
	case 1:
		cycle_count = readl(IOMEM(EMI_TTYPE1));
		break;
	case 2:
		cycle_count = readl(IOMEM(EMI_TTYPE2));
		break;
	case 3:
		cycle_count = readl(IOMEM(EMI_TTYPE3));
		break;
	case 4:
		cycle_count = readl(IOMEM(EMI_TTYPE4));
		break;
	case 5:
		cycle_count = readl(IOMEM(EMI_TTYPE5));
		break;
	case 6:
		cycle_count = readl(IOMEM(EMI_TTYPE6));
		break;
	case 7:
		cycle_count = readl(IOMEM(EMI_TTYPE7));
		break;
	case 9:
		cycle_count = readl(IOMEM(EMI_TTYPE9));
		break;
	case 10:
		cycle_count = readl(IOMEM(EMI_TTYPE10));
		break;
	case 11:
		cycle_count = readl(IOMEM(EMI_TTYPE11));
		break;
	case 12:
		cycle_count = readl(IOMEM(EMI_TTYPE12));
		break;
	case 13:
		cycle_count = readl(IOMEM(EMI_TTYPE13));
		break;
	case 14:
		cycle_count = readl(IOMEM(EMI_TTYPE14));
		break;
	case 15:
		cycle_count = readl(IOMEM(EMI_TTYPE15));
		break;
	case 16:
		cycle_count = readl(IOMEM(EMI_TTYPE16));
		break;
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6797)
	case 17:
		cycle_count = readl(IOMEM(EMI_TTYPE17));
		break;
	case 18:
		cycle_count = readl(IOMEM(EMI_TTYPE18));
		break;
	case 19:
		cycle_count = readl(IOMEM(EMI_TTYPE19));
		break;
	case 20:
		cycle_count = readl(IOMEM(EMI_TTYPE20));
		break;
	case 21:
		cycle_count = readl(IOMEM(EMI_TTYPE21));
		break;
#endif
	default:
		return BM_ERR_WRONG_REQ;
	}
	return cycle_count;
}

int BM_GetEmiDcm(void)
{
	/* return ((readl(IOMEM(EMI_CONM)) >> 24) ? 1 : 0); */
	return readl(IOMEM(EMI_CONM)) >> 24;
}

int BM_SetEmiDcm(const unsigned int setting)
{
	unsigned int value;

	value = readl(IOMEM(EMI_CONM));
	mt_reg_sync_writel((value & 0x00FFFFFF) | (setting << 24), EMI_CONM);

	return BM_REQ_OK;
}

unsigned int DRAMC_GetPageHitCount(const unsigned int CountType)
{
	unsigned int iCount;

	switch (CountType) {
	case DRAMC_R2R:
		iCount = ucDram_Register_Read(DRAMC_R2R_PAGE_HIT);
		break;

	case DRAMC_R2W:
		iCount = ucDram_Register_Read(DRAMC_R2W_PAGE_HIT);
		break;

	case DRAMC_W2R:
		iCount = ucDram_Register_Read(DRAMC_W2R_PAGE_HIT);
		break;

	case DRAMC_W2W:
		iCount = ucDram_Register_Read(DRAMC_W2W_PAGE_HIT);
		break;
	case DRAMC_ALL:
		iCount = ucDram_Register_Read(DRAMC_R2R_PAGE_HIT) +
				ucDram_Register_Read(DRAMC_R2W_PAGE_HIT) +
		    ucDram_Register_Read(DRAMC_W2R_PAGE_HIT) +
		    ucDram_Register_Read(DRAMC_W2W_PAGE_HIT);
		break;
	default:
		return BM_ERR_WRONG_REQ;
	}

	return iCount;
}

unsigned int DRAMC_GetPageMissCount(const unsigned int CountType)
{
	unsigned int iCount;

	switch (CountType) {
	case DRAMC_R2R:
		iCount = ucDram_Register_Read(DRAMC_R2R_PAGE_MISS);
		break;

	case DRAMC_R2W:
		iCount = ucDram_Register_Read(DRAMC_R2W_PAGE_MISS);
		break;

	case DRAMC_W2R:
		iCount = ucDram_Register_Read(DRAMC_W2R_PAGE_MISS);
		break;

	case DRAMC_W2W:
		iCount = ucDram_Register_Read(DRAMC_W2W_PAGE_MISS);
		break;
	case DRAMC_ALL:
		iCount = ucDram_Register_Read(DRAMC_R2R_PAGE_MISS) +
				ucDram_Register_Read(DRAMC_R2W_PAGE_MISS) +
		    ucDram_Register_Read(DRAMC_W2R_PAGE_MISS) +
		    ucDram_Register_Read(DRAMC_W2W_PAGE_MISS);
		break;
	default:
		return BM_ERR_WRONG_REQ;
	}

	return iCount;
}

unsigned int DRAMC_GetInterbankCount(const unsigned int CountType)
{
	unsigned int iCount;

	switch (CountType) {
	case DRAMC_R2R:
		iCount = ucDram_Register_Read(DRAMC_R2R_INTERBANK);
		break;

	case DRAMC_R2W:
		iCount = ucDram_Register_Read(DRAMC_R2W_INTERBANK);
		break;

	case DRAMC_W2R:
		iCount = ucDram_Register_Read(DRAMC_W2R_INTERBANK);
		break;

	case DRAMC_W2W:
		iCount = ucDram_Register_Read(DRAMC_W2W_INTERBANK);
		break;
	case DRAMC_ALL:
		iCount = ucDram_Register_Read(DRAMC_R2R_INTERBANK) +
				ucDram_Register_Read(DRAMC_R2W_INTERBANK) +
		    ucDram_Register_Read(DRAMC_W2R_INTERBANK) +
		    ucDram_Register_Read(DRAMC_W2W_INTERBANK);
		break;
	default:
		return BM_ERR_WRONG_REQ;
	}

	return iCount;
}

unsigned int DRAMC_GetIdleCount(void)
{
	return ucDram_Register_Read(DRAMC_IDLE_COUNT);

}
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6797)

unsigned int BM_GetBWST(void)
{
	return readl(IOMEM(EMI_BWST));
}

unsigned int BM_GetBWST1(void)
{
	return readl(IOMEM(EMI_BWST1));
}

int BM_SetBW(const unsigned int BW_config)
{
	unsigned int value;

	value = readl(IOMEM(EMI_CONH));
	value &= 0xFFFF8007;
	value |= BW_config & 0x7FF8;
	mt_reg_sync_writel(value, EMI_CONH);

	return BM_REQ_OK;
}

int BM_SetBW1(const unsigned int BW_config)
{
	unsigned int value;

	value = readl(IOMEM(EMI_CONO));
	value &= 0xFF008007;
	value |= BW_config & 0xFF7FF8;
	mt_reg_sync_writel(value, EMI_CONO);

	return BM_REQ_OK;
}

unsigned int BM_GetBW(void)
{
	return readl(IOMEM(EMI_CONH)) & 0x7FF8;
}

unsigned int BM_GetBW1(void)
{
	return readl(IOMEM(EMI_CONO)) & 0xFF7FF8;
}
#endif

void *mt_emi_base_get(void)
{
	return EMI_BASE_ADDR;
}
EXPORT_SYMBOL(mt_emi_base_get);
