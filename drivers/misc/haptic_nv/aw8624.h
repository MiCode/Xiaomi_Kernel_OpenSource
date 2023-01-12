#ifndef _AW8624_H_
#define _AW8624_H_

#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include "haptic_nv.h"

/*********************************************************
 *
 * marco
 *
 ********************************************************/
#define AW8624_I2C_RETRIES			(2)
#define AW8624_RTP_NAME_MAX			(64)
#define AW8624_SEQUENCER_SIZE			(8)
#define AW8624_SEQUENCER_LOOP_SIZE		(4)
#define AW8624_OSC_CALIBRATION_T_LENGTH		(5100000)
#define AW8624_PM_QOS_VALUE_VB			(400)
#define AW8624_VBAT_REFER			(4200)
#define AW8624_VBAT_MIN				(3000)
#define AW8624_VBAT_MAX				(4500)
#define AW8624_HAPTIC_NAME			("aw8624_haptic")
#define AW8624_SET_AEADDR_H(addr)		((((addr) >> 1) >> 8))
#define AW8624_SET_AEADDR_L(addr)		(((addr) >> 1) & 0x00FF)
#define AW8624_SET_AFADDR_H(addr)		(((addr) - ((addr) >> 2)) >> 8)
#define AW8624_SET_AFADDR_L(addr)		(((addr) - ((addr) >> 2)) & 0x00FF)
#define AW8624_F0_FORMULA(f0_reg, f0_coeff)	(1000000000 / ((f0_reg) * (f0_coeff)))
#define AW8624_VBAT_FORMULA(reg_val)		(6100 * (reg_val) / 256)
#define AW8624_LRA_FORMULA(reg_val)		(298 * (reg_val))
#define AW8624_VBAT_DELAY_MIN			(2000)
#define AW8624_VBAT_DELAY_MAX			(2500)
#define AW8624_LRA_DELAY_MIN			(3000)
#define AW8624_LRA_DELAY_MAX			(3500)
#define AW8624_STOP_DELAY_MIN			(2000)
#define AW8624_STOP_DELAY_MAX			(2500)
#define AW8624_F0_DELAY_MIN			(10000)
#define AW8624_F0_DELAY_MAX			(10500)
#define AW8624_MUL_GET_F0_RANGE			(150)
#define AW8624_MUL_GET_F0_NUM			(3)
/*********************************************************
 *
 * enum
 *
 ********************************************************/
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

enum aw8624_haptic_play {
	AW8624_HAPTIC_PLAY_NULL = 0,
	AW8624_HAPTIC_PLAY_ENABLE = 1,
	AW8624_HAPTIC_PLAY_STOP = 2,
	AW8624_HAPTIC_PLAY_GAIN = 8,
};

enum aw8624_haptic_cmd {
	AW8624_HAPTIC_CMD_NULL = 0,
	AW8624_HAPTIC_CMD_ENABLE = 1,
	AW8624_HAPTIC_CMD_HAPTIC = 0x0f,
	AW8624_HAPTIC_CMD_TP = 0x10,
	AW8624_HAPTIC_CMD_SYS = 0xf0,
	AW8624_HAPTIC_CMD_STOP = 255,
};
enum aw8624_haptic_work_mode {
	AW8624_HAPTIC_STANDBY_MODE = 0,
	AW8624_HAPTIC_RAM_MODE = 1,
	AW8624_HAPTIC_RAM_LOOP_MODE = 2,
	AW8624_HAPTIC_CONT_MODE = 3,
	AW8624_HAPTIC_RTP_MODE = 4,
	AW8624_HAPTIC_TRIG_MODE = 5,
	AW8624_HAPTIC_NULL = 6,
};

enum aw8624_haptic_bst_mode {
	AW8624_HAPTIC_BYPASS_MODE = 0,
	AW8624_HAPTIC_BOOST_MODE = 1,
};

