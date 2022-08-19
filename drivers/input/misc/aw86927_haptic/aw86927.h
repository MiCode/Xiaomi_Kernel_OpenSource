#ifndef _AW86927_H_
#define _AW86927_H_
/*********************************************************
 *
 * aw86927.h
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
#include "aw_haptic.h"
/*********************************************************
 *
 * marco
 *
 ********************************************************/
#define AW86927_SEQUENCER_SIZE			(8)
#define AW86927_SEQUENCER_LOOP_SIZE		(8)
#define AW86927_TRIG_NUM			(3)
#define AW86927_MAX_BST_VO			(0x7f)
#define AW86927_REG_MAX				(0Xff)
#define AW86927_MAX_BST_VOL			(0x7f)	/* bst_vol-> 7 bit */
#define	AW86927_SYSINT_ERROR			(1 << 0)
#define	AW86927_SYSINT_FF_AEI			(1 << 1)
#define	AW86927_SYSINT_FF_AFI			(1 << 2)


enum aw86927_flags {
	AW86927_FLAG_NONR = 0,
	AW86927_FLAG_SKIP_INTERRUPTS = 1,
};

enum aw86927_haptic_cali_lra {
	AW86927_WRITE_ZERO = 0,
	AW86927_F0_CALI = 1,
	AW86927_OSC_CALI = 2,
};

enum aw86927_haptic_bst_mode {
	AW86927_BST_MODE_BYPASS = 0,
	AW86927_BST_MODE = 1,
};

enum aw86927_haptic_cont_vbat_comp_mode {
	AW86927_VBAT_SW_ADJUST_MODE = 0,
	AW86927_VBAT_HW_ADJUST_MODE = 1,
};
enum aw86927_haptic_activate_mode {
	AW86927_ACTIVATE_RAM_MODE = 0,
	AW86927_ACTIVATE_CONT_MODE = 1,
	AW86927_ACTIVATE_RTP_MODE = 2,
	AW86927_ACTIVATE_RAM_LOOP_MODE = 3,
};

enum aw86927_haptic_cmd {
	AW86927_HAPTIC_CMD_NULL = 0,
	AW86927_HAPTIC_CMD_ENABLE = 1,
	AW86927_HAPTIC_CMD_STOP = 255,
};

enum aw86927_haptic_play {
	AW86927_HAPTIC_PLAY_NULL = 0,
	AW86927_HAPTIC_PLAY_ENABLE = 1,
	AW86927_HAPTIC_PLAY_STOP = 2,
	AW86927_HAPTIC_PLAY_GAIN = 8,
};

enum aw86927_haptic_ram_vbat_compensate_mode {
	AW86927_RAM_VBAT_COMP_DISABLE = 0,
	AW86927_RAM_VBAT_COMP_ENABLE = 1,
};

enum aw86927_haptic_work_mode {
	AW86927_STANDBY_MODE = 0,
	AW86927_RAM_MODE = 1,
	AW86927_RTP_MODE = 2,
	AW86927_TRIG_MODE = 3,
	AW86927_CONT_MODE = 4,
	AW86927_RAM_LOOP_MODE = 5,
};

enum aw86927_haptic_strength {
	AW86927_LIGHT_MAGNITUDE = 0x3fff,
	AW86927_MEDIUM_MAGNITUDE = 0x5fff,
	AW86927_STRONG_MAGNITUDE = 0x7fff,
};

enum aw86927_haptic_pwm_mode {
	AW86927_PWM_48K = 0,
	AW86927_PWM_24K = 1,
	AW86927_PWM_12K = 2,
};



struct aw86927_dts_info {
	unsigned int mode;
	unsigned int f0_pre;
	unsigned int f0_cali_percen;
	unsigned int cont_drv1_lvl;
	unsigned int cont_drv2_lvl;
	unsigned int cont_drv1_time;
	unsigned int cont_drv2_time;
	unsigned int cont_wait_num;
	unsigned int cont_brk_time;
	unsigned int cont_track_margin;
	unsigned int cont_tset;
	unsigned int cont_drv_width;
	unsigned int cont_bemf_set;
	unsigned int cont_brk_gain;
	unsigned int cont_bst_brk_gain;
	unsigned int brk_bst_md;
	unsigned int d2s_gain;
	unsigned int bst_vol_default;
	unsigned int bst_vol_ram;
	unsigned int bst_vol_rtp;
	unsigned int bstcfg[6];
	unsigned int prctmode[3];
	unsigned int sine_array[4];
	unsigned int trig_config[24];
	unsigned int effect_id_boundary;
	unsigned int effect_max;
	unsigned int rtp_time[175];
};

struct aw86927_trig {
	unsigned char trig_level;
	unsigned char trig_polar;
	unsigned char pos_enable;
	unsigned char pos_sequence;
	unsigned char neg_enable;
	unsigned char neg_sequence;
	unsigned char trig_brk;
	unsigned char trig_bst;
};

struct aw86927 {
	struct i2c_client *i2c;
	struct mutex lock;
	struct mutex rtp_lock;
	struct work_struct vibrator_work;
	struct work_struct rtp_work;
	struct work_struct set_gain_work;
	struct delayed_work ram_work;

	struct fileops fileops;
	struct ram ram;

	ktime_t kstart;
	ktime_t kend;

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

	unsigned char seq[AW86927_SEQUENCER_SIZE];
	unsigned char loop[AW86927_SEQUENCER_SIZE];

	unsigned int rtp_cnt;
	unsigned int rtp_file_num;
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

	unsigned char ram_vbat_comp;
	unsigned int vbat;
	unsigned int lra;

	struct aw86927_trig trig[AW86927_TRIG_NUM];

	struct haptic_audio haptic_audio;
	struct aw86927_dts_info info;
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
	struct qti_hap_play_info play;
	struct qti_hap_effect *predefined;
	struct qti_hap_effect constant;
	struct regulator *vdd_supply;
	struct hrtimer stop_timer;
	struct hrtimer hap_disable_timer;
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
};

struct aw86927_container {
	int len;
	unsigned char data[];
};


/********************************************
 *  AW86927 Register List
 *******************************************/
