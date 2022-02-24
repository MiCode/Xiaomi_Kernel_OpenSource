// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
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
	unsigned int CPU_B_MTDES:8;
	unsigned int CPU_B_INITEN:1;
	unsigned int CPU_B_MONEN:1;
	unsigned int CPU_B_DVFS_LOW:3;
	unsigned int CPU_B_SPEC:3;
	unsigned int CPU_B_BDES:8;
	unsigned int CPU_B_MDES:8;

	/* M_HW_RES2 */
	unsigned int CPU_B_HI_MTDES:8;
	unsigned int CPU_B_HI_INITEN:1;
	unsigned int CPU_B_HI_MONEN:1;
	unsigned int CPU_B_HI_DVFS_LOW:3;
	unsigned int CPU_B_HI_SPEC:3;
	unsigned int CPU_B_HI_BDES:8;
	unsigned int CPU_B_HI_MDES:8;

	/* M_HW_RES3 */
	unsigned int CPU_B_LO_MTDES:8;
	unsigned int CPU_B_LO_INITEN:1;
	unsigned int CPU_B_LO_MONEN:1;
	unsigned int CPU_B_LO_DVFS_LOW:3;
	unsigned int CPU_B_LO_SPEC:3;
	unsigned int CPU_B_LO_BDES:8;
	unsigned int CPU_B_LO_MDES:8;

	/* M_HW_RES4 */
	unsigned int CPU_L_MTDES:8;
	unsigned int CPU_L_INITEN:1;
	unsigned int CPU_L_MONEN:1;
	unsigned int CPU_L_DVFS_LOW:3;
	unsigned int CPU_L_SPEC:3;
	unsigned int CPU_L_BDES:8;
	unsigned int CPU_L_MDES:8;

	/* M_HW_RES5 */
	unsigned int CPU_L_HI_MTDES:8;
	unsigned int CPU_L_HI_INITEN:1;
	unsigned int CPU_L_HI__MONEN:1;
	unsigned int CPU_L_HI_DVFS_LOW:3;
	unsigned int CPU_L_HI_SPEC:3;
	unsigned int CPU_L_HI_BDES:8;
	unsigned int CPU_L_HI_MDES:8;

	/* M_HW_RES6 */
	unsigned int CPU_L_LO_MTDES:8;
	unsigned int CPU_L_LO_INITEN:1;
	unsigned int CPU_L_LO_MONEN:1;
	unsigned int CPU_L_LO_DVFS_LOW:3;
	unsigned int CPU_L_LO_SPEC:3;
	unsigned int CPU_L_LO_BDES:8;
	unsigned int CPU_L_LO_MDES:8;

	/* M_HW_RES7 */
	unsigned int CCI_MTDES:8;
	unsigned int CCI_INITEN:1;
	unsigned int CCI_MONEN:1;
	unsigned int CCI_DVFS_LOW:3;
	unsigned int CCI_SPEC:3;
	unsigned int CCI_BDES:8;
	unsigned int CCI_MDES:8;

	/* M_HW_RES8 */
	unsigned int GPU_MTDES:8;
	unsigned int GPU_INITEN:1;
	unsigned int GPU_MONEN:1;
	unsigned int GPU_DVFS_LOW:3;
	unsigned int GPU_SPEC:3;
	unsigned int GPU_BDES:8;
	unsigned int GPU_MDES:8;

	/* M_HW_RES9 */
	unsigned int GPU_HI_MTDES:8;
	unsigned int GPU_HI_INITEN:1;
	unsigned int GPU_HI_MONEN:1;
	unsigned int GPU_HI_DVFS_LOW:3;
	unsigned int GPU_HI_SPEC:3;
	unsigned int GPU_HI_BDES:8;
	unsigned int GPU_HI_MDES:8;

	/* M_HW_RES10 */
	unsigned int GPU_LO_MTDES:8;
	unsigned int GPU_LO_INITEN:1;
	unsigned int GPU_LO_MONEN:1;
	unsigned int GPU_LO_DVFS_LOW:3;
	unsigned int GPU_LO_SPEC:3;
	unsigned int GPU_LO_BDES:8;
	unsigned int GPU_LO_MDES:8;

	/* M_HW_RES11 */
	unsigned int MD_VMODEM:32;

	/* M_HW_RES12 */
	unsigned int MD_VNR:32;

	/* M_HW_RES13 */
	unsigned int CPU_B_HI_DCBDET:8;
	unsigned int CPU_B_HI_DCMDET:8;
	unsigned int CPU_B_DCBDET:8;
	unsigned int CPU_B_DCMDET:8;

	/* M_HW_RES14 */
	unsigned int CPU_L_DCBDET:8;
	unsigned int CPU_L_DCMDET:8;
	unsigned int CPU_B_LO_DCBDET:8;
	unsigned int CPU_B_LO_DCMDET:8;

	/* M_HW_RES15 */
	unsigned int CPU_L_LO_DCBDET:8;
	unsigned int CPU_L_LO_DCMDET:8;
	unsigned int CPU_L_HI_DCBDET:8;
	unsigned int CPU_L_HI_DCMDET:8;

	/* M_HW_RES16 */
	unsigned int GPU_DCBDET:8;
	unsigned int GPU_DCMDET:8;
	unsigned int CCI_DCBDET:8;
	unsigned int CCI_DCMDET:8;


	/* M_HW_RES17 */
	unsigned int GPU_LO_DCBDET:8;
	unsigned int GPU_LO_DCMDET:8;
	unsigned int GPU_HI_DCBDET:8;
	unsigned int GPU_HI_DCMDET:8;

	/* M_HW_RES21 */
	unsigned int BCPU_A_T0_SVT:8;
	unsigned int BCPU_A_T0_LVT:8;
	unsigned int BCPU_A_T0_ULVT:8;
	unsigned int LCPU_A_T0_SVT:8;

	/* M_HW_RES22 */
	unsigned int LCPU_A_T0_LVT:8;
	unsigned int LCPU_A_T0_ULVT:8;
	unsigned int DELTA_VC_BCPU:4;
	unsigned int DELTA_VC_LCPU:4;
	unsigned int DELTA_VC_RT_BCPU:4;
	unsigned int DELTA_VC_RT_LCPU:4;

	/* M_HW_RES23 */
	unsigned int DELTA_VDPPM_BCPU:5;
	unsigned int DELTA_VDPPM_LCPU:5;
	unsigned int ATE_TEMP:3;
	unsigned int SN_PATTERN:3;
	unsigned int A_T0_SVT_BCPU_0P95V:8;
	unsigned int A_T0_SVT_LCPU_0P95V:8;

	/* M_HW_RES24 */
	unsigned int T_SVT_HV_BCPU:8;
	unsigned int T_SVT_LV_BCPU:8;
	unsigned int T_SVT_HV_LCPU:8;
	unsigned int T_SVT_LV_LCPU:8;

	/* M_HW_RES25 */
	unsigned int T_SVT_HV_BCPU_RT:8;
	unsigned int T_SVT_LV_BCPU_RT:8;
	unsigned int T_SVT_HV_LCPU_RT:8;
	unsigned int T_SVT_LV_LCPU_RT:8;
};


