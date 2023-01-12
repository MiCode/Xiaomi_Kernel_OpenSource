#ifndef _AW86214_H_
#define _AW86214_H_

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
#define AW86214_I2C_RETRIES		(5)
#define AW86214_I2C_RETRY_DELAY		(2)
#define AW86214_VBAT_REFER		(4200)
#define AW86214_VBAT_MIN		(3000)
#define AW86214_VBAT_MAX		(5500)
#define AW86214_SEQUENCER_LOOP_SIZE	(4)
#define AW86214_SEQUENCER_SIZE		(8)
#define AW86214_VBAT_FORMULA(code)	(6100 * (code) / 1024)
#define AW86214_LRA_FORMULA(lra_code)	(((lra_code) * 678 * 100) / (1024 * 10))
#define AW86214_STOP_DELAY_MIN		(2000)
#define AW86214_STOP_DELAY_MAX		(2500)
#define AW86214_VBAT_DELAY_MIN		(20000)
#define AW86214_VBAT_DELAY_MAX		(25000)
#define AW86214_F0_DELAY_MIN		(10000)
#define AW86214_F0_DELAY_MAX		(10500)
#define AW86214_LRA_DELAY_MIN		(30000)
#define AW86214_LRA_DELAY_MAX		(35000)
/*********************************************************
 *
 * enum
 *
 ********************************************************/
enum aw86214_flags {
	AW86214_FLAG_NONR = 0,
	AW86214_FLAG_SKIP_INTERRUPTS = 1,
};

enum aw86214_haptic_work_mode {
	AW86214_HAPTIC_STANDBY_MODE = 0,
	AW86214_HAPTIC_RAM_MODE = 1,
	AW86214_HAPTIC_RAM_LOOP_MODE = 2,
	AW86214_HAPTIC_CONT_MODE = 3,
	AW86214_HAPTIC_RTP_MODE = 4,
	AW86214_HAPTIC_TRIG_MODE = 5,
	AW86214_HAPTIC_NULL = 6,
};

enum aw86214_haptic_cont_vbat_comp_mode {
	AW86214_HAPTIC_CONT_VBAT_SW_ADJUST_MODE = 0,
	AW86214_HAPTIC_CONT_VBAT_HW_ADJUST_MODE = 1,
};

enum aw86214_haptic_ram_vbat_compensate_mode {
	AW86214_HAPTIC_RAM_VBAT_COMP_DISABLE = 0,
	AW86214_HAPTIC_RAM_VBAT_COMP_ENABLE = 1,
};

enum aw86214_haptic_f0_flag {
	AW86214_HAPTIC_LRA_F0 = 0,
	AW86214_HAPTIC_CALI_F0 = 1,
};

enum aw86214_sram_size_flag {
	AW86214_HAPTIC_SRAM_1K = 0,
	AW86214_HAPTIC_SRAM_2K = 1,
	AW86214_HAPTIC_SRAM_3K = 2,
};

enum aw86214_haptic_pwm_mode {
	AW86214_PWM_48K = 0,
	AW86214_PWM_24K = 1,
	AW86214_PWM_12K = 2,
};

enum aw86214_haptic_play {
	AW86214_HAPTIC_PLAY_NULL = 0,
	AW86214_HAPTIC_PLAY_ENABLE = 1,
	AW86214_HAPTIC_PLAY_STOP = 2,
	AW86214_HAPTIC_PLAY_GAIN = 8,
};

enum aw86214_haptic_cmd {
	AW86214_HAPTIC_CMD_NULL = 0,
	AW86214_HAPTIC_CMD_ENABLE = 1,
	AW86214_HAPTIC_CMD_HAPTIC = 0x0f,
	AW86214_HAPTIC_CMD_TP = 0x10,
	AW86214_HAPTIC_CMD_SYS = 0xf0,
	AW86214_HAPTIC_CMD_STOP = 255,
};

enum aw86214_haptic_cali_lra {
	AW86214_WRITE_ZERO = 0,
	AW86214_F0_CALI = 1,
};

enum aw86214_haptic_rtp_mode {
	AW86214_RTP_SHORT = 4,
	AW86214_RTP_LONG = 5,
	AW86214_RTP_SEGMENT = 6,
};

enum aw86214_awrw_flag {
	AW86214_WRITE = 0,
	AW86214_READ = 1,
};
/*********************************************************
 *
 * struct
 *
 ********************************************************/
struct aw86214_dts_info {
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
	unsigned int cont_bst_brk_gain;
	unsigned int d2s_gain;
	unsigned int bstcfg[5];
	unsigned int prctmode[3];
	unsigned int duration_time[3];
	unsigned int sine_array[4];
};

