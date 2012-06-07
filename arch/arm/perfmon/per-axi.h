/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
*per-axi
*DESCRIPTION
*Header File for Functions related to AXI bus performance counter manipulations.
*/

#ifndef __PER_AXI_H__
#define __PER_AXI_H__
unsigned long pm_get_axi_cycle_count(void);
unsigned long pm_get_axi_evt0_count(void);
unsigned long pm_get_axi_evt1_count(void);
unsigned long pm_get_axi_evt2_count(void);
unsigned long pm_get_axi_ten_min_count(void);
unsigned long pm_get_axi_ten_max_count(void);
unsigned long pm_get_axi_ten_total_count(void);
unsigned long pm_get_axi_ten_last_count(void);

unsigned long get_axi_sel_reg0(void);
unsigned long get_axi_sel_seg1(void);
unsigned long get_axi_ten_sel_reg(void);
unsigned long get_axi_valid(void);
unsigned long get_axi_enable(void);
unsigned long get_axi_clear(void);

void pm_axi_clear_cnts(void);
void pm_axi_update_cnts(void);

void pm_axi_init(void);
void pm_axi_start(void);
void pm_axi_update(void);
void pm_axi_disable(void);
void pm_axi_enable(void);

struct perf_mon_axi_cnts{
  unsigned long long cycles;
  unsigned long long cnt0;
  unsigned long long cnt1;
  unsigned long long tenure_total;
  unsigned long long tenure_min;
  unsigned long long tenure_max;
  unsigned long long tenure_last;
};

struct perf_mon_axi_data{
  struct proc_dir_entry *proc;
  unsigned long enable;
  unsigned long clear;
  unsigned long valid;
  unsigned long sel_reg0;
  unsigned long sel_reg1;
  unsigned long ten_sel_reg;
  unsigned long refresh;
};

extern struct perf_mon_axi_data pm_axi_info;
extern struct perf_mon_axi_cnts axi_cnts;

void pm_axi_set_proc_entry(char *name, unsigned long *var,
	struct proc_dir_entry *d, int hex);
void pm_axi_get_cnt_proc_entry(char *name, struct perf_mon_axi_cnts *var,
	struct proc_dir_entry *d, int hex);

#endif
