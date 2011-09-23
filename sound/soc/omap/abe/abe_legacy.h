/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  BSD LICENSE

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Texas Instruments Incorporated nor the names of
      its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _ABE_MAIN_H_
#define _ABE_MAIN_H_

#include <linux/io.h>

#include "abe_dm_addr.h"
#include "abe_sm_addr.h"
#include "abe_cm_addr.h"
#include "abe_define.h"
#include "abe_fw.h"
#include "abe_def.h"
#include "abe_typ.h"
#include "abe_ext.h"
#include "abe_dbg.h"
#include "abe_ref.h"
#include "abe_api.h"
#include "abe_typedef.h"
#include "abe_functionsid.h"
#include "abe_taskid.h"
#include "abe_initxxx_labels.h"
#include "abe_fw.h"

/* pipe connection to the TARGET simulator */
#define ABE_DEBUG_CHECKERS              0
/* simulator data extracted from a text-file */
#define ABE_DEBUG_HWFILE                0
/* low-level log files */
#define ABE_DEBUG_LL_LOG                0

extern struct omap_abe *abe;

void omap_abe_dbg_log(struct omap_abe *abe, u32 x, u32 y, u32 z, u32 t);
void omap_abe_dbg_error(struct omap_abe *abe, int level, int error);

/*
 * MACROS
 */
#define _log(x, y, z, t) { if (x & abe->dbg.mask) omap_abe_dbg_log(abe, x, y, z, t); }

#endif				/* _ABE_MAIN_H_ */
