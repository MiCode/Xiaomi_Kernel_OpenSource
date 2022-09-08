/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_MCDI_MCUPM_H__
#define __MTK_MCDI_MCUPM_H__

enum {
	MCUPM_SMC_CALL = 0x00F,
	MCUPM_FW_PARSE = 0x0F0,
	MCUPM_FW_KICK  = 0x0FF,
};

void mcdi_cluster_counter_set_cpu_residency(int cpu);
void mcdi_mcupm_init(void);
int get_mcupmfw_load_info(void);
void set_mcupmfw_load_info(int value);
bool mcdi_mcupm_sram_is_ready(void);
void mcdi_set_pllbuck_req(int param);
unsigned int mcdi_get_pllbuck_req(void);

extern bool mcupm_sram_is_ready;
/* only use in mcusys off flow */
#define MCUPM_PROT_NO_RESPONSE 0
#define MCUPM_PROT_SUCCESS 0xbabe
#define MCUPM_PROT_FAIL 0xdead


#endif /* __MTK_MCDI_MCUPM_H__ */