#define AW86927_REG_RSTCFG		(0x00)
#define AW86927_REG_SYSST		(0x01)
#define AW86927_REG_SYSINT		(0x02)
#define AW86927_REG_SYSINTM		(0x03)
#define AW86927_REG_SYSST2		(0x04)
#define AW86927_REG_SYSER		(0x05)
#define AW86927_REG_PLAYCFG1		(0x06)
#define AW86927_REG_PLAYCFG2		(0x07)
#define AW86927_REG_PLAYCFG3		(0x08)
#define AW86927_REG_PLAYCFG4		(0x09)
#define AW86927_REG_WAVCFG1		(0x0A)
#define AW86927_REG_WAVCFG2		(0x0B)
#define AW86927_REG_WAVCFG3		(0x0C)
#define AW86927_REG_WAVCFG4		(0x0D)
#define AW86927_REG_WAVCFG5		(0x0E)
#define AW86927_REG_WAVCFG6		(0x0F)
#define AW86927_REG_WAVCFG7		(0x10)
#define AW86927_REG_WAVCFG8		(0x11)
#define AW86927_REG_WAVCFG9		(0x12)
#define AW86927_REG_WAVCFG10		(0x13)
#define AW86927_REG_WAVCFG11		(0x14)
#define AW86927_REG_WAVCFG12		(0x15)
#define AW86927_REG_WAVCFG13		(0x16)
#define AW86927_REG_CONTCFG1		(0x18)
#define AW86927_REG_CONTCFG2		(0x19)
#define AW86927_REG_CONTCFG3		(0x1A)
#define AW86927_REG_CONTCFG4		(0x1B)
#define AW86927_REG_CONTCFG5		(0x1C)
#define AW86927_REG_CONTCFG6		(0x1D)
#define AW86927_REG_CONTCFG7		(0x1E)
#define AW86927_REG_CONTCFG8		(0x1F)
#define AW86927_REG_CONTCFG9		(0x20)
#define AW86927_REG_CONTCFG10		(0x21)
#define AW86927_REG_CONTCFG11		(0x22)
#define AW86927_REG_CONTCFG12		(0x23)
#define AW86927_REG_CONTCFG13		(0x24)
#define AW86927_REG_CONTCFG14		(0x25)
#define AW86927_REG_CONTCFG15		(0x26)
#define AW86927_REG_CONTCFG16		(0x27)
#define AW86927_REG_CONTCFG17		(0x28)
#define AW86927_REG_CONTCFG18		(0x29)
#define AW86927_REG_CONTCFG19		(0x2A)
#define AW86927_REG_CONTCFG20		(0x2B)
#define AW86927_REG_CONTCFG21		(0x2C)
#define AW86927_REG_RTPCFG1		(0x2D)
#define AW86927_REG_RTPCFG2		(0x2E)
#define AW86927_REG_RTPCFG3		(0x2F)
#define AW86927_REG_RTPCFG4		(0x30)
#define AW86927_REG_RTPCFG5		(0x31)
#define AW86927_REG_RTPDATA		(0X32)
#define AW86927_REG_TRGCFG1		(0x33)
#define AW86927_REG_TRGCFG2		(0x34)
#define AW86927_REG_TRGCFG3		(0x35)
#define AW86927_REG_TRGCFG4		(0x36)
#define AW86927_REG_TRGCFG5		(0x37)
#define AW86927_REG_TRGCFG6		(0x38)
#define AW86927_REG_TRGCFG7		(0x39)
#define AW86927_REG_TRGCFG8		(0x3A)
#define AW86927_REG_GLBCFG1		(0x3B)
#define AW86927_REG_GLBCFG2		(0x3C)
#define AW86927_REG_GLBCFG3		(0x3D)
#define AW86927_REG_GLBCFG4		(0x3E)
#define AW86927_REG_GLBRD5		(0x3F)
#define AW86927_REG_RAMADDRH		(0x40)
#define AW86927_REG_RAMADDRL		(0x41)
#define AW86927_REG_RAMDATA		(0x42)
#define AW86927_REG_SYSCTRL1		(0x43)
#define AW86927_REG_SYSCTRL2		(0x44)
#define AW86927_REG_SYSCTRL3		(0x45)
#define AW86927_REG_SYSCTRL4		(0x46)
#define AW86927_REG_SYSCTRL5		(0x47)
#define AW86927_REG_PWMCFG1		(0x48)
#define AW86927_REG_PWMCFG2		(0x49)
#define AW86927_REG_PWMCFG3		(0x4A)
#define AW86927_REG_PWMCFG4		(0x4B)
#define AW86927_REG_VBATCTRL		(0x4C)
#define AW86927_REG_DETCFG1		(0x4D)
#define AW86927_REG_DETCFG2		(0x4E)
#define AW86927_REG_DETRD1		(0x4F)
#define AW86927_REG_DETRD2		(0x50)
#define AW86927_REG_DETRD3		(0x51)
#define AW86927_REG_TRIMCFG1		(0x52)
#define AW86927_REG_TRIMCFG2		(0x53)
#define AW86927_REG_TRIMCFG3		(0x54)
#define AW86927_REG_TRIMCFG4		(0x55)
#define AW86927_REG_IDH			(0x57)
#define AW86927_REG_IDL			(0x58)
#define AW86927_REG_TMCFG		(0x5B)
#define AW86927_REG_EFCFG1		(0x5C)
#define AW86927_REG_EFCFG6		(0x61)
#define AW86927_REG_EFCFG7		(0x62)
#define AW86927_REG_EFCFG8		(0x63)
#define AW86927_REG_TESTR		(0x65)
#define AW86927_REG_ANACFG1		(0x66)
#define AW86927_REG_ANACFG2		(0x67)
#define AW86927_REG_ANACFG7		(0x6C)
#define AW86927_REG_ANACFG11		(0x70)
#define AW86927_REG_ANACFG12		(0x71)
#define AW86927_REG_ANACFG13		(0x72)
#define AW86927_REG_ANACFG15		(0x74)
#define AW86927_REG_ANACFG16		(0x75)
#define AW86927_REG_ANACFG20		(0x79)
#define AW86927_REG_ANACFG22		(0x7C)


/******************************************************
 * AW86927 Register Detail
 *****************************************************/
