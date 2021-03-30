#ifndef _AW8697_H_
#define _AW8697_H_

/*********************************************************
 *
 * kernel version
 *
 ********************************************************/
#define INPUT_DEV
//#define TEST_RTP
#define TEST_CONT_TO_RAM
/*********************************************************
 *
 * aw8697.h
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


/*********************************************************
 *
 * marco
 *
 ********************************************************/
#define AW8697_CHIPID                   0x97

#define MAX_I2C_BUFFER_SIZE                 65536

#define AW8697_SEQUENCER_SIZE               8
#define AW8697_SEQUENCER_LOOP_SIZE          4

#define AW8697_RTP_I2C_SINGLE_MAX_NUM       512

#define HAPTIC_MAX_TIMEOUT                  10000

#define AW8697_VBAT_REFER                   4200
#define AW8697_VBAT_MIN                     3000
#define AW8697_VBAT_MAX                     4500
#define ENABLE_PIN_CONTROL

#ifdef INPUT_DEV
/* common definitions */
#define HAP_BRAKE_PATTERN_MAX       4
#define HAP_WAVEFORM_BUFFER_MAX     8	/*used */
#define HAP_VMAX_MV_DEFAULT     1800
#define HAP_VMAX_MV_MAX         3596
#define HAP_PLAY_RATE_US_DEFAULT    5715	/*used */
#define HAP_PLAY_RATE_US_MAX        20475
#define HAP_PLAY_RATE_US_LSB        5
#define VMAX_MIN_PLAY_TIME_US       20000
#define HAP_SC_DET_MAX_COUNT        5
#define HAP_SC_DET_TIME_US      1000000
#define FF_EFFECT_COUNT_MAX     32
#define HAP_DISABLE_DELAY_USEC      1000
#endif
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
#define AW8697_TRIG_NUM                     3


enum aw8697_flags {
	AW8697_FLAG_NONR = 0,
	AW8697_FLAG_SKIP_INTERRUPTS = 1,
};

enum aw8697_haptic_read_write {
	AW8697_HAPTIC_CMD_READ_REG = 0,
	AW8697_HAPTIC_CMD_WRITE_REG = 1,
};

enum aw8697_haptic_work_mode {
	AW8697_HAPTIC_STANDBY_MODE = 0,
	AW8697_HAPTIC_RAM_MODE = 1,
	AW8697_HAPTIC_RTP_MODE = 2,
	AW8697_HAPTIC_TRIG_MODE = 3,
	AW8697_HAPTIC_CONT_MODE = 4,
	AW8697_HAPTIC_RAM_LOOP_MODE = 5,
};

enum aw8697_haptic_bst_mode {
	AW8697_HAPTIC_BYPASS_MODE = 0,
	AW8697_HAPTIC_BOOST_MODE = 1,
};

enum aw8697_haptic_activate_mode {
	AW8697_HAPTIC_ACTIVATE_RAM_MODE = 0,
	AW8697_HAPTIC_ACTIVATE_CONT_MODE = 1,
	AW8697_HAPTIC_ACTIVATE_RTP_MODE = 2,
	AW8697_HAPTIC_ACTIVATE_RAM_LOOP_MODE = 3,
};

enum aw8697_haptic_cont_vbat_comp_mode {
	AW8697_HAPTIC_CONT_VBAT_SW_COMP_MODE = 0,
	AW8697_HAPTIC_CONT_VBAT_HW_COMP_MODE = 1,
};

enum aw8697_haptic_ram_vbat_comp_mode {
	AW8697_HAPTIC_RAM_VBAT_COMP_DISABLE = 0,
	AW8697_HAPTIC_RAM_VBAT_COMP_ENABLE = 1,
};

enum aw8697_haptic_f0_flag {
	AW8697_HAPTIC_LRA_F0 = 0,
	AW8697_HAPTIC_CALI_F0 = 1,
};

enum aw8697_haptic_pwm_mode {
	AW8697_PWM_48K = 0,
	AW8697_PWM_24K = 1,
	AW8697_PWM_12K = 2,
};

enum aw8697_haptic_play {
	AW8697_HAPTIC_PLAY_NULL = 0,
	AW8697_HAPTIC_PLAY_ENABLE = 1,
	AW8697_HAPTIC_PLAY_STOP = 2,
	AW8697_HAPTIC_PLAY_GAIN = 8,
};

enum aw8697_haptic_cmd {
	AW8697_HAPTIC_CMD_NULL = 0,
	AW8697_HAPTIC_CMD_ENABLE = 1,
	AW8697_HAPTIC_CMD_STOP = 255,
};

enum haptics_custom_effect_param {
	CUSTOM_DATA_EFFECT_IDX,
	CUSTOM_DATA_TIMEOUT_SEC_IDX,
	CUSTOM_DATA_TIMEOUT_MSEC_IDX,
	CUSTOM_DATA_LEN,
};
enum aw8697_haptic_strength {
	AW8697_LIGHT_MAGNITUDE = 0x3fff,
	AW8697_MEDIUM_MAGNITUDE = 0x5fff,
	AW8697_STRONG_MAGNITUDE = 0x7fff,
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
};

struct trig {
	unsigned char enable;
	unsigned char default_level;
	unsigned char dual_edge;
	unsigned char frist_seq;
	unsigned char second_seq;
};

