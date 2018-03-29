/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _BACKTRACE_H
#define _BACKTRACE_H

/* defined in backstrace32.asm */
extern asmlinkage void c_backtrace_ramconsole_print(unsigned long fp, int pmode);

#endif /* _BACKTRACE_H */
