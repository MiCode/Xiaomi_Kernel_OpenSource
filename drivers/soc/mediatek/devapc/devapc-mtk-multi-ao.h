/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __DEVAPC_MTK_MULTI_AO_H__
#define __DEVAPC_MTK_MULTI_AO_H__

#include <linux/platform_device.h>
#include <linux/types.h>

/******************************************************************************
 * VARIABLE DEFINATION
 ******************************************************************************/
#define MOD_NO_IN_1_DEVAPC	16
#define VIOLATION_TRIGGERED	1
#define DEAD			0xdeadbeaf
#define RANDOM_OFFSET		0x88
#define PFX			"[DEVAPC]: "
#define SLAVE_TYPE_NUM_MAX	7
#define IRQ_TYPE_NUM_MAX	5
#define IRQ_TYPE_NUM_DEFAULT	1
#define VIO_ADDR_HIGH_MASK	0xFFFFFFFF

#define devapc_log(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))

#define UNUSED(x)		(void)(x)

#define RETRY_COUNT	3

/******************************************************************************
 * DATA STRUCTURE & FUNCTION DEFINATION
 ******************************************************************************/
enum DEVAPC_PD_REG_TYPE {
	VIO_MASK = 0,
	VIO_STA,
	VIO_DBG0,
	VIO_DBG1,
	VIO_DBG2,
	APC_CON,
	VIO_SHIFT_STA,
	VIO_SHIFT_SEL,
	VIO_SHIFT_CON,
	VIO_DBG3,
	PD_REG_TYPE_NUM,
};

enum DEVAPC_UT_CMD {
	DEVAPC_UT_DAPC_INFRA_VIO = 1,
	DEVAPC_UT_DAPC_VLP_VIO,
	DEVAPC_UT_DAPC_ADSP_VIO,
	DEVAPC_UT_DAPC_MMINFRA_VIO,
	DEVAPC_UT_DAPC_MMUP_VIO,
	DEVAPC_UT_SRAM_VIO,
};

enum DEVAPC_DOM_ID {
	DOMAIN_0 = 0,
	DOMAIN_1,
	DOMAIN_2,
	DOMAIN_3,
	DOMAIN_4,
	DOMAIN_5,
	DOMAIN_6,
	DOMAIN_7,
	DOMAIN_8,
	DOMAIN_9,
	DOMAIN_10,
	DOMAIN_11,
	DOMAIN_12,
	DOMAIN_13,
	DOMAIN_14,
	DOMAIN_15,
	DOMAIN_OTHERS,
};

enum SRAMROM_VIO {
	ROM_VIOLATION = 0,
	SRAM_VIOLATION,
};

#ifdef CONFIG_DEVAPC_SWP_SUPPORT
enum DEVAPC_SWP_REG_OFFSET {
	DEVAPC_SWP_CON_OFFSET = 0x0,
	DEVAPC_SWP_SA_OFFSET = 0x4,
	DEVAPC_SWP_RG_OFFSET = 0x8,
	DEVAPC_SWP_WR_VAL_OFFSET = 0xC,
	DEVAPC_SWP_WR_MASK_OFFSET = 0x10,
};

enum DEVAPC_SWP_CON_BIT {
	DEVAPC_SWP_CON_ENABLE = 0x0,
	DEVAPC_SWP_CON_CLEAR,
	DEVAPC_SWP_CON_RW,
};
#endif

struct mtk_devapc_dbg_status {
	bool enable_ut;
	bool enable_KE;
	bool enable_AEE;
	bool enable_WARN;
	bool enable_dapc; /* dump APC */
};

struct mtk_device_info {
	int sys_index;
	int ctrl_index;
	int vio_index;
	const char *device;
	bool enable_vio_irq;
};

struct mtk_device_num {
	int slave_type;
	uint32_t vio_slave_num;
	int irq_type;
};

