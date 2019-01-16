/*

SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             

*/

/*
 * Define Linux versions of macros used to cross compile some of
 * code of this driver as 8051 based starter kit firmware.
 * todo need to see if this can be cleaned up
 */
#define PLACE_IN_CODE_SEG

#define SI_PUSH_STRUCT_PACKING
#define SI_POP_STRUCT_PACKING
#define SI_PACK_THIS_STRUCT	__attribute__((__packed__))

#define SII_OFFSETOF offsetof

#define SII_ASSERT(cond, ...)	\
do {							\
	if (!(cond)) {				\
		printk(__VA_ARGS__);	\
		BUG();					\
	}							\
} while(0)