struct aw86214 {
	cdev_t vib_dev;

	unsigned char seq[AW86214_SEQUENCER_SIZE];
	unsigned char loop[AW86214_SEQUENCER_SIZE];
	unsigned char ram_init;
	unsigned char f0_cali_flag;
	unsigned char ram_vbat_compensate;
	unsigned char hwen_flag;
	unsigned char flags;
	unsigned char chipid;
	unsigned char play_mode;
	unsigned char activate_mode;
	unsigned char duration_time_size;

	char duration_time_flag;

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
	unsigned int ram_num;
	unsigned int f0;
	unsigned int cont_f0;
	unsigned int cont_drv1_lvl;
	unsigned int cont_drv2_lvl;
	unsigned int cont_brk_time;
	unsigned int cont_wait_num;
	unsigned int cont_drv1_time;
	unsigned int cont_drv2_time;
	unsigned int vbat;
	unsigned int lra;
	unsigned int ram_update_flag;
	unsigned int f0_cali_data;
	unsigned int sys_frequency;

	struct regmap *regmap;
	struct i2c_client *i2c;
	struct device *dev;
	struct mutex lock;
	struct mutex ram_lock;
	struct hrtimer timer;
	struct work_struct vibrator_work;
	struct delayed_work ram_work;
	struct aw86214_dts_info dts_info;
	struct ram ram;
	struct haptic_audio haptic_audio;
	struct aw_i2c_package aw_i2c_package;
};

struct aw86214_container {
	int len;
	unsigned char data[];
};

/*********************************************************
 *
 * extern
 *
 ********************************************************/
extern char aw86214_check_qualify_l(struct aw86214 *aw86214);
extern int aw86214_parse_dt_l(struct aw86214 *aw86214, struct device *dev,
			    struct device_node *np);
extern void aw86214_interrupt_setup_l(struct aw86214 *aw86214);
extern int aw86214_vibrator_init_l(struct aw86214 *aw86214);
extern int aw86214_haptic_init_l(struct aw86214 *aw86214);
extern int aw86214_ram_work_init_l(struct aw86214 *aw86214);
extern irqreturn_t aw86214_irq_l(int irq, void *data);
extern int aw86214_haptic_stop_l(struct aw86214 *aw86214);
extern struct attribute_group aw86214_vibrator_attribute_group_l;

/********************************************
 * Register List
 *******************************************/
#define AW86214_REG_ID		0x00
#define AW86214_REG_SYSST	0x01
#define AW86214_REG_SYSINT	0x02
#define AW86214_REG_SYSINTM	0x03
#define AW86214_REG_SYSST2	0x04
#define AW86214_REG_PLAYCFG2	0x07
#define AW86214_REG_PLAYCFG3	0x08
#define AW86214_REG_PLAYCFG4	0x09
#define AW86214_REG_WAVCFG1	0x0A
#define AW86214_REG_WAVCFG2	0x0B
#define AW86214_REG_WAVCFG3	0x0C
#define AW86214_REG_WAVCFG4	0x0D
#define AW86214_REG_WAVCFG5	0x0E
#define AW86214_REG_WAVCFG6	0x0F
#define AW86214_REG_WAVCFG7	0x10
#define AW86214_REG_WAVCFG8	0x11
#define AW86214_REG_WAVCFG9	0x12
#define AW86214_REG_WAVCFG10	0x13
#define AW86214_REG_WAVCFG11	0x14
#define AW86214_REG_WAVCFG12	0x15
#define AW86214_REG_WAVCFG13	0x16
#define AW86214_REG_CONTCFG1	0x18
#define AW86214_REG_CONTCFG2	0x19
#define AW86214_REG_CONTCFG3	0x1A
#define AW86214_REG_CONTCFG4	0x1B
#define AW86214_REG_CONTCFG5	0x1C
#define AW86214_REG_CONTCFG6	0x1D
#define AW86214_REG_CONTCFG7	0x1E
#define AW86214_REG_CONTCFG8	0x1F
#define AW86214_REG_CONTCFG9	0x20
#define AW86214_REG_CONTCFG10	0x21
#define AW86214_REG_CONTCFG11	0x22
#define AW86214_REG_CONTCFG13	0x24
#define AW86214_REG_CONTRD14	0x25
#define AW86214_REG_CONTRD15	0x26
#define AW86214_REG_CONTRD16	0x27
#define AW86214_REG_CONTRD17	0x28
#define AW86214_REG_RTPCFG1	0x2D
#define AW86214_REG_RTPCFG2	0x2E
#define AW86214_REG_RTPCFG3	0x2F
#define AW86214_REG_RTPCFG4	0x30
#define AW86214_REG_RTPCFG5	0x31
#define AW86214_REG_TRGCFG1	0x33
#define AW86214_REG_TRGCFG4	0x36
#define AW86214_REG_TRGCFG7	0x39
#define AW86214_REG_TRGCFG8	0x3A
#define AW86214_REG_GLBCFG4	0x3E
#define AW86214_REG_GLBRD5	0x3F
#define AW86214_REG_RAMADDRH	0x40
#define AW86214_REG_RAMADDRL	0x41
#define AW86214_REG_RAMDATA	0x42
#define AW86214_REG_SYSCTRL1	0x43
#define AW86214_REG_SYSCTRL2	0x44
#define AW86214_REG_SYSCTRL3	0x45
#define AW86214_REG_SYSCTRL4	0x46
#define AW86214_REG_SYSCTRL5	0x47
#define AW86214_REG_SYSCTRL6	0x48
#define AW86214_REG_SYSCTRL7	0x49
#define AW86214_REG_PWMCFG1	0x4C
#define AW86214_REG_PWMCFG3	0x4E
#define AW86214_REG_PWMCFG4	0x4F
#define AW86214_REG_DETCFG1	0x51
#define AW86214_REG_DETCFG2	0x52
#define AW86214_REG_DET_RL	0x53
#define AW86214_REG_DET_VBAT	0x55
#define AW86214_REG_DET_LO	0x57
#define AW86214_REG_TRIMCFG1	0x58
#define AW86214_REG_TRIMCFG3	0x5A
#define AW86214_REG_EFCFG5	0x60
#define AW86214_REG_ANACFG8	0x77
/******************************************************
 * Register Detail
 *****************************************************/
