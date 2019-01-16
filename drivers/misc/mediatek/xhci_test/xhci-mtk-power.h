#ifndef _XHCI_MTK_POWER_H
#define _XHCI_MTK_POWER_H

#include <linux/usb.h>
#include "xhci.h"
#include "mtk-test-lib.h"

static int g_num_u3_port;
static int g_num_u2_port;

void mtktest_enableXhciAllPortPower(struct xhci_hcd *xhci);
void mtktest_disableXhciAllPortPower(struct xhci_hcd *xhci);
void mtktest_enableAllClockPower();
void mtktest_disablePortClockPower();
void mtktest_enablePortClockPower(int port_index, int port_rev);
void mtktest_disableAllClockPower();
void mtktest_resetIP();

#endif
