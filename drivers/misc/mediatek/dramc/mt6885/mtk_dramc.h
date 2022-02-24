/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __DRAMC_H__
#define __DRAMC_H__

/* Feature options */
/* #define LAST_DRAMC */
#define SW_ZQCS
#define SW_TX_TRACKING
#define DVFS_READY
#define EMI_READY
#define PLAT_DBG_INFO_MANAGE

#if defined(CONFIG_MTK_ENG_BUILD)
#define DRAMC_MEMTEST_DEBUG_SUPPORT
#endif

/* Registers define */
#define PDEF_DRAMC0_CHA_REG_0E4	IOMEM((DRAMC_AO_CHA_BASE_ADDR + 0x00e4))
#define PDEF_DRAMC0_CHA_REG_010	IOMEM((DRAMC_AO_CHA_BASE_ADDR + 0x0010))
#define PDEF_SPM_AP_SEMAPHORE	IOMEM((SLEEP_BASE_ADDR + 0x428))
#ifdef DVFS_READY
#define PDEF_SPM_TX_TIMESTAMP	IOMEM((SLEEP_BASE_ADDR + 0x618))
#define PDEF_SYS_TIMER	IOMEM((SYS_TIMER_BASE_ADDR + 0x8))
#endif
/* Define */
#define DUAL_FREQ_HIGH		900
#define DUAL_FREQ_LOW		650
#define DATA_RATE_THRESHOLD	15
#define MPLL_CON0_OFFSET	0x280
#define MPLL_CON1_OFFSET	0x284
#define MEMPLL5_OFFSET		0x614
#define DRAMC_ACTIM1		(0x1e8)
#define TB_DRAM_SPEED
#define DUAL_FREQ_DIFF_RLWL	/* If defined, need to set MR2 in dramcinit.*/
#define DMA_GDMA_LEN_MAX_MASK	(0x000FFFFF)
#define DMA_GSEC_EN_BIT		(0x00000001)
#define DMA_INT_EN_BIT		(0x00000001)
#define DMA_INT_FLAG_CLR_BIT	(0x00000000)
#define LPDDR3_MODE_REG_2_LOW	0x00140002              /*RL6 WL3.*/
#define LPDDR2_MODE_REG_2_LOW	0x00040002              /*RL6 WL3.*/

#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5

#define DRAMC_AO_RKCFG		(dramc_ao_chx_base+0x034)
#define DRAMC_AO_PD_CTRL	(dramc_ao_chx_base+0x038)
#define DRAMC_AO_MRS		(dramc_ao_chx_base+0x05C)
#define DRAMC_AO_SPCMD		(dramc_ao_chx_base+0x060)
#define DRAMC_AO_SPCMDCTRL	(dramc_ao_chx_base+0x064)
#define DRAMC_AO_DQSOSCR	(dramc_ao_chx_base+0x0C8)
#define DRAMC_AO_SHUSTATUS	(dramc_ao_chx_base+0x0E4)
#define DRAMC_AO_DQSOSCTHRD	(dramc_ao_chx_base+0x854)
#define DRAMC_AO_CKECTRL	(dramc_ao_chx_base+0x024)
#define DRAMC_AO_DQSOSC_PRD	(dramc_ao_chx_base+0x868)
#define DRAMC_AO_SHU1RK0_PI	(dramc_ao_chx_base+0xA0C)
#define DRAMC_AO_SHU1RK0_DQSOSC	(dramc_ao_chx_base+0xA10)
#define DRAMC_AO_SHU1RK1_PI     (dramc_ao_chx_base+0xB0C)
#define DRAMC_AO_SHU1RK1_DQSOSC (dramc_ao_chx_base+0xB10)
#define DRAMC_NAO_MISC_STATUSA	(dramc_nao_chx_base+0x80)
#define DRAMC_NAO_SPCMDRESP	(dramc_nao_chx_base+0x88)
#define DRAMC_NAO_MRR_STATUS	(dramc_nao_chx_base+0x8C)
#define DDRPHY_SHU1_R0_B0_DQ7	(ddrphy_chx_base+0xE1C)
#define DDRPHY_SHU1_R0_B1_DQ7	(ddrphy_chx_base+0xE6C)
#define DDRPHY_SHU1_R1_B0_DQ7	(ddrphy_chx_base+0xF1C)
#define DDRPHY_SHU1_R1_B1_DQ7	(ddrphy_chx_base+0xF6C)

