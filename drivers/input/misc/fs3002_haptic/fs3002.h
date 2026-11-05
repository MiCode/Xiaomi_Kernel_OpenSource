#ifndef _FS3002_H_
#define _FS3002_H_
/*********************************************************
 *
 * fs3002.h
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
#include <linux/proc_fs.h>

#include "fs_haptic.h"


#define FS3002_STATUS				0x00
#define FS3002_DEVID				0x01
#define FS3002_REVID				0x02
#define FS3002_ANASTAT				0x03
#define FS3002_DIGSTAT				0x04
#define FS3002_DRPSTAT				0x05
#define FS3002_F0CALB_L				0x06
#define FS3002_F0CALB_H				0x07
#define FS3002_F0TRK_L				0x08
#define FS3002_F0TRK_H				0x09
#define FS3002_BATS_L				0x0A
#define FS3002_BATS_H				0x0B
#define FS3002_CHIPINI				0x0E
#define FS3002_PWRCTRL				0x10
#define FS3002_SYSCTRL				0x11
#define FS3002_OPCTRL				0x12
#define FS3002_RAMACC				0x13
#define FS3002_F0SET_L				0x16
#define FS3002_F0SET_H				0x17
#define FS3002_STACFG				0x1A
#define FS3002_PRICFG				0x1B
#define FS3002_TRGCTRL				0x1C
#define FS3002_TRGCFG0				0x1D
#define FS3002_ACCKEY				0x1F
#define FS3002_BSTCTRL1				0x20
#define FS3002_BSTCTRL2				0x21
#define FS3002_ADPBST1				0x22
#define FS3002_ADPBST2				0x23
#define FS3002_BSTENV1				0x24
#define FS3002_BSTENV2				0x25
#define FS3002_BSTENV3				0x26
#define FS3002_BSTENV4				0x27
#define FS3002_PGACHOP				0x29
#define FS3002_PGACTRL				0x2A
#define FS3002_HDCTRL				0x2B
#define FS3002_RTPWDATA				0x30
#define FS3002_RTPCTRL				0x31
#define FS3002_RTPFIFOAE_L			0x32
#define FS3002_RTPFIFOAE_H			0x33
#define FS3002_RTPFIFOAF_L			0x34
#define FS3002_RTPFIFOAF_H			0x35
#define FS3002_RTPSTART_L			0x36
#define FS3002_RTPSTART_H			0x37
#define FS3002_RAMADDR_L			0x38
#define FS3002_RAMADDR_H			0x39
#define FS3002_RAMWDATA				0x3A
#define FS3002_RAMRDATA				0x3B
#define FS3002_WFSBASE_L			0x3C
#define FS3002_WFSBASE_H			0x3D
#define FS3002_RTPENV_L				0x3E
#define FS3002_RTPENV_H				0x3F
#define FS3002_WFSCFG1				0x40
#define FS3002_WFSCFG2				0x41
#define FS3002_WFSCFG3				0x42
#define FS3002_WFSCFG4				0x43
#define FS3002_WFSCFG5				0x44
#define FS3002_WFSCFG6				0x45
#define FS3002_WFSCFG7				0x46
#define FS3002_WFSCFG8				0x47
#define FS3002_WFSLOOP1				0x49
#define FS3002_WFSLOOP2				0x4A
#define FS3002_WFSLOOP3				0x4B
#define FS3002_WFSLOOP4				0x4C
#define FS3002_WFSMAINLOOP			0x4D
#define FS3002_TIMECTRL				0x4F
#define FS3002_BWAVCTRL				0x50
#define FS3002_BWAVCFG1				0x52
#define FS3002_BWAVCFG2				0x53
#define FS3002_BWAVCFG3				0x54
#define FS3002_BWAVCFG4				0x55
#define FS3002_BWAVCFG5				0x56
#define FS3002_BWAVCFG6				0x57
#define FS3002_ADPBST3				0x58
#define FS3002_ADPBST4				0x59
#define FS3002_DRVCFG1				0x5A
#define FS3002_DRVCFG2				0x5B
#define FS3002_DRVCFG3				0x5C
#define FS3002_DRVCFG4				0x5D
#define FS3002_GAINCFG				0x5F
#define FS3002_CLAMPCTRL			0x60
#define FS3002_TRGCFG1				0x61
#define FS3002_TRGCFG2				0x62
#define FS3002_TRGCFG3				0x63
#define FS3002_BRKCTRL				0x64
#define FS3002_BRKCFG1				0x65
#define FS3002_BRKCFG2				0x66
#define FS3002_BRKCFG3				0x67
#define FS3002_BRKCFG4				0x68
#define FS3002_BRKCFG5				0x69
#define FS3002_BSLOPES1_L			0x6A
#define FS3002_BSLOPES1_H			0x6B
#define FS3002_BSLOPES2_L			0x6C
#define FS3002_BSLOPES2_H			0x6D
#define FS3002_VBATLOWCTRL1			0x6E
#define FS3002_PWMCTRL				0x70
#define FS3002_VCOMPCTRL			0x71
#define FS3002_VCOMPCFG				0x72
#define FS3002_TRGCFG4				0x73
#define FS3002_TRGCFG5				0x74
#define FS3002_TRGCFG6				0x75
#define FS3002_TRGCFG7				0x76
#define FS3002_TRGCFG8				0x77
#define FS3002_FDETCTRL				0x78
#define FS3002_FDETCFG1				0x79
#define FS3002_FDETCFG2				0x7A 
#define FS3002_FTUNECFG				0x7B
#define FS3002_FTUNEDS				0x7C
#define FS3002_BEMFNUM1				0x7E
#define FS3002_BEMFNUM2				0x7F
#define FS3002_BEMFDCTRL			0x80
#define FS3002_BEMFDCFG1			0x81
#define FS3002_BEMFDCFG2			0x82
#define FS3002_BEMFDCFG3			0x83
#define FS3002_BEMFDCFG4			0x84
#define FS3002_BEMFDCFG9			0x89
#define FS3002_BEMFDCFG10			0x8A
#define FS3002_BEMFDCFG11			0x8B
#define FS3002_BEMFDCFG14			0x8E
#define FS3002_TRKCFG2				0x92
#define FS3002_TRKCFG3				0x93
#define FS3002_TRKCFG4				0x94
#define FS3002_TRKCFG5				0x95
#define FS3002_TRKCFG6				0x96
#define FS3002_TRKCFG7				0x97
#define FS3002_TRGCFG9				0x98
#define FS3002_TRGCFG10				0x99
#define FS3002_TRGCFG11				0x9A
#define FS3002_TRGCFG12				0x9B
#define FS3002_BSLOPES3_L			0xA1
#define FS3002_BSLOPES3_H			0xA2
#define FS3002_BSLOPES4_L			0xA3
#define FS3002_BSLOPES4_H			0xA4
#define FS3002_BSLOPES5_L			0xA5
#define FS3002_BSLOPES5_H			0xA6
#define FS3002_BSLOPES6_L			0xA7
#define FS3002_BSLOPES6_H			0xA8
#define FS3002_INTCTRL				0xA9
#define FS3002_INTMASK1				0xAA
#define FS3002_INTMASK2				0xAB
#define FS3002_INTSTAT1				0xAC
#define FS3002_INTSTAT2				0xAD
#define FS3002_INTSTATR1			0xAE
#define FS3002_INTSTATR2			0xAF
#define FS3002_DIAGLRA_L			0xB2
#define FS3002_DIAGLRA_H			0xB3
#define FS3002_SWDIAG1				0xB5
#define FS3002_DCTHDCFG				0xB9
#define FS3002_DCCTRL				0xBA
#define FS3002_SARCTRL				0xBB
#define FS3002_SARTS_L				0xBC
#define FS3002_SARTS_H				0xBD
#define FS3002_TRGCFG13				0xBE
#define FS3002_TRGCFG14				0xBF
#define FS3002_ANACTRL				0xC0
#define FS3002_HDTEST				0xC4
#define FS3002_PWMTEST				0xC6
#define FS3002_PWMTDATA				0xC7
#define FS3002_ANACFG1				0xC8
#define FS3002_BISTCTL1				0xD1
#define FS3002_BISTSTAT1			0xD2
#define FS3002_BISTSTAT2			0xD3
#define FS3002_BISTSTAT3			0xD4
#define FS3002_BISTSTAT4			0xD5
#define FS3002_OTPCMD				0xDC
#define FS3002_OTPADDR				0xDD
#define FS3002_OTPPG0W0B0			0xE0
#define FS3002_OTPPG0W0B1			0xE1
#define FS3002_OTPPG0W1B2			0xE2
#define FS3002_OTPPG0W1B3			0xE3
#define FS3002_OTPPG0W2B4			0xE4
#define FS3002_OTPPG0W2B5			0xE5
#define FS3002_OTPPG0W3B6			0xE6
#define FS3002_OTPPG0W3B7			0xE7



#define FS3002_DEFAULT_F0_REF					(1700)
#define FS3002_DEFAULT_CONT_DRV1_LVL			(0x7F)
#define FS3002_DEFAULT_CONT_DRV2_LVL			(0x50)
#define FS3002_DEFAULT_CONT_DRV1_TIME			(0x04)
#define FS3002_DEFAULT_CONT_DRV2_TIME			(0x06)
#define FS3002_DEFAULT_CONT_1_PERIOD			(0x3F0)
#define FS3002_DEFAULT_BRK_SLOPETH				(0x60)
#define FS3002_DEFAULT_BRK_GAIN					(0x80)
#define FS3002_DEFAULT_BRK_TIMES				(0x0A)
#define FS3002_DEFAULT_BRK_NOISE_GATE			(0x10)
#define FS3002_DEFAULT_BRK_1_PERIOD				(0x300)
#define FS3002_DEFAULT_BRK_COEFP				(1)
#define FS3002_DEFAULT_BRK_PGAGAIN				(0x05)
#define FS3002_DEFAULT_BRK_MARGIN				(0x10)
#define FS3002_DEFAULT_PLAY_RAM_SRATE			(0x00)
#define FS3002_DEFAULT_PLAY_RTP_SRATE			(0x00)
#define FS3002_DEFAULT_VIB_MODE					(0x03)		//0:ram, 1:cont, 2:rtp, 3: ram_loop
#define FS3002_DEFAULT_LR_PGAGAIN				(0x05)
#define FS3002_DEFAULT_REG_INITS				(0xffffff)
#define FS3002_DEFAULT_RTP_AUTO_ENV				(0x01)
#define FS3002_DEFAULT_RTP_AUTO_ENV_MID			(0x40)
#define FS3002_DEFAULT_RTP_AUTO_ENV_HIGH		(0x70)
#define FS3002_DEFAULT_RTP_AUTO_SIZE			(0)
#define FS3002_DEFAULT_RAM_ID_BOUNDARY			(0x00)
#define FS3002_DEFAULT_RTP_MAX					(197)
#define FS3002_DEFAULT_RTP_TIME					(20)
#define FS3002_DEFAULT_TRIG_CONFIG				(0)
#define FS3002_DEFAULT_ENV_LOWV					(29)
#define FS3002_DEFAULT_LOWV_REF					(6)
#define FS3002_DEFAULT_MIDV_REF					(0)
#define FS3002_DEFAULT_HIGHV_REF				(0x23)
#define FS3002_DEFAULT_RTP_BST_SEL				(1)
#define FS3002_DEFAULT_TRIG_REF					(0x21)
#define FS3002_BYPASS_SYSTEM_GAIN_DEFAULT		(0x00)
#define FS3002_HAPSTREAM_FRE_GAP_DEFAULT		(15)
#define FS3002_HAPSTREAM_TRANS_CYCLES_DEFAULT	(4)
#define FS3002_F0_REF_DIFF_DEFAULT				(15)
#define FS3002_DEFAULT_ILMT						(3)




#define FS3002_BASE_FRE						(384000)
#define FS3002_FULL_AMP						(0x7f)
#define FS3002_FULL_GAIN					(0x80)
#define FS3002_BASE_LOWV					(29)
#define FS3002_BASE_MIDV					(65)
#define FS3002_BASE_HIGHV					(60)

#define FS3002_OFFSET_RETRIES				(1)
#define FS3002_SYSCTRL_B1_0_OPMODE_RTP				(0x01)
#define FS3002_DIGSTAT_B7_4_OPS_OFF					(0x00)
#define FS3002_DIGSTAT_B7_4_OPS_GO					(0x02)

enum fs3002_auto_brake_mode 
{
	FS3002_AUTO_BRAKE_DISABLE = 0,
	FS3002_AUTO_BRAKE_ENABLE = 1,
};


enum fs3002_f0_cali_mode 
{
	FS3002_F0_CALI_MODE_AUTO = 0,
	FS3002_F0_CALI_MODE_FORMULA = 1,
};

enum fs3002_f0_cali_data_mode
{
	FS3002_F0_CALI_DATA_SELF_MODE = 0,
	FS3002_F0_CALI_DATA_CMDLINE_MODE = 1,
	FS3002_F0_CALI_DATA_DTS_MODE = 2,
};


/*********************************************************
 *
 * marco
 *
 ********************************************************/

