#ifndef _AW869X_H_
#define _AW869X_H_

/*********************************************************
 *
 * kernel version
 *
 ********************************************************/
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 4, 1)
#define TIMED_OUTPUT
#endif

/*********************************************************
 *
 * aw869x.h
 *
 ********************************************************/
#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#ifdef TIMED_OUTPUT
#include <../../../drivers/staging/android/timed_output.h>
#else
#include <linux/leds.h>
#endif

/*********************************************************
 *
 * Normal Marco
 *
 ********************************************************/
#define AW8695_CHIPID			0x95
#define AW8697_CHIPID			0x97
#define AW869X_I2C_NAME			"aw869x_haptic"
#define AW869X_HAPTIC_NAME		"aw869x_haptic"
#define AW_I2C_RETRIES			2
#define AW_I2C_RETRY_DELAY		2
#define AW_READ_CHIPID_RETRIES		3
#define AW_READ_CHIPID_RETRY_DELAY	2
#define AW869X_SEQUENCER_SIZE		8
#define AW869X_SEQUENCER_LOOP_SIZE	4
#define AW869X_RTP_NAME_MAX		64
#define AW869X_VBAT_REFER		4200
#define AW869X_VBAT_MIN			3000
#define AW869X_VBAT_MAX			4500
#define PM_QOS_VALUE_VB			400
#define OSC_CALIBRATION_T_LENGTH	5100000

/********************************************
 *
 * Functional Macro Definitions
 *
 *******************************************/
#define AWINIC_RAM_UPDATE_DELAY
#define AWINIC_READ_BIN_FLEXBALLY
/* #define AW_ENABLE_RTP_PRINT_LOG */
#define AW_CHECK_RAM_DATA

/********************************************
 *
 * trig config
 *
 *******************************************/
#define AW869X_TRIG_NUM			3
#define AW869X_TRG1_ENABLE		1
#define AW869X_TRG2_ENABLE		1
#define AW869X_TRG3_ENABLE		1

/*
 * trig default high level
 * ___________           _________________
 *           |           |
 *           |           |
 *           |___________|
 *        first edge
 *                   second edge
 *
 *
 * trig default low level
 *            ___________
 *           |           |
 *           |           |
 * __________|           |_________________
 *        first edge
 *                   second edge
 */
#define AW869X_TRG1_DEFAULT_LEVEL	1 /* 1: high level; 0: low level */
#define AW869X_TRG2_DEFAULT_LEVEL	1 /* 1: high level; 0: low level */
#define AW869X_TRG3_DEFAULT_LEVEL	1 /* 1: high level; 0: low level */

#define AW869X_TRG1_DUAL_EDGE		1 /* 1: dual edge; 0: first edge */
#define AW869X_TRG2_DUAL_EDGE		1 /* 1: dual edge; 0: first edge */
#define AW869X_TRG3_DUAL_EDGE		1 /* 1: dual edge; 0: first edge */

#define AW869X_TRG1_FIRST_EDGE_SEQ	1 /* trig1: first edge waveform seq */
#define AW869X_TRG1_SECOND_EDGE_SEQ	2 /* trig1: second edge waveform seq */
#define AW869X_TRG2_FIRST_EDGE_SEQ	1 /* trig2: first edge waveform seq */
#define AW869X_TRG2_SECOND_EDGE_SEQ	2 /* trig2: second edge waveform seq */
#define AW869X_TRG3_FIRST_EDGE_SEQ	1 /* trig3: first edge waveform seq */
#define AW869X_TRG3_SECOND_EDGE_SEQ	2 /* trig3: second edge waveform seq */

#if AW869X_TRG1_ENABLE
#define AW869X_TRG1_DEFAULT_ENABLE	AW869X_BIT_TRGCFG2_TRG1_ENABLE
#else
#define AW869X_TRG1_DEFAULT_ENABLE	AW869X_BIT_TRGCFG2_TRG1_DISABLE
#endif

