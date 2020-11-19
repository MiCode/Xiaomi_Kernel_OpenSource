/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MNOC_PLAT_INTERNAL_H__
#define __MNOC_PLAT_INTERNAL_H__

#include <linux/platform_device.h>

struct mnoc_plat_drv {
	void (*init)(void);
	void (*exit)(void);
	int (*dev_2_core_id)(int dev_type, int dev_core);
	void (*int_endis)(bool endis);
	void (*print_int_sta)(struct seq_file *m);
	int (*chk_int_status)(void);
	void (*hw_reinit)(void);
	void (*get_pmu_counter)(unsigned int *buf);
	void (*clr_pmu_counter)(unsigned int grp);
	bool (*pmu_reg_in_range)(unsigned int addr);

	phys_addr_t (*get_apu_iommu_tfrp)(unsigned int id);
	void (*set_mni_pre_ultra)(int dev_type, int dev_core, bool endis);
	void (*set_lt_guardian_pre_ultra)(int dev_type, int dev_core, bool endis);

	int apu_qos_engine_count;
	unsigned int *vcore_bw_opp_tab;
	unsigned int nr_vcore_opp;
};


phys_addr_t get_apu_iommu_tfrp_v1_50(unsigned int id);
void mnoc_set_mni_pre_ultra_v1_50(int dev_type, int dev_core, bool endis);
void mnoc_set_lt_guardian_pre_ultra_v1_50(int dev_type, int dev_core, bool endis);

int apusys_dev_to_core_id_v1_50(int dev_type, int dev_core);

void mnoc_int_endis_v1_50(bool endis);
void print_int_sta_v1_50(struct seq_file *m);
int mnoc_check_int_status_v1_50(void);

void mnoc_hw_reinit_v1_50(void);

void mnoc_get_pmu_counter_v1_50(unsigned int *buf);
void mnoc_clear_pmu_counter_v1_50(unsigned int grp);
bool mnoc_pmu_reg_in_range_v1_50(unsigned int addr);

void mnoc_hw_v1_50_init(void);
void mnoc_hw_v1_50_exit(void);



phys_addr_t get_apu_iommu_tfrp_v1_51(unsigned int id);
void mnoc_set_mni_pre_ultra_v1_51(int dev_type, int dev_core, bool endis);
void mnoc_set_lt_guardian_pre_ultra_v1_51(int dev_type, int dev_core, bool endis);

int apusys_dev_to_core_id_v1_51(int dev_type, int dev_core);

void mnoc_int_endis_v1_51(bool endis);
void print_int_sta_v1_51(struct seq_file *m);
int mnoc_check_int_status_v1_51(void);

void mnoc_hw_reinit_v1_51(void);

void mnoc_get_pmu_counter_v1_51(unsigned int *buf);
void mnoc_clear_pmu_counter_v1_51(unsigned int grp);
bool mnoc_pmu_reg_in_range_v1_51(unsigned int addr);

void mnoc_hw_v1_51_init(void);
void mnoc_hw_v1_51_exit(void);




phys_addr_t get_apu_iommu_tfrp_v1_52(unsigned int id);
void mnoc_set_mni_pre_ultra_v1_52(int dev_type, int dev_core, bool endis);
void mnoc_set_lt_guardian_pre_ultra_v1_52(int dev_type, int dev_core, bool endis);

int apusys_dev_to_core_id_v1_52(int dev_type, int dev_core);

void mnoc_int_endis_v1_52(bool endis);
void print_int_sta_v1_52(struct seq_file *m);
int mnoc_check_int_status_v1_52(void);

void mnoc_hw_reinit_v1_52(void);

void mnoc_get_pmu_counter_v1_52(unsigned int *buf);
void mnoc_clear_pmu_counter_v1_52(unsigned int grp);
bool mnoc_pmu_reg_in_range_v1_52(unsigned int addr);

void mnoc_hw_v1_52_init(void);
void mnoc_hw_v1_52_exit(void);

void mnoc_met_pmu_reg_init_v1_52(void);
void mnoc_met_pmu_reg_uninit_v1_52(void);


#endif /* __MNOC_PLAT_INTERNAL_H__ */

