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
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
/* #include <mach/mtk_clkmgr.h> */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <asm/setup.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/dma.h>
#include <mt-plat/sync_write.h>
#ifdef DVFS_READY
#include <mtk_spm_vcore_dvfs.h>
#endif

#include "mtk_dramc.h"
#include "dramc.h"
#ifdef EMI_READY
#include "mt_emi_api.h"
#endif

#include <mt-plat/aee.h>
#include <mt-plat/mtk_chip.h>

void __iomem *SYS_TIMER_BASE_ADDR;
void __iomem *DRAMC_AO_CHA_BASE_ADDR;
void __iomem *DRAMC_AO_CHB_BASE_ADDR;
void __iomem *DRAMC_NAO_CHA_BASE_ADDR;
void __iomem *DRAMC_NAO_CHB_BASE_ADDR;
void __iomem *DDRPHY_AO_CHA_BASE_ADDR;
void __iomem *DDRPHY_AO_CHB_BASE_ADDR;
void __iomem *DDRPHY_NAO_CHA_BASE_ADDR;
void __iomem *DDRPHY_NAO_CHB_BASE_ADDR;
#define DRAM_RSV_SIZE 0x1000

#ifdef SW_TX_TRACKING
static unsigned int mr18_cur;
static unsigned int mr19_cur;
#endif

static DEFINE_MUTEX(dram_dfs_mutex);
unsigned int DRAM_TYPE;
unsigned int CH_NUM;
unsigned int CBT_MODE;

/*extern bool spm_vcorefs_is_dvfs_in_porgress(void);*/
#define Reg_Sync_Writel(addr, val)   writel(val, IOMEM(addr))
#define Reg_Readl(addr) readl(IOMEM(addr))

static unsigned int dram_rank_num;
static unsigned int dram_mr_mode;
#ifdef SW_TX_TRACKING
static unsigned int dram_sw_tx;
#endif
#ifdef SW_ZQCS
static unsigned int dram_sw_zq;
#endif


struct dram_info *g_dram_info_dummy_read, *get_dram_info;
struct dram_info dram_info_dummy_read;

static unsigned int cbt_mode_rank[2];

#define DRAMC_TAG "[DRAMC]"
#define DRAMC_RSV_TAG "[DRAMC_RSV]"

#define dramc_info(format, ...)	pr_info(DRAMC_TAG format, ##__VA_ARGS__)

__weak void *mt_spm_base_get(void)
{
	return 0;
}

static int __init dt_scan_dram_info(unsigned long node,
const char *uname, int depth, void *data)
{
	char *type = (char *)of_get_flat_dt_prop(node, "device_type", NULL);

	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0)
		return 0;

	if (node) {
		get_dram_info =
			(struct dram_info *)of_get_flat_dt_prop(node,
				"orig_dram_info", NULL);

		g_dram_info_dummy_read = &dram_info_dummy_read;
		dram_rank_num = get_dram_info->rank_num;
		dram_info_dummy_read.rank_num = dram_rank_num;

		dramc_info("dram info dram rank number = %d\n",
		g_dram_info_dummy_read->rank_num);

		dram_info_dummy_read.rank_info[0].start = 0;
		dram_info_dummy_read.rank_info[1].start = 0;
	}

	return node;
}

#if defined(SW_TX_TRACKING) || defined(DRAMC_MEMTEST_DEBUG_SUPPORT)
static unsigned int read_dram_mode_reg(
	unsigned int mr_index, unsigned int *mr_value,
	void __iomem *dramc_ao_chx_base, void __iomem *dramc_nao_chx_base)
{
	unsigned int response;
	unsigned int time_cnt;
	unsigned int temp;

	/* assign MR index */
	temp = Reg_Readl(DRAMC_AO_MRS) & ~(0x1FFF<<8);
	Reg_Sync_Writel(DRAMC_AO_MRS, temp | (mr_index<<8));

	/* fire MRR by MRREN 0->1 */
	temp = Reg_Readl(DRAMC_AO_SPCMD);
	Reg_Sync_Writel(DRAMC_AO_SPCMD, temp | 0x2);

	/* wait MRR finish response or timeout handling */
	time_cnt = 100;
	do {
		udelay(1);
		response = Reg_Readl(DRAMC_NAO_SPCMDRESP) & 0x2;
		time_cnt--;
	} while ((response == 0) && (time_cnt > 0));
	if (time_cnt == 0)
		return TX_TIMEOUT_MRR_ENABLE;

	/* Read out MR value or timeout handling */
	time_cnt = 10;
	do {
		udelay(1);
		*mr_value = Reg_Readl(DRAMC_NAO_MRR_STATUS) & 0xFFFF;
		time_cnt--;
	} while ((*mr_value == 0) && (time_cnt > 0));
#if 0
	if (time_cnt == 0)
		dramc_info("read mode reg time out 2\n");
#endif

	/* set MRR fire bit MRREN to 0 for next MRR */
	temp = Reg_Readl(DRAMC_AO_SPCMD);
	Reg_Sync_Writel(DRAMC_AO_SPCMD, temp & ~0x2);

	/* wait for the ready response */
	time_cnt = 100;
	do {
		udelay(1);
		response = Reg_Readl(DRAMC_NAO_SPCMDRESP) & 0x2;
		time_cnt--;
	} while ((response == 2) && (time_cnt > 0));
	if (time_cnt == 0)
		return TX_TIMEOUT_MRR_DISABLE;

	return TX_DONE;
}

#if defined(DRAMC_MEMTEST_DEBUG_SUPPORT)
unsigned int read_dram_mode_reg_by_rank(
	unsigned int mr_index, unsigned int *mr_value,
	unsigned int rank, unsigned int channel)
{
	unsigned int temp;
	void __iomem *dramc_ao_chx_base;
	void __iomem *dramc_nao_chx_base;
	void __iomem *ddrphy_chx_base;
	ssize_t ret;
	unsigned long save_flags;
	unsigned int res;
	unsigned int mr_rank_offset = (dram_mr_mode) ? 26 : 24;

	ret = 0;

	if (channel == 0) {
		dramc_ao_chx_base = DRAMC_AO_CHA_BASE_ADDR;
		dramc_nao_chx_base = DRAMC_NAO_CHA_BASE_ADDR;
		ddrphy_chx_base = DDRPHY_AO_CHA_BASE_ADDR;
	} else {
		dramc_ao_chx_base = DRAMC_AO_CHB_BASE_ADDR;
		dramc_nao_chx_base = DRAMC_NAO_CHB_BASE_ADDR;
		ddrphy_chx_base = DDRPHY_AO_CHB_BASE_ADDR;
	}

	local_irq_save(save_flags);

	if (acquire_dram_ctrl() != 0) {
		local_irq_restore(save_flags);
		return -1;
	}

	temp = Reg_Readl(DRAMC_AO_MRS) & ~(0x3<<mr_rank_offset);
	Reg_Sync_Writel(DRAMC_AO_MRS, temp | (rank<<mr_rank_offset));

	res = read_dram_mode_reg(mr_index, mr_value,
		dramc_ao_chx_base, dramc_nao_chx_base);
	if (res != TX_DONE)
		ret = -1;

	temp = Reg_Readl(DRAMC_AO_MRS) & ~(0x3<<mr_rank_offset);
	Reg_Sync_Writel(DRAMC_AO_MRS, temp);

	release_dram_ctrl();

	local_irq_restore(save_flags);

	return ret;
}
#endif
#endif

#ifdef SW_TX_TRACKING
static unsigned int start_dram_dqs_osc(
	void __iomem *dramc_ao_chx_base, void __iomem *dramc_nao_chx_base)
{
	unsigned int response;
	unsigned int time_cnt;
	unsigned int temp;
	unsigned int res;

	temp = Reg_Readl(DRAMC_AO_SPCMD) | (0x1<<10);
	Reg_Sync_Writel(DRAMC_AO_SPCMD, temp);

	time_cnt = 100;
	do {
		udelay(1);
		response = Reg_Readl(DRAMC_NAO_SPCMDRESP) & (0x1<<10);
		time_cnt--;
	} while ((response == 0) && (time_cnt > 0));

	if (time_cnt == 0)
		res = TX_TIMEOUT_DQSOSC;
	else
		res = TX_DONE;

	temp = Reg_Readl(DRAMC_AO_SPCMD) & ~(0x1<<10);
	Reg_Sync_Writel(DRAMC_AO_SPCMD, temp);

	return res;
}

