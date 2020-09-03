/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_SPM_INTERNAL_H__
#define __MTK_SPM_INTERNAL_H__

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/io.h>

#include <mt-plat/mtk_secure_api.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/aee.h>

#include <mtk_spm_reg.h>
#include <pwr_ctrl.h>
//FIXME
#include <mtk_spm.h>


/* IMPORTANT NOTE: Check cpuidle header file version every time !! */
/* For mt6763, use cpuidle_v2/mtk_cpuidle.h */
/* For mt6739/mt6775/mt6771 or later project, use cpuidle_v3/mtk_cpuidle.h */
#include <cpuidle_v3/mtk_cpuidle.h> /* atf/dormant header file */

/**************************************
 * Config and Parameter
 **************************************/
#define LOG_BUF_SIZE        256
#define SPM_WAKE_PERIOD     600 /* sec */

/**************************************
 * Define and Declare
 **************************************/
#define PCM_TIMER_RAMP_BASE_DPIDLE          80          /* 80/32000 = 2.5 ms */
#define PCM_TIMER_RAMP_BASE_SUSPEND_50MS    0xA0
#define PCM_TIMER_RAMP_BASE_SUSPEND_SHORT   0x7D000     /* 16sec */
#define PCM_TIMER_RAMP_BASE_SUSPEND_LONG    0x927C00    /* 5min */
/* PCM_WDT_VAL */
#define PCM_WDT_TIMEOUT		(30 * 32768)	/* 30s */
/* PCM_TIMER_VAL */
#define PCM_TIMER_MAX		(0xffffffff - PCM_WDT_TIMEOUT)

