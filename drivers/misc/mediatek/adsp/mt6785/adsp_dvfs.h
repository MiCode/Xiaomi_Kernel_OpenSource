/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __ADSP_DVFS_H__
#define __ADSP_DVFS_H__

#include <adsp_ipi.h>
#include <adsp_clk.h>

#define ADSP_ITCM_MONITOR               (1)
#define ADSP_DTCM_MONITOR               (1)
#define ADSP_CFG_MONITOR                (0)
#define ADSP_DVFS_PROFILE               (1)
#define ADSP_FREQ_METER_ID              (43) //hf_fadsp_ck

#define CLK_DEFAULT_INIT_CK     CLK_TOP_ADSPPLL_D6
#define CLK_DEFAULT_26M_CK      CLK_ADSP_CLK26M

enum adsp_cur_status_enum {
	ADSP_STATUS_RESET =   0x00,
	ADSP_STATUS_SUSPEND = 0x01,
	ADSP_STATUS_SLEEP =   0x10,
	ADSP_STATUS_ACTIVE =  0x11,
};

enum clk_div_enum {
	CLK_DIV_1 = 0,
	CLK_DIV_2 = 1,
	CLK_DIV_4  = 2,
	CLK_DIV_8  = 3,
	CLK_DIV_UNKNOWN,
};

extern int __init adsp_dvfs_init(void);
extern void __exit adsp_dvfs_exit(void);

/* adsp dvfs variable*/
extern struct mutex adsp_feature_mutex;
extern struct mutex adsp_suspend_mutex;
extern int adsp_is_suspend;

/* adsp new implement */
void adsp_A_send_spm_request(uint32_t enable);
extern void adsp_reset(void);
void adsp_sw_reset(void);
void adsp_set_clock_freq(enum adsp_clk clk);
extern void adsp_release_runstall(uint32_t release);
extern int adsp_suspend_init(void);
void adsp_start_suspend_timer(void);
void adsp_stop_suspend_timer(void);
int adsp_resume(void);
void adsp_suspend(enum adsp_core_id core_id);
/***************************/
#endif  /* __ADSP_DVFS_H__ */
