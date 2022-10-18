#ifndef _AW8624_H_
#define _AW8624_H_

/*********************************************************
 *
 * kernel version
 *
 ********************************************************/
#define TEST_CONT_TO_RAM
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
#include <linux/leds.h>
#include <linux/atomic.h>
#include <linux/timekeeping.h>
#include "haptic.h"

/*********************************************************
 *
 * marco
 *
 ********************************************************/
#define MAX_I2C_BUFFER_SIZE                 65536

#define AW8624_SEQUENCER_SIZE               8
#define AW8624_SEQUENCER_LOOP_SIZE          4

#define AW8624_RTP_I2C_SINGLE_MAX_NUM       512

#define HAPTIC_MAX_TIMEOUT                  10000

#define AW8624_VBAT_REFER                   4400
#define AW8624_VBAT_MIN                     3500
#define AW8624_VBAT_MAX                     4500


#define AW8624_CONT_PLAYBACK_MODE       AW8624_BIT_CONT_CTRL_CLOSE_PLAYBACK

/*
 * trig default high level
 * ___________         _________________
 *           |         |
 *           |         |
 *           |___________|
 *        first edge
 *                   second edge
 *
 *
 * trig default low level
 *            ___________
 *        |           |
 *        |           |
 * __________|        |_________________
 *        first edge
 *                   second edge
 */
/* trig config */
/*dts config
* default_level -> 1: high level; 0: low level
* dual_edge     -> 1: dual edge; 0: first edge
*vib_trig_config = <
*       1   1              1          1           2
*  enable   default_level  dual_edge  first_seq   second_seq
*       1   1              2          1           2
*  enable   default_level  dual_edge  first_seq   second_seq
*       1   1              3          1           2
*  enable   default_level  dual_edge  first_seq   second_seq
*/
#define AW8624_TRIG_NUM                     3


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

enum aw8624_haptic_work_mode {
	AW8624_HAPTIC_STANDBY_MODE = 0,
	AW8624_HAPTIC_RAM_MODE = 1,
	AW8624_HAPTIC_RTP_MODE = 2,
	AW8624_HAPTIC_TRIG_MODE = 3,
	AW8624_HAPTIC_CONT_MODE = 4,
	AW8624_HAPTIC_RAM_LOOP_MODE = 5,
};

enum aw8624_haptic_activate_mode {
	AW8624_HAPTIC_ACTIVATE_RAM_MODE = 0,
	AW8624_HAPTIC_ACTIVATE_CONT_MODE = 1,
	AW8624_HAPTIC_ACTIVATE_RTP_MODE = 2,
	AW8624_HAPTIC_ACTIVATE_RAM_LOOP_MODE = 3,
};

enum aw8624_haptic_cont_vbat_comp_mode {
	AW8624_HAPTIC_CONT_VBAT_SW_COMP_MODE = 0,
	AW8624_HAPTIC_CONT_VBAT_HW_COMP_MODE = 1,
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

enum aw8624_haptic_play {
	AW8624_HAPTIC_PLAY_NULL = 0,
	AW8624_HAPTIC_PLAY_ENABLE = 1,
	AW8624_HAPTIC_PLAY_STOP = 2,
	AW8624_HAPTIC_PLAY_GAIN = 8,
};

enum aw8624_haptic_cmd {
	AW8624_HAPTIC_CMD_NULL = 0,
	AW8624_HAPTIC_CMD_ENABLE = 1,
	AW8624_HAPTIC_CMD_STOP = 255,
};

enum aw8624_haptic_strength {
	AW8624_LIGHT_MAGNITUDE = 0x3fff,
	AW8624_MEDIUM_MAGNITUDE = 0x5fff,
	AW8624_STRONG_MAGNITUDE = 0x7fff,
};

enum aw8624_haptic_cali_lra {
	NORMAL_CALI = 0,
	F0_CALI = 1,
	OSC_CALI = 2,
};

/*********************************************************
 *
 * struct
 *
 ********************************************************/
/*struct ram {
	unsigned int len;
	unsigned int check_sum;
	unsigned int base_addr;
	unsigned char version;
	unsigned char ram_shift;
	unsigned char baseaddr_shift;
};

struct haptic_ctr {
	unsigned char cmd;
	unsigned char play;
	unsigned char wavseq;
	unsigned char loop;
	unsigned char gain;
};

struct haptic_audio {
	struct mutex lock;
	struct hrtimer timer;
	struct work_struct work;
	int delay_val;
	int timer_val;
	unsigned char cnt;
	struct haptic_ctr data[256];
	struct haptic_ctr ctr;
	unsigned char ori_gain;
};*/
struct aw8624_dts_info {
	unsigned int mode;
	unsigned int f0_pre;
	unsigned int f0_cali_percen;
	unsigned int cont_drv_lvl;
	unsigned int cont_drv_lvl_ov;
	unsigned int cont_td;
	unsigned int cont_zc_thr;
	unsigned int cont_num_brk;
	unsigned int f0_coeff;
	unsigned int f0_trace_parameter[4];
	unsigned int bemf_config[4];
	unsigned int sw_brake;
	unsigned int tset;
	unsigned int r_spare;
	unsigned int parameter1;
	unsigned int gain_flag;
	unsigned int effect_id_boundary;
	unsigned int effect_max;
	unsigned int rtp_time[194];
	unsigned int trig_config[3][5];
};

struct aw8624_tirg {
	unsigned char enable;
	unsigned char default_level;
	unsigned char dual_edge;
	unsigned char frist_seq;
	unsigned char second_seq;

};

struct aw8624 {
	struct i2c_client *i2c;
	struct mutex lock;

