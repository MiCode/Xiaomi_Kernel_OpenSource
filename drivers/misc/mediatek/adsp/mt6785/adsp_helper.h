/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __ADSP_HELPER_H__
#define __ADSP_HELPER_H__

#include <linux/notifier.h>
#include <linux/interrupt.h>
#include "adsp_reg.h"
#include "adsp_feature_define.h"
#include "adsp_reserved_mem.h"
#include "adsp_ipi.h"

#define ADSP_SLEEP_ENABLE               (1)

/* adsp config reg. definition*/
#define ADSP_A_TCM_SIZE         (adspreg.total_tcmsize)
#define ADSP_A_ITCM_SIZE        (adspreg.i_tcmsize)
#define ADSP_A_DTCM_SIZE        (adspreg.d_tcmsize)
#define ADSP_A_ITCM             (adspreg.iram)
#define ADSP_A_DTCM             (adspreg.dram)
#define ADSP_A_SYS_DRAM         (adspreg.sysram)

#define ADSP_A_CFG             (adspreg.cfg)
#define ADSP_A_CFG_SIZE        (adspreg.cfgregsize)

#define ADSP_A_DTCM_SHARE_BASE  (adspreg.dram + adspreg.d_tcmsize)
#define ADSP_A_DTCM_SHARE_SIZE   (0x1000)
#define ADSP_A_MPUINFO_BUFFER    (ADSP_A_DTCM_SHARE_BASE - 0x0020)
#define ADSP_A_OSTIMER_BUFFER    (ADSP_A_DTCM_SHARE_BASE - 0x0040)
#define ADSP_A_IPC_BUFFER        (ADSP_A_DTCM_SHARE_BASE - 0x0280)
#define ADSP_A_AUDIO_IPI_BUFFER  (ADSP_A_DTCM_SHARE_BASE - 0x0680)
#define ADSP_A_WAKELOCK_BUFFER   (ADSP_A_DTCM_SHARE_BASE - 0x0684)
#define ADSP_A_SYS_STATUS        (ADSP_A_DTCM_SHARE_BASE - 0x0688)
#define ADSP_BUS_MON_BACKUP_BASE (ADSP_A_DTCM_SHARE_BASE - 0x0744)
#define ADSP_INFRA_BUS_DUMP_BASE (ADSP_A_DTCM_SHARE_BASE - 0x07E4)
#define ADSP_A_AP_AWAKE          (ADSP_A_WAKELOCK_BUFFER)

/* Non-Cacheable MPU entry start from this ID. */
#define ADSP_MPU_NONCACHE_ID ADSP_A_DEBUG_DUMP_MEM_ID
#define ADSP_MPU_DATA_RO_ID

#define WDT_FIRST_WAIT_COUNT     (2)
#define WDT_LAST_WAIT_COUNT      (6)

struct adsp_mpu_info_t {
	u32 prog_addr;
	u32 prog_size;
	u32 data_addr;
	u32 data_size;
	u32 data_non_cache_addr;
	u32 data_non_cache_size;
};

/* adsp notify event */
enum ADSP_NOTIFY_EVENT {
	ADSP_EVENT_STOP = 0,
	ADSP_EVENT_READY,
};

/* adsp reset status */
enum ADSP_RESET_STATUS {
	ADSP_RESET_STATUS_STOP = 0,
	ADSP_RESET_STATUS_START = 1,
};

enum ADSP_RECOVERY_FLAG {
	ADSP_RECOVERY_START,
	ADSP_RECOVERY_OK,
};

/* adsp reset status */
enum ADSP_RESET_TYPE {
	ADSP_RESET_TYPE_WDT = 0,
	ADSP_RESET_TYPE_AWAKE = 1,
};

/* adsp semaphore definition*/
enum ADSP_SEMAPHORE_FLAG {
	ADSP_SEMAPHORE_CLK_CFG_5 = 0,
	ADSP_SEMAPHORE_PTP,
	ADSP_SEMAPHORE_I2C0,
	ADSP_SEMAPHORE_I2C1,
	ADSP_SEMAPHORE_TOUCH,
	ADSP_SEMAPHORE_APDMA,
	ADSP_SEMAPHORE_SENSOR,
	ADSP_SEMAPHORE_ADSP_A_AWAKE,
	ADSP_SEMAPHORE_ADSP_B_AWAKE,
	ADSP_NR_FLAG = 9,
};

struct adsp_regs {
	void __iomem *adspsys;
	void __iomem *iram;
	void __iomem *dram;
	void __iomem *sysram;
	void __iomem *cfg;
	void __iomem *clkctrl;
	void __iomem *infracfg_ao;
	void __iomem *pericfg;
	phys_addr_t sharedram;
	int wdt_irq;
	int ipc_irq;
	size_t i_tcmsize;
	size_t d_tcmsize;
	size_t cfgregsize;
	size_t total_tcmsize;
	size_t sysram_size;
	size_t shared_size;
};

/* adsp work struct definition*/
struct adsp_work_t {
	struct work_struct work;
	unsigned int flags;
	unsigned int id;
};

/* adsp device attribute group*/
extern struct attribute_group adsp_awake_attr_group;
extern struct attribute_group adsp_dvfs_attr_group;
extern struct attribute_group adsp_logger_attr_group;
extern struct attribute_group adsp_excep_attr_group;

extern struct adsp_regs adspreg;

#ifdef CFG_RECOVERY_SUPPORT
extern atomic_t adsp_reset_status;
extern unsigned int adsp_recovery_flag[ADSP_CORE_TOTAL];
extern struct completion adsp_sys_reset_cp;
extern unsigned int wdt_counter;
extern unsigned char *adsp_A_dram_dump_buffer;
#endif

/* adsp exception */
extern int adsp_excep_init(void);
extern void adsp_excep_cleanup(void);

/* adsp irq */
extern irqreturn_t adsp_A_irq_handler(int irq, void *dev_id);
extern irqreturn_t adsp_A_wdt_handler(int irq, void *dev_id);

extern void adsp_A_irq_init(void);
extern void adsp_A_ipi_init(void);

/* adsp helper */
extern void adsp_A_register_notify(struct notifier_block *nb);
extern void adsp_A_unregister_notify(struct notifier_block *nb);
extern struct workqueue_struct *adsp_workqueue;

extern void adsp_enable_dsp_clk(bool enable);
void set_adsp_mpu(phys_addr_t phys_addr, size_t size);
int adsp_dts_mapping(void);
void adsp_bus_sleep_protect(uint32_t enable);
void adsp_way_en_ctrl(uint32_t enable);

extern void adsp_send_reset_wq(enum ADSP_RESET_TYPE type,
			       enum adsp_core_id core_id);
extern void adsp_extern_notify(enum ADSP_NOTIFY_EVENT notify_status);

extern unsigned int adsp_set_reset_status(void);
void adsp_reset_ready(enum adsp_core_id id);
void adsp_A_ready_ipi_handler(int id, void *data, unsigned int len);

uint32_t adsp_power_on(uint32_t enable);
#endif
