/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MSM_IOMMU_H
#define MSM_IOMMU_H

#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/regulator/consumer.h>
#include <soc/qcom/socinfo.h>

extern pgprot_t     pgprot_kernel;
extern struct bus_type msm_iommu_sec_bus_type;
extern struct iommu_access_ops iommu_access_ops_v0;
extern struct iommu_access_ops iommu_access_ops_v1;

/* Domain attributes */
#define MSM_IOMMU_DOMAIN_PT_CACHEABLE	0x1
#define MSM_IOMMU_DOMAIN_PT_SECURE	0x2

/* Mask for the cache policy attribute */
#define MSM_IOMMU_CP_MASK		0x03

/* Maximum number of Machine IDs that we are allowing to be mapped to the same
 * context bank. The number of MIDs mapped to the same CB does not affect
 * performance, but there is a practical limit on how many distinct MIDs may
 * be present. These mappings are typically determined at design time and are
 * not expected to change at run time.
 */
#define MAX_NUM_MIDS	32

/* Maximum number of SMT entries allowed by the system */
#define MAX_NUM_SMR	128

#define MAX_NUM_BFB_REGS	32

/**
 * struct msm_iommu_dev - a single IOMMU hardware instance
 * name		Human-readable name given to this IOMMU HW instance
 * ncb		Number of context banks present on this IOMMU HW instance
 */
struct msm_iommu_dev {
	const char *name;
	int ncb;
	int ttbr_split;
};

/**
 * struct msm_iommu_ctx_dev - an IOMMU context bank instance
 * name		Human-readable name given to this context bank
 * num		Index of this context bank within the hardware
 * mids		List of Machine IDs that are to be mapped into this context
 *		bank, terminated by -1. The MID is a set of signals on the
 *		AXI bus that identifies the function associated with a specific
 *		memory request. (See ARM spec).
 */
struct msm_iommu_ctx_dev {
	const char *name;
	int num;
	int mids[MAX_NUM_MIDS];
};

/**
 * struct msm_iommu_bfb_settings - a set of IOMMU BFB tuning parameters
 * regs		An array of register offsets to configure
 * data		Values to write to corresponding registers
 * length	Number of valid entries in the offset/val arrays
 */
struct msm_iommu_bfb_settings {
	unsigned int regs[MAX_NUM_BFB_REGS];
	unsigned int data[MAX_NUM_BFB_REGS];
	int length;
};

/**
 * struct msm_iommu_drvdata - A single IOMMU hardware instance
 * @base:	IOMMU config port base address (VA)
 * @glb_base:	IOMMU config port base address for global register space (VA)
 * @phys_base:  IOMMU physical base address.
 * @ncb		The number of contexts on this IOMMU
 * @irq:	Interrupt number
 * @clk:	The bus clock for this IOMMU hardware instance
 * @pclk:	The clock for the IOMMU bus interconnect
 * @aclk:	Alternate core clock for this IOMMU core, if any
 * @aiclk:	Alternate interface clock for this IOMMU core, if any
 * @name:	Human-readable name of this IOMMU device
 * @gdsc:	Regulator needed to power this HW block (v2 only)
 * @bfb_settings: Optional BFB performance tuning parameters
 * @dev:	Struct device this hardware instance is tied to
 * @list:	List head to link all iommus together
 * @clk_reg_virt: Optional clock register virtual address.
 * @halt_enabled: Set to 1 if IOMMU halt is supported in the IOMMU, 0 otherwise.
 * @ctx_attach_count: Count of how many context are attached.
 * @bus_client  : Bus client needed to vote for bus bandwidth.
 * @needs_rem_spinlock  : 1 if remote spinlock is needed, 0 otherwise
 * @powered_on: Powered status of the IOMMU. 0 means powered off.
 *
 * A msm_iommu_drvdata holds the global driver data about a single piece
 * of an IOMMU hardware instance.
 */
struct msm_iommu_drvdata {
	void __iomem *base;
	phys_addr_t phys_base;
	void __iomem *glb_base;
	void __iomem *cb_base;
	void __iomem *smmu_local_base;
	void __iomem *vbif_base;
	int ncb;
	int ttbr_split;
	struct clk *clk;
	struct clk *pclk;
	struct clk *aclk;
	struct clk *aiclk;
	const char *name;
	struct regulator *gdsc;
	struct regulator *alt_gdsc;
	struct msm_iommu_bfb_settings *bfb_settings;
	int sec_id;
	struct device *dev;
	struct list_head list;
	void __iomem *clk_reg_virt;
	int halt_enabled;
	unsigned int ctx_attach_count;
	unsigned int bus_client;
	int needs_rem_spinlock;
	int powered_on;
	unsigned int model;
};

