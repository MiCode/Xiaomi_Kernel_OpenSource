
/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef _MTK_EEM_INTERNAL_AP_H_
#define _MTK_EEM_INTERNAL_AP_H_

struct eemsn_det;


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
	IPI_EEMSN_SNAGING_PROC_WRITE,

	IPI_EEMSN_LOGEN_PROC_SHOW,
	IPI_EEMSN_LOGEN_PROC_WRITE,

	IPI_EEMSN_EN_PROC_SHOW,
	IPI_EEMSN_EN_PROC_WRITE,
	IPI_EEMSN_SNEN_PROC_SHOW,
	IPI_EEMSN_SNEN_PROC_WRITE,
	// IPI_EEMSN_VCORE_GET_VOLT,
	// IPI_EEMSN_GPU_DVFS_GET_STATUS,
	IPI_EEMSN_FAKE_SN_INIT_ISR,
	IPI_EEMSN_FORCE_SN_SENSING,
	IPI_EEMSN_PULL_DATA,
	IPI_EEMSN_FAKE_SN_SENSING_ISR,
	NR_EEMSN_IPI,
};

struct eem_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[3];
		} data;
	} u;
};

struct eemsn_det_ops {

#if 0
	/* interface to EEM */
	void (*enable)(struct eemsn_det *det, int reason);
	void (*disable)(struct eemsn_det *det, int reason);
	void (*disable_locked)(struct eemsn_det *det, int reason);
	void (*switch_bank)(struct eemsn_det *det, enum eem_phase phase);

	int (*init01)(struct eemsn_det *det);
	int (*init02)(struct eemsn_det *det);
	int (*mon_mode)(struct eemsn_det *det);


	void (*set_phase)(struct eemsn_det *det, enum eem_phase phase);
	int (*get_status)(struct eemsn_det *det);
	void (*dump_status)(struct eemsn_det *det);

	/* interface to thermal */

	/* interface to DVFS */
	int (*set_volt)(struct eemsn_det *det);
	void (*restore_default_volt)(struct eemsn_det *det);
#endif
	int (*get_volt)(struct eemsn_det *det);
	int (*get_temp)(struct eemsn_det *det);
	void (*get_freq_table)(struct eemsn_det *det);
	void (*get_orig_volt_table)(struct eemsn_det *det);

	/* interface to PMIC */
	int (*volt_2_pmic)(struct eemsn_det *det, int volt);
	int (*volt_2_eem)(struct eemsn_det *det, int volt);
	int (*pmic_2_volt)(struct eemsn_det *det, int pmic_val);
	int (*eem_2_pmic)(struct eemsn_det *det, int eev_val);
};

struct eemsn_devinfo {
	/* M_HW_RES0 0x11c1_0580 */
	unsigned int FT_PGM:8;
	unsigned int FT_BIN:4;
	unsigned int RSV0_1:20;

	/* M_HW_RES1 */
	unsigned int M_HW_RES1;

	/* M_HW_RES2 */
	unsigned int M_HW_RES2;

	/* M_HW_RES3 */
	unsigned int M_HW_RES3;

	/* M_HW_RES4 */
	unsigned int M_HW_RES4;

	/* M_HW_RES5 */
	unsigned int M_HW_RES5;

	/* M_HW_RES6 */
	unsigned int M_HW_RES6;

	/* M_HW_RES7 */
	unsigned int M_HW_RES7;

	/* M_HW_RES8 */
	unsigned int M_HW_RES8;

	/* M_HW_RES9 */
	unsigned int M_HW_RES9;

	/* M_HW_RES10 (0x11F105A8)*/
	unsigned int M_HW_RES10;

	/* M_HW_RES11 (0x11F105AC)*/
	unsigned int M_HW_RES11;

	/* M_HW_RES12 (0x11F105B0)*/
	unsigned int M_HW_RES12;

	/* M_HW_RES13 (0x11F105B4)*/
	unsigned int M_HW_RES13;