enum aw8624_haptic_vbat_comp_mode {
	AW8624_HAPTIC_VBAT_SW_COMP_MODE = 0,
	AW8624_HAPTIC_VBAT_HW_COMP_MODE = 1,
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

enum aw8624_haptic_cali_lra {
	AW8624_HAPTIC_ZERO = 0,
	AW8624_HAPTIC_F0_CALI_LRA = 1,
	AW8624_HAPTIC_RTP_CALI_LRA = 2,
};

enum aw8624_haptic_pin {
	AW8624_TRIG1 = 0,
	AW8624_IRQ = 1,
};

enum aw8624_awrw_flag {
	AW8624_WRITE = 0,
	AW8624_READ = 1,
};

enum aw8624_pwm_clk {
	AW8624_CLK_24K = 2,
	AW8624_CLK_12K = 3,
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

struct aw8624_trig {
	unsigned char trig_enable;
	unsigned char trig_edge;
	unsigned char trig_polar;
	unsigned char pos_sequence;
	unsigned char neg_sequence;
};

struct aw8624_dts_info {
	int aw8624_lk_f0_cali;
	int aw8624_mode;
	int aw8624_f0_pre;
	int aw8624_f0_cali_percen;
	int aw8624_cont_drv_lvl;
	int aw8624_cont_drv_lvl_ov;
	int aw8624_cont_td;
	int aw8624_cont_zc_thr;
	int aw8624_cont_num_brk;
	int aw8624_f0_coeff;
	int aw8624_duration_time[3];
	int aw8624_cont_brake[3][8];
	int aw8624_bemf_config[4];
	int aw8624_sw_brake[2];
	int aw8624_wavseq[16];
	int aw8624_wavloop[10];
	int aw8624_td_brake[3];
	int aw8624_tset;
	unsigned int aw8624_f0_trace_parameter[4];
	unsigned int trig_config[5];
};

struct aw8624 {
	bool haptic_ready;
	bool audio_ready;
	bool IsUsedIRQ;
	bool ram_update_delay;

	ktime_t current_t;
	ktime_t pre_enter_t;
	ktime_t kstart, kend;

	cdev_t vib_dev;

	unsigned char hwen_flag;
	unsigned char flags;
	unsigned char chipid;
	unsigned char chipid_flag;
	unsigned char singlecycle;
	unsigned char play_mode;
	unsigned char activate_mode;
	unsigned char auto_boost;
	unsigned char duration_time_size;
	unsigned char cont_drv_lvl;
	unsigned char cont_drv_lvl_ov;
	unsigned char cont_num_brk;
	unsigned char max_pos_beme;
	unsigned char max_neg_beme;
	unsigned char f0_cali_flag;
	unsigned char ram_vbat_comp;
	unsigned char rtp_init;
	unsigned char ram_init;
	unsigned char rtp_routine_on;
	unsigned char seq[AW8624_SEQUENCER_SIZE];
	unsigned char loop[AW8624_SEQUENCER_SIZE];

	char duration_time_flag;

	int state;
	int duration;
	int amplitude;
	int index;
	int vmax;
	int gain;
	int f0_value;
	int pre_haptic_number;
	int reset_gpio;
	int pdlcen_gpio;
	int irq_gpio;
	int reset_gpio_ret;
	int irq_gpio_ret;

	unsigned int f0;
	unsigned int f0_pre;
	unsigned int cont_td;
	unsigned int cont_f0;
	unsigned int cont_zc_thr;
	unsigned int timeval_flags;
	unsigned int osc_cali_flag;
	unsigned int theory_time;
	unsigned int rtp_len;
	unsigned int gun_type;
	unsigned int bullet_nr;
	unsigned int rtp_cnt;
	unsigned int rtp_file_num;
	unsigned int vbat;
	unsigned int lra;
	unsigned int interval_us;
	unsigned int ramupdate_flag;
	unsigned int rtpupdate_flag;
	unsigned int osc_cali_run;
	unsigned int lra_calib_data;
	unsigned int f0_calib_data;
	unsigned long int microsecond;