/**
 * struct iommu_access_ops - Callbacks for accessing IOMMU
 * @iommu_power_on:     Turn on power to unit
 * @iommu_power_off:    Turn off power to unit
 * @iommu_bus_vote:     Vote for bus bandwidth
 * @iommu_clk_on:       Turn on clks to unit
 * @iommu_clk_off:      Turn off clks to unit
 * @iommu_lock_initialize: Initialize the remote lock
 * @iommu_lock_acquire: Acquire any locks needed
 * @iommu_lock_release: Release locks needed
 */
struct iommu_access_ops {
	int (*iommu_power_on)(struct msm_iommu_drvdata *);
	void (*iommu_power_off)(struct msm_iommu_drvdata *);
	int (*iommu_bus_vote)(struct msm_iommu_drvdata *drvdata,
			      unsigned int vote);
	int (*iommu_clk_on)(struct msm_iommu_drvdata *);
	void (*iommu_clk_off)(struct msm_iommu_drvdata *);
	void * (*iommu_lock_initialize)(void);
	void (*iommu_lock_acquire)(unsigned int need_extra_lock);
	void (*iommu_lock_release)(unsigned int need_extra_lock);
};

void msm_iommu_add_drv(struct msm_iommu_drvdata *drv);
void msm_iommu_remove_drv(struct msm_iommu_drvdata *drv);
void program_iommu_bfb_settings(void __iomem *base,
			const struct msm_iommu_bfb_settings *bfb_settings);
void iommu_halt(const struct msm_iommu_drvdata *iommu_drvdata);
void iommu_resume(const struct msm_iommu_drvdata *iommu_drvdata);

/**
 * struct msm_iommu_ctx_drvdata - an IOMMU context bank instance
 * @num:		Hardware context number of this context
 * @pdev:		Platform device associated wit this HW instance
 * @attached_elm:	List element for domains to track which devices are
 *			attached to them
 * @attached_domain	Domain currently attached to this context (if any)
 * @name		Human-readable name of this context device
 * @sids		List of Stream IDs mapped to this context
 * @nsid		Number of Stream IDs mapped to this context
 * @secure_context	true if this is a secure context programmed by
			the secure environment, false otherwise
 * @asid		ASID used with this context.
 * @attach_count	Number of time this context has been attached.
 * @report_error_on_fault - true if error is returned back to master
 *
 * A msm_iommu_ctx_drvdata holds the driver data for a single context bank
 * within each IOMMU hardware instance
 */
struct msm_iommu_ctx_drvdata {
	int num;
	struct platform_device *pdev;
	struct list_head attached_elm;
	struct iommu_domain *attached_domain;
	const char *name;
	u32 sids[MAX_NUM_SMR];
	unsigned int nsid;
	unsigned int secure_context;
	int asid;
	int attach_count;
	u32 sid_mask[MAX_NUM_SMR];
	unsigned int n_sid_mask;
	bool report_error_on_fault;
};

enum dump_reg {
	DUMP_REG_FIRST,
	DUMP_REG_FAR0 = DUMP_REG_FIRST,
	DUMP_REG_FAR1,
	DUMP_REG_PAR0,
	DUMP_REG_PAR1,
	DUMP_REG_FSR,
	DUMP_REG_FSYNR0,
	DUMP_REG_FSYNR1,
	DUMP_REG_TTBR0_0,
	DUMP_REG_TTBR0_1,
	DUMP_REG_TTBR1_0,
	DUMP_REG_TTBR1_1,
	DUMP_REG_SCTLR,
	DUMP_REG_ACTLR,
	DUMP_REG_PRRR,
	DUMP_REG_MAIR0 = DUMP_REG_PRRR,
	DUMP_REG_NMRR,
	DUMP_REG_MAIR1 = DUMP_REG_NMRR,
	DUMP_REG_CBAR_N,
	DUMP_REG_CBFRSYNRA_N,
	MAX_DUMP_REGS,
};

enum dump_reg_type {
	DRT_CTX_REG,
	DRT_GLOBAL_REG,
	DRT_GLOBAL_REG_N,
};

enum model_id {
	QSMMUv1 = 1,
	QSMMUv2,
	MMU_500 = 500,
	MAX_MODEL,
};

struct dump_regs_tbl_entry {
	/*
	 * To keep things context-bank-agnostic, we only store the
	 * register offset in `reg_offset'
	 */
	unsigned int reg_offset;
	const char *name;
	int must_be_present;
	enum dump_reg_type dump_reg_type;
};
extern struct dump_regs_tbl_entry dump_regs_tbl[MAX_DUMP_REGS];

#define COMBINE_DUMP_REG(upper, lower) (((u64) upper << 32) | lower)

struct msm_iommu_context_reg {
	uint32_t val;
	bool valid;
};

void print_ctx_regs(struct msm_iommu_context_reg regs[]);

/*
 * Interrupt handler for the IOMMU context fault interrupt. Hooking the
 * interrupt is not supported in the API yet, but this will print an error
 * message and dump useful IOMMU registers.
 */
