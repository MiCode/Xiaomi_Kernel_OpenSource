#include <linux/notifier.h>

#ifndef _ISPV4_NOTIFY_H_
#define _ISPV4_NOTIFY_H_

int ispv4_register_notifier(void *data);
int ispv4_unregister_notifier(void *data);
int ispv4_notifier_call_chain(unsigned int val, void *v);
#endif