	struct haptic_audio haptic_audio;
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct device *dev;
	struct input_dev *input;
	struct aw8624_container *rtp_container;
	struct mutex lock;
	struct mutex rtp_lock;
	struct mutex ram_lock;
	struct hrtimer timer;
	struct work_struct vibrator_work;
	struct work_struct irq_work;
	struct work_struct rtp_work;
	struct delayed_work ram_work;
	struct delayed_work stop_work;
	struct fileops fileops;
	struct ram ram;
	struct aw8624_trig trig;
	struct aw_i2c_package aw_i2c_package;
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

/*********************************************************
 *
 * extern
 *
 ********************************************************/
extern int aw8624_parse_dt_l(struct aw8624 *aw8624, struct device *dev,
		struct device_node *np);
extern void aw8624_interrupt_setup_l(struct aw8624 *aw8624);
extern int aw8624_vibrator_init_l(struct aw8624 *aw8624);
extern int aw8624_haptic_init_l(struct aw8624 *aw8624);
extern int aw8624_ram_init_l(struct aw8624 *aw8624);
extern irqreturn_t aw8624_irq_l(int irq, void *data);
extern int aw8624_haptic_stop_l(struct aw8624 *aw8624);
extern struct attribute_group aw8624_vibrator_attribute_group_l;

/********************************************
 * Register List
 *******************************************/
#define AW8624_REG_ID			(0x00)
#define AW8624_REG_SYSST		(0x01)
#define AW8624_REG_SYSINT		(0x02)
#define AW8624_REG_SYSINTM		(0x03)
#define AW8624_REG_SYSCTRL		(0x04)
#define AW8624_REG_GO			(0x05)
#define AW8624_REG_RTP_DATA		(0x06)
#define AW8624_REG_WAVSEQ1		(0x07)
#define AW8624_REG_WAVSEQ2		(0x08)
#define AW8624_REG_WAVSEQ3		(0x09)
#define AW8624_REG_WAVSEQ4		(0x0a)
#define AW8624_REG_WAVSEQ5		(0x0b)
#define AW8624_REG_WAVSEQ6		(0x0c)
#define AW8624_REG_WAVSEQ7		(0x0d)
#define AW8624_REG_WAVSEQ8		(0x0e)
#define AW8624_REG_WAVLOOP1		(0x0f)
#define AW8624_REG_WAVLOOP2		(0x10)
#define AW8624_REG_WAVLOOP3		(0x11)
#define AW8624_REG_WAVLOOP4		(0x12)
#define AW8624_REG_MANLOOP		(0x13)
#define AW8624_REG_TRG1_SEQP		(0x14)
#define AW8624_REG_TRG1_SEQN		(0x17)
#define AW8624_REG_TRG_CFG1		(0x1b)
#define AW8624_REG_TRG_CFG2		(0x1c)
#define AW8624_REG_DBGCTRL		(0x20)
#define AW8624_REG_BASE_ADDRH		(0x21)
#define AW8624_REG_BASE_ADDRL		(0x22)
#define AW8624_REG_FIFO_AEH		(0x23)
#define AW8624_REG_FIFO_AEL		(0x24)
#define AW8624_REG_FIFO_AFH		(0x25)
#define AW8624_REG_FIFO_AFL		(0x26)
#define AW8624_REG_DATCTRL		(0x2b)
#define AW8624_REG_PWMPRC		(0x2d)
#define AW8624_REG_PWMDBG		(0x2e)
#define AW8624_REG_DBGSTAT		(0x30)
#define AW8624_REG_WAVECTRL		(0x31)
#define AW8624_REG_BRAKE0_CTRL		(0x32)
#define AW8624_REG_BRAKE1_CTRL		(0x33)
#define AW8624_REG_BRAKE2_CTRL		(0x34)
#define AW8624_REG_BRAKE_NUM		(0x35)
#define AW8624_REG_ANACTRL		(0x38)
#define AW8624_REG_SW_BRAKE		(0x39)
#define AW8624_REG_DATDBG		(0x3b)
#define AW8624_REG_PRLVL		(0x3e)
#define AW8624_REG_PRTIME		(0x3f)
#define AW8624_REG_RAMADDRH		(0x40)
#define AW8624_REG_RAMADDRL		(0x41)
#define AW8624_REG_RAMDATA		(0x42)
#define AW8624_REG_BRA_MAX_NUM		(0x44)
#define AW8624_REG_GLB_STATE		(0x47)
#define AW8624_REG_CONT_CTRL		(0x48)
#define AW8624_REG_F_PRE_H		(0x49)
#define AW8624_REG_F_PRE_L		(0x4a)
#define AW8624_REG_TD_H			(0x4b)
#define AW8624_REG_TD_L			(0x4c)
#define AW8624_REG_TSET			(0x4d)
#define AW8624_REG_THRS_BRA_END		(0x4f)
#define AW8624_REG_EF_RDATAH		(0x55)
#define AW8624_REG_TRIM_LRA		(0x5b)
#define AW8624_REG_R_SPARE		(0x5d)
#define AW8624_REG_D2SCFG		(0x5e)
#define AW8624_REG_DETCTRL		(0x5f)
#define AW8624_REG_RLDET		(0x60)
#define AW8624_REG_OSDET		(0x61)
#define AW8624_REG_VBATDET		(0x62)
#define AW8624_REG_ADCTEST		(0x66)
#define AW8624_REG_F_LRA_F0_H		(0x68)
#define AW8624_REG_F_LRA_F0_L		(0x69)
#define AW8624_REG_F_LRA_CONT_H		(0x6a)
#define AW8624_REG_F_LRA_CONT_L		(0x6b)
#define AW8624_REG_WAIT_VOL_MP		(0x6e)
#define AW8624_REG_WAIT_VOL_MN		(0x6f)
#define AW8624_REG_ZC_THRSH_H		(0x72)
#define AW8624_REG_ZC_THRSH_L		(0x73)
#define AW8624_REG_BEMF_VTHH_H		(0x74)
#define AW8624_REG_BEMF_VTHH_L		(0x75)
#define AW8624_REG_BEMF_VTHL_H		(0x76)
#define AW8624_REG_BEMF_VTHL_L		(0x77)
#define AW8624_REG_BEMF_NUM		(0x78)
#define AW8624_REG_DRV_TIME		(0x79)
#define AW8624_REG_TIME_NZC		(0x7a)
#define AW8624_REG_DRV_LVL		(0x7b)
#define AW8624_REG_DRV_LVL_OV		(0x7c)
#define AW8624_REG_NUM_F0_1		(0x7d)
#define AW8624_REG_NUM_F0_2		(0x7e)
#define AW8624_REG_NUM_F0_3		(0x7f)
/******************************************************
 * Register Detail
 *****************************************************/
 /* SYSST  0x01 */
#define AW8624_BIT_SYSST_OVS				(1<<6)
#define AW8624_BIT_SYSST_UVLS				(1<<5)
#define AW8624_BIT_SYSST_FF_AES				(1<<4)
#define AW8624_BIT_SYSST_FF_AFS				(1<<3)
#define AW8624_BIT_SYSST_OCDS				(1<<2)
#define AW8624_BIT_SYSST_OTS				(1<<1)
#define AW8624_BIT_SYSST_DONES				(1<<0)

