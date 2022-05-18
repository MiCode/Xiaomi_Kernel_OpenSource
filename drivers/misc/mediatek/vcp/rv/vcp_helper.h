/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __VCP_HELPER_H__
#define __VCP_HELPER_H__

#include <linux/arm-smccc.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include "vcp_reg.h"
#include "vcp_feature_define.h"
#include "vcp.h"

#define ROUNDUP(a, b)		        (((a) + ((b)-1)) & ~((b)-1))
#define VCP_SYNC_TIMEOUT_MS		(100)

/* vcp config reg. definition*/
#define VCP_TCM_SIZE		(vcpreg.total_tcmsize)
#define VCP_A_TCM_SIZE		(vcpreg.vcp_tcmsize)
#define VCP_TCM			(vcpreg.sram)
#define VCP_REGION_INFO_OFFSET	0x4
#define VCP_RTOS_START		(0x800)

#define VCP_DRAM_MAPSIZE	(0x100000)

/* vcp dvfs return status flag */
#define SET_PLL_FAIL		(1)
#define SET_PMIC_VOLT_FAIL	(2)

#define mt_reg_sync_writel(v, a) \
	do {    \
		__raw_writel((v), (void __force __iomem *)((a)));   \
		mb();  /*make sure register access in order */ \
	} while (0)

/* This structre need to sync with VCP-side */
struct VCP_IRQ_AST_INFO {
	unsigned int vcp_irq_ast_time;
	unsigned int vcp_irq_ast_pc_s;
	unsigned int vcp_irq_ast_pc_e;
	unsigned int vcp_irq_ast_lr_s;
	unsigned int vcp_irq_ast_lr_e;
	unsigned int vcp_irq_ast_irqd;
};

/* reset ID */
#define VCP_ALL_ENABLE	0x00
#define VCP_ALL_REBOOT	0x01
#define VCP_ALL_SUSPEND	0x10

#define VCP_PACK_IOVA(addr)     ((uint32_t)((addr) | (((addr) >> 32) & 0xF)))
#define VCP_UNPACK_IOVA(addr)   \
	((uint64_t)(addr & 0xFFFFFFF0) | (((uint64_t)(addr) & 0xF) << 32))

/* vcp semaphore definition*/
enum SEMAPHORE_FLAG {
	SEMAPHORE_CLK_CFG_5 = 0,
	SEMAPHORE_PTP,
	SEMAPHORE_I2C0,
	SEMAPHORE_I2C1,
	SEMAPHORE_TOUCH,
	SEMAPHORE_APDMA,
	SEMAPHORE_SENSOR,
	SEMAPHORE_VCP_A_AWAKE,
	SEMAPHORE_VCP_B_AWAKE,
	NR_FLAG = 9,
};

/* vcp reset status */
enum VCP_RESET_STATUS {
	RESET_STATUS_STOP = 0,
	RESET_STATUS_START = 1,
};

/* vcp reset status */
enum VCP_RESET_TYPE {
	RESET_TYPE_WDT = 0,
	RESET_TYPE_AWAKE = 1,
	RESET_TYPE_CMD = 2,
	RESET_TYPE_TIMEOUT = 3,
};

/* vcp iommus */
enum VCP_IOMMU_DEV {
	VCP_IOMMU_256MB1 = 0,
	VCP_IOMMU_VDEC_512MB1 = 1,
	VCP_IOMMU_VENC_512MB2 = 2,
	VCP_IOMMU_WORK_256MB2 = 3,
	VCP_IOMMU_UBE_LAT = 4,
	VCP_IOMMU_UBE_CORE = 5,
	VCP_IOMMU_DEV_NUM,
};

enum mtk_tinysys_vcp_kernel_op {
	MTK_TINYSYS_VCP_KERNEL_OP_DUMP_START = 0,
	MTK_TINYSYS_VCP_KERNEL_OP_DUMP_POLLING,
	MTK_TINYSYS_VCP_KERNEL_OP_RESET_SET,
	MTK_TINYSYS_VCP_KERNEL_OP_RESET_RELEASE,
	MTK_TINYSYS_VCP_KERNEL_OP_NUM,
};

