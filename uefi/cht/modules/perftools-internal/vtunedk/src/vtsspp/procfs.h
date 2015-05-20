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
#ifndef _VTSS_PROCFS_H_
#define _VTSS_PROCFS_H_

#include "vtss_autoconf.h"

#include <linux/types.h>        /* for size_t */
#include <linux/proc_fs.h>      /* for struct proc_dir_entry */

void vtss_procfs_fini(void);
int  vtss_procfs_init(void);
const char *vtss_procfs_path(void);
struct proc_dir_entry *vtss_procfs_get_root(void);
int  vtss_procfs_ctrl_wake_up(void *msg, size_t size);
int vtss_procfs_ctrl_wake_up_2(void *msg1, size_t size1, void *msg2, size_t size2);
void vtss_procfs_ctrl_flush(void);
const struct cpumask* vtss_procfs_cpumask(void);
int vtss_procfs_defsav(void);

#endif /* _VTSS_PROCFS_H_ */
