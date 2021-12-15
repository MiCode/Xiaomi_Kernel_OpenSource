/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __DRAMC_H__
#define __DRAMC_H__

/* Feature options */
#ifdef CONFIG_MTK_EMI_LEGACY
#define EMI_READY
#endif
/* #define RUNTIME_SHMOO */

#if defined(CONFIG_MTK_ENG_BUILD)
#define DRAMC_MEMTEST_DEBUG_SUPPORT
#endif

/* Registers define */
#define PDEF_DRAMC0_CHA_REG_0E4	IOMEM((DRAMC_AO_CHA_BASE_ADDR + 0x00e4))
#define PDEF_SPM_AP_SEMAPHORE	IOMEM((SLEEP_BASE_ADDR + 0x428))

/* Define */
#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5

/* DRAMC API config */
extern void __iomem *mt_emi_base_get(void);
unsigned int mt_dramc_chn_get(unsigned int emi_cona);
unsigned int mt_dramc_chp_get(unsigned int emi_cona);
phys_addr_t mt_dramc_rankbase_get(unsigned int rank);
unsigned int mt_dramc_ta_support_ranks(void);

void *mt_dramc_chn_base_get(int channel);
void *mt_dramc_nao_chn_base_get(int channel);
void *mt_ddrphy_chn_base_get(int channel);
void *mt_ddrphy_nao_chn_base_get(int channel);
unsigned int get_dram_data_rate(void);
int dram_steps_freq(unsigned int step);
unsigned int get_shuffle_status(void);
int get_ddr_type(void);
unsigned char get_ddr_mr(unsigned int index);
int get_emi_ch_num(void);
unsigned int lpDram_Register_Read(unsigned int Reg_base, unsigned int Offset);
int enter_pasr_dpd_config(unsigned char segment_rank0,
	unsigned char segment_rank1);
int exit_pasr_dpd_config(void);

enum DDRTYPE {
	TYPE_LPDDR3 = 1,
	TYPE_LPDDR4,
	TYPE_LPDDR4X
};

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

#endif   /*__WDT_HW_H__*/
