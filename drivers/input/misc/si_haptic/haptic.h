/*
 *  Silicon Integrated Co., Ltd haptic sih688x haptic header file
 *
 *  Copyright (c) 2021 kugua <canzhen.peng@si-in.com>
 *  Copyright (c) 2021 tianchi <tianchi.zheng@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#ifndef _HAPTIC_H_
#define _HAPTIC_H_

#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <linux/input.h>
/*********************************************************
 *
 * Conditional Marco
 *
 *********************************************************/
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 1)
#define KERNEL_VERSION_49
#endif
#define IOCTL_MMAP_BUF_SIZE					1000
#define SIH_WAVEFORM_MAX_NUM				20
#define SIH_RAMWAVEFORM_MAX_NUM				3
#define SIH_HAPTIC_MAX_DEV_NUM				4
#define SIH_HAPTIC_MMAP_DEV_INDEX			0
#define SIH_HAPTIC_DEV_NUM					1
#define SIH_HAPTIC_SEQUENCER_SIZE			8
#define SIH_HAPTIC_MAX_GAIN					255
#define SIH_HAPTIC_GAIN_LIMIT				128
#define SIH_HAPTIC_REG_SEQLOOP_MAX			15
#define SIH_HAPTIC_SEQUENCER_GAIN_SIZE		4
#define SIH_HAPTIC_RAM_MAX_SIZE				8192
#define SIH_RAMDATA_BUFFER_SIZE				6144
#define SIH_RAMDATA_READ_SIZE				1024
#define SIH_PM_QOS_VALUE_VB					400
#define SIH_RTP_NAME_MAX					64
#define SIH_RTP_START_DEFAULT_THRES			0x05
#define SIH_TRIG_NUM						3
#define SIH_F0_PRE_VALUE					1700
#define SIH_F0_TARGET_VALUE					170
#define SIH_DETECT_FIFO_SIZE				128
#define SIH_RAM_MAIN_LOOP_MAX_TIME			15
#define SIH_RTP_FILE_MAX_NUM				10
#define SIH_RTP_ZERO_MAX_INTERVAL			20
#define SIH_RTP_FILE_FREQUENCY				170
#define SIH_RESET_GPIO_SET					1
#define SIH_RESET_GPIO_RESET				0
#define SIH_F0_MAX_THRESHOLD				1800
#define SIH_F0_MIN_THRESHOLD				1600
#define SIH_F0_DETECT_TRY					4
#define SIH_INIT_ZERO_VALUE					0
#define SIH_DRIVER_VBOOST_INIT_VALUE		80
#define SIH_WAIT_FOR_STANDBY_MAX_TRY		200
#define SIH_ENTER_RTP_MODE_MAX_TRY			200
#define SIH_PROTECTION_TIME					10000
#define SIH_LRA_NAME_LEN					8
#define SIH_OSC_PLAY_FILE_INDEX				2
#define SIH_POLAR_PLAY_FILE_INDEX			2
#define FF_EFFECT_COUNT_MAX					(32)
#define INPUT_DEV

#ifdef INPUT_DEV
typedef struct input_dev cdev_t;
#endif
/*********************************************************
 *
 * Common enumeration
 *
 *********************************************************/
typedef enum sih_haptic_work_mode {
	SIH_IDLE_MODE = 0,
	SIH_RAM_MODE = 1,
	SIH_RTP_MODE = 2,
	SIH_TRIG_MODE = 3,
	SIH_CONT_MODE = 4,
	SIH_RAM_LOOP_MODE = 5,
} sih_haptic_work_mode_e;

typedef enum sih_haptic_state_status {
	SIH_STANDBY_MODE = 0,
	SIH_ACTIVE_MODE = 1,
} sih_haptic_state_status_e;

typedef enum sih_haptic_rw_type {
	SIH_BURST_WRITE = 0,
	SIH_BURST_READ = 1,
} sih_haptic_rw_type_e;

typedef enum sih_haptic_ram_vbat_comp_mode {
	SIH_RAM_VBAT_COMP_DISABLE = 0,
	SIH_RAM_VBAT_COMP_ENABLE = 1,
} sih_haptic_ram_vbat_comp_mode_e;