/* SYSST: reg 0x01 RO */
#define AW86214_BIT_SYSST_BST_SCPS			(1<<7)
#define AW86214_BIT_SYSST_BST_OVPS			(1<<6)
#define AW86214_BIT_SYSST_UVLS				(1<<5)
#define AW86214_BIT_SYSST_FF_AES			(1<<4)
#define AW86214_BIT_SYSST_FF_AFS			(1<<3)
#define AW86214_BIT_SYSST_OCDS				(1<<2)
#define AW86214_BIT_SYSST_OTS				(1<<1)
#define AW86214_BIT_SYSST_DONES				(1<<0)

/* SYSINT: reg 0x02 RC */
#define AW86214_BIT_SYSINT_BST_SCPI			(1<<7)
#define AW86214_BIT_SYSINT_BST_OVPI			(1<<6)
#define AW86214_BIT_SYSINT_UVLI				(1<<5)
#define AW86214_BIT_SYSINT_FF_AEI			(1<<4)
#define AW86214_BIT_SYSINT_FF_AFI			(1<<3)
#define AW86214_BIT_SYSINT_OCDI				(1<<2)
#define AW86214_BIT_SYSINT_OTI				(1<<1)
#define AW86214_BIT_SYSINT_DONEI			(1<<0)

/* SYSINTM: reg 0x03 RW */
#define AW86214_BIT_SYSINTM_BST_SCPM_MASK		(~(1<<7))
#define AW86214_BIT_SYSINTM_BST_SCPM_OFF		(1<<7)
#define AW86214_BIT_SYSINTM_BST_SCPM_ON			(0<<7)
#define AW86214_BIT_SYSINTM_BST_OVPM_MASK		(~(1<<6))
#define AW86214_BIT_SYSINTM_BST_OVPM_OFF		(1<<6)
#define AW86214_BIT_SYSINTM_BST_OVPM_ON			(0<<6)
#define AW86214_BIT_SYSINTM_UVLM_MASK			(~(1<<5))
#define AW86214_BIT_SYSINTM_UVLM_OFF			(1<<5)
#define AW86214_BIT_SYSINTM_UVLM_ON			(0<<5)
#define AW86214_BIT_SYSINTM_FF_AEM_MASK			(~(1<<4))
#define AW86214_BIT_SYSINTM_FF_AEM_OFF			(1<<4)
#define AW86214_BIT_SYSINTM_FF_AEM_ON			(0<<4)
#define AW86214_BIT_SYSINTM_FF_AFM_MASK			(~(1<<3))
#define AW86214_BIT_SYSINTM_FF_AFM_OFF			(1<<3)
#define AW86214_BIT_SYSINTM_FF_AFM_ON			(0<<3)
#define AW86214_BIT_SYSINTM_OCDM_MASK			(~(1<<2))
#define AW86214_BIT_SYSINTM_OCDM_OFF			(1<<2)
#define AW86214_BIT_SYSINTM_OCDM_ON			(0<<2)
#define AW86214_BIT_SYSINTM_OTM_MASK			(~(1<<1))
#define AW86214_BIT_SYSINTM_OTM_OFF			(1<<1)
#define AW86214_BIT_SYSINTM_OTM_ON			(0<<1)
#define AW86214_BIT_SYSINTM_DONEM_MASK			(~(1<<0))
#define AW86214_BIT_SYSINTM_DONEM_OFF			(1<<0)
#define AW86214_BIT_SYSINTM_DONEM_ON			(0<<0)