 /* SYSINT  0x02 */
#define AW8624_BIT_SYSINT_OVI				(1<<6)
#define AW8624_BIT_SYSINT_UVLI				(1<<5)
#define AW8624_BIT_SYSINT_FF_AEI			(1<<4)
#define AW8624_BIT_SYSINT_FF_AFI			(1<<3)
#define AW8624_BIT_SYSINT_OCDI				(1<<2)
#define AW8624_BIT_SYSINT_OTI				(1<<1)
#define AW8624_BIT_SYSINT_DONEI				(1<<0)

 /* SYSINTM 0x03 */
#define AW8624_BIT_SYSINTM_OV_MASK			(~(1<<6))
#define AW8624_BIT_SYSINTM_OV_OFF			(1<<6)
#define AW8624_BIT_SYSINTM_OV_EN			(0<<6)
#define AW8624_BIT_SYSINTM_UVLO_MASK			(~(1<<5))
#define AW8624_BIT_SYSINTM_UVLO_OFF			(1<<5)
#define AW8624_BIT_SYSINTM_UVLO_EN			(0<<5)
#define AW8624_BIT_SYSINTM_FF_AE_MASK			(~(1<<4))
#define AW8624_BIT_SYSINTM_FF_AE_OFF			(1<<4)
#define AW8624_BIT_SYSINTM_FF_AE_EN			(0<<4)
#define AW8624_BIT_SYSINTM_FF_AF_MASK			(~(1<<3))
#define AW8624_BIT_SYSINTM_FF_AF_OFF			(1<<3)
#define AW8624_BIT_SYSINTM_FF_AF_EN			(0<<3)
#define AW8624_BIT_SYSINTM_OCD_MASK			(~(1<<2))
#define AW8624_BIT_SYSINTM_OCD_OFF			(1<<2)
#define AW8624_BIT_SYSINTM_OCD_EN			(0<<2)
#define AW8624_BIT_SYSINTM_OT_MASK			(~(1<<1))
#define AW8624_BIT_SYSINTM_OT_OFF			(1<<1)
#define AW8624_BIT_SYSINTM_OT_EN			(0<<1)
#define AW8624_BIT_SYSINTM_DONE_MASK			(~(1<<0))
#define AW8624_BIT_SYSINTM_DONE_OFF			(1<<0)
#define AW8624_BIT_SYSINTM_DONE_EN			(0<<0)

