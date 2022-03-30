/* SPDX-License-Identifier: GPL-2.0 */
/*
 * eem-dbg-v1.h - eem data structure
 *
 * Copyright (c) 2020 MediaTek Inc.
 * chienwei Chang <chienwei.chang@mediatek.com>
 */

#define EEM_TAG	 "[CPU][EEM]"

extern void __iomem *eem_csram_base;



#define EEM_LOG_BASE		0x00112400
#define EEM_LOG_SIZE		0x1800 /* 6k size 0x1800 */

#define NR_FREQ 32
#define NR_PI_VF 9

enum eemsn_det_id {
	EEMSN_DET_L = 0,
	EEMSN_DET_BL, /* for BL or B */
	EEMSN_DET_B, /* for B or DSU */
	EEMSN_DET_CCI,

	NR_EEMSN_DET,
};

/* Parameter Enum */
enum {
	IPI_EEMSN_SHARERAM_INIT,
	IPI_EEMSN_INIT,
	IPI_EEMSN_PROBE,
	IPI_EEMSN_INIT01,
	IPI_EEMSN_GET_EEM_VOLT,
	IPI_EEMSN_INIT02,
	IPI_EEMSN_DEBUG_PROC_WRITE,
	IPI_EEMSN_SEND_UPOWER_TBL_REF,

	IPI_EEMSN_CUR_VOLT_PROC_SHOW,

	IPI_EEMSN_DUMP_PROC_SHOW,
	IPI_EEMSN_AGING_DUMP_PROC_SHOW,

	IPI_EEMSN_OFFSET_PROC_WRITE,
	IPI_EEMSN_SETCLAMP_PROC_WRITE,
	IPI_EEMSN_SNAGING_PROC_WRITE,


	IPI_EEMSN_LOGEN_PROC_SHOW,
	IPI_EEMSN_LOGEN_PROC_WRITE,

	IPI_EEMSN_EN_PROC_SHOW,
	IPI_EEMSN_EN_PROC_WRITE,
	IPI_EEMSN_SNEN_PROC_SHOW,
	IPI_EEMSN_SNEN_PROC_WRITE,

	IPI_EEMSN_FAKE_SN_INIT_ISR,
	IPI_EEMSN_FORCE_SN_SENSING,
	IPI_EEMSN_PULL_DATA,
	IPI_EEMSN_FAKE_SN_SENSING_ISR,
	NR_EEMSN_IPI,
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
	unsigned short freq_tbl[NR_FREQ];
	unsigned char volt_tbl_pmic[NR_FREQ];
	unsigned char volt_tbl_orig[NR_FREQ];
	unsigned char volt_tbl_init2[NR_FREQ];
	/* unsigned char volt_tbl[NR_FREQ]; */
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
