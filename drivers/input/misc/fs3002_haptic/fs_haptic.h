#ifndef _FS_HAPTIC_H_
#define _FS_HAPTIC_H_
#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/syscalls.h>
#include <linux/input.h>
#include <linux/version.h>

#if defined(pr_fmt) 
	#undef pr_fmt
	#define pr_fmt(fmt) KBUILD_MODNAME ":%s:%d: " fmt, __func__, __LINE__
#else
	#define pr_fmt(fmt) KBUILD_MODNAME ":%s:%d: " fmt, __func__, __LINE__
#endif

#define FOURSEMI_DRIVER_VERSION				("106.1.3.0")
#define FS_I2C_NAME							"fs_haptic"
#define FS_HAPTIC_NAME						"fs_haptic"
#define FS_READ_CHIPID_RETRIES				(5)
#define FS_I2C_RETRIES						(2)
#define FS_I2C_RETRY_DELAY					(2)
#define FS3002_BWAVCTRL_B1_0_SQUARE			(0x02)
#define FS3002_GLB_STATE_STANDBY			(0)
#define FS3002_RAM_NUM_MAX					(32)


#define HAPSTREAM_MMAP_BUF_SIZE				1000
#define HAPSTREAM_MMAP_PAGE_ORDER			2
#define HAPSTREAM_MMAP_BUF_SUM				16
#define FS_HAPSTREAM
#define FS_HAPSTREAM_NAME "tiktap_buf"

#define HAPSTREAM_IOCTL_GROUP		0xFF
#define HAPSTREAM_GET_F0			_IO(HAPSTREAM_IOCTL_GROUP, 0x01)
#define HAPSTREAM_GET_HWINFO		_IO(HAPSTREAM_IOCTL_GROUP, 0x02)
#define HAPSTREAM_SET_FREQ			_IO(HAPSTREAM_IOCTL_GROUP, 0x03)
#define HAPSTREAM_SETTING_GAIN		_IO(HAPSTREAM_IOCTL_GROUP, 0x04)
#define HAPSTREAM_SETTING_SPEED		_IO(HAPSTREAM_IOCTL_GROUP, 0x05)
#define HAPSTREAM_SETTING_BSTVOL	_IO(HAPSTREAM_IOCTL_GROUP, 0x06)
#define HAPSTREAM_ON_MODE			_IO(HAPSTREAM_IOCTL_GROUP, 0x07)
#define HAPSTREAM_OFF_MODE			_IO(HAPSTREAM_IOCTL_GROUP, 0x08)
#define HAPSTREAM_RTP_MODE			_IO(HAPSTREAM_IOCTL_GROUP, 0x09)
#define HAPSTREAM_RTP_IRQ_MODE		_IO(HAPSTREAM_IOCTL_GROUP, 0x0A)
#define HAPSTREAM_STOP_MODE			_IO(HAPSTREAM_IOCTL_GROUP, 0x0B)
#define HAPSTREAM_STOP_RTP_MODE		_IO(HAPSTREAM_IOCTL_GROUP, 0x0C)
#define HAPSTREAM_WRITE_REG			_IO(HAPSTREAM_IOCTL_GROUP, 0x0D)
#define HAPSTREAM_READ_REG			_IO(HAPSTREAM_IOCTL_GROUP, 0x0E)
#define HAPSTREAM_BST_SWITCH		_IO(HAPSTREAM_IOCTL_GROUP, 0x0F)
#define HAPSTREAM_GET_SPEED			_IO(HAPSTREAM_IOCTL_GROUP, 0x10)
#define HAPSTREAM_GET_FRE_GAP		_IO(HAPSTREAM_IOCTL_GROUP, 0x11)
#define HAPSTREAM_GET_TRANS_CYCLES	_IO(HAPSTREAM_IOCTL_GROUP, 0x12)

#define KERNEL_VERSION_HIGHER(a, b, c) \
	(KERNEL_VERSION((a), (b), (c)) < LINUX_VERSION_CODE)

enum {
	MMAP_BUF_DATA_VALID = 0x55,
	MMAP_BUF_DATA_FINISHED = 0xAA,
	MMAP_BUF_DATA_INVALID = 0xFF,
};

#pragma pack(4)
struct mmap_buf_format {
	uint8_t status;
	uint8_t bit;
	int16_t length;

	struct mmap_buf_format *kernel_next;
	struct mmap_buf_format *user_next;
	uint8_t reg_addr;
	int8_t data[HAPSTREAM_MMAP_BUF_SIZE];
};
#pragma pack()





















#define FS_RAMDATA_WR_BUFFER_SIZE	(2048)
#define FS_RAMDATA_RD_BUFFER_SIZE	(1024)
#define FS_PROTECT_EN			(0X01)
#define FS_PROTECT_OFF			(0X00)
#define FS_PROTECT_VAL			(0X00)
#define FS3002_RTP_NAME_MAX		(64)
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
#define FS_RAM_UPDATE_DELAY
//#define ENABLE_PIN_CONTROL		zzzz evk is not needed
//#define FS_CHECK_RAM_DATA			zzzz

/*********************************************************
*
* enum
*
********************************************************/

enum foursemi_chip_name 
{
	FS3002_A1 = 0xA1,
	FS3002_A2 = 0xA2,
	FS3002_A3 = 0xA3,
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

/*foursemi*/
struct foursemi {
	struct i2c_client *i2c;
	struct device *dev;
	unsigned char name;
	bool IsUsedIRQ;
	struct mutex reset_mutex;
	spinlock_t reset_lock;

	int reset_gpio;
	int irq_gpio;
	int reset_gpio_ret;
	int irq_gpio_ret;
	int enable_pin_control;

	struct fs3002 *fs3002;
#ifdef ENABLE_PIN_CONTROL
	struct pinctrl *foursemi_pinctrl;
	struct pinctrl_state *pinctrl_state[3];
#endif
};


struct ram 
{
	unsigned char version;
	unsigned char ram_shift;
	unsigned char baseaddr_shift;
	unsigned int len;
	unsigned int check_sum;
	unsigned int base_addr;
	unsigned int ram_num;
	unsigned char ram_max[FS3002_RAM_NUM_MAX];
	bool b_over_max_num;
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
extern char *foursemi_ram_bin_name[1];
extern int foursemi_sw_reset(struct foursemi *foursemi);

#endif
