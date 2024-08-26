/* N19A code for HQ-359934 by p-huangyunbiao at 2023/12/19 start */
#ifndef __LINUX_TPD_NOTIFY_H__
#define __LINUX_TPD_NOTIFY_H__

#include <linux/notifier.h>

#define TPD_VSP_VSN_UP_EVENT_BLANK		0x01
#define TPD_DETECT_USB_IN 2
#define TPD_DETECT_USB_OUT 3

int tpd_register_client(struct notifier_block *nb);
int tpd_unregister_client(struct notifier_block *nb);
int tpd_notifier_call_chain(unsigned long val, void *v);
int tpd_register_psenable_callback(void);
int ps_enable_register_notifier(struct notifier_block *nb);
int ps_enable_unregister_notifier(struct notifier_block *nb);
int tpd_charger_detect_register_client(struct notifier_block *nb);
int tpd_charger_detect_unregister_client(struct notifier_block *nb);
int tpd_charger_detect_notifier_call_chain(unsigned long val, void *v);

#endif /* __LINUX_TPD_NOTIFY_H__ */
/* N19A code for HQ-359934 by p-huangyunbiao at 2023/12/19 end */
