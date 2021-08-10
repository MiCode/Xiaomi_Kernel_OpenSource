#ifndef _AW8624_H_
#define _AW8624_H_

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
* aw8624.h
*
 ********************************************************/
#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include "haptic.h"
#ifdef TIMED_OUTPUT
#include <../../../drivers/staging/android/timed_output.h>
#else
#include <linux/leds.h>
#endif

/*********************************************************
*
* marco
*
********************************************************/

#define AW8624_I2C_RETRIES		(2)
#define AW8624_RTP_NAME_MAX		(64)
#define AW8624_SEQUENCER_SIZE		(8)
#define AW8624_SEQUENCER_LOOP_SIZE	(4)
#define AW8624_OSC_CALIBRATION_T_LENGTH	(5100000)
#define AW8624_PM_QOS_VALUE_VB		(400)
#define AW8624_VBAT_REFER		(4200)
#define AW8624_VBAT_MIN			(3000)
#define AW8624_VBAT_MAX			(4500)
#define AW8624_HAPTIC_NAME		("aw8624_haptic")


/*
* aw8624 dts info
*
* aw8624_brake_local[3][8]
* [0][0-7] is en_brake1,en_brake2,brake0_level,
* brake1_level,brake2_level,brake0_p_num,brake1_p_num,brake0_level;
*/
struct aw8624_dts_info {
	int aw8624_mode;
	int aw8624_f0_pre;
	int aw8624_f0_cali_percen;
	int aw8624_cont_drv_lvl;
	int aw8624_cont_drv_lvl_ov;
	int aw8624_cont_td;
	int aw8624_cont_zc_thr;
	int aw8624_cont_num_brk;
	int aw8624_f0_coeff;
	int aw8624_duration_time[5];
	int aw8624_cont_brake[3][8];
	int aw8624_f0_trace_parameter[4];
	int aw8624_bemf_config[4];
	int aw8624_sw_brake[2];
	int aw8624_wavseq[16];
	int aw8624_wavloop[10];
	int aw8624_td_brake[3];
	int aw8624_tset;
	int aw8624_parameter1;
};

enum aw8624_flags {
	AW8624_FLAG_NONR = 0,
	AW8624_FLAG_SKIP_INTERRUPTS = 1,
};

enum aw8624_chipids {
	AW8624_ID = 1,
};

enum aw8624_haptic_read_write {
	AW8624_HAPTIC_CMD_READ_REG = 0,
	AW8624_HAPTIC_CMD_WRITE_REG = 1,
};

enum aw8624_haptic_play {
	AW8624_HAPTIC_PLAY_NULL = 0,
	AW8624_HAPTIC_PLAY_ENABLE = 1,
	AW8624_HAPTIC_PLAY_STOP = 2,
	AW8624_HAPTIC_PLAY_GAIN = 8,
};

enum aw8624_haptic_cmd {
	AW8624_HAPTIC_CMD_NULL = 0,
	AW8624_HAPTIC_CMD_ENABLE = 1,
	AW8624_HAPTIC_CMD_HAPTIC = 0x0f,
	AW8624_HAPTIC_CMD_TP = 0x10,
	AW8624_HAPTIC_CMD_SYS = 0xf0,
	AW8624_HAPTIC_CMD_STOP = 255,
};
enum aw8624_haptic_work_mode {
	AW8624_HAPTIC_STANDBY_MODE = 0,
	AW8624_HAPTIC_RAM_MODE = 1,
	AW8624_HAPTIC_RTP_MODE = 2,
	AW8624_HAPTIC_TRIG_MODE = 3,
	AW8624_HAPTIC_CONT_MODE = 4,
	AW8624_HAPTIC_RAM_LOOP_MODE = 5,
};

enum aw8624_haptic_bst_mode {
	AW8624_HAPTIC_BYPASS_MODE = 0,
	AW8624_HAPTIC_BOOST_MODE = 1,
};

enum aw8624_haptic_activate_mode {
	AW8624_HAPTIC_ACTIVATE_RAM_MODE = 0,
	AW8624_HAPTIC_ACTIVATE_CONT_MODE = 1,
};

enum aw8624_haptic_vbat_comp_mode {
	AW8624_HAPTIC_VBAT_SW_COMP_MODE = 0,
	AW8624_HAPTIC_VBAT_HW_COMP_MODE = 1,
};

enum aw8624_haptic_ram_vbat_comp_mode {
	AW8624_HAPTIC_RAM_VBAT_COMP_DISABLE = 0,
	AW8624_HAPTIC_RAM_VBAT_COMP_ENABLE = 1,
};

enum aw8624_haptic_f0_flag {
	AW8624_HAPTIC_LRA_F0 = 0,
	AW8624_HAPTIC_CALI_F0 = 1,
};

enum aw8624_haptic_pwm_mode {
	AW8624_PWM_48K = 0,
	AW8624_PWM_24K = 1,
	AW8624_PWM_12K = 2,
};

