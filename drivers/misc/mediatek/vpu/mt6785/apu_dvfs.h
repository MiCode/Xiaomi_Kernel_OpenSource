/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __APU_DVFS_H
#define __APU_DVFS_H

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_qos.h>

#define VVPU_DVFS_VOLT0	 (82500)	/* mV x 100 */
#define VVPU_DVFS_VOLT1	 (72500)	/* mV x 100 */
#define VVPU_DVFS_VOLT2	 (65000)	/* mV x 100 */
#define VVPU_PTPOD_FIX_VOLT	 (80000)	/* mV x 100 */



#define USER_VPU0  0x1
#define USER_VPU1  0x2

struct vpu_opp_table_info {
	unsigned int vpufreq_khz;
	unsigned int vpufreq_volt;
	unsigned int vpufreq_idx;
};


struct vpu_ptp_count_info {
	int vpu_ptp_count;
};



struct apu_dvfs {
	bool qos_enabled;
	bool dvfs_enabled;

	void __iomem		*regs;
	void __iomem		*sram_regs;

	struct notifier_block	pm_qos_vvpu_opp_nb;

	struct reg_config	*init_config;
	struct device_node *dvfs_node;
	unsigned int dvfs_irq;

	bool opp_forced;
	char			force_start[20];
	char			force_end[20];
};

enum vvpu_opp {
	VVPU_OPP_0 = 0,
	VVPU_OPP_1,
	VVPU_OPP_2,
	VVPU_OPP_3,
	VVPU_OPP_NUM,
	VVPU_OPP_UNREQ = PM_QOS_VVPU_OPP_DEFAULT_VALUE,
};



extern char *apu_dvfs_dump_reg(char *ptr);
extern void apu_dvfs_reg_config(struct reg_config *config);
extern int apu_dvfs_add_interface(struct device *dev);
extern void apu_dvfs_remove_interface(struct device *dev);
extern int apu_dvfs_platform_init(struct apu_dvfs *dvfs);

extern unsigned int vvpu_get_cur_volt(void);
extern unsigned int vvpu_update_volt(unsigned int pmic_volt[]
	, unsigned int array_size);
extern void vvpu_restore_default_volt(void);
extern unsigned int vpu_get_freq_by_idx(unsigned int idx);
extern unsigned int vpu_get_volt_by_idx(unsigned int idx);
extern void vpu_disable_by_ptpod(void);
extern void vpu_enable_by_ptpod(void);
int vvpu_regulator_set_mode(bool enable);
unsigned int vvpu_update_ptp_count(unsigned int ptp_count[],
	unsigned int array_size);
bool get_vvpu_DVFS_is_paused_by_ptpod(void);
bool get_ready_for_ptpod_check(void);
int apu_power_count_enable(bool enable, int user);
int apu_shut_down(void);
void enable_apu_bw(unsigned int core);
void enable_apu_latency(unsigned int core);
int apu_dvfs_dump_info(void);
int vpu_get_hw_vvpu_opp(int core);
extern unsigned int mt_get_ckgen_freq(unsigned int ID);
void apu_get_power_info(void);



#endif /* __APU_DVFS_H */

