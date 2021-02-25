#ifndef _UAPI_MSM_AUDIO_CALIBRATION_H
#define _UAPI_MSM_AUDIO_CALIBRATION_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define CAL_IOCTL_MAGIC 'a'

#define AUDIO_ALLOCATE_CALIBRATION	_IOWR(CAL_IOCTL_MAGIC, \
							200, void *)
#define AUDIO_DEALLOCATE_CALIBRATION	_IOWR(CAL_IOCTL_MAGIC, \
							201, void *)
#define AUDIO_PREPARE_CALIBRATION	_IOWR(CAL_IOCTL_MAGIC, \
							202, void *)
#define AUDIO_SET_CALIBRATION		_IOWR(CAL_IOCTL_MAGIC, \
							203, void *)
#define AUDIO_GET_CALIBRATION		_IOWR(CAL_IOCTL_MAGIC, \
							204, void *)
#define AUDIO_POST_CALIBRATION		_IOWR(CAL_IOCTL_MAGIC, \
							205, void *)

/* For Real-Time Audio Calibration */
#define AUDIO_GET_RTAC_ADM_INFO		_IOR(CAL_IOCTL_MAGIC, \
							207, void *)
#define AUDIO_GET_RTAC_VOICE_INFO	_IOR(CAL_IOCTL_MAGIC, \
							208, void *)
#define AUDIO_GET_RTAC_ADM_CAL		_IOWR(CAL_IOCTL_MAGIC, \
							209, void *)
#define AUDIO_SET_RTAC_ADM_CAL		_IOWR(CAL_IOCTL_MAGIC, \
							210, void *)
#define AUDIO_GET_RTAC_ASM_CAL		_IOWR(CAL_IOCTL_MAGIC, \
							211, void *)
#define AUDIO_SET_RTAC_ASM_CAL		_IOWR(CAL_IOCTL_MAGIC, \
							212, void *)
#define AUDIO_GET_RTAC_CVS_CAL		_IOWR(CAL_IOCTL_MAGIC, \
							213, void *)
#define AUDIO_SET_RTAC_CVS_CAL		_IOWR(CAL_IOCTL_MAGIC, \
							214, void *)
#define AUDIO_GET_RTAC_CVP_CAL		_IOWR(CAL_IOCTL_MAGIC, \
							215, void *)
#define AUDIO_SET_RTAC_CVP_CAL		_IOWR(CAL_IOCTL_MAGIC, \
							216, void *)
#define AUDIO_GET_RTAC_AFE_CAL		_IOWR(CAL_IOCTL_MAGIC, \
							217, void *)
#define AUDIO_SET_RTAC_AFE_CAL		_IOWR(CAL_IOCTL_MAGIC, \
							218, void *)
enum {
	CVP_VOC_RX_TOPOLOGY_CAL_TYPE = 0,
	CVP_VOC_TX_TOPOLOGY_CAL_TYPE,
	CVP_VOCPROC_STATIC_CAL_TYPE,
	CVP_VOCPROC_DYNAMIC_CAL_TYPE,
	CVS_VOCSTRM_STATIC_CAL_TYPE,
	CVP_VOCDEV_CFG_CAL_TYPE,
	CVP_VOCPROC_STATIC_COL_CAL_TYPE,
	CVP_VOCPROC_DYNAMIC_COL_CAL_TYPE,
	CVS_VOCSTRM_STATIC_COL_CAL_TYPE,

	ADM_TOPOLOGY_CAL_TYPE,
	ADM_CUST_TOPOLOGY_CAL_TYPE,
	ADM_AUDPROC_CAL_TYPE,
	ADM_AUDVOL_CAL_TYPE,

	ASM_TOPOLOGY_CAL_TYPE,
	ASM_CUST_TOPOLOGY_CAL_TYPE,
	ASM_AUDSTRM_CAL_TYPE,

