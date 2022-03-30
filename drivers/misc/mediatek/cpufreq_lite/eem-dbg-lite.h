/* SPDX-License-Identifier: GPL-2.0 */
/*
 * eem-dbg-lite.c - eem debug driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Tungchen Shih <tungchen.shih@mediatek.com>
 */
#define MAX_NR_FREQ      32 /* Max supported number of frequency */
#define VOLT_STEP    625

enum eemsn_det_id {
	EEMSN_DET_L = 0,
	EEMSN_DET_BL, /* for BL or B */
	EEMSN_DET_B, /* for B or DSU */
	EEMSN_DET_CCI,

	NR_EEMSN_DET,
};

/* Parameter Enum */
enum {
	IPI_EEMSN_GET_EEM_VOLT = 4,
	NR_EEMSN_IPI = 24,
};

struct eemsn_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[3];
		} data;
	} u;
};

struct eemsn_log_det {
	unsigned int temp;
	unsigned short freq_tbl[MAX_NR_FREQ];
	unsigned char volt_tbl_pmic[MAX_NR_FREQ];
	unsigned char volt_tbl_orig[MAX_NR_FREQ];
	unsigned char volt_tbl_init2[MAX_NR_FREQ];
	unsigned char num_freq_tbl;
	unsigned char lock;
	unsigned char features;
	int8_t volt_clamp;
	int8_t volt_offset;
	enum eemsn_det_id det_id;
};

struct eemsn_log {
	unsigned int eemsn_disable:8;
	unsigned int ctrl_aging_Enable:8;
	unsigned int sn_disable:8;
	unsigned int segCode:8;
	unsigned char init2_v_ready;
	unsigned char init_vboot_done;
	unsigned char lock;
	unsigned char eemsn_log_en;
	struct eemsn_log_det det_log[NR_EEMSN_DET];
};
