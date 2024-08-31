#ifndef PCIE_DEBUGFS_H__
#define PCIE_DEBUGFS_H__

#include <ispv4_pcie.h>
#include "ispv4_pcie_hdma.h"
#include "ispv4_pcie_pm.h"

#define ISPV4_DEBUGFS

enum ispv4_pm_debugfs_option {
	ISPV4_PCIE_DISABLE,
	ISPV4_PCIE_ENABLE,
	ISPV4_PCIE_FORCE_DISABLE,
	ISPV4_PCIE_MAX_OPTION,
};

static const char * const
	ispv4_pm_debugfs_option_desc[ISPV4_PCIE_MAX_OPTION] = {
	"Disable PCIe link",
	"Enable PCIe link",
};

enum ispv4_hdma_debugfs_option {
	HDMA_SINGLE_READ,
	HDMA_SINGLE_WRITE,
	HDMA_LL_READ,
	HDMA_LL_WRITE,
	HDMA_MAX_DEBUGFS_OPTION,
};

static const char * const
	ispv4_hdma_debugfs_option_desc[HDMA_MAX_DEBUGFS_OPTION] = {
	"HDMA SINGLE READ",
	"HDMA SINGLE WRITE",
	"HDMA LINK LIST READ",
	"HDMA LINK LIST WRITE",
};

void ispv4_debugfs_add_pcie_bar(struct ispv4_data *priv);
void ispv4_debugfs_add_pcie_reset(struct ispv4_data *priv);
void ispv4_debugfs_add_pcie_pm(struct ispv4_data *priv);
void ispv4_debugfs_add_pcie_reg(struct ispv4_data *priv);
void ispv4_debugfs_add_pcie_hdma(struct pcie_hdma *hdma);
int ispv4_debugfs_add_pcie_dump_iatu_hdma(struct ispv4_data *priv);
void ispv4_debugfs_add_spi(void);
void ispv4_debugfs_add_mailbox(void);
void ispv4_debugfs_add_pcie_linkctrl(struct ispv4_data *priv);
void ispv4_debugfs_add_pcie_bandwidth(struct ispv4_data *priv);
void ispv4_debugfs_add_pcie(void);

void ispv4_debugfs_init(void);
void ispv4_debugfs_exit(void);

// TODO remove, From ispv4_dev.c
int ispv4_pci_link_resume(struct pci_dev *pdev);
int ispv4_pci_link_suspend(struct pci_dev *pdev);

#endif  /* PCIE_DEBUGFS_H__ */
