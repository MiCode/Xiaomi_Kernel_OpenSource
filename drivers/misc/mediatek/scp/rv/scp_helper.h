/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __SCP_HELPER_H__
#define __SCP_HELPER_H__

#include <linux/arm-smccc.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include "scp_reg.h"
#include "scp_feature_define.h"
#include "scp.h"

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

#define mt_reg_sync_writel(v, a) \
	do {    \
		__raw_writel((v), (void __force __iomem *)((a)));   \
		mb();  /*make sure register access in order */ \
	} while (0)

/* This structre need to sync with SCP-side */
struct SCP_IRQ_AST_INFO {
	unsigned int scp_irq_ast_time;
	unsigned int scp_irq_ast_pc_s;
	unsigned int scp_irq_ast_pc_e;
	unsigned int scp_irq_ast_lr_s;
	unsigned int scp_irq_ast_lr_e;
	unsigned int scp_irq_ast_irqd;
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

/* scp semaphore 3way definition */
enum SEMAPHORE_3WAY_FLAG {
	SEMA_SCP_3WAY_UART = 0,
	SEMA_SCP_3WAY_C2C_A = 1,
	SEMA_SCP_3WAY_C2C_B = 2,
	SEMA_SCP_3WAY_DVFS = 3,
	SEMA_SCP_3WAY_AUDIO = 4,
	SEMA_SCP_3WAY_AUDIOREG = 5,
	SEMA_SCP_3WAY_NUM = 6,
};

/* scp semaphore status */
enum  SEMAPHORE_STATUS {
	SEMAPHORE_NOT_INIT = -1,
	SEMAPHORE_FAIL = 0,
	SEMAPHORE_SUCCESS = 1,
};

/* scp reset status */
enum SCP_RESET_STATUS {
	RESET_STATUS_STOP = 0,
	RESET_STATUS_START = 1,
	/* this state mean scp already kick reboot, if wdt trigger before
	 * recovery finish mean recovery fail, should retry again
	 */
	RESET_STATUS_START_KICK = 2,
	RESET_STATUS_START_WDT = 3,
};

/* scp reset status */
enum SCP_RESET_TYPE {
	RESET_TYPE_WDT = 0,
	RESET_TYPE_AWAKE = 1,
	RESET_TYPE_CMD = 2,
	RESET_TYPE_TIMEOUT = 3,
};

struct scp_bus_tracker_status {
	u32 dbg_con;
	u32 dbg_r[32];
	u32 dbg_w[32];
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
	int irq0;
	int irq1;
	unsigned int total_tcmsize;
	unsigned int cfgregsize;
	unsigned int scp_tcmsize;
	unsigned int core_nums;
	unsigned int twohart;
	unsigned int secure_dump;
	struct scp_bus_tracker_status tracker_status;
};

/* scp work struct definition*/
struct scp_work_struct {
	struct work_struct work;
	unsigned int flags;
	unsigned int id;
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
extern struct device_attribute dev_attr_log_filter;
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
extern void scp_schedule_work(struct scp_work_struct *scp_ws);
extern void scp_schedule_logger_work(struct scp_work_struct *scp_ws);

extern void memcpy_to_scp(void __iomem *trg,
		const void *src, int size);
extern void memcpy_from_scp(void *trg, const void __iomem *src,
		int size);
extern int reset_scp(int reset);

extern int scp_check_resource(void);
void set_scp_mpu(void);
extern phys_addr_t scp_mem_base_phys;
extern phys_addr_t scp_mem_base_virt;
extern phys_addr_t scp_mem_size;
extern atomic_t scp_reset_status;

extern bool mbox_check_send_table(unsigned int id);
extern bool mbox_check_recv_table(unsigned int id);
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
extern struct tasklet_struct scp_A_irq0_tasklet;
extern struct tasklet_struct scp_A_irq1_tasklet;
#endif

enum MTK_TINYSYS_SCP_KERNEL_OP {
	MTK_TINYSYS_SCP_KERNEL_OP_DUMP_START = 0,
	MTK_TINYSYS_SCP_KERNEL_OP_DUMP_POLLING,
	MTK_TINYSYS_SCP_KERNEL_OP_RESET_SET,
	MTK_TINYSYS_SCP_KERNEL_OP_RESET_RELEASE,
	MTK_TINYSYS_SCP_KERNEL_OP_RESTORE_L2TCM,
	MTK_TINYSYS_SCP_KERNEL_OP_RESTORE_DRAM,
	MTK_TINYSYS_SCP_KERNEL_OP_WDT_SET,
	MTK_TINYSYS_SCP_KERNEL_OP_HALT_SET,
	MTK_TINYSYS_SCP_KERNEL_OP_WDT_CLEAR,
	MTK_TINYSYS_SCP_KERNEL_OP_NUM,
};

#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
static inline unsigned long scp_do_dump(void)
{
	return 0;
}

static inline unsigned long scp_do_polling(void)
{
	return 0;
}

static inline uint64_t scp_do_rstn_set(uint64_t boot_ok)
{
	return 0;
}

static inline uint64_t scp_do_rstn_clr(void)
{
	return 0;
}

static inline unsigned long scp_restore_l2tcm(void)
{
	return 0;
}

static inline unsigned long scp_restore_dram(void)
{
	return 0;
}

static inline uint64_t scp_do_wdt_set(uint64_t coreid)
{
	return 0;
}

static inline uint64_t scp_do_halt_set(void)
{
	return 0;
}

static inline uint64_t scp_do_wdt_clear(uint64_t coreid)
{
	return 0;
}

#endif

#endif