struct vcp_regs {
	void __iomem *sram;
	void __iomem *cfg;
	void __iomem *clkctrl;
	void __iomem *l1cctrl;
	void __iomem *cfg_core0;
	void __iomem *cfg_core1;
	void __iomem *cfg_sec;
	void __iomem *cfg_mmu;
	void __iomem *bus_tracker;
	int irq0;
	int irq1;
	unsigned int total_tcmsize;
	unsigned int cfgregsize;
	unsigned int vcp_tcmsize;
	unsigned int core_nums;
	unsigned int twohart;
	unsigned int femter_ck;
};

/* vcp work struct definition*/
struct vcp_work_struct {
	struct work_struct work;
	unsigned int flags;
	unsigned int id;
};

struct vcp_reserve_mblock {
	enum vcp_reserve_mem_id_t num;
	u64 start_phys;
	u64 start_virt;
	u64 size;
};

struct vcp_region_info_st {
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
	uint32_t vcp_log_thru_ap_uart;
	uint32_t TaskContext_ptr;
	uint32_t vcpctl;
	uint32_t regdump_start;
	uint32_t regdump_size;
	uint32_t ap_params_start;
};

extern unsigned int vcp_support;
extern bool driver_init_done;

/* vcp device attribute */
extern struct device_attribute dev_attr_vcp_A_mobile_log_UT;
extern struct device_attribute dev_attr_vcp_A_logger_wakeup_AP;
extern const struct file_operations vcp_A_log_file_ops;

extern struct vcp_regs vcpreg;
extern struct device_attribute dev_attr_vcp_mobile_log;
extern struct device_attribute dev_attr_vcp_A_get_last_log;
extern struct device_attribute dev_attr_vcp_A_status;
extern struct device_attribute dev_attr_log_filter;
extern struct bin_attribute bin_attr_vcp_dump;

/* vcp loggger */
extern int vcp_logger_init(phys_addr_t start, phys_addr_t limit);
extern void vcp_logger_uninit(void);

extern void vcp_logger_stop(void);
extern void vcp_logger_cleanup(void);
extern unsigned int vcp_dbg_log;

/* vcp exception */
int vcp_excep_init(void);
void vcp_ram_dump_init(void);
void vcp_excep_cleanup(void);
void vcp_wait_core_stop_timeout(int mmup_enable);

/* vcp irq */
extern irqreturn_t vcp_A_irq_handler(int irq, void *dev_id);
extern void vcp_A_irq_init(void);
extern void wait_vcp_wdt_irq_done(void);

/* vcp helper */
extern void vcp_schedule_work(struct vcp_work_struct *vcp_ws);
extern void vcp_schedule_logger_work(struct vcp_work_struct *vcp_ws);

extern void memcpy_to_vcp(void __iomem *trg,
		const void *src, int size);
extern void memcpy_from_vcp(void *trg, const void __iomem *src,
		int size);
extern int reset_vcp(int reset);
extern struct device *vcp_get_io_device(enum VCP_IOMMU_DEV io_num);

extern int vcp_check_resource(void);
void set_vcp_mpu(void);
void trigger_vcp_halt(enum vcp_core_id id);
extern phys_addr_t vcp_mem_base_phys;
extern phys_addr_t vcp_mem_base_virt;
extern phys_addr_t vcp_mem_size;
extern atomic_t vcp_reset_status;
extern spinlock_t vcp_awake_spinlock;

#if IS_ENABLED(CONFIG_MTK_GIC_V3_EXT)
extern u32 mt_irq_get_pending(unsigned int irq);
#endif

/*extern vcp notify*/
extern void vcp_send_reset_wq(enum VCP_RESET_TYPE type);
extern void vcp_extern_notify(enum VCP_NOTIFY_EVENT notify_status);

extern void vcp_status_set(unsigned int value);
extern void vcp_logger_init_set(unsigned int value);
extern unsigned int vcp_set_reset_status(void);
extern void vcp_enable_sram(void);
extern void vcp_reset_awake_counts(void);
extern void vcp_awake_init(void);
extern void vcp_enable_pm_clk(enum feature_id id);
extern void vcp_disable_pm_clk(enum feature_id id);

#if VCP_RECOVERY_SUPPORT
extern unsigned int vcp_reset_by_cmd;
extern struct vcp_region_info_st vcp_region_info_copy;
extern struct vcp_region_info_st *vcp_region_info;
extern void __iomem *vcp_ap_dram_virt;
extern void __iomem *vcp_loader_virt;
extern void __iomem *vcp_regdump_virt;
#endif

#endif