enum TX_RESULT {
	TX_DONE = 0,
	TX_TIMEOUT_MRR_ENABLE,
	TX_TIMEOUT_MRR_DISABLE,
	TX_TIMEOUT_DQSOSC,
	TX_TIMEOUT_DDRPHY,
	TX_FAIL_DATA_RATE,
	TX_FAIL_VARIATION
};

extern void __iomem *mt_emi_base_get(void);
unsigned int mt_dramc_chn_get(unsigned int emi_cona);
unsigned int mt_dramc_chp_get(unsigned int emi_cona);
phys_addr_t mt_dramc_rankbase_get(unsigned int rank);
unsigned int mt_dramc_ta_support_ranks(void);

#ifdef LAST_DRAMC
#define LAST_DRAMC_SRAM_MGR
#define LAST_DRAMC_IP_BASED

#define LASTDRAMC_KEY 0xD8A3
#define DBG_INFO_TYPE_MAX	3

#define DRAMC_STORAGE_API_ERR_OFFSET	(28)

#define STORAGE_READ_API_MASK		(0xf)
#define ERR_PL_UPDATED			(0x4)

phys_addr_t mt_dramc_ta_reserve_addr(unsigned int rank);
#endif

#ifdef DRAMC_MEMTEST_DEBUG_SUPPORT
unsigned int read_dram_mode_reg_by_rank(
		unsigned int mr_index, unsigned int *mr_value,
		unsigned int rank, unsigned int channel);
#endif
/* Sysfs config */
/*We use GPT to measurement how many clk pass in 100us*/

#ifdef CONFIG_MTK_MEMORY_LOWPOWER
extern int __init acquire_buffer_from_memory_lowpower(phys_addr_t *addr);
#else
static inline int acquire_buffer_from_memory_lowpower(phys_addr_t *addr)
{
	return -3;
}
#endif

/* DRAMC API config */
void *mt_dramc_chn_base_get(int channel);
void *mt_dramc_nao_chn_base_get(int channel);
void *mt_ddrphy_chn_base_get(int channel);
void *mt_ddrphy_nao_chn_base_get(int channel);

/*void get_mempll_table_info(u32 *high_addr, u32 *low_addr, u32 *num);*/
unsigned int get_dram_data_rate(void);
unsigned int read_dram_temperature(unsigned char channel);
/*void sync_hw_gating_value(void);*/
/*unsigned int is_one_pll_mode(void);*/
int dram_steps_freq(unsigned int step);
unsigned int get_shuffle_status(void);
int get_ddr_type(void);
int get_emi_ch_num(void);
int dram_can_support_fh(void);
unsigned int ucDram_Register_Read(unsigned int u4reg_addr);
unsigned int lpDram_Register_Read(unsigned int Reg_base, unsigned int Offset);
int enter_pasr_dpd_config(unsigned char segment_rank0,
unsigned char segment_rank1);
int exit_pasr_dpd_config(void);
void del_zqcs_timer(void);
void add_zqcs_timer(void);

enum DDRTYPE {
	TYPE_LPDDR3 = 1,
	TYPE_LPDDR4,
	TYPE_LPDDR4X
};

enum DRAM_MODE {
	NORMAL_MODE = 0,
	BYTE_MODE,
	R0_NORMAL_R1_BYTE,
	R0_BYTE_R1_NORMAL
};

#define PDEF_DRAMC0_CHA_REG_01C	IOMEM((DRAMC_AO_CHA_BASE_ADDR + 0x001c))
enum RANK_MODE {
	RANK_NORMAL = 0,
	RANK_BYTE
};

enum {
	DRAM_OK = 0,
	DRAM_FAIL
}; /* DRAM status type */

enum {
	DRAMC_NAO_CHA = 0,
	DRAMC_NAO_CHB,
	DRAMC_AO_CHA,
	DRAMC_AO_CHB,
	PHY_NAO_CHA,
	PHY_NAO_CHB,
	PHY_AO_CHA,
	PHY_AO_CHB
}; /* RegBase */

enum RANKNUM {
	SINGLE_RANK = 1,
	DUAL_RANK,
};
enum {
	CHANNEL_A = 0,
	CHANNEL_B,
	CHANNEL_MAX
}; /* DRAM_CHANNEL_T */

#endif   /*__WDT_HW_H__*/