	AFE_COMMON_RX_CAL_TYPE,
	AFE_COMMON_TX_CAL_TYPE,
	AFE_ANC_CAL_TYPE,
	AFE_AANC_CAL_TYPE,
	AFE_FB_SPKR_PROT_CAL_TYPE,
	AFE_HW_DELAY_CAL_TYPE,
	AFE_SIDETONE_CAL_TYPE,
	AFE_TOPOLOGY_CAL_TYPE,
	AFE_CUST_TOPOLOGY_CAL_TYPE,

	LSM_CUST_TOPOLOGY_CAL_TYPE,
	LSM_TOPOLOGY_CAL_TYPE,
	LSM_CAL_TYPE,

	ADM_RTAC_INFO_CAL_TYPE,
	VOICE_RTAC_INFO_CAL_TYPE,
	ADM_RTAC_APR_CAL_TYPE,
	ASM_RTAC_APR_CAL_TYPE,
	VOICE_RTAC_APR_CAL_TYPE,

	MAD_CAL_TYPE,
	ULP_AFE_CAL_TYPE,
	ULP_LSM_CAL_TYPE,

	DTS_EAGLE_CAL_TYPE,
	AUDIO_CORE_METAINFO_CAL_TYPE,
	SRS_TRUMEDIA_CAL_TYPE,

	CORE_CUSTOM_TOPOLOGIES_CAL_TYPE,
	ADM_RTAC_AUDVOL_CAL_TYPE,

	ULP_LSM_TOPOLOGY_ID_CAL_TYPE,
	AFE_FB_SPKR_PROT_TH_VI_CAL_TYPE,
	AFE_FB_SPKR_PROT_EX_VI_CAL_TYPE,
	AFE_SIDETONE_IIR_CAL_TYPE,
	AFE_LSM_TOPOLOGY_CAL_TYPE,
	AFE_LSM_TX_CAL_TYPE,
	ADM_LSM_TOPOLOGY_CAL_TYPE,
	ADM_LSM_AUDPROC_CAL_TYPE,
	ADM_LSM_AUDPROC_PERSISTENT_CAL_TYPE,
	ADM_AUDPROC_PERSISTENT_CAL_TYPE,
	AFE_FB_SPKR_PROT_V4_EX_VI_CAL_TYPE,
	MAX_CAL_TYPES,
};

#define AFE_FB_SPKR_PROT_TH_VI_CAL_TYPE AFE_FB_SPKR_PROT_TH_VI_CAL_TYPE
#define AFE_FB_SPKR_PROT_EX_VI_CAL_TYPE AFE_FB_SPKR_PROT_EX_VI_CAL_TYPE
#define AFE_FB_SPKR_PROT_V4_EX_VI_CAL_TYPE AFE_FB_SPKR_PROT_V4_EX_VI_CAL_TYPE

#define AFE_SIDETONE_IIR_CAL_TYPE AFE_SIDETONE_IIR_CAL_TYPE

#define AFE_LSM_TOPOLOGY_CAL_TYPE AFE_LSM_TOPOLOGY_CAL_TYPE
#define AFE_LSM_TX_CAL_TYPE AFE_LSM_TX_CAL_TYPE
#define ADM_LSM_TOPOLOGY_CAL_TYPE ADM_LSM_TOPOLOGY_CAL_TYPE
#define ADM_LSM_AUDPROC_CAL_TYPE ADM_LSM_AUDPROC_CAL_TYPE
#define ADM_LSM_AUDPROC_PERSISTENT_CAL_TYPE ADM_LSM_AUDPROC_PERSISTENT_CAL_TYPE
#define ADM_AUDPROC_PERSISTENT_CAL_TYPE ADM_AUDPROC_PERSISTENT_CAL_TYPE
#define LSM_CAL_TYPES

#define TOPOLOGY_SPECIFIC_CHANNEL_INFO
#define MSM_SPKR_PROT_SPV3
#define MSM_SPKR_PROT_SPV4
#define MSM_CMA_MEM_ALLOC

enum {
	VERSION_0_0,
};

enum {
	PER_VOCODER_CAL_BIT_MASK = 0x10000,
};

#define MAX_IOCTL_CMD_SIZE	512