static unsigned int auto_dram_dqs_osc(unsigned int rank,
void __iomem *dramc_ao_chx_base, void __iomem *dramc_nao_chx_base)
{
	unsigned int backup_mrs, backup_pd_ctrl, backup_ckectrl;
	unsigned int temp;
	unsigned int res;
	unsigned int mr_rank_offset = (dram_mr_mode) ? 26 : 24;

	backup_mrs = Reg_Readl(DRAMC_AO_MRS);
	backup_pd_ctrl = Reg_Readl(DRAMC_AO_PD_CTRL);
	backup_ckectrl = Reg_Readl(DRAMC_AO_CKECTRL);

	/* disable DQS OSC 2 ranks simutaneously and specify rank index */
	temp = Reg_Readl(DRAMC_AO_RKCFG) & ~(0x1<<11);
	Reg_Sync_Writel(DRAMC_AO_RKCFG, temp);
	temp = Reg_Readl(DRAMC_AO_MRS) & ~(0x3 << 24);
	Reg_Sync_Writel(DRAMC_AO_MRS, temp | (rank << 24));

	/* set DRAMC clock free run and CKE always on */
	temp = Reg_Readl(DRAMC_AO_PD_CTRL) & 0xFFFFFFFD;
	Reg_Sync_Writel(DRAMC_AO_PD_CTRL, temp);
	temp &= 0xBFFFFFFF;
	Reg_Sync_Writel(DRAMC_AO_PD_CTRL, temp);
	temp |= 0x1 << 26;
	Reg_Sync_Writel(DRAMC_AO_PD_CTRL, temp);
	if (rank == 0) {
		temp = Reg_Readl(DRAMC_AO_CKECTRL) & ~(0x1<<7);
		Reg_Sync_Writel(DRAMC_AO_CKECTRL, temp | (0x1<<6));
	} else {
		temp = Reg_Readl(DRAMC_AO_CKECTRL) & ~(0x1<<5);
		Reg_Sync_Writel(DRAMC_AO_CKECTRL, temp | (0x1<<4));
	}

	res = start_dram_dqs_osc(dramc_ao_chx_base, dramc_nao_chx_base);
	if (res != TX_DONE)
		goto ret_auto_dram_dqs_osc;
	udelay(1);
	temp = Reg_Readl(DRAMC_AO_MRS) & ~(0x3<<mr_rank_offset);
	Reg_Sync_Writel(DRAMC_AO_MRS, temp | (rank<<mr_rank_offset));
	res = read_dram_mode_reg(18, &mr18_cur,
		dramc_ao_chx_base, dramc_nao_chx_base);
	if (res != TX_DONE)
		goto ret_auto_dram_dqs_osc;
	res = read_dram_mode_reg(19, &mr19_cur,
		dramc_ao_chx_base, dramc_nao_chx_base);
	if (res != TX_DONE)
		goto ret_auto_dram_dqs_osc;

	res = TX_DONE;

#if 0 /* print message for debugging */
	/* byte 0 */
	dqs_cnt = (mr18_cur & 0xFF) | ((mr19_cur & 0xFF) << 8);
	/* sagy: our frequency is double data rate */
	if (dqs_cnt != 0)
		dqs_osc[0] = mr23_value*16000000/(dqs_cnt * frequency);
	else
		dqs_osc[0] = 0;
	/* byte 1 */
	dqs_cnt = (mr18_cur >> 8) | (mr19_cur & 0xFF00);
	/* sagy: our frequency is double data rate */
	if (dqs_cnt != 0)
		dqs_osc[1] = mr23_value*16000000/(dqs_cnt * frequency);
	else
		dqs_osc[1] = 0;

	dramc_info("Rank %d, (LSB)MR18= 0x%x, (MSB)MR19= 0x%x,",
		rank, mr18_cur, mr19_cur);
	dramc_info(" tDQSOscB0 = %d ps tDQSOscB1 = %d ps\n",
		dqs_osc[0], dqs_osc[1]);
#endif

ret_auto_dram_dqs_osc:
	Reg_Sync_Writel(DRAMC_AO_MRS, backup_mrs);
	Reg_Sync_Writel(DRAMC_AO_CKECTRL, backup_ckectrl);
	Reg_Sync_Writel(DRAMC_AO_PD_CTRL, backup_pd_ctrl);

	return res;
}