typedef enum sih_haptic_cali_lra {
	SIH_WRITE_ZERO = 0,
	SIH_F0_CALI_LRA = 1,
	SIH_OSC_CALI_LRA = 2,
} sih_haptic_cali_lra_e;

typedef enum sih_haptic_pwm_sample_rpt {
	SIH_SAMPLE_RPT_ONE_TIME = 0,
	SIH_SAMPLE_RPT_TWO_TIME = 1,
	SIH_SAMPLE_RPT_FOUR_TIME = 3,
} sih_haptic_sample_rpt_e;

typedef enum sih_haptic_rtp_play_mode {
	SIH_RTP_NORMAL_PLAY = 0,
	SIH_RTP_OSC_PLAY = 1,
	SIH_RTP_POLAR_PLAY = 2,
} sih_haptic_rtp_play_mode_e;

#ifdef INPUT_DEV
enum haptics_custom_effect_param {
	CUSTOM_DATA_EFFECT_IDX,
	CUSTOM_DATA_TIMEOUT_SEC_IDX,
	CUSTOM_DATA_TIMEOUT_MSEC_IDX,
	CUSTOM_DATA_LEN,
};
#endif
/*********************************************************
 *
 * Common vibrator mode
 *
 *********************************************************/
typedef struct sih_chip_reg {
	uint32_t rw_type;
	uint32_t reg_num;
	uint8_t *reg_addr;
} sih_chip_reg_t;

typedef struct sih_chip_inter_para {
	uint8_t play_mode;					/* chip actual state */
	uint8_t gain;
	u16 new_gain;
	bool state;							/* software control state */
	bool auto_pvdd_en;
	bool low_power;
	uint32_t duration;					/* ram loop play time */
	uint32_t interval_us;
	uint32_t drv_vboost;
	uint32_t brk_vboost;
	ktime_t kcur_time;
	ktime_t kpre_time;
#ifdef INPUT_DEV
	int effect_type;
	int effect_id;
	int effect_max;
	int effects_count;
	int is_custom_wave;
	int effect_id_boundary;
#endif
} sih_chip_inter_para_t;

typedef struct sih_rtp_para {
	bool rtp_init;
	uint32_t rtp_cnt_offset;
	uint32_t rtp_cnt;
	uint32_t rtp_file_num;
	uint8_t rtp_start_thres;
	struct mutex rtp_lock;
	struct work_struct rtp_work;
	struct haptic_container *rtp_cont;
	atomic_t is_in_rtp_loop;
	atomic_t exit_in_rtp_loop;
	atomic_t is_in_write_loop;
	wait_queue_head_t wait_q; /*wait queue for exit irq mode */
	wait_queue_head_t stop_wait_q; /* wait queue for stop rtp mode */
} sih_rtp_para_t;

typedef struct sih_ram_para {
	unsigned char ram_init;
	uint8_t seq[SIH_HAPTIC_SEQUENCER_SIZE];
	uint8_t loop[SIH_HAPTIC_SEQUENCER_SIZE];
	uint8_t gain[SIH_HAPTIC_SEQUENCER_SIZE];
	uint32_t main_loop;
	uint8_t action_mode;			/* ram or ram_loop mode */
	uint8_t ram_loop_lock;
	int index;
	uint32_t len;
	uint32_t check_sum;
	uint32_t base_addr;
	uint8_t wave_num;				/* ram library's wave num */
	uint8_t lib_index;				/* ram library's index */
	uint8_t version;
	uint8_t ram_shift;
	uint8_t baseaddr_shift;
	uint8_t ram_vbat_comp;
	struct work_struct ram_work;
	struct work_struct gain_work;	
} sih_ram_para_t;

typedef struct sih_detect_para {
	bool trig_detect_en;
	bool ram_detect_en;
	bool rtp_detect_en;
	bool cont_detect_en;
	bool detect_f0_read_done;
	uint8_t f0_cali_data;
	uint32_t drive_time;
	uint32_t detect_f0;
	uint32_t tracking_f0;
	uint32_t rl_offset;
	uint32_t vbat;
	uint32_t cali_target_value;
	uint64_t resistance;
} sih_detect_para_t;