/* common structures */

struct audio_cal_header {
	__s32		data_size;
	__s32		version;
	__s32		cal_type;
	__s32		cal_type_size;
};

struct audio_cal_type_header {
	__s32		version;
	__s32		buffer_number;
};

struct audio_cal_data {
	/* Size of cal data at mem_handle allocation or at vaddr */
	__s32		cal_size;
	/* If mem_handle if shared memory is used*/
	__s32		mem_handle;
#ifdef MSM_CMA_MEM_ALLOC
	/* cma allocation flag if cma heap memory is used */
	__u32		cma_mem;
#endif
};


/* AUDIO_ALLOCATE_CALIBRATION */
struct audio_cal_type_alloc {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
};

struct audio_cal_alloc {
	struct audio_cal_header		hdr;
	struct audio_cal_type_alloc	cal_type;
};


/* AUDIO_DEALLOCATE_CALIBRATION */
struct audio_cal_type_dealloc {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
};

struct audio_cal_dealloc {
	struct audio_cal_header		hdr;
	struct audio_cal_type_dealloc	cal_type;
};


/* AUDIO_PREPARE_CALIBRATION */
struct audio_cal_type_prepare {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
};

struct audio_cal_prepare {
	struct audio_cal_header		hdr;
	struct audio_cal_type_prepare	cal_type;
};


/* AUDIO_POST_CALIBRATION */
struct audio_cal_type_post {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
};

struct audio_cal_post {
	struct audio_cal_header		hdr;
	struct audio_cal_type_post	cal_type;
};

/*AUDIO_CORE_META_INFO */

struct audio_cal_info_metainfo {
	__u32 nKey;
};

/* Cal info types */
enum {
	RX_DEVICE,
	TX_DEVICE,
	MAX_PATH_TYPE
};

struct audio_cal_info_adm_top {
	__s32		topology;
	__s32		acdb_id;
	/* RX_DEVICE or TX_DEVICE */
	__s32		path;
	__s32		app_type;
	__s32		sample_rate;
};

struct audio_cal_info_audproc {
	__s32		acdb_id;
	/* RX_DEVICE or TX_DEVICE */
	__s32		path;
	__s32		app_type;
	__s32		sample_rate;
};

struct audio_cal_info_audvol {
	__s32		acdb_id;
	/* RX_DEVICE or TX_DEVICE */
	__s32		path;
	__s32		app_type;
	__s32		vol_index;
};

struct audio_cal_info_afe {
	__s32		acdb_id;
	/* RX_DEVICE or TX_DEVICE */
	__s32		path;
	__s32		sample_rate;
};

struct audio_cal_info_afe_top {
	__s32		topology;
	__s32		acdb_id;
	/* RX_DEVICE or TX_DEVICE */
	__s32		path;
	__s32		sample_rate;
};

struct audio_cal_info_asm_top {
	__s32		topology;
	__s32		app_type;
};

struct audio_cal_info_audstrm {
	__s32		app_type;
};

struct audio_cal_info_aanc {
	__s32		acdb_id;
};

#define MAX_HW_DELAY_ENTRIES	25

struct audio_cal_hw_delay_entry {
	__u32 sample_rate;
	__u32 delay_usec;
};

struct audio_cal_hw_delay_data {
	__u32				num_entries;
	struct audio_cal_hw_delay_entry		entry[MAX_HW_DELAY_ENTRIES];
};

struct audio_cal_info_hw_delay {
	__s32					acdb_id;
	/* RX_DEVICE or TX_DEVICE */
	__s32					path;
	__s32					property_type;
	struct audio_cal_hw_delay_data		data;
};

enum msm_spkr_prot_states {
	MSM_SPKR_PROT_CALIBRATED,
	MSM_SPKR_PROT_CALIBRATION_IN_PROGRESS,
	MSM_SPKR_PROT_DISABLED,
	MSM_SPKR_PROT_NOT_CALIBRATED,
	MSM_SPKR_PROT_PRE_CALIBRATED,
	MSM_SPKR_PROT_IN_FTM_MODE,
	MSM_SPKR_PROT_IN_V_VALI_MODE
};
#define MSM_SPKR_PROT_IN_FTM_MODE MSM_SPKR_PROT_IN_FTM_MODE
#define MSM_SPKR_PROT_IN_V_VALI_MODE MSM_SPKR_PROT_IN_V_VALI_MODE

