/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
#include <sound/q6adm.h>

enum {
	RX_CAL,
	TX_CAL,
	MAX_AUDPROC_TYPES
};

struct acdb_cal_block {
	uint32_t		cal_size;
	uint32_t		cal_kvaddr;
	uint32_t		cal_paddr;
};

struct acdb_atomic_cal_block {
	atomic_t		cal_size;
	atomic_t		cal_kvaddr;
	atomic_t		cal_paddr;
};

struct acdb_cal_data {
	uint32_t			num_cal_blocks;
	struct acdb_atomic_cal_block	*cal_blocks;
};

uint32_t get_voice_rx_topology(void);
uint32_t get_voice_tx_topology(void);
uint32_t get_adm_rx_topology(void);
uint32_t get_adm_tx_topology(void);
uint32_t get_asm_topology(void);
void get_all_voice_cal(struct acdb_cal_block *cal_block);
void get_all_cvp_cal(struct acdb_cal_block *cal_block);
void get_all_vocproc_cal(struct acdb_cal_block *cal_block);
void get_all_vocstrm_cal(struct acdb_cal_block *cal_block);
void get_all_vocvol_cal(struct acdb_cal_block *cal_block);
void get_anc_cal(struct acdb_cal_block *cal_block);
void get_afe_cal(int32_t path, struct acdb_cal_block *cal_block);
void get_audproc_cal(int32_t path, struct acdb_cal_block *cal_block);
void get_audstrm_cal(int32_t path, struct acdb_cal_block *cal_block);
void get_audvol_cal(int32_t path, struct acdb_cal_block *cal_block);
void get_vocproc_cal(struct acdb_cal_data *cal_data);
void get_vocstrm_cal(struct acdb_cal_data *cal_data);
void get_vocvol_cal(struct acdb_cal_data *cal_data);
void get_sidetone_cal(struct sidetone_cal *cal_data);

#endif