/* PLAYCFG3: reg 0x08 RW */
#define AW86214_BIT_PLAYCFG3_AUTO_BST_MASK		(~(1<<6))
#define AW86214_BIT_PLAYCFG3_AUTO_BST_ENABLE		(1<<6)
#define AW86214_BIT_PLAYCFG3_AUTO_BST_DISABLE		(0<<6)
#define AW86214_BIT_PLAYCFG3_STOP_MODE_MASK		(~(1<<5))
#define AW86214_BIT_PLAYCFG3_STOP_MODE_NOW		(1<<5)
#define AW86214_BIT_PLAYCFG3_STOP_MODE_LATER		(0<<5)
#define AW86214_BIT_PLAYCFG3_BRK_EN_MASK		(~(1<<2))
#define AW86214_BIT_PLAYCFG3_BRK			(1<<2)
#define AW86214_BIT_PLAYCFG3_BRK_ENABLE			(1<<2)
#define AW86214_BIT_PLAYCFG3_BRK_DISABLE		(0<<2)
#define AW86214_BIT_PLAYCFG3_PLAY_MODE_MASK		(~(3<<0))
#define AW86214_BIT_PLAYCFG3_PLAY_MODE_STOP		(3<<0)
#define AW86214_BIT_PLAYCFG3_PLAY_MODE_CONT		(2<<0)
#define AW86214_BIT_PLAYCFG3_PLAY_MODE_RTP		(1<<0)
#define AW86214_BIT_PLAYCFG3_PLAY_MODE_RAM		(0<<0)

/* PLAYCFG4: reg 0x09 RW */
#define AW86214_BIT_PLAYCFG4_STOP_MASK			(~(1<<1))
#define AW86214_BIT_PLAYCFG4_STOP_ON			(1<<1)
#define AW86214_BIT_PLAYCFG4_STOP_OFF			(0<<1)
#define AW86214_BIT_PLAYCFG4_GO_MASK			(~(1<<0))
#define AW86214_BIT_PLAYCFG4_GO_ON			(1<<0)
#define AW86214_BIT_PLAYCFG4_GO_OFF			(0<<0)

/* WAVCFG1-8: reg 0x0A - reg 0x11 RW */
#define AW86214_BIT_WAVCFG_SEQWAIT_MASK			(~(1<<7))
#define AW86214_BIT_WAVCFG_SEQWAIT_TIME			(1<<7)
#define AW86214_BIT_WAVCFG_SEQWAIT_NUMBER		(0<<7)

/* WAVCFG9-12: reg 0x12 - reg 0x15 RW */
#define AW86214_BIT_WAVLOOP_SEQ_ODD_MASK		(~(0x0F<<4))
#define AW86214_BIT_WAVLOOP_SEQ_ODD_INIFINITELY		(0x0F<<4)
#define AW86214_BIT_WAVLOOP_SEQ_EVEN_MASK		(~(0x0F<<0))
#define AW86214_BIT_WAVLOOP_SEQ_EVEN_INIFINITELY	(0x0F<<0)
#define AW86214_BIT_WAVLOOP_INIFINITELY			(0x0F<<0)

/* WAVCFG9: reg 0x12 RW */
#define AW86214_BIT_WAVCFG9_SEQ1LOOP_MASK		(~(0x0F<<4))
#define AW86214_BIT_WAVCFG9_SEQ1LOOP_INIFINITELY	(0x0F<<4)
#define AW86214_BIT_WAVCFG9_SEQ2LOOP_MASK		(~(0x0F<<0))
#define AW86214_BIT_WAVCFG9_SEQ2LOOP_INIFINITELY	(0x0F<<0)

