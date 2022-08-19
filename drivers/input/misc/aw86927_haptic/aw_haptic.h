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
#define aw_err(format, ...) \
			pr_err("[haptic_hv]" format, ##__VA_ARGS__)

#define aw_info(format, ...) \
			pr_info("[haptic_hv]" format, ##__VA_ARGS__)

#define aw_dbg(format, ...) \
			pr_debug("[haptic_hv]" format, ##__VA_ARGS__)

/*********************************************************
*
* normal macro
*
********************************************************/
#define AW_I2C_NAME			"awinic_haptic"
#define AW_HAPTIC_NAME			"awinic_haptic"

#define AW8695_CHIP_ID			(0x95)
#define AW8697_CHIP_ID			(0x97)
#define AW86907_CHIP_ID			(0x04)
#define AW86927_CHIP_ID			(0x9270)
#define AW_REG_IDH			(0x57)
#define AW_REG_IDL			(0x58)
#define AW_VBAT_REFER			(4200)
#define AW_VBAT_MIN			(3000)
#define AW_VBAT_MAX			(4500)
#define AW_READ_CHIPID_RETRIES		(5)
#define AW_I2C_RETRIES			(2)
#define AW_I2C_RETRY_DELAY		(2)
#define AW_RAMDATA_WR_BUFFER_SIZE	(2048)
#define AW_RAMDATA_RD_BUFFER_SIZE	(1024)
#define AW_PROTECT_EN			(0X01)
#define AW_PROTECT_OFF			(0X00)
#define AW_PROTECT_VAL			(0X00)
#define AWINIC_RTP_NAME_MAX		(64)
#define PM_QOS_VALUE_VB			(400)
#define OSC_CALIBRATION_T_LENGTH	(5100000)

#define REG_NONE_ACCESS			(0)
#define REG_RD_ACCESS			(1 << 0)
#define REG_WR_ACCESS			(1 << 1)

#define FF_EFFECT_COUNT_MAX		(32)
#define HAP_BRAKE_PATTERN_MAX		(4)
#define HAP_WAVEFORM_BUFFER_MAX		(8)
#define HAP_PLAY_RATE_US_DEFAULT	(5715)
#define HAP_PLAY_RATE_US_MAX		(20475)
/*********************************************************
*
* macro control
*
********************************************************/
#define INPUT_DEV
#define DEBUG
/* #define TEST_RTP */
#define AW_RAM_UPDATE_DELAY
#define ENABLE_PIN_CONTROL
#define AW_CHECK_RAM_DATA
#define AW_CHECK_QUAL
/*********************************************************
*
* enum
*
********************************************************/

enum awinic_chip_name {
	AW_NULL = 0,
	AW8697 = 1,
	AW86927 = 2,
	AW86907 = 3,
	AW8695 = 4,
};

enum haptics_custom_effect_param {
	CUSTOM_DATA_EFFECT_IDX,
	CUSTOM_DATA_TIMEOUT_SEC_IDX,
	CUSTOM_DATA_TIMEOUT_MSEC_IDX,
	CUSTOM_DATA_LEN,
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

	int reset_gpio;
	int irq_gpio;
	int reset_gpio_ret;
	int irq_gpio_ret;
	int enable_pin_control;

	struct aw869x *aw869x;
	struct aw86927 *aw86927;
	struct aw86907 *aw86907;
#ifdef ENABLE_PIN_CONTROL
	struct pinctrl *awinic_pinctrl;
	struct pinctrl_state *pinctrl_state[3];
#endif
};


struct ram {
	unsigned char version;
	unsigned char ram_shift;
	unsigned char baseaddr_shift;
	unsigned int len;
	unsigned int check_sum;
	unsigned int base_addr;
	unsigned int ram_num;
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


/*********************************************************
*
* extern
*
********************************************************/
extern int CUSTOME_WAVE_ID;
extern char awinic_rtp_name[][AWINIC_RTP_NAME_MAX];
extern int awinic_rtp_name_len;
extern char *awinic_ram_name;
extern int aw_i2c_read(struct awinic *, unsigned char, unsigned char *);
extern int aw_i2c_write(struct awinic *, unsigned char, unsigned char);

#endif
