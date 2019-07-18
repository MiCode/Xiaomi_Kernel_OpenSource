/*Linux/include/linux/cpu-boost.h
 *
 * Copyright (C) 2001 Russell King
 * Copyright (C) 2019 XiaoMi, Inc.
 *           (C) 2002 - 2003 Dominik Brodowski <linux@brodo.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_CPU_BOOST_H
#define _LINUX_CPU_BOOST_H

void do_suspend_boost(void);
void do_suspend_boost_reset(void);
#endif