/* SYSST: reg 0x01 RO */
#define AW86927_BIT_SYSST_BST_SCPS			(1<<7)
#define AW86927_BIT_SYSST_BST_OVPS			(1<<6)
#define AW86927_BIT_SYSST_UVLS				(1<<5)
#define AW86927_BIT_SYSST_FF_AES			(1<<4)
#define AW86927_BIT_SYSST_FF_AFS			(1<<3)
#define AW86927_BIT_SYSST_OCDS				(1<<2)
#define AW86927_BIT_SYSST_OTS				(1<<1)
#define AW86927_BIT_SYSST_DONES				(1<<0)
/* SYSINT: reg 0x02 RC */
#define AW86927_BIT_SYSINT_BST_SCPI			(1<<7)
#define AW86927_BIT_SYSINT_BST_OVPI			(1<<6)
#define AW86927_BIT_SYSINT_UVLI				(1<<5)
#define AW86927_BIT_SYSINT_FF_AEI			(1<<4)
#define AW86927_BIT_SYSINT_FF_AFI			(1<<3)
#define AW86927_BIT_SYSINT_OCDI				(1<<2)
#define AW86927_BIT_SYSINT_OTI				(1<<1)
#define AW86927_BIT_SYSINT_DONEI			(1<<0)

/* SYSINTM: reg 0x03 RW */
#define AW86927_BIT_SYSINTM_BST_SCPM_MASK		(~(1<<7))
#define AW86927_BIT_SYSINTM_BST_SCPM_OFF		(1<<7)
#define AW86927_BIT_SYSINTM_BST_SCPM_ON			(0<<7)
#define AW86927_BIT_SYSINTM_BST_OVPM_MASK		(~(1<<6))
#define AW86927_BIT_SYSINTM_BST_OVPM_OFF		(1<<6)
#define AW86927_BIT_SYSINTM_BST_OVPM_ON			(0<<6)
#define AW86927_BIT_SYSINTM_UVLM_MASK			(~(1<<5))
#define AW86927_BIT_SYSINTM_UVLM_OFF			(1<<5)
#define AW86927_BIT_SYSINTM_UVLM_ON			(0<<5)
#define AW86927_BIT_SYSINTM_FF_AEM_MASK			(~(1<<4))
#define AW86927_BIT_SYSINTM_FF_AEM_OFF			(1<<4)
#define AW86927_BIT_SYSINTM_FF_AEM_ON			(0<<4)
#define AW86927_BIT_SYSINTM_FF_AFM_MASK			(~(1<<3))
#define AW86927_BIT_SYSINTM_FF_AFM_OFF			(1<<3)
#define AW86927_BIT_SYSINTM_FF_AFM_ON			(0<<3)
#define AW86927_BIT_SYSINTM_OCDM_MASK			(~(1<<2))
#define AW86927_BIT_SYSINTM_OCDM_OFF			(1<<2)
#define AW86927_BIT_SYSINTM_OCDM_ON			(0<<2)
#define AW86927_BIT_SYSINTM_OTM_MASK			(~(1<<1))
#define AW86927_BIT_SYSINTM_OTM_OFF			(1<<1)
#define AW86927_BIT_SYSINTM_OTM_ON			(0<<1)
#define AW86927_BIT_SYSINTM_DONEM_MASK			(~(1<<0))
#define AW86927_BIT_SYSINTM_DONEM_OFF			(1<<0)
#define AW86927_BIT_SYSINTM_DONEM_ON			(0<<0)

/* SYSST2: reg 0x04 RO */
#define AW86927_BIT_SYSST2_BST_OK			(1<<4)
#define AW86927_BIT_SYSST2_VBG_OK			(1<<3)
#define AW86927_BIT_SYSST2_LDO_OK			(1<<2)
#define AW86927_BIT_SYSST2_FF_FULL			(1<<1)
#define AW86927_BIT_SYSST2_FF_EMPTY			(1<<0)

/* PLAYCFG1: reg 0x06 RW */
#define AW86927_BIT_PLAYCFG1_BST_MODE_MASK		(~(1<<7))
#define AW86927_BIT_PLAYCFG1_BST_MODE			(1<<7)
#define AW86927_BIT_PLAYCFG1_BST_MODE_BYPASS		(0<<7)
#define AW86927_BIT_PLAYCFG1_BST_VOUT_VREFSET_MASK	(~(0x7F<<0))
#define AW86927_BIT_PLAYCFG1_BST_VOUT_VREFSET		(0x7F)
#define AW86927_BIT_PLAYCFG1_BST_VOUT_10P5V		(0x70)
#define AW86927_BIT_PLAYCFG1_BST_VOUT_6V		(0x28)


/* PLAYCFG3: reg 0x08 RW */
#define AW86927_BIT_PLAYCFG3_ONEWIRE_COMP_MASK		(~(1<<5))
#define AW86927_BIT_PLAYCFG3_1908_ONEWIRE_MODE		(1<<5)
#define AW86927_BIT_PLAYCFG3_2102_ONEWIRE_MODE		(0<<5)
#define AW86927_BIT_PLAYCFG3_AUTO_BST_MASK		(~(1<<4))
#define AW86927_BIT_PLAYCFG3_AUTO_BST_ENABLE		(1<<4)
#define AW86927_BIT_PLAYCFG3_AUTO_BST_DISABLE		(0<<4)
#define AW86927_BIT_PLAYCFG3_STOP_MODE_MASK		(~(1<<3))
#define AW86927_BIT_PLAYCFG3_STOP_MODE_NOW		(1<<3)
#define AW86927_BIT_PLAYCFG3_STOP_MODE_LATER		(0<<3)
#define AW86927_BIT_PLAYCFG3_BRK_EN_MASK		(~(1<<2))
#define AW86927_BIT_PLAYCFG3_BRK_ENABLE			(1<<2)
#define AW86927_BIT_PLAYCFG3_BRK_DISABLE		(0<<2)
#define AW86927_BIT_PLAYCFG3_PLAY_MODE_MASK		(~(3<<0))
#define AW86927_BIT_PLAYCFG3_PLAY_MODE_STOP		(3<<0)
#define AW86927_BIT_PLAYCFG3_PLAY_MODE_CONT		(2<<0)
#define AW86927_BIT_PLAYCFG3_PLAY_MODE_RTP		(1<<0)
#define AW86927_BIT_PLAYCFG3_PLAY_MODE_RAM		(0<<0)

