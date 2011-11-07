#ifndef __ASMARM_ARCH_TIMER_H
#define __ASMARM_ARCH_TIMER_H

struct resource;

int arch_timer_register(struct resource *res, int res_nr);

#endif
