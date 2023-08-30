/*********************************************************
 *

 * aw8622x.h
 *
 ********************************************************/
#ifndef _AW8622X_H_
#define _AW8622X_H_


#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include <sound/control.h>
#include <sound/soc.h>
#include "haptic.h"
#include <linux/timekeeping.h>
/*********************************************************
 *
 * Marco
 *
 ********************************************************/
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 4, 1)
#define TIMED_OUTPUT
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 1)
#define AW_KERNEL_VER_OVER_4_19
#endif

#ifdef TIMED_OUTPUT
#include <../../../drivers/staging/android/timed_output.h>
typedef struct timed_output_dev cdev_t;
#else
typedef struct led_classdev cdev_t;
#endif

#define AW8622X_I2C_RETRIES		(5)
#define AW8622X_RTP_NAME_MAX		(64)
#define AW8622X_SEQUENCER_SIZE		(8)
#define AW8622X_SEQUENCER_LOOP_SIZE	(4)
#define AW8622X_OSC_CALI_MAX_LENGTH	(11000000)
#define AW8622X_PM_QOS_VALUE_VB		(0)
#define AW8622X_VBAT_REFER		(4200)
#define AW8622X_VBAT_MIN		(3000)
#define AW8622X_VBAT_MAX		(5500)
#define AW8622X_TRIG_NUM		(3)
#define AW8622X_I2C_RETRY_DELAY		(2)


// static int wf_s_repeat[4] = { 1, 2, 4, 8 };

enum aw8622x_flags {
	AW8622X_FLAG_NONR = 0,
	AW8622X_FLAG_SKIP_INTERRUPTS = 1,
};

enum aw8622x_haptic_work_mode {
	AW8622X_HAPTIC_STANDBY_MODE = 0,
	AW8622X_HAPTIC_RAM_MODE = 1,
	AW8622X_HAPTIC_RTP_MODE = 2,
	AW8622X_HAPTIC_TRIG_MODE = 3,
	AW8622X_HAPTIC_CONT_MODE = 4,
	AW8622X_HAPTIC_RAM_LOOP_MODE = 5,
};

enum aw8622x_haptic_activate_mode {
	AW8622X_HAPTIC_ACTIVATE_RAM_MODE = 0,
	AW8622X_HAPTIC_ACTIVATE_CONT_MODE = 1,
    AW8622X_HAPTIC_ACTIVATE_RTP_MODE = 2,
	AW8622X_HAPTIC_ACTIVATE_RAM_LOOP_MODE = 3,
};

enum aw8622x_haptic_cont_vbat_comp_mode {
	AW8622X_HAPTIC_CONT_VBAT_SW_ADJUST_MODE = 0,
	AW8622X_HAPTIC_CONT_VBAT_HW_ADJUST_MODE = 1,
};

enum aw8622x_haptic_ram_vbat_compensate_mode {
	AW8622X_HAPTIC_RAM_VBAT_COMP_DISABLE = 0,
	AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE = 1,
};

enum aw8622x_haptic_f0_flag {
	AW8622X_HAPTIC_LRA_F0 = 0,
	AW8622X_HAPTIC_CALI_F0 = 1,
};

enum aw8622x_sram_size_flag {
	AW8622X_HAPTIC_SRAM_1K = 0,
	AW8622X_HAPTIC_SRAM_2K = 1,
	AW8622X_HAPTIC_SRAM_3K = 2,
};

enum aw8622x_haptic_pwm_mode {
	AW8622X_PWM_48K = 0,
	AW8622X_PWM_24K = 1,
	AW8622X_PWM_12K = 2,
};

enum aw8622x_haptic_play {
	AW8622X_HAPTIC_PLAY_NULL = 0,
	AW8622X_HAPTIC_PLAY_ENABLE = 1,
	AW8622X_HAPTIC_PLAY_STOP = 2,
	AW8622X_HAPTIC_PLAY_GAIN = 8,
};