//Marco
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 4, 1)
#define TIMED_OUTPUT
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 1)
#define FS_KERNEL_VER_OVER_4_19
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 1)
#define FS_KERNEL_VER_OVER_5_10
#endif


#ifdef TIMED_OUTPUT
#include <../../../drivers/staging/android/timed_output.h>
typedef struct timed_output_dev cdev_t;
#else
typedef struct led_classdev cdev_t;
#endif



#define FS3002_SEQUENCER_SIZE			(8)
#define FS3002_SEQUENCER_LOOP_SIZE		(8)
#define FS3002_VBAT_REFER			(4150)
#define FS3002_VBAT_MIN			(2000)
#define FS3002_VBAT_MAX			(5000)
#define FS3002_TRIG_NUM			(3)
#define FS3002_MAX_BST_VO			(0x7f)
#define FS3002_REG_MAX				(0xe7)
#define FS3002_MAX_BST_VOL			(0x7f)	/* bst_vol-> 7 bit */
#define	FS3002_SYSINT_ERROR			(1 << 0)
#define	FS3002_SYSINT_FF_AEI			(1 << 1)
#define	FS3002_SYSINT_FF_AFI			(1 << 2)
#define FS3002_OSC_CALI_MAX_LENGTH	(11000000)
#define FS3002_WFSLOOP_INIFINITELY					(0x0F)


