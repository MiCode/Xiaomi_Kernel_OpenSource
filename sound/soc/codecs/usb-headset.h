#ifndef __INCLUDE_USB_HEADSET_H
#define __INCLUDE_USB_HEADSET_H

#include <linux/platform_device.h>

int usbhs_init(struct platform_device *pdev);
void usbhs_deinit(void);

#endif /* __INCLUDE_USB_HEADSET_H */