/* PLAYCFG4: reg 0x09 RW */
#define AW86927_BIT_PLAYCFG4_STOP_MASK			(~(1<<1))
#define AW86927_BIT_PLAYCFG4_STOP_ON			(1<<1)
#define AW86927_BIT_PLAYCFG4_STOP_OFF			(0<<1)
#define AW86927_BIT_PLAYCFG4_GO_MASK			(~(1<<0))
#define AW86927_BIT_PLAYCFG4_GO_ON			(1<<0)
#define AW86927_BIT_PLAYCFG4_GO_OFF			(0<<0)

/* WAVCFG9-12: reg 0x12 - reg 0x15 RW */
#define AW86927_BIT_WAVLOOP_SEQ_ODD_MASK		(~(0x0F<<4))
#define AW86927_BIT_WAVLOOP_SEQ_ODD_INIFINITELY		(0x0F<<4)
#define AW86927_BIT_WAVLOOP_SEQ_EVEN_MASK		(~(0x0F<<0))
#define AW86927_BIT_WAVLOOP_SEQ_EVEN_INIFINITELY	(0x0F<<0)
#define AW86927_BIT_WAVLOOP_INIFINITELY			(0x0F<<0)

/* WAVCFG9: reg 0x12 RW */
#define AW86927_BIT_WAVCFG9_SEQ1LOOP_MASK		(~(0x0F<<4))
#define AW86927_BIT_WAVCFG9_SEQ1LOOP_INIFINITELY	(0x0F<<4)
#define AW86927_BIT_WAVCFG9_SEQ2LOOP_MASK		(~(0x0F<<0))
#define AW86927_BIT_WAVCFG9_SEQ2LOOP_INIFINITELY	(0x0F<<0)

/***************** CONT *****************/
/* CONTCFG1: reg 0x18 RW */
#define AW86927_BIT_CONTCFG1_BRK_BST_MD_MASK		(~(1<<6))
#define AW86927_BIT_CONTCFG1_BRK_BST_MD_ENABLE		(1<<6)
#define AW86927_BIT_CONTCFG1_BRK_BST_MD_DISABLE		(0<<6)
#define AW86927_BIT_CONTCFG1_EN_F0_DET_MASK		(~(1<<5))
#define AW86927_BIT_CONTCFG1_F0_DET_ENABLE		(1<<5)
#define AW86927_BIT_CONTCFG1_F0_DET_DISABLE		(0<<5)
#define AW86927_BIT_CONTCFG1_SIN_MODE_MASK		(~(1<<1))
#define AW86927_BIT_CONTCFG1_SIN_MODE_COS		(1<<1)
#define AW86927_BIT_CONTCFG1_SIN_MODE_SINE		(0<<1)
#define AW86927_BIT_CONTCFG1_EDGE_FRE_MASK		(~(0x0F<<0))


/* CONTCFG5: reg 0x1C RW */
#define AW86927_BIT_CONTCFG5_BST_BRK_GAIN_MASK		(~(0x0F<<4))
#define AW86927_BIT_CONTCFG5_BRK_GAIN_MASK		(~(0x0F<<0))

/* CONTCFG6: reg 0x1D RW */
#define AW86927_BIT_CONTCFG6_TRACK_EN_MASK		(~(1<<7))
#define AW86927_BIT_CONTCFG6_TRACK_ENABLE		(1<<7)
#define AW86927_BIT_CONTCFG6_TRACK_DISABLE		(0<<7)
#define AW86927_BIT_CONTCFG6_DRV1_LVL_MASK		(~(0x7F<<0))

/* CONTCFG7: reg 0x1E RW */
#define AW86927_BIT_CONTCFG7_DRV2_LVL_MASK		(~(0x7F<<0))
/* CONTCFG13: reg 0x24 RW */
#define AW86927_BIT_CONTCFG13_TSET_MASK			(~(0x0F<<4))
#define AW86927_BIT_CONTCFG13_BEME_SET_MASK		(~(0x0F<<0))
/***************** RTP *****************/
/* RTPCFG1: reg 0x2D RW */
#define AW86927_BIT_RTPCFG1_TWORTP_EN_MASK		(~(1<<6))
#define AW86927_BIT_RTPCFG1_TWORTP_ENABLE		(1<<6)
#define AW86927_BIT_RTPCFG1_TWORTP_DISABLE		(0<<6)
#define AW86927_BIT_RTPCFG1_BASE_ADDR_H_MASK		(~(0x1F<<0))

/* RTPCFG3: reg 0x2F RW */
#define AW86927_BIT_RTPCFG3_FIFO_AEH_MASK		(~(0x0F<<4))
#define AW86927_BIT_RTPCFG3_FIFO_AFH_MASK		(~(0x0F<<0))

/***************** TRIGGER *****************/
#define AW86927_BIT_TRG_ENABLE_MASK			(~(1<<7))
#define AW86927_BIT_TRG_ENABLE				(1<<7)
#define AW86927_BIT_TRG_DISABLE				(0<<7)
#define AW86927_BIT_TRG_SEQ_MASK			(~(0x7F<<0))
/* TRGCFG1: reg 0x33 RW */
#define AW86927_BIT_TRGCFG1_TRG1_POS_MASK		(~(1<<7))
#define AW86927_BIT_TRGCFG1_TRG1_POS_ENABLE		(1<<7)
#define AW86927_BIT_TRGCFG1_TRG1_POS_DISABLE		(0<<7)
#define AW86927_BIT_TRGCFG1_TRG1SEQ_P_MASK		(~(0x7F<<0))

/* TRGCFG2: reg 0x34 RW */
#define AW86927_BIT_TRGCFG2_TRG2_POS_MASK		(~(1<<7))
#define AW86927_BIT_TRGCFG2_TRG2_POS_ENABLE		(1<<7)
#define AW86927_BIT_TRGCFG2_TRG2_POS_DISABLE		(0<<7)
#define AW86927_BIT_TRGCFG2_TRG2SEQ_P_MASK		(~(0x7F<<0))

