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
#ifndef _VTSSCFG_H_
#define _VTSSCFG_H_

#include "vtssrtcfg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VTSS_CFG_SPACE_SIZE 0x2000
#define VTSS_CFG_CHAIN_SIZE 0x0200

#pragma pack(push)
// main per process configuration
typedef struct
{
    // CPU event version 1
    int cpuevent_count_v1;
    cpuevent_cfg_v1_t cpuevent_cfg_v1[VTSS_CFG_CHAIN_SIZE];
    unsigned char cpuevent_namespace_v1[VTSS_CFG_SPACE_SIZE * 16];

    // OS event configuration
    int osevent_count;
    osevent_cfg_t osevent_cfg[VTSS_CFG_CHAIN_SIZE];

    // branch tracing configuration
    bts_cfg_t bts_cfg;

    // last branch tracing configuration
    lbr_cfg_t lbr_cfg;

    // tracing configuration
    trace_cfg_t trace_cfg;
    unsigned char trace_space[VTSS_CFG_SPACE_SIZE];

    //stack configuration
    unsigned long stk_sz[vtss_stk_last]; //stk_page_cnt*stk_page_size
    unsigned long stk_pg_sz[vtss_stk_last];

} process_cfg_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* _VTSSRTCFG_H_ */