enum msm_spkr_count {
	SP_V2_SPKR_1,
	SP_V2_SPKR_2,
	SP_V2_NUM_MAX_SPKRS
};

struct audio_cal_info_spk_prot_cfg {
	__s32		r0[SP_V2_NUM_MAX_SPKRS];
	__s32		t0[SP_V2_NUM_MAX_SPKRS];
	__u32	quick_calib_flag;
	__u32	mode;
	/*
	 * 0 - Start spk prot
	 * 1 - Start calib
	 * 2 - Disable spk prot
	 */
#ifdef MSM_SPKR_PROT_SPV3
	__u32	sp_version;
	__s32	limiter_th[SP_V2_NUM_MAX_SPKRS];
#endif
};

struct audio_cal_info_sp_th_vi_ftm_cfg {
	/*
	 * mode should be first param, add new params later to this.
	 * we use this mode(first 4 bytes) to differentiate
	 * whether it is TH_VI FTM or v-validation.
	 */
	__u32	mode;
	/*
	 * 0 - normal running mode
	 * 1 - Calibration
	 * 2 - FTM mode
	 */
	__u32	wait_time[SP_V2_NUM_MAX_SPKRS];
	__u32	ftm_time[SP_V2_NUM_MAX_SPKRS];
};

struct audio_cal_info_sp_th_vi_v_vali_cfg {
	/*
	 * mode should be first param, add new params later to this.
	 * we use this mode(first 4 bytes) to differentiate
	 * whether it is TH_VI FTM or v-validation.
	 */
	__u32	mode;
	/*
	 * 0 - normal running mode
	 * 1 - Calibration
	 * 2 - FTM mode
	 * 3 - V-Validation mode
	 */
	__u32	wait_time[SP_V2_NUM_MAX_SPKRS];
	__u32	vali_time[SP_V2_NUM_MAX_SPKRS];

};

struct audio_cal_info_sp_ex_vi_ftm_cfg {
	__u32	wait_time[SP_V2_NUM_MAX_SPKRS];
	__u32	ftm_time[SP_V2_NUM_MAX_SPKRS];
	__u32	mode;
	/*
	 * 0 - normal running mode
	 * 2 - FTM mode
	 */
};

struct audio_cal_info_sp_ex_vi_param {
	__s32		freq_q20[SP_V2_NUM_MAX_SPKRS];
	__s32		resis_q24[SP_V2_NUM_MAX_SPKRS];
	__s32		qmct_q24[SP_V2_NUM_MAX_SPKRS];
	__s32		status[SP_V2_NUM_MAX_SPKRS];
};

struct audio_cal_info_sp_v4_ex_vi_param {
	__s32		ftm_re_q24[SP_V2_NUM_MAX_SPKRS];
	__s32		ftm_Bl_q24[SP_V2_NUM_MAX_SPKRS];
	__s32		ftm_Rms_q24[SP_V2_NUM_MAX_SPKRS];
	__s32		ftm_Kms_q24[SP_V2_NUM_MAX_SPKRS];
	__s32		ftm_freq_q20[SP_V2_NUM_MAX_SPKRS];
	__s32		ftm_Qms_q24[SP_V2_NUM_MAX_SPKRS];
	__u32		status[SP_V2_NUM_MAX_SPKRS];
};

struct audio_cal_info_sp_th_vi_param {
	/*
	 * mode should be first param, add new params later to this.
	 * we use this mode(first 4 bytes) to differentiate
	 * whether it is TH_VI FTM or v-validation.
	 */
	__u32	mode;
	__s32		r_dc_q24[SP_V2_NUM_MAX_SPKRS];
	__s32		temp_q22[SP_V2_NUM_MAX_SPKRS];
	__s32		status[SP_V2_NUM_MAX_SPKRS];
};