static unsigned int dramc_tx_tracking(int channel)
{
	void __iomem *dramc_ao_chx_base;
	void __iomem *dramc_nao_chx_base;
	void __iomem *ddrphy_chx_base;

	unsigned int shu_level;
	unsigned int shu_index;
	unsigned int shu_offset_dramc, shu_offset_ddrphy;
	unsigned int dqsosc_inc[2], dqsosc_dec[2];
	unsigned int pi_orig[3][2][2]; /* [shuffle][rank][byte] */
	unsigned int pi_new[3][2][2]; /* [shuffle][rank][byte] */
	unsigned int dqm_orig[3][2][2];
	unsigned int dqm_new[3][2][2];
	unsigned int pi_adjust;
	unsigned int mr1819_base[2][2];
	unsigned int mr1819_cur[2];
	unsigned int mr1819_delta;
	unsigned int mr4_on_off;
	unsigned int response;
	unsigned int time_cnt;
	unsigned int temp;
	unsigned int rank, byte;
	unsigned int tx_freq_ratio[3];
	unsigned int pi_adj, max_pi_adj[3];
	unsigned int res;

	if (channel == 0) {
		dramc_ao_chx_base = DRAMC_AO_CHA_BASE_ADDR;
		dramc_nao_chx_base = DRAMC_NAO_CHA_BASE_ADDR;
		ddrphy_chx_base = DDRPHY_AO_CHA_BASE_ADDR;
	} else {
		dramc_ao_chx_base = DRAMC_AO_CHB_BASE_ADDR;
		dramc_nao_chx_base = DRAMC_NAO_CHB_BASE_ADDR;
		ddrphy_chx_base = DDRPHY_AO_CHB_BASE_ADDR;
	}

	shu_level = (Reg_Readl(DRAMC_AO_SHUSTATUS) >> 1) & 0x3;

	tx_freq_ratio[0] = dram_steps_freq(0) * 8 / dram_steps_freq(shu_level);
	tx_freq_ratio[1] = dram_steps_freq(1) * 8 / dram_steps_freq(shu_level);
	tx_freq_ratio[2] = dram_steps_freq(2) * 8 / dram_steps_freq(shu_level);

	max_pi_adj[0] = 10;
	max_pi_adj[1] = 7;
	max_pi_adj[2] = 4;

	shu_offset_dramc = 0x600 * shu_level;
	dqsosc_inc[0] = (Reg_Readl(
		DRAMC_AO_DQSOSCTHRD + shu_offset_dramc) >>  0) & 0xFFF;
	dqsosc_dec[0] = (Reg_Readl(
		DRAMC_AO_DQSOSCTHRD + shu_offset_dramc) >> 12) & 0xFFF;
	dqsosc_inc[1] = (Reg_Readl(
		DRAMC_AO_DQSOSC_PRD + shu_offset_dramc) >>  8) & 0xF00;
	dqsosc_inc[1] |= (Reg_Readl(
		DRAMC_AO_DQSOSCTHRD + shu_offset_dramc) >> 24) & 0xFF;
	dqsosc_dec[1] = (Reg_Readl(
		DRAMC_AO_DQSOSC_PRD + shu_offset_dramc) >> 20) & 0xFFF;

	/* mr1819_base[rank][byte] */
	mr1819_base[0][0] = (Reg_Readl(
		DRAMC_AO_SHU1RK0_DQSOSC + shu_offset_dramc) >>  0) & 0xFFFF;
	mr1819_base[1][0] = (Reg_Readl(
		DRAMC_AO_SHU1RK1_DQSOSC + shu_offset_dramc) >>  0) & 0xFFFF;
	if (CBT_MODE == BYTE_MODE) {
		mr1819_base[0][1] = (Reg_Readl(
			DRAMC_AO_SHU1RK0_DQSOSC + shu_offset_dramc) >> 16)
			& 0xFFFF;
		mr1819_base[1][1] = (Reg_Readl(
			DRAMC_AO_SHU1RK1_DQSOSC + shu_offset_dramc) >> 16)
			& 0xFFFF;
	} else if (CBT_MODE == R0_NORMAL_R1_BYTE) {
		mr1819_base[0][1] = mr1819_base[0][0];
		mr1819_base[1][1] = (Reg_Readl(
			DRAMC_AO_SHU1RK1_DQSOSC + shu_offset_dramc) >> 16)
			& 0xFFFF;
	} else if (CBT_MODE == R0_BYTE_R1_NORMAL) {
		mr1819_base[0][1] = (Reg_Readl(
			DRAMC_AO_SHU1RK0_DQSOSC + shu_offset_dramc) >> 16)
			& 0xFFFF;
		mr1819_base[1][1] = mr1819_base[1][0];
	} else { /* normal mode */
		mr1819_base[0][1] = mr1819_base[0][0];
		mr1819_base[1][1] = mr1819_base[1][0];
	}

	/* pi_orig[shuffle][rank][byte] */
	for (shu_index = 0; shu_index < 3; shu_index++) {
		shu_offset_dramc = 0x600 * shu_index;
		temp = Reg_Readl(DRAMC_AO_SHU1RK0_PI + shu_offset_dramc);
		pi_orig[shu_index][0][0] = (temp >> 8) & 0x3F;
		pi_orig[shu_index][0][1] = (temp >> 0) & 0x3F;
		dqm_orig[shu_index][0][0] = (temp >> 24) & 0x3F;
		dqm_orig[shu_index][0][1] = (temp >> 16) & 0x3F;

		temp = Reg_Readl(DRAMC_AO_SHU1RK1_PI + shu_offset_dramc);
		pi_orig[shu_index][1][0] = (temp >> 8) & 0x3F;
		pi_orig[shu_index][1][1] = (temp >> 0) & 0x3F;
		dqm_orig[shu_index][1][0] = (temp >> 24) & 0x3F;
		dqm_orig[shu_index][1][1] = (temp >> 16) & 0x3F;
	}

	temp = Reg_Readl(DRAMC_AO_SPCMDCTRL);
	mr4_on_off = (temp >> 29) & 0x1;
	Reg_Sync_Writel(DRAMC_AO_SPCMDCTRL, temp | (1<<29));
	for (rank = 0; rank < 2; rank++) {
		res = auto_dram_dqs_osc(
			rank, dramc_ao_chx_base, dramc_nao_chx_base);
		if (res != TX_DONE)
			goto ret_dramc_tx_tracking;
		mr1819_cur[0] = (mr18_cur & 0xFF) | ((mr19_cur & 0xFF) << 8);
		if (cbt_mode_rank[rank] == RANK_BYTE)
			mr1819_cur[1] = (mr18_cur >> 8) | (mr19_cur & 0xFF00);
		else /* Normal Mode */
			mr1819_cur[1] = mr1819_cur[0];

		/* inc: mr1819_cur > mr1819_base, PI- */
		/* dec: mr1819_cur < mr1819_base, PI+ */
		for (byte = 0; byte < 2; byte++) {
			if (mr1819_cur[byte] >= mr1819_base[rank][byte]) {
				mr1819_delta =
				mr1819_cur[byte] - mr1819_base[rank][byte];
				pi_adjust = mr1819_delta / dqsosc_inc[rank];
				for (shu_index = 0; shu_index < 3;
						shu_index++) {
					pi_adj = pi_adjust *
						tx_freq_ratio[shu_index] /
						tx_freq_ratio[shu_level];
					if (pi_adj > max_pi_adj[shu_index]) {
						res = TX_FAIL_VARIATION;
						goto ret_dramc_tx_tracking;
					}
					pi_new[shu_index][rank][byte] =
						(pi_orig[shu_index][rank][byte]
						 - pi_adj) & 0x3F;
					dqm_new[shu_index][rank][byte] =
						(dqm_orig[shu_index][rank][byte]
						 - pi_adj) & 0x3F;
#if 0 /* print message for debugging */
dramc_info("CH%d RK%d B%d, shu=%d base=%X cur=%X ",
channel, rank, byte, shu_index, mr1819_base[rank][byte], mr1819_cur[byte]);
dramc_info("delta=%d INC=%d PI=0x%x Adj=%d newPI=0x%x\n",
mr1819_delta, dqsosc_inc[rank], pi_orig[shu_index][rank][byte],
(pi_adjust * tx_freq_ratio[shu_index] / tx_freq_ratio[shu_level]),
pi_new[shu_index][rank][byte]);
#endif
				}
			} else {
				mr1819_delta = mr1819_base[rank][byte] -
					mr1819_cur[byte];
				pi_adjust = mr1819_delta / dqsosc_dec[rank];
				for (shu_index = 0; shu_index < 3;
						shu_index++) {
					pi_adj = pi_adjust *
						tx_freq_ratio[shu_index] /
						tx_freq_ratio[shu_level];
					if (pi_adj > max_pi_adj[shu_index]) {
						res = TX_FAIL_VARIATION;
						goto ret_dramc_tx_tracking;
					}
					pi_new[shu_index][rank][byte] =
						(pi_orig[shu_index][rank][byte]
						 + pi_adj) & 0x3F;
					dqm_new[shu_index][rank][byte] =
						(dqm_orig[shu_index][rank][byte]
						 + pi_adj) & 0x3F;
#if 0 /* print message for debugging */
dramc_info("CH%d RK%d B%d, shu=%d base=%X cur=%X "
channel, rank, byte, shu_index, mr1819_base[rank][byte], mr1819_cur[byte]);
dramc_info("delta=%d DEC=%d PI=0x%x Adj=%d newPI=0x%x\n",
mr1819_delta, dqsosc_dec[rank], pi_orig[shu_index][rank][byte],
(pi_adjust * tx_freq_ratio[shu_index] / tx_freq_ratio[shu_level]),
pi_new[shu_index][rank][byte]);
#endif
				}
			}
		}
	}

	temp = Reg_Readl(DRAMC_AO_DQSOSCR);
	Reg_Sync_Writel(DRAMC_AO_DQSOSCR, temp | (0x1<<5));
	Reg_Sync_Writel(DRAMC_AO_DQSOSCR, temp | (0x3<<5));

	for (shu_index = 0; shu_index < 3; shu_index++) {
		shu_offset_ddrphy = 0x500 * shu_index;
		temp = Reg_Readl(DDRPHY_SHU1_R0_B0_DQ7 + shu_offset_ddrphy) &
			~((0x3F << 8) | (0x3F << 16));
		Reg_Sync_Writel(DDRPHY_SHU1_R0_B0_DQ7 + shu_offset_ddrphy,
				temp | (dqm_new[shu_index][0][0] << 16) |
				(pi_new[shu_index][0][0] << 8));
		temp = Reg_Readl(DDRPHY_SHU1_R0_B1_DQ7 + shu_offset_ddrphy) &
			~((0x3F << 8) | (0x3F << 16));
		Reg_Sync_Writel(DDRPHY_SHU1_R0_B1_DQ7 + shu_offset_ddrphy,
				temp | (dqm_new[shu_index][0][1] << 16) |
				(pi_new[shu_index][0][1] << 8));
		temp = Reg_Readl(DDRPHY_SHU1_R1_B0_DQ7 + shu_offset_ddrphy) &
			~((0x3F << 8) | (0x3F << 16));
		Reg_Sync_Writel(DDRPHY_SHU1_R1_B0_DQ7 + shu_offset_ddrphy,
				temp | (dqm_new[shu_index][1][0] << 16) |
				(pi_new[shu_index][1][0] << 8));
		temp = Reg_Readl(DDRPHY_SHU1_R1_B1_DQ7 + shu_offset_ddrphy) &
			~((0x3F << 8) | (0x3F << 16));
		Reg_Sync_Writel(DDRPHY_SHU1_R1_B1_DQ7 + shu_offset_ddrphy,
				temp | (dqm_new[shu_index][1][1] << 16) |
				(pi_new[shu_index][1][1] << 8));
	}

	time_cnt = 100;
	do {
		udelay(1);
		response = Reg_Readl(DRAMC_NAO_MISC_STATUSA) & (1 << 29);
		time_cnt--;
	} while ((response == 0) && (time_cnt > 0));
	if (time_cnt == 0) {
		dramc_info("write DDRPHY time out\n");
		res = TX_TIMEOUT_DDRPHY;
	} else
		res = TX_DONE;

ret_dramc_tx_tracking:
	temp = Reg_Readl(DRAMC_AO_DQSOSCR);
	Reg_Sync_Writel(DRAMC_AO_DQSOSCR, temp & ~(0x1<<5));
	Reg_Sync_Writel(DRAMC_AO_DQSOSCR, temp & ~(0x3<<5));

	temp = Reg_Readl(DRAMC_AO_SPCMDCTRL) & ~(1<<29);
	Reg_Sync_Writel(DRAMC_AO_SPCMDCTRL, temp | (mr4_on_off<<29));

	return res;
}