struct eemsn_det {
	int64_t		cpe_init_aging;
	int temp; /* det temperature */

	/* dvfs */
	unsigned int max_freq_khz;
	unsigned int mid_freq_khz;
	unsigned int turn_freq;
	unsigned int cur_volt;
	unsigned int *p_sn_cpu_coef;
	struct sn_param *p_sn_cpu_param;


	struct eemsn_det_ops *ops;
	enum eemsn_det_id det_id;

	unsigned int volt_tbl_pmic[NR_FREQ]; /* pmic value */

	/* for PMIC */
	unsigned short eemsn_v_base;
	unsigned short eemsn_step;
	unsigned short pmic_base;
	unsigned short pmic_step;
	short cpe_volt_total_mar;

	/* dvfs */
	unsigned char freq_tbl[NR_FREQ];
	//unsigned char volt_tbl[NR_FREQ]; /* eem value */
	unsigned char volt_tbl_init2[NR_FREQ]; /* eem value */
	unsigned char volt_tbl_orig[NR_FREQ]; /* pmic value */
	unsigned char dst_volt_pmic;
	unsigned char volt_tbl0_min; /* pmic value */

	unsigned char features; /* enum eemsn_features */
	unsigned char cur_phase;
	unsigned char cur_oppidx;

	unsigned char SPEC;

	const char *name;

	unsigned char disabled; /* Disabled by error or sysfs */
	unsigned char num_freq_tbl;

	unsigned char loo_role;
	unsigned char loo_couple;
	unsigned char turn_pt;
	unsigned char vmin_high;
	unsigned char vmin_mid;
	int8_t delta_vc;
	int8_t sn_aging;
	int8_t volt_offset;
	int8_t volt_clamp;
	/* int volt_offset:8; */
	unsigned int delta_ir:4;
	unsigned int delta_vdppm:5;

	unsigned int isSupLoo:1;

#if UPDATE_TO_UPOWER
	/* only when init2, eemsn need to set volt to upower */
	unsigned int set_volt_to_upower:1;
#endif
};

struct eemsn_log_det {
	unsigned int temp;
	unsigned short volt_tbl_pmic[NR_FREQ];
	unsigned char volt_tbl_orig[NR_FREQ];
	unsigned char volt_tbl_init2[NR_FREQ];
	/* unsigned char volt_tbl[NR_FREQ]; */
	unsigned char freq_tbl[NR_FREQ];
	unsigned char num_freq_tbl;
	unsigned char lock;
	unsigned char features;
	unsigned char volt_clamp;
	int8_t volt_offset;
	unsigned char turn_pt;
	enum eemsn_det_id det_id;
};