	struct work_struct vibrator_work;
	struct work_struct rtp_work;
	struct work_struct set_gain_work;
	struct delayed_work ram_work;
	struct delayed_work stop_work;

	struct fileops fileops;
	struct ram ram;

	struct timespec64 start, end;
	unsigned int timeval_flags;
	unsigned int osc_cali_flag;
	unsigned long int microsecond;
	unsigned int sys_frequency;
	unsigned int rtp_len;
	unsigned int lra_calib_data;
	unsigned int f0_calib_data;

	int reset_gpio;
	int irq_gpio;

	unsigned char hwen_flag;
	unsigned char flags;
	unsigned char chipid;
	unsigned char chipid_flag;

	unsigned char play_mode;

	unsigned char activate_mode;

	unsigned char auto_boost;
	unsigned char wk_lock_flag;
	unsigned char irq_sts_flag;

	int state;
	int duration;
	int amplitude;
	int index;
	int vmax;
	int gain;
	u16 new_gain;
	int f0_value;
	unsigned char level;

	unsigned char seq[AW8624_SEQUENCER_SIZE];
	unsigned char loop[AW8624_SEQUENCER_SIZE];

	unsigned int rtp_cnt;
	unsigned int rtp_file_num;

	unsigned char rtp_init;
	unsigned char ram_init;

	unsigned int f0;
	unsigned int cont_f0;
	unsigned char max_pos_beme;
	unsigned char max_neg_beme;
	unsigned char f0_cali_flag;
	unsigned int osc_cali_run;

	unsigned char ram_vbat_comp;
	unsigned int vbat;
	unsigned int lra;

	struct aw8624_tirg trig[AW8624_TRIG_NUM];

	struct haptic_audio haptic_audio;
	struct aw8624_dts_info info;
	atomic_t is_in_rtp_loop;
	atomic_t exit_in_rtp_loop;
	wait_queue_head_t wait_q;//wait queue for exit irq mode
	wait_queue_head_t stop_wait_q;  //wait queue for stop rtp mode
	struct workqueue_struct *work_queue;

#ifdef INPUT_DEV
	struct platform_device *pdev;
	struct device *dev;
	struct regmap *regmap;
	struct input_dev *input_dev;
	struct pwm_device *pwm_dev;
	struct qti_hap_config config;
	struct qti_hap_play_info play;
	struct qti_hap_effect *predefined;
	struct qti_hap_effect constant;
	struct regulator *vdd_supply;
	struct hrtimer stop_timer;
	struct hrtimer hap_disable_timer;
	struct hrtimer timer;	/*test used  ,del */
	struct mutex rtp_lock;
	spinlock_t bus_lock;
	ktime_t last_sc_time;
	int play_irq;
	int sc_irq;
	int effects_count;
	int sc_det_count;
	u16 reg_base;
	bool perm_disable;
	bool play_irq_en;
	bool vdd_enabled;
	int effect_type;
	int effect_id;
	int test_val;
#endif
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

#define AW8624_HAPTIC_IOCTL_MAGIC         'h'

#define AW8624_HAPTIC_SET_QUE_SEQ         _IOWR(AW8624_HAPTIC_IOCTL_MAGIC,\
						1,\
						struct aw8624_que_seq*)
#define AW8624_HAPTIC_SET_SEQ_LOOP        _IOWR(AW8624_HAPTIC_IOCTL_MAGIC,\
						2,\
						struct aw8624_seq_loop*)
#define AW8624_HAPTIC_PLAY_QUE_SEQ        _IOWR(AW8624_HAPTIC_IOCTL_MAGIC,\
						3,\
						unsigned int)
#define AW8624_HAPTIC_SET_GAIN            _IOWR(AW8624_HAPTIC_IOCTL_MAGIC,\
						6,\
						unsigned int)
#define AW8624_HAPTIC_PLAY_REPEAT_SEQ     _IOWR(AW8624_HAPTIC_IOCTL_MAGIC,\
						7,\
						unsigned int)

/*********************************************************
 *
 * extern
 *
 ********************************************************/
extern int aw8624_haptics_upload_effect(struct input_dev *dev,
					struct ff_effect *effect,
					struct ff_effect *old);
extern int aw8624_haptics_playback(struct input_dev *dev, int effect_id,
				   int val);
extern int aw8624_haptics_erase(struct input_dev *dev, int effect_id);
extern void aw8624_haptics_set_gain_work_routine(struct work_struct *work);
extern void aw8624_interrupt_setup(struct aw8624 *aw8624);
extern int aw8624_parse_dt(struct device *dev, struct aw8624 *aw8624,
			   struct device_node *np);
extern int aw8624_vibrator_init(struct aw8624 *aw8624);
extern int aw8624_haptic_init(struct aw8624 *aw8624);
extern int aw8624_ram_init(struct aw8624 *aw8624);
extern void aw8624_haptics_set_gain(struct input_dev *dev, u16 gain);
extern irqreturn_t aw8624_irq(int irq, void *data);
extern struct attribute_group aw8624_vibrator_attribute_group;

#endif