#if AW869X_TRG2_ENABLE
#define AW869X_TRG2_DEFAULT_ENABLE	AW869X_BIT_TRGCFG2_TRG2_ENABLE
#else
#define AW869X_TRG2_DEFAULT_ENABLE	AW869X_BIT_TRGCFG2_TRG2_DISABLE
#endif

#if AW869X_TRG3_ENABLE
#define AW869X_TRG3_DEFAULT_ENABLE	AW869X_BIT_TRGCFG2_TRG3_ENABLE
#else
#define AW869X_TRG3_DEFAULT_ENABLE	AW869X_BIT_TRGCFG2_TRG3_DISABLE
#endif

#if AW869X_TRG1_DEFAULT_LEVEL
#define AW869X_TRG1_DEFAULT_POLAR	AW869X_BIT_TRGCFG1_TRG1_POLAR_POS
#else
#define AW869X_TRG1_DEFAULT_POLAR	AW869X_BIT_TRGCFG1_TRG1_POLAR_NEG
#endif

#if AW869X_TRG2_DEFAULT_LEVEL
#define AW869X_TRG2_DEFAULT_POLAR	AW869X_BIT_TRGCFG1_TRG2_POLAR_POS
#else
#define AW869X_TRG2_DEFAULT_POLAR	AW869X_BIT_TRGCFG1_TRG2_POLAR_NEG
#endif

#if AW869X_TRG3_DEFAULT_LEVEL
#define AW869X_TRG3_DEFAULT_POLAR	AW869X_BIT_TRGCFG1_TRG3_POLAR_POS
#else
#define AW869X_TRG3_DEFAULT_POLAR	AW869X_BIT_TRGCFG1_TRG3_POLAR_NEG
#endif

#if AW869X_TRG1_DUAL_EDGE
#define AW869X_TRG1_DEFAULT_EDGE	AW869X_BIT_TRGCFG1_TRG1_EDGE_POS_NEG
#else
#define AW869X_TRG1_DEFAULT_EDGE	AW869X_BIT_TRGCFG1_TRG1_EDGE_POS
#endif

#if AW869X_TRG2_DUAL_EDGE
#define AW869X_TRG2_DEFAULT_EDGE	AW869X_BIT_TRGCFG1_TRG2_EDGE_POS_NEG
#else
#define AW869X_TRG2_DEFAULT_EDGE	AW869X_BIT_TRGCFG1_TRG2_EDGE_POS
#endif

#if AW869X_TRG3_DUAL_EDGE
#define AW869X_TRG3_DEFAULT_EDGE	AW869X_BIT_TRGCFG1_TRG3_EDGE_POS_NEG
#else
#define AW869X_TRG3_DEFAULT_EDGE	AW869X_BIT_TRGCFG1_TRG3_EDGE_POS
#endif

/********************************************
 *
 * Enum Define
 *
 *******************************************/
enum aw869x_flags {
	AW869X_FLAG_NONR = 0,
	AW869X_FLAG_SKIP_INTERRUPTS = 1,
};

enum aw869x_haptic_read_write {
	AW869X_HAPTIC_CMD_READ_REG = 0,
	AW869X_HAPTIC_CMD_WRITE_REG = 1,
};

enum aw869x_haptic_work_mode {
	AW869X_HAPTIC_STANDBY_MODE = 0,
	AW869X_HAPTIC_RAM_MODE = 1,
	AW869X_HAPTIC_RTP_MODE = 2,
	AW869X_HAPTIC_TRIG_MODE = 3,
	AW869X_HAPTIC_CONT_MODE = 4,
	AW869X_HAPTIC_RAM_LOOP_MODE = 5,
};

enum aw869x_haptic_bst_mode {
	AW869X_HAPTIC_BYPASS_MODE = 0,
	AW869X_HAPTIC_BOOST_MODE = 1,
};

enum aw869x_haptic_activate_mode {
	AW869X_HAPTIC_ACTIVATE_RAM_MODE = 0,
	AW869X_HAPTIC_ACTIVATE_CONT_MODE = 1,
};

enum aw869x_haptic_cont_vbat_comp_mode {
	AW869X_HAPTIC_CONT_VBAT_SW_COMP_MODE = 0,
	AW869X_HAPTIC_CONT_VBAT_HW_COMP_MODE = 1,
};