enum aw8624_haptic_cali_lra {
	AW8624_HAPTIC_ZERO = 0,
	AW8624_HAPTIC_F0_CALI_LRA = 1,
	AW8624_HAPTIC_RTP_CALI_LRA = 2,
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



struct aw8624 {
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct device *dev;
	struct input_dev *input;
	struct aw8624_container *rtp_container;

	struct mutex lock;
	struct mutex rtp_lock;
	struct wake_lock wk_lock;
	unsigned char wk_lock_flag;
	struct hrtimer timer;
	struct work_struct vibrator_work;
	struct work_struct irq_work;
	struct work_struct rtp_work;
	struct delayed_work ram_work;
	struct delayed_work stop_work;
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
	unsigned int theory_time;
	unsigned int rtp_len;
	int reset_gpio;
	int irq_gpio;
	int reset_gpio_ret;
	int irq_gpio_ret;

	unsigned char hwen_flag;
	unsigned char flags;
	unsigned char chipid;
	unsigned char chipid_flag;
	unsigned char singlecycle;
	unsigned char play_mode;
	unsigned char activate_mode;
	unsigned char auto_boost;

	int state;
	int duration;
	int amplitude;
	int index;
	int vmax;
	int gain;
	int f0_value;

	unsigned char seq[AW8624_SEQUENCER_SIZE];
	unsigned char loop[AW8624_SEQUENCER_SIZE];

	unsigned int gun_type;
	unsigned int bullet_nr;
	unsigned int rtp_cnt;
	unsigned int rtp_file_num;

	unsigned char rtp_init;
	unsigned char ram_init;
	unsigned char rtp_routine_on;

	unsigned int f0;
	unsigned int f0_pre;
	unsigned int cont_td;
	unsigned int cont_f0;
	unsigned int cont_zc_thr;
	unsigned char cont_drv_lvl;
	unsigned char cont_drv_lvl_ov;
	unsigned char cont_num_brk;
	unsigned char max_pos_beme;
	unsigned char max_neg_beme;
	unsigned char f0_cali_flag;
	bool IsUsedIRQ;
	bool ram_update_delay;
	struct haptic_audio haptic_audio;

	unsigned char ram_vbat_comp;
	unsigned int vbat;
	unsigned int lra;
	unsigned int interval_us;
	unsigned int ramupdate_flag;
	unsigned int rtpupdate_flag;
	unsigned int osc_cali_run;
	unsigned int lra_calib_data;
	unsigned int f0_calib_data;
};

struct aw8624_container {
	int len;
	unsigned char data[];
};

/*********************************************************
*
* ioctl
*
********************************************************/
struct aw8624_seq_loop {
	unsigned char loop[AW8624_SEQUENCER_SIZE];
};

struct aw8624_que_seq {
	unsigned char index[AW8624_SEQUENCER_SIZE];
};

#define AW8624_HAPTIC_IOCTL_MAGIC		'h'
#define AW8624_HAPTIC_SET_QUE_SEQ	\
		_IOWR(AW8624_HAPTIC_IOCTL_MAGIC, 1, struct aw8624_que_seq*)
#define AW8624_HAPTIC_SET_SEQ_LOOP	\
		_IOWR(AW8624_HAPTIC_IOCTL_MAGIC, 2, struct aw8624_seq_loop*)
#define AW8624_HAPTIC_PLAY_QUE_SEQ	\
		_IOWR(AW8624_HAPTIC_IOCTL_MAGIC, 3, unsigned int)
#define AW8624_HAPTIC_SET_BST_VOL	\
		_IOWR(AW8624_HAPTIC_IOCTL_MAGIC, 4, unsigned int)
#define AW8624_HAPTIC_SET_BST_PEAK_CUR	\
		_IOWR(AW8624_HAPTIC_IOCTL_MAGIC, 5, unsigned int)
#define AW8624_HAPTIC_SET_GAIN		\
		_IOWR(AW8624_HAPTIC_IOCTL_MAGIC, 6, unsigned int)
#define AW8624_HAPTIC_PLAY_REPEAT_SEQ	\
		_IOWR(AW8624_HAPTIC_IOCTL_MAGIC, 7, unsigned int)

extern int aw8624_parse_dt(struct device *dev, struct aw8624 *aw8624,
		struct device_node *np);
extern void aw8624_interrupt_setup(struct aw8624 *aw8624);
extern int aw8624_vibrator_init(struct aw8624 *aw8624);
extern int aw8624_haptic_init(struct aw8624 *aw8624);
extern int aw8624_haptic_trig1_config(struct aw8624 *aw8624,
				unsigned char flag);
extern int aw8624_ram_init(struct aw8624 *aw8624);
extern struct attribute_group aw8624_vibrator_attribute_group;
extern struct miscdevice aw8624_haptic_misc;
extern irqreturn_t aw8624_irq(int irq, void *data);

#endif