typedef struct sih_brake_para {
	bool trig_brake_en;
	bool ram_brake_en;
	bool rtp_brake_en;
	bool cont_brake_en;
} sih_brake_para_t;

typedef struct sih_trig_para {
	bool enable;
	bool boost_bypass;		/* 0:boost enable 1:boost bypass */
	bool polar;				/* 0:high active 1:low active */
	uint8_t mode;
	uint8_t pose_id;
	uint8_t nege_id;
} sih_trig_para_t;

typedef struct sih_frame_core {
	cdev_t vib_dev;
} sih_frame_core_t;

typedef struct sih_chip_attr {
	int reset_gpio;
	int irq_gpio;
	char lra_name[SIH_LRA_NAME_LEN];
} sih_chip_attr_t;

typedef struct sih_osc_para {
	ktime_t kstart;
	ktime_t kend;
	bool start_flag;
	uint8_t osc_data;
	uint32_t actual_time;
	uint32_t osc_rtp_len;
	uint32_t theory_time;
} sih_osc_para_t;

typedef struct haptic_regmap {
	struct regmap *regmapping;
	const struct regmap_config *config;
} haptic_regmap_t;

#pragma pack(4)
typedef struct mmap_buf_format {
	 uint8_t status;
	 uint8_t bit;
	 int16_t length;
	 uint32_t reserve;
	 struct mmap_buf_format *kernel_next;
	 struct mmap_buf_format *user_next;
	 uint8_t data[IOCTL_MMAP_BUF_SIZE];
} mmap_buf_format_t;
#pragma pack()

typedef struct haptic_stream_play_para {
	 bool done_flag;
	 bool stream_mode;
	 uint8_t *rtp_ptr;
	 mmap_buf_format_t *start_buf;
	 struct work_struct stream_work;
} haptic_stream_play_para_t;

typedef struct sih_haptic {
	struct i2c_client *i2c;
	struct device *dev;
	struct mutex lock;
	struct hrtimer timer;
	ktime_t kcurrent_time;
	ktime_t kpre_enter_time;
	unsigned int interval_us;
	struct pm_qos_request pm_qos;
	sih_osc_para_t osc_para;
	sih_chip_reg_t chip_reg;
	sih_chip_attr_t chip_attr;
	sih_chip_inter_para_t chip_ipara;
	sih_detect_para_t detect;
	sih_brake_para_t brake_para;
	sih_frame_core_t soft_frame;
	sih_ram_para_t ram;
	sih_rtp_para_t rtp;
	sih_trig_para_t trig_para[SIH_TRIG_NUM];
	struct haptic_func *hp_func;
	haptic_regmap_t regmapp;
	haptic_stream_play_para_t stream_para;
	struct haptic_stream_func *stream_func;
	struct workqueue_struct *work_queue;
	bool f0_cali_status;
} sih_haptic_t;

typedef struct haptic_stream_func {
	int (*stream_rtp_work_init)(sih_haptic_t *);
	void (*stream_rtp_work_release)(sih_haptic_t *);
	bool (*is_stream_mode)(sih_haptic_t *);
} haptic_stream_func_t;

typedef struct haptic_container {
	int len;
	uint8_t data[];
} haptic_container_t;