/* TRGCFG3: reg 0x35 RW */
#define AW86927_BIT_TRGCFG3_TRG3_POS_MASK		(~(1<<7))
#define AW86927_BIT_TRGCFG3_TRG3_POS_ENABLE		(1<<7)
#define AW86927_BIT_TRGCFG3_TRG3_POS_DISABLE		(0<<7)
#define AW86927_BIT_TRGCFG3_TRG3SEQ_P_MASK		(~(0x7F<<0))

/* TRGCFG4: reg 0x36 RW */
#define AW86927_BIT_TRGCFG4_TRG1_NEG_MASK		(~(1<<7))
#define AW86927_BIT_TRGCFG4_TRG1_NEG_ENABLE		(1<<7)
#define AW86927_BIT_TRGCFG4_TRG1_NEG_DISABLE		(0<<7)
#define AW86927_BIT_TRGCFG4_TRG1SEQ_N_MASK		(~(0x7F<<0))

/* TRGCFG5: reg 0x37 RW */
#define AW86927_BIT_TRGCFG5_TRG2_NEG_MASK		(~(1<<7))
#define AW86927_BIT_TRGCFG5_TRG2_NEG_ENABLE		(1<<7)
#define AW86927_BIT_TRGCFG5_TRG2_NEG_DISABLE		(0<<7)
#define AW86927_BIT_TRGCFG5_TRG2SEQ_N_MASK		(~(0x7F<<0))

/* TRGCFG6: reg 0x38 RW */
#define AW86927_BIT_TRGCFG6_TRG3_NEG_MASK		(~(1<<7))
#define AW86927_BIT_TRGCFG6_TRG3_NEG_ENABLE		(1<<7)
#define AW86927_BIT_TRGCFG6_TRG3_NEG_DISABLE		(0<<7)
#define AW86927_BIT_TRGCFG6_TRG3SEQ_N_MASK		(~(0x7F<<0))

/* TRGCFG7: reg 0x39 RW */
#define AW86927_BIT_TRGCFG7_TRG1_POLAR_MASK		(~(1<<7))
#define AW86927_BIT_TRGCFG7_TRG1_POLAR_NEG		(1<<7)
#define AW86927_BIT_TRGCFG7_TRG1_POLAR_POS		(0<<7)
#define AW86927_BIT_TRGCFG7_TRG1_MODE_MASK		(~(1<<6))
#define AW86927_BIT_TRGCFG7_TRG1_MODE_LEVEL		(1<<6)
#define AW86927_BIT_TRGCFG7_TRG1_MODE_EDGE		(0<<6)
#define AW86927_BIT_TRGCFG7_TRG1_AUTO_BRK_MASK		(~(1<<5))
#define AW86927_BIT_TRGCFG7_TRG1_AUTO_BRK_ENABLE	(1<<5)
#define AW86927_BIT_TRGCFG7_TRG1_AUTO_BRK_DISABLE	(0<<5)
#define AW86927_BIT_TRGCFG7_TRG1_BST_MASK		(~(1<<4))
#define AW86927_BIT_TRGCFG7_TRG1_BST_ENABLE		(1<<4)
#define AW86927_BIT_TRGCFG7_TRG1_BST_DISABLE		(0<<4)
#define AW86927_BIT_TRGCFG7_TRG2_POLAR_MASK		(~(1<<3))
#define AW86927_BIT_TRGCFG7_TRG2_POLAR_NEG		(1<<3)
#define AW86927_BIT_TRGCFG7_TRG2_POLAR_POS		(0<<3)
#define AW86927_BIT_TRGCFG7_TRG2_MODE_MASK		(~(1<<2))
#define AW86927_BIT_TRGCFG7_TRG2_MODE_LEVEL		(1<<2)
#define AW86927_BIT_TRGCFG7_TRG2_MODE_EDGE		(0<<2)
#define AW86927_BIT_TRGCFG7_TRG2_AUTO_BRK_MASK		(~(1<<1))
#define AW86927_BIT_TRGCFG7_TRG2_AUTO_BRK_ENABLE	(1<<1)
#define AW86927_BIT_TRGCFG7_TRG2_AUTO_BRK_DISABLE	(0<<1)
#define AW86927_BIT_TRGCFG7_TRG2_BST_MASK		(~(1<<0))
#define AW86927_BIT_TRGCFG7_TRG2_BST_ENABLE		(1<<0)
#define AW86927_BIT_TRGCFG7_TRG2_BST_DISABLE		(0<<0)

/* TRGCFG8: reg 0x3A RW */
#define AW86927_BIT_TRGCFG8_TRG3_POLAR_MASK		(~(1<<7))
#define AW86927_BIT_TRGCFG8_TRG3_POLAR_NEG		(1<<7)
#define AW86927_BIT_TRGCFG8_TRG3_POLAR_POS		(0<<7)
#define AW86927_BIT_TRGCFG8_TRG3_MODE_MASK		(~(1<<6))
#define AW86927_BIT_TRGCFG8_TRG3_MODE_LEVEL		(1<<6)
#define AW86927_BIT_TRGCFG8_TRG3_MODE_EDGE		(0<<6)
#define AW86927_BIT_TRGCFG8_TRG3_AUTO_BRK_MASK		(~(1<<5))
#define AW86927_BIT_TRGCFG8_TRG3_AUTO_BRK_ENABLE	(1<<5)
#define AW86927_BIT_TRGCFG8_TRG3_AUTO_BRK_DISABLE	(0<<5)
#define AW86927_BIT_TRGCFG8_TRG3_BST_MASK		(~(1<<4))
#define AW86927_BIT_TRGCFG8_TRG3_BST_ENABLE		(1<<4)
#define AW86927_BIT_TRGCFG8_TRG3_BST_DISABLE		(0<<4)
#define AW86927_BIT_TRGCFG8_TRG_ONEWIRE_MASK		(~(1<<3))
#define AW86927_BIT_TRGCFG8_TRG_ONEWIRE_ENABLE		(1<<3)
#define AW86927_BIT_TRGCFG8_TRG_ONEWIRE_DISABLE		(0<<3)
#define AW86927_BIT_TRGCFG8_TRG1_STOP_MASK		(~(1<<2))
#define AW86927_BIT_TRGCFG8_TRG1_STOP			(1<<2)
#define AW86927_BIT_TRGCFG8_TRG2_STOP_MASK		(~(1<<1))
#define AW86927_BIT_TRGCFG8_TRG2_STOP			(1<<1)
#define AW86927_BIT_TRGCFG8_TRG3_STOP_MASK		(~(1<<0))
#define AW86927_BIT_TRGCFG8_TRG3_STOP			(1<<0)