irqreturn_t msm_iommu_global_fault_handler(int irq, void *dev_id);
irqreturn_t msm_iommu_fault_handler(int irq, void *dev_id);
irqreturn_t msm_iommu_fault_handler_v2(int irq, void *dev_id);
irqreturn_t msm_iommu_secure_fault_handler_v2(int irq, void *dev_id);

enum {
	PROC_APPS,
	PROC_GPU,
	PROC_MAX
};

/* Expose structure to allow kgsl iommu driver to use the same structure to
 * communicate to GPU the addresses of the flag and turn variables.
 */
struct remote_iommu_petersons_spinlock {
	uint32_t flag[PROC_MAX];
	uint32_t turn;
};

#ifdef CONFIG_MSM_IOMMU
void *msm_iommu_lock_initialize(void);
void msm_iommu_mutex_lock(void);
void msm_iommu_mutex_unlock(void);
void msm_set_iommu_access_ops(struct iommu_access_ops *ops);
struct iommu_access_ops *msm_get_iommu_access_ops(void);
#else
static inline void *msm_iommu_lock_initialize(void)
{
	return NULL;
}
static inline void msm_iommu_mutex_lock(void) { }
static inline void msm_iommu_mutex_unlock(void) { }
static inline void msm_set_iommu_access_ops(struct iommu_access_ops *ops)
{

}
static inline struct iommu_access_ops *msm_get_iommu_access_ops(void)
{
	return NULL;
}
#endif

#ifdef CONFIG_MSM_IOMMU_SYNC
void msm_iommu_remote_p0_spin_lock(unsigned int need_lock);
void msm_iommu_remote_p0_spin_unlock(unsigned int need_lock);

#define msm_iommu_remote_lock_init() _msm_iommu_remote_spin_lock_init()
#define msm_iommu_remote_spin_lock(need_lock) \
				msm_iommu_remote_p0_spin_lock(need_lock)
#define msm_iommu_remote_spin_unlock(need_lock) \
				msm_iommu_remote_p0_spin_unlock(need_lock)
#else
#define msm_iommu_remote_lock_init()
#define msm_iommu_remote_spin_lock(need_lock)
#define msm_iommu_remote_spin_unlock(need_lock)
#endif

#ifdef CONFIG_MSM_IOMMU
/*
 * Look up an IOMMU context device by its context name. NULL if none found.
 * Useful for testing and drivers that do not yet fully have IOMMU stuff in
 * their platform devices.
 */
struct device *msm_iommu_get_ctx(const char *ctx_name);
#else
static inline struct device *msm_iommu_get_ctx(const char *ctx_name)
{
	return NULL;
}
#endif

/*
 * Function to program the global registers of an IOMMU securely.
 * This should only be called on IOMMUs for which kernel programming
 * of global registers is not possible
 */
void msm_iommu_sec_set_access_ops(struct iommu_access_ops *access_ops);
int msm_iommu_sec_program_iommu(struct msm_iommu_drvdata *drvdata,
				struct msm_iommu_ctx_drvdata *ctx_drvdata);
int is_vfe_secure(void);

#ifdef CONFIG_MSM_IOMMU_V0
static inline int msm_soc_version_supports_iommu_v0(void)
{
	static int soc_supports_v0 = -1;
#ifdef CONFIG_OF
	struct device_node *node;
#endif

	if (soc_supports_v0 != -1)
		return soc_supports_v0;

#ifdef CONFIG_OF
	node = of_find_compatible_node(NULL, NULL, "qcom,msm-smmu-v0");
	if (node) {
		soc_supports_v0 = 1;
		of_node_put(node);
		return 1;
	}
#endif
	if (cpu_is_msm8960() &&
	    SOCINFO_VERSION_MAJOR(socinfo_get_version()) < 2) {
		soc_supports_v0 = 0;
		return 0;
	}

	if (cpu_is_msm8x60() &&
	    (SOCINFO_VERSION_MAJOR(socinfo_get_version()) != 2 ||
	    SOCINFO_VERSION_MINOR(socinfo_get_version()) < 1))	{
		soc_supports_v0 = 0;
		return 0;
	}

	soc_supports_v0 = 1;
	return 1;
}
#else
static inline int msm_soc_version_supports_iommu_v0(void)
{
	return 0;
}
#endif

int msm_iommu_get_scm_call_avail(void);
void msm_iommu_check_scm_call_avail(void);

u32 msm_iommu_get_mair0(void);
u32 msm_iommu_get_mair1(void);
u32 msm_iommu_get_prrr(void);
u32 msm_iommu_get_nmrr(void);

/* events for notifiers passed to msm_iommu_register_notify */
#define TLB_SYNC_TIMEOUT 1

#ifdef CONFIG_MSM_IOMMU_V1
void msm_iommu_register_notify(struct notifier_block *nb);
#else
static inline void msm_iommu_register_notify(struct notifier_block *nb)
{
}
#endif

#endif