	/* M_HW_RES14 SN (0x11F10950)*/
	unsigned int BCPU_A_T0_SVT:16;
	unsigned int BCPU_A_T0_LVT:16;

	/* M_HW_RES15 SN */
	unsigned int BCPU_A_T0_ULVT:16;
	unsigned int LCPU_A_T0_SVT:16;

	/* M_HW_RES16 */
	unsigned int LCPU_A_T0_LVT:16;
	unsigned int LCPU_A_T0_ULVT:16;

	/* M_HW_RES17 */
	unsigned int T_SVT_HV_BCPU_RT:8;
	unsigned int T_SVT_LV_BCPU_RT:8;
	unsigned int T_SVT_HV_LCPU_RT:8;
	unsigned int T_SVT_LV_LCPU_RT:8;

	/* M_HW_RES18 */
	unsigned int T_SVT_HV_BCPU_HT:8;
	unsigned int T_SVT_LV_BCPU_HT:8;
	unsigned int T_SVT_HV_LCPU_HT:8;
	unsigned int T_SVT_LV_LCPU_HT:8;

	/* M_HW_RES19 */
	unsigned int SN_VERSION:4;
	unsigned int SN_PATTERN:4;
	unsigned int ATE_TEMP:8;
	unsigned int RSV4:16;

	/* M_HW_RES20 */
	unsigned int FPC_recovery_BCPU_2P6G:8;
	unsigned int Final_Vmin_BCPU_2P6G:8;
	unsigned int RSV5:16;

	/* M_HW_RES21 */
	unsigned int Vsn_BCPU_2P6G:8;
	unsigned int RSV6:24;
};


struct eemsn_det {
	int temp; /* det temperature */

	/* dvfs */
	unsigned int cur_volt;

	struct eemsn_det_ops *ops;
	enum eemsn_det_id det_id;

	unsigned int volt_tbl_pmic[NR_FREQ]; /* pmic value */

	/* for PMIC */
	unsigned short eemsn_v_base;
	unsigned short eemsn_step;
	unsigned short pmic_base;
	unsigned short pmic_step;

	/* dvfs */
	unsigned short freq_tbl[NR_FREQ];
	//unsigned char volt_tbl[NR_FREQ]; /* eem value */
	unsigned char volt_tbl_init2[NR_FREQ]; /* eem value */
	unsigned char volt_tbl_orig[NR_FREQ]; /* pmic value */
	unsigned char dst_volt_pmic;
	unsigned char volt_tbl0_min; /* pmic value */

	unsigned char features; /* enum eemsn_features */
	const char *name;

	unsigned char disabled; /* Disabled by error or sysfs */
	unsigned char num_freq_tbl;

	//unsigned char turn_pt;
	//unsigned char vmin_high;
	//unsigned char vmin_mid;
	int8_t volt_offset;
	int8_t volt_clamp;

#if UPDATE_TO_UPOWER
	/* only when init2, eemsn need to set volt to upower */
	unsigned int set_volt_to_upower:1;
#endif
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
	unsigned char turn_pt;
	enum eemsn_det_id det_id;
};

struct sensing_stru {
#if VMIN_PREDICT_ENABLE
	uint64_t CPE_Vmin_HW;
#endif
	unsigned int SN_Vmin;
	int CPE_Vmin[2];
	unsigned int cur_volt;
	int max_temp;
	int min_temp;
#if !VMIN_PREDICT_ENABLE
	/* unsigned int count_cur_volt_HT; */
	//int Sensor_Volt_HT;
	//int Sensor_Volt_RT;
	int8_t CPE_temp_RT[2];
	int8_t CPE_temp_HT[2];
	int8_t final_CPE_temp[2];
	int8_t SN_temp[2];
	int8_t cur_sn_aging[2];
	unsigned char T_SVT_current;
#endif
	unsigned char cur_oppidx;
	unsigned char dst_volt_pmic;
};