void dump_tx_log(unsigned int res)
{
	switch (res) {
	case TX_TIMEOUT_MRR_ENABLE:
		dramc_info("TX MRR enable timeout\n");
		break;
	case TX_TIMEOUT_MRR_DISABLE:
		dramc_info("TX MRR disable timeout\n");
		break;
	case TX_TIMEOUT_DQSOSC:
		dramc_info("TX DQS OSC timeout\n");
		break;
	case TX_TIMEOUT_DDRPHY:
		dramc_info("TX DDRPHY update timeout\n");
		break;
	case TX_FAIL_DATA_RATE:
		dramc_info("TX read data rate fail\n");
		break;
	case TX_FAIL_VARIATION:
		dramc_info("TX variation is too large\n");
		break;
	default:
		dramc_info("TX unknown error\n");
		break;
	}
}
#endif

#ifdef CONFIG_MTK_DRAMC_PASR
#define __ETT__ 0
int enter_pasr_dpd_config(unsigned char segment_rank0,
			   unsigned char segment_rank1)
{
	unsigned int rank_pasr_segment[2];
	unsigned int iRankIdx = 0, iChannelIdx = 0, cnt = 1000;
	unsigned int u4value_24 = 0;
	unsigned int u4value_64 = 0;
	unsigned int u4value_38 = 0;
	void __iomem *u4rg_24; /* CKE control */
	void __iomem *u4rg_64; /* MR4 ZQCS */
	void __iomem *u4rg_38; /* MIOCKCTRLOFF */
	void __iomem *u4rg_5C; /* MRS */
	void __iomem *u4rg_60; /* MRWEN */
	void __iomem *u4rg_88; /* MRW_RESPONSE */
#if !__ETT__
	unsigned long save_flags;

	dramc_info("PASR r0 = 0x%x  r1 = 0x%x\n",
			(segment_rank0 & 0xFF), (segment_rank1 & 0xFF));
	local_irq_save(save_flags);
	if (acquire_dram_ctrl() != 0) {
		local_irq_restore(save_flags);
		return -1;
	}
	/* dramc_info("get SPM HW SEMAPHORE!\n"); */
#endif
	rank_pasr_segment[0] = segment_rank0 & 0xFF; /* for rank0 */
	rank_pasr_segment[1] = segment_rank1 & 0xFF; /* for rank1 */

	/*
	 *dramc_info("PASR r0 = 0x%x  r1 = 0x%x\n",
	 *	rank_pasr_segment[0], rank_pasr_segment[1]);
	 */


/* #if PASR_TEST_SCENARIO == PASR_SUPPORT_2_CHANNEL*/
#ifdef EMI_READY
	for (iChannelIdx = 0; iChannelIdx < get_ch_num(); iChannelIdx++) {
#else
	for (iChannelIdx = 0; iChannelIdx < 2; iChannelIdx++) {
#endif
		if ((DRAM_TYPE == TYPE_LPDDR4) || (DRAM_TYPE == TYPE_LPDDR4X)) {
			if (iChannelIdx == 0) { /*Channel-A*/
				u4rg_24 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x24);
				u4rg_64 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x64);
				u4rg_38 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x38);
				u4rg_5C = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x5C);
				u4rg_60 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x60);
				u4rg_88 = IOMEM(DRAMC_NAO_CHA_BASE_ADDR + 0x88);
			} else { /*Channel-B*/
				u4rg_24 = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x24);
				u4rg_64 = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x64);
				u4rg_38 = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x38);
				u4rg_5C = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x5C);
				u4rg_60 = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x60);
				u4rg_88 = IOMEM(DRAMC_NAO_CHB_BASE_ADDR + 0x88);
			}
		} else if (DRAM_TYPE == TYPE_LPDDR3) {
			if (iChannelIdx == 1)
				break;
			u4rg_24 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x24);
			u4rg_64 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x64);
			u4rg_38 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x38);
			u4rg_5C = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x5C);
			u4rg_60 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x60);
			u4rg_88 = IOMEM(DRAMC_NAO_CHA_BASE_ADDR + 0x88);
		} else {
			break;
		}

		u4value_64 = readl(u4rg_64);
		u4value_38 = readl(u4rg_38);
		u4value_24 = readl(u4rg_24);

		/* Disable MR4 => 0x64[29] = 1 */
		writel(readl(u4rg_64) | 0x20000000, u4rg_64);
		/* Disable ZQCS => LPDDR4: 0x64[30] = 0 LPDDR3: 0x64[31] = 0 */
		writel(readl(u4rg_64) & 0x3FFFFFFF, u4rg_64);
#if !__ETT__
		mb(); /* flush memory */
#endif
		udelay(2);
		/* DCMEN2 = 0 */
		writel(readl(u4rg_38) & 0xFFFFFFFD, u4rg_38);
		/* PHYCLKDYNGEN = 0 */
		writel(readl(u4rg_38) & 0xBFFFFFFF, u4rg_38);
		/* MIOCKCTRLOFF = 1 */
		writel(readl(u4rg_38) | 0x04000000, u4rg_38);
		writel((readl(u4rg_24) & (~((0x1<<5) | (0x1<<7)))) |
				((0x1<<4) | (0x1<<6)), u4rg_24);
		/* CKE0 CKE1 fix on no matter the setting of CKE2RANK*/
#ifdef EMI_READY
		for (iRankIdx = 0; iRankIdx < get_rk_num(); iRankIdx++) {
#else
		for (iRankIdx = 0; iRankIdx < 2; iRankIdx++) {
#endif
			writel(((iRankIdx << 24) | rank_pasr_segment[iRankIdx] |
						(0x00000011 << 8)), u4rg_5C);
			writel(readl(u4rg_60) | 0x00000001, u4rg_60);
			cnt = 1000;
			do {
				if (cnt-- == 0) {
					dramc_info("R%d PASR MRW fail!\n",
						(iRankIdx == 0) ? 0 : 1);
#if !__ETT__
					release_dram_ctrl();
					local_irq_restore(save_flags);
#endif
					return -1;
				}
				udelay(1);
			} while ((readl(u4rg_88) & 0x00000001) == 0x0);
			writel(readl(u4rg_60) & 0xfffffffe, u4rg_60);
		}
		writel(u4value_64, u4rg_64);
		writel(u4value_24, u4rg_24);
		writel(u4value_38, u4rg_38);
		writel(0, u4rg_5C);
}
#if !__ETT__
	release_dram_ctrl();

	local_irq_restore(save_flags);
#endif
	return 0;
}

int exit_pasr_dpd_config(void)
{
	int ret;

	ret = enter_pasr_dpd_config(0, 0);

	return ret;
}
#else
int enter_pasr_dpd_config(
	unsigned char segment_rank0, unsigned char segment_rank1)
{
	return 0;
}

int exit_pasr_dpd_config(void)
{
	return 0;
}
#endif