/* WAVCFG10: reg 0x13 RW */
#define AW86214_BIT_WAVCFG10_SEQ3LOOP_MASK		(~(0x0F<<4))
#define AW86214_BIT_WAVCFG10_SEQ3LOOP_INIFINITELY	(0x0F<<4)
#define AW86214_BIT_WAVCFG10_SEQ4LOOP_MASK		(~(0x0F<<0))
#define AW86214_BIT_WAVCFG10_SEQ4LOOP_INIFINITELY	(0x0F<<0)

/* WAVCFG11: reg 0x14 RW */
#define AW86214_BIT_WAVCFG11_SEQ5LOOP_MASK		(~(0x0F<<4))
#define AW86214_BIT_WAVCFG11_SEQ5LOOP_INIFINITELY	(0x0F<<4)
#define AW86214_BIT_WAVCFG11_SEQ6LOOP_MASK		(~(0x0F<<0))
#define AW86214_BIT_WAVCFG11_SEQ6LOOP_INIFINITELY	(0x0F<<0)

/* WAVCFG12: reg 0x15 RW */
#define AW86214_BIT_WAVCFG12_SEQ7LOOP_MASK		(~(0x0F<<4))
#define AW86214_BIT_WAVCFG12_SEQ7LOOP_INIFINITELY	(0x0F<<4)
#define AW86214_BIT_WAVCFG12_SEQ8LOOP_MASK		(~(0x0F<<0))
#define AW86214_BIT_WAVCFG12_SEQ8LOOP_INIFINITELY	(0x0F<<0)

/***************** CONT *****************/
/* CONTCFG1: reg 0x18 RW */
#define AW86214_BIT_CONTCFG1_EDGE_FRE_MASK		(~(0x0F<<4))
#define AW86214_BIT_CONTCFG1_EN_F0_DET_MASK		(~(1<<3))
#define AW86214_BIT_CONTCFG1_F0_DET_ENABLE		(1<<3)
#define AW86214_BIT_CONTCFG1_F0_DET_DISABLE		(0<<3)
#define AW86214_BIT_CONTCFG1_MBRK_MASK			(~(1<<2))
#define AW86214_BIT_CONTCFG1_MBRK_ENABLE		(1<<2)
#define AW86214_BIT_CONTCFG1_MBRK_DISABLE		(0<<2)
#define AW86214_BIT_CONTCFG1_BRK_BST_MD_MASK		(~(1<<1))
#define AW86214_BIT_CONTCFG1_BRK_BST_MD_ENABLE		(1<<1)
#define AW86214_BIT_CONTCFG1_BRK_BST_MD_DISABLE		(0<<1)
#define AW86214_BIT_CONTCFG1_SIN_MODE_MASK		(~(1<<0))
#define AW86214_BIT_CONTCFG1_SIN_MODE_COS		(1<<0)
#define AW86214_BIT_CONTCFG1_SIN_MODE_SINE		(0<<0)

/* CONTCFG5: reg 0x1C RW */
#define AW86214_BIT_CONTCFG5_BST_BRK_GAIN_MASK		(~(0x0F<<4))
#define AW86214_BIT_CONTCFG5_BRK_GAIN_MASK		(~(0x0F<<0))

/* CONTCFG6: reg 0x1D RW */
#define AW86214_BIT_CONTCFG6_TRACK_EN_MASK		(~(1<<7))
#define AW86214_BIT_CONTCFG6_TRACK_ENABLE		(1<<7)
#define AW86214_BIT_CONTCFG6_TRACK_DISABLE		(0<<7)
#define AW86214_BIT_CONTCFG6_DRV1_LVL_MASK		(~(0x7F<<0))

/* CONTCFG7: reg 0x1E RW */
#define AW86214_BIT_CONTCFG7_DRV2_LVL_MASK		(~(0x7F<<0))

/* CONTCFG13: reg 0x24 RW */
#define AW86214_BIT_CONTCFG13_TSET_MASK			(~(0x0F<<4))
#define AW86214_BIT_CONTCFG13_BEME_SET_MASK		(~(0x0F<<0))

/***************** RTP *****************/
/* RTPCFG1: reg 0x2D RW */
#define AW86214_BIT_RTPCFG1_ADDRH_MASK			 (~(0x0F<<0))

#define AW86214_BIT_RTPCFG1_SRAM_SIZE_2K_MASK		 (~(1<<5))
#define AW86214_BIT_RTPCFG1_SRAM_SIZE_2K_EN		 (1<<5)
#define AW86214_BIT_RTPCFG1_SRAM_SIZE_2K_DIS	         (0<<5)