struct sn_log_data {
	unsigned long long timestamp;
	unsigned int reg_dump_cpe[MIN_SIZE_SN_DUMP_CPE];
	unsigned int reg_dump_sndata[SIZE_SN_DUMP_SENSOR];
	unsigned int reg_dump_sn_cpu[NUM_SN_CPU][SIZE_SN_MCUSYS_REG];
	struct sensing_stru sd[NR_SN_DET];
	unsigned int footprint[NR_SN_DET];
	unsigned int allfp;
#if VMIN_PREDICT_ENABLE
	unsigned int sn_cpe_vop;
#endif
};

struct A_Tused_VT {
	unsigned int A_Tused_SVT:16;
	unsigned int A_Tused_LVT:16;
	unsigned int A_Tused_ULVT:16;
	unsigned int A_Tused_RSV0:16;
};

struct sn_log_cal_data {
	int64_t cpe_init_aging[NR_PI_VF];
	struct A_Tused_VT atvt;
	int TEMP_CAL;
	int volt_cross;
	int count_cross;
	int CPE_Aging[NR_PI_VF];
	int8_t sn_aging[NR_PI_VF];
	uint8_t T_SVT_HV_HT;
	uint8_t T_SVT_LV_HT;
	uint8_t T_SVT_HV_RT;
	uint8_t T_SVT_LV_RT;
};

struct dvfs_vf_tbl {
	unsigned short pi_freq_tbl[NR_PI_VF];
	unsigned char pi_volt_tbl[NR_PI_VF];
	unsigned char pi_vf_num;
};

struct eemsn_log {
	struct eemsn_log_det det_log[NR_EEMSN_DET];
	struct sn_log_data sn_log;
	struct sn_log_cal_data sn_cal_data[NR_SN_DET];
	struct eemsn_devinfo efuse_devinfo;
	unsigned char init2_v_ready;
	unsigned char init_vboot_done;
	unsigned char eemsn_enable;
	unsigned char sn_enable;
	unsigned char ctrl_aging_Enable;
	unsigned char segCode;
	unsigned char lock;
	struct dvfs_vf_tbl vf_tbl_det[NR_EEMSN_DET];
#if ENABLE_COUNT_SNTEMP
	unsigned int sn_temp_cnt[NR_SN_DET][5];
#endif
};






#if 0
/*********************************************
 *extern variables defined at mtk_eem.c
 *********************************************
 */
extern unsigned int freq[NR_FREQ];

extern struct mutex record_mutex;


extern void mt_record_lock(unsigned long *flags);
extern void mt_record_unlock(unsigned long *flags);
#endif

/**************************************************
 *extern variables and operations defined at mtk_eem_platform.c
 ***************************************************
 */

extern struct eemsn_det_ops big_det_ops;
extern void get_freq_table_cpu(struct eemsn_det *det);
extern void get_orig_volt_table_cpu(struct eemsn_det *det);
extern int get_volt_cpu(struct eemsn_det *det);



/*********************************************
 *extern operations defined at mtk_eem.c
 *********************************************
 */
extern void base_ops_enable(struct eemsn_det *det, int reason);
extern void base_ops_disable(struct eemsn_det *det, int reason);
extern void base_ops_disable_locked(struct eemsn_det *det, int reason);
extern void base_ops_switch_bank(struct eemsn_det *det, enum eem_phase phase);

extern int base_ops_init01(struct eemsn_det *det);
extern int base_ops_init02(struct eemsn_det *det);
extern int base_ops_mon_mode(struct eemsn_det *det);

extern int base_ops_get_status(struct eemsn_det *det);
extern void base_ops_dump_status(struct eemsn_det *det);

extern void base_ops_set_phase(struct eemsn_det *det, enum eem_phase phase);
extern int base_ops_get_temp(struct eemsn_det *det);
extern int base_ops_get_volt(struct eemsn_det *det);
extern int base_ops_set_volt(struct eemsn_det *det);
extern void base_ops_restore_default_volt(struct eemsn_det *det);
extern void base_ops_get_freq_table(struct eemsn_det *det);
extern void base_ops_get_orig_volt_table(struct eemsn_det *det);
#endif