#define MEM_TEST_SIZE 0x2000
#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5
int Binning_DRAM_complex_mem_test(void)
{
	unsigned char *MEM8_BASE;
	unsigned short *MEM16_BASE;
	unsigned int *MEM32_BASE;
	unsigned int *MEM_BASE;
	unsigned long mem_ptr;
	unsigned char pattern8;
	unsigned short pattern16;
	unsigned int i, j, size, pattern32;
	unsigned int value;
	unsigned int len = MEM_TEST_SIZE;
	void *ptr;
	int ret = 1;

	ptr = vmalloc(MEM_TEST_SIZE);

	if (!ptr) {
		/*dramc_info("fail to vmalloc\n");*/
		/*ASSERT(0);*/
		ret = -24;
		goto fail;
	}

	MEM8_BASE = (unsigned char *)ptr;
	MEM16_BASE = (unsigned short *)ptr;
	MEM32_BASE = (unsigned int *)ptr;
	MEM_BASE = (unsigned int *)ptr;
	/* dramc_info("Test DRAM start address 0x%lx\n", (unsigned long)ptr); */
	dramc_info("Test DRAM start address %p\n", ptr);
	dramc_info("Test DRAM SIZE 0x%x\n", MEM_TEST_SIZE);
	size = len >> 2;

	/* === Verify the tied bits (tied high) === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0;

	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0) {
			/* return -1; */
			ret = -1;
			goto fail;
		} else
			MEM32_BASE[i] = 0xffffffff;
	}

	/* === Verify the tied bits (tied low) === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			/* return -2; */
			ret = -2;
			goto fail;
		} else
			MEM32_BASE[i] = 0x00;
	}

	/* === Verify pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = 0; i < len; i++)
		MEM8_BASE[i] = pattern8++;
	pattern8 = 0x00;
	for (i = 0; i < len; i++) {
		if (MEM8_BASE[i] != pattern8++) {
			/* return -3; */
			ret = -3;
			goto fail;
		}
	}

	/* === Verify pattern 2 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = j = 0; i < len; i += 2, j++) {
		if (MEM8_BASE[i] == pattern8)
			MEM16_BASE[j] = pattern8;
		if (MEM16_BASE[j] != pattern8) {
			/* return -4; */
			ret = -4;
			goto fail;
		}
		pattern8 += 2;
	}

	/* === Verify pattern 3 (0x00~0xffff) === */
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++)
		MEM16_BASE[i] = pattern16++;
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++) {
		if (MEM16_BASE[i] != pattern16++) {
			/* return -5; */
			ret = -5;
			goto fail;
		}
	}

	/* === Verify pattern 4 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++)
		MEM32_BASE[i] = pattern32++;
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++) {
		if (MEM32_BASE[i] != pattern32++) {
			/* return -6; */
			ret = -6;
			goto fail;
		}
	}

	/* === Pattern 5: Filling memory range with 0x44332211 === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0x44332211;

	/* === Read Check then Fill Memory with a5a5a5a5 Pattern === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x44332211) {
			/* return -7; */
			ret = -7;
			goto fail;
		} else {
			MEM32_BASE[i] = 0xa5a5a5a5;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 0h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a5a5) {
			/* return -8; */
			ret = -8;
			goto fail;
		} else {
			MEM8_BASE[i * 4] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 2h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a500) {
			/* return -9; */
			ret = -9;
			goto fail;
		} else {
			MEM8_BASE[i * 4 + 2] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 1h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa500a500) {
			/* return -10; */
			ret = -10;
			goto fail;
		} else {
			MEM8_BASE[i * 4 + 1] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 3h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5000000) {
			/* return -11; */
			ret = -11;
			goto fail;
		} else {
			MEM8_BASE[i * 4 + 3] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with ffff */
	/* Word Pattern at offset 1h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x00000000) {
			/* return -12; */
			ret = -12;
			goto fail;
		} else {
			MEM16_BASE[i * 2 + 1] = 0xffff;
		}
	}

	/* === Read Check then Fill Memory with ffff */
	/* Word Pattern at offset 0h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffff0000) {
			/* return -13; */
			ret = -13;
			goto fail;
		} else {
			MEM16_BASE[i * 2] = 0xffff;
		}
	}
    /*===  Read Check === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			/* return -14; */
			ret = -14;
			goto fail;
		}
	}

    /************************************************
     * Additional verification
     ************************************************/
	/* === stage 1 => write 0 === */

	for (i = 0; i < size; i++)
		MEM_BASE[i] = PATTERN1;

	/* === stage 2 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];

		if (value != PATTERN1) {
			/* return -15; */
			ret = -15;
			goto fail;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 3 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			/* return -16; */
			ret = -16;
			goto fail;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 4 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			/* return -17; */
			ret = -17;
			goto fail;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 5 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			/* return -18; */
			ret = -18;
			goto fail;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 6 => read 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			/* return -19; */
			ret = -19;
			goto fail;
		}
	}

	/* === 1/2/4-byte combination test === */
	mem_ptr = (unsigned long)MEM_BASE;
	while (mem_ptr < ((unsigned long)MEM_BASE + (size << 2))) {
		*((unsigned char *)mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *)mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *)mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *)mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned char *)mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *)mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *)mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *)mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
	}
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != 0x12345678) {
			/* return -20; */
			ret = -20;
			goto fail;
		}
	}

	/* === Verify pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	MEM8_BASE[0] = pattern8;
	for (i = 0; i < size * 4; i++) {
		unsigned char waddr8, raddr8;

		waddr8 = i + 1;
		raddr8 = i;
		if (i < size * 4 - 1)
			MEM8_BASE[waddr8] = pattern8 + 1;
		if (MEM8_BASE[raddr8] != pattern8) {
			/* return -21; */
			ret = -21;
			goto fail;
		}
		pattern8++;
	}

	/* === Verify pattern 2 (0x00~0xffff) === */
	pattern16 = 0x00;
	MEM16_BASE[0] = pattern16;
	for (i = 0; i < size * 2; i++) {
		if (i < size * 2 - 1)
			MEM16_BASE[i + 1] = pattern16 + 1;
		if (MEM16_BASE[i] != pattern16) {
			/* return -22; */
			ret = -22;
			goto fail;
		}
		pattern16++;
	}
	/* === Verify pattern 3 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	MEM32_BASE[0] = pattern32;
	for (i = 0; i < size; i++) {
		if (i < size - 1)
			MEM32_BASE[i + 1] = pattern32 + 1;
		if (MEM32_BASE[i] != pattern32) {
			/* return -23; */
			ret = -23;
			goto fail;
		}
		pattern32++;
	}
	dramc_info("complex R/W mem test pass\n");

fail:
	vfree(ptr);
	return ret;
}

unsigned int ucDram_Register_Read(unsigned int u4reg_addr)
{
	return 0; /* for mbw dummy use */
}

unsigned int lpDram_Register_Read(unsigned int Reg_base, unsigned int Offset)
{
	if ((Reg_base == DRAMC_NAO_CHA) && (Offset < 0x1000))
		return readl(IOMEM(DRAMC_NAO_CHA_BASE_ADDR + Offset));

	else if ((Reg_base == DRAMC_NAO_CHB) && (Offset < 0x1000))
		return readl(IOMEM(DRAMC_NAO_CHB_BASE_ADDR + Offset));

	else if ((Reg_base == DRAMC_AO_CHA) && (Offset < 0x2000))
		return readl(IOMEM(DRAMC_AO_CHA_BASE_ADDR + Offset));

	else if ((Reg_base == DRAMC_AO_CHB) && (Offset < 0x2000))
		return readl(IOMEM(DRAMC_AO_CHB_BASE_ADDR + Offset));

	else if ((Reg_base == PHY_NAO_CHA) && (Offset < 0x1000))
		return readl(IOMEM(DDRPHY_NAO_CHA_BASE_ADDR + Offset));

	else if ((Reg_base == PHY_NAO_CHB) && (Offset < 0x1000))
		return readl(IOMEM(DDRPHY_NAO_CHB_BASE_ADDR + Offset));

	else if ((Reg_base == PHY_AO_CHA) && (Offset < 0x2000))
		return readl(IOMEM(DDRPHY_AO_CHA_BASE_ADDR + Offset));

	else if ((Reg_base == PHY_AO_CHB) && (Offset < 0x2000))
		return readl(IOMEM(DDRPHY_AO_CHB_BASE_ADDR + Offset));

	else
		return 0;
}
EXPORT_SYMBOL(lpDram_Register_Read);

/************************************************
 * CL#46077
 *************************************************/
