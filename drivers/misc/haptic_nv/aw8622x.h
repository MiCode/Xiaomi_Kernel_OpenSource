#ifndef _AW8622X_H_
#define _AW8622X_H_

#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <sound/control.h>
#include <sound/soc.h>
#include "haptic_nv.h"

/*********************************************************
 *
 * marco
 *
 ********************************************************/
#define AW8622X_I2C_RETRIES			(5)
#define AW8622X_RTP_NAME_MAX			(64)
#define AW8622X_SEQUENCER_SIZE			(8)
#define AW8622X_SEQUENCER_LOOP_SIZE		(4)
#define AW8622X_OSC_CALI_MAX_LENGTH		(11000000)
#define AW8622X_PM_QOS_VALUE_VB			(400)
#define AW8622X_VBAT_REFER			(4200)
#define AW8622X_VBAT_MIN			(3000)
#define AW8622X_VBAT_MAX			(5500)
#define AW8622X_TRIG_NUM			(3)
#define AW8622X_I2C_RETRY_DELAY			(2)
#define AW8622X_GAIN_MAX			(0x80)
#define AW8622X_VBAT_FORMULA(code)		(6100 * (code) / 1024)
#define AW8622X_LRA_F0_FORMULA(f0_reg)		(384000 * 10 / (f0_reg))
#define AW8622X_CONT_F0_FORMULA(f0_reg)		(384000 * 10 / (f0_reg))
#define AW8622X_LRA_FORMULA(lra_code)		(((lra_code) * 678 * 100) / (1024 * 10))
#define AW8622X_SET_AEADDR_H(addr)		((((addr) >> 1) >> 4) & 0xF0)
#define AW8622X_SET_AEADDR_L(addr)		(((addr) >> 1) & 0x00FF)
#define AW8622X_SET_AFADDR_H(addr)		((((addr) - ((addr) >> 2)) >> 8) & 0x0F)
#define AW8622X_SET_AFADDR_L(addr)		(((addr) - ((addr) >> 2)) & 0x00FF)
#define AW8622X_STOP_DELAY_MIN			(2000)
#define AW8622X_STOP_DELAY_MAX			(2500)
#define AW8622X_VBAT_DELAY_MIN			(20000)
#define AW8622X_VBAT_DELAY_MAX			(25000)
#define AW8622X_F0_DELAY_MIN			(10000)
#define AW8622X_F0_DELAY_MAX			(10500)
#define AW8622X_LRA_DELAY_MIN			(30000)
#define AW8622X_LRA_DELAY_MAX			(35000)
/*********************************************************
 *
 * enum
 *
 ********************************************************/
enum aw8622x_flags {
	AW8622X_FLAG_NONR = 0,
	AW8622X_FLAG_SKIP_INTERRUPTS = 1,
};

enum aw8622x_haptic_work_mode {
	AW8622X_HAPTIC_STANDBY_MODE = 0,
	AW8622X_HAPTIC_RAM_MODE = 1,
	AW8622X_HAPTIC_RAM_LOOP_MODE = 2,
	AW8622X_HAPTIC_CONT_MODE = 3,
	AW8622X_HAPTIC_RTP_MODE = 4,
	AW8622X_HAPTIC_TRIG_MODE = 5,
	AW8622X_HAPTIC_NULL = 6,
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

enum aw8622X_awrw_flag {
	AW8622X_WRITE = 0,
	AW8622X_READ = 1,
};

/*********************************************************
 *
 * struct
 *
 ********************************************************/
struct aw862xx_trig {
	unsigned char trig_level;
	unsigned char trig_polar;
	unsigned char pos_enable;
	unsigned char pos_sequence;
	unsigned char neg_enable;
	unsigned char neg_sequence;
	unsigned char trig_brk;
};

struct aw8622x_dts_info {
	unsigned int lk_f0_cali;
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
	unsigned int duration_time[3];
	bool is_enabled_powerup_f0_cali;
	bool is_enabled_auto_brk;
};

struct aw8622x {
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
	unsigned char duration_time_size;
	unsigned char seq[AW8622X_SEQUENCER_SIZE];
	unsigned char loop[AW8622X_SEQUENCER_SIZE];

	char duration_time_flag;

	bool isUsedIntn;
	bool haptic_ready;

	int name;
	int reset_gpio;
	int pdlcen_gpio;
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

	struct haptic_audio haptic_audio;
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct device *dev;
	struct input_dev *input;
	struct mutex lock;
	struct mutex rtp_lock;
	struct mutex ram_lock;
	struct hrtimer timer;
	struct work_struct vibrator_work;
	struct work_struct rtp_work;
	struct delayed_work ram_work;
	struct aw862xx_trig trig[AW8622X_TRIG_NUM];
	struct aw8622x_dts_info dts_info;
	struct aw_i2c_package aw_i2c_package;
	struct ram ram;
	struct aw8622x_container *rtp_container;
	ktime_t kstart, kend;