#define AW86214_BIT_RTPCFG1_SRAM_SIZE_1K_MASK		 (~(1<<4))
#define AW86214_BIT_RTPCFG1_SRAM_SIZE_1K_EN		 (1<<4)
#define AW86214_BIT_RTPCFG1_SRAM_SIZE_1K_DIS		 (0<<4)

/* GLBRD5: reg 0x3F R0 */
/* GLB_STATE */
#define AW86214_BIT_GLBRD5_STATE			(15<<0)
#define AW86214_BIT_GLBRD5_STATE_STANDBY		(0<<0)
#define AW86214_BIT_GLBRD5_STATE_WAKEUP			(1<<0)
#define AW86214_BIT_GLBRD5_STATE_STARTUP		(2<<0)
#define AW86214_BIT_GLBRD5_STATE_WAIT			(3<<0)
#define AW86214_BIT_GLBRD5_STATE_CONT_GO		(6<<0)
#define AW86214_BIT_GLBRD5_STATE_RAM_GO			(7<<0)
#define AW86214_BIT_GLBRD5_STATE_RTP_GO			(8<<0)
#define AW86214_BIT_GLBRD5_STATE_TRIG_GO		(9<<0)
#define AW86214_BIT_GLBRD5_STATE_I2S_GO			(10<<0)
#define AW86214_BIT_GLBRD5_STATE_BRAKE			(11<<0)
#define AW86214_BIT_GLBRD5_STATE_END			(12<<0)
/* RAMADDRH: reg 0x40 RWS */
#define AW86214_BIT_RAMADDRH_MASK			(~(63<<0))

/* RAMADDRL: reg 0x41 RWS */
/* RAMADDRL */

/* RAMDATA: reg 0x42 RWS */
/* RAMDATA */

/***************** SYSCTRL *****************/
/* SYSCTRL1: reg 0x43 RW */
#define AW86214_BIT_SYSCTRL1_VBAT_MODE_MASK		(~(1<<7))
#define AW86214_BIT_SYSCTRL1_VBAT_MODE_HW		(1<<7)
#define AW86214_BIT_SYSCTRL1_VBAT_MODE_SW		(0<<7)
#define AW86214_BIT_SYSCTRL1_PERP_MASK			(~(1<<6))
#define AW86214_BIT_SYSCTRL1_PERP_ON			(1<<6)
#define AW86214_BIT_SYSCTRL1_PERP_OFF			(0<<6)
#define AW86214_BIT_SYSCTRL1_CLK_SEL_MASK		(~(3<<4))
#define AW86214_BIT_SYSCTRL1_CLK_SEL_OSC		(1<<4)
#define AW86214_BIT_SYSCTRL1_CLK_SEL_AUTO		(0<<4)
#define AW86214_BIT_SYSCTRL1_RAMINIT_MASK		(~(1<<3))
#define AW86214_BIT_SYSCTRL1_RAMINIT_ON			(1<<3)
#define AW86214_BIT_SYSCTRL1_RAMINIT_OFF		(0<<3)
#define AW86214_BIT_SYSCTRL1_EN_FIR_MASK		(~(1<<2))
#define AW86214_BIT_SYSCTRL1_FIR_ENABLE			(0<<2)
#define AW86214_BIT_SYSCTRL1_WAKE_MODE_MASK		(~(1<<1))
#define AW86214_BIT_SYSCTRL1_WAKE_MODE_WAKEUP		(1<<1)
#define AW86214_BIT_SYSCTRL1_WAKE_MODE_BST		(0<<1)
#define AW86214_BIT_SYSCTRL1_RTP_CLK_MASK		(~(1<<0))
#define AW86214_BIT_SYSCTRL1_RTP_PLL			(1<<0)
#define AW86214_BIT_SYSCTRL1_RTP_OSC			(0<<0)