struct mtk_devapc_vio_info {
	bool read;
	bool write;
	uint32_t vio_addr;
	uint32_t vio_addr_high;
	uint32_t master_id;
	uint32_t domain_id;
	int *vio_mask_sta_num;
	int sramrom_slv_type;
	int sramrom_vio_idx;
	int mm2nd_slv_type;
	int mdp_vio_idx;
	int disp2_vio_idx;
	int mmsys_vio_idx;
	int vio_trigger_times;
	int shift_sta_bit;
};

struct mtk_infra_vio_dbg_desc {
	uint32_t vio_dbg_mstid;
	uint8_t vio_dbg_mstid_start_bit;
	uint32_t vio_dbg_dmnid;
	uint8_t vio_dbg_dmnid_start_bit;
	uint32_t vio_dbg_w_vio;
	uint8_t vio_dbg_w_vio_start_bit;
	uint32_t vio_dbg_r_vio;
	uint8_t vio_dbg_r_vio_start_bit;
	uint32_t vio_addr_high;
	uint8_t vio_addr_high_start_bit;
};

struct mtk_sramrom_sec_vio_desc {
	uint32_t vio_id_mask;
	uint8_t vio_id_shift;
	uint32_t vio_domain_mask;
	uint8_t vio_domain_shift;
	uint32_t vio_rw_mask;
	uint8_t vio_rw_shift;
};

struct mtk_devapc_pd_desc {
	uint32_t pd_vio_mask_offset;
	uint32_t pd_vio_sta_offset;
	uint32_t pd_vio_dbg0_offset;
	uint32_t pd_vio_dbg1_offset;
	uint32_t pd_apc_con_offset;
	uint32_t pd_shift_sta_offset;
	uint32_t pd_shift_sel_offset;
	uint32_t pd_shift_con_offset;
};

struct mtk_devapc_pd_reg {
	uint32_t pd_vio_dbg0_reg;
	uint32_t pd_vio_dbg1_reg;
	uint32_t pd_vio_dbg2_reg;
	uint32_t pd_vio_dbg3_reg;
	uint32_t pd_apc_con_reg;
	uint32_t pd_vio_shift_sta_reg;
	uint32_t pd_vio_shift_sel_reg;
	uint32_t pd_vio_shift_con_reg;
	uint32_t *pd_vio_mask_reg;
	uint32_t *pd_vio_sta_reg;
};

struct mtk_devapc_soc {
	struct mtk_devapc_dbg_status *dbg_stat;
	const char * const *slave_type_arr;
	uint32_t slave_type_num;
	const struct mtk_device_info *device_info[SLAVE_TYPE_NUM_MAX];
	const struct mtk_device_num *ndevices;
	struct mtk_devapc_vio_info *vio_info;
	const struct mtk_infra_vio_dbg_desc *vio_dbgs;
	const struct mtk_sramrom_sec_vio_desc *sramrom_sec_vios;
	const uint32_t *devapc_pds;
	uint32_t irq_type_num;

	/* platform specific operations */
	const char* (*subsys_get)(int slave_type, uint32_t vio_index,
			uint32_t vio_addr);
	const char* (*master_get)(uint32_t bus_id, uint32_t vio_addr,
			int slave_type, int shift_sta_bit, int domain);
	void (*mm2nd_vio_handler)(void __iomem *infracfg,
			struct mtk_devapc_vio_info *vio_info,
			bool mdp_vio, bool disp2_vio, bool mmsys_vio);
	uint32_t (*shift_group_get)(int slave_type, uint32_t vio_index);
};

int devapc_suspend_noirq(struct device *dev);
int devapc_resume_noirq(struct device *dev);
int mtk_devapc_probe(struct platform_device *pdev,
		struct mtk_devapc_soc *soc);
int mtk_devapc_remove(struct platform_device *dev);
ssize_t mtk_devapc_dbg_read(struct file *file, char __user *buffer,
	size_t count, loff_t *ppos);
ssize_t mtk_devapc_dbg_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data);

#endif /* __DEVAPC_MTK_MULTI_AO_H__ */
