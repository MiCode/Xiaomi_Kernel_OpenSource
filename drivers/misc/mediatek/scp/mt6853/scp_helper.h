/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __SCP_HELPER_H__
#define __SCP_HELPER_H__

#include <linux/notifier.h>
#include <linux/interrupt.h>
#include "scp_reg.h"
#include "scp_feature_define.h"

#define ROUNDUP(a, b)		        (((a) + ((b)-1)) & ~((b)-1))

/* scp config reg. definition*/
#define SCP_TCM_SIZE		(scpreg.total_tcmsize)
#define SCP_A_TCM_SIZE		(scpreg.scp_tcmsize)
#define SCP_TCM			(scpreg.sram)
#define SCP_REGION_INFO_OFFSET	0x4
#define SCP_RTOS_START		(0x800)

#define SCP_DRAM_MAPSIZE	(0x100000)

/* scp dvfs return status flag */
#define SET_PLL_FAIL		(1)
#define SET_PMIC_VOLT_FAIL	(2)

/* This structre need to sync with SCP-side */
struct SCP_IRQ_AST_INFO {
	unsigned int scp_irq_ast_time;
	unsigned int scp_irq_ast_pc_s;
	unsigned int scp_irq_ast_pc_e;
	unsigned int scp_irq_ast_lr_s;
	unsigned int scp_irq_ast_lr_e;
	unsigned int scp_irq_ast_irqd;
};

/* scp notify event */
enum SCP_NOTIFY_EVENT {
	SCP_EVENT_READY = 0,
	SCP_EVENT_STOP,
};

/* reset ID */
#define SCP_ALL_ENABLE	0x00
#define SCP_ALL_REBOOT	0x01

/* scp semaphore definition*/
enum SEMAPHORE_FLAG {
	SEMAPHORE_CLK_CFG_5 = 0,
	SEMAPHORE_PTP,
	SEMAPHORE_I2C0,
	SEMAPHORE_I2C1,
	SEMAPHORE_TOUCH,
	SEMAPHORE_APDMA,
	SEMAPHORE_SENSOR,
	SEMAPHORE_SCP_A_AWAKE,
	SEMAPHORE_SCP_B_AWAKE,
	NR_FLAG = 9,
};

/* scp reset status */
enum SCP_RESET_STATUS {
	RESET_STATUS_STOP = 0,
	RESET_STATUS_START = 1,
};

/* scp reset status */
enum SCP_RESET_TYPE {
	RESET_TYPE_WDT = 0,
	RESET_TYPE_AWAKE = 1,
	RESET_TYPE_CMD = 2,
	RESET_TYPE_TIMEOUT = 3,
};

struct scp_regs {
	void __iomem *scpsys;
	void __iomem *sram;
	void __iomem *cfg;
	void __iomem *clkctrl;
	void __iomem *l1cctrl;
	void __iomem *cfg_core0;
	void __iomem *cfg_core1;
	void __iomem *cfg_sec;
	void __iomem *bus_tracker;
	int irq;
	unsigned int total_tcmsize;
	unsigned int cfgregsize;
	unsigned int scp_tcmsize;
};

/* scp work struct definition*/
struct scp_work_struct {
	struct work_struct work;
	unsigned int flags;
	unsigned int id;
};

/* scp reserve memory ID definition*/
enum scp_reserve_mem_id_t {
#ifdef CONFIG_MTK_VOW_SUPPORT
	VOW_MEM_ID,
#endif
	SENS_MEM_ID,
	SCP_A_LOGGER_MEM_ID,
#if defined(CONFIG_SND_SOC_MTK_SCP_SMARTPA) || \
	defined(CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT) || \
	defined(CONFIG_MTK_AUDIO_TUNNELING_SUPPORT) || \
	defined(CONFIG_MTK_VOW_SUPPORT)
	AUDIO_IPI_MEM_ID,
#endif
#ifdef CONFIG_MTK_VOW_BARGE_IN_SUPPORT
	VOW_BARGEIN_MEM_ID,
#endif
#ifdef SCP_PARAMS_TO_SCP_SUPPORT
	SCP_DRV_PARAMS_MEM_ID,
#endif
	NUMS_MEM_ID,
};


struct scp_reserve_mblock {
	enum scp_reserve_mem_id_t num;
	u64 start_phys;
	u64 start_virt;
	u64 size;
};

