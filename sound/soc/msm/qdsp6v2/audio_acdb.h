/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _AUDIO_ACDB_H
#define _AUDIO_ACDB_H

#include <linux/msm_audio_acdb.h>
#include <sound/q6adm-v2.h>

enum {
	RX_CAL,
	TX_CAL,
	MAX_AUDPROC_TYPES
};

enum {
	VOCPROC_CAL,
	VOCSTRM_CAL,
	VOCVOL_CAL,
	MAX_VOCPROC_TYPES
};

struct acdb_cal_block {
	size_t		cal_size;
	void			*cal_kvaddr;
	phys_addr_t		cal_paddr;
};

struct hw_delay_entry {
	uint32_t sample_rate;
	uint32_t delay_usec;
};

uint32_t get_voice_rx_topology(void);
uint32_t get_voice_tx_topology(void);
uint32_t get_adm_rx_topology(void);
uint32_t get_adm_tx_topology(void);
uint32_t get_asm_topology(void);
void reset_custom_topology_flags(void);
int get_adm_custom_topology(struct acdb_cal_block *cal_block);
int get_asm_custom_topology(struct acdb_cal_block *cal_block);
int get_voice_cal_allocation(struct acdb_cal_block *cal_block);
int get_lsm_cal(struct acdb_cal_block *cal_block);
int get_anc_cal(struct acdb_cal_block *cal_block);
int get_afe_cal(int32_t path, struct acdb_cal_block *cal_block);
int get_audproc_cal(int32_t path, struct acdb_cal_block *cal_block);
int get_audstrm_cal(int32_t path, struct acdb_cal_block *cal_block);
int get_audvol_cal(int32_t path, struct acdb_cal_block *cal_block);
int get_voice_col_data(uint32_t vocproc_type,
	struct acdb_cal_block *cal_block);
int get_vocproc_dev_cfg_cal(struct acdb_cal_block *cal_block);
int get_vocproc_cal(struct acdb_cal_block *cal_block);
int get_vocstrm_cal(struct acdb_cal_block *cal_block);
int get_vocvol_cal(struct acdb_cal_block *cal_block);
int get_sidetone_cal(struct sidetone_cal *cal_data);
int get_spk_protection_cfg(struct msm_spk_prot_cfg *prot_cfg);
int get_aanc_cal(struct acdb_cal_block *cal_block);
int get_hw_delay(int32_t path, struct hw_delay_entry *delay_info);
int get_ulp_lsm_cal(struct acdb_cal_block *cal_block);
int get_ulp_afe_cal(struct acdb_cal_block *cal_block);

#endif
