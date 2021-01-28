/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __DEVAPC_MTK_COMMON_H__
#define __DEVAPC_MTK_COMMON_H__

#include <linux/platform_device.h>
#include <linux/types.h>

/******************************************************************************
 * LOG DEFINATION
 ******************************************************************************/

/* Debug message event */
#define DEVAPC_LOG_NONE		0x00000000
#define DEVAPC_LOG_INFO		0x00000001
#define DEVAPC_LOG_DBG		0x00000002

#ifdef DEBUG
#define DEVAPC_LOG_LEVEL	(DEVAPC_LOG_DBG)
#else
#define DEVAPC_LOG_LEVEL	(DEVAPC_LOG_NONE)
#endif

#define PFX			KBUILD_MODNAME ": "

#define DEVAPC_DBG_MSG(fmt, args...) \
	do {    \
		if (DEVAPC_LOG_LEVEL & DEVAPC_LOG_DBG) { \
			pr_debug(PFX fmt, ##args); \
		} else if (DEVAPC_LOG_LEVEL & DEVAPC_LOG_INFO) { \
			pr_info(PFX fmt, ##args); \
		} \
	} while (0)


#define DEVAPC_VIO_LEVEL      (DEVAPC_LOG_INFO)

#define DEVAPC_MSG(fmt, args...) \
	do {    \
		if (DEVAPC_VIO_LEVEL & DEVAPC_LOG_DBG) { \
			pr_debug(PFX fmt, ##args); \
		} else if (DEVAPC_VIO_LEVEL & DEVAPC_LOG_INFO) { \
			pr_info(PFX fmt, ##args); \
		} \
	} while (0)


#define log2buf(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))

#define devapc_log(fmt, args...)  log2buf(p, msg_buf, fmt, ##args)

/******************************************************************************
 * VARIABLE DEFINATION
 ******************************************************************************/

#define MOD_NO_IN_1_DEVAPC	16
#define VIOLATION_TRIGGERED	1
#define DEAD			0xdeadbeaf

/******************************************************************************
 * DATA STRUCTURE & FUNCTION DEFINATION
 ******************************************************************************/
enum DEVAPC_DT_NODE_INDEX {
	DT_DEVAPC_PD_IDX = 0,
	DT_DEVAPC_AO_IDX,
	DT_SRAMROM_IDX,
};

enum DEVAPC_PD_REG_TYPE {
	VIO_MASK = 0,
	VIO_STA,
	VIO_DBG0,
	VIO_DBG1,
	APC_CON,
	VIO_SHIFT_STA,
	VIO_SHIFT_SEL,
	VIO_SHIFT_CON,
};

enum DEVAPC_UT_CMD {
	DEVAPC_UT_DAPC_VIO = 1,
	DEVAPC_UT_SRAM_VIO,
	DEVAPC_UT_DUMP_SUBSYS_CB,
};

enum DEVAPC_SLAVE_TYPE {
	E_DAPC_INFRA_PERI_SLAVE = 0,
	E_DAPC_MM_SLAVE,
	E_DAPC_MD_SLAVE,
	E_DAPC_PERI_SLAVE,
	E_DAPC_MM2ND_SLAVE,
	E_DAPC_OTHERS_SLAVE,
	E_DAPC_SLAVE_TYPE_RESERVRD = 0x7FFFFFFF  /* force enum to use 32 bits */
};

enum E_MASK_DOM {
	E_DOMAIN_0 = 0,
	E_DOMAIN_1,
	E_DOMAIN_2,
	E_DOMAIN_3,
	E_DOMAIN_4,
	E_DOMAIN_5,
	E_DOMAIN_6,
	E_DOMAIN_7,
	E_DOMAIN_8,
	E_DOMAIN_9,
	E_DOMAIN_10,
	E_DOMAIN_11,
	E_DOMAIN_12,
	E_DOMAIN_13,
	E_DOMAIN_14,
	E_DOMAIN_15,
	E_DOMAIN_OTHERS,
	E_MASK_DOM_RESERVRD = 0x7FFFFFFF  /* force enum to use 32 bits */
};

enum SRAMROM_VIO {
	ROM_VIOLATION = 0,
	SRAM_VIOLATION,
};

struct mtk_devapc_dbg_status {
	bool enable_ut;
	bool enable_KE;
	bool enable_AEE;
	bool enable_dapc; /* dump APC */
};

struct mtk_device_info {
	enum DEVAPC_SLAVE_TYPE slave_type;
	int config_index;
	const char *device;
	bool enable_vio_irq;
};

struct mtk_devapc_vio_info {
	uint32_t vio_dbg1;
	uint32_t master_id;
	uint32_t domain_id;
	int vio_cfg_max_idx;
	int vio_max_idx;
	int vio_mask_sta_num;
	int vio_shift_max_bit;
	int sramrom_vio_idx;
	int devapc_vio_trigger_times;
};

struct mtk_infra_vio_dbg_desc {
	uint32_t infra_vio_dbg_mstid;
	uint8_t infra_vio_dbg_mstid_start_bit;
	uint32_t infra_vio_dbg_dmnid;
	uint8_t infra_vio_dbg_dmnid_start_bit;
	uint32_t infra_vio_dbg_w_vio;
	uint8_t infra_vio_dbg_w_vio_start_bit;
	uint32_t infra_vio_dbg_r_vio;
	uint8_t infra_vio_dbg_r_vio_start_bit;
	uint32_t infra_vio_addr_high;
	uint8_t infra_vio_addr_high_start_bit;
};

struct mtk_sramrom_sec_vio_desc {
	uint32_t sramrom_sec_vio_id_mask;
	uint8_t sramrom_sec_vio_id_shift;
	uint32_t sramrom_sec_vio_domain_mask;
	uint8_t sramrom_sec_vio_domain_shift;
	uint32_t sramrom_sec_vio_rw_mask;
	uint8_t sramrom_sec_vio_rw_shift;
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

struct mtk_devapc_soc {
	struct mtk_devapc_dbg_status *dbg_stat;
	const struct mtk_device_info *device_info;
	uint32_t ndevices;
	struct mtk_devapc_vio_info *vio_info;
	const struct mtk_infra_vio_dbg_desc *vio_dbgs;
	const struct mtk_sramrom_sec_vio_desc *sramrom_sec_vios;
	const struct mtk_devapc_pd_desc *devapc_pds;

	/* platform specific operations */
	const char* (*subsys_get)(uint32_t index);
	const char* (*master_get)(int bus_id, uint32_t vio_addr,
			int vio_idx);
};

void handle_sramrom_vio(void);
int mtk_devapc_probe(struct platform_device *pdev,
		struct mtk_devapc_soc *soc);
int mtk_devapc_remove(struct platform_device *dev);
ssize_t mtk_devapc_dbg_read(struct file *file, char __user *buffer,
	size_t count, loff_t *ppos);
ssize_t mtk_devapc_dbg_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data);

#endif /* __DEVAPC_MTK_COMMON_H__ */
