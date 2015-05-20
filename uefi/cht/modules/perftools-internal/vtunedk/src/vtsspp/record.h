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
#ifndef _VTSS_RECORD_H_
#define _VTSS_RECORD_H_

#include "vtss_autoconf.h"
#include "transport.h"
#include "cpuevents.h"

int vtss_record_magic(struct vtss_transport_data* trnd, int is_safe);
int vtss_record_debug_info(struct vtss_transport_data* trnd, const char* message, int is_safe);
int vtss_record_process_exec(struct vtss_transport_data* trnd, pid_t tid, pid_t pid, int cpu, const char *filename, int is_safe);
int vtss_record_process_exit(struct vtss_transport_data* trnd, pid_t tid, pid_t pid, int cpu, const char *filename, int is_safe);
int vtss_record_thread_create(struct vtss_transport_data* trnd, pid_t tid, pid_t pid, int cpu, int is_safe);
int vtss_record_thread_stop(struct vtss_transport_data* trnd, pid_t tid, pid_t pid, int cpu, int is_safe);
int vtss_record_thread_name(struct vtss_transport_data* trnd, pid_t tid, const char *taskname, int is_safe);
int vtss_record_switch_from(struct vtss_transport_data* trnd, int cpu, int is_preempt, int is_safe);
int vtss_record_switch_to(struct vtss_transport_data* trnd, pid_t tid, int cpu, void* ip, int is_safe);
int vtss_record_sample(struct vtss_transport_data* trnd, pid_t tid, int cpu, cpuevent_t* cpuevent_chain, void* ip, int is_safe);
int vtss_record_bts(struct vtss_transport_data* trnd, pid_t tid, int cpu, void* bts_buff, size_t bts_size, int is_safe);
int vtss_record_module(struct vtss_transport_data* trnd, int m32, unsigned long addr, unsigned long len, const char *pname, unsigned long pgoff, long long cputsc, long long realtsc, int is_safe);
int vtss_record_configs(struct vtss_transport_data* trnd, int m32, int is_safe);
int vtss_record_softcfg(struct vtss_transport_data* trnd, pid_t tid, int is_safe);
int vtss_record_probe(struct vtss_transport_data* trnd, int cpu, int fid, int is_safe);

int vtss_record_probe_all(int cpu, int fid, int is_safe);

#endif /* _VTSS_RECORD_H_ */