/* GLBCFG2: reg 0x3C RW */
/* START_DLY */
#define AW86927_BIT_START_DLY_20US			(0x01)
#define AW86927_BIT_START_DLY_2P5MS			(0x75)
/* GLBCFG4: reg 0x3E RW */
#define AW86927_BIT_GLBCFG4_GO_PRIO_MASK		(~(3<<6))
#define AW86927_BIT_GLBCFG4_TRG3_PRIO_MASK		(~(3<<4))
#define AW86927_BIT_GLBCFG4_TRG2_PRIO_MASK		(~(3<<2))
#define AW86927_BIT_GLBCFG4_TRG1_PRIO_MASK		(~(3<<0))

/* GLBRD5: reg 0x3F R0 */
/* GLB_STATE [3:0] */
#define AW86927_BIT_GLBRD5_STATE_MASK			(~(15<<0))
#define AW86927_BIT_GLBRD5_STATE_STANDBY		(0<<0)
#define AW86927_BIT_GLBRD5_STATE_WAKEUP			(1<<0)
#define AW86927_BIT_GLBRD5_STATE_STARTUP		(2<<0)
#define AW86927_BIT_GLBRD5_STATE_WAIT			(3<<0)
#define AW86927_BIT_GLBRD5_STATE_CONT_GO		(6<<0)
#define AW86927_BIT_GLBRD5_STATE_RAM_GO			(7<<0)
#define AW86927_BIT_GLBRD5_STATE_RTP_GO			(8<<0)
#define AW86927_BIT_GLBRD5_STATE_TRIG_GO		(9<<0)
#define AW86927_BIT_GLBRD5_STATE_I2S_GO			(10<<0)
#define AW86927_BIT_GLBRD5_STATE_BRAKE			(11<<0)
#define AW86927_BIT_GLBRD5_STATE_END			(12<<0)
/* RAMADDRH: reg 0x40 RWS */
#define AW86927_BIT_RAMADDRH_MASK			(~(0x1F<<0))
/* SYSCTRL2: reg 0x44 RWS */
#define AW86927_BIT_SYSCTRL2_SLOT_CHSEL_MASK		(~(1<<5))
#define AW86927_BIT_SYSCTRL2_RTP_SLOT_ONE		(0<<5)
#define AW86927_BIT_SYSCTRL2_RTP_SLOT_TWO		(1<<5)
/* SYSCTRL3: reg 0x45 RW */
#define AW86927_BIT_SYSCTRL3_WCK_PIN_MASK		(~(1<<7))
#define AW86927_BIT_SYSCTRL3_WCK_PIN_ON			(1<<7)
#define AW86927_BIT_SYSCTRL3_WCK_PIN_OFF		(0<<7)
#define AW86927_BIT_SYSCTRL3_WAKE_MASK			(~(1<<6))
#define AW86927_BIT_SYSCTRL3_WAKE_ON			(1<<6)
#define AW86927_BIT_SYSCTRL3_WAKE_OFF			(0<<6)
#define AW86927_BIT_SYSCTRL3_STANDBY_MASK		(~(1<<5))
#define AW86927_BIT_SYSCTRL3_STANDBY_ON			(1<<5)
#define AW86927_BIT_SYSCTRL3_STANDBY_OFF		(0<<5)

#define AW86927_BIT_SYSCTRL3_RTP_DLY_MASK		(~(3<<3))

#define AW86927_BIT_SYSCTRL3_EN_RAMINIT_MASK		(~(1<<2))
#define AW86927_BIT_SYSCTRL3_EN_RAMINIT_ON		(1<<2)
#define AW86927_BIT_SYSCTRL3_EN_RAMINIT_OFF		(0<<2)
#define AW86927_BIT_SYSCTRL3_EN_FIR_MASK		(~(1<<1))
#define AW86927_BIT_SYSCTRL3_EN_FIR_ON			(1<<1)
#define AW86927_BIT_SYSCTRL3_EN_FIR_OFF			(0<<1)
#define AW86927_BIT_SYSCTRL3_WAKE_MODE_MASK		(~(1<<0))
#define AW86927_BIT_SYSCTRL3_WAKE_MODE_ON		(1<<0)
#define AW86927_BIT_SYSCTRL3_WAKE_MODE_OFF		(0<<0)

/* SYSCTRL4: reg 0x46 RW */
#define AW86927_BIT_SYSCTRL4_EN_INTN_CLKOUT_MASK	(~(1<<7))
#define AW86927_BIT_SYSCTRL4_EN_INTN_CLKOUT_ON		(1<<7)
#define AW86927_BIT_SYSCTRL4_EN_INTN_CLKOUT_OFF		(0<<7)
#define AW86927_BIT_SYSCTRL4_WAVDAT_MODE_MASK		(~(3<<5))
#define AW86927_BIT_SYSCTRL4_WAVDAT_12K			(2<<5)
#define AW86927_BIT_SYSCTRL4_WAVDAT_48K			(1<<5)
#define AW86927_BIT_SYSCTRL4_WAVDAT_24K			(0<<5)
#define AW86927_BIT_SYSCTRL4_INT_EDGE_MODE_MASK		(~(1<<4))
#define AW86927_BIT_SYSCTRL4_INT_EDGE_MODE_POS		(0<<4)
#define AW86927_BIT_SYSCTRL4_INT_EDGE_MODE_BOTH		(1<<4)
#define AW86927_BIT_SYSCTRL4_INT_MODE_MASK		(~(1<<3))
#define AW86927_BIT_SYSCTRL4_INT_MODE_EDGE		(1<<3)
#define AW86927_BIT_SYSCTRL4_INT_MODE_LEVEL		(0<<3)
#define AW86927_BIT_SYSCTRL4_GAIN_BYPASS_MASK		(~(1<<0))
#define AW86927_BIT_SYSCTRL4_GAIN_FIXED			(0<<0)
#define AW86927_BIT_SYSCTRL4_GAIN_CHANGEABLE		(1<<0)
/* SYSCTRL5: reg 0x47 RW */
#define AW86927_BIR_SYSCTRL5_INIT_VAL			(0x5A)
#define AW86927_BIT_SYSCTRL5_EN_BRO_ADDR_MASK		(~(1<<7))
#define AW86927_BIT_SYSCTRL5_EN_BRO_ADDR_ON		(1<<7)
#define AW86927_BIT_SYSCTRL5_EN_BRO_ADDR_OFF		(0<<7)
#define AW86927_BIT_SYSCTRL5_BROADCAST_ADDR_MASK	(~(0x7F<<0))