 /* SYSCTRL 0x04 */
#define AW8624_BIT_SYSCTRL_WAVDAT_MODE_MASK		(~(3<<6))
#define AW8624_BIT_SYSCTRL_WAVDAT_MODE_4X		(3<<6)
#define AW8624_BIT_SYSCTRL_WAVDAT_MODE_2X		(0<<6)
#define AW8624_BIT_SYSCTRL_WAVDAT_MODE_1X		(1<<6)
#define AW8624_BIT_SYSCTRL_RAMINIT_MASK			(~(1<<5))
#define AW8624_BIT_SYSCTRL_RAMINIT_EN			(1<<5)
#define AW8624_BIT_SYSCTRL_RAMINIT_OFF			(0<<5)
#define AW8624_BIT_SYSCTRL_PLAY_MODE_MASK		(~(3<<2))
#define AW8624_BIT_SYSCTRL_PLAY_MODE_CONT		(2<<2)
#define AW8624_BIT_SYSCTRL_PLAY_MODE_RTP		(1<<2)
#define AW8624_BIT_SYSCTRL_PLAY_MODE_RAM		(0<<2)
#define AW8624_BIT_SYSCTRL_WORK_MODE_MASK		(~(1<<0))
#define AW8624_BIT_SYSCTRL_STANDBY			(1<<0)
#define AW8624_BIT_SYSCTRL_ACTIVE			(0<<0)

 /* GO 0x05 */
#define AW8624_BIT_GO_MASK				(~(1<<0))
#define AW8624_BIT_GO_ENABLE				(1<<0)
#define AW8624_BIT_GO_DISABLE				(0<<0)

 /* WAVSEQ1 0x07 */
#define AW8624_BIT_WAVSEQ1_WAIT				(1<<7)
#define AW8624_BIT_WAVSEQ1_WAV_FRM_SEQ1_MASK		(~(127<<0))

 /* WAVSEQ2 0x08 */
#define AW8624_BIT_WAVSEQ2_WAIT				(1<<7)
#define AW8624_BIT_WAVSEQ2_WAV_FRM_SEQ2_MASK		(~(127<<0))

 /* WAVSEQ3 0x09 */
#define AW8624_BIT_WAVSEQ3_WAIT				(1<<7)
#define AW8624_BIT_WAVSEQ3_WAV_FRM_SEQ3_MASK		(~(127<<0))

 /* WAVSEQ4 0x0A */
#define AW8624_BIT_WAVSEQ4_WAIT				(1<<7)
#define AW8624_BIT_WAVSEQ4_WAV_FRM_SEQ4_MASK		(~(127<<0))

 /* WAVSEQ5 0X0B */
#define AW8624_BIT_WAVSEQ5_WAIT				(1<<7)
#define AW8624_BIT_WAVSEQ5_WAV_FRM_SEQ5_MASK		(~(127<<0))