	cdev_t vib_dev;
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
extern char aw8622x_check_qualify_l(struct aw8622x *aw8622x);
extern int aw8622x_parse_dt_l(struct aw8622x *aw8622x, struct device *dev,
			    struct device_node *np);
extern void aw8622x_interrupt_setup_l(struct aw8622x *aw8622x);
extern int aw8622x_vibrator_init_l(struct aw8622x *aw8622x);
extern int aw8622x_haptic_init_l(struct aw8622x *aw8622x);
extern int aw8622x_ram_work_init_l(struct aw8622x *aw8622x);
extern irqreturn_t aw8622x_irq_l(int irq, void *data);
extern int aw8622x_haptic_stop_l(struct aw8622x *aw8622x);
extern struct attribute_group aw8622x_vibrator_attribute_group_l;

/********************************************
 * Register List
 *******************************************/
#define AW8622X_REG_ID		(0x00)
#define AW8622X_REG_SYSST	(0x01)
#define AW8622X_REG_SYSINT	(0x02)
#define AW8622X_REG_SYSINTM	(0x03)
#define AW8622X_REG_SYSST2	(0x04)
#define AW8622X_REG_SYSER	(0x05)
#define AW8622X_REG_PLAYCFG2	(0x07)
#define AW8622X_REG_PLAYCFG3	(0x08)
#define AW8622X_REG_PLAYCFG4	(0x09)
#define AW8622X_REG_WAVCFG1	(0x0A)
#define AW8622X_REG_WAVCFG2	(0x0B)
#define AW8622X_REG_WAVCFG3	(0x0C)
#define AW8622X_REG_WAVCFG4	(0x0D)
#define AW8622X_REG_WAVCFG5	(0x0E)
#define AW8622X_REG_WAVCFG6	(0x0F)
#define AW8622X_REG_WAVCFG7	(0x10)
#define AW8622X_REG_WAVCFG8	(0x11)
#define AW8622X_REG_WAVCFG9	(0x12)
#define AW8622X_REG_WAVCFG10	(0x13)
#define AW8622X_REG_WAVCFG11	(0x14)
#define AW8622X_REG_WAVCFG12	(0x15)
#define AW8622X_REG_WAVCFG13	(0x16)
#define AW8622X_REG_CONTCFG1	(0x18)
#define AW8622X_REG_CONTCFG2	(0x19)
#define AW8622X_REG_CONTCFG3	(0x1A)
#define AW8622X_REG_CONTCFG4	(0x1B)
#define AW8622X_REG_CONTCFG5	(0x1C)
#define AW8622X_REG_CONTCFG6	(0x1D)
#define AW8622X_REG_CONTCFG7	(0x1E)
#define AW8622X_REG_CONTCFG8	(0x1F)
#define AW8622X_REG_CONTCFG9	(0x20)
#define AW8622X_REG_CONTCFG10	(0x21)
#define AW8622X_REG_CONTCFG11	(0x22)
#define AW8622X_REG_CONTCFG12	(0x23)
#define AW8622X_REG_CONTCFG13	(0x24)
#define AW8622X_REG_CONTRD14	(0x25)
#define AW8622X_REG_CONTRD15	(0x26)
#define AW8622X_REG_CONTRD16	(0x27)
#define AW8622X_REG_CONTRD17	(0x28)
#define AW8622X_REG_CONTRD18	(0x29)
#define AW8622X_REG_CONTRD19	(0x2A)
#define AW8622X_REG_CONTRD20	(0x2B)
#define AW8622X_REG_CONTRD21	(0x2C)
#define AW8622X_REG_RTPCFG1	(0x2D)
#define AW8622X_REG_RTPCFG2	(0x2E)
#define AW8622X_REG_RTPCFG3	(0x2F)
#define AW8622X_REG_RTPCFG4	(0x30)
#define AW8622X_REG_RTPCFG5	(0x31)
#define AW8622X_REG_RTPDATA	(0x32)
#define AW8622X_REG_TRGCFG1	(0x33)
#define AW8622X_REG_TRGCFG2	(0x34)
#define AW8622X_REG_TRGCFG3	(0x35)
#define AW8622X_REG_TRGCFG4	(0x36)
#define AW8622X_REG_TRGCFG5	(0x37)
#define AW8622X_REG_TRGCFG6	(0x38)
#define AW8622X_REG_TRGCFG7	(0x39)
#define AW8622X_REG_TRGCFG8	(0x3A)
#define AW8622X_REG_GLBCFG4	(0x3E)
#define AW8622X_REG_GLBRD5	(0x3F)
#define AW8622X_REG_RAMADDRH	(0x40)
#define AW8622X_REG_RAMADDRL	(0x41)
#define AW8622X_REG_RAMDATA	(0x42)
#define AW8622X_REG_SYSCTRL1	(0x43)
#define AW8622X_REG_SYSCTRL2	(0x44)
#define AW8622X_REG_SYSCTRL3	(0x45)
#define AW8622X_REG_SYSCTRL4	(0x46)
#define AW8622X_REG_SYSCTRL5	(0x47)
#define AW8622X_REG_SYSCTRL6	(0x48)
#define AW8622X_REG_SYSCTRL7	(0x49)
#define AW8622X_REG_PWMCFG1	(0x4C)
#define AW8622X_REG_PWMCFG3	(0x4E)
#define AW8622X_REG_PWMCFG4	(0x4F)
#define AW8622X_REG_DETCFG1	(0x51)
#define AW8622X_REG_DETCFG2	(0x52)
#define AW8622X_REG_DET_RL	(0x53)
#define AW8622X_REG_DET_VBAT	(0x55)
#define AW8622X_REG_DET_LO	(0x57)
#define AW8622X_REG_TRIMCFG1	(0x58)
#define AW8622X_REG_TRIMCFG3	(0x5A)
#define AW8622X_REG_EFRD9	(0x64)
#define AW8622X_REG_ANACFG8	(0x77)
/******************************************************
 * Register Detail
 *****************************************************/
/* SYSST: reg 0x01 RO */
#define AW8622X_BIT_SYSST_UVLS				(1<<5)
#define AW8622X_BIT_SYSST_FF_AES			(1<<4)
#define AW8622X_BIT_SYSST_FF_AFS			(1<<3)
#define AW8622X_BIT_SYSST_OCDS				(1<<2)
#define AW8622X_BIT_SYSST_OTS				(1<<1)
#define AW8622X_BIT_SYSST_DONES				(1<<0)

/* SYSINT: reg 0x02 RC */
#define AW8622X_BIT_SYSINT_UVLI				(1<<5)
#define AW8622X_BIT_SYSINT_FF_AEI			(1<<4)
#define AW8622X_BIT_SYSINT_FF_AFI			(1<<3)
#define AW8622X_BIT_SYSINT_OCDI				(1<<2)
#define AW8622X_BIT_SYSINT_OTI				(1<<1)
#define AW8622X_BIT_SYSINT_DONEI			(1<<0)

/* SYSINTM: reg 0x03 RW */
#define AW8622X_BIT_SYSINTM_UVLM_MASK			(~(1<<5))
#define AW8622X_BIT_SYSINTM_UVLM_OFF			(1<<5)
#define AW8622X_BIT_SYSINTM_UVLM_ON			(0<<5)
#define AW8622X_BIT_SYSINTM_FF_AEM_MASK			(~(1<<4))
#define AW8622X_BIT_SYSINTM_FF_AEM_OFF			(1<<4)
#define AW8622X_BIT_SYSINTM_FF_AEM_ON			(0<<4)
#define AW8622X_BIT_SYSINTM_FF_AFM_MASK			(~(1<<3))
#define AW8622X_BIT_SYSINTM_FF_AFM_OFF			(1<<3)
#define AW8622X_BIT_SYSINTM_FF_AFM_ON			(0<<3)
#define AW8622X_BIT_SYSINTM_OCDM_MASK			(~(1<<2))
#define AW8622X_BIT_SYSINTM_OCDM_OFF			(1<<2)
#define AW8622X_BIT_SYSINTM_OCDM_ON			(0<<2)
#define AW8622X_BIT_SYSINTM_OTM_MASK			(~(1<<1))
#define AW8622X_BIT_SYSINTM_OTM_OFF			(1<<1)
#define AW8622X_BIT_SYSINTM_OTM_ON			(0<<1)
#define AW8622X_BIT_SYSINTM_DONEM_MASK			(~(1<<0))
#define AW8622X_BIT_SYSINTM_DONEM_OFF			(1<<0)
#define AW8622X_BIT_SYSINTM_DONEM_ON			(0<<0)

/* SYSST2: reg 0x04 RO */
#define AW8622X_BIT_SYSST2_RAM_ADDR_ER			(1<<7)
#define AW8622X_BIT_SYSST2_TRG_ADDR_ER			(1<<6)
#define AW8622X_BIT_SYSST2_VBG_OK			(1<<3)
#define AW8622X_BIT_SYSST2_LDO_OK			(1<<2)
#define AW8622X_BIT_SYSST2_FF_FULL			(1<<1)
#define AW8622X_BIT_SYSST2_FF_EMPTY			(1<<0)

/* SYSER: reg 0x05 RC */
#define AW8622X_BIT_SYSER_I2S_ERR			(1<<7)
#define AW8622X_BIT_SYSER_TRIG1_EVENT			(1<<6)
#define AW8622X_BIT_SYSER_TRIG2_EVENT			(1<<5)
#define AW8622X_BIT_SYSER_TRIG3_EVENT			(1<<4)
#define AW8622X_BIT_SYSER_OV				(1<<3)
#define AW8622X_BIT_SYSER_ADDR_ER			(1<<2)
#define AW8622X_BIT_SYSER_FF_ER				(1<<1)
#define AW8622X_BIT_SYSER_PLL_REF_ER			(1<<0)

/* PLAYCFG3: reg 0x08 RW */
#define AW8622X_BIT_PLAYCFG3_STOP_MODE_MASK		(~(1<<5))
#define AW8622X_BIT_PLAYCFG3_STOP_MODE_NOW		(1<<5)
#define AW8622X_BIT_PLAYCFG3_STOP_MODE_LATER		(0<<5)
#define AW8622X_BIT_PLAYCFG3_BRK_EN_MASK		(~(1<<2))
#define AW8622X_BIT_PLAYCFG3_BRK			(1<<2)
#define AW8622X_BIT_PLAYCFG3_BRK_ENABLE			(1<<2)
#define AW8622X_BIT_PLAYCFG3_BRK_DISABLE		(0<<2)
#define AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK		(~(3<<0))
#define AW8622X_BIT_PLAYCFG3_PLAY_MODE_STOP		(3<<0)
#define AW8622X_BIT_PLAYCFG3_PLAY_MODE_CONT		(2<<0)
#define AW8622X_BIT_PLAYCFG3_PLAY_MODE_RTP		(1<<0)
#define AW8622X_BIT_PLAYCFG3_PLAY_MODE_RAM		(0<<0)

/* PLAYCFG4: reg 0x09 RW */
#define AW8622X_BIT_PLAYCFG4_STOP_MASK			(~(1<<1))
#define AW8622X_BIT_PLAYCFG4_STOP_ON			(1<<1)
#define AW8622X_BIT_PLAYCFG4_STOP_OFF			(0<<1)
#define AW8622X_BIT_PLAYCFG4_GO_MASK			(~(1<<0))
#define AW8622X_BIT_PLAYCFG4_GO_ON			(1<<0)
#define AW8622X_BIT_PLAYCFG4_GO_OFF			(0<<0)

/* WAVCFG1-8: reg 0x0A - reg 0x11 RW */
#define AW8622X_BIT_WAVCFG_SEQWAIT_MASK			(~(1<<7))
#define AW8622X_BIT_WAVCFG_SEQWAIT_TIME			(1<<7)
#define AW8622X_BIT_WAVCFG_SEQWAIT_NUMBER		(0<<7)
#define AW8622X_BIT_WAVCFG_SEQ				(0x7F)

/* WAVCFG9-12: reg 0x12 - reg 0x15 RW */
#define AW8622X_BIT_WAVLOOP_SEQ_ODD_MASK		(~(0x0F<<4))
#define AW8622X_BIT_WAVLOOP_SEQ_ODD_INIFINITELY		(0x0F<<4)
#define AW8622X_BIT_WAVLOOP_SEQ_EVEN_MASK		(~(0x0F<<0))
#define AW8622X_BIT_WAVLOOP_SEQ_EVEN_INIFINITELY	(0x0F<<0)
#define AW8622X_BIT_WAVLOOP_INIFINITELY			(0x0F<<0)

/* WAVCFG9: reg 0x12 RW */
#define AW8622X_BIT_WAVCFG9_SEQ1LOOP_MASK		(~(0x0F<<4))
#define AW8622X_BIT_WAVCFG9_SEQ1LOOP_INIFINITELY	(0x0F<<4)
#define AW8622X_BIT_WAVCFG9_SEQ2LOOP_MASK		(~(0x0F<<0))
#define AW8622X_BIT_WAVCFG9_SEQ2LOOP_INIFINITELY	(0x0F<<0)

/* WAVCFG10: reg 0x13 RW */
#define AW8622X_BIT_WAVCFG10_SEQ3LOOP_MASK		(~(0x0F<<4))
#define AW8622X_BIT_WAVCFG10_SEQ3LOOP_INIFINITELY	(0x0F<<4)
#define AW8622X_BIT_WAVCFG10_SEQ4LOOP_MASK		(~(0x0F<<0))
#define AW8622X_BIT_WAVCFG10_SEQ4LOOP_INIFINITELY	(0x0F<<0)

/* WAVCFG11: reg 0x14 RW */
#define AW8622X_BIT_WAVCFG11_SEQ5LOOP_MASK		(~(0x0F<<4))
#define AW8622X_BIT_WAVCFG11_SEQ5LOOP_INIFINITELY	(0x0F<<4)
#define AW8622X_BIT_WAVCFG11_SEQ6LOOP_MASK		(~(0x0F<<0))
#define AW8622X_BIT_WAVCFG11_SEQ6LOOP_INIFINITELY	(0x0F<<0)

/* WAVCFG12: reg 0x15 RW */
#define AW8622X_BIT_WAVCFG12_SEQ7LOOP_MASK		(~(0x0F<<4))
#define AW8622X_BIT_WAVCFG12_SEQ7LOOP_INIFINITELY	(0x0F<<4)
#define AW8622X_BIT_WAVCFG12_SEQ8LOOP_MASK		(~(0x0F<<0))
#define AW8622X_BIT_WAVCFG12_SEQ8LOOP_INIFINITELY	(0x0F<<0)

/***************** CONT *****************/
/* CONTCFG1: reg 0x18 RW */
#define AW8622X_BIT_CONTCFG1_EDGE_FRE_MASK		(~(0x0F<<4))
#define AW8622X_BIT_CONTCFG1_EN_F0_DET_MASK		(~(1<<3))
#define AW8622X_BIT_CONTCFG1_F0_DET_ENABLE		(1<<3)
#define AW8622X_BIT_CONTCFG1_F0_DET_DISABLE		(0<<3)
#define AW8622X_BIT_CONTCFG1_SIN_MODE_MASK		(~(1<<0))
#define AW8622X_BIT_CONTCFG1_SIN_MODE_COS		(1<<0)
#define AW8622X_BIT_CONTCFG1_SIN_MODE_SINE		(0<<0)

/* CONTCFG5: reg 0x1C RW */
#define AW8622X_BIT_CONTCFG5_BRK_GAIN_MASK		(~(0x0F<<0))

/* CONTCFG6: reg 0x1D RW */
#define AW8622X_BIT_CONTCFG6_TRACK_EN_MASK		(~(1<<7))
#define AW8622X_BIT_CONTCFG6_TRACK_ENABLE		(1<<7)
#define AW8622X_BIT_CONTCFG6_TRACK_DISABLE		(0<<7)
#define AW8622X_BIT_CONTCFG6_DRV1_LVL_MASK		(~(0x7F<<0))

/* CONTCFG7: reg 0x1E RW */
#define AW8622X_BIT_CONTCFG7_DRV2_LVL_MASK		(~(0x7F<<0))

/* CONTCFG13: reg 0x24 RW */
#define AW8622X_BIT_CONTCFG13_TSET_MASK			(~(0x0F<<4))
#define AW8622X_BIT_CONTCFG13_BEME_SET_MASK		(~(0x0F<<0))

/***************** RTP *****************/
/* RTPCFG1: reg 0x2D RW */
#define AW8622X_BIT_RTPCFG1_ADDRH_MASK			 (~(0x0F<<0))

#define AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_MASK		 (~(1<<5))
#define AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_EN		 (1<<5)
#define AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_DIS	         (0<<5)

#define AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_MASK		 (~(1<<4))
#define AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_EN		 (1<<4)
#define AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_DIS		 (0<<4)

/* RTPCFG3: reg 0x2F RW */
#define AW8622X_BIT_RTPCFG3_FIFO_AEH_MASK		(~(0x0F<<4))
#define AW8622X_BIT_RTPCFG3_FIFO_AFH_MASK		(~(0x0F<<0))
#define AW8622X_BIT_RTPCFG3_FIFO_AEH			(0x0F<<4)
#define AW8622X_BIT_RTPCFG3_FIFO_AFH			(0x0F<<0)

/***************** TRIGGER *****************/
#define AW8622X_BIT_TRG_ENABLE_MASK		(~(1<<7))
#define AW8622X_BIT_TRG_ENABLE			(1<<7)
#define AW8622X_BIT_TRG_DISABLE			(0<<7)
#define AW8622X_BIT_TRG_SEQ_MASK		(~(0x7F<<0))

/* TRGCFG1: reg 0x33 RW */
#define AW8622X_BIT_TRGCFG1_TRG1_POS_MASK		(~(1<<7))
#define AW8622X_BIT_TRGCFG1_TRG1_POS_ENABLE		(1<<7)
#define AW8622X_BIT_TRGCFG1_TRG1_POS_DISABLE		(0<<7)
#define AW8622X_BIT_TRGCFG1_TRG1SEQ_P_MASK		(~(0x7F<<0))

/* TRGCFG2: reg 0x34 RW */
#define AW8622X_BIT_TRGCFG2_TRG2_POS_MASK		(~(1<<7))
#define AW8622X_BIT_TRGCFG2_TRG2_POS_ENABLE		(1<<7)
#define AW8622X_BIT_TRGCFG2_TRG2_POS_DISABLE		(0<<7)
#define AW8622X_BIT_TRGCFG2_TRG2SEQ_P_MASK		(~(0x7F<<0))

/* TRGCFG3: reg 0x35 RW */
#define AW8622X_BIT_TRGCFG3_TRG3_POS_MASK		(~(1<<7))
#define AW8622X_BIT_TRGCFG3_TRG3_POS_ENABLE		(1<<7)
#define AW8622X_BIT_TRGCFG3_TRG3_POS_DISABLE		(0<<7)
#define AW8622X_BIT_TRGCFG3_TRG3SEQ_P_MASK		(~(0x7F<<0))

/* TRGCFG4: reg 0x36 RW */
#define AW8622X_BIT_TRGCFG4_TRG1_NEG_MASK		(~(1<<7))
#define AW8622X_BIT_TRGCFG4_TRG1_NEG_ENABLE		(1<<7)
#define AW8622X_BIT_TRGCFG4_TRG1_NEG_DISABLE		(0<<7)
#define AW8622X_BIT_TRGCFG4_TRG1SEQ_N_MASK		(~(0x7F<<0))

/* TRGCFG5: reg 0x37 RW */
#define AW8622X_BIT_TRGCFG5_TRG2_NEG_MASK		(~(1<<7))
#define AW8622X_BIT_TRGCFG5_TRG2_NEG_ENABLE		(1<<7)
#define AW8622X_BIT_TRGCFG5_TRG2_NEG_DISABLE		(0<<7)
#define AW8622X_BIT_TRGCFG5_TRG2SEQ_N_MASK		(~(0x7F<<0))

/* TRGCFG6: reg 0x38 RW */
#define AW8622X_BIT_TRGCFG6_TRG3_NEG_MASK		(~(1<<7))
#define AW8622X_BIT_TRGCFG6_TRG3_NEG_ENABLE		(1<<7)
#define AW8622X_BIT_TRGCFG6_TRG3_NEG_DISABLE		(0<<7)
#define AW8622X_BIT_TRGCFG6_TRG3SEQ_N_MASK		(~(0x7F<<0))

/* TRGCFG7: reg 0x39 RW */
#define AW8622X_BIT_TRGCFG7_TRG1_POR_LEV_BRK_MASK	(~(7<<5))
#define AW8622X_BIT_TRGCFG7_TRG2_POR_LEV_BRK_MASK	(~(7<<1))
#define AW8622X_BIT_TRGCFG7_TRG1_POLAR_MASK		(~(1<<7))
#define AW8622X_BIT_TRGCFG7_TRG1_POLAR_NEG		(1<<7)
#define AW8622X_BIT_TRGCFG7_TRG1_POLAR_POS		(0<<7)
#define AW8622X_BIT_TRGCFG7_TRG1_MODE_MASK		(~(1<<6))
#define AW8622X_BIT_TRGCFG7_TRG1_MODE_LEVEL		(1<<6)
#define AW8622X_BIT_TRGCFG7_TRG1_MODE_EDGE		(0<<6)
#define AW8622X_BIT_TRGCFG7_TRG1_AUTO_BRK_MASK		(~(1<<5))
#define AW8622X_BIT_TRGCFG7_TRG1_AUTO_BRK_ENABLE	(1<<5)
#define AW8622X_BIT_TRGCFG7_TRG1_AUTO_BRK_DISABLE	(0<<5)
#define AW8622X_BIT_TRGCFG7_TRG2_POLAR_MASK		(~(1<<3))
#define AW8622X_BIT_TRGCFG7_TRG2_POLAR_NEG		(1<<3)
#define AW8622X_BIT_TRGCFG7_TRG2_POLAR_POS		(0<<3)
#define AW8622X_BIT_TRGCFG7_TRG2_MODE_MASK		(~(1<<2))
#define AW8622X_BIT_TRGCFG7_TRG2_MODE_LEVEL		(1<<2)
#define AW8622X_BIT_TRGCFG7_TRG2_MODE_EDGE		(0<<2)
#define AW8622X_BIT_TRGCFG7_TRG2_AUTO_BRK_MASK		(~(1<<1))
#define AW8622X_BIT_TRGCFG7_TRG2_AUTO_BRK_ENABLE	(1<<1)
#define AW8622X_BIT_TRGCFG7_TRG2_AUTO_BRK_DISABLE	(0<<1)

/* TRGCFG8: reg 0x3A RW */
#define AW8622X_BIT_TRGCFG8_TRG3_POR_LEV_BRK_MASK	(~(7<<5))
#define AW8622X_BIT_TRGCFG8_TRG3_POLAR_MASK		(~(1<<7))
#define AW8622X_BIT_TRGCFG8_TRG3_POLAR_NEG		(1<<7)
#define AW8622X_BIT_TRGCFG8_TRG3_POLAR_POS		(0<<7)
#define AW8622X_BIT_TRGCFG8_TRG3_MODE_MASK		(~(1<<6))
#define AW8622X_BIT_TRGCFG8_TRG3_MODE_LEVEL		(1<<6)
#define AW8622X_BIT_TRGCFG8_TRG3_MODE_EDGE		(0<<6)
#define AW8622X_BIT_TRGCFG8_TRG3_AUTO_BRK_MASK		(~(1<<5))
#define AW8622X_BIT_TRGCFG8_TRG3_AUTO_BRK_ENABLE	(1<<5)
#define AW8622X_BIT_TRGCFG8_TRG3_AUTO_BRK_DISABLE	(0<<5)
#define AW8622X_BIT_TRGCFG8_TRG_TRIG1_MODE_MASK		(~(3<<3))
#define AW8622X_BIT_TRGCFG8_PWM_LRA			(0<<3)
#define AW8622X_BIT_TRGCFG8_PWM_ERA			(1<<3)
#define AW8622X_BIT_TRGCFG8_TRIG1			(2<<3)
#define AW8622X_BIT_TRGCFG8_DISABLE			(3<<3)
#define AW8622X_BIT_TRGCFG8_TRG1_STOP_MASK		(~(1<<2))
#define AW8622X_BIT_TRGCFG8_TRG1_STOP			(1<<2)
#define AW8622X_BIT_TRGCFG8_TRG2_STOP_MASK		(~(1<<1))
#define AW8622X_BIT_TRGCFG8_TRG2_STOP			(1<<1)
#define AW8622X_BIT_TRGCFG8_TRG3_STOP_MASK		(~(1<<0))
#define AW8622X_BIT_TRGCFG8_TRG3_STOP			(1<<0)

/* GLBCFG4: reg 0x3E RW */
#define AW8622X_BIT_GLBCFG4_GO_PRIO_MASK		(~(3<<6))
#define AW8622X_BIT_GLBCFG4_TRG3_PRIO_MASK		(~(3<<4))
#define AW8622X_BIT_GLBCFG4_TRG2_PRIO_MASK		(~(3<<2))
#define AW8622X_BIT_GLBCFG4_TRG1_PRIO_MASK		(~(3<<0))

/* GLBRD5: reg 0x3F R0 */
/* GLB_STATE */
#define AW8622X_BIT_GLBRD5_STATE			(15<<0)
#define AW8622X_BIT_GLBRD5_STATE_STANDBY		(0<<0)
#define AW8622X_BIT_GLBRD5_STATE_WAKEUP			(1<<0)
#define AW8622X_BIT_GLBRD5_STATE_STARTUP		(2<<0)
#define AW8622X_BIT_GLBRD5_STATE_WAIT			(3<<0)
#define AW8622X_BIT_GLBRD5_STATE_CONT_GO		(6<<0)
#define AW8622X_BIT_GLBRD5_STATE_RAM_GO			(7<<0)
#define AW8622X_BIT_GLBRD5_STATE_RTP_GO			(8<<0)
#define AW8622X_BIT_GLBRD5_STATE_TRIG_GO		(9<<0)
#define AW8622X_BIT_GLBRD5_STATE_I2S_GO			(10<<0)
#define AW8622X_BIT_GLBRD5_STATE_BRAKE			(11<<0)
#define AW8622X_BIT_GLBRD5_STATE_END			(12<<0)
/* RAMADDRH: reg 0x40 RWS */
#define AW8622X_BIT_RAMADDRH_MASK			(~(63<<0))

/***************** SYSCTRL *****************/
/* SYSCTRL1: reg 0x43 RW */
#define AW8622X_BIT_SYSCTRL1_VBAT_MODE_MASK		(~(1<<7))
#define AW8622X_BIT_SYSCTRL1_VBAT_MODE_HW		(1<<7)
#define AW8622X_BIT_SYSCTRL1_VBAT_MODE_SW		(0<<7)
#define AW8622X_BIT_SYSCTRL1_PERP_MASK			(~(1<<6))
#define AW8622X_BIT_SYSCTRL1_PERP_ON			(1<<6)
#define AW8622X_BIT_SYSCTRL1_PERP_OFF			(0<<6)
#define AW8622X_BIT_SYSCTRL1_CLK_SEL_MASK		(~(3<<4))
#define AW8622X_BIT_SYSCTRL1_CLK_SEL_OSC		(1<<4)
#define AW8622X_BIT_SYSCTRL1_CLK_SEL_AUTO		(0<<4)
#define AW8622X_BIT_SYSCTRL1_RAMINIT_MASK		(~(1<<3))
#define AW8622X_BIT_SYSCTRL1_RAMINIT_ON			(1<<3)
#define AW8622X_BIT_SYSCTRL1_RAMINIT_OFF		(0<<3)
#define AW8622X_BIT_SYSCTRL1_EN_FIR_MASK		(~(1<<2))
#define AW8622X_BIT_SYSCTRL1_FIR_ENABLE			(0<<2)
#define AW8622X_BIT_SYSCTRL1_WAKE_MODE_MASK		(~(1<<1))
#define AW8622X_BIT_SYSCTRL1_WAKE_MODE_WAKEUP		(1<<1)
#define AW8622X_BIT_SYSCTRL1_WAKE_MODE_BST		(0<<1)
#define AW8622X_BIT_SYSCTRL1_RTP_CLK_MASK		(~(1<<0))
#define AW8622X_BIT_SYSCTRL1_RTP_PLL			(1<<0)
#define AW8622X_BIT_SYSCTRL1_RTP_OSC			(0<<0)

/* SYSCTRL2: reg 0x44 RW */
#define AW8622X_BIT_SYSCTRL2_WAKE_MASK			(~(1<<7))
#define AW8622X_BIT_SYSCTRL2_WAKE_ON			(1<<7)
#define AW8622X_BIT_SYSCTRL2_WAKE_OFF			(0<<7)
#define AW8622X_BIT_SYSCTRL2_STANDBY_MASK		(~(1<<6))
#define AW8622X_BIT_SYSCTRL2_STANDBY_ON			(1<<6)
#define AW8622X_BIT_SYSCTRL2_STANDBY_OFF		(0<<6)
#define AW8622X_BIT_SYSCTRL2_RTP_DLY_MASK		(~(3<<4))
#define AW8622X_BIT_SYSCTRL2_INTN_PIN_MASK		(~(1<<3))
#define AW8622X_BIT_SYSCTRL2_INTN			(1<<3)
#define AW8622X_BIT_SYSCTRL2_TRIG1			(0<<3)
#define AW8622X_BIT_SYSCTRL2_WCK_PIN_MASK		(~(1<<2))
#define AW8622X_BIT_SYSCTRL2_ENABLE_TRIG2		(1<<2)
#define AW8622X_BIT_SYSCTRL2_DISENABLE_TRIG2		(0<<2)
#define AW8622X_BIT_SYSCTRL2_WAVDAT_MODE_MASK		(~(3<<0))
#define AW8622X_BIT_SYSCTRL2_RATE			(3<<0)
#define AW8622X_BIT_SYSCTRL2_RATE_12K			(2<<0)
#define AW8622X_BIT_SYSCTRL2_RATE_24K			(0<<0)
#define AW8622X_BIT_SYSCTRL2_RATE_48K			(1<<0)

/* SYSCTRL7: reg 0x49 RW */
#define AW8622X_BIT_SYSCTRL7_GAIN_BYPASS_MASK		(~(1<<6))
#define AW8622X_BIT_SYSCTRL7_GAIN_CHANGEABLE		(1<<6)
#define AW8622X_BIT_SYSCTRL7_GAIN_FIXED			(0<<6)
#define AW8622X_BIT_SYSCTRL7_GAIN			(0x07)

#define AW8622X_BIT_SYSCTRL7_INT_EDGE_MODE_MASK		(~(1<<5))
#define AW8622X_BIT_SYSCTRL7_INT_EDGE_MODE_POS		(0<<5)
#define AW8622X_BIT_SYSCTRL7_INT_EDGE_MODE_BOTH		(1<<5)
#define AW8622X_BIT_SYSCTRL7_INT_MODE_MASK		(~(1<<4))
#define AW8622X_BIT_SYSCTRL7_INT_MODE_EDGE		(1<<4)
#define AW8622X_BIT_SYSCTRL7_INT_MODE_LEVEL		(0<<4)

#define AW8622X_BIT_SYSCTRL7_INTP_MASK			(~(1<<3))
#define AW8622X_BIT_SYSCTRL7_INTP_HIGH			(1<<3)
#define AW8622X_BIT_SYSCTRL7_INTP_LOW			(0<<3)
#define AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK		(~(7<<0))
#define AW8622X_BIT_SYSCTRL7_D2S_GAIN_1			(0<<0)
#define AW8622X_BIT_SYSCTRL7_D2S_GAIN_2			(1<<0)
#define AW8622X_BIT_SYSCTRL7_D2S_GAIN_4			(2<<0)
#define AW8622X_BIT_SYSCTRL7_D2S_GAIN_5			(3<<0)
#define AW8622X_BIT_SYSCTRL7_D2S_GAIN_8			(4<<0)
#define AW8622X_BIT_SYSCTRL7_D2S_GAIN_10		(5<<0)
#define AW8622X_BIT_SYSCTRL7_D2S_GAIN_20		(6<<0)
#define AW8622X_BIT_SYSCTRL7_D2S_GAIN_40		(7<<0)

/* PWMCFG1: reg 0x4C RW */
#define AW8622X_BIT_PWMCFG1_PRC_EN_MASK			(~(1<<7))
#define AW8622X_BIT_PWMCFG1_PRC_ENABLE			(1<<7)
#define AW8622X_BIT_PWMCFG1_PRC_DISABLE			(0<<7)
#define AW8622X_BIT_PWMCFG1_PRCTIME_MASK		(~(0x7F<<0))

/* PWMCFG2: reg 0x4D RW */
#define AW8622X_BIT_PWMCFG2_REF_SEL_MASK		(~(1<<5))
#define AW8622X_BIT_PWMCFG2_REF_SEL_TRIANGLE		(1<<5)
#define AW8622X_BIT_PWMCFG2_REF_SEL_SAWTOOTH		(0<<5)
#define AW8622X_BIT_PWMCFG2_PD_HWM_MASK			(~(1<<4))
#define AW8622X_BIT_PWMCFG2_PD_HWM_ON			(1<<4)
#define AW8622X_BIT_PWMCFG2_PWMOE_MASK			(~(1<<3))
#define AW8622X_BIT_PWMCFG2_PWMOE_ON			(1<<3)
#define AW8622X_BIT_PWMCFG2_PWMFRC_MASK			(~(7<<0))

/* PWMCFG3: reg 0x4E RW */
#define AW8622X_BIT_PWMCFG3_PR_EN_MASK			(~(1<<7))
#define AW8622X_BIT_PWMCFG3_PR_ENABLE			(1<<7)
#define AW8622X_BIT_PWMCFG3_PR_DISABLE			(0<<7)
#define AW8622X_BIT_PWMCFG3_PRLVL_MASK			(~(0x7F<<0))

/* DETCFG1: reg 0x51 RW */
#define AW8622X_BIT_DETCFG1_FTS_GO_MASK			(~(1<<7))
#define AW8622X_BIT_DETCFG1_FTS_GO_ENABLE		(1<<7)
#define AW8622X_BIT_DETCFG1_TEST_GO_MASK		(~(1<<6))
#define AW8622X_BIT_DETCFG1_TEST_GO_ENABLE		(1<<6)
#define AW8622X_BIT_DETCFG1_ADO_SLOT_MODE_MASK		(~(1<<5))
#define AW8622X_BIT_DETCFG1_ADO_SLOT_ADC_32		(1<<5)
#define AW8622X_BIT_DETCFG1_ADO_SLOT_ADC_256		(0<<5)
#define AW8622X_BIT_DETCFG1_RL_OS_MASK			(~(1<<4))
#define AW8622X_BIT_DETCFG1_RL				(1<<4)
#define AW8622X_BIT_DETCFG1_OS				(0<<4)
#define AW8622X_BIT_DETCFG1_PRCT_MODE_MASK		(~(1<<3))
#define AW8622X_BIT_DETCFG1_PRCT_MODE_INVALID		(1<<3)
#define AW8622X_BIT_DETCFG1_PRCT_MODE_VALID		(0<<3)
#define AW8622X_BIT_DETCFG1_CLK_ADC_MASK		(~(7<<0))
#define AW8622X_BIT_DETCFG1_CLK_ADC_12M			(0<<0)
#define AW8622X_BIT_DETCFG1_CLK_ADC_6M			(1<<0)
#define AW8622X_BIT_DETCFG1_CLK_ADC_3M			(2<<0)
#define AW8622X_BIT_DETCFG1_CLK_ADC_1M5			(3<<0)
#define AW8622X_BIT_DETCFG1_CLK_ADC_M75			(4<<0)
#define AW8622X_BIT_DETCFG1_CLK_ADC_M37			(5<<0)
#define AW8622X_BIT_DETCFG1_CLK_ADC_M18			(6<<0)
#define AW8622X_BIT_DETCFG1_CLK_ADC_M09			(7<<0)

/* DETCFG2: reg 0x52 RW */
#define AW8622X_BIT_DETCFG2_VBAT_GO_MASK		(~(1<<1))
#define AW8622X_BIT_DETCFG2_VABT_GO_ON			(1<<1)
#define AW8622X_BIT_DETCFG2_DIAG_GO_MASK		(~(1<<0))
#define AW8622X_BIT_DETCFG2_DIAG_GO_ON			(1<<0)

/* DET_LO: reg 0x57 RW */
#define AW8622X_BIT_DET_LO_TEST_MASK			(~(3<<6))
#define AW8622X_BIT_DET_LO_VBAT_MASK			(~(3<<4))
#define AW8622X_BIT_DET_LO_VBAT				(3<<4)
#define AW8622X_BIT_DET_LO_OS_MASK			(~(3<<2))
#define AW8622X_BIT_DET_LO_RL_MASK			(~(3<<0))
#define AW8622X_BIT_DET_LO_RL				(3<<0)

/* TRIMCFG1: reg:0x58 RW */
#define AW8622X_BIT_TRIMCFG1_RL_TRIM_SRC_MASK		(~(1<<6))
#define AW8622X_BIT_TRIMCFG1_RL_TRIM_SRC_REG		(1<<6)
#define AW8622X_BIT_TRIMCFG1_RL_TRIM_SRC_EFUSE		(0<<6)
#define AW8622X_BIT_TRIMCFG1_TRIM_RL_MASK		(~(63<<0))

/* TRIMCFG3: reg:0x5A RW */
#define AW8622X_BIT_TRIMCFG3_OSC_TRIM_SRC_MASK		(~(1<<7))
#define AW8622X_BIT_TRIMCFG3_OSC_TRIM_SRC_REG		(1<<7)
#define AW8622X_BIT_TRIMCFG3_OSC_TRIM_SRC_EFUSE		(0<<7)
#define AW8622X_BIT_TRIMCFG3_TRIM_LRA_MASK		(~(63<<0))

/* TRIMCFG4: reg:0x5B RW */
/* TRIM_OSC */

/* ANACFG8: reg:0x77 RW */
#define AW8622X_BIT_ANACFG8_TRTF_CTRL_HDRV_MASK		(~(1<<6))
#define AW8622X_BIT_ANACFG8_TRTF_CTRL_HDRV		(3<<6)

#endif
