/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MTK_SPM_VCORE_DVFS_MT6771_H__
#define __MTK_SPM_VCORE_DVFS_MT6771_H__

#include "mtk_spm.h"
#include <mtk_vcorefs_manager.h>

/* Feature will disable both of DVS/DFS are 0 */
/* TODO: enable after function verify */
#define SPM_VCORE_DVS_EN       1 /* SB disabled */
#define SPM_DDR_DFS_EN         1 /* SB disabled */
#define SPM_MM_CLK_EN          0 /* for intra-frame dvfs */
#define VMODEM_VCORE_COBUCK    1 /* SB disabled */

#define SPM_DVFS_TIMEOUT       1000	/* 1ms */

enum vcorefs_smc_cmd {
	VCOREFS_SMC_CMD_0,
	VCOREFS_SMC_CMD_1,
	VCOREFS_SMC_CMD_2,
	VCOREFS_SMC_CMD_3,
	NUM_VCOREFS_SMC_CMD,
};


#define QOS_SRAM_BASE (qos_sram_base)

#define QOS_TOTAL_BW_BUF_SIZE 8

#define QOS_TOTAL_BW_BUF(idx) (QOS_SRAM_BASE + idx * 4)
#define QOS_TOTAL_BW          (QOS_SRAM_BASE + QOS_TOTAL_BW_BUF_SIZE * 4)
#define QOS_CPU_BW            (QOS_SRAM_BASE + QOS_TOTAL_BW_BUF_SIZE * 4 + 0x4)
#define QOS_MM_BW             (QOS_SRAM_BASE + QOS_TOTAL_BW_BUF_SIZE * 4 + 0x8)
#define QOS_GPU_BW            (QOS_SRAM_BASE + QOS_TOTAL_BW_BUF_SIZE * 4 + 0xC)
#define QOS_MD_PERI_BW        (QOS_SRAM_BASE + QOS_TOTAL_BW_BUF_SIZE * 4 + 0x10)
#define QOS_SRAM_SEG          (QOS_SRAM_BASE + 0x7C)

enum {
	QOS_TOTAL = 0,
	QOS_CPU,
	QOS_MM,
	QOS_GPU,
	QOS_MD_PERI,
	QOS_TOTAL_AVE
};

/* met profile table index */
enum met_info_index {
	INFO_OPP_IDX = 0,
	INFO_FREQ_IDX,
	INFO_VCORE_IDX,
	INFO_SW_RSV5_IDX,
	INFO_MAX,
};

enum met_src_index {
	SRC_MD2SPM_IDX = 0,
	SRC_QOS_EMI_LEVEL_IDX,
	SRC_QOS_VCORE_LEVEL_IDX,
	SRC_CM_MGR_LEVEL_IDX,
	SRC_TOTAL_EMI_LEVEL_1_IDX,
	SRC_TOTAL_EMI_LEVEL_2_IDX,
	SRC_TOTAL_EMI_MON_BW_IDX,
	SRC_QOS_BW_LEVEL1_IDX,
	SRC_QOS_BW_LEVEL2_IDX,
	SRC_SCP_VCORE_LEVEL_IDX,
	SRC_MAX
};

extern void spm_go_to_vcorefs(int spm_flags);
extern int spm_set_vcore_dvfs(struct kicker_config *krconf);
extern void spm_vcorefs_init(void);
extern int spm_dvfs_flag_init(void);
extern char *spm_vcorefs_dump_dvfs_regs(char *p);
extern u32 spm_vcorefs_get_MD_status(void);
extern int spm_vcorefs_pwarp_cmd(void);
extern int spm_vcorefs_get_opp(void);
extern void spm_request_dvfs_opp(int id, enum dvfs_opp opp);
extern u32 spm_vcorefs_get_md_srcclkena(void);
extern void dvfsrc_md_scenario_update_to_fb(bool suspend);
extern void dvfsrc_set_scp_vcore_request(unsigned int val);
extern void dvfsrc_set_power_model_ddr_request(unsigned int level);
extern void helio_dvfsrc_sspm_ipi_init(int dvfs_en, int dram_type);
extern void dvfsrc_hw_policy_mask(bool mask);
extern int spm_get_vcore_opp(unsigned int opp);
extern int spm_vcorefs_get_dvfs_opp(void);
extern void dvfsrc_update_sspm_vcore_opp_table(int opp, unsigned int vcore_uv);
extern void dvfsrc_update_sspm_ddr_opp_table(int opp, unsigned int ddr_khz);
extern void dvfsrc_update_sspm_qos_enable(int dvfs_en, unsigned int dram_type);
extern void vcorefs_temp_opp_config(int temp);
extern void vcorefs_set_lt_opp_feature(int en);
extern void vcorefs_set_lt_opp_enter_temp(int val);
extern void vcorefs_set_lt_opp_leave_temp(int val);
extern int is_force_opp_enable(void);

/* met profile function */
extern int vcorefs_get_opp_info_num(void);
extern char **vcorefs_get_opp_info_name(void);
extern unsigned int *vcorefs_get_opp_info(void);
extern int vcorefs_get_src_req_num(void);
extern char **vcorefs_get_src_req_name(void);
extern unsigned int *vcorefs_get_src_req(void);

#endif /* __MTK_SPM_VCORE_DVFS_MT6771_H__ */