/* SMC call's marco */
#define SMC_CALL(_name, _arg0, _arg1, _arg2) \
	mt_secure_call(MTK_SIP_KERNEL_SPM_##_name, _arg0, _arg1, _arg2, 0)

extern spinlock_t __spm_lock;

extern void __iomem *spm_base;
#undef SPM_BASE
#define SPM_BASE spm_base

extern void __iomem *sleep_reg_md_base;
#undef SLEEP_REG_MD_BASE
#define SLEEP_REG_MD_BASE sleep_reg_md_base


extern struct pwr_ctrl pwrctrl_bus26m;
extern struct pwr_ctrl pwrctrl_syspll;
extern struct pwr_ctrl pwrctrl_dram;

/* SMC secure magic number */
#define SPM_LP_SMC_MAGIC	0xDAF10000

/* SMC: defined parameters for MTK_SIP_KERNEL_SPM_ARGS */
enum {
	SPM_ARGS_SPMFW_IDX = 0,
	SPM_ARGS_SPMFW_INIT,
	SPM_ARGS_SUSPEND,
	SPM_ARGS_SUSPEND_FINISH,
	SPM_ARGS_SODI,
	SPM_ARGS_SODI_FINISH,
	SPM_ARGS_DPIDLE,
	SPM_ARGS_DPIDLE_FINISH,
	SPM_ARGS_PCM_WDT,
	SPM_ARGS_IDLE_DRAM,
	SPM_ARGS_IDLE_DRAM_FINISH,
	SPM_ARGS_IDLE_SYSPLL,
	SPM_ARGS_IDLE_SYSPLL_FINISH,
	SPM_ARGS_IDLE_BUS26M,
	SPM_ARGS_IDLE_BUS26M_FINISH,
	SPM_ARGS_NUM,
};

/* Note: used by kernel/sspm/atf */
enum {
	SPM_SUSPEND,
	SPM_RESUME,
	SPM_DPIDLE_ENTER,
	SPM_DPIDLE_LEAVE,
	SPM_ENTER_SODI,
	SPM_LEAVE_SODI,
	SPM_ENTER_SODI3,
	SPM_LEAVE_SODI3,
	SPM_SUSPEND_PREPARE,
	SPM_POST_SUSPEND,
	SPM_DPIDLE_PREPARE,
	SPM_POST_DPIDLE,
	SPM_SODI_PREPARE,
	SPM_POST_SODI,
	SPM_SODI3_PREPARE,
	SPM_POST_SODI3,
	SPM_VCORE_PWARP_CMD,
	SPM_PWR_CTRL_SUSPEND,
	SPM_PWR_CTRL_DPIDLE,
	SPM_PWR_CTRL_SODI,
	SPM_PWR_CTRL_SODI3,
	SPM_PWR_CTRL_VCOREFS,
	SPM_TWAM_ENABLE,
	SPM_PWR_CTRL_IDLE_DRAM,
	SPM_PWR_CTRL_IDLE_SYSPLL,
	SPM_PWR_CTRL_IDLE_BUS26M,
};

enum vcorefs_smc_cmd {
	VCOREFS_SMC_CMD_0,
	VCOREFS_SMC_CMD_1,
	VCOREFS_SMC_CMD_2,
	VCOREFS_SMC_CMD_3,
	VCOREFS_SMC_CMD_4,
	NUM_VCOREFS_SMC_CMD,
};

enum {
	WR_NONE = 0,
	WR_UART_BUSY = 1,
	WR_PCM_ASSERT = 2,
	WR_PCM_TIMER = 3,
	WR_WAKE_SRC = 4,
	WR_UNKNOWN = 5,
};

struct wake_status {
	u32 assert_pc;      /* PCM_REG_DATA_INI */
	u32 r12;            /* PCM_REG12_DATA */
	u32 r12_ext;        /* PCM_REG12_EXT_DATA */
	u32 raw_sta;        /* SPM_WAKEUP_STA */
	u32 raw_ext_sta;    /* SPM_WAKEUP_EXT_STA */
	u32 wake_misc;      /* SPM_WAKEUP_MISC */
	u32 timer_out;      /* PCM_TIMER_OUT */
	u32 r13;            /* PCM_REG13_DATA */
	u32 idle_sta;       /* SUBSYS_IDLE_STA */
	u32 req_sta;        /* SRC_REQ_STA */
	u32 debug_flag;     /* SPM_SW_DEBUG */
	u32 debug_flag1;    /* WDT_LATCH_SPARE0_FIX */
	u32 event_reg;      /* PCM_EVENT_REG_STA */
	u32 isr;            /* SPM_IRQ_STA */
	u32 log_index;
};

struct spm_lp_scen {
	struct pcm_desc *pcmdesc;
	struct pwr_ctrl *pwrctrl;
	struct wake_status *wakestatus;
};

/***********************************************************
 * mtk_spm.c
 ***********************************************************/
void spm_pm_stay_awake(int sec);
int spm_load_firmware_status(void);


/***********************************************************
 * mtk_spm_irq.c
 ***********************************************************/

int mtk_spm_irq_register(unsigned int spm_irq_0);
void mtk_spm_irq_backup(void);
void mtk_spm_irq_restore(void);


/***********************************************************
 * mtk_spm_internal.c
 ***********************************************************/

long int spm_get_current_time_ms(void);
int __spm_get_pcm_timer_val(const struct pwr_ctrl *pwrctrl);
void __spm_set_pwrctrl_pcm_flags(struct pwr_ctrl *pwrctrl, u32 flags);
void __spm_set_pwrctrl_pcm_flags1(struct pwr_ctrl *pwrctrl, u32 flags);
void __spm_sync_pcm_flags(struct pwr_ctrl *pwrctrl);
void __spm_get_wakeup_status(struct wake_status *wakesta);
unsigned int __spm_output_wake_reason(
	const struct wake_status *wakesta, bool suspend, const char *scenario);
unsigned int __spm_get_wake_period(int pwake_time, unsigned int last_wr);

/***********************************************************
 * mtk_spm_twam.c
 ***********************************************************/
 /* SPM_IRQ_MASK */
#define ISRM_TWAM       (1U << 2)
#define ISRM_PCM_RETURN (1U << 3)
#define ISRM_RET_IRQ0   (1U << 8)
#define ISRM_RET_IRQ1   (1U << 9)
#define ISRM_RET_IRQ2   (1U << 10)
#define ISRM_RET_IRQ3   (1U << 11)
#define ISRM_RET_IRQ4   (1U << 12)
#define ISRM_RET_IRQ5   (1U << 13)
#define ISRM_RET_IRQ6   (1U << 14)
#define ISRM_RET_IRQ7   (1U << 15)
#define ISRM_RET_IRQ8   (1U << 16)
#define ISRM_RET_IRQ9   (1U << 17)
#define ISRM_RET_IRQ_AUX (\
	ISRM_REQ_IRQ1 | ISRM_REQ_IRQ2 | ISRM_REQ_IRQ3 | \
	ISRM_REQ_IRQ4 | ISRM_REQ_IRQ5 | ISRM_REQ_IRQ6 | \
	ISRM_REQ_IRQ7 | ISRM_REQ_IRQ8 | ISRM_REQ_IRQ9)
