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

#ifndef _DISP_LOWPOWER_H_
#define _DISP_LOWPOWER_H_

#define LINE_ACCURACY 1000
unsigned int dsi_phy_get_clk(enum DISP_MODULE_ENUM module);
void primary_display_idlemgr_enter_idle_nolock(void);

struct golden_setting_context *get_golden_setting_pgc(void);
int primary_display_lowpower_init(void);
void primary_display_sodi_rule_init(void);
void kick_logger_dump(char *string);
void kick_logger_dump_reset(void);
char *get_kick_dump(void);
unsigned int get_kick_dump_size(void);
/*return 0: display is not idle trigger now*/
/*return 1: display is idle*/
int primary_display_is_idle(void);
void primary_display_idlemgr_kick(const char *source, int need_lock);
void exit_pd_by_cmdq(struct cmdqRecStruct *handler);
void enter_pd_by_cmdq(struct cmdqRecStruct *handler);
void enter_share_sram(enum CMDQ_EVENT_ENUM resourceEvent);
void leave_share_sram(enum CMDQ_EVENT_ENUM resourceEvent);
void set_hrtnum(unsigned int new_hrtnum);
void set_enterulps(unsigned int flag);
void set_is_dc(unsigned int is_dc);
unsigned int set_one_layer(unsigned int is_onelayer);
void set_rdma_width_height(unsigned int width, unsigned int height);
void enable_idlemgr(unsigned int flag);
unsigned int get_idlemgr_flag(void);
unsigned int set_idlemgr(unsigned int flag, int need_lock);
int _blocking_flush(void);
unsigned int get_idlemgr_flag(void);
unsigned int set_idlemgr(unsigned int flag, int need_lock);
void primary_display_sodi_enable(int flag);
unsigned int get_us_perline(unsigned int width);
unsigned int time_to_line(unsigned int ms, unsigned int width);
/******************************** for met********************************** */
/*return 0: not enter ultra lowpower state which means mipi pll enable*/
/*return 1: enter ultra lowpower state whicn means mipi pll disable*/
unsigned int is_mipi_enterulps(void);

/*read dsi regs to calculate clk*/
unsigned int get_mipi_clk(void);

int primary_display_request_dvfs_perf(int scenario, int req);
#endif
