#ifndef _XHCI_MTK_POWER_H
#define _XHCI_MTK_POWER_H

#include <linux/usb.h>

void enableXhciAllPortPower(struct xhci_hcd *xhci);
void disableXhciAllPortPower(struct xhci_hcd *xhci);
void enableAllClockPower(struct xhci_hcd *xhci, bool is_reset);
void disableAllClockPower(struct xhci_hcd *xhci);
#if 0
void disablePortClockPower(int port_index, int port_rev);
void enablePortClockPower(int port_index, int port_rev);
#endif

#ifdef CONFIG_USB_MTK_DUALMODE
void mtk_switch2host(void);
void mtk_switch2device(bool skip);
#endif

#endif