unsigned int get_dram_data_rate(void)
{
	unsigned int u4PllIdx, u4ShuLevel, u4SDM_PCW, u4PREDIV, u4POSDIV;
	unsigned int u4CKDIV4, u4VCOFreq, u4DataRate = 0;
	unsigned int pcw_ofs[2] = { 0xd9c, 0xd94 };
	unsigned int prepost_ofs[2] = { 0xda8, 0xda0 };
	int channels;

	channels = get_emi_ch_num();
	u4ShuLevel = get_shuffle_status();

	/* only CHA have this bit */
	u4PllIdx = readl(
		IOMEM(DDRPHY_AO_CHA_BASE_ADDR + 0x510)) >> 31 & 0x00000001;

	u4SDM_PCW = readl(
		IOMEM(DDRPHY_AO_CHA_BASE_ADDR + pcw_ofs[u4PllIdx] +
			(0x500 * u4ShuLevel))) >> 16;
	u4PREDIV = (readl(
		IOMEM(DDRPHY_AO_CHA_BASE_ADDR + prepost_ofs[u4PllIdx] +
			(0x500 * u4ShuLevel))) & 0x000c0000) >> 18;
	u4POSDIV = readl(
		IOMEM(DDRPHY_AO_CHA_BASE_ADDR + prepost_ofs[u4PllIdx] +
			(0x500 * u4ShuLevel))) & 0x00000007;
	u4CKDIV4 = (readl(
		IOMEM(DDRPHY_AO_CHA_BASE_ADDR + 0xd18 +
			(0x500 * u4ShuLevel))) & 0x08000000) >> 27;

	u4VCOFreq = ((52>>u4PREDIV)*(u4SDM_PCW>>8))>>u4POSDIV;

	u4DataRate = u4VCOFreq>>u4CKDIV4;

	/*
	 *dramc_info("PCW=0x%X, u4PREDIV=%d, u4POSDIV=%d,"
	 *	"  CKDIV4=%d, DataRate=%d\n",
	 *	u4SDM_PCW, u4PREDIV, u4POSDIV, u4CKDIV4, u4DataRate);
	 */

	if (DRAM_TYPE == TYPE_LPDDR3) {
		if (u4DataRate == 1859)
			u4DataRate = 1866;
		else if (u4DataRate == 1599)
			u4DataRate = 1600;
		else if (u4DataRate == 1534)
			u4DataRate = 1534;
		else if (u4DataRate == 1196)
			u4DataRate = 1200;
		else
			u4DataRate = 0;
	} else if ((DRAM_TYPE == TYPE_LPDDR4) || (DRAM_TYPE == TYPE_LPDDR4X)) {
		if (u4DataRate == 3198)
			u4DataRate = 3200;
		else if (u4DataRate == 3068)
			u4DataRate = 3068;
		else if (u4DataRate == 2392)
			u4DataRate = 2400;
		else if (u4DataRate == 1599)
			u4DataRate = 1600;
		else if (u4DataRate == 1534)
			u4DataRate = 1534;
		else
			u4DataRate = 0;
	} else
		u4DataRate = 0;

	return u4DataRate;
}
EXPORT_SYMBOL(get_dram_data_rate);

unsigned int read_dram_temperature(unsigned char channel)
{
	unsigned int value = 0;

	if (channel == CHANNEL_A) {
		value =
		(readl(IOMEM(DRAMC_NAO_CHA_BASE_ADDR + 0x3b8)) >> 8) & 0x7;
	}	else if (channel == CHANNEL_B) {
		value =
		(readl(IOMEM(DRAMC_NAO_CHB_BASE_ADDR + 0x3b8)) >> 8) & 0x7;
		}

	return value;
}

unsigned int get_shuffle_status(void)
{
	return (readl(PDEF_DRAMC0_CHA_REG_0E4) & 0x6) >> 1;
	/* HPM = 0, LPM = 1, ULPM = 2; */

}

int get_ddr_type(void)
{
	return DRAM_TYPE;

}
EXPORT_SYMBOL(get_ddr_type);

int get_emi_ch_num(void)
{
#ifdef EMI_READY
	return get_ch_num();
#else
	return 0;
#endif
}
EXPORT_SYMBOL(get_emi_ch_num);

int dram_steps_freq(unsigned int step)
{
	int freq = -1;

	/* step here means ddr shuffle */

	switch (step) {
	case 0:
		if (DRAM_TYPE == TYPE_LPDDR3)
			freq = 1866;
		else if ((DRAM_TYPE == TYPE_LPDDR4) ||
				(DRAM_TYPE == TYPE_LPDDR4X))
			freq = 3200;
		break;
	case 1:
		if (DRAM_TYPE == TYPE_LPDDR3)
			freq = 1534;
		else if ((DRAM_TYPE == TYPE_LPDDR4) ||
				(DRAM_TYPE == TYPE_LPDDR4X))
			freq = 2400;
		break;
	case 2:
		if (DRAM_TYPE == TYPE_LPDDR3)
			freq = 1200;
		else if ((DRAM_TYPE == TYPE_LPDDR4) ||
				(DRAM_TYPE == TYPE_LPDDR4X))
			freq = 1534;
		break;
	default:
		return -1;
	}
	return freq;
}
EXPORT_SYMBOL(dram_steps_freq);

int dram_can_support_fh(void)
{
	return 1;
}
EXPORT_SYMBOL(dram_can_support_fh);

static ssize_t complex_mem_test_show(struct device_driver *driver, char *buf)
{
	int ret;

	ret = Binning_DRAM_complex_mem_test();
	if (ret > 0)
		return snprintf(buf, PAGE_SIZE, "MEM Test all pass\n");
	else
		return snprintf(buf, PAGE_SIZE, "MEM TEST failed %d\n", ret);
}

static ssize_t complex_mem_test_store(struct device_driver *driver,
const char *buf, size_t count)
{
	/*snprintf(buf, "do nothing\n");*/
	return count;
}

static ssize_t read_dram_data_rate_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DRAM data rate = %d\n",
	get_dram_data_rate());
}

static ssize_t read_dram_data_rate_store(struct device_driver *driver,
const char *buf, size_t count)
{
	return count;
}

DRIVER_ATTR(emi_clk_mem_test, 0664,
complex_mem_test_show, complex_mem_test_store);

DRIVER_ATTR(read_dram_data_rate, 0664,
read_dram_data_rate_show, read_dram_data_rate_store);

/*DRIVER_ATTR(dram_dfs, 0664, dram_dfs_show, dram_dfs_store);*/
static struct timer_list zqcs_timer;
static unsigned char low_freq_counter;
DEFINE_SPINLOCK(sw_zq_tx_lock);