 /* WAVSEQ6 0X0C */
#define AW8624_BIT_WAVSEQ6_WAIT				(1<<7)
#define AW8624_BIT_WAVSEQ6_WAV_FRM_SEQ6_MASK		(~(127<<0))

 /* WAVSEQ7 */
#define AW8624_BIT_WAVSEQ7_WAIT				(1<<7)
#define AW8624_BIT_WAVSEQ7_WAV_FRM_SEQ7_MASK		(~(127<<0))

 /* WAVSEQ8 */
#define AW8624_BIT_WAVSEQ8_WAIT				(1<<7)
#define AW8624_BIT_WAVSEQ8_WAV_FRM_SEQ8_MASK		(~(127<<0))

 /* WAVLOOP */
#define AW8624_BIT_WAVLOOP_SEQN_MASK			(~(15<<4))
#define AW8624_BIT_WAVLOOP_SEQNP1_MASK			(~(15<<0))
#define AW8624_BIT_WAVLOOP_INIFINITELY			(15<<0)

 /* WAVLOOP1 */
#define AW8624_BIT_WAVLOOP1_SEQ1_MASK			(~(15<<4))
#define AW8624_BIT_WAVLOOP1_SEQ2_MASK			(~(15<<0))

 /* WAVLOOP2 */
#define AW8624_BIT_WAVLOOP2_SEQ3_MASK			(~(15<<4))
#define AW8624_BIT_WAVLOOP2_SEQ4_MASK			(~(15<<0))

 /* WAVLOOP3 */
#define AW8624_BIT_WAVLOOP3_SEQ5_MASK			(~(15<<4))
#define AW8624_BIT_WAVLOOP3_SEQ6_MASK			(~(15<<0))

 /* WAVLOOP4 */
#define AW8624_BIT_WAVLOOP4_SEQ7_MASK			(~(15<<4))
#define AW8624_BIT_WAVLOOP4_SEQ8_MASK			(~(15<<0))

 /* TRGCFG1 */
#define AW8624_BIT_TRGCFG1_TRG1_POLAR_MASK		(~(1<<1))
#define AW8624_BIT_TRGCFG1_TRG1_POLAR_NEG		(1<<1)
#define AW8624_BIT_TRGCFG1_TRG1_POLAR_POS		(0<<1)
#define AW8624_BIT_TRGCFG1_TRG1_EDGE_MASK		(~(1<<0))
#define AW8624_BIT_TRGCFG1_TRG1_EDGE_POS		(1<<0)
#define AW8624_BIT_TRGCFG1_TRG1_EDGE_POS_NEG		(0<<0)

 /* TRGCFG2 */
#define AW8624_BIT_TRGCFG2_TRG1_ENABLE_MASK		(~(1<<0))
#define AW8624_BIT_TRGCFG2_TRG1_ENABLE			(1<<0)
#define AW8624_BIT_TRGCFG2_TRG1_DISABLE			(0<<0)

 /*DBGCTRL 0X20 */
#define AW8624_BIT_DBGCTRL_INTN_TRG_SEL_MASK		(~(1<<5))
#define AW8624_BIT_DBGCTRL_INTN_SEL_ENABLE		(1<<5)
#define AW8624_BIT_DBGCTRL_TRG_SEL_ENABLE		(0<<5)
#define AW8624_BIT_DBGCTRL_INT_MODE_MASK		(~(3<<2))
#define AW8624_BIT_DBGCTRL_INTN_LEVEL_MODE		(0<<2)
#define AW8624_BIT_DBGCTRL_INT_MODE_EDGE		(1<<2)
#define AW8624_BIT_DBGCTRL_INTN_POSEDGE_MODE		(2<<2)
#define AW8624_BIT_DBGCTRL_INTN_BOTH_EDGE_MODE		(3<<2)

 /* DATCTRL */
#define AW8624_BIT_DATCTRL_FC_MASK			(~(1<<6))
#define AW8624_BIT_DATCTRL_FC_1000HZ			(3<<6)
#define AW8624_BIT_DATCTRL_FC_800HZ			(3<<6)
#define AW8624_BIT_DATCTRL_FC_600HZ			(1<<6)
#define AW8624_BIT_DATCTRL_FC_400HZ			(0<<6)
#define AW8624_BIT_DATCTRL_LPF_ENABLE_MASK		(~(1<<5))
#define AW8624_BIT_DATCTRL_LPF_ENABLE			(1<<5)
#define AW8624_BIT_DATCTRL_LPF_DISABLE			(0<<5)