/* PWMCFG1: reg 0x48 RW */
#define AW86927_BIT_PWMCFG1_PRC_EN_MASK			(~(1<<7))
#define AW86927_BIT_PWMCFG1_PRC_ENABLE			(1<<7)
#define AW86927_BIT_PWMCFG1_PRC_DISABLE			(0<<7)
#define AW86927_BIT_PWMCFG1_PRCTIME_MASK		(~(0x7F<<0))
#define AW86927_BIT_PWMCFG1_INIT_VAL			(0xA0)

/* PWMCFG2: reg 0x49 RW */
#define AW86927_BIT_PWMCFG2_PRCT_MODE_MASK		(~(1<<6))
#define AW86927_BIT_PWMCFG2_PRCT_MODE_VALID		(0<<6)
#define AW86927_BIT_PWMCFG2_PRCT_MODE_INVALID		(1<<6)
#define AW86927_BIT_PWMCFG2_REF_SEL_MASK		(~(1<<5))
#define AW86927_BIT_PWMCFG2_REF_SEL_TRIANGLE		(1<<5)
#define AW86927_BIT_PWMCFG2_REF_SEL_SAWTOOTH		(0<<5)
#define AW86927_BIT_PWMCFG2_PD_HWM_MASK			(~(1<<4))
#define AW86927_BIT_PWMCFG2_PD_HWM_ON			(1<<4)
#define AW86927_BIT_PWMCFG2_PWMOE_MASK			(~(1<<3))
#define AW86927_BIT_PWMCFG2_PWMOE_ON			(1<<3)
#define AW86927_BIT_PWMCFG2_PWMFRC_MASK			(~(7<<0))

/* PWMCFG3: reg 0x4A RW */
#define AW86927_BIT_PWMCFG3_PR_EN_MASK			(~(1<<7))
#define AW86927_BIT_PWMCFG3_PR_ENABLE			(1<<7)
#define AW86927_BIT_PWMCFG3_PR_DISABLE			(0<<7)
#define AW86927_BIT_PWMCFG3_PRLVL_MASK			(~(0x7F<<0))

/* VBATCTRL: reg 0x4C RW */
#define AW86927_BIT_VBATCTRL_VBAT_PRO_MASK		(~(1<<7))
#define AW86927_BIT_VBATCTRL_VBAT_PRO_ENABLE		(1<<7)
#define AW86927_BIT_VBATCTRL_VBAT_PRO_DISABLE		(0<<7)

#define AW86927_BIT_VBATCTRL_VBAT_MODE_MASK		(~(1<<6))
#define AW86927_BIT_VBATCTRL_VBAT_MODE_HW		(1<<6)
#define AW86927_BIT_VBATCTRL_VBAT_MODE_SW		(0<<6)

#define AW86927_BIT_VBATCTRL_VBAT_MODE_CON_MASK		(~(1<<5))
#define AW86927_BIT_VBATCTRL_VBAT_MODE_CON_DURING	(1<<5)
#define AW86927_BIT_VBATCTRL_VBAT_MODE_CON_BEFORE	(0<<5)

#define AW86927_BIT_VBATCTRL_DELTA_VBAT_MASK		(~(1<<4))
#define AW86927_BIT_VBATCTRL_DELTA_VBAT_0P2V		(1<<4)
#define AW86927_BIT_VBATCTRL_DELTA_VBAT_0P1V		(0<<4)

#define AW86927_BIT_VBATCTRL_REL_VBAT_MASK		(~(3<<2))
#define AW86927_BIT_VBATCTRL_ABS_VBAT_MASK		(~(3<<0))

/* DETCFG1: reg 0x4D RW */
#define AW86927_BIT_DETCFG1_VBAT_REF_MASK		(~(7<<4))
#define AW86927_BIT_DETCFG1_VBAT_REF_4P2V		(3<<4)
#define AW86927_BIT_DETCFG1_ADC_FS_MASK			(~(3<<2))
#define AW86927_BIT_DETCFG1_ADC_FS_192KHZ		(0<<2)
#define AW86927_BIT_DETCFG1_ADC_FS_96KHZ		(1<<2)
#define AW86927_BIT_DETCFG1_ADC_FS_48KHZ		(2<<2)
#define AW86927_BIT_DETCFG1_ADC_FS_24KHZ		(3<<2)


#define AW86927_BIT_DETCFG1_DET_GO_MASK			(~(3<<0))
#define AW86927_BIT_DETCFG1_DET_GO_NA			(0<<0)

#define AW86927_BIT_DETCFG1_DET_GO_MASK			(~(3<<0))
#define AW86927_BIT_DETCFG1_DET_GO_NA			(0<<0)
#define AW86927_BIT_DETCFG1_DET_GO_DET_SEQ0		(1<<0)

