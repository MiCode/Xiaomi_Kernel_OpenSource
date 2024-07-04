#ifndef _XIAOMI_TOUCH_NOTIFIER_H_
#define _XIAOMI_TOUCH_NOTIFIER_H_

/*
 *	This include file is intended for xiaomi touch that to notify and receive
 *	touch event.
 *
 */
#include <linux/notifier.h>

#define		XIAOMI_TOUCH_SENSOR_EVENT_PS_SWITCH   0x01
#define		XIAOMI_TOUCH_GESTURE_EVENT_SWITCH   0x02

enum {
	/* touch: proximity sensor off */
	XIAOMI_TOUCH_SENSOR_PS_DISABLE,
	/* touch: proximity sensor on */
	XIAOMI_TOUCH_SENSOR_PS_ENABLE,
	/*touch: gesture  disable*/
	XIAOMI_TOUCH_GESTURE_DISABLE,
	/*touch: gesture  enable*/
	XIAOMI_TOUCH_GESTURE_ENABLE,
};

struct xiaomi_touch_notify_data {
	int ps_enable;
	int gesture_enable;
};

extern int xiaomi_touch_notifier_register_client(struct notifier_block *nb);
extern int xiaomi_touch_notifier_unregister_client(struct notifier_block *nb);
extern int xiaomi_touch_notifier_call_chain(unsigned long val, void *v);

#endif /*_XIAOMI_TOUCH_NOTIFIER_H_*/