 /*PWMPRC 0X2D */
#define AW8624_BIT_PWMPRC_PRC_EN_MASK			(~(1<<7))
#define AW8624_BIT_PWMPRC_PRC_ENABLE			(1<<7)
#define AW8624_BIT_PWMPRC_PRC_DISABLE			(0<<7)
#define AW8624_BIT_PWMPRC_PRCTIME_MASK			(~(0x7f<<0))

 /* PWMDBG */
#define AW8624_BIT_PWMDBG_PWM_MODE_MASK			(~(3<<5))
#define AW8624_BIT_PWMDBG_PWM_12K			(3<<5)
#define AW8624_BIT_PWMDBG_PWM_24K			(2<<5)
#define AW8624_BIT_PWMDBG_PWM_48K			(0<<5)

/* GLB_STATE 0x47*/
#define AW8624_BIT_GLBRD5_STATE_MASK			(~(15<<0))
#define AW8624_BIT_GLBRD5_STATE_STANDBY			(0<<0)
#define AW8624_BIT_GLBRD5_STATE_WAKEUP			(1<<0)
#define AW8624_BIT_GLBRD5_STATE_STARTUP			(2<<0)
#define AW8624_BIT_GLBRD5_STATE_WAIT			(3<<0)
#define AW8624_BIT_GLBRD5_STATE_CONT_GO			(6<<0)
#define AW8624_BIT_GLBRD5_STATE_RAM_GO			(7<<0)
#define AW8624_BIT_GLBRD5_STATE_RTP_GO			(8<<0)
#define AW8624_BIT_GLBRD5_STATE_TRIG_GO			(9<<0)
#define AW8624_BIT_GLBRD5_STATE_I2S_GO			(10<<0)
#define AW8624_BIT_GLBRD5_STATE_BRAKE			(11<<0)
#define AW8624_BIT_GLBRD5_STATE_END			(12<<0)

 /* WAVECTRL */
#define AW8624_BIT_WAVECTRL_NUM_OV_DRIVER_MASK		(~(0xF<<4))
#define AW8624_BIT_WAVECTRL_NUM_OV_DRIVER		(0<<4)

