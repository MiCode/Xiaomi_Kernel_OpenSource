#ifndef _AW869X_H_
#define _AW869X_H_

/*********************************************************
 *
 * aw869x.h
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
#define AW869X_SEQUENCER_SIZE		(8)
#define AW869X_SEQUENCER_LOOP_SIZE	(4)
#define AW869X_TRIG_NUM			(3)
#define AW869X_MAX_BST_VO		(0x1f)
#define AW869X_REG_MAX			(0xff)

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

/*********************************************************
 *
 * enum
 *
 ********************************************************/
enum aw869x_flags {
	AW869X_FLAG_NONR = 0,
	AW869X_FLAG_SKIP_INTERRUPTS = 1,
};

enum aw869x_haptic_read_write {
	AW869X_HAPTIC_CMD_READ_REG = 0,
	AW869X_HAPTIC_CMD_WRITE_REG = 1,
};

enum aw869x_haptic_work_mode {
	AW869X_HAPTIC_STANDBY_MODE = 0,
	AW869X_HAPTIC_RAM_MODE = 1,
	AW869X_HAPTIC_RTP_MODE = 2,
	AW869X_HAPTIC_TRIG_MODE = 3,
	AW869X_HAPTIC_CONT_MODE = 4,
	AW869X_HAPTIC_RAM_LOOP_MODE = 5,
};

enum aw869x_haptic_bst_mode {
	AW869X_HAPTIC_BYPASS_MODE = 0,
	AW869X_HAPTIC_BOOST_MODE = 1,
};

enum aw869x_haptic_activate_mode {
	AW869X_HAPTIC_ACTIVATE_RAM_MODE = 0,
	AW869X_HAPTIC_ACTIVATE_CONT_MODE = 1,
	AW869X_HAPTIC_ACTIVATE_RTP_MODE = 2,
	AW869X_HAPTIC_ACTIVATE_RAM_LOOP_MODE = 3,
};

enum aw869x_haptic_cont_vbat_comp_mode {
	AW869X_HAPTIC_CONT_VBAT_SW_COMP_MODE = 0,
	AW869X_HAPTIC_CONT_VBAT_HW_COMP_MODE = 1,
};

enum aw869x_haptic_ram_vbat_comp_mode {
	AW869X_HAPTIC_RAM_VBAT_COMP_DISABLE = 0,
	AW869X_HAPTIC_RAM_VBAT_COMP_ENABLE = 1,
};

enum aw869x_haptic_f0_flag {
	AW869X_HAPTIC_LRA_F0 = 0,
	AW869X_HAPTIC_CALI_F0 = 1,
};

enum aw869x_haptic_pwm_mode {
	AW869X_PWM_48K = 0,
	AW869X_PWM_24K = 1,
	AW869X_PWM_12K = 2,
};

enum aw869x_haptic_play {
	AW869X_HAPTIC_PLAY_NULL = 0,
	AW869X_HAPTIC_PLAY_ENABLE = 1,
	AW869X_HAPTIC_PLAY_STOP = 2,
	AW869X_HAPTIC_PLAY_GAIN = 8,
};

enum aw869x_haptic_cmd {
	AW869X_HAPTIC_CMD_NULL = 0,
	AW869X_HAPTIC_CMD_ENABLE = 1,
	AW869X_HAPTIC_CMD_STOP = 255,
};

enum aw869x_haptic_strength {
	AW869X_LIGHT_MAGNITUDE = 0x3fff,
	AW869X_MEDIUM_MAGNITUDE = 0x5fff,
	AW869X_STRONG_MAGNITUDE = 0x7fff,
};

enum aw869x_haptic_cali_lra {
	AW869X_WRITE_ZERO = 0,
	AW869X_F0_CALI_LRA = 1,
	AW869X_OSC_CALI_LRA = 2,
};
/*********************************************************
 *
 * struct
 *
 ********************************************************/
struct aw869x_trig {
	unsigned char enable;
	unsigned char default_level;
	unsigned char dual_edge;
	unsigned char frist_seq;
	unsigned char second_seq;
};

struct aw869x_dts_info {
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

struct aw869x {
	struct i2c_client *i2c;
	struct mutex lock;
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

	int state;
	int duration;
	int amplitude;
	int index;
	int vmax;
	int gain;
	u16 new_gain;
	unsigned char level;

	unsigned char seq[AW869X_SEQUENCER_SIZE];
	unsigned char loop[AW869X_SEQUENCER_SIZE];

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

	struct aw869x_trig trig[AW869X_TRIG_NUM];