void zqcs_timer_callback(unsigned long data)
{
#ifdef SW_ZQCS
	unsigned int Response, TimeCnt, CHCounter, RankCounter;
	void __iomem *u4rg_24;
	void __iomem *u4rg_38;
	void __iomem *u4rg_5C;
	void __iomem *u4rg_60;
	void __iomem *u4rg_88;
#endif

#ifdef SW_TX_TRACKING
	unsigned int res[2];
#endif

#if defined(SW_ZQCS) || defined(SW_TX_TRACKING)
	unsigned long save_flags, spinlock_save_flags;
#ifdef DVFS_READY
	unsigned int timeout;
#endif

	if ((get_dram_data_rate() >= 3200) || (low_freq_counter >= 10))
		low_freq_counter = 0;
	else {
		low_freq_counter++;
		mod_timer(&zqcs_timer, jiffies + msecs_to_jiffies(280));
		return;
	}

#ifdef DVFS_READY
	if (mt_spm_base_get()) {
		if (spm_vcorefs_get_md_srcclkena()) {
			spm_request_dvfs_opp(0, OPP_1);
			for (timeout = 100; timeout; timeout--) {
				if (get_dram_data_rate() >= 3200)
					break;
				udelay(1);
			}
			if (timeout == 0)
				dramc_info("request OPP0 timeout!\n");
			else
				udelay(100);
		}
	}
#endif
#endif

#ifdef SW_ZQCS
	spin_lock_irqsave(&sw_zq_tx_lock, spinlock_save_flags);
	local_irq_save(save_flags);

	if (!dram_sw_zq)
		goto tx_start;

	if (acquire_dram_ctrl() != 0)
		goto tx_start;

#ifdef DVFS_READY
	writel(readl(PDEF_SYS_TIMER), PDEF_SPM_TX_TIMESTAMP);
#endif
  /* CH0_Rank0 --> CH1Rank0 */
#ifdef EMI_READY
	for (RankCounter = 0; RankCounter < get_rk_num(); RankCounter++) {
#else
	for (RankCounter = 0; RankCounter < 2; RankCounter++) {
#endif
#ifdef EMI_READY
		for (CHCounter = 0; CHCounter < get_ch_num(); CHCounter++) {
#else
		for (CHCounter = 0; CHCounter < 2; CHCounter++) {
#endif
			TimeCnt = 100;

			if (CHCounter == 0) {
				u4rg_24 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x24);
				u4rg_38 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x38);
				u4rg_5C = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x5C);
				u4rg_60 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x60);
				u4rg_88 = IOMEM(DRAMC_NAO_CHA_BASE_ADDR + 0x88);
			} else if (CHCounter == 1) {
				u4rg_24 = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x24);
				u4rg_38 = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x38);
				u4rg_5C = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x5C);
				u4rg_60 = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x60);
				u4rg_88 = IOMEM(DRAMC_NAO_CHB_BASE_ADDR + 0x88);
			}

			/* DCMEN2 */
			writel(readl(u4rg_38) & 0xFFFFFFFD, u4rg_38);
			/* DMPHYCLKDYNGEN */
			writel(readl(u4rg_38) & 0xBFFFFFFF, u4rg_38);
			/* DMMIOCKCTRLOFF */
			writel(readl(u4rg_38) | 0x04000000, u4rg_38);
			/* DMCKEFIXON */
			writel(readl(u4rg_24) | 0x40, u4rg_24);
			/* DMCKE1FIXON */
			writel(readl(u4rg_24) | 0x10, u4rg_24);

			if (RankCounter == 0) {
				/* Rank 0 */
				writel(readl(u4rg_5C) & 0xFCFFFFFF, u4rg_5C);
			} else if (RankCounter == 1) {
				/* Rank 1 */
				writel((readl(u4rg_5C) & 0xFCFFFFFF) |
						0x01000000,
				u4rg_5C);
			}
			/* for ZQCal Start */
			writel(readl(u4rg_60) | 0x10, u4rg_60);

			do {
				Response = readl(u4rg_88) & 0x10;
				TimeCnt--;
				udelay(1);
				/* Wait tZQCAL(min) 1us for next polling */
			} while ((Response == 0) && (TimeCnt > 0));

			/* ZQCal Stop */
			writel(readl(u4rg_60) & 0xFFFFFFEF, u4rg_60);

			if (TimeCnt == 0) { /* time out */
				/* DMCKE1FIXON */
				writel(readl(u4rg_24) & 0xFFFFFFEF, u4rg_24);
				/* DMCKEFIXON */
				writel(readl(u4rg_24) & 0xFFFFFFBF, u4rg_24);
				/* DMMIOCKCTRLOFF */
				writel(readl(u4rg_38) & 0xFBFFFFFF, u4rg_38);
				/* DMPHYCLKDYNGEN */
				writel(readl(u4rg_38) | 0x40000000, u4rg_38);
				/* DCMEN2 */
				writel(readl(u4rg_38) | 0x00000002, u4rg_38);
				release_dram_ctrl();

				mod_timer(&zqcs_timer, jiffies +
						msecs_to_jiffies(280));
				local_irq_restore(save_flags);
				spin_unlock_irqrestore(&sw_zq_tx_lock,
						spinlock_save_flags);
				dramc_info("CA%x Rank%x ZQCal Start time out\n",
						CHCounter, RankCounter);
				return;
			}

			udelay(1);

			/* for ZQCal latch */
			TimeCnt = 100;
			writel(readl(u4rg_60) | 0x40, u4rg_60);

			do {
				Response = readl(u4rg_88) & 0x40;
				TimeCnt--;
				udelay(1);
				/* Wait tZQCAL(min) 1us for next polling */
			} while ((Response == 0) && (TimeCnt > 0));

			/* ZQ latch Stop*/
			writel(readl(u4rg_60) & 0xFFFFFFBF, u4rg_60);

			/* DMCKE1FIXON */
			writel(readl(u4rg_24) & 0xFFFFFFEF, u4rg_24);

			/* DMCKEFIXON */
			writel(readl(u4rg_24) & 0xFFFFFFBF, u4rg_24);

			/* DMMIOCKCTRLOFF */
			writel(readl(u4rg_38) & 0xFBFFFFFF, u4rg_38);

			/* DMPHYCLKDYNGEN */
			writel(readl(u4rg_38) | 0x40000000, u4rg_38);

			/* DCMEN2 */
			writel(readl(u4rg_38) | 0x00000002, u4rg_38);

			if (TimeCnt == 0) { /* time out */
				release_dram_ctrl();

				mod_timer(&zqcs_timer, jiffies +
						msecs_to_jiffies(280));
				local_irq_restore(save_flags);
				spin_unlock_irqrestore(&sw_zq_tx_lock,
						spinlock_save_flags);
				dramc_info("CA%x Rank%x ZQCal latch time out\n",
						CHCounter, RankCounter);
				return;
			}
			udelay(1);
		}
	}
	release_dram_ctrl();

tx_start:
	local_irq_restore(save_flags);
	spin_unlock_irqrestore(&sw_zq_tx_lock, spinlock_save_flags);
#endif

#ifdef SW_TX_TRACKING
	res[0] = TX_DONE;
	res[1] = TX_DONE;

	if (!dram_sw_tx)
		goto tx_end;

	udelay(200);

	spin_lock_irqsave(&sw_zq_tx_lock, spinlock_save_flags);
	local_irq_save(save_flags);
	if (acquire_dram_ctrl() != 0) {
		local_irq_restore(save_flags);
		spin_unlock_irqrestore(&sw_zq_tx_lock, spinlock_save_flags);
		dramc_info("TX 0 can NOT get SPM HW SEMAPHORE!\n");
	} else {
#ifdef DVFS_READY
		writel(readl(PDEF_SYS_TIMER), PDEF_SPM_TX_TIMESTAMP);
#endif
		res[0] = dramc_tx_tracking(0);
		if (release_dram_ctrl() != 0)
			dramc_info("TX 0 release SPM HW SEMAPHORE fail!\n");
		local_irq_restore(save_flags);
		spin_unlock_irqrestore(&sw_zq_tx_lock, spinlock_save_flags);
	}

	udelay(200);

	spin_lock_irqsave(&sw_zq_tx_lock, spinlock_save_flags);
	local_irq_save(save_flags);
	if (acquire_dram_ctrl() != 0) {
		local_irq_restore(save_flags);
		spin_unlock_irqrestore(&sw_zq_tx_lock, spinlock_save_flags);
		dramc_info("TX 1 can NOT get SPM HW SEMAPHORE!\n");
	} else {
#ifdef DVFS_READY
		writel(readl(PDEF_SYS_TIMER), PDEF_SPM_TX_TIMESTAMP);
#endif
		res[1] = dramc_tx_tracking(1);
		if (release_dram_ctrl() != 0)
			dramc_info("TX 1 release SPM HW SEMAPHORE fail!\n");
		local_irq_restore(save_flags);
		spin_unlock_irqrestore(&sw_zq_tx_lock, spinlock_save_flags);
	}
tx_end:
#endif

#if defined(SW_ZQCS) || defined(SW_TX_TRACKING)
#ifdef DVFS_READY
	spm_request_dvfs_opp(0, OPP_3);
#endif
	mod_timer(&zqcs_timer, jiffies + msecs_to_jiffies(280));
#endif

#ifdef SW_TX_TRACKING
	if (res[0] != TX_DONE)
		dump_tx_log(res[0]);
	if (res[1] != TX_DONE)
		dump_tx_log(res[1]);
#endif
}

void del_zqcs_timer(void)
{
	if (dram_sw_tx || dram_sw_zq)
		del_timer_sync(&zqcs_timer);
}

void add_zqcs_timer(void)
{
	/* add_timer(&zqcs_timer); */
	if (dram_sw_tx || dram_sw_zq)
		mod_timer(&zqcs_timer, jiffies + msecs_to_jiffies(280));
}