struct audio_cal_info_sp_th_vi_v_vali_param {
	/*
	 * mode should be first param, add new params later to this.
	 * we use this mode(first 4 bytes) to differentiate
	 * whether it is TH_VI FTM or v-validation.
	 */
	__u32	mode;
	__u32	vrms_q24[SP_V2_NUM_MAX_SPKRS];
	__s32		status[SP_V2_NUM_MAX_SPKRS];
};

struct audio_cal_info_msm_spk_prot_status {
	__s32		r0[SP_V2_NUM_MAX_SPKRS];
	__s32		status;
};

struct audio_cal_info_sidetone {
	__u16	enable;
	__u16	gain;
	__s32		tx_acdb_id;
	__s32		rx_acdb_id;
	__s32		mid;
	__s32		pid;
};

#define MAX_SIDETONE_IIR_DATA_SIZE   224
#define MAX_NO_IIR_FILTER_STAGE      10

struct audio_cal_info_sidetone_iir {
	__u16	iir_enable;
	__u16	num_biquad_stages;
	__u16	pregain;
	__s32	        tx_acdb_id;
	__s32	        rx_acdb_id;
	__s32	        mid;
	__s32	        pid;
	__u8	        iir_config[MAX_SIDETONE_IIR_DATA_SIZE];
};
struct audio_cal_info_lsm_top {
	__s32		topology;
	__s32		acdb_id;
	__s32		app_type;
};


struct audio_cal_info_lsm {
	__s32		acdb_id;
	/* RX_DEVICE or TX_DEVICE */
	__s32		path;
	__s32		app_type;
};

#define VSS_NUM_CHANNELS_MAX	32

struct audio_cal_info_voc_top {
	__s32		topology;
	__s32		acdb_id;
#ifdef TOPOLOGY_SPECIFIC_CHANNEL_INFO
	__u32	num_channels;
	__u8		channel_mapping[VSS_NUM_CHANNELS_MAX];
#endif
};

struct audio_cal_info_vocproc {
	__s32		tx_acdb_id;
	__s32		rx_acdb_id;
	__s32		tx_sample_rate;
	__s32		rx_sample_rate;
};

enum {
	DEFAULT_FEATURE_SET,
	VOL_BOOST_FEATURE_SET,
};

struct audio_cal_info_vocvol {
	__s32		tx_acdb_id;
	__s32		rx_acdb_id;
	/* DEFAULT_ or VOL_BOOST_FEATURE_SET */
	__s32		feature_set;
};

struct audio_cal_info_vocdev_cfg {
	__s32		tx_acdb_id;
	__s32		rx_acdb_id;
};

#define MAX_VOICE_COLUMNS	20

union audio_cal_col_na {
	__u8		val8;
	__u16	val16;
	__u32	val32;
	__u64	val64;
} __packed;

struct audio_cal_col {
	__u32		id;
	__u32		type;
	union audio_cal_col_na	na_value;
} __packed;

struct audio_cal_col_data {
	__u32		num_columns;
	struct audio_cal_col	column[MAX_VOICE_COLUMNS];
} __packed;

struct audio_cal_info_voc_col {
	__s32				table_id;
	__s32				tx_acdb_id;
	__s32				rx_acdb_id;
	struct audio_cal_col_data	data;
};

/* AUDIO_SET_CALIBRATION & */
struct audio_cal_type_basic {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
};

struct audio_cal_basic {
	struct audio_cal_header		hdr;
	struct audio_cal_type_basic	cal_type;
};

struct audio_cal_type_adm_top {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_adm_top	cal_info;
};

struct audio_cal_adm_top {
	struct audio_cal_header		hdr;
	struct audio_cal_type_adm_top	cal_type;
};

struct audio_cal_type_metainfo {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_metainfo	cal_info;
};

struct audio_core_metainfo {
	struct audio_cal_header	  hdr;
	struct audio_cal_type_metainfo cal_type;
};