enum aw8622x_haptic_cmd {
	AW8622X_HAPTIC_CMD_NULL = 0,
	AW8622X_HAPTIC_CMD_ENABLE = 1,
	AW8622X_HAPTIC_CMD_HAPTIC = 0x0f,
	AW8622X_HAPTIC_CMD_TP = 0x10,
	AW8622X_HAPTIC_CMD_SYS = 0xf0,
	AW8622X_HAPTIC_CMD_STOP = 255,
};

enum aw8622x_haptic_cali_lra {
	AW8622X_WRITE_ZERO = 0,
	AW8622X_F0_CALI = 1,
	AW8622X_OSC_CALI = 2,
};

enum aw8622x_haptic_rtp_mode {
	AW8622X_RTP_SHORT = 4,
	AW8622X_RTP_LONG = 5,
	AW8622X_RTP_SEGMENT = 6,
};

enum aw8622x_ef_id {
	AW86223_EF_ID = 0x01,
	AW86224_5_EF_ID = 0x00,
};



/*********************************************************
 *
 * Struct Define
 *
 ********************************************************/


/* trig_config
 * trig default high level
 * ___________           ___________
 *           |           |
 *           |           |
 *           |___________|
 *        first edge
 *                   second edge
 *
 * trig default low level
 *            ___________
 *           |           |
 *           |           |
 * __________|           |__________
 *        first edge
 *                   second edge
 ******************** vib_trig_config *********************
 *     level polar pos_en pos_seq neg_en neg_seq brk bst
 trig1*  1     0     1       1       1      2     0   0
 trig2*  1     0     0       1       0      2     0   0
 trig3*  1     0     0       1       0      2     0   0
*/


struct aw8622x_dts_info {
	unsigned int mode;
	unsigned int f0_ref;
	unsigned int f0_cali_percent;
	unsigned int cont_drv1_lvl_dt;
	unsigned int cont_drv2_lvl_dt;
	unsigned int cont_drv1_time_dt;
	unsigned int cont_drv2_time_dt;
	unsigned int cont_wait_num_dt;
	unsigned int cont_brk_time_dt;
	unsigned int cont_track_margin;
	unsigned int cont_tset;
	unsigned int cont_drv_width;
	unsigned int cont_bemf_set;
	unsigned int cont_brk_gain;
	unsigned int d2s_gain;
	unsigned int prctmode[3];
	unsigned int sine_array[4];
	unsigned int trig_config[24];
	unsigned int effect_id_boundary;
	unsigned int effect_max;
	unsigned int rtp_time[194];
	bool is_enabled_powerup_f0_cali;
	bool is_enabled_auto_bst;
};

struct aw8622x_trig {
	unsigned char trig_level;
	unsigned char trig_polar;
	unsigned char pos_enable;
	unsigned char pos_sequence;
	unsigned char neg_enable;
	unsigned char neg_sequence;
	unsigned char trig_brk;
};

struct aw8622x {
	struct regmap *regmap;
	struct i2c_client *i2c;

	/*struct snd_soc_codec *codec; */
	struct device *dev;
	struct input_dev *input;
	struct mutex lock;
	struct mutex rtp_lock;
	struct hrtimer timer;
	struct work_struct long_vibrate_work;
	struct work_struct rtp_work;
	struct work_struct set_gain_work;
	struct delayed_work ram_work;
	struct delayed_work stop_work;
	struct aw8622x_trig trig[AW8622X_TRIG_NUM];
	struct aw8622x_dts_info dts_info;
	struct fileops fileops;
	struct ram ram;
	struct timespec64 start, end;
	struct aw8622x_container *rtp_container;

	cdev_t vib_dev;

	bool haptic_ready;

