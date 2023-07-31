#ifndef _XIAOMI_PANEL_NOTIFIER_H_
#define _XIAOMI_PANEL_NOTIFIER_H_

/*
 *	This include file is intended for xiaomi display panel that to notify and receive
 *	blank/unblank event.
 *
 */
#include <linux/notifier.h>

#define		XIAOMI_PANEL_EARLY_EVENT_BLANK   0x01
#define		XIAOMI_PANEL_EVENT_BLANK         0x02
#define		XIAOMI_PANEL_R_EARLY_EVENT_BLANK 0x03
#define 	XIAOMI_PANEL_FOD_EVENT           0x04
#define 	XIAOMI_PANEL_FPS_EVENT           0x05
#define 	XIAOMI_PANEL_NORMAL_EVENT_BLANK  0X06

enum {
	/* panel: power on */
	XIAOMI_PANEL_BLANK_UNBLANK,
	/* panel: power off */
	XIAOMI_PANEL_BLANK_POWERDOWN,
};

struct xiaomi_panel_notify_data {
	int blank;
	void *data; //not use, reserved
};

extern int xiaomi_panel_notifier_register_client(struct notifier_block *nb);
extern int xiaomi_panel_notifier_unregister_client(struct notifier_block *nb);
extern int xiaomi_panel_notifier_call_chain(unsigned long val, void *v);

#endif /*_XIAOMI_PANEL_NOTIFIER_H_*/
