#ifndef _XIAOMI_HEADSET_TOUCH_NOTIFIER_H_
#define _XIAOMI_HEADSET_TOUCH_NOTIFIER_H_

/*
 *      This include file is intended for xiaomi touch that to notify and receive
 *      touch event.
 *
 */
#include <linux/notifier.h>

#define         XIAOMI_TOUCH_HEADSET_SWITCH   0x01


enum {
        /* headset  switch off */
        XIAOMI_HEADSET_DISABLE,
        /* headset  switch on */
        XIAOMI_HEADSET_ENABLE
};

struct xiaomi_headset_notify_data {
        int headset_touch_enable;
};

extern int xiaomi_headset_touch_notifier_register_client(struct notifier_block *nb);
extern int xiaomi_headset_touch_notifier_unregister_client(struct notifier_block *nb);
extern int xiaomi_headset_touch_notifier_call_chain(unsigned long val, void *v);

#endif /*_XIAOMI_HEADSET_TOUCH_NOTIFIER_H_*/

