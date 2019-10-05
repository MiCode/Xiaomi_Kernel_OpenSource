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
#include <linux/devfreq.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_qos.h>

#define VVPU_DVFS_VOLT0	 (82500)	/* mV x 100 */
#define VVPU_DVFS_VOLT1	 (72500)	/* mV x 100 */
#define VVPU_DVFS_VOLT2	 (65000)	/* mV x 100 */
#define VVPU_PTPOD_FIX_VOLT	 (80000)	/* mV x 100 */


#define VMDLA_DVFS_VOLT0	 (82500)	/* mV x 100 */
#define VMDLA_DVFS_VOLT1	 (72500)	/* mV x 100 */
#define VMDLA_DVFS_VOLT2	 (65000)	/* mV x 100 */
#define VMDLA_PTPOD_FIX_VOLT	 (80000)	/* mV x 100 */

#define USER_VPU0  0x1
#define USER_VPU1  0x2
#define USER_MDLA  0x4
#define USER_EDMA  0x8

struct vpu_opp_table_info {
	unsigned int vpufreq_khz;
	unsigned int vpufreq_volt;
	unsigned int vpufreq_idx;
};
struct mdla_opp_table_info {
	unsigned int mdlafreq_khz;
	unsigned int mdlafreq_volt;
	unsigned int mdlafreq_idx;
};

struct vpu_ptp_count_info {
	int vpu_ptp_count;
};
struct mdla_ptp_count_info {
	int mdla_ptp_count;
};


struct apu_dvfs {
	struct devfreq		*devfreq;

	bool qos_enabled;
	bool dvfs_enabled;

	void __iomem		*regs;
	void __iomem		*sram_regs;

	struct notifier_block	pm_qos_vvpu_opp_nb;
	struct notifier_block	pm_qos_vmdla_opp_nb;

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
	VVPU_OPP_NUM,
	VVPU_OPP_UNREQ = PM_QOS_VVPU_OPP_DEFAULT_VALUE,
};
enum vmdla_opp {
	VMDLA_OPP_0 = 0,
	VMDLA_OPP_1,
	VMDLA_OPP_2,
	VMDLA_OPP_NUM,
	VMDLA_OPP_UNREQ = PM_QOS_VMDLA_OPP_DEFAULT_VALUE,
};
enum APU_SEGMENT {
	SEGMENT_90M = 0,
	SEGMENT_90,
	SEGMENT_95,
	SEGMENT_NUM,
};

static inline char *apu_dvfs_dump_reg(char *ptr) { return ptr; }
static inline void apu_dvfs_reg_config(struct reg_config *config) { }
static inline int apu_dvfs_add_interface(struct device *dev) { return 0; }
static inline void apu_dvfs_remove_interface(struct device *dev) { }
static inline int apu_dvfs_platform_init(struct apu_dvfs *dvfs) { return 0; }

static inline unsigned int vvpu_get_cur_volt(void) { return 0; }
static inline unsigned int vmdla_get_cur_volt(void) { return 0; }
static inline unsigned int vvpu_update_volt(unsigned int pmic_volt[]
	, unsigned int array_size) { return 0; }
static inline void vvpu_restore_default_volt(void) { }
static inline unsigned int vpu_get_freq_by_idx(unsigned int idx) { return 0; }
static inline unsigned int vpu_get_volt_by_idx(unsigned int idx) { return 0; }
static inline void vpu_disable_by_ptpod(void) { }
static inline void vpu_enable_by_ptpod(void) { }
static inline int vvpu_regulator_set_mode(bool enable) { return 0; }
static inline int vmdla_regulator_set_mode(bool enable) { return 0; }
static inline bool vvpu_vmdla_vcore_checker(void) { return false; }
static inline unsigned int vvpu_update_ptp_count(unsigned int ptp_count[],
	unsigned int array_size) { return 0; }
static inline unsigned int vmdla_update_ptp_count(unsigned int ptp_count[],
	unsigned int array_size) { return 0; }
static inline void mdla_enable_by_ptpod(void) { }
static inline void mdla_disable_by_ptpod(void) { }
static inline bool get_vvpu_DVFS_is_paused_by_ptpod(void) { return false; }
static inline bool get_ready_for_ptpod_check(void) { return false; }
static inline int apu_power_count_enable(bool enable, int user) { return 0; }
static inline int apu_shut_down(void) { return 0; }
static inline void enable_apu_bw(unsigned int core) { }
static inline void enable_apu_latency(unsigned int core) { }
static inline int apu_dvfs_dump_info(void) { return 0; }
static inline int vpu_get_hw_vvpu_opp(int core) { return 0; }
static inline int mdla_get_hw_vmdla_opp(int core) { return 0; }
static inline unsigned int mt_get_ckgen_freq(unsigned int ID) { return 0; }
static inline void check_vpu_clk_sts(void) { }
static inline void apu_get_power_info(void) { }
static inline void ptpod_is_enabled(bool enable) { }

#endif /* __APU_DVFS_H */