	unsigned char seq[AW8622X_SEQUENCER_SIZE];
	unsigned char loop[AW8622X_SEQUENCER_SIZE];
	unsigned char rtp_init;
	unsigned char ram_init;
	unsigned char rtp_routine_on;
	unsigned char max_pos_beme;
	unsigned char max_neg_beme;
	unsigned char f0_cali_flag;
	unsigned char ram_vbat_compensate;
	unsigned char hwen_flag;
	unsigned char flags;
	unsigned char chipid;
	unsigned char play_mode;
	unsigned char activate_mode;
	unsigned char ram_state;

	unsigned char wk_lock_flag;

	bool isUsedIntn;

	int name;
	int reset_gpio;
	int irq_gpio;
	int state;
	int duration;
	int amplitude;
	int index;
	int vmax;
	int gain;
	int sysclk;
	int rate;
	int width;
	int pstream;
	int cstream;

	unsigned int gun_type;
	unsigned int bullet_nr;
	unsigned int rtp_cnt;
	unsigned int rtp_file_num;
	unsigned int f0;
	unsigned int cont_f0;
	unsigned int cont_drv1_lvl;
	unsigned int cont_drv2_lvl;
	unsigned int cont_brk_time;
	unsigned int cont_wait_num;
	unsigned int cont_drv1_time;
	unsigned int cont_drv2_time;
	unsigned int theory_time;
	unsigned int vbat;
	unsigned int lra;
	unsigned int ram_update_flag;
	unsigned int rtp_update_flag;
	unsigned int osc_cali_data;
	unsigned int f0_cali_data;
	unsigned int timeval_flags;
	unsigned int osc_cali_flag;
	unsigned int sys_frequency;
	unsigned int rtp_len;
	unsigned long int microsecond;

	u16 new_gain;
	unsigned char level;
	struct haptic_audio haptic_audio;
	unsigned int osc_cali_run;
	unsigned char ram_vbat_comp;
	atomic_t is_in_rtp_loop;
	atomic_t exit_in_rtp_loop;
	wait_queue_head_t wait_q;
	wait_queue_head_t stop_wait_q;
	struct workqueue_struct *work_queue;
	struct work_struct vibrator_work;
	/* ram monitor */
#ifdef AW_RAM_STATE_OUTPUT
	struct delayed_work ram_monitor_work;
#endif
#ifdef INPUT_DEV
	struct platform_device *pdev;
	struct input_dev *input_dev;
	struct pwm_device *pwm_dev;
	struct qti_hap_config config;
	struct qti_hap_play_info play;
	struct qti_hap_effect *predefined;
	struct qti_hap_effect constant;
	struct regulator *vdd_supply;
	struct hrtimer stop_timer;
	struct hrtimer hap_disable_timer;
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

struct aw8622x_container {
	int len;
	unsigned char data[];
};
/*********************************************************
 *
 * extern
 *
 ********************************************************/
extern char aw8622x_check_qualify(struct aw8622x *aw8622x);

extern int aw8622x_parse_dt(struct device *dev, struct aw8622x *aw8622x,
			    struct device_node *np);
extern void aw8622x_interrupt_setup(struct aw8622x *aw8622x);
extern int aw8622x_vibrator_init(struct aw8622x *aw8622x);
extern int aw8622x_haptic_init(struct aw8622x *aw8622x);
extern int aw8622x_ram_work_init(struct aw8622x *aw8622x);
extern irqreturn_t aw8622x_irq(int irq, void *data);
extern struct attribute_group aw8622x_vibrator_attribute_group;
extern void aw8622x_haptics_set_gain(struct input_dev *dev, u16 gain);
extern int aw8622x_haptics_erase(struct input_dev *dev, int effect_id);
extern int aw8622x_haptics_playback(struct input_dev *dev, int effect_id,
			     int val);
extern int aw8622x_haptics_upload_effect (struct input_dev *dev,
				   struct ff_effect *effect,
				   struct ff_effect *old);
extern void aw8622x_haptics_set_gain_work_routine(struct work_struct *work);


#endif
