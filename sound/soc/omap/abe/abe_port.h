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

#ifndef _ABE_PORT_H_
#define _ABE_PORT_H_

struct omap_abe_data_format {
	/* Sampling frequency of the stream */
	u32 f;
	/* Sample format type  */
	u32 samp_format;
};

struct omap_abe_port_protocol {
	/* Direction=0 means input from AESS point of view */
	u32 direction;
	/* Protocol type (switch) during the data transfers */
	u32 protocol_switch;
	union {
		/* McBSP/McASP peripheral connected to ATC */
		struct {
			u32 desc_addr;
			/* Address of ATC McBSP/McASP descriptor's in bytes */
			u32 buf_addr;
			/* DMEM address in bytes */
			u32 buf_size;
			/* ITERation on each DMAreq signals */
			u32 iter;
		} serial;
		/* DMIC peripheral connected to ATC */
		struct {
			/* DMEM address in bytes */
			u32 buf_addr;
			/* DMEM buffer size in bytes */
			u32 buf_size;
			/* Number of activated DMIC */
			u32 nbchan;
		} dmic;
		/* McPDMDL peripheral connected to ATC */
		struct {
			/* DMEM address in bytes */
			u32 buf_addr;
			/* DMEM size in bytes */
			u32 buf_size;
			/* Control allowed on McPDM DL */
			u32 control;
		} mcpdmdl;
		/* McPDMUL peripheral connected to ATC */
		struct {
			/* DMEM address size in bytes */
			u32 buf_addr;
			/* DMEM buffer size size in bytes */
			u32 buf_size;
		} mcpdmul;
		/* Ping-Pong interface to the Host using cache-flush */
		struct {
			/* Address of ATC descriptor's */
			u32 desc_addr;
			/* DMEM buffer base address in bytes */
			u32 buf_addr;
			/* DMEM size in bytes for each ping and pong buffers */
			u32 buf_size;
			/* IRQ address (either DMA (0) MCU (1) or DSP(2)) */
			u32 irq_addr;
			/* IRQ data content loaded in the AESS IRQ register */
			u32 irq_data;
			/* Call-back function upon IRQ reception */
			u32 callback;
		} pingpong;
		/* DMAreq line to CBPr */
		struct {
			/* Address of ATC descriptor's */
			u32 desc_addr;
			/* DMEM buffer address in bytes */
			u32 buf_addr;
			/* DMEM buffer size size in bytes */
			u32 buf_size;
			/* ITERation on each DMAreq signals */
			u32 iter;
			/* DMAreq address */
			u32 dma_addr;
			/* DMA/AESS = 1 << #DMA */
			u32 dma_data;
		} dmareq;
		/* Circular buffer - direct addressing to DMEM */
		struct {
			/* DMEM buffer base address in bytes */
			u32 buf_addr;
			/* DMEM buffer size in bytes */
			u32 buf_size;
			/* DMAreq address */
			u32 dma_addr;
			/* DMA/AESS = 1 << #DMA */
			u32 dma_data;
		} circular_buffer;
	} port;
};

extern const abe_port_t abe_port_init[];
extern abe_port_t abe_port[];
extern const u32 abe_port_priority[];

int omap_abe_select_main_port(u32 id);
u32 omap_abe_dma_port_iter_factor(struct omap_abe_data_format *f);

#endif/* _ABE_PORT_H_ */