typedef struct haptic_func {
	int (*probe)(sih_haptic_t *);
	void (*get_detect_f0)(sih_haptic_t *);
	int (*update_ram_config)(sih_haptic_t *, haptic_container_t *);
	void (*stop)(sih_haptic_t *);
	void (*get_vbat)(sih_haptic_t *);
	void (*play_go)(sih_haptic_t *, bool);
	void (*ram_init)(sih_haptic_t *, bool);
	void (*detect_fifo_ctrl)(sih_haptic_t *, bool);
	void (*get_lra_resistance)(sih_haptic_t *);
	void (*set_gain)(sih_haptic_t *, uint8_t);
	void (*vbat_comp)(sih_haptic_t *);
	void (*set_play_mode)(sih_haptic_t *, uint8_t);
	void (*set_drv_bst_vol)(sih_haptic_t *, uint32_t);
	void (*set_brk_bst_vol)(sih_haptic_t *, uint32_t);
	void (*set_repeat_seq)(sih_haptic_t *, uint8_t);
	void (*set_wav_seq)(sih_haptic_t *, uint8_t, uint8_t);
	void (*set_wav_loop)(sih_haptic_t *, uint8_t, uint8_t);
	size_t (*write_rtp_data)(sih_haptic_t *, uint8_t *, uint32_t);
	void (*clear_interrupt_state)(sih_haptic_t *);
	void (*set_ram_addr)(sih_haptic_t *);
	void (*interrupt_state_init)(sih_haptic_t *);
	void (*set_rtp_aei)(sih_haptic_t *, bool);
	void (*upload_f0)(sih_haptic_t *, uint8_t);
	void (*get_wav_seq)(sih_haptic_t *, uint32_t);
	void (*set_boost_mode)(sih_haptic_t *, bool);
	void (*get_first_wave_addr)(sih_haptic_t *, uint8_t *);
	void (*get_wav_loop)(sih_haptic_t *);
	ssize_t (*get_ram_data)(sih_haptic_t *, char *);
	bool (*get_rtp_fifo_full_state)(sih_haptic_t *);
	void (*set_auto_pvdd)(sih_haptic_t *, bool);
	void (*set_wav_main_loop)(sih_haptic_t *, uint8_t);
	void (*get_wav_main_loop)(sih_haptic_t *);
	void (*set_low_power_mode)(sih_haptic_t *, bool);
	bool (*if_chip_is_mode)(sih_haptic_t *, uint8_t);
	void (*set_trig_para)(sih_haptic_t *, uint32_t *);
	size_t (*get_trig_para)(sih_haptic_t *, char *);
	void (*chip_software_reset)(sih_haptic_t *);
	void (*chip_hardware_reset)(sih_haptic_t *);
	void (*set_start_thres)(sih_haptic_t *);
	void (*set_ram_seq_gain)(sih_haptic_t *, uint8_t, uint8_t);
	size_t (*get_ram_seq_gain)(sih_haptic_t *, char *);
	void (*set_brk_state)(sih_haptic_t *, uint8_t, bool);
	size_t (*get_brk_state)(sih_haptic_t *, char *);
	void (*set_detect_state)(sih_haptic_t *, uint8_t, bool);
	size_t (*get_detect_state)(sih_haptic_t *, char *);
	void (*read_detect_fifo)(sih_haptic_t *);
	void (*set_pwm_rate)(sih_haptic_t *, uint8_t, uint8_t);
	size_t (*get_pwm_rate)(sih_haptic_t *, char *);
	void (*init)(sih_haptic_t *);
	void (*update_chip_state)(sih_haptic_t *);
	bool (*get_rtp_fifo_empty_state)(sih_haptic_t *);
	void (*osc_cali)(sih_haptic_t *);
	int (*efuse_check)(sih_haptic_t *);
	void (*get_tracking_f0)(sih_haptic_t *);
	void (*set_cont_para)(sih_haptic_t *, uint8_t, uint8_t *);
	ssize_t (*get_cont_para)(sih_haptic_t *, uint8_t, char *);
	bool (*if_chip_is_detect_done)(sih_haptic_t *);
	void (*check_detect_state)(sih_haptic_t *, uint8_t);
} haptic_func_t;

typedef struct sih_haptic_ptr {
	unsigned char sih_num;
	sih_haptic_t *g_haptic[SIH_HAPTIC_MAX_DEV_NUM];
} sih_haptic_ptr_t;

/*********************************************************
 *
 * Function Call
 *
 *********************************************************/

sih_haptic_t *get_global_haptic_ptr(void);

/*********************************************************
 *
 * Extern
 *
 *********************************************************/
extern int CUSTOME_WAVE_ID;

#endif
