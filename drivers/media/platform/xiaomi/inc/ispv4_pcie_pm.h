#ifndef PCIE_RESET_H__
#define PCIE_RESET_H__

#include <ispv4_pcie.h>
#include "media/ispv4_defs.h"

#define LINK_TRAINING_RETRY_MAX_TIMES	3

int ispv4_resume_pci_link(struct ispv4_data *data);
int ispv4_suspend_pci_link(struct ispv4_data *data);
int ispv4_suspend_pci_force(struct ispv4_data *data);
int ispv4_pci_linksta_ctl(struct pci_dev *pdev, enum ispv4_link_sta st);
int ispv4_set_linkspeed(struct pci_dev *pdev, int link_speed);
uint16_t ispv4_get_linkspeed(struct pci_dev *pdev);

#endif  /* PCIE_RESET_H__ */