struct aw8697_dts_info {
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
	unsigned int bstdbg[6];
	unsigned int parameter1;
	unsigned int effect_id_boundary;
	unsigned int effect_max;
	unsigned int rtp_time[175];
	unsigned int trig_config[3][5];
	unsigned int bst_vol_default;
	unsigned int bst_vol_ram;
	unsigned int bst_vol_rtp;
};

#ifdef INPUT_DEV
enum actutor_type {
	ACT_LRA,
	ACT_ERM,
};

enum lra_res_sig_shape {
	RES_SIG_SINE,
	RES_SIG_SQUARE,
};

enum lra_auto_res_mode {
	AUTO_RES_MODE_ZXD,
	AUTO_RES_MODE_QWD,
};

enum wf_src {
	INT_WF_VMAX,
	INT_WF_BUFFER,
	EXT_WF_AUDIO,
	EXT_WF_PWM,
};

struct qti_hap_effect {
	int id;
	u8 *pattern;
	int pattern_length;
	u16 play_rate_us;
	u16 vmax_mv;
	u8 wf_repeat_n;
	u8 wf_s_repeat_n;
	u8 brake[HAP_BRAKE_PATTERN_MAX];
	int brake_pattern_length;
	bool brake_en;
	bool lra_auto_res_disable;
};

struct qti_hap_play_info {
	struct qti_hap_effect *effect;
	u16 vmax_mv;
	int length_us;
	int playing_pos;
	bool playing_pattern;
};

struct qti_hap_config {
	enum actutor_type act_type;
	enum lra_res_sig_shape lra_shape;
	enum lra_auto_res_mode lra_auto_res_mode;
	enum wf_src ext_src;
	u16 vmax_mv;
	u16 play_rate_us;
	bool lra_allow_variable_play_rate;
	bool use_ext_wf_src;
};
#endif

#ifdef ENABLE_PIN_CONTROL
const char * const pctl_names[] = {
	"aw8697_reset_reset",
	"aw8697_reset_active",
	"aw8697_interrupt_active",
};
#endif
struct aw8697 {
	struct i2c_client *i2c;
	struct mutex lock;
#ifdef ENABLE_PIN_CONTROL
	struct pinctrl *aw8697_pinctrl;
	struct pinctrl_state *pinctrl_state[ARRAY_SIZE(pctl_names)];
#endif
	int enable_pin_control;
	struct work_struct vibrator_work;
	struct work_struct rtp_work;
	struct work_struct set_gain_work;
	struct delayed_work ram_work;

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

	unsigned char play_mode;

	unsigned char activate_mode;

	unsigned char auto_boost;
	unsigned char wk_lock_flag;

	int state;
	int duration;
	int amplitude;
	int index;
	int vmax;
	int gain;
	u16 new_gain;
	unsigned char level;

	unsigned char seq[AW8697_SEQUENCER_SIZE];
	unsigned char loop[AW8697_SEQUENCER_SIZE];

	unsigned int rtp_cnt;
	unsigned int rtp_file_num;

	unsigned char rtp_init;
	unsigned char ram_init;

	unsigned int f0;
	unsigned int cont_f0;
	unsigned char max_pos_beme;
	unsigned char max_neg_beme;
	unsigned char f0_cali_flag;
	bool f0_cali_status;
	unsigned int osc_cali_run;

	unsigned char ram_vbat_comp;
	unsigned int vbat;
	unsigned int lra;

	struct trig trig[AW8697_TRIG_NUM];

	struct haptic_audio haptic_audio;
	struct aw8697_dts_info info;
	atomic_t is_in_rtp_loop;
	atomic_t exit_in_rtp_loop;
	atomic_t is_in_write_loop;
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
	struct dentry *hap_debugfs;
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
	int is_custom_wave;
#endif
};

struct aw8697_container {
	int len;
	unsigned char data[];
};

/*********************************************************
 *
 * ioctl
 *
 ********************************************************/
struct aw8697_seq_loop {
	unsigned char loop[AW8697_SEQUENCER_SIZE];
};

struct aw8697_que_seq {
	unsigned char index[AW8697_SEQUENCER_SIZE];
};

#define AW8697_HAPTIC_IOCTL_MAGIC         'h'

#define AW8697_HAPTIC_SET_QUE_SEQ         _IOWR(AW8697_HAPTIC_IOCTL_MAGIC,\
						1,\
						struct aw8697_que_seq*)
#define AW8697_HAPTIC_SET_SEQ_LOOP        _IOWR(AW8697_HAPTIC_IOCTL_MAGIC,\
						2,\
						struct aw8697_seq_loop*)
#define AW8697_HAPTIC_PLAY_QUE_SEQ        _IOWR(AW8697_HAPTIC_IOCTL_MAGIC,\
						3,\
						unsigned int)
#define AW8697_HAPTIC_SET_BST_VOL         _IOWR(AW8697_HAPTIC_IOCTL_MAGIC,\
						4,\
						unsigned int)
#define AW8697_HAPTIC_SET_BST_PEAK_CUR    _IOWR(AW8697_HAPTIC_IOCTL_MAGIC,\
						5,\
						unsigned int)
#define AW8697_HAPTIC_SET_GAIN            _IOWR(AW8697_HAPTIC_IOCTL_MAGIC,\
						6,\
						unsigned int)
#define AW8697_HAPTIC_PLAY_REPEAT_SEQ     _IOWR(AW8697_HAPTIC_IOCTL_MAGIC,\
						7,\
						unsigned int)

#endif