 /* CONT_CTRL */
#define AW8624_BIT_CONT_CTRL_ZC_DETEC_MASK		(~(1<<7))
#define AW8624_BIT_CONT_CTRL_ZC_DETEC_ENABLE		(1<<7)
#define AW8624_BIT_CONT_CTRL_ZC_DETEC_DISABLE		(0<<7)
#define AW8624_BIT_CONT_CTRL_WAIT_PERIOD_MASK		(~(3<<5))
#define AW8624_BIT_CONT_CTRL_WAIT_8PERIOD		(3<<5)
#define AW8624_BIT_CONT_CTRL_WAIT_4PERIOD		(2<<5)
#define AW8624_BIT_CONT_CTRL_WAIT_2PERIOD		(1<<5)
#define AW8624_BIT_CONT_CTRL_WAIT_1PERIOD		(0<<5)
#define AW8624_BIT_CONT_CTRL_MODE_MASK			(~(1<<4))
#define AW8624_BIT_CONT_CTRL_BY_DRV_TIME		(1<<4)
#define AW8624_BIT_CONT_CTRL_BY_GO_SIGNAL		(0<<4)
#define AW8624_BIT_CONT_CTRL_EN_CLOSE_MASK		(~(1<<3))
#define AW8624_BIT_CONT_CTRL_CLOSE_PLAYBACK		(1<<3)
#define AW8624_BIT_CONT_CTRL_OPEN_PLAYBACK		(0<<3)
#define AW8624_BIT_CONT_CTRL_F0_DETECT_MASK		(~(1<<2))
#define AW8624_BIT_CONT_CTRL_F0_DETECT_ENABLE		(1<<2)
#define AW8624_BIT_CONT_CTRL_F0_DETECT_DISABLE		(0<<2)
#define AW8624_BIT_CONT_CTRL_O2C_MASK			(~(1<<1))
#define AW8624_BIT_CONT_CTRL_O2C_ENABLE			(1<<1)
#define AW8624_BIT_CONT_CTRL_O2C_DISABLE		(0<<1)
#define AW8624_BIT_CONT_CTRL_AUTO_BRK_MASK		(~(1<<0))
#define AW8624_BIT_CONT_CTRL_AUTO_BRK_ENABLE		(1<<0)
#define AW8624_BIT_CONT_CTRL_AUTO_BRK_DISABLE		(0<<0)

#define AW8624_BIT_D2SCFG_CLK_ADC_MASK			(~(7<<5))
#define AW8624_BIT_D2SCFG_CLK_ASC_1P5MHZ		(3<<5)

#define AW8624_BIT_D2SCFG_GAIN_MASK			(~(7<<0))
#define AW8624_BIT_D2SCFG_GAIN_40			(7<<0)
 /* DETCTRL */
#define AW8624_BIT_DETCTRL_RL_OS_MASK			(~(1<<6))
#define AW8624_BIT_DETCTRL_RL_DETECT			(1<<6)
#define AW8624_BIT_DETCTRL_OS_DETECT			(0<<6)
#define AW8624_BIT_DETCTRL_PROTECT_MASK			(~(1<<5))
#define AW8624_BIT_DETCTRL_PROTECT_NO_ACTION		(1<<5)
#define AW8624_BIT_DETCTRL_PROTECT_SHUTDOWN		(0<<5)
#define AW8624_BIT_DETCTRL_VBAT_GO_MASK			(~(1<<1))
#define AW8624_BIT_DETCTRL_VABT_GO_ENABLE		(1<<1)
#define AW8624_BIT_DETCTRL_VBAT_GO_DISBALE		(0<<1)
#define AW8624_BIT_DETCTRL_DIAG_GO_MASK			(~(1<<0))
#define AW8624_BIT_DETCTRL_DIAG_GO_ENABLE		(1<<0)
#define AW8624_BIT_DETCTRL_DIAG_GO_DISABLE		(0<<0)


#define AW8624_BIT_RAMADDRH_MASK			(~(63<<0))

 /* VBAT MODE */
#define AW8624_BIT_DETCTRL_VBAT_MODE_MASK		(~(1<<6))
#define AW8624_BIT_DETCTRL_VBAT_HW_COMP			(1<<6)
#define AW8624_BIT_DETCTRL_VBAT_SW_COMP			(0<<6)


 /* ANACTRL */
#define AW8624_BIT_ANACTRL_LRA_SRC_MASK			(~(1<<5))
#define AW8624_BIT_ANACTRL_LRA_SRC_REG			(1<<5)
#define AW8624_BIT_ANACTRL_LRA_SRC_EFUSE		(0<<5)
#define AW8624_BIT_ANACTRL_EN_IO_PD1_MASK		(~(1<<0))
#define AW8624_BIT_ANACTRL_EN_IO_PD1_HIGH		(1<<0)
#define AW8624_BIT_ANACTRL_EN_IO_PD1_LOW		(0<<0)

/* PRLVL */
#define AW8624_BIT_PRLVL_PR_EN_MASK			(~(1<<7))
#define AW8624_BIT_PRLVL_PR_ENABLE			(1<<7)
#define AW8624_BIT_PRLVL_PR_DISABLE			(0<<7)
#define AW8624_BIT_PRLVL_PRLVL_MASK			(~(0x7f<<0))

/* PRTIME */
#define AW8624_BIT_PRTIME_PRTIME_MASK			(~(0xff<<0))

#define AW8624_BIT_BEMF_NUM_BRK_MASK			(~(0xf<<0))

/* TD_H 0x4b TD_brake */
#define AW8624_BIT_R_SPARE_MASK				(~(1<<7))
#define AW8624_BIT_R_SPARE_ENABLE			(1<<7)

/* TIME_NZC */
#define AW8624_BIT_TIME_NZC_DEF_VAL			(0x1F)

#endif