struct audio_cal_type_audproc {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_audproc	cal_info;
};

struct audio_cal_audproc {
	struct audio_cal_header		hdr;
	struct audio_cal_type_audproc	cal_type;
};

struct audio_cal_type_audvol {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_audvol	cal_info;
};

struct audio_cal_audvol {
	struct audio_cal_header		hdr;
	struct audio_cal_type_audvol	cal_type;
};

struct audio_cal_type_asm_top {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_asm_top	cal_info;
};

struct audio_cal_asm_top {
	struct audio_cal_header		hdr;
	struct audio_cal_type_asm_top	cal_type;
};

struct audio_cal_type_audstrm {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_audstrm	cal_info;
};

struct audio_cal_audstrm {
	struct audio_cal_header		hdr;
	struct audio_cal_type_audstrm	cal_type;
};

struct audio_cal_type_afe {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_afe	cal_info;
};

struct audio_cal_afe {
	struct audio_cal_header		hdr;
	struct audio_cal_type_afe	cal_type;
};

struct audio_cal_type_afe_top {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_afe_top	cal_info;
};

struct audio_cal_afe_top {
	struct audio_cal_header		hdr;
	struct audio_cal_type_afe_top	cal_type;
};

struct audio_cal_type_aanc {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_aanc	cal_info;
};

struct audio_cal_aanc {
	struct audio_cal_header		hdr;
	struct audio_cal_type_aanc	cal_type;
};

struct audio_cal_type_fb_spk_prot_cfg {
	struct audio_cal_type_header		cal_hdr;
	struct audio_cal_data			cal_data;
	struct audio_cal_info_spk_prot_cfg	cal_info;
};

struct audio_cal_fb_spk_prot_cfg {
	struct audio_cal_header			hdr;
	struct audio_cal_type_fb_spk_prot_cfg	cal_type;
};

struct audio_cal_type_sp_th_vi_ftm_cfg {
	struct audio_cal_type_header		cal_hdr;
	struct audio_cal_data			cal_data;
	struct audio_cal_info_sp_th_vi_ftm_cfg	cal_info;
};

struct audio_cal_sp_th_vi_ftm_cfg {
	struct audio_cal_header			hdr;
	struct audio_cal_type_sp_th_vi_ftm_cfg	cal_type;
};

struct audio_cal_type_sp_th_vi_v_vali_cfg {
	struct audio_cal_type_header		cal_hdr;
	struct audio_cal_data			cal_data;
	struct audio_cal_info_sp_th_vi_v_vali_cfg	cal_info;
};

struct audio_cal_sp_th_vi_v_vali_cfg {
	struct audio_cal_header			hdr;
	struct audio_cal_type_sp_th_vi_v_vali_cfg	cal_type;
};

struct audio_cal_type_sp_ex_vi_ftm_cfg {
	struct audio_cal_type_header		cal_hdr;
	struct audio_cal_data			cal_data;
	struct audio_cal_info_sp_ex_vi_ftm_cfg	cal_info;
};

struct audio_cal_sp_ex_vi_ftm_cfg {
	struct audio_cal_header			hdr;
	struct audio_cal_type_sp_ex_vi_ftm_cfg	cal_type;
};
struct audio_cal_type_hw_delay {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_hw_delay	cal_info;
};

struct audio_cal_hw_delay {
	struct audio_cal_header		hdr;
	struct audio_cal_type_hw_delay	cal_type;
};

struct audio_cal_type_sidetone {
	struct audio_cal_type_header		cal_hdr;
	struct audio_cal_data			cal_data;
	struct audio_cal_info_sidetone		cal_info;
};

struct audio_cal_sidetone {
	struct audio_cal_header			hdr;
	struct audio_cal_type_sidetone		cal_type;
};

struct audio_cal_type_sidetone_iir {
	struct audio_cal_type_header	   cal_hdr;
	struct audio_cal_data		   cal_data;
	struct audio_cal_info_sidetone_iir cal_info;
};

