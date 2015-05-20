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
#ifndef _VTSS_LBR_H_
#define _VTSS_LBR_H_

#include "vtss_autoconf.h"

#define VTSS_MAX_LBRS 32

typedef struct _lbr_control_t
{
    long long lbrstk[VTSS_MAX_LBRS * 2];
    long long lbrtos;

} lbr_control_t;


int   vtss_lbr_init(void);
void  vtss_lbr_fini(void);
void  vtss_lbr_enable(lbr_control_t* lbrctl);
void  vtss_lbr_disable(void);
void  vtss_lbr_disable_save(lbr_control_t* lbrctl);
void* vtss_lbr_correct_ip(void* ip);

int   vtss_stack_record_lbr(struct vtss_transport_data* trnd, stack_control_t* stk, pid_t tid, int cpu, int is_safe);

#endif /* _VTSS_LBR_H_ */
