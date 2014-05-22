/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#ifndef _SI_FW_MACROS_H_
#define _SI_FW_MACROS_H_

/*
 * Define Linux versions of macros used to cross compile driver code for other
 * platforms.
 */

#define PLACE_IN_CODE_SEG

#define SI_PUSH_STRUCT_PACKING
#define SI_POP_STRUCT_PACKING
#define SI_PACK_THIS_STRUCT __attribute__((__packed__))

#define SII_OFFSETOF offsetof

#define SII_ASSERT(cond, ...)		\
do {					\
	if (!(cond)) {			\
		printk(__VA_ARGS__);	\
		BUG();			\
	}				\
} while (0)

#endif