#define ISRM_ALL_EXC_TWAM \
	(ISRM_RET_IRQ_AUX /*| ISRM_RET_IRQ0 | ISRM_PCM_RETURN*/)
#define ISRM_ALL        (ISRM_ALL_EXC_TWAM | ISRM_TWAM)

/* SPM_IRQ_STA */
#define ISRS_TWAM           (1U << 2)
#define ISRS_PCM_RETURN     (1U << 3)
#define ISRS_SW_INT0        (1U << 4)
#define ISRS_SW_INT1        (1U << 5)
#define ISRC_TWAM           ISRS_TWAM
#define ISRC_ALL_EXC_TWAM   ISRS_PCM_RETURN
#define ISRC_ALL            (ISRC_ALL_EXC_TWAM | ISRC_TWAM)

void spm_twam_register_handler(twam_handler_t handler);
twam_handler_t spm_twam_handler_get(void);
void spm_twam_register_handler(twam_handler_t handler);
void spm_twam_enable_monitor(bool en_monitor, bool debug_signal,
			twam_handler_t cb_handler);
void spm_twam_config_channel(struct twam_cfg *cfg, bool speed_mode,
			unsigned int window_len_hz);
bool spm_twam_met_enable(void);
void spm_twam_set_idle_select(unsigned int sel);

/***********************************************************
 * mtk_spm_dram.c
 ***********************************************************/
struct ddrphy_golden_cfg {
	u32 base;
	u32 offset;
	u32 mask;
	u32 value;
};

enum {
	SPMFW_LP4_2CH_3200 = 0,
	SPMFW_LP4X_2CH_3600,
	SPMFW_LP3_1CH_1866,
};

int spm_get_spmfw_idx(void);
void spm_do_dram_config_check(void);


/***********************************************************
 * mtk_spm_power.c
 ***********************************************************/

void mtk_idle_power_pre_process(int idle_type, unsigned int op_cond);
void mtk_idle_power_pre_process_async_wait(int idle_type, unsigned int op_cond);
void mtk_idle_power_post_process(int idle_type, unsigned int op_cond);
void mtk_idle_power_post_process_async_wait(int idle_type,
	unsigned int op_cond);


/***********************************************************
 * mtk_spm_idle.c
 ***********************************************************/

/* call dormant/atf driver for idle scenario */
extern int mtk_idle_trigger_wfi(
	int idle_type, unsigned int idle_flag, int cpu);
/* call before wfi and setup spm related settings */
extern void mtk_idle_pre_process_by_chip(int idle_type, int cpu,
	unsigned int op_cond, unsigned int idle_flag);
/* call after wfi and clean up spm related settings */
extern void mtk_idle_post_process_by_chip(int idle_type, int cpu,
	unsigned int op_cond, unsigned int idle_flag);

/* get sleep dpidle last wake reason */
extern unsigned int get_slp_dp_last_wr(void);
/***********************************************************
 * mtk_idle_cond_check.c
 ***********************************************************/

/* append idle block info to input logbuf */
extern int mtk_idle_cond_append_info(
	bool short_log, unsigned int idle_type, char *logptr, unsigned int logsize);

/* enable idle cg monitor and print log to SYS_FTRACE */
extern void mtk_idle_cg_monitor(int sel);

/* read mtcmos/cg statue for later check */
extern void mtk_idle_cond_update_state(void);

/* check idle condition for specific idle type */
extern bool mtk_idle_cond_check(unsigned int idle_type);

/* check clkmux for vcore lp mode */
extern bool mtk_idle_cond_vcore_lp_mode(int idle_type);

/* mask/unmask block mask */
void mtk_idle_cond_update_mask(
	unsigned int idle_type, unsigned int reg, unsigned int mask);

/***********************************************************
 * mtk_spm_vcorefs.c
 ***********************************************************/
int spm_dvfs_flag_init(int dvfsrc_en);
void spm_go_to_vcorefs(int spm_flags);
void spm_vcorefs_init(void);
int is_spm_enabled(void);
/***********************************************************
 * mtk_spm_fs.c
 ***********************************************************/
extern struct spm_lp_scen __spm_suspend;


#endif /* __MTK_SPM_INTERNAL_H__ */
