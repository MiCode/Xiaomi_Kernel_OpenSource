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
#ifndef _VTSS_DSA_H_
#define _VTSS_DSA_H_

#include "vtss_autoconf.h"

#define IS_DSA_64ON32 (hardcfg.family == 0x06 && hardcfg.model >= 0x0f && hardcfg.mode == 32)

#pragma pack(push, 1)

typedef union
{
    struct {
        void *bts_base;
        void *bts_index;
        void *bts_absmax;
        void *bts_threshold;

        void *pebs_base;
        void *pebs_index;
        void *pebs_absmax;
        void *pebs_threshold;

        void *pebs_reset[2];
        void *reserved[2];
    } v64;

    struct {
        void *bts_base;
        void *bts_pad0;
        void *bts_index;
        void *bts_pad1;
        void *bts_absmax;
        void *bts_pad2;
        void *bts_threshold;
        void *bts_pad3;

        void *pebs_base;
        void *pebs_pad0;
        void *pebs_index;
        void *pebs_pad1;
        void *pebs_absmax;
        void *pebs_pad2;
        void *pebs_threshold;
        void *pebs_pad3;

        void *pebs_reset[4];
        void *reserved[4];
    } v32;
} vtss_dsa_t;

#pragma pack(pop)

int  vtss_dsa_init(void);
void vtss_dsa_fini(void);
void vtss_dsa_init_cpu(void);
vtss_dsa_t* vtss_dsa_get(int cpu);

#endif /* _VTSS_DSA_H_ */
