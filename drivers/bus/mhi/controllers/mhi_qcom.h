/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2018, The Linux Foundation. All rights reserved.*/

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
#define MHI_RPM_SUSPEND_TMR_MS (1000)
#define MHI_PCI_BAR_NUM (0)

extern const char * const mhi_ee_str[MHI_EE_MAX];
#define TO_MHI_EXEC_STR(ee) (ee >= MHI_EE_MAX ? "INVALID_EE" : mhi_ee_str[ee])

struct mhi_dev {
	struct pci_dev *pci_dev;
	u32 smmu_cfg;
	int resn;
	void *arch_info;
	bool powered_on;
	dma_addr_t iova_start;
	dma_addr_t iova_stop;
};

void mhi_deinit_pci_dev(struct mhi_controller *mhi_cntrl);
int mhi_pci_probe(struct pci_dev *pci_dev,
		  const struct pci_device_id *device_id);

#ifdef CONFIG_ARCH_QCOM

int mhi_arch_pcie_init(struct mhi_controller *mhi_cntrl);
void mhi_arch_pcie_deinit(struct mhi_controller *mhi_cntrl);
int mhi_arch_iommu_init(struct mhi_controller *mhi_cntrl);
void mhi_arch_iommu_deinit(struct mhi_controller *mhi_cntrl);
int mhi_arch_link_off(struct mhi_controller *mhi_cntrl, bool graceful);
int mhi_arch_link_on(struct mhi_controller *mhi_cntrl);

#else

static inline int mhi_arch_iommu_init(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);

	mhi_cntrl->dev = &mhi_dev->pci_dev->dev;

	return dma_set_mask_and_coherent(mhi_cntrl->dev, DMA_BIT_MASK(64));
}

static inline void mhi_arch_iommu_deinit(struct mhi_controller *mhi_cntrl)
{
}

static inline int mhi_arch_pcie_init(struct mhi_controller *mhi_cntrl)
{
	return 0;
}

static inline void mhi_arch_pcie_deinit(struct mhi_controller *mhi_cntrl)
{
}

static inline int mhi_arch_link_off(struct mhi_controller *mhi_cntrl,
				    bool graceful)
{
	return 0;
}

static inline int mhi_arch_link_on(struct mhi_controller *mhi_cntrl)
{
	return 0;
}

#endif

#endif /* _MHI_QCOM_ */
