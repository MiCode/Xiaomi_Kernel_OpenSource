#ifndef _AW_HAPTIC_H_
#define _AW_HAPTIC_H_
#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/syscalls.h>
#include <linux/input.h>

/********************************************
 * print information control
 *******************************************/
#define aw_dev_err(dev, format, ...) \
			pr_err("[%s]" format, dev_name(dev), ##__VA_ARGS__)

#define aw_dev_info(dev, format, ...) \
			pr_info("[%s]" format, dev_name(dev), ##__VA_ARGS__)

#define aw_dev_dbg(dev, format, ...) \
			pr_debug("[%s]" format, dev_name(dev), ##__VA_ARGS__)

/*********************************************************
*
* normal macro
*
********************************************************/
#define AWINIC_DEV_NAME	("awinic_vibrator")
#define FF_EFFECT_COUNT_MAX     32
#define HAP_BRAKE_PATTERN_MAX       4
#define HAP_WAVEFORM_BUFFER_MAX     8
#define HAP_PLAY_RATE_US_DEFAULT    5715
#define HAP_PLAY_RATE_US_MAX        20475
#define FF_EFFECT_COUNT_MAX     32


/*********************************************************
*
* macro control
*
********************************************************/

#define ENABLE_PIN_CONTROL
#define INPUT_DEV
#define AW_CHECK_RAM_DATA
#define AW_READ_BIN_FLEXBALLY
#define AW_OSC_COARSE_CALI

/*********************************************************
*
* enum
*
********************************************************/

enum awinic_chip_name {
	AW86223 = 0,
	AW86224_5 = 1,
	AW8624 = 2,
};

enum haptics_custom_effect_param {
	CUSTOM_DATA_EFFECT_IDX,
	CUSTOM_DATA_TIMEOUT_SEC_IDX,
	CUSTOM_DATA_TIMEOUT_MSEC_IDX,
	CUSTOM_DATA_LEN,
};

enum haptic_nv_read_chip_type {
	AW_FIRST_TRY = 0,
	AW_LAST_TRY = 1,
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
#endif

/*********************************************************
*
* struct
*
********************************************************/

#ifdef INPUT_DEV
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

struct fileops {
	unsigned char cmd;
	unsigned char reg;
	unsigned char ram_addrh;
	unsigned char ram_addrl;
};

/*awinic*/
struct awinic {
	struct i2c_client *i2c;
	struct device *dev;
	unsigned char name;
	bool IsUsedIRQ;

	unsigned int aw8622x_i2c_addr;
	int reset_gpio;
	int irq_gpio;
	int reset_gpio_ret;
	int irq_gpio_ret;
	int enable_pin_control;

	struct aw8624 *aw8624;
	struct aw8622x *aw8622x;
#ifdef ENABLE_PIN_CONTROL
	struct pinctrl *awinic_pinctrl;
	struct pinctrl_state *pinctrl_state[3];
#endif
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
	unsigned char cnt;
	struct haptic_ctr data[256];
	struct haptic_ctr ctr;
	struct list_head ctr_list;
	unsigned char ori_gain;
};

#endif