struct audio_cal_sidetone_iir {
	struct audio_cal_header		   hdr;
	struct audio_cal_type_sidetone_iir cal_type;
};

struct audio_cal_type_lsm_top {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_lsm_top	cal_info;
};

struct audio_cal_lsm_top {
	struct audio_cal_header		hdr;
	struct audio_cal_type_lsm_top	cal_type;
};

struct audio_cal_type_lsm {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_lsm	cal_info;
};

struct audio_cal_lsm {
	struct audio_cal_header		hdr;
	struct audio_cal_type_lsm	cal_type;
};

struct audio_cal_type_voc_top {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_voc_top	cal_info;
};

struct audio_cal_voc_top {
	struct audio_cal_header		hdr;
	struct audio_cal_type_voc_top	cal_type;
};

struct audio_cal_type_vocproc {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_vocproc	cal_info;
};

struct audio_cal_vocproc {
	struct audio_cal_header		hdr;
	struct audio_cal_type_vocproc	cal_type;
};

struct audio_cal_type_vocvol {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_vocvol	cal_info;
};

struct audio_cal_vocvol {
	struct audio_cal_header		hdr;
	struct audio_cal_type_vocvol	cal_type;
};

struct audio_cal_type_vocdev_cfg {
	struct audio_cal_type_header		cal_hdr;
	struct audio_cal_data			cal_data;
	struct audio_cal_info_vocdev_cfg	cal_info;
};

struct audio_cal_vocdev_cfg {
	struct audio_cal_header			hdr;
	struct audio_cal_type_vocdev_cfg	cal_type;
};

struct audio_cal_type_voc_col {
	struct audio_cal_type_header	cal_hdr;
	struct audio_cal_data		cal_data;
	struct audio_cal_info_voc_col	cal_info;
};

struct audio_cal_voc_col {
	struct audio_cal_header		hdr;
	struct audio_cal_type_voc_col	cal_type;
};

/* AUDIO_GET_CALIBRATION */
struct audio_cal_type_fb_spk_prot_status {
	struct audio_cal_type_header			cal_hdr;
	struct audio_cal_data				cal_data;
	struct audio_cal_info_msm_spk_prot_status	cal_info;
};

struct audio_cal_fb_spk_prot_status {
	struct audio_cal_header				hdr;
	struct audio_cal_type_fb_spk_prot_status	cal_type;
};

struct audio_cal_type_sp_th_vi_param {
	struct audio_cal_type_header			cal_hdr;
	struct audio_cal_data				cal_data;
	struct audio_cal_info_sp_th_vi_param		cal_info;
};

struct audio_cal_sp_th_vi_param {
	struct audio_cal_header				hdr;
	struct audio_cal_type_sp_th_vi_param		cal_type;
};

struct audio_cal_type_sp_th_vi_v_vali_param {
	struct audio_cal_type_header			cal_hdr;
	struct audio_cal_data				cal_data;
	struct audio_cal_info_sp_th_vi_v_vali_param	cal_info;
};

struct audio_cal_sp_th_vi_v_vali_param {
	struct audio_cal_header				hdr;
	struct audio_cal_type_sp_th_vi_v_vali_param	cal_type;
};

struct audio_cal_type_sp_ex_vi_param {
	struct audio_cal_type_header			cal_hdr;
	struct audio_cal_data				cal_data;
	struct audio_cal_info_sp_ex_vi_param		cal_info;
};

struct audio_cal_sp_ex_vi_param {
	struct audio_cal_header				hdr;
	struct audio_cal_type_sp_ex_vi_param		cal_type;
};

struct audio_cal_type_sp_v4_ex_vi_param {
	struct audio_cal_type_header			cal_hdr;
	struct audio_cal_data				cal_data;
	struct audio_cal_info_sp_v4_ex_vi_param		cal_info;
};

struct audio_cal_sp_v4_ex_vi_param {
	struct audio_cal_header				hdr;
	struct audio_cal_type_sp_v4_ex_vi_param		cal_type;
};

#endif /* _UAPI_MSM_AUDIO_CALIBRATION_H */