#define FS3002_INTSTAT2_MASK_B1_AE					(1<<0)
#define FS3002_INTSTAT2_MASK_B1_AF					(1<<1)

enum fs3002_flags 
{
	FS3002_FLAG_NONR = 0,
	FS3002_FLAG_SKIP_INTERRUPTS = 1,
};

enum fs3002_haptic_cali_lra 
{
	FS3002_WRITE_ZERO = 0,
	FS3002_F0_CALI = 1,
	FS3002_OSC_CALI = 2,
};

enum fs3002_haptic_bst_mode 
{
	FS3002_BST_MODE_BYPASS = 0,
	FS3002_BST_MODE = 1,
};

enum fs3002_haptic_activate_mode 
{
	FS3002_HAPTIC_ACTIVATE_RAM_MODE = 0,
	FS3002_HAPTIC_ACTIVATE_CONT_MODE = 1,
    FS3002_HAPTIC_ACTIVATE_RTP_MODE = 2,
	FS3002_HAPTIC_ACTIVATE_RAM_LOOP_MODE = 3,
};


enum fs3002_haptic_cmd 
{
	FS3002_HAPTIC_CMD_NULL = 0,
	FS3002_HAPTIC_CMD_ENABLE = 1,
	FS3002_HAPTIC_CMD_STOP = 255,
};

