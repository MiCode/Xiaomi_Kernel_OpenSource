/*
  Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#ifndef _VTSS_REGS_H_
#define _VTSS_REGS_H_

#include "vtss_autoconf.h"

#include <linux/compiler.h>     /* for __user */
#include <asm/ptrace.h>

#ifdef CONFIG_X86_32
#define vtss_get_current_bp(bp) asm("movl %%ebp, %0":"=r"(bp):)
#else
#define vtss_get_current_bp(bp) asm("movq %%rbp, %0":"=r"(bp):)
#endif

#ifdef VTSS_AUTOCONF_X86_UNIREGS
#define REG(name, regs) ((regs)->name)
#else /* VTSS_AUTOCONF_X86_UNIREGS */
#if defined(CONFIG_X86_32)
#define REG(name, regs) ((regs)->e##name)
#elif defined(CONFIG_X86_64)
#define REG(name, regs) ((regs)->r##name)
#endif
#endif /* VTSS_AUTOCONF_X86_UNIREGS */

#endif /* _VTSS_REGS_H_ */
