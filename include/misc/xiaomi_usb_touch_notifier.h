#ifndef _XIAOMI_USB_TOUCH_NOTIFIER_H_
#define _XIAOMI_USB_TOUCH_NOTIFIER_H_

/*
 *	This include file is intended for xiaomi touch that to notify and receive
 *	touch event.
 *
 */
#include <linux/notifier.h>

#define		XIAOMI_TOUCH_USB_SWITCH   0x01


enum {
	/* usb  switch off */
	XIAOMI_USB_DISABLE,
	/* usb  switch on */
	XIAOMI_USB_ENABLE
};

struct xiaomi_usb_notify_data {
	int usb_touch_enable;
};

extern int xiaomi_usb_touch_notifier_register_client(struct notifier_block *nb);
extern int xiaomi_usb_touch_notifier_unregister_client(struct notifier_block *nb);
extern int xiaomi_usb_touch_notifier_call_chain(unsigned long val, void *v);

#endif /*_XIAOMI_USB_TOUCH_NOTIFIER_H_*/