enum aw869x_haptic_ram_vbat_comp_mode {
	AW869X_HAPTIC_RAM_VBAT_COMP_DISABLE = 0,
	AW869X_HAPTIC_RAM_VBAT_COMP_ENABLE = 1,
};

enum aw869x_haptic_f0_flag {
	AW869X_HAPTIC_LRA_F0 = 0,
	AW869X_HAPTIC_CALI_F0 = 1,
};

enum aw869x_haptic_pwm_mode {
	AW869X_PWM_48K = 0,
	AW869X_PWM_24K = 1,
	AW869X_PWM_12K = 2,
};

enum aw869x_haptic_play {
	AW869X_HAPTIC_PLAY_NULL = 0,
	AW869X_HAPTIC_PLAY_ENABLE = 1,
	AW869X_HAPTIC_PLAY_STOP = 2,
	AW869X_HAPTIC_PLAY_GAIN = 8,
};

enum aw869x_haptic_cmd {
	AW869X_HAPTIC_CMD_NULL = 0,
	AW869X_HAPTIC_CMD_ENABLE = 1,
	AW869X_HAPTIC_CMD_HAPTIC = 0x0f,
	AW869X_HAPTIC_CMD_TP = 0x10,
	AW869X_HAPTIC_CMD_SYS = 0xf0,
	AW869X_HAPTIC_CMD_STOP = 255,
};

enum aw869x_haptic_cali_lra {
	AW869X_HAPTIC_WRITE_ZERO = 0,
	AW869X_HAPTIC_F0_CALI_LRA = 1,
	AW869X_HAPTIC_RTP_CALI_LRA = 2,
};

enum aw869x_haptic_bst_pc {
	AW869X_HAPTIC_BST_PC_L1 = 0,
	AW869X_HAPTIC_BST_PC_L2 = 1,
};

/*********************************************************
 *
 * struct
 *
 ********************************************************/
struct fileops {
	unsigned char cmd;
	unsigned char reg;
	unsigned char ram_addrh;
	unsigned char ram_addrl;
};

struct ram {
	unsigned int len;
	unsigned int check_sum;
	unsigned int base_addr;
	unsigned char version;
	unsigned char ram_shift;
	unsigned char baseaddr_shift;
	unsigned char ram_num;
};

struct haptic_ctr {
	unsigned char cnt;
	unsigned char cmd;
	unsigned char play;
	unsigned char wavseq;
	unsigned char loop;
	unsigned char gain;
	struct list_head list;
};

struct haptic_audio {
	struct mutex lock;
	struct hrtimer timer;
	struct work_struct work;
	int delay_val;
	int timer_val;
	struct haptic_ctr ctr;
	struct list_head ctr_list;
	/* struct tp tp; */
	struct list_head list;
	/* struct haptic_audio_tp_size tp_size; */
	/* struct trust_zone_info output_tz_info[10]; */
	int tz_num;
	int tz_high_num;
	int tz_cnt_thr;
	int tz_cnt_max;
	unsigned int uevent_report_flag;
	unsigned int hap_cnt_outside_tz;
	unsigned int hap_cnt_max_outside_tz;
};

struct trig {
	unsigned char enable;
	unsigned char default_level;
	unsigned char dual_edge;
	unsigned char frist_seq;
	unsigned char second_seq;
};

struct aw869x_dts_info {
	unsigned int mode;
	unsigned int f0_pre;
	unsigned int f0_cali_percen;
	unsigned int cont_drv_lvl;
	unsigned int cont_drv_lvl_ov;
	unsigned int cont_td;
	unsigned int cont_zc_thr;
	unsigned int cont_num_brk;
	unsigned int f0_coeff;
	unsigned int duration_time[3];
	unsigned int f0_trace_parameter[4];
	unsigned int bemf_config[4];
	unsigned int sw_brake;
	unsigned int tset;
	unsigned int r_spare;
	unsigned int bstdbg[6];
	unsigned int parameter1;
};