/* SYSCTRL2: reg 0x44 RW */
#define AW86214_BIT_SYSCTRL2_WAKE_MASK			(~(1<<7))
#define AW86214_BIT_SYSCTRL2_WAKE_ON			(1<<7)
#define AW86214_BIT_SYSCTRL2_WAKE_OFF			(0<<7)
#define AW86214_BIT_SYSCTRL2_STANDBY_MASK		(~(1<<6))
#define AW86214_BIT_SYSCTRL2_STANDBY_ON			(1<<6)
#define AW86214_BIT_SYSCTRL2_STANDBY_OFF		(0<<6)
#define AW86214_BIT_SYSCTRL2_RTP_DLY_MASK		(~(3<<4))
#define AW86214_BIT_SYSCTRL2_PLL_PIN_MASK		(~(1<<3))
#define AW86214_BIT_SYSCTRL2_PLL_PIN_TEST		(1<<3)
#define AW86214_BIT_SYSCTRL2_I2S_PIN_MASK		(~(1<<2))
#define AW86214_BIT_SYSCTRL2_I2S_PIN_I2S		(1<<2)
#define AW86214_BIT_SYSCTRL2_I2S_PIN_TRIG		(0<<2)
#define AW86214_BIT_SYSCTRL2_WAVDAT_MODE_MASK		(~(3<<0))
#define AW86214_BIT_SYSCTRL2_RATE_12K			(2<<0)
#define AW86214_BIT_SYSCTRL2_RATE_24K			(0<<0)
#define AW86214_BIT_SYSCTRL2_RATE_48K			(1<<0)

/* SYSCTRL7: reg 0x49 RW */
#define AW86214_BIT_SYSCTRL7_GAIN_BYPASS_MASK		(~(1<<6))
#define AW86214_BIT_SYSCTRL7_GAIN_CHANGEABLE		(1<<6)
#define AW86214_BIT_SYSCTRL7_GAIN_FIXED			(0<<6)

#define AW86214_BIT_SYSCTRL7_INT_EDGE_MODE_MASK		(~(1<<5))
#define AW86214_BIT_SYSCTRL7_INT_EDGE_MODE_POS		(0<<5)
#define AW86214_BIT_SYSCTRL7_INT_EDGE_MODE_BOTH		(1<<5)
#define AW86214_BIT_SYSCTRL7_INT_MODE_MASK		(~(1<<4))
#define AW86214_BIT_SYSCTRL7_INT_MODE_EDGE		(1<<4)
#define AW86214_BIT_SYSCTRL7_INT_MODE_LEVEL		(0<<4)

#define AW86214_BIT_SYSCTRL7_INTP_MASK			(~(1<<3))
#define AW86214_BIT_SYSCTRL7_INTP_HIGH			(1<<3)
#define AW86214_BIT_SYSCTRL7_INTP_LOW			(0<<3)
#define AW86214_BIT_SYSCTRL7_D2S_GAIN_MASK		(~(7<<0))
#define AW86214_BIT_SYSCTRL7_D2S_GAIN_1			(0<<0)
#define AW86214_BIT_SYSCTRL7_D2S_GAIN_2			(1<<0)
#define AW86214_BIT_SYSCTRL7_D2S_GAIN_4			(2<<0)
#define AW86214_BIT_SYSCTRL7_D2S_GAIN_8			(3<<0)
#define AW86214_BIT_SYSCTRL7_D2S_GAIN_10		(4<<0)
#define AW86214_BIT_SYSCTRL7_D2S_GAIN_16		(5<<0)
#define AW86214_BIT_SYSCTRL7_D2S_GAIN_20		(6<<0)
#define AW86214_BIT_SYSCTRL7_D2S_GAIN_40		(7<<0)

/* PWMCFG1: reg 0x4C RW */
#define AW86214_BIT_PWMCFG1_PRC_EN_MASK			(~(1<<7))
#define AW86214_BIT_PWMCFG1_PRC_ENABLE			(1<<7)
#define AW86214_BIT_PWMCFG1_PRC_DISABLE			(0<<7)
#define AW86214_BIT_PWMCFG1_PRCTIME_MASK		(~(0x7F<<0))

/* PWMCFG2: reg 0x4D RW */
#define AW86214_BIT_PWMCFG2_REF_SEL_MASK		(~(1<<5))
#define AW86214_BIT_PWMCFG2_REF_SEL_TRIANGLE		(1<<5)
#define AW86214_BIT_PWMCFG2_REF_SEL_SAWTOOTH		(0<<5)
#define AW86214_BIT_PWMCFG2_PD_HWM_MASK			(~(1<<4))
#define AW86214_BIT_PWMCFG2_PD_HWM_ON			(1<<4)
#define AW86214_BIT_PWMCFG2_PWMOE_MASK			(~(1<<3))
#define AW86214_BIT_PWMCFG2_PWMOE_ON			(1<<3)
#define AW86214_BIT_PWMCFG2_PWMFRC_MASK			(~(7<<0))

/* PWMCFG3: reg 0x4E RW */
#define AW86214_BIT_PWMCFG3_PR_EN_MASK			(~(1<<7))
#define AW86214_BIT_PWMCFG3_PR_ENABLE			(1<<7)
#define AW86214_BIT_PWMCFG3_PR_DISABLE			(0<<7)
#define AW86214_BIT_PWMCFG3_PRLVL_MASK			(~(0x7F<<0))

