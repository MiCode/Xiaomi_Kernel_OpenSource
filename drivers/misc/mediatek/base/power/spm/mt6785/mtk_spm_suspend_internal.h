/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_SPM_SUSPEND_INTERNAL_H__
#define __MTK_SPM_SUSPEND_INTERNAL_H__

#include <mtk_spm_internal.h>

/**************************************
 * SW code for suspend
 **************************************/
#define SPM_SYSCLK_SETTLE       99	/* 3ms */
#define WAIT_UART_ACK_TIMES     10	/* 10 * 10us */
#define spm_is_wakesrc_invalid(wakesrc)     (!!((u32)(wakesrc) & 0xc0003803))
#define CPU_FOOTPRINT_SHIFT 24

#define MTK_SUSPEND_FEATURE_ENABLE (0)

enum spm_suspend_step {
	SPM_SUSPEND_ENTER = 0x00000001,
	SPM_SUSPEND_ENTER_UART_SLEEP = 0x00000003,
	SPM_SUSPEND_ENTER_WFI = 0x000000ff,
	SPM_SUSPEND_LEAVE_WFI = 0x000001ff,
	SPM_SUSPEND_ENTER_UART_AWAKE = 0x000003ff,
	SPM_SUSPEND_LEAVE = 0x000007ff,
};

int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace);
u32 spm_get_sleep_wakesrc(void);
bool spm_is_enable_sleep(void);
bool spm_get_is_cpu_pdn(void);
bool spm_get_is_infra_pdn(void);
unsigned int spm_go_to_sleep(void);
unsigned int spm_go_to_sleep_ex(unsigned int ex_flag);

bool spm_is_md_sleep(void);
bool spm_is_md1_sleep(void);
bool spm_is_md2_sleep(void);
bool spm_is_conn_sleep(void);
void spm_ap_mdsrc_req(u8 set);
ssize_t get_spm_system_stats(char *ToUserBuf, size_t sz, void *priv);
ssize_t get_spm_subsystem_stats(char *ToUserBuf, size_t sz, void *priv);
ssize_t get_spm_sleep_count(char *ToUserBuf, size_t sz, void *priv);
ssize_t get_spm_last_wakeup_src(char *ToUserBuf, size_t sz, void *priv);
ssize_t get_spm_last_debug_flag(char *ToUserBuf, size_t sz, void *priv);
u64 get_md_slp_duration(void);
ssize_t get_spmfw_version(char *ToUserBuf, size_t sz, void *priv);
ssize_t get_spm_lp_ratio_timer_enable(char *ToUserBuf, size_t sz,
	void *priv);
ssize_t set_spm_lp_ratio_timer_enable(char *ToUserBuf, size_t sz,
	void *priv);
ssize_t get_spm_resource_req_timer_enable(char *ToUserBuf,
	size_t sz, void *priv);
ssize_t set_spm_resource_req_timer_enable(char *ToUserBuf,
	size_t sz, void *priv);
ssize_t set_network_traffic(char *ToUserBuf, size_t sz, void *priv);
void spm_output_sleep_option(void);

/* record last wakesta */
u32 spm_get_last_wakeup_src(void);
u32 spm_get_last_wakeup_misc(void);
void spm_set_sysclk_settle(void);
void spm_dump_world_clk_cntcv(void);


void spm_suspend_pre_process(int cmd, struct pwr_ctrl *pwrctrl);
void spm_suspend_post_process(int cmd, struct pwr_ctrl *pwrctrl);

/**************************************
 * only for internal debug
 **************************************/
#ifdef CONFIG_MTK_LDVT
#define SPM_PWAKE_EN            0
#define SPM_PCMWDT_EN           0
#define SPM_BYPASS_SYSPWREQ     1
#define SPM_PMIC_DEBUG          0
#define VCORE_LOW_POWER_MODE	0
#else
#define SPM_PWAKE_EN            1
#define SPM_PCMWDT_EN           1
#define SPM_BYPASS_SYSPWREQ     1
#define SPM_PMIC_DEBUG          0
#define VCORE_LOW_POWER_MODE	0
#endif

/**************************************
 * External functions and variable
 **************************************/
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
extern void mt_print_scp_ipi_id(void);
#endif

#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_spm_suspend_val(u32 val);
#endif

extern int mtk8250_request_to_sleep(void);
extern int mtk8250_request_to_wakeup(void);
extern void mtk8250_backup_dev(void);
extern void mtk8250_restore_dev(void);

extern int spm_ap_mdsrc_req_cnt;
extern struct wake_status spm_wakesta;
extern unsigned int spm_sleep_count;
extern bool slp_dump_golden_setting;
extern bool slp_dump_subsys_sleep_duration;
extern bool slp_dump_ap_awake_duration;
extern int slp_dump_golden_setting_type;

#endif /* __MTK_SPM_SUSPEND_INTERNAL_H__ */
