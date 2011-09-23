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

#ifndef _ABE_MEM_H_
#define _ABE_MEM_H_

#define OMAP_ABE_DMEM 0
#define OMAP_ABE_CMEM 1
#define OMAP_ABE_SMEM 2
#define OMAP_ABE_PMEM 3
#define OMAP_ABE_AESS 4

/* Distinction between Read and Write from/to ABE memory
 * is useful for simulation tool */
static inline void omap_abe_mem_write(struct omap_abe *abe, int bank,
				u32 offset, u32 *src, size_t bytes)
{
	memcpy((abe->io_base[bank] + offset), src, bytes);
}

static inline void omap_abe_mem_read(struct omap_abe *abe, int bank,
				u32 offset, u32 *dest, size_t bytes)
{
	memcpy(dest, (abe->io_base[bank] + offset), bytes);
}

static inline u32 omap_abe_reg_readl(struct omap_abe *abe, u32 offset)
{
	return __raw_readl(abe->io_base[OMAP_ABE_AESS] + offset);
}

static inline void omap_abe_reg_writel(struct omap_abe *abe,
				u32 offset, u32 val)
{
	__raw_writel(val, (abe->io_base[OMAP_ABE_AESS] + offset));
}

static inline void *omap_abe_reset_mem(struct omap_abe *abe, int bank,
			u32 offset, size_t bytes)
{
	return memset(abe->io_base[bank] + offset, 0, bytes);
}

#endif /*_ABE_MEM_H_*/
