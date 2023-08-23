/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/*
 * Linux kernel specific definitions used by code shared with
 * Linux/Windows user space.
 */

#ifndef __CONFIG_LINUX_KERNEL_INC__
#define __CONFIG_LINUX_KERNEL_INC__

#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/crc32.h>
#include <linux/ftrace.h>

#define _ASSERT(e)
#ifndef PRINT_ASSERT
#define PRINT_ASSERT(e) {\
	(if ((e))\
		printk(KERN_ERR "PrintAssert:%s (%s:%d) error code:%d\n",\
		__FUNCTION__, __FILE__, __LINE__, e)) \
	}
#endif
#if defined(CONFIG_TRACING) && defined(DEBUG)
	#define tfa98xx_trace_printk(...) trace_printk(__VA_ARGS__)
#else
	#define tfa98xx_trace_printk(...)
#endif

#endif /* __CONFIG_LINUX_KERNEL_INC__ */