/* DETCFG1: reg 0x51 RW */
#define AW86214_BIT_DETCFG1_FTS_GO_MASK			(~(1<<7))
#define AW86214_BIT_DETCFG1_FTS_GO_ENABLE		(1<<7)
#define AW86214_BIT_DETCFG1_TEST_GO_MASK		(~(1<<6))
#define AW86214_BIT_DETCFG1_TEST_GO_ENABLE		(1<<6)
#define AW86214_BIT_DETCFG1_ADO_SLOT_MODE_MASK		(~(1<<5))
#define AW86214_BIT_DETCFG1_ADO_SLOT_ADC_32		(1<<5)
#define AW86214_BIT_DETCFG1_ADO_SLOT_ADC_256		(0<<5)
#define AW86214_BIT_DETCFG1_RL_OS_MASK			(~(1<<4))
#define AW86214_BIT_DETCFG1_RL				(1<<4)
#define AW86214_BIT_DETCFG1_OS				(0<<4)
#define AW86214_BIT_DETCFG1_PRCT_MODE_MASK		(~(1<<3))
#define AW86214_BIT_DETCFG1_PRCT_MODE_INVALID		(1<<3)
#define AW86214_BIT_DETCFG1_PRCT_MODE_VALID		(0<<3)
#define AW86214_BIT_DETCFG1_CLK_ADC_MASK		(~(7<<0))
#define AW86214_BIT_DETCFG1_CLK_ADC_12M			(0<<0)
#define AW86214_BIT_DETCFG1_CLK_ADC_6M			(1<<0)
#define AW86214_BIT_DETCFG1_CLK_ADC_3M			(2<<0)
#define AW86214_BIT_DETCFG1_CLK_ADC_1M5			(3<<0)
#define AW86214_BIT_DETCFG1_CLK_ADC_M75			(4<<0)
#define AW86214_BIT_DETCFG1_CLK_ADC_M37			(5<<0)
#define AW86214_BIT_DETCFG1_CLK_ADC_M18			(6<<0)
#define AW86214_BIT_DETCFG1_CLK_ADC_M09			(7<<0)

/* DETCFG2: reg 0x52 RW */
#define AW86214_BIT_DETCFG2_VBAT_GO_MASK		(~(1<<1))
#define AW86214_BIT_DETCFG2_VABT_GO_ON			(1<<1)
#define AW86214_BIT_DETCFG2_DIAG_GO_MASK		(~(1<<0))
#define AW86214_BIT_DETCFG2_DIAG_GO_ON			(1<<0)

/* DET_LO: reg 0x57 RW */
#define AW86214_BIT_DET_LO_TEST_MASK			(~(3<<6))
#define AW86214_BIT_DET_LO_VBAT_MASK			(~(3<<4))
#define AW86214_BIT_DET_LO_OS_MASK			(~(3<<2))
#define AW86214_BIT_DET_LO_RL_MASK			(~(3<<0))
#define AW86214_BIT_DET_LO_VBAT				(3<<4)
#define AW86214_BIT_DET_LO_RL				(3<<0)

/* TRIMCFG1: reg:0x58 RW */
#define AW86214_BIT_TRIMCFG1_RL_TRIM_SRC_MASK		(~(1<<6))
#define AW86214_BIT_TRIMCFG1_RL_TRIM_SRC_REG		(1<<6)
#define AW86214_BIT_TRIMCFG1_RL_TRIM_SRC_EFUSE		(0<<6)
#define AW86214_BIT_TRIMCFG1_TRIM_RL_MASK		(~(63<<0))

/* TRIMCFG3: reg:0x5A RW */
#define AW86214_BIT_TRIMCFG3_OSC_TRIM_SRC_MASK		(~(1<<7))
#define AW86214_BIT_TRIMCFG3_OSC_TRIM_SRC_REG		(1<<7)
#define AW86214_BIT_TRIMCFG3_OSC_TRIM_SRC_EFUSE		(0<<7)
#define AW86214_BIT_TRIMCFG3_LRA_TRIM_SRC_MASK		(~(1<<6))
#define AW86214_BIT_TRIMCFG3_LRA_TRIM_SRC_REG		(1<<6)
#define AW86214_BIT_TRIMCFG3_LRA_TRIM_SRC_EFUSE		(0<<6)
#define AW86214_BIT_TRIMCFG3_TRIM_LRA_MASK		(~(63<<0))

#endif