enum fs3002_haptic_play 
{
	FS3002_HAPTIC_PLAY_NULL = 0,
	FS3002_HAPTIC_PLAY_ENABLE = 1,
	FS3002_HAPTIC_PLAY_STOP = 2,
	FS3002_HAPTIC_PLAY_GAIN = 8,
};

enum fs3002_haptic_work_mode 
{
	FS3002_HAPTIC_STANDBY_MODE = 0,
	FS3002_HAPTIC_RAM_MODE = 1,
	FS3002_HAPTIC_RTP_MODE = 2,
	FS3002_HAPTIC_TRIG_MODE = 3,
	FS3002_HAPTIC_CONT_MODE = 4,
	FS3002_HAPTIC_RAM_LOOP_MODE = 5,
	FS3002_HAPTIC_F0_DETECT_MODE = 6,
	FS3002_HAPTIC_F0_CALI_MODE = 7,	
};

enum fs3002_haptic_strength 
{
	FS3002_LIGHT_MAGNITUDE = 0x3fff,
	FS3002_MEDIUM_MAGNITUDE = 0x5fff,
	FS3002_STRONG_MAGNITUDE = 0x7fff,
};

enum fs3002_haptic_pwm_mode 
{
	FS3002_PWM_48K = 0,
	FS3002_PWM_24K = 1,
	FS3002_PWM_12K = 2,
};



