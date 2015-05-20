/*
    Copyright (C) 2005-2014 Intel Corporation.  All Rights Reserved.
 
    This file is part of SEP Development Kit
 
    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.
 
    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 
    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

#ifndef _CORE2_H_
#define _CORE2_H_

#include "msrdefs.h"

extern DISPATCH_NODE  core2_dispatch;
extern DISPATCH_NODE  corei7_dispatch;
extern DISPATCH_NODE  corei7_dispatch_nehalem;
extern DISPATCH_NODE  corei7_dispatch_htoff_mode;
extern DISPATCH_NODE  corei7_dispatch_2;
extern DISPATCH_NODE  corei7_dispatch_htoff_mode_2;

#define CORE2UNC_BLBYPASS_BITMASK      0x00000001
#define CORE2UNC_DISABLE_BL_BYPASS_MSR 0x39C

#if defined(DRV_IA32)
#define CORE2_LBR_DATA_BITS            32
#else
#define CORE2_LBR_DATA_BITS            48
#endif

#define CORE2_LBR_BITMASK                    ((1ULL << CORE2_LBR_DATA_BITS) -1)

#endif 