/* DETCFG2: reg 0x4E RW */
#define AW86927_BIT_DETCFG2_DET_SEQ0_MASK		(~(0xF<<3))
#define AW86927_BIT_DETCFG2_DET_SEQ0_VBAT		(0<<3)
#define AW86927_BIT_DETCFG2_DET_SEQ0_PVDD		(1<<3)
#define AW86927_BIT_DETCFG2_DET_SEQ0_TRIG1		(2<<3)
#define AW86927_BIT_DETCFG2_DET_SEQ0_RL			(3<<3)
#define AW86927_BIT_DETCFG2_DET_SEQ0_OS			(4<<3)
#define AW86927_BIT_DETCFG2_DET_SEQ0_VOUT		(5<<3)
#define AW86927_BIT_DETCFG2_DET_SEQ0_FTS		(6<<3)
#define AW86927_BIT_DETCFG2_D2S_GAIN_MASK		(~(7<<0))
#define AW86927_BIT_DETCFG2_D2S_GAIN			(7<<0)
#define AW86927_BIT_DETCFG2_D2S_GAIN_1			(0<<0)
#define AW86927_BIT_DETCFG2_D2S_GAIN_2			(1<<0)
#define AW86927_BIT_DETCFG2_D2S_GAIN_4			(2<<0)
#define AW86927_BIT_DETCFG2_D2S_GAIN_8			(3<<0)
#define AW86927_BIT_DETCFG2_D2S_GAIN_10			(4<<0)
#define AW86927_BIT_DETCFG2_D2S_GAIN_16			(5<<0)
#define AW86927_BIT_DETCFG2_D2S_GAIN_20			(6<<0)
#define AW86927_BIT_DETCFG2_D2S_GAIN_40			(7<<0)
/* TRIMCFG1: reg 0x52 RW */
#define AW86927_BIT_TRIMCFG1_RL_TRIM_SRC_MASK		(~(0X01<<7))
#define AW86927_BIT_TRIMCFG1_RL_TRIM_SRC_EFUSE		(0<<7)
#define AW86927_BIT_TRIMCFG1_RL_TRIM_SRC_REG		(1<<7)
/* TMCFG: reg 0x5B RW */
#define AW86927_BIT_TMCFG_TM_UNLOCK			(0x7d)
#define AW86927_BIT_TMCFG_TM_LOCK			(0x00)

/* TMCFG: reg 0x5C RW */
#define AW86927_BIT_EFCFG1_INIT_VAL			(0x09)

/* EFCFG6: reg 0x61 */
#define AW86927_BIT_EFCFG6_MASK				(0x80)

/* EFCFG7: reg 0x62 */
#define AW86927_BIT_EFCFG7_RESERVED_MASK		(~(0x01<<7))

/* EFCFG8: reg 0x63 */
#define AW86927_BIT_EFCFG8_EF_TRM_BST_IPEAK_MASK	(~(0xF0<<0))

/* TESTR: reg 0x65 RO */
#define AW86927_BIT_TESTR_BST_SS_FINISH			(8<<0)
#define AW86927_BIT_TESTR_BIST_OVP2S			(4<<0)
#define AW86927_BIT_TESTR_BIST_FAIL			(2<<0)
#define AW86927_BIT_TESTR_BIST_DONE			(1<<0)

/* ANACFG11: reg 0x70 RW */
#define AW86927_BIT_ANACFG11_INIT_VAL			(0x0f)

/* ANACFG12: reg 0x71 RW */
#define AW86927_BIT_ANACFG12_BST_SKIP_MASK		(~(0x01<<7))
#define AW86927_BIT_ANACFG12_BST_SKIP_OPEN		(0<<7)
#define AW86927_BIT_ANACFG12_BST_SKIP_SHUTDOWN		(1<<7)

/* ANACFG13: reg 0x72 RW */
#define AW86927_BIT_ANACFG13_PEAKCUR_MASK		(~(15<<4))
#define AW86927_BIT_ANACFG13_PEAKCUR_4A			(9<<4)
#define AW86927_BIT_ANACFG13_PEAKCUR_3P75A		(8<<4)
#define AW86927_BIT_ANACFG13_PEAKCUR_3P60A		(7<<4)
#define AW86927_BIT_ANACFG13_PEAKCUR_3P45A		(6<<4)
#define AW86927_BIT_ANACFG13_PEAKCUR_3P25A		(5<<4)
#define AW86927_BIT_ANACFG13_PEAKCUR_3P05A		(4<<4)
#define AW86927_BIT_ANACFG13_PEAKCUR_2P95A		(3<<4)
#define AW86927_BIT_ANACFG13_PEAKCUR_2AP75A		(2<<4)
#define AW86927_BIT_ANACFG13_PEAKCUR_2P65A		(1<<4)
#define AW86927_BIT_ANACFG13_PEAKCUR_2P45A		(0<<4)

/* ANACFG15: reg 0x74 RW */
#define AW86927_BIT_ANACFG15_BST_PEAK_MODE_MASK		(~(0x01<<7))
#define AW86927_BIT_ANACFG15_BST_PEAK_ADP		(0<<7)
#define AW86927_BIT_ANACFG15_BST_PEAK_BACK		(1<<7)

/* ANACFG16: reg 0x75 RW */
#define AW86927_BIT_ANACFG16_BST_SRC_MASK		(~(1<<4))
#define AW86927_BIT_ANACFG16_BST_SRC_3NS		(0<<4)

/* ANACFG20: reg 0x79 RW */

#define AW86927_BIT_ANACFG20_TRIM_LRA_MASK		(~(0x3F<<0))
#define AW86927_BIT_ANACFG20_TRIM_LRA			(0x3F<<0)

/*****************************************************
 *
 * Extern function : parse dts
 *
 *****************************************************/
/*********************************************************
 *
 * extern
 *
 ********************************************************/
extern int aw86927_haptics_upload_effect(struct input_dev *dev,
					struct ff_effect *effect,
					struct ff_effect *old);
extern int aw86927_haptics_playback(struct input_dev *dev, int effect_id,
				   int val);
extern int aw86927_haptics_erase(struct input_dev *dev, int effect_id);
extern void aw86927_haptics_set_gain(struct input_dev *dev, u16 gain);
extern void aw86927_haptics_set_gain_work_routine(struct work_struct *work);
extern void aw86927_interrupt_setup(struct aw86927 *aw86927);
extern int aw86927_vibrator_init(struct aw86927 *aw86927);
extern int aw86927_haptic_init(struct aw86927 *aw86927);
extern int aw86927_ram_init(struct aw86927 *aw86927);
extern irqreturn_t aw86927_irq(int irq, void *data);
extern struct attribute_group aw86927_vibrator_attribute_group;
extern int aw86927_parse_dt(struct aw86927 *aw86927, struct device *dev,
		     struct device_node *np);
extern int aw86927_check_qualify(struct awinic *awinic);
#endif