static int dram_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int i;
	struct resource *res;
	void __iomem *base_temp[8];
	struct device_node *node = NULL;

	pr_debug("[DRAMC] module probe.\n");

	for (i = 0; i < (sizeof(base_temp) / sizeof(*base_temp)); i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		base_temp[i] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(base_temp[i])) {
			dramc_info("unable to map %d base\n", i);
			return -EINVAL;
		}
	}

	DRAMC_AO_CHA_BASE_ADDR = base_temp[0];
	DRAMC_AO_CHB_BASE_ADDR = base_temp[1];

	DRAMC_NAO_CHA_BASE_ADDR = base_temp[2];
	DRAMC_NAO_CHB_BASE_ADDR = base_temp[3];

	DDRPHY_AO_CHA_BASE_ADDR = base_temp[4];
	DDRPHY_AO_CHB_BASE_ADDR = base_temp[5];

	DDRPHY_NAO_CHA_BASE_ADDR = base_temp[6];
	DDRPHY_NAO_CHB_BASE_ADDR = base_temp[7];

	dramc_info("get DRAMC_AO_CHA_BASE_ADDR @ %p\n",
			DRAMC_AO_CHA_BASE_ADDR);
	dramc_info("get DRAMC_AO_CHB_BASE_ADDR @ %p\n",
			DRAMC_AO_CHB_BASE_ADDR);

	dramc_info("get DDRPHY_AO_CHA_BASE_ADDR @ %p\n",
			DDRPHY_AO_CHA_BASE_ADDR);
	dramc_info("get DDRPHY_AO_CHB_BASE_ADDR @ %p\n",
			DDRPHY_AO_CHB_BASE_ADDR);

	dramc_info("get DRAMC_NAO_CHA_BASE_ADDR @ %p\n",
			DRAMC_NAO_CHA_BASE_ADDR);
	dramc_info("get DRAMC_NAO_CHB_BASE_ADDR @ %p\n",
			DRAMC_NAO_CHB_BASE_ADDR);

	dramc_info("get DDRPHY_NAO_CHA_BASE_ADDR @ %p\n",
			DDRPHY_NAO_CHA_BASE_ADDR);
	dramc_info("get DDRPHY_NAO_CHB_BASE_ADDR @ %p\n",
			DDRPHY_NAO_CHB_BASE_ADDR);

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (node) {
		SLEEP_BASE_ADDR = of_iomap(node, 0);
		dramc_info("get SLEEP_BASE_ADDR @ %p\n",
				SLEEP_BASE_ADDR);
	} else {
		dramc_info("can't find SLEEP_BASE_ADDR compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,sys_timer");
	if (node) {
		SYS_TIMER_BASE_ADDR = of_iomap(node, 0);
		dramc_info("get SYS_TIMER_BASE_ADDR @ %p\n",
		SYS_TIMER_BASE_ADDR);
	} else {
		dramc_info("can't find SYS_TIMER_BASE_ADDR compatible node\n");
		return -1;
	}

#ifdef EMI_READY
	DRAM_TYPE = get_dram_type();
#else
	DRAM_TYPE = TYPE_LPDDR3;
#endif
	dramc_info("dram type =%d\n", DRAM_TYPE);

	if (!DRAM_TYPE) {
		dramc_info("dram type error !!\n");
		return -1;
	}

#ifdef EMI_READY
	CH_NUM = get_ch_num();
	dramc_info("Channel num =%d\n", CH_NUM);

	if (!CH_NUM) {
		dramc_info("channel number error !!\n");
		return -1;
	}
#else
	CH_NUM = 2;
#endif

	CBT_MODE = (readl(PDEF_DRAMC0_CHA_REG_01C) & 0x6000) >> 13;
	dramc_info("cbt mode =%d\n", CBT_MODE);
	switch (CBT_MODE) {
	case NORMAL_MODE:
		cbt_mode_rank[0] = RANK_NORMAL;
		cbt_mode_rank[1] = RANK_NORMAL;
		break;
	case BYTE_MODE:
		cbt_mode_rank[0] = RANK_BYTE;
		cbt_mode_rank[1] = RANK_BYTE;
		break;
	case R0_NORMAL_R1_BYTE:
		cbt_mode_rank[0] = RANK_NORMAL;
		cbt_mode_rank[1] = RANK_BYTE;
		break;
	case R0_BYTE_R1_NORMAL:
		cbt_mode_rank[0] = RANK_BYTE;
		cbt_mode_rank[1] = RANK_NORMAL;
		break;
	default:
		dramc_info("CBT mode error!!!\n");
		break;
	}

	dram_mr_mode = (readl(PDEF_DRAMC0_CHA_REG_028) >> 29) & 0x1;

	dramc_info("Dram Data Rate = %d\n", get_dram_data_rate());
	dramc_info("shuffle_status = %d\n", get_shuffle_status());
	dramc_info("MR mode = %d\n", dram_mr_mode);

#ifdef SW_TX_TRACKING
	dram_sw_tx = (readl(PDEF_DRAMC0_CHA_REG_0C8) >> 24) & 0x1;
	dramc_info("SW_TX = %d\n", dram_sw_tx);
#endif
#ifdef SW_ZQCS
	dram_sw_zq = 0x1 - ((readl(PDEF_DRAMC0_CHA_REG_22C) >> 20) & 0x1);
	dramc_info("SW_ZQ = %d\n", dram_sw_zq);
#endif

	if (((DRAM_TYPE == TYPE_LPDDR4) || (DRAM_TYPE == TYPE_LPDDR4X)) &&
			(dram_sw_tx || dram_sw_zq)) {
		low_freq_counter = 10;
		init_timer_deferrable(&zqcs_timer);
		zqcs_timer.function = zqcs_timer_callback;
		zqcs_timer.data = 0;
		if (mod_timer(&zqcs_timer, jiffies + msecs_to_jiffies(280)))
			dramc_info("Error in ZQCS mod_timer\n");
	}

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_emi_clk_mem_test);
	if (ret) {
		dramc_info("fail to create emi_clk_mem_test sysfs files\n");
		return ret;
	}

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_read_dram_data_rate);
	if (ret) {
		dramc_info("fail to create read_dram_data_rate sysfs files\n");
		return ret;
	}

	if (dram_can_support_fh())
		dramc_info("dram can support DFS\n");
	else
		dramc_info("dram can not support DFS\n");

	return 0;
}

static int dram_remove(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dram_of_ids[] = {
	{.compatible = "mediatek,dramc",},
	{}
};
#endif

static struct platform_driver dram_test_drv = {
	.probe = dram_probe,
	.remove = dram_remove,
	.driver = {
		.name = "emi_clk_test",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = dram_of_ids,
#endif
		},
};

/* int __init dram_test_init(void) */
static int __init dram_test_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&dram_test_drv);
	if (ret) {
		dramc_info("init fail, ret 0x%x\n", ret);
		return ret;
	}

	if (of_scan_flat_dt(dt_scan_dram_info, NULL) > 0) {
		dramc_info("find dt_scan_dram_info\n");
	} else {
		dramc_info("can't find dt_scan_dram_info\n");
		return -1;
	}

	return ret;
}

static void __exit dram_test_exit(void)
{
	platform_driver_unregister(&dram_test_drv);
}

postcore_initcall(dram_test_init);
module_exit(dram_test_exit);

void *mt_dramc_chn_base_get(int channel)
{
	switch (channel) {
	case 0:
		return DRAMC_AO_CHA_BASE_ADDR;
	case 1:
		return DRAMC_AO_CHB_BASE_ADDR;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(mt_dramc_chn_base_get);

void *mt_dramc_nao_chn_base_get(int channel)
{
	switch (channel) {
	case 0:
		return DRAMC_NAO_CHA_BASE_ADDR;
	case 1:
		return DRAMC_NAO_CHB_BASE_ADDR;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(mt_dramc_nao_chn_base_get);

void *mt_ddrphy_chn_base_get(int channel)
{
	switch (channel) {
	case 0:
		return DDRPHY_AO_CHA_BASE_ADDR;
	case 1:
		return DDRPHY_AO_CHB_BASE_ADDR;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(mt_ddrphy_chn_base_get);

void *mt_ddrphy_nao_chn_base_get(int channel)
{
	switch (channel) {
	case 0:
		return DDRPHY_NAO_CHA_BASE_ADDR;
	case 1:
		return DDRPHY_NAO_CHB_BASE_ADDR;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(mt_ddrphy_nao_chn_base_get);

unsigned int mt_dramc_chn_get(unsigned int emi_cona)
{
	switch ((emi_cona >> 8) & 0x3) {
	case 0:
		return 1;
	case 1:
		return 2;
	default:
		dramc_info("invalid channel num (emi_cona = 0x%x)\n",
				emi_cona);
	}
	return 0;
}

unsigned int mt_dramc_chp_get(unsigned int emi_cona)
{
	unsigned int chp;

	chp = (emi_cona >> 2) & 0x3;

	return chp + 7;
}

phys_addr_t mt_dramc_rankbase_get(unsigned int rank)
{
	if (!get_dram_info)
		return 0;

	if (rank >= get_dram_info->rank_num)
		return 0;

	return get_dram_info->rank_info[rank].start;
}

unsigned int mt_dramc_ta_support_ranks(void)
{
	return dram_rank_num;
}

MODULE_DESCRIPTION("MediaTek DRAMC Driver v0.1");