struct sensing_stru {
#if VMIN_PREDICT_ENABLE
	uint64_t CPE_Vmin_HW;
#endif
	unsigned int SN_Vmin;
	int CPE_Vmin;
	unsigned int cur_volt;
#if !VMIN_PREDICT_ENABLE
	/* unsigned int count_cur_volt_HT; */
	int Sensor_Volt_HT;
	int Sensor_Volt_RT;
	int8_t CPE_temp;
	int8_t SN_temp;
	unsigned char T_SVT_current;
#endif
	unsigned short cur_temp;
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

struct sn_param {
	/* for SN aging*/
	int Param_A_Tused_SVT;
	int Param_A_Tused_LVT;
	int Param_A_Tused_ULVT;
	int Param_A_T0_SVT;
	int Param_A_T0_LVT;
	int Param_A_T0_ULVT;
	int Param_ATE_temp;
	int Param_temp;
	int Param_INTERCEPTION;
	int8_t A_GB;
	int8_t sn_temp_threshold;
	int8_t Default_Aging;
	int8_t threshold_H;
	int8_t threshold_L;

	unsigned char T_GB;

	/* Formula for CPE_Vmin (Vmin prediction) */
	unsigned char CPE_GB;
	unsigned char MSSV_GB;

};

struct A_Tused_VT {
	unsigned int A_Tused_SVT:8;
	unsigned int A_Tused_LVT:8;
	unsigned int A_Tused_ULVT:8;
	unsigned int A_Tused_RSV0:8;
};

struct sn_log_cal_data {
	/* struct sn_param sn_cpu_param; */
	int64_t cpe_init_aging;
	struct A_Tused_VT atvt;
	int TEMP_CAL;
	int volt_cross;
	short CPE_Aging;
	int8_t sn_aging;
	int8_t delta_vc;
	uint8_t T_SVT_HV_RT;
	uint8_t T_SVT_LV_RT;
};

struct sn_ring_buf {
	unsigned int curidx:8;
	unsigned int magicword:24;
	struct sn_log_cal_data sn_cal_data[NR_SN_DET];
	struct sn_log_data sn_dbg[TOTEL_SN_DBG_NUM];
};


/* even if vcore ptp is not enabled, we still need to reserve
 * a mem block for vcore dvfs to store voltages
 */
struct eemsn_log {
#if 0
	unsigned int efuse_workaround;
	unsigned int hw_res[NR_HW_RES];
	unsigned short sn_sndata_reg_dump_off[SIZE_SN_DUMP_SENSOR];
	unsigned int sn_lcpu_coef[SIZE_SN_COEF];
	unsigned int sn_bcpu_coef[SIZE_SN_COEF];
	struct sn_param sn_lcpu_param;
	struct sn_param sn_bcpu_param;
#endif
	struct eemsn_log_det det_log[NR_EEMSN_DET_LOG_ID];
	struct sn_log_data sn_log;
	struct sn_log_cal_data sn_cal_data[NR_SN_DET];
	struct sn_param sn_cpu_param[NR_SN_DET];
	struct eemsn_devinfo efuse_devinfo;
	unsigned int efuse_sv;
	unsigned char init2_v_ready;
	unsigned char eemsn_enable;
	unsigned char sn_enable;
	unsigned char ctrl_aging_Enable;
	unsigned char segCode;
	unsigned char lock;
};






/*********************************************
 *extern variables defined at mtk_eem.c
 *********************************************
 */
extern unsigned int freq[NR_FREQ];

extern struct mutex record_mutex;


extern void mt_record_lock(unsigned long *flags);
extern void mt_record_unlock(unsigned long *flags);


/**************************************************
 *extern variables and operations defined at mtk_eem_platform.c
 ***************************************************
 */

extern struct eemsn_det_ops big_det_ops;
//extern struct eemsn_det_ops cci_det_ops;

extern void get_freq_table_cpu(struct eemsn_det *det);
extern void get_orig_volt_table_cpu(struct eemsn_det *det);
extern int get_volt_cpu(struct eemsn_det *det);
#if 0
extern int set_volt_cpu(struct eemsn_det *det);
extern void restore_default_volt_cpu(struct eemsn_det *det);
extern int get_volt_gpu(struct eemsn_det *det);
extern int set_volt_gpu(struct eemsn_det *det);
extern void restore_default_volt_gpu(struct eemsn_det *det);
extern void get_freq_table_gpu(struct eemsn_det *det);
extern void get_orig_volt_table_gpu(struct eemsn_det *det);
#endif


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

