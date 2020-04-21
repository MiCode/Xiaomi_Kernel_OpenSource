/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.*/

#ifndef _MHI_QCOM_
#define _MHI_QCOM_

/* iova cfg bitmask */
#define MHI_SMMU_ATTACH BIT(0)
#define MHI_SMMU_S1_BYPASS BIT(1)
#define MHI_SMMU_FAST BIT(2)
#define MHI_SMMU_ATOMIC BIT(3)
#define MHI_SMMU_FORCE_COHERENT BIT(4)

#define MHI_PCIE_VENDOR_ID (0x17cb)
#define MHI_PCIE_DEBUG_ID (0xffff)

#define MHI_BHI_SERIAL_NUM_OFFS (0x40)
#define MHI_BHI_OEMPKHASH(n) (0x64 + (0x4 * (n)))
#define MHI_BHI_OEMPKHASH_SEG (16)

/* runtime suspend timer */
#define MHI_RPM_SUSPEND_TMR_MS (250)
#define MHI_PCI_BAR_NUM (0)

/* timesync time calculations */
#define REMOTE_TICKS_TO_US(x) (div_u64((x) * 100ULL, \
			       div_u64(mhi_cntrl->remote_timer_freq, 10000ULL)))
#define REMOTE_TICKS_TO_SEC(x) (div_u64((x), \
				mhi_cntrl->remote_timer_freq))
#define REMOTE_TIME_REMAINDER_US(x) (REMOTE_TICKS_TO_US((x)) % \
					(REMOTE_TICKS_TO_SEC((x)) * 1000000ULL))

#define MHI_MAX_SFR_LEN (256)

extern const char * const mhi_ee_str[MHI_EE_MAX];
#define TO_MHI_EXEC_STR(ee) (ee >= MHI_EE_MAX ? "INVALID_EE" : mhi_ee_str[ee])

enum mhi_debug_mode {
	MHI_DEBUG_MODE_OFF,
	MHI_DEBUG_NO_D3, /* use debug.mbn as fw image and skip first M3/D3 */
	MHI_DEBUG_D3, /* use debug.mbn as fw image and allow first M3/D3 */
	MHI_FWIMAGE_NO_D3, /* use fw image if found and skip first M3/D3 */
	MHI_FWIMAGE_D3, /* use fw image if found and allow first M3/D3 */
	MHI_DEBUG_MODE_MAX = MHI_FWIMAGE_D3,
};

enum mhi_suspend_mode {
	MHI_ACTIVE_STATE,
	MHI_DEFAULT_SUSPEND,
	MHI_FAST_LINK_OFF,
	MHI_FAST_LINK_ON,
	MHI_SUSPEND_MODE_MAX,
};

extern const char * const mhi_suspend_mode_str[MHI_SUSPEND_MODE_MAX];
#define TO_MHI_SUSPEND_MODE_STR(mode) \
	(mode >= MHI_SUSPEND_MODE_MAX ? "Invalid" : mhi_suspend_mode_str[mode])

struct mhi_dev {
	struct pci_dev *pci_dev;
	bool drv_supported;
	int resn;
	void *arch_info;
	bool powered_on;
	dma_addr_t iova_start;
	dma_addr_t iova_stop;
	enum mhi_suspend_mode suspend_mode;

	/* hardware info */
	u32 serial_num;
	u32 oem_pk_hash[MHI_BHI_OEMPKHASH_SEG];
};

void mhi_deinit_pci_dev(struct mhi_controller *mhi_cntrl);
int mhi_pci_probe(struct pci_dev *pci_dev,
		  const struct pci_device_id *device_id);
void mhi_reg_write_work(struct work_struct *w);

#ifdef CONFIG_ARCH_QCOM

int mhi_arch_link_lpm_disable(struct mhi_controller *mhi_cntrl);
int mhi_arch_link_lpm_enable(struct mhi_controller *mhi_cntrl);
void mhi_arch_mission_mode_enter(struct mhi_controller *mhi_cntrl);
int mhi_arch_pcie_init(struct mhi_controller *mhi_cntrl);
void mhi_arch_pcie_deinit(struct mhi_controller *mhi_cntrl);
int mhi_arch_link_suspend(struct mhi_controller *mhi_cntrl);
int mhi_arch_link_resume(struct mhi_controller *mhi_cntrl);

#else

static inline int mhi_arch_pcie_init(struct mhi_controller *mhi_cntrl)
{
	return 0;
}

static inline void mhi_arch_pcie_deinit(struct mhi_controller *mhi_cntrl)
{
}

static inline int mhi_arch_link_suspend(struct mhi_controller *mhi_cntrl)
{
	return 0;
}

static inline int mhi_arch_link_resume(struct mhi_controller *mhi_cntrl)
{
	return 0;
}

static inline void mhi_arch_mission_mode_enter(struct mhi_controller *mhi_cntrl)
{
}

static inline int mhi_arch_link_lpm_disable(struct mhi_controller *mhi_cntrl)
{
	return 0;
}

static inline int mhi_arch_link_lpm_enable(struct mhi_controller *mhi_cntrl)
{
	return 0;
}

#endif

#endif /* _MHI_QCOM_ */
