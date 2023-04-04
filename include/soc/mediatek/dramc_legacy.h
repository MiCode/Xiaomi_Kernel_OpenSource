/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __DRAMC_LEGACY_H__
#define __DRAMC_LEGACY_H__

enum DDRTYPE {
	TYPE_LPDDR3 = 1,
	TYPE_LPDDR4,
	TYPE_LPDDR4X,
	TYPE_LPDDR2
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

enum RANKNUM {
	SINGLE_RANK = 1,
	DUAL_RANK,
};

/* DRAMC API config */
void *mt_dramc_chn_base_get(int channel);
void *mt_dramc_nao_chn_base_get(int channel);
void *mt_ddrphy_chn_base_get(int channel);
void *mt_ddrphy_nao_chn_base_get(int channel);

unsigned int get_dram_data_rate(void);
unsigned int lpDram_Register_Read(unsigned int Reg_base, unsigned int Offset);
int dram_steps_freq(unsigned int step);
int get_ddr_type(void);
int get_emi_ch_num(void);
int dram_can_support_fh(void);

#endif /* !__DRAMC_LEGACY_H__*/