struct fs3002_dts_info 
{
	unsigned int fs3002_f0_ref;
	unsigned int fs3002_auto_brake;
	unsigned int fs3002_f0_cali_mode;
	unsigned int fs3002_cont_drv1_lvl;
	unsigned int fs3002_cont_drv2_lvl;
	unsigned int fs3002_cont_drv1_time;
	unsigned int fs3002_cont_drv2_time;
	unsigned int fs3002_cont_1_period;
	unsigned int fs3002_brk_slopeth;
	unsigned int fs3002_brk_gain;
	unsigned int fs3002_brk_times;
	unsigned int fs3002_brk_noise_gate;
	unsigned int fs3002_brk_1_period;
	unsigned int fs3002_brk_coefp;
	unsigned int fs3002_brk_pgagain;
	unsigned int fs3002_brk_margin;
	unsigned int fs3002_play_ram_srate;
	unsigned int fs3002_play_rtp_srate;
	unsigned int fs3002_default_vib_mode;
	unsigned int fs3002_lr_pgagain;
	unsigned int fs3002_reg_inits[10];
	unsigned int fs3002_rtp_auto_env;
	unsigned int fs3002_rtp_auto_size;
	unsigned int fs3002_rtp_auto_env_mid;
	unsigned int fs3002_rtp_auto_env_high;
	unsigned int fs3002_ram_id_boundary;
	unsigned int fs3002_rtp_max;
	unsigned int fs3002_rtp_time[175];
	unsigned int fs3002_trig_config[24];
	
	unsigned int fs3002_env_lowv;
	unsigned int fs3002_lowv_ref;
	unsigned int fs3002_midv_ref;
	unsigned int fs3002_highv_ref;
	unsigned int fs3002_trig1_p_ref;
	unsigned int fs3002_trig1_n_ref;
	unsigned int fs3002_trig2_p_ref;
	unsigned int fs3002_trig2_n_ref;
	unsigned int fs3002_trig3_p_ref;
	unsigned int fs3002_trig3_n_ref;	
	unsigned int fs3002_lk_f0_cali;
	unsigned int fs3002_f0_cali_data_mode;
	unsigned int bstcfg[6];
	unsigned int prctmode[3];
	unsigned int sine_array[4];
	unsigned int fs3002_bypass_system_gain;
	unsigned int fs3002_hapstream_fre_gap;
	unsigned int fs3002_hapstream_trans_cycles;
	unsigned int fs3002_f0_min;
	unsigned int fs3002_f0_max;
	unsigned int fs3002_rtp_bst_sel;
	unsigned int fs3002_ilmt;
};

struct fs3002_trig 
{
	unsigned char trig_level;
	unsigned char trig_polar;
	unsigned char pos_enable;
	unsigned char pos_sequence;
	unsigned char neg_enable;
	unsigned char neg_sequence;
	unsigned char trig_brk;
	unsigned char trig_bst;
};

struct fs3002 
{
	struct i2c_client *i2c;
	struct mutex lock;
	struct mutex rtp_lock;
	struct work_struct vibrate_work;
	struct work_struct rtp_work;
	struct work_struct set_gain_work;
	struct delayed_work ram_work;