struct aw869x {
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct device *dev;
	struct input_dev *input;

	struct mutex lock;
	struct mutex rtp_lock;
	struct hrtimer timer;
	struct work_struct vibrator_work;
	struct work_struct rtp_work;
	struct delayed_work ram_work;
#ifdef TIMED_OUTPUT
	struct timed_output_dev to_dev;
#else
	struct led_classdev cdev;
#endif
	struct fileops fileops;
	struct ram ram;
	bool haptic_ready;
	bool audio_ready;
	int pre_haptic_number;
	struct timeval current_time;
	struct timeval pre_enter_time;
	struct timeval start, end;
	unsigned int timeval_flags;
	unsigned int osc_cali_flag;
	unsigned long int microsecond;
	unsigned int sys_frequency;
	unsigned int rtp_len;

	int reset_gpio;
	int irq_gpio;

	unsigned char hwen_flag;
	unsigned char flags;
	unsigned char chipid;
	unsigned char bst_pc;
	unsigned char bst_pc_limit;
	unsigned char cont_play_md;
	unsigned char play_mode;
	unsigned char activate_mode;
	unsigned char auto_boost;

	int state;
	int duration;
	int amplitude;
	int index;
	int vmax;
	int gain;

	unsigned char seq[AW869X_SEQUENCER_SIZE];
	unsigned char loop[AW869X_SEQUENCER_SIZE];

	unsigned int rtp_cnt;
	unsigned int rtp_file_num;

	unsigned char rtp_init;
	unsigned char ram_init;
	unsigned char rtp_routine_on;

	unsigned int f0;
	unsigned int cont_f0;
	unsigned char max_pos_beme;
	unsigned char max_neg_beme;
	unsigned char f0_cali_flag;
	unsigned int theory_time;
	unsigned int lra_calib_data;
	unsigned int f0_calib_data;
	unsigned char ram_vbat_comp;
	unsigned int vbat;
	unsigned int lra;
	unsigned int interval_us;
	unsigned int gun_type;
	unsigned int bullet_nr;

	struct trig trig[AW869X_TRIG_NUM];

	struct haptic_audio haptic_audio;
	struct aw869x_dts_info info;
	unsigned int ramupdate_flag;
	unsigned int rtpupdate_flag;
	//Bug651594,chenrui1.wt,ADD,20210701,add osc_calib flag
	bool lra_calib_flag;
};

struct aw869x_container {
	int len;
	unsigned char data[];
};

/*********************************************************
 *
 * ioctl
 *
 ********************************************************/
struct aw869x_seq_loop {
	unsigned char loop[AW869X_SEQUENCER_SIZE];
};

struct aw869x_que_seq {
	unsigned char index[AW869X_SEQUENCER_SIZE];
};

#define AW869X_HAPTIC_IOCTL_MAGIC	'h'

#define AW869X_HAPTIC_SET_QUE_SEQ	\
		_IOWR(AW869X_HAPTIC_IOCTL_MAGIC, 1, struct aw869x_que_seq*)
#define AW869X_HAPTIC_SET_SEQ_LOOP	\
		_IOWR(AW869X_HAPTIC_IOCTL_MAGIC, 2, struct aw869x_seq_loop*)
#define AW869X_HAPTIC_PLAY_QUE_SEQ	\
		_IOWR(AW869X_HAPTIC_IOCTL_MAGIC, 3, unsigned int)
#define AW869X_HAPTIC_SET_BST_VOL	\
		_IOWR(AW869X_HAPTIC_IOCTL_MAGIC, 4, unsigned int)
#define AW869X_HAPTIC_SET_BST_PEAK_CUR	\
		_IOWR(AW869X_HAPTIC_IOCTL_MAGIC, 5, unsigned int)
#define AW869X_HAPTIC_SET_GAIN		\
		_IOWR(AW869X_HAPTIC_IOCTL_MAGIC, 6, unsigned int)
#define AW869X_HAPTIC_PLAY_REPEAT_SEQ	\
		_IOWR(AW869X_HAPTIC_IOCTL_MAGIC, 7, unsigned int)

#endif