struct scp_region_info_st {
	uint32_t ap_loader_start;
	uint32_t ap_loader_size;
	uint32_t ap_firmware_start;
	uint32_t ap_firmware_size;
	uint32_t ap_dram_start;
	uint32_t ap_dram_size;
	uint32_t ap_dram_backup_start;
	/*	This is the size of the structure.
	 *	It can act as a version number if entries can only be
	 *	added to (not deleted from) the structure.
	 *	It should be the first entry of the structure, but for
	 *	compatibility reason, it is appended here.
	 */
	uint32_t struct_size;
	uint32_t scp_log_thru_ap_uart;
	uint32_t TaskContext_ptr;
	uint32_t scpctl;
	uint32_t regdump_start;
	uint32_t regdump_size;
	uint32_t ap_params_start;
};

/* scp device attribute */
extern struct device_attribute dev_attr_scp_A_mobile_log_UT;
extern struct device_attribute dev_attr_scp_A_logger_wakeup_AP;
extern const struct file_operations scp_A_log_file_ops;

extern struct scp_regs scpreg;
extern struct device_attribute dev_attr_scp_mobile_log;
extern struct device_attribute dev_attr_scp_A_get_last_log;
extern struct device_attribute dev_attr_scp_A_status;
extern struct bin_attribute bin_attr_scp_dump;

/* scp loggger */
extern int scp_logger_init(phys_addr_t start, phys_addr_t limit);
extern void scp_logger_uninit(void);

extern void scp_logger_stop(void);
extern void scp_logger_cleanup(void);

/* scp exception */
int scp_excep_init(void);
void scp_ram_dump_init(void);
void scp_excep_cleanup(void);
void scp_reset_wait_timeout(void);

/* scp irq */
extern irqreturn_t scp_A_irq_handler(int irq, void *dev_id);
extern void scp_A_irq_init(void);

/* scp helper */
extern void scp_A_register_notify(struct notifier_block *nb);
extern void scp_A_unregister_notify(struct notifier_block *nb);
extern void scp_schedule_work(struct scp_work_struct *scp_ws);
extern void scp_schedule_logger_work(struct scp_work_struct *scp_ws);
extern int get_scp_semaphore(int flag);
extern int release_scp_semaphore(int flag);
extern int scp_get_semaphore_3way(int flag);
extern int scp_release_semaphore_3way(int flag);

extern void memcpy_to_scp(void __iomem *trg,
		const void *src, int size);
extern void memcpy_from_scp(void *trg, const void __iomem *src,
		int size);
extern int reset_scp(int reset);

extern phys_addr_t scp_get_reserve_mem_phys(enum scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_virt(enum scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_size(enum scp_reserve_mem_id_t id);
extern int scp_check_resource(void);
void set_scp_mpu(void);
extern phys_addr_t scp_mem_base_phys;
extern phys_addr_t scp_mem_base_virt;
extern phys_addr_t scp_mem_size;
extern atomic_t scp_reset_status;

extern void mbox_setup_pin_table(int mbox);
extern void mt_print_scp_ipi_id(unsigned int irq_no);
#ifdef CONFIG_MTK_GIC_V3_EXT
extern u32 mt_irq_get_pending(unsigned int irq);
#endif

/*extern scp notify*/
extern void scp_send_reset_wq(enum SCP_RESET_TYPE type);
extern void scp_extern_notify(enum SCP_NOTIFY_EVENT notify_status);

extern void scp_status_set(unsigned int value);
extern void scp_logger_init_set(unsigned int value);
extern unsigned int scp_set_reset_status(void);
extern void scp_enable_sram(void);
extern int scp_sys_full_reset(void);
extern void scp_reset_awake_counts(void);
extern void scp_awake_init(void);

#if SCP_RECOVERY_SUPPORT
extern unsigned int scp_reset_by_cmd;
extern struct scp_region_info_st scp_region_info_copy;
extern struct scp_region_info_st *scp_region_info;
extern void __iomem *scp_ap_dram_virt;
extern void __iomem *scp_loader_virt;
extern void __iomem *scp_regdump_virt;
#endif

__attribute__((weak))
int sensor_params_to_scp(phys_addr_t addr_vir, size_t size);

#endif