	struct fileops fileops;
	struct ram ram;
	struct fs3002_container *rtp_container;

	//cdev_t vib_dev;
	

	ktime_t kstart;
	ktime_t kend;

	struct timespec64 start, end;
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

	unsigned char play_mode;
	unsigned char bst_mode;

	unsigned char activate_mode;

	unsigned char auto_boost;

	int state;
	int duration;
	int amplitude;
	int index;
	int vmax;
	int gain;
	u16 new_gain;
	unsigned char level;

	unsigned char seq[FS3002_SEQUENCER_SIZE];
	unsigned char loop[FS3002_SEQUENCER_SIZE];

	unsigned int rtp_cnt;
	int rtp_file_num;
	unsigned int rtp_num_max;

	unsigned char rtp_init;
	unsigned char ram_init;
	unsigned char rtp_routine_on;

	unsigned int f0;
	unsigned int cont_f0;
	unsigned char max_pos_beme;
	unsigned char max_neg_beme;
	unsigned char f0_cali_flag;
	bool f0_cali_status;
	unsigned int osc_cali_run;

	unsigned int vbat;
	unsigned int lra;
	unsigned int nv_flag;

	struct fs3002_trig trig[FS3002_TRIG_NUM];

	struct haptic_audio haptic_audio;
	struct fs3002_dts_info dts_info;
	atomic_t is_in_rtp_loop;
	atomic_t exit_in_rtp_loop;
	atomic_t is_in_write_loop;
	wait_queue_head_t wait_q; /*wait queue for exit irq mode */
	wait_queue_head_t stop_wait_q; /* wait queue for stop rtp mode */
	struct workqueue_struct *work_queue;

#ifdef INPUT_DEV
	struct platform_device *pdev;
	struct device *dev;
	struct regmap *regmap;
	struct input_dev *input_dev;
	struct pwm_device *pwm_dev;
	struct qti_hap_config config;
	struct qti_hap_effect *predefined;
	struct qti_hap_play_info play;
	struct regulator *vdd_supply;
	struct hrtimer timer;	/*test used  ,del */
	struct dentry *hap_debugfs;
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
	unsigned char fs3002_debug_enable;
	unsigned int osc_cali_data;
	unsigned int f0_cali_data;
	unsigned int offset;
	unsigned int buf_size;
	unsigned int Qos_time;
	unsigned int lra_test_sleep_time;
#ifdef FS_HAPSTREAM
	struct work_struct rtp_hapstream;
	struct work_struct rtp_irq_hapstream;
	struct fs3002_container *hapstream_rtp;
	struct proc_dir_entry *fs_config_proc;
	struct mmap_buf_format *start_buf;
#endif
	bool hapstream_stop_flag;
	bool vib_stop_flag;
};

struct fs3002_container 
{
	int len;
	unsigned char data[];
};



extern int fs3002_haptics_upload_effect(struct input_dev *dev, struct ff_effect *effect, struct ff_effect *old);
extern int fs3002_haptics_playback(struct input_dev *dev, int effect_id, int val);
extern int fs3002_haptics_erase(struct input_dev *dev, int effect_id);
extern void fs3002_haptic_ff_set_gain(struct input_dev *dev, u16 gain);
extern void fs3002_haptic_ff_set_gain_work_routine(struct work_struct *work);
extern void fs3002_interrupt_setup(struct fs3002 *fs3002);
extern int fs3002_vibrator_init(struct fs3002 *fs3002);
extern void fs3002_reg_init(struct fs3002 *fs3002);
extern int fs3002_haptic_init(struct fs3002 *fs3002);
extern int fs3002_ram_init(struct fs3002 *fs3002);
extern irqreturn_t fs3002_irq(int irq, void *data);
extern struct attribute_group fs3002_vibrator_attribute_group;
extern int fs3002_parse_dt(struct fs3002 *fs3002, struct device *dev, struct device_node *np);
extern int fs3002_check_qualify(struct fs3002 *fs3002);
extern void fs3002_f0_cali_setting_init(struct fs3002 *fs3002);

#endif