	struct haptic_audio haptic_audio;
	struct aw869x_dts_info info;
	atomic_t is_in_rtp_loop;
	atomic_t exit_in_rtp_loop;
	atomic_t is_in_write_loop;
	wait_queue_head_t wait_q;	/* wait queue for exit irq mode */
	wait_queue_head_t stop_wait_q;	/* wait queue for stop rtp mode */
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
	struct hrtimer timer;	/*test used,del */
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

struct aw869x_container {
	int len;
	unsigned char data[];
};

/********************************************
 * Register List
 *******************************************/
#define AW869X_REG_ID            0x00
#define AW869X_REG_SYSST         0x01
#define AW869X_REG_SYSINT        0x02
#define AW869X_REG_SYSINTM       0x03
#define AW869X_REG_SYSCTRL       0x04
#define AW869X_REG_GO            0x05
#define AW869X_REG_RTP_DATA      0x06
#define AW869X_REG_WAVSEQ1       0x07
#define AW869X_REG_WAVSEQ2       0x08
#define AW869X_REG_WAVSEQ3       0x09
#define AW869X_REG_WAVSEQ4       0x0a
#define AW869X_REG_WAVSEQ5       0x0b
#define AW869X_REG_WAVSEQ6       0x0c
#define AW869X_REG_WAVSEQ7       0x0d
#define AW869X_REG_WAVSEQ8       0x0e
#define AW869X_REG_WAVLOOP1      0x0f
#define AW869X_REG_WAVLOOP2      0x10
#define AW869X_REG_WAVLOOP3      0x11
#define AW869X_REG_WAVLOOP4      0x12
#define AW869X_REG_MAIN_LOOP     0x13
#define AW869X_REG_TRG1_WAV_P    0x14
#define AW869X_REG_TRG2_WAV_P    0x15
#define AW869X_REG_TRG3_WAV_P    0x16
#define AW869X_REG_TRG1_WAV_N    0x17
#define AW869X_REG_TRG2_WAV_N    0x18
#define AW869X_REG_TRG3_WAV_N    0x19
#define AW869X_REG_TRG_PRIO      0x1a
#define AW869X_REG_TRG_CFG1      0x1b
#define AW869X_REG_TRG_CFG2      0x1c
#define AW869X_REG_DBGCTRL       0x20
#define AW869X_REG_BASE_ADDRH    0x21
#define AW869X_REG_BASE_ADDRL    0x22
#define AW869X_REG_FIFO_AEH      0x23
#define AW869X_REG_FIFO_AEL      0x24
#define AW869X_REG_FIFO_AFH      0x25
#define AW869X_REG_FIFO_AFL      0x26
#define AW869X_REG_WAKE_DLY      0x27
#define AW869X_REG_START_DLY     0x28
#define AW869X_REG_END_DLY_H     0x29
#define AW869X_REG_END_DLY_L     0x2a
#define AW869X_REG_DATCTRL       0x2b
#define AW869X_REG_PWMDEL        0x2c
#define AW869X_REG_PWMPRC        0x2d
#define AW869X_REG_PWMDBG        0x2e
#define AW869X_REG_LDOCTRL       0x2f
#define AW869X_REG_DBGSTAT       0x30
#define AW869X_REG_BSTDBG1       0x31
#define AW869X_REG_BSTDBG2       0x32
#define AW869X_REG_BSTDBG3       0x33
#define AW869X_REG_BSTCFG        0x34
#define AW869X_REG_ANADBG        0x35
#define AW869X_REG_ANACTRL       0x36
#define AW869X_REG_CPDBG         0x37
#define AW869X_REG_GLBDBG        0x38
#define AW869X_REG_DATDBG        0x39
#define AW869X_REG_BSTDBG4       0x3a
#define AW869X_REG_BSTDBG5       0x3b
#define AW869X_REG_BSTDBG6       0x3c
#define AW869X_REG_HDRVDBG       0x3d
#define AW869X_REG_PRLVL         0x3e
#define AW869X_REG_PRTIME        0x3f
#define AW869X_REG_RAMADDRH      0x40
#define AW869X_REG_RAMADDRL      0x41
#define AW869X_REG_RAMDATA       0x42
#define AW869X_REG_GLB_STATE     0x46
#define AW869X_REG_BST_AUTO      0x47
#define AW869X_REG_CONT_CTRL     0x48
#define AW869X_REG_F_PRE_H       0x49
#define AW869X_REG_F_PRE_L       0x4a
#define AW869X_REG_TD_H          0x4b
#define AW869X_REG_TD_L          0x4c
#define AW869X_REG_TSET          0x4d
#define AW869X_REG_TRIM_LRA      0x5b
#define AW869X_REG_R_SPARE       0x5d
#define AW869X_REG_D2SCFG        0x5e
#define AW869X_REG_DETCTRL       0x5f
#define AW869X_REG_RLDET         0x60
#define AW869X_REG_OSDET         0x61
#define AW869X_REG_VBATDET       0x62
#define AW869X_REG_TESTDET       0x63
#define AW869X_REG_DETLO         0x64
#define AW869X_REG_BEMFDBG       0x65
#define AW869X_REG_ADCTEST       0x66
#define AW869X_REG_BEMFTEST      0x67
#define AW869X_REG_F_LRA_F0_H    0x68
#define AW869X_REG_F_LRA_F0_L    0x69
#define AW869X_REG_F_LRA_CONT_H  0x6a
#define AW869X_REG_F_LRA_CONT_L  0x6b
#define AW869X_REG_WAIT_VOL_MP   0x6d
#define AW869X_REG_WAIT_VOL_MN   0x6f
#define AW869X_REG_BEMF_VOL_H    0x70
#define AW869X_REG_BEMF_VOL_L    0x71
#define AW869X_REG_ZC_THRSH_H    0x72
#define AW869X_REG_ZC_THRSH_L    0x73
#define AW869X_REG_BEMF_VTHH_H   0x74
#define AW869X_REG_BEMF_VTHH_L   0x75
#define AW869X_REG_BEMF_VTHL_H   0x76
#define AW869X_REG_BEMF_VTHL_L   0x77
#define AW869X_REG_BEMF_NUM      0x78
#define AW869X_REG_DRV_TIME      0x79
#define AW869X_REG_TIME_NZC      0x7a
#define AW869X_REG_DRV_LVL       0x7b
#define AW869X_REG_DRV_LVL_OV    0x7c
#define AW869X_REG_NUM_F0_1      0x7d
#define AW869X_REG_NUM_F0_2      0x7e
#define AW869X_REG_NUM_F0_3      0x7f
/******************************************************
 * Register Detail
 *****************************************************/
/* SYSST: reg0x01 */
#define AW869X_BIT_SYSST_BSTERRS                    (1<<7)
#define AW869X_BIT_SYSST_OVS                        (1<<6)
#define AW869X_BIT_SYSST_UVLS                       (1<<5)
#define AW869X_BIT_SYSST_FF_AES                     (1<<4)
#define AW869X_BIT_SYSST_FF_AFS                     (1<<3)
#define AW869X_BIT_SYSST_OCDS                       (1<<2)
#define AW869X_BIT_SYSST_OTS                        (1<<1)
#define AW869X_BIT_SYSST_DONES                      (1<<0)

/* SYSINT: reg0x02 */
#define AW869X_BIT_SYSINT_BSTERRI                   (1<<7)
#define AW869X_BIT_SYSINT_OVI                       (1<<6)
#define AW869X_BIT_SYSINT_UVLI                      (1<<5)
#define AW869X_BIT_SYSINT_FF_AEI                    (1<<4)
#define AW869X_BIT_SYSINT_FF_AFI                    (1<<3)
#define AW869X_BIT_SYSINT_OCDI                      (1<<2)
#define AW869X_BIT_SYSINT_OTI                       (1<<1)
#define AW869X_BIT_SYSINT_DONEI                     (1<<0)

/* SYSINTM: reg0x03 */
#define AW869X_BIT_SYSINTM_BSTERR_MASK              (~(1<<7))
#define AW869X_BIT_SYSINTM_BSTERR_OFF               (1<<7)
#define AW869X_BIT_SYSINTM_BSTERR_EN                (0<<7)
#define AW869X_BIT_SYSINTM_OV_MASK                  (~(1<<6))
#define AW869X_BIT_SYSINTM_OV_OFF                   (1<<6)
#define AW869X_BIT_SYSINTM_OV_EN                    (0<<6)
#define AW869X_BIT_SYSINTM_UVLO_MASK                (~(1<<5))
#define AW869X_BIT_SYSINTM_UVLO_OFF                 (1<<5)
#define AW869X_BIT_SYSINTM_UVLO_EN                  (0<<5)
#define AW869X_BIT_SYSINTM_FF_AE_MASK               (~(1<<4))
#define AW869X_BIT_SYSINTM_FF_AE_OFF                (1<<4)
#define AW869X_BIT_SYSINTM_FF_AE_EN                 (0<<4)
#define AW869X_BIT_SYSINTM_FF_AF_MASK               (~(1<<3))
#define AW869X_BIT_SYSINTM_FF_AF_OFF                (1<<3)
#define AW869X_BIT_SYSINTM_FF_AF_EN                 (0<<3)
#define AW869X_BIT_SYSINTM_OCD_MASK                 (~(1<<2))
#define AW869X_BIT_SYSINTM_OCD_OFF                  (1<<2)
#define AW869X_BIT_SYSINTM_OCD_EN                   (0<<2)
#define AW869X_BIT_SYSINTM_OT_MASK                  (~(1<<1))
#define AW869X_BIT_SYSINTM_OT_OFF                   (1<<1)
#define AW869X_BIT_SYSINTM_OT_EN                    (0<<1)
#define AW869X_BIT_SYSINTM_DONE_MASK                (~(1<<0))
#define AW869X_BIT_SYSINTM_DONE_OFF                 (1<<0)
#define AW869X_BIT_SYSINTM_DONE_EN                  (0<<0)

/* SYSCTRL: reg0x04 */
#define AW869X_BIT_SYSCTRL_WAVDAT_MODE_MASK         (~(3<<6))
#define AW869X_BIT_SYSCTRL_WAVDAT_MODE_4X           (3<<6)
#define AW869X_BIT_SYSCTRL_WAVDAT_MODE_2X           (0<<6)
#define AW869X_BIT_SYSCTRL_WAVDAT_MODE_1X           (1<<6)
#define AW869X_BIT_SYSCTRL_RAMINIT_MASK             (~(1<<5))
#define AW869X_BIT_SYSCTRL_RAMINIT_EN               (1<<5)
#define AW869X_BIT_SYSCTRL_RAMINIT_OFF              (0<<5)
#define AW869X_BIT_SYSCTRL_PLAY_MODE_MASK           (~(3<<2))
#define AW869X_BIT_SYSCTRL_PLAY_MODE_CONT           (2<<2)
#define AW869X_BIT_SYSCTRL_PLAY_MODE_RTP            (1<<2)
#define AW869X_BIT_SYSCTRL_PLAY_MODE_RAM            (0<<2)
#define AW869X_BIT_SYSCTRL_BST_MODE_MASK            (~(1<<1))
#define AW869X_BIT_SYSCTRL_BST_MODE_BOOST           (1<<1)
#define AW869X_BIT_SYSCTRL_BST_MODE_BYPASS          (0<<1)
#define AW869X_BIT_SYSCTRL_WORK_MODE_MASK           (~(1<<0))
#define AW869X_BIT_SYSCTRL_STANDBY                  (1<<0)
#define AW869X_BIT_SYSCTRL_ACTIVE                   (0<<0)

/* GO: reg0x05 */
#define AW869X_BIT_GO_MASK                          (~(1<<0))
#define AW869X_BIT_GO_ENABLE                        (1<<0)
#define AW869X_BIT_GO_DISABLE                       (0<<0)

/* WAVSEQ1: reg0x07 */
#define AW869X_BIT_WAVSEQ1_WAIT                     (1<<7)
#define AW869X_BIT_WAVSEQ1_WAV_FRM_SEQ1_MASK        (~(127<<0))

/* WAVSEQ2: reg0x08 */
#define AW869X_BIT_WAVSEQ2_WAIT                     (1<<7)
#define AW869X_BIT_WAVSEQ2_WAV_FRM_SEQ2_MASK        (~(127<<0))

/* WAVSEQ3: reg0x09 */
#define AW869X_BIT_WAVSEQ3_WAIT                     (1<<7)
#define AW869X_BIT_WAVSEQ3_WAV_FRM_SEQ3_MASK        (~(127<<0))

/* WAVSEQ4: reg0x0a */
#define AW869X_BIT_WAVSEQ4_WAIT                     (1<<7)
#define AW869X_BIT_WAVSEQ4_WAV_FRM_SEQ4_MASK        (~(127<<0))

/* WAVSEQ5: reg0x0b */
#define AW869X_BIT_WAVSEQ5_WAIT                     (1<<7)
#define AW869X_BIT_WAVSEQ5_WAV_FRM_SEQ5_MASK        (~(127<<0))

/* WAVSEQ6: reg0x0c */
#define AW869X_BIT_WAVSEQ6_WAIT                     (1<<7)
#define AW869X_BIT_WAVSEQ6_WAV_FRM_SEQ6_MASK        (~(127<<0))

/* WAVSEQ7: reg0x0d */
#define AW869X_BIT_WAVSEQ7_WAIT                     (1<<7)
#define AW869X_BIT_WAVSEQ7_WAV_FRM_SEQ7_MASK        (~(127<<0))

/* WAVSEQ8: reg0x0e */
#define AW869X_BIT_WAVSEQ8_WAIT                     (1<<7)
#define AW869X_BIT_WAVSEQ8_WAV_FRM_SEQ8_MASK        (~(127<<0))

/* WAVLOOP: */
#define AW869X_BIT_WAVLOOP_SEQN_MASK                (~(15<<4))
#define AW869X_BIT_WAVLOOP_SEQNP1_MASK              (~(15<<0))
#define AW869X_BIT_WAVLOOP_INIFINITELY              (15<<0)

/* WAVLOOP1: reg0x0f */
#define AW869X_BIT_WAVLOOP1_SEQ1_MASK               (~(15<<4))
#define AW869X_BIT_WAVLOOP1_SEQ2_MASK               (~(15<<0))

/* WAVLOOP2: reg0x10 */
#define AW869X_BIT_WAVLOOP2_SEQ3_MASK               (~(15<<4))
#define AW869X_BIT_WAVLOOP2_SEQ4_MASK               (~(15<<0))

/* WAVLOOP3: reg0x11 */
#define AW869X_BIT_WAVLOOP3_SEQ5_MASK               (~(15<<4))
#define AW869X_BIT_WAVLOOP3_SEQ6_MASK               (~(15<<0))

/* WAVLOOP4: reg0x12 */
#define AW869X_BIT_WAVLOOP4_SEQ7_MASK               (~(15<<4))
#define AW869X_BIT_WAVLOOP4_SEQ8_MASK               (~(15<<0))

/* PLAYPRIO: reg0x1a */
#define AW869X_BIT_PLAYPRIO_GO_MASK                 (~(3<<6))
#define AW869X_BIT_PLAYPRIO_TRIG3_MASK              (~(3<<4))
#define AW869X_BIT_PLAYPRIO_TRIG2_MASK              (~(3<<2))
#define AW869X_BIT_PLAYPRIO_TRIG1_MASK              (~(3<<0))

/* TRGCFG1: reg0x1b */
#define AW869X_BIT_TRGCFG1_TRG3_POLAR_MASK          (~(1<<5))
#define AW869X_BIT_TRGCFG1_TRG3_POLAR_NEG           (1<<5)
#define AW869X_BIT_TRGCFG1_TRG3_POLAR_POS           (0<<5)
#define AW869X_BIT_TRGCFG1_TRG3_EDGE_MASK           (~(1<<4))
#define AW869X_BIT_TRGCFG1_TRG3_EDGE_POS            (1<<4)
#define AW869X_BIT_TRGCFG1_TRG3_EDGE_POS_NEG        (0<<4)
#define AW869X_BIT_TRGCFG1_TRG2_POLAR_MASK          (~(1<<3))
#define AW869X_BIT_TRGCFG1_TRG2_POLAR_NEG           (1<<3)
#define AW869X_BIT_TRGCFG1_TRG2_POLAR_POS           (0<<3)
#define AW869X_BIT_TRGCFG1_TRG2_EDGE_MASK           (~(1<<2))
#define AW869X_BIT_TRGCFG1_TRG2_EDGE_POS            (1<<2)
#define AW869X_BIT_TRGCFG1_TRG2_EDGE_POS_NEG        (0<<2)
#define AW869X_BIT_TRGCFG1_TRG1_POLAR_MASK          (~(1<<1))
#define AW869X_BIT_TRGCFG1_TRG1_POLAR_NEG           (1<<1)
#define AW869X_BIT_TRGCFG1_TRG1_POLAR_POS           (0<<1)
#define AW869X_BIT_TRGCFG1_TRG1_EDGE_MASK           (~(1<<0))
#define AW869X_BIT_TRGCFG1_TRG1_EDGE_POS            (1<<0)
#define AW869X_BIT_TRGCFG1_TRG1_EDGE_POS_NEG        (0<<0)

/* TRGCFG2: reg0x1c */
#define AW869X_BIT_TRGCFG2_TRG3_ENABLE_MASK         (~(1<<2))
#define AW869X_BIT_TRGCFG2_TRG3_ENABLE              (1<<2)
#define AW869X_BIT_TRGCFG2_TRG3_DISABLE             (0<<2)
#define AW869X_BIT_TRGCFG2_TRG2_ENABLE_MASK         (~(1<<1))
#define AW869X_BIT_TRGCFG2_TRG2_ENABLE              (1<<1)
#define AW869X_BIT_TRGCFG2_TRG2_DISABLE             (0<<1)
#define AW869X_BIT_TRGCFG2_TRG1_ENABLE_MASK         (~(1<<0))
#define AW869X_BIT_TRGCFG2_TRG1_ENABLE              (1<<0)
#define AW869X_BIT_TRGCFG2_TRG1_DISABLE             (0<<0)

/* DBGCTRL: reg0x20 */
#define AW869X_BIT_DBGCTRL_INT_EDGE_MODE_MASK       (~(1<<3))
#define AW869X_BIT_DBGCTRL_INT_EDGE_MODE_POS        (1<<3)
#define AW869X_BIT_DBGCTRL_INT_EDGE_MODE_BOTH       (0<<3)
#define AW869X_BIT_DBGCTRL_INT_MODE_MASK            (~(1<<2))
#define AW869X_BIT_DBGCTRL_INT_MODE_EDGE            (1<<2)
#define AW869X_BIT_DBGCTRL_INT_MODE_LEVEL           (0<<2)

/* DATCTRL: reg0x2b */
#define AW869X_BIT_DATCTRL_FC_MASK                  (~(1<<6))
#define AW869X_BIT_DATCTRL_FC_1000HZ                (3<<6)
#define AW869X_BIT_DATCTRL_FC_800HZ                 (3<<6)
#define AW869X_BIT_DATCTRL_FC_600HZ                 (1<<6)
#define AW869X_BIT_DATCTRL_FC_400HZ                 (0<<6)
#define AW869X_BIT_DATCTRL_LPF_ENABLE_MASK          (~(1<<5))
#define AW869X_BIT_DATCTRL_LPF_ENABLE               (1<<5)
#define AW869X_BIT_DATCTRL_LPF_DISABLE              (0<<5)
#define AW869X_BIT_DATCTRL_WAKEMODE_ENABLE_MASK     (~(1<<0))
#define AW869X_BIT_DATCTRL_WAKEMODE_ENABLE          (1<<0)
#define AW869X_BIT_DATCTRL_WAKEMODE_DISABLE         (0<<0)

/* PWMPRC: reg0x2d */
#define AW869X_BIT_PWMPRC_PRC_MASK                  (~(1<<7))
#define AW869X_BIT_PWMPRC_PRC_ENABLE                (1<<7)
#define AW869X_BIT_PWMPRC_PRC_DISABLE               (0<<7)
#define AW869X_BIT_PWMPRC_PRCTIME_MASK              (~(0x7f<<0))

/* PWMDBG: reg0x2e */
#define AW869X_BIT_PWMDBG_PWM_MODE_MASK             (~(3<<5))
#define AW869X_BIT_PWMDBG_PWM_12K                   (3<<5)
#define AW869X_BIT_PWMDBG_PWM_24K                   (2<<5)
#define AW869X_BIT_PWMDBG_PWM_48K                   (0<<5)

/* DBGST: reg0x30 */
#define AW869X_BIT_DBGSTAT_FF_EMPTY                 (1<<0)
/* BSTCFG: reg0x34 */
#define AW869X_BIT_BSTCFG_PEAKCUR_MASK              (~(7<<0))
#define AW869X_BIT_BSTCFG_PEAKCUR_4A                (7<<0)
#define AW869X_BIT_BSTCFG_PEAKCUR_3P75A             (6<<0)
#define AW869X_BIT_BSTCFG_PEAKCUR_3P5A              (5<<0)
#define AW869X_BIT_BSTCFG_PEAKCUR_3P25A             (4<<0)
#define AW869X_BIT_BSTCFG_PEAKCUR_3A                (3<<0)
#define AW869X_BIT_BSTCFG_PEAKCUR_2P5A              (2<<0)
#define AW869X_BIT_BSTCFG_PEAKCUR_2A                (1<<0)
#define AW869X_BIT_BSTCFG_PEAKCUR_1P5A              (0<<0)

/* ANADBG: reg0x35 */
#define AW869X_BIT_ANADBG_IOC_MASK                  (~(3<<2))
#define AW869X_BIT_ANADBG_IOC_4P65A                 (3<<2)
#define AW869X_BIT_ANADBG_IOC_4P15A                 (2<<2)
#define AW869X_BIT_ANADBG_IOC_3P65A                 (1<<2)
#define AW869X_BIT_ANADBG_IOC_3P15A                 (0<<2)

/* ANACTRL: reg0x36 */
#define AW869X_BIT_ANACTRL_LRA_SRC_MASK             (~(1<<5))
#define AW869X_BIT_ANACTRL_LRA_SRC_REG              (1<<5)
#define AW869X_BIT_ANACTRL_LRA_SRC_EFUSE            (0<<5)
#define AW869X_BIT_ANACTRL_HD_PD_MASK               (~(1<<3))
#define AW869X_BIT_ANACTRL_HD_PD_EN                 (1<<3)
#define AW869X_BIT_ANACTRL_HD_HZ_EN                 (0<<3)

/* BSTDBG4: reg0x3a */
#define AW869X_BIT_BSTDBG4_BSTVOL_MASK              (~(31<<1))

/* PRLVL: reg0x3e */
#define AW869X_BIT_PRLVL_PR_MASK                    (~(1<<7))
#define AW869X_BIT_PRLVL_PR_ENABLE                  (1<<7)
#define AW869X_BIT_PRLVL_PR_DISABLE                 (0<<7)
#define AW869X_BIT_PRLVL_PRLVL_MASK                 (~(0x7f<<0))

/*PRTIME: reg0x3f */
#define AW869X_BIT_PRTIME_PRTIME_MASK               (~(0xff<<0))

/* BST_AUTO: reg0x47 */
#define AW869X_BIT_BST_AUTO_BST_AUTOSW_MASK         (~(1<<2))
#define AW869X_BIT_BST_AUTO_BST_AUTOMATIC_BOOST     (1<<2)
#define AW869X_BIT_BST_AUTO_BST_MANUAL_BOOST        (0<<2)
#define AW869X_BIT_BST_AUTO_BST_RTP_MASK            (~(1<<1))
#define AW869X_BIT_BST_AUTO_BST_RTP_ENABLE          (1<<1)
#define AW869X_BIT_BST_AUTO_BST_RTP_DISABLE         (0<<1)
#define AW869X_BIT_BST_AUTO_BST_RAM_MASK            (~(1<<0))
#define AW869X_BIT_BST_AUTO_BST_RAM_ENABLE          (1<<0)
#define AW869X_BIT_BST_AUTO_BST_RAM_DISABLE         (0<<0)

/* CONT_CTRL: reg0x48 */
#define AW869X_BIT_CONT_CTRL_ZC_DETEC_MASK          (~(1<<7))
#define AW869X_BIT_CONT_CTRL_ZC_DETEC_ENABLE        (1<<7)
#define AW869X_BIT_CONT_CTRL_ZC_DETEC_DISABLE       (0<<7)
#define AW869X_BIT_CONT_CTRL_WAIT_PERIOD_MASK       (~(3<<5))
#define AW869X_BIT_CONT_CTRL_WAIT_8PERIOD           (3<<5)
#define AW869X_BIT_CONT_CTRL_WAIT_4PERIOD           (2<<5)
#define AW869X_BIT_CONT_CTRL_WAIT_2PERIOD           (1<<5)
#define AW869X_BIT_CONT_CTRL_WAIT_1PERIOD           (0<<5)
#define AW869X_BIT_CONT_CTRL_MODE_MASK              (~(1<<4))
#define AW869X_BIT_CONT_CTRL_BY_DRV_TIME            (1<<4)
#define AW869X_BIT_CONT_CTRL_BY_GO_SIGNAL           (0<<4)
#define AW869X_BIT_CONT_CTRL_EN_CLOSE_MASK          (~(1<<3))
#define AW869X_BIT_CONT_CTRL_CLOSE_PLAYBACK         (1<<3)
#define AW869X_BIT_CONT_CTRL_OPEN_PLAYBACK          (0<<3)
#define AW869X_BIT_CONT_CTRL_F0_DETECT_MASK         (~(1<<2))
#define AW869X_BIT_CONT_CTRL_F0_DETECT_ENABLE       (1<<2)
#define AW869X_BIT_CONT_CTRL_F0_DETECT_DISABLE      (0<<2)
#define AW869X_BIT_CONT_CTRL_O2C_MASK               (~(1<<1))
#define AW869X_BIT_CONT_CTRL_O2C_ENABLE             (1<<1)
#define AW869X_BIT_CONT_CTRL_O2C_DISABLE            (0<<1)
#define AW869X_BIT_CONT_CTRL_AUTO_BRK_MASK          (~(1<<0))
#define AW869X_BIT_CONT_CTRL_AUTO_BRK_ENABLE        (1<<0)
#define AW869X_BIT_CONT_CTRL_AUTO_BRK_DISABLE       (0<<0)

/* D2SCFG: reg0x5e */
#define AW869X_BIT_D2SCFG_CLK_ADC_MASK              (~(7<<5))
#define AW869X_BIT_D2SCFG_CLK_ASC_0P09375MHZ        (7<<5)
#define AW869X_BIT_D2SCFG_CLK_ASC_0P1875MHZ         (6<<5)
#define AW869X_BIT_D2SCFG_CLK_ASC_0P375MHZ          (5<<5)
#define AW869X_BIT_D2SCFG_CLK_ASC_0P75MHZ           (4<<5)
#define AW869X_BIT_D2SCFG_CLK_ASC_1P5MHZ            (3<<5)
#define AW869X_BIT_D2SCFG_CLK_ASC_3MHZ              (2<<5)
#define AW869X_BIT_D2SCFG_CLK_ASC_6MHZ              (1<<5)
#define AW869X_BIT_D2SCFG_CLK_ASC_12MHZ             (0<<5)

/* DETCTRL: reg0x5f */
#define AW869X_BIT_DETCTRL_RL_OS_MASK               (~(1<<6))
#define AW869X_BIT_DETCTRL_RL_DETECT                (1<<6)
#define AW869X_BIT_DETCTRL_OS_DETECT                (0<<6)
#define AW869X_BIT_DETCTRL_PROTECT_MASK             (~(1<<5))
#define AW869X_BIT_DETCTRL_PROTECT_NO_ACTION        (1<<5)
#define AW869X_BIT_DETCTRL_PROTECT_SHUTDOWN         (0<<5)
#define AW869X_BIT_DETCTRL_ADO_SLOT_MODE_MASK       (~(1<<4))
#define AW869X_BIT_DETCTRL_ADO_SLOT_MODE_ENABLE     (1<<4)
#define AW869X_BIT_DETCTRL_ADO_SLOT_MODE_DISABLE    (0<<4)
#define AW869X_BIT_DETCTRL_VBAT_GO_MASK             (~(1<<1))
#define AW869X_BIT_DETCTRL_VABT_GO_ENABLE           (1<<1)
#define AW869X_BIT_DETCTRL_VBAT_GO_DISBALE          (0<<1)
#define AW869X_BIT_DETCTRL_DIAG_GO_MASK             (~(1<<0))
#define AW869X_BIT_DETCTRL_DIAG_GO_ENABLE           (1<<0)
#define AW869X_BIT_DETCTRL_DIAG_GO_DISABLE          (0<<0)

/* ADCTEST: reg0x66 */
#define AW869X_BIT_ADCTEST_VBAT_MODE_MASK           (~(1<<6))
#define AW869X_BIT_ADCTEST_VBAT_HW_COMP             (1<<6)
#define AW869X_BIT_ADCTEST_VBAT_SW_COMP             (0<<6)

/* BEMF_NUM: reg0x78 */
#define AW869X_BIT_BEMF_NUM_BRK_MASK                (~(15<<0))

/*********************************************************
 *
 * extern
 *
 ********************************************************/
extern int aw869x_parse_dt(struct device *dev, struct aw869x *aw869x,
			   struct device_node *np);
extern int aw869x_haptics_upload_effect(struct input_dev *dev,
					struct ff_effect *effect,
					struct ff_effect *old);
extern int aw869x_haptics_playback(struct input_dev *dev, int effect_id,
				   int val);
extern int aw869x_haptics_erase(struct input_dev *dev, int effect_id);
extern void aw869x_haptics_set_gain(struct input_dev *dev, u16 gain);
extern void aw869x_haptics_set_gain_work_routine(struct work_struct *work);
extern void aw869x_interrupt_setup(struct aw869x *aw869x);
extern int aw869x_vibrator_init(struct aw869x *aw869x);
extern int aw869x_haptic_init(struct aw869x *aw869x, unsigned char chip_name);
extern int aw869x_ram_init(struct aw869x *aw869x);
extern irqreturn_t aw869x_irq(int irq, void *data);
extern struct attribute_group aw869x_vibrator_attribute_group;
extern struct miscdevice aw869x_haptic_misc;
/*********************************************************
 *
 * ioctl
 *
 ********************************************************/
struct aw869x_seq_loop {
	unsigned char loop[AW869X_SEQUENCER_SIZE];
};

struct aw869x_que_seq {
	unsigned char index[AW869X_SEQUENCER_SIZE];
};

#define AW869X_HAPTIC_IOCTL_MAGIC         'h'

#define AW869X_HAPTIC_SET_QUE_SEQ         _IOWR(AW869X_HAPTIC_IOCTL_MAGIC,\
						1,\
						struct aw869x_que_seq*)
#define AW869X_HAPTIC_SET_SEQ_LOOP        _IOWR(AW869X_HAPTIC_IOCTL_MAGIC,\
						2,\
						struct aw869x_seq_loop*)
#define AW869X_HAPTIC_PLAY_QUE_SEQ        _IOWR(AW869X_HAPTIC_IOCTL_MAGIC,\
						3,\
						unsigned int)
#define AW869X_HAPTIC_SET_BST_VOL         _IOWR(AW869X_HAPTIC_IOCTL_MAGIC,\
						4,\
						unsigned int)
#define AW869X_HAPTIC_SET_BST_PEAK_CUR    _IOWR(AW869X_HAPTIC_IOCTL_MAGIC,\
						5,\
						unsigned int)
#define AW869X_HAPTIC_SET_GAIN            _IOWR(AW869X_HAPTIC_IOCTL_MAGIC,\
						6,\
						unsigned int)
#define AW869X_HAPTIC_PLAY_REPEAT_SEQ     _IOWR(AW869X_HAPTIC_IOCTL_MAGIC,\
						7,\
						unsigned int)

#endif
