#ifndef _DOUBLE_CLICK_H
#define _DOUBLE_CLICK_H

#include <linux/module.h>

extern void tp_enable_doubleclick(bool state);
extern bool is_tp_doubleclick_enable(void);

#endif
